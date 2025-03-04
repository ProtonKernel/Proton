/*
 * Copyright (c) 2012 Samsung Electronics Co., Ltd.
 *		http://www.samsung.com/
 *
 * EXYNOS - PPMU support
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#include <linux/kernel.h>
#include <linux/errno.h>
#include <linux/device.h>
#include <linux/platform_device.h>
#include <linux/types.h>
#include <linux/io.h>
#include <linux/sched/clock.h>
#include <linux/math64.h>
#include <linux/slab.h>

#include "exynos-ppc.h"

#define PMNC			0x0004
#define CNTENS			0x0008
#define CNTENC			0x000C
#define CNTRESET		0x002C
#define CNTAUTO			0x0030
#define PMCNT			0x0034
#define CCNT			0x0048

#define BIT_CHVALUE		((0x1 << 31) | (0x1 << 0))
#define BIT_REGVALUE		(0x1 << 24)
#define BIT_RESETALL		(0x6)
#define BIT_GLBCNTEN		(0x1)

static u64 ppc_dvfs_total_ccnt;
static u64 ppc_dvfs_total_pmcnt;
static u64 ppc_dvfs_total_ccnt_alt;
static u64 ppc_dvfs_total_pmcnt_alt;
ktime_t last_updated_time;

DEFINE_MUTEX(ppc_dvfs_lock);

static void exynos_init_ppc(void __iomem *base)
{
	/* Initialize Q-CH, OP Mode */
	__raw_writel(BIT_REGVALUE | BIT_RESETALL, base + PMNC);

	/* Enable CCNT, PMCNTTx Counting*/
	__raw_writel(BIT_CHVALUE, base + CNTENS);
}

static void exynos_start_ppc(void __iomem *base)
{
	__raw_writel(BIT_REGVALUE | BIT_GLBCNTEN, base + PMNC);
}

static void exynos_stop_ppc(void __iomem *base)
{
	__raw_writel(BIT_REGVALUE, base + PMNC);
}

static void exynos_clear_ppc(void __iomem *base)
{
	__raw_writel(BIT_RESETALL, base + PMNC);
}

static void exynos_reset_ppc(void __iomem *base)
{
	__raw_writel(BIT_REGVALUE | BIT_RESETALL, base + PMNC);
}

static void exynos_read_ppc(struct exynos_devfreq_data *data)
{
	int i;
	struct ppc_data ppc = { 0, };
	int index = 0;

	data->um_data.val_ccnt = 0;
	data->um_data.val_pmcnt = 0;

	for (i = 0; i < data->um_data.um_count; i++) {
		ppc.ccnt = __raw_readl(data->um_data.va_base[i] + CCNT);
		ppc.pmcnt = __raw_readl(data->um_data.va_base[i] + PMCNT);

		if (data->um_data.val_pmcnt < ppc.pmcnt) {
			data->um_data.val_ccnt = ppc.ccnt;
			data->um_data.val_pmcnt = ppc.pmcnt;
			index = i;
		}
	}
}

int exynos_devfreq_um_init(struct exynos_devfreq_data *data)
{
	int i = 0;

	for (i = 0; i < data->um_data.um_count; i++)
		exynos_init_ppc(data->um_data.va_base[i]);

	for (i = 0; i < data->um_data.um_count; i++)
		exynos_start_ppc(data->um_data.va_base[i]);

	return 0;
}

void exynos_devfreq_um_exit(struct exynos_devfreq_data *data)
{
	int i = 0;

	for (i = 0; i < data->um_data.um_count; i++)
		exynos_clear_ppc(data->um_data.va_base[i]);
}

ktime_t exynos_devfreq_get_ppc_status(struct exynos_devfreq_data *data)
{
	int i;
	unsigned int ccnt = 0, pmcnt = 0, max_ccnt = 0, max_pmcnt = 0;
	ktime_t active_time, cur_time;

	mutex_lock(&ppc_dvfs_lock);

	for (i = 0; i < data->um_data.um_count; i++)
		exynos_stop_ppc(data->um_data.va_base[i]);

	/* Read current counter */
	for (i = 0 ; i < data->um_data.um_count; i++) {
		ccnt = __raw_readl(data->um_data.va_base[i] + CCNT);
		pmcnt = __raw_readl(data->um_data.va_base[i] + PMCNT);

		if (max_pmcnt < pmcnt) {
			max_ccnt = ccnt;
			max_pmcnt = pmcnt;
		}
	}

	for (i = 0; i < data->um_data.um_count; i++)
		exynos_start_ppc(data->um_data.va_base[i]);

	cur_time = sched_clock();
	active_time = (cur_time - last_updated_time) * (max_pmcnt + ppc_dvfs_total_pmcnt_alt - ppc_dvfs_total_pmcnt) / (max_ccnt + ppc_dvfs_total_ccnt_alt - ppc_dvfs_total_ccnt);

	ppc_dvfs_total_pmcnt = max_pmcnt + ppc_dvfs_total_pmcnt_alt;
	ppc_dvfs_total_ccnt = max_ccnt + ppc_dvfs_total_ccnt_alt;
	last_updated_time = cur_time;

	mutex_unlock(&ppc_dvfs_lock);

	return active_time;

}

static int exynos_devfreq_get_dev_status(struct device *dev, struct devfreq_dev_status *stat)
{
	struct platform_device *pdev = container_of(dev, struct platform_device, dev);
	struct exynos_devfreq_data *data = platform_get_drvdata(pdev);
	struct exynos_profile_data *profile_data = (struct exynos_profile_data *)stat->private_data;
	u64 cur_time;
	int i;

	if (data->devfreq_disabled)
		return -EAGAIN;

	mutex_lock(&ppc_dvfs_lock);

	cur_time = sched_clock();
	data->last_monitor_period = (cur_time - data->last_monitor_time);
	data->last_monitor_time = cur_time;

	for (i = 0; i < data->um_data.um_count; i++)
		exynos_stop_ppc(data->um_data.va_base[i]);

	/* Read current counter */
	exynos_read_ppc(data);

	stat->current_frequency = data->previous_freq;
	profile_data->busy_time = data->um_data.val_pmcnt;
	profile_data->total_time = data->um_data.val_ccnt;
	profile_data->delta_time = data->last_monitor_period;

	data->last_um_usage_rate = div64_u64(profile_data->busy_time * 100, profile_data->total_time);

	ppc_dvfs_total_ccnt_alt += data->um_data.val_ccnt;
	ppc_dvfs_total_pmcnt_alt += data->um_data.val_pmcnt;

	for (i = 0; i < data->um_data.um_count; i++) {
		exynos_reset_ppc(data->um_data.va_base[i]);
		exynos_start_ppc(data->um_data.va_base[i]);
	}

	mutex_unlock(&ppc_dvfs_lock);

	return 0;
}

void register_get_dev_status(struct exynos_devfreq_data *data)
{
	data->devfreq->last_status.private_data =
		kzalloc(sizeof(struct exynos_profile_data), GFP_KERNEL);
	data->devfreq_profile.get_dev_status = exynos_devfreq_get_dev_status;

	mutex_init(&ppc_dvfs_lock);
}
