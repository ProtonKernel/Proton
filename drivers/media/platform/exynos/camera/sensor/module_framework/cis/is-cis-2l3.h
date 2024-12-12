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

#ifndef IS_CIS_2L3_H
#define IS_CIS_2L3_H

#include "is-cis.h"

#ifdef USE_CAMERA_SENSOR_RETENTION
#undef USE_CAMERA_SENSOR_RETENTION
#endif

#define EXT_CLK_Mhz (26)

#define SENSOR_2L3_MAX_WIDTH		(4000 + 0)
#define SENSOR_2L3_MAX_HEIGHT		(3000 + 0)

#define SENSOR_2L3_FINE_INTEGRATION_TIME_MIN                0x100
#define SENSOR_2L3_FINE_INTEGRATION_TIME_MAX                0x100
#define SENSOR_2L3_COARSE_INTEGRATION_TIME_MIN              0x02
#define SENSOR_2L3_COARSE_INTEGRATION_TIME_MAX_MARGIN       0x10

#ifdef USE_CAMERA_FACTORY_DRAM_TEST
#define SENSOR_2L3_DRAMTEST_SECTION2_FCOUNT	(8)
#endif

#define USE_GROUP_PARAM_HOLD	(0)

enum sensor_2l3_mode_enum {
	/* MODE 3 */
	SENSOR_2L3_4000X3000_30FPS = 0,
	SENSOR_2L3_4000X2252_30FPS = 1,
	SENSOR_2L3_4000X2252_60FPS = 2,
	SENSOR_2L3_1984X1488_30FPS = 3,
	SENSOR_2L3_1008X756_120FPS = 4,
	SENSOR_2L3_1280X720_240FPS = 5,
	SENSOR_2L3_1280X720_480FPS = 6,
	SENSOR_2L3_MODE_MAX,
};

static bool sensor_2l3_support_wdr[] = {
	/* MODE 3 */
	false, //SENSOR_2L3_4000X3000_30FPS = 0,
	false, //SENSOR_2L3_4000X2250_30FPS = 1,
	false, //SENSOR_2L3_4000X2250_60FPS = 2,
	false, //SENSOR_2L3_1984X1488_30FPS = 3,
	false, //SENSOR_2L3_1008X756_120FPS = 4,
	false, //SENSOR_2L3_1280X720_240FPS = 5,
	false, //SENSOR_2L3_1280X720_480FPS = 6,
};

int sensor_2l3_cis_stream_on(struct v4l2_subdev *subdev);
int sensor_2l3_cis_stream_off(struct v4l2_subdev *subdev);
int sensor_2l3_cis_set_lownoise_mode_change(struct v4l2_subdev *subdev);
#endif
