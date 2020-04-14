/************************************************************
 * Copyright 2017 OPPO Mobile Comm Corp., Ltd.
 * All rights reserved.
 *
 * Description     : record /sdcard/DCIM/Camera and Screenshots delete logs
 *
 *
 ** Version: 1
 ** Date created: 2016/01/06
 ** Author: Jiemin.Zhu@AD.Android.SdcardFs
 ** ------------------------------- Revision History: ---------------------------------------
 **        <author>      <data>           <desc>
 **      Jiemin.Zhu    2017/12/12    create this file
 ************************************************************/

#include <linux/types.h>
#include <linux/errno.h>
#include <linux/time.h>
#include <linux/kernel.h>
#include <linux/security.h>
#include <linux/syscalls.h>
#include <linux/proc_fs.h>
#include <linux/module.h>
#include <linux/moduleparam.h>
#include <linux/interrupt.h>
#include <asm/uaccess.h>
#include "dellog.h"

//extern wait_queue_head_t delbuf_wait;

static u64 dellog_seq;
static u32 dellog_idx;

static u64 delbuf_first_seq;
static u32 delbuf_first_idx;

static u64 delbuf_next_seq;
static u32 delbuf_next_idx;

static u64 dellog_clear_seq;
static u32 dellog_clear_idx;

static u64 dellog_end_seq = -1;

/*
 * Compatibility Issue :
 * dumpstate process of Android M tries to open dellog node with O_NONBLOCK.
 * But on Android L, it tries to open dellog node without O_NONBLOCK.
 * In order to resolve this issue, dellog_open() always works as NONBLOCK mode.
 * If you want runtime debugging, please use dellog_open_pipe().
 */
static int dellog_open(struct inode * inode, struct file * file)
{
	//Open as non-blocking mode for printing once.
	file->f_flags |= O_NONBLOCK;
	return do_dellog(DELLOG_ACTION_OPEN, NULL, 0, DELLOG_FROM_PROC);
}

static int dellog_open_pipe(struct inode * inode, struct file * file)
{
	//Open as blocking mode for runtime debugging
	file->f_flags &= ~(O_NONBLOCK);
	return do_dellog(DELLOG_ACTION_OPEN, NULL, 0, DELLOG_FROM_PROC);
}

static int dellog_release(struct inode * inode, struct file * file)
{
	(void) do_dellog(DELLOG_ACTION_CLOSE, NULL, 0, DELLOG_FROM_PROC);
	return 0;
}

static ssize_t dellog_read(struct file *file, char __user *buf,
			 size_t count, loff_t *ppos)
{
	//Blocking mode for runtime debugging
	if (!(file->f_flags & O_NONBLOCK))
		return do_dellog(DELLOG_ACTION_READ, buf, count, DELLOG_FROM_PROC);

	//Non-blocking mode, print once, consume all the buffers
	return do_dellog(DELLOG_ACTION_READ_ALL, buf, count, DELLOG_FROM_PROC);
}

static ssize_t dellog_write(struct file *file, const char __user *buf, size_t count, loff_t *ppos)
{
	return do_dellog_write(DELLOG_ACTION_WRITE, buf, count, DELLOG_FROM_READER);
}


loff_t dellog_llseek(struct file *file, loff_t offset, int whence)
{
	return (loff_t)do_dellog(DELLOG_ACTION_SIZE_BUFFER, 0, 0, DELLOG_FROM_READER);
}

static const struct file_operations dellog_operations = {
	.read		= dellog_read,
	.write		= dellog_write,
	.open		= dellog_open,
	.release	= dellog_release,
	.llseek		= dellog_llseek,
};

static const struct file_operations dellog_pipe_operations = {
	.read		= dellog_read,
	.open		= dellog_open_pipe,
	.release	= dellog_release,
	.llseek		= dellog_llseek,
};

static const char DEF_DELLOG_VER_STR[] = "0.0.1\n";

static ssize_t dellog_ver_read(struct file *file, char __user *buf,
			 size_t count, loff_t *ppos)
{
	ssize_t ret = 0;
	loff_t off = *ppos;
	ssize_t len = strlen(DEF_DELLOG_VER_STR);

	if (off >= len)
		return 0;

	len -= off;
	if (count < len)
		return -ENOMEM;

	ret = copy_to_user(buf, &DEF_DELLOG_VER_STR[off], len);
	if (ret < 0)
		return ret;

	len -= ret;
	*ppos += len;

	return len;
}

