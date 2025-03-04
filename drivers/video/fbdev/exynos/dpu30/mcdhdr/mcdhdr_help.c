/*
 * Copyright (c) 2016 Samsung Electronics Co., Ltd.
 *		http://www.samsung.com
 *
 * Samsung EXYNOS MCD HDR driver

 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#include <linux/module.h>
#include <linux/platform_device.h>

#include "mcdhdr_drv.h"

#define HDR_CONCAT(a, b)  _HDR_CONCAT(a, b)
#define _HDR_CONCAT(a, b) a ## _ ## b

#define __MCDHDR_ATTR_RO(_name, _mode) __ATTR(_name, _mode, HDR_CONCAT(_name, show), NULL)
#define __MCDHDR_ATTR_WO(_name, _mode) __ATTR(_name, _mode, NULL, HDR_CONCAT(_name, store))
#define __MCDHDR_ATTR_RW(_name, _mode) __ATTR(_name, _mode, HDR_CONCAT(_name, show), HDR_CONCAT(_name, store))


static ssize_t mcdhdr_attr_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	int len = 0;
	struct mcdhdr_device *mcdhdr = dev_get_drvdata(dev);

	if (!mcdhdr) {
		mcdhdr_err("mcdhdr is null\n");
		return 0;
	}

	len = sprintf(buf, "id: %d wcg: %s, hdr10: %s, hdr10+: %s\n", mcdhdr->id,
		IS_SUPPORT_WCG(mcdhdr->attr) ? "support" : "n/a",
		IS_SUPPORT_HDR10(mcdhdr->attr) ? "support" : "n/a",
		IS_SUPPORT_HDR10P(mcdhdr->attr) ? "support" : "n/a");

	return len;
}

struct device_attribute mcdhdr_sysfs_attrs[] = {
	__MCDHDR_ATTR_RO(mcdhdr_attr, 0444),
};


int mcdhdr_probe_sysfs(struct mcdhdr_device *mcdhdr)
{
	size_t i;
	int ret = 0;

	if (!mcdhdr) {
		mcdhdr_err("invalid param mcdhdr is null\n");
		return -EINVAL;
	}

	for (i = 0; i < ARRAY_SIZE(mcdhdr_sysfs_attrs); i++) {
		ret = device_create_file(mcdhdr->dev, &mcdhdr_sysfs_attrs[i]);
		if (ret < 0) {
			mcdhdr_err("failed to add sysfs(%s) entries, %d\n",
					mcdhdr_sysfs_attrs[i].attr.name, ret);
			goto exit_probe;
		}
	}

exit_probe:
	return ret;
}

