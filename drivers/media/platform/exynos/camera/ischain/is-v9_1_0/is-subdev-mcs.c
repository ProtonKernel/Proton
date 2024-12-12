// SPDX-License-Identifier: GPL-2.0
/*
 * Samsung Exynos SoC series Pablo driver
 *
 * Copyright (c) 2020 Samsung Electronics Co., Ltd
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#include "is-device-ischain.h"
#include "is-subdev-ctrl.h"
#include "is-config.h"
#include "is-param.h"
#include "is-video.h"
#include "is-type.h"

#ifdef USE_VRA_OTF
static int is_ischain_mcs_bypass(struct is_subdev *leader,
	void *device_data,
	struct is_frame *frame,
	bool bypass)
{
	int ret = 0;
	struct is_group *group;
	struct param_mcs_output *output;
	struct is_device_ischain *device;
	struct is_subdev *subdev = NULL, *subdev_temp = NULL, *temp;
	u32 lindex = 0, hindex = 0, indexes = 0;

	device = (struct is_device_ischain *)device_data;

	FIMC_BUG(!leader);
	FIMC_BUG(!device);

	group = &device->group_mcs;

	list_for_each_entry_safe(subdev_temp, temp, &group->subdev_list, list) {
		if (subdev_temp == &group->leader)
			continue;

		if (test_bit(IS_SUBDEV_START, &subdev_temp->state)) {
			subdev = subdev_temp;
			goto find_subdev;
		}
	}

find_subdev:
	if (!subdev) {
		mgerr("subdev is NULL\n", device, group);
		goto p_err;
	}

	output = is_itf_g_param(device, frame, subdev->param_dma_ot);

	if (bypass)
		output->otf_cmd = OTF_OUTPUT_COMMAND_DISABLE;
	else
		output->otf_cmd = OTF_OUTPUT_COMMAND_ENABLE;

	lindex |= LOWBIT_OF(subdev->param_dma_ot);
	hindex |= HIGHBIT_OF(subdev->param_dma_ot);
	indexes++;

	ret = is_itf_s_param(device, frame, lindex, hindex, indexes);
	if (ret) {
		mrerr("is_itf_s_param is fail(%d)", device, frame, ret);
		goto p_err;
	}

	minfo("VRA is connected at %s\n", device, group->junction->name);

p_err:
	return ret;
}
#endif

void is_ischain_mcs_stripe_cfg(struct is_subdev *subdev,
		struct is_frame *frame,
		struct is_crop *incrop,
		struct is_crop *otcrop,
		u32 *stripe_in_start_pos_x,
		u32 *stripe_roi_start_pos_x,
		u32 *stripe_roi_end_pos_x,
		u32 *stripe_in_end_pos_x)
{
	u32 stripe_w, stripe_margin;
	u32 region_id = frame->stripe_info.region_id;

	if (IS_ENABLED(YPP_DDK_LIB_CALL))
		stripe_margin = STRIPE_MCSC_MARGIN_WIDTH;
	else
		stripe_margin = STRIPE_MARGIN_WIDTH;

	if (!region_id) {
		/* Left region */
		stripe_w = frame->stripe_info.in.h_pix_num[region_id];
		*stripe_in_start_pos_x = 0;
		*stripe_roi_start_pos_x = 0;
		*stripe_roi_end_pos_x = frame->stripe_info.in.h_pix_num[region_id];
		*stripe_in_end_pos_x = *stripe_roi_end_pos_x + stripe_margin;
	} else if (region_id < frame->stripe_info.region_num - 1) {
		/* Middle region */
		stripe_w = frame->stripe_info.in.h_pix_num[region_id] - frame->stripe_info.in.h_pix_num[region_id - 1];
		*stripe_in_start_pos_x = frame->stripe_info.in.h_pix_num[region_id - 1] - stripe_margin;
		*stripe_roi_start_pos_x = frame->stripe_info.in.h_pix_num[region_id - 1];
		*stripe_roi_end_pos_x = frame->stripe_info.in.h_pix_num[region_id];
		*stripe_in_end_pos_x = *stripe_roi_end_pos_x + stripe_margin;
	} else {
		/* Right region */
		stripe_w = frame->stripe_info.in.h_pix_num[region_id] - frame->stripe_info.in.h_pix_num[region_id - 1];
		*stripe_in_start_pos_x = frame->stripe_info.in.h_pix_num[region_id - 1] - stripe_margin;
		*stripe_roi_start_pos_x = frame->stripe_info.in.h_pix_num[region_id - 1];
		*stripe_roi_end_pos_x = incrop->w;
		*stripe_in_end_pos_x = *stripe_roi_end_pos_x;
	}
	stripe_w += stripe_margin;
	incrop->w = stripe_w;

	/**
	 * Output crop configuration.
	 * No crop & scale.
	 */
	otcrop->x = 0;
	otcrop->y = 0;
	otcrop->w = incrop->w;
	otcrop->h = incrop->h;

	msrdbgs(3, "stripe_in_crop[%d][%d, %d, %d, %d]\n", subdev, subdev, frame,
			frame->stripe_info.region_id,
			incrop->x, incrop->y, incrop->w, incrop->h);
	msrdbgs(3, "stripe_ot_crop[%d][%d, %d, %d, %d]\n", subdev, subdev, frame,
			frame->stripe_info.region_id,
			otcrop->x, otcrop->y, otcrop->w, otcrop->h);
}

