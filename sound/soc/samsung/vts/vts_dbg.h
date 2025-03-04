/* SPDX-License-Identifier: GPL-2.0-or-later
 * sound/soc/samsung/vts/vts_dbg.h
 *
 * ALSA SoC - Samsung Abox Debug driver
 *
 * Copyright (c) 2019 Samsung Electronics Co. Ltd.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#ifndef __SND_SOC_VTS_DEBUG_H
#define __SND_SOC_VTS_DEBUG_H

#include "vts.h"

/**
 * Initialize vts debug driver
 * @return	dentry of vts debugfs root directory
 */
extern struct dentry *vts_dbg_get_root_dir(void);

#define vts_dev_info(dev, fmt, args...)	\
	do {	\
		struct vts_data *vdata;	\
		if (dev) {	\
			vdata = dev_get_drvdata(dev);	\
			dev_info(dev, fmt, ##args);	\
			if (vdata && vdata->kernel_log_obj)	\
				if (vdata->kernel_log_obj->enabled &&	\
					vdata->kernel_log_obj->log_level \
					>= MEMLOG_LEVEL_INFO)	\
					memlog_write_printf(	\
					vdata->kernel_log_obj,	\
						MEMLOG_LEVEL_INFO,	\
						fmt, ##args);	\
		}	\
	} while (0)

#define vts_dev_dbg(dev, fmt, args...)	\
	do {	\
		struct vts_data *vdata;	\
		if (dev) {	\
			vdata = dev_get_drvdata(dev);	\
			dev_dbg(dev, fmt, ##args);	\
			if (vdata && vdata->kernel_log_obj)	\
				if (vdata->kernel_log_obj->enabled &&	\
					vdata->kernel_log_obj->log_level \
					>= MEMLOG_LEVEL_DEBUG)	\
					memlog_write_printf(	\
					vdata->kernel_log_obj,	\
						MEMLOG_LEVEL_DEBUG,	\
						fmt, ##args);	\
		}	\
	} while (0)

#define vts_dev_warn(dev, fmt, args...)	\
	do {	\
		struct vts_data *vdata;	\
		if (dev) {	\
			vdata = dev_get_drvdata(dev);	\
			dev_warn(dev, fmt, ##args);	\
			if (vdata && vdata->kernel_log_obj)	\
				if (vdata->kernel_log_obj->enabled &&	\
					vdata->kernel_log_obj->log_level \
					>= MEMLOG_LEVEL_CAUTION)	\
					memlog_write_printf(	\
					vdata->kernel_log_obj,	\
						MEMLOG_LEVEL_CAUTION,	\
						fmt, ##args);	\
		}	\
	} while (0)

#define vts_dev_err(dev, fmt, args...)	\
	do {	\
		struct vts_data *vdata;	\
		if (dev) {	\
			vdata = dev_get_drvdata(dev);	\
		dev_err(dev, fmt, ##args);	\
		if (vdata && vdata->kernel_log_obj)	\
			if (vdata->kernel_log_obj->enabled &&	\
				vdata->kernel_log_obj->log_level \
				>= MEMLOG_LEVEL_ERR)	\
				memlog_write_printf(	\
				vdata->kernel_log_obj,	\
					MEMLOG_LEVEL_ERR,	\
					fmt, ##args);	\
		}	\
	} while (0)

#endif /* __SND_SOC_VTS_DEBUG_H */
