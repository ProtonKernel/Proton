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
#include "is-subdev-ctrl.h"
#include "is-config.h"
#include "is-param.h"
#include "is-video.h"
#include "is-type.h"

void is_ischain_ixc_stripe_cfg(struct is_subdev *subdev,
		struct is_frame *ldr_frame,
		struct is_crop *incrop,
		struct is_crop *otcrop,
		struct is_frame_cfg *framecfg,
		u32 region_num)
{
	struct is_framemgr *framemgr;
	struct is_frame *frame;
	struct is_fmt *fmt = framecfg->format;
	unsigned long flags;
	u32 region_id = ldr_frame->stripe_info.region_id;
	u32 stripe_w = 0, dma_offset = 0;
	u32 scn_check_remosaic = 0;

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

		scn_check_remosaic = CHK_MODECHANGE_SCN(ldr_frame->shot->ctl.aa.captureIntent);

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
			if (scn_check_remosaic) {
				dma_offset *= fmt->bitsperpixel[0] / BITS_PER_BYTE;
				frame->stripe_info.out.h_pix_num += stripe_w;
			} else {
				dma_offset += STRIPE_MARGIN_WIDTH * ((2 * (region_id - 1)) + 1);
				dma_offset *= fmt->bitsperpixel[0] / BITS_PER_BYTE * otcrop->h;
				frame->stripe_info.out.h_pix_num += stripe_w;
				stripe_w += STRIPE_MARGIN_WIDTH;
			}
		} else {
			/* Right region */
			stripe_w = otcrop->w - frame->stripe_info.out.h_pix_num;
			dma_offset = frame->stripe_info.out.h_pix_num;
			if (scn_check_remosaic) {
				dma_offset *= fmt->bitsperpixel[0] / BITS_PER_BYTE;
				frame->stripe_info.out.h_pix_num += stripe_w;
			} else {
				dma_offset += STRIPE_MARGIN_WIDTH * ((2 * (region_id - 1)) + 1);
				dma_offset *= fmt->bitsperpixel[0] / BITS_PER_BYTE * otcrop->h;
				frame->stripe_info.out.h_pix_num += stripe_w;
			}
		}

		if (!scn_check_remosaic)
			stripe_w += STRIPE_MARGIN_WIDTH;
		otcrop->w = stripe_w;

		frame->dvaddr_buffer[0] = frame->stripe_info.region_base_addr[0] + dma_offset;
		frame->stream->stripe_h_pix_nums[region_id] = frame->stripe_info.out.h_pix_num;
		frame->stream->stripe_region_num = ldr_frame->stripe_info.region_num;

		if (frame->kvaddr_buffer[0]) {
			frame->stripe_info.region_num = ldr_frame->stripe_info.region_num;
			frame->stripe_info.kva[region_id][0] = frame->kvaddr_buffer[0] + dma_offset;
			frame->stripe_info.size[region_id][0] = stripe_w
							* fmt->bitsperpixel[0] / BITS_PER_BYTE
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
	struct is_crop *incrop,
	struct is_crop *otcrop,
	u32 *lindex,
	u32 *hindex,
	u32 *indexes)
{
	int ret = 0;
	struct param_dma_output *dma_output;
	struct is_fmt *format;
	struct is_crop incrop_cfg, otcrop_cfg;

	FIMC_BUG(!queue);
	FIMC_BUG(!queue->framecfg.format);

	format = queue->framecfg.format;
	incrop_cfg = *incrop;
	otcrop_cfg = *otcrop;

	if (IS_ENABLED(CHAIN_USE_STRIPE_PROCESSING) && frame)
		is_ischain_ixc_stripe_cfg(subdev, frame,
				&incrop_cfg, &otcrop_cfg,
				&queue->framecfg,
				frame->stripe_info.region_num);

	dma_output = is_itf_g_param(device, frame, PARAM_ISP_VDMA5_OUTPUT);
	dma_output->cmd = DMA_OUTPUT_COMMAND_ENABLE;
	dma_output->format = format->hw_format;
	dma_output->order = format->hw_order;
	dma_output->bitwidth = format->hw_bitwidth;
	dma_output->plane = format->hw_plane;
	dma_output->width = otcrop_cfg.w;
	dma_output->height = otcrop_cfg.h;
	dma_output->dma_crop_offset_x = otcrop_cfg.x;
	dma_output->dma_crop_offset_y = otcrop_cfg.y;
	dma_output->dma_crop_width = otcrop_cfg.w;
	dma_output->dma_crop_height = otcrop_cfg.h;

	dma_output->v_otf_enable = OTF_INPUT_COMMAND_DISABLE;

	*lindex |= LOWBIT_OF(PARAM_ISP_VDMA5_OUTPUT);
	*hindex |= HIGHBIT_OF(PARAM_ISP_VDMA5_OUTPUT);
	(*indexes)++;

	subdev->output.crop = *otcrop;

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
	struct param_dma_output *vdma5_output;

	mdbgd_ischain("%s\n", device, __func__);

	vdma5_output = is_itf_g_param(device, frame, PARAM_ISP_VDMA5_OUTPUT);
	vdma5_output->cmd = DMA_OUTPUT_COMMAND_DISABLE;
	*lindex |= LOWBIT_OF(PARAM_ISP_VDMA5_OUTPUT);
	*hindex |= HIGHBIT_OF(PARAM_ISP_VDMA5_OUTPUT);
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
		incrop = (struct is_crop *)node->input.cropRegion;
		otcrop = (struct is_crop *)node->output.cropRegion;

		/* Need to check : vdma1_input? otf_input? */
		inparm.x = isp_param->vdma1_input.bayer_crop_offset_x;
		inparm.y = isp_param->vdma1_input.bayer_crop_offset_y;
		inparm.w = isp_param->vdma1_input.bayer_crop_width;
		inparm.h = isp_param->vdma1_input.bayer_crop_height;

		otparm.x = 0;
		otparm.y = 0;
		otparm.w = isp_param->vdma5_output.width;
		otparm.h = isp_param->vdma5_output.height;

		if (IS_NULL_CROP(incrop))
			*incrop = inparm;

		if (!COMPARE_CROP(otcrop, &otparm) ||
			CHECK_STRIPE_CFG(&ldr_frame->stripe_info) ||
			!test_bit(IS_SUBDEV_RUN, &subdev->state) ||
			test_bit(IS_SUBDEV_FORCE_SET, &leader->state)) {
			ret = is_ischain_ixc_start(device,
				subdev,
				ldr_frame,
				queue,
				incrop,
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
