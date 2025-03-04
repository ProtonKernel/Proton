/*
 * linux/drivers/video/fbdev/exynos/mafpc/mafpc_drv.c
 *
 * Source file for AOD Driver
 *
 * Copyright (c) 2016 Samsung Electronics
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */


#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/io.h>

#include "../panel_debug.h"
#include "../panel_drv.h"
#include "mafpc_drv.h"

#ifdef PANEL_PR_TAG
#undef PANEL_PR_TAG
#define PANEL_PR_TAG	"mafpc"
#endif


static int abc_fops_open(struct inode *inode, struct file *file)
{
	int ret = 0;

	struct miscdevice *miscdev = file->private_data;
	struct mafpc_device *mafpc = container_of(miscdev, struct mafpc_device, miscdev); 
 
	file->private_data = mafpc;

	return ret;
}


static ssize_t abc_fops_write(struct file *file, const char __user *buf,
		  size_t count, loff_t *ppos)
{
	char header;
	bool scale_factor = false;
	struct mafpc_device *mafpc = file->private_data;
	int ctrl_sz, img_sz, scale_sz;
	int cp_offset;

	ctrl_sz = mafpc->ctrl_cmd_len;
	img_sz = mafpc->comp_img_len;
	scale_sz = mafpc->scale_len;

	if ((!mafpc->comp_img_buf) || (!mafpc->scale_buf)) {
		panel_err("invalid buffer");
		goto exit_write;
	}

	if (count == (MAFPC_HEADER_SIZE + ctrl_sz + img_sz)) {
		panel_info("Write size: %ld, without scale factor\n", count);
	} else if (count == (MAFPC_HEADER_SIZE + ctrl_sz + img_sz + scale_sz)) {
		panel_info("Write size: %ld, with scale factor\n", count);
		scale_factor = true;
	} else {
		panel_err("invalid count: %d\n", count);
		goto exit_write;
	}

	if (copy_from_user(&header, buf, MAFPC_HEADER_SIZE)) {
		panel_err("failed to get user's header\n");
		goto exit_write;
	}
	cp_offset = MAFPC_HEADER_SIZE;

	panel_info("header : %c\n", header);
	if (header != MAFPC_HEADER) {
		panel_err("wrong header : %c\n", header);
		goto exit_write;
	}

	if (copy_from_user(mafpc->ctrl_cmd, buf + cp_offset, mafpc->ctrl_cmd_len)) {
		panel_err("failed to get user's header\n");
		goto exit_write;
	}
	cp_offset += mafpc->ctrl_cmd_len;

	print_hex_dump(KERN_ERR, "", DUMP_PREFIX_ADDRESS, 32, 4,
		mafpc->ctrl_cmd, mafpc->ctrl_cmd_len, false);

	if (copy_from_user(mafpc->comp_img_buf, buf + cp_offset, mafpc->comp_img_len)) {
		panel_err("failed to get comp img\n");
		goto exit_write;
	}
	cp_offset += mafpc->comp_img_len;

	if (scale_factor) {
		if (copy_from_user(mafpc->scale_buf, buf + cp_offset, mafpc->scale_len)) {
			panel_err("failed to get comp img\n");
			goto exit_write;
		}
		print_hex_dump(KERN_ERR, "", DUMP_PREFIX_ADDRESS, 32, 4,
			mafpc->scale_buf, mafpc->scale_len, false);
	}
	mafpc->written |= MAFPC_UPDATED_FROM_SVC;

exit_write:
	return count;
}

