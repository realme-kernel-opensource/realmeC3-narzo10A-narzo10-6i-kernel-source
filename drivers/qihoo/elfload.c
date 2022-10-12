/*
 * drivers/qihoo/elfload.c for kernel hotfix
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
#include <asm/memory.h>
#include <asm/page.h>
#include <asm/param.h>
#include <asm/uaccess.h>
#include <linux/binfmts.h>
#include <linux/cdev.h>
#include <linux/compiler.h>
#include <linux/coredump.h>
#include <linux/device.h>
#include <linux/elf.h>
#include <linux/elfcore.h>
#include <linux/errno.h>
#include <linux/fcntl.h>
#include <linux/file.h>
#include <linux/fs.h>
#include <linux/highmem.h>
#include <linux/highuid.h>
#include <linux/init.h>
#include <linux/kallsyms.h>
#include <linux/kernel.h>
#include <linux/linkage.h>
#include <linux/list.h>
#include <linux/mm.h>
#include <linux/mman.h>
#include <linux/pagemap.h>
#include <linux/personality.h>
#include <linux/random.h>
#include <linux/sched.h>
#include <linux/security.h>
#include <linux/crypto.h>
#include <crypto/hash.h>
#include <crypto/md5.h>
#include <linux/scatterlist.h>
#include <linux/signal.h>
#include <linux/version.h>
#include <linux/slab.h>
#include <linux/string.h>
#include <linux/utsname.h>
#include <linux/utsname.h>
#include <linux/vmalloc.h>
#include <linux/syscalls.h>
#include <linux/jhash.h>
#include <linux/namei.h>
#include <linux/fsnotify.h>
#include <linux/security.h>
#include <generated/compile.h>
#include <asm-generic/sections.h>
#include <linux/dynamic_debug.h>
#include "elfload.h"
#include "qihoo.h"
#ifdef CONFIG_USE_FUNCTION_ADDRESS
#include "extern.h"
#include "qihooattr.h"
#else
#define DYN_ALLOC_MEM
#endif
#ifdef CONFIG_KASAN
#include <linux/kasan.h>
#define MODULE_ALIGN (PAGE_SIZE << KASAN_SHADOW_SCALE_SHIFT)
#else
#define MODULE_ALIGN PAGE_SIZE
#endif

#ifdef CONFIG_QIHOO_SIGN_CHECK
const bool volatile sign_flag = 1;
#else
const bool volatile sign_flag = 0;
#endif
#ifdef CONFIG_QIHOO_KERNEL_CHECK
const bool volatile kernel_flag = 1;
#else
const bool volatile kernel_flag = 0;
#endif

static struct cdev *my_dev;
static dev_t md;
void *mp;
struct class *qihoo_class;

unsigned char kernel_info[256] = {0};
unsigned char version_digest[ 2 * MD5_DIGEST_SIZE + 1] = {0};

#define MAX_FILE_TABLE 1024
#define MAX_FILE_TABLE_MASK 0x3FF
#ifndef DYN_ALLOC_MEM
static const void (*func)(void);
static unsigned long *fp;
static const unsigned long *fp_end;
#endif
char *INIT_FUNCTION_NAME = "qihoo_patch_init";
char *EXIT_FUNCTION_NAME = "qihoo_patch_exit";
static unsigned long init_addr;
static unsigned long exit_addr;
#ifdef CONFIG_QIHOO_USE_KERNEL_FRAME
char *QIHOO_PATCH_ENTRY_OBJECT = "__qihoo_patch_entry_object__";
struct __s_qihoo_patch_entry_object_ *__qihoo_patch_entry_object_ = NULL;
#ifdef CONFIG_MODULES
char *PATCH_MODULE_NAME = "patch_module_name";
extern const char *(*patch_module_name_func)(void);
#endif
extern int qihoo_patch_init(struct __s_qihoo_patch_entry_object_ *__qihoo_patch_entry_object_);
extern void qihoo_patch_exit(struct __s_qihoo_patch_entry_object_ *__qihoo_patch_entry_object_);
#endif
#ifdef MMAP_KMALLOC
#define LEN (4096 * 32)
#else
#define LEN (4096 * 32)
#endif
struct load_info {
	Elf_Ehdr *hdr;
	unsigned long len;
	Elf_Shdr *sechdrs;
	char *secstrings, *strtab;
	unsigned long symoffs, stroffs;
	struct _ddebug *debug;
	unsigned int num_debug;
	bool sig_ok;
	struct {
		unsigned int sym, str, mod, vers, info, pcpu;
	} index;
};

struct load_addr {
	struct list_head list;
	unsigned long begin;
	unsigned long end;
	unsigned long init;
	unsigned long exit;
	unsigned long mod_addr;
	unsigned int id;
	bool load_flag; /*1: load ok;0:unload*/
	bool load_type; /*0:patch init;1:kernel init*/
};

static LIST_HEAD(patch_addrs);
static LIST_HEAD(local_patches);
LIST_HEAD(sec_table);
#ifdef LKM_NOT_BUILD_IN
struct list_head *patches = (void *)__modules_addr;
const struct exception_table_entry *(*_search_extable)(const struct exception_table_entry *first,
	const struct exception_table_entry *last, unsigned long value) = (void *)__search_extable_addr;
static void __percpu *(*___alloc_reserved_percpu)(size_t size, size_t align) =
	(void *)____alloc_reserved_percpu_addr;
#else
extern const struct exception_table_entry *search_extable(const struct exception_table_entry *first,
	const struct exception_table_entry *last, unsigned long value);
extern void __percpu *__alloc_reserved_percpu(size_t size, size_t align);

const struct exception_table_entry *(*_search_extable)(const struct exception_table_entry *first,
	const struct exception_table_entry *last, unsigned long value) = (void *)search_extable;
static void __percpu *(*___alloc_reserved_percpu)(size_t size, size_t align) =
	(void *)__alloc_reserved_percpu;
#endif
/* Flags for sys_finit_module: */
#define MODULE_INIT_IGNORE_MODVERSIONS 1
#define MODULE_INIT_IGNORE_VERMAGIC 2
DEFINE_MUTEX(patch_mutex);
static int __init qihoo_set_sign_flag(char *str)
{
	bool *b;
	if (!str)
		return 0;
	b = (bool *)&sign_flag;
	if(!strcmp(str,"0"))
		*b = 0;
	return 1;
}
__setup("patch_sign_flag=", qihoo_set_sign_flag);


#if LINUX_VERSION_CODE >= KERNEL_VERSION(4,4,0)
#ifndef MODULES_VSIZE
#ifdef CONFIG_64BIT
#define MODULES_VSIZE		(SZ_64M)
#else
#define MODULES_VSIZE		(SZ_16M)
#endif
#endif
#define PATCH_START ((unsigned long)_etext - MODULES_VSIZE)
#define PATCH_END   (PATCH_START + MODULES_VSIZE - (unsigned long)(_etext - _stext))
#else
#define PATCH_START MODULES_VADDR
#define PATCH_END   MODULES_END
#endif

void *patch_alloc(unsigned long size)
{
#ifdef VMALLOC_NODE_RANGE_PARA_8
	return __vmalloc_node_range(size, 1, PATCH_START, PATCH_END,
				GFP_KERNEL, PAGE_KERNEL_EXEC, NUMA_NO_NODE,
				__builtin_return_address(0));
#else
#ifdef CONFIG_64BIT
	void *p;
	p = __vmalloc_node_range(size, MODULE_ALIGN, PATCH_START, PATCH_END,
				GFP_KERNEL, PAGE_KERNEL_EXEC, 0,
				NUMA_NO_NODE, __builtin_return_address(0));
	if (p && (kasan_module_alloc(p, size) < 0)) {
		vfree(p);
		return NULL;
	}
	return p;
#else
	return __vmalloc_node_range(size, 1, PATCH_START, PATCH_END,
				GFP_KERNEL, PAGE_KERNEL_EXEC, 0, NUMA_NO_NODE,
				__builtin_return_address(0));
#endif
#endif
}
/* Find a module section: 0 means not found. */
static unsigned int find_sec(const struct load_info *info, const char *name)
{
	unsigned int i;
	for (i = 1; i < info->hdr->e_shnum; i++) {
		Elf_Shdr *shdr = &info->sechdrs[i];
		/* Alloc bit cleared means "ignore it." */
		if ((shdr->sh_flags & SHF_ALLOC) &&
		    strcmp(info->secstrings + shdr->sh_name, name) == 0)
			return i;
	}
	return 0;
}

/* Find a module section, or NULL.  Fill in number of "objects" in section. */
static void *section_objs(const struct load_info *info, const char *name,
			  size_t object_size, unsigned int *num)
{
	unsigned int sec = find_sec(info, name);

	/* Section 0 has sh_addr 0 and sh_size 0. */
	*num = info->sechdrs[sec].sh_size / object_size;
	return (void *)info->sechdrs[sec].sh_addr;
}

/* Find a module section, or NULL. */
static void *section_addr(const struct load_info *info, const char *name)
{
	/* Section 0 has sh_addr 0. */
	return (void *)info->sechdrs[find_sec(info, name)].sh_addr;
}

