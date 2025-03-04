/*
 * Copyright (c) 2016 Samsung Electronics Co., Ltd.
 *		http://www.samsung.com
 *
 * Window update file for Samsung EXYNOS DPU driver
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
*/

#include <video/mipi_display.h>

#include "./panels/exynos_panel_drv.h"
#include "decon.h"
#include "dpp.h"
#include "dsim.h"
#if defined(CONFIG_EXYNOS_DECON_DQE)
#include "dqe.h"
#endif
#include "mcd_decon.h"

static void win_update_adjust_region(struct decon_device *decon,
		struct decon_win_config *win_config,
		struct decon_reg_data *regs)
{
#if 0
	int i;
	int div_w, div_h;
	struct decon_rect r1, r2;
	struct decon_win_config *update_config = &win_config[DECON_WIN_UPDATE_IDX];
	struct decon_win_config *config;
	struct decon_frame adj_region;
#endif

	regs->need_update = false;
	DPU_FULL_RECT(&regs->up_region, decon->lcd_info);

	if (!decon->win_up.enabled)
		return;

	if ((decon->state == DECON_STATE_DOZE) || (decon->state == DECON_STATE_DOZE_SUSPEND))
		return;
#if 0
	if (update_config->state != DECON_WIN_STATE_UPDATE)
		return;

	if ((update_config->dst.x < 0) || (update_config->dst.y < 0)) {
		update_config->state = DECON_WIN_STATE_DISABLED;
		return;
	}

	r1.left = update_config->dst.x;
	r1.top = update_config->dst.y;
	r1.right = r1.left + update_config->dst.w - 1;
	r1.bottom = r1.top + update_config->dst.h - 1;

	for (i = 0; i < decon->dt.max_win; i++) {
		config = &win_config[i];
		if (config->state != DECON_WIN_STATE_DISABLED) {
			if (config->dpp_parm.rot || is_scaling(config)) {
				update_config->state = DECON_WIN_STATE_DISABLED;
				return;
			}
		}
	}

	DPU_DEBUG_WIN("original update region[%d %d %d %d]\n",
			update_config->dst.x, update_config->dst.y,
			update_config->dst.w, update_config->dst.h);

	r2.left = (r1.left / decon->win_up.rect_w) * decon->win_up.rect_w;
	r2.top = (r1.top / decon->win_up.rect_h) * decon->win_up.rect_h;

	div_w = (r1.right + 1) / decon->win_up.rect_w;
	div_w = (div_w * decon->win_up.rect_w == r1.right + 1) ? div_w : div_w + 1;
	r2.right = div_w * decon->win_up.rect_w - 1;

	div_h = (r1.bottom + 1) / decon->win_up.rect_h;
	div_h = (div_h * decon->win_up.rect_h == r1.bottom + 1) ? div_h : div_h + 1;
	r2.bottom = div_h * decon->win_up.rect_h - 1;

	/* TODO: Now, 4 slices must be used. This will be modified */
	r2.left = 0;
	r2.right = decon->lcd_info->xres - 1;

	memcpy(&regs->up_region, &r2, sizeof(struct decon_rect));

	memset(&adj_region, 0, sizeof(struct decon_frame));
	adj_region.x = regs->up_region.left;
	adj_region.y = regs->up_region.top;
	adj_region.w = regs->up_region.right - regs->up_region.left + 1;
	adj_region.h = regs->up_region.bottom - regs->up_region.top + 1;
	DPU_EVENT_LOG_UPDATE_REGION(&decon->sd, &update_config->dst, &adj_region);

	DPU_DEBUG_WIN("adjusted update region[%d %d %d %d]\n",
			adj_region.x, adj_region.y, adj_region.w, adj_region.h);
#endif
}

static void win_update_check_limitation(struct decon_device *decon,
		struct decon_win_config *win_config,
		struct decon_reg_data *regs)
{
	struct decon_win_config *config;
	struct decon_win_rect update;
	struct decon_rect r;
	struct v4l2_subdev *sd;
	struct dpp_ch_restriction ch_res;
	struct dpp_restriction *res;
	const struct dpu_fmt *fmt_info;
	int i;
	int sz_align = 1;
	int adj_src_x = 0, adj_src_y = 0;

	memset(&ch_res, 0, sizeof(struct dpp_ch_restriction));

	/*
	 * 'readback/fence/tui + window update' is not a HW limitation,
	 * the update region is changed to full
	 */
	if (decon->win_up.force_full) {
		DPU_DEBUG_WIN("Full size update flag is set!\n");
		DPU_FULL_RECT(&regs->up_region, decon->lcd_info);
		return;;
	}

	if (regs->readback_entry.request) {
		DPU_DEBUG_WIN("Readback case is treated as full size\n");
		DPU_FULL_RECT(&regs->up_region, decon->lcd_info);
		return;
	}

	for (i = 0; i < decon->dt.max_win; i++) {
		config = &win_config[i];
		if (config->state == DECON_WIN_STATE_DISABLED)
			continue;

		if (config->state == DECON_WIN_STATE_CURSOR)
			goto change_full;

		r.left = config->dst.x;
		r.top = config->dst.y;
		r.right = config->dst.w + config->dst.x - 1;
		r.bottom = config->dst.h + config->dst.y - 1;

		if (!decon_intersect(&regs->up_region, &r))
			continue;

		decon_intersection(&regs->up_region, &r, &r);

		if (!(r.right - r.left) && !(r.bottom - r.top))
			continue;

		fmt_info = dpu_find_fmt_info(config->format);
		if (!fmt_info) {
			DPU_DEBUG_WIN("Do not surpport format(%d)\n", config->format);
			goto change_full;
		}
		if (IS_YUV(fmt_info)) {
			/* check alignment for NV12/NV21 format */
			update.x = regs->up_region.left;
			update.y = regs->up_region.top;
			sz_align = 2;

			if (update.y > config->dst.y)
				adj_src_y = config->src.y + (update.y - config->dst.y);
			if (update.x > config->dst.x)
				adj_src_x = config->src.x + (update.x - config->dst.x);

			if (adj_src_x & 0x1 || adj_src_y & 0x1)
				goto change_full;
		}

		sd = decon->dpp_sd[0];
		v4l2_subdev_call(sd, core, ioctl, DPP_GET_RESTRICTION, &ch_res);
		res = &ch_res.restriction;

		if (((r.right - r.left) < (res->src_f_w.min * sz_align)) ||
				((r.bottom - r.top) < (res->src_f_h.min * sz_align))) {
			goto change_full;
		}

		/* cursor async */
		if (((r.right - r.left) > decon->lcd_info->xres) ||
			((r.bottom - r.top) > decon->lcd_info->yres)) {
			goto change_full;
		}
	}

