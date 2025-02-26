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

#ifndef IS_CIS_IMX516_H
#define IS_CIS_IMX516_H

#include "is-cis.h"

#define EXT_CLK_Mhz (26)

/* FIXME */
#define SENSOR_IMX516_MAX_WIDTH    (1280)
#define SENSOR_IMX516_MAX_HEIGHT   (482)

enum sensor_imx516_mode_enum {
	SENSOR_IMX516_1280x3840_30FPS = 0,  /* VGA : 640 * 2, 480 * 8 */
	SENSOR_IMX516_640x1920_30FPS,       /* QVGA : 320 * 2, 240 * 8  */
	SENSOR_IMX516_320x960_30FPS,	    /* QQVGA : 160 * 2, 120 * 8  */
	SENSOR_IMX516_MODE_MAX,
};

enum {
	SENSOR_IMX516_LASER_ERROR_SHOW = 0,
	SENSOR_IMX516_LASER_ERROR_APC2_EM = 1,
	SENSOR_IMX516_LASER_ERROR_APC1_EM,
	SENSOR_IMX516_LASER_ERROR_VCC_EM,
	SENSOR_IMX516_LASER_ERROR_DIF_EM,
	SENSOR_IMX516_LASER_ERROR_OC_EM,
	SENSOR_IMX516_LASER_ERROR_TEMP_EM,
	SENSOR_IMX516_LASER_ERROR_LDVCC_EM,
	SENSOR_IMX516_LASER_ERROR_DIFF_CHK_EM,
	SENSOR_IMX516_LASER_ERROR_POWER_EM,
	SENSOR_IMX516_LASER_ERROR_EXP_EM,
	SENSOR_IMX516_LASER_ERROR_DIFF2_EM,
	SENSOR_IMX516_LASER_ERROR_SHORT_EM,
	SENSOR_IMX516_LASER_ERROR_ITO_EM,
	SENSOR_IMX516_LASER_ERROR_LVDS_EM,
	SENSOR_IMX516_LASER_ERROR_ALL,
	SENSOR_IMX516_LASER_ERROR_MAX,
};

struct sensor_imx516_laser_test {
	u16 error;
	u16 addr;
	u16 data;
};

#ifdef USE_CAMERA_REAR_TOF_TX_FREQ_VARIATION
/* merge into sensor driver */
enum {
	CAM_IMX516_SET_A_REAR_DEFAULT_TX_CLOCK = 0, /* Default - 100Mhz */
	CAM_IMX516_SET_A_REAR_TX_99p9_MHZ = 0, /* 100Mhz */
	CAM_IMX516_SET_A_REAR_TX_95p8_MHZ = 1, /* 96Mhz */
	CAM_IMX516_SET_A_REAR_TX_102p6_MHZ = 2, /* 101Mhz */
};

static const u32 sensor_imx516_supported_tx_freq[] = {
	100, 96, 101,
};

#endif

int sensor_imx516_cis_get_uid_index(struct is_cis *cis, u32 mode, u32 *uid_index);
#endif

