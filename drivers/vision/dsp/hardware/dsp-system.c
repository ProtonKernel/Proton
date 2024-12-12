// SPDX-License-Identifier: GPL-2.0
/*
 * Samsung Exynos SoC series dsp driver
 *
 * Copyright (c) 2019 Samsung Electronics Co., Ltd.
 *              http://www.samsung.com/
 */

#include <dt-bindings/soc/samsung/exynos-dsp.h>

#include "dsp-log.h"
#include "hardware/dsp-system.h"

static const struct dsp_system_ops *sys_ops_list[DSP_DEVICE_ID_END];

static void __dsp_system_dump_ops_list(void)
{
	size_t idx;

	dsp_enter();
	for (idx = 0; idx < ARRAY_SIZE(sys_ops_list); ++idx) {
		if (sys_ops_list[idx])
			dsp_warn("system ops[%zu] registered\n", idx);
		else
			dsp_warn("system ops[%zu] None\n", idx);
	}
	dsp_leave();
}

static int __dsp_system_check_ops(unsigned int dev_id)
{
	int ret;

	dsp_enter();
	if (dev_id >= ARRAY_SIZE(sys_ops_list)) {
		ret = -EINVAL;
		dsp_err("dev_id(%u) is invalid\n", dev_id);
		goto p_err;
	}

	if (!sys_ops_list[dev_id]) {
		ret = -EINVAL;
		dsp_err("system ops is not registered(%u)\n", dev_id);
		__dsp_system_dump_ops_list();
		goto p_err;
	}

	dsp_leave();
	return 0;
p_err:
	return ret;
}

static int __dsp_system_set_ops(struct dsp_system *sys, unsigned int dev_id)
{
	int ret;

	dsp_enter();
	ret = __dsp_system_check_ops(dev_id);
	if (ret)
		goto p_err;

	sys->ops = sys_ops_list[dev_id];
	dsp_leave();
	return 0;
p_err:
	return ret;
}

int dsp_system_set_ops(struct dsp_system *sys, unsigned int dev_id)
{
	int ret;

	dsp_enter();
	ret = __dsp_system_set_ops(sys, dev_id);
	if (ret)
		goto p_err;

	ret = dsp_pm_set_ops(&sys->pm, dev_id);
	if (ret)
		goto p_err;

	ret = dsp_clk_set_ops(&sys->clk, dev_id);
	if (ret)
		goto p_err;

	ret = dsp_bus_set_ops(&sys->bus, dev_id);
	if (ret)
		goto p_err;

	ret = dsp_llc_set_ops(&sys->llc, dev_id);
	if (ret)
		goto p_err;

	ret = dsp_memory_set_ops(&sys->memory, dev_id);
	if (ret)
		goto p_err;

	ret = dsp_interface_set_ops(&sys->interface, dev_id);
	if (ret)
		goto p_err;

	ret = dsp_ctrl_set_ops(&sys->ctrl, dev_id);
	if (ret)
		goto p_err;

	ret = dsp_mailbox_set_ops(&sys->mailbox, dev_id);
	if (ret)
		goto p_err;

	ret = dsp_governor_set_ops(&sys->governor, dev_id);
	if (ret)
		goto p_err;

	ret = dsp_hw_debug_set_ops(&sys->debug, dev_id);
	if (ret)
		goto p_err;

	ret = dsp_dump_set_ops(&sys->dump, dev_id);
	if (ret)
		goto p_err;

	dsp_leave();
	return 0;
p_err:
	return ret;
}

int dsp_system_register_ops(unsigned int dev_id,
		const struct dsp_system_ops *ops)
{
	int ret;
	size_t idx;
	void **check;

	dsp_enter();
	if (dev_id >= ARRAY_SIZE(sys_ops_list)) {
		ret = -EINVAL;
		dsp_err("dev_id(%u) is invalid\n", dev_id);
		goto p_err;
	}

	if (sys_ops_list[dev_id]) {
		ret = -EINVAL;
		dsp_err("system ops is already registered(%u)\n", dev_id);
		__dsp_system_dump_ops_list();
		goto p_err;
	}

	ret = 0;
	check = (void **)ops;
	for (idx = 0; idx < sizeof(*ops) / sizeof(void *); ++idx) {
		if (!check[idx]) {
			ret = -EFAULT;
			dsp_warn("system ops[%zu] is NULL\n", idx);
		}
	}
	if (ret) {
		dsp_err("There should be no space in system ops\n");
		goto p_err;
	}

	sys_ops_list[dev_id] = ops;
	dsp_leave();
	return 0;
p_err:
	return ret;
}

#include "hardware/O1/dsp-hw-o1-system.h"

static int (*system_init[])(void) = {
	dsp_hw_o1_system_init,
};

int dsp_system_init(void)
{
	int ret;
	size_t idx;

	dsp_enter();
	for (idx = 0; idx < ARRAY_SIZE(system_init); ++idx) {
		ret = system_init[idx]();
		if (ret)
			dsp_err("Failed to init system[%zu](%d)\n", idx, ret);
	}
	dsp_leave();
	return 0;
}
