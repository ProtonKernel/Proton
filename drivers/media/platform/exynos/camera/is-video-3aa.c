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
#include <linux/delay.h>
#include <linux/kernel.h>
#include <linux/errno.h>
#include <linux/interrupt.h>
#include <linux/device.h>
#include <linux/platform_device.h>
#include <linux/slab.h>
#include <linux/firmware.h>
#include <linux/dma-mapping.h>
#include <linux/scatterlist.h>
#include <linux/videodev2_exynos_camera.h>
#include <linux/v4l2-mediabus.h>
#include <linux/bug.h>

#include <media/videobuf2-v4l2.h>
#include <media/v4l2-ctrls.h>
#include <media/v4l2-device.h>
#include <media/v4l2-mem2mem.h>
#include <media/v4l2-mediabus.h>

#include "is-core.h"
#include "is-cmd.h"
#include "is-err.h"
#include "is-video.h"
#include "is-param.h"

const struct v4l2_file_operations is_3aa_video_fops;
const struct v4l2_ioctl_ops is_3aa_video_ioctl_ops;
const struct vb2_ops is_3aa_qops;

int is_30s_video_probe(void *data)
{
	int ret = 0;
	struct is_core *core;
	struct is_video *video;

	FIMC_BUG(!data);

	core = (struct is_core *)data;
	video = &core->video_30s;
	video->resourcemgr = &core->resourcemgr;

	if (!core->pdev) {
		probe_err("pdev is NULL");
		ret = -EINVAL;
		goto p_err;
	}

	ret = is_video_probe(video,
		IS_VIDEO_3XS_NAME(0),
		IS_VIDEO_30S_NUM,
		VFL_DIR_M2M,
		&core->resourcemgr.mem,
		&core->v4l2_dev,
		&is_3aa_video_fops,
		&is_3aa_video_ioctl_ops);
	if (ret)
		dev_err(&core->pdev->dev, "%s is fail(%d)\n", __func__, ret);

p_err:
	return ret;
}

int is_31s_video_probe(void *data)
{
	int ret = 0;
	struct is_core *core;
	struct is_video *video;

	FIMC_BUG(!data);

	core = (struct is_core *)data;
	video = &core->video_31s;
	video->resourcemgr = &core->resourcemgr;

	if (!core->pdev) {
		probe_err("pdev is NULL");
		ret = -EINVAL;
		goto p_err;
	}

	ret = is_video_probe(video,
		IS_VIDEO_3XS_NAME(1),
		IS_VIDEO_31S_NUM,
		VFL_DIR_M2M,
		&core->resourcemgr.mem,
		&core->v4l2_dev,
		&is_3aa_video_fops,
		&is_3aa_video_ioctl_ops);
	if (ret)
		dev_err(&core->pdev->dev, "%s is fail(%d)\n", __func__, ret);

p_err:
	return ret;
}

int is_32s_video_probe(void *data)
{
	int ret = 0;
	struct is_core *core;
	struct is_video *video;

	FIMC_BUG(!data);

	core = (struct is_core *)data;
	video = &core->video_32s;
	video->resourcemgr = &core->resourcemgr;

	if (!core->pdev) {
		probe_err("pdev is NULL");
		ret = -EINVAL;
		goto p_err;
	}

	ret = is_video_probe(video,
		IS_VIDEO_3XS_NAME(2),
		IS_VIDEO_32S_NUM,
		VFL_DIR_M2M,
		&core->resourcemgr.mem,
		&core->v4l2_dev,
		&is_3aa_video_fops,
		&is_3aa_video_ioctl_ops);
	if (ret)
		dev_err(&core->pdev->dev, "%s is fail(%d)\n", __func__, ret);

p_err:
	return ret;
}

int is_33s_video_probe(void *data)
{
	int ret = 0;
	struct is_core *core;
	struct is_video *video;

	FIMC_BUG(!data);

	core = (struct is_core *)data;
	video = &core->video_33s;
	video->resourcemgr = &core->resourcemgr;

	if (!core->pdev) {
		probe_err("pdev is NULL");
		ret = -EINVAL;
		goto p_err;
	}

	ret = is_video_probe(video,
		IS_VIDEO_3XS_NAME(3),
		IS_VIDEO_33S_NUM,
		VFL_DIR_M2M,
		&core->resourcemgr.mem,
		&core->v4l2_dev,
		&is_3aa_video_fops,
		&is_3aa_video_ioctl_ops);
	if (ret)
		dev_err(&core->pdev->dev, "%s is fail(%d)\n", __func__, ret);

p_err:
	return ret;
}

/*
 * =============================================================================
 * Video File Opertation
 * =============================================================================
 */

