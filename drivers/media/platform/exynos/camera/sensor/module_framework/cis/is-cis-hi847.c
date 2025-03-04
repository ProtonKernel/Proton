/*
 * Samsung Exynos SoC series Sensor driver
 *
 *
 * Copyright (c) 2020 Samsung Electronics Co., Ltd
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
#include "is-cis-hi847.h"
#include "is-cis-hi847-setA.h"
#include "is-cis-hi847-setB.h"
#include "is-helper-i2c.h"
#include "is-sec-define.h"
#ifdef CAMERA_REAR2_SENSOR_SHIFT_CROP
#include "is-vender.h"
#endif

#define SENSOR_NAME "Hi847"

#define POLL_TIME_MS 5
#define STREAM_MAX_POLL_CNT 60

static const struct v4l2_subdev_ops subdev_ops;

static const u32 *sensor_hi847_global;
static u32 sensor_hi847_global_size;
static const u32 **sensor_hi847_setfiles;
static const u32 *sensor_hi847_setfile_sizes;
static const struct sensor_pll_info_compact **sensor_hi847_pllinfos;
static u32 sensor_hi847_max_setfile_num;
static const u32 *sensor_hi847_otp_initial;
static u32 sensor_hi847_otp_initial_size;

static void sensor_hi847_cis_data_calculation(const struct sensor_pll_info_compact *pll_info, cis_shared_data *cis_data)
{
	u32 vt_pix_clk_hz = 0;
	u32 total_pixels = 0;
	u32 frame_rate = 0, max_fps = 0, frame_valid_us = 0;

	FIMC_BUG_VOID(!pll_info);

	/* 1. get pclk value from pll info */
	vt_pix_clk_hz = pll_info->pclk;
	total_pixels = pll_info->frame_length_lines * pll_info->line_length_pck;

	/* 2. the time of processing one frame calculation (us) */
	cis_data->min_frame_us_time = (((u64)pll_info->frame_length_lines * pll_info->line_length_pck * 1000)
				/ (vt_pix_clk_hz / 1000));
	cis_data->cur_frame_us_time = cis_data->min_frame_us_time;
#ifdef CAMERA_REAR2
	cis_data->min_sync_frame_us_time = cis_data->min_frame_us_time;
#endif

	/* 3. FPS calculation */
	frame_rate = vt_pix_clk_hz / (pll_info->frame_length_lines * pll_info->line_length_pck);
	dbg_sensor(1, "[Hi847 data calculate] frame_rate (%u) = vt_pix_clk_hz(%u) / "
		KERN_CONT "(pll_info->frame_length_lines(%u) * pll_info->line_length_pck(%u))\n",
		frame_rate, vt_pix_clk_hz, pll_info->frame_length_lines, pll_info->line_length_pck);

	/* calculate max fps */
	max_fps = (vt_pix_clk_hz * 10) / (pll_info->frame_length_lines * pll_info->line_length_pck);
	max_fps = (max_fps % 10 >= 5 ? frame_rate + 1 : frame_rate);

	cis_data->pclk = vt_pix_clk_hz;
	cis_data->max_fps = max_fps;
	cis_data->frame_length_lines = pll_info->frame_length_lines;
	cis_data->line_length_pck = pll_info->line_length_pck;
	cis_data->line_readOut_time = sensor_cis_do_div64((u64)cis_data->line_length_pck * (u64)(1000 * 1000 * 1000), cis_data->pclk);
	cis_data->rolling_shutter_skew = (cis_data->cur_height - 1) * cis_data->line_readOut_time;
	cis_data->stream_on = false;

	/* Frame valid time calcuration */
	frame_valid_us = sensor_cis_do_div64((u64)cis_data->cur_height * (u64)cis_data->line_length_pck * (u64)(1000 * 1000), cis_data->pclk);
	cis_data->frame_valid_us_time = (int)frame_valid_us;

	dbg_sensor(1, "[HI847 data calculate] Sensor size(%d x %d) setting SUCCESS!\n",
	                cis_data->cur_width, cis_data->cur_height);
	dbg_sensor(1, "[HI847 data calculate] Frame Valid(%d us)\n", frame_valid_us);
	dbg_sensor(1, "[HI847 data calculate] rolling_shutter_skew(%lld)\n", cis_data->rolling_shutter_skew);
	dbg_sensor(1, "[HI847 data calculate] fps(%d), max fps(%d)\n", frame_rate, cis_data->max_fps);
	dbg_sensor(1, "[HI847 data calculate] min_frame_time(%d us)\n", cis_data->min_frame_us_time);
	dbg_sensor(1, "[HI847 data calculate] pixel rate (%d Kbps)\n", cis_data->pclk / 1000);

	/* Frame period calculation */
	cis_data->frame_time = (cis_data->line_readOut_time * cis_data->cur_height / 1000);

	dbg_sensor(1, "[HI847 data calculate] frame_time(%d), rolling_shutter_skew(%lld)\n",
		cis_data->frame_time, cis_data->rolling_shutter_skew);

	/* Constant values */
	cis_data->min_fine_integration_time = SENSOR_HI847_FINE_INTEGRATION_TIME_MIN;
	cis_data->max_fine_integration_time = SENSOR_HI847_FINE_INTEGRATION_TIME_MAX;
	cis_data->min_coarse_integration_time = SENSOR_HI847_COARSE_INTEGRATION_TIME_MIN;
	cis_data->max_margin_coarse_integration_time= SENSOR_HI847_COARSE_INTEGRATION_TIME_MAX_MARGIN;
}

int sensor_hi847_cis_check_rev(struct is_cis *cis)
{
#if SENSOR_HI847_OTP_READ
	u32 ret = 0;
	u8 rev1 = 0;
	u8 rev2 = 0;
	struct device *dev = NULL;
	struct i2c_client *client = NULL;
	struct device_node *dnode = NULL;
	int gpio_reset = 0;

	FIMC_BUG(!cis);
	FIMC_BUG(!cis->cis_data);

	client = cis->client;
	FIMC_BUG(!client);

	dev = &client->dev;
	FIMC_BUG(!dev);

	dnode = dev->of_node;

	/* OTP ROM Read Sequence : Hi-847_OTP Chip ID read guide_v3.0_20200630.pdf */
	probe_info("%s otp rom read start\n", __func__);

	I2C_MUTEX_LOCK(cis->i2c_lock);

	/* OTP initial setting */
	ret = sensor_cis_set_registers(cis->subdev, sensor_hi847_otp_initial, sensor_hi847_otp_initial_size);
	if (ret < 0) {
		err("sensor_hi847 otp initial setting fail!!!");
		goto p_err;
	}

	/* read otp sequence setting */
	ret = is_sensor_write8(client, 0x0B00, 0x00);
	if (ret < 0) {
		err("sensor_hi847 standby on setting fail!!!");
		goto p_err;
	}
	ret = is_sensor_write8(client, 0x027E, 0x00);
	if (ret < 0) {
		err("sensor_hi847 tg disable setting fail!!!");
		goto p_err;
	}

	usleep_range(10000, 10000);

	ret = is_sensor_write8(client, 0x0260, 0x10);
	if (ret < 0) {
		err("sensor_hi847 otp mode enable setting fail!!!");
		goto p_err;
	}
	ret = is_sensor_write8(client, 0x027E, 0x01);
	if (ret < 0) {
		err("sensor_hi847 tg enable setting fail!!!");
		goto p_err;
	}
	ret = is_sensor_write8(client, 0x0B00, 0x01);
	if (ret < 0) {
		err("sensor_hi847 stream on setting fail!!!");
		goto p_err;
	}

	usleep_range(1000, 1000);

	ret = is_sensor_write8(client, 0x030A, 0x00);
	if (ret < 0) {
		err("sensor_hi847 otp address h setting fail!!!");
		goto p_err;
	}

#if 1		// for only chip revision
	ret = is_sensor_write8(client, 0x030B, 0x0C);
	if (ret < 0) {
		err("sensor_hi847 otp address l setting fail!!!");
		goto p_err;
	}
	ret = is_sensor_write8(client, 0x0302, 0x01);
	if (ret < 0) {
		err("sensor_hi847 continuos read enable setting fail!!!");
		goto p_err;
	}

	/* read chip revision id */
	ret = is_sensor_read8(client, 0x0308, &rev1);
	if (ret < 0) {
		err("sensor_hi847 read rev1 fail!!!");
		goto p_err;
	}

	ret = is_sensor_read8(client, 0x0308, &rev2);
	if (ret < 0) {
		err("sensor_hi847 read rev1 fail!!!");
		goto p_err;
	}
#else	// for all chip info
	ret = is_sensor_write8(client, 0x030B, 0x01);
	if (ret < 0) {
		err("sensor_hi847 otp address l setting fail!!!");
		goto p_err;
	}
	ret = is_sensor_write8(client, 0x0302, 0x01);
	if (ret < 0) {
		err("sensor_hi847 continuos read enable setting fail!!!");
		goto p_err;
	}

	{
		int i = 0;
		u8 read_val[13] = {0, };

		for (i = 0; i < 13; i++) {
			ret = is_sensor_read8(client, 0x0308, &read_val[i]);
			if (ret < 0) {
				err("sensor_hi847 otp read fail (%d)!\n", i);
				goto p_err;
			}

			info("[%s] %d bit value = 0x%X\n", __func__, i, read_val[i]);
		}

		rev1 = read_val[11];
		rev2 = read_val[12];
	}
#endif

	/* OTP read off */
	ret = is_sensor_write8(client, 0x0B00, 0x00);
	if (ret < 0) {
		err("sensor_hi847 standby on setting fail!!!");
		goto p_err;
	}
	ret = is_sensor_write8(client, 0x027E, 0x00);
	if (ret < 0) {
		err("sensor_hi847 tg disable setting fail!!!");
		goto p_err;
	}

	usleep_range(10000, 10000);

	ret = is_sensor_write8(client, 0x0260, 0x00);
	if (ret < 0) {
		err("sensor_hi847 otp mode enable setting fail!!!");
		goto p_err;
	}
	ret = is_sensor_write8(client, 0x027E, 0x01);
	if (ret < 0) {
		err("sensor_hi847 tg enable setting fail!!!");
		goto p_err;
	}
	ret = is_sensor_write8(client, 0x0B00, 0x01);
	if (ret < 0) {
		err("sensor_hi847 stream on setting fail!!!");
		goto p_err;
	}

	/* XSHUTDOWN RESET */
	gpio_reset = of_get_named_gpio(dnode, "gpio_reset", 0);
	if (!gpio_is_valid(gpio_reset)) {
		err("failed to get PIN_RESET!!!");
		goto p_err;
	}

	gpio_request_one(gpio_reset, GPIOF_OUT_INIT_HIGH, "HI847_XRESET_OUTPUT_HIGH");
	if (ret < 0) {
		err("sensor_hi847 gpio request high err\n");
		ret = 0;
	}

	usleep_range(1000, 1000);

	gpio_set_value(gpio_reset, 0);
	usleep_range(500, 500);
	gpio_set_value(gpio_reset, 1);
	usleep_range(200, 200);
	gpio_free(gpio_reset);

	cis->cis_data->cis_rev = (((u16)rev1 << 8) & 0xFF00) | (rev2 & 0xFF);
	probe_info("[%s] chip revision = %#x\n", __func__, cis->cis_data->cis_rev);

	cis->rev_flag = true;

p_err:
	I2C_MUTEX_UNLOCK(cis->i2c_lock);

#else
	int ret = 0;
	u8 rev1 = 0x00;
	u8 rev2 = 0x10;

	cis->cis_data->cis_rev = (((u16)rev1 << 8) & 0xFF00) + (rev2 & 0xFF);
	probe_info("[%s] chip revision = %#x\n", __func__, cis->cis_data->cis_rev);

	cis->rev_flag = true;
#endif

	return ret;
}

