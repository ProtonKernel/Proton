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

#include <linux/videodev2_exynos_media.h>
#include "is-device-ischain.h"
#include "is-device-sensor.h"
#include "is-subdev-ctrl.h"
#include "is-config.h"
#include "is-param.h"
#include "is-video.h"
#include "is-type.h"

static const struct camera2_node init_ypp_out_node = {
	.request = 1,
	.vid = IS_VIDEO_YPP_NUM,
	.input = {
		.cropRegion = {0, 0, 0, 0}
	},
	.output = {
		.cropRegion = {0, 0, 0, 0}

	},
	.pixelformat = V4L2_PIX_FMT_NV16M_P210,
	.flags = 2
};
static const struct camera2_node init_ypp_cap_node_nr_ds = {
	.request = 1,
	.vid = IS_VIDEO_YND_NUM,
	.input = {
		.cropRegion = {0, 0, 0, 0}
	},
	.output = {
		.cropRegion = {0, 0, 0, 0}

	},
	.pixelformat = V4L2_PIX_FMT_YUV420,
	.flags = 2
};
static const struct camera2_node init_ypp_cap_node_noise = {
	.request = 1,
	.vid = IS_VIDEO_YNS_NUM,
	.input = {
		.cropRegion = {0, 0, 0, 0}
	},
	.output = {
		.cropRegion = {0, 0, 0, 0}

	},
	.pixelformat = V4L2_PIX_FMT_GREY,
	.flags = 0
};
static const struct camera2_node init_ypp_cap_node_drc_gain = {
	.request = 1,
	.vid = IS_VIDEO_YDG_NUM,
	.input = {
		.cropRegion = {0, 0, 0, 0}
	},
	.output = {
		.cropRegion = {0, 0, 0, 0}

	},
	.pixelformat = V4L2_PIX_FMT_GREY,
	.flags = 0
};
static const struct camera2_node init_ypp_cap_node_svhist = {
	.request = 1,
	.vid = IS_VIDEO_YSH_NUM,
	.input = {
		.cropRegion = {0, 0, 2048, 144}
	},
	.output = {
		.cropRegion = {0, 0, 2048, 144}

	},
	.pixelformat = V4L2_PIX_FMT_GREY,
	.flags = 0,
	.width = 2048,
	.height = 144
};

void __ypp_in_calc_stripe_start_pos(struct is_frame *frame, struct is_crop *incrop,
	u32 region_num, u32 h_pix_num[])
{
	u32 region_id;
	u32 stripe_w, cur_h_pix_num = 0;

	for (region_id = 0; region_id < region_num; ++region_id) {
		if (!region_id) {
			/* Left region */
			if (frame->shot->uctl.reserved[region_id]) {
				stripe_w = frame->shot->uctl.reserved[region_id];
			} else {
				/* Stripe width should be 4 align because of 4 ppc */
				stripe_w = ALIGN(incrop->w / region_num, 4);
				stripe_w = ALIGN(stripe_w, STRIPE_WIDTH_ALIGN) - (STRIPE_WIDTH_ALIGN / 2);
			}
		} else if (region_id < region_num - 1) {
			/* Middle region */
			if (frame->shot->uctl.reserved[region_id]) {
				stripe_w = frame->shot->uctl.reserved[region_id] - cur_h_pix_num;
			} else {
				stripe_w = ALIGN((incrop->w - cur_h_pix_num) / (region_num - region_id), 4);
				stripe_w = ALIGN_UPDOWN_STRIPE_WIDTH(stripe_w, STRIPE_WIDTH_ALIGN);
			}
		} else {
			/* Right region */
			stripe_w = incrop->w - cur_h_pix_num;
		}
		cur_h_pix_num += stripe_w;
		h_pix_num[region_id] = cur_h_pix_num;
	}
}

