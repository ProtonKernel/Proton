// SPDX-License-Identifier: GPL-2.0
/*
 * linux/drivers/video/fbdev/exynos/panel/s6e3hab/s6e3hab.c
 *
 * S6E3HAB Dimming Driver
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
#include "s6e3hab.h"
#include "s6e3hab_panel.h"
#ifdef CONFIG_PANEL_AID_DIMMING
#include "../dimming.h"
#include "../panel_dimming.h"
#endif
#include "../panel_drv.h"
#include "../panel_debug.h"

#if IS_ENABLED(CONFIG_SEC_ABC)
#include <linux/sti/abc_common.h>
#endif

#ifdef PANEL_PR_TAG
#undef PANEL_PR_TAG
#define PANEL_PR_TAG	"ddi"
#endif

static bool is_supported_vrr(struct panel_info *panel_data,
		struct panel_vrr *vrr)
{
	int i;

	if (!panel_data || !vrr) {
		panel_err("invalid parameter\n");
		return false;
	}

	for (i = 0; i < panel_data->nr_vrrtbl; i++) {
		if (!memcmp(panel_data->vrrtbl[i], vrr, sizeof(struct panel_vrr)))
			return true;
	}

	return false;
}

static int getidx_s6e3hab_vrr_dim(int fps, int mode)
{
	if (mode == VRR_NORMAL_MODE)
		return S6E3HAB_VRR_DIM_60NS;
	else if (fps == 60)
		return S6E3HAB_VRR_DIM_60HS;
	else
		return S6E3HAB_VRR_DIM_120HS;
}

static int getidx_s6e3hab_current_vrr_dim(struct panel_device *panel)
{
	int vrr_fps, vrr_mode;

	vrr_fps = get_panel_refresh_rate(panel);
	if (vrr_fps < 0)
		return -EINVAL;

	vrr_mode = get_panel_refresh_mode(panel);
	if (vrr_mode < 0)
		return -EINVAL;

	return getidx_s6e3hab_vrr_dim(vrr_fps, vrr_mode);
}

static int getidx_s6e3hab_vrr_fps(int fps)
{
	return fps > 60 ? S6E3HAB_VRR_FPS_120 : S6E3HAB_VRR_FPS_60;
}

static int getidx_s6e3hab_current_vrr_fps(struct panel_device *panel)
{
	int vrr_fps;

	vrr_fps = get_panel_refresh_rate(panel);
	if (vrr_fps < 0)
		return -EINVAL;

	return getidx_s6e3hab_vrr_fps(vrr_fps);
}

static int getidx_s6e3hab_vrr_mode(int mode)
{
	return mode == VRR_HS_MODE ?
		S6E3HAB_VRR_MODE_HS : S6E3HAB_VRR_MODE_NS;
}

static int getidx_s6e3hab_current_vrr_mode(struct panel_device *panel)
{
	int vrr_mode;

	vrr_mode = get_panel_refresh_mode(panel);
	if (vrr_mode < 0)
		return -EINVAL;

	return getidx_s6e3hab_vrr_mode(vrr_mode);
}

static int calc_vfp(struct panel_vrr *vrr, int fps)
{
	int vfp, base_fps, base_vbp, base_vfp, base_vactive;

	if (!vrr)
		return -EINVAL;

	if (fps == 0) {
		panel_warn("fps is zero!!\n");
		return -EINVAL;
	}

	base_fps = vrr->base_fps;
	base_vbp = vrr->base_vbp;
	base_vfp = vrr->base_vfp;
	base_vactive = vrr->base_vactive;

	if (base_fps == fps)
		vfp = base_vfp;
	else
		vfp = (((base_vbp + base_vactive + base_vfp) * base_fps) / fps)
			- (base_vbp + base_vactive);

	panel_dbg("fps:%d vfp:%d base(fps:%d vbp:%d vfp:%d vactive:%d)\n",
			fps, vfp, base_fps, base_vbp, base_vfp, base_vactive);

	return vfp;
}

#ifdef CONFIG_PANEL_AID_DIMMING
static int gamma_ctoi(s32 (*dst)[MAX_COLOR], u8 *src, int nr_tp)
{
	unsigned int i, c, pos = 1;

	for (i = nr_tp - 1; i > 1; i--) {
		for_each_color(c) {
			if (i == nr_tp - 1)
				dst[i][c] = ((src[0] >> ((MAX_COLOR - c - 1) * 2)) & 0x03) << 8 | src[pos++];
			else
				dst[i][c] = src[pos++];
		}
	}

	dst[1][RED] = (src[pos + 0] >> 4) & 0xF;
	dst[1][GREEN] = src[pos + 0] & 0xF;
	dst[1][BLUE] = (src[pos + 1] >> 4) & 0xF;

	dst[0][RED] = src[pos + 1] & 0xF;
	dst[0][GREEN] = (src[pos + 2] >> 4) & 0xF;
	dst[0][BLUE] = src[pos + 2] & 0xF;

	return 0;
}

#if defined(CONFIG_SEC_KUNIT)
int s6e3hab_gamma_ctoi(s32 (*dst)[MAX_COLOR], u8 *src, int nr_tp)
{
	return gamma_ctoi(dst, src, nr_tp);
}
#endif

static void mtp_ctoi(s32 (*dst)[MAX_COLOR], u8 *src, int nr_tp)
{
	int v, c, sign;
	int signed_bit[S6E3HAB_NR_TP] = {
		-1, -1, 7, 7, 7, 7, 7, 7, 7, 7, 7, 9
	};
	int value_mask[S6E3HAB_NR_TP] = {
		0xF, 0xF, 0x7F, 0x7F, 0x7F, 0x7F, 0x7F, 0x7F, 0x7F, 0x7F, 0x7F, 0x1FF
	};
	int value;

	gamma_ctoi(dst, src, nr_tp);
	for (v = 0; v < nr_tp; v++) {
		for_each_color(c) {
			sign = (signed_bit[v] < 0) ? 1 :
				(dst[v][c] & (0x1 << signed_bit[v])) ? -1 : 1;
			value = dst[v][c] & value_mask[v];
			dst[v][c] = sign * value;
		}
	}
}

static void copy_tpout_center(u8 *output, u32 value, u32 v, u32 color)
{
	int index = (S6E3HAB_NR_TP - v - 1);

	if (v == S6E3HAB_NR_TP - 1) {
		output[0] &= ~(0x3 << ((MAX_COLOR - color - 1) * 2));
		output[0] |= (((value >> 8) & 0x03) << ((MAX_COLOR - color - 1) * 2));
		output[color + 1] = value & 0x00FF;
		if (value > 1023)
			panel_err("error : exceed output range tp:%d c:%d value:%d!!\n",
					index, color, value);
	} else if (v == 1) {
		if (color == RED) {
			output[index * MAX_COLOR + 1] &= ~(0xF << 4);
			output[index * MAX_COLOR + 1] |= (value & 0xF) << 4;
		} else if (color == GREEN) {
			output[index * MAX_COLOR + 1] &= ~0xF;
			output[index * MAX_COLOR + 1] |= (value & 0xF);
		} else if (color == BLUE) {
			output[index * MAX_COLOR + 2] &= ~(0xF << 4);
			output[index * MAX_COLOR + 2] |= (value & 0xF) << 4;
		}
		if (value > 15)
			panel_err("error : exceed output range tp:%d c:%d value:%d!!\n",
					index, color, value);
	} else if (v == 0) {
		if (color == RED) {
			output[(index - 1) * MAX_COLOR + 2] &= ~0xF;
			output[(index - 1) * MAX_COLOR + 2] |= (value & 0xF);
		} else if (color == GREEN) {
			output[(index - 1) * MAX_COLOR + 3] &= ~(0xF << 4);
			output[(index - 1) * MAX_COLOR + 3] |= (value & 0xF) << 4;
		} else if (color == BLUE) {
			output[(index - 1) * MAX_COLOR + 3] &= ~0xF;
			output[(index - 1) * MAX_COLOR + 3] |= (value & 0xF);
		}
		if (value > 15)
			panel_err("error : exceed output range tp:%d c:%d value:%d!!\n",
					index, color, value);
	} else {
		output[index * MAX_COLOR + color + 1] = value & 0x00FF;
		if (value > 255)
			panel_err("error : exceed output range tp:%d c:%d value:%d!!\n",
					index, color, value);
	}
}

static void gamma_itoc(u8 *dst, s32(*src)[MAX_COLOR], int nr_tp)
{
	int v, c;

	for (v = 0; v < nr_tp; v++) {
		for_each_color(c) {
			copy_tpout_center(dst, src[v][c], v, c);
		}
	}
}

#if defined(CONFIG_SEC_KUNIT)
void s6e3hab_gamma_itoc(u8 *dst, s32(*src)[MAX_COLOR], int nr_tp)
{
	gamma_itoc(dst, src, nr_tp);
}
#endif

static void arr_to_center_gamma(u8 *src, struct dimming_init_info *dst)
{
	unsigned int i, c, pos = 1;

	for (i = dst->nr_tp - 1; i > 1; i--) {
		for_each_color(c) {
			if (i == dst->nr_tp - 1)
				dst->tp[i].center[c] = ((src[0] >> ((MAX_COLOR - c - 1) * 2)) & 0x03) << 8 | src[pos++];
			else
				dst->tp[i].center[c] =  src[pos++];
		}
	}

	dst->tp[1].center[RED] = (src[pos + 0] >> 4) & 0xF;
	dst->tp[1].center[GREEN] = src[pos + 0] & 0xF;
	dst->tp[1].center[BLUE] = (src[pos + 1] >> 4) & 0xF;
}

#ifdef CONFIG_SUPPORT_DIM_FLASH
static int dim_flash_is_valid(struct panel_info *panel_data,
		enum dim_flash_items item, int vrr_idx)
{
	return panel_resource_initialized(panel_data,
			s6e3hab_dim_flash_info[vrr_idx][item].name);
}

static int copy_from_dim_flash(struct panel_info *panel_data,
		u8 *dst, enum dim_flash_items item, int vrr_idx, int row, int col, int size)
{
	static struct resinfo *res;
	char *name = s6e3hab_dim_flash_info[vrr_idx][item].name;
	int nrow = s6e3hab_dim_flash_info[vrr_idx][item].nrow;
	int ncol = s6e3hab_dim_flash_info[vrr_idx][item].ncol;

	res = find_panel_resource(panel_data, name);
	if (unlikely(!res)) {
		panel_warn("resource(%s) not found (vrr_idx %d, row %d, col %d, size %d)\n",
				name, vrr_idx, row, col, size);
		return -EIO;
	}

	if (!panel_resource_initialized(panel_data, name)) {
		panel_warn("resource(%s) not initialized (vrr_idx %d, row %d, col %d, size %d)\n",
				name, vrr_idx, row, col, size);
		return -EINVAL;
	}

	if (row >= nrow || col >= ncol) {
		panel_err("exceeded nrow vrr_idx:%d name:%s nrow:%d ncol:%d size:%d, row:%d, col:%d\n",
				vrr_idx, name, nrow, ncol, size, row, col);
		return 0;
	}

	memcpy(dst, &res->data[(nrow - row - 1) * ncol + col], size);
	return 0;
}
#endif

static void print_gamma_table(struct panel_info *panel_data, int id)
{
	int i, j, vrr_idx, index, len, luminance;
	char strbuf[1024];
	struct panel_device *panel = to_panel_device(panel_data);
	struct panel_bl_device *panel_bl = &panel->panel_bl;
	struct maptbl *gamma_maptbl = NULL;
	struct brightness_table *brt_tbl = &panel_bl->subdev[id].brt_tbl;

#ifdef CONFIG_SUPPORT_HMD
	gamma_maptbl = find_panel_maptbl_by_index(panel_data,
			(id == PANEL_BL_SUBDEV_TYPE_HMD) ? HMD_GAMMA_MAPTBL : GAMMA_MAPTBL);
#else
	gamma_maptbl = find_panel_maptbl_by_index(panel_data, GAMMA_MAPTBL);
#endif
	if (unlikely(!gamma_maptbl))
		return;

	for (vrr_idx = 0; vrr_idx < MAX_S6E3HAB_VRR_DIM; vrr_idx++) {
		if (!is_supported_vrr(panel_data, &S6E3HAB_VRR_DIM[vrr_idx]))
			continue;

		for (i = 0; i < brt_tbl->sz_lum; i++) {
			luminance = brt_tbl->lum[i];
			len = snprintf(strbuf, 1024, "gamma[%3d] : ", luminance);
			for (j = 0; j < S6E3HAB_GAMMA_CMD_CNT - 1; j++) {
				index = maptbl_index(gamma_maptbl, vrr_idx, i, j);
				len += snprintf(strbuf + len, max(1024 - len, 0),
						"%02X ", gamma_maptbl->arr[index]);
			}
			panel_info("%s\n", strbuf);
		}
	}
}

static int generate_hbm_gamma_table(struct panel_info *panel_data, int id)
{
	struct panel_device *panel = to_panel_device(panel_data);
	struct panel_bl_device *panel_bl = &panel->panel_bl;
	struct panel_dimming_info *panel_dim_info;
	struct maptbl *gamma_maptbl = NULL;
	struct resinfo *hbm_gamma_res;
	struct brightness_table *brt_tbl;
	s32 (*out_gamma_tbl)[MAX_COLOR] = NULL;
	s32 (*nor_gamma_tbl)[MAX_COLOR] = NULL;
	s32 (*hbm_gamma_tbl)[MAX_COLOR] = NULL;
	char *hbm_gamma_resource_names[MAX_S6E3HAB_VRR_DIM] = {
		[S6E3HAB_VRR_DIM_60NS] = "hbm_gamma",
		[S6E3HAB_VRR_DIM_60HS] = "hbm_gamma_60hz_hs",
		[S6E3HAB_VRR_DIM_120HS] = "hbm_gamma_120hz",
	};
	int luminance, nr_luminance, nr_hbm_luminance, nr_extend_hbm_luminance;
	int i, vrr_idx, nr_tp, ret = 0;

	panel_dim_info = panel_data->panel_dim_info[id];
	brt_tbl = &panel_bl->subdev[id].brt_tbl;
	nr_tp = panel_dim_info->dim_init_info.nr_tp;
	nr_luminance = panel_dim_info->nr_luminance;
	nr_hbm_luminance = panel_dim_info->nr_hbm_luminance;
	nr_extend_hbm_luminance = panel_dim_info->nr_extend_hbm_luminance;

	out_gamma_tbl = kmalloc(sizeof(s32) * nr_tp * MAX_COLOR, GFP_KERNEL);
	nor_gamma_tbl = kmalloc(sizeof(s32) * nr_tp * MAX_COLOR, GFP_KERNEL);
	hbm_gamma_tbl = kmalloc(sizeof(s32) * nr_tp * MAX_COLOR, GFP_KERNEL);
	for (vrr_idx = 0; vrr_idx < MAX_S6E3HAB_VRR_DIM; vrr_idx++) {
		if (!is_supported_vrr(panel_data, &S6E3HAB_VRR_DIM[vrr_idx]))
			continue;

		memset(out_gamma_tbl, 0, sizeof(s32) * nr_tp * MAX_COLOR);
		memset(nor_gamma_tbl, 0, sizeof(s32) * nr_tp * MAX_COLOR);
		memset(hbm_gamma_tbl, 0, sizeof(s32) * nr_tp * MAX_COLOR);

		hbm_gamma_res = find_panel_resource(panel_data,
				hbm_gamma_resource_names[vrr_idx]);
		if (unlikely(!hbm_gamma_res)) {
			panel_warn("panel_bl-%d %s not found\n",
					id, hbm_gamma_resource_names[vrr_idx]);
			return -EIO;
		}

#ifdef CONFIG_SUPPORT_HMD
		gamma_maptbl = find_panel_maptbl_by_index(panel_data,
				(id == PANEL_BL_SUBDEV_TYPE_HMD) ? HMD_GAMMA_MAPTBL : GAMMA_MAPTBL);
#else
		gamma_maptbl = find_panel_maptbl_by_index(panel_data, GAMMA_MAPTBL);
#endif
		if (unlikely(!gamma_maptbl)) {
			panel_err("panel_bl-%d gamma_maptbl not found\n", id);
			ret = -EINVAL;
			goto err;
		}

		gamma_ctoi(nor_gamma_tbl, &gamma_maptbl->arr[
				maptbl_index(gamma_maptbl, vrr_idx, nr_luminance - 1, 0)], nr_tp);
		gamma_ctoi(hbm_gamma_tbl, hbm_gamma_res->data, nr_tp);
		for (i = nr_luminance; i < nr_luminance +
				nr_hbm_luminance + nr_extend_hbm_luminance; i++) {
			luminance = brt_tbl->lum[i];
			if (i < nr_luminance + nr_hbm_luminance)
				/* UI MAX BRIGHTNESS ~ HBM MAX BRIGHTNESS */
				gamma_table_interpolation_round(nor_gamma_tbl, hbm_gamma_tbl, out_gamma_tbl,
						nr_tp, luminance - panel_dim_info->target_luminance,
						panel_dim_info->hbm_target_luminance - panel_dim_info->target_luminance);
			else
				/* UI MAX BRIGHTNESS ~ EXTENDED HBM MAX BRIGHTNESS */
				memcpy(out_gamma_tbl, hbm_gamma_tbl, sizeof(s32) * nr_tp * MAX_COLOR);

			gamma_itoc((u8 *)&gamma_maptbl->arr[
					maptbl_index(gamma_maptbl, vrr_idx, i, 0)],
					out_gamma_tbl, nr_tp);
		}
	}

