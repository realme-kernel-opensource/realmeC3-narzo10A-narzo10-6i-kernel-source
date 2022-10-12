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

#include <linux/init.h>
#include <linux/module.h>
#include <linux/vmalloc.h>
#include <linux/kthread.h>
#include <linux/cpufreq.h>
#include <linux/types.h>
#include <linux/cpu.h>
#include <linux/topology.h>

#if defined(CONFIG_ARCH_QCOM)
#include <linux/msm_kgsl.h>
#endif

#include "hypnus.h"
#include "hypnus_op.h"
#include "hypnus_dev.h"
#include "hypnus_sysfs.h"
#include "hypnus_uapi.h"

#define CREATE_TRACE_POINTS
#include "hypnus_trace.h"

static struct hypnus_data g_hypdata;

long hypnus_ioctl_get_rq(struct hypnus_data *pdata,
	unsigned int cmd, void *data)
{
	struct hypnus_rq_prop *prop = data;

	if (!pdata->cops->get_running_avg)
		return -ENOTSUPP;

	pdata->cops->get_running_avg(&prop->avg, &prop->big_avg,
					&prop->iowait_avg);

	return 0;
}

long hypnus_ioctl_get_cpuload(struct hypnus_data *pdata,
	unsigned int cmd, void *data)
{
	struct hypnus_cpuload_prop *prop = data;
	unsigned int cpu = 0;

	if (!pdata->cops->get_cpu_load)
		return -ENOTSUPP;

	for_each_possible_cpu(cpu) {
		if (cpu_online(cpu))
			prop->cpu_load[cpu] = pdata->cops->get_cpu_load(cpu);
		else
			prop->cpu_load[cpu] = -1;
	}

	return 0;
}

long hypnus_ioctl_submit_cpufreq(struct hypnus_data *pdata,
	unsigned int cmd, void *data)
{
	struct hypnus_cpufreq_prop *prop = data;
	int i, ret = 0;

	if (!pdata->cops->set_cpu_freq_limit)
		return -ENOTSUPP;

	trace_hypnus_ioctl_submit_cpufreq(prop);
	for (i = 0; i < pdata->cluster_nr; i++) {
		ret |= pdata->cops->set_cpu_freq_limit(i,
				prop->freq_prop[i].min,
				prop->freq_prop[i].max);
	}

	return ret;
}

long hypnus_ioctl_set_lpm_gov(struct hypnus_data *pdata,
	unsigned int cmd, void *data)
{
	struct hypnus_lpm_prop *prop = data;

	if (!pdata->cops->set_lpm_gov)
		return -ENOTSUPP;

	return pdata->cops->set_lpm_gov(prop->lpm_gov);
}

long hypnus_ioctl_get_gpuload(struct hypnus_data *pdata,
	unsigned int cmd, void *data)
{
	struct hypnus_gpuload_prop *prop = data;
	int i;

	if (!pdata->cops->get_gpu_load)
		return -ENOTSUPP;

	for (i = 0; i < pdata->gpu_nr; i++)
		prop->gpu_load[i] = pdata->cops->get_gpu_load(i);

	return 0;
}

long hypnus_ioctl_get_gpufreq(struct hypnus_data *pdata,
	unsigned int cmd, void *data)
{
	struct hypnus_gpufreq_prop *prop = data;
	unsigned int *min, *max, *cur;
	int i;

	if (!pdata->cops->get_gpu_freq)
		return -ENOTSUPP;

	for (i = 0; i < pdata->gpu_nr; i++) {
		min = &prop->freq_prop[i].min;
		max = &prop->freq_prop[i].max;
		cur = &prop->freq_prop[i].cur;
		pdata->cops->get_gpu_freq(i, min, cur, max);
	}

	return 0;
}

long hypnus_ioctl_submit_gpufreq(struct hypnus_data *pdata,
	unsigned int cmd, void *data)
{
	struct hypnus_gpufreq_prop *prop = data;
	unsigned int min, max;
	int i;

	if (!pdata->cops->set_gpu_freq_limit)
		return -ENOTSUPP;

	trace_hypnus_ioctl_submit_gpufreq(prop);
	for (i = 0; i < pdata->gpu_nr; i++) {
		min = prop->freq_prop[i].min;
		max = prop->freq_prop[i].max;
		pdata->cops->set_gpu_freq_limit(i, min, max);
	}

	return 0;
}

