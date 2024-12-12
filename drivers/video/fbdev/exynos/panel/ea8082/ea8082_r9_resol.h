/*
 * linux/drivers/video/fbdev/exynos/panel/ea8082/ea8082_r9_resol.h
 *
 * Header file for Panel Driver
 *
 * Copyright (c) 2019 Samsung Electronics
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#ifndef __EA8082_R9_RESOL_H__
#define __EA8082_R9_RESOL_H__

#include <dt-bindings/display/panel-display.h>
#include "../panel.h"
#include "ea8082.h"
#include "ea8082_dimming.h"

struct panel_vrr ea8082_r9_default_panel_vrr[] = {
	[EA8082_VRR_120HS] = {
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
	[EA8082_VRR_60HS] = {
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

static struct panel_vrr *ea8082_r9_default_vrrtbl[] = {
	&ea8082_r9_default_panel_vrr[EA8082_VRR_120HS],
	&ea8082_r9_default_panel_vrr[EA8082_VRR_60HS],
};

static struct panel_resol ea8082_r9_default_resol[] = {
	[EA8082_RESOL_1080x2340] = {
		.w = 1080,
		.h = 2340,
		.comp_type = PN_COMP_TYPE_DSC,
		.comp_param = {
			.dsc = {
				.slice_w = 540,
				.slice_h = 30,
			},
		},
		.available_vrr = ea8082_r9_default_vrrtbl,
		.nr_available_vrr = ARRAY_SIZE(ea8082_r9_default_vrrtbl),
	},
};

#if defined(CONFIG_PANEL_DISPLAY_MODE)
static struct common_panel_display_mode ea8082_r9_display_mode[MAX_EA8082_DISPLAY_MODE] = {
	/* FHD */
	[EA8082_DISPLAY_MODE_1080x2340_120HS] = {
		.name = PANEL_DISPLAY_MODE_1080x2340_120HS,
		.resol = &ea8082_r9_default_resol[EA8082_RESOL_1080x2340],
		.vrr = &ea8082_r9_default_panel_vrr[EA8082_VRR_120HS],
	},
	[EA8082_DISPLAY_MODE_1080x2340_60HS] = {
		.name = PANEL_DISPLAY_MODE_1080x2340_60HS,
		.resol = &ea8082_r9_default_resol[EA8082_RESOL_1080x2340],
		.vrr = &ea8082_r9_default_panel_vrr[EA8082_VRR_60HS],
	},
};

static struct common_panel_display_mode *ea8082_r9_display_mode_array[MAX_EA8082_DISPLAY_MODE] = {
	[EA8082_DISPLAY_MODE_1080x2340_120HS] = &ea8082_r9_display_mode[EA8082_DISPLAY_MODE_1080x2340_120HS],
	[EA8082_DISPLAY_MODE_1080x2340_60HS] = &ea8082_r9_display_mode[EA8082_DISPLAY_MODE_1080x2340_60HS],
};

static struct common_panel_display_modes ea8082_r9_display_modes = {
	.num_modes = ARRAY_SIZE(ea8082_r9_display_mode_array),
	.modes = (struct common_panel_display_mode **)&ea8082_r9_display_mode_array,
};
#endif /* CONFIG_PANEL_DISPLAY_MODE */
#endif /* __EA8082_R9_RESOL_H__ */
