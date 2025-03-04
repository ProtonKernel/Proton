/*
 *  stk3a8x_drv_i2c.c - Linux kernel modules for sensortek stk3a8x
 *  proximity/ambient light sensor (I2C Interface)
 *
 *  Copyright (C) 2012~2018 Taka Chiu, sensortek Inc.
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 */

#include <linux/input.h>
#include <linux/i2c.h>
#include <linux/module.h>
#include <linux/of.h>
#include <linux/types.h>
#include <linux/pm.h>
//#include <linux/sensors.h>
#include "stk3a8x.h"

int flicker_param_lpcharge;
module_param(flicker_param_lpcharge, int, 0440);

static int stk3a8x_reg_read(struct stk3a8x_data *alps_data,
		unsigned char reg)
{
	int error = 0;
	mutex_lock(&alps_data->io_lock);
	error = i2c_smbus_read_byte_data(alps_data->client, reg);
	mutex_unlock(&alps_data->io_lock);

	if (error < 0)
	{
		dev_err(&alps_data->client->dev,
				"%s: failed to read reg:0x%x\n",
				__func__, reg);
	}

	return error;
}

static int stk3a8x_reg_write(struct stk3a8x_data *alps_data,
		unsigned char reg,
		unsigned char val)
{
	int error = 0;
	mutex_lock(&alps_data->io_lock);
	error = i2c_smbus_write_byte_data(alps_data->client, reg, val);
	mutex_unlock(&alps_data->io_lock);

	if (error < 0)
	{
		dev_err(&alps_data->client->dev,
				"%s: failed to write reg:0x%x with val:0x%x\n",
				__func__, reg, val);
	}

	return error;
}

static int stk3a8x_reg_write_block(struct stk3a8x_data *alps_data,
		unsigned char reg,
		unsigned char *val,
		unsigned char length)
{
	int error = 0;
	mutex_lock(&alps_data->io_lock);
	error = i2c_smbus_write_i2c_block_data(alps_data->client, reg, length, val);
	mutex_unlock(&alps_data->io_lock);

	if (error < 0)
	{
		dev_err(&alps_data->client->dev,
				"%s: failed to write reg:0x%x\n",
				__func__, reg);
	}

	return error;
}
static int stk3a8x_reg_read_modify_write(struct stk3a8x_data *alps_data,
		unsigned char reg,
		unsigned char val,
		unsigned char mask)
{
	uint8_t rw_buffer = 0;
	int error = 0;

	if ((mask == 0xFF) || (mask == 0x0))
	{
		error = stk3a8x_reg_write(alps_data, reg, val);

		if (error < 0)
		{
			dev_err(&alps_data->client->dev,
					"%s: failed to write reg:0x%x with val:0x%x\n",
					__func__, reg, val);
		}
	}
	else
	{
		rw_buffer = (uint8_t)stk3a8x_reg_read(alps_data, reg);

		if (rw_buffer < 0)
		{
			dev_err(&alps_data->client->dev,
					"%s: failed to read reg:0x%x\n",
					__func__, reg);
			return rw_buffer;
		}
		else
		{
			rw_buffer = (rw_buffer & (~mask)) | (val & mask);
			error = stk3a8x_reg_write(alps_data, reg, rw_buffer);

			if (error < 0)
			{
				dev_err(&alps_data->client->dev,
						"%s: failed to write reg(mask):0x%x with val:0x%x\n",
						__func__, reg, val);
			}
		}
	}

	return error;
}

static int stk3a8x_reg_read_block(struct stk3a8x_data *alps_data,
		unsigned char reg,
		int len,
		void *buf)
{
	int error = 0;
	struct i2c_msg msgs[2] = {
		{
			.addr = alps_data->client->addr,
			.flags = 0,
			.len = 1,
			.buf = &reg
		},
		{
			.addr = alps_data->client->addr,
			.flags = I2C_M_RD,
			.len = (0 >= len) ? 1 : len,
			.buf = buf
		}
	};

	mutex_lock(&alps_data->io_lock);
	error = i2c_transfer(alps_data->client->adapter, msgs, 2);
	mutex_unlock(&alps_data->io_lock);

	if (2 == error)
		error = 0;
	else if (0 > error)
	{
		dev_err(&alps_data->client->dev, "transfer failed to read reg:0x%x with len:%d, error=%d\n", reg, len, error);
	}
	else
	{
		dev_err(&alps_data->client->dev, "size error in reading reg:0x%x with len:%d, error=%d\n", reg, len, error);
		error = -1;
	}

	return error;
}                                  

static const struct stk3a8x_bus_ops stk3a8x_i2c_bops =
{
	.bustype            = BUS_I2C,
	.write              = stk3a8x_reg_write,
	.write_block        = stk3a8x_reg_write_block,
	.read               = stk3a8x_reg_read,
	.read_block         = stk3a8x_reg_read_block,
	.read_modify_write  = stk3a8x_reg_read_modify_write,
};

static int stk3a8x_i2c_probe(struct i2c_client *client,
		const struct i2c_device_id *id)
{
	return stk3a8x_probe(client, &stk3a8x_i2c_bops);
}

static int stk3a8x_i2c_remove(struct i2c_client *client)
{
	return stk3a8x_remove(client);
}

int stk3a8x_i2c_suspend(struct device *dev)
{
	stk3a8x_suspend(dev);
	return 0;
}

int stk3a8x_i2c_resume(struct device *dev)
{
	stk3a8x_resume(dev);
	return 0;
}

static const struct dev_pm_ops stk3a8x_pm_ops =
{
	SET_SYSTEM_SLEEP_PM_OPS(stk3a8x_i2c_suspend, stk3a8x_i2c_resume)
};

static const struct i2c_device_id stk_als_id[] =
{
	{ "stk_als", 0},
	{}
};

static struct of_device_id stk_match_table[] =
{
	{ .compatible = "stk,stk3a8x", },
	{ },
};

static struct i2c_driver stk_als_driver =
{
	.driver = {
		.name = DEVICE_NAME,
		.owner = THIS_MODULE,
#ifdef CONFIG_OF
		.of_match_table = stk_match_table,
#endif
		.pm = &stk3a8x_pm_ops,
	},
	.probe      = stk3a8x_i2c_probe,
	.remove     = stk3a8x_i2c_remove,
	.id_table   = stk_als_id,
};

static int __init stk3a8x_init(void)
{
	int ret;
	if (flicker_param_lpcharge == 1) {
		info_flicker("lpm_mode");
		return 0;
	}

	info_flicker("start\n");
	ret = i2c_add_driver(&stk_als_driver);
	info_flicker("Add driver ret = %d\n", ret);

	if (ret)
	{
		i2c_del_driver(&stk_als_driver);
		return ret;
	}

	return 0;
}
static void __exit stk3a8x_exit(void)
{
	i2c_del_driver(&stk_als_driver);
}
int print_debug;
module_param(print_debug, int, 0644);

module_init(stk3a8x_init);
module_exit(stk3a8x_exit);
MODULE_DEVICE_TABLE(i2c, stk_als_id);
MODULE_AUTHOR("Taka Chiu <taka_chiu@sensortek.com.tw>");
MODULE_DESCRIPTION("Sensortek stk3a8x ambient Light Sensor driver");
MODULE_LICENSE("GPL");
MODULE_VERSION(DRIVER_VERSION);
MODULE_IMPORT_NS(ANDROID_GKI_VFS_EXPORT_ONLY);
