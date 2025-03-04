/*
 * Power-management support for Cirrus Logic CS35L41 amplifier
 *
 * Copyright 2018 Cirrus Logic
 *
 * Author:	David Rhodes	<david.rhodes@cirrus.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#include <linux/module.h>
#include <linux/miscdevice.h>
#include <linux/device.h>
#include <linux/uaccess.h>
#include <linux/delay.h>
#include <linux/regmap.h>
#include <linux/slab.h>
#include <linux/syscalls.h>
#include <linux/file.h>
#include <linux/fcntl.h>
#include <linux/fs.h>
#include <linux/init.h>
#include <linux/platform_device.h>
#include <linux/vmalloc.h>
#include <linux/workqueue.h>
#include <linux/fs.h>
#include <linux/ktime.h>

#include <sound/cirrus/core.h>
#include <sound/cirrus/power.h>

#define CIRRUS_PWR_VERSION "5.01.18"

#define CIRRUS_PWR_DIR_NAME		"cirrus_pwr"
#define CIRRUS_PWR_WORKQ_NAME		"cirrus_pwr_wq"

#define CIRRUS_PWR_STATUS_DISABLED	0
#define	CIRRUS_PWR_STATUS_ENABLED	1
#define CIRRUS_PWR_STATUS_ERROR		3

#define CIRRUS_PWR_AMB_TEMP_OFFSET	500
#define CIRRUS_PWR_SCALING_Q15		846397

static unsigned int sqrt_q24(unsigned long int x)
{
	u32 root, remHi, remLo, testDiv, count;

	root = 0;
	remHi = 0;
	remLo = x;
	count = 24;

	do {
		remHi = (remHi << 2) | (remLo >> 30);
		remLo <<= 2;
		root <<= 1;
		testDiv = (root << 1) + 1;
		if (remHi >= testDiv) {
			remHi -= testDiv;
			root++;
		}
	} while (count-- != 0);

	return root; /* Q21 result */
}

static unsigned int convert_power(unsigned int power_squared)
{
	unsigned long long int power;

	power = sqrt_q24(power_squared*2);
	power *= CIRRUS_PWR_SCALING_Q15;

	dev_dbg(amp_group->pwr_dev,
			"converted power (%d W^2): %llu.%04llu W\n",
			power_squared,
			power >> 36,
			(power & (((1ull << 36) - 1ull))) *
			    10000 / (1ull << 36));

	power *= 1000;
	power >>= 28;

	dev_dbg(amp_group->pwr_dev,
		"converted power q8 mW: %d mW = 0x%x\n",
		(unsigned int)(power / 256), (unsigned int)(power));

	return (unsigned int)power;
}

static void cirrus_pwr_passport_enable(struct regmap *regmap_enable,
							bool enable)
{
	if (regmap_enable)
		regmap_write(regmap_enable,
			CIRRUS_PWR_CSPL_PASSPORT_ENABLE,
			(uint)enable);
}

void cirrus_pwr_start(const char *mfd_suffix)
{
	struct cirrus_amp *amp = cirrus_get_amp_from_suffix(mfd_suffix);

	if (!amp)
		return;

	amp->pwr.amp_active = 1;

	if (!amp_group->pwr_enable)
		return;

	mutex_lock(&amp_group->pwr_lock);

	if (amp_group->status == CIRRUS_PWR_STATUS_ENABLED) {
		/* State machine already active on one amp */
		dev_dbg(amp_group->pwr_dev,
			"cirrus_pwr_start(), additional amp activated");
	} else {
		/* Init state machine */
		dev_dbg(amp_group->pwr_dev,
			"cirrus_pwr_start() Entering wait period.\n");
		amp_group->status = CIRRUS_PWR_STATUS_ENABLED;

		/* Queue state machine operation */
		queue_delayed_work(amp_group->pwr_workqueue,
				   &amp_group->pwr_work,
				   msecs_to_jiffies(amp_group->interval));
	}

	mutex_unlock(&amp_group->pwr_lock);
}
EXPORT_SYMBOL_GPL(cirrus_pwr_start);

