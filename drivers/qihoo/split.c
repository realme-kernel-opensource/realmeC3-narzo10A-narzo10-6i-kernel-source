/*
 * drivers/qihoo/split.c for kernel hotfix
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
#include <linux/fb.h>
#include <linux/kernel.h>
#include <linux/slab.h>
#include <linux/string.h>
#include <linux/types.h>
#define e_qihoo_split_success 0
#define e_qihoo_split_fail -1
#ifdef CONFIG_MODULES
#pragma pack(1)
#ifndef ESYMDAT
#define ESYMDAT 0x10007 /*patch sym table error*/
#endif
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
#else
#include "qihoo.h"
#endif
#define MALLOC(x) kmalloc(x, GFP_KERNEL)
#define REALLOC(x, y) krealloc(x, y, GFP_KERNEL)
#define FREE(x) kfree(x)
#define STRTOUL(x, y, z) simple_strtoul(x, y, z)
char *__strtok_r(char *s, const char *delim, char **last)
{
	char *spanp, *tok;
	int c, sc;
	if (s == NULL) {
		if (last == NULL)
			return NULL;
		s = *last;
		if (s == NULL)
			return NULL;
	}
cont:
	c = *s++;
	for (spanp = (char *)delim; (sc = *spanp++) != 0;) {
		if (c == sc)
			goto cont;
	}
	if (c == 0) {
		*last = NULL;
		return NULL;
	}
	tok = s - 1;
	for (;;) {
		c = *s++;
		spanp = (char *)delim;
		do {
			sc = *spanp++;
			if (sc == c) {
				if (c == 0)
					s = NULL;
				else
					s[-1] = '\0';
				*last = s;
				return tok;
			}
		} while (sc != 0);
	}
}
#define TABLE_BASE_LEN 128
struct __s_kernel_symbol_table_ *get_symbols(char *sym)
{
	char *p;
	char *opt_ptr = NULL;
	char *line = sym;
	char *opt[2];
	int i = 0;
	int table_base_len = TABLE_BASE_LEN;
	struct __s_kernel_symbol_table_ *table;
	if (sym == NULL)
		return ERR_PTR(-ESYMDAT);
	table = (struct __s_kernel_symbol_table_ *)MALLOC(
	    sizeof(struct __s_kernel_symbol_table_));
	if (table == NULL) {
		pr_err("patch getSymbol malloc memmory error!\n");
		return ERR_PTR(-ENOMEM);
	}
	table->count = 0;
	table->symbols = NULL;
	table->symbols = (struct __s_kernel_symbol_ *)MALLOC(
	    sizeof(struct __s_kernel_symbol_) * table_base_len);
	if (!(table->symbols)) {
		pr_err("patch getSymbol malloc memmory error!\n");
		FREE(table);
		return ERR_PTR(-ENOMEM);
	}
	while ((p = __strtok_r(line, ",", &opt_ptr)) != NULL) {
		opt[i] = p;
		i++;
		line = NULL;
		if (i == 2) {
			if (table->count > table_base_len) {
				table_base_len += 16;
				table->symbols =
				    (struct __s_kernel_symbol_ *)REALLOC(
					table->symbols,
					sizeof(struct __s_kernel_symbol_) *
					    table_base_len);
				if (!(table->symbols)) {
					pr_err("%s remalloc memmory error!\n",
					       __func__);
					FREE(table);
					return ERR_PTR(-ENOMEM);
				}
			}
			table->symbols[table->count].addr =
			    STRTOUL(opt[0], NULL, 0);
			;
			table->symbols[table->count].name = opt[1];
			i = 0;
			table->count++;
		}
	}
	return table;
}
void put_symbols(struct __s_kernel_symbol_table_ *table)
{
	if (table && table->symbols) {
		FREE(table->symbols);
		table->symbols = NULL;
		FREE(table);
	}
}
unsigned long find_symbol_patch(const char *name,
				struct __s_kernel_symbol_table_ *table)
{
	int i = 0;
	if (name == NULL || table == NULL || table->count == 0 ||
	    table->symbols == NULL) {
		return 0;
	}
	for (i = 0; i < table->count; i++) {
		if (strcmp(name, table->symbols[i].name) == 0)
			return table->symbols[i].addr;
	}
	return 0;
}
#ifdef CONFIG_MODULES
#include <linux/vmalloc.h>
static struct __s_kernel_symbol_table_ *kernel_symbol_table;
static void *sym;
int __verifySymbols(void *data, unsigned long len)
{
	struct _s_head_ *head = NULL;
	void *p = data;
	void *end = data + len;
	kernel_symbol_table = NULL;
	sym = NULL;
	if (data == NULL || len == 0)
		goto fail;
	while (p < end) {
		if ((end - p) < sizeof(struct _s_head_))
			goto fail;
		head = (struct _s_head_ *)p;
		p = p + sizeof(struct _s_head_);
		if (head->len == 0 || head->len > (end - p))
			goto fail;
		switch (head->type) {
		case 0:
		case 1:
		case 2: {
			break;
		}
		default: {
			goto fail;
		}
		}
		p = p + head->len;
	}
	if (p != end)
		goto fail;
	pr_info("__verifySymbols succ\n");
	return e_qihoo_split_success;
fail:
	pr_info("__verifySymbols fail\n");
	return e_qihoo_split_fail;
}
void *__getSymbols(void *data, unsigned long len, unsigned long *elf_len)
{
	struct _s_head_ *head = NULL;
	void *elf = NULL;
	void *p = data;
	void *end = data + len;
	kernel_symbol_table = NULL;
	sym = NULL;
	if (data == NULL || len == 0 || elf_len == NULL)
		return NULL;
	while (p < end) {
		if ((end - p) < sizeof(struct _s_head_))
			goto end;
		head = (struct _s_head_ *)p;
		switch (head->type) {
		case 0: {
			*elf_len = head->len;
			elf = vmalloc(head->len);
			if (elf == NULL)
				goto end;
			memcpy(elf, (void *)head + sizeof(struct _s_head_),
			       head->len);
			pr_debug("elf len is %u", head->len);
			break;
		}
		case 1: {
			sym = vmalloc(head->len);
			if (sym == NULL)
				goto end;
			memcpy(sym, (void *)head + sizeof(struct _s_head_),
			       head->len);
			kernel_symbol_table = get_symbols(sym);
			pr_debug("sym len is %u", head->len);
			break;
		}
		default: {
			break;
		}
		}
		p = p + sizeof(struct _s_head_) + head->len;
	}
	return elf;
end:
	if (sym) {
		vfree(sym);
		sym = NULL;
	}
	if (elf) {
		vfree(elf);
		elf = NULL;
	}
	return NULL;
}
void __putSymbols(void)
{
	put_symbols(kernel_symbol_table);
	kernel_symbol_table = NULL;
	if (sym) {
		vfree(sym);
		sym = NULL;
	}
}
unsigned long __findSymbol(const char *name)
{
	return find_symbol_patch(name, kernel_symbol_table);
}
#endif
