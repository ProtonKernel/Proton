/*
 * Samsung Exynos SoC series Flash driver
 *
 *
 * Copyright (c) 2019 Samsung Electronics Co., Ltd
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#include <linux/gpio.h>
#include <linux/slab.h>
#include <linux/module.h>
#include <linux/moduleparam.h>
#include <linux/platform_device.h>
#include <linux/of_gpio.h>

#include <linux/videodev2.h>
#include <linux/videodev2_exynos_camera.h>

#include "is-device-sensor.h"
#include "is-device-sensor-peri.h"
#include "is-core.h"

extern int s2mu107_led_mode_ctrl(int state);

enum s2mu107_flash_mode {
	S2MU107_FLED_MODE_OFF,
	S2MU107_FLED_MODE_TORCH,
	S2MU107_FLED_MODE_FLASH,
	S2MU107_FLED_MODE_MAX,
};

static int flash_s2mu107_init(struct v4l2_subdev *subdev, u32 val)
{
	int ret = 0;
	struct is_flash *flash;

	FIMC_BUG(!subdev);

	flash = (struct is_flash *)v4l2_get_subdevdata(subdev);

	FIMC_BUG(!flash);

	/* TODO: init flash driver */
	flash->flash_data.mode = CAM2_FLASH_MODE_OFF;
	flash->flash_data.intensity = 100; /* TODO: Need to figure out min/max range */
	flash->flash_data.firing_time_us = 1 * 1000 * 1000; /* Max firing time is 1sec */
	flash->flash_data.flash_fired = false;

	s2mu107_led_mode_ctrl(0);

	return ret;
}

static int sensor_s2mu107_flash_control(struct v4l2_subdev *subdev, enum flash_mode mode, u32 intensity)
{
	int ret = 0;
	struct is_flash *flash = NULL;

	FIMC_BUG(!subdev);

	flash = (struct is_flash *)v4l2_get_subdevdata(subdev);
	FIMC_BUG(!flash);

	dbg_flash("%s : mode = %s, intensity = %d\n", __func__,
		mode == CAM2_FLASH_MODE_OFF ? "OFF" :
		mode == CAM2_FLASH_MODE_SINGLE ? "FLASH" : "TORCH",
		intensity);

	if (mode == CAM2_FLASH_MODE_OFF) {
		ret = s2mu107_led_mode_ctrl(S2MU107_FLED_MODE_OFF);
		if (ret)
			err("torch/flash off fail");
	} else if (mode == CAM2_FLASH_MODE_SINGLE) {
		ret = s2mu107_led_mode_ctrl(S2MU107_FLED_MODE_FLASH);
		if (ret)
			err("capture flash on fail");
	} else if (mode == CAM2_FLASH_MODE_TORCH) {
		ret = s2mu107_led_mode_ctrl(S2MU107_FLED_MODE_TORCH);
		if (ret)
			err("torch flash on fail");
	} else {
		err("Invalid flash mode");
		ret = -EINVAL;
		goto p_err;
	}

p_err:
	return ret;
}

