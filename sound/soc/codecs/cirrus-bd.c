/*
 * Big-data logging support for Cirrus Logic CS35L41 codec
 *
 * Copyright 2017 Cirrus Logic
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
#include <linux/firmware.h>
#include <linux/vmalloc.h>
#include <linux/workqueue.h>
#include <linux/power_supply.h>
#include <linux/fs.h>
#include <linux/ktime.h>

#include <sound/cirrus/core.h>
#include <sound/cirrus/big_data.h>

#include "wmfw.h"

#define CIRRUS_BD_VERSION	"5.01.18"
#define CIRRUS_BD_DIR_NAME	"cirrus_bd"

void cirrus_bd_store_values(const char *mfd_suffix)
{
	unsigned int max_exc, over_exc_count, max_temp, over_temp_count,
			abnm_mute;
	struct cirrus_amp *amp = cirrus_get_amp_from_suffix(mfd_suffix);
	struct regmap *regmap;

	if (!amp)
		return;

	regmap = amp->regmap;

	cirrus_amp_read_ctl(amp, "BDLOG_MAX_TEMP", WMFW_ADSP2_XM,
			    CIRRUS_AMP_ALG_ID_CSPL, &max_temp);
	cirrus_amp_read_ctl(amp, "BDLOG_MAX_EXC", WMFW_ADSP2_XM,
			    CIRRUS_AMP_ALG_ID_CSPL, &max_exc);
	cirrus_amp_read_ctl(amp, "BDLOG_OVER_TEMP_COUNT", WMFW_ADSP2_XM,
			    CIRRUS_AMP_ALG_ID_CSPL, &over_temp_count);
	cirrus_amp_read_ctl(amp, "BDLOG_OVER_EXC_COUNT", WMFW_ADSP2_XM,
			    CIRRUS_AMP_ALG_ID_CSPL, &over_exc_count);
	cirrus_amp_read_ctl(amp, "BDLOG_ABNORMAL_MUTE", WMFW_ADSP2_XM,
			    CIRRUS_AMP_ALG_ID_CSPL, &abnm_mute);

	if (max_temp > (amp->bd.max_temp_limit * (1 << CS35L41_BD_TEMP_RADIX))
	    && over_temp_count == 0)
		max_temp = (amp->bd.max_temp_limit *
			    (1 << CS35L41_BD_TEMP_RADIX));

	amp->bd.over_temp_count += over_temp_count;
	amp->bd.over_exc_count += over_exc_count;
	if (max_exc > amp->bd.max_exc)
		amp->bd.max_exc = max_exc;
	if (max_temp > amp->bd.max_temp)
		amp->bd.max_temp = max_temp;
	amp->bd.abnm_mute += abnm_mute;

	amp->bd.max_temp_keep = amp->bd.max_temp;

	amp_group->last_bd_update = ktime_to_ns(ktime_get());

	dev_info(amp_group->bd_dev, "Values stored for amp%s:\n", mfd_suffix);
	dev_info(amp_group->bd_dev, "Max Excursion:\t\t%d.%04d\n",
		 amp->bd.max_exc >> CS35L41_BD_EXC_RADIX,
		 (amp->bd.max_exc & (((1 << CS35L41_BD_EXC_RADIX) - 1))) *
			10000 / (1 << CS35L41_BD_EXC_RADIX));
	dev_info(amp_group->bd_dev, "Over Excursion Count:\t%d\n",
		 amp->bd.over_exc_count);
	dev_info(amp_group->bd_dev, "Max Temp:\t\t\t%d.%04d\n",
		 amp->bd.max_temp >> CS35L41_BD_TEMP_RADIX,
		 (amp->bd.max_temp & (((1 << CS35L41_BD_TEMP_RADIX) - 1))) *
			10000 / (1 << CS35L41_BD_TEMP_RADIX));
	dev_info(amp_group->bd_dev, "Over Temp Count:\t\t%d\n",
		 amp->bd.over_temp_count);
	dev_info(amp_group->bd_dev, "Abnormal Mute:\t\t%d\n",
		 amp->bd.abnm_mute);
	dev_info(amp_group->bd_dev, "Timestamp:\t\t%llu\n",
		 amp_group->last_bd_update);

	cirrus_amp_write_ctl(amp, "BDLOG_MAX_TEMP", WMFW_ADSP2_XM,
			     CIRRUS_AMP_ALG_ID_CSPL, 0);
	cirrus_amp_write_ctl(amp, "BDLOG_MAX_EXC", WMFW_ADSP2_XM,
			     CIRRUS_AMP_ALG_ID_CSPL, 0);
	cirrus_amp_write_ctl(amp, "BDLOG_OVER_TEMP_COUNT", WMFW_ADSP2_XM,
			     CIRRUS_AMP_ALG_ID_CSPL, 0);
	cirrus_amp_write_ctl(amp, "BDLOG_OVER_EXC_COUNT", WMFW_ADSP2_XM,
			     CIRRUS_AMP_ALG_ID_CSPL, 0);
	cirrus_amp_write_ctl(amp, "BDLOG_ABNORMAL_MUTE", WMFW_ADSP2_XM,
			     CIRRUS_AMP_ALG_ID_CSPL, 0);
}
EXPORT_SYMBOL_GPL(cirrus_bd_store_values);

/***** SYSFS Interfaces *****/

