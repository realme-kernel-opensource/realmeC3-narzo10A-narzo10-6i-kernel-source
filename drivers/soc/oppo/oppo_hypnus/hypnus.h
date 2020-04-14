/*
 * Copyright (C) 2015-2018 OPPO, Inc.
 * Author: Chuck Huang <huangcheng-m@oppo.com>
 *
 * This software is licensed under the terms of the GNU General Public
 * License version 2, as published by the Free Software Foundation, and
 * may be copied, distributed, and modified under those terms.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 */

#ifndef __HYPNUS_H__
#define __HYPNUS_H__

#ifdef pr_fmt
#undef pr_fmt
#endif
#define pr_fmt(fmt) "hypnus: " fmt

#include <linux/kobject.h>
#include <linux/cdev.h>
#include <linux/mutex.h>
#include <linux/device.h>
#include "hypnus_uapi.h"

#define HYPNUS_VERSION "001"

enum lpm_state {
	LPM_DEFAULT,
	LPM_USE_GOVERNOR,
	LPM_DISABLE_SLEEP,
};

struct cpu_data {
	unsigned int id;
	unsigned int cluster_id;
	unsigned int id_in_cluster;
	bool online;
	bool not_preferred;
};

struct cluster_data {
	unsigned int id;
	unsigned int num_cpus;
	unsigned int avail_cpus;
	unsigned int online_cpus;
	struct cpumask cluster_mask;
	unsigned int cpufreq_min;
	unsigned int cpufreq_max;
};

struct gpu_data {
	unsigned int id;
	unsigned int gpufreq_min;
	unsigned int gpufreq_max;
};

struct hypnus_data {
	spinlock_t lock;
	unsigned int cluster_nr;
	struct cpu_data cpu_data[NR_CPUS];
	struct cluster_data cluster_data[NR_CLUSTERS];
	unsigned int gpu_nr;
	struct gpu_data gpu_data[NR_GPUS];
	unsigned int dsp_nr;
	unsigned int npu_nr;

	/* chipset operations*/
	struct hypnus_chipset_operations *cops;

	/* cdev */
	struct cdev cdev;
	struct mutex cdev_mutex;
	dev_t dev_no;
	struct class *class;

	/* sysfs */
	struct kobject kobj;

	/* poll */
	struct completion wait_event;

	/* display on/off */
	unsigned int screen_on;
};

struct hypnus_poll_status {
	unsigned int screen_on;
};

struct hypnus_data *hypnus_get_hypdata(void);
int __init display_info_register(struct hypnus_data *hypdata);
void __exit display_info_unregister(struct hypnus_data *hypdata);

long hypnus_ioctl_get_rq(struct hypnus_data *pdata,
			 unsigned int cmd, void *data);
long hypnus_ioctl_get_cpuload(struct hypnus_data *pdata,
			      unsigned int cmd, void *data);
long hypnus_ioctl_submit_cpufreq(struct hypnus_data *pdata,
				 unsigned int cmd, void *data);
long hypnus_ioctl_get_boost(struct hypnus_data *pdata,
			    unsigned int cmd, void *data);
long hypnus_ioctl_submit_boost(struct hypnus_data *pdata,
			       unsigned int cmd, void *data);
long hypnus_ioctl_get_migration(struct hypnus_data *pdata,
				unsigned int cmd, void *data);
long hypnus_ioctl_submit_cpunr(struct hypnus_data *pdata,
			       unsigned int cmd, void *data);
long hypnus_ioctl_submit_migration(struct hypnus_data *pdata,
				   unsigned int cmd, void *data);
long hypnus_ioctl_submit_decision(struct hypnus_data *pdata,
				  unsigned int cmd, void *data);
long hypnus_ioctl_get_gpuload(struct hypnus_data *pdata,
			      unsigned int cmd, void *data);
long hypnus_ioctl_get_gpufreq(struct hypnus_data *pdata,
			      unsigned int cmd, void *data);
long hypnus_ioctl_submit_gpufreq(struct hypnus_data *pdata,
				 unsigned int cmd, void *data);
long hypnus_ioctl_poll_status(struct hypnus_data *pdata,
				unsigned int cmd, void *data);
long hypnus_ioctl_get_soc_info(struct hypnus_data *pdata, unsigned int cmd, void *data);
long hypnus_ioctl_submit_dspfreq(struct hypnus_data *pdata,
				unsigned int cmd, void *data);
long hypnus_ioctl_submit_npufreq(struct hypnus_data *pdata,
				unsigned int cmd, void *data);
long hypnus_ioctl_set_fpsgo(struct hypnus_data *pdata, unsigned int cmd, void *data);
long hypnus_ioctl_submit_ddr(struct hypnus_data *pdata, unsigned int cmd, void *data);
long hypnus_ioctl_set_lpm_gov(struct hypnus_data *pdata,
				 unsigned int cmd, void *data);
#endif
