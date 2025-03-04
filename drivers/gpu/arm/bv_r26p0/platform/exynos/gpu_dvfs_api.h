/* drivers/gpu/arm/.../platform/gpu_dvfs_api.h
 *
 * Copyright 2011-2020 by S.LSI. Samsung Electronics Inc.
 * San#24, Nongseo-Dong, Giheung-Gu, Yongin, Korea
 *
 * Samsung SoC Mali-T Series DVFS driver
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software FoundatIon.
*/

/**
 * @file gpu_dvfs_api.h
 * DVFS
*/

#ifndef _GPU_DVFS_API_H_
#define _GPU_DVFS_API_H_
#ifndef CSTD_UNUSED
#define CSTD_UNUSED(x)  ((void)(x))
#endif /* CSTD_UNUSED */

int gpu_dvfs_get_level(int clock);
int gpu_dvfs_get_cur_clock(void);
int gpu_dvfs_get_min_freq(void);
int gpu_dvfs_get_max_freq(void);

#ifdef CONFIG_MALI_DVFS
int exynos_stats_get_gpu_max_lock(void);
int exynos_stats_get_gpu_min_lock(void);
#else
static inline int exynos_stats_get_gpu_max_lock(void) { return 0; };
static inline int exynos_stats_get_gpu_min_lock(void) { return 0; };
#endif

unsigned int exynos_stats_get_gpu_table_size(void);
unsigned int *exynos_stats_get_gpu_freq_table(void);
ktime_t *exynos_stats_get_gpu_time_in_state(void);

#ifdef CONFIG_MALI_TSG
ktime_t *exynos_stats_get_gpu_queued_job_time(void);
ktime_t exynos_stats_get_gpu_queued_last_updated(void);
void exynos_stats_set_gpu_polling_speed(int polling_speed);
int exynos_stats_get_gpu_polling_speed(void);
void exynos_migov_set_mode(int mode);
void exynos_migov_set_gpu_margin(int margin);
#else
static inline ktime_t *exynos_stats_get_gpu_queued_job_time(void) { return NULL; };
static inline ktime_t exynos_stats_get_gpu_queued_last_updated(void) { return 0; };
static inline void exynos_stats_set_gpu_polling_speed(int polling_speed) { CSTD_UNUSED(polling_speed); };
static inline int exynos_stats_get_gpu_polling_speed(void) { return 0; };
static inline void exynos_migov_set_mode(int mode) { CSTD_UNUSED(mode); };
static inline void exynos_migov_set_gpu_margin(int margin) { CSTD_UNUSED(margin); };
#endif /* CONFIG_MALI_TSG */
#endif /* _GPU_DVFS_API_H_ */
