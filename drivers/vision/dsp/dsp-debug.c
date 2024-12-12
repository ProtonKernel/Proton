// SPDX-License-Identifier: GPL-2.0
/*
 * Samsung Exynos SoC series dsp driver
 *
 * Copyright (c) 2019 Samsung Electronics Co., Ltd
 */

#include <linux/debugfs.h>
#if IS_ENABLED(CONFIG_EXYNOS_MEMORY_LOGGER)
#include <soc/samsung/memlogger.h>
#endif

#include "dsp-log.h"
#include "dsp-device.h"
#include "dsp-debug.h"

unsigned int dsp_debug_log_enable;
#if IS_ENABLED(CONFIG_EXYNOS_MEMORY_LOGGER)
struct memlog_obj *dsp_memlog_log_obj;
#endif

int dsp_debug_open(struct dsp_debug *debug)
{
	dsp_enter();
	dsp_leave();
	return 0;
}

int dsp_debug_close(struct dsp_debug *debug)
{
	dsp_enter();
	dsp_leave();
	return 0;
}

int dsp_debug_probe(struct dsp_device *dspdev)
{
	int ret;
	struct dsp_debug *debug;

	dsp_enter();
	debug = &dspdev->debug;

	debug->root = debugfs_create_dir("dsp", NULL);
	if (!debug->root) {
		ret = -EFAULT;
		dsp_err("Failed to create debug root file\n");
		goto p_err_root;
	}

	debug->debug_log = debugfs_create_u32("debug_log", 0640, debug->root,
			&dsp_debug_log_enable);
	if (!debug->debug_log)
		dsp_warn("Failed to create debug_log debugfs file\n");

	dsp_leave();
	return 0;
p_err_root:
	return ret;
}

void dsp_debug_remove(struct dsp_debug *debug)
{
	debugfs_remove_recursive(debug->root);
}
