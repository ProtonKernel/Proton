/*
 * linux/drivers/video/fbdev/exynos/panel/s6e3hab/s6e3hab_hubble_resol.h
 *
 * Header file for Panel Driver
 *
 * Copyright (c) 2019 Samsung Electronics
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#ifndef __S6E3HAB_HUBBLE_RESOL_H__
#define __S6E3HAB_HUBBLE_RESOL_H__

#include <dt-bindings/display/panel-display.h>
#include "../panel.h"
#include "s6e3hab.h"
#include "s6e3hab_dimming.h"

struct panel_vrr s6e3hab_hubble_preliminary_panel_vrr[] = {
	[S6E3HAB_VRR_60NS] = {
		.fps = 60,
		.base_fps = 60,
		.base_vactive = 3200,
		.base_vfp = 16,
		.base_vbp = 16,
		.te_sel = true,
		.te_v_st = 3201,
		.te_v_ed = 9,
		.mode = VRR_NORMAL_MODE,
		.aid_cycle = VRR_AID_4_CYCLE,
	},
	[S6E3HAB_VRR_52NS] = {
		.fps = 52,
		.base_fps = 60,
		.base_vactive = 3200,
		.base_vfp = 16,
		.base_vbp = 16,
		.te_sel = true,
		.te_v_st = 3201,
		.te_v_ed = 9,
		.mode = VRR_NORMAL_MODE,
		.aid_cycle = VRR_AID_4_CYCLE,
	},
	[S6E3HAB_VRR_48NS] = {
		.fps = 48,
		.base_fps = 60,
		.base_vactive = 3200,
		.base_vfp = 16,
		.base_vbp = 16,
		.te_sel = true,
		.te_v_st = 3201,
		.te_v_ed = 9,
		.mode = VRR_NORMAL_MODE,
		.aid_cycle = VRR_AID_4_CYCLE,
	},
};

struct panel_vrr s6e3hab_hubble_default_panel_vrr[] = {
	[S6E3HAB_VRR_60NS] = {
		.fps = 60,
		.base_fps = 60,
		.base_vactive = 3200,
		.base_vfp = 16,
		.base_vbp = 16,
		.te_sel = true,
		.te_v_st = 3201,
		.te_v_ed = 9,
		.mode = VRR_NORMAL_MODE,
		.aid_cycle = VRR_AID_4_CYCLE,
	},
	[S6E3HAB_VRR_52NS] = {
		.fps = 52,
		.base_fps = 60,
		.base_vactive = 3200,
		.base_vfp = 16,
		.base_vbp = 16,
		.te_sel = true,
		.te_v_st = 3201,
		.te_v_ed = 9,
		.mode = VRR_NORMAL_MODE,
		.aid_cycle = VRR_AID_4_CYCLE,
	},
	[S6E3HAB_VRR_48NS] = {
		.fps = 48,
		.base_fps = 60,
		.base_vactive = 3200,
		.base_vfp = 16,
		.base_vbp = 16,
		.te_sel = true,
		.te_v_st = 3201,
		.te_v_ed = 9,
		.mode = VRR_NORMAL_MODE,
		.aid_cycle = VRR_AID_4_CYCLE,
	},
	[S6E3HAB_VRR_120HS] = {
		.fps = 120,
		.base_fps = 120,
		.base_vactive = 3200,
		.base_vfp = 16,
		.base_vbp = 16,
		.te_sel = true,
		.te_v_st = 3201,
		.te_v_ed = 9,
		.mode = VRR_HS_MODE,
		.aid_cycle = VRR_AID_2_CYCLE,
	},
	[S6E3HAB_VRR_120HS_AID_4_CYCLE] = {
		.fps = 120,
		.base_fps = 120,
		.base_vactive = 3200,
		.base_vfp = 16,
		.base_vbp = 16,
		.te_sel = true,
		.te_v_st = 3201,
		.te_v_ed = 9,
		.mode = VRR_HS_MODE,
		.aid_cycle = VRR_AID_4_CYCLE,
	},
	[S6E3HAB_VRR_112HS] = {
		.fps = 112,
		.base_fps = 120,
		.base_vactive = 3200,
		.base_vfp = 16,
		.base_vbp = 16,
		.te_sel = true,
		.te_v_st = 3201,
		.te_v_ed = 9,
		.mode = VRR_HS_MODE,
		.aid_cycle = VRR_AID_2_CYCLE,
	},
	[S6E3HAB_VRR_110HS] = {
		.fps = 110,
		.base_fps = 120,
		.base_vactive = 3200,
		.base_vfp = 16,
		.base_vbp = 16,
		.te_sel = true,
		.te_v_st = 3201,
		.te_v_ed = 9,
		.mode = VRR_HS_MODE,
		.aid_cycle = VRR_AID_2_CYCLE,
	},
	[S6E3HAB_VRR_104HS] = {
		.fps = 104,
		.base_fps = 120,
		.base_vactive = 3200,
		.base_vfp = 16,
		.base_vbp = 16,
		.te_sel = true,
		.te_v_st = 3201,
		.te_v_ed = 9,
		.mode = VRR_HS_MODE,
		.aid_cycle = VRR_AID_2_CYCLE,
	},
	[S6E3HAB_VRR_100HS] = {
		.fps = 100,
		.base_fps = 120,
		.base_vactive = 3200,
		.base_vfp = 16,
		.base_vbp = 16,
		.te_sel = true,
		.te_v_st = 3201,
		.te_v_ed = 9,
		.mode = VRR_HS_MODE,
		.aid_cycle = VRR_AID_2_CYCLE,
	},
	[S6E3HAB_VRR_96HS] = {
		.fps = 96,
		.base_fps = 120,
		.base_vactive = 3200,
		.base_vfp = 16,
		.base_vbp = 16,
		.te_sel = true,
		.te_v_st = 3201,
		.te_v_ed = 9,
		.mode = VRR_HS_MODE,
		.aid_cycle = VRR_AID_2_CYCLE,
	},
	[S6E3HAB_VRR_70HS] = {
		.fps = 70,
		.base_fps = 120,
		.base_vactive = 3200,
		.base_vfp = 16,
		.base_vbp = 16,
		.te_sel = true,
		.te_v_st = 3201,
		.te_v_ed = 9,
		.mode = VRR_HS_MODE,
		.aid_cycle = VRR_AID_2_CYCLE,
	},
	[S6E3HAB_VRR_60HS] = {
		.fps = 60,
		.base_fps = 120,
		.base_vactive = 3200,
		.base_vfp = 16,
		.base_vbp = 16,
		.te_sel = true,
		.te_v_st = 3201,
		.te_v_ed = 9,
		.mode = VRR_HS_MODE,
		.aid_cycle = VRR_AID_2_CYCLE,
	},
};

struct panel_vrr s6e3hab_hubble_rev03_panel_vrr[] = {
	[S6E3HAB_VRR_60NS] = {
		.fps = 60,
		.base_fps = 60,
		.base_vactive = 3200,
		.base_vfp = 16,
		.base_vbp = 16,
		.te_sel = true,
		.te_v_st = 3201,
		.te_v_ed = 9,
		.mode = VRR_NORMAL_MODE,
		.aid_cycle = VRR_AID_4_CYCLE,
	},
	[S6E3HAB_VRR_52NS] = {
		.fps = 52,
		.base_fps = 60,
		.base_vactive = 3200,
		.base_vfp = 16,
		.base_vbp = 16,
		.te_sel = true,
		.te_v_st = 3201,
		.te_v_ed = 9,
		.mode = VRR_NORMAL_MODE,
		.aid_cycle = VRR_AID_4_CYCLE,
	},
	[S6E3HAB_VRR_48NS] = {
		.fps = 48,
		.base_fps = 60,
		.base_vactive = 3200,
		.base_vfp = 16,
		.base_vbp = 16,
		.te_sel = true,
		.te_v_st = 3201,
		.te_v_ed = 9,
		.mode = VRR_NORMAL_MODE,
		.aid_cycle = VRR_AID_4_CYCLE,
	},
	[S6E3HAB_VRR_120HS] = {
		.fps = 120,
		.base_fps = 120,
		.base_vactive = 3200,
		.base_vfp = 16,
		.base_vbp = 16,
		.te_sel = true,
		.te_v_st = 3201,
		.te_v_ed = 9,
		.mode = VRR_HS_MODE,
		.aid_cycle = VRR_AID_2_CYCLE,
	},
	[S6E3HAB_VRR_120HS_AID_4_CYCLE] = {
		.fps = 120,
		.base_fps = 120,
		.base_vactive = 3200,
		.base_vfp = 16,
		.base_vbp = 16,
		.te_sel = true,
		.te_v_st = 3201,
		.te_v_ed = 9,
		.mode = VRR_HS_MODE,
		.aid_cycle = VRR_AID_4_CYCLE,
	},
	[S6E3HAB_VRR_112HS] = {
		.fps = 112,
		.base_fps = 120,
		.base_vactive = 3200,
		.base_vfp = 16,
		.base_vbp = 16,
		.te_sel = true,
		.te_v_st = 3201,
		.te_v_ed = 9,
		.mode = VRR_HS_MODE,
		.aid_cycle = VRR_AID_2_CYCLE,
	},
	[S6E3HAB_VRR_110HS] = {
		.fps = 110,
		.base_fps = 120,
		.base_vactive = 3200,
		.base_vfp = 16,
		.base_vbp = 16,
		.te_sel = true,
		.te_v_st = 3201,
		.te_v_ed = 9,
		.mode = VRR_HS_MODE,
		.aid_cycle = VRR_AID_4_CYCLE,
	},
	[S6E3HAB_VRR_104HS] = {
		.fps = 104,
		.base_fps = 120,
		.base_vactive = 3200,
		.base_vfp = 16,
		.base_vbp = 16,
		.te_sel = true,
		.te_v_st = 3201,
		.te_v_ed = 9,
		.mode = VRR_HS_MODE,
		.aid_cycle = VRR_AID_2_CYCLE,
	},
	[S6E3HAB_VRR_100HS] = {
		.fps = 100,
		.base_fps = 120,
		.base_vactive = 3200,
		.base_vfp = 16,
		.base_vbp = 16,
		.te_sel = true,
		.te_v_st = 3201,
		.te_v_ed = 9,
		.mode = VRR_HS_MODE,
		.aid_cycle = VRR_AID_4_CYCLE,
	},
	[S6E3HAB_VRR_96HS] = {
		.fps = 96,
		.base_fps = 120,
		.base_vactive = 3200,
		.base_vfp = 16,
		.base_vbp = 16,
		.te_sel = true,
		.te_v_st = 3201,
		.te_v_ed = 9,
		.mode = VRR_HS_MODE,
		.aid_cycle = VRR_AID_2_CYCLE,
	},
	[S6E3HAB_VRR_70HS] = {
		.fps = 70,
		.base_fps = 120,
		.base_vactive = 3200,
		.base_vfp = 16,
		.base_vbp = 16,
		.te_sel = true,
		.te_v_st = 3201,
		.te_v_ed = 9,
		.mode = VRR_HS_MODE,
		.aid_cycle = VRR_AID_4_CYCLE,
	},
	[S6E3HAB_VRR_60HS] = {
		.fps = 60,
		.base_fps = 120,
		.base_vactive = 3200,
		.base_vfp = 16,
		.base_vbp = 16,
		.te_sel = true,
		.te_v_st = 3201,
		.te_v_ed = 9,
		.mode = VRR_HS_MODE,
		.aid_cycle = VRR_AID_2_CYCLE,
	},
};

static struct panel_vrr *s6e3hab_hubble_preliminary_vrrtbl[] = {
	&s6e3hab_hubble_preliminary_panel_vrr[S6E3HAB_VRR_60NS],
	&s6e3hab_hubble_preliminary_panel_vrr[S6E3HAB_VRR_52NS],
	&s6e3hab_hubble_preliminary_panel_vrr[S6E3HAB_VRR_48NS],
};

static struct panel_vrr *s6e3hab_hubble_wqhd_vrrtbl[] = {
	&s6e3hab_hubble_default_panel_vrr[S6E3HAB_VRR_60NS],
	&s6e3hab_hubble_default_panel_vrr[S6E3HAB_VRR_52NS],
	&s6e3hab_hubble_default_panel_vrr[S6E3HAB_VRR_48NS],
};

static struct panel_vrr *s6e3hab_hubble_default_vrrtbl[] = {
	&s6e3hab_hubble_default_panel_vrr[S6E3HAB_VRR_60NS],
	&s6e3hab_hubble_default_panel_vrr[S6E3HAB_VRR_52NS],
	&s6e3hab_hubble_default_panel_vrr[S6E3HAB_VRR_48NS],
	&s6e3hab_hubble_default_panel_vrr[S6E3HAB_VRR_120HS],
	&s6e3hab_hubble_default_panel_vrr[S6E3HAB_VRR_120HS_AID_4_CYCLE],
	&s6e3hab_hubble_default_panel_vrr[S6E3HAB_VRR_112HS],
	&s6e3hab_hubble_default_panel_vrr[S6E3HAB_VRR_110HS],
	&s6e3hab_hubble_default_panel_vrr[S6E3HAB_VRR_104HS],
	&s6e3hab_hubble_default_panel_vrr[S6E3HAB_VRR_100HS],
	&s6e3hab_hubble_default_panel_vrr[S6E3HAB_VRR_96HS],
	&s6e3hab_hubble_default_panel_vrr[S6E3HAB_VRR_70HS],
	&s6e3hab_hubble_default_panel_vrr[S6E3HAB_VRR_60HS],
};

static struct panel_vrr *s6e3hab_hubble_rev03_wqhd_vrrtbl[] = {
	&s6e3hab_hubble_rev03_panel_vrr[S6E3HAB_VRR_60NS],
	&s6e3hab_hubble_rev03_panel_vrr[S6E3HAB_VRR_52NS],
	&s6e3hab_hubble_rev03_panel_vrr[S6E3HAB_VRR_48NS],
};

static struct panel_vrr *s6e3hab_hubble_rev03_vrrtbl[] = {
	&s6e3hab_hubble_rev03_panel_vrr[S6E3HAB_VRR_60NS],
	&s6e3hab_hubble_rev03_panel_vrr[S6E3HAB_VRR_52NS],
	&s6e3hab_hubble_rev03_panel_vrr[S6E3HAB_VRR_48NS],
	&s6e3hab_hubble_rev03_panel_vrr[S6E3HAB_VRR_120HS],
	&s6e3hab_hubble_rev03_panel_vrr[S6E3HAB_VRR_120HS_AID_4_CYCLE],
	&s6e3hab_hubble_rev03_panel_vrr[S6E3HAB_VRR_112HS],
	&s6e3hab_hubble_rev03_panel_vrr[S6E3HAB_VRR_110HS],
	&s6e3hab_hubble_rev03_panel_vrr[S6E3HAB_VRR_104HS],
	&s6e3hab_hubble_rev03_panel_vrr[S6E3HAB_VRR_100HS],
	&s6e3hab_hubble_rev03_panel_vrr[S6E3HAB_VRR_96HS],
	&s6e3hab_hubble_rev03_panel_vrr[S6E3HAB_VRR_70HS],
	&s6e3hab_hubble_rev03_panel_vrr[S6E3HAB_VRR_60HS],
};

static struct panel_resol s6e3hab_hubble_preliminary_resol[] = {
	[S6E3HAB_RESOL_1440x3200] = {
		.w = 1440,
		.h = 3200,
		.comp_type = PN_COMP_TYPE_DSC,
		.comp_param = {
			.dsc = {
				.slice_w = 720,
				.slice_h = 40,
			},
		},
		.available_vrr = s6e3hab_hubble_preliminary_vrrtbl,
		.nr_available_vrr = ARRAY_SIZE(s6e3hab_hubble_preliminary_vrrtbl),
	},
	[S6E3HAB_RESOL_1080x2400] = {
		.w = 1080,
		.h = 2400,
		.comp_type = PN_COMP_TYPE_DSC,
		.comp_param = {
			.dsc = {
				.slice_w = 540,
				.slice_h = 40,
			},
		},
		.available_vrr = s6e3hab_hubble_preliminary_vrrtbl,
		.nr_available_vrr = ARRAY_SIZE(s6e3hab_hubble_preliminary_vrrtbl),
	},
	[S6E3HAB_RESOL_720x1600] = {
		.w = 720,
		.h = 1600,
		.comp_type = PN_COMP_TYPE_DSC,
		.comp_param = {
			.dsc = {
				.slice_w = 360,
				.slice_h = 80,
			},
		},
		.available_vrr = s6e3hab_hubble_preliminary_vrrtbl,
		.nr_available_vrr = ARRAY_SIZE(s6e3hab_hubble_preliminary_vrrtbl),
	},
};

static struct panel_resol s6e3hab_hubble_default_resol[] = {
	[S6E3HAB_RESOL_1440x3200] = {
		.w = 1440,
		.h = 3200,
		.comp_type = PN_COMP_TYPE_DSC,
		.comp_param = {
			.dsc = {
				.slice_w = 720,
				.slice_h = 40,
			},
		},
		.available_vrr = s6e3hab_hubble_wqhd_vrrtbl,
		.nr_available_vrr = ARRAY_SIZE(s6e3hab_hubble_wqhd_vrrtbl),
	},
	[S6E3HAB_RESOL_1080x2400] = {
		.w = 1080,
		.h = 2400,
		.comp_type = PN_COMP_TYPE_DSC,
		.comp_param = {
			.dsc = {
				.slice_w = 540,
				.slice_h = 40,
			},
		},
		.available_vrr = s6e3hab_hubble_default_vrrtbl,
		.nr_available_vrr = ARRAY_SIZE(s6e3hab_hubble_default_vrrtbl),
	},
	[S6E3HAB_RESOL_720x1600] = {
		.w = 720,
		.h = 1600,
		.comp_type = PN_COMP_TYPE_DSC,
		.comp_param = {
			.dsc = {
				.slice_w = 360,
				.slice_h = 80,
			},
		},
		.available_vrr = s6e3hab_hubble_default_vrrtbl,
		.nr_available_vrr = ARRAY_SIZE(s6e3hab_hubble_default_vrrtbl),
	},
};

static struct panel_resol s6e3hab_hubble_rev03_resol[] = {
	[S6E3HAB_RESOL_1440x3200] = {
		.w = 1440,
		.h = 3200,
		.comp_type = PN_COMP_TYPE_DSC,
		.comp_param = {
			.dsc = {
				.slice_w = 720,
				.slice_h = 40,
			},
		},
		.available_vrr = s6e3hab_hubble_rev03_wqhd_vrrtbl,
		.nr_available_vrr = ARRAY_SIZE(s6e3hab_hubble_rev03_wqhd_vrrtbl),
	},
	[S6E3HAB_RESOL_1080x2400] = {
		.w = 1080,
		.h = 2400,
		.comp_type = PN_COMP_TYPE_DSC,
		.comp_param = {
			.dsc = {
				.slice_w = 540,
				.slice_h = 40,
			},
		},
		.available_vrr = s6e3hab_hubble_rev03_vrrtbl,
		.nr_available_vrr = ARRAY_SIZE(s6e3hab_hubble_rev03_vrrtbl),
	},
	[S6E3HAB_RESOL_720x1600] = {
		.w = 720,
		.h = 1600,
		.comp_type = PN_COMP_TYPE_DSC,
		.comp_param = {
			.dsc = {
				.slice_w = 360,
				.slice_h = 80,
			},
		},
		.available_vrr = s6e3hab_hubble_rev03_vrrtbl,
		.nr_available_vrr = ARRAY_SIZE(s6e3hab_hubble_rev03_vrrtbl),
	},
};

#if defined(CONFIG_PANEL_DISPLAY_MODE)
static struct common_panel_display_mode s6e3hab_hubble_preliminary_display_mode[MAX_S6E3HAB_DISPLAY_MODE] = {
	/* WQHD */
	[S6E3HAB_DISPLAY_MODE_1440x3200_60NS] = {
		.name = PANEL_DISPLAY_MODE_1440x3200_60NS,
		.resol = &s6e3hab_hubble_preliminary_resol[S6E3HAB_RESOL_1440x3200],
		.vrr = &s6e3hab_hubble_preliminary_panel_vrr[S6E3HAB_VRR_60NS],
	},
	[S6E3HAB_DISPLAY_MODE_1440x3200_48NS] = {
		.name = PANEL_DISPLAY_MODE_1440x3200_48NS,
		.resol = &s6e3hab_hubble_preliminary_resol[S6E3HAB_RESOL_1440x3200],
		.vrr = &s6e3hab_hubble_preliminary_panel_vrr[S6E3HAB_VRR_48NS],
	},

	/* FHD */
	[S6E3HAB_DISPLAY_MODE_1080x2400_60NS] = {
		.name = PANEL_DISPLAY_MODE_1080x2400_60NS,
		.resol = &s6e3hab_hubble_preliminary_resol[S6E3HAB_RESOL_1080x2400],
		.vrr = &s6e3hab_hubble_preliminary_panel_vrr[S6E3HAB_VRR_60NS],
	},
	[S6E3HAB_DISPLAY_MODE_1080x2400_48NS] = {
		.name = PANEL_DISPLAY_MODE_1080x2400_48NS,
		.resol = &s6e3hab_hubble_preliminary_resol[S6E3HAB_RESOL_1080x2400],
		.vrr = &s6e3hab_hubble_preliminary_panel_vrr[S6E3HAB_VRR_48NS],
	},

	/* HD */
	[S6E3HAB_DISPLAY_MODE_720x1600_60NS] = {
		.name = PANEL_DISPLAY_MODE_720x1600_60NS,
		.resol = &s6e3hab_hubble_preliminary_resol[S6E3HAB_RESOL_720x1600],
		.vrr = &s6e3hab_hubble_preliminary_panel_vrr[S6E3HAB_VRR_60NS],
	},
	[S6E3HAB_DISPLAY_MODE_720x1600_48NS] = {
		.name = PANEL_DISPLAY_MODE_720x1600_48NS,
		.resol = &s6e3hab_hubble_preliminary_resol[S6E3HAB_RESOL_720x1600],
		.vrr = &s6e3hab_hubble_preliminary_panel_vrr[S6E3HAB_VRR_48NS],
	},
};

