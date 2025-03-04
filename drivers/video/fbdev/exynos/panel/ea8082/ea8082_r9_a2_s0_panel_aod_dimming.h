/*
 * linux/drivers/video/fbdev/exynos/panel/ea8082/ea8082_ea8082_a2_s0_panel_aod_dimming.h
 *
 * Header file for EA8082 Dimming Driver
 *
 * Copyright (c) 2017 Samsung Electronics
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#ifndef __EA8082_R9_A2_S0_PANEL_AOD_DIMMING_H__
#define __EA8082_R9_A2_S0_PANEL_AOD_DIMMING_H__
#include "../dimming.h"
#include "../panel_dimming.h"
#include "ea8082_dimming.h"

/*
 * PANEL INFORMATION
 * LDI : EA8082
 * PANEL : R9_A2_S0
 */
static unsigned int ea8082_a2_s0_aod_brt_tbl[EA8082_AOD_NR_LUMINANCE] = {
	BRT_LT(12), BRT_LT(32), BRT_LT(56), BRT(255),
};

static unsigned int ea8082_a2_s0_aod_lum_tbl[EA8082_AOD_NR_LUMINANCE] = {
	2, 10, 30, 60,
};

static struct brightness_table ea8082_r9_a2_s0_panel_aod_brightness_table = {
	.brt = ea8082_a2_s0_aod_brt_tbl,
	.sz_brt = ARRAY_SIZE(ea8082_a2_s0_aod_brt_tbl),
	.lum = ea8082_a2_s0_aod_lum_tbl,
	.sz_lum = ARRAY_SIZE(ea8082_a2_s0_aod_lum_tbl),
	.brt_to_step = ea8082_a2_s0_aod_brt_tbl,
	.sz_brt_to_step = ARRAY_SIZE(ea8082_a2_s0_aod_brt_tbl),
};

static struct panel_dimming_info ea8082_r9_a2_s0_panel_aod_dimming_info = {
	.name = "ea8082_a2_s0_aod",
	.target_luminance = EA8082_AOD_TARGET_LUMINANCE,
	.nr_luminance = EA8082_AOD_NR_LUMINANCE,
	.hbm_target_luminance = -1,
	.nr_hbm_luminance = 0,
	.extend_hbm_target_luminance = -1,
	.nr_extend_hbm_luminance = 0,
	.brt_tbl = &ea8082_r9_a2_s0_panel_aod_brightness_table,
};
#endif /* __EA8082_R9_A2_S0_PANEL_AOD_DIMMING_H__ */
