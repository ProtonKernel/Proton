/*
 * linux/drivers/video/fbdev/exynos/panel/s6e3fab/s6e3fab_unbound2_resol.h
 *
 * Header file for Panel Driver
 *
 * Copyright (c) 2019 Samsung Electronics
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#ifndef __S6E3FAB_UNBOUND2_RESOL_H__
#define __S6E3FAB_UNBOUND2_RESOL_H__

#include <dt-bindings/display/panel-display.h>
#include "../panel.h"
#include "s6e3fab.h"
#include "s6e3fab_dimming.h"

struct panel_vrr s6e3fab_unbound2_default_panel_vrr[] = {
	[S6E3FAB_VRR_60NS] = {
		.fps = 60,
		.base_fps = 60,
		.base_vactive = 2400,
		.base_vfp = 16,
		.base_vbp = 16,
		.te_sel = true,
		.te_v_st = 2401,
		.te_v_ed = 9,
		.te_sw_skip_count = 0,
		.te_hw_skip_count = 0,
		.mode = VRR_NORMAL_MODE,
	},
	[S6E3FAB_VRR_48NS] = {
		.fps = 48,
		.base_fps = 60,
		.base_vactive = 2400,
		.base_vfp = 16,
		.base_vbp = 16,
		.te_sel = true,
		.te_v_st = 2401,
		.te_v_ed = 9,
		.te_sw_skip_count = 0,
		.te_hw_skip_count = 0,
		.mode = VRR_NORMAL_MODE,
	},
	[S6E3FAB_VRR_120HS] = {
		.fps = 120,
		.base_fps = 120,
		.base_vactive = 2400,
		.base_vfp = 16,
		.base_vbp = 16,
		.te_sel = true,
		.te_v_st = 2401,
		.te_v_ed = 9,
		.te_sw_skip_count = 0,
		.te_hw_skip_count = 0,
		.mode = VRR_HS_MODE,
	},
	[S6E3FAB_VRR_60HS_120HS_TE_HW_SKIP_1] = {
		.fps = 120,
		.base_fps = 120,
		.base_vactive = 2400,
		.base_vfp = 16,
		.base_vbp = 16,
		.te_sel = true,
		.te_v_st = 2401,
		.te_v_ed = 9,
		.te_sw_skip_count = 0,
		.te_hw_skip_count = 1,
		.mode = VRR_HS_MODE,
	},
	[S6E3FAB_VRR_96HS] = {
		.fps = 96,
		.base_fps = 120,
		.base_vactive = 2400,
		.base_vfp = 16,
		.base_vbp = 16,
		.te_sel = true,
		.te_v_st = 2401,
		.te_v_ed = 9,
		.te_sw_skip_count = 0,
		.te_hw_skip_count = 0,
		.mode = VRR_HS_MODE,
	},
	[S6E3FAB_VRR_48HS_96HS_TE_HW_SKIP_1] = {
		.fps = 96,
		.base_fps = 120,
		.base_vactive = 2400,
		.base_vfp = 16,
		.base_vbp = 16,
		.te_sel = true,
		.te_v_st = 2401,
		.te_v_ed = 9,
		.te_sw_skip_count = 0,
		.te_hw_skip_count = 1,
		.mode = VRR_HS_MODE,
	},
	[S6E3FAB_VRR_60HS] = {
		.fps = 60,
		.base_fps = 120,
		.base_vactive = 2400,
		.base_vfp = 16,
		.base_vbp = 16,
		.te_sel = true,
		.te_v_st = 2401,
		.te_v_ed = 9,
		.te_sw_skip_count = 0,
		.te_hw_skip_count = 0,
		.mode = VRR_HS_MODE,
	},
	[S6E3FAB_VRR_48HS] = {
		.fps = 48,
		.base_fps = 120,
		.base_vactive = 2400,
		.base_vfp = 16,
		.base_vbp = 16,
		.te_sel = true,
		.te_v_st = 2401,
		.te_v_ed = 9,
		.te_sw_skip_count = 0,
		.te_hw_skip_count = 0,
		.mode = VRR_HS_MODE,
	},
};

static struct panel_vrr *s6e3fab_unbound2_default_vrrtbl[] = {
	&s6e3fab_unbound2_default_panel_vrr[S6E3FAB_VRR_60NS],
	&s6e3fab_unbound2_default_panel_vrr[S6E3FAB_VRR_48NS],
	&s6e3fab_unbound2_default_panel_vrr[S6E3FAB_VRR_120HS],
	&s6e3fab_unbound2_default_panel_vrr[S6E3FAB_VRR_60HS_120HS_TE_HW_SKIP_1],
	&s6e3fab_unbound2_default_panel_vrr[S6E3FAB_VRR_96HS],
	&s6e3fab_unbound2_default_panel_vrr[S6E3FAB_VRR_48HS_96HS_TE_HW_SKIP_1],
	&s6e3fab_unbound2_default_panel_vrr[S6E3FAB_VRR_60HS],
	&s6e3fab_unbound2_default_panel_vrr[S6E3FAB_VRR_48HS],
};

static struct panel_resol s6e3fab_unbound2_default_resol[] = {
	[S6E3FAB_RESOL_1080x2400] = {
		.w = 1080,
		.h = 2400,
		.comp_type = PN_COMP_TYPE_DSC,
		.comp_param = {
			.dsc = {
				.slice_w = 540,
				.slice_h = 120,
			},
		},
		.available_vrr = s6e3fab_unbound2_default_vrrtbl,
		.nr_available_vrr = ARRAY_SIZE(s6e3fab_unbound2_default_vrrtbl),
	},
};

#if defined(CONFIG_PANEL_DISPLAY_MODE)
static struct common_panel_display_mode s6e3fab_unbound2_display_mode[MAX_S6E3FAB_DISPLAY_MODE] = {
	/* FHD */
	[S6E3FAB_DISPLAY_MODE_1080x2400_120HS] = {
		.name = PANEL_DISPLAY_MODE_1080x2400_120HS,
		.resol = &s6e3fab_unbound2_default_resol[S6E3FAB_RESOL_1080x2400],
		.vrr = &s6e3fab_unbound2_default_panel_vrr[S6E3FAB_VRR_120HS],
	},
	[S6E3FAB_DISPLAY_MODE_1080x2400_60HS_120HS_TE_HW_SKIP_1] = {
		.name = PANEL_DISPLAY_MODE_1080x2400_60HS_120HS_TE_HW_SKIP_1,
		.resol = &s6e3fab_unbound2_default_resol[S6E3FAB_RESOL_1080x2400],
		.vrr = &s6e3fab_unbound2_default_panel_vrr[S6E3FAB_VRR_60HS_120HS_TE_HW_SKIP_1],
	},
	[S6E3FAB_DISPLAY_MODE_1080x2400_96HS] = {
		.name = PANEL_DISPLAY_MODE_1080x2400_96HS,
		.resol = &s6e3fab_unbound2_default_resol[S6E3FAB_RESOL_1080x2400],
		.vrr = &s6e3fab_unbound2_default_panel_vrr[S6E3FAB_VRR_96HS],
	},
	[S6E3FAB_DISPLAY_MODE_1080x2400_48HS_96HS_TE_HW_SKIP_1] = {
		.name = PANEL_DISPLAY_MODE_1080x2400_48HS_96HS_TE_HW_SKIP_1,
		.resol = &s6e3fab_unbound2_default_resol[S6E3FAB_RESOL_1080x2400],
		.vrr = &s6e3fab_unbound2_default_panel_vrr[S6E3FAB_VRR_48HS_96HS_TE_HW_SKIP_1],
	},
	[S6E3FAB_DISPLAY_MODE_1080x2400_60HS] = {
		.name = PANEL_DISPLAY_MODE_1080x2400_60HS,
		.resol = &s6e3fab_unbound2_default_resol[S6E3FAB_RESOL_1080x2400],
		.vrr = &s6e3fab_unbound2_default_panel_vrr[S6E3FAB_VRR_60HS],
	},
	[S6E3FAB_DISPLAY_MODE_1080x2400_48HS] = {
		.name = PANEL_DISPLAY_MODE_1080x2400_48HS,
		.resol = &s6e3fab_unbound2_default_resol[S6E3FAB_RESOL_1080x2400],
		.vrr = &s6e3fab_unbound2_default_panel_vrr[S6E3FAB_VRR_48HS],
	},
	[S6E3FAB_DISPLAY_MODE_1080x2400_60NS] = {
		.name = PANEL_DISPLAY_MODE_1080x2400_60NS,
		.resol = &s6e3fab_unbound2_default_resol[S6E3FAB_RESOL_1080x2400],
		.vrr = &s6e3fab_unbound2_default_panel_vrr[S6E3FAB_VRR_60NS],
	},
	[S6E3FAB_DISPLAY_MODE_1080x2400_48NS] = {
		.name = PANEL_DISPLAY_MODE_1080x2400_48NS,
		.resol = &s6e3fab_unbound2_default_resol[S6E3FAB_RESOL_1080x2400],
		.vrr = &s6e3fab_unbound2_default_panel_vrr[S6E3FAB_VRR_48NS],
	},
};