#ifdef ENABLE_LOGICAL_VIDEO_NODE
void __mcs_in_stripe_cfg(struct is_frame *frame,
		struct camera2_node *node,
		struct is_crop *incrop,
		struct is_crop *otcrop,
		u32 *stripe_in_start_pos_x,
		u32 *stripe_roi_start_pos_x,
		u32 *stripe_roi_end_pos_x,
		u32 *stripe_in_end_pos_x)
{
	u32 stripe_w, stripe_margin;
	u32 region_id = frame->stripe_info.region_id;

	if (IS_ENABLED(YPP_DDK_LIB_CALL))
		stripe_margin = STRIPE_MCSC_MARGIN_WIDTH;
	else
		stripe_margin = STRIPE_MARGIN_WIDTH;

	if (!region_id) {
		/* Left region */
		stripe_w = frame->stripe_info.out.h_pix_num[region_id];
		*stripe_in_start_pos_x = 0;
		*stripe_roi_start_pos_x = 0;
		*stripe_roi_end_pos_x = frame->stripe_info.out.h_pix_num[region_id];
		*stripe_in_end_pos_x = *stripe_roi_end_pos_x + stripe_margin;
	} else if (region_id < frame->stripe_info.region_num - 1) {
		/* Middle region */
		stripe_w = frame->stripe_info.out.h_pix_num[region_id] - frame->stripe_info.out.h_pix_num[region_id - 1];
		*stripe_in_start_pos_x = frame->stripe_info.out.h_pix_num[region_id - 1] - stripe_margin;
		*stripe_roi_start_pos_x = frame->stripe_info.out.h_pix_num[region_id - 1];
		*stripe_roi_end_pos_x = frame->stripe_info.out.h_pix_num[region_id];
		*stripe_in_end_pos_x = *stripe_roi_end_pos_x + stripe_margin;
		stripe_w += stripe_margin;
	} else {
		/* Right region */
		stripe_w = incrop->w - frame->stripe_info.out.h_pix_num[region_id - 1];
		*stripe_in_start_pos_x = frame->stripe_info.out.h_pix_num[region_id - 1] - stripe_margin;
		*stripe_roi_start_pos_x = frame->stripe_info.out.h_pix_num[region_id - 1];
		*stripe_roi_end_pos_x = incrop->w;
		*stripe_in_end_pos_x = *stripe_roi_end_pos_x;
	}
	stripe_w += stripe_margin;
	incrop->w = stripe_w;

	/**
	 * Output crop configuration.
	 * No crop & scale.
	 */
	otcrop->x = 0;
	otcrop->y = 0;
	otcrop->w = incrop->w;
	otcrop->h = incrop->h;

	mlvdbgs(3, "[MCS][F%d] stripe_in_crop[%d][%d, %d, %d, %d]\n", frame, node->vid,
			frame->fcount, frame->stripe_info.region_id,
			incrop->x, incrop->y, incrop->w, incrop->h);
	mlvdbgs(3, "[MCS][F%d] stripe_ot_crop[%d][%d, %d, %d, %d]\n", frame, node->vid,
			frame->fcount, frame->stripe_info.region_id,
			otcrop->x, otcrop->y, otcrop->w, otcrop->h);
}

