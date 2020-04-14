/************************************************************
 * Copyright 2017 OPPO Mobile Comm Corp., Ltd.
 * All rights reserved.
 *
 * Description     : using lower xattr to record uid
 *
 *
 ** Version: 1
 ** Date created: 2018/08/15
 ** Author: Jiemin.Zhu@AD.Android.SdcardFs
 **
 **
 ** ------------------------------- Revision History: ---------------------------------------
 **        <author>      <data>           <desc>
 **      Jiemin.Zhu    2018/08/15    create this file
 ************************************************************/
#include <linux/fs.h>
#include <linux/slab.h>
#include <linux/file.h>
#include <linux/xattr.h>
#include <linux/mount.h>
#include <linux/namei.h>
#include <linux/security.h>
#include <linux/types.h>
#include <linux/errno.h>
#include <linux/kobject.h>
#include <linux/sysfs.h>
#include <linux/init.h>
#include <linux/mutex.h>
#include <asm/uaccess.h>
#include <linux/module.h>
#include <linux/xattr.h>
#include <linux/proc_fs.h>

#include "sdcardfs.h"
#include "xattr.h"
extern ssize_t xattr_getsecurity(struct inode *, const char *, void *, size_t);

int sdcardfs_setxattr(struct dentry *dentry)
{
	int error;
	uid_t uid = current_uid().val;
	char value[MAX_XATTR_VALUE_LEN];

	if (dentry == NULL || d_is_negative(dentry)) {
		return -EOPNOTSUPP;
	}

	snprintf(value, MAX_XATTR_VALUE_LEN, "%u", uid);
	inode_lock(d_inode(dentry));
	error = __vfs_setxattr_noperm(dentry, XATTR_NAME_SDCARDFS_UID, value, strlen(value), 0);
	inode_unlock(d_inode(dentry));

	return error;
}

ssize_t sdcardfs_listxattr(struct dentry *dentry, char *buffer, size_t buffer_size)
{
	struct inode *inode;
	ssize_t total_len;
	int selinux_len, uid_len;

	inode = d_inode(dentry);

	/* only support security.selinux and user.sdcardfs.uid*/
	selinux_len = sizeof(XATTR_NAME_SELINUX);
	uid_len = sizeof(XATTR_NAME_SDCARDFS_UID);
	if (current_uid().val > 1000)
		total_len = selinux_len;
	else
		total_len = selinux_len + uid_len;
	if (buffer && total_len <= buffer_size) {
		memcpy(buffer, XATTR_NAME_SELINUX, selinux_len);
		if (current_uid().val <= 1000)
			memcpy(buffer + selinux_len, XATTR_NAME_SDCARDFS_UID, uid_len);
	}

	return total_len;
}

ssize_t sdcardfs_getxattr_noperm(struct dentry *dentry, const char *name, void *buffer, size_t size)
{
	struct dentry *lower_dentry;
	struct path lower_path;
	struct inode *inode;
	ssize_t error;

	inode = d_inode(dentry);

	sdcardfs_get_lower_path(dentry, &lower_path);
	lower_dentry = lower_path.dentry;

	if (!strcmp(name, XATTR_NAME_SELINUX)) {
		error = xattr_getsecurity(inode, XATTR_SELINUX_SUFFIX, buffer, size);
	} else if (!strcmp(name, XATTR_NAME_SDCARDFS_UID)) {
		//FIXME, kernel after 4.4 do not support xattr ops, use __vfs_getxattr instead
		error = __vfs_getxattr(lower_dentry, d_inode(lower_dentry), name, buffer, size);
	} else {
		error = -EOPNOTSUPP;
		pr_err("sdcardfs xattr ops not supported\n");
	}

	sdcardfs_put_lower_path(dentry, &lower_path);

	return error;
}

ssize_t sdcardfs_getxattr(struct dentry *dentry, const char *name, void *buffer, size_t size)
{
	ssize_t error;

	if (!strcmp(name, XATTR_NAME_SDCARDFS_UID)) {
		if (current_uid().val > 1000) {
			return -EOPNOTSUPP;
		}
	}

	error = sdcardfs_getxattr_noperm(dentry, name, buffer, size);

	return error;
}
