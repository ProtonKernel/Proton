/*
 * Samsung Exynos5 SoC series FIMC-IS driver
 *
 * exynos5 fimc-is video functions
 *
 * Copyright (c) 2011 Samsung Electronics Co., Ltd
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

 #include <linux/module.h>
#include <asm/cacheflush.h>

#include "is-core.h"
#include "is-param.h"
#include "is-device-ischain.h"
#include "is-debug.h"
#include "is-mem.h"
#if defined(SDC_HEADER_GEN)
#include "is-sdc-header.h"
#endif
#include "is-hw-common-dma.h"

bool is_subdev_check_vid(uint32_t vid)
{
	bool is_valid = true;

	if (vid == 0)
		is_valid = false;
	else if (vid >= IS_VIDEO_MAX_NUM)
		is_valid = false;
#ifdef ENABLE_LOGICAL_VIDEO_NODE
	else if (vid >= IS_VIDEO_LVN_BASE)
		is_valid = false;
#endif

	return is_valid;
}

struct is_subdev * video2subdev(enum is_subdev_device_type device_type,
	void *device, u32 vid)
{
	struct is_subdev *subdev = NULL;
	struct is_device_sensor *sensor = NULL;
	struct is_device_ischain *ischain = NULL;

	if (device_type == IS_SENSOR_SUBDEV) {
		sensor = (struct is_device_sensor *)device;
	} else {
		ischain = (struct is_device_ischain *)device;
		sensor = ischain->sensor;
	}

	switch (vid) {
	case IS_VIDEO_SS0VC0_NUM:
	case IS_VIDEO_SS1VC0_NUM:
	case IS_VIDEO_SS2VC0_NUM:
	case IS_VIDEO_SS3VC0_NUM:
	case IS_VIDEO_SS4VC0_NUM:
	case IS_VIDEO_SS5VC0_NUM:
		subdev = &sensor->ssvc0;
		break;
	case IS_VIDEO_SS0VC1_NUM:
	case IS_VIDEO_SS1VC1_NUM:
	case IS_VIDEO_SS2VC1_NUM:
	case IS_VIDEO_SS3VC1_NUM:
	case IS_VIDEO_SS4VC1_NUM:
	case IS_VIDEO_SS5VC1_NUM:
		subdev = &sensor->ssvc1;
		break;
	case IS_VIDEO_SS0VC2_NUM:
	case IS_VIDEO_SS1VC2_NUM:
	case IS_VIDEO_SS2VC2_NUM:
	case IS_VIDEO_SS3VC2_NUM:
	case IS_VIDEO_SS4VC2_NUM:
	case IS_VIDEO_SS5VC2_NUM:
		subdev = &sensor->ssvc2;
		break;
	case IS_VIDEO_SS0VC3_NUM:
	case IS_VIDEO_SS1VC3_NUM:
	case IS_VIDEO_SS2VC3_NUM:
	case IS_VIDEO_SS3VC3_NUM:
	case IS_VIDEO_SS4VC3_NUM:
	case IS_VIDEO_SS5VC3_NUM:
		subdev = &sensor->ssvc3;
		break;
	case IS_VIDEO_30S_NUM:
	case IS_VIDEO_31S_NUM:
	case IS_VIDEO_32S_NUM:
	case IS_VIDEO_33S_NUM:
		subdev = &ischain->group_3aa.leader;
		break;
	case IS_VIDEO_30C_NUM:
	case IS_VIDEO_31C_NUM:
	case IS_VIDEO_32C_NUM:
	case IS_VIDEO_33C_NUM:
		subdev = &ischain->txc;
		break;
	case IS_VIDEO_30P_NUM:
	case IS_VIDEO_31P_NUM:
	case IS_VIDEO_32P_NUM:
	case IS_VIDEO_33P_NUM:
		subdev = &ischain->txp;
		break;
	case IS_VIDEO_30F_NUM:
	case IS_VIDEO_31F_NUM:
	case IS_VIDEO_32F_NUM:
	case IS_VIDEO_33F_NUM:
		subdev = &ischain->txf;
		break;
	case IS_VIDEO_30G_NUM:
	case IS_VIDEO_31G_NUM:
	case IS_VIDEO_32G_NUM:
	case IS_VIDEO_33G_NUM:
		subdev = &ischain->txg;
		break;
	case IS_VIDEO_30O_NUM:
	case IS_VIDEO_31O_NUM:
	case IS_VIDEO_32O_NUM:
	case IS_VIDEO_33O_NUM:
		subdev = &ischain->txo;
		break;
	case IS_VIDEO_30L_NUM:
	case IS_VIDEO_31L_NUM:
	case IS_VIDEO_32L_NUM:
	case IS_VIDEO_33L_NUM:
		subdev = &ischain->txl;
		break;
	case IS_VIDEO_LME0_NUM:
	case IS_VIDEO_LME1_NUM:
		subdev = &ischain->group_lme.leader;
		break;
	case IS_VIDEO_LME0S_NUM:
	case IS_VIDEO_LME1S_NUM:
		subdev = &ischain->lmes;
		break;
	case IS_VIDEO_LME0C_NUM:
	case IS_VIDEO_LME1C_NUM:
		subdev = &ischain->lmec;
		break;
	case IS_VIDEO_ORB0C_NUM:
	case IS_VIDEO_ORB1C_NUM:
		subdev = &ischain->orbxc;
		break;
	case IS_VIDEO_I0S_NUM:
	case IS_VIDEO_I1S_NUM:
		subdev = &ischain->group_isp.leader;
		break;
	case IS_VIDEO_I0C_NUM:
	case IS_VIDEO_I1C_NUM:
		subdev = &ischain->ixc;
		break;
	case IS_VIDEO_I0P_NUM:
	case IS_VIDEO_I1P_NUM:
		subdev = &ischain->ixp;
		break;
	case IS_VIDEO_I0T_NUM:
		subdev = &ischain->ixt;
		break;
	case IS_VIDEO_I0G_NUM:
		subdev = &ischain->ixg;
		break;
	case IS_VIDEO_I0V_NUM:
		subdev = &ischain->ixv;
		break;
	case IS_VIDEO_I0W_NUM:
		subdev = &ischain->ixw;
		break;
	case IS_VIDEO_ME0C_NUM:
	case IS_VIDEO_ME1C_NUM:
		subdev = &ischain->mexc;
		break;
	case IS_VIDEO_M0S_NUM:
	case IS_VIDEO_M1S_NUM:
		subdev = &ischain->group_mcs.leader;
		break;
	case IS_VIDEO_M0P_NUM:
		subdev = &ischain->m0p;
		break;
	case IS_VIDEO_M1P_NUM:
		subdev = &ischain->m1p;
		break;
	case IS_VIDEO_M2P_NUM:
		subdev = &ischain->m2p;
		break;
	case IS_VIDEO_M3P_NUM:
		subdev = &ischain->m3p;
		break;
	case IS_VIDEO_M4P_NUM:
		subdev = &ischain->m4p;
		break;
	case IS_VIDEO_M5P_NUM:
		subdev = &ischain->m5p;
		break;
	case IS_VIDEO_VRA_NUM:
		subdev = &ischain->group_vra.leader;
		break;
	case IS_VIDEO_CLH0S_NUM:
		subdev = &ischain->group_clh.leader;
		break;
	case IS_VIDEO_CLH0C_NUM:
		subdev = &ischain->clhc;
		break;
	default:
		err("[%d] vid %d is NOT found", ((device_type == IS_SENSOR_SUBDEV) ?
				 (ischain ? ischain->instance : 0) : (sensor ? sensor->device_id : 0)), vid);
		break;
	}

	return subdev;
}

int is_subdev_probe(struct is_subdev *subdev,
	u32 instance,
	u32 id,
	char *name,
	const struct is_subdev_ops *sops)
{
	FIMC_BUG(!subdev);
	FIMC_BUG(!name);

	subdev->id = id;
	subdev->wq_id = is_subdev_wq_id[id];
	subdev->instance = instance;
	subdev->ops = sops;
	memset(subdev->name, 0x0, sizeof(subdev->name));
	strncpy(subdev->name, name, sizeof(char[3]));
	clear_bit(IS_SUBDEV_OPEN, &subdev->state);
	clear_bit(IS_SUBDEV_RUN, &subdev->state);
	clear_bit(IS_SUBDEV_START, &subdev->state);
	clear_bit(IS_SUBDEV_VOTF_USE, &subdev->state);

	/* for internal use */
	clear_bit(IS_SUBDEV_INTERNAL_S_FMT, &subdev->state);
	frame_manager_probe(&subdev->internal_framemgr, subdev->id, name);

	return 0;
}

