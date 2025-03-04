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

#define	VENDOR		"AMS"
#if defined(CONFIG_SENSORS_SSP_TMG399x)
#define	CHIP_ID		"TMG399X"
#elif defined(CONFIG_SENSORS_SSP_TMD3782)
#define CHIP_ID		"TMD3782"
#elif defined(CONFIG_SENSORS_SSP_TMD4903)
#define CHIP_ID		"TMD4903"
#elif defined(CONFIG_SENSORS_SSP_TMD4904)
#define CHIP_ID		"TMD4904"
#elif defined(CONFIG_SENSORS_SSP_TMD4905)
#define CHIP_ID		"TMD4905"
#elif defined(CONFIG_SENSORS_SSP_TMD4906)
#define CHIP_ID		"TMD4906"
#elif defined(CONFIG_SENSORS_SSP_TMD4907)
#define CHIP_ID		"TMD4907"
#elif defined(CONFIG_SENSORS_SSP_TMD4910)
#define CHIP_ID		"TMD4910"
#else
#define CHIP_ID		"UNKNOWN"
#endif

#define CONFIG_PANEL_NOTIFY	1

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

static ssize_t light_vendor_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	return sprintf(buf, "%s\n", VENDOR);
}

static ssize_t light_name_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	struct ssp_data *data = dev_get_drvdata(dev);

	pr_info("[SSP] %s :: sensor(%d) : %s", __func__, LIGHT_SENSOR, data->sensor_name[LIGHT_SENSOR]);
	
	return sprintf(buf, "%s\n", data->sensor_name[LIGHT_SENSOR][0] == 0 ? 
			CHIP_ID : data->sensor_name[LIGHT_SENSOR]);
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
	msg->length = 28;
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

#ifdef CONFIG_PANEL_NOTIFY
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
	struct ssp_data *data = dev_get_drvdata(dev);

	switch(data->ap_type) {
#if defined(CONFIG_SENSORS_SSP_CANVAS)
		case 0:
			return snprintf(buf, PAGE_SIZE, "42.1 7.8 2.4\n");
		case 1:
			return snprintf(buf, PAGE_SIZE, "45.2 7.3 2.4\n");
#elif defined(CONFIG_SENSORS_SSP_PICASSO)
		case 0:
			return snprintf(buf, PAGE_SIZE, "45.1 8.2 2.4\n");
		case 1:
			return snprintf(buf, PAGE_SIZE, "42.6 8.0 2.4\n");
		case 2:
			return snprintf(buf, PAGE_SIZE, "44.0 8.0 2.4\n");
#endif
		default:
			return snprintf(buf, PAGE_SIZE, "0.0 0.0 0.0\n");
	}

	return snprintf(buf, PAGE_SIZE, "0.0 0.0 0.0\n");
}
#endif

#ifdef CONFIG_SENSORS_SSP_PROX_SETTING
static ssize_t light_coef_store(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t size)
{
	int iRet, i;
	int coef[7];
	char *token;
	char *str;
	struct ssp_data *data = dev_get_drvdata(dev);

	pr_info("[SSP] %s - %s\n", __func__, buf);

	//parsing
	str = (char *)buf;
	for (i = 0; i < 7; i++) {
		token = strsep(&str, "\n");
		if (token == NULL) {
			pr_err("[SSP] %s : too few arguments (7 needed)", __func__);
				return -EINVAL;
		}

		iRet = kstrtos32(token, 10, &coef[i]);
		if (iRet < 0) {
			pr_err("[SSP] %s : kstrtou8 error %d", __func__, iRet);
			return iRet;
		}
	}

	memcpy(data->light_coef, coef, sizeof(data->light_coef));

	set_light_coef(data);

	return size;
}
#endif