static int sensor_hi847_cis_group_param_hold_func(struct v4l2_subdev *subdev, unsigned int hold)
{
#if USE_GROUP_PARAM_HOLD
	int ret = 0;
	u16 hold_set = 0;
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

	ret = is_sensor_read16(client, SENSOR_HI847_GROUP_PARAM_HOLD_ADDR, & hold_set);
	if (ret < 0)
		goto p_err;

	// set hold setting, bit[8]: Grouped parameter hold
	if (hold == true) {
		hold_set |= (0x1) << 8;
	} else {
		hold_set &= ~((0x1) << 8);
	}
	dbg_sensor(1, "[%s] GPH: %s \n", __func__, (hold_set & (0x1 << 8)) == (0x1 << 8) ? "ON": "OFF");

	ret = is_sensor_write8(client, SENSOR_HI847_GROUP_PARAM_HOLD_ADDR, hold);
	if (ret < 0)
		goto p_err;

	cis->cis_data->group_param_hold = hold;
	ret = 1;

p_err:
	return ret;
#else
	return 0;
#endif
}

static void sensor_hi847_cis_set_paf_stat_enable(u32 mode, cis_shared_data *cis_data)
{
	WARN_ON(!cis_data);

#if SENSOR_HI847_PDAF_DISABLE
	cis_data->is_data.paf_stat_enable = false;
#else
	switch (mode) {
	case SENSOR_HI847_3264X2448_30FPS:
	case SENSOR_HI847_3264X1836_30FPS:
		cis_data->is_data.paf_stat_enable = true;
		break;
	default:
		cis_data->is_data.paf_stat_enable = false;
		break;
	}
#endif
}

/*************************************************
 *  [HI847 Analog gain formular]
 *
 *  Analog Gain = (Reg value)/16 + 1
 *
 *  Analog Gain Range = x1.0 to x16.0
 *
 *************************************************/

u32 sensor_hi847_cis_calc_again_code(u32 permile)
{
	return ((permile - 1000) * 16 / 1000);
}

u32 sensor_hi847_cis_calc_again_permile(u32 code)
{
	code &= 0xF;
	return ((code * 1000 / 16) + 1000);
}

/*************************************************
 *  [HI847 Digital gain formular]
 *
 *  Digital Gain = bit[12:9] + bit[8:0]/512 (Gr, Gb, R, B)
 *
 *  Digital Gain Range = x1.0 to x15.99
 *
 *************************************************/

u32 sensor_hi847_cis_calc_dgain_code(u32 permile)
{
	u32 buf[2] = {0, 0};
	buf[0] = ((permile / 1000) & 0x0F) << 9;
	buf[1] = ((((permile % 1000) * 512) / 1000) & 0x1FF);

	return (buf[0] | buf[1]);
}

u32 sensor_hi847_cis_calc_dgain_permile(u32 code)
{
	return (((code & 0x1E00) >> 9) * 1000) + ((code & 0x01FF) * 1000 / 512);
}

/* CIS OPS */
int sensor_hi847_cis_init(struct v4l2_subdev *subdev)
{
	int ret = 0;
	struct is_cis *cis;
	u32 setfile_index = 0;
	cis_setting_info setinfo;

	struct is_module_enum *module = NULL;
	struct is_device_sensor_peri *sensor_peri = NULL;
	struct is_sensor_cfg *cfg_table;
	u32 cfgs, i;

	setinfo.param = NULL;
	setinfo.return_value = 0;

	FIMC_BUG(!subdev);

	cis = (struct is_cis *)v4l2_get_subdevdata(subdev);
	FIMC_BUG(!cis);
	FIMC_BUG(!cis->cis_data);

	sensor_peri = container_of(cis, struct is_device_sensor_peri, cis);
	FIMC_BUG(!sensor_peri);
	FIMC_BUG(!sensor_peri->module);
	module = sensor_peri->module;

	if (cis->rev_flag == false) {
		memset(cis->cis_data, 0, sizeof(cis_shared_data));

		ret = sensor_hi847_cis_check_rev(cis);
		if (ret < 0) {
			warn("sensor_hi847_check_rev is fail when cis init");
			cis->rev_flag = true;

			return 0;
		}
	}

	if (SENSOR_HI847_BRINGUP_VERSION(cis)) {
		probe_info("[%s] CHIP REV is for Bring-up version !!! Sensor setting is changed for REV00\n", __func__);
		sensor_hi847_global = sensor_hi847_setfile_A_global_rev00;
		sensor_hi847_global_size = ARRAY_SIZE(sensor_hi847_setfile_A_global_rev00);
		sensor_hi847_setfiles = sensor_hi847_setfiles_A_rev00;
		sensor_hi847_setfile_sizes = sensor_hi847_setfile_A_sizes_rev00;
		sensor_hi847_pllinfos = sensor_hi847_pllinfos_A_rev00;
		sensor_hi847_max_setfile_num = ARRAY_SIZE(sensor_hi847_setfiles_A_rev00);

		cfg_table = module->cfg;
		cfgs = module->cfgs;
		for (i = 0; i < cfgs; i++) {
			cfg_table[i].mipi_speed = sensor_hi847_rev00_mipirate[i];
			probe_info("[%s] mode = %d, changed mipirate = %d\n", __func__, i, cfg_table[i].mipi_speed);
		}
	} else {
		probe_info("[%s] CHIP REV is MP version !!!\n", __func__);
	}

	cis->cis_data->product_name = cis->id;
	cis->cis_data->cur_width = SENSOR_HI847_MAX_WIDTH;
	cis->cis_data->cur_height = SENSOR_HI847_MAX_HEIGHT;
	cis->cis_data->low_expo_start = 33000;
	cis->need_mode_change = false;

	cis->cis_data->cur_pattern_mode = SENSOR_TEST_PATTERN_MODE_OFF;

	sensor_hi847_cis_data_calculation(sensor_hi847_pllinfos[setfile_index], cis->cis_data);

#if SENSOR_HI847_DEBUG_INFO
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

#ifdef DEBUG_SENSOR_TIME
	do_gettimeofday(&end);
	dbg_sensor(1, "[%s] time %lu us\n", __func__, (end.tv_sec - st.tv_sec)*1000000 + (end.tv_usec - st.tv_usec));
#endif

	return ret;
}

