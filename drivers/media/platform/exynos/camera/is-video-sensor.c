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

#include "is-core.h"
#include "is-device-sensor.h"
#include "is-cmd.h"
#include "is-err.h"
#include "is-video.h"
#include "is-resourcemgr.h"
#include "is-vender.h"

#ifdef CONFIG_LEDS_IRIS_IRLED_SUPPORT
extern int s2mpb02_ir_led_current(uint32_t current_value);
extern int s2mpb02_ir_led_pulse_width(uint32_t width);
extern int s2mpb02_ir_led_pulse_delay(uint32_t delay);
extern int s2mpb02_ir_led_max_time(uint32_t max_time);
#endif

const struct v4l2_file_operations is_ssx_video_fops;
const struct v4l2_ioctl_ops is_ssx_video_ioctl_ops;
const struct vb2_ops is_ssx_qops;

#define SYSREG_ISP_MIPIPHY_CON	0x144F1040
#define PMU_MIPI_PHY_M0S2_CONTROL	0x10480734

int is_ssx_video_probe(void *data)
{
	int ret = 0;
	struct is_device_sensor *device;
	struct is_video *video;
	char name[30];
	u32 device_id;

	FIMC_BUG(!data);

	device = (struct is_device_sensor *)data;
	device_id = device->device_id;
	video = &device->video;
	video->resourcemgr = device->resourcemgr;
	snprintf(name, sizeof(name), "%s%d", IS_VIDEO_SSX_NAME, device_id);

	if (!device->pdev) {
		err("pdev is NULL");
		ret = -EINVAL;
		goto p_err;
	}

	ret = is_video_probe(video,
		name,
		IS_VIDEO_SS0_NUM + device_id,
#ifdef CONFIG_USE_SENSOR_GROUP
		VFL_DIR_M2M,
#else
		VFL_DIR_RX,
#endif
		&device->mem,
		&device->v4l2_dev,
		&is_ssx_video_fops,
		&is_ssx_video_ioctl_ops);
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

static int is_ssx_video_open(struct file *file)
{
	int ret = 0;
	int ret_err = 0;
	struct is_video *video;
	struct is_video_ctx *vctx;
	struct is_device_sensor *device;
	struct is_resourcemgr *resourcemgr;
	char name[IS_STR_LEN];

	FIMC_BUG(!file);
	vctx = NULL;
	video = video_drvdata(file);
	device = container_of(video, struct is_device_sensor, video);
	resourcemgr = video->resourcemgr;
	if (!resourcemgr) {
		err("resourcemgr is NULL");
		ret = -EINVAL;
		goto err_resource_null;
	}

	if (device && device->reboot) {
		warn("[SS%d:V]%s: fail to open ssx_video_open() - reboot(%d)\n",
			device->device_id, __func__, device->reboot);
		ret = -EINVAL;
		goto err_reboot;
	}

	ret = is_resource_open(resourcemgr, video->id - IS_VIDEO_SS0_NUM, NULL);
	if (ret) {
		err("[SS%d:V] is_resource_open is fail(%d)", device->device_id, ret);
		goto err_resource_open;
	}

	snprintf(name, sizeof(name), "SS%d", device->device_id);
	ret = open_vctx(file, video, &vctx, device->device_id, ENTRY_SENSOR, name);
	if (ret) {
		err("[SS%d:V] open_vctx is fail(%d)", device->device_id, ret);
		goto err_vctx_open;
	}

	ret = is_video_open(vctx,
		device,
		VIDEO_SSX_READY_BUFFERS,
		video,
		&is_ssx_qops,
		&is_sensor_ops);
	if (ret) {
		err("[SS%d:V] is_video_open is fail(%d)", device->device_id, ret);
		goto err_video_open;
	}

	ret = is_sensor_open(device, vctx);
	if (ret) {
		err("[SS%d:V] is_ssx_open is fail(%d)", device->device_id, ret);
		goto err_ischain_open;
	}

	minfo("[SS%d:V] %s\n", device, device->device_id, __func__);

	return 0;

err_ischain_open:
	ret_err = is_video_close(vctx);
	if (ret_err)
		err("[SS%d:V] is_video_close is fail(%d)", device->device_id, ret_err);
err_video_open:
	ret_err = close_vctx(file, video, vctx);
	if (ret_err < 0)
		err("[SS%d:V] close_vctx is fail(%d)", device->device_id, ret_err);
err_vctx_open:
err_resource_open:
err_reboot:
err_resource_null:
	return ret;
}

static int is_ssx_video_close(struct file *file)
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

	ret = is_sensor_close(device);
	if (ret)
		merr("is_sensor_close is fail(%d)", device, ret);

	ret = is_video_close(vctx);
	if (ret)
		merr("is_video_close is fail(%d)", device, ret);

	refcount = close_vctx(file, video, vctx);
	if (refcount < 0)
		merr("close_vctx is fail(%d)", device, refcount);

	minfo("[SS%d:V] %s(%d):%d\n", device,
			GET_SSX_ID(video), __func__, refcount, ret);

	return ret;
}

