/*
 * Samsung Exynos5 SoC series Sensor driver
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
#include <media/v4l2-ctrls.h>
#include <media/v4l2-device.h>
#include <media/v4l2-subdev.h>

#include <exynos-is-sensor.h>
#include "is-sensor-eeprom-5e9.h"
#include "is-sensor-eeprom.h"
#include "is-device-sensor.h"
#include "is-device-sensor-peri.h"
#include "is-core.h"

#define SENSOR_EEPROM_NAME "5E9"

int is_eeprom_5e9_check_all_crc(struct v4l2_subdev *subdev)
{
	int ret = 0;
	struct is_eeprom *eeprom = NULL;
	struct is_device_sensor *sensor = NULL;

	FIMC_BUG(!subdev);

	eeprom = (struct is_eeprom *)v4l2_get_subdevdata(subdev);

	FIMC_BUG(!eeprom);

	sensor = (struct is_device_sensor *)v4l2_get_subdev_hostdata(subdev);
	if (!sensor) {
		err("device sensor is NULL");
		ret = -ENODEV;
		return ret;
	}

	/* Check CRC to Address cal data */
	ret = is_sensor_eeprom_check_crc("ADDR", &eeprom->data[EEPROM_ADD_CRC_FST],
		&eeprom->data[EEPROM_ADD_CRC_CHK_START], EEPROM_ADD_CRC_CHK_SIZE,
		EEPROM_CRC16_BE);
	if (ret) {
		err("%s(): 5E9 EEPROM Address section CRC check fail(%d)", __func__, ret);

		/* All calibration data is zero set only Address section is invalid CRC */
		is_eeprom_cal_data_set(eeprom->data, "all",
				EEPROM_ADD_CRC_SEC, EEPROM_DATA_SIZE, 0xff);

		/* Set all cal_status to ERROR if Address cal data invalid */
		for (int i = 0; i < CAMERA_CRC_INDEX_MAX; i++)
		    sensor->cal_status[i] = CRC_ERROR;

		return ret;
	} else
		info("5E9 EEPROM Address section CRC check success\n");

	/* Check CRC to Information cal data */
	ret = is_sensor_eeprom_check_crc("INFO", &eeprom->data[EEPROM_INFO_CRC_FST],
		&eeprom->data[EEPROM_INFO_CRC_CHK_START], EEPROM_INFO_CRC_CHK_SIZE,
		EEPROM_CRC16_BE);
	if (ret) {
		err("%s(): 5E9 EEPROM Information CRC section check fail(%d)", __func__, ret);

		/* All calibration data is 0xff set but exception Address section */
		is_eeprom_cal_data_set(eeprom->data, "Information - End",
				EEPROM_INFO_CRC_SEC, EEPROM_ADD_CAL_SIZE, 0xff);

		sensor->cal_status[CAMERA_CRC_INDEX_MNF] = CRC_ERROR;

	} else {
		info("5E9 EEPROM Informaion section CRC check success\n");

		sensor->cal_status[CAMERA_CRC_INDEX_MNF] = CRC_NO_ERROR;
	}

	/* Check CRC to AWB cal data */
	ret = is_sensor_eeprom_check_crc("AWB", &eeprom->data[EEPROM_AWB_CRC_FST],
		&eeprom->data[EEPROM_AWB_CRC_CHK_START], EEPROM_AWB_CRC_CHK_SIZE,
		EEPROM_CRC16_BE);
	if (ret) {
		err("%s(): 5E9 EEPROM AWB section CRC check fail(%d)", __func__, ret);

		is_eeprom_cal_data_set(eeprom->data, "AWB",
				EEPROM_AWB_CRC_SEC, EEPROM_AWB_CAL_SIZE, 0xff);

		sensor->cal_status[CAMERA_CRC_INDEX_AWB] = CRC_ERROR;

	} else {
		info("5E9 EEPROM AWB section CRC check success\n");

		sensor->cal_status[CAMERA_CRC_INDEX_AWB] = CRC_NO_ERROR;

		ret = is_sensor_eeprom_check_awb_ratio(&eeprom->data[EEPROM_AWB_UNIT_OFFSET],
			&eeprom->data[EEPROM_AWB_GOLDEN_OFFSET], &eeprom->data[EEPROM_AWB_LIMIT_OFFSET]);
		if (ret) {
			err("%s(): 5E9 EEPROM AWB ratio out of limit(%d)", __func__, ret);

			sensor->cal_status[CAMERA_CRC_INDEX_AWB] = LIMIT_FAILURE;
		}
	}

	/* Check CRC to LSC cal data */
	ret = is_sensor_eeprom_check_crc("LSC", &eeprom->data[EEPROM_LSC_CRC_FST],
		&eeprom->data[EEPROM_LSC_CRC_CHK_START], EEPROM_LSC_CRC_CHK_SIZE,
		EEPROM_CRC16_BE);
	if (ret) {
		err("%s(): 5E9 EEPROM LSC section CRC check fail(%d)", __func__, ret);

		is_eeprom_cal_data_set(eeprom->data, "LSC",
				EEPROM_LSC_CRC_SEC, EEPROM_LSC_CAL_SIZE, 0xff);

	} else
		info("5E9 EEPROM LSC section CRC check success\n");

	/* Check CRC to AE Sync cal data */
	ret = is_sensor_eeprom_check_crc("AE", &eeprom->data[EEPROM_AE_CRC_FST],
		&eeprom->data[EEPROM_AE_CRC_CHK_START], EEPROM_AE_CRC_CHK_SIZE,
		EEPROM_CRC16_BE);
	if (ret) {
		err("%s(): 5E9 EEPROM AE section CRC check fail(%d)", __func__, ret);

		is_eeprom_cal_data_set(eeprom->data, "AE",
				EEPROM_AE_CRC_SEC, EEPROM_AE_CAL_SIZE, 0xff);

	} else
		info("5E9 EEPROM AE section CRC check success\n");

	/* Check CRC to SFR cal data */
	ret = is_sensor_eeprom_check_crc("SFR", &eeprom->data[EEPROM_SFR_CRC_FST],
		&eeprom->data[EEPROM_SFR_CRC_CHK_START], EEPROM_SFR_CRC_CHK_SIZE,
		EEPROM_CRC16_BE);
	if (ret) {
		err("%s(): EEPROM SFR section CRC check fail(%d)", __func__, ret);

		is_eeprom_cal_data_set(eeprom->data, "SFR",
				EEPROM_SFR_CRC_SEC, EEPROM_SFR_CAL_SIZE, 0xff);

	} else
		info("5E9 EEPROM SFR section CRC check success\n");

	/* Write file to serial number of Information calibration data */
	ret = is_eeprom_file_write(EEPROM_SERIAL_NUM_DATA_PATH,
			(void *)&eeprom->data[EEPROM_INFO_SERIAL_NUM_START], EEPROM_INFO_SERIAL_NUM_SIZE);
	if (ret < 0)
		err("%s(), DUAL cal file write fail(%d)", __func__, ret);

	return ret;
}

