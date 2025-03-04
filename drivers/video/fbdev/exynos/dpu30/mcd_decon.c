/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) Samsung Electronics Co., Ltd.
 * Gwanghui Lee <gwanghui.lee@samsung.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */


#include "mcd_decon.h"
#include "panel_drv.h"
#include "./cal_2100/decon_cal.h"

#ifdef CONFIG_MCDHDR
#include "./mcdhdr/mcdhdr.h"
#endif

#ifdef CONFIG_DYNAMIC_MIPI
#define DM_UPDATE_MAGIC_CODE 1

static int check_dm_update(struct dynamic_mipi_info *dm)
{
	int ret = 0;
	struct dm_status_info *dm_status;

	if (!dm) {
		decon_err("ERR:%s dm is null\n", __func__);
		return 0;
	}
	dm_status = &dm->dm_status;
	atomic_inc(&dm_status->frame_cnt);

	if ((dm_status->request_df != dm_status->target_df)
		&& (atomic_read(&dm_status->frame_cnt) >= dm_status->skip_frame_cnt)) {

		if (dm_status->req_ctx)
			decon_info("MCD:DM: %s:DM UPDATED %d->%d\n",
				__func__, dm_status->target_df, dm_status->request_df);

		dm_status->target_df = dm_status->request_df;
		ret = DM_UPDATE_MAGIC_CODE;
	}

	return ret;
}

static int check_dm_ffc_update(struct dynamic_mipi_info *dm)
{
	int ret = 0;
	struct dm_status_info *dm_status;

	if (!dm) {
		decon_err("ERR:%s dm is null\n", __func__);
		return 0;
	}

	dm_status = &dm->dm_status;
	if (dm_status->ffc_df != dm_status->request_df) {
		decon_info("MCD:DM: %s:DM FFC UPDATE REQUIRED %d->%d\n",
			__func__, dm_status->ffc_df, dm_status->request_df);
		ret = DM_UPDATE_MAGIC_CODE;
	}

	return ret;
}

static int mcd_dm_control_panel_ffc(struct mcd_decon_device *mcd_decon, unsigned int cmd)
{
	int ret = 0;
	struct decon_device *decon;

	if (!mcd_decon) {
		decon_err("MCD:DM:ERR:%s:mcd_decon is null\n", __func__);
		goto exit_control;
	}

	decon = container_of(mcd_decon, struct decon_device, mcd_decon);

	if (!decon->panel_sd)
		goto exit_control;

	ret = v4l2_subdev_call(decon->panel_sd, core, ioctl, cmd, NULL);
	if (ret)
		decon_err("MCD:DM:ERR:%s:failed to v4l2_subdevcall, cmd:%x\n", __func__, cmd);

exit_control:
	return ret;
}

static int mcd_dm_pre_change_freq(struct mcd_decon_device *mcd_decon)
{
	int ret = 0;
	struct decon_device *decon;
	struct dm_param_info dm_param;
	struct dynamic_mipi_info *dm;
	struct dm_status_info *dm_status;
	struct dm_hs_info *target_hs_info;

	if (!mcd_decon) {
		decon_err("MCD:DM:ERR:%s:mcd_decon is null\n", __func__);
		goto exit_pre_change;
	}

	dm = mcd_decon->dm_info;
	if (!dm) {
		decon_err("MCD:DM:ERR:%s dm is null\n", __func__);
		goto exit_pre_change;
	}

	dm_status = &dm->dm_status;
	decon = container_of(mcd_decon, struct decon_device, mcd_decon);

	target_hs_info = &dm->dm_dt.dm_hs_list[dm_status->target_df];

	/* fill dm param info */
	dm_param.req_ctx = dm_status->req_ctx;
	memcpy(&dm_param.pms, &target_hs_info->dphy_pms, sizeof(struct stdphy_pms));
	if (dm_status->req_ctx)
		decon_info("MCD:DN:INFO:%s:target hs: %d, m:%d, k:%d ctx:%d\n",
			__func__, target_hs_info->hs_clk, dm_param.pms.m, dm_param.pms.k, dm_param.req_ctx);

	ret = v4l2_subdev_call(decon->out_sd[0], core, ioctl,
			DSIM_IOC_MCD_DM_PRE_CHANGE_FREQ, &dm_param);
	if (ret)
		decon_err("MCD:DM:ERR:%s:failed to v4l2subdev DSIM_IOC_MCD_DM_PRE_CHANGE_FREQ\n", __func__);

exit_pre_change:
	return ret;
}

