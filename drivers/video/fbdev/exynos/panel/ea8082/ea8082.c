/*
 * linux/drivers/video/fbdev/exynos/panel/ea8082/ea8082.c
 *
 * EA8082 Dimming Driver
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
#include "ea8082.h"
#include "ea8082_panel.h"
#ifdef CONFIG_PANEL_AID_DIMMING
#include "../dimming.h"
#include "../panel_dimming.h"
#endif
#include "../panel_drv.h"
#include "../panel_debug.h"

#ifdef CONFIG_DYNAMIC_MIPI
#include "../dynamic_mipi/band_info.h"
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
		panel_info("FFC[%d]: %x\n", i, dst[i]);
}

#endif


#ifdef CONFIG_PANEL_AID_DIMMING

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
	for(i = 0; i < brt_tbl->sz_step_cnt; i++)
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
				if (brt_tbl->brt[brt_tbl->sz_brt - 1] < brt_tbl->brt_to_step[i]) {

					brt_tbl->brt_to_step[i] = disp_pow_round(brt_tbl->brt_to_step[i], 2);
				}
				if (i >= brt_tbl->sz_brt_to_step) {
					panel_err("step cnt over %d %d\n", i, brt_tbl->sz_brt_to_step);
					break;
				}
			}
		}
	}
	return ret;
}

#endif /* CONFIG_PANEL_AID_DIMMING */