int sensor_hi847_cis_log_status(struct v4l2_subdev *subdev)
{
	int ret = 0;
	struct is_cis *cis;
	struct i2c_client *client = NULL;
	u16 data16 = 0;
	u8 data8 = 0;

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
	ret = is_sensor_read16(client, SENSOR_HI847_MODEL_ID_ADDR, &data16);
	if (unlikely(!ret)) pr_info("model id(0x%x)\n", data16);
	else goto p_i2c_err;
	ret = is_sensor_read16(client, SENSOR_HI847_STREAM_MODE_ADDR, &data16);
	if (unlikely(!ret)) pr_info("streaming mode(0x%x)\n", data16);
	else goto p_i2c_err;
	ret = is_sensor_read16(client, SENSOR_HI847_ISP_EN_ADDR, &data16);
	if (unlikely(!ret)) pr_info("ISP EN(0x%x)\n", data16);
	else goto p_i2c_err;
	ret = is_sensor_read16(client, SENSOR_HI847_COARSE_INTEG_TIME_ADDR, &data16);
	if (unlikely(!ret)) pr_info("coarse_integration_time(0x%x)\n", data16);
	else goto p_i2c_err;
	ret = is_sensor_read8(client, SENSOR_HI847_ANALOG_GAIN_ADDR, &data8);
	if (unlikely(!ret)) pr_info("gain_code_global(0x%x)\n", data8);
	else goto p_i2c_err;
	ret = is_sensor_read16(client, SENSOR_HI847_DIGITAL_GAIN_GR_ADDR, &data16);
	if (unlikely(!ret)) pr_info("gain_code_global(0x%x)\n", data16);
	else goto p_i2c_err;
	ret = is_sensor_read16(client, SENSOR_HI847_DIGITAL_GAIN_GB_ADDR, &data16);
	if (unlikely(!ret)) pr_info("gain_code_global(0x%x)\n", data16);
	else goto p_i2c_err;
	ret = is_sensor_read16(client, SENSOR_HI847_DIGITAL_GAIN_R_ADDR, &data16);
	if (unlikely(!ret)) pr_info("gain_code_global(0x%x)\n", data16);
	else goto p_i2c_err;
	ret = is_sensor_read16(client, SENSOR_HI847_DIGITAL_GAIN_B_ADDR, &data16);
	if (unlikely(!ret)) pr_info("gain_code_global(0x%x)\n", data16);
	else goto p_i2c_err;
	ret = is_sensor_read16(client, SENSOR_HI847_FRAME_LENGTH_LINE_ADDR, &data16);
	if (unlikely(!ret)) pr_info("frame_length_line(0x%x)\n", data16);
	else goto p_i2c_err;
	ret = is_sensor_read16(client, SENSOR_HI847_LINE_LENGTH_PCK_ADDR, &data16);
	if (unlikely(!ret)) pr_info("line_length_pck(0x%x)\n", data16);
	else goto p_i2c_err;
	ret= is_sensor_read8(client, 0x1004, &data8);
	if (unlikely(!ret)) pr_info("mipi img data id ctrl (0x%x)\n", data8);
	else goto p_i2c_err;
	ret= is_sensor_read8(client, 0x1005, &data8);
	if (unlikely(!ret)) pr_info("mipi pd data id ctrl (0x%x)\n", data8);
	else goto p_i2c_err;
	ret= is_sensor_read8(client, 0x1038, &data8);
	if (unlikely(!ret)) pr_info("mipi virtual channel ctrl (0x%x)\n", data8);
	else goto p_i2c_err;
	ret= is_sensor_read8(client, 0x1042, &data8);
	if (unlikely(!ret)) pr_info("mipi pd seperation ctrl (0x%x)\n", data8);
	else goto p_i2c_err;

	pr_info("[%s] *******************************\n", __func__);

p_i2c_err:
	I2C_MUTEX_UNLOCK(cis->i2c_lock);

p_err:
	return ret;
}

int sensor_hi847_cis_group_param_hold(struct v4l2_subdev *subdev, bool hold)
{
	int ret = 0;
	struct is_cis *cis = NULL;

	FIMC_BUG(!subdev);

	cis = (struct is_cis *)v4l2_get_subdevdata(subdev);

	FIMC_BUG(!cis);
	FIMC_BUG(!cis->cis_data);

	ret = sensor_hi847_cis_group_param_hold_func(subdev, hold);
	if (ret < 0)
		goto p_err;

p_err:
	return ret;
}

int sensor_hi847_cis_set_global_setting(struct v4l2_subdev *subdev)
{
	int ret = 0;
	struct is_cis *cis = NULL;

	FIMC_BUG(!subdev);

	cis = (struct is_cis *)v4l2_get_subdevdata(subdev);
	FIMC_BUG(!cis);

	I2C_MUTEX_LOCK(cis->i2c_lock);

	info("[%s] global setting start\n", __func__);
	ret = sensor_cis_set_registers(subdev, sensor_hi847_global, sensor_hi847_global_size);
	if (ret < 0) {
		err("sensor_hi847_set_registers fail!!");
		goto p_err;
	}
	info("[%s] global setting done\n", __func__);

p_err:
	I2C_MUTEX_UNLOCK(cis->i2c_lock);

	return ret;
}

int sensor_hi847_cis_mode_change(struct v4l2_subdev *subdev, u32 mode)
{
	int ret = 0;
	struct is_cis *cis = NULL;
	struct is_device_sensor *device;

	FIMC_BUG(!subdev);

	cis = (struct is_cis *)v4l2_get_subdevdata(subdev);
	FIMC_BUG(!cis);
	FIMC_BUG(!cis->cis_data);

	device = (struct is_device_sensor *)v4l2_get_subdev_hostdata(subdev);
	if (unlikely(!device)) {
		err("device sensor is null");
		return -EINVAL;
	}

	if (mode >= sensor_hi847_max_setfile_num) {
		err("invalid mode(%d)!!", mode);
		ret = -EINVAL;
		goto p_err;
	}

	sensor_hi847_cis_set_paf_stat_enable(mode, cis->cis_data);

	I2C_MUTEX_LOCK(cis->i2c_lock);

	info("[%s] sensor mode(%d)\n", __func__, mode);
	ret = sensor_cis_set_registers(subdev, sensor_hi847_setfiles[mode], sensor_hi847_setfile_sizes[mode]);
	if (ret < 0) {
		err("sensor_hi847_set_registers fail!!");
		goto p_i2c_err;
	}

p_i2c_err:
	I2C_MUTEX_UNLOCK(cis->i2c_lock);

p_err:
	return ret;
}

/* TODO: Sensor set size sequence(sensor done, sensor stop, 3AA done in FW case */
int sensor_hi847_cis_set_size(struct v4l2_subdev *subdev, cis_shared_data *cis_data)
{
	return 0;
};

int sensor_hi847_cis_stream_on(struct v4l2_subdev *subdev)
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
	FIMC_BUG(!client);

	cis_data = cis->cis_data;

	dbg_sensor(1, "[MOD:D:%d] %s\n", cis->id, __func__);

	I2C_MUTEX_LOCK(cis->i2c_lock);
	ret = sensor_hi847_cis_group_param_hold_func(subdev, 0x00);
	if (ret < 0)
		err("group_param_hold_func failed at stream on");