void cirrus_pwr_stop(const char *mfd_suffix)
{
	struct cirrus_amp *amp = cirrus_get_amp_from_suffix(mfd_suffix);
	int i;
	bool amps_active = 0;

	if (!amp)
		return;

	amp->pwr.amp_active = 0;

	if (!amp_group->pwr_enable)
		return;

	mutex_lock(&amp_group->pwr_lock);

	for (i = 0; i < amp_group->num_amps; i++)
		amps_active |= amp->pwr.amp_active;

	if (amps_active) {
		/* One amp still active */
		dev_dbg(amp_group->pwr_dev, "Amp cs35l41%s deactivated\n",
			mfd_suffix);
	} else {
		/* Exit state machine */
		dev_dbg(amp_group->pwr_dev,
			"cirrus_pwr_stop(). Disabling PASSPORT\n");

		for (i = 0; i < amp_group->num_amps; i++) {
			cirrus_pwr_passport_enable(
				amp_group->amps[i].regmap, false);
			amp_group->amps[i].pwr.passport_enable = 0;
		}

		/* Reset state machine variables */
		amp_group->uptime_ms = 0;
		amp_group->status = CIRRUS_PWR_STATUS_DISABLED;

		/* cancel workqueue */
		if (delayed_work_pending(&amp_group->pwr_work))
			cancel_delayed_work(&amp_group->pwr_work);
	}

	mutex_unlock(&amp_group->pwr_lock);
}
EXPORT_SYMBOL_GPL(cirrus_pwr_stop);

static void cirrus_pwr_work(struct work_struct *work)
{
	int i;
	struct cirrus_amp *amp;

	mutex_lock(&amp_group->pwr_lock);

	/* Run state machine and enable/disable Passport accordingly */

	if (amp_group->status != CIRRUS_PWR_STATUS_ENABLED)
		goto exit;

	amp_group->uptime_ms += amp_group->interval;

	if (amp_group->uptime_ms <= amp_group->target_min_time_ms) {
		dev_dbg(amp_group->pwr_dev,
			"Waiting for min time... (%d / %d ms)\n",
			amp_group->uptime_ms,
			amp_group->target_min_time_ms);
		goto exit;
	}

	/* Enabled and > min time */
	/* Evaluate temp for each amp and enable/disable Passport */
	for (i = 0; i < amp_group->num_amps; i++) {
		amp = &amp_group->amps[i];

		dev_dbg(amp_group->pwr_dev,"Amp cs35l41%s\n", amp->mfd_suffix);
		dev_dbg(amp_group->pwr_dev,
			"Spk Temp:\t%d.%d C\t(Target: %d.%d C)\n",
			amp->pwr.spk_temp / 100,
			amp->pwr.spk_temp % 100,
			amp->pwr.target_temp / 100,
			amp->pwr.target_temp % 100);
		dev_dbg(amp_group->pwr_dev, "Amb Temp:\t%d.%d\n",
			amp->pwr.amb_temp / 100,
			amp->pwr.amb_temp % 100);

		if (!amp->pwr.amp_active)
			continue;

		if (amp->pwr.passport_enable) {
			/* Evaluate exit criteria */
			if (amp->pwr.spk_temp < amp->pwr.exit_temp) {
				cirrus_pwr_passport_enable(
					amp->regmap,
					false);

				dev_info(amp_group->pwr_dev,
					 "Amp cs35l41%s below exit temp. Disabling PASSPORT\n",
					 amp->mfd_suffix);

				amp->pwr.passport_enable = 0;
			}
		} else {
			/* Evaluate entry criteria */
			if ((amp->pwr.amb_temp + CIRRUS_PWR_AMB_TEMP_OFFSET <
			     amp->pwr.spk_temp) && (amp->pwr.spk_temp >
						    amp->pwr.target_temp)) {
				cirrus_pwr_passport_enable(amp->regmap, true);

				dev_info(amp_group->pwr_dev,
					 "Amp cs35l41%s above target temp and ambient + 5.\n",
					 amp->mfd_suffix);

				dev_info(amp_group->pwr_dev,
					 "Enabling PASSPORT\n");

				amp->pwr.passport_enable = 1;
			}

		}

		dev_dbg(amp_group->pwr_dev, "Amp cs35l41%s: Passport %s\n",
			amp->mfd_suffix, amp->pwr.passport_enable ?
				"Enabled" : "Disabled");
	}

exit:
	mutex_unlock(&amp_group->pwr_lock);

	/* Queue next operation */
	if (amp_group->pwr_enable)
		queue_delayed_work(amp_group->pwr_workqueue,
				   &amp_group->pwr_work,
				   msecs_to_jiffies(amp_group->interval));
}