static int mcd_dm_post_change_freq(struct mcd_decon_device *mcd_decon)
{
	int ret = 0;
	struct decon_device *decon;
	struct dynamic_mipi_info *dm;
	struct dm_status_info *dm_status;
	struct dm_param_info dm_param;

	if (!mcd_decon) {
		decon_err("MCD:DM:ERR:%s mcd_decon is null\n", __func__);
		goto exit_post_change;
	}

	dm = mcd_decon->dm_info;
	if (!dm) {
		decon_err("MCD:DM:ERR:%s dm is null\n", __func__);
		goto exit_post_change;
	}

	dm_status = &dm->dm_status;
	decon = container_of(mcd_decon, struct decon_device, mcd_decon);
	memset(&dm_param, 0, sizeof(struct dm_param_info));

	if (decon->out_sd[0]) {
		ret = v4l2_subdev_call(decon->out_sd[0], core, ioctl, DSIM_IOC_MCD_DM_POST_CHANGE_FREQ, &dm_param);
		if (ret)
			decon_err("MCD:DM:ERR:%s:failed to DSIM_IOC_MCD_DM_POST_CHANGE_FREQ\n", __func__);
	}

exit_post_change:
	return ret;
}


static int mcd_dm_update_target_freq(struct mcd_decon_device *mcd_decon)
{
	int ret = 0;
	struct dynamic_mipi_info *dm;
	struct dm_status_info *dm_status;

	if (!mcd_decon) {
		decon_err("MCD:DM:ERR:%s mcd_decon is null\n", __func__);
		goto exit_update;
	}

	dm = mcd_decon->dm_info;
	if (!dm) {
		decon_err("MCD:DM:ERR:%s dm is null\n", __func__);
		goto exit_update;
	}

	dm_status = &dm->dm_status;
	if (dm_status->current_df == dm_status->target_df)
		decon_info("MCD:DM:INFO:%s: cur_df:%d, tar_df:%d\n", __func__,
			dm_status->current_df, dm_status->target_df);

	dm_status->current_df = dm_status->target_df;
	dm_status->req_ctx = 0;

exit_update:
	return ret;
}

void mcd_set_freq_hop(struct decon_device *decon, struct decon_reg_data *regs, bool en)
{
	struct dynamic_mipi_info *dm;
	struct mcd_decon_device *mcd_decon;
	struct mcd_decon_reg_data *mcd_regs;

	if ((!decon) || (!regs))
		return;

	mcd_decon = &decon->mcd_decon;
	mcd_regs = &regs->mcd_reg_data;
	dm = mcd_decon->dm_info;
	if (!dm)
		return;

	decon = container_of(mcd_decon, struct decon_device, mcd_decon);
	if ((decon->dt.out_type != DECON_OUT_DSI) ||
		(!decon->freq_hop.enabled) || (!dm->enabled))
		return;

	if (en) {
		mcd_regs->dm_update_flag = check_dm_update(dm);

		if (mcd_regs->dm_update_flag) {
#if defined(CONFIG_EXYNOS_PLL_SLEEP)
			/* wakeup PLL if sleeping... */
			decon_reg_set_pll_wakeup(decon->id, true);
			decon_reg_set_pll_sleep(decon->id, false);
#endif
			mcd_dm_pre_change_freq(mcd_decon);
		}
		if (check_dm_ffc_update(dm)) {
			if (mcd_dm_control_panel_ffc(mcd_decon, PANEL_IOC_DM_OFF_PANEL_FFC))
				decon_err("MCD:DM:ERR:%s: failed to PANEL_IOC_DM_OFF_PANEL_FFC\n", __func__);
		}
	} else {
		if (mcd_regs->dm_update_flag)
			mcd_dm_post_change_freq(mcd_decon);

		if (check_dm_ffc_update(dm)) {
			if (mcd_dm_control_panel_ffc(mcd_decon, PANEL_IOC_DM_SET_PANEL_FFC))
				decon_err("MCD:DM:ERR:%s: failed to PANEL_IOC_DM_SET_PANEL_FFC\n", __func__);
		}
		if (mcd_regs->dm_update_flag) {
#if defined(CONFIG_EXYNOS_PLL_SLEEP)
			decon_reg_set_pll_sleep(decon->id, true);
#endif
			mcd_dm_update_target_freq(mcd_decon);
		}
	}
}
#endif



