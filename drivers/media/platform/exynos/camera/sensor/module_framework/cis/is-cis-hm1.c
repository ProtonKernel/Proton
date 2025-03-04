/*
 * Samsung Exynos SoC series Sensor driver
 *
 *
 * Copyright (c) 2016 Samsung Electronics Co., Ltd
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
#include "is-cis-hm1.h"
#include "is-cis-hm1-setA.h"
#include "is-helper-i2c.h"
#include "is-sec-define.h"

#define SENSOR_NAME "S5KHM1"
/* #define DEBUG_HM1_PLL */
/* #define GLOBAL_TIME_CAL_WRITE */
/* #define DEBUG_CAL_WRITE */

static const u32 *sensor_hm1_initial;
static u32 sensor_hm1_initial_size;
static const u32 *sensor_hm1_tnp;
static u32 sensor_hm1_tnp_size;
static const u32 *sensor_hm1_global;
static u32 sensor_hm1_global_size;
static const u32 *sensor_hm1_global_for_cal;
static u32 sensor_hm1_global_for_cal_size;
static const u32 **sensor_hm1_setfiles;
static const u32 *sensor_hm1_setfile_sizes;
static const struct sensor_pll_info_compact **sensor_hm1_pllinfos;
static u32 sensor_hm1_max_setfile_num;
static bool sensor_hm1_fake_retention_status;

/* For Recovery */
static u32 sensor_hm1_frame_duration_backup;
static struct ae_param sensor_hm1_again_backup;
static struct ae_param sensor_hm1_dgain_backup;
static struct ae_param sensor_hm1_target_exp_backup;

static bool sensor_hm1_eeprom_cal_available;
static bool sensor_hm1_first_entrance;

int sensor_hm1_cis_set_global_setting(struct v4l2_subdev *subdev);

static bool sensor_hm1_cis_is_wdr_mode_on(cis_shared_data *cis_data)
{
	unsigned int mode = cis_data->sens_config_index_cur;

	if (!is_vender_wdr_mode_on(cis_data))
		return false;

	if (mode < 0 || mode >= SENSOR_HM1_MODE_MAX) {
		err("invalid mode(%d)!!", mode);
		return false;
	}

	return sensor_hm1_support_wdr[mode];
}

static void sensor_hm1_set_integration_max_margin(u32 mode, cis_shared_data *cis_data)
{
	WARN_ON(!cis_data);

	switch (mode) {
	case SENSOR_HM1_12000X9000_10FPS:
	case SENSOR_HM1_7200X5400_14FPS:
	case SENSOR_HM1_4000X2252_30FPS_CENTER_CROP:
	case SENSOR_HM1_1984X1488_30FPS:
	case SENSOR_HM1_1920X1080_240FPS:
	case SENSOR_HM1_1920X1080_120FPS:
	case SENSOR_HM1_992X744_120FPS:
		cis_data->max_margin_coarse_integration_time = 0x8;
		break;
	case SENSOR_HM1_7680X4320_24FPS:
		cis_data->max_margin_coarse_integration_time = 0x2;
		break;
	case SENSOR_HM1_4000X3000_30FPS:
	case SENSOR_HM1_4000X3000_30FPS_DECOMP_TIMING:
	case SENSOR_HM1_4000X3000_30FPS_FULL_DILLUTION:
	case SENSOR_HM1_4000X3000_30FPS_SHORT:
	case SENSOR_HM1_4000X3000_30FPS_SHORT_DECOMP_TIMING:
	case SENSOR_HM1_4000X3000_30FPS_SHORT_FULL_DILLUTION:
	case SENSOR_HM1_4000X2252_30FPS:
	case SENSOR_HM1_4000X2252_30FPS_FULL_DILLUTION:
	case SENSOR_HM1_4000X2252_30FPS_SHORT:
	case SENSOR_HM1_4000X3000_60FPS:
	case SENSOR_HM1_4000X2252_60FPS:
	case SENSOR_HM1_3840X2160_120FPS:
	case SENSOR_HM1_3328X1872_120FPS:
		cis_data->max_margin_coarse_integration_time = SENSOR_HM1_COARSE_INTEGRATION_TIME_MAX_MARGIN;
		break;
	default:
		err("[%s] Unsupport hm1 sensor mode\n", __func__);
		cis_data->max_margin_coarse_integration_time = SENSOR_HM1_COARSE_INTEGRATION_TIME_MAX_MARGIN;
		break;
	}

	dbg_sensor(1, "max_margin_coarse_integration_time(%d)\n", cis_data->max_margin_coarse_integration_time);
}

static void sensor_hm1_cis_data_calculation(const struct sensor_pll_info_compact *pll_info_compact,
						cis_shared_data *cis_data)
{
	u64 vt_pix_clk_hz;
	u32 frame_rate, max_fps, frame_valid_us;

	WARN_ON(!pll_info_compact);

	/* 1. get pclk value from pll info */
	vt_pix_clk_hz = pll_info_compact->pclk;

	dbg_sensor(1, "ext_clock(%d), mipi_datarate(%llu), pclk(%llu)\n",
			pll_info_compact->ext_clk, pll_info_compact->mipi_datarate, pll_info_compact->pclk);

	/* 2. the time of processing one frame calculation (us) */
	cis_data->min_frame_us_time = (((u64)pll_info_compact->frame_length_lines) * pll_info_compact->line_length_pck * 1000
					/ (vt_pix_clk_hz / 1000));
	cis_data->cur_frame_us_time = cis_data->min_frame_us_time;
#ifdef CAMERA_REAR2
	cis_data->min_sync_frame_us_time = cis_data->min_frame_us_time;
#endif
	/* 3. FPS calculation */
	frame_rate = vt_pix_clk_hz / (pll_info_compact->frame_length_lines * pll_info_compact->line_length_pck);
	dbg_sensor(1, "frame_rate (%d) = vt_pix_clk_hz(%llu) / "
		KERN_CONT "(pll_info_compact->frame_length_lines(%d) * pll_info_compact->line_length_pck(%d))\n",
		frame_rate, vt_pix_clk_hz, pll_info_compact->frame_length_lines, pll_info_compact->line_length_pck);

	/* calculate max fps */
	max_fps = (vt_pix_clk_hz * 10) / (pll_info_compact->frame_length_lines * pll_info_compact->line_length_pck);
	max_fps = (max_fps % 10 >= 5 ? frame_rate + 1 : frame_rate);

	cis_data->pclk = vt_pix_clk_hz;
	cis_data->max_fps = max_fps;
	cis_data->frame_length_lines = pll_info_compact->frame_length_lines;
	cis_data->line_length_pck = pll_info_compact->line_length_pck;
	cis_data->line_readOut_time = (u64)cis_data->line_length_pck * 1000
					* 1000 * 1000 / cis_data->pclk;
	cis_data->rolling_shutter_skew = (cis_data->cur_height - 1) * cis_data->line_readOut_time;
	cis_data->stream_on = false;

	/* Frame valid time calcuration */
	frame_valid_us = (u64)cis_data->cur_height * cis_data->line_length_pck
				* 1000 * 1000 / cis_data->pclk;
	cis_data->frame_valid_us_time = (unsigned int)frame_valid_us;

	dbg_sensor(1, "%s\n", __func__);
	dbg_sensor(1, "Sensor size(%d x %d) setting: SUCCESS!\n",
					cis_data->cur_width, cis_data->cur_height);
	dbg_sensor(1, "Frame Valid(us): %d\n", frame_valid_us);
	dbg_sensor(1, "rolling_shutter_skew: %lld\n", cis_data->rolling_shutter_skew);

	dbg_sensor(1, "Fps: %d, max fps(%d)\n", frame_rate, cis_data->max_fps);
	dbg_sensor(1, "min_frame_time(%d us)\n", cis_data->min_frame_us_time);
	dbg_sensor(1, "Pixel rate(Kbps): %d\n", cis_data->pclk / 1000);

	/* Frame period calculation */
	cis_data->frame_time = (cis_data->line_readOut_time * cis_data->cur_height / 1000);
	cis_data->rolling_shutter_skew = (cis_data->cur_height - 1) * cis_data->line_readOut_time;

	dbg_sensor(1, "[%s] frame_time(%d), rolling_shutter_skew(%lld)\n", __func__,
		cis_data->frame_time, cis_data->rolling_shutter_skew);

	/* Constant values */
	cis_data->min_fine_integration_time = SENSOR_HM1_FINE_INTEGRATION_TIME_MIN;
	cis_data->max_fine_integration_time = SENSOR_HM1_FINE_INTEGRATION_TIME_MAX;
	cis_data->min_coarse_integration_time = SENSOR_HM1_COARSE_INTEGRATION_TIME_MIN;
}

void sensor_hm1_cis_data_calc(struct v4l2_subdev *subdev, u32 mode)
{
	struct is_cis *cis = NULL;

	WARN_ON(!subdev);

	cis = (struct is_cis *)v4l2_get_subdevdata(subdev);
	WARN_ON(!cis);
	WARN_ON(!cis->cis_data);

	if (mode >= sensor_hm1_max_setfile_num) {
		err("invalid mode(%d)!!", mode);
		return;
	}

	if (cis->cis_data->stream_on) {
		info("[%s] call mode change in stream on state\n", __func__);
		sensor_cis_wait_streamon(subdev);
		sensor_hm1_cis_stream_off(subdev);
		sensor_cis_wait_streamoff(subdev);
		info("[%s] stream off done\n", __func__);
	}

	sensor_hm1_cis_data_calculation(sensor_hm1_pllinfos[mode], cis->cis_data);
}

