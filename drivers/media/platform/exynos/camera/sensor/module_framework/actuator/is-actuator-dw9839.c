/*
 * Samsung Exynos9 SoC series Actuator driver
 *
 *
 * Copyright (c) 2018 Samsung Electronics Co., Ltd
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#include <linux/i2c.h>
#include <linux/time.h>
#include <linux/slab.h>
#include <linux/module.h>
#include <linux/videodev2.h>
#include <linux/videodev2_exynos_camera.h>

#include "is-actuator-dw9839.h"
#include "is-device-sensor.h"
#include "is-device-sensor-peri.h"
#include "is-core.h"
#include "is-helper-i2c.h"

#include "interface/is-interface-library.h"

#define ACTUATOR_NAME		"DW9839"

#define REG_TARGET_MSB	0x00 /* [D7:D0] - TARGET[9:2] */
#define REG_TARGET_LSB	0x01 /* [D7:D6] - TARGET[1:0] */
#define REG_ACTIVE_EN	0x03 /* [D4] - ACTIVE, [D0] - EN */
#define REG_ACT_MODE	0x04 /* [D2:D0] - ACT_MODE, b'000: Active, b'010: FRA, b'100: Hall Calibration */
#define REG_SW_RESET	0x05 /* [D0] - S/W reset */
#define REG_M_STATUS	0x0F /* [D0] - M_BUSY */
#define REG_PCAL_ADJ	0x47 /* [D7:D0] - PCAL ADJ */
#define REG_NCAL_ADJ	0x48 /* [D7:D0] - NCAL ADJ */
#define REG_CAL_FLAG	0x5A /* [D1:D0] - CAL_FLAG, b'00: No calibration, b'01: pass, b'1x: fail */
#define REG_PCAL_MSB	0x5B /* [D1:D0] - PCAL[9:8] */
#define REG_PCAL_LSB	0x5C /* [D7:D0] - PCAL[7:0] */
#define REG_NCAL_MSB	0x5D /* [D1:D0] - NCAL[9:8] */
#define REG_NCAL_LSB	0x5E /* [D7:D0] - NCAL[7:0] */
#define REG_INIT_POS	0x61 /* [D7:D0] - INIT_POSITION[7:0] */
#define REG_ADC_R_EN	0x12 /* [D0] - ADC READ */
#define REG_ADC_R_MSB	0x50 /* [D1:D0] - CUR POS[9:8] */
#define REG_ADC_R_LSB	0x51 /* [D7:D0] - CUR POS[7:0] */

#define DEF_DW9839_INIT_POSITION		300

/* #define MANUAL_PID_CAL_SETTING */

extern struct is_sysfs_actuator sysfs_actuator;

