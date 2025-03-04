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

static int is_ischain_mexc_cfg(struct is_subdev *subdev,
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

static int is_ischain_mexc_tag(struct is_subdev *subdev,
	void *device_data,
	struct is_frame *ldr_frame,
	struct camera2_node *node)
{
	int ret = 0;
	struct is_queue *queue;
	struct is_device_ischain *device;
	u32 pixelformat = 0;
	u32 mch_width, mch_height;

	device = (struct is_device_ischain *)device_data;

	BUG_ON(!device);
	BUG_ON(!device->is_region);
	BUG_ON(!subdev);
	BUG_ON(!GET_SUBDEV_QUEUE(subdev));
	BUG_ON(!ldr_frame);
	BUG_ON(!ldr_frame->shot);

	mdbgs_ischain(4, "MEXC TAG(request %d)\n", device, node->request);

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
		mch_width = 8100;	/* max match(3) * max num(300) * region(9) */
		mch_height = 1;

		ret = is_ischain_buf_tag_64bit(device,
			subdev,
			ldr_frame,
			pixelformat,
			mch_width,
			mch_height,
			ldr_frame->mexcTargetAddress);
		if (ret) {
			mswarn("%d frame is drop", device, subdev, ldr_frame->fcount);
			node->request = 0;
		}
	} else {
		ldr_frame->mexcTargetAddress[0] = 0;
		ldr_frame->mexcTargetAddress[1] = 0;
		ldr_frame->mexcTargetAddress[2] = 0;
		node->request = 0;
	}

p_err:
	return ret;
}

const struct is_subdev_ops is_subdev_mexc_ops = {
	.bypass			= NULL,
	.cfg			= is_ischain_mexc_cfg,
	.tag			= is_ischain_mexc_tag,
};
