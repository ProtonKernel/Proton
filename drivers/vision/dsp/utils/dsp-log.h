/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Samsung Exynos SoC series dsp driver
 *
 * Copyright (c) 2019 Samsung Electronics Co., Ltd.
 *              http://www.samsung.com/
 */

#ifndef __DSP_LOG_H__
#define __DSP_LOG_H__

#include <linux/device.h>
#include <linux/printk.h>
#if IS_ENABLED(CONFIG_EXYNOS_MEMORY_LOGGER)
#include <soc/samsung/memlogger.h>
#endif

#include "dsp-config.h"

#define DSP_LOG_TAG	"exynos-dsp"

#if IS_ENABLED(CONFIG_EXYNOS_MEMORY_LOGGER)
#define dsp_err(fmt, args...)						\
do {									\
	pr_err("%s: ERR:%4d:" fmt, DSP_LOG_TAG, __LINE__, ##args);	\
	dsp_memlog_printf(MEMLOG_LEVEL_ERR, "%s: ERR:%4d:" fmt,		\
			DSP_LOG_TAG, __LINE__, ##args);			\
} while (0)

#define dsp_warn(fmt, args...)						\
do {									\
	pr_warn("%s: WAR:%4d:" fmt, DSP_LOG_TAG, __LINE__, ##args);	\
	dsp_memlog_printf(MEMLOG_LEVEL_CAUTION, "%s: WAR:%4d:" fmt,	\
			DSP_LOG_TAG, __LINE__, ##args);			\
} while (0)

#define dsp_notice(fmt, args...)					\
do {									\
	pr_notice("%s: NOT:%4d:" fmt, DSP_LOG_TAG, __LINE__, ##args);	\
	dsp_memlog_printf(MEMLOG_LEVEL_CAUTION, "%s: NOT:%4d:" fmt,	\
			DSP_LOG_TAG, __LINE__, ##args);			\
} while (0)

#if defined(ENABLE_INFO_LOG)
#define dsp_info(fmt, args...)						\
do {									\
	pr_info("%s: INF:%4d:" fmt, DSP_LOG_TAG, __LINE__, ##args);	\
	dsp_memlog_printf(MEMLOG_LEVEL_CAUTION, "%s: INF:%4d:" fmt,	\
			DSP_LOG_TAG, __LINE__, ##args);			\
} while (0)
#else
#define dsp_info(fmt, args...)						\
	dsp_memlog_printf(MEMLOG_LEVEL_NOTICE, "%s: INF:%4d:" fmt,	\
			DSP_LOG_TAG, __LINE__, ##args)
#endif

#if defined(ENABLE_DYNAMIC_DEBUG_LOG)
#define dsp_dbg(fmt, args...)						\
do {									\
	if (dsp_log_get_debug_ctrl() & 0x1)				\
		dsp_info("DBG:" fmt, ##args);				\
	else								\
		pr_debug("%s: DBG:%4d:" fmt, DSP_LOG_TAG, __LINE__,	\
				##args);				\
	dsp_memlog_printf(MEMLOG_LEVEL_DEBUG, "%s: DBG:%4d:" fmt,	\
			DSP_LOG_TAG, __LINE__, ##args);			\
} while (0)
#define dsp_dl_dbg(fmt, args...)					\
do {									\
	if (dsp_log_get_debug_ctrl() & 0x2)				\
		dsp_info("DL-DBG:" fmt, ##args);			\
	else								\
		pr_debug("%s: DL-DBG:%4d:" fmt, DSP_LOG_TAG, __LINE__,	\
				##args);				\
	dsp_memlog_printf(MEMLOG_LEVEL_DEBUG, "%s: DL-DBG:%4d:" fmt,	\
			DSP_LOG_TAG, __LINE__, ##args);			\
} while (0)
#else  // ENABLE_DYNAMIC_DEBUG_LOG
#define dsp_dbg(fmt, args...)						\
	pr_debug("%s: DBG:%4d:" fmt, DSP_LOG_TAG, __LINE__, ##args)
#define dsp_dl_dbg(fmt, args...)					\
	pr_debug("%s: DL-DBG:%4d:" fmt, DSP_LOG_TAG, __LINE__, ##args)
#endif  // ENABLE_DYNAMIC_DEBUG_LOG

#else  // CONFIG_EXYNOS_MEMORY_LOGGER

#define dsp_err(fmt, args...)						\
	pr_err("%s: ERR:%4d:" fmt, DSP_LOG_TAG, __LINE__, ##args)
#define dsp_warn(fmt, args...)						\
	pr_warn("%s: WAR:%4d:" fmt, DSP_LOG_TAG, __LINE__, ##args)

#define dsp_notice(fmt, args...)					\
	pr_notice("%s: NOT:%4d:" fmt, DSP_LOG_TAG, __LINE__, ##args)

#if defined(ENABLE_INFO_LOG)
#define dsp_info(fmt, args...)						\
	pr_info("%s: INF:%4d:" fmt, DSP_LOG_TAG, __LINE__, ##args)
#else
#define dsp_info(fmt, args...)
#endif

#if defined(ENABLE_DYNAMIC_DEBUG_LOG)
#define dsp_dbg(fmt, args...)						\
do {									\
	if (dsp_log_get_debug_ctrl() & 0x1)				\
		dsp_info("DBG:" fmt, ##args);				\
	else								\
		pr_debug("%s: DBG:%4d" fmt, DSP_LOG_TAG, __LINE__,	\
				##args);				\
} while (0)
#define dsp_dl_dbg(fmt, args...)					\
do {									\
	if (dsp_log_get_debug_ctrl() & 0x2)				\
		dsp_info("DL-DBG:" fmt, ##args);			\
	else								\
		pr_debug("%s: DL-DBG:%4d:" fmt, DSP_LOG_TAG, __LINE__,	\
				##args);				\
} while (0)
#else  // ENABLE_DYNAMIC_DEBUG_LOG
#define dsp_dbg(fmt, args...)					\
	pr_debug("%s: DBG:%4d:" fmt, DSP_LOG_TAG, __LINE__, ##args)
#define dsp_dl_dbg(fmt, args...)				\
	pr_debug("%s: DL-DBG:%4d:" fmt, DSP_LOG_TAG, __LINE__, ##args)
#endif  // ENABLE_DYNAMIC_DEBUG_LOG
#endif  // CONFIG_EXYNOS_MEMORY_LOGGER

#if defined(ENABLE_CALL_PATH_LOG)
#define dsp_enter()		dsp_info("[%s] enter\n", __func__)
#define dsp_leave()		dsp_info("[%s] leave\n", __func__)
#define dsp_check()		dsp_info("[%s] check\n", __func__)
#else
#define dsp_enter()
#define dsp_leave()
#define dsp_check()
#endif

extern struct device *dsp_global_dev;
extern unsigned int dsp_debug_log_enable;
static inline unsigned int dsp_log_get_debug_ctrl(void)
{
	return dsp_debug_log_enable;
}

#if IS_ENABLED(CONFIG_EXYNOS_MEMORY_LOGGER)
extern struct memlog_obj *dsp_memlog_log_obj;
static inline void dsp_memlog_printf(int level, const char *fmt, ...)
{
	if (dsp_memlog_log_obj) {
		struct va_format vaf;
		va_list args;

		va_start(args, fmt);

		vaf.fmt = fmt;
		vaf.va = &args;

		memlog_write_printf(dsp_memlog_log_obj, level, "%pV", &vaf);

		va_end(args);
	}
}
#endif

#endif  // __DSP_LOG_H__
