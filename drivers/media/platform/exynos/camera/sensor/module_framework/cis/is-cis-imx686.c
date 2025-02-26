/*
 * Samsung Exynos5 SoC series Sensor driver
 *
 *
 * Copyright (c) 2019 Samsung Electronics Co., Ltd
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#include <linux/i2c.h>
#include <linux/slab.h>
#include <linux/irq.h>
#include <linux/interrupt.h>
#include <linux/delay.h>
#include <linux/version.h>
#include <linux/gpio.h>
#include <linux/clk.h>
#include <linux/regulator/consumer.h>
#include <linux/videodev2.h>
#include <linux/videodev2_exynos_camera.h>
#include <linux/module.h>
#include <linux/moduleparam.h>
#include <linux/platform_device.h>
#include <linux/of_gpio.h>
#include <linux/syscalls.h>
#include <media/v4l2-ctrls.h>
#include <media/v4l2-device.h>
#include <media/v4l2-subdev.h>

#include <exynos-is-sensor.h>
#include "is-hw.h"
#include "is-core.h"
#include "is-param.h"
#include "is-device-sensor.h"
#include "is-device-sensor-peri.h"
#include "is-resourcemgr.h"
#include "is-dt.h"
#include "is-cis-imx686.h"
#include "is-cis-imx686-setA.h"
#include "is-cis-imx686-setB.h"

#include "is-helper-i2c.h"
#ifdef CONFIG_CAMERA_VENDER_MCD_V2
#include "is-sec-define.h"
#endif

#include "interface/is-interface-library.h"

#define SENSOR_NAME "IMX686"

static const struct v4l2_subdev_ops subdev_ops;

static u8 coarse_integ_step_value;
static bool sensor_imx686_cal_write_flag;

static const u32 *sensor_imx686_global;
static u32 sensor_imx686_global_size;
static const u32 **sensor_imx686_setfiles;
static const u32 *sensor_imx686_setfile_sizes;
static u32 sensor_imx686_max_setfile_num;
static const struct sensor_pll_info_compact **sensor_imx686_pllinfos;

#ifdef CONFIG_CAMERA_VENDER_MCD_V2
extern struct is_lib_support gPtr_lib_support;
#endif

static void sensor_imx686_set_coarse_integration_max_margin(u32 mode, cis_shared_data *cis_data)
{
	FIMC_BUG_VOID(!cis_data);

	cis_data->max_margin_coarse_integration_time = SENSOR_IMX686_COARSE_INTEGRATION_TIME_MAX_MARGIN;
	dbg_sensor(1, "max_margin_coarse_integration_time(%d)\n",
		cis_data->max_margin_coarse_integration_time);
}

static void sensor_imx686_set_coarse_integration_min(u32 mode, cis_shared_data *cis_data)
{
	FIMC_BUG_VOID(!cis_data);

	switch (mode) {
		case SENSOR_IMX686_2X2BIN_4624X3468_30FPS:
		case SENSOR_IMX686_2X2BIN_4624X2604_30FPS:
		case SENSOR_IMX686_2X2BIN_4624X2604_60FPS:
			cis_data->min_coarse_integration_time = SENSOR_IMX686_COARSE_INTEGRATION_TIME_MIN_FOR_PDAF;
			dbg_sensor(1, "min_coarse_integration_time(%d)\n",
				cis_data->min_coarse_integration_time);
			break;
		case SENSOR_IMX686_V2H2_2304X1269_240FPS:
		case SENSOR_IMX686_V2H2_2304X1728_120FPS:
			cis_data->min_coarse_integration_time = SENSOR_IMX686_COARSE_INTEGRATION_TIME_MIN_FOR_V2H2;
			dbg_sensor(1, "min_coarse_integration_time(%d)\n",
				cis_data->min_coarse_integration_time);
			break;
		case SENSOR_IMX686_REMOSAIC_FULL_9248X6936_20FPS:
		case SENSOR_IMX686_REMOSAIC_CROP_4624X3468_30FPS:
		case SENSOR_IMX686_REMOSAIC_CROP_4624X2604_30FPS:
		case SENSOR_IMX686_REMOSAIC_CROP_4624X2604_60FPS:
			cis_data->min_coarse_integration_time = SENSOR_IMX686_COARSE_INTEGRATION_TIME_MIN;
			dbg_sensor(1, "min_coarse_integration_time(%d)\n",
				cis_data->min_coarse_integration_time);
			break;
		default:
			err("[%s] Unsupport imx686 sensor mode\n", __func__);
			cis_data->min_coarse_integration_time = SENSOR_IMX686_COARSE_INTEGRATION_TIME_MIN_FOR_V2H2;
			dbg_sensor(1, "min_coarse_integration_time(%d)\n",
				cis_data->min_coarse_integration_time);
			break;
	}
}

static void sensor_imx686_set_coarse_integration_step_value(u32 mode)
{
	switch (mode) {
		case SENSOR_IMX686_2X2BIN_4624X3468_30FPS:
		case SENSOR_IMX686_2X2BIN_4624X2604_30FPS:
		case SENSOR_IMX686_2X2BIN_4624X2604_60FPS:
		case SENSOR_IMX686_V2H2_2304X1269_240FPS:
		case SENSOR_IMX686_V2H2_2304X1728_120FPS:
			coarse_integ_step_value = 2;
			dbg_sensor(1, "coarse_integration step value(%d)\n",
				coarse_integ_step_value);
			break;
		case SENSOR_IMX686_REMOSAIC_FULL_9248X6936_20FPS:
		case SENSOR_IMX686_REMOSAIC_CROP_4624X3468_30FPS:
		case SENSOR_IMX686_REMOSAIC_CROP_4624X2604_30FPS:
		case SENSOR_IMX686_REMOSAIC_CROP_4624X2604_60FPS:
			coarse_integ_step_value = 1;
			dbg_sensor(1, "coarse_integration step value(%d)\n",
				coarse_integ_step_value);
			break;
		default:
			err("[%s] Unsupport imx686 sensor mode\n", __func__);
			coarse_integ_step_value = 2;
			dbg_sensor(1, "coarse_integration step value(%d)\n",
				coarse_integ_step_value);
			break;
	}
}

static int sensor_imx686_get_fine_integration_value(struct is_cis *cis, u16 *fine_integ)
{
	int ret = 0;
	u16 fine_integ_time = 0;
	struct i2c_client *client;

	FIMC_BUG(!cis);
	FIMC_BUG(!cis->cis_data);
	FIMC_BUG(!fine_integ);

	client = cis->client;
	if (unlikely(!client)) {
		err("client is NULL");
		ret = -EINVAL;
		goto p_err;
	}

	I2C_MUTEX_LOCK(cis->i2c_lock);

	ret = is_sensor_read16(client, SENSOR_IMX686_FINE_INTEG_TIME_ADDR, &fine_integ_time);
	if (ret < 0) {
		err("is_sensor_read is fail, (ret %d)", ret);
		ret = -EAGAIN;
		goto i2c_err;
	}

	*fine_integ = fine_integ_time;

	pr_info("%s : fine_integ_time. (%d)\n", __func__, *fine_integ);

i2c_err:
	I2C_MUTEX_UNLOCK(cis->i2c_lock);

p_err:
	return ret;
}

#if SENSOR_IMX686_EBD_LINE_DISABLE
static int sensor_imx686_set_ebd(struct is_cis *cis)
{
	int ret = 0;
	u8 enabled_ebd = 0;
	struct i2c_client *client;

	FIMC_BUG(!cis);
	FIMC_BUG(!cis->cis_data);

	client = cis->client;
	if (unlikely(!client)) {
		err("client is NULL");
		ret = -EINVAL;
		goto p_err;
	}

#if SENSOR_IMX686_DEBUG_INFO
	ret = is_sensor_read8(client, SENSOR_IMX686_EBD_CONTROL_ADDR, &enabled_ebd);
	if (ret < 0) {
		err("is_sensor_read is fail, (ret %d)", ret);
		ret = -EAGAIN;
		goto p_err;
	} else {
		pr_info("%s : before set, EBD Control = %d\n", __func__, enabled_ebd);
	}
#endif

	enabled_ebd = 0;	//default disabled
	ret = is_sensor_write8(client, SENSOR_IMX686_EBD_CONTROL_ADDR, enabled_ebd);
	if (ret < 0) {
		err("is_sensor_read is fail, (ret %d)", ret);
		ret = -EAGAIN;
		goto p_err;
	}

#if SENSOR_IMX686_DEBUG_INFO
	ret = is_sensor_read8(client, SENSOR_IMX686_EBD_CONTROL_ADDR, &enabled_ebd);
	if (ret < 0) {
		err("is_sensor_read is fail, (ret %d)", ret);
		ret = -EAGAIN;
		goto p_err;
	} else {
		pr_info("%s : after set,  EBD Control = %d\n", __func__, enabled_ebd);
	}
#endif

p_err:
	return ret;
}
#endif

/*************************************************
 *  [IMX686 Analog gain formular]
 *
 *  m0: [0x008c:0x008d] fixed to 0
 *  m1: [0x0090:0x0091] fixed to -1
 *  c0: [0x008e:0x008f] fixed to 1024
 *  c1: [0x0092:0x0093] fixed to 1024
 *  X : [0x0204:0x0205] Analog gain setting value
 *
 *  Analog Gain = (m0 * X + c0) / (m1 * X + c1)
 *              = 1024 / (1024 - X)
 *
 *  Analog Gain Range = 0 to 1008 (Tetra: x1 to x36 / Remosaic&HDR: x1 to x24)
 *
 *************************************************/

u32 sensor_imx686_cis_calc_again_code(u32 permille)
{
	return 1024 - (1024000 / permille);
}

u32 sensor_imx686_cis_calc_again_permile(u32 code)
{
	return 1024000 / (1024 - code);
}

u32 sensor_imx686_cis_calc_dgain_code(u32 permile)
{
	u8 buf[2] = {0, 0};
	buf[0] = (permile / 1000) & 0x0F;
	buf[1] = (((permile - (buf[0] * 1000)) * 256) / 1000);

	return (buf[0] << 8 | buf[1]);
}

u32 sensor_imx686_cis_calc_dgain_permile(u32 code)
{
	return (((code & 0x0F00) >> 8) * 1000) + ((code & 0x00FF) * 1000 / 256);
}

static void sensor_imx686_cis_data_calculation(const struct sensor_pll_info_compact *pll_info, cis_shared_data *cis_data)
{
	u64 pixel_rate;
	u32 frame_rate, max_fps, frame_valid_us;

	FIMC_BUG_VOID(!pll_info);

	/* 1. get pclk value from pll info */
	pixel_rate = pll_info->pclk * TOTAL_NUM_OF_IVTPX_CHANNEL;

	/* 2. FPS calcurate */
	frame_rate = pixel_rate  / total_pixels;

	/* 3. the time of processing one frame calcuration (us) */
	cis_data->min_frame_us_time = (1 * 1000 * 1000) / frame_rate;
	cis_data->cur_frame_us_time = cis_data->min_frame_us_time;

	dbg_sensor(1, "[imx686 data_calcurate]frame_duration(%d) : frame_rate (%d) = "
		KERN_CONT " pixel_rate(%llu) / (pll_info->frame_length_lines(%d) * pll_info->line_length_pck(%d))\n",
		cis_data->min_frame_us_time, frame_rate, pixel_rate, pll_info->frame_length_lines, pll_info->line_length_pck);

	/* calcurate max fps */
	max_fps = (pixel_rate * 10) / (pll_info->frame_length_lines * pll_info->line_length_pck);
	max_fps = (max_fps % 10 >= 5 ? frame_rate + 1 : frame_rate);

	cis_data->pclk = (u32)pixel_rate;
	cis_data->max_fps = max_fps;
	cis_data->frame_length_lines = pll_info->frame_length_lines;
	cis_data->line_length_pck = pll_info->line_length_pck;
	cis_data->line_readOut_time = (u64)cis_data->line_length_pck * 1000
					* 1000 * 1000 / cis_data->pclk);
	cis_data->rolling_shutter_skew = (cis_data->cur_height - 1) * cis_data->line_readOut_time;
	cis_data->stream_on = false;

	/* Frame valid time calcuration */
	frame_valid_us = (u64)cis_data->cur_height * cis_data->line_length_pck
				* 1000 * 1000 / cis_data->pclk;
	cis_data->frame_valid_us_time = (int)frame_valid_us;

	dbg_sensor(1, "%s\n", __func__);
	dbg_sensor(1, "[imx686 data_calcurate]Sensor size(%d x %d) setting: SUCCESS!\n", cis_data->cur_width, cis_data->cur_height);
	dbg_sensor(1, "[imx686 data_calcurate]Frame Valid(us): %d\n", frame_valid_us);
	dbg_sensor(1, "[imx686 data_calcurate]rolling_shutter_skew: %lld\n", cis_data->rolling_shutter_skew);

	dbg_sensor(1, "[imx686 data_calcurate]Fps: %d, max fps(%d)\n", frame_rate, cis_data->max_fps);
	dbg_sensor(1, "[imx686 data_calcurate]min_frame_time(%d us)\n", cis_data->min_frame_us_time);
	dbg_sensor(1, "[imx686 data_calcurate]Pixel rate(Kbps): %d\n", cis_data->pclk / 1000);

	/* Frame period calculation */
	cis_data->frame_time = (cis_data->line_readOut_time * cis_data->cur_height / 1000);

	dbg_sensor(1, "[imx686 data_calcurate] frame_time(%d), rolling_shutter_skew(%lld)\n", __func__,
	cis_data->frame_time, cis_data->rolling_shutter_skew);

	info("%s: done", __func__);
}

