/*
 * Samsung Exynos5 SoC series Ranging sensor driver
 *
 *
 * Copyright (c) 2014 Samsung Electronics Co., Ltd
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#include <linux/i2c.h>
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


enum vl53l5_external_control {
	VL53L5_EXT_START,
	VL53L5_EXT_STOP,
	VL53L5_EXT_GET_RANGE,
};

extern int vl53l5_ext_control(enum vl53l5_external_control input, void *data, u32 *size);

static u64 laser_af_start_time = 0;

#define VL53L5_FIRST_DELAY	168000000L

static int laser_af_vl53l5_init(struct v4l2_subdev *subdev, u32 val)
{
	int ret = 0;
	struct is_laser_af *laser_af;

	FIMC_BUG(!subdev);

	laser_af = (struct is_laser_af *)v4l2_get_subdevdata(subdev);

	FIMC_BUG(!laser_af);

	return ret;
}

static int laser_af_vl53l5_set_active(struct v4l2_subdev *subdev, bool is_active)
{
	int ret = 0;
	struct is_laser_af *laser_af;
	struct is_core *core = (struct is_core *)dev_get_drvdata(is_dev);

	FIMC_BUG(!subdev);

	laser_af = (struct is_laser_af *)v4l2_get_subdevdata(subdev);

	if (is_active) {
		if (!laser_af->active) {
			atomic_inc(&core->laser_refcount);

			if (atomic_read(&core->laser_refcount) == 1) {
				mutex_lock(laser_af->laser_lock);

				info("[%s] on E\n", __func__);
				ret = vl53l5_ext_control(VL53L5_EXT_START, NULL, NULL);
				info("[%s] on X\n", __func__);

				laser_af_start_time = is_get_timestamp();

				mutex_unlock(laser_af->laser_lock);
			}
		}

		laser_af->active = true;
	} else {
		if (laser_af->active) {
			atomic_dec(&core->laser_refcount);

			if (atomic_read(&core->laser_refcount) == 0) {
				mutex_lock(laser_af->laser_lock);

				info("[%s] off E\n", __func__);
				ret = vl53l5_ext_control(VL53L5_EXT_STOP, NULL, NULL);
				info("[%s] off X\n", __func__);

				laser_af_start_time = 0;

				mutex_unlock(laser_af->laser_lock);
			}
		}

		laser_af->active = false;
	}

	return ret;
}

static int laser_af_vl53l5_get_distance(struct v4l2_subdev *subdev, void *data, u32 *size)
{
	int ret = 0;
	struct is_laser_af *laser_af;
	u64 current_time;
	unsigned int first_delay;

	FIMC_BUG(!subdev);

	laser_af = (struct is_laser_af *)v4l2_get_subdevdata(subdev);

	FIMC_BUG(!laser_af);

	mutex_lock(laser_af->laser_lock);
	if (laser_af_start_time > 0) {
		current_time = is_get_timestamp();
		if (current_time < laser_af_start_time + VL53L5_FIRST_DELAY) {
			first_delay = (unsigned int)((laser_af_start_time + VL53L5_FIRST_DELAY - current_time) / 1000000L);
			msleep(first_delay);
			info("%s, first delay %d", __func__, first_delay);
		}

		laser_af_start_time = 0;
	}

	ret = vl53l5_ext_control(VL53L5_EXT_GET_RANGE, data, size);
	mutex_unlock(laser_af->laser_lock);

	dbg_sensor(3, "ret(%d) size(%d)", ret, *size);

	return ret;
}

static const struct v4l2_subdev_core_ops core_ops = {
	.init = laser_af_vl53l5_init,
};

static struct is_laser_af_ops laser_af_ops = {
	.set_active = laser_af_vl53l5_set_active,
	.get_distance = laser_af_vl53l5_get_distance,
};

static const struct v4l2_subdev_ops subdev_ops = {
	.core = &core_ops,
};

static int laser_af_vl53l5_probe(struct device *dev)
{
	int ret = 0;
	struct is_core *core;
	struct v4l2_subdev *subdev_laser_af = NULL;
	struct is_device_sensor *device;
	struct is_laser_af *laser_af = NULL;
	struct device_node *dnode;
	const u32 *sensor_id_spec;
	u32 sensor_id_len;
	u32 sensor_id[IS_SENSOR_COUNT];
	int i;

	FIMC_BUG(!is_dev);
	FIMC_BUG(!dev);

	dnode = dev->of_node;

	core = (struct is_core *)dev_get_drvdata(is_dev);
	if (!core) {
		probe_info("core device is not yet probed");
		ret = -EPROBE_DEFER;
		goto p_err;
	}

	sensor_id_spec = of_get_property(dnode, "id", &sensor_id_len);
	if (!sensor_id_spec || sensor_id_len == 0) {
		err("sensor_id num read is fail(%d), sensor_id_len(%d)", ret, sensor_id_len);
		goto p_err;
	}

	sensor_id_len /= (unsigned int)sizeof(*sensor_id_spec);

	ret = of_property_read_u32_array(dnode, "id", sensor_id, sensor_id_len);
	if (ret) {
		err("sensor_id read is fail(%d)", ret);
		goto p_err;
	}

	for (i = 0; i < sensor_id_len; i++) {
		device = &core->sensor[sensor_id[i]];
		if (!device) {
			err("sensor device is NULL");
			ret = -EPROBE_DEFER;
			goto p_err;
		}
	}

	laser_af = kzalloc(sizeof(struct is_laser_af) * sensor_id_len, GFP_KERNEL);
	if (!laser_af) {
		err("laser_af is NULL");
		ret = -ENOMEM;
		goto p_err;
	}

	subdev_laser_af = kzalloc(sizeof(struct v4l2_subdev) * sensor_id_len, GFP_KERNEL);
	if (!subdev_laser_af) {
		err("subdev_laser_af is NULL");
		ret = -ENOMEM;
		kfree(laser_af);
		goto p_err;
	}

	for (i = 0; i < sensor_id_len; i++) {
		probe_info("%s sensor_id %d\n", __func__, sensor_id[i]);
		laser_af[i].id = LASER_AF_NAME_VL53L5;
		laser_af[i].subdev = &subdev_laser_af[i];
		laser_af[i].laser_af_ops = &laser_af_ops;

		device = &core->sensor[sensor_id[i]];
		device->subdev_laser_af = &subdev_laser_af[i];
		device->laser_af = &laser_af[i];

		v4l2_subdev_init(&subdev_laser_af[i], &subdev_ops);

		v4l2_set_subdevdata(&subdev_laser_af[i], &laser_af[i]);
		v4l2_set_subdev_hostdata(&subdev_laser_af[i], device);
		snprintf(subdev_laser_af[i].name, V4L2_SUBDEV_NAME_SIZE,
					"laser_af-subdev.%d", laser_af[i].id);

		probe_info("%s done\n", __func__);
	}

	return ret;

p_err:
	if (laser_af)
		kfree(laser_af);

	if (subdev_laser_af)
		kfree(subdev_laser_af);

	return ret;
}

static int laser_af_vl53l5_platform_probe(struct platform_device *pdev)
{
	int ret = 0;
	struct device *dev;

	FIMC_BUG(!pdev);

	dev = &pdev->dev;

	ret = laser_af_vl53l5_probe(dev);
	if (ret < 0) {
		probe_err("laser_af vl53l5 probe fail(%d)\n", ret);
		goto p_err;
	}

	probe_info("%s done\n", __func__);

p_err:
	return ret;
}

static const struct of_device_id exynos_is_laser_af_vl53l5_match[] = {
	{
		.compatible = "samsung,laser-af-vl53l5",
	},
	{},
};
MODULE_DEVICE_TABLE(of, exynos_is_laser_af_vl53l5_match);

/* register platform driver */
static struct platform_driver sensor_laser_af_vl53l5_platform_driver = {
	.probe = laser_af_vl53l5_platform_probe,
	.driver = {
		.name   = "IS-LASER-AF-VL53L5-PLATFORM",
		.owner  = THIS_MODULE,
		.of_match_table = exynos_is_laser_af_vl53l5_match,
	}
};

#ifdef MODULE
static int laser_af_vl53l5_module_init(void)
{
	int ret = 0;

	ret = platform_driver_register(&sensor_laser_af_vl53l5_platform_driver);
	if (ret) {
		err("sensor_laser_af_vl53l5_platform_driver register failed(%d)", ret);
		return ret;
	}

	return ret;
}

static void laser_af_vl53l5_module_exit(void)
{
	platform_driver_unregister(&sensor_laser_af_vl53l5_platform_driver);
}
module_init(laser_af_vl53l5_module_init);
module_exit(laser_af_vl53l5_module_exit);
#else
static int __init _laser_af_vl53l5_init(void)
{
	int ret;

	ret = platform_driver_probe(&sensor_laser_af_vl53l5_platform_driver,
				laser_af_vl53l5_platform_probe);
	if (ret)
		err("failed to probe %s driver: %d\n",
			sensor_laser_af_vl53l5_platform_driver.driver.name, ret);

	return ret;
}

late_initcall_sync(_laser_af_vl53l5_init);
#endif

MODULE_LICENSE("GPL");
MODULE_SOFTDEP("pre: fimc-is");
