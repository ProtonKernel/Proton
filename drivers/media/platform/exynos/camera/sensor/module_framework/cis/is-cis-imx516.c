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
#include "is-cis-imx516.h"
#include "is-cis-imx516-setA.h"
#include "is-vender-specific.h"
#include "is-helper-i2c.h"

#define SENSOR_NAME "IMX516"

#define SENSOR_EXTAREA_INTG_0_A_0 0x2124
#define SENSOR_EXTAREA_INTG_0_A_1 0x2125
#define SENSOR_EXTAREA_INTG_0_A_2 0x2126
#define SENSOR_EXTAREA_INTG_0_A_3 0x2127

#define SENSOR_EXTAREA_INTG_0_B_0 0x2128
#define SENSOR_EXTAREA_INTG_0_B_1 0x2129
#define SENSOR_EXTAREA_INTG_0_B_2 0x212A
#define SENSOR_EXTAREA_INTG_0_B_3 0x212B

#define SENSOR_LASER_CONTROL_REG_1 0x2144
#define SENSOR_LASER_CONTROL_REG_2 0x2145
#define SENSOR_LASER_OFF  0xAA
#define SENSOR_LASER_ON   0x00

#define SENSOR_HMAX_UPPER 0x0800
#define SENSOR_HMAX_LOWER 0x0801

#define SENSOR_DEPTH_MAP_NUMBER 0x151C
#define SENSOR_FPS_30		33333333
#define SENSOR_FPS_24		41666666
#define SENSOR_FPS_20		50000000
#define SENSOR_FPS_15		66666666
#define SENSOR_FPS_10		100000000
#define SENSOR_FPS_5		200000000
#define SENOSR_FPS_REGISTER_1	0x2112
#define SENOSR_FPS_REGISTER_2	0x2113

static const struct v4l2_subdev_ops subdev_ops;

static const u32 *sensor_imx516_global;
static u32 sensor_imx516_global_size;
static const u32 ***sensor_imx516_setfiles;
static const u32 **sensor_imx516_laser_setting;
static const u32 *sensor_imx516_mode_id;
static u32 sensor_imx516_max_mode_id_num;
static const u32 **sensor_imx516_setfile_sizes;
static const u32 *sensor_imx516_laser_setting_sizes;
static const u16 ***sensor_imx516_setfiles_fps;

int sensor_imx516_mode = -1;
u16 sensor_imx516_HMAX;
u32 sensor_imx516_frame_duration;

#define REAR_TX_DEFAULT_FREQ 100 /* MHz */

#ifdef USE_CAMERA_REAR_TOF_TX_FREQ_VARIATION
static const struct cam_tof_sensor_mode ***sensor_imx516_tx_freq_sensor_mode;
u32 sensor_imx516_rear_tx_freq = REAR_TX_DEFAULT_FREQ;
u32 sensor_imx516_rear_tx_freq_fixed_index = -1;

int sensor_imx516_cis_get_mode_id_index(struct is_cis *cis, u32 mode, u32 *mode_id_index);

static int sensor_imx516_cis_set_tx_clock(struct v4l2_subdev *subdev)
{
	struct is_cis *cis = NULL;
	const struct cam_tof_sensor_mode *cur_tx_sensor_mode;
	int found = -1;
	u32 mode_id_index = 0;
	int ret = 0;

	cis = (struct is_cis *)v4l2_get_subdevdata(subdev);
	if (cis == NULL) {
		err("cis is NULL");
		return -1;
	}

	ret = sensor_imx516_cis_get_mode_id_index(cis, sensor_imx516_mode, &mode_id_index);
	if (ret < 0) {
		return -1;
	}

	cur_tx_sensor_mode = sensor_imx516_tx_freq_sensor_mode[mode_id_index][sensor_imx516_mode];

	if (cur_tx_sensor_mode->mipi_channel_size == 0 ||
		cur_tx_sensor_mode->mipi_channel == NULL) {
		dbg_sensor(1, "skip select mipi channel\n");
		return -1;
	}

	if(sensor_imx516_rear_tx_freq_fixed_index != -1) {
		found = sensor_imx516_rear_tx_freq_fixed_index;
	} else {
		found = is_vendor_select_mipi_by_rf_channel(cur_tx_sensor_mode->mipi_channel,
					cur_tx_sensor_mode->mipi_channel_size);
	}

	if (found != -1) {
		if (found < cur_tx_sensor_mode->sensor_setting_size) {
			sensor_cis_set_registers(subdev,
				cur_tx_sensor_mode->sensor_setting[found].setting,
				cur_tx_sensor_mode->sensor_setting[found].setting_size);
			sensor_imx516_rear_tx_freq = sensor_imx516_supported_tx_freq[found];
			if(sensor_imx516_rear_tx_freq_fixed_index != -1)
				info("%s - tx setting fixed to %d\n", __func__, sensor_imx516_rear_tx_freq);
			else
				info("%s - tx setting %d freq %d\n", __func__, found, sensor_imx516_rear_tx_freq);
		} else {
			err("sensor setting size is out of bound");
		}
	}

	return 0;
}
#endif

int sensor_imx516_cis_set_frame_rate(struct v4l2_subdev *subdev, u32 duration);

int sensor_imx516_cis_get_mode_id_index(struct is_cis *cis, u32 mode, u32 *mode_id_index)
{
	struct is_core *core;
	struct is_vender_specific *specific = NULL;
	struct is_device_sensor_peri *sensor_peri = NULL;
	u32 sensor_mode_id = 0;
	int i = 0;

	if (mode >= SENSOR_IMX516_MODE_MAX || mode == -1) {
		err("sensor mode is out of bound (%d)", mode);
		return -1;
	}

	sensor_peri = container_of(cis, struct is_device_sensor_peri, cis);

	core = (struct is_core *)dev_get_drvdata(is_dev);
	if (!core) {
		probe_info("core device is not yet probed");
		return -1;
	}

	specific = core->vender.private_data;
	if (sensor_peri->module->position == SENSOR_POSITION_REAR_TOF) {
		sensor_mode_id = specific->rear_tof_mode_id;
	} else {  // SENSOR_POSITION_FRONT_TOF
		sensor_mode_id = specific->front_tof_mode_id;
	}

	for (i = 0; i < sensor_imx516_max_mode_id_num; i++) {
		if (sensor_mode_id == sensor_imx516_mode_id[i]) {
			*mode_id_index = i;
			break;
		}
	}

	if (i == sensor_imx516_max_mode_id_num) {
		err("sensor_imx516_cis_mode_change can't find mode_id(mode_id : 0x%x) mode (%d)", sensor_mode_id, mode);
		return -1;
	}

	return 0;
}

