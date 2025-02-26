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

const struct camera2_node init_isp_cap_node_yuv = {
	.request = 1,
	.vid = IS_VIDEO_I0C_NUM,
	.input = {
		.cropRegion = {0, 0, 0, 0}
	},
	.output = {
		.cropRegion = {0, 0, 0, 0}

	},
	.pixelformat = V4L2_PIX_FMT_NV16M_P210,
	.flags = 2
};

const struct camera2_node init_isp_cap_node_nr_ds = {
	.request = 1,
	.vid = IS_VIDEO_IND_NUM,
	.input = {
		.cropRegion = {0, 0, 0, 0}
	},
	.output = {
		.cropRegion = {0, 0, 0, 0}

	},
	.pixelformat = V4L2_PIX_FMT_YUV420,
	.flags = 2
};

const struct camera2_node init_isp_cap_node_noise = {
	.request = 1,
	.vid = IS_VIDEO_INW_NUM,
	.input = {
		.cropRegion = {0, 0, 0, 0}
	},
	.output = {
		.cropRegion = {0, 0, 0, 0}

	},
	.pixelformat = V4L2_PIX_FMT_GREY,
	.flags = 0
};

const struct camera2_node init_isp_cap_node_drc_gain = {
	.request = 1,
	.vid = IS_VIDEO_IDG_NUM,
	.input = {
		.cropRegion = {0, 0, 0, 0}
	},
	.output = {
		.cropRegion = {0, 0, 0, 0}

	},
	.pixelformat = V4L2_PIX_FMT_GREY,
	.flags = 0
};

