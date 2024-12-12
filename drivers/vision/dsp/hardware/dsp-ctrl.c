// SPDX-License-Identifier: GPL-2.0
/*
 * Samsung Exynos SoC series dsp driver
 *
 * Copyright (c) 2019 Samsung Electronics Co., Ltd.
 *              http://www.samsung.com/
 */

#include <dt-bindings/soc/samsung/exynos-dsp.h>

#include "dsp-log.h"
#include "hardware/dsp-ctrl.h"

static const struct dsp_ctrl_ops *ctrl_ops_list[DSP_DEVICE_ID_END];
static struct dsp_ctrl *static_ctrl;

unsigned int dsp_ctrl_remap_readl(unsigned int addr)
{
	dsp_check();
	return static_ctrl->ops->remap_readl(static_ctrl, addr);
}

int dsp_ctrl_remap_writel(unsigned int addr, int val)
{
	dsp_check();
	return static_ctrl->ops->remap_writel(static_ctrl, addr, val);
}

unsigned int dsp_ctrl_sm_readl(unsigned int reg_addr)
{
	dsp_check();
	return static_ctrl->ops->sm_readl(static_ctrl, reg_addr);
}

int dsp_ctrl_sm_writel(unsigned int reg_addr, int val)
{
	dsp_check();
	return static_ctrl->ops->sm_writel(static_ctrl, reg_addr, val);
}

unsigned int dsp_ctrl_dhcp_readl(unsigned int reg_addr)
{
	dsp_check();
	return static_ctrl->ops->dhcp_readl(static_ctrl, reg_addr);
}

int dsp_ctrl_dhcp_writel(unsigned int reg_addr, int val)
{
	dsp_check();
	return static_ctrl->ops->dhcp_writel(static_ctrl, reg_addr, val);
}

unsigned int dsp_ctrl_offset_readl(unsigned int reg_id, unsigned int offset)
{
	dsp_check();
	return static_ctrl->ops->offset_readl(static_ctrl, reg_id, offset);
}

int dsp_ctrl_offset_writel(unsigned int reg_id, unsigned int offset, int val)
{
	dsp_check();
	return static_ctrl->ops->offset_writel(static_ctrl, reg_id, offset,
			val);
}

unsigned int dsp_ctrl_readl(unsigned int reg_id)
{
	dsp_check();
	return static_ctrl->ops->offset_readl(static_ctrl, reg_id, 0);
}

int dsp_ctrl_writel(unsigned int reg_id, int val)
{
	dsp_check();
	return static_ctrl->ops->offset_writel(static_ctrl, reg_id, 0, val);
}

void dsp_ctrl_reg_print(unsigned int reg_id)
{
	dsp_enter();
	static_ctrl->ops->reg_print(static_ctrl, reg_id);
	dsp_leave();
}

void dsp_ctrl_pc_dump(void)
{
	dsp_enter();
	static_ctrl->ops->pc_dump(static_ctrl);
	dsp_leave();
}

void dsp_ctrl_dhcp_dump(void)
{
	dsp_enter();
	static_ctrl->ops->dhcp_dump(static_ctrl);
	dsp_leave();
}

void dsp_ctrl_userdefined_dump(void)
{
	dsp_enter();
	static_ctrl->ops->userdefined_dump(static_ctrl);
	dsp_leave();
}

void dsp_ctrl_fw_info_dump(void)
{
	dsp_enter();
	static_ctrl->ops->fw_info_dump(static_ctrl);
	dsp_leave();
}

void dsp_ctrl_dump(void)
{
	dsp_enter();
	static_ctrl->ops->dump(static_ctrl);
	dsp_leave();
}

void dsp_ctrl_user_reg_print(struct seq_file *file, unsigned int reg_id)
{
	dsp_enter();
	static_ctrl->ops->user_reg_print(static_ctrl, file, reg_id);
	dsp_leave();
}

void dsp_ctrl_user_pc_dump(struct seq_file *file)
{
	dsp_enter();
	static_ctrl->ops->user_pc_dump(static_ctrl, file);
	dsp_leave();
}

void dsp_ctrl_user_dhcp_dump(struct seq_file *file)
{
	dsp_enter();
	static_ctrl->ops->user_dhcp_dump(static_ctrl, file);
	dsp_leave();
}

void dsp_ctrl_user_userdefined_dump(struct seq_file *file)
{
	dsp_enter();
	static_ctrl->ops->user_userdefined_dump(static_ctrl, file);
	dsp_leave();
}

void dsp_ctrl_user_fw_info_dump(struct seq_file *file)
{
	dsp_enter();
	static_ctrl->ops->user_fw_info_dump(static_ctrl, file);
	dsp_leave();
}

void dsp_ctrl_user_dump(struct seq_file *file)
{
	dsp_enter();
	static_ctrl->ops->user_dump(static_ctrl, file);
	dsp_leave();
}

static void __dsp_ctrl_dump_ops_list(void)
{
	size_t idx;

	dsp_enter();
	for (idx = 0; idx < ARRAY_SIZE(ctrl_ops_list); ++idx) {
		if (ctrl_ops_list[idx])
			dsp_warn("ctrl ops[%zu] registered\n", idx);
		else
			dsp_warn("ctrl ops[%zu] None\n", idx);
	}
	dsp_leave();
}

static int __dsp_ctrl_check_ops(unsigned int dev_id)
{
	int ret;

	dsp_enter();
	if (dev_id >= ARRAY_SIZE(ctrl_ops_list)) {
		ret = -EINVAL;
		dsp_err("dev_id(%u) is invalid\n", dev_id);
		goto p_err;
	}

	if (!ctrl_ops_list[dev_id]) {
		ret = -EINVAL;
		dsp_err("ctrl ops is not registered(%u)\n", dev_id);
		__dsp_ctrl_dump_ops_list();
		goto p_err;
	}

	dsp_leave();
	return 0;
p_err:
	return ret;
}

int dsp_ctrl_set_ops(struct dsp_ctrl *ctrl, unsigned int dev_id)
{
	int ret;

	dsp_enter();
	ret = __dsp_ctrl_check_ops(dev_id);
	if (ret)
		goto p_err;

	ctrl->ops = ctrl_ops_list[dev_id];
	static_ctrl = ctrl;
	dsp_leave();
	return 0;
p_err:
	return ret;
}

int dsp_ctrl_register_ops(unsigned int dev_id, const struct dsp_ctrl_ops *ops)
{
	int ret;
	size_t idx;
	void **check;

	dsp_enter();
	if (dev_id >= ARRAY_SIZE(ctrl_ops_list)) {
		ret = -EINVAL;
		dsp_err("dev_id(%u) is invalid\n", dev_id);
		goto p_err;
	}

	if (ctrl_ops_list[dev_id]) {
		ret = -EINVAL;
		dsp_err("ctrl ops is already registered(%u)\n", dev_id);
		__dsp_ctrl_dump_ops_list();
		goto p_err;
	}

	ret = 0;
	check = (void **)ops;
	for (idx = 0; idx < sizeof(*ops) / sizeof(void *); ++idx) {
		if (!check[idx]) {
			ret = -EFAULT;
			dsp_warn("ctrl ops[%zu] is NULL\n", idx);
		}
	}
	if (ret) {
		dsp_err("There should be no space in ctrl ops\n");
		goto p_err;
	}

	ctrl_ops_list[dev_id] = ops;
	dsp_leave();
	return 0;
p_err:
	return ret;
}
