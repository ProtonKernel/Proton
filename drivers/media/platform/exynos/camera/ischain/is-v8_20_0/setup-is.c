/*
 * Samsung Exynos SoC series Pablo driver
 *
 * Exynos9630  gpio and clock configurations
 *
 * Copyright (c) 2019 Samsung Electronics Co., Ltd
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#include <linux/kernel.h>
#include <linux/types.h>
#include <linux/gpio.h>
#include <linux/clk.h>
#include <linux/err.h>
#include <linux/io.h>
#include <linux/regulator/consumer.h>
#include <linux/delay.h>
#include <linux/clk-provider.h>
#include <linux/clkdev.h>

#include <exynos-is.h>
#include <is-config.h>

#if defined(ENABLE_CLOG_RESERVED_MEM)
#include "is-resourcemgr.h"
#endif

/*------------------------------------------------------*/
/*		Common control				*/
/*------------------------------------------------------*/

#define REGISTER_CLK(name) {name, NULL}

struct is_clk is_clk_list[] = {
	REGISTER_CLK("IS_BUS"),
	REGISTER_CLK("IS_ITP"),
	REGISTER_CLK("IS_GDC"),
	REGISTER_CLK("IS_VRA"),
	REGISTER_CLK("MFCMSCL_MCSC"),

	REGISTER_CLK("UMUX_CLK_IS_BUS"),
	REGISTER_CLK("UMUX_CLK_IS_ITP"),
	REGISTER_CLK("UMUX_CLK_IS_VRA"),
	REGISTER_CLK("UMUX_CLK_IS_GDC"),

	REGISTER_CLK("GATE_CLK_ITP"),
	REGISTER_CLK("GATE_CLK_VRA"),
	REGISTER_CLK("GATE_CLK_GDC"),
	REGISTER_CLK("GATE_MCSC_QCH"),

	REGISTER_CLK("GATE_CSIS0_QCH"),
	REGISTER_CLK("GATE_CSIS1_QCH"),
	REGISTER_CLK("GATE_CSIS2_QCH"),

	REGISTER_CLK("GATE_DFTMUX_CMU_QCH_CLK_CIS0"),
	REGISTER_CLK("GATE_DFTMUX_CMU_QCH_CLK_CIS1"),
	REGISTER_CLK("GATE_DFTMUX_CMU_QCH_CLK_CIS2"),

	REGISTER_CLK("CIS_CLK0"),
	REGISTER_CLK("CIS_CLK1"),
	REGISTER_CLK("CIS_CLK2"),
};

int is_set_rate(struct device *dev,
	const char *name, ulong frequency)
{
	int ret = 0;
	size_t index;
	struct clk *clk = NULL;

	for (index = 0; index < ARRAY_SIZE(is_clk_list); index++) {
		if (!strcmp(name, is_clk_list[index].name))
			clk = is_clk_list[index].clk;
	}

	if (IS_ERR_OR_NULL(clk)) {
		pr_err("[@][ERR] %s: clk_target_list is NULL : %s\n", __func__, name);
		return -EINVAL;
	}

	ret = clk_set_rate(clk, frequency);
	if (ret) {
		pr_err("[@][ERR] %s: clk_set_rate is fail(%s)(ret: %d)\n", __func__, name, ret);
		return ret;
	}

	/* is_get_rate_dt(dev, name); */

	return ret;
}

/* utility function to get rate with DT */
ulong is_get_rate(struct device *dev,
	const char *name)
{
	ulong frequency;
	size_t index;
	struct clk *clk = NULL;

	for (index = 0; index < ARRAY_SIZE(is_clk_list); index++) {
		if (!strcmp(name, is_clk_list[index].name))
			clk = is_clk_list[index].clk;
	}

	if (IS_ERR_OR_NULL(clk)) {
		pr_err("[@][ERR] %s: clk_target_list is NULL : %s\n", __func__, name);
		return 0;
	}

	frequency = clk_get_rate(clk);

	pr_info("[@] %s : %ldMhz (enable_count : %d)\n", name, frequency/1000000, __clk_get_enable_count(clk));

	return frequency;
}