static int mafpc_instant_on(struct mafpc_device *mafpc)
{
	int ret = 0;
	struct panel_device *panel = mafpc->panel;

	if (panel == NULL) {
		panel_err("panel is null\n");
		return -EINVAL;
	}

	mafpc->req_update = true;
	mutex_lock(&mafpc->mafpc_lock);

	if (!IS_PANEL_ACTIVE(panel)) {
		panel_err("panel is not active\n");
		goto exit_write;
	}

	if (panel->state.cur_state == PANEL_STATE_ALPM) {
		panel_err("gct not supported on LPM\n");
		goto exit_write;
	}

/* mAFPC Instant_on consists of two stage. 
  1. Send compensation image for mAFPC to DDI, as soon as transmitted the instant_on ioctl.
  2. Send instant_on command to DDI, after frame done */

	panel_err("++ PANEL_MAFPC_IMG_SEQ ++\n");
	ret = panel_do_seqtbl_by_index(panel, PANEL_MAFPC_IMG_SEQ);
	if (unlikely(ret < 0)) {
		panel_err("failed to write init seqtbl\n");
		goto exit_write;
	}

	mafpc->written |= MAFPC_UPDATED_TO_DEV;
	mafpc->req_instant_on = true;

exit_write:
	panel_err("-- PANEL_MAFPC_IMG_SEQ -- \n");
	mutex_unlock(&mafpc->mafpc_lock);
	mafpc->req_update = false;
	return ret;

}


static int mafpc_instant_off(struct mafpc_device *mafpc)
{
	int ret = 0;
	struct panel_device *panel = mafpc->panel;

	mutex_lock(&mafpc->mafpc_lock);

	if (!IS_PANEL_ACTIVE(panel)) {
		panel_err("panel is not active\n");
		goto err;
	}

	if (panel->state.cur_state == PANEL_STATE_ALPM) {
		panel_err("gct not supported on LPM\n");
		goto err;
	}

	ret = panel_do_seqtbl_by_index(panel, PANEL_MAFPC_OFF_SEQ);
	if (unlikely(ret < 0)) {
		panel_err("failed to write init seqtbl\n");
	}

err:
	mutex_unlock(&mafpc->mafpc_lock);
	return ret;
}


static long abc_fops_ioctl(struct file *file, unsigned int cmd, unsigned long arg)
{
	int ret = 0;
	struct mafpc_device *mafpc = file->private_data;

	switch(cmd) {
		case IOCTL_MAFPC_ON:
			panel_info("mAFPC on\n");
			mafpc->enable = true;
			break;
		case IOCTL_MAFPC_ON_INSTANT:
			panel_info("mAFPC on instantly On\n");
			mafpc->enable = true;
			ret = mafpc_instant_on(mafpc);
			if (ret)
				panel_info("failed to instant on\n");
			break;
		case IOCTL_MAFPC_OFF:
			panel_info("mAFPC off\n");
			mafpc->enable = false;
			break;
		case IOCTL_MAFPC_OFF_INSTANT:
			panel_info("mAFPC off instantly\n");
			mafpc->enable = false;
			ret = mafpc_instant_off(mafpc);
			if (ret)
				panel_info("failed to instant off\n");
			break;
		default:
			panel_info("Invalid Command\n");
			break;
	}

	return ret;
}


static int abc_fops_release(struct inode *inode, struct file *file)
{
	int ret = 0;

	panel_info("was called\n");
	
	return ret;
}

static const struct file_operations abc_drv_fops = {
	.owner = THIS_MODULE,
	.open = abc_fops_open,
	.write = abc_fops_write,
	.unlocked_ioctl = abc_fops_ioctl,
	.release = abc_fops_release,
};

#if 0

static int transmit_mafpc_data(struct mafpc_device *mafpc)
{
	int ret = 0;
	s64 time_diff;
	ktime_t timestamp;
	struct panel_device *panel = mafpc->panel;

	timestamp = ktime_get();
#if 0
	decon_systrace(get_decon_drvdata(0), 'C', "mafpc", 1);

	ret = panel_do_seqtbl_by_index(panel, PANEL_MAFPC_IMG_SEQ);
	if (unlikely(ret < 0)) {
		panel_err("failed to write init seqtbl\n");
	}
#endif

#if 0
	ret = panel_do_seqtbl_by_index(panel, PANEL_MAFPC_ON_SEQ);
	if (unlikely(ret < 0)) {
		panel_err("failed to write init seqtbl\n");
	}

	decon_systrace(get_decon_drvdata(0), 'C', "mafpc", 0);
#endif	

	time_diff = ktime_to_us(ktime_sub(ktime_get(), timestamp));
	panel->mafpc_write_time = time_diff;
	panel_info("time for mAFPC : %llu\n", time_diff);

	return ret;
}