#ifdef CONFIG_MCDHDR

static unsigned int check_hdr_header_info(struct fd_hdr_header *header)
{
	unsigned int sz = 0;

	if (!header) {
		decon_err("%s header is null\n", __func__);
		return 0;
	}

	sz = header->total_bytesize;
	if (sz < sizeof(struct fd_hdr_header)) {
		decon_err("%s: error wrong size: %d\n", __func__, header->total_bytesize);
		return 0;
	}

	if (header->type.unpack.hw_type != HDR_HW_MCD) {
		decon_err("%s: wrong hw type: %d\n", __func__, header->type.unpack.hw_type);
		return 0;
	}

	return sz;
}

static struct fd_hdr_header *mcd_map_hdr_info(struct saved_hdr_info *saved_hdr, int fd_hdr)
{
	struct fd_hdr_header *header;
	struct dma_buf *hdr_buf = NULL;

	if (!saved_hdr) {
		decon_err("%s invalid, saved hdr is null %d\n", __func__, fd_hdr);
		goto exit_map;
	}

	hdr_buf = dma_buf_get(fd_hdr);
	if (IS_ERR_OR_NULL(hdr_buf)) {
		decon_err("%s failed to get dma_buf:%ld, fd_hdr: %d\n", __func__, PTR_ERR(hdr_buf), fd_hdr);
		goto exit_map;
	}

	header = (struct fd_hdr_header *)dma_buf_vmap(hdr_buf);
	if (IS_ERR_OR_NULL(header)) {
		decon_err("%s: header from dma_buf_vmap is null, fd: %d\n", __func__, fd_hdr);
		goto exit_map_buf_put;
	}
	saved_hdr->fd = fd_hdr;
	saved_hdr->dma_buf = hdr_buf;
	saved_hdr->header = header;

	return header;

exit_map_buf_put:
	dma_buf_put(hdr_buf);
exit_map:
	return NULL;
}

static void mcd_unmap_hdr_info(struct saved_hdr_info *saved_hdr)
{
	if (!saved_hdr) {
		decon_err("%s: saved hdr is null\n", __func__);
		return;
	}

	if ((saved_hdr->dma_buf != NULL) && (saved_hdr->header != NULL)) {
		dma_buf_vunmap(saved_hdr->dma_buf, saved_hdr->header);
		dma_buf_put(saved_hdr->dma_buf);

		saved_hdr->dma_buf = NULL;
		saved_hdr->header = NULL;
		saved_hdr->fd = 0;
	}
}


