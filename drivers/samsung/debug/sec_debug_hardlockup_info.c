// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright (c) 2014-2020 Samsung Electronics Co., Ltd.
 *      http://www.samsung.com
 *
 * sec_debug_hardlockup_info.c
 */
#include <linux/slab.h>
#include <linux/module.h>
#include <linux/sched/clock.h>
#include "sec_debug_internal.h"
#include <soc/samsung/debug-snapshot-log.h>
#include <soc/samsung/exynos-ehld.h>
#include <linux/hashtable.h>
#include <soc/samsung/exynos-ehld.h>
#include <linux/kallsyms.h>
#include <linux/irq.h>
#include <linux/irqdesc.h>
#include "../../pinctrl/samsung/pinctrl-samsung.h"
#include "../../pinctrl/samsung/pinctrl-exynos.h"

#define PRINT_LINE_MAX	512
#define TASK_COMM_LEN 16
#define BUSY_IRQ_SET_HASH_BITS 4
#define MAX_PR_AUTO 6
#define GPIO_NAME_LEN 10

#define init_vars(item, domain, start, len, max)				\
do {										\
	struct item##_log *item;						\
	start = dss_get_first_##item##_log_idx(domain);				\
	item = dss_get_##item##_log_by_idx(domain, start);			\
	if (start == 0 && item->time == 0) {					\
		len = max = 0;							\
	} else if (item->time == 0) {						\
		start = 0;							\
		len = dss_get_last_##item##_log_idx(domain) + 1;		\
		max = dss_get_len_##item##_log_by_cpu(domain);			\
	} else {								\
		max = len = dss_get_len_##item##_log_by_cpu(domain);		\
	}									\
} while (0)