	return;

change_full:
	DPU_DEBUG_WIN("changed full: win(%d) ch(%d) [%d %d %d %d]\n",
			i, config->channel,
			config->dst.x, config->dst.y,
			config->dst.w, config->dst.h);
	DPU_FULL_RECT(&regs->up_region, decon->lcd_info);
	return;
}

static void win_update_reconfig_coordinates(struct decon_device *decon,
		struct decon_win_config *win_config,
		struct decon_reg_data *regs)
{
	struct decon_win_config *config;
	struct decon_win_rect update;
	struct decon_frame origin_dst, origin_src;
	struct decon_rect r;
	int i;

	/* Assume that, window update doesn't support in case of scaling */
	for (i = 0; i < decon->dt.max_win; i++) {
		config = &win_config[i];

		if (config->state == DECON_WIN_STATE_DISABLED)
			continue;

		r.left = config->dst.x;
		r.top = config->dst.y;
		r.right = r.left + config->dst.w - 1;
		r.bottom = r.top + config->dst.h - 1;
		if (!decon_intersect(&regs->up_region, &r)) {
			config->state = DECON_WIN_STATE_DISABLED;
			continue;
		}

		update.x = regs->up_region.left;
		update.y = regs->up_region.top;
		update.w = regs->up_region.right - regs->up_region.left + 1;
		update.h = regs->up_region.bottom - regs->up_region.top + 1;

		memcpy(&origin_dst, &config->dst, sizeof(struct decon_frame));
		memcpy(&origin_src, &config->src, sizeof(struct decon_frame));

		/* reconfigure destination coordinates */
		if (update.x > config->dst.x)
			config->dst.w = min(update.w,
					config->dst.x + config->dst.w - update.x);
		else if (update.x + update.w < config->dst.x + config->dst.w)
			config->dst.w = min(config->dst.w,
					update.w + update.x - config->dst.x);

		if (update.y > config->dst.y)
			config->dst.h = min(update.h,
					config->dst.y + config->dst.h - update.y);
		else if (update.y + update.h < config->dst.y + config->dst.h)
			config->dst.h = min(config->dst.h,
					update.h + update.y - config->dst.y);
		config->dst.x = max(config->dst.x - update.x, 0);
		config->dst.y = max(config->dst.y - update.y, 0);

		/* reconfigure source coordinates */
		if (update.y > origin_dst.y)
			config->src.y += (update.y - origin_dst.y);
		if (update.x > origin_dst.x)
			config->src.x += (update.x - origin_dst.x);
		config->src.w = config->dst.w;
		config->src.h = config->dst.h;

		DPU_DEBUG_WIN("win(%d), ch(%d)\n", i, config->channel);
		DPU_DEBUG_WIN("src: origin[%d %d %d %d] -> change[%d %d %d %d]\n",
				origin_src.x, origin_src.y,
				origin_src.w, origin_src.h,
				config->src.x, config->src.y,
				config->src.w, config->src.h);
		DPU_DEBUG_WIN("dst: origin[%d %d %d %d] -> change[%d %d %d %d]\n",
				origin_dst.x, origin_dst.y,
				origin_dst.w, origin_dst.h,
				config->dst.x, config->dst.y,
				config->dst.w, config->dst.h);
	}
}

#if IS_ENABLED(CONFIG_MCD_PANEL)
static int dpu_find_display_mode(struct decon_device *decon,
		u32 w, u32 h, u32 vrr_fps)
{
	struct exynos_display_mode_info *supported_mode;
	int i;

	for (i = 0; i < decon->lcd_info->display_mode_count; i++) {
		supported_mode = &decon->lcd_info->display_mode[i];
		if ((supported_mode->mode.width == w) &&
			(supported_mode->mode.height == h) &&
			(supported_mode->mode.fps == vrr_fps))
			break;
	}

	if (i == decon->lcd_info->display_mode_count)
		return -EINVAL;

	return i;
}

static unsigned int dpu_get_maximum_fps_by_mres(struct decon_device *decon, u32 w, u32 h)
{
	struct exynos_display_mode_info *supported_mode;
	unsigned int fps = 0;
	int i;

	for (i = 0; i < decon->lcd_info->display_mode_count; i++) {
		supported_mode = &decon->lcd_info->display_mode[i];
		if ((supported_mode->mode.width == w) &&
			(supported_mode->mode.height == h))
			fps = max(fps, supported_mode->mode.fps);
	}

	return fps;
}

#if defined(CONFIG_PANEL_DISPLAY_MODE)
static int dpu_find_panel_display_mode(struct decon_device *decon,
		u32 w, u32 h, u32 vrr_fps, u32 vrr_mode)
{
	struct panel_display_modes *panel_modes;
	struct panel_display_mode *supported_mode;
	int i;

	panel_modes = decon->lcd_info->panel_modes;
	for (i = 0; i < panel_modes->num_modes; i++) {
		supported_mode = panel_modes->modes[i];
		if ((supported_mode->width == w) &&
			(supported_mode->height == h) &&
			(supported_mode->refresh_rate == vrr_fps) &&
			(supported_mode->refresh_mode == vrr_mode))
			break;
	}

	if (i == panel_modes->num_modes)
		return -EINVAL;

	return i;
}

/*
 * best display ext mode will be found by priority
 * searching priority : resolution > vrr_mode > vrr_fps
 */