int is_eeprom_5e9_get_cal_data(struct v4l2_subdev *subdev)
{
	int ret = 0;
	struct is_eeprom *eeprom;
	struct i2c_client *client;

	FIMC_BUG(!subdev);

	eeprom = (struct is_eeprom *)v4l2_get_subdevdata(subdev);

	FIMC_BUG(!eeprom);

	client = eeprom->client;
	if (unlikely(!client)) {
		err("client is NULL");
		ret = -EINVAL;
		return ret;
	}

	/*
	 * If already read at EEPROM data in module
	 * don't again read at EEPROM but there isn't file or
	 * data is NULL read EEPROM data
	 */
	ret = is_eeprom_file_read(EEPROM_DATA_PATH, (void *)eeprom->data, EEPROM_DATA_SIZE);
	if (ret) {
		/* I2C read to Sensor EEPROM cal data */
		ret = is_eeprom_module_read(client, EEPROM_ADD_CRC_FST, eeprom->data, EEPROM_DATA_SIZE);
		if (ret < 0) {
			err("%s(): eeprom i2c read failed(%d)\n", __func__, ret);
			return ret;
		}

		/* CRC check to each section cal data */
		ret = CALL_EEPROMOPS(eeprom, eeprom_check_all_crc, subdev);
		if (ret < 0)
			err("%s(): eeprom data invalid(%d)\n", __func__, ret);

		/* Write file to Cal data */
		ret = is_eeprom_file_write(EEPROM_DATA_PATH, (void *)eeprom->data, EEPROM_DATA_SIZE);
		if (ret < 0) {
			err("%s(), eeprom file write fail(%d)\n", __func__, ret);
			return ret;
		}
	} else {
		/* CRC check to each section cal data */
		ret = CALL_EEPROMOPS(eeprom, eeprom_check_all_crc, subdev);
		if (ret < 0)
			err("%s(): eeprom data invalid(%d)\n", __func__, ret);
	}

	return ret;
}

