/* SPDX-License-Identifier: GPL-2.0 */
#ifndef _LINUX_FREQ_QOS_TRACER_H
#define _LINUX_FREQ_QOS_TRACER_H
/* interface for the fre_qos tracer infrastructure of the linux kernel.
 *
 * Choonghoon Park <choong.park@samsung.com>
 */

#include <linux/cpufreq.h>

#define freq_qos_tracer_add_request(arg...)	\
	__freq_qos_tracer_add_request((char *)__func__, __LINE__, ##arg)

#if IS_ENABLED(CONFIG_ARM_FREQ_QOS_TRACER)
extern int __freq_qos_tracer_add_request(char *, unsigned int,
					 struct freq_constraints *,
					 struct freq_qos_request *,
					 enum freq_qos_req_type, s32);
extern int freq_qos_tracer_remove_request(struct freq_qos_request *);
#else
static __maybe_unused int
__freq_qos_tracer_add_request(char *func, unsigned int line,
			      struct freq_constraints *f_const,
			      struct freq_qos_request *f_req,
			      enum freq_qos_req_type type, s32 value) { return 0; }
static __maybe_unused int
freq_qos_tracer_remove_request(struct freq_qos_request *f_req) { return 0; }
#endif

#endif
