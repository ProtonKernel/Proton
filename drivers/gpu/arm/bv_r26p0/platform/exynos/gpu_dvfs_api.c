/* drivers/gpu/arm/.../platform/gpu_dvfs_api.c
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
 * @file gpu_dvfs_api.c
 * DVFS
 */

#include <mali_kbase.h>
#ifdef CONFIG_EXYNOS_ASV
#include <soc/samsung/asv-exynos.h>
#endif
#include <linux/version.h>

#include "mali_kbase_platform.h"
#include "gpu_control.h"
#include "gpu_dvfs_handler.h"
#include "gpu_dvfs_governor.h"

#if IS_ENABLED(CONFIG_EXYNOS_BTS)
#include <soc/samsung/bts.h>
#endif

#if IS_ENABLED(CONFIG_MALI_USES_LLC)
#include "gpu_llc.h"
#endif

extern struct kbase_device *pkbdev;

static int gpu_check_target_clock(struct exynos_context *platform, int clock)
{
	int target_clock = clock;

	DVFS_ASSERT(platform);

	if (gpu_dvfs_get_level(target_clock) < 0)
		return -1;

#ifdef CONFIG_MALI_DVFS
	if (!platform->dvfs_status)
		return target_clock;

	GPU_LOG(DVFS_DEBUG, DUMMY, 0u, 0u, "clock: %d, min: %d, max: %d\n", clock, platform->min_lock, platform->max_lock);

	if ((platform->min_lock > 0) && (platform->power_status) &&
			((target_clock < platform->min_lock) || (platform->cur_clock < platform->min_lock)))
		target_clock = platform->min_lock;

	if ((platform->max_lock > 0) && (target_clock > platform->max_lock))
		target_clock = platform->max_lock;
#endif /* CONFIG_MALI_DVFS */

	platform->step = gpu_dvfs_get_level(target_clock);

	return target_clock;
}

#ifdef CONFIG_MALI_DVFS
static int gpu_update_cur_level(struct exynos_context *platform)
{
	unsigned long flags;
	int level = 0;

	DVFS_ASSERT(platform);

	level = gpu_dvfs_get_level(platform->cur_clock);
	if (level >= 0) {
		spin_lock_irqsave(&platform->gpu_dvfs_spinlock, flags);
		if (platform->step != level)
			platform->down_requirement = platform->table[level].down_staycount;
		if (platform->step < level)
			platform->interactive.delay_count = 0;
		platform->step = level;
		spin_unlock_irqrestore(&platform->gpu_dvfs_spinlock, flags);
	} else {
		GPU_LOG(DVFS_ERROR, DUMMY, 0u, 0u, "%s: invalid dvfs level returned %d gpu power %d\n", __func__, platform->cur_clock, gpu_is_power_on());
		return -1;
	}
	return 0;
}
#else
#define gpu_update_cur_level(platform) (0)
#endif

