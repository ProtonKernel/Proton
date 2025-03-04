// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright (c) 2020 Samsung Electronics Co., Ltd.
 *      http://www.samsung.com
 *
 * Samsung debugging code
 */

#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/stacktrace.h>
#include <linux/sec_debug.h>

#define MAX_CALL_ENTRY	128

DEFINE_STATIC_PR_AUTO_NAME_ONCE(callstack, ASL2);

static void __print_stack_trace(struct stack_trace *trace)
{
	unsigned int i;
	const int spaces = 0;

	if (!trace->nr_entries)
		return;

	pr_auto_name_once(callstack);
	pr_auto_name(callstack, "Call trace:\n");

	for (i = 0; i < trace->nr_entries; i++)
		pr_auto_name(callstack, "%*c%pS\n", 1 + spaces, ' ', (void *)trace->entries[i]);
}

static void __show_callstack(struct task_struct *tsk)
{
	unsigned long entry[MAX_CALL_ENTRY];
	struct stack_trace trace = {0,};

	trace.max_entries = MAX_CALL_ENTRY;
	trace.entries = entry;

	if (!tsk) {
		save_stack_trace(&trace);
	} else {
		trace.skip = 1;	/* skipping __switch_to */
		save_stack_trace_tsk(tsk, &trace);
	}

	if (!trace.nr_entries) {
		if (!tsk)
			pr_err("no trace for current\n");
		else
			pr_err("no trace for [%s :%d]\n", tsk->comm, tsk->pid);
		return;
	}

	__print_stack_trace(&trace);
}

void secdbg_stra_show_callstack_auto(struct task_struct *tsk)
{
	__show_callstack(tsk);
}
EXPORT_SYMBOL(secdbg_stra_show_callstack_auto);

static int __init secdbg_stra_init(void)
{
	pr_info("%s: init\n", __func__);

	return 0;
}
module_init(secdbg_stra_init);

static void __exit secdbg_stra_exit(void)
{
}
module_exit(secdbg_stra_exit);

MODULE_DESCRIPTION("Samsung Debug Stacktrace driver");
MODULE_LICENSE("GPL v2");