static void find_patch_sections(struct module *mod, struct load_info *info)
{
	mod->kp = section_objs(info, "__param", sizeof(*mod->kp), &mod->num_kp);
	mod->syms = section_objs(info, "__ksymtab", sizeof(*mod->syms), &mod->num_syms);
	mod->crcs = section_addr(info, "__kcrctab");
	mod->gpl_syms = section_objs(info, "__ksymtab_gpl",
					sizeof(*mod->gpl_syms), &mod->num_gpl_syms);
	mod->gpl_crcs = section_addr(info, "__kcrctab_gpl");
	mod->gpl_future_syms = section_objs(info, "__ksymtab_gpl_future",
					    sizeof(*mod->gpl_future_syms),
					    &mod->num_gpl_future_syms);
	mod->gpl_future_crcs = section_addr(info, "__kcrctab_gpl_future");

#ifdef CONFIG_UNUSED_SYMBOLS
	mod->unused_syms = section_objs(info, "__ksymtab_unused", sizeof(*mod->unused_syms),
			 &mod->num_unused_syms);
	mod->unused_crcs = section_addr(info, "__kcrctab_unused");
	mod->unused_gpl_syms = section_objs(info, "__ksymtab_unused_gpl",
					    sizeof(*mod->unused_gpl_syms),
					    &mod->num_unused_gpl_syms);
	mod->unused_gpl_crcs = section_addr(info, "__kcrctab_unused_gpl");
#endif
#ifdef CONFIG_CONSTRUCTORS
	mod->ctors = section_objs(info, ".ctors", sizeof(*mod->ctors), &mod->num_ctors);
#endif

#ifdef CONFIG_TRACEPOINTS
	mod->tracepoints_ptrs = section_objs(info, "__tracepoints_ptrs",
			 sizeof(*mod->tracepoints_ptrs), &mod->num_tracepoints);
#endif
#ifdef HAVE_JUMP_LABEL
	mod->jump_entries = section_objs(info, "__jump_table", sizeof(*mod->jump_entries),
			 &mod->num_jump_entries);
#endif
#ifdef CONFIG_EVENT_TRACING
	mod->trace_events = section_objs(info, "_ftrace_events", sizeof(*mod->trace_events),
			 &mod->num_trace_events);
#endif
#ifdef CONFIG_TRACING
	mod->trace_bprintk_fmt_start = section_objs(
	    info, "__trace_printk_fmt", sizeof(*mod->trace_bprintk_fmt_start),
	    &mod->num_trace_bprintk_fmt);
#endif
#ifdef CONFIG_FTRACE_MCOUNT_RECORD
	/* sechdrs[0].sh_size is always zero */
	mod->ftrace_callsites = section_objs(info, "__mcount_loc", sizeof(*mod->ftrace_callsites),
			 &mod->num_ftrace_callsites);
#endif

	mod->extable = section_objs(info, "__ex_table", sizeof(*mod->extable),
				    &mod->num_exentries);

	if (section_addr(info, "__obsparm"))
		printk(KERN_WARNING "%s: Ignoring obsolete parameters\n",
		       mod->name);

	info->debug = section_objs(info, "__verbose", sizeof(*info->debug),
				   &info->num_debug);
}

static int rewrite_section_headers(struct load_info *info, int flags)
{
	unsigned int i;
	/* This should always be true, but let's be sure. */
	info->sechdrs[0].sh_addr = 0;
	for (i = 1; i < info->hdr->e_shnum; i++) {
		Elf_Shdr *shdr = &info->sechdrs[i];
		if (shdr->sh_type != SHT_NOBITS &&
		    info->len < shdr->sh_offset + shdr->sh_size) {
			printk(KERN_ERR "Module len %lu truncated\n",
			       info->len);
			return -ENOEXEC;
		}
		/* Mark all sections sh_addr with their address in the
		   temporary image. */
		shdr->sh_addr = (size_t)info->hdr + shdr->sh_offset;
#ifndef CONFIG_MODULE_UNLOAD
		/* Don't load .exit sections */
		if (strstarts(info->secstrings + shdr->sh_name, ".exit"))
			shdr->sh_flags &= ~(unsigned long)SHF_ALLOC;
#endif
	}
	/* Track but don't keep modinfo and version sections. */
	if (flags & MODULE_INIT_IGNORE_MODVERSIONS)
		info->index.vers = 0; /* Pretend no __versions section! */
	else
		info->index.vers = find_sec(info, "__versions");
	info->index.info = find_sec(info, ".modinfo");
	info->sechdrs[info->index.info].sh_flags &= ~(unsigned long)SHF_ALLOC;
	info->sechdrs[info->index.vers].sh_flags &= ~(unsigned long)SHF_ALLOC;
	return 0;
}

static unsigned int find_pcpusec(struct load_info *info);
/*
 * Set up our basic convenience variables (pointers to section headers,
 * search for module section index etc), and do some basic section
 * verification.
 *
 * Return the temporary module pointer (we'll replace it with the final
 * one when we move the module sections around).
 */
static struct module *setup_load_patch(struct load_info *info, int flags)
{
	unsigned int i;
	int err;
	struct module *mod;
	/* Set up the convenience variables */
	info->sechdrs = (void *)info->hdr + info->hdr->e_shoff;
	info->secstrings = (void *)info->hdr
			+ info->sechdrs[info->hdr->e_shstrndx].sh_offset;
	err = rewrite_section_headers(info, flags);
	if (err)
		return ERR_PTR(err);
	/* Find internal symbols and strings. */
	for (i = 1; i < info->hdr->e_shnum; i++) {
		if (info->sechdrs[i].sh_type == SHT_SYMTAB) {
			info->index.sym = i;
			info->index.str = info->sechdrs[i].sh_link;
			info->strtab = (char *)info->hdr +
				       info->sechdrs[info->index.str].sh_offset;
			break;
		}
	}
#ifdef USE_ELF_MODULE
	info->index.mod = find_sec(info, ".gnu.linkonce.this_module");
	if (!info->index.mod) {
		printk(KERN_WARNING "No module found in object\n");
		return ERR_PTR(-ENOEXEC);
	}
	/* This is temporary: point mod into copy of data. */
	mod = (void *)info->sechdrs[info->index.mod].sh_addr;
#else
	mod = kmalloc(sizeof(struct module), GFP_KERNEL);
	if (!mod) {
		pr_err("patch kmalloc mod fail!\n");
		return ERR_PTR(-ENOSPC);
	}
	memset((void *)mod, 0, sizeof(struct module));
	memcpy(mod->name, "qihoo_patch_module", 18);

#endif
	if (info->index.sym == 0) {
		printk(KERN_WARNING " module has no symbols (stripped?)\n");
		kfree(mod);
		return ERR_PTR(-ENOEXEC);
	}

	info->index.pcpu = find_pcpusec(info);
	pr_debug("patch find_pcpusec =%d\n", info->index.pcpu);
	/* Check module struct version now, before we try to use module. */
	return mod;
}
#ifdef CONFIG_MODULES_USE_ELF_RELA
enum aarch64_reloc_op {
	RELOC_OP_NONE,
	RELOC_OP_ABS,
	RELOC_OP_PREL,
	RELOC_OP_PAGE,
};
#define AARCH64_INSN_IMM_MOVNZ AARCH64_INSN_IMM_MAX
#define AARCH64_INSN_IMM_MOVK AARCH64_INSN_IMM_16
static u64 do_reloc(enum aarch64_reloc_op reloc_op, void *place, u64 val)
{
	switch (reloc_op) {
	case RELOC_OP_ABS:
		return val;
	case RELOC_OP_PREL:
		return val - (u64)place;
	case RELOC_OP_PAGE:
		return (val & ~0xfff) - ((u64)place & ~0xfff);
	case RELOC_OP_NONE:
		return 0;
	}

	pr_err("do_reloc: unknown relocation operation %d\n", reloc_op);
	return 0;
}

static int reloc_data(enum aarch64_reloc_op op, void *place, u64 val, int len)
{
        s64 sval = do_reloc(op, place, val);

        switch (len) {
        case 16:
                *(s16 *)place = sval;
                if (sval < S16_MIN || sval > U16_MAX)
                        return -ERANGE;
                break;
        case 32:
                *(s32 *)place = sval;
                if (sval < S32_MIN || sval > U32_MAX)
                        return -ERANGE;
                break;
        case 64:
                *(s64 *)place = sval;
                break;
        default:
                pr_err("Invalid length (%d) for data relocation\n", len);
                return 0;
        }
        return 0;
}

static int reloc_insn_movw(enum aarch64_reloc_op op, void *place, u64 val,
			   int lsb, enum aarch64_insn_imm_type imm_type)
{
	u64 imm, limit = 0;
	s64 sval;
	u32 insn = le32_to_cpu(*(u32 *)place);

	sval = do_reloc(op, place, val);
	sval >>= lsb;
	imm = sval & 0xffff;

	if (imm_type == AARCH64_INSN_IMM_MOVNZ) {
		/*
		 * For signed MOVW relocations, we have to manipulate the
		 * instruction encoding depending on whether or not the
		 * immediate is less than zero.
		 */
		insn &= ~(3 << 29);
		if ((s64)imm >= 0) {
			/* >=0: Set the instruction to MOVZ (opcode 10b). */
			insn |= 2 << 29;
		} else {
			/*
			 * <0: Set the instruction to MOVN (opcode 00b).
			 *     Since we've masked the opcode already, we
			 *     don't need to do anything other than
			 *     inverting the new immediate field.
			 */
			imm = ~imm;
		}
		imm_type = AARCH64_INSN_IMM_MOVK;
	}

	/* Update the instruction with the new encoding. */
	insn = aarch64_insn_encode_immediate(imm_type, insn, imm);
	*(u32 *)place = cpu_to_le32(insn);

	/* Shift out the immediate field. */
	sval >>= 16;

	/*
	 * For unsigned immediates, the overflow check is straightforward.
	 * For signed immediates, the sign bit is actually the bit past the
	 * most significant bit of the field.
	 * The AARCH64_INSN_IMM_16 immediate type is unsigned.
	 */
	if (imm_type != AARCH64_INSN_IMM_16) {
		sval++;
		limit++;
	}

	/* Check the upper bits depending on the sign of the immediate. */
	if ((u64)sval > limit)
		return -ERANGE;

	return 0;
}

