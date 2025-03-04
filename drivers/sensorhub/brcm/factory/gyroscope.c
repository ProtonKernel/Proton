
/*
 *  Copyright (C) 2012, Samsung Electronics Co. Ltd. All Rights Reserved.
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
/* factory Sysfs							 */
/*************************************************************************/

#define CALIBRATION_FILE_PATH	"/efs/FactoryApp/gyro_cal_data"

#define SELFTEST_REVISED 1

#define CALDATATOTALMAX		15
#define CALDATAFIELDLENGTH  17

static u8 gyroCalDataInfo[CALDATATOTALMAX][CALDATAFIELDLENGTH];
static int gyroCalDataIndx = -1;

struct sensor_manager gyro_manager;
#define get_gyro(dev) ((gyro *)get_sensor_ptr(dev_get_drvdata(dev), &gyro_manager, GYROSCOPE_SENSOR))

struct gyroscope_t gyro_default = {
	.name = " ",
	.vendor = " ",	
	.get_gyro_power_off = sensor_default_show,
	.get_gyro_power_on = sensor_default_show,
	.get_gyro_temperature = sensor_default_show,
	.get_gyro_selftest = sensor_default_show,
	.get_gyro_selftest_dps = sensor_default_show,
	.set_gyro_selftest_dps = sensor_default_store,
};

int gyro_open_calibration(struct ssp_data *data)
{
	int iRet = file_manager_read(data, CALIBRATION_FILE_PATH, (char *)&data->gyrocal, 3 * sizeof(int), 0);

	ssp_dbg("[SSP]: open gyro calibration %d, %d, %d\n",
		data->gyrocal.x, data->gyrocal.y, data->gyrocal.z);
	return iRet;
}

int save_gyro_caldata(struct ssp_data *data, s32 *iCalData)
{
	int iRet = 0;
	u8 gyro_mag_cal[25] = {0, };

	data->gyrocal.x = iCalData[0];
	data->gyrocal.y = iCalData[1];
	data->gyrocal.z = iCalData[2];

	ssp_dbg("[SSP]: do gyro calibrate %d, %d, %d\n",
		data->gyrocal.x, data->gyrocal.y, data->gyrocal.z);

	iRet = file_manager_write(data, CALIBRATION_FILE_PATH, (char *)&data->gyrocal, 3 * sizeof(int), 0);

	if (iRet < 0) {
		pr_err("[SSP]: %s - Can't write gyro cal to file\n", __func__);
		iRet = -EIO;
	}

	iRet = file_manager_read(data, CALIBRATION_FILE_PATH, (char *)&gyro_mag_cal, 25, 0);

	pr_err("[SSP]: %s, gyro_cal= %d %d %d %d %d %d %d %d %d %d %d %d", __func__,
			gyro_mag_cal[0], gyro_mag_cal[1], gyro_mag_cal[2], gyro_mag_cal[3],
			gyro_mag_cal[4], gyro_mag_cal[5], gyro_mag_cal[6], gyro_mag_cal[7],
			gyro_mag_cal[8], gyro_mag_cal[9], gyro_mag_cal[10], gyro_mag_cal[11]);
	pr_err("[SSP]: %s, mag_cal= %d %d %d %d %d %d %d %d %d %d %d %d %d", __func__,
			gyro_mag_cal[12], gyro_mag_cal[13], gyro_mag_cal[14], gyro_mag_cal[15],
			gyro_mag_cal[16], gyro_mag_cal[17], gyro_mag_cal[18], gyro_mag_cal[19],
			gyro_mag_cal[20], gyro_mag_cal[21], gyro_mag_cal[22], gyro_mag_cal[23], gyro_mag_cal[24]);

	return iRet;
}

