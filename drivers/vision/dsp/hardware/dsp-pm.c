// SPDX-License-Identifier: GPL-2.0
/*
 * Samsung Exynos SoC series dsp driver
 *
 * Copyright (c) 2019 Samsung Electronics Co., Ltd.
 *              http://www.samsung.com/
 */

#include <dt-bindings/soc/samsung/exynos-dsp.h>

#include "dsp-log.h"
#include "hardware/dsp-pm.h"

static const struct dsp_pm_ops *pm_ops_list[DSP_DEVICE_ID_END];

static void __dsp_pm_dump_ops_list(void)
{
	size_t idx;

	dsp_enter();
	for (idx = 0; idx < ARRAY_SIZE(pm_ops_list); ++idx) {
		if (pm_ops_list[idx])
			dsp_warn("pm ops[%zu] registered\n", idx);
		else
			dsp_warn("pm ops[%zu] None\n", idx);
	}
	dsp_leave();
}

static int __dsp_pm_check_ops(unsigned int dev_id)
{
	int ret;

	dsp_enter();
	if (dev_id >= ARRAY_SIZE(pm_ops_list)) {
		ret = -EINVAL;
		dsp_err("dev_id(%u) is invalid\n", dev_id);
		goto p_err;
	}

	if (!pm_ops_list[dev_id]) {
		ret = -EINVAL;
		dsp_err("pm ops is not registered(%u)\n", dev_id);
		__dsp_pm_dump_ops_list();
		goto p_err;
	}

	dsp_leave();
	return 0;
p_err:
	return ret;
}

int dsp_pm_set_ops(struct dsp_pm *pm, unsigned int dev_id)
{
	int ret;

	dsp_enter();
	ret = __dsp_pm_check_ops(dev_id);
	if (ret)
		goto p_err;

	pm->ops = pm_ops_list[dev_id];
	dsp_leave();
	return 0;
p_err:
	return ret;
}

int dsp_pm_register_ops(unsigned int dev_id, const struct dsp_pm_ops *ops)
{
	int ret;
	size_t idx;
	void **check;

	dsp_enter();
	if (dev_id >= ARRAY_SIZE(pm_ops_list)) {
		ret = -EINVAL;
		dsp_err("dev_id(%u) is invalid\n", dev_id);
		goto p_err;
	}

	if (pm_ops_list[dev_id]) {
		ret = -EINVAL;
		dsp_err("pm ops is already registered(%u)\n", dev_id);
		__dsp_pm_dump_ops_list();
		goto p_err;
	}

	ret = 0;
	check = (void **)ops;
	for (idx = 0; idx < sizeof(*ops) / sizeof(void *); ++idx) {
		if (!check[idx]) {
			ret = -EFAULT;
			dsp_warn("pm ops[%zu] is NULL\n", idx);
		}
	}
	if (ret) {
		dsp_err("There should be no space in pm ops\n");
		goto p_err;
	}

	pm_ops_list[dev_id] = ops;
	dsp_leave();
	return 0;
p_err:
	return ret;
}