static int sensor_dw9839_init(struct is_actuator *actuator)
{
	int ret = 0;
	struct i2c_client *client = NULL;
	struct dw9839_actuator_info *actuator_info;
	u8 ncal_msb, ncal_lsb, pcal_msb, pcal_lsb;

	FIMC_BUG(!actuator->priv_info);

	client = actuator->client;
	if (unlikely(!client)) {
		err("client is NULL");
		return -EINVAL;
	}

	/* delay after power on */
	usleep_range(4000, 4010);

	if (actuator->init_cal_setting)
		goto skip_cal;

#ifdef MANUAL_PID_CAL_SETTING
	/* =============  PID initial code ===================*/
	ret = is_sensor_addr8_write8(client, 0x40, 0xf1);
	ret |= is_sensor_addr8_write8(client, 0x41, 0x31);
	ret |= is_sensor_addr8_write8(client, 0x42, 0x2B);
	ret |= is_sensor_addr8_write8(client, 0x47, 0x00); /* PCAL_ADJ */
	ret |= is_sensor_addr8_write8(client, 0x48, 0x00); /* NCAL_ADJ */
	ret |= is_sensor_addr8_write8(client, 0x49, 0x0A); /* PGAIN */
	ret |= is_sensor_addr8_write8(client, 0x4A, 0xF0);
	ret |= is_sensor_addr8_write8(client, 0x4B, 0x00); /* IGAIN */
	ret |= is_sensor_addr8_write8(client, 0x4C, 0x21);
	ret |= is_sensor_addr8_write8(client, 0x4D, 0x07); /* DGAIN */
	ret |= is_sensor_addr8_write8(client, 0x4E, 0x3A);
	ret |= is_sensor_addr8_write8(client, 0x4F, 0x23); /* DLPF */
	ret |= is_sensor_addr8_write8(client, 0x52, 0x80);
	ret |= is_sensor_addr8_write8(client, 0x53, 0x00);
	ret |= is_sensor_addr8_write8(client, 0x54, 0x00);
	ret |= is_sensor_addr8_write8(client, 0x55, 0x01);
	ret |= is_sensor_addr8_write8(client, 0x57, 0x8F);
	ret |= is_sensor_addr8_write8(client, 0x58, 0x80);
	ret |= is_sensor_addr8_write8(client, 0x59, 0x05);
	ret |= is_sensor_addr8_write8(client, 0x5F, 0x00);
	ret |= is_sensor_addr8_write8(client, 0x60, 0x00);
	ret |= is_sensor_addr8_write8(client, 0x62, 0x0D);
	ret |= is_sensor_addr8_write8(client, 0x63, 0x85);
	ret |= is_sensor_addr8_write8(client, 0x64, 0x02);
	ret |= is_sensor_addr8_write8(client, 0x65, 0x7A);
	ret |= is_sensor_addr8_write8(client, 0x66, 0x00);
	ret |= is_sensor_addr8_write8(client, 0x67, 0x00);
	ret |= is_sensor_addr8_write8(client, 0x68, 0x80);
	ret |= is_sensor_addr8_write8(client, 0x69, 0x80);
	ret |= is_sensor_addr8_write8(client, 0x6A, 0x80);
	ret |= is_sensor_addr8_write8(client, 0x6B, 0x00);
	/* =============  Calibration ===================*/
	ret |= is_sensor_addr8_write8(client, 0x03, 0x01);
	ret |= is_sensor_addr8_write8(client, 0x04, 0x04);
	ret |= is_sensor_addr8_write8(client, 0x03, 0x11);
	msleep(500);
	/* ===========  data store ===================*/
	ret |= is_sensor_addr8_write8(client, 0x03, 0x01);
	ret |= is_sensor_addr8_write8(client, 0x78, 0x01);
	msleep(400);
	ret |= is_sensor_addr8_write8(client, 0x05, 0x01);

	/* goto standby mode */
	usleep_range(1500, 1510);
	ret |= is_sensor_addr8_write8(client, 0x03, 0x01);
	ret |= is_sensor_addr8_write8(client, 0x04, 0x00);

	actuator->init_cal_setting = true;
#endif

skip_cal:
	/* set Active Mode */
	ret = is_sensor_addr8_write8(client, REG_ACTIVE_EN, 0x01);
	ret = is_sensor_addr8_write8(client, 0x40, 0xF1);
	ret = is_sensor_addr8_write8(client, 0x54, 0x0E);
	ret = is_sensor_addr8_write8(client, REG_ACTIVE_EN, 0x11);
	if (ret < 0)
		goto p_err;

	/* delay after active mode */
	usleep_range(3000, 3010);
	ret = is_sensor_addr8_write8(client, REG_ACT_MODE, 0x00);
	if (ret < 0)
		goto p_err;
	/* read pcal, ncal */
	actuator_info = (struct dw9839_actuator_info *)actuator->priv_info;
	ret = is_sensor_addr8_read8(client, REG_PCAL_MSB, &pcal_msb);
	if (ret < 0)
		goto p_err;
	ret = is_sensor_addr8_read8(client, REG_PCAL_LSB, &pcal_lsb);
	if (ret < 0)
		goto p_err;
	ret = is_sensor_addr8_read8(client, REG_NCAL_MSB, &ncal_msb);
	if (ret < 0)
		goto p_err;
	ret = is_sensor_addr8_read8(client, REG_NCAL_LSB, &ncal_lsb);
	if (ret < 0)
		goto p_err;

	actuator_info->pcal = (pcal_msb << 8) | pcal_lsb;
	actuator_info->ncal = (ncal_msb << 8) | ncal_lsb;

	info("%s done\n", __func__);
p_err:
	return ret;
}

