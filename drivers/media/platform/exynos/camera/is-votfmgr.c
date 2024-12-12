/*
 * Samsung Exynos SoC series Pablo driver
 *
 * Copyright (c) 2019 Samsung Electronics Co., Ltd
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#include <linux/errno.h>

#include "votf/camerapp-votf.h"
#include "is-votfmgr.h"
#include "is-votf-id-table.h"
#include "is-groupmgr.h"
#include "is-device-ischain.h"
#include "is-device-sensor.h"

static int is_votf_get_votf_info(struct is_group *group, struct votf_info *src_info,
	struct votf_info *dst_info, char *caller_fn)
{
	int ret;
	struct is_device_sensor *sensor;
	struct is_group *src_gr;
	unsigned int src_gr_id;
	struct is_subdev *src_sd;
	int dma_ch;
	struct is_sensor_cfg *sensor_cfg;
	u32 votf_mode = VOTF_NORMAL;

	FIMC_BUG(!group);
	FIMC_BUG(!group->prev);
	FIMC_BUG(!group->prev->junction);

	src_gr = group->prev;
	src_sd = group->prev->junction;

	if (src_gr->device_type == IS_DEVICE_SENSOR) {
		/*
		 * The sensor group id should be re calculated,
		 * because CSIS WDMA is not matched with sensor group id.
		 */
		sensor = src_gr->sensor;

		sensor_cfg = sensor->cfg;
		if (!sensor_cfg) {
			mgerr("failed to get sensor_cfg(%s)", group, group, caller_fn);
			return -EINVAL;
		}

		ret = v4l2_subdev_call(sensor->subdev_csi, core, ioctl,
			SENSOR_IOCTL_G_DMA_CH, &dma_ch);
		if (ret) {
			mgerr("failed to get dma_ch(%s)", group, group, caller_fn);
			return ret;
		}

		src_gr_id = GROUP_ID_SS0 + dma_ch;

		if (sensor_cfg->ex_mode == EX_DUALFPS_480 ||
		    sensor_cfg->ex_mode == EX_DUALFPS_960)
			votf_mode = VOTF_FRS;
	} else {
		src_gr_id = src_gr->id;
	}

	src_info->service = TWS;
	src_info->ip = is_votf_ip[src_gr_id];
	src_info->id = is_votf_id[src_gr_id][src_sd->id];
	src_info->mode = votf_mode;

	dst_info->service = TRS;
	dst_info->ip = is_votf_ip[group->id];
	dst_info->id = is_votf_id[group->id][group->leader.id];
	dst_info->mode = votf_mode;

	if (caller_fn)
		mginfo("IMG: %s(TWS[%s:%s]-TRS[%s:%s] mode %d)\n",
		group, group, caller_fn,
		group_id_name[src_gr->id], src_sd->name,
		group_id_name[group->id], group->leader.name,
		votf_mode);

	return 0;
}

static int is_votf_get_pd_votf_info(struct is_group *group, struct votf_info *src_info,
	struct votf_info *dst_info, char *caller_fn)
{
	int ret;
	struct is_device_sensor *sensor;
	struct is_group *src_gr;
	unsigned int src_gr_id;
	struct is_subdev *src_sd;
	int dma_ch;
	struct is_sensor_cfg *sensor_cfg;
	int pd_mode;
	u32 votf_mode = VOTF_NORMAL;

	FIMC_BUG(!group);
	FIMC_BUG(!group->prev);
	FIMC_BUG(!group->prev->junction);

	src_gr = group->prev;
	src_sd = group->prev->junction;