static int is_ischain_ypp_stripe_cfg(struct is_subdev *subdev,
		struct is_frame *frame,
		struct is_crop *incrop,
		struct is_crop *otcrop,
		u32 bitwidth)
{
	struct is_group *group;
	u32 stripe_w, dma_offset = 0;
	u32 region_id = frame->stripe_info.region_id;
	struct camera2_stream *stream = (struct camera2_stream *) frame->shot_ext;
	group = frame->group;
	/* Input crop configuration & RDMA offset configuration*/
	if (!region_id) {
		/* Left region */
		/* Stripe width should be 4 align because of 4 ppc */
		if (stream->stripe_h_pix_nums[region_id]) {
			stripe_w = stream->stripe_h_pix_nums[region_id];
		}
		else {
			stripe_w = ALIGN(incrop->w / frame->stripe_info.region_num, 4);
			/* For SBWC Lossless, width should be 512 align with margin */
			stripe_w = ALIGN(stripe_w, STRIPE_WIDTH_ALIGN) - (STRIPE_WIDTH_ALIGN / 2);
		}
		if (stripe_w == 0) {
			msrdbgs(3, "Skip current stripe[#%d] region because stripe_width is too small(%d)\n", subdev, subdev, frame,
									frame->stripe_info.region_id, stripe_w);
			frame->stripe_info.region_id++;
			return -EAGAIN;
		}
		frame->stripe_info.in.h_pix_num[region_id] = stripe_w;
		frame->stripe_info.region_base_addr[0] = frame->dvaddr_buffer[0];
		frame->stripe_info.region_base_addr[1] = frame->dvaddr_buffer[1];
	} else if (region_id < frame->stripe_info.region_num - 1) {
		/* Middle region */
		if (stream->stripe_h_pix_nums[region_id])
			stripe_w = stream->stripe_h_pix_nums[region_id] - frame->stripe_info.in.h_pix_num[region_id - 1];
		else {
			stripe_w = ALIGN((incrop->w - frame->stripe_info.in.h_pix_num[region_id - 1]) / (frame->stripe_info.region_num - frame->stripe_info.region_id), 4);
			stripe_w = ALIGN_UPDOWN_STRIPE_WIDTH(stripe_w, STRIPE_WIDTH_ALIGN);
		}
		if (stripe_w == 0) {
			msrdbgs(3, "Skip current stripe[#%d] region because stripe_width is too small(%d)\n", subdev, subdev, frame,
									frame->stripe_info.region_id, stripe_w);
			frame->stripe_info.region_id++;
			return -EAGAIN;
		}

		/* Consider RDMA offset. */
		if (!test_bit(IS_GROUP_OTF_INPUT, &group->state)) {
			if (stream->stripe_h_pix_nums[region_id]) {
				dma_offset = frame->stripe_info.in.h_pix_num[region_id - 1];
				dma_offset += STRIPE_MARGIN_WIDTH * ((2 * (region_id - 1)) + 1);
				dma_offset = ((dma_offset * bitwidth * incrop->h) / BITS_PER_BYTE);
			} else {
				dma_offset = frame->stripe_info.in.h_pix_num[region_id - 1] - STRIPE_MARGIN_WIDTH;
				dma_offset *= bitwidth / BITS_PER_BYTE;
			}
		}
		frame->stripe_info.in.h_pix_num[region_id] = frame->stripe_info.in.h_pix_num[region_id - 1] + stripe_w;
		stripe_w += STRIPE_MARGIN_WIDTH;
	} else {
		/* Right region */
		stripe_w = incrop->w - frame->stripe_info.in.h_pix_num[region_id - 1];

		/* Consider RDMA offset. */
		if (!test_bit(IS_GROUP_OTF_INPUT, &group->state)) {
			if (stream->stripe_h_pix_nums[region_id]) {
				dma_offset = frame->stripe_info.in.h_pix_num[region_id - 1];
				dma_offset += STRIPE_MARGIN_WIDTH * ((2 * (region_id - 1)) + 1);
				dma_offset = ((dma_offset * bitwidth * incrop->h) / BITS_PER_BYTE);
			} else {
				dma_offset = frame->stripe_info.in.h_pix_num[region_id - 1] - STRIPE_MARGIN_WIDTH;
				dma_offset *= bitwidth / BITS_PER_BYTE;
			}
		}
		frame->stripe_info.in.h_pix_num[region_id] = frame->stripe_info.in.h_pix_num[region_id - 1] + stripe_w;
	}
	/* Add stripe processing horizontal margin into each region. */
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
	frame->dvaddr_buffer[1] = frame->stripe_info.region_base_addr[1] + dma_offset;

	msrdbgs(3, "stripe_in_crop[%d][%d, %d, %d, %d] offset %x\n", subdev, subdev, frame,
			region_id,
			incrop->x, incrop->y, incrop->w, incrop->h, dma_offset);
	msrdbgs(3, "stripe_ot_crop[%d][%d, %d, %d, %d]\n", subdev, subdev, frame,
			region_id,
			otcrop->x, otcrop->y, otcrop->w, otcrop->h);
	return 0;
}

