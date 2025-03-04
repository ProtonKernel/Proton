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

/* accel range : 8g */
#define MAX_ACCEL_1G		(8192/2)
#define MAX_ACCEL_2G		(16384/2)
#define MIN_ACCEL_2G		(-16383/2)
#define MAX_ACCEL_4G		(32768/2)

static ssize_t accel_calibration_show(struct device *dev, char *buf)
{
	int iRet;
	int iCount = 0;
	struct ssp_data *data = dev_get_drvdata(dev);

	iRet = accel_open_calibration(data);
	if (iRet < 0)
		pr_err("[SSP]: %s - calibration open failed(%d)\n", __func__, iRet);

	ssp_dbg("[SSP] Cal data : %d %d %d - %d\n",
		data->accelcal.x, data->accelcal.y, data->accelcal.z, iRet);

	iCount = sprintf(buf, "%d %d %d %d\n", iRet, data->accelcal.x,
			data->accelcal.y, data->accelcal.z);
	return iCount;
}

static ssize_t accel_calibration_store(struct device *dev, const char *buf, size_t size)
{
	int iRet;
	int64_t dEnable;
	struct ssp_data *data = dev_get_drvdata(dev);

	iRet = kstrtoll(buf, 10, &dEnable);
	if (iRet < 0)
		return iRet;

	iRet = accel_do_calibrate(data, (int)dEnable, MAX_ACCEL_1G);
	if (iRet < 0)
		pr_err("[SSP]: %s - accel_do_calibrate() failed\n", __func__);

	return size;
}

static ssize_t accel_reactive_alert_store(struct device *dev, const char *buf, size_t size)
{
	int iRet = 0;
	char chTempBuf = 1;
	struct ssp_data *data = dev_get_drvdata(dev);

	struct ssp_msg *msg;

	if (sysfs_streq(buf, "1"))
		ssp_dbg("[SSP]: %s - on\n", __func__);
	else if (sysfs_streq(buf, "0"))
		ssp_dbg("[SSP]: %s - off\n", __func__);
	else if (sysfs_streq(buf, "2")) {
		ssp_dbg("[SSP]: %s - factory\n", __func__);

		data->bAccelAlert = 0;

		msg = kzalloc(sizeof(*msg), GFP_KERNEL);
		msg->cmd = ACCELEROMETER_FACTORY;
		msg->length = 1;
		msg->options = AP2HUB_READ;
		msg->data = chTempBuf;
		msg->buffer = &chTempBuf;
		msg->free_buffer = 0;

		iRet = ssp_spi_sync(data, msg, 3000);
		data->bAccelAlert = chTempBuf;

		if (iRet != SUCCESS) {
			pr_err("[SSP]: %s - accel Selftest Timeout!!\n", __func__);
			goto exit;
		}

		ssp_dbg("[SSP]: %s factory test success!\n", __func__);
	} else {
		pr_err("[SSP]: %s - invalid value %d\n", __func__, *buf);
		return -EINVAL;
	}
exit:
	return size;
}

static ssize_t accel_reactive_alert_show(struct device *dev, char *buf)
{
	bool bSuccess = false;
	struct ssp_data *data = dev_get_drvdata(dev);

	if (data->bAccelAlert == true)
		bSuccess = true;
	else
		bSuccess = false;

	data->bAccelAlert = false;
	return sprintf(buf, "%u\n", bSuccess);
}

static ssize_t accel_hw_selftest_show(struct device *dev, char *buf)
{
	char chTempBuf[14] = { 2, 0, };
	s8 init_status = 0, result = -1;
	s16 shift_ratio[3] = { 0, }, shift_ratio_N[3] = {0, };
	int iRet;
	struct ssp_data *data = dev_get_drvdata(dev);
	struct ssp_msg *msg;

	msg = kzalloc(sizeof(*msg), GFP_KERNEL);
	msg->cmd = ACCELEROMETER_FACTORY;
	msg->length = 14;
	msg->options = AP2HUB_READ;
	msg->data = chTempBuf[0];
	msg->buffer = chTempBuf;
	msg->free_buffer = 0;

	iRet = ssp_spi_sync(data, msg, 7000);
	if (iRet != SUCCESS) {
		pr_err("[SSP] %s - accel hw selftest Timeout!!\n", __func__);
		goto exit;
	}

	init_status = chTempBuf[0];
	shift_ratio[0] = (s16)((chTempBuf[2] << 8) + chTempBuf[1]);
	shift_ratio[1] = (s16)((chTempBuf[4] << 8) + chTempBuf[3]);
	shift_ratio[2] = (s16)((chTempBuf[6] << 8) + chTempBuf[5]);
	//negative Axis
	shift_ratio_N[0] = (s16)((chTempBuf[8] << 8) + chTempBuf[7]);
	shift_ratio_N[1] = (s16)((chTempBuf[10] << 8) + chTempBuf[9]);
	shift_ratio_N[2] = (s16)((chTempBuf[12] << 8) + chTempBuf[11]);
	result = chTempBuf[13];

	pr_info("[SSP] %s - %d, %d, %d, %d, %d, %d, %d, %d\n", __func__,
		init_status, result, shift_ratio[0], shift_ratio[1], shift_ratio[2],
		shift_ratio_N[0], shift_ratio_N[1], shift_ratio_N[2]);

	return sprintf(buf, "%d,%d,%d,%d,%d,%d,%d\n", result,
		shift_ratio[0],	shift_ratio[1],	shift_ratio[2],
		shift_ratio_N[0], shift_ratio_N[1], shift_ratio_N[2]);
exit:
	return sprintf(buf, "%d,%d,%d,%d\n", -5, 0, 0, 0);
}

static ssize_t accel_lowpassfilter_store(struct device *dev, const char *buf, size_t size)
{
	int iRet = 0, new_enable = 1;
	struct ssp_data *data = dev_get_drvdata(dev);
	struct ssp_msg *msg = kzalloc(sizeof(*msg), GFP_KERNEL);

	if (sysfs_streq(buf, "1"))
		new_enable = 1;
	else if (sysfs_streq(buf, "0"))
		new_enable = 0;
	else
		ssp_dbg("[SSP]: %s - invalid value!\n", __func__);

	msg->cmd = MSG2SSP_AP_SENSOR_LPF;
	msg->length = 1;
	msg->options = AP2HUB_WRITE;
	msg->buffer = kzalloc(1, GFP_KERNEL);

	*msg->buffer = new_enable;
	msg->free_buffer = 1;

	iRet = ssp_spi_async(data, msg);
	if (iRet != SUCCESS)
		pr_err("[SSP] %s - fail %d\n", __func__, iRet);
	else
		pr_info("[SSP] %s - %d\n", __func__, new_enable);

	return size;
}

struct accelerometer_t accel_lsm6dso = {
	.name = "LSM6DSO",
	.vendor = "STM",	
	.get_accel_calibration = accel_calibration_show,
	.set_accel_calibration = accel_calibration_store,
	.get_accel_reactive_alert = accel_reactive_alert_show,
	.set_accel_reactive_alert = accel_reactive_alert_store,
	.get_accel_selftest = accel_hw_selftest_show,
	.set_accel_lowpassfilter = accel_lowpassfilter_store
};

struct accelerometer_t* get_accel_lsm6dso(){
	return &accel_lsm6dso ;
}