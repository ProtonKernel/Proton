/* drivers/input/sec_input/synaptics/synaptics_core.c
 *
 * Copyright (C) 2020 Samsung Electronics Co., Ltd.
 *
 * Core file for Samsung TSC driver
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#include "synaptics_dev.h"
#include "synaptics_reg.h"

#if IS_ENABLED(CONFIG_INPUT_SEC_SECURE_TOUCH)
irqreturn_t synaptics_secure_filter_interrupt(struct synaptics_ts_data *ts)
{
	if (atomic_read(&ts->secure_enabled) == SECURE_TOUCH_ENABLE) {
		if (atomic_cmpxchg(&ts->secure_pending_irqs, 0, 1) == 0) {
			sysfs_notify(&ts->plat_data->input_dev->dev.kobj, NULL, "secure_touch");

		} else {
			input_info(true, &ts->client->dev, "%s: pending irq:%d\n",
					__func__, (int)atomic_read(&ts->secure_pending_irqs));
		}

		return IRQ_HANDLED;
	}

	return IRQ_NONE;
}

/**
 * Sysfs attr group for secure touch & interrupt handler for Secure world.
 * @atomic : syncronization for secure_enabled
 * @pm_runtime : set rpm_resume or rpm_ilde
 */
static ssize_t secure_touch_enable_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct synaptics_ts_data *ts = dev_get_drvdata(dev);

	return snprintf(buf, PAGE_SIZE, "%d", atomic_read(&ts->secure_enabled));
}

static ssize_t secure_touch_enable_store(struct device *dev,
		struct device_attribute *addr, const char *buf, size_t count)
{
	struct synaptics_ts_data *ts = dev_get_drvdata(dev);
	int ret;
	unsigned long data;

	if (count > 2) {
		input_err(true, &ts->client->dev,
				"%s: cmd length is over (%s,%d)!!\n",
				__func__, buf, (int)strlen(buf));
		return -EINVAL;
	}

	ret = kstrtoul(buf, 10, &data);
	if (ret != 0) {
		input_err(true, &ts->client->dev, "%s: failed to read:%d\n",
				__func__, ret);
		return -EINVAL;
	}

	if (data == 1) {
		if (ts->reset_is_on_going) {
			input_err(true, &ts->client->dev, "%s: reset is on going because i2c fail\n", __func__);
			return -EBUSY;
		}

		/* Enable Secure World */
		if (atomic_read(&ts->secure_enabled) == SECURE_TOUCH_ENABLE) {
			input_err(true, &ts->client->dev, "%s: already enabled\n", __func__);
			return -EBUSY;
		}

		sec_delay(200);
		
		/* syncronize_irq -> disable_irq + enable_irq
		 * concern about timing issue.
		 */
		disable_irq(ts->client->irq);

		/* Release All Finger */
		synaptics_ts_release_all_finger(ts);

		if (pm_runtime_get_sync(ts->client->adapter->dev.parent) < 0) {
			enable_irq(ts->client->irq);
			input_err(true, &ts->client->dev, "%s: failed to get pm_runtime\n", __func__);
			return -EIO;
		}

#if IS_ENABLED(CONFIG_INPUT_SEC_NOTIFIER)
		sec_input_notify(&ts->synaptics_input_nb, NOTIFIER_SECURE_TOUCH_ENABLE, NULL);
#endif
		reinit_completion(&ts->secure_powerdown);
		reinit_completion(&ts->secure_interrupt);

		atomic_set(&ts->secure_enabled, 1);
		atomic_set(&ts->secure_pending_irqs, 0);

		enable_irq(ts->client->irq);

		input_info(true, &ts->client->dev, "%s: secure touch enable\n", __func__);
	} else if (data == 0) {
		/* Disable Secure World */
		if (atomic_read(&ts->secure_enabled) == SECURE_TOUCH_DISABLE) {
			input_err(true, &ts->client->dev, "%s: already disabled\n", __func__);
			return count;
		}

		sec_delay(200);

		pm_runtime_put_sync(ts->client->adapter->dev.parent);
		atomic_set(&ts->secure_enabled, 0);

		sysfs_notify(&ts->plat_data->input_dev->dev.kobj, NULL, "secure_touch");

		sec_delay(10);

		synaptics_ts_irq_thread(ts->client->irq, ts);
		complete(&ts->secure_interrupt);
		complete(&ts->secure_powerdown);

		input_info(true, &ts->client->dev, "%s: secure touch disable\n", __func__);

#if IS_ENABLED(CONFIG_INPUT_SEC_NOTIFIER)
		sec_input_notify(&ts->synaptics_input_nb, NOTIFIER_SECURE_TOUCH_DISABLE, NULL);
#endif
	} else {
		input_err(true, &ts->client->dev, "%s: unsupport value:%ld\n", __func__, data);
		return -EINVAL;
	}

	return count;
}

static ssize_t secure_touch_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct synaptics_ts_data *ts = dev_get_drvdata(dev);
	int val = 0;

	if (atomic_read(&ts->secure_enabled) == SECURE_TOUCH_DISABLE) {
		input_err(true, &ts->client->dev, "%s: disabled\n", __func__);
		return -EBADF;
	}

	if (atomic_cmpxchg(&ts->secure_pending_irqs, -1, 0) == -1) {
		input_err(true, &ts->client->dev, "%s: pending irq -1\n", __func__);
		return -EINVAL;
	}

	if (atomic_cmpxchg(&ts->secure_pending_irqs, 1, 0) == 1) {
		val = 1;
		input_err(true, &ts->client->dev, "%s: pending irq is %d\n",
				__func__, atomic_read(&ts->secure_pending_irqs));
	}

	complete(&ts->secure_interrupt);

	return snprintf(buf, PAGE_SIZE, "%u", val);
}

static ssize_t secure_ownership_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	return snprintf(buf, PAGE_SIZE, "1");
}

static int secure_touch_init(struct synaptics_ts_data *ts)
{
	input_info(true, &ts->client->dev, "%s\n", __func__);

	init_completion(&ts->secure_interrupt);
	init_completion(&ts->secure_powerdown);

	return 0;
}

static void secure_touch_stop(struct synaptics_ts_data *ts, bool stop)
{
	if (atomic_read(&ts->secure_enabled)) {
		atomic_set(&ts->secure_pending_irqs, -1);

		sysfs_notify(&ts->plat_data->input_dev->dev.kobj, NULL, "secure_touch");

		if (stop)
			wait_for_completion_interruptible(&ts->secure_powerdown);

		input_info(true, &ts->client->dev, "%s: %d\n", __func__, stop);
	}
}

static DEVICE_ATTR(secure_touch_enable, (S_IRUGO | S_IWUSR | S_IWGRP),
		secure_touch_enable_show, secure_touch_enable_store);
static DEVICE_ATTR(secure_touch, S_IRUGO, secure_touch_show, NULL);
static DEVICE_ATTR(secure_ownership, S_IRUGO, secure_ownership_show, NULL);
static struct attribute *secure_attr[] = {
	&dev_attr_secure_touch_enable.attr,
	&dev_attr_secure_touch.attr,
	&dev_attr_secure_ownership.attr,
	NULL,
};

static struct attribute_group secure_attr_group = {
	.attrs = secure_attr,
};
#endif

#if IS_ENABLED(CONFIG_SAMSUNG_TUI)
extern int stui_i2c_lock(struct i2c_adapter *adap);
extern int stui_i2c_unlock(struct i2c_adapter *adap);

