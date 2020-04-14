#ifdef VENDOR_EDIT
// Liujie.Xie@TECH.Kernel.Sched, 2019/05/22, add for ui first
#include <linux/version.h>
#include <linux/sched.h>
#include <linux/list.h>
#include <linux/jiffies.h>
#include <trace/events/sched.h>
#include <../sched/sched.h>

#include <linux/fs.h>
#include <linux/proc_fs.h>
#include <asm/uaccess.h>
#include <linux/module.h>


int ux_min_sched_delay_granularity;     /*ux thread delay upper bound(ms)*/
int ux_max_dynamic_granularity = 32;    /*ux dynamic max exist time(ms)*/
int ux_min_migration_delay = 10;        /*ux min migration delay time(ms)*/
int ux_max_over_thresh = 2000; /* ms */
#define S2NS_T 1000000

static int entity_before(struct sched_entity *a,
				struct sched_entity *b)
{
	return (s64)(a->vruntime - b->vruntime) < 0;
}

static int entity_over(struct sched_entity *a,
				struct sched_entity *b)
{
	return (s64)(a->vruntime - b->vruntime) > (s64)ux_max_over_thresh * S2NS_T;
}

void enqueue_ux_thread(struct rq *rq, struct task_struct *p)
{
	struct list_head *pos, *n;
	bool exist = false;

	if (!rq || !p || !list_empty(&p->ux_entry)) {
		return;
	}
	p->enqueue_time = rq->clock;
	if (p->static_ux || atomic64_read(&p->dynamic_ux)) {
		list_for_each_safe(pos, n, &rq->ux_thread_list) {
			if (pos == &p->ux_entry) {
				exist = true;
				break;
			}
		}
		if (!exist) {
			list_add_tail(&p->ux_entry, &rq->ux_thread_list);
			get_task_struct(p);
		}
	}
}

void dequeue_ux_thread(struct rq *rq, struct task_struct *p)
{
	struct list_head *pos, *n;
	u64 now =  jiffies_to_nsecs(jiffies);

	if (!rq || !p) {
		return;
	}
	p->enqueue_time = 0;
	if (!list_empty(&p->ux_entry)) {
		list_for_each_safe(pos, n, &rq->ux_thread_list) {
			if (atomic64_read(&p->dynamic_ux) && (now - p->dynamic_ux_start) > (u64)ux_max_dynamic_granularity * S2NS_T) {
				atomic64_set(&p->dynamic_ux, 0);
			}
		}
		list_for_each_safe(pos, n, &rq->ux_thread_list) {
			if (pos == &p->ux_entry) {
				list_del_init(&p->ux_entry);
				put_task_struct(p);
				return;
			}
		}
	}
}

static struct task_struct *pick_first_ux_thread(struct rq *rq)
{
	struct list_head *ux_thread_list = &rq->ux_thread_list;
	struct list_head *pos = NULL;
	struct list_head *n = NULL;
	struct task_struct *temp = NULL;
	struct task_struct *leftmost_task = NULL;
	list_for_each_safe(pos, n, ux_thread_list) {
		temp = list_entry(pos, struct task_struct, ux_entry);
		/*ensure ux task in current rq cpu otherwise delete it*/
		if (unlikely(task_cpu(temp) != rq->cpu)) {
			printk(KERN_WARNING "task(%s,%d,%d) does not belong to cpu%d", temp->comm, task_cpu(temp), temp->policy, rq->cpu);
			list_del_init(&temp->ux_entry);
			continue;
		}
		if (leftmost_task == NULL) {
			leftmost_task = temp;
		} else if (entity_before(&temp->se, &leftmost_task->se)) {
			leftmost_task = temp;
		}
	}

	return leftmost_task;
}

void pick_ux_thread(struct rq *rq, struct task_struct **p, struct sched_entity **se)
{
	struct task_struct *ori_p;
	struct task_struct *key_task;
	struct sched_entity *key_se;
	if (!rq || !p || !se) {
		return;
	}
	ori_p = *p;
	if (ori_p && !ori_p->static_ux && !atomic64_read(&ori_p->dynamic_ux)) {
		if (!list_empty(&rq->ux_thread_list)) {
			key_task = pick_first_ux_thread(rq);
            /* in case that ux thread keep running too long */
            if (key_task && entity_over(&key_task->se, &ori_p->se))
                return;

			if (key_task) {
				key_se = &key_task->se;
				if (key_se && (rq->clock >= key_task->enqueue_time) &&
				rq->clock - key_task->enqueue_time >= ((u64)ux_min_sched_delay_granularity * S2NS_T)) {
					*p = key_task;
					*se = key_se;
				}
			}
		}
	}
}

#define DYNAMIC_UX_SEC_WIDTH   8
#define DYNAMIC_UX_MASK_BASE   0x00000000ff

