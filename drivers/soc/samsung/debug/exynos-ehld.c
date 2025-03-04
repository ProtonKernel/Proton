// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright (c) 2020 Samsung Electronics Co., Ltd.
 *	      http://www.samsung.com/
 *
 * Exynos - Early Hard Lockup Detector
 *
 * Author: Hosung Kim <hosung0.kim@samsung.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#include <linux/platform_device.h>
#include <linux/module.h>
#include <linux/init.h>
#include <linux/smp.h>
#include <linux/errno.h>
#include <linux/suspend.h>
#include <linux/of.h>
#include <linux/of_address.h>
#include <linux/cpu_pm.h>
#include <linux/sched/clock.h>
#include <linux/notifier.h>
#include <linux/kallsyms.h>

#include <soc/samsung/exynos-ehld.h>
#include <soc/samsung/debug-snapshot.h>

#include "core_regs.h"

//#define DEBUG

/* PPC Register */
#define	PPC_PMNC			(0x4)
#define	PPC_CNTENS			(0x8)
#define	PPC_CNTENC			(0xC)
#define PPC_INTENS			(0x10)
#define PPC_INTENC			(0x14)
#define PPC_FLAG			(0x18)
#define PPC_CNT_RESET			(0x2C)

#define PPC_CIG_CFG0			(0x1C)
#define PPC_CIG_CFG1			(0x20)
#define PPC_CIG_CFG2			(0x24)

#define PPC_PMCNT0_LOW			(0x34)
#define PPC_PMCNT0_HIGH			(0x4C)

#define PPC_PMCNT1_LOW			(0x38)
#define PPC_PMCNT1_HIGH			(0x50)

#define PPC_PMCNT2_LOW			(0x3C)
#define PPC_PMCNT2_HIGH			(0x54)

#define PPC_PMCNT3_LOW			(0x40)
#define PPC_PMCNT3_HIGH			(0x44)

#define PPC_CCNT_LOW			(0x48)
#define PPC_CCNT_HIGH			(0x58)

#define PPC_CMCNT0			(1 << 0)
#define PPC_CMCNT1			(1 << 1)
#define PPC_CMCNT2			(1 << 2)
#define PPC_CMCNT3			(1 << 3)
#define PPC_CCNT			(1 << 31)

#ifdef DEBUG
#define ehld_info(f, str...) pr_info(str)
#define ehld_err(f, str...) pr_info(str)
#else
#define ehld_info(f, str...) do { if (f == 1) pr_info(str); } while (0)
#define ehld_err(f, str...)  do { if (f == 1) pr_info(str); } while (0)
#endif

#define MSB_MASKING		(0x0000FF0000000000)
#define MSB_PADDING		(0xFFFFFF0000000000)
#define DBG_UNLOCK(base)	\
	do { isb(); __raw_writel(OSLOCK_MAGIC, base + DBGLAR); } while (0)
#define DBG_LOCK(base)		\
	do { __raw_writel(0x1, base + DBGLAR); isb(); } while (0)
#define DBG_OS_UNLOCK(base)	\
	do { isb(); __raw_writel(0, base + DBGOSLAR); } while (0)
#define DBG_OS_LOCK(base)	\
	do { __raw_writel(0x1, base + DBGOSLAR); isb(); } while (0)

extern struct atomic_notifier_head hardlockup_notifier_list;
extern struct atomic_notifier_head hardlockup_handler_notifier_list;

struct ehld_dbgc {
	unsigned int			support;
	unsigned int			enabled;
	unsigned int			interval;
	unsigned int			warn_count;
};

struct ehld_dev {
	struct device			*dev;
	struct ehld_dbgc		dbgc;
	unsigned int			cs_base;
	void __iomem			*reg_instret[2];
	void __iomem			*reg_instrun[2];
	struct cpumask			cpu_mask;
	int				sjtag_en;
};

struct ehld_ctrl {
	struct ehld_data		data;
	void __iomem			*reg_instret;
	void __iomem			*reg_instrun;
	void __iomem			*dbg_base;
	raw_spinlock_t			lock;
	bool				in_c2;
};

static struct ehld_ctrl __percpu *ehld_ctrl;
static struct ehld_dev *ehld;

struct ehld_data *ehld_get_ctrl_data(int cpu)
{
	struct ehld_ctrl *ctrl = per_cpu_ptr(ehld_ctrl, cpu);

	return &ctrl->data;
}
EXPORT_SYMBOL(ehld_get_ctrl_data);

