/*
 *  stk3a8x.c - Linux kernel modules for sensortek stk3a8x
 *  proximity/ambient light sensor
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

#include <linux/module.h>
#include <linux/init.h>
#include <linux/slab.h>
#include <linux/i2c.h>
#include <linux/mutex.h>
#include <linux/kdev_t.h>
#include <linux/fs.h>
#include <linux/input.h>
#include <linux/workqueue.h>
#include <linux/irq.h>
#include <linux/delay.h>
#include <linux/sched.h>
#include <linux/kthread.h>
#include <linux/errno.h>
//#include <linux/wakelock.h>
#include <linux/interrupt.h>
#include <linux/regulator/consumer.h>
//#include <linux/sensors.h>
#include <linux/gpio.h>
#include <linux/fs.h>
#include <asm/uaccess.h>
#ifdef CONFIG_OF
	#include <linux/of_gpio.h>
#endif
#include "stk3a8x.h"
#if IS_ENABLED(CONFIG_SENSORS_FLICKER_SELF_TEST)
#include "flicker_test.h"
#endif

#define DISABLE_ASYNC_STOP_OPERATION
#define FLICKER_SENSOR_ERR_ID_SATURATION  -3

/****************************************************************************************************
 * Declaration function
 ****************************************************************************************************/
static int32_t stk3a8x_set_als_thd(struct stk3a8x_data *alps_data, uint16_t thd_h, uint16_t thd_l);
static void stk3a8x_get_data_dri(struct work_struct *work);
static void stk3a8x_get_data_polling(struct work_struct *work);
static int32_t stk3a8x_alps_set_config(struct stk3a8x_data *alps_data, bool en);
static int32_t stk_power_ctrl(struct stk3a8x_data *alps_data, bool en);
static void stk3a8x_pin_control(struct stk3a8x_data *alps_data, bool pin_set);

#ifdef STK_FIFO_ENABLE
#if defined (DISABLE_ASYNC_STOP_OPERATION)
	static void stk3a8x_fifo_stop_control(struct stk3a8x_data *alps_data);
#else
	static void stk3a8x_fifo_stop_control(struct work_struct *work);
#endif
#endif

/****************************************************************************************************
 * Declaration Initiation Setting
 ****************************************************************************************************/
stk3a8x_register_table stk3a8x_default_register_table[] =
{
#if defined(CONFIG_AMS_ALS_COMPENSATION_FOR_AUTO_BRIGHTNESS)
	{STK3A8X_REG_ALSCTRL1,          (STK3A8X_ALS_PRS1 | STK3A8X_ALS_GAIN64 | STK3A8X_ALS_IT25),   0xFF},
#else
	{STK3A8X_REG_ALSCTRL1,          (STK3A8X_ALS_PRS1 | STK3A8X_ALS_GAIN64 | STK3A8X_ALS_IT100),   0xFF},
#endif
	{0xDB,                          0x00,                                                          0x3C},
	{STK3A8X_REG_WAIT,              0x00,                                                          0xFF},
	{STK3A8X_REG_GAINCTRL,          (STK3A8X_ALS_GAIN128 | STK3A8X_ALS_IR_GAIN128),                0x06},
#ifdef STK_FIFO_ENABLE
	{STK3A8X_REG_FIFOCTRL1,         STK3A8X_FIFO_SEL_DATA_FLICKER_IR,                              0xFF},
	{STK3A8X_REG_ALSCTRL2,          (STK3A8X_IT_ALS_SEL_MASK | STK3A8X_ALS_IT672),                 0xFF},
#endif
};

stk3a8x_register_table stk3a8x_default_als_thd_table[] =
{
	{STK3A8X_REG_THDH1_ALS, 0x00, 0xFF},
	{STK3A8X_REG_THDH2_ALS, 0x00, 0xFF},
	{STK3A8X_REG_THDL1_ALS, 0xFF, 0xFF},
	{STK3A8X_REG_THDL2_ALS, 0xFF, 0xFF},
};

uint8_t stk3a8x_pid_list[STK3A8X_PID_LIST_NUM] = {0x80, 0x81, 0x82, 0x83};

uint32_t stk3a8x_power(uint32_t base, uint32_t exp)
{
	uint32_t result = 1;

	while (exp)
	{
		if (exp & 1)
			result *= base;

		exp >>= 1;
		base *= base;
	}

	return result;
}

/****************************************************************************************************
 * stk3a8x.c API
 ****************************************************************************************************/
int32_t stk3a8x_request_registry(struct stk3a8x_data *alps_data)
{
	int32_t err = 0;
	uint32_t FILE_SIZE = sizeof(stk3a8x_cali_table);
	uint8_t file_out_buf[FILE_SIZE];
	struct file  *cali_file = NULL;
	mm_segment_t fs;
	loff_t pos;
	info_flicker("start\n");
	memset(file_out_buf, 0, FILE_SIZE);
	//cali_file = filp_open(STK3A8X_CALI_FILE, O_CREAT | O_RDWR, 0644);

	if (IS_ERR(cali_file))
	{
		err = PTR_ERR(cali_file);
		err_flicker("filp_open error!err=%d,path=%s\n", err, STK3A8X_CALI_FILE);
		err_flicker("Loading initial parameters\n");
	}
#ifdef USE_KERNEL_VFS_READ_WRITE
	else
	{
		fs = get_fs();
		set_fs(KERNEL_DS);
		pos = 0;
		vfs_read(cali_file, file_out_buf, sizeof(file_out_buf), &pos);
		set_fs(fs);
		//filp_close(cali_file, NULL);
	}
#endif

	memcpy(&alps_data->cali_info.cali_para.als_version, file_out_buf, FILE_SIZE);
	info_flicker("als_version = 0x%X, als_scale = 0x%X\n",
			alps_data->cali_info.cali_para.als_version,
			alps_data->cali_info.cali_para.als_scale);
	return 0;
}

int32_t stk3a8x_update_registry(struct stk3a8x_data *alps_data)
{
	int32_t err = 0;
	uint32_t FILE_SIZE = sizeof(stk3a8x_cali_table);
	uint8_t file_buf[FILE_SIZE];
	struct file  *cali_file = NULL;
	mm_segment_t fs;
	loff_t pos;
	info_flicker("start\n");
	memset(file_buf, 0, FILE_SIZE);
	memcpy(file_buf, &alps_data->cali_info.cali_para.als_version, FILE_SIZE);
	//cali_file = filp_open(STK3A8X_CALI_FILE, O_CREAT | O_RDWR, 0666);

	if (IS_ERR(cali_file))
	{
		err = PTR_ERR(cali_file);
		err_flicker ("filp_open error!err=%d,path=%s\n", err, STK3A8X_CALI_FILE);
		return -1;
	}
#ifdef USE_KERNEL_VFS_READ_WRITE
	else {
		fs = get_fs();
		set_fs(KERNEL_DS);
		pos = 0;
		vfs_write(cali_file, file_buf, sizeof(file_buf), &pos);
		set_fs(fs);
	}
#endif

	//filp_close(cali_file, NULL);
	info_flicker("Done\n");
	return 0;
}

void stk3a8x_get_reg_default_setting(uint8_t reg, uint16_t* value)
{
	uint16_t reg_count = 0, reg_num = sizeof(stk3a8x_default_register_table) / sizeof(stk3a8x_register_table);

	for (reg_count = 0; reg_count < reg_num; reg_count++)
	{
		if (stk3a8x_default_register_table[reg_count].address == reg)
		{
			*value = (uint16_t)stk3a8x_default_register_table[reg_count].value;
			return;
		}
	}

	*value = 0x7FFF;
}

static void stk3a8x_start_timer(struct stk3a8x_data *alps_data, stk3a8x_timer_type timer_type)
{
	switch (timer_type)
	{
		case STK3A8X_DATA_TIMER_ALPS:
			if (alps_data->alps_timer_info.timer_is_exist)
			{
				if (alps_data->alps_timer_info.timer_is_active)
				{
					info_flicker("STK3A8X_DATA_TIMER_ALPS was already running\n");
				}
				else
				{
					hrtimer_start(&alps_data->alps_timer, alps_data->alps_poll_delay, HRTIMER_MODE_REL);
					alps_data->alps_timer_info.timer_is_active = true;
				}
			}

			break;
#ifdef STK_FIFO_ENABLE

		case STK3A8X_FIFO_RELEASE_TIMER:
#if !defined (DISABLE_ASYNC_STOP_OPERATION)
			if (alps_data->fifo_release_timer_info.timer_is_exist)
			{
				if (alps_data->fifo_release_timer_info.timer_is_active)
				{
					info_flicker("STK3A8X_FIFO_RELEASE_TIMER was already running\n");
				}
				else
				{
					hrtimer_start(&alps_data->fifo_release_timer, alps_data->fifo_release_delay, HRTIMER_MODE_REL);
					alps_data->fifo_release_timer_info.timer_is_active = true;
				}
			}
#else
			stk3a8x_fifo_stop_control(alps_data);
#endif

			break;
#endif
	}
}

static void stk3a8x_stop_timer(struct stk3a8x_data *alps_data, stk3a8x_timer_type timer_type)
{
	switch (timer_type)
	{
		case STK3A8X_DATA_TIMER_ALPS:
			if (alps_data->alps_timer_info.timer_is_exist)
			{
				if (alps_data->alps_timer_info.timer_is_active)
				{
					hrtimer_cancel(&alps_data->alps_timer);
					alps_data->alps_timer_info.timer_is_active = false;
				}
				else
				{
					info_flicker("STK3A8X_DATA_TIMER_ALPS was already stop\n");
				}
			}

			break;
#ifdef STK_FIFO_ENABLE

		case STK3A8X_FIFO_RELEASE_TIMER:
			if (alps_data->fifo_release_timer_info.timer_is_exist)
			{
				if (alps_data->fifo_release_timer_info.timer_is_active)
				{
					hrtimer_cancel(&alps_data->fifo_release_timer);
					alps_data->fifo_release_timer_info.timer_is_active = false;
					info_flicker("STK3A8X_FIFO_RELEASE_TIMER stop\n");
				}
				else
				{
					info_flicker("STK3A8X_FIFO_RELEASE_TIMER already stop\n");
				}
			}

			break;
#endif
	}
}

static enum hrtimer_restart stk_alps_timer_func(struct hrtimer *timer)
{
	struct stk3a8x_data *alps_data = container_of(timer, struct stk3a8x_data, alps_timer);
	queue_work(alps_data->stk_alps_wq, &alps_data->stk_alps_work);
	hrtimer_forward_now(&alps_data->alps_timer, alps_data->alps_poll_delay);
	return HRTIMER_RESTART;
}

#ifdef STK_FIFO_ENABLE
#if !defined (DISABLE_ASYNC_STOP_OPERATION)
static enum hrtimer_restart stk_fifo_stop_timer_func(struct hrtimer *timer)
{
	struct stk3a8x_data *alps_data = container_of(timer, struct stk3a8x_data, fifo_release_timer);
	queue_work(alps_data->stk_fifo_release_wq, &alps_data->stk_fifo_release_work);
	hrtimer_forward_now(&alps_data->fifo_release_timer, alps_data->fifo_release_delay);
	return HRTIMER_RESTART;
}
#endif
#endif

static int32_t stk3a8x_register_timer(struct stk3a8x_data *alps_data, stk3a8x_timer_type timer_type)
{
	info_flicker("regist 0x%X timer\n", timer_type);

	switch (timer_type)
	{
		case STK3A8X_DATA_TIMER_ALPS:
			if (alps_data->stk_alps_wq == NULL) {
				alps_data->stk_alps_wq = create_singlethread_workqueue("stk_alps_wq");
			}

			if (alps_data->stk_alps_wq) {
				INIT_WORK(&alps_data->stk_alps_work, stk3a8x_get_data_polling);
				hrtimer_init(&alps_data->alps_timer, CLOCK_MONOTONIC, HRTIMER_MODE_REL);
				alps_data->alps_poll_delay = ns_to_ktime(alps_data->als_info.als_it * NSEC_PER_MSEC);
				alps_data->alps_timer.function = stk_alps_timer_func;
				alps_data->alps_timer_info.timer_is_exist   = true;
			} else {
				alps_data->alps_timer_info.timer_is_exist   = false;
				err_flicker("could not create workqueue (stk_alps_wq)\n");
			}
			break;
#ifdef STK_FIFO_ENABLE

		case STK3A8X_FIFO_RELEASE_TIMER:
#if !defined (DISABLE_ASYNC_STOP_OPERATION)
			if (alps_data->stk_fifo_release_wq == NULL) {
				alps_data->stk_fifo_release_wq = create_singlethread_workqueue("stk_fifo_release_wq");
			}

			if (alps_data->stk_fifo_release_wq) {
				INIT_WORK(&alps_data->stk_fifo_release_work, stk3a8x_fifo_stop_control);
				hrtimer_init(&alps_data->fifo_release_timer, CLOCK_MONOTONIC, HRTIMER_MODE_REL);
				alps_data->fifo_release_delay = ns_to_ktime(20 * NSEC_PER_MSEC);
				alps_data->fifo_release_timer.function = stk_fifo_stop_timer_func;
				alps_data->fifo_release_timer_info.timer_is_exist   = true;
			}
			else {
				alps_data->fifo_release_timer_info.timer_is_exist   = false;
				err_flicker("could not create workqueue (stk_fifo_release_wq)\n");
			}
#endif
			break;
#endif
	}

	return 0;
}

static irqreturn_t stk_oss_irq_handler(int irq, void *data)
{
	struct stk3a8x_data *pData = data;
	disable_irq_nosync(irq);
	queue_work(pData->stk_irq_wq, &pData->stk_irq_work);
	return IRQ_HANDLED;
}