/**
 * is_get_hw_rate - Return the lately calculated clock rate
 *		that is stored in clk_core structure.
 * @dev: device this lookup is bound
 * @name: format string describing clock name
 *
 * Just return the lately calculated clock rate that is stored in clk_hw structure.
 * It does not guarantee that has currently working clock rate.
 * To get it, call 'is_get_rate()' or 'is_get_rate_dt()' in advance.
 */
ulong is_get_hw_rate(struct device *dev, const char *name)
{
	size_t index;
	struct clk *clk = NULL;

	for (index = 0; index < ARRAY_SIZE(is_clk_list); index++) {
		if (!strcmp(name, is_clk_list[index].name)) {
			clk = is_clk_list[index].clk;
			break;
		}
	}

	if (IS_ERR_OR_NULL(clk)) {
		pr_err("[@][ERR] %s: clk_target_list is NULL : %s\n", __func__, name);
		return 0;
	}

	return clk_hw_get_rate(__clk_get_hw(clk));
}

/* utility function to eable with DT */
int  is_enable(struct device *dev,
	const char *name)
{
	int ret = 0;
	size_t index;
	struct clk *clk = NULL;

	for (index = 0; index < ARRAY_SIZE(is_clk_list); index++) {
		if (!strcmp(name, is_clk_list[index].name))
			clk = is_clk_list[index].clk;
	}

	if (IS_ERR_OR_NULL(clk)) {
		pr_err("[@][ERR] %s: clk_target_list is NULL : %s\n", __func__, name);
		return -EINVAL;
	}

	if (debug_clk > 1)
		pr_info("[@][ENABLE] %s : (enable_count : %d)\n", name, __clk_get_enable_count(clk));

	ret = clk_prepare_enable(clk);
	if (ret) {
		pr_err("[@][ERR] %s: clk_prepare_enable is fail(%s)(ret: %d)\n", __func__, name, ret);
		return ret;
	}

	return ret;
}

/* utility function to disable with DT */
int is_disable(struct device *dev,
	const char *name)
{
	size_t index;
	struct clk *clk = NULL;

	for (index = 0; index < ARRAY_SIZE(is_clk_list); index++) {
		if (!strcmp(name, is_clk_list[index].name))
			clk = is_clk_list[index].clk;
	}

	if (IS_ERR_OR_NULL(clk)) {
		pr_err("[@][ERR] %s: clk_target_list is NULL : %s\n", __func__, name);
		return -EINVAL;
	}

	clk_disable_unprepare(clk);

	if (debug_clk > 1)
		pr_info("[@][DISABLE] %s : (enable_count : %d)\n", name, __clk_get_enable_count(clk));

	return 0;
}

/* utility function to set parent with DT */
int is_set_parent_dt(struct device *dev,
	const char *child, const char *parent)
{
	int ret = 0;
	struct clk *p;
	struct clk *c;

	p = clk_get(dev, parent);
	if (IS_ERR_OR_NULL(p)) {
		pr_err("%s: could not lookup clock : %s\n", __func__, parent);
		return -EINVAL;
	}

	c = clk_get(dev, child);
	if (IS_ERR_OR_NULL(c)) {
		clk_put(p);
		pr_err("%s: could not lookup clock : %s\n", __func__, child);
		return -EINVAL;
	}

	ret = clk_set_parent(c, p);
	if (ret) {
		clk_put(c);
		clk_put(p);
		pr_err("%s: clk_set_parent is fail(%s -> %s)(ret: %d)\n", __func__, child, parent, ret);
		return ret;
	}

	clk_put(c);
	clk_put(p);

	return 0;
}