#ifdef SENSOR_HI847_DEBUG_INFO
	{
	u16 pll;
	is_sensor_read16(client, 0x0702, &pll);
	dbg_sensor(1, "______ pll_cfg(%x)\n", pll);
	is_sensor_read16(client, 0x0732, &pll);
	dbg_sensor(1, "______ pll_clkgen_en_ramp(%x)\n", pll);
	is_sensor_read16(client, 0x0736, &pll);
	dbg_sensor(1, "______ pll_mdiv_ramp(%x)\n", pll);
	is_sensor_read16(client, 0x0738, &pll);
	dbg_sensor(1, "______ pll_prediv_ramp(%x)\n", pll);
	is_sensor_read16(client, 0x073C, &pll);
	dbg_sensor(1, "______ pll_tg_vt_sys_div1(%x)\n", (pll & 0x0F00));
	dbg_sensor(1, "______ pll_tg_vt_sys_div2(%x)\n", (pll & 0x0001));
	is_sensor_read16(client, 0x0742, &pll);
	dbg_sensor(1, "______ pll_clkgen_en_mipi(%x)\n", pll);
	is_sensor_read16(client, 0x0746, &pll);
	dbg_sensor(1, "______ pll_mdiv_mipi(%x)\n", pll);
	is_sensor_read16(client, 0x0748, &pll);
	dbg_sensor(1, "______ pll_prediv_mipi(%x)\n", pll);
	is_sensor_read16(client, 0x074A, &pll);
	dbg_sensor(1, "______ pll_mipi_vt_sys_div1(%x)\n", (pll & 0x0F00));
	dbg_sensor(1, "______ pll_mipi_vt_sys_div2(%x)\n", (pll & 0x0011));
	is_sensor_read16(client, 0x074C, &pll);
	dbg_sensor(1, "______ pll_mipi_div1(%x)\n", pll);
	is_sensor_read16(client, 0x074E, &pll);
	dbg_sensor(1, "______ pll_mipi_byte_div(%x)\n", pll);
	is_sensor_read16(client, 0x020E, &pll);
	dbg_sensor(1, "______ frame_length_line(%x)\n", pll);
	is_sensor_read16(client, 0x0206, &pll);
	dbg_sensor(1, "______ line_length_pck(%x)\n", pll);
	}
#endif

#if SENSOR_HI847_PDAF_DISABLE
	is_sensor_write16(client, 0x0B04, 0x00FC);
	is_sensor_write16(client, 0x1004, 0x2BB0);
	is_sensor_write16(client, 0x1038, 0x0000);
	is_sensor_write16(client, 0x1042, 0x0008);
#endif

	/* Sensor stream on */
	info("%s\n", __func__);
	ret = is_sensor_write16(client, SENSOR_HI847_STREAM_MODE_ADDR, 0x0100);
	if (ret < 0) {
		err("i2c transfer fail addr(%x) ret = %d\n",
				SENSOR_HI847_STREAM_MODE_ADDR, ret);
		goto p_i2c_err;
	}

	cis_data->stream_on = true;

#ifdef DEBUG_SENSOR_TIME
	do_gettimeofday(&end);
	dbg_sensor(1, "[%s] time %lu us\n", __func__, (end.tv_sec - st.tv_sec)*1000000 + (end.tv_usec - st.tv_usec));
#endif

p_i2c_err:
	I2C_MUTEX_UNLOCK(cis->i2c_lock);

	return ret;
}

int sensor_hi847_cis_set_test_pattern(struct v4l2_subdev *subdev, camera2_sensor_ctl_t *sensor_ctl)
{
	int ret = 0;
	struct is_cis *cis;
	struct i2c_client *client;

	cis = (struct is_cis *)v4l2_get_subdevdata(subdev);

	WARN_ON(!cis);
	WARN_ON(!cis->cis_data);

	client = cis->client;
	if (unlikely(!client)) {
		err("client is NULL");
		ret = -EINVAL;
		goto p_err;
	}

	dbg_sensor(1, "[MOD:D:%d] %s, cur_pattern_mode(%d), testPatternMode(%d)\n", cis->id, __func__,
			cis->cis_data->cur_pattern_mode, sensor_ctl->testPatternMode);

	if (cis->cis_data->cur_pattern_mode != sensor_ctl->testPatternMode) {
		info("%s REG : 0x0B04 write to 0x00dd", __func__);
		is_sensor_write16(client, 0x0B04, 0x00dd);

		cis->cis_data->cur_pattern_mode = sensor_ctl->testPatternMode;
		if (sensor_ctl->testPatternMode == SENSOR_TEST_PATTERN_MODE_OFF) {
			info("%s: set DEFAULT pattern! (testpatternmode : %d)\n", __func__, sensor_ctl->testPatternMode);

			I2C_MUTEX_LOCK(cis->i2c_lock);
			is_sensor_write16(client, 0x0C0A, 0x0000);
			I2C_MUTEX_UNLOCK(cis->i2c_lock);
		} else if (sensor_ctl->testPatternMode == SENSOR_TEST_PATTERN_MODE_BLACK) {
			info("%s: set BLACK pattern! (testpatternmode :%d), Data : 0x(%x, %x, %x, %x)\n",
				__func__, sensor_ctl->testPatternMode,
				(unsigned short)sensor_ctl->testPatternData[0],
				(unsigned short)sensor_ctl->testPatternData[1],
				(unsigned short)sensor_ctl->testPatternData[2],
				(unsigned short)sensor_ctl->testPatternData[3]);

			I2C_MUTEX_LOCK(cis->i2c_lock);
			is_sensor_write16(client, 0x0C0A, 0x0100);

			I2C_MUTEX_UNLOCK(cis->i2c_lock);
		}
	}

p_err:
	return ret;
}

int sensor_hi847_cis_stream_off(struct v4l2_subdev *subdev)
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
	FIMC_BUG(!client);

	cis_data = cis->cis_data;

	dbg_sensor(1, "[MOD:D:%d] %s\n", cis->id, __func__);

	I2C_MUTEX_LOCK(cis->i2c_lock);

	ret = sensor_hi847_cis_group_param_hold_func(subdev, 0x00);
	if (ret < 0)
		err("group_param_hold_func failed at stream off");

	info("%s\n", __func__);
	ret = is_sensor_write16(client, SENSOR_HI847_STREAM_MODE_ADDR, 0x0000);
	if (ret < 0) {
		err("i2c transfer fail addr(%x) ret = %d\n",
				SENSOR_HI847_STREAM_MODE_ADDR, ret);
		goto p_i2c_err;
	}

	cis_data->stream_on = false;

	info("[%s] Done.\n", __func__);

#ifdef DEBUG_SENSOR_TIME
	do_gettimeofday(&end);
	dbg_sensor(1, "[%s] time %lu us\n", __func__, (end.tv_sec - st.tv_sec)*1000000 + (end.tv_usec - st.tv_usec));
#endif

p_i2c_err:
	I2C_MUTEX_UNLOCK(cis->i2c_lock);

	return ret;
}

int sensor_hi847_cis_adjust_frame_duration(struct v4l2_subdev *subdev,
						u32 input_exposure_time,
						u32 *target_duration)
{
	int ret = 0;
	struct is_cis *cis   = NULL;
	cis_shared_data *cis_data = NULL;

	u64 vt_pix_clk_khz = 0;
	u32 line_length_pck = 0;
	u32 coarse_integ_time = 0;
	u32 frame_length_lines = 0;
	u32 frame_duration = 0;

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

	if (input_exposure_time == 0) {
		input_exposure_time  = cis_data->min_frame_us_time;
		info("[%s] Not proper exposure time(0), so apply min frame duration to exposure time forcely!!!(%d)\n",
			__func__, cis_data->min_frame_us_time);
	}

	vt_pix_clk_khz = cis_data->pclk / 1000;
	line_length_pck = cis_data->line_length_pck;
	coarse_integ_time = (u32)((vt_pix_clk_khz * input_exposure_time) /(line_length_pck * 1000));
	frame_length_lines = coarse_integ_time + cis_data->max_margin_coarse_integration_time;

	frame_duration = (u32)(((u64)frame_length_lines * line_length_pck) * 1000 / vt_pix_clk_khz);

	dbg_sensor(1, "[%s](vsync cnt = %d) input exp(%d), adj frame duraion(%d), min_frame_us(%d)\n",
			__func__, cis_data->sen_vsync_count, input_exposure_time, frame_duration, cis_data->min_frame_us_time);
	dbg_sensor(1, "[%s] requested min_fps(%d), max_fps(%d) from HAL\n", __func__, cis->min_fps, cis->max_fps);

	*target_duration = MAX(frame_duration, cis_data->min_frame_us_time);

	dbg_sensor(1, "[%s] calcurated frame_duration(%d), adjusted frame_duration(%d)\n", __func__, frame_duration, *target_duration);

#ifdef DEBUG_SENSOR_TIME
	do_gettimeofday(&end);
	dbg_sensor(1, "[%s] time %lu us\n", __func__, (end.tv_sec - st.tv_sec)*1000000 + (end.tv_usec - st.tv_usec));
#endif

	return ret;
}

