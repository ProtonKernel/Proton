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

#include <linux/leds-s2mu106.h>

#define CAPTURE_MAX_TOTAL_CURRENT	(1500)
#define TORCH_MAX_TOTAL_CURRENT		(150)
#define MAX_FLASH_INTENSITY		(256)

static int flash_s2mu106_init(struct v4l2_subdev *subdev, u32 val)
{
	int ret = 0;
	struct is_flash *flash;
	int i;

	FIMC_BUG(!subdev);

	flash = (struct is_flash *)v4l2_get_subdevdata(subdev);

	FIMC_BUG(!flash);

	/* TODO: init flash driver */
	flash->flash_data.mode = CAM2_FLASH_MODE_OFF;
	flash->flash_data.intensity = 100; /* TODO: Need to figure out min/max range */
	flash->flash_data.firing_time_us = 1 * 1000 * 1000; /* Max firing time is 1sec */
	flash->flash_data.flash_fired = false;
	flash->flash_data.led_cal_en = false;

	for (i = 0; i < FLASH_LED_CH_MAX; i++) {
		if (flash->led_ch[i] >= 0)
			s2mu106_fled_set_mode_ctrl(flash->led_ch[i], CAM_FLASH_MODE_OFF);
	}

	return ret;
}

static int flash_s2mu106_adj_current(struct is_flash *flash, enum flash_mode mode, u32 intensity)
{
	int adj_current = 0;
	int max_current = 0;
	int led_num = 0;
	int i;

	for (i = 0; i < FLASH_LED_CH_MAX; i++) {
		if (flash->led_ch[i] != -1)
			led_num++;
	}

	if (led_num == 0) {
		err("wrong flash led number, set to 0");
		return 0;
	}

	if (mode == CAM2_FLASH_MODE_SINGLE)
		max_current = CAPTURE_MAX_TOTAL_CURRENT;
	else if (mode == CAM2_FLASH_MODE_TORCH)
		max_current = TORCH_MAX_TOTAL_CURRENT;
	else
		return 0;

#ifdef FLASH_CAL_DATA_ENABLE
	if (led_num > 2)
		warn("Num of LED is over 2: %d\n", led_num);

	led_num = 0;

	if (flash->flash_data.led_cal_en == false) {
	/* flash or torch set by ddk */
		adj_current = ((max_current * intensity) / MAX_FLASH_INTENSITY);

		if (adj_current > max_current) {
			warn("flash intensity(%d) > max(%d), set to max forcely",
					adj_current, max_current);
			adj_current = max_current;
		}

		for (i = 0; i < FLASH_LED_CH_MAX; i++) {
			if (flash->led_ch[i] == -1)
				continue;

			led_num++;
			if (led_num == 1) {
				flash->flash_data.cal_input_curr[i] = adj_current;
				dbg_flash("[CH: %d] Flash set with adj_current: %d\n",
						flash->led_ch[i], flash->flash_data.cal_input_curr[i]);
			} else if (led_num == 2) {
				flash->flash_data.cal_input_curr[i] = max_current - adj_current;
				dbg_flash("[CH: %d] Flash set with adj_current: %d\n",
						flash->led_ch[i], flash->flash_data.cal_input_curr[i]);
			} else {
				warn("skip set current value\n");
			}
		}
	} else {
	/* flash or torch set by hal */
		for (i = 0; i < FLASH_LED_CH_MAX; i++) {
			if (flash->led_ch[i] != -1) {
				led_num++;
				adj_current = flash->flash_data.cal_input_curr[i];
				if (adj_current > max_current) {
					warn("flash intensity(%d) > max(%d), set to max forcely",
							adj_current, max_current);
					flash->flash_data.cal_input_curr[i] = max_current;
				}
				dbg_flash("[CH: %d] Flash set with adj_current: %d\n",
						flash->led_ch[i], flash->flash_data.cal_input_curr[i]);
			}
		}
	}

	dbg_flash("%s: mode: %s, led_numt: %d\n", __func__,
		mode == CAM2_FLASH_MODE_OFF ? "OFF" :
		mode == CAM2_FLASH_MODE_SINGLE ? "FLASH" : "TORCH",
		led_num);

	return 0;
#else
	if (intensity > MAX_FLASH_INTENSITY) {
		warn("flash intensity(%d) > max(%d), set to max forcely",
				intensity, MAX_FLASH_INTENSITY);
		intensity = MAX_FLASH_INTENSITY;
	}

	adj_current = ((max_current * intensity) / MAX_FLASH_INTENSITY) / led_num;

	dbg_flash("%s: mode: %s, adj_current: %d\n", __func__,
		mode == CAM2_FLASH_MODE_OFF ? "OFF" :
		mode == CAM2_FLASH_MODE_SINGLE ? "FLASH" : "TORCH",
		adj_current);

	return adj_current;
#endif
}

