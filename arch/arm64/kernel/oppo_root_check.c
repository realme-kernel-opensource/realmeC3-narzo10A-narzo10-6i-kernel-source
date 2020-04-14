/*
 * block_root_check.c - for root action upload to user layer and reboot phone
 *  author by wangzhenhua,Plf.Framework
 */
#include <linux/delay.h>
#include <linux/kernel.h>
#include <linux/reboot.h>
#include <linux/string.h>
#include <linux/types.h>
#include <linux/version.h>
#include <linux/vmalloc.h>
#include <linux/selinux.h>

#include <linux/oppo_kevent.h>
#ifdef CONFIG_OPPO_KEVENT_UPLOAD

void oppo_root_check_succ(uid_t uid, uid_t euid, uid_t fsuid, uid_t callnum)
{
	struct kernel_packet_info *dcs_event;
	char dcs_stack[sizeof(struct kernel_packet_info) + 256];
	const char* dcs_event_tag = "kernel_event";
	const char* dcs_event_id = "root_check";
	char* dcs_event_payload = NULL;

	dcs_event = (struct kernel_packet_info*)dcs_stack;
	dcs_event->type = 0;
	strncpy(dcs_event->log_tag, dcs_event_tag, sizeof(dcs_event->log_tag));
	strncpy(dcs_event->event_id, dcs_event_id, sizeof(dcs_event->event_id));
	dcs_event_payload = kmalloc(256, GFP_ATOMIC);
	memset(dcs_event_payload, 0, 256);
	dcs_event->payload_length = sprintf(dcs_event_payload,
	    "%d$$old_euid@@%d$$old_fsuid@@%d$$sys_call_number@@%d$$addr_limit@@%lx$$curr_uid@@%d$$curr_euid@@%d$$curr_fsuid@@%d$$enforce@@%d\n",
	    uid,euid,fsuid,callnum,
	    get_fs(),current_uid().val,current_euid().val,current_fsuid().val,selinux_is_enabled());
	printk(KERN_INFO "oppo_root_check_succ,payload:%s\n",dcs_event_payload);
	memcpy(dcs_event->payload, dcs_event_payload, strlen(dcs_event_payload));

	kevent_send_to_user(dcs_event);

	kfree(dcs_event_payload);

	msleep(5000);

	return;
}

#endif

#ifdef CONFIG_OPPO_ROOT_CHECK
void oppo_root_reboot(void)
{
	int i=0;
	printk(KERN_INFO "oppo_root_reboot,Rebooting in the phone..");
	for (i = 0; i < 10; i++) {
		panic("ROOT for panic");
		msleep(200);
	}
}
#endif /* CONFIG_OPPO_ROOT_CHECK */

#ifdef CONFIG_OPPO_ROOT_CHECK
static int boot_state = -1;
int get_boot_state(void)
{
    if (strstr(saved_command_line, "androidboot.verifiedbootstate=orange")) {
        boot_state = 0;
    } else {
        boot_state = 1;
    }
    return 0;
}

bool is_unlocked(void)
{
    if (boot_state == -1) {
        get_boot_state();
        printk(KERN_INFO "oppo_root_check,is_unlocked:%d\n",(boot_state == 0));
    }
    return (boot_state == 0);
}
#endif /* CONFIG_OPPO_ROOT_CHECK */