static int reloc_insn_imm(enum aarch64_reloc_op op, void *place, u64 val,
			  int lsb, int len,
			  enum aarch64_insn_imm_type imm_type)
{
	u64 imm, imm_mask;
	s64 sval;
	u32 insn = le32_to_cpu(*(u32 *)place);

	/* Calculate the relocation value. */
	sval = do_reloc(op, place, val);
	sval >>= lsb;

	/* Extract the value bits and shift them to bit 0. */
	imm_mask = (BIT(lsb + len) - 1) >> lsb;
	imm = sval & imm_mask;

	/* Update the instruction's immediate field. */
	insn = aarch64_insn_encode_immediate(imm_type, insn, imm);
	*(u32 *)place = cpu_to_le32(insn);

	/*
	 * Extract the upper value bits (including the sign bit) and
	 * shift them to bit 0.
	 */
	sval = (s64)(sval & ~(imm_mask >> 1)) >> (len - 1);

	/*
	 * Overflow has occurred if the upper bits are not all equal to
	 * the sign bit of the value.
	 */
	if ((u64)(sval + 1) >= 2)
		return -ERANGE;

	return 0;
}

int patch_relocate_add(Elf64_Shdr *sechdrs, const char *strtab,
		       unsigned int symindex, unsigned int relsec,
		       struct module *me)
{
	unsigned int i;
	int ovf;
	bool overflow_check;
	Elf64_Sym *sym;
	void *loc;
	u64 val;
	Elf64_Rela *rel = (void *)sechdrs[relsec].sh_addr;

	for (i = 0; i < sechdrs[relsec].sh_size / sizeof(*rel); i++) {
		/* loc corresponds to P in the AArch64 ELF document. */
		loc = (void *)sechdrs[sechdrs[relsec].sh_info].sh_addr +
		      rel[i].r_offset;

		/* sym is the ELF symbol we're referring to. */
		sym = (Elf64_Sym *)sechdrs[symindex].sh_addr +
		      ELF64_R_SYM(rel[i].r_info);

		/* val corresponds to (S + A) in the AArch64 ELF document. */
		val = sym->st_value + rel[i].r_addend;

		/* Check for overflow by default. */
		overflow_check = true;

		/* Perform the static relocation. */
		switch (ELF64_R_TYPE(rel[i].r_info)) {
			/* Null relocations. */
		case R_ARM_NONE:
		case R_AARCH64_NONE:
			ovf = 0;
			break;

		/* Data relocations. */
		case R_AARCH64_ABS64:
			overflow_check = false;
			ovf = reloc_data(RELOC_OP_ABS, loc, val, 64);
			break;
		case R_AARCH64_ABS32:
			ovf = reloc_data(RELOC_OP_ABS, loc, val, 32);
			break;
		case R_AARCH64_ABS16:
			ovf = reloc_data(RELOC_OP_ABS, loc, val, 16);
			break;
		case R_AARCH64_PREL64:
			overflow_check = false;
			ovf = reloc_data(RELOC_OP_PREL, loc, val, 64);
			break;
		case R_AARCH64_PREL32:
			ovf = reloc_data(RELOC_OP_PREL, loc, val, 32);
			break;
		case R_AARCH64_PREL16:
			ovf = reloc_data(RELOC_OP_PREL, loc, val, 16);
			break;

		/* MOVW instruction relocations. */
		case R_AARCH64_MOVW_UABS_G0_NC:
			overflow_check = false;
		case R_AARCH64_MOVW_UABS_G0:
			ovf = reloc_insn_movw(RELOC_OP_ABS, loc, val, 0,
					      AARCH64_INSN_IMM_16);
			break;
		case R_AARCH64_MOVW_UABS_G1_NC:
			overflow_check = false;
		case R_AARCH64_MOVW_UABS_G1:
			ovf = reloc_insn_movw(RELOC_OP_ABS, loc, val,
					      16, AARCH64_INSN_IMM_16);
			break;
		case R_AARCH64_MOVW_UABS_G2_NC:
			overflow_check = false;
		case R_AARCH64_MOVW_UABS_G2:
			ovf = reloc_insn_movw(RELOC_OP_ABS, loc, val,
					      32, AARCH64_INSN_IMM_16);
			break;
		case R_AARCH64_MOVW_UABS_G3:
			/* We're using the top bits so we can't
			 * overflow. */
			overflow_check = false;
			ovf = reloc_insn_movw(RELOC_OP_ABS, loc, val,
					      48, AARCH64_INSN_IMM_16);
			break;
		case R_AARCH64_MOVW_SABS_G0:
			ovf = reloc_insn_movw(RELOC_OP_ABS, loc, val, 0,
					      AARCH64_INSN_IMM_MOVNZ);
			break;
		case R_AARCH64_MOVW_SABS_G1:
			ovf =
			    reloc_insn_movw(RELOC_OP_ABS, loc, val, 16,
					    AARCH64_INSN_IMM_MOVNZ);
			break;
		case R_AARCH64_MOVW_SABS_G2:
			ovf =
			    reloc_insn_movw(RELOC_OP_ABS, loc, val, 32,
					    AARCH64_INSN_IMM_MOVNZ);
			break;
		case R_AARCH64_MOVW_PREL_G0_NC:
			overflow_check = false;
			ovf = reloc_insn_movw(RELOC_OP_PREL, loc, val,
					      0, AARCH64_INSN_IMM_MOVK);
			break;
		case R_AARCH64_MOVW_PREL_G0:
			ovf = reloc_insn_movw(RELOC_OP_PREL, loc, val, 0,
					    AARCH64_INSN_IMM_MOVNZ);
			break;
		case R_AARCH64_MOVW_PREL_G1_NC:
			overflow_check = false;
			ovf = reloc_insn_movw(RELOC_OP_PREL, loc, val, 16,
					    AARCH64_INSN_IMM_MOVK);
			break;
		case R_AARCH64_MOVW_PREL_G1:
			ovf = reloc_insn_movw(RELOC_OP_PREL, loc, val, 16,
					    AARCH64_INSN_IMM_MOVNZ);
			break;
		case R_AARCH64_MOVW_PREL_G2_NC:
			overflow_check = false;
			ovf = reloc_insn_movw(RELOC_OP_PREL, loc, val, 32,
					    AARCH64_INSN_IMM_MOVK);
			break;
		case R_AARCH64_MOVW_PREL_G2:
			ovf = reloc_insn_movw(RELOC_OP_PREL, loc, val, 32,
					    AARCH64_INSN_IMM_MOVNZ);
			break;
		case R_AARCH64_MOVW_PREL_G3:
			/* We're using the top bits so we can't
			 * overflow. */
			overflow_check = false;
			ovf = reloc_insn_movw(RELOC_OP_PREL, loc, val, 48,
					    AARCH64_INSN_IMM_MOVNZ);
			break;
			/* Immediate instruction relocations. */
		case R_AARCH64_LD_PREL_LO19:
			ovf = reloc_insn_imm(RELOC_OP_PREL, loc, val, 2,
					     19, AARCH64_INSN_IMM_19);
			break;
		case R_AARCH64_ADR_PREL_LO21:
			ovf = reloc_insn_imm(RELOC_OP_PREL, loc, val, 0,
					     21, AARCH64_INSN_IMM_ADR);
			break;
#ifndef CONFIG_ARM64_ERRATUM_843419
		case R_AARCH64_ADR_PREL_PG_HI21_NC:
			overflow_check = false;
		case R_AARCH64_ADR_PREL_PG_HI21:
			ovf = reloc_insn_imm(RELOC_OP_PAGE, loc, val, 12,
					   21, AARCH64_INSN_IMM_ADR);
			break;
#endif
		case R_AARCH64_ADD_ABS_LO12_NC:
		case R_AARCH64_LDST8_ABS_LO12_NC:
			overflow_check = false;
			ovf = reloc_insn_imm(RELOC_OP_ABS, loc, val, 0,
					     12, AARCH64_INSN_IMM_12);
			break;
		case R_AARCH64_LDST16_ABS_LO12_NC:
			overflow_check = false;
			ovf = reloc_insn_imm(RELOC_OP_ABS, loc, val, 1,
					     11, AARCH64_INSN_IMM_12);
			break;
		case R_AARCH64_LDST32_ABS_LO12_NC:
			overflow_check = false;
			ovf = reloc_insn_imm(RELOC_OP_ABS, loc, val, 2,
					     10, AARCH64_INSN_IMM_12);
			break;
		case R_AARCH64_LDST64_ABS_LO12_NC:
			overflow_check = false;
			ovf = reloc_insn_imm(RELOC_OP_ABS, loc, val, 3,
					     9, AARCH64_INSN_IMM_12);
			break;
		case R_AARCH64_LDST128_ABS_LO12_NC:
			overflow_check = false;
			ovf = reloc_insn_imm(RELOC_OP_ABS, loc, val, 4,
					     8, AARCH64_INSN_IMM_12);
			break;
		case R_AARCH64_TSTBR14:
			ovf = reloc_insn_imm(RELOC_OP_PREL, loc, val, 2,
					     14, AARCH64_INSN_IMM_14);
			break;
		case R_AARCH64_CONDBR19:
			ovf = reloc_insn_imm(RELOC_OP_PREL, loc, val, 2,
					     19, AARCH64_INSN_IMM_19);
			break;
		case R_AARCH64_JUMP26:
		case R_AARCH64_CALL26:
			ovf = reloc_insn_imm(RELOC_OP_PREL, loc, val, 2,
					     26, AARCH64_INSN_IMM_26);
			break;
		default:
			pr_err("module %s: unsupported RELA relocation: %llu\n",
				    me->name, ELF64_R_TYPE(rel[i].r_info));
			return -ENOEXEC;
		}

		if (overflow_check && ovf == -ERANGE)
			goto overflow;
	}

