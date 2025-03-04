/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Samsung Exynos SoC series dsp driver
 *
 * Copyright (c) 2020 Samsung Electronics Co., Ltd.
 *              http://www.samsung.com/
 */

#ifndef __HW_DUMMY_DSP_HW_DUMMY_BUS_H__
#define __HW_DUMMY_DSP_HW_DUMMY_BUS_H__

#include "hardware/dsp-bus.h"

int dsp_hw_dummy_bus_mo_get_by_name(struct dsp_bus *bus, unsigned char *name);
int dsp_hw_dummy_bus_mo_put_by_name(struct dsp_bus *bus, unsigned char *name);
int dsp_hw_dummy_bus_mo_get_by_id(struct dsp_bus *bus, unsigned int id);
int dsp_hw_dummy_bus_mo_put_by_id(struct dsp_bus *bus, unsigned int id);
int dsp_hw_dummy_bus_mo_all_put(struct dsp_bus *bus);

int dsp_hw_dummy_bus_open(struct dsp_bus *bus);
int dsp_hw_dummy_bus_close(struct dsp_bus *bus);
int dsp_hw_dummy_bus_probe(struct dsp_system *sys);
void dsp_hw_dummy_bus_remove(struct dsp_bus *bus);

#endif
