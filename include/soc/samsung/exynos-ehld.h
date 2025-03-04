/*
 * Copyright (c) 2018 Samsung Electronics Co., Ltd.
 *		http://www.samsung.com
 *
 * Exynos Early Hardlockup Detector for Samsung EXYNOS SoC
 * By Hosung Kim (hosung0.kim@samsung.com)
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 */

#ifndef EXYNOS_EHLD__H
#define EXYNOS_EHLD__H

#define NUM_TRACE			(32)

#define EHLD_STAT_NORMAL		(0x0)
#define EHLD_STAT_LOCKUP_WARN		(0x1)
#define EHLD_STAT_LOCKUP_SW		(0x2)
#define EHLD_STAT_LOCKUP_HW		(0x3)

#define EHLD_OFFSET_CORE_STAT		(0x460)

struct ehld_data {
	unsigned long long		time[NUM_TRACE];
	unsigned long long		instret[NUM_TRACE];
	unsigned long long		instrun[NUM_TRACE];
	unsigned long long		pmpcsr[NUM_TRACE];
	unsigned long			data_ptr;
};

#if IS_ENABLED(CONFIG_EXYNOS_EHLD)
extern void ehld_do_action(int cpu, unsigned int lockup_level);
extern void ehld_prepare_panic(void);
extern void ehld_event_update_allcpu(void);
extern int adv_tracer_ehld_init(void *);
extern int adv_tracer_ehld_set_enable(int en);
extern int adv_tracer_ehld_set_init_val(u32 interval,
					u32 count,
					u32 cpu_mask);
extern int adv_tracer_ehld_set_interval(u32 interval);
extern int adv_tracer_ehld_set_warn_count(u32 count);
extern int adv_tracer_ehld_set_lockup_count(u32 count);
extern u32 adv_tracer_ehld_get_interval(void);
extern int adv_tracer_ehld_get_enable(void);
extern struct ehld_data *ehld_get_ctrl_data(int cpu);
#else
#define ehld_do_action(a,b)		do { } while (0)
#define ehld_prepare_panic(void)	do { } while (0)
#define ehld_event_update_allcpu(void)  do { } while (0)
inline int adv_tracer_ehld_init(void *p) { return -1; }
inline int adv_tracer_ehld_set_enable(int en) {	return -1; }
inline int adv_tracer_ehld_set_init_val(u32 interval,
					u32 count,
					u32 cpu_mask) {	return -1; }
inline int adv_tracer_ehld_set_interval(u32 interval) { return -1; }
inline int adv_tracer_ehld_set_warn_count(u32 count) { return -1; }
inline int adv_tracer_ehld_set_lockup_count(u32 count) { return -1; }
inline u32 adv_tracer_ehld_get_interval(void) { return -1; }
inline int adv_tracer_ehld_get_enable(void) { return -1; }
inline struct ehld_data *ehld_get_ctrl_data(int cpu) { return NULL; }
#endif

#endif