static int __ischain_mcsc_slot(struct camera2_node *node, u32 *pindex)
{
	int ret = 0;

	switch (node->vid) {
	case IS_VIDEO_M0P_NUM:
		*pindex = PARAM_MCS_OUTPUT0;
		ret = 2;
		break;
	case IS_VIDEO_M1P_NUM:
		*pindex = PARAM_MCS_OUTPUT1;
		ret = 2;
		break;
	case IS_VIDEO_M2P_NUM:
		*pindex = PARAM_MCS_OUTPUT2;
		ret = 2;
		break;
	case IS_VIDEO_M3P_NUM:
		*pindex = PARAM_MCS_OUTPUT3;
		ret = 2;
		break;
	case IS_VIDEO_M4P_NUM:
		*pindex = PARAM_MCS_OUTPUT4;
		ret = 2;
		break;
	case IS_VIDEO_M5P_NUM:
		*pindex = PARAM_MCS_OUTPUT5;
		ret = 2;
		break;
	default:
		ret = 0;
		break;
	}

	return ret;
}

int __mcsc_in_cfg(struct is_device_ischain *device,
	struct is_subdev *leader,
	struct is_frame *ldr_frame,
	struct camera2_node *node,
	u32 pindex,
	u32 *lindex,
	u32 *hindex,
	u32 *indexes)
{
	struct is_group *group;
	struct param_control *control;
	struct param_mcs_input *mscs_input;
	struct is_crop *incrop, *otcrop;
	struct is_crop incrop_cfg, otcrop_cfg;
	struct is_fmt *fmt;
	u32 stripe_in_start_pos_x = 0, stripe_in_end_pos_x = 0;
	u32 stripe_roi_start_pos_x = 0, stripe_roi_end_pos_x = 0;

	FIMC_BUG(!device);
	FIMC_BUG(!ldr_frame);
	FIMC_BUG(!node);
	FIMC_BUG(!lindex);
	FIMC_BUG(!hindex);
	FIMC_BUG(!indexes);

	group = &device->group_mcs;
	incrop = (struct is_crop *)node->input.cropRegion;
	if (IS_NULL_CROP(incrop)) {
		mlverr("[MSC] incrop is NULL", device, node->vid);
		return -EINVAL;
	}

	otcrop = (struct is_crop *)node->output.cropRegion;
	if (IS_NULL_CROP(otcrop)) {
		merr("[MSC] otcrop is NULL", device, node->vid);
		return -EINVAL;
	}
	incrop_cfg = *incrop;
	otcrop_cfg = *otcrop;

	fmt = is_find_format(node->pixelformat, node->flags);

	if (IS_ENABLED(CHAIN_USE_STRIPE_PROCESSING) && ldr_frame->stripe_info.region_num)
		__mcs_in_stripe_cfg(ldr_frame, node, &incrop_cfg, &otcrop_cfg,
			&stripe_in_start_pos_x, &stripe_roi_start_pos_x, &stripe_roi_end_pos_x, &stripe_in_end_pos_x);

	mscs_input = is_itf_g_param(device, ldr_frame, pindex);
	if (test_bit(IS_GROUP_OTF_INPUT, &group->state)) {
		if (mscs_input->otf_cmd  != OTF_INPUT_COMMAND_ENABLE)
		mlvinfo("[MSC][F%d] OTF enable: %d -> %d\n", device,
			node->vid, ldr_frame->fcount,
			mscs_input->otf_cmd, OTF_INPUT_COMMAND_ENABLE);
		mscs_input->otf_cmd = OTF_INPUT_COMMAND_ENABLE;
		mscs_input->dma_cmd = DMA_INPUT_COMMAND_DISABLE;
		if (!ldr_frame->stripe_info.region_num &&
			((mscs_input->width != otcrop_cfg.w) || (mscs_input->height != otcrop_cfg.h)))
			mlvinfo("[MSC][F%d] OTF incrop[%d, %d, %d, %d]\n", device,
				node->vid, ldr_frame->fcount,
				otcrop_cfg.x, otcrop_cfg.y, otcrop_cfg.w, otcrop_cfg.h);
		mscs_input->width = otcrop_cfg.w;
		mscs_input->height = otcrop_cfg.h;
	} else {
		mscs_input->otf_cmd = OTF_INPUT_COMMAND_DISABLE;
		mscs_input->dma_cmd = DMA_INPUT_COMMAND_ENABLE;
		mscs_input->width = leader->input.width;
		mscs_input->height = leader->input.height;
		mscs_input->dma_crop_offset_x = leader->input.canv.x
						+ incrop_cfg.x;
		mscs_input->dma_crop_offset_y = leader->input.canv.y
						+ incrop_cfg.y;
		mscs_input->dma_crop_width = incrop_cfg.w;
		mscs_input->dma_crop_height = incrop_cfg.h;

		mscs_input->dma_format = fmt->hw_format;
		mscs_input->dma_bitwidth = fmt->hw_bitwidth;
		mscs_input->dma_order = fmt->hw_order;
		mscs_input->plane = fmt->hw_plane;

		mscs_input->dma_stride_y = max(mscs_input->dma_crop_width * fmt->bitsperpixel[0] / BITS_PER_BYTE,
					node->bytesperline[0]);
		mscs_input->dma_stride_c = max(mscs_input->dma_crop_width * fmt->bitsperpixel[1] / BITS_PER_BYTE,
					node->bytesperline[1]);

	}

	mscs_input->otf_format = OTF_INPUT_FORMAT_YUV422;
	mscs_input->otf_bitwidth = OTF_INPUT_BIT_WIDTH_8BIT;
	mscs_input->otf_order = OTF_INPUT_ORDER_BAYER_GR_BG;
	mscs_input->stripe_total_count = (ldr_frame) ? ldr_frame->stripe_info.region_num : 0;
	mscs_input->stripe_region_index = (ldr_frame) ? ldr_frame->stripe_info.region_id : 0;
	mscs_input->stripe_in_start_pos_x = stripe_in_start_pos_x;
	mscs_input->stripe_roi_start_pos_x = stripe_roi_start_pos_x;
	mscs_input->stripe_roi_end_pos_x = stripe_roi_end_pos_x;
	mscs_input->stripe_in_end_pos_x = stripe_in_end_pos_x;
	mscs_input->full_input_width = incrop->w;
	mscs_input->full_output_width = incrop->w;

	*lindex |= LOWBIT_OF(pindex);
	*hindex |= HIGHBIT_OF(pindex);
	(*indexes)++;

	/* Configure Control */
	control = is_itf_g_param(device, ldr_frame, PARAM_MCS_CONTROL);
	control->cmd = CONTROL_COMMAND_START;
	*lindex |= LOWBIT_OF(PARAM_MCS_CONTROL);
	*hindex |= HIGHBIT_OF(PARAM_MCS_CONTROL);
	(*indexes)++;

	return 0;
}
#endif

