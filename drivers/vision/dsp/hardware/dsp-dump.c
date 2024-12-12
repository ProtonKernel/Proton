// SPDX-License-Identifier: GPL-2.0
/*
 * Samsung Exynos SoC series dsp driver
 *
 * Copyright (c) 2019 Samsung Electronics Co., Ltd.
 *              http://www.samsung.com/
 */

#include <dt-bindings/soc/samsung/exynos-dsp.h>

#include "dsp-log.h"
#include "hardware/dsp-dump.h"

static const struct dsp_dump_ops *dump_ops_list[DSP_DEVICE_ID_END];
static struct dsp_dump *static_dump;

void dsp_dump_set_value(unsigned int dump_value)
{
	dsp_enter();
	static_dump->ops->set_value(static_dump, dump_value);
	dsp_leave();
}

void dsp_dump_print_value(void)
{
	dsp_enter();
	static_dump->ops->print_value(static_dump);
	dsp_leave();
}

void dsp_dump_print_status_user(struct seq_file *file)
{
	dsp_enter();
	static_dump->ops->print_status_user(static_dump, file);
	dsp_leave();
}

void dsp_dump_ctrl(void)
{
	dsp_enter();
	static_dump->ops->ctrl(static_dump);
	dsp_leave();
}

void dsp_dump_ctrl_user(struct seq_file *file)
{
	dsp_enter();
	static_dump->ops->ctrl_user(static_dump, file);
	dsp_leave();
}

void dsp_dump_mailbox_pool_error(struct dsp_mailbox_pool *pool)
{
	dsp_enter();
	static_dump->ops->mailbox_pool_error(static_dump, pool);
	dsp_leave();
}

void dsp_dump_mailbox_pool_debug(struct dsp_mailbox_pool *pool)
{
	dsp_enter();
	static_dump->ops->mailbox_pool_debug(static_dump, pool);
	dsp_leave();
}

void dsp_dump_task_manager_count(struct dsp_task_manager *tmgr)
{
	dsp_enter();
	static_dump->ops->task_manager_count(static_dump, tmgr);
	dsp_leave();
}

void dsp_dump_kernel(struct dsp_kernel_manager *kmgr)
{
	dsp_enter();
	static_dump->ops->kernel(static_dump, kmgr);
	dsp_leave();
}

static void __dsp_dump_dump_ops_list(void)
{
	size_t idx;

	dsp_enter();
	for (idx = 0; idx < ARRAY_SIZE(dump_ops_list); ++idx) {
		if (dump_ops_list[idx])
			dsp_warn("dump ops[%zu] registered\n", idx);
		else
			dsp_warn("dump ops[%zu] None\n", idx);
	}
	dsp_leave();
}

static int __dsp_dump_check_ops(unsigned int dev_id)
{
	int ret;

	dsp_enter();
	if (dev_id >= ARRAY_SIZE(dump_ops_list)) {
		ret = -EINVAL;
		dsp_err("dev_id(%u) is invalid\n", dev_id);
		goto p_err;
	}

	if (!dump_ops_list[dev_id]) {
		ret = -EINVAL;
		dsp_err("dump ops is not registered(%u)\n", dev_id);
		__dsp_dump_dump_ops_list();
		goto p_err;
	}

	dsp_leave();
	return 0;
p_err:
	return ret;
}

int dsp_dump_set_ops(struct dsp_dump *dump, unsigned int dev_id)
{
	int ret;

	dsp_enter();
	ret = __dsp_dump_check_ops(dev_id);
	if (ret)
		goto p_err;

	dump->ops = dump_ops_list[dev_id];
	static_dump = dump;
	dsp_leave();
	return 0;
p_err:
	return ret;
}

int dsp_dump_register_ops(unsigned int dev_id, const struct dsp_dump_ops *ops)
{
	int ret;
	size_t idx;
	void **check;

	dsp_enter();
	if (dev_id >= ARRAY_SIZE(dump_ops_list)) {
		ret = -EINVAL;
		dsp_err("dev_id(%u) is invalid\n", dev_id);
		goto p_err;
	}

	if (dump_ops_list[dev_id]) {
		ret = -EINVAL;
		dsp_err("dump ops is already registered(%u)\n", dev_id);
		__dsp_dump_dump_ops_list();
		goto p_err;
	}

	ret = 0;
	check = (void **)ops;
	for (idx = 0; idx < sizeof(*ops) / sizeof(void *); ++idx) {
		if (!check[idx]) {
			ret = -EFAULT;
			dsp_warn("dump ops[%zu] is NULL\n", idx);
		}
	}
	if (ret) {
		dsp_err("There should be no space in dump ops\n");
		goto p_err;
	}

	dump_ops_list[dev_id] = ops;
	dsp_leave();
	return 0;
p_err:
	return ret;
}
