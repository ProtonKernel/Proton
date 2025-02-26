/*
 * Samsung Exynos5 SoC series Sensor driver
 *
 *
 * Copyright (c) 2018 Samsung Electronics Co., Ltd
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#ifndef IS_CIS_GM1SP_H
#define IS_CIS_GM1SP_H

#include "is-cis.h"

#define EXT_CLK_Mhz (26)

#define SENSOR_GM1SP_MAX_WIDTH		(4000)
#define SENSOR_GM1SP_MAX_HEIGHT		(3000)

/* TODO: Check below values are valid */
#define SENSOR_GM1SP_FINE_INTEGRATION_TIME_MIN                0x0
#define SENSOR_GM1SP_FINE_INTEGRATION_TIME_MAX                0x0 /* Not used */
#define SENSOR_GM1SP_COARSE_INTEGRATION_TIME_MIN              0x2 /* TODO */
#define SENSOR_GM1SP_COARSE_INTEGRATION_TIME_MAX_MARGIN       0x2 /* TODO */

#define SENSOR_GM1SP_LONG_TERM_EXPOSURE_SHITFER		(6)

#define USE_GROUP_PARAM_HOLD	(0)

#endif