static struct common_panel_display_mode s6e3hab_hubble_display_mode[MAX_S6E3HAB_DISPLAY_MODE] = {
	/* WQHD */
	[S6E3HAB_DISPLAY_MODE_1440x3200_60NS] = {
		.name = PANEL_DISPLAY_MODE_1440x3200_60NS,
		.resol = &s6e3hab_hubble_default_resol[S6E3HAB_RESOL_1440x3200],
		.vrr = &s6e3hab_hubble_default_panel_vrr[S6E3HAB_VRR_60NS],
	},
	[S6E3HAB_DISPLAY_MODE_1440x3200_52NS] = {
		.name = PANEL_DISPLAY_MODE_1440x3200_52NS,
		.resol = &s6e3hab_hubble_default_resol[S6E3HAB_RESOL_1440x3200],
		.vrr = &s6e3hab_hubble_default_panel_vrr[S6E3HAB_VRR_52NS],
	},
	[S6E3HAB_DISPLAY_MODE_1440x3200_48NS] = {
		.name = PANEL_DISPLAY_MODE_1440x3200_48NS,
		.resol = &s6e3hab_hubble_default_resol[S6E3HAB_RESOL_1440x3200],
		.vrr = &s6e3hab_hubble_default_panel_vrr[S6E3HAB_VRR_48NS],
	},

