/*
 * Copyright (C) 2018 Motorola Mobility LLC.
 *
 * This software is licensed under the terms of the GNU General Public
 * License version 2, as published by the Free Software Foundation, and
 * may be copied, distributed, and modified under those terms.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 */

#include "is-core.h"
#include "is-device-sensor-peri.h"
#include "is-helper-ois-i2c.h"
#include "is-ois-bu24218gwl.h"
#include "is-ois.h"

#define OIS_NAME "OIS_ROHM_BU24218GWL"
#define OIS_FW_1_NAME		"bu24218_Rev1.5_S_data1.bin"
#define OIS_FW_2_NAME		"bu24218_Rev1.5_S_data2.bin"
#define OIS_FW_NUM		2
#define OIS_FW_ADDR_1		0x0000
#define OIS_FW_ADDR_2		0x1C00
#define OIS_FW_CHECK_SUM	0x038B41

#define OIS_CAL_DATA_PATH_DEFAULT	"/vendor/firmware/bu24218_cal_data_default.bin"
#define OIS_CAL_DATA_CRC_FST		0x1DB0
#define OIS_CAL_DATA_CRC_SEC		0x1DB1
#define OIS_CAL_DATA_OFFSET		0x1DB4
#define OIS_CAL_DATA_SIZE		0x4C
#define OIS_CAL_DATA_OFFSET_DEFAULT	0
#define OIS_CAL_DATA_SIZE_DEFAULT	0x28
#define OIS_CAL_ADDR			0x1DC0
#define OIS_CAL_ACTUAL_DL_SIZE		0x28
#define EEPROM_INFO_TABLE_REVISION	0x64

/* #define OIS_DEBUG */

static u8 ois_fw_data[OIS_FW_NUM][OIS_FW_SIZE] = {{0},};
static int ois_fw_data_size[OIS_FW_NUM] = {0,};
static u8 ois_cal_data[OIS_CAL_DATA_SIZE] = {0,};
int ois_cal_data_size = 0;
int ois_previous_mode = 0;
int ois_isDefaultCalData = 0;
int load_cnt = 0;

static const struct v4l2_subdev_ops subdev_ops;

#ifdef CONFIG_OIS_DIRECT_FW_CONTROL
int is_ois_fw_ver_copy(struct is_ois *ois, u8 *buf, long size)
{
	int ret = 0;

	FIMC_BUG(!ois);

	memcpy(ois_fw_data[load_cnt], (void *)buf, size);
	ois_fw_data_size[load_cnt] = size;

	load_cnt++;
	if (load_cnt == OIS_FW_NUM)
		load_cnt = 0;

	info("%s copy size:%d bytes", __func__, size);

	return ret;
}

int is_ois_check_crc(char *data, size_t size)
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

int is_ois_cal_open(struct is_ois *ois, char *name, int offset, int size)
{
	int ret = 0;
#ifdef USE_KERNEL_VFS_READ_WRITE
	long fsize, nread;
	struct file *fp;
	u8 *buf = NULL;

	FIMC_BUG(!ois);

	info("%s: E", __func__);

	fp = filp_open(name, O_RDONLY, 0);
	if (IS_ERR_OR_NULL(fp)) {
		ret = PTR_ERR(fp);
		err("filp_open(%s) fail(%d)!!\n", name, ret);
		goto p_err;
	}

	buf = vmalloc(size);
	if (!buf) {
		err("failed to allocate memory");
		ret = -ENOMEM;
		goto p_err;
	}

	fsize = fp->f_path.dentry->d_inode->i_size;
	if (fsize < offset + size) {
		err("ois cal data not exist");
		ret = -EIO;
		goto p_err;
	}

	fp->f_pos = offset;
	info("ois f_pos set offset %x", fp->f_pos);
	nread = kernel_read(fp, buf, size, &fp->f_pos);
	if (nread != size) {
		err("failed to read ois cal data from file, (%ld) Bytes", nread);
		ret = -EIO;
		goto p_err;
	}

	/* Cal data save */
	memcpy(ois_cal_data, (void *)buf, OIS_CAL_ACTUAL_DL_SIZE);
	ois_cal_data_size = OIS_CAL_ACTUAL_DL_SIZE;
	info("%s cal data copy size:%d bytes", __func__, OIS_CAL_ACTUAL_DL_SIZE);

p_err:
	if (buf)
		vfree(buf);

	if (!IS_ERR_OR_NULL(fp))
		filp_close(fp, NULL);

	info("%s: X", __func__);
#else
	err("not support %s due to kernel_read!", __func__);
	ret = -EINVAL;
#endif
	return ret;
}