enum hardlockup_type {
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

struct hardlockup_info {
	enum hardlockup_type hl_type;
	unsigned long long time;
	unsigned long long delay_time;
	union {
		struct task_info task_info;
		struct cpuidle_info cpuidle_info;
		struct smc_info smc_info;
		struct irq_info irq_info;
	};
	unsigned int ehld_type;
};

struct busy_irq {
	int irq;
	unsigned int occurrences;
	void *fn;
	unsigned long long total_duration;
	unsigned long long last_time;
	struct hlist_node hlist;
};

static DEFINE_PER_CPU(struct hardlockup_info, percpu_hl_info);
static DEFINE_HASHTABLE(busy_irq_hash, BUSY_IRQ_SET_HASH_BITS);
static const char * const hl_to_name[] = {
	"NONE", "TASK STUCK", "IRQ STUCK",
	"IDLE STUCK", "SMCCALL STUCK", "IRQ STORM",
	"HRTIMER ERROR", "UNKNOWN STUCK"
};
static char dss_freq_name[DSS_DOMAIN_NUM][SZ_8] = {
	"LIT", "MID", "BIG", "INT", "MIF", "CAM",
	"DISP", "INTCAM", "AUD", "DSP", "DNC", "MFC",
	"NPU", "TNR", "DSU", "VPC", "CSIS", "ISP",
	"MFC1", "G3D"
};

extern struct atomic_notifier_head hardlockup_notifier_list;
extern unsigned long hardlockup_watchdog_get_thresh(void);
extern u64 hardlockup_watchdog_get_period(void);

static void secdbg_get_busiest_irq(struct hardlockup_info *hl_info, int cpu)
{
	int start, len, max;
	struct irq_log *irq;
	struct busy_irq *b_irq;
	struct busy_irq *busiest_irq = NULL;
	int i, alloc;

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
				break;

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

	hl_info->irq_info.irq = busiest_irq->irq;
	hl_info->irq_info.fn = busiest_irq->fn;
	hl_info->irq_info.avg_period = (busiest_irq->occurrences == 0) ?
		0 : busiest_irq->total_duration / busiest_irq->occurrences;
}

static void secdbg_hardlockup_get_info(unsigned int cpu, struct hardlockup_info *hl_info)
{
	unsigned long long curr_time;
	unsigned long long thresh = hardlockup_watchdog_get_thresh() * NSEC_PER_SEC - hardlockup_watchdog_get_period();
	unsigned long long cpuidle_delay_time, irq_delay_time, task_delay_time;
	struct cpuidle_log *last_cpuidle;
	struct irq_log *last_irq;
	struct task_log *last_task;

	curr_time = local_clock();
	last_cpuidle = dss_get_last_cpuidle_log(cpu);
	cpuidle_delay_time = (curr_time > last_cpuidle->time) ? curr_time - last_cpuidle->time : 0;

	if (last_cpuidle->en == DSS_FLAG_IN &&
		cpuidle_delay_time > thresh) {
		hl_info->time = last_cpuidle->time;
		hl_info->delay_time = cpuidle_delay_time;
		hl_info->cpuidle_info.mode = last_cpuidle->modes;
		hl_info->hl_type = HL_IDLE_STUCK;
		return;
	}

	last_irq = dss_get_last_irq_log(cpu);
	irq_delay_time = (curr_time > last_irq->time) ? curr_time - last_irq->time : 0;

	if (last_irq->en == DSS_FLAG_IN &&
		irq_delay_time > thresh) {
		hl_info->time = last_irq->time;
		hl_info->delay_time = irq_delay_time;
		hl_info->irq_info.irq = last_irq->irq;
		hl_info->irq_info.fn = last_irq->fn;
		hl_info->hl_type = HL_IRQ_STUCK;
		return;
	}

	last_task = dss_get_last_task_log(cpu);
	task_delay_time = (curr_time > last_task->time) ? curr_time - last_task->time : 0;

	if (last_task->time < curr_time &&
		task_delay_time > thresh) {
		hl_info->time = last_task->time;
		hl_info->delay_time = task_delay_time;

		if (irq_delay_time > thresh) {
			strncpy(hl_info->task_info.task_comm,
				last_task->task_comm,
				TASK_COMM_LEN - 1);
			hl_info->task_info.task_comm[TASK_COMM_LEN - 1] = '\0';
			strncpy(hl_info->task_info.group_leader,
				last_task->task->group_leader->comm,
				TASK_COMM_LEN - 1);
			hl_info->task_info.group_leader[TASK_COMM_LEN - 1] = '\0';
			hl_info->hl_type = HL_TASK_STUCK;
		} else {
			secdbg_get_busiest_irq(hl_info, cpu);
			hl_info->hl_type = HL_IRQ_STORM;
		}
		return;
	}

	hl_info->hl_type = HL_UNKNOWN_STUCK;
}

static void secdbg_hardlockup_print_freq(int domain, unsigned long *freq_idx)
{
	char buf[PRINT_LINE_MAX];
	ssize_t offset = 0;
	struct freq_log *freq;
	int i;

	if (!strcmp(dss_freq_name[domain], "")
		|| (freq_idx[0] == ULONG_MAX && freq_idx[1] == ULONG_MAX))
		return;

	offset += scnprintf(buf + offset, PRINT_LINE_MAX - offset, "%6s", dss_freq_name[domain]);


	for (i = 0; i < 2; i++) {
		if (freq_idx[i] != ULONG_MAX) {
			freq = dss_get_freq_log_by_idx(domain, freq_idx[i]);
			offset += scnprintf(buf + offset, PRINT_LINE_MAX - offset, " [%16llu] %7u -> %7u[%c]",
				freq->time, freq->old_freq, freq->target_freq, (freq->en == 1) ? 'i' : 'o');
			secdbg_exin_set_hardlockup_freq(dss_freq_name[domain], freq);
		}
	}

	if (domain < MAX_PR_AUTO)
		pr_auto(ASL3, "%s\n", buf);
	else
		pr_info("%s\n", buf);
}

static void secdbg_hardlockup_show_freq(struct hardlockup_info *hl_info)
{
	int i;

	for (i = 0; i < DSS_DOMAIN_NUM; i++) {
		unsigned long start, len, max;
		unsigned long freq_idx[2] = {ULONG_MAX, ULONG_MAX};
		struct freq_log *freq;

		init_vars(freq, i, start, len, max);

		for_each_item_in_dss_by_cpu(freq, i, start, len, true) {
			if (freq->time > hl_info->time) {
				freq_idx[0] = (start - 1) & (max - 1);
				freq_idx[1] = start & (max - 1);
				break;
			}
		}

		if (max && !len)
			freq_idx[0] = (start - 1) & (max - 1);

		secdbg_hardlockup_print_freq(i, freq_idx);
	}
}

static char *secdbg_hardlockup_get_gpio_name(int irq)
{
	char symname[KSYM_NAME_LEN];
	struct irq_desc *desc = irq_to_desc(irq);

	snprintf(symname, KSYM_NAME_LEN, "%ps", desc->handle_irq);

	if (strstr(symname, "exynos_irq_eint0_15")) {
		struct exynos_weint_data *eintd = irq_desc_get_handler_data(desc);
		struct samsung_pin_bank *bank;
		char *gpio_name = NULL;

		if (!eintd)
			return "None";

		bank = eintd->bank;

		if (!bank)
			return "None";

		gpio_name = kmalloc(GPIO_NAME_LEN + 1, GFP_NOWAIT | __GFP_NOWARN | __GFP_NORETRY);
		if (!gpio_name)
			return "None";

		snprintf(gpio_name, GPIO_NAME_LEN, "%s[%d]", bank->name, eintd->irq);

		return gpio_name;
	}

	return "None";
}

static void secdbg_hardlockup_show_info(struct hardlockup_info *hl_info)
{
	char buf[PRINT_LINE_MAX];
	ssize_t offset = 0;

	offset += scnprintf(buf + offset, PRINT_LINE_MAX - offset, "BUG: Hardlockup stuck for %lluns from %lluns [%s",
		 hl_info->delay_time, hl_info->time, hl_to_name[hl_info->hl_type]);

	switch (hl_info->hl_type) {
	case HL_TASK_STUCK:
		offset += scnprintf(buf + offset, PRINT_LINE_MAX - offset, " task=%s[%s]]", hl_info->task_info.task_comm, hl_info->task_info.group_leader);
		secdbg_exin_set_hardlockup_type("TASK_%s", hl_info->task_info.task_comm);
		break;
	case HL_IRQ_STUCK:
		offset += scnprintf(buf + offset, PRINT_LINE_MAX - offset, " irq=%d, hwirq=%lu, %s, %ps]",
				hl_info->irq_info.irq, irq_get_irq_data(hl_info->irq_info.irq)->hwirq, secdbg_hardlockup_get_gpio_name(hl_info->irq_info.irq), hl_info->irq_info.fn);
		secdbg_exin_set_hardlockup_type("IRQ_%d_%ps", hl_info->irq_info.irq, hl_info->irq_info.fn);
		break;
	case HL_IDLE_STUCK:
		offset += scnprintf(buf + offset, PRINT_LINE_MAX - offset, " mode=%s]", hl_info->cpuidle_info.mode);
		secdbg_exin_set_hardlockup_type("IDLE_%s", hl_info->cpuidle_info.mode);
		break;
	case HL_SMC_CALL_STUCK:
		offset += scnprintf(buf + offset, PRINT_LINE_MAX - offset, " cmd=%u]", hl_info->smc_info.cmd);
		secdbg_exin_set_hardlockup_type("SMC_%s", hl_info->task_info.task_comm);
		break;
	case HL_IRQ_STORM:
		offset += scnprintf(buf + offset, PRINT_LINE_MAX - offset, " irq=%d, hwirq=%lu, %s, %ps, avg_period=%lluns]",
				hl_info->irq_info.irq, irq_get_irq_data(hl_info->irq_info.irq)->hwirq, secdbg_hardlockup_get_gpio_name(hl_info->irq_info.irq), hl_info->irq_info.fn, hl_info->irq_info.avg_period);
		secdbg_exin_set_hardlockup_type("IRQs_%d_%s_%lluns",
				hl_info->irq_info.irq, hl_info->irq_info.fn, hl_info->irq_info.avg_period);
		break;
	default:
		offset += scnprintf(buf + offset, PRINT_LINE_MAX - offset, "]");
		break;
	}

	pr_auto(ASL3, "%s\n", buf);
	secdbg_exin_set_hardlockup_data(buf);
}

static void secdbg_hardlockup_print_ehld_type(void)
{
	unsigned int cpu;
	struct hardlockup_info *hl_info;
	char buf[PRINT_LINE_MAX];
	ssize_t offset = 0;
	bool is_alive = true;
	const char *str[] = {"NO INSTRET", "NO INSTRUN"};
	int i;

	offset += scnprintf(buf + offset, PRINT_LINE_MAX - offset, "EHLD: ");
	for_each_possible_cpu(cpu) {
		hl_info = per_cpu_ptr(&percpu_hl_info, cpu);
		if (hl_info->ehld_type != 0) {
			is_alive = false;
			offset += scnprintf(buf + offset, PRINT_LINE_MAX - offset, " [C%u]", cpu);
			for (i = 0; i < MAX_ETYPES; i++) {
				if ((hl_info->ehld_type & (1 << i)) != 0)
					offset += scnprintf(buf + offset, PRINT_LINE_MAX - offset, " %s", str[i]);
			}
		}
	}

	if (is_alive)
		offset += scnprintf(buf + offset, PRINT_LINE_MAX - offset, " ALL CORE NOT DETECTED");

	pr_auto(ASL1, "%s\n", buf);
}

static bool secdbg_hardlockup_check_ehld_info(unsigned int cpu, unsigned long long *time, unsigned long long *inst, unsigned long index)
{
	unsigned long long last;
	int count, max;
	unsigned long long curr = local_clock();
	unsigned long long thresh = hardlockup_watchdog_get_thresh() * NSEC_PER_SEC - hardlockup_watchdog_get_period();

	if (time[index] == 0 && inst[index] == 0)
		return false;

	count = max = NUM_TRACE;
	last = inst[index];

	while (--count) {
		index = (index - 1) & (max - 1);
		if (last != inst[index]
			|| (time[index] == 0 && inst[index] == 0))
			break;
	}

	index = (index + 1) & (NUM_TRACE - 1);

	return (curr > time[index] && (curr - time[index] >= thresh)) ? true : false;
}

static void secdbg_hardlockup_save_ehld_type(unsigned int cpu)
{
	struct ehld_data *data = ehld_get_ctrl_data(cpu);
	struct hardlockup_info *hl_info = per_cpu_ptr(&percpu_hl_info, cpu);
	unsigned long index;

	if (!data)
		return;

	index = data->data_ptr & (NUM_TRACE - 1);

	hl_info->ehld_type = 0;

	if (secdbg_hardlockup_check_ehld_info(cpu, data->time, data->instret, index))
		hl_info->ehld_type |= (1 << NO_INSTRET);

	if (secdbg_hardlockup_check_ehld_info(cpu, data->time, data->instrun, index))
		hl_info->ehld_type |= (1 << NO_INSTRUN);

	secdbg_exin_set_hardlockup_ehld(hl_info->ehld_type, cpu);
}

static void secdbg_hardlockup_show_ehld_info(void)
{
	unsigned int cpu;

	for_each_possible_cpu(cpu) {
		secdbg_hardlockup_save_ehld_type(cpu);
	}

	secdbg_hardlockup_print_ehld_type();
}

static int secdbg_hardlockup_info_handler(struct notifier_block *nb,
					unsigned long l, void *core)
{
	unsigned int *cpu = (unsigned int *)core;
	struct hardlockup_info *hl_info = per_cpu_ptr(&percpu_hl_info, *cpu);

	dbg_snapshot_set_item_enable("log_kevents", false);

	secdbg_hardlockup_get_info(*cpu, hl_info);
	secdbg_hardlockup_show_info(hl_info);
	secdbg_hardlockup_show_freq(hl_info);
	secdbg_hardlockup_show_ehld_info();

	return NOTIFY_DONE;
}

static struct notifier_block secdbg_hardlockup_info_block = {
	.notifier_call = secdbg_hardlockup_info_handler,
};

static int __init secdbg_hardlockup_info_init(void)
{
	pr_info("%s: init\n", __func__);

	atomic_notifier_chain_register(&hardlockup_notifier_list,
					&secdbg_hardlockup_info_block);

	return 0;
}
module_init(secdbg_hardlockup_info_init);

static void __exit secdbg_hardlockup_info_exit(void)
{
}
module_exit(secdbg_hardlockup_info_exit);

MODULE_DESCRIPTION("Samsung Debug Watchdog debug driver");
MODULE_LICENSE("GPL v2");
