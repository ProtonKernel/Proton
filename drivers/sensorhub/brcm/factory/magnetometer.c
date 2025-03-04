/*
 *  Copyright (C) 2015, Samsung Electronics Co. Ltd. All Rights Reserved.
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 */
#include <linux/kernel.h>
#include "../ssp.h"
#include "sensors.h"
/*************************************************************************/
/* factory Sysfs                                                         */
/*************************************************************************/

#define CALIBRATION_MAX 13
#define YAS_STATIC_ELLIPSOID_MATRIX	{10000, 0, 0, 0, 10000, 0, 0, 0, 10000}
#define MAG_HW_OFFSET_FILE_PATH	"/efs/FactoryApp/hw_offset"
#define MAG_CAL_PARAM_FILE_PATH	"/efs/FactoryApp/gyro_cal_data"

struct sensor_manager mag_manager;

#define get_mag(data) ((mag *)get_sensor_ptr(data, &mag_manager, GEOMAGNETIC_UNCALIB_SENSOR))

struct magnetometer_t mag_default = {
    .name = " ",
	.vendor = " ",
	.initialize = sensor_default_initialize,
	.check_data_spec = sensor_default_check_func,
	.get_magnetic_asa = sensor_default_show,
	.get_magnetic_matrix = sensor_default_show,
	.set_magnetic_matrix = sensor_default_store,
	.get_magnetic_selftest = sensor_default_show,
	.get_magnetic_si_param = sensor_default_show,
};

/* AKM Functions */
static ssize_t magnetic_get_asa(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	return get_mag(dev_get_drvdata(dev))->get_magnetic_asa(dev, buf);
}

static ssize_t magnetic_get_status(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	bool bSuccess;
	struct ssp_data *data = dev_get_drvdata(dev);

	if ((data->uFuseRomData[0] == 0) ||
		(data->uFuseRomData[0] == 0xff) ||
		(data->uFuseRomData[1] == 0) ||
		(data->uFuseRomData[1] == 0xff) ||
		(data->uFuseRomData[2] == 0) ||
		(data->uFuseRomData[2] == 0xff))
		bSuccess = false;
	else
		bSuccess = true;

	return sprintf(buf, "%s,%u\n", (bSuccess ? "OK" : "NG"), bSuccess);
}

static ssize_t magnetic_check_cntl(struct device *dev,
		struct device_attribute *attr, char *strbuf)
{
	bool bSuccess = false;
	int ret;
	char chTempBuf[22] = { 0,  };
	struct ssp_data *data = dev_get_drvdata(dev);
	struct ssp_msg *msg;

	if (!data->uMagCntlRegData) {
		bSuccess = true;
	} else {
		pr_info("[SSP] %s - check cntl register before selftest",
			__func__);
		msg = kzalloc(sizeof(*msg), GFP_KERNEL);
		msg->cmd = GEOMAGNETIC_FACTORY;
		msg->length = 22;
		msg->options = AP2HUB_READ;
		msg->buffer = chTempBuf;
		msg->free_buffer = 0;

		ret = ssp_spi_sync(data, msg, 1000);

		if (ret != SUCCESS) {
			pr_err("[SSP] %s - spi sync failed due to Timeout!! %d\n",
					__func__, ret);
		}

		data->uMagCntlRegData = chTempBuf[21];
		bSuccess = !data->uMagCntlRegData;
	}

	pr_info("[SSP] %s - CTRL : 0x%x\n", __func__,
				data->uMagCntlRegData);

	data->uMagCntlRegData = 1;	/* reset the value */

	return sprintf(strbuf, "%s,%d,%d,%d\n",
		(bSuccess ? "OK" : "NG"), (bSuccess ? 1 : 0), 0, 0);
}