/***** SYSFS Interfaces *****/

static ssize_t cirrus_pwr_version_show(struct device *dev,
					struct device_attribute *attr,
					char *buf)
{
	return sprintf(buf, CIRRUS_PWR_VERSION "\n");
}

static ssize_t cirrus_pwr_version_store(struct device *dev,
					struct device_attribute *attr,
					const char *buf, size_t size)
{
	return size;
}

static ssize_t cirrus_pwr_uptime_show(struct device *dev,
					struct device_attribute *attr,
					char *buf)
{
	return sprintf(buf, "%d\n", amp_group->uptime_ms);
}

static ssize_t cirrus_pwr_uptime_store(struct device *dev,
					struct device_attribute *attr,
					const char *buf, size_t size)
{
	return size;
}

static ssize_t cirrus_pwr_power_show(struct device *dev,
					struct device_attribute *attr,
					char *buf)
{
	const char *suffix = &(attr->attr.name[strlen("value")]);
	struct cirrus_amp *amp = cirrus_get_amp_from_suffix(suffix);
	unsigned int power_squared;
	unsigned int power = 0;

	if (!amp)
		return 0;

	if (amp->pwr.amp_active) {
		regmap_read(amp->regmap,
			CIRRUS_PWR_CSPL_OUTPUT_POWER_SQ,
			&power_squared);
		power = convert_power(power_squared);
	}

	return sprintf(buf, "%x\n", power);
}

static ssize_t cirrus_pwr_power_store(struct device *dev,
					struct device_attribute *attr,
					const char *buf, size_t size)
{
	return size;
}

static ssize_t cirrus_pwr_interval_show(struct device *dev,
					struct device_attribute *attr,
					char *buf)
{
	return sprintf(buf, "%d\n", amp_group->interval);
}

static ssize_t cirrus_pwr_interval_store(struct device *dev,
					struct device_attribute *attr,
					const char *buf, size_t size)
{
	if (kstrtou32(buf, 0, &amp_group->interval))
		dev_err(amp_group->pwr_dev,
			"%s: Failed to convert from str to u32.\n",
			__func__);
	return size;
}

static ssize_t cirrus_pwr_status_show(struct device *dev,
					struct device_attribute *attr,
					char *buf)
{
	switch (amp_group->status) {
	case CIRRUS_PWR_STATUS_DISABLED:
		return sprintf(buf, "Disabled\n");
	case CIRRUS_PWR_STATUS_ENABLED:
		return sprintf(buf, "Enabled\n");
	case CIRRUS_PWR_STATUS_ERROR:
		return sprintf(buf, "Error\n");
	default:
		return sprintf(buf, "\n");
	}
}

static ssize_t cirrus_pwr_status_store(struct device *dev,
					struct device_attribute *attr,
					const char *buf, size_t size)
{
	return size;
}

static ssize_t cirrus_pwr_target_min_time_ms_show(struct device *dev,
					struct device_attribute *attr,
					char *buf)
{
	return sprintf(buf, "%d\n", amp_group->target_min_time_ms);
}

static ssize_t cirrus_pwr_target_min_time_ms_store(struct device *dev,
					struct device_attribute *attr,
					const char *buf, size_t size)
{
	if (kstrtou32(buf, 0, &amp_group->target_min_time_ms))
		dev_err(amp_group->pwr_dev,
			"%s: Failed to convert from str to u32.\n", __func__);
	return size;
}

