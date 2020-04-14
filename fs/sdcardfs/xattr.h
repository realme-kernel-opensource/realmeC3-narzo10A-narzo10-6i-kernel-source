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
#ifndef _SDCARDFS_XATTR_H
#define _SDCARDFS_XATTR_H

#define MAX_XATTR_VALUE_LEN 10
#define XATTR_SDCARDFS_UID_SUFFIX	"sdcardfs.uid"
#define XATTR_NAME_SDCARDFS_UID	XATTR_USER_PREFIX XATTR_SDCARDFS_UID_SUFFIX
#define XATTR_NAME_SDCARDFS_UID_LEN	(sizeof(XATTR_NAME_SDCARDFS_UID) - 1)

ssize_t sdcardfs_getxattr(struct dentry *dentry, const char *name, void *buffer, size_t size);
ssize_t sdcardfs_getxattr_noperm(struct dentry *dentry, const char *name, void *buffer, size_t size);
ssize_t sdcardfs_listxattr(struct dentry *dentry, char *buffer, size_t buffer_size);
//int sdcardfs_setxattr(struct dentry *dentry, const char *name, const void *value, size_t size, int flags);
int sdcardfs_setxattr(struct dentry *dentry);

#endif /* _SDCARDFS_XATTR_H */
