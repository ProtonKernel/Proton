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

#ifndef IS_CIS_IMX586_H
#define IS_CIS_IMX586_H

#include "is-cis.h"

#define EXT_CLK_Mhz (26)

#define SENSOR_IMX586_MAX_WIDTH          (8000 + 0)
#define SENSOR_IMX586_MAX_HEIGHT         (6000 + 0)

#define SENSOR_IMX586_SPDC_CAL_SIZE		2304
#define SENSOR_IMX586_PDXTC_CAL_SIZE		384

/* Related Sensor Parameter */
#define USE_GROUP_PARAM_HOLD                      (1)
#define TOTAL_NUM_OF_IVTPX_CHANNEL                (8)

#define SENSOR_IMX586_FINE_INTEGRATION_TIME                    (6320)    //FINE_INTEG_TIME is a fixed value (0x0200: 16bit - read value is 0x18b0)
#define SENSOR_IMX586_COARSE_INTEGRATION_TIME_MIN              (5)
#define SENSOR_IMX586_COARSE_INTEGRATION_TIME_MIN_FOR_PDAF     (5)//(6)  // To Do: Temporally min integration is setted '5' until applying the PDAF because when bring-up is not applied the PDAF.
#define SENSOR_IMX586_COARSE_INTEGRATION_TIME_MIN_FOR_V2H2     (6)
#define SENSOR_IMX586_COARSE_INTEGRATION_TIME_MAX_MARGIN       (48)

#define SENSOR_IMX586_OTP_PAGE_SETUP_ADDR         (0x0A02)
#define SENSOR_IMX586_OTP_READ_TRANSFER_MODE_ADDR (0x0A00)
#define SENSOR_IMX586_OTP_STATUS_REGISTER_ADDR    (0x0A01)
#define SENSOR_IMX586_OTP_CHIP_REVISION_ADDR      (0x0018)

#define SENSOR_IMX586_FRAME_LENGTH_LINE_ADDR      (0x0340)
#define SENSOR_IMX586_LINE_LENGTH_PCK_ADDR        (0x0342)
#define SENSOR_IMX586_FINE_INTEG_TIME_ADDR        (0x0200)
#define SENSOR_IMX586_COARSE_INTEG_TIME_ADDR      (0x0202)
#define SENSOR_IMX586_ANALOG_GAIN_ADDR            (0x0204)
#define SENSOR_IMX586_DIG_GAIN_ADDR               (0x020E)

#define SENSOR_IMX586_MIN_ANALOG_GAIN_SET_VALUE   (112)
#define SENSOR_IMX586_MAX_ANALOG_GAIN_SET_VALUE   (1008)
#define SENSOR_IMX586_MIN_DIGITAL_GAIN_SET_VALUE  (0x0100)
#define SENSOR_IMX586_MAX_DIGITAL_GAIN_SET_VALUE  (0x0FFF)

#define SENSOR_IMX586_ABS_GAIN_GR_SET_ADDR        (0x0B8E)
#define SENSOR_IMX586_ABS_GAIN_R_SET_ADDR         (0x0B90)
#define SENSOR_IMX586_ABS_GAIN_B_SET_ADDR         (0x0B92)
#define SENSOR_IMX586_ABS_GAIN_GB_SET_ADDR        (0x0B94)

/* Related Function Option */
#define SENSOR_IMX586_WRITE_PDAF_CAL              (1)
#define SENSOR_IMX586_WRITE_SENSOR_CAL            (1)
#define SENSOR_IMX586_CAL_DEBUG                   (0)
#define SENSOR_IMX586_DEBUG_INFO                  (0)

enum sensor_imx586_mode_enum {
	/* 2x2 Binning 30Fps */
	SENSOR_IMX586_REMOSAIC_FULL_8000X6000_15FPS = 0,
	SENSOR_IMX586_2X2BIN_FULL_4000X3000_30FPS,
	SENSOR_IMX586_REMOSAIC_CROP_4000X3000_30FPS,
	SENSOR_IMX586_2X2BIN_CROP_4000X2252_30FPS,
	SENSOR_IMX586_2X2BIN_CROP_4000X2252_60FPS,
	SENSOR_IMX586_2X2BIN_CROP_1984X1488_30FPS,
	SENSOR_IMX586_2X2BIN_V2H2_992X744_120FPS,
	SENSOR_IMX586_REMOSAIC_CROP_4000X2252_30FPS,
};

#endif


