/* linux/drivers/video/fbdev/exynos/dpu/fcmd.h
 *
 * Copyright (c) 2016 Samsung Electronics Co., Ltd.
 *
 * header file for Samsung EXYNOS SoC DMA DSIM Fast Command driver
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#ifndef __SAMSUNG_FCMD_H__
#define __SAMSUNG_FCMD_H__

#include <linux/delay.h>
#include <linux/sched.h>
#include <linux/spinlock.h>
#include <linux/types.h>
#include <linux/videodev2.h>
#include <linux/io.h>
#include <linux/pm_runtime.h>
#include <soc/samsung/exynos_pm_qos.h>
#include <linux/of.h>
#include <linux/of_address.h>
#include <linux/ion.h>
#include <linux/dma-buf.h>

#include "decon.h"
#include "./cal_2100/regs-fcmd.h"
#include "./cal_2100/fcmd_cal.h"

extern int fcmd_log_level;

#define FCMD_MODULE_NAME	"exynos-fcmd"
#define MAX_FCMD_CNT		3


#define fcmd_err(fmt, ...)							\
	do {									\
		if (fcmd_log_level >= 3) {					\
			pr_err(pr_fmt(fmt), ##__VA_ARGS__);			\
		}								\
	} while (0)

#define fcmd_warn(fmt, ...)							\
	do {									\
		if (fcmd_log_level >= 4) {					\
			pr_warn(pr_fmt(fmt), ##__VA_ARGS__);			\
		}								\
	} while (0)

#define fcmd_info(fmt, ...)							\
	do {									\
		if (fcmd_log_level >= 6)					\
			pr_info(pr_fmt(fmt), ##__VA_ARGS__);			\
	} while (0)

#define fcmd_dbg(fmt, ...)							\
	do {									\
		if (fcmd_log_level >= 7)					\
			pr_info(pr_fmt(fmt), ##__VA_ARGS__);			\
	} while (0)


struct fcmd_resources {
	struct clk *aclk;
	void __iomem *regs;
	int irq;
};

struct fcmd_config {
	u8 bta;
	u8 type;
	u8 di;
	u8 cmd;
	u32 size;
	u32 unit;
	dma_addr_t buf;
	u8 done;
};

struct fcmd_device {
	int id;
	int port;
	struct device *dev;
	struct v4l2_subdev sd;
	struct fcmd_resources res;
	struct fcmd_config *fcmd_config;
	wait_queue_head_t xferdone_wq; /* herb : consider about necessity */
	spinlock_t slock;
	struct mutex lock;
};

extern struct fcmd_device *fcmd_drvdata[MAX_FCMD_CNT];

static inline struct fcmd_device *get_fcmd_drvdata(u32 id)
{
	return fcmd_drvdata[id];
}

#define call_fcmd_ops(q, op, args...)				\
	(((q)->ops->op) ? ((q)->ops->op(args)) : 0)

static inline u32 fcmd_read(u32 id, u32 reg_id)
{
	struct fcmd_device *fcmd = get_fcmd_drvdata(id);
	return readl(fcmd->res.regs + reg_id);
}

static inline u32 fcmd_read_mask(u32 id, u32 reg_id, u32 mask)
{
	u32 val = fcmd_read(id, reg_id);
	val &= (~mask);
	return val;
}

static inline void fcmd_write(u32 id, u32 reg_id, u32 val)
{
	struct fcmd_device *fcmd = get_fcmd_drvdata(id);
	writel(val, fcmd->res.regs + reg_id);
}

static inline void fcmd_write_mask(u32 id, u32 reg_id, u32 val, u32 mask)
{
	struct fcmd_device *fcmd = get_fcmd_drvdata(id);
	u32 old = fcmd_read(id, reg_id);

	val = (val & mask) | (old & ~mask);
	writel(val, fcmd->res.regs + reg_id);
}

void fcmd_dump(struct fcmd_device *fcmd);

#define FCMD_SET_CONFIG			_IOW('C', 0, struct fcmd_config)
#define FCMD_START			_IOW('C', 1, u32)
#define FCMD_STOP			_IOW('C', 2, u32)
#define FCMD_DUMP			_IOW('C', 3, u32)
#define FCMD_GET_PORT_NUM		_IOR('C', 4, u32)

#endif /* __SAMSUNG_FCMD_H__ */