static ssize_t cirrus_pwr_target_temp_show(struct device *dev,
					struct device_attribute *attr,
					char *buf)
{
	const char *suffix = &(attr->attr.name[strlen("target_temp")]);
	struct cirrus_amp *amp = cirrus_get_amp_from_suffix(suffix);

	if (!amp)
		return 0;

	return sprintf(buf, "%d\n", amp->pwr.target_temp);
}

static ssize_t cirrus_pwr_target_temp_store(struct device *dev,
					struct device_attribute *attr,
					const char *buf, size_t size)
{
	const char *suffix = &(attr->attr.name[strlen("target_temp")]);
	struct cirrus_amp *amp = cirrus_get_amp_from_suffix(suffix);

	if (!amp)
		return 0;

	if (kstrtou32(buf, 0, &amp->pwr.target_temp))
		dev_err(amp_group->pwr_dev,
			"%s: Failed to convert from str to u32.\n", __func__);
	return size;
}

static ssize_t cirrus_pwr_exit_temp_show(struct device *dev,
					struct device_attribute *attr,
					char *buf)
{
	const char *suffix = &(attr->attr.name[strlen("exit_temp")]);
	struct cirrus_amp *amp = cirrus_get_amp_from_suffix(suffix);

	if (!amp)
		return 0;

	return sprintf(buf, "%d\n", amp->pwr.exit_temp);
}

static ssize_t cirrus_pwr_exit_temp_store(struct device *dev,
					struct device_attribute *attr,
					const char *buf, size_t size)
{
	const char *suffix = &(attr->attr.name[strlen("exit_temp")]);
	struct cirrus_amp *amp = cirrus_get_amp_from_suffix(suffix);

	if (!amp)
		return 0;

	if (kstrtou32(buf, 0, &amp->pwr.exit_temp))
		dev_err(amp_group->pwr_dev,
			"%s: Failed to convert from str to u32.\n", __func__);
	return size;
}

static ssize_t cirrus_pwr_amb_temp_show(struct device *dev,
					struct device_attribute *attr,
					char *buf)
{
	const char *suffix = &(attr->attr.name[strlen("amb_temp")]);
	struct cirrus_amp *amp = cirrus_get_amp_from_suffix(suffix);

	if (!amp)
		return 0;

	return sprintf(buf, "%d\n", amp->pwr.amb_temp);
}

static ssize_t cirrus_pwr_amb_temp_store(struct device *dev,
					struct device_attribute *attr,
					const char *buf, size_t size)
{
	const char *suffix = &(attr->attr.name[strlen("amb_temp")]);
	struct cirrus_amp *amp = cirrus_get_amp_from_suffix(suffix);

	if (!amp)
		return 0;

	if (kstrtou32(buf, 0, &amp->pwr.amb_temp))
		dev_err(amp_group->pwr_dev,
			"%s: Failed to convert from str to u32.\n", __func__);
	return size;
}

static ssize_t cirrus_pwr_spk_temp_show(struct device *dev,
					struct device_attribute *attr,
					char *buf)
{
	const char *suffix = &(attr->attr.name[strlen("spk_t")]);
	struct cirrus_amp *amp = cirrus_get_amp_from_suffix(suffix);

	if (!amp)
		return 0;

	return sprintf(buf, "%d\n", amp->pwr.spk_temp);
}

static ssize_t cirrus_pwr_spk_temp_store(struct device *dev,
					struct device_attribute *attr,
					const char *buf, size_t size)
{
	const char *suffix = &(attr->attr.name[strlen("spk_t")]);
	struct cirrus_amp *amp = cirrus_get_amp_from_suffix(suffix);

	if (!amp)
		return 0;

	if (kstrtou32(buf, 0, &amp->pwr.spk_temp))
		dev_err(amp_group->pwr_dev,
			"%s: Failed to convert from str to u32.\n", __func__);
	return size;
}

static ssize_t cirrus_pwr_global_enable_show(struct device *dev,
					struct device_attribute *attr,
					char *buf)
{
	return sprintf(buf, "%d\n", amp_group->pwr_enable);
}

