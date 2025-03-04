// SPDX-License-Identifier: GPL-2.0
/*
 * Samsung Exynos SoC series dsp driver
 *
 * Copyright (c) 2019 Samsung Electronics Co., Ltd.
 *              http://www.samsung.com/
 */

#include <linux/slab.h>
#include <dt-bindings/soc/samsung/exynos-dsp.h>

#include "dsp-log.h"
#include "dsp-hw-o1-system.h"
#include "dsp-hw-o1-bus.h"

#ifndef ENABLE_DSP_VELOCE
#include <soc/samsung/bts.h>
#else
#define bts_del_scenario(x)
#define bts_add_scenario(x)
#define bts_get_scenindex(x)	(1)
#endif

static int __dsp_hw_o1_bus_mo_put(struct dsp_bus *bus,
		struct dsp_bus_scenario *scen)
{
	int ret;

	dsp_enter();
	if (!scen->bts_scen_idx) {
		ret = -EINVAL;
		dsp_dbg("scenario_name(%s) was not initilized\n", scen->name);
		goto p_err;
	}
	dsp_dbg("scenario[%s] idx : %u\n", scen->name, scen->bts_scen_idx);

	mutex_lock(&scen->lock);
	if (scen->enabled) {
		bts_del_scenario(scen->bts_scen_idx);
		scen->enabled = false;
		dsp_info("bus scenario[%s] is disabled\n", scen->name);
	} else {
		dsp_dbg("bus scenario[%s] is not enabled\n", scen->name);
	}
	mutex_unlock(&scen->lock);

	dsp_leave();
	return 0;
p_err:
	return ret;
}

static int __dsp_hw_o1_bus_mo_get(struct dsp_bus *bus,
		struct dsp_bus_scenario *scen)
{
	int ret;
	unsigned int idx;

	dsp_enter();
	if (!scen->bts_scen_idx) {
		ret = -EINVAL;
		dsp_err("scenario_name(%s) was not initilized\n", scen->name);
		goto p_err;
	}
	dsp_dbg("scenario[%s] idx : %u\n", scen->name, scen->bts_scen_idx);

	for (idx = 0; idx < DSP_O1_MO_SCENARIO_COUNT; ++idx) {
		if (idx == scen->id)
			continue;

		__dsp_hw_o1_bus_mo_put(bus, &bus->scen[idx]);
	}

	mutex_lock(&scen->lock);
	if (!scen->enabled) {
		bts_add_scenario(scen->bts_scen_idx);
		scen->enabled = true;
		dsp_info("bus scenario[%s] is enabled\n", scen->name);
	} else {
		dsp_dbg("bus scenario[%s] is already enabled\n", scen->name);
	}
	mutex_unlock(&scen->lock);

	dsp_leave();
	return 0;
p_err:
	return ret;
}

static int dsp_hw_o1_bus_mo_get_by_name(struct dsp_bus *bus,
		unsigned char *name)
{
	int ret;
	int idx;
	struct dsp_bus_scenario *scen = NULL;

	dsp_enter();
	for (idx = 0; idx < DSP_O1_MO_SCENARIO_COUNT; ++idx) {
		if (!strncmp(bus->scen[idx].name, name,
					DSP_BUS_SCENARIO_NAME_LEN)) {
			scen = &bus->scen[idx];
			break;
		}
	}

	if (!scen) {
		ret = -EINVAL;
		dsp_err("scenario_name(%s) is invalid\n", name);
		goto p_err;
	}

	ret = __dsp_hw_o1_bus_mo_get(bus, scen);
	if (ret)
		goto p_err;

	dsp_leave();
	return 0;
p_err:
	return ret;
}

static int dsp_hw_o1_bus_mo_put_by_name(struct dsp_bus *bus,
		unsigned char *name)
{
	int ret;
	int idx;
	struct dsp_bus_scenario *scen = NULL;

	dsp_enter();
	for (idx = 0; idx < DSP_O1_MO_SCENARIO_COUNT; ++idx) {
		if (!strncmp(bus->scen[idx].name, name,
					DSP_BUS_SCENARIO_NAME_LEN)) {
			scen = &bus->scen[idx];
			break;
		}
	}

	if (!scen) {
		ret = -EINVAL;
		dsp_err("scenario_name(%s) is invalid\n", name);
		goto p_err;
	}

	ret = __dsp_hw_o1_bus_mo_put(bus, scen);
	if (ret)
		goto p_err;

	dsp_leave();
	return 0;
p_err:
	return ret;
}