int is_subdev_open(struct is_subdev *subdev,
	struct is_video_ctx *vctx,
	void *ctl_data,
	u32 instance)
{
	int ret = 0;
	struct is_video *video;
	const struct param_control *init_ctl = (const struct param_control *)ctl_data;

	FIMC_BUG(!subdev);

	if (test_bit(IS_SUBDEV_OPEN, &subdev->state)) {
		mserr("already open", subdev, subdev);
		ret = -EPERM;
		goto p_err;
	}

	/* If it is internal VC, skip vctx setting. */
	if (vctx) {
		subdev->vctx = vctx;
		video = GET_VIDEO(vctx);
		subdev->vid = (video) ? video->id : 0;
	}

	subdev->instance = instance;
	subdev->cid = CAPTURE_NODE_MAX;
	subdev->input.width = 0;
	subdev->input.height = 0;
	subdev->input.crop.x = 0;
	subdev->input.crop.y = 0;
	subdev->input.crop.w = 0;
	subdev->input.crop.h = 0;
	subdev->output.width = 0;
	subdev->output.height = 0;
	subdev->output.crop.x = 0;
	subdev->output.crop.y = 0;
	subdev->output.crop.w = 0;
	subdev->output.crop.h = 0;

	if (init_ctl) {
		set_bit(IS_SUBDEV_START, &subdev->state);

		if (subdev->id == ENTRY_VRA) {
			/* vra only control by command for enabling or disabling */
			if (init_ctl->cmd == CONTROL_COMMAND_STOP)
				clear_bit(IS_SUBDEV_RUN, &subdev->state);
			else
				set_bit(IS_SUBDEV_RUN, &subdev->state);
		} else {
			if (init_ctl->bypass == CONTROL_BYPASS_ENABLE)
				clear_bit(IS_SUBDEV_RUN, &subdev->state);
			else
				set_bit(IS_SUBDEV_RUN, &subdev->state);
		}
	} else {
		clear_bit(IS_SUBDEV_START, &subdev->state);
		clear_bit(IS_SUBDEV_RUN, &subdev->state);
	}

	set_bit(IS_SUBDEV_OPEN, &subdev->state);

p_err:
	return ret;
}

int is_sensor_subdev_open(struct is_device_sensor *device,
	struct is_video_ctx *vctx)
{
	int ret = 0;
	struct is_subdev *subdev;

	FIMC_BUG(!device);
	FIMC_BUG(!vctx);
	FIMC_BUG(!GET_VIDEO(vctx));

	subdev = video2subdev(IS_SENSOR_SUBDEV, (void *)device, GET_VIDEO(vctx)->id);
	if (!subdev) {
		merr("video2subdev is fail", device);
		ret = -EINVAL;
		goto err_video2subdev;
	}

	ret = is_subdev_open(subdev, vctx, NULL, device->instance);
	if (ret) {
		mserr("is_subdev_open is fail(%d)", subdev, subdev, ret);
		goto err_subdev_open;
	}

	vctx->subdev = subdev;

	return 0;

err_subdev_open:
err_video2subdev:
	return ret;
}

int is_ischain_subdev_open(struct is_device_ischain *device,
	struct is_video_ctx *vctx)
{
	int ret = 0;
	int ret_err = 0;
	struct is_subdev *subdev;

	FIMC_BUG(!device);
	FIMC_BUG(!vctx);
	FIMC_BUG(!GET_VIDEO(vctx));

	subdev = video2subdev(IS_ISCHAIN_SUBDEV, (void *)device, GET_VIDEO(vctx)->id);
	if (!subdev) {
		merr("video2subdev is fail", device);
		ret = -EINVAL;
		goto err_video2subdev;
	}

	vctx->subdev = subdev;

	ret = is_subdev_open(subdev, vctx, NULL, device->instance);
	if (ret) {
		merr("is_subdev_open is fail(%d)", device, ret);
		goto err_subdev_open;
	}

	ret = is_ischain_open_wrap(device, false);
	if (ret) {
		merr("is_ischain_open_wrap is fail(%d)", device, ret);
		goto err_ischain_open;
	}

	return 0;

err_ischain_open:
	ret_err = is_subdev_close(subdev);
	if (ret_err)
		merr("is_subdev_close is fail(%d)", device, ret_err);
err_subdev_open:
err_video2subdev:
	return ret;
}

int is_subdev_close(struct is_subdev *subdev)
{
	int ret = 0;

	if (!test_bit(IS_SUBDEV_OPEN, &subdev->state)) {
		msinfo("subdev is already close", subdev, subdev, __func__);
		ret = -EINVAL;
		goto p_err;
	}

	subdev->leader = NULL;
	subdev->vctx = NULL;
	subdev->vid = 0;

	clear_bit(IS_SUBDEV_OPEN, &subdev->state);
	clear_bit(IS_SUBDEV_RUN, &subdev->state);
	clear_bit(IS_SUBDEV_START, &subdev->state);
	clear_bit(IS_SUBDEV_FORCE_SET, &subdev->state);
	clear_bit(IS_SUBDEV_VOTF_USE, &subdev->state);

	/* for internal use */
	clear_bit(IS_SUBDEV_INTERNAL_S_FMT, &subdev->state);

p_err:
	return 0;
}

int is_sensor_subdev_close(struct is_device_sensor *device,
	struct is_video_ctx *vctx)
{
	int ret = 0;
	struct is_subdev *subdev;

	FIMC_BUG(!device);
	FIMC_BUG(!vctx);

	subdev = vctx->subdev;
	if (!subdev) {
		merr("subdev is NULL", device);
		ret = -EINVAL;
		goto p_err;
	}

	vctx->subdev = NULL;

	if (test_bit(IS_SENSOR_FRONT_START, &device->state)) {
		merr("sudden close, call sensor_front_stop()\n", device);

		ret = is_sensor_front_stop(device, true);
		if (ret)
			merr("is_sensor_front_stop is fail(%d)", device, ret);
	}

	ret = is_subdev_close(subdev);
	if (ret)
		merr("is_subdev_close is fail(%d)", device, ret);

p_err:
	return ret;
}

int is_ischain_subdev_close(struct is_device_ischain *device,
	struct is_video_ctx *vctx)
{
	int ret = 0;
	struct is_subdev *subdev;

	FIMC_BUG(!device);
	FIMC_BUG(!vctx);

	subdev = vctx->subdev;
	if (!subdev) {
		merr("subdev is NULL", device);
		ret = -EINVAL;
		goto p_err;
	}

	vctx->subdev = NULL;

	ret = is_subdev_close(subdev);
	if (ret) {
		merr("is_subdev_close is fail(%d)", device, ret);
		goto p_err;
	}

	ret = is_ischain_close_wrap(device);
	if (ret) {
		merr("is_ischain_close_wrap is fail(%d)", device, ret);
		goto p_err;
	}

p_err:
	return ret;
}

static int is_subdev_start(struct is_subdev *subdev)
{
	int ret = 0;
	struct is_subdev *leader;

	FIMC_BUG(!subdev);
	FIMC_BUG(!subdev->leader);

	leader = subdev->leader;

	if (test_bit(IS_SUBDEV_START, &subdev->state)) {
		msinfo("[%s] already start", subdev, subdev, __func__);
		goto p_err;
	}

	if (test_bit(IS_SUBDEV_START, &leader->state)) {
		mserr("leader%d is ALREADY started", subdev, subdev, leader->id);
		goto p_err;
	}

	set_bit(IS_SUBDEV_START, &subdev->state);

p_err:
	return ret;
}

static int is_sensor_subdev_start(void *qdevice,
	struct is_queue *queue)
{
	int ret = 0;
	struct is_device_sensor *device = qdevice;
	struct is_video_ctx *vctx;
	struct is_subdev *subdev;

	FIMC_BUG(!device);
	FIMC_BUG(!queue);