	if (src_gr->device_type == IS_DEVICE_SENSOR) {
		/*
		 * The sensor group id should be re calculated,
		 * because CSIS WDMA is not matched with sensor group id.
		 */
		sensor = src_gr->sensor;

		sensor_cfg = sensor->cfg;
		if (!sensor_cfg) {
			mgerr("failed to get sensor_cfg(%s)", group, group, caller_fn);
			return -EINVAL;
		}

		pd_mode = sensor_cfg->pd_mode;
		if (pd_mode != PD_MOD3 && pd_mode != PD_MSPD_TAIL)
			return -ECHILD;

		ret = v4l2_subdev_call(sensor->subdev_csi, core, ioctl,
			SENSOR_IOCTL_G_DMA_CH, &dma_ch);
		if (ret) {
			mgerr("failed to get dma_ch(%s)", group, group, caller_fn);
			return ret;
		}

		src_gr_id = GROUP_ID_SS0 + dma_ch;

		if (sensor_cfg->ex_mode == EX_DUALFPS_480 ||
		    sensor_cfg->ex_mode == EX_DUALFPS_960)
			votf_mode = VOTF_FRS;
	} else {
		return -ECHILD;
	}

	src_info->service = TWS;
	src_info->ip = is_votf_ip[src_gr_id];
	src_info->id = is_votf_id[src_gr_id][ENTRY_SSVC1];
	src_info->mode = votf_mode;

	dst_info->service = TRS;
	dst_info->ip = is_votf_ip[group->id];
	dst_info->id = is_votf_id[group->id][ENTRY_PDAF];
	dst_info->mode = votf_mode;

	if (caller_fn)
		mginfo("PD: %s(TWS[%s:%s]-TRS[%s:%s])\n",
		group, group, caller_fn,
		group_id_name[src_gr->id], src_sd->name,
		group_id_name[group->id], group->leader.name);

	return 0;
}

static int is_votf_flush(struct is_group *group)
{
	int ret;
	struct votf_info src_info, dst_info;

	ret = is_votf_get_votf_info(group, &src_info, &dst_info, (char *)__func__);
	if (ret)
		return ret;

	ret = __is_votf_flush(&src_info, &dst_info);
	if (ret)
		mgerr("__is_votf_flush is fail(%d)", group, group, ret);

	if (group->prev->device_type == IS_DEVICE_SENSOR) {
		ret = is_votf_get_pd_votf_info(group, &src_info, &dst_info, (char *)__func__);
		if (ret) {
			if (ret == -ECHILD)
				return 0;
			else
				return ret;
		}

		ret = __is_votf_flush(&src_info, &dst_info);
		if (ret)
			mgerr("__is_votf_flush of subdev_pdaf is fail(%d)", group, group, ret);
	}

	is_votf_subdev_flush(group);

	return 0;
}

static int is_votf_alloc_internal_buffer(struct is_group *group, u32 width, u32 height)
{
	int ret, ret_err;
	struct is_subdev *leader = &group->leader;
	struct is_device_ischain *device;
	struct is_device_sensor *sensor;
	struct is_sensor_cfg *sensor_cfg;
	u32 bpp;
	struct is_subdev *subdev_pdaf;
	u32 pd_width, pd_height;
	struct votf_info src_info, dst_info;

	device = group->device;
	if (!device) {
		mgerr("failed to get devcie", group, group);
		return -ENODEV;
	}

	set_bit(IS_SUBDEV_INTERNAL_USE, &leader->state);

	sensor = device->sensor;
	sensor_cfg = sensor->cfg;

	bpp = is_subdev_internal_g_bpp(device, IS_DEVICE_ISCHAIN,
					leader, sensor_cfg);

	ret = is_subdev_internal_s_format(device, IS_DEVICE_ISCHAIN,
					leader, width, height, bpp,
					NUM_OF_VOTF_BUF, "VOTF");
	if (ret) {
		merr("is_subdev_internal_s_format is fail(%d)", device, ret);
		return ret;
	}

	ret = is_subdev_internal_start(device, IS_DEVICE_ISCHAIN, leader);
	if (ret) {
		merr("subdev internal start is fail(%d)", device, ret);
		return ret;
	}

	/* PD node */
	ret = is_votf_get_pd_votf_info(group, &src_info, &dst_info, NULL);
	if (ret) {
		if (ret == -ECHILD)
			return 0;
		else
			return ret;
	}

	subdev_pdaf = &device->pdaf;
	pd_width = sensor_cfg->input[CSI_VIRTUAL_CH_1].width;
	pd_height = sensor_cfg->input[CSI_VIRTUAL_CH_1].height;

	ret = is_subdev_internal_s_format(device, 0, subdev_pdaf,
					pd_width, pd_height, 16, NUM_OF_VOTF_BUF, "VOTF");
	if (ret) {
		merr("is_subdev_internal_s_format is fail(%d)", device, ret);
		goto err_subdev_pdaf_s_format;
	}

	ret = is_subdev_internal_start(device, IS_DEVICE_ISCHAIN, subdev_pdaf);
	if (ret) {
		merr("subdev_pdaf internal start is fail(%d)", device, ret);
		goto err_subdev_pdaf_start;
	}

	return 0;

err_subdev_pdaf_start:
err_subdev_pdaf_s_format:
	ret_err = is_subdev_internal_stop(device, IS_DEVICE_ISCHAIN, leader);
	if (ret_err)
		merr("subdev internal stop is fail(%d)", device, ret_err);

	return ret;
}

