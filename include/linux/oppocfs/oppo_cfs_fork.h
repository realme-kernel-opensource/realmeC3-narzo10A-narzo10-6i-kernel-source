#ifndef _OPPO_CFS_FORK_H_
#define _OPPO_CFS_FORK_H_

static inline void init_task_ux_info(struct task_struct *p)
{
    p->static_ux = 0;
    atomic64_set(&(p->dynamic_ux), 0);
    INIT_LIST_HEAD(&p->ux_entry);
    p->ux_depth = 0;
    p->enqueue_time = 0;
    p->dynamic_ux_start = 0;
}
#endif