	vctx = container_of(queue, struct is_video_ctx, queue);
	subdev = vctx->subdev;
	if (!subdev) {
		merr("subdev is NULL", device);
		ret = -EINVAL;
		goto p_err;
	}

	if (!test_bit(IS_SENSOR_S_INPUT, &device->state)) {
		mserr("device is not yet init", device, subdev);
		ret = -EINVAL;
		goto p_err;
	}

	if (test_bit(IS_SUBDEV_START, &subdev->state)) {
		mserr("already start", subdev, subdev);
		goto p_err;
	}

	set_bit(IS_SUBDEV_START, &subdev->state);

p_err:
	return ret;
}

static int is_ischain_subdev_start(void *qdevice,
	struct is_queue *queue)
{
	int ret = 0;
	struct is_device_ischain *device = qdevice;
	struct is_video_ctx *vctx;
	struct is_subdev *subdev;

	FIMC_BUG(!device);
	FIMC_BUG(!queue);

	vctx = container_of(queue, struct is_video_ctx, queue);
	subdev = vctx->subdev;
	if (!subdev) {
		merr("subdev is NULL", device);
		ret = -EINVAL;
		goto p_err;
	}

	if (!test_bit(IS_ISCHAIN_INIT, &device->state)) {
		mserr("device is not yet init", device, subdev);
		ret = -EINVAL;
		goto p_err;
	}

	ret = is_subdev_start(subdev);
	if (ret) {
		mserr("is_subdev_start is fail(%d)", device, subdev, ret);
		goto p_err;
	}

p_err:
	return ret;
}

static int is_subdev_stop(struct is_subdev *subdev)
{
	int ret = 0;
	unsigned long flags;
	struct is_subdev *leader;
	struct is_framemgr *framemgr;
	struct is_frame *frame;
	struct is_device_ischain *device;

	FIMC_BUG(!subdev);
	FIMC_BUG(!subdev->leader);
	FIMC_BUG(!subdev->vctx);

	leader = subdev->leader;
	framemgr = GET_SUBDEV_FRAMEMGR(subdev);
	device = GET_DEVICE(subdev->vctx);
	FIMC_BUG(!framemgr);

	if (!test_bit(IS_SUBDEV_START, &subdev->state)) {
		merr("already stop", device);
		goto p_err;
	}

	if (test_bit(IS_SUBDEV_START, &leader->state)) {
		merr("leader%d is NOT stopped", device, leader->id);
		goto p_err;
	}

	framemgr_e_barrier_irqs(framemgr, FMGR_IDX_16, flags);

	if (framemgr->queued_count[FS_PROCESS] > 0) {
		framemgr_x_barrier_irqr(framemgr, FMGR_IDX_16, flags);
		merr("being processed, can't stop", device);
		ret = -EINVAL;
		goto p_err;
	}

	frame = peek_frame(framemgr, FS_REQUEST);
	while (frame) {
		CALL_VOPS(subdev->vctx, done, frame->index, VB2_BUF_STATE_ERROR);
		trans_frame(framemgr, frame, FS_COMPLETE);
		frame = peek_frame(framemgr, FS_REQUEST);
	}

	framemgr_x_barrier_irqr(framemgr, FMGR_IDX_16, flags);

	clear_bit(IS_SUBDEV_RUN, &subdev->state);
	clear_bit(IS_SUBDEV_START, &subdev->state);
	clear_bit(IS_SUBDEV_VOTF_USE, &subdev->state);

p_err:
	return ret;
}

static int is_sensor_subdev_stop(void *qdevice,
	struct is_queue *queue)
{
	int ret = 0;
	unsigned long flags;
	struct is_device_sensor *device = qdevice;
	struct is_video_ctx *vctx;
	struct is_subdev *subdev;
	struct is_framemgr *framemgr;
	struct is_frame *frame;

	FIMC_BUG(!device);
	FIMC_BUG(!queue);

	vctx = container_of(queue, struct is_video_ctx, queue);
	subdev = vctx->subdev;
	if (!subdev) {
		merr("subdev is NULL", device);
		ret = -EINVAL;
		goto p_err;
	}

	if (!test_bit(IS_SUBDEV_START, &subdev->state)) {
		minfo("[%s]already stop", device, __func__);
		goto p_err;
	}

	framemgr = GET_SUBDEV_FRAMEMGR(subdev);
	if (!framemgr) {
		merr("framemgr is NULL", device);
		ret = -EINVAL;
		goto p_err;
	}
	framemgr_e_barrier_irqs(framemgr, FMGR_IDX_16, flags);

	frame = peek_frame(framemgr, FS_PROCESS);
	while (frame) {
		CALL_VOPS(subdev->vctx, done, frame->index, VB2_BUF_STATE_ERROR);
		trans_frame(framemgr, frame, FS_COMPLETE);
		frame = peek_frame(framemgr, FS_PROCESS);
	}

	frame = peek_frame(framemgr, FS_REQUEST);
	while (frame) {
		CALL_VOPS(subdev->vctx, done, frame->index, VB2_BUF_STATE_ERROR);
		trans_frame(framemgr, frame, FS_COMPLETE);
		frame = peek_frame(framemgr, FS_REQUEST);
	}

	framemgr_x_barrier_irqr(framemgr, FMGR_IDX_16, flags);

	clear_bit(IS_SUBDEV_RUN, &subdev->state);
	clear_bit(IS_SUBDEV_START, &subdev->state);
	clear_bit(IS_SUBDEV_VOTF_USE, &subdev->state);

p_err:
	return ret;
}

static int is_ischain_subdev_stop(void *qdevice,
	struct is_queue *queue)
{
	int ret = 0;
	struct is_device_ischain *device = qdevice;
	struct is_video_ctx *vctx;
	struct is_subdev *subdev;

	FIMC_BUG(!device);
	FIMC_BUG(!queue);

	vctx = container_of(queue, struct is_video_ctx, queue);
	subdev = vctx->subdev;
	if (!subdev) {
		merr("subdev is NULL", device);
		ret = -EINVAL;
		goto p_err;
	}

	ret = is_subdev_stop(subdev);
	if (ret) {
		merr("is_subdev_stop is fail(%d)", device, ret);
		goto p_err;
	}

p_err:
	return ret;
}

static int is_subdev_s_format(struct is_subdev *subdev,
	struct is_queue *queue)
{
	int ret = 0;
	u32 pixelformat = 0, width, height;

	FIMC_BUG(!subdev);
	FIMC_BUG(!subdev->vctx);
	FIMC_BUG(!queue);
	FIMC_BUG(!queue->framecfg.format);

	pixelformat = queue->framecfg.format->pixelformat;

	width = queue->framecfg.width;
	height = queue->framecfg.height;

	switch (subdev->id) {
	case ENTRY_M0P:
	case ENTRY_M1P:
	case ENTRY_M2P:
	case ENTRY_M3P:
	case ENTRY_M4P:
		if (width % 8) {
			mserr("width(%d) of format(%d) is not multiple of 8: need to check size",
				subdev, subdev, width, pixelformat);
		}
		break;
	default:
		break;
	}

	subdev->output.width = width;
	subdev->output.height = height;

	subdev->output.crop.x = 0;
	subdev->output.crop.y = 0;
	subdev->output.crop.w = subdev->output.width;
	subdev->output.crop.h = subdev->output.height;

	return ret;
}

static int is_sensor_subdev_s_format(void *qdevice,
	struct is_queue *queue)
{
	int ret = 0;
	struct is_device_sensor *device = qdevice;
	struct is_video_ctx *vctx;
	struct is_subdev *subdev;

	FIMC_BUG(!device);
	FIMC_BUG(!queue);

	vctx = container_of(queue, struct is_video_ctx, queue);
	subdev = vctx->subdev;
	if (!subdev) {
		merr("subdev is NULL", device);
		ret = -EINVAL;
		goto p_err;
	}

	if (test_bit(IS_SUBDEV_INTERNAL_USE, &subdev->state)) {
		msinfo("%s: It is sharing with internal use.", subdev, subdev, __func__);
	} else {
		ret = is_subdev_s_format(subdev, queue);
		if (ret) {
			merr("is_subdev_s_format is fail(%d)", device, ret);
			goto p_err;
		}
	}

p_err:
	return ret;
}

