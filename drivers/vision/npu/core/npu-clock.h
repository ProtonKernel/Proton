/*
 * Samsung Exynos SoC series NPU driver
 *
 * Copyright (c) 2019 Samsung Electronics Co., Ltd.
 *              http://www.samsung.com/
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 */

#ifndef _NPU_CLOCK_H_
#define _NPU_CLOCK_H_

#include <linux/module.h>
#include <linux/platform_device.h>
#include <linux/device.h>
#include <linux/clk.h>
#include <linux/of.h>
#include <linux/clk-provider.h>

struct npu_clocks {
	struct clk		**clocks;
	int			clk_count;
};

#include "npu-log.h"
#include "npu-device.h"

int npu_clk_get(struct npu_clocks *clocks, struct device *dev);
void npu_clk_put(struct npu_clocks *clocks, struct device *dev);
int npu_clk_prepare_enable(struct npu_clocks *clocks);
void npu_clk_disable_unprepare(struct npu_clocks *clocks);
#endif