static int stk3a8x_register_interrupt(struct stk3a8x_data *alps_data)
{
	int irq, err = -EIO;

	if (alps_data->irq_status.irq_is_exist)
	{
		err_flicker("irq alreay regist");
		return 0;
	}

	info_flicker("irq num = %d \n", alps_data->int_pin);
	err = gpio_request(alps_data->int_pin, "stk-int");

	if (err < 0)
	{
		err_flicker("gpio_request, err=%d", err);
		return err;
	}

	alps_data->stk_irq_wq = create_singlethread_workqueue("stk_irq_wq");
	INIT_WORK(&alps_data->stk_irq_work, stk3a8x_get_data_dri);
	err = gpio_direction_input(alps_data->int_pin);

	if (err < 0)
	{
		err_flicker("gpio_direction_input, err=%d", err);
		return err;
	}

	irq = gpio_to_irq(alps_data->int_pin);
	info_flicker("int pin #=%d, irq=%d\n", alps_data->int_pin, irq);

	if (irq < 0)
	{
		err_flicker("irq number is not specified, irq # = %d, int pin=%d\n", irq, alps_data->int_pin);
		return irq;
	}

	alps_data->irq = irq;
	err = request_any_context_irq(irq, stk_oss_irq_handler, IRQF_TRIGGER_FALLING, DEVICE_NAME, alps_data);

	if (err < 0)
	{
		err_flicker("request_any_context_irq(%d) failed for (%d)\n", irq, err);
		goto err_request_any_context_irq;
	}

	alps_data->irq_status.irq_is_exist = true;
	return 0;
err_request_any_context_irq:
	gpio_free(alps_data->int_pin);
	return err;
}

void stk_als_set_new_thd(struct stk3a8x_data *alps_data, uint16_t alscode)
{
	uint16_t high_thd, low_thd;
	high_thd = alscode + STK3A8X_ALS_THRESHOLD;
	low_thd  = (alscode > STK3A8X_ALS_THRESHOLD) ? (alscode - STK3A8X_ALS_THRESHOLD) : 0;
	stk3a8x_set_als_thd(alps_data, (uint16_t)high_thd, (uint16_t)low_thd);
}

static void stk3a8x_dump_reg(struct stk3a8x_data *alps_data)
{
	uint8_t stk3a8x_reg_map[] =
	{
		STK3A8X_REG_STATE,
		STK3A8X_REG_ALSCTRL1,
		STK3A8X_REG_INTCTRL1,
		STK3A8X_REG_WAIT,
		STK3A8X_REG_THDH1_ALS,
		STK3A8X_REG_THDH2_ALS,
		STK3A8X_REG_THDL1_ALS,
		STK3A8X_REG_THDL2_ALS,
		STK3A8X_REG_FLAG,
		STK3A8X_REG_DATA1_F,
		STK3A8X_REG_DATA2_F,
		STK3A8X_REG_DATA1_IR,
		STK3A8X_REG_DATA2_IR,
		STK3A8X_REG_PDT_ID,
		STK3A8X_REG_RSRVD,
		STK3A8X_REG_GAINCTRL,
		STK3A8X_REG_FIFOCTRL1,
		STK3A8X_REG_THD1_FIFO_FCNT,
		STK3A8X_REG_THD2_FIFO_FCNT,
		STK3A8X_REG_FIFOCTRL2,
		STK3A8X_REG_FIFOFCNT1,
		STK3A8X_REG_FIFOFCNT2,
		STK3A8X_REG_FIFO_OUT,
		STK3A8X_REG_FIFO_FLAG,
		STK3A8X_REG_ALSCTRL2,
		STK3A8X_REG_SW_RESET,
		STK3A8X_REG_AGAIN,
		0xE0,
	};
	uint8_t i = 0;
	uint16_t n = sizeof(stk3a8x_reg_map) / sizeof(stk3a8x_reg_map[0]);
	int16_t reg_data;

	for (i = 0; i < n; i++)
	{
		reg_data = STK3A8X_REG_READ(alps_data, stk3a8x_reg_map[i]);

		if (reg_data < 0)
		{
			err_flicker("fail, ret=%d", reg_data);
			return;
		}
		else
		{
			info_flicker("Reg[0x%2X] = 0x%2X\n", stk3a8x_reg_map[i], (uint8_t)reg_data);
		}
	}
}

static int32_t stk3a8x_check_pid(struct stk3a8x_data *alps_data)
{
	uint8_t value;
	uint16_t i = 0, pid_num = (sizeof(stk3a8x_pid_list) / sizeof(stk3a8x_pid_list[0]));
	int err;

	err = STK3A8X_REG_READ(alps_data, STK3A8X_REG_PDT_ID);

	if (err < 0)
	{
		err_flicker("fail, ret=%d\n", err);
		return err;
	}

	value = (uint8_t)err;

	info_flicker("PID = 0x%2X\n", value);

	if ((value & 0xF0) == 0x80)
		alps_data->isTrimmed = 1;
	else
		alps_data->isTrimmed = 0;

	for (i = 0; i < pid_num; i++)
	{
		if (value == stk3a8x_pid_list[i])
		{
			return 0;
		}
	}

	return -1;
}

static int32_t stk3a8x_software_reset(struct stk3a8x_data *alps_data)
{
	int32_t r;
	r = STK3A8X_REG_WRITE(alps_data, STK3A8X_REG_SW_RESET, 0x0);

	if (r < 0)
	{
		err_flicker("software reset: read error after reset\n");
		return r;
	}

	usleep_range(13000, 15000);
	return 0;
}

#ifdef STK_ALS_CALI
static ssize_t stk3a8x_cali_store(struct device *dev, struct device_attribute *attr, const char *buf, size_t size)
{
	struct stk3a8x_data *alps_data =  dev_get_drvdata(dev);
	unsigned int data;
	int error;
	error = kstrtouint(buf, 10, &data);

	if (error)
	{
		dev_err(&alps_data->client->dev, "%s: kstrtoul failed, error=%d\n",
				__func__, error);
		return error;
	}

	dev_err(&alps_data->client->dev, "%s: Enable ALS calibration: %d\n", __func__, data);

	if (0x1 == data)
	{
		stk3a8x_cali_als(alps_data);
	}

	return size;
}
#endif

#ifdef STK_CHECK_LIB
/****************************************************************************************************
 * OTP API
 ****************************************************************************************************/
uint8_t stk3a8x_i2c_read(uint8_t addr, void* ptr)
{
	uint8_t err = 0;
	struct stk3a8x_data *tmp = (struct stk3a8x_data *)ptr;
	err = STK3A8X_REG_READ(tmp, addr);
	return err;
}

void stk3a8x_i2c_write(uint8_t addr, uint8_t value, void* ptr)
{
	struct stk3a8x_data *tmp = (struct stk3a8x_data *)ptr;
	STK3A8X_REG_WRITE(tmp, addr, value);
}

static ssize_t stk3a8x_otp_info_show(struct device *dev, struct device_attribute *attr, char *buf)
{
	struct stk3a8x_data *alps_data = dev_get_drvdata(dev);
	uint8_t otp_encrypt_key[9];
	alps_data->check_operatiop.stk_read = stk3a8x_i2c_read;
	alps_data->check_operatiop.stk_write = stk3a8x_i2c_write;
	stk_otp_encrypt(&alps_data->check_operatiop);
	stk_otp_encrypt_process(alps_data, otp_encrypt_key);
	return scnprintf(buf, PAGE_SIZE, "%2X %2X %2X %2X %2X %2X %2X %2X %2X\n",
			otp_encrypt_key[0],
			otp_encrypt_key[1],
			otp_encrypt_key[2],
			otp_encrypt_key[3],
			otp_encrypt_key[4],
			otp_encrypt_key[5],
			otp_encrypt_key[6],
			otp_encrypt_key[7],
			otp_encrypt_key[8]);
}
#endif

/****************************************************************************************************
 * ALS control API
 ****************************************************************************************************/
#ifdef STK_SW_AGC
static void stk3a8x_get_agcRatio(struct stk3a8x_data *alps_data)
{
	alps_data->als_info.als_agc_ratio_updated = 0;

	if (STK3A8X_SW_AGC_AG_LOCK == false)
	{
		alps_data->als_info.als_agc_ratio_updated = (alps_data->als_info.als_cur_again >> 4);
	}

	if (alps_data->als_info.als_cur_dgain != 0x02)
	{
		alps_data->als_info.als_agc_ratio_updated += (7 - ((alps_data->als_info.als_cur_dgain >> 4) * 2));
	}

	info_flicker("Current Gain offset = %d, Updated Gain offset = %d \n",
			alps_data->als_info.als_agc_ratio,
			alps_data->als_info.als_agc_ratio_updated);
}

static int32_t stk3a8x_get_curGain(struct stk3a8x_data *alps_data)
{
	int32_t ret = 0;
	uint8_t reg_value = 0;
	ret = STK3A8X_REG_READ(alps_data, STK3A8X_REG_GAINCTRL);

	if (ret < 0)
	{
		return ret;
	}
	else
	{
		reg_value = (uint8_t)ret;
	}

	if (reg_value & STK3A8X_ALS_DGAIN128_MASK)
	{
		alps_data->als_info.als_cur_dgain =  reg_value & STK3A8X_ALS_DGAIN128_MASK;
	}
	else
	{
		ret = STK3A8X_REG_READ(alps_data, STK3A8X_REG_ALSCTRL1);

		if (ret < 0)
		{
			return ret;
		}
		else
		{
			reg_value = (uint8_t)ret;
		}

		alps_data->als_info.als_cur_dgain = reg_value & STK3A8X_ALS_GAIN_MASK;
	}

	ret = STK3A8X_REG_READ(alps_data, 0xDB);

	if (ret < 0)
	{
		return ret;
	}
	else
	{
		reg_value = (uint8_t)ret;
	}

	alps_data->als_info.als_cur_again = reg_value & 0x30;
#ifdef STK_FIFO_ENABLE
	ret = STK3A8X_REG_READ(alps_data, STK3A8X_REG_ALSCTRL2);

	if (ret < 0)
	{
		return ret;
	}
	else
	{
		reg_value = (uint8_t)ret;
	}

	alps_data->als_info.als_it2_reg = (reg_value & STK3A8X_IT2_ALS_MASK);
#endif
	info_flicker("Current DG Gain  = %d, AG Gain = %d \n",
			alps_data->als_info.als_cur_dgain,
			alps_data->als_info.als_cur_again);
	stk3a8x_get_agcRatio(alps_data);
	return 0;
}
#ifndef STK_FIFO_ENABLE
static int32_t stk3a8x_als_gain_reg_set_L(struct stk3a8x_data *alps_data, uint8_t* rw_reg, bool is_rw)
{
	int32_t ret = 0;

	if (is_rw) // read
	{
		ret = STK3A8X_REG_READ(alps_data, STK3A8X_REG_ALSCTRL1);

		if (ret < 0)
		{
			return ret;
		}
		else
		{
			*rw_reg = (uint8_t)ret;
		}

		ret = STK3A8X_REG_READ(alps_data, STK3A8X_REG_GAINCTRL);

		if (ret < 0)
		{
			return ret;
		}
		else
		{
			*(rw_reg + 1) = (uint8_t)ret;
		}
	}
	else
	{
		ret = STK3A8X_REG_WRITE(alps_data, STK3A8X_REG_ALSCTRL1, *rw_reg);

		if (ret < 0)
		{
			err_flicker("fail, ret=%d\n", ret);
			return ret;
		}

		ret = STK3A8X_REG_WRITE(alps_data, STK3A8X_REG_GAINCTRL, *(rw_reg + 1));

		if (ret < 0)
		{
			err_flicker("fail, ret=%d\n", ret);
			return ret;
		}

		// restart FSM
		ret = STK3A8X_REG_READ_MODIFY_WRITE(alps_data,
				0x5F,
				0x01,
				0x01);

		if (ret < 0)
		{
			err_flicker("fail, ret=%d\n", ret);
			return ret;
		}
	}

	return 0;
}

static int32_t stk3a8x_als_gain_adjust_L(struct stk3a8x_data *alps_data)
{
	int32_t ret = 0;
	uint8_t reg_value[2];

	if (alps_data->als_info.als_gain_level > 2)
	{
		info_flicker("Gain level = %d\n",
				alps_data->als_info.als_gain_level);
		return -1;
	}
	else
	{
		ret = stk3a8x_als_gain_reg_set_L(alps_data, reg_value, true);

		if (ret != 0)
		{
			err_flicker("Get Gain level failed\n");
			return ret;
		}

		switch (alps_data->als_info.als_gain_level)
		{
			case 0: // ALS/C DG x128
				reg_value[1] = (reg_value[1] & 0xF9) | 0x06;
				break;

			case 1: // ALS/C DG x16
				reg_value[0] = (reg_value[0] & 0xCF) | STK3A8X_ALS_GAIN16;
				reg_value[1] = (reg_value[1] & 0xC9) | STK3A8X_ALS_IR_GAIN16;
				break;

			case 2: // ALS/C DG x1
				reg_value[0] = reg_value[0] & 0xCF;
				reg_value[1] = reg_value[1] & 0xC9;
				break;

			default:
				err_flicker("Wrong Gain level\n")
		}

		ret = stk3a8x_als_gain_reg_set_L(alps_data, reg_value, false);

		if (ret != 0)
		{
			err_flicker("Set Gain level failed\n");
			return ret;
		}

		ret = stk3a8x_get_curGain(alps_data);

		if (ret != 0)
		{
			err_flicker("Get Current Gain failed\n");
			return ret;
		}
	}

	return 0;
}

