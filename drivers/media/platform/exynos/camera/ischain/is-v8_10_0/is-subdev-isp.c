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

#include <linux/kernel.h>
#include <linux/soc/samsung/exynos-soc.h>

#include "is-device-ischain.h"
#include "is-device-sensor.h"
#include "is-subdev-ctrl.h"
#include "is-config.h"
#include "is-param.h"
#include "is-video.h"
#include "is-type.h"

int is_ischain_isp_stripe_cfg(struct is_subdev *subdev,
		struct is_frame *frame,
		struct is_crop *incrop,
		struct is_crop *otcrop,
		u32 bitwidth)
{
	struct is_groupmgr *groupmgr = (struct is_groupmgr *)frame->groupmgr;
	struct is_group *group = frame->group;
	struct is_group *stream_leader = groupmgr->leader[subdev->instance];
	struct camera2_stream *stream = (struct camera2_stream *) frame->shot_ext;
	u32 region_id = frame->stripe_info.region_id;
	u32 stripe_w, dma_offset = 0;

	/* Input crop configuration */
	if (!region_id) {
		/* Left region */
		if (stream->stripe_h_pix_nums[region_id]) {
			stripe_w = stream->stripe_h_pix_nums[region_id];
		} else {
			/* Stripe width should be 4 align because of 4 ppc */
			stripe_w = ALIGN(incrop->w / frame->stripe_info.region_num, 4);
			stripe_w = ALIGN_UPDOWN_STRIPE_WIDTH(stripe_w);
		}

		if (stripe_w == 0) {
			msrdbgs(3, "Skip current stripe[#%d] region because stripe_width is too small(%d)\n", subdev, subdev, frame,
									frame->stripe_info.region_id, stripe_w);
			frame->stripe_info.region_id++;
			return -EAGAIN;
		}

		frame->stripe_info.in.h_pix_ratio = stripe_w * STRIPE_RATIO_PRECISION / incrop->w;
		frame->stripe_info.in.h_pix_num = stripe_w;
		frame->stripe_info.region_base_addr[0] = frame->dvaddr_buffer[0];
	} else if (frame->stripe_info.region_id < frame->stripe_info.region_num - 1) {
		/* Middle region */
		stripe_w = ALIGN((incrop->w * (frame->stripe_info.region_id + 1) / frame->stripe_info.region_num) - frame->stripe_info.in.h_pix_num, 4);
		stripe_w = ALIGN_UPDOWN_STRIPE_WIDTH(stripe_w);

		if (stripe_w == 0) {
			msrdbgs(3, "Skip current stripe[#%d] region because stripe_width is too small(%d)\n", subdev, subdev, frame,
									frame->stripe_info.region_id, stripe_w);
			frame->stripe_info.region_id++;
			return -EAGAIN;
		}

		stripe_w += STRIPE_MARGIN_WIDTH;
		if (!test_bit(IS_GROUP_OTF_INPUT, &group->state)) {
			dma_offset = frame->stripe_info.in.h_pix_num - STRIPE_MARGIN_WIDTH;
			dma_offset = dma_offset * bitwidth / BITS_PER_BYTE;
		}
	} else {
		/* Right region */
		stripe_w = incrop->w - frame->stripe_info.in.h_pix_num;

		/* Consider RDMA offset. */
		if (!test_bit(IS_GROUP_OTF_INPUT, &group->state)) {
			if (stream_leader->id == group->id) {
				/**
				 * ISP reads the right region of original bayer image.
				 * Add horizontal DMA offset only.
				 */
				dma_offset = frame->stripe_info.in.h_pix_num - STRIPE_MARGIN_WIDTH;
				dma_offset = dma_offset * bitwidth / BITS_PER_BYTE;
				msrwarn("Processed bayer reprocessing is NOT supported by stripe processing",
						subdev, subdev, frame);

			} else {
				/**
				 * ISP reads the right region with stripe margin.
				 * Add horizontal DMA offset.
				 */
				dma_offset = frame->stripe_info.in.h_pix_num - STRIPE_MARGIN_WIDTH;
				dma_offset = dma_offset * bitwidth / BITS_PER_BYTE;
			}
		}
	}
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

	frame->dvaddr_buffer[0] = frame->stripe_info.region_base_addr[0] + dma_offset;

	msrdbgs(3, "stripe_in_crop[%d][%d, %d, %d, %d] offset %x\n", subdev, subdev, frame,
			region_id, incrop->x, incrop->y, incrop->w, incrop->h, dma_offset);
	msrdbgs(3, "stripe_ot_crop[%d][%d, %d, %d, %d]\n", subdev, subdev, frame,
			region_id, otcrop->x, otcrop->y, otcrop->w, otcrop->h);
	return 0;
}