int sensor_hi847_cis_set_frame_duration(struct v4l2_subdev *subdev, u32 frame_duration)
{
	int ret = 0;
	int hold = 0;
	struct is_cis *cis   = NULL;
	struct i2c_client *client = NULL;
	cis_shared_data *cis_data = NULL;

	u64 vt_pix_clk_khz = 0;
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
	FIMC_BUG(!client);

	cis_data = cis->cis_data;

	if (frame_duration < cis_data->min_frame_us_time) {
		dbg_sensor(1, "frame duration(%d) is less than min(%d)\n", frame_duration, cis_data->min_frame_us_time);
		frame_duration = cis_data->min_frame_us_time;
	}

	vt_pix_clk_khz = cis_data->pclk / 1000;
	line_length_pck = cis_data->line_length_pck;

	frame_length_lines = (u16)((vt_pix_clk_khz * frame_duration) / (line_length_pck * 1000));

	dbg_sensor(1, "[MOD:D:%d] %s, vt_pix_clk_khz(%#x) frame_duration = %d us,"
			KERN_CONT "(line_length_pck%#x), frame_length_lines(%#x)\n",
			cis->id, __func__, vt_pix_clk_khz, frame_duration,
			line_length_pck, frame_length_lines);

	I2C_MUTEX_LOCK(cis->i2c_lock);

	hold = sensor_hi847_cis_group_param_hold(subdev, true);
	if (hold < 0) {
		ret = hold;
		goto p_i2c_err;
	}

	ret = is_sensor_write16(client, SENSOR_HI847_FRAME_LENGTH_LINE_ADDR, frame_length_lines);
	if (ret < 0) {
		goto p_i2c_err;
	}

	cis_data->cur_frame_us_time = frame_duration;
	cis_data->frame_length_lines = frame_length_lines;
	cis_data->max_coarse_integration_time = cis_data->frame_length_lines - cis_data->max_margin_coarse_integration_time;

#ifdef DEBUG_SENSOR_TIME
	do_gettimeofday(&end);
	dbg_sensor(1, "[%s] time %lu us\n", __func__, (end.tv_sec - st.tv_sec)*1000000 + (end.tv_usec - st.tv_usec));
#endif

p_i2c_err:
	if (hold > 0) {
		hold = sensor_hi847_cis_group_param_hold(subdev, false);
		if (hold < 0)
			ret = hold;
	}

	I2C_MUTEX_UNLOCK(cis->i2c_lock);

	return ret;
}