/* utility function to set rate with DT */
int is_set_rate_dt(struct device *dev,
	const char *conid, unsigned int rate)
{
	int ret = 0;
	struct clk *target;

	target = clk_get(dev, conid);
	if (IS_ERR_OR_NULL(target)) {
		pr_err("%s: could not lookup clock : %s\n", __func__, conid);
		return -EINVAL;
	}

	ret = clk_set_rate(target, rate);
	if (ret) {
		pr_err("%s: clk_set_rate is fail(%s)(ret: %d)\n", __func__, conid, ret);
		return ret;
	}

	/* is_get_rate_dt(dev, conid); */

	clk_put(target);

	return 0;
}

/* utility function to get rate with DT */
ulong is_get_rate_dt(struct device *dev,
	const char *conid)
{
	struct clk *target;
	ulong rate_target;

	target = clk_get(dev, conid);
	if (IS_ERR_OR_NULL(target)) {
		pr_err("%s: could not lookup clock : %s\n", __func__, conid);
		return 0;
	}

	rate_target = clk_get_rate(target);

	clk_put(target);

	pr_info("[@] %s : %ldMhz\n", conid, rate_target/1000000);

	return rate_target;
}

/* utility function to eable with DT */
int is_enable_dt(struct device *dev,
	const char *conid)
{
	int ret;
	struct clk *target;

	target = clk_get(dev, conid);
	if (IS_ERR_OR_NULL(target)) {
		pr_err("%s: could not lookup clock : %s\n", __func__, conid);
		return -EINVAL;
	}

	ret = clk_prepare(target);
	if (ret) {
		pr_err("%s: clk_prepare is fail(%s)(ret: %d)\n", __func__, conid, ret);
		return ret;
	}

	ret = clk_enable(target);
	if (ret) {
		pr_err("%s: clk_enable is fail(%s)(ret: %d)\n", __func__, conid, ret);
		return ret;
	}

	clk_put(target);

	return 0;
}

/* utility function to disable with DT */
int is_disable_dt(struct device *dev,
	const char *conid)
{
	struct clk *target;

	target = clk_get(dev, conid);
	if (IS_ERR_OR_NULL(target)) {
		pr_err("%s: could not lookup clock : %s\n", __func__, conid);
		return -EINVAL;
	}

	clk_disable(target);
	clk_unprepare(target);
	clk_put(target);

	return 0;
}

static void exynos3830_is_print_clk_reg(void)
{

}

static int exynos3830_is_print_clk(struct device *dev)
{
	pr_info("FIMC-IS CLOCK DUMP\n");

	is_get_rate_dt(dev, "IS_BUS");
	is_get_rate_dt(dev, "IS_ITP");
	is_get_rate_dt(dev, "IS_GDC");
	is_get_rate_dt(dev, "IS_VRA");
	is_get_rate_dt(dev, "MFCMSCL_MCSC");

	is_get_rate_dt(dev, "UMUX_CLK_IS_BUS");
	is_get_rate_dt(dev, "UMUX_CLK_IS_ITP");
	is_get_rate_dt(dev, "UMUX_CLK_IS_VRA");
	is_get_rate_dt(dev, "UMUX_CLK_IS_GDC");

	is_get_rate_dt(dev, "GATE_CLK_ITP");
	is_get_rate_dt(dev, "GATE_CLK_VRA");
	is_get_rate_dt(dev, "GATE_CLK_GDC");
	is_get_rate_dt(dev, "GATE_MCSC_QCH");

	is_get_rate_dt(dev, "GATE_CSIS0_QCH");
	is_get_rate_dt(dev, "GATE_CSIS1_QCH");
	is_get_rate_dt(dev, "GATE_CSIS2_QCH");

	is_get_rate_dt(dev, "GATE_DFTMUX_CMU_QCH_CLK_CIS0");
	is_get_rate_dt(dev, "GATE_DFTMUX_CMU_QCH_CLK_CIS1");
	is_get_rate_dt(dev, "GATE_DFTMUX_CMU_QCH_CLK_CIS2");

	is_get_rate_dt(dev, "CIS_CLK0");
	is_get_rate_dt(dev, "CIS_CLK1");
	is_get_rate_dt(dev, "CIS_CLK2");

	exynos3830_is_print_clk_reg();

	return 0;
}