int sensor_imx516_cis_check_rev(struct is_cis *cis)
{
	int ret = 0;
	u8 rev = 0;
	struct i2c_client *client;

	FIMC_BUG(!cis);
	FIMC_BUG(!cis->cis_data);

	client = cis->client;
	if (unlikely(!client)) {
		err("client is NULL");
		ret = -EINVAL;
		return ret;
	}

	I2C_MUTEX_LOCK(cis->i2c_lock);

	info("sensor_imx516_cis_check_rev start ");
	ret = is_sensor_read8(client, 0x0000, &rev);
	info("sensor_imx516_cis_check_rev 0x0000 %x ", rev);
	ret = is_sensor_read8(client, 0x0001, &rev);
	info("sensor_imx516_cis_check_rev 0x0001 %x ", rev);
	ret = is_sensor_read8(client, 0x0002, &rev);
	info("sensor_imx516_cis_check_rev 0x0002 %x ", rev);
	ret = is_sensor_read8(client, 0x0003, &rev);
	info("sensor_imx516_cis_check_rev 0x0003 %x ", rev);
	ret = is_sensor_read8(client, 0x0019,&rev);
	info("sensor_imx516_cis_check_rev 0x0019 %x ", rev);
	if (ret < 0) {
		err("is_sensor_read8 fail, (ret %d)", ret);
		ret = -EAGAIN;
	} else {
		cis->cis_data->cis_rev = rev;
	}
	I2C_MUTEX_UNLOCK(cis->i2c_lock);

	return ret;
}

/* CIS OPS */
int sensor_imx516_cis_init(struct v4l2_subdev *subdev)
{
	int ret = 0;
	struct is_cis *cis;
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

	ret = sensor_imx516_cis_check_rev(cis);
	if (ret < 0) {
#ifdef USE_CAMERA_HW_BIG_DATA
		sensor_peri = container_of(cis, struct is_device_sensor_peri, cis);
		if (sensor_peri)
			is_sec_get_hw_param(&hw_param, sensor_peri->module->position);
		if (hw_param)
			hw_param->i2c_sensor_err_cnt++;
#endif
		warn("sensor_imx516_check_rev is fail when cis init");
		cis->rev_flag = true;
	}

	cis->cis_data->product_name = cis->id;
	cis->cis_data->cur_width = SENSOR_IMX516_MAX_WIDTH;
	cis->cis_data->cur_height = SENSOR_IMX516_MAX_HEIGHT;
	cis->cis_data->low_expo_start = 33000;
	cis->need_mode_change = false;
	sensor_imx516_frame_duration = 0;
	sensor_imx516_mode = -1;

#ifdef DEBUG_SENSOR_TIME
	do_gettimeofday(&end);
	dbg_sensor(1, "[%s] time %lu us\n", __func__, (end.tv_sec - st.tv_sec)*1000000 + (end.tv_usec - st.tv_usec));
#endif

p_err:
	return ret;
}

int sensor_imx516_cis_log_status(struct v4l2_subdev *subdev)
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
	pr_err("[SEN:DUMP] IMX516 *************************\n");
	is_sensor_read16(client, 0x0001, &data16);
	pr_err("[SEN:DUMP] manufacturer ID(%x)\n", data16);
	is_sensor_read8(client, SENSOR_DEPTH_MAP_NUMBER, &data8);
	pr_err("[SEN:DUMP] frame counter (%x)\n", data8);
	is_sensor_read8(client, 0x1001, &data8);
	pr_err("[SEN:DUMP] mode_select(%x)\n", data8);

#if 0
	pr_err("[SEN:DUMP] global \n");
	sensor_cis_dump_registers(subdev, sensor_imx516_global, sensor_imx516_global_size);
	pr_err("[SEN:DUMP] mode \n");
	sensor_cis_dump_registers(subdev, sensor_imx516_setfiles[0], sensor_imx516_setfile_sizes[0]);
#endif
	I2C_MUTEX_UNLOCK(cis->i2c_lock);

	pr_err("[SEN:DUMP] *******************************\n");

p_err:
	return ret;
}

int sensor_imx516_cis_set_global_setting(struct v4l2_subdev *subdev)
{
	int ret = 0;
	struct is_cis *cis = NULL;

	FIMC_BUG(!subdev);

	cis = (struct is_cis *)v4l2_get_subdevdata(subdev);
	FIMC_BUG(!cis);

	I2C_MUTEX_LOCK(cis->i2c_lock);
	/* setfile global setting is at camera entrance */
	info("sensor_imx516_cis_set_global_setting");
	ret = sensor_cis_set_registers(subdev, sensor_imx516_global, sensor_imx516_global_size);
	if (ret < 0) {
		err("sensor_imx516_set_registers fail!!");
		goto p_err;
	}

	dbg_sensor(1, "[%s] global setting done\n", __func__);

p_err:
	I2C_MUTEX_UNLOCK(cis->i2c_lock);
	return ret;
}

int sensor_imx516_get_HMAX_value(const u32 *regs, const u32 size)
{
	int i = 0;

	FIMC_BUG(!regs);

	sensor_imx516_HMAX = 0;
	for (i = 0; i < size; i += I2C_NEXT) {
		if (regs[i + I2C_ADDR] == SENSOR_HMAX_UPPER) {
			sensor_imx516_HMAX |= (regs[i + I2C_DATA] << 8);
		} else if (regs[i + I2C_ADDR] == SENSOR_HMAX_LOWER) {
			sensor_imx516_HMAX |= regs[i + I2C_DATA];
			return 0;
		}
	}
	return 0;
}

