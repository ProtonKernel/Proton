// SPDX-License-Identifier: GPL-2.0
/*
 * Samsung Exynos SoC series dsp driver
 *
 * Copyright (c) 2020 Samsung Electronics Co., Ltd.
 *              http://www.samsung.com/
 */

#include <linux/slab.h>
#include <dt-bindings/soc/samsung/exynos-dsp.h>

#include "dsp-log.h"
#include "dsp-hw-o1-system.h"
#include "dsp-hw-o1-llc.h"

#ifndef ENABLE_DSP_VELOCE
#include <soc/samsung/exynos-sci.h>
#endif

static void __dsp_hw_o1_llc_dump_scen(struct dsp_llc_scenario *scen)
{
	unsigned int idx;
	struct dsp_llc_region *region;

	dsp_enter();
	dsp_dbg("scenario[%s] id : %u / region_count : %u\n", scen->name,
			scen->id, scen->region_count);
	for (idx = 0; idx < scen->region_count; ++idx) {
		region = &scen->region_list[idx];
		if (region->idx)
			dsp_dbg("region[%s] idx : %u / way : %u\n",
					region->name, region->idx, region->way);
		else
			dsp_dbg("region(%u) idx is not set\n", idx);
	}

	dsp_leave();
}

static int __dsp_hw_o1_llc_check_region(struct dsp_llc_scenario *scen)
{
	int ret;
	unsigned int idx;
	struct dsp_llc_region *region;

	dsp_enter();
	for (idx = 0; idx < scen->region_count; ++idx) {
		region = &scen->region_list[idx];
		if (!region->idx) {
			ret = -EINVAL;
			dsp_err("region(%u) idx is not set\n", idx);
			goto p_err;
		}
	}
	dsp_leave();
	return 0;
p_err:
	return ret;
}

static int __dsp_hw_o1_llc_put(struct dsp_llc *llc,
		struct dsp_llc_scenario *scen)
{
	int ret;
	unsigned int idx;
	struct dsp_llc_region *region;

	dsp_enter();
	ret = __dsp_hw_o1_llc_check_region(scen);
	if (ret) {
		ret = -EINVAL;
		dsp_err("scenario_name(%s) was not initilized\n", scen->name);
		goto p_err;
	}
	__dsp_hw_o1_llc_dump_scen(scen);

	mutex_lock(&scen->lock);
	if (scen->enabled) {
		for (idx = 0; idx < scen->region_count; ++idx) {
			region = &scen->region_list[idx];
			llc_region_alloc(region->idx, 0, 0);
			dsp_dbg("llc region[%s/%u] : off\n", region->name,
					region->idx);
		}
		scen->enabled = false;
		dsp_info("llc scen[%s] is disabled\n", scen->name);
	} else {
		dsp_dbg("llc scen[%s] is not enabled\n", scen->name);
	}
	mutex_unlock(&scen->lock);

	dsp_leave();
	return 0;
p_err:
	return ret;
}

static int __dsp_hw_o1_llc_get(struct dsp_llc *llc,
		struct dsp_llc_scenario *scen)
{
	int ret;
	unsigned int idx;
	struct dsp_llc_region *region;

	dsp_enter();
	ret = __dsp_hw_o1_llc_check_region(scen);
	if (ret) {
		ret = -EINVAL;
		dsp_err("scenario_name(%s) was not initilized\n", scen->name);
		goto p_err;
	}
	__dsp_hw_o1_llc_dump_scen(scen);

	for (idx = 0; idx < DSP_O1_LLC_SCENARIO_COUNT; ++idx) {
		if (idx == scen->id)
			continue;

		__dsp_hw_o1_llc_put(llc, &llc->scen[idx]);
	}

	mutex_lock(&scen->lock);
	if (!scen->enabled) {
		for (idx = 0; idx < scen->region_count; ++idx) {
			region = &scen->region_list[idx];
			llc_region_alloc(region->idx, 1, region->way);
			dsp_dbg("llc region[%s/%u] : on\n", region->name,
					region->idx);
		}
		scen->enabled = true;
		dsp_info("llc scen[%s] is enabled\n", scen->name);
	} else {
		dsp_dbg("llc scen[%s] is already enabled\n", scen->name);
	}
	mutex_unlock(&scen->lock);