static void ehld_event_update(int cpu)
{
	struct ehld_ctrl *ctrl;
	struct ehld_data *data;
	unsigned long long val;
	unsigned long flags, count;

	ctrl = per_cpu_ptr(ehld_ctrl, cpu);

	raw_spin_lock_irqsave(&ctrl->lock, flags);
	data = &ctrl->data;
	count = ++data->data_ptr & (NUM_TRACE - 1);
	data->time[count] = cpu_clock(cpu);
	data->instret[count] = readl(ctrl->reg_instret +
				(PPC_PMCNT0_LOW + ((cpu % SZ_4) * SZ_4)));
	data->instrun[count] = readl(ctrl->reg_instrun +
				(PPC_PMCNT0_LOW + ((cpu % SZ_4) * SZ_4)));
	if (cpu_is_offline(cpu) || ctrl->in_c2 || ehld->sjtag_en) {
		data->pmpcsr[count] = 0xC2;
	} else {
		DBG_UNLOCK(ctrl->dbg_base + PMU_OFFSET);
		val = __raw_readq(ctrl->dbg_base + PMU_OFFSET + PMUPCSR);
		val = __raw_readq(ctrl->dbg_base + PMU_OFFSET + PMUPCSR);
		if (MSB_MASKING == (MSB_MASKING & val))
			val |= MSB_PADDING;
		data->pmpcsr[count] = val;
		DBG_LOCK(ctrl->dbg_base + PMU_OFFSET);
	}

	ehld_info(0, "%s: cpu%d: time:%llu, instret:0x%llx, instrun:0x%llx, pmpcsr:%x, c2:%ld\n",
			__func__, cpu,
			data->time[count],
			data->instret[count],
			data->instrun[count],
			data->pmpcsr[count],
			ctrl->in_c2);

	raw_spin_unlock_irqrestore(&ctrl->lock, flags);
}

void ehld_event_update_allcpu(void)
{
	int cpu;

	for_each_cpu(cpu, &ehld->cpu_mask)
		ehld_event_update(cpu);
}
EXPORT_SYMBOL(ehld_event_update_allcpu);

static void ehld_event_dump(int cpu, bool header)
{
	struct ehld_ctrl *ctrl;
	struct ehld_data *data;
	unsigned long flags, count;
	int i;
	char symname[KSYM_NAME_LEN];


	if (header) {
		ehld_info(1, "--------------------------------------------------------------------------\n");
		ehld_info(1, "  Exynos Early Lockup Detector Information\n\n");
		ehld_info(1, "  CPU NUM TIME             INSTRET           INSTRUN           PC\n\n");
	}

	symname[KSYM_NAME_LEN - 1] = '\0';
	ctrl = per_cpu_ptr(ehld_ctrl, cpu);
	raw_spin_lock_irqsave(&ctrl->lock, flags);
	data = &ctrl->data;
	i = 0;
	do {
		count = ++data->data_ptr & (NUM_TRACE - 1);
		symname[KSYM_NAME_LEN - 1] = '\0';
		i++;

		if (sprint_symbol(symname, data->pmpcsr[count]) < 0)
			symname[0] = '\0';

		ehld_info(1, "    %01d  %02d %015llu  0x%015llx  0x%015llx  0x%016llx(%s)\n",
				cpu, i,
				(unsigned long long)data->time[count],
				(unsigned long long)data->instret[count],
				(unsigned long long)data->instrun[count],
				(unsigned long long)data->pmpcsr[count],
				symname);

		if (i >= NUM_TRACE)
			break;
	} while (1);
	raw_spin_unlock_irqrestore(&ctrl->lock, flags);
	ehld_info(1, "--------------------------------------------------------------------------\n");
}

void ehld_event_dump_allcpu(void)
{
	int cpu;

	ehld_info(1, "--------------------------------------------------------------------------\n");
	ehld_info(1, "  Exynos Early Lockup Detector Information\n\n");
	ehld_info(1, "  CPU NUM TIME             INSTRET           INSTRUN           PC\n\n");

	for_each_cpu(cpu, &ehld->cpu_mask)
		ehld_event_dump(cpu, false);
}

void ehld_prepare_panic(void)
{
	ehld->dbgc.support = false;
}

void ehld_do_action(int cpu, unsigned int lockup_level)
{
	switch (lockup_level) {
	case EHLD_STAT_LOCKUP_WARN:
		ehld_event_dump(cpu, true);
		break;
	case EHLD_STAT_LOCKUP_SW:
	case EHLD_STAT_LOCKUP_HW:
		ehld_event_dump(cpu, true);
		panic("Watchdog detected hard LOCKUP on cpu %u by EHLD", cpu);
		break;
	}
}
EXPORT_SYMBOL(ehld_do_action);

static int ehld_hardlockup_handler(struct notifier_block *nb,
					   unsigned long l, void *p)
{
	ehld_event_dump_allcpu();

	return 0;
}

static struct notifier_block ehld_hardlockup_block = {
	.notifier_call = ehld_hardlockup_handler,
};