static int mafpc_thread(void *data)
{
	int ret;
	struct mafpc_device *mafpc = data;

	while (!kthread_should_stop()) {	
		ret = wait_event_interruptible(mafpc->wq_wait, mafpc->req_update);
		mutex_lock(&mafpc->mafpc_lock);
		panel_info("was called\n");
		transmit_mafpc_data(mafpc);
		mafpc->req_update = false;
		mutex_unlock(&mafpc->mafpc_lock);
	}
	return 0;
}


static int mafpc_create_thread(struct mafpc_device *mafpc)
{
	init_waitqueue_head(&mafpc->wq_wait);

	mafpc->thread = kthread_run(mafpc_thread, mafpc, "mafpc-thread");
	if (IS_ERR_OR_NULL(mafpc->thread)) {
		panel_err("failed to create mafpc thread\n");
		return PTR_ERR(mafpc->thread);
	}

	return 0;
}
#endif

static int mafpc_v4l2_probe(struct mafpc_device *mafpc, void *arg)
{
	int ret = 0;
	struct mafpc_info *info = (struct mafpc_info *)arg;

	if (unlikely(!info)) {
		panel_err("got null mafpc info\n");
		ret = -EINVAL;
		goto err_v4l2_probe;
	}

	if (!info->abc_img) {
		panel_err("MCD:ABC: Can't found abc image\n");
		ret = -EINVAL;
		goto err_v4l2_probe;
	}

	if (!info->abc_crc) {
		panel_err("MCD:ABC: Can't found abc's crc value\n");
		ret = -EINVAL;
		goto err_v4l2_probe;
	}

	if (!info->abc_scale_factor) {
		panel_err("MCD:ABC: Can't found abc's scale factor\n");
		ret = -EINVAL;
		goto err_v4l2_probe;
	}

	if (!info->abc_scale_map_tbl) {
		panel_err("MCD:ABC: Can't found abc's scale br map\n");
		ret = -EINVAL;
		goto err_v4l2_probe;
	}

	mafpc->comp_img_buf = info->abc_img;
	mafpc->comp_img_len = info->abc_img_len;
	panel_info("MCD:ABC: ABC Image Size : %d\n", mafpc->comp_img_len);

	mafpc->scale_buf = info->abc_scale_factor;
	mafpc->scale_len = info->abc_scale_factor_len;
	panel_info("MCD:ABC: ABC's Scale Factor Size : %d\n", mafpc->scale_len);

	mafpc->comp_crc_buf = info->abc_crc;
	mafpc->comp_crc_len = info->abc_crc_len;

	mafpc->scale_map_br_tbl = info->abc_scale_map_tbl;
	mafpc->scale_map_br_tbl_len = info->abc_scale_map_tbl_len;

	mafpc->ctrl_cmd_len = info->abc_ctrl_cmd_len;
	return ret;

err_v4l2_probe:
	return ret;
}



static int mafpc_v4l2_frame_done(struct mafpc_device *mafpc)
{
	int ret = 0;
	struct panel_device *panel = mafpc->panel;

	ret = panel_do_seqtbl_by_index(panel, PANEL_MAFPC_ON_SEQ);
	if (unlikely(ret < 0)) {
		panel_err("failed to write init seqtbl\n");
	}

	mafpc->req_instant_on = false;

	return ret;
}


