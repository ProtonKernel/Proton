// SPDX-License-Identifier: GPL-2.0
/*
 * Samsung Exynos SoC series Pablo driver
 *
 * Copyright (c) 2019 Samsung Electronics Co., Ltd
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

static int is_ischain_ixw_cfg(struct is_subdev *subdev,
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

static int is_ischain_ixw_start(struct is_device_ischain *device,
	struct is_subdev *subdev,
	struct is_frame *frame,
	struct is_queue *queue,
	struct is_crop *otcrop,
	u32 *lindex,
	u32 *hindex,
	u32 *indexes)
{
	return 0;
}

static int is_ischain_ixw_stop(struct is_device_ischain *device,
	struct is_subdev *subdev,
	struct is_frame *frame,
	u32 *lindex,
	u32 *hindex,
	u32 *indexes)
{
	int ret = 0;
	struct param_dma_output *vdma7_output;

	mdbgd_ischain("%s\n", device, __func__);

	vdma7_output = is_itf_g_param(device, frame, PARAM_ISP_VDMA7_OUTPUT);
	vdma7_output->cmd = DMA_OUTPUT_COMMAND_DISABLE;
	*lindex |= LOWBIT_OF(PARAM_ISP_VDMA7_OUTPUT);
	*hindex |= HIGHBIT_OF(PARAM_ISP_VDMA7_OUTPUT);
	(*indexes)++;

	clear_bit(IS_SUBDEV_RUN, &subdev->state);

	return ret;
}

static int is_ischain_ixw_tag(struct is_subdev *subdev,
	void *device_data,
	struct is_frame *ldr_frame,
	struct camera2_node *node)
{
	int ret = 0;
	struct is_subdev *leader;
	struct is_queue *queue;
	struct isp_param *isp_param;
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

	mdbgs_ischain(4, "ISPW TAG(request %d)\n", device, node->request);

	lindex = hindex = indexes = 0;
	leader = subdev->leader;
	isp_param = &device->is_region->parameter.isp;
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
		otcrop->x = 0;
		otcrop->y = 0;
		otcrop->w = isp_param->otf_input.width;
		otcrop->h = isp_param->otf_input.height;

		otparm.x = 0;
		otparm.y = 0;
		otparm.w = isp_param->vdma5_output.width;
		otparm.h = isp_param->vdma5_output.height;

		if (!COMPARE_CROP(otcrop, &otparm) ||
			!test_bit(IS_SUBDEV_RUN, &subdev->state) ||
			test_bit(IS_SUBDEV_FORCE_SET, &leader->state)) {
			ret = is_ischain_ixw_start(device,
				subdev,
				ldr_frame,
				queue,
				otcrop,
				&lindex,
				&hindex,
				&indexes);
			if (ret) {
				merr("is_ischain_ixc_start is fail(%d)", device, ret);
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
			ldr_frame->ixwTargetAddress);
		if (ret) {
			mswarn("%d frame is drop", device, subdev, ldr_frame->fcount);
			node->request = 0;
		}
	} else {
		if (test_bit(IS_SUBDEV_RUN, &subdev->state)) {
			ret = is_ischain_ixw_stop(device,
				subdev,
				ldr_frame,
				&lindex,
				&hindex,
				&indexes);
			if (ret) {
				merr("is_ischain_ixc_stop is fail(%d)", device, ret);
				goto p_err;
			}

			mdbg_pframe(" off\n", device, subdev, ldr_frame);
		}

		ldr_frame->ixwTargetAddress[0] = 0;
		ldr_frame->ixwTargetAddress[1] = 0;
		ldr_frame->ixwTargetAddress[2] = 0;
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

const struct is_subdev_ops is_subdev_ixw_ops = {
	.bypass			= NULL,
	.cfg			= is_ischain_ixw_cfg,
	.tag			= is_ischain_ixw_tag,
};
