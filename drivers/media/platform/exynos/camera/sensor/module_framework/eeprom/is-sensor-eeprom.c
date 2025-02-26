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

#include <linux/i2c.h>
#include <linux/slab.h>
#include <linux/module.h>
#include <linux/moduleparam.h>
#include <linux/platform_device.h>
#include <linux/of_gpio.h>
#include <linux/platform_device.h>

#include <exynos-is-sensor.h>
#include "is-sensor-eeprom.h"
#include "is-device-sensor.h"
#include "is-device-sensor-peri.h"
#include "is-helper-i2c.h"
#include "is-core.h"

void is_eeprom_cal_data_set(char *data, char *name,
		u32 addr, u32 size, u32 value)
{
	int i;

	/* value setting to (name) cal data section */
	for (i = addr; i < size; i++)
		data[i] = value;

	info("%s() Done: %s calibration data is %d set\n", __func__, name, value);
}

int is_eeprom_file_write(const char *file_name, const void *data,
		unsigned long size)
{
	int ret = 0;
#ifdef USE_KERNEL_VFS_READ_WRITE
	struct file *fp;

	fp = filp_open(file_name, O_WRONLY | O_CREAT | O_TRUNC | O_SYNC, 0666);
	if (IS_ERR_OR_NULL(fp)) {
		ret = PTR_ERR(fp);
		err("%s(): open file error(%s), error(%d)\n", __func__, file_name, ret);
		goto p_err;
	}

	ret = kernel_write(fp, data, size, &fp->f_pos);
	if (ret < 0) {
		err("%s(): file write fail(%s) to EEPROM data(%d)", __func__,
				file_name, ret);
		goto p_err;
	}

	info("%s(): wirte to file(%s)\n", __func__, file_name);
p_err:
	if (!IS_ERR_OR_NULL(fp))
		filp_close(fp, NULL);

#else
	err("not support %s due to kernel_write!", __func__);
	ret = -EINVAL;
#endif
	return 0;
}

int is_eeprom_file_read(const char *file_name, const void *data,
		unsigned long size)
{
	int ret = 0;
#ifdef USE_KERNEL_VFS_READ_WRITE
	long fsize, nread;
	struct file *fp;

	fp = filp_open(file_name, O_RDONLY, 0);
	if (IS_ERR_OR_NULL(fp)) {
		ret = PTR_ERR(fp);
		err("filp_open(%s) fail(%d)!!\n", file_name, ret);
		goto p_err;
	}

	fsize = fp->f_path.dentry->d_inode->i_size;

	nread = kernel_read(fp, data, size, &fp->f_pos);
	if (nread != size) {
		err("failed to read eeprom file, (%ld) Bytes", nread);
		ret = nread;
		goto p_err;
	}

	info("%s(): read to file(%s)\n", __func__, file_name);
p_err:
	if (!IS_ERR_OR_NULL(fp))
		filp_close(fp, NULL);

#else
	err("not support %s due to kernel_read!", __func__);
	ret = -EINVAL;
#endif
	return ret;
}

int is_sensor_eeprom_check_crc16(char *data, size_t size)
{
	char *tmp = data;
	u32 crc[16];
	int i, j;
	u16 crc16 = 0;

	memset(crc, 0, sizeof(crc));
	for (i = 0; i < size; i++) {
		for (j = 7; j >= 0; j--) {
			/* isolate the bit in the byte */
			u32 doInvert = *tmp & (1 << j);

			// shift the bit to LSB in the byte
			doInvert = doInvert >> j;

			// XOR required?
			doInvert = doInvert ^ crc[15];

			crc[15] = crc[14] ^ doInvert;
			crc[14] = crc[13];
			crc[13] = crc[12];
			crc[12] = crc[11];
			crc[11] = crc[10];
			crc[10] = crc[9];
			crc[9] = crc[8];
			crc[8] = crc[7];
			crc[7] = crc[6];
			crc[6] = crc[5];
			crc[5] = crc[4];
			crc[4] = crc[3];
			crc[3] = crc[2];
			crc[2] = crc[1] ^ doInvert;
			crc[1] = crc[0];
			crc[0] = doInvert;
		}
		tmp++;
	}

	/* convert bits to CRC word */
	for (i = 0; i < 16; i++)
		crc16 = crc16 + (crc[i] << i);

	return crc16;
}

int is_sensor_eeprom_check_crc_sumup(char *data, size_t size)
{
	u32 sum = 0;
	int i;
	u16 crc_sumup = 0;

	for (i = 0; i < size; i++)
		sum += data[i];

	/* The checksum =  Sum(start address to end address)%0xff + 1 */
	crc_sumup = (sum % 0xff) + 1;

	return crc_sumup;
}

int is_sensor_eeprom_get_chksum(char *chksum, enum rom_chksum_method method)
{
	u16 chksum_val = 0;

	switch (method) {
	case EEPROM_CRC16_LE:
		chksum_val = chksum[0] | (chksum[1] << 8);
		break;
	case EEPROM_CRC16_BE:
		chksum_val = (chksum[0] << 8) | chksum[1];
		break;
	case EEPROM_SUMUP: /* use 1byte */
		chksum_val = chksum[0];
		break;
	default:
		err("cannot find rom chksum method(%d)", method);
		break;
	}

	return chksum_val;
}

