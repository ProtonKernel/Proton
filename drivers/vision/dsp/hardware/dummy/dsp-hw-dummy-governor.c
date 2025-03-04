// SPDX-License-Identifier: GPL-2.0
/*
 * Samsung Exynos SoC series dsp driver
 *
 * Copyright (c) 2020 Samsung Electronics Co., Ltd.
 *              http://www.samsung.com/
 */

#include "dsp-log.h"
#include "dsp-hw-dummy-system.h"
#include "dsp-hw-dummy-governor.h"

int dsp_hw_dummy_governor_request(struct dsp_governor *governor,
		struct dsp_control_preset *req)
{
	dsp_enter();
	dsp_leave();
	return 0;
}

int dsp_hw_dummy_governor_check_done(struct dsp_governor *governor)
{
	dsp_enter();
	dsp_leave();
	return 0;
}

int dsp_hw_dummy_governor_flush_work(struct dsp_governor *governor)
{
	dsp_enter();
	dsp_leave();
	return 0;
}

int dsp_hw_dummy_governor_open(struct dsp_governor *governor)
{
	dsp_enter();
	dsp_leave();
	return 0;
}

int dsp_hw_dummy_governor_close(struct dsp_governor *governor)
{
	dsp_enter();
	dsp_leave();
	return 0;
}

int dsp_hw_dummy_governor_probe(struct dsp_system *sys)
{
	dsp_enter();
	dsp_leave();
	return 0;
}

void dsp_hw_dummy_governor_remove(struct dsp_governor *governor)
{
	dsp_enter();
	dsp_leave();
}
