/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Samsung Exynos SoC series dsp driver
 *
 * Copyright (c) 2020 Samsung Electronics Co., Ltd.
 *              http://www.samsung.com/
 */

#ifndef __HW_O1_DSP_HW_O1_LLC_H__
#define __HW_O1_DSP_HW_O1_LLC_H__

#include "hardware/dsp-llc.h"

enum dsp_o1_llc_scenario_id {
#if IS_ENABLED(CONFIG_EXYNOS_SCI) && defined(CONFIG_EXYNOS_DSP_HW_O1)
	/*
	 * 2MB alloc for LLC_REGION_DSP0 (VPC0/VPC1 related DMA)
	 * 1MB alloc for LLC_REGION_DSP1 (VPC2      related Core)
	 */
	DSP_O1_LLC_CASE1,
#endif
	DSP_O1_LLC_SCENARIO_COUNT,
};

int dsp_hw_o1_llc_init(void);

#endif
