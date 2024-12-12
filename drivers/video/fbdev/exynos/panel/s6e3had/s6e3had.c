/*
 * linux/drivers/video/fbdev/exynos/panel/s6e3had/s6e3had.c
 *
 * S6E3HAD Dimming Driver
 *
 * Copyright (c) 2016 Samsung Electronics
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#include <linux/of_gpio.h>
#include <video/mipi_display.h>
#include "../panel.h"
#include "s6e3had.h"
#include "s6e3had_panel.h"
#ifdef CONFIG_PANEL_AID_DIMMING
#include "../dimming.h"
#include "../panel_dimming.h"
#endif
#include "../panel_drv.h"
#include "../panel_debug.h"
#include "../dpui.h"

#if IS_ENABLED(CONFIG_SEC_ABC)
#include <linux/sti/abc_common.h>
#endif

#ifdef PANEL_PR_TAG
#undef PANEL_PR_TAG
#define PANEL_PR_TAG	"ddi"
#endif

#ifdef CONFIG_DYNAMIC_MIPI
static void dynamic_mipi_set_ffc(struct maptbl *tbl, u8 *dst)
{
	int i;
	struct panel_device *panel;
	struct dm_status_info *dm_status;
	struct dm_hs_info *hs_info;

	if (unlikely(tbl == NULL || dst == NULL)) {
		panel_err("maptbl is null\n");
		return;
	}

	if (unlikely(tbl->pdata == NULL)) {
		panel_err("pdata is null\n");
		return;
	}
	panel = tbl->pdata;

	dm_status = &panel->dynamic_mipi.dm_status;
	hs_info = &panel->dynamic_mipi.dm_dt.dm_hs_list[dm_status->request_df];

	panel_info("MCD:DM: ffc-> cur: %d, req: %d, osc -> cur: %d, req: %d\n",
		dm_status->ffc_df, dm_status->request_df, dm_status->current_ddi_osc, dm_status->request_ddi_osc);

	if (hs_info->ffc_cmd_sz != tbl->sz_copy) {
		panel_err("MCD:DM:Wrong ffc command size, dt: %d, header: %d\n",
			hs_info->ffc_cmd_sz, tbl->sz_copy);
		goto exit_copy;
	}

	if (dst[0] != hs_info->ffc_cmd[dm_status->current_ddi_osc][0]) {
		panel_err("MCD:DM:Wrong ffc command:dt:%x, header:%x\n",
			hs_info->ffc_cmd[dm_status->current_ddi_osc][0], dst[0]);
		goto exit_copy;
	}

	for (i = 0; i < hs_info->ffc_cmd_sz; i++)
		dst[i] = hs_info->ffc_cmd[dm_status->current_ddi_osc][i];

	dm_status->ffc_df = dm_status->request_df;

exit_copy:
	for (i = 0; i < hs_info->ffc_cmd_sz; i++)
		panel_dbg("FFC[%d]: %x\n", i, dst[i]);

}

#endif

static int get_s6e3had_unbound3_id(struct panel_device *panel)
{
	u8 id;

	id = panel->panel_data.id[2] & 0xFF;
	if (id < 0xE7)
		return S6E3HAD_UNBOUND3_ID_E5;
	if (id == 0xE7)
		return S6E3HAD_UNBOUND3_ID_E7;
	return S6E3HAD_UNBOUND3_ID_E8;
}

static int get_s6e3had_96hs_mode(int vrr)
{
	int ret;

	switch (vrr) {
	case S6E3HAD_VRR_96HS:
	case S6E3HAD_VRR_48HS_96HS_TE_HW_SKIP_1:
	case S6E3HAD_VRR_48HS_96HS_TE_SW_SKIP_1:
		ret = S6E3HAD_96HS_MODE_ON;
		break;
	case S6E3HAD_VRR_60NS:
	case S6E3HAD_VRR_48NS:
	case S6E3HAD_VRR_120HS:
	case S6E3HAD_VRR_60HS_120HS_TE_HW_SKIP_1:
	case S6E3HAD_VRR_60HS_120HS_TE_SW_SKIP_1:
		ret = S6E3HAD_96HS_MODE_OFF;
		break;
	default:
		//error
		panel_err("got invalid idx %d\n", vrr);
		ret = S6E3HAD_96HS_MODE_OFF;
		break;
	}

	return ret;
}

static int get_s6e3had_brt_direction(struct panel_device *panel)
{
	struct panel_bl_device *panel_bl = &panel->panel_bl;
	bool prev, curr;

	prev = is_hbm_brightness(panel_bl, panel_bl->props.prev_brightness);
	curr = is_hbm_brightness(panel_bl, panel_bl->props.brightness);

	if (!prev && curr)
		return 1;
	else if (prev && !curr)
		return -1;

	return 0;
}

static int find_s6e3had_vrr(struct panel_vrr *vrr)
{
	int i;

	if (!vrr) {
		panel_err("panel_vrr is null\n");
		return -EINVAL;
	}

	for (i = 0; i < ARRAY_SIZE(S6E3HAD_VRR_FPS); i++)
		if (vrr->fps == S6E3HAD_VRR_FPS[i][S6E3HAD_VRR_KEY_REFRESH_RATE] &&
			vrr->mode == S6E3HAD_VRR_FPS[i][S6E3HAD_VRR_KEY_REFRESH_MODE] &&
			vrr->te_sw_skip_count == S6E3HAD_VRR_FPS[i][S6E3HAD_VRR_KEY_TE_SW_SKIP_COUNT] &&
			vrr->te_hw_skip_count == S6E3HAD_VRR_FPS[i][S6E3HAD_VRR_KEY_TE_HW_SKIP_COUNT])
			return i;


	return -EINVAL;
}

static int getidx_s6e3had_lfd_frame_idx(int fps, int mode)
{
	int i = 0, start = 0, end = 0;

	if (mode == VRR_HS_MODE) {
		start = S6E3HAD_VRR_LFD_FRAME_IDX_HS_BEGIN;
		end = S6E3HAD_VRR_LFD_FRAME_IDX_HS_END;
	} else if (mode == VRR_NORMAL_MODE) {
		start = S6E3HAD_VRR_LFD_FRAME_IDX_NS_BEGIN;
		end = S6E3HAD_VRR_LFD_FRAME_IDX_NS_END;
	}
	for (i = start; i <= end; i++)
		if (fps >= S6E3HAD_VRR_LFD_FRAME_IDX_VAL[i])
			return i;

	return -EINVAL;
}

static int get_s6e3had_lfd_min_freq(u32 scalability, u32 vrr_fps_index)
{
	if (scalability > S6E3HAD_VRR_LFD_SCALABILITY_MAX) {
		panel_warn("exceeded scalability (%d)\n", scalability);
		scalability = S6E3HAD_VRR_LFD_SCALABILITY_MAX;
	}
	if (vrr_fps_index >= MAX_S6E3HAD_VRR) {
		panel_err("invalid vrr_fps_index %d\n", vrr_fps_index);
		return -EINVAL;
	}
	return S6E3HAD_VRR_LFD_MIN_FREQ[scalability][vrr_fps_index];
}

static int get_s6e3had_lpm_lfd_min_freq(u32 scalability)
{
	if (scalability > S6E3HAD_VRR_LFD_SCALABILITY_MAX) {
		panel_warn("exceeded scalability (%d)\n", scalability);
		scalability = S6E3HAD_VRR_LFD_SCALABILITY_MAX;
	}
	return S6E3HAD_LPM_LFD_MIN_FREQ[scalability];
}

static int s6e3had_get_vrr_lfd_min_div_count(struct panel_info *panel_data)
{
	struct panel_device *panel = to_panel_device(panel_data);
	struct panel_properties *props = &panel_data->props;
	struct vrr_lfd_config *vrr_lfd_config;
	struct panel_vrr *vrr;
	int index = 0, ret;
	int vrr_fps, vrr_mode;
	u32 lfd_min_freq = 0;
	u32 lfd_min_div_count = 0;
	u32 vrr_div_count;

	vrr = get_panel_vrr(panel);
	if (vrr == NULL)
		return -EINVAL;

	vrr_fps = vrr->fps;
	vrr_mode = vrr->mode;
	vrr_div_count = TE_SKIP_TO_DIV(vrr->te_sw_skip_count, vrr->te_hw_skip_count);
	index = find_s6e3had_vrr(vrr);
	if (index < 0) {
		panel_err("vrr(%d %d) not found\n",
				vrr_fps, vrr_mode);
		return -EINVAL;
	}

	vrr_lfd_config = &props->vrr_lfd_info.cur[VRR_LFD_SCOPE_NORMAL];
	ret = get_s6e3had_lfd_min_freq(vrr_lfd_config->scalability, index);
	if (ret < 0) {
		panel_err("failed to get lfd_min_freq(%d)\n", ret);
		return -EINVAL;
	}
	lfd_min_freq = ret;

	if (vrr_lfd_config->fix == VRR_LFD_FREQ_HIGH) {
		lfd_min_div_count = vrr_div_count;
	} else if (vrr_lfd_config->fix == VRR_LFD_FREQ_LOW ||
			vrr_lfd_config->min == 0 ||
			vrr_div_count == 0) {
		lfd_min_div_count = disp_div_round(vrr_fps, lfd_min_freq);
	} else {
		lfd_min_freq = max(lfd_min_freq,
				min(disp_div_round(vrr_fps, vrr_div_count), vrr_lfd_config->min));
		lfd_min_div_count = (lfd_min_freq == 0) ?
			MIN_S6E3HAD_FPS_DIV_COUNT : disp_div_round(vrr_fps, lfd_min_freq);
	}

	panel_dbg("vrr(%d %d), div(%d), lfd(fix:%d scale:%d min:%d max:%d), div_count(%d)\n",
			vrr_fps, vrr_mode, vrr_div_count,
			vrr_lfd_config->fix, vrr_lfd_config->scalability,
			vrr_lfd_config->min, vrr_lfd_config->max, lfd_min_div_count);

	return lfd_min_div_count;
}

static int s6e3had_get_vrr_lfd_max_div_count(struct panel_info *panel_data)
{
	struct panel_device *panel = to_panel_device(panel_data);
	struct panel_properties *props = &panel_data->props;
	struct vrr_lfd_config *vrr_lfd_config;
	struct panel_vrr *vrr;
	int index = 0, ret;
	int vrr_fps, vrr_mode;
	u32 lfd_min_freq = 0;
	u32 lfd_max_freq = 0;
	u32 lfd_max_div_count = 0;
	u32 vrr_div_count;

	vrr = get_panel_vrr(panel);
	if (vrr == NULL)
		return -EINVAL;

	vrr_fps = vrr->fps;
	vrr_mode = vrr->mode;
	vrr_div_count = TE_SKIP_TO_DIV(vrr->te_sw_skip_count, vrr->te_hw_skip_count);
	index = find_s6e3had_vrr(vrr);
	if (index < 0) {
		panel_err("vrr(%d %d) not found\n",
				vrr_fps, vrr_mode);
		return -EINVAL;
	}

	vrr_lfd_config = &props->vrr_lfd_info.cur[VRR_LFD_SCOPE_NORMAL];
	ret = get_s6e3had_lfd_min_freq(vrr_lfd_config->scalability, index);
	if (ret < 0) {
		panel_err("failed to get lfd_min_freq(%d)\n", ret);
		return -EINVAL;
	}

	lfd_min_freq = ret;
	if (vrr_lfd_config->fix == VRR_LFD_FREQ_LOW) {
		lfd_max_div_count = disp_div_round((u32)vrr_fps, lfd_min_freq);
	} else if (vrr_lfd_config->fix == VRR_LFD_FREQ_HIGH ||
		vrr_lfd_config->max == 0 ||
		vrr_div_count == 0) {
		lfd_max_div_count = vrr_div_count;
	} else {
		lfd_max_freq = max(lfd_min_freq,
				min(disp_div_round(vrr_fps, vrr_div_count), vrr_lfd_config->max));
		lfd_max_div_count = (lfd_max_freq == 0) ?
			MIN_S6E3HAD_FPS_DIV_COUNT : disp_div_round(vrr_fps, lfd_max_freq);
	}

	panel_dbg("vrr(%d %d), div(%d), lfd(fix:%d scale:%d min:%d max:%d), div_count(%d)\n",
			vrr_fps, vrr_mode, vrr_div_count,
			vrr_lfd_config->fix, vrr_lfd_config->scalability,
			vrr_lfd_config->min, vrr_lfd_config->max,
			lfd_max_div_count);

	return lfd_max_div_count;
}

static int s6e3had_get_vrr_lfd_min_freq(struct panel_info *panel_data)
{
	struct panel_device *panel = to_panel_device(panel_data);
	int vrr_fps, div_count;

	vrr_fps = get_panel_refresh_rate(panel);
	if (vrr_fps < 0)
		return -EINVAL;

	div_count = s6e3had_get_vrr_lfd_min_div_count(panel_data);
	if (div_count <= 0)
		return -EINVAL;

	return (int)disp_div_round(vrr_fps, div_count);
}

static int s6e3had_get_vrr_lfd_max_freq(struct panel_info *panel_data)
{
	struct panel_device *panel = to_panel_device(panel_data);
	int vrr_fps, div_count;

	vrr_fps = get_panel_refresh_rate(panel);
	if (vrr_fps < 0)
		return -EINVAL;

	div_count = s6e3had_get_vrr_lfd_max_div_count(panel_data);
	if (div_count <= 0)
		return -EINVAL;

	return (int)disp_div_round(vrr_fps, div_count);
}

static int getidx_s6e3had_vrr_mode(int mode)
{
	return mode == VRR_HS_MODE ?
		S6E3HAD_VRR_MODE_HS : S6E3HAD_VRR_MODE_NS;
}

static int generate_brt_step_table(struct brightness_table *brt_tbl)
{
	int ret = 0;
	int i = 0, j = 0, k = 0;

	if (unlikely(!brt_tbl || !brt_tbl->brt)) {
		panel_err("invalid parameter\n");
		return -EINVAL;
	}
	if (unlikely(!brt_tbl->step_cnt)) {
		if (likely(brt_tbl->brt_to_step)) {
			panel_info("we use static step table\n");
			return ret;
		} else {
			panel_err("invalid parameter, all table is NULL\n");
			return -EINVAL;
		}
	}

	brt_tbl->sz_brt_to_step = 0;
	for (i = 0; i < brt_tbl->sz_step_cnt; i++)
		brt_tbl->sz_brt_to_step += brt_tbl->step_cnt[i];

	brt_tbl->brt_to_step =
		(u32 *)kmalloc(brt_tbl->sz_brt_to_step * sizeof(u32), GFP_KERNEL);

	if (unlikely(!brt_tbl->brt_to_step)) {
		panel_err("alloc fail\n");
		return -EINVAL;
	}
	brt_tbl->brt_to_step[0] = brt_tbl->brt[0];
	i = 1;
	while (i < brt_tbl->sz_brt_to_step) {
		for (k = 1; k < brt_tbl->sz_brt; k++) {
			for (j = 1; j <= brt_tbl->step_cnt[k]; j++, i++) {
				brt_tbl->brt_to_step[i] = interpolation(brt_tbl->brt[k - 1] * disp_pow(10, 2),
					brt_tbl->brt[k] * disp_pow(10, 2), j, brt_tbl->step_cnt[k]);
				brt_tbl->brt_to_step[i] = disp_pow_round(brt_tbl->brt_to_step[i], 2);
				brt_tbl->brt_to_step[i] = disp_div64(brt_tbl->brt_to_step[i], disp_pow(10, 2));
				if (brt_tbl->brt[brt_tbl->sz_brt - 1] < brt_tbl->brt_to_step[i])
					brt_tbl->brt_to_step[i] = disp_pow_round(brt_tbl->brt_to_step[i], 2);
				if (i >= brt_tbl->sz_brt_to_step) {
					panel_err("step cnt over %d %d\n", i, brt_tbl->sz_brt_to_step);
					break;
				}
			}
		}
	}
	return ret;
}

int init_common_table(struct maptbl *tbl)
{
	if (tbl == NULL) {
		panel_err("maptbl is null\n");
		return -EINVAL;
	}

	if (tbl->pdata == NULL) {
		panel_err("pdata is null\n");
		return -EINVAL;
	}

	return 0;
}

#ifdef CONFIG_EXYNOS_DECON_MDNIE_LITE
static int getidx_common_maptbl(struct maptbl *tbl)
{
	return 0;
}
#endif



static int gamma_ctoi_70byte(s32 (*dst)[MAX_COLOR], u8 *src)
{
	static u32 TP_SIZE[S6E3HAD_NR_TP] = {
		0xFFF, 0x7FF, 0x7FF, 0x7FF, 0x7FF, 0x7FF, 0x7FF, 0x7FF, 0x7FF, 0x7FF, 0x7FF, 0x7FF, 0x7FF, 0xFFF
	};
	unsigned int i;

	for (i = 0; i < S6E3HAD_NR_TP; i++) {
		dst[i][RED] = (((src[i * 5 + 0] & 0xF) << 8)  | src[i * 5 + 2]) & TP_SIZE[i];
		dst[i][GREEN] = (((src[i * 5 + 1] & 0xF0) << 4) | src[i * 5 + 3]) & TP_SIZE[i];
		dst[i][BLUE] = (((src[i * 5 + 1] & 0xF) << 8) | src[i * 5 + 4]) & TP_SIZE[i];
	}

	return 0;
}

static int gamma_ctoi_84byte(s32 (*dst)[MAX_COLOR], u8 *src)
{
	static u32 TP_SIZE[S6E3HAD_NR_TP] = {
		0xFFF, 0x7FF, 0x7FF, 0x7FF, 0x7FF, 0x7FF, 0x7FF, 0x7FF, 0x7FF, 0x7FF, 0x7FF, 0x7FF, 0x7FF, 0xFFF
	};
	unsigned int i, c;

	for (i = 0; i < S6E3HAD_NR_TP; i++) {
		for_each_color(c) {
			dst[i][c] = src[(i * (MAX_COLOR * 2)) + (c * 2) + 0] & (TP_SIZE[i] >> 8);
			dst[i][c] = dst[i][c] << 8;
			dst[i][c] = dst[i][c] | (src[(i * (MAX_COLOR * 2)) + (c * 2) + 1] & (TP_SIZE[i] & 0xFF));
		}
	}

	return 0;
}

static int gamma_sum(s32 (*dst)[MAX_COLOR], s32 (*src)[MAX_COLOR],
		s32 (*offset)[MAX_COLOR])
{
	unsigned int i, c;
	int upper_limit[S6E3HAD_NR_TP] = {
		0xFFF, 0x7FF, 0x7FF, 0x7FF, 0x7FF, 0x7FF, 0x7FF, 0x7FF, 0x7FF, 0x7FF, 0x7FF, 0x7FF, 0x7FF, 0xFFF
	};

	for (i = 0; i < S6E3HAD_NR_TP; i++) {
		for_each_color(c)
			dst[i][c] =
			min(max(src[i][c] + offset[i][c], 0), upper_limit[i]);
	}

	return 0;
}

static void gamma_itoc(u8 *dst, s32(*src)[MAX_COLOR])
{
	int v;
	static u32 TP_SIZE[S6E3HAD_NR_TP] = {
		0xFFF, 0x7FF, 0x7FF, 0x7FF, 0x7FF, 0x7FF, 0x7FF, 0x7FF, 0x7FF, 0x7FF, 0x7FF, 0x7FF, 0x7FF, 0xFFF
	};

	for (v = 0; v < S6E3HAD_NR_TP; v++) {
		dst[v * 5 + 0] = (src[v][RED] >> 8) & (TP_SIZE[v] >> 8);
		dst[v * 5 + 1] = ((src[v][GREEN] >> 8) & (TP_SIZE[v] >> 8)) << 4;
		dst[v * 5 + 1] = dst[v * 5 + 1] | ((src[v][BLUE] >> 8) & (TP_SIZE[v] >> 8));
		dst[v * 5 + 2] = src[v][RED] & (TP_SIZE[v] & 0xFF);
		dst[v * 5 + 3] = src[v][GREEN] & (TP_SIZE[v] & 0xFF);
		dst[v * 5 + 4] = src[v][BLUE] & (TP_SIZE[v] & 0xFF);
	}
}

static void dump_s32_arr(char *buf, u32 buflen, s32 *arr, u32 len)
{
	int size = 0, i;

	for (i = 0; i < len; i++)
		size += snprintf(buf + size, buflen - size, "%5d", arr[i]);
}

static int init_gamma_write_table(struct maptbl *tbl, int vrr_idx, int offset, int len)
{
	struct panel_device *panel;
	struct panel_info *panel_data;
	struct panel_dimming_info *panel_dim_info;
	struct gm2_dimming_init_info *gm2_dim_info;
	struct gm2_dimming_lut (*dim_lut_table)[S6E3HAD_TOTAL_STEP];
	struct gm2_dimming_lut *dim_lut;
	int ret = 0, br_idx = 0, maptbl_idx, panel_id_idx, res_size;
	u8 GAMMA_READ[S6E3HAD_GAMMA_LEN] = { 0, };
	s32 GAMMA_COLOR_TABLE[S6E3HAD_NR_TP][MAX_COLOR] = { 0, };
	u8 GAMMA_WRITE[S6E3HAD_GAMMA_WRITE_LEN] = { 0, };
	char debug_str[1024];

	panel_info("++ vrr %d ofs %d len %d\n", vrr_idx, offset, len);

	if (unlikely(!tbl || !tbl->pdata)) {
		panel_err("panel_bl-%d invalid param (tbl %p, pdata %p)\n",
				PANEL_BL_SUBDEV_TYPE_DISP, tbl, tbl ? tbl->pdata : NULL);
		ret = -EINVAL;
		goto exit;
	}

	panel = tbl->pdata;

	panel_data = &panel->panel_data;
	panel_dim_info = panel_data->panel_dim_info[PANEL_BL_SUBDEV_TYPE_DISP];

	if (unlikely(!panel_dim_info)) {
		panel_err("panel_bl-%d panel_dim_info is null\n",
				PANEL_BL_SUBDEV_TYPE_DISP);
		ret = -EINVAL;
		goto exit;
	}

	if (panel_dim_info->nr_gm2_dim_init_info == 0) {
		panel_err("panel_bl-%d gamma mode 2 init info is null\n",
				PANEL_BL_SUBDEV_TYPE_DISP);
		ret = -EINVAL;
		goto exit;
	}

	if (!(vrr_idx < panel_dim_info->nr_gm2_dim_init_info)) {
		panel_err("panel_bl-%d gamma mode 2 init info is invalid. idx: %d cnt %d\n",
				PANEL_BL_SUBDEV_TYPE_DISP, vrr_idx, panel_dim_info->nr_gm2_dim_init_info);
		ret = -EINVAL;
		goto exit;
	}

	gm2_dim_info = &panel_dim_info->gm2_dim_init_info[vrr_idx];
	dim_lut_table = (struct gm2_dimming_lut (*)[S6E3HAD_TOTAL_STEP])gm2_dim_info->dim_lut;
	panel_id_idx = get_s6e3had_unbound3_id(panel);
	panel_info("id idx %d\n", panel_id_idx);

	for (br_idx = 0; br_idx < gm2_dim_info->nr_dim_lut; br_idx++) {
		panel_dbg("br %d\n", br_idx);
		//create color offset tables for brightness ranges
		dim_lut = &dim_lut_table[panel_id_idx][br_idx];
		ret = resource_copy_by_name(panel_data, GAMMA_READ, dim_lut->source_gamma);
		if (unlikely(ret < 0)) {
			panel_err("gamma_read not found in panel resource %s\n",
				dim_lut->source_gamma);
			ret = -EINVAL;
			goto exit;
		}

		res_size = get_resource_size_by_name(panel_data, dim_lut->source_gamma);
		switch (res_size) {
		case 70:
			ret = gamma_ctoi_70byte(GAMMA_COLOR_TABLE, GAMMA_READ);
			break;
		case 84:
			ret = gamma_ctoi_84byte(GAMMA_COLOR_TABLE, GAMMA_READ);
			break;
		default:
			panel_err("invalid gamma length : %d\n", res_size);
			ret = res_size;
			goto exit;
			break;
		}

		if (panel_log_level > 6) {
			panel_dbg("read res %s%s:\n",
				dim_lut->source_gamma, (dim_lut->rgb_color_offset != NULL ? "(with offset)" : ""));

			dump_s32_arr(debug_str, ARRAY_SIZE(debug_str), (s32 *)GAMMA_COLOR_TABLE, S6E3HAD_NR_TP * MAX_COLOR);
			panel_dbg("gamma origin : %s\n", debug_str);
		}

		if (dim_lut->rgb_color_offset != NULL) {
			ret = gamma_sum(GAMMA_COLOR_TABLE, GAMMA_COLOR_TABLE, dim_lut->rgb_color_offset);
			if (panel_log_level > 6) {
				dump_s32_arr(debug_str, ARRAY_SIZE(debug_str), (s32 *)GAMMA_COLOR_TABLE, S6E3HAD_NR_TP * MAX_COLOR);
				panel_info("gamma offset : %s\n", debug_str);
			}
		}

		gamma_itoc(GAMMA_WRITE, GAMMA_COLOR_TABLE);

		if (panel_log_level > 6) {
			print_hex_dump(KERN_INFO, "", DUMP_PREFIX_NONE, 32, 1,
				GAMMA_WRITE, S6E3HAD_GAMMA_WRITE_LEN, false);
		}

		maptbl_idx = maptbl_index(tbl, 0, br_idx, 0);

		memcpy(&tbl->arr[maptbl_idx], &GAMMA_WRITE[offset], len);

	}

exit:
	panel_info("-- rows %d\n", br_idx);
	return ret;
}

static int init_gamma_write_96hs_0_table(struct maptbl *tbl)
{
	return init_gamma_write_table(tbl, S6E3HAD_VRR_96HS, 0, S6E3HAD_GAMMA_WRITE_0_LEN);
}

static int init_gamma_write_96hs_1_table(struct maptbl *tbl)
{
	return init_gamma_write_table(tbl, S6E3HAD_VRR_96HS, S6E3HAD_GAMMA_WRITE_0_LEN, S6E3HAD_GAMMA_WRITE_1_LEN);
}

#if defined(CONFIG_EXYNOS_DOZE) && defined(CONFIG_SUPPORT_AOD_BL)
static int init_aod_dimming_table(struct maptbl *tbl)
{
	int id = PANEL_BL_SUBDEV_TYPE_AOD;
	struct panel_device *panel;
	struct panel_bl_device *panel_bl;

	if (unlikely(!tbl || !tbl->pdata)) {
		panel_err("panel_bl-%d invalid param (tbl %p, pdata %p)\n",
				id, tbl, tbl ? tbl->pdata : NULL);
		return -EINVAL;
	}

	panel = tbl->pdata;
	panel_bl = &panel->panel_bl;

	if (unlikely(!panel->panel_data.panel_dim_info[id])) {
		panel_err("panel_bl-%d panel_dim_info is null\n", id);
		return -EINVAL;
	}

	memcpy(&panel_bl->subdev[id].brt_tbl,
			panel->panel_data.panel_dim_info[id]->brt_tbl,
			sizeof(struct brightness_table));

	return 0;
}
#endif

static int getidx_gm2_elvss_table(struct maptbl *tbl)
{
	int row;
	struct panel_info *panel_data;
	struct panel_bl_device *panel_bl;
	struct panel_device *panel = (struct panel_device *)tbl->pdata;

	if (panel == NULL) {
		panel_err("panel is null\n");
		return -EINVAL;
	}

	panel_data = &panel->panel_data;
	panel_bl = &panel->panel_bl;

	row = get_actual_brightness_index(panel_bl, panel_bl->props.brightness);

	return maptbl_index(tbl, 0, row, 0);
}

#ifdef CONFIG_SUPPORT_HMD
static int init_gamma_mode2_hmd_brt_table(struct maptbl *tbl)
{
	struct panel_info *panel_data;
	struct panel_device *panel;
	struct panel_dimming_info *panel_dim_info;

	panel_info("++\n");
	if (tbl == NULL) {
		panel_err("maptbl is null\n");
		return -EINVAL;
	}

	if (tbl->pdata == NULL) {
		panel_err("pdata is null\n");
		return -EINVAL;
	}

	panel = tbl->pdata;
	panel_data = &panel->panel_data;

	panel_dim_info = panel_data->panel_dim_info[PANEL_BL_SUBDEV_TYPE_HMD];

	if (panel_dim_info == NULL) {
		panel_err("panel_dim_info is null\n");
		return -EINVAL;
	}

	if (panel_dim_info->brt_tbl == NULL) {
		panel_err("panel_dim_info->brt_tbl is null\n");
		return -EINVAL;
	}

	generate_brt_step_table(panel_dim_info->brt_tbl);

	/* initialize brightness_table */
	memcpy(&panel->panel_bl.subdev[PANEL_BL_SUBDEV_TYPE_HMD].brt_tbl,
			panel_dim_info->brt_tbl, sizeof(struct brightness_table));

	return 0;
}