#if SENSOR_IMX686_CAL_DEBUG
int sensor_imx686_cis_cal_dump(char* name, char *buf, size_t size)
{
	int ret = 0;
#ifdef USE_KERNEL_VFS_READ_WRITE
	struct file *fp;

	fp = filp_open(name, O_WRONLY | O_CREAT | O_TRUNC | O_SYNC, 0666);
	if (IS_ERR_OR_NULL(fp)) {
		ret = PTR_ERR(fp);
		err("%s(): open file error(%s), error(%d)\n", __func__, name, ret);
		goto p_err;
	}

	ret = kernel_write(fp, buf, size, &fp->f_pos);
	if (ret < 0) {
		err("%s(): file write fail(%s) ret(%d)", __func__,
				name, ret);
		goto p_err;
	}

p_err:
	if (!IS_ERR_OR_NULL(fp))
		filp_close(fp, NULL);

#else
	err("not support %s due to kernel_write!", __func__);
	ret = -EINVAL;
#endif
	return ret;
}
#endif

#ifdef SENSOR_IMX686_WRITE_PDAF_CAL
int sensor_imx686_cis_LRC_write(struct v4l2_subdev *subdev)
{
	int ret = 0;
	struct is_cis *cis;
	struct i2c_client *client = NULL;
	struct is_device_sensor_peri *sensor_peri = NULL;

	int position;
	u16 start_addr;
	u16 data_size;

	ulong cal_addr;
	u8 cal_data[SENSOR_IMX686_LRC_CAL_SIZE] = {0, };

#ifdef CONFIG_CAMERA_VENDER_MCD_V2
	char *rom_cal_buf = NULL;
#else
	struct is_lib_support *lib = &gPtr_lib_support;
#endif

	FIMC_BUG(!subdev);

	cis = (struct is_cis *)v4l2_get_subdevdata(subdev);
	FIMC_BUG(!cis);
	FIMC_BUG(!cis->cis_data);

	sensor_peri = container_of(cis, struct is_device_sensor_peri, cis);
	FIMC_BUG(!sensor_peri);

	client = cis->client;
	if (unlikely(!client)) {
		err("client is NULL");
		return -EINVAL;
	}

	position = sensor_peri->module->position;

#ifdef CONFIG_CAMERA_VENDER_MCD_V2
	ret = is_sec_get_cal_buf(position, &rom_cal_buf);

	if (ret < 0) {
		goto p_err;
	}

	cal_addr = (ulong)rom_cal_buf;
	if (position == SENSOR_POSITION_REAR) {
		cal_addr += SENSOR_IMX686_LRC_CAL_BASE_REAR;
	} else {
		err("cis_imx686 position(%d) is invalid!\n", position);
		goto p_err;
	}
#else
	if (position == SENSOR_POSITION_REAR){
		cal_addr = lib->minfo->kvaddr_cal[position] + SENSOR_IMX686_LRC_CAL_BASE_REAR;
	}else {
		err("cis_imx686 position(%d) is invalid!\n", position);
		goto p_err;
	}
#endif

	memcpy(cal_data, (u16 *)cal_addr, SENSOR_IMX686_LRC_CAL_SIZE);

#if SENSOR_IMX686_CAL_DEBUG
	ret = sensor_imx686_cis_cal_dump(SENSOR_IMX686_LRC_DUMP_NAME,
								(char *)cal_data, (size_t)SENSOR_IMX686_LRC_CAL_SIZE);
	if (ret < 0) {
		err("cis_imx686 LRC Cal dump fail(%d)!\n", ret);
		goto p_err;
	}
#endif

	start_addr = SENSOR_IMX686_LRC_REG_ADDR;
	data_size = SENSOR_IMX686_LRC_CAL_SIZE;

	I2C_MUTEX_LOCK(cis->i2c_lock);

	ret = is_sensor_write8_sequential(client, start_addr, cal_data, data_size);
	if (ret < 0) {
		err("cis_imx686 LRC write Error(%d)\n", ret);
	}

	I2C_MUTEX_UNLOCK(cis->i2c_lock);

p_err:
	return ret;
}
#endif

#ifdef SENSOR_IMX686_WRITE_SENSOR_CAL
int sensor_imx686_cis_QuadSensCal_write(struct v4l2_subdev *subdev)
{
	int ret = 0;
	struct is_cis *cis;
	struct i2c_client *client = NULL;
	struct is_device_sensor_peri *sensor_peri = NULL;

	int position;
	u16 start_addr;
	u16 data_size;

	ulong cal_addr;
	u8 cal_data[SENSOR_IMX686_QUAD_SENS_CAL_SIZE] = {0, };

#ifdef CONFIG_CAMERA_VENDER_MCD_V2
	char *rom_cal_buf = NULL;
#else
	struct is_lib_support *lib = &gPtr_lib_support;
#endif

	FIMC_BUG(!subdev);

	cis = (struct is_cis *)v4l2_get_subdevdata(subdev);
	FIMC_BUG(!cis);
	FIMC_BUG(!cis->cis_data);

	sensor_peri = container_of(cis, struct is_device_sensor_peri, cis);
	FIMC_BUG(!sensor_peri);

	client = cis->client;
	if (unlikely(!client)) {
		err("client is NULL");
		return -EINVAL;
	}

	position = sensor_peri->module->position;

#ifdef CONFIG_CAMERA_VENDER_MCD_V2
	ret = is_sec_get_cal_buf(position, &rom_cal_buf);

	if (ret < 0) {
		goto p_err;
	}

	cal_addr = (ulong)rom_cal_buf;
	if (position == SENSOR_POSITION_REAR) {
		cal_addr += SENSOR_IMX686_QUAD_SENS_CAL_BASE_REAR;
	} else {
		err("cis_imx686 position(%d) is invalid!\n", position);
		goto p_err;
	}
#else
	if (position == SENSOR_POSITION_REAR){
		cal_addr = lib->minfo->kvaddr_cal[position] + SENSOR_IMX686_QUAD_SENS_CAL_BASE_REAR;
	}else {
		err("cis_imx686 position(%d) is invalid!\n", position);
		goto p_err;
	}
#endif

	memcpy(cal_data, (u16 *)cal_addr, SENSOR_IMX686_QUAD_SENS_CAL_SIZE);

#if SENSOR_IMX686_CAL_DEBUG
	ret = sensor_imx686_cis_cal_dump(SENSOR_IMX686_QSC_DUMP_NAME, (char *)cal_data, (size_t)SENSOR_IMX686_QUAD_SENS_CAL_SIZE);
	if (ret < 0) {
		err("cis_imx686 QSC Cal dump fail(%d)!\n", ret);
		goto p_err;
	}
#endif

	start_addr = SENSOR_IMX686_QUAD_SENS_REG_ADDR;
	data_size = SENSOR_IMX686_QUAD_SENS_CAL_SIZE;

	I2C_MUTEX_LOCK(cis->i2c_lock);

	ret = is_sensor_write8_sequential(client, start_addr, cal_data, data_size);
	if (ret < 0) {
		err("cis_imx686 QSC write Error(%d)\n", ret);
	}

	I2C_MUTEX_UNLOCK(cis->i2c_lock);

p_err:
	return ret;
}
#endif

#ifdef USE_AP_PDAF
static void sensor_imx686_cis_set_paf_stat_enable(u32 mode, cis_shared_data *cis_data)
{
	WARN_ON(!cis_data);

	switch (mode) {
	case SENSOR_IMX686_2X2BIN_4624X3468_30FPS:
	case SENSOR_IMX686_2X2BIN_4624X2604_30FPS:
	case SENSOR_IMX686_2X2BIN_4624X2604_60FPS:
	case SENSOR_IMX686_REMOSAIC_FULL_9248X6936_20FPS:
	case SENSOR_IMX686_REMOSAIC_CROP_4624X3468_30FPS:
	case SENSOR_IMX686_REMOSAIC_CROP_4624X2604_30FPS:
	case SENSOR_IMX686_REMOSAIC_CROP_4624X2604_60FPS:
		cis_data->is_data.paf_stat_enable = true;
		break;
	default:
		cis_data->is_data.paf_stat_enable = false;
		break;
	}
}
#endif

static int sensor_imx686_wait_stream_off_status(cis_shared_data *cis_data)
{
	int ret = 0;
	u32 timeout = 0;

	FIMC_BUG(!cis_data);

#define STREAM_OFF_WAIT_TIME 500
	while (timeout < STREAM_OFF_WAIT_TIME) {
		if (cis_data->is_active_area == false &&
				cis_data->stream_on == false) {
			pr_debug("actual stream off\n");
			break;
		}
		timeout++;
	}

	if (timeout == STREAM_OFF_WAIT_TIME) {
		pr_err("actual stream off wait timeout\n");
		ret = -1;
	}

	return ret;
}

int sensor_imx686_cis_check_rev(struct is_cis *cis)
{
	int ret = 0;
	u8 status = 0;
	u8 rev = 0;
	struct i2c_client *client;

	FIMC_BUG(!cis);
	FIMC_BUG(!cis->cis_data);

	client = cis->client;
	if (unlikely(!client)) {
		err("client is NULL");
		ret = -EINVAL;
		goto p_err;
	}

	I2C_MUTEX_LOCK(cis->i2c_lock);

	/* Specify OTP Page Address for READ - Page127(dec) */
	ret = is_sensor_write8(client, SENSOR_IMX686_OTP_PAGE_SETUP_ADDR, 0x7F);
	if (ret < 0) {
		err("i2c fail addr(%x) ,ret = %d\n",
			SENSOR_IMX686_OTP_PAGE_SETUP_ADDR, ret);
		goto i2c_err;
	}

	/* Turn ON OTP Read MODE */
	ret = is_sensor_write8(client, SENSOR_IMX686_OTP_READ_TRANSFER_MODE_ADDR,  0x01);
	if (ret < 0) {
		err("is_sensor_read is fail, (ret %d)", ret);
		ret = -EAGAIN;
		goto i2c_err;
	}

	/* Check status - 0x01 : read ready*/
	ret = is_sensor_read8(client, SENSOR_IMX686_OTP_STATUS_REGISTER_ADDR,  &status);
	if (ret < 0) {
		err("is_sensor_read is fail, (ret %d)", ret);
		ret = -EAGAIN;
		goto i2c_err;
	}

	if ((status & 0x1) == false)
		err("status fail, (%d)", status);

	/* CHIP REV 0x0018 */
	ret = is_sensor_read8(client, SENSOR_IMX686_OTP_CHIP_REVISION_ADDR, &rev);
	if (ret < 0) {
		err("is_sensor_read is fail, (ret %d)", ret);
		ret = -EAGAIN;
		goto i2c_err;
	}

	cis->cis_data->cis_rev = rev;
	pr_info("%s : rev checking: Rev. %#x\n",
		__func__, cis->cis_data->cis_rev);

i2c_err:
	I2C_MUTEX_UNLOCK(cis->i2c_lock);

p_err:
	return ret;
}

