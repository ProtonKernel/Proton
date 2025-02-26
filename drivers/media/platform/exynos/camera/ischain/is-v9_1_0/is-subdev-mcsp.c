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

#include "is-core.h"
#include "is-dvfs.h"
#include "is-hw-dvfs.h"
#include "is-interface-ddk.h"

int is_ischain_mxp_stripe_cfg(struct is_subdev *subdev,
		struct is_frame *ldr_frame,
		struct is_crop *incrop,
		struct is_crop *otcrop,
		struct is_frame_cfg *framecfg,
		u32 *stripe_in_start_pos_x,
		u32 *stripe_roi_start_pos_x,
		u32 *stripe_roi_end_pos_x,
		u32 *use_out_crop)
{
	struct is_framemgr *framemgr;
	struct is_frame *frame;
	struct is_fmt *fmt = framecfg->format;
	bool x_flip = test_bit(SCALER_FLIP_COMMAND_X_MIRROR, &framecfg->flip);
	bool is_last_region = false;
	unsigned long flags;
	u32 stripe_x = 0, stripe_w = 0;
	u32 dma_offset = 0;
	int temp_stripe_x = 0, temp_stripe_w = 0;
	u32 stripe_start_x = 0, stripe_end_x = 0, stripe_roi_start_x = 0, stripe_roi_end_x = 0;
	u32 h_ratio = GET_ZOOM_RATIO(incrop->w, otcrop->w);
	u32 stripe_margin_width;
	u32 region_id = ldr_frame->stripe_info.region_id;
	/* Do not use outcrop when MCSC crop is used */
	*use_out_crop = (incrop->x == 0) ? 1 : 0;
#if IS_ENABLED(YPP_DDK_LIB_CALL)
	stripe_margin_width = STRIPE_MCSC_MARGIN_WIDTH;
#else
	stripe_margin_width = STRIPE_MARGIN_WIDTH;
#endif
	framemgr = GET_SUBDEV_FRAMEMGR(subdev);
	if (!framemgr)
		return 0;

	framemgr_e_barrier_irqs(framemgr, FMGR_IDX_24, flags);

	frame = peek_frame(framemgr, ldr_frame->state);
	if (frame) {
		/* Input crop configuration */
		if (!region_id) {
			/* Left region w/o margin */
			stripe_x = incrop->x;
			temp_stripe_w = ldr_frame->stripe_info.in.h_pix_num[region_id] - stripe_x;

			stripe_w = temp_stripe_w > 0 ? temp_stripe_w: 0;

			frame->stripe_info.in.h_pix_ratio = stripe_w * STRIPE_RATIO_PRECISION / incrop->w;
			frame->stripe_info.in.h_pix_num[region_id] = stripe_w;
			if (*use_out_crop) {
				stripe_start_x = 0;
				stripe_end_x = ldr_frame->stripe_info.in.h_pix_num[region_id] + stripe_margin_width;
				stripe_roi_start_x = incrop->x;
				stripe_roi_end_x = ldr_frame->stripe_info.in.h_pix_num[region_id];

				stripe_w += stripe_margin_width;
			}
		} else if (region_id < ldr_frame->stripe_info.region_num - 1) {
			/* Middle region w/o margin */
			if (!*use_out_crop)
				stripe_x = stripe_margin_width;
			temp_stripe_w = ldr_frame->stripe_info.in.h_pix_num[region_id] - frame->stripe_info.in.h_pix_num[region_id - 1] - incrop->x;

			/* Stripe x when crop offset x start in middle region */
			if (!frame->stripe_info.in.h_pix_num[region_id - 1] && incrop->x < ldr_frame->stripe_info.in.h_pix_num[region_id]) {
				temp_stripe_x = incrop->x - ldr_frame->stripe_info.in.h_pix_num[region_id - 1];
				stripe_roi_start_x = incrop->x;
			} else {
				stripe_roi_start_x = ldr_frame->stripe_info.in.h_pix_num[region_id - 1];
			}
			/* Stripe width when crop region end in middle region */
			if (incrop->x + incrop->w < ldr_frame->stripe_info.in.h_pix_num[region_id]) {
				temp_stripe_w = incrop->w - frame->stripe_info.in.h_pix_num[region_id - 1];
				is_last_region = true;
				stripe_roi_end_x = incrop->x + incrop->w;
			} else {
				stripe_roi_end_x = ldr_frame->stripe_info.in.h_pix_num[region_id];
			}

			stripe_w = temp_stripe_w > 0 ? temp_stripe_w: 0;
			stripe_x = stripe_x + temp_stripe_x;

			frame->stripe_info.in.h_pix_ratio = stripe_w * STRIPE_RATIO_PRECISION / incrop->w;
			frame->stripe_info.in.h_pix_num[region_id] = frame->stripe_info.in.h_pix_num[region_id - 1] + stripe_w;
			if (*use_out_crop) {
				stripe_start_x = stripe_roi_start_x - stripe_margin_width;
				stripe_end_x = stripe_roi_end_x + stripe_margin_width;
				stripe_w += stripe_margin_width * 2;
			}
		} else {
			/* Right region w/o margin */
			if (!*use_out_crop)
				stripe_x = stripe_margin_width;
			temp_stripe_w = incrop->w - frame->stripe_info.in.h_pix_num[region_id - 1];
			stripe_w = temp_stripe_w > 0 ? temp_stripe_w: 0;
			frame->stripe_info.in.h_pix_num[region_id] = frame->stripe_info.in.h_pix_num[region_id - 1] + stripe_w;
			if (*use_out_crop) {
				stripe_roi_start_x = ldr_frame->stripe_info.in.h_pix_num[region_id - 1];
				stripe_roi_end_x = incrop->x + incrop->w;
				stripe_start_x = stripe_roi_start_x - stripe_margin_width;
				stripe_end_x = ldr_frame->stripe_info.in.h_pix_num[region_id];

				stripe_w += stripe_margin_width;
			}
		}
		/* Use only when out crop */
		*stripe_in_start_pos_x  = stripe_start_x;
		*stripe_roi_start_pos_x = stripe_roi_start_x;
		*stripe_roi_end_pos_x = stripe_roi_end_x;

		incrop->x = stripe_x;
		incrop->w = stripe_w;

		/* Output crop & WDMA offset configuration */
		if (!region_id) {
			/* Left region */
			/* Stripe width should be 4 align because of 4 ppc */
			if (*use_out_crop) {
				stripe_w = ALIGN_DOWN(GET_SCALED_SIZE(stripe_w, h_ratio), MCSC_WIDTH_ALIGN);
			} else {
				stripe_w = ALIGN(otcrop->w * frame->stripe_info.in.h_pix_ratio / STRIPE_RATIO_PRECISION, 4);
				/* Add horizontal DMA offset */
				if (x_flip)
					dma_offset = (otcrop->w - stripe_w) * fmt->bitsperpixel[0] / BITS_PER_BYTE;
			}

			frame->stripe_info.out.h_pix_num[region_id] = stripe_w;
			frame->stripe_info.region_base_addr[0] = frame->dvaddr_buffer[0];
			frame->stripe_info.region_base_addr[1] = frame->dvaddr_buffer[1];
		} else if (region_id < ldr_frame->stripe_info.region_num - 1) {
			/* Middle region */
			if (*use_out_crop) {
				stripe_w = ALIGN_DOWN(GET_SCALED_SIZE(stripe_w, h_ratio), MCSC_WIDTH_ALIGN);
			} else {
				if (is_last_region)
					stripe_w = otcrop->w - frame->stripe_info.out.h_pix_num[region_id - 1];
				else
					stripe_w = ALIGN(otcrop->w * frame->stripe_info.in.h_pix_ratio / STRIPE_RATIO_PRECISION, 4);

				/* Add horizontal DMA offset */
				if (x_flip)
					dma_offset = (otcrop->w - frame->stripe_info.out.h_pix_num[region_id - 1] - stripe_w) * fmt->bitsperpixel[0] / BITS_PER_BYTE;
				else
					dma_offset = frame->stripe_info.out.h_pix_num[region_id - 1] * fmt->bitsperpixel[0] / BITS_PER_BYTE;
			}
			frame->stripe_info.out.h_pix_num[region_id] = frame->stripe_info.out.h_pix_num[region_id - 1] + stripe_w;
		} else {
			/* Right region */
			if (*use_out_crop) {
				stripe_w = ALIGN_DOWN(GET_SCALED_SIZE(stripe_w, h_ratio), MCSC_WIDTH_ALIGN);
			} else {
				stripe_w = otcrop->w - frame->stripe_info.out.h_pix_num[region_id - 1];

				/* Add horizontal DMA offset */
				if (x_flip)
					dma_offset = 0;
				else
					dma_offset = frame->stripe_info.out.h_pix_num[region_id - 1] * fmt->bitsperpixel[0] / BITS_PER_BYTE;
			}
			frame->stripe_info.out.h_pix_num[region_id] = frame->stripe_info.out.h_pix_num[region_id - 1] + stripe_w;
		}

		otcrop->w = stripe_w;

		if (temp_stripe_w <= 0) {
			mdbg_pframe("Skip current stripe[#%d] region because stripe_width is too small(%d)\n",
				subdev, subdev, ldr_frame,
				region_id, stripe_w);
			framemgr_x_barrier_irqr(framemgr, FMGR_IDX_24, flags);
			return -EAGAIN;
		}

		frame->dvaddr_buffer[0] = frame->stripe_info.region_base_addr[0] + dma_offset;
		/* Calculate chroma base address for NV21M */
		frame->dvaddr_buffer[1] = frame->stripe_info.region_base_addr[1] + dma_offset;

		msrdbgs(3, "stripe_in_crop[%d][%d, %d, %d, %d]\n", subdev, subdev, ldr_frame,
				region_id,
				incrop->x, incrop->y, incrop->w, incrop->h);
		msrdbgs(3, "stripe_ot_crop[%d][%d, %d, %d, %d] offset %x use_outcrop %d\n", subdev, subdev, ldr_frame,
				region_id,
				otcrop->x, otcrop->y, otcrop->w, otcrop->h, dma_offset, *use_out_crop);
	}

	framemgr_x_barrier_irqr(framemgr, FMGR_IDX_24, flags);
	return 0;
}

