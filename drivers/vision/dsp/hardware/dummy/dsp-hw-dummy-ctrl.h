/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Samsung Exynos SoC series dsp driver
 *
 * Copyright (c) 2020 Samsung Electronics Co., Ltd.
 *              http://www.samsung.com/
 */

#ifndef __HW_DUMMY_DSP_HW_DUMMY_CTRL_H__
#define __HW_DUMMY_DSP_HW_DUMMY_CTRL_H__

#include "hardware/dsp-ctrl.h"

unsigned int dsp_hw_dummy_ctrl_remap_readl(struct dsp_ctrl *ctrl,
		unsigned int addr);
int dsp_hw_dummy_ctrl_remap_writel(struct dsp_ctrl *ctrl, unsigned int addr,
		int val);
unsigned int dsp_hw_dummy_ctrl_sm_readl(struct dsp_ctrl *ctrl,
		unsigned int reg_addr);
int dsp_hw_dummy_ctrl_sm_writel(struct dsp_ctrl *ctrl, unsigned int reg_addr,
		int val);
unsigned int dsp_hw_dummy_ctrl_offset_readl(struct dsp_ctrl *ctrl,
		unsigned int reg_id, unsigned int offset);
int dsp_hw_dummy_ctrl_offset_writel(struct dsp_ctrl *ctrl, unsigned int reg_id,
		unsigned int offset, int val);

void dsp_hw_dummy_ctrl_reg_print(struct dsp_ctrl *ctrl, unsigned int reg_id);
void dsp_hw_dummy_ctrl_dump(struct dsp_ctrl *ctrl);
void dsp_hw_dummy_ctrl_pc_dump(struct dsp_ctrl *ctrl);
void dsp_hw_dummy_ctrl_reserved_sm_dump(struct dsp_ctrl *ctrl);
void dsp_hw_dummy_ctrl_userdefined_dump(struct dsp_ctrl *ctrl);
void dsp_hw_dummy_ctrl_fw_info_dump(struct dsp_ctrl *ctrl);

void dsp_hw_dummy_ctrl_user_reg_print(struct dsp_ctrl *ctrl,
		struct seq_file *file, unsigned int reg_id);
void dsp_hw_dummy_ctrl_user_dump(struct dsp_ctrl *ctrl, struct seq_file *file);
void dsp_hw_dummy_ctrl_user_pc_dump(struct dsp_ctrl *ctrl,
		struct seq_file *file);
void dsp_hw_dummy_ctrl_user_reserved_sm_dump(struct dsp_ctrl *ctrl,
		struct seq_file *file);
void dsp_hw_dummy_ctrl_user_userdefined_dump(struct dsp_ctrl *ctrl,
		struct seq_file *file);
void dsp_hw_dummy_ctrl_user_fw_info_dump(struct dsp_ctrl *ctrl,
		struct seq_file *file);

int dsp_hw_dummy_ctrl_common_init(struct dsp_ctrl *ctrl);
int dsp_hw_dummy_ctrl_extra_init(struct dsp_ctrl *ctrl);
int dsp_hw_dummy_ctrl_all_init(struct dsp_ctrl *ctrl);
int dsp_hw_dummy_ctrl_start(struct dsp_ctrl *ctrl);
int dsp_hw_dummy_ctrl_reset(struct dsp_ctrl *ctrl);
int dsp_hw_dummy_ctrl_force_reset(struct dsp_ctrl *ctrl);

int dsp_hw_dummy_ctrl_open(struct dsp_ctrl *ctrl);
int dsp_hw_dummy_ctrl_close(struct dsp_ctrl *ctrl);
int dsp_hw_dummy_ctrl_probe(struct dsp_system *sys);
void dsp_hw_dummy_ctrl_remove(struct dsp_ctrl *ctrl);

#endif
