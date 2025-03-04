// SPDX-License-Identifier: GPL-2.0
/*
 * Samsung Exynos SoC series dsp driver
 *
 * Copyright (c) 2019 Samsung Electronics Co., Ltd.
 *              http://www.samsung.com/
 */

#include <dt-bindings/soc/samsung/exynos-dsp.h>

#include "dsp-log.h"
#include "hardware/dsp-mailbox.h"

static const struct dsp_mailbox_ops *mbox_ops_list[DSP_DEVICE_ID_END];

struct dsp_mailbox_pool *dsp_mailbox_alloc_pool(struct dsp_mailbox *mbox,
		unsigned int size)
{
	dsp_check();
	return mbox->ops->alloc_pool(mbox, size);
}

void dsp_mailbox_free_pool(struct dsp_mailbox_pool *pool)
{
	struct dsp_mailbox *mbox = pool->owner->mbox;

	dsp_enter();
	mbox->ops->free_pool(mbox, pool);
	dsp_leave();
}

void dsp_mailbox_dump_pool(struct dsp_mailbox_pool *pool)
{
	struct dsp_mailbox *mbox = pool->owner->mbox;

	dsp_enter();
	mbox->ops->dump_pool(mbox, pool);
	dsp_leave();
}

static void __dsp_mailbox_dump_ops_list(void)
{
	size_t idx;

	dsp_enter();
	for (idx = 0; idx < ARRAY_SIZE(mbox_ops_list); ++idx) {
		if (mbox_ops_list[idx])
			dsp_warn("mailbox ops[%zu] registered\n", idx);
		else
			dsp_warn("mailbox ops[%zu] None\n", idx);
	}
	dsp_leave();
}

static int __dsp_mailbox_check_ops(unsigned int dev_id)
{
	int ret;

	dsp_enter();
	if (dev_id >= ARRAY_SIZE(mbox_ops_list)) {
		ret = -EINVAL;
		dsp_err("dev_id(%u) is invalid\n", dev_id);
		goto p_err;
	}

	if (!mbox_ops_list[dev_id]) {
		ret = -EINVAL;
		dsp_err("mailbox ops is not registered(%u)\n", dev_id);
		__dsp_mailbox_dump_ops_list();
		goto p_err;
	}

	dsp_leave();
	return 0;
p_err:
	return ret;
}

int dsp_mailbox_set_ops(struct dsp_mailbox *mbox, unsigned int dev_id)
{
	int ret;

	dsp_enter();
	ret = __dsp_mailbox_check_ops(dev_id);
	if (ret)
		goto p_err;

	mbox->ops = mbox_ops_list[dev_id];
	dsp_leave();
	return 0;
p_err:
	return ret;
}

int dsp_mailbox_register_ops(unsigned int dev_id,
		const struct dsp_mailbox_ops *ops)
{
	int ret;
	size_t idx;
	void **check;

	dsp_enter();
	if (dev_id >= ARRAY_SIZE(mbox_ops_list)) {
		ret = -EINVAL;
		dsp_err("dev_id(%u) is invalid\n", dev_id);
		goto p_err;
	}

	if (mbox_ops_list[dev_id]) {
		ret = -EINVAL;
		dsp_err("mailbox ops is already registered(%u)\n", dev_id);
		__dsp_mailbox_dump_ops_list();
		goto p_err;
	}

	ret = 0;
	check = (void **)ops;
	for (idx = 0; idx < sizeof(*ops) / sizeof(void *); ++idx) {
		if (!check[idx]) {
			ret = -EFAULT;
			dsp_warn("mailbox ops[%zu] is NULL\n", idx);
		}
	}
	if (ret) {
		dsp_err("There should be no space in mailbox ops\n");
		goto p_err;
	}

	mbox_ops_list[dev_id] = ops;
	dsp_leave();
	return 0;
p_err:
	return ret;
}