static int is_ischain_mxp_adjust_crop(struct is_device_ischain *device,
	u32 input_crop_w, u32 input_crop_h,
	u32 *output_crop_w, u32 *output_crop_h);

static int is_ischain_mxp_cfg(struct is_subdev *subdev,
	void *device_data,
	struct is_frame *frame,
	struct is_crop *incrop,
	struct is_crop *otcrop,
	u32 *lindex,
	u32 *hindex,
	u32 *indexes)
{
	int ret = 0;
	struct is_queue *queue;
	struct is_fmt *format;
	struct param_mcs_output *mcs_output;
	struct is_device_ischain *device;
	u32 width, height;
	u32 crange;

	device = (struct is_device_ischain *)device_data;

	FIMC_BUG(!device);
	FIMC_BUG(!incrop);

	queue = GET_SUBDEV_QUEUE(subdev);
	if (!queue) {
		merr("queue is NULL", device);
		ret = -EINVAL;
		goto p_err;
	}

	format = queue->framecfg.format;
	if (!format) {
		mserr("format is NULL", device, subdev);
		ret = -EINVAL;
		goto p_err;
	}

	width = queue->framecfg.width;
	height = queue->framecfg.height;
	is_ischain_mxp_adjust_crop(device, incrop->w, incrop->h, &width, &height);

	if (queue->framecfg.quantization == V4L2_QUANTIZATION_FULL_RANGE) {
		crange = SCALER_OUTPUT_YUV_RANGE_FULL;
		msinfo("CRange:W\n", device, subdev);
	} else {
		crange = SCALER_OUTPUT_YUV_RANGE_NARROW;
		msinfo("CRange:N\n", device, subdev);
	}

	mcs_output = is_itf_g_param(device, frame, subdev->param_dma_ot);

	mcs_output->otf_cmd = OTF_OUTPUT_COMMAND_ENABLE;
	mcs_output->otf_format = OTF_OUTPUT_FORMAT_YUV422;
	mcs_output->otf_bitwidth = OTF_OUTPUT_BIT_WIDTH_8BIT;
	mcs_output->otf_order = OTF_OUTPUT_ORDER_BAYER_GR_BG;

	mcs_output->dma_bitwidth = format->hw_bitwidth;
	mcs_output->dma_format = format->hw_format;
	mcs_output->dma_order = format->hw_order;
	mcs_output->plane = format->hw_plane;
	mcs_output->bitsperpixel = format->bitsperpixel[0] | format->bitsperpixel[1] << 8 | format->bitsperpixel[2] << 16;

	mcs_output->crop_offset_x = incrop->x;
	mcs_output->crop_offset_y = incrop->y;
	mcs_output->crop_width = incrop->w;
	mcs_output->crop_height = incrop->h;

	mcs_output->offset_x = otcrop->x; /* per frame */
	mcs_output->offset_y = otcrop->y; /* per frame */
	mcs_output->width = width;
	mcs_output->height = height;

	mcs_output->dma_stride_y = max(width * format->bitsperpixel[0] / BITS_PER_BYTE,
					queue->framecfg.bytesperline[0]);
	mcs_output->dma_stride_c = max(width * format->bitsperpixel[1] / BITS_PER_BYTE,
					queue->framecfg.bytesperline[1]);
	if (!IS_ALIGNED(mcs_output->dma_stride_y, 16) || !IS_ALIGNED(mcs_output->dma_stride_c, 16))
		mwarn("WDMA stride(y:%#x uv:%#x) should be %d aligned",
			device, mcs_output->dma_stride_y, mcs_output->dma_stride_c, 16);

	mcs_output->yuv_range = crange;
	mcs_output->flip = (u32)queue->framecfg.flip >> 1; /* Caution: change from bitwise to enum */

#ifdef ENABLE_HWFC
	if (test_bit(IS_ISCHAIN_REPROCESSING, &device->state))
		mcs_output->hwfc = 1; /* TODO: enum */
	else
		mcs_output->hwfc = 0; /* TODO: enum */
#endif

#ifdef SOC_VRA
	if (device->group_mcs.junction == subdev) {
		struct param_otf_input *otf_input;
		otf_input = is_itf_g_param(device, frame, PARAM_FD_OTF_INPUT);
		otf_input->width = width;
		otf_input->height = height;
		*lindex |= LOWBIT_OF(PARAM_FD_OTF_INPUT);
		*hindex |= HIGHBIT_OF(PARAM_FD_OTF_INPUT);
		(*indexes)++;
	}
#endif

	*lindex |= LOWBIT_OF(subdev->param_dma_ot);
	*hindex |= HIGHBIT_OF(subdev->param_dma_ot);
	(*indexes)++;


p_err:
	return ret;
}

