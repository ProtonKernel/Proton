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
#include <linux/hashtable.h>
#include "sec_debug_internal.h"
#include <soc/samsung/debug-snapshot.h>
#include <soc/samsung/debug-snapshot-log.h>
#include <linux/sched/prio.h>
#include "../../../kernel/sched/sched.h"
#include "../../soc/samsung/debug/debug-snapshot-local.h"

#define MAX_PC 10
#define MAX_CB 5
#define TRACK_ADDRS_COUNT 16
#define PRINT_LINE_MAX	512

#define STORM_THREASH (100*NSEC_PER_USEC)
#define LOCKUP_THREAH (5*NSEC_PER_SEC)
#define BUSY_TRHEASH (20*NSEC_PER_SEC)
#define LATEST_IRQ_THREASH NSEC_PER_SEC

#define BUSY_IRQ_SET_HASH_BITS 4

#define init_vars(item, domain, start, len, max)				\
do {										\
	struct item##_log *__log;							\
	start = dss_get_first_##item##_log_idx(domain);				\
	__log = dss_get_##item##_log_by_idx(domain, start);			\
	if (start == 0 && __log->time == 0) {					\
		len = max = 0;							\
	} else if (__log->time == 0) {						\
		start = 0;							\
		len = dss_get_last_##item##_log_idx(domain) + 1;		\
		max = dss_get_len_##item##_log_by_cpu(domain);			\
	} else {								\
		max = len = dss_get_len_##item##_log_by_cpu(domain);		\
	}									\
} while (0)

#define print_sched_report(buf) (enable_auto_comment ? pr_auto(ASL3, "%s\n", buf) : pr_info("%s\n", buf))

#define is_valid_task(data) (data->pid == data->tsk->pid \
		&& !strcmp(data->task_comm, data->tsk->comm) \
		&& data->tsk->stack != 0)

enum sched_issue_type {
	SCHED_ISSUE_NONE,
	SCHED_ISSUE_IDLE,
	SCHED_ISSUE_HOTPLUGOUT,
	SCHED_ISSUE_ERROR,
	SCHED_ISSUE_LOCKUP,
	SCHED_ISSUE_BUSY
};

enum lockup_type {
	HL_TASK_STUCK = 1,
	HL_IRQ_STUCK,
	HL_IDLE_STUCK,
	HL_SMC_CALL_STUCK,
	HL_IRQ_STORM,
	HL_HRTIMER_ERROR,
	HL_UNKNOWN_STUCK
};

struct task_info {
	char task_comm[TASK_COMM_LEN];
	char group_leader[TASK_COMM_LEN];
};

struct cpuidle_info {
	char *mode;
};

struct smc_info {
	int cmd;
};

struct irq_info {
	int irq;
	void *fn;
	unsigned long long avg_period;
};

struct busy_irq {
	int irq;
	unsigned int occurrences;
	void *fn;
	unsigned long long total_duration;
	unsigned long long last_time;
	struct hlist_node hlist;
};

struct lockup_info {
	enum lockup_type lockup_type;
	unsigned long long time;
	unsigned long long delay_time;
	union {
		struct task_info task_info;
		struct cpuidle_info cpuidle_info;
		struct smc_info smc_info;
		struct irq_info irq_info;
	};
};

struct busy_data {
	struct list_head node;
	unsigned long long residency;
	struct task_struct *tsk;
	char task_comm[TASK_COMM_LEN];
	pid_t pid;
};

struct busy_info {
	unsigned long long duration;
	struct list_head busy_info_list;
};

struct sched_report {
	enum sched_issue_type issue_type;

	unsigned long pc[MAX_PC];
	unsigned long addrs[MAX_CB][TRACK_ADDRS_COUNT];

	union {
		struct lockup_info lockup_info;
		struct busy_info busy_info;
	};
};

static const char * const panic_string[] = {
	"RCU Stall",
	"Software Watchdog Timer expired",
	"Crash Key"
};

static bool enable_auto_comment;

static const char * const lockup_to_name[] = {
	"NONE", "TASK LOCKUP", "IRQ LOCKUP",
	"IDLE LOCKUP", "SMCCALL LOCKUP", "IRQ STORM",
	"HRTIMER ERROR", "UNKNOWN LOCKUP"
};

static DEFINE_PER_CPU(struct sched_report, sched_report);

static DEFINE_HASHTABLE(busy_irq_hash, BUSY_IRQ_SET_HASH_BITS);

static void __enable_auto_comment(void *buf)
{
	int i;

	for (i = 0; i < ARRAY_SIZE(panic_string); i++) {
		if (strstr(buf, panic_string[i])) {
			enable_auto_comment = true;
			return;
		}
	}
}

