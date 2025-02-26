/*
 * Samsung Exynos SoC series Pablo driver
 *
 * Exynos pablo subdev functions
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

#include "is-core.h"
#include "is-dvfs.h"
#include "is-hw-dvfs.h"

static int is_ischain_ixt_cfg(struct is_subdev *subdev,
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

static int is_ischain_ixt_start(struct is_device_ischain *device,
	struct is_subdev *subdev,
	struct is_frame *frame,
	struct is_queue *queue,
	struct is_crop *incrop,
	u32 *lindex,
	u32 *hindex,
	u32 *indexes)
{
	int ret = 0;
	struct param_dma_input *dma_input;
	struct is_fmt *format;
	u32 hw_format = DMA_INOUT_FORMAT_BAYER;
	u32 hw_bitwidth = DMA_INOUT_BIT_WIDTH_16BIT;
	u32 hw_msb = MSB_OF_3AA_DMA_OUT;
	u32 flag_extra, flag_pixel_size;

	FIMC_BUG(!device);
	FIMC_BUG(!incrop);
	FIMC_BUG(!lindex);
	FIMC_BUG(!hindex);
	FIMC_BUG(!indexes);
	FIMC_BUG(!queue);
	FIMC_BUG(!queue->framecfg.format);

	format = queue->framecfg.format;
	if (!format) {
		merr("format is NULL", device);
		ret = -EINVAL;
		goto p_err;
	}

	hw_format = format->hw_format;
	hw_bitwidth = format->hw_bitwidth;
	/* pixel type [0:5] : pixel size, [6:7] : extra */
	flag_pixel_size = queue->framecfg.hw_pixeltype & PIXEL_TYPE_SIZE_MASK;
	flag_extra = (queue->framecfg.hw_pixeltype & PIXEL_TYPE_EXTRA_MASK) >> PIXEL_TYPE_EXTRA_SHIFT;

	if (hw_format == DMA_INOUT_FORMAT_BAYER_PACKED
		&& flag_pixel_size == CAMERA_PIXEL_SIZE_13BIT) {
		msinfo("in_crop[bitwidth: %d -> %d: 13 bit BAYER]\n", device, subdev,
			hw_bitwidth, DMA_INOUT_BIT_WIDTH_13BIT);
		hw_msb = MSB_OF_3AA_DMA_OUT + 1;
		hw_bitwidth = DMA_INOUT_BIT_WIDTH_13BIT;
	} else if (hw_format == DMA_INOUT_FORMAT_BAYER) {
		hw_msb = hw_bitwidth;	/* consider signed format only */
		hw_bitwidth = DMA_INOUT_BIT_WIDTH_16BIT;
		msinfo("in_crop[unpack bitwidth: %d, msb: %d]\n", device, subdev,
			hw_bitwidth, hw_msb);
	}

	dma_input = is_itf_g_param(device, frame, PARAM_ISP_VDMA3_INPUT);
	dma_input->cmd = DMA_INPUT_COMMAND_ENABLE;
	dma_input->format = hw_format;
	dma_input->bitwidth = hw_bitwidth;
	dma_input->msb = hw_msb;
	dma_input->order = format->hw_order;
	dma_input->plane = format->hw_plane;
	dma_input->width = incrop->w;
	dma_input->height = incrop->h;
	dma_input->dma_crop_offset = (incrop->x << 16) | (incrop->y << 0);
	dma_input->dma_crop_width = incrop->w;
	dma_input->dma_crop_height = incrop->h;
	/* VOTF of slave in is alwasy disabled */
	dma_input->v_otf_enable = OTF_INPUT_COMMAND_DISABLE;
	dma_input->v_otf_token_line = VOTF_TOKEN_LINE;

	*lindex |= LOWBIT_OF(PARAM_ISP_VDMA3_INPUT);
	*hindex |= HIGHBIT_OF(PARAM_ISP_VDMA3_INPUT);
	(*indexes)++;

	subdev->input.crop = *incrop;

	set_bit(IS_SUBDEV_RUN, &subdev->state);

p_err:
	return ret;
}

static int is_ischain_ixt_stop(struct is_device_ischain *device,
	struct is_subdev *subdev,
	struct is_frame *frame,
	u32 *lindex,
	u32 *hindex,
	u32 *indexes)
{
	int ret = 0;
	struct param_dma_input *vdma3_input;

	mdbgd_ischain("%s\n", device, __func__);

	vdma3_input = is_itf_g_param(device, frame, PARAM_ISP_VDMA3_INPUT);
	vdma3_input->cmd = DMA_OUTPUT_COMMAND_DISABLE;
	*lindex |= LOWBIT_OF(PARAM_ISP_VDMA3_INPUT);
	*hindex |= HIGHBIT_OF(PARAM_ISP_VDMA3_INPUT);
	(*indexes)++;

	clear_bit(IS_SUBDEV_RUN, &subdev->state);

	return ret;
}

static int is_ischain_ixt_tag(struct is_subdev *subdev,
	void *device_data,
	struct is_frame *ldr_frame,
	struct camera2_node *node)
{
	int ret = 0;
	struct is_subdev *leader;
	struct is_queue *queue;
	struct isp_param *isp_param;
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
	FIMC_BUG(!node);

	mdbgs_ischain(4, "ISPT TAG(request %d)\n", device, node->request);

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
		incrop = (struct is_crop *)node->input.cropRegion;

		inparm.x = 0;
		inparm.y = 0;
		inparm.w = isp_param->vdma3_input.width;
		inparm.h = isp_param->vdma3_input.height;

		if (IS_NULL_CROP(incrop))
			*incrop = inparm;

		if (!COMPARE_CROP(incrop, &inparm) ||
			!test_bit(IS_SUBDEV_RUN, &subdev->state) ||
			test_bit(IS_SUBDEV_FORCE_SET, &leader->state)) {
			ret = is_ischain_ixt_start(device,
				subdev,
				ldr_frame,
				queue,
				incrop,
				&lindex,
				&hindex,
				&indexes);
			if (ret) {
				merr("is_ischain_ixt_start is fail(%d)", device, ret);
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
			ldr_frame->ixtTargetAddress);
		if (ret) {
			mswarn("%d frame is drop", device, subdev, ldr_frame->fcount);
			node->request = 0;
		}
	} else {
		if (test_bit(IS_SUBDEV_RUN, &subdev->state)) {
			ret = is_ischain_ixt_stop(device,
				subdev,
				ldr_frame,
				&lindex,
				&hindex,
				&indexes);
			if (ret) {
				merr("is_ischain_ixt_stop is fail(%d)", device, ret);
				goto p_err;
			}

			mdbg_pframe(" off\n", device, subdev, ldr_frame);
		}

		ldr_frame->ixtTargetAddress[0] = 0;
		ldr_frame->ixtTargetAddress[1] = 0;
		ldr_frame->ixtTargetAddress[2] = 0;
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

const struct is_subdev_ops is_subdev_ixt_ops = {
	.bypass			= NULL,
	.cfg			= is_ischain_ixt_cfg,
	.tag			= is_ischain_ixt_tag,
};
