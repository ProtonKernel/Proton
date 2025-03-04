/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Samsung Exynos SoC series dsp driver
 *
 * Copyright (c) 2019 Samsung Electronics Co., Ltd.
 *              http://www.samsung.com/
 */

#ifndef __HW_O1_DSP_HW_O1_BUS_H__
#define __HW_O1_DSP_HW_O1_BUS_H__

#include "hardware/dsp-bus.h"

enum dsp_o1_bus_scenario_id {
	DSP_O1_MO_MAX,
	DSP_O1_MO_CAMERA,
	DSP_O1_MO_SCENARIO_COUNT,
};

int dsp_hw_o1_bus_init(void);

#endif
