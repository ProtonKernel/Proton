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

void is_ischain_ixc_stripe_cfg(struct is_subdev *subdev,
		struct is_frame *ldr_frame,
		struct is_crop *otcrop,
		struct is_frame_cfg *framecfg)
{
	struct is_framemgr *framemgr;
	struct is_frame *frame;
	struct is_fmt *fmt = framecfg->format;
	unsigned long flags;
	u32 stripe_w = 0, dma_offset = 0;
	u32 region_id = ldr_frame->stripe_info.region_id;
	framemgr = GET_SUBDEV_FRAMEMGR(subdev);
	if (!framemgr)
		return;

	framemgr_e_barrier_irqs(framemgr, FMGR_IDX_24, flags);

	frame = peek_frame(framemgr, ldr_frame->state);
	if (frame) {
		/* Output crop & WDMA offset configuration */
		if (!region_id) {
			/* Left region */
			stripe_w = ldr_frame->stripe_info.in.h_pix_num[region_id];
			frame->stripe_info.out.h_pix_num[region_id] = stripe_w;
			frame->stripe_info.region_base_addr[0] = frame->dvaddr_buffer[0];
			frame->stripe_info.region_base_addr[1] = frame->dvaddr_buffer[1];
		} else if (region_id < ldr_frame->stripe_info.region_num - 1) {
			/* Middle region */
			stripe_w = ldr_frame->stripe_info.in.h_pix_num[region_id] - frame->stripe_info.out.h_pix_num[region_id - 1];
			dma_offset = frame->stripe_info.out.h_pix_num[region_id - 1];
			dma_offset += STRIPE_MARGIN_WIDTH * ((2 * (region_id - 1)) + 1);
			dma_offset = ((dma_offset * fmt->bitsperpixel[0] * otcrop->h) / BITS_PER_BYTE);
			frame->stripe_info.out.h_pix_num[region_id] = frame->stripe_info.out.h_pix_num[region_id - 1] + stripe_w;
			stripe_w += STRIPE_MARGIN_WIDTH;
		} else {
			/* Right region */
			stripe_w = otcrop->w - frame->stripe_info.out.h_pix_num[region_id - 1];
			dma_offset = frame->stripe_info.out.h_pix_num[region_id - 1];
			dma_offset += STRIPE_MARGIN_WIDTH * ((2 * (region_id - 1)) + 1);
			dma_offset = ((dma_offset * fmt->bitsperpixel[0] * otcrop->h) / BITS_PER_BYTE);
			frame->stripe_info.out.h_pix_num[region_id] = frame->stripe_info.out.h_pix_num[region_id - 1] + stripe_w;
		}

		stripe_w += STRIPE_MARGIN_WIDTH;
		otcrop->w = stripe_w;

		frame->dvaddr_buffer[0] = frame->stripe_info.region_base_addr[0] + dma_offset;
		frame->dvaddr_buffer[1] = frame->stripe_info.region_base_addr[1] + dma_offset;
		frame->stream->stripe_h_pix_nums[region_id] = frame->stripe_info.out.h_pix_num[region_id];

		if (frame->kvaddr_buffer[0]) {
			frame->stripe_info.region_num = ldr_frame->stripe_info.region_num;
			frame->stripe_info.kva[region_id][0] = frame->kvaddr_buffer[0] + dma_offset;
			frame->stripe_info.size[region_id][0] = stripe_w
							* fmt->bitsperpixel[0] / BITS_PER_BYTE
							* otcrop->h;
		}

		msrdbgs(3, "stripe_ot_crop[%d][%d, %d, %d, %d] offset %x\n", subdev, subdev, ldr_frame,
				region_id,
				otcrop->x, otcrop->y, otcrop->w, otcrop->h, dma_offset);
	}

	framemgr_x_barrier_irqr(framemgr, FMGR_IDX_24, flags);
}

