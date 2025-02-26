// SPDX-License-Identifier: GPL-2.0
/*
 * Samsung Exynos SoC series Pablo driver
 *
 * Copyright (c) 2020 Samsung Electronics Co., Ltd
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#if defined(CONFIG_S3C2410_WATCHDOG)
#include <soc/samsung/exynos-debug.h>
#elif defined(CONFIG_DEBUG_SNAPSHOT_MODULE)
#include <soc/samsung/debug-snapshot.h>
#endif

#include "camerapp-debug.h"

void camerapp_debug_s2d(bool en_s2d, const char *fmt, ...)
{
	static char buf[1024];
	va_list args;

	va_start(args, fmt);
	vsnprintf(buf, sizeof(buf), fmt, args);
	va_end(args);

	if (en_s2d) {
		pr_err("[DBG] S2D!!!: %s", buf);
		dump_stack();
#if defined(CONFIG_S3C2410_WATCHDOG)
		s3c2410wdt_set_emergency_reset(100, 0);
#elif defined(CONFIG_DEBUG_SNAPSHOT_MODULE)
		dbg_snapshot_expire_watchdog();
#else
		panic("S2D is not enabled.", buf);
#endif
	} else {
		panic(buf);
	}
}
EXPORT_SYMBOL_GPL(camerapp_debug_s2d);
MODULE_LICENSE("GPL");