#ifdef ENABLE_LOGICAL_VIDEO_NODE
static int __ypp_in_stripe_cfg(struct is_frame *frame,
		struct camera2_node *node,
		struct fimc_is_stripe_info *stripe_info,
		struct is_crop *incrop,
		struct is_crop *otcrop,
		u32 bitwidth,
		u32 param_width)
{
	struct is_group *group;
	u32 stripe_w, temp_stripe_w = 0;
	u32 dma_offset = 0, main_dma_offset = 0;
	u32 i, j;
	u32 region_id = stripe_info->region_id;
	u32 width, height;
	group = frame->group;
	/* Input crop configuration & RDMA offset configuration*/
	if (!region_id) {
		/* Left region */
		__ypp_in_calc_stripe_start_pos(frame, incrop, frame->stripe_info.region_num, frame->stripe_info.in.h_pix_num);
		stripe_w = frame->stripe_info.in.h_pix_num[region_id];
		if (!frame->shot->uctl.reserved[region_id])
			mlvinfo("[F%d] Warning: frame->shot->uctl.reserved[region_id]: %d, %d\n",
					frame, node->vid, frame->fcount,
					frame->shot->uctl.reserved[region_id], stripe_w);
		if (stripe_w == 0) {
			mlvdbgs(3, "[F%d] Skip current stripe[#%d] region because stripe_width is too small(%d)\n",
				frame, node->vid, frame->fcount, region_id, stripe_w);
			frame->stripe_info.region_id++;
			return -EAGAIN;
		}
		frame->stripe_info.region_base_addr[0] = frame->dvaddr_buffer[0];
		frame->stripe_info.region_base_addr[1] = frame->dvaddr_buffer[1];

		for (i = 0; i < CAPTURE_NODE_MAX; i++) {
			if (frame->cap_node.sframe[i].id == IS_VIDEO_YSH_NUM
				|| !frame->cap_node.sframe[i].id)
				continue;
			if ((frame->cap_node.sframe[i].id >= IS_VIDEO_M0P_NUM) &&
				(frame->cap_node.sframe[i].id <= IS_VIDEO_M5P_NUM))
				continue;

			for (j = 0; j < frame->cap_node.sframe[i].num_planes; j++)
				frame->cap_node.sframe[i].stripe_info.region_base_addr[j]
					= frame->cap_node.sframe[i].dva[j];
		}
	} else if (region_id < frame->stripe_info.region_num - 1) {
		/* Middle region */
		stripe_w = frame->stripe_info.in.h_pix_num[region_id] - frame->stripe_info.in.h_pix_num[region_id - 1];
		if (stripe_w == 0) {
			mlvdbgs(3, "[F%d] Skip current stripe[#%d] region because stripe_width is too small(%d)\n",
				frame, node->vid, frame->fcount,
				frame->stripe_info.region_id, stripe_w);
			frame->stripe_info.region_id++;
			return -EAGAIN;
		}

		/* Consider RDMA offset. */
		if (!test_bit(IS_GROUP_OTF_INPUT, &group->state)) {
			if (frame->shot->uctl.reserved[region_id]) {
				dma_offset = frame->stripe_info.in.h_pix_num[region_id - 1];
				dma_offset += STRIPE_MARGIN_WIDTH * ((2 * (region_id - 1)) + 1);
				dma_offset = ALIGN(dma_offset * bitwidth / BITS_PER_BYTE, 16) * incrop->h;
			} else {
				dma_offset = frame->stripe_info.in.h_pix_num[region_id - 1] - STRIPE_MARGIN_WIDTH;
				dma_offset = ALIGN(dma_offset * bitwidth / BITS_PER_BYTE, 16);
			}
		}
		temp_stripe_w = stripe_w;
		stripe_w += STRIPE_MARGIN_WIDTH;
	} else {
		/* Right region */
		stripe_w = incrop->w - frame->stripe_info.in.h_pix_num[region_id - 1];

		/* Consider RDMA offset. */
		if (!test_bit(IS_GROUP_OTF_INPUT, &group->state)) {
			if (frame->shot->uctl.reserved[region_id]) {
				dma_offset = frame->stripe_info.in.h_pix_num[region_id - 1];
				dma_offset += STRIPE_MARGIN_WIDTH * ((2 * (region_id - 1)) + 1);
				dma_offset = ALIGN(dma_offset * bitwidth / BITS_PER_BYTE, 16) * incrop->h;
			} else {
				dma_offset = frame->stripe_info.in.h_pix_num[region_id - 1] - STRIPE_MARGIN_WIDTH;
				dma_offset = ALIGN(dma_offset * bitwidth / BITS_PER_BYTE, 16);
				mlvinfo("[F%d] Warning: frame->shot->uctl.reserved[region_id]: %d, %d\n",
					frame, node->vid, frame->fcount,
					frame->shot->uctl.reserved[region_id], stripe_w);
			}
		}
		temp_stripe_w = stripe_w;
	}
	main_dma_offset = dma_offset;

	/* Add stripe processing horizontal margin into each region. */
	stripe_w += STRIPE_MARGIN_WIDTH;
	frame->out_node.sframe[0].stripe_info.stripe_w = stripe_w;

	frame->dvaddr_buffer[0] = frame->stripe_info.region_base_addr[0] + dma_offset;
	frame->dvaddr_buffer[1] = frame->stripe_info.region_base_addr[1] + dma_offset;

	for (i = 0; i < CAPTURE_NODE_MAX; i++) {
		if (frame->cap_node.sframe[i].id == IS_VIDEO_YSH_NUM
			|| !frame->cap_node.sframe[i].id)
			continue;
		if ((frame->cap_node.sframe[i].id >= IS_VIDEO_M0P_NUM) &&
			(frame->cap_node.sframe[i].id <= IS_VIDEO_M5P_NUM))
			continue;

		switch (frame->cap_node.sframe[i].id) {
		case IS_VIDEO_YNS_NUM:
			for (j = 0; j < frame->cap_node.sframe[i].num_planes; j++)
				frame->cap_node.sframe[i].dva[j] =
					frame->cap_node.sframe[i].stripe_info.region_base_addr[j] + main_dma_offset;
			break;
		case IS_VIDEO_YDG_NUM:
			stripe_w = ((stripe_w + 2) >> 2) + 1;
			if (region_id) {
				if (frame->shot->uctl.reserved[region_id]) {
					width = frame->stripe_info.in.h_pix_num[region_id - 1];
					width += STRIPE_MARGIN_WIDTH * ((2 * (region_id - 1)) + 1);
					width = ((width + 2) >> 2) + 1;
					height = ((incrop->h + 2) >> 2) + 1;
					dma_offset = height * ALIGN(width * 8 / BITS_PER_BYTE, 16);
				} else {
					width = frame->stripe_info.in.h_pix_num[region_id - 1] - STRIPE_MARGIN_WIDTH;
					width = ((width + 2) >> 2) + 1;
					dma_offset = ALIGN(width * 8 / BITS_PER_BYTE, 16);
				}
			}
			for (j = 0; j < frame->cap_node.sframe[i].num_planes; j++) {
				frame->cap_node.sframe[i].dva[j] =
					frame->cap_node.sframe[i].stripe_info.region_base_addr[j] + dma_offset;
			}
			break;
		case IS_VIDEO_YND_NUM:
			stripe_w = ((stripe_w + 2) >> 2) + 1;
			for (j = 0; j < frame->cap_node.sframe[i].num_planes; j++) {
				if (region_id) {
					if (frame->shot->uctl.reserved[region_id]) {
						width = frame->stripe_info.in.h_pix_num[region_id - 1];
						width += STRIPE_MARGIN_WIDTH * ((2 * (region_id - 1)) + 1);
						if (j == 0) {
							width = ((width + 2) >> 2) + 1;
							height = ((incrop->h + 2) >> 2) + 1;
						} else {
							width = (((width + 4) >> 3) + 1) << 1;
							height = ((incrop->h + 7) >> 3) + 1;
						}
						dma_offset = height * ALIGN(width * bitwidth / BITS_PER_BYTE, 16);
					} else {
						width = frame->stripe_info.in.h_pix_num[region_id - 1] - STRIPE_MARGIN_WIDTH;
						if (j == 0)
							width = ((width + 2) >> 2) + 1;
						else
							width = (((width + 4) >> 3) + 1) << 1;

						dma_offset = ALIGN(width * bitwidth / BITS_PER_BYTE, 16);
					}
				}
				frame->cap_node.sframe[i].dva[j] = frame->cap_node.sframe[i].stripe_info.region_base_addr[j] + dma_offset;
			}
			break;
		default:
			err("Invalid video id(%d)", frame->cap_node.sframe[i].id);
			return -EINVAL;
		}
		frame->cap_node.sframe[i].stripe_info.stripe_w = stripe_w;
	}
	frame->stripe_info.out.h_pix_num[region_id] = frame->stripe_info.in.h_pix_num[region_id];
	mlvdbgs(3, "[F%d] stripe_in_crop[%d][%d, %d, %d, %d] offset %x, %d\n", frame, node->vid,
			frame->fcount, frame->stripe_info.region_id,
			incrop->x, incrop->y, incrop->w, incrop->h, dma_offset,
			frame->shot->uctl.reserved[region_id]);
	mlvdbgs(3, "[F%d] stripe_ot_crop[%d][%d, %d, %d, %d]\n", frame, node->vid,
			frame->fcount, frame->stripe_info.region_id,
			otcrop->x, otcrop->y, otcrop->w, otcrop->h);
	mlvdbgs(3, "[F%d] [%d] frame->dvaddr_buffer: 0x%x, 0x%x\n", frame, node->vid,
			frame->fcount, frame->stripe_info.region_id,
			frame->dvaddr_buffer[0],
			frame->dvaddr_buffer[1]);
	return 0;
}