int sensor_imx686_cis_check_rev_on_init(struct v4l2_subdev *subdev)
{
	int ret = 0;
	struct i2c_client *client;
	struct is_cis *cis = NULL;

	FIMC_BUG(!subdev);

	cis = (struct is_cis *)v4l2_get_subdevdata(subdev);
	FIMC_BUG(!cis);
	FIMC_BUG(!cis->cis_data);

	client = cis->client;
	if (unlikely(!client)) {
		err("client is NULL");
		ret = -EINVAL;
		return ret;
	}

	memset(cis->cis_data, 0, sizeof(cis_shared_data));

	ret = sensor_imx686_cis_check_rev(cis);

	return ret;
}

/* CIS OPS */
int sensor_imx686_cis_init(struct v4l2_subdev *subdev)
{
	int ret = 0;
	struct is_cis *cis;
	u32 setfile_index = 0;
	cis_setting_info setinfo;
#ifdef USE_CAMERA_HW_BIG_DATA
	struct cam_hw_param *hw_param = NULL;
	struct is_device_sensor_peri *sensor_peri = NULL;
#endif

	setinfo.param = NULL;
	setinfo.return_value = 0;

	FIMC_BUG(!subdev);

	cis = (struct is_cis *)v4l2_get_subdevdata(subdev);
	if (!cis) {
		err("cis is NULL");
		ret = -EINVAL;
		goto p_err;
	}

	FIMC_BUG(!cis->cis_data);
	memset(cis->cis_data, 0, sizeof(cis_shared_data));
	cis->rev_flag = false;

	probe_info("%s IMX686 init\n", __func__);

/***********************************************************************
***** Check that QSC Cal is written for Remosaic Capture.
***** false : Not yet write the QSC
***** true  : Written the QSC Or Skip
***********************************************************************/
	sensor_imx686_cal_write_flag = false;

	ret = sensor_imx686_cis_check_rev(cis);
	if (ret < 0) {
#ifdef USE_CAMERA_HW_BIG_DATA
		sensor_peri = container_of(cis, struct is_device_sensor_peri, cis);
		if (sensor_peri)
			is_sec_get_hw_param(&hw_param, sensor_peri->module->position);
		if (hw_param)
			hw_param->i2c_sensor_err_cnt++;
#endif
		warn("sensor_imx686_check_rev is fail when cis init");
		cis->rev_flag = true;

		ret = 0;
		goto p_err;
	}

	cis->cis_data->product_name = cis->id;
	cis->cis_data->cur_width = SENSOR_IMX686_MAX_WIDTH;
	cis->cis_data->cur_height = SENSOR_IMX686_MAX_HEIGHT;
	cis->cis_data->low_expo_start = 33000;
	cis->need_mode_change = false;

	sensor_imx686_cis_data_calculation(sensor_imx686_pllinfos[setfile_index], cis->cis_data);
	sensor_imx686_set_coarse_integration_max_margin(setfile_index, cis->cis_data);
	sensor_imx686_set_coarse_integration_min(setfile_index, cis->cis_data);
	sensor_imx686_set_coarse_integration_step_value(setfile_index);

#if SENSOR_IMX686_DEBUG_INFO
	setinfo.return_value = 0;
	CALL_CISOPS(cis, cis_get_min_exposure_time, subdev, &setinfo.return_value);
	dbg_sensor(1, "[%s] min exposure time : %d\n", __func__, setinfo.return_value);
	setinfo.return_value = 0;
	CALL_CISOPS(cis, cis_get_max_exposure_time, subdev, &setinfo.return_value);
	dbg_sensor(1, "[%s] max exposure time : %d\n", __func__, setinfo.return_value);
	setinfo.return_value = 0;
	CALL_CISOPS(cis, cis_get_min_analog_gain, subdev, &setinfo.return_value);
	dbg_sensor(1, "[%s] min again : %d\n", __func__, setinfo.return_value);
	setinfo.return_value = 0;
	CALL_CISOPS(cis, cis_get_max_analog_gain, subdev, &setinfo.return_value);
	dbg_sensor(1, "[%s] max again : %d\n", __func__, setinfo.return_value);
	setinfo.return_value = 0;
	CALL_CISOPS(cis, cis_get_min_digital_gain, subdev, &setinfo.return_value);
	dbg_sensor(1, "[%s] min dgain : %d\n", __func__, setinfo.return_value);
	setinfo.return_value = 0;
	CALL_CISOPS(cis, cis_get_max_digital_gain, subdev, &setinfo.return_value);
	dbg_sensor(1, "[%s] max dgain : %d\n", __func__, setinfo.return_value);
	setinfo.return_value = 0;
#endif

#if SENSOR_IMX686_WRITE_PDAF_CAL
	info("[%s] PDAF is eanbled! Write LRC data.\n", __func__);
	ret = sensor_imx686_cis_LRC_write(subdev);
	if (ret < 0) {
		err("sensor_imx686_LRC_Cal_write fail!! (%d)", ret);
		goto p_err;
	}
#endif

#if SENSOR_IMX686_WRITE_SENSOR_CAL
	if (sensor_imx686_cal_write_flag == false) {
		sensor_imx686_cal_write_flag = true;

		info("[%s] mode is QBC Remosaic Mode! Write QSC data.\n", __func__);
		ret = sensor_imx686_cis_QuadSensCal_write(subdev);
		if (ret < 0) {
			err("sensor_imx686_Quad_Sens_Cal_write fail!! (%d)", ret);
			goto p_err;
		}
	}
#endif

#ifdef DEBUG_SENSOR_TIME
	do_gettimeofday(&end);
	dbg_sensor(1, "[%s] time %lu us\n", __func__, (end.tv_sec - st.tv_sec)*1000000 + (end.tv_usec - st.tv_usec));
#endif

p_err:
	return ret;
}

int sensor_imx686_cis_log_status(struct v4l2_subdev *subdev)
{
	int ret = 0;
	struct is_cis *cis;
	struct i2c_client *client = NULL;
	u8 data8 = 0;
	u16 data16 = 0;

	FIMC_BUG(!subdev);

	cis = (struct is_cis *)v4l2_get_subdevdata(subdev);
	if (!cis) {
		err("cis is NULL");
		ret = -ENODEV;
		goto p_err;
	}

	client = cis->client;
	if (unlikely(!client)) {
		err("client is NULL");
		ret = -ENODEV;
		goto p_err;
	}

	I2C_MUTEX_LOCK(cis->i2c_lock);

	pr_info("[%s] *******************************\n", __func__);

	ret = is_sensor_read16(client, SENSOR_IMX686_MODEL_ID_ADDR, &data16);
	if (unlikely(!ret)) pr_info("model_id(0x%x)\n", data16);
	else goto i2c_err;
	ret = is_sensor_read8(client, SENSOR_IMX686_REVISION_NUM_ADDR, &data8);
	if (unlikely(!ret)) pr_info("revision_number(0x%x)\n", data8);
	else goto i2c_err;
	ret = is_sensor_read8(client, SENSOR_IMX686_FRAME_COUNT_ADDR, &data8);
	if (unlikely(!ret)) pr_info("frame_count(0x%x)\n", data8);
	else goto i2c_err;
	ret = is_sensor_read8(client, SENSOR_IMX686_SETUP_MODE_SELECT_ADDR, &data8);
	if (unlikely(!ret)) pr_info("setup_mode(streaming)(0x%x)\n", data8);
	else goto i2c_err;
	ret = is_sensor_read16(client, SENSOR_IMX686_COARSE_INTEG_TIME_ADDR, &data16);
	if (unlikely(!ret)) pr_info("coarse_integration_time(0x%x)\n", data16);
	else goto i2c_err;
	ret = is_sensor_read16(client, SENSOR_IMX686_ANALOG_GAIN_ADDR, &data16);
	if (unlikely(!ret)) pr_info("gain_code_global(0x%x)\n", data16);
	else goto i2c_err;
	ret = is_sensor_read16(client, SENSOR_IMX686_FRAME_LENGTH_LINE_ADDR, &data16);
	if (unlikely(!ret)) pr_info("frame_length_line(0x%x)\n", data16);
	else goto i2c_err;
	ret = is_sensor_read8(client, SENSOR_IMX686_LINE_LENGTH_PCK_ADDR, &data8);
	if (unlikely(!ret)) pr_info("line_length_pck(0x%x)\n", data8);
	else goto i2c_err;

	sensor_cis_dump_registers(subdev, sensor_imx686_setfiles[0], sensor_imx686_setfile_sizes[0]);
	pr_info("[%s] *******************************\n", __func__);

i2c_err:
	I2C_MUTEX_UNLOCK(cis->i2c_lock);

p_err:
	return ret;
}

static int sensor_imx686_cis_group_param_hold_func(struct v4l2_subdev *subdev, unsigned int hold)
{
#if USE_GROUP_PARAM_HOLD
	int ret = 0;
	struct is_cis *cis = NULL;
	struct i2c_client *client = NULL;

	FIMC_BUG(!subdev);

	cis = (struct is_cis *)v4l2_get_subdevdata(subdev);

	FIMC_BUG(!cis);
	FIMC_BUG(!cis->cis_data);

	client = cis->client;
	if (unlikely(!client)) {
		err("client is NULL");
		ret = -EINVAL;
		goto p_err;
	}

	if (hold == cis->cis_data->group_param_hold) {
		pr_debug("already group_param_hold (%d)\n", cis->cis_data->group_param_hold);
		goto p_err;
	}

	I2C_MUTEX_LOCK(cis->i2c_lock);
	ret = is_sensor_write8(client, SENSOR_IMX686_GROUP_PARAM_HOLD_ADDR, hold);
	if (ret < 0)
		goto i2c_err;

	cis->cis_data->group_param_hold = hold;
	ret = 1;

i2c_err:
	I2C_MUTEX_UNLOCK(cis->i2c_lock);
p_err:
	return ret;
#else

	return 0;
#endif
}

/* Input
 *	hold : true - hold, flase - no hold
 * Output
 *      return: 0 - no effect(already hold or no hold)
 *		positive - setted by request
 *		negative - ERROR value
 */
int sensor_imx686_cis_group_param_hold(struct v4l2_subdev *subdev, bool hold)
{
	int ret = 0;
	struct is_cis *cis = NULL;

	FIMC_BUG(!subdev);

	cis = (struct is_cis *)v4l2_get_subdevdata(subdev);

	FIMC_BUG(!cis);
	FIMC_BUG(!cis->cis_data);

	ret = sensor_imx686_cis_group_param_hold_func(subdev, hold);
	if (ret < 0)
		goto p_err;

p_err:
	return ret;
}

int sensor_imx686_cis_set_global_setting(struct v4l2_subdev *subdev)
{
	int ret = 0;
	struct is_cis *cis = NULL;

	FIMC_BUG(!subdev);

	cis = (struct is_cis *)v4l2_get_subdevdata(subdev);
	FIMC_BUG(!cis);

	I2C_MUTEX_LOCK(cis->i2c_lock);

	/* setfile global setting is at camera entrance */
	info("[%s] global setting start\n", __func__);
	ret = sensor_cis_set_registers(subdev, sensor_imx686_global, sensor_imx686_global_size);
	if (ret < 0) {
		err("sensor_imx686_set_registers fail!!");
		goto p_err;
	}

	dbg_sensor(1, "[%s] global setting done\n", __func__);

p_err:
	I2C_MUTEX_UNLOCK(cis->i2c_lock);

	// Check that QSC and DPC Cal is written for Remosaic Capture.
	// false : Not yet write the QSC and DPC
	// true  : Written the QSC and DPC
	sensor_imx686_cal_write_flag = false;
	return ret;
}