static int sensor_hm1_wait_stream_off_status(cis_shared_data *cis_data)
{
	int ret = 0;
	u32 timeout = 0;

	WARN_ON(!cis_data);

#define STREAM_OFF_WAIT_TIME 250
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

int sensor_hm1_cis_select_setfile(struct v4l2_subdev *subdev)
{
	int ret = 0;
	u16 rev = 0;
	struct is_cis *cis = NULL;

	WARN_ON(!subdev);

	cis = (struct is_cis *)v4l2_get_subdevdata(subdev);
	WARN_ON(!cis);
	WARN_ON(!cis->cis_data);

	rev = cis->cis_data->cis_rev;

	info("hm1 sensor revision(0x%#x)\n", rev);

	if(rev >= 0xA101) {
		info("hm1 sensor OTP\n");
		sensor_hm1_tnp = sensor_hm1_setfile_A_TnP_OTP;
		sensor_hm1_tnp_size  = ARRAY_SIZE(sensor_hm1_setfile_A_TnP_OTP);
	}

	return ret;
}

/* CIS OPS */
int sensor_hm1_cis_init(struct v4l2_subdev *subdev)
{
	int ret = 0;
	struct is_cis *cis;
	u32 setfile_index = 0;
	cis_setting_info setinfo;
	setinfo.param = NULL;
	setinfo.return_value = 0;

	WARN_ON(!subdev);

	cis = (struct is_cis *)v4l2_get_subdevdata(subdev);
	if (!cis) {
		err("cis is NULL");
		ret = -EINVAL;
		goto p_err;
	}

	WARN_ON(!cis->cis_data);
#if !defined(CONFIG_CAMERA_VENDER_MCD)
	memset(cis->cis_data, 0, sizeof(cis_shared_data));

	ret = sensor_cis_check_rev(cis);
	if (ret < 0) {
		warn("sensor_hm1_check_rev is fail when cis init");
		ret = -EINVAL;
		goto p_err;
	}
#endif

	sensor_hm1_eeprom_cal_available = false;
	sensor_hm1_first_entrance = true;
	cis->cis_data->sens_config_index_pre = SENSOR_HM1_MODE_MAX;

	sensor_hm1_cis_select_setfile(subdev);

	cis->cis_data->stream_on = false;
	cis->cis_data->product_name = cis->id;
	cis->cis_data->cur_width = SENSOR_HM1_MAX_WIDTH;
	cis->cis_data->cur_height = SENSOR_HM1_MAX_HEIGHT;
	cis->cis_data->low_expo_start = 33000;
	cis->cis_data->highres_capture_mode = false;
	cis->need_mode_change = false;
	cis->long_term_mode.sen_strm_off_on_step = 0;
	cis->long_term_mode.sen_strm_off_on_enable = false;
	cis->mipi_clock_index_cur = CAM_MIPI_NOT_INITIALIZED;
	cis->mipi_clock_index_new = CAM_MIPI_NOT_INITIALIZED;

	sensor_hm1_cis_data_calculation(sensor_hm1_pllinfos[setfile_index], cis->cis_data);
	sensor_hm1_set_integration_max_margin(setfile_index, cis->cis_data);

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

	/* CALL_CISOPS(cis, cis_log_status, subdev); */

#ifdef DEBUG_SENSOR_TIME
	do_gettimeofday(&end);
	dbg_sensor(1, "[%s] time %lu us\n", __func__, (end.tv_sec - st.tv_sec)*1000000 + (end.tv_usec - st.tv_usec));
#endif

p_err:
	return ret;
}

int sensor_hm1_cis_log_status(struct v4l2_subdev *subdev)
{
	int ret = 0;
	struct is_cis *cis;
	struct i2c_client *client = NULL;
	u8 data8 = 0;
	u16 data16 = 0;

	WARN_ON(!subdev);

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
	ret = is_sensor_write16(client, 0x6000, 0x0005);
	if (unlikely(!ret)) pr_info("page unlock\n");
	else goto i2c_err;
	ret = is_sensor_write16(client, 0xFCFC, 0x4000);
	if (unlikely(!ret)) pr_info("0x4000 page\n");
	else goto i2c_err;
	ret = is_sensor_read16(client, 0x0000, &data16);
	if (unlikely(!ret)) pr_info("model_id(0x%x)\n", data16);
	else goto i2c_err;
	ret = is_sensor_read16(client, 0x0002, &data16);
	if (unlikely(!ret)) pr_info("revision_number(0x%x)\n", data16);
	else goto i2c_err;
	ret = is_sensor_read8(client, 0x0004, &data8);
	if (unlikely(!ret)) pr_info("0x0004(0x%x)\n", data8);
	else goto i2c_err;
	ret = is_sensor_read8(client, 0x0005, &data8);
	if (unlikely(!ret)) pr_info("frame_count(0x%x)\n", data8);
	else goto i2c_err;
	ret = is_sensor_read16(client, 0x001A, &data16);
	if (unlikely(!ret)) pr_info("0x001A(0x%x)\n", data16);
	else goto i2c_err;
	ret = is_sensor_read8(client, 0x0100, &data8);
	if (unlikely(!ret)) pr_info("0x0100(0x%x)\n", data8);
	else goto i2c_err;
	ret = is_sensor_read8(client, 0x0118, &data8);
	if (unlikely(!ret)) pr_info("0x0118(0x%x)\n", data8);
	else goto i2c_err;
	ret = is_sensor_read16(client, 0x021E, &data16);
	if (unlikely(!ret)) pr_info("0x021E(0x%x)\n", data16);
	else goto i2c_err;
	ret = is_sensor_read16(client, 0x0202, &data16);
	if (unlikely(!ret)) pr_info("0x0202(0x%x)\n", data16);
	else goto i2c_err;
	ret = is_sensor_read16(client, 0x0226, &data16);
	if (unlikely(!ret)) pr_info("0x0226(0x%x)\n", data16);
	else goto i2c_err;
	ret = is_sensor_read16(client, 0x0702, &data16);
	if (unlikely(!ret)) pr_info("0x0702(0x%x)\n", data16);
	else goto i2c_err;
	ret = is_sensor_read16(client, 0x0704, &data16);
	if (unlikely(!ret)) pr_info("0x0704(0x%x)\n", data16);
	else goto i2c_err;
	ret = is_sensor_read16(client, 0x0204, &data16);
	if (unlikely(!ret)) pr_info("0x0204(0x%x)\n", data16);
	else goto i2c_err;
	ret = is_sensor_read16(client, 0x020E, &data16);
	if (unlikely(!ret)) pr_info("0x020E(0x%x)\n", data16);
	else goto i2c_err;
	ret = is_sensor_read16(client, 0x0230, &data16);
	if (unlikely(!ret)) pr_info("0x0230(0x%x)\n", data16);
	else goto i2c_err;
	ret = is_sensor_read16(client, 0x0340, &data16);
	if (unlikely(!ret)) pr_info("0x0340(0x%x)\n", data16);
	else goto i2c_err;
	ret = is_sensor_read16(client, 0x0342, &data16);
	if (unlikely(!ret)) pr_info("0x0342(0x%x)\n", data16);
	else goto i2c_err;
	ret = is_sensor_write16(client, 0xFCFC, 0x2000);
	if (unlikely(!ret)) pr_info("0x2000 page\n");
	else goto i2c_err;
	{
		int i = 0;
		for (i = 0; i < 96; i += 2) {
			ret = is_sensor_read16(client, 0xE580 + i, &data16);
			if (unlikely(!ret)) pr_info("0x%X(0x%X)\n", 0xE580 + i, data16);
		}
	}
	ret = is_sensor_write16(client, 0xFCFC, 0x4000);
	if (unlikely(!ret)) pr_info("0x4000 page\n");
	else goto i2c_err;
	ret = is_sensor_write16(client, 0x6000, 0x0085);
	if (unlikely(!ret)) pr_info("page lock\n");
	else goto i2c_err;
	pr_info("[%s] *******************************\n", __func__);

i2c_err:
	I2C_MUTEX_UNLOCK(cis->i2c_lock);
p_err:
	return ret;
}

#if USE_GROUP_PARAM_HOLD
static int sensor_hm1_cis_group_param_hold_func(struct v4l2_subdev *subdev, unsigned int hold)
{
	int ret = 0;
	struct is_cis *cis = NULL;
	struct i2c_client *client = NULL;

	WARN_ON(!subdev);

	cis = (struct is_cis *)v4l2_get_subdevdata(subdev);

	WARN_ON(!cis);
	WARN_ON(!cis->cis_data);

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

	ret = is_sensor_write8(client, 0x0104, hold); /* api_rw_general_setup_grouped_parameter_hold */
	if (ret < 0)
		goto p_err;

	cis->cis_data->group_param_hold = hold;
	ret = 1;
p_err:
	return ret;
}
#else
static inline int sensor_hm1_cis_group_param_hold_func(struct v4l2_subdev *subdev, unsigned int hold)
{ return 0; }
#endif

/* Input
 *	hold : true - hold, flase - no hold
 * Output
 *      return: 0 - no effect(already hold or no hold)
 *		positive - setted by request
 *		negative - ERROR value
 */
int sensor_hm1_cis_group_param_hold(struct v4l2_subdev *subdev, bool hold)
{
	int ret = 0;
	struct is_cis *cis = NULL;

	WARN_ON(!subdev);

	cis = (struct is_cis *)v4l2_get_subdevdata(subdev);

	WARN_ON(!cis);
	WARN_ON(!cis->cis_data);

	I2C_MUTEX_LOCK(cis->i2c_lock);
	ret = sensor_hm1_cis_group_param_hold_func(subdev, hold);
	if (ret < 0)
		goto p_err;

p_err:
	I2C_MUTEX_UNLOCK(cis->i2c_lock);
	return ret;
}

static void sensor_hm1_cis_set_paf_stat_enable(u32 mode, cis_shared_data *cis_data)
{
	WARN_ON(!cis_data);

	switch (mode) {
	case SENSOR_HM1_12000X9000_10FPS:
	case SENSOR_HM1_7200X5400_14FPS:
	case SENSOR_HM1_4000X2252_30FPS_CENTER_CROP:
		cis_data->is_data.paf_stat_enable = false;
		break;
	default:
		cis_data->is_data.paf_stat_enable = true;
		break;
	}
}

#ifdef HM1_BURST_WRITE
int sensor_hm1_cis_write16_burst(struct i2c_client *client, u16 addr, u8 *val, u32 num, bool endian)
{
	int ret = 0;
	struct i2c_msg msg[1];
	int i = 0;
	u8 *wbuf;

	if (val == NULL) {
		pr_err("val array is null\n");
		ret = -ENODEV;
		goto p_err;
	}

	if (!client->adapter) {
		pr_err("Could not find adapter!\n");
		ret = -ENODEV;
		goto p_err;
	}

	wbuf = kmalloc((2 + (num * 2)), GFP_KERNEL);
	if (!wbuf) {
		pr_err("failed to alloc buffer for burst i2c\n");
		ret = -ENODEV;
		goto p_err;
	}

	msg->addr = client->addr;
	msg->flags = 0;
	msg->len = 2 + (num * 2);
	msg->buf = wbuf;
	wbuf[0] = (addr & 0xFF00) >> 8;
	wbuf[1] = (addr & 0xFF);
	if (endian == HM1_BIG_ENDIAN) {
		for (i = 0; i < num; i++) {
			wbuf[(i * 2) + 2] = (val[(i * 2)] & 0xFF);
			wbuf[(i * 2) + 3] = (val[(i * 2) + 1] & 0xFF);
			i2c_info("I2CW16(%d) [0x%04x] : 0x%x%x\n", client->addr, addr, (val[(i * 2)] & 0xFF), (val[(i * 2) + 1] & 0xFF));
		}
	} else {
		for (i = 0; i < num; i++) {
			wbuf[(i * 2) + 2] = (val[(i * 2) + 1] & 0xFF);
			wbuf[(i * 2) + 3] = (val[(i * 2)] & 0xFF);
			i2c_info("I2CW16(%d) [0x%04x] : 0x%x%x\n", client->addr, addr, (val[(i * 2)] & 0xFF), (val[(i * 2) + 1] & 0xFF));
		}
	}

	ret = is_i2c_transfer(client->adapter, msg, 1);
	if (ret < 0) {
		pr_err("i2c transfer fail(%d)", ret);
		goto p_err_free;
	}

	kfree(wbuf);
	return 0;

p_err_free:
	kfree(wbuf);
p_err:
	return ret;
}
#endif

int sensor_hm1_cis_set_cal(struct v4l2_subdev *subdev)
{
	int ret = 0;
	struct is_rom_info *finfo = NULL;
	char *cal_buf = NULL;
	struct is_cis *cis = NULL;
	bool endian = HM1_BIG_ENDIAN;
	int start_addr = 0, end_addr = 0;
#ifdef HM1_BURST_WRITE
	int cal_size = 0;
#endif
	int i = 0;
	u16 val = 0;
	int len = 0;

	info("[%s] E", __func__);

	if(sensor_hm1_eeprom_cal_available)
		return 0;

	is_sec_get_sysfs_finfo(&finfo, ROM_ID_REAR);

	if (!test_bit(IS_ROM_STATE_CAL_READ_DONE, &finfo->rom_state)) {
		err("eeprom read fail status, skip set cal");
		sensor_hm1_eeprom_cal_available = false;
		return 0;
	}

	info("[%s] eeprom read, start set cal\n", __func__);
	is_sec_get_cal_buf(&cal_buf, ROM_ID_REAR);

#ifdef CAMERA_HM1_CAL_MODULE_VERSION
	if (finfo->header_ver[10] < CAMERA_HM1_CAL_MODULE_VERSION) {
		start_addr = finfo->rom_xtc_cal_data_addr_list[HM1_CAL_START_ADDR];
		if (cal_buf[start_addr + 2] == 0xFF && cal_buf[start_addr + 3] == 0xFF &&
			cal_buf[start_addr + 4] == 0xFF && cal_buf[start_addr + 5] == 0xFF &&
			cal_buf[start_addr + 6] == 0xFF && cal_buf[start_addr + 7] == 0xFF &&
			cal_buf[start_addr + 8] == 0xFF && cal_buf[start_addr + 9] == 0xFF) {
			info("empty Cal - cal offset[0x%04X] = val[0x%02X], cal offset[0x%04X] = val[0x%02X]",
				start_addr + 2, cal_buf[start_addr + 2], start_addr + 9, cal_buf[start_addr + 9]);
			info("[%s] empty Cal", __func__);
			return 0;
		}

		len = (finfo->rom_xtc_cal_data_addr_list_len / HM1_CAL_ROW_LEN) - 1;
		if (len >= 0) {
			end_addr = finfo->rom_xtc_cal_data_addr_list[len * HM1_CAL_ROW_LEN + HM1_CAL_END_ADDR];
			if (end_addr >= 7) {
				if (cal_buf[end_addr    ] == 0xFF && cal_buf[end_addr - 1] == 0xFF &&
					cal_buf[end_addr - 2] == 0xFF && cal_buf[end_addr - 3] == 0xFF &&
					cal_buf[end_addr - 4] == 0xFF && cal_buf[end_addr - 5] == 0xFF &&
					cal_buf[end_addr - 6] == 0xFF && cal_buf[end_addr - 7] == 0xFF) {
					info("empty Cal - cal offset[0x%04X] = val[0x%02X], cal offset[0x%04X] = val[0x%02X]",
						end_addr, cal_buf[end_addr], end_addr - 7, cal_buf[end_addr - 7]);
					info("[%s] empty Cal", __func__);
					return 0;
				}
			}
		}
	}
#endif

	WARN_ON(!subdev);

	cis = (struct is_cis *)v4l2_get_subdevdata(subdev);
	WARN_ON(!cis);

	if (finfo->rom_pdxtc_cal_data_addr_list_len <= 0
		|| finfo->rom_gcc_cal_data_addr_list_len <= 0
		|| finfo->rom_xtc_cal_data_addr_list_len <= 0) {
		err("Not available DT, skip set cal");
		sensor_hm1_eeprom_cal_available = false;
		return 0;
	}

	ret |= is_sensor_write16(cis->client, 0xFCFC, 0x4000);
	ret |= is_sensor_write16(cis->client, 0x6000, 0x0085);


	info("[%s] PDXTC start\n", __func__);
	start_addr = finfo->rom_pdxtc_cal_data_addr_list[HM1_CAL_START_ADDR];
	if (finfo->rom_pdxtc_cal_endian_check) {
		if (cal_buf[start_addr] == 0xFF && cal_buf[start_addr + 1] == 0x00)
			endian = HM1_BIG_ENDIAN;
		else
			endian = HM1_LITTLE_ENDIAN;

		start_addr = start_addr + 2;
	} else {
		endian = HM1_BIG_ENDIAN;
	}

	for (len = 0; len < finfo->rom_pdxtc_cal_data_addr_list_len / HM1_CAL_ROW_LEN; len ++) {
		ret |= sensor_cis_set_registers(subdev, sensor_hm1_pre_PDXTC[len], sensor_hm1_pre_PDXTC_size[len]);

		dbg_sensor(1, "[%s] PDXTC Calibration Data E\n", __func__);
		if (len != 0) start_addr = finfo->rom_pdxtc_cal_data_addr_list[len * HM1_CAL_ROW_LEN + HM1_CAL_START_ADDR];
		end_addr = finfo->rom_pdxtc_cal_data_addr_list[len * HM1_CAL_ROW_LEN + HM1_CAL_END_ADDR];

#ifdef HM1_BURST_WRITE
		if (finfo->rom_pdxtc_cal_data_addr_list[len * HM1_CAL_ROW_LEN + HM1_CAL_BURST_CHECK]) {
			cal_size = (end_addr - start_addr) / 2 + 1;
			dbg_sensor(1, "[%s] rom_pdxtc_cal burst write start(0x%X) end(0x%X)\n", __func__, start_addr, end_addr);
			ret = sensor_hm1_cis_write16_burst(cis->client, 0x6F12, (u8 *)&cal_buf[start_addr], cal_size, endian);
			if (ret < 0) {
				err("sensor_hm1_cis_write16_burst fail!!");
				goto p_err;
			}
		} else
#endif
		{
			for(i = start_addr; i <= end_addr; i += 2) {
				val = HM1_ENDIAN(cal_buf[i], cal_buf[i + 1], endian);
				ret = is_sensor_write16(cis->client, 0x6F12, val);
				if (ret < 0) {
					err("is_sensor_write16 fail!!");
					goto p_err;
				}
#ifdef DEBUG_CAL_WRITE
				info("cal offset[0x%04X] , val[0x%04X]", i, val);
#endif
			}
		}

		dbg_sensor(1, "[%s] PDXTC Calibration Data X\n", __func__);

		ret |= sensor_cis_set_registers(subdev, sensor_hm1_post_PDXTC[len], sensor_hm1_post_PDXTC_size[len]);
	}

	info("[%s] PDXTC end\n", __func__);

	info("[%s] GCC start\n", __func__);
	start_addr = finfo->rom_gcc_cal_data_addr_list[HM1_CAL_START_ADDR];
	if (finfo->rom_gcc_cal_endian_check) {
		if (cal_buf[start_addr] == 0xFF && cal_buf[start_addr + 1] == 0x00)
			endian = HM1_BIG_ENDIAN;
		else
			endian = HM1_LITTLE_ENDIAN;

		start_addr = start_addr + 2;
	} else {
		endian = HM1_BIG_ENDIAN;
	}

	for (len = 0; len < finfo->rom_gcc_cal_data_addr_list_len / HM1_CAL_ROW_LEN; len ++) {
		ret |= sensor_cis_set_registers(subdev, sensor_hm1_pre_GCC[len], sensor_hm1_pre_GCC_size[len]);

		dbg_sensor(1, "[%s] GCC Calibration Data E\n", __func__);
		if (len != 0) start_addr = finfo->rom_gcc_cal_data_addr_list[len * HM1_CAL_ROW_LEN + HM1_CAL_START_ADDR];
		end_addr = finfo->rom_gcc_cal_data_addr_list[len * HM1_CAL_ROW_LEN + HM1_CAL_END_ADDR];

#ifdef HM1_BURST_WRITE
		if (finfo->rom_gcc_cal_data_addr_list[len * HM1_CAL_ROW_LEN + HM1_CAL_BURST_CHECK]) {
			cal_size = (end_addr - start_addr) / 2 + 1;
			dbg_sensor(1, "[%s] rom_gcc_cal burst write start(0x%X) end(0x%X)\n", __func__, start_addr, end_addr);
			ret = sensor_hm1_cis_write16_burst(cis->client, 0x6F12, (u8 *)&cal_buf[start_addr], cal_size, endian);
			if (ret < 0) {
				err("sensor_hm1_cis_write16_burst fail!!");
				goto p_err;
			}
		} else
#endif
		{
			for(i = start_addr; i <= end_addr; i += 2) {
				val = HM1_ENDIAN(cal_buf[i], cal_buf[i + 1], endian);
				ret = is_sensor_write16(cis->client, 0x6F12, val);
				if (ret < 0) {
					err("is_sensor_write16 fail!!");
					goto p_err;
				}
#ifdef DEBUG_CAL_WRITE
				info("cal offset[0x%04X] , val[0x%04X]", i, val);
#endif
			}
		}

		dbg_sensor(1, "[%s] GCC Calibration Data X\n", __func__);

		ret |= sensor_cis_set_registers(subdev, sensor_hm1_post_GCC[len], sensor_hm1_post_GCC_size[len]);
	}
	info("[%s] GCC end\n", __func__);

	info("[%s] XTC start\n", __func__);
	start_addr = finfo->rom_xtc_cal_data_addr_list[HM1_CAL_START_ADDR];
	if (finfo->rom_xtc_cal_endian_check) {
		if (cal_buf[start_addr] == 0xFF && cal_buf[start_addr + 1] == 0x00)
			endian = HM1_BIG_ENDIAN;
		else
			endian = HM1_LITTLE_ENDIAN;

		start_addr = start_addr + 2;
	} else {
		endian = HM1_BIG_ENDIAN;
	}

	for (len = 0; len < finfo->rom_xtc_cal_data_addr_list_len / HM1_CAL_ROW_LEN; len ++) {
		ret |= sensor_cis_set_registers(subdev, sensor_hm1_pre_XTC[len], sensor_hm1_pre_XTC_size[len]);

		dbg_sensor(1, "[%s] XTC Calibration Data E\n", __func__);
		if (len != 0) start_addr = finfo->rom_xtc_cal_data_addr_list[len * HM1_CAL_ROW_LEN + HM1_CAL_START_ADDR];
		end_addr = finfo->rom_xtc_cal_data_addr_list[len * HM1_CAL_ROW_LEN + HM1_CAL_END_ADDR];

#ifdef HM1_BURST_WRITE
		if (finfo->rom_xtc_cal_data_addr_list[len * HM1_CAL_ROW_LEN + HM1_CAL_BURST_CHECK]) {
			cal_size = (end_addr - start_addr) / 2 + 1;
			dbg_sensor(1, "[%s] rom_xtc_cal burst write start(0x%X) end(0x%X) size(%d)\n", __func__, start_addr, end_addr, cal_size);
			ret = sensor_hm1_cis_write16_burst(cis->client, 0x6F12, (u8 *)&cal_buf[start_addr], cal_size, endian);
			if (ret < 0) {
				err("sensor_hm1_cis_write16_burst fail!!");
				goto p_err;
			}
		} else
#endif
		{
			for(i = start_addr; i <= end_addr; i += 2) {
				val = HM1_ENDIAN(cal_buf[i], cal_buf[i + 1], endian);
				ret = is_sensor_write16(cis->client, 0x6F12, val);
				if (ret < 0) {
					err("is_sensor_write16 fail!!");
					goto p_err;
				}
#ifdef DEBUG_CAL_WRITE
				info("cal offset[0x%04X] , val[0x%04X]", i, val);
#endif
			}
		}

		dbg_sensor(1, "[%s] XTC Calibration Data X\n", __func__);

		ret |= sensor_cis_set_registers(subdev, sensor_hm1_post_XTC[len], sensor_hm1_post_XTC_size[len]);
	}
	info("[%s] XTC end\n", __func__);

	sensor_hm1_eeprom_cal_available = true;

	info("[%s] X", __func__);

p_err:
	return ret;
}

int sensor_hm1_cis_mode_change(struct v4l2_subdev *subdev, u32 mode)
{
	int ret = 0;
	struct is_cis *cis = NULL;
	struct is_module_enum *module;
	struct is_device_sensor_peri *sensor_peri = NULL;
	struct sensor_open_extended *ext_info = NULL;
	struct is_device_sensor *device;
	u32 ex_mode;

	WARN_ON(!subdev);

	cis = (struct is_cis *)v4l2_get_subdevdata(subdev);
	WARN_ON(!cis);
	WARN_ON(!cis->cis_data);

	device = (struct is_device_sensor *)v4l2_get_subdev_hostdata(subdev);
	if (unlikely(!device)) {
		err("device sensor is null");
		return -EINVAL;
	}

	if (mode >= sensor_hm1_max_setfile_num) {
		err("invalid mode(%d)!!", mode);
		ret = -EINVAL;
		goto p_err;
	}

	sensor_peri = container_of(cis, struct is_device_sensor_peri, cis);
	module = sensor_peri->module;
	ext_info = &module->ext;
	WARN_ON(!ext_info);

	info("[%s] E\n", __func__);

	switch(mode) {
		case SENSOR_HM1_12000X9000_10FPS:
		case SENSOR_HM1_7200X5400_14FPS:
		case SENSOR_HM1_7680X4320_24FPS:
		case SENSOR_HM1_4000X2252_30FPS_CENTER_CROP:
			cis->cis_data->max_analog_gain[0] = 0x200; /* x16, gain=x/0x20 */
			cis->cis_data->max_analog_gain[1] = sensor_cis_calc_again_permile(cis->cis_data->max_analog_gain[0]);
			cis->cis_data->max_digital_gain[0] = 0x800; /* x8, gain=x/0x100 */
			cis->cis_data->max_digital_gain[1] = sensor_cis_calc_dgain_permile(cis->cis_data->max_digital_gain[0]);
		break;
		default:
			cis->cis_data->max_analog_gain[0] = 0x600; /* x48, gain=x/0x20 */
			cis->cis_data->max_analog_gain[1] = sensor_cis_calc_again_permile(cis->cis_data->max_analog_gain[0]);
			cis->cis_data->max_digital_gain[0] = 0x1000; /* x16, gain=x/0x100 */
			cis->cis_data->max_digital_gain[1] = sensor_cis_calc_dgain_permile(cis->cis_data->max_digital_gain[0]);
		break;
	}

#if 0 /* cis_data_calculation is called in module_s_format */
	sensor_hm1_cis_data_calculation(sensor_hm1_pllinfos[mode], cis->cis_data);
#endif
	sensor_hm1_set_integration_max_margin(mode, cis->cis_data);

	cis->mipi_clock_index_cur = CAM_MIPI_NOT_INITIALIZED;

	sensor_hm1_cis_set_paf_stat_enable(mode, cis->cis_data);

	I2C_MUTEX_LOCK(cis->i2c_lock);

	is_sensor_write8(cis->client, 0x0100, 0x00);
	ex_mode = is_sensor_g_ex_mode(device);

#ifndef GLOBAL_TIME_CAL_WRITE
	if ((ex_mode == EX_REMOSAIC_CAL
		|| mode == SENSOR_HM1_7680X4320_24FPS
		|| mode == SENSOR_HM1_12000X9000_10FPS
		|| mode == SENSOR_HM1_7200X5400_14FPS)
		&& !sensor_hm1_eeprom_cal_available) {
		ret = sensor_hm1_cis_set_cal(subdev);
		if (ret < 0) {
			err("sensor_hm1_cis_set_cal fail!!");
			goto p_err_i2c_unlock;
		}
	}
#endif

	if (cis->cis_data->sens_config_index_pre == SENSOR_HM1_992X744_120FPS
		&& ex_mode != EX_REMOSAIC_CAL) {
		switch (mode) {
		case SENSOR_HM1_4000X3000_30FPS:
			info("[%s] SENSOR_HM1_4000X3000_30FPS_SHORT\n", __func__);
			mode = SENSOR_HM1_4000X3000_30FPS_SHORT;
			break;
		case SENSOR_HM1_4000X3000_30FPS_DECOMP_TIMING:
			info("[%s] SENSOR_HM1_4000X3000_30FPS_SHORT_DECOMP_TIMING\n", __func__);
			mode = SENSOR_HM1_4000X3000_30FPS_SHORT_DECOMP_TIMING;
			break;
		case SENSOR_HM1_4000X3000_30FPS_FULL_DILLUTION:
			info("[%s] SENSOR_HM1_4000X3000_30FPS_SHORT_FULL_DILLUTION\n", __func__);
			mode = SENSOR_HM1_4000X3000_30FPS_SHORT_FULL_DILLUTION;
			break;
		case SENSOR_HM1_4000X2252_30FPS:
			info("[%s] SENSOR_HM1_4000X2252_30FPS_SHORT\n", __func__);
			mode = SENSOR_HM1_4000X2252_30FPS_SHORT;
			break;
		}
	}

	info("[%s] sensor mode(%d) ex_mode(%d)\n", __func__, mode, ex_mode);
	ret = sensor_cis_set_registers(subdev, sensor_hm1_setfiles[mode],
							sensor_hm1_setfile_sizes[mode]);
	if (ret < 0) {
		err("sensor_hm1_set_registers fail!!");
		goto p_err_i2c_unlock;
	}
	info("[%s] sensor mode done(%d)\n", __func__, mode);

	cis->cis_data->sens_config_index_pre = mode;

	info("[%s] dual sync always master\n", __func__);
	ret = sensor_cis_set_registers(subdev, sensor_hm1_cis_dual_master_settings, sensor_hm1_cis_dual_master_settings_size);

	if (ex_mode == EX_REMOSAIC_CAL
		|| mode == SENSOR_HM1_7680X4320_24FPS
		|| mode == SENSOR_HM1_12000X9000_10FPS
		|| mode == SENSOR_HM1_7200X5400_14FPS) {
		if (!sensor_hm1_eeprom_cal_available) {
			info("[%s] no calibration data\n", __func__);
			ret |= is_sensor_write16(cis->client, 0xFCFC, 0x4000);
			ret |= is_sensor_write16(cis->client, 0x6000, 0x0005);
			ret |= is_sensor_write16(cis->client, 0x0D02, 0x0000);
			ret |= is_sensor_write16(cis->client, 0x0B00, 0x0001);
			ret |= is_sensor_write16(cis->client, 0x6000, 0x0085);
		} else {
			if (sensor_hm1_first_entrance) {
				info("[%s] sensor_hm1_global_for_cal\n", __func__);
				ret |= sensor_cis_set_registers(subdev, sensor_hm1_global_for_cal, sensor_hm1_global_for_cal_size);
				sensor_hm1_first_entrance = false;
			}
		}
	}

	if (mode == SENSOR_HM1_12000X9000_10FPS
		|| mode == SENSOR_HM1_7200X5400_14FPS) {
		cis->cis_data->highres_capture_mode = true;
	} else {
		cis->cis_data->highres_capture_mode = false;
	}

p_err_i2c_unlock:
	I2C_MUTEX_UNLOCK(cis->i2c_lock);

p_err:
	/* sensor_hm1_cis_log_status(subdev); */
	info("[%s] X\n", __func__);

	return ret;
}

int sensor_hm1_cis_set_global_setting(struct v4l2_subdev *subdev)
{
	int ret = 0;
	struct is_cis *cis = NULL;

	WARN_ON(!subdev);

	cis = (struct is_cis *)v4l2_get_subdevdata(subdev);
	WARN_ON(!cis);

	I2C_MUTEX_LOCK(cis->i2c_lock);

	if (sensor_hm1_fake_retention_status) {
		info("[%s] skip global setting\n", __func__);
		sensor_hm1_fake_retention_status = false;
	} else {
		info("[%s] global setting start\n", __func__);
		/* setfile global setting is at camera entrance */
		ret |= sensor_cis_set_registers(subdev, sensor_hm1_initial, sensor_hm1_initial_size);
		ret |= sensor_cis_set_registers(subdev, sensor_hm1_tnp, sensor_hm1_tnp_size);
		ret |= sensor_cis_set_registers(subdev, sensor_hm1_global, sensor_hm1_global_size);
		if (ret < 0) {
			err("sensor_hm1_set_registers fail!!");
			goto p_err;
		}
#ifdef USE_WIDE_MIPI_STRENGTH_490MV
		ret |= is_sensor_write16(cis->client, 0xFCFC, 0x4000);
		ret |= is_sensor_write16(cis->client, 0x6B04, 0xB000);
		info("[%s] mipi strength 490mV\n", __func__);
#else
		ret |= is_sensor_write16(cis->client, 0xFCFC, 0x4000);
		ret |= is_sensor_write16(cis->client, 0x6B04, 0xE000);
		info("[%s] mipi strength 520mV(default)\n", __func__);
#endif
		info("[%s] global setting done\n", __func__);

#ifdef GLOBAL_TIME_CAL_WRITE
		ret = sensor_hm1_cis_set_cal(subdev);
		if (ret < 0) {
			err("sensor_hm1_cis_set_cal fail!!");
			goto p_err;
		}
#endif
	}
p_err:
	I2C_MUTEX_UNLOCK(cis->i2c_lock);
	return ret;
}

/* TODO: Sensor set size sequence(sensor done, sensor stop, 3AA done in FW case */
int sensor_hm1_cis_set_size(struct v4l2_subdev *subdev, cis_shared_data *cis_data)
{
	int ret = 0;
	bool binning = false;
	u32 ratio_w = 0, ratio_h = 0, start_x = 0, start_y = 0, end_x = 0, end_y = 0;
	u32 even_x = 0, odd_x = 0, even_y = 0, odd_y = 0;
	struct i2c_client *client = NULL;
	struct is_cis *cis = NULL;
#ifdef DEBUG_SENSOR_TIME
	struct timeval st, end;

	do_gettimeofday(&st);
#endif
	WARN_ON(!subdev);

	cis = (struct is_cis *)v4l2_get_subdevdata(subdev);
	WARN_ON(!cis);

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
	ret = sensor_hm1_wait_stream_off_status(cis_data);
	if (ret) {
		err("Must stream off\n");
		ret = -EINVAL;
		goto p_err;
	}

	binning = cis_data->binning;
	if (binning) {
		ratio_w = (SENSOR_HM1_MAX_WIDTH / cis_data->cur_width);
		ratio_h = (SENSOR_HM1_MAX_HEIGHT / cis_data->cur_height);
	} else {
		ratio_w = 1;
		ratio_h = 1;
	}

	if (((cis_data->cur_width * ratio_w) > SENSOR_HM1_MAX_WIDTH) ||
		((cis_data->cur_height * ratio_h) > SENSOR_HM1_MAX_HEIGHT)) {
		err("Config max sensor size over~!!\n");
		ret = -EINVAL;
		goto p_err;
	}

	I2C_MUTEX_LOCK(cis->i2c_lock);
	/* 1. page_select */
	ret = is_sensor_write16(client, 0xFCFC, 0x4000);
	if (ret < 0)
		goto p_err_i2c_unlock;

	/* 2. pixel address region setting */
	start_x = ((SENSOR_HM1_MAX_WIDTH - cis_data->cur_width * ratio_w) / 2) & (~0x1);
	start_y = ((SENSOR_HM1_MAX_HEIGHT - cis_data->cur_height * ratio_h) / 2) & (~0x1);
	end_x = start_x + (cis_data->cur_width * ratio_w - 1);
	end_y = start_y + (cis_data->cur_height * ratio_h - 1);

	if (!(end_x & (0x1)) || !(end_y & (0x1))) {
		err("Sensor pixel end address must odd\n");
		ret = -EINVAL;
		goto p_err_i2c_unlock;
	}

	ret = is_sensor_write16(client, 0x0344, start_x);
	if (ret < 0)
		goto p_err_i2c_unlock;
	ret = is_sensor_write16(client, 0x0346, start_y);
	if (ret < 0)
		goto p_err_i2c_unlock;
	ret = is_sensor_write16(client, 0x0348, end_x);
	if (ret < 0)
		goto p_err_i2c_unlock;
	ret = is_sensor_write16(client, 0x034A, end_y);
	if (ret < 0)
		goto p_err_i2c_unlock;

	/* 3. output address setting */
	ret = is_sensor_write16(client, 0x034C, cis_data->cur_width);
	if (ret < 0)
		goto p_err_i2c_unlock;
	ret = is_sensor_write16(client, 0x034E, cis_data->cur_height);
	if (ret < 0)
		goto p_err_i2c_unlock;

	/* If not use to binning, sensor image should set only crop */
	if (!binning) {
		dbg_sensor(1, "Sensor size set is not binning\n");
		goto p_err_i2c_unlock;
	}

	/* 4. sub sampling setting */
	even_x = 1;	/* 1: not use to even sampling */
	even_y = 1;
	odd_x = (ratio_w * 2) - even_x;
	odd_y = (ratio_h * 2) - even_y;

	ret = is_sensor_write16(client, 0x0380, even_x);
	if (ret < 0)
		goto p_err_i2c_unlock;
	ret = is_sensor_write16(client, 0x0382, odd_x);
	if (ret < 0)
		goto p_err_i2c_unlock;
	ret = is_sensor_write16(client, 0x0384, even_y);
	if (ret < 0)
		goto p_err_i2c_unlock;
	ret = is_sensor_write16(client, 0x0386, odd_y);
	if (ret < 0)
		goto p_err_i2c_unlock;

#if 0
	/* 5. binnig setting */
	ret = is_sensor_write8(client, 0x0900, binning);	/* 1:  binning enable, 0: disable */
	if (ret < 0)
		goto p_err;
	ret = is_sensor_write8(client, 0x0901, (ratio_w << 4) | ratio_h);
	if (ret < 0)
		goto p_err;
#endif

	/* 6. scaling setting: but not use */
	/* scaling_mode (0: No scaling, 1: Horizontal, 2: Full, 4:Separate vertical) */
	ret = is_sensor_write16(client, 0x0402, 0x0000);
	if (ret < 0)
		goto p_err_i2c_unlock;
	/* down_scale_m: 1 to 16 upwards (scale_n: 16(fixed))
	 * down scale factor = down_scale_m / down_scale_n
	 */
	ret = is_sensor_write16(client, 0x0404, 0x10);
	if (ret < 0)
		goto p_err_i2c_unlock;

	cis_data->frame_time = (cis_data->line_readOut_time * cis_data->cur_height / 1000);
	cis->cis_data->rolling_shutter_skew = (cis->cis_data->cur_height - 1) * cis->cis_data->line_readOut_time;
	dbg_sensor(1, "[%s] frame_time(%d), rolling_shutter_skew(%lld)\n",
		__func__, cis->cis_data->frame_time, cis->cis_data->rolling_shutter_skew);

#ifdef DEBUG_SENSOR_TIME
	do_gettimeofday(&end);
	dbg_sensor(1, "[%s] time %lu us\n", __func__, (end.tv_sec - st.tv_sec) * 1000000 + (end.tv_usec - st.tv_usec));
#endif

p_err_i2c_unlock:
	I2C_MUTEX_UNLOCK(cis->i2c_lock);

p_err:
	return ret;
}

int sensor_hm1_cis_wait_streamon(struct v4l2_subdev *subdev)
{
	int ret = 0;
	struct is_cis *cis;
	struct i2c_client *client;
	u16 data16;

	WARN_ON(!subdev);

	ret = sensor_cis_wait_streamon(subdev);
	if (ret < 0)
		goto p_err;

	cis = (struct is_cis *)v4l2_get_subdevdata(subdev);
	if (unlikely(!cis)) {
		err("cis is NULL");
		ret = -EINVAL;
		goto p_err;
	}

	client = cis->client;
	if (unlikely(!client)) {
		err("client is NULL");
		ret = -EINVAL;
		goto p_err;
	}

	I2C_MUTEX_LOCK(cis->i2c_lock);
	is_sensor_write16(client, 0x602C, 0x2001);
	pr_info("[%s] 0x2001 page\n", __func__);

	is_sensor_write16(client, 0x602E, 0x20FC);
	is_sensor_read16(client, 0x6F12, &data16);
	pr_info("[%s] 0x20FC(0x%x)\n", __func__, data16);

	is_sensor_write16(client, 0x602E, 0x20FE);
	is_sensor_read16(client, 0x6F12, &data16);
	pr_info("[%s] 0x20FE(0x%x)\n", __func__, data16);

	is_sensor_write16(client, 0x602E, 0x2100);
	is_sensor_read16(client, 0x6F12, &data16);
	pr_info("[%s] 0x2100(0x%x)\n", __func__, data16);

	is_sensor_write16(client, 0x602C, 0x4000);
	I2C_MUTEX_UNLOCK(cis->i2c_lock);

p_err:
	return ret;
}

int sensor_hm1_cis_stream_on(struct v4l2_subdev *subdev)
{
	int ret = 0;
	struct is_cis *cis;
	struct i2c_client *client;
	cis_shared_data *cis_data;
	struct is_device_sensor_peri *sensor_peri = NULL;
	u16 data16;
	struct is_device_sensor *device;
#ifdef DEBUG_SENSOR_TIME
	struct timeval st, end;

	do_gettimeofday(&st);
#endif

	WARN_ON(!subdev);

	device = (struct is_device_sensor *)v4l2_get_subdev_hostdata(subdev);
	WARN_ON(!device);

	cis = (struct is_cis *)v4l2_get_subdevdata(subdev);

	WARN_ON(!cis);
	WARN_ON(!cis->cis_data);

	sensor_peri = container_of(cis, struct is_device_sensor_peri, cis);
	WARN_ON(!sensor_peri);

	client = cis->client;
	if (unlikely(!client)) {
		err("client is NULL");
		ret = -EINVAL;
		goto p_err;
	}

	cis_data = cis->cis_data;

	dbg_sensor(1, "[MOD:D:%d] %s\n", cis->id, __func__);

	is_vendor_set_mipi_clock(device);

	I2C_MUTEX_LOCK(cis->i2c_lock);
	ret = sensor_hm1_cis_group_param_hold_func(subdev, 0x01);
	if (ret < 0)
		err("group_param_hold_func failed at stream on");

#ifdef DEBUG_HM1_PLL
	{
		u16 pll;

		is_sensor_read16(client, 0x0300, &pll);
		dbg_sensor(1, "______ vt_pix_clk_div(0x%x)\n", pll);
		is_sensor_read16(client, 0x0302, &pll);
		dbg_sensor(1, "______ vt_sys_clk_div(0x%x)\n", pll);
		is_sensor_read16(client, 0x0304, &pll);
		dbg_sensor(1, "______ vt_pre_pll_clk_div(0x%x)\n", pll);
		is_sensor_read16(client, 0x0306, &pll);
		dbg_sensor(1, "______ vt_pll_multiplier(0x%x)\n", pll);
		is_sensor_read16(client, 0x0308, &pll);
		dbg_sensor(1, "______ op_pix_clk_div(0x%x)\n", pll);
		is_sensor_read16(client, 0x030a, &pll);
		dbg_sensor(1, "______ op_sys_clk_div(0x%x)\n", pll);

		is_sensor_read16(client, 0x030c, &pll);
		dbg_sensor(1, "______ vt_pll_post_scaler(0x%x)\n", pll);
		is_sensor_read16(client, 0x030e, &pll);
		dbg_sensor(1, "______ op_pre_pll_clk_dv(0x%x)\n", pll);
		is_sensor_read16(client, 0x0310, &pll);
		dbg_sensor(1, "______ op_pll_multiplier(0x%x)\n", pll);
		is_sensor_read16(client, 0x0312, &pll);
		dbg_sensor(1, "______ op_pll_post_scalar(0x%x)\n", pll);

		is_sensor_read16(client, 0x0340, &pll);
		dbg_sensor(1, "______ frame_length_lines(0x%x)\n", pll);
		is_sensor_read16(client, 0x0342, &pll);
		dbg_sensor(1, "______ line_length_pck(0x%x)\n", pll);
	}
#endif

	/*
	 * If a companion is used,
	 * then 8 ms waiting is needed before the StreamOn of a sensor (S5KHM1).
	 */
	if (test_bit(IS_SENSOR_PREPROCESSOR_AVAILABLE, &sensor_peri->peri_state))
		mdelay(8);

	/* EMB Header off */
	ret = is_sensor_write8(client, 0x0118, 0x00);
	if (ret < 0){
		err("EMB header off fail");
	}

	is_sensor_read16(client, 0x0D08, &data16);
	pr_info("[%s] 0x0D08(0x%x)\n", __func__, data16);

	is_sensor_read16(client, 0x0D0A, &data16);
	pr_info("[%s] 0x0D0A(0x%x)\n", __func__, data16);

	/* Sensor stream on */
	info("%s\n", __func__);
	is_sensor_write16(client, 0x0100, 0x0100);

	ret = sensor_hm1_cis_group_param_hold_func(subdev, 0x00);
	if (ret < 0)
		err("group_param_hold_func failed at stream on");

	I2C_MUTEX_UNLOCK(cis->i2c_lock);

	cis_data->stream_on = true;

#ifdef DEBUG_SENSOR_TIME
	do_gettimeofday(&end);
	dbg_sensor(1, "[%s] time %lu us\n", __func__, (end.tv_sec - st.tv_sec)*1000000 + (end.tv_usec - st.tv_usec));
#endif

p_err:
	return ret;
}

int sensor_hm1_cis_stream_off(struct v4l2_subdev *subdev)
{
	int ret = 0;
	struct is_cis *cis;
	struct i2c_client *client;
	cis_shared_data *cis_data;
	u8 cur_frame_count = 0;

#ifdef DEBUG_SENSOR_TIME
	struct timeval st, end;

	do_gettimeofday(&st);
#endif

	WARN_ON(!subdev);

	cis = (struct is_cis *)v4l2_get_subdevdata(subdev);

	WARN_ON(!cis);
	WARN_ON(!cis->cis_data);

	client = cis->client;
	if (unlikely(!client)) {
		err("client is NULL");
		ret = -EINVAL;
		goto p_err;
	}

	cis_data = cis->cis_data;

	dbg_sensor(1, "[MOD:D:%d] %s\n", cis->id, __func__);

	I2C_MUTEX_LOCK(cis->i2c_lock);
	ret = sensor_hm1_cis_group_param_hold_func(subdev, 0x00);
	if (ret < 0)
		err("group_param_hold_func failed at stream off");

	is_sensor_read8(client, 0x0005, &cur_frame_count);
	info("%s: frame_count(0x%x)\n", __func__, cur_frame_count);

	is_sensor_write8(client, 0x0100, 0x00);

	I2C_MUTEX_UNLOCK(cis->i2c_lock);

	cis_data->stream_on = false;

#ifdef DEBUG_SENSOR_TIME
	do_gettimeofday(&end);
	dbg_sensor(1, "[%s] time %lu us\n", __func__, (end.tv_sec - st.tv_sec)*1000000 + (end.tv_usec - st.tv_usec));
#endif

p_err:
	return ret;
}

int sensor_hm1_cis_set_exposure_time(struct v4l2_subdev *subdev, struct ae_param *target_exposure)
{
	int ret = 0;
	int hold = 0;
	struct is_cis *cis;
	struct i2c_client *client;
	cis_shared_data *cis_data;

	u64 vt_pic_clk_freq_khz = 0;
	u16 long_coarse_int = 0;
	u16 short_coarse_int = 0;
	u32 line_length_pck = 0;
	u32 min_fine_int = 0;
	u16 coarse_integration_time_shifter = 0;

	u16 cit_shifter_array[33] = {0,1,2,2,3,3,3,3,4,4,4,4,4,4,4,4,5,5,5,5,5,5,5,5,5,5,5,5,5,5,5,5,6};
	u16 cit_shifter_val = 0;
	int cit_shifter_idx = 0;
	u8 cit_denom_array[7] = {1, 2, 4, 8, 16, 32, 64};

#ifdef DEBUG_SENSOR_TIME
	struct timeval st, end;

	do_gettimeofday(&st);
#endif

	WARN_ON(!subdev);
	WARN_ON(!target_exposure);

	cis = (struct is_cis *)v4l2_get_subdevdata(subdev);

	WARN_ON(!cis);
	WARN_ON(!cis->cis_data);

	client = cis->client;
	if (unlikely(!client)) {
		err("client is NULL");
		ret = -EINVAL;
		goto p_err;
	}

	sensor_hm1_target_exp_backup.short_val = target_exposure->short_val;
	sensor_hm1_target_exp_backup.long_val = target_exposure->long_val;

	if ((target_exposure->long_val <= 0) || (target_exposure->short_val <= 0)) {
		err("[%s] invalid target exposure(%d, %d)\n", __func__,
				target_exposure->long_val, target_exposure->short_val);
		ret = -EINVAL;
		goto p_err;
	}

	cis_data = cis->cis_data;

	if (cis->long_term_mode.sen_strm_off_on_enable == false) {
		switch(cis_data->sens_config_index_cur) {
#if 0
		case SENSOR_HM1_2016X1134_30FPS:
			if (MAX(target_exposure->long_val, target_exposure->short_val) > 80000) {
				cit_shifter_idx = MIN(MAX(MAX(target_exposure->long_val, target_exposure->short_val) / 80000, 0), 32);
				cit_shifter_val = MAX(cit_shifter_array[cit_shifter_idx], cis_data->frame_length_lines_shifter);
			} else {
				cit_shifter_val = (u16)(cis_data->frame_length_lines_shifter);
			}
			target_exposure->long_val = target_exposure->long_val / cit_denom_array[cit_shifter_val];
			target_exposure->short_val = target_exposure->short_val / cit_denom_array[cit_shifter_val];
			coarse_integration_time_shifter = ((cit_shifter_val<<8) & 0xFF00) + (cit_shifter_val & 0x00FF);
			break;
#endif
		default:
			if (MAX(target_exposure->long_val, target_exposure->short_val) > 160000) {
				cit_shifter_idx = MIN(MAX(MAX(target_exposure->long_val, target_exposure->short_val) / 160000, 0), 32);
				cit_shifter_val = MAX(cit_shifter_array[cit_shifter_idx], cis_data->frame_length_lines_shifter);
			} else {
				cit_shifter_val = (u16)(cis_data->frame_length_lines_shifter);
			}
			target_exposure->long_val = target_exposure->long_val / cit_denom_array[cit_shifter_val];
			target_exposure->short_val = target_exposure->short_val / cit_denom_array[cit_shifter_val];
			coarse_integration_time_shifter = ((cit_shifter_val<<8) & 0xFF00) + (cit_shifter_val & 0x00FF);
			break;
		}
	}

	dbg_sensor(1, "[MOD:D:%d] %s, vsync_cnt(%d), target long(%d), short(%d)\n", cis->id, __func__,
			cis_data->sen_vsync_count, target_exposure->long_val, target_exposure->short_val);

	vt_pic_clk_freq_khz = cis_data->pclk / (1000);
	line_length_pck = cis_data->line_length_pck;
	min_fine_int = cis_data->min_fine_integration_time;

	long_coarse_int = ((target_exposure->long_val * vt_pic_clk_freq_khz) / 1000 - min_fine_int)
											/ line_length_pck;
	if (long_coarse_int < cis_data->min_coarse_integration_time) {
		dbg_sensor(1, "[MOD:D:%d] %s, vsync_cnt(%d), long coarse(%d) min(%d)\n", cis->id, __func__,
			cis_data->sen_vsync_count, long_coarse_int, cis_data->min_coarse_integration_time);
		long_coarse_int = cis_data->min_coarse_integration_time;
	}
	short_coarse_int = ((target_exposure->short_val * vt_pic_clk_freq_khz) / 1000 - min_fine_int)
											/ line_length_pck;
	if (short_coarse_int < cis_data->min_coarse_integration_time) {
		dbg_sensor(1, "[MOD:D:%d] %s, vsync_cnt(%d), short coarse(%d) min(%d)\n", cis->id, __func__,
			cis_data->sen_vsync_count, short_coarse_int, cis_data->min_coarse_integration_time);
		short_coarse_int = cis_data->min_coarse_integration_time;
	}

	if (long_coarse_int > cis_data->max_coarse_integration_time) {
		dbg_sensor(1, "[MOD:D:%d] %s, vsync_cnt(%d), long coarse(%d) max(%d)\n", cis->id, __func__,
			cis_data->sen_vsync_count, long_coarse_int, cis_data->max_coarse_integration_time);
		long_coarse_int = cis_data->max_coarse_integration_time;
	}

	if (short_coarse_int > cis_data->max_coarse_integration_time) {
		dbg_sensor(1, "[MOD:D:%d] %s, vsync_cnt(%d), short coarse(%d) max(%d)\n", cis->id, __func__,
			cis_data->sen_vsync_count, short_coarse_int, cis_data->max_coarse_integration_time);
		short_coarse_int = cis_data->max_coarse_integration_time;
	}

	cis_data->cur_long_exposure_coarse = long_coarse_int;
	cis_data->cur_short_exposure_coarse = short_coarse_int;

	I2C_MUTEX_LOCK(cis->i2c_lock);
	hold = sensor_hm1_cis_group_param_hold_func(subdev, 0x01);
	if (hold < 0) {
		ret = hold;
		goto p_err_i2c_unlock;
	}

	/* WDR mode */
	is_sensor_write16(cis->client, 0xFCFC, 0x4000);
	if (sensor_hm1_cis_is_wdr_mode_on(cis_data)) {
		is_sensor_write16(cis->client, 0x021E, 0x0100);
	} else {
		is_sensor_write16(cis->client, 0x021E, 0x0000);
	}

	/* Short exposure */
	ret = is_sensor_write16(client, 0x0202, short_coarse_int);
	if (ret < 0)
		goto p_err_i2c_unlock;

	/* Long exposure */
	if (sensor_hm1_cis_is_wdr_mode_on(cis_data)) {
		ret = is_sensor_write16(client, 0x0226, long_coarse_int);
		if (ret < 0)
			goto p_err_i2c_unlock;
	}

	/* CIT shifter */
	if (cis->long_term_mode.sen_strm_off_on_enable == false) {
		ret = is_sensor_write16(client, 0x0704, coarse_integration_time_shifter);
		if (ret < 0)
			goto p_err_i2c_unlock;
	}

	dbg_sensor(1, "[MOD:D:%d] %s, vsync_cnt(%d), vt_pic_clk_freq_khz (%d),"
		KERN_CONT "line_length_pck(%d), min_fine_int (%d)\n",
		cis->id, __func__, cis_data->sen_vsync_count, vt_pic_clk_freq_khz/1000,
		line_length_pck, min_fine_int);
	dbg_sensor(1, "[MOD:D:%d] %s, vsync_cnt(%d), frame_length_lines(%#x),"
		KERN_CONT "long_coarse_int %#x, short_coarse_int %#x coarse_integration_time_shifter %#x\n",
		cis->id, __func__, cis_data->sen_vsync_count, cis_data->frame_length_lines,
		long_coarse_int, short_coarse_int, coarse_integration_time_shifter);

#ifdef DEBUG_SENSOR_TIME
	do_gettimeofday(&end);
	dbg_sensor(1, "[%s] time %lu us\n", __func__, (end.tv_sec - st.tv_sec)*1000000 + (end.tv_usec - st.tv_usec));
#endif

p_err_i2c_unlock:
	if (hold > 0) {
		hold = sensor_hm1_cis_group_param_hold_func(subdev, 0x00);
		if (hold < 0)
			ret = hold;
	}
	I2C_MUTEX_UNLOCK(cis->i2c_lock);

p_err:
	return ret;
}

int sensor_hm1_cis_get_min_exposure_time(struct v4l2_subdev *subdev, u32 *min_expo)
{
	int ret = 0;
	struct is_cis *cis = NULL;
	cis_shared_data *cis_data = NULL;
	u32 min_integration_time = 0;
	u32 min_coarse = 0;
	u32 min_fine = 0;
	u64 vt_pic_clk_freq_khz = 0;
	u32 line_length_pck = 0;

#ifdef DEBUG_SENSOR_TIME
	struct timeval st, end;

	do_gettimeofday(&st);
#endif

	WARN_ON(!subdev);
	WARN_ON(!min_expo);

	cis = (struct is_cis *)v4l2_get_subdevdata(subdev);

	WARN_ON(!cis);
	WARN_ON(!cis->cis_data);

	cis_data = cis->cis_data;

	vt_pic_clk_freq_khz = cis_data->pclk / (1000);
	if (vt_pic_clk_freq_khz == 0) {
		pr_err("[MOD:D:%d] %s, Invalid vt_pic_clk_freq_khz(%d)\n", cis->id, __func__, vt_pic_clk_freq_khz/1000);
		goto p_err;
	}
	line_length_pck = cis_data->line_length_pck;
	min_coarse = cis_data->min_coarse_integration_time;
	min_fine = cis_data->min_fine_integration_time;

	min_integration_time = (u32)((u64)((line_length_pck * min_coarse) + min_fine) * 1000 / vt_pic_clk_freq_khz);
	*min_expo = min_integration_time;

	dbg_sensor(1, "[%s] min integration time %d\n", __func__, min_integration_time);

#ifdef DEBUG_SENSOR_TIME
	do_gettimeofday(&end);
	dbg_sensor(1, "[%s] time %lu us\n", __func__, (end.tv_sec - st.tv_sec)*1000000 + (end.tv_usec - st.tv_usec));
#endif

p_err:
	return ret;
}

int sensor_hm1_cis_get_max_exposure_time(struct v4l2_subdev *subdev, u32 *max_expo)
{
	int ret = 0;
	struct is_cis *cis;
	cis_shared_data *cis_data;
	u32 max_integration_time = 0;
	u32 max_coarse_margin = 0;
	u32 max_fine_margin = 0;
	u32 max_coarse = 0;
	u32 max_fine = 0;
	u64 vt_pic_clk_freq_khz = 0;
	u32 line_length_pck = 0;
	u32 frame_length_lines = 0;

#ifdef DEBUG_SENSOR_TIME
	struct timeval st, end;

	do_gettimeofday(&st);
#endif

	WARN_ON(!subdev);
	WARN_ON(!max_expo);

	cis = (struct is_cis *)v4l2_get_subdevdata(subdev);

	WARN_ON(!cis);
	WARN_ON(!cis->cis_data);

	cis_data = cis->cis_data;

	vt_pic_clk_freq_khz = cis_data->pclk / (1000);
	if (vt_pic_clk_freq_khz == 0) {
		pr_err("[MOD:D:%d] %s, Invalid vt_pic_clk_freq_khz(%d)\n", cis->id, __func__, vt_pic_clk_freq_khz/1000);
		goto p_err;
	}
	line_length_pck = cis_data->line_length_pck;
	frame_length_lines = cis_data->frame_length_lines;

	max_coarse_margin = cis_data->max_margin_coarse_integration_time;
	max_fine_margin = line_length_pck - cis_data->min_fine_integration_time;
	max_coarse = frame_length_lines - max_coarse_margin;
	max_fine = cis_data->max_fine_integration_time;

	max_integration_time = (u32)((u64)((line_length_pck * max_coarse) + max_fine) * 1000 / vt_pic_clk_freq_khz);

	*max_expo = max_integration_time;

	/* TODO: Is this values update hear? */
	cis_data->max_margin_fine_integration_time = max_fine_margin;
	cis_data->max_coarse_integration_time = max_coarse;

	dbg_sensor(1, "[%s] max integration time %d, max margin fine integration %d, max coarse integration %d\n",
			__func__, max_integration_time,
			cis_data->max_margin_fine_integration_time, cis_data->max_coarse_integration_time);

#ifdef DEBUG_SENSOR_TIME
	do_gettimeofday(&end);
	dbg_sensor(1, "[%s] time %lu us\n", __func__, (end.tv_sec - st.tv_sec)*1000000 + (end.tv_usec - st.tv_usec));
#endif

p_err:
	return ret;
}

int sensor_hm1_cis_adjust_frame_duration(struct v4l2_subdev *subdev,
						u32 input_exposure_time,
						u32 *target_duration)
{
	int ret = 0;
	struct is_cis *cis;
	cis_shared_data *cis_data;

	u64 vt_pic_clk_freq_khz = 0;
	u32 line_length_pck = 0;
	u32 frame_length_lines = 0;
	u32 frame_duration = 0;

#ifdef DEBUG_SENSOR_TIME
	struct timeval st, end;

	do_gettimeofday(&st);
#endif

	WARN_ON(!subdev);
	WARN_ON(!target_duration);

	cis = (struct is_cis *)v4l2_get_subdevdata(subdev);

	WARN_ON(!cis);
	WARN_ON(!cis->cis_data);

	cis_data = cis->cis_data;

	if (input_exposure_time == 0) {
		input_exposure_time  = cis_data->min_frame_us_time;
		info("[%s] Not proper exposure time(0), so apply min frame duration to exposure time forcely!!!(%d)\n",
			__func__, cis_data->min_frame_us_time);
	}

	vt_pic_clk_freq_khz = cis_data->pclk / (1000);
	line_length_pck = cis_data->line_length_pck;
	frame_length_lines = (u32)(((vt_pic_clk_freq_khz * input_exposure_time) / 1000
						- cis_data->min_fine_integration_time) / line_length_pck);
	frame_length_lines += cis_data->max_margin_coarse_integration_time;

	frame_duration = (u32)(((u64)frame_length_lines * line_length_pck) * 1000 / vt_pic_clk_freq_khz);

	dbg_sensor(1, "[%s](vsync cnt = %d) input exp(%d), adj duration, frame duraion(%d), min_frame_us(%d)\n",
			__func__, cis_data->sen_vsync_count,
			input_exposure_time, frame_duration, cis_data->min_frame_us_time);
	dbg_sensor(1, "[%s](vsync cnt = %d) adj duration, frame duraion(%d), min_frame_us(%d)\n",
			__func__, cis_data->sen_vsync_count, frame_duration, cis_data->min_frame_us_time);

	if (cis->long_term_mode.sen_strm_off_on_enable == false) {
		*target_duration = MAX(frame_duration, cis_data->min_frame_us_time);
	} else {
		*target_duration = frame_duration;
	}

#ifdef DEBUG_SENSOR_TIME
	do_gettimeofday(&end);
	dbg_sensor(1, "[%s] time %lu us\n", __func__, (end.tv_sec - st.tv_sec)*1000000 + (end.tv_usec - st.tv_usec));
#endif

	return ret;
}

int sensor_hm1_cis_set_frame_duration(struct v4l2_subdev *subdev, u32 frame_duration)
{
	int ret = 0;
	int hold = 0;
	struct is_cis *cis;
	struct i2c_client *client;
	cis_shared_data *cis_data;

	u64 vt_pic_clk_freq_khz = 0;
	u32 line_length_pck = 0;
	u16 frame_length_lines = 0;
	u8 frame_length_lines_shifter = 0;

	u8 fll_shifter_array[33] = {0,1,2,2,3,3,3,3,4,4,4,4,4,4,4,4,5,5,5,5,5,5,5,5,5,5,5,5,5,5,5,5,6};
	int fll_shifter_idx = 0;
	u8 fll_denom_array[7] = {1, 2, 4, 8, 16, 32, 64};

#ifdef DEBUG_SENSOR_TIME
	struct timeval st, end;

	do_gettimeofday(&st);
#endif

	WARN_ON(!subdev);

	cis = (struct is_cis *)v4l2_get_subdevdata(subdev);

	WARN_ON(!cis);
	WARN_ON(!cis->cis_data);

	client = cis->client;
	if (unlikely(!client)) {
		err("client is NULL");
		ret = -EINVAL;
		goto p_err;
	}

	cis_data = cis->cis_data;

	if (cis->long_term_mode.sen_strm_off_on_enable == false) {
		if (frame_duration < cis_data->min_frame_us_time) {
			dbg_sensor(1, "frame duration is less than min(%d)\n", frame_duration);
			frame_duration = cis_data->min_frame_us_time;
		}
	}

	sensor_hm1_frame_duration_backup = frame_duration;
	cis_data->cur_frame_us_time = frame_duration;

	if (cis->long_term_mode.sen_strm_off_on_enable == false) {
		switch(cis_data->sens_config_index_cur) {
#if 0
		case SENSOR_HM1_2016X1134_30FPS:
			if (frame_duration > 80000) {
				fll_shifter_idx = MIN(MAX(frame_duration / 80000, 0), 32);
				frame_length_lines_shifter = fll_shifter_array[fll_shifter_idx];
				frame_duration = frame_duration / fll_denom_array[frame_length_lines_shifter];
			} else {
				frame_length_lines_shifter = 0x0;
			}
			break;
#endif
		default:
			if (frame_duration > 160000) {
				fll_shifter_idx = MIN(MAX(frame_duration / 160000, 0), 32);
				frame_length_lines_shifter = fll_shifter_array[fll_shifter_idx];
				frame_duration = frame_duration / fll_denom_array[frame_length_lines_shifter];
			} else {
				frame_length_lines_shifter = 0x0;
			}
			break;
		}
	}

	vt_pic_clk_freq_khz = cis_data->pclk / (1000);
	line_length_pck = cis_data->line_length_pck;

	frame_length_lines = (u16)((vt_pic_clk_freq_khz * frame_duration) / (line_length_pck * 1000));

	dbg_sensor(1, "[MOD:D:%d] %s, vt_pic_clk_freq_khz(%#x) frame_duration = %d us,"
			KERN_CONT "(line_length_pck%#x), frame_length_lines(%#x), frame_length_lines_shifter(%#x)\n",
			cis->id, __func__, vt_pic_clk_freq_khz/1000, frame_duration,
			line_length_pck, frame_length_lines, frame_length_lines_shifter);

	I2C_MUTEX_LOCK(cis->i2c_lock);
	hold = sensor_hm1_cis_group_param_hold_func(subdev, 0x01);
	if (hold < 0) {
		ret = hold;
		goto p_err_i2c_unlock;
	}

	ret |= is_sensor_write16(client, 0x0340, frame_length_lines);

	if (ret < 0)
		goto p_err_i2c_unlock;

	/* frame duration shifter */
	if (cis->long_term_mode.sen_strm_off_on_enable == false) {
		ret = is_sensor_write8(client, 0x0702, frame_length_lines_shifter);
		if (ret < 0)
			goto p_err_i2c_unlock;
	}

	cis_data->frame_length_lines = frame_length_lines;
	cis_data->max_coarse_integration_time = cis_data->frame_length_lines - cis_data->max_margin_coarse_integration_time;
	cis_data->frame_length_lines_shifter = frame_length_lines_shifter;

#ifdef DEBUG_SENSOR_TIME
	do_gettimeofday(&end);
	dbg_sensor(1, "[%s] time %lu us\n", __func__, (end.tv_sec - st.tv_sec)*1000000 + (end.tv_usec - st.tv_usec));
#endif

p_err_i2c_unlock:
	if (hold > 0) {
		hold = sensor_hm1_cis_group_param_hold_func(subdev, 0x00);
		if (hold < 0)
			ret = hold;
	}
	I2C_MUTEX_UNLOCK(cis->i2c_lock);

p_err:
	return ret;
}

int sensor_hm1_cis_set_frame_rate(struct v4l2_subdev *subdev, u32 min_fps)
{
	int ret = 0;
	struct is_cis *cis;
	cis_shared_data *cis_data;

	u32 frame_duration = 0;

#ifdef DEBUG_SENSOR_TIME
	struct timeval st, end;

	do_gettimeofday(&st);
#endif

	WARN_ON(!subdev);

	cis = (struct is_cis *)v4l2_get_subdevdata(subdev);

	WARN_ON(!cis);
	WARN_ON(!cis->cis_data);

	cis_data = cis->cis_data;

	if (min_fps > cis_data->max_fps) {
		err("[MOD:D:%d] %s, request FPS is too high(%d), set to max(%d)\n",
			cis->id, __func__, min_fps, cis_data->max_fps);
		min_fps = cis_data->max_fps;
	}

	if (min_fps == 0) {
		err("[MOD:D:%d] %s, request FPS is 0, set to min FPS(1)\n",
			cis->id, __func__);
		min_fps = 1;
	}

	frame_duration = (1 * 1000 * 1000) / min_fps;

	dbg_sensor(1, "[MOD:D:%d] %s, set FPS(%d), frame duration(%d)\n",
			cis->id, __func__, min_fps, frame_duration);

	ret = sensor_hm1_cis_set_frame_duration(subdev, frame_duration);
	if (ret < 0) {
		err("[MOD:D:%d] %s, set frame duration is fail(%d)\n",
			cis->id, __func__, ret);
		goto p_err;
	}

#ifdef CAMERA_REAR2
	cis_data->min_frame_us_time = MAX(frame_duration, cis_data->min_sync_frame_us_time);
#else
	cis_data->min_frame_us_time = frame_duration;
#endif

#ifdef DEBUG_SENSOR_TIME
	do_gettimeofday(&end);
	dbg_sensor(1, "[%s] time %lu us\n", __func__, (end.tv_sec - st.tv_sec)*1000000 + (end.tv_usec - st.tv_usec));
#endif

p_err:

	return ret;
}

int sensor_hm1_cis_adjust_analog_gain(struct v4l2_subdev *subdev, u32 input_again, u32 *target_permile)
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

	WARN_ON(!subdev);
	WARN_ON(!target_permile);

	cis = (struct is_cis *)v4l2_get_subdevdata(subdev);

	WARN_ON(!cis);
	WARN_ON(!cis->cis_data);

	cis_data = cis->cis_data;

	again_code = sensor_cis_calc_again_code(input_again);

	if (again_code > cis_data->max_analog_gain[0])
		again_code = cis_data->max_analog_gain[0];
	else if (again_code < cis_data->min_analog_gain[0])
		again_code = cis_data->min_analog_gain[0];

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

int sensor_hm1_cis_set_analog_gain(struct v4l2_subdev *subdev, struct ae_param *again)
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

	WARN_ON(!subdev);
	WARN_ON(!again);

	cis = (struct is_cis *)v4l2_get_subdevdata(subdev);

	WARN_ON(!cis);

	client = cis->client;
	if (unlikely(!client)) {
		err("client is NULL");
		ret = -EINVAL;
		goto p_err;
	}

	sensor_hm1_again_backup.short_val = again->short_val;
	sensor_hm1_again_backup.long_val = again->long_val;

	analog_gain = (u16)sensor_cis_calc_again_code(again->val);

	if (analog_gain < cis->cis_data->min_analog_gain[0]) {
		info("[%s] not proper analog_gain value, reset to min_analog_gain\n", __func__);
		analog_gain = cis->cis_data->min_analog_gain[0];
	}

	if (analog_gain > cis->cis_data->max_analog_gain[0]) {
		info("[%s] not proper analog_gain value, reset to max_analog_gain\n", __func__);
		analog_gain = cis->cis_data->max_analog_gain[0];
	}

	dbg_sensor(1, "[MOD:D:%d] %s(vsync cnt = %d), input_again = %d us, analog_gain(%#x)\n",
		cis->id, __func__, cis->cis_data->sen_vsync_count, again->val, analog_gain);

	I2C_MUTEX_LOCK(cis->i2c_lock);
	hold = sensor_hm1_cis_group_param_hold_func(subdev, 0x01);
	if (hold < 0) {
		ret = hold;
		goto p_err_i2c_unlock;
	}

	ret = is_sensor_write16(client, 0x0204, analog_gain);
	if (ret < 0)
		goto p_err_i2c_unlock;

#ifdef DEBUG_SENSOR_TIME
	do_gettimeofday(&end);
	dbg_sensor(1, "[%s] time %lu us\n", __func__, (end.tv_sec - st.tv_sec)*1000000 + (end.tv_usec - st.tv_usec));
#endif

p_err_i2c_unlock:
	if (hold > 0) {
		hold = sensor_hm1_cis_group_param_hold_func(subdev, 0x00);
		if (hold < 0)
			ret = hold;
	}
	I2C_MUTEX_UNLOCK(cis->i2c_lock);

p_err:
	return ret;
}

