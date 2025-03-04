// SPDX-License-Identifier: GPL-2.0
/*
 * Samsung Exynos SoC series Pablo driver
 *
 * Copyright (c) 2019 Samsung Electronics Co., Ltd
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

static int is_sensor_vc2_cfg(struct is_subdev *leader,
	void *device_data,
	struct is_frame *frame,
	struct is_crop *incrop,
	struct is_crop *otcrop,
	u32 *lindex,
	u32 *hindex,
	u32 *indexes)
{
	int ret =0;

	return ret;
}

static int is_sensor_vc2_tag(struct is_subdev *subdev,
	void *device_data,
	struct is_frame *ldr_frame,
	struct camera2_node *node)
{
	int ret = 0;
	struct v4l2_subdev *subdev_csi;
	struct is_device_sensor *device;

	FIMC_BUG(!subdev);
	FIMC_BUG(!ldr_frame);

	device = (struct is_device_sensor *)device_data;
	subdev_csi = device->subdev_csi;

	if (!test_bit(IS_SUBDEV_OPEN, &subdev->state)) {
		merr("[SUB%d] is not opened", device, subdev->vid);
		ret = -EINVAL;
		goto p_err;
	}

	mdbgs_ischain(4, "VC2 TAG(request %d)\n", device, node->request);

	if (node->request) {
		ret = is_sensor_buf_tag(device,
			subdev,
			subdev_csi,
			ldr_frame);
		if (ret) {
			mswarn("%d frame is drop", device, subdev, ldr_frame->fcount);
			node->request = 0;
		}
	}

p_err:
	return ret;
}


const struct is_subdev_ops is_subdev_ssvc2_ops = {
	.bypass			= NULL,
	.cfg			= is_sensor_vc2_cfg,
	.tag			= is_sensor_vc2_tag,
};
