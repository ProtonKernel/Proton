/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Samsung Exynos SoC series dsp driver
 *
 * Copyright (c) 2020 Samsung Electronics Co., Ltd.
 *              http://www.samsung.com/
 */

#ifndef __HW_COMMON_DSP_HW_COMMON_DEBUG_H__
#define __HW_COMMON_DSP_HW_COMMON_DEBUG_H__

#include "hardware/dsp-debug.h"

int dsp_hw_common_debug_probe(struct dsp_device *dspdev);
void dsp_hw_common_debug_remove(struct dsp_hw_debug *debug);

#endif
