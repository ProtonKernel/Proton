/* sound/soc/samsung/abox/abox_failsafe.c
 *
 * ALSA SoC Audio Layer - Samsung Abox Failsafe driver
 *
 * Copyright (c) 2016 Samsung Electronics Co. Ltd.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */
/* #define DEBUG */
#include <linux/module.h>
#include <linux/debugfs.h>
#include <linux/proc_fs.h>
#include <linux/mutex.h>
#include <linux/vmalloc.h>
#include <linux/pm_runtime.h>
#include <sound/samsung/abox.h>

#include "abox_util.h"
#include "abox.h"
#include "abox_proc.h"
#include "abox_log.h"
#include "abox_memlog.h"
#include "abox_dbg.h"

#define SMART_FAILSAFE

static int abox_failsafe_reset_count;
static struct device *abox_failsafe_dev;
static struct device *abox_failsafe_dev_abox;
static struct abox_data *abox_failsafe_abox_data;
static atomic_t abox_failsafe_reported;
static bool abox_failsafe_service;

static int abox_failsafe_start(struct device *dev, struct abox_data *data)
{
	int ret = 0;

	abox_dbg(dev, "%s\n", __func__);

	if (atomic_read(&abox_failsafe_reported)) {
		/* Set AUD_OPTION[2] for abox silent reset in PMU */
		abox_silent_reset(data, true);
		if (abox_failsafe_service)
			pm_runtime_put(dev);
		abox_info(dev, "%s\n", __func__);
		abox_clear_cpu_gear_requests(dev);
	}

	return ret;
}

static int abox_failsafe_end(struct device *dev)
{
	int ret = 0;

	abox_dbg(dev, "%s\n", __func__);

	if (atomic_cmpxchg(&abox_failsafe_reported, 1, 0)) {
		abox_info(dev, "%s\n", __func__);
		abox_failsafe_abox_data->failsafe = false;
		abox_failsafe_abox_data->error = false;
	}

	return ret;
}

static void abox_failsafe_report_work_func(struct work_struct *work)
{
	struct device *dev = abox_failsafe_dev;
	char env[32] = {0,};
	char *envp[2] = {env, NULL};

	abox_dbg(dev, "%s\n", __func__);
	pm_runtime_barrier(dev);
	snprintf(env, sizeof(env), "COUNT=%d", abox_failsafe_reset_count);
	kobject_uevent_env(&dev->kobj, KOBJ_CHANGE, envp);
}

DECLARE_WORK(abox_failsafe_report_work, abox_failsafe_report_work_func);

#ifdef SMART_FAILSAFE
void abox_failsafe_report(struct device *dev, bool error)
{
	abox_dbg(dev, "%s\n", __func__);

	BUG_ON(error && abox_failsafe_abox_data->debug_mode);

	abox_failsafe_dev = dev;
	abox_failsafe_reset_count++;
	if (!atomic_cmpxchg(&abox_failsafe_reported, 0, 1)) {
		abox_failsafe_abox_data->failsafe = true;
		abox_failsafe_abox_data->error = error;
		if (abox_failsafe_service)
			pm_runtime_get(dev);
		schedule_work(&abox_failsafe_report_work);
	}
}
#else
/* TODO: Use SMART_FAILSAFE.
 * SMART_FAILSAFE needs support from user space.
 */
void abox_failsafe_report(struct device *dev, bool error)
{
	struct abox_data *data = dev_get_drvdata(dev);

	abox_dbg(dev, "%s\n", __func__);

	BUG_ON(error && abox_failsafe_abox_data->debug_mode);

	abox_failsafe_start(dev, data);
}
#endif
void abox_failsafe_report_reset(struct device *dev)
{
	abox_dbg(dev, "%s\n", __func__);

	abox_failsafe_end(dev);
}

static int abox_failsafe_reset(struct device *dev, struct abox_data *data)
{
	abox_dbg(dev, "%s\n", __func__);

	return abox_failsafe_start(data->dev, data);
}

static ssize_t reset_store(struct device *dev, struct device_attribute *attr,
		const char *buf, size_t count)
{
	static const char code[] = "CALLIOPE";
	struct abox_data *data = dev_get_drvdata(dev);
	int ret;

	abox_dbg(dev, "%s(%zu)\n", __func__, count);

	if (!strncmp(code, buf, min(sizeof(code) - 1, count))) {
		ret = abox_failsafe_reset(dev, data);
		if (ret < 0)
			return ret;
	}

	return count;
}

static DEVICE_ATTR_WO(reset);
static DEVICE_BOOL_ATTR(service, 0660, abox_failsafe_service);
static DEVICE_INT_ATTR(reset_count, 0660, abox_failsafe_reset_count);

void abox_failsafe_init(struct device *dev)
{
	struct proc_dir_entry *dir;
	int ret;

	abox_failsafe_dev_abox = dev;
	abox_failsafe_abox_data = (struct abox_data *)dev_get_drvdata(dev);

	dir = abox_proc_mkdir("failsafe", NULL);

	ret = device_create_file(dev, &dev_attr_reset);
	if (ret < 0)
		abox_warn(dev, "%s: %s file creation failed: %d\n",
				__func__, "reset", ret);
	abox_proc_symlink_attr(dev, "reset", dir);

	ret = device_create_file(dev, &dev_attr_service.attr);
	if (ret < 0)
		abox_warn(dev, "%s: %s file creation failed: %d\n",
				__func__, "service", ret);
	abox_proc_symlink_attr(dev, "service", dir);

	ret = device_create_file(dev, &dev_attr_reset_count.attr);
	if (ret < 0)
		abox_warn(dev, "%s: %s file creation failed: %d\n",
				__func__, "reset_count", ret);
	abox_proc_symlink_attr(dev, "reset_count", dir);
}

