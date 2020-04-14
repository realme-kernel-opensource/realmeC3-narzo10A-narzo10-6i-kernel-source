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

#ifndef __HYPNUS_UAPI_H__
#define __HYPNUS_UAPI_H__

#define NR_CLUSTERS	4
#define NR_GPUS		1
#define NR_DSPS		2
#define NR_NPUS		1

/* ioctl part */
#define HYPNUS_IOC_MAGIC	0xF4

#define IOCTL_HYPNUS_SUBMIT_DECISION \
	_IOWR(HYPNUS_IOC_MAGIC, 0x1, struct hypnus_decision_prop)
#define IOCTL_HYPNUS_GET_BOOST \
	_IOR(HYPNUS_IOC_MAGIC, 0x2, struct hypnus_boost_prop)
#define IOCTL_HYPNUS_SUBMIT_BOOST \
	_IOW(HYPNUS_IOC_MAGIC, 0x3, struct hypnus_boost_prop)
#define IOCTL_HYPNUS_GET_MIGRATION \
	_IOR(HYPNUS_IOC_MAGIC, 0x4, struct hypnus_migration_prop)
#define IOCTL_HYPNUS_SUBMIT_MIGRATION \
	_IOW(HYPNUS_IOC_MAGIC, 0x5, struct hypnus_migration_prop)
#define	IOCTL_HYPNUS_SUBMIT_CPUNR \
	_IOW(HYPNUS_IOC_MAGIC, 0x20, struct hypnus_cpunr_prop)
#define	IOCTL_HYPNUS_GET_CPULOAD \
	_IOR(HYPNUS_IOC_MAGIC, 0x21, struct hypnus_cpuload_prop)
#define	IOCTL_HYPNUS_GET_RQ \
	_IOR(HYPNUS_IOC_MAGIC, 0x22, struct hypnus_rq_prop)
#define IOCTL_HYPNUS_SUBMIT_CPUFREQ \
	_IOW(HYPNUS_IOC_MAGIC, 0x23, struct hypnus_cpufreq_prop)
#define IOCTL_HYPNUS_SET_LPM_GOV \
	_IOW(HYPNUS_IOC_MAGIC, 0x24, struct hypnus_lpm_prop)
#define	IOCTL_HYPNUS_GET_GPULOAD \
	_IOR(HYPNUS_IOC_MAGIC, 0x32, struct hypnus_gpuload_prop)
#define	IOCTL_HYPNUS_GET_GPUFREQ \
	_IOR(HYPNUS_IOC_MAGIC, 0x31, struct hypnus_gpufreq_prop)
#define	IOCTL_HYPNUS_SUBMIT_GPUFREQ \
	_IOW(HYPNUS_IOC_MAGIC, 0x33, struct hypnus_gpufreq_prop)
#define	IOCTL_HYPNUS_POLL_STATUS \
	_IOR(HYPNUS_IOC_MAGIC, 0x34, struct hypnus_poll_status)
#define IOCTL_HYPNUS_GET_SOC_INFO \
	_IOWR(HYPNUS_IOC_MAGIC, 0x35, struct hypnus_soc_info)
#define IOCTL_HYPNUS_SET_FPSGO \
	_IOW(HYPNUS_IOC_MAGIC, 0x36, unsigned int)
#define IOCTL_HYPNUS_SUBMIT_DSPFREQ \
	_IOW(HYPNUS_IOC_MAGIC, 0x40, struct hypnus_dspfreq_prop)
#define IOCTL_HYPNUS_SUBMIT_NPUFREQ \
	_IOW(HYPNUS_IOC_MAGIC, 0x50, struct hypnus_npufreq_prop)
#define IOCTL_HYPNUS_SUBMIT_DDR \
	_IOW(HYPNUS_IOC_MAGIC, 0x52, struct hypnus_ddr_prop)

struct hypnus_boost_prop {
	u32 sched_boost;
};

struct hypnus_migration_prop {
	int up_migrate[NR_CLUSTERS];
	int down_migrate[NR_CLUSTERS];
};

struct hypnus_ddr_prop {
	int state;
};

struct hypnus_cpuload_prop {
	int cpu_load[NR_CPUS]; /* >0: cpu load, <0: offline*/
};

struct hypnus_rq_prop {
	int avg;
	int big_avg;
	int iowait_avg;
};

struct freq_prop {
	unsigned int min;
	unsigned int max;
	unsigned int cur;
};

struct hypnus_cpufreq_prop {
	struct freq_prop freq_prop[NR_CLUSTERS];
};

struct hypnus_lpm_prop {
    unsigned int lpm_gov;
};

struct hypnus_gpunr_prop {
	unsigned int need_gpus[0];
};

struct hypnus_gpuload_prop {
	unsigned int gpu_load[NR_GPUS];
};

struct hypnus_gpufreq_prop {
	struct freq_prop freq_prop[NR_GPUS];
};

struct hypnus_decision_prop {

};

struct uint_range {
	unsigned int max;
	unsigned int min;
};

struct hypnus_cpunr_prop {
	struct uint_range cpus[NR_CLUSTERS];
};

struct hypnus_cluster_report {
	unsigned int cluster_id;
	unsigned int cpu_mask;
	struct uint_range cpufreq;
};

struct hypnus_gpu_report {
	unsigned int gpu_id;
	struct uint_range gpufreq;
};

struct hypnus_soc_info {
	unsigned int cluster_nr;
	unsigned int gpu_nr;
	unsigned int dsp_nr;
	unsigned int npu_nr;
	struct hypnus_cluster_report cluster[NR_CLUSTERS];
	struct hypnus_gpu_report gpu[NR_GPUS];
};

struct hypnus_dspfreq_prop {
	struct freq_prop freq_prop[NR_DSPS];
};

struct hypnus_npufreq_prop {
	struct freq_prop freq_prop[NR_NPUS];
};

#endif