static long mafpc_core_ioctl(struct v4l2_subdev *sd, unsigned int cmd, void *arg)
{
	int ret = 0;
	struct mafpc_device *mafpc = container_of(sd, struct mafpc_device, sd);

	switch(cmd) {
		case V4L2_IOCTL_PROBE_ABC:
			panel_info("MCD:ABC: v4l2_ioctl: PROBE_ABC\n");
			ret = mafpc_v4l2_probe(mafpc, arg);
			break;

		case V4L2_IOCTL_MAFPC_GET_INFO:
			v4l2_set_subdev_hostdata(sd, mafpc);
			break;

		case V4L2_IOCTL_MAFPC_PANEL_INIT:
			panel_info("MCD:ABC: v4l2_ioctl: V4L2_IOCTL_MAFPC_GET_INIT\n");
			mafpc->written |= MAFPC_UPDATED_TO_DEV;
			break;

		case V4L2_IOCTL_MAFPC_PANEL_EXIT:
			panel_info("MCD:ABC: v4l2_ioctl: V4L2_IOCTL_MAFPC_GET_EXIT\n");
			mafpc->written &= ~(MAFPC_UPDATED_TO_DEV);
			break;

		case V4L2_IOCL_MAFPC_FRAME_DONE:
			if (mafpc->req_instant_on) {
				panel_info("MCD:ABC: v4l2_ioctl: V4L2_IOCL_MAFPC_FRAME_DONE\n");
				ret = mafpc_v4l2_frame_done(mafpc);
			}
			break;
		default:
			panel_err("invalid cmd\n");
	}

	return ret;
}

static const struct v4l2_subdev_core_ops mafpc_v4l2_sd_core_ops = {
	.ioctl = mafpc_core_ioctl,
};

static const struct v4l2_subdev_ops mafpc_subdev_ops = {
	.core = &mafpc_v4l2_sd_core_ops,
};


static void mafpc_init_v4l2_subdev(struct mafpc_device *mafpc)
{
	struct v4l2_subdev *sd = &mafpc->sd;

	v4l2_subdev_init(sd, &mafpc_subdev_ops);
	sd->owner = THIS_MODULE;
	sd->grp_id = 0;
	snprintf(sd->name, sizeof(sd->name), "%s.%d", MAFPC_V4L2_DEV_NAME, mafpc->id);
	v4l2_set_subdevdata(sd, mafpc);
}

static int mafpc_probe(struct platform_device *pdev)
{
	int ret = 0;
	struct device *dev = &pdev->dev;
	struct mafpc_device *mafpc = NULL;

	mafpc = devm_kzalloc(dev, sizeof(struct mafpc_device), GFP_KERNEL);
	if (!mafpc) {
		panel_err("failed to allocate mafpc device.\n");
		ret = -ENOMEM;
		goto probe_err;
	}
	
	mafpc->dev = dev;
	mafpc->miscdev.minor = MISC_DYNAMIC_MINOR;
	mafpc->miscdev.fops = &abc_drv_fops;
	mafpc->miscdev.name = MAFPC_DEV_NAME;
	mafpc->req_instant_on = false;
	mafpc->id = 0;

	platform_set_drvdata(pdev, mafpc);

	mutex_init(&mafpc->mafpc_lock);

	ret = misc_register(&mafpc->miscdev);
	if (ret) {
		panel_err("failed to register mafpc drv\n");
		goto probe_err;
	}

	mafpc_init_v4l2_subdev(mafpc);
	panel_info("MCD:ABC: probed done\n");
probe_err:
	return ret;
}


static const struct of_device_id mafpc_drv_of_match_table[] = {
	{ .compatible = "samsung,panel-mafpc", },
	{},
};

MODULE_DEVICE_TABLE(of, mafpc_drv_of_match_table);

struct platform_driver mafpc_driver = {
	.probe = mafpc_probe,
	.driver = {
		.name = MAFPC_DEV_NAME,
		.owner = THIS_MODULE,
		.of_match_table = of_match_ptr(mafpc_drv_of_match_table),
	}
};

MODULE_AUTHOR("<minwoo7945.kim@samsung.com>");
MODULE_LICENSE("GPL");
