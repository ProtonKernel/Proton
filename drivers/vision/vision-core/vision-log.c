/*
 * Samsung Exynos SoC series VPU driver
 *
 * Copyright (c) 2015 Samsung Electronics Co., Ltd
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#include <stdarg.h>

#include "include/vision-config.h"

/* Log level to filter message written to kernel log */
static struct vision_log vision_log = {
	.pr_level = MEMLOG_LEVEL_NOTICE,	/* To kmsg */
	.memlog_desc_log = NULL,
	.vision_memlog_obj = NULL,
	.vision_memfile_obj = NULL,
};

void vision_memlog_store(int loglevel, const char *fmt, ...)
{
	char vision_string[1024];

	va_list ap;

	va_start(ap, fmt);
	vsprintf(vision_string, fmt, ap);
	va_end(ap);

		memlog_write_printf(vision_log.vision_memlog_obj, loglevel, vision_string);
}

/* Exported functions */
int vision_log_probe(struct vision_device *vision_device)
{
	int ret = 0;

	vprobe_info("start in %s\n", __func__);

	/* Basic initialization of log store */
	vision_log.dev = &vision_device->dev;

	/* Register NPU Device Driver for Unified Logging */
	ret = memlog_register("NPU_DRV2", &vision_device->dev, &vision_log.memlog_desc_log);
	if (ret)
		vprobe_err("memlog_register() for log failed: ret = %d\n", ret);

	/* Receive allocation of memory for saving data to vendor storage */
	vision_log.vision_memfile_obj = memlog_alloc_file(vision_log.memlog_desc_log, "vis-fil",
						SZ_128K*2, SZ_128K*2, 500, 1);
	if (vision_log.vision_memfile_obj) {
		vision_log.vision_memlog_obj = memlog_alloc_printf(vision_log.memlog_desc_log, SZ_128K,
						vision_log.vision_memfile_obj, "vis-mem", 0);
		if (!vision_log.vision_memlog_obj)
			vprobe_err("memlog_alloc_printf() failed\n");
	} else
		vprobe_err("memlog_alloc_file() failed\n");

	vprobe_info("complete in %s\n", __func__);

	return ret;
}