static int is_3aa_video_open(struct file *file)
{
	int ret = 0;
	int ret_err = 0;
	struct is_video *video;
	struct is_video_ctx *vctx;
	struct is_device_ischain *device;
	struct is_resourcemgr *resourcemgr;
	char name[IS_STR_LEN];
	vctx = NULL;
	device = NULL;

	FIMC_BUG(!file);

	video = video_drvdata(file);
	resourcemgr = video->resourcemgr;
	if (!resourcemgr) {
		err("resourcemgr is NULL");
		ret = -EINVAL;
		goto err_resource_null;
	}

	ret = is_resource_open(resourcemgr, RESOURCE_TYPE_ISCHAIN, (void **)&device);
	if (ret) {
		err("is_resource_open is fail(%d)", ret);
		goto err_resource_open;
	}

	if (!device) {
		err("device is NULL");
		ret = -EINVAL;
		goto err_device_null;
	}

	minfo("[3%dS:V] %s\n", device, GET_3XS_ID(video), __func__);

	snprintf(name, sizeof(name), "3%dS", GET_3XS_ID(video));
	ret = open_vctx(file, video, &vctx, device->instance, ENTRY_3AA, name);
	if (ret) {
		merr("open_vctx is fail(%d)", device, ret);
		goto err_vctx_open;
	}

	ret = is_video_open(vctx,
		device,
		VIDEO_3XS_READY_BUFFERS,
		video,
		&is_3aa_qops,
		&is_ischain_3aa_ops);
	if (ret) {
		merr("is_video_open is fail(%d)", device, ret);
		goto err_video_open;
	}

	ret = is_ischain_3aa_open(device, vctx);
	if (ret) {
		merr("is_ischain_3aa_open is fail(%d)", device, ret);
		goto err_ischain_open;
	}

	return 0;

err_ischain_open:
	ret_err = is_video_close(vctx);
	if (ret_err)
		merr("is_video_close is fail(%d)", device, ret_err);
err_video_open:
	ret_err = close_vctx(file, video, vctx);
	if (ret_err < 0)
		merr("close_vctx is fail(%d)", device, ret_err);
err_vctx_open:
err_device_null:
err_resource_open:
err_resource_null:
	return ret;
}

static int is_3aa_video_close(struct file *file)
{
	int ret = 0;
	int refcount;
	struct is_video_ctx *vctx;
	struct is_video *video;
	struct is_device_ischain *device;

	FIMC_BUG(!file);
	FIMC_BUG(!file->private_data);
	vctx = file->private_data;

	FIMC_BUG(!GET_VIDEO(vctx));
	video = GET_VIDEO(vctx);

	FIMC_BUG(!GET_DEVICE(vctx));
	device = GET_DEVICE(vctx);

	ret = is_ischain_3aa_close(device, vctx);
	if (ret)
		merr("is_ischain_3aa_close is fail(%d)", device, ret);

	ret = is_video_close(vctx);
	if (ret)
		merr("is_video_close is fail(%d)", device, ret);

	refcount = close_vctx(file, video, vctx);
	if (refcount < 0)
		merr("close_vctx is fail(%d)", device, refcount);

	minfo("[3%dS:V] %s(%d,%d):%d\n", device, GET_3XS_ID(video), __func__, atomic_read(&device->open_cnt), refcount, ret);

	return ret;
}

static unsigned int is_3aa_video_poll(struct file *file,
	struct poll_table_struct *wait)
{
	u32 ret = 0;
	struct is_video_ctx *vctx;
	struct is_device_ischain *device;

	FIMC_BUG(!wait);
	FIMC_BUG(!file);
	FIMC_BUG(!file->private_data);
	vctx = file->private_data;

	FIMC_BUG(!GET_DEVICE(vctx));
	device = GET_DEVICE(vctx);

	ret = is_video_poll(file, vctx, wait);
	if (ret)
		merr("is_video_poll is fail(%d)", device, ret);

	return ret;
}

static int is_3aa_video_mmap(struct file *file,
	struct vm_area_struct *vma)
{
	int ret = 0;
	struct is_video_ctx *vctx;
	struct is_device_ischain *device;

	FIMC_BUG(!vma);
	FIMC_BUG(!file);
	FIMC_BUG(!file->private_data);
	vctx = file->private_data;

	FIMC_BUG(!GET_DEVICE(vctx));
	device = GET_DEVICE(vctx);

	ret = is_video_mmap(file, vctx, vma);
	if (ret)
		merr("is_video_mmap is fail(%d)", device, ret);

	return ret;
}

const struct v4l2_file_operations is_3aa_video_fops = {
	.owner		= THIS_MODULE,
	.open		= is_3aa_video_open,
	.release	= is_3aa_video_close,
	.poll		= is_3aa_video_poll,
	.unlocked_ioctl	= video_ioctl2,
	.mmap		= is_3aa_video_mmap,
};

/*
 * =============================================================================
 * Video Ioctl Opertation
 * =============================================================================
 */