static ssize_t cirrus_bd_version_show(struct device *dev,
					struct device_attribute *attr,
					char *buf)
{
	return sprintf(buf, CIRRUS_BD_VERSION "\n");
}

static ssize_t cirrus_bd_version_store(struct device *dev,
					struct device_attribute *attr,
					const char *buf, size_t size)
{
	return 0;
}

static ssize_t cirrus_bd_max_exc_show(struct device *dev,
					struct device_attribute *attr,
					char *buf)
{
	const char *suffix = &(attr->attr.name[strlen("max_exc")]);
	struct cirrus_amp *amp = cirrus_get_amp_from_suffix(suffix);
	int ret;

	if (!amp)
		return 0;

	ret = sprintf(buf, "%d.%04d\n", amp->bd.max_exc >> CS35L41_BD_EXC_RADIX,
		      (amp->bd.max_exc & ((1 << CS35L41_BD_EXC_RADIX) - 1)) *
			10000 / (1 << CS35L41_BD_EXC_RADIX));

	amp->bd.max_exc = 0;

	return ret;
}

static ssize_t cirrus_bd_max_exc_store(struct device *dev,
					struct device_attribute *attr,
					const char *buf, size_t size)
{
	return 0;
}

static ssize_t cirrus_bd_over_exc_count_show(struct device *dev,
					struct device_attribute *attr,
					char *buf)
{
	const char *suffix = &(attr->attr.name[strlen("over_exc_count")]);
	struct cirrus_amp *amp = cirrus_get_amp_from_suffix(suffix);
	int ret;

	if (!amp)
		return 0;

	ret = sprintf(buf, "%d\n", amp->bd.over_exc_count);

	amp->bd.over_exc_count = 0;

	return ret;
}

static ssize_t cirrus_bd_over_exc_count_store(struct device *dev,
					struct device_attribute *attr,
					const char *buf, size_t size)
{
	return 0;
}

static ssize_t cirrus_bd_max_temp_show(struct device *dev,
					struct device_attribute *attr,
					char *buf)
{
	const char *suffix = &(attr->attr.name[strlen("max_temp")]);
	struct cirrus_amp *amp = cirrus_get_amp_from_suffix(suffix);
	int ret;

	if (!amp)
		return 0;

	ret = sprintf(buf, "%d.%04d\n",
		      amp->bd.max_temp >> CS35L41_BD_TEMP_RADIX,
		      (amp->bd.max_temp & ((1 << CS35L41_BD_TEMP_RADIX) - 1)) *
			10000 / (1 << CS35L41_BD_TEMP_RADIX));

	amp->bd.max_temp = 0;

	return ret;
}

static ssize_t cirrus_bd_max_temp_store(struct device *dev,
					struct device_attribute *attr,
					const char *buf, size_t size)
{
	return 0;
}

static ssize_t cirrus_bd_max_temp_keep_show(struct device *dev,
					struct device_attribute *attr,
					char *buf)
{
	const char *suffix = &(attr->attr.name[strlen("max_temp_keep")]);
	struct cirrus_amp *amp = cirrus_get_amp_from_suffix(suffix);
	int ret;

	if (!amp)
		return 0;

	ret = sprintf(buf, "%d.%04d\n",
		      amp->bd.max_temp_keep >> CS35L41_BD_TEMP_RADIX,
		      (amp->bd.max_temp_keep &
			((1 << CS35L41_BD_TEMP_RADIX) - 1)) *
			10000 / (1 << CS35L41_BD_TEMP_RADIX));

	return ret;
}