int sensor_hm1_cis_get_analog_gain(struct v4l2_subdev *subdev, u32 *again)
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

	WARN_ON(!subdev);
	WARN_ON(!again);

	cis = (struct is_cis *)v4l2_get_subdevdata(subdev);

	WARN_ON(!cis);

	client = cis->client;
	if (unlikely(!client)) {
		err("client is NULL");
		ret = -EINVAL;
		goto p_err;
	}

	I2C_MUTEX_LOCK(cis->i2c_lock);
	hold = sensor_hm1_cis_group_param_hold_func(subdev, 0x01);
	if (hold < 0) {
		ret = hold;
		goto p_err_i2c_unlock;
	}

	ret = is_sensor_read16(client, 0x0204, &analog_gain);
	if (ret < 0)
		goto p_err_i2c_unlock;

	*again = sensor_cis_calc_again_permile(analog_gain);

	dbg_sensor(1, "[MOD:D:%d] %s, cur_again = %d us, analog_gain(%#x)\n",
			cis->id, __func__, *again, analog_gain);

#ifdef DEBUG_SENSOR_TIME
	do_gettimeofday(&end);
	dbg_sensor(1, "[%s] time %lu us\n", __func__, (end.tv_sec - st.tv_sec)*1000000 + (end.tv_usec - st.tv_usec));
