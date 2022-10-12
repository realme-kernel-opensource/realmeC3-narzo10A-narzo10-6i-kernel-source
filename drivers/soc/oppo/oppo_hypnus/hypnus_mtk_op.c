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

#include <linux/cpufreq.h>
#include <linux/sched.h>
#include <mt-plat/mtk_sched.h>
#include <ppm_v3/mtk_ppm_api.h>
#include <mtk_mcdi_governor.h>
#include <sched_ctl.h>
#include <mtk_mcdi_state.h>
#include "hypnus_op.h"

//#include <mtk_gpufreq_plat.h>

#if 0
#include <vpu_drv.h>
#endif
#include <mdla_ioctl.h>
#include <mt-plat/fpsgo_common.h>
#include <linux/pm_qos.h>

static struct pm_qos_request hypnus_ddr_qos_request;

extern void sched_get_nr_running_avg(int *avg, int *iowait_avg);
extern int mt_gpufreq_scene_protect(unsigned int min_freq, unsigned int max_freq);
extern int set_sched_boost(unsigned int val);
extern int sched_deisolate_cpu(int cpu);
extern int sched_isolate_cpu(int cpu);

extern unsigned int mt_gpufreq_get_freq_by_idx(unsigned int idx);


#if 0
extern int vpu_lock_set_power(struct vpu_lock_power *vpu_lock_power);
#endif
extern unsigned int mt_gpufreq_get_dvfs_table_num(void);
extern struct g_opp_table_info *mt_gpufreq_get_dvfs_table(void);
extern unsigned int mt_gpufreq_get_min_limit_freq(void);
extern int mdla_lock_set_power(struct mdla_lock_power *mdla_lock_power);
//extern u32 kbasep_get_gl_utilization(void);

enum g_limited_idx_enum {
	IDX_THERMAL_PROTECT_LIMITED = 0,
	IDX_LOW_BATT_LIMITED,
	IDX_BATT_PERCENT_LIMITED,
	IDX_BATT_OC_LIMITED,
	IDX_PBM_LIMITED,
	IDX_SCENE_LIMITED,
	NUMBER_OF_LIMITED_IDX,
};

enum {
	IDX_SCENE_MIN_LIMITED,
	NR_IDX_POWER_MIN_LIMITED,
};

static int mt_get_running_avg(int *avg, int *big_avg, int *iowait)
{
	sched_get_nr_running_avg(avg, iowait);
	*big_avg = sched_get_nr_heavy_task();
	if (*avg >= 100 * NR_CPUS)
		*avg = 100 * NR_CPUS;
	*big_avg *= 100;

	return 0;
}

static int mt_get_cpu_load(u32 cpu)
{
	return sched_get_percpu_load(cpu, 1, 1);
}

static int mt_set_boost(u32 boost)
{
	set_sched_boost(boost);
	return 0;
}

static int cpufreq_set_cpu_scaling_limit(unsigned int cpu, unsigned int min,
        unsigned int max)
{
	struct cpufreq_policy *policy;

	if (!cpu_online(cpu))
		return 0;

	policy = cpufreq_cpu_get(cpu);

	if (!policy)
		return -1;

	policy->user_policy.min = min;
	policy->user_policy.max = max;
	cpufreq_cpu_put(policy);

	cpufreq_update_policy(cpu);
	return 0;
}

static int mt_set_cpu_freq_limit(u32 c_index, u32 min, u32 max)
{
	struct hypnus_data *hypdata = hypnus_get_hypdata();
	struct cpumask *pmask = NULL;
	int cpu;

	if (!hypdata)
		return -EINVAL;

	pmask = &hypdata->cluster_data[c_index].cluster_mask;
	mt_ppm_sysboost_set_freq_limit(BOOST_BY_UT, c_index, min, max);
	cpu = cpumask_first(pmask);
	if (cpufreq_set_cpu_scaling_limit(cpu, min, max) < 0) {
		pr_info("Fail to update cpu %d, min: %u, max: %u\n",
			cpu, min, max);
		return -1;
	}
	return 0;
}

static int mt_gpu_info_init(struct hypnus_data *pdata)
{
	unsigned int table_num = mt_gpufreq_get_dvfs_table_num();
	if (table_num < 0)
		return -1;

	pdata->gpu_data[0].gpufreq_min = mt_gpufreq_get_freq_by_idx(table_num - 1);
	pdata->gpu_data[0].gpufreq_max = mt_gpufreq_get_freq_by_idx(0);

	return 0;
}

static int mt_get_gpu_load(u32 gpu_index)
{
	return 0;
}

static int mt_get_gpu_freq(u32 gpu_index, unsigned int *min,
				unsigned int *cur, unsigned int *max)
{
	return 0;
}

static int mt_set_gpu_freq_limit(u32 gpu_index, u32 min_freq, u32 max_freq)
{
	struct hypnus_data *hypdata = hypnus_get_hypdata();
	int ret = 0;

	if (!hypdata || gpu_index >= hypdata->gpu_nr) {
		pr_err("GPU index %d is not supported!\n", gpu_index);
		return -EINVAL;
	}

	if (min_freq > max_freq) {
		pr_err("GPU: Invalid min_freq:%u max_freq:%u\n", min_freq, max_freq);
		return -EINVAL;
	}

	ret = mt_gpufreq_scene_protect(min_freq, max_freq);
	if (ret) {
		pr_err("GPU freq setting min_freq:%u max_freq:%u Failed\n", min_freq, max_freq);
		return ret;
	}
	return 0;
}

