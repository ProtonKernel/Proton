// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright (c) 2020 Samsung Electronics Co., Ltd.
 *      http://www.samsung.com
 *
 * Samsung debugging code
 */

#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/slab.h>
#include <linux/list_sort.h>
#include <linux/sec_debug.h>
#include <soc/samsung/debug-snapshot.h>
#include <soc/samsung/debug-snapshot-log.h>

#include "../../../kernel/sched/sched.h"
#include "../../soc/samsung/debug/debug-snapshot-local.h"
#include "sec_debug_internal.h"

#include <trace/hooks/wqlockup.h>

#define PRINT_LINE_MAX	512

static void secdbg_scin_show_sched_info(unsigned int cpu, int count)
{
	unsigned long long task_idx;
	ssize_t offset = 0;
	int max_count = dss_get_len_task_log_by_cpu(0);
	char buf[PRINT_LINE_MAX];
	struct dbg_snapshot_log_item *log_item = dbg_snapshot_log_get_item_by_index(DSS_LOG_TASK_ID);
	struct task_log *task;

	if (!log_item->entry.enabled)
		return;

	if (cpu >= DSS_NR_CPUS) {
		pr_warn("%s: invalid cpu %d\n", __func__, cpu);
		return;
	}

	if (count > max_count)
		count = max_count;

	offset += scnprintf(buf + offset, PRINT_LINE_MAX - offset, "Sched info ");
	task_idx = dss_get_last_task_log_idx(cpu);

	for_each_item_in_dss_by_cpu(task, cpu, task_idx, count, false) {
		if (task->time == 0)
			break;
		offset += scnprintf(buf + offset, PRINT_LINE_MAX - offset, "[%d]<", task->pid);
	}

	pr_auto(ASL5, "%s\n", buf);
}

struct busy_info {
	struct list_head node;
	unsigned long long residency;
	struct task_struct *tsk;
	pid_t pid;
};

static LIST_HEAD(busy_info_list);
static int is_busy;
static bool is_busy_info_list;
static unsigned long long real_duration;

static struct list_head *__create_busy_info(struct task_struct *tsk, unsigned long long residency)
{
	struct busy_info *info;

	info = kzalloc(sizeof(struct busy_info), GFP_ATOMIC);
	if (!info)
		return NULL;

	info->pid = tsk->pid;
	if (info->pid == 0)
		is_busy = 0;

	info->tsk = tsk;
	info->residency = residency;

	return &info->node;
}

static int __residency_cmp(void *priv, struct list_head *a, struct list_head *b)
{
	struct busy_info *busy_info_a;
	struct busy_info *busy_info_b;

	busy_info_a = container_of(a, struct busy_info, node);
	busy_info_b = container_of(b, struct busy_info, node);

	if (busy_info_a->residency < busy_info_b->residency)
		return 1;
	else if (busy_info_a->residency > busy_info_b->residency)
		return -1;
	else
		return 0;
}

static void show_callstack(void *info)
{
	secdbg_stra_show_callstack_auto(NULL);
}

static unsigned long long secdbg_scin_make_busy_task_list(unsigned int cpu, unsigned long long duration)
{
	unsigned long long residency;
	unsigned long long limit_time = local_clock() - duration * NSEC_PER_SEC;
	struct list_head *entry;
	int count = dss_get_len_task_log_by_cpu(0) - 1;
	unsigned long task_idx = dss_get_last_task_log_idx(cpu);
	struct busy_info *info;
	struct task_log *tl;
	struct task_log *tl_next;
	struct task_log *task;

	is_busy_info_list = true;
	is_busy = 1;

	tl = dss_get_task_log_by_idx(cpu, task_idx);
	if (!tl || tl->time == 0)
		return 0;

	residency = local_clock() - tl->time;
	real_duration += residency;
	entry = __create_busy_info(tl->task, residency);
	if (!entry)
		return 0;

	list_add(entry, &busy_info_list);

	tl_next = tl;
	task_idx = task_idx > 0 ? (task_idx - 1) : count;

	for_each_item_in_dss_by_cpu(task, cpu, task_idx, count, false) {
		if (task->time == 0 ||
			(task->time > tl_next->time) ||
			(task->time < limit_time)) {
			break;
		}

		residency = tl_next->time - task->time;
		real_duration += residency;

		list_for_each_entry(info, &busy_info_list, node) {
			if (info->pid == task->pid) {
				info->residency += residency;
				goto next;
			}
		}

		entry = __create_busy_info(task->task, residency);
		if (!entry)
			return real_duration - residency;

		list_add(entry, &busy_info_list);
next:
		tl_next = task;
	}

	return real_duration;
}