static int is_ischain_mcs_cfg(struct is_subdev *leader,
	void *device_data,
	struct is_frame *frame,
	struct is_crop *incrop,
	struct is_crop *otcrop,
	u32 *lindex,
	u32 *hindex,
	u32 *indexes)
{
	int ret = 0;
	struct is_fmt *format;
	struct is_group *group;
	struct is_queue *queue;
	struct param_mcs_input *input;
	struct param_control *control;
	struct is_device_ischain *device;
	struct is_crop incrop_cfg, otcrop_cfg;
	u32 flag_extra;
	u32 hw_sbwc = 0;
	u32 stripe_in_start_pos_x = 0, stripe_in_end_pos_x = 0;
	u32 stripe_roi_start_pos_x = 0, stripe_roi_end_pos_x = 0;
	bool chg_format = false;
#ifdef USE_VRA_OTF
	struct is_subdev *subdev = NULL, *subdev_temp = NULL, *temp;
#endif

	device = (struct is_device_ischain *)device_data;

	FIMC_BUG(!leader);
	FIMC_BUG(!device);
	FIMC_BUG(!device->sensor);
	FIMC_BUG(!incrop);
	FIMC_BUG(!lindex);
	FIMC_BUG(!hindex);
	FIMC_BUG(!indexes);

	group = &device->group_mcs;
	incrop_cfg = *incrop;
	otcrop_cfg = *otcrop;

	queue = GET_SUBDEV_QUEUE(leader);
	if (!queue) {
		merr("queue is NULL", device);
		ret = -EINVAL;
		goto p_err;
	}

	if (IS_ENABLED(CHAIN_USE_STRIPE_PROCESSING) && frame && frame->stripe_info.region_num)
		is_ischain_mcs_stripe_cfg(leader, frame,
				&incrop_cfg, &otcrop_cfg,
				&stripe_in_start_pos_x, &stripe_roi_start_pos_x, &stripe_roi_end_pos_x, &stripe_in_end_pos_x);

	input = is_itf_g_param(device, frame, PARAM_MCS_INPUT);
	if (test_bit(IS_GROUP_OTF_INPUT, &group->state)) {
		input->otf_cmd = OTF_INPUT_COMMAND_ENABLE;
		input->dma_cmd = DMA_INPUT_COMMAND_DISABLE;
		input->width = incrop_cfg.w;
		input->height = incrop_cfg.h;
	} else {
		flag_extra = (queue->framecfg.hw_pixeltype
				& PIXEL_TYPE_EXTRA_MASK)
				>> PIXEL_TYPE_EXTRA_SHIFT;

		if (flag_extra) {
			hw_sbwc = (SBWC_BASE_ALIGN_MASK_LLC_ON | flag_extra);
			chg_format = true;
		}
		format = queue->framecfg.format;
		if (chg_format)
			msdbgs(3, "in_crop[bitwidth %d sbwc 0x%x]\n",
					device, leader,
					format->hw_bitwidth,
					hw_sbwc);

		input->otf_cmd = OTF_INPUT_COMMAND_DISABLE;
		input->dma_cmd = DMA_INPUT_COMMAND_ENABLE;
		input->width = leader->input.canv.w;
		input->height = leader->input.canv.h;
		input->dma_crop_offset_x = leader->input.canv.x + incrop_cfg.x;
		input->dma_crop_offset_y = leader->input.canv.y + incrop_cfg.y;
		input->dma_crop_width = incrop_cfg.w;
		input->dma_crop_height = incrop_cfg.h;

		input->dma_format = format->hw_format;
		input->dma_bitwidth = format->hw_bitwidth;
		input->dma_order = format->hw_order;
		input->plane = format->hw_plane;
		input->sbwc_type = hw_sbwc;

		input->dma_stride_y = max(input->dma_crop_width * format->bitsperpixel[0] / BITS_PER_BYTE,
					queue->framecfg.bytesperline[0]);
		input->dma_stride_c = max(input->dma_crop_width * format->bitsperpixel[1] / BITS_PER_BYTE,
					queue->framecfg.bytesperline[1]);

	}

	input->otf_format = OTF_INPUT_FORMAT_YUV422;
	input->otf_bitwidth = OTF_INPUT_BIT_WIDTH_8BIT;
	input->otf_order = OTF_INPUT_ORDER_BAYER_GR_BG;
	input->stripe_total_count = (frame) ? frame->stripe_info.region_num : 0;
	input->stripe_region_index = (frame) ? frame->stripe_info.region_id : 0;
	input->stripe_in_start_pos_x = stripe_in_start_pos_x;
	input->stripe_roi_start_pos_x = stripe_roi_start_pos_x;
	input->stripe_roi_end_pos_x = stripe_roi_end_pos_x;
	input->stripe_in_end_pos_x = stripe_in_end_pos_x;
	input->full_input_width = incrop->w;
	input->full_output_width = incrop->w;

	*lindex |= LOWBIT_OF(PARAM_MCS_INPUT);
	*hindex |= HIGHBIT_OF(PARAM_MCS_INPUT);
	(*indexes)++;

	/* Configure Control */
	control = is_itf_g_param(device, frame, PARAM_MCS_CONTROL);
	control->cmd = CONTROL_COMMAND_START;
#ifdef ENABLE_DNR_IN_MCSC
	control->buffer_address = device->minfo->dvaddr_mcsc_dnr;
#endif
	*lindex |= LOWBIT_OF(PARAM_MCS_CONTROL);
	*hindex |= HIGHBIT_OF(PARAM_MCS_CONTROL);
	(*indexes)++;

	leader->input.crop = *incrop;

	if (test_bit(IS_ISCHAIN_REPROCESSING, &device->state)) {
		msinfo("[%s] reprocessing cannot connect to VRA\n", device, leader, __func__);
		goto p_err;
	}

#ifdef USE_VRA_OTF
	list_for_each_entry_safe(subdev_temp, temp, &group->subdev_list, list) {
		if (subdev_temp == &group->leader)
			continue;

		if (test_bit(IS_SUBDEV_START, &subdev_temp->state)) {
			subdev = subdev_temp;
			goto find_subdev;
		}
	}

find_subdev:
	if (!subdev) {
		mgerr("subdev is NULL\n", device, group);
		goto p_err;
	}

	CALL_SOPS(subdev, cfg, device, frame, incrop, NULL, lindex, hindex, indexes);
#endif

p_err:
	return ret;
}

