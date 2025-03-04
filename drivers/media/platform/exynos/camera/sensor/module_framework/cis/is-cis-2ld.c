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
#include "is-cis-2ld.h"
#ifdef USE_CAMERA_2LD_4000X3000
#include "is-cis-2ld-4000x3000-setA.h"
#else
#include "is-cis-2ld-setA.h"
#endif
#include "is-helper-i2c.h"

#define SENSOR_NAME "S5K2LD"
/* #define DEBUG_2LD_PLL */
#define NEED_UPDATE_LUT0_LUT1(cis_data) \
	((cis_data)->sen_frame_id == 0x0 \
	|| (cis_data)->sen_frame_id == 0xF \
	|| (cis_data)->sen_frame_id == 0xC \
	|| (cis_data)->stream_on == false)

#define NEED_UPDATE_LUT2_LUT3(cis_data) \
	((cis_data)->sen_frame_id == 0x0 \
	|| (cis_data)->sen_frame_id == 0xD \
	|| (cis_data)->sen_frame_id == 0xE \
	|| (cis_data)->stream_on == false)

static const u32 *sensor_2ld_global;
static u32 sensor_2ld_global_size;
static const u32 **sensor_2ld_setfiles;
static const u32 *sensor_2ld_setfile_sizes;
static const struct sensor_pll_info_compact **sensor_2ld_pllinfos;
static u32 sensor_2ld_max_setfile_num;
#ifdef USE_CAMERA_SENSOR_RETENTION
static const u32 *sensor_2ld_global_retention;
static u32 sensor_2ld_global_retention_size;
static const u32 **sensor_2ld_retention;
static const u32 *sensor_2ld_retention_size;
static u32 sensor_2ld_max_retention_num;
static const u32 **sensor_2ld_load_sram;
static const u32 *sensor_2ld_load_sram_size;
#endif

static int sensor_2ld_ln_mode_delay_count;
static u8 sensor_2ld_ln_mode_frame_count;
static bool sensor_2ld_load_retention;

/* For Recovery */
static u32 sensor_2ld_frame_duration_backup;
static struct ae_param sensor_2ld_again_backup;
static struct ae_param sensor_2ld_dgain_backup;
static struct ae_param sensor_2ld_target_exp_backup;

static bool sensor_2ld_LTE_1s_flag;
static bool sensor_2ld_night_flag;

static bool sensor_2ld_cis_is_wdr_mode_on(cis_shared_data *cis_data)
{
	unsigned int mode = cis_data->sens_config_index_cur;

	if (!is_vender_wdr_mode_on(cis_data))
		return false;

	if (mode >= SENSOR_2LD_MODE_MAX) {
		err("invalid mode(%d)!!", mode);
		return false;
	}

	return sensor_2ld_support_wdr[mode];
}

static bool sensor_2ld_cis_get_aeb_supported(cis_shared_data *cis_data)
{
	unsigned int mode = cis_data->sens_config_index_cur;

	if (mode < 0 || mode >= SENSOR_2LD_MODE_MAX) {
		err("invalid mode(%d)!!", mode);
		return false;
	}

	return sensor_2ld_support_aeb[mode];
}


int sensor_2ld_cis_set_aeb_mode_change(struct v4l2_subdev *subdev)
{
	int ret = 0;
	struct is_cis *cis = NULL;
	unsigned int mode = 0;

	WARN_ON(!subdev);

	cis = (struct is_cis *)v4l2_get_subdevdata(subdev);
	WARN_ON(!cis);
	WARN_ON(!cis->cis_data);

	mode = cis->cis_data->sens_config_index_cur;

	if (!sensor_2ld_cis_get_aeb_supported(cis->cis_data)) {
		pr_info("[%s] not support mode %d evt %x\n", __func__,
			mode, cis->cis_data->cis_rev);
		cis->cis_data->pre_aeb_mode = cis->cis_data->cur_aeb_mode;
		return ret;
	}

	ret = sensor_2ld_cis_group_param_hold_func(subdev, 0x01);
	if (ret < 0)
		err("group_param_hold_func failed");

	switch (cis->cis_data->cur_aeb_mode) {
	case SENSOR_AEB_MODE_ON:
		pr_info("%s : enable AEB\n", __func__);
		ret |= is_sensor_write16(cis->client, 0xFCFC, 0x4000);
		ret |= is_sensor_write8(cis->client, 0x0E0B, 0x04);
		ret |= is_sensor_write16(cis->client, 0x0E0C, 0x0300);
		ret |= is_sensor_write16(cis->client, 0x0E0E, 0x0000); // all register 6*2 offset
		ret |= is_sensor_write16(cis->client,0x0B30, 0x0000); // LN OFF
		break;
	case SENSOR_AEB_MODE_OFF:
		if (!sensor_2ld_LTE_1s_flag) {
			pr_info("%s : disable AEB\n", __func__);
			ret |= is_sensor_write16(cis->client, 0xFCFC, 0x4000);
			ret |= is_sensor_write8(cis->client, 0x0E0B, 0x00);
		}
		break;
	default:
		dbg_sensor(1, "[%s] not support aeb mode(%d)\n",
				__func__, cis->cis_data->cur_aeb_mode);
	}

	if (ret < 0) {
		err("is_sensor_write fail!!");
		goto p_err;
	}

	ret = sensor_2ld_cis_group_param_hold_func(subdev, 0x00);
	if (ret < 0)
		err("group_param_hold_func failed");

	cis->cis_data->pre_aeb_mode = cis->cis_data->cur_aeb_mode;

p_err:
	return ret;
}

#ifdef USE_CAMERA_EMBEDDED_HEADER
#define SENSOR_2LD_PAGE_LENGTH 512
#define SENSOR_2LD_VALID_TAG 0x5A
#define SENSOR_2LD_FRAME_ID_PAGE 0
#define SENSOR_2LD_FRAME_ID_OFFSET 26
#define SENSOR_2LD_FLL_MSB_PAGE 0
#define SENSOR_2LD_FLL_MSB_OFFSET 358
#define SENSOR_2LD_FLL_LSB_PAGE 0
#define SENSOR_2LD_FLL_LSB_OFFSET 360
#define SENSOR_2LD_CIT_MSB_PAGE 0
#define SENSOR_2LD_CIT_MSB_OFFSET 210
#define SENSOR_2LD_CIT_LSB_PAGE 0
#define SENSOR_2LD_CIT_LSB_OFFSET 212
#define SENSOR_2LD_FRAME_COUNT_PAGE 0
#define SENSOR_2LD_FRAME_COUNT_OFFSET 16

static u32 frame_id_idx = (SENSOR_2LD_PAGE_LENGTH * SENSOR_2LD_FRAME_ID_PAGE) + SENSOR_2LD_FRAME_ID_OFFSET;
static u32 fll_msb_idx = (SENSOR_2LD_PAGE_LENGTH * SENSOR_2LD_FLL_MSB_PAGE) + SENSOR_2LD_FLL_MSB_OFFSET;
static u32 fll_lsb_idx = (SENSOR_2LD_PAGE_LENGTH * SENSOR_2LD_FLL_LSB_PAGE) + SENSOR_2LD_FLL_LSB_OFFSET;
static u32 cit_msb_idx = (SENSOR_2LD_PAGE_LENGTH * SENSOR_2LD_CIT_MSB_PAGE) + SENSOR_2LD_CIT_MSB_OFFSET;
static u32 cit_lsb_idx = (SENSOR_2LD_PAGE_LENGTH * SENSOR_2LD_CIT_LSB_PAGE) + SENSOR_2LD_CIT_LSB_OFFSET;
static u32 frame_count_idx = (SENSOR_2LD_PAGE_LENGTH * SENSOR_2LD_FRAME_COUNT_PAGE) + SENSOR_2LD_FRAME_COUNT_OFFSET;

static int sensor_2ld_cis_get_frame_id(struct v4l2_subdev *subdev, u8 *embedded_buf, u16 *frame_id)
{
	int ret = 0;
	struct is_cis *cis = NULL;

	WARN_ON(!subdev);

	cis = (struct is_cis *)v4l2_get_subdevdata(subdev);
	WARN_ON(!cis);
	WARN_ON(!cis->cis_data);

	if (embedded_buf[frame_id_idx-1] == SENSOR_2LD_VALID_TAG) {
		*frame_id = embedded_buf[frame_id_idx];

		dbg_sensor(1, "%s - frame_count(%d), frame_id(0x%x), fll(0x%x) cit(0x%x)",
			__func__, embedded_buf[frame_count_idx], *frame_id,
			((embedded_buf[fll_msb_idx]<<8)|embedded_buf[fll_lsb_idx]),
			((embedded_buf[cit_msb_idx]<<8)|embedded_buf[cit_lsb_idx]));
	} else {
		err("%s : invalid valid tag(0x%x)", __func__, embedded_buf[frame_id_idx-1]);
		*frame_id = 1;
	}

	cis->cis_data->sen_frame_id = *frame_id;

	return ret;
}
#endif

static void sensor_2ld_set_integration_max_margin(u32 mode, cis_shared_data *cis_data)
{
	WARN_ON(!cis_data);

	switch (mode) {
#ifdef USE_CAMERA_2LD_4000X3000
	case SENSOR_2LD_4000X3000_30FPS:
	case SENSOR_2LD_4000X3000_60FPS:
	case SENSOR_2LD_4000X2252_60FPS:
	case SENSOR_2LD_4000X2252_30FPS:
	case SENSOR_2LD_4000X3000_24FPS:
	case SENSOR_2LD_4000X2252_24FPS:
	case SENSOR_2LD_4000X2252_120FPS:
#else
	case SENSOR_2LD_4032X3024_30FPS:
	case SENSOR_2LD_4032X3024_60FPS:
	case SENSOR_2LD_4032X2268_60FPS:
	case SENSOR_2LD_4032X2268_30FPS:
	case SENSOR_2LD_4032X3024_24FPS:
	case SENSOR_2LD_4032X2268_24FPS:
	case SENSOR_2LD_4032X2268_120FPS:
#endif
	case SENSOR_2LD_3328X1872_120FPS:
	case SENSOR_2LD_2800X2100_30FPS:
		/* FRS */
		cis_data->max_margin_coarse_integration_time = SENSOR_2LD_COARSE_INTEGRATION_TIME_MAX_MARGIN;
		break;
#ifdef USE_CAMERA_2LD_4000X3000
	case SENSOR_2LD_2000X1500_30FPS:
	case SENSOR_2LD_2000X1128_30FPS:
#else
	case SENSOR_2LD_2016X1512_30FPS:
	case SENSOR_2LD_2016X1134_30FPS:
#endif
	case SENSOR_2LD_2016X1134_240FPS:
	case SENSOR_2LD_2016X1134_480FPS:
	case SENSOR_2LD_1008X756_120FPS_MODE2:
	case SENSOR_2LD_2016X1134_60FPS_MODE2_SSM_960:
	case SENSOR_2LD_2016X1134_60FPS_MODE2_SSM_480:
	case SENSOR_2LD_1280X720_60FPS_MODE2_SSM_960:
	case SENSOR_2LD_1280X720_60FPS_MODE2_SSM_960_SDC_OFF:
		/* Binning */
		cis_data->max_margin_coarse_integration_time = 0x2E;
		break;
	default:
		err("[%s] Unsupport 2ld sensor mode\n", __func__);
		cis_data->max_margin_coarse_integration_time = SENSOR_2LD_COARSE_INTEGRATION_TIME_MAX_MARGIN;
		break;
	}

	dbg_sensor(1, "max_margin_coarse_integration_time(%d)\n", cis_data->max_margin_coarse_integration_time);
}

static void sensor_2ld_cis_data_calculation(const struct sensor_pll_info_compact *pll_info_compact,
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
		"(pll_info_compact->frame_length_lines(%d) * pll_info_compact->line_length_pck(%d))\n",
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
	cis_data->min_fine_integration_time = SENSOR_2LD_FINE_INTEGRATION_TIME_MIN;
	cis_data->max_fine_integration_time = SENSOR_2LD_FINE_INTEGRATION_TIME_MAX;
	cis_data->min_coarse_integration_time = SENSOR_2LD_COARSE_INTEGRATION_TIME_MIN;
}

void sensor_2ld_cis_data_calc(struct v4l2_subdev *subdev, u32 mode)
{
	struct is_cis *cis = NULL;

	WARN_ON(!subdev);

	cis = (struct is_cis *)v4l2_get_subdevdata(subdev);
	WARN_ON(!cis);
	WARN_ON(!cis->cis_data);

	if (mode >= sensor_2ld_max_setfile_num) {
		err("invalid mode(%d)!!", mode);
		return;
	}

	if (cis->cis_data->stream_on) {
		info("[%s] call mode change in stream on state\n", __func__);
		sensor_cis_wait_streamon(subdev);
		sensor_2ld_cis_stream_off(subdev);
		sensor_cis_wait_streamoff(subdev);
		info("[%s] stream off done\n", __func__);
	}

	sensor_2ld_cis_data_calculation(sensor_2ld_pllinfos[mode], cis->cis_data);
}

static int sensor_2ld_wait_stream_off_status(cis_shared_data *cis_data)
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

int sensor_2ld_cis_select_setfile(struct v4l2_subdev *subdev)
{
	int ret = 0;
	u16 rev = 0;
	struct is_cis *cis = NULL;
	struct is_module_enum *module;
	struct is_device_sensor_peri *sensor_peri = NULL;
	struct sensor_open_extended *ext_info;
	u8 check = 0;

	WARN_ON(!subdev);

	cis = (struct is_cis *)v4l2_get_subdevdata(subdev);
	WARN_ON(!cis);
	WARN_ON(!cis->cis_data);

	sensor_peri = container_of(cis, struct is_device_sensor_peri, cis);
	module = sensor_peri->module;
	ext_info = &module->ext;
	WARN_ON(!ext_info);

	rev = cis->cis_data->cis_rev;

	switch (rev) {
	case 0xA000:
	case 0xA001:
	case 0xA101:
	case 0xA102:
	case 0xA201:
	case 0xA202:
		info("2ld sensor revision(%#x)\n", rev);
		sensor_2ld_global = sensor_2ld_setfile_A_Global_A2;
		sensor_2ld_global_size = ARRAY_SIZE(sensor_2ld_setfile_A_Global_A2);
		sensor_2ld_setfiles = sensor_2ld_setfiles_A;
		sensor_2ld_setfile_sizes = sensor_2ld_setfile_A_sizes;
		sensor_2ld_pllinfos = sensor_2ld_pllinfos_A;
		sensor_2ld_max_setfile_num = ARRAY_SIZE(sensor_2ld_setfiles_A);
#ifdef USE_CAMERA_SENSOR_RETENTION
		sensor_2ld_global_retention = sensor_2ld_setfile_A_Global_retention;
		sensor_2ld_global_retention_size = ARRAY_SIZE(sensor_2ld_setfile_A_Global_retention);
		sensor_2ld_retention = sensor_2ld_setfiles_A_retention;
		sensor_2ld_retention_size = sensor_2ld_setfile_A_sizes_retention;
		sensor_2ld_max_retention_num = ARRAY_SIZE(sensor_2ld_setfiles_A_retention);
		sensor_2ld_load_sram = sensor_2ld_setfile_A_load_sram;
		sensor_2ld_load_sram_size = sensor_2ld_setfile_A_sizes_load_sram;
#endif
		break;
	case 0xA301:
		info("2ld sensor revision(%#x)\n", rev);
		sensor_2ld_global = sensor_2ld_setfile_A_Global_A3;
		sensor_2ld_global_size = ARRAY_SIZE(sensor_2ld_setfile_A_Global_A3);
		sensor_2ld_setfiles = sensor_2ld_setfiles_A;
		sensor_2ld_setfile_sizes = sensor_2ld_setfile_A_sizes;
		sensor_2ld_pllinfos = sensor_2ld_pllinfos_A;
		sensor_2ld_max_setfile_num = ARRAY_SIZE(sensor_2ld_setfiles_A);
#ifdef USE_CAMERA_SENSOR_RETENTION
		sensor_2ld_global_retention = sensor_2ld_setfile_A_Global_retention;
		sensor_2ld_global_retention_size = ARRAY_SIZE(sensor_2ld_setfile_A_Global_retention);
		sensor_2ld_retention = sensor_2ld_setfiles_A_retention;
		sensor_2ld_retention_size = sensor_2ld_setfile_A_sizes_retention;
		sensor_2ld_max_retention_num = ARRAY_SIZE(sensor_2ld_setfiles_A_retention);
		sensor_2ld_load_sram = sensor_2ld_setfile_A_load_sram;
		sensor_2ld_load_sram_size = sensor_2ld_setfile_A_sizes_load_sram;
#endif
		break;
	default:
		info("2ld sensor revision(%#x)\n", rev);
		sensor_2ld_global = sensor_2ld_setfile_A_Global_A2;
		sensor_2ld_global_size = ARRAY_SIZE(sensor_2ld_setfile_A_Global_A2);
		sensor_2ld_setfiles = sensor_2ld_setfiles_A;
		sensor_2ld_setfile_sizes = sensor_2ld_setfile_A_sizes;
		sensor_2ld_pllinfos = sensor_2ld_pllinfos_A;
		sensor_2ld_max_setfile_num = ARRAY_SIZE(sensor_2ld_setfiles_A);
#ifdef USE_CAMERA_SENSOR_RETENTION
		sensor_2ld_global_retention = sensor_2ld_setfile_A_Global_retention;
		sensor_2ld_global_retention_size = ARRAY_SIZE(sensor_2ld_setfile_A_Global_retention);
		sensor_2ld_retention = sensor_2ld_setfiles_A_retention;
		sensor_2ld_retention_size = sensor_2ld_setfile_A_sizes_retention;
		sensor_2ld_max_retention_num = ARRAY_SIZE(sensor_2ld_setfiles_A_retention);
		sensor_2ld_load_sram = sensor_2ld_setfile_A_load_sram;
		sensor_2ld_load_sram_size = sensor_2ld_setfile_A_sizes_load_sram;
#endif
		break;
	}

	I2C_MUTEX_LOCK(cis->i2c_lock);
	ret |= is_sensor_read8(cis->client, 0x0016, &check);
	I2C_MUTEX_UNLOCK(cis->i2c_lock);
	info("2ld sensor revision 0x0016(%#x), %s sample\n", check, check == 0x10 ? "GF" : "SF");
	
	return ret;
}