int flash_s2mu107_s_ctrl(struct v4l2_subdev *subdev, struct v4l2_control *ctrl)
{
	int ret = 0;
	struct is_flash *flash = NULL;

	FIMC_BUG(!subdev);

	flash = (struct is_flash *)v4l2_get_subdevdata(subdev);
	FIMC_BUG(!flash);

	switch (ctrl->id) {
	case V4L2_CID_FLASH_SET_INTENSITY:
		/* TODO : Check min/max intensity */
		if (ctrl->value < 0) {
			err("failed to flash set intensity: %d\n", ctrl->value);
			ret = -EINVAL;
			goto p_err;
		}
		flash->flash_data.intensity = ctrl->value;
		break;
	case V4L2_CID_FLASH_SET_FIRING_TIME:
		/* TODO : Check min/max firing time */
		if (ctrl->value < 0) {
			err("failed to flash set firing time: %d\n", ctrl->value);
			ret = -EINVAL;
			goto p_err;
		}
		flash->flash_data.firing_time_us = ctrl->value;
		break;
	case V4L2_CID_FLASH_SET_FIRE:
		ret =  sensor_s2mu107_flash_control(subdev, flash->flash_data.mode, ctrl->value);
		if (ret) {
			err("sensor_s2mu107_flash_control(mode:%d, val:%d) is fail(%d)",
					(int)flash->flash_data.mode, ctrl->value, ret);
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

long flash_s2mu107_ioctl(struct v4l2_subdev *subdev, unsigned int cmd, void *arg)
{
	int ret = 0;
	struct v4l2_control *ctrl;

	ctrl = (struct v4l2_control *)arg;
	switch (cmd) {
	case SENSOR_IOCTL_FLS_S_CTRL:
		ret = flash_s2mu107_s_ctrl(subdev, ctrl);
		if (ret) {
			err("err!!! flash_gpio_s_ctrl failed(%d)", ret);
			goto p_err;
		}
		break;
	case SENSOR_IOCTL_FLS_G_CTRL:
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
	.init = flash_s2mu107_init,
	.ioctl = flash_s2mu107_ioctl,
};

static const struct v4l2_subdev_ops subdev_ops = {
	.core = &core_ops,
};

static int __init flash_s2mu107_probe(struct device *dev, struct i2c_client *client)
{
	int ret = 0;
	struct is_core *core;
	struct v4l2_subdev *subdev_flash;
	struct is_device_sensor *device;
	struct is_flash *flash;
	u32 sensor_id = 0;
	struct device_node *dnode;

	FIMC_BUG(!is_dev);
	FIMC_BUG(!dev);

	dnode = dev->of_node;

	ret = of_property_read_u32(dnode, "id", &sensor_id);
	if (ret) {
		err("id read is fail(%d)", ret);
		goto p_err;
	}

	core = (struct is_core *)dev_get_drvdata(is_dev);
	if (!core) {
		probe_info("core device is not yet probed");
		ret = -EPROBE_DEFER;
		goto p_err;
	}

	device = &core->sensor[sensor_id];

	flash = kzalloc(sizeof(struct is_flash), GFP_KERNEL);
	if (!flash) {
		err("flash is NULL");
		ret = -ENOMEM;
		goto p_err;
	}

	subdev_flash = kzalloc(sizeof(struct v4l2_subdev), GFP_KERNEL);
	if (!subdev_flash) {
		err("subdev_flash is NULL");
		ret = -ENOMEM;
		kfree(flash);
		goto p_err;
	}

	flash->id = FLADRV_NAME_S2MU107;
	flash->subdev = subdev_flash;
	flash->client = client;

	flash->flash_data.mode = CAM2_FLASH_MODE_OFF;
	flash->flash_data.intensity = 100; /* TODO: Need to figure out min/max range */
	flash->flash_data.firing_time_us = 1 * 1000 * 1000; /* Max firing time is 1sec */

	device->subdev_flash = subdev_flash;
	device->flash = flash;

	v4l2_subdev_init(subdev_flash, &subdev_ops);

	v4l2_set_subdevdata(subdev_flash, flash);
	v4l2_set_subdev_hostdata(subdev_flash, device);
	snprintf(subdev_flash->name, V4L2_SUBDEV_NAME_SIZE, "flash-subdev.%d", flash->id);

p_err:
	return ret;
}

static int __init flash_s2mu107_platform_probe(struct platform_device *pdev)
{
	int ret = 0;
	struct device *dev;

	FIMC_BUG(!pdev);

	dev = &pdev->dev;

	ret = flash_s2mu107_probe(dev, NULL);
	if (ret < 0) {
		probe_err("flash s2mu107 probe fail(%d)\n", ret);
		goto p_err;
	}

	probe_info("%s done\n", __func__);

p_err:
	return ret;
}

static const struct of_device_id exynos_is_sensor_flash_s2mu107_match[] = {
	{
		.compatible = "samsung,sensor-flash-s2mu107",
	},
};
MODULE_DEVICE_TABLE(of, exynos_is_sensor_flash_s2mu107_match);

/* register platform driver */
static struct platform_driver sensor_flash_s2mu107_platform_driver = {
	.driver = {
		.name   = "FIMC-IS-SENSOR-FLASH-S2MU107-PLATFORM",
		.owner  = THIS_MODULE,
		.of_match_table = exynos_is_sensor_flash_s2mu107_match,
	}
};

static int __init is_sensor_flash_s2mu107_init(void)
{
	int ret;

	ret = platform_driver_probe(&sensor_flash_s2mu107_platform_driver,
				flash_s2mu107_platform_probe);
	if (ret)
		err("failed to probe %s driver: %d\n",
			sensor_flash_s2mu107_platform_driver.driver.name, ret);

	return ret;
}
late_initcall_sync(is_sensor_flash_s2mu107_init);