static int is_ischain_mcs_tag(struct is_subdev *subdev,
	void *device_data,
	struct is_frame *frame,
	struct camera2_node *node)
{
	int ret = 0;
	struct is_group *group;
	struct mcs_param *mcs_param;
	struct is_crop inparm;
	struct is_crop *incrop, *otcrop;
	struct is_subdev *leader;
	struct is_device_ischain *device;
	u32 lindex, hindex, indexes;
#ifdef ENABLE_LOGICAL_VIDEO_NODE
	struct is_queue *queue;
	struct camera2_node *out_node = NULL;
	struct camera2_node *cap_node = NULL;
	struct is_sub_frame *sframe;
	u32 *targetAddr;
	u32 pindex = 0, num_planes;
	int j, i, p, dma_type;
#endif
	device = (struct is_device_ischain *)device_data;

	FIMC_BUG(!subdev);
	FIMC_BUG(!device);
	FIMC_BUG(!device->is_region);
	FIMC_BUG(!frame);

	mdbgs_ischain(4, "MCSC TAG\n", device);

	incrop = (struct is_crop *)node->input.cropRegion;
	otcrop = (struct is_crop *)node->output.cropRegion;

	group = &device->group_mcs;
	leader = subdev->leader;
	lindex = hindex = indexes = 0;
	mcs_param = &device->is_region->parameter.mcs;

#ifdef ENABLE_LOGICAL_VIDEO_NODE
	queue = GET_SUBDEV_QUEUE(leader);
	if (!queue) {
		merr("queue is NULL", device);
		ret = -EINVAL;
		goto p_err;
	}