int sensor_2ld_cis_set_global_setting_internal(struct v4l2_subdev *subdev);

/* CIS OPS */
int sensor_2ld_cis_init(struct v4l2_subdev *subdev)
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
		warn("sensor_2ld_check_rev is fail when cis init");
		ret = -EINVAL;
		goto p_err;
	}
#endif

	sensor_2ld_cis_select_setfile(subdev);

	cis->cis_data->stream_on = false;
	cis->cis_data->product_name = cis->id;
	cis->cis_data->cur_width = SENSOR_2LD_MAX_WIDTH;
	cis->cis_data->cur_height = SENSOR_2LD_MAX_HEIGHT;
	cis->cis_data->low_expo_start = 33000;
	cis->cis_data->pre_lownoise_mode = IS_CIS_LNOFF;
	cis->cis_data->cur_lownoise_mode = IS_CIS_LNOFF;
	cis->cis_data->pre_aeb_mode = SENSOR_AEB_MODE_OFF;
	cis->cis_data->cur_aeb_mode = SENSOR_AEB_MODE_OFF;
	cis->need_mode_change = false;
	cis->long_term_mode.sen_strm_off_on_step = 0;
	cis->long_term_mode.sen_strm_off_on_enable = false;
	cis->mipi_clock_index_cur = CAM_MIPI_NOT_INITIALIZED;
	cis->mipi_clock_index_new = CAM_MIPI_NOT_INITIALIZED;
	cis->cis_data->lte_multi_capture_mode = false;
	cis->cis_data->sen_frame_id = 0x0;
	cis->cis_data->cur_pattern_mode = SENSOR_TEST_PATTERN_MODE_OFF;

	sensor_2ld_load_retention = false;
	sensor_2ld_LTE_1s_flag = false;

	sensor_2ld_cis_data_calculation(sensor_2ld_pllinfos[setfile_index], cis->cis_data);
	sensor_2ld_set_integration_max_margin(setfile_index, cis->cis_data);

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

int sensor_2ld_cis_deinit(struct v4l2_subdev *subdev)
{
	int ret = 0;

	if (sensor_2ld_load_retention == false) {
		pr_info("%s: need to load retention\n", __func__);
		sensor_2ld_cis_stream_on(subdev);
		sensor_cis_wait_streamon(subdev);
		sensor_2ld_cis_stream_off(subdev);
		sensor_cis_wait_streamoff(subdev);
		pr_info("%s: complete to load retention\n", __func__);
	}

#ifdef USE_CAMERA_SENSOR_RETENTION
	/* retention mode CRC wait calculation */
	usleep_range(10000, 10000);
#endif

	return ret;
}

int sensor_2ld_cis_log_status(struct v4l2_subdev *subdev)
{
	int ret = 0;
	struct is_cis *cis;
	struct i2c_client *client = NULL;
	u8 data8 = 0;
	u16 data16 = 0;
#ifdef USE_CAMERA_SENSOR_RETENTION
	struct is_module_enum *module;
	struct is_device_sensor_peri *sensor_peri = NULL;
	struct sensor_open_extended *ext_info;
#endif

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

#ifdef USE_CAMERA_SENSOR_RETENTION
	sensor_peri = container_of(cis, struct is_device_sensor_peri, cis);
	module = sensor_peri->module;
	ext_info = &module->ext;
	FIMC_BUG(!ext_info);

	ext_info->use_retention_mode = SENSOR_RETENTION_INACTIVE;
#endif

	I2C_MUTEX_LOCK(cis->i2c_lock);

	pr_info("[%s] *******************************\n", __func__);
	/* 4000 page */
	ret = is_sensor_read16(client, 0x0000, &data16);
	if (unlikely(!ret)) pr_info("model_id(0x%x)\n", data16); else goto i2c_err;
	ret = is_sensor_read16(client, 0x0002, &data16);
	if (unlikely(!ret)) pr_info("revision_number(0x%x)\n", data16); else goto i2c_err;
	ret = is_sensor_read8(client, 0x0005, &data8);
	if (unlikely(!ret)) pr_info("frame_count(0x%x)\n", data8); else goto i2c_err;
	ret = is_sensor_read8(client, 0x0100, &data8);
	if (unlikely(!ret)) pr_info("0x0100(0x%x)\n", data8); else goto i2c_err;
	ret = is_sensor_read16(client, 0x0340, &data16);
	if (unlikely(!ret)) pr_info("0x0340(0x%x)\n", data16); else goto i2c_err;
	ret = is_sensor_read16(client, 0x021E, &data16);
	if (unlikely(!ret)) pr_info("0x021E(0x%x)\n", data16); else goto i2c_err;
	ret = is_sensor_read16(client, 0x0202, &data16);
	if (unlikely(!ret)) pr_info("0x0202(0x%x)\n", data16); else goto i2c_err;
	ret = is_sensor_read16(client, 0x0226, &data16);
	if (unlikely(!ret)) pr_info("0x0226(0x%x)\n", data16); else goto i2c_err;
	ret = is_sensor_read16(client, 0x0702, &data16);
	if (unlikely(!ret)) pr_info("0x0702(0x%x)\n", data16); else goto i2c_err;
	ret = is_sensor_read16(client, 0x0704, &data16);
	if (unlikely(!ret)) pr_info("0x0704(0x%x)\n", data16); else goto i2c_err;
	ret = is_sensor_read16(client, 0x0204, &data16);
	if (unlikely(!ret)) pr_info("0x0204(0x%x)\n", data16); else goto i2c_err;
	ret = is_sensor_read16(client, 0x020E, &data16);
	if (unlikely(!ret)) pr_info("0x020E(0x%x)\n", data16); else goto i2c_err;
	ret = is_sensor_read16(client, 0x0230, &data16);
	if (unlikely(!ret)) pr_info("0x0230(0x%x)\n", data16); else goto i2c_err;
	ret = is_sensor_read16(client, 0x0B30, &data16);
	if (unlikely(!ret)) pr_info("0x0B30(0x%x)\n", data16); else goto i2c_err;
	ret = is_sensor_read16(client, 0x0A7A, &data16);
	if (unlikely(!ret)) pr_info("0x0A7A(0x%x)\n", data16); else goto i2c_err;
	ret = is_sensor_read16(client, 0x0A7C, &data16);
	if (unlikely(!ret)) pr_info("0x0A7C(0x%x)\n", data16); else goto i2c_err;
	ret = is_sensor_read8(client, 0x0E0B, &data8);
	if (unlikely(!ret)) pr_info("0x0E0B(0x%x)\n", data8); else goto i2c_err;
	/* 2000 page */
	ret = is_sensor_write16(client, 0x602C, 0x2000);
	if (unlikely(!ret)) pr_info("0x2000 page\n"); else goto i2c_err;
	ret = is_sensor_write16(client, 0x602E, 0xD570);
	ret |= is_sensor_read16(client, 0x6F12, &data16);
	if (unlikely(!ret)) pr_info("0xD570(0x%x)\n", data16); else goto i2c_err;
	/* 2007 page */
	ret = is_sensor_write16(client, 0x602C, 0x2007);
	if (unlikely(!ret)) pr_info("0x2007 page\n"); else goto i2c_err;
	ret = is_sensor_write16(client, 0x602E, 0xF4B2);
	ret |= is_sensor_read16(client, 0x6F12, &data16);
	if (unlikely(!ret)) pr_info("0xF4B2(0x%x)\n", data16); else goto i2c_err;
	ret = is_sensor_write16(client, 0x602E, 0xF4B6);
	ret |= is_sensor_read16(client, 0x6F12, &data16);
	if (unlikely(!ret)) pr_info("0xF4B6(0x%x)\n", data16); else goto i2c_err;
	ret = is_sensor_write16(client, 0x602E, 0xF4B8);
	ret |= is_sensor_read16(client, 0x6F12, &data16);
	if (unlikely(!ret)) pr_info("0xF4B8(0x%x)\n", data16); else goto i2c_err;
	/* restore 4000 page */
	ret = is_sensor_write16(client, 0xFCFC, 0x4000);
	ret |= is_sensor_write16(client, 0x602C, 0x4000);
	if (unlikely(!ret)) pr_info("restore to 0x4000 page\n"); else goto i2c_err;
	pr_info("[%s] *******************************\n", __func__);

i2c_err:
	I2C_MUTEX_UNLOCK(cis->i2c_lock);
p_err:
	return ret;
}

#if USE_GROUP_PARAM_HOLD
static int sensor_2ld_cis_group_param_hold_func(struct v4l2_subdev *subdev, unsigned int hold)
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
static inline int sensor_2ld_cis_group_param_hold_func(struct v4l2_subdev *subdev, unsigned int hold)
{ return 0; }
#endif

/* Input
 *	hold : true - hold, flase - no hold
 * Output
 *      return: 0 - no effect(already hold or no hold)
 *		positive - setted by request
 *		negative - ERROR value
 */
int sensor_2ld_cis_group_param_hold(struct v4l2_subdev *subdev, bool hold)
{
	int ret = 0;
	struct is_cis *cis = NULL;

	WARN_ON(!subdev);

	cis = (struct is_cis *)v4l2_get_subdevdata(subdev);

	WARN_ON(!cis);
	WARN_ON(!cis->cis_data);

	I2C_MUTEX_LOCK(cis->i2c_lock);
	ret = sensor_2ld_cis_group_param_hold_func(subdev, hold);
	if (ret < 0)
		goto p_err;

p_err:
	I2C_MUTEX_UNLOCK(cis->i2c_lock);
	return ret;
}

int sensor_2ld_cis_set_global_setting_internal(struct v4l2_subdev *subdev)
{
	int ret = 0;
	struct is_cis *cis = NULL;

	WARN_ON(!subdev);

	cis = (struct is_cis *)v4l2_get_subdevdata(subdev);
	WARN_ON(!cis);

	I2C_MUTEX_LOCK(cis->i2c_lock);
	info("[%s] global setting start\n", __func__);
	/* setfile global setting is at camera entrance */
	ret |= sensor_cis_set_registers(subdev, sensor_2ld_global, sensor_2ld_global_size);
	if (ret < 0) {
		err("sensor_2ld_set_registers fail!!");
		goto p_err;
	}

	info("[%s] global setting done\n", __func__);

p_err:
	I2C_MUTEX_UNLOCK(cis->i2c_lock);
	return ret;
}

#ifdef USE_CAMERA_SENSOR_RETENTION
int sensor_2ld_cis_set_global_setting_retention(struct v4l2_subdev *subdev)
{
	int ret = 0;
	struct is_cis *cis = NULL;

	WARN_ON(!subdev);

	cis = (struct is_cis *)v4l2_get_subdevdata(subdev);
	WARN_ON(!cis);

	I2C_MUTEX_LOCK(cis->i2c_lock);
	info("[%s] global retention setting start\n", __func__);
	/* setfile global retention setting is at camera entrance */
	ret = sensor_cis_set_registers(subdev, sensor_2ld_global_retention, sensor_2ld_global_retention_size);
	if (ret < 0) {
		err("sensor_2ld_set_registers fail!!");
		goto p_err;
	}

	info("[%s] global retention setting done\n", __func__);

p_err:
	I2C_MUTEX_UNLOCK(cis->i2c_lock);

	return ret;
}

#ifdef CAMERA_REAR2
int sensor_2ld_cis_retention_crc_enable(struct v4l2_subdev *subdev, u32 mode)
{
	int ret = 0;
	struct is_cis *cis = NULL;
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

	switch (mode) {
	case SENSOR_2LD_1008X756_120FPS_MODE2:
	case SENSOR_2LD_2016X1134_60FPS_MODE2_SSM_960:
	case SENSOR_2LD_2016X1134_60FPS_MODE2_SSM_480:
	case SENSOR_2LD_1280X720_60FPS_MODE2_SSM_960:
	case SENSOR_2LD_1280X720_60FPS_MODE2_SSM_960_SDC_OFF:
		break;
	default:
		/* Sensor stream on */
		is_sensor_write16(client, 0x0100, 0x0100);

		/* retention mode CRC check register enable */
		is_sensor_write8(client, 0x010E, 0x01); /* api_rw_general_setup_checksum_on_ram_enable */
		info("[MOD:D:%d] %s : retention enable CRC check\n", cis->id, __func__);

		/* Sensor stream off */
		is_sensor_write8(client, 0x0100, 0x00);
		break;
	}

p_err:
	return ret;
}
#endif
#endif

static void sensor_2ld_cis_set_paf_stat_enable(u32 mode, cis_shared_data *cis_data)
{
	WARN_ON(!cis_data);

	switch (mode) {
#ifdef USE_CAMERA_2LD_4000X3000
	case SENSOR_2LD_4000X3000_30FPS:
	case SENSOR_2LD_4000X3000_60FPS:
	case SENSOR_2LD_4000X2252_60FPS:
	case SENSOR_2LD_4000X2252_30FPS:
	case SENSOR_2LD_2000X1500_30FPS:
	case SENSOR_2LD_2000X1128_30FPS:
	case SENSOR_2LD_4000X3000_24FPS:
	case SENSOR_2LD_4000X2252_24FPS:
	case SENSOR_2LD_2016X1134_240FPS:
	case SENSOR_2LD_4000X2252_120FPS:
	case SENSOR_2LD_3328X1872_120FPS:
	case SENSOR_2LD_2800X2100_30FPS:
#else
	case SENSOR_2LD_4032X3024_30FPS:
	case SENSOR_2LD_4032X3024_60FPS:
	case SENSOR_2LD_4032X2268_60FPS:
	case SENSOR_2LD_4032X2268_30FPS:
	case SENSOR_2LD_2016X1512_30FPS:
	case SENSOR_2LD_2016X1134_30FPS:
	case SENSOR_2LD_4032X3024_24FPS:
	case SENSOR_2LD_4032X2268_24FPS:
	case SENSOR_2LD_2016X1134_240FPS:
	case SENSOR_2LD_4032X2268_120FPS:
	case SENSOR_2LD_3328X1872_120FPS:
	case SENSOR_2LD_2800X2100_30FPS:
#endif
		cis_data->is_data.paf_stat_enable = true;
		break;
	default:
		cis_data->is_data.paf_stat_enable = false;
		break;
	}
}

bool sensor_2ld_cis_get_lownoise_supported(cis_shared_data *cis_data)
{
	WARN_ON(!cis_data);

	if (cis_data->cur_aeb_mode)
		return false;

	switch (cis_data->sens_config_index_cur) {
#ifdef USE_CAMERA_2LD_4000X3000
	case SENSOR_2LD_4000X3000_30FPS:
	case SENSOR_2LD_4000X2252_30FPS:
	case SENSOR_2LD_4000X3000_24FPS:
	case SENSOR_2LD_4000X2252_24FPS:
#else
	case SENSOR_2LD_4032X3024_30FPS:
	case SENSOR_2LD_4032X2268_30FPS:
	case SENSOR_2LD_4032X3024_24FPS:
	case SENSOR_2LD_4032X2268_24FPS:
#endif
		return true;
	default:
		break;
	}

	return false;
}

/* For FHD autofps only support LN2 in case of below 30fps */
bool sensor_2ld_cis_get_lownoise_autofps_supported(cis_shared_data *cis_data)
{
	WARN_ON(!cis_data);

	if (cis_data->cur_aeb_mode)
		return false;

	switch (cis_data->sens_config_index_cur) {
#ifdef USE_CAMERA_2LD_4000X3000
	case SENSOR_2LD_4000X3000_60FPS:
	case SENSOR_2LD_4000X2252_60FPS:
#else
	case SENSOR_2LD_4032X3024_60FPS:
	case SENSOR_2LD_4032X2268_60FPS:
#endif
		if (cis_data->cur_frame_us_time > 33000 &&
			cis_data->cur_lownoise_mode != IS_CIS_LN4)
			return true;
	default:
		break;
	}

	return false;
}