int gpu_set_target_clk_vol(int clk, bool pending_is_allowed, bool force)
{
	int ret = 0, target_clk = 0;
	int prev_clk = 0;
	struct kbase_device *kbdev = pkbdev;
	struct exynos_context *platform = (struct exynos_context *) kbdev->platform_context;

	DVFS_ASSERT(platform);

	if (!gpu_control_is_power_on(pkbdev)) {
		GPU_LOG(DVFS_INFO, DUMMY, 0u, 0u, "%s: can't set clock and voltage in the power-off state!\n", __func__);
		return -1;
	}

	mutex_lock(&platform->gpu_clock_lock);
#ifdef CONFIG_MALI_DVFS
	if (pending_is_allowed && platform->dvs_is_enabled) {
		if (!platform->dvfs_pending && clk < platform->cur_clock) {
			platform->dvfs_pending = clk;
			GPU_LOG(DVFS_DEBUG, DUMMY, 0u, 0u, "pending to change the clock [%d -> %d\n", platform->cur_clock, platform->dvfs_pending);
		} else if (clk > platform->cur_clock) {
			platform->dvfs_pending = 0;
		}
		mutex_unlock(&platform->gpu_clock_lock);
		return 0;
	} else {
		platform->dvfs_pending = 0;
	}

	if (platform->dvs_is_enabled || !platform->power_status) {
		mutex_unlock(&platform->gpu_clock_lock);
		GPU_LOG(DVFS_INFO, DUMMY, 0u, 0u, "%s: can't control clock and voltage in dvs and power off %d %d\n",
				__func__,
				platform->dvs_is_enabled,
				platform->power_status);
		return 0;
	}

#endif /* CONFIG_MALI_DVFS */
	if(!force)
		target_clk = gpu_check_target_clock(platform, clk);
	else
		target_clk = clk;
	if (target_clk < 0) {
		mutex_unlock(&platform->gpu_clock_lock);
		GPU_LOG(DVFS_ERROR, DUMMY, 0u, 0u,
				"%s: mismatch clock error (source %d, target %d)\n", __func__, clk, target_clk);
		return -1;
	}

#ifdef CONFIG_MALI_RT_PM
	if (platform->exynos_pm_domain) {
		mutex_lock(&platform->exynos_pm_domain->access_lock);
		if (!platform->dvs_is_enabled && gpu_is_power_on())
			prev_clk = gpu_get_cur_clock(platform);
		mutex_unlock(&platform->exynos_pm_domain->access_lock);
	}
#endif

#ifdef CONFIG_MALI_DVFS
	gpu_control_set_dvfs(kbdev, target_clk, force);
#endif
	ret = gpu_update_cur_level(platform);

/* W/A for BS_G3D_PERFORMANCE misspelling on kernel version 4.4 */
#if (LINUX_VERSION_CODE < KERNEL_VERSION(4, 9, 0))
#define BS_G3D_PERFORMANCE BS_G3D_PEFORMANCE
#endif

#if IS_ENABLED(CONFIG_EXYNOS_BTS)
	{
		unsigned long flags;

		spin_lock_irqsave(&platform->bts_spinlock, flags);
		if (platform->gpu_bts_support) {
			if (target_clk >= platform->mo_min_clock && !platform->is_set_bts) {
				bts_add_scenario(platform->bts_scen_idx);
				platform->is_set_bts = 1;
			} else if (target_clk < platform->mo_min_clock && platform->is_set_bts) {
				bts_del_scenario(platform->bts_scen_idx);
				platform->is_set_bts = 0;
			}
		}
		spin_unlock_irqrestore(&platform->bts_spinlock, flags);
	}
#endif

#if IS_ENABLED(CONFIG_MALI_USES_LLC)
	{
		int idx = 0;
		int ways = 0;

		for (idx = 0; idx < platform->table_size; idx++) {
			if (platform->cur_clock >= platform->table[idx].clock) {
				ways = platform->table[idx].llc_ways;
				break;
			}
		}

		gpu_llc_set(kbdev, ways);
	}
#endif

	mutex_unlock(&platform->gpu_clock_lock);

	GPU_LOG(DVFS_DEBUG, DUMMY, 0u, 0u, "clk[%d -> %d], vol[%d (margin : %d)]\n",
		prev_clk, target_clk, gpu_get_cur_voltage(platform), platform->voltage_margin);

	return ret;
}

#ifdef CONFIG_MALI_DVFS
int gpu_set_target_clk_vol_pending(int clk)
{
	int ret = 0, target_clk = 0;
	int prev_clk = 0;
	struct kbase_device *kbdev = pkbdev;
	struct exynos_context *platform = (struct exynos_context *) kbdev->platform_context;

	DVFS_ASSERT(platform);

	target_clk = gpu_check_target_clock(platform, clk);
	if (target_clk < 0) {
		GPU_LOG(DVFS_ERROR, DUMMY, 0u, 0u,
				"%s: mismatch clock error (source %d, target %d)\n", __func__, clk, target_clk);
		return -1;
	}

#ifdef CONFIG_MALI_RT_PM
	if (platform->exynos_pm_domain) {
		mutex_lock(&platform->exynos_pm_domain->access_lock);
		if (!platform->dvs_is_enabled && gpu_is_power_on())
			prev_clk = gpu_get_cur_clock(platform);
		mutex_unlock(&platform->exynos_pm_domain->access_lock);
	}
#endif

	gpu_control_set_dvfs(kbdev, target_clk, false);
	ret = gpu_update_cur_level(platform);

	GPU_LOG(DVFS_INFO, DUMMY, 0u, 0u, "pending clk[%d -> %d], vol[%d (margin : %d)]\n",
		prev_clk, target_clk, gpu_get_cur_voltage(platform), platform->voltage_margin);

	return ret;
}

