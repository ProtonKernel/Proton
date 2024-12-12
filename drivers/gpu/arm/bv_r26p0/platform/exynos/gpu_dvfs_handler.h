/* drivers/gpu/arm/.../platform/gpu_dvfs_handler.h
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
 * @file gpu_dvfs_handler.h
 * DVFS
 */

#ifndef _GPU_DVFS_HANDLER_H_
#define _GPU_DVFS_HANDLER_H_

#define UPDATE_PERIOD	250

#define DVFS_ASSERT(x) \
do { if (x) break; \
	printk(KERN_EMERG "### ASSERTION FAILED %s: %s: %d: %s\n", __FILE__, __func__, __LINE__, #x); dump_stack(); \
} while (0)

typedef enum {
	GPU_DVFS_MAX_LOCK = 0,
	GPU_DVFS_MIN_LOCK,
	GPU_DVFS_MAX_UNLOCK,
	GPU_DVFS_MIN_UNLOCK,
} gpu_dvfs_lock_command;

typedef enum {
	GPU_DVFS_BOOST_SET = 0,
	GPU_DVFS_BOOST_UNSET,
	GPU_DVFS_BOOST_GPU_UNSET,
	GPU_DVFS_BOOST_END,
} gpu_dvfs_boost_command;

int kbase_platform_dvfs_event(struct kbase_device *kbdev, u32 utilisation);
int gpu_dvfs_handler_init(struct kbase_device *kbdev);
int gpu_dvfs_handler_deinit(struct kbase_device *kbdev);

/* gpu_dvfs_api.c */
int gpu_set_target_clk_vol(int clk, bool pending_is_allowed, bool force);
int gpu_set_target_clk_vol_pending(int clk);
int gpu_dvfs_boost_lock(gpu_dvfs_boost_command boost_command);
int gpu_dvfs_clock_lock(gpu_dvfs_lock_command lock_command, gpu_dvfs_lock_type lock_type, int clock);
void gpu_dvfs_timer_control(bool enable);
int gpu_dvfs_on_off(bool enable);
int gpu_dvfs_governor_change(int governor_type);
int gpu_dvfs_init_time_in_state(void);
int gpu_dvfs_update_time_in_state(int clock);
int gpu_dvfs_get_level(int clock);
int gpu_dvfs_get_level_clock(int clock);
int gpu_dvfs_get_voltage(int clock);
int gpu_dvfs_get_cur_asv_abb(void);
int gpu_dvfs_get_clock(int level);
int gpu_dvfs_get_step(void);
int gpu_dvfs_get_cur_clock(void);
int gpu_dvfs_get_utilization(void);
int gpu_dvfs_get_max_freq(void);
bool gpu_dvfs_get_need_cpu_qos(void);
int gpu_dvfs_decide_max_clock(struct exynos_context *platform);

#ifdef CONFIG_MALI_TSG
int exynos_stats_get_gpu_cur_idx(void);
int exynos_stats_get_gpu_coeff(void);
unsigned int exynos_stats_get_gpu_table_size(void);
unsigned int *exynos_stats_get_gpu_freq_table(void);
unsigned int *exynos_stats_get_gpu_volt_table(void);
ktime_t *exynos_stats_get_gpu_time_in_state(void);
ktime_t *exynos_stats_get_gpu_queued_job_time(void);
void exynos_stats_set_gpu_polling_speed(int polling_speed);
int exynos_stats_get_gpu_polling_speed(void);
#endif
/* gpu_utilization */
int gpu_dvfs_start_env_data_gathering(struct kbase_device *kbdev);
int gpu_dvfs_stop_env_data_gathering(struct kbase_device *kbdev);
int gpu_dvfs_reset_env_data(struct kbase_device *kbdev);
int gpu_dvfs_calculate_env_data(struct kbase_device *kbdev);
int gpu_dvfs_calculate_env_data_ppmu(struct kbase_device *kbdev);
int gpu_dvfs_utilization_init(struct kbase_device *kbdev);
int gpu_dvfs_utilization_deinit(struct kbase_device *kbdev);
void gpu_register_out_data(void (*fn)(u64 *cnt));
bool update_gpu_data(int input, int input2, int freq);

/* gpu_pmqos.c */
typedef enum {
	GPU_CONTROL_PM_QOS_INIT = 0,
	GPU_CONTROL_PM_QOS_DEINIT,
	GPU_CONTROL_PM_QOS_SET,
	GPU_CONTROL_PM_QOS_RESET,
	GPU_CONTROL_PM_QOS_EGL_SET,
	GPU_CONTROL_PM_QOS_EGL_RESET,
} gpu_pmqos_state;

int gpu_pm_qos_command(struct exynos_context *platform, gpu_pmqos_state state);
int gpu_mif_pmqos(struct exynos_context *platform, int mem_freq);

struct gpu_shared_d  {
	u64	out_data[4];
	u64	input;
	u64	input2;
	u64	freq;
	u64	flimit;
};

struct gpu_data {
	void	(*get_out_data)(u64 *cnt);
	u64	last_out_data;
	u64	freq;
	u64	input;
	u64	input2;
	int	update_idx;
	ktime_t	last_update_t;
	struct	gpu_shared_d sd;
	int	odidx;
};
#endif /* _GPU_DVFS_HANDLER_H_ */