#endif

p_err_i2c_unlock:
	if (hold > 0) {
		hold = sensor_hm1_cis_group_param_hold_func(subdev, 0x00);
		if (hold < 0)
			ret = hold;
	}
	I2C_MUTEX_UNLOCK(cis->i2c_lock);

p_err:
	return ret;
}

int sensor_hm1_cis_get_min_analog_gain(struct v4l2_subdev *subdev, u32 *min_again)
{
	int ret = 0;
	struct is_cis *cis;
	cis_shared_data *cis_data;

#ifdef DEBUG_SENSOR_TIME
	struct timeval st, end;

	do_gettimeofday(&st);
#endif

	WARN_ON(!subdev);
	WARN_ON(!min_again);

	cis = (struct is_cis *)v4l2_get_subdevdata(subdev);

	WARN_ON(!cis);
	WARN_ON(!cis->cis_data);

	cis_data = cis->cis_data;
	cis_data->min_analog_gain[0] = 0x20; /* x1, gain=x/0x20 */
	cis_data->min_analog_gain[1] = sensor_cis_calc_again_permile(cis_data->min_analog_gain[0]);

	*min_again = cis_data->min_analog_gain[1];

	dbg_sensor(1, "[%s] code %d, permile %d\n", __func__, cis_data->min_analog_gain[0],
		cis_data->min_analog_gain[1]);

#ifdef DEBUG_SENSOR_TIME
	do_gettimeofday(&end);
	dbg_sensor(1, "[%s] time %lu us\n", __func__, (end.tv_sec - st.tv_sec)*1000000 + (end.tv_usec - st.tv_usec));
#endif

	return ret;
}