static int load_light_cal_from_nvm(u32 *light_cal_data, int size) {
	int iRet = 0;
	mm_segment_t old_fs;
	struct file *cal_filp = NULL;

	old_fs = get_fs();
	set_fs(KERNEL_DS);

	cal_filp = filp_open(LIGHT_CAL_PARAM_FILE_PATH, O_CREAT | O_RDONLY | O_NOFOLLOW | O_NONBLOCK, 0660);
	if (IS_ERR(cal_filp)) {
		pr_err("[SSP] %s: filp_open failed\n", __func__);
		set_fs(old_fs);
		iRet = PTR_ERR(cal_filp);

		return iRet;
	}

	cal_filp->f_pos = LIGHT_CAL_FILE_INDEX; // gyro_cal : 12 bytes , mag_cal : 7 / 13 bytes

	iRet = vfs_read(cal_filp, (char *)light_cal_data, size, &cal_filp->f_pos);

	if (iRet != size) {
		pr_err("[SSP] %s: filp_open read failed, read size = %d", __func__, iRet);
		iRet = -EIO;
	} else {
		pr_info("[SSP] %s, val[0] = %d, val[1] = %d", __func__, light_cal_data[0], light_cal_data[1]);
		iRet = light_cal_data[0] == 0xFFFFFFFF ? -1 : 1; // 0xFFFFFFFF for NG
	}

	filp_close(cal_filp, current->files);
	set_fs(old_fs);

	return iRet;
}
int set_lcd_panel_type_to_ssp(struct ssp_data *data)
{
	int iRet = 0;
	char lcd_type_data[256] = { 0, };
	mm_segment_t old_fs;
	struct file *lcd_type_filp = NULL;
	struct ssp_msg *msg;

	old_fs = get_fs();
	set_fs(KERNEL_DS);

	lcd_type_filp = filp_open(LCD_PANEL_TYPE, O_RDONLY, 0444);

	if (IS_ERR(lcd_type_filp)) {
		iRet = PTR_ERR(lcd_type_filp);
		pr_err("[SSP]: %s - Can't open lcd_type file err(%d)\n", __func__, iRet);
		return iRet;
	}

	iRet = vfs_read(lcd_type_filp, (char *)lcd_type_data, sizeof(lcd_type_data), &lcd_type_filp->f_pos);

	if(iRet > 0) {
		/* lcd_type_data[iRet - 2], which type have different transmission ratio.
		 * [0 ~ 2] : 0.7%, [3] : 15%, [4] : 40%
		*/
		if(lcd_type_data[iRet - 2] >= '0' && lcd_type_data[iRet - 2] <= '2')
			lcd_type_flag = 0;
		else if(lcd_type_data[iRet - 2] == '3')
			lcd_type_flag = 1;
		else
			lcd_type_flag = 2;
	}	

	pr_info("[SSP]: %s - lcd_type_data: %s(%d) flag: %d", __func__, lcd_type_data, iRet, lcd_type_flag);

	filp_close(lcd_type_filp, current->files);
	set_fs(old_fs);

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

	iRet = load_light_cal_from_nvm(light_cal_data, sizeof(light_cal_data));
	
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

	iRet = load_light_cal_from_nvm(light_cal_data, sizeof(light_cal_data));

	if (lcd_type_flag == 0)
		return sprintf(buf, "%d, %d,%d\n", iRet, light_cal_data[0], data->buf[UNCAL_LIGHT_SENSOR].light_t.lux);
	else
		return sprintf(buf, "%d, %d,%d\n", iRet, light_cal_data[1], data->buf[UNCAL_LIGHT_SENSOR].light_t.lux);
}