int sensor_imx516_cis_mode_change(struct v4l2_subdev *subdev, u32 mode)
{
	int ret = 0;
	struct is_core *core;
	struct is_cis *cis = NULL;
	struct is_vender_specific *specific = NULL;
	struct is_device_sensor_peri *sensor_peri = NULL;
	u32 sensor_mode_id = 0;
	u32 mode_id_index = 0;

	FIMC_BUG(!subdev);

	cis = (struct is_cis *)v4l2_get_subdevdata(subdev);
	FIMC_BUG(!cis);
	FIMC_BUG(!cis->cis_data);

	if (mode >= SENSOR_IMX516_MODE_MAX) {
		err("invalid mode(%d)!!", mode);
		ret = -EINVAL;
		return ret;
	}

	/* If check_rev fail when cis_init, one more check_rev in mode_change */
	if (cis->rev_flag == true) {
		cis->rev_flag = false;
		ret = sensor_cis_check_rev(cis);
		if (ret < 0) {
			err("sensor_imx516_check_rev is fail");
			return ret;
		}
	}

	sensor_peri = container_of(cis, struct is_device_sensor_peri, cis);

	core = (struct is_core *)dev_get_drvdata(is_dev);
	if (!core) {
		probe_info("core device is not yet probed");
		ret = -EINVAL;
		return ret;
	}

	specific = core->vender.private_data;
	if (sensor_peri->module->position == SENSOR_POSITION_REAR_TOF) {
#ifdef USE_CAMERA_REAR_TOF_TX_FREQ_VARIATION
		sensor_imx516_rear_tx_freq = REAR_TX_DEFAULT_FREQ;
#endif
		sensor_mode_id = specific->rear_tof_mode_id;
	} else {  // SENSOR_POSITION_FRONT_TOF
		sensor_mode_id = specific->front_tof_mode_id;
	}

	ret = sensor_imx516_cis_get_mode_id_index(cis, mode, &mode_id_index);
	if (ret < 0) {
		return ret;
	}

	info("sensor_imx516_cis_mode_change pos %d sensor_mode_id %x mode %d index %d 0x%x",
		sensor_peri->module->position, sensor_mode_id, mode, mode_id_index, sensor_imx516_mode_id[mode_id_index]);

	I2C_MUTEX_LOCK(cis->i2c_lock);
	ret = sensor_cis_set_registers(subdev, sensor_imx516_setfiles[mode_id_index][mode], sensor_imx516_setfile_sizes[mode_id_index][mode]);
	if (ret < 0) {
		err("sensor_imx516_set_registers fail!!");
		goto p_err;
	}
	/* laser setting */
	ret = sensor_cis_set_registers(subdev, sensor_imx516_laser_setting[mode_id_index], sensor_imx516_laser_setting_sizes[mode_id_index]);
	if (ret < 0) {
		err("sensor_imx516_set_registers fail!!");
		goto p_err;
	}

	sensor_imx516_get_HMAX_value(sensor_imx516_setfiles[mode_id_index][mode], sensor_imx516_setfile_sizes[mode_id_index][mode]);
	sensor_imx516_mode = mode;
p_err:
	I2C_MUTEX_UNLOCK(cis->i2c_lock);
	return ret;
}

int sensor_imx516_cis_wait_streamoff(struct v4l2_subdev *subdev)
{
	int ret = 0;
	struct is_cis *cis;
	struct i2c_client *client;
	cis_shared_data *cis_data;
	u32 wait_cnt = 0, time_out_cnt = 250;
	u8 sensor_fcount = 0;

	FIMC_BUG(!subdev);

	cis = (struct is_cis *)v4l2_get_subdevdata(subdev);
	if (unlikely(!cis)) {
		err("cis is NULL");
		ret = -EINVAL;
		goto p_err;
	}

	cis_data = cis->cis_data;
	if (unlikely(!cis_data)) {
		err("cis_data is NULL");
		ret = -EINVAL;
		goto p_err;
	}

	client = cis->client;
	if (unlikely(!client)) {
		err("client is NULL");
		ret = -EINVAL;
		goto p_err;
	}

	usleep_range(33000, 33000);
	I2C_MUTEX_LOCK(cis->i2c_lock);
	ret = is_sensor_read8(client, SENSOR_DEPTH_MAP_NUMBER, &sensor_fcount);
	I2C_MUTEX_UNLOCK(cis->i2c_lock);

	if (ret < 0) {
		err("i2c transfer fail addr(%x), val(%x), ret = %d\n", SENSOR_DEPTH_MAP_NUMBER, sensor_fcount, ret);
		ret = -EINVAL;
		goto p_err;
	}

	/*
	 * stream on (0x00 ~ 0xFE), stream off (0xFF)
	 */
	while (sensor_fcount != 0xFF) {
		I2C_MUTEX_LOCK(cis->i2c_lock);
		ret = is_sensor_read8(client, SENSOR_DEPTH_MAP_NUMBER, &sensor_fcount);
		I2C_MUTEX_UNLOCK(cis->i2c_lock);

		if (ret < 0) {
			err("i2c transfer fail addr(%x), val(%x), ret = %d\n", SENSOR_DEPTH_MAP_NUMBER, sensor_fcount, ret);
			ret = -EINVAL;
			goto p_err;
		}

		usleep_range(CIS_STREAM_OFF_WAIT_TIME, CIS_STREAM_OFF_WAIT_TIME);
		wait_cnt++;

		if (wait_cnt >= time_out_cnt) {
			err("[MOD:D:%d] %s, time out, wait_limit(%d) > time_out(%d), sensor_fcount(%d)",
					cis->id, __func__, wait_cnt, time_out_cnt, sensor_fcount);
			ret = -EINVAL;
			goto p_err;
		}

		dbg_sensor(1, "[MOD:D:%d] %s, sensor_fcount(%d), (wait_limit(%d) < time_out(%d))\n",
				cis->id, __func__, sensor_fcount, wait_cnt, time_out_cnt);
	}

p_err:
	return ret;
}