static int __ischain_ypp_set_votf_node(struct camera2_node *node,
	struct is_frame *ldr_frame, struct is_sub_frame *sub_frame)
{
	struct camera2_node *ldr_frame_node;
	int ret = 0;
	int i = 0;

	ldr_frame_node = &ldr_frame->shot_ext->node_group.leader;

	switch (sub_frame->id) {
	case IS_VIDEO_YPP_NUM:
		memcpy(node, &init_ypp_out_node, sizeof(struct camera2_node));
		for (i = 0; i < 4; i++)
			node->output.cropRegion[i] = node->input.cropRegion[i] =
				ldr_frame_node->output.cropRegion[i];
		node->flags = sub_frame->hw_pixeltype;
		break;
	case IS_VIDEO_YND_NUM:
		memcpy(node, &init_ypp_cap_node_nr_ds, sizeof(struct camera2_node));
		for (i = 0; i < 4; i++)
			node->output.cropRegion[i] = node->input.cropRegion[i] =
				((ldr_frame_node->output.cropRegion[i] + 2) >> 2) + 1;
		break;
	case IS_VIDEO_YDG_NUM:
		memcpy(node, &init_ypp_cap_node_drc_gain, sizeof(struct camera2_node));
		for (i = 0; i < 4; i++)
			node->output.cropRegion[i] = node->input.cropRegion[i] =
				((ldr_frame_node->output.cropRegion[i] + 2) >> 2) + 1;
		break;
	case IS_VIDEO_YSH_NUM:
		memcpy(node, &init_ypp_cap_node_svhist, sizeof(struct camera2_node));
		break;
	case IS_VIDEO_YNS_NUM:
		memcpy(node, &init_ypp_cap_node_noise, sizeof(struct camera2_node));
		for (i = 0; i < 4; i++)
			node->output.cropRegion[i] = node->input.cropRegion[i] =
				ldr_frame_node->output.cropRegion[i];
		node->flags = sub_frame->hw_pixeltype;
		break;
	default:
		ret = -EINVAL;
		break;
	}

	return ret;
}

static int __ischain_ypp_slot(struct camera2_node *node, u32 *pindex)
{
	int ret = 0;

	switch (node->vid) {
	case IS_VIDEO_YND_NUM:
		*pindex = PARAM_YPP_DMA_LV2_INPUT;
		ret = 1;
		break;
	case IS_VIDEO_YDG_NUM:
		*pindex = PARAM_YPP_DMA_DRC_INPUT;
		ret = 1;
		break;
	case IS_VIDEO_YSH_NUM:
		*pindex = PARAM_YPP_DMA_HIST_INPUT;
		ret = 1;
		break;
	case IS_VIDEO_YNS_NUM:
		*pindex = PARAM_YPP_DMA_NOISE_INPUT;
		ret = 1;
		break;
	default:
		ret = 0;
		break;
	}

	return ret;
}