int sensor_imx686_cis_mode_change(struct v4l2_subdev *subdev, u32 mode)
{
	int ret = 0;
	u16 fine_integ = 0;
	struct is_cis *cis = NULL;
	struct is_device_sensor *device;

	FIMC_BUG(!subdev);

	cis = (struct is_cis *)v4l2_get_subdevdata(subdev);
	FIMC_BUG(!cis);
	FIMC_BUG(!cis->cis_data);

	device = (struct is_device_sensor *)v4l2_get_subdev_hostdata(subdev);
	if (unlikely(!device)) {
		err("device sensor is null");
		ret = -EINVAL;
		goto p_err;
	}

	if (mode >= sensor_imx686_max_setfile_num) {
		err("invalid mode(%d)!!", mode);
		ret = -EINVAL;
		goto p_err;
	}

	/* If check_rev of IMX686 fail when cis_init, one more check_rev in mode_change */
	if (cis->rev_flag == true) {
		cis->rev_flag = false;
		ret = sensor_imx686_cis_check_rev(cis);
		if (ret < 0) {
			err("sensor_imx686_check_rev is fail");
			goto p_err;
		}
	}

#ifdef USE_AP_PDAF
	sensor_imx686_cis_set_paf_stat_enable(mode, cis->cis_data);
#endif

	I2C_MUTEX_LOCK(cis->i2c_lock);

	info("[%s] mode=%d, mode change setting start\n", __func__, mode);
	ret = sensor_cis_set_registers(subdev, sensor_imx686_setfiles[mode], sensor_imx686_setfile_sizes[mode]);
	if (ret < 0) {
		err("sensor_imx686_set_registers fail!!");
		goto i2c_err;
	}

#if SENSOR_IMX686_EBD_LINE_DISABLE
	/* To Do :  if the mode is needed EBD, add the condition with mode. */
	ret = sensor_imx686_set_ebd(cis);
	if (ret < 0) {
		err("sensor_imx686_set_registers fail!!");
		goto i2c_err;
	}
#endif

	dbg_sensor(1, "[%s] mode changed(%d)\n", __func__, mode);

i2c_err:
	I2C_MUTEX_UNLOCK(cis->i2c_lock);

	sensor_imx686_set_coarse_integration_max_margin(mode, cis->cis_data);
	sensor_imx686_set_coarse_integration_min(mode, cis->cis_data);
	sensor_imx686_set_coarse_integration_step_value(mode);
	sensor_imx686_get_fine_integration_value(cis, &fine_integ);

	/* Constant values */
	cis->cis_data->min_fine_integration_time = fine_integ;
	cis->cis_data->max_fine_integration_time = fine_integ;

p_err:
	return ret;
}

/* TODO: Sensor set size sequence(sensor done, sensor stop, 3AA done in FW case */
int sensor_imx686_cis_set_size(struct v4l2_subdev *subdev, cis_shared_data *cis_data)
{
	int ret = 0;
	bool binning = false;
	u32 ratio_w = 0, ratio_h = 0, start_x = 0, start_y = 0, end_x = 0, end_y = 0;
	u32 even_x= 0, odd_x = 0, even_y = 0, odd_y = 0;
	struct i2c_client *client = NULL;
	struct is_cis *cis = NULL;
#ifdef DEBUG_SENSOR_TIME
	struct timeval st, end;
	do_gettimeofday(&st);
#endif
	FIMC_BUG(!subdev);

	cis = (struct is_cis *)v4l2_get_subdevdata(subdev);
	FIMC_BUG(!cis);

	dbg_sensor(1, "[MOD:D:%d] %s\n", cis->id, __func__);

	if (unlikely(!cis_data)) {
		err("cis data is NULL");
		if (unlikely(!cis->cis_data)) {
			ret = -EINVAL;
			goto p_err;
		} else {
			cis_data = cis->cis_data;
		}
	}

	client = cis->client;
	if (unlikely(!client)) {
		err("client is NULL");
		ret = -EINVAL;
		goto p_err;
	}

	/* Wait actual stream off */
	ret = sensor_imx686_wait_stream_off_status(cis_data);
	if (ret) {
		err("Must stream off\n");
		ret = -EINVAL;
		goto p_err;
	}

	binning = cis_data->binning;
	if (binning) {
		ratio_w = (SENSOR_IMX686_MAX_WIDTH / cis_data->cur_width);
		ratio_h = (SENSOR_IMX686_MAX_HEIGHT / cis_data->cur_height);
	} else {
		ratio_w = 1;
		ratio_h = 1;
	}

	if (((cis_data->cur_width * ratio_w) > SENSOR_IMX686_MAX_WIDTH) ||
		((cis_data->cur_height * ratio_h) > SENSOR_IMX686_MAX_HEIGHT)) {
		err("Config max sensor size over~!!\n");
		ret = -EINVAL;
		goto p_err;
	}

	/* 1. page_select */
	ret = is_sensor_write16(client, 0x6028, 0x2000);
	if (ret < 0)
		 goto p_err;

	/* 2. pixel address region setting */
	start_x = ((SENSOR_IMX686_MAX_WIDTH - cis_data->cur_width * ratio_w) / 2) & (~0x1);
	start_y = ((SENSOR_IMX686_MAX_HEIGHT - cis_data->cur_height * ratio_h) / 2) & (~0x1);
	end_x = start_x + (cis_data->cur_width * ratio_w - 1);
	end_y = start_y + (cis_data->cur_height * ratio_h - 1);

	if (!(end_x & (0x1)) || !(end_y & (0x1))) {
		err("Sensor pixel end address must odd\n");
		ret = -EINVAL;
		goto p_err;
	}

	ret = is_sensor_write16(client, 0x0344, start_x);
	if (ret < 0)
		 goto p_err;
	ret = is_sensor_write16(client, 0x0346, start_y);
	if (ret < 0)
		 goto p_err;
	ret = is_sensor_write16(client, 0x0348, end_x);
	if (ret < 0)
		 goto p_err;
	ret = is_sensor_write16(client, 0x034A, end_y);
	if (ret < 0)
		 goto p_err;

	/* 3. output address setting */
	ret = is_sensor_write16(client, 0x034C, cis_data->cur_width);
	if (ret < 0)
		 goto p_err;
	ret = is_sensor_write16(client, 0x034E, cis_data->cur_height);
	if (ret < 0)
		 goto p_err;

	/* If not use to binning, sensor image should set only crop */
	if (!binning) {
		dbg_sensor(1, "Sensor size set is not binning\n");
		goto p_err;
	}

	/* 4. sub sampling setting */
	even_x = 1;	/* 1: not use to even sampling */
	even_y = 1;
	odd_x = (ratio_w * 2) - even_x;
	odd_y = (ratio_h * 2) - even_y;

	ret = is_sensor_write16(client, 0x0380, even_x);
	if (ret < 0)
		 goto p_err;
	ret = is_sensor_write16(client, 0x0382, odd_x);
	if (ret < 0)
		 goto p_err;
	ret = is_sensor_write16(client, 0x0384, even_y);
	if (ret < 0)
		 goto p_err;
	ret = is_sensor_write16(client, 0x0386, odd_y);
	if (ret < 0)
		 goto p_err;

	/* 5. binnig setting */
	ret = is_sensor_write8(client, 0x0900, binning);	/* 1:  binning enable, 0: disable */
	if (ret < 0)
		goto p_err;
	ret = is_sensor_write8(client, 0x0901, (ratio_w << 4) | ratio_h);
	if (ret < 0)
		goto p_err;

	/* 6. scaling setting: but not use */
	/* scaling_digital_scaling */
	ret = is_sensor_write16(client, 0x0402, 0x1010);
	if (ret < 0)
		goto p_err;
	/* scaling_hbin_digital_binning_factor */
	ret = is_sensor_write16(client, 0x0404, 0x0010);
	if (ret < 0)
		goto p_err;
	/* scaling_tetracell_digital_binning_factor */
	ret = is_sensor_write16(client, 0x0400, 0x1010);
	if (ret < 0)
		goto p_err;

	cis_data->frame_time = (cis_data->line_readOut_time * cis_data->cur_height / 1000);
	cis->cis_data->rolling_shutter_skew = (cis->cis_data->cur_height - 1) * cis->cis_data->line_readOut_time;
	dbg_sensor(1, "[%s] frame_time(%d), rolling_shutter_skew(%lld)\n", __func__,
		cis->cis_data->frame_time, cis->cis_data->rolling_shutter_skew);

#ifdef DEBUG_SENSOR_TIME
	do_gettimeofday(&end);
	dbg_sensor(1, "[%s] time %lu us\n", __func__, (end.tv_sec - st.tv_sec) * 1000000 + (end.tv_usec - st.tv_usec));
#endif

p_err:
	return ret;
}

int sensor_imx686_cis_stream_on(struct v4l2_subdev *subdev)
{
	int ret = 0;
	struct is_cis *cis;
	struct i2c_client *client;
	cis_shared_data *cis_data;

#ifdef DEBUG_SENSOR_TIME
	struct timeval st, end;
	do_gettimeofday(&st);
#endif

	FIMC_BUG(!subdev);

	cis = (struct is_cis *)v4l2_get_subdevdata(subdev);

	FIMC_BUG(!cis);
	FIMC_BUG(!cis->cis_data);

	client = cis->client;
	if (unlikely(!client)) {
		err("client is NULL");
		ret = -EINVAL;
		goto p_err;
	}

	cis_data = cis->cis_data;

	dbg_sensor(1, "[MOD:D:%d] %s\n", cis->id, __func__);

	sensor_imx686_cis_group_param_hold(subdev, 0x01);
	if (ret < 0)
		err("group_param_hold_func failed at stream on");

	I2C_MUTEX_LOCK(cis->i2c_lock);

#if SENSOR_IMX686_DEBUG_INFO
	{
		u16 pll;
		is_sensor_read16(client, 0x0300, &pll);
		dbg_sensor(1, "______ vt_pix_clk_div(%x)\n", pll);
		is_sensor_read16(client, 0x0302, &pll);
		dbg_sensor(1, "______ vt_sys_clk_div(%x)\n", pll);
		is_sensor_read16(client, 0x0304, &pll);
		dbg_sensor(1, "______ pre_pll_clk_div(%x)\n", pll);
		is_sensor_read16(client, 0x0306, &pll);
		dbg_sensor(1, "______ pll_multiplier(%x)\n", pll);
		is_sensor_read16(client, 0x030a, &pll);
		dbg_sensor(1, "______ op_sys_clk_div(%x)\n", pll);
		is_sensor_read16(client, 0x030c, &pll);
		dbg_sensor(1, "______ op_prepllck_div(%x)\n", pll);
		is_sensor_read16(client, 0x030e, &pll);
		dbg_sensor(1, "______ op_pll_multiplier(%x)\n", pll);
		is_sensor_read16(client, 0x0310, &pll);
		dbg_sensor(1, "______ pll_mult_driv(%x)\n", pll);
		is_sensor_read16(client, 0x0340, &pll);
		dbg_sensor(1, "______ frame_length_lines(%x)\n", pll);
		is_sensor_read16(client, 0x0342, &pll);
		dbg_sensor(1, "______ line_length_pck(%x)\n", pll);
	}

	{
		u16 data_format = 0x00;
		u8 value = 0x0;

		is_sensor_read8(client, 0x0110, &value);
		info("IMX686: CSI_CH_ID(%x)\n", value);
		is_sensor_read8(client, 0x3076, &value);
		info("IMX686: PDAF_L_VCID(%x)\n", value);
		is_sensor_read16(client, 0x0112, &data_format);
		info("IMX686: CSI_DT_FMT(%x)\n", data_format);
		is_sensor_read8(client, 0x3077, &value);
		info("IMX686: PD_L_DT(%x)\n", value);
		is_sensor_read8(client, 0x3400, &value);
		info("IMX686: PDAF_TYPE_SEL(%x)\n", value);
		is_sensor_read8(client, 0x3092, &value);
		info("IMX686: PDAF_OUT_EN(%x)\n", value);
		is_sensor_read8(client, 0x401B, &value);
		info("IMX686: PDAF_Y_OUT_EN(%x)\n", value);
		is_sensor_read8(client, 0x4018, &value);
		info("IMX686: PDAF_X_OUT_SIZE_H(%x)\n", value);
		is_sensor_read8(client, 0x4019, &value);
		info("IMX686: PDAF_X_OUT_SIZE_L(%x)\n", value);
		is_sensor_read8(client, 0xBCF1, &value);
		info("IMX686: EBD_SIZE_V(%x)\n", value);
		is_sensor_read8(client, 0x4107, &value);
		info("IMX686: EBD_DT(%x)\n", value);
	}
#endif

	info("[%s] start\n", __func__);

	/* Sensor stream on */
	ret = is_sensor_write8(client, SENSOR_IMX686_SETUP_MODE_SELECT_ADDR, 0x01);
	if (ret < 0)
		err("i2c transfer fail addr(%x), val(%x), ret = %d\n", 0x0100, 0x01, ret);

	I2C_MUTEX_UNLOCK(cis->i2c_lock);

	sensor_imx686_cis_group_param_hold(subdev, 0x00);
	if (ret < 0)
		err("group_param_hold_func failed at stream on");

	cis_data->stream_on = true;

#ifdef DEBUG_SENSOR_TIME
	do_gettimeofday(&end);
	dbg_sensor(1, "[%s] time %lu us\n", __func__, (end.tv_sec - st.tv_sec)*1000000 + (end.tv_usec - st.tv_usec));
#endif

p_err:
	return ret;
}

