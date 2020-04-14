/***********************************************************
** Copyright (C), 2008-2018, OPPO Mobile Comm Corp., Ltd.
** ODM_WT_EDIT
** File: - secboot_fuse.c
** Description: Source file for efuse check.
**           To create proc node and set the efuse value
** Version: 1.0
** Date : 2018/08/17
** Author: Haibin1.zhang@ODM_WT.BSP.Kernel.Security
**
** ------------------------------- Revision History: -------------------------------
**              <author>                <data>     <version >          <desc>
**  Haibin1.zhang       2018/08/17     1.0     build this module
**  Haibin1.zhang       2018/10/10     1.1     use new sbc_enable api

****************************************************************/

#include <linux/fs.h>
#include <linux/init.h>
#include <linux/kernel.h>
#include <linux/proc_fs.h>
#include <linux/seq_file.h>
#include <linux/utsname.h>
#include "sec_boot_lib.h"

#define EFUSE_IDX 27
#define CTRL_SBC_EN (0x1U << 1)

extern u32 get_devinfo_with_index(u32 index);
static u32 check_sbc_enabled(void)
{
        u32 efuse_value;
        u32 sbc_enable;

        efuse_value = get_devinfo_with_index(EFUSE_IDX);
        sbc_enable = (efuse_value & CTRL_SBC_EN) ? 1:0;

        return sbc_enable;
}

static int secboot_proc_show(struct seq_file *m, void *v)
{
        seq_printf(m, "0x%x\n",check_sbc_enabled()? 0x303030 : 0x0);
        return 0;
}
static int secboot_proc_open(struct inode *inode, struct file *file)
{
        return single_open(file, secboot_proc_show, NULL);
}

static const struct file_operations secboot_proc_fops = {
        .open           = secboot_proc_open,
        .read           = seq_read,
        .llseek         = seq_lseek,
        .release        = single_release,
};

static int __init proc_secboot_init(void)
{
        proc_create("secboot_fuse_reg", 0, NULL, &secboot_proc_fops);
        return 0;
}
fs_initcall(proc_secboot_init);