	/* FHD */
	[S6E3HAB_DISPLAY_MODE_1080x2400_120HS] = {
		.name = PANEL_DISPLAY_MODE_1080x2400_120HS,
		.resol = &s6e3hab_hubble_default_resol[S6E3HAB_RESOL_1080x2400],
		.vrr = &s6e3hab_hubble_default_panel_vrr[S6E3HAB_VRR_120HS],
	},
	[S6E3HAB_DISPLAY_MODE_1080x2400_120HS_AID_4_CYCLE] = {
		.name = PANEL_DISPLAY_MODE_1080x2400_120HS_AID_4_CYCLE,
		.resol = &s6e3hab_hubble_default_resol[S6E3HAB_RESOL_1080x2400],
		.vrr = &s6e3hab_hubble_default_panel_vrr[S6E3HAB_VRR_120HS_AID_4_CYCLE],
	},
	[S6E3HAB_DISPLAY_MODE_1080x2400_112HS] = {
		.name = PANEL_DISPLAY_MODE_1080x2400_112HS,
		.resol = &s6e3hab_hubble_default_resol[S6E3HAB_RESOL_1080x2400],
		.vrr = &s6e3hab_hubble_default_panel_vrr[S6E3HAB_VRR_112HS],
	},
	[S6E3HAB_DISPLAY_MODE_1080x2400_110HS] = {
		.name = PANEL_DISPLAY_MODE_1080x2400_110HS,
		.resol = &s6e3hab_hubble_default_resol[S6E3HAB_RESOL_1080x2400],
		.vrr = &s6e3hab_hubble_default_panel_vrr[S6E3HAB_VRR_110HS],
	},
	[S6E3HAB_DISPLAY_MODE_1080x2400_104HS] = {
		.name = PANEL_DISPLAY_MODE_1080x2400_112HS,
		.resol = &s6e3hab_hubble_default_resol[S6E3HAB_RESOL_1080x2400],
		.vrr = &s6e3hab_hubble_default_panel_vrr[S6E3HAB_VRR_104HS],
	},
	[S6E3HAB_DISPLAY_MODE_1080x2400_100HS] = {
		.name = PANEL_DISPLAY_MODE_1080x2400_100HS,
		.resol = &s6e3hab_hubble_default_resol[S6E3HAB_RESOL_1080x2400],
		.vrr = &s6e3hab_hubble_default_panel_vrr[S6E3HAB_VRR_100HS],
	},
	[S6E3HAB_DISPLAY_MODE_1080x2400_96HS] = {
		.name = PANEL_DISPLAY_MODE_1080x2400_96HS,
		.resol = &s6e3hab_hubble_default_resol[S6E3HAB_RESOL_1080x2400],
		.vrr = &s6e3hab_hubble_default_panel_vrr[S6E3HAB_VRR_96HS],
	},
	[S6E3HAB_DISPLAY_MODE_1080x2400_70HS] = {
		.name = PANEL_DISPLAY_MODE_1080x2400_70HS,
		.resol = &s6e3hab_hubble_default_resol[S6E3HAB_RESOL_1080x2400],
		.vrr = &s6e3hab_hubble_default_panel_vrr[S6E3HAB_VRR_70HS],
	},
	[S6E3HAB_DISPLAY_MODE_1080x2400_60HS] = {
		.name = PANEL_DISPLAY_MODE_1080x2400_60HS,
		.resol = &s6e3hab_hubble_default_resol[S6E3HAB_RESOL_1080x2400],
		.vrr = &s6e3hab_hubble_default_panel_vrr[S6E3HAB_VRR_60HS],
	},
	[S6E3HAB_DISPLAY_MODE_1080x2400_60NS] = {
		.name = PANEL_DISPLAY_MODE_1080x2400_60NS,
		.resol = &s6e3hab_hubble_default_resol[S6E3HAB_RESOL_1080x2400],
		.vrr = &s6e3hab_hubble_default_panel_vrr[S6E3HAB_VRR_60NS],
	},
	[S6E3HAB_DISPLAY_MODE_1080x2400_52NS] = {
		.name = PANEL_DISPLAY_MODE_1080x2400_52NS,
		.resol = &s6e3hab_hubble_default_resol[S6E3HAB_RESOL_1080x2400],
		.vrr = &s6e3hab_hubble_default_panel_vrr[S6E3HAB_VRR_52NS],
	},
	[S6E3HAB_DISPLAY_MODE_1080x2400_48NS] = {
		.name = PANEL_DISPLAY_MODE_1080x2400_48NS,
		.resol = &s6e3hab_hubble_default_resol[S6E3HAB_RESOL_1080x2400],
		.vrr = &s6e3hab_hubble_default_panel_vrr[S6E3HAB_VRR_48NS],
	},

	/* HD */
	[S6E3HAB_DISPLAY_MODE_720x1600_120HS] = {
		.name = PANEL_DISPLAY_MODE_720x1600_120HS,
		.resol = &s6e3hab_hubble_default_resol[S6E3HAB_RESOL_720x1600],
		.vrr = &s6e3hab_hubble_default_panel_vrr[S6E3HAB_VRR_120HS],
	},
	[S6E3HAB_DISPLAY_MODE_720x1600_120HS_AID_4_CYCLE] = {
		.name = PANEL_DISPLAY_MODE_720x1600_120HS_AID_4_CYCLE,
		.resol = &s6e3hab_hubble_default_resol[S6E3HAB_RESOL_720x1600],
		.vrr = &s6e3hab_hubble_default_panel_vrr[S6E3HAB_VRR_120HS_AID_4_CYCLE],
	},
	[S6E3HAB_DISPLAY_MODE_720x1600_112HS] = {
		.name = PANEL_DISPLAY_MODE_720x1600_112HS,
		.resol = &s6e3hab_hubble_default_resol[S6E3HAB_RESOL_720x1600],
		.vrr = &s6e3hab_hubble_default_panel_vrr[S6E3HAB_VRR_112HS],
	},
	[S6E3HAB_DISPLAY_MODE_720x1600_110HS] = {
		.name = PANEL_DISPLAY_MODE_720x1600_110HS,
		.resol = &s6e3hab_hubble_default_resol[S6E3HAB_RESOL_720x1600],
		.vrr = &s6e3hab_hubble_default_panel_vrr[S6E3HAB_VRR_110HS],
	},
	[S6E3HAB_DISPLAY_MODE_720x1600_100HS] = {
		.name = PANEL_DISPLAY_MODE_720x1600_100HS,
		.resol = &s6e3hab_hubble_default_resol[S6E3HAB_RESOL_720x1600],
		.vrr = &s6e3hab_hubble_default_panel_vrr[S6E3HAB_VRR_100HS],
	},
	[S6E3HAB_DISPLAY_MODE_720x1600_104HS] = {
		.name = PANEL_DISPLAY_MODE_720x1600_104HS,
		.resol = &s6e3hab_hubble_default_resol[S6E3HAB_RESOL_720x1600],
		.vrr = &s6e3hab_hubble_default_panel_vrr[S6E3HAB_VRR_104HS],
	},
	[S6E3HAB_DISPLAY_MODE_720x1600_96HS] = {
		.name = PANEL_DISPLAY_MODE_720x1600_96HS,
		.resol = &s6e3hab_hubble_default_resol[S6E3HAB_RESOL_720x1600],
		.vrr = &s6e3hab_hubble_default_panel_vrr[S6E3HAB_VRR_96HS],
	},
	[S6E3HAB_DISPLAY_MODE_720x1600_70HS] = {
		.name = PANEL_DISPLAY_MODE_720x1600_70HS,
		.resol = &s6e3hab_hubble_default_resol[S6E3HAB_RESOL_720x1600],
		.vrr = &s6e3hab_hubble_default_panel_vrr[S6E3HAB_VRR_70HS],
	},
	[S6E3HAB_DISPLAY_MODE_720x1600_60HS] = {
		.name = PANEL_DISPLAY_MODE_720x1600_60HS,
		.resol = &s6e3hab_hubble_default_resol[S6E3HAB_RESOL_720x1600],
		.vrr = &s6e3hab_hubble_default_panel_vrr[S6E3HAB_VRR_60HS],
	},
	[S6E3HAB_DISPLAY_MODE_720x1600_60NS] = {
		.name = PANEL_DISPLAY_MODE_720x1600_60NS,
		.resol = &s6e3hab_hubble_default_resol[S6E3HAB_RESOL_720x1600],
		.vrr = &s6e3hab_hubble_default_panel_vrr[S6E3HAB_VRR_60NS],
	},
	[S6E3HAB_DISPLAY_MODE_720x1600_52NS] = {
		.name = PANEL_DISPLAY_MODE_720x1600_52NS,
		.resol = &s6e3hab_hubble_default_resol[S6E3HAB_RESOL_720x1600],
		.vrr = &s6e3hab_hubble_default_panel_vrr[S6E3HAB_VRR_52NS],
	},
	[S6E3HAB_DISPLAY_MODE_720x1600_48NS] = {
		.name = PANEL_DISPLAY_MODE_720x1600_48NS,
		.resol = &s6e3hab_hubble_default_resol[S6E3HAB_RESOL_720x1600],
		.vrr = &s6e3hab_hubble_default_panel_vrr[S6E3HAB_VRR_48NS],
	},
};

static struct common_panel_display_mode s6e3hab_hubble_rev03_display_mode[MAX_S6E3HAB_DISPLAY_MODE] = {
	/* WQHD */
	[S6E3HAB_DISPLAY_MODE_1440x3200_60NS] = {
		.name = PANEL_DISPLAY_MODE_1440x3200_60NS,
		.resol = &s6e3hab_hubble_rev03_resol[S6E3HAB_RESOL_1440x3200],
		.vrr = &s6e3hab_hubble_rev03_panel_vrr[S6E3HAB_VRR_60NS],
	},
	[S6E3HAB_DISPLAY_MODE_1440x3200_52NS] = {
		.name = PANEL_DISPLAY_MODE_1440x3200_52NS,
		.resol = &s6e3hab_hubble_rev03_resol[S6E3HAB_RESOL_1440x3200],
		.vrr = &s6e3hab_hubble_rev03_panel_vrr[S6E3HAB_VRR_52NS],
	},
	[S6E3HAB_DISPLAY_MODE_1440x3200_48NS] = {
		.name = PANEL_DISPLAY_MODE_1440x3200_48NS,
		.resol = &s6e3hab_hubble_rev03_resol[S6E3HAB_RESOL_1440x3200],
		.vrr = &s6e3hab_hubble_rev03_panel_vrr[S6E3HAB_VRR_48NS],
	},

