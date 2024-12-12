/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Samsung Exynos SoC series dsp driver
 *
 * Copyright (c) 2019 Samsung Electronics Co., Ltd
 */

#ifndef __DSP_DEBUG_H__
#define __DSP_DEBUG_H__

#include <linux/dcache.h>

struct dsp_device;

struct dsp_debug {
	struct dentry	*root;
	struct dentry	*debug_log;
};

int dsp_debug_open(struct dsp_debug *debug);
int dsp_debug_close(struct dsp_debug *debug);

int dsp_debug_probe(struct dsp_device *dspdev);
void dsp_debug_remove(struct dsp_debug *debug);

#endif