static int secdbg_get_busiest_irq(struct lockup_info *info, int cpu, unsigned long long curr_time)
{
	long start, len, max;
	struct irq_log *irq;
	struct busy_irq *b_irq;
	struct busy_irq *busiest_irq = NULL;
	int i, alloc;
	struct hlist_node *tmp;
	unsigned long long avg_period;

	hash_init(busy_irq_hash);

	init_vars(irq, cpu, start, len, max);

	for_each_item_in_dss_by_cpu(irq, cpu, start, len, true) {
		if (!irq || irq->time == 0)
			break;

		if (irq->en == DSS_FLAG_OUT)
			continue;

		alloc = 1;

		hash_for_each_possible(busy_irq_hash, b_irq, hlist, irq->irq) {
			if (b_irq->irq == irq->irq) {
				b_irq->total_duration += (irq->time - b_irq->last_time);
				b_irq->last_time = irq->time;
				b_irq->occurrences++;
				alloc = 0;
				break;
			}
		}

		if (alloc) {
			b_irq = kzalloc(sizeof(*b_irq), GFP_ATOMIC);

			if (!b_irq)
				return -ENOMEM;

			b_irq->irq = irq->irq;
			b_irq->fn = irq->fn;
			b_irq->occurrences = 0;
			b_irq->total_duration = 0;
			b_irq->last_time = irq->time;
			hash_add(busy_irq_hash, &b_irq->hlist, irq->irq);
		}
	}

	hash_for_each(busy_irq_hash, i, b_irq, hlist) {
		if (!busiest_irq)
			busiest_irq = b_irq;
		else if (busiest_irq->occurrences < b_irq->occurrences)
			busiest_irq = b_irq;
	}

	avg_period = (busiest_irq->occurrences == 0) ?
		0 : busiest_irq->total_duration / busiest_irq->occurrences;

	if (avg_period > STORM_THREASH ||
		curr_time - busiest_irq->last_time > LATEST_IRQ_THREASH)
		return 0;

	info->irq_info.irq = busiest_irq->irq;
	info->irq_info.fn = busiest_irq->fn;
	info->irq_info.avg_period = avg_period;

	hash_for_each_safe(busy_irq_hash, i, tmp, b_irq, hlist) {
		hash_del(&b_irq->hlist);
		kfree(b_irq);
	}

	return 1;
}

static enum sched_issue_type secdbg_sched_report_check_lockup(unsigned int cpu, struct sched_report *sr)
{
	unsigned long long curr_time;
	unsigned long long thresh = LOCKUP_THREAH;
	unsigned long long cpuidle_delay_time, irq_delay_time, task_delay_time;
	struct cpuidle_log *last_cpuidle;
	struct irq_log *last_irq;
	struct task_log *last_task;
	struct lockup_info *li = &sr->lockup_info;

	curr_time = local_clock();
	last_cpuidle = dss_get_last_cpuidle_log(cpu);

	if (last_cpuidle) {
		cpuidle_delay_time = (curr_time > last_cpuidle->time) ? curr_time - last_cpuidle->time : 0;

		if (last_cpuidle->en == DSS_FLAG_IN &&
			cpuidle_delay_time > thresh) {
			li->time = last_cpuidle->time;
			li->delay_time = cpuidle_delay_time;
			li->cpuidle_info.mode = last_cpuidle->modes;
			li->lockup_type = HL_IDLE_STUCK;
			return SCHED_ISSUE_LOCKUP;
		}
	}

	last_irq = dss_get_last_irq_log(cpu);

	if (!last_irq)
		return SCHED_ISSUE_ERROR;

	irq_delay_time = (curr_time > last_irq->time) ? curr_time - last_irq->time : 0;

	if (last_irq->en == DSS_FLAG_IN &&
		irq_delay_time > thresh) {
		li->time = last_irq->time;
		li->delay_time = irq_delay_time;
		li->irq_info.irq = last_irq->irq;
		li->irq_info.fn = last_irq->fn;
		li->lockup_type = HL_IRQ_STUCK;

		return SCHED_ISSUE_LOCKUP;
	}

	last_task = dss_get_last_task_log(cpu);

	if (!last_task)
		return SCHED_ISSUE_ERROR;

	task_delay_time = (curr_time > last_task->time) ? curr_time - last_task->time : 0;