int is_ois_fw_download(struct is_ois *ois)
{
	int ret = 0;
	int retry = 3;
	u8 check_sum[4] = {0};
	int sum = 0;
	u8 send_data[256];
	int position = 0;
	int i, quotienti, remainder;
	u8 ois_status = 0;
	int fw_num = 0;
	int fw_download_start_addr[OIS_FW_NUM] = {OIS_FW_ADDR_1, OIS_FW_ADDR_2};

	FIMC_BUG(!ois);

	info("%s: E", __func__);

	/* Step 1 Enable download*/
	I2C_MUTEX_LOCK(ois->i2c_lock);
	ret = is_ois_write(ois->client, 0xF010, 0x00);
	if (ret < 0) {
		err("ois start download write is fail");
		ret = 0;
		I2C_MUTEX_UNLOCK(ois->i2c_lock);
		goto p_err;
	}
	I2C_MUTEX_UNLOCK(ois->i2c_lock);

	/* wait over 200us */
	usleep_range(200, 201);
	ret = is_ois_read(ois->client, 0x6024, &ois_status);
	info("ois status(0x6024):0x%x", ois_status);

	/* Step 2 Download FW*/
	for (fw_num = 0; fw_num < OIS_FW_NUM; fw_num++) {
		quotienti = 0;
		remainder = 0;
		position = 0;
		quotienti = ois_fw_data_size[fw_num]/FW_TRANS_SIZE;
		remainder = ois_fw_data_size[fw_num]%FW_TRANS_SIZE;

		for (i = 0; i < quotienti ; i++) {
			memcpy(send_data, &ois_fw_data[fw_num][position], (size_t)FW_TRANS_SIZE);
			I2C_MUTEX_LOCK(ois->i2c_lock);
			ret = is_ois_write_multi(ois->client, fw_download_start_addr[fw_num]+position,
				send_data, FW_TRANS_SIZE + 2);
			if (ret < 0) {
				err("ois fw download is fail");
				ret = 0;
				I2C_MUTEX_UNLOCK(ois->i2c_lock);
				goto p_err;
			}
			I2C_MUTEX_UNLOCK(ois->i2c_lock);

			position += FW_TRANS_SIZE;
		}
		if (remainder) {
			I2C_MUTEX_LOCK(ois->i2c_lock);
			memcpy(send_data, &ois_fw_data[fw_num][position], (size_t)remainder);
			ret = is_ois_write_multi(ois->client, fw_download_start_addr[fw_num]+position,
				send_data, remainder + 2);
			I2C_MUTEX_UNLOCK(ois->i2c_lock);
		}
		if (ret < 0) {
			err("ois fw download is fail");
			ret = 0;
			goto p_err;
		}
		info("ois fw %d download size:%d", fw_num, position + remainder);
	}

	/* Step 3 Sum Check*/
	I2C_MUTEX_LOCK(ois->i2c_lock);
	ret = is_ois_read_multi(ois->client, 0xF008, check_sum, 4);
	if (ret < 0) {
		err("ois read check sum fail");
		ret = 0;
		I2C_MUTEX_UNLOCK(ois->i2c_lock);
		goto p_err;
	}
	I2C_MUTEX_UNLOCK(ois->i2c_lock);

	sum = ((check_sum[0] << 24) | (check_sum[1] << 16) | (check_sum[2] << 8) | (check_sum[3]));
	info("ois check sum value:0x%0x, expected value:0x%0x", sum, OIS_FW_CHECK_SUM);
	if (sum != OIS_FW_CHECK_SUM) {
		err("ois check sum fail, force return");
		ret = 0;
		goto p_err;
	}

	/* Step 4 Calibration data download */
	info("ois download cal data");
	I2C_MUTEX_LOCK(ois->i2c_lock);
	ret = is_ois_write_multi(ois->client, OIS_CAL_ADDR, ois_cal_data, ois_cal_data_size + 2);
	if (ret < 0) {
		err("ois cal data download is fail");
		ret = 0;
		I2C_MUTEX_UNLOCK(ois->i2c_lock);
		goto p_err;
	}
	info("ois cal data download size :%d", ois_cal_data_size);
	I2C_MUTEX_UNLOCK(ois->i2c_lock);

	/* Step 5 OIS download complete */
	I2C_MUTEX_LOCK(ois->i2c_lock);
	ret = is_ois_write(ois->client, 0xF006, 0x00);
	if (ret < 0) {
		err("ois write download complete is fail");
		ret = 0;
		I2C_MUTEX_UNLOCK(ois->i2c_lock);
		goto p_err;
	}
	I2C_MUTEX_UNLOCK(ois->i2c_lock);

	/* wait 18ms */
	usleep_range(18000, 18001);

	/* OIS status */
	ret = is_ois_read(ois->client, 0x6024, &ois_status);
	info("ois status(0x6024) after D/L complete :0x%x", ois_status);

	while ((ois_status == 0) && (retry-- > 0)) {
		usleep_range(4000, 4001);
		I2C_MUTEX_LOCK(ois->i2c_lock);
		ret = is_ois_read(ois->client, 0x6024, &ois_status);
		I2C_MUTEX_UNLOCK(ois->i2c_lock);
		info("ois status(0x6024) :0x%x", ois_status);
	}
	if (ois_status != 1) {
		err("ois_status is 0,force return error");
		ret = 0;
		goto p_err;
	}

	info("%s: ois fw download success\n", __func__);
p_err:

	return ret;
}

