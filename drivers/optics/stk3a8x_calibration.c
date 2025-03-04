/*
 *  stk3a8x_calibration.c - Linux kernel modules for sensortek stk3a8x,
 *  proximity/ambient light sensor (factory calibration)
 *
 *  Copyright (C) 2012~2020 Taka Chiu, sensortek Inc.
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
#include <linux/fs.h>
#include <linux/file.h>
#include <linux/unistd.h>
#include <asm/uaccess.h>
//#include <asm/segment.h>
#include <linux/buffer_head.h>
//#include <linux/sensors.h>
#include "stk3a8x.h"

#if (defined(STK_ALS_CALI) || defined(STK_PS_CALI))
static void stk3a8x_cali_func(struct work_struct *work);

static enum hrtimer_restart stk_cali_timer_func(struct hrtimer *timer)
{
	struct stk3a8x_data *alps_data = container_of(timer, struct stk3a8x_data, cali_timer);
	queue_work(alps_data->stk_cali_wq, &alps_data->stk_cali_work);
	hrtimer_forward_now(&alps_data->cali_timer, alps_data->cali_poll_delay);
	return HRTIMER_RESTART;
}

static int32_t stk3a8x_register_cali_timer(struct stk3a8x_data *alps_data)
{
	info_flicker("regist cali timer\n");

	if (alps_data->cali_info.timer_status.timer_is_active)
	{
		err_flicker("ALS: Other calibration is running\n");
		return 0;
	}

	if (!alps_data->cali_info.timer_status.timer_is_exist)
	{
		err_flicker("ALS: regist cali timer\n");
		alps_data->stk_cali_wq = create_singlethread_workqueue("stk_cali_wq");
		INIT_WORK(&alps_data->stk_cali_work, stk3a8x_cali_func);
		hrtimer_init(&alps_data->cali_timer, CLOCK_MONOTONIC, HRTIMER_MODE_REL);
		alps_data->cali_poll_delay = ns_to_ktime(55 * NSEC_PER_MSEC);
		alps_data->cali_timer.function = stk_cali_timer_func;
		alps_data->cali_info.timer_status.timer_is_exist = true;
	}

	if (!alps_data->cali_info.timer_status.timer_is_active)
	{
		info_flicker("Start cali timer\n");
		hrtimer_start(&alps_data->cali_timer, alps_data->cali_poll_delay, HRTIMER_MODE_REL);
		alps_data->cali_info.timer_status.timer_is_active = true;
	}

	info_flicker("Regist cali timer done\n");
	return 0;
}

static int32_t stk3a8x_factory_cali_ctrl_setting(uint8_t* reg_config)
{
	uint16_t err = 0;
	info_flicker("start\n");
	info_flicker("get reg setting\n");
	stk3a8x_get_reg_default_setting(STK3A8X_REG_ALSCTRL1, &err);
	info_flicker("get reg setting result = 0x%X\n", err);

	if (err > 0xFF)
	{
		return -1;
	}
	else
	{
		*reg_config = (err & 0xFF);
		return 0;
	}
}
static int32_t stk3a8x_als_get_data_without_notify(struct stk3a8x_data *alps_data, uint16_t* als_data)
{
	int32_t rv = 0;
	uint8_t  raw_data[4];
	uint16_t als_raw_data[2];
	int      loop_count;
	rv = STK3A8X_REG_BLOCK_READ(alps_data, STK3A8X_REG_DATA1_F, 10, &raw_data[0]);

	if (rv < 0)
	{
		err_flicker("read i2c (0x%X) error\n", STK3A8X_REG_DATA1_F);
		return rv;
	}

	for (loop_count = 0; loop_count < (sizeof(als_raw_data) / sizeof(als_raw_data[0])); loop_count++)
	{
		*(als_raw_data + loop_count ) = (*(raw_data + (2 * loop_count)) << 8 | *(raw_data + (2 * loop_count + 1) ));
	}

	*als_data = als_raw_data[0];
	return rv;
}

static int32_t stk3a8x_als_factory_cali_enable(struct stk3a8x_data *alps_data, bool en)
{
	uint8_t als_reg_config;
	bool config_changed = false;
	int32_t rv = 0;
	info_flicker("en = %d\n", en);

	if (en)
	{
		rv = stk3a8x_factory_cali_ctrl_setting(&als_reg_config);

		if (rv < 0)
		{
			return rv;
		}

		rv = STK3A8X_REG_READ_MODIFY_WRITE(alps_data, STK3A8X_REG_ALSCTRL1, als_reg_config, 0xFF);

		if (rv < 0)
		{
			err_flicker("read modify write i2c (0x%X) error\n", STK3A8X_REG_ALSCTRL1);
			return rv;
		}
	}

	rv = STK3A8X_REG_READ(alps_data, STK3A8X_REG_STATE);

	if (rv < 0)
	{
		return rv;
	}

	als_reg_config = (uint8_t)rv;
	als_reg_config &= (~(STK3A8X_STATE_EN_ALS_MASK | STK3A8X_STATE_EN_WAIT_MASK));

	if (en)
	{
		if (alps_data->als_info.enable)
		{
			err_flicker("ALS is running, ignore!\n");
		}
		else
		{
			als_reg_config |= STK3A8X_STATE_EN_ALS_MASK;
			config_changed = true;
		}
	}
	else
	{
		if (alps_data->als_info.enable)
		{
			err_flicker("Other thread used, ignore!\n");
		}
	}

	if (config_changed)
	{
		rv = STK3A8X_REG_READ_MODIFY_WRITE(alps_data, STK3A8X_REG_STATE, als_reg_config, 0xFF);

		if (rv < 0)
		{
			err_flicker("read modify write i2c (0x%X) error\n", STK3A8X_REG_STATE);
			return rv;
		}
	}

	info_flicker("cali enable done rv = 0x%x\n", rv);
	alps_data->als_info.cali_enable = en;
	return rv;
}

int32_t stk3a8x_cali_als(struct stk3a8x_data *alps_data)
{
	int32_t ret = 0;

	switch (alps_data->cali_info.cali_status)
	{
		case STK3A8X_CALI_IDLE:
			info_flicker("idle\n");
			ret = stk3a8x_als_factory_cali_enable(alps_data, true);

			if (ret < 0)
			{
				err_flicker("cali enable failed\n");
				return ret;
			}

			info_flicker("cali enable Success\n");
			ret = stk3a8x_register_cali_timer(alps_data);

			if (ret < 0)
			{
				err_flicker("regist timer failed\n");
				return ret;
			}

			break;

		case STK3A8X_CALI_RUNNING:
		case STK3A8X_CALI_FAILED:
			info_flicker("STK3A8X_CALI_RUNNING: running or failed\n");
			alps_data->als_info.als_cali_data = 0;
			ret = stk3a8x_als_get_data_without_notify(alps_data, &alps_data->als_info.als_cali_data);

			if (ret < 0)
			{
				err_flicker("STK3A8X_CALI_RUNNING: get data failed\n");
				return ret;
			}

			if (alps_data->als_info.als_cali_data > 0)
			{
				alps_data->als_info.scale = (uint32_t)(STK3A8X_ALS_CALI_TARGET_LUX * 1000) / alps_data->als_info.als_cali_data;
				alps_data->cali_info.cali_para.als_version ++;
				alps_data->cali_info.cali_para.als_scale = alps_data->als_info.scale;
				alps_data->cali_info.cali_status = STK3A8X_CALI_DONE;
				info_flicker("ALS scale = %d\n", alps_data->als_info.scale);
				ret = stk3a8x_als_factory_cali_enable(alps_data, false);

				if (ret < 0)
				{
					err_flicker("STK3A8X_CALI_RUNNING: cali disable failed\n");
					return ret;
				}
			}
			else
			{
				alps_data->cali_info.cali_status = STK3A8X_CALI_FAILED;
				alps_data->als_info.cali_failed_count ++;

				if (alps_data->als_info.cali_failed_count >= 10)
				{
					alps_data->cali_info.cali_status = STK3A8X_CALI_DONE;
				}
			}

			break;

		case STK3A8X_CALI_DONE:
			info_flicker("done\n");

			if (alps_data->als_info.cali_failed_count < 10)
			{
				info_flicker("STK3A8X_CALI_DONE: Success\n");
			}
			else
			{
				err_flicker("STK3A8X_CALI_DONE: Failed\n");
			}

			alps_data->cali_info.cali_status = STK3A8X_CALI_IDLE;

			if (alps_data->cali_info.timer_status.timer_is_active)
			{
				alps_data->cali_info.timer_status.timer_is_active = false;
				hrtimer_cancel(&alps_data->cali_timer);
			}

			break;
	}

	info_flicker("exit\n");
	return ret;
}

static void stk3a8x_cali_func(struct work_struct *work)
{
	struct stk3a8x_data *alps_data = container_of(work, struct stk3a8x_data, stk_cali_work);
	info_flicker("start\n");
	stk3a8x_cali_als(alps_data);
}

#endif
