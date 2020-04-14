/************************************************************
 * Copyright 2017 OPPO Mobile Comm Corp., Ltd.
 * All rights reserved.
 *
 * Description     : record /sdcard/DCIM/Camera and Screenshots, and send uevent
 *
 *
 ** Version: 1
 ** Date created: 2016/01/06
 ** Author: Jiemin.Zhu@AD.Android.SdcardFs
 **
 ** /DCIM/Camera /DCIM/ScreenShots:
 **    base picture
 ** /.ColorOSGalleryRecycler:
 **    only com.coloros.gallery3d,com.coloros.cloud and system app can unlink
 ** /DCIM/MyAlbums:
 **    only com.coloros.gallery3d and system app can unlink
 ** /Tencent/MicroMsg/WeiXin:
 **    only com.tencent.mm and system app can unlink
 ** /Tencent/QQ_Images /Tencent/QQfile_recv:
 **    only com.tencent.mobileqq and system app can unlink
 **
 ** ------------------------------- Revision History: ---------------------------------------
 **        <author>      <data>           <desc>
 **      Jiemin.Zhu    2017/12/12    create this file
 **      Jiemin.Zhu    2018/08/08    modify for adding more protected directorys
 ************************************************************/
#include <linux/types.h>
#include <linux/errno.h>
#include <linux/time.h>
#include <linux/kernel.h>
#include <linux/security.h>
#include <linux/kobject.h>
#include <linux/sysfs.h>
#include <linux/init.h>
#include <linux/mutex.h>
#include <asm/uaccess.h>
#include <linux/module.h>
#include <linux/miscdevice.h>
#include <linux/proc_fs.h>
#include <linux/xattr.h>
#include <linux/slab.h>

#include "sdcardfs.h"
#include "xattr.h"
#include "dellog.h"

static DEFINE_MUTEX(dcim_mutex);
static struct kobject *dcim_kobject = NULL;
#define MAX_UNLINK_EVENT_PARAM	6
#define MAX_RENAME_EVENT_PARAM	7
#define MAX_ALLUNLINK_EVENT_PARAM	6
#define MAX_SKIPD_UID_NUMBER	20
#define MAX_PROTECT_DIR_NUM		10

#define AID_APP_START 10000 /* first app user */

static int skipd_enable = 1;
module_param_named(skipd_enable, skipd_enable, int, S_IRUGO | S_IWUSR);

static struct proc_dir_entry *sdcardfs_procdir;

static struct kmem_cache *sdcardfs_path_cachep;

typedef enum {
	PIC_BASE_SHIFT,
	PIC_RECYCLER_SHIFT,
	PIC_ALBUMS_SHIFT,
	PIC_TENCENT_MM_SHIFT,
	PIC_TENCENT_QQ_SHIFT,
} pic_shift_t;

struct oppo_skiped {
	unsigned int mask;
	int enable;
	int serial;
	uid_t *skipd_uid;
};
static struct oppo_skiped *oppo_skiped_list;

int is_oppo_skiped(unsigned int mask)
{
	int i;
	int index = 0;
	struct oppo_skiped *skiped;

	if (!skipd_enable)
		return 1;

	//system app, do skip
	if (current_uid().val < AID_APP_START)
		return 1;

	if (mask == 0)
		return 1;

	mask >>= 1;
	while (mask != 0) {
		index++;
		mask >>= 1;
	}

	if (index >= MAX_PROTECT_DIR_NUM || !oppo_skiped_list[index].enable)
		return 1;

	skiped = &oppo_skiped_list[index];
	for (i = 0; i < MAX_SKIPD_UID_NUMBER; i++) {
		if (skiped->skipd_uid[i] == current_uid().val) {
			return 1;
		}
		if (skiped->skipd_uid[i] == 0)
			break;
	}

	return 0;
}

static int sdcardfs_rename_uevent(char *old_path, char *new_path, unsigned int mask)
{
	char *denied_param[MAX_RENAME_EVENT_PARAM] = { "RENAME_STAT", NULL };
	int i;

	for(i = 1; i < MAX_RENAME_EVENT_PARAM - 1; i++) {
		denied_param[i] = kmem_cache_alloc(sdcardfs_path_cachep, GFP_KERNEL | __GFP_ZERO);
		if (!denied_param[i]) {
			goto free_memory;
		}
	}

	sprintf(denied_param[1], "MASK=%u", mask);
	sprintf(denied_param[2], "UID=%u", from_kuid(&init_user_ns, current_uid()));
	sprintf(denied_param[3], "OLD_PATH=%s", old_path);
	sprintf(denied_param[4], "NEW_PATH=%s", new_path);
	sprintf(denied_param[5], "PID=%u", current->pid);

	if (dcim_kobject) {
		printk("sdcardfs: send rename uevent %s %s\n", denied_param[2], denied_param[3]);
		kobject_uevent_env(dcim_kobject, KOBJ_CHANGE, denied_param);
	}

free_memory:
    for(i--; i > 0; i--)
        kmem_cache_free(sdcardfs_path_cachep, denied_param[i]);

	return 0;
}