static int sensor_dw9839_write_position(struct i2c_client *client, u32 val)
{
	int ret = 0;
	u8 val_msb, val_lsb;

	FIMC_BUG(!client);

	if (!client->adapter) {
		err("Could not find adapter!\n");
		return -ENODEV;
	}

	if (val > DW9839_POS_MAX_SIZE) {
		err("Invalid af position(position : %d, Max : %d).\n",
					val, DW9839_POS_MAX_SIZE);
		return -EINVAL;
	}

	val_msb = (val >> 2) & 0x00FF;
	val_lsb = (val & 0x3) << 6;

	ret = is_sensor_addr8_write8(client, REG_TARGET_MSB, val_msb);
	if (ret < 0)
		goto p_err;
	ret = is_sensor_addr8_write8(client, REG_TARGET_LSB, val_lsb);
	if (ret < 0)
		goto p_err;

p_err:
	return ret;
}

static int sensor_dw9839_valid_check(struct i2c_client *client)
{
	int i;

	FIMC_BUG(!client);

	if (sysfs_actuator.init_step > 0) {
		for (i = 0; i < sysfs_actuator.init_step; i++) {
			if (sysfs_actuator.init_positions[i] < 0) {
				warn("invalid position value, default setting to position");
				return 0;
			} else if (sysfs_actuator.init_delays[i] < 0) {
				warn("invalid delay value, default setting to delay");
				return 0;
			}
		}
	} else
		return 0;

	return sysfs_actuator.init_step;
}

static void sensor_dw9839_print_log(int step)
{
	int i;

	if (step > 0) {
		dbg_actuator("initial position ");
		for (i = 0; i < step; i++)
			dbg_actuator(" %d", sysfs_actuator.init_positions[i]);
		dbg_actuator(" setting");
	}
}

static int sensor_dw9839_init_position(struct i2c_client *client,
		struct is_actuator *actuator)
{
	int i;
	int ret = 0;
	int init_step = 0;

	init_step = sensor_dw9839_valid_check(client);

	if (init_step > 0) {
		for (i = 0; i < init_step; i++) {
			ret = sensor_dw9839_write_position(client, sysfs_actuator.init_positions[i]);
			if (ret < 0)
				goto p_err;

			msleep(sysfs_actuator.init_delays[i]);
		}

		actuator->position = sysfs_actuator.init_positions[i];

		sensor_dw9839_print_log(init_step);

	} else {
		/* REG_INIT_POS range is 0 ~ 255 (1/4 of 0 ~ 1023 position) */
		ret = is_sensor_addr8_write8(client, REG_INIT_POS, DEF_DW9839_INIT_POSITION >> 2);
		if (ret < 0)
			goto p_err;

		actuator->position = DEF_DW9839_INIT_POSITION;

		dbg_actuator("initial position %d setting\n", DEF_DW9839_INIT_POSITION);
	}

p_err:
	return ret;
}

int sensor_dw9839_actuator_init(struct v4l2_subdev *subdev, u32 val)
{
	int ret = 0;
	struct is_actuator *actuator;
	struct i2c_client *client = NULL;
#ifdef DEBUG_ACTUATOR_TIME
	struct timeval st, end;

	do_gettimeofday(&st);
#endif

	FIMC_BUG(!subdev);

	dbg_actuator("%s\n", __func__);

	actuator = (struct is_actuator *)v4l2_get_subdevdata(subdev);
	if (!actuator) {
		err("actuator is not detect!\n");
		return -ENODEV;
	}

	client = actuator->client;
	if (unlikely(!client)) {
		err("client is NULL");
		return -EINVAL;
	}

	I2C_MUTEX_LOCK(actuator->i2c_lock);

	ret = sensor_dw9839_init(actuator);
	if (ret < 0)
		goto p_err;

	ret = sensor_dw9839_init_position(client, actuator);
	if (ret < 0)
		goto p_err;

#ifdef DEBUG_ACTUATOR_TIME
	do_gettimeofday(&end);
	pr_info("[%s] time %lu us", __func__, (end.tv_sec - st.tv_sec) * 1000000 + (end.tv_usec - st.tv_usec));
#endif

	/* dw9839 actuator do not use af cal */
	actuator->actuator_data.actuator_init = false;
p_err:
	I2C_MUTEX_UNLOCK(actuator->i2c_lock);

	return ret;
}