#define MXP_RATIO_UP		(MCSC_POLY_RATIO_UP)
#define MXP_RATIO_DOWN		(256)
void is_ischain_mxp_check_align(struct is_device_ischain *device,
	struct is_crop *incrop,
	struct is_crop *otcrop)
{
	if (incrop->x % MCSC_OFFSET_ALIGN) {
		mwarn("Input crop offset x should be multiple of %d, change size(%d -> %d)",
			device, MCSC_OFFSET_ALIGN, incrop->x, ALIGN_DOWN(incrop->x, MCSC_OFFSET_ALIGN));
		incrop->x = ALIGN_DOWN(incrop->x, MCSC_OFFSET_ALIGN);
	}
	if (incrop->w % MCSC_WIDTH_ALIGN) {
		mwarn("Input crop width should be multiple of %d, change size(%d -> %d)",
			device, MCSC_WIDTH_ALIGN, incrop->w, ALIGN_DOWN(incrop->w, MCSC_WIDTH_ALIGN));
		incrop->w = ALIGN_DOWN(incrop->w, MCSC_WIDTH_ALIGN);
	}
	if (incrop->h % MCSC_HEIGHT_ALIGN) {
		mwarn("Input crop height should be multiple of %d, change size(%d -> %d)",
			device, MCSC_HEIGHT_ALIGN, incrop->h, ALIGN_DOWN(incrop->h, MCSC_HEIGHT_ALIGN));
		incrop->h = ALIGN_DOWN(incrop->h, MCSC_HEIGHT_ALIGN);
	}

	if (otcrop->x % MCSC_OFFSET_ALIGN) {
		mwarn("Output crop offset x should be multiple of %d, change size(%d -> %d)",
			device, MCSC_OFFSET_ALIGN, otcrop->x, ALIGN_DOWN(otcrop->x, MCSC_OFFSET_ALIGN));
		otcrop->x = ALIGN_DOWN(otcrop->x, MCSC_OFFSET_ALIGN);
	}
	if (otcrop->w % MCSC_WIDTH_ALIGN) {
		mwarn("Output crop width should be multiple of %d, change size(%d -> %d)",
			device, MCSC_WIDTH_ALIGN, otcrop->w, ALIGN_DOWN(otcrop->w, MCSC_WIDTH_ALIGN));
		otcrop->w = ALIGN_DOWN(otcrop->w, MCSC_WIDTH_ALIGN);
	}
	if (otcrop->h % MCSC_HEIGHT_ALIGN) {
		mwarn("Output crop height should be multiple of %d, change size(%d -> %d)",
			device, MCSC_HEIGHT_ALIGN, otcrop->h, ALIGN_DOWN(otcrop->h, MCSC_HEIGHT_ALIGN));
		otcrop->h = ALIGN_DOWN(otcrop->h, MCSC_HEIGHT_ALIGN);
	}
}

static int is_ischain_mxp_adjust_crop(struct is_device_ischain *device,
	u32 input_crop_w, u32 input_crop_h,
	u32 *output_crop_w, u32 *output_crop_h)
{
	int changed = 0;

	if (*output_crop_w > input_crop_w * MXP_RATIO_UP) {
		mwarn("Cannot be scaled up beyond %d times(%d -> %d)",
			device, MXP_RATIO_UP, input_crop_w, *output_crop_w);
		*output_crop_w = input_crop_w * MXP_RATIO_UP;
		changed |= 0x01;
	}

	if (*output_crop_h > input_crop_h * MXP_RATIO_UP) {
		mwarn("Cannot be scaled up beyond %d times(%d -> %d)",
			device, MXP_RATIO_UP, input_crop_h, *output_crop_h);
		*output_crop_h = input_crop_h * MXP_RATIO_UP;
		changed |= 0x02;
	}

	if (*output_crop_w < (input_crop_w + (MXP_RATIO_DOWN - 1)) / MXP_RATIO_DOWN) {
		mwarn("Cannot be scaled down beyond 1/%d times(%d -> %d)",
			device, MXP_RATIO_DOWN, input_crop_w, *output_crop_w);
		*output_crop_w = ALIGN((input_crop_w + (MXP_RATIO_DOWN - 1)) / MXP_RATIO_DOWN, MCSC_WIDTH_ALIGN);
		changed |= 0x10;
	}

	if (*output_crop_h < (input_crop_h + (MXP_RATIO_DOWN - 1))/ MXP_RATIO_DOWN) {
		mwarn("Cannot be scaled down beyond 1/%d times(%d -> %d)",
			device, MXP_RATIO_DOWN, input_crop_h, *output_crop_h);
		*output_crop_h = ALIGN((input_crop_h + (MXP_RATIO_DOWN - 1))/ MXP_RATIO_DOWN, MCSC_HEIGHT_ALIGN);
		changed |= 0x20;
	}

	return changed;
}

static int is_ischain_mxp_compare_size(struct is_device_ischain *device,
	struct mcs_param *mcs_param,
	struct is_crop *incrop)
{
	int changed = 0;

	if (mcs_param->input.otf_cmd == OTF_INPUT_COMMAND_ENABLE) {
		if (incrop->x + incrop->w > mcs_param->input.width) {
			mwarn("Out of crop width region(%d < %d)",
				device, mcs_param->input.width, incrop->x + incrop->w);
			incrop->x = 0;
			incrop->w = mcs_param->input.width;
			changed |= 0x01;
		}

		if (incrop->y + incrop->h > mcs_param->input.height) {
			mwarn("Out of crop height region(%d < %d)",
				device, mcs_param->input.height, incrop->y + incrop->h);
			incrop->y = 0;
			incrop->h = mcs_param->input.height;
			changed |= 0x02;
		}
	} else {
		if (incrop->x + incrop->w > mcs_param->input.dma_crop_width) {
			mwarn("Out of crop width region(%d < %d)",
				device, mcs_param->input.dma_crop_width, incrop->x + incrop->w);
			incrop->x = 0;
			incrop->w = mcs_param->input.dma_crop_width;
			changed |= 0x01;
		}

		if (incrop->y + incrop->h > mcs_param->input.dma_crop_height) {
			mwarn("Out of crop height region(%d < %d)",
				device, mcs_param->input.dma_crop_height, incrop->y + incrop->h);
			incrop->y = 0;
			incrop->h = mcs_param->input.dma_crop_height;
			changed |= 0x02;
		}
	}

	return changed;
}

void is_ischain_mxp_check_hf_size(struct is_device_ischain *device,
	struct is_crop *incrop,
	struct is_crop *otcrop)
{
	if (incrop->w > otcrop->w) {
		mwarn("High frequency scaler can't scale down(%d > %d), change out width(%d->%d)",
			device, incrop->w, otcrop->w, otcrop->w, incrop->w);
		otcrop->w = incrop->w;
	}
	if (incrop->h != otcrop->h) {
		mwarn("High frequency scaler support only horizontal scaling, change out height(%d->%d)",
			device, otcrop->h, incrop->h);
		otcrop->h = incrop->h;
	}
}