static int is_3aa_video_querycap(struct file *file, void *fh,
	struct v4l2_capability *cap)
{
	struct is_video *video = video_drvdata(file);

	FIMC_BUG(!cap);
	FIMC_BUG(!video);

	snprintf(cap->driver, sizeof(cap->driver), "%s", video->vd.name);
	snprintf(cap->card, sizeof(cap->card), "%s", video->vd.name);
	cap->capabilities |= V4L2_CAP_STREAMING
			| V4L2_CAP_VIDEO_OUTPUT
			| V4L2_CAP_VIDEO_OUTPUT_MPLANE;
	cap->device_caps |= cap->capabilities;

	return 0;
}

static int is_3aa_video_get_format_mplane(struct file *file, void *fh,
	struct v4l2_format *format)
{
	/* Todo : add to get format code */
	return 0;
}

static int is_3aa_video_set_format_mplane(struct file *file, void *fh,
	struct v4l2_format *format)
{
	int ret = 0;
	struct is_video_ctx *vctx;
	struct is_device_ischain *device;

	FIMC_BUG(!format);
	FIMC_BUG(!file);
	FIMC_BUG(!file->private_data);
	vctx = file->private_data;

	FIMC_BUG(!GET_DEVICE(vctx));
	device = GET_DEVICE(vctx);

	mdbgv_3aa("%s\n", vctx, __func__);

	ret = is_video_set_format_mplane(file, vctx, format);
	if (ret) {
		merr("is_video_set_format_mplane is fail(%d)", device, ret);
		goto p_err;
	}

p_err:
	return ret;
}

static int is_3aa_video_reqbufs(struct file *file, void *priv,
	struct v4l2_requestbuffers *buf)
{
	int ret = 0;
	struct is_video_ctx *vctx;
	struct is_device_ischain *device;

	FIMC_BUG(!buf);
	FIMC_BUG(!file);
	FIMC_BUG(!file->private_data);
	vctx = file->private_data;

	FIMC_BUG(!GET_DEVICE(vctx));
	device = GET_DEVICE(vctx);

	mdbgv_3aa("%s(buffers : %d)\n", vctx, __func__, buf->count);

	ret = is_video_reqbufs(file, vctx, buf);
	if (ret)
		merr("is_video_reqbufs is fail(%d)", device, ret);

	return ret;
}

static int is_3aa_video_querybuf(struct file *file, void *priv,
	struct v4l2_buffer *buf)
{
	int ret;
	struct is_video_ctx *vctx;
	struct is_device_ischain *device;

	FIMC_BUG(!buf);
	FIMC_BUG(!file);
	FIMC_BUG(!file->private_data);
	vctx = file->private_data;

	FIMC_BUG(!GET_DEVICE(vctx));
	device = GET_DEVICE(vctx);

	mdbgv_3aa("%s\n", vctx, __func__);

	ret = is_video_querybuf(file, vctx, buf);
	if (ret)
		merr("is_video_querybuf is fail(%d)", device, ret);

	return ret;
}

static int is_3aa_video_qbuf(struct file *file, void *priv,
	struct v4l2_buffer *buf)
{
	int ret = 0;
	struct is_video_ctx *vctx;
	struct is_device_ischain *device;

	FIMC_BUG(!buf);
	FIMC_BUG(!file);
	FIMC_BUG(!file->private_data);
	vctx = file->private_data;

	FIMC_BUG(!GET_DEVICE(vctx));
	device = GET_DEVICE(vctx);

	mvdbgs(3, "%s(%02d:%d)\n", vctx, &vctx->queue, __func__, buf->type, buf->index);

	ret = CALL_VOPS(vctx, qbuf, buf);
	if (ret)
		merr("qbuf is fail(%d)", device, ret);

	return ret;
}

static int is_3aa_video_dqbuf(struct file *file, void *priv,
	struct v4l2_buffer *buf)
{
	int ret = 0;
	struct is_video_ctx *vctx;
	struct is_device_ischain *device;
	bool blocking;

	FIMC_BUG(!buf);
	FIMC_BUG(!file);
	FIMC_BUG(!file->private_data);
	vctx = file->private_data;
	blocking = file->f_flags & O_NONBLOCK;

	FIMC_BUG(!GET_DEVICE(vctx));
	device = GET_DEVICE(vctx);

	mvdbgs(3, "%s\n", vctx, &vctx->queue, __func__);

	ret = CALL_VOPS(vctx, dqbuf, buf, blocking);
	if (ret)
		merr("dqbuf is fail(%d)", device, ret);

	return ret;
}

static int is_3aa_video_prepare(struct file *file, void *priv,
	struct v4l2_buffer *buf)
{
	int ret = 0;
	struct is_video_ctx *vctx;
	struct is_device_ischain *device;
	struct is_framemgr *framemgr;
	struct is_frame *frame;

