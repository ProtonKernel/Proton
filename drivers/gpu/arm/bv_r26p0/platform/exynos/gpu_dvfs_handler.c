/* drivers/gpu/arm/.../platform/gpu_dvfs_handler.c
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
 * @file gpu_dvfs_handler.c
 * DVFS
 */

#include <mali_kbase.h>

#include "mali_kbase_platform.h"
#include "gpu_control.h"
#include "gpu_dvfs_handler.h"
#include "gpu_dvfs_governor.h"

#if defined(CONFIG_MALI_NOTIFY_UTILISATION)
#include <linux/notifier.h>
#endif

extern struct kbase_device *pkbdev;

#ifdef CONFIG_MALI_DVFS

struct gpu_data gpud;

void gpu_register_out_data(void (*fn)(u64 *cnt))
{
	gpud.get_out_data = fn;
}
EXPORT_SYMBOL_GPL(gpu_register_out_data);

static inline int get_outd_idx(int idx)
{
	if (++idx >= 4)
		return 0;
	return idx;
}

bool update_gpu_data(int input, int input2, int freq)
{
	ktime_t now = ktime_get();
	u64 out_data;

	if (unlikely(!gpud.get_out_data)) {
		gpud.sd.input = 0;
		gpud.sd.input2 = 0;
		return false;
	}

	gpud.freq += freq;
	gpud.input += input;
	gpud.update_idx++;

	if (UPDATE_PERIOD > ktime_to_ms(ktime_sub(now, gpud.last_update_t)) || !gpud.update_idx)
		return false;

	gpud.sd.freq = gpud.freq / gpud.update_idx;
	gpud.sd.input = gpud.input/ gpud.update_idx;
	gpud.sd.input2 = input2;
	gpud.freq = 0;
	gpud.input = 0;
	gpud.update_idx = 0;

	gpud.get_out_data(&out_data);
	gpud.odidx = get_outd_idx(gpud.odidx);
	gpud.sd.out_data[gpud.odidx] = out_data - gpud.last_out_data;
	gpud.last_out_data = out_data;
	gpud.last_update_t = now;

	return true;
}

int kbase_platform_dvfs_event(struct kbase_device *kbdev, u32 utilisation)
{
	struct exynos_context *platform;
	char *env[2] = {"FEATURE=GPUI", NULL};

	platform = (struct exynos_context *) kbdev->platform_context;

	DVFS_ASSERT(platform);

	if(platform->fault_count >= 5 && platform->bigdata_uevent_is_sent == false)
	{
		platform->bigdata_uevent_is_sent = true;
		kobject_uevent_env(&kbdev->dev->kobj, KOBJ_CHANGE, env);
	}

	mutex_lock(&platform->gpu_dvfs_handler_lock);
	if (gpu_control_is_power_on(kbdev)) {
		int clk = 0;

		gpu_dvfs_calculate_env_data(kbdev);
		clk = gpu_dvfs_decide_next_freq(kbdev, platform->env_data.utilization);
		gpu_set_target_clk_vol(clk, true, false);
	}
	mutex_unlock(&platform->gpu_dvfs_handler_lock);

#if IS_ENABLED(CONFIG_MALI_DELAYED_IFPO_ON_RESUME)
	if (platform->ifpo_delay > 0)
		platform->ifpo_delay--;
#endif

	GPU_LOG(DVFS_DEBUG, DUMMY, 0u, 0u, "dvfs hanlder is called\n");

	return 0;
}

int gpu_dvfs_handler_init(struct kbase_device *kbdev)
{
	struct exynos_context *platform = (struct exynos_context *) kbdev->platform_context;

	DVFS_ASSERT(platform);

	if (!platform->dvfs_status)
		platform->dvfs_status = true;


#ifdef CONFIG_MALI_PM_QOS
	gpu_pm_qos_command(platform, GPU_CONTROL_PM_QOS_INIT);
#endif /* CONFIG_MALI_PM_QOS */

	gpu_set_target_clk_vol(platform->table[platform->step].clock, false, false);
#if defined(CONFIG_MALI_NOTIFY_UTILISATION)
	ATOMIC_INIT_NOTIFIER_HEAD(&platform->frag_utils_change_notifier_list);
#endif
	GPU_LOG(DVFS_INFO, DUMMY, 0u, 0u, "dvfs handler initialized\n");
	return 0;
}

int gpu_dvfs_handler_deinit(struct kbase_device *kbdev)
{
	struct exynos_context *platform = (struct exynos_context *) kbdev->platform_context;

	DVFS_ASSERT(platform);

	if (platform->dvfs_status)
		platform->dvfs_status = false;

#ifdef CONFIG_MALI_PM_QOS
	gpu_pm_qos_command(platform, GPU_CONTROL_PM_QOS_DEINIT);
#endif /* CONFIG_MALI_PM_QOS */


	GPU_LOG(DVFS_INFO, DUMMY, 0u, 0u, "dvfs handler de-initialized\n");
	return 0;
}
#else
#define gpu_dvfs_event_proc(q) do { } while (0)
int kbase_platform_dvfs_event(struct kbase_device *kbdev, u32 utilisation)
{
	return 0;
}
#endif /* CONFIG_MALI_DVFS */