int sensor_imx516_cis_wait_streamon(struct v4l2_subdev *subdev)
{
	int ret = 0;
	struct is_cis *cis;
	struct i2c_client *client;
	cis_shared_data *cis_data;
	u32 wait_cnt = 0, time_out_cnt = 250;
	u8 sensor_fcount = 0;

	FIMC_BUG(!subdev);

	cis = (struct is_cis *)v4l2_get_subdevdata(subdev);
	if (unlikely(!cis)) {
	    err("cis is NULL");
	    ret = -EINVAL;
	    goto p_err;
	}

	cis_data = cis->cis_data;
	if (unlikely(!cis_data)) {
	    err("cis_data is NULL");
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
	ret = is_sensor_read8(client, SENSOR_DEPTH_MAP_NUMBER, &sensor_fcount);
	I2C_MUTEX_UNLOCK(cis->i2c_lock);
	if (ret < 0) {
		err("i2c transfer fail addr(%x), val(%x), ret = %d\n", SENSOR_DEPTH_MAP_NUMBER, sensor_fcount, ret);
		ret = -EINVAL;
		goto p_err;
	}

	/*
	 * Read sensor frame counter (sensor_fcount address = 0x0005)
	 * stream on (0x00 ~ 0xFE), stream off (0xFF)
	 */
	while (sensor_fcount == 0xff) {
		usleep_range(CIS_STREAM_ON_WAIT_TIME, CIS_STREAM_ON_WAIT_TIME);
		wait_cnt++;

		I2C_MUTEX_LOCK(cis->i2c_lock);
		ret = is_sensor_read8(client, SENSOR_DEPTH_MAP_NUMBER, &sensor_fcount);
		I2C_MUTEX_UNLOCK(cis->i2c_lock);

		if (ret < 0) {
			err("i2c transfer fail addr(%x), val(%x), ret = %d\n", SENSOR_DEPTH_MAP_NUMBER, sensor_fcount, ret);
			ret = -EINVAL;
			goto p_err;
		}

		if (wait_cnt >= time_out_cnt) {
			err("[MOD:D:%d] %s, Don't sensor stream on and time out, wait_limit(%d) > time_out(%d), sensor_fcount(%d)",
				cis->id, __func__, wait_cnt, time_out_cnt, sensor_fcount);
			ret = -EINVAL;
			goto p_err;
		}

		dbg_sensor(1, "[MOD:D:%d] %s, sensor_fcount(%d), (wait_limit(%d) < time_out(%d))\n",
				cis->id, __func__, sensor_fcount, wait_cnt, time_out_cnt);
	}

p_err:
	return ret;
}

int sensor_imx516_cis_stream_on(struct v4l2_subdev *subdev)
{
	int ret = 0;
	struct is_cis *cis;
	struct i2c_client *client;
	struct is_module_enum *module;
	cis_shared_data *cis_data;
	struct is_device_sensor_peri *sensor_peri = NULL;
	struct exynos_platform_is_module *pdata;

#ifdef DEBUG_SENSOR_TIME
	struct timeval st, end;
	do_gettimeofday(&st);
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
		ret = -EINVAL;
		goto p_err;
	}

	cis_data = cis->cis_data;
	module = sensor_peri->module;

	dbg_sensor(1, "[MOD:D:%d] %s\n", cis->id, __func__);
	/* fps setting */
	if (sensor_imx516_frame_duration != 0)
		sensor_imx516_cis_set_frame_rate(subdev, sensor_imx516_frame_duration);

	/* Sensor stream on */
	info("sensor_imx516_cis_stream_on %d", module->position);

	I2C_MUTEX_LOCK(cis->i2c_lock);
#ifdef USE_CAMERA_REAR_TOF_TX_FREQ_VARIATION
	if (module->position == SENSOR_POSITION_REAR_TOF) {
		sensor_imx516_cis_set_tx_clock(subdev);
	}
#endif
	is_sensor_write8(client, 0x1001, 0x01);
	I2C_MUTEX_UNLOCK(cis->i2c_lock);

	cis_data->stream_on = true;

	/* VCSEL ON  */
	pdata = module->pdata;
	if (!pdata) {
		clear_bit(IS_MODULE_GPIO_ON, &module->state);
		err("pdata is NULL");
		ret = -EINVAL;
		goto p_err;
	}

	if (!pdata->gpio_cfg) {
		clear_bit(IS_MODULE_GPIO_ON, &module->state);
		err("gpio_cfg is NULL");
		ret = -EINVAL;
		goto p_err;
	}

	usleep_range(200, 200);
	ret = pdata->gpio_cfg(module, SENSOR_SCENARIO_ADDITIONAL_POWER, GPIO_SCENARIO_ON);
	if (ret) {
		clear_bit(IS_MODULE_GPIO_ON, &module->state);
		err("gpio_cfg is fail(%d)", ret);
		goto p_err;
	}

#ifdef DEBUG_SENSOR_TIME
	do_gettimeofday(&end);
	dbg_sensor(1, "[%s] time %lu us\n", __func__, (end.tv_sec - st.tv_sec)*1000000 + (end.tv_usec - st.tv_usec));
#endif

p_err:
	return ret;
}

int sensor_imx516_cis_stream_off(struct v4l2_subdev *subdev)
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

	I2C_MUTEX_LOCK(cis->i2c_lock);
	/* Sensor stream off */
	info("sensor_imx516_cis_stream_off");
	is_sensor_write8(client, 0x1001, 0x00);
	I2C_MUTEX_UNLOCK(cis->i2c_lock);

	cis_data->stream_on = false;

#ifdef DEBUG_SENSOR_TIME
	do_gettimeofday(&end);
	dbg_sensor(1, "[%s] time %lu us\n", __func__, (end.tv_sec - st.tv_sec)*1000000 + (end.tv_usec - st.tv_usec));
#endif

p_err:
	return ret;
}

