/*
 * Samsung Exynos5 SoC series FIMC-IS driver
 *
 * exynos5 fimc-is video functions
 *
 * Copyright (c) 2018 Samsung Electronics Co., Ltd
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#include "is-device-ischain.h"
#include "is-device-sensor.h"
#include "is-subdev-ctrl.h"
#include "is-config.h"
#include "is-param.h"
#include "is-video.h"
#include "is-type.h"

void is_ischain_paf_stripe_cfg(struct is_subdev *subdev,
		struct is_frame *frame,
		struct is_crop *incrop,
		struct is_crop *otcrop,
		struct is_fmt *fmt)
{
	u32 stripe_w, dma_offset = 0;

	/* Input crop & RDMA offset configuration */
	if (!frame->stripe_info.region_id) {
		/* Left region */
		stripe_w = ALIGN(incrop->w / frame->stripe_info.region_num, STRIPE_WIDTH_ALIGN);

		frame->stripe_info.in.h_pix_ratio = stripe_w * STRIPE_RATIO_PRECISION / incrop->w;
		frame->stripe_info.in.h_pix_num = stripe_w;
	} else {
		/* Right region */
		stripe_w = incrop->w - frame->stripe_info.in.h_pix_num;

		/* Add horizontal DMA offset */
		dma_offset = frame->stripe_info.in.h_pix_num - STRIPE_MARGIN_WIDTH;
		dma_offset = dma_offset * fmt->bitsperpixel[0] / BITS_PER_BYTE;
	}

	/* Add stripe processing horizontal margin into each region. */
	stripe_w += STRIPE_MARGIN_WIDTH;

	incrop->w = stripe_w;

	/**
	 *	 * Output crop configuration.
	 *		 * No crop & scale.
	 *			 */
	otcrop->x = 0;
	otcrop->y = 0;
	otcrop->w = incrop->w;
	otcrop->h = incrop->h;

	frame->dvaddr_buffer[0] += dma_offset;

	msrdbgs(3, "stripe_in_crop[%d][%d, %d, %d, %d] offset %x\n", subdev, subdev, frame,
			frame->stripe_info.region_id,
			incrop->x, incrop->y, incrop->w, incrop->h, dma_offset);
	msrdbgs(3, "stripe_ot_crop[%d][%d, %d, %d, %d]\n", subdev, subdev, frame,
			frame->stripe_info.region_id,
			otcrop->x, otcrop->y, otcrop->w, otcrop->h);
}

static int is_ischain_paf_cfg(struct is_subdev *leader,
	void *device_data,
	struct is_frame *frame,
	struct is_crop *incrop,
	struct is_crop *otcrop,
	u32 *lindex,
	u32 *hindex,
	u32 *indexes)
{
	int ret = 0;
	struct is_group *group;
	struct is_queue *queue;
	struct param_dma_input *dma_input;
	struct param_control *control;
	struct is_device_ischain *device;
	struct is_device_sensor *sensor;
	u32 hw_format = DMA_INOUT_FORMAT_BAYER;
	u32 hw_bitwidth = DMA_INOUT_BIT_WIDTH_16BIT;
	struct is_crop incrop_cfg, otcrop_cfg;

	device = (struct is_device_ischain *)device_data;

	FIMC_BUG(!leader);
	FIMC_BUG(!device);
	FIMC_BUG(!device->sensor);
	FIMC_BUG(!incrop);
	FIMC_BUG(!lindex);
	FIMC_BUG(!hindex);
	FIMC_BUG(!indexes);

	sensor = device->sensor;
	group = &device->group_paf;
	incrop_cfg = *incrop;
	otcrop_cfg = *otcrop;

	group = &device->group_paf;

	queue = GET_SUBDEV_QUEUE(leader);
	if (!queue) {
		merr("queue is NULL", device);
		ret = -EINVAL;
		goto p_err;
	}

	if (!test_bit(IS_GROUP_OTF_INPUT, &group->state)) {
		if (!queue->framecfg.format) {
			merr("format is NULL", device);
			ret = -EINVAL;
			goto p_err;
		}

		hw_format = queue->framecfg.format->hw_format;
		hw_bitwidth = queue->framecfg.format->hw_bitwidth; /* memory width per pixel */
	}

	/* Configure Conrtol */
	if (!frame) {
		control = is_itf_g_param(device, NULL, PARAM_PAF_CONTROL);
		if (test_bit(IS_GROUP_START, &group->state)) {
			control->cmd = CONTROL_COMMAND_START;
			control->bypass = CONTROL_BYPASS_DISABLE;
		} else {
			control->cmd = CONTROL_COMMAND_STOP;
			control->bypass = CONTROL_BYPASS_DISABLE;
		}
		*lindex |= LOWBIT_OF(PARAM_PAF_CONTROL);
		*hindex |= HIGHBIT_OF(PARAM_PAF_CONTROL);
		(*indexes)++;
	}

	if (IS_ENABLED(CHAIN_USE_STRIPE_PROCESSING) && frame && frame->stripe_info.region_num)
		is_ischain_paf_stripe_cfg(leader, frame,
				&incrop_cfg, &otcrop_cfg,
				queue->framecfg.format);

	dma_input = is_itf_g_param(device, frame, PARAM_PAF_DMA_INPUT);
	if (test_bit(IS_GROUP_OTF_INPUT, &group->state)) {
		if (test_bit(IS_GROUP_VOTF_INPUT, &group->state)) {
			dma_input->cmd = DMA_INPUT_COMMAND_ENABLE;
			dma_input->v_otf_enable = OTF_INPUT_COMMAND_ENABLE;
		} else {
			dma_input->cmd = DMA_INPUT_COMMAND_DISABLE;
			dma_input->v_otf_enable = OTF_INPUT_COMMAND_DISABLE;
		}
	} else {
		dma_input->cmd = DMA_INPUT_COMMAND_ENABLE;
		dma_input->v_otf_enable = OTF_INPUT_COMMAND_DISABLE;
	}
	dma_input->format = hw_format;
	dma_input->bitwidth = hw_bitwidth;
	dma_input->msb = sensor->image.format.hw_bitwidth - 1;/* msb zero padding by HW constraint */
	dma_input->order = DMA_INOUT_ORDER_GR_BG;
	dma_input->plane = 1;
	dma_input->width = leader->input.width;
	dma_input->height = leader->input.height;

	dma_input->dma_crop_offset = 0;
	dma_input->dma_crop_width = incrop_cfg.w;
	dma_input->dma_crop_height = incrop_cfg.h;
	dma_input->bayer_crop_offset_x = incrop_cfg.x;
	dma_input->bayer_crop_offset_y = incrop_cfg.y;
	dma_input->bayer_crop_width = incrop_cfg.w;
	dma_input->bayer_crop_height = incrop_cfg.h;
	*lindex |= LOWBIT_OF(PARAM_PAF_DMA_INPUT);
	*hindex |= HIGHBIT_OF(PARAM_PAF_DMA_INPUT);
	(*indexes)++;

	leader->input.crop = *incrop;

p_err:
	return ret;
}

