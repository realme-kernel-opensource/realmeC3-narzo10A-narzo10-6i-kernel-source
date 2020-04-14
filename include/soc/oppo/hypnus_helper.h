/*
 * Copyright (C) 2016 OPPO, Inc.
 * Author: Jie Cheng <jie.cheng@oppo.com>
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
#ifndef __HYPNUS_HELPER_H__
#define __HYPNUS_HELPER_H__

#define DEFAULT_CAPACITY 50

extern int hypnus_get_batt_capacity(void);
extern bool hypnus_get_charger_status(void);
extern bool hypnus_is_release(void);

extern unsigned int sched_get_percpu_load(int cpu, bool reset, bool use_maxfreq);
extern void sched_get_nr_running_avg(int *avg, int *iowait_avg);
extern unsigned int sched_get_heavy_task_threshold(void);
extern unsigned int sched_get_nr_heavy_task(void);

extern unsigned int is_audio_normal_playback(void);


#endif