	if (queue->mode == CAMERA_NODE_NORMAL)
		goto p_skip;

	out_node = &frame->shot_ext->node_group.leader;
	/* The input of MCSC is always OTF */
	ret = __mcsc_in_cfg(device, subdev, frame, out_node,
		PARAM_MCS_INPUT, &lindex, &hindex, &indexes);

	for (i = 0; i < CAPTURE_NODE_MAX; i++) {
		cap_node = &frame->shot_ext->node_group.capture[i];
		if (!cap_node->vid)
			continue;

		dma_type = __ischain_mcsc_slot(cap_node, &pindex);
		if (dma_type == 1)
			ret = __mcsc_in_cfg(device, subdev,
					frame, cap_node,
					pindex,
					&lindex,
					&hindex,
					&indexes);
		else if (dma_type == 2)
			ret = __mcsc_dma_out_cfg(device, frame,
					cap_node,
					pindex,
					&lindex,
					&hindex,
					&indexes,
					i);
	}

	for (i = 0; i < CAPTURE_NODE_MAX; i++) {
		sframe = &frame->cap_node.sframe[i];
		if (!sframe->id)
			continue;

		if ((sframe->id < IS_VIDEO_M0P_NUM) ||
			(sframe->id > IS_VIDEO_M5P_NUM))
			continue;

		targetAddr = NULL;
		ret = is_hw_get_capture_slot(frame, &targetAddr, NULL, sframe->id);
		if (ret) {
			mrerr("Invalid capture slot(%d)", device, frame,
				sframe->id);
			return -EINVAL;
		}

		if (!targetAddr)
			continue;

		num_planes = sframe->num_planes / frame->num_shots;
		for (j = 0, p = num_planes * frame->cur_shot_idx;
			j < num_planes; j++, p++)
			targetAddr[j] = sframe->dva[p];
	}