static int32_t stk3a8x_als_gain_control_L(struct stk3a8x_data *alps_data)
{
	int32_t ret = 0;
	info_flicker("F ch = %d, C ch = %d\n",
			alps_data->als_info.last_raw_data[0],
			alps_data->als_info.last_raw_data[1]);

	if (((alps_data->als_info.last_raw_data[0] > (STK3A8X_SW_AGC_ALS_IT_HTD / (1 << alps_data->als_info.als_agc_ratio))) ||
				(alps_data->als_info.last_raw_data[1] > (STK3A8X_SW_AGC_ALS_IT_HTD / (1 << alps_data->als_info.als_agc_ratio)))) &&
			(alps_data->als_info.als_gain_level < 2))
	{
		alps_data->als_info.als_gain_level++;
	}
	else if (((alps_data->als_info.last_raw_data[0] < (STK3A8X_SW_AGC_ALS_IT_LTD / (1 << alps_data->als_info.als_agc_ratio))) ||
				(alps_data->als_info.last_raw_data[1] < (STK3A8X_SW_AGC_ALS_IT_LTD / (1 << alps_data->als_info.als_agc_ratio)))) &&
			(alps_data->als_info.als_gain_level > 0))
	{
		alps_data->als_info.als_gain_level--;
	}

	if (alps_data->als_info.last_als_gain_level != alps_data->als_info.als_gain_level)
	{
		ret = stk3a8x_als_gain_adjust_L(alps_data);

		if (ret != 0)
		{
			info_flicker("Adjust Gain failed\n");
			return ret;
		}
	}

	info_flicker("Original level = %d, new level = %d\n",
			alps_data->als_info.last_als_gain_level,
			alps_data->als_info.als_gain_level);
	alps_data->als_info.last_als_gain_level = alps_data->als_info.als_gain_level;
	return 0;
}

#else
static int32_t stk3a8x_als_get_saturation(struct stk3a8x_data *alps_data, uint16_t* value)
{
	uint16_t als_it2;
	uint8_t dgain_sel = 0;
	uint32_t saturation_table;
	als_it2 = 192 + (96 * alps_data->als_info.als_it2_reg);

	switch (alps_data->als_info.als_cur_dgain)
	{
		case STK3A8X_ALS_GAIN1:
			dgain_sel = 0;
			break;

		case STK3A8X_ALS_GAIN4:
			dgain_sel = 1;
			break;

		case STK3A8X_ALS_GAIN16:
			dgain_sel = 2;
			break;

		case STK3A8X_ALS_GAIN64:
			dgain_sel = 3;
			break;

		case STK3A8X_ALS_GAIN128:
			dgain_sel = 4;
			break;
	}

	saturation_table = STK3A8X_SW_AGC_SAT_GOLDEN * stk3a8x_power(4, dgain_sel);
	saturation_table += STK3A8X_SW_AGC_SAT_STEP * stk3a8x_power(4, dgain_sel) * ((als_it2 - 192) / 96);

	if (alps_data->als_info.als_cur_dgain == STK3A8X_ALS_GAIN128)
	{
		saturation_table /= 2;
	}

	if (saturation_table > 65535)
	{
		saturation_table = 65535;
	}

	*value = (uint16_t)saturation_table;
	return 0;
}
static bool stk3a8x_als_agc_get_IdeaDG(uint8_t als_it, uint8_t * gain)
{
	uint8_t d_gain_table[5] = {0, 1, 2, 3, 4}; //d-gain value, *1,*4,*16,*64,*128
	uint8_t i = 0;
	uint16_t als_it2 = 192 + (96 * als_it);
	uint32_t saturation;
	*gain = 0;

	for (i = 0; i < 5; i++)
	{
		saturation = STK3A8X_SW_AGC_SAT_GOLDEN * stk3a8x_power(4, d_gain_table[i]);
		saturation += STK3A8X_SW_AGC_SAT_STEP * stk3a8x_power(4, d_gain_table[i]) * ((als_it2 - 192) / 96);

		if (i == 4)
		{
			saturation /= 2;
		}

		if (saturation > 0xFFFF)
		{
			if (i > 0)
			{
				i--;
			}

			*gain = d_gain_table[i];
			break;
		}
	}

	if (i == 5)
	{
		return false;
	}
	else
	{
		return true;
	}
}

static int32_t stk3a8x_als_agc_adjust_gain(struct stk3a8x_data *alps_data, bool again_sel, uint8_t again, bool dgain_sel, uint8_t dgain)
{
	int32_t ret = 0;
	uint8_t reg_value = 0;

	if (again_sel)
	{
		ret = STK3A8X_REG_READ(alps_data, 0xDB);

		if (ret < 0)
		{
			return ret;
		}
		else
		{
			reg_value = (uint8_t)ret;
		}

		reg_value = (reg_value & ~0x3C) | (again | (again >> 2));
		ret = STK3A8X_REG_READ_MODIFY_WRITE(alps_data,
				0xDB,
				reg_value,
				0xFF);

		if (ret < 0)
		{
			err_flicker("fail, ret=%d\n", ret);
			return ret;
		}

		alps_data->als_info.als_cur_again = again;
	}

	if (dgain_sel)
	{
		//read
		uint8_t temp_reg = 0;
		ret = STK3A8X_REG_READ(alps_data, STK3A8X_REG_ALSCTRL1);

		if (ret < 0)
		{
			return ret;
		}
		else
		{
			reg_value = (uint8_t)ret;
		}

		ret = STK3A8X_REG_READ(alps_data, STK3A8X_REG_GAINCTRL);

		if (ret < 0)
		{
			return ret;
		}
		else
		{
			temp_reg = (uint8_t)ret;
		}

		if (dgain == STK3A8X_ALS_GAIN128)
		{
			reg_value &= (~STK3A8X_ALS_GAIN_MASK);
			temp_reg = (temp_reg & ~STK3A8X_ALS_GAIN_C_MASK) | (dgain | STK3A8X_ALS_GAIN128_C_MASK);
		}
		else
		{
			temp_reg = (temp_reg & ~(STK3A8X_ALS_DGAIN128_MASK | STK3A8X_ALS_GAIN128_C_MASK | STK3A8X_ALS_GAIN_C_MASK)) | dgain;
			reg_value = (reg_value & ~STK3A8X_ALS_GAIN_MASK) | dgain;
		}

		//write
		ret = STK3A8X_REG_READ_MODIFY_WRITE(alps_data,
				STK3A8X_REG_ALSCTRL1,
				reg_value,
				0xFF);

		if (ret < 0)
		{
			err_flicker("fail, ret=%d\n", ret);
			return ret;
		}

		ret = STK3A8X_REG_READ_MODIFY_WRITE(alps_data,
				STK3A8X_REG_GAINCTRL,
				temp_reg,
				0xFF);

		if (ret < 0)
		{
			err_flicker("fail, ret=%d\n", ret);
			return ret;
		}

		alps_data->als_info.als_cur_dgain = dgain;
	}

	return 1;
}

static uint8_t stk3a8x_als_agc_adjust_down_gain(struct stk3a8x_data *alps_data)
{
	uint8_t again_sel = 0, als_dgain = 0, dgain_sel = 0, idea_gain = 0;
	bool gain_sel = true;
	int8_t ret;

	if(!alps_data->als_info.enable)
	{
		info_flicker("flicker disabled");
		return 0;
	}

	if (alps_data->als_info.als_cur_dgain & STK3A8X_ALS_DGAIN128_MASK)
	{
		als_dgain = 4;
	}
	else
	{
		als_dgain = alps_data->als_info.als_cur_dgain >> STK3A8X_ALS_GAIN_SHIFT;
	}

	//dgain down
	if (stk3a8x_als_agc_get_IdeaDG(alps_data->als_info.als_it2_reg, &idea_gain) && idea_gain < als_dgain)
	{
		switch (alps_data->als_info.als_cur_dgain)
		{
			case STK3A8X_ALS_GAIN128:
				dgain_sel = STK3A8X_ALS_GAIN64;
				break;

			case STK3A8X_ALS_GAIN64:
				dgain_sel = STK3A8X_ALS_GAIN16;
				break;

			case STK3A8X_ALS_GAIN16:
				dgain_sel = STK3A8X_ALS_GAIN4;
				break;

			case STK3A8X_ALS_GAIN4:
				dgain_sel = STK3A8X_ALS_GAIN1;
				break;

			default:
				gain_sel = false;
				break;
		}

		if (!gain_sel)
		{
			return 0;
		}

		ret = stk3a8x_als_agc_adjust_gain(alps_data, false, 0, gain_sel, dgain_sel);

		if (!ret)
		{
			return 0;
		}

		stk3a8x_enable_fifo(alps_data, false);
		stk3a8x_enable_fifo(alps_data, true);
		alps_data->als_info.is_gain_changing = true;
	}
	else //again down
	{
		if (alps_data->als_info.als_cur_again == 0x20)
		{
			return 0;
		}
		else if (alps_data->als_info.als_cur_again  == 0x0)
		{
			again_sel = 0x10;
		}
		else
		{
			again_sel = 0x20;
		}

		ret = stk3a8x_als_agc_adjust_gain(alps_data, true, again_sel, false, 0);

		if (!ret)
		{
			return 0;
		}

		stk3a8x_enable_fifo(alps_data, false);
		stk3a8x_enable_fifo(alps_data, true);
		alps_data->als_info.is_gain_changing = true;
	}

	if (alps_data->als_info.is_gain_changing == true)
	{
		if (alps_data->alps_timer_info.timer_is_active)
		{
			stk3a8x_stop_timer(alps_data, STK3A8X_DATA_TIMER_ALPS);
		}

		if (alps_data->alps_timer_info.timer_is_active == false)
		{
			stk3a8x_start_timer(alps_data, STK3A8X_DATA_TIMER_ALPS);
		}
	}

	info_flicker("Final D-GAIN:0x%02X, A-GAIN:0x%02X\n",
			alps_data->als_info.als_cur_dgain,
			alps_data->als_info.als_cur_again);
	stk3a8x_get_agcRatio(alps_data);
	return 1;
}

static uint8_t stk3a8x_als_agc_adjust_up_gain(struct stk3a8x_data *alps_data)
{
	uint8_t  again_sel = 0, dgain_sel = 0, ret;
	bool gain_sel = true;

	if(!alps_data->als_info.enable)
	{
		info_flicker("flicker disabled");
		return 0;
	}

	//dgain up.
	if ( alps_data->als_info.als_cur_again == 0x0)
	{
		switch (alps_data->als_info.als_cur_dgain)
		{
			case STK3A8X_ALS_GAIN1:
				dgain_sel = STK3A8X_ALS_GAIN4;
				break;

			case STK3A8X_ALS_GAIN4:
				dgain_sel = STK3A8X_ALS_GAIN16;
				break;

			case STK3A8X_ALS_GAIN16:
				dgain_sel = STK3A8X_ALS_GAIN64;
				break;

			case STK3A8X_ALS_GAIN64:
				dgain_sel = STK3A8X_ALS_GAIN128;
				break;

			default:
				gain_sel = false;
				break;
		}

		if (!gain_sel)
		{
			return 0;
		}

		ret = stk3a8x_als_agc_adjust_gain(alps_data, false, 0, gain_sel, dgain_sel);

		if (!ret)
		{
			return 0;
		}

		stk3a8x_enable_fifo(alps_data, false);
		stk3a8x_enable_fifo(alps_data, true);
		alps_data->als_info.is_gain_changing = true;
	}
	else //again up
	{
		if (alps_data->als_info.als_cur_again == 0x0)
		{
			return 0;
		}
		else if (alps_data->als_info.als_cur_again == 0x10)
		{
			again_sel = 0x00;
		}
		else
		{
			again_sel = 0x10;
		}

		ret = stk3a8x_als_agc_adjust_gain(alps_data, true, again_sel, false, 0);

		if (!ret)
		{
			return 0;
		}

		stk3a8x_enable_fifo(alps_data, false);
		stk3a8x_enable_fifo(alps_data, true);
		alps_data->als_info.is_gain_changing = true;
	}

	if (alps_data->als_info.is_gain_changing == true)
	{
		if (alps_data->alps_timer_info.timer_is_active)
		{
			stk3a8x_stop_timer(alps_data, STK3A8X_DATA_TIMER_ALPS);
		}

		if (alps_data->alps_timer_info.timer_is_active == false)
		{
			stk3a8x_start_timer(alps_data, STK3A8X_DATA_TIMER_ALPS);
		}
	}

	info_flicker("Final D-GAIN:0x%02X, A-GAIN:0x%02X\n",
			alps_data->als_info.als_cur_dgain,
			alps_data->als_info.als_cur_again);
	stk3a8x_get_agcRatio(alps_data);
	return 1;
}

void stk3a8x_als_agc_check(struct stk3a8x_data *alps_data)
{
	int32_t ret;
	uint16_t als_saturation = 0;
	uint16_t max = 0, min = 0xFFFF;
	ret = stk3a8x_als_get_saturation(alps_data, &als_saturation);

	if (ret == 0)
	{
		if (als_saturation)
		{
			alps_data->als_info.als_sat_thd_h = (uint16_t)((als_saturation * STK3A8X_SW_AGC_H_THD) / 100);
			alps_data->als_info.als_sat_thd_l = (uint16_t)((als_saturation * STK3A8X_SW_AGC_L_THD) / 100);
		}
		else
		{
			return;
		}

		if (alps_data->fifo_info.last_frame_count < STK3A8X_SW_AGC_DATA_BLOCK)
		{
			err_flicker("AGC check size is not enough\n");
			return;
		}
		else
		{
			if (alps_data->als_info.als_sat_thd_h && alps_data->als_info.als_sat_thd_l)
			{
				uint16_t i;

				for (i = 0; i < STK3A8X_SW_AGC_DATA_BLOCK; i++)
				{
					if (alps_data->fifo_info.fifo_data0[i] > max)
					{
						max = alps_data->fifo_info.fifo_data0[i];
					}

					if (alps_data->fifo_info.fifo_data0[i] < min)
					{
						min = alps_data->fifo_info.fifo_data0[i];
					}
				}

				dbg_flicker("AGC check: max=%d, min=%d\n", max, min);
			}
		}

		alps_data->als_info.is_gain_changing = false;
		alps_data->als_info.is_data_saturated = false;

		if (max > alps_data->als_info.als_sat_thd_h)
		{
			stk3a8x_als_agc_adjust_down_gain(alps_data);

			if (max >= als_saturation && alps_data->als_info.is_gain_changing == false)
			{
				alps_data->als_info.is_data_saturated = true;
			}
		}
		else if (max < alps_data->als_info.als_sat_thd_l)
		{
			stk3a8x_als_agc_adjust_up_gain(alps_data);
		}
	}

	return;
}
#endif
#endif

