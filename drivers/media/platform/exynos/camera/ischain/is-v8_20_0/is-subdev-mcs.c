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

int is_ischain_mcs_stripe_cfg(struct is_group* group,
		struct is_subdev *subdev,
		struct is_frame *frame,
		struct is_crop *incrop,
		struct is_crop *otcrop,
		struct is_frame_cfg *framecfg)
{
	struct camera2_stream *stream = (struct camera2_stream *) frame->shot_ext;
	struct is_fmt *fmt = framecfg->format;
	u32 region_id = frame->stripe_info.region_id;
	u32 stripe_w, dma_offset = 0;
	u32 scn_check_remosaic = 0;

	scn_check_remosaic = CHK_MODECHANGE_SCN(frame->shot->ctl.aa.captureIntent);
	if (!region_id) {
		/* Left region */
		if (test_bit(IS_GROUP_OTF_INPUT, &group->state)) {
			stripe_w = frame->stripe_info.in.h_pix_num;
		} else if (stream->stripe_h_pix_nums[region_id]) {
			stripe_w = stream->stripe_h_pix_nums[region_id];
		} else {
			stripe_w = ALIGN(incrop->w / frame->stripe_info.region_num, 2);
			stripe_w = ALIGN_UPDOWN_STRIPE_WIDTH(stripe_w);
		}

		if (stripe_w == 0) {
			msrdbgs(3, "Skip current stripe[#%d] region because stripe_width is too small(%d)\n",
					subdev, subdev, frame, region_id, stripe_w);
			frame->stripe_info.region_id++;
			return -EAGAIN;
		}

		if (!test_bit(IS_GROUP_OTF_INPUT, &group->state)) {
			frame->stripe_info.in.h_pix_num = stripe_w;
			frame->stripe_info.region_base_addr[0] = frame->dvaddr_buffer[0];
		}
	} else if (region_id < frame->stripe_info.region_num - 1) {
		/* Middle region */
		if (test_bit(IS_GROUP_OTF_INPUT, &group->state)) {
			stripe_w = frame->stripe_info.in.h_pix_num - frame->stripe_info.in.prev_h_pix_num;
		} else if (stream->stripe_h_pix_nums[region_id]) {
			stripe_w = stream->stripe_h_pix_nums[region_id] - frame->stripe_info.in.h_pix_num;
		} else {
			stripe_w = incrop->w * (region_id + 1) / frame->stripe_info.region_num;
			stripe_w = ALIGN((stripe_w - frame->stripe_info.in.h_pix_num), 2);
			stripe_w = ALIGN_UPDOWN_STRIPE_WIDTH(stripe_w);
		}

		if (stripe_w == 0) {
			msrdbgs(3, "Skip current stripe[#%d] region because stripe_width is too small(%d)\n",
					subdev, subdev, frame, region_id, stripe_w);
			frame->stripe_info.region_id++;
			return -EAGAIN;
		}

		if (!test_bit(IS_GROUP_OTF_INPUT, &group->state)) {
			if (scn_check_remosaic) {
				dma_offset = frame->stripe_info.in.h_pix_num;
				dma_offset *= fmt->bitsperpixel[0] / BITS_PER_BYTE;
			} else {
				if (stream->stripe_h_pix_nums[region_id]) {
					dma_offset = frame->stripe_info.in.h_pix_num;
					dma_offset += STRIPE_MARGIN_WIDTH * ((2 * (region_id - 1)) + 1);
					dma_offset *= fmt->bitsperpixel[0] / BITS_PER_BYTE * incrop->h;
				} else {
					dma_offset = frame->stripe_info.in.h_pix_num - STRIPE_MARGIN_WIDTH;
					dma_offset *= fmt->bitsperpixel[0] / BITS_PER_BYTE;
				}
			}

			frame->stripe_info.in.h_pix_num += stripe_w;
		} else {
			frame->stripe_info.in.prev_h_pix_num = frame->stripe_info.in.h_pix_num;
		}

		if (!scn_check_remosaic)
			stripe_w += STRIPE_MARGIN_WIDTH;
	} else {
		/* Right region */
		stripe_w = incrop->w - frame->stripe_info.in.h_pix_num;

		if (!test_bit(IS_GROUP_OTF_INPUT, &group->state)) {
			if (scn_check_remosaic) {
				dma_offset = frame->stripe_info.in.h_pix_num;
				dma_offset *= fmt->bitsperpixel[0] / BITS_PER_BYTE;
			} else {
				if (stream->stripe_h_pix_nums[region_id]) {
					dma_offset = frame->stripe_info.in.h_pix_num;
					dma_offset += STRIPE_MARGIN_WIDTH * ((2 * (region_id - 1)) + 1);
					dma_offset *= fmt->bitsperpixel[0] / BITS_PER_BYTE * incrop->h;
				} else {
					dma_offset = frame->stripe_info.in.h_pix_num - STRIPE_MARGIN_WIDTH;
					dma_offset *= fmt->bitsperpixel[0] / BITS_PER_BYTE;
				}
			}
		}
	}

