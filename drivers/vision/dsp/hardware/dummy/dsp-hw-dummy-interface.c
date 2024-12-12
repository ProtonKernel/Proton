// SPDX-License-Identifier: GPL-2.0
/*
 * Samsung Exynos SoC series dsp driver
 *
 * Copyright (c) 2020 Samsung Electronics Co., Ltd.
 *              http://www.samsung.com/
 */

#include "dsp-log.h"
#include "dsp-hw-dummy-system.h"
#include "dsp-hw-dummy-interface.h"

int dsp_hw_dummy_interface_send_irq(struct dsp_interface *itf, int status)
{
	dsp_enter();
	dsp_leave();
	return 0;
}

int dsp_hw_dummy_interface_check_irq(struct dsp_interface *itf)
{
	dsp_enter();
	dsp_leave();
	return 0;
}

int dsp_hw_dummy_interface_start(struct dsp_interface *itf)
{
	dsp_enter();
	dsp_leave();
	return 0;
}

int dsp_hw_dummy_interface_stop(struct dsp_interface *itf)
{
	dsp_enter();
	dsp_leave();
	return 0;
}

int dsp_hw_dummy_interface_open(struct dsp_interface *itf)
{
	dsp_enter();
	dsp_leave();
	return 0;
}

int dsp_hw_dummy_interface_close(struct dsp_interface *itf)
{
	dsp_enter();
	dsp_leave();
	return 0;
}

int dsp_hw_dummy_interface_probe(struct dsp_system *sys)
{
	dsp_enter();
	dsp_leave();
	return 0;
}

void dsp_hw_dummy_interface_remove(struct dsp_interface *itf)
{
	dsp_enter();
	dsp_leave();
}