static int dpu_find_best_panel_display_mode(struct decon_device *decon,
		u32 w, u32 h, u32 vrr_fps, u32 vrr_mode)
{
	struct panel_display_modes *panel_modes;
	struct panel_display_mode *supported_mode;
	struct exynos_panel_info *lcd_info;
	int index;

	lcd_info = decon->lcd_info;
	panel_modes = lcd_info->panel_modes;
	if (!panel_modes)
		return -EINVAL;

	/*
	 * check if requested mres and vrr is available
	 */
	index = dpu_find_panel_display_mode(decon,
			w, h, vrr_fps, vrr_mode);
	if (index >= 0)
		return index;

	/* consider passive mode as HS */
	if (IS_EXYNOS_VRR_PASSIVE_MODE(vrr_mode)) {
		index = dpu_find_panel_display_mode(decon,
				w, h, vrr_fps, IS_EXYNOS_VRR_HS_MODE(vrr_mode) ?
				EXYNOS_PANEL_VRR_HS_MODE : EXYNOS_PANEL_VRR_NS_MODE);
		if (index >= 0)
			return index;
	}

	/*
	 * check if requested mres and vrr_mode and less than vrr_fps
	 */
	for (index = 0; index < panel_modes->num_modes; index++) {
		supported_mode = panel_modes->modes[index];
		if ((supported_mode->width == w) &&
			(supported_mode->height == h) &&
			(supported_mode->refresh_rate <= vrr_fps) &&
			(supported_mode->refresh_mode == vrr_mode))
			break;
	}

	if (index != panel_modes->num_modes)
		return index;

	/* toggle vrr_mode (NS->HS, HS->NS) */
	index = dpu_find_panel_display_mode(decon,
			w, h, vrr_fps, IS_EXYNOS_VRR_HS_MODE(vrr_mode) ?
			EXYNOS_PANEL_VRR_NS_MODE : EXYNOS_PANEL_VRR_HS_MODE);
	if (index >= 0)
		return index;

	/*
	 * check if requested mres and less than vrr_fps
	 */
	for (index = 0; index < panel_modes->num_modes; index++) {
		supported_mode = panel_modes->modes[index];
		if ((supported_mode->width == w) &&
			(supported_mode->height == h) &&
			(supported_mode->refresh_rate <= vrr_fps))
			break;
	}

	if (index != panel_modes->num_modes)
		return index;

	DPU_DEBUG_MRES("best display mode not found(%dx%d@%d%s)\n",
			w, h, vrr_fps, EXYNOS_VRR_MODE_STR(vrr_mode));

	return -EINVAL;
}
#endif
#endif

static bool dpu_need_mres_config(struct decon_device *decon,
		struct decon_win_config *win_config,
		struct decon_reg_data *regs)
{
	struct decon_win_config *mres_config = &win_config[DECON_WIN_UPDATE_IDX];
#if !IS_ENABLED(CONFIG_MCD_PANEL)
	struct exynos_display_mode_info *supported_mode;
	int i;
#else
	int mode_idx, cur_mode_idx;
	unsigned int fps, max_fps;
#endif

	regs->mode_update = false;

	if (!decon->mres_enabled) {
		DPU_DEBUG_MRES("multi-resolution feature is disabled\n");
		goto end;
	}

	if (decon->dt.out_type != DECON_OUT_DSI) {
		DPU_DEBUG_MRES("multi resolution only support DSI path\n");
		goto end;
	}

	if (!decon->lcd_info->mres.en) {
		DPU_DEBUG_MRES("panel doesn't support multi-resolution\n");
		goto end;
	}

	if (!(mres_config->state == DECON_WIN_STATE_MRESOL))
		goto end;

	/* requested LCD resolution */
	regs->lcd_width = mres_config->dst.f_w;
	regs->lcd_height = mres_config->dst.f_h;

#if !IS_ENABLED(CONFIG_MCD_PANEL)
	/* compare previous and requested LCD resolution */
	if ((decon->lcd_info->xres == regs->lcd_width) &&
			(decon->lcd_info->yres == regs->lcd_height)) {
		DPU_DEBUG_MRES("prev & req LCD resolution is same(%d %d)\n",
				regs->lcd_width, regs->lcd_height);
		if (decon->lcd_info->fps == regs->fps)
			goto end;
	}

	/* match supported and requested display mode(resolution & fps) */
	for (i = 0; i < decon->lcd_info->display_mode_count; i++) {
		supported_mode = &decon->lcd_info->display_mode[i];
		if ((supported_mode->mode.width == regs->lcd_width) &&
			(supported_mode->mode.height == regs->lcd_height) &&
			(supported_mode->mode.fps == regs->fps)) {
			regs->mode_update = true;
			regs->mode_idx = i;
			break;
		}
	}

	DPU_DEBUG_MRES("update(%d), mode idx(%d), mode(%dx%d@%d_%d -> %dx%d@%d_%d)\n",
			regs->mode_update, regs->mode_idx,
			decon->lcd_info->xres, decon->lcd_info->yres,
			decon->lcd_info->fps,
			regs->lcd_width, regs->lcd_height, fps);

	/* in case of only fps change, false should be returnned */
	if ((decon->lcd_info->fps != regs->fps) &&
		(decon->lcd_info->xres == regs->lcd_width) &&
		(decon->lcd_info->yres == regs->lcd_height))
		return false;
#else
	/* compare previous and requested LCD resolution */
	if ((decon->lcd_info->xres == regs->lcd_width) &&
			(decon->lcd_info->yres == regs->lcd_height)) {
		DPU_DEBUG_MRES("prev & req LCD resolution is same(%d %d)\n",
				regs->lcd_width, regs->lcd_height);
		goto end;
	}

	cur_mode_idx = decon->lcd_info->cur_mode_idx;
	max_fps = dpu_get_maximum_fps_by_mres(decon,
			regs->lcd_width, regs->lcd_height);
	fps = min(decon->lcd_info->display_mode[cur_mode_idx].mode.fps, max_fps);

	/* match supported and requested display mode(resolution & fps) */
	mode_idx = dpu_find_display_mode(decon,
			regs->lcd_width, regs->lcd_height, fps);
	if (mode_idx < 0) {
		DPU_ERR_MRES("%s:could not find display mode(%dx%d@%d)\n",
				__func__, regs->lcd_width, regs->lcd_height, fps);
		goto end;
	}

	regs->mode_update = true;
	regs->mode_idx = mode_idx;

	DPU_DEBUG_MRES("update(%d), mode idx(%d), mode(%dx%d@%d_%d -> %dx%d@%d_%d)\n",
			regs->mode_update, regs->mode_idx,
			decon->lcd_info->xres, decon->lcd_info->yres,
			decon->lcd_info->display_mode[cur_mode_idx].mode.fps,
			decon->lcd_info->fps, regs->lcd_width, regs->lcd_height,
			decon->lcd_info->display_mode[mode_idx].mode.fps, fps);

	/* in case of only fps change, false should be returnned */
	if ((decon->lcd_info->display_mode[cur_mode_idx].mode.fps != regs->fps) &&
		(decon->lcd_info->xres == regs->lcd_width) &&
		(decon->lcd_info->yres == regs->lcd_height))
		return false;
#endif

end:
	return regs->mode_update;
}

void dpu_prepare_win_update_config(struct decon_device *decon,
		struct decon_win_config_data *win_data,
		struct decon_reg_data *regs)
{
	struct decon_win_config *win_config;
	bool reconfigure = false;
	struct decon_rect r;