	dsp_leave();
	return 0;
p_err:
	return ret;
}

static int dsp_hw_o1_llc_get_by_name(struct dsp_llc *llc, unsigned char *name)
{
	int ret;
	int idx;
	struct dsp_llc_scenario *scen = NULL;

	dsp_enter();
	for (idx = 0; idx < DSP_O1_LLC_SCENARIO_COUNT; ++idx) {
		if (!strncmp(llc->scen[idx].name, name,
					DSP_LLC_SCENARIO_NAME_LEN)) {
			scen = &llc->scen[idx];
			break;
		}
	}

	if (!scen) {
		ret = -EINVAL;
		dsp_err("scenario_name(%s) is invalid\n", name);
		goto p_err;
	}

	ret = __dsp_hw_o1_llc_get(llc, scen);
	if (ret)
		goto p_err;

	dsp_leave();
	return 0;
p_err:
	return ret;
}

static int dsp_hw_o1_llc_put_by_name(struct dsp_llc *llc, unsigned char *name)
{
	int ret;
	int idx;
	struct dsp_llc_scenario *scen = NULL;

	dsp_enter();
	for (idx = 0; idx < DSP_O1_LLC_SCENARIO_COUNT; ++idx) {
		if (!strncmp(llc->scen[idx].name, name,
					DSP_LLC_SCENARIO_NAME_LEN)) {
			scen = &llc->scen[idx];
			break;
		}
	}

	if (!scen) {
		ret = -EINVAL;
		dsp_err("scenario_name(%s) is invalid\n", name);
		goto p_err;
	}

	ret = __dsp_hw_o1_llc_put(llc, scen);
	if (ret)
		goto p_err;

	dsp_leave();
	return 0;
p_err:
	return ret;
}

static int dsp_hw_o1_llc_get_by_id(struct dsp_llc *llc, unsigned int id)
{
	int ret;

	dsp_enter();
	if (id >= DSP_O1_LLC_SCENARIO_COUNT) {
		ret = -EINVAL;
		dsp_err("scenario_id(%u) is invalid\n", id);
		goto p_err;
	}

	ret = __dsp_hw_o1_llc_get(llc, &llc->scen[id]);
	if (ret)
		goto p_err;

	dsp_leave();
	return 0;
p_err:
	return ret;
}

static int dsp_hw_o1_llc_put_by_id(struct dsp_llc *llc, unsigned int id)
{
	int ret;

	dsp_enter();
	if (id >= DSP_O1_LLC_SCENARIO_COUNT) {
		ret = -EINVAL;
		dsp_err("scenario_id(%u) is invalid\n", id);
		goto p_err;
	}

	ret = __dsp_hw_o1_llc_put(llc, &llc->scen[id]);
	if (ret)
		goto p_err;

	dsp_leave();
	return 0;
p_err:
	return ret;
}

static int dsp_hw_o1_llc_all_put(struct dsp_llc *llc)
{
	unsigned int idx;

	dsp_enter();
	for (idx = 0; idx < DSP_O1_LLC_SCENARIO_COUNT; ++idx)
		__dsp_hw_o1_llc_put(llc, &llc->scen[idx]);

	dsp_leave();
	return 0;
}

static int dsp_hw_o1_llc_open(struct dsp_llc *llc)
{
	int idx;
	struct dsp_llc_scenario *scen;

	dsp_enter();
	for (idx = 0; idx < DSP_O1_LLC_SCENARIO_COUNT; ++idx) {
		scen = &llc->scen[idx];
		__dsp_hw_o1_llc_dump_scen(scen);
	}
	dsp_leave();
	return 0;
}

static int dsp_hw_o1_llc_close(struct dsp_llc *llc)
{
	dsp_enter();
	llc->ops->llc_all_put(llc);
	dsp_leave();
	return 0;
}

