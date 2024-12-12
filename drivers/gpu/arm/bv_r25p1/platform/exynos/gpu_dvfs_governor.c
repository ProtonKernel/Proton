/* drivers/gpu/arm/.../platform/gpu_dvfs_governor.c
 *
 * Copyright 2011 by S.LSI. Samsung Electronics Inc.
 * San#24, Nongseo-Dong, Giheung-Gu, Yongin, Korea
 *
 * Samsung SoC Mali-T Series DVFS driver
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software FoundatIon.
 */

/**
 * @file gpu_dvfs_governor.c
 * DVFS
 */
#include <mali_kbase.h>

#include "mali_kbase_platform.h"
#include "gpu_dvfs_handler.h"
#include "gpu_dvfs_governor.h"
#include "gpu_dvfs_api.h"
#ifdef CONFIG_CPU_THERMAL_IPA
#include "gpu_ipa.h"
#endif /* CONFIG_CPU_THERMAL_IPA */

#ifdef CONFIG_MALI_DVFS
typedef int (*GET_NEXT_LEVEL)(struct exynos_context *platform, int utilization);
GET_NEXT_LEVEL gpu_dvfs_get_next_level;



static int gpu_dvfs_governor_default(struct exynos_context *platform, int utilization);
static int gpu_dvfs_governor_interactive(struct exynos_context *platform, int utilization);
#ifdef CONFIG_MALI_TSG
static int gpu_dvfs_governor_joint(struct exynos_context *platform, int utilization);
#endif
static int gpu_dvfs_governor_static(struct exynos_context *platform, int utilization);
static int gpu_dvfs_governor_booster(struct exynos_context *platform, int utilization);
static int gpu_dvfs_governor_dynamic(struct exynos_context *platform, int utilization);

static gpu_dvfs_governor_info governor_info[G3D_MAX_GOVERNOR_NUM] = {
	{
		G3D_DVFS_GOVERNOR_DEFAULT,
		"Default",
		gpu_dvfs_governor_default,
		NULL
	},
	{
		G3D_DVFS_GOVERNOR_INTERACTIVE,
		"Interactive",
		gpu_dvfs_governor_interactive,
		NULL
	},
#ifdef CONFIG_MALI_TSG
	{
		G3D_DVFS_GOVERNOR_JOINT,
		"Joint",
		gpu_dvfs_governor_joint,
		NULL
	},
#endif
	{
		G3D_DVFS_GOVERNOR_STATIC,
		"Static",
		gpu_dvfs_governor_static,
		NULL
	},
	{
		G3D_DVFS_GOVERNOR_BOOSTER,
		"Booster",
		gpu_dvfs_governor_booster,
		NULL
	},
	{
		G3D_DVFS_GOVERNOR_DYNAMIC,
		"Dynamic",
		gpu_dvfs_governor_dynamic,
		NULL
	},
};

void gpu_dvfs_update_start_clk(int governor_type, int clk)
{
	governor_info[governor_type].start_clk = clk;
}

void gpu_dvfs_update_table(int governor_type, gpu_dvfs_info *table)
{
	governor_info[governor_type].table = table;
}

void gpu_dvfs_update_table_size(int governor_type, int size)
{
	governor_info[governor_type].table_size = size;
}

void *gpu_dvfs_get_governor_info(void)
{
	return &governor_info;
}

static int gpu_dvfs_governor_default(struct exynos_context *platform, int utilization)
{
	DVFS_ASSERT(platform);

	if ((platform->step > gpu_dvfs_get_level(platform->gpu_max_clock)) &&
			(utilization > platform->table[platform->step].max_threshold)) {
		platform->step--;
		if (platform->table[platform->step].clock > platform->gpu_max_clock_limit)
			platform->step = gpu_dvfs_get_level(platform->gpu_max_clock_limit);
		platform->down_requirement = platform->table[platform->step].down_staycount;
	} else if ((platform->step < gpu_dvfs_get_level(platform->gpu_min_clock)) && (utilization < platform->table[platform->step].min_threshold)) {
		platform->down_requirement--;
		if (platform->down_requirement == 0) {
			platform->step++;
			platform->down_requirement = platform->table[platform->step].down_staycount;
		}
	} else {
		platform->down_requirement = platform->table[platform->step].down_staycount;
	}
	DVFS_ASSERT((platform->step >= gpu_dvfs_get_level(platform->gpu_max_clock))
					&& (platform->step <= gpu_dvfs_get_level(platform->gpu_min_clock)));

	return 0;
}

