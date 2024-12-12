/*
 * Copyright (c) 2013 Samsung Electronics Co., Ltd.
 *		http://www.samsung.com
 *
 * Debug-SnapShot: Debug Framework for Ramdump based debugging method
 * The original code is Exynos-Snapshot for Exynos SoC
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#include <linux/kernel.h>
#include <linux/slab.h>
#include <linux/module.h>
#include <linux/ktime.h>
#include <linux/clk-provider.h>
#include <linux/sched/clock.h>
#include <linux/ftrace.h>
#include <linux/kernel_stat.h>
#include <linux/irqnr.h>
#include <linux/irq.h>
#include <linux/irqdesc.h>
#include <linux/sec_debug.h>

#include <asm/stacktrace.h>
#include <soc/samsung/debug-snapshot.h>
#include "debug-snapshot-local.h"

#include <trace/events/power.h>

struct dbg_snapshot_log_item dss_log_items[] = {
	[DSS_LOG_TASK_ID]	= {DSS_LOG_TASK,	{0, 0, 0, false}, },
	[DSS_LOG_WORK_ID]	= {DSS_LOG_WORK,	{0, 0, 0, false}, },
	[DSS_LOG_CPUIDLE_ID]	= {DSS_LOG_CPUIDLE,	{0, 0, 0, false}, },
	[DSS_LOG_SUSPEND_ID]	= {DSS_LOG_SUSPEND,	{0, 0, 0, false}, },
	[DSS_LOG_IRQ_ID]	= {DSS_LOG_IRQ,		{0, 0, 0, false}, },
	[DSS_LOG_SPINLOCK_ID]	= {DSS_LOG_SPINLOCK,	{0, 0, 0, false}, },
	[DSS_LOG_HRTIMER_ID]	= {DSS_LOG_HRTIMER,	{0, 0, 0, false}, },
	[DSS_LOG_CLK_ID]	= {DSS_LOG_CLK,		{0, 0, 0, false}, },
	[DSS_LOG_PMU_ID]	= {DSS_LOG_PMU,		{0, 0, 0, false}, },
	[DSS_LOG_FREQ_ID]	= {DSS_LOG_FREQ,	{0, 0, 0, false}, },
	[DSS_LOG_DM_ID]		= {DSS_LOG_DM,		{0, 0, 0, false}, },
	[DSS_LOG_REGULATOR_ID]	= {DSS_LOG_REGULATOR,	{0, 0, 0, false}, },
	[DSS_LOG_THERMAL_ID]	= {DSS_LOG_THERMAL,	{0, 0, 0, false}, },
	[DSS_LOG_ACPM_ID]	= {DSS_LOG_ACPM,	{0, 0, 0, false}, },
	[DSS_LOG_PRINTK_ID]	= {DSS_LOG_PRINTK,	{0, 0, 0, false}, },
};

/*  Internal interface variable */
struct dbg_snapshot_log_misc dss_log_misc;
static char dss_freq_name[SZ_32][SZ_8];
static unsigned int dss_freq_size = 0;