int gpu_dvfs_boost_lock(gpu_dvfs_boost_command boost_command)
{
	struct kbase_device *kbdev = pkbdev;
	struct exynos_context *platform = (struct exynos_context *) kbdev->platform_context;

	DVFS_ASSERT(platform);

	if (!platform->dvfs_status)
		return 0;

	if ((boost_command < GPU_DVFS_BOOST_SET) || (boost_command > GPU_DVFS_BOOST_END)) {
		GPU_LOG(DVFS_WARNING, DUMMY, 0u, 0u, "%s: invalid boost command is called (%d)\n", __func__, boost_command);
		return -1;
	}

	switch (boost_command) {
	case GPU_DVFS_BOOST_SET:
		platform->boost_is_enabled = true;
		if (platform->boost_gpu_min_lock)
			gpu_dvfs_clock_lock(GPU_DVFS_MIN_LOCK, BOOST_LOCK, platform->boost_gpu_min_lock);
#ifdef CONFIG_MALI_PM_QOS
		if (platform->boost_egl_min_lock)
			gpu_pm_qos_command(platform, GPU_CONTROL_PM_QOS_EGL_SET);
#endif /* CONFIG_MALI_PM_QOS */
		GPU_LOG(DVFS_INFO, DUMMY, 0u, 0u, "%s: boost mode is enabled (CPU: %d, GPU %d)\n",
				__func__, platform->boost_egl_min_lock, platform->boost_gpu_min_lock);
		break;
	case GPU_DVFS_BOOST_UNSET:
		platform->boost_is_enabled = false;
		if (platform->boost_gpu_min_lock)
			gpu_dvfs_clock_lock(GPU_DVFS_MIN_UNLOCK, BOOST_LOCK, 0);
#ifdef CONFIG_MALI_PM_QOS
		if (platform->boost_egl_min_lock)
			gpu_pm_qos_command(platform, GPU_CONTROL_PM_QOS_EGL_RESET);
#endif /* CONFIG_MALI_PM_QOS */
		GPU_LOG(DVFS_INFO, DUMMY, 0u, 0u, "%s: boost mode is disabled (CPU: %d, GPU %d)\n",
				__func__, platform->boost_egl_min_lock, platform->boost_gpu_min_lock);
		break;
	case GPU_DVFS_BOOST_GPU_UNSET:
		if (platform->boost_gpu_min_lock)
			gpu_dvfs_clock_lock(GPU_DVFS_MIN_UNLOCK, BOOST_LOCK, 0);
		GPU_LOG(DVFS_INFO, DUMMY, 0u, 0u, "%s: boost mode is disabled (GPU %d)\n",
				__func__, platform->boost_gpu_min_lock);
		break;
	default:
		break;
	}

	return 0;
}

int gpu_dvfs_clock_lock(gpu_dvfs_lock_command lock_command, gpu_dvfs_lock_type lock_type, int clock)
{
	struct kbase_device *kbdev = pkbdev;
	struct exynos_context *platform = (struct exynos_context *) kbdev->platform_context;

	int i;
	bool dirty = false;
	unsigned long flags;

	DVFS_ASSERT(platform);

	if (!platform->dvfs_status)
		return 0;

	if ((lock_type < TMU_LOCK) || (lock_type >= NUMBER_LOCK)) {
		GPU_LOG(DVFS_ERROR, DUMMY, 0u, 0u, "%s: invalid lock type is called (%d)\n", __func__, lock_type);
		return -1;
	}

	switch (lock_command) {
	case GPU_DVFS_MAX_LOCK:
		spin_lock_irqsave(&platform->gpu_dvfs_spinlock, flags);
		if (gpu_dvfs_get_level(clock) < 0) {
			spin_unlock_irqrestore(&platform->gpu_dvfs_spinlock, flags);
			GPU_LOG(DVFS_ERROR, DUMMY, 0u, 0u, "max lock error: invalid clock value %d\n", clock);
			return -1;
		}

		platform->user_max_lock[lock_type] = clock;
		platform->max_lock = clock;

		if (platform->max_lock > 0) {
			for (i = 0; i < NUMBER_LOCK; i++) {
				if (platform->user_max_lock[i] > 0)
					platform->max_lock = MIN(platform->max_lock, platform->user_max_lock[i]);
			}
		} else {
			platform->max_lock = clock;
		}

		spin_unlock_irqrestore(&platform->gpu_dvfs_spinlock, flags);

		if ((platform->max_lock > 0) && (platform->cur_clock >= platform->max_lock))
			gpu_set_target_clk_vol(platform->max_lock, false, false);

		GPU_LOG(DVFS_DEBUG, LSI_GPU_MAX_LOCK, lock_type, clock,
			"lock max clk[%d], user lock[%d], current clk[%d]\n",
			platform->max_lock, platform->user_max_lock[lock_type], platform->cur_clock);
		break;
	case GPU_DVFS_MIN_LOCK:
		spin_lock_irqsave(&platform->gpu_dvfs_spinlock, flags);
		if (gpu_dvfs_get_level(clock) < 0) {
			spin_unlock_irqrestore(&platform->gpu_dvfs_spinlock, flags);
			GPU_LOG(DVFS_ERROR, DUMMY, 0u, 0u, "min lock error: invalid clock value %d\n", clock);
			return -1;
		}

		platform->user_min_lock[lock_type] = clock;
		platform->min_lock = clock;

		if (platform->min_lock > 0) {
			for (i = 0; i < NUMBER_LOCK; i++) {
				if (platform->user_min_lock[i] > 0)
					platform->min_lock = MAX(platform->min_lock, platform->user_min_lock[i]);
			}
		} else {
			platform->min_lock = clock;
		}

		spin_unlock_irqrestore(&platform->gpu_dvfs_spinlock, flags);

		if ((platform->min_lock > 0) && (platform->cur_clock < platform->min_lock)
						&& (platform->min_lock <= platform->max_lock))
			gpu_set_target_clk_vol(platform->min_lock, false, false);

		GPU_LOG(DVFS_DEBUG, LSI_GPU_MIN_LOCK, lock_type, clock,
			"lock min clk[%d], user lock[%d], current clk[%d]\n",
			platform->min_lock, platform->user_min_lock[lock_type], platform->cur_clock);
		break;
	case GPU_DVFS_MAX_UNLOCK:
		spin_lock_irqsave(&platform->gpu_dvfs_spinlock, flags);

		platform->user_max_lock[lock_type] = 0;
		platform->max_lock = platform->gpu_max_clock;

		for (i = 0; i < NUMBER_LOCK; i++) {
			if (platform->user_max_lock[i] > 0) {
				dirty = true;
				platform->max_lock = MIN(platform->user_max_lock[i], platform->max_lock);
			}
		}

		if (!dirty)
			platform->max_lock = 0;

		spin_unlock_irqrestore(&platform->gpu_dvfs_spinlock, flags);
		GPU_LOG(DVFS_DEBUG, LSI_GPU_MAX_LOCK, lock_type, clock, "unlock max clk\n");
		break;
	case GPU_DVFS_MIN_UNLOCK:
		spin_lock_irqsave(&platform->gpu_dvfs_spinlock, flags);

		platform->user_min_lock[lock_type] = 0;
		platform->min_lock = platform->gpu_min_clock;

		for (i = 0; i < NUMBER_LOCK; i++) {
			if (platform->user_min_lock[i] > 0) {
				dirty = true;
				platform->min_lock = MAX(platform->user_min_lock[i], platform->min_lock);
			}
		}

		if (!dirty)
			platform->min_lock = 0;

		spin_unlock_irqrestore(&platform->gpu_dvfs_spinlock, flags);
		GPU_LOG(DVFS_DEBUG, LSI_GPU_MIN_LOCK, lock_type, clock, "unlock min clk\n");
		break;
	default:
		break;
	}

	return 0;
}