#ifdef ENABLE_LOGICAL_VIDEO_NODE
int __mxp_out_stripe_cfg(struct is_frame *ldr_frame,
		struct camera2_node *node,
		struct is_crop *incrop,
		struct is_crop *otcrop,
		u32 bitwidth,
		int index,
		u32 *stripe_in_start_pos_x,
		u32 *stripe_roi_start_pos_x,
		u32 *stripe_roi_end_pos_x,
		u32 *use_out_crop)
{
	struct fimc_is_stripe_info *stripe_info;
	ulong flip = (ulong)node->flip;
	bool x_flip;
	bool is_last_region = false;
	u32 stripe_x = 0, stripe_w = 0;
	u32 dma_offset = 0;
	int temp_stripe_x = 0, temp_stripe_w = 0;
	u32 stripe_start_x = 0, stripe_end_x = 0, stripe_roi_start_x = 0, stripe_roi_end_x = 0;
	u32 h_ratio = GET_ZOOM_RATIO(incrop->w, otcrop->w);
	u32 stripe_margin_width;
	u32 region_id = ldr_frame->stripe_info.region_id;
	/* Do not use outcrop when MCSC crop is used */
	*use_out_crop = (incrop->x == 0) ? 1 : 0;

#if IS_ENABLED(YPP_DDK_LIB_CALL)
	stripe_margin_width = STRIPE_MCSC_MARGIN_WIDTH;
#else
	stripe_margin_width = STRIPE_MARGIN_WIDTH;
#endif
	x_flip = test_bit(SCALER_FLIP_COMMAND_X_MIRROR, &flip);
	stripe_info = &ldr_frame->cap_node.sframe[index].stripe_info;

	/* Input crop configuration */
	if (!region_id) {
		/* Left region w/o margin */
		stripe_x = incrop->x;
		temp_stripe_w = ldr_frame->stripe_info.out.h_pix_num[region_id] - stripe_x;

		stripe_w = temp_stripe_w > 0 ? temp_stripe_w: 0;

		stripe_info->in.h_pix_ratio = stripe_w * STRIPE_RATIO_PRECISION / incrop->w;
		stripe_info->in.h_pix_num[region_id] = stripe_w;
		if (*use_out_crop) {
			stripe_start_x = 0;
			stripe_end_x = ldr_frame->stripe_info.out.h_pix_num[region_id] + stripe_margin_width;
			stripe_roi_start_x = incrop->x;
			stripe_roi_end_x = ldr_frame->stripe_info.out.h_pix_num[region_id];

			stripe_w += stripe_margin_width;
		}
	} else if (region_id < ldr_frame->stripe_info.region_num - 1) {
		/* Middle region w/o margin */
		if (!*use_out_crop)
			stripe_x = stripe_margin_width;
		temp_stripe_w = ldr_frame->stripe_info.out.h_pix_num[region_id] - stripe_info->in.h_pix_num[region_id - 1] - incrop->x;

		/* Stripe x when crop offset x start in middle region */
		if (!stripe_info->in.h_pix_num[region_id - 1] && incrop->x < ldr_frame->stripe_info.out.h_pix_num[region_id]){
			temp_stripe_x = incrop->x - ldr_frame->stripe_info.out.h_pix_num[region_id - 1];
			stripe_roi_start_x = incrop->x;
		} else {
			stripe_roi_start_x = ldr_frame->stripe_info.out.h_pix_num[region_id - 1];
		}
		/* Stripe width when crop region end in middle region */
		if (incrop->x + incrop->w < ldr_frame->stripe_info.out.h_pix_num[region_id]) {
			temp_stripe_w = incrop->w - stripe_info->in.h_pix_num[region_id - 1];
			is_last_region = true;
			stripe_roi_end_x = incrop->x + incrop->w;
		} else {
			stripe_roi_end_x = ldr_frame->stripe_info.out.h_pix_num[region_id];
		}

		stripe_w = temp_stripe_w > 0 ? temp_stripe_w: 0;
		stripe_x = stripe_x + temp_stripe_x;

		stripe_info->in.h_pix_ratio = stripe_w * STRIPE_RATIO_PRECISION / incrop->w;
		stripe_info->in.h_pix_num[region_id] = stripe_info->in.h_pix_num[region_id - 1] + stripe_w;
		if (*use_out_crop) {
			stripe_start_x = stripe_roi_start_x - stripe_margin_width;
			stripe_end_x = stripe_roi_end_x + stripe_margin_width;
			stripe_w += stripe_margin_width * 2;
		}
	} else {
		/* Right region w/o margin */
		if (!*use_out_crop)
			stripe_x = stripe_margin_width;
		temp_stripe_w = incrop->w - stripe_info->in.h_pix_num[region_id - 1];
		stripe_w = temp_stripe_w > 0 ? temp_stripe_w: 0;
		stripe_info->in.h_pix_num[region_id] = stripe_info->in.h_pix_num[region_id - 1] + stripe_w;
		if (*use_out_crop) {
			stripe_roi_start_x = ldr_frame->stripe_info.out.h_pix_num[region_id - 1];
			stripe_roi_end_x = incrop->x + incrop->w;
			stripe_start_x = stripe_roi_start_x - stripe_margin_width;
			stripe_end_x = ldr_frame->stripe_info.out.h_pix_num[region_id];

			stripe_w += stripe_margin_width;
		}
	}
	/* Use only when out crop */
	*stripe_in_start_pos_x  = stripe_start_x;
	*stripe_roi_start_pos_x = stripe_roi_start_x;
	*stripe_roi_end_pos_x = stripe_roi_end_x;

	incrop->x = stripe_x;
	incrop->w = stripe_w;

	/* Output crop & WDMA offset configuration */
	if (!region_id) {
		/* Left region */
		/* Stripe width should be 4 align because of 4 ppc */
		if (*use_out_crop) {
			stripe_w = ALIGN_DOWN(GET_SCALED_SIZE(stripe_w, h_ratio), MCSC_WIDTH_ALIGN);
		} else {
			stripe_w = ALIGN(otcrop->w * stripe_info->in.h_pix_ratio / STRIPE_RATIO_PRECISION, 4);
			/* Add horizontal DMA offset */
			if (x_flip)
				dma_offset = (otcrop->w - stripe_w) * bitwidth / BITS_PER_BYTE;
		}

		stripe_info->out.h_pix_num[region_id] = stripe_w;
		stripe_info->region_base_addr[0] = ldr_frame->cap_node.sframe[index].dva[0];
		stripe_info->region_base_addr[1] = ldr_frame->cap_node.sframe[index].dva[1];
	} else if (region_id < ldr_frame->stripe_info.region_num - 1) {
		/* Middle region */
		if (*use_out_crop) {
			stripe_w = ALIGN_DOWN(GET_SCALED_SIZE(stripe_w, h_ratio), MCSC_WIDTH_ALIGN);
		} else {
			if (is_last_region)
				stripe_w = otcrop->w - stripe_info->out.h_pix_num[region_id - 1];
			else
				stripe_w = ALIGN(otcrop->w * stripe_info->in.h_pix_ratio / STRIPE_RATIO_PRECISION, 4);

			/* Add horizontal DMA offset */
			if (x_flip)
				dma_offset = (otcrop->w - stripe_info->out.h_pix_num[region_id - 1] - stripe_w) * bitwidth / BITS_PER_BYTE;
			else
				dma_offset = stripe_info->out.h_pix_num[region_id - 1] * bitwidth / BITS_PER_BYTE;
		}
		stripe_info->out.h_pix_num[region_id] = stripe_info->out.h_pix_num[region_id - 1] + stripe_w;
	} else {
		/* Right region */
		if (*use_out_crop) {
			stripe_w = ALIGN_DOWN(GET_SCALED_SIZE(stripe_w, h_ratio), MCSC_WIDTH_ALIGN);
		} else {
			stripe_w = otcrop->w - stripe_info->out.h_pix_num[region_id - 1];
			/* Add horizontal DMA offset */
			if (x_flip)
				dma_offset = 0;
			else
				dma_offset = stripe_info->out.h_pix_num[region_id - 1] * bitwidth / BITS_PER_BYTE;
		}
		stripe_info->out.h_pix_num[region_id] = stripe_info->out.h_pix_num[region_id - 1] + stripe_w;
	}

	otcrop->w = stripe_w;

	if (temp_stripe_w <= 0) {
		mlvinfo("[F%d] Skip current stripe[#%d] region because stripe_width is too small(%d)\n", ldr_frame,
			node->vid, ldr_frame->fcount,
			region_id, temp_stripe_w);
		return -EAGAIN;
	}

	ldr_frame->cap_node.sframe[index].dva[0] = stripe_info->region_base_addr[0] + dma_offset;
	/* Calculate chroma base address for NV21M */
	ldr_frame->cap_node.sframe[index].dva[1] = stripe_info->region_base_addr[1] + dma_offset;

	mlvdbgs(3, "[F%d] stripe_in_crop[%d][%d, %d, %d, %d]\n", ldr_frame, node->vid,
			ldr_frame->fcount, region_id,
			incrop->x, incrop->y, incrop->w, incrop->h);
	mlvdbgs(3, "[F%d] stripe_ot_crop[%d][%d, %d, %d, %d] offset %x use_outcrop %d\n", ldr_frame, node->vid,
			ldr_frame->fcount, region_id,
			otcrop->x, otcrop->y, otcrop->w, otcrop->h, dma_offset, *use_out_crop);

	return 0;
}

