/*
 * drivers/qihoo/core.c for kernel hotfix
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
#if defined(DEBUG)
#ifdef pr_fmt
#undef pr_fmt
#endif
#define pr_fmt(fmt) KBUILD_MODNAME ":%s:%d: " fmt, __func__, __LINE__
#else
#ifdef pr_fmt
#undef pr_fmt
#endif
#define pr_fmt(fmt) KBUILD_MODNAME ": " fmt
#ifdef pr_debug
#undef pr_debug
#define pr_debug(fmt, ...) no_printk(KERN_DEBUG pr_fmt(fmt), ##__VA_ARGS__)
#endif
#endif
#include "qihoo.h"
#include <asm/cacheflush.h>
#include <asm/tlbflush.h>
#include <linux/bug.h>
#include <linux/linkage.h>
#include <linux/list.h>
#include <linux/module.h>
#include <linux/mutex.h>
#include <linux/notifier.h>
#include <linux/percpu.h>
#include <linux/rcupdate.h>
#include <linux/slab.h>
#include <linux/smp.h>
#include <linux/spinlock.h>
#include <linux/syscalls.h>
#include <linux/stop_machine.h>
#define __is_check_address_range_ 0
#define e_qihoo_hook_success 0
#define e_qihoo_hook_fail -1
extern const void *sys_call_table[];
EXPORT_SYMBOL(sys_call_table);
#ifdef CONFIG_COMPAT
extern const void *compat_sys_call_table[];
EXPORT_SYMBOL(compat_sys_call_table);
#endif
LIST_HEAD(qpobj_list);
DEFINE_MUTEX(qpobj_list_mutex);
static DEFINE_SPINLOCK(mem_block_lock);
void mem_block_spinlock(unsigned long *flags)
{
	spin_lock_irqsave(&mem_block_lock, *flags);
}
void mem_block_spinunlock(unsigned long *flags)
{
	spin_unlock_irqrestore(&mem_block_lock, *flags);
}
#if __SIZEOF_POINTER__ == 4
static struct {
	pmd_t *pmd_to_flush;
	pmd_t *pmd;
	unsigned long addr;
	pmd_t saved_pmd;
	bool made_writeable;
} mem_block;
void mem_block_writeable(unsigned long addr)
{
	struct task_struct *tsk = current;
	struct mm_struct *mm = tsk->active_mm;
	pgd_t *pgd = pgd_offset(mm, addr);
	pud_t *pud = pud_offset(pgd, addr);
	mem_block.made_writeable = 0;
#if __is_check_address_range_
	if ((addr < (unsigned long)RX_AREA_START) ||
	    (addr >= (unsigned long)RX_AREA_END))
		return;
#endif
	mem_block.pmd = pmd_offset(pud, addr);
	mem_block.pmd_to_flush = mem_block.pmd;
	mem_block.addr = addr & PAGE_MASK;
	if (addr & SECTION_SIZE)
		mem_block.pmd++;
	mem_block.saved_pmd = *mem_block.pmd;
	if ((pmd_val(mem_block.saved_pmd) & PMD_TYPE_MASK) != PMD_TYPE_SECT)
		return;
	*mem_block.pmd &= ~(1 << 15);
	flush_pmd_entry(mem_block.pmd_to_flush);
	flush_tlb_kernel_page(mem_block.addr);
	mem_block.made_writeable = 1;
}
void mem_block_restore(void)
{
	if (mem_block.made_writeable) {
		*mem_block.pmd = mem_block.saved_pmd;
		flush_pmd_entry(mem_block.pmd_to_flush);
		flush_tlb_kernel_page(mem_block.addr);
	}
}
void mem_block_write_32_word(unsigned long *addr, unsigned long word, int is_change_writeable)
{
	unsigned long flags;
	if (is_change_writeable) {
		mem_block_spinlock(&flags);
		mem_block_writeable((unsigned long)addr);
	}
	*addr = word;
	flush_icache_range((unsigned long)addr,
			   ((unsigned long)addr + sizeof(long)));
	if (is_change_writeable) {
		mem_block_restore();
		mem_block_spinunlock(&flags);
	}
}
#else
static struct {
	pmd_t *pmd;
	pte_t *pte;
	pmd_t saved_pmd;
	pte_t saved_pte;
	bool made_writeable;
} mem_block;
struct aarch64_insn_patch {
	void *addr;
	unsigned long insn;
	int insn_length;
	int is_change_writeable;
	atomic_t cpu_count;
};
void mem_block_writeable(u64 addr)
{
	pmdval_t prot_sect_kernel;
	pgd_t *pgd = pgd_offset(&init_mm, addr);
	pud_t *pud = pud_offset(pgd, addr);
	u64 addr_aligned;
	mem_block.made_writeable = 0;
#if __is_check_address_range_
	if ((addr < (u64)_stext) || (addr >= (u64)__start_rodata))
		return;
#endif
	prot_sect_kernel =
	    PMD_TYPE_SECT | PMD_SECT_AF | PMD_ATTRINDX(MT_NORMAL);
#ifdef CONFIG_SMP
	prot_sect_kernel |= PMD_SECT_S;
#endif
	mem_block.pmd = pmd_offset(pud, addr);
	addr_aligned = addr & PAGE_MASK;
	mem_block.saved_pmd = *mem_block.pmd;
	if ((pmd_val(mem_block.saved_pmd) & PMD_TYPE_MASK) == PMD_TYPE_SECT) {
		set_pmd(mem_block.pmd,
			__pmd(__pa(addr_aligned) | prot_sect_kernel));
	} else {
		mem_block.pte = pte_offset_kernel(mem_block.pmd, addr);
		mem_block.saved_pte = *mem_block.pte;
		set_pte(mem_block.pte,
			pfn_pte(__pa(addr) >> PAGE_SHIFT, PAGE_KERNEL_EXEC));
	}
	flush_tlb_kernel_range(addr, addr + PAGE_SIZE);
	mem_block.made_writeable = 1;
}
void mem_block_restore(u64 addr)
{
	if (mem_block.made_writeable) {
		if ((pmd_val(mem_block.saved_pmd) & PMD_TYPE_MASK) == PMD_TYPE_SECT)
			*mem_block.pmd = mem_block.saved_pmd;
		else
			*mem_block.pte = mem_block.saved_pte;
		flush_tlb_kernel_range(addr, addr + PAGE_SIZE);
	}
}
void mem_block_write_64_word(unsigned long *addr, unsigned long word, int len , int is_change_writeable)
{
	unsigned long flags;
	if (is_change_writeable) {
		mem_block_spinlock(&flags);
		mem_block_writeable((unsigned long)addr);
	}
	if (len == sizeof(int))
		*(unsigned int *)addr = (unsigned int)word;
	else
		*addr = word;
	flush_icache_range((unsigned long)addr,
			   ((unsigned long)addr + sizeof(long)));
	if (is_change_writeable) {
		mem_block_restore((unsigned long)addr);
		mem_block_spinunlock(&flags);
	}
}
int mem_block_write_64_word_sync_callback(void *arg)
{
	struct aarch64_insn_patch *patch = (struct aarch64_insn_patch *)arg;
	if (atomic_inc_return(&patch->cpu_count) == 1) {
		mem_block_write_64_word(patch->addr, patch->insn, patch->insn_length, patch->is_change_writeable);
		atomic_inc(&patch->cpu_count);
	} else {
		while (atomic_read(&patch->cpu_count) <= num_online_cpus())
			cpu_relax();
		isb();
	}
	return 0;
}
void mem_block_write_64_word_sync(unsigned long *addr, unsigned long word, int len, int is_change_writeable)
{
	struct aarch64_insn_patch patch = {
		.addr = addr,
		.insn = word,
		.insn_length = len,
		.is_change_writeable = is_change_writeable,
		.cpu_count = ATOMIC_INIT(0),
	};
	stop_machine(mem_block_write_64_word_sync_callback, &patch, cpu_online_mask);
	return;
}
#endif
/*redirect_handler begin*/
static inline void __register_redirect_handler(struct qihoo_patch_object *qpobj, const void *table[])
{
	void *addr = &table[qpobj->o_func_addr];
	uintptr_t insn = qpobj->n_func_addr;
	qpobj->o_instruction = (uintptr_t)sys_call_table[qpobj->o_func_addr];
	pr_debug("redirect hook @ %p\n", (void *)qpobj->o_func_addr);
#if __SIZEOF_POINTER__ == 4
	mem_block_write_32_word(addr, insn, 1);
#else
	mem_block_write_64_word(addr, insn, sizeof(long), 1);
#endif
}
static inline void
__unregister_redirect_handler(struct qihoo_patch_object *qpobj, const void *table[])
{
	void *addr = &table[qpobj->o_func_addr];
	uintptr_t insn = qpobj->o_instruction;
	pr_debug("redirect unhook @ %p\n", (void *)qpobj->o_func_addr);
#if __SIZEOF_POINTER__ == 4
	mem_block_write_32_word(addr, insn, 1);
#else
	mem_block_write_64_word(addr, insn, sizeof(long), 1);
#endif
}
/*redirect_handler end*/
/*break_handler begin*/
static inline void __register_break_handler(struct qihoo_patch_object *qpobj, int is_change_writeable)
{
	void *addr = (void *)qpobj->o_func_addr;
	uintptr_t insn = qpobj->n_func_addr;
	qpobj->o_instruction = *(unsigned int *)addr;
	pr_debug("break hook @ %p %p %p\n", (void *)qpobj->o_func_addr,
		 (void *)qpobj->o_instruction, (void *)insn);
#if __SIZEOF_POINTER__ == 4
	mem_block_write_32_word(addr, insn, is_change_writeable);
#else
	if(qpobj->type == e_patch_sync_break || qpobj->type == e_module_patch_sync_break){
		mem_block_write_64_word_sync(addr, insn, sizeof(int), is_change_writeable);
	} else {
		mem_block_write_64_word(addr, insn, sizeof(int), is_change_writeable);
	}
#endif
}
static inline void __unregister_break_handler(struct qihoo_patch_object *qpobj, int is_change_writeable)
{
	void *addr = (void *)qpobj->o_func_addr;
	unsigned long insn = qpobj->o_instruction;
	pr_debug("break unhook @ %p\n", (void *)qpobj->o_func_addr);
#if __SIZEOF_POINTER__ == 4
	mem_block_write_32_word(addr, insn, is_change_writeable);
#else
	if(qpobj->type == e_patch_sync_break || qpobj->type == e_module_patch_sync_break){
		mem_block_write_64_word_sync(addr, insn, sizeof(int), is_change_writeable);
	} else {
		mem_block_write_64_word(addr, insn, sizeof(int), is_change_writeable);
	}
#endif
}
/*break_handler end*/
/*patch_object begin*/
static inline struct qihoo_patch_object *
_init_patch_object(struct qihoo_patch_object *_cur_qpobj)
{
	struct qihoo_patch_object *_new_qpobj;
	_new_qpobj = kzalloc(sizeof(struct qihoo_patch_object), GFP_KERNEL);
	if (!_new_qpobj)
		return NULL;
	_new_qpobj->type = _cur_qpobj->type;
	_new_qpobj->o_func_addr = _cur_qpobj->o_func_addr;
	_new_qpobj->n_func_addr = _cur_qpobj->n_func_addr;
	_new_qpobj->next = NULL;
	return _new_qpobj;
}
static inline void _free_patch_object(struct qihoo_patch_object *_cur_qpobj)
{
	struct qihoo_patch_object *qpobj_pre;
	if (_cur_qpobj == NULL) {
		return;
	}
	if (_cur_qpobj->next == NULL) {
		kfree(_cur_qpobj);
		return;
	}
	while (_cur_qpobj->next != NULL) {
		qpobj_pre = _cur_qpobj;
		_cur_qpobj = _cur_qpobj->next;
		kfree(qpobj_pre);
		qpobj_pre = NULL;
		if (_cur_qpobj->next == NULL) {
			kfree(_cur_qpobj);
			break;
		}
	}
	return;
}
/*patch_object end*/
static struct qihoo_patch_object *
find_patch_object(struct qihoo_patch_object *_qpobj)
{
	struct qihoo_patch_object *cur = NULL;
	mutex_lock(&qpobj_list_mutex);
	list_for_each_entry(cur, &qpobj_list, list_member)
	{
		if (cur->o_func_addr == _qpobj->o_func_addr) {
			mutex_unlock(&qpobj_list_mutex);
			return cur;
		}
	}
	mutex_unlock(&qpobj_list_mutex);
	return NULL;
}
#ifdef CONFIG_MODULES
#ifdef CONFIG_DEBUG_SET_MODULE_RONX
static inline void set_page_attributes(void *start, void *end, int (*set)(unsigned long start, int num_pages))
{
	unsigned long begin_pfn = PFN_DOWN((unsigned long)start);
	unsigned long end_pfn = PFN_DOWN((unsigned long)end);
	if (end_pfn > begin_pfn)
		set(begin_pfn << PAGE_SHIFT, end_pfn - begin_pfn);
}
#else
static inline void set_page_attributes(void *start, void *end, int (*set)(unsigned long start, int num_pages)){}
#endif
#endif
static inline void _qihoo_patch_register(struct qihoo_patch_object *_qpobj)
{
#ifdef CONFIG_MODULES
	struct module *mod = NULL;
#endif
	switch (_qpobj->type) {
	case e_patch_break:
	case e_patch_sync_break: {
		__register_break_handler(_qpobj, 1);
		break;
	}
	case e_patch_redirect: {
		__register_redirect_handler(_qpobj, sys_call_table);
		break;
	}
#ifdef CONFIG_COMPAT
	case e_patch_compat_redirect: {
		__register_redirect_handler(_qpobj, compat_sys_call_table);
		break;
	}
#endif
#ifdef CONFIG_MODULES
	case e_module_patch_break:
	case e_module_patch_sync_break: {
		mod = __module_text_address(_qpobj->o_func_addr);
		if (mod) {
			__module_get(mod);
			set_page_attributes(mod->core_layout.base, mod->core_layout.base + mod->core_layout.text_size, set_memory_rw);
			__register_break_handler(_qpobj, 0);
			set_page_attributes(mod->core_layout.base, mod->core_layout.base + mod->core_layout.text_size, set_memory_ro);
		}
		break;
	}
#endif
	default: {
		return;
	}
	}
	_qpobj->mod = e_patch_mod_in;
}
static inline void _qihoo_patch_unregister(struct qihoo_patch_object *_qpobj)
{
#ifdef CONFIG_MODULES
	struct module *mod = NULL;
#endif
	switch (_qpobj->type) {
	case e_patch_break:
	case e_patch_sync_break: {
		__unregister_break_handler(_qpobj, 1);
		break;
	}
	case e_patch_redirect: {
		__unregister_redirect_handler(_qpobj, sys_call_table);
		break;
	}
#ifdef CONFIG_COMPAT
	case e_patch_compat_redirect: {
		__unregister_redirect_handler(_qpobj, compat_sys_call_table);
		break;
	}
#endif
#ifdef CONFIG_MODULES
	case e_module_patch_break:
	case e_module_patch_sync_break: {
		mod = __module_text_address(_qpobj->o_func_addr);
		if (mod) {
			set_page_attributes(mod->core_layout.base, mod->core_layout.base + mod->core_layout.text_size, set_memory_rw);
			__unregister_break_handler(_qpobj, 0);
			set_page_attributes(mod->core_layout.base, mod->core_layout.base + mod->core_layout.text_size, set_memory_ro);
			module_put(mod);
		}
		break;
	}
#endif
	default: {
		return;
	}
	}
	_qpobj->mod = e_patch_mod_out;
}
int qihoo_patch_register(struct qihoo_patch_object *_qpobj)
{
	int ret = e_qihoo_hook_success;
	struct qihoo_patch_object *qpobj_head;
	struct qihoo_patch_object *qpobj;
	struct qihoo_patch_object *qpobj_pre;
	if (_qpobj == NULL || ((_qpobj->o_func_addr == 0) && !(_qpobj->type == e_patch_redirect || _qpobj->type == e_patch_compat_redirect))) {
		pr_err("in o_func_addr is 0\n");
		return -EOLDADDR;
	}
	if (_qpobj->n_func_addr == 0) {
		if (!(_qpobj->type == e_patch_redirect || _qpobj->type == e_patch_compat_redirect))
			return -ENEWADDR;
		pr_err("in n_func_addr is 0\n");
	}
	qpobj = find_patch_object(_qpobj);
	if (qpobj == NULL) {
		/*
			this branch don't check qpobj->mod, success: qpobj->mod
		   =e_patch_mod_in ,fail: return
		*/
		qpobj = _init_patch_object(_qpobj);
		if (qpobj == NULL)
			return -EINITOBJ;
	} else if (qpobj->mod == e_patch_mod_in &&
		   _qpobj->mod == e_patch_mod_update) {
		/*this mod have memory leak , when qihoo_patch_unregister
		execut qpobj's code segment isn't free*/
		ret = qihoo_patch_unregister(qpobj);
		if (ret == e_qihoo_hook_fail)
			return ret;
		qpobj = _init_patch_object(_qpobj);
		if (qpobj == NULL)
			return -EINITOBJ;
	} else {
		/*if find the object but _qpobj->mod != e_patch_mod_update
		return e_qihoo_hook_fail*/
		return -EEXEFUNC;
	}
	/*add others patch*/
	qpobj_head = qpobj_pre = qpobj;
	while (_qpobj->next != NULL) {
		_qpobj = _qpobj->next;
		qpobj = _init_patch_object(_qpobj);
		if (qpobj == NULL) {
			_free_patch_object(qpobj_head);
			return -EINITOBJ;
		}
		qpobj_pre->next = qpobj;
		qpobj_pre = qpobj;
	}
	mutex_lock(&qpobj_list_mutex);
	list_add(&qpobj_head->list_member, &qpobj_list);
	mutex_unlock(&qpobj_list_mutex);
	qpobj = qpobj_head;
	while (qpobj) {
		_qihoo_patch_register(qpobj);
		qpobj = qpobj->next;
	}
	return ret;
}
EXPORT_SYMBOL(qihoo_patch_register);
int qihoo_patch_unregister(struct qihoo_patch_object *_qpobj)
{
	int ret = e_qihoo_hook_success;
	struct qihoo_patch_object *qpobj_head;
	struct qihoo_patch_object *qpobj;
	if (_qpobj == NULL || ((_qpobj->o_func_addr == 0) && !(_qpobj->type == e_patch_redirect || _qpobj->type == e_patch_compat_redirect))) {
		pr_err("ou o_func_addr is 0\n");
		return -EOLDADDR;
	}
	if (_qpobj->n_func_addr == 0) {
		if (!(_qpobj->type == e_patch_redirect || _qpobj->type == e_patch_compat_redirect))
			return -ENEWADDR;
		pr_err("ou n_func_addr is 0\n");
	}
	qpobj_head = qpobj = find_patch_object(_qpobj);
	if (qpobj == NULL)
		return e_qihoo_hook_fail;
	if (qpobj->mod == e_patch_mod_out)
		return e_qihoo_hook_success;
	_qihoo_patch_unregister(qpobj);
	while (qpobj->next != NULL) {
		qpobj = qpobj->next;
		_qihoo_patch_unregister(qpobj);
	}
	mutex_lock(&qpobj_list_mutex);
	list_del(&qpobj_head->list_member);
	mutex_unlock(&qpobj_list_mutex);
	_free_patch_object(qpobj_head);
	return ret;
}
EXPORT_SYMBOL(qihoo_patch_unregister);
#ifdef CONFIG_MODULES
int __weak init_extern(void) { return 0; }
int __weak exit_extern(void) { return 0; }
#endif
int qihoo_engine_init(void)
{
	int ret = 0;
#ifdef CONFIG_MODULES
	ret = init_extern();
	if (ret != 0)
		return ret;
#endif
	pr_debug("init\n");
	return ret;
}
void qihoo_engine_exit(void)
{
	struct qihoo_patch_object *cur = NULL;
	struct list_head *list_cur = NULL;
	struct list_head *safe = NULL;
	struct qihoo_patch_object *qpobj = NULL;
	mutex_lock(&qpobj_list_mutex);
	list_for_each_safe(list_cur, safe, &qpobj_list)
	{
		cur = list_entry(list_cur, struct qihoo_patch_object,
				 list_member);
		if (cur->mod == e_patch_mod_in) {
			qpobj = cur;
			_qihoo_patch_unregister(qpobj);
			while (qpobj->next != NULL) {
				qpobj = qpobj->next;
				_qihoo_patch_unregister(qpobj);
			}
		}
		list_del(&cur->list_member);
		_free_patch_object(cur);
	}
	mutex_unlock(&qpobj_list_mutex);
#ifdef CONFIG_MODULES
	exit_extern();
#endif
	pr_debug("exit\n");
}