static ssize_t light_cal_store(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t size)
{
	int iRet = 0;
	struct file *cal_filp = NULL;
	mm_segment_t old_fs;
	u32 light_cal_data[2] = {0, };
	bool update = sysfs_streq(buf, "1");

	old_fs = get_fs();
	set_fs(KERNEL_DS);

	cal_filp = filp_open(LIGHT_CAL_PARAM_FILE_PATH, O_CREAT | O_RDWR | O_NOFOLLOW | O_NONBLOCK, 0660);

	if (IS_ERR(cal_filp)) {
		iRet = PTR_ERR(cal_filp);
		pr_err("[SSP]: %s - Can't open light cal file err(%d)\n", __func__, iRet);
		goto exit_light_cal_store;
	}

	if (update) {
 		struct ssp_data *data = dev_get_drvdata(dev);
		struct ssp_msg *msg;

		msg = kzalloc(sizeof(*msg), GFP_KERNEL);
		msg->cmd = MSG2SSP_AP_GET_LIGHT_CAL;
		msg->length = sizeof(light_cal_data);
		msg->options = AP2HUB_READ;
		msg->buffer = (u8 *)&light_cal_data;
		msg->free_buffer = 0;

		iRet = ssp_spi_sync(data, msg, 1000);
		
		cal_filp->f_pos = SVC_OCTA_FILE_INDEX;
		iRet = vfs_write(cal_filp, (char *)&svc_octa_data[1], SVC_OCTA_DATA_SIZE, &cal_filp->f_pos);
		memcpy(svc_octa_data[0], svc_octa_data[1], SVC_OCTA_DATA_SIZE);

		if (iRet != SVC_OCTA_DATA_SIZE) {
			pr_err("[SSP]: %s - Can't write svc_octa_data to file\n", __func__);
			iRet = -EIO;
		} else {
			pr_err("[SSP]: %s - svc_octa_data[1]: %s", __func__, svc_octa_data[1]);
		}

	}

	cal_filp->f_pos = LIGHT_CAL_FILE_INDEX; // gyro_cal : 12 bytes , mag_cal : 7 / 13 bytes

	iRet = vfs_write(cal_filp, (char *)&light_cal_data, sizeof(light_cal_data), &cal_filp->f_pos);

	if (iRet != sizeof(light_cal_data)) {
		pr_err("[SSP]: %s - Can't write light cal to file\n", __func__);
		iRet = -EIO;
	}

	filp_close(cal_filp, current->files);

exit_light_cal_store:
	set_fs(old_fs);

	return iRet;
}

int initialize_light_sensor(struct ssp_data *data){
	int iRet = 0, i = 0;
	struct file *svc_octa_filp[2];
	mm_segment_t old_fs;

	old_fs = get_fs();
	set_fs(KERNEL_DS);

	for (i = 0; i < 2; i++) {
		svc_octa_filp[i] = filp_open(svc_octa_filp_name[i], O_RDONLY, 0444);
		if (IS_ERR(svc_octa_filp[i])) {
			pr_err("[SSP]: %s - Can't open svc_octa_filp[%d], errno = (%d)\n", __func__, i, PTR_ERR(svc_octa_filp[i]));
			continue;
		}
		svc_octa_filp[i]->f_pos = svc_octa_filp_offset[i];
		iRet = vfs_read(svc_octa_filp[i], (char *)svc_octa_data[i], SVC_OCTA_DATA_SIZE, &svc_octa_filp[i]->f_pos);

		pr_err("[SSP]: %s - svc_octa_filp[%d]: %s", __func__, i, svc_octa_data[i]);  
		filp_close(svc_octa_filp[i], current->files);
	}
	set_fs(old_fs);

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

#ifdef CONFIG_SENSORS_SSP_PROX_SETTING
static DEVICE_ATTR(coef, 0660, light_coef_show, light_coef_store);
#else
static DEVICE_ATTR(coef, 0440, light_coef_show, NULL);
#endif

#ifdef CONFIG_PANEL_NOTIFY
static DEVICE_ATTR(sensorhub_ddi_spi_check, 0440, light_sensorhub_ddi_spi_check_show, NULL);
static DEVICE_ATTR(test_copr, 0440, light_test_copr_show, NULL);
static DEVICE_ATTR(light_circle, 0440, light_circle_show, NULL);
static DEVICE_ATTR(copr_roix, 0440, light_copr_roix_show, NULL);
static DEVICE_ATTR(light_test, 0444, light_test_show, NULL);
#endif

static DEVICE_ATTR(light_cal, 0664, light_cal_show, light_cal_store);

static struct device_attribute *light_attrs[] = {
	&dev_attr_vendor,
	&dev_attr_name,
	&dev_attr_lux,
	&dev_attr_raw_data,
	&dev_attr_coef,
#ifdef CONFIG_PANEL_NOTIFY
	&dev_attr_sensorhub_ddi_spi_check,
	&dev_attr_test_copr,
	&dev_attr_light_circle,
	&dev_attr_copr_roix,
#endif
	&dev_attr_light_cal,
	&dev_attr_light_test,	
	NULL,
};

void initialize_light_factorytest(struct ssp_data *data)
{
	sensors_register(&data->light_device, data, light_attrs, "light_sensor");
}

void remove_light_factorytest(struct ssp_data *data)
{
	sensors_unregister(data->light_device, light_attrs);
}