	if (IS_ERR_OR_NULL(win_data->config)) {
		DPU_ERR_MRES("%s: config ptr of win_data is invalid\n", __func__);
		return;
	}

	win_config = win_data->config;

	if (!decon->win_up.enabled)
		return;

	if (decon->dt.out_type != DECON_OUT_DSI)
		return;

	/* If LCD resolution is changed, window update is ignored */
	if (dpu_need_mres_config(decon, win_config, regs)) {
		regs->up_region.left = 0;
		regs->up_region.top = 0;
		regs->up_region.right = regs->lcd_width - 1;
		regs->up_region.bottom = regs->lcd_height - 1;
		return;
	} else if (regs->mode_update) { /* fps change only */
		memcpy(&regs->up_region, &decon->win_up.prev_up_region,
			sizeof(struct decon_rect));
		return;
	}

	/* find adjusted update region on LCD */
	win_update_adjust_region(decon, win_config, regs);

	/* check DPP hw limitation if violated, update region is changed to full */
	win_update_check_limitation(decon, win_config, regs);

	/*
	 * If update region is changed, need_update flag is set.
	 * That means hw configuration is needed
	 */
	if (is_decon_rect_differ(&decon->win_up.prev_up_region, &regs->up_region))
		regs->need_update = true;
	else
		regs->need_update = false;

	/*
	 * 'readback/fence/tui + window update' is not a HW limitation,
	 * the need_update is set to true to avoid size-mismatch
	 */
	if (decon->win_up.force_full) {
		regs->need_update = true;
		decon->win_up.force_full = false;
	}

	/*
	 * If partial update region is requested, source and destination
	 * coordinates are needed to change if overlapped with update region.
	 */
	DPU_FULL_RECT(&r, decon->lcd_info);
	if (is_decon_rect_differ(&regs->up_region, &r))
		reconfigure = true;

	if (regs->need_update || reconfigure) {
		DPU_DEBUG_WIN("need_update(%d), reconfigure(%d)\n",
				regs->need_update, reconfigure);
		DPU_EVENT_LOG_WINUP_FLAGS(&decon->sd, regs->need_update, reconfigure);
	}

	/* Reconfigure source and destination coordinates, if needed. */
	if (reconfigure)
		win_update_reconfig_coordinates(decon, win_config, regs);
}

#if !IS_ENABLED(CONFIG_MCD_PANEL)
void dpu_set_mres_config(struct decon_device *decon, struct decon_reg_data *regs)
{
	struct dsim_device *dsim = get_dsim_drvdata(0);
	struct exynos_display_mode_info *mode_info = dsim->panel->lcd_info.display_mode;
	struct decon_param p;
	u32 idx;

	if (!decon->mres_enabled) {
		DPU_DEBUG_MRES("multi-resolution feature is disabled\n");
		return;
	}

	if (decon->dt.out_type != DECON_OUT_DSI) {
		DPU_DEBUG_MRES("multi resolution only support DSI path\n");
		return;
	}

	if (!decon->lcd_info->mres.en) {
		DPU_DEBUG_MRES("panel doesn't support multi-resolution\n");
		return;
	}

	if (!regs->mode_update)
		return;

	if (IS_ERR_OR_NULL(dsim)) {
		DPU_ERR_MRES("%s: dsim device ptr is invalid\n", __func__);
		return;
	}

	/*
	 * If fps differs even if the requested resolution is same as before,
	 * cur_mode_idx should be updated,
	 * in order to respond to EXYNOS_GET_DISPLAY_CURRENT_MODE correctly
	 */
	if ((dsim->panel->lcd_info.xres == regs->lcd_width) &&
		(dsim->panel->lcd_info.yres == regs->lcd_height) &&
		(dsim->panel->lcd_info.fps != regs->fps)) {
		dsim->panel->lcd_info.cur_mode_idx = regs->mode_idx;
		decon_reg_wait_idle_status_timeout(decon->id, IDLE_WAIT_TIMEOUT);
		dsim_reg_set_cm_underrun_lp_ref(dsim->id,
				dsim->panel->lcd_info.display_mode[regs->mode_idx].cmd_lp_ref);
		return;
	}

	/*
	 * Before LCD resolution is changed, previous frame data must be
	 * finished to transfer.
	 */
	decon_reg_wait_idle_status_timeout(decon->id, IDLE_WAIT_TIMEOUT);

	/* backup current LCD resolution information to previous one */
	dsim->panel->lcd_info.xres = regs->lcd_width;
	dsim->panel->lcd_info.yres = regs->lcd_height;
	idx = regs->mode_idx;

	dsim->panel->lcd_info.dsc.en = mode_info[idx].dsc_en;
	dsim->panel->lcd_info.dsc.slice_h = mode_info[idx].dsc_height;
	dsim->panel->lcd_info.dsc.enc_sw = mode_info[idx].dsc_enc_sw;
	dsim->panel->lcd_info.dsc.dec_sw = mode_info[idx].dsc_dec_sw;

	/* transfer LCD resolution change commands to panel */
	dsim_call_panel_ops(dsim, EXYNOS_PANEL_IOC_MRES, &regs->mode_idx);
	dsim->panel->lcd_info.cur_mode_idx = regs->mode_idx;

	/* DECON and DSIM are reconfigured by changed LCD resolution */
	dsim_reg_set_mres(dsim->id, &dsim->panel->lcd_info);
	decon_to_init_param(decon, &p);
	decon_reg_set_mres(decon->id, &p);
#if defined(CONFIG_EXYNOS_DECON_DQE)
	dqe_reg_start(decon->id, decon->lcd_info);
#endif


	/* If LCD resolution is changed, initial partial size is also changed */
	dpu_init_win_update(decon);

	DPU_DEBUG_MRES("changed LCD resolution(%d %d), dsc enc/dec sw(%d %d)\n",
			decon->lcd_info->xres, decon->lcd_info->yres,
			dsim->panel->lcd_info.dsc.enc_sw,
			dsim->panel->lcd_info.dsc.dec_sw);
}
#else
static int dpu_check_mres_condition(struct decon_device *decon,
		bool mode_update)
{
	if (!decon->mres_enabled) {
		DPU_DEBUG_MRES("multi-resolution feature is disabled\n");
		return -EPERM;
	}

	if (decon->dt.out_type != DECON_OUT_DSI) {
		DPU_DEBUG_MRES("multi resolution only support DSI path\n");
		return -EPERM;
	}

	if (!decon->lcd_info->mres.en) {
		DPU_DEBUG_MRES("panel doesn't support multi-resolution\n");
		return -EPERM;
	}

	if (!mode_update)
		return -EPERM;

	return 0;
}