int sensor_imx686_cis_stream_off(struct v4l2_subdev *subdev)
{
	int ret = 0;
	struct is_cis *cis;
	struct i2c_client *client;
	cis_shared_data *cis_data;

#ifdef DEBUG_SENSOR_TIME
	struct timeval st, end;
	do_gettimeofday(&st);
#endif

	FIMC_BUG(!subdev);

	cis = (struct is_cis *)v4l2_get_subdevdata(subdev);

	FIMC_BUG(!cis);
	FIMC_BUG(!cis->cis_data);

	client = cis->client;
	if (unlikely(!client)) {
		err("client is NULL");
		ret = -EINVAL;
		goto p_err;
	}

	cis_data = cis->cis_data;

	dbg_sensor(1, "[MOD:D:%d] %s\n", cis->id, __func__);

	ret = sensor_imx686_cis_group_param_hold_func(subdev, 0x00);
	if (ret < 0)
		err("group_param_hold_func failed at stream off");

	I2C_MUTEX_LOCK(cis->i2c_lock);

	/* Sensor stream off */
	ret = is_sensor_write8(client, SENSOR_IMX686_SETUP_MODE_SELECT_ADDR, 0x00);
	if (ret < 0)
		err("i2c transfer fail addr(%x), val(%x), ret = %d\n", 0x0100, 0x0000, ret);

	I2C_MUTEX_UNLOCK(cis->i2c_lock);

	info("%s\n", __func__);
	cis_data->stream_on = false;

#ifdef DEBUG_SENSOR_TIME
	do_gettimeofday(&end);
	dbg_sensor(1, "[%s] time %lu us\n", __func__, (end.tv_sec - st.tv_sec)*1000000 + (end.tv_usec - st.tv_usec));
#endif

p_err:
	return ret;
}

int sensor_imx686_cis_adjust_frame_duration(struct v4l2_subdev *subdev,
						u32 input_exposure_time,
						u32 *target_duration)
{
	int ret = 0;
	struct is_cis *cis;
	cis_shared_data *cis_data;

	u64 pix_rate_freq_khz = 0;
	u32 line_length_pck = 0;
	u32 coarse_integ_time = 0;
	u32 frame_length_lines = 0;
	u32 frame_duration = 0;
	u32 max_frame_us_time = 0;

#ifdef DEBUG_SENSOR_TIME
	struct timeval st, end;
	do_gettimeofday(&st);
#endif

	FIMC_BUG(!subdev);
	FIMC_BUG(!target_duration);

	cis = (struct is_cis *)v4l2_get_subdevdata(subdev);

	FIMC_BUG(!cis);
	FIMC_BUG(!cis->cis_data);

	cis_data = cis->cis_data;

	pix_rate_freq_khz = cis_data->pclk / 1000;
	line_length_pck = cis_data->line_length_pck;
	coarse_integ_time = (u32)(((pix_rate_freq_khz * input_exposure_time)
							- cis_data->min_fine_integration_time) / (line_length_pck * 1000));

	if (cis->min_fps == cis->max_fps) {
		dbg_sensor(1, "[%s] requested min_fps(%d), max_fps(%d) from HAL\n", __func__, cis->min_fps, cis->max_fps);

		if (coarse_integ_time > cis_data->max_coarse_integration_time) {
			dbg_sensor(1, "[MOD:D:%d] %s, vsync_cnt(%d), coarse(%d) max(%d)\n", cis->id, __func__,
				cis_data->sen_vsync_count, coarse_integ_time, cis_data->max_coarse_integration_time);
			coarse_integ_time = cis_data->max_coarse_integration_time;
		}

		if (coarse_integ_time < cis_data->min_coarse_integration_time) {
			dbg_sensor(1, "[MOD:D:%d] %s, vsync_cnt(%d), coarse(%d) min(%d)\n", cis->id, __func__,
				cis_data->sen_vsync_count, coarse_integ_time, cis_data->min_coarse_integration_time);
			coarse_integ_time = cis_data->min_coarse_integration_time;
		}
	}

	frame_length_lines = coarse_integ_time + cis_data->max_margin_coarse_integration_time;
	frame_duration = (u32)((((u64)frame_length_lines * line_length_pck) / pix_rate_freq_khz) * 1000);
	max_frame_us_time = 1000000/cis->min_fps;

	dbg_sensor(1, "[%s](vsync cnt = %d) input exp(%d), adj duration, frame duraion(%d), min_frame_us(%d)\n",
			__func__, cis_data->sen_vsync_count, input_exposure_time, frame_duration, cis_data->min_frame_us_time);
	dbg_sensor(1, "[%s](vsync cnt = %d) adj duration, frame duraion(%d), min_frame_us(%d), max_frame_us_time(%d)\n",
			__func__, cis_data->sen_vsync_count, frame_duration, cis_data->min_frame_us_time, max_frame_us_time);

	*target_duration = MAX(frame_duration, cis_data->min_frame_us_time);
	if(cis->min_fps == cis->max_fps) {
		*target_duration = MIN(frame_duration, max_frame_us_time);
	}

	dbg_sensor(1, "[%s] calcurated frame_duration(%d), adjusted frame_duration(%d)\n", __func__, frame_duration, *target_duration);

#ifdef DEBUG_SENSOR_TIME
	do_gettimeofday(&end);
	dbg_sensor(1, "[%s] time %lu us\n", __func__, (end.tv_sec - st.tv_sec)*1000000 + (end.tv_usec - st.tv_usec));
#endif

	return ret;
}

int sensor_imx686_cis_set_frame_duration(struct v4l2_subdev *subdev, u32 frame_duration)
{
	int ret = 0;
	int hold = 0;
	struct is_cis *cis;
	struct i2c_client *client;
	cis_shared_data *cis_data;

	u64 pix_rate_freq_khz = 0;
	u32 line_length_pck = 0;
	u16 frame_length_lines = 0;

#ifdef DEBUG_SENSOR_TIME
	struct timeval st, end;
	do_gettimeofday(&st);
#endif

	FIMC_BUG(!subdev);

	cis = (struct is_cis *)v4l2_get_subdevdata(subdev);

	FIMC_BUG(!cis);
	FIMC_BUG(!cis->cis_data);

	client = cis->client;
	if (unlikely(!client)) {
		err("client is NULL");
		ret = -EINVAL;
		goto p_err;
	}

	cis_data = cis->cis_data;

	if (frame_duration < cis_data->min_frame_us_time) {
		dbg_sensor(1, "frame duration(%d) is less than min(%d)\n",
			frame_duration, cis_data->min_frame_us_time);
		frame_duration = cis_data->min_frame_us_time;
	}

	pix_rate_freq_khz = cis_data->pclk / 1000;
	line_length_pck = cis_data->line_length_pck;

	frame_length_lines = (u16)((pix_rate_freq_khz * frame_duration) / (line_length_pck * 1000));

	dbg_sensor(1, "[MOD:D:%d] %s, pix_rate_freq_khz(%zu), frame_duration = %d us,"
		KERN_CONT " line_length_pck(%d), frame_length_lines(%d)\n",
		cis->id, __func__, pix_rate_freq_khz, frame_duration, line_length_pck, frame_length_lines);

	hold = sensor_imx686_cis_group_param_hold_func(subdev, 0x01);
	if (hold < 0) {
		ret = hold;
		goto p_err;
	}

	I2C_MUTEX_LOCK(cis->i2c_lock);

	ret = is_sensor_write16(client, SENSOR_IMX686_FRAME_LENGTH_LINE_ADDR, frame_length_lines);
	if (ret < 0)
		goto i2c_err;

	cis_data->cur_frame_us_time = frame_duration;
	cis_data->frame_length_lines = frame_length_lines;
	cis_data->max_coarse_integration_time =
		cis_data->frame_length_lines - cis_data->max_margin_coarse_integration_time;

#ifdef DEBUG_SENSOR_TIME
	do_gettimeofday(&end);
	dbg_sensor(1, "[%s] time %lu us\n", __func__, (end.tv_sec - st.tv_sec)*1000000 + (end.tv_usec - st.tv_usec));
#endif

i2c_err:
	I2C_MUTEX_UNLOCK(cis->i2c_lock);

p_err:
	if (hold > 0) {
		hold = sensor_imx686_cis_group_param_hold_func(subdev, 0x00);
		if (hold < 0)
			ret = hold;
	}

	return ret;
}

int sensor_imx686_cis_set_frame_rate(struct v4l2_subdev *subdev, u32 min_fps)
{
	int ret = 0;
	struct is_cis *cis;
	cis_shared_data *cis_data;

	u32 frame_duration = 0;

#ifdef DEBUG_SENSOR_TIME
	struct timeval st, end;
	do_gettimeofday(&st);
#endif

	FIMC_BUG(!subdev);

	cis = (struct is_cis *)v4l2_get_subdevdata(subdev);

	FIMC_BUG(!cis);
	FIMC_BUG(!cis->cis_data);

	cis_data = cis->cis_data;

	if (min_fps > cis_data->max_fps) {
		err("[MOD:D:%d] %s, request FPS is too high(%d), set to max_fps(%d)\n",
			cis->id, __func__, min_fps, cis_data->max_fps);
		min_fps = cis_data->max_fps;
	}

	if (min_fps == 0) {
		err("[MOD:D:%d] %s, request FPS is 0, set to min FPS(1)\n", cis->id, __func__);
		min_fps = 1;
	}

	frame_duration = (1 * 1000 * 1000) / min_fps;

	dbg_sensor(1, "[MOD:D:%d] %s, set FPS(%d), frame duration(%d)\n",
			cis->id, __func__, min_fps, frame_duration);

	ret = sensor_imx686_cis_set_frame_duration(subdev, frame_duration);
	if (ret < 0) {
		err("[MOD:D:%d] %s, set frame duration is fail(%d)\n",
			cis->id, __func__, ret);
		goto p_err;
	}

	cis_data->min_frame_us_time = frame_duration;

#ifdef DEBUG_SENSOR_TIME
	do_gettimeofday(&end);
	dbg_sensor(1, "[%s] time %lu us\n", __func__, (end.tv_sec - st.tv_sec)*1000000 + (end.tv_usec - st.tv_usec));
#endif

p_err:
	return ret;
}

