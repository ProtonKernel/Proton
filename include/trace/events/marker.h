/* SPDX-License-Identifier: GPL-2.0 */
#undef TRACE_SYSTEM
#define TRACE_SYSTEM marker

#if !defined(_TRACE_MARKER_H) || defined(TRACE_HEADER_MULTI_READ)
#define _TRACE_MARKER_H

#include <linux/types.h>
#include <linux/tracepoint.h>

DECLARE_EVENT_CLASS(mark_begin_end,

	TP_PROTO(char event_type,
		const char *category,
		const char *title,
		const char *name,
		unsigned long value,
		char ignore_pid),

	TP_ARGS(event_type, category, title, name, value, ignore_pid),

	TP_STRUCT__entry(
		__field(char,	event_type)
		__field(unsigned long, pid)
		__field(const char*, category)
		__field(const char*, title)
		__field(const char*, name)
		__field(unsigned long, value)
	),

	TP_fast_assign(
		__entry->event_type = event_type;
		__entry->pid = (ignore_pid) ? 0:current->tgid;
		__entry->category = category;
		__entry->title = title;
		__entry->name = name;
		__entry->value = value;
	),

	TP_printk("%c|%lu|%s:%s|%s=%lu;",
		__entry->event_type,
		__entry->pid,
		__entry->category,
		__entry->title,
		__entry->name,
		__entry->value
	)
);

DEFINE_EVENT(mark_begin_end, mark_begin_end,

	TP_PROTO(char event_type,
		const char *category,
		const char *title,
		const char *name,
		unsigned long value,
		char ignore_pid),

	TP_ARGS(event_type, category, title, name, value, ignore_pid)
);

DECLARE_EVENT_CLASS(mark_start_finish,

	TP_PROTO(char event_type,
		const char *category,
		const char *title,
		unsigned long cookie,
		const char *name,
		unsigned long value,
		char ignore_pid),

	TP_ARGS(event_type, category, title, cookie, name, value, ignore_pid),

	TP_STRUCT__entry(
		__field(char, event_type)
		__field(unsigned long, pid)
		__field(const char*, category)
		__field(const char*, title)
		__field(unsigned long, cookie)
		__field(const char*, name)
		__field(unsigned long, value)
	),

	TP_fast_assign(
		__entry->event_type = event_type;
		__entry->pid = (ignore_pid) ? 0:current->tgid;
		__entry->category = category;
		__entry->title = title;
		__entry->cookie = cookie;
		__entry->name = name;
		__entry->value = value;
	),

	TP_printk("%c|%lu|%s:%s|%lu|%s=%lu;",
		__entry->event_type,
		__entry->pid,
		__entry->category,
		__entry->title,
		__entry->cookie,
		__entry->name,
		__entry->value
	)
);

DEFINE_EVENT(mark_start_finish, mark_start_finish,

	TP_PROTO(char event_type,
		const char *category,
		const char *title,
		unsigned long cookie,
		const char *name,
		unsigned long value,
		char ignore_pid),

	TP_ARGS(event_type, category, title, cookie, name, value, ignore_pid)
);

DECLARE_EVENT_CLASS(mark_count,

	TP_PROTO(char event_type,
		const char *category,
		const char *name,
		unsigned long value,
		unsigned long ref_01,
		unsigned long ref_02,
		unsigned long ref_03,
		char ignore_pid),

	TP_ARGS(event_type, category, name, value, ref_01, ref_02, ref_03, ignore_pid),

	TP_STRUCT__entry(
		__field(char, event_type)
		__field(unsigned long, pid)
		__field(const char*, category)
		__field(const char*, name)
		__field(unsigned long, value)
		__field(unsigned long, ref_01)
		__field(unsigned long, ref_02)
		__field(unsigned long, ref_03)
	),

	TP_fast_assign(
		__entry->event_type = 'C';
		__entry->pid = (ignore_pid) ? 0:current->tgid;
		__entry->category = category;
		__entry->name = name;
		__entry->value = value;
		__entry->ref_01 = ref_01;
		__entry->ref_02 = ref_02;
		__entry->ref_03 = ref_03;
	),

	TP_printk("%c|%lu|%s:%s|%lu|%lu|%lu|%lu",
		__entry->event_type,
		__entry->pid,
		__entry->category,
		__entry->name,
		__entry->value,
		__entry->ref_01,
		__entry->ref_02,
		__entry->ref_03
	)
);

DEFINE_EVENT(mark_count, mark_count,

	TP_PROTO(char event_type,
		const char *category,
		const char *name,
		unsigned long value,
		unsigned long ref_01,
		unsigned long ref_02,
		unsigned long ref_03,
		char ignore_pid),

	TP_ARGS(event_type, category, name, value, ref_01, ref_02, ref_03, ignore_pid)
);

#endif /* _TRACE_MARKER_H */

/* This part must be outside protection */
#include <trace/define_trace.h>




/*
 * Usage
 *

#define CREATE_TRACE_POINTS
#include <trace/events/marker.h>

trace_mark_begin_end('B', "category", "title", "arg_name", arg_value, 0);
trace_mark_begin_end('E', "category", "title", "arg_name", arg_value, 0);

trace_mark_start_finish('S', "category", "title", cookie, "arg_name", arg_value, 0);
trace_mark_start_finish('F', "category", "title", cookie, "arg_name", arg_value, 0);

trace_mark_count('C', "category", "name", value, ref_01, ref_02, ref_03, 1);

 */