err:
	kfree(out_gamma_tbl);
	kfree(nor_gamma_tbl);
	kfree(hbm_gamma_tbl);

	return ret;
}

static int generate_gamma_table_using_lut(struct panel_info *panel_data, int id)
{
	struct panel_dimming_info *panel_dim_info;
	struct dimming_info *dim_info;
	struct maptbl *gamma_maptbl = NULL;
	struct panel_device *panel = to_panel_device(panel_data);
	struct panel_bl_device *panel_bl = &panel->panel_bl;
	struct dimming_init_info *dim_init_info[MAX_S6E3HAB_VRR_DIM];
	struct brightness_table *brt_tbl;
	struct resinfo *res;
	s32 (*mtp)[MAX_COLOR] = NULL;
	int i, ret, nr_tp, vrr_idx;
	u32 nr_luminance;
	char *center_gamma_resource_names[MAX_S6E3HAB_VRR_DIM] = {
		[S6E3HAB_VRR_DIM_60NS] = "center_gamma_60hz",	// default
		[S6E3HAB_VRR_DIM_60HS] = "center_gamma_60hz_hs",
		[S6E3HAB_VRR_DIM_120HS] = "center_gamma_120hz",
	};

	if (id >= MAX_PANEL_BL_SUBDEV) {
		panel_err("panel_bl-%d invalid id\n", id);
		return -EINVAL;
	}

	brt_tbl = &panel_bl->subdev[id].brt_tbl;
	panel_dim_info = panel_data->panel_dim_info[id];
	dim_init_info[S6E3HAB_VRR_DIM_60NS] = &panel_dim_info->dim_init_info;
	dim_init_info[S6E3HAB_VRR_DIM_120HS] = &panel_dim_info->dim_120hz_init_info;
	dim_init_info[S6E3HAB_VRR_DIM_60HS] = &panel_dim_info->dim_60hz_hs_init_info;

	for (vrr_idx = 0; vrr_idx < MAX_S6E3HAB_VRR_DIM; vrr_idx++) {
		if (!is_supported_vrr(panel_data, &S6E3HAB_VRR_DIM[vrr_idx]))
			continue;
		nr_tp = dim_init_info[vrr_idx]->nr_tp;
		nr_luminance = panel_dim_info->nr_luminance;

		if (id == PANEL_BL_SUBDEV_TYPE_DISP) {
			if ((vrr_idx > S6E3HAB_VRR_DIM_60NS) && (vrr_idx < MAX_S6E3HAB_VRR_DIM)) {
				res = find_panel_resource(panel_data, center_gamma_resource_names[vrr_idx]);
				if (likely(res)) {
					arr_to_center_gamma(res->data, dim_init_info[vrr_idx]);
					panel_warn("%d : %d, %s\n",
							id, vrr_idx, center_gamma_resource_names[vrr_idx]);
				} else {
					panel_warn("panel_bl-%d resource(%s) not found, so use default(60normal)\n",
							id, center_gamma_resource_names[vrr_idx]);
				}
			}
		}
#ifdef CONFIG_SUPPORT_PANEL_SWAP
		if (panel_data->dim_info[id]) {
			kfree(panel_data->dim_info[id]);
			panel_data->dim_info[id] = NULL;
			panel_info("panel_bl-%d free dimming info\n", id);
		}
#endif

		dim_info = kzalloc(sizeof(struct dimming_info), GFP_KERNEL);
		if (unlikely(!dim_info)) {
			panel_err("panel_bl-%d failed to alloc dimming_info\n", id);
			return -ENOMEM;
		}

		ret = init_dimming_info(dim_info, dim_init_info[vrr_idx]);
		if (unlikely(ret)) {
			panel_err("panel_bl-%d failed to init_dimming_info\n", id);
			ret = -EINVAL;
			goto err;
		}

		if (panel_dim_info->dimming_maptbl) {
			gamma_maptbl = &panel_dim_info->dimming_maptbl[DIMMING_GAMMA_MAPTBL];
		} else {
#ifdef CONFIG_SUPPORT_HMD
			gamma_maptbl = find_panel_maptbl_by_index(panel_data,
					(id == PANEL_BL_SUBDEV_TYPE_HMD) ?
					HMD_GAMMA_MAPTBL : GAMMA_MAPTBL);
#else
			gamma_maptbl = find_panel_maptbl_by_index(panel_data, GAMMA_MAPTBL);
#endif
		}
		if (unlikely(!gamma_maptbl)) {
			panel_err("panel_bl-%d gamma_maptbl not found\n", id);
			ret = -EINVAL;
			goto err;
		}

		/* GET MTP OFFSET */
		res = find_panel_resource(panel_data, "mtp");
		if (unlikely(!res)) {
			panel_warn("panel_bl-%d resource(mtp) not found\n", id);
			ret = -EINVAL;
			goto err;
		}

		mtp = kzalloc(sizeof(s32) * nr_tp * MAX_COLOR, GFP_KERNEL);
		mtp_ctoi(mtp, res->data, nr_tp);
		init_dimming_mtp(dim_info, mtp);
		process_dimming(dim_info);

		for (i = 0; i < nr_luminance; i++)
			get_dimming_gamma(dim_info, brt_tbl->lum[i],
					(u8 *)&gamma_maptbl->arr[
					maptbl_index(gamma_maptbl, vrr_idx, i, 0)],
					//i * sizeof_row(gamma_maptbl)],
					copy_tpout_center);

		panel_data->dim_info[id] = dim_info;
	}

	panel_info("panel_bl-%d done\n", id);
	kfree(mtp);

	return 0;

err:
	kfree(dim_info);
	return ret;
}