int sensor_hm1_cis_get_max_analog_gain(struct v4l2_subdev *subdev, u32 *max_again)
{
	int ret = 0;
	struct is_cis *cis;
	cis_shared_data *cis_data;

#ifdef DEBUG_SENSOR_TIME
	struct timeval st, end;

	do_gettimeofday(&st);
#endif

	WARN_ON(!subdev);
	WARN_ON(!max_again);

	cis = (struct is_cis *)v4l2_get_subdevdata(subdev);

	WARN_ON(!cis);
	WARN_ON(!cis->cis_data);

	cis_data = cis->cis_data;
	cis_data->max_analog_gain[0] = 0x200; /* x16, gain=x/0x20 */
	cis_data->max_analog_gain[1] = sensor_cis_calc_again_permile(cis_data->max_analog_gain[0]);

	*max_again = cis_data->max_analog_gain[1];

	dbg_sensor(1, "[%s] code %d, permile %d\n", __func__, cis_data->max_analog_gain[0],
		cis_data->max_analog_gain[1]);

#ifdef DEBUG_SENSOR_TIME
	do_gettimeofday(&end);
	dbg_sensor(1, "[%s] time %lu us\n", __func__, (end.tv_sec - st.tv_sec)*1000000 + (end.tv_usec - st.tv_usec));
#endif

	return ret;
}

