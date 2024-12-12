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
#include "../ssp.h"
#include "sensors.h"

/*************************************************************************/
/* factory Sysfs							 */
/*************************************************************************/

#define CALIBRATION_FILE_PATH	"/efs/FactoryApp/calibration_data"
#define CALIBRATION_DATA_AMOUNT	20

#define CALDATATOTALMAX		20
#define CALDATAFIELDLENGTH  17

static u8 accelCalDataInfo[CALDATATOTALMAX][CALDATAFIELDLENGTH];
static int accelCalDataIndx = -1;

struct sensor_manager accel_manager;
#define get_accel(dev) ((accel *)get_sensor_ptr(dev_get_drvdata(dev), &accel_manager, ACCELEROMETER_SENSOR))

struct accelerometer_t accel_default = {
	.name = " ",
	.vendor = " ",	
	.get_accel_calibration = sensor_default_show,
	.set_accel_calibration = sensor_default_store,
	.get_accel_reactive_alert = sensor_default_show,
	.set_accel_reactive_alert = sensor_default_store,
	.get_accel_selftest = sensor_default_show,
	.set_accel_lowpassfilter = sensor_default_store
};

int accel_open_calibration(struct ssp_data *data)
{
	int iRet = 0;

	data->accelcal.x = 0;
	data->accelcal.y = 0;
	data->accelcal.z = 0;

	iRet = file_manager_read(data,CALIBRATION_FILE_PATH, (char *)&data->accelcal,  3 * sizeof(int), 0);
	ssp_dbg("[SSP]: open accel calibration %d, %d, %d\n",
		data->accelcal.x, data->accelcal.y, data->accelcal.z);

	if ((data->accelcal.x == 0) && (data->accelcal.y == 0)
		&& (data->accelcal.z == 0))
		return ERROR;

	return iRet;
}

int set_accel_cal(struct ssp_data *data)
{
	int iRet = 0;
	struct ssp_msg *msg;
	s16 accel_cal[3];

	if (!(data->uSensorState & (1 << ACCELEROMETER_SENSOR))) {
		pr_info("[SSP]: %s - Skip this function!!!, accel sensor is not connected(0x%llx)\n",
			__func__, data->uSensorState);
		return iRet;
	}
	accel_cal[0] = data->accelcal.x;
	accel_cal[1] = data->accelcal.y;
	accel_cal[2] = data->accelcal.z;

	msg = kzalloc(sizeof(*msg), GFP_KERNEL);
	msg->cmd = MSG2SSP_AP_MCU_SET_ACCEL_CAL;
	msg->length = 6;
	msg->options = AP2HUB_WRITE;
	msg->buffer = kzalloc(6, GFP_KERNEL);

	msg->free_buffer = 1;
	memcpy(msg->buffer, accel_cal, 6);

	iRet = ssp_spi_async(data, msg);

	if (iRet != SUCCESS) {
		pr_err("[SSP]: %s - i2c fail %d\n", __func__, iRet);
		iRet = ERROR;
	}

	pr_info("[SSP] Set accel cal data %d, %d, %d\n", accel_cal[0], accel_cal[1], accel_cal[2]);
	return iRet;
}

int enable_accel_for_cal(struct ssp_data *data)
{
	u8 uBuf[4] = { 0, };
	s32 dMsDelay = get_msdelay(data->adDelayBuf[ACCELEROMETER_SENSOR]);

	memcpy(&uBuf[0], &dMsDelay, 4);

	if (atomic64_read(&data->aSensorEnable) & (1 << ACCELEROMETER_SENSOR)) {
		if (get_msdelay(data->adDelayBuf[ACCELEROMETER_SENSOR]) != 10) {
			send_instruction(data, CHANGE_DELAY,
				ACCELEROMETER_SENSOR, uBuf, 4);
			return SUCCESS;
		}
	} else {
		send_instruction(data, ADD_SENSOR,
			ACCELEROMETER_SENSOR, uBuf, 4);
	}

	return FAIL;
}