int synaptics_stui_tsp_enter(void)
{
	struct synaptics_ts_data *ts = dev_get_drvdata(ptsp);
	int ret = 0;

	if (!ts)
		return -EINVAL;

#if IS_ENABLED(CONFIG_INPUT_SEC_NOTIFIER)
	sec_input_notify(&ts->synaptics_input_nb, NOTIFIER_SECURE_TOUCH_ENABLE, NULL);
#endif

	disable_irq(ts->irq);
	synaptics_ts_release_all_finger(ts);

	ret = stui_i2c_lock(ts->client->adapter);
	if (ret) {
		pr_err("[STUI] stui_i2c_lock failed : %d\n", ret);
#if IS_ENABLED(CONFIG_INPUT_SEC_NOTIFIER)
		sec_input_notify(&ts->synaptics_input_nb, NOTIFIER_SECURE_TOUCH_DISABLE, NULL);
#endif
		enable_irq(ts->irq);
		return -1;
	}

	return 0;
}

int synaptics_stui_tsp_exit(void)
{
	struct synaptics_ts_data *ts = dev_get_drvdata(ptsp);
	int ret = 0;

	if (!ts)
		return -EINVAL;

	ret = stui_i2c_unlock(ts->client->adapter);
	if (ret)
		pr_err("[STUI] stui_i2c_unlock failed : %d\n", ret);

	enable_irq(ts->irq);

#if IS_ENABLED(CONFIG_INPUT_SEC_NOTIFIER)
	sec_input_notify(&ts->synaptics_input_nb, NOTIFIER_SECURE_TOUCH_DISABLE, NULL);
#endif

	return ret;
}

int synaptics_stui_tsp_type(void)
{
	return STUI_TSP_TYPE_SNAPTICS;
}
#endif

int synaptics_ts_i2c_write(struct synaptics_ts_data *ts, u8 *reg, int cnum, u8 *data, int len)
{
	int ret;
	unsigned char retry;
	struct i2c_msg msg;
	int i;
	const int len_max = 0xffff;
	u8 *buf;
	u8 *buff;

	if (len + 1 > len_max) {
		input_err(true, &ts->client->dev,
				"%s: The i2c buffer size is exceeded.\n", __func__);
		return -ENOMEM;
	}

	if (!ts->plat_data->resume_done.done) {
		ret = wait_for_completion_interruptible_timeout(&ts->plat_data->resume_done, msecs_to_jiffies(500));
		if (ret <= 0) {
			input_err(true, &ts->client->dev, "%s: LPM: pm resume is not handled:%d\n", __func__, ret);
			return -EIO;
		}
	}

#if IS_ENABLED(CONFIG_INPUT_SEC_SECURE_TOUCH)
	if (atomic_read(&ts->secure_enabled) == SECURE_TOUCH_ENABLE) {
		input_err(true, &ts->client->dev,
				"%s: TSP no accessible from Linux, TUI is enabled!\n", __func__);
		return -EBUSY;
	}
#endif
#if IS_ENABLED(CONFIG_SAMSUNG_TUI)
	if (STUI_MODE_TOUCH_SEC & stui_get_mode())
		return -EBUSY;
#endif
	buf = kzalloc(cnum, GFP_KERNEL);
	if (!buf)
		return -ENOMEM;

	memcpy(buf, reg, cnum);

	buff = kzalloc(len + cnum, GFP_KERNEL);
	if (!buff) {
		kfree(buf);
		return -ENOMEM;
	}

	if (ts->plat_data->power_state == SEC_INPUT_STATE_POWER_OFF) {
		input_err(true, &ts->client->dev, "%s: POWER_STATUS : OFF\n", __func__);
		goto err;
	}

	memcpy(buff, buf, cnum);
	memcpy(buff + cnum, data, len);

	msg.addr = ts->client->addr;
	msg.flags = 0 | I2C_M_DMA_SAFE;
	msg.len = len + cnum;
	msg.buf = buff;

	mutex_lock(&ts->i2c_mutex);
	for (retry = 0; retry < SYNAPTICS_TS_I2C_RETRY_CNT; retry++) {
		ret = i2c_transfer(ts->client->adapter, &msg, 1);
		if (ret == 1)
			break;

		if (ts->plat_data->power_state == SEC_INPUT_STATE_POWER_OFF) {
			input_err(true, &ts->client->dev, "%s: POWER_STATUS : OFF, retry:%d\n", __func__, retry);
			mutex_unlock(&ts->i2c_mutex);
			goto err;
		}

		sec_delay(20);
	}

	mutex_unlock(&ts->i2c_mutex);

	if (retry == SYNAPTICS_TS_I2C_RETRY_CNT) {
		char result[32];
		input_err(true, &ts->client->dev, "%s: I2C write over retry limit retry:%d\n", __func__, retry);
		ts->plat_data->hw_param.comm_err_count++;

		snprintf(result, sizeof(result), "RESULT=I2C");
		sec_cmd_send_event_to_user(&ts->sec, NULL, result);

		ret = -EIO;
		if (ts->probe_done && !ts->reset_is_on_going && !ts->plat_data->shutdown_called)
			schedule_delayed_work(&ts->reset_work, msecs_to_jiffies(TOUCH_RESET_DWORK_TIME));
	}

	if (ts->debug_flag & SEC_TS_DEBUG_PRINT_WRITE_CMD) {
		pr_info("sec_input:i2c_cmd: W:");
		for (i = 0; i < cnum; i++)
			pr_cont("%02X ", reg[i]);
		pr_cont(" | ");
		for (i = 0; i < len; i++)
			pr_cont("%02X ", data[i]);
		pr_cont("\n");
	}

	if (ret == 1) {
		kfree(buf);
		kfree(buff);
		return 0;
	}
err:
	kfree(buf);
	kfree(buff);
	return -EIO;
}

/**
 * syna_i2c_read()
 *
 * TouchCom over I2C uses the normal I2C addressing and transaction direction
 * mechanisms to select the device and retrieve the data.
 *
 * @param
 *    [ in] hw_if:   the handle of hw interface
 *    [out] rd_data: buffer for storing data retrieved from device
 *    [ in] rd_len: number of bytes retrieved from device
 *
 * @return
 *    on success, 0; otherwise, negative value on error.
 */
