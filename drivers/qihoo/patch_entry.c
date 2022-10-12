/*
 * drivers/qihoo/qihoo.h for kernel hotfix
 *
 * Copyright (C) 2017 Qihoo, Inc.
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
#include <linux/debugfs.h>
#include <linux/kernel.h>
#include <linux/seq_file.h>
#include <linux/types.h>
#include <asm/cacheflush.h>
#include <qihoo.h>
#ifdef CONFIG_MODULES
#include <linux/module.h>
const char *(*patch_module_name_func)(void) = NULL;
#endif
static inline int _qihoo_patch_register(struct qihoo_patch_object *_qhobj)
{
    struct qihoo_patch_object *qhobj = _qhobj;
#ifdef CONFIG_MODULES
    struct module *m = NULL;
#endif
    if (qhobj == NULL) {
        return -1;
    }
    pr_debug("patch %s type %d\n", __FUNCTION__, qhobj->type);
    do {
        switch (qhobj->type) {
        case e_patch_break:
        case e_patch_sync_break: {
#if __SIZEOF_POINTER__ == 4
            qhobj->n_func_addr = (((qhobj->n_func_addr - (qhobj->o_func_addr + 8)) >> 2) & 0x00ffffff) | 0xea000000;
#else
            qhobj->n_func_addr = (((qhobj->n_func_addr - qhobj->o_func_addr) >> 2) & 0x3ffffff) | 0x14000000;
#endif
            break;
        }
#ifdef CONFIG_MODULES
        case e_module_patch_break:
        case e_module_patch_sync_break: {
            if (patch_module_name_func) {
                m = find_module(patch_module_name_func());
                if (m == NULL) {
                    pr_debug("module not find %s\n", patch_module_name_func());
                    return -1;
                }
                if(qhobj->o_func_addr < (unsigned long)m->core_layout.base){
                    qhobj->o_func_addr = (unsigned long)m->core_layout.base + qhobj->o_func_addr;
#if __SIZEOF_POINTER__ == 4
                    qhobj->n_func_addr = (((qhobj->n_func_addr - (qhobj->o_func_addr + 8)) >> 2) & 0x00ffffff) | 0xea000000;
#else
                    qhobj->n_func_addr = (((qhobj->n_func_addr - qhobj->o_func_addr) >> 2) & 0x3ffffff) | 0x14000000;
#endif
                }
            }
            break;
        }
#endif
        }
        qhobj = qhobj->next;
    } while (qhobj != NULL);
    pr_debug("patch %s into qihoo_patch_register\n", __FUNCTION__);
    return qihoo_patch_register(_qhobj);
}
static inline int _qihoo_patch_unregister(struct qihoo_patch_object *_qhobj)
{
    return qihoo_patch_unregister(_qhobj);
}
static inline int ___qihoo_patch_init_(struct qihoo_patch_object **_qhobj)
{
    if (_qhobj == NULL) {
        return -1;
    }
    return _qihoo_patch_register(*_qhobj);
}
static inline int ___qihoo_patch_exit_(struct qihoo_patch_object **_qhobj)
{
    if (_qhobj == NULL) {
        return -1;
    }
    return _qihoo_patch_unregister(*_qhobj);
}
int qihoo_patch_init(struct __s_qihoo_patch_entry_object_ *__qihoo_patch_entry_object_)
{
    int i = 0;
    int ret = -1;
    if (__qihoo_patch_entry_object_ == NULL)
        return -1;
    pr_debug("patch %s entry addr %p\n", __FUNCTION__, __qihoo_patch_entry_object_);
    for (i = 0; i < __qihoo_patch_entry_object_->argc; i++) {
        ret = ___qihoo_patch_init_(__qihoo_patch_entry_object_->argv[i].obj);
        pr_debug("patch %s hook ret %d\n", __FUNCTION__, ret);
        if (ret != 0)
            break;
    }
    return ret;
}
void qihoo_patch_exit(struct __s_qihoo_patch_entry_object_ *__qihoo_patch_entry_object_)
{
    int i = 0;
    if (__qihoo_patch_entry_object_ == NULL)
        return;
    for (i = 0; i < __qihoo_patch_entry_object_->argc; i++) {
        ___qihoo_patch_exit_(__qihoo_patch_entry_object_->argv[i].obj);
    }
}
