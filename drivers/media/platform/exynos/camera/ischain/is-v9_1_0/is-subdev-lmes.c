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

static int is_ischain_lmes_cfg(struct is_subdev *subdev,
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

static int is_ischain_lmes_start(struct is_device_ischain *device,
	struct is_subdev *subdev,
	struct is_frame *frame,
	struct is_queue *queue,
	struct is_crop *incrop,
	u32 *lindex,
	u32 *hindex,
	u32 *indexes)
{
	int ret = 0;
	struct is_group *group;
	struct param_lme_dma *dma_prev_input;
	struct is_fmt *format;

	FIMC_BUG(!queue);
	FIMC_BUG(!queue->framecfg.format);

	format = queue->framecfg.format;
	group = &device->group_lme;

	dma_prev_input = is_itf_g_param(device, frame, PARAM_LME_DMA);
	if (test_bit(IS_GROUP_OTF_INPUT, &group->state)) {
		if (test_bit(IS_GROUP_VOTF_INPUT, &group->state)) {
			dma_prev_input->prev_input_cmd = DMA_INPUT_COMMAND_ENABLE;
		} else {
			dma_prev_input->prev_input_cmd = DMA_INPUT_COMMAND_DISABLE;
		}
	} else {
		dma_prev_input->prev_input_cmd = DMA_INPUT_COMMAND_ENABLE;
	}


	dma_prev_input->prev_input_width = incrop->w;
	dma_prev_input->prev_input_height = incrop->h;

	*lindex |= LOWBIT_OF(PARAM_LME_DMA);
	*hindex |= HIGHBIT_OF(PARAM_LME_DMA);
	(*indexes)++;

	subdev->input.crop = *incrop;

	set_bit(IS_SUBDEV_RUN, &subdev->state);

	return ret;
}

static int is_ischain_lmes_stop(struct is_device_ischain *device,
	struct is_subdev *subdev,
	struct is_frame *frame,
	u32 *lindex,
	u32 *hindex,
	u32 *indexes)
{
	int ret = 0;
	struct param_lme_dma *dma_prev_input;

	mdbgd_ischain("%s\n", device, __func__);

	dma_prev_input = is_itf_g_param(device, frame, PARAM_LME_DMA);
	dma_prev_input->prev_input_cmd = DMA_INPUT_COMMAND_DISABLE;
	*lindex |= LOWBIT_OF(PARAM_LME_DMA);
	*hindex |= HIGHBIT_OF(PARAM_LME_DMA);
	(*indexes)++;

	clear_bit(IS_SUBDEV_RUN, &subdev->state);

	return ret;
}

static int is_ischain_lmes_tag(struct is_subdev *subdev,
	void *device_data,
	struct is_frame *ldr_frame,
	struct camera2_node *node)
{
	int ret = 0;
	struct is_subdev *leader;
	struct is_queue *queue;
	struct lme_param *lme_param;
	struct is_crop *incrop, inparm;
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

	mdbgs_ischain(4, "LMES TAG(request %d)\n", device, node->request);

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
		incrop = (struct is_crop *)node->input.cropRegion;

		/* Only DMA input case */
		inparm.x = 0;
		inparm.y = 0;
		inparm.w = lme_param->dma.prev_input_width;
		inparm.h = lme_param->dma.prev_input_height;

		if (IS_NULL_CROP(incrop))
			*incrop = inparm;

		if (!COMPARE_CROP(incrop, &inparm) ||
			!test_bit(IS_SUBDEV_RUN, &subdev->state) ||
			test_bit(IS_SUBDEV_FORCE_SET, &leader->state)) {
			ret = is_ischain_lmes_start(device,
				subdev,
				ldr_frame,
				queue,
				incrop,
				&lindex,
				&hindex,
				&indexes);
			if (ret) {
				merr("is_ischain_lmes_start is fail(%d)", device, ret);
				goto p_err;
			}

			mdbg_pframe("in_crop[%d, %d, %d, %d]\n", device, subdev, ldr_frame,
				incrop->x, incrop->y, incrop->w, incrop->h);
		}

		ret = is_ischain_buf_tag(device,
			subdev,
			ldr_frame,
			pixelformat,
			incrop->w,
			incrop->h,
			ldr_frame->lmesTargetAddress);

		if (ret) {
			mswarn("%d frame is drop", device, subdev, ldr_frame->fcount);
			node->request = 0;
		}

	} else {
		if (test_bit(IS_SUBDEV_RUN, &subdev->state)) {
			ret = is_ischain_lmes_stop(device,
				subdev,
				ldr_frame,
				&lindex,
				&hindex,
				&indexes);
			if (ret) {
				merr("is_ischain_lmes_stop is fail(%d)", device, ret);
				goto p_err;
			}

			mdbg_pframe(" off\n", device, subdev, ldr_frame);
		}

		ldr_frame->lmesTargetAddress[0] = 0;
		ldr_frame->lmesTargetAddress[1] = 0;
		ldr_frame->lmesTargetAddress[2] = 0;
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

const struct is_subdev_ops is_subdev_lmes_ops = {
	.bypass			= NULL,
	.cfg			= is_ischain_lmes_cfg,
	.tag			= is_ischain_lmes_tag,
};