	return 0;

overflow:
	pr_err("module %s: overflow in relocation type %d val %Lx\n", me->name,
	       (int)ELF64_R_TYPE(rel[i].r_info), val);
	return -ENOEXEC;
}

#else
static inline int patch_relocate_add(Elf_Shdr *sechdrs, const char *strtab,
				     unsigned int symindex, unsigned int relsec,
				     struct module *me)
{
	printk(KERN_ERR "module %s: REL relocation unsupported\n", me->name);
	return -ENOEXEC;
}
#endif

#ifndef CONFIG_MODULES_USE_ELF_RELA
int patch_apply_relocate(Elf32_Shdr *sechdrs, const char *strtab,
			 unsigned int symindex, unsigned int relindex,
			 struct module *module)
{
	Elf32_Shdr *symsec = sechdrs + symindex;
	Elf32_Shdr *relsec = sechdrs + relindex;
	Elf32_Shdr *dstsec = sechdrs + relsec->sh_info;
	Elf32_Rel *rel = (void *)relsec->sh_addr;
	unsigned int i;
	for (i = 0; i < relsec->sh_size / sizeof(Elf32_Rel); i++, rel++) {
		unsigned long loc;
		Elf32_Sym *sym;
		const char *symname;
		s32 offset;
#ifdef CONFIG_THUMB2_KERNEL
		u32 upper, lower, sign, j1, j2;
#endif
		offset = ELF32_R_SYM(rel->r_info);
		if (offset < 0 ||
		    offset > (symsec->sh_size / sizeof(Elf32_Sym))) {
			pr_err("patch %s: section %u reloc %u: bad relocation sym offset\n",
			    module->name, relindex, i);
			return -ENOEXEC;
		}
		sym = ((Elf32_Sym *)symsec->sh_addr) + offset;
		symname = strtab + sym->st_name;
		if (rel->r_offset < 0 ||
		    rel->r_offset > dstsec->sh_size - sizeof(u32)) {
			pr_err(
			    "patch %s: section %u reloc %u sym '%s': out of "
			    "bounds relocation, offset %d size %u\n",
			    module->name, relindex, i, symname, rel->r_offset,
			    dstsec->sh_size);
			return -ENOEXEC;
		}
		loc = dstsec->sh_addr + rel->r_offset;
		switch (ELF32_R_TYPE(rel->r_info)) {
		case R_ARM_NONE:
			/* ignore */
			break;
		case R_ARM_ABS32:
			*(u32 *)loc += sym->st_value;
			break;
		case R_ARM_PC24:
		case R_ARM_CALL:
		case R_ARM_JUMP24:
			offset = (*(u32 *)loc & 0x00ffffff) << 2;
			if (offset & 0x02000000)
				offset -= 0x04000000;
			offset += sym->st_value - loc;
			if (offset & 3 || offset <= (s32)0xfe000000 ||
			    offset >= (s32)0x02000000) {
				pr_err(
					"patch %s: section %u reloc %u sym %s: "
					"relocation %u out of range%#lx -> %#x\n",
				    module->name, relindex, i, symname,
				    ELF32_R_TYPE(rel->r_info), loc, sym->st_value);
				return -ENOEXEC;
			}
			offset >>= 2;
			*(u32 *)loc &= 0xff000000;
			*(u32 *)loc |= offset & 0x00ffffff;
			break;
		case R_ARM_V4BX:
			/* Preserve Rm and the condition code. Alter
			 * other bits to re-code instruction as
			 * MOV PC,Rm.
			 */
			*(u32 *)loc &= 0xf000000f;
			*(u32 *)loc |= 0x01a0f000;
			break;
		case R_ARM_PREL31:
			offset = *(u32 *)loc + sym->st_value - loc;
			*(u32 *)loc = offset & 0x7fffffff;
			break;
		case R_ARM_MOVW_ABS_NC:
		case R_ARM_MOVT_ABS:
			offset = *(u32 *)loc;
			offset = ((offset & 0xf0000) >> 4) | (offset & 0xfff);
			offset = (offset ^ 0x8000) - 0x8000;
			offset += sym->st_value;
			if (ELF32_R_TYPE(rel->r_info) == R_ARM_MOVT_ABS)
				offset >>= 16;
			*(u32 *)loc &= 0xfff0f000;
			*(u32 *)loc |= ((offset & 0xf000) << 4) | (offset & 0x0fff);
			break;
#ifdef CONFIG_THUMB2_KERNEL
		case R_ARM_THM_CALL:
		case R_ARM_THM_JUMP24:
			upper = *(u16 *)loc;
			lower = *(u16 *)(loc + 2);
			/*
			 * 25 bit signed address range (Thumb-2 BL and
			 * B.W
			 * instructions):
			 *   S:I1:I2:imm10:imm11:0
			 * where:
			 *   S     = upper[10]   = offset[24]
			 *   I1    = ~(J1 ^ S)   = offset[23]
			 *   I2    = ~(J2 ^ S)   = offset[22]
			 *   imm10 = upper[9:0]  = offset[21:12]
			 *   imm11 = lower[10:0] = offset[11:1]
			 *   J1    = lower[13]
			 *   J2    = lower[11]
			 */
			sign = (upper >> 10) & 1;
			j1 = (lower >> 13) & 1;
			j2 = (lower >> 11) & 1;
			offset = (sign << 24) | ((~(j1 ^ sign) & 1) << 23) |
				 ((~(j2 ^ sign) & 1) << 22) |
				 ((upper & 0x03ff) << 12) |
				 ((lower & 0x07ff) << 1);
			if (offset & 0x01000000)
				offset -= 0x02000000;
			offset += sym->st_value - loc;
			/*
			 * For function symbols, only Thumb addresses
			 * are
			 * allowed (no interworking).
			 *
			 * For non-function symbols, the destination
			 * has no specific ARM/Thumb disposition, so
			 * the branch is resolved under the assumption
			 * that interworking is not required.
			 */
			if ((ELF32_ST_TYPE(sym->st_info) == STT_FUNC &&
			     !(offset & 1)) || offset <= (s32)0xff000000 ||
			    offset >= (s32)0x01000000) {
				pr_err(
					"patch %s: section %u reloc %u sym "
					"'%s': relocation %u out of range "
					"(%#lx -> %#x)\n",
					module->name, relindex, i, symname,
					ELF32_R_TYPE(rel->r_info), loc,
					sym->st_value);
				return -ENOEXEC;
			}
			sign = (offset >> 24) & 1;
			j1 = sign ^ (~(offset >> 23) & 1);
			j2 = sign ^ (~(offset >> 22) & 1);
			*(u16 *)loc = (u16)((upper & 0xf800) | (sign << 10) |
				  ((offset >> 12) & 0x03ff));
			*(u16 *)(loc + 2) = (u16)((lower & 0xd000) | (j1 << 13) | (j2 << 11) |
				    ((offset >> 1) & 0x07ff));
			break;
		case R_ARM_THM_MOVW_ABS_NC:
		case R_ARM_THM_MOVT_ABS:
			upper = *(u16 *)loc;
			lower = *(u16 *)(loc + 2);
			/*
			 * MOVT/MOVW instructions encoding in Thumb-2:
			 *
			 * i	= upper[10]
			 * imm4	= upper[3:0]
			 * imm3	= lower[14:12]
			 * imm8	= lower[7:0]
			 *
			 * imm16 = imm4:i:imm3:imm8
			 */
			offset = ((upper & 0x000f) << 12) |
				 ((upper & 0x0400) << 1) |
				 ((lower & 0x7000) >> 4) |
				 (lower & 0x00ff);
			offset = (offset ^ 0x8000) - 0x8000;
			offset += sym->st_value;
			if (ELF32_R_TYPE(rel->r_info) == R_ARM_THM_MOVT_ABS)
				offset >>= 16;
			*(u16 *)loc = (u16)((upper & 0xfbf0) |
					    ((offset & 0xf000) >> 12) |
					    ((offset & 0x0800) >> 1));
			*(u16 *)(loc + 2) = (u16)((lower & 0x8f00) |
					  ((offset & 0x0700) << 4) |
					  (offset & 0x00ff));
			break;
#endif
		default:
			printk(KERN_ERR "patch %s: unknown relocation: %u\n",
			       module->name, ELF32_R_TYPE(rel->r_info));
			return -ENOEXEC;
		}
		/*pr_debug("patch relocate loc =%#lx,loc value =%0x,name =%s,addr =%0x,offset =%d\n",
			    loc, *(u32 *)loc, symname, sym->st_value, offset);*/
	}
	return 0;
}
#else
static inline int patch_apply_relocate(Elf_Shdr *sechdrs, const char *strtab,
				       unsigned int symindex,
				       unsigned int relsec, struct module *me)
{
	printk(KERN_ERR
	       "module patch_apply_relocate: REL relocation unsupported\n");
	return -ENOEXEC;
}

#endif
static int patch_relocations(struct module *mod, const struct load_info *info)
{
	unsigned int i;
	int err = 0;
	/* Now do relocations. */
	for (i = 1; i < info->hdr->e_shnum; i++) {
		unsigned int infosec = info->sechdrs[i].sh_info;
		/* Not a valid relocation section? */
		if (infosec >= info->hdr->e_shnum)
			continue;
		/* Don't bother with non-allocated sections */
		if (!(info->sechdrs[infosec].sh_flags & SHF_ALLOC))
			continue;
		if (info->sechdrs[i].sh_type == SHT_REL)
			err = patch_apply_relocate(info->sechdrs, info->strtab,
						   info->index.sym, i, mod);
		else if (info->sechdrs[i].sh_type == SHT_RELA)
			err = patch_relocate_add(info->sechdrs, info->strtab,
						 info->index.sym, i, mod);
		if (err < 0)
			break;
	}
	return err;
}
#define USE_SYS_FUNCTION

