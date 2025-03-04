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

#define LIGHT_CAL_PARAM_FILE_PATH	"/efs/FactoryApp/gyro_cal_data"
#define LCD_PANEL_SVC_OCTA		"/sys/class/lcd/panel/SVC_OCTA"
#define LCD_PANEL_TYPE			"/sys/class/lcd/panel/lcd_type"

/*************************************************************************/
/* factory Sysfs                                                         */
/*************************************************************************/
static char *svc_octa_filp_name[2] = {LIGHT_CAL_PARAM_FILE_PATH, LCD_PANEL_SVC_OCTA};
static int svc_octa_filp_offset[2] = {SVC_OCTA_FILE_INDEX, 0};
static char svc_octa_data[2][SVC_OCTA_DATA_SIZE + 1] = { {0, }, };
static char lcd_type_flag = 0;

struct sensor_manager light_manager;
#define get_light(dev) ((light *)get_sensor_ptr(dev_get_drvdata(dev), &light_manager, LIGHT_SENSOR))

struct light_t light_default = {
	.name = " ",
	.vendor = " ",
	.get_light_circle = sensor_default_show
};

static ssize_t light_vendor_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	return sprintf(buf, "%s\n", get_light(dev)->vendor);
}

static ssize_t light_name_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	return sprintf(buf, "%s\n", get_light(dev)->name);
}

static ssize_t light_lux_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	struct ssp_data *data = dev_get_drvdata(dev);

	return sprintf(buf, "%u,%u,%u,%u,%u,%u\n",
		data->buf[UNCAL_LIGHT_SENSOR].light_t.r, data->buf[UNCAL_LIGHT_SENSOR].light_t.g,
		data->buf[UNCAL_LIGHT_SENSOR].light_t.b, data->buf[UNCAL_LIGHT_SENSOR].light_t.w,
		data->buf[UNCAL_LIGHT_SENSOR].light_t.a_time, data->buf[UNCAL_LIGHT_SENSOR].light_t.a_gain);
}

static ssize_t light_data_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	struct ssp_data *data = dev_get_drvdata(dev);

	return sprintf(buf, "%u,%u,%u,%u,%u,%u\n",
		data->buf[UNCAL_LIGHT_SENSOR].light_t.r, data->buf[UNCAL_LIGHT_SENSOR].light_t.g,
		data->buf[UNCAL_LIGHT_SENSOR].light_t.b, data->buf[UNCAL_LIGHT_SENSOR].light_t.w,
		data->buf[UNCAL_LIGHT_SENSOR].light_t.a_time, data->buf[UNCAL_LIGHT_SENSOR].light_t.a_gain);
}

static ssize_t light_coef_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	struct ssp_data *data = dev_get_drvdata(dev);
	int iRet, iReties = 0;
	struct ssp_msg *msg;
	int coef_buf[7];

	memset(coef_buf, 0, sizeof(int)*7);
retries:
	msg = kzalloc(sizeof(*msg), GFP_KERNEL);
	msg->cmd = MSG2SSP_AP_GET_LIGHT_COEF;
	msg->length = sizeof(coef_buf);
	msg->options = AP2HUB_READ;
	msg->buffer = (u8 *)coef_buf;
	msg->free_buffer = 0;

	iRet = ssp_spi_sync(data, msg, 1000);
	if (iRet != SUCCESS) {
		pr_err("[SSP] %s fail %d\n", __func__, iRet);

		if (iReties++ < 2) {
			pr_err("[SSP] %s fail, retry\n", __func__);
			mdelay(5);
			goto retries;
		}
		return FAIL;
	}

	pr_info("[SSP] %s - %d %d %d %d %d %d %d\n", __func__,
		coef_buf[0], coef_buf[1], coef_buf[2], coef_buf[3], coef_buf[4], coef_buf[5], coef_buf[6]);

	return snprintf(buf, PAGE_SIZE, "%d,%d,%d,%d,%d,%d,%d\n",
		coef_buf[0], coef_buf[1], coef_buf[2], coef_buf[3], coef_buf[4], coef_buf[5], coef_buf[6]);
}

static ssize_t light_sensorhub_ddi_spi_check_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
 	struct ssp_data *data = dev_get_drvdata(dev);
	int iRet = 0;
	int iReties = 0;
	struct ssp_msg *msg;
	short copr_buf = 0;

