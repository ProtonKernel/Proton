/*
 * Copyright (C) 2013 Google, Inc.
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

#if !defined(_TRACE_SYSTRACE_H) || defined(TRACE_HEADER_MULTI_READ)
#define _TRACE_SYSTRACE_H

#include <linux/types.h>
#include <linux/tracepoint.h>

#undef TRACE_SYSTEM
#define TRACE_SYSTEM systrace

TRACE_EVENT(tracing_mark_write,

        TP_PROTO(char *ev),
        TP_ARGS(ev),
        TP_STRUCT__entry(
                            __string(event, ev)
        ),
        TP_fast_assign(
                            __assign_str(event, ev);
        ),
        TP_printk("%s", __get_str(event))
    );

#endif /* _TRACE_SYSTRACE_H */

/* This part must be outside protection */
#include <trace/define_trace.h>