static int getidx_gamma_mode2_hmd_brt_table(struct maptbl *tbl)
{
	int row = 0;
	struct panel_info *panel_data;
	struct panel_bl_device *panel_bl;
	struct panel_device *panel = (struct panel_device *)tbl->pdata;

	if (panel == NULL) {
		panel_err("panel is null\n");
		return -EINVAL;
	}
	panel_bl = &panel->panel_bl;
	panel_data = &panel->panel_data;

	row = get_brightness_pac_step(panel_bl, panel_bl->props.brightness);

	return maptbl_index(tbl, 0, row, 0);
}
#endif

static int getidx_sync_control_table(struct maptbl *tbl)
{
	int layer = 0, row = 0, dir;
	struct panel_info *panel_data;
	struct panel_bl_device *panel_bl;
	struct panel_device *panel = (struct panel_device *)tbl->pdata;
	struct panel_vrr *vrr;

	if (panel == NULL) {
		panel_err("panel is null\n");
		return -EINVAL;
	}

	panel_data = &panel->panel_data;
	panel_bl = &panel->panel_bl;

	row = panel_bl->props.smooth_transition;
	vrr = get_panel_vrr(panel);
	if (vrr == NULL) {
		panel_err("vrr is null\n");
		return maptbl_index(tbl, layer, row, 0);
	}
	if (get_s6e3had_96hs_mode(find_s6e3had_vrr(vrr)) == S6E3HAD_96HS_MODE_ON) {
		//96hs
		dir = get_s6e3had_brt_direction(panel);
		if (dir > 0) {
			//transition from non-hbm to hbm mode
			row = SMOOTH_TRANS_OFF;
			panel_dbg("96hs to hbm\n");
		} else if (dir < 0) {
			//transition from hbm to non-hbm mode
			row = SMOOTH_TRANS_ON;
			panel_dbg("96hs to normal\n");
		} else {
			if (is_hbm_brightness(panel_bl, panel_bl->props.brightness)) {
				//transition inside of hbm
				row = SMOOTH_TRANS_ON;
				panel_dbg("96hs hbm\n");
			} else {
				//transition inside of normal
				row = SMOOTH_TRANS_OFF;
				panel_dbg("96hs normal\n");
			}
		}
	}
	return maptbl_index(tbl, layer, row, 0);
}