int sensor_imx686_cis_set_exposure_time(struct v4l2_subdev *subdev, struct ae_param *target_exposure)
{
	int ret = 0;
	int hold = 0;
	struct is_cis *cis;
	struct i2c_client *client;
	cis_shared_data *cis_data;

	u64 pix_rate_freq_khz = 0;
	u16 short_coarse_int = 0;
	u32 line_length_pck = 0;
	u32 min_fine_int = 0;

#ifdef DEBUG_SENSOR_TIME
	struct timeval st, end;
	do_gettimeofday(&st);
#endif

	FIMC_BUG(!subdev);
	FIMC_BUG(!target_exposure);

	cis = (struct is_cis *)v4l2_get_subdevdata(subdev);

	FIMC_BUG(!cis);
	FIMC_BUG(!cis->cis_data);

	client = cis->client;
	if (unlikely(!client)) {
		err("client is NULL");
		ret = -EINVAL;
		goto p_err;
	}

	if ((target_exposure->long_val <= 0) || (target_exposure->short_val <= 0)) {
		err("[%s] invalid target exposure(%d, %d)\n", __func__,
				target_exposure->long_val, target_exposure->short_val);
		ret = -EINVAL;
		goto p_err;
	}

	cis_data = cis->cis_data;

	dbg_sensor(1, "[MOD:D:%d] %s, vsync_cnt(%d), target long(%d), short(%d)\n", cis->id, __func__,
			cis_data->sen_vsync_count, target_exposure->long_val, target_exposure->short_val);

	pix_rate_freq_khz = cis_data->pclk / 1000;
	line_length_pck = cis_data->line_length_pck;
	min_fine_int = cis_data->min_fine_integration_time;

	dbg_sensor(1, "[MOD:D:%d] %s, pix_rate_freq_khz (%d), line_length_pck(%d), min_fine_int (%d)\n",
		cis->id, __func__, pix_rate_freq_khz, line_length_pck, min_fine_int);

	short_coarse_int = (u16)(((target_exposure->short_val * pix_rate_freq_khz) - min_fine_int) / (line_length_pck * 1000));

	if (short_coarse_int%coarse_integ_step_value)
		short_coarse_int -= 1;

	if (short_coarse_int > cis_data->max_coarse_integration_time) {
		dbg_sensor(1, "[MOD:D:%d] %s, vsync_cnt(%d), short coarse(%d) max(%d)\n", cis->id, __func__,
			cis_data->sen_vsync_count, short_coarse_int, cis_data->max_coarse_integration_time);
		short_coarse_int = cis_data->max_coarse_integration_time;
	}

	if (short_coarse_int < cis_data->min_coarse_integration_time) {
		dbg_sensor(1, "[MOD:D:%d] %s, vsync_cnt(%d), short coarse(%d) min(%d)\n", cis->id, __func__,
			cis_data->sen_vsync_count, short_coarse_int, cis_data->min_coarse_integration_time);
		short_coarse_int = cis_data->min_coarse_integration_time;
	}

	if (cis_data->min_coarse_integration_time == SENSOR_IMX686_COARSE_INTEGRATION_TIME_MIN
		&& short_coarse_int < 14
		&& short_coarse_int%2 == 0) {
		short_coarse_int -= 1;
	}

	cis_data->cur_exposure_coarse = short_coarse_int;
	cis_data->cur_long_exposure_coarse = short_coarse_int;
	cis_data->cur_short_exposure_coarse = short_coarse_int;

	hold = sensor_imx686_cis_group_param_hold_func(subdev, 0x01);
	if (hold < 0) {
		ret = hold;
		goto p_err;
	}

	I2C_MUTEX_LOCK(cis->i2c_lock);

	/* Short exposure */
	ret = is_sensor_write16(client, SENSOR_IMX686_COARSE_INTEG_TIME_ADDR, short_coarse_int);
	if (ret < 0)
		goto i2c_err;

	dbg_sensor(1, "[MOD:D:%d] %s, vsync_cnt(%d), pix_rate_freq_khz (%zu), line_length_pck(%d), min_fine_int (%d)\n",
		cis->id, __func__, cis_data->sen_vsync_count, pix_rate_freq_khz, line_length_pck, min_fine_int);
	dbg_sensor(1, "[MOD:D:%d] %s, vsync_cnt(%d), frame_length_lines(%d), short_coarse_int(%d)\n",
		cis->id, __func__, cis_data->sen_vsync_count, cis_data->frame_length_lines, short_coarse_int);

#ifdef DEBUG_SENSOR_TIME
	do_gettimeofday(&end);
	dbg_sensor(1, "[%s] time %lu us\n", __func__, (end.tv_sec - st.tv_sec)*1000000 + (end.tv_usec - st.tv_usec));
#endif

i2c_err:
	I2C_MUTEX_UNLOCK(cis->i2c_lock);

p_err:
	if (hold > 0) {
		hold = sensor_imx686_cis_group_param_hold_func(subdev, 0x00);
		if (hold < 0)
			ret = hold;
	}

	return ret;
}

int sensor_imx686_cis_get_min_exposure_time(struct v4l2_subdev *subdev, u32 *min_expo)
{
	int ret = 0;
	struct is_cis *cis = NULL;
	cis_shared_data *cis_data = NULL;
	u32 min_integration_time = 0;
	u32 min_coarse = 0;
	u32 min_fine = 0;
	u64 pix_rate_freq_khz = 0;
	u32 line_length_pck = 0;

#ifdef DEBUG_SENSOR_TIME
	struct timeval st, end;
	do_gettimeofday(&st);
#endif

	FIMC_BUG(!subdev);
	FIMC_BUG(!min_expo);

	cis = (struct is_cis *)v4l2_get_subdevdata(subdev);

	FIMC_BUG(!cis);
	FIMC_BUG(!cis->cis_data);

	cis_data = cis->cis_data;

	pix_rate_freq_khz = cis_data->pclk / 1000;
	line_length_pck = cis_data->line_length_pck;
	min_coarse = cis_data->min_coarse_integration_time;
	min_fine = cis_data->min_fine_integration_time;

	min_integration_time = (u32)((u64)((line_length_pck * min_coarse) + min_fine) * 1000 / pix_rate_freq_khz);
	*min_expo = min_integration_time;

	dbg_sensor(1, "[%s] min integration time %d\n", __func__, min_integration_time);

#ifdef DEBUG_SENSOR_TIME
	do_gettimeofday(&end);
	dbg_sensor(1, "[%s] time %lu us\n", __func__, (end.tv_sec - st.tv_sec)*1000000 + (end.tv_usec - st.tv_usec));
#endif

	return ret;
}

int sensor_imx686_cis_get_max_exposure_time(struct v4l2_subdev *subdev, u32 *max_expo)
{
	int ret = 0;
	struct is_cis *cis;
	cis_shared_data *cis_data;
	u32 max_integration_time = 0;
	u32 max_coarse_margin = 0;
	u32 max_coarse = 0;
	u32 max_fine = 0;
	u64 pix_rate_freq_khz = 0;
	u32 line_length_pck = 0;
	u32 frame_length_lines = 0;

#ifdef DEBUG_SENSOR_TIME
	struct timeval st, end;
	do_gettimeofday(&st);
#endif

	FIMC_BUG(!subdev);
	FIMC_BUG(!max_expo);

	cis = (struct is_cis *)v4l2_get_subdevdata(subdev);

	FIMC_BUG(!cis);
	FIMC_BUG(!cis->cis_data);

	cis_data = cis->cis_data;

	pix_rate_freq_khz = cis_data->pclk / 1000;
	line_length_pck = cis_data->line_length_pck;
	frame_length_lines = cis_data->frame_length_lines;

	max_coarse_margin = cis_data->max_margin_coarse_integration_time;
	max_coarse = frame_length_lines - max_coarse_margin;
	max_fine = cis_data->max_fine_integration_time;

	max_integration_time = (u32)((u64)((line_length_pck * max_coarse) + max_fine) * 1000 / pix_rate_freq_khz) ;

	*max_expo = max_integration_time;

	cis_data->max_coarse_integration_time = max_coarse;

	dbg_sensor(1, "[%s] frame_length_lines (%d), line_length_pck (%d)\n",
			__func__, frame_length_lines, line_length_pck);
	dbg_sensor(1, "[%s] max integration time (%d), max coarse integration (%d)\n",
			__func__, max_integration_time, cis_data->max_coarse_integration_time);

#ifdef DEBUG_SENSOR_TIME
	do_gettimeofday(&end);
	dbg_sensor(1, "[%s] time %lu us\n", __func__, (end.tv_sec - st.tv_sec)*1000000 + (end.tv_usec - st.tv_usec));
#endif

	return ret;
}

int sensor_imx686_cis_adjust_analog_gain(struct v4l2_subdev *subdev, u32 input_again, u32 *target_permile)
{
	int ret = 0;
	struct is_cis *cis;
	cis_shared_data *cis_data;

	u32 again_code = 0;
	u32 again_permile = 0;

#ifdef DEBUG_SENSOR_TIME
	struct timeval st, end;
	do_gettimeofday(&st);
#endif

	FIMC_BUG(!subdev);
	FIMC_BUG(!target_permile);

	cis = (struct is_cis *)v4l2_get_subdevdata(subdev);

	FIMC_BUG(!cis);
	FIMC_BUG(!cis->cis_data);

	cis_data = cis->cis_data;

	again_code = sensor_cis_calc_again_code(input_again);

	if (again_code > cis_data->max_analog_gain[0]) {
		again_code = cis_data->max_analog_gain[0];
	} else if (again_code < cis_data->min_analog_gain[0]) {
		again_code = cis_data->min_analog_gain[0];
	}

	again_permile = sensor_cis_calc_again_permile(again_code);

	dbg_sensor(1, "[%s] min again(%d), max(%d), input_again(%d), code(%d), permile(%d)\n", __func__,
			cis_data->max_analog_gain[0],
			cis_data->min_analog_gain[0],
			input_again,
			again_code,
			again_permile);

	*target_permile = again_permile;

	return ret;
}

int sensor_imx686_cis_set_analog_gain(struct v4l2_subdev *subdev, struct ae_param *again)
{
	int ret = 0;
	int hold = 0;
	struct is_cis *cis;
	struct i2c_client *client;

	u16 analog_gain = 0;

#ifdef DEBUG_SENSOR_TIME
	struct timeval st, end;
	do_gettimeofday(&st);
#endif

	FIMC_BUG(!subdev);
	FIMC_BUG(!again);

	cis = (struct is_cis *)v4l2_get_subdevdata(subdev);

	FIMC_BUG(!cis);

	client = cis->client;
	if (unlikely(!client)) {
		err("client is NULL");
		ret = -EINVAL;
		goto p_err;
	}

	analog_gain = (u16)sensor_imx686_cis_calc_again_code(again->val);

	if (analog_gain < cis->cis_data->min_analog_gain[0]) {
		analog_gain = cis->cis_data->min_analog_gain[0];
	}

	if (analog_gain > cis->cis_data->max_analog_gain[0]) {
		analog_gain = cis->cis_data->max_analog_gain[0];
	}

	dbg_sensor(1, "[MOD:D:%d] %s(vsync cnt = %d), input_again = %d us, analog_gain(%#x)\n",
		cis->id, __func__, cis->cis_data->sen_vsync_count, again->val, analog_gain);

	hold = sensor_imx686_cis_group_param_hold_func(subdev, 0x01);
	if (hold < 0) {
		ret = hold;
		goto p_err;
	}

	I2C_MUTEX_LOCK(cis->i2c_lock);

	// the address of analog_gain is [9:0] from 0x0204 to 0x0205
	analog_gain &= 0x03FF;

	ret = is_sensor_write16(client, SENSOR_IMX686_ANALOG_GAIN_ADDR, analog_gain);
	if (ret < 0)
		goto i2c_err;

#ifdef DEBUG_SENSOR_TIME
	do_gettimeofday(&end);
	dbg_sensor(1, "[%s] time %lu us\n", __func__, (end.tv_sec - st.tv_sec)*1000000 + (end.tv_usec - st.tv_usec));
#endif

i2c_err:
	I2C_MUTEX_UNLOCK(cis->i2c_lock);

p_err:
	if (hold > 0) {
		hold = sensor_imx686_cis_group_param_hold_func(subdev, 0x00);
		if (hold < 0)
			ret = hold;
	}

	return ret;
}

int sensor_imx686_cis_get_analog_gain(struct v4l2_subdev *subdev, u32 *again)
{
	int ret = 0;
	int hold = 0;
	struct is_cis *cis;
	struct i2c_client *client;

	u16 analog_gain = 0;

#ifdef DEBUG_SENSOR_TIME
	struct timeval st, end;
	do_gettimeofday(&st);
#endif

	FIMC_BUG(!subdev);
	FIMC_BUG(!again);

	cis = (struct is_cis *)v4l2_get_subdevdata(subdev);

	FIMC_BUG(!cis);

	client = cis->client;
	if (unlikely(!client)) {
		err("client is NULL");
		ret = -EINVAL;
		goto p_err;
	}

	hold = sensor_imx686_cis_group_param_hold_func(subdev, 0x01);
	if (hold < 0) {
		ret = hold;
		goto p_err;
	}

	I2C_MUTEX_LOCK(cis->i2c_lock);

	ret = is_sensor_read16(client, SENSOR_IMX686_ANALOG_GAIN_ADDR, &analog_gain);
	if (ret < 0)
		goto i2c_err;

	analog_gain &= 0x03FF;
	*again = sensor_imx686_cis_calc_again_permile(analog_gain);

	dbg_sensor(1, "[MOD:D:%d] %s, cur_again = %d us, analog_gain(%#x)\n",
			cis->id, __func__, *again, analog_gain);

#ifdef DEBUG_SENSOR_TIME
	do_gettimeofday(&end);
	dbg_sensor(1, "[%s] time %lu us\n", __func__, (end.tv_sec - st.tv_sec)*1000000 + (end.tv_usec - st.tv_usec));
#endif

i2c_err:
	I2C_MUTEX_UNLOCK(cis->i2c_lock);

p_err:
	if (hold > 0) {
		hold = sensor_imx686_cis_group_param_hold_func(subdev, 0x00);
		if (hold < 0)
			ret = hold;
	}

	return ret;
}