static ssize_t magnetic_logging_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	char buffer[21] = {0, };
	int ret = 0;
	int logging_data[8] = {0, };
	struct ssp_data *data = dev_get_drvdata(dev);
	struct ssp_msg *msg;

	msg = kzalloc(sizeof(*msg), GFP_KERNEL);
	msg->cmd = MSG2SSP_AP_GEOMAG_LOGGING;
	msg->length = 21;
	msg->options = AP2HUB_READ;
	msg->buffer = buffer;
	msg->free_buffer = 0;

	ret = ssp_spi_sync(data, msg, 1000);
	if (ret != SUCCESS) {
		pr_err("[SSP] %s - Magnetic logging data Timeout!! %d\n",
			__func__, ret);
		goto exit;
	}

	logging_data[0] = buffer[0];	/* ST1 Reg */
	logging_data[1] = (short)((buffer[3] << 8) + buffer[2]);
	logging_data[2] = (short)((buffer[5] << 8) + buffer[4]);
	logging_data[3] = (short)((buffer[7] << 8) + buffer[6]);
	logging_data[4] = buffer[1];	/* ST2 Reg */
	logging_data[5] = (short)((buffer[9] << 8) + buffer[8]);
	logging_data[6] = (short)((buffer[11] << 8) + buffer[10]);
	logging_data[7] = (short)((buffer[13] << 8) + buffer[12]);

	return snprintf(buf, PAGE_SIZE, "%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d\n",
			logging_data[0], logging_data[1],
			logging_data[2], logging_data[3],
			logging_data[4], logging_data[5],
			logging_data[6], logging_data[7],
			data->uFuseRomData[0], data->uFuseRomData[1],
			data->uFuseRomData[2]);
exit:
	return snprintf(buf, PAGE_SIZE, "-1,0,0,0,0,0,0,0,0,0,0\n");
}

int mag_open_hwoffset(struct ssp_data *data)
{
	int iRet = 0;

	data->magoffset.x = 0;
	data->magoffset.y = 0;
	data->magoffset.z = 0;

	iRet = file_manager_read(data, MAG_HW_OFFSET_FILE_PATH, (char *)&data->magoffset, 3 * sizeof(char), 0);
	if (iRet != 3 * sizeof(char)) {
		pr_err("[SSP] %s: filp_open failed\n", __func__);
		iRet = -EIO;
	}

	ssp_dbg("[SSP]: %s: %d, %d, %d\n", __func__,
		(s8)data->magoffset.x,
		(s8)data->magoffset.y,
		(s8)data->magoffset.z);

	if ((data->magoffset.x == 0) && (data->magoffset.y == 0)
		&& (data->magoffset.z == 0))
		return ERROR;

	return iRet;
}

int mag_store_hwoffset(struct ssp_data *data)
{
	int iRet = 0;
	struct file *cal_filp = NULL;
	mm_segment_t old_fs;

	if (get_hw_offset(data) < 0) {
		pr_err("[SSP]: %s - get_hw_offset failed\n", __func__);
		return ERROR;
	}
	old_fs = get_fs();
	set_fs(KERNEL_DS);

	cal_filp = filp_open(MAG_HW_OFFSET_FILE_PATH,
		O_CREAT | O_TRUNC | O_WRONLY, 0660);
	if (IS_ERR(cal_filp)) {
		pr_err("[SSP]: %s - Can't open hw_offset file\n",
			__func__);
		set_fs(old_fs);
		iRet = PTR_ERR(cal_filp);
		return iRet;
	}
	iRet = cal_filp->f_op->write(cal_filp,
		(char *)&data->magoffset,
		3 * sizeof(char), &cal_filp->f_pos);
	if (iRet != 3 * sizeof(char)) {
		pr_err("[SSP]: %s - Can't write the hw_offset to file\n",
			__func__);
		iRet = -EIO;
	}
	filp_close(cal_filp, current->files);
	set_fs(old_fs);
	return iRet;
}

int set_hw_offset(struct ssp_data *data)
{
	int iRet = 0;
	struct ssp_msg *msg;

	if (!(data->uSensorState & 0x04)) {
		pr_info("[SSP]: %s - Skip this function!!!, magnetic sensor is not connected(0x%llx)\n",
			__func__, data->uSensorState);
		return iRet;
	}

	msg = kzalloc(sizeof(*msg), GFP_KERNEL);
	msg->cmd = MSG2SSP_AP_SET_MAGNETIC_HWOFFSET;
	msg->length = 3;
	msg->options = AP2HUB_WRITE;
	msg->buffer = kzalloc(3, GFP_KERNEL);
	msg->free_buffer = 1;

	msg->buffer[0] = data->magoffset.x;
	msg->buffer[1] = data->magoffset.y;
	msg->buffer[2] = data->magoffset.z;

	iRet = ssp_spi_async(data, msg);

	if (iRet != SUCCESS) {
		pr_err("[SSP]: %s - i2c fail %d\n", __func__, iRet);
		iRet = ERROR;
	}

	pr_info("[SSP]: %s: x: %d, y: %d, z: %d\n", __func__,
		(s8)data->magoffset.x, (s8)data->magoffset.y, (s8)data->magoffset.z);
	return iRet;
}



