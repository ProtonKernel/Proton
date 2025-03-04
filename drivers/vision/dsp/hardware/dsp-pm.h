/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Samsung Exynos SoC series dsp driver
 *
 * Copyright (c) 2019 Samsung Electronics Co., Ltd.
 *              http://www.samsung.com/
 */

#ifndef __HW_DSP_PM_H__
#define __HW_DSP_PM_H__

#include <linux/pm_runtime.h>
#include <linux/pm_qos.h>
#include <linux/cpufreq.h>
#include <linux/kthread.h>
#include <linux/spinlock.h>
#include <linux/version.h>
#if IS_ENABLED(CONFIG_EXYNOS_PM_QOS)
#include <soc/samsung/exynos_pm_qos.h>
#endif

#include "hardware/dsp-governor.h"

#define DSP_DEVFREQ_NAME_LEN		(10)
#define DSP_DEVFREQ_RESERVED_NUM	(16)

struct dsp_system;
struct dsp_pm;

enum dsp_pm_qos_level {
	DSP_PM_QOS_L0,
	DSP_PM_QOS_L1,
	DSP_PM_QOS_L2,
	DSP_PM_QOS_L3,
	DSP_PM_QOS_L4,
	DSP_PM_QOS_L5,
	DSP_PM_QOS_L6,
	DSP_PM_QOS_L7,
	DSP_PM_QOS_L8,
	DSP_PM_QOS_L9,
	DSP_PM_QOS_L10,
	DSP_PM_QOS_L11,
	DSP_PM_QOS_L12,
	DSP_PM_QOS_L13,
	DSP_PM_QOS_L14,
	DSP_PM_QOS_L15,
	DSP_PM_QOS_L16,
	DSP_PM_QOS_L17,
	DSP_PM_QOS_L18,
	DSP_PM_QOS_L19,
	DSP_PM_QOS_L20,
	DSP_PM_QOS_L21,
	DSP_PM_QOS_L22,
	DSP_PM_QOS_L23,
	DSP_PM_QOS_L24,
	DSP_PM_QOS_L25,
	DSP_PM_QOS_L26,
	DSP_PM_QOS_L27,
	DSP_PM_QOS_L28,
	DSP_PM_QOS_L29,
	DSP_PM_QOS_L30,
	DSP_PM_QOS_DEFAULT = DSP_GOVERNOR_DEFAULT,
};

struct dsp_pm_devfreq {
	unsigned int			id;
	char				name[DSP_DEVFREQ_NAME_LEN];
#if IS_ENABLED(CONFIG_EXYNOS_PM_QOS)
	struct exynos_pm_qos_request	req;
#else
	struct pm_qos_request		req;
#endif
	bool				use_freq_qos;
	struct cpufreq_policy		*policy;
#if KERNEL_VERSION(5, 4, 0) <= LINUX_VERSION_CODE
	struct freq_qos_request		freq_qos_req;
#endif
	int				count;
	unsigned int			*table;
	int				class_id;
	int				boot_qos;
	int				dynamic_qos;
	unsigned int			dynamic_total_count;
	unsigned int			dynamic_count[DSP_DEVFREQ_RESERVED_NUM];
	int				static_qos;
	unsigned int			static_total_count;
	unsigned int			static_count[DSP_DEVFREQ_RESERVED_NUM];
	int				current_qos;
	int				force_qos;
	int				min_qos;

	struct kthread_work		work;
	spinlock_t			slock;
	int				async_status;
	int				async_qos;

	struct dsp_pm			*pm;
};

struct dsp_pm_ops {
	int (*devfreq_active)(struct dsp_pm *pm);
	int (*update_devfreq_nolock)(struct dsp_pm *pm, int id, int val);
	int (*update_devfreq_nolock_async)(struct dsp_pm *pm, int id, int val);
	int (*update_extra_devfreq)(struct dsp_pm *pm, int id, int val);
	int (*set_force_qos)(struct dsp_pm *pm, int id, int val);
	int (*dvfs_enable)(struct dsp_pm *pm, int val);
	int (*dvfs_disable)(struct dsp_pm *pm, int val);
	int (*update_devfreq_busy)(struct dsp_pm *pm, int val);
	int (*update_devfreq_idle)(struct dsp_pm *pm, int val);
	int (*update_devfreq_boot)(struct dsp_pm *pm);
	int (*update_devfreq_max)(struct dsp_pm *pm);
	int (*update_devfreq_min)(struct dsp_pm *pm);
	int (*set_boot_qos)(struct dsp_pm *pm, int val);
	int (*boost_enable)(struct dsp_pm *pm);
	int (*boost_disable)(struct dsp_pm *pm);
	int (*enable)(struct dsp_pm *pm);
	int (*disable)(struct dsp_pm *pm);

	int (*open)(struct dsp_pm *pm);
	int (*close)(struct dsp_pm *pm);
	int (*probe)(struct dsp_system *sys);
	void (*remove)(struct dsp_pm *pm);
};

struct dsp_pm {
	struct mutex			lock;
	struct dsp_pm_devfreq		*devfreq;
	bool				dvfs;
	unsigned int			dvfs_disable_count;
	bool				dvfs_lock;
	struct dsp_pm_devfreq		*extra_devfreq;
	struct kthread_worker		async_worker;
	struct task_struct		*async_task;

	const struct dsp_pm_ops		*ops;
	struct dsp_system		*sys;
};

int dsp_pm_set_ops(struct dsp_pm *pm, unsigned int dev_id);
int dsp_pm_register_ops(unsigned int dev_id, const struct dsp_pm_ops *ops);

#endif
