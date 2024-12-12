/*
 * Samsung Exynos5 SoC series EEPROM driver
 *
 *
 * Copyright (c) 2018 Samsung Electronics Co., Ltd
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#ifndef IS_EEPROM_5E9_H
#define IS_EEPROM_5E9_H

#define EEPROM_DATA_PATH		"/data/vendor/camera/5e9_eeprom_data.bin"
#define EEPROM_SERIAL_NUM_DATA_PATH	"/data/vendor/camera/serial_number_5e.bin"

/* Total Cal data size */
#define EEPROM_DATA_SIZE		SZ_8K

/* Address */
#define EEPROM_ADD_CRC_FST		0x00
#define EEPROM_ADD_CRC_SEC		0x01
#define EEPROM_ADD_CRC_CHK_START	0x04
#define EEPROM_ADD_CRC_CHK_SIZE		0x48
#define EEPROM_ADD_CAL_SIZE		0x4C

/* Information Cal */
#define EEPROM_INFO_CRC_FST		0x50
#define EEPROM_INFO_CRC_SEC		0x51
#define EEPROM_INFO_CRC_CHK_START	0x54
#define EEPROM_INFO_CRC_CHK_SIZE	0x2E
#define EEPROM_INFO_SERIAL_NUM_START	0x79
#define EEPROM_INFO_SERIAL_NUM_SIZE	0x8
#define EEPROM_INFO_CAL_SIZE		0x32

/* AWB Cal */
#define EEPROM_AWB_CRC_FST		0x90
#define EEPROM_AWB_CRC_SEC		0x91
#define EEPROM_AWB_CRC_CHK_START	0x94
#define EEPROM_AWB_CRC_CHK_SIZE		0x32
#define EEPROM_AWB_CAL_SIZE		0x36
#define EEPROM_AWB_LIMIT_OFFSET		0xB4
#define EEPROM_AWB_GOLDEN_OFFSET	0xB8
#define EEPROM_AWB_UNIT_OFFSET		0xBE

/* LSC Cal */
#define EEPROM_LSC_CRC_FST		0x100
#define EEPROM_LSC_CRC_SEC		0x101
#define EEPROM_LSC_CRC_CHK_START	0x104
#define EEPROM_LSC_CRC_CHK_SIZE		0x1374
#define EEPROM_LSC_CAL_SIZE		0x1378

/* AE Sync Cal */
#define EEPROM_AE_CRC_FST		0x1480
#define EEPROM_AE_CRC_SEC		0x1481
#define EEPROM_AE_CRC_CHK_START		0x1484
#define EEPROM_AE_CRC_CHK_SIZE		0x10
#define EEPROM_AE_CAL_SIZE		0x14

/* SFR Cal */
#define EEPROM_SFR_CRC_FST		0x1520
#define EEPROM_SFR_CRC_SEC		0x1521
#define EEPROM_SFR_CRC_CHK_START	0x14A0
#define EEPROM_SFR_CRC_CHK_SIZE		0x80
#define EEPROM_SFR_CAL_SIZE		0x82

#endif
