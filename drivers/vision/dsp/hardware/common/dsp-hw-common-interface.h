/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Samsung Exynos SoC series dsp driver
 *
 * Copyright (c) 2020 Samsung Electronics Co., Ltd.
 *              http://www.samsung.com/
 */

#ifndef __HW_COMMON_DSP_HW_COMMON_INTERFACE_H__
#define __HW_COMMON_DSP_HW_COMMON_INTERFACE_H__

#include "hardware/dsp-interface.h"

int dsp_hw_common_interface_probe(struct dsp_system *sys);
void dsp_hw_common_interface_remove(struct dsp_interface *itf);

#endif
