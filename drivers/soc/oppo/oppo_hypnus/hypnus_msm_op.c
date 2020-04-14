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

#include <linux/version.h>
#include <linux/mutex.h>
#include <linux/cpufreq.h>
#include <linux/sched/stat.h>
#include <linux/sched/core_ctl.h>
#include "kgsl.h"
#include "kgsl_device.h"
#include "kgsl_pwrctrl.h"
#include "hypnus_op.h"

#define PM_QOS_DISABLE_SLEEP_VALUE	(43)

static struct pm_qos_request hypnus_lpm_qos_request;

static int msm_get_running_avg(int *avg, int *big_avg, int *iowait_avg)
{
/* TODO qinyonghui*/
#if 0
	int max_nr, big_max_nr;

	sched_get_nr_running_avg(avg, iowait_avg, big_avg,
				&max_nr, &big_max_nr);
#endif

	return 0;
}

static int msm_get_cpu_load(u32 cpu)
{
#if LINUX_VERSION_CODE >= KERNEL_VERSION(4,9,0)
	return sched_get_cpu_util(cpu);
#else
	return 0;
#endif
}

static int msm_get_boost(void)
{
	return sched_boost();
}

static int msm_set_boost(u32 boost)
{
	return sched_set_boost(boost);
}

static int msm_set_cpu_freq_limit(u32 c_index, u32 min, u32 max)
{
	struct cpufreq_policy *policy;
	struct hypnus_data *hypdata = hypnus_get_hypdata();
	int cpu;

	if (!hypdata)
		return -EINVAL;

	cpu = cpumask_first(&hypdata->cluster_data[c_index].cluster_mask);
	if (!cpu_online(cpu))
		return 0;

	policy = cpufreq_cpu_get(cpu);

	if (!policy)
		return -EINVAL;

	policy->user_policy.min = min;
	policy->user_policy.max = max;
	cpufreq_cpu_put(policy);

	/* TODO qinyonghui */
	cpufreq_update_policy(cpu);
	return 0;
}

static int msm_gpu_info_init(struct hypnus_data *pdata)
{
	unsigned long min = 0, max =0;
	struct kgsl_pwrscale *pwrscale;
	struct kgsl_device *device = NULL;
	int ret = 0;

	pdata->gpu_nr = 1;

	mutex_lock(&kgsl_driver.devlock);
	device = kgsl_driver.devp[0];
	mutex_unlock(&kgsl_driver.devlock);

	if (device == NULL) {
		pr_err("Invalid gpu device\n");
		return -EINVAL;
	}

	pwrscale = &device->pwrscale;
	ret = devfreq_get_limit(pwrscale->devfreqptr, &min, &max);
	if (ret) {
		pr_err("Fail to get gpufreq %d~%d\n", min, max);
		return -EINVAL;
	}
	pdata->gpu_data[0].gpufreq_min = min;
	pdata->gpu_data[0].gpufreq_max = max;
	return 0;
}

static int msm_get_gpu_load(u32 gpu_index)
{
	struct kgsl_device *device = NULL;
	struct kgsl_pwrctrl *pwr;
	struct kgsl_clk_stats *stats;
	unsigned int load;

	if (gpu_index >= NR_GPUS)
		return -EINVAL;

	mutex_lock(&kgsl_driver.devlock);
	device = kgsl_driver.devp[gpu_index];
	mutex_unlock(&kgsl_driver.devlock);

	pwr = &device->pwrctrl;
	stats = &pwr->clk_stats;
	load = stats->busy_old * 100 / stats->total_old;

	return load;
}

static int msm_get_gpu_freq(u32 gpu_index, unsigned int *min,
			    unsigned int *cur, unsigned int *max)
{
	struct kgsl_device *device = NULL;
	struct kgsl_pwrctrl *pwr;
	unsigned int max_level, min_level;

	if (gpu_index >= NR_GPUS)
		return -EINVAL;

	mutex_lock(&kgsl_driver.devlock);
	device = kgsl_driver.devp[gpu_index];
	mutex_unlock(&kgsl_driver.devlock);

	pwr = &device->pwrctrl;

	max_level = max_t(unsigned int, pwr->max_pwrlevel,
				pwr->thermal_pwrlevel);
	min_level = max_t(unsigned int, pwr->min_pwrlevel,
				pwr->thermal_pwrlevel);
	*max = pwr->pwrlevels[max_level].gpu_freq;
	*min = pwr->pwrlevels[min_level].gpu_freq;

	return 0;
}

/* Given a GPU clock value, return the lowest matching powerlevel */
static int get_nearest_pwrlevel(struct kgsl_pwrctrl *pwr, unsigned int clock,
				unsigned int *real_freq)
{
	int i;

	if (clock > pwr->pwrlevels[0].gpu_freq) {
		if (real_freq)
			*real_freq = pwr->pwrlevels[0].gpu_freq;
		return 0;
	} else if (clock < pwr->pwrlevels[pwr->num_pwrlevels - 1].gpu_freq) {
		if (real_freq)
			*real_freq =
				pwr->pwrlevels[pwr->num_pwrlevels - 1].gpu_freq;
		return pwr->num_pwrlevels - 1;
	}

	/* Find the neighboring frequencies */
	for (i = 0; i < pwr->num_pwrlevels - 1; i++) {
		if ((pwr->pwrlevels[i].gpu_freq >= clock) &&
			(pwr->pwrlevels[i + 1].gpu_freq < clock)) {
			if (real_freq)
				*real_freq = pwr->pwrlevels[i].gpu_freq;
			return i;
		}
	}
	return -ERANGE;
}