	/* FHD */
	[S6E3HAB_DISPLAY_MODE_1080x2400_120HS] = {
		.name = PANEL_DISPLAY_MODE_1080x2400_120HS,
		.resol = &s6e3hab_hubble_rev03_resol[S6E3HAB_RESOL_1080x2400],
		.vrr = &s6e3hab_hubble_rev03_panel_vrr[S6E3HAB_VRR_120HS],
	},
	[S6E3HAB_DISPLAY_MODE_1080x2400_120HS_AID_4_CYCLE] = {
		.name = PANEL_DISPLAY_MODE_1080x2400_120HS_AID_4_CYCLE,
		.resol = &s6e3hab_hubble_rev03_resol[S6E3HAB_RESOL_1080x2400],
		.vrr = &s6e3hab_hubble_rev03_panel_vrr[S6E3HAB_VRR_120HS_AID_4_CYCLE],
	},
	[S6E3HAB_DISPLAY_MODE_1080x2400_112HS] = {
		.name = PANEL_DISPLAY_MODE_1080x2400_112HS,
		.resol = &s6e3hab_hubble_rev03_resol[S6E3HAB_RESOL_1080x2400],
		.vrr = &s6e3hab_hubble_rev03_panel_vrr[S6E3HAB_VRR_112HS],
	},
	[S6E3HAB_DISPLAY_MODE_1080x2400_110HS] = {
		.name = PANEL_DISPLAY_MODE_1080x2400_110HS,
		.resol = &s6e3hab_hubble_rev03_resol[S6E3HAB_RESOL_1080x2400],
		.vrr = &s6e3hab_hubble_rev03_panel_vrr[S6E3HAB_VRR_110HS],
	},
	[S6E3HAB_DISPLAY_MODE_1080x2400_104HS] = {
		.name = PANEL_DISPLAY_MODE_1080x2400_112HS,
		.resol = &s6e3hab_hubble_rev03_resol[S6E3HAB_RESOL_1080x2400],
		.vrr = &s6e3hab_hubble_rev03_panel_vrr[S6E3HAB_VRR_104HS],
	},
	[S6E3HAB_DISPLAY_MODE_1080x2400_100HS] = {
		.name = PANEL_DISPLAY_MODE_1080x2400_100HS,
		.resol = &s6e3hab_hubble_rev03_resol[S6E3HAB_RESOL_1080x2400],
		.vrr = &s6e3hab_hubble_rev03_panel_vrr[S6E3HAB_VRR_100HS],
	},
	[S6E3HAB_DISPLAY_MODE_1080x2400_96HS] = {
		.name = PANEL_DISPLAY_MODE_1080x2400_96HS,
		.resol = &s6e3hab_hubble_rev03_resol[S6E3HAB_RESOL_1080x2400],
		.vrr = &s6e3hab_hubble_rev03_panel_vrr[S6E3HAB_VRR_96HS],
	},
	[S6E3HAB_DISPLAY_MODE_1080x2400_70HS] = {
		.name = PANEL_DISPLAY_MODE_1080x2400_70HS,
		.resol = &s6e3hab_hubble_rev03_resol[S6E3HAB_RESOL_1080x2400],
		.vrr = &s6e3hab_hubble_rev03_panel_vrr[S6E3HAB_VRR_70HS],
	},
	[S6E3HAB_DISPLAY_MODE_1080x2400_60HS] = {
		.name = PANEL_DISPLAY_MODE_1080x2400_60HS,
		.resol = &s6e3hab_hubble_rev03_resol[S6E3HAB_RESOL_1080x2400],
		.vrr = &s6e3hab_hubble_rev03_panel_vrr[S6E3HAB_VRR_60HS],
	},
	[S6E3HAB_DISPLAY_MODE_1080x2400_60NS] = {
		.name = PANEL_DISPLAY_MODE_1080x2400_60NS,
		.resol = &s6e3hab_hubble_rev03_resol[S6E3HAB_RESOL_1080x2400],
		.vrr = &s6e3hab_hubble_rev03_panel_vrr[S6E3HAB_VRR_60NS],
	},
	[S6E3HAB_DISPLAY_MODE_1080x2400_52NS] = {
		.name = PANEL_DISPLAY_MODE_1080x2400_52NS,
		.resol = &s6e3hab_hubble_rev03_resol[S6E3HAB_RESOL_1080x2400],
		.vrr = &s6e3hab_hubble_rev03_panel_vrr[S6E3HAB_VRR_52NS],
	},
	[S6E3HAB_DISPLAY_MODE_1080x2400_48NS] = {
		.name = PANEL_DISPLAY_MODE_1080x2400_48NS,
		.resol = &s6e3hab_hubble_rev03_resol[S6E3HAB_RESOL_1080x2400],
		.vrr = &s6e3hab_hubble_rev03_panel_vrr[S6E3HAB_VRR_48NS],
	},

	/* HD */
	[S6E3HAB_DISPLAY_MODE_720x1600_120HS] = {
		.name = PANEL_DISPLAY_MODE_720x1600_120HS,
		.resol = &s6e3hab_hubble_rev03_resol[S6E3HAB_RESOL_720x1600],
		.vrr = &s6e3hab_hubble_rev03_panel_vrr[S6E3HAB_VRR_120HS],
	},
	[S6E3HAB_DISPLAY_MODE_720x1600_120HS_AID_4_CYCLE] = {
		.name = PANEL_DISPLAY_MODE_720x1600_120HS_AID_4_CYCLE,
		.resol = &s6e3hab_hubble_rev03_resol[S6E3HAB_RESOL_720x1600],
		.vrr = &s6e3hab_hubble_rev03_panel_vrr[S6E3HAB_VRR_120HS_AID_4_CYCLE],
	},
	[S6E3HAB_DISPLAY_MODE_720x1600_112HS] = {
		.name = PANEL_DISPLAY_MODE_720x1600_112HS,
		.resol = &s6e3hab_hubble_rev03_resol[S6E3HAB_RESOL_720x1600],
		.vrr = &s6e3hab_hubble_rev03_panel_vrr[S6E3HAB_VRR_112HS],
	},
	[S6E3HAB_DISPLAY_MODE_720x1600_110HS] = {
		.name = PANEL_DISPLAY_MODE_720x1600_110HS,
		.resol = &s6e3hab_hubble_rev03_resol[S6E3HAB_RESOL_720x1600],
		.vrr = &s6e3hab_hubble_rev03_panel_vrr[S6E3HAB_VRR_110HS],
	},
	[S6E3HAB_DISPLAY_MODE_720x1600_104HS] = {
		.name = PANEL_DISPLAY_MODE_720x1600_104HS,
		.resol = &s6e3hab_hubble_rev03_resol[S6E3HAB_RESOL_720x1600],
		.vrr = &s6e3hab_hubble_rev03_panel_vrr[S6E3HAB_VRR_104HS],
	},
	[S6E3HAB_DISPLAY_MODE_720x1600_100HS] = {
		.name = PANEL_DISPLAY_MODE_720x1600_100HS,
		.resol = &s6e3hab_hubble_rev03_resol[S6E3HAB_RESOL_720x1600],
		.vrr = &s6e3hab_hubble_rev03_panel_vrr[S6E3HAB_VRR_100HS],
	},
	[S6E3HAB_DISPLAY_MODE_720x1600_96HS] = {
		.name = PANEL_DISPLAY_MODE_720x1600_96HS,
		.resol = &s6e3hab_hubble_rev03_resol[S6E3HAB_RESOL_720x1600],
		.vrr = &s6e3hab_hubble_rev03_panel_vrr[S6E3HAB_VRR_96HS],
	},
	[S6E3HAB_DISPLAY_MODE_720x1600_70HS] = {
		.name = PANEL_DISPLAY_MODE_720x1600_70HS,
		.resol = &s6e3hab_hubble_rev03_resol[S6E3HAB_RESOL_720x1600],
		.vrr = &s6e3hab_hubble_rev03_panel_vrr[S6E3HAB_VRR_70HS],
	},
	[S6E3HAB_DISPLAY_MODE_720x1600_60HS] = {
		.name = PANEL_DISPLAY_MODE_720x1600_60HS,
		.resol = &s6e3hab_hubble_rev03_resol[S6E3HAB_RESOL_720x1600],
		.vrr = &s6e3hab_hubble_rev03_panel_vrr[S6E3HAB_VRR_60HS],
	},
	[S6E3HAB_DISPLAY_MODE_720x1600_60NS] = {
		.name = PANEL_DISPLAY_MODE_720x1600_60NS,
		.resol = &s6e3hab_hubble_rev03_resol[S6E3HAB_RESOL_720x1600],
		.vrr = &s6e3hab_hubble_rev03_panel_vrr[S6E3HAB_VRR_60NS],
	},
	[S6E3HAB_DISPLAY_MODE_720x1600_52NS] = {
		.name = PANEL_DISPLAY_MODE_720x1600_52NS,
		.resol = &s6e3hab_hubble_rev03_resol[S6E3HAB_RESOL_720x1600],
		.vrr = &s6e3hab_hubble_rev03_panel_vrr[S6E3HAB_VRR_52NS],
	},
	[S6E3HAB_DISPLAY_MODE_720x1600_48NS] = {
		.name = PANEL_DISPLAY_MODE_720x1600_48NS,
		.resol = &s6e3hab_hubble_rev03_resol[S6E3HAB_RESOL_720x1600],
		.vrr = &s6e3hab_hubble_rev03_panel_vrr[S6E3HAB_VRR_48NS],
	},
};

static struct common_panel_display_mode *s6e3hab_hubble_preliminary_display_mode_array[MAX_S6E3HAB_DISPLAY_MODE] = {
	[S6E3HAB_DISPLAY_MODE_1440x3200_60NS] = &s6e3hab_hubble_preliminary_display_mode[S6E3HAB_DISPLAY_MODE_1440x3200_60NS],
	[S6E3HAB_DISPLAY_MODE_1440x3200_48NS] = &s6e3hab_hubble_preliminary_display_mode[S6E3HAB_DISPLAY_MODE_1440x3200_48NS],
	[S6E3HAB_DISPLAY_MODE_1080x2400_60NS] = &s6e3hab_hubble_preliminary_display_mode[S6E3HAB_DISPLAY_MODE_1080x2400_60NS],
	[S6E3HAB_DISPLAY_MODE_1080x2400_48NS] = &s6e3hab_hubble_preliminary_display_mode[S6E3HAB_DISPLAY_MODE_1080x2400_48NS],
	[S6E3HAB_DISPLAY_MODE_720x1600_60NS] = &s6e3hab_hubble_preliminary_display_mode[S6E3HAB_DISPLAY_MODE_720x1600_60NS],
	[S6E3HAB_DISPLAY_MODE_720x1600_48NS] = &s6e3hab_hubble_preliminary_display_mode[S6E3HAB_DISPLAY_MODE_720x1600_48NS],
};