void sdcardfs_rename_record(struct dentry *old_dentry, struct dentry *new_dentry)
{
	struct sdcardfs_inode_info *info = SDCARDFS_I(d_inode(old_dentry));
	char *old_buf, *old_path;
	char *new_buf, *new_path;

	if (!(info->data->oppo_flags & OPPO_PICTURE_BASE))
		return;

	if (current_uid().val < AID_APP_START)
		return;

	old_buf = kmem_cache_alloc(sdcardfs_path_cachep, GFP_KERNEL);
	if (!old_buf) {
		return;
	}
	new_buf = kmem_cache_alloc(sdcardfs_path_cachep, GFP_KERNEL);
	if (!new_buf) {
		kmem_cache_free(sdcardfs_path_cachep, old_buf);
		return;
	}

	old_path = dentry_path_raw(old_dentry, old_buf, PATH_MAX);
	if (IS_ERR(old_path)) {
		kmem_cache_free(sdcardfs_path_cachep, old_buf);
		kmem_cache_free(sdcardfs_path_cachep, new_buf);
		return;
	}
	new_path = dentry_path_raw(new_dentry, new_buf, PATH_MAX);
	if (IS_ERR(new_path)) {
		kmem_cache_free(sdcardfs_path_cachep, old_buf);
		kmem_cache_free(sdcardfs_path_cachep, new_buf);
		return;
	}

	sdcardfs_rename_uevent(old_path, new_path, info->data->oppo_flags);

	DEL_LOG("[%u] rename from %s to %s", (unsigned int) current_uid().val,
			old_path, new_path);

	kmem_cache_free(sdcardfs_path_cachep, old_buf);
	kmem_cache_free(sdcardfs_path_cachep, new_buf);
}

static int sdcardfs_getuid(struct dentry *dentry)
{
	char value[MAX_XATTR_VALUE_LEN] = {0};
	ssize_t ret;
	int uid = -1;

	ret = sdcardfs_getxattr_noperm(dentry, XATTR_NAME_SDCARDFS_UID, value, MAX_XATTR_VALUE_LEN);
	if (ret > 0) {
		if (kstrtoint(value, MAX_XATTR_VALUE_LEN, &uid) < 0) {
			uid = -1;
		}
	}
	pr_info("sdcardfs uid=%d", uid);
	return uid;
}

int sdcardfs_allunlink_uevent(struct dentry *dentry)
{
	char *buf, *path;
	int uid;
	char *denied_param[MAX_ALLUNLINK_EVENT_PARAM] = { "UNLINK_ALLSTAT", NULL };
	int i;

	uid = sdcardfs_getuid(dentry);
	if (uid < 0 || (uid == from_kuid(&init_user_ns, current_uid()))) {
		return 0;
	}
	mutex_lock(&dcim_mutex);

	buf = kmem_cache_alloc(sdcardfs_path_cachep, GFP_KERNEL);
	if (buf == NULL) {
		mutex_unlock(&dcim_mutex);
		return -1;
	}
	path = dentry_path_raw(dentry, buf, PATH_MAX);
	if (IS_ERR(path)) {
		kmem_cache_free(sdcardfs_path_cachep, buf);
		mutex_unlock(&dcim_mutex);
		return -1;
	}

	for(i = 1; i < MAX_ALLUNLINK_EVENT_PARAM - 1; i++) {
		denied_param[i] = kmem_cache_alloc(sdcardfs_path_cachep, GFP_KERNEL | __GFP_ZERO);
		if (!denied_param[i]) {
			goto free_memory;
		}
	}

	sprintf(denied_param[1], "FS_UID=%u", uid);
	sprintf(denied_param[2], "UID=%u", from_kuid(&init_user_ns, current_uid()));
	sprintf(denied_param[3], "PATH=%s", path);
	sprintf(denied_param[4], "PID=%u", current->pid);

	if (dcim_kobject) {
		printk("sdcardfs: send delete all uevent owner %s, by %s, path %s\n",
				denied_param[1], denied_param[2], denied_param[3]);
		kobject_uevent_env(dcim_kobject, KOBJ_CHANGE, denied_param);
	}

free_memory:
    for(i--; i > 0; i--)
        kmem_cache_free(sdcardfs_path_cachep, denied_param[i]);

	kmem_cache_free(sdcardfs_path_cachep, buf);

	mutex_unlock(&dcim_mutex);

	return 0;
}