static int is_votf_free_internal_buffer(struct is_group *group)
{
	int ret;
	struct is_subdev *leader = &group->leader;
	struct is_device_ischain *device;
	struct is_subdev *subdev_pdaf;
	struct votf_info src_info, dst_info;

	device = group->device;
	if (!device) {
		mgerr("failed to get devcie", group, group);
		return -EINVAL;
	}

	ret = is_subdev_internal_stop(device, 0, leader);
	if (ret)
		merr("subdev internal stop is fail(%d)", device, ret);

	clear_bit(IS_SUBDEV_INTERNAL_USE, &leader->state);

	/* PD node */
	ret = is_votf_get_pd_votf_info(group, &src_info, &dst_info, NULL);
	if (ret) {
		if (ret == -ECHILD)
			return 0;
		else
			return ret;
	}

	subdev_pdaf = &device->pdaf;

	ret = is_subdev_internal_stop(device, 0, subdev_pdaf);
	if (ret)
		merr("subdev internal stop is fail(%d)", device, ret);

	return ret;
}

int is_votf_check_wait_con(struct is_group *group)
{
	int ret;
	struct votf_info src_info, dst_info;

	ret = is_votf_get_votf_info(group, &src_info, &dst_info, NULL);
	if (ret)
		return ret;

	ret = votfitf_check_wait_con(&src_info, &dst_info);
	if (ret)
		mgerr("votfitf_check_wait_con is fail(%d)", group, group, ret);

	if (group->prev->device_type == IS_DEVICE_SENSOR) {
		ret = is_votf_get_pd_votf_info(group, &src_info, &dst_info, NULL);
		if (ret) {
			if (ret == -ECHILD)
				return 0;
			else
				return ret;
		}

		ret = votfitf_check_wait_con(&src_info, &dst_info);
		if (ret)
			mgerr("votfitf_check_wait_con of subdev_pdaf is fail(%d)",
				group, group, ret);
	}

	return 0;
}

int is_votf_check_invalid_state(struct is_group *group)
{
	int ret;
	struct votf_info src_info, dst_info;

	ret = is_votf_get_votf_info(group, &src_info, &dst_info, NULL);
	if (ret)
		return ret;

	ret = votfitf_check_invalid_state(&src_info, &dst_info);
	if (ret)
		mgerr("votfitf_check_invalid_state is fail(%d)", group, group, ret);

	if (group->prev->device_type == IS_DEVICE_SENSOR) {
		ret = is_votf_get_pd_votf_info(group, &src_info, &dst_info, NULL);
		if (ret) {
			if (ret == -ECHILD)
				return 0;
			else
				return ret;
		}

		ret = votfitf_check_invalid_state(&src_info, &dst_info);
		if (ret)
			mgerr("votfitf_check_invalid_state of subdev_pdaf is fail(%d)",
				group, group, ret);
	}

	return 0;
}