int sensor_2ld_cis_mode_change(struct v4l2_subdev *subdev, u32 mode)
{
	int ret = 0;
	struct is_cis *cis = NULL;
	struct is_module_enum *module;
	struct is_device_sensor_peri *sensor_peri = NULL;
	struct sensor_open_extended *ext_info = NULL;
	u8 data8 = 0;

	WARN_ON(!subdev);

	cis = (struct is_cis *)v4l2_get_subdevdata(subdev);
	WARN_ON(!cis);
	WARN_ON(!cis->cis_data);

	if (mode >= sensor_2ld_max_setfile_num) {
		err("invalid mode(%d)!!", mode);
		ret = -EINVAL;
		goto p_err;
	}

	sensor_2ld_ln_mode_delay_count = 0;

	sensor_peri = container_of(cis, struct is_device_sensor_peri, cis);
	module = sensor_peri->module;
	ext_info = &module->ext;
	WARN_ON(!ext_info);

#if 0 /* cis_data_calculation is called in module_s_format */
	sensor_2ld_cis_data_calculation(sensor_2ld_pllinfos[mode], cis->cis_data);
#endif
	sensor_2ld_set_integration_max_margin(mode, cis->cis_data);

	cis->mipi_clock_index_cur = CAM_MIPI_NOT_INITIALIZED;

	sensor_2ld_cis_set_paf_stat_enable(mode, cis->cis_data);

	I2C_MUTEX_LOCK(cis->i2c_lock);

#ifdef USE_CAMERA_SENSOR_RETENTION
	/* Retention mode sensor mode select */
	if (ext_info->use_retention_mode == SENSOR_RETENTION_ACTIVATED) {
		if (sensor_2ld_load_retention == false) {
			is_sensor_write16(cis->client, 0x6028, 0x2000);
			is_sensor_write16(cis->client, 0x602A, 0x0E1C);
			is_sensor_write16(cis->client, 0x6F12, 0x0000);
			is_sensor_write16(cis->client, 0x6F12, 0x0000);
		}

		sensor_2ld_load_retention = false;

		switch (mode) {
#ifdef USE_CAMERA_2LD_4000X3000
		case SENSOR_2LD_4000X3000_30FPS:
			info("[%s] retention mode: SENSOR_2LD_4000X3000_30FPS\n", __func__);
			ret = sensor_cis_set_registers(subdev,
				sensor_2ld_load_sram[SENSOR_2LD_4000x3000_30FPS_LOAD_SRAM],
				sensor_2ld_load_sram_size[SENSOR_2LD_4000x3000_30FPS_LOAD_SRAM]);
			if (ret < 0) {
				err("sensor_2ld_set_registers fail!!");
				goto p_err_i2c_unlock;
			}
			break;
		case SENSOR_2LD_4000X2252_30FPS:
			info("[%s] retention mode: SENSOR_2LD_4000X2252_30FPS\n", __func__);
			ret = sensor_cis_set_registers(subdev,
				sensor_2ld_load_sram[SENSOR_2LD_4000x2252_30FPS_LOAD_SRAM],
				sensor_2ld_load_sram_size[SENSOR_2LD_4000x2252_30FPS_LOAD_SRAM]);
			if (ret < 0) {
				err("sensor_2ld_set_registers fail!!");
				goto p_err_i2c_unlock;
			}
			break;
		case SENSOR_2LD_4000X2252_60FPS:
			info("[%s] retention mode: SENSOR_2LD_4000X2252_60FPS\n", __func__);
			ret = sensor_cis_set_registers(subdev,
				sensor_2ld_load_sram[SENSOR_2LD_4000x2252_60FPS_LOAD_SRAM],
				sensor_2ld_load_sram_size[SENSOR_2LD_4000x2252_60FPS_LOAD_SRAM]);
			if (ret < 0) {
				err("sensor_2ld_set_registers fail!!");
				goto p_err_i2c_unlock;
			}
			break;
		case SENSOR_2LD_1008X756_120FPS_MODE2:
			info("[%s] retention mode: SENSOR_2LD_1008X756_120FPS_MODE2\n", __func__);
			ret = sensor_cis_set_registers(subdev,
				sensor_2ld_load_sram[SENSOR_2LD_1008x756_120FPS_LOAD_SRAM],
				sensor_2ld_load_sram_size[SENSOR_2LD_1008x756_120FPS_LOAD_SRAM]);
			if (ret < 0) {
				err("sensor_2ld_set_registers fail!!");
				goto p_err_i2c_unlock;
			}
			break;
		case SENSOR_2LD_4000X3000_24FPS:
			info("[%s] retention mode: SENSOR_2LD_4000X3000_24FPS\n", __func__);
			ret = sensor_cis_set_registers(subdev,
				sensor_2ld_load_sram[SENSOR_2LD_4000x3000_24FPS_LOAD_SRAM],
				sensor_2ld_load_sram_size[SENSOR_2LD_4000x3000_24FPS_LOAD_SRAM]);
			if (ret < 0) {
				err("sensor_2ld_set_registers fail!!");
				goto p_err_i2c_unlock;
			}
			break;
		case SENSOR_2LD_4000X2252_24FPS:
			info("[%s] retention mode: SENSOR_2LD_4000X2252_24FPS\n", __func__);
			ret = sensor_cis_set_registers(subdev,
				sensor_2ld_load_sram[SENSOR_2LD_4000x2252_24FPS_LOAD_SRAM],
				sensor_2ld_load_sram_size[SENSOR_2LD_4000x2252_24FPS_LOAD_SRAM]);
			if (ret < 0) {
				err("sensor_2ld_set_registers fail!!");
				goto p_err_i2c_unlock;
			}
			break;
#else
		case SENSOR_2LD_4032X3024_30FPS:
			info("[%s] retention mode: SENSOR_2LD_4032X3024_30FPS\n", __func__);
			ret = sensor_cis_set_registers(subdev,
				sensor_2ld_load_sram[SENSOR_2LD_4032x3024_30FPS_LOAD_SRAM],
				sensor_2ld_load_sram_size[SENSOR_2LD_4032x3024_30FPS_LOAD_SRAM]);
			if (ret < 0) {
				err("sensor_2ld_set_registers fail!!");
				goto p_err_i2c_unlock;
			}
			break;
		case SENSOR_2LD_4032X2268_30FPS:
			info("[%s] retention mode: SENSOR_2LD_4032X2268_30FPS\n", __func__);
			ret = sensor_cis_set_registers(subdev,
				sensor_2ld_load_sram[SENSOR_2LD_4032x2268_30FPS_LOAD_SRAM],
				sensor_2ld_load_sram_size[SENSOR_2LD_4032x2268_30FPS_LOAD_SRAM]);
			if (ret < 0) {
				err("sensor_2ld_set_registers fail!!");
				goto p_err_i2c_unlock;
			}
			break;
		case SENSOR_2LD_4032X2268_60FPS:
			info("[%s] retention mode: SENSOR_2LD_4032X2268_60FPS\n", __func__);
			ret = sensor_cis_set_registers(subdev,
				sensor_2ld_load_sram[SENSOR_2LD_4032x2268_60FPS_LOAD_SRAM],
				sensor_2ld_load_sram_size[SENSOR_2LD_4032x2268_60FPS_LOAD_SRAM]);
			if (ret < 0) {
				err("sensor_2ld_set_registers fail!!");
				goto p_err_i2c_unlock;
			}
			break;
		case SENSOR_2LD_1008X756_120FPS_MODE2:
			info("[%s] retention mode: SENSOR_2LD_1008X756_120FPS_MODE2\n", __func__);
			ret = sensor_cis_set_registers(subdev,
				sensor_2ld_load_sram[SENSOR_2LD_1008x756_120FPS_LOAD_SRAM],
				sensor_2ld_load_sram_size[SENSOR_2LD_1008x756_120FPS_LOAD_SRAM]);
			if (ret < 0) {
				err("sensor_2ld_set_registers fail!!");
				goto p_err_i2c_unlock;
			}
			break;
		case SENSOR_2LD_4032X3024_24FPS:
			info("[%s] retention mode: SENSOR_2LD_4032X3024_24FPS\n", __func__);
			ret = sensor_cis_set_registers(subdev,
				sensor_2ld_load_sram[SENSOR_2LD_4032x3024_24FPS_LOAD_SRAM],
				sensor_2ld_load_sram_size[SENSOR_2LD_4032x3024_24FPS_LOAD_SRAM]);
			if (ret < 0) {
				err("sensor_2ld_set_registers fail!!");
				goto p_err_i2c_unlock;
			}
			break;
		case SENSOR_2LD_4032X2268_24FPS:
			info("[%s] retention mode: SENSOR_2LD_4032X2268_24FPS\n", __func__);
			ret = sensor_cis_set_registers(subdev,
				sensor_2ld_load_sram[SENSOR_2LD_4032x2268_24FPS_LOAD_SRAM],
				sensor_2ld_load_sram_size[SENSOR_2LD_4032x2268_24FPS_LOAD_SRAM]);
			if (ret < 0) {
				err("sensor_2ld_set_registers fail!!");
				goto p_err_i2c_unlock;
			}
			break;
#endif
		default:
			info("[%s] not support retention sensor mode(%d)\n", __func__, mode);
			ret = sensor_cis_set_registers(subdev, sensor_2ld_setfiles[mode],
								sensor_2ld_setfile_sizes[mode]);
			if (ret < 0) {
				err("sensor_2ld_set_registers fail!!");
				goto p_err_i2c_unlock;
			}
			break;
		}
	} else
#endif
	{
		is_sensor_write8(cis->client, 0x0100, 0x00);
		info("[%s] sensor mode(%d)\n", __func__, mode);
		ret = sensor_cis_set_registers(subdev, sensor_2ld_setfiles[mode],
								sensor_2ld_setfile_sizes[mode]);
		if (ret < 0) {
			err("sensor_2ld_set_registers fail!!");
			goto p_err_i2c_unlock;
		}
		info("[%s] sensor mode done(%d)\n", __func__, mode);
	}

	pr_info("%s : disable AEB\n", __func__);
	cis->cis_data->pre_aeb_mode = SENSOR_AEB_MODE_OFF;
	cis->cis_data->cur_aeb_mode = SENSOR_AEB_MODE_OFF;
	ret |= is_sensor_write16(cis->client, 0xFCFC, 0x4000);
	ret |= is_sensor_write8(cis->client, 0x0E0B, 0x00);

	if (sensor_2ld_cis_get_lownoise_supported(cis->cis_data)) {
		cis->cis_data->pre_lownoise_mode = IS_CIS_LN2;
		cis->cis_data->cur_lownoise_mode = IS_CIS_LN2;
		sensor_2ld_cis_set_lownoise_mode_change(subdev);
	} else {
		cis->cis_data->pre_lownoise_mode = IS_CIS_LNOFF;
		cis->cis_data->cur_lownoise_mode = IS_CIS_LNOFF;
	}

	if (mode == SENSOR_2LD_2016X1134_60FPS_MODE2_SSM_960
		|| mode == SENSOR_2LD_2016X1134_60FPS_MODE2_SSM_480
		|| mode == SENSOR_2LD_1280X720_60FPS_MODE2_SSM_960
		|| mode == SENSOR_2LD_1280X720_60FPS_MODE2_SSM_960_SDC_OFF) {
		info("[%s] mitigation off (%d)\n", __func__, mode);
		ret |= is_sensor_write8(cis->client, 0x0120, 0x01);

		ret |= is_sensor_read8(cis->client, 0x1982, &data8);
		if (unlikely(!ret))
			pr_info("0x1982(0x%x)\n", data8);
		else
			goto p_err_i2c_unlock;
	}

	info("[%s] mode changed(%d)\n", __func__, mode);

p_err_i2c_unlock:
	I2C_MUTEX_UNLOCK(cis->i2c_lock);

p_err:
	/* sensor_2ld_cis_log_status(subdev); */

	return ret;
}

int sensor_2ld_cis_set_lownoise_mode_change(struct v4l2_subdev *subdev)
{
	int ret = 0;
	struct is_cis *cis = NULL;
	unsigned int mode = 0;
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

	mode = cis->cis_data->sens_config_index_cur;

	if (!sensor_2ld_cis_get_lownoise_supported(cis->cis_data)
		&& !sensor_2ld_cis_get_lownoise_autofps_supported(cis->cis_data)) {
		pr_info("[%s] not support mode (%d) evt (%x) frame_duration(%d) LN_mode(%d)\n", __func__,
			mode, cis->cis_data->cis_rev, cis->cis_data->cur_frame_us_time, cis->cis_data->cur_lownoise_mode);
		cis->cis_data->pre_lownoise_mode = cis->cis_data->cur_lownoise_mode;
		return ret;
	}

	is_sensor_read8(cis->client, 0x0005, &sensor_2ld_ln_mode_frame_count);
	pr_info("[%s] lownoise mode changed(%d) cur_mode(%d) sensor_2ld_ln_mode_frame_count(0x%x)\n",
		__func__, cis->cis_data->cur_lownoise_mode, mode, sensor_2ld_ln_mode_frame_count);

	sensor_2ld_ln_mode_delay_count = 3;

	ret = sensor_2ld_cis_group_param_hold_func(subdev, 0x01);
	if (ret < 0)
		err("group_param_hold_func failed");

	switch (cis->cis_data->cur_lownoise_mode) {
	case IS_CIS_LNOFF:
		dbg_sensor(1, "[%s] IS_CIS_LNOFF\n", __func__);
		ret |= is_sensor_write16(cis->client, 0xFCFC, 0x4000);
		ret |= is_sensor_write16(cis->client, 0x0B30, 0x0100);
#ifdef CAMERA_REAR2
		switch (mode) {
#ifdef USE_CAMERA_2LD_4000X3000
		case SENSOR_2LD_4000X3000_60FPS:
		case SENSOR_2LD_4000X3000_30FPS:
			ret |= is_sensor_write16(cis->client, 0x0A7A, 0x2A30);
			ret |= is_sensor_write16(cis->client, 0x0A7C, 0x0010);
			break;
		case SENSOR_2LD_4000X2252_60FPS:
		case SENSOR_2LD_4000X2252_30FPS:
			ret |= is_sensor_write16(cis->client, 0x0A7A, 0x2B70);
			ret |= is_sensor_write16(cis->client, 0x0A7C, 0x0010);
			break;
		case SENSOR_2LD_4000X3000_24FPS:
			ret |= is_sensor_write16(cis->client, 0x0A7A, 0x3606);
			ret |= is_sensor_write16(cis->client, 0x0A7C, 0x0010);
			break;
		case SENSOR_2LD_4000X2252_24FPS:
			ret |= is_sensor_write16(cis->client, 0x0A7A, 0x3740);
			ret |= is_sensor_write16(cis->client, 0x0A7C, 0x0010);
			break;
#else
		case SENSOR_2LD_4032X3024_60FPS:
		case SENSOR_2LD_4032X3024_30FPS:
			ret |= is_sensor_write16(cis->client, 0x0A7A, 0x2A30);
			ret |= is_sensor_write16(cis->client, 0x0A7C, 0x0010);
			break;
		case SENSOR_2LD_4032X2268_60FPS:
		case SENSOR_2LD_4032X2268_30FPS:
			ret |= is_sensor_write16(cis->client, 0x0A7A, 0x2B70);
			ret |= is_sensor_write16(cis->client, 0x0A7C, 0x0010);
			break;
		case SENSOR_2LD_4032X3024_24FPS:
			ret |= is_sensor_write16(cis->client, 0x0A7A, 0x3606);
			ret |= is_sensor_write16(cis->client, 0x0A7C, 0x0010);
			break;
		case SENSOR_2LD_4032X2268_24FPS:
			ret |= is_sensor_write16(cis->client, 0x0A7A, 0x3740);
			ret |= is_sensor_write16(cis->client, 0x0A7C, 0x0010);
			break;
#endif
		}
#endif
		cis->cis_data->max_margin_coarse_integration_time = 0x24; /* 36 */
		cis->cis_data->rolling_shutter_skew = (cis->cis_data->cur_height - 1) * cis->cis_data->line_readOut_time;
		break;
	case IS_CIS_LN2:
		dbg_sensor(1, "[%s] IS_CIS_LN2\n", __func__);
#ifdef CAMERA_REAR2
		switch (mode) {
#ifdef USE_CAMERA_2LD_4000X3000
		case SENSOR_2LD_4000X3000_60FPS:
		case SENSOR_2LD_4000X3000_30FPS:
			ret |= is_sensor_write16(cis->client, 0xFCFC, 0x4000);
			ret |= is_sensor_write16(cis->client, 0x0B30, 0x0101);
			ret |= is_sensor_write16(cis->client, 0x0A7A, 0x0065);
			ret |= is_sensor_write16(cis->client, 0x0A7C, 0x0010);
			break;
		case SENSOR_2LD_4000X2252_60FPS:
		case SENSOR_2LD_4000X2252_30FPS:
			ret |= is_sensor_write16(cis->client, 0xFCFC, 0x4000);
			ret |= is_sensor_write16(cis->client, 0x0B30, 0x0101);
			ret |= is_sensor_write16(cis->client, 0x0A7A, 0x0060);
			ret |= is_sensor_write16(cis->client, 0x0A7C, 0x0010);
			break;
		case SENSOR_2LD_4000X3000_24FPS:
			ret |= is_sensor_write16(cis->client, 0xFCFC, 0x4000);
			ret |= is_sensor_write16(cis->client, 0x0B30, 0x0101);
			ret |= is_sensor_write16(cis->client, 0x0A7A, 0x0066);
			ret |= is_sensor_write16(cis->client, 0x0A7C, 0x0010);
			break;
		case SENSOR_2LD_4000X2252_24FPS:
			ret |= is_sensor_write16(cis->client, 0xFCFC, 0x4000);
			ret |= is_sensor_write16(cis->client, 0x0B30, 0x0101);
			ret |= is_sensor_write16(cis->client, 0x0A7A, 0x0025);
			ret |= is_sensor_write16(cis->client, 0x0A7C, 0x0010);
			break;
#else
		case SENSOR_2LD_4032X3024_60FPS:
		case SENSOR_2LD_4032X3024_30FPS:
			ret |= is_sensor_write16(cis->client, 0xFCFC, 0x4000);
			ret |= is_sensor_write16(cis->client, 0x0B30, 0x0101);
			ret |= is_sensor_write16(cis->client, 0x0A7A, 0x0065);
			ret |= is_sensor_write16(cis->client, 0x0A7C, 0x0010);
			break;
		case SENSOR_2LD_4032X2268_60FPS:
		case SENSOR_2LD_4032X2268_30FPS:
			ret |= is_sensor_write16(cis->client, 0xFCFC, 0x4000);
			ret |= is_sensor_write16(cis->client, 0x0B30, 0x0101);
			ret |= is_sensor_write16(cis->client, 0x0A7A, 0x0060);
			ret |= is_sensor_write16(cis->client, 0x0A7C, 0x0010);
			break;
		case SENSOR_2LD_4032X3024_24FPS:
			ret |= is_sensor_write16(cis->client, 0xFCFC, 0x4000);
			ret |= is_sensor_write16(cis->client, 0x0B30, 0x0101);
			ret |= is_sensor_write16(cis->client, 0x0A7A, 0x0066);
			ret |= is_sensor_write16(cis->client, 0x0A7C, 0x0010);
			break;
		case SENSOR_2LD_4032X2268_24FPS:
			ret |= is_sensor_write16(cis->client, 0xFCFC, 0x4000);
			ret |= is_sensor_write16(cis->client, 0x0B30, 0x0101);
			ret |= is_sensor_write16(cis->client, 0x0A7A, 0x0025);
			ret |= is_sensor_write16(cis->client, 0x0A7C, 0x0010);
			break;
#endif
		}
#endif
		cis->cis_data->max_margin_coarse_integration_time = 0x48; /* 72 */
		cis->cis_data->rolling_shutter_skew = (cis->cis_data->cur_height - 1) * cis->cis_data->line_readOut_time * 2;
		break;
	case IS_CIS_LN4:
		dbg_sensor(1, "[%s] IS_CIS_LN4\n", __func__);
#ifdef CAMERA_REAR2
		switch (mode) {
#ifdef USE_CAMERA_2LD_4000X3000
		case SENSOR_2LD_4000X3000_30FPS:
			ret |= is_sensor_write16(cis->client, 0xFCFC, 0x4000);
			ret |= is_sensor_write16(cis->client, 0x0B30, 0x0102);
			ret |= is_sensor_write16(cis->client, 0x0A7A, 0x0265);
			ret |= is_sensor_write16(cis->client, 0x0A7C, 0x0010);
			break;
		case SENSOR_2LD_4000X2252_30FPS:
			ret |= is_sensor_write16(cis->client, 0xFCFC, 0x4000);
			ret |= is_sensor_write16(cis->client, 0x0B30, 0x0102);
			ret |= is_sensor_write16(cis->client, 0x0A7A, 0x01F0);
			ret |= is_sensor_write16(cis->client, 0x0A7C, 0x0010);
			break;
		case SENSOR_2LD_4000X3000_24FPS:
			ret |= is_sensor_write16(cis->client, 0xFCFC, 0x4000);
			ret |= is_sensor_write16(cis->client, 0x0B30, 0x0102);
			ret |= is_sensor_write16(cis->client, 0x0A7A, 0x0240);
			ret |= is_sensor_write16(cis->client, 0x0A7C, 0x0010);
			break;
		case SENSOR_2LD_4000X2252_24FPS:
			ret |= is_sensor_write16(cis->client, 0xFCFC, 0x4000);
			ret |= is_sensor_write16(cis->client, 0x0B30, 0x0102);
			ret |= is_sensor_write16(cis->client, 0x0A7A, 0x01C0);
			ret |= is_sensor_write16(cis->client, 0x0A7C, 0x0010);
			break;
		default:
			pr_info("[%s] mode(%d) not support LN4\n", __func__, mode);
			break;
#else
		case SENSOR_2LD_4032X3024_30FPS:
			ret |= is_sensor_write16(cis->client, 0xFCFC, 0x4000);
			ret |= is_sensor_write16(cis->client, 0x0B30, 0x0102);
			ret |= is_sensor_write16(cis->client, 0x0A7A, 0x0265);
			ret |= is_sensor_write16(cis->client, 0x0A7C, 0x0010);
			break;
		case SENSOR_2LD_4032X2268_30FPS:
			ret |= is_sensor_write16(cis->client, 0xFCFC, 0x4000);
			ret |= is_sensor_write16(cis->client, 0x0B30, 0x0102);
			ret |= is_sensor_write16(cis->client, 0x0A7A, 0x01F0);
			ret |= is_sensor_write16(cis->client, 0x0A7C, 0x0010);
			break;
		case SENSOR_2LD_4032X3024_24FPS:
			ret |= is_sensor_write16(cis->client, 0xFCFC, 0x4000);
			ret |= is_sensor_write16(cis->client, 0x0B30, 0x0102);
			ret |= is_sensor_write16(cis->client, 0x0A7A, 0x0240);
			ret |= is_sensor_write16(cis->client, 0x0A7C, 0x0010);
			break;
		case SENSOR_2LD_4032X2268_24FPS:
			ret |= is_sensor_write16(cis->client, 0xFCFC, 0x4000);
			ret |= is_sensor_write16(cis->client, 0x0B30, 0x0102);
			ret |= is_sensor_write16(cis->client, 0x0A7A, 0x01C0);
			ret |= is_sensor_write16(cis->client, 0x0A7C, 0x0010);
			break;
		default:
			pr_info("[%s] mode(%d) not support LN4\n", __func__, mode);
			break;
#endif
		}
#endif
		cis->cis_data->max_margin_coarse_integration_time = 0x6C; /* 108 */
		cis->cis_data->rolling_shutter_skew = (cis->cis_data->cur_height - 1) * cis->cis_data->line_readOut_time * 4;
		break;
	case IS_CIS_LN2_PEDESTAL128:
		pr_info("[%s] IS_CIS_LN2_PEDESTAL128 to be checked\n", __func__);
		break;
	case IS_CIS_LN4_PEDESTAL128:
		pr_info("[%s] IS_CIS_LN4_PEDESTAL128 to be checked\n", __func__);
		break;
	default:
		dbg_sensor(1, "[%s] not support lownoise mode(%d)\n",
				__func__, cis->cis_data->cur_lownoise_mode);
	}

	if (ret < 0) {
		err("sensor_2ld_set_registers fail!!");
		goto p_err;
	}

	pr_info("[%s] max_margin_coarse_integration_time(%d)\n", __func__,
		cis->cis_data->max_margin_coarse_integration_time);

	ret = sensor_2ld_cis_group_param_hold_func(subdev, 0x00);
	if (ret < 0)
		err("group_param_hold_func failed");

	cis->cis_data->pre_lownoise_mode = cis->cis_data->cur_lownoise_mode;

p_err:
	return ret;
}

