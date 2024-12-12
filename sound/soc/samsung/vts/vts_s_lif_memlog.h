/* sound/soc/samsung/vts/vts_memlog.h
 *
 * ALSA SoC - Samsung Abox Debug driver
 *
 * Copyright (c) 2020 Samsung Electronics Co. Ltd.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#ifndef __SND_SOC_VTS_S_LIF_LOG_H
#define __SND_SOC_VTS_S_LIF_LOG_H

#include "vts.h"


#define DEBUG_LOG_MEMORY
#if defined(DEBUG_LOG_MEMORY)
#define slif_log_err(fmt, ...)		printk(KERN_ERR fmt, ##__VA_ARGS__)
#define slif_log_warn(fmt, ...)		printk(KERN_WARNING fmt, ##__VA_ARGS__)
#define slif_log_info(fmt, ...)		printk(KERN_INFO fmt, ##__VA_ARGS__)
#define slif_log_cont(fmt, ...)		printk(KERN_CONT fmt, ##__VA_ARGS__)
#ifdef DEBUG
#define slif_log_dbg(fmt, ...)		printk(KERN_DEBUG fmt, ##__VA_ARGS__)
#else
#define slif_log_dbg(fmt, ...)
#endif
#else
#define slif_log_err(fmt, ...)		pr_err(fmt, ##__VA_ARGS__)
#define slif_log_warn(fmt, ...)		pr_warning(fmt, ##__VA_ARGS__)
#define slif_log_dbg(fmt, ...)		pr_info(fmt, ##__VA_ARGS__)
#define slif_log_info(fmt, ...)		pr_info(fmt, ##__VA_ARGS__)
#define slif_log_cont(fmt, ...)		pr_cont(fmt, ##__VA_ARGS__)
#endif

#define slif_log_dev_info(prefix, fmt, args...)				\
		slif_log_info("[slif]" prefix fmt, ##args)

#define slif_log_dev_dbg(prefix, fmt, args...)				\
		slif_log_dbg("[slif]" prefix fmt, ##args)

#define slif_log_dev_warn(prefix, fmt, args...)				\
		slif_log_warn("[slif]" prefix fmt, ##args)

#define slif_log_dev_err(prefix, fmt, args...)				\
		slif_log_err("[slif]" prefix fmt, ##args)

#define slif_info(dev, fmt, args...)					\
do {									\
struct vts_data *vdata;							\
struct vts_s_lif_data *data;						\
if (dev) {								\
	vdata = vts_get_data(dev);					\
	data = dev_get_drvdata(dev);					\
	slif_log_dev_info("%s:%d:", fmt, __func__, __LINE__, ##args);	\
	if (vdata && data &&						\
		vdata->kernel_log_obj && vdata->kernel_log_obj->enabled)\
		memlog_write_printf(vdata->kernel_log_obj,		\
			MEMLOG_LEVEL_INFO,				\
			"[%s][%d]%s:%d:" fmt, "slif", data->id,		\
			__func__, __LINE__, ##args);			\
	}								\
} while(0)

#define slif_dbg(dev, fmt, args...)					\
do {									\
struct vts_data *vdata;							\
struct vts_s_lif_data *data;						\
if (dev) {								\
	vdata = vts_get_data(dev);					\
	data = dev_get_drvdata(dev);					\
	slif_log_dev_dbg("%s:%d:", fmt, __func__, __LINE__, ##args);	\
	if (vdata && data &&						\
		vdata->kernel_log_obj && vdata->kernel_log_obj->enabled)\
		memlog_write_printf(vdata->kernel_log_obj,		\
			MEMLOG_LEVEL_DEBUG,				\
			"[%s][%d]%s:%d:" fmt, "slif", data->id,		\
				__func__, __LINE__, ##args);		\
	}								\
} while(0)

#define slif_warn(dev, fmt, args...)					\
do {									\
struct vts_data *vdata;							\
struct vts_s_lif_data *data;						\
if (dev) {								\
	vdata = vts_get_data(dev);					\
	data = dev_get_drvdata(dev);					\
	slif_log_dev_warn("%s:%d:", fmt, __func__, __LINE__, ##args);	\
	if (vdata && data &&						\
		vdata->kernel_log_obj && vdata->kernel_log_obj->enabled)\
		memlog_write_printf(vdata->kernel_log_obj,		\
			MEMLOG_LEVEL_CAUTION,				\
			"[%s][%d]%s:%d:" fmt, "slif", data->id,		\
			__func__, __LINE__, ##args);			\
	}								\
} while(0)

#define slif_err(dev, fmt, args...)					\
do {									\
struct vts_data *vdata;							\
struct vts_s_lif_data *data;						\
if (dev) {								\
	vdata = vts_get_data(dev);					\
	data = dev_get_drvdata(dev);					\
	slif_log_dev_err("%s:%d:", fmt, __func__, __LINE__, ##args);	\
	if (vdata && data &&						\
		vdata->kernel_log_obj && vdata->kernel_log_obj->enabled)\
		memlog_write_printf(vdata->kernel_log_obj,		\
			MEMLOG_LEVEL_ERR,				\
			"[%s][%d]%s:%d:" fmt, "slif", data->id,		\
			__func__, __LINE__, ##args);			\
	}								\
} while(0)

#endif /* __SND_SOC_VTS_S_LIF_LOG_H */
