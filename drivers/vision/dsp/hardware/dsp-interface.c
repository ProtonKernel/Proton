// SPDX-License-Identifier: GPL-2.0
/*
 * Samsung Exynos SoC series dsp driver
 *
 * Copyright (c) 2019 Samsung Electronics Co., Ltd.
 *              http://www.samsung.com/
 */

#include <dt-bindings/soc/samsung/exynos-dsp.h>

#include "dsp-log.h"
#include "hardware/dsp-interface.h"

static const struct dsp_interface_ops *itf_ops_list[DSP_DEVICE_ID_END];

static void __dsp_interface_dump_ops_list(void)
{
	size_t idx;

	dsp_enter();
	for (idx = 0; idx < ARRAY_SIZE(itf_ops_list); ++idx) {
		if (itf_ops_list[idx])
			dsp_warn("interface ops[%zu] registered\n", idx);
		else
			dsp_warn("interface ops[%zu] None\n", idx);
	}
	dsp_leave();
}

static int __dsp_interface_check_ops(unsigned int dev_id)
{
	int ret;

	dsp_enter();
	if (dev_id >= ARRAY_SIZE(itf_ops_list)) {
		ret = -EINVAL;
		dsp_err("dev_id(%u) is invalid\n", dev_id);
		goto p_err;
	}

	if (!itf_ops_list[dev_id]) {
		ret = -EINVAL;
		dsp_err("interface ops is not registered(%u)\n", dev_id);
		__dsp_interface_dump_ops_list();
		goto p_err;
	}

	dsp_leave();
	return 0;
p_err:
	return ret;
}

int dsp_interface_set_ops(struct dsp_interface *itf, unsigned int dev_id)
{
	int ret;

	dsp_enter();
	ret = __dsp_interface_check_ops(dev_id);
	if (ret)
		goto p_err;

	itf->ops = itf_ops_list[dev_id];
	dsp_leave();
	return 0;
p_err:
	return ret;
}

int dsp_interface_register_ops(unsigned int dev_id,
		const struct dsp_interface_ops *ops)
{
	int ret;
	size_t idx;
	void **check;

	dsp_enter();
	if (dev_id >= ARRAY_SIZE(itf_ops_list)) {
		ret = -EINVAL;
		dsp_err("dev_id(%u) is invalid\n", dev_id);
		goto p_err;
	}

	if (itf_ops_list[dev_id]) {
		ret = -EINVAL;
		dsp_err("interface ops is already registered(%u)\n", dev_id);
		__dsp_interface_dump_ops_list();
		goto p_err;
	}

	ret = 0;
	check = (void **)ops;
	for (idx = 0; idx < sizeof(*ops) / sizeof(void *); ++idx) {
		if (!check[idx]) {
			ret = -EFAULT;
			dsp_warn("interface ops[%zu] is NULL\n", idx);
		}
	}
	if (ret) {
		dsp_err("There should be no space in interface ops\n");
		goto p_err;
	}

	itf_ops_list[dev_id] = ops;
	dsp_leave();
	return 0;
p_err:
	return ret;
}
