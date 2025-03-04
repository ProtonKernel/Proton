/*
* Samsung Exynos5 SoC series FIMC-IS driver
 *
 * exynos5 fimc-is vender functions
 *
 * Copyright (c) 2011 Samsung Electronics Co., Ltd
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#ifndef IS_VENDER_SPECIFIC_H
#define IS_VENDER_SPECIFIC_H

#include <linux/types.h>
#include <linux/spinlock.h>
#include <linux/sched.h>
#include <linux/sched/rt.h>

#ifdef CONFIG_COMPANION_DCDC_USE
#include "is-pmic.h"
#endif

#if defined(CONFIG_CAMERA_FROM)
typedef enum FRomPowersource{
    FROM_POWER_SOURCE_REAR	= 0,  /*wide*/
    FROM_POWER_SOURCE_REAR_SECOND  /*tele*/
} FRomPowersource;
#endif

/* #define for standard calibration */
#ifdef CONFIG_SEC_CAL_ENABLE
#define IS_CAMINFO_IOCTL_MAGIC 			0xFB
#define CAM_MAX_SUPPORTED_LIST			20


#define IS_CAMINFO_IOCTL_COMMAND		_IOWR(IS_CAMINFO_IOCTL_MAGIC, 0x01, caminfo_ioctl_cmd *)

#define SEC2LSI_AWB_DATA_SIZE			8
#define SEC2LSI_LSC_DATA_SIZE			6632
#define SEC2LSI_MODULE_INFO_SIZE		11
#define SEC2LSI_CHECKSUM_SIZE			4
#endif

/* #define USE_ION_ALLOC */
#define IS_COMPANION_CRC_SIZE			4
#define I2C_RETRY_COUNT					5
#define IS_TILT_CAL_TELE_EFS_MAX_SIZE	800
#define IS_GYRO_EFS_MAX_SIZE			30

struct is_companion_retention {
	int firmware_size;
	char firmware_crc32[IS_COMPANION_CRC_SIZE];
};

#ifdef CONFIG_SEC_CAL_ENABLE
enum is_crc32_check_list {
	CRC32_CHECK_HEADER			= 0,
	CRC32_CHECK					= 1,
	CRC32_CHECK_FACTORY			= 2,
	CRC32_CHECK_SETFILE			= 3,
	CRC32_CHECK_FW				= 4,
	CRC32_CHECK_FW_VER			= 5,
	CRC32_CHECK_STANDARD_CAL	= 6,
	CRC32_SCENARIO_MAX,
};
#endif

struct is_vender_specific {
	struct mutex		rom_lock;
#ifdef CONFIG_OIS_USE
	bool			ois_ver_read;
#endif /* CONFIG_OIS_USE */

	struct i2c_client	*eeprom_client[ROM_ID_MAX];
#if defined(USE_CAMERA_DUALIZED)
	struct i2c_client	*otprom_client[ROM_ID_MAX];
#endif
	bool		rom_valid[ROM_ID_MAX];

	/* dt */
	u32			rear_sensor_id;
	u32			front_sensor_id;
	u32			rear2_sensor_id;
	u32			front2_sensor_id;
	u32			rear3_sensor_id;
	u32			rear4_sensor_id;
	u32			rear_tof_sensor_id;
	u32			front_tof_sensor_id;
#if IS_ENABLED(CONFIG_SECURE_CAMERA_USE)
	u32			secure_sensor_id;
#endif
	u32			ois_sensor_index;
	u32			mcu_sensor_index;
	u32			aperture_sensor_index;
	bool			check_sensor_vendor;
	bool			use_ois_hsi2c;
	bool			use_ois;
	bool			use_module_check;

	bool			suspend_resume_disable;
	bool			need_cold_reset;
	bool			zoom_running;
	int32_t			rear_tof_mode_id;
	int32_t			front_tof_mode_id;
#ifdef USE_TOF_AF
	struct tof_data_t	tof_af_data;
	struct mutex		tof_af_lock;
#endif
	struct tof_info_t	tof_info;

#if defined(CONFIG_CAMERA_FROM)
	FRomPowersource		f_rom_power;
#endif
	int tilt_cal_tele_efs_size;
	uint8_t	tilt_cal_tele_efs_data[IS_TILT_CAL_TELE_EFS_MAX_SIZE];
	int tilt_cal_tele2_efs_size;
	uint8_t	tilt_cal_tele2_efs_data[IS_TILT_CAL_TELE_EFS_MAX_SIZE];
	int gyro_efs_size;
	uint8_t 	gyro_efs_data[IS_GYRO_EFS_MAX_SIZE];
};

#endif
