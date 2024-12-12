/*
 *  Copyright (C) 2010,Imagis Technology Co. Ltd. All Rights Reserved.
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
 */

#include <linux/module.h>
#include <linux/slab.h>
#include <linux/i2c.h>
#include <linux/delay.h>
#include <linux/input.h>
#include <linux/gpio.h>
#include <linux/of_gpio.h>
#include <linux/fs.h>
#include <linux/uaccess.h>
#include <linux/pm_wakeup.h>
#include <linux/interrupt.h>
#include <linux/regulator/consumer.h>
#include <linux/power_supply.h>
#include <linux/vmalloc.h>
#if IS_ENABLED(CONFIG_MUIC_NOTIFIER)
#include <linux/muic/muic.h>
#include <linux/muic/muic_notifier.h>
#endif
#if IS_ENABLED(CONFIG_CCIC_NOTIFIER) || IS_ENABLED(CONFIG_PDIC_NOTIFIER)
#include <linux/usb/typec/common/pdic_notifier.h>
#endif
#if IS_ENABLED(CONFIG_USB_TYPEC_MANAGER_NOTIFIER)
#include <linux/usb/typec/manager/usb_typec_manager_notifier.h>
#endif

#include "isg5320a_sub_reg.h"

#define CHIP_ID                 0x32
#define DEVICE_NAME             "ISG5320A_SUB"
#define VENDOR_NAME             "IMAGIS"
#define MODULE_NAME             "grip_sensor_sub"

#define ISG5320A_MODE_SLEEP      0
#define ISG5320A_MODE_NORMAL     1
#define ISG5320A_DIFF_AVR_CNT    10
#define ISG5320A_DISPLAY_TIME    30
#define ISG5320A_TAG             "[ISG5320A_SUB]"

#define HALLIC_PATH            "/sys/class/sec/hall_ic/hall_detect"
#define HALLIC_CERT_PATH       "/sys/class/sec/hall_ic/certify_hall_detect"

#define ISG5320A_INIT_DELAYEDWORK
#define GRIP_LOG_TIME            40 /* 20 sec */

#define vfs_read(fp, buf, len, pos) kernel_read(fp, buf, len, pos)

#pragma pack(1)
typedef struct {
	char cmd;
	u8 addr;
	u8 val;
} direct_info;
#pragma pack()

struct isg5320a_data {
	struct i2c_client *client;
	struct input_dev *input_dev;
	struct device *dev;
	struct delayed_work debug_work;
	struct delayed_work cal_work;
#ifdef ISG5320A_INIT_DELAYEDWORK
	struct delayed_work init_work;
#endif
	struct wakeup_source *grip_ws;
	struct mutex lock;
#if IS_ENABLED(CONFIG_MUIC_NOTIFIER)
	struct notifier_block cpuidle_muic_nb;
#endif
#if IS_ENABLED(CONFIG_CCIC_NOTIFIER) || IS_ENABLED(CONFIG_PDIC_NOTIFIER)
	struct notifier_block cpuidle_ccic_nb;
#endif
	int gpio_int;

	int enable;
	int state;

	bool skip_data;
	int initialized;

	u16 normal_th;

	u32 cdc;
	u32 base;
	s32 diff;
	s32 max_diff;
	s32 max_normal_diff;

	int diff_cnt;
	int diff_sum;
	int diff_avg;
	int cdc_sum;
	int cdc_avg;

	u32 debug_cdc[3];
	s32 debug_diff[3];
	u32 debug_base[2];

	int irq_count;
	int abnormal_mode;

	u16 fine_coarse;
	u32 cfcal_th;
	bool bfcal_chk_ready;
	bool bfcal_chk_start;
	u32 bfcal_chk_count;
	u32 bfcal_chk_cdc;
	s32 bfcal_chk_diff;

	int debug_cnt;

	u8 intr_debug_addr;
	int intr_debug_size;
	direct_info direct;

	int hall_flag;
	int hall_cert_flag;
	int hallic_detect;
	int hallic_cert_detect;
	unsigned char hall_ic[6];
};

static int check_hallic_state(char *file_path, unsigned char hall_ic_status[])
{
	int ret = 0;
	mm_segment_t old_fs;
	struct file *filep;
	u8 hall_sysfs[5];

	old_fs = get_fs();
	set_fs(KERNEL_DS);

	filep = filp_open(file_path, O_RDONLY, 0666);
	if (IS_ERR(filep)) {
		ret = PTR_ERR(filep);
		set_fs(old_fs);
		goto exit;
	}

	ret = vfs_read(filep, hall_sysfs, sizeof(hall_sysfs),
		       &filep->f_pos);

	if (ret != sizeof(hall_sysfs))
		ret = -EIO;
	else
		strncpy(hall_ic_status, hall_sysfs, sizeof(hall_sysfs));

	filp_close(filep, current->files);
	set_fs(old_fs);

exit:
	return ret;
}

static int isg5320a_i2c_write(struct isg5320a_data *data, u8 cmd, u8 *val)
{
	int ret;
	u8 buf[3];
	struct i2c_msg msg;

	buf[0] = cmd;
	memcpy(buf + sizeof(cmd), val, 2);

	msg.addr = data->client->addr;
	msg.flags = 0; /*I2C_M_WR*/
	msg.len = 3;
	msg.buf = buf;

	ret = i2c_transfer(data->client->adapter, &msg, 1);
	if (ret < 0)
		pr_err("%s %s: i2c_transfer failed(%d)\n", ISG5320A_TAG, __func__, ret);

	return ret;
}

static int isg5320a_i2c_write_one(struct isg5320a_data *data, u8 cmd, u8 val)
{
	int ret;
	u8 buf[2];
	struct i2c_msg msg;

	buf[0] = cmd;
	buf[1] = val;

	msg.addr = data->client->addr;
	msg.flags = 0; /*I2C_M_WR*/
	msg.len = 2;
	msg.buf = buf;

	ret = i2c_transfer(data->client->adapter, &msg, 1);
	if (ret < 0)
		pr_err("%s %s: i2c_transfer failed(%d)\n", ISG5320A_TAG, __func__, ret);

	return ret;
}

static int isg5320a_i2c_read(struct isg5320a_data *data, u8 cmd, u8 *val,
			     int len)
{
	int ret;
	struct i2c_msg msgs[2] = {
		{
			.addr = data->client->addr,
			.flags = 0,
			.len = sizeof(cmd),
			.buf = &cmd,
		},
		{
			.addr = data->client->addr,
			.flags = I2C_M_RD,
			.len = len,
			.buf = val,
		},
	};

	ret = i2c_transfer(data->client->adapter, msgs, 2);
	if (ret < 0)
		pr_err("%s %s: i2c_transfer failed(%d)\n", ISG5320A_TAG, __func__, ret);

	return ret;
}

static int isg5320a_reset(struct isg5320a_data *data)
{
	int ret = 0;
	int cnt = 0;
	u8 val;

	pr_info("%s %s\n", ISG5320A_TAG, __func__);

	if (data->initialized == OFF)
		usleep_range(5000, 5100);

	ret = isg5320a_i2c_read(data, ISG5320A_IRQSRC_REG, &val, 1);
	if (ret < 0)
		pr_err("%s irq to high failed(%d)\n", ISG5320A_TAG, ret);

	while (gpio_get_value_cansleep(data->gpio_int) == 0 && cnt++ < 10)
		usleep_range(5000, 5100);

	if (cnt >= 10)
		pr_err("%s wait irq to high failed\n", ISG5320A_TAG);

	ret = isg5320a_i2c_write_one(data, ISG5320A_PROTECT_REG,
				     ISG5320A_PRT_VALUE);
	if (ret < 0)
		pr_err("%s unlock protect failed(%d)\n", ISG5320A_TAG, ret);

	ret = isg5320a_i2c_write_one(data, ISG5320A_SOFTRESET_REG,
				     ISG5320A_RST_VALUE);
	if (ret < 0)
		pr_err("%s soft reset failed(%d)\n", ISG5320A_TAG, ret);

	usleep_range(1000, 1100);

	cnt = 0;
	while (gpio_get_value_cansleep(data->gpio_int) != 0 && cnt++ < 10)
		usleep_range(5000, 5100);

	if (cnt >= 10) {
		pr_err("%s wait soft reset failed\n", ISG5320A_TAG);
		return -EIO;
	} else {
		pr_info("%s wait cnt:%d\n", ISG5320A_TAG, cnt);
	}

	return ret;
}

static void isg5320a_force_calibration(struct isg5320a_data *data,
				       bool only_bfcal)