static int is_ischain_subdev_s_format(void *qdevice,
	struct is_queue *queue)
{
	int ret = 0;
	struct is_device_ischain *device = qdevice;
	struct is_video_ctx *vctx;
	struct is_subdev *subdev;

	FIMC_BUG(!device);
	FIMC_BUG(!queue);

	vctx = container_of(queue, struct is_video_ctx, queue);
	subdev = vctx->subdev;
	if (!subdev) {
		merr("subdev is NULL", device);
		ret = -EINVAL;
		goto p_err;
	}

	ret = is_subdev_s_format(subdev, queue);
	if (ret) {
		merr("is_subdev_s_format is fail(%d)", device, ret);
		goto p_err;
	}

p_err:
	return ret;
}

int is_sensor_subdev_reqbuf(void *qdevice,
	struct is_queue *queue, u32 count)
{
	int i = 0;
	int ret = 0;
	struct is_device_sensor *device = qdevice;
	struct is_video_ctx *vctx;
	struct is_subdev *subdev;
	struct is_framemgr *framemgr;
	struct is_frame *frame;

	FIMC_BUG(!device);
	FIMC_BUG(!queue);

	if (!count)
		goto p_err;

	vctx = container_of(queue, struct is_video_ctx, queue);
	subdev = vctx->subdev;
	if (!subdev) {
		merr("subdev is NULL", device);
		ret = -EINVAL;
		goto p_err;
	}

	framemgr = GET_SUBDEV_FRAMEMGR(subdev);
	if (!framemgr) {
		merr("framemgr is NULL", device);
		ret = -EINVAL;
		goto p_err;
	}

	for (i = 0; i < count; i++) {
		frame = &framemgr->frames[i];
		frame->subdev = subdev;
	}

p_err:
	return ret;
}

int is_subdev_buffer_queue(struct is_subdev *subdev,
	struct vb2_buffer *vb)
{
	int ret = 0;
	unsigned long flags;
	struct is_framemgr *framemgr = GET_SUBDEV_FRAMEMGR(subdev);
	struct is_frame *frame;
	unsigned int index = vb->index;

	FIMC_BUG(!subdev);
	FIMC_BUG(!framemgr);
	FIMC_BUG(index >= framemgr->num_frames);

	frame = &framemgr->frames[index];

	/* 1. check frame validation */
	if (unlikely(!test_bit(FRAME_MEM_INIT, &frame->mem_state))) {
		mserr("frame %d is NOT init", subdev, subdev, index);
		ret = EINVAL;
		goto p_err;
	}

	/* 2. update frame manager */
	framemgr_e_barrier_irqs(framemgr, FMGR_IDX_17, flags);

	if (frame->state == FS_FREE) {
		trans_frame(framemgr, frame, FS_REQUEST);
	} else {
		mserr("frame %d is invalid state(%d)\n", subdev, subdev, index, frame->state);
		frame_manager_print_queues(framemgr);
		ret = -EINVAL;
	}

	framemgr_x_barrier_irqr(framemgr, FMGR_IDX_17, flags);

p_err:
	return ret;
}

int is_subdev_buffer_finish(struct is_subdev *subdev,
	struct vb2_buffer *vb)
{
	int ret = 0;
	struct is_device_ischain *device;
	struct is_framemgr *framemgr;
	struct is_frame *frame;
	unsigned int index = vb->index;

	if (!subdev) {
		warn("subdev is NULL(%d)", index);
		ret = -EINVAL;
		return ret;
	}

	if (unlikely(!test_bit(IS_SUBDEV_OPEN, &subdev->state))) {
		warn("subdev was closed..(%d)", index);
		ret = -EINVAL;
		return ret;
	}

	FIMC_BUG(!subdev->vctx);

	device = GET_DEVICE(subdev->vctx);
	framemgr = GET_SUBDEV_FRAMEMGR(subdev);
	if (unlikely(!framemgr)) {
		warn("subdev's framemgr is null..(%d)", index);
		ret = -EINVAL;
		return ret;
	}

	FIMC_BUG(index >= framemgr->num_frames);

	frame = &framemgr->frames[index];
	framemgr_e_barrier_irq(framemgr, FMGR_IDX_18);

	if (frame->state == FS_COMPLETE) {
		trans_frame(framemgr, frame, FS_FREE);
	} else {
		merr("frame is empty from complete", device);
		merr("frame(%d) is not com state(%d)", device,
					index, frame->state);
		frame_manager_print_queues(framemgr);
		ret = -EINVAL;
	}

	framemgr_x_barrier_irq(framemgr, FMGR_IDX_18);

	return ret;
}

const struct is_queue_ops is_sensor_subdev_ops = {
	.start_streaming	= is_sensor_subdev_start,
	.stop_streaming		= is_sensor_subdev_stop,
	.s_format		= is_sensor_subdev_s_format,
	.request_bufs		= is_sensor_subdev_reqbuf,
};

const struct is_queue_ops is_ischain_subdev_ops = {
	.start_streaming	= is_ischain_subdev_start,
	.stop_streaming		= is_ischain_subdev_stop,
	.s_format		= is_ischain_subdev_s_format
};

static bool is_subdev_internal_use_shared_framemgr(const struct is_subdev *subdev)
{
	struct is_core *core;
	struct is_device_ischain *device;
	struct is_device_sensor *sensor;

	switch (subdev->id) {
	case ENTRY_YPP:
		if (IS_ENABLED(USE_YPP_SHARED_INTERNAL_BUFFER)) {
			core = (struct is_core *)dev_get_drvdata(is_dev);
			device = GET_DEVICE(subdev->vctx);
			sensor = device->sensor;

			if (!sensor || !sensor->cfg) {
				mswarn(" sensor config is not valid", subdev, subdev);
				return false;
			}

			if (core->scenario == IS_SCENARIO_SECURE ||
				sensor->cfg->special_mode == IS_SPECIAL_MODE_FASTAE)
				return false;
			else
				return true;
		}
		fallthrough;
	default:
		return false;
	}
}

static int is_subdev_internal_get_buffer_size(const struct is_subdev *subdev,
	u32 *width, u32 *height, u32 *sbwc_en)
{
	struct is_core *core = (struct is_core *)dev_get_drvdata(is_dev);
	u32 max_width, max_height;

	switch (subdev->id) {
	case ENTRY_YPP:
		if (subdev->use_shared_framemgr) {
			is_sensor_g_max_size(&max_width, &max_height);

			if (core->hardware.ypp_internal_buf_max_width)
				*width = MIN(max_width, core->hardware.ypp_internal_buf_max_width);
			else
				*width = MIN(max_width, subdev->leader->constraints_width);

			*height = max_height;

			if (core->vender.isp_max_width)
				*width = MIN(*width, core->vender.isp_max_width);
			if (core->vender.isp_max_height)
				*height = core->vender.isp_max_height;
		} else {
			*width = MIN(subdev->output.width, subdev->leader->constraints_width);
			*height = subdev->output.height;
		}
		*sbwc_en = 1;
		break;
	default:
		*width = subdev->output.width;
		*height = subdev->output.height;
		*sbwc_en = 0;
		break;
	}

	return 0;
}

static void is_subdev_internal_lock_shared_framemgr(struct is_subdev *subdev)
{
	struct is_groupmgr *groupmgr;
	struct is_core *core;

	switch (subdev->id) {
	case ENTRY_YPP:
		if (subdev->use_shared_framemgr) {
			core = (struct is_core *)dev_get_drvdata(is_dev);
			groupmgr = &core->groupmgr;

			mutex_lock(&groupmgr->shared_framemgr[GROUP_ID_YPP].mlock);
		}
		break;
	default:
		break;
	}
}

static void is_subdev_internal_unlock_shared_framemgr(struct is_subdev *subdev)
{
	struct is_groupmgr *groupmgr;
	struct is_core *core;

	switch (subdev->id) {
	case ENTRY_YPP:
		if (subdev->use_shared_framemgr) {
			core = (struct is_core *)dev_get_drvdata(is_dev);
			groupmgr = &core->groupmgr;

			mutex_unlock(&groupmgr->shared_framemgr[GROUP_ID_YPP].mlock);
		}
		break;
	default:
		break;
	}
}

