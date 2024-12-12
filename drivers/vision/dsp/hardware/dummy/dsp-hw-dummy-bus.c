// SPDX-License-Identifier: GPL-2.0
/*
 * Samsung Exynos SoC series dsp driver
 *
 * Copyright (c) 2020 Samsung Electronics Co., Ltd.
 *              http://www.samsung.com/
 */

#include "dsp-log.h"
#include "dsp-hw-dummy-system.h"
#include "dsp-hw-dummy-bus.h"

int dsp_hw_dummy_bus_mo_get_by_name(struct dsp_bus *bus, unsigned char *name)
{
	dsp_enter();
	dsp_leave();
	return 0;
}

int dsp_hw_dummy_bus_mo_put_by_name(struct dsp_bus *bus, unsigned char *name)
{
	dsp_enter();
	dsp_leave();
	return 0;
}

int dsp_hw_dummy_bus_mo_get_by_id(struct dsp_bus *bus, unsigned int id)
{
	dsp_enter();
	dsp_leave();
	return 0;
}

int dsp_hw_dummy_bus_mo_put_by_id(struct dsp_bus *bus, unsigned int id)
{
	dsp_enter();
	dsp_leave();
	return 0;
}

int dsp_hw_dummy_bus_mo_all_put(struct dsp_bus *bus)
{
	dsp_enter();
	dsp_leave();
	return 0;
}

int dsp_hw_dummy_bus_open(struct dsp_bus *bus)
{
	dsp_enter();
	dsp_leave();
	return 0;
}

int dsp_hw_dummy_bus_close(struct dsp_bus *bus)
{
	dsp_enter();
	dsp_leave();
	return 0;
}

int dsp_hw_dummy_bus_probe(struct dsp_system *sys)
{
	dsp_enter();
	dsp_leave();
	return 0;
}

void dsp_hw_dummy_bus_remove(struct dsp_bus *bus)
{
	dsp_enter();
	dsp_leave();
}