static int find_ea8082_vrr(struct panel_vrr *vrr)
{
	size_t i;

	if (!vrr) {
		panel_err("panel_vrr is null\n");
		return -EINVAL;
	}

	for (i = 0; i < ARRAY_SIZE(EA8082_VRR_FPS); i++) {
		if (vrr->fps == EA8082_VRR_FPS[i][EA8082_VRR_KEY_REFRESH_RATE] &&
				vrr->mode == EA8082_VRR_FPS[i][EA8082_VRR_KEY_REFRESH_MODE] &&
				vrr->te_sw_skip_count == EA8082_VRR_FPS[i][EA8082_VRR_KEY_TE_SW_SKIP_COUNT] &&
				vrr->te_hw_skip_count == EA8082_VRR_FPS[i][EA8082_VRR_KEY_TE_HW_SKIP_COUNT])
			return (int)i;
	}

	return -EINVAL;
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

static int getidx_common_maptbl(struct maptbl *tbl)
{
	return 0;
}

#ifdef CONFIG_EXYNOS_DOZE
#ifdef CONFIG_SUPPORT_AOD_BL
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
#endif

static int getidx_hbm_and_transition_table(struct maptbl *tbl)
{
	int layer, row;
	struct panel_info *panel_data;
	struct panel_bl_device *panel_bl;
	struct panel_device *panel = (struct panel_device *)tbl->pdata;

	if (panel == NULL) {
		panel_err("panel is null\n");
		return -EINVAL;
	}

	panel_data = &panel->panel_data;
	panel_bl = &panel->panel_bl;

	layer = is_hbm_brightness(panel_bl, panel_bl->props.brightness);
	row = panel_bl->props.smooth_transition;

	return maptbl_index(tbl, layer, row, 0);
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

static int getidx_vrr_table(struct maptbl *tbl)
{
	struct panel_device *panel = (struct panel_device *)tbl->pdata;
	struct panel_vrr *vrr;
	int row = 0, layer = 0, index;

	vrr = get_panel_vrr(panel);
	if (vrr == NULL) {
		panel_err("failed to get vrr\n");
		return -EINVAL;
	}

	index = find_ea8082_vrr(vrr);
	if (index < 0) {
		panel_warn("vrr not found\n");
		row = 0;
	} else {
		row = index;
	}

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

#if defined(CONFIG_SEC_FACTORY) && defined(CONFIG_SUPPORT_FAST_DISCHARGE)
static int getidx_fast_discharge_table(struct maptbl *tbl)
{
	struct panel_device *panel = (struct panel_device *)tbl->pdata;
	struct panel_info *panel_data;
	int row = 0;

	if (panel == NULL) {
		panel_err("panel is null\n");
		return -EINVAL;
	}

	panel_data = &panel->panel_data;

	row = ((panel_data->props.enable_fd) ? 1 : 0);
	panel_info("fast_discharge %d\n", row);

	return maptbl_index(tbl, 0, row, 0);
}

#endif

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

	if (EA8082_SCR_CR_OFS + mdnie->props.sz_scr > sizeof_maptbl(tbl)) {
		panel_err("invalid size (maptbl_size %d, sz_scr %d)\n",
				sizeof_maptbl(tbl), mdnie->props.sz_scr);
		return -EINVAL;
	}

	memcpy(&tbl->arr[EA8082_SCR_CR_OFS],
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

static int getidx_mdnie_night_mode_maptbl(struct maptbl *tbl)
{
	int mode = 0;
	struct mdnie_info *mdnie = (struct mdnie_info *)tbl->pdata;

	if(mdnie->props.mode != AUTO) mode = 1;

	return maptbl_index(tbl, mode , mdnie->props.night_level, 0);
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

	if (sizeof_maptbl(tbl) < (EA8082_NIGHT_MODE_OFS +
			sizeof_row(night_maptbl))) {
		panel_err("invalid size (maptbl_size %d, night_maptbl_size %d)\n",
				sizeof_maptbl(tbl), sizeof_row(night_maptbl));
		return -EINVAL;
	}

	maptbl_copy(night_maptbl, &tbl->arr[EA8082_NIGHT_MODE_OFS]);

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

	if (sizeof_maptbl(tbl) < (EA8082_COLOR_LENS_OFS +
			sizeof_row(color_lens_maptbl))) {
		panel_err("invalid size (maptbl_size %d, color_lens_maptbl_size %d)\n",
				sizeof_maptbl(tbl), sizeof_row(color_lens_maptbl));
		return -EINVAL;
	}

	if (IS_COLOR_LENS_MODE(mdnie))
		maptbl_copy(color_lens_maptbl, &tbl->arr[EA8082_COLOR_LENS_OFS]);

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
	mdnie->props.cur_wrgb[1] = *(dst + 1);
	mdnie->props.cur_wrgb[2] = *(dst + 2);
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
		dst[i] = value;
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
		dst[i] = tbl->arr[idx + i];
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
		dst[i] = value;
		panel_dbg("cur_wrgb[%d] %d(%02X) (orig:0x%02X offset:%d)\n",
				i, mdnie->props.cur_wrgb[i], mdnie->props.cur_wrgb[i],
				tbl->arr[idx + i], mdnie->props.def_wrgb_ofs[i]);
	}
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

static int getidx_mdnie_3_maptbl(struct pkt_update_info *pktui)
{
	return getidx_mdnie_maptbl(pktui, 3);
}

static int getidx_mdnie_4_maptbl(struct pkt_update_info *pktui)
{
	return getidx_mdnie_maptbl(pktui, 4);
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

static void copy_dummy_maptbl(struct maptbl *tbl, u8 *dst)
{
	return;
}

#endif /* CONFIG_EXYNOS_DECON_MDNIE_LITE */


static void copy_common_maptbl(struct maptbl *tbl, u8 *dst)
{
	int idx;

	if (!tbl || !dst) {
		panel_err("invalid parameter (tbl %pK, dst %pK)\n",
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

#ifdef CONFIG_SUPPORT_GRAYSPOT_TEST
static void copy_grayspot_cal_maptbl(struct maptbl *tbl, u8 *dst)
{
	struct panel_device *panel;
	struct panel_info *panel_data;
	int ret = 0;

	if (!tbl || !dst)
		return;

	panel = (struct panel_device *)tbl->pdata;
	if (unlikely(!panel))
		return;

	panel_data = &panel->panel_data;

	ret = resource_copy_by_name(panel_data, dst, "grayspot_cal");
	if (unlikely(ret)) {
		panel_err("grayspot_cal not found in panel resource\n");
		return;
	}

	if (panel_data->props.grayspot) {
		dst[0] = GRAYSPOT_START_SWIRE;
		dst[3] = GRAYSPOT_CAL_OFFSET;
	}
	panel_info("grayspot_cal 0x%02x, 0x%02x, 0x%02x, 0x%02x,\n",
		dst[0], dst[1], dst[2], dst[3]);
}
#endif

static void show_rddpm(struct dumpinfo *info)
{
	int ret;
	struct resinfo *res = info->res;
	u8 rddpm[EA8082_RDDPM_LEN] = { 0, };
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

	panel_info("========== SHOW PANEL [0Ah:RDDPM] INFO (After DISPLAY_ON) ==========\n");
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
	u8 rddpm[EA8082_RDDPM_LEN] = { 0, };

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
	u8 rddsm[EA8082_RDDSM_LEN] = { 0, };
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

static void show_dsi_err(struct dumpinfo *info)
{
	int ret;
	struct resinfo *res = info->res;
	u8 dsi_err[EA8082_DSI_ERR_LEN] = { 0, };

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
//Todo
#ifdef CONFIG_DISPLAY_USE_INFO
	inc_dpui_u32_field(DPUI_KEY_PNDSIE, dsi_err[0]);
#endif
}

static void show_self_diag(struct dumpinfo *info)
{
	int ret;
	struct resinfo *res = info->res;
	u8 self_diag[EA8082_SELF_DIAG_LEN] = { 0, };

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
//Todo
#ifdef CONFIG_DISPLAY_USE_INFO
	inc_dpui_u32_field(DPUI_KEY_PNSDRE, (self_diag[0] & 0x40) ? 0 : 1);
#endif
}

#ifdef CONFIG_SUPPORT_DDI_CMDLOG
static void show_cmdlog(struct dumpinfo *info)
{
	int ret;
	struct resinfo *res = info->res;
	u8 cmdlog[EA8082_CMDLOG_LEN];

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

	row = get_brightness_pac_step_by_subdev_id(panel_bl, PANEL_BL_SUBDEV_TYPE_DISP, panel_bl->props.brightness);

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

static bool is_panel_state_not_lpm(struct panel_device *panel)
{
	if (panel->state.cur_state != PANEL_STATE_ALPM)
		return true;

	return false;
}

static inline bool is_id_gte_03(struct panel_device *panel)
{
	return ((panel->panel_data.id[2] & 0xFF) >= 0x03) ? true : false;
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

static int __init ea8082_panel_init(void)
{
	register_common_panel(&ea8082_r9_a2_s0_panel_info);
	return 0;
}

static void __exit ea8082_panel_exit(void)
{
	deregister_common_panel(&ea8082_r9_a2_s0_panel_info);
}

module_init(ea8082_panel_init)
module_exit(ea8082_panel_exit)
MODULE_DESCRIPTION("Samsung Mobile Panel Driver");
MODULE_LICENSE("GPL");