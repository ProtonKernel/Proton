/*
 * Samsung Exynos5 SoC series FIMC-IS driver
 *
 * exynos5 fimc-is core functions
 *
 * Copyright (c) 2011 Samsung Electronics Co., Ltd
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/fs.h>
#include <linux/slab.h>
#include <linux/init.h>
#include <linux/list.h>
#include <linux/platform_device.h>
#include <linux/spi/spi.h>
#include <linux/vmalloc.h>
#include <linux/firmware.h>
#include <linux/i2c.h>
#include <linux/gpio.h>
#include <linux/of.h>
#include <linux/of_gpio.h>
#include <linux/io.h>
#include <linux/platform_device.h>
#include <linux/regulator/consumer.h>
#include <linux/delay.h>

#include "is-core.h"
#include "is-interface.h"
#include "is-device-ischain.h"
#include "is-dt.h"
#include "is-device-af.h"
#include "is-vender-specific.h"
#include "is-i2c-config.h"
#include "is-device-sensor-peri.h"

#define IS_AF_DEV_NAME "exynos-is-af"
#define AK737X_MAX_PRODUCT_LIST		10

bool check_af_init_rear = false;
bool check_af_init_tele = false;
bool check_af_init_tele2 = false;

int is_af_i2c_read_8(struct i2c_client *client,
	u8 addr, u8 *val)
{
	int ret = 0;
	struct i2c_msg msg[2];
	u8 wbuf[1];

	if (!client->adapter) {
		pr_err("Could not find adapter!\n");
		ret = -ENODEV;
		goto p_err;
	}

	/* 1. I2C operation for writing. */
	msg[0].addr = client->addr;
	msg[0].flags = 0; /* write : 0, read : 1 */
	msg[0].len = 1;
	msg[0].buf = wbuf;
	wbuf[0] = addr;

	/* 2. I2C operation for reading data. */
	msg[1].addr = client->addr;
	msg[1].flags = I2C_M_RD;
	msg[1].len = 1;
	msg[1].buf = val;

	ret = is_i2c_transfer(client->adapter, msg, 2);
	if (ret < 0) {
		pr_err("i2c transfer fail(%d)", ret);
		goto p_err;
	}

	i2c_info("I2CR08(%d) [0x%04X] : 0x%04X\n", client->addr, addr, *val);
	return 0;
p_err:
	return ret;
}

int is_af_i2c_read(struct i2c_client *client, u16 addr, u16 *data)
{
	int err;
	u8 rxbuf[2], txbuf[2];
	struct i2c_msg msg[2];

	txbuf[0] = (addr & 0xff00) >> 8;
	txbuf[1] = (addr & 0xff);

	msg[0].addr = client->addr;
	msg[0].flags = 0;
	msg[0].len = 2;
	msg[0].buf = txbuf;

	msg[1].addr = client->addr;
	msg[1].flags = I2C_M_RD;
	msg[1].len = 2;
	msg[1].buf = rxbuf;

	err = i2c_transfer(client->adapter, msg, 2);
	if (unlikely(err != 2)) {
		err("%s: register read fail err = %d\n", __func__, err);
		return -EIO;
	}

	*data = ((rxbuf[0] << 8) | rxbuf[1]);
	return 0;
}

int is_af_i2c_write(struct i2c_client *client ,u8 addr, u8 data)
{
        int retries = I2C_RETRY_COUNT;
        int ret = 0, err = 0;
        u8 buf[2] = {0,};
        struct i2c_msg msg = {
                .addr   = client->addr,
                .flags  = 0,
                .len    = 2,
                .buf    = buf,
        };

        buf[0] = addr;
        buf[1] = data;

#if 0
        pr_info("%s : W(0x%02X%02X)\n",__func__, buf[0], buf[1]);
#endif

        do {
                ret = i2c_transfer(client->adapter, &msg, 1);
                if (likely(ret == 1))
                        break;

                usleep_range(10000,11000);
                err = ret;
        } while (--retries > 0);

        /* Retry occured */
        if (unlikely(retries < I2C_RETRY_COUNT)) {
                err("i2c_write: error %d, write (%02X, %02X), retry %d\n",
                        err, addr, data, I2C_RETRY_COUNT - retries);
        }

        if (unlikely(ret != 1)) {
                err("I2C does not work\n\n");
                return -EIO;
        }

        return 0;
}