int sensor_imx516_cis_set_exposure_time(struct v4l2_subdev *subdev, struct ae_param *target_exposure)
{
	#define AE_MAX 0x1D4C0
	int ret = 0;
	struct is_cis *cis;
	struct i2c_client *client;
	u8 value;
	u32 value_32;

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

	if (target_exposure->long_val) {
		info("initial setting is skip");
		goto p_err;
	}

	value_32 = target_exposure->short_val * 120;
	if (value_32 > AE_MAX) {
		value_32 = AE_MAX;
	} else if (value_32 < sensor_imx516_HMAX) {
		value_32 = sensor_imx516_HMAX;
	}

	info("sensor_imx516_cis_set_exposure_time org %d value %x sensor_imx516_HMAX %x",
		target_exposure->short_val, value_32, sensor_imx516_HMAX);

	I2C_MUTEX_LOCK(cis->i2c_lock);
	value = value_32 & 0xFF;
	is_sensor_write8(client, SENSOR_EXTAREA_INTG_0_A_3, value);
	is_sensor_write8(client, SENSOR_EXTAREA_INTG_0_B_3, value);
	value = (value_32 & 0xFF00) >> 8;
	is_sensor_write8(client, SENSOR_EXTAREA_INTG_0_A_2, value);
	is_sensor_write8(client, SENSOR_EXTAREA_INTG_0_B_2, value);
	value = (value_32 & 0xFF0000) >> 16;
	is_sensor_write8(client, SENSOR_EXTAREA_INTG_0_A_1, value);
	is_sensor_write8(client, SENSOR_EXTAREA_INTG_0_B_1, value);
	value = (value_32 & 0xFF000000) >> 24;
	is_sensor_write8(client, SENSOR_EXTAREA_INTG_0_A_0, value);
	is_sensor_write8(client, SENSOR_EXTAREA_INTG_0_B_0, value);
	I2C_MUTEX_UNLOCK(cis->i2c_lock);

p_err:
	return ret;
}

int sensor_imx516_cis_set_frame_rate(struct v4l2_subdev *subdev, u32 duration)
{
	int ret = 0;
	struct is_cis *cis;
	struct i2c_client *client;
	u16 cur_fps = 0;
	const u16 *fps_list = NULL;
	u8 fps = 0;
	u32 mode_id_index = 0;

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

	if (duration == 0) {
		err("duration is 0");
		return ret;
	}

	if (sensor_imx516_mode == -1) {
		sensor_imx516_frame_duration = duration;
		warn("need sensor setting (%d)", duration);
		ret = -EINVAL;
		goto p_err;
	}

	duration = (1000 * 1000 * 1000) / duration;
	sensor_imx516_frame_duration = duration;

	ret = sensor_imx516_cis_get_mode_id_index(cis, sensor_imx516_mode, &mode_id_index);
	if (ret < 0) {
		return ret;
	}

	fps_list = sensor_imx516_setfiles_fps[mode_id_index][sensor_imx516_mode];
	I2C_MUTEX_LOCK(cis->i2c_lock);
	is_sensor_write8(client, 0x0102, 0x01);

	info("%s : set %d", __func__, duration);
	switch (duration) {
		case SENSOR_FPS_5:
			cur_fps = fps_list[5];
			fps = cur_fps & 0xFF;
			is_sensor_write8(client, SENOSR_FPS_REGISTER_2, fps);
			fps = cur_fps >> 8;
			is_sensor_write8(client, SENOSR_FPS_REGISTER_1, fps);
			break;
		case SENSOR_FPS_10:
			cur_fps = fps_list[4];
			fps = cur_fps & 0xFF;
			is_sensor_write8(client, SENOSR_FPS_REGISTER_2, fps);
			fps = cur_fps >> 8;
			is_sensor_write8(client, SENOSR_FPS_REGISTER_1, fps);
			break;
		case SENSOR_FPS_15:
			cur_fps = fps_list[3];
			fps = cur_fps & 0xFF;
			is_sensor_write8(client, SENOSR_FPS_REGISTER_2, fps);
			fps = cur_fps >> 8;
			is_sensor_write8(client, SENOSR_FPS_REGISTER_1, fps);
			break;
		case SENSOR_FPS_20:
			cur_fps = fps_list[2];
			fps = cur_fps & 0xFF;
			is_sensor_write8(client, SENOSR_FPS_REGISTER_2, fps);
			fps = cur_fps >> 8;
			is_sensor_write8(client, SENOSR_FPS_REGISTER_1, fps);
			break;
		case SENSOR_FPS_24:
			cur_fps = fps_list[1];
			fps = cur_fps & 0xFF;
			is_sensor_write8(client, SENOSR_FPS_REGISTER_2, fps);
			fps = cur_fps >> 8;
			is_sensor_write8(client, SENOSR_FPS_REGISTER_1, fps);
			break;
		case SENSOR_FPS_30:
			cur_fps = fps_list[0];
			fps = cur_fps & 0xFF;
			is_sensor_write8(client, SENOSR_FPS_REGISTER_2, fps);
			fps = cur_fps >> 8;
			is_sensor_write8(client, SENOSR_FPS_REGISTER_1, fps);
			break;
		default:
			err("Unsupported fps : %d", duration);
			break;
	}

	is_sensor_write8(client, 0x0102, 0x00);
	I2C_MUTEX_UNLOCK(cis->i2c_lock);

p_err:
	return ret;
}

int sensor_imx516_cis_set_laser_control(struct v4l2_subdev *subdev, u32 onoff)
{
	int ret = 0;
	struct is_cis *cis;
	struct i2c_client *client;
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
	info("sensor_imx516_cis_set_laser_control %d", onoff);

	I2C_MUTEX_LOCK(cis->i2c_lock);
	if (onoff) {
		is_sensor_write8(client, SENSOR_LASER_CONTROL_REG_1, SENSOR_LASER_ON);
		is_sensor_write8(client, SENSOR_LASER_CONTROL_REG_2, SENSOR_LASER_ON);
	} else {
		is_sensor_write8(client, SENSOR_LASER_CONTROL_REG_1, SENSOR_LASER_OFF);
		is_sensor_write8(client, SENSOR_LASER_CONTROL_REG_2, SENSOR_LASER_OFF);
	}
	I2C_MUTEX_UNLOCK(cis->i2c_lock);
p_err:
	return ret;
}

