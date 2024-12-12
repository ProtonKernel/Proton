// SPDX-License-Identifier: GPL-2.0-or-later
/* sound/soc/samsung/vts/vts_dbg.c
 *
 * ALSA SoC Audio Layer - Samsung Abox Debug driver
 *
 * Copyright (c) 2019 Samsung Electronics Co. Ltd.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */
/* #define DEBUG */
#include <linux/io.h>
#include <linux/device.h>
#include <linux/debugfs.h>
#include <linux/module.h>
#include <linux/iommu.h>
#include <linux/of_reserved_mem.h>
#include <linux/pm_runtime.h>
#include <linux/sched/clock.h>
#include <linux/mm_types.h>
#include <asm/cacheflush.h>
#include "vts_dbg.h"

static struct dentry *vts_dbg_root_dir __read_mostly;

struct dentry *vts_dbg_get_root_dir(void)
{
	pr_info("%s\n", __func__);

	if (vts_dbg_root_dir == NULL)
		vts_dbg_root_dir = debugfs_create_dir("vts", NULL);

	return vts_dbg_root_dir;
}
EXPORT_SYMBOL(vts_dbg_get_root_dir);