{
	mutex_lock(&data->lock);

	pr_info("%s %s(%d)\n", ISG5320A_TAG, __func__, only_bfcal ? 1 : 0);

	if (!only_bfcal) {
		isg5320a_i2c_write_one(data, ISG5320A_SCANCTRL1_REG,
				       ISG5320A_SCAN_STOP);
		msleep(30);
		isg5320a_i2c_write_one(data, ISG5320A_BS_ON_WD_REG, ISG5320A_BS_WD_OFF);
		isg5320a_i2c_write_one(data, ISG5320A_SCANCTRL2_REG,
				       ISG5320A_SCAN2_CLEAR);
		msleep(100);
		isg5320a_i2c_write_one(data, ISG5320A_OSCCON_REG, ISG5320A_OSC_NOMAL);

		isg5320a_i2c_write_one(data, ISG5320A_BS_ON_WD_REG, ISG5320A_BS_WD_ON);
		isg5320a_i2c_write_one(data, ISG5320A_SCANCTRL2_REG,
				       ISG5320A_SCAN2_RESET);

		isg5320a_i2c_write_one(data, ISG5320A_SCANCTRL1_REG,
				       ISG5320A_CFCAL_START);
		msleep(500);
	}

	isg5320a_i2c_write_one(data, ISG5320A_SCANCTRL2_REG, ISG5320A_BFCAL_START);

	mutex_unlock(&data->lock);
}

static inline unsigned char str2int(unsigned char c)
{
	if (c >= '0' && c <= '9')
		return c - '0';

	if (c >= 'A' && c <= 'F')
		return c - 'A' + 10;

	if (c >= 'a' && c <= 'f')
		return c - 'a' + 10;

	return 0;
}

static int isg5320a_setup_reg(struct isg5320a_data *data)
{
	int ret;
	int i;
	long file_size = 0;
	uint8_t *file_data;
	struct file *fp = NULL;
	loff_t pos;
	mm_segment_t old_fs = { 0 };
	u8 addr = 0;
	u8 val = 0;

	old_fs = get_fs();
	set_fs(KERNEL_DS);

	fp = filp_open("/sdcard/isg5320a_sub.reg", O_RDONLY, 0);
	if (IS_ERR(fp)) {
		pr_err("%s %s - file not exist:%ld\n", ISG5320A_TAG, __func__,
		       PTR_ERR(fp));
		set_fs(old_fs);
		return -1;
	}

	file_size = fp->f_path.dentry->d_inode->i_size;
	pr_info("%s %s - size of the file : %ld(bytes)\n", ISG5320A_TAG,
		__func__, file_size);
	file_data = vzalloc(file_size);
	pos = 0;
	ret = vfs_read(fp, (char __user *)file_data, file_size, &pos);
	if (ret != file_size) {
		pr_err("%s %s - failed to read file (ret = %d)\n", ISG5320A_TAG,
		       __func__, ret);
		vfree(file_data);
		filp_close(fp, current->files);
		return -1;
	}

	filp_close(fp, current->files);
	set_fs(old_fs);

	for (i = 0; i < (int)file_size; i++) {
		if (file_data[i] == 'W') {
			if ((i + 6) >= file_size)
				break;
			addr = (u8)((str2int(file_data[i + 2]) * 16) +
				    str2int(file_data[i + 3]));
			val = (u8)((str2int(file_data[i + 5]) * 16) +
				   str2int(file_data[i + 6]));
			isg5320a_i2c_write_one(data, addr, val);
			pr_info("%s W %02X %02X\n", ISG5320A_TAG, addr, val);
			isg5320a_i2c_read(data, addr, &val, 1);
			pr_info("%s   %02X %02X\n", ISG5320A_TAG, addr, val);
		} else if (file_data[i] == 'R') {
			if ((i + 3) >= file_size)
				break;
			addr = (u8)((str2int(file_data[i + 2]) * 16) +
				    str2int(file_data[i + 3]));
			isg5320a_i2c_read(data, addr, &val, 1);
			pr_info("%s R %02X %02X\n", ISG5320A_TAG, addr, val);
		}
	}

	vfree(file_data);

	return 0;
}

static void isg5320a_get_raw_data(struct isg5320a_data *data, bool log_print)
{
	int ret = 0;
	u8 buf[4];
	u16 cpbuf;
	u32 temp;

	mutex_lock(&data->lock);
	ret = isg5320a_i2c_read(data, ISG5320A_CDC16_T_H_REG, buf, sizeof(buf));

	if (ret < 0) {
		pr_info("%s fail to get data\n", ISG5320A_TAG);
	} else {
		temp = ((u32)buf[0] << 8) | (u32)buf[1];
		if ((temp != 0) && (temp != 0xFFFF))
			data->cdc = temp;

		temp = ((u32)buf[2] << 8) | (u32)buf[3];
		if ((temp != 0) && (temp != 0xFFFF))
			data->base = temp;
		data->diff = (s32)data->cdc - (s32)data->base;
	}

	ret = isg5320a_i2c_read(data, ISG5320A_COARSE_OUT_B_REG, (u8 *)&cpbuf, 2);
	if (ret < 0)
		pr_info("%s fail to get capMain\n", ISG5320A_TAG);
	else
		data->fine_coarse = cpbuf;

	mutex_unlock(&data->lock);

	if (log_print) {
		pr_info("%s capMain: %d%02d, cdc: %d, baseline:%d, diff:%d, skip_data:%d\n",
			ISG5320A_TAG, (data->fine_coarse & 0xFF),
			((data->fine_coarse >> 8) & 0x3F), data->cdc, data->base,
			data->diff, data->skip_data);
	} else {
		if (data->debug_cnt >= GRIP_LOG_TIME) {
			pr_info("%s capMain: %d%02d, cdc: %d, baseline:%d, diff:%d, skip_data:%d\n",
				ISG5320A_TAG, (data->fine_coarse & 0xFF),
				((data->fine_coarse >> 8) & 0x3F), data->cdc, data->base,
				data->diff, data->skip_data);
			data->debug_cnt = 0;
		} else {
			data->debug_cnt++;
		}
	}
}

static void force_far_grip(struct isg5320a_data *data)
{
	if (data->state == CLOSE) {
		pr_info("%s %s\n", ISG5320A_TAG, __func__);

		if (data->skip_data == true)
			return;

		input_report_rel(data->input_dev, REL_MISC, 2);
		input_sync(data->input_dev);
		data->state = FAR;
	}
}

static void report_event_data(struct isg5320a_data *data, u8 intr_msg)
{
	int state;

	if (data->skip_data == true) {
		pr_info("%s - skip grip event\n", ISG5320A_TAG);
		return;
	}

	state = (intr_msg & (1 << ISG5320A_PROX_STATE)) ? CLOSE : FAR;

	if (data->abnormal_mode) {
		if (state == CLOSE) {
			if (data->max_diff < data->diff)
				data->max_diff = data->diff;
			data->irq_count++;
		}
	}

	if (state == CLOSE) {
		if (data->state == FAR) {
			pr_info("%s CLOSE\n", ISG5320A_TAG);
			data->state = CLOSE;
			data->bfcal_chk_start = true;
			data->bfcal_chk_ready = false;
			data->bfcal_chk_count = 0;
		} else {
			pr_info("%s still CLOSE\n", ISG5320A_TAG);
		}
	} else {
		if (data->state == CLOSE) {
			pr_info("%s FAR\n", ISG5320A_TAG);
			data->state = FAR;
			data->bfcal_chk_start = false;
			data->bfcal_chk_ready = false;
			data->bfcal_chk_count = 0;
		} else {
			pr_info("%s already FAR\n", ISG5320A_TAG);
		}
	}

	if (data->state == CLOSE)
		input_report_rel(data->input_dev, REL_MISC, 1);
	else
		input_report_rel(data->input_dev, REL_MISC, 2);

	input_sync(data->input_dev);
}

static irqreturn_t isg5320a_irq_thread(int irq, void *ptr)
{
	int ret;
	int i;
	u8 intr_msg = 0;
	u8 *buf8;
	struct isg5320a_data *data = (struct isg5320a_data *)ptr;

	if (data->initialized == OFF)
		return IRQ_HANDLED;

	__pm_stay_awake(data->grip_ws);

	isg5320a_get_raw_data(data, true);

	isg5320a_i2c_read(data, ISG5320A_IRQSRC_REG, &intr_msg, 1);

	if (data->intr_debug_size > 0) {
		buf8 = kzalloc(data->intr_debug_size, GFP_KERNEL);
		if (buf8) {
			pr_info("%s Intr_debug1 (0x%02X)\n", ISG5320A_TAG,
				data->intr_debug_addr);
			isg5320a_i2c_read(data, data->intr_debug_addr, buf8,
					  data->intr_debug_size);
			for (i = 0; i < data->intr_debug_size; i++)
				pr_info("%s \t%02X\n", ISG5320A_TAG, buf8[i]);
			kfree(buf8);
		}
	}

	ret = isg5320a_i2c_read(data, ISG5320A_IRQSTS_REG, &intr_msg, 1);
	if (ret < 0) {
		pr_err("%s fail to read state(%d)\n", ISG5320A_TAG, ret);
		goto irq_end;
	}

	pr_info("%s intr msg: 0x%02X\n", ISG5320A_TAG, intr_msg);

	report_event_data(data, intr_msg);

irq_end:
	__pm_relax(data->grip_ws);

	return IRQ_HANDLED;
}