int synaptics_ts_i2c_only_read(struct synaptics_ts_data *ts, u8 *data, int len)
{
	int ret;
	unsigned char retry;
	struct i2c_msg msg;
	int remain = len;
	int i;
	u8 *buff;

	if (!ts->plat_data->resume_done.done) {
		ret = wait_for_completion_interruptible_timeout(&ts->plat_data->resume_done, msecs_to_jiffies(500));
		if (ret <= 0) {
			input_err(true, &ts->client->dev, "%s: LPM: pm resume is not handled:%d\n", __func__, ret);
			return -EIO;
		}
	}

#if IS_ENABLED(CONFIG_INPUT_SEC_SECURE_TOUCH)
	if (atomic_read(&ts->secure_enabled) == SECURE_TOUCH_ENABLE) {
		input_err(true, &ts->client->dev,
				"%s: TSP no accessible from Linux, TUI is enabled!\n", __func__);
		return -EBUSY;
	}
#endif
#if IS_ENABLED(CONFIG_SAMSUNG_TUI)
	if (STUI_MODE_TOUCH_SEC & stui_get_mode())
		return -EBUSY;
#endif

	buff = kzalloc(len, GFP_KERNEL);
	if (!buff)
		return -ENOMEM;

	if (ts->plat_data->power_state == SEC_INPUT_STATE_POWER_OFF) {
		input_err(true, &ts->client->dev, "%s: POWER_STATUS : OFF\n", __func__);
		goto err;
	}

	msg.addr = ts->client->addr;
	msg.flags = I2C_M_RD | I2C_M_DMA_SAFE;
	msg.buf = buff;

	mutex_lock(&ts->i2c_mutex);
	if (len <= ts->plat_data->i2c_burstmax) {
		msg.len = len;
		for (retry = 0; retry < SYNAPTICS_TS_I2C_RETRY_CNT; retry++) {
			ret = i2c_transfer(ts->client->adapter, &msg, 1);
			if (ret == 1)
				break;
			sec_delay(20);
			if (ts->plat_data->power_state == SEC_INPUT_STATE_POWER_OFF) {
				input_err(true, &ts->client->dev, "%s: POWER_STATUS : OFF, retry:%d\n", __func__, retry);
				mutex_unlock(&ts->i2c_mutex);
				goto err;
			}
		}
	} else {
		/*
		 * I2C read buffer is 256 byte. do not support long buffer over than 256.
		 * So, try to seperate reading data about 256 bytes.
		 */
		for (retry = 0; retry < SYNAPTICS_TS_I2C_RETRY_CNT; retry++) {
			ret = i2c_transfer(ts->client->adapter, &msg, 1);
			if (ret == 1)
				break;
			sec_delay(20);
			if (ts->plat_data->power_state == SEC_INPUT_STATE_POWER_OFF) {
				input_err(true, &ts->client->dev, "%s: POWER_STATUS : OFF, retry:%d\n", __func__, retry);
				mutex_unlock(&ts->i2c_mutex);
				goto err;
			}
		}

		do {
			if (remain > ts->plat_data->i2c_burstmax)
				msg.len = ts->plat_data->i2c_burstmax;
			else
				msg.len = remain;

			remain -= ts->plat_data->i2c_burstmax;

			for (retry = 0; retry < SYNAPTICS_TS_I2C_RETRY_CNT; retry++) {
				ret = i2c_transfer(ts->client->adapter, &msg, 1);
				if (ret == 1)
					break;
				sec_delay(20);
				if (ts->plat_data->power_state == SEC_INPUT_STATE_POWER_OFF) {
					input_err(true, &ts->client->dev, "%s: POWER_STATUS : OFF, retry:%d\n", __func__, retry);
					mutex_unlock(&ts->i2c_mutex);
					goto err;
				}
			}
			msg.buf += msg.len;
		} while (remain > 0);
	}

	mutex_unlock(&ts->i2c_mutex);

	if (retry == SYNAPTICS_TS_I2C_RETRY_CNT) {
		char result[32];
		input_err(true, &ts->client->dev, "%s: I2C read over retry limit retry:%d\n", __func__, retry);
		ts->plat_data->hw_param.comm_err_count++;

		snprintf(result, sizeof(result), "RESULT=I2C");
		sec_cmd_send_event_to_user(&ts->sec, NULL, result);

		ret = -EIO;
		if (ts->probe_done && !ts->reset_is_on_going && !ts->plat_data->shutdown_called)
			schedule_delayed_work(&ts->reset_work, msecs_to_jiffies(TOUCH_RESET_DWORK_TIME));
	}

	memcpy(data, buff, len);
	if (ts->debug_flag & SEC_TS_DEBUG_PRINT_READ_CMD) {
		pr_info("sec_input:i2c_cmd: only read:");
		for (i = 0; i < len; i++)
			pr_cont("%02X ", data[i]);
		pr_cont("\n");
	}

	kfree(buff);
	return ret;
err:
	kfree(buff);
	return -EIO;
}


int synaptics_ts_i2c_read(struct synaptics_ts_data *ts, u8 *reg, int cnum, u8 *data, int len)
{
	int ret;
	unsigned char retry;
	struct i2c_msg msg[2];
	int remain = len;
	int i;
	u8 *buff;
	u8 *buf;

	if (!ts->plat_data->resume_done.done) {
		ret = wait_for_completion_interruptible_timeout(&ts->plat_data->resume_done, msecs_to_jiffies(500));
		if (ret <= 0) {
			input_err(true, &ts->client->dev, "%s: LPM: pm resume is not handled:%d\n", __func__, ret);
			return -EIO;
		}
	}

#if IS_ENABLED(CONFIG_INPUT_SEC_SECURE_TOUCH)
	if (atomic_read(&ts->secure_enabled) == SECURE_TOUCH_ENABLE) {
		input_err(true, &ts->client->dev,
				"%s: TSP no accessible from Linux, TUI is enabled!\n", __func__);
		return -EBUSY;
	}
#endif
#if IS_ENABLED(CONFIG_SAMSUNG_TUI)
	if (STUI_MODE_TOUCH_SEC & stui_get_mode())
		return -EBUSY;
#endif
	buf = kzalloc(cnum, GFP_KERNEL);
	if (!buf)
		return -ENOMEM;
	
	memcpy(buf, reg, cnum);

	buff = kzalloc(len, GFP_KERNEL);
	if (!buff) {
		kfree(buf);
		return -ENOMEM;
	}

	if (ts->plat_data->power_state == SEC_INPUT_STATE_POWER_OFF) {
		input_err(true, &ts->client->dev, "%s: POWER_STATUS : OFF\n", __func__);
		goto err;
	}

	msg[0].addr = ts->client->addr;
	msg[0].flags = 0 | I2C_M_DMA_SAFE;
	msg[0].len = cnum;
	msg[0].buf = buf;

	msg[1].addr = ts->client->addr;
	msg[1].flags = I2C_M_RD | I2C_M_DMA_SAFE;
	msg[1].buf = buff;

	mutex_lock(&ts->i2c_mutex);
	if (len <= ts->plat_data->i2c_burstmax) {
		msg[1].len = len;
		for (retry = 0; retry < SYNAPTICS_TS_I2C_RETRY_CNT; retry++) {
			ret = i2c_transfer(ts->client->adapter, msg, 2);
			if (ret == 2)
				break;
			sec_delay(20);
			if (ts->plat_data->power_state == SEC_INPUT_STATE_POWER_OFF) {
				input_err(true, &ts->client->dev, "%s: POWER_STATUS : OFF, retry:%d\n", __func__, retry);
				mutex_unlock(&ts->i2c_mutex);
				goto err;
			}
		}
	} else {
		/*
		 * I2C read buffer is 256 byte. do not support long buffer over than 256.
		 * So, try to seperate reading data about 256 bytes.
		 */
		for (retry = 0; retry < SYNAPTICS_TS_I2C_RETRY_CNT; retry++) {
			ret = i2c_transfer(ts->client->adapter, msg, 1);
			if (ret == 1)
				break;
			sec_delay(20);
			if (ts->plat_data->power_state == SEC_INPUT_STATE_POWER_OFF) {
				input_err(true, &ts->client->dev, "%s: POWER_STATUS : OFF, retry:%d\n", __func__, retry);
				mutex_unlock(&ts->i2c_mutex);
				goto err;
			}
		}

		do {
			if (remain > ts->plat_data->i2c_burstmax)
				msg[1].len = ts->plat_data->i2c_burstmax;
			else
				msg[1].len = remain;

			remain -= ts->plat_data->i2c_burstmax;

			for (retry = 0; retry < SYNAPTICS_TS_I2C_RETRY_CNT; retry++) {
				ret = i2c_transfer(ts->client->adapter, &msg[1], 1);
				if (ret == 1)
					break;
				sec_delay(20);
				if (ts->plat_data->power_state == SEC_INPUT_STATE_POWER_OFF) {
					input_err(true, &ts->client->dev, "%s: POWER_STATUS : OFF, retry:%d\n", __func__, retry);
					mutex_unlock(&ts->i2c_mutex);
					goto err;
				}
			}
			msg[1].buf += msg[1].len;
		} while (remain > 0);
	}

	mutex_unlock(&ts->i2c_mutex);

	if (retry == SYNAPTICS_TS_I2C_RETRY_CNT) {
		char result[32];
		input_err(true, &ts->client->dev, "%s: I2C read over retry limit retry:%d\n", __func__, retry);
		ts->plat_data->hw_param.comm_err_count++;

		snprintf(result, sizeof(result), "RESULT=I2C");
		sec_cmd_send_event_to_user(&ts->sec, NULL, result);

		ret = -EIO;
		if (ts->probe_done && !ts->reset_is_on_going && !ts->plat_data->shutdown_called)
			schedule_delayed_work(&ts->reset_work, msecs_to_jiffies(TOUCH_RESET_DWORK_TIME));
	}

	memcpy(data, buff, len);
	if (ts->debug_flag & SEC_TS_DEBUG_PRINT_READ_CMD) {
		pr_info("sec_input:i2c_cmd: R:");
		for (i = 0; i < cnum; i++)
			pr_cont("%02X ", reg[i]);
		pr_cont(" | ");
		for (i = 0; i < len; i++)
			pr_cont("%02X ", data[i]);
		pr_cont("\n");
	}

	kfree(buf);
	kfree(buff);
	return ret;
err:
	kfree(buf);
	kfree(buff);
	return -EIO;
}