void disable_accel_for_cal(struct ssp_data *data, int iDelayChanged)
{
	u8 uBuf[4] = { 0, };
	s32 dMsDelay = get_msdelay(data->adDelayBuf[ACCELEROMETER_SENSOR]);

	memcpy(&uBuf[0], &dMsDelay, 4);

	if (atomic64_read(&data->aSensorEnable) & (1 << ACCELEROMETER_SENSOR)) {
		if (iDelayChanged)
			send_instruction(data, CHANGE_DELAY,
				ACCELEROMETER_SENSOR, uBuf, 4);
	} else {
		send_instruction(data, REMOVE_SENSOR,
			ACCELEROMETER_SENSOR, uBuf, 4);
	}
}

int accel_do_calibrate(struct ssp_data *data, int iEnable, const int max_accel_1g)
{
	int iSum[3] = { 0, };
	int iRet = 0, iCount;

	if (iEnable) {
		data->accelcal.x = 0;
		data->accelcal.y = 0;
		data->accelcal.z = 0;
		set_accel_cal(data);
		iRet = enable_accel_for_cal(data);
		msleep(300);

		for (iCount = 0; iCount < CALIBRATION_DATA_AMOUNT; iCount++) {
			iSum[0] += data->buf[ACCELEROMETER_SENSOR].x;
			iSum[1] += data->buf[ACCELEROMETER_SENSOR].y;
			iSum[2] += data->buf[ACCELEROMETER_SENSOR].z;
			mdelay(10);
		}
		disable_accel_for_cal(data, iRet);

		data->accelcal.x = (iSum[0] / CALIBRATION_DATA_AMOUNT);
		data->accelcal.y = (iSum[1] / CALIBRATION_DATA_AMOUNT);
		data->accelcal.z = (iSum[2] / CALIBRATION_DATA_AMOUNT);

		if (data->accelcal.z > 0)
			data->accelcal.z -= max_accel_1g;
		else if (data->accelcal.z < 0)
			data->accelcal.z += max_accel_1g;
	} else {
		data->accelcal.x = 0;
		data->accelcal.y = 0;
		data->accelcal.z = 0;
	}

	ssp_dbg("[SSP]: do accel calibrate %d, %d, %d\n",
		data->accelcal.x, data->accelcal.y, data->accelcal.z);

	file_manager_write(data,CALIBRATION_FILE_PATH, (char *)&data->accelcal, 3 * sizeof(int), 0);

	set_accel_cal(data);
	return iRet;
}

static ssize_t accel_vendor_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	return sprintf(buf, "%s\n", get_accel(dev)->vendor);
}

static ssize_t accel_name_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
    return sprintf(buf, "%s\n", get_accel(dev)->name);
}

static ssize_t accel_calibration_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
    return get_accel(dev)->get_accel_calibration(dev, buf);
}

static ssize_t accel_calibration_store(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t size)
{
	return get_accel(dev)->set_accel_calibration(dev, buf, size);
}

static ssize_t raw_data_read(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	struct ssp_data *data = dev_get_drvdata(dev);

	return snprintf(buf, PAGE_SIZE, "%d,%d,%d\n",
		data->buf[ACCELEROMETER_SENSOR].x,
		data->buf[ACCELEROMETER_SENSOR].y,
		data->buf[ACCELEROMETER_SENSOR].z);
}

static ssize_t accel_reactive_alert_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
    return get_accel(dev)->get_accel_reactive_alert(dev, buf);
}

static ssize_t accel_reactive_alert_store(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t size)
{
    return get_accel(dev)->set_accel_reactive_alert(dev, buf,size);
}

static ssize_t accel_hw_selftest_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
    return get_accel(dev)->get_accel_selftest(dev, buf);
}

static ssize_t accel_lowpassfilter_store(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t size)
{
    return get_accel(dev)->set_accel_lowpassfilter(dev, buf, size);
}

static ssize_t accel_scale_range_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	struct ssp_data *data = dev_get_drvdata(dev);

	return sprintf(buf, "\"FULL_SCALE\":\"%dG\"\n", data->dhrAccelScaleRange);
}