int set_gyro_cal(struct ssp_data *data)
{
	int iRet = 0;
	struct ssp_msg *msg;
	s32 gyro_cal[3];

	if (!(data->uSensorState & (1 << GYROSCOPE_SENSOR))) {
		pr_info("[SSP]: %s - Skip this function!!!, gyro sensor is not connected(0x%llx)\n",
			__func__, data->uSensorState);
		return iRet;
	}

	gyro_cal[0] = data->gyrocal.x;
	gyro_cal[1] = data->gyrocal.y;
	gyro_cal[2] = data->gyrocal.z;

	msg = kzalloc(sizeof(*msg), GFP_KERNEL);
	msg->cmd = MSG2SSP_AP_MCU_SET_GYRO_CAL;
	msg->length = sizeof(gyro_cal);
	msg->options = AP2HUB_WRITE;
	msg->buffer = kzalloc(sizeof(gyro_cal), GFP_KERNEL);

	msg->free_buffer = 1;
	memcpy(msg->buffer, gyro_cal, sizeof(gyro_cal));

	iRet = ssp_spi_async(data, msg);

	if (iRet != SUCCESS) {
		pr_err("[SSP]: %s - i2c fail %d\n", __func__, iRet);
		iRet = ERROR;
	}

	pr_info("[SSP] Set gyro cal data %d, %d, %d\n", gyro_cal[0], gyro_cal[1], gyro_cal[2]);
	return iRet;
}

short get_gyro_temperature(struct ssp_data *data)
{
	char chTempBuf[2] = { 0};
	unsigned char reg[2];
	short temperature = 0;
	int iRet = 0;
	struct ssp_msg *msg = kzalloc(sizeof(*msg), GFP_KERNEL);

	msg->cmd = GYROSCOPE_TEMP_FACTORY;
	msg->length = 2;
	msg->options = AP2HUB_READ;
	msg->buffer = chTempBuf;
	msg->free_buffer = 0;

	iRet = ssp_spi_sync(data, msg, 3000);

	if (iRet != SUCCESS) {
		pr_err("[SSP]: %s - Gyro Temp Timeout!!\n", __func__);
		goto exit;
	}

	reg[0] = chTempBuf[1];
	reg[1] = chTempBuf[0];
	temperature = (short) (((reg[0]) << 8) | reg[1]);
	ssp_dbg("[SSP]: %s - %d\n", __func__, temperature);

exit:
	return temperature;
}

static ssize_t gyro_vendor_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
   	return sprintf(buf, "%s\n", get_gyro(dev)->vendor);
}

static ssize_t gyro_name_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
    return sprintf(buf, "%s\n", get_gyro(dev)->name);
}

static ssize_t selftest_revised_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	return snprintf(buf, PAGE_SIZE, "%d\n", SELFTEST_REVISED);
}

static ssize_t gyro_power_off(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	return get_gyro(dev)->get_gyro_power_off(dev, buf);
}

static ssize_t gyro_power_on(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	return get_gyro(dev)->get_gyro_power_on(dev, buf);
}

static ssize_t gyro_get_temperature(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	return get_gyro(dev)->get_gyro_temperature(dev,buf);
}

static ssize_t gyro_selftest_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	return get_gyro(dev)->get_gyro_selftest(dev,buf);
}

static ssize_t gyro_selftest_dps_store(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t count)
{
	return get_gyro(dev)->set_gyro_selftest_dps(dev,buf,count);
}

static ssize_t gyro_selftest_dps_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	return get_gyro(dev)->get_gyro_selftest_dps(dev,buf);
}

static ssize_t gyro_calibration_info_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
   // struct ssp_data *data = dev_get_drvdata(dev);
	s32 i, j, float_bias;
       static char printCalData[1024*3] = {0, };

	memset(printCalData, 0, sizeof(printCalData));

	if (gyroCalDataIndx == -1)
		return 0;
	for (i = 0; i <= gyroCalDataIndx; i++) {
		char temp[300] = {0, };
		struct gyro_bigdata_info infoFrame = {0, };

		memcpy(&infoFrame, gyroCalDataInfo[i], sizeof(struct gyro_bigdata_info));

		sprintf(temp, "VERSION:%hd,UPDATE_INDEX:%d,",
		infoFrame.version, infoFrame.updated_index);
		strcat(printCalData, temp);

		for (j = 0; j < 3; j++) {
			float_bias = ((infoFrame.bias[j] % 1000) >  0 ? (infoFrame.bias[j] % 1000)
					: -(infoFrame.bias[j] % 1000));

			if (infoFrame.bias[j] / 1000 == 0)
				infoFrame.bias[j] > 0 ? (sprintf(temp, "BIAS_%c:0.%03d,", 'X'+j, float_bias))
					: (sprintf(temp, "BIAS_%c:-0.%03d,", 'X'+j, float_bias));
			else
				sprintf(temp, "BIAS_%c:%d.%03d,", 'X'+j,  infoFrame.bias[j] / 1000, float_bias);

			strcat(printCalData, temp);
		}
		sprintf(temp, "TEMPERATURE:%d;", infoFrame.temperature);
		strcat(printCalData, temp);
	}

	gyroCalDataIndx = -1;
	return sprintf(buf, "%s", printCalData);
}

