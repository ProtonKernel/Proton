/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Samsung Exynos SoC series dsp driver
 *
 * Copyright (c) 2019 Samsung Electronics Co., Ltd.
 *              http://www.samsung.com/
 */

#ifndef __HW_DSP_CTRL_H__
#define __HW_DSP_CTRL_H__

#include <linux/seq_file.h>

struct dsp_system;
struct dsp_ctrl;

struct dsp_ctrl_ops {
	unsigned int (*remap_readl)(struct dsp_ctrl *ctrl, unsigned int addr);
	int (*remap_writel)(struct dsp_ctrl *ctrl, unsigned int addr, int val);
	unsigned int (*sm_readl)(struct dsp_ctrl *ctrl, unsigned int reg_addr);
	int (*sm_writel)(struct dsp_ctrl *ctrl, unsigned int reg_addr, int val);
	unsigned int (*dhcp_readl)(struct dsp_ctrl *ctrl,
			unsigned int reg_addr);
	int (*dhcp_writel)(struct dsp_ctrl *ctrl, unsigned int reg_addr,
			int val);
	unsigned int (*offset_readl)(struct dsp_ctrl *ctrl, unsigned int reg_id,
			unsigned int offset);
	int (*offset_writel)(struct dsp_ctrl *ctrl, unsigned int reg_id,
			unsigned int offset, int val);

	void (*reg_print)(struct dsp_ctrl *ctrl, unsigned int reg_id);
	void (*dump)(struct dsp_ctrl *ctrl);
	void (*pc_dump)(struct dsp_ctrl *ctrl);
	void (*dhcp_dump)(struct dsp_ctrl *ctrl);
	void (*userdefined_dump)(struct dsp_ctrl *ctrl);
	void (*fw_info_dump)(struct dsp_ctrl *ctrl);

	void (*user_reg_print)(struct dsp_ctrl *ctrl, struct seq_file *file,
			unsigned int reg_id);
	void (*user_dump)(struct dsp_ctrl *ctrl, struct seq_file *file);
	void (*user_pc_dump)(struct dsp_ctrl *ctrl, struct seq_file *file);
	void (*user_dhcp_dump)(struct dsp_ctrl *ctrl, struct seq_file *file);
	void (*user_userdefined_dump)(struct dsp_ctrl *ctrl,
			struct seq_file *file);
	void (*user_fw_info_dump)(struct dsp_ctrl *ctrl, struct seq_file *file);

	int (*common_init)(struct dsp_ctrl *ctrl);
	int (*extra_init)(struct dsp_ctrl *ctrl);
	int (*all_init)(struct dsp_ctrl *ctrl);
	int (*start)(struct dsp_ctrl *ctrl);
	int (*reset)(struct dsp_ctrl *ctrl);
	int (*force_reset)(struct dsp_ctrl *ctrl);

	int (*open)(struct dsp_ctrl *ctrl);
	int (*close)(struct dsp_ctrl *ctrl);
	int (*probe)(struct dsp_system *sys);
	void (*remove)(struct dsp_ctrl *ctrl);
};

struct dsp_ctrl {
	struct device			*dev;
	phys_addr_t			sfr_pa;
	void __iomem			*sfr;

	const struct dsp_ctrl_ops	*ops;
	struct dsp_system		*sys;
};

unsigned int dsp_ctrl_remap_readl(unsigned int addr);
int dsp_ctrl_remap_writel(unsigned int addr, int val);
unsigned int dsp_ctrl_sm_readl(unsigned int reg_addr);
int dsp_ctrl_sm_writel(unsigned int reg_addr, int val);
unsigned int dsp_ctrl_dhcp_readl(unsigned int reg_addr);
int dsp_ctrl_dhcp_writel(unsigned int reg_addr, int val);
unsigned int dsp_ctrl_offset_readl(unsigned int reg_id, unsigned int offset);
int dsp_ctrl_offset_writel(unsigned int reg_id, unsigned int offset, int val);
unsigned int dsp_ctrl_readl(unsigned int reg_id);
int dsp_ctrl_writel(unsigned int reg_id, int val);

void dsp_ctrl_reg_print(unsigned int reg_id);
void dsp_ctrl_pc_dump(void);
void dsp_ctrl_dhcp_dump(void);
void dsp_ctrl_userdefined_dump(void);
void dsp_ctrl_fw_info_dump(void);
void dsp_ctrl_dump(void);

void dsp_ctrl_user_reg_print(struct seq_file *file, unsigned int reg_id);
void dsp_ctrl_user_pc_dump(struct seq_file *file);
void dsp_ctrl_user_dhcp_dump(struct seq_file *file);
void dsp_ctrl_user_userdefined_dump(struct seq_file *file);
void dsp_ctrl_user_fw_info_dump(struct seq_file *file);
void dsp_ctrl_user_dump(struct seq_file *file);

int dsp_ctrl_set_ops(struct dsp_ctrl *ctrl, unsigned int dev_id);
int dsp_ctrl_register_ops(unsigned int dev_id, const struct dsp_ctrl_ops *ops);

#endif