static int getidx_hbm_transition_table(struct maptbl *tbl)
{
	int layer, row, dir;
	struct panel_info *panel_data;
	struct panel_bl_device *panel_bl;
	struct panel_device *panel = (struct panel_device *)tbl->pdata;
	struct panel_vrr *vrr;

	if (panel == NULL) {
		panel_err("panel is null\n");
		return -EINVAL;
	}

	panel_data = &panel->panel_data;
	panel_bl = &panel->panel_bl;

	layer = is_hbm_brightness(panel_bl, panel_bl->props.brightness);
	row = panel_bl->props.smooth_transition;

	vrr = get_panel_vrr(panel);
	if (vrr == NULL) {
		panel_err("vrr is null\n");
		return maptbl_index(tbl, layer, row, 0);
	}

	if (get_s6e3had_96hs_mode(find_s6e3had_vrr(vrr)) == S6E3HAD_96HS_MODE_ON) {
		dir = get_s6e3had_brt_direction(panel);
		if (dir == 0) {
			if (layer)
				row = SMOOTH_TRANS_ON;
			else
				row = SMOOTH_TRANS_OFF;
		} else {
			row = SMOOTH_TRANS_OFF;
		}
	}
	return maptbl_index(tbl, layer, row, 0);
}

static int getidx_hbm_table(struct maptbl *tbl)
{
	int row;
	struct panel_bl_device *panel_bl;
	struct panel_device *panel = (struct panel_device *)tbl->pdata;

	if (panel == NULL) {
		panel_err("panel is null\n");
		return -EINVAL;
	}

	panel_bl = &panel->panel_bl;

	row = is_hbm_brightness(panel_bl, panel_bl->props.brightness);

	return maptbl_index(tbl, 0, row, 0);
}

static int getidx_dia_onoff_table(struct maptbl *tbl)
{
	struct panel_device *panel = (struct panel_device *)tbl->pdata;
	struct panel_info *panel_data;

	if (panel == NULL) {
		panel_err("panel is null\n");
		return -EINVAL;
	}
	panel_data = &panel->panel_data;

	return maptbl_index(tbl, 0, panel_data->props.dia_mode, 0);
}

static int getidx_irc_mode_table(struct maptbl *tbl)
{
	struct panel_device *panel = (struct panel_device *)tbl->pdata;

	return maptbl_index(tbl, 0, panel->panel_data.props.irc_mode ? 1 : 0, 0);
}

#ifdef CONFIG_SUPPORT_XTALK_MODE
static int getidx_vgh_table(struct maptbl *tbl)
{
	struct panel_device *panel = (struct panel_device *)tbl->pdata;
	struct panel_info *panel_data;
	int row = 0;

	if (panel == NULL) {
		panel_err("panel is null\n");
		return -EINVAL;
	}

	panel_data = &panel->panel_data;

	row = ((panel_data->props.xtalk_mode) ? 1 : 0);
	panel_info("xtalk_mode %d\n", row);

	return maptbl_index(tbl, 0, row, 0);
}
#endif

static int getidx_acl_opr_table(struct maptbl *tbl)
{
	struct panel_device *panel = (struct panel_device *)tbl->pdata;
	struct panel_bl_device *panel_bl;
	u32 layer, row;

	if (panel == NULL) {
		panel_err("panel is null\n");
		return -EINVAL;
	}
	panel_bl = &panel->panel_bl;

	layer = is_hbm_brightness(panel_bl, panel_bl->props.brightness) ? PANEL_HBM_ON : PANEL_HBM_OFF;
	row = panel_bl_get_acl_opr(panel_bl);
	if (tbl->nrow <= row) {
		panel_err("invalid acl opr range %d %d\n", tbl->nrow, row);
		row = 0;
	}

	return maptbl_index(tbl, layer, row, 0);
}

static int getidx_acl_start_point_table(struct maptbl *tbl)
{
	struct panel_device *panel = (struct panel_device *)tbl->pdata;
	struct panel_bl_device *panel_bl;
	u32 layer = 0, row;

	if (panel == NULL) {
		panel_err("panel is null\n");
		return -EINVAL;
	}
	panel_bl = &panel->panel_bl;

	row = panel_bl_get_acl_opr(panel_bl);
	if (tbl->nrow <= row) {
		panel_err("invalid acl opr range %d %d\n", tbl->nrow, row);
		row = 0;
	}

	return maptbl_index(tbl, layer, row, 0);
}

static int getidx_dsc_table(struct maptbl *tbl)
{
	struct panel_device *panel = (struct panel_device *)tbl->pdata;
	struct panel_properties *props;
	struct panel_mres *mres;
	int row = 1;

	if (panel == NULL) {
		panel_err("panel is null\n");
		return -EINVAL;
	}
	props = &panel->panel_data.props;
	mres = &panel->panel_data.mres;

	if (mres->nr_resol == 0 || mres->resol == NULL) {
		panel_info("nor_resol is null\n");
		return maptbl_index(tbl, 0, row, 0);
	}

	if (mres->resol[props->mres_mode].comp_type
			== PN_COMP_TYPE_DSC)
		row = 1;

	return maptbl_index(tbl, 0, row, 0);
}

static int getidx_resolution_table(struct maptbl *tbl)
{
	struct panel_device *panel = (struct panel_device *)tbl->pdata;
	struct panel_mres *mres = &panel->panel_data.mres;
	struct panel_properties *props = &panel->panel_data.props;
	int row = 0, layer = 0, index;
	int xres = 0, yres = 0;

	if (mres->nr_resol == 0 || mres->resol == NULL)
		return maptbl_index(tbl, layer, row, 0);

	if (props->mres_mode >= mres->nr_resol) {
		xres = mres->resol[0].w;
		yres = mres->resol[0].h;
		panel_err("invalid mres_mode %d, nr_resol %d\n",
				props->mres_mode, mres->nr_resol);
	} else {
		xres = mres->resol[props->mres_mode].w;
		yres = mres->resol[props->mres_mode].h;
		panel_info("mres_mode %d (%dx%d)\n",
				props->mres_mode,
				mres->resol[props->mres_mode].w,
				mres->resol[props->mres_mode].h);
	}

	index = search_table_u32(S6E3HAD_SCALER_1440,
			ARRAY_SIZE(S6E3HAD_SCALER_1440), xres);
	if (index < 0)
		row = 0;

	row = index;

	return maptbl_index(tbl, layer, row, 0);
}

static int getidx_vrr_fps_table(struct maptbl *tbl)
{
	struct panel_device *panel = (struct panel_device *)tbl->pdata;
	struct panel_vrr *vrr;
	int row = 0, layer = 0, index;

	vrr = get_panel_vrr(panel);
	if (vrr == NULL)
		return -EINVAL;

	index = find_s6e3had_vrr(vrr);
	if (index < 0)
		row = (vrr->mode == VRR_NORMAL_MODE) ?
			S6E3HAD_VRR_60NS : S6E3HAD_VRR_120HS;
	else
		row = index;

	return maptbl_index(tbl, layer, row, 0);
}

static int getidx_aor_manual_onoff_table(struct maptbl *tbl)
{
	struct panel_device *panel = (struct panel_device *)tbl->pdata;
	struct panel_vrr *vrr;
	struct panel_bl_device *panel_bl;
	int row = 0, layer = 0;

	panel_bl = &panel->panel_bl;

	vrr = get_panel_vrr(panel);
	if (vrr == NULL)
		return -EINVAL;

	row = get_s6e3had_96hs_mode(find_s6e3had_vrr(vrr));
	return maptbl_index(tbl, layer, row, 0);
}

static int getidx_aor_manual_value_table(struct maptbl *tbl)
{
	struct panel_device *panel = (struct panel_device *)tbl->pdata;
	struct panel_vrr *vrr;
	struct panel_bl_device *panel_bl;
	int row = 0, layer = 0;

	panel_bl = &panel->panel_bl;

	vrr = get_panel_vrr(panel);
	if (vrr == NULL)
		return -EINVAL;

	layer = get_s6e3had_unbound3_id(panel);
	row = get_actual_brightness_index(panel_bl, panel_bl->props.brightness);

	return maptbl_index(tbl, layer, row, 0);
}