static int32_t stk3a8x_als_latency(struct stk3a8x_data *alps_data)
{
	int32_t ret;
	uint8_t reg_value = 0;
	uint32_t wait_time = 0, als_it_time = 0;
	bool is_need_wait = false;
	ret = STK3A8X_REG_READ(alps_data, STK3A8X_REG_STATE);

	if (ret < 0)
	{
		return ret;
	}
	else
	{
		reg_value = (uint8_t)ret;
	}

	if (reg_value & 0x4)
	{
		is_need_wait = true;
		ret = STK3A8X_REG_READ(alps_data, STK3A8X_REG_WAIT);

		if (ret < 0)
		{
			return ret;
		}
		else
		{
			reg_value = (uint8_t)ret;
		}

		wait_time = (reg_value + 1) * 154 / 100;
	}

#ifdef STK_FIFO_ENABLE
	ret = STK3A8X_REG_READ(alps_data, STK3A8X_REG_ALSCTRL2);

	if (ret < 0)
	{
		return ret;
	}
	else
	{
		reg_value = (uint8_t)ret;
	}

	if ((reg_value & 0x80) >> 7) //using IT2
	{
		als_it_time = (reg_value & 0x1f) * 96 + 192 + 96;
		als_it_time *= alps_data->fifo_info.block_size;
		als_it_time /= 1000;
	}
	else //using IT
#endif
	{
		ret = STK3A8X_REG_READ(alps_data, STK3A8X_REG_ALSCTRL1);

		if (ret < 0)
		{
			return ret;
		}
		else
		{
			reg_value = (uint8_t)ret;
		}

		reg_value &= 0xf;
		als_it_time = stk3a8x_power(2, reg_value) * 3125 / 1000;
	}

	if (is_need_wait)
	{
		alps_data->als_info.als_it = (als_it_time * 12 / 10) + wait_time;
	}
	else
	{
		alps_data->als_info.als_it = als_it_time * 12 / 10;
	}

	info_flicker("ALS IT = %d\n", alps_data->als_info.als_it);
	return 0;
}

static int32_t stk3a8x_set_als_thd(struct stk3a8x_data *alps_data, uint16_t thd_h, uint16_t thd_l)
{
	unsigned char val[4];
	int ret;
	val[0] = (thd_h & 0xFF00) >> 8;
	val[1] = thd_h & 0x00FF;
	val[2] = (thd_l & 0xFF00) >> 8;
	val[3] = thd_l & 0x00FF;
	ret = STK3A8X_REG_WRITE_BLOCK(alps_data, STK3A8X_REG_THDH1_ALS, val, sizeof(val));

	if (ret < 0)
	{
		err_flicker("fail, ret=%d\n", ret);
	}

	return ret;
}

static void stk_als_report(struct stk3a8x_data *alps_data, int als)
{
	input_report_abs(alps_data->als_input_dev, ABS_MISC, als);
	input_sync(alps_data->als_input_dev);
	info_flicker("als input event %d lux\n", als);
}

static void stk_sec_report(struct stk3a8x_data *alps_data)
{
	sec_raw_data *raw_data = &alps_data->als_info.raw_data;
	//ir = 1.6*wide - 4.3 *clear
	uint32_t temp_wide = 1600 * raw_data->wideband;
	uint32_t temp_clear = 4300 * raw_data->clear;
	uint32_t temp_ir = 0;
	static int cnt=0;

	if (alps_data->saturation || alps_data->als_info.is_data_saturated) {
		raw_data->ir = 0;
		temp_ir = FLICKER_SENSOR_ERR_ID_SATURATION;
	} else {
		if (temp_wide < temp_clear)
			raw_data->ir = 0;
		else
			raw_data->ir = (temp_wide - temp_clear)/1000;

		temp_ir = raw_data->ir;
	}

	if (cnt++ >= 10) {
		info_flicker("I:%d W:%d C:%d Gain:%d | Flicker:%dHz",
				temp_ir, raw_data->wideband, raw_data->clear, raw_data->wide_gain, raw_data->flicker);
		cnt = 0;
	}

	input_report_rel(alps_data->als_input_dev, REL_X, temp_ir + 1);
	input_report_rel(alps_data->als_input_dev, REL_RY, raw_data->clear + 1);
	input_report_abs(alps_data->als_input_dev, ABS_Y, raw_data->wide_gain + 1);
	input_sync(alps_data->als_input_dev);
	msleep_interruptible(30);
	input_report_rel(alps_data->als_input_dev, REL_RZ, raw_data->flicker + 1);
	input_sync(alps_data->als_input_dev);

#if IS_ENABLED(CONFIG_SENSORS_FLICKER_SELF_TEST)
	als_eol_update_als(raw_data->ir, raw_data->clear, raw_data->wideband, 0);
	als_eol_update_flicker(raw_data->flicker);
#endif
	alps_data->saturation = false;
}

static int32_t stk_power_ctrl(struct stk3a8x_data *alps_data, bool en)
{
	int rc = 0;
	info_flicker("%s", en?"ON":"OFF");

	if (en) {
		if (alps_data->vdd_regulator != NULL) {
			if(!alps_data->vdd_1p8_enable) {
				rc = regulator_enable(alps_data->vdd_regulator);
				if (rc) {
					err_flicker("%s - enable vdd_1p8 failed, rc=%d\n", __func__, rc);
					goto enable_vdd_1p8_failed;
				} else {
					alps_data->vdd_1p8_enable = true;
					info_flicker("%s - enable vdd_1p8 done, rc=%d\n", __func__, rc);
					msleep_interruptible(40);
				}
			} else {
				info_flicker("%s - vdd_1p8 already enabled, en=%d\n", __func__, alps_data->vdd_1p8_enable);
			}
		}

		if (alps_data->vbus_regulator != NULL) {
			if (!alps_data->vbus_1p8_enable && !regulator_is_enabled(alps_data->vbus_regulator)) {
				rc = regulator_enable(alps_data->vbus_regulator);
				if (rc) {
					err_flicker("%s - enable vbus_1p8 failed, rc=%d\n",	__func__, rc);
					goto enable_vbus_1p8_failed;
				} else {
					alps_data->vbus_1p8_enable = true;
					info_flicker("%s - enable vbus_1p8 done, rc=%d\n", __func__, rc);
				}
			} else {
				info_flicker("%s - vbus_1p8 already enabled, en=%d\n", __func__, alps_data->vbus_1p8_enable);
			}
		}
	} else {
		if (alps_data->vbus_regulator != NULL) {
			if(alps_data->vbus_1p8_enable) {
				rc = regulator_disable(alps_data->vbus_regulator);
				if (rc) {
					err_flicker("%s - disable vbus_1p8 failed, rc=%d\n", __func__, rc);
				} else {
					alps_data->vbus_1p8_enable = false;
					info_flicker("%s - disable vbus_1p8 done, rc=%d\n", __func__, rc);
				}
			} else {
				info_flicker("%s - vbus_1p8 already disabled, en=%d\n", __func__, alps_data->vbus_1p8_enable);
			}
		}

		if (alps_data->vdd_regulator != NULL) {
			if(alps_data->vdd_1p8_enable) {
				rc = regulator_disable(alps_data->vdd_regulator);
				if (rc) {
					err_flicker("%s - disable vdd_1p8 failed, rc=%d\n", __func__, rc);
				} else {
					alps_data->vdd_1p8_enable = false;
					info_flicker("%s - disable vdd_1p8 done, rc=%d\n", __func__, rc);
				}
			} else {
				info_flicker("%s - vdd_1p8 already disabled, en=%d\n", __func__, alps_data->vdd_1p8_enable);
			}
		}
	}

	goto done;

enable_vbus_1p8_failed:
	if (alps_data->vdd_regulator != NULL) {
		if(alps_data->vdd_1p8_enable) {
			rc = regulator_disable(alps_data->vdd_regulator);
			if (rc) {
				err_flicker("%s - disable vdd_1p8 failed, rc=%d\n", __func__, rc);
			} else {
				alps_data->vdd_1p8_enable = false;
				info_flicker("%s - disable vdd_1p8 done, rc=%d\n", __func__, rc);
			}
		} else {
			info_flicker("%s - vdd_1p8 already disabled, en=%d\n", __func__, alps_data->vdd_1p8_enable);
		}
	}

done:
enable_vdd_1p8_failed:

	return rc;
}

static int32_t stk3a8x_enable_als(struct stk3a8x_data *alps_data, bool en)
{
	int32_t ret = 0;
	uint8_t reg_value = 0;

	if (alps_data->als_info.enable == en)
	{
		err_flicker("ALS already set\n");
		return ret;
	}

	ret = STK3A8X_REG_READ(alps_data, STK3A8X_REG_STATE);

	if (ret < 0)
	{
		return ret;
	}
	else
	{
		reg_value = (uint8_t)ret;
	}

	reg_value &= (~(STK3A8X_STATE_EN_WAIT_MASK | STK3A8X_STATE_EN_ALS_MASK));

	if (en)
	{
		reg_value |= STK3A8X_STATE_EN_ALS_MASK;
	}
	else
	{
#ifdef STK_ALS_CALI

		if (alps_data->als_info.cali_enable)
		{
			reg_value |= STK3A8X_STATE_EN_ALS_MASK;
		}

#endif
	}

	ret = STK3A8X_REG_READ_MODIFY_WRITE(alps_data,
			STK3A8X_REG_STATE,
			reg_value,
			0xFF);

	if (ret < 0)
	{
		return ret;
	}

	alps_data->als_info.enable = en;
	return ret;
}

#if 0
static uint16_t stk_als_max_min(uint16_t* als_raw_data)
{
	uint8_t loop_count = 0;
	uint16_t als_max = 0, als_min = 0xFFFF;

	for (loop_count = 0; loop_count < STK_CHANNEL_NUMBER; loop_count++)
	{
		if ( *(als_raw_data + loop_count + 1) > als_max )
		{
			als_max = *(als_raw_data + loop_count + 1);
		}

		if ( *(als_raw_data + loop_count + 1) < als_min )
		{
			als_min = *(als_raw_data + loop_count + 1);
		}
	}

	return (als_max - als_min);
}


static void stk_als_check_code(struct stk3a8x_data *alps_data, uint16_t* als_raw_data)
{
	uint16_t average_data = 0, diff_data = 0;
	uint8_t loop_count = 0, channel_count = 0;
	bool is_comp = false;

	if (*als_raw_data < STK_CHANNEL_GOLDEN)
	{
		printk(KERN_ERR "%s: ALS raw data too small\n");
		return;
	}

	average_data = (*als_raw_data) / STK_CHANNEL_NUMBER;
	diff_data = stk_als_max_min(als_raw_data);

	if (diff_data < average_data)
	{
		printk(KERN_ERR "%s: ALS max_min is normal\n");
		return;
	}

	for (loop_count = 0; loop_count < STK_CHANNEL_NUMBER; loop_count++)
	{
		if ((average_data > *(als_raw_data + loop_count + 1)) &&
				(average_data - (*(als_raw_data + loop_count + 1))) >= (average_data - STK_CHANNEL_OFFSET))
		{
			is_comp = true;
			channel_count ++;
		}
	}

	if (is_comp)
	{
		*als_raw_data = ((*als_raw_data * STK_CHANNEL_NUMBER) / (STK_CHANNEL_NUMBER - channel_count));
		printk(KERN_ERR "%s: als_raw_data = %u\n", *als_raw_data);
	}
}
#endif

static int32_t stk3a8x_als_get_data(struct stk3a8x_data *alps_data)
{
	uint8_t  raw_data[4];
	uint16_t als_raw_data[2];
	int      loop_count;
	int      err = 0;
	err = STK3A8X_REG_BLOCK_READ(alps_data, STK3A8X_REG_DATA1_F, 4, &raw_data[0]);

	if (err < 0)
	{
		err_flicker("return err \n");
		return err;
	}

	for (loop_count = 0; loop_count < (sizeof(als_raw_data) / sizeof(als_raw_data[0])); loop_count++)
	{
		*(als_raw_data + loop_count ) = (*(raw_data + (2 * loop_count)) << 8 | *(raw_data + (2 * loop_count + 1) ));
	}

	memcpy(alps_data->als_info.last_raw_data, als_raw_data, sizeof(als_raw_data));

	if (alps_data->als_info.is_dri)
	{
		stk_als_set_new_thd(alps_data, alps_data->als_info.last_raw_data[0]);
	}

	return err;
}

static ssize_t stk_als_code_show(struct device *dev, struct device_attribute *attr, char *buf)
{
	struct stk3a8x_data *alps_data =  dev_get_drvdata(dev);
	info_flicker("F ch = %d, C ch = %d\n",
			__func__,
			alps_data->als_info.last_raw_data[0],
			alps_data->als_info.last_raw_data[1]);
	return scnprintf(buf, PAGE_SIZE, "%u\n", (uint32_t)alps_data->als_info.last_raw_data[0]);
	return scnprintf(buf, PAGE_SIZE, "F ch = %d, C ch = %d\n", alps_data->als_info.last_raw_data[0], alps_data->als_info.last_raw_data[1]);

}