static const struct file_operations dellog_ver_operations = {
	.read		= dellog_ver_read,
};

static int __init dellog_init(void)
{
	proc_create("dellog", S_IRUGO, NULL, &dellog_operations);
	proc_create("dellog_pipe", S_IRUGO, NULL, &dellog_pipe_operations);
	proc_create("dellog_version", S_IRUGO, NULL, &dellog_ver_operations);
	return 0;
}
module_init(dellog_init);

#define CONFIG_DELBUF_SHIFT 19 /*512KB*/

struct delbuf {
	u16 len;
	u16 text_len;
	struct timeval tv;
	char comm[TASK_COMM_LEN];
	char tgid_comm[TASK_COMM_LEN];
};

/*
 * The delbuf_lock protects smsg buffer, indices, counters.
 */
static DEFINE_RAW_SPINLOCK(delbuf_lock);

static DECLARE_WAIT_QUEUE_HEAD(delbuf_wait);
/* the next dellog record to read by /proc/dellog */

#define S_PREFIX_MAX		32
#define DELBUF_LINE_MAX		1024 - S_PREFIX_MAX

/* record buffer */
#define DELBUF_ALIGN __alignof__(struct delbuf)
#define __DELBUF_LEN (1 << CONFIG_DELBUF_SHIFT)

static char __delbuf_buf[__DELBUF_LEN] __aligned(DELBUF_ALIGN);
static char *delbuf_buf = __delbuf_buf;

static u32 delbuf_buf_len = __DELBUF_LEN;

/* cpu currently holding logbuf_lock */
//static volatile unsigned int delbuf_cpu = UINT_MAX;

/* human readable text of the record */
static char *delbuf_text(const struct delbuf *msg)
{
	return (char *)msg + sizeof(struct delbuf);
}

static struct delbuf *delbuf_from_idx(u32 idx)
{
	struct delbuf *msg = (struct delbuf *)(delbuf_buf + idx);

	if (!msg->len)
		return (struct delbuf *)delbuf_buf;
	return msg;
}

static u32 delbuf_next(u32 idx)
{
	struct delbuf *msg = (struct delbuf *)(delbuf_buf + idx);

	if (!msg->len) {
		msg = (struct delbuf *)delbuf_buf;
		return msg->len;
	}
	return idx + msg->len;
}

static void delbuf_store(const char *text, u16 text_len,
		      struct task_struct *owner)
{
	struct delbuf *msg;
	u32 size, pad_len;
	struct task_struct *p = find_task_by_vpid(owner->tgid);

	/* number of '\0' padding bytes to next message */
	size = sizeof(struct delbuf) + text_len;
	pad_len = (-size) & (DELBUF_ALIGN - 1);
	size += pad_len;

	while (delbuf_first_seq < delbuf_next_seq) {
		u32 free;

		if (delbuf_next_idx > delbuf_first_idx)
			free = max(delbuf_buf_len - delbuf_next_idx, delbuf_first_idx);
		else
			free = delbuf_first_idx - delbuf_next_idx;

		if (free > size + sizeof(struct delbuf))
			break;

		/* drop old messages until we have enough space */
		delbuf_first_idx = delbuf_next(delbuf_first_idx);
		delbuf_first_seq++;
	}

	if (delbuf_next_idx + size + sizeof(struct delbuf) >= delbuf_buf_len) {
		memset(delbuf_buf + delbuf_next_idx, 0, sizeof(struct delbuf));
		delbuf_next_idx = 0;
	}

	/* fill message */
	msg = (struct delbuf *)(delbuf_buf + delbuf_next_idx);
	memcpy(delbuf_text(msg), text, text_len);
	msg->text_len = text_len;
	memcpy(msg->comm, owner->comm, TASK_COMM_LEN);
	if (p)
		memcpy(msg->tgid_comm, p->comm, TASK_COMM_LEN);
	else
		msg->tgid_comm[0] = 0;
	msg->len = sizeof(struct delbuf) + text_len + pad_len;
	do_gettimeofday(&msg->tv);

