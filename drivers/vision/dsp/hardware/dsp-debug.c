// SPDX-License-Identifier: GPL-2.0
/*
 * Samsung Exynos SoC series dsp driver
 *
 * Copyright (c) 2019 Samsung Electronics Co., Ltd.
 *              http://www.samsung.com/
 */

#include <dt-bindings/soc/samsung/exynos-dsp.h>

#include "dsp-log.h"
#include "hardware/dsp-debug.h"

static const struct dsp_hw_debug_ops *hw_debug_ops_list[DSP_DEVICE_ID_END];

static void __dsp_hw_debug_dump_ops_list(void)
{
	size_t idx;

	dsp_enter();
	for (idx = 0; idx < ARRAY_SIZE(hw_debug_ops_list); ++idx) {
		if (hw_debug_ops_list[idx])
			dsp_warn("hw_debug ops[%zu] registered\n", idx);
		else
			dsp_warn("hw_debug ops[%zu] None\n", idx);
	}
	dsp_leave();
}

static int __dsp_hw_debug_check_ops(unsigned int dev_id)
{
	int ret;

	dsp_enter();
	if (dev_id >= ARRAY_SIZE(hw_debug_ops_list)) {
		ret = -EINVAL;
		dsp_err("dev_id(%u) is invalid\n", dev_id);
		goto p_err;
	}

	if (!hw_debug_ops_list[dev_id]) {
		ret = -EINVAL;
		dsp_err("hw_debug ops is not registered(%u)\n", dev_id);
		__dsp_hw_debug_dump_ops_list();
		goto p_err;
	}

	dsp_leave();
	return 0;
p_err:
	return ret;
}

int dsp_hw_debug_set_ops(struct dsp_hw_debug *debug, unsigned int dev_id)
{
	int ret;

	dsp_enter();
	ret = __dsp_hw_debug_check_ops(dev_id);
	if (ret)
		goto p_err;

	debug->ops = hw_debug_ops_list[dev_id];
	dsp_leave();
	return 0;
p_err:
	return ret;
}

int dsp_hw_debug_register_ops(unsigned int dev_id,
		const struct dsp_hw_debug_ops *ops)
{
	int ret;
	size_t idx;
	void **check;

	dsp_enter();
	if (dev_id >= ARRAY_SIZE(hw_debug_ops_list)) {
		ret = -EINVAL;
		dsp_err("dev_id(%u) is invalid\n", dev_id);
		goto p_err;
	}

	if (hw_debug_ops_list[dev_id]) {
		ret = -EINVAL;
		dsp_err("hw_debug ops is already registered(%u)\n", dev_id);
		__dsp_hw_debug_dump_ops_list();
		goto p_err;
	}

	ret = 0;
	check = (void **)ops;
	for (idx = 0; idx < sizeof(*ops) / sizeof(void *); ++idx) {
		if (!check[idx]) {
			ret = -EFAULT;
			dsp_warn("hw_debug ops[%zu] is NULL\n", idx);
		}
	}
	if (ret) {
		dsp_err("There should be no space in hw_debug ops\n");
		goto p_err;
	}

	hw_debug_ops_list[dev_id] = ops;
	dsp_leave();
	return 0;
p_err:
	return ret;
}