#define dynamic_ux_offset_of(type) (type * DYNAMIC_UX_SEC_WIDTH)
#define dynamic_ux_mask_of(type) ((u64)(DYNAMIC_UX_MASK_BASE) << (dynamic_ux_offset_of(type)))
#define dynamic_ux_get_bits(value, type) ((value & dynamic_ux_mask_of(type)) >> dynamic_ux_offset_of(type))
#define dynamic_ux_one(type) ((u64)1 << dynamic_ux_offset_of(type))


bool test_dynamic_ux(struct task_struct *task, int type)
{
	u64 dynamic_ux;
	if (!task) {
		return false;
	}
	dynamic_ux = atomic64_read(&task->dynamic_ux);
	return dynamic_ux_get_bits(dynamic_ux, type) > 0;
}

static bool test_task_exist(struct task_struct *task, struct list_head *head)
{
	struct list_head *pos, *n;
	list_for_each_safe(pos, n, head) {
		if (pos == &task->ux_entry) {
			return true;
		}
	}
	return false;
}

static inline void dynamic_ux_dec(struct task_struct *task, int type)
{
	atomic64_sub(dynamic_ux_one(type), &task->dynamic_ux);
}

static inline void dynamic_ux_inc(struct task_struct *task, int type)
{
	atomic64_add(dynamic_ux_one(type), &task->dynamic_ux);
}

static void __dynamic_ux_dequeue(struct task_struct *task, int type)
{
#if LINUX_VERSION_CODE < KERNEL_VERSION(4,9,0)
        unsigned long flags;
#else
        struct rq_flags flags;
#endif
	bool exist = false;
	struct rq *rq = NULL;
	u64 dynamic_ux = 0;

	rq = task_rq_lock(task, &flags);
	dynamic_ux = atomic64_read(&task->dynamic_ux);
	if (dynamic_ux <= 0) {
		task_rq_unlock(rq, task, &flags);
		return;
	}
	dynamic_ux_dec(task, type);
	dynamic_ux = atomic64_read(&task->dynamic_ux);
	if (dynamic_ux > 0) {
		task_rq_unlock(rq, task, &flags);
		return;
	}
	task->ux_depth = 0;

	exist = test_task_exist(task, &rq->ux_thread_list);
	if (exist) {
		list_del_init(&task->ux_entry);
		put_task_struct(task);
	}
	task_rq_unlock(rq, task, &flags);
}

void dynamic_ux_dequeue(struct task_struct *task, int type)
{
	if (!task || type >= DYNAMIC_UX_MAX) {
		return;
	}
	__dynamic_ux_dequeue(task, type);
}

extern const struct sched_class fair_sched_class;
static void __dynamic_ux_enqueue(struct task_struct *task, int type, int depth)
{
#if LINUX_VERSION_CODE < KERNEL_VERSION(4,9,0)
        unsigned long flags;
#else
        struct rq_flags flags;
#endif
	bool exist = false;
	struct rq *rq = NULL;

	rq = task_rq_lock(task, &flags);
	if (task->sched_class != &fair_sched_class) {
		task_rq_unlock(rq, task, &flags);
		return;
	}
	if (unlikely(!list_empty(&task->ux_entry))) {
		printk(KERN_WARNING "task(%s,%d,%d) is already in another list", task->comm, task->pid, task->policy);
		task_rq_unlock(rq, task, &flags);
		return;
	}

	dynamic_ux_inc(task, type);
	task->dynamic_ux_start = jiffies_to_nsecs(jiffies);
	task->ux_depth = task->ux_depth > depth + 1 ? task->ux_depth : depth + 1;
	if (task->state == TASK_RUNNING) {
		exist = test_task_exist(task, &rq->ux_thread_list);
		if (!exist) {
			get_task_struct(task);
			list_add_tail(&task->ux_entry, &rq->ux_thread_list);
		}
	}
	task_rq_unlock(rq, task, &flags);
}

void dynamic_ux_enqueue(struct task_struct *task, int type, int depth)
{
	if (!task || type >= DYNAMIC_UX_MAX) {
		return;
	}
	__dynamic_ux_enqueue(task, type, depth);
}

inline bool test_task_ux(struct task_struct *task)
{
	return task && (task->static_ux || atomic64_read(&task->dynamic_ux));
}

inline bool test_task_ux_depth(int ux_depth)
{
	return ux_depth < UX_DEPTH_MAX;
}

inline bool test_set_dynamic_ux(struct task_struct *tsk)
{
	return test_task_ux(tsk) && test_task_ux_depth(tsk->ux_depth);
}

static struct task_struct *check_ux_delayed(struct rq *rq)
{
	struct list_head *pos = NULL;
	struct list_head *n = NULL;
	struct task_struct *tsk = NULL;
	struct list_head *ux_thread_list = NULL;
	ux_thread_list = &rq->ux_thread_list;

	list_for_each_safe(pos, n, ux_thread_list) {
		tsk = list_entry(pos, struct task_struct, ux_entry);
		if (tsk && (rq->clock - tsk->enqueue_time) >= (u64)ux_min_migration_delay * S2NS_T)
			return tsk;
	}
	return NULL;
}