void gpu_dvfs_timer_control(bool enable)
{
	unsigned long flags;
	struct kbase_device *kbdev = pkbdev;
	struct exynos_context *platform = (struct exynos_context *) kbdev->platform_context;

	DVFS_ASSERT(platform);

	if (!platform->dvfs_status) {
		GPU_LOG(DVFS_ERROR, DUMMY, 0u, 0u, "%s: DVFS is disabled\n", __func__);
		return;
	}
	if (kbdev->pm.backend.metrics.timer_active && !enable) {
		cancel_delayed_work(platform->delayed_work);
		flush_workqueue(platform->dvfs_wq);
	} else if (!kbdev->pm.backend.metrics.timer_active && enable) {
		queue_delayed_work_on(0, platform->dvfs_wq,
				platform->delayed_work, msecs_to_jiffies(platform->polling_speed));
		spin_lock_irqsave(&platform->gpu_dvfs_spinlock, flags);
#if IS_ENABLED(CONFIG_MALI_RESET_DOWNSTAY_COUNT)
		platform->down_requirement = platform->table[platform->step].down_staycount;
#endif
		platform->interactive.delay_count = 0;
		spin_unlock_irqrestore(&platform->gpu_dvfs_spinlock, flags);
	}

	spin_lock_irqsave(&kbdev->pm.backend.metrics.lock, flags);
	kbdev->pm.backend.metrics.timer_active = enable;
	spin_unlock_irqrestore(&kbdev->pm.backend.metrics.lock, flags);
}

int gpu_dvfs_on_off(bool enable)
{
	struct kbase_device *kbdev = pkbdev;
	struct exynos_context *platform = (struct exynos_context *)kbdev->platform_context;

	DVFS_ASSERT(platform);

	if (enable && !platform->dvfs_status) {
		mutex_lock(&platform->gpu_dvfs_handler_lock);
		gpu_set_target_clk_vol(platform->cur_clock, false, false);
		gpu_dvfs_handler_init(kbdev);
		mutex_unlock(&platform->gpu_dvfs_handler_lock);

		gpu_dvfs_timer_control(true);
	} else if (!enable && platform->dvfs_status) {
		gpu_dvfs_timer_control(false);

		mutex_lock(&platform->gpu_dvfs_handler_lock);
		gpu_dvfs_handler_deinit(kbdev);
		gpu_set_target_clk_vol(platform->gpu_dvfs_config_clock, false, false);
		mutex_unlock(&platform->gpu_dvfs_handler_lock);
	} else {
		GPU_LOG(DVFS_WARNING, DUMMY, 0u, 0u, "%s: impossible state to change dvfs status (current: %d, request: %d)\n",
				__func__, platform->dvfs_status, enable);
		return -1;
	}

	return 0;
}

