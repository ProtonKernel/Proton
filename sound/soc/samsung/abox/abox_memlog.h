/* sound/soc/samsung/abox/abox_memlog.h
 *
 * Samsung Abox Logging API with memlog
 *
 * Copyright (c) 2020 Samsung Electronics Co. Ltd.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */
#ifndef __SND_SOC_ABOX_MEMLOG_H
#define __SND_SOC_ABOX_MEMLOG_H

#include <linux/device.h>
#include <soc/samsung/memlogger.h>
#include <soc/samsung/sysevent.h>
#include "abox.h"

#define abox_dbg(dev, fmt, args...)                             	\
        do {                                                    	\
	    struct abox_data *abox_data = abox_get_data(dev);		\
            dev_dbg(dev, "" fmt, ##args);  				\
	    if (dev && abox_data && abox_data->drv_log_obj &&		\
			    abox_data->drv_log_obj->enabled)		\
            memlog_write_printf(abox_data->drv_log_obj,   		\
                                MEMLOG_LEVEL_DEBUG,             	\
                                "[%s] %s: " fmt, dev_name(dev),		\
				__func__, ##args);  			\
        } while (0)

#define abox_info(dev, fmt, args...)                            	\
        do {                                                    	\
	    struct abox_data *abox_data = abox_get_data(dev);		\
            dev_info(dev, "" fmt, ##args);				\
	    if (dev && abox_data && abox_data->drv_log_obj &&		\
			    abox_data->drv_log_obj->enabled)		\
            memlog_write_printf(abox_data->drv_log_obj,   		\
                                MEMLOG_LEVEL_INFO,              	\
                                "[%s] %s: " fmt, dev_name(dev),		\
				__func__, ##args);  			\
        } while (0)

#define abox_warn(dev, fmt, args...)                            	\
        do {                                                    	\
	    struct abox_data *abox_data = abox_get_data(dev);		\
            dev_warn(dev, "" fmt, ##args);  				\
	    if (dev && abox_data && abox_data->drv_log_obj &&		\
			    abox_data->drv_log_obj->enabled)		\
            memlog_write_printf(abox_data->drv_log_obj,   		\
                                MEMLOG_LEVEL_CAUTION,           	\
                                "[%s] %s: " fmt, dev_name(dev),		\
				__func__, ##args);  			\
        } while (0)

#define abox_err(dev, fmt, args...)                             	\
        do {                                                    	\
	    struct abox_data *abox_data = abox_get_data(dev);		\
            dev_err(dev, "" fmt, ##args);  				\
	    if (dev && abox_data && abox_data->drv_log_obj &&		\
			    abox_data->drv_log_obj->enabled)		\
            memlog_write_printf(abox_data->drv_log_obj,   		\
                                MEMLOG_LEVEL_ERR,               	\
                                "[%s] %s: " fmt, dev_name(dev),		\
				__func__, ##args);  			\
        } while (0)

#define abox_sysevent_get(dev)                              		\
	do {                                                    	\
	    struct abox_data *abox_data = abox_get_data(dev);		\
	    if (abox_data->sysevent_dev)				\
		sysevent_get(abox_data->sysevent_desc.name);		\
        } while (0)

#define abox_sysevent_put(dev)                              		\
	do {                                                    	\
	    struct abox_data *abox_data = abox_get_data(dev);		\
	    if (abox_data->sysevent_dev)				\
		sysevent_put(abox_data->sysevent_dev);			\
        } while (0)

#endif /* __SND_SOC_ABOX_MEMLOG_H */