int is_ois_fw_update(struct v4l2_subdev *subdev)
{
	int ret = 0;
	int i = 0;
	u16 crc_value = 0;
	u16 crc16_dvt = 0;
	u16 crc16_pvt = 0;
	struct is_ois *ois = NULL;
	static int is_first_load = 1;
	struct is_eeprom *eeprom = NULL;
	struct is_device_sensor *sensor = NULL;
	bool use_default_cal = false;

	FIMC_BUG(!subdev);

	info("%s: E", __func__);

	ois = (struct is_ois *)v4l2_get_subdevdata(subdev);
	FIMC_BUG(!ois);

	sensor = (struct is_device_sensor *)v4l2_get_subdev_hostdata(subdev);

	FIMC_BUG(!sensor);

	eeprom = sensor->eeprom;

	if (!eeprom) {
		use_default_cal = true;
		goto skip_cal_check;
	}

	/* OIS Firmware load*/
	if (is_first_load == 1) {
		ret = is_ois_fw_open(ois, OIS_FW_1_NAME);
		if (ret < 0) {
			err("OIS %s load is fail\n", OIS_FW_1_NAME);
			return 0;
		}
		ret = is_ois_fw_open(ois, OIS_FW_2_NAME);
		if (ret < 0) {
			err("OIS %s load is fail\n", OIS_FW_2_NAME);
			return 0;
		}

		crc_value = ((eeprom->data[OIS_CAL_DATA_CRC_FST] << 8) | (eeprom->data[OIS_CAL_DATA_CRC_SEC]));

		crc16_dvt = is_ois_check_crc(&eeprom->data[OIS_CAL_DATA_OFFSET], OIS_CAL_DATA_SIZE);
		crc16_pvt = is_ois_check_crc(&eeprom->data[OIS_CAL_DATA_OFFSET], OIS_CAL_DATA_SIZE + 4);
		if ((crc_value != crc16_dvt) && (crc_value != crc16_pvt)) {
			err("Error to OIS CRC16 dvt 0x%x, pvt:0x%x, cal_buffer CRC: 0x%x",
					crc16_dvt, crc16_pvt, crc_value);
			ret = -EINVAL;
			use_default_cal = true;
		} else {
			info("OIS CRC16 dvt 0x%x, pvt:0x%x, cal_buffer CRC: 0x%x",
					crc16_dvt, crc16_pvt, crc_value);
			/* The eeprom table revision in DVT2 is 0x33,
			 * use it to check the OIS cal data should apply or not
			 */
			if (eeprom->data[EEPROM_INFO_TABLE_REVISION] < 0x33) {
				err("ois cal data in eeprom is wrong");
				ret = -EINVAL;
				use_default_cal = true;
			} else {
				/* Cal data save */
				memcpy(ois_cal_data, &eeprom->data[OIS_CAL_DATA_OFFSET], OIS_CAL_ACTUAL_DL_SIZE);
				ois_cal_data_size = OIS_CAL_ACTUAL_DL_SIZE;
				info("%s cal data copy size:%d bytes", __func__, OIS_CAL_ACTUAL_DL_SIZE);
				ret = 0;
			}
			info("eeprom table revision data 0x%0x", eeprom->data[EEPROM_INFO_TABLE_REVISION]);
		}

skip_cal_check:
		if (use_default_cal) {
			info("switch to load default OIS Cal Data %s \n", OIS_CAL_DATA_PATH_DEFAULT);
			ret = is_ois_cal_open(ois, OIS_CAL_DATA_PATH_DEFAULT, OIS_CAL_DATA_OFFSET_DEFAULT,
				OIS_CAL_DATA_SIZE_DEFAULT);
			if (ret < 0) {
				err("OIS %s load is fail\n", OIS_CAL_DATA_PATH_DEFAULT);
				return 0;
			}
			ois_isDefaultCalData = 1;
		}
		for (i = 0; i < ois_cal_data_size / 4; i++) {
			info("ois cal data (%d): 0x%0x,%0x,%0x,%0x", i, ois_cal_data[4*i+0], ois_cal_data[4*i+1],
				ois_cal_data[4*i+2], ois_cal_data[4*i+3]);
		}
		is_first_load = 0;
	}

	if (ois_isDefaultCalData == 1) {
		info("%s OIS using default cal data", __func__);
	} else {
		info("%s OIS using real cal data from eeprom", __func__);
	}

	/* OIS Firmware download */
	ret = is_ois_fw_download(ois);
	if (ret < 0) {
		err("OIS Firmware download fail");
		return 0;
	}

	info("%s: X", __func__);
	return ret;
}