	FIMC_BUG(!buf);
	FIMC_BUG(!file);
	FIMC_BUG(!file->private_data);
	vctx = file->private_data;

	FIMC_BUG(!GET_DEVICE(vctx));
	device = GET_DEVICE(vctx);

	FIMC_BUG(!GET_FRAMEMGR(vctx));
	framemgr = GET_FRAMEMGR(vctx);
	frame = &framemgr->frames[buf->index];

	ret = is_video_prepare(file, vctx, buf);
	if (ret) {
		merr("is_video_prepare is fail(%d)", device, ret);
		goto p_err;
	}

p_err:
	minfo("[3%dS:V] %s(%d):%d\n", device, GET_3XS_ID(GET_VIDEO(vctx)), __func__, buf->index, ret);
	return ret;
}

static int is_3aa_video_streamon(struct file *file, void *priv,
	enum v4l2_buf_type type)
{
	int ret = 0;
	struct is_video_ctx *vctx;
	struct is_device_ischain *device;

	FIMC_BUG(!file);
	FIMC_BUG(!file->private_data);
	vctx = file->private_data;

	FIMC_BUG(!GET_DEVICE(vctx));
	device = GET_DEVICE(vctx);

	mdbgv_3aa("%s\n", vctx, __func__);

	ret = is_video_streamon(file, vctx, type);
	if (ret)
		merr("is_video_streamon is fail(%d)", device, ret);

	return ret;
}

static int is_3aa_video_streamoff(struct file *file, void *priv,
	enum v4l2_buf_type type)
{
	int ret = 0;
	struct is_video_ctx *vctx;
	struct is_device_ischain *device;

	FIMC_BUG(!file);
	FIMC_BUG(!file->private_data);
	vctx = file->private_data;

	FIMC_BUG(!GET_DEVICE(vctx));
	device = GET_DEVICE(vctx);

	mdbgv_3aa("%s\n", vctx, __func__);

	ret = is_video_streamoff(file, vctx, type);
	if (ret)
		merr("is_video_streamoff is fail(%d)", device, ret);

	return ret;
}

static int is_3aa_video_enum_input(struct file *file, void *priv,
	struct v4l2_input *input)
{
	/* Todo: add enum input control code */
	return 0;
}

static int is_3aa_video_g_input(struct file *file, void *priv,
	unsigned int *input)
{
	/* Todo: add to get input control code */
	return 0;
}

static int is_3aa_video_s_input(struct file *file, void *priv,
	unsigned int input)
{
	int ret = 0;
	u32 stream, position, vindex, intype, leader;
	struct is_video_ctx *vctx;
	struct is_device_ischain *device;

	FIMC_BUG(!file);
	FIMC_BUG(!file->private_data);
	vctx = file->private_data;

	FIMC_BUG(!GET_DEVICE(vctx));
	device = GET_DEVICE(vctx);

	stream = (input & INPUT_STREAM_MASK) >> INPUT_STREAM_SHIFT;
	position = (input & INPUT_POSITION_MASK) >> INPUT_POSITION_SHIFT;
	vindex = (input & INPUT_VINDEX_MASK) >> INPUT_VINDEX_SHIFT;
	intype = (input & INPUT_INTYPE_MASK) >> INPUT_INTYPE_SHIFT;
	leader = (input & INPUT_LEADER_MASK) >> INPUT_LEADER_SHIFT;

	mdbgv_3aa("%s(input : %08X)[%d,%d,%d,%d,%d]\n", vctx, __func__, input,
			stream, position, vindex, intype, leader);

	ret = is_video_s_input(file, vctx);
	if (ret) {
		merr("is_video_s_input is fail(%d)", device, ret);
		goto p_err;
	}

	ret = is_ischain_3aa_s_input(device, stream, position,
					vindex, intype, leader);
	if (ret) {
		merr("is_ischain_3aa_s_input is fail(%d)", device, ret);
		goto p_err;
	}

p_err:
	return ret;
}

static int is_3aa_video_s_ctrl(struct file *file, void *priv,
	struct v4l2_control *ctrl)
{
	int ret = 0;
	struct is_video_ctx *vctx;
	struct is_device_ischain *device;
	unsigned int value = 0;
	unsigned int captureIntent = 0;
	unsigned int captureCount = 0;
	struct is_group *head;

	FIMC_BUG(!ctrl);
	FIMC_BUG(!file);
	FIMC_BUG(!file->private_data);
	vctx = file->private_data;

	FIMC_BUG(!GET_DEVICE(vctx));
	device = GET_DEVICE(vctx);

	mdbgv_3aa("%s\n", vctx, __func__);

