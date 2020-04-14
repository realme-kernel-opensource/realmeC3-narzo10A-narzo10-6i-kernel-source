/***************************************************************
** Copyright (C),  2018,  OPPO Mobile Comm Corp.,  Ltd
** VENDOR_EDIT
** File : oppo_mm_kevent_fb.h
** Description : MM kevent fb data
** Version : 1.0
** Date : 2018/12/03
** Author : Ling.Guo@PSW.MM.Display.Stability
**
** ------------------------------- Revision History: -----------
**  <author>        <data>        <version >        <desc>
**   Guo.Ling          2018/12/03        1.0           Build this moudle
******************************************************************/
#include <linux/err.h>
#include <linux/string.h>
#include <linux/sysfs.h>
#include <linux/oppo_kevent.h>
#include <linux/oppo_mm_kevent_fb.h>
#include <linux/delay.h>
#include <linux/mutex.h>
static DEFINE_MUTEX(mm_kevent_lock);



extern int kevent_send_to_user(struct kernel_packet_info *userinfo);

int upload_mm_kevent_fb_data(enum OPPO_MM_DIRVER_FB_EVENT_MODULE module, unsigned char *payload) {
	struct kernel_packet_info *user_msg_info;
	char log_tag[32] = "psw_multimedia";
	char event_id_display[20] = "20181203";
	char event_id_audio[20] = "20181205";
	void* buffer = NULL;
	int len, size;

	mutex_lock(&mm_kevent_lock);

	len = strlen(payload);

	size = sizeof(struct kernel_packet_info) + len + 1;
	printk(KERN_INFO "kevent_send_to_user:size=%d\n", size);

	buffer = kmalloc(size, GFP_ATOMIC);
	memset(buffer, 0, size);
	user_msg_info = (struct kernel_packet_info *)buffer;
	user_msg_info->type = 1;

	memcpy(user_msg_info->log_tag, log_tag, strlen(log_tag) + 1);
	if (OPPO_MM_DIRVER_FB_EVENT_MODULE_DISPLAY == module) {
		memcpy(user_msg_info->event_id, event_id_display, strlen(event_id_display) + 1);
	} else {
		memcpy(user_msg_info->event_id, event_id_audio, strlen(event_id_audio) + 1);
	}

	user_msg_info->payload_length = len + 1;
	memcpy(user_msg_info->payload, payload, len + 1);

	kevent_send_to_user(user_msg_info);
	msleep(20);
	kfree(buffer);
	mutex_unlock(&mm_kevent_lock);
	return 0;
}
