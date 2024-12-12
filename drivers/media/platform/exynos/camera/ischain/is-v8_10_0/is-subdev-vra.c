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

static int is_ischain_vra_cfg(struct is_subdev *leader,
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
	struct param_otf_input *otf_input;
	struct param_dma_input *dma_input;
	struct is_device_ischain *device;
	u32 width, height;

	device = (struct is_device_ischain *)device_data;

	FIMC_BUG(!leader);
	FIMC_BUG(!device);
	FIMC_BUG(!incrop);
	FIMC_BUG(!lindex);
	FIMC_BUG(!hindex);
	FIMC_BUG(!indexes);

	width = incrop->w;
	height = incrop->h;
	group = &device->group_vra;

	if (!test_bit(IS_GROUP_INIT, &group->state)) {
		mswarn("group is NOT initialized", leader, leader);
		goto p_err;
	}

	otf_input = is_itf_g_param(device, frame, PARAM_FD_OTF_INPUT);
	if (test_bit(IS_GROUP_OTF_INPUT, &group->state))
		otf_input->cmd = OTF_INPUT_COMMAND_ENABLE;
	else
		otf_input->cmd = OTF_INPUT_COMMAND_DISABLE;
	otf_input->format = OTF_YUV_FORMAT;
	otf_input->bitwidth = OTF_INPUT_BIT_WIDTH_8BIT;
	otf_input->order = OTF_INPUT_ORDER_BAYER_GR_BG;
	otf_input->width = width;
	otf_input->height = height;
	*lindex |= LOWBIT_OF(PARAM_FD_OTF_INPUT);
	*hindex |= HIGHBIT_OF(PARAM_FD_OTF_INPUT);
	(*indexes)++;

	dma_input = is_itf_g_param(device, frame, PARAM_FD_DMA_INPUT);
	if (test_bit(IS_GROUP_OTF_INPUT, &group->state))
		dma_input->cmd = DMA_INPUT_COMMAND_DISABLE;
	else
		dma_input->cmd = DMA_INPUT_COMMAND_ENABLE;
	dma_input->format = DMA_INOUT_FORMAT_YUV422;
	dma_input->bitwidth = DMA_INOUT_BIT_WIDTH_8BIT;
	dma_input->order = DMA_INOUT_ORDER_CrCb;
	dma_input->plane = DMA_INOUT_PLANE_2;
	dma_input->width = width;
	dma_input->height = height;
	*lindex |= LOWBIT_OF(PARAM_FD_DMA_INPUT);
	*hindex |= HIGHBIT_OF(PARAM_FD_DMA_INPUT);
	(*indexes)++;

	leader->input.crop = *incrop;

p_err:
	return ret;
}

static int is_ischain_vra_tag(struct is_subdev *subdev,
	void *device_data,
	struct is_frame *frame,
	struct camera2_node *node)
{
	int ret = 0;
	struct is_group *group;
	struct is_crop inparm;
	struct is_crop *incrop, *otcrop;
	struct vra_param *vra_param;
	struct is_subdev *leader;
	struct is_device_ischain *device;
	u32 lindex, hindex, indexes;

	device = (struct is_device_ischain *)device_data;

	FIMC_BUG(!subdev);
	FIMC_BUG(!device);
	FIMC_BUG(!device->is_region);
	FIMC_BUG(!frame);
	FIMC_BUG(!frame->shot);

	incrop = (struct is_crop *)node->input.cropRegion;
	otcrop = (struct is_crop *)node->output.cropRegion;

	group = &device->group_vra;
	leader = subdev->leader;
	lindex = hindex = indexes = 0;
	vra_param = &device->is_region->parameter.vra;

	mdbgs_ischain(4, "VRA TAG\n", device);

	if (test_bit(IS_GROUP_OTF_INPUT, &group->state)) {
		inparm.x = 0;
		inparm.y = 0;
		inparm.w = vra_param->otf_input.width;
		inparm.h = vra_param->otf_input.height;
	} else {
		inparm.x = 0;
		inparm.y = 0;
		inparm.w = vra_param->dma_input.width;
		inparm.h = vra_param->dma_input.height;
	}

	if (IS_NULL_CROP(incrop))
		*incrop = inparm;

	if (!COMPARE_CROP(incrop, &inparm)||
		test_bit(IS_SUBDEV_FORCE_SET, &leader->state)) {
		ret = is_ischain_vra_cfg(subdev,
			device,
			frame,
			incrop,
			otcrop,
			&lindex,
			&hindex,
			&indexes);
		if (ret) {
			merr("is_ischain_vra_cfg is fail(%d)", device, ret);
			goto p_err;
		}

		msrinfo("in_crop[%d, %d, %d, %d]\n", device, subdev, frame,
			incrop->x, incrop->y, incrop->w, incrop->h);
	}

	ret = is_itf_s_param(device, frame, lindex, hindex, indexes);
	if (ret) {
		mrerr("is_itf_s_param is fail(%d)", device, frame, ret);
		goto p_err;
	}

p_err:
	return ret;
}

const struct is_subdev_ops is_subdev_vra_ops = {
	.bypass			= NULL,
	.cfg			= is_ischain_vra_cfg,
	.tag			= is_ischain_vra_tag,
};