retries:
	msg = kzalloc(sizeof(*msg), GFP_KERNEL);
	msg->cmd = MSG2SSP_GET_DDI_COPR;
	msg->length = sizeof(copr_buf);
	msg->options = AP2HUB_READ;
	msg->buffer = (u8 *)&copr_buf;
	msg->free_buffer = 0;

	iRet = ssp_spi_sync(data, msg, 1000);
	if (iRet != SUCCESS) {
		pr_err("[SSP] %s fail %d\n", __func__, iRet);

		if (iReties++ < 2) {
			pr_err("[SSP] %s fail, retry\n", __func__);
			mdelay(5);
			goto retries;
		}
		return FAIL;
	}

	pr_info("[SSP] %s - %d\n", __func__, copr_buf);

	return snprintf(buf, PAGE_SIZE, "%d\n", copr_buf);
}

static ssize_t light_test_copr_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
 	struct ssp_data *data = dev_get_drvdata(dev);
	int iRet = 0;
	int iReties = 0;
	struct ssp_msg *msg;
	short copr_buf[4];

	memset(copr_buf, 0, sizeof(copr_buf));
retries:
	msg = kzalloc(sizeof(*msg), GFP_KERNEL);
	msg->cmd = MSG2SSP_GET_TEST_COPR;
	msg->length = sizeof(copr_buf);
	msg->options = AP2HUB_READ;
	msg->buffer = (u8 *)&copr_buf;
	msg->free_buffer = 0;

	iRet = ssp_spi_sync(data, msg, 1000);
	if (iRet != SUCCESS) {
		pr_err("[SSP] %s fail %d\n", __func__, iRet);

		if (iReties++ < 2) {
			pr_err("[SSP] %s fail, retry\n", __func__);
			mdelay(5);
			goto retries;
		}
		return FAIL;
	}

	pr_info("[SSP] %s - %d, %d, %d, %d\n", __func__,
		       	copr_buf[0], copr_buf[1], copr_buf[2], copr_buf[3]);

	return snprintf(buf, PAGE_SIZE, "%d, %d, %d, %d\n", 
			copr_buf[0], copr_buf[1], copr_buf[2], copr_buf[3]);
}

static ssize_t light_copr_roix_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{

 	struct ssp_data *data = dev_get_drvdata(dev);
	int iRet = 0;
	int iReties = 0;
	struct ssp_msg *msg;
	short copr_buf[12];

	memset(copr_buf, 0, sizeof(copr_buf));
retries:
	msg = kzalloc(sizeof(*msg), GFP_KERNEL);
	msg->cmd = MSG2SSP_GET_COPR_ROIX;
	msg->length = sizeof(copr_buf);
	msg->options = AP2HUB_READ;
	msg->buffer = (u8 *)&copr_buf;
	msg->free_buffer = 0;

	iRet = ssp_spi_sync(data, msg, 1000);
	if (iRet != SUCCESS) {
		pr_err("[SSP] %s fail %d\n", __func__, iRet);

		if (iReties++ < 2) {
			pr_err("[SSP] %s fail, retry\n", __func__);
			mdelay(5);
			goto retries;
		}
		return FAIL;
	}

	pr_info("[SSP] %s - %d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d\n", __func__,
		       	copr_buf[0], copr_buf[1], copr_buf[2], copr_buf[3],
			copr_buf[4], copr_buf[5], copr_buf[6], copr_buf[7],
			copr_buf[8], copr_buf[9], copr_buf[10], copr_buf[11]);

	return snprintf(buf, PAGE_SIZE, "%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d\n", 
			copr_buf[0], copr_buf[1], copr_buf[2], copr_buf[3],
			copr_buf[4], copr_buf[5], copr_buf[6], copr_buf[7],
			copr_buf[8], copr_buf[9], copr_buf[10], copr_buf[11]);
}


static ssize_t light_test_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
 	struct ssp_data *data = dev_get_drvdata(dev);
	int iRet = 0;
	int iReties = 0;
	struct ssp_msg *msg;
	u32 lux_buf[4] = {0, }; // [0]: 690us cal data, [1]: 16ms cal data, [2]: COPRW, [3}: 690us lux