static int __dsp_hw_o1_llc_set_scen(struct dsp_llc *llc)
{
#if IS_ENABLED(CONFIG_EXYNOS_SCI) && defined(CONFIG_EXYNOS_DSP_HW_O1)
	int ret;
	struct dsp_llc_scenario *scen;
	struct dsp_llc_region *region;

	dsp_enter();
	scen = &llc->scen[DSP_O1_LLC_CASE1];
	snprintf(scen->name, DSP_LLC_SCENARIO_NAME_LEN, "case1");
	scen->region_count = 2;

	scen->region_list = kzalloc((sizeof(*region) * scen->region_count),
			GFP_KERNEL);
	if (!scen->region_list) {
		ret = -ENOMEM;
		dsp_err("Failed to alloc dsp_llc_region\n");
		goto p_err;
	}

	region = &scen->region_list[0];
	snprintf(region->name, DSP_LLC_REGION_NAME_LEN, "llc_dsp0");
	region->idx = LLC_REGION_DSP0;
	region->way = 4;

	region = &scen->region_list[1];
	snprintf(region->name, DSP_LLC_REGION_NAME_LEN, "llc_dsp1");
	region->idx = LLC_REGION_DSP1;
	region->way = 2;

	dsp_leave();
	return 0;
p_err:
	return ret;
#else
	dsp_enter();
	dsp_leave();
	return 0;
#endif
}

static int dsp_hw_o1_llc_probe(struct dsp_system *sys)
{
	int ret;
	struct dsp_llc *llc;
	struct dsp_llc_scenario *scen;
	int idx;

	dsp_enter();
	llc = &sys->llc;
	llc->sys = sys;

	llc->scen = kzalloc(sizeof(*scen) * DSP_O1_LLC_SCENARIO_COUNT,
			GFP_KERNEL);
	if (!llc->scen) {
		ret = -ENOMEM;
		dsp_err("Failed to alloc dsp_llc_scenario\n");
		goto p_err;
	}

	ret = __dsp_hw_o1_llc_set_scen(llc);
	if (ret) {
		dsp_err("Failed to set scenario\n");
		goto p_err_scen;
	}

	for (idx = 0; idx < DSP_O1_LLC_SCENARIO_COUNT; ++idx) {
		llc->scen[idx].id = idx;
		mutex_init(&llc->scen[idx].lock);
	}

	dsp_leave();
	return 0;
p_err_scen:
	kfree(llc->scen);
p_err:
	return ret;
}

static void __dsp_hw_o1_llc_unset_scen(struct dsp_llc *llc)
{
#if IS_ENABLED(CONFIG_EXYNOS_SCI) && defined(CONFIG_EXYNOS_DSP_HW_O1)
	int idx;

	dsp_enter();
	for (idx = 0; idx < DSP_O1_LLC_SCENARIO_COUNT; ++idx) {
		if (llc->scen[idx].region_count)
			kfree(llc->scen[idx].region_list);
	}

	dsp_leave();
#else
	dsp_enter();
	dsp_leave();
#endif
}

static void dsp_hw_o1_llc_remove(struct dsp_llc *llc)
{
	int idx;

	dsp_enter();
	for (idx = 0; idx < DSP_O1_LLC_SCENARIO_COUNT; ++idx)
		mutex_destroy(&llc->scen[idx].lock);

	__dsp_hw_o1_llc_unset_scen(llc);
	kfree(llc->scen);
	dsp_leave();
}

static const struct dsp_llc_ops o1_llc_ops = {
	.llc_get_by_name	= dsp_hw_o1_llc_get_by_name,
	.llc_put_by_name	= dsp_hw_o1_llc_put_by_name,
	.llc_get_by_id		= dsp_hw_o1_llc_get_by_id,
	.llc_put_by_id		= dsp_hw_o1_llc_put_by_id,
	.llc_all_put		= dsp_hw_o1_llc_all_put,

	.open			= dsp_hw_o1_llc_open,
	.close			= dsp_hw_o1_llc_close,
	.probe			= dsp_hw_o1_llc_probe,
	.remove			= dsp_hw_o1_llc_remove,
};

int dsp_hw_o1_llc_init(void)
{
	int ret;

	dsp_enter();
	ret = dsp_llc_register_ops(DSP_DEVICE_ID_O1, &o1_llc_ops);
	if (ret)
		goto p_err;

	dsp_leave();
	return 0;
p_err:
	return ret;
}
