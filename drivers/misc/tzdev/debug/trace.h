/*
 * Copyright (C) 2012-2019, Samsung Electronics Co., Ltd.
 *
 * This software is licensed under the terms of the GNU General Public
 * License version 2, as published by the Free Software Foundation, and
 * may be copied, distributed, and modified under those terms.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */

#if !defined(_TRACE_TZDEV_H) || defined(TRACE_HEADER_MULTI_READ)
#define _TRACE_TZDEV_H

#include <linux/tracepoint.h>
#include "tzdev_internal.h"

#ifndef CONFIG_TZDEV_TRACING

#undef TRACE_EVENT
#define TRACE_EVENT(name, proto, ...) \
	static inline void trace_ ## name(proto) {}

#undef DECLARE_EVENT_CLASS
#define DECLARE_EVENT_CLASS(...)

#undef DEFINE_EVENT
#define DEFINE_EVENT(evt_class, name, proto, ...) \
static inline void trace_ ## name(proto) {}

#endif

#undef TRACE_SYSTEM
#define TRACE_SYSTEM tzdev

TRACE_EVENT(tzdev_ioctl,
		TP_PROTO(unsigned int num),

		TP_ARGS(num),

		TP_STRUCT__entry(
			__field(unsigned int, ioctl_num)
		),

		TP_fast_assign(
			__entry->ioctl_num = num;
		),

		TP_printk("command  = %u", __entry->ioctl_num)
);

TRACE_EVENT(tzdev_pre_smc,
		TP_PROTO(struct tzdev_smc_data *data),

		TP_ARGS(data),

		TP_STRUCT__entry(
			__array(uint32_t, args, NR_SMC_ARGS)
		),

		TP_fast_assign(
			memcpy(&__entry->args, &data->args, sizeof(uint32_t) * NR_SMC_ARGS);
		),

		TP_printk("args=[0x%x 0x%x 0x%x 0x%x 0x%x 0x%x 0x%x]",
			__entry->args[0],
			__entry->args[1],
			__entry->args[2],
			__entry->args[3],
			__entry->args[4],
			__entry->args[5],
			__entry->args[6]
		)
);

TRACE_EVENT(tzdev_post_smc,
		TP_PROTO(struct tzdev_smc_data *data),

		TP_ARGS(data),

		TP_STRUCT__entry(
			__array(uint32_t, args, NR_SMC_ARGS)
		),

		TP_fast_assign(
			memcpy(&__entry->args, &data->args, sizeof(uint32_t) * NR_SMC_ARGS);
		),

		TP_printk("args=[0x%x 0x%x 0x%x 0x%x]",
			__entry->args[0],
			__entry->args[1],
			__entry->args[2],
			__entry->args[3]
		)
);

TRACE_EVENT(tzdev_worker,
		TP_PROTO(const char *name, int event),

		TP_ARGS(name, event),

		TP_STRUCT__entry(
			__field(int, event)
			__array(char, name, 80)
		),

		TP_fast_assign(
			__entry->event = event;
			memcpy(__entry->name, name, 80);
		),

		TP_printk("%s, event = %d", __entry->name, __entry->event)
);

DECLARE_EVENT_CLASS(tzdev_cred_cache_template,
		TP_PROTO(int iterations),

		TP_ARGS(iterations),

		TP_STRUCT__entry(
			__field(int, iter)
		),

		TP_fast_assign(
			__entry->iter = iterations;
		),

		TP_printk("after %d iterations", __entry->iter)
);

DEFINE_EVENT(tzdev_cred_cache_template,
		tzdev_cred_cache_hit,
		TP_PROTO(int iterations),
		TP_ARGS(iterations)
);

DEFINE_EVENT(tzdev_cred_cache_template,
		tzdev_cred_cache_miss,
		TP_PROTO(int iterations),
		TP_ARGS(iterations)
);

#endif /* _TRACE_TZDEV_H */

#ifdef CONFIG_TZDEV_TRACING
#undef TRACE_INCLUDE_PATH
#define TRACE_INCLUDE_PATH ../debug
#undef TRACE_INCLUDE_FILE
#define TRACE_INCLUDE_FILE trace

 /* This part must be outside protection */
#include <trace/define_trace.h>

#endif /* CONFIG_TZDEV_TRACING */