static struct common_panel_display_mode *s6e3hab_hubble_display_mode_array[MAX_S6E3HAB_DISPLAY_MODE] = {
	[S6E3HAB_DISPLAY_MODE_1440x3200_60NS] = &s6e3hab_hubble_display_mode[S6E3HAB_DISPLAY_MODE_1440x3200_60NS],
	[S6E3HAB_DISPLAY_MODE_1440x3200_52NS] = &s6e3hab_hubble_display_mode[S6E3HAB_DISPLAY_MODE_1440x3200_52NS],
	[S6E3HAB_DISPLAY_MODE_1440x3200_48NS] = &s6e3hab_hubble_display_mode[S6E3HAB_DISPLAY_MODE_1440x3200_48NS],
	[S6E3HAB_DISPLAY_MODE_1080x2400_120HS] = &s6e3hab_hubble_display_mode[S6E3HAB_DISPLAY_MODE_1080x2400_120HS],
	[S6E3HAB_DISPLAY_MODE_1080x2400_120HS_AID_4_CYCLE] = &s6e3hab_hubble_display_mode[S6E3HAB_DISPLAY_MODE_1080x2400_120HS_AID_4_CYCLE],
	[S6E3HAB_DISPLAY_MODE_1080x2400_112HS] = &s6e3hab_hubble_display_mode[S6E3HAB_DISPLAY_MODE_1080x2400_112HS],
	[S6E3HAB_DISPLAY_MODE_1080x2400_110HS] = &s6e3hab_hubble_display_mode[S6E3HAB_DISPLAY_MODE_1080x2400_110HS],
	[S6E3HAB_DISPLAY_MODE_1080x2400_104HS] = &s6e3hab_hubble_display_mode[S6E3HAB_DISPLAY_MODE_1080x2400_104HS],
	[S6E3HAB_DISPLAY_MODE_1080x2400_100HS] = &s6e3hab_hubble_display_mode[S6E3HAB_DISPLAY_MODE_1080x2400_100HS],
	[S6E3HAB_DISPLAY_MODE_1080x2400_96HS] = &s6e3hab_hubble_display_mode[S6E3HAB_DISPLAY_MODE_1080x2400_96HS],
	[S6E3HAB_DISPLAY_MODE_1080x2400_70HS] = &s6e3hab_hubble_display_mode[S6E3HAB_DISPLAY_MODE_1080x2400_70HS],
	[S6E3HAB_DISPLAY_MODE_1080x2400_60HS] = &s6e3hab_hubble_display_mode[S6E3HAB_DISPLAY_MODE_1080x2400_60HS],
	[S6E3HAB_DISPLAY_MODE_1080x2400_60NS] = &s6e3hab_hubble_display_mode[S6E3HAB_DISPLAY_MODE_1080x2400_60NS],
	[S6E3HAB_DISPLAY_MODE_1080x2400_52NS] = &s6e3hab_hubble_display_mode[S6E3HAB_DISPLAY_MODE_1080x2400_52NS],
	[S6E3HAB_DISPLAY_MODE_1080x2400_48NS] = &s6e3hab_hubble_display_mode[S6E3HAB_DISPLAY_MODE_1080x2400_48NS],
	[S6E3HAB_DISPLAY_MODE_720x1600_120HS] = &s6e3hab_hubble_display_mode[S6E3HAB_DISPLAY_MODE_720x1600_120HS],
	[S6E3HAB_DISPLAY_MODE_720x1600_120HS_AID_4_CYCLE] = &s6e3hab_hubble_display_mode[S6E3HAB_DISPLAY_MODE_720x1600_120HS_AID_4_CYCLE],
	[S6E3HAB_DISPLAY_MODE_720x1600_112HS] = &s6e3hab_hubble_display_mode[S6E3HAB_DISPLAY_MODE_720x1600_112HS],
	[S6E3HAB_DISPLAY_MODE_720x1600_110HS] = &s6e3hab_hubble_display_mode[S6E3HAB_DISPLAY_MODE_720x1600_110HS],
	[S6E3HAB_DISPLAY_MODE_720x1600_104HS] = &s6e3hab_hubble_display_mode[S6E3HAB_DISPLAY_MODE_720x1600_104HS],
	[S6E3HAB_DISPLAY_MODE_720x1600_100HS] = &s6e3hab_hubble_display_mode[S6E3HAB_DISPLAY_MODE_720x1600_100HS],
	[S6E3HAB_DISPLAY_MODE_720x1600_96HS] = &s6e3hab_hubble_display_mode[S6E3HAB_DISPLAY_MODE_720x1600_96HS],
	[S6E3HAB_DISPLAY_MODE_720x1600_70HS] = &s6e3hab_hubble_display_mode[S6E3HAB_DISPLAY_MODE_720x1600_70HS],
	[S6E3HAB_DISPLAY_MODE_720x1600_60HS] = &s6e3hab_hubble_display_mode[S6E3HAB_DISPLAY_MODE_720x1600_60HS],
	[S6E3HAB_DISPLAY_MODE_720x1600_60NS] = &s6e3hab_hubble_display_mode[S6E3HAB_DISPLAY_MODE_720x1600_60NS],
	[S6E3HAB_DISPLAY_MODE_720x1600_52NS] = &s6e3hab_hubble_display_mode[S6E3HAB_DISPLAY_MODE_720x1600_52NS],
	[S6E3HAB_DISPLAY_MODE_720x1600_48NS] = &s6e3hab_hubble_display_mode[S6E3HAB_DISPLAY_MODE_720x1600_48NS],
};

static struct common_panel_display_mode *s6e3hab_hubble_rev03_display_mode_array[MAX_S6E3HAB_DISPLAY_MODE] = {
	[S6E3HAB_DISPLAY_MODE_1440x3200_60NS] = &s6e3hab_hubble_rev03_display_mode[S6E3HAB_DISPLAY_MODE_1440x3200_60NS],
	[S6E3HAB_DISPLAY_MODE_1440x3200_52NS] = &s6e3hab_hubble_rev03_display_mode[S6E3HAB_DISPLAY_MODE_1440x3200_52NS],
	[S6E3HAB_DISPLAY_MODE_1440x3200_48NS] = &s6e3hab_hubble_rev03_display_mode[S6E3HAB_DISPLAY_MODE_1440x3200_48NS],
	[S6E3HAB_DISPLAY_MODE_1080x2400_120HS] = &s6e3hab_hubble_rev03_display_mode[S6E3HAB_DISPLAY_MODE_1080x2400_120HS],
	[S6E3HAB_DISPLAY_MODE_1080x2400_120HS_AID_4_CYCLE] = &s6e3hab_hubble_rev03_display_mode[S6E3HAB_DISPLAY_MODE_1080x2400_120HS_AID_4_CYCLE],
	[S6E3HAB_DISPLAY_MODE_1080x2400_112HS] = &s6e3hab_hubble_rev03_display_mode[S6E3HAB_DISPLAY_MODE_1080x2400_112HS],
	[S6E3HAB_DISPLAY_MODE_1080x2400_110HS] = &s6e3hab_hubble_rev03_display_mode[S6E3HAB_DISPLAY_MODE_1080x2400_110HS],
	[S6E3HAB_DISPLAY_MODE_1080x2400_104HS] = &s6e3hab_hubble_rev03_display_mode[S6E3HAB_DISPLAY_MODE_1080x2400_104HS],
	[S6E3HAB_DISPLAY_MODE_1080x2400_100HS] = &s6e3hab_hubble_rev03_display_mode[S6E3HAB_DISPLAY_MODE_1080x2400_100HS],
	[S6E3HAB_DISPLAY_MODE_1080x2400_96HS] = &s6e3hab_hubble_rev03_display_mode[S6E3HAB_DISPLAY_MODE_1080x2400_96HS],
	[S6E3HAB_DISPLAY_MODE_1080x2400_70HS] = &s6e3hab_hubble_rev03_display_mode[S6E3HAB_DISPLAY_MODE_1080x2400_70HS],
	[S6E3HAB_DISPLAY_MODE_1080x2400_60HS] = &s6e3hab_hubble_rev03_display_mode[S6E3HAB_DISPLAY_MODE_1080x2400_60HS],
	[S6E3HAB_DISPLAY_MODE_1080x2400_60NS] = &s6e3hab_hubble_rev03_display_mode[S6E3HAB_DISPLAY_MODE_1080x2400_60NS],
	[S6E3HAB_DISPLAY_MODE_1080x2400_52NS] = &s6e3hab_hubble_rev03_display_mode[S6E3HAB_DISPLAY_MODE_1080x2400_52NS],
	[S6E3HAB_DISPLAY_MODE_1080x2400_48NS] = &s6e3hab_hubble_rev03_display_mode[S6E3HAB_DISPLAY_MODE_1080x2400_48NS],
	[S6E3HAB_DISPLAY_MODE_720x1600_120HS] = &s6e3hab_hubble_rev03_display_mode[S6E3HAB_DISPLAY_MODE_720x1600_120HS],
	[S6E3HAB_DISPLAY_MODE_720x1600_120HS_AID_4_CYCLE] = &s6e3hab_hubble_rev03_display_mode[S6E3HAB_DISPLAY_MODE_720x1600_120HS_AID_4_CYCLE],
	[S6E3HAB_DISPLAY_MODE_720x1600_112HS] = &s6e3hab_hubble_rev03_display_mode[S6E3HAB_DISPLAY_MODE_720x1600_112HS],
	[S6E3HAB_DISPLAY_MODE_720x1600_110HS] = &s6e3hab_hubble_rev03_display_mode[S6E3HAB_DISPLAY_MODE_720x1600_110HS],
	[S6E3HAB_DISPLAY_MODE_720x1600_104HS] = &s6e3hab_hubble_rev03_display_mode[S6E3HAB_DISPLAY_MODE_720x1600_104HS],
	[S6E3HAB_DISPLAY_MODE_720x1600_100HS] = &s6e3hab_hubble_rev03_display_mode[S6E3HAB_DISPLAY_MODE_720x1600_100HS],
	[S6E3HAB_DISPLAY_MODE_720x1600_96HS] = &s6e3hab_hubble_rev03_display_mode[S6E3HAB_DISPLAY_MODE_720x1600_96HS],
	[S6E3HAB_DISPLAY_MODE_720x1600_70HS] = &s6e3hab_hubble_rev03_display_mode[S6E3HAB_DISPLAY_MODE_720x1600_70HS],
	[S6E3HAB_DISPLAY_MODE_720x1600_60HS] = &s6e3hab_hubble_rev03_display_mode[S6E3HAB_DISPLAY_MODE_720x1600_60HS],
	[S6E3HAB_DISPLAY_MODE_720x1600_60NS] = &s6e3hab_hubble_rev03_display_mode[S6E3HAB_DISPLAY_MODE_720x1600_60NS],
	[S6E3HAB_DISPLAY_MODE_720x1600_52NS] = &s6e3hab_hubble_rev03_display_mode[S6E3HAB_DISPLAY_MODE_720x1600_52NS],
	[S6E3HAB_DISPLAY_MODE_720x1600_48NS] = &s6e3hab_hubble_rev03_display_mode[S6E3HAB_DISPLAY_MODE_720x1600_48NS],
};