#ifndef USE_SYS_FUNCTION
/* Resolve a symbol for this module.  I.e. if we find one, record usage. */
static const struct kernel_symbol *resolve_symbol(struct module *mod,
						  const struct load_info *info,
						  const char *name,
						  char ownername[])
{
	struct module *owner;
	const struct kernel_symbol *sym;
	const unsigned long *crc;
	int err;

	mutex_lock(&patch_mutex);

	sym = find_symbol(name, &owner, &crc,
			!(mod->taints & (1 << TAINT_PROPRIETARY_MODULE)), true);

	mutex_unlock(&patch_mutex);
	return sym;
}

#endif
static struct kernel_symbol *resolve_symbol_wait(struct module *mod,
						 const struct load_info *info,
						 const char *name)
{
	struct kernel_symbol *ksym;
#ifdef USE_SYS_FUNCTION
	unsigned long addr = kallsyms_lookup_name(name);
	static struct kernel_symbol content = {};
	if (addr > 0) {
		content.name = name;
		content.value = addr;
	} else {
		pr_err("patch find address error! func name =%s\n", name);
		return ERR_PTR(-ENOENT);
	}
	ksym = &content;
#else
	char owner[MODULE_NAME_LEN];
	if (wait_event_interruptible_timeout(module_wq,
		!IS_ERR(ksym = resolve_symbol(mod, info, name, owner)) ||
		    PTR_ERR(ksym) != -EBUSY, 30 * HZ) <= 0) {
		printk(KERN_WARNING
		       "%s: gave up waiting for init of module %s.\n",
		       mod->name, owner);
	}
#endif
	return ksym;
}

/*mod per cpu*/

#ifdef CONFIG_SMP

static inline void __percpu *mod_percpu(struct module *mod)
{
	return mod->percpu;
}

static int percpu_patchalloc(struct module *mod, unsigned long size,
			     unsigned long align)
{
	if (align > PAGE_SIZE) {
		printk(KERN_WARNING "%s: per-cpu alignment %li > %li\n",
		       mod->name, align, PAGE_SIZE);
		align = PAGE_SIZE;
	}

	mod->percpu = ___alloc_reserved_percpu(size, align);
	if (!mod->percpu) {
		printk(KERN_WARNING
		       "%s: Could not allocate %lu bytes percpu data\n",
		       mod->name, size);
		return -ENOMEM;
	}
	mod->percpu_size = size;
	return 0;
}
static void percpu_patchfree(struct module *mod)
{
	free_percpu(mod->percpu);
}

static unsigned int find_pcpusec(struct load_info *info)
{
	return find_sec(info, ".data..percpu");
}

static void percpu_patchcopy(struct module *mod, const void *from,
			     unsigned long size)
{
	int cpu;

	for_each_possible_cpu(cpu)
	    memcpy(per_cpu_ptr(mod->percpu, cpu), from, size);
}
#else
static inline void __percpu *mod_percpu(struct module *mod)
{
	return NULL;
}

static inline int percpu_patchalloc(struct module *mod, unsigned long size,
				    unsigned long align)
{
	return -ENOMEM;
}
static inline void percpu_patchfree(struct module *mod)
{
}
static unsigned int find_pcpusec(struct load_info *info)
{
	return 0;
}
static inline void percpu_patchcopy(struct module *mod, const void *from,
				    unsigned long size)
{
	/* pcpusec should be 0, and size of that section should be 0. */
	BUG_ON(size != 0);
}

#endif

/* Change all symbols so that st_value encodes the pointer directly. */

static int patch_symbols(struct module *mod, const struct load_info *info,
			 struct __s_kernel_symbol_table_ *table, uint8_t count)
{
	Elf_Shdr *symsec = &info->sechdrs[info->index.sym];
	Elf_Sym *sym = (void *)symsec->sh_addr;
	unsigned long secbase;
	unsigned int i;
	int ret = 0;
	unsigned long elf_addr = 0;
	const struct kernel_symbol *ksym;
#ifdef CONFIG_RANDOMIZE_BASE
    u64 const kernel_offset = kimage_vaddr - KIMAGE_VADDR;
#endif
#ifdef CONFIG_QIHOO_USE_KERNEL_FRAME
#ifdef CONFIG_MODULES
	patch_module_name_func = NULL;
#endif
#endif
	for (i = 1; i < symsec->sh_size / sizeof(Elf_Sym); i++) {
		const char *name = info->strtab + sym[i].st_name;
		switch (sym[i].st_shndx) {
		case SHN_COMMON:
			/* We compiled with -fno-common.  These are not
			   supposed to happen.  */
			pr_debug("Common symbol: %s\n", name);
			pr_debug("%s: please compile with -fno-common\n",
			    mod->name);
			ret = -ENOEXEC;
			break;
		case SHN_ABS:
			/* Don't need to do anything */
			pr_debug("Absolute symbol: 0x%08lx\n",
				 (long)sym[i].st_value);
			break;
		case SHN_UNDEF:
			if (!count)
				ksym = resolve_symbol_wait(mod, info, name);
			else
				ksym = NULL;
			elf_addr = find_symbol_patch(name, table);

#ifdef CONFIG_RANDOMIZE_BASE
                        if (IS_ENABLED(CONFIG_RANDOMIZE_BASE) && kernel_offset > 0 && elf_addr) {
				/*pr_debug("patch symbol  name =%s, offset = %#llx, "
					"elf old  addr =%#lx\n",
					name, kernel_offset, (unsigned long)elf_addr);*/
                                elf_addr = elf_addr + kernel_offset;
                        }
#endif
			/* Ok if resolved.  */
			if (ksym && !IS_ERR(ksym)) {
				if (!elf_addr) {
					sym[i].st_value = ksym->value;
				} else if (elf_addr == ksym->value) {
					sym[i].st_value = ksym->value;
				} else {
					pr_err(
						"patch symbol error  name =%s,system "
						"addr =%#lx,elf addr =%#lx\n",
						name, ksym->value, (unsigned long)elf_addr);
					return -ENOENT;
				}
				break;
			}
			if (elf_addr) {
				sym[i].st_value = elf_addr;
				break;
			}
			/* Ok if weak.  */
			if (!ksym && ELF_ST_BIND(sym[i].st_info) == STB_WEAK)
				break;
			printk(KERN_WARNING "%s: Unknown symbol %s (err %li)\n",
			       mod->name, name, PTR_ERR(ksym));
			ret = PTR_ERR(ksym) ?: -ENOENT;
			break;
		default:
/* Divert to percpu allocation if a percpu var. */
			/*pr_debug("patch sym[i].st_shndx =%d,index.pcpu =%d\n",
			    sym[i].st_shndx, info->index.pcpu);*/
			if (sym[i].st_shndx == info->index.pcpu)
				secbase = (unsigned long)mod_percpu(mod);
			else
				secbase = info->sechdrs[sym[i].st_shndx].sh_addr;
			sym[i].st_value += secbase;
			break;
		}
		if (!strcmp(name, INIT_FUNCTION_NAME)) {
			init_addr = (unsigned long)sym[i].st_value;
			/*pr_debug("patch simple symbol, qihoo_patch_init name =%s,addr =%#lx\n",
			    name, (unsigned long)sym[i].st_value);*/
		} else if (!strcmp(name, EXIT_FUNCTION_NAME)) {
			exit_addr = (unsigned long)sym[i].st_value;
			/*pr_debug("patch simple symbol, qihoo_patch_exit name =%s,addr =%#lx\n",
			    name, (unsigned long)sym[i].st_value);*/
#ifdef CONFIG_QIHOO_USE_KERNEL_FRAME
		} else if (!strcmp(name, QIHOO_PATCH_ENTRY_OBJECT)) {
			__qihoo_patch_entry_object_ = (struct __s_qihoo_patch_entry_object_ *)sym[i].st_value;
			/*pr_debug("patch simple symbol, qihoo_entry_addr =%s,addr =%#lx\n",
			    name, (unsigned long)sym[i].st_value);*/
#ifdef CONFIG_MODULES
		} else if (!strcmp(name, PATCH_MODULE_NAME)) {
			patch_module_name_func = (const char *(*)(void))sym[i].st_value;
			/*pr_debug("patch simple symbol, qihoo_entry_addr =%s,addr =%#lx\n",
			    name, (unsigned long)sym[i].st_value);*/
#endif
#endif
		} else {
			/*pr_debug("patch final symbol, name =%s,addr =%#lx\n",
				 name, (unsigned long)sym[i].st_value);*/
		}
	}
	return ret;
}

static int alloc_patch_percpu(struct module *mod, struct load_info *info)
{
	Elf_Shdr *pcpusec = &info->sechdrs[info->index.pcpu];
	if (!pcpusec->sh_size)
		return 0;

	/* We have a special allocation for this section. */
	return percpu_patchalloc(mod, pcpusec->sh_size, pcpusec->sh_addralign);
}

int __weak module_finalize(const Elf_Ehdr *hdr, const Elf_Shdr *sechdrs,
			   struct module *me)
{
	return 0;
}

static int post_relocation(struct module *mod, const struct load_info *info)
{
	/* Sort exception table now relocations are done. */
	sort_extable(mod->extable, mod->extable + mod->num_exentries);

	/* Copy relocated percpu area over. */
	percpu_patchcopy(mod, (void *)info->sechdrs[info->index.pcpu].sh_addr,
			 info->sechdrs[info->index.pcpu].sh_size);

	/* Setup kallsyms-specific fields. */
	/*	add_kallsyms(mod, info);*/

	/* Arch-specific module finalizing. */
	return module_finalize(info->hdr, info->sechdrs, mod);
}