	if (last_task->time < curr_time &&
		task_delay_time > thresh) {
		li->time = last_task->time;
		li->delay_time = task_delay_time;

		if (irq_delay_time > thresh || secdbg_get_busiest_irq(li, cpu, curr_time) <= 0) {

			if (irq_delay_time > thresh && cpu_rq(cpu)->stop == last_task->task)
				return SCHED_ISSUE_HOTPLUGOUT;
			else if (irq_delay_time <= thresh && last_task->pid == 0)
				return SCHED_ISSUE_IDLE;

			strncpy(li->task_info.task_comm,
				last_task->task_comm,
				TASK_COMM_LEN - 1);
			li->task_info.task_comm[TASK_COMM_LEN - 1] = '\0';
			strncpy(li->task_info.group_leader,
				last_task->task->group_leader->comm,
				TASK_COMM_LEN - 1);
			li->task_info.group_leader[TASK_COMM_LEN - 1] = '\0';
			li->lockup_type = HL_TASK_STUCK;
		} else {
			li->lockup_type = HL_IRQ_STORM;
		}
		return SCHED_ISSUE_LOCKUP;
	}
	return SCHED_ISSUE_NONE;
}


static struct list_head *__create_busy_info(struct task_log *task, unsigned long long residency)
{
	struct busy_data *data;

	data = kzalloc(sizeof(struct busy_data), GFP_ATOMIC);
	if (!data)
		return NULL;

	data->pid = task->pid;
	data->tsk = task->task;
	strncpy(data->task_comm, task->task->comm, TASK_COMM_LEN - 1);
	data->residency = residency;

	return &data->node;
}

static int __residency_cmp(void *priv, struct list_head *a, struct list_head *b)
{
	struct busy_data *busy_data_a;
	struct busy_data *busy_data_b;

	busy_data_a = container_of(a, struct busy_data, node);
	busy_data_b = container_of(b, struct busy_data, node);

	if (busy_data_a->residency < busy_data_b->residency)
		return 1;
	else if (busy_data_a->residency > busy_data_b->residency)
		return -1;
	else
		return 0;
}

static bool __add_task_in_busy_info(struct task_log *task, unsigned long long residency, struct busy_info *bi)
{
	struct list_head *entry;
	struct busy_data *data;

	bi->duration += residency;

	list_for_each_entry(data, &bi->busy_info_list, node) {
		if (data->pid == task->pid && !strcmp(data->task_comm, task->task_comm)) {
			data->residency += residency;
			return true;
		}
	}

	entry = __create_busy_info(task, residency);

	if (!entry)
		return false;

	list_add(entry, &bi->busy_info_list);
	return true;
}

static enum sched_issue_type secdbg_sched_report_check_busy(unsigned int cpu, struct sched_report *sr)
{
	long len = dss_get_len_task_log_by_cpu(cpu) - 1;
	long start = dss_get_last_task_log_idx(cpu);
	unsigned long long limit_time = local_clock() - BUSY_TRHEASH;
	struct task_log *task, *next_task;
	enum sched_issue_type ret = SCHED_ISSUE_NONE;
	struct busy_info *bi = &sr->busy_info;

	bi->duration = 0;
	INIT_LIST_HEAD(&bi->busy_info_list);

	task = dss_get_task_log_by_idx(cpu, start);

	if (task && task->time != 0 && task->pid != 0 &&
		__add_task_in_busy_info(task, local_clock() - task->time, bi)) {

		ret = SCHED_ISSUE_BUSY;

		next_task = task;
		start = start > 0 ? (start - 1) : len;

		for_each_item_in_dss_by_cpu(task, cpu, start, len, false) {
			if (task->time != 0 && task->time <= next_task->time &&
				task->pid != 0 && __add_task_in_busy_info(task, next_task->time - task->time, bi)) {
				next_task = task;
			} else {
				ret = next_task->time < limit_time ? SCHED_ISSUE_BUSY : SCHED_ISSUE_NONE;
				break;
			}
		}
	}

	list_sort(NULL, &bi->busy_info_list, __residency_cmp);

	return ret;
}


void __show_lockup_info(unsigned int cpu, struct sched_report *sr)
{
	struct lockup_info *info = &sr->lockup_info;
	char buf[PRINT_LINE_MAX];
	ssize_t offset = 0;

	offset += scnprintf(buf + offset, PRINT_LINE_MAX - offset, "CPU%u %s for %lldsec[",
		 cpu, lockup_to_name[info->lockup_type], info->delay_time / NSEC_PER_SEC);

	switch (info->lockup_type) {
	case HL_TASK_STUCK:
		offset += scnprintf(buf + offset, PRINT_LINE_MAX - offset, " task=%s[%s]]",
			info->task_info.task_comm, info->task_info.group_leader);
		break;
	case HL_IRQ_STUCK:
		offset += scnprintf(buf + offset, PRINT_LINE_MAX - offset, " irq=%d, func=%ps]",
			info->irq_info.irq, info->irq_info.fn);
		break;
	case HL_IDLE_STUCK:
		offset += scnprintf(buf + offset, PRINT_LINE_MAX - offset, " mode=%s]",
			info->cpuidle_info.mode);
		break;
	case HL_SMC_CALL_STUCK:
		offset += scnprintf(buf + offset, PRINT_LINE_MAX - offset, " cmd=%u]",
			info->smc_info.cmd);
		break;
	case HL_IRQ_STORM:
		offset += scnprintf(buf + offset, PRINT_LINE_MAX - offset, " irq=%d, func=%ps, avg_period=%lluns]",
			info->irq_info.irq, info->irq_info.fn, info->irq_info.avg_period);
		break;
	default:
		offset += scnprintf(buf + offset, PRINT_LINE_MAX - offset, "UNKNOWN]");
		break;
	}

	print_sched_report(buf);
}


