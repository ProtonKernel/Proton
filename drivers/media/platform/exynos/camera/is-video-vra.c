/*
 * Samsung Exynos5 SoC series FIMC-IS driver
 *
 * exynos5 fimc-is video functions
 *
 * Copyright (c) 2011 Samsung Electronics Co., Ltd
 *
 * This program is free software; you can revratribute it and/or modify
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
#include <linux/videodev2.h>
#include <linux/videodev2_exynos_media.h>
#include <linux/videodev2_exynos_camera.h>
#include <linux/v4l2-mediabus.h>
#include <linux/bug.h>

#include "is-core.h"
#include "is-cmd.h"
#include "is-err.h"
#include "is-video.h"
#include "is-metadata.h"
#include "is-param.h"

#if defined(CONVERT_BUFFER_SECURE_TO_NON_SECURE)
#include "../../../../staging/android/ion/ion.h"
#endif

const struct v4l2_file_operations is_vra_video_fops;
const struct v4l2_ioctl_ops is_vra_video_ioctl_ops;
const struct vb2_ops is_vra_qops;

int is_vra_video_probe(void *data)
{
	int ret = 0;
	struct is_core *core;
	struct is_video *video;

	FIMC_BUG(!data);

	core = (struct is_core *)data;
	video = &core->video_vra;
	video->resourcemgr = &core->resourcemgr;

	if (!core->pdev) {
		probe_err("pdev is NULL");
		ret = -EINVAL;
		goto p_err;
	}

	ret = is_video_probe(video,
		IS_VIDEO_VRA_NAME,
		IS_VIDEO_VRA_NUM,
		VFL_DIR_M2M,
		&core->resourcemgr.mem,
		&core->v4l2_dev,
		&is_vra_video_fops,
		&is_vra_video_ioctl_ops);
	if (ret)
		dev_err(&core->pdev->dev, "%s is fail(%d)\n", __func__, ret);

p_err:
	return ret;
}

static int is_vra_video_open(struct file *file)
{
	int ret = 0;
	int ret_err = 0;
	struct is_video *video;
	struct is_video_ctx *vctx;
	struct is_device_ischain *device;
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

	minfo("[VRA:V] %s\n", device, __func__);

	snprintf(name, sizeof(name), "VRA");
	ret = open_vctx(file, video, &vctx, device->instance, ENTRY_VRA, name);
	if (ret) {
		merr("open_vctx is fail(%d)", device, ret);
		goto err_vctx_open;
	}

	ret = is_video_open(vctx,
		device,
		VIDEO_VRA_READY_BUFFERS,
		video,
		&is_vra_qops,
		&is_ischain_vra_ops);
	if (ret) {
		merr("is_video_open is fail(%d)", device, ret);
		goto err_video_open;
	}

	ret = is_ischain_vra_open(device, vctx);
	if (ret) {
		merr("is_ischain_vra_open is fail(%d)", device, ret);
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

static int is_vra_video_close(struct file *file)
{
	int ret = 0;
	int refcount;
	struct is_video_ctx *vctx;
	struct is_video *video;
	struct is_device_ischain *device;

	FIMC_BUG(!file);
	FIMC_BUG(!file->private_data);
	vctx = file->private_data;

	FIMC_BUG(!GET_DEVICE(vctx));
	device = GET_DEVICE(vctx);

	FIMC_BUG(!GET_VIDEO(vctx));
	video = GET_VIDEO(vctx);

	ret = is_ischain_vra_close(device, vctx);
	if (ret)
		merr("is_ischain_vra_close is fail(%d)", device, ret);

	ret = is_video_close(vctx);
	if (ret)
		merr("is_video_close is fail(%d)", device, ret);

	refcount = close_vctx(file, video, vctx);
	if (refcount < 0)
		merr("close_vctx is fail(%d)", device, refcount);

	minfo("[VRA:V] %s(%d,%d):%d\n", device, __func__, atomic_read(&device->open_cnt), refcount, ret);

	return ret;
}

static unsigned int is_vra_video_poll(struct file *file,
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

static int is_vra_video_mmap(struct file *file,
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

const struct v4l2_file_operations is_vra_video_fops = {
	.owner		= THIS_MODULE,
	.open		= is_vra_video_open,
	.release	= is_vra_video_close,
	.poll		= is_vra_video_poll,
	.unlocked_ioctl	= video_ioctl2,
	.mmap		= is_vra_video_mmap,
};

static int is_vra_video_querycap(struct file *file, void *fh,
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

static int is_vra_video_get_format_mplane(struct file *file, void *fh,
	struct v4l2_format *format)
{
	/* Todo: add get format control code */
	return 0;
}