int sensor_imx686_cis_get_min_analog_gain(struct v4l2_subdev *subdev, u32 *min_again)
{
	int ret = 0;
	struct is_cis *cis;
	cis_shared_data *cis_data;
	u16 min_again_code = SENSOR_IMX686_MIN_ANALOG_GAIN_SET_VALUE;

#ifdef DEBUG_SENSOR_TIME
	struct timeval st, end;
	do_gettimeofday(&st);
#endif

	FIMC_BUG(!subdev);
	FIMC_BUG(!min_again);

	cis = (struct is_cis *)v4l2_get_subdevdata(subdev);

	FIMC_BUG(!cis);
	FIMC_BUG(!cis->cis_data);

	cis_data = cis->cis_data;

	cis_data->min_analog_gain[0] = min_again_code;
	cis_data->min_analog_gain[1] = sensor_imx686_cis_calc_again_permile(min_again_code);

	*min_again = cis_data->min_analog_gain[1];

	dbg_sensor(1, "[%s] min_again_code %d, min_again_permile %d\n", __func__,
		cis_data->min_analog_gain[0], cis_data->min_analog_gain[1]);

#ifdef DEBUG_SENSOR_TIME
	do_gettimeofday(&end);
	dbg_sensor(1, "[%s] time %lu us\n", __func__, (end.tv_sec - st.tv_sec)*1000000 + (end.tv_usec - st.tv_usec));
#endif

	return ret;
}

int sensor_imx686_cis_get_max_analog_gain(struct v4l2_subdev *subdev, u32 *max_again)
{
	int ret = 0;
	struct is_cis *cis;
	cis_shared_data *cis_data;
	u16 max_again_code = SENSOR_IMX686_MAX_ANALOG_GAIN_SET_VALUE;

#ifdef DEBUG_SENSOR_TIME
	struct timeval st, end;
	do_gettimeofday(&st);
#endif

	FIMC_BUG(!subdev);
	FIMC_BUG(!max_again);

	cis = (struct is_cis *)v4l2_get_subdevdata(subdev);

	FIMC_BUG(!cis);
	FIMC_BUG(!cis->cis_data);

	cis_data = cis->cis_data;

	cis_data->max_analog_gain[0] = max_again_code;
	cis_data->max_analog_gain[1] = sensor_imx686_cis_calc_again_permile(max_again_code);

	*max_again = cis_data->max_analog_gain[1];

	dbg_sensor(1, "[%s] max_again_code %d, max_again_permile %d\n", __func__,
		cis_data->max_analog_gain[0], cis_data->max_analog_gain[1]);

#ifdef DEBUG_SENSOR_TIME
	do_gettimeofday(&end);
	dbg_sensor(1, "[%s] time %lu us\n", __func__, (end.tv_sec - st.tv_sec)*1000000 + (end.tv_usec - st.tv_usec));
#endif

	return ret;
}

int sensor_imx686_cis_set_digital_gain(struct v4l2_subdev *subdev, struct ae_param *dgain)
{
	int ret = 0;
	int hold = 0;
	struct is_cis *cis;
	struct i2c_client *client;
	cis_shared_data *cis_data;

	u16 short_dgain = 0;
	u8 dgains[2] = {0};

#ifdef DEBUG_SENSOR_TIME
	struct timeval st, end;
	do_gettimeofday(&st);
#endif

	FIMC_BUG(!subdev);
	FIMC_BUG(!dgain);

	cis = (struct is_cis *)v4l2_get_subdevdata(subdev);

	FIMC_BUG(!cis);
	FIMC_BUG(!cis->cis_data);

	client = cis->client;
	if (unlikely(!client)) {
		err("client is NULL");
		ret = -EINVAL;
		goto p_err;
	}

	cis_data = cis->cis_data;

	short_dgain = (u16)sensor_cis_calc_dgain_code(dgain->short_val);

	if (short_dgain < cis->cis_data->min_digital_gain[0]) {
		short_dgain = cis->cis_data->min_digital_gain[0];
	}
	if (short_dgain > cis->cis_data->max_digital_gain[0]) {
		short_dgain = cis->cis_data->max_digital_gain[0];
	}

	dbg_sensor(1, "[MOD:D:%d] %s(vsync cnt = %d), input_short_dgain = %d us, short_dgain(%#x)\n",
			cis->id, __func__, cis->cis_data->sen_vsync_count, dgain->short_val, short_dgain);

	hold = sensor_imx686_cis_group_param_hold_func(subdev, 0x01);
	if (hold < 0) {
		ret = hold;
		goto p_err;
	}

	I2C_MUTEX_LOCK(cis->i2c_lock);

	/*
	 * 0x020E [15:8] - range 1d to 15d
	 * 0x020F [7:0] - range 0d to 255d
	 */
	dgains[0] = (u8)((short_dgain & 0x0F00) >> 8);
	dgains[1] = (u8)(short_dgain & 0xFF);

	/* Short digital gain */
	ret = is_sensor_write8_array(client, SENSOR_IMX686_DIG_GAIN_ADDR, dgains, 2);
	if (ret < 0)
		goto i2c_err;

#ifdef DEBUG_SENSOR_TIME
	do_gettimeofday(&end);
	dbg_sensor(1, "[%s] time %lu us\n", __func__, (end.tv_sec - st.tv_sec)*1000000 + (end.tv_usec - st.tv_usec));
#endif

i2c_err:
	I2C_MUTEX_UNLOCK(cis->i2c_lock);

p_err:
	if (hold > 0) {
		hold = sensor_imx686_cis_group_param_hold_func(subdev, 0x00);
		if (hold < 0)
			ret = hold;
	}

	return ret;
}

int sensor_imx686_cis_get_digital_gain(struct v4l2_subdev *subdev, u32 *dgain)
{
	int ret = 0;
	int hold = 0;
	struct is_cis *cis;
	struct i2c_client *client;

	u16 digital_gain = 0;

#ifdef DEBUG_SENSOR_TIME
	struct timeval st, end;
	do_gettimeofday(&st);
#endif

	FIMC_BUG(!subdev);
	FIMC_BUG(!dgain);

	cis = (struct is_cis *)v4l2_get_subdevdata(subdev);

	FIMC_BUG(!cis);

	client = cis->client;
	if (unlikely(!client)) {
		err("client is NULL");
		ret = -EINVAL;
		goto p_err;
	}

	hold = sensor_imx686_cis_group_param_hold_func(subdev, 0x01);
	if (hold < 0) {
		ret = hold;
		goto p_err;
	}

	I2C_MUTEX_LOCK(cis->i2c_lock);

	ret = is_sensor_read16(client, SENSOR_IMX686_DIG_GAIN_ADDR, &digital_gain);
	if (ret < 0)
		goto i2c_err;

	*dgain = sensor_imx686_cis_calc_dgain_permile(digital_gain);

	dbg_sensor(1, "[MOD:D:%d] %s, dgain_permile = %d, dgain_code(%#x)\n",
			cis->id, __func__, *dgain, digital_gain);

#ifdef DEBUG_SENSOR_TIME
	do_gettimeofday(&end);
	dbg_sensor(1, "[%s] time %lu us\n", __func__, (end.tv_sec - st.tv_sec)*1000000 + (end.tv_usec - st.tv_usec));
#endif

i2c_err:
	I2C_MUTEX_UNLOCK(cis->i2c_lock);

p_err:
	if (hold > 0) {
		hold = sensor_imx686_cis_group_param_hold_func(subdev, 0x00);
		if (hold < 0)
			ret = hold;
	}

	return ret;
}

int sensor_imx686_cis_get_min_digital_gain(struct v4l2_subdev *subdev, u32 *min_dgain)
{
	int ret = 0;
	struct is_cis *cis;
	cis_shared_data *cis_data;
	u16 min_dgain_code = SENSOR_IMX686_MIN_DIGITAL_GAIN_SET_VALUE;

#ifdef DEBUG_SENSOR_TIME
	struct timeval st, end;
	do_gettimeofday(&st);
#endif

	FIMC_BUG(!subdev);
	FIMC_BUG(!min_dgain);

	cis = (struct is_cis *)v4l2_get_subdevdata(subdev);

	FIMC_BUG(!cis);
	FIMC_BUG(!cis->cis_data);

	cis_data = cis->cis_data;

	cis_data->min_digital_gain[0] = min_dgain_code;
	cis_data->min_digital_gain[1] = sensor_imx686_cis_calc_dgain_permile(min_dgain_code);

	*min_dgain = cis_data->min_digital_gain[1];

	dbg_sensor(1, "[%s] min_dgain_code %d, min_dgain_permile %d\n", __func__,
		cis_data->min_digital_gain[0], cis_data->min_digital_gain[1]);

#ifdef DEBUG_SENSOR_TIME
	do_gettimeofday(&end);
	dbg_sensor(1, "[%s] time %lu us\n", __func__, (end.tv_sec - st.tv_sec)*1000000 + (end.tv_usec - st.tv_usec));
#endif

	return ret;
}

int sensor_imx686_cis_get_max_digital_gain(struct v4l2_subdev *subdev, u32 *max_dgain)
{
	int ret = 0;
	struct is_cis *cis;
	cis_shared_data *cis_data;
	u16 max_dgain_code = SENSOR_IMX686_MAX_DIGITAL_GAIN_SET_VALUE;

#ifdef DEBUG_SENSOR_TIME
	struct timeval st, end;
	do_gettimeofday(&st);
#endif

	FIMC_BUG(!subdev);
	FIMC_BUG(!max_dgain);

	cis = (struct is_cis *)v4l2_get_subdevdata(subdev);

	FIMC_BUG(!cis);
	FIMC_BUG(!cis->cis_data);

	cis_data = cis->cis_data;

	cis_data->max_digital_gain[0] = max_dgain_code;
	cis_data->max_digital_gain[1] = sensor_imx686_cis_calc_dgain_permile(max_dgain_code);

	*max_dgain = cis_data->max_digital_gain[1];

	dbg_sensor(1, "[%s] max_dgain_code %d, max_dgain_permile %d\n", __func__,
		cis_data->max_digital_gain[0], cis_data->max_digital_gain[1]);

#ifdef DEBUG_SENSOR_TIME
	do_gettimeofday(&end);
	dbg_sensor(1, "[%s] time %lu us\n", __func__, (end.tv_sec - st.tv_sec)*1000000 + (end.tv_usec - st.tv_usec));
#endif

	return ret;
}

void sensor_imx686_cis_data_calc(struct v4l2_subdev *subdev, u32 mode)
{
	int ret = 0;
	struct is_cis *cis = NULL;

	FIMC_BUG_VOID(!subdev);

	cis = (struct is_cis *)v4l2_get_subdevdata(subdev);
	FIMC_BUG_VOID(!cis);
	FIMC_BUG_VOID(!cis->cis_data);

	if (mode > sensor_imx686_max_setfile_num) {
		err("invalid mode(%d)!!", mode);
		return;
	}

	/* If check_rev fail when cis_init, one more check_rev in mode_change */
	if (cis->rev_flag == true) {
		cis->rev_flag = false;
		ret = sensor_imx686_cis_check_rev(cis);
		if (ret < 0) {
			err("sensor_imx686_check_rev is fail: ret(%d)", ret);
			return;
		}
	}

	sensor_imx686_cis_data_calculation(sensor_imx686_pllinfos[mode], cis->cis_data);
}

int sensor_imx686_cis_set_adjust_sync(struct v4l2_subdev *subdev, u32 sync)
{
	int ret = 0;

	return ret;
}

int sensor_imx686_cis_recover_stream_on(struct v4l2_subdev *subdev)
{
	int ret = 0;

	return ret;
}

