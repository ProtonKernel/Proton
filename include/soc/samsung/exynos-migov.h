/* include/soc/samsung/exynos-migov.h
 *
 * Copyright (C) 2020 Samsung Electronics Co., Ltd.
 *		http://www.samsung.com
 *
 * EXYNOS - Header file for Exynos Multi IP Governor support
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#ifndef __EXYNOS_MIGOV_H
#define __EXYNOS_MIGOV_H

//#include <soc/samsung/exynos_pm_qos.h>

#include <dt-bindings/soc/samsung/exynos-migov.h>

/* SHOULD SYNC-UP with definition in "dt-bindings/../exynos-migov.h" */
static char *domain_name[] = {
	"cl0",
	"cl1",
	"cl2",
	"gpu",
	"mif",
};

struct private_fn_cpu {
	s32 (*cpu_active_pct)(s32 id, s32 *cpu_active_pct);
	s32 (*cpu_asv_ids)(s32 id);
};

struct private_fn_gpu {
	u64 (*get_q_empty_pct)(s32 type);
	u64 (*get_input_nr_avg_cnt)(void);
};

struct private_fn_mif {
	u64 (*get_stats0_sum)(void);
	u64 (*get_stats0_avg)(void);
	u64 (*get_stats1_sum)(void);
	u64 (*get_stats_ratio)(void);
	u64 (*get_llc_status)(void);
};

struct domain_fn {
	u32 (*get_table_cnt)(s32 id);
	u32 (*get_freq_table)(s32 id, u32 *table);
	u32 (*get_max_freq)(s32 id);
	u32 (*get_min_freq)(s32 id);
	u32 (*get_freq)(s32 id);
	void (*get_power)(s32 id, u64 *dyn_power, u64 *st_power);
	void (*get_power_change)(s32 id, s32 freq_delta_ratio,
		u32 *freq, u64 *dyn_power, u64 *st_power);
	u32 (*get_active_pct)(s32 id);
	s32 (*get_temp)(s32 id);
	void (*set_margin)(s32, s32 margin);
	u32 (*update_mode)(s32 id, int mode);
};

extern int exynos_migov_register_domain(int id, struct domain_fn *fn, void *pd_fn);
extern void exynos_migov_register_frame_cnt(void (*fn)(u64 *cnt, ktime_t *time));
extern void exynos_migov_register_fence_cnt(void (*fn)(u64 *cnt, ktime_t *time));
extern void exynos_migov_register_vsync_cnt(void (*fn)(u64 *cnt, ktime_t *time));
extern void migov_update_fps_change(u32 new_fps);

#endif /* __EXYNOS_MIGOV_H */