int is_votf_link_set_service_cfg(struct votf_info *src_info,
		struct votf_info *dst_info,
		u32 width, u32 height, u32 change)
{
	int ret = 0;
	struct votf_service_cfg cfg;
	struct votf_lost_cfg lost_cfg;

	memset(&cfg, 0, sizeof(struct votf_service_cfg));

	/* TWS: Master */
	src_info->service = TWS;

	cfg.enable = 0x1;
	/*
	 * 0xFF is max value.
	 * Buffer size is (limit x token_size).
	 * But VOTF can hold only 1 frame.
	 */
	cfg.limit = 0xFF;
	cfg.width = width;
	cfg.height = height & 0xFFFF;
	cfg.token_size = is_votf_get_token_size(src_info);
	cfg.connected_ip = dst_info->ip;
	cfg.connected_id = dst_info->id;
	cfg.option = change;

	ret = votfitf_set_service_cfg(src_info, &cfg);
	if (ret < 0) {
		ret = -EINVAL;
		err("TWS votf set_service_cfg fail. TWS 0x%04X-%d TRS 0x%04X-%d",
				src_info->ip, src_info->id,
				dst_info->ip, dst_info->id);

		return ret;
	}

	/* TRS: Slave */
	dst_info->service = TRS;

	cfg.enable = 0x1;
	/*
	 * 0xFF is max value.
	 * Buffer size is (limit x token_size).
	 * But VOTF can hold only 1 frame.
	 */
	cfg.limit = 0xFF;
	cfg.width = width;
	cfg.height = (height >> 16) ? (height >> 16) : height;
	cfg.token_size = is_votf_get_token_size(dst_info);
	cfg.connected_ip = src_info->ip;
	cfg.connected_id = src_info->id;
	cfg.option = change;

	ret = votfitf_set_service_cfg(dst_info, &cfg);
	if (ret < 0) {
		ret = -EINVAL;
		err("TRS votf set_service_cfg fail. TWS 0x%04X-%d TRS 0x%04X-%d",
				src_info->ip, src_info->id,
				dst_info->ip, dst_info->id);

		return ret;
	}

	memset(&lost_cfg, 0, sizeof(struct votf_lost_cfg));

	lost_cfg.recover_enable = 0x1;
	/* lost_cfg.flush_enable = 0x1; */
	ret = votfitf_set_trs_lost_cfg(dst_info, &lost_cfg);
	if (ret < 0) {
		ret = -EINVAL;
		err("TRS votf set_lost_cfg fail. TWS 0x%04X-%d TRS 0x%04X-%d",
				src_info->ip, src_info->id,
				dst_info->ip, dst_info->id);
		return ret;
	}

	info(" VOTF create link size %dx%d TWS 0x%04X-%d(%d) TRS 0x%04X-%d(%d)\n",
			width, height,
			src_info->ip, src_info->id, src_info->mode,
			dst_info->ip, dst_info->id, dst_info->mode);

	return 0;
}

int __is_votf_flush(struct votf_info *src_info,
		struct votf_info *dst_info)
{
	int ret = 0;

	/* TWS: Master */
	src_info->service = TWS;

	ret = votfitf_set_flush(src_info);
	if (ret < 0)
		err("TWS votf_flush fail. TWS 0x%04X-%d TRS 0x%04X-%d",
			src_info->ip, src_info->id,
			dst_info->ip, dst_info->id);

	/* TRS: Slave */
	dst_info->service = TRS;

	ret = votfitf_set_flush(dst_info);
	if (ret < 0)
		err("TRS votf_flush fail. TWS 0x%04X-%d TRS 0x%04X-%d",
			src_info->ip, src_info->id,
			dst_info->ip, dst_info->id);

	info("VOTF flush TWS 0x%04X-%d TRS 0x%04X-%d\n",
		src_info->ip, src_info->id,
		dst_info->ip, dst_info->id);

	return 0;
}

int __is_votf_force_flush(struct votf_info *src_info,
		struct votf_info *dst_info)
{
	int ret = 0;

	/* TWS: Master */
	src_info->service = TWS;

	ret = votfitf_set_flush_alone(src_info);
	if (ret < 0)
		err("TWS votf_flush fail. TWS 0x%04X-%d TRS 0x%04X-%d",
			src_info->ip, src_info->id,
			dst_info->ip, dst_info->id);

	/* TRS: Slave */
	dst_info->service = TRS;