int gpu_dvfs_governor_change(int governor_type)
{
	struct kbase_device *kbdev = pkbdev;
	struct exynos_context *platform = (struct exynos_context *) kbdev->platform_context;

	DVFS_ASSERT(platform);

	mutex_lock(&platform->gpu_dvfs_handler_lock);
	gpu_dvfs_governor_setting(platform, governor_type);
	mutex_unlock(&platform->gpu_dvfs_handler_lock);

	return 0;
}
#endif /* CONFIG_MALI_DVFS */

int gpu_dvfs_init_time_in_state(void)
{
#ifdef CONFIG_MALI_DEBUG_SYS
	struct kbase_device *kbdev = pkbdev;
	struct exynos_context *platform = (struct exynos_context *) kbdev->platform_context;
	int i;

	DVFS_ASSERT(platform);

	for (i = gpu_dvfs_get_level(platform->gpu_max_clock); i <= gpu_dvfs_get_level(platform->gpu_min_clock); i++){
		platform->table[i].time = 0;
#ifdef CONFIG_MALI_TSG
		platform->table[i].time_busy = 0;
#endif
	}
#endif /* CONFIG_MALI_DEBUG_SYS */

	return 0;
}

int gpu_dvfs_update_time_in_state(int clock)
{
#if defined(CONFIG_MALI_DEBUG_SYS) && defined(CONFIG_MALI_DVFS)
	struct kbase_device *kbdev = pkbdev;
	struct exynos_context *platform = (struct exynos_context *) kbdev->platform_context;

	u64 current_time;
	static u64 prev_time;
	int level = gpu_dvfs_get_level(clock);

	DVFS_ASSERT(platform);

	if (prev_time == 0)
		prev_time = get_jiffies_64();

	current_time = get_jiffies_64();
	if ((level >= gpu_dvfs_get_level(platform->gpu_max_clock)) && (level <= gpu_dvfs_get_level(platform->gpu_min_clock))){
		platform->table[level].time += current_time-prev_time;
#ifdef CONFIG_MALI_TSG
		platform->table[level].time_busy += (unsigned long)((current_time-prev_time) * platform->env_data.utilization);	/* it has to divide 100 */
		GPU_LOG(DVFS_DEBUG, DUMMY, 0u, 0u, "%s: util = %d cur_clock[%d] = %d time_busy[%d] = %llu(%llu)\n",
				__func__, platform->env_data.utilization, level, clock, level, platform->table[level].time_busy / 100, platform->table[level].time);
#endif
	}

	prev_time = current_time;
#endif /* CONFIG_MALI_DEBUG_SYS */

	return 0;
}

int gpu_dvfs_get_level(int clock)
{
	struct kbase_device *kbdev = pkbdev;
	struct exynos_context *platform = (struct exynos_context *) kbdev->platform_context;
	int i;

	DVFS_ASSERT(platform);

	if ((clock < platform->gpu_min_clock) ||
	    (!platform->using_max_limit_clock && (clock > platform->gpu_max_clock)) ||
	    (platform->using_max_limit_clock &&  (clock > platform->gpu_max_clock_limit)))
		return -1;

	for (i = 0; i < platform->table_size; i++) {
		if (platform->table[i].clock == clock)
			return i;
	}

	return -1;
}
EXPORT_SYMBOL(gpu_dvfs_get_level);

int gpu_dvfs_get_level_clock(int clock)
{
	struct kbase_device *kbdev = pkbdev;
	struct exynos_context *platform = (struct exynos_context *) kbdev->platform_context;
	int i, min, max;

	DVFS_ASSERT(platform);

	min = gpu_dvfs_get_level(platform->gpu_min_clock);
	max = gpu_dvfs_get_level(platform->gpu_max_clock);

	for (i = max; i <= min; i++)
		if (clock - (int)(platform->table[i].clock) >= 0)
			return platform->table[i].clock;

	return -1;
}

int exynos_stats_get_gpu_cur_idx()
{
	struct kbase_device *kbdev = pkbdev;
	struct exynos_context *platform = (struct exynos_context *) kbdev->platform_context;
	int i;
	int level = -1;
	int clock = 0;

	DVFS_ASSERT(platform);
	DVFS_ASSERT(platform->table);

#ifdef CONFIG_MALI_RT_PM
	/* 20.03.06 need to pm domain access mutex lock, however, not yet applied it because other issue */
	if (platform->exynos_pm_domain) {
		if (!platform->dvs_is_enabled && gpu_is_power_on())
			clock = gpu_get_cur_clock(platform);
	}
#else
	if (gpu_control_is_power_on(pkbdev) == 1) {
		if (platform->dvs_is_enabled || (platform->inter_frame_pm_status && !platform->inter_frame_pm_is_poweron)) {
			GPU_LOG(DVFS_INFO, DUMMY, 0u, 0u,
					"%s: can't get dvfs cur clock\n", __func__);
			clock = 0;
		} else {
			clock = gpu_get_cur_clock(platform);
		}
	}
#endif

	if (clock == 0)
		return (gpu_dvfs_get_level(platform->gpu_min_clock) - gpu_dvfs_get_level(platform->gpu_max_clock));

	for (i = gpu_dvfs_get_level(platform->gpu_max_clock); i <= gpu_dvfs_get_level(platform->gpu_min_clock); i++) {
		if(platform->table[i].clock == clock) {
			level = i;
			break;
		}
	}

	return (level - gpu_dvfs_get_level(platform->gpu_max_clock));
}