static int gpu_dvfs_governor_interactive(struct exynos_context *platform, int utilization)
{
	int util;
	DVFS_ASSERT(platform);

	util = platform->interactive_fix == 0 ?
			utilization :
			utilization + (platform->freq_margin / 10);

	if ((platform->step > gpu_dvfs_get_level(platform->gpu_max_clock) ||
	    (platform->using_max_limit_clock && platform->step > gpu_dvfs_get_level(platform->gpu_max_clock_limit)))
			&& (util > platform->table[platform->step].max_threshold)) {
		int highspeed_level = gpu_dvfs_get_level(platform->interactive.highspeed_clock);
		if ((highspeed_level > 0) && (platform->step > highspeed_level)
				&& (util > platform->interactive.highspeed_load)) {
			if (platform->interactive.delay_count == platform->interactive.highspeed_delay) {
				platform->step = highspeed_level;
				platform->interactive.delay_count = 0;
			} else {
				platform->interactive.delay_count++;
			}
		} else {
			platform->step--;
			platform->interactive.delay_count = 0;
		}
		if (platform->table[platform->step].clock > platform->gpu_max_clock_limit)
			platform->step = gpu_dvfs_get_level(platform->gpu_max_clock_limit);
		platform->down_requirement = platform->table[platform->step].down_staycount;
	} else if ((platform->step < gpu_dvfs_get_level(platform->gpu_min_clock))
			&& (util < platform->table[platform->step].min_threshold)) {
		platform->interactive.delay_count = 0;
		platform->down_requirement--;
		if (platform->down_requirement == 0) {
			platform->step++;
			platform->down_requirement = platform->table[platform->step].down_staycount;
		}
	} else {
		platform->interactive.delay_count = 0;
		platform->down_requirement = platform->table[platform->step].down_staycount;
	}

	DVFS_ASSERT(((platform->using_max_limit_clock && (platform->step >= gpu_dvfs_get_level(platform->gpu_max_clock_limit))) ||
			((!platform->using_max_limit_clock && (platform->step >= gpu_dvfs_get_level(platform->gpu_max_clock)))))
			&& (platform->step <= gpu_dvfs_get_level(platform->gpu_min_clock)));

	return 0;
}

#ifdef CONFIG_MALI_TSG
extern struct kbase_device *pkbdev;
static int gpu_dvfs_governor_joint(struct exynos_context *platform, int utilization)
{
	int i;
	int min_value;
	int weight_util;
	int utilT;
	int weight_fmargin_clock;
	int next_clock;
	int diff_clock;

	DVFS_ASSERT(platform);

	min_value = platform->gpu_max_clock;
	next_clock = platform->cur_clock;

	weight_util = gpu_weight_prediction_utilisation(platform, utilization);
	utilT = ((long long)(weight_util) * platform->cur_clock / 100) >> 10;
	weight_fmargin_clock = utilT + ((platform->gpu_max_clock - utilT) / 1000) * platform->freq_margin;

	if (weight_fmargin_clock > platform->gpu_max_clock) {
		platform->step = gpu_dvfs_get_level(platform->gpu_max_clock);
	} else if (weight_fmargin_clock < platform->gpu_min_clock) {
		platform->step = gpu_dvfs_get_level(platform->gpu_min_clock);
	} else {
		for (i = gpu_dvfs_get_level(platform->gpu_max_clock); i <= gpu_dvfs_get_level(platform->gpu_min_clock); i++) {
			diff_clock = (platform->table[i].clock - weight_fmargin_clock);
			if (diff_clock < min_value) {
				if (diff_clock >= 0) {
					min_value = diff_clock;
					next_clock = platform->table[i].clock;
				} else {
					break;
				}
			}
		}
		platform->step = gpu_dvfs_get_level(next_clock);
	}

	GPU_LOG(DVFS_DEBUG, DUMMY, 0u, 0u, "%s: F_margin[%d] weight_util[%d] utilT[%d] weight_fmargin_clock[%d] next_clock[%d], step[%d]\n",
			__func__, platform->freq_margin, weight_util, utilT, weight_fmargin_clock, next_clock, platform->step);

	DVFS_ASSERT((platform->step >= gpu_dvfs_get_level(platform->gpu_max_clock))
			&& (platform->step <= gpu_dvfs_get_level(platform->gpu_min_clock)));

	return 0;
}