static void flush_patch_icache(unsigned long begin, unsigned long end)
{
	mm_segment_t old_fs;

	/* flush the icache in correct context */
	old_fs = get_fs();
	set_fs(KERNEL_DS);

	/*
	 * Flush the instruction cache, since we've played with text.
	 * Do it before processing of module parameters, so the module
	 * can provide parameter accessor functions of its own.
	 */
	flush_icache_range(begin, end);

	set_fs(old_fs);
}

#ifdef UNFORM_MOD
/* Search for module by name: must hold module_mutex. */
static struct module *find_patch_all(const char *name, bool even_unformed)
{
	struct module *mod;

	list_for_each_entry(mod, &patches, list) {
		if (!even_unformed && mod->state == MODULE_STATE_UNFORMED)
			continue;
		if (strcmp(mod->name, name) == 0)
			return mod;
	}
	return NULL;
}
#endif
/*
 * We try to place it in the list now to make sure it's unique before
 * we dedicate too many resources.  In particular, temporary percpu
 * memory exhaustion.
 */
static int add_unformed_patch(struct module *mod)
{
	int err = 0;
#ifdef UNFORM_MOD
	struct module *old;

	mod->state = MODULE_STATE_UNFORMED;

again:
	mutex_lock(&patch_mutex);
	old = find_patch_all(mod->name, true);
	if (old != NULL) {
		if (old->state == MODULE_STATE_COMING ||
		    old->state == MODULE_STATE_UNFORMED) {
			/* Wait in case it fails to load. */
			mutex_unlock(&patch_mutex);
			err = wait_event_interruptible(module_wq,
				finished_loading(mod->name));
			if (err)
				goto out_unlocked;
			goto again;
		}
		err = -EEXIST;
		goto out;
	}
	list_add_rcu(&mod->list, &local_patches);
	err = 0;

out:
	mutex_unlock(&patch_mutex);
out_unlocked:
#endif
	return err;
}

const struct exception_table_entry *search_qihoo_patch_extables(unsigned long addr)
{
	const struct exception_table_entry *e = NULL;
	struct module *mod;

	preempt_disable();
	list_for_each_entry_rcu(mod, &local_patches, list) {
		if (mod->num_exentries == 0)
			continue;
		e = _search_extable(mod->extable, mod->extable + mod->num_exentries - 1, addr);
		if (e)
			break;
	}
	preempt_enable();
	return e;
}
#define CONFIG_RELATE_SYM 1

#ifdef CONFIG_RELATE_SYM
struct __s_kernel_symbol_table_ *get_relate_sym(void *data, uint32_t len, void **elf,
				      unsigned long *elf_len)
{
	struct _s_head_ *head = NULL;
	struct _s_head_ *elf_head = NULL;
	struct _s_head_ *sym_head = NULL;
	int ret = 0;
	void *sym = NULL;
	void *p = data;
	void *end = data + len;
	struct __s_kernel_symbol_table_ *table = NULL;
	unsigned int s_len = sizeof(struct _s_head_);
	*elf = NULL;
	*elf_len = 0;
	if (!data || len < 1) {
		pr_err("patch get relate sym error\n");
		return ERR_PTR(-ENODAT);
	}

	while ((p + s_len) < end) {
		head = (struct _s_head_ *)p;

		if (head->len > len) {
			pr_err("patch get relate sym len out memory\n");
			return ERR_PTR(-ESYMDAT);
		}
		switch (head->type) {
		case 0: {
			elf_head = head;
			*elf = (void *)head + s_len;
			*elf_len = head->len;
			break;
		}
		case 1: {
			sym_head = head;
			sym = (void *)head + s_len;
			table = get_symbols(sym);
			break;
		}
		case 2: {
			ret = 2 * MD5_DIGEST_SIZE + 1;
			if (head->len == ret) {
				ret = strncmp((char *)head + s_len, version_digest, 2 * MD5_DIGEST_SIZE);
				if (ret) {
					pr_err("patch data digest info error\n");
					if (kernel_flag)
						return ERR_PTR(-ESYMMD5);
				}

			} else {
				pr_err("patch data digest info len  error\n");
				if (kernel_flag)
					return ERR_PTR(-ESYMDAT);
			}
			break;
		}
		default:
			break;
		}
		p = p + s_len + head->len;
	}
	return table;
}
#endif

/* Sanity checks against invalid binaries, wrong arch, weird elf version. */
static int patch_elf_header_check(struct load_info *info)
{
	if (info->len < sizeof(*(info->hdr)))
		return -ENOEXEC;

	if (memcmp(info->hdr->e_ident, ELFMAG, SELFMAG) != 0
	    || info->hdr->e_type != ET_REL
	    || !elf_check_arch(info->hdr)
	    || info->hdr->e_shentsize != sizeof(Elf_Shdr))
		return -ENOEXEC;

	if (info->hdr->e_shoff >= info->len
	    || (info->hdr->e_shnum * sizeof(Elf_Shdr) >
		info->len - info->hdr->e_shoff))
		return -ENOEXEC;

	return 0;
}

#ifdef CONFIG_GENERIC_BUG
static inline unsigned long bug_addr(const struct bug_entry *bug)
{
#ifndef CONFIG_GENERIC_BUG_RELATIVE_POINTERS
	return bug->bug_addr;
#else
	return (unsigned long)bug + bug->bug_addr_disp;
#endif
}

/* Updates are protected by module mutex */
static LIST_HEAD(patch_bug_list);

struct bug_entry *qihoo_patch_find_bug(unsigned long bugaddr)
{
	struct module *mod;
	struct bug_entry *bug = NULL;

	rcu_read_lock_sched();
	list_for_each_entry_rcu(mod, &patch_bug_list, bug_list) {
		unsigned i;

		bug = mod->bug_table;
		for (i = 0; i < mod->num_bugs; ++i, ++bug)
			if (bugaddr == bug_addr(bug))
				goto out;
	}
	bug = NULL;
out:
	rcu_read_unlock_sched();

	return bug;
}

void patch_bug_finalize(const Elf_Ehdr *hdr, const Elf_Shdr *sechdrs,
			 struct module *mod)
{
	char *secstrings;
	unsigned int i;

	lockdep_assert_held(&patch_mutex);

	mod->bug_table = NULL;
	mod->num_bugs = 0;

	/* Find the __bug_table section, if present */
	secstrings = (char *)hdr + sechdrs[hdr->e_shstrndx].sh_offset;
	for (i = 1; i < hdr->e_shnum; i++) {
		if (strcmp(secstrings+sechdrs[i].sh_name, "__bug_table"))
			continue;
		mod->bug_table = (void *) sechdrs[i].sh_addr;
		mod->num_bugs = sechdrs[i].sh_size / sizeof(struct bug_entry);
		break;
	}

	/*
	 * Strictly speaking this should have a spinlock to protect against
	 * traversals, but since we only traverse on BUG()s, a spinlock
	 * could potentially lead to deadlock and thus be counter-productive.
	 * Thus, this uses RCU to safely manipulate the bug list, since BUG
	 * must run in non-interruptive state.
	 */
	list_add_rcu(&mod->bug_list, &patch_bug_list);
}

void patch_bug_cleanup(struct module *mod)
{
	lockdep_assert_held(&patch_mutex);
	list_del_rcu(&mod->bug_list);
}
#else

static inline struct bug_entry *qihoo_patch_find_bug(unsigned long bugaddr)
{
	return NULL;
}
#endif