int gpu_dvfs_get_voltage(int clock)
{
	struct kbase_device *kbdev = pkbdev;
	struct exynos_context *platform = (struct exynos_context *) kbdev->platform_context;
	int i;

	DVFS_ASSERT(platform);

	for (i = 0; i < platform->table_size; i++) {
		if (platform->table[i].clock == clock)
			return platform->table[i].voltage;
	}

	return -1;
}
EXPORT_SYMBOL_GPL(gpu_dvfs_get_voltage);

int gpu_dvfs_get_cur_asv_abb(void)
{
	struct kbase_device *kbdev = pkbdev;
	struct exynos_context *platform = (struct exynos_context *) kbdev->platform_context;

	DVFS_ASSERT(platform);

	if ((platform->step < 0) || (platform->step >= platform->table_size))
		return 0;

	return platform->table[platform->step].asv_abb;
}

int gpu_dvfs_get_clock(int level)
{
	struct kbase_device *kbdev = pkbdev;
	struct exynos_context *platform = (struct exynos_context *) kbdev->platform_context;

	DVFS_ASSERT(platform);

	if ((level < 0) || (level >= platform->table_size))
		return -1;

	return platform->table[level].clock;
}
EXPORT_SYMBOL_GPL(gpu_dvfs_get_clock);

int gpu_dvfs_get_step(void)
{
	struct kbase_device *kbdev = pkbdev;
	struct exynos_context *platform = (struct exynos_context *) kbdev->platform_context;

	DVFS_ASSERT(platform);

	return platform->table_size;
}
EXPORT_SYMBOL_GPL(gpu_dvfs_get_step);

int gpu_dvfs_get_cur_clock(void)
{
	struct kbase_device *kbdev = pkbdev;
	struct exynos_context *platform = (struct exynos_context *) kbdev->platform_context;
	int clock = 0;

	DVFS_ASSERT(platform);
#ifdef CONFIG_MALI_RT_PM
	if (platform->exynos_pm_domain) {
		mutex_lock(&platform->exynos_pm_domain->access_lock);
		if (!platform->dvs_is_enabled && gpu_is_power_on())
			clock = gpu_get_cur_clock(platform);
		mutex_unlock(&platform->exynos_pm_domain->access_lock);
	}
#else
	if (gpu_control_is_power_on(pkbdev) == 1) {
		mutex_lock(&platform->gpu_clock_lock);

		if (platform->dvs_is_enabled || (platform->inter_frame_pm_status && !platform->inter_frame_pm_is_poweron)) {
			mutex_unlock(&platform->gpu_clock_lock);
			GPU_LOG(DVFS_INFO, DUMMY, 0u, 0u,
					"%s: can't get dvfs cur clock\n", __func__);
			return 0;
		}
		clock = gpu_get_cur_clock(platform);
		mutex_unlock(&platform->gpu_clock_lock);
	}
#endif

	return clock;
}
EXPORT_SYMBOL_GPL(gpu_dvfs_get_cur_clock);

int gpu_dvfs_get_utilization(void)
{
	struct kbase_device *kbdev = pkbdev;
	struct exynos_context *platform = (struct exynos_context *) kbdev->platform_context;
	int util = 0;

	DVFS_ASSERT(platform);

	if (gpu_control_is_power_on(pkbdev) == 1)
		util  = platform->env_data.utilization;

	return util;
}
EXPORT_SYMBOL_GPL(gpu_dvfs_get_utilization);

int gpu_dvfs_get_min_freq(void)
{
	struct kbase_device *kbdev = pkbdev;
	struct exynos_context *platform = (struct exynos_context *) kbdev->platform_context;

	DVFS_ASSERT(platform);

	return platform->gpu_min_clock;
}
EXPORT_SYMBOL_GPL(gpu_dvfs_get_min_freq);

int gpu_dvfs_get_max_freq(void)
{
	struct kbase_device *kbdev = pkbdev;
	struct exynos_context *platform = (struct exynos_context *) kbdev->platform_context;

	DVFS_ASSERT(platform);

	return platform->gpu_max_clock;
}
EXPORT_SYMBOL_GPL(gpu_dvfs_get_max_freq);

