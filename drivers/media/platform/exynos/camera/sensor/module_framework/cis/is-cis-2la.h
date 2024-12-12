/*
 * Samsung Exynos SoC series Sensor driver
 *
 *
 * Copyright (c) 2016 Samsung Electronics Co., Ltd
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#ifndef IS_CIS_2LA_H
#define IS_CIS_2LA_H

#include "is-cis.h"

#define EXT_CLK_Mhz (26)

#define SENSOR_2LA_MAX_WIDTH		(4032 + 0)
#define SENSOR_2LA_MAX_HEIGHT		(3024 + 0)

#define SENSOR_2LA_FINE_INTEGRATION_TIME_MIN                0x100
#define SENSOR_2LA_FINE_INTEGRATION_TIME_MAX                0x100
#define SENSOR_2LA_COARSE_INTEGRATION_TIME_MIN              0x10
#define SENSOR_2LA_COARSE_INTEGRATION_TIME_MAX_MARGIN       0x10

#define USE_GROUP_PARAM_HOLD	(0)

enum sensor_2la_mode_enum {
	SENSOR_2LA_4032X3024_30FPS = 0,
	SENSOR_2LA_4032X2268_60FPS = 1,
	SENSOR_2LA_4032X2268_30FPS = 2,
	SENSOR_2LA_2016X1134_240FPS = 3,
	SENSOR_2LA_2016X1134_120FPS = 4,
	SENSOR_2LA_4032X3024_60FPS = 5,
	SENSOR_2LA_MODE_MAX,
};

static bool sensor_2la_support_wdr[] = {
	false, //SENSOR_2LA_4032X3024_30FPS = 0,
	false, //SENSOR_2LA_4032X2268_60FPS = 1,
	false, //SENSOR_2LA_4032X2268_30FPS = 2,
	false, //SENSOR_2LA_2016X1134_240FPS = 3,
	false, //SENSOR_2LA_2016X1134_120FPS = 4,
	false, //SENSOR_2LA_4032X3024_60FPS = 5,
};

int sensor_2la_cis_stream_on(struct v4l2_subdev *subdev);
int sensor_2la_cis_stream_off(struct v4l2_subdev *subdev);
#endif
