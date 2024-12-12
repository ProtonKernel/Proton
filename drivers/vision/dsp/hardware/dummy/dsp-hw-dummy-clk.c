// SPDX-License-Identifier: GPL-2.0
/*
 * Samsung Exynos SoC series dsp driver
 *
 * Copyright (c) 2020 Samsung Electronics Co., Ltd.
 *              http://www.samsung.com/
 */

#include "dsp-log.h"
#include "dsp-hw-dummy-system.h"
#include "dsp-hw-dummy-clk.h"

void dsp_hw_dummy_clk_dump(struct dsp_clk *clk)
{
	dsp_enter();
	dsp_leave();
}

void dsp_hw_dummy_clk_user_dump(struct dsp_clk *clk, struct seq_file *file)
{
	dsp_enter();
	dsp_leave();
}

int dsp_hw_dummy_clk_enable(struct dsp_clk *clk)
{
	dsp_enter();
	dsp_leave();
	return 0;
}

int dsp_hw_dummy_clk_disable(struct dsp_clk *clk)
{
	dsp_enter();
	dsp_leave();
	return 0;
}

int dsp_hw_dummy_clk_open(struct dsp_clk *clk)
{
	dsp_enter();
	dsp_leave();
	return 0;
}

int dsp_hw_dummy_clk_close(struct dsp_clk *clk)
{
	dsp_enter();
	dsp_leave();
	return 0;
}

int dsp_hw_dummy_clk_probe(struct dsp_system *sys)
{
	dsp_enter();
	dsp_leave();
	return 0;
}

void dsp_hw_dummy_clk_remove(struct dsp_clk *clk)
{
	dsp_enter();
	dsp_leave();
}