static void *mcd_get_hdr_info(struct decon_device *decon, int fd_hdr, int idx)
{
	void *hdr_info = NULL;
	unsigned int bytesize = 0;
	struct dma_buf *hdr_buf = NULL;
	struct fd_hdr_header *header = NULL;
	struct saved_hdr_info *saved_hdr = NULL;
	struct mcd_decon_device *mcd_decon = &decon->mcd_decon;

	saved_hdr = &mcd_decon->saved_hdr[idx];
	if (!saved_hdr) {
		decon_err("%s: invalid arg, mcd_decon is null\n", __func__);
		return NULL;
	}

	hdr_buf = dma_buf_get(fd_hdr);
	if (IS_ERR_OR_NULL(hdr_buf)) {
		decon_err("%s failed to get dma_buf:%ld, fd_hdr: %d\n", __func__, PTR_ERR(hdr_buf), fd_hdr);
		goto exit_get;
	}

	if ((saved_hdr->fd == fd_hdr) && (saved_hdr->dma_buf == hdr_buf)) {
		if (!saved_hdr->header) {
			decon_err("%s: header is null, idx: %d, fd: %d\n", __func__, idx, fd_hdr);
			goto exit_buf_put;
		}
		header = saved_hdr->header;
	} else {
		/* free saved buffer info */
		mcd_unmap_hdr_info(saved_hdr);
		header = mcd_map_hdr_info(saved_hdr, fd_hdr);
		if (!header) {
			decon_err("%s: header from dma_buf_vmap is null, idx: %d, fd: %d\n", __func__, idx, fd_hdr);
			goto exit_buf_put;
		}
	}

	bytesize = check_hdr_header_info(header);
	if (!bytesize) {
		decon_err("%s: invalid header\n", __func__);
		goto exit_dma_buf;
	}

	hdr_info = kzalloc(bytesize, GFP_KERNEL);
	if (!hdr_info) {
		decon_err("%s: failed to alloc hdr_info, size: %d\n", __func__, header->total_bytesize);
		goto exit_dma_buf;
	}
	if (bytesize != header->total_bytesize) {
		decon_err("%s: hdr header info was changed %d -> %d\n", __func__, bytesize, header->total_bytesize);
		kfree(hdr_info);
		goto exit_dma_buf;
	}
	memcpy(hdr_info, header, bytesize);

#ifdef DEBUG_MCDHDR
	decon_dbg("hdr header info-> size: %d, layer: %d, log: %d, shall: %d, need: %d\n",
		header->total_bytesize, header->type.unpack.layer_index,  header->type.unpack.log_level,
		header->num.unpack.shall, header->num.unpack.need);
#endif
	dma_buf_put(hdr_buf);

	return hdr_info;

exit_dma_buf:
	mcd_unmap_hdr_info(saved_hdr);

exit_buf_put:
	dma_buf_put(hdr_buf);

exit_get:
	return NULL;
}


void mcd_unmap_all_hdr_info(struct decon_device *decon)
{
	int i;
	struct saved_hdr_info *saved_hdr = NULL;
	struct mcd_decon_device *mcd_decon = &decon->mcd_decon;

	if (!mcd_decon) {
		decon_err("%s: invalid arg, mcd_decon is null\n", __func__);
		return;
	}

	for (i = 0; i < MAX_MCDHDR_CNT; i++) {
		saved_hdr = &mcd_decon->saved_hdr[i];

		if (saved_hdr)
			mcd_unmap_hdr_info(saved_hdr);
	}
}

int mcd_prepare_hdr_config(struct decon_device *decon,
	struct decon_win_config_data *win_data, struct decon_reg_data *regs)
{
	int i;
	int ret = 0;
	struct decon_win_config *config;
	struct decon_win_config *win_config = win_data->config;

	for (i = 0; i < decon->dt.max_win; i++) {
		config = &win_config[i];

		if (config->state != DECON_WIN_STATE_BUFFER)
			continue;

		if (config->fd_hdr <= 0) {
			goto exit_get;
		}
		regs->mcd_reg_data.hdr_info[i] = mcd_get_hdr_info(decon, config->fd_hdr, i);
		if (!regs->mcd_reg_data.hdr_info[i]) {
			decon_err("%s: failed to alloc for hdr info\n", __func__);
			ret = -ENOMEM;
		}
	}
exit_get:
	return ret;
}


void mcd_clear_hdr_info(struct decon_device *decon, struct decon_reg_data *regs)
{
	int i;

	for (i = 0; i < decon->dt.max_win; i++)
		regs->mcd_reg_data.hdr_info[i] = NULL;

}

void mcd_free_hdr_info(struct decon_device *decon, struct decon_reg_data *regs)
{
	int i;

	for (i = 0; i < decon->dt.max_win; i++)
		kfree(regs->mcd_reg_data.hdr_info[i]);

}


void mcd_fill_hdr_info(int win_idx, struct dpp_config *config, struct decon_reg_data *regs)
{
	config->mcd_config.hdr_info = regs->mcd_reg_data.hdr_info[win_idx];
}

#endif


bool is_hmd_not_running(struct decon_device *decon)
{
#ifdef CONFIG_SUPPORT_HMD
	if ((decon->panel_state != NULL) && (decon->panel_state->hmd_on))
		return false;
#endif
	return true;
}

