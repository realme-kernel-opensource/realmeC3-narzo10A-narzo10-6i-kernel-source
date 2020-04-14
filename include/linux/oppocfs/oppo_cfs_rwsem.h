#ifndef _OPPO_CFS_RWSEM_H_
#define _OPPO_CFS_RWSEM_H_
extern bool rwsem_list_add(struct task_struct *tsk, struct list_head *entry, struct list_head *head, struct rw_semaphore *sem);
extern void rwsem_dynamic_ux_enqueue(struct task_struct *tsk, struct task_struct *waiter_task, struct task_struct *owner, struct rw_semaphore *sem);
extern void rwsem_dynamic_ux_dequeue(struct rw_semaphore *sem, struct task_struct *tsk);
#endif
