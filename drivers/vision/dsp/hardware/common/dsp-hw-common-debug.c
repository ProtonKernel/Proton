// SPDX-License-Identifier: GPL-2.0
/*
 * Samsung Exynos SoC series dsp driver
 *
 * Copyright (c) 2020 Samsung Electronics Co., Ltd.
 *              http://www.samsung.com/
 */

#include <linux/debugfs.h>

#include "dsp-log.h"
#include "dsp-device.h"
#include "dsp-debug.h"
#include "dsp-hw-common-system.h"
#include "dsp-hw-common-debug.h"

static int dsp_hw_debug_dbg_mode_show(struct seq_file *file, void *unused)
{
	struct dsp_hw_debug *debug;
	struct dsp_system *sys;

	dsp_enter();
	debug = file->private;
	sys = &debug->dspdev->system;

	seq_printf(file, "DSP debug mode count:%u / status:%#x\n",
			DSP_DEBUG_MODE_NUM, sys->debug_mode);
	seq_puts(file, "status is checked through value of 1 << (id) bit\n");
	seq_puts(file, "Command to control DSP debug mode\n");
	seq_puts(file, " on : echo {mode id} 1 > /d/dsp/hardware/debug_mode\n");
	seq_puts(file, " off: echo {mode id} 0 > /d/dsp/hardware/debug_mode\n");
	dsp_leave();
	return 0;
}

static int dsp_hw_debug_dbg_mode_open(struct inode *inode,
		struct file *filp)
{
	return single_open(filp, dsp_hw_debug_dbg_mode_show, inode->i_private);
}

static ssize_t dsp_hw_debug_dbg_mode_write(struct file *filp,
		const char __user *user_buf, size_t count, loff_t *ppos)
{
	int ret;
	struct seq_file *file;
	struct dsp_hw_debug *debug;
	struct dsp_system *sys;
	char buf[30];
	ssize_t size;
	int mode, enable;
	unsigned int org;

	dsp_enter();
	file = filp->private_data;
	debug = file->private;
	sys = &debug->dspdev->system;

	size = simple_write_to_buffer(buf, sizeof(buf), ppos, user_buf, count);
	if (size <= 0) {
		ret = -EINVAL;
		dsp_err("Failed to get user parameter(%zd)\n", size);
		goto p_err;
	}
	buf[size - 1] = '\0';

	ret = sscanf(buf, "%d %d", &mode, &enable);
	if (ret != 2) {
		dsp_err("Failed to get bus parameter(%d)\n", ret);
		ret = -EINVAL;
		goto p_err;
	}

	if (mode > 31) {
		ret = -EINVAL;
		dsp_err("mode id(%d) is invalid(id:0 ~ 31)\n", mode);
		goto p_err;
	}

	org = sys->debug_mode;
	if (enable)
		sys->debug_mode |= (unsigned int)1 << mode;
	else
		sys->debug_mode &= (unsigned int)1 << mode;

	dsp_info("debug mode is set(%#x -> %#x)\n", org, sys->debug_mode);
	dsp_leave();
	return count;
p_err:
	return ret;
}

static const struct file_operations dsp_hw_debug_dbg_mode_fops = {
	.open		= dsp_hw_debug_dbg_mode_open,
	.read		= seq_read,
	.write		= dsp_hw_debug_dbg_mode_write,
	.llseek		= seq_lseek,
	.release	= single_release
};

int dsp_hw_common_debug_probe(struct dsp_device *dspdev)
{
	int ret;
	struct dsp_hw_debug *debug;

	dsp_enter();
	debug = &dspdev->system.debug;
	debug->dspdev = dspdev;

	debug->root = debugfs_create_dir("hardware", dspdev->debug.root);
	if (!debug->root) {
		ret = -EFAULT;
		dsp_err("Failed to create hw debug root file\n");
		goto p_err_root;
	}

	debug->debug_mode = debugfs_create_file("debug_mode", 0640, debug->root,
			debug, &dsp_hw_debug_dbg_mode_fops);
	if (!debug->sfr)
		dsp_warn("Failed to create debug_mode debugfs file\n");

	dsp_leave();
	return 0;
p_err_root:
	return ret;
}

void dsp_hw_common_debug_remove(struct dsp_hw_debug *debug)
{
	dsp_enter();
	debugfs_remove_recursive(debug->root);
	dsp_leave();
}