#ifdef CONFIG_MALI_SEC_NEGATIVE_BOOST
bool gpu_dvfs_get_need_cpu_qos(void)
{
	struct kbase_device *kbdev = pkbdev;
	struct exynos_context *platform = (struct exynos_context *) kbdev->platform_context;
	bool need_cpu_qos = false;

	DVFS_ASSERT(platform);

	if (platform->need_cpu_qos
		&& ((platform->min_lock == platform->gpu_neg_boost_clock
				&& platform->max_lock == platform->gpu_neg_boost_clock)
		|| (platform->min_lock == platform->gpu_sustainable_clock
			&& platform->max_lock == platform->gpu_sustainable_clock))
		)
		need_cpu_qos = true;

	return need_cpu_qos;
}
#else
bool gpu_dvfs_get_need_cpu_qos(void)
{
	return false;
}
#endif

int exynos_stats_get_gpu_coeff(void)
{
	int coef = 6144;

	return coef;
}

unsigned int exynos_stats_get_gpu_table_size(void)
{
	struct kbase_device *kbdev = pkbdev;
	struct exynos_context *platform = (struct exynos_context *) kbdev->platform_context;

	DVFS_ASSERT(platform);

	return (gpu_dvfs_get_level(platform->gpu_min_clock) - gpu_dvfs_get_level(platform->gpu_max_clock) + 1);
}
EXPORT_SYMBOL(exynos_stats_get_gpu_table_size);

static unsigned int freqs[DVFS_TABLE_ROW_MAX];
unsigned int *exynos_stats_get_gpu_freq_table(void)
{
	struct kbase_device *kbdev = pkbdev;
	struct exynos_context *platform = (struct exynos_context *) kbdev->platform_context;
	int i;

	DVFS_ASSERT(platform);
	DVFS_ASSERT(platform->table);

	for (i = gpu_dvfs_get_level(platform->gpu_max_clock); i <= gpu_dvfs_get_level(platform->gpu_min_clock); i++) {
		freqs[i - gpu_dvfs_get_level(platform->gpu_max_clock)] = (unsigned int) platform->table[i].clock;
	}

	return freqs;
}
EXPORT_SYMBOL(exynos_stats_get_gpu_freq_table);

static unsigned int volts[DVFS_TABLE_ROW_MAX];
unsigned int *exynos_stats_get_gpu_volt_table(void)
{
	struct kbase_device *kbdev = pkbdev;
	struct exynos_context *platform = (struct exynos_context *) kbdev->platform_context;
	int i;

	DVFS_ASSERT(platform);
	DVFS_ASSERT(platform->table);

	for (i = gpu_dvfs_get_level(platform->gpu_max_clock); i <= gpu_dvfs_get_level(platform->gpu_min_clock); i++) {
		volts[i - gpu_dvfs_get_level(platform->gpu_max_clock)] = (unsigned int) platform->table[i].voltage;
	}

	return volts;
}
EXPORT_SYMBOL(exynos_stats_get_gpu_volt_table);

static ktime_t time_in_state[DVFS_TABLE_ROW_MAX];
ktime_t *exynos_stats_get_gpu_time_in_state(void)
{
	struct kbase_device *kbdev = pkbdev;
	struct exynos_context *platform = (struct exynos_context *) kbdev->platform_context;
	int i;

	DVFS_ASSERT(platform);
	DVFS_ASSERT(platform->table);

	for (i = gpu_dvfs_get_level(platform->gpu_max_clock); i <= gpu_dvfs_get_level(platform->gpu_min_clock); i++) {
#ifdef CONFIG_MALI_TSG
		time_in_state[i - gpu_dvfs_get_level(platform->gpu_max_clock)] = ms_to_ktime((u64)(platform->table[i].time_busy * 4) / 100);
#else
		time_in_state[i - gpu_dvfs_get_level(platform->gpu_max_clock)] = ms_to_ktime((u64)(platform->table[i].time * 4));
#endif /* CONFIG_MALI_TSG */
	}

	return time_in_state;
}
EXPORT_SYMBOL(exynos_stats_get_gpu_time_in_state);

#ifdef CONFIG_MALI_DVFS
int exynos_stats_get_gpu_max_lock(void)
{
	struct kbase_device *kbdev = pkbdev;
	struct exynos_context *platform = (struct exynos_context *) kbdev->platform_context;
	unsigned long flags;
	int locked_clock = -1;

	DVFS_ASSERT(platform);

	spin_lock_irqsave(&platform->gpu_dvfs_spinlock, flags);
	locked_clock = platform->max_lock;
	if (locked_clock <= 0)
		locked_clock = platform->gpu_max_clock;
	spin_unlock_irqrestore(&platform->gpu_dvfs_spinlock, flags);

	return locked_clock;
}
EXPORT_SYMBOL(exynos_stats_get_gpu_max_lock);

int exynos_stats_get_gpu_min_lock(void)
{
	struct kbase_device *kbdev = pkbdev;
	struct exynos_context *platform = (struct exynos_context *) kbdev->platform_context;
	unsigned long flags;
	int locked_clock = -1;

	DVFS_ASSERT(platform);

	spin_lock_irqsave(&platform->gpu_dvfs_spinlock, flags);
	locked_clock = platform->min_lock;
	if (locked_clock <= 0)
		locked_clock = platform->gpu_min_clock;
	spin_unlock_irqrestore(&platform->gpu_dvfs_spinlock, flags);

	return locked_clock;
}
EXPORT_SYMBOL(exynos_stats_get_gpu_min_lock);
#else
int exynos_stats_get_gpu_max_lock(void)
{
	return 0;
}
EXPORT_SYMBOL(exynos_stats_get_gpu_max_lock);

