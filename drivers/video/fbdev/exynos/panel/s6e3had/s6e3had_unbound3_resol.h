/*
 * linux/drivers/video/fbdev/exynos/panel/s6e3had/s6e3had_unbound3_resol.h
 *
 * Header file for Panel Driver
 *
 * Copyright (c) 2019 Samsung Electronics
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#ifndef __S6E3HAD_UNBOUND3_RESOL_H__
#define __S6E3HAD_UNBOUND3_RESOL_H__

#include <dt-bindings/display/panel-display.h>
#include "../panel.h"
#include "s6e3had.h"
#include "s6e3had_dimming.h"

struct panel_vrr s6e3had_unbound3_default_panel_vrr[] = {
	[S6E3HAD_VRR_60NS] = {
		.fps = 60,
		.te_sw_skip_count = 0,
		.te_hw_skip_count = 0,
		.mode = VRR_NORMAL_MODE,
	},
	[S6E3HAD_VRR_48NS] = {
		.fps = 48,
		.te_sw_skip_count = 0,
		.te_hw_skip_count = 0,
		.mode = VRR_NORMAL_MODE,
	},
	[S6E3HAD_VRR_120HS] = {
		.fps = 120,
		.te_sw_skip_count = 0,
		.te_hw_skip_count = 0,
		.mode = VRR_HS_MODE,
	},
	[S6E3HAD_VRR_96HS] = {
		.fps = 96,
		.te_sw_skip_count = 0,
		.te_hw_skip_count = 0,
		.mode = VRR_HS_MODE,
	},
	[S6E3HAD_VRR_60HS_120HS_TE_SW_SKIP_1] = {
		.fps = 120,
		.te_sw_skip_count = 1,
		.te_hw_skip_count = 0,
		.mode = VRR_HS_MODE,
	},
	[S6E3HAD_VRR_48HS_96HS_TE_SW_SKIP_1] = {
		.fps = 96,
		.te_sw_skip_count = 1,
		.te_hw_skip_count = 0,
		.mode = VRR_HS_MODE,
	},
	[S6E3HAD_VRR_60HS_120HS_TE_HW_SKIP_1] = {
		.fps = 120,
		.te_sw_skip_count = 0,
		.te_hw_skip_count = 1,
		.mode = VRR_HS_MODE,
	},
	[S6E3HAD_VRR_48HS_96HS_TE_HW_SKIP_1] = {
		.fps = 96,
		.te_sw_skip_count = 0,
		.te_hw_skip_count = 1,
		.mode = VRR_HS_MODE,
	},
};

static struct panel_vrr *s6e3had_unbound3_default_vrrtbl[] = {
	&s6e3had_unbound3_default_panel_vrr[S6E3HAD_VRR_60NS],
	&s6e3had_unbound3_default_panel_vrr[S6E3HAD_VRR_48NS],
	&s6e3had_unbound3_default_panel_vrr[S6E3HAD_VRR_120HS],
	&s6e3had_unbound3_default_panel_vrr[S6E3HAD_VRR_96HS],
	&s6e3had_unbound3_default_panel_vrr[S6E3HAD_VRR_60HS_120HS_TE_SW_SKIP_1],
	&s6e3had_unbound3_default_panel_vrr[S6E3HAD_VRR_48HS_96HS_TE_SW_SKIP_1],
	&s6e3had_unbound3_default_panel_vrr[S6E3HAD_VRR_60HS_120HS_TE_HW_SKIP_1],
	&s6e3had_unbound3_default_panel_vrr[S6E3HAD_VRR_48HS_96HS_TE_HW_SKIP_1],
};

static struct panel_resol s6e3had_unbound3_default_resol[] = {
	[S6E3HAD_RESOL_1440x3200] = {
		.w = 1440,
		.h = 3200,
		.comp_type = PN_COMP_TYPE_DSC,
		.comp_param = {
			.dsc = {
				.slice_w = 720,
				.slice_h = 40,
			},
		},
		.available_vrr = s6e3had_unbound3_default_vrrtbl,
		.nr_available_vrr = ARRAY_SIZE(s6e3had_unbound3_default_vrrtbl),
	},
	[S6E3HAD_RESOL_1080x2400] = {
		.w = 1080,
		.h = 2400,
		.comp_type = PN_COMP_TYPE_DSC,
		.comp_param = {
			.dsc = {
				.slice_w = 540,
				.slice_h = 40,
			},
		},
		.available_vrr = s6e3had_unbound3_default_vrrtbl,
		.nr_available_vrr = ARRAY_SIZE(s6e3had_unbound3_default_vrrtbl),
	},
	[S6E3HAD_RESOL_720x1600] = {
		.w = 720,
		.h = 1600,
		.comp_type = PN_COMP_TYPE_DSC,
		.comp_param = {
			.dsc = {
				.slice_w = 360,
				.slice_h = 80,
			},
		},
		.available_vrr = s6e3had_unbound3_default_vrrtbl,
		.nr_available_vrr = ARRAY_SIZE(s6e3had_unbound3_default_vrrtbl),
	},
};

#if defined(CONFIG_PANEL_DISPLAY_MODE)
static struct common_panel_display_mode s6e3had_unbound3_display_mode[] = {
	/* WQHD */
	[S6E3HAD_DISPLAY_MODE_1440x3200_120HS] = {
		.name = PANEL_DISPLAY_MODE_1440x3200_120HS,
		.resol = &s6e3had_unbound3_default_resol[S6E3HAD_RESOL_1440x3200],
		.vrr = &s6e3had_unbound3_default_panel_vrr[S6E3HAD_VRR_120HS],
	},
	[S6E3HAD_DISPLAY_MODE_1440x3200_96HS] = {
		.name = PANEL_DISPLAY_MODE_1440x3200_96HS,
		.resol = &s6e3had_unbound3_default_resol[S6E3HAD_RESOL_1440x3200],
		.vrr = &s6e3had_unbound3_default_panel_vrr[S6E3HAD_VRR_96HS],
	},
	[S6E3HAD_DISPLAY_MODE_1440x3200_60HS_120HS_TE_SW_SKIP_1] = {
		.name = PANEL_DISPLAY_MODE_1440x3200_60HS_120HS_TE_SW_SKIP_1,
		.resol = &s6e3had_unbound3_default_resol[S6E3HAD_RESOL_1440x3200],
		.vrr = &s6e3had_unbound3_default_panel_vrr[S6E3HAD_VRR_60HS_120HS_TE_SW_SKIP_1],
	},
	[S6E3HAD_DISPLAY_MODE_1440x3200_48HS_96HS_TE_SW_SKIP_1] = {
		.name = PANEL_DISPLAY_MODE_1440x3200_48HS_96HS_TE_SW_SKIP_1,
		.resol = &s6e3had_unbound3_default_resol[S6E3HAD_RESOL_1440x3200],
		.vrr = &s6e3had_unbound3_default_panel_vrr[S6E3HAD_VRR_48HS_96HS_TE_SW_SKIP_1],
	},
	[S6E3HAD_DISPLAY_MODE_1440x3200_60HS_120HS_TE_HW_SKIP_1] = {
		.name = PANEL_DISPLAY_MODE_1440x3200_60HS_120HS_TE_HW_SKIP_1,
		.resol = &s6e3had_unbound3_default_resol[S6E3HAD_RESOL_1440x3200],
		.vrr = &s6e3had_unbound3_default_panel_vrr[S6E3HAD_VRR_60HS_120HS_TE_HW_SKIP_1],
	},
	[S6E3HAD_DISPLAY_MODE_1440x3200_48HS_96HS_TE_HW_SKIP_1] = {
		.name = PANEL_DISPLAY_MODE_1440x3200_48HS_96HS_TE_HW_SKIP_1,
		.resol = &s6e3had_unbound3_default_resol[S6E3HAD_RESOL_1440x3200],
		.vrr = &s6e3had_unbound3_default_panel_vrr[S6E3HAD_VRR_48HS_96HS_TE_HW_SKIP_1],
	},
	[S6E3HAD_DISPLAY_MODE_1440x3200_60NS] = {
		.name = PANEL_DISPLAY_MODE_1440x3200_60NS,
		.resol = &s6e3had_unbound3_default_resol[S6E3HAD_RESOL_1440x3200],
		.vrr = &s6e3had_unbound3_default_panel_vrr[S6E3HAD_VRR_60NS],
	},
	[S6E3HAD_DISPLAY_MODE_1440x3200_48NS] = {
		.name = PANEL_DISPLAY_MODE_1440x3200_48NS,
		.resol = &s6e3had_unbound3_default_resol[S6E3HAD_RESOL_1440x3200],
		.vrr = &s6e3had_unbound3_default_panel_vrr[S6E3HAD_VRR_48NS],
	},

	/* FHD */
	[S6E3HAD_DISPLAY_MODE_1080x2400_120HS] = {
		.name = PANEL_DISPLAY_MODE_1080x2400_120HS,
		.resol = &s6e3had_unbound3_default_resol[S6E3HAD_RESOL_1080x2400],
		.vrr = &s6e3had_unbound3_default_panel_vrr[S6E3HAD_VRR_120HS],
	},
	[S6E3HAD_DISPLAY_MODE_1080x2400_96HS] = {
		.name = PANEL_DISPLAY_MODE_1080x2400_96HS,
		.resol = &s6e3had_unbound3_default_resol[S6E3HAD_RESOL_1080x2400],
		.vrr = &s6e3had_unbound3_default_panel_vrr[S6E3HAD_VRR_96HS],
	},
	[S6E3HAD_DISPLAY_MODE_1080x2400_60HS_120HS_TE_SW_SKIP_1] = {
		.name = PANEL_DISPLAY_MODE_1080x2400_60HS_120HS_TE_SW_SKIP_1,
		.resol = &s6e3had_unbound3_default_resol[S6E3HAD_RESOL_1080x2400],
		.vrr = &s6e3had_unbound3_default_panel_vrr[S6E3HAD_VRR_60HS_120HS_TE_SW_SKIP_1],
	},
	[S6E3HAD_DISPLAY_MODE_1080x2400_48HS_96HS_TE_SW_SKIP_1] = {
		.name = PANEL_DISPLAY_MODE_1080x2400_48HS_96HS_TE_SW_SKIP_1,
		.resol = &s6e3had_unbound3_default_resol[S6E3HAD_RESOL_1080x2400],
		.vrr = &s6e3had_unbound3_default_panel_vrr[S6E3HAD_VRR_48HS_96HS_TE_SW_SKIP_1],
	},
	[S6E3HAD_DISPLAY_MODE_1080x2400_60HS_120HS_TE_HW_SKIP_1] = {
		.name = PANEL_DISPLAY_MODE_1080x2400_60HS_120HS_TE_HW_SKIP_1,
		.resol = &s6e3had_unbound3_default_resol[S6E3HAD_RESOL_1080x2400],
		.vrr = &s6e3had_unbound3_default_panel_vrr[S6E3HAD_VRR_60HS_120HS_TE_HW_SKIP_1],
	},
	[S6E3HAD_DISPLAY_MODE_1080x2400_48HS_96HS_TE_HW_SKIP_1] = {
		.name = PANEL_DISPLAY_MODE_1080x2400_48HS_96HS_TE_HW_SKIP_1,
		.resol = &s6e3had_unbound3_default_resol[S6E3HAD_RESOL_1080x2400],
		.vrr = &s6e3had_unbound3_default_panel_vrr[S6E3HAD_VRR_48HS_96HS_TE_HW_SKIP_1],
	},
	[S6E3HAD_DISPLAY_MODE_1080x2400_60NS] = {
		.name = PANEL_DISPLAY_MODE_1080x2400_60NS,
		.resol = &s6e3had_unbound3_default_resol[S6E3HAD_RESOL_1080x2400],
		.vrr = &s6e3had_unbound3_default_panel_vrr[S6E3HAD_VRR_60NS],
	},
	[S6E3HAD_DISPLAY_MODE_1080x2400_48NS] = {
		.name = PANEL_DISPLAY_MODE_1080x2400_48NS,
		.resol = &s6e3had_unbound3_default_resol[S6E3HAD_RESOL_1080x2400],
		.vrr = &s6e3had_unbound3_default_panel_vrr[S6E3HAD_VRR_48NS],
	},

	/* HD */
	[S6E3HAD_DISPLAY_MODE_720x1600_120HS] = {
		.name = PANEL_DISPLAY_MODE_720x1600_120HS,
		.resol = &s6e3had_unbound3_default_resol[S6E3HAD_RESOL_720x1600],
		.vrr = &s6e3had_unbound3_default_panel_vrr[S6E3HAD_VRR_120HS],
	},
	[S6E3HAD_DISPLAY_MODE_720x1600_96HS] = {
		.name = PANEL_DISPLAY_MODE_720x1600_96HS,
		.resol = &s6e3had_unbound3_default_resol[S6E3HAD_RESOL_720x1600],
		.vrr = &s6e3had_unbound3_default_panel_vrr[S6E3HAD_VRR_96HS],
	},
	[S6E3HAD_DISPLAY_MODE_720x1600_60HS_120HS_TE_SW_SKIP_1] = {
		.name = PANEL_DISPLAY_MODE_720x1600_60HS_120HS_TE_SW_SKIP_1,
		.resol = &s6e3had_unbound3_default_resol[S6E3HAD_RESOL_720x1600],
		.vrr = &s6e3had_unbound3_default_panel_vrr[S6E3HAD_VRR_60HS_120HS_TE_SW_SKIP_1],
	},
	[S6E3HAD_DISPLAY_MODE_720x1600_48HS_96HS_TE_SW_SKIP_1] = {
		.name = PANEL_DISPLAY_MODE_720x1600_48HS_96HS_TE_SW_SKIP_1,
		.resol = &s6e3had_unbound3_default_resol[S6E3HAD_RESOL_720x1600],
		.vrr = &s6e3had_unbound3_default_panel_vrr[S6E3HAD_VRR_48HS_96HS_TE_SW_SKIP_1],
	},
	[S6E3HAD_DISPLAY_MODE_720x1600_60HS_120HS_TE_HW_SKIP_1] = {
		.name = PANEL_DISPLAY_MODE_720x1600_60HS_120HS_TE_HW_SKIP_1,
		.resol = &s6e3had_unbound3_default_resol[S6E3HAD_RESOL_720x1600],
		.vrr = &s6e3had_unbound3_default_panel_vrr[S6E3HAD_VRR_60HS_120HS_TE_HW_SKIP_1],
	},
	[S6E3HAD_DISPLAY_MODE_720x1600_48HS_96HS_TE_HW_SKIP_1] = {
		.name = PANEL_DISPLAY_MODE_720x1600_48HS_96HS_TE_HW_SKIP_1,
		.resol = &s6e3had_unbound3_default_resol[S6E3HAD_RESOL_720x1600],
		.vrr = &s6e3had_unbound3_default_panel_vrr[S6E3HAD_VRR_48HS_96HS_TE_HW_SKIP_1],
	},
	[S6E3HAD_DISPLAY_MODE_720x1600_60NS] = {
		.name = PANEL_DISPLAY_MODE_720x1600_60NS,
		.resol = &s6e3had_unbound3_default_resol[S6E3HAD_RESOL_720x1600],
		.vrr = &s6e3had_unbound3_default_panel_vrr[S6E3HAD_VRR_60NS],
	},
	[S6E3HAD_DISPLAY_MODE_720x1600_48NS] = {
		.name = PANEL_DISPLAY_MODE_720x1600_48NS,
		.resol = &s6e3had_unbound3_default_resol[S6E3HAD_RESOL_720x1600],
		.vrr = &s6e3had_unbound3_default_panel_vrr[S6E3HAD_VRR_48NS],
	},
};

