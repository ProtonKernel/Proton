/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Samsung Exynos SoC series dsp driver
 *
 * Copyright (c) 2019 Samsung Electronics Co., Ltd.
 *              http://www.samsung.com/
 */

#ifndef __DSP_TIME_H__
#define __DSP_TIME_H__

#include <linux/ktime.h>

#define TIMESTAMP_START		(1 << 0)
#define TIMESTAMP_END		(1 << 1)

struct dsp_time {
	struct timespec		start;
	struct timespec		end;
	struct timespec		interval;
};

void dsp_time_get_timestamp(struct dsp_time *time, int opt);
void dsp_time_get_interval(struct dsp_time *time);
void dsp_time_print(struct dsp_time *time, const char *f, ...);

#endif