int sensor_imx516_cis_set_vcsel_current(struct v4l2_subdev *subdev, u32 value)
{
	int ret = 0;
	struct is_cis *cis;
	cis_shared_data *cis_data;
	struct i2c_client *client;
	struct is_module_enum *module;
	struct is_device_sensor_peri *sensor_peri = NULL;
	u8 value8 = (u8)value;

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

	sensor_peri = container_of(cis, struct is_device_sensor_peri, cis);
	FIMC_BUG(!sensor_peri);
	module = sensor_peri->module;

	cis_data = cis->cis_data;
	if (cis_data->stream_on == false) {
		err("sensor_imx516_cis_set_vcsel_current fail!! sensor is off ");
		goto p_err;
	}

	info("sensor_imx516_cis_set_vcsel_current %d", value);

	I2C_MUTEX_LOCK(cis->i2c_lock);

	/* for setting IPD_OFFSET */
	is_sensor_write8(client, 0x0403, 0x20);
	is_sensor_write8(client, 0x0405, 0x00);
	is_sensor_write8(client, 0x0407, 0x00);
	is_sensor_write8(client, 0x0500, 0x02);
	is_sensor_write8(client, 0x0501, 0x0e);
	is_sensor_write8(client, 0x0502, 0x80);
	is_sensor_write8(client, 0x0401, 0x01);
	is_sensor_write8(client, 0x0400, 0x01);
	is_sensor_write8(client, 0x0401, 0x00);

	/* release DIFF2 Error Mask */
	is_sensor_write8(client, 0x0403, 0x20);
	is_sensor_write8(client, 0x0405, 0x00);
	is_sensor_write8(client, 0x0407, 0x00);
	is_sensor_write8(client, 0x0500, 0x02);
	is_sensor_write8(client, 0x0501, 0x14);
	is_sensor_write8(client, 0x0502, 0x00);
	is_sensor_write8(client, 0x0401, 0x01);
	is_sensor_write8(client, 0x0400, 0x01);
	is_sensor_write8(client, 0x0401, 0x00);

	/* for setting VCSEL current */
	is_sensor_write8(client, 0x0403, 0x20);
	is_sensor_write8(client, 0x0405, 0x00);
	is_sensor_write8(client, 0x0407, 0x00);
	is_sensor_write8(client, 0x0500, 0x02);
	is_sensor_write8(client, 0x0501, 0x08);
	is_sensor_write8(client, 0x0502, value8);
	is_sensor_write8(client, 0x0401, 0x01);
	is_sensor_write8(client, 0x0400, 0x01);
	is_sensor_write8(client, 0x0401, 0x00);

	I2C_MUTEX_UNLOCK(cis->i2c_lock);
	usleep_range(5000, 5000);

p_err:
	return ret;
}

int sensor_imx516_cis_set_laser_error(struct v4l2_subdev *subdev, u32 mode)
{
	int ret = 0;
	struct is_cis *cis;
	struct i2c_client *client;
	int i=0;
	struct sensor_imx516_laser_test test;

	FIMC_BUG(!subdev);
	cis = (struct is_cis *)v4l2_get_subdevdata(subdev);

	FIMC_BUG(!cis);
	FIMC_BUG(!cis->i2c_lock);

	client = cis->client;
	if (unlikely(!client)) {
		err("client is NULL");
		ret = -EINVAL;
		goto p_err;
	}

	for(i=0; i < sizeof(sensor_imx516_laser_testcases)/sizeof(sensor_imx516_laser_testcases[0]); i++){
		test = sensor_imx516_laser_testcases[i];

		if(test.error == mode){
			I2C_MUTEX_LOCK(cis->i2c_lock);

			is_sensor_write8(client, 0x0403, 0x20);
			is_sensor_write8(client, 0x0405, 0x00);
			is_sensor_write8(client, 0x0407, 0x00);		/*send buf CXA4026 config */
			is_sensor_write8(client, 0x0500, 0x02);
			is_sensor_write8(client, 0x0501, test.addr);		/* CXA4026 addr */
			is_sensor_write8(client, 0x0502, test.data);		/* Data to send */
			is_sensor_write8(client, 0x0401, 0x01);		/* Send start*/
			is_sensor_write8(client, 0x0400, 0x01);
			is_sensor_write8(client, 0x0401, 0x00);		/* Send end */

			I2C_MUTEX_UNLOCK(cis->i2c_lock);

			info("%s - Set err: %d (0x%x):0x%x", __func__, test.error, test.addr, test.data);
		}
	}
p_err:
	return ret;
}

int sensor_imx516_cis_get_laser_error_flag(struct v4l2_subdev *subdev, int *value)
{
	int ret = 0;
	struct is_cis *cis;
	struct i2c_client *client;
	u8 err_flag[2] = { 0 };

	FIMC_BUG(!subdev);
	cis = (struct is_cis *)v4l2_get_subdevdata(subdev);

	FIMC_BUG(!cis);
	FIMC_BUG(!cis->i2c_lock);

	client = cis->client;
	if (unlikely(!client)) {
		err("client is NULL");
		ret = -EINVAL;
		goto p_err;
	}

	I2C_MUTEX_LOCK(cis->i2c_lock);

	is_sensor_read8(client, 0x0590, &err_flag[0]);	/* 0x0590 for CSA4026 error flag(0x27) */
	is_sensor_read8(client, 0x0591, &err_flag[1]);	/* 0x0591 for CSA4026 error flag(0x28) */

	I2C_MUTEX_UNLOCK(cis->i2c_lock);

	*value = (((u16)err_flag[0] << 6) | err_flag[1]);
	info("%s - err : %x (err[0] : %x, err[1] : %x)", __func__, *value, err_flag[0], err_flag[1]);

p_err:
	return ret;
}

int sensor_imx516_cis_laser_error_req(struct v4l2_subdev *subdev, u32 mode, int *value){
	int ret = 0;
	if(mode)
		ret = sensor_imx516_cis_set_laser_error(subdev, mode);
	else
		ret = sensor_imx516_cis_get_laser_error_flag(subdev, value);
	return ret;
}

