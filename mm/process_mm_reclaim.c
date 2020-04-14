/*
 * process_mm_reclaim.c
 *
 */
#include <linux/init.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/moduleparam.h>
#include <linux/sched/task.h>
#include <linux/seq_file.h>
#include <linux/mm.h>
#include <linux/rmap.h>
/* Kui.Zhang@PSW.BSP.Kernel.Performance, 2019-01-01,
 * Create /proc/process_reclaim interface for process reclaim. */
#include <linux/proc_fs.h>

/* Kui.Zhang@PSW.BSP.Kernel.Performance, 2018-12-25, check current need
 * cancel reclaim or not, please check task not NULL first.
 * If the reclaimed task has goto foreground, cancel reclaim immediately*/
#define RECLAIM_SCAN_REGION_LEN (400ul<<20)

static inline int _is_reclaim_should_cancel(struct mm_walk *walk)
{
	struct mm_struct *mm = walk->mm;
	struct task_struct *task;

	if (!mm)
		return 0;

	task = ((struct reclaim_param *)(walk->private))->reclaimed_task;
	if (!task)
		return 0;

	if (mm != task->mm)
		return PR_TASK_DIE;
	if (rwsem_is_wlocked(&mm->mmap_sem))
		return PR_SEM_OUT;
	if (task_is_fg(task))
		return PR_TASK_FG;
	if (task->state == TASK_RUNNING)
		return PR_TASK_RUN;
	if (time_is_before_eq_jiffies(current->reclaim.stop_jiffies))
		return PR_TIME_OUT;

	return 0;
}

int is_reclaim_should_cancel(struct mm_walk *walk)
{
	struct task_struct *task;

	if (!current_is_reclaimer() || !walk->private)
		return 0;

	task = ((struct reclaim_param *)(walk->private))->reclaimed_task;
	if (!task)
		return 0;

	return _is_reclaim_should_cancel(walk);
}

int is_reclaim_addr_over(struct mm_walk *walk, unsigned long addr)
{
	struct task_struct *task;

	if (!current_is_reclaimer() || !walk->private)
		return 0;

	task = ((struct reclaim_param *)(walk->private))->reclaimed_task;
	if (!task)
		return 0;

	if (task->reclaim.stop_scan_addr + RECLAIM_SCAN_REGION_LEN <= addr) {
		task->reclaim.stop_scan_addr = addr;
		return PR_ADDR_OVER;
	}

	return _is_reclaim_should_cancel(walk);
}

/* Kui.Zhang@PSW.BSP.Kernel.Performance, 2019-01-01,
 * Create /proc/process_reclaim interface for process reclaim.
 * Because /proc/$pid/reclaim has deifferent permissiones of different processes,
 * and can not set to 0444 because that have security risk.
 * Use /proc/process_reclaim and setting with selinux */
#ifdef CONFIG_PROC_FS

#define PROCESS_RECLAIM_CMD_LEN 64

static int process_reclaim_enable = 1;
module_param_named(process_reclaim_enable, process_reclaim_enable, int, 0644);

ssize_t process_reclaim_write(struct file *file, const char __user *buffer,
		size_t count, loff_t *ppos)
{
	char kbuf[PROCESS_RECLAIM_CMD_LEN];
	char *act_str;
	char *end;
	long value;
	pid_t tsk_pid;
	struct task_struct* tsk;
	ssize_t ret = 0;

	if (!process_reclaim_enable) {
		pr_warn("Process memory reclaim is disabled!\n");
		return -EFAULT;
	}

	if (count > PROCESS_RECLAIM_CMD_LEN)
		return -EINVAL;

	memset(kbuf, 0, PROCESS_RECLAIM_CMD_LEN);
	if (copy_from_user(&kbuf, buffer, count))
		return -EFAULT;
	kbuf[PROCESS_RECLAIM_CMD_LEN - 1] = '\0';

	act_str = strstrip(kbuf);
	if (*act_str <= '0' || *act_str > '9') {
		pr_err("process_reclaim write [%s] pid format is invalid.\n", kbuf);
		return -EINVAL;
	}

	value = simple_strtol(act_str, &end, 10);
	if (value < 0 || value > INT_MAX) {
		pr_err("process_reclaim write [%s] is invalid.\n", kbuf);
		return -EINVAL;
	}

	tsk_pid = (pid_t)value;

	if (end == (act_str + strlen(act_str))) {
		pr_err("process_reclaim write [%s] do not set reclaim type.\n", kbuf);
		return -EINVAL;
	}

	if (*end != ' ' && *end != '	') {
		pr_err("process_reclaim write [%s] format is wrong.\n", kbuf);
		return -EINVAL;
	}

	end = strstrip(end);
	rcu_read_lock();
	tsk = find_task_by_vpid(tsk_pid);
	if (!tsk) {
		rcu_read_unlock();
		pr_err("process_reclaim can not find task of pid:%d\n", tsk_pid);
		return -ESRCH;
	}

	if (tsk != tsk->group_leader)
		tsk = tsk->group_leader;
	get_task_struct(tsk);
	rcu_read_unlock();

	ret = reclaim_task_write(tsk, end);

	put_task_struct(tsk);
	if (ret < 0) {
		pr_err("process_reclaim failed, command [%s]\n", kbuf);
		return ret;
	}

	return count;
}

static struct file_operations process_reclaim_fops = {
	.write	= process_reclaim_write,
	.llseek	= noop_llseek,
};

static inline void process_reclaim_init_procfs(void)
{
	if (!proc_create("process_reclaim", 0222, NULL,
				&process_reclaim_fops))
		pr_err("Failed to register proc interface\n");
}
#else /* CONFIG_PROC_FS */
static inline void process_reclaim_init_procfs(void)
{
}
#endif

static int __init process_reclaim_proc_init(void)
{
	process_reclaim_init_procfs();
	return 0;
}

late_initcall(process_reclaim_proc_init);
MODULE_LICENSE("GPL");
