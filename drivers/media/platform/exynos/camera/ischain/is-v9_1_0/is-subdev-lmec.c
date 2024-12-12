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

static int is_ischain_lmec_cfg(struct is_subdev *subdev,
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

static int is_ischain_lmec_start(struct is_device_ischain *device,
	struct is_subdev *subdev,
	struct is_frame *frame,
	struct is_queue *queue,
	struct is_crop *otcrop,
	u32 *lindex,
	u32 *hindex,
	u32 *indexes)
{
	int ret = 0;
	struct param_lme_dma *dma_output;
	struct is_fmt *format;

	FIMC_BUG(!queue);
	FIMC_BUG(!queue->framecfg.format);

	format = queue->framecfg.format;

	dma_output = is_itf_g_param(device, frame, PARAM_LME_DMA);
	dma_output->output_cmd = DMA_OUTPUT_COMMAND_ENABLE;
	dma_output->output_format = format->hw_format;
	dma_output->output_order = format->hw_order;
	dma_output->output_bitwidth = format->hw_bitwidth;
	dma_output->output_plane = format->hw_plane;
	dma_output->output_msb = format->hw_bitwidth - 1;
	dma_output->output_width = otcrop->w;
	dma_output->output_height = otcrop->h;

	*lindex |= LOWBIT_OF(PARAM_LME_DMA);
	*hindex |= HIGHBIT_OF(PARAM_LME_DMA);
	(*indexes)++;

	subdev->output.crop = *otcrop;

	set_bit(IS_SUBDEV_RUN, &subdev->state);

	return ret;
}

static int is_ischain_lmec_stop(struct is_device_ischain *device,
	struct is_subdev *subdev,
	struct is_frame *frame,
	u32 *lindex,
	u32 *hindex,
	u32 *indexes)
{
	int ret = 0;
	struct param_lme_dma *dma_output;

	mdbgd_ischain("%s\n", device, __func__);

	dma_output = is_itf_g_param(device, frame, PARAM_LME_DMA);
	dma_output->output_cmd = DMA_OUTPUT_COMMAND_DISABLE;
	*lindex |= LOWBIT_OF(PARAM_LME_DMA);
	*hindex |= HIGHBIT_OF(PARAM_LME_DMA);
	(*indexes)++;

	clear_bit(IS_SUBDEV_RUN, &subdev->state);

	return ret;
}

static int is_ischain_lmec_tag(struct is_subdev *subdev,
	void *device_data,
	struct is_frame *ldr_frame,
	struct camera2_node *node)
{
	int ret = 0;
	struct is_subdev *leader;
	struct is_queue *queue;
	struct lme_param *lme_param;
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

	mdbgs_ischain(4, "LMEC TAG(request %d)\n", device, node->request);

	lindex = hindex = indexes = 0;
	leader = subdev->leader;
	lme_param = &device->is_region->parameter.lme;
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

	if (node->request) {
		otcrop = (struct is_crop *)node->output.cropRegion;

		otparm.x = 0;
		otparm.y = 0;
		otparm.w = lme_param->dma.output_width;
		otparm.h = lme_param->dma.output_height;

		if (!COMPARE_CROP(otcrop, &otparm) ||
			!test_bit(IS_SUBDEV_RUN, &subdev->state) ||
			test_bit(IS_SUBDEV_FORCE_SET, &leader->state)) {
			ret = is_ischain_lmec_start(device,
				subdev,
				ldr_frame,
				queue,
				otcrop,
				&lindex,
				&hindex,
				&indexes);
			if (ret) {
				merr("is_ischain_lmec_start is fail(%d)", device, ret);
				goto p_err;
			}

			mdbg_pframe("ot_crop[%d, %d, %d, %d]\n", device, subdev, ldr_frame,
				otcrop->x, otcrop->y, otcrop->w, otcrop->h);
		}

		ret = is_ischain_buf_tag(device,
			subdev,
			ldr_frame,
			pixelformat,
			otcrop->w,
			otcrop->h,
			ldr_frame->lmecTargetAddress);
		if (ret) {
			mswarn("%d frame is drop", device, subdev, ldr_frame->fcount);
			node->request = 0;
		}
	} else {
		if (test_bit(IS_SUBDEV_RUN, &subdev->state)) {
			ret = is_ischain_lmec_stop(device,
				subdev,
				ldr_frame,
				&lindex,
				&hindex,
				&indexes);
			if (ret) {
				merr("is_ischain_lmec_stop is fail(%d)", device, ret);
				goto p_err;
			}

			mdbg_pframe(" off\n", device, subdev, ldr_frame);
		}

		ldr_frame->lmecTargetAddress[0] = 0;
		ldr_frame->lmecTargetAddress[1] = 0;
		ldr_frame->lmecTargetAddress[2] = 0;
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

const struct is_subdev_ops is_subdev_lmec_ops = {
	.bypass			= NULL,
	.cfg			= is_ischain_lmec_cfg,
	.tag			= is_ischain_lmec_tag,
};