void synaptics_ts_reinit(void *data) 
{
	struct synaptics_ts_data *ts = (struct synaptics_ts_data *)data;

/*	unsigned char address;
	unsigned char buf[4] = { 0, };

	address = 0x07;
	ret = ts->synaptics_ts_i2c_read(ts, &address, 1, buf, 4);
	if (ret < 0)
		input_err(true, &ts->client->dev, "%s: failed to get bare data\n", __func__);

	input_info(true, &ts->client->dev, "%s:baredata, %02x %02x %02x %02x\n", __func__,
			buf[0], buf[1], buf[2], buf[3]);

	ret = synaptics_ts_detect_protocol(ts, buf, (unsigned int)sizeof(buf));
	if (ret < 0)
		input_err(true, &ts->client->dev, "%s: fail to detect protocol\n", __func__);
*/
	/* synaptics_ts_set_up_app_fw(ts); */

	if (ts->do_set_up_report)
		synaptics_ts_set_up_report(ts);

	synaptics_ts_rezero(ts);

	input_info(true, &ts->client->dev,
		"%s: charger=0x%x, touch_functions=0x%x, Power mode=0x%x ic mode = %d\n",
		__func__, ts->plat_data->charger_flag, ts->plat_data->touch_functions, ts->plat_data->power_state, ts->dev_mode);

	ts->plat_data->touch_noise_status = 0;
	ts->plat_data->touch_pre_noise_status = 0;
	ts->plat_data->wet_mode = 0;
	/* buffer clear asking */
	synaptics_ts_release_all_finger(ts);

	if (ts->cover_closed)
		synaptics_ts_set_cover_type(ts, ts->cover_closed);

	synaptics_ts_set_custom_library(ts);
	synaptics_ts_set_press_property(ts);

	if (ts->plat_data->support_fod && ts->plat_data->fod_data.set_val)
		synaptics_ts_set_fod_rect(ts);

	/* Power mode */
	if (ts->plat_data->power_state == SEC_INPUT_STATE_LPM) {
		ts->plat_data->lpmode(ts, TO_LOWPOWER_MODE);
		sec_delay(50);
		if (ts->plat_data->lowpower_mode & SEC_TS_MODE_SPONGE_AOD)
			synaptics_ts_set_aod_rect(ts);
	} else {
		sec_input_set_grip_type(&ts->client->dev, ONLY_EDGE_HANDLER);

		//synaptics_ts_set_external_noise_mode(ts, EXT_NOISE_MODE_MAX);

//		if (ts->plat_data->touchable_area)
			//ret = synaptics_ts_set_touchable_area(ts);
	}

	if (ts->plat_data->ed_enable)
		synaptics_ts_ear_detect_enable(ts, ts->plat_data->ed_enable);
	if (ts->plat_data->pocket_mode)
		synaptics_ts_pocket_mode_enable(ts, ts->plat_data->pocket_mode);
	if (ts->plat_data->low_sensitivity_mode)
		synaptics_ts_low_sensitivity_mode_enable(ts, ts->plat_data->low_sensitivity_mode);
	if (ts->plat_data->charger_flag)
		synaptics_ts_set_charger_mode(&ts->client->dev, ts->plat_data->charger_flag);
}

/**
 * synaptics_ts_init_message_wrap()
 *
 * Initialize internal buffers and related structures for command processing.
 * The function must be called to prepare all essential structures for
 * command wrapper.
 *
 * @param
 *    [in] tcm_msg: message wrapper structure
 *
 * @return
 *    on success, 0 or positive value; otherwise, negative value on error.
 */
static int synaptics_ts_init_message_wrap(struct synaptics_ts_data *ts, struct synaptics_ts_message_data_blob *tcm_msg)
{
	/* initialize internal buffers */
	synaptics_ts_buf_init(&tcm_msg->in);
	synaptics_ts_buf_init(&tcm_msg->out);
	synaptics_ts_buf_init(&tcm_msg->temp);

	/* allocate the completion event for command processing */
	if (synaptics_ts_pal_completion_alloc(&tcm_msg->cmd_completion) < 0) {
		input_err(true, &ts->client->dev, "%s:Fail to allocate cmd completion event\n", __func__);
		return -EINVAL;
	}

	/* allocate the cmd_mutex for command protection */
	if (synaptics_ts_pal_mutex_alloc(&tcm_msg->cmd_mutex) < 0) {
		input_err(true, &ts->client->dev, "%s:Fail to allocate cmd_mutex\n", __func__);
		return -EINVAL;
	}

	/* allocate the rw_mutex for rw protection */
	if (synaptics_ts_pal_mutex_alloc(&tcm_msg->rw_mutex) < 0) {
		input_err(true, &ts->client->dev, "%s:Fail to allocate rw_mutex\n", __func__);
		return -EINVAL;
	}

	/* set default state of command_status  */
	ATOMIC_SET(tcm_msg->command_status, CMD_STATE_IDLE);

	/* allocate the internal buffer.in at first */
	synaptics_ts_buf_lock(&tcm_msg->in);

	if (synaptics_ts_buf_alloc(&tcm_msg->in, SYNAPTICS_TS_MESSAGE_HEADER_SIZE) < 0) {
		input_err(true, &ts->client->dev, "%s:Fail to allocate memory for buf.in (size = %d)\n", __func__,
			SYNAPTICS_TS_MESSAGE_HEADER_SIZE);
		tcm_msg->in.buf_size = 0;
		tcm_msg->in.data_length = 0;
		synaptics_ts_buf_unlock(&tcm_msg->in);
		return -EINVAL;
	}
	tcm_msg->in.buf_size = SYNAPTICS_TS_MESSAGE_HEADER_SIZE;

	synaptics_ts_buf_unlock(&tcm_msg->in);

	return 0;
}