static void isg5320a_initialize(struct isg5320a_data *data)
{
	int ret;
	int i;
	u8 val;
	u8 buf[2];

	pr_info("%s %s\n", ISG5320A_TAG, __func__);

	force_far_grip(data);

	isg5320a_i2c_read(data, ISG5320A_IRQSRC_REG, &val, 1);
	isg5320a_i2c_write_one(data, ISG5320A_SCANCTRL1_REG, ISG5320A_SCAN_STOP);
	msleep(30);

	ret = isg5320a_setup_reg(data);
	if (ret < 0) {
		for (i = 0; i < (int)(sizeof(setup_reg) >> 1); i++) {
			isg5320a_i2c_write_one(data, setup_reg[i].addr, setup_reg[i].val);
			pr_info("%s W %02X %02X\n", ISG5320A_TAG, setup_reg[i].addr,
				setup_reg[i].val);
			isg5320a_i2c_read(data, setup_reg[i].addr, &val, 1);
			pr_info("%s   %02X %02X\n", ISG5320A_TAG, setup_reg[i].addr, val);
		}
	}

	if (data->normal_th > 0) {
		buf[0] = (data->normal_th >> 8) & 0xFF;
		buf[1] = data->normal_th & 0xFF;

		isg5320a_i2c_write(data, ISG5320A_B_PROXCTL3_REG, buf);
	}

	ret = isg5320a_i2c_read(data, ISG5320A_DIGITAL_ACC_REG, &val, 1);
	if (ret < 0)
		pr_err("%s fail to read DIGITAL ACC(%d)\n", ISG5320A_TAG, ret);
	else
		data->cfcal_th = ISG5320A_RESET_CONDITION * val / 8;

	data->initialized = ON;
}

static void isg5320a_set_debug_work(struct isg5320a_data *data, bool enable,
				    unsigned int delay_ms)
{
	if (enable == ON) {
		data->debug_cnt = 0;
		schedule_delayed_work(&data->debug_work, msecs_to_jiffies(delay_ms));
		schedule_delayed_work(&data->cal_work, msecs_to_jiffies(delay_ms));
	} else {
		cancel_delayed_work_sync(&data->debug_work);
		cancel_delayed_work_sync(&data->cal_work);
	}
}

static void isg5320a_set_enable(struct isg5320a_data *data, int enable)
{
	u8 state;
	int ret = 0;

	pr_info("%s %s\n", ISG5320A_TAG, __func__);

	if (data->enable == enable) {
		pr_info("%s - already enabled\n", ISG5320A_TAG);
		return;
	}

	if (enable == ON) {
		pr_info("%s %s - enable\n", ISG5320A_TAG, __func__);

		data->diff_avg = 0;
		data->diff_cnt = 0;
		data->cdc_avg = 0;

		ret = isg5320a_i2c_read(data, ISG5320A_IRQSTS_REG, &state, 1);
		if (ret < 0)
			return;

		isg5320a_get_raw_data(data, true);

		if (data->skip_data == true) {
			input_report_rel(data->input_dev, REL_MISC, 2);
		} else if (state & (1 << ISG5320A_PROX_STATE)) {
			data->state = CLOSE;
			input_report_rel(data->input_dev, REL_MISC, 1);
		} else {
			data->state = FAR;
			input_report_rel(data->input_dev, REL_MISC, 2);
		}
		input_sync(data->input_dev);

		isg5320a_i2c_read(data, ISG5320A_IRQSRC_REG, &state, 1);
		isg5320a_i2c_write_one(data, ISG5320A_IRQFUNC_REG, ISG5320A_IRQ_ENABLE);

		enable_irq(data->client->irq);
		enable_irq_wake(data->client->irq);
	} else {
		pr_info("%s %s - disable\n", ISG5320A_TAG, __func__);

		isg5320a_i2c_write_one(data, ISG5320A_IRQFUNC_REG, ISG5320A_IRQ_DISABLE);

		disable_irq(data->client->irq);
		disable_irq_wake(data->client->irq);
	}

	data->enable = enable;
}

static int isg5320a_set_mode(struct isg5320a_data *data, unsigned char mode)
{
	int ret = -EINVAL;
	u8 state;

	isg5320a_i2c_read(data, ISG5320A_IRQSRC_REG, &state, 1);

	if (mode == ISG5320A_MODE_SLEEP) {
		isg5320a_i2c_write_one(data, ISG5320A_SCANCTRL1_REG,
				       ISG5320A_SCAN_STOP);
		isg5320a_i2c_write_one(data, ISG5320A_OSCCON_REG, ISG5320A_OSC_SLEEP);
		isg5320a_i2c_write_one(data, ISG5320A_BS_ON_WD_REG, ISG5320A_BS_WD_OFF);
	} else if (mode == ISG5320A_MODE_NORMAL) {
		isg5320a_i2c_write_one(data, ISG5320A_BS_ON_WD_REG, ISG5320A_BS_WD_ON);
		isg5320a_force_calibration(data, false);
	}

	pr_info("%s %s - change the mode : %u\n", ISG5320A_TAG, __func__, mode);

	return ret;
}

static ssize_t isg5320a_name_show(struct device *dev,
				  struct device_attribute *attr, char *buf)
{
	pr_info("%s %s%s\n", ISG5320A_TAG, __func__, DEVICE_NAME);

	return sprintf(buf, "%s\n", DEVICE_NAME);
}

static ssize_t isg5320a_vendor_show(struct device *dev,
				    struct device_attribute *attr, char *buf)
{
	pr_info("%s %s%s\n", ISG5320A_TAG, __func__, VENDOR_NAME);

	return sprintf(buf, "%s\n", VENDOR_NAME);
}

static ssize_t isg5320a_mode_show(struct device *dev,
				  struct device_attribute *attr, char *buf)
{
	return sprintf(buf, "1\n");
}

static ssize_t isg5320a_acal_show(struct device *dev,
				  struct device_attribute *attr, char *buf)
{
	return sprintf(buf, "2,0,0\n");
}

static ssize_t isg5320a_manual_acal_show(struct device *dev,
					 struct device_attribute *attr, char *buf)
{


	return sprintf(buf, "OK\n");
}

static ssize_t isg5320a_onoff_store(struct device *dev,
				    struct device_attribute *attr, const char *buf, size_t count)
{
	u8 val;
	int ret;
	struct isg5320a_data *data = dev_get_drvdata(dev);

	ret = kstrtou8(buf, 2, &val);
	if (ret) {
		pr_err("%s invalid argument\n", ISG5320A_TAG);
		return ret;
	}

	if (val == 0) {
		data->skip_data = true;
		if (data->enable == ON) {
			data->state = FAR;
			input_report_rel(data->input_dev, REL_MISC, 2);
			input_sync(data->input_dev);
		}
	} else {
		data->skip_data = false;
	}

	pr_info("%s %d\n", ISG5320A_TAG, (int)val);

	return count;
}

static ssize_t isg5320a_onoff_show(struct device *dev,
				   struct device_attribute *attr, char *buf)
{
	struct isg5320a_data *data = dev_get_drvdata(dev);

	return sprintf(buf, "%u\n", !data->skip_data);
}

static ssize_t isg5320a_sw_reset_show(struct device *dev,
				      struct device_attribute *attr, char *buf)
{
	struct isg5320a_data *data = dev_get_drvdata(dev);

	pr_info("%s %s\n", ISG5320A_TAG, __func__);
	isg5320a_force_calibration(data, true);
	msleep(300);
	isg5320a_get_raw_data(data, true);

	return sprintf(buf, "%d\n", 0);
}

static ssize_t isg5320a_normal_threshold_store(struct device *dev,
					       struct device_attribute *attr, const char *buf, size_t size)
{
	int val = 0;
	u8 buf8[2];
	struct isg5320a_data *data = dev_get_drvdata(dev);

	sscanf(buf, "%d", &val);

	if (val < 0) {
		pr_err("%s invalid argument\n", ISG5320A_TAG);
		return size;
	}

