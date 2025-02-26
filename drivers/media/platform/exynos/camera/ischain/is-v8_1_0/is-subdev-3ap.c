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
#include "is-device-sensor.h"
#include "is-subdev-ctrl.h"
#include "is-config.h"
#include "is-param.h"
#include "is-video.h"
#include "is-type.h"

void is_ischain_3ap_stripe_cfg(struct is_subdev *subdev,
		struct is_frame *ldr_frame,
		struct is_crop *incrop,
		struct is_crop *otcrop,
		u32 bitwidth,
		u32 region_num)
{
	struct is_framemgr *framemgr;
	struct is_frame *frame;
	unsigned long flags;
	u32 region_id = ldr_frame->stripe_info.region_id;
	u32 stripe_w = 0, dma_offset = 0;

	framemgr = GET_SUBDEV_FRAMEMGR(subdev);
	if (!framemgr)
		return;

	framemgr_e_barrier_irqs(framemgr, FMGR_IDX_24, flags);

	frame = peek_frame(framemgr, ldr_frame->state);
	if (frame) {
		if (!region_num) {
			frame->stream->stripe_region_num = 0;
			framemgr_x_barrier_irqr(framemgr, FMGR_IDX_24, flags);
			return;
		}

		/* Output crop & WDMA offset configuration */
		if (!region_id) {
			/* Left region */
			stripe_w = ldr_frame->stripe_info.in.h_pix_num;
			frame->stripe_info.out.h_pix_num = stripe_w;
			frame->stripe_info.region_base_addr[0] = frame->dvaddr_buffer[0];
		} else if (region_id < ldr_frame->stripe_info.region_num - 1) {
			/* Middle region */
			stripe_w = ldr_frame->stripe_info.in.h_pix_num - frame->stripe_info.out.h_pix_num;
			dma_offset = frame->stripe_info.out.h_pix_num;
			dma_offset += STRIPE_MARGIN_WIDTH * ((2 * (region_id - 1)) + 1);
			dma_offset = dma_offset / BITS_PER_BYTE * bitwidth * otcrop->h;
			frame->stripe_info.out.h_pix_num += stripe_w;
			stripe_w += STRIPE_MARGIN_WIDTH;
		} else {
			/* Right region */
			stripe_w = incrop->w - frame->stripe_info.out.h_pix_num;
			dma_offset = frame->stripe_info.out.h_pix_num;
			dma_offset += STRIPE_MARGIN_WIDTH * ((2 * (region_id - 1)) + 1);
			dma_offset = dma_offset / BITS_PER_BYTE * bitwidth * otcrop->h;
			frame->stripe_info.out.h_pix_num += stripe_w;
		}

		stripe_w += STRIPE_MARGIN_WIDTH;
		otcrop->w = stripe_w;

		frame->dvaddr_buffer[0] = frame->stripe_info.region_base_addr[0] + dma_offset;
		frame->stream->stripe_h_pix_nums[region_id] = frame->stripe_info.out.h_pix_num;
		frame->stream->stripe_region_num = ldr_frame->stripe_info.region_num;

		if (frame->kvaddr_buffer[0]) {
			frame->stripe_info.region_num = ldr_frame->stripe_info.region_num;
			frame->stripe_info.kva[region_id][0] = frame->kvaddr_buffer[0] + dma_offset;
			frame->stripe_info.size[region_id][0] = stripe_w
							* bitwidth / BITS_PER_BYTE
							* otcrop->h;
		}

		msrdbgs(3, "stripe_in_crop[%d][%d, %d, %d, %d]\n", subdev, subdev, ldr_frame,
				region_id,
				incrop->x, incrop->y, incrop->w, incrop->h);
		msrdbgs(3, "stripe_ot_crop[%d][%d, %d, %d, %d] offset %x\n", subdev, subdev, ldr_frame,
				region_id,
				otcrop->x, otcrop->y, otcrop->w, otcrop->h, dma_offset);
	}

	framemgr_x_barrier_irqr(framemgr, FMGR_IDX_24, flags);
}

