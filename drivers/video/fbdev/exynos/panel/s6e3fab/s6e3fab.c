/*
 * linux/drivers/video/fbdev/exynos/panel/s6e3fab/s6e3fab.c
 *
 * S6E3FAB Dimming Driver
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
#include "s6e3fab.h"
#include "s6e3fab_panel.h"
#ifdef CONFIG_PANEL_AID_DIMMING
#include "../dimming.h"
#include "../panel_dimming.h"
#endif
#include "../panel_drv.h"
#include "../panel_debug.h"

#ifdef CONFIG_DYNAMIC_MIPI
#include "../dynamic_mipi/band_info.h"
#endif

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
		panel_info("FFC[%d]: %x\n", i, dst[i]);
}


void dynamic_mipi_set_ddi_osc(struct maptbl *tbl, u8 *dst)
{
	struct panel_device *panel;
	struct dm_status_info *dm_status;

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

	if (dm_status->current_ddi_osc != dm_status->request_ddi_osc) {
		panel_info("MCD:DM:%s: osc was changed %d -> %d\n", __func__,
			dm_status->current_ddi_osc, dm_status->request_ddi_osc);

		switch (dm_status->request_ddi_osc) {
		case 0: /* default */
			dst[1] = MCD_DM_DDI_OSC_96_5;
			break;
		case 1:
			dst[1] = MCD_DM_DDI_OSC_94_5;
			break;
		default:
			panel_err("MCD:DM: invalid request ddi osc: %d, set default\n",
				dm_status->request_ddi_osc);
			goto exit_copy;
		}
		dm_status->current_ddi_osc = dm_status->request_ddi_osc;
	}

	if (dm_status->current_ddi_osc == 1)
		dst[1] = MCD_DM_DDI_OSC_94_5;
	else
		dst[1] = MCD_DM_DDI_OSC_96_5;

	panel_info("MCD:DM:%s: dst[0]: %x, dst[1]:%x\n", __func__, dst[0], dst[1]);

exit_copy:

	return;
}

