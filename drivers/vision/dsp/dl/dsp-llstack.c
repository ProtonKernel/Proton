// SPDX-License-Identifier: GPL-2.0
/*
 * Samsung Exynos SoC series dsp driver
 *
 * Copyright (c) 2019 Samsung Electronics Co., Ltd.
 *              http://www.samsung.com/
 */

#include "dl/dsp-llstack.h"

void dsp_llstack_init(struct dsp_llstack *st)
{
	st->top = -1;
}

int dsp_llstack_push(struct dsp_llstack *st, long long v)
{
	if (st->top >= LLSTACK_MAX - 1)
		return -1;

	st->arr[++st->top] = v;
	return 0;
}

int dsp_llstack_pop(struct dsp_llstack *st, long long *v)
{
	if (st->top == -1)
		return -1;

	*v = st->arr[st->top--];
	return 0;
}