#define weight_table_size 12
#define WEIGHT_TABLE_MAX_IDX 11
int gpu_weight_prediction_utilisation(struct exynos_context *platform, int utilization)
{
	int i;
	int idx;
	int t_window = weight_table_size;
	static int weight_sum[2] = {0, 0};
	static int weight_table_idx[2];
	int weight_table[WEIGHT_TABLE_MAX_IDX][weight_table_size] = {
											{  48,  44,  40,  36,  32,  28,  24,  20,  16,  12,   8,   4},
											{ 100,  10,   1,   0,   0,   0,   0,   0,   0,   0,   0,   0},
											{ 200,  40,   8,   2,   1,   0,   0,   0,   0,   0,   0,   0},
											{ 300,  90,  27,   8,   2,   1,   0,   0,   0,   0,   0,   0},
											{ 400, 160,  64,  26,  10,   4,   2,   1,   0,   0,   0,   0},
											{ 500, 250, 125,  63,  31,  16,   8,   4,   2,   1,   0,   0},
											{ 600, 360, 216, 130,  78,  47,  28,  17,  10,   6,   4,   2},
											{ 700, 490, 343, 240, 168, 118,  82,  58,  40,  28,  20,  14},
											{ 800, 640, 512, 410, 328, 262, 210, 168, 134, 107,  86,  69},
											{ 900, 810, 729, 656, 590, 531, 478, 430, 387, 349, 314, 282},
											{  48,  44,  40,  36,  32,  28,  24,  20,  16,  12,   8,   4}
										};
	int normalized_util;
	int util_conv;
	int table_idx[2];

	DVFS_ASSERT(platform);

	table_idx[0] = platform->weight_table_idx_0;
	table_idx[1] = platform->weight_table_idx_1;

	if ((weight_table_idx[0] != table_idx[0]) || (weight_table_idx[1] != table_idx[1])) {
		weight_table_idx[0] = (table_idx[0] < WEIGHT_TABLE_MAX_IDX) ?
								table_idx[0] :
								WEIGHT_TABLE_MAX_IDX - 1;
		weight_table_idx[1] = (table_idx[1] < WEIGHT_TABLE_MAX_IDX) ?
								table_idx[1] :
								WEIGHT_TABLE_MAX_IDX - 1;
		weight_sum[0] = 0;
		weight_sum[1] = 0;
	}

	if ((weight_sum[0] == 0) || (weight_sum[1] == 0)) {
		weight_sum[0] = 0;
		weight_sum[1] = 0;
		for(i = 0; i < t_window; i++) {
			weight_sum[0] += weight_table[table_idx[0]][i];
			weight_sum[1] += weight_table[table_idx[1]][i];
		}
	}

	normalized_util = ((long long)(utilization * platform->cur_clock) << 10) / platform->gpu_max_clock;

	GPU_LOG(DVFS_DEBUG, DUMMY, 0u, 0u, "%s: util[%d] cur_clock[%d] max_clock[%d] normalized_util[%d]\n",
			__func__, utilization, platform->cur_clock, platform->gpu_max_clock, normalized_util);

	for(idx = 0; idx < 2; idx++) {
		platform->prediction.weight_util[idx] = 0;
		platform->prediction.weight_freq = 0;

		for (i = t_window - 1; i >= 0; i--) {
			if (0 == i) {
				platform->prediction.util_history[idx][0] = normalized_util;
				platform->prediction.weight_util[idx] += platform->prediction.util_history[idx][i] * weight_table[table_idx[idx]][i];
				platform->prediction.weight_util[idx] /= weight_sum[idx];
				platform->prediction.en_signal = true;
				break;
			}

			platform->prediction.util_history[idx][i] = platform->prediction.util_history[idx][i - 1];
			platform->prediction.weight_util[idx] += platform->prediction.util_history[idx][i] * weight_table[table_idx[idx]][i];
		}

		/* Check history */
		GPU_LOG(DVFS_DEBUG, DUMMY, 0u, 0u, "%s: cur_util[%d]_cur_freq[%d]_weight_util[%d]_pre_util[%d]_window[%d]\n",
					__func__,
				platform->env_data.utilization,
				platform->cur_clock,
				platform->prediction.weight_util[idx],
				platform->prediction.pre_util,
				t_window);
	}
	if (platform->prediction.weight_util[0] < platform->prediction.weight_util[1]) {
		platform->prediction.weight_util[0] = platform->prediction.weight_util[1];
	}

	if (platform->prediction.en_signal == true)
		util_conv = (long long)(platform->prediction.weight_util[0]) * platform->gpu_max_clock / platform->cur_clock;
	else
		util_conv = utilization << 10;

	return util_conv;
}
#endif