	switch (ctrl->id) {
	case V4L2_CID_IS_INTENT:
		value = (unsigned int)ctrl->value;
		captureIntent = (value >> 16) & 0x0000FFFF;
		switch (captureIntent) {
		case AA_CAPTURE_INTENT_STILL_CAPTURE_DEBLUR_DYNAMIC_SHOT:
		case AA_CAPTURE_INTENT_STILL_CAPTURE_OIS_DYNAMIC_SHOT:
		case AA_CAPTURE_INTENT_STILL_CAPTURE_EXPOSURE_DYNAMIC_SHOT:
		case AA_CAPTURE_INTENT_STILL_CAPTURE_MFHDR_DYNAMIC_SHOT:
		case AA_CAPTURE_INTENT_STILL_CAPTURE_LLHDR_DYNAMIC_SHOT:
		case AA_CAPTURE_INTENT_STILL_CAPTURE_SUPER_NIGHT_SHOT_HANDHELD:
		case AA_CAPTURE_INTENT_STILL_CAPTURE_SUPER_NIGHT_SHOT_TRIPOD:
		case AA_CAPTURE_INTENT_STILL_CAPTURE_LLHDR_VEHDR_DYNAMIC_SHOT:
		case AA_CAPTURE_INTENT_STILL_CAPTURE_VENR_DYNAMIC_SHOT:
		case AA_CAPTURE_INTENT_STILL_CAPTURE_LLS_FLASH:
		case AA_CAPTURE_INTENT_STILL_CAPTURE_SUPER_NIGHT_SHOT_HANDHELD_FAST:
		case AA_CAPTURE_INTENT_STILL_CAPTURE_SUPER_NIGHT_SHOT_TRIPOD_FAST:
		case AA_CAPTURE_INTENT_STILL_CAPTURE_SUPER_NIGHT_SHOT_TRIPOD_LE_FAST:
		case AA_CAPTURE_INTENT_STILL_CAPTURE_CROPPED_REMOSAIC_DYNAMIC_SHOT:
		case AA_CAPTURE_INTENT_STILL_CAPTURE_SHORT_REF_LLHDR_DYNAMIC_SHOT:
			captureCount = value & 0x0000FFFF;
			break;
		default:
			captureIntent = ctrl->value;
			captureCount = 0;
			break;
		}

		head = GET_HEAD_GROUP_IN_DEVICE(IS_DEVICE_ISCHAIN, &device->group_3aa);

		head->intent_ctl.captureIntent = captureIntent;
		head->intent_ctl.vendor_captureCount = captureCount;
		if (captureIntent == AA_CAPTURE_INTENT_STILL_CAPTURE_OIS_MULTI) {
			head->remainIntentCount = 2 + INTENT_RETRY_CNT;
		} else {
			head->remainIntentCount = 0 + INTENT_RETRY_CNT;
		}

		minfo("[3AA:V] s_ctrl intent(%d) count(%d) remainIntentCount(%d)\n",
			device, captureIntent, captureCount, head->remainIntentCount);
		break;
	case V4L2_CID_IS_FORCE_DONE:
		set_bit(IS_GROUP_REQUEST_FSTOP, &device->group_3aa.state);
		break;
	case V4L2_CID_IS_FAST_CTL_LENS_POS:
		{
			struct fast_control_mgr *fastctlmgr = &device->fastctlmgr;
			struct is_fast_ctl *fast_ctl = NULL;
			unsigned long flags;
			u32 state;

			spin_lock_irqsave(&fastctlmgr->slock, flags);

			state = IS_FAST_CTL_FREE;
			if (fastctlmgr->queued_count[state]) {
				/* get free list */
				fast_ctl = list_first_entry(&fastctlmgr->queued_list[state],
					struct is_fast_ctl, list);
				list_del(&fast_ctl->list);
				fastctlmgr->queued_count[state]--;

				/* Write fast_ctl: lens */
				if (ctrl->id == V4L2_CID_IS_FAST_CTL_LENS_POS) {
					fast_ctl->lens_pos = ctrl->value;
					fast_ctl->lens_pos_flag = true;
				}

				/* TODO: Here is place for additional fast_ctl. */

				/* set req list */
				state = IS_FAST_CTL_REQUEST;
				fast_ctl->state = state;
				list_add_tail(&fast_ctl->list, &fastctlmgr->queued_list[state]);
				fastctlmgr->queued_count[state]++;
			} else {
				mwarn("not enough fast_ctl free queue\n", device, ctrl->value);
			}

			spin_unlock_irqrestore(&fastctlmgr->slock, flags);

			if (fast_ctl)
				mdbgv_3aa("%s: uctl.lensUd.pos(%d)\n", vctx, __func__, ctrl->value);
		}
		break;
	default:
		ret = is_video_s_ctrl(file, vctx, ctrl);
		if (ret) {
			merr("is_video_s_ctrl is fail(%d)", device, ret);
			goto p_err;
		}
		break;
	}

p_err:
	return ret;
}