int __mcsc_dma_out_cfg(struct is_device_ischain *device,
	struct is_frame *ldr_frame,
	struct camera2_node *node,
	u32 pindex,
	u32 *lindex,
	u32 *hindex,
	u32 *indexes,
	int index)
{
	int ret = 0;
	struct param_mcs_output *mcs_output;
	struct is_crop *incrop, *otcrop;
	struct is_fmt *fmt;
	u32 flip;
	u32 crange;
	u32 stripe_in_start_pos_x = 0;
	u32 stripe_roi_start_pos_x = 0, stripe_roi_end_pos_x = 0;
	u32 use_out_crop = 0;

	FIMC_BUG(!device);
	FIMC_BUG(!ldr_frame);
	FIMC_BUG(!node);

	incrop = (struct is_crop *)node->input.cropRegion;
	/* TODO: prevent null crop info */

	otcrop = (struct is_crop *)node->output.cropRegion;
	/* TODO: prevent null crop info */

	/* Check MCSC in/out crop size alignment */
	is_ischain_mxp_check_align(device, incrop, otcrop);

	mcs_output = is_itf_g_param(device, ldr_frame, pindex);
	if (mcs_output->dma_cmd != (node->request & BIT(0))) {
		mlvinfo("[F%d] WDMA enable: %d -> %d\n", device,
			node->vid, ldr_frame->fcount,
			mcs_output->dma_cmd, node->request);

		/* HF on/off */
		if (pindex == PARAM_MCS_OUTPUT5) {
			struct is_resourcemgr *resourcemgr;
			struct is_dvfs_ctrl *dvfs_ctrl;
			int scenario_id  = 0;
			struct param_mcs_output *mcs_output_region;

			mcs_output_region = is_itf_g_param(device, NULL, pindex);
			mcs_output_region->dma_cmd = node->request & BIT(0);

			resourcemgr = device->resourcemgr;
			dvfs_ctrl = &resourcemgr->dvfs_ctrl;

			mutex_lock(&dvfs_ctrl->lock);

			/* try to find dynamic scenario to apply */
			scenario_id = is_dvfs_sel_static(device);
			if (scenario_id >= 0) {
				struct is_dvfs_scenario_ctrl *static_ctrl = dvfs_ctrl->static_ctrl;

				minfo("[ISC:D] tbl[%d] HF static scenario(%d)-[%s]\n", device,
					dvfs_ctrl->dvfs_table_idx, scenario_id,
					static_ctrl->scenarios[static_ctrl->cur_scenario_idx].scenario_nm);
				is_set_dvfs((struct is_core *)device->interface->core, device, scenario_id);
			}

			mutex_unlock(&dvfs_ctrl->lock);
		}
	}
	mcs_output->otf_cmd = OTF_OUTPUT_COMMAND_DISABLE;
	mcs_output->otf_format = OTF_OUTPUT_FORMAT_YUV422;
	mcs_output->otf_bitwidth = OTF_OUTPUT_BIT_WIDTH_8BIT;
	mcs_output->otf_order = OTF_OUTPUT_ORDER_BAYER_GR_BG;

	mcs_output->dma_cmd = DMA_OUTPUT_COMMAND_DISABLE;
	mcs_output->width = otcrop->w;
	mcs_output->height = otcrop->h;
	*lindex |= LOWBIT_OF(pindex);
	*hindex |= HIGHBIT_OF(pindex);
	(*indexes)++;

	if (!node->request)
		return ret;

	/* if output 5, port for high frequency */
	if ((pindex - PARAM_MCS_OUTPUT0) == MCSC_INPUT_HF) {
		is_ischain_mxp_check_hf_size(device, incrop, otcrop);
	} else {
		/*
		* For compatibility with post processing IP,
		* width & stride should be aligned by 16.
		* Just check it because it is not MCSC HW spec
		*/
		if (otcrop->w % 16)
			mwarn("Output crop width(%d) should be multiple of %d", device, otcrop->w, 16);
	}

	fmt = is_find_format(node->pixelformat, node->flags);
	if (!fmt) {
		mlverr("pixelformat(%c%c%c%c) is not found", device, node->vid,
			(char)((node->pixelformat >> 0) & 0xFF),
			(char)((node->pixelformat >> 8) & 0xFF),
			(char)((node->pixelformat >> 16) & 0xFF),
			(char)((node->pixelformat >> 24) & 0xFF));
		return -EINVAL;
	}
	flip = mcs_output->flip;
	if ((pindex >= PARAM_MCS_OUTPUT0 && pindex < PARAM_MCS_OUTPUT5) &&
		ldr_frame->shot_ext->mcsc_flip[pindex - PARAM_MCS_OUTPUT0] != mcs_output->flip) {
		flip = ldr_frame->shot_ext->mcsc_flip[pindex - PARAM_MCS_OUTPUT0];
		node->flip = flip << 1;
		mlvinfo("[F%d]flip is changed(%d->%d)\n", device,
				node->vid, ldr_frame->fcount,
				mcs_output->flip, flip);
	}

	if (IS_ENABLED(CHAIN_USE_STRIPE_PROCESSING) && ldr_frame->stripe_info.region_num
		&& (pindex - PARAM_MCS_OUTPUT0) != MCSC_INPUT_HF) {
		use_out_crop = 1;
	} else {
		if ((mcs_output->crop_width != incrop->w) ||
			(mcs_output->crop_height != incrop->h) ||
			(mcs_output->width != otcrop->w) ||
			(mcs_output->height != otcrop->h))
			mlvinfo("[F%d]WDMA incrop[%d, %d, %d, %d] otcrop[%d, %d, %d, %d]\n",
				device, node->vid, ldr_frame->fcount,
				incrop->x, incrop->y, incrop->w, incrop->h,
				otcrop->x, otcrop->y, otcrop->w, otcrop->h);
	}
	is_ischain_mxp_adjust_crop(device, incrop->w, incrop->h, &otcrop->w, &otcrop->h);

	if (node->quantization == V4L2_QUANTIZATION_FULL_RANGE)
		crange = SCALER_OUTPUT_YUV_RANGE_FULL;
	else
		crange = SCALER_OUTPUT_YUV_RANGE_NARROW;

	if ((mcs_output->yuv_range != crange) &&
		!IS_VIDEO_HDR_SCENARIO(device->setfile & IS_SETFILE_MASK))
		mlvinfo("[F%d] CRange: %d -> %d\n", device, node->vid,
			ldr_frame->fcount, mcs_output->yuv_range, crange);

	if (mcs_output->dma_format != fmt->hw_format)
		mlvinfo("[F%d]WDMA format: %d -> %d\n", device,
			node->vid, ldr_frame->fcount,
			mcs_output->dma_format, fmt->hw_format);
	if (mcs_output->dma_bitwidth != fmt->hw_bitwidth)
		mlvinfo("[F%d]WDMA bitwidth: %d -> %d\n", device,
			node->vid, ldr_frame->fcount,
			mcs_output->dma_bitwidth, fmt->hw_bitwidth);

	mcs_output->dma_cmd = DMA_OUTPUT_COMMAND_ENABLE;
	mcs_output->dma_bitwidth = fmt->hw_bitwidth;
	mcs_output->dma_format = fmt->hw_format;
	mcs_output->dma_order = fmt->hw_order;
	mcs_output->plane = fmt->hw_plane;
	mcs_output->bitsperpixel = fmt->bitsperpixel[0] | fmt->bitsperpixel[1] << 8 | fmt->bitsperpixel[2] << 16;

	mcs_output->crop_offset_x = incrop->x;
	mcs_output->crop_offset_y = incrop->y;
	mcs_output->crop_width = incrop->w;
	mcs_output->crop_height = incrop->h;

	mcs_output->width = otcrop->w;
	mcs_output->height = otcrop->h;

	mcs_output->full_input_width = incrop->w; /* for stripe */
	mcs_output->full_output_width = otcrop->w; /* for stripe */

	mcs_output->stripe_in_start_pos_x = stripe_in_start_pos_x; /* for stripe */
	mcs_output->stripe_roi_start_pos_x = stripe_roi_start_pos_x; /* for stripe */
	mcs_output->stripe_roi_end_pos_x = stripe_roi_end_pos_x; /* for stripe */
	mcs_output->cmd = (use_out_crop & BIT(MCSC_OUT_CROP)); /* for stripe */
	mcs_output->cmd |= (node->request & BIT(MCSC_CROP_TYPE)); /* 0: ceter crop, 1: freeform crop */

	mcs_output->dma_stride_y = max(node->bytesperline[0], (otcrop->w * fmt->bitsperpixel[0]) / BITS_PER_BYTE);
	mcs_output->dma_stride_c = max(node->bytesperline[1], (otcrop->w * fmt->bitsperpixel[1]) / BITS_PER_BYTE);

	if ((pindex - PARAM_MCS_OUTPUT0) == MCSC_INPUT_HF && !node->bytesperline[0]) {
		mcs_output->dma_stride_y = ALIGN(mcs_output->dma_stride_y, 16);
		mcs_output->dma_stride_c = ALIGN(mcs_output->dma_stride_c, 16);
	} else {
		if (!IS_ALIGNED(mcs_output->dma_stride_y, 16) ||
		!IS_ALIGNED(mcs_output->dma_stride_c, 16))
		mwarn("WDMA stride(y:%#x uv:%#x) should be %d aligned",
			device, mcs_output->dma_stride_y, mcs_output->dma_stride_c, 16);
	}

	mcs_output->yuv_range = crange;
	mcs_output->flip = flip;

#ifdef ENABLE_HWFC
	if (test_bit(IS_ISCHAIN_REPROCESSING, &device->state))
		mcs_output->hwfc = 1; /* TODO: enum */
	else
		mcs_output->hwfc = 0; /* TODO: enum */
#endif

	return ret;
}
#endif

