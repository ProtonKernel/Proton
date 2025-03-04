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

#ifndef IS_CIS_3M5_H
#define IS_CIS_3M5_H

#include "is-cis.h"

#define EXT_CLK_Mhz (26)

#define SENSOR_3M5_MAX_WIDTH		(4032)
#define SENSOR_3M5_MAX_HEIGHT		(3024)

/* TODO: Check below values are valid */
#define SENSOR_3M5_FINE_INTEGRATION_TIME_MIN                0x618
#define SENSOR_3M5_FINE_INTEGRATION_TIME_MAX                0x618
#define SENSOR_3M5_COARSE_INTEGRATION_TIME_MIN              0x7
#define SENSOR_3M5_COARSE_INTEGRATION_TIME_MAX_MARGIN       0x1C

#define USE_GROUP_PARAM_HOLD	(0)

#define MAX_EFS_DATA_LENGTH	780
#define CROP_SHIFT_VALUE_FROM_EFS	"/efs/FactoryApp/camera_tilt_calibration_info_3_1"
#define CROP_SHIFT_ADDR_X	0x029E
#define CROP_SHIFT_ADDR_Y	0x02A0
#define CROP_SHIFT_DELTA_ALIGN	32
#define CROP_SHIFT_DELTA_MAX_X	96
#define CROP_SHIFT_DELTA_MIN_X	-96
#define CROP_SHIFT_DELTA_MAX_Y	32
#define CROP_SHIFT_DELTA_MIN_Y	-64

enum is_efs_state {
	IS_EFS_STATE_READ,
};

struct is_efs_info {
	unsigned long	efs_state;
	s16 crop_shift_delta_x;
	s16 crop_shift_delta_y;
};

enum sensor_3m5_mode_enum {
	SENSOR_3M5_4000X3000_30FPS = 0,
	SENSOR_3M5_4000X2252_30FPS,
	SENSOR_3M5_2000x1128_30FPS,
	SENSOR_3M5_2000x1500_30FPS,
	SENSOR_3M5_1504x1504_30FPS,
	SENSOR_3M5_1344X756_120FPS,
};

#endif

