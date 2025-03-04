/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Samsung Exynos SoC series dsp driver
 *
 * Copyright (c) 2020 Samsung Electronics Co., Ltd.
 *              http://www.samsung.com/
 */

#ifndef __HW_COMMON_DSP_HW_COMMON_SYSTEM_H__
#define __HW_COMMON_DSP_HW_COMMON_SYSTEM_H__

#include "hardware/dsp-system.h"

int dsp_hw_common_system_probe(struct dsp_device *dspdev);
void dsp_hw_common_system_remove(struct dsp_system *sys);

#endif