static int getidx_read_addr_table(struct maptbl *tbl)
{
	struct panel_device *panel = (struct panel_device *)tbl->pdata;
	int row = 0, layer = 0;
	u8 id;

	id = panel->panel_data.id[2] & 0xFF;

	if (id < 0xE3)
		row = S6E3HAD_UNBOUND3_FLASH_ADDR_ID_LT_E3;
	else if (id < 0xE5)
		row = S6E3HAD_UNBOUND3_FLASH_ADDR_ID_LT_E5;
	else if (id < 0xE8)
		row = S6E3HAD_UNBOUND3_FLASH_ADDR_ID_LT_E8;
	else
		row = S6E3HAD_UNBOUND3_FLASH_ADDR_ID_GTE_E8;

	panel_info("id %02X %d\n", panel->panel_data.id[2], row);

	return maptbl_index(tbl, layer, row, 0);
}


static int init_lpm_brt_table(struct maptbl *tbl)
{
#ifdef CONFIG_SUPPORT_AOD_BL
	return init_aod_dimming_table(tbl);
#else
	return init_common_table(tbl);
#endif
}

static int getidx_lpm_brt_table(struct maptbl *tbl)
{
	int row = 0;
	struct panel_device *panel;
	struct panel_bl_device *panel_bl;
	struct panel_properties *props;

	panel = (struct panel_device *)tbl->pdata;
	panel_bl = &panel->panel_bl;
	props = &panel->panel_data.props;

#ifdef CONFIG_EXYNOS_DOZE
#ifdef CONFIG_SUPPORT_AOD_BL
	panel_bl = &panel->panel_bl;
	row = get_subdev_actual_brightness_index(panel_bl, PANEL_BL_SUBDEV_TYPE_AOD,
			panel_bl->subdev[PANEL_BL_SUBDEV_TYPE_AOD].brightness);

	props->lpm_brightness = panel_bl->subdev[PANEL_BL_SUBDEV_TYPE_AOD].brightness;
	panel_info("alpm_mode %d, brightness %d, row %d\n", props->cur_alpm_mode,
		panel_bl->subdev[PANEL_BL_SUBDEV_TYPE_AOD].brightness, row);

#else
	switch (props->alpm_mode) {
	case ALPM_LOW_BR:
	case HLPM_LOW_BR:
		row = 0;
		break;
	case ALPM_HIGH_BR:
	case HLPM_HIGH_BR:
		row = tbl->nrow - 1;
		break;
	default:
		panel_err("Invalid alpm mode : %d\n", props->alpm_mode);
		break;
	}

	panel_info("alpm_mode %d, row %d\n", props->alpm_mode, row);
#endif
#endif

	return maptbl_index(tbl, 0, row, 0);
}

static int getidx_lpm_mode_table(struct maptbl *tbl)
{
	int row = 0;
	struct panel_device *panel;
	struct panel_bl_device *panel_bl;
	struct panel_properties *props;

	panel = (struct panel_device *)tbl->pdata;
	panel_bl = &panel->panel_bl;
	props = &panel->panel_data.props;

#ifdef CONFIG_EXYNOS_DOZE
	switch (props->alpm_mode) {
	case ALPM_LOW_BR:
	case ALPM_HIGH_BR:
		row = ALPM_MODE;
		break;
	case HLPM_LOW_BR:
	case HLPM_HIGH_BR:
	default:
		row = HLPM_MODE;
		break;
	}
	panel_info("alpm_mode %d -> %d row %d\n", props->cur_alpm_mode, props->alpm_mode, row);
	props->cur_alpm_mode = props->alpm_mode;
#endif

	return maptbl_index(tbl, 0, row, 0);
}

static int getidx_lpm_fps_table(struct maptbl *tbl)
{
	int row = LPM_LFD_1HZ, lpm_lfd_min_freq;
	struct panel_device *panel;
	struct panel_properties *props;
	struct vrr_lfd_config *vrr_lfd_config;
	struct vrr_lfd_status *vrr_lfd_status;

	panel = (struct panel_device *)tbl->pdata;
	props = &panel->panel_data.props;

	vrr_lfd_status = &props->vrr_lfd_info.status[VRR_LFD_SCOPE_LPM];
	vrr_lfd_status->lfd_max_freq = 30;
	vrr_lfd_status->lfd_max_freq_div = 1;
	vrr_lfd_status->lfd_min_freq = 1;
	vrr_lfd_status->lfd_min_freq_div = 30;

	vrr_lfd_config = &props->vrr_lfd_info.cur[VRR_LFD_SCOPE_LPM];
	if (vrr_lfd_config->fix == VRR_LFD_FREQ_HIGH) {
		row = LPM_LFD_30HZ;
		vrr_lfd_status->lfd_min_freq = 30;
		vrr_lfd_status->lfd_min_freq_div = 1;
		panel_info("lpm_fps %dhz (row:%d)\n",
				(row == LPM_LFD_1HZ) ? 1 : 30, row);
		return maptbl_index(tbl, 0, row, 0);
	}

	lpm_lfd_min_freq =
		get_s6e3had_lpm_lfd_min_freq(vrr_lfd_config->scalability);
	if (lpm_lfd_min_freq <= 0 || lpm_lfd_min_freq > 1) {
		row = LPM_LFD_30HZ;
		vrr_lfd_status->lfd_min_freq = 30;
		vrr_lfd_status->lfd_min_freq_div = 1;
		panel_info("lpm_fps %dhz (row:%d)\n",
				(row == LPM_LFD_1HZ) ? 1 : 30, row);
		return maptbl_index(tbl, 0, row, 0);
	}

#ifdef CONFIG_EXYNOS_DOZE
	switch (props->lpm_fps) {
	case LPM_LFD_1HZ:
		row = props->lpm_fps;
		vrr_lfd_status->lfd_min_freq = 1;
		vrr_lfd_status->lfd_min_freq_div = 30;
		break;
	case LPM_LFD_30HZ:
		row = props->lpm_fps;
		vrr_lfd_status->lfd_min_freq = 30;
		vrr_lfd_status->lfd_min_freq_div = 1;
		break;
	default:
		panel_err("invalid lpm_fps %d\n",
				props->lpm_fps);
		break;
	}
		panel_info("lpm_fps %dhz(row:%d)\n",
				(row == LPM_LFD_1HZ) ? 1 : 30, row);
#endif

	return maptbl_index(tbl, 0, row, 0);
}

static int getidx_vrr_mode_table(struct maptbl *tbl)
{
	struct panel_device *panel = (struct panel_device *)tbl->pdata;
	int row = 0, layer = 0;
	int vrr_mode;

	vrr_mode = get_panel_refresh_mode(panel);
	if (vrr_mode < 0)
		return -EINVAL;

	row = getidx_s6e3had_vrr_mode(vrr_mode);
	if (row < 0) {
		panel_err("failed to getidx_s6e3had_vrr_mode(mode:%d)\n", vrr_mode);
		row = 0;
	}
	panel_dbg("vrr_mode:%d(%s)\n", row,
			row == S6E3HAD_VRR_MODE_HS ? "HS" : "NM");

	return maptbl_index(tbl, layer, row, 0);
}

static int getidx_lfd_frame_insertion_table(struct maptbl *tbl)
{
	struct panel_device *panel = (struct panel_device *)tbl->pdata;
	struct panel_info *panel_data = &panel->panel_data;
	struct panel_properties *props = &panel->panel_data.props;
	struct vrr_lfd_config *vrr_lfd_config;
	int vrr_mode;
	int lfd_max_freq, lfd_max_index;
	int lfd_min_freq, lfd_min_index;

	vrr_mode = get_panel_refresh_mode(panel);
	if (vrr_mode < 0)
		return -EINVAL;

	vrr_lfd_config = &props->vrr_lfd_info.cur[VRR_LFD_SCOPE_NORMAL];
	lfd_max_freq = s6e3had_get_vrr_lfd_max_freq(panel_data);
	if (lfd_max_freq < 0) {
		panel_err("failed to get s6e3had_get_vrr_lfd_max_freq\n");
		return -EINVAL;
	}

	lfd_min_freq = s6e3had_get_vrr_lfd_min_freq(panel_data);
	if (lfd_min_freq < 0) {
		panel_err("failed to get s6e3had_get_vrr_lfd_min_freq\n");
		return -EINVAL;
	}

	lfd_max_index = getidx_s6e3had_lfd_frame_idx(lfd_max_freq, vrr_mode);
	if (lfd_max_index < 0) {
		panel_err("failed to get lfd_max_index(lfd_max_freq:%d vrr_mode:%d)\n",
				lfd_max_freq, vrr_mode);
		return -EINVAL;
	}

	lfd_min_index = getidx_s6e3had_lfd_frame_idx(lfd_min_freq, vrr_mode);
	if (lfd_min_index < 0) {
		panel_err("failed to get lfd_min_index(lfd_min_freq:%d vrr_mode:%d)\n",
				lfd_min_freq, vrr_mode);
		return -EINVAL;
	}

	panel_dbg("lfd_max_freq %d%s(%d) lfd_min_freq %d%s(%d)\n",
			lfd_max_freq, vrr_mode == S6E3HAD_VRR_MODE_HS ? "HS" : "NM", lfd_max_index,
			lfd_min_freq, vrr_mode == S6E3HAD_VRR_MODE_HS ? "HS" : "NM", lfd_min_index);

	return maptbl_index(tbl, lfd_max_index, lfd_min_index, 0);
}

#ifdef CONFIG_SUPPORT_GRAM_CHECKSUM
static int s6e3had_getidx_vddm_table(struct maptbl *tbl)
{
	struct panel_device *panel = (struct panel_device *)tbl->pdata;
	struct panel_properties *props = &panel->panel_data.props;

	panel_info("vddm %d\n", props->gct_vddm);

	return maptbl_index(tbl, 0, props->gct_vddm, 0);
}

static int s6e3had_getidx_gram_img_pattern_table(struct maptbl *tbl)
{
	struct panel_device *panel = (struct panel_device *)tbl->pdata;
	struct panel_properties *props = &panel->panel_data.props;

	panel_info("gram img %d\n", props->gct_pattern);
	props->gct_valid_chksum[0] = S6E3HAD_GRAM_CHECKSUM_VALID_1;
	props->gct_valid_chksum[1] = S6E3HAD_GRAM_CHECKSUM_VALID_2;
	props->gct_valid_chksum[2] = S6E3HAD_GRAM_CHECKSUM_VALID_1;
	props->gct_valid_chksum[3] = S6E3HAD_GRAM_CHECKSUM_VALID_2;

	return maptbl_index(tbl, 0, props->gct_pattern, 0);
}
#endif

#ifdef CONFIG_SUPPORT_BRIGHTDOT_TEST
static int s6e3had_getidx_brightdot_aor_table(struct maptbl *tbl)
{
	struct panel_device *panel = (struct panel_device *)tbl->pdata;
	struct panel_properties *props = &panel->panel_data.props;

	return maptbl_index(tbl, 0, props->brightdot_test_enable, 0);

}

static bool s6e3had_is_brightdot_enabled(struct panel_device *panel)
{
	if (panel->panel_data.props.brightdot_test_enable != 0)
		return true;

	return false;
}

#endif

#ifdef CONFIG_SUPPORT_TDMB_TUNE
static int s6e3had_getidx_tdmb_tune_table(struct maptbl *tbl)
{
	struct panel_device *panel = (struct panel_device *)tbl->pdata;
	struct panel_properties *props = &panel->panel_data.props;

	return maptbl_index(tbl, 0, props->tdmb_on, 0);
}
#endif


#ifdef CONFIG_EXYNOS_DECON_MDNIE_LITE
static void copy_dummy_maptbl(struct maptbl *tbl, u8 *dst)
{
}
#endif

static void copy_common_maptbl(struct maptbl *tbl, u8 *dst)
{
	int idx;

	if (!tbl || !dst) {
		panel_err("invalid parameter (tbl %p, dst %p)\n",
				tbl, dst);
		return;
	}

	idx = maptbl_getidx(tbl);
	if (idx < 0) {
		panel_err("failed to getidx %d\n", idx);
		return;
	}

	memcpy(dst, &(tbl->arr)[idx], sizeof(u8) * tbl->sz_copy);
	panel_dbg("copy from %s %d %d\n",
			tbl->name, idx, tbl->sz_copy);
	print_data(dst, tbl->sz_copy);
}

static void copy_tset_maptbl(struct maptbl *tbl, u8 *dst)
{
	struct panel_device *panel;
	struct panel_info *panel_data;

	if (!tbl || !dst)
		return;

	panel = (struct panel_device *)tbl->pdata;
	if (unlikely(!panel))
		return;

	panel_data = &panel->panel_data;

	*dst = (panel_data->props.temperature < 0) ?
		BIT(7) | abs(panel_data->props.temperature) :
		panel_data->props.temperature;
}