int sdcardfs_unlink_uevent(struct dentry *dentry, unsigned int mask)
{
	char *buf, *path;
	char *denied_param[MAX_UNLINK_EVENT_PARAM] = { "UNLINK_STAT", NULL };
	int i;

	if (sdcardfs_getuid(dentry) == from_kuid(&init_user_ns, current_uid())) {
		printk("sdcardfs: app[uid %u] delete it's own picture %s, do unlink directly\n",
			from_kuid(&init_user_ns, current_uid()), dentry->d_name.name);
		return -1;
	}

	mutex_lock(&dcim_mutex);

	buf = kmem_cache_alloc(sdcardfs_path_cachep, GFP_KERNEL);
	if (buf == NULL) {
		mutex_unlock(&dcim_mutex);
		return -1;
	}
	path = dentry_path_raw(dentry, buf, PATH_MAX);
	if (IS_ERR(path)) {
		kmem_cache_free(sdcardfs_path_cachep, buf);
		mutex_unlock(&dcim_mutex);
		return -1;
	}

	for(i = 1; i < MAX_UNLINK_EVENT_PARAM - 1; i++) {
		denied_param[i] = kmem_cache_alloc(sdcardfs_path_cachep, GFP_KERNEL | __GFP_ZERO);
		if (!denied_param[i]) {
			goto free_memory;
		}
	}

	sprintf(denied_param[1], "MASK=%u", mask);
	sprintf(denied_param[2], "UID=%u", from_kuid(&init_user_ns, current_uid()));
	sprintf(denied_param[3], "PATH=%s", path);
	sprintf(denied_param[4], "PID=%u", current->pid);

	if (dcim_kobject) {
		printk("sdcardfs: send delete uevent %s %s\n", denied_param[2], denied_param[3]);
		kobject_uevent_env(dcim_kobject, KOBJ_CHANGE, denied_param);
	}

free_memory:
    for(i--; i > 0; i--)
        kmem_cache_free(sdcardfs_path_cachep, denied_param[i]);

	kmem_cache_free(sdcardfs_path_cachep, buf);

	mutex_unlock(&dcim_mutex);

	return 0;
}

static void get_mask_path(unsigned int mask, char *path)
{
	int index = 0;

	mask >>= 1;
	while (mask != 0) {
		index++;
		mask >>= 1;
	}

	switch (index) {
	case PIC_BASE_SHIFT:
		sprintf(path, "/DCIM/Camera /DCIM/Screenshots");
		break;
	case PIC_RECYCLER_SHIFT:
		sprintf(path, "/.ColorOSGalleryRecycler");
		break;
	case PIC_ALBUMS_SHIFT:
		sprintf(path, "/DCIM/MyAlbums");
		break;
	case PIC_TENCENT_MM_SHIFT:
		sprintf(path, "/Tencent/MicroMsg/WeiXin");
		break;
	case PIC_TENCENT_QQ_SHIFT:
		sprintf(path, "/Tencent/QQ_Images /Tencent/QQfile_recv");
		break;
	default:
		path[0] = '\0';
	}
}

static int proc_sdcardfs_skip_show(struct seq_file *m, void *v)
{
	int i, j;
	char *path;

	path = kzalloc(PATH_MAX, GFP_KERNEL);
	if (!path)
		return 0;

	for (i = 0; i < MAX_PROTECT_DIR_NUM; i++) {
		seq_printf(m, "mask: %u \n", oppo_skiped_list[i].mask);
		seq_printf(m, "enable: %d \n", oppo_skiped_list[i].enable);
		get_mask_path(oppo_skiped_list[i].mask, path);
		seq_printf(m, "path: %s \n", path);
		seq_printf(m, "skiped uid: ");
		for (j = 0; j < oppo_skiped_list[i].serial; j++) {
			seq_printf(m, "%u ", oppo_skiped_list[i].skipd_uid[j]);
		}
		seq_printf(m, " \n\n");
	}

	kfree(path);

	return 0;
}

static int proc_sdcardfs_skip_open(struct inode *inode, struct file *file)
{
	return single_open(file, proc_sdcardfs_skip_show, NULL);
}