int sensor_dw9839_actuator_get_status(struct v4l2_subdev *subdev, u32 *info)
{
	int ret = 0;
	u32 status = 0;
	struct is_actuator *actuator = NULL;
	struct i2c_client *client = NULL;
#ifdef DEBUG_ACTUATOR_TIME
	struct timeval st, end;

	do_gettimeofday(&st);
#endif

	dbg_actuator("%s\n", __func__);

	FIMC_BUG(!subdev);
	FIMC_BUG(!info);

	actuator = (struct is_actuator *)v4l2_get_subdevdata(subdev);
	FIMC_BUG(!actuator);

	client = actuator->client;
	if (unlikely(!client)) {
		err("client is NULL");
		ret = -EINVAL;
		goto p_err;
	}

	/*
	 * The info is busy flag.
	 * But, this module can't get busy flag.
	 */
	status = ACTUATOR_STATUS_NO_BUSY;
	*info = status;

#ifdef DEBUG_ACTUATOR_TIME
	do_gettimeofday(&end);
	pr_info("[%s] time %lu us", __func__, (end.tv_sec - st.tv_sec) * 1000000 + (end.tv_usec - st.tv_usec));
#endif

p_err:
	return ret;
}

int sensor_dw9839_actuator_set_position(struct v4l2_subdev *subdev, u32 *info)
{
	int ret = 0;
	struct is_actuator *actuator;
	struct i2c_client *client;
	u32 position = 0;
#ifdef DEBUG_ACTUATOR_TIME
	struct timeval st, end;

	do_gettimeofday(&st);
#endif

	FIMC_BUG(!subdev);
	FIMC_BUG(!info);

	actuator = (struct is_actuator *)v4l2_get_subdevdata(subdev);
	FIMC_BUG(!actuator);

	client = actuator->client;
	if (unlikely(!client)) {
		err("client is NULL");
		return -EINVAL;
	}

	position = *info;
	if (position > DW9839_POS_MAX_SIZE) {
		err("Invalid af position(position : %d, Max : %d).\n",
					position, DW9839_POS_MAX_SIZE);
		return -EINVAL;
	}

	/* debug option : fixed position testing */
	if (sysfs_actuator.enable_fixed)
		position = sysfs_actuator.fixed_position;

	I2C_MUTEX_LOCK(actuator->i2c_lock);

	/* position Set */
	ret = sensor_dw9839_write_position(client, position);
	if (ret < 0)
		goto p_err;
	actuator->position = position;

	dbg_actuator("%s: position(%d)\n", __func__, position);

#ifdef DEBUG_ACTUATOR_TIME
	do_gettimeofday(&end);
	pr_info("[%s] time %lu us", __func__, (end.tv_sec - st.tv_sec) * 1000000 + (end.tv_usec - st.tv_usec));
#endif
p_err:
	I2C_MUTEX_UNLOCK(actuator->i2c_lock);

	return ret;
}

int sensor_dw9839_actuator_get_actual_position(struct v4l2_subdev *subdev, u32 *info)
{
	int ret = 0;
	struct is_actuator *actuator;
	struct i2c_client *client;
	struct dw9839_actuator_info *actuator_info;
	u8 pos_msb = 0, pos_lsb = 0;
	u32 adc_pos;
	u64 temp;

#ifdef DEBUG_ACTUATOR_TIME
	struct timeval st, end;

	do_gettimeofday(&st);
#endif

	FIMC_BUG(!subdev);
	FIMC_BUG(!info);

	actuator = (struct is_actuator *)v4l2_get_subdevdata(subdev);
	FIMC_BUG(!actuator);

	client = actuator->client;
	if (unlikely(!client)) {
		err("client is NULL");
		return -EINVAL;
	}

	FIMC_BUG(!actuator->priv_info);
	actuator_info = (struct dw9839_actuator_info *)actuator->priv_info;

	I2C_MUTEX_LOCK(actuator->i2c_lock);

	ret = is_sensor_addr8_write8(client, REG_ADC_R_EN, 1);
	if (ret < 0)
		goto p_err;
	ret = is_sensor_addr8_read8(client, REG_ADC_R_MSB, &pos_msb);
	if (ret < 0)
		goto p_err;
	ret = is_sensor_addr8_read8(client, REG_ADC_R_LSB, &pos_lsb);
	if (ret < 0)
		goto p_err;

	/* pos_msb uses [1:0] bit */
	adc_pos = ((pos_msb & 0x3) << 8) | pos_lsb;

	/* convert adc_pos to 10bit position
	 * ncal <= adc_pos <= pcal ------> 0 <= 10bit_pos <= 1023
	 */
	temp = (u64)(adc_pos - actuator_info->ncal) << ACTUATOR_POS_SIZE_10BIT;
	*info = (u32)(temp / (u64)(actuator_info->pcal - actuator_info->ncal));

	if (*info > 1023)
		*info = 1023;

	dbg_actuator("%s: cal(p:%d, n:%d), adc_pos(msb:%d, lsb:%d, sum:%d) -> target_pos(%d) actual pos(%d)\n",
			__func__, actuator_info->pcal, actuator_info->ncal, pos_msb & 0x3, pos_lsb, adc_pos,
			actuator->position, *info);

#ifdef DEBUG_ACTUATOR_TIME
	do_gettimeofday(&end);
	pr_info("[%s] time %lu us", __func__, (end.tv_sec - st.tv_sec) * 1000000 + (end.tv_usec - st.tv_usec));
#endif

p_err:
	I2C_MUTEX_UNLOCK(actuator->i2c_lock);

	return ret;
}

