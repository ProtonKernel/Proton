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

static ssize_t pressure_cabratioin_show(struct device *dev, char *buf)
{
	struct ssp_data *data = dev_get_drvdata(dev);

	pressure_open_calibration(data);

	return sprintf(buf, "%d\n", data->iPressureCal);
}

static ssize_t pressure_cabratioin_store(struct device *dev, const char *buf, size_t size)
{
	struct ssp_data *data = dev_get_drvdata(dev);
	int iPressureCal = 0, iErr = 0;

	iErr = kstrtoint(buf, 10, &iPressureCal);
	if (iErr < 0) {
		pr_err("[SSP]: %s - kstrtoint failed.(%d)\n", __func__, iErr);
		return iErr;
	}

	if (iPressureCal < PR_ABS_MIN || iPressureCal > PR_ABS_MAX)
		return -EINVAL;

	data->iPressureCal = (s32)iPressureCal;

	return size;
}

static ssize_t pressure_temperature_show(struct device *dev, char *buf)
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

struct barometer_t baro_lps22hhtr = {
	.name = "LPS22HHTR",
	.vendor = "STM",
	.get_baro_calibration = pressure_cabratioin_show,
	.set_baro_calibration = pressure_cabratioin_store,
	.get_baro_temperature = pressure_temperature_show
};

struct barometer_t* get_baro_lps22hhtr() {
	return &baro_lps22hhtr;
}