#ifdef CONFIG_PANEL_VRR_BRIDGE
static struct common_panel_display_mode_bridge s6e3hab_hubble_display_mode_bridge[MAX_S6E3HAB_DISPLAY_MODE][MAX_S6E3HAB_DISPLAY_MODE] = {
	/* WQHD */
	[S6E3HAB_DISPLAY_MODE_1440x3200_60NS] = {
		[S6E3HAB_DISPLAY_MODE_1440x3200_48NS] = { .mode = &s6e3hab_hubble_display_mode[S6E3HAB_DISPLAY_MODE_1440x3200_52NS], .nframe_duration = 4, },
	},
	[S6E3HAB_DISPLAY_MODE_1440x3200_52NS] = {
		[S6E3HAB_DISPLAY_MODE_1440x3200_60NS] = { .mode = &s6e3hab_hubble_display_mode[S6E3HAB_DISPLAY_MODE_1440x3200_60NS], .nframe_duration = 1, },
		[S6E3HAB_DISPLAY_MODE_1440x3200_48NS] = { .mode = &s6e3hab_hubble_display_mode[S6E3HAB_DISPLAY_MODE_1440x3200_48NS], .nframe_duration = 1, },
	},
	[S6E3HAB_DISPLAY_MODE_1440x3200_48NS] = {
		[S6E3HAB_DISPLAY_MODE_1440x3200_60NS] = { .mode = &s6e3hab_hubble_display_mode[S6E3HAB_DISPLAY_MODE_1440x3200_52NS], .nframe_duration = 4, },
	},

	/* FHD */
	[S6E3HAB_DISPLAY_MODE_1080x2400_120HS] = {
		[S6E3HAB_DISPLAY_MODE_1080x2400_96HS] = { .mode = &s6e3hab_hubble_display_mode[S6E3HAB_DISPLAY_MODE_1080x2400_112HS], .nframe_duration = 4, },
		[S6E3HAB_DISPLAY_MODE_1080x2400_60HS] = { .mode = &s6e3hab_hubble_display_mode[S6E3HAB_DISPLAY_MODE_1080x2400_110HS], .nframe_duration = 1, },
	},
	[S6E3HAB_DISPLAY_MODE_1080x2400_112HS] = {
		[S6E3HAB_DISPLAY_MODE_1080x2400_120HS] = { .mode = &s6e3hab_hubble_display_mode[S6E3HAB_DISPLAY_MODE_1080x2400_120HS], .nframe_duration = 1, },
		[S6E3HAB_DISPLAY_MODE_1080x2400_96HS] = { .mode = &s6e3hab_hubble_display_mode[S6E3HAB_DISPLAY_MODE_1080x2400_104HS], .nframe_duration = 4, },
		[S6E3HAB_DISPLAY_MODE_1080x2400_60HS] = { .mode = &s6e3hab_hubble_display_mode[S6E3HAB_DISPLAY_MODE_1080x2400_120HS], .nframe_duration = 4, },
	},
	[S6E3HAB_DISPLAY_MODE_1080x2400_110HS] = {
		[S6E3HAB_DISPLAY_MODE_1080x2400_120HS] = { .mode = &s6e3hab_hubble_display_mode[S6E3HAB_DISPLAY_MODE_1080x2400_120HS], .nframe_duration = 1, },
		[S6E3HAB_DISPLAY_MODE_1080x2400_96HS] = { .mode = &s6e3hab_hubble_display_mode[S6E3HAB_DISPLAY_MODE_1080x2400_120HS], .nframe_duration = 1, },
		[S6E3HAB_DISPLAY_MODE_1080x2400_60HS] = { .mode = &s6e3hab_hubble_display_mode[S6E3HAB_DISPLAY_MODE_1080x2400_100HS], .nframe_duration = 8, },
	},
	[S6E3HAB_DISPLAY_MODE_1080x2400_104HS] = {
		[S6E3HAB_DISPLAY_MODE_1080x2400_120HS] = { .mode = &s6e3hab_hubble_display_mode[S6E3HAB_DISPLAY_MODE_1080x2400_112HS], .nframe_duration = 4, },
		[S6E3HAB_DISPLAY_MODE_1080x2400_96HS] = { .mode = &s6e3hab_hubble_display_mode[S6E3HAB_DISPLAY_MODE_1080x2400_96HS], .nframe_duration = 1, },
		[S6E3HAB_DISPLAY_MODE_1080x2400_60HS] = { .mode = &s6e3hab_hubble_display_mode[S6E3HAB_DISPLAY_MODE_1080x2400_112HS], .nframe_duration = 4, },
	},
	[S6E3HAB_DISPLAY_MODE_1080x2400_100HS] = {
		[S6E3HAB_DISPLAY_MODE_1080x2400_120HS] = { .mode = &s6e3hab_hubble_display_mode[S6E3HAB_DISPLAY_MODE_1080x2400_110HS], .nframe_duration = 5, },
		[S6E3HAB_DISPLAY_MODE_1080x2400_96HS] = { .mode = &s6e3hab_hubble_display_mode[S6E3HAB_DISPLAY_MODE_1080x2400_110HS], .nframe_duration = 5, },
		[S6E3HAB_DISPLAY_MODE_1080x2400_60HS] = { .mode = &s6e3hab_hubble_display_mode[S6E3HAB_DISPLAY_MODE_1080x2400_70HS], .nframe_duration = 8, },
	},
	[S6E3HAB_DISPLAY_MODE_1080x2400_96HS] = {
		[S6E3HAB_DISPLAY_MODE_1080x2400_120HS] = { .mode = &s6e3hab_hubble_display_mode[S6E3HAB_DISPLAY_MODE_1080x2400_104HS], .nframe_duration = 4, },
		[S6E3HAB_DISPLAY_MODE_1080x2400_60HS] = { .mode = &s6e3hab_hubble_display_mode[S6E3HAB_DISPLAY_MODE_1080x2400_104HS], .nframe_duration = 4, },
	},
	[S6E3HAB_DISPLAY_MODE_1080x2400_70HS] = {
		[S6E3HAB_DISPLAY_MODE_1080x2400_120HS] = { .mode = &s6e3hab_hubble_display_mode[S6E3HAB_DISPLAY_MODE_1080x2400_100HS], .nframe_duration = 5, },
		[S6E3HAB_DISPLAY_MODE_1080x2400_96HS] = { .mode = &s6e3hab_hubble_display_mode[S6E3HAB_DISPLAY_MODE_1080x2400_100HS], .nframe_duration = 5, },
		[S6E3HAB_DISPLAY_MODE_1080x2400_60HS] = { .mode = &s6e3hab_hubble_display_mode[S6E3HAB_DISPLAY_MODE_1080x2400_60HS], .nframe_duration = 1, },
	},
	[S6E3HAB_DISPLAY_MODE_1080x2400_60HS] = {
		[S6E3HAB_DISPLAY_MODE_1080x2400_120HS] = { .mode = &s6e3hab_hubble_display_mode[S6E3HAB_DISPLAY_MODE_1080x2400_100HS], .nframe_duration = 5, },
		[S6E3HAB_DISPLAY_MODE_1080x2400_96HS] = { .mode = &s6e3hab_hubble_display_mode[S6E3HAB_DISPLAY_MODE_1080x2400_100HS], .nframe_duration = 5, },
	},
	[S6E3HAB_DISPLAY_MODE_1080x2400_60NS] = {
		[S6E3HAB_DISPLAY_MODE_1080x2400_48NS] = { .mode = &s6e3hab_hubble_display_mode[S6E3HAB_DISPLAY_MODE_1080x2400_52NS], .nframe_duration = 4, },
	},
	[S6E3HAB_DISPLAY_MODE_1080x2400_52NS] = {
		[S6E3HAB_DISPLAY_MODE_1080x2400_60NS] = { .mode = &s6e3hab_hubble_display_mode[S6E3HAB_DISPLAY_MODE_1080x2400_60NS], .nframe_duration = 1, },
		[S6E3HAB_DISPLAY_MODE_1080x2400_48NS] = { .mode = &s6e3hab_hubble_display_mode[S6E3HAB_DISPLAY_MODE_1080x2400_48NS], .nframe_duration = 1, },
	},
	[S6E3HAB_DISPLAY_MODE_1080x2400_48NS] = {
		[S6E3HAB_DISPLAY_MODE_1080x2400_60NS] = { .mode = &s6e3hab_hubble_display_mode[S6E3HAB_DISPLAY_MODE_1080x2400_52NS], .nframe_duration = 4, },
	},

	/* HD */
	[S6E3HAB_DISPLAY_MODE_720x1600_120HS] = {
		[S6E3HAB_DISPLAY_MODE_720x1600_96HS] = { .mode = &s6e3hab_hubble_display_mode[S6E3HAB_DISPLAY_MODE_720x1600_112HS], .nframe_duration = 4, },
		[S6E3HAB_DISPLAY_MODE_720x1600_60HS] = { .mode = &s6e3hab_hubble_display_mode[S6E3HAB_DISPLAY_MODE_720x1600_110HS], .nframe_duration = 1, },
	},
	[S6E3HAB_DISPLAY_MODE_720x1600_112HS] = {
		[S6E3HAB_DISPLAY_MODE_720x1600_120HS] = { .mode = &s6e3hab_hubble_display_mode[S6E3HAB_DISPLAY_MODE_720x1600_120HS], .nframe_duration = 1, },
		[S6E3HAB_DISPLAY_MODE_720x1600_96HS] = { .mode = &s6e3hab_hubble_display_mode[S6E3HAB_DISPLAY_MODE_720x1600_104HS], .nframe_duration = 4, },
		[S6E3HAB_DISPLAY_MODE_720x1600_60HS] = { .mode = &s6e3hab_hubble_display_mode[S6E3HAB_DISPLAY_MODE_720x1600_120HS], .nframe_duration = 4, },
	},
	[S6E3HAB_DISPLAY_MODE_720x1600_110HS] = {
		[S6E3HAB_DISPLAY_MODE_720x1600_120HS] = { .mode = &s6e3hab_hubble_display_mode[S6E3HAB_DISPLAY_MODE_720x1600_120HS], .nframe_duration = 1, },
		[S6E3HAB_DISPLAY_MODE_720x1600_96HS] = { .mode = &s6e3hab_hubble_display_mode[S6E3HAB_DISPLAY_MODE_720x1600_120HS], .nframe_duration = 1, },
		[S6E3HAB_DISPLAY_MODE_720x1600_60HS] = { .mode = &s6e3hab_hubble_display_mode[S6E3HAB_DISPLAY_MODE_720x1600_100HS], .nframe_duration = 8, },
	},
	[S6E3HAB_DISPLAY_MODE_720x1600_104HS] = {
		[S6E3HAB_DISPLAY_MODE_720x1600_120HS] = { .mode = &s6e3hab_hubble_display_mode[S6E3HAB_DISPLAY_MODE_720x1600_112HS], .nframe_duration = 4, },
		[S6E3HAB_DISPLAY_MODE_720x1600_96HS] = { .mode = &s6e3hab_hubble_display_mode[S6E3HAB_DISPLAY_MODE_720x1600_96HS], .nframe_duration = 1, },
		[S6E3HAB_DISPLAY_MODE_720x1600_60HS] = { .mode = &s6e3hab_hubble_display_mode[S6E3HAB_DISPLAY_MODE_720x1600_112HS], .nframe_duration = 4, },
	},
	[S6E3HAB_DISPLAY_MODE_720x1600_100HS] = {
		[S6E3HAB_DISPLAY_MODE_720x1600_120HS] = { .mode = &s6e3hab_hubble_display_mode[S6E3HAB_DISPLAY_MODE_720x1600_110HS], .nframe_duration = 5, },
		[S6E3HAB_DISPLAY_MODE_720x1600_96HS] = { .mode = &s6e3hab_hubble_display_mode[S6E3HAB_DISPLAY_MODE_720x1600_110HS], .nframe_duration = 5, },
		[S6E3HAB_DISPLAY_MODE_720x1600_60HS] = { .mode = &s6e3hab_hubble_display_mode[S6E3HAB_DISPLAY_MODE_720x1600_70HS], .nframe_duration = 8, },
	},
	[S6E3HAB_DISPLAY_MODE_720x1600_96HS] = {
		[S6E3HAB_DISPLAY_MODE_720x1600_120HS] = { .mode = &s6e3hab_hubble_display_mode[S6E3HAB_DISPLAY_MODE_720x1600_104HS], .nframe_duration = 4, },
		[S6E3HAB_DISPLAY_MODE_720x1600_60HS] = { .mode = &s6e3hab_hubble_display_mode[S6E3HAB_DISPLAY_MODE_720x1600_104HS], .nframe_duration = 4, },
	},
	[S6E3HAB_DISPLAY_MODE_720x1600_70HS] = {
		[S6E3HAB_DISPLAY_MODE_720x1600_120HS] = { .mode = &s6e3hab_hubble_display_mode[S6E3HAB_DISPLAY_MODE_720x1600_100HS], .nframe_duration = 5, },
		[S6E3HAB_DISPLAY_MODE_720x1600_96HS] = { .mode = &s6e3hab_hubble_display_mode[S6E3HAB_DISPLAY_MODE_720x1600_100HS], .nframe_duration = 5, },
		[S6E3HAB_DISPLAY_MODE_720x1600_60HS] = { .mode = &s6e3hab_hubble_display_mode[S6E3HAB_DISPLAY_MODE_720x1600_60HS], .nframe_duration = 1, },
	},
	[S6E3HAB_DISPLAY_MODE_720x1600_60HS] = {
		[S6E3HAB_DISPLAY_MODE_720x1600_120HS] = { .mode = &s6e3hab_hubble_display_mode[S6E3HAB_DISPLAY_MODE_720x1600_100HS], .nframe_duration = 5, },
		[S6E3HAB_DISPLAY_MODE_720x1600_96HS] = { .mode = &s6e3hab_hubble_display_mode[S6E3HAB_DISPLAY_MODE_720x1600_100HS], .nframe_duration = 5, },
	},
	[S6E3HAB_DISPLAY_MODE_720x1600_60NS] = {
		[S6E3HAB_DISPLAY_MODE_720x1600_48NS] = { .mode = &s6e3hab_hubble_display_mode[S6E3HAB_DISPLAY_MODE_720x1600_52NS], .nframe_duration = 4, },
	},
	[S6E3HAB_DISPLAY_MODE_720x1600_52NS] = {
		[S6E3HAB_DISPLAY_MODE_720x1600_60NS] = { .mode = &s6e3hab_hubble_display_mode[S6E3HAB_DISPLAY_MODE_720x1600_60NS], .nframe_duration = 1, },
		[S6E3HAB_DISPLAY_MODE_720x1600_48NS] = { .mode = &s6e3hab_hubble_display_mode[S6E3HAB_DISPLAY_MODE_720x1600_48NS], .nframe_duration = 1, },
	},
	[S6E3HAB_DISPLAY_MODE_720x1600_48NS] = {
		[S6E3HAB_DISPLAY_MODE_720x1600_60NS] = { .mode = &s6e3hab_hubble_display_mode[S6E3HAB_DISPLAY_MODE_720x1600_52NS], .nframe_duration = 4, },
	},
};