void set_GyroCalibrationInfoData(char *pchRcvDataFrame, int *iDataIdx)
{
	if (gyroCalDataIndx < (CALDATATOTALMAX - 1))
		memcpy(gyroCalDataInfo[++gyroCalDataIndx],  pchRcvDataFrame + *iDataIdx, CALDATAFIELDLENGTH);

	*iDataIdx += CALDATAFIELDLENGTH;
}

int send_vdis_flag(struct ssp_data *data, bool bFlag)
{
	int iRet = 0;
	struct ssp_msg *msg;
	char flag = bFlag;
	if (!(data->uSensorState & (1 << GYROSCOPE_SENSOR))) {
		pr_info("[SSP]: %s - Skip this function!!!, gyro sensor is not connected(0x%llx)\n",
			__func__, data->uSensorState);
		return iRet;
	}

	msg = kzalloc(sizeof(*msg), GFP_KERNEL);
	msg->cmd = MSG2SSP_INST_VDIS_FLAG;
	msg->length = 1;
	msg->options = AP2HUB_WRITE;
	msg->buffer = &flag;

	iRet = ssp_spi_async(data, msg);

	if (iRet != SUCCESS) {
		pr_err("[SSP]: %s - i2c fail %d\n", __func__, iRet);
		iRet = ERROR;
	}

	pr_info("[SSP] Set VDIS FLAG %d\n", bFlag);
	return iRet;
}

static DEVICE_ATTR(name, 0440, gyro_name_show, NULL);
static DEVICE_ATTR(vendor, 0440, gyro_vendor_show, NULL);
static DEVICE_ATTR(power_off, 0440, gyro_power_off, NULL);
static DEVICE_ATTR(power_on, 0440, gyro_power_on, NULL);
static DEVICE_ATTR(temperature, 0440, gyro_get_temperature, NULL);
static DEVICE_ATTR(selftest, 0440, gyro_selftest_show, NULL);
static DEVICE_ATTR(selftest_dps, 0660,
	gyro_selftest_dps_show, gyro_selftest_dps_store);
static DEVICE_ATTR(calibration_info, 0440, gyro_calibration_info_show, NULL);
static DEVICE_ATTR(selftest_revised, 0440, selftest_revised_show, NULL);

static struct device_attribute *gyro_attrs[] = {
	&dev_attr_name,
	&dev_attr_vendor,
	&dev_attr_selftest,
	&dev_attr_power_on,
	&dev_attr_power_off,
	&dev_attr_temperature,
	&dev_attr_selftest_dps,
	&dev_attr_calibration_info,
	&dev_attr_selftest_revised,
	NULL,
};

void initialize_gyro_factorytest(struct ssp_data *data)
{
    memset(&gyro_manager, 0, sizeof(gyro_manager));
	push_back(&gyro_manager, "DEFAULT", &gyro_default);
#ifdef CONFIG_SENSORS_LSM6DSO
    push_back(&gyro_manager, "LSM6DSO", get_gyro_lsm6dso());
#endif
#ifdef CONFIG_SENSORS_ICM42632M
    push_back(&gyro_manager, "ICM42632M", get_gyro_icm42632m());
#endif
	sensors_register(&data->gyro_device, data, gyro_attrs, "gyro_sensor");
}

void remove_gyro_factorytest(struct ssp_data *data)
{
	sensors_unregister(data->gyro_device, gyro_attrs);
}

