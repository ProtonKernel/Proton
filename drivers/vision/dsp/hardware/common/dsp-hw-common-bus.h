/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Samsung Exynos SoC series dsp driver
 *
 * Copyright (c) 2020 Samsung Electronics Co., Ltd.
 *              http://www.samsung.com/
 */

#ifndef __HW_COMMON_DSP_HW_COMMON_BUS_H__
#define __HW_COMMON_DSP_HW_COMMON_BUS_H__

#include "hardware/dsp-bus.h"

int dsp_hw_common_bus_probe(struct dsp_system *sys);
void dsp_hw_common_bus_remove(struct dsp_bus *bus);

#endif
