/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Samsung Exynos SoC series dsp driver
 *
 * Copyright (c) 2020 Samsung Electronics Co., Ltd.
 *              http://www.samsung.com/
 */

#ifndef __HW_COMMON_DSP_HW_COMMON_CLK_H__
#define __HW_COMMON_DSP_HW_COMMON_CLK_H__

#include "hardware/dsp-clk.h"

int dsp_hw_common_clk_probe(struct dsp_system *sys);
void dsp_hw_common_clk_remove(struct dsp_clk *clk);

#endif