static int __ypp_dma_in_cfg(struct is_device_ischain *device,
	struct is_frame *frame,
	struct camera2_node *node,
	struct fimc_is_stripe_info *stripe_info,
	u32 pindex,
	u32 *lindex,
	u32 *hindex,
	u32 *indexes)
{
	struct is_fmt *fmt;
	struct param_dma_input *dma_input;
	struct param_otf_output *otf_output;
	struct param_stripe_input *stripe_input;
	struct is_crop *incrop, *otcrop;
	u32 hw_format = DMA_INOUT_FORMAT_YUV422;
	u32 hw_bitwidth = DMA_INOUT_BIT_WIDTH_10BIT;
	u32 hw_msb = (DMA_INOUT_BIT_WIDTH_10BIT - 1);
	u32 hw_sbwc = DMA_INPUT_SBWC_DISABLE;
	u32 hw_order, hw_plane = 2;
	u32 width, flag_extra;
	int i;
	int ret = 0;
	int stripe_ret = -1;

	FIMC_BUG(!frame);
	FIMC_BUG(!node);
	FIMC_BUG(!lindex);
	FIMC_BUG(!hindex);
	FIMC_BUG(!indexes);

	dma_input = is_itf_g_param(device, frame, pindex);
	if (dma_input->cmd != node->request)
		mlvinfo("[F%d] RDMA enable: %d -> %d\n", device,
			node->vid, frame->fcount,
			dma_input->cmd, node->request);
	dma_input->cmd = DMA_INPUT_COMMAND_DISABLE;
	*lindex |= LOWBIT_OF(pindex);
	*hindex |= HIGHBIT_OF(pindex);
	(*indexes)++;

	if (!node->request)
		return 0;
	incrop = (struct is_crop *)node->input.cropRegion;
	if (IS_NULL_CROP(incrop)) {
		merr("incrop is NULL", device);
		return -EINVAL;
	}

	otcrop = (struct is_crop *)node->output.cropRegion;
	if (IS_NULL_CROP(otcrop)) {
		merr("otcrop is NULL", device);
		return -EINVAL;
	}

	width = incrop->w;

	fmt = is_find_format(node->pixelformat, node->flags);
	if (!fmt) {
		merr("pixel format(0x%x) is not found", device, node->pixelformat);
		return -EINVAL;
	}

	hw_format = fmt->hw_format;
	/* FIXME */
	/* hw_bitwidth = fmt->hw_bitwidth; */
	hw_msb = hw_bitwidth - 1;
	hw_order = fmt->hw_order;
	hw_plane = fmt->hw_plane;
	flag_extra = (node->flags & PIXEL_TYPE_EXTRA_MASK)
			>> PIXEL_TYPE_EXTRA_SHIFT;
	if (flag_extra)
		hw_sbwc = (SBWC_BASE_ALIGN_MASK_LLC_ON | flag_extra);

	if (test_bit(IS_GROUP_OTF_INPUT, &device->group_ypp.state)) {
		if (test_bit(IS_GROUP_VOTF_INPUT, &device->group_ypp.state)) {
			dma_input->cmd = DMA_INPUT_COMMAND_ENABLE;
			dma_input->v_otf_enable = OTF_INPUT_COMMAND_ENABLE;
		} else {
			dma_input->cmd = DMA_INPUT_COMMAND_DISABLE;
			dma_input->v_otf_enable = OTF_INPUT_COMMAND_DISABLE;
		}
	} else {
		dma_input->cmd = DMA_INPUT_COMMAND_ENABLE;
		dma_input->v_otf_enable = OTF_INPUT_COMMAND_DISABLE;
	}

	if (IS_ENABLED(CHAIN_USE_STRIPE_PROCESSING)
		&& frame && stripe_info->region_num) {
		switch (node->vid) {
		/* RDMA */
		case IS_VIDEO_YPP_NUM:
			while (stripe_ret) {
				if (dma_input->v_otf_enable == OTF_INPUT_COMMAND_ENABLE) {
					mlvdbgs(3, "[F%d] VOTF Frame, YPP stripe configuration skip!!\n", frame, node->vid,
						frame->fcount);
					break;
				}
				stripe_ret = __ypp_in_stripe_cfg(frame,
					node, stripe_info, incrop, otcrop, hw_bitwidth,
					dma_input->width);
			}
			if (frame->out_node.sframe[0].stripe_info.stripe_w != 0)
				width = frame->out_node.sframe[0].stripe_info.stripe_w;
			break;
		case IS_VIDEO_YND_NUM:
		case IS_VIDEO_YDG_NUM:
		case IS_VIDEO_YNS_NUM:
			for (i = 0; i < CAPTURE_NODE_MAX; i++) {
				if (frame->cap_node.sframe[i].id == node->vid
					&& frame->cap_node.sframe[i].stripe_info.stripe_w != 0)
					width = frame->cap_node.sframe[i].stripe_info.stripe_w;
			}
			break;
		case IS_VIDEO_YSH_NUM:
			width = 2048;
			break;
		default:
			break;
		}
	}

	if (dma_input->sbwc_type != hw_sbwc)
		mlvinfo("[F%d]RDMA sbwc_type: %d -> %d\n", device,
			node->vid, frame->fcount,
			dma_input->sbwc_type, hw_sbwc);
	if (!stripe_info->region_num && ((dma_input->width != width) ||
			(dma_input->height != incrop->h)))
		mlvinfo("[F%d]RDMA incrop[%d, %d, %d, %d]\n", device,
			node->vid, frame->fcount,
			incrop->x, incrop->y, incrop->w, incrop->h);

	/* DRC and SV hist have own format enum */
	switch (node->vid) {
	/* RDMA */
	case IS_VIDEO_YND_NUM:
		hw_plane = DMA_INOUT_PLANE_2;
		break;
	case IS_VIDEO_YDG_NUM:
		hw_format = DMA_INOUT_FORMAT_DRCGAIN;
		hw_bitwidth = DMA_INOUT_BIT_WIDTH_8BIT;
		hw_msb = hw_bitwidth - 1;
		break;
	case IS_VIDEO_YSH_NUM:
		hw_format = DMA_INOUT_FORMAT_SVHIST;
		hw_plane = DMA_INOUT_PLANE_1;
		break;
	case IS_VIDEO_YNS_NUM:
		hw_format = DMA_INOUT_FORMAT_NOISE;
		hw_bitwidth = DMA_INOUT_BIT_WIDTH_10BIT;
		hw_plane = DMA_INOUT_PLANE_1;
		break;
	default:
		if (dma_input->bitwidth != hw_bitwidth)
			mlvinfo("[F%d]RDMA bitwidth: %d -> %d\n", device,
				node->vid, frame->fcount,
				dma_input->bitwidth, hw_bitwidth);

		if (dma_input->format != hw_format)
			mlvinfo("[F%d]RDMA format: %d -> %d\n", device,
				node->vid, frame->fcount,
				dma_input->format, hw_format);
		break;
	}

	dma_input->sbwc_type= hw_sbwc;
	dma_input->format = hw_format;
	dma_input->bitwidth = hw_bitwidth;
	dma_input->msb = hw_msb;
	dma_input->order = hw_order;
	dma_input->sbwc_type = hw_sbwc;
	dma_input->plane = hw_plane;
	dma_input->width = width;
	dma_input->height = incrop->h;
	dma_input->dma_crop_offset = (incrop->x << 16) | (incrop->y << 0);
	dma_input->dma_crop_width = width;
	dma_input->dma_crop_height = incrop->h;
	dma_input->bayer_crop_offset_x = incrop->x;
	dma_input->bayer_crop_offset_y = incrop->y;
	dma_input->bayer_crop_width = width;
	dma_input->bayer_crop_height = incrop->h;
	dma_input->stride_plane0 = incrop->w;
	dma_input->stride_plane1 = incrop->w;
	dma_input->stride_plane2 = incrop->w;
	dma_input->v_otf_token_line = VOTF_TOKEN_LINE;

	/* otf input and output should be set for ISP */
	if (node->vid != IS_VIDEO_YPP_NUM)
		return ret;

	otf_output = is_itf_g_param(device, frame, PARAM_YPP_OTF_OUTPUT);
	otf_output->cmd = OTF_OUTPUT_COMMAND_ENABLE;
	otf_output->width = width;
	otf_output->height = incrop->h;
	otf_output->format = OTF_OUTPUT_FORMAT_YUV422;
	otf_output->bitwidth = OTF_OUTPUT_BIT_WIDTH_10BIT;
	otf_output->order = OTF_INPUT_ORDER_BAYER_GR_BG;
	*lindex |= LOWBIT_OF(PARAM_YPP_OTF_OUTPUT);
	*hindex |= HIGHBIT_OF(PARAM_YPP_OTF_OUTPUT);
	(*indexes)++;

	stripe_input = is_itf_g_param(device, frame, PARAM_YPP_STRIPE_INPUT);
	if (frame && stripe_info->region_num) {
		stripe_input->index = stripe_info->region_id;
		stripe_input->total_count = stripe_info->region_num;
		if (!stripe_info->region_id) {
			stripe_input->left_margin = 0;
			stripe_input->right_margin = STRIPE_MARGIN_WIDTH;
			stripe_input->stripe_roi_start_pos_x[0] = 0;
			for (i = 1; i < stripe_info->region_num; ++i)
				stripe_input->stripe_roi_start_pos_x[i] = stripe_info->in.h_pix_num[i - 1];
		} else if (stripe_info->region_id < stripe_info->region_num - 1) {
			stripe_input->left_margin = STRIPE_MARGIN_WIDTH;
			stripe_input->right_margin = STRIPE_MARGIN_WIDTH;
		} else {
			stripe_input->left_margin = STRIPE_MARGIN_WIDTH;
			stripe_input->right_margin = 0;
		}
		stripe_input->full_width = incrop->w;
		stripe_input->full_height = incrop->h;
	} else {
		stripe_input->index = 0;
		stripe_input->total_count = 0;
		stripe_input->left_margin = 0;
		stripe_input->right_margin = 0;
		stripe_input->full_width = 0;
		stripe_input->full_height = 0;
	}
	*lindex |= LOWBIT_OF(PARAM_YPP_STRIPE_INPUT);
	*hindex |= HIGHBIT_OF(PARAM_YPP_STRIPE_INPUT);
	(*indexes)++;

	return ret;
}
#endif