static int dpu_update_display_mode(struct decon_device *decon,
		int mode_idx, int vrr_mode)
{
	struct dsim_device *dsim = get_dsim_drvdata(0);
	struct exynos_display_mode_info *display_mode;
	struct exynos_panel_info *lcd_info;
#if defined(CONFIG_PANEL_DISPLAY_MODE)
	struct panel_display_modes *panel_modes;
	int panel_mode_idx;
#endif

	if (IS_ERR_OR_NULL(dsim)) {
		DPU_ERR_MRES("%s: dsim device ptr is invalid\n", __func__);
		return -EINVAL;
	}

	lcd_info = &dsim->panel->lcd_info;
	display_mode = lcd_info->display_mode;
	if (mode_idx >= lcd_info->display_mode_count) {
		DPU_ERR_MRES("invalid mode_idx(%d) display_mode_count(%d)\n",
				mode_idx, lcd_info->display_mode_count);
		return -EINVAL;
	}

	lcd_info->cur_mode_idx = mode_idx;
	lcd_info->xres = display_mode[mode_idx].mode.width;
	lcd_info->yres = display_mode[mode_idx].mode.height;
	lcd_info->dsc.en = display_mode[mode_idx].dsc_en;
	lcd_info->dsc.slice_h = display_mode[mode_idx].dsc_height;
	lcd_info->dsc.enc_sw = display_mode[mode_idx].dsc_enc_sw;
	lcd_info->dsc.dec_sw = display_mode[mode_idx].dsc_dec_sw;

#if defined(CONFIG_PANEL_DISPLAY_MODE)
	panel_modes = dsim->panel->lcd_info.panel_modes;
	/* update fps, vrr_mode using panel_display_mode */
	panel_mode_idx =
		dpu_find_best_panel_display_mode(decon,
				dsim->panel->lcd_info.xres,
				dsim->panel->lcd_info.yres,
				display_mode[mode_idx].mode.fps, vrr_mode);
	if (panel_mode_idx < 0) {
		DPU_ERR_MRES("could not find panel display mode(%dx%d@%d%s)\n",
				dsim->panel->lcd_info.xres,
				dsim->panel->lcd_info.yres,
				dsim->panel->lcd_info.fps,
				REFRESH_MODE_STR(vrr_mode));
		return -EPERM;
	}
	lcd_info->panel_mode_idx = panel_mode_idx;
	lcd_info->fps =
		panel_modes->modes[panel_mode_idx]->panel_refresh_rate;
	lcd_info->vrr_mode = vrr_mode;
	lcd_info->panel_te_sw_skip_count =
		panel_modes->modes[panel_mode_idx]->panel_te_sw_skip_count;
	lcd_info->panel_te_hw_skip_count =
		panel_modes->modes[panel_mode_idx]->panel_te_hw_skip_count;
	lcd_info->cmd_lp_ref =
		panel_modes->modes[panel_mode_idx]->cmd_lp_ref;
	DPU_INFO_MRES("%s: update cmd_lp_ref:%d\n", __func__, lcd_info->cmd_lp_ref);
#else
	dsim->panel->lcd_info.fps =
		display_mode[mode_idx].mode.fps;
#endif

#if defined(CONFIG_DECON_VRR_MODULATION)
	/*
	 * update refresh-rate s/w modulation count
	 */
	decon->vsync.div_count =
		exynos_panel_div_count(lcd_info);
#endif

#if defined(CONFIG_PANEL_DISPLAY_MODE)
	DPU_DEBUG_MRES("changed display_mode(%d:%dx%d@%d%s_%d%s) dsc enc/dec sw(%d %d)\n",
			mode_idx, lcd_info->xres, lcd_info->yres,
			panel_modes->modes[panel_mode_idx]->refresh_rate,
			REFRESH_MODE_STR(lcd_info->vrr_mode),
			lcd_info->fps, REFRESH_MODE_STR(lcd_info->vrr_mode),
			lcd_info->dsc.enc_sw, lcd_info->dsc.dec_sw);
#else
	DPU_DEBUG_MRES("changed display_mode(%d:%dx%d@%d%s) dsc enc/dec sw(%d %d)\n",
			mode_idx, lcd_info->xres, lcd_info->yres,
			lcd_info->fps, REFRESH_MODE_STR(lcd_info->vrr_mode),
			lcd_info->dsc.enc_sw, lcd_info->dsc.dec_sw);
#endif

	return 0;
}

void dpu_update_mres_lcd_info(struct decon_device *decon,
		struct decon_reg_data *regs)
{
	struct dsim_device *dsim = get_dsim_drvdata(0);
	int ret;

	if (dpu_check_mres_condition(decon, regs->mode_update))
		return;

	if (IS_ERR_OR_NULL(dsim)) {
		DPU_ERR_MRES("%s: dsim device ptr is invalid\n", __func__);
		return;
	}

	/* backup current LCD resolution information to previous one */
	ret = dpu_update_display_mode(decon, regs->mode_idx,
			dsim->panel->lcd_info.vrr_mode);
	if (ret < 0) {
		DPU_ERR_MRES("%s: could not update display_mode(%d)\n",
				__func__, regs->mode_idx);
		return;
	}
}