	pr_info("%s change threshold(%d->%d)\n", ISG5320A_TAG, data->normal_th,
		val);

	data->normal_th = val;

	buf8[0] = (data->normal_th >> 8) & 0xFF;
	buf8[1] = data->normal_th & 0xFF;

	isg5320a_i2c_write(data, ISG5320A_B_PROXCTL3_REG, buf8);

	return size;
}

static ssize_t isg5320a_normal_threshold_show(struct device *dev,
					      struct device_attribute *attr, char *buf)
{
	u32 threshold = 0;
	u32 close_hyst = 0;
	u32 far_hyst = 0;
	u8 buf8[6];
	struct isg5320a_data *data = dev_get_drvdata(dev);

	isg5320a_i2c_read(data, ISG5320A_B_PROXCTL3_REG, buf8, sizeof(buf8));

	threshold = ((u32)buf8[0] << 8) | (u32)buf8[1];
	close_hyst = ((u32)buf8[2] << 8) | (u32)buf8[3];
	far_hyst = ((u32)buf8[4] << 8) | (u32)buf8[5];

	return sprintf(buf, "%d,%d\n", threshold + close_hyst,
		       threshold - far_hyst);
}


static ssize_t isg5320a_raw_data_show(struct device *dev,
				      struct device_attribute *attr, char *buf)
{
	struct isg5320a_data *data = dev_get_drvdata(dev);

	isg5320a_get_raw_data(data, true);
	if (data->diff_cnt == 0) {
		data->diff_sum = data->diff;
		data->cdc_sum = data->cdc;
	} else {
		data->diff_sum += data->diff;
		data->cdc_sum += data->cdc;
	}

	if (++data->diff_cnt >= ISG5320A_DIFF_AVR_CNT) {
		data->diff_avg = data->diff_sum / ISG5320A_DIFF_AVR_CNT;
		data->cdc_avg = data->cdc_sum / ISG5320A_DIFF_AVR_CNT;
		data->diff_cnt = 0;
	}

	return sprintf(buf, "%d%02d,%d,%d,%d,%d\n", (data->fine_coarse & 0xFF),
		       ((data->fine_coarse >> 8) & 0x3F), data->cdc,
		       data->fine_coarse, data->diff, data->base);
}

static ssize_t isg5320a_debug_raw_data_show(struct device *dev,
					    struct device_attribute *attr, char *buf)
{
	int ret = 0;
	u8 buff[10];
	u16 temp;
	struct isg5320a_data *data = dev_get_drvdata(dev);

	mutex_lock(&data->lock);
	ret = isg5320a_i2c_read(data, ISG5320A_CDC16_A_H_REG, buff, sizeof(buff));
	mutex_unlock(&data->lock);
	if (ret < 0) {
		pr_info("%s fail to get data\n", ISG5320A_TAG);
	} else {
		temp = ((u32)buff[0] << 8) | (u32)buff[1];
		if ((temp != 0) && (temp != 0xFFFF))
			data->debug_cdc[0] = temp;

		temp = ((u32)buff[2] << 8) | (u32)buff[3];
		if ((temp != 0) && (temp != 0xFFFF))
			data->debug_base[0] = temp;
		data->debug_diff[0] =
			(s32)data->debug_cdc[0] - (s32)data->debug_base[0];

		temp = ((u32)buff[6] << 8) | (u32)buff[7];
		if ((temp != 0) && (temp != 0xFFFF))
			data->debug_cdc[1] = temp;

		temp = ((u32)buff[8] << 8) | (u32)buff[9];
		if ((temp != 0) && (temp != 0xFFFF))
			data->debug_base[1] = temp;
		data->debug_diff[1] =
			(s32)data->debug_cdc[1] - (s32)data->debug_base[1];

		temp = ((u32)buff[4] << 8) | (u32)buff[5];
		if ((temp != 0) && (temp != 0xFFFF))
			data->debug_cdc[2] = temp;
		data->debug_diff[2] =
			(s32)data->debug_cdc[2] - (s32)data->debug_base[1];
	}

	return sprintf(buf, "%d,%d,%d,%d,%d,%d,%d,%d,%d\n", data->debug_cdc[0],
		       data->debug_diff[0], data->debug_base[0], data->debug_cdc[1],
		       data->debug_diff[1], data->debug_base[1], data->debug_cdc[2],
		       data->debug_diff[2], data->debug_base[1]);
}

static ssize_t isg5320a_debug_data_show(struct device *dev,
					struct device_attribute *attr, char *buf)
{
	struct isg5320a_data *data = dev_get_drvdata(dev);

	return sprintf(buf, "%d,%d,%d\n", data->cdc, data->base, data->diff);
}

static ssize_t isg5320a_diff_avg_show(struct device *dev,
				      struct device_attribute *attr, char *buf)
{
	struct isg5320a_data *data = dev_get_drvdata(dev);

	return sprintf(buf, "%d\n", data->diff_avg);
}

static ssize_t isg5320a_cdc_avg_show(struct device *dev,
				     struct device_attribute *attr, char *buf)
{
	struct isg5320a_data *data = dev_get_drvdata(dev);

	return snprintf(buf, PAGE_SIZE, "%d\n", data->cdc_avg);
}

static ssize_t isg5320a_ch_state_show(struct device *dev,
				      struct device_attribute *attr, char *buf)
{
	int count;
	struct isg5320a_data *data = dev_get_drvdata(dev);

	if (data->skip_data == true)
		count = snprintf(buf, PAGE_SIZE, "%d,%d\n", NONE_ENABLE, NONE_ENABLE);
	else if (data->enable == ON)
		count = snprintf(buf, PAGE_SIZE, "%d,%d\n", data->state, NONE_ENABLE);
	else
		count = snprintf(buf, PAGE_SIZE, "%d,%d\n", NONE_ENABLE, NONE_ENABLE);

	return count;
}

static ssize_t isg5320a_hysteresis_show(struct device *dev,
					struct device_attribute *attr, char *buf)
{
	u32 far_hyst = 0;
	u8 buf8[6];
	struct isg5320a_data *data = dev_get_drvdata(dev);

	isg5320a_i2c_read(data, ISG5320A_B_PROXCTL3_REG, buf8, sizeof(buf8));

	far_hyst = ((u32)buf8[4] << 8) | (u32)buf8[5];

	return sprintf(buf, "%d\n", far_hyst);
}

static ssize_t isg5320a_reg_update_show(struct device *dev,
					struct device_attribute *attr, char *buf)
{
	int enable_backup;
	struct isg5320a_data *data = dev_get_drvdata(dev);

	enable_backup = data->enable;

	isg5320a_reset(data);
	if (enable_backup)
		isg5320a_set_enable(data, OFF);
	isg5320a_set_mode(data, ISG5320A_MODE_SLEEP);
	isg5320a_initialize(data);
	if (enable_backup)
		isg5320a_set_enable(data, ON);
	isg5320a_set_mode(data, ISG5320A_MODE_NORMAL);

	return sprintf(buf, "OK\n");
}

#define DIRECT_CMD_WRITE            'w'
#define DIRECT_CMD_READ             'r'
#define DIRECT_BUF_COUNT            16
static ssize_t isg5320a_direct_store(struct device *dev,
				     struct device_attribute *attr, const char *buf, size_t size)
{
	int ret = -EPERM;
	u32 tmp1, tmp2;
	struct isg5320a_data *data = dev_get_drvdata(dev);
	direct_info *direct = (direct_info *)&data->direct;

	sscanf(buf, "%c %x %x", &direct->cmd, &tmp1, &tmp2);

	direct->addr = tmp1;
	direct->val = tmp2;

	pr_info("%s direct cmd: %c, addr: %x, val: %x\n", ISG5320A_TAG, direct->cmd,
		direct->addr, direct->val);

	if ((direct->cmd != DIRECT_CMD_WRITE) && (direct->cmd != DIRECT_CMD_READ)) {
		pr_err("%s direct cmd is not correct!\n", ISG5320A_TAG);
		return size;
	}

	if (direct->cmd == DIRECT_CMD_WRITE) {
		ret = isg5320a_i2c_write_one(data, direct->addr, direct->val);
		if (ret < 0)
			pr_err("%s direct write fail\n", ISG5320A_TAG);
		else
			pr_info("%s direct write addr: %x, val: %x\n", ISG5320A_TAG,
				direct->addr, direct->val);
	}

	return size;
}