static struct common_panel_display_mode *s6e3had_unbound3_display_mode_array[] = {
	[S6E3HAD_DISPLAY_MODE_1440x3200_120HS] = &s6e3had_unbound3_display_mode[S6E3HAD_DISPLAY_MODE_1440x3200_120HS],
	[S6E3HAD_DISPLAY_MODE_1440x3200_96HS] = &s6e3had_unbound3_display_mode[S6E3HAD_DISPLAY_MODE_1440x3200_96HS],
	[S6E3HAD_DISPLAY_MODE_1440x3200_60HS_120HS_TE_SW_SKIP_1] = &s6e3had_unbound3_display_mode[S6E3HAD_DISPLAY_MODE_1440x3200_60HS_120HS_TE_SW_SKIP_1],
	[S6E3HAD_DISPLAY_MODE_1440x3200_48HS_96HS_TE_SW_SKIP_1] = &s6e3had_unbound3_display_mode[S6E3HAD_DISPLAY_MODE_1440x3200_48HS_96HS_TE_SW_SKIP_1],
	[S6E3HAD_DISPLAY_MODE_1440x3200_60HS_120HS_TE_HW_SKIP_1] = &s6e3had_unbound3_display_mode[S6E3HAD_DISPLAY_MODE_1440x3200_60HS_120HS_TE_HW_SKIP_1],
	[S6E3HAD_DISPLAY_MODE_1440x3200_48HS_96HS_TE_HW_SKIP_1] = &s6e3had_unbound3_display_mode[S6E3HAD_DISPLAY_MODE_1440x3200_48HS_96HS_TE_HW_SKIP_1],
	[S6E3HAD_DISPLAY_MODE_1440x3200_60NS] = &s6e3had_unbound3_display_mode[S6E3HAD_DISPLAY_MODE_1440x3200_60NS],
	[S6E3HAD_DISPLAY_MODE_1440x3200_48NS] = &s6e3had_unbound3_display_mode[S6E3HAD_DISPLAY_MODE_1440x3200_48NS],
	[S6E3HAD_DISPLAY_MODE_1080x2400_120HS] = &s6e3had_unbound3_display_mode[S6E3HAD_DISPLAY_MODE_1080x2400_120HS],
	[S6E3HAD_DISPLAY_MODE_1080x2400_96HS] = &s6e3had_unbound3_display_mode[S6E3HAD_DISPLAY_MODE_1080x2400_96HS],
	[S6E3HAD_DISPLAY_MODE_1080x2400_60HS_120HS_TE_SW_SKIP_1] = &s6e3had_unbound3_display_mode[S6E3HAD_DISPLAY_MODE_1080x2400_60HS_120HS_TE_SW_SKIP_1],
	[S6E3HAD_DISPLAY_MODE_1080x2400_48HS_96HS_TE_SW_SKIP_1] = &s6e3had_unbound3_display_mode[S6E3HAD_DISPLAY_MODE_1080x2400_48HS_96HS_TE_SW_SKIP_1],
	[S6E3HAD_DISPLAY_MODE_1080x2400_60HS_120HS_TE_HW_SKIP_1] = &s6e3had_unbound3_display_mode[S6E3HAD_DISPLAY_MODE_1080x2400_60HS_120HS_TE_HW_SKIP_1],
	[S6E3HAD_DISPLAY_MODE_1080x2400_48HS_96HS_TE_HW_SKIP_1] = &s6e3had_unbound3_display_mode[S6E3HAD_DISPLAY_MODE_1080x2400_48HS_96HS_TE_HW_SKIP_1],
	[S6E3HAD_DISPLAY_MODE_1080x2400_60NS] = &s6e3had_unbound3_display_mode[S6E3HAD_DISPLAY_MODE_1080x2400_60NS],
	[S6E3HAD_DISPLAY_MODE_1080x2400_48NS] = &s6e3had_unbound3_display_mode[S6E3HAD_DISPLAY_MODE_1080x2400_48NS],
	[S6E3HAD_DISPLAY_MODE_720x1600_120HS] = &s6e3had_unbound3_display_mode[S6E3HAD_DISPLAY_MODE_720x1600_120HS],
	[S6E3HAD_DISPLAY_MODE_720x1600_96HS] = &s6e3had_unbound3_display_mode[S6E3HAD_DISPLAY_MODE_720x1600_96HS],
	[S6E3HAD_DISPLAY_MODE_720x1600_60HS_120HS_TE_SW_SKIP_1] = &s6e3had_unbound3_display_mode[S6E3HAD_DISPLAY_MODE_720x1600_60HS_120HS_TE_SW_SKIP_1],
	[S6E3HAD_DISPLAY_MODE_720x1600_48HS_96HS_TE_SW_SKIP_1] = &s6e3had_unbound3_display_mode[S6E3HAD_DISPLAY_MODE_720x1600_48HS_96HS_TE_SW_SKIP_1],
	[S6E3HAD_DISPLAY_MODE_720x1600_60HS_120HS_TE_HW_SKIP_1] = &s6e3had_unbound3_display_mode[S6E3HAD_DISPLAY_MODE_720x1600_60HS_120HS_TE_HW_SKIP_1],
	[S6E3HAD_DISPLAY_MODE_720x1600_48HS_96HS_TE_HW_SKIP_1] = &s6e3had_unbound3_display_mode[S6E3HAD_DISPLAY_MODE_720x1600_48HS_96HS_TE_HW_SKIP_1],
	[S6E3HAD_DISPLAY_MODE_720x1600_60NS] = &s6e3had_unbound3_display_mode[S6E3HAD_DISPLAY_MODE_720x1600_60NS],
	[S6E3HAD_DISPLAY_MODE_720x1600_48NS] = &s6e3had_unbound3_display_mode[S6E3HAD_DISPLAY_MODE_720x1600_48NS],
};

static struct common_panel_display_modes s6e3had_unbound3_display_modes = {
	.num_modes = ARRAY_SIZE(s6e3had_unbound3_display_mode),
	.modes = (struct common_panel_display_mode **)&s6e3had_unbound3_display_mode_array,
};
#endif /* CONFIG_PANEL_DISPLAY_MODE */
#endif /* __S6E3HAD_UNBOUND3_RESOL_H__ */