int is_af_init(struct is_actuator *actuator, struct i2c_client *client, int val)
{
	int ret = 0;
	int i = 0;
	u32 product_id_list[AK737X_MAX_PRODUCT_LIST] = {0, };
	u32 product_id_len = 0;
	u8 product_id = 0;
	const u32 *product_id_spec;

	struct device *dev;
	struct device_node *dnode;

	WARN_ON(!actuator);

	if (unlikely(!client)) {
		err("client is NULL");
		ret = -EINVAL;
		goto p_err;
	}

	dev = &client->dev;
	dnode = dev->of_node;

	product_id_spec = of_get_property(dnode, "vendor_product_id", &product_id_len);
	if (!product_id_spec)
		err("vendor_product_id num read is fail(%d)", ret);

	product_id_len /= (unsigned int)sizeof(*product_id_spec);

	ret = of_property_read_u32_array(dnode, "vendor_product_id", product_id_list, product_id_len);
	if (ret)
		err("vendor_product_id read is fail(%d)", ret);

	if (product_id_len < 2 || (product_id_len % 2) != 0
		|| product_id_len > AK737X_MAX_PRODUCT_LIST) {
		err("[%s] Invalid product_id in dts\n", __func__);
		ret = -EINVAL;
		goto p_err;
	}

	if (actuator->vendor_use_standby_mode) {
		/* Go standby mode */
		ret = is_af_i2c_write(client, 0x02, 0x40);
		if (ret < 0)
			goto p_err;
		if (actuator->vendor_sleep_to_standby_delay) {
			usleep_range(actuator->vendor_sleep_to_standby_delay, actuator->vendor_sleep_to_standby_delay + 10);
		} else {
			usleep_range(2200, 2210);
		}
	}

	for (i = 0; i < product_id_len; i += 2) {
		ret = is_af_i2c_read_8(client, product_id_list[i], &product_id);
		if (ret < 0) {
			goto p_err;
		}

		info("[%s][%d] dt[addr=0x%X,id=0x%X], product_id=0x%X\n",
				__func__, actuator->device, product_id_list[i], product_id_list[i+1], product_id);

		if (product_id_list[i+1] == product_id) {
			actuator->vendor_product_id = product_id_list[i+1];
			break;
		}
	}

	if (i == product_id_len) {
		err("[%s] Invalid product_id in module\n", __func__);
		ret = -EINVAL;
		goto p_err;
	}

	/* Go sleep mode */
	ret = is_af_i2c_write(client, 0x02, 0x20);
	if (ret < 0)
		goto p_err;

p_err:
	return ret;
}

int is_af_check_init_state(struct is_actuator *actuator, struct i2c_client *client,  int position)
{
	int ret = 0;

	if (position == SENSOR_POSITION_REAR) {
		if (!check_af_init_rear) {
			ret = is_af_init(actuator, client, 0);
			if (ret) {
				err("v4l2_actuator_call(init) is fail(%d)", ret);
				return ret;
			}
			check_af_init_rear = true;
		}
	} else if (position == SENSOR_POSITION_REAR2) {
		if (!check_af_init_tele) {
			ret = is_af_init(actuator, client, 0);
			if (ret) {
				err("v4l2_actuator_call(init) is fail(%d)", ret);
				return ret;
			}
			check_af_init_tele = true;
		}
	} else if (position == SENSOR_POSITION_REAR4) {
		if (!check_af_init_tele2) {
			ret = is_af_init(actuator, client, 0);
			if (ret) {
				err("v4l2_actuator_call(init) is fail(%d)", ret);
				return ret;
			}
			check_af_init_tele2 = true;
		}
	}
	return ret;
}