static ssize_t accel_calibration_info_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
   static char printCalData[1024*3] = {0, };
	s32 i, j, float_accuracy, float_bias, float_calNorm, float_uncalNorm;

	memset(printCalData, 0, sizeof(printCalData));

	if (accelCalDataIndx == -1)
		return 0;
	for (i = 0; i <= accelCalDataIndx; i++) {
		char temp[300] = {0, };
		struct accel_bigdata_info infoFrame = {0, };

		memcpy(&infoFrame, accelCalDataInfo[i], sizeof(struct accel_bigdata_info));

		float_accuracy = infoFrame.accuracy % 100;
		float_calNorm = (infoFrame.cal_norm % 1000) >  0 ? (infoFrame.cal_norm % 1000)
			: -(infoFrame.cal_norm % 1000);

		float_uncalNorm = (infoFrame.uncal_norm % 1000) >  0 ? (infoFrame.uncal_norm % 1000)
			: -(infoFrame.uncal_norm % 1000);

		sprintf(temp, "VERSION:%hd,ELAPSED_TIME_MIN:%hd,UPDATE_INDEX:%d,ACCURACY_INDEX:%d.%02d,",
		infoFrame.version, infoFrame.elapsed_time, infoFrame.updated_index,
		infoFrame.accuracy / 100, float_accuracy);

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
		sprintf(temp, "CALIBRATED_NORM:%d.%03d,UNCALIBRATED_NORM:%d.%03d;",
				infoFrame.cal_norm / 1000, float_calNorm, infoFrame.uncal_norm / 1000, float_uncalNorm);
		strcat(printCalData, temp);
	}

	accelCalDataIndx = -1;
	return sprintf(buf, "%s", printCalData);
}

void set_AccelCalibrationInfoData(char *pchRcvDataFrame, int *iDataIdx)
{
	if (accelCalDataIndx < (CALDATATOTALMAX - 1))
		memcpy(accelCalDataInfo[++accelCalDataIndx],  pchRcvDataFrame + *iDataIdx, CALDATAFIELDLENGTH);

	*iDataIdx += CALDATAFIELDLENGTH;
}

static DEVICE_ATTR(name, 0440, accel_name_show, NULL);
static DEVICE_ATTR(vendor, 0440, accel_vendor_show, NULL);
static DEVICE_ATTR(calibration, 0660,
	accel_calibration_show, accel_calibration_store);
static DEVICE_ATTR(raw_data, 0440, raw_data_read, NULL);
static DEVICE_ATTR(reactive_alert, 0660,
	accel_reactive_alert_show, accel_reactive_alert_store);
static DEVICE_ATTR(selftest, 0440, accel_hw_selftest_show, NULL);
static DEVICE_ATTR(lowpassfilter, 0220,
	NULL, accel_lowpassfilter_store);
static DEVICE_ATTR(dhr_sensor_info, 0440,	accel_scale_range_show, NULL);
static DEVICE_ATTR(calibration_info, 0440,	accel_calibration_info_show, NULL);

static struct device_attribute *acc_attrs[] = {
	&dev_attr_name,
	&dev_attr_vendor,
	&dev_attr_calibration,
	&dev_attr_raw_data,
	&dev_attr_reactive_alert,
	&dev_attr_selftest,
	&dev_attr_lowpassfilter,
	&dev_attr_dhr_sensor_info,
	&dev_attr_calibration_info,
	NULL,
};

void initialize_accel_factorytest(struct ssp_data *data)
{
    memset(&accel_manager, 0, sizeof(accel_manager));
	push_back(&accel_manager, "DEFAULT", &accel_default);
#ifdef CONFIG_SENSORS_ICM42632M
    push_back(&accel_manager, "ICM42632M", get_accel_icm42632m());
#endif
#ifdef CONFIG_SENSORS_LSM6DSO
    push_back(&accel_manager, "LSM6DSO", get_accel_lsm6dso());
#endif
	sensors_register(&data->acc_device, data, acc_attrs,
		"accelerometer_sensor");
}

void remove_accel_factorytest(struct ssp_data *data)
{
	sensors_unregister(data->acc_device, acc_attrs);
}