static int is_ischain_ypp_cfg(struct is_subdev *leader,
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
	struct param_dma_input *dma_input;
	struct param_otf_output *otf_output;
	struct param_stripe_input *stripe_input;
	struct param_control *control;
	struct is_device_ischain *device;
	struct is_device_sensor *sensor;
	u32 hw_format = DMA_INOUT_FORMAT_YUV422;
	u32 hw_bitwidth = DMA_INOUT_BIT_WIDTH_10BIT;
	u32 hw_order = DMA_INOUT_ORDER_YCbYCr;
	u32 hw_plane = 2;
	u32 flag_extra;
	u32 hw_sbwc = 0;
	bool chg_format = false;
	struct is_crop incrop_cfg, otcrop_cfg;
	int stripe_ret = -1;

	device = (struct is_device_ischain *)device_data;

	FIMC_BUG(!leader);
	FIMC_BUG(!device);
	FIMC_BUG(!device->sensor);
	FIMC_BUG(!incrop);
	FIMC_BUG(!lindex);
	FIMC_BUG(!hindex);
	FIMC_BUG(!indexes);

	sensor = device->sensor;
	group = &device->group_ypp;
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
		hw_bitwidth = queue->framecfg.format->hw_bitwidth; /* memory width per pixel */
		hw_order = queue->framecfg.format->hw_order;
		hw_plane = queue->framecfg.format->hw_plane;
		flag_extra = (queue->framecfg.hw_pixeltype
				& PIXEL_TYPE_EXTRA_MASK)
				>> PIXEL_TYPE_EXTRA_SHIFT;

		if (flag_extra) {
			hw_sbwc = (SBWC_BASE_ALIGN_MASK_LLC_ON | flag_extra);
			chg_format = true;
		}

		if (chg_format)
			msdbgs(3, "in_crop[bitwidth %d sbwc 0x%x]\n",
					device, leader,
					hw_bitwidth,
					hw_sbwc);
	}

	/* Configure Conrtol */
	if (!frame) {
		control = is_itf_g_param(device, NULL, PARAM_YPP_CONTROL);
		if (test_bit(IS_GROUP_START, &group->state)) {
			control->cmd = CONTROL_COMMAND_START;
			control->bypass = CONTROL_BYPASS_DISABLE;
		} else {
			control->cmd = CONTROL_COMMAND_STOP;
			control->bypass = CONTROL_BYPASS_DISABLE;
		}
		*lindex |= LOWBIT_OF(PARAM_YPP_CONTROL);
		*hindex |= HIGHBIT_OF(PARAM_YPP_CONTROL);
		(*indexes)++;
	}

	if (IS_ENABLED(CHAIN_USE_STRIPE_PROCESSING) && frame && frame->stripe_info.region_num) {
		while(stripe_ret)
			stripe_ret =is_ischain_ypp_stripe_cfg(leader, frame,
				&incrop_cfg, &otcrop_cfg,
				hw_bitwidth);
	}

	dma_input = is_itf_g_param(device, frame, PARAM_YPP_DMA_INPUT);
	if (test_bit(IS_GROUP_OTF_INPUT, &group->state)) {
		if (test_bit(IS_GROUP_VOTF_INPUT, &group->state)) {
			dma_input->cmd = DMA_INPUT_COMMAND_ENABLE;
			dma_input->v_otf_enable = OTF_INPUT_COMMAND_ENABLE;
		} else {
			dma_input->cmd = DMA_INPUT_COMMAND_DISABLE;
			dma_input->v_otf_enable = OTF_INPUT_COMMAND_DISABLE;
		}
	} else {
		dma_input->cmd = DMA_INPUT_COMMAND_ENABLE;
		dma_input->v_otf_enable = OTF_INPUT_COMMAND_DISABLE;
	}

	dma_input->format = hw_format;
	dma_input->bitwidth = hw_bitwidth;
	dma_input->msb = hw_bitwidth - 1;
	dma_input->order = hw_order;
	dma_input->sbwc_type = hw_sbwc;
	dma_input->plane = hw_plane;
	dma_input->width = incrop_cfg.w;
	dma_input->height = incrop_cfg.h;

	dma_input->dma_crop_offset = (incrop_cfg.x << 16) | (incrop_cfg.y << 0);
	dma_input->dma_crop_width = incrop_cfg.w;
	dma_input->dma_crop_height = incrop_cfg.h;
	dma_input->bayer_crop_offset_x = incrop_cfg.x;
	dma_input->bayer_crop_offset_y = incrop_cfg.y;
	dma_input->bayer_crop_width = incrop_cfg.w;
	dma_input->bayer_crop_height = incrop_cfg.h;
	dma_input->stride_plane0 = incrop->w;
	*lindex |= LOWBIT_OF(PARAM_YPP_DMA_INPUT);
	*hindex |= HIGHBIT_OF(PARAM_YPP_DMA_INPUT);
	(*indexes)++;

	otf_output = is_itf_g_param(device, frame, PARAM_YPP_OTF_OUTPUT);
	otf_output->cmd = OTF_OUTPUT_COMMAND_ENABLE;
	otf_output->width = incrop_cfg.w;
	otf_output->height = incrop_cfg.h;
	otf_output->format = OTF_OUTPUT_FORMAT_YUV422;
	otf_output->bitwidth = OTF_OUTPUT_BIT_WIDTH_10BIT;
	otf_output->order = OTF_INPUT_ORDER_BAYER_GR_BG;
	*lindex |= LOWBIT_OF(PARAM_YPP_OTF_OUTPUT);
	*hindex |= HIGHBIT_OF(PARAM_YPP_OTF_OUTPUT);
	(*indexes)++;

	stripe_input = is_itf_g_param(device, frame, PARAM_YPP_STRIPE_INPUT);
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
	*lindex |= LOWBIT_OF(PARAM_YPP_STRIPE_INPUT);
	*hindex |= HIGHBIT_OF(PARAM_YPP_STRIPE_INPUT);
	(*indexes)++;