static int is_ischain_mxp_start(struct is_device_ischain *device,
	struct camera2_node *node,
	struct is_subdev *subdev,
	struct is_frame *frame,
	struct is_queue *queue,
	struct mcs_param *mcs_param,
	struct param_mcs_output *mcs_output,
	struct is_crop *incrop,
	struct is_crop *otcrop,
	ulong index,
	u32 *lindex,
	u32 *hindex,
	u32 *indexes)
{
	int ret = 0;
	struct is_fmt *format, *tmp_format;
#ifdef SOC_VRA
	struct param_otf_input *otf_input = NULL;
#endif
	u32 flip = mcs_output->flip;
	u32 crange;
	u32 flag_extra;
	u32 hw_sbwc = 0;
	bool chg_format = false;
	u32 stripe_in_start_pos_x = 0;
	u32 stripe_roi_start_pos_x = 0, stripe_roi_end_pos_x = 0;
	u32 use_out_crop = 0;
	struct is_crop incrop_cfg, otcrop_cfg;

	FIMC_BUG(!queue);
	FIMC_BUG(!queue->framecfg.format);

	format = queue->framecfg.format;
	flag_extra = (queue->framecfg.hw_pixeltype & PIXEL_TYPE_EXTRA_MASK)
			>> PIXEL_TYPE_EXTRA_SHIFT;

	/* Check MCSC in/out crop size alignment */
	is_ischain_mxp_check_align(device, incrop, otcrop);

	/* if output 5, port for high frequency */
	if ((index - PARAM_MCS_OUTPUT0) == MCSC_INPUT_HF) {
		is_ischain_mxp_check_hf_size(device, incrop, otcrop);
	} else {
		/*
		* For compatibility with post processing IP,
		* width & stride should be aligned by 16.
		* Just check it because it is not MCSC HW spec
		*/
		if (otcrop->w % 16)
			mwarn("Output crop width(%d) should be multiple of %d", device, otcrop->w, 16);
	}

	incrop_cfg = *incrop;
	otcrop_cfg = *otcrop;

	if (node->pixelformat && format->pixelformat != node->pixelformat) { /* per-frame control for RGB */
		tmp_format = is_find_format((u32)node->pixelformat, 0);
		if (tmp_format) {
			mdbg_pframe("pixelformat is changed(%c%c%c%c->%c%c%c%c)\n",
				device, subdev, frame,
				(char)((format->pixelformat >> 0) & 0xFF),
				(char)((format->pixelformat >> 8) & 0xFF),
				(char)((format->pixelformat >> 16) & 0xFF),
				(char)((format->pixelformat >> 24) & 0xFF),
				(char)((tmp_format->pixelformat >> 0) & 0xFF),
				(char)((tmp_format->pixelformat >> 8) & 0xFF),
				(char)((tmp_format->pixelformat >> 16) & 0xFF),
				(char)((tmp_format->pixelformat >> 24) & 0xFF));
			queue->framecfg.format = format = tmp_format;
		} else {
			mdbg_pframe("pixelformat is not found(%c%c%c%c)\n",
				device, subdev, frame,
				(char)((node->pixelformat >> 0) & 0xFF),
				(char)((node->pixelformat >> 8) & 0xFF),
				(char)((node->pixelformat >> 16) & 0xFF),
				(char)((node->pixelformat >> 24) & 0xFF));
		}
	}

	if ((index >= PARAM_MCS_OUTPUT0 && index < PARAM_MCS_OUTPUT5) &&
		frame->shot_ext->mcsc_flip[index - PARAM_MCS_OUTPUT0] != mcs_output->flip) { /* per-frame control for Flip */
		flip = frame->shot_ext->mcsc_flip[index - PARAM_MCS_OUTPUT0];
		queue->framecfg.flip = flip << 1;
		mdbg_pframe("flip is changed(%d->%d)\n",
			device, subdev, frame,
			mcs_output->flip, flip);
	}

	if (IS_ENABLED(CHAIN_USE_STRIPE_PROCESSING) && frame && frame->stripe_info.region_num)
		use_out_crop = 1;
	/* if output DS, skip check a incrop & input mcs param
	 * because, DS input size set to preview port output size
	 */
	if ((index - PARAM_MCS_OUTPUT0) != MCSC_OUTPUT_DS)
		is_ischain_mxp_compare_size(device, mcs_param, &incrop_cfg);

	is_ischain_mxp_adjust_crop(device, incrop_cfg.w, incrop_cfg.h, &otcrop_cfg.w, &otcrop_cfg.h);

	if (queue->framecfg.quantization == V4L2_QUANTIZATION_FULL_RANGE) {
		crange = SCALER_OUTPUT_YUV_RANGE_FULL;
		if ((!IS_VIDEO_HDR_SCENARIO(device->setfile & IS_SETFILE_MASK) &&
			(!COMPARE_CROP(incrop, &subdev->input.crop) ||
			!COMPARE_CROP(otcrop, &subdev->output.crop))) ||
			debug_stream)
			mdbg_pframe("CRange:W\n", device, subdev, frame);
	} else {
		crange = SCALER_OUTPUT_YUV_RANGE_NARROW;
		if ((!IS_VIDEO_HDR_SCENARIO(device->setfile & IS_SETFILE_MASK) &&
			(!COMPARE_CROP(incrop, &subdev->input.crop) ||
			!COMPARE_CROP(otcrop, &subdev->output.crop))) ||
			debug_stream)
			mdbg_pframe("CRange:N\n", device, subdev, frame);
	}

	if (flag_extra) {
		hw_sbwc = (SBWC_BASE_ALIGN_MASK_LLC_ON | flag_extra);
		chg_format = true;
	}

	if (chg_format)
		mdbg_pframe("ot_crop[bitwidth %d sbwc 0x%x]\n",
				device, subdev, frame,
				format->hw_bitwidth,
				hw_sbwc);


	mcs_output->otf_format = OTF_OUTPUT_FORMAT_YUV422;
	mcs_output->otf_bitwidth = OTF_OUTPUT_BIT_WIDTH_8BIT;
	mcs_output->otf_order = OTF_OUTPUT_ORDER_BAYER_GR_BG;

	mcs_output->dma_cmd = DMA_OUTPUT_COMMAND_ENABLE;
	mcs_output->dma_bitwidth = format->hw_bitwidth;
	mcs_output->dma_format = format->hw_format;
	mcs_output->dma_order = format->hw_order;
	mcs_output->plane = format->hw_plane;
	mcs_output->sbwc_type = hw_sbwc;
	mcs_output->bitsperpixel = format->bitsperpixel[0] | format->bitsperpixel[1] << 8 | format->bitsperpixel[2] << 16;

	mcs_output->crop_offset_x = incrop->x; /* per frame */
	mcs_output->crop_offset_y = incrop->y; /* per frame */
	mcs_output->crop_width = incrop->w; /* per frame */
	mcs_output->crop_height = incrop->h; /* per frame */

	mcs_output->width = otcrop->w; /* per frame */
	mcs_output->height = otcrop->h; /* per frame */

	mcs_output->full_input_width = incrop->w; /* for stripe */
	mcs_output->full_output_width = otcrop->w; /* for stripe */

	mcs_output->stripe_in_start_pos_x = stripe_in_start_pos_x; /* for stripe */
	mcs_output->stripe_roi_start_pos_x = stripe_roi_start_pos_x; /* for stripe */
	mcs_output->stripe_roi_end_pos_x = stripe_roi_end_pos_x; /* for stripe */
	mcs_output->cmd = (use_out_crop & BIT(MCSC_OUT_CROP)); /* for stripe */
	mcs_output->cmd |= (node->request & BIT(MCSC_CROP_TYPE)); /* 0: ceter crop, 1: freeform crop */

	mcs_output->dma_stride_y = max(otcrop->w * format->bitsperpixel[0] / BITS_PER_BYTE,
					queue->framecfg.bytesperline[0]);
	mcs_output->dma_stride_c = max(otcrop->w * format->bitsperpixel[1] / BITS_PER_BYTE,
					queue->framecfg.bytesperline[1]);
	if ((index - PARAM_MCS_OUTPUT0) != MCSC_INPUT_HF &&
		(!IS_ALIGNED(mcs_output->dma_stride_y, 16) ||
		!IS_ALIGNED(mcs_output->dma_stride_c, 16)))
		mwarn("WDMA stride(y:%#x uv:%#x) should be %d aligned",
			device, mcs_output->dma_stride_y, mcs_output->dma_stride_c, 16);

	mcs_output->yuv_range = crange;
	mcs_output->flip = flip;

#ifdef ENABLE_HWFC
	if (test_bit(IS_ISCHAIN_REPROCESSING, &device->state))
		mcs_output->hwfc = 1; /* TODO: enum */
	else
		mcs_output->hwfc = 0; /* TODO: enum */
#endif
	*lindex |= LOWBIT_OF(index);
	*hindex |= HIGHBIT_OF(index);
	(*indexes)++;

#ifdef SOC_VRA
	if (device->group_mcs.junction == subdev) {
		otf_input = is_itf_g_param(device, frame, PARAM_FD_OTF_INPUT);
		otf_input->width = otcrop_cfg.w;
		otf_input->height = otcrop_cfg.h;
		*lindex |= LOWBIT_OF(PARAM_FD_OTF_INPUT);
		*hindex |= HIGHBIT_OF(PARAM_FD_OTF_INPUT);
		(*indexes)++;
	}
#endif
	set_bit(IS_SUBDEV_RUN, &subdev->state);

	return ret;
}