#define G3D_GOVERNOR_STATIC_PERIOD		10
static int gpu_dvfs_governor_static(struct exynos_context *platform, int utilization)
{
	static bool step_down = true;
	static int count;

	DVFS_ASSERT(platform);

	if (count == G3D_GOVERNOR_STATIC_PERIOD) {
		if (step_down) {
			if (platform->step > gpu_dvfs_get_level(platform->gpu_max_clock))
				platform->step--;
			if (((platform->max_lock > 0) && (platform->table[platform->step].clock == platform->max_lock))
					|| (platform->step == gpu_dvfs_get_level(platform->gpu_max_clock)))
				step_down = false;
		} else {
			if (platform->step < gpu_dvfs_get_level(platform->gpu_min_clock))
				platform->step++;
			if (((platform->min_lock > 0) && (platform->table[platform->step].clock == platform->min_lock))
					|| (platform->step == gpu_dvfs_get_level(platform->gpu_min_clock)))
				step_down = true;
		}

		count = 0;
	} else {
		count++;
	}

	return 0;
}

static int gpu_dvfs_governor_booster(struct exynos_context *platform, int utilization)
{
	static int weight;
	int cur_weight, booster_threshold, dvfs_table_lock;

	DVFS_ASSERT(platform);

	cur_weight = platform->cur_clock*utilization;
	/* booster_threshold = current clock * set the percentage of utilization */
	booster_threshold = platform->cur_clock * 50;

	dvfs_table_lock = gpu_dvfs_get_level(platform->gpu_max_clock);

	if ((platform->step >= dvfs_table_lock+2) &&
			((cur_weight - weight) > booster_threshold)) {
		platform->step -= 2;
		platform->down_requirement = platform->table[platform->step].down_staycount;
		GPU_LOG(DVFS_WARNING, DUMMY, 0u, 0u, "Booster Governor: G3D level 2 step\n");
	} else if ((platform->step > gpu_dvfs_get_level(platform->gpu_max_clock)) &&
			(utilization > platform->table[platform->step].max_threshold)) {
		platform->step--;
		platform->down_requirement = platform->table[platform->step].down_staycount;
	} else if ((platform->step < gpu_dvfs_get_level(platform->gpu_min_clock)) &&
			(utilization < platform->table[platform->step].min_threshold)) {
		platform->down_requirement--;
		if (platform->down_requirement == 0) {
			platform->step++;
			platform->down_requirement = platform->table[platform->step].down_staycount;
		}
	} else {
		platform->down_requirement = platform->table[platform->step].down_staycount;
	}

	DVFS_ASSERT((platform->step >= gpu_dvfs_get_level(platform->gpu_max_clock))
					&& (platform->step <= gpu_dvfs_get_level(platform->gpu_min_clock)));

	weight = cur_weight;

	return 0;
}