p_err:
	return ret;
}

static int is_ischain_ypp_tag(struct is_subdev *subdev,
	void *device_data,
	struct is_frame *frame,
	struct camera2_node *node)
{
	int ret = 0;
	struct is_group *group;
	struct ypp_param *ypp_param;
	struct is_crop inparm;
	struct is_crop *incrop, *otcrop;
	struct is_subdev *leader;
	struct is_device_ischain *device;
	u32 lindex, hindex, indexes;
#ifdef ENABLE_LOGICAL_VIDEO_NODE
	struct is_queue *queue;
	struct camera2_node votf_out_node;
	struct camera2_node votf_cap_node;
	struct is_framemgr *votf_framemgr = NULL;
	struct is_frame *lvn_frame = NULL;
	struct is_frame *votf_frame = NULL;
	struct camera2_node *out_node = NULL;
	struct camera2_node *cap_node = NULL;
	struct is_sub_frame *sframe;
	u32 *targetAddr;
	u32 n, p, dva_i, num_planes, pindex = 0;
	bool is_group_votf_input = false;
	int dma_type;
#endif

	device = (struct is_device_ischain *)device_data;

	FIMC_BUG(!subdev);
	FIMC_BUG(!device);
	FIMC_BUG(!device->is_region);
	FIMC_BUG(!frame);

	mdbgs_ischain(4, "YPP TAG\n", device);

	incrop = (struct is_crop *)node->input.cropRegion;
	otcrop = (struct is_crop *)node->output.cropRegion;

