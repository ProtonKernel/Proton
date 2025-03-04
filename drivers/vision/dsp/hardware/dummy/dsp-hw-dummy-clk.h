/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Samsung Exynos SoC series dsp driver
 *
 * Copyright (c) 2020 Samsung Electronics Co., Ltd.
 *              http://www.samsung.com/
 */

#ifndef __HW_DUMMY_DSP_HW_DUMMY_CLK_H__
#define __HW_DUMMY_DSP_HW_DUMMY_CLK_H__

#include "hardware/dsp-clk.h"

void dsp_hw_dummy_clk_dump(struct dsp_clk *clk);
void dsp_hw_dummy_clk_user_dump(struct dsp_clk *clk, struct seq_file *file);

int dsp_hw_dummy_clk_enable(struct dsp_clk *clk);
int dsp_hw_dummy_clk_disable(struct dsp_clk *clk);

int dsp_hw_dummy_clk_open(struct dsp_clk *clk);
int dsp_hw_dummy_clk_close(struct dsp_clk *clk);
int dsp_hw_dummy_clk_probe(struct dsp_system *sys);
void dsp_hw_dummy_clk_remove(struct dsp_clk *clk);

#endif
