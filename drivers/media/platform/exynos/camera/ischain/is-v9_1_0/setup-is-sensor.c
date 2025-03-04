// SPDX-License-Identifier: GPL-2.0
/*
 * Samsung Exynos SoC series Pablo driver
 *
 * gpio and clock configurations
 *
 * Copyright (c) 2020 Samsung Electronics Co., Ltd
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
#ifdef CONFIG_OF
#include <linux/of_gpio.h>
#endif

#include <exynos-is.h>
#include <exynos-is-sensor.h>
#include <exynos-is-module.h>

static int exynos991_is_csi_gate(struct device *dev, u32 instance, bool mask)
{
	int ret = 0;

	pr_debug("%s(instance : %d / mask : %d)\n", __func__, instance, mask);

	switch (instance) {
	case 0:
		if (mask)
			is_disable(dev, "GATE_MIPI_PHY_LINK_WRAP_QCH_CSIS0");
		else
			is_enable(dev, "GATE_MIPI_PHY_LINK_WRAP_QCH_CSIS0");
		break;
	case 1:
		if (mask)
			is_disable(dev, "GATE_MIPI_PHY_LINK_WRAP_QCH_CSIS1");
		else
			is_enable(dev, "GATE_MIPI_PHY_LINK_WRAP_QCH_CSIS1");
		break;
	case 2:
		if (mask)
			is_disable(dev, "GATE_MIPI_PHY_LINK_WRAP_QCH_CSIS2");
		else
			is_enable(dev, "GATE_MIPI_PHY_LINK_WRAP_QCH_CSIS2");
		break;
	case 3:
		if (mask)
			is_disable(dev, "GATE_MIPI_PHY_LINK_WRAP_QCH_CSIS3");
		else
			is_enable(dev, "GATE_MIPI_PHY_LINK_WRAP_QCH_CSIS3");
		break;
	case 4:
		if (mask)
			is_disable(dev, "GATE_MIPI_PHY_LINK_WRAP_QCH_CSIS4");
		else
			is_enable(dev, "GATE_MIPI_PHY_LINK_WRAP_QCH_CSIS4");
		break;
	case 5:
		if (mask)
			is_disable(dev, "GATE_MIPI_PHY_LINK_WRAP_QCH_CSIS5");
		else
			is_enable(dev, "GATE_MIPI_PHY_LINK_WRAP_QCH_CSIS5");
		break;
	default:
		pr_err("(%s) instance is invalid(%d)\n", __func__, instance);
		ret = -EINVAL;
		break;
	}

	return ret;
}

int exynos991_is_sensor_iclk_cfg(struct device *dev,
	u32 scenario,
	u32 channel)
{
	is_enable(dev, "GATE_MIPI_PHY_LINK_WRAP_QCH_CSIS0");
	is_enable(dev, "GATE_MIPI_PHY_LINK_WRAP_QCH_CSIS1");
	is_enable(dev, "GATE_MIPI_PHY_LINK_WRAP_QCH_CSIS2");
	is_enable(dev, "GATE_MIPI_PHY_LINK_WRAP_QCH_CSIS3");
	is_enable(dev, "GATE_MIPI_PHY_LINK_WRAP_QCH_CSIS4");
	is_enable(dev, "GATE_MIPI_PHY_LINK_WRAP_QCH_CSIS5");
#if IS_ENABLED(CONFIG_SOC_EXYNOS2100_EVT0)
	is_enable(dev, "GATE_CSISX6_QCH_CSIS_DMA");
#else
	is_enable(dev, "GATE_CSISX6_QCH_DMA");
#endif

	return  0;
}

int exynos991_is_sensor_iclk_on(struct device *dev,
	u32 scenario,
	u32 channel)
{
	int ret = 0;

	switch (channel) {
	case 0:
		exynos991_is_csi_gate(dev, 1, true);
		exynos991_is_csi_gate(dev, 2, true);
		exynos991_is_csi_gate(dev, 3, true);
		exynos991_is_csi_gate(dev, 4, true);
		exynos991_is_csi_gate(dev, 5, true);
		break;
	case 1:
		exynos991_is_csi_gate(dev, 0, true);
		exynos991_is_csi_gate(dev, 2, true);
		exynos991_is_csi_gate(dev, 3, true);
		exynos991_is_csi_gate(dev, 4, true);
		exynos991_is_csi_gate(dev, 5, true);
		break;
	case 2:
		exynos991_is_csi_gate(dev, 0, true);
		exynos991_is_csi_gate(dev, 1, true);
		exynos991_is_csi_gate(dev, 3, true);
		exynos991_is_csi_gate(dev, 4, true);
		exynos991_is_csi_gate(dev, 5, true);
		break;
	case 3:
		exynos991_is_csi_gate(dev, 0, true);
		exynos991_is_csi_gate(dev, 1, true);
		exynos991_is_csi_gate(dev, 2, true);
		exynos991_is_csi_gate(dev, 4, true);
		exynos991_is_csi_gate(dev, 5, true);
		break;
	case 4:
		exynos991_is_csi_gate(dev, 0, true);
		exynos991_is_csi_gate(dev, 1, true);
		exynos991_is_csi_gate(dev, 2, true);
		exynos991_is_csi_gate(dev, 3, true);
		exynos991_is_csi_gate(dev, 5, true);
		break;
	case 5:
		exynos991_is_csi_gate(dev, 0, true);
		exynos991_is_csi_gate(dev, 1, true);
		exynos991_is_csi_gate(dev, 2, true);
		exynos991_is_csi_gate(dev, 3, true);
		exynos991_is_csi_gate(dev, 4, true);
		break;
	default:
		pr_err("channel is invalid(%d)\n", channel);
		ret = -EINVAL;
		goto p_err;
	}

p_err:
	return ret;
}

int exynos991_is_sensor_iclk_off(struct device *dev,
	u32 scenario,
	u32 channel)
{
	exynos991_is_csi_gate(dev, channel, true);

#if IS_ENABLED(CONFIG_SOC_EXYNOS2100_EVT0)
	is_disable(dev, "GATE_CSISX6_QCH_CSIS_DMA");
#else
	is_disable(dev, "GATE_CSISX6_QCH_DMA");
#endif

	return 0;
}

int exynos991_is_sensor_mclk_on(struct device *dev,
	u32 scenario,
	u32 channel)
{
	char clk_name[30];

	pr_debug("%s(scenario : %d / ch : %d)\n", __func__, scenario, channel);

	snprintf(clk_name, sizeof(clk_name), "CIS_CLK%d", channel);
	is_enable(dev, clk_name);
	is_set_rate(dev, clk_name, 26 * 1000000);

	snprintf(clk_name, sizeof(clk_name), "GATE_DFTMUX_CMU_QCH_CIS_CLK%d", channel);
	is_enable(dev, clk_name);

	return 0;
}

int exynos991_is_sensor_mclk_off(struct device *dev,
		u32 scenario,
		u32 channel)
{
	char clk_name[30];

	pr_debug("%s(scenario : %d / ch : %d)\n", __func__, scenario, channel);

	snprintf(clk_name, sizeof(clk_name), "GATE_DFTMUX_CMU_QCH_CIS_CLK%d", channel);
	is_disable(dev, clk_name);

	snprintf(clk_name, sizeof(clk_name), "CIS_CLK%d", channel);
	is_disable(dev, clk_name);

	return 0;
}

int exynos_is_sensor_iclk_cfg(struct device *dev,
	u32 scenario,
	u32 channel)
{
	exynos991_is_sensor_iclk_cfg(dev, scenario, channel);
	return 0;
}

int exynos_is_sensor_iclk_on(struct device *dev,
	u32 scenario,
	u32 channel)
{
	exynos991_is_sensor_iclk_on(dev, scenario, channel);
	return 0;
}

int exynos_is_sensor_iclk_off(struct device *dev,
	u32 scenario,
	u32 channel)
{
	exynos991_is_sensor_iclk_off(dev, scenario, channel);
	return 0;
}

int exynos_is_sensor_mclk_on(struct device *dev,
	u32 scenario,
	u32 channel)
{
	exynos991_is_sensor_mclk_on(dev, scenario, channel);
	return 0;
}

int exynos_is_sensor_mclk_off(struct device *dev,
	u32 scenario,
	u32 channel)
{
	exynos991_is_sensor_mclk_off(dev, scenario, channel);
	return 0;
}

int is_sensor_mclk_force_off(struct device *dev, u32 channel)
{
	char clk_name[30];

#ifndef CONFIG_CAMERA_USU_V03
	return 0;
#endif

	snprintf(clk_name, sizeof(clk_name), "GATE_DFTMUX_CMU_QCH_CIS_CLK%d", channel);
	is_enabled_clk_disable(dev, clk_name);

	snprintf(clk_name, sizeof(clk_name), "CIS_CLK%d", channel);
	is_enabled_clk_disable(dev, clk_name);

	return 0;
}