	group = &device->group_ypp;
	leader = subdev->leader;
	lindex = hindex = indexes = 0;
	ypp_param = &device->is_region->parameter.ypp;

#ifdef ENABLE_LOGICAL_VIDEO_NODE
	queue = GET_SUBDEV_QUEUE(leader);
	if (!queue) {
		merr("queue is NULL", device);
		ret = -EINVAL;
		goto p_err;
	}

	if (queue->mode == CAMERA_NODE_NORMAL)
		goto p_skip;
	lvn_frame = frame;
	out_node = &frame->shot_ext->node_group.leader;
	if (test_bit(IS_GROUP_VOTF_INPUT, &group->state)) {
		is_group_votf_input = true;
		votf_framemgr = GET_SUBDEV_I_FRAMEMGR(subdev);
		if (!votf_framemgr) {
			mgerr("internal framemgr is NULL\n", group, group);
			return -EINVAL;
		}

		votf_frame = &votf_framemgr->frames[votf_framemgr->num_frames ?
			(frame->fcount % votf_framemgr->num_frames) : 0];
		if (!votf_frame) {
			mgerr("internal frame is NULL\n", group, group);
			return -EINVAL;
		}

		ret = __ischain_ypp_set_votf_node(&votf_out_node, frame,
			&(votf_frame->out_node.sframe[0]));
		if (ret)
			return ret;

		lvn_frame = votf_frame;

		out_node = &votf_out_node;
	}

	ret = __ypp_dma_in_cfg(device, lvn_frame, out_node, &frame->stripe_info, PARAM_YPP_DMA_INPUT, &lindex, &hindex, &indexes);

	for (n = 0; n < CAPTURE_NODE_MAX; n++) {
		cap_node = &frame->shot_ext->node_group.capture[n];
		if (is_group_votf_input) {
			sframe = &votf_frame->cap_node.sframe[n];
			if (!sframe->id)
				continue;

			ret = __ischain_ypp_set_votf_node(&votf_cap_node,
				frame, sframe);
			if (ret)
				return ret;
			cap_node = &votf_cap_node;
		}

		if (!cap_node->vid)
			continue;

		dma_type = __ischain_ypp_slot(cap_node, &pindex);
		if (dma_type == 1)
			ret = __ypp_dma_in_cfg(device, lvn_frame, cap_node, &frame->stripe_info, pindex, &lindex, &hindex, &indexes);
	}

#ifdef ENABLE_LVN_DUMMYOUTPUT
	for (p = 0; p < lvn_frame->out_node.sframe[0].num_planes; p++) {
		if (!lvn_frame->out_node.sframe[0].id) {
			mrerr("Invalid video id(%d)", device, lvn_frame,
				lvn_frame->out_node.sframe[0].id);
			return -EINVAL;
		}
		lvn_frame->dvaddr_buffer[p] =
			lvn_frame->out_node.sframe[0].dva[p];
		lvn_frame->kvaddr_buffer[p] =
			lvn_frame->out_node.sframe[0].kva[p];
	}
#endif

	/* Total plane count include batch buffer */
	num_planes = lvn_frame->out_node.sframe[0].num_planes * frame->num_buffers;

	for (p = 0; p < num_planes; p++) {
		dva_i = is_group_votf_input ? (p % lvn_frame->out_node.sframe[0].num_planes) : p;
		lvn_frame->dvaddr_buffer[p] = lvn_frame->out_node.sframe[0].dva[dva_i];
	}

	for (n = 0; n < CAPTURE_NODE_MAX; n++) {
		sframe = &lvn_frame->cap_node.sframe[n];
		if (!sframe->id)
			continue;

		if ((sframe->id >= IS_VIDEO_M0P_NUM) &&
			(sframe->id <= IS_VIDEO_M5P_NUM))
			continue;

		targetAddr = NULL;
		ret = is_hw_get_capture_slot(lvn_frame, &targetAddr, NULL,
				sframe->id);
		if (ret) {
			mrerr("Invalid capture slot(%d)", device, lvn_frame,
				sframe->id);
			return -EINVAL;
		}

		if (!targetAddr)
			continue;

		num_planes = sframe->num_planes * frame->num_buffers;

		for (p = 0; p < num_planes; p++) {
			dva_i = is_group_votf_input ? (p % sframe->num_planes) : p;
			targetAddr[p] = sframe->dva[dva_i];
		}
	}

	ret = is_itf_s_param(device, frame, lindex, hindex, indexes);
	if (ret) {
		mrerr("is_itf_s_param is fail(%d)", device, frame, ret);
		goto p_err;
	}

	return ret;
p_skip:
#endif
	inparm.x = 0;
	inparm.y = 0;
	inparm.w = ypp_param->dma_input.width;
	inparm.h = ypp_param->dma_input.height;

	if (IS_NULL_CROP(incrop))
		*incrop = inparm;

	/* not supported DMA input crop */
	if ((incrop->x != 0) || (incrop->y != 0))
		*incrop = inparm;

	if (!COMPARE_CROP(incrop, &inparm) ||
		CHECK_STRIPE_CFG(&frame->stripe_info) ||
		test_bit(IS_SUBDEV_FORCE_SET, &leader->state)) {
		ret = is_ischain_ypp_cfg(subdev,
			device,
			frame,
			incrop,
			otcrop,
			&lindex,
			&hindex,
			&indexes);
		if (ret) {
			merr("is_ischain_ypp_cfg is fail(%d)", device, ret);
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

const struct is_subdev_ops is_subdev_ypp_ops = {
	.bypass			= NULL,
	.cfg			= is_ischain_ypp_cfg,
	.tag			= is_ischain_ypp_tag,
};
