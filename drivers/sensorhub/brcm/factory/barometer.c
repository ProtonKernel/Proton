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

#define CALIBRATION_FILE_PATH		"/efs/FactoryApp/baro_delta"

/*************************************************************************/
/* factory Sysfs                                                         */
/*************************************************************************/

struct sensor_manager baro_manager;
#define get_baro(dev) ((baro *)get_sensor_ptr(dev_get_drvdata(dev), &baro_manager, PRESSURE_SENSOR))

struct barometer_t baro_default = {
	.name = " ",
	.vendor = " ",
	.get_baro_calibration = sensor_default_show,
	.set_baro_calibration = sensor_default_store,
	.get_baro_temperature = sensor_default_show
};

int pressure_open_calibration(struct ssp_data *data)
{
	char chBuf[10] = {0,};
	int iErr = 0;

	iErr = file_manager_read(data, CALIBRATION_FILE_PATH, chBuf, 10 * sizeof(char), 0);
	if (iErr < 0) {
		pr_err("[SSP]: %s - Can't read the cal data from file (%d)\n", __func__, iErr);
		return iErr;
	}

	iErr = kstrtoint(chBuf, 10, &data->iPressureCal);
	if (iErr < 0) {
		pr_err("[SSP]: %s - kstrtoint failed. %d\n", __func__, iErr);
		return iErr;
	}

	ssp_dbg("[SSP]: open barometer calibration %d\n", data->iPressureCal);

	if (data->iPressureCal < PR_ABS_MIN || data->iPressureCal > PR_ABS_MAX)
		pr_err("[SSP]: %s - wrong offset value!!!\n", __func__);

	return iErr;
}

static ssize_t sea_level_pressure_store(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t size)
{
	struct ssp_data *data = dev_get_drvdata(dev);
	int iRet;

	iRet = kstrtoint(buf, 10, &data->sealevelpressure);
	if (iRet < 0)
		return iRet;

	if (data->sealevelpressure == 0) {
		pr_info("%s, our->temperature = 0\n", __func__);
		data->sealevelpressure = -1;
	}

	pr_info("[SSP] %s sea_level_pressure = %d\n",
		__func__, data->sealevelpressure);
	return size;
}

static ssize_t pressure_cabratioin_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	return get_baro(dev)->get_baro_calibration(dev, buf);
}

static ssize_t pressure_cabratioin_store(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t size)
{
	return get_baro(dev)->set_baro_calibration(dev, buf, size);
}

static ssize_t pressure_selftest_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	struct ssp_data *data = dev_get_drvdata(dev);
	struct ssp_msg *msg = kzalloc(sizeof(*msg), GFP_KERNEL);

	int iRet = 0;
	char selftest_ret = 0;

	msg->cmd = PRESSURE_FACTORY;
	msg->length = sizeof(selftest_ret);
	msg->options = AP2HUB_READ;
	msg->buffer = (u8 *)&selftest_ret;
	msg->free_buffer = 0;

	iRet = ssp_spi_sync(data, msg, 3000);

	if (iRet != SUCCESS)
		pr_err("[SSP]: %s - Pressure Selftest Timeout!!\n", __func__);

	pr_info("[SSP] %s - selftest - %d\n", __func__, (int)selftest_ret);

	return snprintf(buf, PAGE_SIZE, "%d\n", (int)selftest_ret);
}

static ssize_t pressure_name_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	return sprintf(buf, "%s\n", get_baro(dev)->name);
}

static ssize_t pressure_vendor_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	return sprintf(buf, "%s\n", get_baro(dev)->vendor);
}

static ssize_t pressure_temperature_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	struct ssp_data *data = dev_get_drvdata(dev);
	s32 temperature = 0;
	s32 float_temperature = 0;
	s32 temp = 0;

	temp = (s32) (data->buf[PRESSURE_SENSOR].temperature);
	temperature = (4250) + ((temp / (120 * 4))*100); //(42.5f) + (temperature/(120 * 4));
	float_temperature = ((temperature%100) > 0 ? (temperature%100) : -(temperature%100));

	return sprintf(buf, "%d.%02d\n", (temperature/100), float_temperature);
}

static DEVICE_ATTR(vendor,  0444, pressure_vendor_show, NULL);
static DEVICE_ATTR(name,  0444, pressure_name_show, NULL);
static DEVICE_ATTR(calibration,  0664,
	pressure_cabratioin_show, pressure_cabratioin_store);
static DEVICE_ATTR(sea_level_pressure, /*S_IRUGO |*/ 0220,
	NULL, sea_level_pressure_store);
static DEVICE_ATTR(temperature, 0444, pressure_temperature_show, NULL);
static DEVICE_ATTR(selftest,  0440, pressure_selftest_show, NULL);

static struct device_attribute *pressure_attrs[] = {
	&dev_attr_vendor,
	&dev_attr_name,
	&dev_attr_calibration,
	&dev_attr_sea_level_pressure,
	&dev_attr_temperature,
	&dev_attr_selftest,
	NULL,
};

void initialize_pressure_factorytest(struct ssp_data *data)
{
	memset(&baro_manager, 0, sizeof(baro_manager));
	push_back(&baro_manager, "DEFAULT", &baro_default);
#ifdef CONFIG_SENSORS_LPS22H
	push_back(&baro_manager, "LPS22HH", get_baro_lps22hhtr());
#endif
	sensors_register(&data->prs_device, data, pressure_attrs, "barometer_sensor");
}

void remove_pressure_factorytest(struct ssp_data *data)
{
	sensors_unregister(data->prs_device, pressure_attrs);
}