static int is_ischain_isp_cfg(struct is_subdev *leader,
	void *device_data,
	struct is_frame *frame,
	struct is_crop *incrop,
	struct is_crop *otcrop,
	u32 *lindex,
	u32 *hindex,
	u32 *indexes)
{
	int ret = 0;
	struct is_group *group;
	struct is_queue *queue;
	struct param_otf_input *otf_input;
	struct param_otf_output *otf_output;
	struct param_dma_input *dma_input;
	struct param_stripe_input *stripe_input;
	struct param_control *control;
	struct is_crop *scc_incrop;
	struct is_crop *scp_incrop;
	struct is_device_ischain *device;
	u32 hw_format = DMA_INOUT_FORMAT_BAYER;
	u32 hw_bitwidth = DMA_INOUT_BIT_WIDTH_16BIT;
	u32 hw_msb = MSB_OF_3AA_DMA_OUT;
	u32 flag_extra, flag_pixel_size;
	struct is_crop incrop_cfg, otcrop_cfg;
	int stripe_ret = -1;

	device = (struct is_device_ischain *)device_data;

	FIMC_BUG(!leader);
	FIMC_BUG(!device);
	FIMC_BUG(!incrop);
	FIMC_BUG(!lindex);
	FIMC_BUG(!hindex);
	FIMC_BUG(!indexes);

	scc_incrop = scp_incrop = incrop;
	group = &device->group_isp;
	incrop_cfg = *incrop;
	otcrop_cfg = *otcrop;

	queue = GET_SUBDEV_QUEUE(leader);
	if (!queue) {
		merr("queue is NULL", device);
		ret = -EINVAL;
		goto p_err;
	}

	if (!test_bit(IS_GROUP_OTF_INPUT, &group->state)) {
		if (!queue->framecfg.format) {
			merr("format is NULL", device);
			ret = -EINVAL;
			goto p_err;
		}

		hw_format = queue->framecfg.format->hw_format;
		hw_bitwidth = queue->framecfg.format->hw_bitwidth;

		/* pixel type [0:5] : pixel size, [6:7] : extra */
		flag_pixel_size = queue->framecfg.hw_pixeltype & PIXEL_TYPE_SIZE_MASK;
		flag_extra = (queue->framecfg.hw_pixeltype & PIXEL_TYPE_EXTRA_MASK) >> PIXEL_TYPE_EXTRA_SHIFT;

		if (hw_format == DMA_INOUT_FORMAT_BAYER_PACKED
			&& flag_pixel_size == CAMERA_PIXEL_SIZE_13BIT) {
			msdbgs(3, "in_crop[bitwidth: %d -> %d: 13 bit BAYER]\n", device, leader,
				hw_bitwidth, DMA_INOUT_BIT_WIDTH_13BIT);
			hw_msb = MSB_OF_3AA_DMA_OUT + 1;
			hw_bitwidth = DMA_INOUT_BIT_WIDTH_13BIT;
		} else if (hw_format == DMA_INOUT_FORMAT_BAYER) {
			/* consider signed format only */
			if (flag_pixel_size == CAMERA_PIXEL_SIZE_13BIT)
				hw_msb = MSB_OF_3AA_DMA_OUT + 1;
			else if (flag_pixel_size == CAMERA_PIXEL_SIZE_14BIT)
				hw_msb = MSB_OF_3AA_DMA_OUT + 2;
			else if (flag_pixel_size == CAMERA_PIXEL_SIZE_15BIT)
				hw_msb = MSB_OF_3AA_DMA_OUT + 3;
			else
				hw_msb = hw_bitwidth;

			hw_bitwidth = DMA_INOUT_BIT_WIDTH_16BIT;
			msdbgs(3, "in_crop[unpack bitwidth: %d, msb: %d]\n", device, leader,
				hw_bitwidth, hw_msb);
		}

		/* SBWC need to be activated over EVT1 */
		if ((hw_format == DMA_INOUT_FORMAT_BAYER_PACKED) && (flag_extra == COMP || flag_extra == COMP_LOSS)) {
			if (exynos_soc_info.main_rev >= 1) {
				u32 comp_format =
				(flag_extra == COMP) ? DMA_INOUT_FORMAT_BAYER_COMP : DMA_INOUT_FORMAT_BAYER_COMP_LOSSY;
				msdbgs(3, "in_crop[fmt: %d ->%d: BAYER_COMP]\n", device, leader,
					hw_format, comp_format);
				hw_format = comp_format;
			} else {
				mdbgd_ischain("SBWC not supported in EVT%d.%d\n",
					device, exynos_soc_info.main_rev, exynos_soc_info.sub_rev);
			}
		}
	}

	/* Configure Control */
	if (!frame) {
		control = is_itf_g_param(device, NULL, PARAM_ISP_CONTROL);
		control->cmd = CONTROL_COMMAND_START;
		control->bypass = CONTROL_BYPASS_DISABLE;
		*lindex |= LOWBIT_OF(PARAM_ISP_CONTROL);
		*hindex |= HIGHBIT_OF(PARAM_ISP_CONTROL);
		(*indexes)++;
	}

	if (IS_ENABLED(CHAIN_USE_STRIPE_PROCESSING) && frame && frame->stripe_info.region_num) {
		while (stripe_ret)
			stripe_ret = is_ischain_isp_stripe_cfg(leader, frame,
					&incrop_cfg, &otcrop_cfg,
					hw_bitwidth);
	}

	/* ISP */
	otf_input = is_itf_g_param(device, frame, PARAM_ISP_OTF_INPUT);
	if (test_bit(IS_GROUP_OTF_INPUT, &group->state))
		otf_input->cmd = OTF_INPUT_COMMAND_ENABLE;
	else
		otf_input->cmd = OTF_INPUT_COMMAND_DISABLE;
	otf_input->width = incrop_cfg.w;
	otf_input->height = incrop_cfg.h;
	otf_input->format = OTF_INPUT_FORMAT_BAYER;
	otf_input->bayer_crop_offset_x = 0;
	otf_input->bayer_crop_offset_y = 0;
	otf_input->bayer_crop_width = incrop_cfg.w;
	otf_input->bayer_crop_height = incrop_cfg.h;
	*lindex |= LOWBIT_OF(PARAM_ISP_OTF_INPUT);
	*hindex |= HIGHBIT_OF(PARAM_ISP_OTF_INPUT);
	(*indexes)++;

	dma_input = is_itf_g_param(device, frame, PARAM_ISP_VDMA1_INPUT);
	if (test_bit(IS_GROUP_OTF_INPUT, &group->state))
		dma_input->cmd = DMA_INPUT_COMMAND_DISABLE;
	else
		dma_input->cmd = DMA_INPUT_COMMAND_ENABLE;
	dma_input->format = hw_format;
	dma_input->bitwidth = hw_bitwidth;
	dma_input->msb = hw_msb;
	dma_input->width = incrop_cfg.w;
	dma_input->height = incrop_cfg.h;
	dma_input->dma_crop_offset = (incrop_cfg.x << 16) | (incrop_cfg.y << 0);
	dma_input->dma_crop_width = incrop_cfg.w;
	dma_input->dma_crop_height = incrop_cfg.h;
	dma_input->bayer_crop_offset_x = 0;
	dma_input->bayer_crop_offset_y = 0;
	dma_input->bayer_crop_width = incrop_cfg.w;
	dma_input->bayer_crop_height = incrop_cfg.h;
	dma_input->stride_plane0 = incrop->w;
	*lindex |= LOWBIT_OF(PARAM_ISP_VDMA1_INPUT);
	*hindex |= HIGHBIT_OF(PARAM_ISP_VDMA1_INPUT);
	(*indexes)++;

	otf_output = is_itf_g_param(device, frame, PARAM_ISP_OTF_OUTPUT);
	if (test_bit(IS_GROUP_OTF_OUTPUT, &group->state))
		otf_output->cmd = OTF_OUTPUT_COMMAND_ENABLE;
	else
		otf_output->cmd = OTF_OUTPUT_COMMAND_DISABLE;
	otf_output->width = incrop_cfg.w;
	otf_output->height = incrop_cfg.h;
	otf_output->format = OTF_YUV_FORMAT;
	otf_output->bitwidth = OTF_OUTPUT_BIT_WIDTH_12BIT;
	otf_output->order = OTF_INPUT_ORDER_BAYER_GR_BG;
	*lindex |= LOWBIT_OF(PARAM_ISP_OTF_OUTPUT);
	*hindex |= HIGHBIT_OF(PARAM_ISP_OTF_OUTPUT);
	(*indexes)++;

	stripe_input = is_itf_g_param(device, frame, PARAM_ISP_STRIPE_INPUT);
	if (frame && frame->stripe_info.region_num) {
		stripe_input->index = frame->stripe_info.region_id;
		stripe_input->total_count = frame->stripe_info.region_num;
		if (!frame->stripe_info.region_id) {
			stripe_input->left_margin = 0;
			stripe_input->right_margin = STRIPE_MARGIN_WIDTH;
		} else if (frame->stripe_info.region_id < frame->stripe_info.region_num - 1) {
			stripe_input->left_margin = STRIPE_MARGIN_WIDTH;
			stripe_input->right_margin = STRIPE_MARGIN_WIDTH;
		} else {
			stripe_input->left_margin = STRIPE_MARGIN_WIDTH;
			stripe_input->right_margin = 0;
		}
		stripe_input->full_width = leader->input.width;
		stripe_input->full_height = leader->input.height;
	} else {
		stripe_input->index = 0;
		stripe_input->total_count = 0;
		stripe_input->left_margin = 0;
		stripe_input->right_margin = 0;
		stripe_input->full_width = 0;
		stripe_input->full_height = 0;
	}
	*lindex |= LOWBIT_OF(PARAM_ISP_STRIPE_INPUT);
	*hindex |= HIGHBIT_OF(PARAM_ISP_STRIPE_INPUT);
	(*indexes)++;

p_err:
	return ret;
}

