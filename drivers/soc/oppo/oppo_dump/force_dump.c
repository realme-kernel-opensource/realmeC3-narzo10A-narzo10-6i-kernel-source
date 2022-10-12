/*
 * Copyright (C), 2019, OPPO Mobile Comm Corp., Ltd.
 * VENDOR_EDIT
 * File: - oppo_dump/oppo_dump.c
 * Description: Util about dump in the period of debugging.
 * Version: 1.0
 * Date: 2019/11/01
 * Author: Bin.Xu@BSP.Kernel.Stability
 *
 *----------------------Revision History: ---------------------------
 *   <author>        <date>         <version>         <desc>
 *    Bin.Xu       2019/11/01        1.0              created
 *-------------------------------------------------------------------
 */

#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/proc_fs.h>
#include <linux/fs.h>
#include <linux/bug.h>
#include <linux/clk.h>
#include <linux/io.h>
#include <linux/of.h>
#include <uapi/linux/input-event-codes.h>

#include <linux/oppo_dump.h>

u16 DUMPPASSWORD;
int door_open;
static unsigned int vol_key_password = 0;
static unsigned long start_timer_last = 0;

static __attribute__((unused)) int atoi(const char *str)
{
	int num = 0;
	if (strlen(str) < 1)
	{
		return -1;
	}

	while (*str <= '9' && *str >= '0')
	{
		num = num * 10 + *str - '0';
		str++;
	}
	return num;
}

void __attribute__((unused)) oppo_key_process(int key, int val)
{

	unsigned long start_timer_current;
	if (val) {
		if (key == KEY_POWER) {
			door_open = 0;
			vol_key_password = 0;
			start_timer_last = 0;
			return;
		}

		if (key == KEY_VOLUMEUP)
			vol_key_password = (vol_key_password << 1)|0x01;

		if(key == KEY_VOLUMEDOWN)
			vol_key_password = (vol_key_password << 1)&~0x01;

		if (door_open) {
			door_open = 0;
			vol_key_password = 0;
			pr_info("vol_key_password dump_door_close \n");
		}

		start_timer_current = jiffies;

		if(start_timer_last != 0){
			if (time_after(start_timer_current,start_timer_last + msecs_to_jiffies(1000))) {
				vol_key_password = 0;
			}

			if (vol_key_password == DUMPPASSWORD) {
					dump_process(key, val);
			}
		}
		start_timer_last = start_timer_current;
	}

}

void __attribute__((unused)) OPPO_DUMP(const char f[24])
{
	if (door_open) {
		pr_err("%s: enter dump\n", f);
		BUG_ON(1);
	}
}

void __attribute__((unused)) dump_process(int key, int val)
{
	if (door_open == 0) {
		pr_info("vol_key_password dump_door_open \n");
		door_open = 1;
	}
}


void __attribute__((unused)) dump_post_process(int key, int val)
{
	if (door_open) {
		// TO DO if needed
	}
}

void __attribute__((unused)) oppo_key_event(int type, int key, int val)
{
	if (type != EV_KEY) return;

	if (key != KEY_POWER && key != KEY_VOLUMEUP && key != KEY_VOLUMEDOWN) return;

	oppo_key_process(key, val);

	dump_post_process(key, val);
}


void __attribute__((unused)) get_fdump_passwd(const char *val)
{
	door_open = 0;
	DUMPPASSWORD = (u16)atoi(val) ^ 0x2019;
	pr_info("get_fdump_passwd fd_passwd: %u\n", DUMPPASSWORD);
}

