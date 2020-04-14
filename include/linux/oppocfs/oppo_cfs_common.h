#ifndef _OPPO_CFS_COMMON_H_
#define _OPPO_CFS_COMMON_H_
struct rq;
extern void enqueue_ux_thread(struct rq *rq, struct task_struct *p);
extern void dequeue_ux_thread(struct rq *rq, struct task_struct *p);
extern void pick_ux_thread(struct rq *rq, struct task_struct **p, struct sched_entity **se);
extern bool test_dynamic_ux(struct task_struct *task, int type);
extern void dynamic_ux_dequeue(struct task_struct *task, int type);
extern void dynamic_ux_enqueue(struct task_struct *task, int type, int depth);
extern bool test_task_ux(struct task_struct *task);
extern bool test_task_ux_depth(int ux_depth);
extern bool test_set_dynamic_ux(struct task_struct *tsk);
extern void trigger_ux_balance(struct rq *rq);
extern void ux_init_rq_data(struct rq *rq);
#endif