/* This callback is called when every 4s in each cpu */
static int ehld_hardlockup_callback_handler(struct notifier_block *nb,
						unsigned long l, void *p)
{
	int *cpu = (int *)p;

	ehld_info(0, "%s: cpu%d\n", __func__, *cpu);

	ehld_event_update_allcpu();

	return 0;
}

static struct notifier_block ehld_hardlockup_handler_block = {
		.notifier_call = ehld_hardlockup_callback_handler,
};

static int ehld_panic_handler(struct notifier_block *nb,
				unsigned long l, void *p)
{
	ehld_event_dump_allcpu();

	return 0;
}

static struct notifier_block ehld_panic_block = {
	.notifier_call = ehld_panic_handler,
};

static int ehld_c2_pm_notifier(struct notifier_block *self,
				unsigned long action, void *v)
{
	int cpu = raw_smp_processor_id();
	struct ehld_ctrl *ctrl = per_cpu_ptr(ehld_ctrl, cpu);
	unsigned long flags;

	raw_spin_lock_irqsave(&ctrl->lock, flags);

	switch (action) {
	case CPU_PM_ENTER:
	case CPU_CLUSTER_PM_ENTER:
		ctrl->in_c2 = true;
		break;
        case CPU_PM_ENTER_FAILED:
        case CPU_PM_EXIT:
	case CPU_CLUSTER_PM_ENTER_FAILED:
	case CPU_CLUSTER_PM_EXIT:
		ctrl->in_c2 = false;
		break;
	}

	raw_spin_unlock_irqrestore(&ctrl->lock, flags);

	return NOTIFY_OK;
}

static struct notifier_block ehld_c2_pm_nb = {
	.notifier_call = ehld_c2_pm_notifier,
};

static void ehld_setup(void)
{
	/* register cpu pm notifier for C2 */
	cpu_pm_register_notifier(&ehld_c2_pm_nb);

	/* panic_notifier_list */
	atomic_notifier_chain_register(&panic_notifier_list,
					&ehld_panic_block);
	/* hardlockup_notifier_list */
	atomic_notifier_chain_register(&hardlockup_notifier_list,
					&ehld_hardlockup_block);
	/* hardlockup_handler_notifier_list */
	atomic_notifier_chain_register(&hardlockup_handler_notifier_list,
					&ehld_hardlockup_handler_block);

	ehld->sjtag_en = dbg_snapshot_get_sjtag_status();
}

static int ehld_init_dt_parse(struct device_node *np)
{
	struct device_node *child;
	int ret = 0, cpu = 0;
	unsigned int offset, base;
	char name[SZ_16];
	struct ehld_ctrl *ctrl;

	if (of_property_read_u32(np, "cs_base", &base)) {
		ehld_info(1, "ehld: no coresight base address\n");
		return -EINVAL;
	}
	ehld->cs_base = base;

	ehld->reg_instret[0] = of_iomap(np, 0);
	if (!ehld->reg_instret[0]) {
		ehld_info(1, "ehld: no instret reg[0] address\n");
		return -EINVAL;
	}

	ehld->reg_instret[1] = of_iomap(np, 1);
	if (!ehld->reg_instret[1]) {
		ehld_info(1, "ehld: no instret reg[1] address\n");
		return -EINVAL;
	}

	ehld->reg_instrun[0] = of_iomap(np, 2);
	if (!ehld->reg_instrun[0]) {
		ehld_info(1, "ehld: no instrun reg[0] address\n");
		return -EINVAL;
	}

	ehld->reg_instrun[1] = of_iomap(np, 3);
	if (!ehld->reg_instrun[1]) {
		ehld_info(1, "ehld: no instrun reg[1] address\n");
		return -EINVAL;
	}

	child = of_get_child_by_name(np, "dbgc");
	if (!child) {
		ehld_info(1, "ehld: dbgc dt is not supported\n");
		return -EINVAL;
	}

	if (!of_property_read_u32(child, "interval", &base)) {
		ehld->dbgc.interval = base;
		ehld_info(1, "Support dbgc interval:%u ms\n", base);
	}
	if (!of_property_read_u32(child, "warn-count", &base)) {
		ehld->dbgc.warn_count = base;
		ehld_info(1, "Support dbgc warnning count: %u\n", base);
	}
	if (adv_tracer_ehld_init((void *)child) < 0) {
		ehld_info(1, "ehld: failed to init ehld ipc driver\n");
		return -EINVAL;
	}

	of_node_put(child);

	ehld_ctrl = alloc_percpu(struct ehld_ctrl);
	if (!ehld_ctrl) {
		ehld_err(1, "ehld: alloc_percpu is failed\n", cpu);
		return -ENOMEM;
	}

	for_each_possible_cpu(cpu) {
		snprintf(name, sizeof(name), "cpu%d", cpu);
		child = of_get_child_by_name(np, (const char *)name);

		if (!child) {
			ehld_err(1, "ehld: no cpu dbg info - cpu%d\n", cpu);
			return -EINVAL;
		}

		ret = of_property_read_u32(child, "dbg-offset", &offset);
		if (ret) {
			ehld_err(1, "ehld: no cpu dbg-offset info - cpu%d\n", cpu);
			return -EINVAL;
		}

		ctrl = per_cpu_ptr(ehld_ctrl, cpu);
		ctrl->dbg_base = ioremap(ehld->cs_base + offset, SZ_256K);

		if (!ctrl->dbg_base) {
			ehld_err(1, "ehld: fail ioremap for dbg_base of cpu%d\n", cpu);
			return -ENOMEM;
		}

		memset((void *)&ctrl->data, 0, sizeof(struct ehld_data));
		raw_spin_lock_init(&ctrl->lock);

		if (cpu < 4) {
			ctrl->reg_instret = ehld->reg_instret[0];
			ctrl->reg_instrun = ehld->reg_instrun[0];
		} else {
			ctrl->reg_instret = ehld->reg_instret[1];
			ctrl->reg_instrun = ehld->reg_instrun[1];
		}

		ehld_info(1, "ehld: cpu#%d, dbg_base:0x%x, total:0x%x, ioremap:0x%lx\n",
				cpu, offset, ehld->cs_base + offset,
				(unsigned long)ctrl->dbg_base);

		of_node_put(child);
	}

	return 0;
}