/**
 * synaptics_ts_del_message_wrap()
 *
 * Remove message wrapper interface and internal buffers.
 * Call the function once the message wrapper is no longer needed.
 *
 * @param
 *    [in] tcm_msg: message wrapper structure
 *
 * @return
 *    none.
 */
static void synaptics_ts_del_message_wrap(struct synaptics_ts_message_data_blob *tcm_msg)
{
	/* release the mutex */
	synaptics_ts_pal_mutex_free(&tcm_msg->rw_mutex);
	synaptics_ts_pal_mutex_free(&tcm_msg->cmd_mutex);

	/* release the completion event */
	synaptics_ts_pal_completion_free(&tcm_msg->cmd_completion);

	/* release internal buffers  */
	synaptics_ts_buf_release(&tcm_msg->temp);
	synaptics_ts_buf_release(&tcm_msg->out);
	synaptics_ts_buf_release(&tcm_msg->in);
}

/**
 * synaptics_ts_allocate_device()
 *
 * Create the TouchCom core device handle.
 * This function must be called in order to allocate the main device handle,
 * structure synaptics_ts_dev, which will be passed to all other operations and
 * functions within the entire source code.
 *
 * Meanwhile, caller has to prepare specific synaptics_ts_hw_interface structure,
 * so that all the implemented functions can access hardware components
 * through synaptics_ts_hw_interface.
 *
 * @param
 *    [out] ptcm_dev_ptr: a pointer to the device handle returned
 *    [ in] hw_if:        hardware-specific data on target platform
 *
 * @return
 *    on success, 0 or positive value; otherwise, negative value on error.
 */
int synaptics_ts_allocate_device(struct synaptics_ts_data *ts)
{
	int retval = 0;

	ts->max_rd_size = RD_CHUNK_SIZE;
	ts->max_wr_size = WR_CHUNK_SIZE;

	ts->write_message = NULL;
	ts->read_message = NULL;
	ts->write_immediate_message = NULL;

	/* allocate internal buffers */
	synaptics_ts_buf_init(&ts->report_buf);
	synaptics_ts_buf_init(&ts->resp_buf);
	synaptics_ts_buf_init(&ts->external_buf);
	synaptics_ts_buf_init(&ts->touch_config);
	synaptics_ts_buf_init(&ts->event_data);

	/* initialize the command wrapper interface */
	retval = synaptics_ts_init_message_wrap(ts, &ts->msg_data);
	if (retval < 0) {
		input_err(true, &ts->client->dev, "%s: Fail to initialize command interface\n", __func__);
		goto err_init_message_wrap;
	}

	input_err(true, &ts->client->dev, "%s:TouchCom core module created, ver.: %d.%02d\n", __func__,
		(unsigned char)(SYNA_TCM_CORE_LIB_VERSION >> 8),
		(unsigned char)SYNA_TCM_CORE_LIB_VERSION & 0xff);

	input_err(true, &ts->client->dev, "%s:Capability: wr_chunk(%d), rd_chunk(%d)\n", __func__,
		ts->max_wr_size, ts->max_rd_size);

	return 0;

err_init_message_wrap:
	synaptics_ts_buf_release(&ts->touch_config);
	synaptics_ts_buf_release(&ts->external_buf);
	synaptics_ts_buf_release(&ts->report_buf);
	synaptics_ts_buf_release(&ts->resp_buf);
	synaptics_ts_buf_release(&ts->event_data);

	return retval;
}

/**
 * synaptics_ts_remove_device()
 *
 * Remove the TouchCom core device handler.
 * This function must be invoked when the device is no longer needed.
 *
 * @param
 *    [ in] tcm_dev: the device handle
 *
 * @return
 *    none.
 */
void synaptics_ts_remove_device(struct synaptics_ts_data *ts)
{
	if (!ts) {
		pr_err("%s%s: Invalid tcm device handle\n", SECLOG, __func__);
		return;
	}

	/* release the command interface */
	synaptics_ts_del_message_wrap(&ts->msg_data);

	/* release buffers */
	synaptics_ts_buf_release(&ts->touch_config);
	synaptics_ts_buf_release(&ts->external_buf);
	synaptics_ts_buf_release(&ts->report_buf);
	synaptics_ts_buf_release(&ts->resp_buf);
	synaptics_ts_buf_release(&ts->event_data);

	input_err(true, &ts->client->dev, "%s: Failtcm device handle removed\n", __func__);
}


/*
 * don't need it in interrupt handler in reality, but, need it in vendor IC for requesting vendor IC.
 * If you are requested additional i2c protocol in interrupt handler by vendor.
 * please add it in synaptics_ts_external_func.
 */
void synaptics_ts_external_func(struct synaptics_ts_data *ts)
{
	if (ts->support_immediate_cmd)
		sec_input_set_temperature(&ts->client->dev, SEC_INPUT_SET_TEMPERATURE_IN_IRQ);
}

int synaptics_ts_input_open(struct input_dev *dev)
{
	struct synaptics_ts_data *ts = input_get_drvdata(dev);
	int ret;

	if (!ts->probe_done) {
		input_err(true, &ts->client->dev, "%s: probe is not done yet\n", __func__);
		return 0;
	}

	cancel_delayed_work_sync(&ts->work_read_info);

	mutex_lock(&ts->modechange);

	ts->plat_data->enabled = true;
	ts->plat_data->prox_power_off = 0;

#if IS_ENABLED(CONFIG_INPUT_SEC_SECURE_TOUCH)
	secure_touch_stop(ts, 0);
#endif

	if (ts->plat_data->power_state == SEC_INPUT_STATE_LPM) {
		ts->plat_data->lpmode(ts, TO_TOUCH_MODE);
		sec_input_set_grip_type(&ts->client->dev, ONLY_EDGE_HANDLER);
		if (ts->plat_data->low_sensitivity_mode)
			synaptics_ts_low_sensitivity_mode_enable(ts, ts->plat_data->low_sensitivity_mode);
	} else {
		ret = ts->plat_data->start_device(ts);
		if (ret < 0)
			input_err(true, &ts->client->dev, "%s: Failed to start device\n", __func__);
	}

	sec_input_set_temperature(&ts->client->dev, SEC_INPUT_SET_TEMPERATURE_FORCE);

	mutex_unlock(&ts->modechange);

	cancel_delayed_work(&ts->work_print_info);
	ts->plat_data->print_info_cnt_open = 0;
	ts->plat_data->print_info_cnt_release = 0;
	if (!ts->plat_data->shutdown_called)
		schedule_work(&ts->work_print_info.work);
	return 0;
}

