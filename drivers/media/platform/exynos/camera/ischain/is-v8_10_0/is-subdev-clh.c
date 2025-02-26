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

static int is_ischain_clh_cfg(struct is_subdev *leader,
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
	struct is_fmt *format;
	u32 hw_format = DMA_INOUT_FORMAT_YUV422;
	u32 hw_bitwidth = DMA_INOUT_BIT_WIDTH_8BIT;
	u32 hw_order = DMA_INOUT_ORDER_YCbYCr;
	u32 hw_plane = 2;
	u32 width, height;

	device = (struct is_device_ischain *)device_data;

	FIMC_BUG(!leader);
	FIMC_BUG(!device);
	FIMC_BUG(!incrop);
	FIMC_BUG(!lindex);
	FIMC_BUG(!hindex);
	FIMC_BUG(!indexes);

	width = incrop->w;
	height = incrop->h;
	group = &device->group_clh;
	queue = GET_SUBDEV_QUEUE(leader);
	if (!queue) {
		merr("queue is NULL", device);
		ret = -EINVAL;
		goto p_err;
	}

	if (!queue->framecfg.format) {
		merr("format is NULL", device);
		ret = -EINVAL;
		goto p_err;
	}

	format = queue->framecfg.format;
	if (format) {
		hw_format = format->hw_format;
		hw_bitwidth = format->hw_bitwidth; /* memory width per pixel */
		hw_order = format->hw_order;
		hw_plane = format->hw_plane;
	}

	/* Configure Control */
	if (!frame) {
		control = is_itf_g_param(device, NULL, PARAM_CLH_CONTROL);
		control->cmd = CONTROL_COMMAND_START;
		control->bypass = CONTROL_BYPASS_DISABLE;
		*lindex |= LOWBIT_OF(PARAM_CLH_CONTROL);
		*hindex |= HIGHBIT_OF(PARAM_CLH_CONTROL);
		(*indexes)++;
	}

	/* CLAHE */
	dma_input = is_itf_g_param(device, frame, PARAM_CLH_DMA_INPUT);
	dma_input->cmd = DMA_INPUT_COMMAND_ENABLE;
	dma_input->format = hw_format;
	dma_input->bitwidth = hw_bitwidth;
	dma_input->order = hw_order;
	dma_input->msb = hw_bitwidth - 1;
	dma_input->plane = hw_plane;
	dma_input->width = width;
	dma_input->height = height;
	dma_input->dma_crop_offset = (incrop->x << 16) | (incrop->y << 0);
	dma_input->dma_crop_width = width;
	dma_input->dma_crop_height = height;
	dma_input->bayer_crop_offset_x = 0;
	dma_input->bayer_crop_offset_y = 0;
	dma_input->bayer_crop_width = width;
	dma_input->bayer_crop_height = height;
	*lindex |= LOWBIT_OF(PARAM_CLH_DMA_INPUT);
	*hindex |= HIGHBIT_OF(PARAM_CLH_DMA_INPUT);
	(*indexes)++;

	leader->input.crop = *incrop;

p_err:
	return ret;
}

static int is_ischain_clh_tag(struct is_subdev *subdev,
	void *device_data,
	struct is_frame *frame,
	struct camera2_node *node)
{
	int ret = 0;
	struct is_group *group;
	struct clh_param *clh_param;
	struct is_crop inparm;
	struct is_crop *incrop, *otcrop;
	struct is_subdev *leader;
	struct is_device_ischain *device;
	struct is_queue *queue;
	u32 lindex, hindex, indexes;
	u32 pixelformat = 0;

	device = (struct is_device_ischain *)device_data;

	FIMC_BUG(!subdev);
	FIMC_BUG(!device);
	FIMC_BUG(!device->is_region);
	FIMC_BUG(!frame);

	mdbgs_ischain(4, "CLH TAG\n", device);

	incrop = (struct is_crop *)node->input.cropRegion;
	otcrop = (struct is_crop *)node->output.cropRegion;

	group = &device->group_clh;
	leader = subdev->leader;
	lindex = hindex = indexes = 0;
	clh_param = &device->is_region->parameter.clh;

	queue = GET_SUBDEV_QUEUE(subdev);
	if (!queue) {
		merr("queue is NULL", device);
		ret = -EINVAL;
		goto p_err;
	}

	if (!queue->framecfg.format) {
		merr("format is NULL", device);
		ret = -EINVAL;
		goto p_err;
	}

	pixelformat = queue->framecfg.format->pixelformat;

	/* Only DMA input case */
	inparm.x = 0;
	inparm.y = 0;
	inparm.w = clh_param->dma_input.width;
	inparm.h = clh_param->dma_input.height;

	if (IS_NULL_CROP(incrop))
		*incrop = inparm;

	if (!COMPARE_CROP(incrop, &inparm) ||
		test_bit(IS_SUBDEV_FORCE_SET, &leader->state)) {
		ret = is_ischain_clh_cfg(subdev,
			device,
			frame,
			incrop,
			otcrop,
			&lindex,
			&hindex,
			&indexes);
		if (ret) {
			merr("is_ischain_clh_cfg is fail(%d)", device, ret);
			goto p_err;
		}

		msrinfo("in_crop[%d, %d, %d, %d]\n", device, subdev, frame,
				incrop->x, incrop->y, incrop->w, incrop->h);
	}
	ret = is_ischain_buf_tag_input(device,
		subdev,
		frame,
		pixelformat,
		incrop->w,
		incrop->h,
		frame->clxsTargetAddress);
	if (ret) {
		mswarn("%d frame is drop", device, subdev, frame->fcount);
	}

	ret = is_itf_s_param(device, frame, lindex, hindex, indexes);
	if (ret) {
		mrerr("is_itf_s_param is fail(%d)", device, frame, ret);
		goto p_err;
	}

p_err:
	return ret;
}

const struct is_subdev_ops is_subdev_clh_ops = {
	.bypass			= NULL,
	.cfg			= is_ischain_clh_cfg,
	.tag			= is_ischain_clh_tag,
};