int sensor_2ld_cis_set_global_setting(struct v4l2_subdev *subdev)
{
	int ret = 0;
#ifdef USE_CAMERA_SENSOR_RETENTION
	struct is_cis *cis = NULL;
	struct is_module_enum *module;
	struct is_device_sensor_peri *sensor_peri = NULL;
	struct sensor_open_extended *ext_info;

	WARN_ON(!subdev);

	cis = (struct is_cis *)v4l2_get_subdevdata(subdev);
	WARN_ON(!cis);

	sensor_peri = container_of(cis, struct is_device_sensor_peri, cis);
	module = sensor_peri->module;
	ext_info = &module->ext;
	WARN_ON(!ext_info);

	/* setfile global setting is at camera entrance */
	if (ext_info->use_retention_mode == SENSOR_RETENTION_INACTIVE) {
		sensor_2ld_cis_set_global_setting_internal(subdev);
		sensor_2ld_cis_retention_prepare(subdev);
	} else if (ext_info->use_retention_mode == SENSOR_RETENTION_ACTIVATED) {
		sensor_2ld_cis_retention_crc_check(subdev);
	} else { /* SENSOR_RETENTION_UNSUPPORTED */
		sensor_2ld_cis_set_global_setting_internal(subdev);
	}
#else
	WARN_ON(!subdev);
	sensor_2ld_cis_set_global_setting_internal(subdev);
#endif

	return ret;
}

#ifdef USE_CAMERA_SENSOR_RETENTION
int sensor_2ld_cis_retention_prepare(struct v4l2_subdev *subdev)
{
	int ret = 0;
	int i = 0;
	struct is_cis *cis = NULL;
	struct is_module_enum *module;
	struct is_device_sensor_peri *sensor_peri = NULL;
	struct sensor_open_extended *ext_info;

	WARN_ON(!subdev);

	cis = (struct is_cis *)v4l2_get_subdevdata(subdev);
	WARN_ON(!cis);

	sensor_peri = container_of(cis, struct is_device_sensor_peri, cis);
	module = sensor_peri->module;
	ext_info = &module->ext;
	WARN_ON(!ext_info);

	I2C_MUTEX_LOCK(cis->i2c_lock);
	for (i = 0; i < sensor_2ld_max_retention_num; i++) {
		ret = sensor_cis_set_registers(subdev, sensor_2ld_retention[i], sensor_2ld_retention_size[i]);
		if (ret < 0) {
			err("sensor_2ld_set_registers fail!!");
			goto p_err;
		}
	}

	ret |= is_sensor_write16(cis->client, 0x6028, 0x3000);
	ret |= is_sensor_write16(cis->client, 0x602A, 0x0484);
	ret |= is_sensor_write16(cis->client, 0x6F12, 0x0100);
	ret |= is_sensor_write16(cis->client, 0x010E, 0x0100);

	ret |= is_sensor_write16(cis->client, 0x0BCC, 0x0000); // Block mipi signal
	ret |= is_sensor_write16(cis->client, 0x0100, 0x0100);
	ret |= is_sensor_write8(cis->client, 0x0100, 0x00);

	if (ret < 0) {
		err("is_sensor_write fail!!");
		goto p_err;
	}

	usleep_range(10000, 10000);

	ext_info->use_retention_mode = SENSOR_RETENTION_ACTIVATED;

	info("[%s] retention sensor RAM write done\n", __func__);

p_err:
	I2C_MUTEX_UNLOCK(cis->i2c_lock);

	return ret;
}

int sensor_2ld_cis_retention_crc_check(struct v4l2_subdev *subdev)
{
	int ret = 0;
	u8 crc_check = 0;
	struct is_cis *cis = NULL;

	WARN_ON(!subdev);

	cis = (struct is_cis *)v4l2_get_subdevdata(subdev);
	WARN_ON(!cis);
	WARN_ON(!cis->cis_data);

	I2C_MUTEX_LOCK(cis->i2c_lock);

	/* retention mode CRC check */
	is_sensor_read8(cis->client, 0x100E, &crc_check); /* api_ro_checksum_on_ram_passed */

	I2C_MUTEX_UNLOCK(cis->i2c_lock);

	if (crc_check == 0x01) {
		info("[%s] retention SRAM CRC check: pass!\n", __func__);

		/* init pattern */
		is_sensor_write16(cis->client, 0x0600, 0x0000);

		ret = sensor_2ld_cis_set_global_setting_retention(subdev);
		if (ret < 0) {
			err("write retention global setting failed");
			goto p_err;
		}
	} else {
		info("[%s] retention SRAM CRC check: fail!\n", __func__);
		info("retention CRC Check register value: 0x%x\n", crc_check);
		info("[%s] rewrite retention modes to SRAM\n", __func__);

		ret = sensor_2ld_cis_set_global_setting_internal(subdev);
		if (ret < 0) {
			err("CRC error recover: rewrite sensor global setting failed");
			goto p_err;
		}

		ret = sensor_2ld_cis_retention_prepare(subdev);
		if (ret < 0) {
			err("CRC error recover: retention prepare failed");
			goto p_err;
		}
	}

p_err:

	return ret;
}
#endif

