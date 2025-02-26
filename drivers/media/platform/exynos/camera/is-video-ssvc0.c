/*
 * Samsung Exynos SoC series FIMC-IS driver
 *
 * exynos fimc-is2 video functions
 *
 * Copyright (c) 2015 Samsung Electronics Co., Ltd
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

#include "is-hw.h"
#include "is-core.h"
#include "is-cmd.h"
#include "is-err.h"
#include "is-video.h"

const struct v4l2_file_operations is_ssxvc0_video_fops;
const struct v4l2_ioctl_ops is_ssxvc0_video_ioctl_ops;
const struct vb2_ops is_ssxvc0_qops;

#define TRACKING_BUF_INDEX
#ifdef TRACKING_BUF_INDEX
#define MAX_BUFFERS_VC0	IS_MAX_BUFS
static u32 qbuf_index[IS_SENSOR_COUNT];
static u32 dqbuf_index[IS_SENSOR_COUNT];
static u32 qbuf_arr[IS_SENSOR_COUNT][MAX_BUFFERS_VC0];
#endif

int is_ssxvc0_video_probe(void *data)
{
	int ret = 0;
	struct is_device_sensor *device;
	struct is_video *video;
	char* video_name;
	u32 video_id;
	u32 device_id;

	FIMC_BUG(!data);

	device = (struct is_device_sensor *)data;
	if (!device->pdev) {
		probe_err("pdev is NULL");
		ret = -EINVAL;
		goto p_err;
	}

	device_id = device->device_id;
	video = &device->video_ssxvc0;
	video->resourcemgr = device->resourcemgr;

	if (device_id >= IS_SENSOR_COUNT) {
		err("device_id %d was invalid", device_id);
		ret = -EINVAL;
		goto p_err;
	}

	video_name = IS_VIDEO_SSXVC0_NAME(device_id);
	video_id = IS_VIDEO_SS0VC0_NUM + (device_id * 4);

	ret = is_video_probe(video,
			video_name,
			video_id,
			VFL_DIR_RX,
			&device->resourcemgr->mem,
			&device->v4l2_dev,
			&is_ssxvc0_video_fops,
			&is_ssxvc0_video_ioctl_ops);
	if (ret)
		dev_err(&device->pdev->dev, "%s is fail(%d)\n", __func__, ret);

p_err:
	return ret;
}

/*
 * =============================================================================
 * Video File Opertation
 * =============================================================================
 */