int sensor_hi847_cis_set_frame_rate(struct v4l2_subdev *subdev, u32 min_fps)
{
	int ret = 0;
	struct is_cis *cis   = NULL;
	cis_shared_data *cis_data = NULL;

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

	ret = sensor_hi847_cis_set_frame_duration(subdev, frame_duration);
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

int sensor_hi847_cis_get_min_exposure_time(struct v4l2_subdev *subdev, u32 *min_expo)
{
	int ret = 0;
	struct is_cis *cis = NULL;
	cis_shared_data *cis_data = NULL;

	u32 min_integration_time = 0;
	u32 min_coarse = 0;
	u32 min_fine = 0;
	u64 vt_pix_clk_khz = 0;
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

	vt_pix_clk_khz = cis_data->pclk / 1000;
	if (vt_pix_clk_khz == 0) {
		pr_err("[MOD:D:%d] %s, Invalid vt_pix_clk_khz(%d)\n", cis->id, __func__, vt_pix_clk_khz);
		goto p_err;
	}

	line_length_pck = cis_data->line_length_pck;
	min_coarse = cis_data->min_coarse_integration_time;
	min_fine = cis_data->min_fine_integration_time;

	min_integration_time = (u32)((u64)((line_length_pck * min_coarse) + min_fine) * 1000 / vt_pix_clk_khz);
	*min_expo = min_integration_time;

	dbg_sensor(1, "[%s] min integration time %d\n", __func__, min_integration_time);

#ifdef DEBUG_SENSOR_TIME
	do_gettimeofday(&end);
	dbg_sensor(1, "[%s] time %lu us\n", __func__, (end.tv_sec - st.tv_sec)*1000000 + (end.tv_usec - st.tv_usec));
#endif

p_err:
	return ret;
}

int sensor_hi847_cis_get_max_exposure_time(struct v4l2_subdev *subdev, u32 *max_expo)
{
	int ret = 0;
	struct is_cis *cis   = NULL;
	cis_shared_data *cis_data = NULL;
	u32 max_integration_time = 0;
	u32 max_coarse_margin = 0;
	u32 max_coarse = 0;
	u32 max_fine = 0;
	u32 vt_pix_clk_khz = 0;
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

	vt_pix_clk_khz = cis_data->pclk / 1000;
	if (vt_pix_clk_khz == 0) {
		pr_err("[MOD:D:%d] %s, Invalid vt_pix_clk_khz(%d)\n", cis->id, __func__, vt_pix_clk_khz);
		goto p_err;
	}
	line_length_pck = cis_data->line_length_pck;
	frame_length_lines = cis_data->frame_length_lines;
	max_coarse_margin = cis_data->max_margin_coarse_integration_time;
	max_coarse = frame_length_lines - max_coarse_margin;
	max_fine = cis_data->max_fine_integration_time;

	max_integration_time = (u32)((((u64)line_length_pck * max_coarse) + max_fine) * 1000 / vt_pix_clk_khz);

	*max_expo = max_integration_time;

	/* TODO: Is this values update hear? */
	cis_data->max_coarse_integration_time = max_coarse;

	dbg_sensor(1, "[%s] max integration time %d, max coarse integration %d\n",
			__func__, max_integration_time, cis_data->max_coarse_integration_time);

#ifdef DEBUG_SENSOR_TIME
	do_gettimeofday(&end);
	dbg_sensor(1, "[%s] time %lu us\n", __func__, (end.tv_sec - st.tv_sec)*1000000 + (end.tv_usec - st.tv_usec));
#endif

p_err:
	return ret;
}

int sensor_hi847_cis_set_exposure_time(struct v4l2_subdev *subdev, struct ae_param *target_exposure)
{
	int ret = 0;
	int hold = 0;
	struct is_cis *cis   = NULL;
	struct i2c_client *client = NULL;
	cis_shared_data *cis_data = NULL;

	u64 vt_pix_clk_khz = 0;
	u16 coarse_int = 0;
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
	FIMC_BUG(!client);

	if ((target_exposure->long_val <= 0) || (target_exposure->short_val <= 0)) {
		err("[%s] invalid target exposure(%d, %d)\n", __func__,
				target_exposure->long_val, target_exposure->short_val);
		ret = -EINVAL;
		goto p_err;
	}

	cis_data = cis->cis_data;

	dbg_sensor(1, "[MOD:D:%d] %s, vsync_cnt(%d), target long(%d), short(%d)\n", cis->id, __func__,
			cis_data->sen_vsync_count, target_exposure->long_val, target_exposure->short_val);

	vt_pix_clk_khz = cis_data->pclk / 1000;
	line_length_pck = cis_data->line_length_pck;
	min_fine_int = cis_data->min_fine_integration_time;

	coarse_int = ((target_exposure->val * vt_pix_clk_khz) - min_fine_int) / line_length_pck /1000;

	if (coarse_int > cis_data->max_coarse_integration_time) {
		dbg_sensor(1, "[MOD:D:%d] %s, vsync_cnt(%d), input coarse_int(%d) max coarse_int(%d)\n", cis->id, __func__,
			cis_data->sen_vsync_count, coarse_int, cis_data->max_coarse_integration_time);
		coarse_int = cis_data->max_coarse_integration_time;
	}

	if (coarse_int < cis_data->min_coarse_integration_time) {
		dbg_sensor(1, "[MOD:D:%d] %s, vsync_cnt(%d), input coarse_int(%d) min coarse_int(%d)\n", cis->id, __func__,
			cis_data->sen_vsync_count, coarse_int, cis_data->min_coarse_integration_time);
		coarse_int = cis_data->min_coarse_integration_time;
	}

	cis_data->cur_exposure_coarse = coarse_int;
	cis_data->cur_long_exposure_coarse = coarse_int;
	cis_data->cur_short_exposure_coarse = coarse_int;

	I2C_MUTEX_LOCK(cis->i2c_lock);

	hold = sensor_hi847_cis_group_param_hold(subdev, true);
	if (hold < 0) {
		ret = hold;
		goto p_i2c_err;
	}

	ret = is_sensor_write16(client, SENSOR_HI847_COARSE_INTEG_TIME_ADDR, coarse_int);
	if (ret < 0)
		goto p_i2c_err;

	dbg_sensor(1, "[MOD:D:%d] %s, vt_pix_clk_khz (%llu), LLP(%d), FLL(%d), CIT(%d)\n",
		cis->id, __func__, vt_pix_clk_khz, line_length_pck, cis_data->frame_length_lines, coarse_int);

#ifdef DEBUG_SENSOR_TIME
	do_gettimeofday(&end);
	dbg_sensor(1, "[%s] time %lu us\n", __func__, (end.tv_sec - st.tv_sec)*1000000 + (end.tv_usec - st.tv_usec));
#endif

p_i2c_err:
	if (hold > 0) {
		hold = sensor_hi847_cis_group_param_hold(subdev, false);
		if (hold < 0)
			ret = hold;
	}

	I2C_MUTEX_UNLOCK(cis->i2c_lock);

p_err:
	return ret;
}

int sensor_hi847_cis_get_min_analog_gain(struct v4l2_subdev *subdev, u32 *min_again)
{
	int ret = 0;
	struct is_cis *cis   = NULL;
	cis_shared_data *cis_data = NULL;
	u16 min_again_code = SENSOR_HI847_MIN_ANALOG_GAIN_SET_VALUE;

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
	cis_data->min_analog_gain[1] = sensor_hi847_cis_calc_again_permile(min_again_code);
	*min_again = cis_data->min_analog_gain[1];

	dbg_sensor(1, "[%s] min_again_code(0x%X), main_again_permile(%d)\n", __func__,
		cis_data->min_analog_gain[0], cis_data->min_analog_gain[1]);

#ifdef DEBUG_SENSOR_TIME
	do_gettimeofday(&end);
	dbg_sensor(1, "[%s] time %lu us\n", __func__, (end.tv_sec - st.tv_sec)*1000000 + (end.tv_usec - st.tv_usec));
#endif

	return ret;
}

int sensor_hi847_cis_get_max_analog_gain(struct v4l2_subdev *subdev, u32 *max_again)
{
	int ret = 0;
	struct is_cis *cis   = NULL;
	cis_shared_data *cis_data = NULL;
	u16 max_again_code = SENSOR_HI847_MAX_ANALOG_GAIN_SET_VALUE;

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
	cis_data->max_analog_gain[1] = sensor_hi847_cis_calc_again_permile(max_again_code);
	*max_again = cis_data->max_analog_gain[1];

	dbg_sensor(1, "[%s] max_again_code(0x%X), max_again_permile(%d)\n", __func__,
		cis_data->max_analog_gain[0], cis_data->max_analog_gain[1]);

#ifdef DEBUG_SENSOR_TIME
	do_gettimeofday(&end);
	dbg_sensor(1, "[%s] time %lu us\n", __func__, (end.tv_sec - st.tv_sec)*1000000 + (end.tv_usec - st.tv_usec));
#endif

	return ret;
}

int sensor_hi847_cis_adjust_analog_gain(struct v4l2_subdev *subdev, u32 input_again, u32 *target_permile)
{
	int ret = 0;
	struct is_cis *cis   = NULL;
	cis_shared_data *cis_data = NULL;

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

	again_code = sensor_hi847_cis_calc_again_code(input_again);

	if (again_code > cis_data->max_analog_gain[0]) {
		again_code = cis_data->max_analog_gain[0];
	} else if (again_code < cis_data->min_analog_gain[0]) {
		again_code = cis_data->min_analog_gain[0];
	}

	again_permile = sensor_hi847_cis_calc_again_permile(again_code);

	dbg_sensor(1, "[%s] max again(%d), min again(%d), input_again(%d), code(%d), permile(%d)\n", __func__,
			cis_data->max_analog_gain[0], cis_data->min_analog_gain[0],
			input_again, again_code, again_permile);

	*target_permile = again_permile;

	return ret;
}

int sensor_hi847_cis_set_analog_gain(struct v4l2_subdev *subdev, struct ae_param *again)
{
	int ret = 0;
	int hold = 0;
	struct is_cis *cis   = NULL;
	struct i2c_client *client = NULL;

	u8 analog_gain = 0;

#ifdef DEBUG_SENSOR_TIME
	struct timeval st, end;
	do_gettimeofday(&st);
#endif

	FIMC_BUG(!subdev);
	FIMC_BUG(!again);

	cis = (struct is_cis *)v4l2_get_subdevdata(subdev);

	FIMC_BUG(!cis);
	FIMC_BUG(!cis->cis_data);

	client = cis->client;
	FIMC_BUG(!client);

	analog_gain = (u8)sensor_hi847_cis_calc_again_code(again->val);

	if (analog_gain < cis->cis_data->min_analog_gain[0]) {
		info("[%s] not proper analog_gain value, reset to min_analog_gain\n", __func__);
		analog_gain = cis->cis_data->min_analog_gain[0];
	}

	if (analog_gain > cis->cis_data->max_analog_gain[0]) {
		info("[%s] not proper analog_gain value, reset to max_analog_gain\n", __func__);
		analog_gain = cis->cis_data->max_analog_gain[0];
	}

	dbg_sensor(1, "[MOD:D:%d] %s(vsync cnt = %d), input_again permile(%d us), analog_gain code(%#x)\n",
		cis->id, __func__, cis->cis_data->sen_vsync_count, again->val, analog_gain);

	I2C_MUTEX_LOCK(cis->i2c_lock);

	hold = sensor_hi847_cis_group_param_hold(subdev, true);
	if (hold < 0) {
		ret = hold;
		goto p_i2c_err;
	}

	analog_gain &= 0xFF;
	ret = is_sensor_write8(client, SENSOR_HI847_ANALOG_GAIN_ADDR, analog_gain);
	if (ret < 0)
		goto p_i2c_err;

#ifdef DEBUG_SENSOR_TIME
	do_gettimeofday(&end);
	dbg_sensor(1, "[%s] time %lu us\n", __func__, (end.tv_sec - st.tv_sec)*1000000 + (end.tv_usec - st.tv_usec));
#endif

p_i2c_err:
	if (hold > 0) {
		hold = sensor_hi847_cis_group_param_hold(subdev, false);
		if (hold < 0)
			ret = hold;
	}

	I2C_MUTEX_UNLOCK(cis->i2c_lock);

	return ret;
}

int sensor_hi847_cis_get_analog_gain(struct v4l2_subdev *subdev, u32 *again)
{
	int ret = 0;
	int hold = 0;
	struct is_cis *cis   = NULL;
	struct i2c_client *client = NULL;

	u8 analog_gain = 0;

#ifdef DEBUG_SENSOR_TIME
	struct timeval st, end;
	do_gettimeofday(&st);
#endif

	FIMC_BUG(!subdev);
	FIMC_BUG(!again);

	cis = (struct is_cis *)v4l2_get_subdevdata(subdev);

	FIMC_BUG(!cis);

	client = cis->client;
	FIMC_BUG(!client);

	I2C_MUTEX_LOCK(cis->i2c_lock);

	hold = sensor_hi847_cis_group_param_hold(subdev, true);
	if (hold < 0) {
		ret = hold;
		goto p_i2c_err;
	}

	ret = is_sensor_read8(client, SENSOR_HI847_ANALOG_GAIN_ADDR, &analog_gain);
	if (ret < 0)
		goto p_i2c_err;

	*again = sensor_hi847_cis_calc_again_permile((u32)analog_gain);

	dbg_sensor(1, "[MOD:D:%d] %s, cur_again permile(%d us), analog_gain code(0x%X)\n",
		cis->id, __func__, *again, analog_gain);

#ifdef DEBUG_SENSOR_TIME
	do_gettimeofday(&end);
	dbg_sensor(1, "[%s] time %lu us\n", __func__, (end.tv_sec - st.tv_sec)*1000000 + (end.tv_usec - st.tv_usec));
#endif

p_i2c_err:
	if (hold > 0) {
		hold = sensor_hi847_cis_group_param_hold(subdev, false);
		if (hold < 0)
			ret = hold;
	}

	I2C_MUTEX_UNLOCK(cis->i2c_lock);

	return ret;
}

int sensor_hi847_cis_get_min_digital_gain(struct v4l2_subdev *subdev, u32 *min_dgain)
{
	int ret = 0;
	struct is_cis *cis   = NULL;
	cis_shared_data *cis_data = NULL;
	u16 min_dgain_code = SENSOR_HI847_MIN_DIGITAL_GAIN_SET_VALUE;

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
	cis_data->min_digital_gain[1] = sensor_hi847_cis_calc_dgain_permile(min_dgain_code);

	*min_dgain = cis_data->min_digital_gain[1];

	dbg_sensor(1, "[%s] min_dgain_code(0x%X), min_dgain_permile(%d)\n", __func__,
		cis_data->min_digital_gain[0], cis_data->min_digital_gain[1]);

#ifdef DEBUG_SENSOR_TIME
	do_gettimeofday(&end);
	dbg_sensor(1, "[%s] time %lu us\n", __func__, (end.tv_sec - st.tv_sec)*1000000 + (end.tv_usec - st.tv_usec));
#endif

	return ret;
}

int sensor_hi847_cis_get_max_digital_gain(struct v4l2_subdev *subdev, u32 *max_dgain)
{
	int ret = 0;
	struct is_cis *cis   = NULL;
	cis_shared_data *cis_data = NULL;
	u16 max_dgain_code = SENSOR_HI847_MAX_DIGITAL_GAIN_SET_VALUE;

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
	cis_data->max_digital_gain[1] = sensor_hi847_cis_calc_dgain_permile(max_dgain_code);

	*max_dgain = cis_data->max_digital_gain[1];

	dbg_sensor(1, "[%s] max_dgain_code %d, max_dgain_permile %d\n", __func__,
		cis_data->max_digital_gain[0], cis_data->max_digital_gain[1]);

#ifdef DEBUG_SENSOR_TIME
	do_gettimeofday(&end);
	dbg_sensor(1, "[%s] time %lu us\n", __func__, (end.tv_sec - st.tv_sec)*1000000 + (end.tv_usec - st.tv_usec));
#endif

	return ret;
}

int sensor_hi847_cis_set_digital_gain(struct v4l2_subdev *subdev, struct ae_param *dgain)
{
	int ret = 0;
	int hold = 0;
	struct is_cis *cis   = NULL;
	struct i2c_client *client = NULL;
	cis_shared_data *cis_data = NULL;

	u16 dgain_code = 0;
	u16 dgains[4] = {0};
	u16 read_val = 0;
	u16 enable_dgain = 0;

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
	FIMC_BUG(!client);

	cis_data = cis->cis_data;

	dgain_code = (u16)sensor_hi847_cis_calc_dgain_code(dgain->val);

	if (dgain_code < cis->cis_data->min_digital_gain[0]) {
		info("[%s] not proper dgain_code value, reset to min_digital_gain\n", __func__);
		dgain_code = cis->cis_data->min_digital_gain[0];
	}
	if (dgain_code > cis->cis_data->max_digital_gain[0]) {
		info("[%s] not proper dgain_code value, reset to max_digital_gain\n", __func__);
		dgain_code = cis->cis_data->max_digital_gain[0];
	}

	dbg_sensor(1, "[MOD:D:%d] %s(vsync cnt = %d), input_dgain permile(%d), dgain_code(0x%X)\n",
			cis->id, __func__, cis->cis_data->sen_vsync_count, dgain->val, dgain_code);

	I2C_MUTEX_LOCK(cis->i2c_lock);

	hold = sensor_hi847_cis_group_param_hold(subdev, true);
	if (hold < 0) {
		ret = hold;
		goto p_i2c_err;
	}

	dgains[0] = dgains[1] = dgains[2] = dgains[3] = dgain_code;
	ret = is_sensor_write16_array(client, SENSOR_HI847_DIGITAL_GAIN_ADDR, dgains, 4);
	if (ret < 0) {
		goto p_i2c_err;
	}

	ret = is_sensor_read16(client, SENSOR_HI847_ISP_EN_ADDR, &read_val);
	if (ret < 0) {
		goto p_i2c_err;
	}

	enable_dgain = read_val | (0x1 << 4); // B[4]: D gain enable
	ret = is_sensor_write16(client, SENSOR_HI847_ISP_EN_ADDR, enable_dgain);
	if (ret < 0) {
		goto p_i2c_err;
	}

#ifdef DEBUG_SENSOR_TIME
	do_gettimeofday(&end);
	dbg_sensor(1, "[%s] time %lu us\n", __func__, (end.tv_sec - st.tv_sec)*1000000 + (end.tv_usec - st.tv_usec));
#endif

p_i2c_err:
	if (hold > 0) {
		hold = sensor_hi847_cis_group_param_hold(subdev, false);
		if (hold < 0)
			ret = hold;
	}

	I2C_MUTEX_UNLOCK(cis->i2c_lock);

	return ret;
}

int sensor_hi847_cis_get_digital_gain(struct v4l2_subdev *subdev, u32 *dgain)
{
	int ret = 0;
	int hold = 0;
	struct is_cis *cis   = NULL;
	struct i2c_client *client = NULL;

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
	FIMC_BUG(!client);

	I2C_MUTEX_LOCK(cis->i2c_lock);

	hold = sensor_hi847_cis_group_param_hold(subdev, true);
	if (hold < 0) {
		ret = hold;
		goto p_i2c_err;
	}

	ret = is_sensor_read16(client, SENSOR_HI847_DIGITAL_GAIN_ADDR, &digital_gain);
	if (ret < 0)
		goto p_i2c_err;

	*dgain = sensor_hi847_cis_calc_dgain_permile(digital_gain);

	dbg_sensor(1, "[MOD:D:%d] %s, dgain_permile = %d, dgain_code(%#x)\n",
			cis->id, __func__, *dgain, digital_gain);

#ifdef DEBUG_SENSOR_TIME
	do_gettimeofday(&end);
	dbg_sensor(1, "[%s] time %lu us\n", __func__, (end.tv_sec - st.tv_sec)*1000000 + (end.tv_usec - st.tv_usec));
#endif

p_i2c_err:
	if (hold > 0) {
		hold = sensor_hi847_cis_group_param_hold(subdev, false);
		if (hold < 0)
			ret = hold;
	}

	I2C_MUTEX_UNLOCK(cis->i2c_lock);

	return ret;
}

int sensor_hi847_cis_wait_streamoff(struct v4l2_subdev *subdev)
{
	int ret = 0;
	struct is_cis *cis   = NULL;
	struct i2c_client *client = NULL;
	u32 wait_cnt = 0;
	u16 PLL_en = 0;

	FIMC_BUG(!subdev);

	cis = (struct is_cis *) v4l2_get_subdevdata(subdev);
	FIMC_BUG(!cis);

	client = cis->client;
	FIMC_BUG(!client);

	do {
		ret = is_sensor_read16(client, SENSOR_HI847_ISP_PLL_ENABLE_ADDR, &PLL_en);
		if (ret < 0) {
			err("i2c transfer fail addr(%x) ret = %d\n",
					SENSOR_HI847_ISP_PLL_ENABLE_ADDR, ret);
			goto p_err;
		}

		dbg_sensor(1, "%s: PLL_en 0x%x \n", __func__, PLL_en);
		/* pll_bypass */
		if (!(PLL_en & 0x0100))
			break;

		wait_cnt++;
		msleep(POLL_TIME_MS);
	} while (wait_cnt < STREAM_MAX_POLL_CNT);

	if (wait_cnt < STREAM_MAX_POLL_CNT) {
		info("%s: finished after %d ms\n", __func__,
				(wait_cnt + 1) * POLL_TIME_MS);
	} else {
		warn("%s: finished : polling timeout occurred after %d ms\n", __func__,
				(wait_cnt + 1) * POLL_TIME_MS);
	}

p_err:
	return ret;
}

int sensor_hi847_cis_wait_streamon(struct v4l2_subdev *subdev)
{
	int ret = 0;
	struct is_cis *cis   = NULL;
	struct i2c_client *client = NULL;
	u32 wait_cnt = 0;
	u16 PLL_en = 0;

	FIMC_BUG(!subdev);

	cis = (struct is_cis *) v4l2_get_subdevdata(subdev);
	FIMC_BUG(!cis);

	client = cis->client;
	FIMC_BUG(!client);

	probe_info("[%s] start\n", __func__);

	do {
		ret = is_sensor_read16(client, SENSOR_HI847_ISP_PLL_ENABLE_ADDR, &PLL_en);
		if (ret < 0) {
			err("i2c transfer fail addr(%x) ret = %d\n", SENSOR_HI847_ISP_PLL_ENABLE_ADDR, ret);
			goto p_err;
		}

		dbg_sensor(1, "%s: PLL_en 0x%x\n", __func__, PLL_en);
		/* pll_enable */
		if (PLL_en & 0x0100)
			break;

		wait_cnt++;
		msleep(POLL_TIME_MS);
	} while (wait_cnt < STREAM_MAX_POLL_CNT);

	if (wait_cnt < STREAM_MAX_POLL_CNT) {
		info("%s: finished after %d ms\n", __func__, (wait_cnt + 1) * POLL_TIME_MS);
	} else {
		warn("%s: finished : polling timeout occurred after %d ms\n", __func__, (wait_cnt + 1) * POLL_TIME_MS);
	}

p_err:
	return ret;
}


void sensor_hi847_cis_data_calc(struct v4l2_subdev *subdev, u32 mode)
{
	struct is_cis *cis = NULL;

	FIMC_BUG_VOID(!subdev);

	cis = (struct is_cis *)v4l2_get_subdevdata(subdev);
	FIMC_BUG_VOID(!cis);
	FIMC_BUG_VOID(!cis->cis_data);

	if (mode >= sensor_hi847_max_setfile_num) {
		err("invalid mode(%d)!!", mode);
		return;
	}

	sensor_hi847_cis_data_calculation(sensor_hi847_pllinfos[mode], cis->cis_data);
}

int sensor_hi847_cis_check_rev_on_init(struct v4l2_subdev *subdev)
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

	ret = sensor_hi847_cis_check_rev(cis);

	return ret;
}

