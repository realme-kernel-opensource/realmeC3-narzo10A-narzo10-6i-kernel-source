#ifndef _OPPO_CFS_BINDER_H_
#define _OPPO_CFS_BINDER_H_
#include <linux/oppocfs/oppo_cfs_common.h>
static inline void binder_thread_check_and_set_dynamic_ux(struct task_struct *thread_task, struct task_struct *from_task)
{
    if (from_task && test_task_ux(from_task) && test_task_ux_depth(from_task->ux_depth) &&
        !test_task_ux(thread_task)) {
        dynamic_ux_enqueue(thread_task, DYNAMIC_UX_BINDER, from_task->ux_depth);
    }
}

static inline void binder_thread_check_and_remove_dynamic_ux(struct task_struct *thread_task)
{
    if (test_dynamic_ux(thread_task, DYNAMIC_UX_BINDER)) {
        dynamic_ux_dequeue(thread_task, DYNAMIC_UX_BINDER);
    }
}
#endif
