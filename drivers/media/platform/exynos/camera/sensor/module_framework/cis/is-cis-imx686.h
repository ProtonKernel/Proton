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

#ifndef IS_CIS_IMX686_H
#define IS_CIS_IMX686_H

#include "is-cis.h"

#define EXT_CLK_Mhz (26)

#define SENSOR_IMX686_MAX_WIDTH	            (9248)
#define SENSOR_IMX686_MAX_HEIGHT            (6936)


/* Related Sensor Parameter */
#define USE_GROUP_PARAM_HOLD                      (1)
#define TOTAL_NUM_OF_IVTPX_CHANNEL                (16)

#define SENSOR_IMX686_FINE_INTEGRATION_TIME        (3920)    //FINE_INTEG_TIME is a fixed value (0x0200: 16bit - read value is 0xfb0 for 4:3@30fps)
#define SENSOR_IMX686_COARSE_INTEGRATION_TIME_MIN              (1)
#define SENSOR_IMX686_COARSE_INTEGRATION_TIME_MIN_FOR_PDAF     (4)
#define SENSOR_IMX686_COARSE_INTEGRATION_TIME_MIN_FOR_V2H2     (2)
#define SENSOR_IMX686_COARSE_INTEGRATION_TIME_MAX_MARGIN       (64)

#define SENSOR_IMX686_OTP_PAGE_SETUP_ADDR         (0x0A02)
#define SENSOR_IMX686_OTP_READ_TRANSFER_MODE_ADDR (0x0A00)
#define SENSOR_IMX686_OTP_STATUS_REGISTER_ADDR    (0x0A01)
#define SENSOR_IMX686_OTP_CHIP_REVISION_ADDR      (0x0018)

#define SENSOR_IMX686_MODEL_ID_ADDR               (0x0000)
#define SENSOR_IMX686_REVISION_NUM_ADDR           (0x0002)
#define SENSOR_IMX686_FRAME_COUNT_ADDR            (0x0005)
#define SENSOR_IMX686_SETUP_MODE_SELECT_ADDR      (0x0100)
#define SENSOR_IMX686_GROUP_PARAM_HOLD_ADDR       (0x0104)
#define SENSOR_IMX686_FRAME_LENGTH_LINE_ADDR      (0x0340)
#define SENSOR_IMX686_LINE_LENGTH_PCK_ADDR        (0x0342)
#define SENSOR_IMX686_FINE_INTEG_TIME_ADDR        (0x0200)
#define SENSOR_IMX686_COARSE_INTEG_TIME_ADDR      (0x0202)
#define SENSOR_IMX686_ANALOG_GAIN_ADDR            (0x0204)
#define SENSOR_IMX686_DIG_GAIN_ADDR               (0x020E)

#define SENSOR_IMX686_MIN_ANALOG_GAIN_SET_VALUE   (0)
#define SENSOR_IMX686_MAX_ANALOG_GAIN_SET_VALUE   (1008)
#define SENSOR_IMX686_MIN_DIGITAL_GAIN_SET_VALUE  (0x0100)
#define SENSOR_IMX686_MAX_DIGITAL_GAIN_SET_VALUE  (0x0FFF)

#define SENSOR_IMX686_ABS_GAIN_GR_SET_ADDR        (0x0B8E)
#define SENSOR_IMX686_ABS_GAIN_R_SET_ADDR         (0x0B90)
#define SENSOR_IMX686_ABS_GAIN_B_SET_ADDR         (0x0B92)
#define SENSOR_IMX686_ABS_GAIN_GB_SET_ADDR        (0x0B94)

#define SENSOR_IMX686_EBD_CONTROL_ADDR            (0xBCF1)

/* Related EEPROM CAL */
#define SENSOR_IMX686_LRC_CAL_BASE_REAR           (0x15C0)
#define SENSOR_IMX686_LRC_CAL_SIZE                (504)
#define SENSOR_IMX686_LRC_REG_ADDR                (0x7b00)

#define SENSOR_IMX686_QUAD_SENS_CAL_BASE_REAR     (0x0970)
#define SENSOR_IMX686_QUAD_SENS_CAL_SIZE          (3024)
#define SENSOR_IMX686_QUAD_SENS_REG_ADDR          (0xCA00)

#define SENSOR_IMX686_LRC_DUMP_NAME               "/data/vendor/camera/IMX686_LRC_DUMP.bin"
#define SENSOR_IMX686_QSC_DUMP_NAME               "/data/vendor/camera/IMX686_QSC_DUMP.bin"