static unsigned int is_ssx_video_poll(struct file *file,
	struct poll_table_struct *wait)
{
	u32 ret = 0;
	struct is_video_ctx *vctx;

	FIMC_BUG(!wait);
	FIMC_BUG(!file);
	FIMC_BUG(!file->private_data);
	vctx = file->private_data;

	ret = is_video_poll(file, vctx, wait);

	return ret;
}

static int is_ssx_video_mmap(struct file *file,
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

const struct v4l2_file_operations is_ssx_video_fops = {
	.owner		= THIS_MODULE,
	.open		= is_ssx_video_open,
	.release	= is_ssx_video_close,
	.poll		= is_ssx_video_poll,
	.unlocked_ioctl	= video_ioctl2,
	.mmap		= is_ssx_video_mmap,
};

/*
 * =============================================================================
 * Video Ioctl Opertation
 * =============================================================================
 */

static int is_ssx_video_querycap(struct file *file, void *fh,
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

static int is_ssx_video_get_format_mplane(struct file *file, void *fh,
						struct v4l2_format *format)
{
	/* Todo : add to get format code */
	return 0;
}

static int is_ssx_video_set_format_mplane(struct file *file, void *fh,
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

	mdbgv_sensor("%s\n", vctx, __func__);

	ret = is_video_set_format_mplane(file, vctx, format);
	if (ret) {
		merr("is_video_set_format_mplane is fail(%d)", device, ret);
		goto p_err;
	}

p_err:
	return ret;
}

static int is_ssx_video_reqbufs(struct file *file, void *priv,
	struct v4l2_requestbuffers *buf)
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

	mdbgv_sensor("%s(buffers : %d)\n", vctx, __func__, buf->count);

	ret = is_video_reqbufs(file, vctx, buf);
	if (ret)
		merr("is_video_reqbufs is fail(error %d)", device, ret);

	return ret;
}

static int is_ssx_video_querybuf(struct file *file, void *priv,
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

	mdbgv_sensor("%s\n", vctx, __func__);

	ret = is_video_querybuf(file, vctx, buf);
	if (ret)
		merr("is_video_querybuf is fail(%d)", device, ret);

	return ret;
}

static int is_ssx_video_qbuf(struct file *file, void *priv,
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

	mvdbgs(3, "%s\n", vctx, &vctx->queue, __func__);

	ret = CALL_VOPS(vctx, qbuf, buf);
	if (ret)
		merr("qbuf is fail(%d)", device, ret);

	return ret;
}

static int is_ssx_video_dqbuf(struct file *file, void *priv,
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
	if (ret) {
		if (!blocking || (ret != -EAGAIN))
			merr("dqbuf is fail(%d)", device, ret);
	}

	return ret;
}

static int is_ssx_video_prepare(struct file *file, void *prev,
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

	ret = is_video_prepare(file, vctx, buf);
	if (ret) {
		merr("is_video_prepare is fail(%d)", device, ret);
		goto p_err;
	}

p_err:
	minfo("[S%dS:V] %s(%d):%d\n", device,
		GET_SSX_ID(GET_VIDEO(vctx)),__func__, buf->index, ret);
	return ret;
}


static int is_ssx_video_streamon(struct file *file, void *priv,
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

	mdbgv_sensor("%s\n", vctx, __func__);

	ret = is_video_streamon(file, vctx, type);
	if (ret)
		merr("is_video_streamon is fail(%d)", device, ret);

	return ret;
}

static int is_ssx_video_streamoff(struct file *file, void *priv,
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

	mdbgv_sensor("%s\n", vctx, __func__);

	ret = is_video_streamoff(file, vctx, type);
	if (ret)
		merr("is_video_streamoff is fail(%d)", device, ret);

	return ret;
}

static int is_ssx_video_enum_input(struct file *file, void *priv,
	struct v4l2_input *input)
{
	/* Todo: add to enumerate input code */
	info("%s is calld\n", __func__);
	return 0;
}

static int is_ssx_video_g_input(struct file *file, void *priv,
	unsigned int *input)
{
	/* Todo: add to get input control code */
	return 0;
}

#ifdef CONFIG_USE_SENSOR_GROUP
static int is_ssx_video_s_input(struct file *file, void *priv,
	unsigned int input)
{
	int ret = 0;
	u32 scenario, stream, position, vindex, intype, leader;
	struct is_video_ctx *vctx;
	struct is_device_sensor *device;
	struct is_framemgr *framemgr;

	FIMC_BUG(!file);
	FIMC_BUG(!file->private_data);
	vctx = file->private_data;

	FIMC_BUG(!GET_DEVICE(vctx));
	device = GET_DEVICE(vctx);

	FIMC_BUG(!GET_FRAMEMGR(vctx));
	framemgr = GET_FRAMEMGR(vctx);

	scenario = (input & SENSOR_SCN_MASK) >> SENSOR_SCN_SHIFT;
	stream = (input & INPUT_STREAM_MASK) >> INPUT_STREAM_SHIFT;
	position = (input & INPUT_POSITION_MASK) >> INPUT_POSITION_SHIFT;
	vindex = (input & INPUT_VINDEX_MASK) >> INPUT_VINDEX_SHIFT;
	intype = (input & INPUT_INTYPE_MASK) >> INPUT_INTYPE_SHIFT;
	leader = (input & INPUT_LEADER_MASK) >> INPUT_LEADER_SHIFT;

	mdbgv_sensor("%s(input : %08X)[%d,%d,%d,%d,%d,%d]\n", vctx, __func__, input,
			scenario, stream, position, vindex, intype, leader);

	ret = is_video_s_input(file, vctx);
	if (ret) {
		merr("is_video_s_input is fail(%d)", device, ret);
		goto p_err;
	}

	ret = is_sensor_s_input(device, position, scenario, vindex, leader);
	if (ret) {
		merr("is_sensor_s_input is fail(%d)", device, ret);
		goto p_err;
	}

p_err:
	return ret;
}
#else /* CONFIG_USE_SENSOR_GROUP */
static int is_ssx_video_s_input(struct file *file, void *priv,
	unsigned int input)
{
	int ret = 0;
	u32 scenario;
	struct is_video_ctx *vctx;
	struct is_device_sensor *device;
	struct is_framemgr *framemgr;

	FIMC_BUG(!file);
	FIMC_BUG(!file->private_data);
	vctx = file->private_data;

	FIMC_BUG(!GET_DEVICE(vctx));
	device = GET_DEVICE(vctx);

	FIMC_BUG(!GET_FRAMEMGR(vctx));
	framemgr = GET_FRAMEMGR(vctx);

	mdbgv_sensor("%s(input : %08X)\n", vctx, __func__, input);

	scenario = (input & SENSOR_SCENARIO_MASK) >> SENSOR_SCENARIO_SHIFT;
	input = (input & SENSOR_MODULE_MASK) >> SENSOR_MODULE_SHIFT;

	ret = is_video_s_input(file, vctx);
	if (ret) {
		merr("is_video_s_input is fail(%d)", vctx, ret);
		goto p_err;
	}

	ret = is_sensor_s_input(device, input, scenario);
	if (ret) {
		merr("is_sensor_s_input is fail(%d)", device, ret);
		goto p_err;
	}

p_err:
	return ret;
}
#endif


static int is_ssx_video_check_3aa_state(struct is_device_sensor *device,  u32 next_id)
{
	int ret = 0;
	int i;
	struct is_core *core;
	struct exynos_platform_is *pdata;
	struct is_device_sensor *sensor;
	struct is_device_ischain *ischain;
	struct is_group *group_3aa;

	core = (struct is_core *)dev_get_drvdata(is_dev);
	if (!core) {
		merr("[SS%d] The core is NULL", device, device->instance);
		ret = -ENXIO;
		goto p_err;
	}

	pdata = core->pdata;
	if (!pdata) {
		merr("[SS%d] The pdata is NULL", device, device->instance);
		ret = -ENXIO;
		goto p_err;
	}

	if (next_id >= pdata->num_of_ip.taa) {
		merr("[SS%d] The next_id is too big.(next_id: %d)", device, device->instance, next_id);
		ret = -ENODEV;
		goto p_err;
	}

	if (test_bit(IS_SENSOR_FRONT_START, &device->state)) {
		merr("[SS%d] current sensor is not in stop state", device, device->instance);
		ret = -EBUSY;
		goto p_err;
	}

	for (i = 0; i < IS_SENSOR_COUNT; i++) {
		sensor = &core->sensor[i];
		if (test_bit(IS_SENSOR_FRONT_START, &sensor->state)) {
			ischain = sensor->ischain;
			if (!ischain)
				continue;

			group_3aa = &ischain->group_3aa;
			if (group_3aa->id == GROUP_ID_3AA0 + next_id) {
				merr("[SS%d] 3AA%d is busy", device, device->instance, next_id);
				ret = -EBUSY;
				goto p_err;
			}
		}
	}

p_err:
	return ret;
}

static int is_ssx_video_s_ctrl(struct file *file, void *priv,
	struct v4l2_control *ctrl)
{
	int ret = 0;
	struct is_video_ctx *vctx;
	struct is_device_sensor *device;

	FIMC_BUG(!ctrl);
	FIMC_BUG(!file);
	FIMC_BUG(!file->private_data);
	vctx = file->private_data;

	FIMC_BUG(!GET_DEVICE(vctx));
	device = GET_DEVICE(vctx);

	ret = is_vender_ssx_video_s_ctrl(ctrl, device);
	if (ret) {
		merr("is_vender_ssx_video_s_ctrl is fail(%d)", device, ret);
		goto p_err;
	}

	switch (ctrl->id) {
	case V4L2_CID_IS_S_STREAM:
		{
			u32 sstream, instant, noblock, standby;

			sstream = (ctrl->value & SENSOR_SSTREAM_MASK) >> SENSOR_SSTREAM_SHIFT;
			standby = (ctrl->value & SENSOR_USE_STANDBY_MASK) >> SENSOR_USE_STANDBY_SHIFT;
			instant = (ctrl->value & SENSOR_INSTANT_MASK) >> SENSOR_INSTANT_SHIFT;
			noblock = (ctrl->value & SENSOR_NOBLOCK_MASK) >> SENSOR_NOBLOCK_SHIFT;
			/*
			 * nonblock(0) : blocking command
			 * nonblock(1) : non-blocking command
			 */

			minfo(" Sensor Stream %s : (cnt:%d, noblk:%d, standby:%d)\n", device,
				sstream ? "On" : "Off", instant, noblock, standby);

			device->use_standby = standby;
			device->sstream = sstream;

			if (sstream == IS_ENABLE_STREAM) {
				ret = is_sensor_front_start(device, instant, noblock);
				if (ret) {
					merr("is_sensor_front_start is fail(%d)", device, ret);
					goto p_err;
				}
			} else {
				ret = is_sensor_front_stop(device, false);
				if (ret) {
					merr("is_sensor_front_stop is fail(%d)", device, ret);
					goto p_err;
				}
			}
		}
		break;
	case V4L2_CID_IS_CHANGE_CHAIN_PATH:	/* Change OTF HW path */
		ret = is_ssx_video_check_3aa_state(device, ctrl->value);
		if (ret) {
			merr("[SS%d] 3AA%d is in invalid state(%d)", device, device->instance,
				ctrl->value, ret);
			goto p_err;
		}

		ret = is_group_change_chain(device->groupmgr, &device->group_sensor, ctrl->value);
		if (ret)
			merr("is_group_change_chain is fail(%d)", device, ret);
		break;
	case V4L2_CID_IS_CHECK_CHAIN_STATE:
		ret = is_ssx_video_check_3aa_state(device, ctrl->value);
		if (ret) {
			merr("[SS%d] 3AA%d is in invalid state(%d)", device, device->instance,
				ctrl->value, ret);
			goto p_err;
		}
		break;
	/*
	 * gain boost: min_target_fps,  max_target_fps, scene_mode
	 */
	case V4L2_CID_IS_MIN_TARGET_FPS:
		minfo("%s: set min_target_fps(%d), state(0x%lx)\n", device, __func__, ctrl->value, device->state);

		device->min_target_fps = ctrl->value;
		break;
	case V4L2_CID_IS_MAX_TARGET_FPS:
		minfo("%s: set max_target_fps(%d), state(0x%lx)\n", device, __func__, ctrl->value, device->state);

		device->max_target_fps = ctrl->value;
		break;
	case V4L2_CID_SCENEMODE:
		if (test_bit(IS_SENSOR_FRONT_START, &device->state)) {
			err("failed to set scene_mode: %d - sensor stream on already\n",
					ctrl->value);
			ret = -EINVAL;
		} else {
			device->scene_mode = ctrl->value;
		}
		break;
	case V4L2_CID_SENSOR_SET_FRAME_RATE:
		if (is_sensor_s_frame_duration(device, ctrl->value)) {
			err("failed to set frame duration : %d\n - %d",
					ctrl->value, ret);
			ret = -EINVAL;
		}
		break;
	case V4L2_CID_SENSOR_SET_AE_TARGET:
		if (is_sensor_s_exposure_time(device, ctrl->value)) {
			err("failed to set exposure time : %d\n - %d",
					ctrl->value, ret);
			ret = -EINVAL;
		}
		break;
	case VENDER_S_CTRL:
		/* This s_ctrl is needed to skip, when the s_ctrl id was found. */
		break;
	case V4L2_CID_IS_S_SENSOR_SIZE:
		device->sensor_width = (ctrl->value & SENSOR_SIZE_WIDTH_MASK) >> SENSOR_SIZE_WIDTH_SHIFT;
		device->sensor_height = (ctrl->value & SENSOR_SIZE_HEIGHT_MASK) >> SENSOR_SIZE_HEIGHT_SHIFT;
		minfo("sensor size : %d x %d (0x%08X)", device,
				device->sensor_width, device->sensor_height, ctrl->value);
		break;
	case V4L2_CID_IS_FORCE_DONE:
		set_bit(IS_GROUP_REQUEST_FSTOP, &device->group_sensor.state);
		break;
	case V4L2_CID_IS_END_OF_STREAM:
	case V4L2_CID_IS_CAMERA_TYPE:
	case V4L2_CID_IS_SET_SETFILE:
	case V4L2_CID_IS_HAL_VERSION:
	case V4L2_CID_IS_DEBUG_DUMP:
	case V4L2_CID_IS_DVFS_CLUSTER0:
	case V4L2_CID_IS_DVFS_CLUSTER1:
	case V4L2_CID_IS_DVFS_CLUSTER2:
	case V4L2_CID_IS_DEBUG_SYNC_LOG:
	case V4L2_CID_HFLIP:
	case V4L2_CID_VFLIP:
	case V4L2_CID_IS_INTENT:
	case V4L2_CID_IS_CAPTURE_EXPOSURETIME:
	case V4L2_CID_IS_TRANSIENT_ACTION:
	case V4L2_CID_IS_FORCE_FLASH_MODE:
	case V4L2_CID_IS_OPENING_HINT:
	case V4L2_CID_IS_CLOSING_HINT:
	case V4L2_CID_IS_S_LLC_CONFIG:
		ret = is_video_s_ctrl(file, vctx, ctrl);
		if (ret) {
			merr("is_video_s_ctrl is fail(%d)", device, ret);
			goto p_err;
		}
		break;
	case V4L2_CID_IS_SECURE_MODE:
	{
		u32 scenario;
		struct is_core *core;

		scenario = (ctrl->value & IS_SCENARIO_MASK) >> IS_SCENARIO_SHIFT;
		core = (struct is_core *)dev_get_drvdata(is_dev);
		if (core && scenario == IS_SCENARIO_SECURE) {
			mvinfo("[SCENE_MODE] SECURE scenario(%d) was detected\n",
				device, GET_VIDEO(vctx), scenario);
			device->ex_scenario = core->scenario = scenario;
		}
		break;
	}
	case V4L2_CID_SENSOR_SET_GAIN:
		if (is_sensor_s_again(device, ctrl->value)) {
			err("failed to set gain : %d\n - %d",
				ctrl->value, ret);
			ret = -EINVAL;
		}
		break;
	case V4L2_CID_SENSOR_SET_SHUTTER:
		if (is_sensor_s_shutterspeed(device, ctrl->value)) {
			err("failed to set shutter speed : %d\n - %d",
				ctrl->value, ret);
			ret = -EINVAL;
		}
		break;
#ifdef CONFIG_LEDS_IRIS_IRLED_SUPPORT
	case V4L2_CID_IRLED_SET_WIDTH:
		ret = s2mpb02_ir_led_pulse_width(ctrl->value);
		if (ret < 0) {
			err("failed to set irled pulse width : %d\n - %d", ctrl->value, ret);
			ret = -EINVAL;
		}
		break;
	case V4L2_CID_IRLED_SET_DELAY:
		ret = s2mpb02_ir_led_pulse_delay(ctrl->value);
		if (ret < 0) {
			err("failed to set irled pulse delay : %d\n - %d", ctrl->value, ret);
			ret = -EINVAL;
		}
		break;
	case V4L2_CID_IRLED_SET_CURRENT:
		ret = s2mpb02_ir_led_current(ctrl->value);
		if (ret < 0) {
			err("failed to set irled current : %d\n - %d", ctrl->value, ret);
			ret = -EINVAL;
		}
		break;
	case V4L2_CID_IRLED_SET_ONTIME:
		ret = s2mpb02_ir_led_max_time(ctrl->value);
		if (ret < 0) {
			err("failed to set irled max time : %d\n - %d", ctrl->value, ret);
			ret = -EINVAL;
		}
		break;
#endif
	case V4L2_CID_IS_S_BNS:
		break;
	case V4L2_CID_SENSOR_SET_EXTENDEDMODE:
		device->ex_mode = (ctrl->value & 0x1f);
		device->ex_mode_option = (ctrl->value & 0xe0);
		break;
	case V4L2_CID_IS_S_STANDBY_OFF:
		/*
		 * User must set before qbuf for next stream on in standby on state.
		 * If it is not cleared, all qbuf buffer is returned with unprocessed.
		 */
		clear_bit(IS_GROUP_STANDBY, &device->group_sensor.state);
		minfo("Clear STANDBY state", device);
		break;
	default:
		ret = is_sensor_s_ctrl(device, ctrl);
		if (ret) {
			err("invalid ioctl(0x%08X) is requested", ctrl->id);
			ret = -EINVAL;
			goto p_err;
		}
		break;
	}

p_err:
	return ret;
}

static int is_ssx_video_s_ext_ctrls(struct file *file, void *priv,
				 struct v4l2_ext_controls *ctrls)
{
	int ret = 0;
	struct is_video_ctx *vctx;
	struct is_device_sensor *device;

	FIMC_BUG(!ctrls);
	FIMC_BUG(!file);
	FIMC_BUG(!file->private_data);
	vctx = file->private_data;

	FIMC_BUG(!GET_DEVICE(vctx));
	device = GET_DEVICE(vctx);

	ret = is_sensor_s_ext_ctrls(device, ctrls);
	if (ret) {
		merr("s_ext_ctrl is fail(%d)", device, ret);
		goto p_err;
	}

p_err:
	return ret;
}

static int is_ssx_video_g_ctrl(struct file *file, void *priv,
	struct v4l2_control *ctrl)
{
	int ret = 0;
	struct is_video_ctx *vctx;
	struct is_device_sensor *device;

	FIMC_BUG(!ctrl);
	FIMC_BUG(!file);
	FIMC_BUG(!file->private_data);
	vctx = file->private_data;

	FIMC_BUG(!GET_DEVICE(vctx));
	device = GET_DEVICE(vctx);

	ret = is_vender_ssx_video_g_ctrl(ctrl, device);
	if (ret) {
		merr("is_vender_ssx_video_g_ctrl is fail(%d)", device, ret);
		goto p_err;
	}

	switch (ctrl->id) {
	case V4L2_CID_IS_G_STREAM:
		if (device->instant_ret)
			ctrl->value = device->instant_ret;
		else
			ctrl->value = (test_bit(IS_SENSOR_FRONT_START, &device->state) ?
				IS_ENABLE_STREAM : IS_DISABLE_STREAM);
		break;
	case V4L2_CID_IS_G_BNS_SIZE:
		{
			u32 width, height;

			width = is_sensor_g_bns_width(device);
			height = is_sensor_g_bns_height(device);

			ctrl->value = (width << 16) | height;
		}
		break;
	case V4L2_CID_IS_G_DTPSTATUS:
		if (test_bit(IS_SENSOR_FRONT_DTP_STOP, &device->state))
			ctrl->value = 1;
		else
			ctrl->value = 0;
		/* additional set for ESD notification */
		if (test_bit(IS_SENSOR_ESD_RECOVERY, &device->state))
			ctrl->value += 2;
		break;
	case V4L2_CID_IS_G_MIPI_ERR:
		ctrl->value = is_sensor_g_csis_error(device);
		break;
	case V4L2_CID_IS_G_ACTIVE_CAMERA:
		if (test_bit(IS_SENSOR_S_INPUT, &device->state))
			ctrl->value = device->sensor_id;
		else
			ctrl->value = 0;
		break;
	case VENDER_G_CTRL:
		/* This s_ctrl is needed to skip, when the s_ctrl id was found. */
		break;
	default:
		ret = is_sensor_g_ctrl(device, ctrl);
		if (ret) {
			err("invalid ioctl(0x%08X) is requested", ctrl->id);
			ret = -EINVAL;
			goto p_err;
		}
		break;
	}

p_err:
	return ret;
}

static int is_ssx_video_g_parm(struct file *file, void *priv,
	struct v4l2_streamparm *parm)
{
	struct is_video_ctx *vctx;
	struct is_device_sensor *device;
	struct v4l2_captureparm *cp;
	struct v4l2_fract *tfp;

	FIMC_BUG(!parm);
	FIMC_BUG(!file);
	FIMC_BUG(!file->private_data);
	vctx = file->private_data;

	FIMC_BUG(!GET_DEVICE(vctx));
	device = GET_DEVICE(vctx);

	cp = &parm->parm.capture;
	tfp = &cp->timeperframe;
	if (parm->type != V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE)
		return -EINVAL;

	cp->capability |= V4L2_CAP_TIMEPERFRAME;
	tfp->numerator = 1;
	tfp->denominator = device->image.framerate;

	return 0;
}

static int is_ssx_video_s_parm(struct file *file, void *priv,
	struct v4l2_streamparm *parm)
{
	int ret = 0;
	struct is_video_ctx *vctx;
	struct is_device_sensor *device;

	FIMC_BUG(!parm);
	FIMC_BUG(!file);
	FIMC_BUG(!file->private_data);
	vctx = file->private_data;

	FIMC_BUG(!GET_DEVICE(vctx));
	device = GET_DEVICE(vctx);

	mdbgv_sensor("%s\n", vctx, __func__);

	device = vctx->device;
	if (!device) {
		err("device is NULL");
		ret = -EINVAL;
		goto p_err;
	}

	ret = is_sensor_s_framerate(device, parm);
	if (ret) {
		merr("is_ssx_s_framerate is fail(%d)", device, ret);
		goto p_err;
	}

p_err:
	return ret;
}

const struct v4l2_ioctl_ops is_ssx_video_ioctl_ops = {
	.vidioc_querycap		= is_ssx_video_querycap,
	.vidioc_g_fmt_vid_out_mplane	= is_ssx_video_get_format_mplane,
	.vidioc_g_fmt_vid_cap_mplane	= is_ssx_video_get_format_mplane,
	.vidioc_s_fmt_vid_out_mplane	= is_ssx_video_set_format_mplane,
	.vidioc_s_fmt_vid_cap_mplane	= is_ssx_video_set_format_mplane,
	.vidioc_reqbufs			= is_ssx_video_reqbufs,
	.vidioc_querybuf		= is_ssx_video_querybuf,
	.vidioc_qbuf			= is_ssx_video_qbuf,
	.vidioc_dqbuf			= is_ssx_video_dqbuf,
	.vidioc_prepare_buf		= is_ssx_video_prepare,
	.vidioc_streamon		= is_ssx_video_streamon,
	.vidioc_streamoff		= is_ssx_video_streamoff,
	.vidioc_enum_input		= is_ssx_video_enum_input,
	.vidioc_g_input			= is_ssx_video_g_input,
	.vidioc_s_input			= is_ssx_video_s_input,
	.vidioc_s_ctrl			= is_ssx_video_s_ctrl,
	.vidioc_s_ext_ctrls		= is_ssx_video_s_ext_ctrls,
	.vidioc_g_ctrl			= is_ssx_video_g_ctrl,
	.vidioc_g_parm			= is_ssx_video_g_parm,
	.vidioc_s_parm			= is_ssx_video_s_parm,
};

static int is_ssx_queue_setup(struct vb2_queue *vbq,
	unsigned int *num_buffers, unsigned int *num_planes,
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

	mdbgv_sensor("%s\n", vctx, __func__);

	ret = is_queue_setup(queue,
		video->alloc_ctx,
		num_planes,
		sizes,
		alloc_devs);
	if (ret)
		merr("is_queue_setup is fail(%d)", device, ret);

	return ret;
}

static int is_ssx_start_streaming(struct vb2_queue *vbq,
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
	device = GET_DEVICE(vctx);

	FIMC_BUG(!GET_QUEUE(vctx));
	queue = GET_QUEUE(vctx);

	mdbgv_sensor("%s\n", vctx, __func__);

	ret = is_queue_start_streaming(queue, device);
	if (ret) {
		merr("is_queue_start_streaming is fail(%d)", device, ret);
		goto p_err;
	}

p_err:
	return ret;
}

static void is_ssx_stop_streaming(struct vb2_queue *vbq)
{
	int ret = 0;
	struct is_video_ctx *vctx;
	struct is_queue *queue;
	struct is_device_sensor *device;

	FIMC_BUG_VOID(!vbq);
	FIMC_BUG_VOID(!vbq->drv_priv);
	vctx = vbq->drv_priv;

	FIMC_BUG_VOID(!GET_DEVICE(vctx));
	device = GET_DEVICE(vctx);

	FIMC_BUG_VOID(!GET_QUEUE(vctx));
	queue = GET_QUEUE(vctx);

	mdbgv_sensor("%s\n", vctx, __func__);

	ret = is_queue_stop_streaming(queue, device);
	if (ret) {
		merr("is_queue_stop_streaming is fail(%d)", device, ret);
		return;
	}

}

static void is_ssx_buffer_queue(struct vb2_buffer *vb)
{
	int ret = 0;
	struct is_video_ctx *vctx;
	struct is_queue *queue;
	struct is_device_sensor *device;

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

	ret = is_sensor_buffer_queue(device, queue, vb->index);
	if (ret) {
		merr("is_sensor_buffer_queue is fail(%d)", device, ret);
		return;
	}
}

static void is_ssx_buffer_finish(struct vb2_buffer *vb)
{
	int ret;
	struct is_video_ctx *vctx;
	struct is_device_sensor *device;

	FIMC_BUG_VOID(!vb);
	FIMC_BUG_VOID(!vb->vb2_queue->drv_priv);
	vctx = vb->vb2_queue->drv_priv;

	FIMC_BUG_VOID(!GET_DEVICE(vctx));
	device = GET_DEVICE(vctx);

	mvdbgs(3, "%s(%d)\n", vctx, &vctx->queue, __func__, vb->index);

	ret = is_sensor_buffer_finish(device, vb->index);
	if (ret)
		merr("is_sensor_buffer_finish is fail(%d)", device, ret);

	is_queue_buffer_finish(vb);
}

const struct vb2_ops is_ssx_qops = {
	.queue_setup		= is_ssx_queue_setup,
	.buf_init		= is_queue_buffer_init,
	.buf_cleanup		= is_queue_buffer_cleanup,
	.buf_prepare		= is_queue_buffer_prepare,
	.buf_queue		= is_ssx_buffer_queue,
	.buf_finish		= is_ssx_buffer_finish,
	.wait_prepare		= is_queue_wait_prepare,
	.wait_finish		= is_queue_wait_finish,
	.start_streaming	= is_ssx_start_streaming,
	.stop_streaming		= is_ssx_stop_streaming,
};