static int ux_task_hot(struct task_struct *p, struct rq *src_rq, struct rq *dst_rq)
{
	s64 delta;

	lockdep_assert_held(&src_rq->lock);

	if (p->sched_class != &fair_sched_class)
		return 0;

	if (unlikely(p->policy == SCHED_IDLE))
		return 0;

	if (sched_feat(CACHE_HOT_BUDDY) && dst_rq->nr_running &&
	    (&p->se == src_rq->cfs.next || &p->se == src_rq->cfs.last))
		return 1;

	if (sysctl_sched_migration_cost == (unsigned int)-1)
		return 1;
	if (sysctl_sched_migration_cost == 0)
		return 0;

	delta = src_rq->clock_task - p->se.exec_start;
	return delta < (s64)sysctl_sched_migration_cost;
}

static void detach_task(struct task_struct *p, struct rq *src_rq, struct rq *dst_rq)
{
	lockdep_assert_held(&src_rq->lock);
	deactivate_task(src_rq, p, 0);
	p->on_rq = TASK_ON_RQ_MIGRATING;
	double_lock_balance(src_rq, dst_rq);
	set_task_cpu(p, dst_rq->cpu);
	double_unlock_balance(src_rq, dst_rq);
}

static void attach_task(struct rq *dst_rq, struct task_struct *p)
{
	raw_spin_lock(&dst_rq->lock);
	BUG_ON(task_rq(p) != dst_rq);
	p->on_rq = TASK_ON_RQ_QUEUED;
	activate_task(dst_rq, p, 0);
	check_preempt_curr(dst_rq, p, 0);
	raw_spin_unlock(&dst_rq->lock);
}

static int ux_can_migrate(struct task_struct *p, struct rq *src_rq, struct rq *dst_rq)
{
	if (task_running(src_rq, p))
		return 0;
	if (ux_task_hot(p, src_rq, dst_rq))
		return 0;
	if (task_rq(p) != src_rq) /*lint !e58*/
		return 0;
	if (!test_task_exist(p, &src_rq->ux_thread_list))
		return 0;
	return 1;
}

static int __do_ux_balance(void *data)
{
	struct rq *src_rq = data;
	struct rq *dst_rq = NULL;
	int src_cpu = cpu_of(src_rq);
	int i = 0;
	struct task_struct *p = NULL;
	bool is_mig = false;

	/*find a delayed ux task*/
	raw_spin_lock_irq(&src_rq->lock);
	if (unlikely(src_cpu != smp_processor_id() || !src_rq->active_ux_balance)) {
		src_rq->active_ux_balance = 0;
		raw_spin_unlock_irq(&src_rq->lock);
		return 0;
	}
	p = check_ux_delayed(src_rq);
	if (!p) {
		src_rq->active_ux_balance = 0;
		raw_spin_unlock_irq(&src_rq->lock);
		return 0;
	}

	raw_spin_unlock(&src_rq->lock);
	for_each_cpu_and(i, &(p->cpus_allowed), cpu_coregroup_mask(src_cpu)) {
		if (i == src_cpu || !cpu_online(i))
			continue;
		dst_rq = cpu_rq(i);
		raw_spin_lock(&dst_rq->lock);
		if (!dst_rq->rt.rt_nr_running || list_empty(&dst_rq->ux_thread_list)) {
			raw_spin_unlock(&dst_rq->lock);
			break;
		} else {
			raw_spin_unlock(&dst_rq->lock);
		}
	}

	/*move p from src to dst cpu*/
	raw_spin_lock(&src_rq->lock);
	if (i != nr_cpu_ids && p != NULL && dst_rq != NULL) {
		if (ux_can_migrate(p, src_rq, dst_rq)) {
			detach_task(p, src_rq, dst_rq);
			is_mig = true;
		}
	}
	src_rq->active_ux_balance = 0;
	raw_spin_unlock(&src_rq->lock);
	if (is_mig) {
		attach_task(dst_rq, p);
	}
	local_irq_enable();
	return 0;
}

void trigger_ux_balance(struct rq *rq)
{
	struct task_struct *task = NULL;
	int active_ux_balance = 0;
	if (!rq) {
		return;
	}
	raw_spin_lock(&rq->lock);
	task = check_ux_delayed(rq);
	/*
	 * active_ux_balance synchronized accesss to ux_balance_work
	 * 1 means ux_balance_work is ongoing, and reset to 0 after
	 * ux_balance_work is done
	 */
	if (!rq->active_ux_balance && task) {
		active_ux_balance = 1;
		rq->active_ux_balance = 1;
	}
	raw_spin_unlock(&rq->lock);
	if (active_ux_balance) {
		stop_one_cpu_nowait(cpu_of(rq), __do_ux_balance, rq, &rq->ux_balance_work);
	}
}

void ux_init_rq_data(struct rq *rq)
{
	if (!rq) {
		return;
	}
	rq->active_ux_balance = 0;
	INIT_LIST_HEAD(&rq->ux_thread_list);
}
#endif /* VENDOR_EDIT */
