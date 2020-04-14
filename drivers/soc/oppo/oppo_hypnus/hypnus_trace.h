/*
 * Copyright (c) 2019, Guangdong OPPO Mobile Communication(Shanghai)
 * Corp.,Ltd. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *     * Redistributions of source code must retain the above copyright
 *       notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above
 *       copyright notice, this list of conditions and the following
 *       disclaimer in the documentation and/or other materials provided
 *       with the distribution.
 *     * Neither the name of The Linux Foundation nor the names of its
 *       contributors may be used to endorse or promote products derived
 *       from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED "AS IS" AND ANY EXPRESS OR IMPLIED
 * WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF
 * MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NON-INFRINGEMENT
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR
 * BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
 * WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE
 * OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN
 * IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#undef TRACE_SYSTEM
#define TRACE_SYSTEM hypnus
#undef TRACE_INCLUDE_PATH
#define TRACE_INCLUDE_PATH .
#undef TRACE_INCLUDE_FILE
#define TRACE_INCLUDE_FILE hypnus_trace

#if !defined(__HYPNUS_TRACE_H__) || defined(TRACE_HEADER_MULTI_READ)
#define __HYPNUS_TRACE_H__

#include <linux/tracepoint.h>

TRACE_EVENT(hypnus_ioctl_submit_cpufreq,
	TP_PROTO(struct hypnus_cpufreq_prop *prop),
	TP_ARGS(prop),
	TP_STRUCT__entry(
		__field(u32, c0_min)
		__field(u32, c0_max)
		__field(u32, c1_min)
		__field(u32, c1_max)
	),
	TP_fast_assign(
		__entry->c0_min = prop->freq_prop[0].min;
		__entry->c0_max = prop->freq_prop[0].max;
		__entry->c1_min = prop->freq_prop[1].min;
		__entry->c1_max = prop->freq_prop[1].max;
	),
	TP_printk("CPUFreq C0: %u:%u, C1: %u:%u", __entry->c0_min, __entry->c0_max,
		__entry->c1_min, __entry->c1_max
	)
);

TRACE_EVENT(hypnus_ioctl_submit_gpufreq,
	TP_PROTO(struct hypnus_gpufreq_prop *prop),
	TP_ARGS(prop),
	TP_STRUCT__entry(
		__field(u32, min)
		__field(u32, max)
	),
	TP_fast_assign(
		__entry->min = prop->freq_prop[0].min;
		__entry->max = prop->freq_prop[0].max;
	),
	TP_printk("GPUFreq: %u:%u", __entry->min, __entry->max)
);

#endif /* __HYPNUS_TRACE_H__ */
/* This part must be outside protection */
#include <trace/define_trace.h>
