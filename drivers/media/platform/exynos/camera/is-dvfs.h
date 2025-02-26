/*
 * Samsung Exynos5 SoC series FIMC-IS driver
 *
 *
 * Copyright (c) 2011 Samsung Electronics Co., Ltd
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#ifndef IS_DVFS_H
#define IS_DVFS_H

#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/errno.h>
#include <linux/slab.h>

#include "is-time.h"
#include "is-cmd.h"
#include "is-err.h"
#include "is-video.h"
#include "is-groupmgr.h"
#include "is-device-ischain.h"

#define	DVFS_NOT_MATCHED	0 /* not matched in this scenario */
#define	DVFS_MATCHED		1 /* matched in this scenario */
#define	DVFS_SKIP		2 /* matched, but do not anything. skip changing dvfs */

#define KEEP_FRAME_TICK_DEFAULT (5)
#define KEEP_FRAME_TICK_FASTAE	(120)
#define IS_DVFS_DUAL_TICK (4)
#define IS_DVFS_SKIP_DYNAMIC (1)
#define DVFS_SN_STR(__SCENARIO) #__SCENARIO
#define GET_DVFS_CHK_FUNC(__SCENARIO) check_ ## __SCENARIO
#define DECLARE_DVFS_CHK_FUNC(__SCENARIO) \
	int check_ ## __SCENARIO \
		(struct is_device_ischain *device, struct camera2_shot *shot, int position, int resol, \
			int fps, int stream_cnt, int streaming_cnt, unsigned long sensor_map, \
			struct is_dual_info *dual_info, ...)
#define DECLARE_EXT_DVFS_CHK_FUNC(__SCENARIO) \
	int check_ ## __SCENARIO \
		(struct is_device_sensor *device, int position, int resol, int fps, \
			int stream_cnt, int streaming_cnt, unsigned long sensor_map, \
			struct is_dual_info *dual_info, ...)
#define GET_KEY_FOR_DVFS_TBL_IDX(__HAL_VER) \
	(#__HAL_VER "_TBL_IDX")

#define DECLARE_DVFS_DT(SIZE, ...) \
	struct is_dvfs_dt_t is_dvfs_dt_arr[SIZE] = {__VA_ARGS__};
#define DECLARE_EXTERN_DVFS_DT(SIZE) \
	extern struct is_dvfs_dt_t is_dvfs_dt_arr[SIZE];

#define SIZE_HD (720 * 480)
#define SIZE_FHD (1920 * 1080)
#define SIZE_WHD (2560 * 1440)
#define SIZE_UHD (3840 * 2160)
#define SIZE_8K (7680 * 4320)
#define SIZE_16MP_FHD_BDS (3072 * 2304) /* based a 4:3 */
#define SIZE_12MP_FHD_BDS (2688 * 2016) /* based a 4:3 */
#define SIZE_12MP_QHD_BDS (3072 * 2304)
#define SIZE_12MP_UHD_BDS (4032 * 3024)
#define SIZE_8MP_FHD_BDS (2176 * 1632)
#define SIZE_8MP_QHD_BDS (3264 * 1836)
#define SIZE_17MP_FHD_BDS (2432 * 1824)
#define SIZE_17MP (4864 * 3648)

extern int debug_dvfs;

#define dbg_dvfs(level, fmt, device, args...) \
	dbg_common((debug_dvfs) >= (level), "[%d]", fmt, device->instance, ##args)

struct is_dvfs_dt_t {
	const char *parse_scenario_nm;	/* string for parsing from DTS */
	u32 scenario_id;	/* scenario_id */
	int keep_frame_tick;	/* keep qos lock during specific frames when dynamic scenario */
};

struct is_dvfs_scenario {
	u32 scenario_id;	/* scenario_id */
	char *scenario_nm;	/* string of scenario_id */
	int priority;		/* priority for dynamic scenario */
	int keep_frame_tick;	/* keep qos lock during specific frames when dynamic scenario */

	/* function pointer to check a scenario */
	int (*check_func)(struct is_device_ischain *device, struct camera2_shot *shot, int position, int resol,
			int fps, int stream_cnt, int streaming_cnt, unsigned long sensor_map, struct is_dual_info *dual_info, ...);
	int (*ext_check_func)(struct is_device_sensor *device, int position, int resol,
			int fps, int stream_cnt, int streaming_cnt, unsigned long sensor_map, struct is_dual_info *dual_info, ...);
};

struct is_dvfs_scenario_ctrl {
	int cur_scenario_id;	/* selected scenario idx */
	int cur_frame_tick;	/* remained frame tick to keep qos lock in dynamic scenario */
	int scenario_cnt;	/* total scenario count */
	int fixed_scenario_cnt;	/* always scenario count */
	int cur_scenario_idx;	/* selected scenario idx for scenarios */
	struct is_dvfs_scenario *scenarios;
};

int is_dvfs_init(struct is_resourcemgr *resourcemgr);
int is_dvfs_sel_table(struct is_resourcemgr *resourcemgr);
int is_dvfs_sel_static(struct is_device_ischain *device);
int is_dvfs_sel_dynamic(struct is_device_ischain *device, struct is_group *group,
	struct is_frame *frame);
int is_dvfs_sel_external(struct is_device_sensor *device);
int is_get_qos(struct is_core *core, u32 type, u32 scenario_id);
int is_set_dvfs(struct is_core *core, struct is_device_ischain *device, int scenario_id);
void is_dual_mode_update(struct is_device_ischain *device,
	struct is_group *group,
	struct is_frame *frame);
void is_dual_dvfs_update(struct is_device_ischain *device,
	struct is_group *group,
	struct is_frame *frame);

unsigned int is_get_bit_count(unsigned long bits);
bool is_dvfs_is_fast_ae(struct is_dvfs_ctrl *dvfs_ctrl);
#endif