static ssize_t isg5320a_direct_show(struct device *dev,
				    struct device_attribute *attr, char *buf)
{
	int i, count = 0;
	int ret = 0;
	int len;
	u8 addr;
	const int msg_len = 256;
	char msg[msg_len];
	struct isg5320a_data *data = dev_get_drvdata(dev);
	direct_info *direct = (direct_info *)&data->direct;
	u8 buf8[DIRECT_BUF_COUNT] = {0,};
	int max_len = DIRECT_BUF_COUNT;

	if (direct->cmd != DIRECT_CMD_READ)
		return sprintf(buf, "ex) echo r addr len size(display) > direct\n");

	len = direct->val;
	addr = direct->addr;

	while (len > 0) {
		if (len < max_len)
			max_len = len;

		ret = isg5320a_i2c_read(data, addr, buf8, max_len);
		if (ret < 0) {
			count = sprintf(buf, "i2c read fail\n");
			break;
		}
		addr += max_len;

		for (i = 0; i < max_len; i++) {
			count += snprintf(msg, msg_len, "0x%02X ", buf8[i]);
			strncat(buf, msg, msg_len);
		}
		count += snprintf(msg, msg_len, "\n");
		strncat(buf, msg, msg_len);

		len -= max_len;
	}

	return count;
}

static ssize_t isg5320a_intr_debug_store(struct device *dev,
					 struct device_attribute *attr, const char *buf, size_t size)
{
	u32 tmp1;
	struct isg5320a_data *data = dev_get_drvdata(dev);

	sscanf(buf, "%x %d", &tmp1, &data->intr_debug_size);

	data->intr_debug_addr = tmp1;

	pr_info("%s intr debug addr: 0x%x, count: %d\n", ISG5320A_TAG,
		data->intr_debug_addr, data->intr_debug_size);

	return size;
}

static ssize_t isg5320a_intr_debug_show(struct device *dev,
					struct device_attribute *attr, char *buf)
{
	struct isg5320a_data *data = dev_get_drvdata(dev);

	pr_info("%s intr debug addr: 0x%x, count: %d\n", ISG5320A_TAG,
		data->intr_debug_addr, data->intr_debug_size);

	return sprintf(buf, "intr debug addr: 0x%x, count: %d\n",
		       data->intr_debug_addr, data->intr_debug_size);
}

static ssize_t isg5320a_cp_show(struct device *dev,
				struct device_attribute *attr, char *buf)
{
	int ret;
	u16 buff;
	struct isg5320a_data *data = dev_get_drvdata(dev);

	ret = isg5320a_i2c_read(data, ISG5320A_COARSE_B_REG, (u8 *)&buff, 2);
	if (ret < 0) {
		pr_info("%s fail to get cp\n", ISG5320A_TAG);
	} else {
		data->fine_coarse = buff;
		pr_info("%s coarse B:%04X\n", ISG5320A_TAG, data->fine_coarse);
	}

	return sprintf(buf, "%d%02d,0\n", (data->fine_coarse & 0xFF),
		       (data->fine_coarse >> 8) & 0x3F);
}

#define SCAN_INT            0x12
#define FAR_CLOSE_INT       0x0C
static ssize_t isg5320a_scan_int_show(struct device *dev,
				      struct device_attribute *attr, char *buf)
{
	int ret;
	struct isg5320a_data *data = dev_get_drvdata(dev);

	ret = isg5320a_i2c_write_one(data, ISG5320A_IRQCON_REG, SCAN_INT);
	if (ret < 0) {
		pr_err("%s fail to set scan done int\n", ISG5320A_TAG);
		return sprintf(buf, "FAIL\n");
	} else {
		pr_info("%s set scan done int\n", ISG5320A_TAG);
		return sprintf(buf, "OK\n");
	}
}

static ssize_t isg5320a_far_close_int_show(struct device *dev,
					   struct device_attribute *attr, char *buf)
{
	int ret;
	struct isg5320a_data *data = dev_get_drvdata(dev);

	ret = isg5320a_i2c_write_one(data, ISG5320A_IRQCON_REG, FAR_CLOSE_INT);
	if (ret < 0) {
		pr_err("%s fail to set normal int\n", ISG5320A_TAG);
		return sprintf(buf, "FAIL\n");
	} else {
		pr_info("%s set normal int\n", ISG5320A_TAG);
		return sprintf(buf, "OK\n");
	}
}

static ssize_t isg5320a_toggle_enable_show(struct device *dev,
					   struct device_attribute *attr, char *buf)
{
	int enable;
	struct isg5320a_data *data = dev_get_drvdata(dev);

	enable = (data->enable == OFF) ? ON : OFF;
	isg5320a_set_enable(data, (int)enable);

	return sprintf(buf, "%d\n", data->enable);
}

static ssize_t isg5320a_enable_store(struct device *dev,
				     struct device_attribute *attr, const char *buf, size_t size)
{
	int ret;
	u8 enable;
	struct isg5320a_data *data = dev_get_drvdata(dev);
	int pre_enable = data->enable;

	ret = kstrtou8(buf, 2, &enable);
	if (ret) {
		pr_err("%s invalid argument\n", ISG5320A_TAG);
		return size;
	}

	pr_info("%s new_value=%d old_value=%d\n", ISG5320A_TAG, (int)enable,
		pre_enable);

	if (pre_enable == enable)
		return size;

	isg5320a_set_enable(data, (int)enable);

	return size;
}

static ssize_t isg5320a_enable_show(struct device *dev,
				    struct device_attribute *attr, char *buf)
{
	struct isg5320a_data *data = dev_get_drvdata(dev);

	return sprintf(buf, "%d\n", data->enable);
}

static ssize_t isg5320a_sampling_freq_show(struct device *dev,
					   struct device_attribute *attr, char *buf)
{
	struct isg5320a_data *data = dev_get_drvdata(dev);
	u8 buff;
	int sampling_freq;

	isg5320a_i2c_read(data, ISG5320A_NUM_OF_CLK, &buff, 1);
	sampling_freq = (int)(4000 / ((int)buff + 1));

	return snprintf(buf, PAGE_SIZE, "%dkHz\n", sampling_freq);
}

static ssize_t isg5320a_isum_show(struct device *dev,
				  struct device_attribute *attr, char *buf)
{
	struct isg5320a_data *data = dev_get_drvdata(dev);
	const char *table[16] = {
		"0", "0", "0", "0", "0", "0", "0", "0", "0",
		"20", "24", "28", "32", "40", "48", "64"
	};
	u8 buff = 0;

	isg5320a_i2c_read(data, ISG5320A_CHB_LSUM_TYPE_REG, &buff, 1);
	buff = (buff & 0xf0) >> 4;

	return snprintf(buf, PAGE_SIZE, "%s\n", table[buff]);
}

static ssize_t isg5320a_scan_period_show(struct device *dev,
					 struct device_attribute *attr, char *buf)
{
	struct isg5320a_data *data = dev_get_drvdata(dev);
	u8 buff[2];
	int scan_period;

	isg5320a_i2c_read(data, ISG5320A_WUTDATA_REG, (u8 *)&buff, sizeof(buff));

	scan_period = (int)(((u16)buff[1] & 0xff) | (((u16)buff[0] & 0x3f) << 8));
	if (!scan_period)
		return snprintf(buf, PAGE_SIZE, "%d\n", scan_period);

	scan_period = (int)(4000 / scan_period);

	return snprintf(buf, PAGE_SIZE, "%d\n", scan_period);
}

static ssize_t isg5320a_again_show(struct device *dev,
				   struct device_attribute *attr, char *buf)
{
	struct isg5320a_data *data = dev_get_drvdata(dev);
	u8 buff;
	u8 temp1, temp2;

	isg5320a_i2c_read(data, ISG5320A_ANALOG_GAIN, &buff, 1);
	temp1 = (buff & 0x38) >> 3;
	temp2 = (buff & 0x07);

	return snprintf(buf, PAGE_SIZE, "%d/%d\n", (int)temp2 + 1, (int)temp1 + 1);
}

static ssize_t isg5320a_cdc_up_coef_show(struct device *dev,
					 struct device_attribute *attr, char *buf)
{
	struct isg5320a_data *data = dev_get_drvdata(dev);
	u8 buff;
	int coef;

	isg5320a_i2c_read(data, ISG5320A_CHB_CDC_UP_COEF_REG, &buff, 1);
	coef = (int)buff;

	return snprintf(buf, PAGE_SIZE, "%x, %d\n", buff, coef);
}

static ssize_t isg5320a_cdc_down_coef_show(struct device *dev,
					   struct device_attribute *attr, char *buf)
{
	struct isg5320a_data *data = dev_get_drvdata(dev);
	u8 buff;
	int coef;

	isg5320a_i2c_read(data, ISG5320A_CHB_CDC_DN_COEF_REG, &buff, 1);
	coef = (int)buff;