static int is_ischain_isp_tag(struct is_subdev *subdev,
	void *device_data,
	struct is_frame *frame,
	struct camera2_node *node)
{
	int ret = 0;
	struct is_group *group;
	struct isp_param *isp_param;
	struct is_crop inparm;
	struct is_crop *incrop, *otcrop;
	struct is_subdev *leader;
	struct is_device_ischain *device;
	struct is_fmt *format, *tmp_format;
	struct is_queue *queue;
	u32 fmt_pixelsize;
	bool chg_fmt_size = false;
	u32 lindex, hindex, indexes;

	device = (struct is_device_ischain *)device_data;

	FIMC_BUG(!subdev);
	FIMC_BUG(!device);
	FIMC_BUG(!device->is_region);
	FIMC_BUG(!frame);

	mdbgs_ischain(4, "ISP TAG\n", device);

	incrop = (struct is_crop *)node->input.cropRegion;
	otcrop = (struct is_crop *)node->output.cropRegion;

	group = &device->group_isp;
	leader = subdev->leader;
	lindex = hindex = indexes = 0;
	isp_param = &device->is_region->parameter.isp;

	if (test_bit(IS_GROUP_OTF_INPUT, &group->state)) {
		inparm.x = 0;
		inparm.y = 0;
		inparm.w = isp_param->otf_input.width;
		inparm.h = isp_param->otf_input.height;
	} else {
		inparm.x = 0;
		inparm.y = 0;
		inparm.w = isp_param->vdma1_input.width;
		inparm.h = isp_param->vdma1_input.height;
	}