long hypnus_ioctl_submit_lpm(struct hypnus_data *pdata,
	unsigned int cmd, void *data)
{
	return 0;
}

int hypnus_ioclt_submit_ddr(struct hypnus_data *pdata, u32 type)
{
	/* Todo */
	return 0;
}

int hypnus_ioclt_submit_thermal_policy(struct hypnus_data *pdata)
{
	/* Todo */
	return 0;
}

long hypnus_ioctl_get_boost(struct hypnus_data *pdata,
	unsigned int cmd, void *data)
{
	struct hypnus_boost_prop *prop = data;

	if (pdata->cops->get_boost)
		prop->sched_boost = pdata->cops->get_boost();
	else
		return -ENOTSUPP;

	return 0;
}

long hypnus_ioctl_submit_boost(struct hypnus_data *pdata,
	unsigned int cmd, void *data)
{
	int ret = 0;
	struct hypnus_boost_prop *prop = data;

	if (pdata->cops->set_boost) {
		ret = pdata->cops->set_boost(prop->sched_boost);
		if (ret)
			pr_err("%s err %d\n", __func__, ret);
	} else
		return -ENOTSUPP;

	return ret;
}

long hypnus_ioctl_get_migration(struct hypnus_data *pdata,
	unsigned int cmd, void *data)
{
	int ret = 0;
	struct hypnus_migration_prop *prop = data;
	int *up, *down;

	if (!pdata->cops->get_updown_migrate)
		return -ENOTSUPP;

	up = prop->up_migrate;
	down = prop->down_migrate;

	ret = pdata->cops->get_updown_migrate(up, down);
	if (ret)
		pr_err("%s err %d\n", __func__, ret);

	return ret;
}

long hypnus_ioctl_submit_migration(struct hypnus_data *pdata,
	unsigned int cmd, void *data)
{
	int ret;
	struct hypnus_migration_prop *prop = data;
	int up, down;

	if (!pdata->cops->set_updown_migrate)
		return -ENOTSUPP;

	up = prop->up_migrate[0];
	down = prop->down_migrate[0];

	ret = pdata->cops->set_updown_migrate(up, down);
	if (ret)
		pr_err("%s err %d\n", __func__, ret);

	return ret;
}

long hypnus_ioctl_submit_ddr(struct hypnus_data *pdata,
	unsigned int cmd, void *data)
{
	int ret;
	struct hypnus_ddr_prop *prop = data;
	int state;

	if (!pdata->cops->set_ddr_state)
		return -ENOTSUPP;

	state = prop->state;
	ret = pdata->cops->set_ddr_state(state);
	if (ret)
		pr_err("%s err %d\n", __func__, ret);

	return ret;
}

static inline unsigned int
cpu_available_count(struct cpumask *cluster_mask)
{
	struct cpumask mask;

	cpumask_and(&mask, cluster_mask, cpu_online_mask);
	cpumask_andnot(&mask, &mask, cpu_isolated_mask);

	return cpumask_weight(&mask);
}

static int hypnus_unisolate_cpu(struct hypnus_data *pdata, unsigned int cpu)
{
	int ret = 0;

	if (cpu_isolated(cpu) && !pdata->cpu_data[cpu].not_preferred) {
		ret = pdata->cops->unisolate_cpu(cpu);
		if (ret)
			pr_err("Unisolate CPU%u failed! err %d\n", cpu, ret);
	}

	return ret;
}

static int hypnus_isolate_cpu(struct hypnus_data *pdata, unsigned int cpu)
{
	int ret;

	ret = pdata->cops->isolate_cpu(cpu);
	if (ret)
		pr_err("Isolate CPU%u failed! err %d\n", cpu, ret);

	return ret;
}

long hypnus_ioctl_submit_cpunr(struct hypnus_data *pdata,
	unsigned int cmd, void *data)
{
	struct hypnus_cpunr_prop *prop = data;
	int i, ret = 0;

	if (!pdata->cops->set_cpunr_limit)
		return -ENOTSUPP;

	for (i = 0; i < pdata->cluster_nr; i++) {
		ret |= pdata->cops->set_cpunr_limit(pdata, i, prop->cpus[i].min, prop->cpus[i].max);
	}

	return ret;
}