static struct common_panel_display_mode *s6e3fab_unbound2_display_mode_array[MAX_S6E3FAB_DISPLAY_MODE] = {
	[S6E3FAB_DISPLAY_MODE_1080x2400_120HS] = &s6e3fab_unbound2_display_mode[S6E3FAB_DISPLAY_MODE_1080x2400_120HS],
	[S6E3FAB_DISPLAY_MODE_1080x2400_60HS_120HS_TE_HW_SKIP_1] = &s6e3fab_unbound2_display_mode[S6E3FAB_DISPLAY_MODE_1080x2400_60HS_120HS_TE_HW_SKIP_1],
	[S6E3FAB_DISPLAY_MODE_1080x2400_96HS] = &s6e3fab_unbound2_display_mode[S6E3FAB_DISPLAY_MODE_1080x2400_96HS],
	[S6E3FAB_DISPLAY_MODE_1080x2400_48HS_96HS_TE_HW_SKIP_1] = &s6e3fab_unbound2_display_mode[S6E3FAB_DISPLAY_MODE_1080x2400_48HS_96HS_TE_HW_SKIP_1],
	[S6E3FAB_DISPLAY_MODE_1080x2400_60HS] = &s6e3fab_unbound2_display_mode[S6E3FAB_DISPLAY_MODE_1080x2400_60HS],
	[S6E3FAB_DISPLAY_MODE_1080x2400_48HS] = &s6e3fab_unbound2_display_mode[S6E3FAB_DISPLAY_MODE_1080x2400_48HS],
	[S6E3FAB_DISPLAY_MODE_1080x2400_60NS] = &s6e3fab_unbound2_display_mode[S6E3FAB_DISPLAY_MODE_1080x2400_60NS],
	[S6E3FAB_DISPLAY_MODE_1080x2400_48NS] = &s6e3fab_unbound2_display_mode[S6E3FAB_DISPLAY_MODE_1080x2400_48NS],
};

static struct common_panel_display_modes s6e3fab_unbound2_display_modes = {
	.num_modes = ARRAY_SIZE(s6e3fab_unbound2_display_mode_array),
	.modes = (struct common_panel_display_mode **)&s6e3fab_unbound2_display_mode_array,
};
#endif /* CONFIG_PANEL_DISPLAY_MODE */
#endif /* __S6E3FAB_UNBOUND2_RESOL_H__ */