#if defined(CONFIG_AMS_ALS_COMPENSATION_FOR_AUTO_BRIGHTNESS)
void stk_als_init(struct stk3a8x_data *alps_data)
{
	uint8_t val, val2, val3;

	val = STK3A8X_REG_READ(alps_data, STK3A8X_REG_ALSCTRL1);
	val2 = STK3A8X_REG_READ(alps_data, STK3A8X_REG_GAINCTRL);
	val3 = STK3A8X_REG_READ(alps_data, STK3A8X_REG_ALSCTRL2);

	info_flicker("%s start 0x%x, 0x%x, 0x%x", __func__, val, val2, val3);

	val &= (~STK3A8X_ALS_GAIN_MASK);
	val |= STK3A8X_ALS_GAIN16;

	val2 &= (~(STK3A8X_ALS_DGAIN128_MASK | STK3A8X_ALS_GAIN128_C_MASK | STK3A8X_ALS_GAIN_C_MASK));
	val2 |= STK3A8X_ALS_IR_GAIN16;

	val3 &= (~STK3A8X_IT_ALS_SEL_MASK);

	STK3A8X_REG_WRITE(alps_data, STK3A8X_REG_ALSCTRL1, val);
	STK3A8X_REG_WRITE(alps_data, STK3A8X_REG_GAINCTRL, val2);
	STK3A8X_REG_WRITE(alps_data, STK3A8X_REG_ALSCTRL2, val3);

	info_flicker("%s end 0x%x, 0x%x, 0x%x", __func__, val, val2, val3);
}

void stk_als_start(struct stk3a8x_data *alps_data)
{
	info_flicker("stk_als_start");

	if (!alps_data->flicker_flag) {
		stk3a8x_alps_set_config(alps_data, 1);
		stk_als_init(alps_data);
	}
}

void stk_als_stop(struct stk3a8x_data *alps_data)
{
	info_flicker("%s", __func__);

	if (!alps_data->flicker_flag)
		stk3a8x_alps_set_config(alps_data, 0);
}

static ssize_t stk_rear_als_enable_store(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t size)
{
	struct stk3a8x_data *data = dev_get_drvdata(dev);
	bool value;

	mutex_lock(&data->enable_lock);

	if (strtobool(buf, &value)) {
		mutex_unlock(&data->enable_lock);
		return -EINVAL;
	}

	info_flicker("en : %d, c : %d\n", value, data->als_info.enable);
	if (data->als_flag == value) {
		mutex_unlock(&data->enable_lock);
		return size;
	}

	data->als_flag = value;

	if (value)
		stk_als_start(data);
	else
		stk_als_stop(data);

	mutex_unlock(&data->enable_lock);

	return size;
}

static ssize_t stk_rear_als_enable_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct stk3a8x_data *data = dev_get_drvdata(dev);

	return snprintf(buf, PAGE_SIZE, "%u\n", data->als_flag);
}

static ssize_t stk_rear_als_data_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct stk3a8x_data *data = dev_get_drvdata(dev);
	uint8_t raw_data[4];
	uint16_t als_raw_data[2];
	int loop_count;
	int err = 0;

	if (data->als_info.enable && data->als_flag && !data->flicker_flag) {
		err = STK3A8X_REG_BLOCK_READ(data, STK3A8X_REG_DATA1_F, 4, &raw_data[0]);

		if (err < 0) {
			err_flicker("fail to read als data\n");
			return snprintf(buf, PAGE_SIZE, "-6, -6");
		}

		for (loop_count = 0; loop_count < (sizeof(als_raw_data) / sizeof(als_raw_data[0])); loop_count++) {
			*(als_raw_data + loop_count) = (*(raw_data + (2 * loop_count)) << 8 | *(raw_data + (2 * loop_count + 1)));
		}
	} else {
		return snprintf(buf, PAGE_SIZE, "-1, -1");
	}

	return snprintf(buf, PAGE_SIZE, "%u, %u", als_raw_data[0], als_raw_data[1]);
}
#endif /* CONFIG_AMS_ALS_COMPENSATION_FOR_AUTO_BRIGHTNESS */

static ssize_t stk_als_enable_show(struct device *dev,
		struct device_attribute *attr,
		char *buf)
{
	struct stk3a8x_data *alps_data =  dev_get_drvdata(dev);
	int32_t ret;
	ret = STK3A8X_REG_READ(alps_data, STK3A8X_REG_STATE);

	if (ret < 0)
		return ret;

	ret = (ret & STK3A8X_STATE_EN_ALS_MASK) ? 1 : 0;
	return scnprintf(buf, PAGE_SIZE, "%d\n", ret);
}

static ssize_t stk_als_enable_store(struct device *dev,
		struct device_attribute *attr,
		const char *buf,
		size_t size)
{
	struct stk3a8x_data *alps_data =  dev_get_drvdata(dev);
	unsigned int data;
	int error;

#if defined(CONFIG_AMS_ALS_COMPENSATION_FOR_AUTO_BRIGHTNESS)
	mutex_lock(&alps_data->enable_lock);
#endif

	error = kstrtouint(buf, 10, &data);
	if (error)
	{
		dev_err(&alps_data->client->dev, "%s: kstrtoul failed, error=%d\n",
				__func__, error);
#if defined(CONFIG_AMS_ALS_COMPENSATION_FOR_AUTO_BRIGHTNESS)
		mutex_unlock(&alps_data->enable_lock);
#endif
		return error;
	}

#if IS_ENABLED(CONFIG_SENSORS_FLICKER_SELF_TEST)
	if (alps_data->eol_enabled) {
		dev_err(&alps_data->client->dev, "%s: TEST RUNNING. recover %d after finish test", __func__, data);
		alps_data->recover_state = data;
#if defined(CONFIG_AMS_ALS_COMPENSATION_FOR_AUTO_BRIGHTNESS)
		mutex_unlock(&alps_data->enable_lock);
#endif
		return size;
	}
#endif

	if ((1 == data) || (0 == data))
	{
#if defined(CONFIG_AMS_ALS_COMPENSATION_FOR_AUTO_BRIGHTNESS)
		alps_data->flicker_flag = (bool)data;

		if (alps_data->flicker_flag && alps_data->als_flag) {
			stk3a8x_enable_als(alps_data, false);
			stk3a8x_enable_fifo(alps_data, false);
		}
#endif
		dev_err(&alps_data->client->dev, "%s: Enable ALS : %d\n", __func__, data);
		stk3a8x_alps_set_config(alps_data, data);
#if defined(CONFIG_AMS_ALS_COMPENSATION_FOR_AUTO_BRIGHTNESS)
		if (!alps_data->flicker_flag && alps_data->als_flag) {
			stk3a8x_alps_set_config(alps_data, true);
			stk_als_init(alps_data);
		}
#endif
	}
#if defined(CONFIG_AMS_ALS_COMPENSATION_FOR_AUTO_BRIGHTNESS)
	mutex_unlock(&alps_data->enable_lock);
#endif
	return size;
}

static ssize_t stk_als_lux_show(struct device *dev, struct device_attribute *attr, char *buf)
{
	struct stk3a8x_data *alps_data = dev_get_drvdata(dev);
	return scnprintf(buf, PAGE_SIZE, "%u lux\n", alps_data->als_info.last_raw_data[0] * alps_data->als_info.scale / 1000);
}

static ssize_t stk_als_lux_store(struct device *dev, struct device_attribute *attr, const char *buf, size_t size)
{
	struct stk3a8x_data *alps_data =  dev_get_drvdata(dev);
	unsigned long value = 0;
	int ret;
	ret = kstrtoul(buf, 16, &value);

	if (ret < 0)
	{
		err_flicker("kstrtoul failed, ret=0x%x\n", ret);
		return ret;
	}

	stk_als_report(alps_data, value);
	return size;
}


static ssize_t stk_als_transmittance_show(struct device *dev, struct device_attribute *attr, char *buf)
{
	struct stk3a8x_data *alps_data =  dev_get_drvdata(dev);
	int32_t transmittance;
	transmittance = alps_data->als_info.scale;
	return scnprintf(buf, PAGE_SIZE, "%d\n", transmittance);
}


static ssize_t stk_als_transmittance_store(struct device *dev, struct device_attribute *attr, const char *buf, size_t size)
{
	struct stk3a8x_data *alps_data =  dev_get_drvdata(dev);
	unsigned long value = 0;
	int ret;
	ret = kstrtoul(buf, 10, &value);

	if (ret < 0)
	{
		err_flicker("kstrtoul failed, ret=0x%x\n", ret);
		return ret;
	}

	alps_data->als_info.scale = value;
	return size;
}

static ssize_t stk3a8x_als_registry_show(struct device *dev, struct device_attribute *attr, char *buf)
{
	struct stk3a8x_data *alps_data = dev_get_drvdata(dev);
	stk3a8x_update_registry(alps_data);
	return scnprintf(buf, PAGE_SIZE, "als scale = %d (Ver. %d)\n",
			alps_data->cali_info.cali_para.als_scale,
			alps_data->cali_info.cali_para.als_version);
}

static ssize_t stk3a8x_registry_store(struct device *dev, struct device_attribute *attr, const char *buf, size_t size)
{
	struct stk3a8x_data *alps_data =  dev_get_drvdata(dev);
	unsigned long value = 0;
	int ret;
	ret = kstrtoul(buf, 10, &value);

	if (ret < 0)
	{
		err_flicker("kstrtoul failed, ret=0x%x\n", ret);
		return ret;
	}

	ret = stk3a8x_update_registry(alps_data);

	if (ret < 0)
		err_flicker("update registry fail!\n");
	else
		info_flicker("update registry success!\n");

	return size;
}

#ifdef STK_ALS_CALI
static ssize_t stk3a8x_als_cali_show(struct device *dev, struct device_attribute *attr, char *buf)
{
	struct stk3a8x_data *alps_data = dev_get_drvdata(dev);
	stk3a8x_request_registry(alps_data);
	return scnprintf(buf, PAGE_SIZE, "als scale = %d (Ver. %d)\n",
			alps_data->cali_info.cali_para.als_scale,
			alps_data->cali_info.cali_para.als_version);
}
#endif

static ssize_t stk_recv_show(struct device *dev, struct device_attribute *attr, char *buf)
{
	struct stk3a8x_data *alps_data =  dev_get_drvdata(dev);
	return scnprintf(buf, PAGE_SIZE, "0x%04X\n", atomic_read(&alps_data->recv_reg));
}

static ssize_t stk_recv_store(struct device *dev, struct device_attribute *attr, const char *buf, size_t size)
{
	unsigned long value = 0;
	int ret;
	int32_t recv_data;
	struct stk3a8x_data *alps_data = dev_get_drvdata(dev);

	if ((ret = kstrtoul(buf, 16, &value)) < 0)
	{
		err_flicker("kstrtoul failed, ret=0x%x\n", ret);
		return ret;
	}

	atomic_set(&alps_data->recv_reg, 0);
	recv_data = STK3A8X_REG_READ(alps_data, value);
	atomic_set(&alps_data->recv_reg, recv_data);
	return size;
}

static ssize_t stk_send_show(struct device *dev, struct device_attribute *attr, char *buf)
{
	return 0;
}

static ssize_t stk_send_store(struct device *dev, struct device_attribute *attr, const char *buf, size_t size)
{
	unsigned char addr, cmd;
	unsigned long temp_addr, temp_cmd;
	int32_t ret, i;
	char *token[10];
	struct stk3a8x_data *alps_data =  dev_get_drvdata(dev);

	for (i = 0; i < 2; i++) {
		if ((token[i] = strsep((char **)&buf, " ")) == NULL) {
			err_flicker("invalid input parameter (need two parameters: address and data)\n");
			return -EINVAL;
		}
	}

	if ((ret = kstrtoul(token[0], 16, &temp_addr)) < 0)
	{
		err_flicker("kstrtoul failed, ret=0x%x\n", ret);
		return ret;
	}

	if ((ret = kstrtoul(token[1], 16, &temp_cmd)) < 0)
	{
		err_flicker("kstrtoul failed, ret=0x%x\n", ret);
		return ret;
	}

	addr = temp_addr & 0xFF;
	cmd = temp_cmd & 0xFF;
	info_flicker("write reg 0x%x=0x%x\n", addr, cmd);
	ret = STK3A8X_REG_WRITE(alps_data, addr, cmd);

	if (0 != ret)
	{
		err_flicker("stk3a8x_i2c_smbus_write_byte_data fail\n");
		return ret;
	}

	return size;
}

static void stk3a8x_get_data_polling(struct work_struct *work)
{
	struct stk3a8x_data *alps_data = container_of(work, struct stk3a8x_data, stk_alps_work);
	//int32_t als_lux = 0;
	uint32_t temp_data[2];
#ifdef STK_FIFO_ENABLE
	stk3a8x_get_fifo_data_polling(alps_data);
#else
	int32_t reg_value;
	uint8_t flag_value;

	// ALPS timer without FIFO control
	if (alps_data->als_info.enable && !alps_data->als_info.is_dri)
	{
		reg_value = STK3A8X_REG_READ(alps_data, STK3A8X_REG_FLAG);

		if (reg_value < 0)
		{
			err_flicker("i2c failed\n");
			return;
		}

		flag_value = (uint8_t)reg_value;

		if (flag_value & STK3A8X_FLG_ALSDR_MASK)
		{
			stk3a8x_als_get_data(alps_data);

			alps_data->als_info.is_data_ready = true;
		}
		else
		{
			err_flicker("ALS data was not ready\n");
		}
	}

#endif
#ifdef STK_SW_AGC

	if (alps_data->als_info.is_data_ready)
	{
#ifdef STK_FIFO_ENABLE
		stk3a8x_als_agc_check(alps_data);
#else
		stk3a8x_als_gain_control_L(alps_data);
#endif
		temp_data[0] = alps_data->als_info.last_raw_data[0] * (1 << alps_data->als_info.als_agc_ratio);
		temp_data[1] = alps_data->als_info.last_raw_data[1] * (1 << alps_data->als_info.als_agc_ratio);
	}
	else
	{
		temp_data[0] = alps_data->als_info.last_raw_data[0];
		temp_data[1] = alps_data->als_info.last_raw_data[1];
	}

	if (temp_data[0] > 0xFFFF)
	{
		temp_data[0] = 0xFFFF;
	}

	if (temp_data[1] > 0xFFFF)
	{
		temp_data[1] = 0xFFFF;
	}

	if (alps_data->als_info.is_data_ready)
	{
		alps_data->als_info.als_agc_ratio = alps_data->als_info.als_agc_ratio_updated;
	}

#else
	temp_data[0] = alps_data->als_info.last_raw_data[0];
	temp_data[1] = alps_data->als_info.last_raw_data[1];
#endif
	alps_data->als_info.last_raw_data[0] = temp_data[0];
	alps_data->als_info.last_raw_data[1] = temp_data[1];
	// Last information
	// data[0] is C channel average
	// data[1] is F channel average
	// data[2] is Flicker frequency
#if 0 // NOT USE LUX REPORT YET
	printk(KERN_ERR "%s : C channel = %d, F channel = %d, Frequency = %d\n",
			__func__,
			(uint16_t)temp_data[0],
			(uint16_t)temp_data[1],
			alps_data->als_info.last_raw_data[2]);
	als_lux = ((uint16_t)temp_data[0]) * alps_data->als_info.scale / 1000;
	printk(KERN_ERR "%s: report data = %u \n", als_lux);
	//stk_als_report(alps_data, als_lux);
#endif

#ifdef STK_FFT_FLICKER
	if (alps_data->fifo_info.enable && alps_data->als_info.is_data_ready)
	{
		alps_data->fifo_info.fifo_reading = true;
		SEC_fft_entry(alps_data);
		alps_data->fifo_info.fifo_reading = false;
	}
#endif
	stk_sec_report(alps_data);

	alps_data->als_info.is_data_saturated = false;
	alps_data->als_info.is_gain_changing = false;
	alps_data->als_info.is_data_ready = false;
}