enum sched_class_type {
	SECDBG_SCHED_IDLE,
	SECDBG_SCHED_FAIR,
	SECDBG_SCHED_RT,
	SECDBG_SCHED_DL,
	SECDBG_SCHED_STOP,
	SECDBG_SCHED_NONE
};

static enum sched_class_type get_sched_class(struct task_struct *tsk)
{
	const struct sched_class *class = tsk->sched_class;
	int type = SECDBG_SCHED_IDLE;

	while (class->next) {
		if (type == SECDBG_SCHED_NONE)
			break;

		class = class->next;
		type++;
	}

	return type;
}

static int secdbg_scin_show_busy_task(unsigned int cpu, unsigned long long duration, int count)
{
	unsigned long long real_duration;
	struct busy_info *info;
	struct task_struct *main_busy_tsk;
	ssize_t offset = 0;
	char buf[PRINT_LINE_MAX];
	struct dbg_snapshot_log_item *log_item = dbg_snapshot_log_get_item_by_index(DSS_LOG_TASK_ID);
	char sched_class_array[] = {'I', 'F', 'R', 'D', 'S', '0'};

	if (is_busy_info_list)
		return -1;

	if (cpu >= DSS_NR_CPUS) {
		pr_warn("%s: invalid cpu %d\n", __func__, cpu);
		return -EINVAL;
	}

	/* This needs runqueues with EXPORT_SYMBOL */
	pr_auto(ASL5, "CPU%u [CFS util_avg:%lu load_avg:%lu nr_running:%u][RT rt_nr_running:%u][avg_rt util_avg:%lu]\n",
		cpu, cpu_rq(cpu)->cfs.avg.util_avg, cpu_rq(cpu)->cfs.avg.load_avg, cpu_rq(cpu)->cfs.nr_running,
		cpu_rq(cpu)->rt.rt_nr_running, cpu_rq(cpu)->avg_rt.util_avg);

	if (!log_item->entry.enabled)
		return -1;

	real_duration = secdbg_scin_make_busy_task_list(cpu, duration);
	list_sort(NULL, &busy_info_list, __residency_cmp);

	if (list_empty(&busy_info_list))
		return -1;

	offset += scnprintf(buf + offset, PRINT_LINE_MAX - offset, "CPU Usage: PERIOD(%us)", (unsigned int)(real_duration / NSEC_PER_SEC));

	list_for_each_entry(info, &busy_info_list, node) {
		offset += scnprintf(buf + offset, PRINT_LINE_MAX - offset,
			" %s:%d[%c,%d](%u%%)", info->tsk->comm, info->tsk->pid,
			sched_class_array[get_sched_class(info->tsk)],
			info->tsk->prio, real_duration > 0 ? (unsigned int)((info->residency * 100) / real_duration) : 0);
		if (--count == 0)
			break;
	}

	pr_auto(ASL5, "%s\n", buf);

	info = list_first_entry(&busy_info_list, struct busy_info, node);
	main_busy_tsk = info->tsk;

	if (!is_busy) {
		pr_auto(ASL5, "CPU%u is not busy.", cpu);
	} else if (main_busy_tsk->on_cpu && (main_busy_tsk->cpu == cpu)) {
		smp_call_function_single(cpu, show_callstack, NULL, 1);
	} else {
		secdbg_stra_show_callstack_auto(main_busy_tsk);
#if IS_ENABLED(CONFIG_SEC_DEBUG_SHOW_USER_STACK)
		secdbg_send_sig_debuggerd(main_busy_tsk, 2);
#endif
	}

	return is_busy;
}

static void secdbg_wqlockup_info_handler(void *ignore, int cpu, unsigned long pool_ts)
{
	pr_info("%s\n", __func__);

	if (cpu < 0)
		return;

	secdbg_scin_show_sched_info(cpu, 10);
	secdbg_scin_show_busy_task(cpu, jiffies_to_msecs(jiffies - pool_ts) / 1000, 5);
}

static int __init secdbg_scin_init(void)
{
	pr_info("%s: init\n", __func__);

	if (IS_ENABLED(CONFIG_SEC_DEBUG_WQ_LOCKUP_INFO))
		register_trace_android_vh_wq_lockup_pool(secdbg_wqlockup_info_handler, NULL);

	return 0;
}
module_init(secdbg_scin_init);

static void __exit secdbg_scin_exit(void)
{
	if (IS_ENABLED(CONFIG_SEC_DEBUG_WQ_LOCKUP_INFO))
		unregister_trace_android_vh_wq_lockup_pool(secdbg_wqlockup_info_handler, NULL);
}
module_exit(secdbg_scin_exit);

MODULE_DESCRIPTION("Samsung Debug Sched info driver");
MODULE_LICENSE("GPL v2");