static int is_ischain_ixc_cfg(struct is_subdev *subdev,
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

static int is_ischain_ixc_start(struct is_device_ischain *device,
	struct is_subdev *subdev,
	struct is_frame *frame,
	struct is_queue *queue,
	struct is_crop *otcrop,
	u32 *lindex,
	u32 *hindex,
	u32 *indexes)
{
	int ret = 0;
	struct param_dma_output *dma_output;
	struct is_fmt *format;
	struct is_crop otcrop_cfg;
	u32 votf_dst_ip = 0;
	u32 votf_dst_axi_id_m = 0;
	u32 flag_extra;
	u32 hw_sbwc = 0;
	bool chg_format = false;

	FIMC_BUG(!queue);
	FIMC_BUG(!queue->framecfg.format);

	format = queue->framecfg.format;
	otcrop_cfg = *otcrop;

	flag_extra = (queue->framecfg.hw_pixeltype & PIXEL_TYPE_EXTRA_MASK)
			>> PIXEL_TYPE_EXTRA_SHIFT;

	if (flag_extra) {
		hw_sbwc = (SBWC_BASE_ALIGN_MASK_LLC_OFF | flag_extra);
		chg_format = true;
	}

	if (chg_format)
		mdbg_pframe("ot_crop[bitwidth %d sbwc 0x%x]\n",
				device, subdev, frame,
				format->hw_bitwidth,
				hw_sbwc);

	if (IS_ENABLED(CHAIN_USE_STRIPE_PROCESSING) && frame && frame->stripe_info.region_num)
		is_ischain_ixc_stripe_cfg(subdev, frame,
				&otcrop_cfg,
				&queue->framecfg);

	dma_output = is_itf_g_param(device, frame, PARAM_ISP_VDMA4_OUTPUT);
	dma_output->cmd = DMA_OUTPUT_COMMAND_ENABLE;
	dma_output->format = format->hw_format;
	dma_output->order = format->hw_order;
	dma_output->bitwidth = format->hw_bitwidth;
	dma_output->sbwc_type = hw_sbwc;
	dma_output->plane = format->hw_plane;
	dma_output->width = otcrop_cfg.w;
	dma_output->height = otcrop_cfg.h;
	dma_output->dma_crop_offset_x = 0;
	dma_output->dma_crop_offset_y = 0;
	dma_output->dma_crop_width = otcrop_cfg.w;
	dma_output->dma_crop_height = otcrop_cfg.h;

	dma_output->v_otf_enable = OTF_INPUT_COMMAND_DISABLE;
	dma_output->v_otf_dst_ip = votf_dst_ip;
	dma_output->v_otf_dst_axi_id = votf_dst_axi_id_m;
	dma_output->v_otf_token_line = VOTF_TOKEN_LINE;

	*lindex |= LOWBIT_OF(PARAM_ISP_VDMA4_OUTPUT);
	*hindex |= HIGHBIT_OF(PARAM_ISP_VDMA4_OUTPUT);
	(*indexes)++;

	set_bit(IS_SUBDEV_RUN, &subdev->state);

	return ret;
}

static int is_ischain_ixc_stop(struct is_device_ischain *device,
	struct is_subdev *subdev,
	struct is_frame *frame,
	u32 *lindex,
	u32 *hindex,
	u32 *indexes)
{
	int ret = 0;
	struct param_dma_output *vdma4_output;

	mdbgd_ischain("%s\n", device, __func__);

	vdma4_output = is_itf_g_param(device, frame, PARAM_ISP_VDMA4_OUTPUT);
	vdma4_output->cmd = DMA_OUTPUT_COMMAND_DISABLE;
	*lindex |= LOWBIT_OF(PARAM_ISP_VDMA4_OUTPUT);
	*hindex |= HIGHBIT_OF(PARAM_ISP_VDMA4_OUTPUT);
	(*indexes)++;

	clear_bit(IS_SUBDEV_RUN, &subdev->state);

	return ret;
}

static int is_ischain_ixc_tag(struct is_subdev *subdev,
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

	mdbgs_ischain(4, "ISPC TAG(request %d)\n", device, node->request);

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

		otparm.x = 0;
		otparm.y = 0;
		otparm.w = isp_param->vdma4_output.width;
		otparm.h = isp_param->vdma4_output.height;

		if (IS_NULL_CROP(otcrop))
			*otcrop = otparm;

		if (!COMPARE_CROP(otcrop, &otparm) ||
			!test_bit(IS_SUBDEV_RUN, &subdev->state) ||
			test_bit(IS_SUBDEV_FORCE_SET, &leader->state)) {
			ret = is_ischain_ixc_start(device,
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

			if (!COMPARE_CROP(otcrop, &subdev->output.crop) ||
				debug_stream) {
				mdbg_pframe("ot_crop[%d, %d, %d, %d]\n", device, subdev, ldr_frame,
				otcrop->x, otcrop->y, otcrop->w, otcrop->h);

				subdev->output.crop = *otcrop;
			}

		}

		ret = is_ischain_buf_tag(device,
			subdev,
			ldr_frame,
			pixelformat,
			otcrop->w,
			otcrop->h,
			ldr_frame->ixcTargetAddress);
		if (ret) {
			mswarn("%d frame is drop", device, subdev, ldr_frame->fcount);
			node->request = 0;
		}
	} else {
		if (test_bit(IS_SUBDEV_RUN, &subdev->state)) {
			ret = is_ischain_ixc_stop(device,
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

		ldr_frame->ixcTargetAddress[0] = 0;
		ldr_frame->ixcTargetAddress[1] = 0;
		ldr_frame->ixcTargetAddress[2] = 0;
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

const struct is_subdev_ops is_subdev_ixc_ops = {
	.bypass			= NULL,
	.cfg			= is_ischain_ixc_cfg,
	.tag			= is_ischain_ixc_tag,
};