static void copy_vaint_maptbl(struct maptbl *tbl, u8 *dst)
{
	struct panel_device *panel;
	struct panel_info *panel_data;
	struct panel_bl_device *panel_bl;
	u8 val;
	int ret = 0, idx;

	if (!tbl || !dst)
		return;

	panel = (struct panel_device *)tbl->pdata;
	if (unlikely(!panel))
		return;

	panel_data = &panel->panel_data;
	panel_bl = &panel->panel_bl;

	if (!tbl || !dst) {
		panel_err("invalid parameter (tbl %p, dst %p)\n", tbl, dst);
		return;
	}

	idx = maptbl_getidx(tbl);
	if (idx < 0) {
		panel_err("failed to getidx %d\n", idx);
		return;
	}

	memcpy(dst, &(tbl->arr)[idx], sizeof(u8) * tbl->sz_copy);
	panel_dbg("copy from %s %d %d\n", tbl->name, idx, tbl->sz_copy);

	if (*dst == MTP) {
		ret = resource_copy_by_name(panel_data, &val, "vaint");
		if (unlikely(ret)) {
			panel_err("vaint not found in panel resource\n");
			val = 0x00;
		}
		*dst = val;
	}
	print_data(dst, tbl->sz_copy);
}

#ifdef CONFIG_SUPPORT_GRAYSPOT_TEST
static void copy_grayspot_cal_maptbl(struct maptbl *tbl, u8 *dst)
{
	struct panel_device *panel;
	struct panel_info *panel_data;
	u8 val;
	int ret = 0;

	if (!tbl || !dst)
		return;

	panel = (struct panel_device *)tbl->pdata;
	if (unlikely(!panel))
		return;

	panel_data = &panel->panel_data;

	ret = resource_copy_by_name(panel_data, &val, "grayspot_cal");
	if (unlikely(ret)) {
		panel_err("grayspot_cal not found in panel resource\n");
		return;
	}

	panel_info("grayspot_cal 0x%02x\n", val);
	*dst = val;
}
#endif

#ifdef CONFIG_EXYNOS_DECON_LCD_COPR
static void copy_copr_maptbl(struct maptbl *tbl, u8 *dst)
{
	struct copr_info *copr;
	struct copr_reg_v6 *reg;
	int i, offset = 0;

	if (!tbl || !dst)
		return;

	copr = (struct copr_info *)tbl->pdata;
	if (unlikely(!copr))
		return;

	reg = &copr->props.reg.v6;

	dst[offset++] = (reg->copr_mask << 4) | (reg->copr_pwr << 1) | reg->copr_en;
	dst[offset++] = reg->roi_on;
	dst[offset++] = reg->copr_gamma;
	dst[offset++] = (reg->copr_frame_count >> 8) & 0x03;
	dst[offset++] = reg->copr_frame_count & 0xFF;

	/* COPR_ROI_ER/G/B */
	for (i = 0; i < 5; i++) {
		dst[offset++] = (reg->roi[i].roi_er >> 8) & 0x3;
		dst[offset++] = reg->roi[i].roi_er & 0xFF;
		dst[offset++] = (reg->roi[i].roi_eg >> 8) & 0x3;
		dst[offset++] = reg->roi[i].roi_eg & 0xFF;
		dst[offset++] = (reg->roi[i].roi_eb >> 8) & 0x3;
		dst[offset++] = reg->roi[i].roi_eb & 0xFF;
	}

	/* COPR_ROI_XS/YS/XE/YE */
	for (i = 0; i < 5; i++) {
		dst[offset++] = (reg->roi[i].roi_xs >> 8) & 0x7;
		dst[offset++] = reg->roi[i].roi_xs & 0xFF;
		dst[offset++] = (reg->roi[i].roi_ys >> 8) & 0xF;
		dst[offset++] = reg->roi[i].roi_ys & 0xFF;
		dst[offset++] = (reg->roi[i].roi_xe >> 8) & 0x7;
		dst[offset++] = reg->roi[i].roi_xe & 0xFF;
		dst[offset++] = (reg->roi[i].roi_ye >> 8) & 0xF;
		dst[offset++] = reg->roi[i].roi_ye & 0xFF;
	}

	if (offset != S6E3HAD_COPR_CTRL_LEN)
		panel_err("copr ctrl register size mismatch(%d, %d)\n",
				offset, S6E3HAD_COPR_CTRL_LEN);

	print_data(dst, S6E3HAD_COPR_CTRL_LEN);
}
#endif

static void copy_lfd_min_maptbl(struct maptbl *tbl, u8 *dst)
{
	struct panel_device *panel = (struct panel_device *)tbl->pdata;
	struct panel_info *panel_data = &panel->panel_data;
	struct panel_properties *props = &panel_data->props;
	struct vrr_lfd_config *vrr_lfd_config;
	struct vrr_lfd_status *vrr_lfd_status;
	struct panel_vrr *vrr;
	int vrr_fps, vrr_mode;
	u32 vrr_div_count, value;

	vrr = get_panel_vrr(panel);
	if (vrr == NULL)
		return;

	vrr_fps = vrr->fps;
	vrr_mode = vrr->mode;
	vrr_div_count = TE_SKIP_TO_DIV(vrr->te_sw_skip_count, vrr->te_hw_skip_count);
	if (vrr_div_count < MIN_S6E3HAD_FPS_DIV_COUNT ||
		vrr_div_count > MAX_S6E3HAD_FPS_DIV_COUNT) {
		panel_err("out of range vrr(%d %d) vrr_div_count(%d)\n",
				vrr_fps, vrr_mode, vrr_div_count);
		return;
	}

	vrr_lfd_config = &props->vrr_lfd_info.cur[VRR_LFD_SCOPE_NORMAL];
	vrr_div_count = s6e3had_get_vrr_lfd_min_div_count(panel_data);
	if (vrr_div_count <= 0) {
		panel_err("failed to get vrr(%d %d) div count\n",
				vrr_fps, vrr_mode);
		return;
	}

	/* update lfd_min status */
	vrr_lfd_status = &props->vrr_lfd_info.status[VRR_LFD_SCOPE_NORMAL];
	vrr_lfd_status->lfd_min_freq_div = vrr_div_count;
	vrr_lfd_status->lfd_min_freq =
		disp_div_round(vrr_fps, vrr_div_count);

	panel_dbg("vrr(%d %d) lfd(fix:%d scale:%d min:%d max:%d) --> lfd_min(1/%d)\n",
			vrr_fps, vrr_mode,
			vrr_lfd_config->fix, vrr_lfd_config->scalability,
			vrr_lfd_config->min, vrr_lfd_config->max, vrr_div_count);

	/* change modulation count to skip frame count */
	value = (u32)(vrr_div_count - MIN_VRR_DIV_COUNT) << 2;
	dst[0] = (value >> 8) & 0xFF;
	dst[1] = value & 0xFF;
}

static void copy_lfd_max_maptbl(struct maptbl *tbl, u8 *dst)
{
	struct panel_device *panel = (struct panel_device *)tbl->pdata;
	struct panel_info *panel_data = &panel->panel_data;
	struct panel_properties *props = &panel_data->props;
	struct vrr_lfd_config *vrr_lfd_config;
	struct vrr_lfd_status *vrr_lfd_status;
	struct panel_vrr *vrr;
	int vrr_fps, vrr_mode;
	u32 vrr_div_count, value;

	vrr = get_panel_vrr(panel);
	if (vrr == NULL)
		return;

	vrr_fps = vrr->fps;
	vrr_mode = vrr->mode;
	vrr_div_count = TE_SKIP_TO_DIV(vrr->te_sw_skip_count, vrr->te_hw_skip_count);
	if (vrr_div_count < MIN_S6E3HAD_FPS_DIV_COUNT ||
		vrr_div_count > MAX_S6E3HAD_FPS_DIV_COUNT) {
		panel_err("out of range vrr(%d %d) vrr_div_count(%d)\n",
				vrr_fps, vrr_mode, vrr_div_count);
		return;
	}

	vrr_lfd_config = &props->vrr_lfd_info.cur[VRR_LFD_SCOPE_NORMAL];
	vrr_div_count = s6e3had_get_vrr_lfd_max_div_count(panel_data);
	if (vrr_div_count <= 0) {
		panel_err("failed to get vrr(%d %d) div count\n",
				vrr_fps, vrr_mode);
		return;
	}

	/* update lfd_max status */
	vrr_lfd_status = &props->vrr_lfd_info.status[VRR_LFD_SCOPE_NORMAL];
	vrr_lfd_status->lfd_max_freq_div = vrr_div_count;
	vrr_lfd_status->lfd_max_freq =
		disp_div_round(vrr_fps, vrr_div_count);

	panel_dbg("vrr(%d %d) lfd(fix:%d scale:%d min:%d max:%d) --> lfd_max(1/%d)\n",
			vrr_fps, vrr_mode,
			vrr_lfd_config->fix, vrr_lfd_config->scalability,
			vrr_lfd_config->min, vrr_lfd_config->max, vrr_div_count);

	/* change modulation count to skip frame count */
	value = (u32)(vrr_div_count - MIN_VRR_DIV_COUNT) << 2;
	if (vrr_lfd_status->lfd_max_freq > 60) {
		dst[0] = 0x08;
		dst[1] = 0x00;
	} else {
		dst[0] = (value >> 8) & 0xFF;
		dst[1] = value & 0xFF;
	}
}

#ifdef CONFIG_EXYNOS_DECON_MDNIE_LITE
static int init_color_blind_table(struct maptbl *tbl)
{
	struct mdnie_info *mdnie;

	if (tbl == NULL) {
		panel_err("maptbl is null\n");
		return -EINVAL;
	}

	if (tbl->pdata == NULL) {
		panel_err("pdata is null\n");
		return -EINVAL;
	}

	mdnie = tbl->pdata;

	if (S6E3HAD_SCR_CR_OFS + mdnie->props.sz_scr > sizeof_maptbl(tbl)) {
		panel_err("invalid size (maptbl_size %d, sz_scr %d)\n",
				sizeof_maptbl(tbl), mdnie->props.sz_scr);
		return -EINVAL;
	}

	memcpy(&tbl->arr[S6E3HAD_SCR_CR_OFS],
			mdnie->props.scr, mdnie->props.sz_scr);

	return 0;
}

static int getidx_mdnie_scenario_maptbl(struct maptbl *tbl)
{
	struct mdnie_info *mdnie = (struct mdnie_info *)tbl->pdata;

	return tbl->ncol * (mdnie->props.mode);
}

static int getidx_mdnie_hdr_maptbl(struct maptbl *tbl)
{
	struct mdnie_info *mdnie = (struct mdnie_info *)tbl->pdata;

	return tbl->ncol * (mdnie->props.hdr);
}

static int getidx_mdnie_trans_mode_maptbl(struct maptbl *tbl)
{
	struct mdnie_info *mdnie = (struct mdnie_info *)tbl->pdata;

	if (mdnie->props.trans_mode == TRANS_OFF)
		panel_dbg("mdnie trans_mode off\n");
	return tbl->ncol * (mdnie->props.trans_mode);
}

static int getidx_mdnie_night_mode_maptbl(struct maptbl *tbl)
{
	int mode = 0;
	struct mdnie_info *mdnie = (struct mdnie_info *)tbl->pdata;

	if (mdnie->props.mode != AUTO)
		mode = 1;

	return maptbl_index(tbl, mode, mdnie->props.night_level, 0);
}

static int init_mdnie_night_mode_table(struct maptbl *tbl)
{
	struct mdnie_info *mdnie;
	struct maptbl *night_maptbl;

	if (tbl == NULL) {
		panel_err("maptbl is null\n");
		return -EINVAL;
	}

	if (tbl->pdata == NULL) {
		panel_err("pdata is null\n");
		return -EINVAL;
	}

	mdnie = tbl->pdata;

	night_maptbl = mdnie_find_etc_maptbl(mdnie, MDNIE_ETC_NIGHT_MAPTBL);
	if (!night_maptbl) {
		panel_err("NIGHT_MAPTBL not found\n");
		return -EINVAL;
	}

	if (sizeof_maptbl(tbl) < (S6E3HAD_NIGHT_MODE_OFS +
			sizeof_row(night_maptbl))) {
		panel_err("invalid size (maptbl_size %d, night_maptbl_size %d)\n",
				sizeof_maptbl(tbl), sizeof_row(night_maptbl));
		return -EINVAL;
	}

	maptbl_copy(night_maptbl, &tbl->arr[S6E3HAD_NIGHT_MODE_OFS]);

	return 0;
}

static int init_mdnie_color_lens_table(struct maptbl *tbl)
{
	struct mdnie_info *mdnie;
	struct maptbl *color_lens_maptbl;

	if (tbl == NULL) {
		panel_err("maptbl is null\n");
		return -EINVAL;
	}

	if (tbl->pdata == NULL) {
		panel_err("pdata is null\n");
		return -EINVAL;
	}

	mdnie = tbl->pdata;

	color_lens_maptbl = mdnie_find_etc_maptbl(mdnie, MDNIE_ETC_COLOR_LENS_MAPTBL);
	if (!color_lens_maptbl) {
		panel_err("COLOR_LENS_MAPTBL not found\n");
		return -EINVAL;
	}

	if (sizeof_maptbl(tbl) < (S6E3HAD_COLOR_LENS_OFS +
			sizeof_row(color_lens_maptbl))) {
		panel_err("invalid size (maptbl_size %d, color_lens_maptbl_size %d)\n",
				sizeof_maptbl(tbl), sizeof_row(color_lens_maptbl));
		return -EINVAL;
	}

	if (IS_COLOR_LENS_MODE(mdnie))
		maptbl_copy(color_lens_maptbl, &tbl->arr[S6E3HAD_COLOR_LENS_OFS]);

	return 0;
}