#define MAX_OPPO_SKIPED_INPUT_LEN	255
static ssize_t proc_sdcardfs_skip_write(struct file *file, const char __user *buffer,
				   size_t count, loff_t *offp)
{
	int argc, serial, i, index = 0;
	uid_t uid;
	unsigned int mask;
	char input_buf[MAX_OPPO_SKIPED_INPUT_LEN];
	struct oppo_skiped *skiped;
	bool found = false;

	mutex_lock(&dcim_mutex);

	if (count >= MAX_OPPO_SKIPED_INPUT_LEN) {
		mutex_unlock(&dcim_mutex);
		return -EINVAL;
	}
	if (copy_from_user(input_buf, buffer, count)) {
		mutex_unlock(&dcim_mutex);
		return -EFAULT;
	}
	input_buf[count] = '\0';

	argc = sscanf(input_buf, "%u %u", &mask, &uid);
	if (argc != 2) {
		printk("sdcardfs: input error\n");
		mutex_unlock(&dcim_mutex);
		return -EINVAL;
	}
	if (mask <= 0 || (mask & (mask - 1)) != 0) {
		printk("sdcardfs: input mask must be 2^n, your input is %u\n", mask);
		mutex_unlock(&dcim_mutex);
		return -EINVAL;
	}
	printk("sdcardfs: input mask: 0x%x, uid: %u\n", mask, uid);
	if (uid < AID_APP_START && uid != 0 && uid != 1) {
		mutex_unlock(&dcim_mutex);
		return -EINVAL;
	}

	mask >>= 1;
	while (mask != 0) {
		index++;
		mask >>= 1;
	}

	if (index >= MAX_PROTECT_DIR_NUM) {
		printk("sdcardfs: input mask should be 2^n, and n is %d must < %d\n",
			index, MAX_PROTECT_DIR_NUM);
		mutex_unlock(&dcim_mutex);
		return -EINVAL;
	}
	skiped = &oppo_skiped_list[index];
	if (uid == 0 || uid == 1) {
		skiped->enable = uid;
	} else {
		serial = skiped->serial;
		for (i = 0; i < MAX_SKIPD_UID_NUMBER; i++) {
			if (skiped->skipd_uid[i] == 0)
				break;
			if (skiped->skipd_uid[i] == uid) {
				found = true;
				break;
			}
		}
		if (!found) {
			skiped->skipd_uid[serial] = uid;
			skiped->serial++;
			if (skiped->serial > MAX_SKIPD_UID_NUMBER)
				skiped->serial = 0;
		}
	}

	mutex_unlock(&dcim_mutex);

	return count;
}

static const struct file_operations proc_sdcardfs_skip_fops = {
	.open		= proc_sdcardfs_skip_open,
	.read		= seq_read,
	.write		= proc_sdcardfs_skip_write,
	.llseek		= seq_lseek,
	.release	= single_release,
};

static int __init dcim_event_init(void)
{
	struct proc_dir_entry *skipd_entry;
	int i;
	struct oppo_skiped *skiped;

	sdcardfs_path_cachep = kmem_cache_create("sdcardfs_path", PATH_MAX, 0, 0, NULL);
	if (!sdcardfs_path_cachep) {
		printk("sdcardfs: sdcardfs_path cache create failed\n");
		return -ENOMEM;
	}

	dcim_kobject = kset_find_obj(module_kset, "sdcardfs");
	if (dcim_kobject == NULL) {
		printk("sdcardfs: sdcardfs uevent kobject is null");
		kmem_cache_destroy(sdcardfs_path_cachep);
		return -1;
	}
	oppo_skiped_list = kzalloc(sizeof(struct oppo_skiped) * MAX_PROTECT_DIR_NUM, GFP_KERNEL);
	if (!oppo_skiped_list) {
		printk("sdcardfs: sdcardfs skiped list malloc failed");
		kmem_cache_destroy(sdcardfs_path_cachep);
		kobject_put(dcim_kobject);
		dcim_kobject = NULL;
		return -ENOMEM;
	}
	for (i = 0; i < MAX_PROTECT_DIR_NUM; i++) {
		skiped = &oppo_skiped_list[i];
		skiped->mask = 1 << i;
		skiped->enable = 0;
		skiped->skipd_uid = kzalloc(sizeof(uid_t) * MAX_SKIPD_UID_NUMBER, GFP_KERNEL);
	}
	sdcardfs_procdir = proc_mkdir("fs/sdcardfs", NULL);
	skipd_entry = proc_create_data("skipd_delete", 664, sdcardfs_procdir,
				&proc_sdcardfs_skip_fops, NULL);

	return 0;
}

static void __exit dcim_event_exit(void)
{
}

module_init(dcim_event_init);
module_exit(dcim_event_exit);

MODULE_AUTHOR("jiemin.zhu@oppo.com");
MODULE_LICENSE("GPL");