	ret = votfitf_set_flush_alone(dst_info);
	if (ret < 0)
		err("TRS votf_flush fail. TWS 0x%04X-%d TRS 0x%04X-%d",
			src_info->ip, src_info->id,
			dst_info->ip, dst_info->id);

	info("VOTF flush TWS 0x%04X-%d TRS 0x%04X-%d\n",
		src_info->ip, src_info->id,
		dst_info->ip, dst_info->id);

	return 0;
}

int is_votf_create_link_sensor(struct is_group *group, u32 width, u32 height)
{
	int ret = 0;
	struct votf_info src_info, dst_info;

	if (group->prev && group->prev->device_type == IS_DEVICE_ISCHAIN)
		return 0;

	ret = is_votf_get_votf_info(group, &src_info, &dst_info, (char *)__func__);
	if (ret)
		return ret;

#if defined(USE_VOTF_AXI_APB)
	votfitf_create_link(src_info.ip, dst_info.ip);
#elif
	votfitf_create_ring();
#endif

	/* is_votf_link_set_service_cfg() is moved variant part */

	/* Allocate VOTF internal buffer */
	ret = is_votf_alloc_internal_buffer(group, width, height);
	if (ret) {
		mgerr("failed to create suvdev link", group, group);
		goto err_alloc_internal_buffer;
	}

	return 0;

err_alloc_internal_buffer:
#if defined(USE_VOTF_AXI_APB)
	votfitf_destroy_link(src_info.ip, dst_info.ip);
#elif
	votfitf_destroy_ring();
#endif

	return ret;
}

int is_votf_destroy_link_sensor(struct is_group *group)
{
	int ret = 0;
	struct votf_info src_info, dst_info;
	struct is_framemgr *votf_fmgr;

	if (group->id >= GROUP_ID_MAX) {
		mgerr("group ID is invalid(%d)", group, group);

		/* Do not access C2SERV after power off */
		return 0;
	}

	if (group->prev && group->prev->device_type == IS_DEVICE_ISCHAIN)
		return 0;

	ret = is_votf_get_votf_info(group, &src_info, &dst_info, (char *)__func__);
	if (ret)
		return ret;

	ret = is_votf_flush(group);
	if (ret)
		mgerr("is_votf_flush is fail(%d)", group, group, ret);

	/*
	 * All VOTF control such as flush must be set in ring clock enable and ring enable.
	 * So, calling votfitf_destroy_ring must be called at the last.
	 */
#if defined(USE_VOTF_AXI_APB)
	votfitf_destroy_link(src_info.ip, dst_info.ip);
#else
	votfitf_destroy_ring();
#endif

	/*
	 * TRS wait idle after flush
	 * TWS wait idle is not necessary.
	 */
	votf_fmgr = is_votf_get_framemgr(group, TRS, group->leader.id);
	votf_fmgr_call(votf_fmgr, slave, wait_idle);

	/* Free VOTF internal buffer */
	ret = is_votf_free_internal_buffer(group);
	if (ret)
		mgerr("failed to create suvdev link", group, group);

	return 0;
}

int is_votf_create_link(struct is_group *group, u32 width, u32 height)
{
	int ret = 0;
	struct votf_info src_info, dst_info;
	u32 change_option = VOTF_OPTION_MSK_COUNT;

	if (group->prev && group->prev->device_type == IS_DEVICE_SENSOR)
		return 0;

	ret = is_votf_get_votf_info(group, &src_info, &dst_info, (char *)__func__);
	if (ret)
		return ret;

#if defined(USE_VOTF_AXI_APB)
	votfitf_create_link(src_info.ip, dst_info.ip);

	ret = is_votf_subdev_create_link(group);
	if (ret) {
		mgerr("failed to create suvdev link", group, group);
		goto err_subdev_create_link;
	}
#endif

	ret = is_votf_link_set_service_cfg(&src_info, &dst_info,
			width, height, change_option);
	if (ret)
		goto err_link_set_service_cfg;

	/* Allocate VOTF internal buffer */
	ret = is_votf_alloc_internal_buffer(group, width, height);
	if (ret) {
		mgerr("failed to create suvdev link", group, group);
		goto err_alloc_internal_buffer;
	}

	return 0;

err_alloc_internal_buffer:
err_link_set_service_cfg:
#if defined(USE_VOTF_AXI_APB)
	is_votf_subdev_destroy_link(group);

err_subdev_create_link:
	votfitf_destroy_link(src_info.ip, dst_info.ip);
#endif

	return ret;
}