static void update_current_scr_white(struct maptbl *tbl, u8 *dst)
{
	struct mdnie_info *mdnie;

	if (!tbl || !tbl->pdata) {
		panel_err("invalid param\n");
		return;
	}

	mdnie = (struct mdnie_info *)tbl->pdata;
	mdnie->props.cur_wrgb[0] = *dst;
	mdnie->props.cur_wrgb[1] = *(dst + 2);
	mdnie->props.cur_wrgb[2] = *(dst + 4);
}

static int init_color_coordinate_table(struct maptbl *tbl)
{
	struct mdnie_info *mdnie;
	int type, color;

	if (tbl == NULL) {
		panel_err("maptbl is null\n");
		return -EINVAL;
	}

	if (tbl->pdata == NULL) {
		panel_err("pdata is null\n");
		return -EINVAL;
	}

	mdnie = tbl->pdata;

	if (sizeof_row(tbl) != ARRAY_SIZE(mdnie->props.coord_wrgb[0])) {
		panel_err("invalid maptbl size %d\n", tbl->ncol);
		return -EINVAL;
	}

	for_each_row(tbl, type) {
		for_each_col(tbl, color) {
			tbl->arr[sizeof_row(tbl) * type + color] =
				mdnie->props.coord_wrgb[type][color];
		}
	}

	return 0;
}

static int init_sensor_rgb_table(struct maptbl *tbl)
{
	struct mdnie_info *mdnie;
	int i;

	if (tbl == NULL) {
		panel_err("maptbl is null\n");
		return -EINVAL;
	}

	if (tbl->pdata == NULL) {
		panel_err("pdata is null\n");
		return -EINVAL;
	}

	mdnie = tbl->pdata;

	if (tbl->ncol != ARRAY_SIZE(mdnie->props.ssr_wrgb)) {
		panel_err("invalid maptbl size %d\n", tbl->ncol);
		return -EINVAL;
	}

	for (i = 0; i < tbl->ncol; i++)
		tbl->arr[i] = mdnie->props.ssr_wrgb[i];

	return 0;
}

static int getidx_color_coordinate_maptbl(struct maptbl *tbl)
{
	struct mdnie_info *mdnie = (struct mdnie_info *)tbl->pdata;
	static int wcrd_type[MODE_MAX] = {
		WCRD_TYPE_D65, WCRD_TYPE_D65, WCRD_TYPE_D65,
		WCRD_TYPE_ADAPTIVE, WCRD_TYPE_ADAPTIVE,
	};
	if ((mdnie->props.mode < 0) || (mdnie->props.mode >= MODE_MAX)) {
		panel_err("out of mode range %d\n", mdnie->props.mode);
		return -EINVAL;
	}
	return maptbl_index(tbl, 0, wcrd_type[mdnie->props.mode], 0);
}

static int getidx_adjust_ldu_maptbl(struct maptbl *tbl)
{
	struct mdnie_info *mdnie = (struct mdnie_info *)tbl->pdata;
	static int wcrd_type[MODE_MAX] = {
		WCRD_TYPE_D65, WCRD_TYPE_D65, WCRD_TYPE_D65,
		WCRD_TYPE_ADAPTIVE, WCRD_TYPE_ADAPTIVE,
	};

	if (!IS_LDU_MODE(mdnie))
		return -EINVAL;

	if ((mdnie->props.mode < 0) || (mdnie->props.mode >= MODE_MAX)) {
		panel_err("out of mode range %d\n", mdnie->props.mode);
		return -EINVAL;
	}
	if ((mdnie->props.ldu < 0) || (mdnie->props.ldu >= MAX_LDU_MODE)) {
		panel_err("out of ldu mode range %d\n", mdnie->props.ldu);
		return -EINVAL;
	}
	return maptbl_index(tbl, wcrd_type[mdnie->props.mode], mdnie->props.ldu, 0);
}

static int getidx_color_lens_maptbl(struct maptbl *tbl)
{
	struct mdnie_info *mdnie = (struct mdnie_info *)tbl->pdata;

	if (!IS_COLOR_LENS_MODE(mdnie))
		return -EINVAL;

	if ((mdnie->props.color_lens_color < 0) || (mdnie->props.color_lens_color >= COLOR_LENS_COLOR_MAX)) {
		panel_err("out of color lens color range %d\n", mdnie->props.color_lens_color);
		return -EINVAL;
	}
	if ((mdnie->props.color_lens_level < 0) || (mdnie->props.color_lens_level >= COLOR_LENS_LEVEL_MAX)) {
		panel_err("out of color lens level range %d\n", mdnie->props.color_lens_level);
		return -EINVAL;
	}
	return maptbl_index(tbl, mdnie->props.color_lens_color, mdnie->props.color_lens_level, 0);
}

static void copy_color_coordinate_maptbl(struct maptbl *tbl, u8 *dst)
{
	struct mdnie_info *mdnie;
	int i, idx;
	u8 value;

	if (unlikely(!tbl || !dst))
		return;

	mdnie = (struct mdnie_info *)tbl->pdata;
	idx = maptbl_getidx(tbl);
	if (idx < 0 || (idx + MAX_COLOR > sizeof_maptbl(tbl))) {
		panel_err("invalid index %d\n", idx);
		return;
	}

	if (tbl->ncol != MAX_COLOR) {
		panel_err("invalid maptbl size %d\n", tbl->ncol);
		return;
	}

	for (i = 0; i < tbl->ncol; i++) {
		mdnie->props.def_wrgb[i] = tbl->arr[idx + i];
		value = mdnie->props.def_wrgb[i] +
			(char)((mdnie->props.mode == AUTO) ?
				mdnie->props.def_wrgb_ofs[i] : 0);
		mdnie->props.cur_wrgb[i] = value;
		dst[i * 2] = value;
		if (mdnie->props.mode == AUTO)
			panel_dbg("cur_wrgb[%d] %d(%02X) def_wrgb[%d] %d(%02X), def_wrgb_ofs[%d] %d\n",
					i, mdnie->props.cur_wrgb[i], mdnie->props.cur_wrgb[i],
					i, mdnie->props.def_wrgb[i], mdnie->props.def_wrgb[i],
					i, mdnie->props.def_wrgb_ofs[i]);
		else
			panel_dbg("cur_wrgb[%d] %d(%02X) def_wrgb[%d] %d(%02X), def_wrgb_ofs[%d] none\n",
					i, mdnie->props.cur_wrgb[i], mdnie->props.cur_wrgb[i],
					i, mdnie->props.def_wrgb[i], mdnie->props.def_wrgb[i], i);
	}
}

static void copy_scr_white_maptbl(struct maptbl *tbl, u8 *dst)
{
	struct mdnie_info *mdnie;
	int i, idx;

	if (unlikely(!tbl || !dst))
		return;

	mdnie = (struct mdnie_info *)tbl->pdata;
	idx = maptbl_getidx(tbl);
	if (idx < 0 || (idx + MAX_COLOR > sizeof_maptbl(tbl))) {
		panel_err("invalid index %d\n", idx);
		return;
	}

	if (tbl->ncol != MAX_COLOR) {
		panel_err("invalid maptbl size %d\n", tbl->ncol);
		return;
	}

	for (i = 0; i < tbl->ncol; i++) {
		mdnie->props.cur_wrgb[i] = tbl->arr[idx + i];
		dst[i * 2] = tbl->arr[idx + i];
		panel_dbg("cur_wrgb[%d] %d(%02X)\n",
				i, mdnie->props.cur_wrgb[i], mdnie->props.cur_wrgb[i]);
	}
}

static void copy_adjust_ldu_maptbl(struct maptbl *tbl, u8 *dst)
{
	struct mdnie_info *mdnie;
	int i, idx;
	u8 value;

	if (unlikely(!tbl || !dst))
		return;

	mdnie = (struct mdnie_info *)tbl->pdata;
	idx = maptbl_getidx(tbl);
	if (idx < 0 || (idx + MAX_COLOR > sizeof_maptbl(tbl))) {
		panel_err("invalid index %d\n", idx);
		return;
	}

	if (tbl->ncol != MAX_COLOR) {
		panel_err("invalid maptbl size %d\n", tbl->ncol);
		return;
	}

	for (i = 0; i < tbl->ncol; i++) {
		value = tbl->arr[idx + i] +
			(((mdnie->props.mode == AUTO) && (mdnie->props.scenario != EBOOK_MODE)) ?
				mdnie->props.def_wrgb_ofs[i] : 0);
		mdnie->props.cur_wrgb[i] = value;
		dst[i * 2] = value;
		panel_dbg("cur_wrgb[%d] %d(%02X) (orig:0x%02X offset:%d)\n",
				i, mdnie->props.cur_wrgb[i], mdnie->props.cur_wrgb[i],
				tbl->arr[idx + i], mdnie->props.def_wrgb_ofs[i]);
	}
}

static int getidx_trans_maptbl(struct pkt_update_info *pktui)
{
	struct panel_device *panel = pktui->pdata;
	struct mdnie_info *mdnie = &panel->mdnie;

	return (mdnie->props.trans_mode == TRANS_ON) ?
		MDNIE_ETC_NONE_MAPTBL : MDNIE_ETC_TRANS_MAPTBL;
}

static int getidx_mdnie_maptbl(struct pkt_update_info *pktui, int offset)
{
	struct panel_device *panel = pktui->pdata;
	struct mdnie_info *mdnie = &panel->mdnie;
	int row = mdnie_get_maptbl_index(mdnie);
	int index;

	if (row < 0) {
		panel_err("invalid row %d\n", row);
		return -EINVAL;
	}

	index = row * mdnie->nr_reg + offset;
	if (index >= mdnie->nr_maptbl) {
		panel_err("exceeded index %d row %d offset %d\n",
				index, row, offset);
		return -EINVAL;
	}
	return index;
}

static int getidx_mdnie_0_maptbl(struct pkt_update_info *pktui)
{
	return getidx_mdnie_maptbl(pktui, 0);
}

static int getidx_mdnie_1_maptbl(struct pkt_update_info *pktui)
{
	return getidx_mdnie_maptbl(pktui, 1);
}

static int getidx_mdnie_2_maptbl(struct pkt_update_info *pktui)
{
	return getidx_mdnie_maptbl(pktui, 2);
}

static int getidx_mdnie_scr_white_maptbl(struct pkt_update_info *pktui)
{
	struct panel_device *panel = pktui->pdata;
	struct mdnie_info *mdnie = &panel->mdnie;
	int index;

	if (mdnie->props.scr_white_mode < 0 ||
			mdnie->props.scr_white_mode >= MAX_SCR_WHITE_MODE) {
		panel_warn("out of range %d\n",
				mdnie->props.scr_white_mode);
		return -1;
	}

	if (mdnie->props.scr_white_mode == SCR_WHITE_MODE_COLOR_COORDINATE) {
		panel_dbg("coordinate maptbl\n");
		index = MDNIE_COLOR_COORDINATE_MAPTBL;
	} else if (mdnie->props.scr_white_mode == SCR_WHITE_MODE_ADJUST_LDU) {
		panel_dbg("adjust ldu maptbl\n");
		index = MDNIE_ADJUST_LDU_MAPTBL;
	} else if (mdnie->props.scr_white_mode == SCR_WHITE_MODE_SENSOR_RGB) {
		panel_dbg("sensor rgb maptbl\n");
		index = MDNIE_SENSOR_RGB_MAPTBL;
	} else {
		panel_dbg("empty maptbl\n");
		index = MDNIE_SCR_WHITE_NONE_MAPTBL;
	}

	return index;
}
#endif /* CONFIG_EXYNOS_DECON_MDNIE_LITE */

static void show_rddpm(struct dumpinfo *info)
{
	int ret;
	struct resinfo *res = info->res;
	u8 rddpm[S6E3HAD_RDDPM_LEN] = { 0, };
#ifdef CONFIG_LOGGING_BIGDATA_BUG
	extern unsigned int g_rddpm;
#endif

	if (!res || ARRAY_SIZE(rddpm) != res->dlen) {
		panel_err("invalid resource\n");
		return;
	}

	ret = resource_copy(rddpm, info->res);
	if (unlikely(ret < 0)) {
		panel_err("failed to copy rddpm resource\n");
		return;
	}

	panel_info("========== SHOW PANEL [0Ah:RDDPM] INFO ==========\n");
	panel_info("* Reg Value : 0x%02x, Result : %s\n",
			rddpm[0], ((rddpm[0] & 0x9C) == 0x9C) ? "GOOD" : "NG");
	panel_info("* Bootster Mode : %s\n", rddpm[0] & 0x80 ? "ON (GD)" : "OFF (NG)");
	panel_info("* Idle Mode     : %s\n", rddpm[0] & 0x40 ? "ON (NG)" : "OFF (GD)");
	panel_info("* Partial Mode  : %s\n", rddpm[0] & 0x20 ? "ON" : "OFF");
	panel_info("* Sleep Mode    : %s\n", rddpm[0] & 0x10 ? "OUT (GD)" : "IN (NG)");
	panel_info("* Normal Mode   : %s\n", rddpm[0] & 0x08 ? "OK (GD)" : "SLEEP (NG)");
	panel_info("* Display ON    : %s\n", rddpm[0] & 0x04 ? "ON (GD)" : "OFF (NG)");
	panel_info("=================================================\n");
#ifdef CONFIG_LOGGING_BIGDATA_BUG
	g_rddpm = (unsigned int)rddpm[0];
#endif
}