int sensor_hm1_cis_set_digital_gain(struct v4l2_subdev *subdev, struct ae_param *dgain)
{
	int ret = 0;
	int hold = 0;
	struct is_cis *cis;
	struct i2c_client *client;
	cis_shared_data *cis_data;

	u16 long_gain = 0;
	u16 short_gain = 0;

#ifdef DEBUG_SENSOR_TIME
	struct timeval st, end;

	do_gettimeofday(&st);
#endif

	WARN_ON(!subdev);
	WARN_ON(!dgain);

	cis = (struct is_cis *)v4l2_get_subdevdata(subdev);

	WARN_ON(!cis);
	WARN_ON(!cis->cis_data);

	client = cis->client;
	if (unlikely(!client)) {
		err("client is NULL");
		ret = -EINVAL;
		goto p_err;
	}

	sensor_hm1_dgain_backup.short_val = dgain->short_val;
	sensor_hm1_dgain_backup.long_val = dgain->long_val;

	cis_data = cis->cis_data;

	long_gain = (u16)sensor_cis_calc_dgain_code(dgain->long_val);
	short_gain = (u16)sensor_cis_calc_dgain_code(dgain->short_val);

	if (long_gain < cis_data->min_digital_gain[0]) {
		info("[%s] not proper long_gain value, reset to min_digital_gain\n", __func__);
		long_gain = cis_data->min_digital_gain[0];
	}

	if (long_gain > cis_data->max_digital_gain[0]) {
		info("[%s] not proper long_gain value, reset to max_digital_gain\n", __func__);
		long_gain = cis_data->max_digital_gain[0];
	}

	if (short_gain < cis_data->min_digital_gain[0]) {
		info("[%s] not proper short_gain value, reset to min_digital_gain\n", __func__);
		short_gain = cis_data->min_digital_gain[0];
	}

	if (short_gain > cis_data->max_digital_gain[0]) {
		info("[%s] not proper short_gain value, reset to max_digital_gain\n", __func__);
		short_gain = cis_data->max_digital_gain[0];
	}

	dbg_sensor(1, "[MOD:D:%d] %s(vsync cnt = %d), input_dgain = %d/%d us,"
			KERN_CONT "long_gain(%#x), short_gain(%#x)\n",
			cis->id, __func__, cis->cis_data->sen_vsync_count,
			dgain->long_val, dgain->short_val, long_gain, short_gain);

	I2C_MUTEX_LOCK(cis->i2c_lock);
	hold = sensor_hm1_cis_group_param_hold_func(subdev, 0x01);
	if (hold < 0) {
		ret = hold;
		goto p_err_i2c_unlock;
	}

	/* Short digital gain */
	ret = is_sensor_write16(client, 0x020E, short_gain);
	if (ret < 0)
		goto p_err_i2c_unlock;

	/* Long digital gain */
	if (sensor_hm1_cis_is_wdr_mode_on(cis_data)) {
		ret = is_sensor_write16(client, 0x0230, long_gain);
		if (ret < 0)
			goto p_err_i2c_unlock;
	}

#ifdef DEBUG_SENSOR_TIME
	do_gettimeofday(&end);
	dbg_sensor(1, "[%s] time %lu us\n", __func__, (end.tv_sec - st.tv_sec)*1000000 + (end.tv_usec - st.tv_usec));
#endif

p_err_i2c_unlock:
	if (hold > 0) {
		hold = sensor_hm1_cis_group_param_hold_func(subdev, 0x00);
		if (hold < 0)
			ret = hold;
	}
	I2C_MUTEX_UNLOCK(cis->i2c_lock);

p_err:
	return ret;
}

