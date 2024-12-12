// SPDX-License-Identifier: GPL-2.0
/*
 * Samsung Exynos SoC series dsp driver
 *
 * Copyright (c) 2020 Samsung Electronics Co., Ltd.
 *              http://www.samsung.com/
 */

#include "dsp-log.h"
#include "dsp-hw-dummy-system.h"
#include "dsp-hw-dummy-llc.h"

int dsp_hw_dummy_llc_get_by_name(struct dsp_llc *llc, unsigned char *name)
{
	dsp_enter();
	dsp_leave();
	return 0;
}

int dsp_hw_dummy_llc_put_by_name(struct dsp_llc *llc, unsigned char *name)
{
	dsp_enter();
	dsp_leave();
	return 0;
}

int dsp_hw_dummy_llc_get_by_id(struct dsp_llc *llc, unsigned int id)
{
	dsp_enter();
	dsp_leave();
	return 0;
}

int dsp_hw_dummy_llc_put_by_id(struct dsp_llc *llc, unsigned int id)
{
	dsp_enter();
	dsp_leave();
	return 0;
}

int dsp_hw_dummy_llc_all_put(struct dsp_llc *llc)
{
	dsp_enter();
	dsp_leave();
	return 0;
}

int dsp_hw_dummy_llc_open(struct dsp_llc *llc)
{
	dsp_enter();
	dsp_leave();
	return 0;
}

int dsp_hw_dummy_llc_close(struct dsp_llc *llc)
{
	dsp_enter();
	dsp_leave();
	return 0;
}

int dsp_hw_dummy_llc_probe(struct dsp_system *sys)
{
	dsp_enter();
	dsp_leave();
	return 0;
}

void dsp_hw_dummy_llc_remove(struct dsp_llc *llc)
{
	dsp_enter();
	dsp_leave();
}