static void show_rddpm_before_sleep_in(struct dumpinfo *info)
{
	int ret;
	struct resinfo *res = info->res;
	u8 rddpm[S6E3HAD_RDDPM_LEN] = { 0, };

	if (!res || ARRAY_SIZE(rddpm) != res->dlen) {
		panel_err("invalid resource\n");
		return;
	}

	ret = resource_copy(rddpm, info->res);
	if (unlikely(ret < 0)) {
		panel_err("failed to copy rddpm resource\n");
		return;
	}

	panel_info("========== SHOW PANEL [0Ah:RDDPM] INFO (Before SLEEP_IN) ==========\n");
	panel_info("* Reg Value : 0x%02x, Result : %s\n",
			rddpm[0], ((rddpm[0] & 0x9C) == 0x98) ? "GOOD" : "NG");
	panel_info("* Bootster Mode : %s\n", rddpm[0] & 0x80 ? "ON (GD)" : "OFF (NG)");
	panel_info("* Idle Mode     : %s\n", rddpm[0] & 0x40 ? "ON (NG)" : "OFF (GD)");
	panel_info("* Partial Mode  : %s\n", rddpm[0] & 0x20 ? "ON" : "OFF");
	panel_info("* Sleep Mode    : %s\n", rddpm[0] & 0x10 ? "OUT (GD)" : "IN (NG)");
	panel_info("* Normal Mode   : %s\n", rddpm[0] & 0x08 ? "OK (GD)" : "SLEEP (NG)");
	panel_info("* Display ON    : %s\n", rddpm[0] & 0x04 ? "ON (NG)" : "OFF (GD)");
	panel_info("=================================================\n");

}

static void show_rddsm(struct dumpinfo *info)
{
	int ret;
	struct resinfo *res = info->res;
	u8 rddsm[S6E3HAD_RDDSM_LEN] = { 0, };
#ifdef CONFIG_LOGGING_BIGDATA_BUG
	extern unsigned int g_rddsm;
#endif

	if (!res || ARRAY_SIZE(rddsm) != res->dlen) {
		panel_err("invalid resource\n");
		return;
	}

	ret = resource_copy(rddsm, info->res);
	if (unlikely(ret < 0)) {
		panel_err("failed to copy rddsm resource\n");
		return;
	}

	panel_info("========== SHOW PANEL [0Eh:RDDSM] INFO ==========\n");
	panel_info("* Reg Value : 0x%02x, Result : %s\n",
			rddsm[0], (rddsm[0] == 0x80) ? "GOOD" : "NG");
	panel_info("* TE Mode : %s\n", rddsm[0] & 0x80 ? "ON(GD)" : "OFF(NG)");
	panel_info("=================================================\n");
#ifdef CONFIG_LOGGING_BIGDATA_BUG
	g_rddsm = (unsigned int)rddsm[0];
#endif
}

static void show_err(struct dumpinfo *info)
{
	int ret;
	struct resinfo *res = info->res;
	u8 err[S6E3HAD_ERR_LEN] = { 0, }, err_15_8, err_7_0;

	if (!res || ARRAY_SIZE(err) != res->dlen) {
		panel_err("invalid resource\n");
		return;
	}

	ret = resource_copy(err, info->res);
	if (unlikely(ret < 0)) {
		panel_err("failed to copy err resource\n");
		return;
	}

	err_15_8 = err[0];
	err_7_0 = err[1];

	panel_info("========== SHOW PANEL [E9h:DSI-ERR] INFO ==========\n");
	panel_info("* Reg Value : 0x%02x%02x, Result : %s\n", err_15_8, err_7_0,
			(err[0] || err[1] || err[2] || err[3] || err[4]) ? "NG" : "GOOD");

	if (err_15_8 & 0x80)
		panel_info("* DSI Protocol Violation\n");

	if (err_15_8 & 0x40)
		panel_info("* Data P Lane Contention Detetion\n");

	if (err_15_8 & 0x20)
		panel_info("* Invalid Transmission Length\n");

	if (err_15_8 & 0x10)
		panel_info("* DSI VC ID Invalid\n");

	if (err_15_8 & 0x08)
		panel_info("* DSI Data Type Not Recognized\n");

	if (err_15_8 & 0x04)
		panel_info("* Checksum Error\n");

	if (err_15_8 & 0x02)
		panel_info("* ECC Error, multi-bit (detected, not corrected)\n");

	if (err_15_8 & 0x01)
		panel_info("* ECC Error, single-bit (detected and corrected)\n");

	if (err_7_0 & 0x80)
		panel_info("* Data Lane Contention Detection\n");

	if (err_7_0 & 0x40)
		panel_info("* False Control Error\n");

	if (err_7_0 & 0x20)
		panel_info("* HS RX Timeout\n");

	if (err_7_0 & 0x10)
		panel_info("* Low-Power Transmit Sync Error\n");

	if (err_7_0 & 0x08)
		panel_info("* Escape Mode Entry Command Error");

	if (err_7_0 & 0x04)
		panel_info("* EoT Sync Error\n");

	if (err_7_0 & 0x02)
		panel_info("* SoT Sync Error\n");

	if (err_7_0 & 0x01)
		panel_info("* SoT Error\n");

	if (err[2] != 0)
		panel_info("* CRC Error Count : %d\n", err[2]);

	if (err[3] != 0)
		panel_info("* ECC1 Error Count : %d\n", err[3]);

	if (err[4] != 0)
		panel_info("* ECC2 Error Count : %d\n", err[4]);

	panel_info("==================================================\n");
}

static void show_err_fg(struct dumpinfo *info)
{
	int ret;
	u8 err_fg[S6E3HAD_ERR_FG_LEN] = { 0, };
	struct resinfo *res = info->res;

	if (!res || ARRAY_SIZE(err_fg) != res->dlen) {
		panel_err("invalid resource\n");
		return;
	}

	ret = resource_copy(err_fg, res);
	if (unlikely(ret < 0)) {
		panel_err("failed to copy err_fg resource\n");
		return;
	}

	panel_info("========== SHOW PANEL [EEh:ERR_FG] INFO ==========\n");
	panel_info("* Reg Value : 0x%02x, Result : %s\n",
			err_fg[0], (err_fg[0] & 0x4C) ? "NG" : "GOOD");

	if (err_fg[0] & 0x04) {
		panel_info("* VLOUT3 Error\n");
		inc_dpui_u32_field(DPUI_KEY_PNVLO3E, 1);
	}

	if (err_fg[0] & 0x08) {
		panel_info("* ELVDD Error\n");
		inc_dpui_u32_field(DPUI_KEY_PNELVDE, 1);
	}

	if (err_fg[0] & 0x40) {
		panel_info("* VLIN1 Error\n");
		inc_dpui_u32_field(DPUI_KEY_PNVLI1E, 1);
	}

	panel_info("==================================================\n");
}

static void show_dsi_err(struct dumpinfo *info)
{
	int ret;
	struct resinfo *res = info->res;
	u8 dsi_err[S6E3HAD_DSI_ERR_LEN] = { 0, };

	if (!res || ARRAY_SIZE(dsi_err) != res->dlen) {
		panel_err("invalid resource\n");
		return;
	}

	ret = resource_copy(dsi_err, res);
	if (unlikely(ret < 0)) {
		panel_err("failed to copy dsi_err resource\n");
		return;
	}

	panel_info("========== SHOW PANEL [05h:DSIE_CNT] INFO ==========\n");
	panel_info("* Reg Value : 0x%02x, Result : %s\n",
			dsi_err[0], (dsi_err[0]) ? "NG" : "GOOD");
	if (dsi_err[0])
		panel_info("* DSI Error Count : %d\n", dsi_err[0]);
	panel_info("====================================================\n");

	inc_dpui_u32_field(DPUI_KEY_PNDSIE, dsi_err[0]);
#if IS_ENABLED(CONFIG_SEC_ABC)
	if (dsi_err[0] > 0)
		sec_abc_send_event("MODULE=display@ERROR=act_section_panel_main_dsi_error");
#endif
}

static void show_self_diag(struct dumpinfo *info)
{
	int ret;
	struct resinfo *res = info->res;
	u8 self_diag[S6E3HAD_SELF_DIAG_LEN] = { 0, };

	ret = resource_copy(self_diag, res);
	if (unlikely(ret < 0)) {
		panel_err("failed to copy self_diag resource\n");
		return;
	}

	panel_info("========== SHOW PANEL [0Fh:SELF_DIAG] INFO ==========\n");
	panel_info("* Reg Value : 0x%02x, Result : %s\n",
			self_diag[0], (self_diag[0] & 0x40) ? "GOOD" : "NG");
	if ((self_diag[0] & 0x80) == 0)
		panel_info("* OTP value is changed\n");
	if ((self_diag[0] & 0x40) == 0)
		panel_info("* Panel Boosting Error\n");

	panel_info("=====================================================\n");

	inc_dpui_u32_field(DPUI_KEY_PNSDRE, (self_diag[0] & 0x40) ? 0 : 1);
}

#ifdef CONFIG_SUPPORT_DDI_CMDLOG
static void show_cmdlog(struct dumpinfo *info)
{
	int ret;
	struct resinfo *res = info->res;
	u8 cmdlog[S6E3HAD_CMDLOG_LEN];

	memset(cmdlog, 0, sizeof(cmdlog));
	ret = resource_copy(cmdlog, res);
	if (unlikely(ret < 0)) {
		panel_err("failed to copy cmdlog resource\n");
		return;
	}

	panel_info("dump:cmdlog\n");
	print_data(cmdlog, ARRAY_SIZE(cmdlog));
}
#endif

#ifdef CONFIG_SUPPORT_MAFPC

#define S6E3HAD_MAFPC_ENA_REG	0
#define S6E3HAD_MAFPC_VBP_REG 	2

static void copy_mafpc_enable_maptbl(struct maptbl *tbl, u8 *dst)
{
	struct panel_device *panel = tbl->pdata;
	struct mafpc_device *mafpc = NULL;
	struct panel_info *panel_data;

	if (panel == NULL)
		goto err_enable;

	panel_data = &panel->panel_data;
	if (panel_data == NULL) {
		panel_err("invalid panel data\n");
		goto err_enable;
	}

	if (panel->mafpc_sd == NULL) {
		panel_err("mafpc_sd is null\n");
		goto err_enable;
	}

	v4l2_subdev_call(panel->mafpc_sd, core, ioctl,
		V4L2_IOCTL_MAFPC_GET_INFO, NULL);

	mafpc = (struct mafpc_device *)v4l2_get_subdev_hostdata(panel->mafpc_sd);
	if (mafpc == NULL) {
		panel_err("failed to get mafpc info\n");
		goto err_enable;
	}

	panel_info("MCD:ABC:enabled: %x, written: %x, id[2]: %x\n",
		mafpc->enable, mafpc->written, panel_data->id[2]);

	if (!mafpc->enable) {
		dst[S6E3HAD_MAFPC_ENA_REG] = 0;
		goto err_enable;
	}

	if ((mafpc->written & MAFPC_UPDATED_FROM_SVC) &&
		(mafpc->written & MAFPC_UPDATED_TO_DEV)) {

		dst[S6E3HAD_MAFPC_ENA_REG] = S6E3HAD_MAFPC_ENABLE;

		/* default 0x20, refer abc op guide */
		dst[S6E3HAD_MAFPC_VBP_REG] = 0x20;
		if (panel_data->id[2] < 0xE2)
			dst[S6E3HAD_MAFPC_VBP_REG] = 0x1F;

		if (mafpc->written & MAFPC_UPDATED_FROM_SVC)
			memcpy(&dst[S6E3HAD_MAFPC_CTRL_CMD_OFFSET], mafpc->ctrl_cmd, mafpc->ctrl_cmd_len);
	}
err_enable:
	print_hex_dump(KERN_ERR, "", DUMP_PREFIX_ADDRESS, 32, 4,
			dst, S6E3HAD_MAFPC_CTRL_CMD_OFFSET + mafpc->ctrl_cmd_len, false);
	return;
}


static void copy_mafpc_ctrl_maptbl(struct maptbl *tbl, u8 *dst)
{
	struct panel_device *panel = tbl->pdata;
	struct mafpc_device *mafpc = NULL;

	if (panel->mafpc_sd == NULL) {
		panel_err("mafpc_sd is null\n");
		goto err_enable;
	}

	v4l2_subdev_call(panel->mafpc_sd, core, ioctl, V4L2_IOCTL_MAFPC_GET_INFO, NULL);

	mafpc = (struct mafpc_device *)v4l2_get_subdev_hostdata(panel->mafpc_sd);
	if (mafpc == NULL) {
		panel_err("failed to get mafpc info\n");
		goto err_enable;
	}

	if (mafpc->enable) {
		if (mafpc->written)
			memcpy(dst, mafpc->ctrl_cmd, mafpc->ctrl_cmd_len);

		print_hex_dump(KERN_INFO, "", DUMP_PREFIX_ADDRESS, 32, 4, dst, mafpc->ctrl_cmd_len, false);
	}

err_enable:
	return;
}


#define S6E3HAD_MAFPC_SCALE_MAX	75