int is_votf_destroy_link(struct is_group *group)
{
	int ret = 0;
	struct votf_info src_info, dst_info;

	if (group->id >= GROUP_ID_MAX) {
		mgerr("group ID is invalid(%d)", group, group);

		/* Do not access C2SERV after power off */
		return 0;
	}

	if (group->prev && group->prev->device_type == IS_DEVICE_SENSOR)
		return 0;

	ret = is_votf_get_votf_info(group, &src_info, &dst_info, (char *)__func__);
	if (ret)
		return ret;

	ret = is_votf_flush(group);
	if (ret)
		mgerr("is_votf_flush is fail(%d)", group, group, ret);

	/*
	 * All VOTF control such as flush must be set in ring clock enable and ring enable.
	 * So, calling votfitf_destroy_ring must be called at the last.
	 */
#if defined(USE_VOTF_AXI_APB)
	votfitf_destroy_link(src_info.ip, dst_info.ip);

	is_votf_subdev_destroy_link(group);
#endif

	/* Free VOTF internal buffer */
	ret = is_votf_free_internal_buffer(group);
	if (ret)
		mgerr("failed to create suvdev link", group, group);

	return 0;
}

struct is_framemgr *is_votf_get_framemgr(struct is_group *group,  enum votf_service type,
	unsigned long subdev_id)
{
	struct is_framemgr *framemgr;
	struct is_subdev *subdev;
	struct is_device_ischain *device;
	struct is_subdev *subdev_pdaf;

	FIMC_BUG_NULL(!group);

	if (type == TWS) {
		if (!group->next) {
			mgerr("The next group is NULL (subdev_id: %ld)\n", group, group, subdev_id);
			return NULL;
		}

		subdev = &group->next->leader;
	} else {
		subdev = &group->leader;
	}

	framemgr = GET_SUBDEV_I_FRAMEMGR(subdev);

	/* FIXME */
	if (subdev_id == ENTRY_SSVC1 || subdev_id == ENTRY_PDAF) {
		device = group->device;
		subdev_pdaf = &device->pdaf;

		framemgr = GET_SUBDEV_I_FRAMEMGR(subdev_pdaf);
	}

	FIMC_BUG_NULL(!framemgr);

	return framemgr;
}

struct is_frame *is_votf_get_frame(struct is_group *group,  enum votf_service type,
	unsigned long subdev_id, u32 fcount)
{
	struct is_framemgr *framemgr;
	struct is_frame *frame;

	framemgr = is_votf_get_framemgr(group, type, subdev_id);
	if (!framemgr) {
		mgerr("framemgr is NULL (subdev_id: %ld)\n", group, group, subdev_id);
		return NULL;
	}

	frame = &framemgr->frames[framemgr->num_frames ? (fcount % framemgr->num_frames) : 0];

	return frame;
}

int is_votf_register_framemgr(struct is_group *group, enum votf_service type,
	void *data, const struct votf_ops *ops, unsigned long subdev_id)
{
	struct is_framemgr *framemgr;

	framemgr = is_votf_get_framemgr(group, type, subdev_id);
	if (!framemgr) {
		mgerr("framemgr is NULL. (subdev_id: %ld)\n", group, group, subdev_id);
		return -EINVAL;
	}

	if (type == TWS) {
		framemgr->master.id = subdev_id;
		framemgr->master.data = data;
		framemgr->master.ops = ops;
		mginfo("Register VOTF master callback (subdev_id: %ld)\n", group, group, subdev_id);
	} else {
		framemgr->slave.id = subdev_id;
		framemgr->slave.data = data;
		framemgr->slave.ops = ops;
		mginfo("Register VOTF slave callback (subdev_id: %ld)\n", group, group, subdev_id);
	}

	return 0;
}