static ssize_t cirrus_bd_max_temp_keep_store(struct device *dev,
					struct device_attribute *attr,
					const char *buf, size_t size)
{
	return 0;
}

static ssize_t cirrus_bd_over_temp_count_show(struct device *dev,
					struct device_attribute *attr,
					char *buf)
{
	const char *suffix = &(attr->attr.name[strlen("over_temp_count")]);
	struct cirrus_amp *amp = cirrus_get_amp_from_suffix(suffix);
	int ret;

	if (!amp)
		return 0;

	ret = sprintf(buf, "%d\n", amp->bd.over_temp_count);

	amp->bd.over_temp_count = 0;

	return ret;
}

static ssize_t cirrus_bd_over_temp_count_store(struct device *dev,
					struct device_attribute *attr,
					const char *buf, size_t size)
{
	return 0;
}

static ssize_t cirrus_bd_abnm_mute_show(struct device *dev,
					struct device_attribute *attr,
					char *buf)
{
	const char *suffix = &(attr->attr.name[strlen("abnm_mute")]);
	struct cirrus_amp *amp = cirrus_get_amp_from_suffix(suffix);
	int ret;

	if (!amp)
		return 0;

	ret = sprintf(buf, "%d\n", amp->bd.abnm_mute);

	amp->bd.abnm_mute = 0;

	return ret;
}

static ssize_t cirrus_bd_abnm_mute_store(struct device *dev,
					struct device_attribute *attr,
					const char *buf, size_t size)
{
	return 0;
}

static ssize_t cirrus_bd_store_show(struct device *dev,
					struct device_attribute *attr,
					char *buf)
{
	return 0;
}

static ssize_t cirrus_bd_store_store(struct device *dev,
					struct device_attribute *attr,
					const char *buf, size_t size)
{
	int store;
	int ret = kstrtos32(buf, 10, &store);
	const char *suffix = &(attr->attr.name[strlen("store")]);
	struct cirrus_amp *amp = cirrus_get_amp_from_suffix(suffix);

	if (ret == 0 && store == 1 && amp)
		cirrus_bd_store_values(suffix);

	return size;
}

static DEVICE_ATTR(version, 0444, cirrus_bd_version_show,
		   cirrus_bd_version_store);

static struct attribute *cirrus_bd_attr_base[] = {
	&dev_attr_version.attr,
	NULL,
};

static struct device_attribute generic_amp_attrs[CIRRUS_BD_NUM_ATTRS_AMP] = {
	{
		.attr = {.mode = VERIFY_OCTAL_PERMISSIONS(0444)},
		.show = cirrus_bd_max_exc_show,
		.store = cirrus_bd_max_exc_store,
	},
	{
		.attr = {.mode = VERIFY_OCTAL_PERMISSIONS(0444)},
		.show = cirrus_bd_over_exc_count_show,
		.store = cirrus_bd_over_exc_count_store,
	},
	{
		.attr = {.mode = VERIFY_OCTAL_PERMISSIONS(0444)},
		.show = cirrus_bd_max_temp_show,
		.store = cirrus_bd_max_temp_store,
	},
	{
		.attr = {.mode = VERIFY_OCTAL_PERMISSIONS(0444)},
		.show = cirrus_bd_max_temp_keep_show,
		.store = cirrus_bd_max_temp_keep_store,
	},
	{
		.attr = {.mode = VERIFY_OCTAL_PERMISSIONS(0444)},
		.show = cirrus_bd_over_temp_count_show,
		.store = cirrus_bd_over_temp_count_store,
	},
	{
		.attr = {.mode = VERIFY_OCTAL_PERMISSIONS(0444)},
		.show = cirrus_bd_abnm_mute_show,
		.store = cirrus_bd_abnm_mute_store,
	},
	{
		.attr = {.mode = VERIFY_OCTAL_PERMISSIONS(0644)},
		.show = cirrus_bd_store_show,
		.store = cirrus_bd_store_store,
	},
};

static const char *generic_amp_attr_names[CIRRUS_BD_NUM_ATTRS_AMP] = {
	"max_exc",
	"over_exc_count",
	"max_temp",
	"max_temp_keep",
	"over_temp_count",
	"abnm_mute",
	"store"
};

static struct attribute_group cirrus_bd_attr_grp;
static struct device_attribute
		amp_attrs_prealloc[CIRRUS_MAX_AMPS][CIRRUS_BD_NUM_ATTRS_AMP];
