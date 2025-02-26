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
#include "is-device-sensor.h"
#include "is-subdev-ctrl.h"
#include "is-config.h"
#include "is-param.h"
#include "is-video.h"
#include "is-type.h"

static int is_ischain_3ao_cfg(struct is_subdev *subdev,
	void *device_data,
	struct is_frame *frame,
	struct is_crop *incrop,
	struct is_crop *otcrop,
	u32 *lindex,
	u32 *hindex,
	u32 *indexes)
{
	return 0;
}

static int is_ischain_3ao_start(struct is_device_ischain *device,
	struct is_subdev *subdev,
	struct is_frame *frame,
	struct is_queue *queue,
	struct is_crop *otcrop,
	u32 *lindex,
	u32 *hindex,
	u32 *indexes)
{
	int ret = 0;
	struct is_group *group;
	struct param_dma_output *dma_output;
	u32 hw_format, hw_bitwidth, hw_order, hw_plane;

	FIMC_BUG(!queue);
	FIMC_BUG(!queue->framecfg.format);

	group = &device->group_3aa;

	hw_format = queue->framecfg.format->hw_format;
	hw_order = queue->framecfg.format->hw_order;
	hw_bitwidth = queue->framecfg.format->hw_bitwidth;
	hw_plane = queue->framecfg.format->hw_plane;

	dma_output = is_itf_g_param(device, frame, subdev->param_dma_ot);

	dma_output->cmd = DMA_OUTPUT_COMMAND_ENABLE;
	dma_output->format = hw_format;
	dma_output->plane = hw_plane;
	dma_output->order = hw_order;
	dma_output->bitwidth = hw_bitwidth;
	dma_output->msb = dma_output->bitwidth - 1;
	dma_output->crop_enable = 0;
	dma_output->width = otcrop->w;
	dma_output->height = otcrop->h;
	dma_output->stride_plane0 = queue->framecfg.width;
	dma_output->stride_plane1 = queue->framecfg.width;
	dma_output->stride_plane2 = queue->framecfg.width;
	*lindex |= LOWBIT_OF(subdev->param_dma_ot);
	*hindex |= HIGHBIT_OF(subdev->param_dma_ot);
	(*indexes)++;

	subdev->output.crop = *otcrop;

	set_bit(IS_SUBDEV_RUN, &subdev->state);

	return ret;
}

static int is_ischain_3ao_stop(struct is_device_ischain *device,
	struct is_subdev *subdev,
	struct is_frame *frame,
	u32 *lindex,
	u32 *hindex,
	u32 *indexes)
{
	int ret = 0;
	struct param_dma_output *dma_output;

	mdbgd_ischain("%s\n", device, __func__);

	dma_output = is_itf_g_param(device, frame, subdev->param_dma_ot);
	dma_output->cmd = DMA_OUTPUT_COMMAND_DISABLE;
	*lindex |= LOWBIT_OF(subdev->param_dma_ot);
	*hindex |= HIGHBIT_OF(subdev->param_dma_ot);
	(*indexes)++;

	clear_bit(IS_SUBDEV_RUN, &subdev->state);

	return ret;
}

static int is_ischain_3ao_tag(struct is_subdev *subdev,
	void *device_data,
	struct is_frame *ldr_frame,
	struct camera2_node *node)
{
	int ret = 0;
	struct is_subdev *leader;
	struct is_queue *queue;
	struct taa_param *taa_param;
	struct is_crop *otcrop, otparm;
	struct is_device_ischain *device;
	u32 lindex, hindex, indexes;
	u32 pixelformat = 0;

	device = (struct is_device_ischain *)device_data;

	FIMC_BUG(!device);
	FIMC_BUG(!device->is_region);
	FIMC_BUG(!subdev);
	FIMC_BUG(!GET_SUBDEV_QUEUE(subdev));
	FIMC_BUG(!ldr_frame);
	FIMC_BUG(!ldr_frame->shot);
	FIMC_BUG(!node);

	mdbgs_ischain(4, "3AO TAG(request %d)\n", device, node->request);

	lindex = hindex = indexes = 0;
	leader = subdev->leader;
	taa_param = &device->is_region->parameter.taa;
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
	otcrop = (struct is_crop *)node->output.cropRegion;

	otparm.x = 0;
	otparm.y = 0;
	otparm.w = taa_param->ods_output.width;
	otparm.h = taa_param->ods_output.height;

	if (IS_NULL_CROP(otcrop))
		*otcrop = otparm;

	if (node->request) {
		if (!COMPARE_CROP(otcrop, &otparm) ||
			!test_bit(IS_SUBDEV_RUN, &subdev->state) ||
			test_bit(IS_SUBDEV_FORCE_SET, &leader->state)) {

			ret = is_ischain_3ao_start(device,
				subdev,
				ldr_frame,
				queue,
				otcrop,
				&lindex,
				&hindex,
				&indexes);

			if (ret) {
				merr("is_ischain_3ao_start is fail(%d)", device, ret);
				goto p_err;
			}

			mdbg_pframe("ot_crop[%d, %d, %d, %d] on\n", device, subdev, ldr_frame,
				otcrop->x, otcrop->y, otcrop->w, otcrop->h);
		}

		ret = is_ischain_buf_tag(device,
			subdev,
			ldr_frame,
			pixelformat,
			otcrop->w,
			otcrop->h,
			ldr_frame->txodsTargetAddress);
		if (ret) {
			mswarn("%d frame is drop", device, subdev, ldr_frame->fcount);
			node->request = 0;
		}
	} else {
		if (!COMPARE_CROP(otcrop, &otparm) ||
			test_bit(IS_SUBDEV_RUN, &subdev->state) ||
			test_bit(IS_SUBDEV_FORCE_SET, &leader->state)) {
			ret = is_ischain_3ao_stop(device,
				subdev,
				ldr_frame,
				&lindex,
				&hindex,
				&indexes);
			if (ret) {
				merr("is_ischain_3ao_stop is fail(%d)", device, ret);
				goto p_err;
			}

			mdbg_pframe("ot_crop[%d, %d, %d, %d] off\n", device, subdev, ldr_frame,
				otcrop->x, otcrop->y, otcrop->w, otcrop->h);
		}

		ldr_frame->txodsTargetAddress[0] = 0;
		ldr_frame->txodsTargetAddress[1] = 0;
		ldr_frame->txodsTargetAddress[2] = 0;
		node->request = 0;
	}

	ret = is_itf_s_param(device, ldr_frame, lindex, hindex, indexes);
	if (ret) {
		mrerr("is_itf_s_param is fail(%d)", device, ldr_frame, ret);
		goto p_err;
	}

p_err:
	return ret;
}

const struct is_subdev_ops is_subdev_3ao_ops = {
	.bypass			= NULL,
	.cfg			= is_ischain_3ao_cfg,
	.tag			= is_ischain_3ao_tag,
};