retries:
	msg = kzalloc(sizeof(*msg), GFP_KERNEL);
	msg->cmd = MSG2SSP_GET_LIGHT_TEST;
	msg->length = sizeof(lux_buf);
	msg->options = AP2HUB_READ;
	msg->buffer = (u8 *)&lux_buf;
	msg->free_buffer = 0;

	iRet = ssp_spi_sync(data, msg, 1000);
	if (iRet != SUCCESS) {
		pr_err("[SSP] %s fail %d\n", __func__, iRet);

		if (iReties++ < 2) {
			pr_err("[SSP] %s fail, retry\n", __func__);
			mdelay(5);
			goto retries;
		}
		return FAIL;
	}

	pr_info("[SSP] %s - %d\n", __func__, lux_buf);

	return snprintf(buf, PAGE_SIZE, "%d, %d, %d, %d\n", lux_buf[0], lux_buf[1], lux_buf[2], lux_buf[3]);
}

static ssize_t light_circle_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	return get_light(dev)->get_light_circle(dev, buf);
}

static int load_light_cal_from_nvm(struct ssp_data *data, u32 *light_cal_data, int size) {
	file_manager_read(data, LIGHT_CAL_PARAM_FILE_PATH, (char *)light_cal_data, size, LIGHT_CAL_FILE_INDEX);
	return SUCCESS;
}
int set_lcd_panel_type_to_ssp(struct ssp_data *data)
{
	int iRet = 0;
	char lcd_type_data[256] = { 0, };
	struct ssp_msg *msg;

	file_manager_read(data, LIGHT_CAL_PARAM_FILE_PATH, (char *)lcd_type_data, sizeof(lcd_type_data), LIGHT_CAL_FILE_INDEX);

    msg = kzalloc(sizeof(*msg), GFP_KERNEL);
	msg->cmd = MSG2SSP_AP_SET_LCD_TYPE;
	msg->length = sizeof(lcd_type_flag);
	msg->options = AP2HUB_WRITE;
	msg->buffer = (u8 *)&lcd_type_flag;
	msg->free_buffer = 0;

	iRet = ssp_spi_async(data, msg);
	if (iRet != SUCCESS) {
		pr_err("[SSP] %s -fail to set. %d\n", __func__, iRet);
		iRet = ERROR;
	} 

	return iRet;
}

int set_light_cal_param_to_ssp(struct ssp_data *data)
{
	int iRet = 0, i = 0;
	u32 light_cal_data[2] = {0, };

	struct ssp_msg *msg;

	iRet = load_light_cal_from_nvm(data, light_cal_data, sizeof(light_cal_data));
	
	if (iRet != SUCCESS)
		return iRet;

	if (strcmp(svc_octa_data[0], svc_octa_data[1]) != 0) {
		pr_err("[SSP] %s - svc_octa_data, previous = %s, current = %s", __func__, svc_octa_data[0], svc_octa_data[1]);
		data->svc_octa_change = true;
		return -EIO;
	}

	// spec-out check. if true, we try to setting cal coef value to 1.
	for (i = 0; i < 2; i++) {
		if (light_cal_data[i] < 100 || light_cal_data[i] > 400)
			light_cal_data[i] = 0;
	}

	msg = kzalloc(sizeof(*msg), GFP_KERNEL);
	msg->cmd = MSG2SSP_AP_SET_LIGHT_CAL;
	msg->length = sizeof(light_cal_data);
	msg->options = AP2HUB_WRITE;
	msg->buffer = (u8 *)&light_cal_data;
	msg->free_buffer = 0;

	iRet = ssp_spi_async(data, msg);

	if (iRet != SUCCESS) {
		pr_err("[SSP] %s -fail to set. %d\n", __func__, iRet);
		iRet = ERROR;
	}

	return iRet;
}

static ssize_t light_cal_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	int iRet = 0;
	u32 light_cal_data[2] = {0, };
	struct ssp_data *data = dev_get_drvdata(dev);

	iRet = load_light_cal_from_nvm(data, light_cal_data, sizeof(light_cal_data));

	return sprintf(buf, "%d, %d,%d\n", iRet, light_cal_data[1], data->buf[UNCAL_LIGHT_SENSOR].light_t.lux);
}