static int get_current_ddi_osc_index(struct panel_device *panel)
{
	struct dm_status_info *dm_status = &panel->dynamic_mipi.dm_status;

	return dm_status->current_ddi_osc;
}
#else
static int get_current_ddi_osc_index(struct panel_device *panel)
{
	return 0;
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

static int find_s6e3fab_vrr(struct panel_vrr *vrr)
{
	size_t i;

	if (!vrr) {
		panel_err("panel_vrr is null\n");
		return -EINVAL;
	}

	for (i = 0; i < ARRAY_SIZE(S6E3FAB_VRR_FPS); i++) {
		if (vrr->fps == S6E3FAB_VRR_FPS[i][S6E3FAB_VRR_KEY_REFRESH_RATE] &&
				vrr->mode == S6E3FAB_VRR_FPS[i][S6E3FAB_VRR_KEY_REFRESH_MODE] &&
				vrr->te_sw_skip_count == S6E3FAB_VRR_FPS[i][S6E3FAB_VRR_KEY_TE_SW_SKIP_COUNT] &&
				vrr->te_hw_skip_count == S6E3FAB_VRR_FPS[i][S6E3FAB_VRR_KEY_TE_HW_SKIP_COUNT])
			return (int)i;
	}

	return -EINVAL;
}

static int getidx_s6e3fab_vrr_mode(int mode)
{
	return mode == VRR_HS_MODE ?
		S6E3FAB_VRR_MODE_HS : S6E3FAB_VRR_MODE_NS;
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

static int init_elvss_cal_offset_table(struct maptbl *tbl)
{
	struct panel_info *panel_data;
	struct panel_device *panel;
	int row, layer, box, ret;
	u8 elvss_cal_offset_otp_value = 0;

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

	ret = resource_copy_by_name(panel_data,
			&elvss_cal_offset_otp_value, "elvss_cal_offset");
	if (unlikely(ret)) {
		panel_err("elvss_cal_offset not found in panel resource\n");
		return -EINVAL;
	}

	for_each_box(tbl, box) {
		for_each_layer(tbl, layer) {
			for_each_row(tbl, row) {
				tbl->arr[maptbl_4d_index(tbl, box, layer, row, 0)] =
					elvss_cal_offset_otp_value;
			}
		}
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

static int getidx_acl_onoff_table(struct maptbl *tbl)
{
	struct panel_device *panel = (struct panel_device *)tbl->pdata;

	if (panel == NULL) {
		panel_err("panel is null\n");
		return -EINVAL;
	}

	return maptbl_index(tbl, 0, panel_bl_get_acl_pwrsave(&panel->panel_bl), 0);
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

static int getidx_vrr_aor_fq_brt_table(struct maptbl *tbl)
{
	struct panel_device *panel = (struct panel_device *)tbl->pdata;
	struct panel_vrr *vrr;
	struct panel_bl_device *panel_bl;
	int row = -1, layer = 0, index;

	panel_bl = &panel->panel_bl;
	vrr = get_panel_vrr(panel);
	if (vrr == NULL) {
		panel_err("failed to get vrr\n");
		return -EINVAL;
	}

	index = find_s6e3fab_vrr(vrr);
	if (index < 0) {
		panel_warn("vrr not found\n");
		layer = 0;
	} else {
		layer = index;
	}

	row = get_actual_brightness_index(panel_bl, panel_bl->props.brightness);

	if (row < 0 || tbl->nrow <= row) {
		panel_err("aor_fq_brt table is invalid, brightness %d row %d nrow %d\n",
			panel_bl->bd->props.brightness, row, tbl->nrow);
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

	index = find_s6e3fab_vrr(vrr);
	if (index < 0) {
		panel_warn("vrr not found\n");
		row = 0;
	} else {
		row = index;
	}

	return maptbl_index(tbl, layer, row, 0);
}

static int getidx_vrr_osc_mode_table(struct maptbl *tbl)
{
	struct panel_device *panel = (struct panel_device *)tbl->pdata;
	int row = 0, layer = 0;
	int vrr_mode;

	vrr_mode = get_panel_refresh_mode(panel);
	if (vrr_mode < 0)
		return -EINVAL;

	switch (vrr_mode) {
	case S6E3FAB_VRR_MODE_HS:
		if (get_current_ddi_osc_index(panel) == 1)
			row = S6E3FAB_VRR_MODE_HS_OSC1;
		else
			row = S6E3FAB_VRR_MODE_HS;
		break;
	case S6E3FAB_VRR_MODE_NS:
		if (get_current_ddi_osc_index(panel) == 1)
			row = S6E3FAB_VRR_MODE_NS_OSC1;
		else
			row = S6E3FAB_VRR_MODE_NS;
		break;
	default:
		panel_err("invalid vrr_mode: %d\n", vrr_mode);
		row = S6E3FAB_VRR_MODE_HS;
		break;
	}

	panel_dbg("vrr_mode:%d(%s:%s)\n", row,
			row & (0x1 << 1) ? "OSC1" : "Default OSC",
			row & (0x1 << 0) ? "HS" : "NS");

	return maptbl_index(tbl, layer, row, 0);
}

static int getidx_vrr_mode_table(struct maptbl *tbl)
{
	struct panel_device *panel = (struct panel_device *)tbl->pdata;
	int row = 0, layer = 0;
	int vrr_mode;

	vrr_mode = get_panel_refresh_mode(panel);
	if (vrr_mode < 0)
		return -EINVAL;

	row = getidx_s6e3fab_vrr_mode(vrr_mode);
	if (row < 0) {
		panel_err("failed to getidx_s6e3fab_vrr_mode(mode:%d)\n", vrr_mode);
		row = 0;
	}
	panel_dbg("vrr_mode:%d(%s)\n", row,
			row == S6E3FAB_VRR_MODE_HS ? "HS" : "NM");

	return maptbl_index(tbl, layer, row, 0);
}

static int getidx_gamma_brt_index_table(struct maptbl *tbl)
{
	struct panel_device *panel = (struct panel_device *)tbl->pdata;
	struct panel_bl_device *panel_bl;
	struct panel_dimming_info *panel_dim_info;
	struct panel_info *panel_data;
	struct panel_vrr *vrr;
	int *brt_range;
	int row = 0, vrr_fps_index = 0, brt_range_size;

	panel_data = &panel->panel_data;
	panel_dim_info = panel_data->panel_dim_info[PANEL_BL_SUBDEV_TYPE_DISP];

	if (unlikely(!panel_dim_info)) {
		panel_err("panel_bl-%d panel_dim_info is null\n",
				PANEL_BL_SUBDEV_TYPE_DISP);
		return -EINVAL;
	}

	if (panel_dim_info->nr_gm2_dim_init_info == 0) {
		panel_err("panel_bl-%d gamma mode 2 init info is null\n",
				PANEL_BL_SUBDEV_TYPE_DISP);
		return -EINVAL;
	}

	vrr = get_panel_vrr(panel);
	if (vrr == NULL) {
		panel_err("failed to get vrr\n");
		return -EINVAL;
	}

	vrr_fps_index = find_s6e3fab_vrr(vrr);
	if (vrr_fps_index < 0) {
		panel_warn("vrr not found\n");
		vrr_fps_index = 0;
	}

	brt_range_size = panel_dim_info->gm2_dim_init_info[vrr_fps_index].sz_brt_range;
	if (brt_range_size < 1) {
		panel_err("no brt range\n");
		return -EINVAL;
	}
	brt_range = panel_dim_info->gm2_dim_init_info[vrr_fps_index].brt_range;

	panel_bl = &panel->panel_bl;
	for (row = brt_range_size - 1; row >= 0; row--) {
		if (brt_range[row] <= panel_bl->bd->props.brightness)
			return maptbl_index(tbl, vrr_fps_index, row, 0);
	}
	panel_warn("invalid brt %d\n", panel_bl->bd->props.brightness);

	return maptbl_index(tbl, vrr_fps_index, 0, 0);
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

static int getidx_irc_mode_table(struct maptbl *tbl)
{
	struct panel_device *panel = (struct panel_device *)tbl->pdata;

	return maptbl_index(tbl, 0, !!panel->panel_data.props.irc_mode, 0);
}

#ifdef CONFIG_SUPPORT_GRAM_CHECKSUM
static int s6e3fab_getidx_vddm_table(struct maptbl *tbl)
{
	struct panel_device *panel = (struct panel_device *)tbl->pdata;
	struct panel_properties *props = &panel->panel_data.props;

	panel_info("vddm %d\n", props->gct_vddm);

	return maptbl_index(tbl, 0, props->gct_vddm, 0);
}

static int s6e3fab_getidx_gram_img_pattern_table(struct maptbl *tbl)
{
	struct panel_device *panel = (struct panel_device *)tbl->pdata;
	struct panel_properties *props = &panel->panel_data.props;

	panel_info("gram img %d\n", props->gct_pattern);
	props->gct_valid_chksum[0] = S6E3FAB_GRAM_CHECKSUM_VALID_1;
	props->gct_valid_chksum[1] = S6E3FAB_GRAM_CHECKSUM_VALID_2;
	props->gct_valid_chksum[2] = S6E3FAB_GRAM_CHECKSUM_VALID_1;
	props->gct_valid_chksum[3] = S6E3FAB_GRAM_CHECKSUM_VALID_2;

	return maptbl_index(tbl, 0, props->gct_pattern, 0);
}
#endif

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
static void copy_dummy_maptbl(struct maptbl *tbl, u8 *dst)
{
	return;
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

#ifdef CONFIG_EXYNOS_DECON_LCD_COPR
static void copy_copr_maptbl(struct maptbl *tbl, u8 *dst)
{
	struct copr_info *copr;
	struct copr_reg_v5 *reg;
	int i;

	if (!tbl || !dst)
		return;

	copr = (struct copr_info *)tbl->pdata;
	if (unlikely(!copr))
		return;

	reg = &copr->props.reg.v5;

	dst[0] = (reg->copr_mask << 5) | (reg->cnt_re << 4) |
		(reg->copr_ilc << 3) | (reg->copr_gamma << 1) | reg->copr_en;
	dst[1] = ((reg->copr_er >> 8) & 0x3) << 4 |
		((reg->copr_eg >> 8) & 0x3) << 2 | ((reg->copr_eb >> 8) & 0x3);
	dst[2] = ((reg->copr_erc >> 8) & 0x3) << 4 |
		((reg->copr_egc >> 8) & 0x3) << 2 | ((reg->copr_ebc >> 8) & 0x3);
	dst[3] = reg->copr_er;
	dst[4] = reg->copr_eg;
	dst[5] = reg->copr_eb;
	dst[6] = reg->copr_erc;
	dst[7] = reg->copr_egc;
	dst[8] = reg->copr_ebc;
	dst[9] = (reg->max_cnt >> 8) & 0xFF;
	dst[10] = reg->max_cnt & 0xFF;
	dst[11] = reg->roi_on;
	for (i = 0; i < 5; i++) {
		dst[12 + i * 8] = (reg->roi[i].roi_xs >> 8) & 0x7;
		dst[13 + i * 8] = reg->roi[i].roi_xs & 0xFF;
		dst[14 + i * 8] = (reg->roi[i].roi_ys >> 8) & 0xF;
		dst[15 + i * 8] = reg->roi[i].roi_ys & 0xFF;
		dst[16 + i * 8] = (reg->roi[i].roi_xe >> 8) & 0x7;
		dst[17 + i * 8] = reg->roi[i].roi_xe & 0xFF;
		dst[18 + i * 8] = (reg->roi[i].roi_ye >> 8) & 0xF;
		dst[19 + i * 8] = reg->roi[i].roi_ye & 0xFF;
	}
	print_data(dst, 52);
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

	if (S6E3FAB_SCR_CR_OFS + mdnie->props.sz_scr > sizeof_maptbl(tbl)) {
		panel_err("invalid size (maptbl_size %d, sz_scr %d)\n",
				sizeof_maptbl(tbl), mdnie->props.sz_scr);
		return -EINVAL;
	}

	memcpy(&tbl->arr[S6E3FAB_SCR_CR_OFS],
			mdnie->props.scr, mdnie->props.sz_scr);

	return 0;
}

static int getidx_mdnie_scenario_maptbl(struct maptbl *tbl)
{
	struct mdnie_info *mdnie = (struct mdnie_info *)tbl->pdata;

	return tbl->ncol * (mdnie->props.mode);
}

#ifdef CONFIG_SUPPORT_HMD
static int getidx_mdnie_hmd_maptbl(struct maptbl *tbl)
{
	struct mdnie_info *mdnie = (struct mdnie_info *)tbl->pdata;

	return tbl->ncol * (mdnie->props.hmd);
}
#endif

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

	if (sizeof_maptbl(tbl) < (S6E3FAB_NIGHT_MODE_OFS +
			sizeof_row(night_maptbl))) {
		panel_err("invalid size (maptbl_size %d, night_maptbl_size %d)\n",
				sizeof_maptbl(tbl), sizeof_row(night_maptbl));
		return -EINVAL;
	}

	maptbl_copy(night_maptbl, &tbl->arr[S6E3FAB_NIGHT_MODE_OFS]);

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

	if (sizeof_maptbl(tbl) < (S6E3FAB_COLOR_LENS_OFS +
			sizeof_row(color_lens_maptbl))) {
		panel_err("invalid size (maptbl_size %d, color_lens_maptbl_size %d)\n",
				sizeof_maptbl(tbl), sizeof_row(color_lens_maptbl));
		return -EINVAL;
	}

	if (IS_COLOR_LENS_MODE(mdnie))
		maptbl_copy(color_lens_maptbl, &tbl->arr[S6E3FAB_COLOR_LENS_OFS]);

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
	u8 rddpm[S6E3FAB_RDDPM_LEN] = { 0, };
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
	u8 rddpm[S6E3FAB_RDDPM_LEN] = { 0, };

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
	u8 rddsm[S6E3FAB_RDDSM_LEN] = { 0, };
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
	u8 err[S6E3FAB_ERR_LEN] = { 0, }, err_15_8, err_7_0;

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

	panel_info("========== SHOW PANEL [E9h:DSIERR] INFO ==========\n");
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
	u8 err_fg[S6E3FAB_ERR_FG_LEN] = { 0, };
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
//Todo
#ifdef CONFIG_DISPLAY_USE_INFO
		inc_dpui_u32_field(DPUI_KEY_PNVLO3E, 1);
#endif
	}

	if (err_fg[0] & 0x08) {
		panel_info("* ELVDD Error\n");
//Todo
#ifdef CONFIG_DISPLAY_USE_INFO
		inc_dpui_u32_field(DPUI_KEY_PNELVDE, 1);
#endif
	}

	if (err_fg[0] & 0x40) {
		panel_info("* VLIN1 Error\n");
//Todo
#ifdef CONFIG_DISPLAY_USE_INFO
		inc_dpui_u32_field(DPUI_KEY_PNVLI1E, 1);
#endif
	}

	panel_info("==================================================\n");
}

static void show_dsi_err(struct dumpinfo *info)
{
	int ret;
	struct resinfo *res = info->res;
	u8 dsi_err[S6E3FAB_DSI_ERR_LEN] = { 0, };

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
#if IS_ENABLED(CONFIG_SEC_ABC)
	if (dsi_err[0] > 0)
		sec_abc_send_event("MODULE=display@ERROR=act_section_panel_main_dsi_error");
#endif
}

static void show_self_diag(struct dumpinfo *info)
{
	int ret;
	struct resinfo *res = info->res;
	u8 self_diag[S6E3FAB_SELF_DIAG_LEN] = { 0, };

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
	u8 cmdlog[S6E3FAB_CMDLOG_LEN];

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
static void copy_mafpc_enable_maptbl(struct maptbl *tbl, u8 *dst)
{
	struct panel_device *panel = tbl->pdata;
	struct mafpc_device *mafpc = NULL;

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

	panel_info("MCD:ABC:enabled: %x, written: %x\n", mafpc->enable, mafpc->written);

	if (!mafpc->enable) {
		dst[0] = 0;
		goto err_enable;
	}

	if ((mafpc->written & MAFPC_UPDATED_FROM_SVC) &&
		(mafpc->written & MAFPC_UPDATED_TO_DEV)) {

		dst[0] = S6E3FAB_MAFPC_ENABLE;

		if (mafpc->written & MAFPC_UPDATED_FROM_SVC)
			memcpy(&dst[S6E3FAB_MAFPC_CTRL_CMD_OFFSET], mafpc->ctrl_cmd, mafpc->ctrl_cmd_len);
	}

err_enable:
	print_hex_dump(KERN_ERR, "", DUMP_PREFIX_ADDRESS, 32, 4,
			dst, S6E3FAB_MAFPC_CTRL_CMD_OFFSET + mafpc->ctrl_cmd_len, false);
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


#define S6E3FAB_MAFPC_SCALE_MAX	75


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
		if (!mafpc->scale_buf)
			panel_err("MCD:ABC: scale_buf is null\n");
		if (!mafpc->scale_map_br_tbl)
			panel_err("MCD:ABC: scale_map_br_tbl is null\n");
		goto err_scale;
	}

	index = get_mafpc_scale_index(mafpc, panel);
	if (index < 0) {
		panel_err("mfapc invalid scale index : %d\n", index);
		goto err_scale;
	}

	if (index >= S6E3FAB_MAFPC_SCALE_MAX)
		index = S6E3FAB_MAFPC_SCALE_MAX - 1;

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
	u8 mafpc[S6E3FAB_MAFPC_LEN] = {0, };

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
	u8 mafpc_flash[S6E3FAB_MAFPC_FLASH_LEN] = {0, };

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

static void show_abc_crc_log(struct dumpinfo *info)
{
	int ret;
	struct resinfo *res = info->res;
	u8 abc_crc[S6E3FAB_MAFPC_CRC_LEN] = {0, };

	if (!res || ARRAY_SIZE(abc_crc) != res->dlen) {
		panel_err("invalid resource\n");
		return;
	}

	ret = resource_copy(abc_crc, info->res);
	if (unlikely(ret < 0)) {
		panel_err("failed to copy rddpm resource\n");
		return;
	}

	panel_info("======= SHOW PANEL [FEh(0x09):MAFPC_CRC] INFO =======\n");
	panel_info("* Reg Value : 0x%02x, 0x%02x Result : %s\n", abc_crc[0], abc_crc[1]);
	panel_info("====================================================\n");
}
#endif

static void show_self_mask_crc(struct dumpinfo *info)
{
	int ret;
	struct resinfo *res = info->res;
	u8 crc[S6E3FAB_SELF_MASK_CRC_LEN] = {0, };

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

/*
 * all_gamma_to_each_addr_table
 *
 * S6E3FAB gamma mtp register map is mess up.
 * Frist, read all register values, Make and sort to one big gamma table.
 * Do Gamma offset cal operation as one ALL big gamma table.
 * And then Split the table into mess up register map again.
 */

static int gamma_to_reg_map_table(struct panel_device *panel,
			u8 (*new_gamma)[MAX_S6E3FAB_FQ_SET][S6E3FAB_GAMMA_MTP_LEN], int vrr_index, int br_index)
{
	int buffer_pos = 0, maptbl_pos = 0, mtp_index;
	struct maptbl *target_maptbl = NULL;
	struct maptbl_pos target_pos = {0, };
	u8 gamma_table_temp[MAX_S6E3FAB_FQ_SET * S6E3FAB_GAMMA_MTP_LEN] = { 0, };
	struct panel_info *panel_data = NULL;

	static int copy_order_mtp[MAX_S6E3FAB_FQ_SET] = {
		S6E3FAB_FQ_LOW_7,
		S6E3FAB_FQ_LOW_6,
		S6E3FAB_FQ_LOW_5,
		S6E3FAB_FQ_LOW_4,
		S6E3FAB_FQ_LOW_3,
		S6E3FAB_FQ_LOW_2,
		S6E3FAB_FQ_LOW_1,
		S6E3FAB_FQ_LOW_0,
		S6E3FAB_FQ_HIGH_7,
		S6E3FAB_FQ_HIGH_6,
		S6E3FAB_FQ_HIGH_5,
		S6E3FAB_FQ_HIGH_4,
		S6E3FAB_FQ_HIGH_3,
		S6E3FAB_FQ_HIGH_2,
		S6E3FAB_FQ_HIGH_1,
		S6E3FAB_FQ_HIGH_0,
	};

	panel_data = &panel->panel_data;
	for (mtp_index = 0; mtp_index < ARRAY_SIZE(copy_order_mtp); mtp_index++) {
		memcpy(gamma_table_temp + buffer_pos,
			(*new_gamma)[copy_order_mtp[mtp_index]], S6E3FAB_GAMMA_MTP_LEN);
		buffer_pos += S6E3FAB_GAMMA_MTP_LEN;
	}

	target_pos.index[NDARR_2D] = br_index;
	target_pos.index[NDARR_3D] = vrr_index;
	maptbl_pos = 0;

	target_maptbl = find_panel_maptbl_by_index(panel_data, GAMMA_MTP_0_MAPTBL);
	maptbl_fill(target_maptbl, &target_pos, gamma_table_temp + maptbl_pos, S6E3FAB_GAMMA_0_LEN);
	maptbl_pos += S6E3FAB_GAMMA_0_LEN;

	target_maptbl = find_panel_maptbl_by_index(panel_data, GAMMA_MTP_1_MAPTBL);
	maptbl_fill(target_maptbl, &target_pos, gamma_table_temp + maptbl_pos, S6E3FAB_GAMMA_1_LEN);
	maptbl_pos += S6E3FAB_GAMMA_1_LEN;

	target_maptbl = find_panel_maptbl_by_index(panel_data, GAMMA_MTP_2_MAPTBL);
	maptbl_fill(target_maptbl, &target_pos, gamma_table_temp + maptbl_pos, S6E3FAB_GAMMA_2_LEN);
	maptbl_pos += S6E3FAB_GAMMA_2_LEN;

	target_maptbl = find_panel_maptbl_by_index(panel_data, GAMMA_MTP_3_MAPTBL);
	maptbl_fill(target_maptbl, &target_pos, gamma_table_temp + maptbl_pos, S6E3FAB_GAMMA_3_LEN);
	maptbl_pos += S6E3FAB_GAMMA_3_LEN;

	target_maptbl = find_panel_maptbl_by_index(panel_data, GAMMA_MTP_4_MAPTBL);
	maptbl_fill(target_maptbl, &target_pos, gamma_table_temp + maptbl_pos, S6E3FAB_GAMMA_4_LEN);
	maptbl_pos += S6E3FAB_GAMMA_4_LEN;


	if (buffer_pos != maptbl_pos) {
		panel_warn("size mismatch! br_idx %d buffer %d copied %d\n", br_index, buffer_pos, maptbl_pos);
		return -EINVAL;
	}

	panel_dbg("done, br_idx %d buffer %d copied %d\n", br_index, buffer_pos, maptbl_pos);
	return 0;
}


static void gamma_itoc(u8 *dst, s32(*src)[MAX_COLOR], int nr_tp)
{
	unsigned int i, dst_pos = 0;

	if (nr_tp != S6E3FAB_NR_TP) {
		panel_err("invalid nr_tp(%d)\n", nr_tp);
		return;
	}

	//11th
	dst[0] = src[0][RED] & 0xFF;
	dst[1] = src[0][GREEN] & 0xFF;
	dst[2] = src[0][BLUE] & 0xFF;

	dst_pos = 3;

	//10th ~ 1st
	for (i = 1; i < nr_tp; i++) {
		dst_pos = (i - 1) * 4 + 3;
		dst[dst_pos] = ((src[i][RED] >> 4) & 0x3F);
		dst[dst_pos + 1] = ((src[i][RED] << 4) & 0xF0) | ((src[i][GREEN] >> 6) & 0x0F);
		dst[dst_pos + 2] = ((src[i][GREEN] << 2) & 0xFC) | ((src[i][BLUE] >> 8) & 0x03);
		dst[dst_pos + 3] = ((src[i][BLUE] << 0) & 0xFF);
	}
}


static int gamma_ctoi(s32 (*dst)[MAX_COLOR], u8 *src, int nr_tp)
{
	unsigned int i, pos = 0;

	//11th
	dst[0][RED] = src[pos];
	dst[0][GREEN] = src[pos + 1];
	dst[0][BLUE] = src[pos + 2];

	//10th ~ 1st
	for (i = 1 ; i < nr_tp ; i++) {
		pos = (i - 1) * 4 + 3;
		dst[i][RED] = ((src[pos] & 0x3F) << 4) | ((src[pos + 1] & 0xF0) >> 4);
		dst[i][GREEN] = ((src[pos + 1] & 0x0F) << 6) | ((src[pos + 2] & 0xFC) >> 2);
		dst[i][BLUE] = ((src[pos + 2] & 0x03) << 8) | ((src[pos + 3] & 0xFF) >> 0);
	}

	return 0;
}

static int gamma_sum(s32 (*dst)[MAX_COLOR], s32 (*src)[MAX_COLOR],
		s32 (*offset)[MAX_COLOR], int nr_tp)
{
	unsigned int i, c;
	int upper_limit[S6E3FAB_NR_TP] = {
		0xFF, 0x3FF, 0x3FF, 0x3FF, 0x3FF, 0x3FF, 0x3FF, 0x3FF, 0x3FF, 0x3FF, 0x3FF
	};

	if (nr_tp != S6E3FAB_NR_TP) {
		panel_err("invalid nr_tp(%d)\n", nr_tp);
		return -EINVAL;
	}

	for (i = 0; i < nr_tp; i++) {
		for_each_color(c)
			dst[i][c] =
			min(max(src[i][c] + offset[i][c], 0), upper_limit[i]);
	}

	return 0;
}

static int do_gamma_update(struct panel_device *panel, int vrr_index, int br_index)
{
	int ret;
	struct panel_info *panel_data;
	struct gm2_dimming_lut *update_row;
	struct panel_dimming_info *panel_dim_info;
	struct gm2_dimming_init_info *gm2_dim_init_info;
	u8 gamma_8_org[S6E3FAB_GAMMA_MTP_LEN];
	u8 gamma_8_new[S6E3FAB_GAMMA_MTP_LEN];
	s32 gamma_32_org[S6E3FAB_NR_TP][MAX_COLOR];
	s32 gamma_32_new[S6E3FAB_NR_TP][MAX_COLOR];
	s32(*rgb_color_offset)[MAX_COLOR];
	int gamma_index = 0, lut_idx = 0;
	u8 gamma_8_new_set[MAX_S6E3FAB_FQ_SET][S6E3FAB_GAMMA_MTP_LEN] = { 0, };
	char *source_gamma_str = NULL;

	panel_data = &panel->panel_data;
	panel_dim_info = panel_data->panel_dim_info[PANEL_BL_SUBDEV_TYPE_DISP];

	if (unlikely(!panel_dim_info)) {
		panel_err("panel_bl-%d panel_dim_info is null\n",
				PANEL_BL_SUBDEV_TYPE_DISP);
		return -EINVAL;
	}

	if (panel_dim_info->nr_gm2_dim_init_info == 0) {
		panel_err("panel_bl-%d gm2 init info is null\n",
				PANEL_BL_SUBDEV_TYPE_DISP);
		return -EINVAL;
	}

	if (vrr_index >= panel_dim_info->nr_gm2_dim_init_info) {
		panel_err("panel_bl-%d cannot find gm2 init info[%d]\n",
				PANEL_BL_SUBDEV_TYPE_DISP, vrr_index);
		return -EINVAL;
	}

	gm2_dim_init_info = &panel_dim_info->gm2_dim_init_info[vrr_index];
	if (gm2_dim_init_info == NULL) {
		panel_err("panel_bl-%d gm2 init info[%d] is null\n",
			PANEL_BL_SUBDEV_TYPE_DISP, vrr_index);
		return -EINVAL;
	}

	if (!gm2_dim_init_info->dim_lut) {
		panel_err("no dim_lut(vrr %d)\n");
		return -ENODATA;
	}

	for (gamma_index = 0; gamma_index < MAX_S6E3FAB_FQ_SET; gamma_index++) {
		lut_idx = MAX_S6E3FAB_FQ_SET * br_index + gamma_index;
		panel_dbg("lut idx %d\n", lut_idx);
		if (gm2_dim_init_info->nr_dim_lut <= lut_idx) {
			panel_err("out of range %d, vrr %d br %d gamma %d\n", lut_idx, vrr_index, br_index, gamma_index);
			continue;
		}
		update_row = &gm2_dim_init_info->dim_lut[lut_idx];
		rgb_color_offset = update_row->rgb_color_offset;
		source_gamma_str = update_row->source_gamma;
		if (source_gamma_str == NULL) {
			panel_err("source gamma is null, vrr %d br %d gamma %d\n", vrr_index, br_index, gamma_index);
			continue;
		}

		ret = resource_copy_by_name(panel_data, gamma_8_org, source_gamma_str);
		if (unlikely(ret < 0)) {
			panel_err("%s not found in panel resource, vrr %d br %d gamma %d\n",
				source_gamma_str, vrr_index, br_index, gamma_index);
			continue;
		}

		if (rgb_color_offset == NULL) {
			panel_dbg("vrr %d br_index %d gamma_index %d source_gamma %s\n",
				vrr_index, br_index, gamma_index, source_gamma_str);
			memcpy(gamma_8_new, gamma_8_org, S6E3FAB_GAMMA_MTP_LEN);
		} else {
			panel_info("vrr %d br_index %d gamma_index %d source_gamma %s offset updated\n",
				vrr_index, br_index, gamma_index, source_gamma_str);
			panel_dbg("gamma resource org:\n");
			if (panel_log_level >= 7)
				print_hex_dump(KERN_INFO, "", DUMP_PREFIX_NONE, 16, 1, gamma_8_org, ARRAY_SIZE(gamma_8_org), false);
			/* byte to int */
			gamma_ctoi(gamma_32_org, gamma_8_org, S6E3FAB_NR_TP);
			/* add offset */
			gamma_sum(gamma_32_new, gamma_32_org, rgb_color_offset, S6E3FAB_NR_TP);
			/* int to byte */
			gamma_itoc(gamma_8_new, gamma_32_new, S6E3FAB_NR_TP);
			panel_dbg("gamma resource new:\n");
			if (panel_log_level >= 7)
				print_hex_dump(KERN_INFO, "", DUMP_PREFIX_NONE, 16, 1, gamma_8_new, ARRAY_SIZE(gamma_8_new), false);
		}

		memcpy(gamma_8_new_set[gamma_index], gamma_8_new, S6E3FAB_GAMMA_MTP_LEN);
	}

	ret = gamma_to_reg_map_table(panel, &gamma_8_new_set, vrr_index, br_index);

	return ret;
}

static int init_gamma_mtp_table(struct maptbl *tbl)
{
	struct panel_device *panel;
	struct panel_info *panel_data;
	struct panel_dimming_info *panel_dim_info;
	int ret = 0;
	int vrr_index, br_index = 0, br_range_size;

	if (unlikely(!tbl || !tbl->pdata)) {
		panel_err("panel_bl-%d invalid param (tbl %p, pdata %p)\n",
				PANEL_BL_SUBDEV_TYPE_DISP, tbl, tbl ? tbl->pdata : NULL);
		return -EINVAL;
	}

	panel = tbl->pdata;

	panel_data = &panel->panel_data;
	panel_dim_info = panel_data->panel_dim_info[PANEL_BL_SUBDEV_TYPE_DISP];

	if (unlikely(!panel_dim_info)) {
		panel_err("panel_bl-%d panel_dim_info is null\n",
				PANEL_BL_SUBDEV_TYPE_DISP);
		return -EINVAL;
	}

	if (panel_dim_info->nr_gm2_dim_init_info == 0) {
		panel_err("panel_bl-%d gamma mode 2 init info is null\n",
				PANEL_BL_SUBDEV_TYPE_DISP);
		return -EINVAL;
	}

	for (vrr_index = 0; vrr_index < MAX_S6E3FAB_VRR; vrr_index++) {
		br_range_size = panel_dim_info->gm2_dim_init_info[vrr_index].sz_brt_range;
		if (br_range_size < 1) {
			panel_err("no brt range\n");
			return -EINVAL;
		}
		for (br_index = 0; br_index < br_range_size; br_index++) {
			panel_dbg("generate gamma %d %d\n", vrr_index, br_index);
			ret = do_gamma_update(panel, vrr_index, br_index);
			if (ret < 0)
				panel_err("%s failed to generate gamma offset table, vrr_index %d, br_index %d\n",
					tbl->name, vrr_index, br_index);
		}
	}
	if (ret)
		return ret;

	panel_info("%s initialized, nr_br: %d\n", tbl->name, br_range_size);
	return ret;
}

static int init_gamma_mtp_all_table(struct maptbl *tbl)
{
	return init_gamma_mtp_table(tbl);
}

#ifdef CONFIG_SUPPORT_DDI_FLASH
static int do_gamma_flash_checksum(struct panel_device *panel, void *data, u32 len)
{
	int ret = 0;
	struct dim_flash_result *result;
	u32 sum_read = 0, sum_calc = 0, sum = 0;
	int state, index, exist = 0;

	result = (struct dim_flash_result *) data;

	if (!result)
		return -ENODATA;

	if (atomic_cmpxchg(&result->running, 0, 1) != 0) {
		panel_info("already running\n");
		return -EBUSY;
	}

	result->state = state = GAMMA_FLASH_PROGRESS;

	memset(result->result, 0, ARRAY_SIZE(result->result));

	if (is_id_gte_03(panel)) {
		do {
			/* gamma mode 2 flash data */
			index = POC_GM2_GAMMA_PARTITION;
			ret = set_panel_poc(&panel->poc_dev, POC_OP_GM2_READ, &index);
			if (unlikely(ret < 0)) {
				panel_err("failed to read gamma flash(ret %d)\n", ret);
				state = GAMMA_FLASH_ERROR_READ_FAIL;
				break;
			}

			exist = check_poc_partition_exists(&panel->poc_dev, index);
			if (unlikely(exist < 0)) {
				panel_err("failed to check dim_flash exist\n");
				state = GAMMA_FLASH_ERROR_READ_FAIL;
				break;
			} else if (unlikely(exist == PARTITION_WRITE_CHECK_NOK)) {
				panel_err("dim partition not exist(%d)\n", exist);
				state = GAMMA_FLASH_ERROR_NOT_EXIST;
				break;
			}

			ret = get_poc_partition_chksum(&panel->poc_dev, index, &sum, &sum_calc, &sum_read);
			if (unlikely(ret < 0)) {
				panel_err("failed to get chksum(ret %d)\n", ret);
				state = GAMMA_FLASH_ERROR_READ_FAIL;
				break;
			}
			if (sum_calc != sum_read) {
				panel_err("flash checksum(%04X,%04X) mismatch\n", sum_calc, sum_read);
				state = GAMMA_FLASH_ERROR_CHECKSUM_MISMATCH;
			}
		} while (0);
	}
	else
		panel_info("not supported\n");

	if (state == GAMMA_FLASH_PROGRESS)
		state = GAMMA_FLASH_SUCCESS;

	snprintf(result->result, ARRAY_SIZE(result->result), "1\n%d %08X %08X %08X",
		state, sum_calc, sum_read, exist);

	result->exist = exist;
	result->state = state;

	atomic_xchg(&result->running, 0);

	return ret;
}

static int s6e3fab_mtp_gamma_check(struct panel_device *panel, void *data, u32 len)
{
	struct panel_info *panel_data;
	int ret = 0, i, k, result = 1;
	u32 res_index, tmp;
	u8 gamma_buffer[S6E3FAB_GAMMA_MTP_LEN];
	static const char *res_names[] = {
		"gamma_mtp_120hs_0",
		"gamma_mtp_120hs_1",
		"gamma_mtp_120hs_2",
		"gamma_mtp_120hs_3",
		"gamma_mtp_120hs_4",
		"gamma_mtp_120hs_5",
		"gamma_mtp_120hs_6",
		"gamma_mtp_120hs_7",
		"gamma_mtp_60hs_0",
		"gamma_mtp_60hs_1",
		"gamma_mtp_60hs_2",
		"gamma_mtp_60hs_3",
		"gamma_mtp_60hs_4",
		"gamma_mtp_60hs_5",
		"gamma_mtp_60hs_6",
		"gamma_mtp_60hs_7",
	};

	panel_data = &panel->panel_data;

	for (res_index = 0; res_index < ARRAY_SIZE(res_names); res_index++) {
		ret = get_resource_size_by_name(panel_data, (char *)res_names[res_index]);
		if (ret != S6E3FAB_GAMMA_MTP_LEN) {
			panel_err("error size check %s, ret %d\n", res_names[res_index], ret);
			result = 0;
			continue;
		}

		ret = resource_copy_by_name(panel_data, gamma_buffer, (char *)res_names[res_index]);
		if (ret < 0) {
			//error
			panel_err("error read %s, ret %d\n", res_names[res_index], ret);
			result = 0;
			continue;
		}

		for (i = 1; i < 9; i++) {
			k = 3 + 4 * i;
			if (k + 3 >= S6E3FAB_GAMMA_MTP_LEN) {
				panel_err("error compare %s, exceeded max size %d, i: %d\n",
					res_names[res_index], S6E3FAB_GAMMA_MTP_LEN, i);
				result = 0;
				break;
			}
			tmp = ((gamma_buffer[k] & 0x3F) << 24) | (gamma_buffer[k + 1] << 16) |
				(gamma_buffer[k + 2] << 8) | (gamma_buffer[k + 3]);
			if (unlikely(tmp == 0x3FFFFFFF)) {
				panel_err("error compare %s, i: %d value: 0x%08X\n", res_names[res_index], i, tmp);
				result = 0;
				continue;
			}
			panel_dbg("%s[%d] = 0x%08X\n", res_names[res_index], i, tmp);
		}
	}

	return result;
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

static int __init s6e3fab_panel_init(void)
{
	register_common_panel(&s6e3fab_unbound1_a3_s0_panel_info);
	register_common_panel(&s6e3fab_unbound2_a3_s0_panel_info);

	return 0;
}

static void __exit s6e3fab_panel_exit(void)
{
	deregister_common_panel(&s6e3fab_unbound1_a3_s0_panel_info);
	deregister_common_panel(&s6e3fab_unbound2_a3_s0_panel_info);
}

module_init(s6e3fab_panel_init)
module_exit(s6e3fab_panel_exit)
MODULE_DESCRIPTION("Samsung Mobile Panel Driver");
MODULE_LICENSE("GPL");