static char *dpp_state_str[] = {
	"WIN_DISABLE",
	"WIN_COLOR_",
	"WIN_BUFFER",
	"WIN_UPDATE",
	"WIN_CURSOR",
};

static char *dpp_rot_str[] = {
	"NORMAL",
	"XFLIP",
	"YFLIP",
	"ROT180",
	"ROT90",
	"ROT90_XFLIP",
	"ROT90_YFLIP",
	"ROT270",
};

static char *csc_type_str[] = {
	"BT_601",
	"BT_709",
	"BT_2020",
	"DCI_P3",
	"BT_601_625",
	"BT_601_625_UN",
	"BT_601_525",
	"BT_601_525_UN",
	"BT_2020_CON",
	"BT_470M",
	"FILM",
	"ADOBE_RGB",
	"BT709_C",
	"BT2020_C",
	"DCI_P3_C",
};

static char *csc_range_str[] = {
	"RANGE_LIMITED",
	"RAMGE_FULL",
	"RANGE_EXTENDED",
};

static char *hdr_str[] = {
	"HDR_OFF",
	"HDR_ST2084",
	"HDR_HLG",
	"LINEAR",
	"SRGB",
	"SMPTE_170M",
	"GAMMA2_2",
	"GAMMA2_6",
	"GAMMA2_8",
};

static ssize_t last_winconfig_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	int i;
	int count = 0;
	u32 type, range;
	struct decon_reg_data *last_reg_data;
	struct decon_device *decon = dev_get_drvdata(dev);
	struct decon_win_config *dpp_config;
	const struct dpu_fmt *fmt_info;

	if (!decon) {
		decon_info("%s: decon is null\n", __func__);
		goto err_show;
	}
	last_reg_data = &decon->last_regs;

	for (i = 0; i < decon->dt.max_win + 2; i++) {
		dpp_config = &last_reg_data->dpp_config[i];
		if (dpp_config == NULL) {
			decon_info("dpp_config is null\n");
			continue;
		}

		fmt_info = dpu_find_fmt_info(dpp_config->format);
		type = (dpp_config->dpp_parm.eq_mode >> CSC_STANDARD_SHIFT) & 0x3F;
		range = (dpp_config->dpp_parm.eq_mode >> CSC_RANGE_SHIFT) & 0x7;

		if ((dpp_config->state != DECON_WIN_STATE_BUFFER) &&
			(dpp_config->state != DECON_WIN_STATE_COLOR) && (dpp_config->state != DECON_WIN_STATE_CURSOR))
			continue;

		count += sprintf(buf + count, "idx: %02d, %s src: %04d %04d %04d %04d, dst: %04d %04d %04d %04d: ",
			i, dpp_state_str[dpp_config->state],
			dpp_config->src.x, dpp_config->src.y, dpp_config->src.w, dpp_config->src.h,
			dpp_config->dst.x, dpp_config->dst.y, dpp_config->dst.w, dpp_config->dst.h);

		count += sprintf(buf + count, "rot: %s, comp:%01d, fmt: %s, eq: 0x%08x (",
			dpp_rot_str[dpp_config->dpp_parm.rot], dpp_config->compression, fmt_info->name,
			dpp_config->dpp_parm.eq_mode);

		if (type == CSC_STANDARD_UNSPECIFIED)
			count += sprintf(buf + count, "type: Unspecified");
		else
			count += sprintf(buf + count, "type: %s, ", (type > CSC_DCI_P3_C) ? "NA" : csc_type_str[type]);

		if (range == CSC_RANGE_UNSPECIFIED)
			count += sprintf(buf + count, "range: Unspecified)");
		else
			count += sprintf(buf + count, "range: %s)", (range > CSC_RANGE_EXTENDED) ? "NA" : csc_range_str[range]);

		 count += sprintf(buf + count, " hdr: %s\n",
			(dpp_config->dpp_parm.hdr_std > DPP_TRANSFER_GAMMA2_8 || dpp_config->dpp_parm.hdr_std < 0) ?
			"NA" : hdr_str[dpp_config->dpp_parm.hdr_std]);

	}
err_show:
	return count;
}
static DEVICE_ATTR(d_last_win, 0440, last_winconfig_show, NULL);