static int gpu_dvfs_governor_dynamic(struct exynos_context *platform, int utilization)
{
	int max_clock_lev = gpu_dvfs_get_level(platform->gpu_max_clock);
	int min_clock_lev = gpu_dvfs_get_level(platform->gpu_min_clock);

	DVFS_ASSERT(platform);

	if ((platform->step > max_clock_lev) && (utilization > platform->table[platform->step].max_threshold)) {
		if (platform->table[platform->step].clock * utilization >
				platform->table[platform->step - 1].clock * platform->table[platform->step - 1].max_threshold) {
			platform->step -= 2;
			if (platform->step < max_clock_lev) {
				platform->step = max_clock_lev;
			}
		} else {
			platform->step--;
		}

		if (platform->table[platform->step].clock > platform->gpu_max_clock_limit)
			platform->step = gpu_dvfs_get_level(platform->gpu_max_clock_limit);

		platform->down_requirement = platform->table[platform->step].down_staycount;
	} else if ((platform->step < min_clock_lev) && (utilization < platform->table[platform->step].min_threshold)) {
		platform->down_requirement--;
		if (platform->down_requirement == 0)
		{
			if (platform->table[platform->step].clock * utilization <
					platform->table[platform->step + 1].clock * platform->table[platform->step + 1].min_threshold) {
				platform->step += 2;
				if (platform->step > min_clock_lev) {
					platform->step = min_clock_lev;
				}
			} else {
				platform->step++;
			}
			platform->down_requirement = platform->table[platform->step].down_staycount;
		}
	} else {
		platform->down_requirement = platform->table[platform->step].down_staycount;
	}

	DVFS_ASSERT(((platform->using_max_limit_clock && (platform->step >= gpu_dvfs_get_level(platform->gpu_max_clock_limit))) ||
			((!platform->using_max_limit_clock && (platform->step >= gpu_dvfs_get_level(platform->gpu_max_clock)))))
			&& (platform->step <= gpu_dvfs_get_level(platform->gpu_min_clock)));

	return 0;
}

static int gpu_dvfs_decide_next_governor(struct exynos_context *platform)
{
	return 0;
}

void ipa_mali_dvfs_requested(unsigned int freq);
int gpu_dvfs_decide_next_freq(struct kbase_device *kbdev, int utilization)
{
	unsigned long flags;
	struct exynos_context *platform = (struct exynos_context *) kbdev->platform_context;
	DVFS_ASSERT(platform);

#ifdef CONFIG_MALI_TSG
if (platform->interactive_fix == 0) {
	if (platform->migov_mode == 1 && platform->is_gov_set != 1) {
		gpu_dvfs_governor_setting_locked(platform, G3D_DVFS_GOVERNOR_JOINT);
		platform->migov_saved_polling_speed = platform->polling_speed;
		platform->polling_speed = 16;
		platform->is_gov_set = 1;
		platform->prediction.en_signal = false;
		platform->is_pm_qos_tsg = true;
	} else if (platform->migov_mode != 1 && platform->is_gov_set != 0) {
		gpu_dvfs_governor_setting_locked(platform, platform->governor_type_init);
		platform->polling_speed = platform->migov_saved_polling_speed;
		platform->is_gov_set = 0;
		platform->is_pm_qos_tsg = false;
	}
}
#endif /* CONFIG_MALI_TSG */
	spin_lock_irqsave(&platform->gpu_dvfs_spinlock, flags);
	gpu_dvfs_decide_next_governor(platform);
	gpu_dvfs_get_next_level(platform, utilization);
	spin_unlock_irqrestore(&platform->gpu_dvfs_spinlock, flags);

#ifdef CONFIG_MALI_SEC_CL_BOOST
	if (kbdev->pm.backend.metrics.is_full_compute_util && platform->cl_boost_disable == false)
		platform->step = gpu_dvfs_get_level(platform->gpu_max_clock);
#endif

#ifdef CONFIG_CPU_THERMAL_IPA
	ipa_mali_dvfs_requested(platform->table[platform->step].clock);
#endif /* CONFIG_CPU_THERMAL_IPA */

	return platform->table[platform->step].clock;
}