#define dss_get_log(item)						\
long dss_get_len_##item##_log(void) {					\
	return ARRAY_SIZE(dss_log->item);				\
}									\
EXPORT_SYMBOL(dss_get_len_##item##_log);				\
long dss_get_last_##item##_log_idx(void) {				\
	return (atomic_read(&dss_log_misc.item##_log_idx) - 1) &	\
			(dss_get_len_##item##_log() - 1);		\
}									\
EXPORT_SYMBOL(dss_get_last_##item##_log_idx);				\
long dss_get_first_##item##_log_idx(void) {				\
	return atomic_read(&dss_log_misc.item##_log_idx) &		\
			(dss_get_len_##item##_log() - 1);		\
}									\
EXPORT_SYMBOL(dss_get_first_##item##_log_idx);				\
struct item##_log *dss_get_last_##item##_log(void) {			\
	return &dss_log->item[dss_get_last_##item##_log_idx()];		\
}									\
EXPORT_SYMBOL(dss_get_last_##item##_log);				\
struct item##_log *dss_get_first_##item##_log(void) {			\
	return &dss_log->item[dss_get_first_##item##_log_idx()];	\
}									\
EXPORT_SYMBOL(dss_get_first_##item##_log);				\
struct item##_log *dss_get_##item##_log_by_idx(int idx) {		\
	if (idx < 0 || idx >= dss_get_len_##item##_log())		\
		return NULL;						\
	return &dss_log->item[idx];					\
}									\
EXPORT_SYMBOL(dss_get_##item##_log_by_idx);				\
struct item##_log *dss_get_##item##_log_iter(int idx) {			\
	if (idx < 0)							\
		idx = dss_get_len_##item##_log() - abs(idx);		\
	if (idx >= dss_get_len_##item##_log())				\
		idx -= dss_get_len_##item##_log();			\
	return &dss_log->item[idx];					\
}									\
EXPORT_SYMBOL(dss_get_##item##_log_iter);				\
unsigned long dss_get_vaddr_##item##_log(void) {			\
	return (unsigned long)dss_log->item;				\
}									\
EXPORT_SYMBOL(dss_get_vaddr_##item##_log)


#define dss_get_log_by_cpu(item)					\
long dss_get_len_##item##_log(void) {					\
	return ARRAY_SIZE(dss_log->item);				\
}									\
EXPORT_SYMBOL(dss_get_len_##item##_log);				\
long dss_get_len_##item##_log_by_cpu(int cpu) {				\
	if (cpu < 0 || cpu >= dss_get_len_##item##_log())		\
		return -EINVAL;						\
	return ARRAY_SIZE(dss_log->item[cpu]);				\
}									\
EXPORT_SYMBOL(dss_get_len_##item##_log_by_cpu);				\
long dss_get_last_##item##_log_idx(int cpu) {				\
	if (cpu < 0 || cpu >= dss_get_len_##item##_log())		\
		return -EINVAL;						\
	return (atomic_read(&dss_log_misc.item##_log_idx[cpu]) - 1) &	\
			(dss_get_len_##item##_log_by_cpu(cpu) - 1);	\
}									\
EXPORT_SYMBOL(dss_get_last_##item##_log_idx);				\
long dss_get_first_##item##_log_idx(int cpu) {				\
	if (cpu < 0 || cpu >= dss_get_len_##item##_log())		\
		return -EINVAL;						\
	return atomic_read(&dss_log_misc.item##_log_idx[cpu]) &		\
			(dss_get_len_##item##_log_by_cpu(cpu) - 1);	\
}									\
EXPORT_SYMBOL(dss_get_first_##item##_log_idx);				\
struct item##_log *dss_get_last_##item##_log(int cpu) {			\
	if (cpu < 0 || cpu >= dss_get_len_##item##_log())		\
		return NULL;						\
	return &dss_log->item[cpu][dss_get_last_##item##_log_idx(cpu)];	\
}									\
EXPORT_SYMBOL(dss_get_last_##item##_log);				\
struct item##_log *dss_get_first_##item##_log(int cpu) {		\
	if (cpu < 0 || cpu >= dss_get_len_##item##_log())		\
		return NULL;						\
	return &dss_log->item[cpu][dss_get_first_##item##_log_idx(cpu)];\
}									\
EXPORT_SYMBOL(dss_get_first_##item##_log);				\
struct item##_log *dss_get_##item##_log_by_idx(int cpu, int idx) {	\
	if (cpu < 0 || cpu >= dss_get_len_##item##_log())		\
		return NULL;						\
	if (idx < 0 || idx >= dss_get_len_##item##_log_by_cpu(cpu))	\
		return NULL;						\
	return &dss_log->item[cpu][idx];				\
}									\
EXPORT_SYMBOL(dss_get_##item##_log_by_idx);				\
struct item##_log *dss_get_##item##_log_by_cpu_iter(int cpu, int idx) {	\
	if (cpu < 0 || cpu >= dss_get_len_##item##_log())		\
		return NULL;						\
	if (idx < 0)							\
		idx = dss_get_len_##item##_log_by_cpu(cpu) - abs(idx);	\
	if (idx >= dss_get_len_##item##_log_by_cpu(cpu))		\
		idx -= dss_get_len_##item##_log_by_cpu(cpu);		\
	return &dss_log->item[cpu][idx];				\
}									\
EXPORT_SYMBOL(dss_get_##item##_log_by_cpu_iter);			\
unsigned long dss_get_vaddr_##item##_log_by_cpu(int cpu) {		\
	if (cpu < 0 || cpu >= dss_get_len_##item##_log())		\
		return 0;						\
	return (unsigned long)dss_log->item[cpu];			\
}									\
EXPORT_SYMBOL(dss_get_vaddr_##item##_log_by_cpu)

dss_get_log_by_cpu(task);
dss_get_log_by_cpu(work);
dss_get_log_by_cpu(cpuidle);
dss_get_log_by_cpu(freq);
dss_get_log(suspend);
dss_get_log_by_cpu(irq);
dss_get_log_by_cpu(spinlock);
dss_get_log_by_cpu(hrtimer);
dss_get_log(clk);
dss_get_log(pmu);
dss_get_log(dm);
dss_get_log(regulator);
dss_get_log(thermal);
dss_get_log(acpm);
dss_get_log(print);

#define log_item_set_filed(id, log_name)				\
		log_item = &dss_log_items[DSS_LOG_##id##_ID];		\
		log_item->entry.vaddr = (size_t)(&dss_log->log_name[0]);\
		log_item->entry.paddr = item->entry.paddr +		\
					((size_t)&dss_log->log_name[0] -\
					(size_t)&dss_log->task[0]);	\
		log_item->entry.size = sizeof(dss_log->log_name);	\
		if (!log_item->entry.paddr || !log_item->entry.vaddr	\
				|| !log_item->entry.size)		\
			log_item->entry.enabled = false;

#if IS_ENABLED(CONFIG_SEC_DEBUG_BASE)
#include <linux/bitfield.h>

static unsigned long __get_par_to_addr(u64 par, unsigned long vaddr)
{
	u64 tmp, rpar;

	tmp = (u64)vaddr & (0xfff);
	rpar = ((par & ~(0xfffUL << 52)) & ~(0xfff));
	rpar = rpar | tmp;

	return rpar;
}

static unsigned long secdbg_base_get_pa_from_mmu(unsigned long vaddr)
{
	unsigned long flags;
	u64 par, dfsc;

	local_irq_save(flags);
	asm volatile("at s1e1r, %0" :: "r" (vaddr));
	isb();
	par = read_sysreg(par_el1);
	local_irq_restore(flags);

	if (par & SYS_PAR_EL1_F) {
		dfsc = FIELD_GET(SYS_PAR_EL1_FST, par);
		pr_err("failed to get v:0x%lx, p:0x%lx, fsc:0x%lx\n",
			vaddr, par, dfsc);
		return 0;
	}

	return __get_par_to_addr(par, vaddr);
}

#define _OFFSET_MISC_IDX(member)	\
	(offsetof(typeof(dss_log_misc), member))

static unsigned long secdbg_base_get_kevent_index_addr(int type)
{
	unsigned long paddr;

	paddr = secdbg_base_get_pa_from_mmu((unsigned long)&dss_log_misc);
	if (!paddr)
		return 0;

	switch (type) {
	case DSS_KEVENT_TASK:
		return paddr + _OFFSET_MISC_IDX(task_log_idx);

	case DSS_KEVENT_WORK:
		return paddr + _OFFSET_MISC_IDX(work_log_idx);

	case DSS_KEVENT_IRQ:
		return paddr + _OFFSET_MISC_IDX(irq_log_idx);

	case DSS_KEVENT_FREQ:
		return paddr + _OFFSET_MISC_IDX(freq_log_idx);

	case DSS_KEVENT_IDLE:
		return paddr + _OFFSET_MISC_IDX(cpuidle_log_idx);

	case DSS_KEVENT_THRM:
		return paddr + _OFFSET_MISC_IDX(thermal_log_idx);

	case DSS_KEVENT_ACPM:
		return paddr + _OFFSET_MISC_IDX(acpm_log_idx);

	default:
		return 0;
	}
}

void secdbg_base_get_kevent_info(struct ess_info_offset *p, int type)
{
	unsigned long kevent_base_va = (unsigned long)(dss_log->task);
	unsigned long kevent_base_pa = dss_items[DSS_ITEM_KEVENTS_ID].entry.paddr;

	switch (type) {
	case DSS_KEVENT_TASK:
		p->base = kevent_base_pa + (unsigned long)(dss_log->task) - kevent_base_va;
		p->nr = ARRAY_SIZE(dss_log->task[0]);
		p->size = sizeof(struct task_log);
		p->per_core = 1;
		break;

	case DSS_KEVENT_WORK:
		p->base = kevent_base_pa + (unsigned long)(dss_log->work) - kevent_base_va;
		p->nr = ARRAY_SIZE(dss_log->work[0]);
		p->size = sizeof(struct work_log);
		p->per_core = 1;
		break;

	case DSS_KEVENT_IRQ:
		p->base = kevent_base_pa + (unsigned long)(dss_log->irq) - kevent_base_va;
		p->nr = ARRAY_SIZE(dss_log->irq[0]);
		p->size = sizeof(struct irq_log);
		p->per_core = 1;
		break;

	case DSS_KEVENT_FREQ:
		p->base = kevent_base_pa + (unsigned long)(dss_log->freq) - kevent_base_va;
		p->nr = ARRAY_SIZE(dss_log->freq[0]);
		p->size = sizeof(struct freq_log);
		p->per_core = ARRAY_SIZE(dss_log->freq);
		break;

	case DSS_KEVENT_IDLE:
		p->base = kevent_base_pa + (unsigned long)(dss_log->cpuidle) - kevent_base_va;
		p->nr = ARRAY_SIZE(dss_log->cpuidle[0]);
		p->size = sizeof(struct cpuidle_log);
		p->per_core = 1;
		break;

	case DSS_KEVENT_THRM:
		p->base = kevent_base_pa + (unsigned long)(dss_log->thermal) - kevent_base_va;
		p->nr = ARRAY_SIZE(dss_log->thermal);
		p->size = sizeof(struct thermal_log);
		p->per_core = 0;
		break;

	case DSS_KEVENT_ACPM:
		p->base = kevent_base_pa + (unsigned long)(dss_log->acpm) - kevent_base_va;
		p->nr = ARRAY_SIZE(dss_log->acpm);
		p->size = sizeof(struct acpm_log);
		p->per_core = 0;
		break;

	default:
		p->base = 0;
		p->nr = 0;
		p->size = 0;
		p->per_core = 0;
		break;
	}

	p->last = secdbg_base_get_kevent_index_addr(type);
}
EXPORT_SYMBOL_GPL(secdbg_base_get_kevent_info);
#endif

static inline bool dbg_snapshot_is_log_item_enabled(int id)
{
	bool item_enabled = dss_items[DSS_ITEM_KEVENTS_ID].entry.enabled;
	bool log_enabled = dss_log_items[id].entry.enabled;

	return dbg_snapshot_get_enable() && item_enabled && log_enabled;
}

void *dbg_snapshot_log_get_item_by_index(int index)
{
	if (index < 0 || index > ARRAY_SIZE(dss_log_items))
		return NULL;

	return  &dss_log_items[index];
}
EXPORT_SYMBOL(dbg_snapshot_log_get_item_by_index);

int dbg_snapshot_log_get_num_items(void)
{
	return ARRAY_SIZE(dss_log_items);
}
EXPORT_SYMBOL(dbg_snapshot_log_get_num_items);

int dbg_snapshot_get_freq_idx(const char *name)
{
	unsigned int i;

	for (i = 0; i < dss_freq_size; i++) {
		if (!strncmp(dss_freq_name[i], name, strlen(name)))
			return i;
	}

	return -EFAULT;
}
EXPORT_SYMBOL(dbg_snapshot_get_freq_idx);

void dbg_snapshot_log_output(void)
{
	unsigned long i;

	pr_info("debug-snapshot-log physical / virtual memory layout:\n");
	for (i = 0; i < ARRAY_SIZE(dss_log_items); i++) {
		if (dss_log_items[i].entry.enabled)
			pr_info("%-12s: phys:0x%zx / virt:0x%zx / size:0x%zx / en:%d\n",
				dss_log_items[i].name,
				dss_log_items[i].entry.paddr,
				dss_log_items[i].entry.vaddr,
				dss_log_items[i].entry.size,
				dss_log_items[i].entry.enabled);
	}
}

void __init dbg_snapshot_set_enable_log_item(const char *name, int en)
{
	struct dbg_snapshot_log_item *log_item;
	int i;

	if (!name)
		return;

	for (i = 0; i < ARRAY_SIZE(dss_log_items); i++) {
		log_item = &dss_log_items[i];
		if (log_item->name &&
				!strncmp(log_item->name, name, strlen(name))) {
			log_item->entry.enabled = en;
			pr_info("log item - %s is %sabled\n", name, en ? "en" : "dis");
			break;
		}
	}
}

static void dbg_snapshot_suspend(const char *log, struct device *dev,
				int event, int en)
{
	unsigned long i;

	if (unlikely(!dbg_snapshot_is_log_item_enabled(DSS_LOG_SUSPEND_ID)))
		return;

	i = atomic_fetch_inc(&dss_log_misc.suspend_log_idx) &
			(ARRAY_SIZE(dss_log->suspend) - 1);

	dss_log->suspend[i].time = local_clock();
	dss_log->suspend[i].log = log ? log : NULL;
	dss_log->suspend[i].event = event;
	dss_log->suspend[i].dev = dev ? dev_name(dev) : "";
	dss_log->suspend[i].core = raw_smp_processor_id();
	dss_log->suspend[i].en = en;
}

static void dbg_snapshot_suspend_resume(void *ignore, const char *action,
					int event, bool start)
{
	dbg_snapshot_suspend(action, NULL, event,
			start ? DSS_FLAG_IN : DSS_FLAG_OUT);
}

static void dbg_snapshot_dev_pm_cb_start(void *ignore, struct device *dev,
					const char *info, int event)
{
	dbg_snapshot_suspend(info, dev, event, DSS_FLAG_IN);
}

static void dbg_snapshot_dev_pm_cb_end(void *ignore, struct device *dev,
					int error)
{
	dbg_snapshot_suspend(NULL, dev, error, DSS_FLAG_OUT);
}

void dbg_snapshot_cpuidle_mod(char *modes, unsigned int state, s64 diff, int en)
{
	int cpu = raw_smp_processor_id();
	unsigned int i;

	if (unlikely(!dbg_snapshot_is_log_item_enabled(DSS_LOG_CPUIDLE_ID)))
		return;

	i = atomic_fetch_inc(&dss_log_misc.cpuidle_log_idx[cpu]) &
		(ARRAY_SIZE(dss_log->cpuidle[0]) - 1);
	dss_log->cpuidle[cpu][i].time = local_clock();
	dss_log->cpuidle[cpu][i].modes = modes;
	dss_log->cpuidle[cpu][i].state = state;
	dss_log->cpuidle[cpu][i].num_online_cpus = num_online_cpus();
	dss_log->cpuidle[cpu][i].delta = (int)diff;
	dss_log->cpuidle[cpu][i].en = en;
}
EXPORT_SYMBOL(dbg_snapshot_cpuidle_mod);

void dbg_snapshot_regulator(unsigned long long timestamp, char* f_name,
			unsigned int addr, unsigned int volt,
			unsigned int rvolt, int en)
{
	unsigned long i;

	if (!dbg_snapshot_is_log_item_enabled(DSS_LOG_REGULATOR_ID))
		return;

	i = atomic_fetch_inc(&dss_log_misc.regulator_log_idx) &
		(ARRAY_SIZE(dss_log->regulator) - 1);

	dss_log->regulator[i].time = local_clock();
	dss_log->regulator[i].cpu = raw_smp_processor_id();
	dss_log->regulator[i].acpm_time = timestamp;
	strncpy(dss_log->regulator[i].name, f_name,
			min((int)strlen(f_name), SZ_16 - 1));
	dss_log->regulator[i].reg = addr;
	dss_log->regulator[i].en = en;
	dss_log->regulator[i].voltage = volt;
	dss_log->regulator[i].raw_volt = rvolt;
}
EXPORT_SYMBOL(dbg_snapshot_regulator);

void dbg_snapshot_thermal(void *data, unsigned int temp, char *name,
			unsigned int max_cooling)
{
	unsigned long i;

	if (!dbg_snapshot_is_log_item_enabled(DSS_LOG_THERMAL_ID))
		return;

	i = atomic_fetch_inc(&dss_log_misc.thermal_log_idx) &
		(ARRAY_SIZE(dss_log->thermal) - 1);

	dss_log->thermal[i].time = local_clock();
	dss_log->thermal[i].cpu = raw_smp_processor_id();
	dss_log->thermal[i].data = (struct exynos_tmu_data *)data;
	dss_log->thermal[i].temp = temp;
	dss_log->thermal[i].cooling_device = name;
	dss_log->thermal[i].cooling_state = max_cooling;

}
EXPORT_SYMBOL(dbg_snapshot_thermal);

void dbg_snapshot_clk(void *clock, const char *func_name, unsigned long arg, int mode)
{
	unsigned long i;

	if (!dbg_snapshot_is_log_item_enabled(DSS_LOG_CLK_ID))
		return;

	i = atomic_fetch_inc(&dss_log_misc.clk_log_idx) &
		(ARRAY_SIZE(dss_log->clk) - 1);

	dss_log->clk[i].time = local_clock();
	dss_log->clk[i].mode = mode;
	dss_log->clk[i].arg = arg;
	dss_log->clk[i].clk = (struct clk_hw *)clock;
	dss_log->clk[i].f_name = func_name;
}
EXPORT_SYMBOL(dbg_snapshot_clk);

void dbg_snapshot_pmu(int id, const char *func_name, int mode)
{
	unsigned long i;

	if (!dbg_snapshot_is_log_item_enabled(DSS_LOG_PMU_ID))
		return;

	i = atomic_fetch_inc(&dss_log_misc.pmu_log_idx) &
		(ARRAY_SIZE(dss_log->pmu) - 1);

	dss_log->pmu[i].time = local_clock();
	dss_log->pmu[i].mode = mode;
	dss_log->pmu[i].id = id;
	dss_log->pmu[i].f_name = func_name;
}
EXPORT_SYMBOL(dbg_snapshot_pmu);

void dbg_snapshot_freq(int type, unsigned int old_freq,
			unsigned int target_freq, int en)
{
	unsigned long i;

	if (unlikely(type < 0 || type > dss_freq_size))
		return;

	if (unlikely(!dbg_snapshot_is_log_item_enabled(DSS_LOG_FREQ_ID)))
		return;

	i = atomic_fetch_inc(&dss_log_misc.freq_log_idx[type]) &
		(ARRAY_SIZE(dss_log->freq[0]) - 1);

	dss_log->freq[type][i].time = local_clock();
	dss_log->freq[type][i].cpu = raw_smp_processor_id();
	dss_log->freq[type][i].old_freq = old_freq;
	dss_log->freq[type][i].target_freq = target_freq;
	dss_log->freq[type][i].en = en;
}
EXPORT_SYMBOL(dbg_snapshot_freq);

void dbg_snapshot_dm(int type, unsigned int min, unsigned int max,
			s32 wait_t, s32 t)
{
	unsigned long i;

	if (!dbg_snapshot_is_log_item_enabled(DSS_LOG_DM_ID))
		return;

	i = atomic_fetch_inc(&dss_log_misc.dm_log_idx) &
		(ARRAY_SIZE(dss_log->dm) - 1);

	dss_log->dm[i].time = local_clock();
	dss_log->dm[i].cpu = raw_smp_processor_id();
	dss_log->dm[i].dm_num = type;
	dss_log->dm[i].min_freq = min;
	dss_log->dm[i].max_freq = max;
	dss_log->dm[i].wait_dmt = wait_t;
	dss_log->dm[i].do_dmt = t;
}
EXPORT_SYMBOL(dbg_snapshot_dm);

void dbg_snapshot_acpm(unsigned long long timestamp, const char *log,
			unsigned int data)
{
	unsigned long i;
	int len;

	if (unlikely(!dbg_snapshot_is_log_item_enabled(DSS_LOG_ACPM_ID)))
		return;

	i = atomic_fetch_inc(&dss_log_misc.acpm_log_idx) &
		(ARRAY_SIZE(dss_log->acpm) - 1);

	len = min((int)strlen(log), 8);
	dss_log->acpm[i].time = local_clock();
	dss_log->acpm[i].acpm_time = timestamp;
	strncpy(dss_log->acpm[i].log, log, len);
	dss_log->acpm[i].log[len] = '\0';
	dss_log->acpm[i].data = data;
}
EXPORT_SYMBOL(dbg_snapshot_acpm);

void dbg_snapshot_printk(const char *fmt, ...)
{
	unsigned long i;
	va_list args;

	if (unlikely(!dbg_snapshot_is_log_item_enabled(DSS_LOG_PRINTK_ID)))
		return;

	i = atomic_fetch_inc(&dss_log_misc.print_log_idx) &
		(ARRAY_SIZE(dss_log->print) - 1);

	va_start(args, fmt);
	vsnprintf(dss_log->print[i].log, sizeof(dss_log->print[i].log),
			fmt, args);
	va_end(args);

	dss_log->print[i].time = local_clock();
	dss_log->print[i].cpu = raw_smp_processor_id();
}
EXPORT_SYMBOL(dbg_snapshot_printk);

static inline void dbg_snapshot_get_sec(unsigned long long ts,
					unsigned long *sec, unsigned long *msec)
{
	*sec = ts / NSEC_PER_SEC;
	*msec = (ts % NSEC_PER_SEC) / USEC_PER_MSEC;
}

static void dbg_snapshot_print_last_irq(int cpu)
{
	struct dbg_snapshot_log_item *log_item = &dss_log_items[DSS_LOG_IRQ_ID];
	unsigned long idx, sec, msec;

	if (!log_item->entry.enabled)
		return;

	idx = dss_get_last_irq_log_idx(cpu);
	dbg_snapshot_get_sec(dss_log->irq[cpu][idx].time, &sec, &msec);

	pr_info("%-12s: [%4ld] %10lu.%06lu sec, %10s: %pS, %8s: %8d, %10s: %2d, %s\n",
			">>> irq", idx, sec, msec,
			"handler", dss_log->irq[cpu][idx].fn,
			"irq", dss_log->irq[cpu][idx].irq,
			"en", dss_log->irq[cpu][idx].en,
			(dss_log->irq[cpu][idx].en == 1) ? "[Missmatch]" : "");
}

static void dbg_snapshot_print_last_task(int cpu)
{
	struct dbg_snapshot_log_item *log_item = &dss_log_items[DSS_LOG_TASK_ID];
	unsigned long idx, sec, msec;
	struct task_struct *task;

	if (!log_item->entry.enabled)
		return;

	idx = dss_get_last_task_log_idx(cpu);
	dbg_snapshot_get_sec(dss_log->task[cpu][idx].time, &sec, &msec);
	task = dss_log->task[cpu][idx].task;

	pr_info("%-12s: [%4lu] %10lu.%06lu sec, %10s: %-16s, %8s: 0x%-16px, %10s: %16llu\n",
			">>> task", idx, sec, msec,
			"task_comm", (task) ? task->comm : "NULL",
			"task", task,
			"exec_start", (task) ? task->se.exec_start : 0);
}

static void dbg_snapshot_print_last_work(int cpu)
{
	struct dbg_snapshot_log_item *log_item = &dss_log_items[DSS_LOG_WORK_ID];
	unsigned long idx, sec, msec;

	if (!log_item->entry.enabled)
		return;

	idx = dss_get_last_work_log_idx(cpu);
	dbg_snapshot_get_sec(dss_log->work[cpu][idx].time, &sec, &msec);

	pr_info("%-12s: [%4lu] %10lu.%06lu sec, %10s: %pS, %3s: %3d %s\n",
			">>> work", idx, sec, msec,
			"work_fn", dss_log->work[cpu][idx].fn,
			"en", dss_log->work[cpu][idx].en,
			(dss_log->work[cpu][idx].en == 1) ? "[Missmatch]" : "");
}

static void dbg_snapshot_print_last_cpuidle(int cpu)
{
	struct dbg_snapshot_log_item *log_item = &dss_log_items[DSS_LOG_CPUIDLE_ID];
	unsigned long idx, sec, msec;

	if (!log_item->entry.enabled)
		return;

	idx = dss_get_last_cpuidle_log_idx(cpu);
	dbg_snapshot_get_sec(dss_log->cpuidle[cpu][idx].time, &sec, &msec);

	pr_info("%-12s: [%4lu] %10lu.%06lu sec, %10s: %s, %6s: %2d, %10s: %10d"
			", %12s: %2d, %3s: %3d %s\n",
			">>> cpuidle", idx, sec, msec,
			"modes", dss_log->cpuidle[cpu][idx].modes,
			"state", dss_log->cpuidle[cpu][idx].state,
			"stay time", dss_log->cpuidle[cpu][idx].delta,
			"online_cpus", dss_log->cpuidle[cpu][idx].num_online_cpus,
			"en", dss_log->cpuidle[cpu][idx].en,
			(dss_log->cpuidle[cpu][idx].en == 1) ? "[Missmatch]" : "");
}

static void dbg_snapshot_print_lastinfo(void)
{
	int cpu;

	pr_info("<last info>\n");
	for (cpu = 0; cpu < DSS_NR_CPUS; cpu++) {
		pr_info("CPU ID: %d ----------------------------------\n", cpu);
		dbg_snapshot_print_last_task(cpu);
		dbg_snapshot_print_last_work(cpu);
		dbg_snapshot_print_last_irq(cpu);
		dbg_snapshot_print_last_cpuidle(cpu);
	}
}

static void dbg_snapshot_print_freqinfo(void)
{
	struct dbg_snapshot_log_item *log_item = &dss_log_items[DSS_LOG_FREQ_ID];
	unsigned long i, idx, sec, msec;
	unsigned long old_freq, target_freq;

	if (!log_item->entry.enabled)
		return;

	pr_info("\n<last freq info>\n");
	for (i = 0; i < dss_freq_size; i++) {
		if (!atomic_read(&dss_log_misc.freq_log_idx[i])) {
			pr_info("%10s: no infomation\n", dss_freq_name[i]);
			continue;
		}
		idx = dss_get_last_freq_log_idx(i);
		dbg_snapshot_get_sec(dss_log->freq[i][idx].time, &sec, &msec);
		old_freq = dss_log->freq[i][idx].old_freq;
		target_freq = dss_log->freq[i][idx].target_freq;
		pr_info("%10s[%4lu]: %10lu.%06lu sec, %12s: %5luMhz, %12s: %5luMhz, %3s: %d %s\n",
				dss_freq_name[i], idx, sec, msec,
				"old_freq", old_freq / 1000,
				"target_freq", target_freq / 1000,
				"en", dss_log->freq[i][idx].en,
				(dss_log->freq[i][idx].en == 1) ? "[Missmatch]" : "");
	}
}

#ifndef arch_irq_stat
#define arch_irq_stat() 0
#endif

static void dbg_snapshot_print_irq(void)
{
	int i, cpu;
	u64 sum = 0;

	for_each_possible_cpu(cpu)
		sum += kstat_cpu_irqs_sum(cpu);

	sum += arch_irq_stat();

	pr_info("<irq info>\n");
	pr_info("----------------------------------------------------------\n");
	pr_info("sum irq : %llu", sum);
	pr_info("----------------------------------------------------------\n");

	for_each_irq_nr(i) {
		struct irq_desc *desc = irq_to_desc(i);
		unsigned int irq_stat = 0;
		const char *name;

		if (!desc)
			continue;

		for_each_possible_cpu(cpu)
			irq_stat += *per_cpu_ptr(desc->kstat_irqs, cpu);

		if (!irq_stat)
			continue;

		if (desc->action && desc->action->name)
			name = desc->action->name;
		else
			name = "???";
		pr_info("irq-%-4d(hwirq-%-3d) : %8u %s\n",
			i, (int)desc->irq_data.hwirq, irq_stat, name);
	}
}

void dbg_snapshot_print_log_report(void)
{
	if (unlikely(!dbg_snapshot_get_enable()))
		return;

	pr_info("==========================================================\n");
	pr_info("Panic Report\n");
	pr_info("==========================================================\n");
	dbg_snapshot_print_lastinfo();
	dbg_snapshot_print_freqinfo();
	dbg_snapshot_print_irq();
	pr_info("==========================================================\n");
}

#if IS_ENABLED(CONFIG_SEC_PM_LOG)
#include <linux/proc_fs.h>

static unsigned long curr_thermal_log_idx;

static int thermal_log_seq_show(struct seq_file *m, void *v)
{
	unsigned long idx, size = ARRAY_SIZE(dss_log->thermal);
	unsigned long rem_nsec;
	u64 ts;
	int i = *(loff_t *)v;

	if (!dbg_snapshot_is_log_item_enabled(DSS_LOG_THERMAL_ID))
		return 0;

	if (i >= size)
		return 0;

	if (i == 0) {
		curr_thermal_log_idx =
			atomic_read(&dss_log_misc.thermal_log_idx) - 1;

		seq_puts(m, "time\t\t\ttemperature\tcooling_device\t\tmax_frequency\n");
	}

	idx = curr_thermal_log_idx;

	idx &= size - 1;

	ts = dss_log->thermal[idx].time;
	if (!ts)
		return 0;

	rem_nsec = do_div(ts, NSEC_PER_SEC);

	seq_printf(m, "[%8lu.%06lu]\t%-12u\t%-16s\t%u\n",
			(unsigned long)ts, rem_nsec / NSEC_PER_USEC,
			dss_log->thermal[idx].temp,
			dss_log->thermal[idx].cooling_device,
			dss_log->thermal[idx].cooling_state);

	curr_thermal_log_idx--;

	return 0;
}

static void *thermal_log_seq_start(struct seq_file *f, loff_t *pos)
{
	unsigned long size = ARRAY_SIZE(dss_log->thermal);

	if (*pos)
		curr_thermal_log_idx++;

	return (*pos < size) ? pos : NULL;
}

static void *thermal_log_seq_next(struct seq_file *f, void *v, loff_t *pos)
{
	unsigned long size = ARRAY_SIZE(dss_log->thermal);

	(*pos)++;
	if (*pos >= size)
		return NULL;

	return pos;
}

static void thermal_log_seq_stop(struct seq_file *f, void *v)
{
	/* Nothing to do */
}

static const struct seq_operations thermal_log_fops = {
	.start = thermal_log_seq_start,
	.next  = thermal_log_seq_next,
	.stop  = thermal_log_seq_stop,
	.show  = thermal_log_seq_show,
};

void dbg_snapshot_log_procfs_init(struct proc_dir_entry *parent)
{
	if (!parent) {
		pr_err("%s: parent is NULL\n", __func__);
		return;
	}

	proc_create_seq("thermal_log", 0, parent, &thermal_log_fops);
}
EXPORT_SYMBOL_GPL(dbg_snapshot_log_procfs_init);
#endif /* CONFIG_SEC_PM_LOG */

static void disable_all_log_itmes(void)
{
	int i;

	for (i = 0; i < (int)ARRAY_SIZE(dss_log_items); i++)
		dss_log_items[i].entry.enabled = 0;
}

void dbg_snapshot_init_log(void)
{
	struct dbg_snapshot_item *item = &dss_items[DSS_ITEM_KEVENTS_ID];
	struct dbg_snapshot_log_item *log_item;
	struct device_node *np = dss_desc.dev->of_node;
	struct property *prop;
	const char *str;
	unsigned int i = 0;

	if (!item->entry.enabled || !item->entry.size) {
		disable_all_log_itmes();
		return;
	}

	log_item_set_filed(TASK, task);
	log_item_set_filed(WORK, work);
	log_item_set_filed(CPUIDLE, cpuidle);
	log_item_set_filed(IRQ, irq);
	log_item_set_filed(SPINLOCK, spinlock);
	log_item_set_filed(HRTIMER, hrtimer);
	log_item_set_filed(FREQ, freq);
	log_item_set_filed(SUSPEND, suspend);
	log_item_set_filed(CLK, clk);
	log_item_set_filed(PMU, pmu);
	log_item_set_filed(DM, dm);
	log_item_set_filed(REGULATOR, regulator);
	log_item_set_filed(THERMAL, thermal);
	log_item_set_filed(ACPM, acpm);
	log_item_set_filed(PRINTK, print);

	register_trace_suspend_resume(dbg_snapshot_suspend_resume, NULL);
	register_trace_device_pm_callback_start(dbg_snapshot_dev_pm_cb_start, NULL);
	register_trace_device_pm_callback_end(dbg_snapshot_dev_pm_cb_end, NULL);

	dss_freq_size = of_property_count_strings(np, "freq_names");
	of_property_for_each_string(np, "freq_names", prop, str) {
		strncpy(dss_freq_name[i++], str, strlen(str));
	}

	dbg_snapshot_log_output();
}