int is_sensor_eeprom_check_crc(const char *name, char *chksum,
		char *data, size_t size, enum rom_chksum_method method)
{
	int ret = 0;
	u16 chksum_val = 0;
	u16 result = 0;

	FIMC_BUG(!name);
	FIMC_BUG(!chksum);
	FIMC_BUG(!data);

	chksum_val = is_sensor_eeprom_get_chksum(chksum, method);

	switch (method) {
	case EEPROM_CRC16_LE:
	case EEPROM_CRC16_BE:
		result = is_sensor_eeprom_check_crc16(data, size);
		break;
	case EEPROM_SUMUP:
		result = is_sensor_eeprom_check_crc_sumup(data, size);
		break;
	default:
		err("cannot find eeprom crc method(%d)", method);
		return -EINVAL;
	}

	if (chksum_val == result) {
		info("Matched %s Checksum: 0x%x, Caculated(%s): 0x%x",
				name, chksum_val,
				method == EEPROM_SUMUP ? "SUMUP" : "CRC16", result);
	} else {
		err("Mismatched %s Checksum: 0x%x != Caculated(%s): 0x%x",
				name, chksum_val,
				method == EEPROM_SUMUP ? "SUMUP" : "CRC16", result);
		ret = -EINVAL;
	}

	return ret;
}

int is_sensor_eeprom_check_awb_ratio(char *unit, char *golden, char *limit)
{
	int ret = 0;

	float r_g_min = (float)(limit[0]) / 1000;
	float r_g_max = (float)(limit[1]) / 1000;
	float b_g_min = (float)(limit[2]) / 1000;
	float b_g_max = (float)(limit[3]) / 1000;

	float rg = (float) ((unit[1] << 8) | (unit[0])) / 16384;
	float bg = (float) ((unit[3] << 8) | (unit[2])) / 16384;

	float golden_rg = (float) ((golden[1] << 8) | (golden[0])) / 16384;
	float golden_bg = (float) ((golden[3] << 8) | (golden[2])) / 16384;

	if (rg < (golden_rg - r_g_min) || rg > (golden_rg + r_g_max)) {
		err("%s(): Final RG calibration factors out of range! rg=0x%x golden_rg=0x%x",
			__func__, (unit[1] << 8 | unit[0]), (golden[1] << 8 | golden[0]));
		ret = 1;
	}

	if (bg < (golden_bg - b_g_min) || bg > (golden_bg + b_g_max)) {
		err("%s(): Final BG calibration factors out of range! bg=0x%x, golden_bg=0x%x",
			__func__, (unit[3] << 8 | unit[2]), (golden[3] << 8 | golden[2]));
		ret = 1;
	}

	return ret;
}

int is_eeprom_module_read(struct i2c_client *client, u32 addr,
		char *data, unsigned long size)
{
	int ret = 0;

	/* Read EEPROM cal data in module */
	ret = is_sensor_read8_size(client, &data[0], addr, size);
	if (ret < 0) {
		err("%s(), i2c read failed(%d)\n", __func__, ret);
		return ret;
	}

	info("EEPROM module read done!!\n");

	return ret;
}

int is_eeprom_get_sensor_id(struct v4l2_subdev *subdev, int *sensor_id)
{
	int ret = 0;
	struct is_eeprom *eeprom = NULL;
	int i;
	char val;
	struct is_device_sensor *device = NULL;

	FIMC_BUG(!subdev);

	eeprom = (struct is_eeprom *)v4l2_get_subdevdata(subdev);
	FIMC_BUG(!eeprom);

	device = (struct is_device_sensor *)v4l2_get_subdev_hostdata(subdev);
	FIMC_BUG(!device);

	/* get identifier in EEPROM by using index from DT */
	ret = is_sensor_read8(eeprom->client, device->pdata->module_sel_idx, &val);
	if (ret) {
		err("is_sensor_read8 fail(%d)\n", ret);
		*sensor_id = -1;
		goto p_err;
	}

	/* find sensor id by matching identifier */
	for (i = 0; i < device->pdata->module_sel_cnt; i++) {
		if (device->pdata->module_sel_val[i] == val) {
			*sensor_id = device->pdata->module_sel_sensor_id[i];
			break;
		}
	}

	if (i >= device->pdata->module_sel_cnt) {
		err("%s(), can't find matched identifier", __func__);
		*sensor_id = -1;
		goto p_err;
	}

	dbg_sensor(1, "%s(), selected sensor_id: %d\n", __func__, *sensor_id);
p_err:
	return ret;
}

int is_eeprom_caldata_variation(struct v4l2_subdev *subdev,
		int type, u16 offset, u16 val)
{
	int ret = 0;
	struct is_eeprom *eeprom = NULL;
	u16 temp;

	eeprom = (struct is_eeprom *)v4l2_get_subdevdata(subdev);
	FIMC_BUG(!eeprom);

	switch (type) {
	case VARIATION_NONE:
		break;
	case VARIATION_LSC_FLIP:
	case VARIATION_LSC_MIRROR: /* condition: mirror = flip + 1 */
	case VARIATION_LSC_FLIP_MIRROR:
		temp = (eeprom->data[offset] << 8) | (eeprom->data[offset + 1]);

		if (type == VARIATION_LSC_FLIP) {
			eeprom->data[offset] = 0x1;
		} else if (type == VARIATION_LSC_MIRROR) {
			eeprom->data[offset + 1] = 0x1;
		} else {
			eeprom->data[offset] = 0x1;
			eeprom->data[offset + 1] = 0x1;
		}
		info("EEPROM[%d] cal LSC/Flip data changed(%4x -> %2x%2x)\n",
			eeprom->id, temp, eeprom->data[offset], eeprom->data[offset + 1]);
		break;
	default:
		break;
	}

	return ret;
}