static int load_patch(unsigned char *buf, unsigned int len, uint32_t id, uint8_t count,
		      uintptr_t *fun_addr, uintptr_t *end_addr)
{
	int ret = 0;
	void *buf_ex = NULL;
	unsigned long (*foo)(void);
	struct load_info info = {};
	struct module *mod;
	int mod_flags = 0;
	void *elf = NULL;
	unsigned long elf_len = 0;
	struct load_addr *patch_addr = NULL;
	bool select_func = 0;
#ifdef CONFIG_RELATE_SYM
	struct __s_kernel_symbol_table_ *table = NULL;
#endif
#ifndef DYN_ALLOC_MEM
	static unsigned long *fp_cur;
	unsigned long vaddr;
#endif
#ifdef FLUSH_DCACHE
	unsigned long addr = (unsigned int)func;
	struct page *page = NULL;
	int i = 0;
#endif
	init_addr = 0;
	exit_addr = 0;
#ifdef CONFIG_QIHOO_USE_KERNEL_FRAME
	__qihoo_patch_entry_object_ = NULL;
#endif
	elf = buf;
	elf_len = len;
	/*check signal*/
	ret = qh_verify_sig((void *)buf, len, &elf, &elf_len);
	if (sign_flag && (ret != 0)) {
		pr_err("qh_verify_sig failed, ret.%d\n", ret);
		return -ECHKSIG;
	}
	buf = elf;
	len = elf_len;
/*create symtable*/
#ifdef CONFIG_RELATE_SYM
	table = get_relate_sym(buf, len, &elf, &elf_len);
	if (IS_ERR(table)) {
		ret = PTR_ERR(table);
		return ret;
	}

	if (!table || !(table->symbols) || !elf || (elf_len < 1)) {
		put_symbols(table);
		return -ESYMDAT;
	}
	pr_debug("patch get-relate sym elf =%#lx,elf_len=%ld\n",
		 (unsigned long)elf, elf_len);
#endif
/*allocate patch memmory*/
#ifdef DYN_ALLOC_MEM
	buf_ex = patch_alloc(elf_len + 1);
	if (!buf_ex) {
		pr_err("patch vmalloc  fail !\n");
#ifdef CONFIG_RELATE_SYM
		put_symbols(table);
#endif
		return -EPNOMEM;
	}
	/*
	 * The pointer to this block is stored in the module structure
	 * which is inside the block. Just mark it as not being a
	 * leak.
	 */
	kmemleak_not_leak(buf_ex);
	if (!buf_ex) {
		ret = -EMEMLEAK;
		mod = NULL;
		goto free_all;
	}
	memset(buf_ex, 0, elf_len + 1);
	pr_debug("patch buf_ex dynamic address =%#lx\n", (unsigned long)buf_ex);
#else
	/*between fun1 and fun2 with NOP separation*/
	fp_cur = fp;
	buf_ex = (void *)(fp + 1);
	fp += ((elf_len >> 2) + 1);
	if (fp > fp_end) {
		pr_err("patch not enough kernel memory!\n");
		fp = fp_cur;
		return -EPNOMEM;
	}
	pr_debug("patch fcur=%#lx, fp =%#lx,fp_end =%#lx\n",
		 (unsigned long)fp_cur, (unsigned long)fp,
		 (unsigned long)fp_end);

#endif

#ifndef DYN_ALLOC_MEM
	/*set mem writable*/
	vaddr = ((unsigned long)func + 0x1000) & PAGE_MASK;
	set_page_rw(vaddr);
#endif

	memcpy((char *)buf_ex, elf, elf_len);

	info.hdr = buf_ex;
	info.len = elf_len;
	/*check elf header*/
	ret = patch_elf_header_check(&info);
	if (ret < 0) {
		pr_err("patch add_unformed_patch error! errno=%#x\n", ret);
		ret = -EPDATA;
		mod = NULL;
		goto free_all;
	}

	mod = setup_load_patch(&info, mod_flags);
	if (IS_ERR(mod)) {
		pr_err("patch load info error!,errno=%#lx\n", PTR_ERR(mod));
		ret = -ESETPATCH;
		mod = NULL;
		goto free_all;
	}

	/* We will do a special allocation for per-cpu sections later. */
	info.sechdrs[info.index.pcpu].sh_flags &= ~(unsigned long)SHF_ALLOC;

	/* To avoid stressing percpu allocator, do this once we're unique. */

	/* Reserve our place in the list. */
	ret = add_unformed_patch(mod);
	if (ret) {
		pr_err("patch add_unformed_patch error! errno=%#x\n", ret);
		goto free_all;
	}

	ret = alloc_patch_percpu(mod, &info);
	if (ret) {
		pr_err("patch alloc_patch_percpu error! errno=%#x\n", ret);
		ret = -EPERCPU;
		goto free_all;
	}

	/* Now we've got everything in the final locations, we can
	 * find optional sections. */
	find_patch_sections(mod, &info);

	ret = patch_symbols(mod, &info, table, count);
	if (ret < 0) {
		pr_err("patch patch_symbols error! errno=%#x\n", ret);
		ret = -ESYMANA;
		goto free_percpu;
	}
	ret = patch_relocations(mod, &info);
	if (ret < 0) {
		pr_err("patch relocate error! errno=%#x\n", ret);
		ret = -ERELOCATE;
		goto free_percpu;
	}
	ret = post_relocation(mod, &info);
	if (ret < 0) {
		pr_err("patch post relocate error! errno%#x\n", ret);
		ret = -EPOSTR;
		goto free_percpu;
	}

	/*end mem modify and flush*/
	flush_patch_icache((unsigned long)buf_ex,
			   (unsigned long)((unsigned char *)buf_ex + elf_len));

#ifdef CONFIG_GENERIC_BUG
	mutex_lock(&patch_mutex);
	/* This relies on patch_mutex for list integrity. */
	patch_bug_finalize(info.hdr, info.sechdrs, mod);
	mutex_unlock(&patch_mutex);
#endif

	if (!init_addr) {
#ifdef CONFIG_QIHOO_USE_KERNEL_FRAME
		if (__qihoo_patch_entry_object_) {
			select_func = 1;
		} else {
			pr_err("patch not found init entry addr!\n");
			ret = -ENOINIT;
			goto free_percpu;
		}
#else
		pr_err("patch not found init function!\n");
		ret = -ENOINIT;
		goto free_percpu;
#endif
	}

	mutex_lock(&patch_mutex);
	list_add(&mod->list, &local_patches);
	mutex_unlock(&patch_mutex);
	if (!select_func) {
		foo = (void *)init_addr;
		ret = (*foo)();
		pr_debug("patch exec patch init function\n");
	}
#ifdef CONFIG_QIHOO_USE_KERNEL_FRAME
	else {
		ret = qihoo_patch_init(__qihoo_patch_entry_object_);
		pr_debug("patch exec patch frame init function\n");
	}
#endif
	if (ret < 0) {
		pr_err("patch function execute error!errno=%#x\n", ret);
		mutex_lock(&patch_mutex);
		list_del(&mod->list);
		mutex_unlock(&patch_mutex);
		if (ret == -1)
			ret = -EEXEFUNC;
		goto free_percpu;
	}
	/*pr_debug("patch init  function process ok! function add =%#lx\n",
		 (unsigned long)foo);*/
	/*save mod address */
	patch_addr = kmalloc(sizeof(struct load_addr), GFP_KERNEL);
	if (!patch_addr) {
		pr_err("patch kmalloc patch_addr fail!\n");
		mutex_lock(&patch_mutex);
		list_del(&mod->list);
		mutex_unlock(&patch_mutex);
		return -EPNOMEM;
		goto free_percpu;
	}
	patch_addr->begin = (unsigned long)buf_ex;
	patch_addr->end = (unsigned long)((char *)buf_ex + elf_len);
	if (!select_func)
		patch_addr->init = init_addr;
#ifdef CONFIG_QIHOO_USE_KERNEL_FRAME
	else
		patch_addr->init = (unsigned long)__qihoo_patch_entry_object_;
#endif
	patch_addr->exit = exit_addr;
	patch_addr->mod_addr = (unsigned long)mod;
	patch_addr->id = id;
	patch_addr->load_flag = 1;
	patch_addr->load_type = select_func;
	mutex_lock(&patch_mutex);
	list_add(&patch_addr->list, &patch_addrs);
	mutex_unlock(&patch_mutex);
	pr_debug("patch load sucess!\n");
	*fun_addr = patch_addr->begin;
	*end_addr = patch_addr->end;
	goto finish_load;
free_percpu:
	percpu_patchfree(mod);
free_all:
#ifdef DYN_ALLOC_MEM
	vfree(buf_ex);
#else
	fp = fp_cur;
#endif
	if (mod)
		kfree(mod);

finish_load:
#ifdef CONFIG_RELATE_SYM
	put_symbols(table);
#endif
#ifdef COPY_OUT_RESULT
	memcpy(elf, (char *)buf_ex, elf_len);
#endif
	return ret;
}

struct inode_security_struct {
	struct inode *inode;	/* back pointer to inode object */
	union {
	struct list_head list;	/* list of inode_security_struct */
	struct rcu_head rcu;	/* for freeing the inode_security_struct */
									};
	u32 task_sid;		/* SID of creating task */
	u32 sid;		/* SID of this object */
	u16 sclass;		/* security class of this object */
	unsigned char initialized;	/* initialization flag */
	struct mutex lock;
};


static int mem_open(struct inode *inode, struct file *filp)
{
	filp->private_data = mp;
	return 0;
}
static int mem_mmap(struct file *filp, struct vm_area_struct *vma)
{
	int err;
	unsigned long size;
#ifdef MMAP_KMALLOC
	unsigned long start;
	unsigned long pfn;
#endif
	size = vma->vm_end - vma->vm_start;
	if (size > LEN) {
		pr_err("patch remap_vmalloc error!size out memory  =%#lx\n", size);
		return -EINVAL;
	}
#ifdef MMAP_KMALLOC
	start = vma->vm_start;
	/* use remap_pfn_range to map phy addr*/
	/* 2 user kmalloc */
	pfn = virt_to_phys(mp);
	err = remap_pfn_range(vma, start, pfn >> 12, size, vma->vm_page_prot);
#else
	/* 1 user vmalloc */
	err = remap_vmalloc_range(vma, mp, 0);
#endif
	if (err < 0)
		pr_err("patch remap_vmalloc error!errno =%d\n", err);

	return err;
}