static int generate_brt_step_table(struct brightness_table *brt_tbl)
{
	int ret = 0;
	int i = 0, j = 0, k = 0;

	if (unlikely(!brt_tbl || !brt_tbl->brt || !brt_tbl->lum)) {
		panel_err("invalid parameter\n");
		return -EINVAL;
	}
	if (unlikely(!brt_tbl->step_cnt)) {
		if (likely(brt_tbl->brt_to_step)) {
			panel_info("we use static step table\n");
			return ret;
		}
		panel_err("invalid parameter, all table is NULL\n");
		return -EINVAL;
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
				if (brt_tbl->brt[brt_tbl->sz_ui_lum - 1] < brt_tbl->brt_to_step[i])
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

#ifdef CONFIG_SUPPORT_DIM_FLASH
static int generate_gamma_table_using_flash(struct panel_info *panel_data, int id)
{
	struct maptbl *tbl = NULL;
	int i, vrr_idx, ret, index;
	u32 nr_luminance;

	if (id >= MAX_PANEL_BL_SUBDEV) {
		panel_err("panel_bl-%d invalid id\n", id);
		return -EINVAL;
	}

	if (id == PANEL_BL_SUBDEV_TYPE_DISP)
		index = GAMMA_MAPTBL;
#ifdef CONFIG_SUPPORT_HMD
	else if (id == PANEL_BL_SUBDEV_TYPE_HMD)
		index = HMD_GAMMA_MAPTBL;
#endif

	nr_luminance = panel_data->panel_dim_info[id]->nr_luminance;
	tbl = find_panel_maptbl_by_index(panel_data, index);
	if (unlikely(!tbl)) {
		panel_err("panel_bl-%d gamma_maptbl not found\n", id);
		return -EINVAL;
	}

	for (vrr_idx = 0; vrr_idx < MAX_S6E3HAB_VRR_DIM; vrr_idx++) {
		if (!is_supported_vrr(panel_data, &S6E3HAB_VRR_DIM[vrr_idx]))
			continue;

		if (!dim_flash_is_valid(panel_data,
#ifdef CONFIG_SUPPORT_HMD
			(id == PANEL_BL_SUBDEV_TYPE_HMD) ? DIM_FLASH_HMD_GAMMA :
#endif
			DIM_FLASH_GAMMA, vrr_idx)) {
			panel_warn("dim_flash(%d) vrr_idx:%d not prepared\n",
#ifdef CONFIG_SUPPORT_HMD
					(id == PANEL_BL_SUBDEV_TYPE_HMD) ? DIM_FLASH_HMD_GAMMA :
#endif
					DIM_FLASH_GAMMA, vrr_idx);
			continue;
		}

		for (i = 0; i < nr_luminance; i++) {
			ret = copy_from_dim_flash(panel_data,
					tbl->arr + maptbl_index(tbl, vrr_idx, i, 0),
#ifdef CONFIG_SUPPORT_HMD
					(id == PANEL_BL_SUBDEV_TYPE_HMD) ? DIM_FLASH_HMD_GAMMA :
#endif
					DIM_FLASH_GAMMA,
					vrr_idx, i, 0, sizeof_row(tbl));
		}
	}

	panel_info("panel_bl-%d done\n", id);

	return 0;
}

static int init_subdev_gamma_table_using_flash(struct maptbl *tbl, int id)
{
	struct panel_info *panel_data;
	struct panel_device *panel;
	struct panel_dimming_info *panel_dim_info;
	int ret;

	if (unlikely(!tbl || !tbl->pdata)) {
		panel_err("panel_bl-%d invalid param (tbl %p, pdata %p)\n",
				id, tbl, tbl ? tbl->pdata : NULL);
		return -EINVAL;
	}

	if (id >= MAX_PANEL_BL_SUBDEV) {
		panel_err("panel_bl-%d invalid id\n", id);
		return -EINVAL;
	}

	panel = tbl->pdata;
	panel_data = &panel->panel_data;

	panel_dim_info = panel_data->panel_dim_info[id];
	if (unlikely(!panel_dim_info)) {
		panel_err("panel_bl-%d panel_dim_info is null\n", id);
		return -EINVAL;
	}
	if (id == PANEL_BL_SUBDEV_TYPE_DISP)
		generate_brt_step_table(panel_dim_info->brt_tbl);

	/* initialize brightness_table */
	memcpy(&panel->panel_bl.subdev[id].brt_tbl,
			panel_dim_info->brt_tbl, sizeof(struct brightness_table));

	ret = generate_gamma_table_using_flash(panel_data, id);
	if (ret < 0) {
		panel_err("failed to generate gamma using flash\n");
		return ret;
	}

	if (id == PANEL_BL_SUBDEV_TYPE_DISP) {
		ret = generate_hbm_gamma_table(panel_data, id);
		if (ret < 0) {
			panel_err("failed to generate hbm gamma\n");
			return ret;
		}
	}

	panel_info("panel_bl-%d gamma_table initialized\n", id);
	print_gamma_table(panel_data, id);

	return 0;
}
#endif /* CONFIG_SUPPORT_DIM_FLASH */

static int init_subdev_gamma_table_using_lut(struct maptbl *tbl, int id)
{
	struct panel_info *panel_data;
	struct panel_device *panel;
	struct panel_dimming_info *panel_dim_info;
	int ret;

	if (unlikely(!tbl || !tbl->pdata)) {
		panel_err("panel_bl-%d invalid param (tbl %p, pdata %p)\n",
				id, tbl, tbl ? tbl->pdata : NULL);
		return -EINVAL;
	}

	if (id >= MAX_PANEL_BL_SUBDEV) {
		panel_err("panel_bl-%d invalid id\n", id);
		return -EINVAL;
	}

	panel = tbl->pdata;
	panel_data = &panel->panel_data;
	panel_dim_info = panel_data->panel_dim_info[id];
	if (unlikely(!panel_dim_info)) {
		panel_err("panel_bl-%d panel_dim_info is null\n", id);
		return -EINVAL;
	}
	if (id == PANEL_BL_SUBDEV_TYPE_DISP)
		generate_brt_step_table(panel_dim_info->brt_tbl);

	/* initialize brightness_table */
	memcpy(&panel->panel_bl.subdev[id].brt_tbl,
			panel_dim_info->brt_tbl, sizeof(struct brightness_table));

	ret = generate_gamma_table_using_lut(panel_data, id);
	if (ret < 0) {
		panel_err("failed to generate gamma using lut\n");
		return ret;
	}

	if (panel_dim_info->dimming_maptbl)
		maptbl_memcpy(tbl,
				&panel_dim_info->dimming_maptbl[DIMMING_GAMMA_MAPTBL]);

	if (id == PANEL_BL_SUBDEV_TYPE_DISP) {
		ret = generate_hbm_gamma_table(panel_data, id);
		if (ret < 0) {
			panel_err("failed to generate hbm gamma\n");
			return ret;
		}
	}

	panel_info("panel_bl-%d gamma_table initialized\n", id);
	print_gamma_table(panel_data, id);

	return 0;
}

#else
static int init_subdev_gamma_table_using_lut(struct maptbl *tbl, int id)
{
	panel_err("aid dimming unspported\n");
	return -ENODEV;
}
#endif /* CONFIG_PANEL_AID_DIMMING */

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

#if defined(__PANEL_NOT_USED_VARIABLE__)
static int init_mtp_table(struct maptbl *tbl)
{
	struct panel_device *panel = tbl->pdata;
	struct resinfo *res;
	int ret, size;

	res = find_panel_resource(&panel->panel_data, "mtp");
	if (unlikely(!res)) {
		panel_warn("resource(mtp) not found\n");
		return -EINVAL;
	}

	size = get_panel_resource_size(res);
	if (size <= 0 || size != sizeof_maptbl(tbl)) {
		panel_err("invalid resource size(%d) maptbl_size(%d)\n",
				size, sizeof_maptbl(tbl));
		return -EINVAL;
	}
	ret = resource_copy(tbl->arr, res);
	if (unlikely(ret < 0)) {
		panel_err("failed to copy mtp resource\n");
		return -EINVAL;
	}

	return 0;
}
#endif

static int init_gamma_table(struct maptbl *tbl)
{
#ifdef CONFIG_SUPPORT_DIM_FLASH
	struct panel_device *panel = tbl->pdata;
	struct panel_info *panel_data = &panel->panel_data;

	if (panel_data->props.cur_dim_type == DIM_TYPE_DIM_FLASH)
		return init_subdev_gamma_table_using_flash(tbl, PANEL_BL_SUBDEV_TYPE_DISP);
	else
		return init_subdev_gamma_table_using_lut(tbl, PANEL_BL_SUBDEV_TYPE_DISP);
#else
	return init_subdev_gamma_table_using_lut(tbl, PANEL_BL_SUBDEV_TYPE_DISP);
#endif
}

#ifdef CONFIG_EXYNOS_DECON_MDNIE_LITE
static int getidx_common_maptbl(struct maptbl *tbl)
{
	return 0;
}
#endif

static int getidx_dimming_maptbl(struct maptbl *tbl)
{
	struct panel_bl_device *panel_bl;
	struct panel_device *panel = (struct panel_device *)tbl->pdata;
	int index;

	if (panel == NULL) {
		panel_err("panel is null\n");
		return -EINVAL;
	}
	panel_bl = &panel->panel_bl;
	index = get_actual_brightness_index(panel_bl,
			panel_bl->props.brightness);

	return maptbl_index(tbl, 0, index, 0);
}

static int getidx_dimming_vrr_maptbl(struct maptbl *tbl)
{
	struct panel_bl_device *panel_bl;
	struct panel_device *panel = (struct panel_device *)tbl->pdata;
	int layer, row;

	if (panel == NULL) {
		panel_err("panel is null\n");
		return -EINVAL;
	}
	panel_bl = &panel->panel_bl;
	row = get_actual_brightness_index(panel_bl,
			panel_bl->props.brightness);

	layer = getidx_s6e3hab_current_vrr_dim(panel);
	if (layer < 0) {
		panel_err("vrr(%d%s) not found\n",
				get_panel_refresh_rate(panel),
				get_panel_refresh_mode(panel) ==
				VRR_HS_MODE ? "HS" : "NM");
		layer = 0;
	}

	return maptbl_index(tbl, layer, row, 0);
}

static int getidx_dimming_vrr_60hz_maptbl(struct maptbl *tbl)
{
	struct panel_bl_device *panel_bl;
	struct panel_device *panel = (struct panel_device *)tbl->pdata;
	int row, layer;

	if (panel == NULL) {
		panel_err("panel is null\n");
		return -EINVAL;
	}

	layer = getidx_s6e3hab_current_vrr_dim(panel);
	if (layer == S6E3HAB_VRR_DIM_120HS)
		layer = S6E3HAB_VRR_DIM_60NS;

	panel_bl = &panel->panel_bl;
	row = get_actual_brightness_index(panel_bl,
			panel_bl->props.brightness);

	return maptbl_index(tbl, layer, row, 0);
}

static int getidx_dimming_vrr_120hz_maptbl(struct maptbl *tbl)
{
	struct panel_bl_device *panel_bl;
	struct panel_device *panel = (struct panel_device *)tbl->pdata;
	int row, layer;

	if (panel == NULL) {
		panel_err("panel is null\n");
		return -EINVAL;
	}

	layer = getidx_s6e3hab_current_vrr_dim(panel);
	if (layer == S6E3HAB_VRR_DIM_60NS || layer == S6E3HAB_VRR_DIM_60HS)
		layer = S6E3HAB_VRR_DIM_120HS;

	panel_bl = &panel->panel_bl;
	row = get_actual_brightness_index(panel_bl,
			panel_bl->props.brightness);

	return maptbl_index(tbl, layer, row, 0);
}

#ifdef CONFIG_SUPPORT_HMD
static int init_hmd_gamma_table(struct maptbl *tbl)
{
	return init_subdev_gamma_table_using_lut(tbl, PANEL_BL_SUBDEV_TYPE_HMD);
}

static int getidx_hmd_dimming_maptbl(struct maptbl *tbl)
{
	struct panel_bl_device *panel_bl;
	struct panel_device *panel = (struct panel_device *)tbl->pdata;
	int layer, row;

	if (panel == NULL) {
		panel_err("panel is null\n");
		return -EINVAL;
	}

	panel_bl = &panel->panel_bl;
	row = get_subdev_actual_brightness_index(panel_bl,
			PANEL_BL_SUBDEV_TYPE_HMD,
			panel_bl->subdev[PANEL_BL_SUBDEV_TYPE_HMD].brightness);

	layer = getidx_s6e3hab_current_vrr_dim(panel);
	if (layer < 0)
		layer = 0;

	return maptbl_index(tbl, layer, row, 0);
}
#endif /* CONFIG_SUPPORT_HMD */

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

#ifdef CONFIG_SUPPORT_ISC_TUNE_TEST
static void get_stm_info(u8 *stm_field, u8 *stm_pack)
{
	stm_field[STM_CTRL_EN] = stm_pack[0];
	stm_field[STM_MAX_OPT] = (stm_pack[2] >> 4) & 0x0F;
	stm_field[STM_DEFAULT_OPT] = stm_pack[2] & 0x0F;
	stm_field[STM_DIM_STEP] = stm_pack[3] & 0x0F;
	stm_field[STM_FRAME_PERIOD] = (stm_pack[4] >> 4) & 0x0F;
	stm_field[STM_MIN_SECT] = stm_pack[4] & 0x0F;
	stm_field[STM_PIXEL_PERIOD] = (stm_pack[5] >> 4) & 0x0F;
	stm_field[STM_LINE_PERIOD] = stm_pack[5] & 0x0F;
	stm_field[STM_MIN_MOVE] = stm_pack[6];
	stm_field[STM_M_THRES] = stm_pack[7];
	stm_field[STM_V_THRES] = stm_pack[8];
}

static int init_stm_tune(struct maptbl *tbl)
{
	struct panel_info *panel_data;
	struct panel_device *panel;

	u8 stm_ref[] = {
	0x01, 0x08, 0x10, 0x21, 0x11, 0x42, 0x01, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00
	};

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

	get_stm_info(panel_data->props.stm_field_info, stm_ref);
	return 0;
}

static void set_stm_pack(u8 *stm_field, u8 *stm_pack)
{
	stm_pack[0] = stm_field[STM_CTRL_EN];
	stm_pack[2] = ((stm_field[STM_MAX_OPT] << 4) & 0xF0) | (stm_field[STM_DEFAULT_OPT] & 0x0F);
	stm_pack[3] |= (stm_field[STM_DIM_STEP] & 0x0F);
	stm_pack[4] = ((stm_field[STM_FRAME_PERIOD] << 4) & 0xF0) | (stm_field[STM_MIN_SECT] & 0x0F);
	stm_pack[5] = ((stm_field[STM_PIXEL_PERIOD] << 4) & 0xF0) | (stm_field[STM_LINE_PERIOD] & 0x0F);
	stm_pack[6] = stm_field[STM_MIN_MOVE];
	stm_pack[7] = stm_field[STM_M_THRES];
	stm_pack[8] = stm_field[STM_V_THRES];
}

static void copy_stm_tune_maptbl(struct maptbl *tbl, u8 *dst)
{
	struct panel_device *panel;
	struct panel_info *panel_data;

	if (!tbl || !dst)
		return;

	panel = (struct panel_device *)tbl->pdata;
	if (unlikely(!panel))
		return;

	panel_data = &panel->panel_data;
	set_stm_pack(panel_data->props.stm_field_info, dst);
}

static void copy_isc_threshold_maptbl(struct maptbl *tbl, u8 *dst)
{
	struct panel_device *panel;
	struct panel_info *panel_data;

	if (!tbl || !dst)
		return;

	panel = (struct panel_device *)tbl->pdata;
	if (unlikely(!panel))
		return;

	panel_data = &panel->panel_data;

	*dst = panel_data->props.isc_threshold;
}
#endif

static void copy_gamma_inter_control_maptbl(struct maptbl *tbl, u8 *dst)
{
	struct panel_device *panel;
	struct panel_info *panel_data;
	int i = 0;

	if (!tbl || !dst)
		return;

	panel = (struct panel_device *)tbl->pdata;
	if (unlikely(!panel))
		return;

	panel_data = &panel->panel_data;

	for (i  = 0; i < S6E3HAB_GAMMA_INTER_LEN; i++)
		dst[i] = panel_data->props.gamma_control_buf[i];
	for (i = 0; i < 6; i++)
		panel_info("%x %x\n", dst[i], panel_data->props.gamma_control_buf[i]);
}

//#define VRR_BRIDGE_GAMMA_DEBUG
#ifdef CONFIG_PANEL_VRR_BRIDGE
static int generate_interpolation_vrr_gamma(struct panel_info *panel_data,
		struct maptbl *gamma_maptbl, int lower_vrr_idx, int upper_vrr_idx, int fps, s32 *out)
{
	struct panel_device *panel = to_panel_device(panel_data);
	struct panel_bl_device *panel_bl = &panel->panel_bl;
	struct panel_dimming_info *panel_dim_info;
	struct resinfo *hbm_gamma_res[2];
	struct brightness_table *brt_tbl;
	s32 (*out_vrr_gamma_tbl)[MAX_COLOR];
	s32 (*(out_gamma_tbl[2]))[MAX_COLOR];
	s32 (*(lower_gamma_tbl[2]))[MAX_COLOR];
	s32 (*(upper_gamma_tbl[2]))[MAX_COLOR];
	int luminance, nr_luminance, nr_hbm_luminance, nr_extend_hbm_luminance;
	int id, nr_tp, brightness, ret = 0;
	int upper_idx, lower_idx, nor_max_idx, hbm_max_idx;
	u32 upper_lum, lower_lum, nor_max_lum, hbm_max_lum;
	u32 scaleup_upper_lum, scaleup_lower_lum, scaleup_nor_max_lum, scaleup_hbm_max_lum;
	u32 scaleup_target_lum, scaleup_hbm_target_lum;
	u32 upper_brt, lower_brt, nor_max_brt, hbm_max_brt;
	char *hbm_gamma_resource_names[MAX_S6E3HAB_VRR_DIM] = {
		[S6E3HAB_VRR_DIM_60NS] = "hbm_gamma",
		[S6E3HAB_VRR_DIM_60HS] = "hbm_gamma_60hz_hs",
		[S6E3HAB_VRR_DIM_120HS] = "hbm_gamma_120hz",
	};
#ifdef VRR_BRIDGE_GAMMA_DEBUG
	char buf[256];
	int i, c, len;
#endif

	if (lower_vrr_idx < 0 || lower_vrr_idx >= MAX_S6E3HAB_VRR_DIM ||
		!is_supported_vrr(panel_data, &S6E3HAB_VRR_DIM[lower_vrr_idx]))
		return -EINVAL;

	if (upper_vrr_idx < 0 || upper_vrr_idx >= MAX_S6E3HAB_VRR_DIM ||
		!is_supported_vrr(panel_data, &S6E3HAB_VRR_DIM[upper_vrr_idx]))
		return -EINVAL;

	id = panel_bl->props.id;
	panel_dim_info = panel_data->panel_dim_info[id];
	brt_tbl = &panel_bl->subdev[id].brt_tbl;
	brightness = panel_bl->props.brightness_of_step;

	nr_tp = panel_dim_info->dim_init_info.nr_tp;
	nr_luminance = panel_dim_info->nr_luminance;
	nr_hbm_luminance = panel_dim_info->nr_hbm_luminance;
	nr_extend_hbm_luminance = panel_dim_info->nr_extend_hbm_luminance;
	scaleup_target_lum = panel_dim_info->target_luminance * disp_pow(10, 3);
	scaleup_hbm_target_lum = panel_dim_info->hbm_target_luminance * disp_pow(10, 3);

	upper_idx = panel_bl->props.actual_brightness_index;
	lower_idx = max(0, (upper_idx - 1));
	upper_lum = brt_tbl->lum[upper_idx];
	lower_lum = brt_tbl->lum[lower_idx];
	scaleup_upper_lum = upper_lum * disp_pow(10, 3);
	scaleup_lower_lum = lower_lum * disp_pow(10, 3);
	upper_brt = brt_tbl->brt[upper_idx];
	lower_brt = brt_tbl->brt[lower_idx];

	hbm_max_idx = nr_luminance + nr_hbm_luminance - 1;
	nor_max_idx = nr_luminance - 1;
	hbm_max_lum = brt_tbl->lum[hbm_max_idx];
	nor_max_lum = brt_tbl->lum[nor_max_idx];
	scaleup_hbm_max_lum = hbm_max_lum * disp_pow(10, 3);
	scaleup_nor_max_lum = nor_max_lum * disp_pow(10, 3);
	hbm_max_brt = brt_tbl->brt[hbm_max_idx];
	nor_max_brt = brt_tbl->brt[nor_max_idx];

	hbm_gamma_res[0] = find_panel_resource(panel_data,
			hbm_gamma_resource_names[lower_vrr_idx]);
	if (unlikely(!hbm_gamma_res[0])) {
		panel_warn("panel_bl-%d %s not found\n",
				id, hbm_gamma_resource_names[lower_vrr_idx]);
		return -EIO;
	}

	hbm_gamma_res[1] = find_panel_resource(panel_data,
			hbm_gamma_resource_names[upper_vrr_idx]);
	if (unlikely(!hbm_gamma_res[1])) {
		panel_warn("panel_bl-%d %s not found\n",
				id, hbm_gamma_resource_names[upper_vrr_idx]);
		return -EIO;
	}

	out_vrr_gamma_tbl = kzalloc(sizeof(s32) * nr_tp * MAX_COLOR, GFP_KERNEL);

	out_gamma_tbl[0] = kzalloc(sizeof(s32) * nr_tp * MAX_COLOR, GFP_KERNEL);
	upper_gamma_tbl[0] = kzalloc(sizeof(s32) * nr_tp * MAX_COLOR, GFP_KERNEL);
	lower_gamma_tbl[0] = kzalloc(sizeof(s32) * nr_tp * MAX_COLOR, GFP_KERNEL);

	out_gamma_tbl[1] = kzalloc(sizeof(s32) * nr_tp * MAX_COLOR, GFP_KERNEL);
	upper_gamma_tbl[1] = kzalloc(sizeof(s32) * nr_tp * MAX_COLOR, GFP_KERNEL);
	lower_gamma_tbl[1] = kzalloc(sizeof(s32) * nr_tp * MAX_COLOR, GFP_KERNEL);

	luminance = interpolation(scaleup_lower_lum, scaleup_upper_lum,
			(s32)((u64)brightness - lower_brt), (s32)(upper_brt - lower_brt));
#ifdef VRR_BRIDGE_GAMMA_DEBUG
	panel_info("fps:%d luminance:%d brightness:%d\n",
			fps, luminance, brightness);
#endif

	if (luminance <= scaleup_target_lum) {
		/* ~ UI MAX BRIGHTNESS */
		gamma_ctoi(lower_gamma_tbl[0], &gamma_maptbl->arr[
				maptbl_index(gamma_maptbl, lower_vrr_idx, lower_idx, 0)], nr_tp);
		gamma_ctoi(upper_gamma_tbl[0], &gamma_maptbl->arr[
				maptbl_index(gamma_maptbl, lower_vrr_idx, upper_idx, 0)], nr_tp);
		gamma_table_interpolation_round(lower_gamma_tbl[0], upper_gamma_tbl[0], out_gamma_tbl[0],
				nr_tp, luminance - scaleup_lower_lum, scaleup_upper_lum - scaleup_lower_lum);
#ifdef VRR_BRIDGE_GAMMA_DEBUG
		panel_info("maptbl_idx:%d lower_vrr_idx:%d lower_idx:%d\n",
				maptbl_index(gamma_maptbl, lower_vrr_idx, lower_idx, 0),
				lower_vrr_idx, lower_idx);
		panel_info("maptbl_idx:%d lower_vrr_idx:%d upper_idx:%d\n",
				maptbl_index(gamma_maptbl, lower_vrr_idx, upper_idx, 0),
				lower_vrr_idx, upper_idx);

		len = 0;
		for (i = 0; i < nr_tp; i++)
			for (c = 0; c < MAX_COLOR; c++)
				len += snprintf(buf + len, ARRAY_SIZE(buf) - len, "%d ", lower_gamma_tbl[0][i][c]);
		panel_info("vrr:%d-lower[0]: %s\n", lower_vrr_idx, buf);

		len = 0;
		for (i = 0; i < nr_tp; i++)
			for (c = 0; c < MAX_COLOR; c++)
				len += snprintf(buf + len, ARRAY_SIZE(buf) - len, "%d ", upper_gamma_tbl[0][i][c]);
		panel_info("vrr:%d-upper[0]: %s\n", lower_vrr_idx, buf);

		len = 0;
		for (i = 0; i < nr_tp; i++)
			for (c = 0; c < MAX_COLOR; c++)
				len += snprintf(buf + len, ARRAY_SIZE(buf) - len, "%d ", out_gamma_tbl[0][i][c]);
		panel_info("vrr:%d-out[0]: %s\n", lower_vrr_idx, buf);
#endif
		gamma_ctoi(lower_gamma_tbl[1], &gamma_maptbl->arr[
				maptbl_index(gamma_maptbl, upper_vrr_idx, lower_idx, 0)], nr_tp);
		gamma_ctoi(upper_gamma_tbl[1], &gamma_maptbl->arr[
				maptbl_index(gamma_maptbl, upper_vrr_idx, upper_idx, 0)], nr_tp);
		gamma_table_interpolation_round(lower_gamma_tbl[1], upper_gamma_tbl[1], out_gamma_tbl[1],
				nr_tp, luminance - scaleup_lower_lum, scaleup_upper_lum - scaleup_lower_lum);
#ifdef VRR_BRIDGE_GAMMA_DEBUG
		panel_info("maptbl_idx:%d upper_vrr_idx:%d lower_idx:%d\n",
				maptbl_index(gamma_maptbl, upper_vrr_idx, lower_idx, 0),
				upper_vrr_idx, lower_idx);
		panel_info("maptbl_idx:%d upper_vrr_idx:%d upper_idx:%d\n",
				maptbl_index(gamma_maptbl, upper_vrr_idx, upper_idx, 0),
				upper_vrr_idx, upper_idx);

		len = 0;
		for (i = 0; i < nr_tp; i++)
			for (c = 0; c < MAX_COLOR; c++)
				len += snprintf(buf + len, ARRAY_SIZE(buf) - len, "%d ", lower_gamma_tbl[1][i][c]);
		panel_info("vrr:%d-lower[1]: %s\n", lower_vrr_idx, buf);

		len = 0;
		for (i = 0; i < nr_tp; i++)
			for (c = 0; c < MAX_COLOR; c++)
				len += snprintf(buf + len, ARRAY_SIZE(buf) - len, "%d ", upper_gamma_tbl[1][i][c]);
		panel_info("vrr:%d-upper[1]: %s\n", lower_vrr_idx, buf);

		len = 0;
		for (i = 0; i < nr_tp; i++)
			for (c = 0; c < MAX_COLOR; c++)
				len += snprintf(buf + len, ARRAY_SIZE(buf) - len, "%d ", out_gamma_tbl[1][i][c]);
		panel_info("vrr:%d-out[1]: %s\n", lower_vrr_idx, buf);
#endif
		if (lower_vrr_idx == upper_vrr_idx) {
#ifdef VRR_BRIDGE_GAMMA_DEBUG
			panel_info("memcpy out_gamma_tbl[0]\n");
#endif
			memcpy(out_vrr_gamma_tbl, out_gamma_tbl[0], sizeof(s32) * nr_tp * MAX_COLOR);
		} else {
#ifdef VRR_BRIDGE_GAMMA_DEBUG
			panel_info("gamma_table_interpolation_round(cur_fps:%d lower_fps:%d upper_fps:%d)\n",
					fps, S6E3HAB_VRR_DIM[lower_vrr_idx].fps,
					S6E3HAB_VRR_DIM[upper_vrr_idx].fps);
#endif
			gamma_table_interpolation_round(out_gamma_tbl[0], out_gamma_tbl[1], out_vrr_gamma_tbl, nr_tp,
					abs(fps - S6E3HAB_VRR_DIM[lower_vrr_idx].fps),
					abs(S6E3HAB_VRR_DIM[upper_vrr_idx].fps - S6E3HAB_VRR_DIM[lower_vrr_idx].fps));
		}
	} else if (luminance <= scaleup_hbm_target_lum) {
		/* UI MAX BRIGHTNESS ~ HBM MAX BRIGHTNESS */
		gamma_ctoi(lower_gamma_tbl[0], &gamma_maptbl->arr[
				maptbl_index(gamma_maptbl, lower_vrr_idx, nor_max_idx, 0)], nr_tp);
		gamma_ctoi(upper_gamma_tbl[0], hbm_gamma_res[0]->data, nr_tp);
		gamma_table_interpolation_round(lower_gamma_tbl[0], upper_gamma_tbl[0], out_gamma_tbl[0],
				nr_tp, luminance - scaleup_nor_max_lum, scaleup_hbm_max_lum - scaleup_nor_max_lum);

		gamma_ctoi(lower_gamma_tbl[1], &gamma_maptbl->arr[
				maptbl_index(gamma_maptbl, upper_vrr_idx, nor_max_idx, 0)], nr_tp);
		gamma_ctoi(upper_gamma_tbl[1], hbm_gamma_res[1]->data, nr_tp);
		gamma_table_interpolation_round(lower_gamma_tbl[1], upper_gamma_tbl[1], out_gamma_tbl[1],
				nr_tp, luminance - scaleup_nor_max_lum, scaleup_hbm_max_lum - scaleup_nor_max_lum);

		if (lower_vrr_idx == upper_vrr_idx)
			memcpy(out_vrr_gamma_tbl, out_gamma_tbl[0], sizeof(s32) * nr_tp * MAX_COLOR);
		else
			gamma_table_interpolation_round(out_gamma_tbl[0], out_gamma_tbl[1], out_vrr_gamma_tbl, nr_tp,
					abs(fps - S6E3HAB_VRR_DIM[lower_vrr_idx].fps),
					abs(S6E3HAB_VRR_DIM[upper_vrr_idx].fps - S6E3HAB_VRR_DIM[lower_vrr_idx].fps));
	} else {
		/* UI MAX BRIGHTNESS ~ EXTENDED HBM MAX BRIGHTNESS */
		gamma_ctoi(upper_gamma_tbl[0], hbm_gamma_res[0]->data, nr_tp);
		memcpy(out_vrr_gamma_tbl, upper_gamma_tbl[0], sizeof(s32) * nr_tp * MAX_COLOR);
	}

	memcpy(out, out_vrr_gamma_tbl, sizeof(s32) * nr_tp * MAX_COLOR);
	//gamma_itoc(dst, out_vrr_gamma_tbl, nr_tp);

	kfree(out_gamma_tbl[0]);
	kfree(lower_gamma_tbl[0]);
	kfree(upper_gamma_tbl[0]);

	kfree(out_gamma_tbl[1]);
	kfree(lower_gamma_tbl[1]);
	kfree(upper_gamma_tbl[1]);

	kfree(out_vrr_gamma_tbl);

	panel_dbg("brightness:%d fps:%d vrr(%d %d)\n",
			brightness, fps, lower_vrr_idx, upper_vrr_idx);

	return ret;
}
#endif

static int generate_interpolation_gamma(struct panel_info *panel_data,
		struct maptbl *gamma_maptbl, s32 *out)
{
	struct panel_device *panel = to_panel_device(panel_data);
	struct panel_bl_device *panel_bl = &panel->panel_bl;
	struct panel_dimming_info *panel_dim_info;
	struct resinfo *hbm_gamma_res;
	struct brightness_table *brt_tbl;
	s32 (*out_gamma_tbl)[MAX_COLOR] = NULL;
	s32 (*lower_gamma_tbl)[MAX_COLOR] = NULL;
	s32 (*upper_gamma_tbl)[MAX_COLOR] = NULL;
	int luminance, nr_luminance, nr_hbm_luminance, nr_extend_hbm_luminance;
	int id, nr_tp, brightness, ret = 0;
	int upper_idx, lower_idx, nor_max_idx, hbm_max_idx;
	u32 upper_lum, lower_lum, nor_max_lum, hbm_max_lum;
	u32 scaleup_upper_lum, scaleup_lower_lum, scaleup_nor_max_lum, scaleup_hbm_max_lum;
	u32 scaleup_target_lum, scaleup_hbm_target_lum;
	u32 upper_brt, lower_brt, nor_max_brt, hbm_max_brt;
	char *hbm_gamma_resource_names[MAX_S6E3HAB_VRR_DIM] = {
		[S6E3HAB_VRR_DIM_60NS] = "hbm_gamma",
		[S6E3HAB_VRR_DIM_60HS] = "hbm_gamma_60hz_hs",
		[S6E3HAB_VRR_DIM_120HS] = "hbm_gamma_120hz",
	};
	int vrr_idx = 0;

	id = panel_bl->props.id;
	panel_dim_info = panel_data->panel_dim_info[id];
	brt_tbl = &panel_bl->subdev[id].brt_tbl;
	brightness = panel_bl->props.brightness_of_step;

	nr_tp = panel_dim_info->dim_init_info.nr_tp;
	nr_luminance = panel_dim_info->nr_luminance;
	nr_hbm_luminance = panel_dim_info->nr_hbm_luminance;
	nr_extend_hbm_luminance = panel_dim_info->nr_extend_hbm_luminance;
	scaleup_target_lum = panel_dim_info->target_luminance * disp_pow(10, 3);
	scaleup_hbm_target_lum = panel_dim_info->hbm_target_luminance * disp_pow(10, 3);

	upper_idx = panel_bl->props.actual_brightness_index;
	lower_idx = max(0, (upper_idx - 1));
	upper_lum = brt_tbl->lum[upper_idx];
	lower_lum = brt_tbl->lum[lower_idx];
	scaleup_upper_lum = upper_lum * disp_pow(10, 3);
	scaleup_lower_lum = lower_lum * disp_pow(10, 3);
	upper_brt = brt_tbl->brt[upper_idx];
	lower_brt = brt_tbl->brt[lower_idx];

	hbm_max_idx = nr_luminance + nr_hbm_luminance - 1;
	nor_max_idx = nr_luminance - 1;
	hbm_max_lum = brt_tbl->lum[hbm_max_idx];
	nor_max_lum = brt_tbl->lum[nor_max_idx];
	scaleup_hbm_max_lum = hbm_max_lum * disp_pow(10, 3);
	scaleup_nor_max_lum = nor_max_lum * disp_pow(10, 3);
	hbm_max_brt = brt_tbl->brt[hbm_max_idx];
	nor_max_brt = brt_tbl->brt[nor_max_idx];

	vrr_idx = getidx_s6e3hab_current_vrr_dim(panel);
	if (vrr_idx < 0)
		vrr_idx = 0;

	hbm_gamma_res = find_panel_resource(panel_data,
			hbm_gamma_resource_names[vrr_idx]);
	if (unlikely(!hbm_gamma_res)) {
		panel_warn("panel_bl-%d %s not found\n",
				id, hbm_gamma_resource_names[vrr_idx]);
		return -EIO;
	}

	out_gamma_tbl = kzalloc(sizeof(s32) * nr_tp * MAX_COLOR, GFP_KERNEL);
	upper_gamma_tbl = kzalloc(sizeof(s32) * nr_tp * MAX_COLOR, GFP_KERNEL);
	lower_gamma_tbl = kzalloc(sizeof(s32) * nr_tp * MAX_COLOR, GFP_KERNEL);

	luminance = interpolation(scaleup_lower_lum, scaleup_upper_lum,
			(s32)((u64)brightness - lower_brt), (s32)(upper_brt - lower_brt));
	panel_dbg("brt:%d up:%d lower:%d lum:%d up:%d lower:%d\n",
		brightness,	upper_brt, lower_brt,
		luminance, scaleup_upper_lum, scaleup_lower_lum);
	if (luminance <= scaleup_target_lum) {
		/* ~ UI MAX BRIGHTNESS */
		gamma_ctoi(lower_gamma_tbl, &gamma_maptbl->arr[
				maptbl_index(gamma_maptbl, vrr_idx, lower_idx, 0)], nr_tp);
		gamma_ctoi(upper_gamma_tbl, &gamma_maptbl->arr[
				maptbl_index(gamma_maptbl, vrr_idx, upper_idx, 0)], nr_tp);
		gamma_table_interpolation_round(lower_gamma_tbl, upper_gamma_tbl, out_gamma_tbl,
				nr_tp, luminance - scaleup_lower_lum, scaleup_upper_lum - scaleup_lower_lum);
	} else if (luminance <= scaleup_hbm_target_lum) {
		/* UI MAX BRIGHTNESS ~ HBM MAX BRIGHTNESS */
		gamma_ctoi(lower_gamma_tbl, &gamma_maptbl->arr[
				maptbl_index(gamma_maptbl, vrr_idx, nor_max_idx, 0)], nr_tp);
		gamma_ctoi(upper_gamma_tbl, hbm_gamma_res->data, nr_tp);
		gamma_table_interpolation_round(lower_gamma_tbl, upper_gamma_tbl, out_gamma_tbl,
				nr_tp, luminance - scaleup_nor_max_lum, scaleup_hbm_max_lum - scaleup_nor_max_lum);
	} else {
		/* UI MAX BRIGHTNESS ~ EXTENDED HBM MAX BRIGHTNESS */
		gamma_ctoi(upper_gamma_tbl, hbm_gamma_res->data, nr_tp);
		memcpy(out_gamma_tbl, upper_gamma_tbl, sizeof(s32) * nr_tp * MAX_COLOR);
	}

	memcpy(out, out_gamma_tbl, sizeof(s32) * nr_tp * MAX_COLOR);
	//gamma_itoc(dst, out_gamma_tbl, nr_tp);

	kfree(out_gamma_tbl);
	kfree(lower_gamma_tbl);
	kfree(upper_gamma_tbl);

	panel_dbg("brightness:%d\n", brightness);

	return ret;
}

static bool is_adaptive_sync_vrr(int fps, int mode)
{
	static int adaptive_sync_vrr_table[][2] = {
		{ 48, VRR_NORMAL_MODE },
		{ 52, VRR_NORMAL_MODE },
		{ 56, VRR_NORMAL_MODE },
		{ 96, VRR_HS_MODE },
		{ 104, VRR_HS_MODE },
		{ 112, VRR_HS_MODE },
	};
	int i;

	if (fps < 60)
		return true;

	for (i = 0; i < ARRAY_SIZE(adaptive_sync_vrr_table); i++)
		if (fps == adaptive_sync_vrr_table[i][0] &&
			mode == adaptive_sync_vrr_table[i][1])
			return true;

	return false;
}

static void copy_gamma_maptbl(struct maptbl *tbl, u8 *dst)
{
	struct panel_device *panel;
	struct panel_info *panel_data;
	struct panel_bl_device *panel_bl;
	struct panel_dimming_info *panel_dim_info;
	int id, nr_tp, ret;
	int vrr_fps, vrr_mode;
	s32 *out;
#ifdef CONFIG_PANEL_VRR_BRIDGE
	int i;
	s32 *(out_gamma_tbl[5]);
#ifdef VRR_BRIDGE_GAMMA_DEBUG
	char buf[256], *p;
#endif
#endif

	if (!tbl || !dst || !tbl->pdata) {
		panel_err("invalid parameter (tbl %p, dst %p tbl->pdata %p)\n",
				tbl, dst, tbl->pdata);
		return;
	}

	panel = (struct panel_device *)tbl->pdata;
	panel_data = &panel->panel_data;
	panel_bl = &panel->panel_bl;
	id = panel_bl->props.id;
	panel_dim_info = panel_data->panel_dim_info[id];
	nr_tp = panel_dim_info->dim_init_info.nr_tp;
	vrr_fps = get_panel_refresh_rate(panel);
	vrr_mode = get_panel_refresh_mode(panel);

	out = kzalloc(sizeof(s32) * nr_tp * MAX_COLOR, GFP_KERNEL);

#ifdef CONFIG_PANEL_VRR_BRIDGE
	if (is_supported_vrr(panel_data, &S6E3HAB_VRR_DIM[S6E3HAB_VRR_DIM_60HS]) &&
		is_supported_vrr(panel_data, &S6E3HAB_VRR_DIM[S6E3HAB_VRR_DIM_120HS])) {
		if (is_adaptive_sync_vrr(vrr_fps, vrr_mode)) {
			for (i = 0; i < ARRAY_SIZE(out_gamma_tbl); i++)
				out_gamma_tbl[i] = kzalloc(sizeof(s32) * nr_tp * MAX_COLOR, GFP_KERNEL);

			if (vrr_fps < 60) {
				generate_interpolation_vrr_gamma(panel_data, tbl,
						S6E3HAB_VRR_DIM_60HS, S6E3HAB_VRR_DIM_120HS, 110, out_gamma_tbl[0]);
				generate_interpolation_vrr_gamma(panel_data, tbl,
						S6E3HAB_VRR_DIM_60NS, S6E3HAB_VRR_DIM_60NS, 60, out_gamma_tbl[1]);
				generate_interpolation_vrr_gamma(panel_data, tbl,
						S6E3HAB_VRR_DIM_120HS, S6E3HAB_VRR_DIM_120HS, 120, out_gamma_tbl[2]);
				generate_interpolation_vrr_gamma(panel_data, tbl,
						S6E3HAB_VRR_DIM_60HS, S6E3HAB_VRR_DIM_60HS, 60, out_gamma_tbl[3]);
#ifdef VRR_BRIDGE_GAMMA_DEBUG
				p = buf;
				for (i = 0; i < nr_tp * MAX_COLOR; i++)
					p += sprintf(p, "%d ", out_gamma_tbl[3][i]);
				panel_info("60hs: %s\n", buf);

				p = buf;
				for (i = 0; i < nr_tp * MAX_COLOR; i++)
					p += sprintf(p, "%d ", out_gamma_tbl[0][i]);
				panel_info("110hs: %s\n", buf);

				p = buf;
				for (i = 0; i < nr_tp * MAX_COLOR; i++)
					p += sprintf(p, "%d ", out_gamma_tbl[2][i]);
				panel_info("120hs: %s\n", buf);

				p = buf;
				for (i = 0; i < nr_tp * MAX_COLOR; i++)
					p += sprintf(p, "%d ", out_gamma_tbl[1][i]);
				panel_info("60nm: %s\n", buf);
#endif
				for (i = 0; i < nr_tp * MAX_COLOR; i++)
					out_gamma_tbl[0][i] =
						(out_gamma_tbl[2][i] == 0) ? 0 :
						(out_gamma_tbl[0][i] * out_gamma_tbl[1][i]
							+ out_gamma_tbl[2][i] / 2) / out_gamma_tbl[2][i];
#ifdef VRR_BRIDGE_GAMMA_DEBUG
				p = buf;
				for (i = 0; i < nr_tp * MAX_COLOR; i++)
					p += sprintf(p, "%d ", out_gamma_tbl[0][i]);
				panel_info("48nm: %s\n", buf);
#endif
				gamma_table_interpolation_round((s32 (*)[MAX_COLOR])out_gamma_tbl[0],
						(s32 (*)[MAX_COLOR])out_gamma_tbl[1], (s32 (*)[MAX_COLOR])out_gamma_tbl[2],
						nr_tp, vrr_fps - 48, 60 - 48);
#ifdef VRR_BRIDGE_GAMMA_DEBUG
				p = buf;
				for (i = 0; i < nr_tp * MAX_COLOR; i++)
					p += sprintf(p, "%d ", out_gamma_tbl[2][i]);
				panel_info("%d%s: %s\n", vrr_fps,
						vrr_mode == VRR_NORMAL_MODE ? "nm" : "hs", buf);
#endif
				for (i = 0; i < nr_tp * MAX_COLOR; i++)
					out[i] = out_gamma_tbl[2][i];
			} else if (get_actual_brightness(panel_bl, panel_bl->props.brightness) < 98) {
				generate_interpolation_vrr_gamma(panel_data, tbl,
						S6E3HAB_VRR_DIM_60HS, S6E3HAB_VRR_DIM_120HS, 110, out_gamma_tbl[0]);
				generate_interpolation_vrr_gamma(panel_data, tbl,
						S6E3HAB_VRR_DIM_120HS, S6E3HAB_VRR_DIM_120HS, 120, out_gamma_tbl[1]);
				gamma_table_interpolation_round((s32 (*)[MAX_COLOR])out_gamma_tbl[0],
						(s32 (*)[MAX_COLOR])out_gamma_tbl[1], (s32 (*)[MAX_COLOR])out_gamma_tbl[2],
						nr_tp, vrr_fps - 96, 120 - 96);
#ifdef VRR_BRIDGE_GAMMA_DEBUG
				p = buf;
				for (i = 0; i < nr_tp * MAX_COLOR; i++)
					p += sprintf(p, "%d ", out_gamma_tbl[0][i]);
				panel_info("110hs: %s\n", buf);

				p = buf;
				for (i = 0; i < nr_tp * MAX_COLOR; i++)
					p += sprintf(p, "%d ", out_gamma_tbl[1][i]);
				panel_info("120hs: %s\n", buf);

				p = buf;
				for (i = 0; i < nr_tp * MAX_COLOR; i++)
					p += sprintf(p, "%d ", out_gamma_tbl[2][i]);
				panel_info("%d%s: %s\n", vrr_fps,
						vrr_mode == VRR_NORMAL_MODE ? "nm" : "hs", buf);
#endif
				for (i = 0; i < nr_tp * MAX_COLOR; i++)
					out[i] = out_gamma_tbl[2][i];
			} else {
				generate_interpolation_vrr_gamma(panel_data, tbl,
						S6E3HAB_VRR_DIM_60HS, S6E3HAB_VRR_DIM_120HS, 100, out_gamma_tbl[0]);
				generate_interpolation_vrr_gamma(panel_data, tbl,
						S6E3HAB_VRR_DIM_120HS, S6E3HAB_VRR_DIM_120HS, 120, out_gamma_tbl[1]);
				gamma_table_interpolation_round((s32 (*)[MAX_COLOR])out_gamma_tbl[0],
						(s32 (*)[MAX_COLOR])out_gamma_tbl[1], (s32 (*)[MAX_COLOR])out_gamma_tbl[2],
						nr_tp, vrr_fps - 96, 120 - 96);
#ifdef VRR_BRIDGE_GAMMA_DEBUG
				p = buf;
				for (i = 0; i < nr_tp * MAX_COLOR; i++)
					p += sprintf(p, "%d ", out_gamma_tbl[0][i]);
				panel_info("100hs: %s\n", buf);

				p = buf;
				for (i = 0; i < nr_tp * MAX_COLOR; i++)
					p += sprintf(p, "%d ", out_gamma_tbl[1][i]);
				panel_info("120hs: %s\n", buf);

				p = buf;
				for (i = 0; i < nr_tp * MAX_COLOR; i++)
					p += sprintf(p, "%d ", out_gamma_tbl[2][i]);
				panel_info("%d%s: %s\n", vrr_fps,
						vrr_mode == VRR_NORMAL_MODE ? "nm" : "hs", buf);
#endif
				for (i = 0; i < nr_tp * MAX_COLOR; i++)
					out[i] = out_gamma_tbl[2][i];
			}

			for (i = 0; i < ARRAY_SIZE(out_gamma_tbl); i++)
				kfree(out_gamma_tbl[i]);
		} else if (vrr_fps != 60 && vrr_fps != 120) {
			ret = generate_interpolation_vrr_gamma(panel_data, tbl,
					S6E3HAB_VRR_DIM_60HS, S6E3HAB_VRR_DIM_120HS,
					vrr_fps, out);
#ifdef VRR_BRIDGE_GAMMA_DEBUG
			p = buf;
			for (i = 0; i < nr_tp * MAX_COLOR; i++)
				p += sprintf(p, "%d ", out[i]);
			panel_info("%d%s: %s\n", vrr_fps,
					vrr_mode == VRR_NORMAL_MODE ? "nm" : "hs", buf);
#endif
			if (ret < 0) {
				panel_err("failed to generate_interpolation_vrr_gamma (ret %d)\n",
						ret);
				goto exit;
			}
		} else {
			ret = generate_interpolation_gamma(&panel->panel_data, tbl, out);
			if (ret < 0) {
				panel_err("failed to generate_interpolation_gamma (ret %d)\n",
						ret);
				goto exit;
			}
#ifdef VRR_BRIDGE_GAMMA_DEBUG
			p = buf;
			for (i = 0; i < nr_tp * MAX_COLOR; i++)
				p += sprintf(p, "%d ", out[i]);
			panel_info("%d%s: %s\n", vrr_fps,
					vrr_mode == VRR_NORMAL_MODE ? "nm" : "hs", buf);
#endif
		}
	} else {
		ret = generate_interpolation_gamma(&panel->panel_data, tbl, out);
		if (ret < 0) {
			panel_err("failed to generate_interpolation_gamma (ret %d)\n",
					ret);
			goto exit;
		}
	}
#else
	ret = generate_interpolation_gamma(&panel->panel_data, tbl, out);
	if (ret < 0) {
		panel_err("failed to generate_interpolation_gamma (ret %d)\n",
				ret);
		goto exit;
	}
#endif
	gamma_itoc(dst, (s32 (*)[MAX_COLOR])out, nr_tp);

exit:
	kfree(out);
}

static void copy_aor_maptbl(struct maptbl *tbl, u8 *dst)
{
	struct panel_device *panel;
	struct panel_bl_device *panel_bl;
	struct brightness_table *brt_tbl;
	struct panel_info *panel_data;
	struct panel_dimming_info *panel_dim_info;
	int id, old_aor, aor, aor_offset = 0, layer, vfp, vtotal, base_vtotal;
	int vrr_fps, vrr_mode;
	struct panel_vrr *vrr;
	u8(*aor_tbl)[2];

	if (!tbl || !dst) {
		panel_err("invalid parameter (tbl %p, dst %p)\n",
				tbl, dst);
		return;
	}

	panel = (struct panel_device *)tbl->pdata;
	panel_data = &panel->panel_data;
	panel_bl = &panel->panel_bl;
	id = panel_bl->props.id;
	brt_tbl = &panel_bl->subdev[id].brt_tbl;
	panel_dim_info = panel_data->panel_dim_info[id];
	vrr_fps = get_panel_refresh_rate(panel);
	vrr_mode = get_panel_refresh_mode(panel);

	layer = getidx_s6e3hab_current_vrr_dim(panel);
	if (layer < 0) {
		panel_err("failed to search vrr\n");
		layer = 0;
	}
	vrr = &S6E3HAB_VRR_DIM[layer];

	aor_tbl = (u8 (*)[2])(tbl->arr + maptbl_index(tbl, layer, 0, 0));
	aor = panel_bl_aor_interpolation_2(panel_bl, id, aor_tbl);
	if (aor < 0) {
		panel_err("invalid aor %d\n", aor);
		return;
	}

	old_aor = aor;
	if ((panel_data->id[2] & 0x0F) >= 3) {
		if (vrr_fps != 60 && vrr_fps != 120) {
			/* gamma & aor interpolation */
			vfp = calc_vfp(vrr, vrr_fps);
			if (vfp < 0) {
				panel_err("invalid vfp %d\n", vfp);
				return;
			}
			if (is_adaptive_sync_vrr(vrr_fps,
						vrr_mode))
				aor_offset = 0;
			else if (vrr_fps == 70)
				aor_offset = 14;
			else if (vrr_fps == 100)
				aor_offset = -1;
			else if (vrr_fps == 110)
				aor_offset = 0;

			vtotal = vrr->base_vbp + vrr->base_vactive + vfp;
			base_vtotal = vrr->base_vbp + vrr->base_vactive + vrr->base_vfp;
			if (base_vtotal == 0) {
				panel_warn("base_vtotal is zero!!\n");
				return;
			}
			aor = ((aor * vtotal) / base_vtotal) + aor_offset;
			panel_dbg("aor interpolation(aor:%d aor_offset:%d fps:%d vfp:%d vtotal:%d)\n",
					aor, aor_offset, vrr_fps, vfp, vtotal);
		} else {
			vfp = vrr->base_vfp;
			vtotal = vrr->base_vbp + vrr->base_vactive + vrr->base_vfp;
			panel_dbg("aor from table(aor:%d fps:%d vfp:%d vtotal:%d)\n",
					aor, vrr_fps, vfp, vtotal);
		}
	} else {
		if (is_adaptive_sync_vrr(vrr_fps,
					vrr_mode)) {
			/* gamma & aor interpolation */
			vfp = calc_vfp(vrr, vrr_fps);
			if (vfp < 0) {
				panel_err("invalid vfp %d\n", vfp);
				return;
			}
			vtotal = vrr->base_vbp + vrr->base_vactive + vfp;
			base_vtotal = vrr->base_vbp + vrr->base_vactive + vrr->base_vfp;
			if (base_vtotal == 0) {
				panel_warn("base_vtotal is zero!!\n");
				return;
			}
			aor = (aor * vtotal) / base_vtotal;
			panel_dbg("aor interpolation(aor:%d fps:%d vfp:%d vtotal:%d)\n",
					aor, vrr_fps, vfp, vtotal);
		} else {
			vfp = vrr->base_vfp;
			vtotal = vrr->base_vbp + vrr->base_vactive + vrr->base_vfp;
			panel_dbg("aor from table(aor:%d fps:%d vfp:%d vtotal:%d)\n",
					aor, vrr_fps, vfp, vtotal);
		}
	}

	dst[0] = (aor >> 8) & 0xFF;
	dst[1] = aor & 0xFF;

	if (panel_dim_info->hbm_aor && is_hbm_brightness(panel_bl, panel_bl->props.brightness)) {
		dst[0] = panel_dim_info->hbm_aor[0];
		dst[1] = panel_dim_info->hbm_aor[1];
		aor = (dst[0] << 8) + dst[1];
	}
	panel_bl->props.aor_ratio = AOR_TO_RATIO(aor, vtotal);

	panel_dbg("aor %d->%d(%d) offset(%d), vfp %d vrr(%d %d)\n",
			old_aor, aor, aor_offset, panel_bl->props.aor_ratio, vfp,
			vrr_fps, vrr_mode);
}

static void copy_irc_maptbl(struct maptbl *tbl, u8 *dst)
{
	struct panel_device *panel;
	struct panel_bl_device *panel_bl;
	struct brightness_table *brt_tbl;
	struct panel_irc_info *irc_info;
	struct panel_dimming_info *panel_dim_info;
	int id, brightness, ret;

	if (!tbl || !dst) {
		panel_err("invalid parameter (tbl %p, dst %p)\n",
				tbl, dst);
		return;
	}

	panel = (struct panel_device *)tbl->pdata;
#ifdef CONFIG_SUPPORT_HMD
	if (panel->state.hmd_on == PANEL_HMD_ON) {
		panel_info("don't support HMD ON\n");
		return;
	}
#endif
	panel_bl = &panel->panel_bl;
	id = panel_bl->props.id;
	brightness = panel_bl->props.brightness;
	brt_tbl = &panel_bl->subdev[id].brt_tbl;
	panel_dim_info = panel->panel_data.panel_dim_info[id];
	irc_info = panel_dim_info->irc_info;

	memcpy(irc_info->ref_tbl, &tbl->arr[(brt_tbl->sz_ui_lum - 1) * irc_info->total_len], irc_info->total_len);
	ret = panel_bl_irc_interpolation(panel_bl, id, irc_info);
	if (ret < 0) {
		panel_err("invalid irc (ret %d)\n", ret);
		return;
	}
	memcpy(dst, irc_info->buffer, irc_info->total_len);
}

static int getidx_irc_mode_table(struct maptbl *tbl)
{
	struct panel_device *panel = (struct panel_device *)tbl->pdata;

	return maptbl_index(tbl, 0, !!panel->panel_data.props.irc_mode, 0);
}

static int getidx_mps_table(struct maptbl *tbl)
{
	struct panel_device *panel = (struct panel_device *)tbl->pdata;
	struct panel_bl_device *panel_bl;
	int row;

	if (panel == NULL) {
		panel_err("panel is null\n");
		return -EINVAL;
	}

	panel_bl = &panel->panel_bl;
	row = (get_actual_brightness(panel_bl,
			panel_bl->props.brightness) <= 39) ? 0 : 1;

	return maptbl_index(tbl, 0, row, 0);
}

#ifdef CONFIG_SUPPORT_DIM_FLASH
static int init_maptbl_from_table(struct maptbl *tbl, enum dim_flash_items item)
{
	struct panel_info *panel_data;
	struct panel_device *panel;
	struct panel_dimming_info *panel_dim_info;
	int index, id;

	if (tbl == NULL || tbl->pdata == NULL) {
		panel_err("maptbl is null\n");
		return -EINVAL;
	}

	panel = tbl->pdata;
	panel_data = &panel->panel_data;
	id = PANEL_BL_SUBDEV_TYPE_DISP;
#ifdef CONFIG_SUPPORT_HMD
	if (item == DIM_FLASH_HMD_GAMMA || item == DIM_FLASH_HMD_AOR)
		id = PANEL_BL_SUBDEV_TYPE_HMD;
#endif
	panel_dim_info = panel_data->panel_dim_info[id];

	if (!panel_dim_info->dimming_maptbl) {
		panel_err("dimming_maptbl is null\n");
		return -EINVAL;
	}

	/* TODO : HMD DIMMING MAPTBL */
	if (item == DIM_FLASH_GAMMA || item == DIM_FLASH_HMD_GAMMA)
		index = DIMMING_GAMMA_MAPTBL;
	else if (item == DIM_FLASH_AOR || item == DIM_FLASH_HMD_AOR)
		index = DIMMING_AOR_MAPTBL;
	else if (item == DIM_FLASH_VINT)
		index = DIMMING_VINT_MAPTBL;
	else if (item == DIM_FLASH_ELVSS)
		index = DIMMING_ELVSS_MAPTBL;
	else if (item == DIM_FLASH_IRC)
		index = DIMMING_IRC_MAPTBL;
	else
		return -EINVAL;

	if (!sizeof_maptbl(&panel_dim_info->dimming_maptbl[index]))
		return -EINVAL;

	maptbl_memcpy(tbl, &panel_dim_info->dimming_maptbl[index]);

	panel_info("copy from %s to %s, size %d, item:%d, index:%d\n",
			panel_dim_info->dimming_maptbl[index].name, tbl->name,
			sizeof_maptbl(tbl), item, index);

	print_maptbl(tbl);

	return 0;
}

static int init_maptbl_from_flash(struct maptbl *tbl, enum dim_flash_items item)
{
	struct panel_info *panel_data;
	struct panel_device *panel;
	int box, layer, row, ret;

	if (tbl == NULL || tbl->pdata == NULL) {
		panel_err("maptbl is null\n");
		return -EINVAL;
	}

	panel = tbl->pdata;
	panel_data = &panel->panel_data;

	for_each_box(tbl, box) {
		for_each_layer(tbl, layer) {
			for_each_row(tbl, row) {
				if (item == DIM_FLASH_GAMMA && row >= S6E3HAB_NR_LUMINANCE)
					continue;

				if (item == DIM_FLASH_ELVSS) {
					if (!is_supported_vrr(panel_data, &S6E3HAB_VRR_DIM[box]))
						continue;

					ret = copy_from_dim_flash(panel_data,
							tbl->arr + maptbl_4d_index(tbl, box, layer, row, 0),
							item, box, row, layer, tbl->sz_copy);
				} else {
					if (!is_supported_vrr(panel_data, &S6E3HAB_VRR_DIM[layer]))
						continue;

					ret = copy_from_dim_flash(panel_data,
							tbl->arr + maptbl_4d_index(tbl, box, layer, row, 0),
							item, layer, row, 0, tbl->sz_copy);
				}

				if (ret < 0) {
					panel_err("failed to copy_from_dim_flash(box:%d, layer:%d, row:%d)\n",
							box, layer, row);
					return init_common_table(tbl);
				}
			}
		}
	}

	print_maptbl(tbl);

	return 0;
}
#endif

static int init_elvss_table(struct maptbl *tbl)
{
#ifdef CONFIG_SUPPORT_DIM_FLASH
	struct panel_device *panel = tbl->pdata;
	struct panel_info *panel_data = &panel->panel_data;

	if (panel_data->props.cur_dim_type == DIM_TYPE_DIM_FLASH)
		return init_maptbl_from_flash(tbl, DIM_FLASH_ELVSS);
	else
		return init_maptbl_from_table(tbl, DIM_FLASH_ELVSS);

	return 0;
#else
	return init_common_table(tbl);
#endif
}

static int init_vint_table(struct maptbl *tbl)
{
#ifdef CONFIG_SUPPORT_DIM_FLASH
	struct panel_device *panel = tbl->pdata;
	struct panel_info *panel_data = &panel->panel_data;

	if (panel_data->props.cur_dim_type == DIM_TYPE_DIM_FLASH)
		return init_maptbl_from_flash(tbl, DIM_FLASH_VINT);
	else
		return init_maptbl_from_table(tbl, DIM_FLASH_VINT);

	return 0;
#else
	return init_common_table(tbl);
#endif
}

static int init_aor_table(struct maptbl *tbl)
{
#ifdef CONFIG_SUPPORT_DIM_FLASH
	struct panel_device *panel = tbl->pdata;
	struct panel_info *panel_data = &panel->panel_data;

	if (panel_data->props.cur_dim_type == DIM_TYPE_DIM_FLASH)
		return init_maptbl_from_flash(tbl, DIM_FLASH_AOR);
	else
		return init_maptbl_from_table(tbl, DIM_FLASH_AOR);

	return 0;
#else
	return init_common_table(tbl);
#endif
}

static int init_irc_table(struct maptbl *tbl)
{
#ifdef CONFIG_SUPPORT_DIM_FLASH
	struct panel_device *panel = tbl->pdata;
	struct panel_info *panel_data = &panel->panel_data;

	if (panel_data->props.cur_dim_type == DIM_TYPE_DIM_FLASH)
		return init_maptbl_from_flash(tbl, DIM_FLASH_IRC);
	else
		return init_maptbl_from_table(tbl, DIM_FLASH_IRC);

	return 0;
#else
	return init_common_table(tbl);
#endif
}

#ifdef CONFIG_SUPPORT_HMD
static int init_hmd_aor_table(struct maptbl *tbl)
{
#ifdef CONFIG_SUPPORT_DIM_FLASH
	struct panel_device *panel = tbl->pdata;
	struct panel_info *panel_data = &panel->panel_data;

	if (panel_data->props.cur_dim_type == DIM_TYPE_DIM_FLASH)
		return init_maptbl_from_flash(tbl, DIM_FLASH_HMD_AOR);
	else
		return init_maptbl_from_table(tbl, DIM_FLASH_HMD_AOR);

	return 0;
#else
	return init_common_table(tbl);
#endif
}
#endif

static int init_elvss_temp_table(struct maptbl *tbl)
{
	struct panel_info *panel_data;
	struct panel_device *panel;
	int row, layer, box, ret;
	u8 elvss_temp_0_otp_value = 0;
	u8 elvss_temp_1_otp_value = 0;

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

	ret = resource_copy_by_name(panel_data, &elvss_temp_0_otp_value, "elvss_temp_0");
	if (unlikely(ret)) {
		panel_err("elvss_temp not found in panel resource\n");
		return -EINVAL;
	}

	ret = resource_copy_by_name(panel_data, &elvss_temp_1_otp_value, "elvss_temp_1");
	if (unlikely(ret)) {
		panel_err("elvss_temp not found in panel resource\n");
		return -EINVAL;
	}

	for_each_box(tbl, box) {
		for_each_layer(tbl, layer) {
			for_each_row(tbl, row) {
				tbl->arr[maptbl_4d_index(tbl, box, layer, row, 0)] =
					(row < S6E3HAB_NR_LUMINANCE) ?
					elvss_temp_0_otp_value : elvss_temp_1_otp_value;
			}
		}
	}

	return 0;
}

static int getidx_elvss_temp_table(struct maptbl *tbl)
{
	int row, layer, box;
	struct panel_info *panel_data;
	struct panel_bl_device *panel_bl;
	struct panel_device *panel = (struct panel_device *)tbl->pdata;

	if (panel == NULL) {
		panel_err("panel is null\n");
		return -EINVAL;
	}

	panel_data = &panel->panel_data;
	panel_bl = &panel->panel_bl;

	box = getidx_s6e3hab_current_vrr_dim(panel);
	if (box < 0)
		box = 0;

	layer = (UNDER_MINUS_15(panel_data->props.temperature) ? UNDER_MINUS_FIFTEEN :
			(UNDER_0(panel_data->props.temperature) ? UNDER_ZERO : OVER_ZERO));
	row = get_actual_brightness_index(panel_bl, panel_bl->props.brightness);

	return maptbl_4d_index(tbl, box, layer, row, 0);
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

static int getidx_acl_onoff_table(struct maptbl *tbl)
{
	struct panel_device *panel = (struct panel_device *)tbl->pdata;

	if (panel == NULL) {
		panel_err("panel is null\n");
		return -EINVAL;
	}

	return maptbl_index(tbl, 0, panel_bl_get_acl_pwrsave(&panel->panel_bl), 0);
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

static int getidx_hbm_onoff_table(struct maptbl *tbl)
{
	struct panel_device *panel = (struct panel_device *)tbl->pdata;
	struct panel_bl_device *panel_bl;

	if (panel == NULL) {
		panel_err("panel is null\n");
		return -EINVAL;
	}

	panel_bl = &panel->panel_bl;

	return maptbl_index(tbl, 0,
			is_hbm_brightness(panel_bl, panel_bl->props.brightness), 0);
}

static int getidx_acl_opr_table(struct maptbl *tbl)
{
	struct panel_device *panel = (struct panel_device *)tbl->pdata;

	if (panel == NULL) {
		panel_err("panel is null\n");
		return -EINVAL;
	}

	return maptbl_index(tbl, 0, panel_bl_get_acl_opr(&panel->panel_bl), 0);
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
#ifdef CONFIG_SUPPORT_DSU
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

	index = search_table_u32(S6E3HAB_SCALER_1440,
			ARRAY_SIZE(S6E3HAB_SCALER_1440), xres);
	if (index < 0)
		row = 0;

	row = index;

	return maptbl_index(tbl, layer, row, 0);
}
#endif

static int getidx_vrr_fps_table(struct maptbl *tbl)
{
	struct panel_device *panel = (struct panel_device *)tbl->pdata;
	int row = 0, layer = 0, index;

	index = getidx_s6e3hab_current_vrr_fps(panel);
	if (index < 0)
		row = 0;
	else
		row = index;

	return maptbl_index(tbl, layer, row, 0);
}

static int getidx_vrr_aid_cycle_table(struct maptbl *tbl)
{
	struct panel_device *panel = (struct panel_device *)tbl->pdata;
	struct panel_vrr *vrr;
	int row = 0, layer = 0;

	vrr = get_panel_vrr(panel);
	if (vrr == NULL)
		return -EINVAL;

	row = vrr->aid_cycle;
	if (row != S6E3HAB_AID_2_CYCLE &&
		row != S6E3HAB_AID_4_CYCLE) {
		panel_err("invalid vrr_aid_cycle:%d\n", row);
		row = 0;
	}

	return maptbl_index(tbl, layer, row, 0);
}

static int getidx_vrr_mode_table(struct maptbl *tbl)
{
	struct panel_device *panel = (struct panel_device *)tbl->pdata;
	int row = 0, layer = 0;

	row = getidx_s6e3hab_current_vrr_mode(panel);
	if (row < 0) {
		panel_err("failed to getidx_s6e3hab_vrr_mode\n");
		row = 0;
	}
	panel_dbg("vrr_mode:%d(%s)\n", row,
			row == S6E3HAB_VRR_MODE_HS ? "HS" : "NM");

	return maptbl_index(tbl, layer, row, 0);
}

static int getidx_vrr_async_table(struct maptbl *tbl)
{
	struct panel_device *panel = (struct panel_device *)tbl->pdata;
	int vrr_fps;
	int row = 0, layer = 0;

	vrr_fps = get_panel_refresh_rate(panel);
	if (vrr_fps < 0)
		return -EINVAL;

	/* adaptive sync fps (i.e. 48HZ, 96HZ) */
	row = (vrr_fps == 48 || vrr_fps == 96) ? 1 : 0;

	return maptbl_index(tbl, layer, row, 0);
}

static int init_lpm_table(struct maptbl *tbl)
{
#ifdef CONFIG_SUPPORT_AOD_BL
	return init_aod_dimming_table(tbl);
#else
	return init_common_table(tbl);
#endif
}

static int getidx_lpm_table(struct maptbl *tbl)
{
	int layer = 0, row = 0;
	struct panel_device *panel;
	struct panel_bl_device *panel_bl;
	struct panel_properties *props;

	panel = (struct panel_device *)tbl->pdata;
	panel_bl = &panel->panel_bl;
	props = &panel->panel_data.props;

#ifdef CONFIG_EXYNOS_DOZE
#ifdef CONFIG_SUPPORT_AOD_BL
	switch (props->alpm_mode) {
	case ALPM_LOW_BR:
	case ALPM_HIGH_BR:
		layer = ALPM_MODE;
		break;
	case HLPM_LOW_BR:
	case HLPM_HIGH_BR:
		layer = HLPM_MODE;
		break;
	default:
		panel_err("Invalid alpm mode : %d\n",
				props->alpm_mode);
		break;
	}

	panel_bl = &panel->panel_bl;
	row = get_subdev_actual_brightness_index(panel_bl,
			PANEL_BL_SUBDEV_TYPE_AOD,
			panel_bl->subdev[PANEL_BL_SUBDEV_TYPE_AOD].brightness);

	props->lpm_brightness =
		panel_bl->subdev[PANEL_BL_SUBDEV_TYPE_AOD].brightness;

	panel_info("alpm_mode %d, brightness %d, layer %d row %d\n",
			props->alpm_mode,
			panel_bl->subdev[PANEL_BL_SUBDEV_TYPE_AOD].brightness,
			layer, row);

#else
	switch (props->alpm_mode) {
	case ALPM_LOW_BR:
		layer = ALPM_MODE;
		row = 0;
		break;
	case HLPM_LOW_BR:
		layer = HLPM_MODE;
		row = 0;
		break;
	case ALPM_HIGH_BR:
		layer = ALPM_MODE;
		row = tbl->nrow - 1;
		break;
	case HLPM_HIGH_BR:
		layer = HLPM_MODE;
		row = tbl->nrow - 1;
		break;
	default:
		panel_err("Invalid alpm mode : %d\n",
				props->alpm_mode);
		break;
	}

	panel_dbg("alpm_mode %d, layer %d row %d\n",
			props->alpm_mode, layer, row);
#endif
#endif
	props->cur_alpm_mode = props->alpm_mode;

	return maptbl_index(tbl, layer, row, 0);
}

#if defined(__PANEL_NOT_USED_VARIABLE__)
static int getidx_lpm_dyn_vlin_table(struct maptbl *tbl)
{
	int row = 0;
	struct panel_device *panel;
	struct panel_properties *props;

	panel = (struct panel_device *)tbl->pdata;
	props = &panel->panel_data.props;

	if (props->lpm_opr < 250)
		row = 0;
	else
		row = 1;

	props->cur_lpm_opr = props->lpm_opr;

	return maptbl_index(tbl, 0, row, 0);
}
#endif

#ifdef CONFIG_SUPPORT_GRAM_CHECKSUM
static int s6e3hab_getidx_vddm_table(struct maptbl *tbl)
{
	struct panel_device *panel = (struct panel_device *)tbl->pdata;
	struct panel_properties *props = &panel->panel_data.props;

	panel_info("vddm %d\n", props->gct_vddm);

	return maptbl_index(tbl, 0, props->gct_vddm, 0);
}

static int s6e3hab_getidx_gram_img_pattern_table(struct maptbl *tbl)
{
	struct panel_device *panel = (struct panel_device *)tbl->pdata;
	struct panel_properties *props = &panel->panel_data.props;

	panel_info("gram img %d\n", props->gct_pattern);
	props->gct_valid_chksum[0] = S6E3HAB_GRAM_CHECKSUM_VALID_1;
	props->gct_valid_chksum[1] = S6E3HAB_GRAM_CHECKSUM_VALID_2;
	props->gct_valid_chksum[2] = S6E3HAB_GRAM_CHECKSUM_VALID_1;
	props->gct_valid_chksum[3] = S6E3HAB_GRAM_CHECKSUM_VALID_2;

	return maptbl_index(tbl, 0, props->gct_pattern, 0);
}
#endif

#ifdef CONFIG_SUPPORT_TDMB_TUNE
static int s6e3hab_getidx_tdmb_tune_table(struct maptbl *tbl)
{
	struct panel_device *panel = (struct panel_device *)tbl->pdata;
	struct panel_properties *props = &panel->panel_data.props;

	return maptbl_index(tbl, 0, props->tdmb_on, 0);
}
#endif

#ifdef CONFIG_DYNAMIC_FREQ

static int getidx_96_5_dyn_ffc_table(struct maptbl *tbl)
{
	int row = 0;
	struct df_status_info *status;
	struct panel_info *panel_data;
	struct panel_device *panel = (struct panel_device *)tbl->pdata;
	int vrr_fps, vrr_mode;

	if (panel == NULL) {
		panel_err("panel is null\n");
		return -EINVAL;
	}

	panel_data = &panel->panel_data;
	status = &panel->df_status;
	vrr_fps = get_panel_refresh_rate(panel);
	vrr_mode = get_panel_refresh_mode(panel);

	panel_info("osc : %d, fps:%d, vrr_mode: %d\n",
		status->current_ddi_osc, vrr_fps, vrr_mode);

	if (status->current_ddi_osc) {
		if ((vrr_fps == 60 && vrr_mode == VRR_HS_MODE))
			row = 8;
		else
			row = 4;
	}

	row += status->ffc_df;

	panel_info("ffc idx: %d, ddi_osc: %d, row: %d\n",
			status->ffc_df, status->current_ddi_osc, row);

	return maptbl_index(tbl, 0, row, 0);
}


static int getidx_96_5_dyn_default_ffc_table(struct maptbl *tbl)
{
	int row = 0;
	struct df_status_info *status;
	struct panel_info *panel_data;
	struct panel_device *panel = (struct panel_device *)tbl->pdata;
	int vrr_fps, vrr_mode;

	if (panel == NULL) {
		panel_err("panel is null\n");
		return -EINVAL;
	}

	panel_data = &panel->panel_data;
	status = &panel->df_status;
	vrr_fps = get_panel_refresh_rate(panel);
	vrr_mode = get_panel_refresh_mode(panel);

	panel_info("osc:%d, vrr_fps:%d, vrr_mode:%d\n",
		status->current_ddi_osc, vrr_fps, vrr_mode);

	if (status->current_ddi_osc) {
		if ((vrr_fps == 60) && (vrr_mode == VRR_HS_MODE))
			row = 2;
		else
			row = 1;
	}

	panel_info("ffc idx: %d, ddi_osc: %d, row: %d\n",
			status->ffc_df, status->current_ddi_osc, row);

	return maptbl_index(tbl, 0, row, 0);
}


static int getidx_86_4_dyn_ffc_table(struct maptbl *tbl)
{
	int row = 0;
	struct df_status_info *status;
	struct panel_device *panel = (struct panel_device *)tbl->pdata;

	if (panel == NULL) {
		panel_err("panel is null\n");
		return -EINVAL;
	}
	status = &panel->df_status;

	row = status->ffc_df;

	panel_info("ffc idx:%d, ddi_osc:%d, row:%d\n",
			status->ffc_df, status->current_ddi_osc, row);

	return maptbl_index(tbl, 0, row, 0);
}


static int getidx_ddi_osc_ltps_comp_table(struct maptbl *tbl)
{
	int row = 0;
	struct df_status_info *status;
	struct panel_info *panel_data;
	struct panel_device *panel = (struct panel_device *)tbl->pdata;
	int vrr_fps, vrr_mode;

	if (panel == NULL) {
		panel_err("panel is null\n");
		return -EINVAL;
	}

	panel_data = &panel->panel_data;
	status = &panel->df_status;
	vrr_fps = get_panel_refresh_rate(panel);
	vrr_mode = get_panel_refresh_mode(panel);

	panel_info("osc:%d, fps:%d, vrr_mode:%d\n",
		status->current_ddi_osc, vrr_fps, vrr_mode);

	if (vrr_mode == VRR_NORMAL_MODE)
		row = 0;
	else
		row = 1;

	panel_info("vrr_mode:%d, ddi_osc:%d, row:%d\n",
			vrr_mode, status->current_ddi_osc, row);

	return maptbl_index(tbl, 0, row, 0);
}



static int getidx_ddi_osc_comp_table(struct maptbl *tbl)
{
	int row = 0;
	struct df_status_info *status;
	struct panel_info *panel_data;
	struct panel_device *panel = (struct panel_device *)tbl->pdata;
	int vrr_fps, vrr_mode;

	if (panel == NULL) {
		panel_err("panel is null\n");
		return -EINVAL;
	}

	panel_data = &panel->panel_data;
	status = &panel->df_status;
	vrr_fps = get_panel_refresh_rate(panel);
	vrr_mode = get_panel_refresh_mode(panel);

	panel_info("osc:%d, fps:%d, vrr_mode:%d\n",
		status->current_ddi_osc, vrr_fps, vrr_mode);

	if (status->current_ddi_osc)
		row = 1;

	panel_info("ddi_osc: %d, row: %d\n",
			status->current_ddi_osc, row);

	return maptbl_index(tbl, 0, row, 0);
}

static int getidx_ddi_osc2_comp_table(struct maptbl *tbl)
{
	int row = 0;
	struct df_status_info *status;
	struct panel_info *panel_data;
	struct panel_device *panel = (struct panel_device *)tbl->pdata;
	int vrr_fps, vrr_mode;

	if (panel == NULL) {
		panel_err("panel is null\n");
		return -EINVAL;
	}


	panel_data = &panel->panel_data;
	status = &panel->df_status;
	vrr_fps = get_panel_refresh_rate(panel);
	vrr_mode = get_panel_refresh_mode(panel);

	panel_info("osc:%d, fps:%d, vrr_mode:%d\n",
		status->current_ddi_osc, vrr_fps, vrr_mode);

	if (status->current_ddi_osc) {
		if ((vrr_fps == 60) && (vrr_mode == VRR_HS_MODE))
			row = 2;
		else
			row = 1;
	}

	panel_info("ffc idx:%d, ddi_osc:%d, row:%d\n",
			status->ffc_df, status->current_ddi_osc, row);

	return maptbl_index(tbl, 0, row, 0);
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

static void copy_mcd_resistance_maptbl(struct maptbl *tbl, u8 *dst)
{
	struct panel_device *panel;
	struct panel_info *panel_data;

	if (!tbl || !dst)
		return;

	panel = (struct panel_device *)tbl->pdata;
	if (unlikely(!panel))
		return;

	panel_data = &panel->panel_data;
	*dst = panel_data->props.mcd_resistance;
	panel_dbg("mcd resistance %02X\n", *dst);
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

static void copy_vfp_nm_maptbl(struct maptbl *tbl, u8 *dst)
{
	struct panel_device *panel;
	struct panel_info *panel_data;
	int vrr_fps, vrr_mode;
	int vfp, index;

	if (!tbl || !dst)
		return;

	panel = (struct panel_device *)tbl->pdata;
	if (unlikely(!panel))
		return;

	panel_data = &panel->panel_data;
	vrr_fps = get_panel_refresh_rate(panel);
	vrr_mode = get_panel_refresh_mode(panel);
	index = getidx_s6e3hab_current_vrr_dim(panel);
	if (index == S6E3HAB_VRR_DIM_60NS || index == S6E3HAB_VRR_DIM_60HS) {
		vfp = calc_vfp(&S6E3HAB_VRR_DIM[index], vrr_fps);
		if (vfp < 0) {
			panel_err("failed to calc_vfp %d\n", vfp);
			return;
		}
	} else {
		vfp = S6E3HAB_VRR_DIM[S6E3HAB_VRR_DIM_60NS].base_vfp;
	}

	panel_dbg("vrr(%d %d) vfp(%d)\n", vrr_fps, vrr_mode, vfp);

	dst[0] = (vfp >> 8) & 0xFF;
	dst[1] = vfp & 0xFF;
}

static void copy_vfp_hs_maptbl(struct maptbl *tbl, u8 *dst)
{
	struct panel_device *panel;
	struct panel_info *panel_data;
	int vrr_fps, vrr_mode;
	int vfp, index;

	if (!tbl || !dst)
		return;

	panel = (struct panel_device *)tbl->pdata;
	if (unlikely(!panel))
		return;

	panel_data = &panel->panel_data;
	vrr_fps = get_panel_refresh_rate(panel);
	vrr_mode = get_panel_refresh_mode(panel);
	index = getidx_s6e3hab_current_vrr_dim(panel);
	if (index == S6E3HAB_VRR_DIM_120HS) {
		vfp = calc_vfp(&S6E3HAB_VRR_DIM[index], vrr_fps);
		if (vfp < 0) {
			panel_err("failed to calc_vfp %d\n", vfp);
			return;
		}
	} else {
		vfp = S6E3HAB_VRR_DIM[S6E3HAB_VRR_DIM_120HS].base_vfp;
	}

	panel_dbg("vrr(%d %d) vfp(%d)\n", vrr_fps, vrr_mode, vfp);

	dst[0] = (vfp >> 8) & 0xFF;
	dst[1] = vfp & 0xFF;
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

	if (S6E3HAB_SCR_CR_OFS + mdnie->props.sz_scr > sizeof_maptbl(tbl)) {
		panel_err("invalid size (maptbl_size %d, sz_scr %d)\n",
				sizeof_maptbl(tbl), mdnie->props.sz_scr);
		return -EINVAL;
	}

	memcpy(&tbl->arr[S6E3HAB_SCR_CR_OFS],
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

	if (sizeof_maptbl(tbl) < (S6E3HAB_NIGHT_MODE_OFS +
			sizeof_row(night_maptbl))) {
		panel_err("invalid size (maptbl_size %d, night_maptbl_size %d)\n",
				sizeof_maptbl(tbl), sizeof_row(night_maptbl));
		return -EINVAL;
	}

	maptbl_copy(night_maptbl, &tbl->arr[S6E3HAB_NIGHT_MODE_OFS]);

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

	if (sizeof_maptbl(tbl) < (S6E3HAB_COLOR_LENS_OFS +
			sizeof_row(color_lens_maptbl))) {
		panel_err("invalid size (maptbl_size %d, color_lens_maptbl_size %d)\n",
				sizeof_maptbl(tbl), sizeof_row(color_lens_maptbl));
		return -EINVAL;
	}

	if (IS_COLOR_LENS_MODE(mdnie))
		maptbl_copy(color_lens_maptbl, &tbl->arr[S6E3HAB_COLOR_LENS_OFS]);

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

#ifdef CONFIG_SUPPORT_AFC
static void copy_afc_maptbl(struct maptbl *tbl, u8 *dst)
{
	struct mdnie_info *mdnie;

	if (!tbl || !dst) {
		panel_err("invalid parameter (tbl %p, dst %p)\n",
				tbl, dst);
		return;
	}
	mdnie = (struct mdnie_info *)tbl->pdata;
	memcpy(dst, mdnie->props.afc_roi,
			sizeof(mdnie->props.afc_roi));

	panel_dbg("afc_on %d\n", mdnie->props.afc_on);
	print_data(dst, sizeof(mdnie->props.afc_roi));
}
#endif
#endif /* CONFIG_EXYNOS_DECON_MDNIE_LITE */

static void show_rddpm(struct dumpinfo *info)
{
	int ret;
	struct resinfo *res = info->res;
	u8 rddpm[S6E3HAB_RDDPM_LEN] = { 0, };
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

static void show_rddsm(struct dumpinfo *info)
{
	int ret;
	struct resinfo *res = info->res;
	u8 rddsm[S6E3HAB_RDDSM_LEN] = { 0, };
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
	u8 err[S6E3HAB_ERR_LEN] = { 0, }, err_15_8, err_7_0;

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

	panel_info("* CRC Error Count : %d\n", err[2]);
	panel_info("* ECC1 Error Count : %d\n", err[3]);
	panel_info("* ECC2 Error Count : %d\n", err[4]);

	panel_info("==================================================\n");
}

static void show_err_fg(struct dumpinfo *info)
{
	int ret;
	u8 err_fg[S6E3HAB_ERR_FG_LEN] = { 0, };
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
	u8 dsi_err[S6E3HAB_DSI_ERR_LEN] = { 0, };

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
	u8 self_diag[S6E3HAB_SELF_DIAG_LEN] = { 0, };

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
	u8 cmdlog[S6E3HAB_CMDLOG_LEN];

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


#define MAFPC_ENABLE 0x51
static int getidx_mafpc_enable_table(struct maptbl *tbl)
{
	struct panel_device *panel = tbl->pdata;
	struct mafpc_device *mafpc = NULL;
	int row = 0;

	if (panel->mafpc_sd == NULL) {
		panel_err("mafpc_sd is null\n");
		goto ret;
	}

	v4l2_subdev_call(panel->mafpc_sd, core, ioctl, V4L2_IOCTL_MAFPC_GET_INFO, NULL);

	mafpc = (struct mafpc_device *)v4l2_get_subdev_hostdata(panel->mafpc_sd);
	if (mafpc == NULL) {
		panel_err("failed to get mafpc info\n");
		goto ret;
	}

	if (mafpc->enable) {
		if ((mafpc->written & MAFPC_UPDATED_FROM_SVC) && (mafpc->written & MAFPC_UPDATED_TO_DEV)) {
			row = 1;
		} else
			panel_warn("enabled: %x, written: %x\n", mafpc->enable, mafpc->written);
	}
ret:
	return maptbl_index(tbl, 0, row, 0);

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


#define S6E3HAB_MAFPC_SCALE_MAX	75

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

	if (index >= S6E3HAB_MAFPC_SCALE_MAX)
		index = S6E3HAB_MAFPC_SCALE_MAX - 1;

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
	u8 mafpc[S6E3HAB_MAFPC_LEN] = {0, };

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
	u8 mafpc_flash[S6E3HAB_MAFPC_FLASH_LEN] = {0, };

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
	u8 crc[S6E3HAB_SELF_MASK_CRC_LEN] = {0, };

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

static bool is_panel_state_not_lpm(struct panel_device *panel)
{
	if (panel->state.cur_state != PANEL_STATE_ALPM)
		return true;

	return false;
}

#ifdef CONFIG_PANEL_VRR_BRIDGE
#define S6E3HAB_HUBBLE_ARR_MIN_LUMINANCE (11)
#define S6E3HAB_HUBBLE_ARR_MAX_LUMINANCE (S6E3HAB_HUBBLE_TARGET_LUMINANCE)
#define S6E3HAB_HUBBLE_BRR_MIN_LUMINANCE (98)
#define S6E3HAB_HUBBLE_BRR_MAX_LUMINANCE (S6E3HAB_HUBBLE_TARGET_LUMINANCE)
static bool s6e3hab_hubble_bridge_refresh_rate_changeable(struct panel_device *panel)
{
	struct panel_bl_device *panel_bl;
	int nit, vrr_fps, vrr_mode;
	bool changeable = false;

	if (panel == NULL) {
		panel_err("panel is null\n");
		return false;
	}

	panel_bl = &panel->panel_bl;
	vrr_fps = get_panel_refresh_rate(panel);
	vrr_mode = get_panel_refresh_mode(panel);
	nit = get_actual_brightness(panel_bl,
			panel_bl->props.brightness);

	if (is_adaptive_sync_vrr(vrr_fps, vrr_mode))
		changeable = (nit >= S6E3HAB_HUBBLE_ARR_MIN_LUMINANCE &&
				nit <= S6E3HAB_HUBBLE_ARR_MAX_LUMINANCE);
	else
		changeable = (nit >= S6E3HAB_HUBBLE_BRR_MIN_LUMINANCE &&
				nit <= S6E3HAB_HUBBLE_BRR_MAX_LUMINANCE);

	return changeable;
}
#endif
static int __init s6e3hab_panel_init(void)
{
	register_common_panel(&s6e3hab_hubble1_a3_s0_preliminary_osc_96_5m_panel_info);
	register_common_panel(&s6e3hab_hubble1_a3_s0_preliminary_panel_info);
	register_common_panel(&s6e3hab_hubble1_a3_s0_default_osc_96_5m_panel_info);
	register_common_panel(&s6e3hab_hubble1_a3_s0_default_panel_info);
	register_common_panel(&s6e3hab_hubble1_a3_s0_default_osc_96_5m_rev03_panel_info);
	register_common_panel(&s6e3hab_hubble1_a3_s0_default_rev03_panel_info);

	register_common_panel(&s6e3hab_hubble2_a3_s0_preliminary_osc_96_5m_panel_info);
	register_common_panel(&s6e3hab_hubble2_a3_s0_preliminary_panel_info);
	register_common_panel(&s6e3hab_hubble2_a3_s0_default_osc_96_5m_panel_info);
	register_common_panel(&s6e3hab_hubble2_a3_s0_default_panel_info);
	register_common_panel(&s6e3hab_hubble2_a3_s0_default_osc_96_5m_rev03_panel_info);
	register_common_panel(&s6e3hab_hubble2_a3_s0_default_rev03_panel_info);

	register_common_panel(&s6e3hab_hubble3_a3_s0_preliminary_osc_96_5m_panel_info);
	register_common_panel(&s6e3hab_hubble3_a3_s0_preliminary_panel_info);
	register_common_panel(&s6e3hab_hubble3_a3_s0_default_osc_96_5m_panel_info);
	register_common_panel(&s6e3hab_hubble3_a3_s0_default_panel_info);
	register_common_panel(&s6e3hab_hubble3_a3_s0_default_osc_96_5m_rev03_panel_info);
	register_common_panel(&s6e3hab_hubble3_a3_s0_default_rev03_panel_info);

	return 0;
}

static void __exit s6e3hab_panel_exit(void)
{
	deregister_common_panel(&s6e3hab_hubble1_a3_s0_preliminary_osc_96_5m_panel_info);
	deregister_common_panel(&s6e3hab_hubble1_a3_s0_preliminary_panel_info);
	deregister_common_panel(&s6e3hab_hubble1_a3_s0_default_osc_96_5m_panel_info);
	deregister_common_panel(&s6e3hab_hubble1_a3_s0_default_panel_info);
	deregister_common_panel(&s6e3hab_hubble1_a3_s0_default_osc_96_5m_rev03_panel_info);
	deregister_common_panel(&s6e3hab_hubble1_a3_s0_default_rev03_panel_info);

	deregister_common_panel(&s6e3hab_hubble2_a3_s0_preliminary_osc_96_5m_panel_info);
	deregister_common_panel(&s6e3hab_hubble2_a3_s0_preliminary_panel_info);
	deregister_common_panel(&s6e3hab_hubble2_a3_s0_default_osc_96_5m_panel_info);
	deregister_common_panel(&s6e3hab_hubble2_a3_s0_default_panel_info);
	deregister_common_panel(&s6e3hab_hubble2_a3_s0_default_osc_96_5m_rev03_panel_info);
	deregister_common_panel(&s6e3hab_hubble2_a3_s0_default_rev03_panel_info);

	deregister_common_panel(&s6e3hab_hubble3_a3_s0_preliminary_osc_96_5m_panel_info);
	deregister_common_panel(&s6e3hab_hubble3_a3_s0_preliminary_panel_info);
	deregister_common_panel(&s6e3hab_hubble3_a3_s0_default_osc_96_5m_panel_info);
	deregister_common_panel(&s6e3hab_hubble3_a3_s0_default_panel_info);
	deregister_common_panel(&s6e3hab_hubble3_a3_s0_default_osc_96_5m_rev03_panel_info);
	deregister_common_panel(&s6e3hab_hubble3_a3_s0_default_rev03_panel_info);
}

module_init(s6e3hab_panel_init)
module_exit(s6e3hab_panel_exit)
MODULE_DESCRIPTION("Samsung Mobile Panel Driver");
MODULE_LICENSE("GPL");