static struct attribute *mcd_debug_attrs[] = {
	&dev_attr_d_last_win.attr,
	NULL,
};

static struct attribute_group mcd_debug_group = {
	.attrs = mcd_debug_attrs,
};

int mcd_decon_create_debug_sysfs(struct decon_device *decon)
{
	int ret = 0;

	ret = sysfs_create_group(&decon->dev->kobj, &mcd_debug_group);
	if (ret) {
		decon_err("%s failed to create debug sysfs gruop: %d\n", __func__, ret);
		goto err_create;
	}
err_create:
	return ret;
}

#if IS_ENABLED(CONFIG_MCD_PANEL)
void mcd_decon_panel_dump(struct decon_device *decon)
{
	int acquired = console_trylock();

	if (IS_DECON_OFF_STATE(decon)) {
		decon_info("%s: DECON%d is disabled, state(%d)\n",
				__func__, decon->id, decon->state);
		goto out;
	}

	if (decon->dt.out_type == DECON_OUT_DSI)
		v4l2_subdev_call(decon->out_sd[0], core, ioctl,
				DSIM_IOC_PANEL_DUMP, NULL);

out:
	if (acquired)
		console_unlock();
}

void mcd_decon_flush_image(struct decon_device *decon)
{
	if (!decon)
		return;

	decon_hiber_block_exit(decon);
	mutex_lock(&decon->lock);
	if (IS_DECON_ON_STATE(decon))
		kthread_flush_worker(&decon->up.worker);
	mutex_unlock(&decon->lock);
	decon_hiber_unblock(decon);
}
#endif

#if defined(CONFIG_DECON_BTS_VRR_ASYNC)
static void fps_sync_init(struct fps_sync *sync)
{
	mutex_init(&sync->lock);
	INIT_LIST_HEAD(&sync->head);
	sync->candidate_fps = 0;
	sync->sync_pt_count = 0;
}

static int fps_sync_update_timeline(struct fps_sync *sync, u64 timeline)
{
	struct fps_sync_pt *sync_pt, *tmp;
	u64 seqno = 0;

	list_for_each_entry_safe(sync_pt, tmp, &sync->head, list) {
		if (timeline < sync_pt->seqno)
			continue;

		if (seqno <= sync_pt->seqno) {
			seqno = sync_pt->seqno;
			sync->applied_fps = sync_pt->fps;
		}

		list_del(&sync_pt->list);
		kfree(sync_pt);
		sync->sync_pt_count--;
	}

	return 0;
}

static u32 fps_sync_get_maximum_fps(struct fps_sync *sync, u64 timeline)
{
	u32 maximum_fps = 0;
	struct fps_sync_pt *sync_pt;

	list_for_each_entry(sync_pt, &sync->head, list)
		maximum_fps = max(maximum_fps, sync_pt->fps);

	return max(maximum_fps, sync->candidate_fps);
}

static void fps_sync_add_sync_pt(struct fps_sync *sync, u64 seqno, u32 fps)
{
	struct fps_sync_pt *sync_pt;

	sync_pt = kzalloc(sizeof(struct fps_sync_pt), GFP_KERNEL);
	sync_pt->seqno = seqno;
	sync_pt->fps = fps;
	list_add_tail(&sync_pt->list, &sync->head);
	sync->sync_pt_count++;
}

static void fps_sync_set_candidate_fps(struct fps_sync *sync, u32 fps)
{
	if (sync->candidate_fps != 0)
		pr_warn("%s overwrite candidate_fps (%d->%d)\n",
				__func__, sync->candidate_fps, fps);

	sync->candidate_fps = fps;
}

static void fps_sync_clear_candidate_fps(struct fps_sync *sync)
{
	sync->candidate_fps = 0;
}

static void fps_sync_remove_all(struct fps_sync *sync)
{
	struct fps_sync_pt *sync_pt, *tmp;

	list_for_each_entry_safe(sync_pt, tmp, &sync->head, list) {
		list_del(&sync_pt->list);
		kfree(sync_pt);
		sync->sync_pt_count--;
	}

	fps_sync_clear_candidate_fps(sync);

	sync->applied_fps = 0;
}