static int is_ischain_3ap_cfg(struct is_subdev *subdev,
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

static int is_ischain_3ap_start(struct is_device_ischain *device,
	struct is_subdev *subdev,
	struct is_frame *frame,
	struct is_queue *queue,
	struct taa_param *taa_param,
	struct is_crop *incrop,
	struct is_crop *otcrop,
	u32 *lindex,
	u32 *hindex,
	u32 *indexes)
{
	int ret = 0;
	struct is_group *group;
	struct param_dma_output *dma_output;
	u32 hw_format, hw_bitwidth;
	u32 hw_msb = MSB_OF_3AA_DMA_OUT;
	u32 flag_extra, flag_pixel_size;
	struct is_crop incrop_cfg, otcrop_cfg;

	FIMC_BUG(!queue);
	FIMC_BUG(!queue->framecfg.format);

	group = &device->group_3aa;
	incrop_cfg = *incrop;
	otcrop_cfg = *otcrop;

	hw_format = queue->framecfg.format->hw_format;
	hw_bitwidth = queue->framecfg.format->hw_bitwidth;

	/* pixel type [0:5] : pixel size, [6:7] : extra */
	flag_pixel_size = queue->framecfg.hw_pixeltype & PIXEL_TYPE_SIZE_MASK;
	flag_extra = (queue->framecfg.hw_pixeltype & PIXEL_TYPE_EXTRA_MASK) >> PIXEL_TYPE_EXTRA_SHIFT;

	if (hw_format == DMA_INOUT_FORMAT_BAYER_PACKED
		&& flag_pixel_size == CAMERA_PIXEL_SIZE_13BIT) {
		mdbg_pframe("ot_crop[bitwidth: %d -> %d: 13 bit BAYER]\n", device, subdev, frame,
			hw_bitwidth, DMA_INOUT_BIT_WIDTH_13BIT);
		hw_msb = MSB_OF_3AA_DMA_OUT + 1;
		hw_bitwidth = DMA_INOUT_BIT_WIDTH_13BIT;
	} else if (hw_format == DMA_INOUT_FORMAT_BAYER) {
		hw_msb = hw_bitwidth;	/* consider signed format only */
		hw_bitwidth = DMA_INOUT_BIT_WIDTH_16BIT;
		mdbg_pframe("ot_crop[unpack bitwidth: %d, msb: %d]\n", device, subdev, frame,
			hw_bitwidth, hw_msb);
	}

	if (hw_format == DMA_INOUT_FORMAT_BAYER_PACKED && flag_extra == COMP) {
		mdbg_pframe("ot_crop[fmt: %d -> %d: BAYER_COMP]\n", device, subdev, frame,
			hw_format, DMA_INOUT_FORMAT_BAYER_COMP);
		hw_format = DMA_INOUT_FORMAT_BAYER_COMP;
	}

	if (IS_ENABLED(CHAIN_USE_STRIPE_PROCESSING) && frame)
		is_ischain_3ap_stripe_cfg(subdev,
				frame,
				&incrop_cfg, &otcrop_cfg,
				hw_bitwidth,
				frame->stripe_info.region_num);

	if ((otcrop_cfg.w > taa_param->otf_input.bayer_crop_width) ||
		(otcrop_cfg.h > taa_param->otf_input.bayer_crop_height)) {
		mrerr("bds output size is invalid((%d, %d) > (%d, %d))", device, frame,
			otcrop_cfg.w,
			otcrop_cfg.h,
			taa_param->otf_input.bayer_crop_width,
			taa_param->otf_input.bayer_crop_height);
		ret = -EINVAL;
		goto p_err;
	}

	/*
	 * 3AA BDS ratio limitation on width, height
	 * ratio = input * 256 / output
	 * real output = input * 256 / ratio
	 * real output &= ~1
	 * real output is same with output crop
	 */
	dma_output = is_itf_g_param(device, frame, subdev->param_dma_ot);
	dma_output->cmd = DMA_OUTPUT_COMMAND_ENABLE;
	dma_output->format = hw_format;
	dma_output->bitwidth = hw_bitwidth;
	dma_output->msb = hw_msb;
#ifdef USE_3AA_CROP_AFTER_BDS
	if (!test_bit(IS_ISCHAIN_REPROCESSING, &device->state)) {
		if (otcrop->x || otcrop->y)
			mwarn("crop pos(%d, %d) is ignored", device, otcrop->x, otcrop->y);

		dma_output->width = otcrop_cfg.w;
		dma_output->height = otcrop_cfg.h;
		dma_output->crop_enable = 0;
	} else {
		dma_output->width = taa_param->otf_input.bayer_crop_width;
		dma_output->height = taa_param->otf_input.bayer_crop_height;
		dma_output->crop_enable = 1;
		dma_output->dma_crop_offset_x = otcrop_cfg.x;
		dma_output->dma_crop_offset_y = otcrop_cfg.y;
		dma_output->dma_crop_width = otcrop_cfg.w;
		dma_output->dma_crop_height = otcrop_cfg.h;
	}
#else
	if (otcrop->x || otcrop->y)
		mwarn("crop pos(%d, %d) is ignored", device, otcrop->x, otcrop->y);

	dma_output->width = otcrop_cfg.w;
	dma_output->height = otcrop_cfg.h;
	dma_output->crop_enable = 0;
#endif
	*lindex |= LOWBIT_OF(subdev->param_dma_ot);
	*hindex |= HIGHBIT_OF(subdev->param_dma_ot);
	(*indexes)++;

	subdev->output.crop = *otcrop;

	set_bit(IS_SUBDEV_RUN, &subdev->state);

p_err:
	return ret;
}

static int is_ischain_3ap_stop(struct is_device_ischain *device,
	struct is_subdev *subdev,
	struct is_frame *frame,
	struct taa_param *taa_param,
	struct is_crop *otcrop,
	u32 *lindex,
	u32 *hindex,
	u32 *indexes)
{
	int ret = 0;
	struct is_group *group;
	struct param_dma_output *dma_output;

	mdbgd_ischain("%s\n", device, __func__);

	group = &device->group_3aa;

	if ((otcrop->w > taa_param->otf_input.bayer_crop_width) ||
		(otcrop->h > taa_param->otf_input.bayer_crop_height)) {
		mrerr("bds output size is invalid((%d, %d) > (%d, %d))", device, frame,
			otcrop->w,
			otcrop->h,
			taa_param->otf_input.bayer_crop_width,
			taa_param->otf_input.bayer_crop_height);
		ret = -EINVAL;
		goto p_err;
	}

	dma_output = is_itf_g_param(device, frame, subdev->param_dma_ot);
	dma_output->cmd = DMA_OUTPUT_COMMAND_DISABLE;
#ifdef USE_3AA_CROP_AFTER_BDS
	if (!test_bit(IS_ISCHAIN_REPROCESSING, &device->state)) {
		if (otcrop->x || otcrop->y)
			mwarn("crop pos(%d, %d) is ignored", device, otcrop->x, otcrop->y);

		dma_output->width = otcrop->w;
		dma_output->height = otcrop->h;
		dma_output->crop_enable = 0;
	} else {
		dma_output->width = taa_param->otf_input.bayer_crop_width;
		dma_output->height = taa_param->otf_input.bayer_crop_height;
		dma_output->crop_enable = 1;
		dma_output->dma_crop_offset_x = otcrop->x;
		dma_output->dma_crop_offset_y = otcrop->y;
		dma_output->dma_crop_width = otcrop->w;
		dma_output->dma_crop_height = otcrop->h;
	}
#else
	if (otcrop->x || otcrop->y)
		mwarn("crop pos(%d, %d) is ignored", device, otcrop->x, otcrop->y);

	dma_output->width = otcrop->w;
	dma_output->height = otcrop->h;
	dma_output->crop_enable = 0;
#endif
	*lindex |= LOWBIT_OF(subdev->param_dma_ot);
	*hindex |= HIGHBIT_OF(subdev->param_dma_ot);
	(*indexes)++;

	clear_bit(IS_SUBDEV_RUN, &subdev->state);

p_err:
	return ret;
}

static int is_ischain_3ap_tag(struct is_subdev *subdev,
	void *device_data,
	struct is_frame *ldr_frame,
	struct camera2_node *node)
{
	int ret = 0;
	struct is_subdev *leader;
	struct is_queue *queue;
	struct taa_param *taa_param;
	struct is_crop *otcrop, otparm, inparm, *incrop;
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

	mdbgs_ischain(4, "3AAP TAG(request %d)\n", device, node->request);

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
	otparm.w = taa_param->vdma2_output.width;
	otparm.h = taa_param->vdma2_output.height;
#ifdef USE_3AA_CROP_AFTER_BDS
	if (test_bit(IS_ISCHAIN_REPROCESSING, &device->state)) {
		otparm.x = taa_param->vdma2_output.dma_crop_offset_x;
		otparm.y = taa_param->vdma2_output.dma_crop_offset_y;
		otparm.w = taa_param->vdma2_output.dma_crop_width;
		otparm.h = taa_param->vdma2_output.dma_crop_height;
	}
#endif

	if (IS_NULL_CROP(otcrop))
		*otcrop = otparm;

	if (node->request) {
		incrop = (struct is_crop *)node->input.cropRegion;

		/* Need to check : vdma1_input? otf_input? */
		inparm.x = taa_param->vdma1_input.bayer_crop_offset_x;
		inparm.y = taa_param->vdma1_input.bayer_crop_offset_y;
		inparm.w = taa_param->vdma1_input.bayer_crop_width;
		inparm.h = taa_param->vdma1_input.bayer_crop_height;

		if (IS_NULL_CROP(incrop))
			*incrop = inparm;

		if (!COMPARE_CROP(otcrop, &otparm) ||
			CHECK_STRIPE_CFG(&ldr_frame->stripe_info) ||
			!test_bit(IS_SUBDEV_RUN, &subdev->state) ||
			test_bit(IS_SUBDEV_FORCE_SET, &leader->state)) {
			ret = is_ischain_3ap_start(device,
				subdev,
				ldr_frame,
				queue,
				taa_param,
				incrop,
				otcrop,
				&lindex,
				&hindex,
				&indexes);
			if (ret) {
				merr("is_ischain_3ap_start is fail(%d)", device, ret);
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
			ldr_frame->txpTargetAddress);
		if (ret) {
			mswarn("%d frame is drop", device, subdev, ldr_frame->fcount);
			node->request = 0;
		}
	} else {
		if (!COMPARE_CROP(otcrop, &otparm) ||
			test_bit(IS_SUBDEV_RUN, &subdev->state) ||
			test_bit(IS_SUBDEV_FORCE_SET, &leader->state)) {
			ret = is_ischain_3ap_stop(device,
				subdev,
				ldr_frame,
				taa_param,
				otcrop,
				&lindex,
				&hindex,
				&indexes);
			if (ret) {
				merr("is_ischain_3ap_stop is fail(%d)", device, ret);
				goto p_err;
			}

			mdbg_pframe("ot_crop[%d, %d, %d, %d] off\n", device, subdev, ldr_frame,
				otcrop->x, otcrop->y, otcrop->w, otcrop->h);
		}

		ldr_frame->txpTargetAddress[0] = 0;
		ldr_frame->txpTargetAddress[1] = 0;
		ldr_frame->txpTargetAddress[2] = 0;
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

const struct is_subdev_ops is_subdev_3ap_ops = {
	.bypass			= NULL,
	.cfg			= is_ischain_3ap_cfg,
	.tag			= is_ischain_3ap_tag,
};