static int msm_set_gpu_freq_limit(u32 gpu_index, u32 min_freq, u32 max_freq)
{
	struct kgsl_device *device = NULL;
	struct kgsl_pwrctrl *pwr;
	struct kgsl_pwrscale *pwrscale;
	int max_level, min_level;
	int ret;

	if (gpu_index >= NR_GPUS)
		return -EINVAL;

	mutex_lock(&kgsl_driver.devlock);
	device = kgsl_driver.devp[gpu_index];
	mutex_unlock(&kgsl_driver.devlock);

	if (device == NULL) {
		pr_err("Invalid gpu device\n");
		return -EINVAL;
	}

	pwr = &device->pwrctrl;
	pwrscale = &device->pwrscale;
	max_level = get_nearest_pwrlevel(pwr, max_freq, &max_freq);
	min_level = get_nearest_pwrlevel(pwr, min_freq, &min_freq);

	ret = devfreq_set_limit(pwrscale->devfreqptr, (unsigned long)min_freq,
				(unsigned long)max_freq);
	if (ret) {
		pr_err("Fail to set gpufreq %d~%d\n", min_freq, max_freq);
		return -EINVAL;
	}

	return ret;
}

static int msm_set_lpm_gov(u32 type)
{
	switch (type) {
		case LPM_DEFAULT:
			pm_qos_update_request(&hypnus_lpm_qos_request, PM_QOS_DEFAULT_VALUE);
			break;
		case LPM_USE_GOVERNOR:
			break;
		case LPM_DISABLE_SLEEP:
			pm_qos_update_request(&hypnus_lpm_qos_request, PM_QOS_DISABLE_SLEEP_VALUE);
			break;
	}
	return 0;
}

static int msm_set_storage_clk_scaling(void)
{
	/* Todo */
	return 0;
}

static int msm_display_init(void)
{
	/* Todo */
	return 0;
}

static u64 msm_get_frame_cnt(void)
{
	/* Todo */
	return 0;
}

static int msm_get_display_resolution(unsigned int *xres, unsigned int *yres)
{
	/* Todo */
	return 0;
}

static int msm_set_ddr_state(u32 state)
{
	/* Todo */
	return 0;
}

static int msm_set_thermal_policy(bool use_default)
{
	/* Todo */
	return 0;
}

static int msm_get_updown_migrate(unsigned int *up_migrate,
				  unsigned int *down_migrate)
{
#if LINUX_VERSION_CODE >= KERNEL_VERSION(4,9,0)
	sched_get_updown_migrate(up_migrate, down_migrate);
	return 0;
#else
	return -ENOTSUPP;
#endif
}

static int msm_set_updown_migrate(unsigned int up_migrate,
				  unsigned int down_migrate)
{
#if LINUX_VERSION_CODE >= KERNEL_VERSION(4,9,0)
	return sched_set_updown_migrate(&up_migrate, &down_migrate);
#else
	return -ENOTSUPP;
#endif
}

static int msm_set_cpunr_limit(struct hypnus_data *pdata, unsigned int index,
		unsigned int min, unsigned int max)
{
	return hypnus_set_min_max_cpus(index, min, max);
}

static struct hypnus_chipset_operations msm_op = {
	.name = "msm",
	.get_running_avg = msm_get_running_avg,
	.get_cpu_load = msm_get_cpu_load,
	.set_cpu_freq_limit = msm_set_cpu_freq_limit,
	.set_cpunr_limit = msm_set_cpunr_limit,
	.isolate_cpu = sched_isolate_cpu,
	.unisolate_cpu = sched_unisolate_cpu,
	.get_gpu_load = msm_get_gpu_load,
	.get_gpu_freq = msm_get_gpu_freq,
	.set_gpu_freq_limit = msm_set_gpu_freq_limit,
	.gpu_info_init = msm_gpu_info_init,
	.display_init = msm_display_init,
	.get_frame_cnt = msm_get_frame_cnt,
	.get_display_resolution = msm_get_display_resolution,
	.set_lpm_gov = msm_set_lpm_gov,
	.set_storage_scaling = msm_set_storage_clk_scaling,
	.set_sched_prefer_idle = NULL,
	.set_ddr_state = msm_set_ddr_state,
	.set_thermal_policy = msm_set_thermal_policy,
	.get_boost = msm_get_boost,
	.set_boost = msm_set_boost,
	.get_updown_migrate = msm_get_updown_migrate,
	.set_updown_migrate = msm_set_updown_migrate,
};

void hypnus_chipset_op_init(struct hypnus_data *pdata)
{
	pm_qos_add_request(&hypnus_lpm_qos_request, PM_QOS_CPU_DMA_LATENCY, PM_QOS_DEFAULT_VALUE);
	pdata->cops = &msm_op;
}
