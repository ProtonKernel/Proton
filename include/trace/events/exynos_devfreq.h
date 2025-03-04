/* SPDX-License-Identifier: GPL-2.0 */
#undef TRACE_SYSTEM
#define TRACE_SYSTEM exynos_devfreq

#if !defined(_TRACE_EXYNOS_DEVFREQ_H) || defined(TRACE_HEADER_MULTI_READ)
#define _TRACE_EXYNOS_DEVFREQ_H

#include <linux/tracepoint.h>

TRACE_EVENT(exynos_devfreq,
	TP_PROTO(unsigned int type, unsigned int old_freq,
		 unsigned int new_freq, unsigned int en),

	TP_ARGS(type, old_freq, new_freq, en),

	TP_STRUCT__entry(
		__field(unsigned int, type)
		__field(unsigned int, old_freq)
		__field(unsigned int, new_freq)
		__field(unsigned int, en)
	),

	TP_fast_assign(
		__entry->type = type;
		__entry->old_freq = old_freq;
		__entry->new_freq = new_freq;
		__entry->en = en;
	),

	TP_printk("type=%u old_freq=%u new_freq=%u en=%u",
		__entry->type, __entry->old_freq, __entry->new_freq,
		__entry->en)
);

#endif /* _TRAC_EXYNOS_DEVFREQ_H */

/* This part must be outside protection */
#include <trace/define_trace.h>