long hypnus_ioctl_submit_decision(struct hypnus_data *pdata,
	unsigned int cmd, void *data)
{
	return 0;
}

long hypnus_ioctl_poll_status(struct hypnus_data *pdata, unsigned int cmd, void *data)
{
	struct hypnus_poll_status *status = data;
	unsigned long t, flags;
	int ret = 0;

	if (!pdata)
		return -EINVAL;

	t = wait_for_completion_interruptible_timeout(&pdata->wait_event, 5*HZ);
	if (!t) {
		ret = -ETIMEDOUT;
	}
	spin_lock_irqsave(&pdata->lock, flags);
	status->screen_on = pdata->screen_on;
	spin_unlock_irqrestore(&pdata->lock, flags);
	return ret;
}

long hypnus_ioctl_get_soc_info(struct hypnus_data *pdata, unsigned int cmd, void *data)
{
	struct hypnus_soc_info *info = data;
	int i = 0;

	info->cluster_nr = pdata->cluster_nr;
	info->gpu_nr = pdata->gpu_nr;
	info->dsp_nr = pdata->dsp_nr;
	info->npu_nr = pdata->npu_nr;
	for (i = 0; i < pdata->cluster_nr; i++) {
		info->cluster[i].cluster_id = i;
		info->cluster[i].cpu_mask = cpumask_bits(&pdata->cluster_data[i].cluster_mask)[0];
		info->cluster[i].cpufreq.min = pdata->cluster_data[i].cpufreq_min;
		info->cluster[i].cpufreq.max = pdata->cluster_data[i].cpufreq_max;
	}

	for (i = 0; i < NR_GPUS; i++) {
		info->gpu[i].gpufreq.min = pdata->gpu_data[i].gpufreq_min;
		info->gpu[i].gpufreq.max = pdata->gpu_data[i].gpufreq_max;
	}
	return 0;
}

long hypnus_ioctl_submit_dspfreq(struct hypnus_data *pdata,
	unsigned int cmd, void *data)
{
	struct hypnus_dspfreq_prop *prop = data;
	int i, ret = 0;

	if (!pdata->cops->set_dsp_freq_limit)
		return -ENOTSUPP;

	for (i = 0; i < pdata->dsp_nr; i++) {
		ret |= pdata->cops->set_dsp_freq_limit(i,
				prop->freq_prop[i].min,
				prop->freq_prop[i].max);
	}

	return ret;
}

long hypnus_ioctl_submit_npufreq(struct hypnus_data *pdata,
	unsigned int cmd, void *data)
{
	struct hypnus_npufreq_prop *prop = data;
	int i, ret = 0;

	if (!pdata->cops->set_npu_freq_limit)
		return -ENOTSUPP;

	for (i = 0; i < pdata->npu_nr; i++) {
		ret |= pdata->cops->set_npu_freq_limit(i,
				prop->freq_prop[i].min,
				prop->freq_prop[i].max);
	}

	return ret;
}

long hypnus_ioctl_set_fpsgo(struct hypnus_data *pdata, unsigned int cmd, void *data)
{
	unsigned int *val = (unsigned int *)data;
	unsigned int enable;

	if (!pdata->cops->set_fpsgo_engine)
		return -ENOTSUPP;

	enable = !!(*val);
	return pdata->cops->set_fpsgo_engine(enable);
}

