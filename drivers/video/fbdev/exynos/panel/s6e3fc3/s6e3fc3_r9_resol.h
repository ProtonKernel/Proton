/*
 * linux/drivers/video/fbdev/exynos/panel/s6e3fc3/s6e3fc3_r9_resol.h
 *
 * Header file for Panel Driver
 *
 * Copyright (c) 2019 Samsung Electronics
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#ifndef __S6E3FC3_R9_RESOL_H__
#define __S6E3FC3_R9_RESOL_H__

#include <dt-bindings/display/panel-display.h>
#include "../panel.h"
#include "s6e3fc3.h"
#include "s6e3fc3_dimming.h"

struct panel_vrr s6e3fc3_r9_default_panel_vrr[] = {
	[S6E3FC3_VRR_120HS] = {
		.fps = 120,
		.base_fps = 120,
		.base_vactive = 2340,
		.base_vfp = 16,
		.base_vbp = 16,
		.te_sel = true,
		.te_v_st = 2341,
		.te_v_ed = 9,
		.te_sw_skip_count = 0,
		.te_hw_skip_count = 0,
		.mode = VRR_HS_MODE,
	},
	[S6E3FC3_VRR_60HS] = {
		.fps = 60,
		.base_fps = 60,
		.base_vactive = 2340,
		.base_vfp = 16,
		.base_vbp = 16,
		.te_sel = true,
		.te_v_st = 2341,
		.te_v_ed = 9,
		.te_sw_skip_count = 0,
		.te_hw_skip_count = 0,
		.mode = VRR_HS_MODE,
	},
};

static struct panel_vrr *s6e3fc3_r9_default_vrrtbl[] = {
	&s6e3fc3_r9_default_panel_vrr[S6E3FC3_VRR_120HS],
	&s6e3fc3_r9_default_panel_vrr[S6E3FC3_VRR_60HS],
};

static struct panel_resol s6e3fc3_r9_default_resol[] = {
	[S6E3FC3_RESOL_1080x2340] = {
		.w = 1080,
		.h = 2340,
		.comp_type = PN_COMP_TYPE_DSC,
		.comp_param = {
			.dsc = {
				.slice_w = 540,
				.slice_h = 30,
			},
		},
		.available_vrr = s6e3fc3_r9_default_vrrtbl,
		.nr_available_vrr = ARRAY_SIZE(s6e3fc3_r9_default_vrrtbl),
	},
};

#if defined(CONFIG_PANEL_DISPLAY_MODE)
static struct common_panel_display_mode s6e3fc3_r9_display_mode[MAX_S6E3FC3_DISPLAY_MODE] = {
	/* FHD */
	[S6E3FC3_DISPLAY_MODE_1080x2340_120HS] = {
		.name = PANEL_DISPLAY_MODE_1080x2340_120HS,
		.resol = &s6e3fc3_r9_default_resol[S6E3FC3_RESOL_1080x2340],
		.vrr = &s6e3fc3_r9_default_panel_vrr[S6E3FC3_VRR_120HS],
	},
	[S6E3FC3_DISPLAY_MODE_1080x2340_60HS] = {
		.name = PANEL_DISPLAY_MODE_1080x2340_60HS,
		.resol = &s6e3fc3_r9_default_resol[S6E3FC3_RESOL_1080x2340],
		.vrr = &s6e3fc3_r9_default_panel_vrr[S6E3FC3_VRR_60HS],
	},
};

static struct common_panel_display_mode *s6e3fc3_r9_display_mode_array[MAX_S6E3FC3_DISPLAY_MODE] = {
	[S6E3FC3_DISPLAY_MODE_1080x2340_120HS] = &s6e3fc3_r9_display_mode[S6E3FC3_DISPLAY_MODE_1080x2340_120HS],
	[S6E3FC3_DISPLAY_MODE_1080x2340_60HS] = &s6e3fc3_r9_display_mode[S6E3FC3_DISPLAY_MODE_1080x2340_60HS],
};

static struct common_panel_display_modes s6e3fc3_r9_display_modes = {
	.num_modes = ARRAY_SIZE(s6e3fc3_r9_display_mode_array),
	.modes = (struct common_panel_display_mode **)&s6e3fc3_r9_display_mode_array,
};
#endif /* CONFIG_PANEL_DISPLAY_MODE */
#endif /* __S6E3FC3_R9_RESOL_H__ */