	ret = is_itf_s_param(device, frame, lindex, hindex, indexes);
	if (ret) {
		mrerr("is_itf_s_param is fail(%d)", device, frame, ret);
		goto p_err;
	}

	return ret;
p_skip:
#endif
	if (test_bit(IS_GROUP_OTF_INPUT, &group->state)) {
		inparm.x = 0;
		inparm.y = 0;
		inparm.w = mcs_param->input.width;
		inparm.h = mcs_param->input.height;
	} else {
		inparm.x = mcs_param->input.dma_crop_offset_x;
		inparm.y = mcs_param->input.dma_crop_offset_y;
		inparm.w = mcs_param->input.dma_crop_width;
		inparm.h = mcs_param->input.dma_crop_height;
	}

	if (IS_NULL_CROP(incrop))
		*incrop = inparm;

	if (!COMPARE_CROP(incrop, &inparm) ||
		CHECK_STRIPE_CFG(&frame->stripe_info) ||
		test_bit(IS_SUBDEV_FORCE_SET, &leader->state)) {
		ret = is_ischain_mcs_cfg(subdev,
			device,
			frame,
			incrop,
			otcrop,
			&lindex,
			&hindex,
			&indexes);
		if (ret) {
			merr("is_ischain_mcs_start is fail(%d)", device, ret);
			goto p_err;
		}

		if (!COMPARE_CROP(incrop, &subdev->input.crop) ||
			debug_stream) {
			msrinfo("in_crop[%d, %d, %d, %d]\n", device, subdev, frame,
				incrop->x, incrop->y, incrop->w, incrop->h);
			subdev->input.crop = *incrop;
		}
	}

	ret = is_itf_s_param(device, frame, lindex, hindex, indexes);
	if (ret) {
		mrerr("is_itf_s_param is fail(%d)", device, frame, ret);
		goto p_err;
	}

p_err:
	return ret;
}

const struct is_subdev_ops is_subdev_mcs_ops = {
#ifdef USE_VRA_OTF
	.bypass			= is_ischain_mcs_bypass,
#else
	.bypass			= NULL,
#endif
	.cfg			= is_ischain_mcs_cfg,
	.tag			= is_ischain_mcs_tag,
};