const struct camera2_node init_isp_cap_node_svhist = {
	.request = 1,
	.vid = IS_VIDEO_ISH_NUM,
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

int is_ischain_isp_stripe_cfg(struct is_subdev *subdev,
		struct is_frame *frame,
		struct is_crop *incrop,
		struct is_crop *otcrop,
		u32 bitwidth)
{
	struct is_groupmgr *groupmgr;
	struct is_group *group;
	struct is_group *stream_leader;
	u32 stripe_w, dma_offset = 0;
	u32 region_id = frame->stripe_info.region_id;

	groupmgr = (struct is_groupmgr *)frame->groupmgr;
	group = frame->group;
	stream_leader = groupmgr->leader[subdev->instance];

	/* Input crop configuration */
	if (!region_id) {
		/* Left region */
		/* Stripe width should be 4 align because of 4 ppc */
		stripe_w = ALIGN(incrop->w / frame->stripe_info.region_num, 4);
		/* For SBWC Lossless, width should be 512 align with margin */
		stripe_w = ALIGN(stripe_w, STRIPE_WIDTH_ALIGN) - (STRIPE_WIDTH_ALIGN / 2);

		if (stripe_w == 0) {
			msrdbgs(3, "Skip current stripe[#%d] region because stripe_width is too small(%d)\n", subdev, subdev, frame,
									region_id, stripe_w);
			frame->stripe_info.region_id++;
			return -EAGAIN;
		}

		frame->stripe_info.in.h_pix_ratio = stripe_w * STRIPE_RATIO_PRECISION / incrop->w;
		frame->stripe_info.in.h_pix_num[region_id] = stripe_w;
		frame->stripe_info.region_base_addr[0] = frame->dvaddr_buffer[0];
	} else if (region_id < frame->stripe_info.region_num - 1) {
		/* Middle region */
		stripe_w = ALIGN((incrop->w - frame->stripe_info.in.h_pix_num[region_id - 1]) / (frame->stripe_info.region_num - frame->stripe_info.region_id), 4);
		stripe_w = ALIGN_UPDOWN_STRIPE_WIDTH(stripe_w, STRIPE_WIDTH_ALIGN);

		if (stripe_w == 0) {
			msrdbgs(3, "Skip current stripe[#%d] region because stripe_width is too small(%d)\n", subdev, subdev, frame,
									region_id, stripe_w);
			frame->stripe_info.region_id++;
			return -EAGAIN;
		}

		if (!test_bit(IS_GROUP_OTF_INPUT, &group->state)) {
			dma_offset = frame->stripe_info.in.h_pix_num[region_id - 1] - STRIPE_MARGIN_WIDTH;
			dma_offset = dma_offset * bitwidth / BITS_PER_BYTE;
		}
		frame->stripe_info.in.h_pix_num[region_id] = frame->stripe_info.in.h_pix_num[region_id - 1] + stripe_w;
		stripe_w += STRIPE_MARGIN_WIDTH;
	} else {
		/* Right region */
		stripe_w = incrop->w - frame->stripe_info.in.h_pix_num[region_id - 1];

		/* Consider RDMA offset. */
		if (!test_bit(IS_GROUP_OTF_INPUT, &group->state)) {
			if (stream_leader->id == group->id) {
				/**
				 * ISP reads the right region of original bayer image.
				 * Add horizontal DMA offset only.
				 */
				dma_offset = frame->stripe_info.in.h_pix_num[region_id - 1] - STRIPE_MARGIN_WIDTH;
				dma_offset = dma_offset * bitwidth / BITS_PER_BYTE;
				msrwarn("Processed bayer reprocessing is NOT supported by stripe processing",
						subdev, subdev, frame);

			} else {
				/**
				 * ISP reads the right region with stripe margin.
				 * Add horizontal DMA offset.
				 */
				dma_offset = frame->stripe_info.in.h_pix_num[region_id - 1] - STRIPE_MARGIN_WIDTH;
				dma_offset = dma_offset * bitwidth / BITS_PER_BYTE;
			}
		}
		frame->stripe_info.in.h_pix_num[region_id] = frame->stripe_info.in.h_pix_num[region_id - 1] + stripe_w;
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
			region_id,
			incrop->x, incrop->y, incrop->w, incrop->h, dma_offset);
	msrdbgs(3, "stripe_ot_crop[%d][%d, %d, %d, %d]\n", subdev, subdev, frame,
			region_id,
			otcrop->x, otcrop->y, otcrop->w, otcrop->h);
	return 0;
}

u32 __isp_calc_stripe_dma_offset(u32 start_pos_x, u32 bitwidth,
	u32 sbwc_block_width, u32 sbwc_block_height, u32 sbwc_type)
{
	u32 byte_32_num = 4;	/* default lossy rate */
	u32 dma_offset = 0;
	u32 align_byte = 64;
	/* Bitwidth should be even when SBWC */
	if (sbwc_type != DMA_INPUT_SBWC_DISABLE && bitwidth % 2)
		bitwidth = ALIGN(bitwidth, 2);
	switch (sbwc_type) {
		case DMA_INPUT_SBWC_LOSSY_64B:
			dma_offset = DIV_ROUND_UP(start_pos_x, sbwc_block_width) * ((sbwc_block_width * byte_32_num) + (sbwc_block_width * (byte_32_num % 2)));
			break;
		case DMA_INPUT_SBWC_LOSSYLESS_64B:
			dma_offset = DIV_ROUND_UP(start_pos_x, sbwc_block_width) * (sbwc_block_width * sbwc_block_height * (bitwidth
						+ (((16 * bitwidth) % 64) / 16))) / BITS_PER_BYTE;
			break;
		case DMA_INPUT_SBWC_LOSSY_32B:
			align_byte = 32;
			dma_offset = DIV_ROUND_UP(start_pos_x, sbwc_block_width) * (sbwc_block_width * byte_32_num);
			break;
		case DMA_INPUT_SBWC_LOSSYLESS_32B:
			align_byte = 32;
			dma_offset = DIV_ROUND_UP(start_pos_x, sbwc_block_width) * (sbwc_block_width * sbwc_block_height * bitwidth) / BITS_PER_BYTE;
			break;
		default:
			align_byte = 1;
			dma_offset = start_pos_x * bitwidth / BITS_PER_BYTE;
	}
	dma_offset = ALIGN(dma_offset, align_byte);
	return dma_offset;
}

void __isp_in_calc_stripe_start_pos(struct is_crop *incrop, u32 region_num, u32 h_pix_num[])
{
	u32 region_id;
	u32 stripe_w, cur_h_pix_num = 0;

	for (region_id = 0; region_id < region_num; ++region_id) {
		if (!region_id) {
			/* Left region */
			/* Stripe width should be 4 align because of 4 ppc */
			stripe_w = ALIGN(incrop->w / region_num, 4);
			/* For SBWC Lossless, width should be 512 align with margin */
			stripe_w = ALIGN(stripe_w, STRIPE_WIDTH_ALIGN) - (STRIPE_WIDTH_ALIGN / 2);
		} else if (region_id < region_num - 1) {
			/* Middle region */
			stripe_w = ALIGN((incrop->w - cur_h_pix_num) / (region_num - region_id), 4);
			stripe_w = ALIGN_UPDOWN_STRIPE_WIDTH(stripe_w, STRIPE_WIDTH_ALIGN);
		} else {
			/* Right region */
			stripe_w = incrop->w - cur_h_pix_num;
		}
		cur_h_pix_num += stripe_w;
		h_pix_num[region_id] = cur_h_pix_num;
	}
}

#ifdef ENABLE_LOGICAL_VIDEO_NODE
int __isp_in_stripe_cfg(struct is_frame *frame,
		struct camera2_node *node,
		struct is_crop *incrop,
		struct is_crop *otcrop,
		u32 bitwidth,
		u32 sbwc_type)
{
	struct is_groupmgr *groupmgr;
	struct is_group *group;
	struct is_group *stream_leader;
	u32 stripe_w, dma_offset = 0;
	u32 region_id = frame->stripe_info.region_id;

	groupmgr = (struct is_groupmgr *)frame->groupmgr;
	group = frame->group;
	stream_leader = groupmgr->leader[frame->instance];

	/* Input crop configuration */
	if (!region_id) {
		/* Left region */
		__isp_in_calc_stripe_start_pos(incrop, frame->stripe_info.region_num, frame->stripe_info.in.h_pix_num);
		stripe_w = frame->stripe_info.in.h_pix_num[region_id];
		if (stripe_w == 0) {
			mlvdbgs(3, "Skip current stripe[#%d] region because stripe_width is too small(%d)\n", frame, node->vid,
									region_id, stripe_w);
			frame->stripe_info.region_id++;
			return -EAGAIN;
		}
		frame->stripe_info.region_base_addr[0] = frame->dvaddr_buffer[0];
	} else if (region_id < frame->stripe_info.region_num - 1) {
		/* Middle region */
		stripe_w = frame->stripe_info.in.h_pix_num[region_id] - frame->stripe_info.in.h_pix_num[region_id - 1];
		if (stripe_w == 0) {
			mlvdbgs(3, "Skip current stripe[#%d] region because stripe_width is too small(%d)\n", frame, node->vid,
									region_id, stripe_w);
			frame->stripe_info.region_id++;
			return -EAGAIN;
		}

		if (!test_bit(IS_GROUP_OTF_INPUT, &group->state)) {
			dma_offset = frame->stripe_info.in.h_pix_num[region_id - 1] - STRIPE_MARGIN_WIDTH;
			dma_offset = __isp_calc_stripe_dma_offset(dma_offset, bitwidth, 32, 4, sbwc_type);
		}
		stripe_w += STRIPE_MARGIN_WIDTH;
	} else {
		/* Right region */
		stripe_w = incrop->w - frame->stripe_info.in.h_pix_num[region_id - 1];
		/* Consider RDMA offset. */
		if (!test_bit(IS_GROUP_OTF_INPUT, &group->state)) {
			/**
			 * ISP reads the right region with stripe margin.
			 * Add horizontal DMA offset.
			 */
			dma_offset = frame->stripe_info.in.h_pix_num[region_id - 1] - STRIPE_MARGIN_WIDTH;
			dma_offset = __isp_calc_stripe_dma_offset(dma_offset, bitwidth, 32, 4, sbwc_type);
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

	mlvdbgs(3, "[F%d] stripe_in_crop[%d][%d, %d, %d, %d] offset %x\n", frame, node->vid,
			frame->fcount, region_id,
			incrop->x, incrop->y, incrop->w, incrop->h, dma_offset);
	mlvdbgs(3, "[F%d] stripe_ot_crop[%d][%d, %d, %d, %d]\n", frame, node->vid,
			frame->fcount, region_id,
			otcrop->x, otcrop->y, otcrop->w, otcrop->h);
	mlvdbgs(3, "[F%d] stripe_ot_crop[%d]dva: 0x%x\n", frame, node->vid,
			frame->fcount, region_id,
			frame->dvaddr_buffer[0]);
	return 0;
}

void __ixc_out_stripe_cfg(struct is_device_ischain *device,
		struct is_frame *frame,
		struct camera2_node *node,
		struct fimc_is_stripe_info *stripe_info,
		struct is_crop *otcrop,
		u32 bitwidth,
		int index, u32 v_otf_enable)
{
	int i, j, width, height;
	u32 stripe_w = 0, dma_offset = 0, main_dma_offset = 0;
	u32 region_id = stripe_info->region_id;
	u32	*region_base_addr;

	if (frame->out_node.sframe[0].id == IS_VIDEO_YPP_NUM)
		region_base_addr = frame->out_node.sframe[0].stripe_info.region_base_addr;
	else
		region_base_addr = frame->cap_node.sframe[index].stripe_info.region_base_addr;

	/* Output crop & WDMA offset configuration */
	if (!region_id) {
		/* Left region */
		stripe_w = stripe_info->in.h_pix_num[region_id];
		stripe_info->out.h_pix_num[region_id] = stripe_w;
		if (frame->out_node.sframe[0].id == IS_VIDEO_YPP_NUM) {
			region_base_addr[0] = frame->dvaddr_buffer[0];
			region_base_addr[1] = frame->dvaddr_buffer[1];
		} else {
			region_base_addr[0] = frame->cap_node.sframe[index].dva[0];
			region_base_addr[1] = frame->cap_node.sframe[index].dva[1];
		}
	} else if (region_id < stripe_info->region_num - 1) {
		/* Middle region */
		stripe_w = stripe_info->in.h_pix_num[region_id] - stripe_info->out.h_pix_num[region_id - 1];
		dma_offset = stripe_info->out.h_pix_num[region_id - 1];
		dma_offset += STRIPE_MARGIN_WIDTH * ((2 * (region_id - 1)) + 1);
		dma_offset = otcrop->h * ALIGN(dma_offset * bitwidth / BITS_PER_BYTE, 16);
		stripe_info->out.h_pix_num[region_id] = stripe_info->out.h_pix_num[region_id - 1] + stripe_w;
		stripe_w += STRIPE_MARGIN_WIDTH;
	} else {
		/* Right region */
		stripe_w = otcrop->w - stripe_info->out.h_pix_num[region_id - 1];
		dma_offset = stripe_info->out.h_pix_num[region_id - 1];
		dma_offset += STRIPE_MARGIN_WIDTH * ((2 * (region_id - 1)) + 1);
		dma_offset = otcrop->h * ALIGN(dma_offset * bitwidth / BITS_PER_BYTE, 16);
		stripe_info->out.h_pix_num[region_id] = stripe_info->out.h_pix_num[region_id - 1] + stripe_w;
	}
	stripe_w += STRIPE_MARGIN_WIDTH;
	otcrop->w = stripe_w;

	if (v_otf_enable == OTF_OUTPUT_COMMAND_ENABLE)
		dma_offset = 0;

	/* Main output */
	/* If votf frame, share the same buffer with YUVPP */
	if (frame->out_node.sframe[0].id == IS_VIDEO_YPP_NUM) {
		frame->out_node.sframe[0].stripe_info.stripe_w = stripe_w;
		frame->dvaddr_buffer[0] = region_base_addr[0] + dma_offset;
		frame->dvaddr_buffer[1] = region_base_addr[1] + dma_offset;
	} else {
		frame->cap_node.sframe[index].stripe_info.stripe_w = stripe_w;
		frame->cap_node.sframe[index].dva[0] = region_base_addr[0] + dma_offset;
		frame->cap_node.sframe[index].dva[1] = region_base_addr[1] + dma_offset;
		if (frame->cap_node.sframe[index].kva[0]) {
			stripe_info->region_num = stripe_info->region_num;
			stripe_info->kva[region_id][0] = frame->cap_node.sframe[index].kva[0] + dma_offset;
			stripe_info->size[region_id][0] = stripe_w
							* bitwidth / BITS_PER_BYTE
							* otcrop->h;
		}
	}
	main_dma_offset = dma_offset;
	/* TODO: fixme */
	frame->shot->uctl.reserved[region_id] = stripe_info->out.h_pix_num[region_id];
	for (i = 0; i < CAPTURE_NODE_MAX; i++) {
		if (frame->cap_node.sframe[i].id == IS_VIDEO_I0C_NUM
		|| frame->cap_node.sframe[i].id == IS_VIDEO_ISH_NUM
		|| !frame->cap_node.sframe[i].id)
			continue;

		switch (frame->cap_node.sframe[i].id) {
		case IS_VIDEO_INW_NUM:
		case IS_VIDEO_YNS_NUM:
			for (j = 0; j < frame->cap_node.sframe[i].num_planes; j++) {
				if (!region_id) {
					frame->cap_node.sframe[i].stripe_info.region_base_addr[j] = frame->cap_node.sframe[i].dva[j];
				}
				frame->cap_node.sframe[i].dva[j] =
					frame->cap_node.sframe[i].stripe_info.region_base_addr[j] + main_dma_offset;
			}
			break;
		case IS_VIDEO_IDG_NUM:
		case IS_VIDEO_YDG_NUM:
			stripe_w = ((stripe_w + 2) >> 2) + 1;
			if (region_id) {
				if (frame->shot->uctl.reserved[region_id]) {
					width = stripe_info->out.h_pix_num[region_id - 1];
					width += STRIPE_MARGIN_WIDTH * ((2 * (region_id - 1)) + 1);
					width = ((width + 2) >> 2) + 1;
					height = ((otcrop->h + 2) >> 2) + 1;
					dma_offset = height * ALIGN(width * 8 / BITS_PER_BYTE, 16);
				} else {
					width = stripe_info->out.h_pix_num[region_id - 1] - STRIPE_MARGIN_WIDTH;
					width = ((width + 2) >> 2) + 1;
					dma_offset = ALIGN(width * 8 / BITS_PER_BYTE, 16);
				}
			}

			if (v_otf_enable == OTF_OUTPUT_COMMAND_ENABLE)
				dma_offset = 0;

			for (j = 0; j < frame->cap_node.sframe[i].num_planes; j++) {
				if (!region_id) {
					frame->cap_node.sframe[i].stripe_info.region_base_addr[j] = frame->cap_node.sframe[i].dva[j];
				}
				frame->cap_node.sframe[i].dva[j] =
					frame->cap_node.sframe[i].stripe_info.region_base_addr[j] + dma_offset;
			}
			break;
		case IS_VIDEO_IND_NUM:
		case IS_VIDEO_YND_NUM:
			stripe_w = ((stripe_w + 2) >> 2) + 1;
			for (j = 0; j < frame->cap_node.sframe[i].num_planes; j++) {
				if (region_id) {
					if (frame->shot->uctl.reserved[region_id]) {
						width = stripe_info->out.h_pix_num[region_id - 1];
						width += STRIPE_MARGIN_WIDTH * ((2 * (region_id - 1)) + 1);
						if (j == 0) {
							width = ((width + 2) >> 2) + 1;
							height = ((otcrop->h + 2) >> 2) + 1;
						} else {
							width = (((width + 4) >> 3) + 1) << 1;
							height = ((otcrop->h + 7) >> 3) + 1;
						}
						dma_offset = height * ALIGN(width * bitwidth / BITS_PER_BYTE, 16);
					} else {
						width = stripe_info->out.h_pix_num[region_id - 1] - STRIPE_MARGIN_WIDTH;
						if (j == 0) {
							width = ((width + 2) >> 2) + 1;
						} else {
							width = (((width + 4) >> 3) + 1) << 1;
						}
						dma_offset = ALIGN(width * bitwidth / BITS_PER_BYTE, 16);
					}
				} else {
					frame->cap_node.sframe[i].stripe_info.region_base_addr[j] = frame->cap_node.sframe[i].dva[j];
				}

				if (v_otf_enable == OTF_OUTPUT_COMMAND_ENABLE)
					dma_offset = 0;

				frame->cap_node.sframe[i].dva[j] = frame->cap_node.sframe[i].stripe_info.region_base_addr[j] + dma_offset;
			}
			break;
		}
		frame->cap_node.sframe[i].stripe_info.stripe_w = stripe_w;
		mlvdbgs(3, "[F%d] stripe_ot_crop[%d] stripe_width: %d, dva: %#x, %#x, offset %x\n", frame, frame->cap_node.sframe[i].id,
			frame->fcount,
			region_id,
			stripe_w, frame->cap_node.sframe[i].dva[0], frame->cap_node.sframe[i].dva[1],
			dma_offset);
	}
	mlvdbgs(3, "[F%d] stripe_ot_crop[%d][%d, %d, %d, %d] offset %x, %d\n", frame, node->vid,
			frame->fcount, region_id,
			otcrop->x, otcrop->y, otcrop->w, otcrop->h, dma_offset,
			frame->shot->uctl.reserved[region_id]);
	mlvdbgs(3, "[F%d] stripe_ot_crop[%d]dva: 0x%x, 0x%x, pixel(%d)\n", frame, node->vid,
			frame->fcount, region_id,
			frame->cap_node.sframe[index].dva[0],
			frame->cap_node.sframe[index].dva[1],
			bitwidth);
}

static int __ischain_isp_slot(struct camera2_node *node, u32 *pindex)
{
	int ret = 0;

	switch (node->vid) {
	case IS_VIDEO_I0T_NUM:
		*pindex = PARAM_ISP_VDMA3_INPUT;
		ret = 1;
		break;
	case IS_VIDEO_I0G_NUM:
		*pindex = PARAM_ISP_VDMA2_INPUT;
		ret = 1;
		break;
	case IS_VIDEO_IMM_NUM:
		*pindex = PARAM_ISP_MOTION_DMA_INPUT;
		ret = 1;
		break;
	case IS_VIDEO_IRG_NUM:
		*pindex = PARAM_ISP_RGB_INPUT;
		ret = 1;
		break;
	case IS_VIDEO_ISC_NUM:
		*pindex = PARAM_ISP_SCMAP_INPUT;
		ret = 1;
		break;
	case IS_VIDEO_IDR_NUM:
		*pindex = PARAM_ISP_DRC_INPUT;
		ret = 1;
		break;
	case IS_VIDEO_INR_NUM:
		*pindex = PARAM_ISP_NOISE_INPUT;
		ret = 1;
		break;
	case IS_VIDEO_I0C_NUM:
		*pindex = PARAM_ISP_VDMA4_OUTPUT;
		ret = 2;
		break;
	case IS_VIDEO_I0P_NUM:
		*pindex = PARAM_ISP_VDMA5_OUTPUT;
		ret = 2;
		break;
	case IS_VIDEO_I0V_NUM:
		*pindex = PARAM_ISP_VDMA6_OUTPUT;
		ret = 2;
		break;
	case IS_VIDEO_I0W_NUM:
		*pindex = PARAM_ISP_VDMA7_OUTPUT;
		ret = 2;
		break;
	case IS_VIDEO_IND_NUM:
		*pindex = PARAM_ISP_NRDS_OUTPUT;
		ret = 2;
		break;
	case IS_VIDEO_IDG_NUM:
		*pindex = PARAM_ISP_DRC_OUTPUT;
		ret = 2;
		break;
	case IS_VIDEO_ISH_NUM:
		*pindex = PARAM_ISP_HIST_OUTPUT;
		ret = 2;
		break;
	case IS_VIDEO_IHF_NUM:
		*pindex = PARAM_ISP_HF_OUTPUT;
		ret = 2;
		break;
	case IS_VIDEO_INW_NUM:
		*pindex = PARAM_ISP_NOISE_OUTPUT;
		ret = 2;
		break;
	case IS_VIDEO_INRW_NUM:
		*pindex = PARAM_ISP_NOISE_REP_OUTPUT;
		ret = 2;
		break;
	case IS_VIDEO_IRGW_NUM:
		*pindex = PARAM_ISP_RGB_OUTPUT;
		ret = 2;
		break;
	case IS_VIDEO_INB_NUM:
		*pindex = PARAM_ISP_NRB_OUTPUT;
		ret = 2;
		break;
	default:
		ret = 0;
		break;
	}

	return ret;
}

static int __ischain_isp_set_votf_node(struct camera2_node *node,
	struct is_frame *ldr_frame, struct is_sub_frame *sub_frame)
{
	struct camera2_node *ldr_frame_node;
	int ret = 0;
	int i = 0;

	ldr_frame_node = &ldr_frame->shot_ext->node_group.leader;

	switch (sub_frame->id) {
	case IS_VIDEO_YPP_NUM:
		memcpy(node, &init_isp_cap_node_yuv, sizeof(struct camera2_node));
		for (i = 0; i < 4; i++)
			node->output.cropRegion[i] = node->input.cropRegion[i] =
				ldr_frame_node->output.cropRegion[i];
		node->flags = sub_frame->hw_pixeltype;
		break;
	case IS_VIDEO_YND_NUM:
		memcpy(node, &init_isp_cap_node_nr_ds, sizeof(struct camera2_node));
		for (i = 0; i < 4; i++)
			node->output.cropRegion[i] = node->input.cropRegion[i] =
				((ldr_frame_node->output.cropRegion[i] + 2) >> 2) + 1;
		break;
	case IS_VIDEO_YDG_NUM:
		memcpy(node, &init_isp_cap_node_drc_gain, sizeof(struct camera2_node));
		for (i = 0; i < 4; i++)
			node->output.cropRegion[i] = node->input.cropRegion[i] =
				((ldr_frame_node->output.cropRegion[i] + 2) >> 2) + 1;
		break;
	case IS_VIDEO_YSH_NUM:
		memcpy(node, &init_isp_cap_node_svhist, sizeof(struct camera2_node));
		break;
	case IS_VIDEO_YNS_NUM:
		memcpy(node, &init_isp_cap_node_noise, sizeof(struct camera2_node));
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

static int __isp_dma_in_cfg(struct is_device_ischain *device,
	struct is_subdev *leader,
	struct is_frame *ldr_frame,
	struct camera2_node *node,
	u32 pindex,
	u32 *lindex,
	u32 *hindex,
	u32 *indexes)
{
	struct is_fmt *fmt;
	struct param_dma_input *dma_input;
	struct param_otf_input *otf_input;
	struct param_otf_output *otf_output;
	struct param_stripe_input *stripe_input;
	struct is_crop *incrop, *otcrop;
	struct is_group *group;
	u32 hw_format = DMA_INOUT_FORMAT_BAYER;
	u32 hw_bitwidth = DMA_INOUT_BIT_WIDTH_16BIT;
	u32 hw_sbwc = DMA_INPUT_SBWC_DISABLE;
	u32 hw_msb = MSB_OF_3AA_DMA_OUT;
	u32 hw_order, flag_extra, flag_pixel_size;
	struct is_crop incrop_cfg, otcrop_cfg;
	int ret = 0;
	int i;

	FIMC_BUG(!device);
	FIMC_BUG(!ldr_frame);
	FIMC_BUG(!node);
	FIMC_BUG(!lindex);
	FIMC_BUG(!hindex);
	FIMC_BUG(!indexes);

	dma_input = is_itf_g_param(device, ldr_frame, pindex);
	if (dma_input->cmd  != node->request)
		mlvinfo("[F%d] RDMA enable: %d -> %d\n", device,
			node->vid, ldr_frame->fcount,
			dma_input->cmd, node->request);
	dma_input->cmd = DMA_INPUT_COMMAND_DISABLE;
	*lindex |= LOWBIT_OF(pindex);
	*hindex |= HIGHBIT_OF(pindex);
	(*indexes)++;

	if (!node->request)
		return 0;

	group = &device->group_isp;
	incrop = (struct is_crop *)node->input.cropRegion;
	if (IS_NULL_CROP(incrop)) {
		incrop->x = 0;
		incrop->y = 0;
		incrop->w = leader->input.width;
		incrop->h = leader->input.height;
		mlverr("incrop is NULL (forcely set: %d, %d)", device, node->vid,
			incrop->w, incrop->h);
		/* return -EINVAL; */
	}

	otcrop = (struct is_crop *)node->output.cropRegion;
	if (IS_NULL_CROP(otcrop)) {
		mlverr("otcrop is NULL", device, node->vid);
		return -EINVAL;
	}

	incrop_cfg = *incrop;
	otcrop_cfg = *otcrop;
	fmt = is_find_format(node->pixelformat, node->flags);
	if (!fmt) {
		merr("pixel format(0x%x) is not found", device, node->pixelformat);
		return -EINVAL;
	}

	hw_format = fmt->hw_format;
	hw_bitwidth = fmt->hw_bitwidth;
	hw_msb = hw_bitwidth - 1;
	hw_order = fmt->hw_order;
	/* pixel type [0:5] : pixel size, [6:7] : extra */
	flag_pixel_size = node->flags & PIXEL_TYPE_SIZE_MASK;
	flag_extra = (node->flags & PIXEL_TYPE_EXTRA_MASK)
			>> PIXEL_TYPE_EXTRA_SHIFT;

	if (hw_format == DMA_INOUT_FORMAT_BAYER_PACKED
		&& flag_pixel_size == CAMERA_PIXEL_SIZE_13BIT) {
		mdbgs_ischain(3, "in_crop[bitwidth: %d -> %d: 13 bit BAYER]\n",
				device,
				hw_bitwidth,
				DMA_INOUT_BIT_WIDTH_13BIT);
		hw_bitwidth = DMA_INOUT_BIT_WIDTH_13BIT;
		hw_msb = hw_bitwidth - 1;
	} else if (hw_format == DMA_INOUT_FORMAT_BAYER) {
		hw_msb = hw_bitwidth;	/* consider signed format only */
		hw_bitwidth = DMA_INOUT_BIT_WIDTH_16BIT;
		mdbgs_ischain(3, "in_crop[unpack bitwidth: %d, msb: %d]\n",
				device,
				hw_bitwidth,
				hw_msb);
	}

	if (flag_extra)
		hw_sbwc = (SBWC_BASE_ALIGN_MASK_LLC_OFF | flag_extra);

	/* DRC, HF, Noise, SC map have own format enum */
	switch (node->vid) {
	/* RDMA */
	case IS_VIDEO_IMM_NUM:
		break;
	case IS_VIDEO_IRG_NUM:
		hw_format = DMA_INOUT_FORMAT_RGB;
		hw_bitwidth = DMA_INOUT_BIT_WIDTH_12BIT;
		hw_msb = hw_bitwidth - 1;
		hw_order = fmt->hw_order;
		break;
	case IS_VIDEO_ISC_NUM:
		hw_format = DMA_INOUT_FORMAT_SEGCONF;
		hw_bitwidth = DMA_INOUT_BIT_WIDTH_8BIT;
		hw_msb = hw_bitwidth - 1;
		hw_order = fmt->hw_order;
		break;
	case IS_VIDEO_IDR_NUM:
		hw_format = DMA_INOUT_FORMAT_DRCGRID;
		hw_bitwidth = DMA_INOUT_BIT_WIDTH_32BIT;
		hw_msb = hw_bitwidth - 1;
		hw_order = DMA_INOUT_ORDER_NO;
		break;
	case IS_VIDEO_INR_NUM:
		hw_format = DMA_INOUT_FORMAT_NOISE;
		hw_bitwidth = DMA_INOUT_BIT_WIDTH_14BIT;
		hw_msb = hw_bitwidth - 1;
		hw_order = fmt->hw_order;
		break;
	default:
		break;
	}

	if (IS_ENABLED(CHAIN_USE_STRIPE_PROCESSING) && ldr_frame->stripe_info.region_num &&
		(node->vid == IS_VIDEO_I0S_NUM)) {
		 __isp_in_stripe_cfg(ldr_frame, node,
					&incrop_cfg, &otcrop_cfg,
					hw_bitwidth, hw_sbwc);
	}

	if (dma_input->format != hw_format)
		mlvinfo("[F%d]RDMA format: %d -> %d\n", device,
			node->vid, ldr_frame->fcount,
			dma_input->format, hw_format);
	if (dma_input->bitwidth != hw_bitwidth)
		mlvinfo("[F%d]RDMA bitwidth: %d -> %d\n", device,
			node->vid, ldr_frame->fcount,dma_input->bitwidth, hw_bitwidth);
	if (dma_input->sbwc_type != hw_sbwc)
		mlvinfo("[F%d]RDMA sbwc_type: %d -> %d\n", device,
			node->vid, ldr_frame->fcount,
			dma_input->sbwc_type, hw_sbwc);
	if (!ldr_frame->stripe_info.region_num && ((dma_input->width != incrop->w) ||
		(dma_input->height != incrop->h)))
		mlvinfo("[F%d]RDMA incrop[%d, %d, %d, %d]\n", device,
			node->vid, ldr_frame->fcount,
			incrop->x, incrop->y, incrop->w, incrop->h);

	dma_input->cmd = DMA_INPUT_COMMAND_ENABLE;
	dma_input->format = hw_format;
	dma_input->bitwidth = hw_bitwidth;
	dma_input->msb = hw_msb;
	dma_input->sbwc_type = hw_sbwc;
	dma_input->order = hw_order;
	dma_input->plane = fmt->hw_plane;
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
	dma_input->stride_plane1 = incrop->w;
	dma_input->stride_plane2 = incrop->w;
	/* VOTF of slave in is always disabled */
	dma_input->v_otf_enable = OTF_INPUT_COMMAND_DISABLE;
	dma_input->v_otf_token_line = VOTF_TOKEN_LINE;

	/* otf input and output should be set for ISP */
	if (node->vid != IS_VIDEO_I0S_NUM)
		return ret;

	/* setting OTF input and OTF output */
	otf_input = is_itf_g_param(device, ldr_frame, PARAM_ISP_OTF_INPUT);
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

	otf_output = is_itf_g_param(device, ldr_frame, PARAM_ISP_OTF_OUTPUT);
	if (test_bit(IS_GROUP_OTF_OUTPUT, &group->state)
		&& !test_bit(IS_GROUP_VOTF_OUTPUT, &group->state))
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

	stripe_input = is_itf_g_param(device, ldr_frame, PARAM_ISP_STRIPE_INPUT);
	if (ldr_frame->stripe_info.region_num) {
		stripe_input->index = ldr_frame->stripe_info.region_id;
		stripe_input->total_count = ldr_frame->stripe_info.region_num;
		if (!ldr_frame->stripe_info.region_id) {
			stripe_input->left_margin = 0;
			stripe_input->right_margin = STRIPE_MARGIN_WIDTH;
			stripe_input->stripe_roi_start_pos_x[0] = 0;
			for (i = 1; i < ldr_frame->stripe_info.region_num; ++i)
				stripe_input->stripe_roi_start_pos_x[i] = ldr_frame->stripe_info.in.h_pix_num[i - 1];
		} else if (ldr_frame->stripe_info.region_id < ldr_frame->stripe_info.region_num - 1) {
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
	*lindex |= LOWBIT_OF(PARAM_ISP_STRIPE_INPUT);
	*hindex |= HIGHBIT_OF(PARAM_ISP_STRIPE_INPUT);
	(*indexes)++;

	return ret;
}

static int __isp_dma_out_cfg(struct is_device_ischain *device,
	struct is_subdev *leader,
	struct is_frame *frame,
	struct camera2_node *node,
	struct fimc_is_stripe_info *stripe_info,
	u32 pindex,
	u32 *lindex,
	u32 *hindex,
	u32 *indexes,
	int index)
{
	struct is_fmt *fmt;
	struct param_dma_output *dma_output;
	struct is_crop *incrop, *otcrop;
	struct is_crop otcrop_cfg;
	u32 votf_dst_ip = 0;
	u32 votf_dst_axi_id_m = 0;
	u32 hw_format = DMA_INOUT_FORMAT_BAYER;
	u32 hw_bitwidth = DMA_INOUT_BIT_WIDTH_16BIT;
	u32 hw_sbwc = DMA_INPUT_SBWC_DISABLE;
	u32 hw_plane, hw_msb, hw_order, flag_extra, flag_pixel_size;
	u32 width;
	int ret = 0;

	FIMC_BUG(!frame);
	FIMC_BUG(!node);

	dma_output = is_itf_g_param(device, frame, pindex);
	if (dma_output->cmd != node->request)
		mlvinfo("[F%d] WDMA enable: %d -> %d\n", device,
			node->vid, frame->fcount,
			dma_output->cmd, node->request);
	dma_output->cmd = DMA_OUTPUT_COMMAND_DISABLE;
	*lindex |= LOWBIT_OF(pindex);
	*hindex |= HIGHBIT_OF(pindex);
	(*indexes)++;

	if (!node->request)
		return 0;

	incrop = (struct is_crop *)node->input.cropRegion;
	otcrop = (struct is_crop *)node->output.cropRegion;
	if (IS_NULL_CROP(otcrop)) {
		mlverr("[F%d][%d] otcrop is NULL (%d, %d, %d, %d), disable DMA",
			device, node->vid, frame->fcount, pindex,
			otcrop->x, otcrop->y, otcrop->w, otcrop->h);
		otcrop->x = otcrop->y = 0;
		otcrop->w = leader->input.width;
		otcrop->h = leader->input.height;
		dma_output->cmd = DMA_OUTPUT_COMMAND_DISABLE;
	}
	otcrop_cfg = *otcrop;
	width = otcrop_cfg.w;
	if (IS_NULL_CROP(incrop)) {
		mlverr("[F%d][%d] incrop is NULL (%d, %d, %d, %d), disable DMA",
			device, node->vid, frame->fcount, pindex,
			incrop->x, incrop->y, incrop->w, incrop->h);
		incrop->x = 0;
		incrop->y = 0;
		incrop->w = leader->input.width;
		incrop->h = leader->input.height;
		dma_output->cmd = DMA_OUTPUT_COMMAND_DISABLE;
	}

	fmt = is_find_format(node->pixelformat, node->flags);
	if (!fmt) {
		merr("pixelformat(%c%c%c%c) is not found", device,
			(char)((node->pixelformat >> 0) & 0xFF),
			(char)((node->pixelformat >> 8) & 0xFF),
			(char)((node->pixelformat >> 16) & 0xFF),
			(char)((node->pixelformat >> 24) & 0xFF));
		return -EINVAL;
	}
	hw_format = fmt->hw_format;
	hw_bitwidth = fmt->hw_bitwidth;
	hw_msb = hw_bitwidth - 1;
	hw_order = fmt->hw_order;
	hw_plane = fmt->hw_plane;
	/* pixel type [0:5] : pixel size, [6:7] : extra */
	flag_pixel_size = node->flags & PIXEL_TYPE_SIZE_MASK;
	flag_extra = (node->flags & PIXEL_TYPE_EXTRA_MASK) >> PIXEL_TYPE_EXTRA_SHIFT;

	if (hw_format == DMA_INOUT_FORMAT_BAYER_PACKED
		&& flag_pixel_size == CAMERA_PIXEL_SIZE_13BIT) {
		hw_bitwidth = DMA_INOUT_BIT_WIDTH_13BIT;
		hw_msb = hw_bitwidth - 1;
	} else if (hw_format == DMA_INOUT_FORMAT_BAYER) {
		/* NOTICE: consider signed format only */
		hw_msb = hw_bitwidth;
		hw_bitwidth = DMA_INOUT_BIT_WIDTH_16BIT;
	}

	if (flag_extra)
		hw_sbwc = (SBWC_BASE_ALIGN_MASK_LLC_ON | flag_extra);

	/* DRC, HF, Noise, SC map have own format enum */
	switch (node->vid) {
	/* WDMA */
	case IS_VIDEO_IND_NUM:
		hw_plane = DMA_INOUT_PLANE_2;
		hw_bitwidth = DMA_INOUT_BIT_WIDTH_10BIT;
		hw_msb = hw_bitwidth - 1;
		break;
	case IS_VIDEO_IDG_NUM:
		hw_format = DMA_INOUT_FORMAT_DRCGAIN;
		hw_bitwidth = DMA_INOUT_BIT_WIDTH_8BIT;
		hw_msb = hw_bitwidth - 1;
		break;
	case IS_VIDEO_ISH_NUM:
		hw_format = DMA_INOUT_FORMAT_SVHIST;
		break;
	case IS_VIDEO_IHF_NUM:
		hw_format = DMA_INOUT_FORMAT_HF;
		hw_bitwidth = DMA_INOUT_BIT_WIDTH_8BIT;
		hw_msb = hw_bitwidth - 1;
		break;
	case IS_VIDEO_INW_NUM:
		hw_format = DMA_INOUT_FORMAT_NOISE;
		hw_bitwidth = DMA_INOUT_BIT_WIDTH_10BIT;
		hw_msb = hw_bitwidth - 1;
		break;
	case IS_VIDEO_INRW_NUM:
		hw_format = DMA_INOUT_FORMAT_NOISE;
		hw_bitwidth = DMA_INOUT_BIT_WIDTH_14BIT;
		hw_msb = hw_bitwidth - 1;
		break;
	case IS_VIDEO_IRGW_NUM:
		hw_format = DMA_INOUT_FORMAT_RGB;
		hw_bitwidth = DMA_INOUT_BIT_WIDTH_12BIT;
		hw_msb = hw_bitwidth - 1;
		break;
	case IS_VIDEO_INB_NUM:
		/* TODO: fill this */
		break;
	default:
		break;
	}

	if (dma_output->format != hw_format)
		mlvinfo("[F%d]WDMA format: %d -> %d\n", device,
			node->vid, frame->fcount,
			dma_output->format, hw_format);
	if (dma_output->bitwidth != hw_bitwidth)
		mlvinfo("[F%d]WDMA bitwidth: %d -> %d\n", device,
			node->vid, frame->fcount,
			dma_output->bitwidth, hw_bitwidth);
	if (dma_output->sbwc_type != hw_sbwc)
		mlvinfo("[F%d]WDMA sbwc_type: %d -> %d\n", device,
				node->vid, frame->fcount,
				dma_output->sbwc_type, hw_sbwc);
	if (!stripe_info->region_num && ((dma_output->width != otcrop->w) ||
		(dma_output->height != otcrop->h)))
		mlvinfo("[F%d]WDMA otcrop[%d, %d, %d, %d]\n", device,
			node->vid, frame->fcount,
			otcrop->x, otcrop->y, otcrop->w, otcrop->h);


	if (test_bit(IS_GROUP_OTF_OUTPUT, &device->group_isp.state)) {
		if (test_bit(IS_GROUP_VOTF_OUTPUT, &device->group_isp.state)) {
			dma_output->cmd = DMA_OUTPUT_COMMAND_ENABLE;
			dma_output->v_otf_enable = OTF_OUTPUT_COMMAND_ENABLE;
		} else {
			dma_output->cmd = DMA_OUTPUT_COMMAND_DISABLE;
			dma_output->v_otf_enable = OTF_OUTPUT_COMMAND_DISABLE;
		}
	} else {
		dma_output->cmd = DMA_OUTPUT_COMMAND_ENABLE;
		dma_output->v_otf_enable = OTF_OUTPUT_COMMAND_DISABLE;
	}

	if (IS_ENABLED(CHAIN_USE_STRIPE_PROCESSING) && stripe_info->region_num){
		switch (node->vid) {
		/* RDMA */
		case IS_VIDEO_I0C_NUM:
			__ixc_out_stripe_cfg(device, frame,
					node, stripe_info, &otcrop_cfg, hw_bitwidth, index, dma_output->v_otf_enable);
			if (frame->out_node.sframe[0].stripe_info.stripe_w != 0)
				width = frame->out_node.sframe[0].stripe_info.stripe_w;
			else
				width = frame->cap_node.sframe[index].stripe_info.stripe_w;
			break;
		default:
			if (frame->cap_node.sframe[index].stripe_info.stripe_w != 0)
				width = frame->cap_node.sframe[index].stripe_info.stripe_w;
			break;
		}
	}

	dma_output->format = hw_format;
	dma_output->bitwidth = hw_bitwidth;
	dma_output->msb = hw_msb;
	dma_output->sbwc_type = hw_sbwc;
	dma_output->order = hw_order;
	dma_output->plane = hw_plane;
	dma_output->width = width;//otcrop_cfg.w;
	dma_output->height = otcrop_cfg.h;
	dma_output->dma_crop_offset_x = 0;
	dma_output->dma_crop_offset_y = 0;
	dma_output->dma_crop_width = width;//otcrop_cfg.w;
	dma_output->dma_crop_height = otcrop_cfg.h;
	/* VOTF of slave in is alwasy disabled */

	dma_output->v_otf_dst_ip = votf_dst_ip;
	dma_output->v_otf_dst_axi_id = votf_dst_axi_id_m;
	dma_output->v_otf_token_line = VOTF_TOKEN_LINE;

	return ret;
}

static int __ischain_isp_set_votf(struct is_device_ischain *device,
	struct is_group *group, struct is_frame *ldr_frame,
	u32 *lindex, u32 *hindex, u32 *indexes)
{
	struct is_subdev *leader = &group->leader;
	struct is_framemgr *votf_framemgr = NULL;
	struct is_frame *votf_frame = NULL;
	struct camera2_node votf_cap_node;
	u32 *targetAddr;
	u32 pindex = 0;
	u32 num_planes;
	int n, p, dva_i, dma_type;
	int ret = 0;

	votf_framemgr = GET_SUBDEV_I_FRAMEMGR(&group->next->leader);
	if (!votf_framemgr) {
		mgerr("internal framemgr is NULL\n", group, group);
		return -EINVAL;
	}

	votf_frame = &votf_framemgr->frames[votf_framemgr->num_frames ?
		(ldr_frame->fcount % votf_framemgr->num_frames) : 0];

	if (!votf_frame) {
		mgerr("internal frame is NULL\n", group, group);
		return -EINVAL;
	}

	if (!votf_frame->out_node.sframe[0].id) {
		mgerr("internal frame is invalid\n", group, group);
		return -EINVAL;
	}
	/* out_node of next group is cap_node of current group	*
	 * L0 YUV OUT param_set, target addr setting		*/

	ret = __ischain_isp_set_votf_node(&votf_cap_node,
		ldr_frame, &(votf_frame->out_node.sframe[0]));
	if (ret)
		return ret;

	votf_frame->shot = ldr_frame->shot;
	votf_frame->fcount = ldr_frame->fcount;

	for (p = 0; p < votf_frame->out_node.sframe[0].num_planes; p++)
		votf_frame->dvaddr_buffer[p] = votf_frame->out_node.sframe[0].dva[p];

	for (n = 0; n < CAPTURE_NODE_MAX; n++) {
		if (!votf_frame->cap_node.sframe[n].id)
			continue;
		for (p = 0; p < votf_frame->cap_node.sframe[n].num_planes; p++)
			votf_frame->cap_node.sframe[n].dva[p] = votf_frame->cap_node.sframe[n].backup_dva[p];
	}

	dma_type = __ischain_isp_slot(&votf_cap_node, &pindex);
	if (dma_type == 2)
		ret = __isp_dma_out_cfg(device, leader, votf_frame,
			&votf_cap_node, &ldr_frame->stripe_info,
			pindex, lindex, hindex, indexes, 0);

	targetAddr = NULL;
	ret = is_hw_get_capture_slot(votf_frame, &targetAddr, NULL, votf_cap_node.vid);
	if (ret) {
		mrerr("Invalid capture slot(%d)", device, ldr_frame,
			votf_cap_node.vid);
		return -EINVAL;
	}

	/* Total plane count include batch buffer */
	num_planes = votf_frame->out_node.sframe[0].num_planes
			* ldr_frame->num_buffers;

	if (targetAddr) {
		for (p = 0; p < num_planes; p++) {
			dva_i = p % votf_frame->out_node.sframe[0].num_planes;
			targetAddr[p] = votf_frame->dvaddr_buffer[dva_i];
		}
	}

	for (n = 0; n < CAPTURE_NODE_MAX; n++) {
		if (!votf_frame->cap_node.sframe[n].id)
			continue;

		ret = __ischain_isp_set_votf_node(&votf_cap_node,
			ldr_frame, &(votf_frame->cap_node.sframe[n]));
		if (ret)
			return ret;

		dma_type = __ischain_isp_slot(&votf_cap_node, &pindex);
		if (dma_type == 2)
			ret = __isp_dma_out_cfg(device, leader,
					votf_frame, &votf_cap_node,
					&ldr_frame->stripe_info,
					pindex, lindex, hindex, indexes, n);

		targetAddr = NULL;
		ret = is_hw_get_capture_slot(votf_frame, &targetAddr, NULL, votf_cap_node.vid);
		if (ret) {
			mrerr("Invalid capture slot(%d)", device, ldr_frame,
				votf_cap_node.vid);
			return -EINVAL;
		}

		if (!targetAddr)
			continue;

		/* Total plane count include batch buffer */
		num_planes = votf_frame->cap_node.sframe[n].num_planes
				* ldr_frame->num_buffers;

		for (p = 0; p < num_planes; p++) {
			dva_i = p % votf_frame->cap_node.sframe[n].num_planes;
			targetAddr[p] =	votf_frame->cap_node.sframe[n].dva[dva_i];
		}
	}

	return ret;
}
#endif

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
	struct param_dma_output *dma_output;
	struct param_stripe_input *stripe_input;
	struct param_control *control;
	struct is_crop *scc_incrop;
	struct is_crop *scp_incrop;
	struct is_device_ischain *device;
	u32 hw_format = DMA_INOUT_FORMAT_BAYER;
	u32 hw_bitwidth = DMA_INOUT_BIT_WIDTH_16BIT;
	u32 hw_sbwc = 0, hw_msb = MSB_OF_3AA_DMA_OUT;
	u32 flag_extra, flag_pixel_size;
	bool chg_format = false;
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
		flag_extra = (queue->framecfg.hw_pixeltype
				& PIXEL_TYPE_EXTRA_MASK)
				>> PIXEL_TYPE_EXTRA_SHIFT;

		if (hw_format == DMA_INOUT_FORMAT_BAYER_PACKED
			&& flag_pixel_size == CAMERA_PIXEL_SIZE_13BIT) {
			hw_msb = MSB_OF_3AA_DMA_OUT + 1;
			hw_bitwidth = DMA_INOUT_BIT_WIDTH_13BIT;
			chg_format = true;
		} else if (hw_format == DMA_INOUT_FORMAT_BAYER) {
			/* consider signed format only */
			hw_msb = hw_bitwidth;
			hw_bitwidth = DMA_INOUT_BIT_WIDTH_16BIT;
			chg_format = true;
		}

		if (flag_extra) {
			hw_sbwc = (SBWC_BASE_ALIGN_MASK_LLC_OFF | flag_extra);
			chg_format = true;
		}

		if (chg_format)
			msdbgs(3, "in_crop[bitwidth %d msb %d sbwc 0x%x]\n",
					device, leader,
					hw_bitwidth, hw_msb,
					hw_sbwc);
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
	dma_input->plane = queue->framecfg.format->hw_plane;
	dma_input->format = hw_format;
	dma_input->bitwidth = hw_bitwidth;
	dma_input->msb = hw_msb;
	dma_input->sbwc_type = hw_sbwc;
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

	if (test_bit(IS_GROUP_OTF_OUTPUT, &group->state)) {
		if (test_bit(IS_GROUP_VOTF_OUTPUT, &group->state)) {
			dma_output = is_itf_g_param(device, frame, PARAM_ISP_VDMA4_OUTPUT);
			dma_output->cmd = DMA_OUTPUT_COMMAND_ENABLE;
			dma_output->v_otf_enable = OTF_OUTPUT_COMMAND_ENABLE;
			dma_output->width = incrop_cfg.w;
			dma_output->height = incrop_cfg.h;
			dma_output->format = DMA_INOUT_FORMAT_YUV422;
			dma_output->bitwidth = DMA_INOUT_BIT_WIDTH_10BIT;
			dma_output->plane = DMA_INOUT_PLANE_2;

			*lindex |= LOWBIT_OF(PARAM_ISP_VDMA4_OUTPUT);
			*hindex |= HIGHBIT_OF(PARAM_ISP_VDMA4_OUTPUT);
			(*indexes)++;
		}
	}

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
#ifdef ENABLE_LOGICAL_VIDEO_NODE
	struct camera2_node *out_node = NULL;
	struct camera2_node *cap_node = NULL;
	struct is_sub_frame *sframe;
	u32 *targetAddr;
	u64 *targetAddr_k;
	u32 pindex = 0, num_planes;
	int j, i, p, dma_type;
#endif
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

#ifdef ENABLE_LOGICAL_VIDEO_NODE
	queue = GET_SUBDEV_QUEUE(leader);
	if (!queue) {
		merr("queue is NULL", device);
		ret = -EINVAL;
		goto p_err;
	}

	if (queue->mode == CAMERA_NODE_NORMAL)
		goto p_skip;

	out_node = &frame->shot_ext->node_group.leader;
	ret = __isp_dma_in_cfg(device, subdev, frame, out_node,
		PARAM_ISP_VDMA1_INPUT, &lindex, &hindex, &indexes);

	for (i = 0; i < CAPTURE_NODE_MAX; i++) {
		cap_node = &frame->shot_ext->node_group.capture[i];

		if (!cap_node->vid)
			continue;
		dma_type = __ischain_isp_slot(cap_node, &pindex);
		if (dma_type == 1)
			ret = __isp_dma_in_cfg(device, subdev, frame, cap_node,
				pindex, &lindex, &hindex, &indexes);
		else if (dma_type == 2)
			ret = __isp_dma_out_cfg(device, subdev, frame, cap_node,
				&frame->stripe_info, pindex,
				&lindex, &hindex, &indexes, i);
	}

	/* buffer tagging */
#ifdef ENABLE_LVN_DUMMYOUTPUT
	for (i = 0; i < frame->out_node.sframe[0].num_planes; i++) {
		if (!frame->out_node.sframe[0].id) {
			mrerr("Invalid video id(%d)", device, frame,
				frame->out_node.sframe[0].id);
			return -EINVAL;
		}

		frame->dvaddr_buffer[i] = frame->out_node.sframe[0].dva[i];
		frame->kvaddr_buffer[i] = frame->out_node.sframe[0].kva[i];
	}
#endif
	for (i = 0; i < CAPTURE_NODE_MAX; i++) {
		sframe = &frame->cap_node.sframe[i];
		if (!sframe->id)
			continue;

		targetAddr = NULL;
		targetAddr_k = NULL;
		ret = is_hw_get_capture_slot(frame, &targetAddr, &targetAddr_k, sframe->id);
		if (ret) {
			mrerr("Invalid capture slot(%d)", device, frame,
				sframe->id);
			return -EINVAL;
		}

		num_planes = sframe->num_planes / frame->num_shots;

		if (targetAddr) {
			for (j = 0, p = num_planes * frame->cur_shot_idx;
				j < num_planes; j++, p++)
				targetAddr[j] = sframe->dva[p];
		}

		if (targetAddr_k) {
			/* IS_VIDEO_IMM_NUM */
			for (j = 0, p = num_planes * frame->cur_shot_idx;
				j < num_planes; j++, p++)
				targetAddr_k[j] = sframe->kva[p];
		}
	}

	if (test_bit(IS_GROUP_VOTF_OUTPUT, &device->group_isp.state)) {
		ret = __ischain_isp_set_votf(device, group, frame, &lindex, &hindex, &indexes);
		if (ret) {
			mrerr("__ischain_isp_set_votf is fail(%d)", device, frame, ret);
			goto p_err;
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
		CHECK_STRIPE_CFG(&frame->stripe_info) ||
		chg_fmt_size ||
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