void synaptics_ts_input_close(struct input_dev *dev)
{
	struct synaptics_ts_data *ts = input_get_drvdata(dev);

	if (!ts->probe_done) {
		input_err(true, &ts->client->dev, "%s: probe is not done yet\n", __func__);
		return;
	}

	cancel_delayed_work_sync(&ts->work_read_info);

	if (ts->plat_data->shutdown_called) {
		input_err(true, &ts->client->dev, "%s shutdown was called\n", __func__);
		return;
	}

	mutex_lock(&ts->modechange);

	ts->plat_data->enabled = false;

#ifdef TCLM_CONCEPT
	sec_tclm_debug_info(ts->tdata);
#endif
	cancel_delayed_work(&ts->work_print_info);
	sec_input_print_info(&ts->client->dev, ts->tdata);
#if IS_ENABLED(CONFIG_INPUT_SEC_SECURE_TOUCH)
	secure_touch_stop(ts, 1);
#endif
#if IS_ENABLED(CONFIG_SAMSUNG_TUI)
	stui_cancel_session();
#endif

	cancel_delayed_work(&ts->reset_work);

	if (ts->plat_data->lowpower_mode || ts->plat_data->ed_enable || ts->plat_data->pocket_mode || ts->plat_data->fod_lp_mode)
		ts->plat_data->lpmode(ts, TO_LOWPOWER_MODE);
	else
		ts->plat_data->stop_device(ts);

	mutex_unlock(&ts->modechange);
}

int synaptics_ts_stop_device(void *data)
{
	struct synaptics_ts_data *ts = (struct synaptics_ts_data *)data;

	input_info(true, &ts->client->dev, "%s\n", __func__);

	mutex_lock(&ts->device_mutex);

	if (ts->plat_data->power_state == SEC_INPUT_STATE_POWER_OFF) {
		input_err(true, &ts->client->dev, "%s: already power off\n", __func__);
		goto out;
	}

	disable_irq(ts->irq);

	ts->plat_data->power_state = SEC_INPUT_STATE_POWER_OFF;

	synaptics_ts_locked_release_all_finger(ts);

	ts->plat_data->power(&ts->client->dev, false);
	ts->plat_data->pinctrl_configure(&ts->client->dev, false);

out:
	mutex_unlock(&ts->device_mutex);
	return 0;
}

int synaptics_ts_start_device(void *data)
{
	struct synaptics_ts_data *ts = (struct synaptics_ts_data *)data;
	int ret = 0;
	unsigned char address;
	unsigned char buf[4] = { 0, };
	
	input_info(true, &ts->client->dev, "%s\n", __func__);

	ts->plat_data->pinctrl_configure(&ts->client->dev, true);

	mutex_lock(&ts->device_mutex);

	if (ts->plat_data->power_state == SEC_INPUT_STATE_POWER_ON) {
		input_err(true, &ts->client->dev, "%s: already power on\n", __func__);
		goto out;
	}

	synaptics_ts_locked_release_all_finger(ts);

	ts->plat_data->power(&ts->client->dev, true);

	sec_delay(TOUCH_POWER_ON_DWORK_TIME);

	ts->plat_data->power_state = SEC_INPUT_STATE_POWER_ON;
	ts->plat_data->touch_noise_status = 0;

	address = 0x07;
	ret = ts->synaptics_ts_i2c_read(ts, &address, 1, buf, 4);
	if (ret < 0)
		input_err(true, &ts->client->dev, "%s: failed to get bare data\n", __func__);

	input_info(true, &ts->client->dev, "%s:baredata, %02x %02x %02x %02x\n", __func__,
			buf[0], buf[1], buf[2], buf[3]);

	ret = synaptics_ts_detect_protocol(ts, buf, (unsigned int)sizeof(buf));
	if (ret < 0)
		input_err(true, &ts->client->dev, "%s: fail to detect protocol\n", __func__);

	enable_irq(ts->irq);

	ts->plat_data->init(ts);
out:
	mutex_unlock(&ts->device_mutex);
	return ret;
}

static int synaptics_ts_hw_init(struct i2c_client *client)
{
	struct synaptics_ts_data *ts = i2c_get_clientdata(client);
	int ret = 0;
	u8 address;
	u8 data[4] = { 0, };

	ret = synaptics_ts_allocate_device(ts);
	if (ret < 0) {
		input_err(true, &ts->client->dev, "%s: Fail to allocate TouchCom device handle\n", __func__);
		goto err_allocate_cdev;
	}

	ts->plat_data->pinctrl_configure(&ts->client->dev, true);

	ts->plat_data->power(&ts->client->dev, true);
	if (!ts->plat_data->regulator_boot_on)
		sec_delay(TOUCH_POWER_ON_DWORK_TIME);

	ts->plat_data->power_state = SEC_INPUT_STATE_POWER_ON;

	address = 0x07;
	ret = ts->synaptics_ts_i2c_read(ts, &address, 1, data, 4);
	if (ret < 0) {
		input_err(true, &ts->client->dev, "%s: failed to get bare data\n", __func__);
		goto err_hw_init;
	}

	input_info(true, &ts->client->dev, "%s:baredata, %02x %02x %02x %02x\n", __func__,
			data[0], data[1], data[2], data[3]);

	ret = synaptics_ts_detect_protocol(ts, data, (unsigned int)sizeof(data));
	if (ret < 0) {
		input_err(true, &ts->client->dev, "%s: fail to detect protocol\n", __func__);
		goto err_hw_init;
	}
	
	/* check the running mode */
	switch (ts->dev_mode) {
	case SYNAPTICS_TS_MODE_APPLICATION_FIRMWARE:
		input_info(true, &ts->client->dev, "%s:Device in Application FW, build id: %d, %s\n", __func__,
			ts->packrat_number,
			ts->id_info.part_number);
		break;
	case SYNAPTICS_TS_MODE_BOOTLOADER:
	case SYNAPTICS_TS_MODE_TDDI_BOOTLOADER:
		input_info(true, &ts->client->dev, "%s:Device in Bootloader\n", __func__);
		break;
	case SYNAPTICS_TS_MODE_ROMBOOTLOADER:
		input_info(true, &ts->client->dev, "%s:Device in ROMBoot uBL\n", __func__);
		break;
	case SYNAPTICS_TS_MODE_MULTICHIP_TDDI_BOOTLOADER:
		input_info(true, &ts->client->dev, "%s:Device in multi-chip TDDI Bootloader\n", __func__);
		break;
	default:
		input_err(true, &ts->client->dev, "%s:Found TouchCom device, but unsupported mode: 0x%02x\n", __func__,
			ts->dev_mode);
		break;
	}

	ret = synaptics_ts_set_up_app_fw(ts);
	if (ret < 0)
		input_err(true, &ts->client->dev, "%s: Fail to set up application firmware\n", __func__);

	ret = synaptics_ts_fw_update_on_probe(ts);
	if (ret < 0) {
		input_err(true, &ts->client->dev, "%s: failed to synaptics_ts_fw_update_on_probe\n", __func__);
		goto err_hw_init;
	}

	if (ts->dev_mode != SYNAPTICS_TS_MODE_APPLICATION_FIRMWARE) {
		input_err(true, &ts->client->dev, "%s: Not app mode mode: 0x%02x\n", __func__,
			ts->dev_mode);
		goto err_hw_init;
	}

	if (ts->rows && ts->cols) {
		ts->pFrame = devm_kzalloc(&client->dev, ts->rows * ts->cols * sizeof(int), GFP_KERNEL);
		if (!ts->pFrame)
			return -ENOMEM;

//		slsi_ts_init_proc(ts);
	} else {
		input_err(true, &ts->client->dev, "%s: fail to alloc pFrame nTX:%d, nRX:%d\n",
			__func__, ts->rows, ts->cols);
		return -ENOMEM;
	}

err_hw_init:
err_allocate_cdev:
	return ret;

}