static ssize_t light_cal_store(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t size)
{
	struct ssp_data *data = dev_get_drvdata(dev);
	int iRet = 0;
	u32 light_cal_data[2] = {0, };
	bool update = sysfs_streq(buf, "1");

	if (update) {
		struct ssp_msg *msg;

		msg = kzalloc(sizeof(*msg), GFP_KERNEL);
		msg->cmd = MSG2SSP_AP_GET_LIGHT_CAL;
		msg->length = sizeof(light_cal_data);
		msg->options = AP2HUB_READ;
		msg->buffer = (u8 *)&light_cal_data;
		msg->free_buffer = 0;

		iRet = ssp_spi_sync(data, msg, 1000);
		
		iRet = file_manager_write(data, LIGHT_CAL_PARAM_FILE_PATH, (char *)&svc_octa_data[1], SVC_OCTA_DATA_SIZE, SVC_OCTA_FILE_INDEX);
		memcpy(svc_octa_data[0], svc_octa_data[1], SVC_OCTA_DATA_SIZE);
		if (iRet != SVC_OCTA_DATA_SIZE) {
			pr_err("[SSP]: %s - Can't write svc_octa_data to file\n", __func__);
			iRet = -EIO;
		} else {
			pr_err("[SSP]: %s - svc_octa_data[1]: %s", __func__, svc_octa_data[1]);
		}
	}

	iRet = file_manager_write(data, LIGHT_CAL_PARAM_FILE_PATH, (char *)&light_cal_data, sizeof(light_cal_data), LIGHT_CAL_FILE_INDEX);

	if (iRet != sizeof(light_cal_data)) {
		pr_err("[SSP]: %s - Can't write light cal to file\n", __func__);
		iRet = -EIO;
	}

	return size;
}

int initialize_light_sensor(struct ssp_data *data){
	int iRet = 0, i = 0;

	for (i = 0; i < 2; i++) {
		file_manager_read(data, svc_octa_filp_name[i], (char *)svc_octa_data[i], SVC_OCTA_DATA_SIZE, svc_octa_filp_offset[i]);
		pr_err("[SSP]: %s - svc_octa_filp[%d]: %s", __func__, i, svc_octa_data[i]);
	}

	iRet = set_lcd_panel_type_to_ssp(data);
	if (iRet < 0)
		pr_err("[SSP]: %s - sending lcd type data failed\n", __func__);

	iRet = set_light_cal_param_to_ssp(data);
	if (iRet < 0)
		pr_err("[SSP]: %s - sending light calibration data failed\n", __func__);
	
	return iRet;
}


static DEVICE_ATTR(vendor, 0440, light_vendor_show, NULL);
static DEVICE_ATTR(name, 0440, light_name_show, NULL);
static DEVICE_ATTR(lux, 0440, light_lux_show, NULL);
static DEVICE_ATTR(raw_data, 0440, light_data_show, NULL);
static DEVICE_ATTR(coef, 0440, light_coef_show, NULL);
static DEVICE_ATTR(sensorhub_ddi_spi_check, 0440, light_sensorhub_ddi_spi_check_show, NULL);
static DEVICE_ATTR(test_copr, 0440, light_test_copr_show, NULL);
static DEVICE_ATTR(light_circle, 0440, light_circle_show, NULL);
static DEVICE_ATTR(copr_roix, 0440, light_copr_roix_show, NULL);
static DEVICE_ATTR(light_test, 0444, light_test_show, NULL);
static DEVICE_ATTR(light_cal, 0664, light_cal_show, light_cal_store);

static struct device_attribute *light_attrs[] = {
	&dev_attr_vendor,
	&dev_attr_name,
	&dev_attr_lux,
	&dev_attr_raw_data,
	&dev_attr_coef,
	&dev_attr_sensorhub_ddi_spi_check,
	&dev_attr_test_copr,
	&dev_attr_light_circle,
	&dev_attr_copr_roix,
	&dev_attr_light_cal,
	&dev_attr_light_test,	
	NULL,
};

void initialize_light_factorytest(struct ssp_data *data)
{
    memset(&light_manager, 0, sizeof(light_manager));
	push_back(&light_manager, "DEFAULT", &light_default);
#ifdef CONFIG_SENSORS_STK33910
    push_back(&light_manager, "STK33910", get_light_stk33910());
#endif
#ifdef CONFIG_SENSORS_TMD4907
    push_back(&light_manager, "TMD4907", get_light_tmd4907());
#endif
#ifdef CONFIG_SENSORS_TMD4912
    push_back(&light_manager, "TMD4912", get_light_tmd4912());
#endif
	sensors_register(&data->light_device, data, light_attrs, "light_sensor");
}

void remove_light_factorytest(struct ssp_data *data)
{
	sensors_unregister(data->light_device, light_attrs);
}
