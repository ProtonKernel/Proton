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

#ifndef IS_VENDER_CAMINFO_H
#define IS_VENDER_CAMINFO_H

#include "is-core.h"

#ifndef _LINUX_TYPES_H
typedef unsigned char uint8_t;
typedef unsigned short uint16_t;
typedef signed short int16_t;
typedef signed int int32_t;
typedef unsigned int uint32_t;
typedef unsigned long long uint64_t;
#endif

#ifdef CONFIG_SEC_CAL_ENABLE
static bool sec2lsi_conversion_done[8] = {false, false, false, false, false, false, false, false};
#endif

#define IS_CAMINFO_IOCTL_MAGIC 0xFB

#define IS_CAMINFO_IOCTL_COMMAND	_IOWR(IS_CAMINFO_IOCTL_MAGIC, 0x01, caminfo_ioctl_cmd *)

typedef struct
{
	uint32_t cmd;
	void *data;
} caminfo_ioctl_cmd;

enum caminfo_cmd_id
{
	CAMINFO_CMD_ID_GET_FACTORY_SUPPORTED_ID = 0,
	CAMINFO_CMD_ID_GET_ROM_DATA_BY_POSITION = 1,
	CAMINFO_CMD_ID_SET_EFS_DATA = 2,
	CAMINFO_CMD_ID_GET_SENSOR_ID = 3,
	CAMINFO_CMD_ID_GET_AWB_DATA_ADDR = 4,

#ifdef CONFIG_SEC_CAL_ENABLE
	/* Standard CAL */
	CAMINFO_CMD_ID_GET_MODULE_INFO = 20,
	CAMINFO_CMD_ID_GET_SEC2LSI_BUFF = 21,
	CAMINFO_CMD_ID_SET_SEC2LSI_BUFF = 22,
#endif
};

#define CAM_MAX_SUPPORTED_LIST	20

typedef struct
{
	uint32_t cam_position;
	uint32_t buf_size;
	uint8_t *buf;
	uint32_t rom_size;
} caminfo_romdata;

typedef struct
{
	uint32_t size;
	uint32_t data[CAM_MAX_SUPPORTED_LIST];
} caminfo_supported_list;

typedef struct
{
	int tilt_cal_tele_efs_size;
	int tilt_cal_tele2_efs_size;
	int gyro_efs_size;
	uint8_t *tilt_cal_tele_efs_buf;
	uint8_t *tilt_cal_tele2_efs_buf;
	uint8_t *gyro_efs_buf;
} caminfo_efs_data;

typedef struct
{
	uint32_t cameraId;
	uint32_t sensorId;
} caminfo_sensor_id;

typedef struct
{
	int cameraId;
	int32_t awb_master_addr;
	int32_t awb_module_addr;
	int32_t awb_master_data_size;
	int32_t awb_module_data_size;
} caminfo_awb_data_addr;

#define CAM_MAX_SENSOR_POSITION	8

typedef struct
{
	uint32_t size;
	uint8_t *data;
} caminfo_bin_data;

typedef struct
{
	caminfo_bin_data ddk;
	caminfo_bin_data rta;
	caminfo_bin_data setfile[CAM_MAX_SENSOR_POSITION];
} caminfo_bin_data_isp;

typedef struct
{
	struct mutex	mlock;
} is_vender_caminfo;

#ifdef CONFIG_SEC_CAL_ENABLE
typedef struct
{
	uint32_t camID;
	unsigned char *secBuf;
	unsigned char *lsiBuf;
	unsigned char *mdInfo; //Module information data in the calmap Header area
	uint32_t awb_size;
	uint32_t lsc_size;
} caminfo_romdata_sec2lsi;
#endif

#ifdef CONFIG_SEC_CAL_ENABLE
bool is_need_use_standard_cal(uint32_t rom_id);
#endif

#endif /* IS_VENDER_CAMINFO_H */
