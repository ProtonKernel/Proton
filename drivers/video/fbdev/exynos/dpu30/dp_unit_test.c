/*
 * Copyright (c) 2021 Samsung Electronics Co., Ltd.
 *		http://www.samsung.com
 *
 * DP unit test
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#include <linux/kernel.h>
#include <linux/errno.h>
#include <linux/string.h>
#include <linux/types.h>
#include <linux/stat.h>
#include <linux/module.h>
#include <linux/proc_fs.h>
#include <linux/time.h>
#include <linux/uaccess.h>
#include <linux/sched/clock.h>

#include "displayport.h"

#include "dp_unit_test.h"

int unit_edid_find_resolution(u16 xres, u16 yres, u16 refresh)
{
	return edid_find_resolution(xres, yres, refresh);
}

bool unit_edid_set_preferred_preset(int mode)
{
	edid_set_preferred_preset(mode);
	return supported_videos[mode].edid_support_match;
}
int unit_displayport_init_sst_info(struct displayport_device *displayport)
{
	return displayport_init_sst_info(displayport);
}

u32 unit_edid_update(struct displayport_device *displayport)
{
	return edid_update(0, displayport);
}

int unit_edid_checksum(u8 *data, int block)
{
	return edid_checksum(data, block);
}

MODULE_DESCRIPTION("SEC Displaport unit test");
MODULE_LICENSE("GPL");