int get_hw_offset(struct ssp_data *data)
{
	int iRet = 0;
	char buffer[3] = { 0, };
	struct ssp_msg *msg = kzalloc(sizeof(*msg), GFP_KERNEL);

	msg->cmd = MSG2SSP_AP_GET_MAGNETIC_HWOFFSET;
	msg->length = 3;
	msg->options = AP2HUB_READ;
	msg->buffer = buffer;
	msg->free_buffer = 0;

	data->magoffset.x = 0;
	data->magoffset.y = 0;
	data->magoffset.z = 0;

	iRet = ssp_spi_sync(data, msg, 1000);

	if (iRet != SUCCESS) {
		pr_err("[SSP]: %s - i2c fail %d\n", __func__, iRet);
		iRet = ERROR;
	}

	data->magoffset.x = buffer[0];
	data->magoffset.y = buffer[1];
	data->magoffset.z = buffer[2];

	pr_info("[SSP]: %s: x: %d, y: %d, z: %d\n", __func__,
		(s8)data->magoffset.x,
		(s8)data->magoffset.y,
		(s8)data->magoffset.z);
	return iRet;
}

static ssize_t hw_offset_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	struct ssp_data *data = dev_get_drvdata(dev);

	mag_open_hwoffset(data);

	pr_info("[SSP] %s: %d %d %d\n", __func__,
		(s8)data->magoffset.x,
		(s8)data->magoffset.y,
		(s8)data->magoffset.z);

	return sprintf(buf, "%d %d %d\n",
		(s8)data->magoffset.x,
		(s8)data->magoffset.y,
		(s8)data->magoffset.z);
}

static ssize_t matrix_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	return get_mag(dev_get_drvdata(dev))->get_magnetic_matrix(dev,buf);
}

static ssize_t matrix_store(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t size)
{
    return get_mag(dev_get_drvdata(dev))->set_magnetic_matrix(dev, buf, size);
}

/* common functions */

static ssize_t magnetic_vendor_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
    return sprintf(buf, "%s\n", get_mag(dev_get_drvdata(dev))->vendor);
}

static ssize_t magnetic_name_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
    return sprintf(buf, "%s\n", get_mag(dev_get_drvdata(dev))->name);
}

static ssize_t raw_data_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	struct ssp_data *data = dev_get_drvdata(dev);

	pr_info("[SSP] %s - %d,%d,%d\n", __func__,
		data->buf[GEOMAGNETIC_RAW].x,
		data->buf[GEOMAGNETIC_RAW].y,
		data->buf[GEOMAGNETIC_RAW].z);

	if (data->bGeomagneticRawEnabled == false) {
		data->buf[GEOMAGNETIC_RAW].x = -1;
		data->buf[GEOMAGNETIC_RAW].y = -1;
		data->buf[GEOMAGNETIC_RAW].z = -1;
	}

	return snprintf(buf, PAGE_SIZE, "%d,%d,%d\n",
		data->buf[GEOMAGNETIC_RAW].x,
		data->buf[GEOMAGNETIC_RAW].y,
		data->buf[GEOMAGNETIC_RAW].z);
}


/* Get magnetic cal data from a file */
int load_magnetic_cal_param_from_nvm(struct ssp_data *data, u8 *cal_data, u8 length)
{
	int iRet = file_manager_read(data, MAG_CAL_PARAM_FILE_PATH, (char *)cal_data, length * sizeof(char), 3 * sizeof(int));

	if (iRet != length * sizeof(char)) {
		pr_err("[SSP] %s: filp_open read failed, read size = %d", __func__, iRet);
		iRet = -EIO;
	}

	return iRet;
}


int set_magnetic_cal_param_to_ssp(struct ssp_data *data)
{
	int iRet = 0;
	u8 mag_caldata[CALIBRATION_MAX] = {0, };
	u16 length = 0;

	struct ssp_msg *msg = kzalloc(sizeof(*msg), GFP_KERNEL);

	if (msg == NULL) {
		iRet = -ENOMEM;
		pr_err("[SSP] %s, failed to alloc memory for ssp_msg\n", __func__);
		return iRet;
	}

	length = CALIBRATION_MAX;

    load_magnetic_cal_param_from_nvm(data, mag_caldata, length);
	msg->cmd = MSG2SSP_AP_MAG_CAL_PARAM;
	msg->length = length;
	msg->options = AP2HUB_WRITE;
	msg->buffer = kzalloc(length, GFP_KERNEL);
	msg->free_buffer = 1;
	memcpy(msg->buffer, mag_caldata, length);

	pr_err("[SSP] %s, %d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d\n", __func__,
	mag_caldata[0], mag_caldata[1], mag_caldata[2], mag_caldata[3], mag_caldata[4],
	mag_caldata[5], mag_caldata[6], mag_caldata[7], mag_caldata[8], mag_caldata[9],
	mag_caldata[10], mag_caldata[11], mag_caldata[12]);

	iRet = ssp_spi_async(data, msg);
	if (iRet != SUCCESS) {
		pr_err("[SSP] %s -fail to set. %d\n", __func__, iRet);
		iRet = ERROR;
	}

	return iRet;
}