	if (!scn_check_remosaic)
		stripe_w += STRIPE_MARGIN_WIDTH;
	incrop->w = stripe_w;

	/**
	 * Output crop configuration.
	 * No crop & scale.
	 */
	otcrop->x = 0;
	otcrop->y = 0;
	otcrop->w = incrop->w;
	otcrop->h = incrop->h;

	if (!test_bit(IS_GROUP_OTF_INPUT, &group->state))
		frame->dvaddr_buffer[0] = frame->stripe_info.region_base_addr[0] + dma_offset;

	msrdbgs(3, "stripe_in_crop[%d][%d, %d, %d, %d] offset %x\n", subdev, subdev, frame,
			frame->stripe_info.region_id,
			incrop->x, incrop->y, incrop->w, incrop->h, dma_offset);
	msrdbgs(3, "stripe_ot_crop[%d][%d, %d, %d, %d]\n", subdev, subdev, frame,
			frame->stripe_info.region_id,
			otcrop->x, otcrop->y, otcrop->w, otcrop->h);

	return 0;
}

static int is_ischain_mcs_cfg(struct is_subdev *leader,
	void *device_data,
	struct is_frame *frame,
	struct is_crop *incrop,
	struct is_crop *otcrop,
	u32 *lindex,
	u32 *hindex,
	u32 *indexes)
{
	int ret = 0;
	struct is_fmt *format;
	struct is_group *group;
	struct is_queue *queue;
	struct param_mcs_input *input;
	struct param_control *control;
	struct is_device_ischain *device;
	struct is_crop incrop_cfg, otcrop_cfg;
	u32 scn_check_remosaic = 0;

	device = (struct is_device_ischain *)device_data;

	FIMC_BUG(!leader);
	FIMC_BUG(!device);
	FIMC_BUG(!device->sensor);
	FIMC_BUG(!incrop);
	FIMC_BUG(!lindex);
	FIMC_BUG(!hindex);
	FIMC_BUG(!indexes);

	queue = GET_SUBDEV_QUEUE(leader);
	if (!queue) {
		merr("queue is NULL", device);
		ret = -EINVAL;
		goto p_err;
	}

	format = queue->framecfg.format;
	group = &device->group_mcs;
	incrop_cfg = *incrop;
	otcrop_cfg = *otcrop;


	if (IS_ENABLED(CHAIN_USE_STRIPE_PROCESSING)	&& frame
			&& frame->stripe_info.region_num
			&& device->sensor->use_stripe_flag) {
		is_ischain_mcs_stripe_cfg(group, leader, frame,
				&incrop_cfg, &otcrop_cfg, &queue->framecfg);
		scn_check_remosaic = CHK_MODECHANGE_SCN(frame->shot->ctl.aa.captureIntent);
	}

	input = is_itf_g_param(device, frame, PARAM_MCS_INPUT);
	if (test_bit(IS_GROUP_OTF_INPUT, &group->state)) {
		input->otf_cmd = OTF_INPUT_COMMAND_ENABLE;
		input->dma_cmd = DMA_INPUT_COMMAND_DISABLE;
		input->width = incrop_cfg.w;
		input->height = incrop_cfg.h;
	} else {
		input->otf_cmd = OTF_INPUT_COMMAND_DISABLE;
		input->dma_cmd = DMA_INPUT_COMMAND_ENABLE;
		input->width = leader->input.canv.w;
		input->height = leader->input.canv.h;
		input->dma_crop_offset_x = leader->input.canv.x + incrop_cfg.x;
		input->dma_crop_offset_y = leader->input.canv.y + incrop_cfg.y;
		input->dma_crop_width = incrop_cfg.w;
		input->dma_crop_height = incrop_cfg.h;

		input->dma_format = format->hw_format;
		input->dma_bitwidth = format->hw_bitwidth;
		input->dma_order = format->hw_order;
		input->plane = format->hw_plane;

		/*
		 * HW spec: DMA stride should be aligned by 16 byte.
		 */
		if (scn_check_remosaic) {
			input->dma_stride_y = ALIGN(max(input->width * format->bitsperpixel[0] / BITS_PER_BYTE,
						queue->framecfg.bytesperline[0]), 16);
			input->dma_stride_c = ALIGN(max(input->width * format->bitsperpixel[1] / BITS_PER_BYTE,
						queue->framecfg.bytesperline[1]), 16);
		} else {
			input->dma_stride_y = ALIGN(max(input->dma_crop_width * format->bitsperpixel[0] / BITS_PER_BYTE,
						queue->framecfg.bytesperline[0]), 16);
			input->dma_stride_c = ALIGN(max(input->dma_crop_width * format->bitsperpixel[1] / BITS_PER_BYTE,
						queue->framecfg.bytesperline[1]), 16);
		}
	}

	input->otf_format = OTF_INPUT_FORMAT_YUV422;
	input->otf_bitwidth = OTF_INPUT_BIT_WIDTH_8BIT;
	input->otf_order = OTF_INPUT_ORDER_BAYER_GR_BG;

	*lindex |= LOWBIT_OF(PARAM_MCS_INPUT);
	*hindex |= HIGHBIT_OF(PARAM_MCS_INPUT);
	(*indexes)++;

	/* Configure Control */
	control = is_itf_g_param(device, frame, PARAM_MCS_CONTROL);
	control->cmd = CONTROL_COMMAND_START;
#ifdef ENABLE_DNR_IN_MCSC
	control->buffer_address = device->minfo->dvaddr_mcsc_dnr;
#endif
	*lindex |= LOWBIT_OF(PARAM_MCS_CONTROL);
	*hindex |= HIGHBIT_OF(PARAM_MCS_CONTROL);
	(*indexes)++;

	leader->input.crop = *incrop;

	if (test_bit(IS_ISCHAIN_REPROCESSING, &device->state)) {
		mswarn("reprocessing cannot connect to VRA\n", device, leader);
		goto p_err;
	}

p_err:
	return ret;
}