	return snprintf(buf, PAGE_SIZE, "%x, %d\n", buff, coef);
}

static ssize_t isg5320a_temp_enable_show(struct device *dev,
					 struct device_attribute *attr, char *buf)
{
	struct isg5320a_data *data = dev_get_drvdata(dev);
	u8 buff;

	isg5320a_i2c_read(data, ISG5320A_TEMPERATURE_ENABLE_REG, &buff, 1);

	return snprintf(buf, PAGE_SIZE, "%d\n", ((buff & 0x40) >> 6));
}


static ssize_t isg5320a_cml_show(struct device *dev,
				 struct device_attribute *attr, char *buf)
{
	struct isg5320a_data *data = dev_get_drvdata(dev);
	u8 buff;

	isg5320a_i2c_read(data, ISG5320A_CML_BIAS_REG, &buff, 1);

	return snprintf(buf, PAGE_SIZE, "%d\n", (buff & 0x07));
}

static ssize_t isg5320a_irq_count_show(struct device *dev,
				       struct device_attribute *attr, char *buf)
{
	struct isg5320a_data *data = dev_get_drvdata(dev);

	int ret = 0;
	s16 max_diff_val = 0;

	if (data->irq_count) {
		ret = -1;
		max_diff_val = data->max_diff;
	} else {
		max_diff_val = data->max_normal_diff;
	}

	pr_info("%s %s - called\n", ISG5320A_TAG, __func__);

	return snprintf(buf, PAGE_SIZE, "%d,%d,%d\n", ret, data->irq_count,
			max_diff_val);
}

static ssize_t isg5320a_irq_count_store(struct device *dev,
					struct device_attribute *attr, const char *buf, size_t count)
{
	struct isg5320a_data *data = dev_get_drvdata(dev);

	u8 onoff;
	int ret;

	ret = kstrtou8(buf, 10, &onoff);
	if (ret < 0) {
		pr_err("%s invalid argument\n", ISG5320A_TAG);
		return count;
	}

	mutex_lock(&data->lock);

	if (onoff == 0) {
		data->abnormal_mode = OFF;
	} else if (onoff == 1) {
		data->abnormal_mode = ON;
		data->irq_count = 0;
		data->max_diff = 0;
		data->max_normal_diff = 0;
	} else {
		pr_err("%s invalid value %d\n", ISG5320A_TAG, onoff);
	}

	mutex_unlock(&data->lock);

	pr_info("%s %s - %d\n", ISG5320A_TAG, __func__, onoff);

	return count;
}

static DEVICE_ATTR(name, 0444, isg5320a_name_show, NULL);
static DEVICE_ATTR(vendor, 0444, isg5320a_vendor_show, NULL);
static DEVICE_ATTR(mode, 0444, isg5320a_mode_show, NULL);
static DEVICE_ATTR(manual_acal, 0444, isg5320a_manual_acal_show, NULL);
static DEVICE_ATTR(calibration, 0444, isg5320a_acal_show, NULL);
static DEVICE_ATTR(onoff, 0664, isg5320a_onoff_show, isg5320a_onoff_store);
static DEVICE_ATTR(reset, 0444, isg5320a_sw_reset_show, NULL);
static DEVICE_ATTR(normal_threshold, 0664, isg5320a_normal_threshold_show,
		   isg5320a_normal_threshold_store);
static DEVICE_ATTR(raw_data, 0444, isg5320a_raw_data_show, NULL);
static DEVICE_ATTR(debug_raw_data, 0444, isg5320a_debug_raw_data_show, NULL);
static DEVICE_ATTR(debug_data, 0444, isg5320a_debug_data_show, NULL);
static DEVICE_ATTR(diff_avg, 0444, isg5320a_diff_avg_show, NULL);
static DEVICE_ATTR(cdc_avg, 0444, isg5320a_cdc_avg_show, NULL);
static DEVICE_ATTR(useful_avg, 0444, isg5320a_cdc_avg_show, NULL);
static DEVICE_ATTR(ch_state, 0444, isg5320a_ch_state_show, NULL);
static DEVICE_ATTR(hysteresis, 0444, isg5320a_hysteresis_show, NULL);
static DEVICE_ATTR(reg_update, 0444, isg5320a_reg_update_show, NULL);
static DEVICE_ATTR(direct, 0664, isg5320a_direct_show, isg5320a_direct_store);
static DEVICE_ATTR(intr_debug, 0664, isg5320a_intr_debug_show,
		   isg5320a_intr_debug_store);
static DEVICE_ATTR(cp, 0444, isg5320a_cp_show, NULL);
static DEVICE_ATTR(scan_int, 0444, isg5320a_scan_int_show, NULL);
static DEVICE_ATTR(far_close_int, 0444, isg5320a_far_close_int_show, NULL);
static DEVICE_ATTR(toggle_enable, 0444, isg5320a_toggle_enable_show, NULL);
static DEVICE_ATTR(sampling_freq, 0444, isg5320a_sampling_freq_show, NULL);
static DEVICE_ATTR(isum, 0444, isg5320a_isum_show, NULL);
static DEVICE_ATTR(scan_period, 0444, isg5320a_scan_period_show, NULL);
static DEVICE_ATTR(analog_gain, 0444, isg5320a_again_show, NULL);
static DEVICE_ATTR(cdc_up, 0444, isg5320a_cdc_down_coef_show, NULL);
static DEVICE_ATTR(cdc_down, 0444, isg5320a_cdc_up_coef_show, NULL);
static DEVICE_ATTR(temp_enable, 0444, isg5320a_temp_enable_show, NULL);
static DEVICE_ATTR(cml, 0444, isg5320a_cml_show, NULL);
static DEVICE_ATTR(irq_count, 0664, isg5320a_irq_count_show,
		   isg5320a_irq_count_store);

static struct device_attribute *sensor_attrs[] = {
	&dev_attr_name,
	&dev_attr_vendor,
	&dev_attr_mode,
	&dev_attr_manual_acal,
	&dev_attr_calibration,
	&dev_attr_onoff,
	&dev_attr_reset,
	&dev_attr_normal_threshold,
	&dev_attr_raw_data,
	&dev_attr_debug_raw_data,
	&dev_attr_debug_data,
	&dev_attr_diff_avg,
	&dev_attr_useful_avg,
	&dev_attr_cdc_avg,
	&dev_attr_ch_state,
	&dev_attr_hysteresis,
	&dev_attr_reg_update,
	&dev_attr_direct,
	&dev_attr_intr_debug,
	&dev_attr_cp,
	&dev_attr_scan_int,
	&dev_attr_far_close_int,
	&dev_attr_toggle_enable,
	&dev_attr_sampling_freq,
	&dev_attr_isum,
	&dev_attr_scan_period,
	&dev_attr_analog_gain,
	&dev_attr_cdc_up,
	&dev_attr_cdc_down,
	&dev_attr_temp_enable,
	&dev_attr_cml,
	&dev_attr_irq_count,
	NULL,
};

static DEVICE_ATTR(enable, 0664, isg5320a_enable_show, isg5320a_enable_store);

static struct attribute *isg5320a_attributes[] = {
	&dev_attr_enable.attr,
	NULL,
};

static struct attribute_group isg5320a_attribute_group = {
	.attrs = isg5320a_attributes,
};

#ifdef ISG5320A_INIT_DELAYEDWORK
static void init_work_func(struct work_struct *work)
{
	struct delayed_work *delayed_work = to_delayed_work(work);
	struct isg5320a_data *data = container_of(delayed_work,
						  struct isg5320a_data, init_work);

	isg5320a_initialize(data);
	isg5320a_set_mode(data, ISG5320A_MODE_NORMAL);
	isg5320a_set_debug_work(data, ON, 2000);
}
#endif