int sensor_imx516_cis_get_vcsel_photo_diode(struct v4l2_subdev *subdev, u16 *value)
{
	int ret = 0;
	struct is_cis *cis;
	struct i2c_client *client;
	u8 value8_1 = 0, value8_0 = 0;
	u8 err_flag[2] = { 0 };

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

	I2C_MUTEX_LOCK(cis->i2c_lock);

	is_sensor_read8(client, 0x0588, &value8_1) ;	/* 0x058B for PD_TARGET_DATA[9:8] */
	is_sensor_read8(client, 0x058F, &value8_0);	/* 0x0582 for PD_TARGET_DATA[7:0] */
	is_sensor_read8(client, 0x0590, &err_flag[0]);	/* 0x0590 for CSA4026 error flag(0x27) */
	is_sensor_read8(client, 0x0591, &err_flag[1]);	/* 0x0591 for CSA4026 error flag(0x28) */

	I2C_MUTEX_UNLOCK(cis->i2c_lock);

	*value = (((value8_1 & 0x30) << 4) | value8_0);
	info("%s - value8_1 0x%x value8_0 0x%x => (0x%x)%d", __func__, value8_1, value8_0, *value, *value);
	info("%s - debug for Tx err 0x0590(%x) 0x0591(%x)", __func__, err_flag[0], err_flag[1]);

p_err:
	return ret;
}

#ifdef USE_CAMERA_REAR_TOF_TX_FREQ_VARIATION
int sensor_imx516_cis_set_tx_freq(struct v4l2_subdev *subdev, u32 value)
{
	int i=0;
	int freq_size = ARRAY_SIZE(sensor_imx516_supported_tx_freq);

	sensor_imx516_rear_tx_freq_fixed_index = -1;
	for(i=0;i < freq_size;i++){
		if(value == sensor_imx516_supported_tx_freq[i]) {
			sensor_imx516_rear_tx_freq_fixed_index = i;
			break;
		}
	}
	if(sensor_imx516_rear_tx_freq_fixed_index == -1){
		info("%s - not support freq",__func__);
		return -1;
	}
	info("%s - tx freq fixed : %d", __func__,
			sensor_imx516_supported_tx_freq[sensor_imx516_rear_tx_freq_fixed_index]);
	return sensor_imx516_supported_tx_freq[sensor_imx516_rear_tx_freq_fixed_index];
}
#endif
#if defined(USE_CAMERA_REAR_TOF_TX_FREQ_VARIATION) || defined(USE_CAMERA_REAR_TOF_TX_FREQ_VARIATION_SYSFS_ENABLE)
int sensor_imx516_cis_get_tx_freq(struct v4l2_subdev *subdev, u32 *value)
{
	int ret = 0;
	struct is_cis *cis;
	struct is_module_enum *module;
	struct is_device_sensor_peri *sensor_peri = NULL;
	FIMC_BUG(!subdev);

	cis = (struct is_cis *)v4l2_get_subdevdata(subdev);

	FIMC_BUG(!cis);
	FIMC_BUG(!cis->cis_data);

	sensor_peri = container_of(cis, struct is_device_sensor_peri, cis);
	FIMC_BUG(!sensor_peri);
	module = sensor_peri->module;

	if (module->position == SENSOR_POSITION_REAR_TOF) {
#ifdef USE_CAMERA_REAR_TOF_TX_FREQ_VARIATION
		info("%s rear tx freq %d", __func__, sensor_imx516_rear_tx_freq);
		*value = sensor_imx516_rear_tx_freq;
#else
		info("%s freq variation disabled, rear tx freq %d", __func__, REAR_TX_DEFAULT_FREQ);
		*value = REAR_TX_DEFAULT_FREQ;
#endif
	} else { //SENSOR_POSITION_FRONT_TOF
		info("%s front tx freq none ", __func__);
	}

	return ret;
}
#endif

static struct is_cis_ops cis_ops_imx516 = {
	.cis_init = sensor_imx516_cis_init,
	.cis_log_status = sensor_imx516_cis_log_status,
	.cis_set_global_setting = sensor_imx516_cis_set_global_setting,
	.cis_mode_change = sensor_imx516_cis_mode_change,
	.cis_stream_on = sensor_imx516_cis_stream_on,
	.cis_stream_off = sensor_imx516_cis_stream_off,
	.cis_set_exposure_time = sensor_imx516_cis_set_exposure_time,
	.cis_wait_streamoff = sensor_imx516_cis_wait_streamoff,
	.cis_wait_streamon = sensor_imx516_cis_wait_streamon,
	.cis_set_laser_control = sensor_imx516_cis_set_laser_control,
	.cis_set_laser_current = sensor_imx516_cis_set_vcsel_current,
	.cis_get_laser_photo_diode = sensor_imx516_cis_get_vcsel_photo_diode,
	.cis_set_frame_rate = sensor_imx516_cis_set_frame_rate,
	.cis_get_tof_laser_error_flag = sensor_imx516_cis_laser_error_req,
#if defined(USE_CAMERA_REAR_TOF_TX_FREQ_VARIATION) || defined(USE_CAMERA_REAR_TOF_TX_FREQ_VARIATION_SYSFS_ENABLE)
	.cis_get_tof_tx_freq = sensor_imx516_cis_get_tx_freq,
#endif
#ifdef USE_CAMERA_REAR_TOF_TX_FREQ_VARIATION
	.cis_set_tof_tx_freq = sensor_imx516_cis_set_tx_freq,
#endif
};