static struct is_eeprom_ops sensor_eeprom_ops = {
	.eeprom_read = is_eeprom_5e9_get_cal_data,
	.eeprom_check_all_crc = is_eeprom_5e9_check_all_crc,
};

static int sensor_eeprom_5e9_probe(struct i2c_client *client,
		const struct i2c_device_id *id)
{
	int ret = 0, i;
	struct is_core *core;
	struct v4l2_subdev *subdev_eeprom = NULL;
	struct is_eeprom *eeprom = NULL;
	struct is_device_sensor *device;
	struct device *dev;
	struct device_node *dnode;
	struct is_device_sensor_peri *sensor_peri = NULL;
	u32 sensor_id = 0;

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
		probe_info("core device is not yet probed");
		return -EPROBE_DEFER;
	}

	device = &core->sensor[sensor_id];
	if (!device) {
		err("sensor device is NULL");
		ret = -ENOMEM;
		goto p_err;
	}

	sensor_peri = find_peri_by_eeprom_id(device, EEPROM_NAME_5E9);
	if (!sensor_peri) {
		probe_info("sensor peri is net yet probed");
		return -EPROBE_DEFER;
	}

	eeprom = devm_kzalloc(dev, sizeof(struct is_eeprom), GFP_KERNEL);
	if (!eeprom) {
		err("eeprom is NULL");
		ret = -ENOMEM;
		goto p_err;
	}

	subdev_eeprom = devm_kzalloc(dev, sizeof(struct v4l2_subdev), GFP_KERNEL);
	if (!subdev_eeprom) {
		probe_err("subdev_eeprom NULL");
		ret = -ENOMEM;
		goto err_alloc_subdev_eeprom;
	}

	eeprom->data = devm_kzalloc(dev, EEPROM_DATA_SIZE, GFP_KERNEL);
	if (!eeprom->data) {
		err("data is NULL");
		ret = -ENOMEM;
		goto err_alloc_subdev_data;
	}

	eeprom->id = EEPROM_NAME_5E9;
	eeprom->subdev = subdev_eeprom;
	eeprom->device = sensor_id;
	eeprom->client = client;
	eeprom->i2c_lock = NULL;
	eeprom->total_size = EEPROM_DATA_SIZE;
	eeprom->eeprom_ops = &sensor_eeprom_ops;

	for (i = 0; i < CAMERA_CRC_INDEX_MAX; i++)
		device->cal_status[i] = CRC_NO_ERROR;

	device->subdev_eeprom = subdev_eeprom;
	device->eeprom = eeprom;

	v4l2_set_subdevdata(subdev_eeprom, eeprom);
	v4l2_set_subdev_hostdata(subdev_eeprom, device);

	snprintf(subdev_eeprom->name, V4L2_SUBDEV_NAME_SIZE, "eeprom-subdev.%d", eeprom->id);

	probe_info("%s done\n", __func__);

	return 0;

err_alloc_subdev_data:
	devm_kfree(dev, subdev_eeprom);
err_alloc_subdev_eeprom:
	devm_kfree(dev, eeprom);
p_err:
	return ret;
}

static const struct of_device_id sensor_eeprom_5e9_match[] = {
	{
		.compatible = "samsung,exynos-is-sensor-eeprom-5e9",
	},
	{},
};
MODULE_DEVICE_TABLE(of, sensor_eeprom_5e9_match);

static const struct i2c_device_id sensor_eeprom_5e9_idt[] = {
	{ SENSOR_EEPROM_NAME, 0 },
	{},
};

static struct i2c_driver sensor_eeprom_5e9_driver = {
	.probe  = sensor_eeprom_5e9_probe,
	.driver = {
		.name	= SENSOR_EEPROM_NAME,
		.owner	= THIS_MODULE,
		.of_match_table = sensor_eeprom_5e9_match,
		.suppress_bind_attrs = true,
	},
	.id_table = sensor_eeprom_5e9_idt
};

static int __init sensor_eeprom_5e9_init(void)
{
	int ret;

	ret = i2c_add_driver(&sensor_eeprom_5e9_driver);
	if (ret)
		err("failed to add %s driver: %d\n",
			sensor_eeprom_5e9_driver.driver.name, ret);

	return ret;
}
late_initcall_sync(sensor_eeprom_5e9_init);
