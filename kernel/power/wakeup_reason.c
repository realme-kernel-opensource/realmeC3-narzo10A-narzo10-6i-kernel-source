/*
 * kernel/power/wakeup_reason.c
 *
 * Logs the reasons which caused the kernel to resume from
 * the suspend mode.
 *
 * Copyright (C) 2014 Google, Inc.
 * This software is licensed under the terms of the GNU General Public
 * License version 2, as published by the Free Software Foundation, and
 * may be copied, distributed, and modified under those terms.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */

#include <linux/wakeup_reason.h>
#include <linux/kernel.h>
#include <linux/irq.h>
#include <linux/interrupt.h>
#include <linux/io.h>
#include <linux/kobject.h>
#include <linux/sysfs.h>
#include <linux/init.h>
#include <linux/spinlock.h>
#include <linux/notifier.h>
#include <linux/suspend.h>

//Yongyao.Song@PSW.NW.PWR.1053636, 2017/08/01, add for modem wake up source
#define MODEM_WAKEUP_SRC_NUM 10
extern int data_wakeup_index;
extern int modem_wakeup_src_count[MODEM_WAKEUP_SRC_NUM];
extern char modem_wakeup_src_string[MODEM_WAKEUP_SRC_NUM][20];
extern void modem_clear_wakeupsrc_count(void);
//Yongyao.Song@PSW.NW.PWR add end

#define MAX_WAKEUP_REASON_IRQS 32
static int irq_list[MAX_WAKEUP_REASON_IRQS];
static int irqcount;
static bool suspend_abort;
static char abort_reason[MAX_SUSPEND_ABORT_LEN];
static struct kobject *wakeup_reason;
static DEFINE_SPINLOCK(resume_reason_lock);

static ktime_t last_monotime; /* monotonic time before last suspend */
static ktime_t curr_monotime; /* monotonic time after last suspend */
static ktime_t last_stime; /* monotonic boottime offset before last suspend */
static ktime_t curr_stime; /* monotonic boottime offset after last suspend */
#ifdef VENDOR_EDIT
//Wenxian.zhen@Prd.BaseDrv, 2016/07/19, add for analysis power consumption
void wakeup_src_clean(void);
#endif /* VENDOR_EDIT */

//Yongyao.Song@PSW.NW.PWR.1053636, 2017/08/01, add for modem wake up source
static ssize_t modem_resume_reason_stastics_show(struct kobject *kobj, struct kobj_attribute *attr,
        char *buf)
{
        int max_wakeup_src_count = 0;
        int max_wakeup_src_index = 0;
        int i, total = 0;
        int temp_d = 0;

        for(i = 0; i < MODEM_WAKEUP_SRC_NUM; i++)
        {
            total += modem_wakeup_src_count[i];
            printk(KERN_WARNING "%s wakeup %d times, total %d times\n",
                    modem_wakeup_src_string[i],modem_wakeup_src_count[i],total);
            if (i == data_wakeup_index)
            {
                temp_d = modem_wakeup_src_count[i] + (modem_wakeup_src_count[i]>>1);
                printk(KERN_WARNING "%s wakeup real %d times, count %d times\n",
                             modem_wakeup_src_string[i],modem_wakeup_src_count[i],temp_d);
                if(temp_d > max_wakeup_src_count){
                    max_wakeup_src_index = i;
                    max_wakeup_src_count = temp_d;
                }
            }
            else if (modem_wakeup_src_count[i] > max_wakeup_src_count)
            {
                max_wakeup_src_index = i;
                max_wakeup_src_count = modem_wakeup_src_count[i];
            }
        }
        return sprintf(buf, "%s:%d:%d\n", modem_wakeup_src_string[max_wakeup_src_index], max_wakeup_src_count, total);
}

static struct kobj_attribute modem_resume_reason_stastics = __ATTR_RO(modem_resume_reason_stastics);
//Yongyao.Song@PSW.NW.PWR add end
static ssize_t last_resume_reason_show(struct kobject *kobj, struct kobj_attribute *attr,
		char *buf)
{
	int irq_no, buf_offset = 0;
	struct irq_desc *desc;
	spin_lock(&resume_reason_lock);
	if (suspend_abort) {
		buf_offset = sprintf(buf, "Abort: %s", abort_reason);
	} else {
		for (irq_no = 0; irq_no < irqcount; irq_no++) {
			desc = irq_to_desc(irq_list[irq_no]);
			if (desc && desc->action && desc->action->name)
				buf_offset += sprintf(buf + buf_offset, "%d %s\n",
						irq_list[irq_no], desc->action->name);
			else
				buf_offset += sprintf(buf + buf_offset, "%d\n",
						irq_list[irq_no]);
		}
	}
	spin_unlock(&resume_reason_lock);
	return buf_offset;
}

static ssize_t last_suspend_time_show(struct kobject *kobj,
			struct kobj_attribute *attr, char *buf)
{
	struct timespec sleep_time;
	struct timespec total_time;
	struct timespec suspend_resume_time;

	/*
	 * total_time is calculated from monotonic bootoffsets because
	 * unlike CLOCK_MONOTONIC it include the time spent in suspend state.
	 */
	total_time = ktime_to_timespec(ktime_sub(curr_stime, last_stime));

