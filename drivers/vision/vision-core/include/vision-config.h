/*
 * Samsung Exynos SoC series VPU driver
 *
 * Copyright (c) 2015 Samsung Electronics Co., Ltd
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */
#ifndef VISION_CONFIG_H_
#define VISION_CONFIG_H_

#include <soc/samsung/memlogger.h>

#include "vision-dev.h"

#define VISION_MAX_CONTAINERLIST	16
#define VISION_MAX_BUFFER		16
#define VISION_MAX_PLANE		3
#define VISION_MAP_KVADDR

#define vprobe_info(fmt, ...)		pr_info("[V]" fmt, ##__VA_ARGS__)
#define vprobe_warn(fmt, args...)	pr_warning("[V][WRN]" fmt, ##args)
#define vprobe_err(fmt, args...)	pr_err("[V][ERR]%s:%d:" fmt, __func__, __LINE__, ##args)

struct vision_log {
	struct device *dev;
	/* kernel logger level */
	int pr_level;

	/* memlog for Unified Logging System*/
	struct memlog *memlog_desc_log;
	struct memlog_obj *vision_memlog_obj;
	struct memlog_obj *vision_memfile_obj;
};

int vision_log_probe(struct vision_device *vision_device);
void vision_memlog_store(int loglevel, const char *fmt, ...);

#ifdef DEBUG_LOG_MEMORY
#define vision_err_target(fmt, ...)	pr_debug(fmt, ##__VA_ARGS__)
#define vision_warn_target(fmt, ...)	pr_debug(fmt, ##__VA_ARGS__)
#define vision_info_target(fmt, ...)	pr_debug(fmt, ##__VA_ARGS__)
#define vision_dbg_target(fmt, ...)	pr_debug(fmt, ##__VA_ARGS__)
#else
#define vision_err_target(fmt, ...)	pr_err(fmt, ##__VA_ARGS__)
#define vision_info_target(fmt, ...)	pr_info(fmt, ##__VA_ARGS__)
#define vision_dbg_target(fmt, ...)	pr_debug(fmt, ##__VA_ARGS__)
#endif

#define vision_err(fmt, args...) \
	((console_printk[0] > MEMLOG_LEVEL_CAUTION) ? vision_memlog_store(MEMLOG_LEVEL_CAUTION, "[V][ERR]%s:%d:" fmt, __func__, __LINE__, ##args) : 0)

#define vision_info(fmt, args...) \
	((console_printk[0] > MEMLOG_LEVEL_CAUTION) ? vision_memlog_store(MEMLOG_LEVEL_CAUTION, "[V]" fmt, ##args) : 0)

#define vision_dbg(fmt, args...) \
	((console_printk[0] > MEMLOG_LEVEL_INFO) ? vision_memlog_store(MEMLOG_LEVEL_INFO, "[V]" fmt, ##args) : 0)

#endif