static void cal_work_func(struct work_struct *work)
{

	struct delayed_work *delayed_work = to_delayed_work(work);
	struct isg5320a_data *data = container_of(delayed_work,
						  struct isg5320a_data, cal_work);
	bool force_cal = false;

	isg5320a_get_raw_data(data, false);
	// check cfcal
	if (data->cdc < data->cfcal_th) {
		pr_info("%s cdc : %d, cfcal_th %d\n", ISG5320A_TAG, data->cdc,
			data->cfcal_th);
		isg5320a_force_calibration(data, false);
		force_cal = true;
	}

	// check bfcal
	if (data->bfcal_chk_start) {
		data->bfcal_chk_count++;
		if (data->bfcal_chk_count == ISG5320A_BFCAL_CHK_RDY_TIME) {
			data->bfcal_chk_ready = true;
			data->bfcal_chk_cdc = data->cdc;
			data->bfcal_chk_diff =
				data->diff / ISG5320A_BFCAL_CHK_DIFF_RATIO;
		} else if (data->bfcal_chk_count > ISG5320A_BFCAL_CHK_RDY_TIME) {
			if (((data->bfcal_chk_count - ISG5320A_BFCAL_CHK_RDY_TIME) %
			     ISG5320A_BFCAL_CHK_CYCLE_TIME) == 0) {
				if (((s32)data->bfcal_chk_cdc - (s32)data->cdc) >=
				    data->bfcal_chk_diff) {
					isg5320a_force_calibration(data, true);
					force_cal = true;
					data->bfcal_chk_start = false;
					data->bfcal_chk_ready = false;
					data->bfcal_chk_count = 0;
				} else {
					data->bfcal_chk_cdc = data->cdc;
					data->bfcal_chk_diff =
						data->diff / ISG5320A_BFCAL_CHK_DIFF_RATIO;
				}
			}
		}
	}

	if (force_cal)
		schedule_delayed_work(&data->cal_work, msecs_to_jiffies(1000));
	else
		schedule_delayed_work(&data->cal_work, msecs_to_jiffies(500));
}

static void debug_work_func(struct work_struct *work)
{
	int ret;
	struct delayed_work *delayed_work = to_delayed_work(work);
	struct isg5320a_data *data = container_of(delayed_work,
						  struct isg5320a_data, debug_work);

	if (data->hallic_detect) {
		ret = check_hallic_state(HALLIC_PATH, data->hall_ic);
		if (ret < 0)
			pr_err("%s hallic detect fail = %d\n", ISG5320A_TAG, ret);

		if (strcmp(data->hall_ic, "CLOSE") == 0) {
			if (data->hall_flag) {
				pr_info("%s hall IC is closed\n", ISG5320A_TAG);
				isg5320a_force_calibration(data, true);
				data->hall_flag = 0;
			}
		} else {
			data->hall_flag = 1;
		}
	}

	if (data->hallic_cert_detect) {
		ret = check_hallic_state(HALLIC_CERT_PATH, data->hall_ic);
		if (ret < 0)
			pr_err("%s Cert hall IC detect fail = %d\n", ISG5320A_TAG, ret);

		if (strcmp(data->hall_ic, "CLOSE") == 0) {
			if (data->hall_cert_flag) {
				pr_info("%s Cert hall IC is closed\n", ISG5320A_TAG);
				isg5320a_force_calibration(data, true);
				data->hall_cert_flag = 0;
			}
		} else {
			data->hall_cert_flag = 1;
		}
	}

	if (data->enable == ON) {
		if (data->abnormal_mode) {
			isg5320a_get_raw_data(data, true);
			if (data->max_normal_diff < data->diff)
				data->max_normal_diff = data->diff;
		}
	}

	schedule_delayed_work(&data->debug_work, msecs_to_jiffies(2000));
}

#if (IS_ENABLED(CONFIG_CCIC_NOTIFIER) || IS_ENABLED(CONFIG_PDIC_NOTIFIER)) && IS_ENABLED(CONFIG_USB_TYPEC_MANAGER_NOTIFIER)
static int isg5320a_ccic_handle_notification(struct notifier_block *nb,
					     unsigned long action, void *data)
{
#if IS_ENABLED(CONFIG_PDIC_NOTIFIER)
	PD_NOTI_USB_STATUS_TYPEDEF usb_status = *(PD_NOTI_USB_STATUS_TYPEDEF *)data;
#else
	CC_NOTI_USB_STATUS_TYPEDEF usb_status = *(CC_NOTI_USB_STATUS_TYPEDEF *)data;
#endif
	struct isg5320a_data *pdata = container_of(nb, struct isg5320a_data,
						   cpuidle_ccic_nb);
	static int pre_attach;

	if (pre_attach == usb_status.attach)
		return 0;
	/*
	 * USB_STATUS_NOTIFY_DETACH = 0,
	 * USB_STATUS_NOTIFY_ATTACH_DFP = 1, // Host
	 * USB_STATUS_NOTIFY_ATTACH_UFP = 2, // Device
	 * USB_STATUS_NOTIFY_ATTACH_DRP = 3, // Dual role
	 */

	if (pdata->initialized == ON) {
		switch (usb_status.drp) {
		case USB_STATUS_NOTIFY_ATTACH_UFP:
		case USB_STATUS_NOTIFY_ATTACH_DFP:
		case USB_STATUS_NOTIFY_DETACH:
			pr_info("%s - drp = %d attat = %d\n", ISG5320A_TAG, usb_status.drp,
				usb_status.attach);
			isg5320a_force_calibration(pdata, true);
			break;
		default:
			pr_info("%s - DRP type : %d\n", ISG5320A_TAG, usb_status.drp);
			break;
		}
	}

	pre_attach = usb_status.attach;

	return 0;
}
#elif IS_ENABLED(CONFIG_MUIC_NOTIFIER)
static int isg5320a_cpuidle_muic_notifier(struct notifier_block *nb,
					  unsigned long action, void *muic_data)
{

	muic_attached_dev_t attached_dev = *(muic_attached_dev_t *)muic_data;

	struct isg5320a_data *data = container_of(nb, struct isg5320a_data,
						  cpuidle_muic_nb);

	switch (attached_dev) {
	case ATTACHED_DEV_OTG_MUIC:
	case ATTACHED_DEV_USB_MUIC:
	case ATTACHED_DEV_TA_MUIC:
	case ATTACHED_DEV_AFC_CHARGER_PREPARE_MUIC:
	case ATTACHED_DEV_AFC_CHARGER_9V_MUIC:
		if (action == MUIC_NOTIFY_CMD_ATTACH)
			pr_info("%s TA/USB is inserted\n", ISG5320A_TAG);
		else if (action == MUIC_NOTIFY_CMD_DETACH)
			pr_info("%s TA/USB is removed\n", ISG5320A_TAG);

		if (data->initialized == ON)
			isg5320a_force_calibration(data, true);
		else
			pr_info("%s not initialized\n", ISG5320A_TAG);

		break;
	default:
		break;
	}

	pr_info("%s dev=%d, action=%lu\n", ISG5320A_TAG, attached_dev, action);

	return NOTIFY_DONE;
}
#endif

static int isg5320a_parse_dt(struct isg5320a_data *data, struct device *dev)
{
	struct device_node *node = dev->of_node;
	enum of_gpio_flags flags;
	int ret;

	data->gpio_int = of_get_named_gpio_flags(node, "isg5320a_sub,irq-gpio", 0,
						 &flags);
	if (data->gpio_int < 0) {
		pr_err("%s get gpio_int error\n", ISG5320A_TAG);
		return -ENODEV;
	}

	pr_info("%s gpio_int:%d\n", ISG5320A_TAG, data->gpio_int);

	ret = of_property_read_u32(node, "isg5320a_sub,hallic_detect",
				   &data->hallic_detect);
	if (ret < 0)
		data->hallic_detect = 0;

	ret = of_property_read_u32(node, "isg5320a_sub,hallic_cert_detect",
				   &data->hallic_cert_detect);
	if (ret < 0)
		data->hallic_cert_detect = 0;

	return 0;
}

static int isg5320a_gpio_init(struct isg5320a_data *data)
{
	int ret = 0;

	ret = gpio_request(data->gpio_int, "isg5320a_irq");
	if (ret < 0) {
		pr_err("%s gpio %d request failed\n", ISG5320A_TAG, data->gpio_int);
		return ret;
	}

	ret = gpio_direction_input(data->gpio_int);
	if (ret < 0) {
		pr_err("%s failed to set gpio %d(%d)\n", ISG5320A_TAG, data->gpio_int,
		       ret);
		gpio_free(data->gpio_int);
		return ret;
	}

	return ret;
}