	/*
	 * suspend_resume_time is calculated as monotonic (CLOCK_MONOTONIC)
	 * time interval before entering suspend and post suspend.
	 */
	suspend_resume_time = ktime_to_timespec(ktime_sub(curr_monotime, last_monotime));

	/* sleep_time = total_time - suspend_resume_time */
	sleep_time = timespec_sub(total_time, suspend_resume_time);

	/* Export suspend_resume_time and sleep_time in pair here. */
	return sprintf(buf, "%lu.%09lu %lu.%09lu\n",
				suspend_resume_time.tv_sec, suspend_resume_time.tv_nsec,
				sleep_time.tv_sec, sleep_time.tv_nsec);
}

static struct kobj_attribute resume_reason = __ATTR_RO(last_resume_reason);
static struct kobj_attribute suspend_time = __ATTR_RO(last_suspend_time);

static struct attribute *attrs[] = {
	&resume_reason.attr,
    //Yongyao.Song@PSW.NW.PWR.1053636, 2017/08/01, add for modem wake up source
    &modem_resume_reason_stastics.attr,
    //Yongyao.Song@PSW.NW.PWR add end
	&suspend_time.attr,
	NULL,
};
static struct attribute_group attr_group = {
	.attrs = attrs,
};

/*
 * logs all the wake up reasons to the kernel
 * stores the irqs to expose them to the userspace via sysfs
 */
void log_wakeup_reason(int irq)
{
	struct irq_desc *desc;
	desc = irq_to_desc(irq);
	if (desc && desc->action && desc->action->name)
		printk(KERN_INFO "Resume caused by IRQ %d, %s\n", irq,
				desc->action->name);
	else
		printk(KERN_INFO "Resume caused by IRQ %d\n", irq);

	spin_lock(&resume_reason_lock);
	if (irqcount == MAX_WAKEUP_REASON_IRQS) {
		spin_unlock(&resume_reason_lock);
		printk(KERN_WARNING "Resume caused by more than %d IRQs\n",
				MAX_WAKEUP_REASON_IRQS);
		return;
	}

	irq_list[irqcount++] = irq;
	spin_unlock(&resume_reason_lock);
}

int check_wakeup_reason(int irq)
{
	int irq_no;
	int ret = false;

	spin_lock(&resume_reason_lock);
	for (irq_no = 0; irq_no < irqcount; irq_no++)
		if (irq_list[irq_no] == irq) {
			ret = true;
			break;
	}
	spin_unlock(&resume_reason_lock);
	return ret;
}

void log_suspend_abort_reason(const char *fmt, ...)
{
	va_list args;

	spin_lock(&resume_reason_lock);

	//Suspend abort reason has already been logged.
	if (suspend_abort) {
		spin_unlock(&resume_reason_lock);
		return;
	}

	suspend_abort = true;
	va_start(args, fmt);
	vsnprintf(abort_reason, MAX_SUSPEND_ABORT_LEN, fmt, args);
	va_end(args);
	spin_unlock(&resume_reason_lock);
}

/* Detects a suspend and clears all the previous wake up reasons*/
static int wakeup_reason_pm_event(struct notifier_block *notifier,
		unsigned long pm_event, void *unused)
{
	switch (pm_event) {
	case PM_SUSPEND_PREPARE:
		spin_lock(&resume_reason_lock);
		irqcount = 0;
		suspend_abort = false;
		spin_unlock(&resume_reason_lock);
		/* monotonic time since boot */
		last_monotime = ktime_get();
		/* monotonic time since boot including the time spent in suspend */
		last_stime = ktime_get_boottime();
		break;
	case PM_POST_SUSPEND:
		/* monotonic time since boot */
		curr_monotime = ktime_get();
		/* monotonic time since boot including the time spent in suspend */
		curr_stime = ktime_get_boottime();
		break;
	default:
		break;
	}
	return NOTIFY_DONE;
}

static struct notifier_block wakeup_reason_pm_notifier_block = {
	.notifier_call = wakeup_reason_pm_event,
};

#ifdef VENDOR_EDIT
/* ChaoYing.Chen@BSP.Power.Basic.1056413, 2017/12/11, Add for print wakeup source */
void wakeup_src_clean(void)
{
    //Yongyao.Song@PSW.NW.PWR.1053636, 2017/08/01, add for modem wake up source
    modem_clear_wakeupsrc_count();
    //Yongyao.Song@PSW.NW.PWR add end
}
EXPORT_SYMBOL(wakeup_src_clean);
#endif /* VENDOR_EDIT */
/* Initializes the sysfs parameter
 * registers the pm_event notifier
 */
int __init wakeup_reason_init(void)
{
	int retval;

	retval = register_pm_notifier(&wakeup_reason_pm_notifier_block);
	if (retval)
		printk(KERN_WARNING "[%s] failed to register PM notifier %d\n",
				__func__, retval);

	wakeup_reason = kobject_create_and_add("wakeup_reasons", kernel_kobj);
	if (!wakeup_reason) {
		printk(KERN_WARNING "[%s] failed to create a sysfs kobject\n",
				__func__);
		return 1;
	}
	retval = sysfs_create_group(wakeup_reason, &attr_group);
	if (retval) {
		kobject_put(wakeup_reason);
		printk(KERN_WARNING "[%s] failed to create a sysfs group %d\n",
				__func__, retval);
	}
	return 0;
}

late_initcall(wakeup_reason_init);
