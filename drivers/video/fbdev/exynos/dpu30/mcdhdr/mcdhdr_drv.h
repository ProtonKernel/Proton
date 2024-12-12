/*
 * Copyright (c) 2016 Samsung Electronics Co., Ltd.
 *		http://www.samsung.com
 *
 * Samsung EXYNOS MCD HDR driver

 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#ifndef __HDR_DRV_H__
#define __HDR_DRV_H__

#include <linux/delay.h>
#include <linux/sched.h>
#include <linux/spinlock.h>
#include <linux/types.h>
#include <linux/videodev2.h>
#include <media/v4l2-device.h>
#include <media/videobuf2-core.h>

#include <linux/io.h>
#include <linux/pm_runtime.h>
#include <linux/pm_qos.h>
#include <linux/of.h>
#include <linux/of_address.h>

#include "mcdhdr.h"
#include "../decon.h"


/* Registers for hdr ip*/


#define SDR_TOTAL_REG_CNT	340
#define HDR_TOTAL_REG_CNT	504

#define MCDHDR_REG_CON		0x0
#define MCDHDR_REG_CON_EN	0x1

#define MCDHDR_ADDR_RANGE	0x1000

#define HEADER_REG_OFFSET	0
#define HEADER_REG_LENGTH	1



#define HDR_WIN_CONFIG			_IOW('H', 0, struct mcdhdr_win_config *)
#define HDR_STOP			_IOW('H', 11, u32)
#define HDR_DUMP			_IOW('H', 12, u32)
#define HDR_RESET			_IOW('H', 14, u32)
#define HDR_GET_CAPA			_IOR('H', 21, u32)

int mcdhdr_probe_sysfs(struct mcdhdr_device *mcdhdr);

#endif
