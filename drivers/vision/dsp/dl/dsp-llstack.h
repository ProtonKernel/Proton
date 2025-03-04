/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Samsung Exynos SoC series dsp driver
 *
 * Copyright (c) 2019 Samsung Electronics Co., Ltd.
 *              http://www.samsung.com/
 */

#ifndef __DL_DSP_LLSTACK_H__
#define __DL_DSP_LLSTACK_H__

#include "dl/dsp-common.h"

#define LLSTACK_MAX		(100)

struct dsp_llstack {
	long long arr[LLSTACK_MAX];
	int top;
};

void dsp_llstack_init(struct dsp_llstack *st);
int dsp_llstack_push(struct dsp_llstack *st, long long v);
int dsp_llstack_pop(struct dsp_llstack *st, long long *v);

#endif