static ssize_t cirrus_pwr_global_enable_store(struct device *dev,
					struct device_attribute *attr,
					const char *buf, size_t size)
{
	unsigned int enable;
	int i;

	if (kstrtou32(buf, 0, &enable)) {
		dev_err(amp_group->pwr_dev,
			"%s: Failed to convert from str to u32.\n", __func__);
		return size;
	}

	amp_group->pwr_enable = enable;

	if (enable == 0 &&
		amp_group->status == CIRRUS_PWR_STATUS_ENABLED) {
		/* Stop all amps */
		for (i = 0; i < amp_group->num_amps; i++)
			cirrus_pwr_stop(amp_group->amps[i].mfd_suffix);
	}

	return size;
}

static DEVICE_ATTR(version, 0444, cirrus_pwr_version_show,
				cirrus_pwr_version_store);
static DEVICE_ATTR(uptime, 0444, cirrus_pwr_uptime_show,
				cirrus_pwr_uptime_store);
static DEVICE_ATTR(global_enable, 0664, cirrus_pwr_global_enable_show,
				cirrus_pwr_global_enable_store);
static DEVICE_ATTR(interval, 0664, cirrus_pwr_interval_show,
				cirrus_pwr_interval_store);
static DEVICE_ATTR(status, 0664, cirrus_pwr_status_show,
				cirrus_pwr_status_store);
static DEVICE_ATTR(target_min_time_ms, 0664, cirrus_pwr_target_min_time_ms_show,
				cirrus_pwr_target_min_time_ms_store);

static struct attribute *cirrus_pwr_attr_base[] = {
	&dev_attr_version.attr,
	&dev_attr_uptime.attr,
	&dev_attr_interval.attr,
	&dev_attr_status.attr,
	&dev_attr_target_min_time_ms.attr,
	&dev_attr_global_enable.attr,
	NULL,
};

static struct device_attribute generic_amp_attrs[CIRRUS_PWR_NUM_ATTRS_AMP] = {
	{
		.attr = {.mode = VERIFY_OCTAL_PERMISSIONS(0444)},
		.show = cirrus_pwr_power_show,
		.store = cirrus_pwr_power_store,
	},
	{
		.attr = {.mode = VERIFY_OCTAL_PERMISSIONS(0664)},
		.show = cirrus_pwr_target_temp_show,
		.store = cirrus_pwr_target_temp_store,
	},
	{
		.attr = {.mode = VERIFY_OCTAL_PERMISSIONS(0664)},
		.show = cirrus_pwr_exit_temp_show,
		.store = cirrus_pwr_exit_temp_store,
	},
	{
		.attr = {.mode = VERIFY_OCTAL_PERMISSIONS(0664)},
		.show = cirrus_pwr_amb_temp_show,
		.store = cirrus_pwr_amb_temp_store,
	},
	{
		.attr = {.mode = VERIFY_OCTAL_PERMISSIONS(0664)},
		.show = cirrus_pwr_spk_temp_show,
		.store = cirrus_pwr_spk_temp_store,
	},
};

static const char *generic_amp_attr_names[CIRRUS_PWR_NUM_ATTRS_AMP] = {
	"value",
	"target_temp",
	"exit_temp",
	"env_temp",
	"spk_t",
};

static struct attribute_group cirrus_pwr_attr_grp;
static struct device_attribute
		amp_attrs_prealloc[CIRRUS_MAX_AMPS][CIRRUS_PWR_NUM_ATTRS_AMP];
static char attr_names_prealloc[CIRRUS_MAX_AMPS][CIRRUS_PWR_NUM_ATTRS_AMP][20];

struct device_attribute *cirrus_pwr_create_amp_attrs(const char *mfd_suffix,
							int index)
{
	struct device_attribute *amp_attrs_new;
	int i, suffix_len = strlen(mfd_suffix);

	if (index >= CIRRUS_MAX_AMPS)
		return NULL;

	amp_attrs_new = &(amp_attrs_prealloc[index][0]);

	memcpy(amp_attrs_new, &generic_amp_attrs,
		sizeof(struct device_attribute) *
		CIRRUS_PWR_NUM_ATTRS_AMP);

	for (i = 0; i < CIRRUS_PWR_NUM_ATTRS_AMP; i++) {
		amp_attrs_new[i].attr.name = attr_names_prealloc[index][i];
		snprintf((char *)amp_attrs_new[i].attr.name,
			strlen(generic_amp_attr_names[i]) + suffix_len + 1,
			"%s%s", generic_amp_attr_names[i], mfd_suffix);
	}

	return amp_attrs_new;
}

