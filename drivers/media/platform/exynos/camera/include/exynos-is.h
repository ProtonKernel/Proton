/*
 * Samsung Exynos SoC series Pablo driver
 *
 *
 * Copyright (c) 2019 Samsung Electronics Co., Ltd
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#ifndef EXYNOS_IS_H_
#define EXYNOS_IS_H_

#include <linux/platform_device.h>
#include <linux/clk-provider.h>
#include "is-hw-dvfs.h"

#define MHZ (1000 * 1000)

extern int debug_clk;

struct is_clk {
	const char *name;
	struct clk *clk;
};

enum IS_DVFS_QOS_TYPE {
	IS_DVFS_CPU_MIN,
	IS_DVFS_CPU_MAX,
	IS_DVFS_INT_CAM,
	IS_DVFS_INT,
	IS_DVFS_MIF,
	IS_DVFS_I2C,
	IS_DVFS_CAM,
	IS_DVFS_DISP,
	IS_DVFS_HPG,
	IS_DVFS_TNR,
	IS_DVFS_CSIS,
	IS_DVFS_ISP,
	IS_DVFS_END,
};

enum IS_CLK_GATE {
	IS_GATE_3AA1_IP,
	IS_GATE_ISP_IP,
	IS_GATE_DRC_IP,
	IS_GATE_SCC_IP,
	IS_GATE_ODC_IP,
	IS_GATE_DIS_IP,
	IS_GATE_3DNR_IP,
	IS_GATE_SCP_IP,
	IS_GATE_FD_IP,
	IS_GATE_3AA0_IP,
	IS_GATE_ISP1_IP,
	IS_GATE_TPU_IP,
	IS_GATE_VRA_IP,
	IS_CLK_GATE_MAX,
};

enum IS_GRP {
	IS_GRP_3A0,
	IS_GRP_3A1,
	IS_GRP_ISP,
	IS_GRP_DIS,
	IS_GRP_MAX,
};

enum IS_CLK_GATE_USR_SCENARIO {
	CLK_GATE_NOT_FULL_BYPASS_SN = 1,
	CLK_GATE_FULL_BYPASS_SN,
	CLK_GATE_DIS_SN,
};

/*
 * struct exynos_is_clk_gate_group
 *	This struct is for host clock gating.
 * 	It decsribes register, masking bit info and other control for each group.
 *	If you uses host clock gating, You must define this struct in exynos_is_clk_gate_info.
 */
struct exynos_is_clk_gate_group {
	u32	mask_clk_on_org;	/* masking value in clk on */
	u32	mask_clk_on_mod;	/* masking value in clk on */
	u32	mask_clk_off_self_org;	/* masking value in clk off(not depend) original */
	u32	mask_clk_off_self_mod;	/* masking value in clk off(not depend) final */
	u32	mask_clk_off_depend;	/* masking value in clk off(depend on other grp) */
	u32	mask_cond_for_depend;	/* masking group having dependancy for other */
};

/*
 * struct exynos_is_clk_gate_info
 * 	This struct is for host clock gating.
 * 	It has exynos_is_clk_gate_group to control each group's clk gating.
 * 	And it has function pointer to include user scenario masking
 */
struct exynos_is_clk_gate_info {
	const char *gate_str[IS_CLK_GATE_MAX];			/* register adr for gating */
	struct exynos_is_clk_gate_group groups[IS_GRP_MAX];
	/* You must set this function pointer (on/off) */
	int (*clk_on_off)(u32 clk_gate_id, bool is_on);
	/*
	 * if there are specific scenarios for clock gating,
	 * You can define user function.
	 * user_scenario_id will be in
	 */
	int (*user_clk_gate)(u32 group_id,
			bool is_on,
			u32 user_scenario_id,
			unsigned long msk_state,
			struct exynos_is_clk_gate_info *gate_info);
};

struct is_num_of_ip {
	unsigned int taa;
	unsigned int isp;
	unsigned int mcsc;
	unsigned int vra;
	unsigned int clh;
	unsigned int ypp;
	unsigned int lme;
};
/**
* struct exynos_platform_is - camera host interface platform data
*
* @isp_info: properties of camera sensor required for host interface setup
*/
struct exynos_platform_is {
	int hw_ver;
	bool clock_on;
	int (*clk_get)(struct device *dev);
	int (*clk_cfg)(struct device *dev);
	int (*clk_on)(struct device *dev);
	int (*clk_off)(struct device *dev);
	int (*print_clk)(struct device *dev);
	int (*print_cfg)(struct device *dev, u32 channel);
	struct pinctrl *pinctrl;
	/* These fields are to return qos value for dvfs scenario */
	u32 dvfs_data[IS_DVFS_TABLE_IDX_MAX][IS_SN_END][IS_DVFS_END];
	const char *dvfs_cpu[IS_DVFS_TABLE_IDX_MAX][IS_SN_END];

	/* For host clock gating */
	struct exynos_is_clk_gate_info *gate_info;

	/* number of available IP */
	struct is_num_of_ip num_of_ip;

	/* memlogger size : [0] Driver log buffer size, [1] DDK log buffer size */
	u32 is_memlog_size[2];
};

extern struct device *is_dev;

int is_set_parent_dt(struct device *dev, const char *child, const char *parent);
int is_set_rate_dt(struct device *dev, const char *conid, unsigned int rate);
ulong is_get_rate_dt(struct device *dev, const char *conid);
ulong is_dump_rate_dt(struct device *dev, const char *conid);
int is_enable_dt(struct device *dev, const char *conid);
int is_disable_dt(struct device *dev, const char *conid);

int is_set_rate(struct device *dev, const char *name, ulong frequency);
ulong is_get_rate(struct device *dev, const char *conid);
ulong is_get_hw_rate(struct device *dev, const char *name);
int is_enable(struct device *dev, const char *conid);
int is_disable(struct device *dev, const char *conid);
int is_enabled_clk_disable(struct device *dev, const char *conid);

/* platform specific clock functions */
int exynos_is_clk_get(struct device *dev);
int exynos_is_clk_cfg(struct device *dev);
int exynos_is_clk_on(struct device *dev);
int exynos_is_clk_off(struct device *dev);
int exynos_is_clk_get_rate(struct device *dev);
int exynos_is_print_clk(struct device *dev);
int exynos_is_clk_gate(u32 clk_gate_id, bool is_on);
int exynos_is_dump_clk(struct device *dev);

#endif /* EXYNOS_IS_H_ */