static int is_subdev_internal_get_shared_framemgr(struct is_subdev *subdev,
	struct is_framemgr **framemgr, u32 width, u32 height)
{
	int ret = 0;
	struct is_groupmgr *groupmgr;
	struct is_core *core;
	int prev_refcount;
	struct is_shared_framemgr *shared_framemgr;

	switch (subdev->id) {
	case ENTRY_YPP:
		if (subdev->use_shared_framemgr) {
			core = (struct is_core *)dev_get_drvdata(is_dev);
			groupmgr = &core->groupmgr;
			shared_framemgr = &groupmgr->shared_framemgr[GROUP_ID_YPP];

			prev_refcount = refcount_read(&shared_framemgr->refcount);
			if (!prev_refcount) {
				ret = frame_manager_open(&shared_framemgr->framemgr,
					subdev->buffer_num);
				if (ret < 0) {
					mserr("frame_manager_open is fail(%d)",
						subdev, subdev, ret);
					return ret;
				}
				shared_framemgr->width = width;
				shared_framemgr->height = height;
				refcount_set(&shared_framemgr->refcount, 1);
			} else {
				if (shared_framemgr->width != width ||
					shared_framemgr->height != height) {
					mserr("shared_framemgr size(%d x %d) is not matched with (%d x %d)\n",
						subdev, subdev,
						shared_framemgr->width, shared_framemgr->height,
						width, height);
					return -EINVAL;
				}
				refcount_inc(&shared_framemgr->refcount);
			}
			*framemgr = &shared_framemgr->framemgr;

			return prev_refcount;
		}
		fallthrough;
	default:
		*framemgr = NULL;
		break;
	}

	return ret;
}

static int is_subdev_internal_put_shared_framemgr(struct is_subdev *subdev)
{
	struct is_groupmgr *groupmgr;
	struct is_core *core;
	struct is_shared_framemgr *shared_framemgr;

	switch (subdev->id) {
	case ENTRY_YPP:
		if (subdev->use_shared_framemgr) {
			core = (struct is_core *)dev_get_drvdata(is_dev);
			groupmgr = &core->groupmgr;
			shared_framemgr = &groupmgr->shared_framemgr[GROUP_ID_YPP];

			if (refcount_dec_and_test(&shared_framemgr->refcount))
				frame_manager_close(&shared_framemgr->framemgr);

			return refcount_read(&shared_framemgr->refcount);
		}
		fallthrough;
	default:
		break;
	}

	return 0;
}

#ifdef ENABLE_LOGICAL_VIDEO_NODE
static int __is_subdev_internal_get_cap_node_num(const struct is_subdev *subdev)
{
	int ret = 0;

	switch (subdev->id) {
	case ENTRY_YPP:
		if (IS_ENABLED(YPP_DDK_LIB_CALL))
			ret = 4;
		else
			ret = 0;
		break;
	default:
		ret = 0;
		break;
	}

	return ret;
}

static int __is_subdev_internal_get_cap_node_info(const struct is_subdev *subdev, u32 *vid,
	u32 *num_planes, u32 *buffer_size, u32 index)
{
	int ret = 0;
	u32 l0_width;
	u32 l0_height;
	u32 l2_width;
	u32 l2_height;
	u32 payload_size, header_size;
	u32 sbwc_en;

	*vid = IS_VIDEO_YND_NUM + index;
	is_subdev_internal_get_buffer_size(subdev, &l0_width, &l0_height, &sbwc_en);
	l2_width = (((l0_width + 2) >> 2) + 1);
	l2_height = (((l0_height + 2) >> 2) + 1);

	switch (subdev->id) {
	case ENTRY_YPP:
		switch (*vid) {
		case IS_VIDEO_YND_NUM:
			*num_planes = 2;
			*buffer_size =
				ALIGN(DIV_ROUND_UP(l2_width * subdev->bits_per_pixel, BITS_PER_BYTE), 16) * l2_height;
			break;
		case IS_VIDEO_YDG_NUM:
			*num_planes = 1;
			*buffer_size =
				ALIGN(DIV_ROUND_UP(l2_width * subdev->bits_per_pixel, BITS_PER_BYTE), 16) * l2_height;
			break;
		case IS_VIDEO_YSH_NUM:
			*num_planes = 1;
			*buffer_size = 2048 * 144;
			break;
		case IS_VIDEO_YNS_NUM:
			*num_planes = 1;
			payload_size = is_hw_dma_get_payload_stride(COMP, subdev->bits_per_pixel, l0_width,
				0, 0, 256, 1) * l0_height;
			header_size = is_hw_dma_get_header_stride(l0_width, 256) * l0_height;
			*buffer_size = payload_size + header_size;
			break;
		default:
			mserr("invalid vid(%d)",
				subdev, subdev, *vid);
			return -EINVAL;
		}
		break;
	default:
		mserr("invalid subdev id",
			subdev, subdev);
		return -EINVAL;
	}

	return ret;
}

static int __is_subdev_internal_get_out_node_info(const struct is_subdev *subdev, u32 *num_planes)
{
	int ret = 0;

	switch (subdev->id) {
	case ENTRY_YPP:
		*num_planes = 2;
		break;
	default:
		*num_planes = 1;
		break;
	}

	return ret;
}
#endif
static int is_subdev_internal_alloc_buffer(struct is_subdev *subdev,
	struct is_mem *mem)
{
	int ret;
	int i, j;
	unsigned int flags = 0;
	u32 width, height;
	u32 buffer_size; /* single buffer */
	u32 total_size; /* multi-buffer for FRO */
	u32 total_alloc_size = 0;
	u32 num_planes;
	struct is_core *core;
	struct is_frame *frame;
	struct is_queue *queue;
	struct is_framemgr *shared_framemgr;
	int use_shared_framemgr;
#ifdef ENABLE_LOGICAL_VIDEO_NODE
	int k;
	struct is_sub_node * snode = NULL;
	int cap_node_num;
#endif
	u32 batch_num;
	u32 payload_size, header_size;
	u32 sbwc_en;

	FIMC_BUG(!subdev);

	if (subdev->buffer_num > SUBDEV_INTERNAL_BUF_MAX || subdev->buffer_num <= 0) {
		mserr("invalid internal buffer num size(%d)",
			subdev, subdev, subdev->buffer_num);
		return -EINVAL;
	}

	core = (struct is_core *)dev_get_drvdata(is_dev);
	queue = GET_SUBDEV_QUEUE(subdev);

	subdev->use_shared_framemgr = is_subdev_internal_use_shared_framemgr(subdev);

	is_subdev_internal_get_buffer_size(subdev, &width, &height, &sbwc_en);

	if (sbwc_en) {
		payload_size = is_hw_dma_get_payload_stride(COMP, subdev->bits_per_pixel, width,
			0, 0, 256, 1) * height;
		header_size = is_hw_dma_get_header_stride(width, 256) * height;
		buffer_size = payload_size + header_size;
	} else {
		buffer_size =
			ALIGN(DIV_ROUND_UP(width * subdev->bits_per_pixel, BITS_PER_BYTE), 16) * height;
	}

	if (buffer_size <= 0) {
		mserr("wrong internal subdev buffer size(%d)",
					subdev, subdev, buffer_size);
		return -EINVAL;
	}

	batch_num = subdev->batch_num;

	ret = __is_subdev_internal_get_out_node_info(subdev, &num_planes);
	if (ret)
		return ret;

	cap_node_num = __is_subdev_internal_get_cap_node_num(subdev);
	if (cap_node_num < 0)
		return -EINVAL;

	total_size = buffer_size * batch_num * num_planes;

	if (test_bit(IS_SUBDEV_IOVA_EXTENSION_USE, &subdev->state)) {
		if (IS_ENABLED(USE_IOVA_EXTENSION))
			flags |= ION_EXYNOS_FLAG_IOVA_EXTENSION;
		else
			total_size *= 2;
		msinfo(" %s IOVA_EXTENSION_USE\n", subdev, subdev, __func__);
	}