int sensor_hm1_cis_get_digital_gain(struct v4l2_subdev *subdev, u32 *dgain)
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

	WARN_ON(!subdev);
	WARN_ON(!dgain);

	cis = (struct is_cis *)v4l2_get_subdevdata(subdev);

	WARN_ON(!cis);

	client = cis->client;
	if (unlikely(!client)) {
		err("client is NULL");
		ret = -EINVAL;
		goto p_err;
	}

	I2C_MUTEX_LOCK(cis->i2c_lock);
	hold = sensor_hm1_cis_group_param_hold_func(subdev, 0x01);
	if (hold < 0) {
		ret = hold;
		goto p_err_i2c_unlock;
	}

	/*
	 * NOTE : In S5KHM1, digital gain is long/short separated, should set 2 registers like below,
	 * Write same value to : 0x020E : short // GreenB
	 * Write same value to : 0x0214 : short // GreenR
	 * Write same value to : Need To find : long
	 */

	ret = is_sensor_read16(client, 0x020E, &digital_gain);
	if (ret < 0)
		goto p_err_i2c_unlock;

	*dgain = sensor_cis_calc_dgain_permile(digital_gain);

	dbg_sensor(1, "[MOD:D:%d] %s, cur_dgain = %d us, digital_gain(%#x)\n",
			cis->id, __func__, *dgain, digital_gain);

#ifdef DEBUG_SENSOR_TIME
	do_gettimeofday(&end);
	dbg_sensor(1, "[%s] time %lu us\n", __func__, (end.tv_sec - st.tv_sec)*1000000 + (end.tv_usec - st.tv_usec));
#endif

p_err_i2c_unlock:
	if (hold > 0) {
		hold = sensor_hm1_cis_group_param_hold_func(subdev, 0x00);
		if (hold < 0)
			ret = hold;
	}
	I2C_MUTEX_UNLOCK(cis->i2c_lock);

p_err:
	return ret;
}

int sensor_hm1_cis_get_min_digital_gain(struct v4l2_subdev *subdev, u32 *min_dgain)
{
	int ret = 0;
	struct is_cis *cis;
	cis_shared_data *cis_data;

#ifdef DEBUG_SENSOR_TIME
	struct timeval st, end;

	do_gettimeofday(&st);
#endif

	WARN_ON(!subdev);
	WARN_ON(!min_dgain);

	cis = (struct is_cis *)v4l2_get_subdevdata(subdev);

	WARN_ON(!cis);
	WARN_ON(!cis->cis_data);

	cis_data = cis->cis_data;
	cis_data->min_digital_gain[0] = 0x100;
	cis_data->min_digital_gain[1] = sensor_cis_calc_dgain_permile(cis_data->min_digital_gain[0]);

	*min_dgain = cis_data->min_digital_gain[1];

	dbg_sensor(1, "[%s] code %d, permile %d\n", __func__, cis_data->min_digital_gain[0],
		cis_data->min_digital_gain[1]);

#ifdef DEBUG_SENSOR_TIME
	do_gettimeofday(&end);
	dbg_sensor(1, "[%s] time %lu us\n", __func__, (end.tv_sec - st.tv_sec)*1000000 + (end.tv_usec - st.tv_usec));
#endif

	return ret;
}

int sensor_hm1_cis_get_max_digital_gain(struct v4l2_subdev *subdev, u32 *max_dgain)
{
	int ret = 0;
	struct is_cis *cis;
	cis_shared_data *cis_data;

#ifdef DEBUG_SENSOR_TIME
	struct timeval st, end;

	do_gettimeofday(&st);
#endif

	WARN_ON(!subdev);
	WARN_ON(!max_dgain);

	cis = (struct is_cis *)v4l2_get_subdevdata(subdev);

	WARN_ON(!cis);
	WARN_ON(!cis->cis_data);

	cis_data = cis->cis_data;
	cis_data->max_digital_gain[0] = 0x1000;
	cis_data->max_digital_gain[1] = sensor_cis_calc_dgain_permile(cis_data->max_digital_gain[0]);

	*max_dgain = cis_data->max_digital_gain[1];

	dbg_sensor(1, "[%s] code %d, permile %d\n", __func__, cis_data->max_digital_gain[0],
		cis_data->max_digital_gain[1]);

#ifdef DEBUG_SENSOR_TIME
	do_gettimeofday(&end);
	dbg_sensor(1, "[%s] time %lu us\n", __func__, (end.tv_sec - st.tv_sec)*1000000 + (end.tv_usec - st.tv_usec));
#endif

	return ret;
}

int sensor_hm1_cis_long_term_exposure(struct v4l2_subdev *subdev)
{
	int ret = 0;
	struct is_cis *cis;
	struct is_long_term_expo_mode *lte_mode;
	unsigned char cit_lshift_val = 0;
	unsigned char shift_count = 0;
#ifdef USE_SENSOR_LONG_EXPOSURE_SHOT
	u32 lte_expousre = 0;
#endif
	WARN_ON(!subdev);

	cis = (struct is_cis *)v4l2_get_subdevdata(subdev);
	lte_mode = &cis->long_term_mode;

	I2C_MUTEX_LOCK(cis->i2c_lock);
	/* LTE mode or normal mode set */
	if (lte_mode->sen_strm_off_on_enable) {
		if (lte_mode->expo[0] > 125000) {
#ifdef USE_SENSOR_LONG_EXPOSURE_SHOT
			lte_expousre = lte_mode->expo[0];
			cit_lshift_val = (unsigned char)(lte_mode->expo[0] / 125000);
			while (cit_lshift_val) {
				cit_lshift_val = cit_lshift_val / 2;
				lte_expousre = lte_expousre / 2;
				shift_count++;
			}
			lte_mode->expo[0] = lte_expousre;
#else
			cit_lshift_val = (unsigned char)(lte_mode->expo[0] / 125000);
			while (cit_lshift_val) {
				cit_lshift_val = cit_lshift_val / 2;
				if (cit_lshift_val > 0)
					shift_count++;
			}
			lte_mode->expo[0] = 125000;
#endif
			ret |= is_sensor_write16(cis->client, 0xFCFC, 0x4000);
			ret |= is_sensor_write8(cis->client, 0x0702, shift_count);
			ret |= is_sensor_write8(cis->client, 0x0704, shift_count);
		}
	} else {
		cit_lshift_val = 0;
		ret |= is_sensor_write16(cis->client, 0xFCFC, 0x4000);
		ret |= is_sensor_write8(cis->client, 0x0702, cit_lshift_val);
		ret |= is_sensor_write8(cis->client, 0x0704, cit_lshift_val);
	}

	I2C_MUTEX_UNLOCK(cis->i2c_lock);

	info("%s enable(%d)", __func__, lte_mode->sen_strm_off_on_enable);

	if (ret < 0) {
		pr_err("ERR[%s]: LTE register setting fail\n", __func__);
		return ret;
	}

	return ret;
}

int sensor_hm1_cis_set_factory_control(struct v4l2_subdev *subdev, u32 command)
{
	int ret = 0;
	struct is_cis *cis = NULL;
	struct is_module_enum *module;
	struct is_device_sensor_peri *sensor_peri = NULL;
	struct sensor_open_extended *ext_info = NULL;

	WARN_ON(!subdev);

	cis = (struct is_cis *)v4l2_get_subdevdata(subdev);
	WARN_ON(!cis);
	WARN_ON(!cis->cis_data);

	sensor_peri = container_of(cis, struct is_device_sensor_peri, cis);
	module = sensor_peri->module;
	ext_info = &module->ext;
	WARN_ON(!ext_info);

	switch (command) {
	case FAC_CTRL_BIT_TEST:
		pr_info("[%s] FAC_CTRL_BIT_TEST to be checked\n", __func__);
#if 0
		ret |= is_sensor_write16(cis->client, 0xFCFC, 0x4000);
		ret |= is_sensor_write16(cis->client, 0xF44A, 0x0009); // TG 2.55v -> 2.45v
		ret |= is_sensor_write16(cis->client, 0xF44C, 0x0009); // TG 2.55v -> 2.45v
		msleep(50);
		ret |= is_sensor_write16(cis->client, 0xFCFC, 0x2000);
		ret |= is_sensor_write16(cis->client, 0xAE16, 0x0010); // RG 3.70v -> 3.30v
#endif
		break;
	default:
		pr_info("[%s] not support command(%d)\n", __func__, command);
	}

	return ret;
}

int sensor_hm1_cis_compensate_gain_for_extremely_br(struct v4l2_subdev *subdev, u32 expo, u32 *again, u32 *dgain)
{
	int ret = 0;
	struct is_cis *cis;
	cis_shared_data *cis_data;

	u64 vt_pic_clk_freq_khz = 0;
	u32 line_length_pck = 0;
	u32 min_fine_int = 0;
	u32 coarse_int = 0;
	u32 compensated_again = 0;

	FIMC_BUG(!subdev);
	FIMC_BUG(!again);
	FIMC_BUG(!dgain);

	cis = (struct is_cis *)v4l2_get_subdevdata(subdev);
	if (!cis) {
		err("cis is NULL");
		ret = -EINVAL;
		goto p_err;
	}
	cis_data = cis->cis_data;

	vt_pic_clk_freq_khz = cis_data->pclk / (1000);
	line_length_pck = cis_data->line_length_pck;
	min_fine_int = cis_data->min_fine_integration_time;

	if (line_length_pck <= 0) {
		err("[%s] invalid line_length_pck(%d)\n", __func__, line_length_pck);
		goto p_err;
	}

	coarse_int = ((expo * vt_pic_clk_freq_khz) / 1000 - min_fine_int) / line_length_pck;
	if (coarse_int < cis_data->min_coarse_integration_time) {
		dbg_sensor(1, "[MOD:D:%d] %s, vsync_cnt(%d), long coarse(%d) min(%d)\n", cis->id, __func__,
			cis_data->sen_vsync_count, coarse_int, cis_data->min_coarse_integration_time);
		coarse_int = cis_data->min_coarse_integration_time;
	}

	if (coarse_int <= 1024) {
		compensated_again = (*again * ((expo * vt_pic_clk_freq_khz) / 1000 - min_fine_int)) / (line_length_pck * coarse_int);

		if (compensated_again < cis_data->min_analog_gain[1]) {
			*again = cis_data->min_analog_gain[1];
		} else if (*again >= cis_data->max_analog_gain[1]) {
			*dgain = (*dgain * ((expo * vt_pic_clk_freq_khz) / 1000 - min_fine_int)) / (line_length_pck * coarse_int);
		} else {
			//*again = compensated_again;
			*dgain = (*dgain * ((expo * vt_pic_clk_freq_khz) / 1000 - min_fine_int)) / (line_length_pck * coarse_int);
		}

		dbg_sensor(1, "[%s] exp(%d), again(%d), dgain(%d), coarse_int(%d), compensated_again(%d)\n",
			__func__, expo, *again, *dgain, coarse_int, compensated_again);
	}

p_err:
	return ret;
}

int sensor_hm1_cis_set_wb_gain(struct v4l2_subdev *subdev, struct wb_gains wb_gains)
{
	int ret = 0;
	int hold = 0;
	struct is_cis *cis;
	struct i2c_client *client;
	int mode = 0;
	u16 abs_gains[3] = {0, }; /* R, G, B */
	u32 avg_g = 0, div = 0;

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

	if (!cis->use_wb_gain)
		return ret;

	mode = cis->cis_data->sens_config_index_cur;
	if (!sensor_hm1_support_wb_gain[mode]) {
		return 0;
	}

	if (wb_gains.gr != wb_gains.gb) {
		err("gr, gb not euqal"); /* check DDK layer */
		return -EINVAL;
	}

	if (wb_gains.gr == 1024)
		div = 4;
	else if (wb_gains.gr == 2048)
		div = 8;
	else {
		err("invalid gr,gb %d", wb_gains.gr); /* check DDK layer */
		return -EINVAL;
	}

	dbg_sensor(1, "[SEN:%d]%s:DDK vlaue: wb_gain_gr(%d), wb_gain_r(%d), wb_gain_b(%d), wb_gain_gb(%d)\n",
		cis->id, __func__, wb_gains.gr, wb_gains.r, wb_gains.b, wb_gains.gb);

	avg_g = (wb_gains.gr + wb_gains.gb) / 2;
	abs_gains[0] = (u16)((wb_gains.r / div) & 0xFFFF);
	abs_gains[1] = (u16)((avg_g / div) & 0xFFFF);
	abs_gains[2] = (u16)((wb_gains.b / div) & 0xFFFF);

	dbg_sensor(1, "[SEN:%d]%s, abs_gain_r(0x%4X), abs_gain_g(0x%4X), abs_gain_b(0x%4X)\n",
		cis->id, __func__, abs_gains[0], abs_gains[1], abs_gains[2]);

	hold = sensor_hm1_cis_group_param_hold(subdev, 0x01);
	if (hold < 0) {
		ret = hold;
		goto p_err;
	}

	I2C_MUTEX_LOCK(cis->i2c_lock);
	ret |= is_sensor_write16(client, 0xFCFC, 0x4000);
	ret |= is_sensor_write16(client, 0x0D82, abs_gains[0]);
	ret |= is_sensor_write16(client, 0x0D84, abs_gains[1]);
	ret |= is_sensor_write16(client, 0x0D86, abs_gains[2]);
	if (ret < 0) {
		err("sensor_hm1_set_registers fail!!");
		goto p_i2c_err;
	}

#ifdef DEBUG_SENSOR_TIME
	do_gettimeofday(&end);
	dbg_sensor(1, "[%s] time %lu us\n", __func__, (end.tv_sec - st.tv_sec)*1000000 + (end.tv_usec - st.tv_usec));
#endif

p_i2c_err:
	I2C_MUTEX_UNLOCK(cis->i2c_lock);

p_err:
	if (hold > 0) {
		hold = sensor_hm1_cis_group_param_hold(subdev, 0x00);
		if (hold < 0)
			ret = hold;
	}

	return ret;
}