int cirrus_pwr_init(void)
{
	struct device_attribute *new_attrs;
	struct cirrus_amp *amp;
	int ret = 0, i, j, num_amps;

	if (!amp_group) {
		pr_err("%s: Empty amp group\n", __func__);
		return -ENODATA;
	}

	amp_group->pwr_dev = device_create(cirrus_amp_class, NULL, 1, NULL,
					   CIRRUS_PWR_DIR_NAME);
	if (IS_ERR(amp_group->pwr_dev)) {
		ret = PTR_ERR(amp_group->pwr_dev);
		pr_err("%s: Failed to create PWR device (%d)\n", __func__, ret);
		return ret;
	}

	dev_set_drvdata(amp_group->pwr_dev, amp_group);

	num_amps = amp_group->num_amps;

	for (i = 0; i < num_amps; i++) {
		amp_group->amps[i].pwr.amb_temp = 2500;
		amp_group->amps[i].pwr.spk_temp = 2500;
		amp_group->amps[i].pwr.target_temp = 3400;
		amp_group->amps[i].pwr.exit_temp = 3250;
		amp_group->amps[i].pwr.passport_enable = 0;
	}

	cirrus_pwr_attr_grp.attrs = kzalloc(sizeof(struct attribute *) *
					(CIRRUS_PWR_NUM_ATTRS_AMP * num_amps +
					CIRRUS_PWR_NUM_ATTRS_BASE + 1),
							GFP_KERNEL);
	for (i = 0; i < num_amps; i++) {
		amp = &amp_group->amps[i];
		new_attrs = cirrus_pwr_create_amp_attrs(amp->mfd_suffix, i);
		for (j = 0; j < CIRRUS_PWR_NUM_ATTRS_AMP; j++) {
			dev_dbg(amp_group->pwr_dev, "New attribute: %s\n",
				new_attrs[j].attr.name);
			cirrus_pwr_attr_grp.attrs[i * CIRRUS_PWR_NUM_ATTRS_AMP
						 + j] = &new_attrs[j].attr;
		}
	}

	memcpy(&cirrus_pwr_attr_grp.attrs[num_amps * CIRRUS_PWR_NUM_ATTRS_AMP],
		cirrus_pwr_attr_base, sizeof(struct attribute *) *
					CIRRUS_PWR_NUM_ATTRS_BASE);
	cirrus_pwr_attr_grp.attrs[num_amps * CIRRUS_PWR_NUM_ATTRS_AMP +
			CIRRUS_PWR_NUM_ATTRS_BASE] = NULL;

	amp_group->pwr_workqueue = create_singlethread_workqueue(
						CIRRUS_PWR_WORKQ_NAME);
	if (amp_group->pwr_workqueue == NULL) {
		dev_err(amp_group->pwr_dev, "Failed to create workqueue\n");
		ret = -ENOENT;
		goto err;
	}

	amp_group->interval = 10000;
	amp_group->uptime_ms = 0;
	amp_group->target_min_time_ms = 300000;
	amp_group->pwr_enable = 0;

	ret = sysfs_create_group(&amp_group->pwr_dev->kobj,
				 &cirrus_pwr_attr_grp);
	if (ret) {
		dev_err(amp_group->pwr_dev, "Failed to create sysfs group\n");
		goto err;
	}

	mutex_init(&amp_group->pwr_lock);
	INIT_DELAYED_WORK(&amp_group->pwr_work, cirrus_pwr_work);

	return 0;

err:
	return ret;
}

void cirrus_pwr_exit(void)
{
	kfree(cirrus_pwr_attr_grp.attrs);
	device_del(amp_group->pwr_dev);
}