	if (subdev->id == ENTRY_YPP && core->scenario == IS_SCENARIO_SECURE) {
		mem->default_ctx->heapmask_s = is_ion_query_heapmask("secure_camera_heap");
		if (!mem->default_ctx->heapmask_s) {
			mserr("can't find secure_camera_heap in ion", subdev, subdev);
			return -EINVAL;
		}
		flags |= ION_EXYNOS_FLAG_PROTECTED;
		msinfo(" %s heapmask(0x%x),heapmask_s(0x%x) scenario(%d) flag(0x%x)\n", subdev, subdev, __func__,
			mem->default_ctx->heapmask, mem->default_ctx->heapmask_s, core->scenario, flags);
	}

	ret = frame_manager_open(&subdev->internal_framemgr, subdev->buffer_num);
	if (ret) {
		mserr("is_frame_open is fail(%d)", subdev, subdev, ret);
		ret = -EINVAL;
		goto err_open_framemgr;
	}

	is_subdev_internal_lock_shared_framemgr(subdev);
	use_shared_framemgr = is_subdev_internal_get_shared_framemgr(subdev, &shared_framemgr, width, height);
	if (use_shared_framemgr < 0) {
		mserr("is_subdev_internal_get_shared_framemgr is fail(%d)", subdev, subdev, use_shared_framemgr);
		ret = -EINVAL;
		goto err_open_shared_framemgr;
	}

	for (i = 0; i < subdev->buffer_num; i++) {
		if (use_shared_framemgr > 0) {
			subdev->pb_subdev[i] = shared_framemgr->frames[i].pb_output;
		} else {
#if defined(CONFIG_CAMERA_VENDER_MCD)
			if (core->scenario != IS_SCENARIO_SECURE && subdev->instance == 0) {
				subdev->pb_subdev[i] = CALL_PTR_MEMOP(mem, alloc, mem->default_ctx, total_size, "camera_heap", flags);
				if (IS_ERR_OR_NULL(subdev->pb_subdev[i])) {
					msinfo("not enough reserved memory, use ion system heap instead, total_size = %d", subdev, subdev, total_size);

					subdev->pb_subdev[i] = CALL_PTR_MEMOP(mem, alloc, mem->default_ctx, total_size, NULL, flags);
					if (IS_ERR_OR_NULL(subdev->pb_subdev[i])) {
						mserr("failed to allocate buffer for internal subdev",
										subdev, subdev);
						subdev->pb_subdev[i] = NULL;
						ret = -ENOMEM;
						goto err_allocate_pb_subdev;
					}
				} else {
					msinfo("success to alloc reserved memory, size = %d", subdev, subdev, total_size);
				}
			} else
#endif
			{
				subdev->pb_subdev[i] = CALL_PTR_MEMOP(mem, alloc, mem->default_ctx, total_size, NULL, flags);
				if (IS_ERR_OR_NULL(subdev->pb_subdev[i])) {
					mserr("failed to allocate buffer for internal subdev",
									subdev, subdev);
					subdev->pb_subdev[i] = NULL;
					ret = -ENOMEM;
					goto err_allocate_pb_subdev;
				}
			}

			if (shared_framemgr)
				shared_framemgr->frames[i].pb_output = subdev->pb_subdev[i];
		}

		total_alloc_size += total_size;
	}

	for (i = 0; i < subdev->buffer_num; i++) {
		frame = &subdev->internal_framemgr.frames[i];
		frame->subdev = subdev;

		/* TODO : support multi-plane */
		frame->planes = 1;
		frame->num_buffers = batch_num;
#ifdef ENABLE_LOGICAL_VIDEO_NODE
		snode = &frame->out_node;

		snode->sframe[0].dva[0] = frame->dvaddr_buffer[0] =
			CALL_BUFOP(subdev->pb_subdev[i], dvaddr, subdev->pb_subdev[i]);

		if (core->scenario == IS_SCENARIO_SECURE)
			frame->kvaddr_buffer[0] = 0;
		else
			frame->kvaddr_buffer[0] = CALL_BUFOP(subdev->pb_subdev[i], kvaddr, subdev->pb_subdev[i]);

		frame->size[0] = buffer_size;
		set_bit(FRAME_MEM_INIT, &frame->mem_state);

		for (j = 1; j < batch_num * num_planes; j++) {
			snode->sframe[0].dva[j] = frame->dvaddr_buffer[j] = frame->dvaddr_buffer[j - 1] + buffer_size;
			frame->kvaddr_buffer[j] = frame->kvaddr_buffer[j - 1] + buffer_size;
		}

		snode->sframe[0].id = subdev->vid;
		snode->sframe[0].num_planes = num_planes;
		if (queue)
			snode->sframe[0].hw_pixeltype = queue->framecfg.hw_pixeltype;

		snode = &frame->cap_node;

		for (j = 0; j < cap_node_num; j++) {
			ret = __is_subdev_internal_get_cap_node_info(subdev,
				&snode->sframe[j].id, &snode->sframe[j].num_planes, &buffer_size, j);
			if (ret) {
				subdev->pb_capture_subdev[i][j] = NULL;
				goto err_allocate_pb_capture_subdev;
			}

			snode->sframe[j].hw_pixeltype =
				queue->framecfg.hw_pixeltype;
			total_size = buffer_size * snode->sframe[j].num_planes;

			if (j == 0 && core->scenario == IS_SCENARIO_SECURE) {
				/* NR_DS */
				flags |= ION_EXYNOS_FLAG_PROTECTED;
			} else
				flags = 0;

			if (use_shared_framemgr > 0) {
				subdev->pb_capture_subdev[i][j] = shared_framemgr->frames[i].pb_capture[j];
			} else {
#if defined(CONFIG_CAMERA_VENDER_MCD)
				if (core->scenario != IS_SCENARIO_SECURE && subdev->instance == 0) {
					subdev->pb_capture_subdev[i][j] = CALL_PTR_MEMOP(mem, alloc,
						mem->default_ctx, total_size, "camera_heap", flags);
					if (IS_ERR_OR_NULL(subdev->pb_capture_subdev[i][j])) {
						msinfo("not enough reserved memory, use ion system heap instead, total_size = %d", subdev, subdev, total_size);

						subdev->pb_capture_subdev[i][j] = CALL_PTR_MEMOP(mem, alloc,
							mem->default_ctx, total_size, NULL, flags);
						if (IS_ERR_OR_NULL(subdev->pb_capture_subdev[i][j])) {
							mserr("failed to allocate buffer for internal subdev",
											subdev, subdev);
							subdev->pb_capture_subdev[i][j] = NULL;
							ret = -ENOMEM;
							goto err_allocate_pb_capture_subdev;
						}
					} else {
						msinfo("success to alloc reserved memory, size = %d", subdev, subdev, total_size);
					}
				} else
#endif
				{
					subdev->pb_capture_subdev[i][j] = CALL_PTR_MEMOP(mem, alloc,
						mem->default_ctx, total_size, NULL, flags);
					if (IS_ERR_OR_NULL(subdev->pb_capture_subdev[i][j])) {
						mserr("failed to allocate buffer for internal subdev",
										subdev, subdev);
						subdev->pb_capture_subdev[i][j] = NULL;
						ret = -ENOMEM;
						goto err_allocate_pb_capture_subdev;
					}
				}

				if (shared_framemgr)
					shared_framemgr->frames[i].pb_capture[j] = subdev->pb_capture_subdev[i][j];
			}

			total_alloc_size += total_size;

			snode->sframe[j].backup_dva[0] = snode->sframe[j].dva[0] =
				CALL_BUFOP(subdev->pb_capture_subdev[i][j], dvaddr, subdev->pb_capture_subdev[i][j]);

			if (j == 0 && core->scenario == IS_SCENARIO_SECURE)
				snode->sframe[j].kva[0] = 0;
			else
				snode->sframe[j].kva[0] = CALL_BUFOP(subdev->pb_capture_subdev[i][j],
					kvaddr, subdev->pb_capture_subdev[i][j]);

			for (k = 1; k < snode->sframe[j].num_planes; k++) {
				snode->sframe[j].backup_dva[k] = snode->sframe[j].dva[k] =
					snode->sframe[0].dva[k - 1] + buffer_size;
				snode->sframe[j].kva[k] = snode->sframe[0].kva[k - 1] + buffer_size;
			}
		}
#else
		frame->dvaddr_buffer[0] = CALL_BUFOP(subdev->pb_subdev[i], dvaddr, subdev->pb_subdev[i]);
		frame->kvaddr_buffer[0] = CALL_BUFOP(subdev->pb_subdev[i], kvaddr, subdev->pb_subdev[i]);
		frame->size[0] = buffer_size;

		set_bit(FRAME_MEM_INIT, &frame->mem_state);

		for (j = 1; j < batch_num; j++) {
			frame->dvaddr_buffer[j] = frame->dvaddr_buffer[j - 1] + buffer_size;
			frame->kvaddr_buffer[j] = frame->kvaddr_buffer[j - 1] + buffer_size;
		}
#endif

#if defined(SDC_HEADER_GEN)
		if (subdev->id == ENTRY_PAF) {
			const u32 *header = NULL;
			u32 byte_per_line;
			u32 header_size;
			u32 width = subdev->output.width;

			if (width == SDC_WIDTH_HD)
				header = is_sdc_header_hd;
			else if (width == SDC_WIDTH_FHD)
				header = is_sdc_header_fhd;
			else
				mserr("invalid SDC size: width(%d)", subdev, subdev, width);

			if (header) {
				byte_per_line = ALIGN(width / 2 * 10 / BITS_PER_BYTE, 16);
				header_size = byte_per_line * SDC_HEADER_LINE;

				memcpy((void *)frame->kvaddr_buffer[0], header, header_size);
#if !defined(MODULE)
				__flush_dcache_area((void *)frame->kvaddr_buffer[0], header_size);
#endif
				msinfo("Write SDC header: width(%d) size(%d)\n",
					subdev, subdev, width, header_size);
			}
		}
#endif
	}
	is_subdev_internal_unlock_shared_framemgr(subdev);