static int hypnus_parse_cpu_topology(struct hypnus_data *pdata)
{
	struct list_head *head = get_cpufreq_policy_list();
	struct cpufreq_policy *policy;
	int cluster_nr = 0;

	if (!head)
		return -EINVAL;

	list_for_each_entry(policy, head, policy_list) {
		int first_cpu = cpumask_first(policy->related_cpus);
		int index, cpu;
		struct cpu_data *pcpu = NULL;

		if (unlikely(first_cpu > NR_CPUS)) {
			pr_err("Wrong related cpus 0x%x\n",
				(int)cpumask_bits(policy->related_cpus)[0]);
			return -EINVAL;
		}

		for_each_cpu(cpu, policy->related_cpus) {
			pcpu = &pdata->cpu_data[cpu];
			pcpu->id = cpu;
			pcpu->cluster_id = topology_physical_package_id(cpu);
		}

		index = topology_physical_package_id(first_cpu);
		pr_err("cluster idx = %d, cpumask = 0x%x\n", index,
				(int)cpumask_bits(policy->related_cpus)[0]);
		pdata->cluster_data[index].id = index;
		cpumask_copy(&pdata->cluster_data[index].cluster_mask,
				policy->related_cpus);
		pdata->cluster_data[index].cpufreq_min = policy->cpuinfo.min_freq;
		pdata->cluster_data[index].cpufreq_max = policy->cpuinfo.max_freq;
		pr_err("min freq: %u, max freq: %u\n", pdata->cluster_data[index].cpufreq_min,
					pdata->cluster_data[index].cpufreq_max);
		cluster_nr++;
	}
	pdata->cluster_nr = cluster_nr;
	pr_err("Totally %d clusters\n", pdata->cluster_nr);
	return 0;
}

static int cpu_data_init(struct hypnus_data *pdata, unsigned int cpuid)
{
	unsigned int cpu;
	struct cpufreq_policy *policy;
	struct cpu_data *cpudata = &pdata->cpu_data[cpuid];
	struct cluster_data *c_cluster;
	unsigned int first_cpu;

	if (!cpu_online(cpuid))
		return 0;

	policy = cpufreq_cpu_get(cpuid);
	if (!policy)
		return 0;

	for_each_cpu(cpu, policy->related_cpus) {
		cpudata = &pdata->cpu_data[cpu];
		c_cluster = &pdata->cluster_data[cpudata->cluster_id];
		first_cpu = cpumask_first(&c_cluster->cluster_mask);
		cpudata->id_in_cluster = cpu - first_cpu;
		c_cluster->num_cpus = cpumask_weight(&c_cluster->cluster_mask);
		c_cluster->avail_cpus = c_cluster->num_cpus;

		if (cpu_online(cpu)) {
			cpudata->online = true;
			c_cluster->online_cpus++;
		}
	}
	cpufreq_cpu_put(policy);

	return 0;
}

struct hypnus_data *hypnus_get_hypdata(void)
{
	return &g_hypdata;
}

int __init gpu_info_init(struct hypnus_data *pdata)
{
	pdata->gpu_nr = 1;
	if (pdata->cops->gpu_info_init)
		pdata->cops->gpu_info_init(pdata);
	return 0;
}

int __init dsp_info_init(struct hypnus_data *pdata)
{
	pdata->dsp_nr = 2; /* TO BE IMPROVED */
	return 0;
}

int __init npu_info_init(struct hypnus_data *pdata)
{
	pdata->npu_nr = 1; /* TO BE IMPROVED */
	return 0;
}

int __init hypnus_init(void)
{
	struct hypnus_data *pdata;
	unsigned int cpu;
	int ret;

	pdata = &g_hypdata;

	spin_lock_init(&pdata->lock);
	ret = hypnus_parse_cpu_topology(pdata);
	if (ret)
		goto err_hypnus_init;

	get_online_cpus();
	for_each_online_cpu(cpu) {
		ret = cpu_data_init(pdata, cpu);
		if (ret < 0) {
			pr_err("%s cpu data init err!\n", __func__);
			goto err_hypnus_init;
		}
	}
	put_online_cpus();

	/* initialize chipset operation hooks */
	hypnus_chipset_op_init(pdata);

	gpu_info_init(pdata);

	dsp_info_init(pdata);

	npu_info_init(pdata);

	ret = hypnus_sysfs_init(pdata);
	if (ret)
		goto err_hypnus_init;

	ret = hypnus_dev_init(pdata);
	if (ret)
		goto err_dev_init;

	display_info_register(pdata);

	return 0;

err_dev_init:
	hypnus_sysfs_remove(pdata);
err_hypnus_init:
	return ret;
}

static void __exit hypnus_exit(void)
{
	struct hypnus_data *pdata = &g_hypdata;

	hypnus_dev_uninit(pdata);
	hypnus_sysfs_remove(pdata);
	display_info_unregister(pdata);
}


//module_init(hypnus_init);
module_exit(hypnus_exit);

MODULE_DESCRIPTION("Hypnus system controller");
MODULE_VERSION(HYPNUS_VERSION);
MODULE_LICENSE("GPL");
