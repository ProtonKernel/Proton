/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Samsung Exynos SoC series dsp driver
 *
 * Copyright (c) 2019 Samsung Electronics Co., Ltd.
 *              http://www.samsung.com/
 */

#ifndef __DSP_CORE_H__
#define __DSP_CORE_H__

#include <linux/mutex.h>
#include <linux/spinlock.h>
#include <linux/device.h>
#include <linux/miscdevice.h>

#include "dsp-util.h"
#include "dsp-ioctl.h"
#include "dsp-graph.h"

#define DSP_DEV_NAME_LEN		16
#define DSP_DEV_NAME			"dsp"

#define DSP_CORE_MAX_CONTEXT		(16)

struct dsp_device;

struct dsp_miscdev {
	int				minor;
	char				name[DSP_DEV_NAME_LEN];
	struct miscdevice		miscdev;
};

struct dsp_core {
	struct dsp_miscdev		miscdev;
	const struct dsp_ioctl_ops	*ioctl_ops;

	struct list_head		dctx_list;
	unsigned int			dctx_count;
	spinlock_t			dctx_slock;
	struct dsp_util_bitmap		context_map;

	struct dsp_graph_manager	graph_manager;
	struct dsp_device		*dspdev;
};

int dsp_core_probe(struct dsp_device *dspdev);
void dsp_core_remove(struct dsp_core *core);

#endif