int gpu_dvfs_governor_setting(struct exynos_context *platform, int governor_type)
{
#ifdef CONFIG_MALI_DVFS
	int i;
#endif /* CONFIG_MALI_DVFS */
	unsigned long flags;

	DVFS_ASSERT(platform);

	if ((governor_type < 0) || (governor_type >= G3D_MAX_GOVERNOR_NUM)) {
		GPU_LOG(DVFS_WARNING, DUMMY, 0u, 0u, "%s: invalid governor type (%d)\n", __func__, governor_type);
		return -1;
	}

	spin_lock_irqsave(&platform->gpu_dvfs_spinlock, flags);
#ifdef CONFIG_MALI_DVFS
	platform->table = governor_info[governor_type].table;
	platform->table_size = governor_info[governor_type].table_size;
	platform->step = gpu_dvfs_get_level(governor_info[governor_type].start_clk);
	gpu_dvfs_get_next_level = (GET_NEXT_LEVEL)(governor_info[governor_type].governor);

	platform->env_data.utilization = 80;
	platform->max_lock = 0;
	platform->min_lock = 0;

	for (i = 0; i < NUMBER_LOCK; i++) {
		platform->user_max_lock[i] = 0;
		platform->user_min_lock[i] = 0;
	}

	platform->down_requirement = 1;
	platform->governor_type = governor_type;

	gpu_dvfs_init_time_in_state();
#else /* CONFIG_MALI_DVFS */
	platform->table = (gpu_dvfs_info *)gpu_get_attrib_data(platform->attrib, GPU_GOVERNOR_TABLE_DEFAULT);
	platform->table_size = (u32)gpu_get_attrib_data(platform->attrib, GPU_GOVERNOR_TABLE_SIZE_DEFAULT);
	platform->step = gpu_dvfs_get_level(platform->gpu_dvfs_start_clock);
#endif /* CONFIG_MALI_DVFS */
	platform->cur_clock = platform->table[platform->step].clock;

	spin_unlock_irqrestore(&platform->gpu_dvfs_spinlock, flags);

	return 0;
}

#ifdef CONFIG_MALI_TSG
/* its function is to maintain clock & clock_lock */
int gpu_dvfs_governor_setting_locked(struct exynos_context *platform, int governor_type)
{
	unsigned long flags;

	DVFS_ASSERT(platform);

	if ((governor_type < 0) || (governor_type >= G3D_MAX_GOVERNOR_NUM)) {
		GPU_LOG(DVFS_WARNING, DUMMY, 0u, 0u, "%s: invalid governor type (%d)\n", __func__, governor_type);
		return -1;
	}

	spin_lock_irqsave(&platform->gpu_dvfs_spinlock, flags);
#ifdef CONFIG_MALI_DVFS
	platform->table = governor_info[governor_type].table;
	platform->table_size = governor_info[governor_type].table_size;
	platform->step = gpu_dvfs_get_level(governor_info[governor_type].start_clk);
	gpu_dvfs_get_next_level = (GET_NEXT_LEVEL)(governor_info[governor_type].governor);

	platform->governor_type = governor_type;
#else /* CONFIG_MALI_DVFS */
	platform->table = (gpu_dvfs_info *)gpu_get_attrib_data(platform->attrib, GPU_GOVERNOR_TABLE_DEFAULT);
	platform->table_size = (u32)gpu_get_attrib_data(platform->attrib, GPU_GOVERNOR_TABLE_SIZE_DEFAULT);
#endif /* CONFIG_MALI_DVFS */
	spin_unlock_irqrestore(&platform->gpu_dvfs_spinlock, flags);

	return 0;
}
#endif /* CONFIG_MALI_TSG */

int gpu_dvfs_governor_init(struct kbase_device *kbdev)
{
	int governor_type = G3D_DVFS_GOVERNOR_DEFAULT;
	struct exynos_context *platform = (struct exynos_context *) kbdev->platform_context;

	DVFS_ASSERT(platform);

#ifdef CONFIG_MALI_DVFS
	governor_type = platform->governor_type;
#endif /* CONFIG_MALI_DVFS */
	if (gpu_dvfs_governor_setting(platform, governor_type) < 0) {
		GPU_LOG(DVFS_WARNING, DUMMY, 0u, 0u, "%s: fail to initialize governor\n", __func__);
		return -1;
	}

	/* share table_size among governors, as every single governor has same table_size. */
	platform->save_cpu_max_freq = kmalloc(sizeof(int) * platform->table_size, GFP_KERNEL);
#if defined(CONFIG_MALI_DVFS) && defined(CONFIG_CPU_THERMAL_IPA)
	gpu_ipa_dvfs_calc_norm_utilisation(kbdev);
#endif /* CONFIG_MALI_DVFS && CONFIG_CPU_THERMAL_IPA */

	return 0;
}