#endif

int is_set_ois_mode(struct v4l2_subdev *subdev, int mode)
{
	int ret = 0;
	struct is_ois *ois;
	struct i2c_client *client = NULL;
#ifdef OIS_DEBUG
	u8 ois_mode = 0;
	u8 ois_gyro_mode = 0;
	u8 ois_gyro_output1[2] = {0};
	u8 ois_gyro_output2[2] = {0};
	u8 ois_gyro_output3[2] = {0};
	u8 ois_gyro_output4[2] = {0};
	u8 ois_hall_x = 0;
	u8 ois_hall_y = 0;
#endif

	FIMC_BUG(!subdev);

	ois = (struct is_ois *)v4l2_get_subdevdata(subdev);
	if (!ois) {
		err("ois is NULL");
		ret = -EINVAL;
		return ret;
	}

	client = ois->client;
	if (!client) {
		err("client is NULL");
		ret = -EINVAL;
		return ret;
	}

	I2C_MUTEX_LOCK(ois->i2c_lock);

#ifdef OIS_DEBUG
	ret != is_ois_read(ois->client, 0x6021, &ois_mode);
	info("last ois mode(0x6021) is 0x%x", ois_mode);
	ret != is_ois_read(ois->client, 0x6023, &ois_gyro_mode);
	info("ois Gyro mode(0x6023) is 0x%x", ois_gyro_mode);
#endif

	if (ois_previous_mode == mode) {
#ifdef OIS_DEBUG
	ret != is_ois_read_multi(ois->client, 0x6040, ois_gyro_output1, 2);
	info("ois Gyro output1(0x6040) is 0x%x%x", ois_gyro_output1[0], ois_gyro_output1[1]);
	ret != is_ois_read_multi(ois->client, 0x6042, ois_gyro_output2, 2);
	info("ois Gyro output2(0x6042) is 0x%x%x", ois_gyro_output2[0], ois_gyro_output2[1]);
	ret != is_ois_read_multi(ois->client, 0x6044, ois_gyro_output3, 2);
	info("ois Gyro output3(0x6044) is 0x%x%x", ois_gyro_output3[0], ois_gyro_output3[1]);
	ret != is_ois_read_multi(ois->client, 0x6046, ois_gyro_output4, 2);
	info("ois Gyro output4(0x6046) is 0x%x%x", ois_gyro_output4[0], ois_gyro_output4[1]);
	ret != is_ois_read(ois->client, 0x6058, &ois_hall_x);
	info("ois Hall X(0x6058) is 0x%x", ois_hall_x);
	ret != is_ois_read(ois->client, 0x6059, &ois_hall_y);
	info("ois Hall Y(0x6059) is 0x%x", ois_hall_y);
	info("set ois mode:%d %s", mode,
		mode == OPTICAL_STABILIZATION_MODE_STILL ? "OPTICAL_STABILIZATION_MODE_STILL" :
		mode == OPTICAL_STABILIZATION_MODE_STILL_ZOOM ? "OPTICAL_STABILIZATION_MODE_STILL_ZOOM" :
		mode == OPTICAL_STABILIZATION_MODE_VIDEO ? "OPTICAL_STABILIZATION_MODE_VIDEO" :
		mode == OPTICAL_STABILIZATION_MODE_CENTERING ? "OPTICAL_STABILIZATION_MODE_CENTERING" :
		mode == 0 ? "OFF" : "other");
#endif
		goto p_err;
	} else {
		info("set ois mode:%d", mode);
		switch (mode) {
		case OPTICAL_STABILIZATION_MODE_STILL:
			is_ois_write(ois->client, 0x6020, 0x01);
			usleep_range(100000, 100001);
			is_ois_write(client, 0x6021, 0x7b); // ZSL
			is_ois_write(client, 0x6020, 0x02);
			break;
		case OPTICAL_STABILIZATION_MODE_STILL_ZOOM:
			is_ois_write(ois->client, 0x6020, 0x01);
			usleep_range(100000, 100001);
			is_ois_write(client, 0x6021, 0x03); // Exposure/Shake
			is_ois_write(client, 0x6020, 0x02);
			break;
		case OPTICAL_STABILIZATION_MODE_VIDEO:
			is_ois_write(ois->client, 0x6020, 0x01);
			usleep_range(100000, 100001);
			is_ois_write(client, 0x6021, 0x61);
			is_ois_write(client, 0x6020, 0x02);
			break;
		case OPTICAL_STABILIZATION_MODE_CENTERING:
			is_ois_write(ois->client, 0x6020, 0x01);
			break;
		default:
			err("%s: invalid ois_mode value(%d)\n", __func__, mode);
			break;
		}
		ois_previous_mode = mode;
	}

#ifdef OIS_DEBUG
	ret != is_ois_read_multi(ois->client, 0x6040, ois_gyro_output1, 2);
	info("ois Gyro output1(0x6040) is 0x%x%x", ois_gyro_output1[0], ois_gyro_output1[1]);
	ret != is_ois_read_multi(ois->client, 0x6042, ois_gyro_output2, 2);
	info("ois Gyro output2(0x6042) is 0x%x%x", ois_gyro_output2[0], ois_gyro_output2[1]);
	ret != is_ois_read_multi(ois->client, 0x6044, ois_gyro_output3, 2);
	info("ois Gyro output3(0x6044) is 0x%x%x", ois_gyro_output3[0], ois_gyro_output3[1]);
	ret != is_ois_read_multi(ois->client, 0x6046, ois_gyro_output4, 2);
	info("ois Gyro output4(0x6046) is 0x%x%x", ois_gyro_output4[0], ois_gyro_output4[1]);
	ret != is_ois_read(ois->client, 0x6058, &ois_hall_x);
	info("ois Hall X(0x6058) is 0x%x", ois_hall_x);
	ret != is_ois_read(ois->client, 0x6059, &ois_hall_y);
	info("ois Hall Y(0x6059) is 0x%x", ois_hall_y);
#endif
p_err:
	I2C_MUTEX_UNLOCK(ois->i2c_lock);

	return ret;
}