static int is_3aa_video_g_ctrl(struct file *file, void *priv,
	struct v4l2_control *ctrl)
{
	/* Todo: add to get control code */
	return 0;
}

static int is_3aa_video_s_ext_ctrl(struct file *file, void *priv,
	struct v4l2_ext_controls *ctrls)
{
	int ret = 0;
	int i;
	struct is_video_ctx *vctx;
	struct is_device_ischain *device;
	struct is_framemgr *framemgr;
	struct is_queue *queue;
	struct v4l2_ext_control *ext_ctrl;
	struct v4l2_control ctrl;
	struct nfd_info *nfd_data, local_nfd_info;
	unsigned long flags;

	FIMC_BUG(!ctrls);
	FIMC_BUG(!file);
	FIMC_BUG(!file->private_data);
	vctx = file->private_data;

	FIMC_BUG(!GET_DEVICE(vctx));
	device = GET_DEVICE(vctx);

	FIMC_BUG(!GET_QUEUE(vctx));
	queue = GET_QUEUE(vctx);
	framemgr = &queue->framemgr;

	mdbgv_3aa("%s\n", vctx, __func__);

	if (ctrls->which != V4L2_CTRL_CLASS_CAMERA) {
		merr("Invalid control class(%d)", device, ctrls->which);
		ret = -EINVAL;
		goto p_err;
	}

	for (i = 0; i < ctrls->count; i++) {
		ext_ctrl = (ctrls->controls + i);

		switch (ext_ctrl->id) {
#ifdef ENABLE_ULTRA_FAST_SHOT
		case V4L2_CID_IS_FAST_CAPTURE_CONTROL:
			{
				struct fast_ctl_capture *fast_capture =
					(struct fast_ctl_capture *)&device->is_region->fast_ctl.fast_capture;
				ret = copy_from_user(fast_capture, ext_ctrl->ptr, sizeof(struct fast_ctl_capture));
				if (ret) {
					merr("copy_from_user is fail(%d)", device, ret);
					goto p_err;
				}

				fast_capture->ready = 1;
				device->fastctlmgr.fast_capture_count = 2;

				vb2_ion_sync_for_device(
					device->imemory.fw_cookie,
					(ulong)fast_capture - device->imemory.kvaddr,
					sizeof(struct fast_ctl_capture),
					DMA_TO_DEVICE);

				mvinfo("Fast capture control(Intent:%d, count:%d, exposureTime:%d)\n",
					vctx, vctx->video, fast_capture->capture_intent, fast_capture->capture_count,
					fast_capture->capture_exposureTime);
			}
			break;
#endif
		case V4L2_CID_IS_S_NFD_DATA:
			nfd_data = (struct nfd_info *)&device->is_region->fd_info;
			ret = copy_from_user(&local_nfd_info, ext_ctrl->ptr, sizeof(struct nfd_info));
			spin_lock_irqsave(&device->is_region->fd_info_slock, flags);
			memcpy((void *)nfd_data, &local_nfd_info, sizeof(struct nfd_info));
			if ((nfd_data->face_num < 0) || (nfd_data->face_num > MAX_FACE_COUNT))
				nfd_data->face_num = 0;
			spin_unlock_irqrestore(&device->is_region->fd_info_slock, flags);
			if (ret) {
				merr("copy_from_user of nfd_info is fail(%d)",
							device, ret);
				goto p_err;
			}

			mdbgv_3aa("[F%d] face num(%d)\n", vctx,
				nfd_data->frame_count, nfd_data->face_num);
			break;
		case V4L2_CID_IS_G_SETFILE_VERSION:
			ret = is_ischain_g_ddk_setfile_version(device, ext_ctrl->ptr);
			break;
		case V4L2_CID_SENSOR_SET_FLASH_FASTSHOT:
		{
			struct is_group *head;

			head = GET_HEAD_GROUP_IN_DEVICE(IS_DEVICE_ISCHAIN, &device->group_paf);

			ret = copy_from_user(&head->flash_ctl, ext_ctrl->ptr, sizeof(enum camera_flash_mode));
			if (ret) {
				err("fail to copy_from_user, ret(%d)\n", ret);
				goto p_err;
			}
			head->remainFlashCtlCount = 5;
			break;
		}
		case V4L2_CID_SENSOR_SET_CAPTURE_INTENT_INFO:
		{
			struct is_group *head;
			struct capture_intent_info_t info;

			ret = copy_from_user(&info, ext_ctrl->ptr, sizeof(struct capture_intent_info_t));
			if (ret) {
				err("fail to copy_from_user, ret(%d)\n", ret);
				goto p_err;
			}

			head = GET_HEAD_GROUP_IN_DEVICE(IS_DEVICE_ISCHAIN, &device->group_3aa);

			head->intent_ctl.captureIntent = info.captureIntent;
			head->intent_ctl.vendor_captureCount = info.captureCount;
			head->intent_ctl.vendor_captureEV = info.captureEV;
			if (info.captureIso) {
				head->intent_ctl.vendor_isoValue = info.captureIso;
			}
			if (info.captureAeExtraMode) {
				head->intent_ctl.vendor_aeExtraMode = info.captureAeExtraMode;
			}
			if (info.captureAeMode) {
				head->intent_ctl.aeMode = info.captureAeMode;
			}
			memcpy(&(head->intent_ctl.vendor_multiFrameEvList), &(info.captureMultiEVList),
				sizeof(info.captureMultiEVList));
			memcpy(&(head->intent_ctl.vendor_multiFrameIsoList), &(info.captureMultiIsoList),
				sizeof(info.captureMultiIsoList));
			memcpy(&(head->intent_ctl.vendor_multiFrameExposureList), &(info.CaptureMultiExposureList),
				sizeof(info.CaptureMultiExposureList));

			switch (info.captureIntent) {
			case AA_CAPTURE_INTENT_STILL_CAPTURE_OIS_MULTI:
			case AA_CAPTURE_INTENT_STILL_CAPTURE_GALAXY_RAW_DYNAMIC_SHOT:
			case AA_CAPTURE_INTENT_STILL_CAPTURE_EXECUTOR_NIGHT_SHOT:
			case AA_CAPTURE_INTENT_STILL_CAPTURE_ASTRO_SHOT:
				head->remainIntentCount = 3 + INTENT_RETRY_CNT;
				break;
			default:
				head->remainIntentCount = 1 + INTENT_RETRY_CNT;
				break;
			}
			mginfo("s_ext_ctrl SET_CAPTURE_INTENT_INFO, intent(%d) count(%d) captureEV(%d) captureIso(%d)"
				"captureAeExtraMode(%d) captureAeMode(%d) remainIntentCount(%d)\n",
				head, head, info.captureIntent, info.captureCount, info.captureEV, info.captureIso,
				info.captureAeExtraMode, info.captureAeMode, head->remainIntentCount);
			break;
		}
		default:
			ctrl.id = ext_ctrl->id;
			ctrl.value = ext_ctrl->value;

			ret = is_video_s_ctrl(file, vctx, &ctrl);
			if (ret) {
				merr("is_video_s_ctrl is fail(%d)", device, ret);
				goto p_err;
			}
			break;
		}
	}

p_err:
	return ret;
}