#if defined(CONFIG_PANEL_DISPLAY_MODE)
static int dpu_set_panel_display_mode(struct decon_device *decon, int panel_mode_idx)
{
	struct dsim_device *dsim = get_dsim_drvdata(0);
	struct panel_display_modes *panel_modes;
	struct exynos_panel_info *lcd_info;
	struct panel_display_mode *pdm;
	int ret;

	if (decon->dt.out_type != DECON_OUT_DSI)
		return -EPERM;

	if (IS_ERR_OR_NULL(dsim)) {
		DPU_ERR_MRES("%s: dsim device ptr is invalid\n", __func__);
		return -EINVAL;
	}

	lcd_info = &dsim->panel->lcd_info;
	panel_modes = lcd_info->panel_modes;
	if (!panel_modes) {
		DPU_ERR_MRES("%s: panel_modes not prepared\n", __func__);
		return -EINVAL;
	}

	pdm = panel_modes->modes[panel_mode_idx];

#if defined(CONFIG_DECON_BTS_VRR_ASYNC)
	/*
	 * lcd_info's fps is used in bts calculation.
	 * To prevent underrun, update fps first
	 * if target fps is bigger than previous fps.
	 */
	decon_bts_set_candidate_fps(&decon->bts, pdm->panel_refresh_rate);
	DPU_DEBUG_BTS("\tacquire min bts_fps(%d)\n", pdm->panel_refresh_rate);
#endif

	DPU_INFO_MRES("set panel_display_mode(%d:%dx%d@%d%s_%d%s) +\n",
			panel_mode_idx,
			panel_modes->modes[panel_mode_idx]->width,
			panel_modes->modes[panel_mode_idx]->height,
			panel_modes->modes[panel_mode_idx]->refresh_rate,
			REFRESH_MODE_STR(panel_modes->modes[panel_mode_idx]->refresh_mode),
			panel_modes->modes[panel_mode_idx]->panel_refresh_rate,
			REFRESH_MODE_STR(panel_modes->modes[panel_mode_idx]->panel_refresh_mode));

	ret = dsim_call_panel_ops(dsim,
			EXYNOS_PANEL_IOC_SET_DISPLAY_MODE, &panel_mode_idx);
	if (ret < 0) {
		DPU_ERR_MRES("%s: failed to set display mode(ret:%d)\n",
				__func__, ret);
#if defined(CONFIG_DECON_BTS_VRR_ASYNC)
		decon_bts_clear_candidate_fps(&decon->bts);
#endif
		return ret;
	}

	if (lcd_info->mode == DECON_MIPI_COMMAND_MODE) {
		decon_reg_wait_idle_status_timeout(decon->id, IDLE_WAIT_TIMEOUT);
		dsim_reg_set_cm_underrun_lp_ref(dsim->id, lcd_info->cmd_lp_ref);
		dsim_reg_set_cmd_ctrl(dsim->id, decon->lcd_info, &dsim->clks);
#if defined(CONFIG_EXYNOS_EWR)
		decon_reg_update_ewr_control(decon->id, lcd_info->fps);
#endif
		DPU_INFO_MRES("%s: apply cmd_lp_ref:%d\n",
				__func__, lcd_info->cmd_lp_ref);
	}

	DPU_INFO_MRES("set panel_display_mode(%d:%dx%d@%d%s_%d%s) -\n",
			panel_mode_idx,
			panel_modes->modes[panel_mode_idx]->width,
			panel_modes->modes[panel_mode_idx]->height,
			panel_modes->modes[panel_mode_idx]->refresh_rate,
			REFRESH_MODE_STR(panel_modes->modes[panel_mode_idx]->refresh_mode),
			panel_modes->modes[panel_mode_idx]->panel_refresh_rate,
			REFRESH_MODE_STR(panel_modes->modes[panel_mode_idx]->panel_refresh_mode));

#if defined(CONFIG_DECON_BTS_VRR_ASYNC)
	decon_bts_accept_candidate_fps(&decon->bts, decon->vsync.count + 2);
	DPU_DEBUG_BTS("\trelease min bts_fps(%d)\n", pdm->panel_refresh_rate);
	DPU_DEBUG_BTS("\tadd fps_sync(fps:%d timeline:%llu)\n",
		pdm->panel_refresh_rate, decon->vsync.count + 2);
#endif

	return 0;
}
#endif

static int dpu_set_panel_mres(struct decon_device *decon, int mode_idx)
{
	struct dsim_device *dsim = get_dsim_drvdata(0);
	struct exynos_panel_info *lcd_info;
#if defined(CONFIG_PANEL_DISPLAY_MODE)
	int panel_mode_idx;
#endif
	int ret;

	if (decon->dt.out_type != DECON_OUT_DSI)
		return -EPERM;

	if (IS_ERR_OR_NULL(dsim)) {
		DPU_ERR_MRES("%s: dsim device ptr is invalid\n", __func__);
		return -EINVAL;
	}

	lcd_info = &dsim->panel->lcd_info;
#if defined(CONFIG_PANEL_DISPLAY_MODE)
	/* change exynos display mode to panel display mode index */
	panel_mode_idx = dpu_find_best_panel_display_mode(decon,
			lcd_info->display_mode[mode_idx].mode.width,
			lcd_info->display_mode[mode_idx].mode.height,
			lcd_info->display_mode[mode_idx].mode.fps,
			lcd_info->vrr_mode);
	if (panel_mode_idx < 0) {
		DPU_ERR_MRES("could not find panel display mode(%dx%d@%d%s)\n",
				lcd_info->display_mode[mode_idx].mode.width,
				lcd_info->display_mode[mode_idx].mode.height,
				lcd_info->display_mode[mode_idx].mode.fps,
				REFRESH_MODE_STR(lcd_info->vrr_mode));
		return -EPERM;
	}

	ret = dpu_set_panel_display_mode(decon, panel_mode_idx);
	if (ret < 0) {
		DPU_ERR_MRES("%s:failed to set display_mode(%d) ret(%d)\n",
				__func__, mode_idx, ret);
		return ret;
	}
#else
	ret = dsim_call_panel_ops(dsim, EXYNOS_PANEL_IOC_MRES, &mode_idx);
	if (ret < 0)
		return ret;
#endif

	return 0;
}

void dpu_set_mres_config(struct decon_device *decon, struct decon_reg_data *regs)
{
	struct dsim_device *dsim = get_dsim_drvdata(0);
	struct decon_param p;

	if (dpu_check_mres_condition(decon, regs->mode_update))
		return;

	/*
	 * Before LCD resolution is changed, previous frame data must be
	 * finished to transfer.
	 */
	decon_reg_wait_idle_status_timeout(decon->id, IDLE_WAIT_TIMEOUT);

	/* transfer LCD resolution change commands to panel */
	dpu_set_panel_mres(decon, regs->mode_idx);

	/* DECON and DSIM are reconfigured by changed LCD resolution */
	dsim_reg_set_mres(dsim->id, &dsim->panel->lcd_info);
	decon_to_init_param(decon, &p);
	decon_reg_set_mres(decon->id, &p);
#if defined(CONFIG_EXYNOS_DECON_DQE)
	dqe_reg_start(decon->id, decon->lcd_info);
#endif

	/* If LCD resolution is changed, initial partial size is also changed */
	dpu_init_win_update(decon);
}

void dpu_update_vrr_lcd_info(struct decon_device *decon,
		struct vrr_config_data *vrr_config)
{
	struct dsim_device *dsim = get_dsim_drvdata(0);
	struct exynos_panel_info *lcd_info;
	int ret, mode_idx, cur_mode_idx;
	unsigned int fps, max_fps;

