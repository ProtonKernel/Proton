/*
 * Samsung Exynos SoC series Sensor driver
 *
 *
 * Copyright (c) 2020 Samsung Electronics Co., Ltd
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#ifndef IS_CIS_HI847_H
#define IS_CIS_HI847_H

#include "is-cis.h"

#define EXT_CLK_Mhz	(26)

#define SENSOR_HI847_MAX_WIDTH	(3264)
#define SENSOR_HI847_MAX_HEIGHT	(2448)

#define USE_GROUP_PARAM_HOLD	(0)

#define SENSOR_HI847_FINE_INTEGRATION_TIME_MIN			(0x0)	//Not Support
#define SENSOR_HI847_FINE_INTEGRATION_TIME_MAX			(0x0)	//Not Support
#define SENSOR_HI847_COARSE_INTEGRATION_TIME_MIN		(4)
#define SENSOR_HI847_COARSE_INTEGRATION_TIME_MAX_MARGIN	(4)

#define SENSOR_HI847_MIN_ANALOG_GAIN_SET_VALUE			(0)		//x1.0
#define SENSOR_HI847_MAX_ANALOG_GAIN_SET_VALUE			(0xF0)	//x16.0
#define SENSOR_HI847_MIN_DIGITAL_GAIN_SET_VALUE			(0x200)	//x1.0
#define SENSOR_HI847_MAX_DIGITAL_GAIN_SET_VALUE			(0x1FFB)	//x15.99

#define SENSOR_HI847_GROUP_PARAM_HOLD_ADDR		(0x0208)
#define SENSOR_HI847_COARSE_INTEG_TIME_ADDR		(0x020A)
#define SENSOR_HI847_FRAME_LENGTH_LINE_ADDR		(0x020E)
#define SENSOR_HI847_LINE_LENGTH_PCK_ADDR		(0x0206)
#define SENSOR_HI847_ANALOG_GAIN_ADDR			(0x0213)
#define SENSOR_HI847_DIGITAL_GAIN_ADDR			(0x0214)
#define SENSOR_HI847_DIGITAL_GAIN_GR_ADDR		(0x0214)
#define SENSOR_HI847_DIGITAL_GAIN_GB_ADDR		(0x0216)
#define SENSOR_HI847_DIGITAL_GAIN_R_ADDR			(0x0218)
#define SENSOR_HI847_DIGITAL_GAIN_B_ADDR			(0x021A)

#define SENSOR_HI847_MODEL_ID_ADDR				(0x0716)
#define SENSOR_HI847_STREAM_MODE_ADDR			(0x0B00)
#define SENSOR_HI847_ISP_EN_ADDR					(0x0B04)	//B[8]: PCM, B[5]: Hscaler, B[4]: Digital gain, B[3]: DPC, B[1]: LSC, B[0]: TestPattern
#define SENSOR_HI847_MIPI_TX_OP_MODE_ADDR		(0x1002)
#define SENSOR_HI847_ISP_PLL_ENABLE_ADDR		(0x0702)

#define SENSOR_HI847_DEBUG_INFO (1)
#define SENSOR_HI847_PDAF_DISABLE (0)
#define SENSOR_HI847_OTP_READ (0)

enum sensor_hi847_mode_enum {
	SENSOR_HI847_3264X2448_30FPS,
	SENSOR_HI847_3264X1836_30FPS,
	SENSOR_HI847_1632X1224_30FPS,
	SENSOR_HI847_1632X1224_60FPS,
	SENSOR_HI847_MODE_MAX
};

const u32 sensor_hi847_rev00_mipirate[] = {
	958,		//SENSOR_HI847_3264X2448_30FPS
	910,		//SENSOR_HI847_3264X1836_30FPS, REV00 is not supported.
	455,		//SENSOR_HI847_1632X1224_30FPS, REV00 is not supported.
	400,		//SENSOR_HI847_1632X1224_60FPS
};

#define SENSOR_HI847_BRINGUP_VERSION_ID (0x0000)
#define SENSOR_HI847_BRINGUP_VERSION(cis) ({	\
	u32 rev;									\
	typecheck(struct is_cis *, cis);				\
	rev = cis->cis_data->cis_rev;				\
	(rev == SENSOR_HI847_BRINGUP_VERSION_ID); \
})

#endif