static int ehld_dbgc_setup(void)
{
	int val, cpu;
	u32 online_mask = 0;

	cpumask_copy(&ehld->cpu_mask, cpu_possible_mask);

	for_each_cpu(cpu, &ehld->cpu_mask)
		online_mask |= BIT(cpu);

	val = adv_tracer_ehld_set_init_val(ehld->dbgc.interval,
				ehld->dbgc.warn_count,
				online_mask);
	if (val < 0)
		return -EINVAL;

	val = adv_tracer_ehld_set_enable(true);
	if (val < 0)
		return -EINVAL;

	val = adv_tracer_ehld_get_enable();
	if (val >= 0)
		ehld->dbgc.enabled = val;
	else
		return -EINVAL;

	return 0;
}

static int ehld_probe(struct platform_device *pdev)
{
	int err;
	struct ehld_dev *edev;

	edev = devm_kzalloc(&pdev->dev,
				sizeof(struct ehld_dev), GFP_KERNEL);
	if (!edev)
		return -ENOMEM;

	edev->dev = &pdev->dev;

	platform_set_drvdata(pdev, edev);

	ehld = edev;

	err = ehld_init_dt_parse(pdev->dev.of_node);
	if (err) {
		ehld_err(1, "ehld: fail setup to parse dt:%d\n", err);
		return err;
	}

	ehld_setup();

	err = ehld_dbgc_setup();
	if (err) {
		ehld_err(1, "ehld: fail setup for ehld dbgc:%d\n", err);
		return err;
	}

	ehld_info(1, "ehld: success to initialize\n");
	return 0;
}

static int ehld_remove(struct platform_device *pdev)
{
	return 0;
}

static int ehld_suspend(struct device *dev)
{
	int val = 0;

	if (ehld->dbgc.enabled)
		val = adv_tracer_ehld_set_enable(false);

	if (val < 0)
		ehld_err(1, "ehld: failed to suspend:%d\n", val);

	return 0;
}

static int ehld_resume(struct device *dev)
{
	int val = 0;

	if (ehld->dbgc.enabled)
		val = adv_tracer_ehld_set_enable(true);

	if (val < 0)
		ehld_err(1, "ehld: failed to resume:%d\n", val);

	return 0;
}

static const struct dev_pm_ops ehld_pm_ops = {
	.suspend = ehld_suspend,
	.resume = ehld_resume,
};

static const struct of_device_id ehld_dt_match[] = {
	{ .compatible	= "samsung,exynos-ehld", },
	{},
};
MODULE_DEVICE_TABLE(of, ehld_dt_match);

static struct platform_driver ehld_driver = {
	.probe = ehld_probe,
	.remove = ehld_remove,
	.driver = {
		.name = "ehld",
		.owner = THIS_MODULE,
		.of_match_table = of_match_ptr(ehld_dt_match),
		.pm = &ehld_pm_ops,
	},
};
module_platform_driver(ehld_driver);

MODULE_DESCRIPTION("Samsung Early Hard Lockup Detector");
MODULE_LICENSE("GPL v2");
MODULE_AUTHOR("Hosung Kim <hosung0.kim@samsung.com>");