/* TODO: Sensor set size sequence(sensor done, sensor stop, 3AA done in FW case */
int sensor_2ld_cis_set_size(struct v4l2_subdev *subdev, cis_shared_data *cis_data)
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
	ret = sensor_2ld_wait_stream_off_status(cis_data);
	if (ret) {
		err("Must stream off\n");
		ret = -EINVAL;
		goto p_err;
	}

	binning = cis_data->binning;
	if (binning) {
		ratio_w = (SENSOR_2LD_MAX_WIDTH / cis_data->cur_width);
		ratio_h = (SENSOR_2LD_MAX_HEIGHT / cis_data->cur_height);
	} else {
		ratio_w = 1;
		ratio_h = 1;
	}

	if (((cis_data->cur_width * ratio_w) > SENSOR_2LD_MAX_WIDTH) ||
		((cis_data->cur_height * ratio_h) > SENSOR_2LD_MAX_HEIGHT)) {
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
	start_x = ((SENSOR_2LD_MAX_WIDTH - cis_data->cur_width * ratio_w) / 2) & (~0x1);
	start_y = ((SENSOR_2LD_MAX_HEIGHT - cis_data->cur_height * ratio_h) / 2) & (~0x1);
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

int sensor_2ld_cis_stream_on(struct v4l2_subdev *subdev)
{
	int ret = 0;
	struct is_cis *cis;
	struct i2c_client *client;
	cis_shared_data *cis_data;
	struct is_device_sensor_peri *sensor_peri = NULL;
	struct is_device_sensor *device;
	u32 ex_mode;

#ifdef CAMERA_REAR2
	u32 mode;
#endif

#ifdef DEBUG_SENSOR_TIME
	struct timeval st, end;

	do_gettimeofday(&st);
#endif

	WARN_ON(!subdev);

	cis = (struct is_cis *)v4l2_get_subdevdata(subdev);

	WARN_ON(!cis);
	WARN_ON(!cis->cis_data);

	sensor_peri = container_of(cis, struct is_device_sensor_peri, cis);
	WARN_ON(!sensor_peri);

	device = (struct is_device_sensor *)v4l2_get_subdev_hostdata(subdev);
	WARN_ON(!device);

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
	ret = sensor_2ld_cis_group_param_hold_func(subdev, 0x01);
	if (ret < 0)
		err("group_param_hold_func failed at stream on");

#ifdef DEBUG_2LD_PLL
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
	 * then 8 ms waiting is needed before the StreamOn of a sensor (S5K2LD).
	 */
	if (test_bit(IS_SENSOR_PREPROCESSOR_AVAILABLE, &sensor_peri->peri_state))
		mdelay(8);

	device = (struct is_device_sensor *)v4l2_get_subdev_hostdata(subdev);
	if (unlikely(!device)) {
		err("device sensor is null");
		ret = -EINVAL;
		goto p_err;
	}

	ex_mode = is_sensor_g_ex_mode(device);

#ifdef CONFIG_SEC_FACTORY
	if (ex_mode == EX_DUALFPS_480 || ex_mode == EX_DUALFPS_960) {
		info("%s : apply TX off setting for SSM factory test\n", __func__);
		is_sensor_write16(client, 0xFCFC, 0x4000);
		is_sensor_write16(client, 0x6000, 0x0005);
		is_sensor_write16(client, 0xFCFC, 0x2000);
		is_sensor_write16(client, 0x8A10, 0x0009);
		is_sensor_write16(client, 0x8A0C, 0x03F7);
		is_sensor_write16(client, 0xFCFC, 0x4000);
		is_sensor_write16(client, 0x6000, 0x0085);
	}
#endif

	/* Sensor stream on */
	info("%s\n", __func__);
	is_sensor_write16(client, 0x0BCC, 0x0100); // Enable mipi signal
	is_sensor_write16(client, 0x0100, 0x0100);

	ret = sensor_2ld_cis_group_param_hold_func(subdev, 0x00);
	if (ret < 0)
		err("group_param_hold_func failed at stream on");

	cis_data->stream_on = true;
	sensor_2ld_load_retention = true;

#ifdef CAMERA_REAR2
	mode = cis_data->sens_config_index_cur;
	dbg_sensor(1, "[%s] sens_config_index_cur=%d\n", __func__, mode);

	switch (mode) {
#ifdef USE_CAMERA_2LD_4000X3000
	case SENSOR_2LD_4000X3000_30FPS:
	case SENSOR_2LD_4000X2252_30FPS:
#else
	case SENSOR_2LD_4032X3024_30FPS:
	case SENSOR_2LD_4032X2268_30FPS:
#endif
		cis->cis_data->min_sync_frame_us_time = cis->cis_data->min_frame_us_time = 33333;
		break;
	default:
		break;
	}
#endif

#ifdef DEBUG_SENSOR_TIME
	do_gettimeofday(&end);
	dbg_sensor(1, "[%s] time %lu us\n", __func__, (end.tv_sec - st.tv_sec)*1000000 + (end.tv_usec - st.tv_usec));
#endif

p_err:
	I2C_MUTEX_UNLOCK(cis->i2c_lock);

	return ret;
}

int sensor_2ld_cis_wait_ln_mode_delay(struct v4l2_subdev *subdev)
{
	/* LN Off -> LN2 -> N+2 frame -> Stream Off */
	if (sensor_2ld_ln_mode_delay_count > 0) {
		info("%s: sensor_2ld_ln_mode_delay_count : %d ->(%d ms)\n",
			__func__, sensor_2ld_ln_mode_delay_count, 100 * sensor_2ld_ln_mode_delay_count);
		msleep(100 * sensor_2ld_ln_mode_delay_count);
	}

	return 0;
}

int sensor_2ld_cis_stream_off(struct v4l2_subdev *subdev)
{
	int ret = 0;
	struct is_cis *cis;
	struct i2c_client *client;
	cis_shared_data *cis_data;
	u8 cur_frame_count = 0;
#ifdef USE_CAMERA_SENSOR_RETENTION
	struct is_module_enum *module;
	struct is_device_sensor_peri *sensor_peri = NULL;
	struct sensor_open_extended *ext_info;
#endif

#ifdef DEBUG_SENSOR_TIME
	struct timeval st, end;

	do_gettimeofday(&st);
#endif

	WARN_ON(!subdev);

	cis = (struct is_cis *)v4l2_get_subdevdata(subdev);

	WARN_ON(!cis);
	WARN_ON(!cis->cis_data);

#ifdef USE_CAMERA_SENSOR_RETENTION
	sensor_peri = container_of(cis, struct is_device_sensor_peri, cis);
	module = sensor_peri->module;
	ext_info = &module->ext;
	WARN_ON(!ext_info);
#endif

	client = cis->client;
	if (unlikely(!client)) {
		err("client is NULL");
		ret = -EINVAL;
		goto p_err;
	}

	cis_data = cis->cis_data;

	dbg_sensor(1, "[MOD:D:%d] %s\n", cis->id, __func__);

	I2C_MUTEX_LOCK(cis->i2c_lock);
	ret = sensor_2ld_cis_group_param_hold_func(subdev, 0x00);
	if (ret < 0)
		err("group_param_hold_func failed at stream off");

	is_sensor_read8(client, 0x0005, &cur_frame_count);
	info("%s: frame_count(0x%x)\n", __func__, cur_frame_count);

#ifdef USE_CAMERA_SENSOR_RETENTION
	/* retention mode CRC check register enable */
	is_sensor_write16(cis->client, 0x6028, 0x3000);
	is_sensor_write16(cis->client, 0x602A, 0x0484);
	is_sensor_write16(cis->client, 0x6F12, 0x0100);
	is_sensor_write16(cis->client, 0x010E, 0x0100);
	if (ext_info->use_retention_mode == SENSOR_RETENTION_INACTIVE) {
		ext_info->use_retention_mode = SENSOR_RETENTION_ACTIVATED;
	}

	info("[MOD:D:%d] %s : retention enable CRC check\n", cis->id, __func__);
#endif

	is_sensor_write8(client, 0x0100, 0x00);
	cis_data->stream_on = false;

	I2C_MUTEX_UNLOCK(cis->i2c_lock);

#ifdef DEBUG_SENSOR_TIME
	do_gettimeofday(&end);
	dbg_sensor(1, "[%s] time %lu us\n", __func__, (end.tv_sec - st.tv_sec)*1000000 + (end.tv_usec - st.tv_usec));
#endif

p_err:
	return ret;
}

int sensor_2ld_cis_set_exposure_time(struct v4l2_subdev *subdev, struct ae_param *target_exposure)
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

	u16 remainder_cit = 0;

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

	sensor_2ld_target_exp_backup.short_val = target_exposure->short_val;
	sensor_2ld_target_exp_backup.long_val = target_exposure->long_val;

	if ((target_exposure->long_val <= 0) || (target_exposure->short_val <= 0)) {
		err("[%s] invalid target exposure(%d, %d)\n", __func__,
				target_exposure->long_val, target_exposure->short_val);
		ret = -EINVAL;
		goto p_err;
	}

	cis_data = cis->cis_data;

	if (cis->long_term_mode.sen_strm_off_on_enable == false) {
		switch(cis_data->sens_config_index_cur) {
#ifdef USE_CAMERA_2LD_4000X3000
		case SENSOR_2LD_2000X1500_30FPS:
		case SENSOR_2LD_2000X1128_30FPS:
#else
		case SENSOR_2LD_2016X1512_30FPS:
		case SENSOR_2LD_2016X1134_30FPS:
#endif
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

	switch (cis->cis_data->cur_lownoise_mode) {
	case IS_CIS_LNOFF:
		long_coarse_int = ((target_exposure->long_val * vt_pic_clk_freq_khz) / 1000 - min_fine_int)
												/ line_length_pck;
		if (long_coarse_int < 2) {
			long_coarse_int = 2;
		}
		remainder_cit = (long_coarse_int - 2) % 4;
		long_coarse_int -= remainder_cit;
		if (long_coarse_int < cis_data->min_coarse_integration_time) {
			dbg_sensor(1, "[MOD:D:%d] %s, vsync_cnt(%d), long coarse(%d) min(%d)\n", cis->id, __func__,
				cis_data->sen_vsync_count, long_coarse_int, cis_data->min_coarse_integration_time);
			long_coarse_int = cis_data->min_coarse_integration_time;
		}

		short_coarse_int = ((target_exposure->short_val * vt_pic_clk_freq_khz) / 1000 - min_fine_int)
												/ line_length_pck;
		if (short_coarse_int < 2) {
			short_coarse_int = 2;
		}
		remainder_cit = (short_coarse_int - 2) % 4;
		short_coarse_int -= remainder_cit;
		if (short_coarse_int < cis_data->min_coarse_integration_time) {
			dbg_sensor(1, "[MOD:D:%d] %s, vsync_cnt(%d), short coarse(%d) min(%d)\n", cis->id, __func__,
				cis_data->sen_vsync_count, short_coarse_int, cis_data->min_coarse_integration_time);
			short_coarse_int = cis_data->min_coarse_integration_time;
		}
		break;
	case IS_CIS_LN2:
		long_coarse_int = ((target_exposure->long_val * vt_pic_clk_freq_khz) / 1000 - min_fine_int)
												/ line_length_pck;
		if (long_coarse_int < 5) {
			long_coarse_int = 5;
		}
		remainder_cit = (long_coarse_int - 5) % 8;
		long_coarse_int -= remainder_cit;
		if (long_coarse_int < cis_data->min_coarse_integration_time * 2) {
			dbg_sensor(1, "[MOD:D:%d] %s, vsync_cnt(%d), long coarse(%d) min(%d)\n", cis->id, __func__,
				cis_data->sen_vsync_count, long_coarse_int, cis_data->min_coarse_integration_time * 2);
			long_coarse_int = cis_data->min_coarse_integration_time * 2;
		}
		short_coarse_int = ((target_exposure->short_val * vt_pic_clk_freq_khz) / 1000 - min_fine_int)
												/ line_length_pck;
		if (short_coarse_int < 5) {
			short_coarse_int = 5;
		}
		remainder_cit = (short_coarse_int - 5) % 8;
		short_coarse_int -= remainder_cit;
		if (short_coarse_int < cis_data->min_coarse_integration_time * 2) {
			dbg_sensor(1, "[MOD:D:%d] %s, vsync_cnt(%d), short coarse(%d) min(%d)\n", cis->id, __func__,
				cis_data->sen_vsync_count, short_coarse_int, cis_data->min_coarse_integration_time * 2);
			short_coarse_int = cis_data->min_coarse_integration_time * 2;
		}
		break;
	case IS_CIS_LN4:
		long_coarse_int = ((target_exposure->long_val * vt_pic_clk_freq_khz) / 1000 - min_fine_int)
												/ line_length_pck;
		if (long_coarse_int < 7) {
			long_coarse_int = 7;
		}
		remainder_cit = (long_coarse_int - 7) % 12;
		long_coarse_int -= remainder_cit;
		if (long_coarse_int < cis_data->min_coarse_integration_time * 4) {
			dbg_sensor(1, "[MOD:D:%d] %s, vsync_cnt(%d), long coarse(%d) min(%d)\n", cis->id, __func__,
				cis_data->sen_vsync_count, long_coarse_int, cis_data->min_coarse_integration_time * 4);
			long_coarse_int = cis_data->min_coarse_integration_time * 4;
		}
		short_coarse_int = ((target_exposure->short_val * vt_pic_clk_freq_khz) / 1000 - min_fine_int)
												/ line_length_pck;
		if (short_coarse_int < 7) {
			short_coarse_int = 7;
		}

		remainder_cit = (short_coarse_int - 7) % 12;
		short_coarse_int -= remainder_cit;
		if (short_coarse_int < cis_data->min_coarse_integration_time * 4) {
			dbg_sensor(1, "[MOD:D:%d] %s, vsync_cnt(%d), short coarse(%d) min(%d)\n", cis->id, __func__,
				cis_data->sen_vsync_count, short_coarse_int, cis_data->min_coarse_integration_time * 4);
			short_coarse_int = cis_data->min_coarse_integration_time * 4;
		}
		break;
	default:
		long_coarse_int = ((target_exposure->long_val * vt_pic_clk_freq_khz) / 1000 - min_fine_int)
												/ line_length_pck;
		if (long_coarse_int < 2) {
			long_coarse_int = 2;
		}
		remainder_cit = (long_coarse_int - 2) % 4;
		long_coarse_int -= remainder_cit;

		if (long_coarse_int < cis_data->min_coarse_integration_time) {
			dbg_sensor(1, "[MOD:D:%d] %s, vsync_cnt(%d), long coarse(%d) min(%d)\n", cis->id, __func__,
				cis_data->sen_vsync_count, long_coarse_int, cis_data->min_coarse_integration_time);
			long_coarse_int = cis_data->min_coarse_integration_time;
		}
		short_coarse_int = ((target_exposure->short_val * vt_pic_clk_freq_khz) / 1000 - min_fine_int)
												/ line_length_pck;
		if (short_coarse_int < 2) {
			short_coarse_int = 2;
		}
		remainder_cit = (short_coarse_int - 2) % 4;
		short_coarse_int -= remainder_cit;

		if (short_coarse_int < cis_data->min_coarse_integration_time) {
			dbg_sensor(1, "[MOD:D:%d] %s, vsync_cnt(%d), short coarse(%d) min(%d)\n", cis->id, __func__,
				cis_data->sen_vsync_count, short_coarse_int, cis_data->min_coarse_integration_time);
			short_coarse_int = cis_data->min_coarse_integration_time;
		}
		break;
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
	if (cis_data->stream_on == false)
		sensor_2ld_load_retention = false;

	hold = sensor_2ld_cis_group_param_hold_func(subdev, 0x01);
	if (hold < 0) {
		ret = hold;
		goto p_err_i2c_unlock;
	}

	ret |= is_sensor_write16(cis->client, 0xFCFC, 0x4000);

	/* AEB mode */
	if (cis->cis_data->cur_aeb_mode == SENSOR_AEB_MODE_ON) {
		if (NEED_UPDATE_LUT0_LUT1(cis_data)) {
			ret |= is_sensor_write16(client, AEB_2LD_LUT0 + AEB_2LD_OFFSET_CIT, long_coarse_int); // #1 short CIT
			ret |= is_sensor_write16(client, AEB_2LD_LUT0 + AEB_2LD_OFFSET_LCIT, long_coarse_int); // #1 long CIT
			ret |= is_sensor_write16(client, AEB_2LD_LUT1 + AEB_2LD_OFFSET_CIT, short_coarse_int); // #2 short CIT
			ret |= is_sensor_write16(client, AEB_2LD_LUT1 + AEB_2LD_OFFSET_LCIT, short_coarse_int); // #2 long CIT
		}
			if (NEED_UPDATE_LUT2_LUT3(cis_data)) {
			ret |= is_sensor_write16(client, AEB_2LD_LUT2 + AEB_2LD_OFFSET_CIT, long_coarse_int); // #3 short CIT
			ret |= is_sensor_write16(client, AEB_2LD_LUT2 + AEB_2LD_OFFSET_LCIT, long_coarse_int); // #3 long CIT
			ret |= is_sensor_write16(client, AEB_2LD_LUT3 + AEB_2LD_OFFSET_CIT, short_coarse_int); // #4 short CIT
			ret |= is_sensor_write16(client, AEB_2LD_LUT3 + AEB_2LD_OFFSET_LCIT, short_coarse_int); // #4 long CIT
		}

		dbg_sensor(1, "%s, vsync_cnt(%d), frame_id(%#x), fll(%#x), lcit %#x, scit %#x cit_shift %#x\n",
			__func__, cis_data->sen_vsync_count, cis_data->sen_frame_id, cis_data->frame_length_lines,
			long_coarse_int, short_coarse_int, coarse_integration_time_shifter);
	} else {
		/* WDR mode */
		if (sensor_2ld_cis_is_wdr_mode_on(cis_data)) {
			ret |= is_sensor_write16(cis->client, 0x021E, 0x0100);
		} else {
			ret |= is_sensor_write16(cis->client, 0x021E, 0x0000);
		}

		if (sensor_2ld_LTE_1s_flag) {
			ret |= is_sensor_write16(cis->client, 0x0E1A, short_coarse_int);
			ret |= is_sensor_write16(cis->client, 0x0E1E, long_coarse_int);
			if (ret < 0)
				goto p_err_i2c_unlock;
		}

		/* Short exposure */
		ret |= is_sensor_write16(client, 0x0202, short_coarse_int);
		if (ret < 0)
			goto p_err_i2c_unlock;

		/* Long exposure */
		if (sensor_2ld_cis_is_wdr_mode_on(cis_data)) {
			ret |= is_sensor_write16(client, 0x0226, long_coarse_int);
			if (ret < 0)
				goto p_err_i2c_unlock;
		}

		/* CIT shifter */
		if (cis->long_term_mode.sen_strm_off_on_enable == false) {
			ret |= is_sensor_write16(client, 0x0704, coarse_integration_time_shifter);
			if (ret < 0)
				goto p_err_i2c_unlock;
		}

		dbg_sensor(1, "[MOD:D:%d] %s, vsync_cnt(%d), vt_pic_clk_freq_khz (%d),"
			"line_length_pck(%d), min_fine_int (%d)\n",
			cis->id, __func__, cis_data->sen_vsync_count, vt_pic_clk_freq_khz/1000,
			line_length_pck, min_fine_int);
		dbg_sensor(1, "[MOD:D:%d] %s, vsync_cnt(%d), frame_length_lines(%#x),"
			"long_coarse_int %#x, short_coarse_int %#x coarse_integration_time_shifter %#x\n",
			cis->id, __func__, cis_data->sen_vsync_count, cis_data->frame_length_lines,
			long_coarse_int, short_coarse_int, coarse_integration_time_shifter);
	}

#ifdef DEBUG_SENSOR_TIME
	do_gettimeofday(&end);
	dbg_sensor(1, "[%s] time %lu us\n", __func__, (end.tv_sec - st.tv_sec)*1000000 + (end.tv_usec - st.tv_usec));
#endif

p_err_i2c_unlock:
	if (hold > 0) {
		hold = sensor_2ld_cis_group_param_hold_func(subdev, 0x00);
		if (hold < 0)
			ret = hold;
	}
	I2C_MUTEX_UNLOCK(cis->i2c_lock);

p_err:
	return ret;
}

int sensor_2ld_cis_get_min_exposure_time(struct v4l2_subdev *subdev, u32 *min_expo)
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

int sensor_2ld_cis_get_max_exposure_time(struct v4l2_subdev *subdev, u32 *max_expo)
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

int sensor_2ld_cis_adjust_frame_duration(struct v4l2_subdev *subdev,
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

//#define USE_CAMERA_SSM_TEST
#ifdef USE_CAMERA_SSM_TEST
static int record_status;
#endif

int sensor_2ld_cis_set_frame_duration(struct v4l2_subdev *subdev, u32 frame_duration)
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

#ifdef USE_CAMERA_SSM_TEST
	if (cis->cis_data->sens_config_index_cur == SENSOR_2LD_1280X720_60FPS_MODE2_SSM_960) {
		if ((cis->cis_data->sen_vsync_count % 120) == 119) {
			I2C_MUTEX_LOCK(cis->i2c_lock);
			switch (record_status) {
			case 0:
				info("%s - Manual Cue(MSB) + select 960fps(%d)", __func__, cis->cis_data->sen_vsync_count);
				is_sensor_write16(cis->client, 0x0A52, 0x0103);
				record_status++;
				break;
			case 5:
				info("%s - ssm_end_record : sen_vsync_count(%d)", __func__, cis->cis_data->sen_vsync_count);
				is_sensor_write8(cis->client, 0x0A51, 0x01);
				record_status++;
				break;
			case 10:
				record_status = 0;
				break;
			default:
				record_status++;
				break;
			}
			I2C_MUTEX_UNLOCK(cis->i2c_lock);
		}
	}
#endif

	if (cis->cis_data->sens_config_index_cur == SENSOR_2LD_2016X1134_60FPS_MODE2_SSM_960
		|| cis->cis_data->sens_config_index_cur == SENSOR_2LD_2016X1134_60FPS_MODE2_SSM_480
		|| cis->cis_data->sens_config_index_cur == SENSOR_2LD_1280X720_60FPS_MODE2_SSM_960
		|| cis->cis_data->sens_config_index_cur == SENSOR_2LD_1280X720_60FPS_MODE2_SSM_960_SDC_OFF) {
		return ret;
	}

	cis_data = cis->cis_data;

	if (cis->long_term_mode.sen_strm_off_on_enable == false) {
		if (frame_duration < cis_data->min_frame_us_time) {
			dbg_sensor(1, "frame duration is less than min(%d)\n", frame_duration);
			frame_duration = cis_data->min_frame_us_time;
		}

		I2C_MUTEX_LOCK(cis->i2c_lock);
		if (cis_data->lte_multi_capture_mode == true
			&& sensor_2ld_night_flag == false) {
			info("[%s] lte_multi_capture_mode(%d)\n", __func__, cis_data->lte_multi_capture_mode);
			sensor_2ld_night_flag = true;
			ret |= sensor_cis_set_registers(subdev, sensor_2ld_cis_night_settings_enable,
										sensor_2ld_cis_night_settings_enable_size);
		} else if (cis_data->lte_multi_capture_mode == false
			&& sensor_2ld_night_flag == true) {
			info("[%s] lte_multi_capture_mode -> restore(%d)\n", __func__, cis_data->lte_multi_capture_mode);
			sensor_2ld_night_flag = false;
			ret |= sensor_cis_set_registers(subdev, sensor_2ld_cis_night_settings_disable,
										sensor_2ld_cis_night_settings_disable_size);
		}
		I2C_MUTEX_UNLOCK(cis->i2c_lock);
	}

	sensor_2ld_frame_duration_backup = frame_duration;
	cis_data->cur_frame_us_time = frame_duration;

	if (sensor_2ld_ln_mode_delay_count > 0)
		sensor_2ld_ln_mode_delay_count--;

	if (cis->long_term_mode.sen_strm_off_on_enable == false) {
		switch(cis_data->sens_config_index_cur) {
#ifdef USE_CAMERA_2LD_4000X3000
		case SENSOR_2LD_2000X1500_30FPS:
		case SENSOR_2LD_2000X1128_30FPS:
#else
		case SENSOR_2LD_2016X1512_30FPS:
		case SENSOR_2LD_2016X1134_30FPS:
#endif
			if (frame_duration > 80000) {
				fll_shifter_idx = MIN(MAX(frame_duration / 80000, 0), 32);
				frame_length_lines_shifter = fll_shifter_array[fll_shifter_idx];
				frame_duration = frame_duration / fll_denom_array[frame_length_lines_shifter];
			} else {
				frame_length_lines_shifter = 0x0;
			}
			break;
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

	I2C_MUTEX_LOCK(cis->i2c_lock);
	if (cis_data->stream_on == false)
		sensor_2ld_load_retention = false;

	hold = sensor_2ld_cis_group_param_hold_func(subdev, 0x01);
	if (hold < 0) {
		ret = hold;
		goto p_err_i2c_unlock;
	}


	if (cis->cis_data->cur_aeb_mode != cis->cis_data->pre_aeb_mode)
		ret |= sensor_2ld_cis_set_aeb_mode_change(subdev);

	if (cis->cis_data->cur_aeb_mode == SENSOR_AEB_MODE_ON) {
		frame_duration /= 2;

		frame_length_lines = (u16)((vt_pic_clk_freq_khz * frame_duration) / (line_length_pck * 1000));
		if (NEED_UPDATE_LUT0_LUT1(cis_data)) {
			ret |= is_sensor_write16(client, AEB_2LD_LUT0 + AEB_2LD_OFFSET_FLL, frame_length_lines); // #1 FLL
			ret |= is_sensor_write16(client, AEB_2LD_LUT1 + AEB_2LD_OFFSET_FLL, frame_length_lines); // #2 FLL
		}
		if (NEED_UPDATE_LUT2_LUT3(cis_data)) {
			ret |= is_sensor_write16(client, AEB_2LD_LUT2 + AEB_2LD_OFFSET_FLL, frame_length_lines); // #3 FLL
			ret |= is_sensor_write16(client, AEB_2LD_LUT3 + AEB_2LD_OFFSET_FLL, frame_length_lines); // #4 FLL
		}

		dbg_sensor(1, "%s, vsync_cnt(%d), frame_id(%#x), frame_duration = %d us, fll %#x\n",
			__func__, cis_data->sen_vsync_count, cis_data->sen_frame_id, frame_duration, frame_length_lines);
	} else {
		if (cis->cis_data->cur_lownoise_mode != cis->cis_data->pre_lownoise_mode)
			ret |= sensor_2ld_cis_set_lownoise_mode_change(subdev);

		if (sensor_2ld_LTE_1s_flag) {
			ret |= is_sensor_write16(client, 0x0E22, frame_length_lines);
			if (ret < 0)
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

		dbg_sensor(1, "[MOD:D:%d] %s, vt_pic_clk_freq_khz(%#x) frame_duration = %d us,"
				"(line_length_pck%#x), frame_length_lines(%#x), frame_length_lines_shifter(%#x)\n",
				cis->id, __func__, vt_pic_clk_freq_khz/1000, frame_duration,
				line_length_pck, frame_length_lines, frame_length_lines_shifter);
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
		hold = sensor_2ld_cis_group_param_hold_func(subdev, 0x00);
		if (hold < 0)
			ret = hold;
	}
	I2C_MUTEX_UNLOCK(cis->i2c_lock);

p_err:
	return ret;
}

int sensor_2ld_cis_set_frame_rate(struct v4l2_subdev *subdev, u32 min_fps)
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

	ret = sensor_2ld_cis_set_frame_duration(subdev, frame_duration);
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

int sensor_2ld_cis_adjust_analog_gain(struct v4l2_subdev *subdev, u32 input_again, u32 *target_permile)
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

int sensor_2ld_cis_set_analog_gain(struct v4l2_subdev *subdev, struct ae_param *again)
{
	int ret = 0;
	int hold = 0;
	struct is_cis *cis;
	struct i2c_client *client;
	cis_shared_data *cis_data;

	u16 analog_gain = 0;

#ifdef DEBUG_SENSOR_TIME
	struct timeval st, end;

	do_gettimeofday(&st);
#endif

	WARN_ON(!subdev);
	WARN_ON(!again);

	cis = (struct is_cis *)v4l2_get_subdevdata(subdev);

	WARN_ON(!cis);
	WARN_ON(!cis->cis_data);

	cis_data = cis->cis_data;
	client = cis->client;
	if (unlikely(!client)) {
		err("client is NULL");
		ret = -EINVAL;
		goto p_err;
	}

	sensor_2ld_again_backup.short_val = again->short_val;
	sensor_2ld_again_backup.long_val = again->long_val;

	analog_gain = (u16)sensor_cis_calc_again_code(again->val);

	if (analog_gain < cis_data->min_analog_gain[0]) {
		info("[%s] not proper analog_gain value, reset to min_analog_gain\n", __func__);
		analog_gain = cis_data->min_analog_gain[0];
	}

	if (analog_gain > cis_data->max_analog_gain[0]) {
		info("[%s] not proper analog_gain value, reset to max_analog_gain\n", __func__);
		analog_gain = cis_data->max_analog_gain[0];
	}

	I2C_MUTEX_LOCK(cis->i2c_lock);
	if (cis_data->stream_on == false)
		sensor_2ld_load_retention = false;

	hold = sensor_2ld_cis_group_param_hold_func(subdev, 0x01);
	if (hold < 0) {
		ret = hold;
		goto p_err_i2c_unlock;
	}

	if (cis->cis_data->cur_aeb_mode == SENSOR_AEB_MODE_ON) {
		if (NEED_UPDATE_LUT0_LUT1(cis_data)) {
			ret |= is_sensor_write16(client, AEB_2LD_LUT0 + AEB_2LD_OFFSET_AGAIN, analog_gain); // #1 Again
			ret |= is_sensor_write16(client, AEB_2LD_LUT1 + AEB_2LD_OFFSET_AGAIN, analog_gain); // #2 Again

		}
		if (NEED_UPDATE_LUT2_LUT3(cis_data)) {
			ret |= is_sensor_write16(client, AEB_2LD_LUT2 + AEB_2LD_OFFSET_AGAIN, analog_gain); // #3 Again
			ret |= is_sensor_write16(client, AEB_2LD_LUT3 + AEB_2LD_OFFSET_AGAIN, analog_gain); // #4 Again
		}

		dbg_sensor(1, "%s, vsync_cnt(%d), frame_id(%#x), input_again = %d us, analog_gain(%#x)\n",
			__func__, cis_data->sen_vsync_count, cis_data->sen_frame_id, again->val, analog_gain);
	} else {
		if (sensor_2ld_LTE_1s_flag) {
			ret |= is_sensor_write16(client, 0x0E1C, analog_gain);
			if (ret < 0)
				goto p_err_i2c_unlock;
		}

		ret |= is_sensor_write16(client, 0x0204, analog_gain);
		if (ret < 0)
			goto p_err_i2c_unlock;

		dbg_sensor(1, "[MOD:D:%d] %s(vsync cnt = %d), input_again = %d us, analog_gain(%#x)\n",
			cis->id, __func__, cis_data->sen_vsync_count, again->val, analog_gain);
	}

#ifdef DEBUG_SENSOR_TIME
	do_gettimeofday(&end);
	dbg_sensor(1, "[%s] time %lu us\n", __func__, (end.tv_sec - st.tv_sec)*1000000 + (end.tv_usec - st.tv_usec));
#endif

p_err_i2c_unlock:
	if (hold > 0) {
		hold = sensor_2ld_cis_group_param_hold_func(subdev, 0x00);
		if (hold < 0)
			ret = hold;
	}
	I2C_MUTEX_UNLOCK(cis->i2c_lock);

p_err:
	return ret;
}

int sensor_2ld_cis_get_analog_gain(struct v4l2_subdev *subdev, u32 *again)
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
	hold = sensor_2ld_cis_group_param_hold_func(subdev, 0x01);
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
		hold = sensor_2ld_cis_group_param_hold_func(subdev, 0x00);
		if (hold < 0)
			ret = hold;
	}
	I2C_MUTEX_UNLOCK(cis->i2c_lock);

p_err:
	return ret;
}

int sensor_2ld_cis_get_min_analog_gain(struct v4l2_subdev *subdev, u32 *min_again)
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

int sensor_2ld_cis_get_max_analog_gain(struct v4l2_subdev *subdev, u32 *max_again)
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

int sensor_2ld_cis_set_digital_gain(struct v4l2_subdev *subdev, struct ae_param *dgain)
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

	sensor_2ld_dgain_backup.short_val = dgain->short_val;
	sensor_2ld_dgain_backup.long_val = dgain->long_val;

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

	I2C_MUTEX_LOCK(cis->i2c_lock);
	if (cis_data->stream_on == false)
		sensor_2ld_load_retention = false;

	hold = sensor_2ld_cis_group_param_hold_func(subdev, 0x01);
	if (hold < 0) {
		ret = hold;
		goto p_err_i2c_unlock;
	}

	if (cis->cis_data->cur_aeb_mode == SENSOR_AEB_MODE_ON) {
		if (NEED_UPDATE_LUT0_LUT1(cis_data)) {
			ret |= is_sensor_write16(client, AEB_2LD_LUT0 + AEB_2LD_OFFSET_DGAIN, long_gain); // #1 Dgain
			ret |= is_sensor_write16(client, AEB_2LD_LUT0 + AEB_2LD_OFFSET_LDGAIN, long_gain); // #1 long Dgain

			ret |= is_sensor_write16(client, AEB_2LD_LUT1 + AEB_2LD_OFFSET_DGAIN, short_gain); // #2 Dgain
			ret |= is_sensor_write16(client, AEB_2LD_LUT1 + AEB_2LD_OFFSET_LDGAIN, short_gain); // #2 long Dgain
		}
		if (NEED_UPDATE_LUT2_LUT3(cis_data)) {
			ret |= is_sensor_write16(client, AEB_2LD_LUT2 + AEB_2LD_OFFSET_DGAIN, long_gain); // #3 Dgain
			ret |= is_sensor_write16(client, AEB_2LD_LUT2 + AEB_2LD_OFFSET_LDGAIN, long_gain); // #3 long Dgain

			ret |= is_sensor_write16(client, AEB_2LD_LUT3 + AEB_2LD_OFFSET_DGAIN, short_gain); // #4 Dgain
			ret |= is_sensor_write16(client, AEB_2LD_LUT3 + AEB_2LD_OFFSET_LDGAIN, short_gain); // #4 long Dgain
		}
		dbg_sensor(1, "%s, vsync_cnt(%d), frame_id(%#x), input_dgain = %d/%d us, long_gain(%#x), short_gain(%#x)\n",
			__func__, cis_data->sen_vsync_count, cis_data->sen_frame_id, dgain->long_val, dgain->short_val, long_gain, short_gain);
	} else {
		if (sensor_2ld_LTE_1s_flag) {
			ret |= is_sensor_write16(client, 0x0E20, short_gain);
			if (ret < 0)
				goto p_err_i2c_unlock;
		}

		/* Short digital gain */
		ret |= is_sensor_write16(client, 0x020E, short_gain);
		if (ret < 0)
			goto p_err_i2c_unlock;

		/* Long digital gain */
		if (sensor_2ld_cis_is_wdr_mode_on(cis_data)) {
			ret |= is_sensor_write16(client, 0x0230, long_gain);
			if (ret < 0)
				goto p_err_i2c_unlock;
		}

		dbg_sensor(1, "[MOD:D:%d] %s(vsync cnt = %d), input_dgain = %d/%d us,"
				"long_gain(%#x), short_gain(%#x)\n",
				cis->id, __func__, cis->cis_data->sen_vsync_count,
				dgain->long_val, dgain->short_val, long_gain, short_gain);
	}

#ifdef DEBUG_SENSOR_TIME
	do_gettimeofday(&end);
	dbg_sensor(1, "[%s] time %lu us\n", __func__, (end.tv_sec - st.tv_sec)*1000000 + (end.tv_usec - st.tv_usec));
#endif

p_err_i2c_unlock:
	if (hold > 0) {
		hold = sensor_2ld_cis_group_param_hold_func(subdev, 0x00);
		if (hold < 0)
			ret = hold;
	}
	I2C_MUTEX_UNLOCK(cis->i2c_lock);

p_err:
	return ret;
}

int sensor_2ld_cis_get_digital_gain(struct v4l2_subdev *subdev, u32 *dgain)
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
	hold = sensor_2ld_cis_group_param_hold_func(subdev, 0x01);
	if (hold < 0) {
		ret = hold;
		goto p_err_i2c_unlock;
	}

	/*
	 * NOTE : In S5K2LD, digital gain is long/short seperated, should set 2 registers like below,
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
		hold = sensor_2ld_cis_group_param_hold_func(subdev, 0x00);
		if (hold < 0)
			ret = hold;
	}
	I2C_MUTEX_UNLOCK(cis->i2c_lock);

p_err:
	return ret;
}

int sensor_2ld_cis_get_min_digital_gain(struct v4l2_subdev *subdev, u32 *min_dgain)
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

int sensor_2ld_cis_get_max_digital_gain(struct v4l2_subdev *subdev, u32 *max_dgain)
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

int sensor_2ld_cis_long_term_exposure(struct v4l2_subdev *subdev)
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
		dbg_sensor(1, "%s ****************************************************************\n", __func__);
		dbg_sensor(1, "%s enable(%d)\n", __func__, lte_mode->sen_strm_off_on_enable);
		dbg_sensor(1, "%s expo[0](%d) expo[1](%d)\n", __func__, lte_mode->expo[0], lte_mode->expo[1]);
		dbg_sensor(1, "%s tgain[0](%d) tgain[1](%d)\n", __func__, lte_mode->tgain[0], lte_mode->tgain[1]);
		dbg_sensor(1, "%s again[0](%d) again[1](%d)\n", __func__, lte_mode->again[0], lte_mode->again[1]);
		dbg_sensor(1, "%s dgain[0](%d) dgain[1](%d)\n", __func__, lte_mode->dgain[0], lte_mode->dgain[1]);
		dbg_sensor(1, "%s frame_interval(%d)\n", __func__, lte_mode->frame_interval);
		dbg_sensor(1, "%s ****************************************************************\n", __func__);

		if (lte_mode->expo[0] >= 1000000) {
			info("%s sensor LTE special setting", __func__);
			sensor_2ld_LTE_1s_flag = true;
			ret |= sensor_cis_set_registers(subdev, sensor_2ld_cis_LTE_settings_1,sensor_2ld_cis_LTE_settings_1_size);
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
			info("%s lte_mode->expo[0](%d), shift_count(%d)", __func__, lte_mode->expo[0], shift_count);
			ret |= is_sensor_write16(cis->client, 0xBCA2, (shift_count << 8) | shift_count); // LTE FLL Shifter (1byte), CIT Shifter (1byte)
			ret |= is_sensor_write16(cis->client, 0xBCA4, (shift_count << 8) | 0x9E); // LTE Long CIT Shifter (1byte)
			ret |= sensor_cis_set_registers(subdev, sensor_2ld_cis_LTE_settings_2,sensor_2ld_cis_LTE_settings_2_size);
		} else if (lte_mode->expo[0] > 125000) {
			if(sensor_2ld_LTE_1s_flag) {
				info("%s senosr LTE special setting -> restore", __func__);
				sensor_2ld_LTE_1s_flag = false;
				ret |= sensor_cis_set_registers(subdev, sensor_2ld_cis_LTE_settings_3,sensor_2ld_cis_LTE_settings_3_size);
			}
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
		if(sensor_2ld_LTE_1s_flag) {
			info("%s senosr LTE special setting -> preview", __func__);
			sensor_2ld_LTE_1s_flag = false;
			ret |= sensor_cis_set_registers(subdev, sensor_2ld_cis_LTE_settings_3,sensor_2ld_cis_LTE_settings_3_size);
		}
	}

	I2C_MUTEX_UNLOCK(cis->i2c_lock);

	info("%s enable(%d)", __func__, lte_mode->sen_strm_off_on_enable);

	if (ret < 0) {
		pr_err("ERR[%s]: LTE register setting fail\n", __func__);
		return ret;
	}

	return ret;
}

int sensor_2ld_cis_set_frs_control(struct v4l2_subdev *subdev, u32 command)
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
	case FRS_SSM_START:
		pr_info("[%s] SUPER_SLOW_MOTION_START\n", __func__);
		ret |= is_sensor_write8(cis->client, 0x0A52, 0x01); /* start_user_record */
		/* ret |= is_sensor_write8(cis->client, 0x0A51, 0x01); *//* enable_preview_during_recording */
		/* ret |= is_sensor_write8(cis->client, 0x0A55, 0x08); *//* tg_to_oif_ratio */
		/* ret |= is_sensor_write8(cis->client, 0x0A56, 0x02); *//* tg_to_sg_ratio */
		/* ret |= is_sensor_write16(cis->client, 0x0A58, 0x0010); *//* q_mask_frames */
		/* ret |= is_sensor_write16(cis->client, 0x0A5A, 0x0010); *//* before_q_frames */
		/* ret |= is_sensor_write16(cis->client, 0x0A60, 0x0050); *//* dram_frame_num */
		break;
	case FRS_SSM_MANUAL_CUE_ENABLE:
		pr_info("[%s] SUPER_SLOW_MOTION_START_MANUAL_CUE\n", __func__);
		ret |= is_sensor_write8(cis->client, 0x0A54, 0x01); /* Manual Q Enable */
		break;
	case FRS_SSM_STOP:
		pr_info("[%s] SUPER_SLOW_MOTION_STOP\n", __func__);
		ret |= is_sensor_write8(cis->client, 0x0A51, 0x01); /* stop_user_record */
		break;
	case FRS_SSM_MODE_AUTO_MANUAL_CUE_16:
		pr_info("[%s] SUPER_SLOW_MOTION_MODE_AUTO_MANUAL_CUE_16\n", __func__);
		ret |= is_sensor_write8(cis->client, 0x0A50, 0x02);    /* Enable Manual /Auto Q */
		ret |= is_sensor_write16(cis->client, 0x0A5A, 0x0010); /* before_q_frames = 16 */
		break;
	case FRS_SSM_MODE_AUTO_MANUAL_CUE_32:
		pr_info("[%s] SUPER_SLOW_MOTION_MODE_AUTO_MANUAL_CUE_32\n", __func__);
		ret |= is_sensor_write8(cis->client, 0x0A50, 0x02);    /* Enable Manual /Auto Q */
		ret |= is_sensor_write16(cis->client, 0x0A5A, 0x0020); /* before_q_frames = 32 */
		break;
	case FRS_SSM_MODE_AUTO_MANUAL_CUE_48:
		pr_info("[%s] SUPER_SLOW_MOTION_MODE_AUTO_MANUAL_CUE_48\n", __func__);
		ret |= is_sensor_write8(cis->client, 0x0A50, 0x02);    /* Enable Manual /Auto Q */
		ret |= is_sensor_write16(cis->client, 0x0A5A, 0x0030); /* before_q_frames = 48 */
		break;
	case FRS_SSM_MODE_AUTO_MANUAL_CUE_64:
		pr_info("[%s] SUPER_SLOW_MOTION_MODE_AUTO_MANUAL_CUE_64\n", __func__);
		ret |= is_sensor_write8(cis->client, 0x0A50, 0x02);    /* Enable Manual /Auto Q */
		ret |= is_sensor_write16(cis->client, 0x0A5A, 0x0040); /* before_q_frames = 64 */
		break;
	case FRS_SSM_MODE_ONLY_MANUAL_CUE:
		ret |= is_sensor_write8(cis->client, 0x0A50, 0x01);    /* Enable Manual Q Only */
		ret |= is_sensor_write16(cis->client, 0x0A58, 0x0000); /* q_mask_frames */
		ret |= is_sensor_write16(cis->client, 0x0A5A, 0x0000); /* before_q_frames = 0 */
		break;
	case FRS_SSM_MODE_FACTORY_TEST:
		pr_info("[%s] SUPER_SLOW_MOTION_MODE_FACTORY_TEST\n", __func__);
		ext_info->use_retention_mode = SENSOR_RETENTION_INACTIVE;
		break;
	case FRS_SSM_MODE_FLICKER_DETECT_OFF:
		pr_info("[%s] FRS_SSM_MODE_FLICKER_DETECT_OFF\n", __func__);
		ret |= is_sensor_write16(cis->client, 0xFCFC, 0x2000);
		ret |= is_sensor_write8(cis->client, 0xC26E, 0x01); /* 0 : on, 1 : bypass */
		ret |= is_sensor_write16(cis->client, 0xFCFC, 0x4000);
		break;
	case FRS_SSM_MODE_FLICKER_DETECT_ENABLE:
		pr_info("[%s] FRS_SSM_MODE_FLICKER_DETECT_ON\n", __func__);
		ret |= is_sensor_write16(cis->client, 0xFCFC, 0x2000);
		ret |= is_sensor_write8(cis->client, 0xC26E, 0x00); /* 0 : on, 1 : bypass */
		ret |= is_sensor_write16(cis->client, 0xFCFC, 0x4000);
		break;
	case FRS_SSM_MODE_FPS_960:
		pr_info("[%s] SUPER_SLOW_MOTION_MODE__FPS_960\n", __func__);
		ret |= is_sensor_write8(cis->client, 0x0A53, 0x02); /* ssm fps = 960 */
		break;
	case FRS_SSM_MODE_FPS_480:
		pr_info("[%s] SUPER_SLOW_MOTION_MODE__FPS_480\n", __func__);
		ret |= is_sensor_write8(cis->client, 0x0A53, 0x01); /* ssm fps = 480 */
		break;
	case FRS_SSM_MANUAL_MODE_START:
		pr_info("[%s] SUPER_SLOW_MOTION_MANUAL_MODE_START\n", __func__);
		ret |= is_sensor_write8(cis->client, 0x0A52, 0x01); /* Start Manual Q Only */
		break;
	case FRS_SSM_AUTO_MODE_START:
		pr_info("[%s] SUPER_SLOW_MOTION_AUTO_MODE_START\n", __func__);
		ret |= is_sensor_write8(cis->client, 0x0A50, 0x01); /* Start Auto Q Only */
		break;
	case FRS_SSM_MODE_MITIGATION_ENABLE:
		pr_info("[%s] SUPER_SLOW_MOTION_MODE_MITIGATION_ENABLE\n", __func__);
		ret |= is_sensor_write8(cis->client, 0x0A50, 0x02); /* burst output w/o MD/GMC */
		break;
	default:
		pr_info("[%s] not support command(%d)\n", __func__, command);
	}

	if (ret < 0) {
		pr_err("ERR[%s]: super slow control setting fail\n", __func__);
		return ret;
	}

	return ret;
}

int sensor_2ld_cis_set_super_slow_motion_roi(struct v4l2_subdev *subdev, struct v4l2_rect *ssm_roi)
{
	int ret = 0;
	struct is_cis *cis = NULL;

	WARN_ON(!subdev);

	cis = (struct is_cis *)v4l2_get_subdevdata(subdev);
	WARN_ON(!cis);
	WARN_ON(!cis->cis_data);

	pr_info("[%s] : left(%d), width(%d), top(%d), height(%d)\n", __func__,
		ssm_roi->left, ssm_roi->width, ssm_roi->top, ssm_roi->height);

	ret |= is_sensor_write16(cis->client, 0x0A62, ssm_roi->left);
	ret |= is_sensor_write16(cis->client, 0x0A64, ssm_roi->width);
	ret |= is_sensor_write16(cis->client, 0x0A66, ssm_roi->top);
	ret |= is_sensor_write16(cis->client, 0x0A68, ssm_roi->height);
	if (ret < 0) {
		pr_err("ERR[%s]: super slow roi setting fail\n", __func__);
		return ret;
	}

	msleep(17); // sensor limitation : start cue needs delay margin

	return ret;
}

int sensor_2ld_cis_set_super_slow_motion_setting(struct v4l2_subdev *subdev, struct v4l2_rect *setting)
{
	int ret = 0;
	struct is_cis *cis = NULL;

	WARN_ON(!subdev);

	cis = (struct is_cis *)v4l2_get_subdevdata(subdev);
	WARN_ON(!cis);
	WARN_ON(!cis->cis_data);

	if (setting->left == 0x2000 || setting->left == 0x2001) {
		if (setting->height == 0x2) {
			ret |= is_sensor_write16(cis->client, 0x6028, setting->left);
			ret |= is_sensor_write16(cis->client, 0x602A, setting->width);
			ret |= is_sensor_write16(cis->client, 0x6F12, setting->top);
			pr_info("[%s] : 0x2000 2bytes  page(%x), addr(%x), value(%x), bytes(%x)\n", __func__,
				setting->left, setting->width, setting->top, setting->height);
		} else {
			ret |= is_sensor_write16(cis->client, 0x6028, setting->left);
			ret |= is_sensor_write16(cis->client, 0x602A, setting->width);
			ret |= is_sensor_write8(cis->client, 0x6F12, setting->top);
			pr_info("[%s] :0x2000 1bytes  page(%x), addr(%x), value(%x), bytes(%x)\n", __func__,
				setting->left, setting->width, setting->top, setting->height);
		}
		ret |= is_sensor_write16(cis->client, 0xFCFC, 0x4000);
	} else {
		ret |= is_sensor_write16(cis->client, 0xFCFC, 0x4000);
		if (setting->height == 0x2) {
			ret |= is_sensor_write16(cis->client, setting->width, setting->top);
			pr_info("[%s] : 0x4000 2bytes  page(%x), addr(%x), value(%x), bytes(%x)\n", __func__,
				setting->left, setting->width, setting->top, setting->height);
		} else {
			ret |= is_sensor_write8(cis->client, setting->width, setting->top);
			pr_info("[%s] :0x4000 1bytes  page(%x), addr(%x), value(%x), bytes(%x)\n", __func__,
				setting->left, setting->width, setting->top, setting->height);
		}
	}

	if (ret < 0) {
		pr_err("ERR[%s]: super slow roi setting fail\n", __func__);
		return ret;
	}

	return ret;
}

int sensor_2ld_cis_set_super_slow_motion_threshold(struct v4l2_subdev *subdev, u32 threshold)
{
	int ret = 0;
	struct is_cis *cis = NULL;
	u8 final_threshold = (u8)threshold;

	WARN_ON(!subdev);

	cis = (struct is_cis *)v4l2_get_subdevdata(subdev);
	WARN_ON(!cis);
	WARN_ON(!cis->cis_data);

	ret |= is_sensor_write16(cis->client, 0x6028, 0x2001);
	ret |= is_sensor_write16(cis->client, 0x602A, 0x2CC0);
	ret |= is_sensor_write16(cis->client, 0x6F12, final_threshold);
	ret |= is_sensor_write16(cis->client, 0xFCFC, 0x4000);
	if (ret < 0) {
		pr_err("ERR[%s]: super slow threshold setting fail\n", __func__);
	}

	pr_info("[%s] : super slow threshold(%d)\n", __func__, threshold);

	return ret;
}

int sensor_2ld_cis_get_super_slow_motion_threshold(struct v4l2_subdev *subdev, u32 *threshold)
{
	int ret = 0;
	struct is_cis *cis = NULL;
	u8 final_threshold = 0;

	WARN_ON(!subdev);

	cis = (struct is_cis *)v4l2_get_subdevdata(subdev);
	WARN_ON(!cis);
	WARN_ON(!cis->cis_data);

	ret |= is_sensor_write16(cis->client, 0x602C, 0x2000);
	ret |= is_sensor_write16(cis->client, 0x602E, 0xFF75);
	ret |= is_sensor_read8(cis->client, 0x6F12, &final_threshold);
	ret |= is_sensor_write16(cis->client, 0xFCFC, 0x4000);
	if (ret < 0) {
		pr_err("ERR[%s]: super slow threshold getting fail\n", __func__);
		*threshold = 0;
		return ret;
	}

	*threshold = final_threshold;

	pr_info("[%s] : super slow threshold(%d)\n", __func__, *threshold);

	return ret;
}

int sensor_2ld_cis_get_super_slow_motion_gmc(struct v4l2_subdev *subdev, u32 *gmc)
{
	int ret = 0;
	struct is_cis *cis = NULL;
	u8 gmc_state = 0;

	WARN_ON(!subdev);

	cis = (struct is_cis *)v4l2_get_subdevdata(subdev);
	WARN_ON(!cis);
	WARN_ON(!cis->cis_data);

	ret |= is_sensor_write16(cis->client, 0x602C, 0x2000);
	ret |= is_sensor_write16(cis->client, 0x602E, 0x0E484);
	ret |= is_sensor_read8(cis->client, 0x6F12, &gmc_state);
	ret |= is_sensor_write16(cis->client, 0xFCFC, 0x4000);
	if (ret < 0) {
		pr_err("ERR[%s]: super slow gmc getting fail\n", __func__);
		*gmc = 0;
		return ret;
	}

	*gmc = gmc_state;

	pr_info("[%s] : super slow gmc(%d)\n", __func__, *gmc);

	return ret;
}

int sensor_2ld_cis_get_super_slow_motion_frame_id(struct v4l2_subdev *subdev, u32 *frameid)
{
	int ret = 0;
	struct is_cis *cis = NULL;
	u8 frame_id = 0;

	WARN_ON(!subdev);

	cis = (struct is_cis *)v4l2_get_subdevdata(subdev);
	WARN_ON(!cis);
	WARN_ON(!cis->cis_data);

	ret |= is_sensor_read8(cis->client, 0x000A, &frame_id);
	if (ret < 0) {
		pr_err("ERR[%s]: super slow frame id getting fail\n", __func__);
		*frameid = 0;
		return ret;
	}

	*frameid = frame_id;

	pr_info("[%s] : super slow frame_id(%d)\n", __func__, *frameid);

	return ret;
}

int sensor_2ld_cis_set_super_slow_motion_flicker(struct v4l2_subdev *subdev, u32 flicker)
{
	int ret = 0;
	struct is_cis *cis = NULL;
	u8 final_flicker = (u8)flicker;

	WARN_ON(!subdev);

	cis = (struct is_cis *)v4l2_get_subdevdata(subdev);
	WARN_ON(!cis);
	WARN_ON(!cis->cis_data);

	ret |= is_sensor_write8(cis->client, 0x0C21, final_flicker);
	if (ret < 0) {
		pr_err("ERR[%s]: super slow flicker setting fail\n", __func__);
	}

	pr_info("[%s] : super slow flicker(%d)\n", __func__, flicker);

	return ret;
}

int sensor_2ld_cis_get_super_slow_motion_flicker(struct v4l2_subdev *subdev, u32 *flicker)
{
	int ret = 0;
	struct is_cis *cis = NULL;
	u8 final_flicker = 0;

	WARN_ON(!subdev);

	cis = (struct is_cis *)v4l2_get_subdevdata(subdev);
	WARN_ON(!cis);
	WARN_ON(!cis->cis_data);

	ret |= is_sensor_read8(cis->client, 0x1982, &final_flicker);
	if (ret < 0) {
		pr_err("ERR[%s]: super slow threshold getting fail\n", __func__);
		*flicker = 0;
		return ret;
	}

	*flicker = final_flicker;

	pr_info("[%s] : super slow flicker(%d)\n", __func__, *flicker);

	return ret;
}

int sensor_2ld_cis_get_super_slow_motion_md_threshold(struct v4l2_subdev *subdev, u32 *threshold)
{
	int ret = 0;
	struct is_cis *cis = NULL;
	u16 md_threshold = 0;

	WARN_ON(!subdev);

	cis = (struct is_cis *)v4l2_get_subdevdata(subdev);
	WARN_ON(!cis);
	WARN_ON(!cis->cis_data);

	ret |= is_sensor_read16(cis->client, 0xDB0E, &md_threshold);
	if (ret < 0) {
		pr_err("ERR[%s]: super slow threshold getting fail\n", __func__);
		*threshold = 0;
		return ret;
	}

	*threshold = md_threshold;

	pr_info("[%s] : super slow threshold(%d)\n", __func__, *threshold);

	return ret;
}

int sensor_2ld_cis_set_super_slow_motion_gmc_table_idx(struct v4l2_subdev *subdev, u32 index)
{
	int ret = 0;
	struct is_cis *cis = NULL;
	u16 table_idx = (u16)index;

	WARN_ON(!subdev);

	cis = (struct is_cis *)v4l2_get_subdevdata(subdev);
	WARN_ON(!cis);
	WARN_ON(!cis->cis_data);

	ret |= is_sensor_write16(cis->client, 0x6028, 0x2001);
	ret |= is_sensor_write16(cis->client, 0x602A, 0x3D14);
	ret |= is_sensor_write16(cis->client, 0x6F12, table_idx);
	ret |= is_sensor_write16(cis->client, 0xFCFC, 0x4000);
	if (ret < 0) {
		pr_err("ERR[%s]: super slow gmc table idx setting fail\n", __func__);
	}

	pr_info("[%s] : super slow table_idx(%d)\n", __func__, table_idx);

	return ret;
}

int sensor_2ld_cis_set_super_slow_motion_gmc_block_with_md_low(struct v4l2_subdev *subdev, u32 block)
{
	int ret = 0;
	struct is_cis *cis = NULL;
	u16 gmc_block = (u16)block;

	WARN_ON(!subdev);

	cis = (struct is_cis *)v4l2_get_subdevdata(subdev);
	WARN_ON(!cis);
	WARN_ON(!cis->cis_data);

	ret |= is_sensor_write16(cis->client, 0x6028, 0x2001);
	ret |= is_sensor_write16(cis->client, 0x602A, 0x3D68);
	ret |= is_sensor_write16(cis->client, 0x6F12, gmc_block);
	ret |= is_sensor_write16(cis->client, 0xFCFC, 0x4000);
	if (ret < 0) {
		pr_err("ERR[%s]: super slow gmc block with md low setting fail\n", __func__);
	}

	pr_info("[%s] : super slow gmc_block(%d)\n", __func__, gmc_block);

	return ret;
}

int sensor_2ld_cis_set_factory_control(struct v4l2_subdev *subdev, u32 command)
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
		ext_info->use_retention_mode = SENSOR_RETENTION_INACTIVE;
#endif
		break;
	default:
		pr_info("[%s] not support command(%d)\n", __func__, command);
	}

	return ret;
}