static void synaptics_ts_parse_dt(struct device *dev, struct synaptics_ts_data *ts)
{
	struct device_node *np = dev->of_node;

	if (!np) {
		input_err(true, dev, "%s: of_node is not exist\n", __func__);
		return;
	}

	ts->support_immediate_cmd = of_property_read_bool(np, "synaptics,support_immediate_cmd");
	ts->do_set_up_report = of_property_read_bool(np, "synaptics,do_set_up_report");
	if (of_property_read_u32(np, "synaptics,edgehandler_direction_max", &ts->edgehandler_direction_max))
		ts->edgehandler_direction_max = 3;

	input_info(true, dev, "%s: support_immediate_cmd:%d, do_set_up_report:%d, edgehandler_direction_max:%d\n",
			__func__, ts->support_immediate_cmd, ts->do_set_up_report, ts->edgehandler_direction_max);
}

static int synaptics_ts_init(struct i2c_client *client)
{
	struct synaptics_ts_data *ts;
	struct sec_ts_plat_data *pdata;
	struct sec_tclm_data *tdata = NULL;
	int ret = 0;

	if (client->dev.of_node) {
		pdata = devm_kzalloc(&client->dev,
				sizeof(struct sec_ts_plat_data), GFP_KERNEL);

		if (!pdata) {
			ret = -ENOMEM;
			goto error_allocate_pdata;
		}

		client->dev.platform_data = pdata;

		ret = sec_input_parse_dt(&client->dev);
		if (ret) {
			input_err(true, &client->dev, "%s: Failed to parse dt\n", __func__);
			goto error_allocate_mem;
		}
		tdata = devm_kzalloc(&client->dev,
				sizeof(struct sec_tclm_data), GFP_KERNEL);
		if (!tdata) {
			ret = -ENOMEM;
			goto error_allocate_tdata;
		}

#ifdef TCLM_CONCEPT
		sec_tclm_parse_dt(&client->dev, tdata);
#endif
	} else {
		pdata = client->dev.platform_data;
		if (!pdata) {
			ret = -ENOMEM;
			input_err(true, &client->dev, "%s: No platform data found\n", __func__);
			goto error_allocate_pdata;
		}
	}

	pdata->pinctrl = devm_pinctrl_get(&client->dev);
	if (IS_ERR(pdata->pinctrl))
		input_err(true, &client->dev, "%s: could not get pinctrl\n", __func__);

	ts = devm_kzalloc(&client->dev, sizeof(struct synaptics_ts_data), GFP_KERNEL);
	if (!ts) {
		ret = -ENOMEM;
		goto error_allocate_mem;
	}

	synaptics_ts_parse_dt(&client->dev, ts);

#if !IS_ENABLED(CONFIG_SAMSUNG_PRODUCT_SHIP)
	ts->vendor_data = devm_kzalloc(&client->dev, sizeof(struct synaptics_ts_sysfs), GFP_KERNEL);
	if (!ts->vendor_data) {
		ret = -ENOMEM;
		goto error_allocate_mem;
	}
#endif

	client->irq = gpio_to_irq(pdata->irq_gpio);

	ts->client = client;
	ts->plat_data = pdata;
	ts->irq = client->irq;
	ts->synaptics_ts_i2c_read = synaptics_ts_i2c_read;
	ts->synaptics_ts_i2c_write = synaptics_ts_i2c_write;
	ts->synaptics_ts_read_sponge = synaptics_ts_read_from_sponge;
	ts->synaptics_ts_write_sponge = synaptics_ts_write_to_sponge;

	ts->plat_data->pinctrl_configure = sec_input_pinctrl_configure;
	ts->plat_data->power = sec_input_power;
	ts->plat_data->start_device = synaptics_ts_start_device;
	ts->plat_data->stop_device = synaptics_ts_stop_device;
	ts->plat_data->init = synaptics_ts_reinit;
	ts->plat_data->lpmode = synaptics_ts_set_lowpowermode;
	ts->plat_data->set_grip_data = synaptics_set_grip_data_to_ic;
	if (!ts->plat_data->not_support_temp_noti)
		ts->plat_data->set_temperature = synaptics_ts_set_temperature;
	if (ts->plat_data->support_vbus_notifier)
		ts->plat_data->set_charger_mode = synaptics_ts_set_charger_mode;
	ptsp = &client->dev;

#if IS_ENABLED(CONFIG_SAMSUNG_TUI)
	ts->plat_data->stui_tsp_enter = synaptics_stui_tsp_enter;
	ts->plat_data->stui_tsp_exit = synaptics_stui_tsp_exit;
	ts->plat_data->stui_tsp_type = synaptics_stui_tsp_type;
#endif

	ts->tdata = tdata;
	if (!ts->tdata) {
		ret = -ENOMEM;
		goto err_null_tdata;
	}

#ifdef TCLM_CONCEPT
	sec_tclm_initialize(ts->tdata);
	ts->tdata->client = ts->client;
	ts->tdata->tclm_read = synaptics_tclm_data_read;
	ts->tdata->tclm_write = synaptics_tclm_data_write;
	ts->tdata->tclm_execute_force_calibration = synaptics_ts_tclm_execute_force_calibration;
	ts->tdata->tclm_parse_dt = sec_tclm_parse_dt;
#endif

	INIT_DELAYED_WORK(&ts->reset_work, synaptics_ts_reset_work);
	INIT_DELAYED_WORK(&ts->work_read_info, synaptics_ts_read_info_work);
	INIT_DELAYED_WORK(&ts->work_print_info, synaptics_ts_print_info_work);
	INIT_DELAYED_WORK(&ts->work_read_functions, synaptics_ts_get_touch_function);
	mutex_init(&ts->device_mutex);
	mutex_init(&ts->i2c_mutex);
	mutex_init(&ts->eventlock);
	mutex_init(&ts->modechange);
	mutex_init(&ts->sponge_mutex);
	mutex_init(&ts->fn_mutex);

	ts->plat_data->sec_ws = wakeup_source_register(&ts->client->dev, "tsp");
	device_init_wakeup(&client->dev, true);

	init_completion(&ts->plat_data->resume_done);
	complete_all(&ts->plat_data->resume_done);

	i2c_set_clientdata(client, ts);

	ret = sec_input_device_register(&client->dev, ts);
	if (ret) {
		input_err(true, &ts->client->dev, "failed to register input device, %d\n", ret);
		goto err_register_input_device;
	}

	ts->plat_data->input_dev->open = synaptics_ts_input_open;
	ts->plat_data->input_dev->close = synaptics_ts_input_close;

	ret = synaptics_ts_fn_init(ts);
	if (ret) {
		input_err(true, &ts->client->dev, "%s: fail to init fn\n", __func__);
		goto err_fn_init;
	}

#if !IS_ENABLED(CONFIG_SAMSUNG_PRODUCT_SHIP)
	/* for vendor */
	/* create the device file and register to char device classes */
	ret = syna_cdev_create_sysfs(ts);
	if (ret < 0) {
		input_err(true, &ts->client->dev, "Fail to create the device sysfs\n");
		goto error_vendor_data;
	}
#endif

#if IS_ENABLED(CONFIG_INPUT_SEC_SECURE_TOUCH)
	if (sysfs_create_group(&ts->plat_data->input_dev->dev.kobj, &secure_attr_group) < 0)
		input_err(true, &ts->client->dev, "%s: do not make secure group\n", __func__);
	else
		secure_touch_init(ts);

	sec_secure_touch_register(ts, ts->plat_data->ss_touch_num, &ts->plat_data->input_dev->dev.kobj);
#endif
#if IS_ENABLED(CONFIG_TOUCHSCREEN_DUMP_MODE)
	dump_callbacks.inform_dump = synaptics_ts_dump_tsp_log;
	INIT_DELAYED_WORK(&ts->check_rawdata, synaptics_ts_check_rawdata);
#endif

	input_info(true, &client->dev, "%s: init resource\n", __func__);

	return 0;

#if !IS_ENABLED(CONFIG_SAMSUNG_PRODUCT_SHIP)
error_vendor_data:
	synaptics_ts_fn_remove(ts);
#endif
err_fn_init:
err_register_input_device:
	wakeup_source_unregister(ts->plat_data->sec_ws);
err_null_tdata:
error_allocate_mem:
	regulator_put(pdata->dvdd);
	regulator_put(pdata->avdd);
error_allocate_tdata:
error_allocate_pdata:
	input_err(true, &client->dev, "%s: failed(%d)\n", __func__, ret);
	input_log_fix();
	return ret;
}

