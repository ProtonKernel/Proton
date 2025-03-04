// SPDX-License-Identifier: GPL-2.0
/*
 * Samsung Exynos SoC series dsp driver
 *
 * Copyright (c) 2020 Samsung Electronics Co., Ltd.
 *              http://www.samsung.com/
 */

#include "dsp-log.h"
#include "dsp-hw-dummy-system.h"
#include "dsp-hw-dummy-debug.h"

void dsp_hw_dummy_debug_log_flush(struct dsp_hw_debug *debug)
{
	dsp_enter();
	dsp_leave();
}

int dsp_hw_dummy_debug_log_start(struct dsp_hw_debug *debug)
{
	dsp_enter();
	dsp_leave();
	return 0;
}

int dsp_hw_dummy_debug_log_stop(struct dsp_hw_debug *debug)
{
	dsp_enter();
	dsp_leave();
	return 0;
}

int dsp_hw_dummy_debug_open(struct dsp_hw_debug *debug)
{
	dsp_enter();
	dsp_leave();
	return 0;
}

int dsp_hw_dummy_debug_close(struct dsp_hw_debug *debug)
{
	dsp_enter();
	dsp_leave();
	return 0;
}

int dsp_hw_dummy_debug_probe(struct dsp_device *dspdev)
{
	dsp_enter();
	dsp_leave();
	return 0;
}

void dsp_hw_dummy_debug_remove(struct dsp_hw_debug *debug)
{
	dsp_enter();
	dsp_leave();
}
