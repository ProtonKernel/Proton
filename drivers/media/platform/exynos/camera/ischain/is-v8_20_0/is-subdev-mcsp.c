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

#include "is-core.h"
#include "is-dvfs.h"
#include "is-hw-dvfs.h"

int is_ischain_mxp_stripe_cfg(struct is_subdev *subdev,
		struct is_frame *ldr_frame,
		struct is_crop *incrop,
		struct is_crop *otcrop,
		struct is_frame_cfg *framecfg,
		u32 mcs_out_flip)
{
	struct is_framemgr *framemgr;
	struct is_frame *frame;
	struct is_fmt *fmt = framecfg->format;
	bool x_flip = mcs_out_flip & CAM_FLIP_MODE_HORIZONTAL ? true : false;
	bool is_last_region = false;
	unsigned long flags;
	u32 region_id = ldr_frame->stripe_info.region_id;
	u32 stripe_x, stripe_w;
	u32 dma_offset = 0;
	int temp_stripe_x = 0, temp_stripe_w = 0;
	u32 scn_check_remosaic = 0, temp_stripe_margin_width = 0;;

	framemgr = GET_SUBDEV_FRAMEMGR(subdev);
	if (!framemgr)
		return 0;

	framemgr_e_barrier_irqs(framemgr, FMGR_IDX_24, flags);

	frame = peek_frame(framemgr, ldr_frame->state);
	if (frame) {
		scn_check_remosaic = CHK_MODECHANGE_SCN(ldr_frame->shot->ctl.aa.captureIntent);
		if (!scn_check_remosaic)
			temp_stripe_margin_width = STRIPE_MARGIN_WIDTH;

		/* Input crop configuration */
		if (!region_id) {
			/* Left region w/o margin */
			stripe_x = incrop->x;
			temp_stripe_w = ldr_frame->stripe_info.in.h_pix_num - stripe_x;
			stripe_w = temp_stripe_w > 0 ? temp_stripe_w: 0;
			frame->stripe_info.in.h_pix_ratio = stripe_w * STRIPE_RATIO_PRECISION / incrop->w;
			frame->stripe_info.in.h_pix_num = stripe_w;
		} else if (region_id < ldr_frame->stripe_info.region_num - 1) {
			/* Middle region w/o margin */
			stripe_x = temp_stripe_margin_width;
			temp_stripe_w = ldr_frame->stripe_info.in.h_pix_num - frame->stripe_info.in.h_pix_num - incrop->x;

			/* Stripe x when crop offset x start in middle region */
			if (!frame->stripe_info.in.h_pix_num && incrop->x < ldr_frame->stripe_info.in.h_pix_num)
				temp_stripe_x = incrop->x - ldr_frame->stripe_info.in.prev_h_pix_num;

			/* Stripe width when crop region end in middle region */
			if (incrop->x + incrop->w < ldr_frame->stripe_info.in.h_pix_num) {
				temp_stripe_w = incrop->w - frame->stripe_info.in.h_pix_num;
				is_last_region = true;
			}

			stripe_w = temp_stripe_w > 0 ? temp_stripe_w: 0;
			stripe_x = stripe_x + temp_stripe_x;

			frame->stripe_info.in.h_pix_ratio = stripe_w * STRIPE_RATIO_PRECISION / incrop->w;
			frame->stripe_info.in.h_pix_num += stripe_w;
		} else {
			/* Right region w/o margin */
			stripe_x = temp_stripe_margin_width;
			temp_stripe_w = incrop->w - frame->stripe_info.in.h_pix_num;
			stripe_w = temp_stripe_w > 0 ? temp_stripe_w: 0;
		}

		incrop->x = stripe_x;
		incrop->w = stripe_w;

		/* Output crop & WDMA offset configuration */
		if (!region_id) {
			/* Left region */
			stripe_w = ALIGN(otcrop->w * frame->stripe_info.in.h_pix_ratio / STRIPE_RATIO_PRECISION, 2);

			frame->stripe_info.out.h_pix_num = stripe_w;
			frame->stripe_info.region_base_addr[0] = frame->dvaddr_buffer[0];
			frame->stripe_info.region_base_addr[1] = frame->dvaddr_buffer[1];

			/* Add horizontal DMA offset */
			if (x_flip)
				dma_offset = (otcrop->w - stripe_w) * fmt->bitsperpixel[0] / BITS_PER_BYTE;
		} else if (region_id < ldr_frame->stripe_info.region_num - 1) {
			/* Middle region */
			if (is_last_region)
				stripe_w = otcrop->w - frame->stripe_info.out.h_pix_num;
			else
				stripe_w = ALIGN(otcrop->w * frame->stripe_info.in.h_pix_ratio / STRIPE_RATIO_PRECISION, 2);

			/* Add horizontal DMA offset */
			if (x_flip)
				dma_offset = (otcrop->w - frame->stripe_info.out.h_pix_num - stripe_w) * fmt->bitsperpixel[0] / BITS_PER_BYTE;
			else
				dma_offset = frame->stripe_info.out.h_pix_num * fmt->bitsperpixel[0] / BITS_PER_BYTE;
			frame->stripe_info.out.h_pix_num += stripe_w;
		} else {
			/* Right region */
			stripe_w = otcrop->w - frame->stripe_info.out.h_pix_num;

			/* Add horizontal DMA offset */
			if (x_flip)
				dma_offset = 0;
			else
				dma_offset = frame->stripe_info.out.h_pix_num * fmt->bitsperpixel[0] / BITS_PER_BYTE;
		}

		otcrop->w = stripe_w;

		if (temp_stripe_w <= 0) {
			mdbg_pframe("Skip current stripe[#%d] region because stripe_width is too small(%d)\n",
				subdev, subdev, ldr_frame, region_id, stripe_w);
			framemgr_x_barrier_irqr(framemgr, FMGR_IDX_24, flags);
			return -EAGAIN;
		}

		frame->dvaddr_buffer[0] = frame->stripe_info.region_base_addr[0] + dma_offset;
		/* Calculate chroma base address for NV21M */
		frame->dvaddr_buffer[1] = frame->stripe_info.region_base_addr[1] + dma_offset;

		msrdbgs(3, "stripe_in_crop[%d][%d, %d, %d, %d]\n", subdev, subdev, ldr_frame,
				region_id,
				incrop->x, incrop->y, incrop->w, incrop->h);
		msrdbgs(3, "stripe_ot_crop[%d][%d, %d, %d, %d] offset %x\n", subdev, subdev, ldr_frame,
				region_id,
				otcrop->x, otcrop->y, otcrop->w, otcrop->h, dma_offset);
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
	/* HW spec: stride should be aligned by 16 byte. */
	mcs_output->dma_stride_y = ALIGN(max(width * format->bitsperpixel[0] / BITS_PER_BYTE,
					queue->framecfg.bytesperline[0]), 16);
	mcs_output->dma_stride_c = ALIGN(max(width * format->bitsperpixel[1] / BITS_PER_BYTE,
					queue->framecfg.bytesperline[1]), 16);

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

#define MXP_RATIO_UP	(14)
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

	if (*output_crop_w < (input_crop_w + 23) / 24) {
		mwarn("Cannot be scaled down beyond 1/16 times(%d -> %d)",
			device, input_crop_w, *output_crop_w);
		*output_crop_w = ALIGN((input_crop_w + 23) / 24, 2);
		changed |= 0x10;
	}

	if (*output_crop_h < (input_crop_h + 23) / 24) {
		mwarn("Cannot be scaled down beyond 1/16 times(%d -> %d)",
			device, input_crop_h, *output_crop_h);
		*output_crop_h = ALIGN((input_crop_h + 23) / 24, 2);
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
	u32 crange;
	struct is_crop incrop_cfg, otcrop_cfg;

	FIMC_BUG(!queue);
	FIMC_BUG(!queue->framecfg.format);

	format = queue->framecfg.format;
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

	if (queue->framecfg.quantization == V4L2_QUANTIZATION_FULL_RANGE) {
		crange = SCALER_OUTPUT_YUV_RANGE_FULL;
		if (!COMPARE_CROP(incrop, &subdev->input.crop) ||
			!COMPARE_CROP(otcrop, &subdev->output.crop) ||
			debug_stream)
			mdbg_pframe("CRange:W\n", device, subdev, frame);
	} else {
		crange = SCALER_OUTPUT_YUV_RANGE_NARROW;
		if (!COMPARE_CROP(incrop, &subdev->input.crop) ||
			!COMPARE_CROP(otcrop, &subdev->output.crop) ||
			debug_stream)
			mdbg_pframe("CRange:N\n", device, subdev, frame);
	}

	if ((index >= PARAM_MCS_OUTPUT0 && index < PARAM_MCS_OUTPUT5) &&
		frame->shot_ext->mcsc_flip[index - PARAM_MCS_OUTPUT0] != mcs_output->flip) {
		mdbg_pframe("flip is changed(%d->%d)\n",
			device, subdev, frame,
			mcs_output->flip,
			frame->shot_ext->mcsc_flip[index - PARAM_MCS_OUTPUT0]);
		mcs_output->flip = frame->shot_ext->mcsc_flip[index - PARAM_MCS_OUTPUT0];
	}

	if (IS_ENABLED(CHAIN_USE_STRIPE_PROCESSING) && frame
		&& frame->stripe_info.region_num
		&& device->sensor->use_stripe_flag)
		is_ischain_mxp_stripe_cfg(subdev, frame,
				&incrop_cfg, &otcrop_cfg,
				&queue->framecfg,
				mcs_output->flip);

	/* if output DS, skip check a incrop & input mcs param
	 * because, DS input size set to preview port output size
	 */
	if ((index - PARAM_MCS_OUTPUT0) != MCSC_OUTPUT_DS)
		is_ischain_mxp_compare_size(device, mcs_param, &incrop_cfg);

	is_ischain_mxp_adjust_crop(device, incrop_cfg.w, incrop_cfg.h, &otcrop_cfg.w, &otcrop_cfg.h);

	mcs_output->otf_format = OTF_OUTPUT_FORMAT_YUV422;
	mcs_output->otf_bitwidth = OTF_OUTPUT_BIT_WIDTH_8BIT;
	mcs_output->otf_order = OTF_OUTPUT_ORDER_BAYER_GR_BG;

	mcs_output->dma_cmd = DMA_OUTPUT_COMMAND_ENABLE;
	mcs_output->dma_bitwidth = format->hw_bitwidth;
	mcs_output->dma_format = format->hw_format;
	mcs_output->dma_order = format->hw_order;
	mcs_output->plane = format->hw_plane;
	mcs_output->bitsperpixel = format->bitsperpixel[0] | format->bitsperpixel[1] << 8 | format->bitsperpixel[2] << 16;

	mcs_output->crop_offset_x = incrop_cfg.x; /* per frame */
	mcs_output->crop_offset_y = incrop_cfg.y; /* per frame */
	mcs_output->crop_width = incrop_cfg.w; /* per frame */
	mcs_output->crop_height = incrop_cfg.h; /* per frame */

	mcs_output->width = otcrop_cfg.w; /* per frame */
	mcs_output->height = otcrop_cfg.h; /* per frame */

	mcs_output->cmd = 0;
	mcs_output->cmd |= (node->request & BIT(MCSC_CROP_TYPE)); /* 0: ceter crop, 1: freeform crop */
	/* HW spec: stride should be aligned by 16 byte. */
	mcs_output->dma_stride_y = ALIGN(max(otcrop->w * format->bitsperpixel[0] / BITS_PER_BYTE,
					queue->framecfg.bytesperline[0]), 16);
	mcs_output->dma_stride_c = ALIGN(max(otcrop->w * format->bitsperpixel[1] / BITS_PER_BYTE,
					queue->framecfg.bytesperline[1]), 16);

	mcs_output->yuv_range = crange;

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
	bool change_pixelformat = false;
	bool change_flip = false;

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

	if (node->request) {
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
			change_pixelformat ||
			change_flip ||
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

			if (!COMPARE_CROP(incrop, &subdev->input.crop) ||
				!COMPARE_CROP(otcrop, &subdev->output.crop) ||
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
			otcrop->w,
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
