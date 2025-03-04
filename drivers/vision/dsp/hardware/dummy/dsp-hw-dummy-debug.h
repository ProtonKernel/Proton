/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Samsung Exynos SoC series dsp driver
 *
 * Copyright (c) 2020 Samsung Electronics Co., Ltd.
 *              http://www.samsung.com/
 */

#ifndef __HW_DUMMY_DSP_HW_DUMMY_DEBUG_H__
#define __HW_DUMMY_DSP_HW_DUMMY_DEBUG_H__

#include "hardware/dsp-debug.h"

void dsp_hw_dummy_debug_log_flush(struct dsp_hw_debug *debug);
int dsp_hw_dummy_debug_log_start(struct dsp_hw_debug *debug);
int dsp_hw_dummy_debug_log_stop(struct dsp_hw_debug *debug);

int dsp_hw_dummy_debug_open(struct dsp_hw_debug *debug);
int dsp_hw_dummy_debug_close(struct dsp_hw_debug *debug);
int dsp_hw_dummy_debug_probe(struct dsp_device *dspdev);
void dsp_hw_dummy_debug_remove(struct dsp_hw_debug *debug);

#endif