static int is_vra_video_set_format_mplane(struct file *file, void *fh,
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

	mdbgv_vra("%s\n", vctx, __func__);

	ret = is_video_set_format_mplane(file, vctx, format);
	if (ret) {
		merr("is_video_set_format_mplane is fail(%d)", device, ret);
		goto p_err;
	}

p_err:
	return ret;
}

static int is_vra_video_reqbufs(struct file *file, void *priv,
	struct v4l2_requestbuffers *buf)
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

	mdbgv_vra("%s(buffers : %d)\n", vctx, __func__, buf->count);

	ret = is_video_reqbufs(file, vctx, buf);
	if (ret)
		merr("is_video_reqbufs is fail(error %d)", device, ret);

	return ret;
}

static int is_vra_video_querybuf(struct file *file, void *priv,
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

	mdbgv_vra("%s\n", vctx, __func__);

	ret = is_video_querybuf(file, vctx, buf);
	if (ret)
		merr("is_video_querybuf is fail(%d)", device, ret);

	return ret;
}

static int is_vra_video_qbuf(struct file *file, void *priv,
	struct v4l2_buffer *buf)
{
	int ret = 0;
	struct is_video_ctx *vctx;
	struct is_device_ischain *device;
	struct is_queue *queue;

	FIMC_BUG(!buf);
	FIMC_BUG(!file);
	FIMC_BUG(!file->private_data);
	vctx = file->private_data;

	FIMC_BUG(!GET_DEVICE(vctx));
	device = GET_DEVICE(vctx);

	FIMC_BUG(!GET_QUEUE(vctx));
	queue = GET_QUEUE(vctx);

	mvdbgs(3, "%s\n", vctx, &vctx->queue, __func__);

	if (!test_bit(IS_QUEUE_STREAM_ON, &queue->state)) {
		merr("stream off state, can NOT qbuf", device);
		ret = -EINVAL;
		goto p_err;
	}

	ret = CALL_VOPS(vctx, qbuf, buf);
	if (ret)
		merr("qbuf is fail(%d)", device, ret);

p_err:
	return ret;
}