static int flash_s2mu106_control(struct v4l2_subdev *subdev, enum flash_mode mode, u32 intensity)
{
	int ret = 0;
	struct is_flash *flash = NULL;
	int i;
	int adj_current = 0;

	FIMC_BUG(!subdev);

	flash = (struct is_flash *)v4l2_get_subdevdata(subdev);
	FIMC_BUG(!flash);

	dbg_flash("%s : mode = %s, intensity = %d\n", __func__,
		mode == CAM2_FLASH_MODE_OFF ? "OFF" :
		mode == CAM2_FLASH_MODE_SINGLE ? "FLASH" : "TORCH",
		intensity);

	adj_current = flash_s2mu106_adj_current(flash, mode, intensity);

	for (i = 0; i < FLASH_LED_CH_MAX; i++) {
		if (flash->led_ch[i] == -1)
			continue;
#ifdef FLASH_CAL_DATA_ENABLE
		adj_current = flash->flash_data.cal_input_curr[i];

		/* If adj_current value is zero, it must be skipped to set */
		/* Even if zero is set to flash, 50mA will flow, because 50mA is minimized value */
		if (adj_current == 0 && mode != CAM2_FLASH_MODE_OFF) {
			dbg_flash("[CH: %d] current value is 0, so current set need to skip\n", flash->led_ch[i]);
			continue;
		}

		dbg_flash("[CH: %d] current is set with val: %d\n", flash->led_ch[i], adj_current);
#endif
		switch (mode) {
		case CAM2_FLASH_MODE_OFF:
			ret = s2mu106_fled_set_mode_ctrl(flash->led_ch[i], CAM_FLASH_MODE_OFF);
			if (ret < 0) {
				err("flash off fail(led_ch:%d)", flash->led_ch[i]);
				ret = -EINVAL;
			}
			break;
		case CAM2_FLASH_MODE_SINGLE:
			ret = s2mu106_fled_set_curr(flash->led_ch[i], CAM_FLASH_MODE_SINGLE, adj_current);
			if (ret < 0) {
				err("capture flash set current fail(led_ch:%d)", flash->led_ch[i]);
				ret = -EINVAL;
			}

			ret |= s2mu106_fled_set_mode_ctrl(flash->led_ch[i], CAM_FLASH_MODE_SINGLE);
			if (ret < 0) {
				err("capture flash on fail(led_ch:%d)", flash->led_ch[i]);
				ret = -EINVAL;
			}
			break;
		case CAM2_FLASH_MODE_TORCH:
			ret = s2mu106_fled_set_curr(flash->led_ch[i], CAM_FLASH_MODE_TORCH, adj_current);
			if (ret < 0) {
				err("torch flash set current fail(led_ch:%d)", flash->led_ch[i]);
				ret = -EINVAL;
			}

			ret |= s2mu106_fled_set_mode_ctrl(flash->led_ch[i], CAM_FLASH_MODE_TORCH);
			if (ret < 0) {
				err("torch flash on fail(led_ch:%d)", flash->led_ch[i]);
				ret = -EINVAL;
			}
			break;
		default:
			err("Invalid flash mode");
			ret = -EINVAL;
			goto p_err;
		}
	}

p_err:
	return ret;
}