int exynos_stats_get_gpu_min_lock(void)
{
	return 0;
}
EXPORT_SYMBOL(exynos_stats_get_gpu_min_lock);
#endif /* CONFIG_MALI_DVFS */

#ifdef CONFIG_MALI_TSG
int exynos_stats_set_queued_threshold_0(unsigned int threshold)
{
	struct kbase_device *kbdev = pkbdev;

	kbdev->queued_threshold[0] = threshold;

	return kbdev->queued_threshold[0];
}

int exynos_stats_set_queued_threshold_1(unsigned int threshold)
{
	struct kbase_device *kbdev = pkbdev;

	kbdev->queued_threshold[1] = threshold;

	return kbdev->queued_threshold[1];
}

ktime_t *exynos_stats_get_gpu_queued_job_time(void)
{
	struct kbase_device *kbdev = pkbdev;
	int i;

	for (i = 0; i < 2; i++) {
		kbdev->queued_time[i] = kbdev->queued_time_tick[i];
		/* kbdev->queued_time_tick[i] = 0; */
	}

	return kbdev->queued_time;
}
EXPORT_SYMBOL(exynos_stats_get_gpu_queued_job_time);

ktime_t exynos_stats_get_gpu_queued_last_updated(void)
{
	struct kbase_device *kbdev = pkbdev;

	return kbdev->last_updated;
}
EXPORT_SYMBOL(exynos_stats_get_gpu_queued_last_updated);

void exynos_stats_set_gpu_polling_speed(int polling_speed)
{
	struct kbase_device *kbdev = pkbdev;
	struct exynos_context *platform = (struct exynos_context *) kbdev->platform_context;

	DVFS_ASSERT(platform);

	if ((polling_speed < 4) || (polling_speed > 1000)) {
		GPU_LOG(DVFS_WARNING, DUMMY, 0u, 0u, "%s: out of range [100~1000] (%d)\n", __func__, polling_speed);
		return;
	}
	platform->polling_speed = polling_speed;
}
EXPORT_SYMBOL(exynos_stats_set_gpu_polling_speed);

int exynos_stats_get_gpu_polling_speed(void)
{
   struct kbase_device *kbdev = pkbdev;
   struct exynos_context *platform = (struct exynos_context *) kbdev->platform_context;

	DVFS_ASSERT(platform);

	return platform->polling_speed;
}
EXPORT_SYMBOL(exynos_stats_get_gpu_polling_speed);

void exynos_migov_set_mode(int mode)
{
	struct kbase_device *kbdev = pkbdev;
	struct exynos_context *platform = (struct exynos_context *) kbdev->platform_context;

	platform->migov_mode = mode;
}
EXPORT_SYMBOL(exynos_migov_set_mode);

void exynos_migov_set_gpu_margin(int margin)
{
	struct kbase_device *kbdev = pkbdev;
	struct exynos_context *platform = (struct exynos_context *) kbdev->platform_context;

	DVFS_ASSERT(platform);

	platform->freq_margin = margin;
}
EXPORT_SYMBOL(exynos_migov_set_gpu_margin);

#endif /* CONFIG_MALI_TSG */

#if 1 /* WA_FRAME_CNT */
int gpu_dvfs_get_wa_frame_cnt(void)
{
	struct kbase_device *kbdev = pkbdev;
	struct exynos_context *platform = (struct exynos_context *) kbdev->platform_context;
	DVFS_ASSERT(platform);
	return platform->wa_frame_cnt;
}
#endif

#if defined(CONFIG_MALI_NOTIFY_UTILISATION)
int register_frag_utils_change_notifier(struct notifier_block *nb)
{
	struct exynos_context *platform = (struct exynos_context *) pkbdev->platform_context;

	return atomic_notifier_chain_register(&platform->frag_utils_change_notifier_list, nb);
}
EXPORT_SYMBOL(register_frag_utils_change_notifier);

int unregister_frag_utils_change_notifier(struct notifier_block *nb)
{
	struct exynos_context *platform = (struct exynos_context *) pkbdev->platform_context;

	return atomic_notifier_chain_unregister(&platform->frag_utils_change_notifier_list, nb);
}
EXPORT_SYMBOL(unregister_frag_utils_change_notifier);

void notify_frag_utils_change(u32 js0_utils)
{
	struct exynos_context *platform = (struct exynos_context *) pkbdev->platform_context;

	atomic_notifier_call_chain(&platform->frag_utils_change_notifier_list, js0_utils, NULL);
}
#endif
