/************************************************************
 * Copyright 2017 OPPO Mobile Comm Corp., Ltd.
 * All rights reserved.
 *
 * Description     : disable selinux denied log in MP version
 *
 *
 ** Version: 1
 ** Date created: 2016/01/06
 ** Author: Jiemin.Zhu@AD.Android.SELinux
 ** ------------------------------- Revision History: ---------------------------------------
 **        <author>      <data>           <desc>
 **      Jiemin.Zhu    2017/12/12    create this file
 ************************************************************/
#include <linux/types.h>
#include <linux/errno.h>
#include <linux/kernel.h>
#include <linux/kobject.h>
#include <linux/init.h>
#include <linux/uaccess.h>
//#include <asm/uaccess.h>
#include <linux/module.h>
#include <linux/proc_fs.h>
#include <linux/seq_file.h>
#include <linux/slab.h>

#ifdef CONFIG_OPPO_REALEASE_BUILD
static int enable_audit = 0;
#else
static int enable_audit = 1;
#endif

static struct proc_dir_entry *denied_procdir;

int is_avc_audit_enable(void)
{
	return enable_audit;
}

static int proc_selinux_denied_show(struct seq_file *m, void *v)
{
	seq_printf(m, "selinux denied log enable is: %d\n", enable_audit);

	return 0;
}

static int proc_selinux_denied_open(struct inode *inode, struct file *file)
{
	return single_open(file, proc_selinux_denied_show, NULL);
}

static ssize_t proc_selinux_denied_write(struct file *file, const char __user *buffer,
				   size_t count, loff_t *offp)
{
	int enable, ret;
	char *tmp;


	tmp = kzalloc((count + 1), GFP_KERNEL);
	if (!tmp) {
		return 0;
	}

	if(copy_from_user(tmp, buffer, count)) {
		count = 0;
		goto out;
	}

	ret = kstrtoint(tmp, 10, &enable);
	if (ret < 0) {
		count = 0;
		goto out;
	}

	enable_audit = enable;

out:
	kfree(tmp);

	return count;
}

static const struct file_operations proc_selinux_denied_fops = {
	.open		= proc_selinux_denied_open,
	.read		= seq_read,
	.write		= proc_selinux_denied_write,
	.llseek		= seq_lseek,
	.release	= seq_release_private,
};

int init_denied_proc(void)
{
	struct proc_dir_entry *denied_entry;

	denied_procdir = proc_mkdir("fs/selinux", NULL);
	denied_entry = proc_create_data("enable_audit", 664, denied_procdir,
				&proc_selinux_denied_fops, NULL);

	return 0;
}