static int isg5320a_probe(struct i2c_client *client,
			  const struct i2c_device_id *id)
{
	int ret = -ENODEV;
	struct isg5320a_data *data;
	struct input_dev *input_dev;

	pr_info("%s ### %s probe ###\n", ISG5320A_TAG, DEVICE_NAME);

	if (!i2c_check_functionality(client->adapter, I2C_FUNC_I2C)) {
		pr_err("%s i2c_check_functionality error\n", ISG5320A_TAG);
		goto err;
	}

	data = kzalloc(sizeof(struct isg5320a_data), GFP_KERNEL);
	if (!data) {
		pr_err("%s failed to allocate memory\n", ISG5320A_TAG);
		goto err_kzalloc;
	}

	ret = isg5320a_parse_dt(data, &client->dev);
	if (ret) {
		pr_err("%s failed to parse dt\n", ISG5320A_TAG);
		goto err_parse_dt;
	}

	ret = isg5320a_gpio_init(data);
	if (ret) {
		pr_err("%s failed to init sys\n", ISG5320A_TAG);
		goto err_gpio_init;
	}

	data->client = client;
	i2c_set_clientdata(client, data);

	input_dev = input_allocate_device();
	if (!input_dev) {
		pr_err("%s input_allocate_device failed\n", ISG5320A_TAG);
		goto err_input_alloc;
	}

	data->dev = &client->dev;
	data->input_dev = input_dev;

	input_dev->name = MODULE_NAME;
	input_dev->id.bustype = BUS_I2C;

	input_set_capability(input_dev, EV_REL, REL_MISC);
	input_set_capability(input_dev, EV_REL, REL_MAX);
	input_set_drvdata(input_dev, data);

	ret = isg5320a_reset(data);
	if (ret < 0) {
		pr_err("%s IMAGIS reset failed\n", ISG5320A_TAG);
		goto err_soft_reset;
	}

	data->skip_data = false;
	data->state = FAR;
	data->enable = OFF;
	data->initialized = OFF;
	data->debug_cnt = 0;
	data->normal_th = 0;
	data->fine_coarse = 0;
	data->cfcal_th = ISG5320A_RESET_CONDITION;
	data->bfcal_chk_ready = false;
	data->bfcal_chk_start = false;
	data->bfcal_chk_count = 0;
	data->hall_flag = 1;
	data->hall_cert_flag = 1;
	data->debug_cdc[0] = 0;
	data->debug_cdc[1] = 0;
	data->debug_cdc[2] = 0;
	data->debug_base[0] = 0;
	data->debug_base[1] = 0;
	data->debug_diff[0] = 0;
	data->debug_diff[1] = 0;

	client->irq = gpio_to_irq(data->gpio_int);
	ret = request_threaded_irq(client->irq, NULL, isg5320a_irq_thread,
				   IRQF_TRIGGER_FALLING | IRQF_ONESHOT, DEVICE_NAME, data);
	if (ret < 0) {
		pr_err("%s failed to register interrupt\n", ISG5320A_TAG);
		goto err_irq;
	}
	disable_irq(client->irq);
	mutex_init(&data->lock);

	ret = input_register_device(input_dev);
	if (ret) {
		input_free_device(input_dev);
		pr_err("%s failed to register input dev (%d)\n", ISG5320A_TAG, ret);
		goto err_register_input_dev;
	}

	ret = sensors_create_symlink(input_dev);
	if (ret < 0) {
		pr_err("%s failed to create symlink (%d)\n", ISG5320A_TAG, ret);
		goto err_create_symlink;
	}

	ret = sysfs_create_group(&input_dev->dev.kobj, &isg5320a_attribute_group);
	if (ret < 0) {
		pr_err("%s failed to create sysfs group (%d)\n", ISG5320A_TAG, ret);
		goto err_sysfs_create_group;
	}

	ret = sensors_register(data->dev, data, sensor_attrs, MODULE_NAME);
	if (ret) {
		pr_err("%s could not register sensor(%d).\n", ISG5320A_TAG, ret);
		goto err_sensor_register;
	}

	data->grip_ws = wakeup_source_register(&client->dev, "grip_wake_lock");
	INIT_DELAYED_WORK(&data->debug_work, debug_work_func);
	INIT_DELAYED_WORK(&data->cal_work, cal_work_func);
#ifdef ISG5320A_INIT_DELAYEDWORK
	INIT_DELAYED_WORK(&data->init_work, init_work_func);
	schedule_delayed_work(&data->init_work, msecs_to_jiffies(300));
#else
	isg5320a_initialize(data);
	isg5320a_set_mode(data, ISG5320A_MODE_NORMAL);
	isg5320a_set_debug_work(data, ON, 2000);
#endif

#if IS_ENABLED(CONFIG_PDIC_NOTIFIER) && IS_ENABLED(CONFIG_USB_TYPEC_MANAGER_NOTIFIER)
	manager_notifier_register(&data->cpuidle_ccic_nb,
				  isg5320a_ccic_handle_notification,
				  MANAGER_NOTIFY_PDIC_SENSORHUB);
#elif IS_ENABLED(CONFIG_CCIC_NOTIFIER) && IS_ENABLED(CONFIG_USB_TYPEC_MANAGER_NOTIFIER)
	manager_notifier_register(&data->cpuidle_ccic_nb,
				  isg5320a_ccic_handle_notification,
				  MANAGER_NOTIFY_CCIC_SENSORHUB);
#elif IS_ENABLED(CONFIG_MUIC_NOTIFIER)
	muic_notifier_register(&data->cpuidle_muic_nb,
			       isg5320a_cpuidle_muic_notifier, MUIC_NOTIFY_DEV_CPUIDLE);
#endif

	pr_info("%s ### IMAGIS probe done ###\n", ISG5320A_TAG);

	return 0;

err_sensor_register:
	sysfs_remove_group(&input_dev->dev.kobj, &isg5320a_attribute_group);
err_sysfs_create_group:
	sensors_remove_symlink(input_dev);
err_create_symlink:
	input_unregister_device(input_dev);
err_register_input_dev:
	mutex_destroy(&data->lock);
	free_irq(client->irq, data);
err_irq:
err_soft_reset:
err_input_alloc:
	gpio_free(data->gpio_int);
err_gpio_init:
err_parse_dt:
	kfree(data);
err_kzalloc:
err:
	pr_info("%s ### IMAGIS probe failed ###\n", ISG5320A_TAG);

	return -ENODEV;
}

static int isg5320a_remove(struct i2c_client *client)
{
	struct isg5320a_data *data = i2c_get_clientdata(client);

	pr_info("%s %s\n", ISG5320A_TAG, __func__);

	isg5320a_set_debug_work(data, OFF, 0);
	if (data->enable == ON)
		isg5320a_set_enable(data, OFF);
	isg5320a_set_mode(data, ISG5320A_MODE_SLEEP);

	free_irq(client->irq, data);
	gpio_free(data->gpio_int);

	wakeup_source_unregister(data->grip_ws);
	sensors_unregister(data->dev, sensor_attrs);
	sensors_remove_symlink(data->input_dev);
	sysfs_remove_group(&data->input_dev->dev.kobj, &isg5320a_attribute_group);
	input_unregister_device(data->input_dev);
	mutex_destroy(&data->lock);

	kfree(data);

	return 0;
}

static int isg5320a_suspend(struct device *dev)
{
	struct isg5320a_data *data = dev_get_drvdata(dev);

	pr_info("%s %s\n", ISG5320A_TAG, __func__);
	isg5320a_set_debug_work(data, OFF, 0);

	return 0;
}

static int isg5320a_resume(struct device *dev)
{
	struct isg5320a_data *data = dev_get_drvdata(dev);

	pr_info("%s %s\n", ISG5320A_TAG, __func__);
	isg5320a_set_debug_work(data, ON, 1000);

	return 0;
}

static void isg5320a_shutdown(struct i2c_client *client)
{
	struct isg5320a_data *data = i2c_get_clientdata(client);

	pr_info("%s %s\n", ISG5320A_TAG, __func__);

	isg5320a_set_debug_work(data, OFF, 0);
	if (data->enable == ON)
		isg5320a_set_enable(data, OFF);
	isg5320a_set_mode(data, ISG5320A_MODE_SLEEP);
}

static const struct of_device_id isg5320a_match_table[] = {
	{ .compatible = "isg5320a_sub", },
	{ },
};

static struct i2c_device_id isg5320a_id_table[] = {
	{ DEVICE_NAME, 0 },
	{ },
};
MODULE_DEVICE_TABLE(i2c, isg5320a_id_table);

static const struct dev_pm_ops isg5320a_pm_ops = {
	.suspend = isg5320a_suspend,
	.resume = isg5320a_resume,
};

static struct i2c_driver isg5320a_driver = {
	.driver = {
		.name = DEVICE_NAME,
		.owner = THIS_MODULE,
		.of_match_table = isg5320a_match_table,
		.pm = &isg5320a_pm_ops,
	},
	.id_table = isg5320a_id_table,
	.probe = isg5320a_probe,
	.remove = isg5320a_remove,
	.shutdown = isg5320a_shutdown,
};

static int __init isg5320a_init(void)
{
	return i2c_add_driver(&isg5320a_driver);
}

static void __exit isg5320a_exit(void)
{
	i2c_del_driver(&isg5320a_driver);
}

module_init(isg5320a_init);
module_exit(isg5320a_exit);

MODULE_DESCRIPTION("Imagis Grip Sensor driver");
MODULE_LICENSE("GPL");
