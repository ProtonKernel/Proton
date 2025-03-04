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
	DVFS_ASSERT(platform);

	if ((platform->step > gpu_dvfs_get_level(platform->gpu_max_clock) ||
	    (platform->using_max_limit_clock && platform->step > gpu_dvfs_get_level(platform->gpu_max_clock_limit)))
			&& (utilization > platform->table[platform->step].max_threshold)) {
		int highspeed_level = gpu_dvfs_get_level(platform->interactive.highspeed_clock);
		if ((highspeed_level > 0) && (platform->step > highspeed_level)
				&& (utilization > platform->interactive.highspeed_load)) {
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
			&& (utilization < platform->table[platform->step].min_threshold)) {
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
int gpu_weight_prediction_utilisation(struct exynos_context *platform, int utilization)
{
	int i;
	int idx;
	int t_window = weight_table_size;
	static int weight_sum[2] = {0, 0};
	int weight_table[2][weight_table_size] = {/* select_weight_table */
											{  48,  44,  40,  36,  32,  28,  24,  20,  16,  12,   8,   4}, // Default
											{ 100,  10,   1,   0,   0,   0,   0,   0,   0,   0,   0,   0}  // 0.1
										};
	int normalized_util;
	int util_conv;

	DVFS_ASSERT(platform);

	if (0 == weight_sum[0]) {
		for(i = 0; i < t_window; i++) {
			weight_sum[0] += weight_table[0][i];
			weight_sum[1] += weight_table[1][i];
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
				platform->prediction.weight_util[idx] += platform->prediction.util_history[idx][i] * weight_table[idx][i];
				platform->prediction.weight_util[idx] /= weight_sum[idx];
				platform->prediction.en_signal = true;
				break;
			}

			platform->prediction.util_history[idx][i] = platform->prediction.util_history[idx][i - 1];
			platform->prediction.weight_util[idx] += platform->prediction.util_history[idx][i] * weight_table[idx][i];
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
		gpu_tsg_reset_acc_count();	/* tsg acc count reset */
	}
	gpu_tsg_input_nr_acc_cnt();	/* acc input nr each dvfs period(30ms) */
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

	gpu_dvfs_init_time_in_state();
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
/* Return whether katom will run on the GPU or not. Currently only soft jobs and
 * dependency-only atoms do not run on the GPU */
#define IS_GPU_ATOM(katom) (!((katom->core_req & BASE_JD_REQ_SOFT_JOB) ||  \
			((katom->core_req & BASE_JD_REQ_ATOM_TYPE) ==    \
			 BASE_JD_REQ_DEP)))
int gpu_tsg_set_count(struct kbase_jd_atom *katom, u32 core_req, u32 status, bool stop)
{
	struct kbase_device *kbdev = pkbdev;
	ktime_t current_time[2];
	static ktime_t prev_time[2];
	static bool pre_queued_threshold_0_state = false;
	static bool pre_queued_threshold_1_state = false;

	if (kbdev == NULL)
		return -ENODEV;

	if (IS_GPU_ATOM(katom)) {
		if (stop == true) {
			kbdev->stop_cnt++;
			kbdev->in_js_total_job_nr--;    /* IN_JS to QUEUED, so in_js count decrease... is it X-depenency??? */
			kbdev->input_job_nr++;
			kbdev->output_job_nr--;
			if (kbdev->in_js_total_job_nr < 0)
				kbdev->in_js_total_job_nr = 0;
			if (kbdev->output_job_nr < 0)
				kbdev->output_job_nr = 0;
		} else {
			switch (status) {
				case KBASE_JD_ATOM_STATE_QUEUED:
					kbdev->queued_total_job_nr++;
					kbdev->input_job_nr++;
					break;
				case KBASE_JD_ATOM_STATE_IN_JS:
					kbdev->in_js_total_job_nr++;
					kbdev->input_job_nr--;
					if (kbdev->input_job_nr < 0)
						kbdev->input_job_nr = 0;
					kbdev->output_job_nr++;
					break;
				case KBASE_JD_ATOM_STATE_HW_COMPLETED:
					kbdev->hw_complete_job_nr_cnt++;
					kbdev->output_job_nr--;
					if (kbdev->output_job_nr < 0)
						kbdev->output_job_nr = 0;
					break;
				case KBASE_JD_ATOM_STATE_COMPLETED:
					kbdev->complete_job_nr_cnt++;
					break;
				default:
					break;
			}
		}
	}

	if (prev_time[0] == 0)
		prev_time[0] = ktime_get();

	if (prev_time[1] == 0)
		prev_time[1] = ktime_get();

	current_time[0] = ktime_get();
	current_time[1] = ktime_get();
	if (pre_queued_threshold_0_state == true)
		kbdev->queued_time_tick[0] += current_time[0] - prev_time[0];

	if (pre_queued_threshold_1_state == true)
		kbdev->queued_time_tick[1] += current_time[1] - prev_time[1];

	prev_time[0] = current_time[0];
	prev_time[1] = current_time[1];

	if ((kbdev->input_job_nr + kbdev->output_job_nr) > kbdev->queued_threshold[0])
		pre_queued_threshold_0_state = true;
	else
		pre_queued_threshold_0_state = false;

	if ((kbdev->input_job_nr + kbdev->output_job_nr) > kbdev->queued_threshold[1])
		pre_queued_threshold_1_state = true;
	else
		pre_queued_threshold_1_state = false;

	return 0;
}

int gpu_tsg_reset_count(int powered)
{
	struct kbase_device *kbdev = pkbdev;

	if (kbdev == NULL)
		return -ENODEV;

	if (powered == 0) {
		kbdev->queued_total_job_nr = 0;
		kbdev->in_js_total_job_nr = 0;
		kbdev->hw_complete_job_nr_cnt = 0;
		kbdev->complete_job_nr_cnt = 0;
		kbdev->stop_cnt = 0;
		kbdev->input_job_nr = 0;
		kbdev->output_job_nr = 0;
	}

	return 0;
}

void gpu_tsg_input_nr_acc_cnt(void)
{
	struct kbase_device * kbdev = pkbdev;

	if (kbdev == NULL)
		return;

	kbdev->input_job_nr_acc += kbdev->input_job_nr;
}

void gpu_tsg_reset_acc_count(void)
{
	struct kbase_device *kbdev = pkbdev;

	if (kbdev == NULL)
		return;

	kbdev->input_job_nr_acc = 0;
	kbdev->queued_time_tick[0] = 0;
	kbdev->queued_time_tick[1] = 0;
}
#endif /* CONFIG_MALI_TSG */
#endif /* CONFIG_MALI_DVFS */