static int mt_set_lpm_gov(u32 type)
{
	switch (type) {
	case LPM_DEFAULT:
	set_mcdi_enable_status(true);
	set_mcdi_s_state(MCDI_STATE_SODI3);
    break;
	case LPM_USE_GOVERNOR:
	case LPM_DISABLE_SLEEP:
	set_mcdi_enable_status(false);
	break;
	default:
	pr_err("Error: Invalid decision for LPM:%d\n",type);
	break;
	}
	return 0;
}

int mt_set_ddr_state(u32 state)
{
	pm_qos_update_request(&hypnus_ddr_qos_request, state);
	return 0;
}

int mt_set_fpsgo_engine(u32 enable)
{
	if (enable && !fpsgo_is_enable()) {
		fpsgo_switch_enable(1);
	} else if (!enable && fpsgo_is_enable()) {
		fpsgo_switch_enable(0);
	}
	return 0;
}

static int mt_set_thermal_policy(bool use_default)
{
	return 0;
}

static int mt_unisolate_cpu(int cpu)
{
	return sched_deisolate_cpu(cpu);
}

static u64 mt_get_frame_cnt(void)
{
	return 0;
}

static int mt_get_display_resolution(unsigned int *xres, unsigned int *yres)
{
	return 0;
}

static int mt_get_updown_migrate(unsigned int *up_migrate, unsigned int *down_migrate)
{
	return sched_get_updown_migrate(up_migrate, down_migrate);
}

static int mt_set_updown_migrate(unsigned int up_migrate, unsigned int down_migrate)
{
	return sched_set_updown_migrate(up_migrate, down_migrate);
}


static int mt_set_vpu_freq_limit(u32 core_id, u32 min, u32 max)
{
	/*struct vpu_lock_power vpu_lock_power;


	vpu_lock_power.core = (0x1 << core_id);
	vpu_lock_power.lock = true;
	vpu_lock_power.priority = POWER_HAL;
	vpu_lock_power.max_boost_value = max;
	vpu_lock_power.min_boost_value = min;


	return vpu_lock_set_power(&vpu_lock_power);*/
	return 0;
}

static int mt_set_mdla_freq_limit(u32 core_id, u32 min, u32 max)
{
	/*struct mdla_lock_power mdla_lock_power;


	mdla_lock_power.core = (0x1 << core_id);
	mdla_lock_power.lock = true;
	mdla_lock_power.priority = MDLA_OPP_POWER_HAL;
	mdla_lock_power.max_boost_value = max;
	mdla_lock_power.min_boost_value = min;


	return mdla_lock_set_power(&mdla_lock_power);*/
	return 0;


}

static int mt_set_cpunr_limit(struct hypnus_data *pdata, unsigned int index,
		unsigned int min, unsigned int max)
{
#if 0
	struct cluster_data *cluster;
	struct cpumask *cluster_mask, pmask;
	unsigned int cpu, now_cpus, need_cpus;
	int ret;

	if (index >=  pdata->cluster_nr)
		return -EINVAL;

	ret = now_cpus = cpu = 0;
	cluster = &pdata->cluster_data[i];
	cluster_mask = &cluster->cluster_mask;
	now_cpus = cpu_available_count(cluster_mask);
	need_cpus = prop->need_cpus[i];

	if (need_cpus > now_cpus) {
		cpumask_and(&pmask, cluster_mask, cpu_online_mask);
		cpumask_and(&pmask, &pmask, cpu_isolated_mask);
		for_each_cpu(cpu, &pmask) {
			hypnus_unisolate_cpu(pdata, cpu);
			if (need_cpus
				<= cpu_available_count(cluster_mask))
				break;
		}
	} else if (need_cpus < now_cpus) {
		cpu = cpumask_first(cluster_mask);

		for (j = cluster->num_cpus - 1; j >= 0; j--) {
			if (cpu_isolated(cpu + j)
				|| !cpu_online(cpu + j))
				continue;
			hypnus_isolate_cpu(pdata, cpu + j);
			if (need_cpus
				>= cpu_available_count(cluster_mask))
				break;
		}
	}

	ret |= (need_cpus != cpu_available_count(cluster_mask));
	return ret;
#endif
	return 0;
}

static struct hypnus_chipset_operations mediatek_op = {
	.name = "mediatek",
	.get_running_avg = mt_get_running_avg,
	.get_cpu_load = mt_get_cpu_load,
	.get_gpu_load = mt_get_gpu_load,
	.get_gpu_freq = mt_get_gpu_freq,
	.gpu_info_init = mt_gpu_info_init,
	.set_boost = mt_set_boost,
	.set_cpu_freq_limit = mt_set_cpu_freq_limit,
	.set_gpu_freq_limit = mt_set_gpu_freq_limit,
	.set_lpm_gov = mt_set_lpm_gov,
	.set_storage_scaling = NULL,
	.set_ddr_state = mt_set_ddr_state,
	.set_fpsgo_engine = mt_set_fpsgo_engine,
	.set_thermal_policy = mt_set_thermal_policy,
	.isolate_cpu = sched_isolate_cpu,
	.unisolate_cpu = mt_unisolate_cpu,
	.set_sched_prefer_idle = NULL,
	.get_frame_cnt = mt_get_frame_cnt,
	.get_display_resolution = mt_get_display_resolution,
	.get_updown_migrate = mt_get_updown_migrate,
	.set_updown_migrate = mt_set_updown_migrate,
	.set_dsp_freq_limit = NULL, //mt_set_vpu_freq_limit,
	.set_npu_freq_limit = NULL, //mt_set_mdla_freq_limit,

};

void hypnus_chipset_op_init(struct hypnus_data *pdata)
{
	pm_qos_add_request(&hypnus_ddr_qos_request,
		       PM_QOS_DDR_OPP,
		       PM_QOS_VCORE_OPP_DEFAULT_VALUE);
	pdata->cops = &mediatek_op;
}