int flash_s2mu106_s_ctrl(struct v4l2_subdev *subdev, struct v4l2_control *ctrl)
{
	int ret = 0;
	struct is_flash *flash = NULL;

	FIMC_BUG(!subdev);

	flash = (struct is_flash *)v4l2_get_subdevdata(subdev);
	FIMC_BUG(!flash);

	switch (ctrl->id) {
#ifdef FLASH_CAL_DATA_ENABLE
	case V4L2_CID_FLASH_SET_CAL_EN:
		if (ctrl->value < 0) {
			err("failed to flash set cal_en: %d\n", ctrl->value);
			ret = -EINVAL;
			goto p_err;
		}
		flash->flash_data.led_cal_en = ctrl->value;
		dbg_flash("led_cal_en ctrl set: %s\n", (flash->flash_data.led_cal_en ? "enable" : "disable"));
		break;
	case V4L2_CID_FLASH_SET_BY_CAL_CH0:
		if (ctrl->value < 0) {
			err("[ch0] failed to flash set current val by cal: %d\n", ctrl->value);
			ret = -EINVAL;
			goto p_err;
		}
		flash->flash_data.cal_input_curr[0] = ctrl->value;
		break;
	case V4L2_CID_FLASH_SET_BY_CAL_CH1:
		if (ctrl->value < 0) {
			err("[ch1] failed to flash set current val by cal: %d\n", ctrl->value);
			ret = -EINVAL;
			goto p_err;
		}
		flash->flash_data.cal_input_curr[1] = ctrl->value;
		break;
#else
	case V4L2_CID_FLASH_SET_INTENSITY:
		/* TODO : Check min/max intensity */
		if (ctrl->value < 0) {
			err("failed to flash set intensity: %d\n", ctrl->value);
			ret = -EINVAL;
			goto p_err;
		}
		flash->flash_data.intensity = ctrl->value;
		break;
#endif
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
		ret =  flash_s2mu106_control(subdev, flash->flash_data.mode, ctrl->value);
		if (ret) {
			err("flash_s2mu106_control(mode:%d, val:%d) is fail(%d)",
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

long flash_s2mu106_ioctl(struct v4l2_subdev *subdev, unsigned int cmd, void *arg)
{
	int ret = 0;
	struct v4l2_control *ctrl;

	ctrl = (struct v4l2_control *)arg;
	switch (cmd) {
	case SENSOR_IOCTL_FLS_S_CTRL:
		ret = flash_s2mu106_s_ctrl(subdev, ctrl);
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
	.init = flash_s2mu106_init,
	.ioctl = flash_s2mu106_ioctl,
};

static const struct v4l2_subdev_ops subdev_ops = {
	.core = &core_ops,
};

static int __init flash_s2mu106_probe(struct device *dev, struct i2c_client *client)
{
	int ret = 0;
	struct is_core *core;
	struct v4l2_subdev *subdev_flash;
	struct is_device_sensor *device;
	struct is_flash *flash;
	u32 sensor_id = 0;
	struct device_node *dnode;
	int i, elements;

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

	flash->id = FLADRV_NAME_S2MU106;
	flash->subdev = subdev_flash;
	flash->client = client;

	flash->flash_data.mode = CAM2_FLASH_MODE_OFF;
	flash->flash_data.intensity = 100; /* TODO: Need to figure out min/max range */
	flash->flash_data.firing_time_us = 1 * 1000 * 1000; /* Max firing time is 1sec */

	/* get flash_led ch by dt */
	for (i = 0; i < FLASH_LED_CH_MAX; i++)
		flash->led_ch[i] = -1;

	elements = of_property_count_u32_elems(dnode, "led_ch");
	if (elements < 0 || elements > FLASH_LED_CH_MAX) {
		warn("flash led elements is too much or wrong(%d), set to max(%d)\n",
			elements, FLASH_LED_CH_MAX);
		elements = FLASH_LED_CH_MAX;
	}

	if (elements) {
		if (of_property_read_u32_array(dnode, "led_ch", flash->led_ch, elements)) {
			err("cannot get flash led_ch, set only ch1\n");
			flash->led_ch[0] = 1;
		}
	} else {
		probe_info("set flash_ch as default(only ch1)\n");
		flash->led_ch[0] = 1;
	}

	device->subdev_flash = subdev_flash;
	device->flash = flash;

	v4l2_subdev_init(subdev_flash, &subdev_ops);

	v4l2_set_subdevdata(subdev_flash, flash);
	v4l2_set_subdev_hostdata(subdev_flash, device);
	snprintf(subdev_flash->name, V4L2_SUBDEV_NAME_SIZE, "flash-subdev.%d", flash->id);

p_err:
	return ret;
}

static int __init flash_s2mu106_platform_probe(struct platform_device *pdev)
{
	int ret = 0;
	struct device *dev;

	FIMC_BUG(!pdev);

	dev = &pdev->dev;

	ret = flash_s2mu106_probe(dev, NULL);
	if (ret < 0) {
		probe_err("flash gpio probe fail(%d)\n", ret);
		goto p_err;
	}

	probe_info("%s done\n", __func__);

p_err:
	return ret;
}

static const struct of_device_id exynos_is_sensor_flash_s2mu106_match[] = {
	{
		.compatible = "samsung,sensor-flash-s2mu106",
	},
};
MODULE_DEVICE_TABLE(of, exynos_is_sensor_flash_s2mu106_match);

/* register platform driver */
static struct platform_driver sensor_flash_s2mu106_platform_driver = {
	.driver = {
		.name   = "IS-SENSOR-FLASH-S2MU106-PLATFORM",
		.owner  = THIS_MODULE,
		.of_match_table = exynos_is_sensor_flash_s2mu106_match,
	}
};

static int __init is_sensor_flash_s2mu106_init(void)
{
	int ret;

	ret = platform_driver_probe(&sensor_flash_s2mu106_platform_driver,
				flash_s2mu106_platform_probe);
	if (ret)
		err("failed to probe %s driver: %d\n",
			sensor_flash_s2mu106_platform_driver.driver.name, ret);

	return ret;
}
late_initcall_sync(is_sensor_flash_s2mu106_init);