static int is_3aa_video_g_ext_ctrl(struct file *file, void *priv,
	struct v4l2_ext_controls *ctrls)
{
	/* Todo: add to get extra control code */
	return 0;
}

const struct v4l2_ioctl_ops is_3aa_video_ioctl_ops = {
	.vidioc_querycap		= is_3aa_video_querycap,

	.vidioc_g_fmt_vid_out_mplane	= is_3aa_video_get_format_mplane,
	.vidioc_g_fmt_vid_cap_mplane	= is_3aa_video_get_format_mplane,

	.vidioc_s_fmt_vid_out_mplane	= is_3aa_video_set_format_mplane,
	.vidioc_s_fmt_vid_cap_mplane	= is_3aa_video_set_format_mplane,

	.vidioc_querybuf		= is_3aa_video_querybuf,
	.vidioc_reqbufs			= is_3aa_video_reqbufs,

	.vidioc_qbuf			= is_3aa_video_qbuf,
	.vidioc_dqbuf			= is_3aa_video_dqbuf,
	.vidioc_prepare_buf		= is_3aa_video_prepare,

	.vidioc_streamon		= is_3aa_video_streamon,
	.vidioc_streamoff		= is_3aa_video_streamoff,

	.vidioc_enum_input		= is_3aa_video_enum_input,
	.vidioc_g_input			= is_3aa_video_g_input,
	.vidioc_s_input			= is_3aa_video_s_input,

	.vidioc_s_ctrl			= is_3aa_video_s_ctrl,
	.vidioc_g_ctrl			= is_3aa_video_g_ctrl,
	.vidioc_s_ext_ctrls		= is_3aa_video_s_ext_ctrl,
	.vidioc_g_ext_ctrls		= is_3aa_video_g_ext_ctrl,
};

static int is_3aa_queue_setup(struct vb2_queue *vbq,
	unsigned int *num_buffers,
	unsigned int *num_planes,
	unsigned int sizes[],
	struct device *alloc_devs[])
{
	int ret = 0;
	struct is_video_ctx *vctx;
	struct is_device_ischain *device;
	struct is_video *video;
	struct is_queue *queue;

	FIMC_BUG(!vbq);
	FIMC_BUG(!vbq->drv_priv);
	vctx = vbq->drv_priv;

	FIMC_BUG(!GET_DEVICE(vctx));
	device = GET_DEVICE(vctx);

