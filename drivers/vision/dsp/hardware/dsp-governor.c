// SPDX-License-Identifier: GPL-2.0
/*
 * Samsung Exynos SoC series dsp driver
 *
 * Copyright (c) 2020 Samsung Electronics Co., Ltd.
 *              http://www.samsung.com/
 */

#include <dt-bindings/soc/samsung/exynos-dsp.h>

#include "dsp-log.h"
#include "hardware/dsp-governor.h"

static const struct dsp_governor_ops *governor_ops_list[DSP_DEVICE_ID_END];

static void __dsp_governor_dump_ops_list(void)
{
	size_t idx;

	dsp_enter();
	for (idx = 0; idx < ARRAY_SIZE(governor_ops_list); ++idx) {
		if (governor_ops_list[idx])
			dsp_warn("governor ops[%zu] registered\n", idx);
		else
			dsp_warn("governor ops[%zu] None\n", idx);
	}
	dsp_leave();
}

static int __dsp_governor_check_ops(unsigned int dev_id)
{
	int ret;

	dsp_enter();
	if (dev_id >= ARRAY_SIZE(governor_ops_list)) {
		ret = -EINVAL;
		dsp_err("dev_id(%u) is invalid\n", dev_id);
		goto p_err;
	}

	if (!governor_ops_list[dev_id]) {
		ret = -EINVAL;
		dsp_err("governor ops is not registered(%u)\n", dev_id);
		__dsp_governor_dump_ops_list();
		goto p_err;
	}

	dsp_leave();
	return 0;
p_err:
	return ret;
}

int dsp_governor_set_ops(struct dsp_governor *governor, unsigned int dev_id)
{
	int ret;

	dsp_enter();
	ret = __dsp_governor_check_ops(dev_id);
	if (ret)
		goto p_err;

	governor->ops = governor_ops_list[dev_id];
	dsp_leave();
	return 0;
p_err:
	return ret;
}

int dsp_governor_register_ops(unsigned int dev_id,
		const struct dsp_governor_ops *ops)
{
	int ret;
	size_t idx;
	void **check;

	dsp_enter();
	if (dev_id >= ARRAY_SIZE(governor_ops_list)) {
		ret = -EINVAL;
		dsp_err("dev_id(%u) is invalid\n", dev_id);
		goto p_err;
	}

	if (governor_ops_list[dev_id]) {
		ret = -EINVAL;
		dsp_err("governor ops is already registered(%u)\n", dev_id);
		__dsp_governor_dump_ops_list();
		goto p_err;
	}

	ret = 0;
	check = (void **)ops;
	for (idx = 0; idx < sizeof(*ops) / sizeof(void *); ++idx) {
		if (!check[idx]) {
			ret = -EFAULT;
			dsp_warn("governor ops[%zu] is NULL\n", idx);
		}
	}
	if (ret) {
		dsp_err("There should be no space in governor ops\n");
		goto p_err;
	}

	governor_ops_list[dev_id] = ops;
	dsp_leave();
	return 0;
p_err:
	return ret;
}
