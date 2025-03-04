/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Samsung Exynos SoC series dsp driver
 *
 * Copyright (c) 2020 Samsung Electronics Co., Ltd.
 *              http://www.samsung.com/
 */

#ifndef __HW_COMMON_DSP_HW_COMMON_PM_H__
#define __HW_COMMON_DSP_HW_COMMON_PM_H__

#include "hardware/dsp-pm.h"

int dsp_hw_common_pm_probe(struct dsp_system *sys);
void dsp_hw_common_pm_remove(struct dsp_pm *pm);

#endif