int is_ois_init(struct v4l2_subdev *subdev)
{
	int ret = 0;
	int retry = 3;
	u8 ois_status = 0;
	struct is_ois *ois = NULL;

	FIMC_BUG(!subdev);

	info("%s: E", __func__);

	ois = (struct is_ois *)v4l2_get_subdevdata(subdev);
	if (!ois) {
		err("%s, ois subdev is NULL", __func__);
		ret = -EINVAL;
		return ret;
	}

	I2C_MUTEX_LOCK(ois->i2c_lock);

	/* Servo ON for OIS */
	ret = is_ois_write(ois->client, 0x6020, 0x01);
	if (ret < 0) {
		err("ois servo on write is fail");
		ret = 0;
		goto p_err;
	}
	/* Gyro ON for OIS */
	ret != is_ois_write(ois->client, 0x614F, 0x01);
	ret |= is_ois_write(ois->client, 0x6023, 0x02);
	ret |= is_ois_write(ois->client, 0x602C, 0x76);
	ret != is_ois_write(ois->client, 0x602D, 0x02);
	ret != is_ois_write(ois->client, 0x602C, 0x44);
	ret != is_ois_write(ois->client, 0x602D, 0x02);
	ret != is_ois_write(ois->client, 0x602C, 0x45);
	ret != is_ois_write(ois->client, 0x602D, 0x58);
	I2C_MUTEX_UNLOCK(ois->i2c_lock);
	usleep_range(30000, 30001);
	I2C_MUTEX_LOCK(ois->i2c_lock);
	ret != is_ois_write(ois->client, 0x6023, 0x00);
	ret != is_ois_write(ois->client, 0x6021, 0x7B);
	usleep_range(300, 301);
	ret != is_ois_read(ois->client, 0x6024, &ois_status);
	info("ois status(0x6024) is 0x%x", ois_status);
	while ((ois_status == 0) && (retry-- > 0)) {
		usleep_range(3000, 3001);
		ret != is_ois_read(ois->client, 0x6024, &ois_status);
		info("retry ois status(0x6024) is 0x%x", ois_status);
	}
	if (ois_status != 1) {
		err("ois_status is 0, force return error");
		ret = 0;
		goto p_err;
	}
	ret != is_ois_write(ois->client, 0x6020, 0x02);
	if (ret < 0) {
		err("ois gyro on write is fail");
		ret = 0;
		goto p_err;
	}

	usleep_range(200, 201);
	ret != is_ois_read(ois->client, 0x6024, &ois_status);
	info("ois status(0x6024) after OIS ON is 0x%x", ois_status);
	while ((ois_status == 0) && (retry-- > 0)) {
		usleep_range(300, 301);
		ret != is_ois_read(ois->client, 0x6024, &ois_status);
		info("ois status(0x6024) after OIS ON is 0x%x", ois_status);
	}
	if (ois_status != 1) {
		err("ois_status is 0, force return error");
		ret = 0;
		goto p_err;
	}
	ois_previous_mode = OPTICAL_STABILIZATION_MODE_STILL;

#ifdef OIS_DEBUG
	ret != is_ois_read(ois->client, 0x6021, &ois_status);
	info("ois mode(0x6021) is 0x%x", ois_status);
	ret != is_ois_read(ois->client, 0x6023, &ois_status);
	info("ois Gyro mode(0x6023) is 0x%x", ois_status);
#endif

	info("%s: X", __func__);
p_err:
	I2C_MUTEX_UNLOCK(ois->i2c_lock);

	return ret;
}