int save_magnetic_cal_param_to_nvm(struct ssp_data *data, char *pchRcvDataFrame, int *iDataIdx)
{
	int iRet = 0;
	int i = 0;
	int length = 0;
	u8 mag_caldata[CALIBRATION_MAX] = {0, }; //AKM uses 13 byte. YAMAHA uses 7 byte.
	u8 gyro_mag_cal[25] = {0,};

	length = CALIBRATION_MAX;

	//AKM uses 13 byte. YAMAHA uses 7 byte.
	memcpy(mag_caldata, pchRcvDataFrame + *iDataIdx, length);

	ssp_dbg("[SSP]: %s\n", __func__);
	for (i = 0; i < length; i++) {
		if (data->mag_type == MAG_TYPE_AKM)
			ssp_dbg("[SSP] mag cal param[%d] %d\n", i, mag_caldata[i]);
    }

	iRet = file_manager_write(data, MAG_CAL_PARAM_FILE_PATH, (char *)mag_caldata, length * sizeof(char), 12);

	if (iRet != length * sizeof(char)) {
		pr_err("[SSP]: %s - Can't write mag cal to file\n", __func__);
		iRet = -EIO;
	}

	iRet = file_manager_read(data, MAG_CAL_PARAM_FILE_PATH, (char *)gyro_mag_cal, 25, 0);

	pr_err("[SSP]: %s, gyro_cal= %d %d %d %d %d %d %d %d %d %d %d %d", __func__,
			gyro_mag_cal[0], gyro_mag_cal[1], gyro_mag_cal[2], gyro_mag_cal[3],
			gyro_mag_cal[4], gyro_mag_cal[5], gyro_mag_cal[6], gyro_mag_cal[7],
			gyro_mag_cal[8], gyro_mag_cal[9], gyro_mag_cal[10], gyro_mag_cal[11]);
	pr_err("[SSP]: %s, mag_cal= %d %d %d %d %d %d %d %d %d %d %d %d %d", __func__,
			gyro_mag_cal[12], gyro_mag_cal[13], gyro_mag_cal[14], gyro_mag_cal[15],
			gyro_mag_cal[16], gyro_mag_cal[17], gyro_mag_cal[18], gyro_mag_cal[19],
			gyro_mag_cal[20], gyro_mag_cal[21], gyro_mag_cal[22], gyro_mag_cal[23], gyro_mag_cal[24]);

	*iDataIdx += length;

	return iRet;
}

static ssize_t raw_data_store(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t size)
{
	char chTempbuf[4] = { 0 };
	int ret;
	int64_t dEnable;
	int retries = 50;
	struct ssp_data *data = dev_get_drvdata(dev);
	s32 dMsDelay = 20;

	memcpy(&chTempbuf[0], &dMsDelay, 4);

	ret = kstrtoll(buf, 10, &dEnable);
	if (ret < 0)
		return ret;

	if (dEnable) {
		data->buf[GEOMAGNETIC_RAW].x = 0;
		data->buf[GEOMAGNETIC_RAW].y = 0;
		data->buf[GEOMAGNETIC_RAW].z = 0;

		send_instruction(data, ADD_SENSOR, GEOMAGNETIC_RAW,
			chTempbuf, 4);

		do {
			msleep(20);
			if (get_mag(data)->check_data_spec(data, GEOMAGNETIC_RAW) == SUCCESS)
				break;
		} while (--retries);

		if (retries > 0) {
			pr_info("[SSP] %s - success, %d\n", __func__, retries);
			data->bGeomagneticRawEnabled = true;
		} else {
			pr_err("[SSP] %s - wait timeout, %d\n", __func__,
				retries);
			data->bGeomagneticRawEnabled = false;
		}


	} else {
		send_instruction(data, REMOVE_SENSOR, GEOMAGNETIC_RAW,
			chTempbuf, 4);
		data->bGeomagneticRawEnabled = false;
	}

	return size;
}