static unsigned int fps_sync_snprintf(struct fps_sync *sync, char *buf, size_t size)
{
	struct fps_sync_pt *sync_pt, *tmp;
	unsigned int len = 0;

	len += snprintf(buf + len, size - len, "====== fps_sync ======\n");
	list_for_each_entry_safe(sync_pt, tmp, &sync->head, list)
		len += snprintf(buf + len, size - len,
				"seqno:%lld fps:%d\n", sync_pt->seqno, sync_pt->fps);
	len += snprintf(buf + len, sizeof(buf) - len,
			"applied_fps:%d\n", sync->applied_fps);
	len += snprintf(buf + len, sizeof(buf) - len,
			"candidate_fps:%d\n", sync->candidate_fps);
	len += snprintf(buf + len, size - len, "======================\n");

	return len;
}

void decon_bts_print_fps_sync(struct decon_bts *bts)
{
	char buf[256];

	fps_sync_snprintf(&bts->fps_sync, buf, sizeof(buf));

	pr_info("%s", buf);
}

void decon_bts_init_fps_sync(struct decon_bts *bts)
{
	fps_sync_init(&bts->fps_sync);
}

void decon_bts_set_applied_fps(struct decon_bts *bts, u32 fps)
{
	mutex_lock(&bts->fps_sync.lock);
	bts->fps_sync.applied_fps = fps;
	mutex_unlock(&bts->fps_sync.lock);
}

int decon_bts_get_bts_fps(struct decon_bts *bts, u64 timeline)
{
	u32 bts_fps;

	mutex_lock(&bts->fps_sync.lock);
	fps_sync_update_timeline(&bts->fps_sync, timeline);
	bts_fps = max(bts->fps_sync.applied_fps,
			fps_sync_get_maximum_fps(&bts->fps_sync, timeline));
	mutex_unlock(&bts->fps_sync.lock);

	if (bts_fps == 0) {
		pr_warn("%s set default bts_fps(%d)\n",
				__func__, DEFAULT_BTS_FPS);
		bts_fps = DEFAULT_BTS_FPS;
	}

	return bts_fps;
}

int decon_bts_add_fps_sync(struct decon_bts *bts, u64 seqno, u32 fps)
{
	mutex_lock(&bts->fps_sync.lock);
	if (bts->fps_sync.sync_pt_count >= MAX_SYNC_PT_COUNT) {
		pr_warn("%s exceeded maximum sync_pt_count(%d)\n",
				__func__, MAX_SYNC_PT_COUNT);
		fps_sync_remove_all(&bts->fps_sync);
	}

	fps_sync_add_sync_pt(&bts->fps_sync, seqno, fps);
	mutex_unlock(&bts->fps_sync.lock);

	return 0;
}

int decon_bts_clear_fps_sync(struct decon_bts *bts)
{
	mutex_lock(&bts->fps_sync.lock);
	fps_sync_remove_all(&bts->fps_sync);
	mutex_unlock(&bts->fps_sync.lock);

	return 0;
}

int decon_bts_set_candidate_fps(struct decon_bts *bts, u32 fps)
{
	mutex_lock(&bts->fps_sync.lock);
	fps_sync_set_candidate_fps(&bts->fps_sync, fps);
	mutex_unlock(&bts->fps_sync.lock);

	return 0;
}

int decon_bts_clear_candidate_fps(struct decon_bts *bts)
{
	mutex_lock(&bts->fps_sync.lock);
	fps_sync_clear_candidate_fps(&bts->fps_sync);
	mutex_unlock(&bts->fps_sync.lock);

	return 0;
}

int decon_bts_accept_candidate_fps(struct decon_bts *bts, u64 seqno)
{
	mutex_lock(&bts->fps_sync.lock);
	if (bts->fps_sync.candidate_fps == 0) {
		pr_warn("%s candidate_fps not exist\n", __func__);
		mutex_unlock(&bts->fps_sync.lock);
		return -EINVAL;
	}

	fps_sync_add_sync_pt(&bts->fps_sync,
			seqno, bts->fps_sync.candidate_fps);
	fps_sync_clear_candidate_fps(&bts->fps_sync);
	mutex_unlock(&bts->fps_sync.lock);

	return 0;
}
#endif /* CONFIG_DECON_BTS_VRR_ASYNC */