static int is_ischain_mcs_tag(struct is_subdev *subdev,
	void *device_data,
	struct is_frame *frame,
	struct camera2_node *node)
{
	int ret = 0;
	struct is_group *group;
	struct mcs_param *mcs_param;
	struct is_crop inparm;
	struct is_crop *incrop, *otcrop;
	struct is_subdev *leader;
	struct is_device_ischain *device;
	u32 lindex, hindex, indexes;

	device = (struct is_device_ischain *)device_data;

	FIMC_BUG(!subdev);
	FIMC_BUG(!device);
	FIMC_BUG(!device->is_region);
	FIMC_BUG(!frame);

	mdbgs_ischain(4, "MCSC TAG\n", device);

	incrop = (struct is_crop *)node->input.cropRegion;
	otcrop = (struct is_crop *)node->output.cropRegion;

	group = &device->group_mcs;
	leader = subdev->leader;
	lindex = hindex = indexes = 0;
	mcs_param = &device->is_region->parameter.mcs;

	if (test_bit(IS_GROUP_OTF_INPUT, &group->state)) {
		inparm.x = 0;
		inparm.y = 0;
		inparm.w = mcs_param->input.width;
		inparm.h = mcs_param->input.height;
	} else {
		inparm.x = mcs_param->input.dma_crop_offset_x;
		inparm.y = mcs_param->input.dma_crop_offset_y;
		inparm.w = mcs_param->input.dma_crop_width;
		inparm.h = mcs_param->input.dma_crop_height;
	}

	if (IS_NULL_CROP(incrop))
		*incrop = inparm;

	if (!COMPARE_CROP(incrop, &inparm) ||
		CHECK_STRIPE_CFG(&frame->stripe_info) ||
		test_bit(IS_SUBDEV_FORCE_SET, &leader->state)) {
		ret = is_ischain_mcs_cfg(subdev,
			device,
			frame,
			incrop,
			otcrop,
			&lindex,
			&hindex,
			&indexes);
		if (ret) {
			merr("is_ischain_mcs_start is fail(%d)", device, ret);
			goto p_err;
		}
		if (!COMPARE_CROP(incrop, &subdev->input.crop) ||
			debug_stream) {
			msrinfo("in_crop[%d, %d, %d, %d]\n", device, subdev, frame,
				incrop->x, incrop->y, incrop->w, incrop->h);
			subdev->input.crop = *incrop;
		}
	}

	ret = is_itf_s_param(device, frame, lindex, hindex, indexes);
	if (ret) {
		mrerr("is_itf_s_param is fail(%d)", device, frame, ret);
		goto p_err;
	}

p_err:
	return ret;
}

const struct is_subdev_ops is_subdev_mcs_ops = {
#ifdef USE_VRA_OTF
	.bypass			= is_ischain_mcs_bypass,
#else
	.bypass			= NULL,
#endif
	.cfg			= is_ischain_mcs_cfg,
	.tag			= is_ischain_mcs_tag,
};