static int is_vra_video_dqbuf(struct file *file, void *priv,
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

static int is_vra_video_prepare(struct file *file, void *priv,
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
	minfo("[VRA:V] %s(%d):%d\n", device, __func__, buf->index, ret);
	return ret;
}

static int is_vra_video_streamon(struct file *file, void *priv,
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

	mdbgv_vra("%s\n", vctx, __func__);

	ret = is_video_streamon(file, vctx, type);
	if (ret)
		merr("is_vra_video_streamon is fail(%d)", device, ret);

	return ret;
}

static int is_vra_video_streamoff(struct file *file, void *priv,
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

	mdbgv_vra("%s\n", vctx, __func__);

	ret = is_video_streamoff(file, vctx, type);
	if (ret)
		merr("is_video_streamoff is fail(%d)", device, ret);

	return ret;
}

static int is_vra_video_enum_input(struct file *file, void *priv,
						struct v4l2_input *input)
{
	/* Todo : add to enum input control code */
	return 0;
}

static int is_vra_video_g_input(struct file *file, void *priv,
	unsigned int *input)
{
	/* Todo: add get input control code */
	return 0;
}

static int is_vra_video_s_input(struct file *file, void *priv,
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

	mdbgv_vra("%s(input : %08X)[%d,%d,%d,%d,%d]\n", vctx, __func__, input,
			stream, position, vindex, intype, leader);

	ret = is_video_s_input(file, vctx);
	if (ret) {
		merr("is_video_s_input is fail(%d)", device, ret);
		goto p_err;
	}

	ret = is_ischain_vra_s_input(device, stream, position, vindex, intype, leader);
	if (ret) {
		merr("is_ischain_isp_s_input is fail(%d)", device, ret);
		goto p_err;
	}

p_err:
	return ret;
}

static int is_vra_video_s_ctrl(struct file *file, void *priv,
	struct v4l2_control *ctrl)
{
int ret = 0;
	struct is_video_ctx *vctx;
	struct is_device_ischain *device;

	FIMC_BUG(!ctrl);
	FIMC_BUG(!file);
	FIMC_BUG(!file->private_data);
	vctx = file->private_data;

	FIMC_BUG(!GET_DEVICE(vctx));
	device = GET_DEVICE(vctx);

	mdbgv_vra("%s\n", vctx, __func__);

	switch (ctrl->id) {
	case V4L2_CID_IS_FORCE_DONE:
		set_bit(IS_GROUP_REQUEST_FSTOP, &device->group_vra.state);
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

static int is_vra_video_g_ctrl(struct file *file, void *priv,
	struct v4l2_control *ctrl)
{
	/* Todo: add get control code */
	return 0;
}

static int is_vra_video_s_ext_ctrl(struct file *file, void *priv,
	struct v4l2_ext_controls *ctrls)
{
	int ret = 0;
	int i;
	struct is_video_ctx *vctx;
	struct is_device_ischain *device;
	struct v4l2_ext_control *ext_ctrl;
	struct v4l2_control ctrl;

	FIMC_BUG(!ctrls);
	FIMC_BUG(!file);
	FIMC_BUG(!file->private_data);
	vctx = file->private_data;

	FIMC_BUG(!GET_DEVICE(vctx));
	device = GET_DEVICE(vctx);

	mdbgv_vra("%s\n", vctx, __func__);

	if (ctrls->which != V4L2_CTRL_CLASS_CAMERA) {
		merr("Invalid control class(%d)", device, ctrls->which);
		ret = -EINVAL;
		goto p_err;
	}

	for (i = 0; i < ctrls->count; i++) {
		ext_ctrl = (ctrls->controls + i);

		switch (ext_ctrl->id) {
#ifdef ENABLE_HYBRID_FD
		case V4L2_CID_IS_FDAE:
			{
				unsigned long flags = 0;
				struct is_fdae_info *fdae_info =
					(struct is_fdae_info *)&device->is_region->fdae_info;

				spin_lock_irqsave(&fdae_info->slock, flags);
				ret = copy_from_user(fdae_info, ext_ctrl->ptr, sizeof(struct is_fdae_info) - sizeof(spinlock_t));
				spin_unlock_irqrestore(&fdae_info->slock, flags);
				if (ret) {
					merr("copy_from_user of fdae_info is fail(%d)", device, ret);
					goto p_err;
				}

				mdbgv_vra("[F%d] fdae_info(id: %d, score:%d, rect(%d, %d, %d, %d), face_num:%d\n",
					vctx,
					fdae_info->frame_count,
					fdae_info->id[0],
					fdae_info->score[0],
					fdae_info->rect[0].offset_x,
					fdae_info->rect[0].offset_y,
					fdae_info->rect[0].width,
					fdae_info->rect[0].height,
					fdae_info->face_num);
			}
			break;
#endif
#if defined(CONVERT_BUFFER_SECURE_TO_NON_SECURE)
		case V4L2_CID_IS_CONVERT_BUFFER_SECURE_TO_NON_SUCURE:
		{
			break;
		}
#endif

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

static int is_vra_video_g_ext_ctrl(struct file *file, void *priv,
	struct v4l2_ext_controls *ctrls)
{
	/* Todo: add get extra control code */
	return 0;
}

const struct v4l2_ioctl_ops is_vra_video_ioctl_ops = {
	.vidioc_querycap		= is_vra_video_querycap,
	.vidioc_g_fmt_vid_out_mplane	= is_vra_video_get_format_mplane,
	.vidioc_s_fmt_vid_out_mplane	= is_vra_video_set_format_mplane,
	.vidioc_reqbufs			= is_vra_video_reqbufs,
	.vidioc_querybuf		= is_vra_video_querybuf,
	.vidioc_qbuf			= is_vra_video_qbuf,
	.vidioc_dqbuf			= is_vra_video_dqbuf,
	.vidioc_prepare_buf		= is_vra_video_prepare,
	.vidioc_streamon		= is_vra_video_streamon,
	.vidioc_streamoff		= is_vra_video_streamoff,
	.vidioc_enum_input		= is_vra_video_enum_input,
	.vidioc_g_input			= is_vra_video_g_input,
	.vidioc_s_input			= is_vra_video_s_input,
	.vidioc_s_ctrl			= is_vra_video_s_ctrl,
	.vidioc_g_ctrl			= is_vra_video_g_ctrl,
	.vidioc_s_ext_ctrls		= is_vra_video_s_ext_ctrl,
	.vidioc_g_ext_ctrls		= is_vra_video_g_ext_ctrl,
};

static int is_vra_queue_setup(struct vb2_queue *vbq,
	unsigned int *num_buffers, unsigned int *num_planes,
	unsigned int sizes[],
	struct device *alloc_devs[])
{
	int ret = 0;
	struct is_video_ctx *vctx;
	struct is_device_ischain *device;
	struct is_video *video;
	struct is_queue *queue;
#if defined(SECURE_CAMERA_FACE) && !defined(CONVERT_BUFFER_SECURE_TO_NON_SECURE)
	struct is_core *core =
		(struct is_core *)dev_get_drvdata(is_dev);
#endif

	FIMC_BUG(!vbq);
	FIMC_BUG(!vbq->drv_priv);
	vctx = vbq->drv_priv;

	FIMC_BUG(!GET_DEVICE(vctx));
	device = GET_DEVICE(vctx);

	FIMC_BUG(!GET_VIDEO(vctx));
	video = GET_VIDEO(vctx);

	FIMC_BUG(!GET_QUEUE(vctx));
	queue = GET_QUEUE(vctx);

	mdbgv_isp("%s\n", vctx, __func__);

#if defined(SECURE_CAMERA_FACE) && !defined(CONVERT_BUFFER_SECURE_TO_NON_SECURE)
	if (core->scenario == IS_SCENARIO_SECURE)
		set_bit(IS_QUEUE_NEED_TO_REMAP, &queue->state);
#endif

	set_bit(IS_QUEUE_NEED_TO_KMAP, &queue->state);

	ret = is_queue_setup(queue,
		video->alloc_ctx,
		num_planes,
		sizes,
		alloc_devs);
	if (ret)
		merr("is_queue_setup is fail(%d)", device, ret);

	return ret;
}

static int is_vra_start_streaming(struct vb2_queue *vbq,
	unsigned int count)
{
	int ret = 0;
	struct is_video_ctx *vctx;
	struct is_queue *queue;
	struct is_device_ischain *device;

	FIMC_BUG(!vbq);
	FIMC_BUG(!vbq->drv_priv);
	vctx = vbq->drv_priv;

	FIMC_BUG(!GET_DEVICE(vctx));
	device = GET_DEVICE(vctx);

	FIMC_BUG(!GET_QUEUE(vctx));
	queue = GET_QUEUE(vctx);

	mdbgv_vra("%s\n", vctx, __func__);

	ret = is_queue_start_streaming(queue, device);
	if (ret) {
		merr("is_queue_start_streaming is fail(%d)", device, ret);
		goto p_err;
	}

p_err:
	return ret;
}

static void is_vra_stop_streaming(struct vb2_queue *vbq)
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

	mdbgv_vra("%s\n", vctx, __func__);

	ret = is_queue_stop_streaming(queue, device);
	if (ret) {
		merr("is_queue_stop_streaming is fail(%d)", device, ret);
		return;
	}
}

static void is_vra_buffer_queue(struct vb2_buffer *vb)
{
	int ret = 0;
	struct is_video_ctx *vctx;
	struct is_queue *queue;
	struct is_device_ischain *device;

	FIMC_BUG_VOID(!vb);
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

	ret = is_ischain_vra_buffer_queue(device, queue, vb->index);
	if (ret) {
		merr("is_ischain_vra_buffer_queue is fail(%d)", device, ret);
		return;
	}
}

static void is_vra_buffer_finish(struct vb2_buffer *vb)
{
	int ret;
	struct is_video_ctx *vctx;
	struct is_device_ischain *device;

	FIMC_BUG_VOID(!vb);
	FIMC_BUG_VOID(!vb->vb2_queue->drv_priv);
	vctx = vb->vb2_queue->drv_priv;

	FIMC_BUG_VOID(!GET_DEVICE(vctx));
	device = GET_DEVICE(vctx);

	mvdbgs(3, "%s(%d)\n", vctx, &vctx->queue, __func__, vb->index);

	ret = is_ischain_vra_buffer_finish(device, vb->index);
	if (ret)
		merr("is_ischain_vra_buffer_finish is fail(%d)", device, ret);

	is_queue_buffer_finish(vb);
}

const struct vb2_ops is_vra_qops = {
	.queue_setup		= is_vra_queue_setup,
	.buf_init		= is_queue_buffer_init,
	.buf_cleanup		= is_queue_buffer_cleanup,
	.buf_prepare		= is_queue_buffer_prepare,
	.buf_queue		= is_vra_buffer_queue,
	.buf_finish		= is_vra_buffer_finish,
	.wait_prepare		= is_queue_wait_prepare,
	.wait_finish		= is_queue_wait_finish,
	.start_streaming	= is_vra_start_streaming,
	.stop_streaming		= is_vra_stop_streaming,
};