/*define CMD */
#define PATCH_LOAD_CMD 0x80
#define PATCH_QUERY_CMD 0x50
#define PATCH_UNLOAD_CMD 0x90
#define PATCH_FILE_REPLACE_CMD 0x60
#define PATCH_FILE_UNREPLACE_CMD 0x70
#define MAX_ELF_SIZE 65536 /*0x1<<16*/
static ssize_t mem_read(struct file *file, char __user *buf, size_t count,
			loff_t *ppos)
{
	int ret = 0;
	uint8_t cmd = 0;
	uint8_t cnt = 0;
	uint16_t len = 0;
	uint32_t id = 0;
	uintptr_t fun_addr = 0;
	uintptr_t end_addr = 0;
	struct patch *dat = NULL;
	struct load_addr *addr = NULL;
	void (*foo)(void);

	struct ret_value retv;
	dat = (struct patch *)file->private_data;

	if (!dat) {
		ret = -ENOMEM;
		goto read_out;
	}
	cmd = dat->cmd;
	cnt = dat->count;
	len = dat->size;
	id = dat->id;
	fun_addr = dat->start;
	end_addr = dat->end;
	/*check patch len*/
	if ((len + sizeof(struct patch)) > LEN || len > MAX_ELF_SIZE) {
		ret = -EOUTMEM;
		goto read_out;
	}
	/* check CMD */
	if (cmd != PATCH_LOAD_CMD && cmd != PATCH_UNLOAD_CMD && cmd != PATCH_QUERY_CMD) {
		ret = -EINCMD;
		goto read_out;
	}
	/* Do all the hard work */
	pr_debug("patch memdev read  cmd =%d,len =%d ,address=%#lx\n", cmd, len,
		 fun_addr);
	switch (cmd) {
	case PATCH_LOAD_CMD: {
		mutex_lock(&patch_mutex);
		list_for_each_entry(addr, &patch_addrs, list) {
			if (addr && (addr->id == id)) {
				pr_err("patch repeate load patch error!\n");
				ret = -EREPATCH;
				mutex_unlock(&patch_mutex);
				goto read_out;
			}
		}
		mutex_unlock(&patch_mutex);

		ret = load_patch((unsigned char *)(dat + 1), len, id, cnt,
			       	&fun_addr, &end_addr);
		if (ret < 0)
			pr_err("patch fail load module !\n");


		retv.start = fun_addr;
		retv.end = end_addr;

		pr_debug("patch load module finish!\n");
		break;
	}
	case PATCH_UNLOAD_CMD: {
		ret = -ENOPATCH;
		mutex_lock(&patch_mutex);
		list_for_each_entry(addr, &patch_addrs, list) {
			if (addr && (addr->begin == fun_addr)) {
				if (!addr->load_flag) {
					pr_err("patch unload repeate error!\n");
					ret = -EREUNPATCH;
					break;
				}
				if (!addr->exit && !addr->load_type) {
					pr_err("patch no exit funciton error!\n");
					ret = -ENOEXIT;
					break;
				}
				if (!addr->load_type) {
					foo = (void *)addr->exit;
					(*foo)();
				}
#ifdef CONFIG_QIHOO_USE_KERNEL_FRAME
				else {
					qihoo_patch_exit((struct __s_qihoo_patch_entry_object_ *)addr->init);
					pr_debug("patch exec entry exit!\n");
				}
#endif
				pr_debug("patch unload ok!\n");
				ret = 0;
				list_del(&addr->list);
				addr->load_flag = 0;
				list_add_tail(&addr->list, &patch_addrs);
				break;
			}
		}
		mutex_unlock(&patch_mutex);
		retv.start = 0;
		retv.end = 0;

		break;
	}
	case PATCH_FILE_REPLACE_CMD: {
		ret = -ENOPROC;
		break;
	}
	case PATCH_FILE_UNREPLACE_CMD: {
		ret = -ENOPROC;
		break;
	}
	case PATCH_QUERY_CMD: {
		ret = strlen(kernel_info);
		if(copy_to_user(buf, kernel_info, ret))
			ret = -EFAULT;
		return ret;
		/*break;*/
	}
	default:
		ret = -ENOPROC;
	}
read_out:
	if (dat)
		memset(dat, 0, sizeof(struct patch));
	retv.errno = ret;
	ret = sizeof(struct ret_value);
	if(copy_to_user(buf, &retv, ret))
		ret = -EFAULT;
	return ret;
}

static ssize_t mem_write(struct file *file, const char __user *buf,
			 size_t count, loff_t *ppos)
{
	return -ENOPROC;
}

static long mem_ioctl(struct file *filep, unsigned int cmd, unsigned long arg)
{
	return -ENOPROC;
}

static const struct file_operations mmap_fops = {
	.owner = THIS_MODULE,
	.open = mem_open,
	.mmap = mem_mmap,
	.read = mem_read,
	.write = mem_write,
	.unlocked_ioctl = mem_ioctl,
};

static int __init map_init(void)
{
	int ret = 0;
	struct device *qihoo_device;
	int i = 0;
	struct crypto_shash *tfm;
	struct shash_desc *desc;
	char version[__NEW_UTS_LEN * 4] = {0};
	char digest[MD5_DIGEST_SIZE] = {0};
	printk(PATCH_VERSION);

#ifdef MMAP_KMALLOC
	/*  user kmalloc */
	mp = kmalloc(LEN, GFP_KERNEL);
/*SetPageReserved(virt_to_page(mp));*/
/*  end */
#else
	/*  use vmalloc */
	mp = vmalloc_user(LEN);
#endif
	if (!mp) {
		pr_err("patch malloc mem fail!\n");
		return -ENOMEM;
	}
	pr_debug("patch mp address =%#lx\n", (unsigned long)mp);
	memset(mp, 0, LEN);

#ifndef DYN_ALLOC_MEM
	/*function check */
	func = (void *)qihoo_patch;

	fp = (unsigned long *)((unsigned char *)func + FUNCTION_END_OFFSET);
	fp_end = fp;
	fp = (unsigned long *)func;
#endif
	/*create cdev and alloc page */
	my_dev = cdev_alloc();
	if (!my_dev) {
		pr_err("patch cdev_alloc  fail!\n");
		ret = -ENOMEM;
		goto free_mmap;
	}
	cdev_init(my_dev, &mmap_fops);
	ret = alloc_chrdev_region(&md, 0, 1, "memdev");
	if (ret < 0) {
		pr_err("patch alloc_chrdev_region  fail!\n");
		goto free_dev;
	}
	pr_debug("patch major=%d,minor=%d\n", MAJOR(md), MINOR(md));
	my_dev->owner = THIS_MODULE;
	ret = cdev_add(my_dev, md, 1);
	if (ret < 0) {
		pr_err("patch cdev_add  fail!\n");
		goto free_region;
	}
	/*devfs create  */
	qihoo_class = class_create(THIS_MODULE, "memdev");
	if (IS_ERR(qihoo_class)) {
		ret =  PTR_ERR(qihoo_class);
		goto free_device;
	}
	qihoo_device = device_create(qihoo_class, NULL, MKDEV(MAJOR(md),
				MINOR(md)), NULL, "memdev");
	if (IS_ERR(qihoo_device)) {
		pr_err("patch device_create failed!\n");
		ret = PTR_ERR(qihoo_device);
		goto free_class;
	}
	pr_debug("patch memdev device create ok!\n");
	ret = qihoo_engine_init();
	if (ret) {
		pr_err("patch qihoo_engine_init  fail!\n");
		goto free_class;
	}
	/*init kernel info :release + '\t' + version + '\t' +
	 *  UTS_KERNEL_HASH + '\t' + md5(release version/UTS_KERNEL_HASH) */
	strcat(kernel_info, init_utsname()->release);
	strcat(kernel_info, "\t");
	strcat(kernel_info, init_utsname()->version);
	strcat(kernel_info, "\t");
#ifdef UTS_KERNEL_HASH
	strcat(kernel_info, UTS_KERNEL_HASH);
#endif
	strcat(kernel_info, "\t");
	if(init_utsname()->release != NULL)
	{
		strcat(version,init_utsname()->release);
		strcat(version," ");
	}

#ifdef UTS_KERNEL_HASH
	strcat(version, UTS_KERNEL_HASH);
#else
	if(init_utsname()->version != NULL)
	{
		strcat(version,init_utsname()->version);
	}
#endif

	tfm = crypto_alloc_shash("md5", 0, CRYPTO_ALG_ASYNC);
	if (IS_ERR(tfm)) {
		pr_err("patch Unable to allocate struct crypto_shash\n");
		goto free_class;
	}
	desc = kmalloc(sizeof(*desc) + crypto_shash_descsize(tfm), GFP_KERNEL);
	if (!desc) {
		pr_err("Unable to allocate struct shash_desc\n");
		goto free_class;
	}
	desc->tfm = tfm;
	desc->flags = 0;
	ret = crypto_shash_init(desc);
	if (ret < 0) {
		pr_err("patch crypto_shash_init() failed\n");
		crypto_free_shash(tfm);
		goto free_class;
	}
	ret = crypto_shash_update(desc, version, strlen(version));
	if (ret < 0) {
		pr_err("patch crypto_shash_update() failed for id\n");
		crypto_free_shash(tfm);
		goto free_class;
	}
	ret = crypto_shash_final(desc, digest);
	if (ret < 0) {
		pr_err("crypto_shash_final() failed for server digest\n");
		crypto_free_shash(tfm);
		goto free_class;
	}
	i=0;
	while(i < MD5_DIGEST_SIZE) {
		sprintf(&version_digest[2*i], "%02x", digest[i]);
		i++;
	}
	pr_debug("\npatch init uts info =%s,digest =%s,len=%d\n",version, version_digest, (int)strlen(version));
	crypto_free_shash(tfm);
	kzfree(desc);
	strcat(kernel_info, version_digest);

	goto init_end;

free_class:
	class_destroy(qihoo_class);
free_device:
	cdev_del(my_dev);
free_region:
	unregister_chrdev_region(md, 1);
free_dev:
	kfree(my_dev);
free_mmap:
	if (!mp) {
#ifdef MMAP_KMALLOC
		kfree(mp);
#else
		vfree(mp);
#endif
	}
init_end:
	return ret;
}
static void __exit map_exit(void)
{
	pr_debug("patch bye kernel...\n");
	qihoo_engine_exit();

	device_destroy(qihoo_class, MKDEV(MAJOR(md), MINOR(md)));
	class_destroy(qihoo_class);

	cdev_del(my_dev);
	unregister_chrdev_region(md, 1);
	kfree(my_dev);
	if (mp) {
#ifdef MMAP_KMALLOC
		kfree(mp);
#else
		vfree(mp);
#endif
	}
}
MODULE_AUTHOR("QIHOO");
MODULE_LICENSE("GPL");
MODULE_DESCRIPTION("qihoo kernel patch ");
__initcall(map_init);
__exitcall(map_exit);