	msinfo(" %s (size: %dx%d, bpp: %d, total: %d, buffer_num: %d, batch_num: %d, shared_framemgr(ref: %d))",
		subdev, subdev, __func__,
		width, height, subdev->bits_per_pixel,
		total_alloc_size, subdev->buffer_num, batch_num, use_shared_framemgr);

	return 0;

#ifdef ENABLE_LOGICAL_VIDEO_NODE
err_allocate_pb_capture_subdev:
	for (i = 0; i < subdev->buffer_num; i++)
		for (j = 0; j < cap_node_num; j++)
			if (subdev->pb_capture_subdev[i][j])
				CALL_VOID_BUFOP(subdev->pb_capture_subdev[i][j], free, subdev->pb_capture_subdev[i][j]);
			else
				goto err_allocate_pb_capture_subdev_end;

err_allocate_pb_capture_subdev_end:
#endif

err_allocate_pb_subdev:
	for (i = 0; i < subdev->buffer_num; i++)
		if (subdev->pb_subdev[i])
			CALL_VOID_BUFOP(subdev->pb_subdev[i], free, subdev->pb_subdev[i]);
		else
			goto err_allocate_pb_subdev_end;

err_allocate_pb_subdev_end:
	is_subdev_internal_put_shared_framemgr(subdev);
err_open_shared_framemgr:
	is_subdev_internal_unlock_shared_framemgr(subdev);
	frame_manager_close(&subdev->internal_framemgr);
err_open_framemgr:
	return ret;
};

static int is_subdev_internal_free_buffer(struct is_subdev *subdev)
{
	int ret = 0;
	int i;
#ifdef ENABLE_LOGICAL_VIDEO_NODE
	int j;
	u32 cap_node_num;
#endif

	FIMC_BUG(!subdev);

	if (subdev->internal_framemgr.num_frames == 0) {
		mswarn(" already free internal buffer", subdev, subdev);
		return -EINVAL;
	}

	frame_manager_close(&subdev->internal_framemgr);

	is_subdev_internal_lock_shared_framemgr(subdev);
	ret = is_subdev_internal_put_shared_framemgr(subdev);

	if (!ret) {
		for (i = 0; i < subdev->buffer_num; i++)
			CALL_VOID_BUFOP(subdev->pb_subdev[i], free, subdev->pb_subdev[i]);

#ifdef ENABLE_LOGICAL_VIDEO_NODE
		cap_node_num = __is_subdev_internal_get_cap_node_num(subdev);
		for (i = 0; i < subdev->buffer_num; i++) {
			for (j = 0; j < cap_node_num; j++) {
				CALL_VOID_BUFOP(subdev->pb_capture_subdev[i][j], free, subdev->pb_capture_subdev[i][j]);
			}
		}
#endif
	}
	is_subdev_internal_unlock_shared_framemgr(subdev);

	msinfo(" %s. shared_framemgr refcount: %d\n", subdev, subdev, __func__, ret);

	return 0;
};

static int _is_subdev_internal_start(struct is_subdev *subdev)
{
	int ret = 0;
	int j;
	struct is_framemgr *framemgr;
	struct is_frame *frame;
	unsigned long flags;

	if (test_bit(IS_SUBDEV_START, &subdev->state)) {
		msinfo("[%s] subdev already start", subdev, subdev, __func__);
		goto p_err;
	}

	/* qbuf a setting num of buffers before stream on */
	framemgr = GET_SUBDEV_I_FRAMEMGR(subdev);
	if (unlikely(!framemgr)) {
		mserr(" subdev's framemgr is null", subdev, subdev);
		ret = -EINVAL;
		goto p_err;
	}

	for (j = 0; j < framemgr->num_frames; j++) {
		frame = &framemgr->frames[j];

		/* 1. check frame validation */
		if (unlikely(!test_bit(FRAME_MEM_INIT, &frame->mem_state))) {
			mserr("frame %d is NOT init", subdev, subdev, j);
			ret = -EINVAL;
			goto p_err;
		}

		/* 2. update frame manager */
		framemgr_e_barrier_irqs(framemgr, FMGR_IDX_17, flags);

		if (frame->state != FS_FREE) {
			mserr("frame %d is invalid state(%d)\n",
				subdev, subdev, j, frame->state);
			frame_manager_print_queues(framemgr);
			ret = -EINVAL;
			goto p_err;
		}

		framemgr_x_barrier_irqr(framemgr, FMGR_IDX_17, flags);
	}

	set_bit(IS_SUBDEV_START, &subdev->state);

p_err:

	return ret;
}

static int _is_subdev_internal_stop(struct is_subdev *subdev)
{
	int ret = 0;
	struct is_framemgr *framemgr;
	struct is_frame *frame;
	unsigned long flags;

	if (!test_bit(IS_SUBDEV_START, &subdev->state)) {
		mserr("already stopped", subdev, subdev);
		ret = -EINVAL;
		goto p_err;
	}

	framemgr = GET_SUBDEV_I_FRAMEMGR(subdev);
	if (unlikely(!framemgr)) {
		mserr(" subdev's framemgr is null", subdev, subdev);
		ret = -EINVAL;
		goto p_err;
	}

	framemgr_e_barrier_irqs(framemgr, FMGR_IDX_16, flags);

	frame = peek_frame(framemgr, FS_PROCESS);
	while (frame) {
		trans_frame(framemgr, frame, FS_COMPLETE);
		frame = peek_frame(framemgr, FS_PROCESS);
	}

	frame = peek_frame(framemgr, FS_REQUEST);
	while (frame) {
		trans_frame(framemgr, frame, FS_COMPLETE);
		frame = peek_frame(framemgr, FS_REQUEST);
	}

	frame = peek_frame(framemgr, FS_COMPLETE);
	while (frame) {
		trans_frame(framemgr, frame, FS_FREE);
		frame = peek_frame(framemgr, FS_COMPLETE);
	}

	framemgr_x_barrier_irqr(framemgr, FMGR_IDX_16, flags);

	clear_bit(IS_SUBDEV_START, &subdev->state);
	clear_bit(IS_SUBDEV_VOTF_USE, &subdev->state);

p_err:

	return ret;
}