static int sensor_dw9839_actuator_g_ctrl(struct v4l2_subdev *subdev, struct v4l2_control *ctrl)
{
	int ret = 0;
	u32 val = 0;

	switch (ctrl->id) {
	case V4L2_CID_ACTUATOR_GET_STATUS:
		ret = sensor_dw9839_actuator_get_status(subdev, &val);
		if (ret < 0) {
			err("err!!! ret(%d), actuator status(%d)", ret, val);
			ret = -EINVAL;
			goto p_err;
		}
		break;
	case V4L2_CID_ACTUATOR_GET_ACTUAL_POSITION:
		ret = sensor_dw9839_actuator_get_actual_position(subdev, &val);
		if (ret < 0) {
			err("sensor_dw9839_get_actual_position failed(%d)", ret);
			ret = -EINVAL;
			goto p_err;
		}
		break;
	default:
		err("err!!! Unknown CID(%#x)", ctrl->id);
		ret = -EINVAL;
		goto p_err;
	}

	ctrl->value = val;

p_err:
	return ret;
}

static int sensor_dw9839_actuator_s_ctrl(struct v4l2_subdev *subdev, struct v4l2_control *ctrl)
{
	int ret = 0;

	switch (ctrl->id) {
	case V4L2_CID_ACTUATOR_SET_POSITION:
		ret = sensor_dw9839_actuator_set_position(subdev, &ctrl->value);
		if (ret) {
			err("failed to actuator set position: %d, (%d)\n", ctrl->value, ret);
			ret = -EINVAL;
			goto p_err;
		}
		break;
	default:
		err("err!!! Unknown CID(%#x)", ctrl->id);
		ret = -EINVAL;
		goto p_err;
	}

p_err:
	return ret;
}

long sensor_dw9839_actuator_ioctl(struct v4l2_subdev *subdev, unsigned int cmd, void *arg)
{
	int ret = 0;
	struct v4l2_control *ctrl;

	ctrl = (struct v4l2_control *)arg;
	switch (cmd) {
	case SENSOR_IOCTL_ACT_S_CTRL:
		ret = sensor_dw9839_actuator_s_ctrl(subdev, ctrl);
		if (ret) {
			err("err!!! actuator_s_ctrl failed(%d)", ret);
			goto p_err;
		}
		break;
	case SENSOR_IOCTL_ACT_G_CTRL:
		ret = sensor_dw9839_actuator_g_ctrl(subdev, ctrl);
		if (ret) {
			err("err!!! actuator_g_ctrl failed(%d)", ret);
			goto p_err;
		}
		break;
	default:
		err("err!!! Unknown command(%#x)", cmd);
		ret = -EINVAL;
		goto p_err;
	}
p_err:
	return (long)ret;
}

static const struct v4l2_subdev_core_ops core_ops = {
	.init = sensor_dw9839_actuator_init,
	.ioctl = sensor_dw9839_actuator_ioctl,
};

static const struct v4l2_subdev_ops subdev_ops = {
	.core = &core_ops,
};

