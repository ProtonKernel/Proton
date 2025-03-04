/*
 * Samsung Exynos5 SoC series Sensor driver
 *
 *
 * Copyright (c) 2019 Samsung Electronics Co., Ltd
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#ifndef IS_CIS_IMX518_H
#define IS_CIS_IMX518_H

#include "is-cis.h"

#define EXT_CLK_Mhz (26)

/* FIXME */
#define SENSOR_IMX518_MAX_WIDTH    (1280)
#define SENSOR_IMX518_MAX_HEIGHT   (482)

enum sensor_imx518_mode_enum {
	SENSOR_IMX518_1280x3840_30FPS = 0,  /* VGA : 640 * 2, 480 * 8 */
	SENSOR_IMX518_640x1920_30FPS,       /* QVGA : 320 * 2, 240 * 8  */
	SENSOR_IMX518_320x480_30FPS,	    /* QQVGA(AF) : 160 * 2, 120 * 4  */
	SENSOR_IMX518_MODE_MAX,
};

enum LASER_MODE {
    LASER_MODE_NORMAL,
    LASER_MODE_LIVE_FOCUS,
};

enum {
	SENSOR_IMX518_LASER_ERROR_SHOW = 0,
	SENSOR_IMX518_LASER_ERROR_APC2_EM = 1,
	SENSOR_IMX518_LASER_ERROR_APC1_EM,
	SENSOR_IMX518_LASER_ERROR_VCC_EM,
	SENSOR_IMX518_LASER_ERROR_DIF_EM,
	SENSOR_IMX518_LASER_ERROR_OC_EM,
	SENSOR_IMX518_LASER_ERROR_TEMP_EM,
	SENSOR_IMX518_LASER_ERROR_LDVCC_EM,
	SENSOR_IMX518_LASER_ERROR_DIFF_CHK_EM,
	SENSOR_IMX518_LASER_ERROR_POWER_EM,
	SENSOR_IMX518_LASER_ERROR_EXP_EM,
	SENSOR_IMX518_LASER_ERROR_DIFF2_EM,
	SENSOR_IMX518_LASER_ERROR_SHORT_EM,
	SENSOR_IMX518_LASER_ERROR_ITO_EM,
	SENSOR_IMX518_LASER_ERROR_LVDS_EM,
	SENSOR_IMX518_LASER_ERROR_ALL,
	SENSOR_IMX518_LASER_ERROR_MAX,
};

struct sensor_imx518_laser_test {
	u16 error;
	u16 addr;
	u16 data;
};

#ifdef USE_CAMERA_REAR_TOF_TX_FREQ_VARIATION
/* merge into sensor driver */
static const u32 sensor_imx518_supported_tx_freq[] = {
	100, 96, 101,
};

#endif

int sensor_imx518_cis_get_uid_index(struct is_cis *cis, u32 mode, u32 *uid_index);
#endif

