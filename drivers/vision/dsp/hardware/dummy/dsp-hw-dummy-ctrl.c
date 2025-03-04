// SPDX-License-Identifier: GPL-2.0
/*
 * Samsung Exynos SoC series dsp driver
 *
 * Copyright (c) 2020 Samsung Electronics Co., Ltd.
 *              http://www.samsung.com/
 */

#include "dsp-log.h"
#include "dsp-hw-dummy-system.h"
#include "dsp-hw-dummy-ctrl.h"

unsigned int dsp_hw_dummy_ctrl_remap_readl(struct dsp_ctrl *ctrl,
		unsigned int addr)
{
	dsp_enter();
	dsp_leave();
	return 0;
}

int dsp_hw_dummy_ctrl_remap_writel(struct dsp_ctrl *ctrl, unsigned int addr,
		int val)
{
	dsp_enter();
	dsp_leave();
	return 0;
}

unsigned int dsp_hw_dummy_ctrl_sm_readl(struct dsp_ctrl *ctrl,
		unsigned int reg_addr)
{
	dsp_enter();
	dsp_leave();
	return 0;
}

int dsp_hw_dummy_ctrl_sm_writel(struct dsp_ctrl *ctrl, unsigned int reg_addr,
		int val)
{
	dsp_enter();
	dsp_leave();
	return 0;
}

unsigned int dsp_hw_dummy_ctrl_offset_readl(struct dsp_ctrl *ctrl,
		unsigned int reg_id, unsigned int offset)
{
	dsp_enter();
	dsp_leave();
	return 0;
}

int dsp_hw_dummy_ctrl_offset_writel(struct dsp_ctrl *ctrl, unsigned int reg_id,
		unsigned int offset, int val)
{
	dsp_enter();
	dsp_leave();
	return 0;
}

void dsp_hw_dummy_ctrl_reg_print(struct dsp_ctrl *ctrl, unsigned int reg_id)
{
	dsp_enter();
	dsp_leave();
}

void dsp_hw_dummy_ctrl_dump(struct dsp_ctrl *ctrl)
{
	dsp_enter();
	dsp_leave();
}

void dsp_hw_dummy_ctrl_pc_dump(struct dsp_ctrl *ctrl)
{
	dsp_enter();
	dsp_leave();
}

void dsp_hw_dummy_ctrl_reserved_sm_dump(struct dsp_ctrl *ctrl)
{
	dsp_enter();
	dsp_leave();
}

void dsp_hw_dummy_ctrl_userdefined_dump(struct dsp_ctrl *ctrl)
{
	dsp_enter();
	dsp_leave();
}

void dsp_hw_dummy_ctrl_fw_info_dump(struct dsp_ctrl *ctrl)
{
	dsp_enter();
	dsp_leave();
}

void dsp_hw_dummy_ctrl_user_reg_print(struct dsp_ctrl *ctrl,
		struct seq_file *file, unsigned int reg_id)
{
	dsp_enter();
	dsp_leave();
}

void dsp_hw_dummy_ctrl_user_dump(struct dsp_ctrl *ctrl, struct seq_file *file)
{
	dsp_enter();
	dsp_leave();
}

void dsp_hw_dummy_ctrl_user_pc_dump(struct dsp_ctrl *ctrl,
		struct seq_file *file)
{
	dsp_enter();
	dsp_leave();
}

void dsp_hw_dummy_ctrl_user_reserved_sm_dump(struct dsp_ctrl *ctrl,
		struct seq_file *file)
{
	dsp_enter();
	dsp_leave();
}

void dsp_hw_dummy_ctrl_user_userdefined_dump(struct dsp_ctrl *ctrl,
		struct seq_file *file)
{
	dsp_enter();
	dsp_leave();
}

void dsp_hw_dummy_ctrl_user_fw_info_dump(struct dsp_ctrl *ctrl,
		struct seq_file *file)
{
	dsp_enter();
	dsp_leave();
}

int dsp_hw_dummy_ctrl_common_init(struct dsp_ctrl *ctrl)
{
	dsp_enter();
	dsp_leave();
	return 0;
}

int dsp_hw_dummy_ctrl_extra_init(struct dsp_ctrl *ctrl)
{
	dsp_enter();
	dsp_leave();
	return 0;
}

int dsp_hw_dummy_ctrl_all_init(struct dsp_ctrl *ctrl)
{
	dsp_enter();
	dsp_leave();
	return 0;
}

int dsp_hw_dummy_ctrl_start(struct dsp_ctrl *ctrl)
{
	dsp_enter();
	dsp_leave();
	return 0;
}

int dsp_hw_dummy_ctrl_reset(struct dsp_ctrl *ctrl)
{
	dsp_enter();
	dsp_leave();
	return 0;
}

int dsp_hw_dummy_ctrl_force_reset(struct dsp_ctrl *ctrl)
{
	dsp_enter();
	dsp_leave();
	return 0;
}

int dsp_hw_dummy_ctrl_open(struct dsp_ctrl *ctrl)
{
	dsp_enter();
	dsp_leave();
	return 0;
}

int dsp_hw_dummy_ctrl_close(struct dsp_ctrl *ctrl)
{
	dsp_enter();
	dsp_leave();
	return 0;
}

int dsp_hw_dummy_ctrl_probe(struct dsp_system *sys)
{
	dsp_enter();
	dsp_leave();
	return 0;
}

void dsp_hw_dummy_ctrl_remove(struct dsp_ctrl *ctrl)
{
	dsp_enter();
	dsp_leave();
}