static int is_ischain_paf_tag(struct is_subdev *subdev,
	void *device_data,
	struct is_frame *frame,
	struct camera2_node *node)
{
	int ret = 0;
	struct is_group *group;
	struct paf_rdma_param *paf_param;
	struct is_crop inparm;
	struct is_crop *incrop, *otcrop;
	struct is_subdev *leader;
	struct is_device_ischain *device;
	u32 lindex, hindex, indexes;

	device = (struct is_device_ischain *)device_data;

	FIMC_BUG(!subdev);
	FIMC_BUG(!device);
	FIMC_BUG(!device->is_region);
	FIMC_BUG(!frame);

	mdbgs_ischain(4, "PAF TAG\n", device);

	incrop = (struct is_crop *)node->input.cropRegion;
	otcrop = (struct is_crop *)node->output.cropRegion;

	group = &device->group_paf;
	leader = subdev->leader;
	lindex = hindex = indexes = 0;
	paf_param = &device->is_region->parameter.paf;

	inparm.x = 0;
	inparm.y = 0;
	inparm.w = paf_param->dma_input.dma_crop_width;
	inparm.h = paf_param->dma_input.dma_crop_height;

	if (IS_NULL_CROP(incrop))
		*incrop = inparm;

	/* not supported DMA input crop */
	if ((incrop->x != 0) || (incrop->y != 0))
		*incrop = inparm;

	if (test_bit(IS_GROUP_OTF_INPUT, &group->state)) {
		inparm.x = 0;
		inparm.y = 0;
		inparm.w = paf_param->dma_input.width;
		inparm.h = paf_param->dma_input.height;
	} else {
		inparm.x = incrop->x;
		inparm.y = incrop->y;
		inparm.w = incrop->w;
		inparm.h = incrop->h;
	}

	if (!COMPARE_CROP(incrop, &inparm) ||
		CHECK_STRIPE_CFG(&frame->stripe_info) ||
		test_bit(IS_SUBDEV_FORCE_SET, &leader->state)) {
		ret = is_ischain_paf_cfg(subdev,
			device,
			frame,
			incrop,
			otcrop,
			&lindex,
			&hindex,
			&indexes);
		if (ret) {
			merr("is_ischain_paf_cfg is fail(%d)", device, ret);
			goto p_err;
		}

		msrinfo("in_crop[%d, %d, %d, %d]\n", device, subdev, frame,
			incrop->x, incrop->y, incrop->w, incrop->h);
		msrinfo("ot_crop[%d, %d, %d, %d]\n", device, subdev, frame,
			otcrop->x, otcrop->y, otcrop->w, otcrop->h);
	}

	ret = is_itf_s_param(device, frame, lindex, hindex, indexes);
	if (ret) {
		mrerr("is_itf_s_param is fail(%d)", device, frame, ret);
		goto p_err;
	}

p_err:
	return ret;
}

const struct is_subdev_ops is_subdev_paf_ops = {
	.bypass			= NULL,
	.cfg			= is_ischain_paf_cfg,
	.tag			= is_ischain_paf_tag,
};
