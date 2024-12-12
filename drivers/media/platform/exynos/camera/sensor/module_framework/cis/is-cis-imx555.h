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

#ifndef IS_CIS_IMX555_H
#define IS_CIS_IMX555_H

#include "is-cis.h"

#define EXT_CLK_Mhz (26)

#define SENSOR_IMX555_MAX_WIDTH									(4032 + 0)
#define SENSOR_IMX555_MAX_HEIGHT								(3024 + 0)

#define SENSOR_IMX555_FINE_INTEGRATION_TIME_MIN					0x100
#define SENSOR_IMX555_FINE_INTEGRATION_TIME_MAX					0x100
#define SENSOR_IMX555_COARSE_INTEGRATION_TIME_MIN				0x2
#define SENSOR_IMX555_COARSE_INTEGRATION_TIME_MAX_MARGIN		0x32
#define SENSOR_IMX555_ANA_GAIN_GLOBAL_MAX						0x380
#define SENSOR_IMX555_DIG_GAIN_GLOBAL_MIN						0x100
#define SENSOR_IMX555_DIG_GAIN_GLOBAL_MAX						0xFD9

#define SENSOR_IMX555_MODEL_ID_ADDR 							0x0000
#define SENSOR_IMX555_REVISION_NUMBER_ADDR						0x0002
#define SENSOR_IMX555_FRAME_COUNT_ADDR							0x0005
#define SENSOR_IMX555_MODE_SEL_ADDR								0x0100
#define SENSOR_IMX555_FRAME_LENGTH_LINE_ADDR					0x0340
#define SENSOR_IMX555_GROUP_PARAM_HOLD_ADDR						0x0104
#define SENSOR_IMX555_EBD_SIZE_V_LOW_ADDR						0xBCF0
#define SENSOR_IMX555_EBD_SIZE_V_HIGH_ADDR						0xBCF1
#define SENSOR_IMX555_COARSE_INTEG_TIME_ADDR					0x0202
#define SENSOR_IMX555_ANA_GAIN_GLOBAL_ADDR						0x0204
#define SENSOR_IMX555_DIG_GAIN_GLOBAL_ADDR						0x020E
#define SENSOR_IMX555_HDR_MODE_ADDR								0x0220
#define SENSOR_IMX555_ST_COARSE_INTEG_TIME_ADDR					0x0224
#define SENSOR_IMX555_CIT_LSHIFT_ADDR							0x3100
#define SENSOR_IMX555_FLL_LSHIFT_ADDR							0x3101

#define USE_GROUP_PARAM_HOLD	(1)

enum sensor_imx555_mode_enum {
	SENSOR_IMX555_4032X3024_30FPS = 0,	/* HDR M3-11 */
	SENSOR_IMX555_MODE_MAX,
};

static bool sensor_imx555_support_wdr[] = {
	true, //SENSOR_IMX555_4032X3024_30FPS
	false,
};

#endif