static char attr_names_prealloc[CIRRUS_MAX_AMPS][CIRRUS_BD_NUM_ATTRS_AMP][30];

struct device_attribute *cirrus_bd_create_amp_attrs(const char *mfd_suffix,
							const char *bd_suffix,
							int index)
{
	struct device_attribute *amp_attrs_new;
	int i, suffix_len;
	const char *suffix;

	suffix = (bd_suffix) ? bd_suffix : mfd_suffix;
	suffix_len = strlen(suffix);

	if (index >= CIRRUS_MAX_AMPS)
		return NULL;

	amp_attrs_new = &(amp_attrs_prealloc[index][0]);

	memcpy(amp_attrs_new, &generic_amp_attrs,
		sizeof(struct device_attribute) *
		CIRRUS_BD_NUM_ATTRS_AMP);

	for (i = 0; i < CIRRUS_BD_NUM_ATTRS_AMP - 1; i++) {
		amp_attrs_new[i].attr.name = attr_names_prealloc[index][i];
		snprintf((char *)amp_attrs_new[i].attr.name,
			strlen(generic_amp_attr_names[i]) + suffix_len + 1,
			"%s%s", generic_amp_attr_names[i], suffix);
	}

	/* "store" is special and will always be assigned the MFD suffix */
	amp_attrs_new[CIRRUS_BD_NUM_ATTRS_AMP - 1].attr.name =
			attr_names_prealloc[index][CIRRUS_BD_NUM_ATTRS_AMP - 1];
	snprintf((char *)amp_attrs_new[CIRRUS_BD_NUM_ATTRS_AMP - 1].attr.name,
		strlen("store") + strlen(mfd_suffix) + 1,
		"%s%s", "store", mfd_suffix);

	return amp_attrs_new;
}

int cirrus_bd_init(void)
{
	struct device_attribute *new_attrs;
	struct cirrus_amp *amp;
	int ret = 0, i, j, num_amps;

	if (!amp_group) {
		pr_err("%s: Empty amp group\n", __func__);
		return -ENODATA;
	}

	amp_group->bd_dev = device_create(cirrus_amp_class, NULL, 1, NULL,
					  CIRRUS_BD_DIR_NAME);
	if (IS_ERR(amp_group->bd_dev)) {
		ret = PTR_ERR(amp_group->bd_dev);
		pr_err("%s: Failed to create BD device (%d)\n", __func__, ret);
		return ret;
	}

	dev_set_drvdata(amp_group->bd_dev, amp_group);

	num_amps = amp_group->num_amps;

	cirrus_bd_attr_grp.attrs = kzalloc(sizeof(struct attribute *) *
					(CIRRUS_BD_NUM_ATTRS_AMP * num_amps +
					CIRRUS_BD_NUM_ATTRS_BASE + 1),
							GFP_KERNEL);
	for (i = 0; i < num_amps; i++) {
		amp = &amp_group->amps[i];
		new_attrs = cirrus_bd_create_amp_attrs(amp->mfd_suffix,
							amp->bd.bd_suffix, i);
		for (j = 0; j < CIRRUS_BD_NUM_ATTRS_AMP; j++) {
			dev_dbg(amp_group->bd_dev, "New attribute: %s\n",
				new_attrs[j].attr.name);
			cirrus_bd_attr_grp.attrs[i * CIRRUS_BD_NUM_ATTRS_AMP
						 + j] = &new_attrs[j].attr;
		}
	}

	memcpy(&cirrus_bd_attr_grp.attrs[num_amps * CIRRUS_BD_NUM_ATTRS_AMP],
		cirrus_bd_attr_base, sizeof(struct attribute *) *
					CIRRUS_BD_NUM_ATTRS_BASE);
	cirrus_bd_attr_grp.attrs[num_amps * CIRRUS_BD_NUM_ATTRS_AMP +
			CIRRUS_BD_NUM_ATTRS_BASE] = NULL;

	ret = sysfs_create_group(&amp_group->bd_dev->kobj, &cirrus_bd_attr_grp);
	if (ret < 0) {
		dev_err(amp_group->bd_dev,
			"Failed to create sysfs group (%d)\n", ret);
		device_del(amp_group->bd_dev);
	}

	return ret;
}
EXPORT_SYMBOL_GPL(cirrus_bd_init);

void cirrus_bd_exit(void)
{
	kfree(cirrus_bd_attr_grp.attrs);
	device_del(amp_group->bd_dev);
}
EXPORT_SYMBOL_GPL(cirrus_bd_exit);