	/* insert message */
	delbuf_next_idx += msg->len;
	delbuf_next_seq++;
	wake_up_interruptible(&delbuf_wait);

}

static size_t dellog_print_pid(const struct delbuf *msg, char *buf)
{

	if (!buf)
		return snprintf(NULL, 0, "[%s|%s] ", msg->comm, msg->tgid_comm);

	return sprintf(buf, "[%lu][%s|%s] ", msg->tv.tv_sec, msg->comm, msg->tgid_comm);
}

static size_t dellog_print_prefix(const struct delbuf *msg, bool delbuf, char *buf)
{
	size_t len = 0;

	len += dellog_print_pid(msg, buf ? buf + len : NULL);
	return len;
}

static size_t dellog_print_text(const struct delbuf *msg,
			     bool delbuf, char *buf, size_t size)
{
	const char *text = delbuf_text(msg);
	size_t text_size = msg->text_len;
	bool prefix = true;
	bool newline = true;
	size_t len = 0;

	do {
		const char *next = memchr(text, '\n', text_size);
		size_t text_len;

		if (next) {
			text_len = next - text;
			next++;
			text_size -= next - text;
		} else {
			text_len = text_size;
		}

		if (buf) {
			if (dellog_print_prefix(msg, delbuf, NULL) + text_len + 1 >= size - len)
				break;

			if (prefix)
				len += dellog_print_prefix(msg, delbuf, buf + len);
			memcpy(buf + len, text, text_len);
			len += text_len;
			if (next || newline)
				buf[len++] = '\n';
		} else {
			/*  buffer size only calculation */
			if (prefix)
				len += dellog_print_prefix(msg, delbuf, NULL);
			len += text_len;
			if (next || newline)
				len++;
		}

		prefix = true;
		text = next;
	} while (text);

	return len;
}

static int dellog_print(char __user *buf, int size)
{
	char *text;
	struct delbuf *msg;
	int len = 0;

	text = kmalloc(DELBUF_LINE_MAX + S_PREFIX_MAX, GFP_KERNEL);
	if (!text)
		return -ENOMEM;

	while (size > 0) {
		size_t n;

		raw_spin_lock_irq(&delbuf_lock);
		if (dellog_seq < delbuf_first_seq) {
			/* messages are gone, move to first one */
			dellog_seq = delbuf_first_seq;
			dellog_idx = delbuf_first_idx;
		}
		if (dellog_seq == delbuf_next_seq) {
			raw_spin_unlock_irq(&delbuf_lock);
			break;
		}

		msg = delbuf_from_idx(dellog_idx);
		n = dellog_print_text(msg, false, text, DELBUF_LINE_MAX + S_PREFIX_MAX);
		if (n <= size) {
			/* message fits into buffer, move forward */
			dellog_idx = delbuf_next(dellog_idx);
			dellog_seq++;
		} else if (!len){
			n = size;
		} else
			n = 0;
		raw_spin_unlock_irq(&delbuf_lock);

		if (!n)
			break;

		if (copy_to_user(buf, text, n)) {
			if (!len)
				len = -EFAULT;
			break;
		}

		len += n;
		size -= n;
		buf += n;
	}

	kfree(text);
	return len;
}

static int dellog_print_all(char __user *buf, int size)
{
	char *text;
	int len = 0;
	u64 seq = delbuf_next_seq;
	u32 idx = delbuf_next_idx;

	text = kmalloc(DELBUF_LINE_MAX + S_PREFIX_MAX, GFP_KERNEL);
	if (!text)
		return -ENOMEM;
	raw_spin_lock_irq(&delbuf_lock);

	if (dellog_end_seq == -1)
		dellog_end_seq = delbuf_next_seq;

	if (buf) {

		if (dellog_clear_seq < delbuf_first_seq) {
			/* messages are gone, move to first available one */
			dellog_clear_seq = delbuf_first_seq;
			dellog_clear_idx = delbuf_first_idx;
		}

		seq = dellog_clear_seq;
		idx = dellog_clear_idx;

		while (seq < dellog_end_seq) {
			struct delbuf *msg = delbuf_from_idx(idx);
			int textlen;

			textlen = dellog_print_text(msg, false, text,
						 DELBUF_LINE_MAX + S_PREFIX_MAX);
			if (textlen < 0) {
				len = textlen;
				break;
			} else if(len + textlen > size) {
				break;
			}
			idx = delbuf_next(idx);
			seq++;

			raw_spin_unlock_irq(&delbuf_lock);
			if (copy_to_user(buf + len, text, textlen))
				len = -EFAULT;
			else
				len += textlen;
			raw_spin_lock_irq(&delbuf_lock);

			if (seq < delbuf_first_seq) {
				/* messages are gone, move to next one */
				seq = delbuf_first_seq;
				idx = delbuf_first_idx;
			}
		}
	}

	dellog_clear_seq = seq;
	dellog_clear_idx = idx;

	raw_spin_unlock_irq(&delbuf_lock);

	kfree(text);
	return len;
}

