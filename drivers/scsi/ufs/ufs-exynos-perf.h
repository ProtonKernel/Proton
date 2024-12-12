/*
 * IO Performance mode with UFS
 *
 * Copyright (C) 2019 Samsung Electronics Co., Ltd.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * Authors:
 *	Kiwoong <kwmad.kim@samsung.com>
 */
#ifndef _UFS_PERF_H_
#define _UFS_PERF_H_

#include <linux/types.h>
#include <linux/kthread.h>
#include <linux/completion.h>
#include <linux/spinlock.h>
#include <linux/sched/clock.h>
#include "ufshcd.h"
#if IS_ENABLED(CONFIG_CPU_FREQ)
#include <linux/cpufreq.h>
#endif
#if IS_ENABLED(CONFIG_EXYNOS_PM_QOS)
#include <soc/samsung/exynos_pm_qos.h>
#endif
#if IS_ENABLED(CONFIG_ARM_FREQ_QOS_TRACER)
#include <soc/samsung/freq-qos-tracer.h>
#endif

enum ufs_perf_op {
	UFS_PERF_OP_NONE = 0,
	UFS_PERF_OP_R,
	UFS_PERF_OP_W,
	UFS_PERF_OP_MAX,
};

enum ufs_perf_ctrl {
	UFS_PERF_CTRL_NONE = 0,	/* Not used to run handler */
	UFS_PERF_CTRL_LOCK,
	UFS_PERF_CTRL_RELEASE,
};

struct ufs_perf_control {
	/* from device tree */
	u32 th_chunk_in_kb;	/* big vs little */
	u32 th_count_b;		/* count for big chunk */
	u32 th_count_l;		/* count for little chunk */
	u32 th_period_in_ms_b;	/* period for big chunk */
	u32 th_period_in_ms_l;	/* period for little chunk */
	u32 th_reset_in_ms;	/* timeout for reset */

	u32 th_count_b_r_cont;

#if IS_ENABLED(CONFIG_EXYNOS_PM_QOS)
	struct exynos_pm_qos_request	pm_qos_int;
	s32			pm_qos_int_value;
	struct exynos_pm_qos_request	pm_qos_mif;
	s32			pm_qos_mif_value;
#endif

#if IS_ENABLED(CONFIG_CPU_FREQ) || IS_ENABLED(CONFIG_ARM_FREQ_QOS_TRACER)
	struct notifier_block cpufreq_nb;
	struct freq_qos_request qos_req_cpu_cl0;
	s32			pm_qos_cluster0_value;
	struct freq_qos_request qos_req_cpu_cl1;
	s32			pm_qos_cluster1_value;
	struct freq_qos_request qos_req_cpu_cl2;
	s32			pm_qos_cluster2_value;

#define CL0			0 /* LIT CPU */
#define CL1			4 /* MID CPU */
#define CL2			7 /* BIG CPU */
#endif

	/* spin lock */
	spinlock_t lock;

	/* Control factors */
	bool is_locked;
	bool is_held;
	bool will_stop;

	/* Control factors, need to care for concurrency */
	u32 count_b;		/* big chunk */
	u32 count_l;		/* little chunk */
	u32 count_b_r_cont;	/* little chunk */
	s64 cp_time_b;		/* last check point time */
	s64 cp_time_l;		/* last check point time */
	s64 cur_time;		/* current point time */

	u32 ctrl_flag;
	u32 ctrl_flag_in_transit;

	struct task_struct *handler;	/* thread for PM QoS */
	bool is_active;			/* handler status */
	struct completion completion;	/* wake-up source */
	struct timer_list reset_timer;	/* stat reset timer */

	/* handles */
	struct ufs_hba *hba;
};

/* EXTERNAL FUNCTIONS */
void ufs_perf_reset(void *data, struct ufs_hba *hba, bool boot);
void ufs_perf_update_stat(void *data, unsigned int len, enum ufs_perf_op op);
void ufs_perf_populate_dt(void *data, struct device_node *np);
bool ufs_perf_init(void **data, struct device *dev);
void ufs_perf_exit(void *data);

#endif /* _UFS_PERF_H_ */