static int is_ssxvc0_video_open(struct file *file)
{
	int ret = 0;
	int ret_err = 0;
	u32 device_id;
	u32 resource_type;
	struct is_video *video;
	struct is_video_ctx *vctx;
	struct is_device_sensor *device;
	struct is_resourcemgr *resourcemgr;
	char name[IS_STR_LEN];

	FIMC_BUG(!file);
	vctx = NULL;
	device = NULL;
	video = video_drvdata(file);
	resourcemgr = video->resourcemgr;
	if (!resourcemgr) {
		err("resourcemgr is NULL");
		ret = -EINVAL;
		goto err_resource_null;
	}

	device_id = ((video->id - IS_VIDEO_SS0VC0_NUM) >> 2);
	switch(device_id) {
	case 0:
		resource_type = RESOURCE_TYPE_SENSOR0;
		break;
	case 1:
		resource_type = RESOURCE_TYPE_SENSOR1;
		break;
	case 2:
		resource_type = RESOURCE_TYPE_SENSOR2;
		break;
	case 3:
		resource_type = RESOURCE_TYPE_SENSOR3;
		break;
	case 4:
		resource_type = RESOURCE_TYPE_SENSOR4;
		break;
	case 5:
		resource_type = RESOURCE_TYPE_SENSOR5;
		break;
	default:
		err("video id was invalid %d", video->id);
		ret = -EINVAL;
		goto err_video_id;
	}

	ret = is_resource_open(resourcemgr, resource_type, (void **)&device);
	if (ret) {
		err("is_resource_open is fail(%d)", ret);
		goto err_resource_open;
	}

	if (!device) {
		err("device is NULL");
		ret = -EINVAL;
		goto err_device_null;
	}

	minfo("[SS%dVC0:V] %s\n", device, device->device_id, __func__);

	snprintf(name, sizeof(name), "SS%dVC0", device->device_id);
	ret = open_vctx(file, video, &vctx, device->instance, ENTRY_SSVC0, name);
	if (ret) {
		merr("open_vctx is fail(%d)", device, ret);
		goto err_vctx_open;
	}

	ret = is_video_open(vctx,
		device,
		VIDEO_SSXVC0_READY_BUFFERS,
		video,
		&is_ssxvc0_qops,
		&is_sensor_subdev_ops);
	if (ret) {
		merr("is_video_open is fail(%d)", device, ret);
		goto err_video_open;
	}

	ret = is_sensor_subdev_open(device, vctx);
	if (ret) {
		merr("is_sensor_subdev_open is fail(%d)", device, ret);
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
err_video_id:
err_resource_null:
	return ret;
}

static int is_ssxvc0_video_close(struct file *file)
{
	int ret = 0;
	int refcount;
	struct is_video_ctx *vctx;
	struct is_video *video;
	struct is_device_sensor *device;

	FIMC_BUG(!file);
	FIMC_BUG(!file->private_data);
	vctx = file->private_data;

	FIMC_BUG(!GET_DEVICE(vctx));
	device = GET_DEVICE(vctx);

	FIMC_BUG(!GET_VIDEO(vctx));
	video = GET_VIDEO(vctx);

	ret = is_sensor_subdev_close(device, vctx);
	if (ret)
		merr("is_sensor_subdev_close is fail(%d)", device, ret);

	ret = is_video_close(vctx);
	if (ret)
		merr("is_video_close is fail(%d)", device, ret);

	refcount = close_vctx(file, video, vctx);
	if (refcount < 0)
		merr("close_vctx is fail(%d)", device, refcount);

	minfo("[SS%dVC0:V] %s(%d):%d\n", device, device->device_id, __func__, refcount, ret);

	return ret;
}

static unsigned int is_ssxvc0_video_poll(struct file *file,
	struct poll_table_struct *wait)
{
	u32 ret = 0;
	struct is_video_ctx *vctx;
	struct is_device_sensor *device;

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

static int is_ssxvc0_video_mmap(struct file *file,
	struct vm_area_struct *vma)
{
	int ret = 0;
	struct is_video_ctx *vctx;
	struct is_device_sensor *device;

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

const struct v4l2_file_operations is_ssxvc0_video_fops = {
	.owner		= THIS_MODULE,
	.open		= is_ssxvc0_video_open,
	.release	= is_ssxvc0_video_close,
	.poll		= is_ssxvc0_video_poll,
	.unlocked_ioctl	= video_ioctl2,
	.mmap		= is_ssxvc0_video_mmap,
};

/*
 * =============================================================================
 * Video Ioctl Opertation
 * =============================================================================
 */

static int is_ssxvc0_video_querycap(struct file *file, void *fh,
	struct v4l2_capability *cap)
{
	struct is_video *video = video_drvdata(file);

	FIMC_BUG(!cap);
	FIMC_BUG(!video);

	snprintf(cap->driver, sizeof(cap->driver), "%s", video->vd.name);
	snprintf(cap->card, sizeof(cap->card), "%s", video->vd.name);
	cap->capabilities |= V4L2_CAP_STREAMING
			| V4L2_CAP_VIDEO_CAPTURE
			| V4L2_CAP_VIDEO_CAPTURE_MPLANE;
	cap->device_caps |= cap->capabilities;

	return 0;
}

static int is_ssxvc0_video_get_format_mplane(struct file *file, void *fh,
	struct v4l2_format *format)
{
	dbg("%s\n", __func__);
	return 0;
}

static int is_ssxvc0_video_set_format_mplane(struct file *file, void *fh,
	struct v4l2_format *format)
{
	int ret = 0;
	struct is_video_ctx *vctx;
	struct is_device_sensor *device;

	FIMC_BUG(!format);
	FIMC_BUG(!file);
	FIMC_BUG(!file->private_data);
	vctx = file->private_data;

	FIMC_BUG(!GET_DEVICE(vctx));
	device = GET_DEVICE(vctx);

	mdbgv_ssxvc0("%s\n", vctx, __func__);

	ret = is_video_set_format_mplane(file, vctx, format);
	if (ret) {
		merr("is_video_set_format_mplane is fail(%d)", device, ret);
		goto p_err;
	}

p_err:
	return ret;
}

static int is_ssxvc0_video_try_format_mplane(struct file *file, void *fh,
	struct v4l2_format *format)
{
	dbg("%s\n", __func__);
	return 0;
}

static int is_ssxvc0_video_reqbufs(struct file *file, void *priv,
	struct v4l2_requestbuffers *buf)
{
	int ret = 0;
	struct is_video_ctx *vctx;
	struct is_device_sensor *device;
	struct is_video *video;

	FIMC_BUG(!buf);
	FIMC_BUG(!file);
	FIMC_BUG(!file->private_data);
	vctx = file->private_data;

	FIMC_BUG(!GET_DEVICE(vctx));
	device = GET_DEVICE(vctx);

	FIMC_BUG(!GET_VIDEO(vctx));
	video = GET_VIDEO(vctx);

	mdbgv_ssxvc0("%s(buffers : %d)\n", vctx, __func__, buf->count);

	if (test_bit(IS_SENSOR_BACK_START, &device->state)) {
		err("sensor%d still running, vid%d req is not applied", device->device_id, video->id);
		ret = -EINVAL;
		goto p_err;
	}

	ret = is_video_reqbufs(file, vctx, buf);
	if (ret) {
		merr("is_video_reqbufs is fail(%d)", device, ret);
		goto p_err;
	}
#ifdef TRACKING_BUF_INDEX
	{
		int i;
		int device_id = 0;
		struct is_device_sensor *device;

		device = GET_DEVICE(vctx);
		device_id = device->device_id;

		for (i = 0; i < MAX_BUFFERS_VC0; i++)
			qbuf_arr[device_id][i] = -1;

		qbuf_index[device_id] = 0;
		dqbuf_index[device_id] = 0;
	}
#endif

 p_err:
	return ret;
}

static int is_ssxvc0_video_querybuf(struct file *file, void *priv,
	struct v4l2_buffer *buf)
{
	int ret;
	struct is_video_ctx *vctx;
	struct is_device_sensor *device;

	FIMC_BUG(!buf);
	FIMC_BUG(!file);
	FIMC_BUG(!file->private_data);
	vctx = file->private_data;

	FIMC_BUG(!GET_DEVICE(vctx));
	device = GET_DEVICE(vctx);

	mdbgv_ssxvc0("%s\n", vctx, __func__);

	ret = is_video_querybuf(file, vctx, buf);
	if (ret)
		merr("is_video_querybuf is fail(%d)", device, ret);

	return ret;
}

static int is_ssxvc0_video_qbuf(struct file *file, void *priv,
	struct v4l2_buffer *buf)
{
	int ret = 0;
	struct is_video_ctx *vctx;
	struct is_device_sensor *device;

	FIMC_BUG(!buf);
	FIMC_BUG(!file);
	FIMC_BUG(!file->private_data);
	vctx = file->private_data;

	FIMC_BUG(!GET_DEVICE(vctx));
	device = GET_DEVICE(vctx);

	mvdbgs(3, "%s(index : %d)\n", vctx, &vctx->queue, __func__, buf->index);

	ret = CALL_VOPS(vctx, qbuf, buf);
	if (ret)
		merr("qbuf is fail(%d)", device, ret);

#ifdef TRACKING_BUF_INDEX
	if (!ret) {
	    int device_id = 0;
	    struct is_queue *queue;
	    struct is_device_sensor *device;

	    device = GET_DEVICE(vctx);
	    queue = GET_QUEUE(vctx);

	    if (device && queue) {
		    device_id = device->device_id;

		    qbuf_arr[device_id][qbuf_index[device_id]] = buf->index;
		    qbuf_index[device_id] = ((qbuf_index[device_id] + 1) % (queue->buf_maxcount));
	    }
	}
#endif

	return ret;
}

static int is_ssxvc0_video_dqbuf(struct file *file, void *priv,
	struct v4l2_buffer *buf)
{
	int ret = 0;
	struct is_video_ctx *vctx;
	struct is_device_sensor *device;
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

	mvdbgs(3, "%s(index: %d)\n", vctx, &vctx->queue,
				__func__, ret ? -1 : buf->index);

#ifdef TRACKING_BUF_INDEX
	if (!ret) {
		int i;
		int device_id = 0;
		struct is_queue *queue;
		struct is_device_sensor *device;

		queue = GET_QUEUE(vctx);
		device = GET_DEVICE(vctx);

		if (device && queue) {
			device_id = device->device_id;

			if (qbuf_arr[device_id][dqbuf_index[device_id]] != buf->index) {
				err("[%d] Detect!!(reverse index) >> ssxvc0_dqbuf: internal idx(%d), expected dq(%d) != actual dq(%d)\n",
						device_id,
						dqbuf_index[device_id],
						qbuf_arr[device_id][dqbuf_index[device_id]],
						buf->index);

				for (i = 0; i < queue->buf_maxcount; i++) {
					err(" - [%d] internal idx(q:%d, dq:%d), qbuf_arr[%d]: %d\n",
							device_id,
							qbuf_index[device_id],
							dqbuf_index[device_id],
							i,
							qbuf_arr[device_id][i]);
				}
			} else {
				dqbuf_index[device_id] = ((dqbuf_index[device_id] + 1) % (queue->buf_maxcount));
			}
		}
	}
#endif

	return ret;
}

static int is_ssxvc0_video_prepare(struct file *file, void *priv,
	struct v4l2_buffer *buf)
{
	int ret = 0;
	struct is_video_ctx *vctx;
	struct is_device_sensor *device;
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
	minfo("[SS%dVC0:V] %s(%d):%d\n", device, device->device_id, __func__, buf->index, ret);
	return ret;
}

static int is_ssxvc0_video_streamon(struct file *file, void *priv,
	enum v4l2_buf_type type)
{
	int ret = 0;
	struct is_video_ctx *vctx;
	struct is_device_sensor *device;

	FIMC_BUG(!file);
	FIMC_BUG(!file->private_data);
	vctx = file->private_data;

	FIMC_BUG(!GET_DEVICE(vctx));
	device = GET_DEVICE(vctx);

	mdbgv_ssxvc0("%s\n", vctx, __func__);

	ret = is_video_streamon(file, vctx, type);
	if (ret)
		merr("is_video_streamon is fail(%d)", device, ret);

	return ret;
}

static int is_ssxvc0_video_streamoff(struct file *file, void *priv,
	enum v4l2_buf_type type)
{
	int ret = 0;
	struct is_video_ctx *vctx;
	struct is_device_sensor *device;

	FIMC_BUG(!file);
	FIMC_BUG(!file->private_data);
	vctx = file->private_data;

	FIMC_BUG(!GET_DEVICE(vctx));
	device = GET_DEVICE(vctx);

	mdbgv_ssxvc0("%s\n", vctx, __func__);

	ret = is_video_streamoff(file, vctx, type);
	if (ret)
		merr("is_video_streamoff is fail(%d)", device, ret);

	return ret;
}

static int is_ssxvc0_video_enum_input(struct file *file, void *priv,
	struct v4l2_input *input)
{
	/* Todo: add enum input control code */
	return 0;
}

static int is_ssxvc0_video_g_input(struct file *file, void *priv,
	unsigned int *input)
{
	dbg("%s\n", __func__);
	return 0;
}

static int is_ssxvc0_video_s_input(struct file *file, void *priv,
	unsigned int input)
{
	int ret = 0;
	struct is_video_ctx *vctx;
	struct is_device_sensor *device;

	FIMC_BUG(!file);
	FIMC_BUG(!file->private_data);
	vctx = file->private_data;

	FIMC_BUG(!GET_DEVICE(vctx));
	device = GET_DEVICE(vctx);

	ret = is_video_s_input(file, vctx);
	if (ret) {
		merr("is_video_s_input is fail(%d)", device, ret);
		goto p_err;
	}

p_err:
	return ret;
}

static int is_ssxvc0_video_g_ctrl(struct file *file, void *priv,
	struct v4l2_control *ctrl)
{
	int ret = 0;
	struct is_video_ctx *vctx;
	struct is_framemgr *framemgr;

	FIMC_BUG(!ctrl);
	FIMC_BUG(!file);
	FIMC_BUG(!file->private_data);
	vctx = file->private_data;

	FIMC_BUG(!GET_FRAMEMGR(vctx));
	framemgr = GET_FRAMEMGR(vctx);

	mdbgv_ssxvc0("%s\n", vctx, __func__);

	switch (ctrl->id) {
	case V4L2_CID_IS_G_COMPLETES:
		ctrl->value = framemgr->queued_count[FS_COMPLETE];
		break;
	default:
		err("unsupported ioctl(%d)\n", ctrl->id);
		ret = -EINVAL;
		break;
	}

	return ret;
}

static int is_ssxvc0_video_g_ext_ctrl(struct file *file, void *priv,
	struct v4l2_ext_controls *ctrls)
{
	return 0;
}

static int is_ssxvc0_video_s_ctrl(struct file *file, void *priv,
	struct v4l2_control *ctrl)
{
	int ret = 0;
	struct is_video_ctx *vctx;

	FIMC_BUG(!ctrl);
	FIMC_BUG(!file);
	FIMC_BUG(!file->private_data);
	vctx = file->private_data;

	mdbgv_ssxvc0("%s\n", vctx, __func__);

	switch (ctrl->id) {
	default:
		ret = is_video_s_ctrl(file, vctx, ctrl);
		if (ret) {
			err("is_video_s_ctrl is fail(%d)", ret);
			goto p_err;
		}
		break;
	}

p_err:
	return ret;
}

const struct v4l2_ioctl_ops is_ssxvc0_video_ioctl_ops = {
	.vidioc_querycap		= is_ssxvc0_video_querycap,
	.vidioc_g_fmt_vid_cap_mplane	= is_ssxvc0_video_get_format_mplane,
	.vidioc_s_fmt_vid_cap_mplane	= is_ssxvc0_video_set_format_mplane,
	.vidioc_try_fmt_vid_cap_mplane	= is_ssxvc0_video_try_format_mplane,
	.vidioc_reqbufs			= is_ssxvc0_video_reqbufs,
	.vidioc_querybuf		= is_ssxvc0_video_querybuf,
	.vidioc_qbuf			= is_ssxvc0_video_qbuf,
	.vidioc_dqbuf			= is_ssxvc0_video_dqbuf,
	.vidioc_prepare_buf		= is_ssxvc0_video_prepare,
	.vidioc_streamon		= is_ssxvc0_video_streamon,
	.vidioc_streamoff		= is_ssxvc0_video_streamoff,
	.vidioc_enum_input		= is_ssxvc0_video_enum_input,
	.vidioc_g_input			= is_ssxvc0_video_g_input,
	.vidioc_s_input			= is_ssxvc0_video_s_input,
	.vidioc_g_ctrl			= is_ssxvc0_video_g_ctrl,
	.vidioc_s_ctrl			= is_ssxvc0_video_s_ctrl,
	.vidioc_g_ext_ctrls		= is_ssxvc0_video_g_ext_ctrl,
};

static int is_ssxvc0_queue_setup(struct vb2_queue *vbq,
	unsigned int *num_buffers,
	unsigned int *num_planes,
	unsigned int sizes[],
	struct device *alloc_devs[])
{
	int ret = 0;
	struct is_video_ctx *vctx;
	struct is_device_sensor *device;
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

	mdbgv_ssxvc0("%s\n", vctx, __func__);

	ret = is_queue_setup(queue,
		video->alloc_ctx,
		num_planes,
		sizes,
		alloc_devs);
	if (ret)
		merr("is_queue_setup is fail(%d)", device, ret);

	return ret;
}

static int is_ssxvc0_start_streaming(struct vb2_queue *vbq,
	unsigned int count)
{
	int ret = 0;
	struct is_video_ctx *vctx;
	struct is_queue *queue;
	struct is_device_sensor *device;

	FIMC_BUG(!vbq);
	FIMC_BUG(!vbq->drv_priv);
	vctx = vbq->drv_priv;

	FIMC_BUG(!GET_DEVICE(vctx));

	mdbgv_ssxvc0("%s\n", vctx, __func__);

	device = GET_DEVICE(vctx);
	queue = GET_QUEUE(vctx);

	ret = is_queue_start_streaming(queue, device);
	if (ret) {
		merr("is_queue_start_streaming is fail(%d)", device, ret);
		goto p_err;
	}

p_err:
	return ret;
}

static void is_ssxvc0_stop_streaming(struct vb2_queue *vbq)
{
	int ret = 0;
	struct is_video_ctx *vctx;
	struct is_queue *queue;
	struct is_device_sensor *device;

	FIMC_BUG_VOID(!vbq);
	FIMC_BUG_VOID(!vbq->drv_priv);
	vctx = vbq->drv_priv;

	mdbgv_ssxvc0("%s\n", vctx, __func__);

	device = GET_DEVICE(vctx);
	queue = GET_QUEUE(vctx);

	ret = is_queue_stop_streaming(queue, device);
	if (ret) {
		merr("is_queue_stop_streaming is fail(%d)", device, ret);
		return;
	}
}

static void is_ssxvc0_buffer_queue(struct vb2_buffer *vb)
{
	int ret = 0;
	struct is_video_ctx *vctx;
	struct is_queue *queue;
	struct is_device_sensor *device;
	struct is_subdev *subdev;

	FIMC_BUG_VOID(!vb);
	FIMC_BUG_VOID(!vb->vb2_queue->drv_priv);
	vctx = vb->vb2_queue->drv_priv;

	FIMC_BUG_VOID(!GET_DEVICE(vctx));
	device = GET_DEVICE(vctx);

	FIMC_BUG_VOID(!GET_QUEUE(vctx));
	queue = GET_QUEUE(vctx);

	mvdbgs(3, "%s(%d)\n", vctx, &vctx->queue, __func__, vb->index);

	subdev = &device->ssvc0;

	ret = is_queue_buffer_queue(queue, vb);
	if (ret) {
		merr("is_queue_buffer_queue is fail(%d)", device, ret);
		return;
	}

	ret = is_subdev_buffer_queue(subdev, vb);
	if (ret) {
		merr("is_subdev_buffer_queue is fail(%d)", device, ret);
		return;
	}

#if !defined(CONFIG_USE_SENSOR_GROUP)
	ret = is_sensor_subdev_buffer_queue(device, ENTRY_SSVC0, vb->index);
	if (ret) {
		merr("is_sensor_subdev_buffer_queue is fail(%d)", device, ret);
		return;
	}
#endif
}

static void is_ssxvc0_buffer_finish(struct vb2_buffer *vb)
{
	int ret;
	struct is_video_ctx *vctx;
	struct is_device_sensor *device;
	struct is_subdev *subdev;

	FIMC_BUG_VOID(!vb);
	FIMC_BUG_VOID(!vb->vb2_queue->drv_priv);
	vctx = vb->vb2_queue->drv_priv;

	FIMC_BUG_VOID(!GET_DEVICE(vctx));
	device = GET_DEVICE(vctx);

	mvdbgs(3, "%s(%d)\n", vctx, &vctx->queue, __func__, vb->index);

	subdev = &device->ssvc0;

	ret = is_subdev_buffer_finish(subdev, vb);
	if (ret)
		merr("is_subdev_buffer_finish is fail(%d)", device, ret);

	is_queue_buffer_finish(vb);
}

const struct vb2_ops is_ssxvc0_qops = {
	.queue_setup		= is_ssxvc0_queue_setup,
	.buf_init		= is_queue_buffer_init,
	.buf_cleanup		= is_queue_buffer_cleanup,
	.buf_prepare		= is_queue_buffer_prepare,
	.buf_queue		= is_ssxvc0_buffer_queue,
	.buf_finish		= is_ssxvc0_buffer_finish,
	.wait_prepare		= is_queue_wait_prepare,
	.wait_finish		= is_queue_wait_finish,
	.start_streaming	= is_ssxvc0_start_streaming,
	.stop_streaming		= is_ssxvc0_stop_streaming,
};
