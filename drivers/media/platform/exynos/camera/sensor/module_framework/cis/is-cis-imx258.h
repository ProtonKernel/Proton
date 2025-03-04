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

#ifndef IS_CIS_IMX258_H
#define IS_CIS_IMX258_H

#include "is-cis.h"

#define EXT_CLK_Mhz (26)

#define SENSOR_IMX258_MAX_WIDTH		(4000 + 16)
#define SENSOR_IMX258_MAX_HEIGHT	(3000 + 10)

/* TODO: Check below values are valid */
#define SENSOR_IMX258_FINE_INTEGRATION_TIME_MIN                0x54C
#define SENSOR_IMX258_FINE_INTEGRATION_TIME_MAX                0x54C
#define SENSOR_IMX258_COARSE_INTEGRATION_TIME_MIN              0x1
#define SENSOR_IMX258_COARSE_INTEGRATION_TIME_MAX_MARGIN       0xA

#define USE_GROUP_PARAM_HOLD	(0)

enum sensor_imx258_mode_enum {
	SENSOR_IMX258_4000X3000_30FPS = 0,
	SENSOR_IMX258_4000X2252_30FPS = 1,
	SENSOR_IMX258_2000X1128_60FPS = 2,
	SENSOR_IMX258_2000X1500_60FPS = 3,
	SENSOR_IMX258_1000X750_120FPS = 4,
	SENSOR_IMX258_2800X2100_30FPS = 5,
	SENSOR_IMX258_MODE_MAX,
};

#endif