static struct common_panel_display_mode_bridge s6e3hab_hubble_rev03_display_mode_bridge[MAX_S6E3HAB_DISPLAY_MODE][MAX_S6E3HAB_DISPLAY_MODE] = {
	/* WQHD */
	[S6E3HAB_DISPLAY_MODE_1440x3200_60NS] = {
		[S6E3HAB_DISPLAY_MODE_1440x3200_48NS] = { .mode = &s6e3hab_hubble_rev03_display_mode[S6E3HAB_DISPLAY_MODE_1440x3200_52NS], .nframe_duration = 4, },
	},
	[S6E3HAB_DISPLAY_MODE_1440x3200_52NS] = {
		[S6E3HAB_DISPLAY_MODE_1440x3200_60NS] = { .mode = &s6e3hab_hubble_rev03_display_mode[S6E3HAB_DISPLAY_MODE_1440x3200_60NS], .nframe_duration = 1, },
		[S6E3HAB_DISPLAY_MODE_1440x3200_48NS] = { .mode = &s6e3hab_hubble_rev03_display_mode[S6E3HAB_DISPLAY_MODE_1440x3200_48NS], .nframe_duration = 1, },
	},
	[S6E3HAB_DISPLAY_MODE_1440x3200_48NS] = {
		[S6E3HAB_DISPLAY_MODE_1440x3200_60NS] = { .mode = &s6e3hab_hubble_rev03_display_mode[S6E3HAB_DISPLAY_MODE_1440x3200_52NS], .nframe_duration = 4, },
	},

	/* FHD */
	[S6E3HAB_DISPLAY_MODE_1080x2400_120HS] = {
		[S6E3HAB_DISPLAY_MODE_1080x2400_96HS] = { .mode = &s6e3hab_hubble_rev03_display_mode[S6E3HAB_DISPLAY_MODE_1080x2400_112HS], .nframe_duration = 4, },
		[S6E3HAB_DISPLAY_MODE_1080x2400_60HS] = { .mode = &s6e3hab_hubble_rev03_display_mode[S6E3HAB_DISPLAY_MODE_1080x2400_120HS_AID_4_CYCLE], .nframe_duration = 1, },
	},
	[S6E3HAB_DISPLAY_MODE_1080x2400_120HS_AID_4_CYCLE] = {
		[S6E3HAB_DISPLAY_MODE_1080x2400_120HS] = { .mode = &s6e3hab_hubble_rev03_display_mode[S6E3HAB_DISPLAY_MODE_1080x2400_120HS], .nframe_duration = 1, },
		[S6E3HAB_DISPLAY_MODE_1080x2400_96HS] = { .mode = &s6e3hab_hubble_rev03_display_mode[S6E3HAB_DISPLAY_MODE_1080x2400_120HS], .nframe_duration = 1, },
		[S6E3HAB_DISPLAY_MODE_1080x2400_60HS] = { .mode = &s6e3hab_hubble_rev03_display_mode[S6E3HAB_DISPLAY_MODE_1080x2400_110HS], .nframe_duration = 8, },
	},
	[S6E3HAB_DISPLAY_MODE_1080x2400_112HS] = {
		[S6E3HAB_DISPLAY_MODE_1080x2400_120HS] = { .mode = &s6e3hab_hubble_rev03_display_mode[S6E3HAB_DISPLAY_MODE_1080x2400_120HS_AID_4_CYCLE], .nframe_duration = 1, },
		[S6E3HAB_DISPLAY_MODE_1080x2400_96HS] = { .mode = &s6e3hab_hubble_rev03_display_mode[S6E3HAB_DISPLAY_MODE_1080x2400_104HS], .nframe_duration = 4, },
		[S6E3HAB_DISPLAY_MODE_1080x2400_60HS] = { .mode = &s6e3hab_hubble_rev03_display_mode[S6E3HAB_DISPLAY_MODE_1080x2400_120HS_AID_4_CYCLE], .nframe_duration = 4, },
	},
	[S6E3HAB_DISPLAY_MODE_1080x2400_110HS] = {
		[S6E3HAB_DISPLAY_MODE_1080x2400_120HS] = { .mode = &s6e3hab_hubble_rev03_display_mode[S6E3HAB_DISPLAY_MODE_1080x2400_120HS_AID_4_CYCLE], .nframe_duration = 1, },
		[S6E3HAB_DISPLAY_MODE_1080x2400_96HS] = { .mode = &s6e3hab_hubble_rev03_display_mode[S6E3HAB_DISPLAY_MODE_1080x2400_120HS_AID_4_CYCLE], .nframe_duration = 1, },
		[S6E3HAB_DISPLAY_MODE_1080x2400_60HS] = { .mode = &s6e3hab_hubble_rev03_display_mode[S6E3HAB_DISPLAY_MODE_1080x2400_100HS], .nframe_duration = 8, },
	},
	[S6E3HAB_DISPLAY_MODE_1080x2400_104HS] = {
		[S6E3HAB_DISPLAY_MODE_1080x2400_120HS] = { .mode = &s6e3hab_hubble_rev03_display_mode[S6E3HAB_DISPLAY_MODE_1080x2400_112HS], .nframe_duration = 4, },
		[S6E3HAB_DISPLAY_MODE_1080x2400_96HS] = { .mode = &s6e3hab_hubble_rev03_display_mode[S6E3HAB_DISPLAY_MODE_1080x2400_96HS], .nframe_duration = 1, },
		[S6E3HAB_DISPLAY_MODE_1080x2400_60HS] = { .mode = &s6e3hab_hubble_rev03_display_mode[S6E3HAB_DISPLAY_MODE_1080x2400_112HS], .nframe_duration = 4, },
	},
	[S6E3HAB_DISPLAY_MODE_1080x2400_100HS] = {
		[S6E3HAB_DISPLAY_MODE_1080x2400_120HS] = { .mode = &s6e3hab_hubble_rev03_display_mode[S6E3HAB_DISPLAY_MODE_1080x2400_110HS], .nframe_duration = 5, },
		[S6E3HAB_DISPLAY_MODE_1080x2400_96HS] = { .mode = &s6e3hab_hubble_rev03_display_mode[S6E3HAB_DISPLAY_MODE_1080x2400_110HS], .nframe_duration = 5, },
		[S6E3HAB_DISPLAY_MODE_1080x2400_60HS] = { .mode = &s6e3hab_hubble_rev03_display_mode[S6E3HAB_DISPLAY_MODE_1080x2400_70HS], .nframe_duration = 8, },
	},
	[S6E3HAB_DISPLAY_MODE_1080x2400_96HS] = {
		[S6E3HAB_DISPLAY_MODE_1080x2400_120HS] = { .mode = &s6e3hab_hubble_rev03_display_mode[S6E3HAB_DISPLAY_MODE_1080x2400_104HS], .nframe_duration = 4, },
		[S6E3HAB_DISPLAY_MODE_1080x2400_60HS] = { .mode = &s6e3hab_hubble_rev03_display_mode[S6E3HAB_DISPLAY_MODE_1080x2400_104HS], .nframe_duration = 4, },
	},
	[S6E3HAB_DISPLAY_MODE_1080x2400_70HS] = {
		[S6E3HAB_DISPLAY_MODE_1080x2400_120HS] = { .mode = &s6e3hab_hubble_rev03_display_mode[S6E3HAB_DISPLAY_MODE_1080x2400_100HS], .nframe_duration = 5, },
		[S6E3HAB_DISPLAY_MODE_1080x2400_96HS] = { .mode = &s6e3hab_hubble_rev03_display_mode[S6E3HAB_DISPLAY_MODE_1080x2400_100HS], .nframe_duration = 5, },
		[S6E3HAB_DISPLAY_MODE_1080x2400_60HS] = { .mode = &s6e3hab_hubble_rev03_display_mode[S6E3HAB_DISPLAY_MODE_1080x2400_60HS], .nframe_duration = 1, },
	},
	[S6E3HAB_DISPLAY_MODE_1080x2400_60HS] = {
		[S6E3HAB_DISPLAY_MODE_1080x2400_120HS] = { .mode = &s6e3hab_hubble_rev03_display_mode[S6E3HAB_DISPLAY_MODE_1080x2400_70HS], .nframe_duration = 5, },
		[S6E3HAB_DISPLAY_MODE_1080x2400_96HS] = { .mode = &s6e3hab_hubble_rev03_display_mode[S6E3HAB_DISPLAY_MODE_1080x2400_70HS], .nframe_duration = 5, },
	},
	[S6E3HAB_DISPLAY_MODE_1080x2400_60NS] = {
		[S6E3HAB_DISPLAY_MODE_1080x2400_48NS] = { .mode = &s6e3hab_hubble_rev03_display_mode[S6E3HAB_DISPLAY_MODE_1080x2400_52NS], .nframe_duration = 4, },
	},
	[S6E3HAB_DISPLAY_MODE_1080x2400_52NS] = {
		[S6E3HAB_DISPLAY_MODE_1080x2400_60NS] = { .mode = &s6e3hab_hubble_rev03_display_mode[S6E3HAB_DISPLAY_MODE_1080x2400_60NS], .nframe_duration = 1, },
		[S6E3HAB_DISPLAY_MODE_1080x2400_48NS] = { .mode = &s6e3hab_hubble_rev03_display_mode[S6E3HAB_DISPLAY_MODE_1080x2400_48NS], .nframe_duration = 1, },
	},
	[S6E3HAB_DISPLAY_MODE_1080x2400_48NS] = {
		[S6E3HAB_DISPLAY_MODE_1080x2400_60NS] = { .mode = &s6e3hab_hubble_rev03_display_mode[S6E3HAB_DISPLAY_MODE_1080x2400_52NS], .nframe_duration = 4, },
	},