static int is_ischain_mxp_stop(struct is_device_ischain *device,
	struct is_subdev *subdev,
	struct is_frame *frame,
	struct param_mcs_output *mcs_output,
	ulong index,
	u32 *lindex,
	u32 *hindex,
	u32 *indexes)
{
	int ret = 0;

	mdbgd_ischain("%s\n", device, __func__);

	mcs_output->dma_cmd = DMA_OUTPUT_COMMAND_DISABLE;
	*lindex |= LOWBIT_OF(index);
	*hindex |= HIGHBIT_OF(index);
	(*indexes)++;

	clear_bit(IS_SUBDEV_RUN, &subdev->state);

	return ret;
}

static void is_ischain_mxp_otf_enable(struct is_device_ischain *device,
	struct is_subdev *subdev,
	struct is_frame *frame,
	struct param_mcs_output *mcs_output,
	ulong index,
	u32 *lindex,
	u32 *hindex,
	u32 *indexes)
{
#if !defined(USE_VRA_OTF)
	struct param_mcs_input *input;

	input = is_itf_g_param(device, frame, PARAM_MCS_INPUT);

	mcs_output->otf_cmd = OTF_OUTPUT_COMMAND_ENABLE;

	mcs_output->otf_format = OTF_OUTPUT_FORMAT_YUV422;
	mcs_output->otf_bitwidth = OTF_OUTPUT_BIT_WIDTH_8BIT;
	mcs_output->otf_order = OTF_OUTPUT_ORDER_BAYER_GR_BG;

	mcs_output->crop_offset_x = 0;
	mcs_output->crop_offset_y = 0;

	if (input->otf_cmd == OTF_INPUT_COMMAND_ENABLE) {
		mcs_output->crop_width = input->width;
		mcs_output->crop_height = input->height;
	} else {
		mcs_output->crop_width = input->dma_crop_width;
		mcs_output->crop_height = input->dma_crop_height;
	}

	/* HACK */
	if (mcs_output->crop_width > 640 && mcs_output->crop_height > 480) {
		mcs_output->width = mcs_output->crop_width;
		mcs_output->height = mcs_output->crop_height;
	} else {
		mcs_output->width = 640;
		mcs_output->height = 480;
	}

	*lindex |= LOWBIT_OF(index);
	*hindex |= HIGHBIT_OF(index);
	(*indexes)++;

	mdbg_pframe("OTF only enable [%d, %d, %d, %d]-->[%d, %d]\n", device, subdev, frame,
		mcs_output->crop_offset_x, mcs_output->crop_offset_y,
		mcs_output->crop_width, mcs_output->crop_height,
		mcs_output->width, mcs_output->height);
#endif
}