	if (IS_ERR_OR_NULL(dsim)) {
		DPU_ERR_MRES("%s: dsim device ptr is invalid\n", __func__);
		return;
	}

	lcd_info = &dsim->panel->lcd_info;
	cur_mode_idx = lcd_info->cur_mode_idx;

	max_fps = dpu_get_maximum_fps_by_mres(decon,
				lcd_info->display_mode[cur_mode_idx].mode.width,
				lcd_info->display_mode[cur_mode_idx].mode.height);
	fps = min(vrr_config->fps, max_fps);

	/* find exynos_display_mode with new fps */
	mode_idx = dpu_find_display_mode(decon,
			lcd_info->display_mode[cur_mode_idx].mode.width,
			lcd_info->display_mode[cur_mode_idx].mode.height, fps);
	if (mode_idx < 0) {
		DPU_ERR_MRES("%s:could not find display mode(%dx%d@%d)\n",
				__func__, lcd_info->display_mode[cur_mode_idx].mode.width,
				lcd_info->display_mode[cur_mode_idx].mode.height, fps);
		return;
	}

	/* backup current LCD refresh-rate information to previous one */
	ret = dpu_update_display_mode(decon, mode_idx, vrr_config->mode);
	if (ret < 0) {
		DPU_ERR_MRES("%s: could not update display_mode(%d)\n",
				__func__, mode_idx);
		return;
	}
}

static int dpu_set_panel_vrr(struct decon_device *decon,
		struct vrr_config_data *vrr_config)
{
	struct dsim_device *dsim = get_dsim_drvdata(0);
	int ret;
#if defined(CONFIG_PANEL_DISPLAY_MODE)
	struct exynos_panel_info *lcd_info;
	int panel_mode_idx;
	int mode_idx, cur_mode_idx;
	unsigned int fps, max_fps;

	lcd_info = &dsim->panel->lcd_info;
	cur_mode_idx = lcd_info->cur_mode_idx;

	max_fps = dpu_get_maximum_fps_by_mres(decon,
			lcd_info->display_mode[cur_mode_idx].mode.width,
			lcd_info->display_mode[cur_mode_idx].mode.height);
	fps = min(vrr_config->fps, max_fps);

	/* find exynos_display_mode with new fps */
	mode_idx = dpu_find_display_mode(decon,
			lcd_info->display_mode[cur_mode_idx].mode.width,
			lcd_info->display_mode[cur_mode_idx].mode.height, fps);
	if (mode_idx < 0) {
		DPU_ERR_MRES("%s:could not find display mode(%dx%d@%d)\n",
				__func__, lcd_info->display_mode[cur_mode_idx].mode.width,
				lcd_info->display_mode[cur_mode_idx].mode.height, fps);
		return -EPERM;
	}

	/* change exynos display mode to panel display mode index */
	panel_mode_idx = dpu_find_best_panel_display_mode(decon,
			lcd_info->display_mode[mode_idx].mode.width,
			lcd_info->display_mode[mode_idx].mode.height,
			vrr_config->fps, vrr_config->mode);
	if (panel_mode_idx < 0) {
		DPU_ERR_MRES("could not find panel display mode(%dx%d@%d%s)\n",
			lcd_info->display_mode[mode_idx].mode.width,
			lcd_info->display_mode[mode_idx].mode.height,
			vrr_config->fps, REFRESH_MODE_STR(vrr_config->mode));
		return -EPERM;
	}

	ret = dpu_set_panel_display_mode(decon, panel_mode_idx);
	if (ret < 0) {
		DPU_ERR_MRES("%s:failed to set display_mode(%d) ret(%d)\n",
				__func__, mode_idx, ret);
		return ret;
	}
#else
	ret = dsim_call_panel_ops(dsim,
			EXYNOS_PANEL_IOC_SET_VREFRESH, vrr_config);
	if (ret < 0)
		return ret;
#endif

	return 0;
}

void dpu_set_vrr_config(struct decon_device *decon,
		struct vrr_config_data *vrr_config)
{
	int ret;

	ret = dpu_set_panel_vrr(decon, vrr_config);
	if (ret < 0)
		DPU_ERR_MRES("%s:failed to set vrr(%d%s)\n",
				__func__, vrr_config->fps,
				REFRESH_MODE_STR(vrr_config->mode));
}
#endif

#if !IS_ENABLED(CONFIG_MCD_PANEL)
static int win_update_send_partial_command(struct dsim_device *dsim,
		struct decon_rect *rect)
{
	char column[5];
	char page[5];
	int retry;

	DPU_DEBUG_WIN("SET: [%d %d %d %d]\n", rect->left, rect->top,
			rect->right - rect->left + 1, rect->bottom - rect->top + 1);

	column[0] = MIPI_DCS_SET_COLUMN_ADDRESS;
	column[1] = (rect->left >> 8) & 0xff;
	column[2] = rect->left & 0xff;
	column[3] = (rect->right >> 8) & 0xff;
	column[4] = rect->right & 0xff;

	page[0] = MIPI_DCS_SET_PAGE_ADDRESS;
	page[1] = (rect->top >> 8) & 0xff;
	page[2] = rect->top & 0xff;
	page[3] = (rect->bottom >> 8) & 0xff;
	page[4] = rect->bottom & 0xff;

	retry = 2;
	while (dsim_write_data(dsim, MIPI_DSI_DCS_LONG_WRITE,
				(unsigned long)column, ARRAY_SIZE(column), true) != 0) {
		dsim_err("failed to write COLUMN_ADDRESS\n");
		dsim_reg_function_reset(dsim->id);
		if (--retry <= 0) {
			dsim_err("COLUMN_ADDRESS is failed: exceed retry count\n");
			return -EINVAL;
		}
	}

	retry = 2;
	while (dsim_write_data(dsim, MIPI_DSI_DCS_LONG_WRITE,
				(unsigned long)page, ARRAY_SIZE(page), true) != 0) {
		dsim_err("failed to write PAGE_ADDRESS\n");
		dsim_reg_function_reset(dsim->id);
		if (--retry <= 0) {
			dsim_err("PAGE_ADDRESS is failed: exceed retry count\n");
			return -EINVAL;
		}
	}

	return 0;
}
#else
static int win_update_send_partial_command(struct dsim_device *dsim,
		struct decon_rect *rect)
{
	DPU_DEBUG_WIN("SET: [%d %d %d %d]\n", rect->left, rect->top,
			rect->right - rect->left + 1, rect->bottom - rect->top + 1);