static void __show_sched_info(unsigned int cpu, long count)
{
	long task_idx;
	ssize_t offset = 0;
	long max_count = dss_get_len_task_log_by_cpu(0);
	char buf[PRINT_LINE_MAX];
	struct dbg_snapshot_log_item *log_item = dbg_snapshot_log_get_item_by_index(DSS_LOG_TASK_ID);
	struct task_log *task;

	if (!log_item->entry.enabled)
		return;

	if (cpu < 0 || cpu >= DSS_NR_CPUS) {
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

	print_sched_report(buf);
}

static void __show_busy_info(unsigned int cpu, struct sched_report *sr)
{
	char buf[PRINT_LINE_MAX];
	ssize_t offset = 0;
	struct busy_info *bi = &sr->busy_info;
	int count = 5;
	struct busy_data *data;

	offset += scnprintf(buf + offset, PRINT_LINE_MAX - offset, "CPU%d busy for %ums",
		cpu, (unsigned int)(bi->duration / NSEC_PER_MSEC));

	list_for_each_entry(data, &bi->busy_info_list, node) {
		if (is_valid_task(data)) {
			offset += scnprintf(buf + offset, PRINT_LINE_MAX - offset,
				" %s:%d[%u%%,%d,%s,%llu]", data->task_comm, data->pid,
				bi->duration > 0 ? (unsigned int)((data->residency * 100) / bi->duration) : 0,
				data->tsk->prio,
#ifdef CONFIG_FAIR_GROUP_SCHED
				data->tsk->se.cfs_rq->tg->css.cgroup->kn->name,
#else
				"",
#endif
				data->tsk->prio < MAX_RT_PRIO ? 0 : data->tsk->se.vruntime);
		} else {
			offset += scnprintf(buf + offset, PRINT_LINE_MAX - offset,
				" %s:%d[%u%%]", data->task_comm, data->pid,
				bi->duration > 0 ? (unsigned int)((data->residency * 100) / bi->duration) : 0);
		}

		if (--count == 0)
			break;
	}

	print_sched_report(buf);
}

static void __show_rq_stat(unsigned int cpu)
{
	char buf[PRINT_LINE_MAX];
	ssize_t offset = 0;

	offset += scnprintf(buf + offset, PRINT_LINE_MAX - offset, "CPU%u nr_running:%u, CFS load_avg:%lu",
		cpu, cpu_rq(cpu)->nr_running, cpu_rq(cpu)->cfs.avg.load_avg);

	print_sched_report(buf);
}

static void secdbg_sched_report_show_info(unsigned int cpu, struct sched_report *sr)
{
	if (sr->issue_type == SCHED_ISSUE_LOCKUP)
		__show_lockup_info(cpu, sr);
	else if (sr->issue_type == SCHED_ISSUE_BUSY) {
		__show_busy_info(cpu, sr);
		__show_sched_info(cpu, 10);
		__show_rq_stat(cpu);
	}
}

static int secdbg_sched_report_handler(struct notifier_block *nb,
				   unsigned long l, void *buf)
{
	unsigned int cpu;

	__enable_auto_comment(buf);

	for_each_possible_cpu(cpu) {
		struct sched_report *sr = per_cpu_ptr(&sched_report, cpu);

		sr->issue_type = secdbg_sched_report_check_lockup(cpu, sr);
		if (sr->issue_type == SCHED_ISSUE_NONE)
			sr->issue_type = secdbg_sched_report_check_busy(cpu, sr);
	}

	for_each_possible_cpu(cpu) {
		struct sched_report *sr = per_cpu_ptr(&sched_report, cpu);

		secdbg_sched_report_show_info(cpu, sr);
	}

	return NOTIFY_DONE;
}

static struct notifier_block nb_panic_block = {
	.notifier_call = secdbg_sched_report_handler,
};

static int __init secdbg_sched_report_init(void)
{
	pr_info("%s: init\n", __func__);

	atomic_notifier_chain_register(&panic_notifier_list, &nb_panic_block);

	return 0;
}
module_init(secdbg_sched_report_init);

static void __exit secdbg_sched_report_exit(void)
{
}
module_exit(secdbg_sched_report_exit);

MODULE_DESCRIPTION("Samsung Debug Sched report driver");
MODULE_LICENSE("GPL v2");
