/*
 * Samsung Exynos SoC series Pablo driver
 *
 * Copyright (c) 2019 Samsung Electronics Co., Ltd
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#ifndef IS_HW_CAC_H
#define IS_HW_CAC_H

#ifdef CAC_G2_VERSION
#define CAC_MAX_NI_DEPENDED_CFG		(12)
#define CAC_MAP_COEF_SIZE		(3)
#define CAC_MAP_BLEND_TBL_SIZE		(9)
#define CAC_CRT_HUE_TBL_SIZE		(12)
struct cac_map_thr_cfg {
	u32 map_right_shift;
	u32 map_normal_gain;
	u32 map_line_thr;
	int map_coef_y0[CAC_MAP_COEF_SIZE];
	int map_coef_y1[CAC_MAP_COEF_SIZE];
	int map_coef_y2[CAC_MAP_COEF_SIZE];
	u32 map_blend_tbl[CAC_MAP_BLEND_TBL_SIZE];
};

struct cac_crt_thr_cfg {
	u32 crt_hue_tbl[CAC_CRT_HUE_TBL_SIZE];
};
#else
#define CAC_MAX_NI_DEPENDED_CFG     (9)

struct cac_map_thr_cfg {
	u32	map_spot_thr_l;
	u32	map_spot_thr_h;
	u32	map_spot_thr;
	u32	map_spot_nr_strength;
};

struct cac_crt_thr_cfg {
	u32	crt_color_thr_l_dot;
	u32	crt_color_thr_l_line;
	u32	crt_color_thr_h;
};
#endif
struct cac_cfg_by_ni {
	struct cac_map_thr_cfg	map_thr_cfg;
	struct cac_crt_thr_cfg	crt_thr_cfg;
};

struct cac_setfile_contents {
	bool	cac_en;
	u32	ni_max;
	u32	ni_vals[CAC_MAX_NI_DEPENDED_CFG];
	struct cac_cfg_by_ni	cfgs[CAC_MAX_NI_DEPENDED_CFG];
};
#endif