static struct is_cis_ops cis_ops_hi847 = {
	.cis_init = sensor_hi847_cis_init,
	.cis_log_status = sensor_hi847_cis_log_status,
	.cis_group_param_hold = sensor_hi847_cis_group_param_hold,
	.cis_set_global_setting = sensor_hi847_cis_set_global_setting,
	.cis_mode_change = sensor_hi847_cis_mode_change,
	.cis_set_size = sensor_hi847_cis_set_size,
	.cis_stream_on = sensor_hi847_cis_stream_on,
	.cis_stream_off = sensor_hi847_cis_stream_off,
	.cis_adjust_frame_duration = sensor_hi847_cis_adjust_frame_duration,
	.cis_set_frame_duration = sensor_hi847_cis_set_frame_duration,
	.cis_set_frame_rate = sensor_hi847_cis_set_frame_rate,
	.cis_get_min_exposure_time = sensor_hi847_cis_get_min_exposure_time,
	.cis_get_max_exposure_time = sensor_hi847_cis_get_max_exposure_time,
	.cis_set_exposure_time = sensor_hi847_cis_set_exposure_time,
	.cis_get_min_analog_gain = sensor_hi847_cis_get_min_analog_gain,
	.cis_get_max_analog_gain = sensor_hi847_cis_get_max_analog_gain,
	.cis_adjust_analog_gain = sensor_hi847_cis_adjust_analog_gain,
	.cis_set_analog_gain = sensor_hi847_cis_set_analog_gain,
	.cis_get_analog_gain = sensor_hi847_cis_get_analog_gain,
	.cis_get_min_digital_gain = sensor_hi847_cis_get_min_digital_gain,
	.cis_get_max_digital_gain = sensor_hi847_cis_get_max_digital_gain,
	.cis_set_digital_gain = sensor_hi847_cis_set_digital_gain,
	.cis_get_digital_gain = sensor_hi847_cis_get_digital_gain,
	.cis_compensate_gain_for_extremely_br = sensor_cis_compensate_gain_for_extremely_br,
	.cis_wait_streamoff = sensor_hi847_cis_wait_streamoff,
	.cis_wait_streamon = sensor_hi847_cis_wait_streamon,
	.cis_data_calculation = sensor_hi847_cis_data_calc,
	.cis_set_test_pattern = sensor_hi847_cis_set_test_pattern,
	.cis_set_adjust_sync = NULL,
#ifdef USE_CAMERA_MIPI_CLOCK_VARIATION
	.cis_update_mipi_info = NULL,
#endif
#ifdef CAMERA_REAR2_SENSOR_SHIFT_CROP
	.cis_update_pdaf_tail_size = NULL,
#endif
	.cis_check_rev_on_init = sensor_hi847_cis_check_rev_on_init,
	.cis_set_initial_exposure = sensor_cis_set_initial_exposure,
	.cis_recover_stream_on = NULL,
	.cis_set_fake_retention = NULL,
};

