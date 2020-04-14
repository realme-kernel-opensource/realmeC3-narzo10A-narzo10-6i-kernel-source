#ifdef VENDOR_EDIT
// Liujie.Xie@TECH.Kernel.Sched, 2019/05/22, add for ui first

#include <linux/sched.h>
#include <linux/list.h>
#include <linux/rwsem.h>
#include <linux/oppocfs/oppo_cfs_common.h>

enum rwsem_waiter_type {
	RWSEM_WAITING_FOR_WRITE,
	RWSEM_WAITING_FOR_READ
};

struct rwsem_waiter {
	struct list_head list;
	struct task_struct *task;
	enum rwsem_waiter_type type;
};

#define RWSEM_READER_OWNED	((struct task_struct *)1UL)

static inline bool rwsem_owner_is_writer(struct task_struct *owner)
{
	return owner && owner != RWSEM_READER_OWNED;
}

static void rwsem_list_add_ux(struct list_head *entry, struct list_head *head)
{
	struct list_head *pos = NULL;
	struct list_head *n = NULL;
	struct rwsem_waiter *waiter = NULL;
	list_for_each_safe(pos, n, head) {
		waiter = list_entry(pos, struct rwsem_waiter, list);
		if (!test_task_ux(waiter->task) && waiter->task->prio > MAX_RT_PRIO) {
			list_add(entry, waiter->list.prev);
			return;
		}
	}
	if (pos == head) {
		list_add_tail(entry, head);
	}
}

bool rwsem_list_add(struct task_struct *tsk, struct list_head *entry, struct list_head *head, struct rw_semaphore *sem)
{
	bool is_ux = test_set_dynamic_ux(tsk);
	if (!entry || !head) {
		return false;
	}
	if (is_ux) {
		rwsem_list_add_ux(entry, head);
        return true;
	} else {
		//list_add_tail(entry, head);
		return false;
	}

    return false;
}

void rwsem_dynamic_ux_enqueue(struct task_struct *tsk, struct task_struct *waiter_task, struct task_struct *owner, struct rw_semaphore *sem)
{
	bool is_ux = test_set_dynamic_ux(tsk);
	if (waiter_task && is_ux) {
		if (rwsem_owner_is_writer(owner) && !test_task_ux(owner) && sem && !sem->ux_dep_task) {
			dynamic_ux_enqueue(owner, DYNAMIC_UX_RWSEM, tsk->ux_depth);
			sem->ux_dep_task = owner;
		}
	}
}

void rwsem_dynamic_ux_dequeue(struct rw_semaphore *sem, struct task_struct *tsk)
{
	if (tsk && sem && sem->ux_dep_task == tsk) {
		dynamic_ux_dequeue(tsk, DYNAMIC_UX_RWSEM);
		sem->ux_dep_task = NULL;
	}
}

#endif