#ifdef CONFIG_MALI_TSG
#include <mali_base_kernel.h>
#define TSG_PERIOD NSEC_PER_MSEC
/* Return whether katom will run on the GPU or not. Currently only soft jobs and
 * dependency-only atoms do not run on the GPU */
#define IS_GPU_ATOM(katom) (!((katom->core_req & BASE_JD_REQ_SOFT_JOB) ||  \
			((katom->core_req & BASE_JD_REQ_ATOM_TYPE) ==    \
			 BASE_JD_REQ_DEP)))
static inline bool both_q_active(int in_nr, int out_nr)
{
	return in_nr > 0 && out_nr > 0;
}

static inline bool hw_q_active(int out_nr)
{
	return out_nr > 0;
}

static void accum_queued_time(ktime_t current_time, bool accum0, bool accum1)
{
	struct kbase_device *kbdev = pkbdev;

	if (accum0)
		kbdev->queued_time_tick[0] += current_time - kbdev->last_updated;
	if (accum1)
		kbdev->queued_time_tick[1] += current_time - kbdev->last_updated;
	kbdev->last_updated = current_time;
}

int gpu_tsg_set_count(struct kbase_jd_atom *katom, u32 core_req, u32 status, bool stop)
{
	struct kbase_device *kbdev = pkbdev;
	int prev_out_nr, prev_in_nr;
	int cur_out_nr, cur_in_nr;
	bool need_update = false;
	ktime_t current_time = 0;

	if (kbdev == NULL)
		return -ENODEV;

	if (!IS_GPU_ATOM(katom) || status == KBASE_JD_ATOM_STATE_COMPLETED)
		return 0;

	if (kbdev->last_updated == 0)
		kbdev->last_updated = ktime_get();

	prev_out_nr = atomic_read(&kbdev->out_nr);
	prev_in_nr	= atomic_read(&kbdev->in_nr);

	if (stop) {
		atomic_inc(&kbdev->in_nr);
		atomic_dec(&kbdev->out_nr);
	} else {
		switch (status) {
		case KBASE_JD_ATOM_STATE_QUEUED:
			atomic_inc(&kbdev->in_nr);
			break;
		case KBASE_JD_ATOM_STATE_IN_JS:
			atomic_dec(&kbdev->in_nr);
			atomic_inc(&kbdev->out_nr);
			break;
		case KBASE_JD_ATOM_STATE_HW_COMPLETED:
			atomic_dec(&kbdev->out_nr);
			break;
		default:
			break;
		}
	}

	cur_in_nr	= atomic_read(&kbdev->in_nr);
	cur_out_nr	= atomic_read(&kbdev->out_nr);

	if ((!both_q_active(prev_in_nr, prev_out_nr) && both_q_active(cur_in_nr, cur_out_nr)) ||
		(!hw_q_active(prev_out_nr) && hw_q_active(cur_out_nr)))
		need_update = true;
	else if ((both_q_active(prev_in_nr, prev_out_nr) && !both_q_active(cur_in_nr, cur_out_nr)) ||
		(hw_q_active(prev_out_nr) && !hw_q_active(cur_out_nr)))
		need_update = true;
	else if (prev_out_nr > cur_out_nr) {
		current_time = ktime_get();
		need_update = current_time - kbdev->last_updated > 2 * TSG_PERIOD;
	}

	if (need_update) {
		current_time = (current_time == 0) ? ktime_get() : current_time;
		if (raw_spin_trylock(&kbdev->tsg_lock)) {
			accum_queued_time(current_time, both_q_active(prev_in_nr, prev_out_nr),
								hw_q_active(prev_out_nr));
			raw_spin_unlock(&kbdev->tsg_lock);
		}
	}

	if ((cur_in_nr + cur_out_nr) < 0)
		gpu_tsg_reset_count(0);

	return 0;
}

int gpu_tsg_reset_count(int powered)
{
	struct kbase_device *kbdev = pkbdev;

	if (kbdev == NULL)
		return -ENODEV;

	if (powered == 0) {
		atomic_set(&kbdev->job_nr, 0);
		atomic_set(&kbdev->in_nr, 0);
		atomic_set(&kbdev->out_nr, 0);
		kbdev->last_updated = 0;
	}

	return 0;
}

#endif /* CONFIG_MALI_TSG */
#endif /* CONFIG_MALI_DVFS */
