/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Samsung Exynos SoC series dsp driver
 *
 * Copyright (c) 2019 Samsung Electronics Co., Ltd.
 *              http://www.samsung.com/
 */

#ifndef __HW_DSP_CLK_H__
#define __HW_DSP_CLK_H__

#include <linux/device.h>
#include <linux/clk.h>

struct dsp_system;
struct dsp_clk;

struct dsp_clk_ops {
	void (*dump)(struct dsp_clk *clk);
	void (*user_dump)(struct dsp_clk *clk, struct seq_file *file);
	int (*enable)(struct dsp_clk *clk);
	int (*disable)(struct dsp_clk *clk);

	int (*open)(struct dsp_clk *clk);
	int (*close)(struct dsp_clk *clk);
	int (*probe)(struct dsp_system *sys);
	void (*remove)(struct dsp_clk *clk);
};

struct dsp_clk {
	struct device			*dev;
	struct dsp_clk_format		*array;
	unsigned int			array_size;

	const struct dsp_clk_ops	*ops;
};

struct dsp_clk_format {
	struct clk		*clk;
	const char		*name;
};

int dsp_clk_set_ops(struct dsp_clk *clk, unsigned int dev_id);
int dsp_clk_register_ops(unsigned int dev_id, const struct dsp_clk_ops *ops);

#endif