#ifdef STK_FIFO_ENABLE
#if defined (DISABLE_ASYNC_STOP_OPERATION)
void stk3a8x_fifo_stop_control(struct stk3a8x_data *alps_data)
{
	//struct stk3a8x_data *alps_data = container_of(work, struct stk3a8x_data, stk_fifo_release_work);

#if defined(CONFIG_AMS_ALS_COMPENSATION_FOR_AUTO_BRIGHTNESS)
	if (alps_data->stk_alps_wq != NULL)
#endif
		flush_workqueue(alps_data->stk_alps_wq);	//stop polling thread before power off

	while (alps_data->fifo_info.fifo_reading)
	{
		info_flicker("FIFO is reading!\n");
		msleep_interruptible(20);	// original delay time of fifo_release_timer
	}

	info_flicker("Free FIFO array\n");
	stk3a8x_free_fifo_data(alps_data);
	alps_data->fifo_info.latency_status = STK3A8X_NONE;
	stk_power_ctrl(alps_data, 0);
}
#else
void stk3a8x_fifo_stop_control(struct work_struct *work)
{
	struct stk3a8x_data *alps_data = container_of(work, struct stk3a8x_data, stk_fifo_release_work);

	if (alps_data->fifo_info.fifo_reading)
	{
		info_flicker("FIFO is reading!\n");
		return;
	}
	else
	{
		info_flicker("Free FIFO array\n");
		stk3a8x_free_fifo_data(alps_data);
		alps_data->fifo_info.latency_status = STK3A8X_NONE;
		stk3a8x_stop_timer(alps_data, STK3A8X_FIFO_RELEASE_TIMER);
		stk_power_ctrl(alps_data, 0);
	}
}
#endif
#endif

int32_t stk3a8x_clr_flag(struct stk3a8x_data *alps_data, uint8_t org_flag_reg, uint8_t clr)
{
	uint8_t w_flag;
	int32_t ret;
	w_flag = org_flag_reg | STK3A8X_FLG_ALSINT_MASK;
	w_flag &= (~clr);
	ret = STK3A8X_REG_WRITE(alps_data, STK3A8X_REG_FLAG, w_flag);

	if (ret < 0)
		err_flicker("fail, ret=%d\n", ret);

	return ret;
}

static void stk3a8x_get_data_dri(struct work_struct *work)
{
	struct stk3a8x_data *alps_data = container_of(work, struct stk3a8x_data, stk_irq_work);
	uint8_t valid_int_flag = STK3A8X_FLG_ALSINT_MASK;
	int32_t err;
	uint8_t flag_reg;
	info_flicker("Enter\n");
#if (defined(STK_FIFO_ENABLE) && defined(STK_FIFO_DRI))

	if (alps_data->fifo_info.enable)
	{
		stk3a8x_get_fifo_data_dri(alps_data);
		stk3a8x_als_report_fifo_data(alps_data);
	}

#endif
	err = STK3A8X_REG_READ(alps_data, STK3A8X_REG_FLAG);

	if (err < 0)
	{
		err_flicker("Read flag failed\n");
		goto err_i2c_rw;
	}

	flag_reg = (uint8_t)err;
	err = stk3a8x_clr_flag(alps_data, flag_reg, (flag_reg & valid_int_flag));

	if (err < 0)
		goto err_i2c_rw;

	if (alps_data->als_info.is_dri)
	{
		if (flag_reg & STK3A8X_FLG_ALSINT_MASK)
		{
			stk3a8x_als_get_data(alps_data);
		}
	}

	usleep_range(1000, 2000);
	enable_irq(alps_data->irq);
	return;
err_i2c_rw:
	msleep(30);
	enable_irq(alps_data->irq);
	return;
}

static ssize_t stk_name_show(struct device *dev, struct device_attribute *attr, char *buf)
{
	return snprintf(buf, PAGE_SIZE, "%s\n", DEVICE_NAME);
}

static ssize_t stk_flush_store(struct device *dev, struct device_attribute *attr, const char *buf, size_t size)
{
    	struct stk3a8x_data *data = dev_get_drvdata(dev);
	int ret = 0;
	u8 handle = 0;

	ret = kstrtou8(buf, 10, &handle);
	if (ret < 0) {
		err_flicker("kstrtou8 failed.(%d)\n", ret);
		return ret;
	}
	input_report_rel(data->als_input_dev, REL_MISC, handle);
	info_flicker("flush done");

	return size;
}

static ssize_t stk_nothing_show(struct device *dev, struct device_attribute *attr, char *buf)
{
	return 0;
}

static ssize_t stk_nothing_store(struct device *dev, struct device_attribute *attr, const char *buf, size_t size)
{
	return size;
}

static ssize_t stk_factory_cmd_show(struct device *dev, struct device_attribute *attr, char *buf)
{
	struct stk3a8x_data *data = dev_get_drvdata(dev);
	static int cmd_result;

	mutex_lock(&data->data_info_lock);

	if (data->isTrimmed)
		cmd_result = 1;
	else
		cmd_result = 0;

	info_flicker("%s - cmd_result = %d\n", __func__, cmd_result);

	mutex_unlock(&data->data_info_lock);

	return snprintf(buf, PAGE_SIZE, "%d\n", cmd_result);
}

static ssize_t als_ir_show(struct device *dev, struct device_attribute *attr, char *buf)
{
	struct stk3a8x_data *data = dev_get_drvdata(dev);
	int waiting_cnt = 0;

	while(!data->als_info.is_raw_update && waiting_cnt < 100)
	{
		waiting_cnt++;
		msleep_interruptible(10);
	}

	info_flicker("read als_ir = %d (is_raw_update = %d, waiting_cnt = %d)\n", data->als_info.raw_data.ir, data->als_info.is_raw_update, waiting_cnt);
	return snprintf(buf, PAGE_SIZE, "%d\n", data->als_info.raw_data.ir);
}

static ssize_t als_clear_show(struct device *dev, struct device_attribute *attr, char *buf)
{
	struct stk3a8x_data *data = dev_get_drvdata(dev);
	int waiting_cnt = 0;

	while(!data->als_info.is_raw_update && waiting_cnt < 100)
	{
		waiting_cnt++;
		msleep_interruptible(10);
	}

	info_flicker("read als_clear = %d (is_raw_update = %d, waiting_cnt = %d)\n", data->als_info.raw_data.clear, data->als_info.is_raw_update, waiting_cnt);
	return snprintf(buf, PAGE_SIZE, "%d\n", data->als_info.raw_data.clear);
}

static ssize_t als_wideband_show(struct device *dev, struct device_attribute *attr, char *buf)
{
	struct stk3a8x_data *data = dev_get_drvdata(dev);
	int waiting_cnt = 0;

	while(!data->als_info.is_raw_update && waiting_cnt < 100)
	{
		waiting_cnt++;
		msleep_interruptible(10);
	}

	info_flicker("read als_wideband = %d (is_raw_update = %d, waiting_cnt = %d)\n", data->als_info.raw_data.wideband, data->als_info.is_raw_update, waiting_cnt);
	return snprintf(buf, PAGE_SIZE, "%d\n", data->als_info.raw_data.wideband);
}

static ssize_t flicker_data_show(struct device *dev, struct device_attribute *attr, char *buf)
{
	struct stk3a8x_data *data = dev_get_drvdata(dev);
	int waiting_cnt = 0;

	while(!data->als_info.is_raw_update && waiting_cnt < 100)
	{
		waiting_cnt++;
		msleep_interruptible(10);
	}

	info_flicker("read flicker_data = %d (is_raw_update = %d, waiting_cnt = %d)\n", data->als_info.raw_data.flicker, data->als_info.is_raw_update, waiting_cnt);
	return snprintf(buf, PAGE_SIZE, "%d\n", data->als_info.raw_data.flicker);
}

#if IS_ENABLED(CONFIG_SENSORS_FLICKER_SELF_TEST)
#define FREQ_SPEC_MARGIN    10
#define FREQ100_SPEC_IN(X)    (((X > (100 - FREQ_SPEC_MARGIN)) && (X < (100 + FREQ_SPEC_MARGIN)))?"PASS":"FAIL")
#define FREQ120_SPEC_IN(X)    (((X > (120 - FREQ_SPEC_MARGIN)) && (X < (120 + FREQ_SPEC_MARGIN)))?"PASS":"FAIL")

#define WIDE_SPEC_IN(X)		((X >= 0 && X <= 200000)?"PASS":"FAIL")
#define CLEAR_SPEC_IN(X)	((X >= 0 && X <= 200000)?"PASS":"FAIL")
#define ICRATIO_SPEC_IN(X)	"PASS"

static struct result_data *eol_result = NULL;
static ssize_t stk_eol_test_show(struct device *dev, struct device_attribute *attr, char *buf)
{
	struct stk3a8x_data *alps_data =  dev_get_drvdata(dev);

	if (alps_data->eol_enabled) {
		snprintf(buf, MAX_TEST_RESULT, "EOL_RUNNING");
	} else if (eol_result == NULL) {
		snprintf(buf, MAX_TEST_RESULT, "NO_EOL_TEST");
	} else {
		snprintf(buf, MAX_TEST_RESULT, "%d, %s, %d, %s, %d, %s, %d, %s, %d, %s, %d, %s, %d, %s, %d, %s\n",
				eol_result->flicker[EOL_STATE_100], FREQ100_SPEC_IN(eol_result->flicker[EOL_STATE_100]),
				eol_result->flicker[EOL_STATE_120], FREQ120_SPEC_IN(eol_result->flicker[EOL_STATE_120]),
				eol_result->wideband[EOL_STATE_100], WIDE_SPEC_IN(eol_result->wideband[EOL_STATE_100]),
				eol_result->wideband[EOL_STATE_120], WIDE_SPEC_IN(eol_result->wideband[EOL_STATE_120]),
				eol_result->clear[EOL_STATE_100], CLEAR_SPEC_IN(eol_result->clear[EOL_STATE_100]),
				eol_result->clear[EOL_STATE_120], CLEAR_SPEC_IN(eol_result->clear[EOL_STATE_120]),
				eol_result->ratio[EOL_STATE_100], ICRATIO_SPEC_IN(eol_result->ratio[EOL_STATE_100]),
				eol_result->ratio[EOL_STATE_120], ICRATIO_SPEC_IN(eol_result->ratio[EOL_STATE_120]));

		eol_result = NULL;
	}
	info_flicker("%s", buf);

	return MAX_TEST_RESULT;
}

static ssize_t stk_eol_test_store(struct device *dev, struct device_attribute *attr, const char *buf, size_t size)
{
	struct stk3a8x_data *alps_data =  dev_get_drvdata(dev);
#if defined(CONFIG_AMS_ALS_COMPENSATION_FOR_AUTO_BRIGHTNESS)
	bool flicker_flag = alps_data->flicker_flag;

	if (!flicker_flag) {
		alps_data->flicker_flag = true;

		if (alps_data->als_flag) {
			stk3a8x_enable_als(alps_data, false);
			stk3a8x_enable_fifo(alps_data, false);
		}
	}
#endif
	//Start
	alps_data->recover_state = alps_data->als_info.enable;

	if (!alps_data->recover_state)
		stk3a8x_alps_set_config(alps_data, 1);

	alps_data->eol_enabled = true;

	als_eol_set_env(true, 80);
	eol_result = als_eol_mode();

	alps_data->eol_enabled = false;

	//Stop
	stk3a8x_alps_set_config(alps_data, alps_data->recover_state);
#if defined(CONFIG_AMS_ALS_COMPENSATION_FOR_AUTO_BRIGHTNESS)
	if (!flicker_flag) {
		alps_data->flicker_flag = false;

		if (alps_data->als_flag) {
			stk3a8x_alps_set_config(alps_data, true);
			stk_als_init(alps_data);
		}
	}
#endif
	return size;
}

#endif

/****************************************************************************************************
 * ALS ATTR List
 ****************************************************************************************************/