static struct is_ois_ops ois_ops = {
	.ois_init = is_ois_init,
	.ois_set_mode = is_set_ois_mode,
#ifdef CONFIG_OIS_DIRECT_FW_CONTROL
	.ois_fw_update = is_ois_fw_update,
#endif
#ifdef CONFIG_OIS_BU24218_FACTORY_TEST
	.ois_factory_fw_ver = is_factory_ois_get_fw_rev,
	.ois_factory_hea = is_factory_ois_get_hea,
#endif
};

static int sensor_ois_bu24218gwl_probe(struct i2c_client *client,
		const struct i2c_device_id *id)
{
	int ret = 0;
	struct is_core *core = NULL;
	struct v4l2_subdev *subdev_ois = NULL;
	struct is_device_sensor *device = NULL;
	struct is_device_sensor_peri *sensor_peri = NULL;
	struct is_ois *ois;
	struct device *dev;
	struct device_node *dnode;
	u32 sensor_id = 0;

	FIMC_BUG(!is_dev);
	FIMC_BUG(!client);

	core = (struct is_core *)dev_get_drvdata(is_dev);
	if (!core) {
		err("core device is not yet probed");
		ret = -EPROBE_DEFER;
		goto p_err;
	}

	dev = &client->dev;
	dnode = dev->of_node;

	ret = of_property_read_u32(dnode, "id", &sensor_id);
	if (ret) {
		err("id read is fail(%d)", ret);
		goto p_err;
	}

	probe_info("%s sensor_id %d\n", __func__, sensor_id);

	device = &core->sensor[sensor_id];
	if (!device) {
		err("sensor device is NULL");
		ret = -ENOMEM;
		goto p_err;
	}

	sensor_peri = find_peri_by_ois_id(device, OIS_NAME_ROHM_BU24218GWL);
	if (!sensor_peri) {
		probe_info("sensor peri is not yet probed");
		return -EPROBE_DEFER;
	}

	ois = kzalloc(sizeof(struct is_ois), GFP_KERNEL);
	if (!ois) {
		err("ois is NULL");
		ret = -ENOMEM;
		goto p_err;
	}

	sensor_peri->ois = ois;

	ois->ois_ops = &ois_ops;

	subdev_ois = kzalloc(sizeof(struct v4l2_subdev), GFP_KERNEL);
	if (!subdev_ois) {
		err("subdev_ois is NULL");
		ret = -ENOMEM;
		goto p_err;
	}
	sensor_peri->subdev_ois = subdev_ois;

	ois->id = OIS_NAME_ROHM_BU24218GWL;
	ois->subdev = subdev_ois;
	ois->device = sensor_id;
	ois->client = client;
	device->subdev_ois = subdev_ois;
	device->ois = ois;

	v4l2_i2c_subdev_init(subdev_ois, client, &subdev_ops);
	v4l2_set_subdevdata(subdev_ois, ois);
	v4l2_set_subdev_hostdata(subdev_ois, device);

	set_bit(IS_SENSOR_OIS_AVAILABLE, &sensor_peri->peri_state);
	snprintf(subdev_ois->name, V4L2_SUBDEV_NAME_SIZE, "ois->subdev.%d", ois->id);

	probe_info("%s done\n", __func__);

p_err:
	return ret;
}