static int get_mafpc_scale_index(struct mafpc_device *mafpc, struct panel_device *panel)
{
	int ret = 0;
	int br_index, index = 0;
	struct panel_bl_device *panel_bl;

	panel_bl = &panel->panel_bl;
	if (!panel_bl) {
		panel_err("panel_bl is null\n");
		goto err_get_scale;
	}

	if (!mafpc->scale_buf || !mafpc->scale_map_br_tbl)  {
		panel_err("mafpc img buf is null\n");
		goto err_get_scale;
	}

	br_index = panel_bl->props.brightness;
	if (br_index >= mafpc->scale_map_br_tbl_len)
		br_index = mafpc->scale_map_br_tbl_len - 1;

	index = mafpc->scale_map_br_tbl[br_index];
	if (index < 0) {
		panel_err("mfapc invalid scale index : %d\n", br_index);
		goto err_get_scale;
	}
	return index;

err_get_scale:
	return ret;
}

static void copy_mafpc_scale_maptbl(struct maptbl *tbl, u8 *dst)
{
	int row = 0;
	int index = 0;

	struct mafpc_device *mafpc = NULL;
	struct panel_device *panel = (struct panel_device *)tbl->pdata;

	if (panel == NULL) {
		panel_err("panel is null\n");
		goto err_scale;
	}

	v4l2_subdev_call(panel->mafpc_sd, core, ioctl,
		V4L2_IOCTL_MAFPC_GET_INFO, NULL);
	mafpc  = (struct mafpc_device *)v4l2_get_subdev_hostdata(panel->mafpc_sd);
	if (mafpc == NULL) {
		panel_err("failed to get mafpc info\n");
		goto err_scale;
	}

	if (!mafpc->scale_buf || !mafpc->scale_map_br_tbl)  {
		panel_err("mafpc img buf is null\n");
		goto err_scale;
	}

	index = get_mafpc_scale_index(mafpc, panel);
	if (index < 0) {
		panel_err("mfapc invalid scale index : %d\n", index);
		goto err_scale;
	}

	if (index >= S6E3HAD_MAFPC_SCALE_MAX)
		index = S6E3HAD_MAFPC_SCALE_MAX - 1;

	row = index * 3;
	memcpy(dst, mafpc->scale_buf + row, 3);

	panel_info("idx: %d, %x:%x:%x\n",
			index, dst[0], dst[1], dst[2]);

err_scale:
	return;
}


static void show_mafpc_log(struct dumpinfo *info)
{
	int ret;
	struct resinfo *res = info->res;
	u8 mafpc[S6E3HAD_MAFPC_LEN] = {0, };

	if (!res || ARRAY_SIZE(mafpc) != res->dlen) {
		panel_err("invalid resource\n");
		return;
	}

	ret = resource_copy(mafpc, info->res);
	if (unlikely(ret < 0)) {
		panel_err("failed to copy rddpm resource\n");
		return;
	}

	panel_info("========== SHOW PANEL [87h:MAFPC_EN] INFO ==========\n");
	panel_info("* Reg Value : 0x%02x, Result : %s\n",
			mafpc[0], (mafpc[0] & 0x01) ? "ON" : "OFF");
	panel_info("====================================================\n");

}
static void show_mafpc_flash_log(struct dumpinfo *info)
{
	int ret;
	struct resinfo *res = info->res;
	u8 mafpc_flash[S6E3HAD_MAFPC_FLASH_LEN] = {0, };

	if (!res || ARRAY_SIZE(mafpc_flash) != res->dlen) {
		panel_err("invalid resource\n");
		return;
	}

	ret = resource_copy(mafpc_flash, info->res);
	if (unlikely(ret < 0)) {
		panel_err("failed to copy rddpm resource\n");
		return;
	}

	panel_info("======= SHOW PANEL [FEh(0x09):MAFPC_FLASH] INFO =======\n");
	panel_info("* Reg Value : 0x%02x, Result : %s\n",
			mafpc_flash[0], (mafpc_flash[0] & 0x02) ? "BYPASS" : "POC");
	panel_info("====================================================\n");
}
#endif

static void show_self_mask_crc(struct dumpinfo *info)
{
	int ret;
	struct resinfo *res = info->res;
	u8 crc[S6E3HAD_SELF_MASK_CRC_LEN] = {0, };

	if (!res || ARRAY_SIZE(crc) != res->dlen) {
		panel_err("invalid resource\n");
		return;
	}

	ret = resource_copy(crc, info->res);
	if (unlikely(ret < 0)) {
		panel_err("failed to self mask crc resource\n");
		return;
	}

	panel_info("======= SHOW PANEL [7Fh:SELF_MASK_CRC] INFO =======\n");
	panel_info("* Reg Value : 0x%02x, 0x%02x, 0x%02x, 0x%02x\n",
			crc[0], crc[1], crc[2], crc[3]);
	panel_info("====================================================\n");
}

static int init_gamma_mode2_brt_table(struct maptbl *tbl)
{
	struct panel_info *panel_data;
	struct panel_device *panel;
	struct panel_dimming_info *panel_dim_info;
	//todo:remove
	panel_info("++\n");
	if (tbl == NULL) {
		panel_err("maptbl is null\n");
		return -EINVAL;
	}

	if (tbl->pdata == NULL) {
		panel_err("pdata is null\n");
		return -EINVAL;
	}

	panel = tbl->pdata;
	panel_data = &panel->panel_data;

	panel_dim_info = panel_data->panel_dim_info[PANEL_BL_SUBDEV_TYPE_DISP];

	if (panel_dim_info == NULL) {
		panel_err("panel_dim_info is null\n");
		return -EINVAL;
	}

	if (panel_dim_info->brt_tbl == NULL) {
		panel_err("panel_dim_info->brt_tbl is null\n");
		return -EINVAL;
	}

	generate_brt_step_table(panel_dim_info->brt_tbl);

	/* initialize brightness_table */
	memcpy(&panel->panel_bl.subdev[PANEL_BL_SUBDEV_TYPE_DISP].brt_tbl,
			panel_dim_info->brt_tbl, sizeof(struct brightness_table));

	return 0;
}

static int getidx_gamma_mode2_brt_table(struct maptbl *tbl)
{
	int row = 0;
	struct panel_info *panel_data;
	struct panel_bl_device *panel_bl;
	struct panel_device *panel = (struct panel_device *)tbl->pdata;

	if (panel == NULL) {
		panel_err("panel is null\n");
		return -EINVAL;
	}
	panel_bl = &panel->panel_bl;
	panel_data = &panel->panel_data;

	row = get_brightness_pac_step_by_subdev_id(
			panel_bl, PANEL_BL_SUBDEV_TYPE_DISP, panel_bl->props.brightness);

	return maptbl_index(tbl, 0, row, 0);
}

static int do_gamma_flash_checksum(struct panel_device *panel, void *data, u32 len)
{
	int ret = 0;
	struct dim_flash_result *result;
	struct panel_info *panel_data;
	int state, index;
	int res_size;
	char read_buf[16] = { 0, };

	result = (struct dim_flash_result *) data;

	if (!result)
		return -ENODATA;

	if (atomic_cmpxchg(&result->running, 0, 1) != 0) {
		panel_info("already running\n");
		return -EBUSY;
	}

	panel_data = &panel->panel_data;

	result->state = state = GAMMA_FLASH_PROGRESS;
	result->exist = 0;

	memset(result->result, 0, ARRAY_SIZE(result->result));
	do {
		ret = panel_resource_update_by_name(panel, "flash_loaded");
		if (unlikely(ret < 0)) {
			panel_err("resource init failed\n");
			state = GAMMA_FLASH_ERROR_READ_FAIL;
			break;
		}

		res_size = get_resource_size_by_name(panel_data, "flash_loaded");
		if (unlikely(res_size < 0)) {
			panel_err("cannot found flash_loaded in panel resource\n");
			state = GAMMA_FLASH_ERROR_NOT_EXIST;
			break;
		}

		result->exist = 1;
		if (res_size > sizeof(read_buf)) {
			panel_err("flash_loaded resource size is too long %d\n", res_size);
			state = GAMMA_FLASH_ERROR_READ_FAIL;
			break;
		}

		ret = resource_copy_by_name(panel_data, read_buf, "flash_loaded");
		if (unlikely(ret < 0)) {
			panel_err("flash_loaded copy failed\n");
			state = GAMMA_FLASH_ERROR_READ_FAIL;
			break;
		}

		for (index = 0; index < res_size; index++) {
			if (read_buf[index] != 0x00) {
				state = GAMMA_FLASH_ERROR_CHECKSUM_MISMATCH;
				break;
			}
		}
	} while (0);

	if (state == GAMMA_FLASH_PROGRESS)
		state = GAMMA_FLASH_SUCCESS;

	snprintf(result->result, ARRAY_SIZE(result->result), "1\n%d %02X%02X%02X%02X",
		state, read_buf[0], read_buf[1], read_buf[2], read_buf[3]);

	result->state = state;

	atomic_xchg(&result->running, 0);

	return ret;
}

static bool is_panel_state_not_lpm(struct panel_device *panel)
{
	if (panel->state.cur_state != PANEL_STATE_ALPM)
		return true;

	return false;
}

static bool is_panel_mres_updated(struct panel_device *panel)
{
	if (panel->panel_data.props.mres_updated)
		return true;

	return false;
}

static bool is_panel_mres_updated_bigger(struct panel_device *panel)
{
	struct panel_mres *mres = &panel->panel_data.mres;
	struct panel_properties *props = &panel->panel_data.props;

	panel_info("updated:%d %dx%d -> %dx%d\n",
			props->mres_updated,
			mres->resol[props->old_mres_mode].w,
			mres->resol[props->old_mres_mode].h,
			mres->resol[props->mres_mode].w,
			mres->resol[props->mres_mode].h);

	return is_panel_mres_updated(panel) &&
			((mres->resol[props->old_mres_mode].w *
			  mres->resol[props->old_mres_mode].h) <
			 (mres->resol[props->mres_mode].w *
			  mres->resol[props->mres_mode].h));
}

static inline bool is_id_gte_e2(struct panel_device *panel)
{
	return ((panel->panel_data.id[2] & 0xFF) >= 0xE2) ? true : false;
}

static inline bool is_id_gte_e3(struct panel_device *panel)
{
	return ((panel->panel_data.id[2] & 0xFF) >= 0xE3) ? true : false;
}

static inline bool is_id_gte_e4(struct panel_device *panel)
{
	return ((panel->panel_data.id[2] & 0xFF) >= 0xE4) ? true : false;
}

static inline bool is_id_gte_e5(struct panel_device *panel)
{
	return ((panel->panel_data.id[2] & 0xFF) >= 0xE5) ? true : false;
}

static inline bool is_id_gte_e7(struct panel_device *panel)
{
	return ((panel->panel_data.id[2] & 0xFF) >= 0xE7) ? true : false;
}

static bool is_gamma_select_off_brt(struct panel_device *panel)
{
	struct panel_bl_device *panel_bl = &panel->panel_bl;

	if (panel_bl->props.brightness >= 256 &&
		panel_bl->props.brightness <= 260) {
		return true;
	}
	return false;
}

static bool is_first_set_bl(struct panel_device *panel)
{
	struct panel_bl_device *panel_bl = &panel->panel_bl;
	int cnt;

	cnt = panel_bl_get_brightness_set_count(panel_bl);
	if (cnt == 0) {
		panel_info("true %d\n", cnt);
		return true;
	}
	return false;
}

static bool is_wait_vsync_needed(struct panel_device *panel)
{
	struct panel_properties *props = &panel->panel_data.props;

	if (props->vrr_origin_mode != props->vrr_mode) {
		panel_info("true(vrr mode changed)\n");
		return true;
	}

	if (!!(props->vrr_origin_fps % 48) != !!(props->vrr_fps % 48)) {
		panel_info("true(adaptive vrr changed)\n");
		return true;
	}

	return false;
}



static bool is_vrr_96hs_mode(struct panel_device *panel)
{
	struct panel_info *panel_data;
	struct panel_vrr *vrr;
	int ret;

	panel_data = &panel->panel_data;
	if (unlikely(!(panel_data->maptbl[GAMMA_WRITE_0_MAPTBL].initialized) ||
		!(panel_data->maptbl[GAMMA_WRITE_0_MAPTBL].initialized))) {
		return false;
	}

	vrr = get_panel_vrr(panel);
	if (vrr == NULL)
		return false;

	ret = get_s6e3had_96hs_mode(find_s6e3had_vrr(vrr));

	return (ret == S6E3HAD_96HS_MODE_ON ? true : false);
}

static bool is_vrr_96hs_hbm_enter(struct panel_device *panel)
{
	int dir;

	if (is_vrr_96hs_mode(panel)) {
		dir = get_s6e3had_brt_direction(panel);
		if (dir > 0) {
			panel_dbg("true %d\n", dir);
			return true;
		}
	}
	return false;
}

static int __init s6e3had_panel_init(void)
{
	register_common_panel(&s6e3had_unbound3_a3_s0_panel_info);

	return 0;
}

static void __exit s6e3had_panel_exit(void)
{
	deregister_common_panel(&s6e3had_unbound3_a3_s0_panel_info);
}

module_init(s6e3had_panel_init)
module_exit(s6e3had_panel_exit)
MODULE_DESCRIPTION("Samsung Mobile Panel Driver");
MODULE_LICENSE("GPL");