static int sensor_dw9839_actuator_probe(struct i2c_client *client,
		const struct i2c_device_id *id)
{
	int ret = 0;
	struct is_core *core = NULL;
	struct v4l2_subdev *subdev_actuator = NULL;
	struct is_actuator *actuator = NULL;
	struct is_device_sensor *device = NULL;
	struct is_device_sensor_peri *sensor_peri = NULL;
	u32 sensor_id = 0;
	u32 place = 0;
	struct device *dev;
	struct device_node *dnode;

	FIMC_BUG(!is_dev);
	FIMC_BUG(!client);

	core = (struct is_core *)dev_get_drvdata(is_dev);
	if (!core) {
		err("core device is not yet probed");
		ret = -EPROBE_DEFER;
		goto p_err;
	}

	dev = &client->dev;
	dnode = dev->of_node;

	ret = of_property_read_u32(dnode, "id", &sensor_id);
	if (ret) {
		err("id read is fail(%d)", ret);
		goto p_err;
	}

	ret = of_property_read_u32(dnode, "place", &place);
	if (ret) {
		pr_info("place read is fail(%d)", ret);
		place = 0;
	}
	probe_info("%s sensor_id(%d) actuator_place(%d)\n", __func__, sensor_id, place);

	device = &core->sensor[sensor_id];
	if (!test_bit(IS_SENSOR_PROBE, &device->state)) {
		err("sensor device is not yet probed");
		ret = -EPROBE_DEFER;
		goto p_err;
	}

	sensor_peri = find_peri_by_act_id(device, ACTUATOR_NAME_DW9839);
	if (!sensor_peri) {
		probe_info("sensor peri is net yet probed");
		return -EPROBE_DEFER;
	}

	actuator = kzalloc(sizeof(struct is_actuator), GFP_KERNEL);
	if (!actuator) {
		err("acuator is NULL");
		ret = -ENOMEM;
		goto p_err;
	}

	subdev_actuator = kzalloc(sizeof(struct v4l2_subdev), GFP_KERNEL);
	if (!subdev_actuator) {
		err("subdev_actuator is NULL");
		ret = -ENOMEM;
		goto p_err;
	}
	sensor_peri->subdev_actuator = subdev_actuator;

	/* This name must is match to sensor_open_extended actuator name */
	actuator->id = ACTUATOR_NAME_DW9839;
	actuator->subdev = subdev_actuator;
	actuator->device = sensor_id;
	actuator->client = client;
	actuator->position = 0;
	actuator->max_position = DW9839_POS_MAX_SIZE;
	actuator->pos_size_bit = DW9839_POS_SIZE_BIT;
	actuator->pos_direction = DW9839_POS_DIRECTION;
	actuator->init_cal_setting = false;
	actuator->actual_pos_support = true;

	actuator->priv_info = vzalloc(sizeof(struct dw9839_actuator_info));
	if (!actuator->priv_info) {
		err("actuator->priv_info alloc fail");
		ret = -ENOMEM;
		goto p_err;
	}

	device->subdev_actuator[sensor_id] = subdev_actuator;
	device->actuator[sensor_id] = actuator;

	v4l2_i2c_subdev_init(subdev_actuator, client, &subdev_ops);
	v4l2_set_subdevdata(subdev_actuator, actuator);
	v4l2_set_subdev_hostdata(subdev_actuator, device);

	set_bit(IS_SENSOR_ACTUATOR_AVAILABLE, &sensor_peri->peri_state);

	snprintf(subdev_actuator->name, V4L2_SUBDEV_NAME_SIZE, "actuator-subdev.%d", actuator->id);

	probe_info("%s done\n", __func__);
	return ret;

p_err:
	if (actuator)
		kzfree(actuator);

	if (subdev_actuator)
		kzfree(subdev_actuator);

	return ret;
}

static const struct of_device_id sensor_actuator_dw9839_match[] = {
	{
		.compatible = "samsung,exynos-is-actuator-dw9839",
	},
	{},
};
MODULE_DEVICE_TABLE(of, sensor_actuator_dw9839_match);

static const struct i2c_device_id sensor_actuator_dw9839_idt[] = {
	{ ACTUATOR_NAME, 0 },
	{},
};

static struct i2c_driver sensor_actuator_dw9839_driver = {
	.probe  = sensor_dw9839_actuator_probe,
	.driver = {
		.name	= ACTUATOR_NAME,
		.owner	= THIS_MODULE,
		.of_match_table = sensor_actuator_dw9839_match,
		.suppress_bind_attrs = true,
	},
	.id_table = sensor_actuator_dw9839_idt,
};

static int __init sensor_actuator_dw9839_init(void)
{
	int ret;

	ret = i2c_add_driver(&sensor_actuator_dw9839_driver);
	if (ret)
		err("failed to add %s driver: %d\n",
			sensor_actuator_dw9839_driver.driver.name, ret);

	return ret;
}
late_initcall_sync(sensor_actuator_dw9839_init);

MODULE_LICENSE("GPL");