int is_subdev_internal_start(void *device, enum is_device_type type, struct is_subdev *subdev)
{
	int ret = 0;
	struct is_device_sensor *sensor;
	struct is_device_ischain *ischain;
	struct is_mem *mem = NULL;

	FIMC_BUG(!device);

	if (!test_bit(IS_SUBDEV_INTERNAL_USE, &subdev->state)) {
		mserr("subdev is not in INTERNAL_USE state.", subdev, subdev);
		return -EINVAL;
	}

	if (!test_bit(IS_SUBDEV_INTERNAL_S_FMT, &subdev->state)) {
		mserr("subdev is not in s_fmt state.", subdev, subdev);
		return -EINVAL;
	}

	switch (type) {
	case IS_DEVICE_SENSOR:
		sensor = (struct is_device_sensor *)device;
		mem = &sensor->resourcemgr->mem;
		break;
	case IS_DEVICE_ISCHAIN:
		ischain = (struct is_device_ischain *)device;
		mem = &ischain->resourcemgr->mem;
		break;
	default:
		err("invalid device_type(%d)", type);
		return -EINVAL;
	}

	if (subdev->internal_framemgr.num_frames > 0) {
		mswarn("%s already internal buffer alloced, re-alloc after free",
			subdev, subdev, __func__);

		ret = is_subdev_internal_free_buffer(subdev);
		if (ret) {
			mserr("subdev internal free buffer is fail", subdev, subdev);
			ret = -EINVAL;
			goto p_err;
		}
	}

	ret = is_subdev_internal_alloc_buffer(subdev, mem);
	if (ret) {
		mserr("ischain internal alloc buffer fail(%d)", subdev, subdev, ret);
		goto p_err;
	}

	ret = _is_subdev_internal_start(subdev);
	if (ret) {
		mserr("_is_subdev_internal_start fail(%d)", subdev, subdev, ret);
		goto p_err;
	}

p_err:
	msinfo(" %s(%s)(%d)\n", subdev, subdev, __func__, subdev->data_type, ret);
	return ret;
};

int is_subdev_internal_stop(void *device, enum is_device_type type, struct is_subdev *subdev)
{
	int ret = 0;

	if (!test_bit(IS_SUBDEV_INTERNAL_USE, &subdev->state)) {
		mserr("subdev is not in INTERNAL_USE state.", subdev, subdev);
		return -EINVAL;
	}

	ret = _is_subdev_internal_stop(subdev);
	if (ret) {
		mserr("_is_subdev_internal_stop fail(%d)", subdev, subdev, ret);
		goto p_err;
	}

	ret = is_subdev_internal_free_buffer(subdev);
	if (ret) {
		mserr("subdev internal free buffer is fail(%d)", subdev, subdev, ret);
		ret = -EINVAL;
		goto p_err;
	}

p_err:
	msinfo(" %s(%s)(%d)\n", subdev, subdev, __func__, subdev->data_type, ret);
	return ret;
};

int is_subdev_internal_s_format(void *device, enum is_device_type type, struct is_subdev *subdev,
	u32 width, u32 height, u32 bits_per_pixel, u32 buffer_num, const char *type_name)
{
	int ret = 0;
	struct is_device_ischain *ischain;
	struct is_device_sensor *sensor;
	struct is_sensor_cfg *sensor_cfg;
	u32 batch_num = 1;

	if (!test_bit(IS_SUBDEV_INTERNAL_USE, &subdev->state)) {
		mserr("subdev is not in INTERNAL_USE state.", subdev, subdev);
		return -EINVAL;
	}

	if (!test_bit(IS_SUBDEV_OPEN, &subdev->state)) {
		mserr("subdev is not in OPEN state.", subdev, subdev);
		return -EINVAL;
	}

	if (width == 0 || height == 0) {
		mserr("wrong internal vc size(%d x %d)", subdev, subdev, width, height);
		return -EINVAL;
	}

	switch (type) {
	case IS_DEVICE_SENSOR:
		break;
	case IS_DEVICE_ISCHAIN:
		FIMC_BUG(!device);
		ischain = (struct is_device_ischain *)device;

		if (subdev->id == ENTRY_PAF) {
			if (test_bit(IS_ISCHAIN_REPROCESSING, &ischain->state))
				break;

			sensor = ischain->sensor;
			if (!sensor) {
				mserr("failed to get sensor", subdev, subdev);
				return -EINVAL;
			}

			sensor_cfg = sensor->cfg;
			if (!sensor_cfg) {
				mserr("failed to get senso_cfgr", subdev, subdev);
				return -EINVAL;
			}

			if (sensor_cfg->ex_mode == EX_DUALFPS_960)
				batch_num = 16;
			else if (sensor_cfg->ex_mode == EX_DUALFPS_480)
				batch_num = 8;
		} else if (subdev->id == ENTRY_YPP)
			batch_num = 1;

		break;
	default:
		err("invalid device_type(%d)", type);
		ret = -EINVAL;
		break;
	}

	subdev->output.width = width;
	subdev->output.height = height;
	subdev->output.crop.x = 0;
	subdev->output.crop.y = 0;
	subdev->output.crop.w = subdev->output.width;
	subdev->output.crop.h = subdev->output.height;
	subdev->bits_per_pixel = bits_per_pixel;
	subdev->buffer_num = buffer_num;
	subdev->batch_num = batch_num;

	snprintf(subdev->data_type, sizeof(subdev->data_type), "%s", type_name);

	set_bit(IS_SUBDEV_INTERNAL_S_FMT, &subdev->state);

	return ret;
};

int is_subdev_internal_g_bpp(void *device, enum is_device_type type, struct is_subdev *subdev,
	struct is_sensor_cfg *sensor_cfg)
{
	int bpp;

	switch (subdev->id) {
	case ENTRY_YPP:
		return 10;
	case ENTRY_PAF:
		if (!sensor_cfg)
			return BITS_PER_BYTE *2;

		switch (sensor_cfg->output[CSI_VIRTUAL_CH_0].hwformat) {
		case HW_FORMAT_RAW10:
			bpp = 10;
			break;
		case HW_FORMAT_RAW12:
			bpp = 12;
			break;
		default:
			bpp = BITS_PER_BYTE * 2;
			break;
		}

		if (sensor_cfg->votf && sensor_cfg->img_pd_ratio == IS_IMG_PD_RATIO_1_1)
			set_bit(IS_SUBDEV_IOVA_EXTENSION_USE, &subdev->state);
		else
			clear_bit(IS_SUBDEV_IOVA_EXTENSION_USE, &subdev->state);

		return bpp;
	default:
		return BITS_PER_BYTE * 2; /* 2byte = 16bits */
	}
}

int is_subdev_internal_open(void *device, enum is_device_type type, struct is_subdev *subdev)
{
	int ret = 0;
	struct is_device_sensor *sensor;
	struct is_device_ischain *ischain;

	if (test_bit(IS_SUBDEV_INTERNAL_USE, &subdev->state)) {
		msinfo("[%s] already INTERNAL_USE state", subdev, subdev, __func__);
		goto p_err;
	}

	switch (type) {
	case IS_DEVICE_SENSOR:
		sensor = (struct is_device_sensor *)device;

		if (!test_bit(IS_SUBDEV_OPEN, &subdev->state)) {
			ret = is_subdev_open(subdev, NULL, NULL, sensor->instance);
			if (ret) {
				mserr("is_subdev_open is fail(%d)", subdev, subdev, ret);
				goto p_err;
			}
		}

		msinfo("[SS%d] %s\n", sensor, subdev, sensor->device_id, __func__);
		break;
	case IS_DEVICE_ISCHAIN:
		ischain = (struct is_device_ischain *)device;

		ret = is_subdev_open(subdev, NULL, NULL, ischain->instance);
		if (ret) {
			mserr("is_subdev_open is fail(%d)", subdev, subdev, ret);
			goto p_err;
		}
		msinfo("%s\n", subdev, subdev, __func__);
		break;
	default:
		err("invalid device_type(%d)", type);
		ret = -EINVAL;
		break;
	}

	set_bit(IS_SUBDEV_INTERNAL_USE, &subdev->state);

p_err:
	return ret;
};

int is_subdev_internal_close(void *device, enum is_device_type type, struct is_subdev *subdev)
{
	int ret = 0;

	if (!test_bit(IS_SUBDEV_INTERNAL_USE, &subdev->state)) {
		mserr("subdev is not in INTERNAL_USE state.", subdev, subdev);
		return -EINVAL;
	}

	ret = is_subdev_close(subdev);
	if (ret)
		mserr("is_subdev_close is fail(%d)", subdev, subdev, ret);

	clear_bit(IS_SUBDEV_INTERNAL_USE, &subdev->state);

	return ret;
};