int do_dellog(int type, char __user *buf, int len, bool from_file)
{
	int error=0;

	switch (type) {
	case DELLOG_ACTION_CLOSE:	/* Close log */
		break;
	case DELLOG_ACTION_OPEN:	/* Open log */

		break;
	case DELLOG_ACTION_READ:	/* cat -f /proc/dellog */
		error = -EINVAL;
		if (!buf || len < 0)
			goto out;
		if (!len)
			goto out;
		if (!access_ok(VERIFY_WRITE, buf, len)) {
			error = -EFAULT;
			goto out;
		}
		error = wait_event_interruptible(delbuf_wait,
			dellog_seq != delbuf_next_seq);
		if (error)
			goto out;
		error = dellog_print(buf, len);
		break;
	case DELLOG_ACTION_READ_ALL: /* cat /proc/dellog */ /* dumpstate */
		error = -EINVAL;
		if (!buf || len < 0)
			goto out;
		error = 0;
		if (!len)
			goto out;
		if (!access_ok(VERIFY_WRITE, buf, len)) {
			error = -EFAULT;
			goto out;
		}

		error = dellog_print_all(buf, len);
		if (error == 0) {
			dellog_clear_seq=delbuf_first_seq;
			dellog_clear_idx=delbuf_first_idx;
			dellog_end_seq = -1;
		}

		break;
	/* Size of the log buffer */
	case DELLOG_ACTION_SIZE_BUFFER:
		error = delbuf_buf_len;
		break;
	default:
		error = -EINVAL;
		break;
	}
out:
	return error;
}


int do_dellog_write(int type, const char __user *buf, int len, bool from_file)
{
	int error=0;
	char *kern_buf=0;
	char *line=0;

	if (!buf || len < 0)
		goto out;
	if (!len)
		goto out;
	if (len > DELBUF_LINE_MAX)
	return -EINVAL;

	kern_buf = kmalloc(len+1, GFP_KERNEL);
	if (kern_buf == NULL)
		return -ENOMEM;

	line = kern_buf;
	if (copy_from_user(line, buf, len)) {
		error = -EFAULT;
		goto out;
	}

	line[len] = '\0';
	error = dellog("%s", line);
	if ((line[len-1] == '\n') && (error == (len-1)))
		error++;
out:
	kfree(kern_buf);
	return error;

}

asmlinkage int vdellog(const char *fmt, va_list args)
{
	static char textbuf[DELBUF_LINE_MAX];
	char *text = textbuf;
	size_t text_len;
	unsigned long flags;
	int printed_len = 0;
	bool stored = false;

	local_irq_save(flags);

	raw_spin_lock(&delbuf_lock);

	text_len = vscnprintf(text, sizeof(textbuf), fmt, args);


	/* mark and strip a trailing newline */
	if (text_len && text[text_len-1] == '\n') {
		text_len--;
	}

	if (!stored)
		delbuf_store(text, text_len,  current);

	printed_len += text_len;

	raw_spin_unlock(&delbuf_lock);
	local_irq_restore(flags);


	return printed_len;
}

EXPORT_SYMBOL(vdellog);

/**
 * dellog - print a storage message
 * @fmt: format string
 */
asmlinkage int dellog(const char *fmt, ...)
{
	va_list args;
	int r;

	va_start(args, fmt);
	r = vdellog(fmt, args);

	va_end(args);

	return r;
}
EXPORT_SYMBOL(dellog);