void synaptics_ts_release(struct i2c_client *client)
{
	struct synaptics_ts_data *ts = i2c_get_clientdata(client);

	input_info(true, &ts->client->dev, "%s\n", __func__);

	cancel_delayed_work_sync(&ts->work_read_info);
	cancel_delayed_work_sync(&ts->work_print_info);
	cancel_delayed_work_sync(&ts->work_read_functions);
	cancel_delayed_work_sync(&ts->reset_work);
	flush_delayed_work(&ts->reset_work);
#if IS_ENABLED(CONFIG_TOUCHSCREEN_DUMP_MODE)
	cancel_delayed_work_sync(&ts->check_rawdata);
	dump_callbacks.inform_dump = NULL;
#endif
	synaptics_ts_fn_remove(ts);
#if !IS_ENABLED(CONFIG_SAMSUNG_PRODUCT_SHIP)
	syna_cdev_remove_sysfs(ts);
#endif
	device_init_wakeup(&client->dev, false);
	wakeup_source_unregister(ts->plat_data->sec_ws);

	ts->plat_data->lowpower_mode = false;
	ts->probe_done = false;

	ts->plat_data->power(&ts->client->dev, false);

	regulator_put(ts->plat_data->dvdd);
	regulator_put(ts->plat_data->avdd);
}


int synaptics_ts_probe(struct i2c_client *client, const struct i2c_device_id *id)
{
	struct synaptics_ts_data *ts;
	int ret = 0;

	input_info(true, &client->dev, "%s\n", __func__);

	if (!i2c_check_functionality(client->adapter, I2C_FUNC_I2C)) {
		input_err(true, &client->dev, "%s: EIO err!\n", __func__);
		return -EIO;
	}

	ret = synaptics_ts_init(client);
	if (ret < 0) {
		input_err(true, &client->dev, "%s: fail to init resource\n", __func__);
		return ret;
	}

	ts = i2c_get_clientdata(client);

	ret = synaptics_ts_hw_init(client);
	if (ret < 0) {
		input_err(true, &ts->client->dev, "%s: fail to init hw\n", __func__);
		synaptics_ts_release(client);
		return ret;
	}

	input_info(true, &ts->client->dev, "%s: request_irq = %d\n", __func__, client->irq);
	ret = request_threaded_irq(client->irq, NULL, synaptics_ts_irq_thread,
			IRQF_TRIGGER_LOW | IRQF_ONESHOT, SYNAPTICS_TS_I2C_NAME, ts);
	if (ret < 0) {
		input_err(true, &ts->client->dev, "%s: Unable to request threaded irq\n", __func__);
		synaptics_ts_release(client);
		return ret;
	}

	synaptics_ts_get_custom_library(ts);
	synaptics_ts_set_custom_library(ts);

	sec_input_register_vbus_notifier(&ts->client->dev);

	ts->probe_done = true;
	ts->plat_data->enabled = true;

	input_err(true, &ts->client->dev, "%s: done\n", __func__);

	input_log_fix();

	if (!ts->plat_data->shutdown_called)
		schedule_delayed_work(&ts->work_read_info, msecs_to_jiffies(50));

	return 0;
}

int synaptics_ts_remove(struct i2c_client *client)
{
	struct synaptics_ts_data *ts = i2c_get_clientdata(client);

	input_info(true, &ts->client->dev, "%s\n", __func__);

	sec_input_unregister_vbus_notifier(&ts->client->dev);

	mutex_lock(&ts->modechange);
	ts->plat_data->shutdown_called = true;
	mutex_unlock(&ts->modechange);

	disable_irq_nosync(ts->client->irq);
	free_irq(ts->client->irq, ts);

	synaptics_ts_release(client);
	synaptics_ts_remove_device(ts);

	return 0;
}

void synaptics_ts_shutdown(struct i2c_client *client)
{
	struct synaptics_ts_data *ts = i2c_get_clientdata(client);

	input_info(true, &ts->client->dev, "%s\n", __func__);

	synaptics_ts_remove(client);
}


#if IS_ENABLED(CONFIG_PM)
static int synaptics_ts_pm_suspend(struct device *dev)
{
	struct synaptics_ts_data *ts = dev_get_drvdata(dev);

	reinit_completion(&ts->plat_data->resume_done);

	return 0;
}

static int synaptics_ts_pm_resume(struct device *dev)
{
	struct synaptics_ts_data *ts = dev_get_drvdata(dev);

	complete_all(&ts->plat_data->resume_done);

	return 0;
}
#endif


static const struct i2c_device_id synaptics_ts_id[] = {
	{ SYNAPTICS_TS_I2C_NAME, 0 },
	{ },
};

#if IS_ENABLED(CONFIG_PM)
static const struct dev_pm_ops synaptics_ts_dev_pm_ops = {
	.suspend = synaptics_ts_pm_suspend,
	.resume = synaptics_ts_pm_resume,
};
#endif

#if IS_ENABLED(CONFIG_OF)
static const struct of_device_id synaptics_ts_match_table[] = {
	{ .compatible = "synaptics,synaptics_ts",},
	{ },
};
#else
#define synaptics_ts_match_table NULL
#endif

static struct i2c_driver synaptics_ts_driver = {
	.probe		= synaptics_ts_probe,
	.remove		= synaptics_ts_remove,
	.shutdown	= synaptics_ts_shutdown,
	.id_table	= synaptics_ts_id,
	.driver = {
		.owner	= THIS_MODULE,
		.name	= SYNAPTICS_TS_I2C_NAME,
#if IS_ENABLED(CONFIG_OF)
		.of_match_table = synaptics_ts_match_table,
#endif
#if IS_ENABLED(CONFIG_PM)
		.pm = &synaptics_ts_dev_pm_ops,
#endif
	},
};

module_i2c_driver(synaptics_ts_driver);

MODULE_SOFTDEP("pre: acpm-mfd-bus");
MODULE_DESCRIPTION("synaptics TouchScreen driver");
MODULE_LICENSE("GPL");