	dsim_call_panel_ops(dsim, EXYNOS_PANEL_IOC_SETAREA, rect);

	return 0;
}
#endif

static void win_update_find_included_slice(struct exynos_panel_info *lcd,
		struct decon_rect *rect, bool in_slice[])
{
	int slice_left, slice_right, slice_width;
	int i;

	slice_left = 0;
	slice_right = 0;
	slice_width = lcd->dsc.dec_sw;

	for (i = 0; i < lcd->dsc.slice_num; ++i) {
		slice_left = slice_width * i;
		slice_right = slice_left + slice_width - 1;
		in_slice[i] = false;

		if ((slice_left >= rect->left) && (slice_right <= rect->right))
			in_slice[i] = true;

		DPU_DEBUG_WIN("slice_left(%d), right(%d)\n", slice_left, slice_right);
		DPU_DEBUG_WIN("slice[%d] is %s\n", i,
				in_slice[i] ? "included" : "not included");
	}
}

static void win_update_set_partial_size(struct decon_device *decon,
		struct decon_rect *rect)
{
	struct exynos_panel_info lcd_info;
	struct dsim_device *dsim = get_dsim_drvdata(0);
	bool in_slice[MAX_DSC_SLICE_CNT];

	memcpy(&lcd_info, decon->lcd_info, sizeof(struct exynos_panel_info));
	lcd_info.xres = rect->right - rect->left + 1;
	lcd_info.yres = rect->bottom - rect->top + 1;

	lcd_info.hfp = decon->lcd_info->hfp +
		((decon->lcd_info->xres - lcd_info.xres) >> 1);
	lcd_info.vfp = decon->lcd_info->vfp + decon->lcd_info->yres - lcd_info.yres;

	dsim_reg_set_partial_update(dsim->id, &lcd_info);

	win_update_find_included_slice(decon->lcd_info, rect, in_slice);
	decon_reg_set_partial_update(decon->id, decon->dt.dsi_mode,
			decon->lcd_info, in_slice,
			lcd_info.xres, lcd_info.yres);
#if defined(CONFIG_EXYNOS_DECON_DQE)
	dqe_reg_start(decon->id, &lcd_info);
#endif
	DPU_DEBUG_WIN("SET: vfp %d vbp %d vsa %d hfp %d hbp %d hsa %d w %d h %d\n",
			lcd_info.vfp, lcd_info.vbp, lcd_info.vsa,
			lcd_info.hfp, lcd_info.hbp, lcd_info.hsa,
			lcd_info.xres, lcd_info.yres);
}

void dpu_set_win_update_config(struct decon_device *decon,
		struct decon_reg_data *regs)
{
	struct dsim_device *dsim = get_dsim_drvdata(0);
	bool in_slice[MAX_DSC_SLICE_CNT];
	bool full_partial_update = false;

	if (!decon->win_up.enabled)
		return;

	if (decon->dt.out_type != DECON_OUT_DSI)
		return;

	if (regs == NULL) {
		regs = kzalloc(sizeof(struct decon_reg_data), GFP_KERNEL);
		if (!regs) {
			decon_err("%s: reg_data allocation fail\n", __func__);
			return;
		}
		DPU_FULL_RECT(&regs->up_region, decon->lcd_info);
		regs->need_update = true;
		full_partial_update = true;
	}

	if (regs->need_update) {
		win_update_find_included_slice(decon->lcd_info,
				&regs->up_region, in_slice);

		/*
		 * hw configuration related to partial update must be set
		 * without DMA operation
		 */
		win_update_send_partial_command(dsim, &regs->up_region);
		win_update_set_partial_size(decon, &regs->up_region);
		DPU_EVENT_LOG_APPLY_REGION(&decon->sd, &regs->up_region);
	}

	if (full_partial_update)
		kfree(regs);
}

void dpu_set_win_update_partial_size(struct decon_device *decon,
		struct decon_rect *up_region)
{
	if (!decon->win_up.enabled)
		return;

	win_update_set_partial_size(decon, up_region);
}

void dpu_init_win_update(struct decon_device *decon)
{
	struct exynos_panel_info *lcd = decon->lcd_info;

	decon->win_up.enabled = false;
	decon->cursor.xpos = lcd->xres / 2;
	decon->cursor.ypos = lcd->yres / 2;

	if (!IS_ENABLED(CONFIG_EXYNOS_WINDOW_UPDATE)) {
		decon_info("window update feature is disabled\n");
		return;
	}

	if (decon->dt.out_type != DECON_OUT_DSI) {
		decon_info("out_type(%d) doesn't support window update\n",
				decon->dt.out_type);
		return;
	}

	if (lcd->dsc.en) {
		decon->win_up.rect_w = lcd->xres / lcd->dsc.slice_num;
		decon->win_up.rect_h = lcd->dsc.slice_h;
	} else {
		decon->win_up.rect_w = MIN_WIN_BLOCK_WIDTH;
		decon->win_up.rect_h = MIN_WIN_BLOCK_HEIGHT;
	}

	DPU_FULL_RECT(&decon->win_up.prev_up_region, lcd);

	decon->win_up.hori_cnt = decon->lcd_info->xres / decon->win_up.rect_w;
	if (decon->lcd_info->xres - decon->win_up.hori_cnt * decon->win_up.rect_w) {
		decon_warn("%s: parameters is wrong. lcd w(%d), win rect w(%d)\n",
				__func__, decon->lcd_info->xres,
				decon->win_up.rect_w);
		return;
	}

	decon->win_up.verti_cnt = decon->lcd_info->yres / decon->win_up.rect_h;
	if (decon->lcd_info->yres - decon->win_up.verti_cnt * decon->win_up.rect_h) {
		decon_warn("%s: parameters is wrong. lcd h(%d), win rect h(%d)\n",
				__func__, decon->lcd_info->yres,
				decon->win_up.rect_h);
		return;
	}

	decon_info("window update is enabled: win rectangle w(%d), h(%d)\n",
			decon->win_up.rect_w, decon->win_up.rect_h);
	decon_info("horizontal count(%d), vertical count(%d)\n",
			decon->win_up.hori_cnt, decon->win_up.verti_cnt);

	decon->win_up.enabled = true;

	decon->mres_enabled = false;
	if (!IS_ENABLED(CONFIG_EXYNOS_MULTIRESOLUTION)) {
		decon_info("multi-resolution feature is disabled\n");
		return;
	}
	/* TODO: will be printed supported resolution list */
	decon_info("multi-resolution feature is enabled\n");
	decon->mres_enabled = true;
}