static DEVICE_ATTR(enable,          0664, stk_als_enable_show,          stk_als_enable_store);
static DEVICE_ATTR(lux,             0664, stk_als_lux_show,             stk_als_lux_store);
static DEVICE_ATTR(code,            0444, stk_als_code_show,            NULL);
static DEVICE_ATTR(transmittance,   0664, stk_als_transmittance_show,   stk_als_transmittance_store);
static DEVICE_ATTR(updateregistry,  0644, stk3a8x_als_registry_show,    stk3a8x_registry_store);
static DEVICE_ATTR(recv,            0664, stk_recv_show,                stk_recv_store);
static DEVICE_ATTR(send,            0664, stk_send_show,                stk_send_store);
static DEVICE_ATTR(poll_delay,		0664, stk_nothing_show,				stk_nothing_store);
#ifdef STK_ALS_FIR
static DEVICE_ATTR(firlen, 0644, stk_als_firlen_show, stk_als_firlen_store);
#endif
#ifdef STK_ALS_CALI
static DEVICE_ATTR(cali, 0644, stk3a8x_als_cali_show, stk3a8x_cali_store);
#endif
#ifdef STK_CHECK_LIB
static DEVICE_ATTR(otp, 0444, stk3a8x_otp_info_show, NULL);
#endif

static DEVICE_ATTR(name,            0444, stk_name_show,                NULL);
static DEVICE_ATTR(als_flush,       0664, stk_nothing_show,             stk_flush_store);
static DEVICE_ATTR(als_factory_cmd, 0444, stk_factory_cmd_show,         NULL);
static DEVICE_ATTR(als_ir,          0444, als_ir_show,                  NULL);
static DEVICE_ATTR(als_clear,       0444, als_clear_show,               NULL);
static DEVICE_ATTR(als_wideband,    0444, als_wideband_show,            NULL);
static DEVICE_ATTR(flicker_data,    0444, flicker_data_show,            NULL);
#if IS_ENABLED(CONFIG_SENSORS_FLICKER_SELF_TEST)
static DEVICE_ATTR(eol_mode,        0664, stk_eol_test_show,            stk_eol_test_store);
#endif
#if defined(CONFIG_AMS_ALS_COMPENSATION_FOR_AUTO_BRIGHTNESS)
static DEVICE_ATTR(als_enable, 0664, stk_rear_als_enable_show, stk_rear_als_enable_store);
static DEVICE_ATTR(als_data, 0444, stk_rear_als_data_show, NULL);
#endif

static struct attribute *stk_als_attrs [] =
{
	&dev_attr_enable.attr,
	&dev_attr_lux.attr,
	&dev_attr_code.attr,
	&dev_attr_transmittance.attr,
	&dev_attr_updateregistry.attr,
	&dev_attr_recv.attr,
	&dev_attr_send.attr,
#ifdef STK_ALS_FIR
	&dev_attr_firlen.attr,
#endif
#ifdef STK_ALS_CALI
	&dev_attr_cali.attr,
#endif
#ifdef STK_CHECK_LIB
	&dev_attr_otp.attr,
#endif
	&dev_attr_poll_delay.attr,
	NULL
};
static struct attribute_group stk_als_attribute_group =
{
	//.name = "driver",
	.attrs = stk_als_attrs,
};

static struct device_attribute *stk_sensor_attrs[] = {
	&dev_attr_name,
	&dev_attr_als_flush,
	&dev_attr_als_factory_cmd,
	&dev_attr_als_ir,
	&dev_attr_als_clear,
	&dev_attr_als_wideband,
	&dev_attr_flicker_data,
#if IS_ENABLED(CONFIG_SENSORS_FLICKER_SELF_TEST)
	&dev_attr_eol_mode,
#endif
#if defined(CONFIG_AMS_ALS_COMPENSATION_FOR_AUTO_BRIGHTNESS)
	&dev_attr_als_enable,
	&dev_attr_als_data,
#endif
	NULL
};

#ifdef SUPPORT_SENSOR_CLASS
/****************************************************************************************************
 * Sensor class API
 ****************************************************************************************************/
static int stk3a8x_cdev_enable_als(struct sensors_classdev *sensors_cdev,
		unsigned int enable)
{
	struct stk3a8x_data *alps_data = container_of(sensors_cdev,
			struct stk3a8x_data, als_cdev);
	stk3a8x_alps_set_config(alps_data, enable);
	return 0;
}

static int stk3a8x_cdev_set_als_calibration(struct sensors_classdev *sensors_cdev, int axis, int apply_now)
{
	struct stk3a8x_data *alps_data = container_of(sensors_cdev,
			struct stk3a8x_data, als_cdev);
	stk3a8x_cali_als(alps_data);
	return 0;
}

static struct sensors_classdev als_cdev =
{
	.name = "stk3a8x-light",
	.vendor = "sensortek",
	.version = 1,
	.handle = SENSORS_LIGHT_HANDLE,
	.type = SENSOR_TYPE_LIGHT,
	.max_range = "65536",
	.resolution = "1.0",
	.sensor_power = "0.25",
	.min_delay = 50000,
	.max_delay = 2000,
	.fifo_reserved_event_count = 0,
	.fifo_max_event_count = 0,
	.flags = 2,
	.enabled = 0,
	.delay_msec = 50,
	.sensors_enable = NULL,
	.sensors_calibrate = NULL,
};

#endif

/****************************************************************************************************
 * System Init. API
 ****************************************************************************************************/
static void stk3a8x_init_para(struct stk3a8x_data *alps_data,
		struct stk3a8x_platform_data *plat_data)
{
	memset(&alps_data->als_info, 0, sizeof(alps_data->als_info));
	alps_data->als_info.first_init  = true;
	alps_data->als_info.is_dri      = plat_data->als_is_dri;
#ifdef STK_FIFO_ENABLE
	alps_data->fifo_info.is_dri      = true;
#endif
#ifdef STK_FFT_FLICKER
	alps_data->fifo_info.last_flicker_freq = 0;
#endif
#ifdef STK_ALS_CALI

	if (alps_data->cali_info.cali_para.als_version != 0)
	{
		alps_data->als_info.scale = alps_data->cali_info.cali_para.als_scale;
	}
	else
#endif
	{
		alps_data->als_info.scale = plat_data->als_scale;
	}

	alps_data->int_pin                  = plat_data->int_pin;
	alps_data->pdata                    = plat_data;
#if (defined(STK_FIFO_ENABLE) && defined(STK_FIFO_POLLING))
	alps_data->fifo_info.latency_status = 0;
	alps_data->fifo_info.update_latency = false;
#endif
	alps_data->als_info.is_data_ready = false;
	alps_data->als_info.is_raw_update = false;
#ifdef STK_SW_AGC
	alps_data->als_info.als_agc_ratio         = 0;
	alps_data->als_info.als_agc_ratio_updated = 0;
	alps_data->als_info.als_gain_level        = 0;
	alps_data->als_info.last_als_gain_level   = 0;
	alps_data->als_info.is_gain_changing      = false;
	alps_data->als_info.is_data_saturated     = false;
#endif
#if IS_ENABLED(CONFIG_SENSORS_FLICKER_SELF_TEST)
	alps_data->eol_enabled = false;
#endif
	alps_data->saturation = false;
	alps_data->stk_alps_wq = NULL;
	alps_data->stk_fifo_release_wq = NULL;
}

static int32_t stk3a8x_init_all_reg(struct stk3a8x_data *alps_data)
{
	int32_t ret = 0;
	uint16_t reg_count = 0, reg_num = sizeof(stk3a8x_default_register_table) / sizeof(stk3a8x_register_table);

	for (reg_count = 0; reg_count < reg_num; reg_count++)
	{
		ret = STK3A8X_REG_READ_MODIFY_WRITE(alps_data,
				stk3a8x_default_register_table[reg_count].address,
				stk3a8x_default_register_table[reg_count].value,
				stk3a8x_default_register_table[reg_count].mask_bit);

		if (ret < 0)
		{
			err_flicker("write i2c error\n");
			return ret;
		}
	}

	return ret;
}

static int32_t stk3a8x_init_all_setting(struct i2c_client *client, struct stk3a8x_platform_data *plat_data)
{
	int32_t ret;
	struct stk3a8x_data *alps_data = i2c_get_clientdata(client);
	ret = stk3a8x_software_reset(alps_data);

	if (ret < 0)
		return ret;

	ret = stk3a8x_check_pid(alps_data);

	if (ret < 0)
		return ret;

	ret = stk3a8x_init_all_reg(alps_data);

	if (ret < 0)
		return ret;

#ifdef STK_FIFO_ENABLE
	stk3a8x_fifo_init(alps_data);
#endif
	alps_data->first_init = true;
#if defined(CONFIG_AMS_ALS_COMPENSATION_FOR_AUTO_BRIGHTNESS)
	alps_data->als_flag = false;
	alps_data->flicker_flag = false;
#endif
	stk3a8x_dump_reg(alps_data);
	return 0;
}

static int32_t stk3a8x_alps_set_config(struct stk3a8x_data *alps_data, bool en)
{
	bool is_irq = false;
	int32_t ret = 0;

	mutex_lock(&alps_data->config_lock);
	if (alps_data->first_init)
	{
		ret = stk3a8x_request_registry(alps_data);

		if (ret < 0)
			goto err;

		stk3a8x_init_para(alps_data, alps_data->pdata);
		alps_data->first_init = false;
	}

	if (alps_data->als_info.enable == en)
	{
		err_flicker("ALS status is same (%d)\n", en);
		goto done;
	}

	//To-Do: ODR setting
	if (en && alps_data->als_info.is_dri)
	{
		is_irq = true;
	}

	if (en == 1) {
		alps_data->als_info.is_raw_update = false;
		stk_power_ctrl(alps_data, 1);
		stk3a8x_init_all_reg(alps_data);
	}

	stk3a8x_pin_control(alps_data, en);

#ifdef STK_SW_AGC
	stk3a8x_get_curGain(alps_data);
	alps_data->als_info.als_agc_ratio = alps_data->als_info.als_agc_ratio_updated;
#ifdef STK_FIFO_ENABLE
	info_flicker("Set default Clear channel gain\n");
	stk3a8x_als_agc_adjust_gain(alps_data, true, alps_data->als_info.als_cur_again, true, alps_data->als_info.als_cur_dgain);
#endif
#endif

	if (alps_data->als_info.is_dri && is_irq)
	{
		info_flicker("ALS DRI SETTING\n");
		stk3a8x_set_als_thd(alps_data, 0x0, 0xFFFF);
		ret = STK3A8X_REG_READ_MODIFY_WRITE(alps_data,
				STK3A8X_REG_INTCTRL1,
				STK3A8X_INT_ALS_MASK,
				STK3A8X_INT_ALS_MASK);

		if (ret < 0)
		{
			err_flicker("ALS set INT fail\n");
			goto err;
		}

		stk3a8x_register_interrupt(alps_data);
	}
	else
	{
		if (en)
		{
			info_flicker("ALPS Timer SETTING\n");

			if (!alps_data->alps_timer_info.timer_is_exist)
			{
				stk3a8x_als_latency(alps_data);
				stk3a8x_register_timer(alps_data, STK3A8X_DATA_TIMER_ALPS);
			}

#if defined(CONFIG_AMS_ALS_COMPENSATION_FOR_AUTO_BRIGHTNESS)
			if (en && alps_data->flicker_flag)
#endif
			{
				if (!alps_data->alps_timer_info.timer_is_active) {
					stk3a8x_start_timer(alps_data, STK3A8X_DATA_TIMER_ALPS);
				}
			}
		}
	}

	stk3a8x_enable_als(alps_data, en);

	if (!en &&  !alps_data->als_info.enable)
	{
#if defined(CONFIG_AMS_ALS_COMPENSATION_FOR_AUTO_BRIGHTNESS)
		if (alps_data->stk_alps_wq != NULL)
#endif
			flush_workqueue(alps_data->stk_alps_wq);
		info_flicker("(%d) Stop ALSPS timer\n", __LINE__);
		stk3a8x_stop_timer(alps_data, STK3A8X_DATA_TIMER_ALPS);
	}

#ifdef STK_FIFO_ENABLE

	if ((alps_data->als_info.enable == true) &&
			((alps_data->fifo_info.data_type == STK3A8X_FIFO_SEL_DATA_FLICKER) ||
			 (alps_data->fifo_info.data_type == STK3A8X_FIFO_SEL_DATA_IR) ||
			 (alps_data->fifo_info.data_type == STK3A8X_FIFO_SEL_DATA_FLICKER_IR)))
	{
		info_flicker("(%d) FIFO enable\n", __LINE__);
		stk3a8x_enable_fifo(alps_data, true);
		stk3a8x_alloc_fifo_data(alps_data, STK_FIFO_GET_MAX_FRAME);
	}
	else
	{
		info_flicker("FIFO disable\n");
		stk3a8x_enable_fifo(alps_data, false);
	}

	/* Create a timer for release FIFO array */
	if (alps_data->fifo_info.enable == false)
	{
		if (!alps_data->fifo_release_timer_info.timer_is_exist)
		{
			info_flicker("register FIFO array release timer\n");
			stk3a8x_register_timer(alps_data, STK3A8X_FIFO_RELEASE_TIMER);
		}

		if (!alps_data->fifo_release_timer_info.timer_is_active)
		{
			info_flicker("start FIFO array release timer\n");
			stk3a8x_start_timer(alps_data, STK3A8X_FIFO_RELEASE_TIMER);
		}

		alps_data->fifo_info.latency_status = STK3A8X_NONE;
	}

#endif
	//stk3a8x_dump_reg(alps_data);
err:
	err_flicker("error %d", ret);
done:
	mutex_unlock(&alps_data->config_lock);
	return ret;
}

static void stk3a8x_pin_control(struct stk3a8x_data *alps_data, bool pin_set)
{
	struct device *dev = alps_data->dev;
	int status = 0;

	if (!alps_data->als_pinctrl) {
		err_flicker("als_pinctrl is null");
		return;
	}

	if (pin_set) {
		if (!IS_ERR_OR_NULL(alps_data->pins_active)) {
			status = pinctrl_select_state(alps_data->als_pinctrl, alps_data->pins_active);
			if (status)
				err_flicker("can't set pin active state");
			else
				info_flicker("set active state");
		}
	} else {
		if (!IS_ERR_OR_NULL(alps_data->pins_sleep)) {
			status = pinctrl_select_state(alps_data->als_pinctrl, alps_data->pins_sleep);
			if (status)
				err_flicker("can't set pin sleep state");
			else
				info_flicker("set sleep state");
		}
	}
}