	/* HD */
	[S6E3HAB_DISPLAY_MODE_720x1600_120HS] = {
		[S6E3HAB_DISPLAY_MODE_720x1600_96HS] = { .mode = &s6e3hab_hubble_rev03_display_mode[S6E3HAB_DISPLAY_MODE_720x1600_112HS], .nframe_duration = 4, },
		[S6E3HAB_DISPLAY_MODE_720x1600_60HS] = { .mode = &s6e3hab_hubble_rev03_display_mode[S6E3HAB_DISPLAY_MODE_720x1600_120HS_AID_4_CYCLE], .nframe_duration = 1, },
	},
	[S6E3HAB_DISPLAY_MODE_720x1600_120HS_AID_4_CYCLE] = {
		[S6E3HAB_DISPLAY_MODE_720x1600_120HS] = { .mode = &s6e3hab_hubble_rev03_display_mode[S6E3HAB_DISPLAY_MODE_720x1600_120HS], .nframe_duration = 1, },
		[S6E3HAB_DISPLAY_MODE_720x1600_96HS] = { .mode = &s6e3hab_hubble_rev03_display_mode[S6E3HAB_DISPLAY_MODE_720x1600_120HS], .nframe_duration = 1, },
		[S6E3HAB_DISPLAY_MODE_720x1600_60HS] = { .mode = &s6e3hab_hubble_rev03_display_mode[S6E3HAB_DISPLAY_MODE_720x1600_110HS], .nframe_duration = 8, },
	},
	[S6E3HAB_DISPLAY_MODE_720x1600_112HS] = {
		[S6E3HAB_DISPLAY_MODE_720x1600_120HS] = { .mode = &s6e3hab_hubble_rev03_display_mode[S6E3HAB_DISPLAY_MODE_720x1600_120HS_AID_4_CYCLE], .nframe_duration = 1, },
		[S6E3HAB_DISPLAY_MODE_720x1600_96HS] = { .mode = &s6e3hab_hubble_rev03_display_mode[S6E3HAB_DISPLAY_MODE_720x1600_104HS], .nframe_duration = 4, },
		[S6E3HAB_DISPLAY_MODE_720x1600_60HS] = { .mode = &s6e3hab_hubble_rev03_display_mode[S6E3HAB_DISPLAY_MODE_720x1600_120HS_AID_4_CYCLE], .nframe_duration = 4, },
	},
	[S6E3HAB_DISPLAY_MODE_720x1600_110HS] = {
		[S6E3HAB_DISPLAY_MODE_720x1600_120HS] = { .mode = &s6e3hab_hubble_rev03_display_mode[S6E3HAB_DISPLAY_MODE_720x1600_120HS_AID_4_CYCLE], .nframe_duration = 1, },
		[S6E3HAB_DISPLAY_MODE_720x1600_96HS] = { .mode = &s6e3hab_hubble_rev03_display_mode[S6E3HAB_DISPLAY_MODE_720x1600_120HS_AID_4_CYCLE], .nframe_duration = 1, },
		[S6E3HAB_DISPLAY_MODE_720x1600_60HS] = { .mode = &s6e3hab_hubble_rev03_display_mode[S6E3HAB_DISPLAY_MODE_720x1600_100HS], .nframe_duration = 8, },
	},
	[S6E3HAB_DISPLAY_MODE_720x1600_104HS] = {
		[S6E3HAB_DISPLAY_MODE_720x1600_120HS] = { .mode = &s6e3hab_hubble_rev03_display_mode[S6E3HAB_DISPLAY_MODE_720x1600_112HS], .nframe_duration = 4, },
		[S6E3HAB_DISPLAY_MODE_720x1600_96HS] = { .mode = &s6e3hab_hubble_rev03_display_mode[S6E3HAB_DISPLAY_MODE_720x1600_96HS], .nframe_duration = 1, },
		[S6E3HAB_DISPLAY_MODE_720x1600_60HS] = { .mode = &s6e3hab_hubble_rev03_display_mode[S6E3HAB_DISPLAY_MODE_720x1600_112HS], .nframe_duration = 4, },
	},
	[S6E3HAB_DISPLAY_MODE_720x1600_100HS] = {
		[S6E3HAB_DISPLAY_MODE_720x1600_120HS] = { .mode = &s6e3hab_hubble_rev03_display_mode[S6E3HAB_DISPLAY_MODE_720x1600_110HS], .nframe_duration = 5, },
		[S6E3HAB_DISPLAY_MODE_720x1600_96HS] = { .mode = &s6e3hab_hubble_rev03_display_mode[S6E3HAB_DISPLAY_MODE_720x1600_110HS], .nframe_duration = 5, },
		[S6E3HAB_DISPLAY_MODE_720x1600_60HS] = { .mode = &s6e3hab_hubble_rev03_display_mode[S6E3HAB_DISPLAY_MODE_720x1600_70HS], .nframe_duration = 8, },
	},
	[S6E3HAB_DISPLAY_MODE_720x1600_96HS] = {
		[S6E3HAB_DISPLAY_MODE_720x1600_120HS] = { .mode = &s6e3hab_hubble_rev03_display_mode[S6E3HAB_DISPLAY_MODE_720x1600_104HS], .nframe_duration = 4, },
		[S6E3HAB_DISPLAY_MODE_720x1600_60HS] = { .mode = &s6e3hab_hubble_rev03_display_mode[S6E3HAB_DISPLAY_MODE_720x1600_104HS], .nframe_duration = 4, },
	},
	[S6E3HAB_DISPLAY_MODE_720x1600_70HS] = {
		[S6E3HAB_DISPLAY_MODE_720x1600_120HS] = { .mode = &s6e3hab_hubble_rev03_display_mode[S6E3HAB_DISPLAY_MODE_720x1600_100HS], .nframe_duration = 5, },
		[S6E3HAB_DISPLAY_MODE_720x1600_96HS] = { .mode = &s6e3hab_hubble_rev03_display_mode[S6E3HAB_DISPLAY_MODE_720x1600_100HS], .nframe_duration = 5, },
		[S6E3HAB_DISPLAY_MODE_720x1600_60HS] = { .mode = &s6e3hab_hubble_rev03_display_mode[S6E3HAB_DISPLAY_MODE_720x1600_60HS], .nframe_duration = 1, },
	},
	[S6E3HAB_DISPLAY_MODE_720x1600_60HS] = {
		[S6E3HAB_DISPLAY_MODE_720x1600_120HS] = { .mode = &s6e3hab_hubble_rev03_display_mode[S6E3HAB_DISPLAY_MODE_720x1600_70HS], .nframe_duration = 5, },
		[S6E3HAB_DISPLAY_MODE_720x1600_96HS] = { .mode = &s6e3hab_hubble_rev03_display_mode[S6E3HAB_DISPLAY_MODE_720x1600_70HS], .nframe_duration = 5, },
	},
	[S6E3HAB_DISPLAY_MODE_720x1600_60NS] = {
		[S6E3HAB_DISPLAY_MODE_720x1600_48NS] = { .mode = &s6e3hab_hubble_rev03_display_mode[S6E3HAB_DISPLAY_MODE_720x1600_52NS], .nframe_duration = 4, },
	},
	[S6E3HAB_DISPLAY_MODE_720x1600_52NS] = {
		[S6E3HAB_DISPLAY_MODE_720x1600_60NS] = { .mode = &s6e3hab_hubble_rev03_display_mode[S6E3HAB_DISPLAY_MODE_720x1600_60NS], .nframe_duration = 1, },
		[S6E3HAB_DISPLAY_MODE_720x1600_48NS] = { .mode = &s6e3hab_hubble_rev03_display_mode[S6E3HAB_DISPLAY_MODE_720x1600_48NS], .nframe_duration = 1, },
	},
	[S6E3HAB_DISPLAY_MODE_720x1600_48NS] = {
		[S6E3HAB_DISPLAY_MODE_720x1600_60NS] = { .mode = &s6e3hab_hubble_rev03_display_mode[S6E3HAB_DISPLAY_MODE_720x1600_52NS], .nframe_duration = 4, },
	},
};

struct common_panel_display_mode_bridge_ops s6e3hab_hubble_display_mode_bridge_ops = {
	.check_changeable = s6e3hab_hubble_bridge_refresh_rate_changeable,
	.check_jumpmode = NULL,
};
#endif /* CONFIG_PANEL_VRR_BRIDGE */

static struct common_panel_display_modes s6e3hab_hubble_preliminary_display_modes = {
	.num_modes = ARRAY_SIZE(s6e3hab_hubble_preliminary_display_mode_array),
	.modes = (struct common_panel_display_mode **)&s6e3hab_hubble_preliminary_display_mode_array,
#ifdef CONFIG_PANEL_VRR_BRIDGE
	.bridges = NULL,
	.bridge_ops = NULL,
#endif
};

static struct common_panel_display_modes s6e3hab_hubble_display_modes = {
	.num_modes = ARRAY_SIZE(s6e3hab_hubble_display_mode_array),
	.modes = (struct common_panel_display_mode **)&s6e3hab_hubble_display_mode_array,
#ifdef CONFIG_PANEL_VRR_BRIDGE
	.bridges = (struct common_panel_display_mode_bridge *)s6e3hab_hubble_display_mode_bridge,
	.bridge_ops = &s6e3hab_hubble_display_mode_bridge_ops,
#endif
};

static struct common_panel_display_modes s6e3hab_hubble_rev03_display_modes = {
	.num_modes = ARRAY_SIZE(s6e3hab_hubble_rev03_display_mode_array),
	.modes = (struct common_panel_display_mode **)&s6e3hab_hubble_rev03_display_mode_array,
#ifdef CONFIG_PANEL_VRR_BRIDGE
	.bridges = (struct common_panel_display_mode_bridge *)s6e3hab_hubble_rev03_display_mode_bridge,
	.bridge_ops = &s6e3hab_hubble_display_mode_bridge_ops,
#endif
};
#endif /* CONFIG_PANEL_DISPLAY_MODE */
#endif /* __S6E3HAB_HUBBLE_RESOL_H__ */