#if defined(ENABLE_CLOG_RESERVED_MEM)
/* utility function to get rate with DT */
ulong is_dump_rate_dt(struct device *dev,
	const char *conid)
{
	struct clk *target;
	ulong rate_target;

	target = clk_get(dev, conid);
	if (IS_ERR_OR_NULL(target)) {
		pr_err("%s: could not lookup clock : %s\n", __func__, conid);
		return -EINVAL;
	}

	rate_target = clk_get_rate(target);

	clk_put(target);

	cinfo("[@] %s : %ldMhz\n", conid, rate_target/1000000);
	return rate_target;
}

int exynos3830_is_dump_clk(struct device *dev)
{

	cinfo("### FIMC-IS CLOCK DUMP ###\n");

	is_dump_rate_dt(dev, "IS_BUS");
	is_dump_rate_dt(dev, "IS_ITP");
	is_dump_rate_dt(dev, "IS_GDC");
	is_dump_rate_dt(dev, "IS_VRA");
	is_dump_rate_dt(dev, "MFCMSCL_MCSC");

	is_dump_rate_dt(dev, "UMUX_CLK_IS_BUS");
	is_dump_rate_dt(dev, "UMUX_CLK_IS_ITP");
	is_dump_rate_dt(dev, "UMUX_CLK_IS_VRA");
	is_dump_rate_dt(dev, "UMUX_CLK_IS_GDC");

	is_dump_rate_dt(dev, "GATE_CLK_ITP");
	is_dump_rate_dt(dev, "GATE_CLK_VRA");
	is_dump_rate_dt(dev, "GATE_CLK_GDC");
	is_dump_rate_dt(dev, "GATE_MCSC_QCH");

	is_dump_rate_dt(dev, "GATE_CSIS0_QCH");
	is_dump_rate_dt(dev, "GATE_CSIS1_QCH");
	is_dump_rate_dt(dev, "GATE_CSIS2_QCH");

	is_dump_rate_dt(dev, "GATE_DFTMUX_CMU_QCH_CLK_CIS0");
	is_dump_rate_dt(dev, "GATE_DFTMUX_CMU_QCH_CLK_CIS1");
	is_dump_rate_dt(dev, "GATE_DFTMUX_CMU_QCH_CLK_CIS2");

	is_dump_rate_dt(dev, "CIS_CLK0");
	is_dump_rate_dt(dev, "CIS_CLK1");
	is_dump_rate_dt(dev, "CIS_CLK2");

	return 0;
}
#endif

int exynos3830_is_clk_gate(u32 clk_gate_id, bool is_on)
{
	return 0;
}

int exynos3830_is_uart_gate(struct device *dev, bool mask)
{
	return 0;
}

int exynos3830_is_get_clk(struct device *dev)
{
	struct clk *clk;
	size_t i;

	for (i = 0; i < ARRAY_SIZE(is_clk_list); i++) {
		clk = devm_clk_get(dev, is_clk_list[i].name);
		if (IS_ERR_OR_NULL(clk)) {
			pr_err("[@][ERR] %s: could not lookup clock : %s\n",
				__func__, is_clk_list[i].name);
			return -EINVAL;
		}

		is_clk_list[i].clk = clk;
	}

	return 0;
}

int exynos3830_is_cfg_clk(struct device *dev)
{
	return 0;
}