int sensor_hm1_cis_recover_stream_on(struct v4l2_subdev *subdev)
{
	int ret = 0;
	struct is_cis *cis = NULL;

	FIMC_BUG(!subdev);

	cis = (struct is_cis *)v4l2_get_subdevdata(subdev);
	FIMC_BUG(!cis);
	FIMC_BUG(!cis->cis_data);

	info("%s start\n", __func__);

	ret = sensor_hm1_cis_set_global_setting(subdev);
	if (ret < 0) goto p_err;
	ret = sensor_hm1_cis_mode_change(subdev, cis->cis_data->sens_config_index_cur);
	if (ret < 0) goto p_err;
	ret = sensor_hm1_cis_set_frame_duration(subdev, sensor_hm1_frame_duration_backup);
	if (ret < 0) goto p_err;
	ret = sensor_hm1_cis_set_analog_gain(subdev, &sensor_hm1_again_backup);
	if (ret < 0) goto p_err;
	ret = sensor_hm1_cis_set_digital_gain(subdev, &sensor_hm1_dgain_backup);
	if (ret < 0) goto p_err;
	ret = sensor_hm1_cis_set_exposure_time(subdev, &sensor_hm1_target_exp_backup);
	if (ret < 0) goto p_err;
	ret = sensor_hm1_cis_stream_on(subdev);
	if (ret < 0) goto p_err;
	ret = sensor_cis_wait_streamon(subdev);
	if (ret < 0) goto p_err;

	info("%s end\n", __func__);
p_err:
	return ret;
}

int sensor_hm1_cis_set_fake_retention(struct v4l2_subdev *subdev, bool enable)
{
	struct is_cis *cis = NULL;

	FIMC_BUG(!subdev);

	cis = (struct is_cis *)v4l2_get_subdevdata(subdev);
	FIMC_BUG(!cis);
	FIMC_BUG(!cis->cis_data);

	info("%s(%d)\n", __func__, enable);
	sensor_hm1_fake_retention_status = enable;

	return 0;
}

static struct is_cis_ops cis_ops_hm1 = {
	.cis_init = sensor_hm1_cis_init,
	.cis_log_status = sensor_hm1_cis_log_status,
	.cis_group_param_hold = sensor_hm1_cis_group_param_hold,
	.cis_set_global_setting = sensor_hm1_cis_set_global_setting,
	.cis_mode_change = sensor_hm1_cis_mode_change,
	.cis_set_size = sensor_hm1_cis_set_size,
	.cis_stream_on = sensor_hm1_cis_stream_on,
	.cis_stream_off = sensor_hm1_cis_stream_off,
	.cis_set_exposure_time = sensor_hm1_cis_set_exposure_time,
	.cis_get_min_exposure_time = sensor_hm1_cis_get_min_exposure_time,
	.cis_get_max_exposure_time = sensor_hm1_cis_get_max_exposure_time,
	.cis_adjust_frame_duration = sensor_hm1_cis_adjust_frame_duration,
	.cis_set_frame_duration = sensor_hm1_cis_set_frame_duration,
	.cis_set_frame_rate = sensor_hm1_cis_set_frame_rate,
	.cis_adjust_analog_gain = sensor_hm1_cis_adjust_analog_gain,
	.cis_set_analog_gain = sensor_hm1_cis_set_analog_gain,
	.cis_get_analog_gain = sensor_hm1_cis_get_analog_gain,
	.cis_get_min_analog_gain = sensor_hm1_cis_get_min_analog_gain,
	.cis_get_max_analog_gain = sensor_hm1_cis_get_max_analog_gain,
	.cis_set_digital_gain = sensor_hm1_cis_set_digital_gain,
	.cis_get_digital_gain = sensor_hm1_cis_get_digital_gain,
	.cis_get_min_digital_gain = sensor_hm1_cis_get_min_digital_gain,
	.cis_get_max_digital_gain = sensor_hm1_cis_get_max_digital_gain,
	.cis_compensate_gain_for_extremely_br = sensor_hm1_cis_compensate_gain_for_extremely_br,
	.cis_wait_streamoff = sensor_cis_wait_streamoff,
	.cis_wait_streamon = sensor_hm1_cis_wait_streamon,
	.cis_set_wb_gains = sensor_hm1_cis_set_wb_gain,
	.cis_data_calculation = sensor_hm1_cis_data_calc,
	.cis_set_long_term_exposure = sensor_hm1_cis_long_term_exposure,
	.cis_check_rev_on_init = sensor_cis_check_rev_on_init,
	.cis_set_initial_exposure = sensor_cis_set_initial_exposure,
//	.cis_recover_stream_on = sensor_hm1_cis_recover_stream_on,
	.cis_set_factory_control = sensor_hm1_cis_set_factory_control,
	.cis_set_fake_retention = sensor_hm1_cis_set_fake_retention,
};

static int cis_hm1_probe(struct i2c_client *client,
	const struct i2c_device_id *id)
{
	int ret = 0;
	struct is_core *core = NULL;
	struct v4l2_subdev *subdev_cis = NULL;
	struct is_cis *cis = NULL;
	struct is_device_sensor *device = NULL;
	struct is_device_sensor_peri *sensor_peri = NULL;
	u32 sensor_id[IS_STREAM_COUNT] = {0, };
	u32 sensor_id_len;
	const u32 *sensor_id_spec;
	char const *setfile;
	struct device *dev;
	struct device_node *dnode;
	int i;
	int index;
	const int *verify_sensor_mode = NULL;
	int verify_sensor_mode_size = 0;

	WARN_ON(!client);
	WARN_ON(!is_dev);

	core = (struct is_core *)dev_get_drvdata(is_dev);
	if (!core) {
		probe_info("core device is not yet probed");
		return -EPROBE_DEFER;
	}

	dev = &client->dev;
	dnode = dev->of_node;

	sensor_id_spec = of_get_property(dnode, "id", &sensor_id_len);
	if (!sensor_id_spec) {
		err("sensor_id num read is fail(%d)", ret);
		goto p_err;
	}

	sensor_id_len /= (unsigned int)sizeof(*sensor_id_spec);

	probe_info("%s sensor_id_spec %d, sensor_id_len %d\n", __func__,
			*sensor_id_spec, sensor_id_len);

	ret = of_property_read_u32_array(dnode, "id", sensor_id, sensor_id_len);
	if (ret) {
		err("sensor_id read is fail(%d)", ret);
		goto p_err;
	}

	for (i = 0; i < sensor_id_len; i++) {
		probe_info("%s sensor_id %d\n", __func__, sensor_id[i]);
		device = &core->sensor[sensor_id[i]];

		sensor_peri = find_peri_by_cis_id(device, SENSOR_NAME_S5KHM1);
		if (!sensor_peri) {
			probe_info("sensor peri is net yet probed");
			return -EPROBE_DEFER;
		}
	}

	for (i = 0; i < sensor_id_len; i++) {
		device = &core->sensor[sensor_id[i]];
		sensor_peri = find_peri_by_cis_id(device, SENSOR_NAME_S5KHM1);

		cis = &sensor_peri->cis;
		subdev_cis = kzalloc(sizeof(struct v4l2_subdev), GFP_KERNEL);
		if (!subdev_cis) {
			probe_err("subdev_cis is NULL");
			ret = -ENOMEM;
			goto p_err;
		}

		sensor_peri->subdev_cis = subdev_cis;

		cis->id = SENSOR_NAME_S5KHM1;
		cis->subdev = subdev_cis;
		cis->device = sensor_id[i];
		cis->client = client;
		sensor_peri->module->client = cis->client;
		cis->i2c_lock = NULL;
		cis->ctrl_delay = N_PLUS_TWO_FRAME;
		cis->mipi_clock_index_cur = CAM_MIPI_NOT_INITIALIZED;
		cis->mipi_clock_index_new = CAM_MIPI_NOT_INITIALIZED;
		cis->cis_data = kzalloc(sizeof(cis_shared_data), GFP_KERNEL);
		if (!cis->cis_data) {
			err("cis_data is NULL");
			ret = -ENOMEM;
			goto p_err;
		}

		cis->cis_ops = &cis_ops_hm1;

		/* belows are depend on sensor cis. MUST check sensor spec */
		cis->bayer_order = OTF_INPUT_ORDER_BAYER_GR_BG;

		if (of_property_read_bool(dnode, "sensor_f_number")) {
			ret = of_property_read_u32(dnode, "sensor_f_number", &cis->aperture_num);
			if (ret)
				warn("f-number read is fail(%d)", ret);
		} else {
			cis->aperture_num = F1_5;
		}

		probe_info("%s f-number %d\n", __func__, cis->aperture_num);

		cis->use_dgain = true;
		cis->use_wb_gain = true;
		cis->hdr_ctrl_by_again = false;

		v4l2_set_subdevdata(subdev_cis, cis);
		v4l2_set_subdev_hostdata(subdev_cis, device);
		snprintf(subdev_cis->name, V4L2_SUBDEV_NAME_SIZE, "cis-subdev.%d", cis->id);

		sensor_cis_parse_dt(dev, cis->subdev);
	}

	sensor_hm1_eeprom_cal_available = false;
	sensor_hm1_first_entrance = false;
	sensor_hm1_fake_retention_status = false;

	cis->use_initial_ae = of_property_read_bool(dnode, "use_initial_ae");
	probe_info("%s use initial_ae(%d)\n", __func__, cis->use_initial_ae);

	cis->vendor_use_adaptive_mipi = of_property_read_bool(dnode, "vendor_use_adaptive_mipi");
	probe_info("%s vendor_use_adaptive_mipi(%d)\n", __func__, cis->vendor_use_adaptive_mipi);

	ret = of_property_read_string(dnode, "setfile", &setfile);
	if (ret) {
		err("setfile index read fail(%d), take default setfile!!", ret);
		setfile = "default";
	}

	if (strcmp(setfile, "default") == 0 ||
			strcmp(setfile, "setA") == 0) {
		probe_info("%s setfile_A\n", __func__);
		sensor_hm1_initial = sensor_hm1_setfile_A_Initial;
		sensor_hm1_initial_size  = ARRAY_SIZE(sensor_hm1_setfile_A_Initial);
		sensor_hm1_tnp = sensor_hm1_setfile_A_TnP;
		sensor_hm1_tnp_size  = ARRAY_SIZE(sensor_hm1_setfile_A_TnP);
		sensor_hm1_global = sensor_hm1_setfile_A_Global;
		sensor_hm1_global_size = ARRAY_SIZE(sensor_hm1_setfile_A_Global);
		sensor_hm1_global_for_cal = sensor_hm1_setfile_A_Global_For_Calibration;
		sensor_hm1_global_for_cal_size = ARRAY_SIZE(sensor_hm1_setfile_A_Global_For_Calibration);
		sensor_hm1_setfiles = sensor_hm1_setfiles_A;
		sensor_hm1_setfile_sizes = sensor_hm1_setfile_A_sizes;
		sensor_hm1_pllinfos = sensor_hm1_pllinfos_A;
		sensor_hm1_max_setfile_num = ARRAY_SIZE(sensor_hm1_setfiles_A);
		cis->mipi_sensor_mode = sensor_hm1_setfile_A_mipi_sensor_mode;
		cis->mipi_sensor_mode_size = ARRAY_SIZE(sensor_hm1_setfile_A_mipi_sensor_mode);
		verify_sensor_mode = sensor_hm1_setfile_A_verify_sensor_mode;
		verify_sensor_mode_size = ARRAY_SIZE(sensor_hm1_setfile_A_verify_sensor_mode);
	} else {
		err("%s setfile index out of bound, take default (setfile_A)", __func__);
		sensor_hm1_initial = sensor_hm1_setfile_A_Initial;
		sensor_hm1_initial_size  = ARRAY_SIZE(sensor_hm1_setfile_A_Initial);
		sensor_hm1_tnp = sensor_hm1_setfile_A_TnP;
		sensor_hm1_tnp_size  = ARRAY_SIZE(sensor_hm1_setfile_A_TnP);
		sensor_hm1_global = sensor_hm1_setfile_A_Global;
		sensor_hm1_global_size = ARRAY_SIZE(sensor_hm1_setfile_A_Global);
		sensor_hm1_global_for_cal = sensor_hm1_setfile_A_Global_For_Calibration;
		sensor_hm1_global_for_cal_size = ARRAY_SIZE(sensor_hm1_setfile_A_Global_For_Calibration);
		sensor_hm1_setfiles = sensor_hm1_setfiles_A;
		sensor_hm1_setfile_sizes = sensor_hm1_setfile_A_sizes;
		sensor_hm1_pllinfos = sensor_hm1_pllinfos_A;
		sensor_hm1_max_setfile_num = ARRAY_SIZE(sensor_hm1_setfiles_A);
		cis->mipi_sensor_mode = sensor_hm1_setfile_A_mipi_sensor_mode;
		cis->mipi_sensor_mode_size = ARRAY_SIZE(sensor_hm1_setfile_A_mipi_sensor_mode);
		verify_sensor_mode = sensor_hm1_setfile_A_verify_sensor_mode;
		verify_sensor_mode_size = ARRAY_SIZE(sensor_hm1_setfile_A_verify_sensor_mode);
	}

	if (cis->vendor_use_adaptive_mipi) {
		for (i = 0; i < verify_sensor_mode_size; i++) {
			index = verify_sensor_mode[i];

			if (index >= cis->mipi_sensor_mode_size || index < 0) {
				panic("wrong mipi_sensor_mode index");
				break;
			}

			if (is_vendor_verify_mipi_channel(cis->mipi_sensor_mode[index].mipi_channel,
						cis->mipi_sensor_mode[index].mipi_channel_size)) {
				panic("wrong mipi channel");
				break;
			}
		}
	}

	probe_info("%s done\n", __func__);

p_err:
	return ret;
}

static const struct of_device_id sensor_cis_hm1_match[] = {
	{
		.compatible = "samsung,exynos-is-cis-hm1",
	},
	{},
};
MODULE_DEVICE_TABLE(of, sensor_cis_hm1_match);

static const struct i2c_device_id sensor_cis_hm1_idt[] = {
	{ SENSOR_NAME, 0 },
	{},
};

static struct i2c_driver sensor_cis_hm1_driver = {
	.probe	= cis_hm1_probe,
	.driver = {
		.name	= SENSOR_NAME,
		.owner	= THIS_MODULE,
		.of_match_table = sensor_cis_hm1_match,
		.suppress_bind_attrs = true,
	},
	.id_table = sensor_cis_hm1_idt
};

#ifdef MODULE
builtin_i2c_driver(sensor_cis_hm1_driver);
#else
static int __init sensor_cis_hm1_init(void)
{
	int ret;

	ret = i2c_add_driver(&sensor_cis_hm1_driver);
	if (ret)
		err("failed to add %s driver: %d\n",
			sensor_cis_hm1_driver.driver.name, ret);

	return ret;
}
late_initcall_sync(sensor_cis_hm1_init);
#endif

MODULE_LICENSE("GPL");
