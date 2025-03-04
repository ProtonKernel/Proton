/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Samsung Exynos SoC series dsp driver
 *
 * Copyright (c) 2019 Samsung Electronics Co., Ltd.
 *              http://www.samsung.com/
 */

#ifndef __DSP_CONTEXT_H__
#define __DSP_CONTEXT_H__

#include <linux/mutex.h>

#include "dsp-ioctl.h"
#include "dsp-core.h"

struct dsp_context {
	int				id;
	struct list_head		list;
	struct mutex			lock;
	unsigned int			boot_count;
	unsigned int			sec_boot_count;

	struct dsp_core			*core;
};

struct dsp_context *dsp_context_create(struct dsp_core *core);
void dsp_context_destroy(struct dsp_context *dctx);

const struct dsp_ioctl_ops *dsp_context_get_ops(void);

#endif
