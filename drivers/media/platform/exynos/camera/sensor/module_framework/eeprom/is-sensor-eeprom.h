/*
 * Samsung Exynos5 SoC series Sensor driver
 * Copyright (c) 2018 Samsung Electronics Co., Ltd
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#ifndef IS_EEPROM_H
#define IS_EEPROM_H

#include <linux/platform_device.h>
#include <media/v4l2-ctrls.h>
#include <media/v4l2-device.h>
#include <media/v4l2-subdev.h>

#include "is-device-sensor-peri.h"

enum rom_chksum_method {
	EEPROM_CRC16_LE = 0,
	EEPROM_CRC16_BE,
	EEPROM_SUMUP,
};

enum rom_variation_type {
	VARIATION_NONE = 0,
	VARIATION_LSC_FLIP,
	VARIATION_LSC_MIRROR,
	VARIATION_LSC_FLIP_MIRROR,
};

void is_eeprom_cal_data_set(char *data, char *name,
		u32 addr, u32 size, u32 value);
int is_eeprom_file_write(const char *file_name, const void *data,
		unsigned long size);
int is_eeprom_file_read(const char *file_name, const void *data,
		unsigned long size);
int is_eeprom_module_read(struct i2c_client *client, u32 addr,
		char *data, unsigned long size);
int is_sensor_eeprom_check_crc16(char *data, size_t size);
int is_sensor_eeprom_check_crc_sumup(char *data, size_t size);
int is_sensor_eeprom_check_crc(const char *name, char *chksum,
		char *data, size_t size, enum rom_chksum_method method);
int is_sensor_eeprom_check_awb_ratio(char *unit, char *golden, char *limit);

int is_eeprom_get_sensor_id(struct v4l2_subdev *subdev, int *sensor_id);
int is_eeprom_caldata_variation(struct v4l2_subdev *subdev,
		int type, u16 offset, u16 val);

#endif