int sensor_2ld_cis_compensate_gain_for_extremely_br(struct v4l2_subdev *subdev, u32 expo, u32 *again, u32 *dgain)
{
	int ret = 0;
	struct is_cis *cis;
	cis_shared_data *cis_data;

	u64 vt_pic_clk_freq_khz = 0;
	u32 line_length_pck = 0;
	u32 min_fine_int = 0;
	u32 coarse_int = 0;
	u32 compensated_again = 0;
	u32 remainder_cit = 0;

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

	switch (cis->cis_data->cur_lownoise_mode) {
	case IS_CIS_LNOFF:
		coarse_int = ((expo * vt_pic_clk_freq_khz) / 1000 - min_fine_int) / line_length_pck;
		if (coarse_int < 2) {
			coarse_int = 2;
		}
		remainder_cit = (coarse_int - 2) % 4;
		coarse_int -= remainder_cit;
		if (coarse_int < cis_data->min_coarse_integration_time) {
			dbg_sensor(1, "[MOD:D:%d] %s, vsync_cnt(%d), long coarse(%d) min(%d)\n", cis->id, __func__,
				cis_data->sen_vsync_count, coarse_int, cis_data->min_coarse_integration_time);
			coarse_int = cis_data->min_coarse_integration_time;
		}
		break;
	case IS_CIS_LN2:
		coarse_int = ((expo * vt_pic_clk_freq_khz) / 1000 - min_fine_int) / line_length_pck;
		if (coarse_int < 5) {
			coarse_int = 5;
		}
		remainder_cit = (coarse_int - 5) % 8;
		coarse_int -= remainder_cit;
		if (coarse_int < cis_data->min_coarse_integration_time * 2) {
			dbg_sensor(1, "[MOD:D:%d] %s, vsync_cnt(%d), long coarse(%d) min(%d)\n", cis->id, __func__,
				cis_data->sen_vsync_count, coarse_int, cis_data->min_coarse_integration_time * 2);
			coarse_int = cis_data->min_coarse_integration_time * 2;
		}
		break;
	case IS_CIS_LN4:
		coarse_int = ((expo * vt_pic_clk_freq_khz) / 1000 - min_fine_int) / line_length_pck;
		if (coarse_int < 7) {
			coarse_int = 7;
		}
		remainder_cit = (coarse_int - 7) % 12;
		coarse_int -= remainder_cit;
		if (coarse_int < cis_data->min_coarse_integration_time * 4) {
			dbg_sensor(1, "[MOD:D:%d] %s, vsync_cnt(%d), long coarse(%d) min(%d)\n", cis->id, __func__,
				cis_data->sen_vsync_count, coarse_int, cis_data->min_coarse_integration_time * 4);
			coarse_int = cis_data->min_coarse_integration_time * 4;
		}
		break;
	default:
		coarse_int = ((expo * vt_pic_clk_freq_khz) / 1000 - min_fine_int) / line_length_pck;
		if (coarse_int < 2) {
			coarse_int = 2;
		}
		remainder_cit = (coarse_int - 2) % 4;
		coarse_int -= remainder_cit;
		if (coarse_int < cis_data->min_coarse_integration_time) {
			dbg_sensor(1, "[MOD:D:%d] %s, vsync_cnt(%d), long coarse(%d) min(%d)\n", cis->id, __func__,
				cis_data->sen_vsync_count, coarse_int, cis_data->min_coarse_integration_time);
			coarse_int = cis_data->min_coarse_integration_time;
		}
		break;
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

int sensor_2ld_cis_recover_stream_on(struct v4l2_subdev *subdev)
{
	int ret = 0;
	struct is_cis *cis = NULL;
#ifdef USE_CAMERA_SENSOR_RETENTION
	struct is_module_enum *module;
	struct is_device_sensor_peri *sensor_peri = NULL;
	struct sensor_open_extended *ext_info;
#endif

	FIMC_BUG(!subdev);

	cis = (struct is_cis *)v4l2_get_subdevdata(subdev);
	FIMC_BUG(!cis);
	FIMC_BUG(!cis->cis_data);

#ifdef USE_CAMERA_SENSOR_RETENTION
	sensor_peri = container_of(cis, struct is_device_sensor_peri, cis);
	module = sensor_peri->module;
	ext_info = &module->ext;
	FIMC_BUG(!ext_info);

	ext_info->use_retention_mode = SENSOR_RETENTION_INACTIVE;
#endif

	info("%s start\n", __func__);

	ret = sensor_2ld_cis_set_global_setting(subdev);
	if (ret < 0) goto p_err;
	ret = sensor_2ld_cis_mode_change(subdev, cis->cis_data->sens_config_index_cur);
	if (ret < 0) goto p_err;
	ret = sensor_2ld_cis_set_frame_duration(subdev, sensor_2ld_frame_duration_backup);
	if (ret < 0) goto p_err;
	ret = sensor_2ld_cis_set_analog_gain(subdev, &sensor_2ld_again_backup);
	if (ret < 0) goto p_err;
	ret = sensor_2ld_cis_set_digital_gain(subdev, &sensor_2ld_dgain_backup);
	if (ret < 0) goto p_err;
	ret = sensor_2ld_cis_set_exposure_time(subdev, &sensor_2ld_target_exp_backup);
	if (ret < 0) goto p_err;
	ret = sensor_2ld_cis_stream_on(subdev);
	if (ret < 0) goto p_err;
	ret = sensor_cis_wait_streamon(subdev);
	if (ret < 0) goto p_err;

	info("%s end\n", __func__);
p_err:
	return ret;
}

int sensor_2ld_cis_recover_stream_off(struct v4l2_subdev *subdev)
{
	int ret = 0;
	struct is_cis *cis = NULL;
#ifdef USE_CAMERA_SENSOR_RETENTION
	struct is_module_enum *module;
	struct is_device_sensor_peri *sensor_peri = NULL;
	struct sensor_open_extended *ext_info;
#endif

	FIMC_BUG(!subdev);

	cis = (struct is_cis *)v4l2_get_subdevdata(subdev);
	FIMC_BUG(!cis);
	FIMC_BUG(!cis->cis_data);

#ifdef USE_CAMERA_SENSOR_RETENTION
	sensor_peri = container_of(cis, struct is_device_sensor_peri, cis);
	module = sensor_peri->module;
	ext_info = &module->ext;
	FIMC_BUG(!ext_info);

	ext_info->use_retention_mode = SENSOR_RETENTION_INACTIVE;
#endif

	info("%s start\n", __func__);

	ret = sensor_2ld_cis_set_global_setting(subdev);
	if (ret < 0) goto p_err;
	ret = sensor_2ld_cis_stream_off(subdev);
	if (ret < 0) goto p_err;
	ret = sensor_cis_wait_streamoff(subdev);
	if (ret < 0) goto p_err;

	info("%s end\n", __func__);
p_err:
	return ret;
}

static struct is_cis_ops cis_ops_2ld = {
	.cis_init = sensor_2ld_cis_init,
	.cis_deinit = sensor_2ld_cis_deinit,
	.cis_log_status = sensor_2ld_cis_log_status,
	.cis_group_param_hold = sensor_2ld_cis_group_param_hold,
	.cis_set_global_setting = sensor_2ld_cis_set_global_setting,
	.cis_mode_change = sensor_2ld_cis_mode_change,
	.cis_set_size = sensor_2ld_cis_set_size,
	.cis_stream_on = sensor_2ld_cis_stream_on,
	.cis_stream_off = sensor_2ld_cis_stream_off,
	.cis_set_exposure_time = sensor_2ld_cis_set_exposure_time,
	.cis_get_min_exposure_time = sensor_2ld_cis_get_min_exposure_time,
	.cis_get_max_exposure_time = sensor_2ld_cis_get_max_exposure_time,
	.cis_adjust_frame_duration = sensor_2ld_cis_adjust_frame_duration,
	.cis_set_frame_duration = sensor_2ld_cis_set_frame_duration,
	.cis_set_frame_rate = sensor_2ld_cis_set_frame_rate,
	.cis_adjust_analog_gain = sensor_2ld_cis_adjust_analog_gain,
	.cis_set_analog_gain = sensor_2ld_cis_set_analog_gain,
	.cis_get_analog_gain = sensor_2ld_cis_get_analog_gain,
	.cis_get_min_analog_gain = sensor_2ld_cis_get_min_analog_gain,
	.cis_get_max_analog_gain = sensor_2ld_cis_get_max_analog_gain,
	.cis_set_digital_gain = sensor_2ld_cis_set_digital_gain,
	.cis_get_digital_gain = sensor_2ld_cis_get_digital_gain,
	.cis_get_min_digital_gain = sensor_2ld_cis_get_min_digital_gain,
	.cis_get_max_digital_gain = sensor_2ld_cis_get_max_digital_gain,
	.cis_compensate_gain_for_extremely_br = sensor_2ld_cis_compensate_gain_for_extremely_br,
	.cis_wait_streamoff = sensor_cis_wait_streamoff,
	.cis_wait_streamon = sensor_cis_wait_streamon,
	.cis_data_calculation = sensor_2ld_cis_data_calc,
	.cis_set_long_term_exposure = sensor_2ld_cis_long_term_exposure,
#ifdef USE_CAMERA_EMBEDDED_HEADER
	.cis_get_frame_id = sensor_2ld_cis_get_frame_id,
#endif
	.cis_set_frs_control = sensor_2ld_cis_set_frs_control,
	.cis_set_super_slow_motion_roi = sensor_2ld_cis_set_super_slow_motion_roi,
	.cis_set_super_slow_motion_setting = sensor_2ld_cis_set_super_slow_motion_setting,
	.cis_check_rev_on_init = sensor_cis_check_rev_on_init,
	.cis_set_super_slow_motion_threshold = sensor_2ld_cis_set_super_slow_motion_threshold,
	.cis_get_super_slow_motion_threshold = sensor_2ld_cis_get_super_slow_motion_threshold,
	.cis_set_initial_exposure = sensor_cis_set_initial_exposure,
	.cis_get_super_slow_motion_gmc = sensor_2ld_cis_get_super_slow_motion_gmc,
	.cis_get_super_slow_motion_frame_id = sensor_2ld_cis_get_super_slow_motion_frame_id,
	.cis_set_super_slow_motion_flicker = sensor_2ld_cis_set_super_slow_motion_flicker,
	.cis_get_super_slow_motion_flicker = sensor_2ld_cis_get_super_slow_motion_flicker,
	.cis_get_super_slow_motion_md_threshold = sensor_2ld_cis_get_super_slow_motion_md_threshold,
	.cis_set_super_slow_motion_gmc_table_idx = sensor_2ld_cis_set_super_slow_motion_gmc_table_idx,
	.cis_set_super_slow_motion_gmc_block_with_md_low = sensor_2ld_cis_set_super_slow_motion_gmc_block_with_md_low,
//	.cis_recover_stream_on = sensor_2ld_cis_recover_stream_on,
//	.cis_recover_stream_off = sensor_2ld_cis_recover_stream_off,
	.cis_set_factory_control = sensor_2ld_cis_set_factory_control,
	.cis_wait_ln_mode_delay = sensor_2ld_cis_wait_ln_mode_delay,
	.cis_set_test_pattern = sensor_cis_set_test_pattern,
};

static int cis_2ld_probe(struct i2c_client *client,
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

		sensor_peri = find_peri_by_cis_id(device, SENSOR_NAME_S5K2LD);
		if (!sensor_peri) {
			probe_info("sensor peri is net yet probed");
			return -EPROBE_DEFER;
		}
	}

	for (i = 0; i < sensor_id_len; i++) {
		device = &core->sensor[sensor_id[i]];
		sensor_peri = find_peri_by_cis_id(device, SENSOR_NAME_S5K2LD);

		cis = &sensor_peri->cis;
		subdev_cis = kzalloc(sizeof(struct v4l2_subdev), GFP_KERNEL);
		if (!subdev_cis) {
			probe_err("subdev_cis is NULL");
			ret = -ENOMEM;
			goto p_err;
		}

		sensor_peri->subdev_cis = subdev_cis;

		cis->id = SENSOR_NAME_S5K2LD;
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

		cis->cis_ops = &cis_ops_2ld;

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
		cis->hdr_ctrl_by_again = false;

		v4l2_set_subdevdata(subdev_cis, cis);
		v4l2_set_subdev_hostdata(subdev_cis, device);
		snprintf(subdev_cis->name, V4L2_SUBDEV_NAME_SIZE, "cis-subdev.%d", cis->id);

		sensor_cis_parse_dt(dev, cis->subdev);
	}

	cis->use_initial_ae = of_property_read_bool(dnode, "use_initial_ae");
	probe_info("%s use initial_ae(%d)\n", __func__, cis->use_initial_ae);

	cis->vendor_use_adaptive_mipi = of_property_read_bool(dnode, "vendor_use_adaptive_mipi");
	probe_info("%s vendor_use_adaptive_mipi(%d)\n", __func__, cis->vendor_use_adaptive_mipi);

	sensor_2ld_night_flag = false;

	ret = of_property_read_string(dnode, "setfile", &setfile);
	if (ret) {
		err("setfile index read fail(%d), take default setfile!!", ret);
		setfile = "default";
	}

	if (strcmp(setfile, "default") == 0 ||
			strcmp(setfile, "setA") == 0) {
		probe_info("%s setfile_A\n", __func__);
		sensor_2ld_global = sensor_2ld_setfile_A_Global_A2;
		sensor_2ld_global_size = ARRAY_SIZE(sensor_2ld_setfile_A_Global_A2);
		sensor_2ld_setfiles = sensor_2ld_setfiles_A;
		sensor_2ld_setfile_sizes = sensor_2ld_setfile_A_sizes;
		sensor_2ld_pllinfos = sensor_2ld_pllinfos_A;
		sensor_2ld_max_setfile_num = ARRAY_SIZE(sensor_2ld_setfiles_A);
#ifdef USE_CAMERA_SENSOR_RETENTION
		sensor_2ld_global_retention = sensor_2ld_setfile_A_Global_retention;
		sensor_2ld_global_retention_size = ARRAY_SIZE(sensor_2ld_setfile_A_Global_retention);
		sensor_2ld_retention = sensor_2ld_setfiles_A_retention;
		sensor_2ld_retention_size = sensor_2ld_setfile_A_sizes_retention;
		sensor_2ld_max_retention_num = ARRAY_SIZE(sensor_2ld_setfiles_A_retention);
		sensor_2ld_load_sram = sensor_2ld_setfile_A_load_sram;
		sensor_2ld_load_sram_size = sensor_2ld_setfile_A_sizes_load_sram;
#endif
		cis->mipi_sensor_mode = sensor_2ld_setfile_A_mipi_sensor_mode;
		cis->mipi_sensor_mode_size = ARRAY_SIZE(sensor_2ld_setfile_A_mipi_sensor_mode);
		verify_sensor_mode = sensor_2ld_setfile_A_verify_sensor_mode;
		verify_sensor_mode_size = ARRAY_SIZE(sensor_2ld_setfile_A_verify_sensor_mode);
	} else {
		err("%s setfile index out of bound, take default (setfile_A)", __func__);
		sensor_2ld_global = sensor_2ld_setfile_A_Global_A2;
		sensor_2ld_global_size = ARRAY_SIZE(sensor_2ld_setfile_A_Global_A2);
		sensor_2ld_setfiles = sensor_2ld_setfiles_A;
		sensor_2ld_setfile_sizes = sensor_2ld_setfile_A_sizes;
		sensor_2ld_pllinfos = sensor_2ld_pllinfos_A;
		sensor_2ld_max_setfile_num = ARRAY_SIZE(sensor_2ld_setfiles_A);
#ifdef USE_CAMERA_SENSOR_RETENTION
		sensor_2ld_global_retention = sensor_2ld_setfile_A_Global_retention;
		sensor_2ld_global_retention_size = ARRAY_SIZE(sensor_2ld_setfile_A_Global_retention);
		sensor_2ld_retention = sensor_2ld_setfiles_A_retention;
		sensor_2ld_retention_size = sensor_2ld_setfile_A_sizes_retention;
		sensor_2ld_max_retention_num = ARRAY_SIZE(sensor_2ld_setfiles_A_retention);
		sensor_2ld_load_sram = sensor_2ld_setfile_A_load_sram;
		sensor_2ld_load_sram_size = sensor_2ld_setfile_A_sizes_load_sram;
#endif
		cis->mipi_sensor_mode = sensor_2ld_setfile_A_mipi_sensor_mode;
		cis->mipi_sensor_mode_size = ARRAY_SIZE(sensor_2ld_setfile_A_mipi_sensor_mode);
		verify_sensor_mode = sensor_2ld_setfile_A_verify_sensor_mode;
		verify_sensor_mode_size = ARRAY_SIZE(sensor_2ld_setfile_A_verify_sensor_mode);
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

static const struct of_device_id sensor_cis_2ld_match[] = {
	{
		.compatible = "samsung,exynos-is-cis-2ld",
	},
	{},
};
MODULE_DEVICE_TABLE(of, sensor_cis_2ld_match);

static const struct i2c_device_id sensor_cis_2ld_idt[] = {
	{ SENSOR_NAME, 0 },
	{},
};

static struct i2c_driver sensor_cis_2ld_driver = {
	.probe	= cis_2ld_probe,
	.driver = {
		.name	= SENSOR_NAME,
		.owner	= THIS_MODULE,
		.of_match_table = sensor_cis_2ld_match,
		.suppress_bind_attrs = true,
	},
	.id_table = sensor_cis_2ld_idt
};

#ifdef MODULE
builtin_i2c_driver(sensor_cis_2ld_driver);
#else
static int __init sensor_cis_2ld_init(void)
{
	int ret;

	ret = i2c_add_driver(&sensor_cis_2ld_driver);
	if (ret)
		err("failed to add %s driver: %d\n",
			sensor_cis_2ld_driver.driver.name, ret);

	return ret;
}
late_initcall_sync(sensor_cis_2ld_init);
#endif

MODULE_LICENSE("GPL");
MODULE_SOFTDEP("pre: fimc-is");