int16_t is_af_move_lens(struct is_core *core, int position)
{
	int ret = 0;
	struct is_actuator *actuator = NULL;
	struct i2c_client *client = NULL;
	struct is_device_sensor *device;
	struct is_module_enum *module;
	int sensor_id = 0;

	is_vendor_get_module_from_position(position, &module);
	if (!module) {
		err("%s, module is NULL", __func__);
		ret = -EINVAL;
		return ret;
	}

	sensor_id = module->pdata->id;

	info("[%s] is_af_move_lens : sensor_id = %d\n", __func__, sensor_id);

	device = &core->sensor[sensor_id];
	actuator = device->actuator[sensor_id];
	client = actuator->client;

	if (actuator->vendor_use_standby_mode) {
		is_af_check_init_state(actuator, client, position);

		/* Go standby mode */
		ret = is_af_i2c_write(client, 0x02, 0x40);
		if (ret) {
			err("i2c write fail\n");
		}

		if (actuator->vendor_sleep_to_standby_delay) {
			usleep_range(actuator->vendor_sleep_to_standby_delay, actuator->vendor_sleep_to_standby_delay + 10);
		} else {
			usleep_range(2200, 2210);
		}

		info("[%s] Set standy mode\n", __func__);
	}

	ret = is_af_i2c_write(client, 0x00, 0x80);
	if (ret) {
		err("i2c write fail\n");
	}

	ret = is_af_i2c_write(client, 0x01, 0x00);
	if (ret) {
		err("i2c write fail\n");
	}

	ret = is_af_i2c_write(client, 0x02, 0x00);
	if (ret) {
		err("i2c write fail\n");
	}

	return ret;
}

int16_t is_af_move_lens_pos(struct is_core *core, int position, u32 val)
{
	int ret = 0;
	struct is_actuator *actuator = NULL;
	struct i2c_client *client = NULL;
	struct is_device_sensor *device;
	struct is_module_enum *module;
	int sensor_id = 0;
	u8 val_high = 0, val_low = 0;

	is_vendor_get_module_from_position(position, &module);
	if (!module) {
		err("%s, module is NULL", __func__);
		ret = -EINVAL;
		return ret;
	}

	sensor_id = module->pdata->id;

	info("[%s] is_af_move_lens : sensor_id = %d\n", __func__, sensor_id);

	device = &core->sensor[sensor_id];
	actuator = device->actuator[sensor_id];
	client = actuator->client;

	if (actuator->vendor_use_standby_mode) {
		is_af_check_init_state(actuator, client, position);

		/* Go standby mode */
		ret = is_af_i2c_write(client, 0x02, 0x40);
		if (ret) {
			err("i2c write fail\n");
		}

		if (actuator->vendor_sleep_to_standby_delay) {
			usleep_range(actuator->vendor_sleep_to_standby_delay, actuator->vendor_sleep_to_standby_delay + 10);
		} else {
			usleep_range(2200, 2210);
		}

		info("[%s] Set standy mode\n", __func__);
	}

	val_high = (val & 0x0FFF) >> 4;
	val_low = (val & 0x000F) << 4;

	ret = is_af_i2c_write(client, 0x00, val_high);
	if (ret) {
		err("i2c write fail\n");
	}

	ret = is_af_i2c_write(client, 0x01, val_low);
	if (ret) {
		err("i2c write fail\n");
	}

	/* Go active mode */
	ret = is_af_i2c_write(client, 0x02, 0x00);
	if (ret) {
		err("i2c write fail\n");
	}

	return ret;
}

EXPORT_SYMBOL_GPL(is_af_move_lens);

MODULE_DESCRIPTION("AF driver for remove noise");
MODULE_AUTHOR("kyoungho yun <kyoungho.yun@samsung.com>");
MODULE_LICENSE("GPL v2");