static int cis_imx516_probe(struct i2c_client *client,
	const struct i2c_device_id *id)
{
	int ret = 0;
	struct is_core *core = NULL;
	struct v4l2_subdev *subdev_cis = NULL;
	struct is_cis *cis = NULL;
	struct is_device_sensor *device = NULL;
	struct is_device_sensor_peri *sensor_peri = NULL;
	u32 sensor_id;
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

	sensor_peri = find_peri_by_cis_id(device, SENSOR_NAME_IMX516);
	if (!sensor_peri) {
		probe_info("sensor peri is net yet probed");
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

	cis->id = SENSOR_NAME_IMX516;
	cis->subdev = subdev_cis;
	cis->device = 0;
	cis->client = client;
	sensor_peri->module->client = cis->client;
	cis->ctrl_delay = N_PLUS_TWO_FRAME;
	cis->mipi_clock_index_cur = CAM_MIPI_NOT_INITIALIZED;
	cis->mipi_clock_index_new = CAM_MIPI_NOT_INITIALIZED;
	cis->cis_data = kzalloc(sizeof(cis_shared_data), GFP_KERNEL);
	if (!cis->cis_data) {
		err("cis_data is NULL");
		ret = -ENOMEM;
		goto p_err;
	}
	cis->cis_ops = &cis_ops_imx516;

	if (of_property_read_string(dnode, "setfile", &setfile)) {
		err("setfile index read fail(%d), take default setfile!!", ret);
		setfile = "default";
	}

	if (strcmp(setfile, "default") == 0 || strcmp(setfile, "setA") == 0) {
		probe_info("%s setfile_A\n", __func__);
		sensor_imx516_global = sensor_imx516_setfile_A_Global;
		sensor_imx516_global_size = ARRAY_SIZE(sensor_imx516_setfile_A_Global);
		sensor_imx516_setfiles = sensor_imx516_setfiles_A;
		sensor_imx516_mode_id = sensor_imx516_setfiles_mode_id;
		sensor_imx516_max_mode_id_num = sensor_imx516_setfiles_mode_id_num;
		sensor_imx516_setfile_sizes = sensor_imx516_setfile_A_sizes;
		sensor_imx516_setfiles_fps = sensor_imx516_setfile_FPS_A;
		sensor_imx516_laser_setting = sensor_imx516_laser_setting_A;
		sensor_imx516_laser_setting_sizes = sensor_imx516_laser_setting_A_sizes;
#ifdef USE_CAMERA_REAR_TOF_TX_FREQ_VARIATION
		sensor_imx516_tx_freq_sensor_mode = sensor_imx516_setfile_A_tof_sensor_mode;
#endif
	}
#if 0
	else if (strcmp(setfile, "setB") == 0) {
		probe_info("%s setfile_B\n", __func__);
		sensor_imx516_global = sensor_imx516_setfile_B_Global;
		sensor_imx516_global_size = ARRAY_SIZE(sensor_imx516_setfile_B_Global);
		sensor_imx516_setfiles = sensor_imx516_setfiles_B;
		sensor_imx516_setfile_sizes = sensor_imx516_setfile_B_sizes;
	}
#endif
	else {
		err("%s setfile index out of bound, take default (setfile_A)", __func__);
		sensor_imx516_global = sensor_imx516_setfile_A_Global;
		sensor_imx516_global_size = ARRAY_SIZE(sensor_imx516_setfile_A_Global);
		sensor_imx516_setfiles = sensor_imx516_setfiles_A;
		sensor_imx516_mode_id = sensor_imx516_setfiles_mode_id;
		sensor_imx516_max_mode_id_num = sensor_imx516_setfiles_mode_id_num;
		sensor_imx516_setfile_sizes = sensor_imx516_setfile_A_sizes;
	}

	/* belows are depend on sensor cis. MUST check sensor spec */
	cis->bayer_order = OTF_INPUT_ORDER_BAYER_GR_BG;

	if (of_property_read_bool(dnode, "sensor_f_number")) {
		ret = of_property_read_u32(dnode, "sensor_f_number", &cis->aperture_num);
		if (ret) {
			warn("f-number read is fail(%d)",ret);
		}
	} else {
		cis->aperture_num = F2_4;
	}

	probe_info("%s f-number %d\n", __func__, cis->aperture_num);

	cis->use_dgain = true;
	cis->hdr_ctrl_by_again = false;

	ret = of_property_read_string(dnode, "setfile", &setfile);
	if (ret) {
		err("setfile index read fail(%d), take default setfile!!", ret);
		setfile = "default";
	}

	cis->use_initial_ae = of_property_read_bool(dnode, "use_initial_ae");
	probe_info("%s use initial_ae(%d)\n", __func__, cis->use_initial_ae);

	v4l2_i2c_subdev_init(subdev_cis, client, &subdev_ops);
	v4l2_set_subdevdata(subdev_cis, cis);
	v4l2_set_subdev_hostdata(subdev_cis, device);
	snprintf(subdev_cis->name, V4L2_SUBDEV_NAME_SIZE, "cis-subdev.%d", cis->id);
#ifdef USE_CAMERA_REAR_TOF_TX_FREQ_VARIATION
	{
		int mode = 0;
		for (mode = 0; mode < SENSOR_IMX516_MODE_MAX; mode++) {
			probe_info("%s verify mipi_channel mode :%d",__func__,mode);
			if (is_vendor_verify_mipi_channel(sensor_imx516_tx_freq_sensor_mode[0][mode]->mipi_channel,
						sensor_imx516_tx_freq_sensor_mode[0][mode]->mipi_channel_size)) {
				panic("wrong mipi channel");
				break;
			}
		}
	}
#endif
	probe_info("%s done\n", __func__);

p_err:
	return ret;
}

static const struct of_device_id sensor_cis_imx516_match[] = {
	{
		.compatible = "samsung,exynos-is-cis-imx516",
	},
	{},
};
MODULE_DEVICE_TABLE(of, sensor_cis_imx516_match);

static const struct i2c_device_id sensor_cis_imx516_idt[] = {
	{ SENSOR_NAME, 0 },
	{},
};

static struct i2c_driver sensor_cis_imx516_driver = {
	.probe	= cis_imx516_probe,
	.driver = {
		.name	= SENSOR_NAME,
		.owner	= THIS_MODULE,
		.of_match_table = sensor_cis_imx516_match,
		.suppress_bind_attrs = true,
	},
	.id_table = sensor_cis_imx516_idt
};

static int __init sensor_cis_imx516_init(void)
{
	int ret;

	probe_info("sensor_cis_imx516_init");
	ret = i2c_add_driver(&sensor_cis_imx516_driver);
	if (ret)
		err("failed to add %s driver: %d\n",
			sensor_cis_imx516_driver.driver.name, ret);

	return ret;
}
late_initcall_sync(sensor_cis_imx516_init);