	if (IS_NULL_CROP(incrop))
		*incrop = inparm;

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

	/* per-frame pixelformat control for changing fmt */
	format = queue->framecfg.format;
	if (node->pixelformat && format->pixelformat != node->pixelformat) {
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
		chg_fmt_size = true;
	}

	fmt_pixelsize = queue->framecfg.hw_pixeltype & PIXEL_TYPE_SIZE_MASK;
	if (node->pixelsize && node->pixelsize != fmt_pixelsize) {
		mdbg_pframe("pixelsize is changed(%d->%d)\n",
			device, subdev, frame,
			fmt_pixelsize, node->pixelsize);
		queue->framecfg.hw_pixeltype =
			(queue->framecfg.hw_pixeltype & PIXEL_TYPE_EXTRA_MASK)
			| (node->pixelsize & PIXEL_TYPE_SIZE_MASK);

		chg_fmt_size = true;
	}

	if (!COMPARE_CROP(incrop, &inparm) ||
		chg_fmt_size ||
		CHECK_STRIPE_CFG(&frame->stripe_info) ||
		test_bit(IS_SUBDEV_FORCE_SET, &leader->state)) {
		ret = is_ischain_isp_cfg(subdev,
			device,
			frame,
			incrop,
			otcrop,
			&lindex,
			&hindex,
			&indexes);
		if (ret) {
			merr("is_ischain_isp_cfg is fail(%d)", device, ret);
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

const struct is_subdev_ops is_subdev_isp_ops = {
	.bypass			= NULL,
	.cfg			= is_ischain_isp_cfg,
	.tag			= is_ischain_isp_tag,
};