int sensor_imx686_cis_set_wb_gain(struct v4l2_subdev *subdev, struct wb_gains wb_gains)
{
#define ABS_WB_GAIN_NUM	4
	int ret = 0;
	int hold = 0;
	int mode = 0;
	int index = 0;
	struct is_cis *cis;
	struct i2c_client *client;
	u16 abs_gains[ABS_WB_GAIN_NUM] = {0, };	//[0]=gr, [1]=r, [2]=b, [3]=gb

#ifdef DEBUG_SENSOR_TIME
	struct timeval st, end;
	do_gettimeofday(&st);
#endif

	FIMC_BUG(!subdev);

	cis = (struct is_cis *)v4l2_get_subdevdata(subdev);

	FIMC_BUG(!cis);
	FIMC_BUG(!cis->cis_data);

	if (!cis->use_wb_gain)
		return ret;

	client = cis->client;
	if (unlikely(!client)) {
		err("client is NULL");
		ret = -EINVAL;
		goto p_err;
	}

	mode = cis->cis_data->sens_config_index_cur;

	if (mode < SENSOR_IMX686_REMOSAIC_START || mode > SENSOR_IMX686_REMOSAIC_END)
		return 0;

	dbg_sensor(1, "[SEN:%d]%s:DDK vlaue: wb_gain_gr(%d), wb_gain_r(%d), wb_gain_b(%d), wb_gain_gb(%d)\n",
		cis->id, __func__, wb_gains.gr, wb_gains.r, wb_gains.b, wb_gains.gb);

	abs_gains[0] = (u16)((wb_gains.gr / 4) & 0xFFFF);
	abs_gains[1] = (u16)((wb_gains.r / 4) & 0xFFFF);
	abs_gains[2] = (u16)((wb_gains.b / 4) & 0xFFFF);
	abs_gains[3] = (u16)((wb_gains.gb / 4) & 0xFFFF);

	dbg_sensor(1, "[SEN:%d]%s: abs_gain_gr(0x%4X), abs_gain_r(0x%4X), abs_gain_b(0x%4X), abs_gain_gb(0x%4X)\n",
		cis->id, __func__, abs_gains[0], abs_gains[1], abs_gains[2], abs_gains[3]);

	hold = sensor_imx686_cis_group_param_hold(subdev, 0x01);
	if (hold < 0) {
		ret = hold;
		goto p_err;
	}

	I2C_MUTEX_LOCK(cis->i2c_lock);
	for (index=0; index < ABS_WB_GAIN_NUM; index++) {
		ret = is_sensor_write16(client, (SENSOR_IMX686_ABS_GAIN_GR_SET_ADDR + (index * 2)), abs_gains[index]);
		if (ret < 0) {
			err("i2c fail addr(%x), val(%04x), ret = %d\n",
				SENSOR_IMX686_ABS_GAIN_GR_SET_ADDR + (index * 2), abs_gains[index], ret);
			goto i2c_err;
		}
	}

#ifdef DEBUG_SENSOR_TIME
	do_gettimeofday(&end);
	dbg_sensor(1, "[%s] time %lu us\n", __func__, (end.tv_sec - st.tv_sec)*1000000 + (end.tv_usec - st.tv_usec));
#endif

i2c_err:
	I2C_MUTEX_UNLOCK(cis->i2c_lock);

p_err:
	if (hold > 0) {
		hold = sensor_imx686_cis_group_param_hold(subdev, 0x00);
		if (hold < 0)
			ret = hold;
	}

	return ret;
}

static struct is_cis_ops cis_ops_imx686 = {
	.cis_init = sensor_imx686_cis_init,
	.cis_log_status = sensor_imx686_cis_log_status,
	.cis_group_param_hold = sensor_imx686_cis_group_param_hold,
	.cis_set_global_setting = sensor_imx686_cis_set_global_setting,
	.cis_mode_change = sensor_imx686_cis_mode_change,
	.cis_set_size = sensor_imx686_cis_set_size,
	.cis_stream_on = sensor_imx686_cis_stream_on,
	.cis_stream_off = sensor_imx686_cis_stream_off,
	.cis_adjust_frame_duration = sensor_imx686_cis_adjust_frame_duration,
	.cis_set_frame_duration = sensor_imx686_cis_set_frame_duration,
	.cis_set_frame_rate = sensor_imx686_cis_set_frame_rate,
	.cis_get_min_exposure_time = sensor_imx686_cis_get_min_exposure_time,
	.cis_get_max_exposure_time = sensor_imx686_cis_get_max_exposure_time,
	.cis_set_exposure_time = sensor_imx686_cis_set_exposure_time,
	.cis_get_min_analog_gain = sensor_imx686_cis_get_min_analog_gain,
	.cis_get_max_analog_gain = sensor_imx686_cis_get_max_analog_gain,
	.cis_adjust_analog_gain = sensor_imx686_cis_adjust_analog_gain,
	.cis_set_analog_gain = sensor_imx686_cis_set_analog_gain,
	.cis_get_analog_gain = sensor_imx686_cis_get_analog_gain,
	.cis_get_min_digital_gain = sensor_imx686_cis_get_min_digital_gain,
	.cis_get_max_digital_gain = sensor_imx686_cis_get_max_digital_gain,
	.cis_set_digital_gain = sensor_imx686_cis_set_digital_gain,
	.cis_get_digital_gain = sensor_imx686_cis_get_digital_gain,
	.cis_compensate_gain_for_extremely_br = sensor_cis_compensate_gain_for_extremely_br,
	.cis_wait_streamoff = sensor_cis_wait_streamoff,
	.cis_wait_streamon = sensor_cis_wait_streamon,
	.cis_data_calculation = sensor_imx686_cis_data_calc,
	.cis_set_adjust_sync = sensor_imx686_cis_set_adjust_sync,
	.cis_check_rev_on_init = sensor_imx686_cis_check_rev_on_init,
	.cis_set_initial_exposure = sensor_cis_set_initial_exposure,
	.cis_recover_stream_on = sensor_imx686_cis_recover_stream_on,
	.cis_set_wb_gains = sensor_imx686_cis_set_wb_gain,
};

int cis_imx686_probe(struct i2c_client *client,
	const struct i2c_device_id *id)
{
	int ret = 0;
	bool use_pdaf = false;
	struct is_core *core = NULL;
	struct v4l2_subdev *subdev_cis = NULL;
	struct is_cis *cis = NULL;
	struct is_device_sensor *device = NULL;
	struct is_device_sensor_peri *sensor_peri = NULL;
	u32 sensor_id = 0;
	char const *setfile;
	struct device *dev;
	struct device_node *dnode;

	FIMC_BUG(!client);
	FIMC_BUG(!is_dev);

	core = (struct is_core *)dev_get_drvdata(is_dev);
	if (!core) {
		probe_info("core device is not yet probed");
		return -EPROBE_DEFER;
	}

	dev = &client->dev;
	dnode = dev->of_node;

	ret = of_property_read_u32(dnode, "id", &sensor_id);
	if (ret) {
		err("sensor id read is fail(%d)", ret);
		goto p_err;
	}

	probe_info("%s sensor id %d\n", __func__, sensor_id);

	device = &core->sensor[sensor_id];

	sensor_peri = find_peri_by_cis_id(device, SENSOR_NAME_IMX686);
	if (!sensor_peri) {
		probe_info("sensor peri is not yet probed");
		return -EPROBE_DEFER;
	}

	cis = &sensor_peri->cis;
	if (!cis) {
		err("cis is NULL");
		ret = -ENOMEM;
		goto p_err;
	}

	subdev_cis = kzalloc(sizeof(struct v4l2_subdev), GFP_KERNEL);
	if (!subdev_cis) {
		probe_err("subdev_cis is NULL");
		ret = -ENOMEM;
		goto p_err;
	}
	sensor_peri->subdev_cis = subdev_cis;

	cis->id = SENSOR_NAME_IMX686;
	cis->subdev = subdev_cis;
	cis->device = sensor_id;
	cis->client = client;
	sensor_peri->module->client = cis->client;
	cis->i2c_lock = NULL;
	cis->ctrl_delay = N_PLUS_TWO_FRAME;

	cis->cis_data = kzalloc(sizeof(cis_shared_data), GFP_KERNEL);
	if (!cis->cis_data) {
		err("cis_data is NULL");
		ret = -ENOMEM;
		goto p_err;
	}

	cis->cis_ops = &cis_ops_imx686;

	/* belows are depend on sensor cis. MUST check sensor spec */
	cis->bayer_order = OTF_INPUT_ORDER_BAYER_RG_GB;

	if (of_property_read_bool(dnode, "use_pdaf")) {
		use_pdaf = true;
	}
	probe_info("%s use_pdaf %d\n", __func__, use_pdaf);

	if (of_property_read_bool(dnode, "sensor_f_number")) {
		ret = of_property_read_u32(dnode, "sensor_f_number", &cis->aperture_num);
		if (ret) {
			warn("f-number read is fail(%d)", ret);
		}
	} else {
		cis->aperture_num = F1_8;
	}
	probe_info("%s f-number %d\n", __func__, cis->aperture_num);

	cis->use_dgain = true;
	cis->hdr_ctrl_by_again = false;
	cis->use_wb_gain = true;
	cis->use_pdaf = use_pdaf;

	v4l2_set_subdevdata(subdev_cis, cis);
	v4l2_set_subdev_hostdata(subdev_cis, device);
	snprintf(subdev_cis->name, V4L2_SUBDEV_NAME_SIZE, "cis-subdev.%d", cis->id);

	ret = of_property_read_string(dnode, "setfile", &setfile);
	if (ret) {
		err("setfile index read fail(%d), take default setfile!!", ret);
		setfile = "default";
	}

	if (strcmp(setfile, "default") == 0 || strcmp(setfile, "setA") == 0) {
		probe_info("%s setfile_A\n", __func__);
		sensor_imx686_global = sensor_imx686_setfile_A_global;
		sensor_imx686_global_size = ARRAY_SIZE(sensor_imx686_setfile_A_global);
		sensor_imx686_setfiles = sensor_imx686_setfiles_A;
		sensor_imx686_setfile_sizes = sensor_imx686_setfile_A_sizes;
		sensor_imx686_pllinfos = sensor_imx686_pllinfos_A;
		sensor_imx686_max_setfile_num = ARRAY_SIZE(sensor_imx686_setfiles_A);
	} else if (strcmp(setfile, "setB") == 0) {
		probe_info("%s setfile_B\n", __func__);
		sensor_imx686_global = sensor_imx686_setfile_B_global;
		sensor_imx686_global_size = ARRAY_SIZE(sensor_imx686_setfile_B_global);
		sensor_imx686_setfiles = sensor_imx686_setfiles_B;
		sensor_imx686_setfile_sizes = sensor_imx686_setfile_B_sizes;
		sensor_imx686_pllinfos = sensor_imx686_pllinfos_B;
		sensor_imx686_max_setfile_num = ARRAY_SIZE(sensor_imx686_setfiles_B);
	} else {
		err("%s setfile index out of bound, take default (setfile_B)", __func__);
		sensor_imx686_global = sensor_imx686_setfile_A_global;
		sensor_imx686_global_size = ARRAY_SIZE(sensor_imx686_setfile_A_global);
		sensor_imx686_setfiles = sensor_imx686_setfiles_A;
		sensor_imx686_setfile_sizes = sensor_imx686_setfile_A_sizes;
		sensor_imx686_pllinfos = sensor_imx686_pllinfos_A;
		sensor_imx686_max_setfile_num = ARRAY_SIZE(sensor_imx686_setfiles_A);
	}

	probe_info("%s done\n", __func__);

p_err:
	return ret;
}

static const struct of_device_id sensor_cis_imx686_match[] = {
	{
		.compatible = "samsung,exynos-is-cis-imx686",
	},
	{},
};
MODULE_DEVICE_TABLE(of, sensor_cis_imx686_match);

static const struct i2c_device_id sensor_cis_imx686_idt[] = {
	{ SENSOR_NAME, 0 },
	{},
};

static struct i2c_driver sensor_cis_imx686_driver = {
	.probe	= cis_imx686_probe,
	.driver = {
		.name	= SENSOR_NAME,
		.owner	= THIS_MODULE,
		.of_match_table = sensor_cis_imx686_match,
		.suppress_bind_attrs = true,
	},
	.id_table = sensor_cis_imx686_idt
};

static int __init sensor_cis_imx686_init(void)
{
	int ret;

	ret = i2c_add_driver(&sensor_cis_imx686_driver);
	if (ret)
		err("failed to add %s driver: %d\n",
			sensor_cis_imx686_driver.driver.name, ret);

	return ret;
}
late_initcall_sync(sensor_cis_imx686_init);

MODULE_LICENSE("GPL");