int stk3a8x_suspend(struct device *dev)
{
	struct stk3a8x_data *alps_data = dev_get_drvdata(dev);
	info_flicker();
	//mutex_lock(&alps_data->io_lock);

#if defined(CONFIG_AMS_ALS_COMPENSATION_FOR_AUTO_BRIGHTNESS)
	if (alps_data->als_flag) {
		alps_data->als_flag = false;
		stk_als_stop(alps_data);
	}
#endif
	if (alps_data->als_info.enable)
	{
		info_flicker("Disable ALS : 0\n");
		stk3a8x_alps_set_config(alps_data, 0);
	}

	stk3a8x_pin_control(alps_data, false);

	//mutex_unlock(&alps_data->io_lock);
	return 0;
}

int stk3a8x_resume(struct device *dev)
{
	struct stk3a8x_data *alps_data = dev_get_drvdata(dev);
	info_flicker();
	//mutex_lock(&alps_data->io_lock);

	if (alps_data->als_info.enable)
	{
		info_flicker("Enable ALS : 1\n");
		stk3a8x_alps_set_config(alps_data, 1);
	}

	stk3a8x_pin_control(alps_data, true);

	//mutex_unlock(&alps_data->io_lock);
	return 0;
}

static int stk3a8x_parse_dt(struct stk3a8x_data *alps_data,
		struct stk3a8x_platform_data *pdata)
{
	int rc;
	struct device *dev = alps_data->dev;
	struct device_node *np = dev->of_node;
	u32 temp_val;

	info_flicker("parse dt");
	pdata->int_pin = of_get_named_gpio_flags(np, "stk,irq-gpio",
			0, &pdata->int_flags);

	if (pdata->int_pin < 0)
	{
		err_flicker("Unable to read irq-gpio");
		return pdata->int_pin;
	}

	alps_data->als_pinctrl = devm_pinctrl_get(dev);

	if (IS_ERR_OR_NULL(alps_data->als_pinctrl)) {
		err_flicker("get pinctrl(%li) error", PTR_ERR(alps_data->als_pinctrl));
		alps_data->als_pinctrl = NULL;
	} else {
		alps_data->pins_sleep = pinctrl_lookup_state(alps_data->als_pinctrl, "sleep");
		if (IS_ERR_OR_NULL(alps_data->pins_sleep)) {
			err_flicker("get pins_sleep(%li) error", PTR_ERR(alps_data->pins_sleep));
			alps_data->pins_sleep = NULL;
		}

		alps_data->pins_active = pinctrl_lookup_state(alps_data->als_pinctrl, "active");
		if (IS_ERR_OR_NULL(alps_data->pins_active)) {
			err_flicker("get pins_active(%li) error", PTR_ERR(alps_data->pins_active));
			alps_data->pins_active = NULL;
		}
	}

	rc = of_property_read_u32(np, "stk,als_scale", &temp_val);

	if (!rc)
	{
		pdata->als_scale = temp_val;
		info_flicker("stk-als_scale = %d\n", temp_val);
	}
	else
	{
		dev_err(dev, "Unable to read als_scale\n");
		return rc;
	}

	pdata->als_is_dri = of_property_read_bool(np, "stk,als_is_dri");
	return 0;
}

static int stk3a8x_set_input_devices(struct stk3a8x_data *alps_data)
{
	int err;
	/****** Create ALS ATTR ******/
	alps_data->als_input_dev = input_allocate_device();

	if (!alps_data->als_input_dev)
	{
		info_flicker("could not allocate als device\n");
		err = -ENOMEM;
		return err;
	}

	alps_data->als_input_dev->name = ALS_NAME;
	alps_data->als_input_dev->id.bustype = BUS_I2C;
	input_set_drvdata(alps_data->als_input_dev, alps_data);
	//set_bit(EV_ABS, alps_data->als_input_dev->evbit);
	//input_set_abs_params(alps_data->als_input_dev, ABS_MISC, 0, ((1 << 16) - 1), 0, 0);
	input_set_capability(alps_data->als_input_dev, EV_REL, REL_X);
	input_set_capability(alps_data->als_input_dev, EV_REL, REL_RY);
	input_set_capability(alps_data->als_input_dev, EV_REL, REL_RZ);
	input_set_capability(alps_data->als_input_dev, EV_REL, REL_MISC);
	err = input_register_device(alps_data->als_input_dev);

	if (err)
	{
		info_flicker("can not register als input device\n");
		goto err_als_input_register;
	}

	err = sysfs_create_group(&alps_data->als_input_dev->dev.kobj, &stk_als_attribute_group);

	if (err < 0)
	{
		err_flicker("could not create sysfs group for als\n");
		goto err_als_create_group;
	}

	input_set_drvdata(alps_data->als_input_dev, alps_data);
#ifdef SUPPORT_SENSOR_CLASS
	alps_data->als_cdev = als_cdev;
	alps_data->als_cdev.sensors_enable = stk3a8x_cdev_enable_als;
	alps_data->als_cdev.sensors_calibrate = stk3a8x_cdev_set_als_calibration;
	err = sensors_classdev_register(&alps_data->als_input_dev->dev, &alps_data->als_cdev);

	if (err)
	{
		err_flicker("ALS sensors class register failed.\n");
		goto err_register_als_cdev;
	}

#endif
	sensors_register(&alps_data->sensor_dev, alps_data, stk_sensor_attrs, ALS_NAME);
	info_flicker("done sensors_register");
	return 0;
#ifdef SUPPORT_SENSOR_CLASS
	sensors_classdev_unregister(&alps_data->als_cdev);
err_register_als_cdev:
#endif
	sysfs_remove_group(&alps_data->als_input_dev->dev.kobj, &stk_als_attribute_group);
err_als_create_group:
	input_unregister_device(alps_data->als_input_dev);
err_als_input_register:
	input_free_device(alps_data->als_input_dev);
	return err;
}

int stk3a8x_probe(struct i2c_client *client,
		const struct stk3a8x_bus_ops *bops)
{
	int err = -ENODEV;
	struct stk3a8x_data *alps_data;
	struct stk3a8x_platform_data *plat_data;
	struct regulator *vdd_regulator = NULL, *vbus_regulator = NULL;
	bool vdd_1p8_enable = false;
	bool vbus_1p8_enable = false;

	vdd_regulator = regulator_get(&client->dev, "vdd_1p8");
	vbus_regulator = regulator_get(&client->dev, "vbus_1p8");
	if (!IS_ERR(vdd_regulator) && vdd_regulator != NULL) {
		err = regulator_enable(vdd_regulator);
		if (err < 0) {
			dev_err(&client->dev, "Regulator vdd enable failed");
			goto err_regulator_enable;
		}

		vdd_1p8_enable = true;
		info_flicker("check regulator vdd = %d",	regulator_is_enabled(vdd_regulator));
		msleep_interruptible(40);
	} else {
		vdd_regulator = NULL;
	}

	if (!IS_ERR(vbus_regulator) && vbus_regulator != NULL) {
		err = regulator_enable(vbus_regulator);
		if (err < 0) {
			dev_err(&client->dev, "Regulator vbus enable failed");
			goto err_regulator_enable;
		}

		vbus_1p8_enable = true;
		info_flicker("check regulator vbus = %d", regulator_is_enabled(vbus_regulator));
	} else {
		vbus_regulator = NULL;
	}

	info_flicker("driver version = %s\n", DRIVER_VERSION);
	err = i2c_check_functionality(client->adapter, I2C_FUNC_I2C);

	if (!err)
	{
		dev_err(&client->dev, "SMBUS Byte Data not Supported\n");
		return -EIO;
	}

	alps_data = kzalloc(sizeof(struct stk3a8x_data), GFP_KERNEL);

	if (!alps_data)
	{
		err_flicker("failed to allocate stk3a8x_data\n");
		return -ENOMEM;
	}

	alps_data->client = client;
	alps_data->dev    = &client->dev;
	alps_data->bops   = bops;
	i2c_set_clientdata(client, alps_data);
	mutex_init(&alps_data->config_lock);
	mutex_init(&alps_data->io_lock);
	mutex_init(&alps_data->data_info_lock);
#if defined(CONFIG_AMS_ALS_COMPENSATION_FOR_AUTO_BRIGHTNESS)
	mutex_init(&alps_data->enable_lock);
#endif
	// Parsing device tree
	if (client->dev.of_node)
	{
		info_flicker("probe with device tree\n");
		plat_data = devm_kzalloc(&client->dev,
				sizeof(struct stk3a8x_platform_data), GFP_KERNEL);

		if (!plat_data)
		{
			dev_err(&client->dev, "Failed to allocate memory\n");
			return -ENOMEM;
		}

		err = stk3a8x_parse_dt(alps_data, plat_data);

		if (err)
		{
			dev_err(&client->dev,
					"%s: stk3a8x_parse_dt ret=%d\n", __func__, err);
			goto err_out;
		}
	}
	else
	{
		err_flicker("probe with platform data\n");
		plat_data = client->dev.platform_data;
	}

	if (!plat_data)
	{
		dev_err(&client->dev,
				"%s: no stk3a8x platform data!\n", __func__);
		goto err_als_input_allocate;
	}

	if (plat_data->als_scale == 0)
	{
		dev_err(&client->dev,
				"%s: Please set als_scale = %d\n", __func__, plat_data->als_scale);
		goto err_als_input_allocate;
	}

	// Register device
	err = stk3a8x_set_input_devices(alps_data);
	if (err < 0)
		goto err_setup_input_device;

	alps_data->pdata  = plat_data;

	alps_data->vdd_regulator = vdd_regulator;
	alps_data->vbus_regulator = vbus_regulator;
	alps_data->vdd_1p8_enable = vdd_1p8_enable;
	alps_data->vbus_1p8_enable = vbus_1p8_enable;

	err = stk3a8x_init_all_setting(client, plat_data);
	if (err < 0) {
		err_flicker("init setting failed %d", err);
		goto err_setup_init_reg;
	}

	info_flicker("probe successfully");

	stk_power_ctrl(alps_data, false);
	return 0;
err_setup_input_device:
err_als_input_allocate:
err_setup_init_reg:
	if (alps_data->als_pinctrl) {
		devm_pinctrl_put(alps_data->als_pinctrl);
		alps_data->als_pinctrl = NULL;
	}
	if (alps_data->pins_active)
		alps_data->pins_active = NULL;
	if (alps_data->pins_sleep)
		alps_data->pins_sleep = NULL;

	sensors_unregister(alps_data->sensor_dev, stk_sensor_attrs);
#ifdef SUPPORT_SENSOR_CLASS
	sensors_classdev_unregister(&alps_data->als_cdev);
#endif
	sysfs_remove_group(&alps_data->als_input_dev->dev.kobj, &stk_als_attribute_group);
	input_unregister_device(alps_data->als_input_dev);

	kfree(alps_data);
err_regulator_enable:
	if (!IS_ERR(vdd_regulator) && vdd_regulator != NULL) {
		if (vdd_1p8_enable)
			regulator_disable(vdd_regulator);
		regulator_put(vdd_regulator);
		vdd_regulator = NULL;
	}

	if (!IS_ERR(vbus_regulator) && vbus_regulator != NULL) {
		if (vbus_1p8_enable)
			regulator_disable(vbus_regulator);
#if IS_ENABLED(CONFIG_ARCH_EXYNOS)
		regulator_put(vbus_regulator);
#endif
		vbus_regulator = NULL;
	}

err_out:
	return err;
}

int stk3a8x_remove(struct i2c_client *client)
{
	struct stk3a8x_data *alps_data = i2c_get_clientdata(client);
	if (alps_data->vdd_regulator != NULL) {
		regulator_put(alps_data->vdd_regulator);
	}
	if (alps_data->vbus_regulator != NULL) {
		regulator_put(alps_data->vbus_regulator);
	}

	if (alps_data->als_pinctrl) {
		devm_pinctrl_put(alps_data->als_pinctrl);
		alps_data->als_pinctrl = NULL;
	}
	if (alps_data->pins_active)
		alps_data->pins_active = NULL;
	if (alps_data->pins_sleep)
		alps_data->pins_sleep = NULL;

	device_init_wakeup(&client->dev, false);
	free_irq(alps_data->irq, alps_data);
	gpio_free(alps_data->int_pin);
	hrtimer_cancel(&alps_data->alps_timer);
	hrtimer_cancel(&alps_data->fifo_release_timer);
	if (alps_data->stk_alps_wq) {
		cancel_work_sync(&alps_data->stk_alps_work);
		destroy_workqueue(alps_data->stk_alps_wq);
	}
	if (alps_data->stk_fifo_release_wq) {
		cancel_work_sync(&alps_data->stk_fifo_release_work);
		destroy_workqueue(alps_data->stk_fifo_release_wq);
	}
	sensors_unregister(alps_data->sensor_dev, stk_sensor_attrs);
	sysfs_remove_group(&alps_data->als_input_dev->dev.kobj, &stk_als_attribute_group);
	input_unregister_device(alps_data->als_input_dev);
#ifdef SUPPORT_SENSOR_CLASS
	input_unregister_device(alps_data->als_input_dev);
#endif
	mutex_destroy(&alps_data->config_lock);
	mutex_destroy(&alps_data->io_lock);
	mutex_destroy(&alps_data->data_info_lock);
#if defined(CONFIG_AMS_ALS_COMPENSATION_FOR_AUTO_BRIGHTNESS)
	mutex_destroy(&alps_data->enable_lock);
#endif
	kfree(alps_data);
	info_flicker("remove successfully");
	return 0;
}