int exynos3830_is_clk_on(struct device *dev)
{
	int ret = 0;
	struct exynos_platform_is *pdata;

	pdata = dev_get_platdata(dev);
	if (pdata->clock_on) {
		ret = pdata->clk_off(dev);
		if (ret) {
			pr_err("clk_off is fail(%d)\n", ret);
			goto p_err;
		}
	}

	is_enable(dev, "IS_BUS");
	is_enable(dev, "IS_ITP");
	is_enable(dev, "IS_GDC");
	is_enable(dev, "IS_VRA");
	is_enable(dev, "MFCMSCL_MCSC");

	is_enable(dev, "UMUX_CLK_IS_BUS");
	is_enable(dev, "UMUX_CLK_IS_ITP");
	is_enable(dev, "UMUX_CLK_IS_VRA");
	is_enable(dev, "UMUX_CLK_IS_GDC");

	is_enable(dev, "GATE_CLK_ITP");
	is_enable(dev, "GATE_CLK_VRA");
	is_enable(dev, "GATE_CLK_GDC");
	is_enable(dev, "GATE_MCSC_QCH");

	if (debug_clk > 1)
		exynos3830_is_print_clk(dev);

	pdata->clock_on = true;

p_err:
	return 0;
}

int exynos3830_is_clk_off(struct device *dev)
{
	int ret = 0;
	struct exynos_platform_is *pdata;

	pdata = dev_get_platdata(dev);
	if (!pdata->clock_on) {
		pr_err("clk_off is fail(already off)\n");
		ret = -EINVAL;
		goto p_err;
	}

	is_disable(dev, "IS_BUS");
	is_disable(dev, "IS_ITP");
	is_disable(dev, "IS_GDC");
	is_disable(dev, "IS_VRA");
	is_disable(dev, "MFCMSCL_MCSC");

	is_disable(dev, "UMUX_CLK_IS_BUS");
	is_disable(dev, "UMUX_CLK_IS_ITP");
	is_disable(dev, "UMUX_CLK_IS_VRA");
	is_disable(dev, "UMUX_CLK_IS_GDC");

	is_disable(dev, "GATE_CLK_ITP");
	is_disable(dev, "GATE_CLK_VRA");
	is_disable(dev, "GATE_CLK_GDC");
	is_disable(dev, "GATE_MCSC_QCH");

	pdata->clock_on = false;

p_err:
	return 0;
}

/**
 * exynos3830_is_clk_get_rate - Call 'clk_get_rate()' for every clock
 * @dev: device that has clocks
 *
 * By calling 'clk_get_rate()' for each clock source,
 * it get rate changes via clock tree traversal.
 */
int exynos3830_is_clk_get_rate(struct device *dev)
{
	size_t index;

	for (index = 0; index < ARRAY_SIZE(is_clk_list); index++)
		clk_get_rate(is_clk_list[index].clk);

	return 0;
}

/* Wrapper functions */
int exynos_is_clk_get(struct device *dev)
{
	exynos3830_is_get_clk(dev);
	return 0;
}

int exynos_is_clk_cfg(struct device *dev)
{
	exynos3830_is_cfg_clk(dev);
	return 0;
}

int exynos_is_clk_on(struct device *dev)
{
	exynos3830_is_clk_on(dev);
	return 0;
}

int exynos_is_clk_off(struct device *dev)
{
	exynos3830_is_clk_off(dev);
	return 0;
}

int exynos_is_clk_get_rate(struct device *dev)
{
	exynos3830_is_clk_get_rate(dev);
	return 0;
}

int exynos_is_print_clk(struct device *dev)
{
	exynos3830_is_print_clk(dev);
	return 0;
}

int exynos_is_dump_clk(struct device *dev)
{
#if defined(ENABLE_CLOG_RESERVED_MEM)
	exynos3830_is_dump_clk(dev);
#endif
	return 0;
}

int exynos_is_set_user_clk_gate(u32 group_id, bool is_on,
	u32 user_scenario_id,
	unsigned long msk_state,
	struct exynos_is_clk_gate_info *gate_info)
{
	return 0;
}

int exynos_is_clk_gate(u32 clk_gate_id, bool is_on)
{
	exynos3830_is_clk_gate(clk_gate_id, is_on);
	return 0;
}