	FIMC_BUG(!GET_VIDEO(vctx));
	video = GET_VIDEO(vctx);

	FIMC_BUG(!GET_QUEUE(vctx));
	queue = GET_QUEUE(vctx);

	mdbgv_3aa("%s\n", vctx, __func__);

	ret = is_queue_setup(queue,
		video->alloc_ctx,
		num_planes,
		sizes,
		alloc_devs);
	if (ret)
		merr("is_queue_setup is fail(%d)", device, ret);

	return ret;
}

static int is_3aa_start_streaming(struct vb2_queue *vbq,
	unsigned int count)
{
	int ret = 0;
	struct is_video_ctx *vctx = vbq->drv_priv;
	struct is_queue *queue;
	struct is_device_ischain *device;

	FIMC_BUG(!vbq);
	FIMC_BUG(!vbq->drv_priv);
	vctx = vbq->drv_priv;

	FIMC_BUG(!GET_DEVICE(vctx));
	device = GET_DEVICE(vctx);

	FIMC_BUG(!GET_QUEUE(vctx));
	queue = GET_QUEUE(vctx);

	mdbgv_3aa("%s\n", vctx, __func__);

	ret = is_queue_start_streaming(queue, device);
	if (ret) {
		merr("is_queue_start_streaming is fail(%d)", device, ret);
		goto p_err;
	}

p_err:
	return ret;
}

static void is_3aa_stop_streaming(struct vb2_queue *vbq)
{
	int ret = 0;
	struct is_video_ctx *vctx;
	struct is_queue *queue;
	struct is_device_ischain *device;

	FIMC_BUG_VOID(!vbq);
	FIMC_BUG_VOID(!vbq->drv_priv);
	vctx = vbq->drv_priv;

	FIMC_BUG_VOID(!GET_DEVICE(vctx));
	device = GET_DEVICE(vctx);

	FIMC_BUG_VOID(!GET_QUEUE(vctx));
	queue = GET_QUEUE(vctx);

	mdbgv_3aa("%s\n", vctx, __func__);

	ret = is_queue_stop_streaming(queue, device);
	if (ret) {
		merr("is_queue_stop_streaming is fail(%d)", device, ret);
		return;
	}
}

static void is_3aa_buffer_queue(struct vb2_buffer *vb)
{
	int ret = 0;
	struct is_video_ctx *vctx;
	struct is_device_ischain *device;
	struct is_queue *queue;

	FIMC_BUG_VOID(!vb);
	FIMC_BUG_VOID(!vb->vb2_queue);
	FIMC_BUG_VOID(!vb->vb2_queue->drv_priv);
	vctx = vb->vb2_queue->drv_priv;

	FIMC_BUG_VOID(!GET_DEVICE(vctx));
	device = GET_DEVICE(vctx);

	FIMC_BUG_VOID(!GET_QUEUE(vctx));
	queue = GET_QUEUE(vctx);

	mvdbgs(3, "%s(%d)\n", vctx, &vctx->queue, __func__, vb->index);

	ret = is_queue_buffer_queue(queue, vb);
	if (ret) {
		merr("is_queue_buffer_queue is fail(%d)", device, ret);
		return;
	}

	ret = is_ischain_3aa_buffer_queue(device, queue, vb->index);
	if (ret) {
		merr("is_ischain_3aa_buffer_queue is fail(%d)", device, ret);
		return;
	}
}

static void is_3aa_buffer_finish(struct vb2_buffer *vb)
{
	int ret;
	struct is_video_ctx *vctx;
	struct is_device_ischain *device;

	FIMC_BUG_VOID(!vb);
	FIMC_BUG_VOID(!vb->vb2_queue);
	FIMC_BUG_VOID(!vb->vb2_queue->drv_priv);
	vctx = vb->vb2_queue->drv_priv;

	FIMC_BUG_VOID(!GET_DEVICE(vctx));
	device = GET_DEVICE(vctx);

	mvdbgs(3, "%s(%d)\n", vctx, &vctx->queue, __func__, vb->index);

	ret = is_ischain_3aa_buffer_finish(device, vb->index);
	if (ret)
		merr("is_ischain_3aa_buffer_finish is fail(%d)", device, ret);

	is_queue_buffer_finish(vb);
}

const struct vb2_ops is_3aa_qops = {
	.queue_setup		= is_3aa_queue_setup,
	.buf_init		= is_queue_buffer_init,
	.buf_cleanup		= is_queue_buffer_cleanup,
	.buf_prepare		= is_queue_buffer_prepare,
	.buf_queue		= is_3aa_buffer_queue,
	.buf_finish		= is_3aa_buffer_finish,
	.wait_prepare		= is_queue_wait_prepare,
	.wait_finish		= is_queue_wait_finish,
	.start_streaming	= is_3aa_start_streaming,
	.stop_streaming		= is_3aa_stop_streaming,
};