static int dsp_hw_o1_bus_mo_get_by_id(struct dsp_bus *bus, unsigned int id)
{
	int ret;

	dsp_enter();
	if (id >= DSP_O1_MO_SCENARIO_COUNT) {
		ret = -EINVAL;
		dsp_err("scenario_id(%u) is invalid\n", id);
		goto p_err;
	}

	ret = __dsp_hw_o1_bus_mo_get(bus, &bus->scen[id]);
	if (ret)
		goto p_err;

	dsp_leave();
	return 0;
p_err:
	return ret;
}

static int dsp_hw_o1_bus_mo_put_by_id(struct dsp_bus *bus, unsigned int id)
{
	int ret;

	dsp_enter();
	if (id >= DSP_O1_MO_SCENARIO_COUNT) {
		ret = -EINVAL;
		dsp_err("scenario_id(%u) is invalid\n", id);
		goto p_err;
	}

	ret = __dsp_hw_o1_bus_mo_put(bus, &bus->scen[id]);
	if (ret)
		goto p_err;

	dsp_leave();
	return 0;
p_err:
	return ret;
}

static int dsp_hw_o1_bus_mo_all_put(struct dsp_bus *bus)
{
	unsigned int idx;

	dsp_enter();
	for (idx = 0; idx < DSP_O1_MO_SCENARIO_COUNT; ++idx)
		__dsp_hw_o1_bus_mo_put(bus, &bus->scen[idx]);

	dsp_leave();
	return 0;
}

static int dsp_hw_o1_bus_open(struct dsp_bus *bus)
{
	int idx;
	struct dsp_bus_scenario *scen;

	dsp_enter();
	for (idx = 0; idx < DSP_O1_MO_SCENARIO_COUNT; ++idx) {
		scen = &bus->scen[idx];

		dsp_dbg("scenario[%d/%d] info : [%s/%u]\n",
				idx, DSP_O1_MO_SCENARIO_COUNT,
				scen->name, scen->bts_scen_idx);
	}
	dsp_leave();
	return 0;
}

static int dsp_hw_o1_bus_close(struct dsp_bus *bus)
{
	dsp_enter();
	dsp_hw_o1_bus_mo_all_put(bus);
	dsp_leave();
	return 0;
}

static int dsp_hw_o1_bus_probe(struct dsp_system *sys)
{
	int ret;
	struct dsp_bus *bus;
	struct dsp_bus_scenario *scen;
	int idx;

	dsp_enter();
	bus = &sys->bus;
	bus->sys = sys;

	bus->scen = kzalloc(sizeof(*scen) * DSP_O1_MO_SCENARIO_COUNT,
			GFP_KERNEL);
	if (!bus->scen) {
		ret = -ENOMEM;
		dsp_err("Failed to alloc dsp_bus_scenario\n");
		goto p_err;
	}

#if IS_ENABLED(CONFIG_EXYNOS_BTS)
	scen = &bus->scen[DSP_O1_MO_MAX];
	snprintf(scen->name, DSP_BUS_SCENARIO_NAME_LEN, "max");
	scen->bts_scen_idx = bts_get_scenindex("npu_performance");

	scen = &bus->scen[DSP_O1_MO_CAMERA];
	snprintf(scen->name, DSP_BUS_SCENARIO_NAME_LEN, "camera");
	scen->bts_scen_idx = bts_get_scenindex("npu_normal");
#endif

	for (idx = 0; idx < DSP_O1_MO_SCENARIO_COUNT; ++idx) {
		bus->scen[idx].id = idx;
		mutex_init(&bus->scen[idx].lock);
	}

	dsp_leave();
	return 0;
p_err:
	return ret;
}

static void dsp_hw_o1_bus_remove(struct dsp_bus *bus)
{
	int idx;

	dsp_enter();
	for (idx = 0; idx < DSP_O1_MO_SCENARIO_COUNT; ++idx)
		mutex_destroy(&bus->scen[idx].lock);
	kfree(bus->scen);
	dsp_leave();
}

static const struct dsp_bus_ops o1_bus_ops = {
	.mo_get_by_name	= dsp_hw_o1_bus_mo_get_by_name,
	.mo_put_by_name	= dsp_hw_o1_bus_mo_put_by_name,
	.mo_get_by_id	= dsp_hw_o1_bus_mo_get_by_id,
	.mo_put_by_id	= dsp_hw_o1_bus_mo_put_by_id,
	.mo_all_put	= dsp_hw_o1_bus_mo_all_put,

	.open		= dsp_hw_o1_bus_open,
	.close		= dsp_hw_o1_bus_close,
	.probe		= dsp_hw_o1_bus_probe,
	.remove		= dsp_hw_o1_bus_remove,
};

int dsp_hw_o1_bus_init(void)
{
	int ret;

	dsp_enter();
	ret = dsp_bus_register_ops(DSP_DEVICE_ID_O1, &o1_bus_ops);
	if (ret)
		goto p_err;

	dsp_leave();
	return 0;
p_err:
	return ret;
}