/* Related Function Option */
#define SENSOR_IMX686_WRITE_PDAF_CAL              (1)
#define SENSOR_IMX686_WRITE_SENSOR_CAL            (1)
#define SENSOR_IMX686_CAL_DEBUG                   (0)
#define SENSOR_IMX686_DEBUG_INFO                  (0)
#define SENSOR_IMX686_EBD_LINE_DISABLE            (0)


/*
 * [Mode Information]
 *
 * Reference File : IMX686-FAKH5-SEC-CPHY-26M_RegisterSetting_ver3.00-4.00_b5_191219.xlsx
 * Update Data    : 2019-1220
 * Author         : takkyoum.kim
 *
 * - Global Setting with ImageQuality setting -
 *
 * - 2x2 BIN For Still Preview / Capture / Recording -
 *    [  0 ] 2Bin_A     : 2x2 Binning 4624X3468 30fps      : Single Still Preview/Capture (4:3)   ,  MIPI lane: 3, MIPI data rate(Mbps/lane): 2483
 *    [  1 ] 2Bin_B     : 2x2 Binning 4624X2604 30fps      : Single Still Preview/Capture (16:9)  ,  MIPI lane: 3, MIPI data rate(Mbps/lane): 2483
 *    [  2 ] 2Bin_C     : 2x2 Binning 4624X2604 60fps      : Single Still Preview/Capture (16:9)  ,  MIPI lane: 3, MIPI data rate(Mbps/lane): 2483
 *
 * - Remosaic Full For Still Remosaic Capture -
 *    [  3 ] Full          : Remosaic Full 9248x6936 20fps   : Single Still Remosaic Capture (4:3)  ,  MIPI lane: 3, MIPI data rate(Mbps/lane): 2483
 *
 * - Remosaic Crop For RemosaicCropZoom Mode -
 *    [  4 ] Full_CropA : Remosaic Crop 4624x3468 30fps : Single Still Preview (4:3)               ,  MIPI lane: 3, MIPI data rate(Mbps/lane): 2483
 *    [  5 ] Full_CropB : Remosaic Crop 4624x2604 30fps : Single Still Preview (16:9)              ,  MIPI lane: 3, MIPI data rate(Mbps/lane): 2483
 *    [  6 ] Full_CropC : Remosaic Crop 4624x2604 60fps : Single Still Preview (16:9)              ,  MIPI lane: 3, MIPI data rate(Mbps/lane): 2483
 *
 * - V2H2 For HighSpeed Recording/FastAE -
 *    [  7 ] V2H2_SSL : V2H2 SlowMotion 2304x1296 240fps : Single Still Preview/Capture (16:9)  ,  MIPI lane: 3, MIPI data rate(Mbps/lane): 2483
 *    [  8 ] V2H2_FAE : V2H2 FastAE       2304x1728 120fps : Single Still Preview/Capture (4;3)    ,  MIPI lane: 3, MIPI data rate(Mbps/lane): 2483
 *
 * - SuperNightMode (none USED) -
 *    [  9 ] SNM         : SuperNightMode  4624x3468 24fps   : Single Still Preview/Capture (4:3)    ,  MIPI lane: 3, MIPI data rate(Mbps/lane): 2483
 *
 */


enum sensor_imx686_mode_enum {
	SENSOR_IMX686_2X2BIN_4624X3468_30FPS,
	SENSOR_IMX686_2X2BIN_4624X2604_30FPS,
	SENSOR_IMX686_2X2BIN_4624X2604_60FPS,
	SENSOR_IMX686_REMOSAIC_START,
	SENSOR_IMX686_REMOSAIC_FULL_9248X6936_20FPS = SENSOR_IMX686_REMOSAIC_START,
	SENSOR_IMX686_REMOSAIC_CROP_4624X3468_30FPS,
	SENSOR_IMX686_REMOSAIC_CROP_4624X2604_30FPS,
	SENSOR_IMX686_REMOSAIC_CROP_4624X2604_60FPS,
	SENSOR_IMX686_REMOSAIC_END = SENSOR_IMX686_REMOSAIC_CROP_4624X2604_60FPS,
	SENSOR_IMX686_V2H2_2304X1269_240FPS,
	SENSOR_IMX686_V2H2_2304X1728_120FPS,

//	SENSOR_IMX686_SNM_4624X3468_24FPS,		//This setting is not used.
};

#endif