static ssize_t adc_data_read(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	bool bSuccess = false;
	u8 chTempbuf[4] = { 0 };
	s16 iSensorBuf[3] = {0, };
	int retries = 10;
	struct ssp_data *data = dev_get_drvdata(dev);
	s32 dMsDelay = 20;

	memcpy(&chTempbuf[0], &dMsDelay, 4);

	data->buf[GEOMAGNETIC_SENSOR].x = 0;
	data->buf[GEOMAGNETIC_SENSOR].y = 0;
	data->buf[GEOMAGNETIC_SENSOR].z = 0;

	if (!(atomic64_read(&data->aSensorEnable) & (1 << GEOMAGNETIC_SENSOR)))
		send_instruction(data, ADD_SENSOR, GEOMAGNETIC_SENSOR,
			chTempbuf, 4);

	do {
		msleep(60);
		if (get_mag(data)->check_data_spec(data, GEOMAGNETIC_SENSOR) == SUCCESS)
			break;
	} while (--retries);

	if (retries > 0)
		bSuccess = true;

	iSensorBuf[0] = data->buf[GEOMAGNETIC_SENSOR].x;
	iSensorBuf[1] = data->buf[GEOMAGNETIC_SENSOR].y;
	iSensorBuf[2] = data->buf[GEOMAGNETIC_SENSOR].z;

	if (!(atomic64_read(&data->aSensorEnable) & (1 << GEOMAGNETIC_SENSOR)))
		send_instruction(data, REMOVE_SENSOR, GEOMAGNETIC_SENSOR,
			chTempbuf, 4);

	pr_info("[SSP] %s - x = %d, y = %d, z = %d\n", __func__,
		iSensorBuf[0], iSensorBuf[1], iSensorBuf[2]);

	return sprintf(buf, "%s,%d,%d,%d\n", (bSuccess ? "OK" : "NG"),
		iSensorBuf[0], iSensorBuf[1], iSensorBuf[2]);
}

static ssize_t magnetic_get_selftest(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	return get_mag(dev_get_drvdata(dev))->get_magnetic_selftest(dev, buf);
}
static ssize_t si_param_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
    return get_mag(dev_get_drvdata(dev))->get_magnetic_si_param(dev, buf);
}

static DEVICE_ATTR(name, 0440, magnetic_name_show, NULL);
static DEVICE_ATTR(vendor, 0440, magnetic_vendor_show, NULL);
static DEVICE_ATTR(raw_data, 0660,
		raw_data_show, raw_data_store);
static DEVICE_ATTR(adc, 0440, adc_data_read, NULL);
static DEVICE_ATTR(selftest, 0440, magnetic_get_selftest, NULL);

static DEVICE_ATTR(status, 0440,  magnetic_get_status, NULL);
static DEVICE_ATTR(dac, 0440, magnetic_check_cntl, NULL);
static DEVICE_ATTR(ak09911_asa, 0440, magnetic_get_asa, NULL);
static DEVICE_ATTR(logging_data, 0440, magnetic_logging_show, NULL);

static DEVICE_ATTR(hw_offset, 0440, hw_offset_show, NULL);
static DEVICE_ATTR(matrix, 0660, matrix_show, matrix_store);
static DEVICE_ATTR(dhr_sensor_info, 0440, si_param_show, NULL);

static struct device_attribute *mag_attrs[] = {
	&dev_attr_name,
	&dev_attr_vendor,
	&dev_attr_adc,
	&dev_attr_dac,
	&dev_attr_raw_data,
	&dev_attr_selftest,
	&dev_attr_status,
	&dev_attr_ak09911_asa,
	&dev_attr_logging_data,
	&dev_attr_hw_offset,
	&dev_attr_matrix,
	&dev_attr_dhr_sensor_info,
	NULL,
};

int initialize_magnetic_sensor(struct ssp_data *data)
{
	int ret, i;

	for(i = 0;  i < mag_manager.size; i++)
		((mag *)mag_manager.item[i])->initialize(data);

	ret = set_magnetic_cal_param_to_ssp(data);

	if (ret < 0)
		pr_err("[SSP]: %s - set_magnetic_static_matrix failed %d\n", __func__, ret);

	return ret < 0 ? ret : SUCCESS;
}

void initialize_magnetic_factorytest(struct ssp_data *data)
{
    memset(&mag_manager, 0, sizeof(mag_manager));
	push_back(&mag_manager, "DEFAULT", &mag_default);
#ifdef CONFIG_SENSORS_AK09918C
    push_back(&mag_manager, "AK09918C", get_mag_ak09918c());
#endif
    
	sensors_register(&data->mag_device, data, mag_attrs, "magnetic_sensor");
}

void remove_magnetic_factorytest(struct ssp_data *data)
{
	sensors_unregister(data->mag_device, mag_attrs);
}