static int cis_hi847_probe(struct i2c_client *client,
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

	probe_info("[%s] sensor id %d\n", __func__, sensor_id);

	device = &core->sensor[sensor_id];

	sensor_peri = find_peri_by_cis_id(device, SENSOR_NAME_HI847);
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

	cis->id = SENSOR_NAME_HI847;
	cis->subdev = subdev_cis;
	cis->device = sensor_id;
	cis->client = client;
	sensor_peri->module->client = cis->client;
	cis->i2c_lock = NULL;
	cis->ctrl_delay = N_PLUS_TWO_FRAME;
	/* belows are depend on sensor cis. MUST check sensor spec */
	cis->bayer_order = OTF_INPUT_ORDER_BAYER_GR_BG;

	cis->cis_data = kzalloc(sizeof(cis_shared_data), GFP_KERNEL);
	if (!cis->cis_data) {
		err("cis_data is NULL");
		ret = -ENOMEM;
		goto p_err;
	}

	cis->cis_ops = &cis_ops_hi847;
	cis->rev_flag = false;

	if (of_property_read_bool(dnode, "use_pdaf")) {
		use_pdaf = true;
	}
	probe_info("[%s] use_pdaf %d\n", __func__, use_pdaf);

	if (of_property_read_bool(dnode, "sensor_f_number")) {
		ret = of_property_read_u32(dnode, "sensor_f_number", &cis->aperture_num);
		if (ret) {
			warn("f-number read is fail(%d)",ret);
		}
	} else {
		cis->aperture_num = F2_4;
	}
	probe_info("[%s] f-number %d\n", __func__, cis->aperture_num);

	cis->use_dgain = true;
	cis->hdr_ctrl_by_again = false;
	cis->use_pdaf = use_pdaf;

	if (of_property_read_string(dnode, "setfile", &setfile)) {
		err("setfile index read fail(%d), take default setfile!!", ret);
		setfile = "default";
	}

	if (strcmp(setfile, "default") == 0 || strcmp(setfile, "setA") == 0) {
		probe_info("%s setfile_A\n", __func__);
		sensor_hi847_global = sensor_hi847_setfile_A_global;
		sensor_hi847_global_size = ARRAY_SIZE(sensor_hi847_setfile_A_global);
		sensor_hi847_setfiles = sensor_hi847_setfiles_A;
		sensor_hi847_setfile_sizes = sensor_hi847_setfile_A_sizes;
		sensor_hi847_pllinfos = sensor_hi847_pllinfos_A;
		sensor_hi847_max_setfile_num = ARRAY_SIZE(sensor_hi847_setfiles_A);
		sensor_hi847_otp_initial = sensor_hi847_otp_initial_A;
		sensor_hi847_otp_initial_size = ARRAY_SIZE(sensor_hi847_otp_initial_A);
	}
	else if (strcmp(setfile, "setB") == 0) {
		probe_info("%s setfile_B\n", __func__);
		sensor_hi847_global = sensor_hi847_setfile_B_global;
		sensor_hi847_global_size = ARRAY_SIZE(sensor_hi847_setfile_B_global);
		sensor_hi847_setfiles = sensor_hi847_setfiles_B;
		sensor_hi847_setfile_sizes = sensor_hi847_setfile_B_sizes;
		sensor_hi847_pllinfos = sensor_hi847_pllinfos_B;
		sensor_hi847_max_setfile_num = ARRAY_SIZE(sensor_hi847_setfiles_B);
		sensor_hi847_otp_initial = sensor_hi847_otp_initial_B;
		sensor_hi847_otp_initial_size = ARRAY_SIZE(sensor_hi847_otp_initial_B);
	}
	else {
		err("%s setfile index out of bound, take default (setfile_A)", __func__);
		sensor_hi847_global = sensor_hi847_setfile_A_global;
		sensor_hi847_global_size = ARRAY_SIZE(sensor_hi847_setfile_A_global);
		sensor_hi847_setfiles = sensor_hi847_setfiles_A;
		sensor_hi847_setfile_sizes = sensor_hi847_setfile_A_sizes;
		sensor_hi847_pllinfos = sensor_hi847_pllinfos_A;
		sensor_hi847_max_setfile_num = ARRAY_SIZE(sensor_hi847_setfiles_A);
		sensor_hi847_otp_initial = sensor_hi847_otp_initial_A;
		sensor_hi847_otp_initial_size = ARRAY_SIZE(sensor_hi847_otp_initial_A);
	}

	v4l2_i2c_subdev_init(subdev_cis, client, &subdev_ops);
	v4l2_set_subdevdata(subdev_cis, cis);
	v4l2_set_subdev_hostdata(subdev_cis, device);
	snprintf(subdev_cis->name, V4L2_SUBDEV_NAME_SIZE, "cis-subdev.%d", cis->id);

	probe_info("%s done\n", __func__);

p_err:
	return ret;
}

static const struct of_device_id sensor_cis_hi847_match[] = {
	{
		.compatible = "samsung,exynos-is-cis-hi847",
	},
	{},
};
MODULE_DEVICE_TABLE(of, sensor_cis_hi847_match);

static const struct i2c_device_id sensor_cis_hi847_idt[] = {
	{ SENSOR_NAME, 0 },
	{},
};

static struct i2c_driver sensor_cis_hi847_driver = {
	.probe	= cis_hi847_probe,
	.driver = {
		.name	= SENSOR_NAME,
		.owner	= THIS_MODULE,
		.of_match_table = sensor_cis_hi847_match,
		.suppress_bind_attrs = true,
	},
	.id_table = sensor_cis_hi847_idt
};

#ifdef MODULE
builtin_i2c_driver(sensor_cis_hi847_driver);
#else
static int __init sensor_cis_hi847_init(void)
{
	int ret;

	ret = i2c_add_driver(&sensor_cis_hi847_driver);
	if (ret)
		err("failed to add %s driver: %d\n",
			sensor_cis_hi847_driver.driver.name, ret);

	return ret;
}
late_initcall_sync(sensor_cis_hi847_init);
#endif
MODULE_LICENSE("GPL");
MODULE_SOFTDEP("pre: fimc-is");

