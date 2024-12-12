// SPDX-License-Identifier: GPL-2.0
/*
 * Samsung Exynos SoC series dsp driver
 *
 * Copyright (c) 2019 Samsung Electronics Co., Ltd.
 *              http://www.samsung.com/
 */

#include <dt-bindings/soc/samsung/exynos-dsp.h>

#include "dsp-log.h"
#include "hardware/dsp-bus.h"

static const struct dsp_bus_ops *bus_ops_list[DSP_DEVICE_ID_END];

static void __dsp_bus_dump_ops_list(void)
{
	size_t idx;

	dsp_enter();
	for (idx = 0; idx < ARRAY_SIZE(bus_ops_list); ++idx) {
		if (bus_ops_list[idx])
			dsp_warn("bus ops list[%zu] registered\n", idx);
		else
			dsp_warn("bus ops list[%zu] None\n", idx);
	}
	dsp_leave();
}

static int __dsp_bus_check_ops(unsigned int dev_id)
{
	int ret;

	dsp_enter();
	if (dev_id >= ARRAY_SIZE(bus_ops_list)) {
		ret = -EINVAL;
		dsp_err("dev_id(%u) is invalid\n", dev_id);
		goto p_err;
	}

	if (!bus_ops_list[dev_id]) {
		ret = -EINVAL;
		dsp_err("bus ops is not registered(%u)\n", dev_id);
		__dsp_bus_dump_ops_list();
		goto p_err;
	}

	dsp_leave();
	return 0;
p_err:
	return ret;
}

int dsp_bus_set_ops(struct dsp_bus *bus, unsigned int dev_id)
{
	int ret;

	dsp_enter();
	ret = __dsp_bus_check_ops(dev_id);
	if (ret)
		goto p_err;

	bus->ops = bus_ops_list[dev_id];
	dsp_leave();
	return 0;
p_err:
	return ret;
}

int dsp_bus_register_ops(unsigned int dev_id, const struct dsp_bus_ops *ops)
{
	int ret;
	size_t idx;
	void **check;

	dsp_enter();
	if (dev_id >= ARRAY_SIZE(bus_ops_list)) {
		ret = -EINVAL;
		dsp_err("dev_id(%u) is invalid\n", dev_id);
		goto p_err;
	}

	if (bus_ops_list[dev_id]) {
		ret = -EINVAL;
		dsp_err("bus ops is already registered(%u)\n", dev_id);
		__dsp_bus_dump_ops_list();
		goto p_err;
	}

	ret = 0;
	check = (void **)ops;
	for (idx = 0; idx < sizeof(*ops) / sizeof(void *); ++idx) {
		if (!check[idx]) {
			ret = -EFAULT;
			dsp_warn("bus ops[%zu] is NULL\n", idx);
		}
	}
	if (ret) {
		dsp_err("There should be no space in bus ops\n");
		goto p_err;
	}

	bus_ops_list[dev_id] = ops;
	dsp_leave();
	return 0;
p_err:
	return ret;
}