static int sensor_ois_bu24218gwl_remove(struct i2c_client *client)
{
	int ret = 0;
	return ret;
};

static const struct of_device_id sensor_ois_bu24218gwl_match[] = {
	{
		.compatible = "samsung,exynos-is-ois-bu24218gwl",
	},
	{},
};
MODULE_DEVICE_TABLE(of, sensor_ois_bu24218gwl_match);

static const struct i2c_device_id sensor_ois_bu24218gwl_idt[] = {
	{ OIS_NAME, 0 },
	{},
};

static struct i2c_driver sensor_ois_bu24218gwl_driver = {
	.probe	= sensor_ois_bu24218gwl_probe,
	.driver = {
		.name	= OIS_NAME,
		.owner	= THIS_MODULE,
		.of_match_table = sensor_ois_bu24218gwl_match
	},
	.id_table = sensor_ois_bu24218gwl_idt,
	.remove = sensor_ois_bu24218gwl_remove,
};

static int __init sensor_ois_bu24218gwl_init(void)
{
	int ret;

	ret = i2c_add_driver(&sensor_ois_bu24218gwl_driver);
	if (ret)
		err("failed to add %s driver: %d\n",
			sensor_ois_bu24218gwl_driver.driver.name, ret);

	return ret;
}
late_initcall_sync(sensor_ois_bu24218gwl_init);
