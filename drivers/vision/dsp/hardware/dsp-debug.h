/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Samsung Exynos SoC series dsp driver
 *
 * Copyright (c) 2019 Samsung Electronics Co., Ltd.
 *              http://www.samsung.com/
 */

#ifndef __HW_DSP_DEBUG_H__
#define __HW_DSP_DEBUG_H__

#include "dsp-util.h"

struct dsp_device;
struct dsp_hw_debug;

enum dsp_hw_debug_dbg_mode {
	DSP_DEBUG_MODE_BOOT_TIMEOUT_PANIC,
	DSP_DEBUG_MODE_TASK_TIMEOUT_PANIC,
	DSP_DEBUG_MODE_RESET_TIMEOUT_PANIC,
	DSP_DEBUG_MODE_NUM,
};

struct dsp_hw_debug_log {
	struct timer_list		timer;
	struct dsp_util_queue		*queue;
};

struct dsp_hw_debug_ops {
	void (*log_flush)(struct dsp_hw_debug *debug);
	int (*log_start)(struct dsp_hw_debug *debug);
	int (*log_stop)(struct dsp_hw_debug *debug);

	int (*open)(struct dsp_hw_debug *debug);
	int (*close)(struct dsp_hw_debug *debug);
	int (*probe)(struct dsp_device *dspdev);
	void (*remove)(struct dsp_hw_debug *debug);
};

struct dsp_hw_debug {
	struct dentry			*root;
	struct dentry			*power;
	struct dentry			*clk;
	struct dentry			*devfreq;
	struct dentry			*sfr;
	struct dentry			*mem;
	struct dentry			*fw_log;
	struct dentry			*wait_ctrl;
	struct dentry			*layer_range;
	struct dentry			*mailbox;
	struct dentry			*userdefined;
	struct dentry			*dump_value;
	struct dentry			*firmware_mode;
	struct dentry			*bus;
	struct dentry			*governor;
	struct dentry			*debug_mode;
	void				*sub_data;

	struct dsp_hw_debug_log		*log;
	struct dsp_device		*dspdev;
	const struct dsp_hw_debug_ops	*ops;
};

int dsp_hw_debug_set_ops(struct dsp_hw_debug *debug, unsigned int dev_id);
int dsp_hw_debug_register_ops(unsigned int dev_id,
		const struct dsp_hw_debug_ops *ops);

#endif
