// SPDX-License-Identifier: GPL-2.0
/*
 * Samsung Exynos SoC series dsp driver
 *
 * Copyright (c) 2019 Samsung Electronics Co., Ltd.
 *              http://www.samsung.com/
 */

#include <linux/slab.h>
#include <linux/uaccess.h>

#include "dsp-log.h"
#include "dsp-device.h"
#include "dsp-context.h"
#include "dsp-core.h"

static int dsp_open(struct inode *inode, struct file *file)
{
	int ret;
	struct miscdevice *miscdev;
	struct dsp_device *dspdev;
	struct dsp_core *core;
	struct dsp_context *dctx;

	dsp_enter();
	dsp_dbg("open start\n");
	miscdev = file->private_data;
	dspdev = dev_get_drvdata(miscdev->parent);
	core = &dspdev->core;

	ret = dsp_device_open(dspdev);
	if (ret)
		goto p_err_device;

	dctx = dsp_context_create(core);
	if (IS_ERR(dctx)) {
		ret = PTR_ERR(dctx);
		goto p_err_dctx;
	}

	file->private_data = dctx;

	dsp_info("dsp has been successfully opened\n");
	dsp_dbg("open end\n");
	dsp_leave();
	return 0;
p_err_dctx:
	dsp_device_close(core->dspdev);
p_err_device:
	return ret;
}

static int dsp_release(struct inode *inode, struct file *file)
{
	struct dsp_context *dctx;
	struct dsp_core *core;

	dsp_enter();
	dsp_dbg("release start\n");
	dctx = file->private_data;
	core = dctx->core;

	dsp_graph_manager_stop(&core->graph_manager, dctx->id);

	if (dctx->boot_count) {
		dsp_device_stop(core->dspdev, dctx->boot_count);
		dsp_graph_manager_close(&core->graph_manager, dctx->boot_count);
	} else if (dctx->sec_boot_count) {
		dsp_device_stop_secure(core->dspdev, dctx->sec_boot_count);
	}

	dsp_context_destroy(dctx);
	dsp_device_close(core->dspdev);

	dsp_info("dsp has been released\n");
	dsp_dbg("release end\n");
	dsp_leave();
	return 0;
}

static const struct file_operations dsp_file_ops = {
	.owner		= THIS_MODULE,
	.open		= dsp_open,
	.release	= dsp_release,
	.unlocked_ioctl = dsp_ioctl,
	.compat_ioctl	= dsp_compat_ioctl32,
};

/* Top-level data for debugging */
static struct dsp_miscdev *dsp_miscdev;

int dsp_core_probe(struct dsp_device *dspdev)
{
	int ret;
	struct dsp_core *core;

	dsp_enter();
	core = &dspdev->core;
	dsp_miscdev = &core->miscdev;
	core->dspdev = dspdev;

	core->ioctl_ops = dsp_context_get_ops();

	INIT_LIST_HEAD(&core->dctx_list);
	spin_lock_init(&core->dctx_slock);
	ret = dsp_util_bitmap_init(&core->context_map, "context_bitmap",
			DSP_CORE_MAX_CONTEXT);
	if (ret)
		goto p_err;

	dsp_miscdev->miscdev.minor = MISC_DYNAMIC_MINOR;
	dsp_miscdev->miscdev.name = DSP_DEV_NAME;
	dsp_miscdev->miscdev.fops = &dsp_file_ops;
	dsp_miscdev->miscdev.parent = dspdev->dev;

	ret = misc_register(&dsp_miscdev->miscdev);
	if (ret) {
		dsp_err("Failed to register miscdevice(%d)\n", ret);
		goto p_err_misc;
	}

	ret = dsp_graph_manager_probe(core);
	if (ret)
		goto p_err_graph;

	dsp_leave();
	return 0;
p_err_graph:
	misc_deregister(&dsp_miscdev->miscdev);
p_err_misc:
	dsp_util_bitmap_deinit(&core->context_map);
p_err:
	return ret;
}

void dsp_core_remove(struct dsp_core *core)
{
	dsp_enter();
	dsp_graph_manager_remove(&core->graph_manager);
	misc_deregister(&core->miscdev.miscdev);
	dsp_util_bitmap_deinit(&core->context_map);
	dsp_leave();
}