static void is_ischain_mxp_otf_disable(struct is_device_ischain *device,
		struct is_subdev *subdev,
		struct is_frame *frame,
		struct param_mcs_output *mcs_output,
		ulong index,
		u32 *lindex,
		u32 *hindex,
		u32 *indexes)
{
#if !defined(USE_VRA_OTF)
	mcs_output->otf_cmd = OTF_OUTPUT_COMMAND_DISABLE;

	*lindex |= LOWBIT_OF(index);
	*hindex |= HIGHBIT_OF(index);
	(*indexes)++;

	mdbg_pframe("OTF only disable\n", device, subdev, frame);
#endif
}

static int is_ischain_mxp_tag(struct is_subdev *subdev,
	void *device_data,
	struct is_frame *ldr_frame,
	struct camera2_node *node)
{
	int ret = 0;
	struct is_subdev *leader;
	struct is_queue *queue;
	struct mcs_param *mcs_param;
	struct param_mcs_output *mcs_output;
	struct is_crop *incrop, *otcrop, inparm, otparm;
	struct is_device_ischain *device;
	u32 index, lindex, hindex, indexes;
	u32 pixelformat = 0;
	u32 *target_addr;
	bool change_flip = false;
	bool change_pixelformat = false;

	device = (struct is_device_ischain *)device_data;

	FIMC_BUG(!device);
	FIMC_BUG(!device->is_region);
	FIMC_BUG(!subdev);
	FIMC_BUG(!GET_SUBDEV_QUEUE(subdev));
	FIMC_BUG(!ldr_frame);
	FIMC_BUG(!ldr_frame->shot);
	FIMC_BUG(!node);

	mdbgs_ischain(4, "MXP TAG(request %d)\n", device, node->request);

	lindex = hindex = indexes = 0;
	leader = subdev->leader;
	mcs_param = &device->is_region->parameter.mcs;
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

	switch (node->vid) {
	case IS_VIDEO_M0P_NUM:
		index = PARAM_MCS_OUTPUT0;
		target_addr = ldr_frame->sc0TargetAddress;
		break;
	case IS_VIDEO_M1P_NUM:
		index = PARAM_MCS_OUTPUT1;
		target_addr = ldr_frame->sc1TargetAddress;
		break;
	case IS_VIDEO_M2P_NUM:
		index = PARAM_MCS_OUTPUT2;
		target_addr = ldr_frame->sc2TargetAddress;
		break;
	case IS_VIDEO_M3P_NUM:
		index = PARAM_MCS_OUTPUT3;
		target_addr = ldr_frame->sc3TargetAddress;
		break;
	case IS_VIDEO_M4P_NUM:
		index = PARAM_MCS_OUTPUT4;
		target_addr = ldr_frame->sc4TargetAddress;
		break;
	case IS_VIDEO_M5P_NUM:
		index = PARAM_MCS_OUTPUT5;
		target_addr = ldr_frame->sc5TargetAddress;
		break;
	default:
		mserr("vid(%d) is not matched", device, subdev, node->vid);
		ret = -EINVAL;
		goto p_err;
	}

	memset(target_addr, 0, sizeof(ldr_frame->sc0TargetAddress));

	mcs_output = is_itf_g_param(device, ldr_frame, index);

	if (node->request & BIT(0)) {
		incrop = (struct is_crop *)node->input.cropRegion;
		otcrop = (struct is_crop *)node->output.cropRegion;
		if (node->pixelformat) {
			change_pixelformat = !COMPARE_FORMAT(pixelformat, node->pixelformat);
			pixelformat = node->pixelformat;
		}
		if ((index >= PARAM_MCS_OUTPUT0 && index < PARAM_MCS_OUTPUT5) &&
			ldr_frame->shot_ext->mcsc_flip[index - PARAM_MCS_OUTPUT0] != mcs_output->flip)
			change_flip = true;

		inparm.x = mcs_output->crop_offset_x;
		inparm.y = mcs_output->crop_offset_y;
		inparm.w = mcs_output->crop_width;
		inparm.h = mcs_output->crop_height;

		otparm.x = mcs_output->offset_x;
		otparm.y = mcs_output->offset_y;
		otparm.w = mcs_output->width;
		otparm.h = mcs_output->height;

		if (IS_NULL_CROP(incrop))
			*incrop = inparm;

		if (IS_NULL_CROP(otcrop))
			*otcrop = otparm;

		if (!COMPARE_CROP(incrop, &inparm) ||
			!COMPARE_CROP(otcrop, &otparm) ||
			CHECK_STRIPE_CFG(&ldr_frame->stripe_info) ||
			change_flip ||
			change_pixelformat ||
			!test_bit(IS_SUBDEV_RUN, &subdev->state) ||
			test_bit(IS_SUBDEV_FORCE_SET, &leader->state)) {
			ret = is_ischain_mxp_start(device,
				node,
				subdev,
				ldr_frame,
				queue,
				mcs_param,
				mcs_output,
				incrop,
				otcrop,
				index,
				&lindex,
				&hindex,
				&indexes);
			if (ret) {
				merr("is_ischain_mxp_start is fail(%d)", device, ret);
				goto p_err;
			}

			if ((!IS_VIDEO_HDR_SCENARIO(device->setfile & IS_SETFILE_MASK) &&
				(!COMPARE_CROP(incrop, &subdev->input.crop) ||
				!COMPARE_CROP(otcrop, &subdev->output.crop))) ||
				debug_stream) {
				mdbg_pframe("in_crop[%d, %d, %d, %d]\n", device, subdev, ldr_frame,
					incrop->x, incrop->y, incrop->w, incrop->h);
				mdbg_pframe("ot_crop[%d, %d, %d, %d]\n", device, subdev, ldr_frame,
					otcrop->x, otcrop->y, otcrop->w, otcrop->h);

				subdev->input.crop = *incrop;
				subdev->output.crop = *otcrop;
			}
		}

		ret = is_ischain_buf_tag(device,
			subdev,
			ldr_frame,
			pixelformat,
			mcs_output->dma_stride_y,
			otcrop->h,
			target_addr);
		if (ret) {
			mswarn("%d frame is drop", device, subdev, ldr_frame->fcount);
			node->request = 0;
		}
	} else {
		if (test_bit(IS_SUBDEV_RUN, &subdev->state)) {
			ret = is_ischain_mxp_stop(device,
				subdev,
				ldr_frame,
				mcs_output,
				index,
				&lindex,
				&hindex,
				&indexes);
			if (ret) {
				merr("is_ischain_mxp_stop is fail(%d)", device, ret);
				goto p_err;
			}

			if (!IS_VIDEO_HDR_SCENARIO(device->setfile & IS_SETFILE_MASK))
				mdbg_pframe(" off\n", device, subdev, ldr_frame);
		}

		if ((node->vid - IS_VIDEO_M0P_NUM)
			== (ldr_frame->shot->uctl.scalerUd.mcsc_sub_blk_port[INTERFACE_TYPE_DS]))
			is_ischain_mxp_otf_enable(device,
					subdev,
					ldr_frame,
					mcs_output,
					index,
					&lindex,
					&hindex,
					&indexes);
		else if (mcs_output->otf_cmd == OTF_OUTPUT_COMMAND_ENABLE)
			is_ischain_mxp_otf_disable(device,
					subdev,
					ldr_frame,
					mcs_output,
					index,
					&lindex,
					&hindex,
					&indexes);
		target_addr[0] = 0;
		target_addr[1] = 0;
		target_addr[2] = 0;
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

const struct is_subdev_ops is_subdev_mcsp_ops = {
	.bypass			= NULL,
	.cfg			= is_ischain_mxp_cfg,
	.tag			= is_ischain_mxp_tag,
};
