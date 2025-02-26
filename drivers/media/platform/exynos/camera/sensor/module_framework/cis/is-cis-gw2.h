/*
 * Samsung Exynos5 SoC series Sensor driver
 *
 *
 * Copyright (c) 2011 Samsung Electronics Co., Ltd
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#ifndef IS_CIS_GW2_H
#define IS_CIS_GW2_H

#include "is-cis.h"

#define EXT_CLK_Mhz (26)

#define SENSOR_GW2_MAX_WIDTH		(9280)
#define SENSOR_GW2_MAX_HEIGHT		(6944)

/* TODO: Check below values are valid */
#define SENSOR_GW2_FINE_INTEGRATION_TIME_MIN                0x100
#define SENSOR_GW2_FINE_INTEGRATION_TIME_MAX                0x100
#define SENSOR_GW2_COARSE_INTEGRATION_TIME_MIN              0x04
#define SENSOR_GW2_COARSE_INTEGRATION_TIME_MAX_MARGIN       0x04

#define USE_GROUP_PARAM_HOLD	(0)
#define MAX_EFS_DATA_LENGTH	780
#define CROP_SHIFT_VALUE_FROM_EFS	"/efs/FactoryApp/camera_tilt_calibration_info_3_1"
#define CROP_SHIFT_ADDR_X	0x029E
#define CROP_SHIFT_ADDR_Y	0x02A0

enum is_efs_state {
	IS_EFS_STATE_READ,
};

struct is_efs_info {
	unsigned long	efs_state;
	s16 crop_shift_delta_x;
	s16 crop_shift_delta_y;
};

enum sensor_gw2_mode_enum {
	SENSOR_GW2_9248x6936_15FPS = 0,
	SENSOR_GW2_7680X4320_30FPS,
	SENSOR_GW2_4864x3648_30FPS,
	SENSOR_GW2_4864x2736_30FPS,
	SENSOR_GW2_4624x3468_30FPS,
	SENSOR_GW2_4624x2604_30FPS,
	SENSOR_GW2_4624x2604_60FPS,
	SENSOR_GW2_3200x2400_60FPS,
	SENSOR_GW2_3200x1800_60FPS,
	SENSOR_GW2_2432x1824_30FPS,
	SENSOR_GW2_1920X1080_120FPS,
	SENSOR_GW2_1920X1080_240FPS,
};

#endif

