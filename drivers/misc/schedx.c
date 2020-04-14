/* drivers/misc/schedx.c
 * Copyright (c)  2018  Guangdong OPPO Mobile Comm Corp., Ltd
 *
 * This software is licensed under the terms of the GNU General Public
 * License version 2, as published by the Free Software Foundation, and
 * may be copied, distributed, and modified under those terms.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 */

#include <linux/module.h>
#include <linux/types.h>
#include <linux/errno.h>
#include <linux/kernel.h>
#include <linux/proc_fs.h>
#include <linux/fs.h>
#include <linux/seq_file.h>
#include <linux/sched.h>
#include <linux/vmalloc.h>
#include <linux/sched/signal.h>
#include <asm/uaccess.h>
#include <linux/sched.h>
#include <linux/cpumask.h>

static DEFINE_SPINLOCK(cpustat_lock);
struct task_info {
	u64 sum_exec;
	pid_t pid;
	char comm[TASK_COMM_LEN];
};

static int cpustat_show(struct seq_file *m, void *v)
{
	int cpu;
	unsigned long cpustat_flag;
	unsigned long util, capacity;

	for_each_possible_cpu(cpu) {
		spin_lock_irqsave(&cpustat_lock, cpustat_flag);

		util = cpu_online(cpu) ? sched_get_cpu_util(cpu) : 0;
		capacity = sched_get_capacity_orig(cpu);

		spin_unlock_irqrestore(&cpustat_lock, cpustat_flag);

		seq_printf(m, "%lu:%ld ", util, capacity);
	}

	seq_printf(m, "\n");

	return 0;
}

/* Use a memory storage to store task info for speed */
#define MAX_THREAD_INFO     4096
static int process_cputime_show(struct seq_file *m, void *v)
{
	struct task_struct *t, *s;
	int total = 0;
	int i;
	struct task_info *instant_task_info;

	instant_task_info = (struct task_info *)vmalloc(MAX_THREAD_INFO * sizeof(struct task_info));
	if (!instant_task_info)
		return -ENOMEM;

	rcu_read_lock();
	for_each_process(t) {
		u64 sum_thread = 0;
		for_each_thread(t, s) {
			sum_thread += s->se.sum_exec_runtime;
		}
		instant_task_info[total].sum_exec = sum_thread;
		instant_task_info[total].pid = t->pid;
		instant_task_info[total].comm[TASK_COMM_LEN - 1] = '\0';
		strncpy(instant_task_info[total].comm, t->comm, TASK_COMM_LEN - 1);
		total++;

		if (total >= MAX_THREAD_INFO)
			break;
	}
	rcu_read_unlock();

	for (i = 0; i < total; i++) {
		seq_printf(m, "%d %llu %s\n", instant_task_info[i].pid,
				instant_task_info[i].sum_exec,
				instant_task_info[i].comm);
	}

	vfree(instant_task_info);

	return 0;
}

static int process_cputime_open(struct inode *inode, struct file *filp)
{
	return single_open(filp, process_cputime_show, PDE_DATA(inode));
}

static int cpustat_open(struct inode *inode, struct file *filp)
{
	return single_open(filp, cpustat_show, PDE_DATA(inode));
}

static const struct file_operations process_cputime_fops = {
	.open       = process_cputime_open,
	.read       = seq_read,
	.llseek     = seq_lseek,
	.release    = single_release,
};

static const struct file_operations cpustat_fops = {
	.open       = cpustat_open,
	.read       = seq_read,
	.llseek     = seq_lseek,
	.release    = single_release,
};

static int __init schedx_init(void)
{
	struct proc_dir_entry *p_parent;

	p_parent = proc_mkdir("schedx", NULL);
	if (!p_parent){
		pr_err("%s: failed to create schedx directory\n", __func__);
		goto err;
	}

	proc_create_data("process_cputime", 0440, p_parent,
			&process_cputime_fops, NULL);

	proc_create_data("cpustat", 0440, p_parent,
			&cpustat_fops, NULL);

	return 0;

err:
	return -ENOMEM;
}

early_initcall(schedx_init);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("wangmengmeng <wangmengmeng@oppo.com>");
MODULE_DESCRIPTION("optimize foreground process to promote performance");
