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
#pragma once
#include <linux/list.h>
#include <linux/types.h>

#define PATCH_VERSION "kernel patch version v1.2.010\n"
/*define ERRNO*/
#define ENOPROC 4070    /*no process  and return error*/
#define EOUTMEM 4071    /*out memmory length error*/
#define EREPATCH 4072   /*repeate load patch error*/
#define EREUNPATCH 4073 /*repeate unload patch error*/
#define EINCMD 4074     /*input CMD error*/
#define ENODAT 4075     /*patch data is null  */
#define ESYMDAT 4076    /*patch sym table error*/
#define ECHKSIG 4077    /*patch check signature error*/
#define EPNOMEM 4078    /*patch alloc memory error*/
#define EMEMLEAK 4079   /*kmem leak error*/
#define ESETPATCH 4080  /*setup load patch error*/
#define EPERCPU 4081    /*alloc percpu error*/
#define ESYMANA 4082    /*analysis symbol error*/
#define ERELOCATE 4083  /*patch relocate error*/
#define EPOSTR 4084     /*post relocate error*/
#define ENOINIT 4085    /*no init function error*/
#define ENOEXIT 4086    /*no exit function error*/
#define EEXEFUNC 4087   /*execute patch's init function error*/
#define ENOPATCH 4088   /*no load this patch*/
#define EPDATA 4089     /*patch data error*/
#define ENOFILE 4090    /*patch find file error*/
#define ESYMMD5 4091    /*patch sym data error*/
#define EPLTMEM 4092    /*patch alloc plt mem error*/
#define EOLDADDR 4093   /*patch o_func_addr null error*/
#define ENEWADDR 4094   /*patch n_func_addr null error*/
#define EINITOBJ 4095   /*patch init_patch_object error*/

#pragma pack(1)
struct _s_head_ {
	uint8_t type;
	uint32_t len;
};
struct __s_kernel_symbol_ {
	unsigned long addr;
	const char *name;
};
struct __s_kernel_symbol_table_ {
	uint16_t count;
	struct __s_kernel_symbol_ *symbols;
};
#pragma pack()
struct qihoo_patch_object {
	struct list_head list_member;
	uintptr_t o_func_addr;
	uintptr_t n_func_addr;
	uintptr_t o_instruction;
	uint8_t mod;
	uint8_t type;
	struct qihoo_patch_object *next;
};
enum e_patch_mod {
	e_patch_mod_out = 0,
	e_patch_mod_in,
	e_patch_mod_update,
	e_patch_mod_end
};
enum e_patch_type {
	e_patch_break = 0,
	e_patch_redirect,
	e_patch_compat_redirect,
	e_module_patch_break,
	e_patch_sync_break,
	e_module_patch_sync_break,
	e_patch_end,
};
int qihoo_engine_init(void);
void qihoo_engine_exit(void);
int qihoo_patch_register(struct qihoo_patch_object *);
int qihoo_patch_unregister(struct qihoo_patch_object *);
struct __s_qihoo_patch_entry_args_ {
	const char *name;
	struct qihoo_patch_object **obj;
};
struct __s_qihoo_patch_entry_object_ {
	int (*init)(struct qihoo_patch_object **);
	int (*exit)(struct qihoo_patch_object **);
	int argc;
	struct __s_qihoo_patch_entry_args_ *argv;
};

struct sec_file {
	struct list_head list;
	unsigned long ino;
	uint32_t  sid;
	uint16_t  sclass;
};
unsigned long find_symbol_patch(const char *name,
				struct __s_kernel_symbol_table_ *table);
struct __s_kernel_symbol_table_ *get_symbols(char *sym);
void put_symbols(struct __s_kernel_symbol_table_ *table);
