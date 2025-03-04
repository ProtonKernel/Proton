/*
 * PCIe host controller driver for Samsung EXYNOS SoCs
 * Supporting PCIe Gen4
 *
 * Copyright (C) 2019 Samsung Electronics Co., Ltd.
 *		http://www.samsung.com
 *
 * Author: Kyounghye Yun <k-hye.yun@samsung.com>
 * Author: Seungo Jang <seungo.jang@samsung.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#include <linux/clk.h>
#include <linux/delay.h>
#include <linux/gpio.h>
#include <linux/interrupt.h>
#include <linux/kernel.h>
#include <linux/mfd/syscon.h>
#include <linux/module.h>
#include <linux/of_gpio.h>
#include <linux/of_address.h>
#include <linux/pci.h>
#include <linux/platform_device.h>
#include <linux/regmap.h>
#include <linux/resource.h>
#include <linux/signal.h>
#include <linux/types.h>
#include <linux/pm_qos.h>
#include <dt-bindings/pci/pci.h>
#include <linux/exynos-pci-noti.h>
#include <linux/exynos-pci-ctrl.h>
#if IS_ENABLED(CONFIG_EXYNOS_ITMON)
#include <soc/samsung/exynos-itmon.h>
#endif

#include <linux/pm_runtime.h>
#include <linux/kthread.h>
#include <linux/random.h>

#if IS_ENABLED(CONFIG_CPU_IDLE)
//#include <soc/samsung/exynos-powermode.h>
//#include <soc/samsung/exynos-pm.h>
#include <soc/samsung/exynos-cpupm.h>
#include <soc/samsung/exynos-pm.h>
#include <soc/samsung/exynos_pm_qos.h>
#endif
#ifdef CONFIG_SEC_PANIC_PCIE_ERR
#include <soc/samsung/exynos-modem-ctrl.h>
#endif
/* to be removed */
//#include <linux/shm_ipc.h>     /* for S5100 MSI target addr. set */
#if IS_ENABLED(CONFIG_LINK_DEVICE_PCIE)
#define MODIFY_MSI_ADDR
#endif	/* CONFIG_LINK_DEVICE_PCIE */

#include "pcie-designware.h"
#include "pcie-exynos-common.h"
#include "pcie-exynos-rc.h"
#include "pcie-exynos-dbg.h"

struct exynos_pcie g_pcie_rc[MAX_RC_NUM];
int pcie_is_linkup = 0;

static struct pci_dev *exynos_pcie_get_pci_dev(struct pcie_port *pp);
int exynos_pcie_rc_set_outbound_atu(int ch_num, u32 target_addr, u32 offset, u32 size);

#if IS_ENABLED(CONFIG_EXYNOS_PM_QOS)
static struct exynos_pm_qos_request exynos_pcie_int_qos[MAX_RC_NUM];
#endif

void exynos_pcie_set_perst_gpio(int ch_num, bool on)
{
	struct exynos_pcie *exynos_pcie = &g_pcie_rc[ch_num];

	if (exynos_pcie->ep_device_type == EP_SAMSUNG_MODEM) {
		pr_info("%s: force settig for abnormal state\n", __func__);
		if (on) {
			gpio_set_value(exynos_pcie->perst_gpio, 1);
			pr_info("%s: Set PERST to HIGH, gpio val = %d\n",
					__func__, gpio_get_value(exynos_pcie->perst_gpio));
		} else {
			gpio_set_value(exynos_pcie->perst_gpio, 0);
			pr_info("%s: Set PERST to LOW, gpio val = %d\n",
					__func__, gpio_get_value(exynos_pcie->perst_gpio));
		}
	}
}
EXPORT_SYMBOL(exynos_pcie_set_perst_gpio);

static ssize_t exynos_pcie_rc_show(struct device *dev,
		struct device_attribute *attr,
		char *buf)
{
	int ret = 0;
	ret += snprintf(buf + ret, PAGE_SIZE - ret, ">>>> PCIe Test <<<<\n");
	ret += snprintf(buf + ret, PAGE_SIZE - ret, "0 : PCIe Unit Test\n");
	ret += snprintf(buf + ret, PAGE_SIZE - ret, "1 : Link Test\n");
	ret += snprintf(buf + ret, PAGE_SIZE - ret, "2 : DisLink Test\n");

	return ret;
}
static ssize_t exynos_pcie_rc_store(struct device *dev,
		struct device_attribute *attr,
		const char *buf, size_t count)
{
	int op_num;
	struct exynos_pcie *exynos_pcie = dev_get_drvdata(dev);
	int ret = 0;

	if (sscanf(buf, "%10d", &op_num) == 0)
		return -EINVAL;
	switch (op_num) {
	case 0:
		dev_info(dev, "## PCIe UNIT test START ##\n");
		ret = exynos_pcie_dbg_unit_test(dev, exynos_pcie);
		if (ret) {
			dev_err(dev, "PCIe UNIT test failed (%d)\n", ret);
			break;
		}
		dev_err(dev, "## PCIe UNIT test SUCCESS!!##\n");
		break;
	case 1:
		dev_info(dev, "## PCIe establish link test ##\n");
		ret = exynos_pcie_dbg_link_test(dev, exynos_pcie, 1);
		if (ret) {
			dev_err(dev, "PCIe establish link test failed (%d)\n", ret);
			break;
		}
		dev_err(dev, "PCIe establish link test success\n");
		break;
	case 2:
		dev_info(dev, "## PCIe dis-link test ##\n");
		ret = exynos_pcie_dbg_link_test(dev, exynos_pcie, 0);
		if (ret) {
			dev_err(dev, "PCIe dis-link test failed (%d)\n", ret);
			break;
		}
		dev_err(dev, "PCIe dis-link test success\n");
		break;
	case 3:
		dev_info(dev, "## LTSSM ##\n");
		ret = exynos_elbi_read(exynos_pcie,
					PCIE_ELBI_RDLH_LINKUP) & 0xff;
		dev_info(dev, "PCIE_ELBI_RDLH_LINKUP :0x%x \n", ret);
		break;

	case 10:
		dev_info(dev, "L1.2 Disable....\n");
		exynos_pcie_rc_l1ss_ctrl(0, PCIE_L1SS_CTRL_TEST);
		break;

	case 11:
		dev_info(dev, "L1.2 Enable....\n");
		exynos_pcie_rc_l1ss_ctrl(1, PCIE_L1SS_CTRL_TEST);
		break;

	case 12:
		dev_info(dev, "l1ss_ctrl_id_state = 0x%08x\n",
				exynos_pcie->l1ss_ctrl_id_state);
		dev_info(dev, "LTSSM: 0x%08x, PM_STATE = 0x%08x\n",
				exynos_elbi_read(exynos_pcie, PCIE_ELBI_RDLH_LINKUP),
				exynos_phy_pcs_read(exynos_pcie, 0x188));
		break;

	case 13:
		dev_info(dev, "%s: force perst setting \n", __func__);
		exynos_pcie_set_perst_gpio(1, 0);

		break;

	case 16:
		exynos_pcie_rc_set_outbound_atu(1, 0x47200000, 0x0, SZ_1M);
		break;
	}

	return count;
}

static DEVICE_ATTR(pcie_rc_test, S_IWUSR | S_IWGRP | S_IRUSR | S_IRGRP,
			exynos_pcie_rc_show, exynos_pcie_rc_store);

static ssize_t exynos_pcie_eom1_show(struct device *dev,
		struct device_attribute *attr,
		char *buf)
{
	struct exynos_pcie *exynos_pcie = dev_get_drvdata(dev);
	struct pcie_eom_result **eom_result = exynos_pcie->eom_result;
	struct device_node *np = dev->of_node;
	int len = 0;
	u32 test_cnt = 0;
	static int current_cnt = 0;
	unsigned int lane_width = 1;
	int i = 0, ret;

	if (eom_result  == NULL) {
		len += snprintf(buf + len, PAGE_SIZE,
				"eom_result structure is NULL !!! \n");
		goto exit;
	}

	ret = of_property_read_u32(np, "num-lanes", &lane_width);
	if (ret) {
		lane_width = 0;
	}

	while (current_cnt != EOM_PH_SEL_MAX * EOM_DEF_VREF_MAX) {
		len += snprintf(buf + len, PAGE_SIZE,
				"%u %u %lu\n",
				eom_result[i][current_cnt].phase,
				eom_result[i][current_cnt].vref,
				eom_result[i][current_cnt].err_cnt);
		current_cnt++;
		test_cnt++;
		if (test_cnt == 100)
			break;
	}

	if (current_cnt == EOM_PH_SEL_MAX * EOM_DEF_VREF_MAX)
		current_cnt = 0;

exit:
	return len;
}

static ssize_t exynos_pcie_eom1_store(struct device *dev,
		struct device_attribute *attr,
		const char *buf, size_t count)
{
	int op_num;
	struct exynos_pcie *exynos_pcie = dev_get_drvdata(dev);

	if (sscanf(buf, "%10d", &op_num) == 0)
		return -EINVAL;
	switch (op_num) {
	case 0:
		if (exynos_pcie->phy_ops.phy_eom != NULL)
			exynos_pcie->phy_ops.phy_eom(dev,
					exynos_pcie->phy_base);
		break;
	}

	return count;
}

static ssize_t exynos_pcie_eom2_store(struct device *dev,
		struct device_attribute *attr,
		const char *buf, size_t count)
{
	/* prevent to print kerenl warning message
	   eom1_store function do all operation to get eom data */

	return count;
}

static ssize_t exynos_pcie_eom2_show(struct device *dev,
		struct device_attribute *attr,
		char *buf)
{
	struct exynos_pcie *exynos_pcie = dev_get_drvdata(dev);
	struct pcie_eom_result **eom_result = exynos_pcie->eom_result;
	struct device_node *np = dev->of_node;
	int len = 0;
	u32 test_cnt = 0;
	static int current_cnt = 0;
	unsigned int lane_width = 1;
	int i = 1, ret;

	if (eom_result  == NULL) {
		len += snprintf(buf + len, PAGE_SIZE,
				"eom_result structure is NULL !!! \n");
		goto exit;
	}

	ret = of_property_read_u32(np, "num-lanes", &lane_width);
	if (ret) {
		lane_width = 0;
		len += snprintf(buf + len, PAGE_SIZE,
				"can't get num of lanes !! \n");
		goto exit;
	}

	if (lane_width == 1) {
		len += snprintf(buf + len, PAGE_SIZE,
				"EOM2NULL\n");
		goto exit;
	}

	while (current_cnt != EOM_PH_SEL_MAX * EOM_DEF_VREF_MAX) {
		len += snprintf(buf + len, PAGE_SIZE,
				"%u %u %lu\n",
				eom_result[i][current_cnt].phase,
				eom_result[i][current_cnt].vref,
				eom_result[i][current_cnt].err_cnt);
		current_cnt++;
		test_cnt++;
		if (test_cnt == 100)
			break;
	}

	if (current_cnt == EOM_PH_SEL_MAX * EOM_DEF_VREF_MAX)
		current_cnt = 0;

exit:
	return len;
}

static DEVICE_ATTR(eom1, S_IWUSR | S_IWGRP | S_IRUSR | S_IRGRP,
			exynos_pcie_eom1_show, exynos_pcie_eom1_store);

static DEVICE_ATTR(eom2, S_IWUSR | S_IWGRP | S_IRUSR | S_IRGRP,
			exynos_pcie_eom2_show, exynos_pcie_eom2_store);

static inline int create_pcie_sys_file(struct device *dev)
{
	struct device_node *np = dev->of_node;
	int ret;
	int num_lane;

	ret = device_create_file(dev, &dev_attr_pcie_rc_test);
	if (ret) {
		dev_err(dev, "%s: couldn't create device file for test(%d)\n",
				__func__, ret);
		return ret;
	}

	ret = of_property_read_u32(np, "num-lanes", &num_lane);
	if (ret) {
		num_lane = 0;
	}

	ret = device_create_file(dev, &dev_attr_eom1);
	if (ret) {
		dev_err(dev, "%s: couldn't create device file for eom(%d)\n",
				__func__, ret);
		return ret;
	}

	if (num_lane > 0) {
		ret = device_create_file(dev, &dev_attr_eom2);
		if (ret) {
			dev_err(dev, "%s: couldn't create device file for eom(%d)\n",
					__func__, ret);
			return ret;
		}

	}

	return 0;
}

static inline void remove_pcie_sys_file(struct device *dev)
{
	device_remove_file(dev, &dev_attr_pcie_rc_test);
}

static int exynos_pcie_rc_clock_enable(struct pcie_port *pp, int enable)
{
	struct dw_pcie *pci = to_dw_pcie_from_pp(pp);
	struct exynos_pcie *exynos_pcie = to_exynos_pcie(pci);
	struct exynos_pcie_clks	*clks = &exynos_pcie->clks;
	int i;
	int ret = 0;

	if (enable) {
		for (i = 0; i < exynos_pcie->pcie_clk_num; i++) {
			ret = clk_prepare_enable(clks->pcie_clks[i]);
#ifdef CONFIG_SEC_PANIC_PCIE_ERR
			if (ret)
				panic("[PCIe RC Case#2] pcie clk fail! %s\n", exynos_pcie->ep_device_name);
#endif
		}
	} else {
		for (i = 0; i < exynos_pcie->pcie_clk_num; i++)
			clk_disable_unprepare(clks->pcie_clks[i]);
	}

	return ret;
}

static int exynos_pcie_rc_phy_clock_enable(struct pcie_port *pp, int enable)
{
	struct dw_pcie *pci = to_dw_pcie_from_pp(pp);
	struct exynos_pcie *exynos_pcie = to_exynos_pcie(pci);
	struct exynos_pcie_clks	*clks = &exynos_pcie->clks;
	int i;
	int ret = 0;

	if (enable) {
		for (i = 0; i < exynos_pcie->phy_clk_num; i++) {
			ret = clk_prepare_enable(clks->phy_clks[i]);
#ifdef CONFIG_SEC_PANIC_PCIE_ERR
			if (ret)
				panic("[PCIe RC Case#2] PHY clk fail! %s\n", exynos_pcie->ep_device_name);
#endif
		}
	} else {
		for (i = 0; i < exynos_pcie->phy_clk_num; i++)
			clk_disable_unprepare(clks->phy_clks[i]);
	}

	return ret;
}

static int exynos_pcie_rc_rd_own_conf(struct pcie_port *pp, int where, int size,
				u32 *val)
{
	struct dw_pcie *pci = to_dw_pcie_from_pp(pp);
	struct exynos_pcie *exynos_pcie = to_exynos_pcie(pci);
	int is_linked = 0;
	int ret = 0;
	u32 __maybe_unused reg_val;

	if (exynos_pcie->state == STATE_LINK_UP ||
			exynos_pcie->state == STATE_LINK_UP_TRY)
		is_linked = 1;

	if (is_linked == 0) {
		/* TCXO_FAR on */
		hsi_tcxo_far_control(2, 1);

		exynos_pcie_rc_clock_enable(pp, PCIE_ENABLE_CLOCK);
		exynos_pcie_rc_phy_clock_enable(pp, PCIE_ENABLE_CLOCK);
		/* set input clk path change to enable */
		if (exynos_pcie->phy_ops.phy_input_clk_change != NULL) {
			exynos_pcie->phy_ops.phy_input_clk_change(exynos_pcie, 1);
		}

		if (exynos_pcie->phy_ops.phy_check_rx_elecidle != NULL)
			exynos_pcie->phy_ops.phy_check_rx_elecidle(
				exynos_pcie->phy_pcs_base, IGNORE_ELECIDLE,
				exynos_pcie->ch_num);
	}

	ret = dw_pcie_read(exynos_pcie->rc_dbi_base + (where), size, val);

	if (is_linked == 0) {
		if (exynos_pcie->phy_ops.phy_check_rx_elecidle != NULL)
			exynos_pcie->phy_ops.phy_check_rx_elecidle(
				exynos_pcie->phy_pcs_base, ENABLE_ELECIDLE,
				exynos_pcie->ch_num);
		/* set input clk path change to disable */
		if (exynos_pcie->phy_ops.phy_input_clk_change != NULL) {
			exynos_pcie->phy_ops.phy_input_clk_change(exynos_pcie, 0);
		}

		exynos_pcie_rc_phy_clock_enable(pp, PCIE_DISABLE_CLOCK);
		exynos_pcie_rc_clock_enable(pp, PCIE_DISABLE_CLOCK);

		/* TCXO_FAR off */
		hsi_tcxo_far_control(2, 0);
	}

	return ret;
}

static int exynos_pcie_rc_wr_own_conf(struct pcie_port *pp, int where, int size,
				u32 val)
{
	struct dw_pcie *pci = to_dw_pcie_from_pp(pp);
	struct exynos_pcie *exynos_pcie = to_exynos_pcie(pci);
	int is_linked = 0;
	int ret = 0;
	u32 __maybe_unused reg_val;

	if (exynos_pcie->state == STATE_LINK_UP ||
			exynos_pcie->state == STATE_LINK_UP_TRY)
		is_linked = 1;

	if (is_linked == 0) {
		/* TCXO_FAR on */
		hsi_tcxo_far_control(2, 1);

		exynos_pcie_rc_clock_enable(pp, PCIE_ENABLE_CLOCK);
		exynos_pcie_rc_phy_clock_enable(pp, PCIE_ENABLE_CLOCK);
		/* set input clk path change to enable */
		if (exynos_pcie->phy_ops.phy_input_clk_change != NULL) {
			exynos_pcie->phy_ops.phy_input_clk_change(exynos_pcie, 1);
		}

		if (exynos_pcie->phy_ops.phy_check_rx_elecidle != NULL)
			exynos_pcie->phy_ops.phy_check_rx_elecidle(
				exynos_pcie->phy_pcs_base, IGNORE_ELECIDLE,
				exynos_pcie->ch_num);
	}

	ret = dw_pcie_write(exynos_pcie->rc_dbi_base + (where), size, val);

	if (is_linked == 0) {
		if (exynos_pcie->phy_ops.phy_check_rx_elecidle != NULL)
			exynos_pcie->phy_ops.phy_check_rx_elecidle(
				exynos_pcie->phy_pcs_base, ENABLE_ELECIDLE,
				exynos_pcie->ch_num);
		/* set input clk path change to disable */
		if (exynos_pcie->phy_ops.phy_input_clk_change != NULL) {
			exynos_pcie->phy_ops.phy_input_clk_change(exynos_pcie, 0);
		}

		exynos_pcie_rc_phy_clock_enable(pp, PCIE_DISABLE_CLOCK);
		exynos_pcie_rc_clock_enable(pp, PCIE_DISABLE_CLOCK);

		/* TCXO_FAR off */
		hsi_tcxo_far_control(2, 0);
	}

	return ret;
}

static void exynos_pcie_rc_prog_viewport_cfg0(struct pcie_port *pp, u32 busdev)
{
	struct dw_pcie *pci = to_dw_pcie_from_pp(pp);
	struct exynos_pcie *exynos_pcie = to_exynos_pcie(pci);

	/* Program viewport 0 : OUTBOUND : CFG0 */
	exynos_pcie_rc_wr_own_conf(pp, PCIE_ATU_LOWER_BASE_OUTBOUND0, 4, pp->cfg0_base);
	exynos_pcie_rc_wr_own_conf(pp, PCIE_ATU_UPPER_BASE_OUTBOUND0, 4,
					(pp->cfg0_base >> 32));
	exynos_pcie_rc_wr_own_conf(pp, PCIE_ATU_LIMIT_OUTBOUND0, 4,
					pp->cfg0_base + pp->cfg0_size - 1);
	exynos_pcie_rc_wr_own_conf(pp, PCIE_ATU_LOWER_TARGET_OUTBOUND0, 4, busdev);
	exynos_pcie_rc_wr_own_conf(pp, PCIE_ATU_UPPER_TARGET_OUTBOUND0, 4, 0);
	exynos_pcie_rc_wr_own_conf(pp, PCIE_ATU_CR1_OUTBOUND0, 4, EXYNOS_PCIE_ATU_TYPE_CFG0);
	exynos_pcie_rc_wr_own_conf(pp, PCIE_ATU_CR2_OUTBOUND0, 4, EXYNOS_PCIE_ATU_ENABLE);
	exynos_pcie->atu_ok = 1;
}

static void exynos_pcie_rc_prog_viewport_cfg1(struct pcie_port *pp, u32 busdev)
{
	/* Program viewport 1 : OUTBOUND : CFG1 */
	exynos_pcie_rc_wr_own_conf(pp, PCIE_ATU_CR1_OUTBOUND0, 4, EXYNOS_PCIE_ATU_TYPE_CFG1);
	exynos_pcie_rc_wr_own_conf(pp, PCIE_ATU_LOWER_BASE_OUTBOUND0, 4, pp->cfg1_base);
	exynos_pcie_rc_wr_own_conf(pp, PCIE_ATU_UPPER_BASE_OUTBOUND0, 4,
					(pp->cfg1_base >> 32));
	exynos_pcie_rc_wr_own_conf(pp, PCIE_ATU_LIMIT_OUTBOUND0, 4,
					pp->cfg1_base + pp->cfg1_size - 1);
	exynos_pcie_rc_wr_own_conf(pp, PCIE_ATU_LOWER_TARGET_OUTBOUND0, 4, busdev);
	exynos_pcie_rc_wr_own_conf(pp, PCIE_ATU_UPPER_TARGET_OUTBOUND0, 4, 0);
	exynos_pcie_rc_wr_own_conf(pp, PCIE_ATU_CR2_OUTBOUND0, 4, EXYNOS_PCIE_ATU_ENABLE);
}

static void exynos_pcie_rc_prog_viewport_mem_outbound(struct pcie_port *pp)
{
	/* Program viewport 0 : OUTBOUND : MEM */
	exynos_pcie_rc_wr_own_conf(pp, PCIE_ATU_CR1_OUTBOUND1, 4, EXYNOS_PCIE_ATU_TYPE_MEM);
	exynos_pcie_rc_wr_own_conf(pp, PCIE_ATU_LOWER_BASE_OUTBOUND1, 4, pp->mem_base);
	exynos_pcie_rc_wr_own_conf(pp, PCIE_ATU_UPPER_BASE_OUTBOUND1, 4,
					(pp->mem_base >> 32));
//@@	exynos_pcie_rc_wr_own_conf(pp, PCIE_ATU_LIMIT_OUTBOUND1, 4,
//@@				pp->mem_base + pp->mem_size - 1);
	exynos_pcie_rc_wr_own_conf(pp, PCIE_ATU_LIMIT_OUTBOUND1, 4,
				pp->mem_base + SZ_2M - 1);
	exynos_pcie_rc_wr_own_conf(pp, PCIE_ATU_LOWER_TARGET_OUTBOUND1, 4, pp->mem_bus_addr);
	exynos_pcie_rc_wr_own_conf(pp, PCIE_ATU_UPPER_TARGET_OUTBOUND1, 4,
					upper_32_bits(pp->mem_bus_addr));
	exynos_pcie_rc_wr_own_conf(pp, PCIE_ATU_CR2_OUTBOUND1, 4, EXYNOS_PCIE_ATU_ENABLE);

}


int exynos_pcie_rc_set_bar(int ch_num, u32 bar_num)
{
	struct exynos_pcie *exynos_pcie = &g_pcie_rc[ch_num];
	struct dw_pcie *pci = exynos_pcie->pci;
	struct pcie_port *pp = &pci->pp;
	struct pci_dev *ep_pci_dev;
	u32 val;

	pr_info("%s: +++\n", __func__);

	if (exynos_pcie->state == STATE_LINK_UP) {
		ep_pci_dev = exynos_pcie_get_pci_dev(pp);
	} else {
		pr_info("%s: PCIe link is not up\n", __func__);
		return -EPIPE;
	}

	/* EP BAR setup */
	ep_pci_dev->resource[bar_num].start = exynos_pcie->btl_target_addr + \
		exynos_pcie->btl_offset;
	ep_pci_dev->resource[bar_num].end = exynos_pcie->btl_target_addr + \
		exynos_pcie->btl_offset + exynos_pcie->btl_size;
	ep_pci_dev->resource[bar_num].flags = 0x82000000;
	//pci_update_resource(ep_pci_dev, bar_num); No EXPORT_SYMBOL

	pci_read_config_dword(ep_pci_dev, PCI_BASE_ADDRESS_0 + (bar_num * 0x4), &val);
	pr_info("%s: Check EP BAR[%d] = 0x%x\n", __func__, bar_num, val);

	pr_info("%s: ---\n", __func__);
	return 0;
}

int exynos_pcie_rc_set_outbound_atu(int ch_num, u32 target_addr, u32 offset, u32 size)
{
	struct exynos_pcie *exynos_pcie = &g_pcie_rc[ch_num];
	struct dw_pcie *pci = exynos_pcie->pci;
	struct pcie_port *pp = &pci->pp;
	u32 val;
	int ret;

	pr_info("%s: +++\n", __func__);

	exynos_pcie->btl_target_addr = target_addr;
	exynos_pcie->btl_offset = offset;
	exynos_pcie->btl_size = size;

	pr_info("%s: target_addr = 0x%x, offset = 0x%x, size = 0x%x\n", __func__,
			exynos_pcie->btl_target_addr,
			exynos_pcie->btl_offset,
			exynos_pcie->btl_size);

	/* Only for BTL */
	/* 0x1420_0000 ~ (size -1) */
	exynos_pcie_rc_wr_own_conf(pp, PCIE_ATU_CR1_OUTBOUND2, 4, EXYNOS_PCIE_ATU_TYPE_MEM);
	exynos_pcie_rc_wr_own_conf(pp, PCIE_ATU_LOWER_BASE_OUTBOUND2, 4,
			pp->mem_base + SZ_2M);
	exynos_pcie_rc_wr_own_conf(pp, PCIE_ATU_UPPER_BASE_OUTBOUND2, 4,
			((pp->mem_base + SZ_2M) >> 32));
	exynos_pcie_rc_wr_own_conf(pp, PCIE_ATU_LIMIT_OUTBOUND2, 4,
			pp->mem_base + SZ_2M + exynos_pcie->btl_size - 1);
	exynos_pcie_rc_wr_own_conf(pp, PCIE_ATU_LOWER_TARGET_OUTBOUND2, 4,
			exynos_pcie->btl_target_addr + exynos_pcie->btl_offset);
	exynos_pcie_rc_wr_own_conf(pp, PCIE_ATU_UPPER_TARGET_OUTBOUND2, 4, 0);
	exynos_pcie_rc_wr_own_conf(pp, PCIE_ATU_CR2_OUTBOUND2, 4, EXYNOS_PCIE_ATU_ENABLE);

	exynos_pcie_rc_rd_own_conf(pp, PCIE_ATU_CR1_OUTBOUND2, 4, &val);
	pr_info("%s:  PCIE_ATU_CR1_OUTBOUND2(0x400) = 0x%x\n", __func__, val);
	exynos_pcie_rc_rd_own_conf(pp, PCIE_ATU_LOWER_BASE_OUTBOUND2, 4, &val);
	pr_info("%s:  PCIE_ATU_LOWER_BASE_OUTBOUND2(0x408) = 0x%x\n", __func__, val);
	exynos_pcie_rc_rd_own_conf(pp, PCIE_ATU_UPPER_BASE_OUTBOUND2, 4, &val);
	pr_info("%s:  PCIE_ATU_UPPER_BASE_OUTBOUND2(0x40C) = 0x%x\n", __func__, val);
	exynos_pcie_rc_rd_own_conf(pp, PCIE_ATU_LIMIT_OUTBOUND2, 4, &val);
	pr_info("%s:  PCIE_ATU_LIMIT_OUTBOUND2(0x410) = 0x%x\n", __func__, val);
	exynos_pcie_rc_rd_own_conf(pp, PCIE_ATU_LOWER_TARGET_OUTBOUND2, 4, &val);
	pr_info("%s:  PCIE_ATU_LOWER_TARGET_OUTBOUND2(0x414) = 0x%x\n", __func__, val);
	exynos_pcie_rc_rd_own_conf(pp, PCIE_ATU_UPPER_TARGET_OUTBOUND2, 4, &val);
	pr_info("%s:  PCIE_ATU_UPPER_TARGET_OUTBOUND2(0x418) = 0x%x\n", __func__, val);
	exynos_pcie_rc_rd_own_conf(pp, PCIE_ATU_CR2_OUTBOUND2, 4, &val);
	pr_info("%s:  PCIE_ATU_CR2_OUTBOUND2(0x404) = 0x%x\n", __func__, val);

	ret = exynos_pcie_rc_set_bar(ch_num, 2);

	pr_info("%s: ---\n", __func__);

	return ret;
}
EXPORT_SYMBOL(exynos_pcie_rc_set_outbound_atu);

static int exynos_pcie_rc_rd_other_conf(struct pcie_port *pp,
		struct pci_bus *bus, u32 devfn, int where, int size, u32 *val)
{
	int ret, type;
	u32 busdev, cfg_size;
	u64 cpu_addr;
	void __iomem *va_cfg_base;
	struct dw_pcie *pci = to_dw_pcie_from_pp(pp);
	struct exynos_pcie *exynos_pcie = to_exynos_pcie(pci);
	int is_linked = 0;

	if (exynos_pcie->state == STATE_LINK_UP)
		is_linked = 1;

	if (is_linked == 0) {
		/* TCXO_FAR on */
		hsi_tcxo_far_control(2, 1);

		/* set input clk path change to enable */
		if (exynos_pcie->phy_ops.phy_input_clk_change != NULL) {
			exynos_pcie->phy_ops.phy_input_clk_change(exynos_pcie, 1);
		}
	}
	busdev = EXYNOS_PCIE_ATU_BUS(bus->number) | EXYNOS_PCIE_ATU_DEV(PCI_SLOT(devfn)) |
		 EXYNOS_PCIE_ATU_FUNC(PCI_FUNC(devfn));
	if (bus->parent->number == pp->root_bus_nr) {
		type = EXYNOS_PCIE_ATU_TYPE_CFG0;
		cpu_addr = pp->cfg0_base;
		cfg_size = pp->cfg0_size;
		va_cfg_base = pp->va_cfg0_base;
		/* setup ATU for cfg/mem outbound */
		exynos_pcie_rc_prog_viewport_cfg0(pp, busdev);
	} else {
		type = EXYNOS_PCIE_ATU_TYPE_CFG1;
		cpu_addr = pp->cfg1_base;
		cfg_size = pp->cfg1_size;
		va_cfg_base = pp->va_cfg1_base;
		/* setup ATU for cfg/mem outbound */
		exynos_pcie_rc_prog_viewport_cfg1(pp, busdev);
	}
	ret = dw_pcie_read(va_cfg_base + where, size, val);
	if (is_linked == 0) {
		/* set input clk path change to disable */
		if (exynos_pcie->phy_ops.phy_input_clk_change != NULL) {
			exynos_pcie->phy_ops.phy_input_clk_change(exynos_pcie, 0);
		}

		/* TCXO_FAR off */
		hsi_tcxo_far_control(2, 0);
	}

	return ret;
}

static int exynos_pcie_rc_wr_other_conf(struct pcie_port *pp,
		struct pci_bus *bus, u32 devfn, int where, int size, u32 val)
{

	int ret, type;
	u32 busdev, cfg_size;
	u64 cpu_addr;
	void __iomem *va_cfg_base;
	struct dw_pcie *pci = to_dw_pcie_from_pp(pp);
	struct exynos_pcie *exynos_pcie = to_exynos_pcie(pci);
	int is_linked = 0;

	if (exynos_pcie->state == STATE_LINK_UP)
		is_linked = 1;

	if (is_linked == 0) {
		/* TCXO_FAR on */
		hsi_tcxo_far_control(2, 1);

		/* set input clk path change to enable */
		if (exynos_pcie->phy_ops.phy_input_clk_change != NULL) {
			exynos_pcie->phy_ops.phy_input_clk_change(exynos_pcie, 1);
		}
	}
	busdev = EXYNOS_PCIE_ATU_BUS(bus->number) | EXYNOS_PCIE_ATU_DEV(PCI_SLOT(devfn)) |
		 EXYNOS_PCIE_ATU_FUNC(PCI_FUNC(devfn));

	if (bus->parent->number == pp->root_bus_nr) {
		type = EXYNOS_PCIE_ATU_TYPE_CFG0;
		cpu_addr = pp->cfg0_base;
		cfg_size = pp->cfg0_size;
		va_cfg_base = pp->va_cfg0_base;
		/* setup ATU for cfg/mem outbound */
		exynos_pcie_rc_prog_viewport_cfg0(pp, busdev);
	} else {
		type = EXYNOS_PCIE_ATU_TYPE_CFG1;
		cpu_addr = pp->cfg1_base;
		cfg_size = pp->cfg1_size;
		va_cfg_base = pp->va_cfg1_base;
		/* setup ATU for cfg/mem outbound */
		exynos_pcie_rc_prog_viewport_cfg1(pp, busdev);
	}

	ret = dw_pcie_write(va_cfg_base + where, size, val);
	if (is_linked == 0) {
		/* set input clk path change to disable */
		if (exynos_pcie->phy_ops.phy_input_clk_change != NULL) {
			exynos_pcie->phy_ops.phy_input_clk_change(exynos_pcie, 0);
		}

		/* TCXO_FAR off */
		hsi_tcxo_far_control(2, 0);
	}

	return ret;
}

u32 exynos_pcie_rc_read_dbi(struct dw_pcie *pci, void __iomem *base,
				u32 reg, size_t size)
{
	struct pcie_port *pp = &pci->pp;
	u32 val;
	exynos_pcie_rc_rd_own_conf(pp, reg, size, &val);
	return val;
}

void exynos_pcie_rc_write_dbi(struct dw_pcie *pci, void __iomem *base,
				  u32 reg, size_t size, u32 val)
{
	struct pcie_port *pp = &pci->pp;

	exynos_pcie_rc_wr_own_conf(pp, reg, size, val);
}

static int exynos_pcie_rc_link_up(struct dw_pcie *pci)
{
	struct exynos_pcie *exynos_pcie = to_exynos_pcie(pci);
	u32 val;

	if (exynos_pcie->state != STATE_LINK_UP)
		return 0;

	val = exynos_elbi_read(exynos_pcie, PCIE_ELBI_RDLH_LINKUP) & 0x1f;
	if (val >= 0x0d && val <= 0x15)
		return 1;

	return 0;
}

static const struct dw_pcie_ops dw_pcie_ops = {
	.read_dbi = exynos_pcie_rc_read_dbi,
	.write_dbi = exynos_pcie_rc_write_dbi,
        .link_up = exynos_pcie_rc_link_up,
};

static void exynos_pcie_rc_set_iocc(struct pcie_port *pp, int enable)
{
	struct dw_pcie *pci = to_dw_pcie_from_pp(pp);
	struct exynos_pcie *exynos_pcie = to_exynos_pcie(pci);
	int val;

	/* only for 9830 HSI2 block */
	if (exynos_pcie->ip_ver == 0x983000) {
		if (enable) {
			dev_info(pci->dev, "enable cache coherency.\n");

			/* set PCIe Axcache[1] = 1 */
			exynos_pcie_rc_wr_own_conf(pp, PCIE_COHERENCY_CONTROL_3_OFF, 4, 0x10101010);

			/* set PCIe Shareability */
			val = exynos_sysreg_read(exynos_pcie, PCIE_SYSREG_HSI2_SHARABILITY_CTRL);
			val &= ~(PCIE_SYSREG_HSI2_SHARABLE_MASK);
			val |= PCIE_SYSREG_SHARABLE_ENABLE;
			exynos_sysreg_write(exynos_pcie, val, PCIE_SYSREG_HSI2_SHARABILITY_CTRL);
		} else {
			dev_info(pci->dev, "disable cache coherency.\n");

			/* clear PCIe Axcache[1] = 1 */
			exynos_pcie_rc_wr_own_conf(pp, PCIE_COHERENCY_CONTROL_3_OFF, 4, 0x0);

			/* clear PCIe Shareability */
			val = exynos_sysreg_read(exynos_pcie, PCIE_SYSREG_HSI2_SHARABILITY_CTRL);
			val &= ~(PCIE_SYSREG_HSI2_SHARABLE_MASK);
			exynos_sysreg_write(exynos_pcie, val, PCIE_SYSREG_HSI2_SHARABILITY_CTRL);
		}

		exynos_pcie_rc_rd_own_conf(pp, PCIE_COHERENCY_CONTROL_3_OFF, 4, &val);
		dev_info(pci->dev, "%s: PCIe Axcache[1] = 0x%x\n", __func__, val);

		dev_info(pci->dev, "%s: PCIe Shareability = 0x%x.\n", __func__,
				exynos_sysreg_read(exynos_pcie, PCIE_SYSREG_HSI2_SHARABILITY_CTRL));

	} else {
		dev_info(pci->dev, "%s: not supported!!!\n", __func__);
	}
}

static int exynos_pcie_rc_parse_dt(struct device *dev, struct exynos_pcie *exynos_pcie)
{
	struct device_node *np = dev->of_node;
	const char *use_cache_coherency;
	const char *use_msi;
	const char *use_sicd;
	const char *use_sysmmu;
	const char *use_ia;
	const char *use_nclkoff_en;
	const char *use_pcieon_sleep;

	if (of_property_read_u32(np, "ip-ver",
					&exynos_pcie->ip_ver)) {
		dev_err(dev, "Failed to parse the number of ip-ver\n");
		return -EINVAL;
	}

	if (of_property_read_u32(np, "pcie-clk-num",
					&exynos_pcie->pcie_clk_num)) {
		dev_err(dev, "Failed to parse the number of pcie clock\n");
		return -EINVAL;
	}

	if (of_property_read_u32(np, "phy-clk-num",
					&exynos_pcie->phy_clk_num)) {
		dev_err(dev, "Failed to parse the number of phy clock\n");
		return -EINVAL;
	}

	if (of_property_read_u32(np, "pmu-offset",
					&exynos_pcie->pmu_offset)) {
		dev_err(dev, "Failed to parse the number of pmu-offset\n");
		return -EINVAL;
	}

	if (of_property_read_u32(np, "ep-device-type",
				&exynos_pcie->ep_device_type)) {
		dev_err(dev, "EP device type is NOT defined, device type is 'EP_NO_DEVICE(0)'\n");
		exynos_pcie->ep_device_type = EP_NO_DEVICE;
	}

	if (of_property_read_u32(np, "max-link-speed",
				&exynos_pcie->max_link_speed)) {
		dev_err(dev, "MAX Link Speed is NOT defined...(GEN1)\n");
		/* Default Link Speet is GEN1 */
		exynos_pcie->max_link_speed = LINK_SPEED_GEN1;
	}

	if (of_property_read_u32(np, "chip-ver",
					&exynos_pcie->chip_ver)) {
		dev_err(dev, "Failed to parse the number of chip-ver, default '0'\n");
		exynos_pcie->chip_ver = 0;
	}

	if (!of_property_read_string(np, "use-cache-coherency",
						&use_cache_coherency)) {
		if (!strcmp(use_cache_coherency, "true")) {
			dev_info(dev, "Cache Coherency unit is ENABLED.\n");
			exynos_pcie->use_cache_coherency = true;
		} else if (!strcmp(use_cache_coherency, "false")) {
			exynos_pcie->use_cache_coherency = false;
		} else {
			dev_err(dev, "Invalid use-cache-coherency value"
					"(Set to default -> false)\n");
			exynos_pcie->use_cache_coherency = false;
		}
	} else {
		exynos_pcie->use_cache_coherency = false;
	}

#ifdef CONFIG_SEC_PANIC_PCIE_ERR
	if (!of_property_read_string(np, "ep-device-name", (const char**)&exynos_pcie->ep_device_name)) {
		dev_info(dev, "EP device name is %s\n", exynos_pcie->ep_device_name);
	} else {
		dev_err(dev, "EP device name is NOT defined, device node name is %s\n", np->name);
		exynos_pcie->ep_device_name = np->name;
	}
#endif

	if (!of_property_read_string(np, "use-msi", &use_msi)) {
		if (!strcmp(use_msi, "true")) {
			exynos_pcie->use_msi = true;
			dev_info(dev, "MSI is ENABLED.\n");
		} else if (!strcmp(use_msi, "false")) {
			dev_info(dev, "## PCIe don't use MSI\n");
			exynos_pcie->use_msi = false;
		} else {
			dev_err(dev, "Invalid use-msi value"
					"(Set to default -> true)\n");
			exynos_pcie->use_msi = true;
		}
	} else {
		exynos_pcie->use_msi = false;
	}

	if (!of_property_read_string(np, "use-sicd", &use_sicd)) {
		if (!strcmp(use_sicd, "true")) {
			dev_info(dev, "## PCIe use SICD\n");
			exynos_pcie->use_sicd = true;
		} else if (!strcmp(use_sicd, "false")) {
			dev_info(dev, "## PCIe don't use SICD\n");
			exynos_pcie->use_sicd = false;
		} else {
			dev_err(dev, "Invalid use-sicd value"
				       "(set to default -> false)\n");
			exynos_pcie->use_sicd = false;
		}
	} else {
		exynos_pcie->use_sicd = false;
	}

	if (!of_property_read_string(np, "use-pcieon-sleep", &use_pcieon_sleep)) {
		if (!strcmp(use_pcieon_sleep, "true")) {
			dev_info(dev, "## PCIe use PCIE ON Sleep\n");
			exynos_pcie->use_pcieon_sleep = true;
		} else if (!strcmp(use_pcieon_sleep, "false")) {
			dev_info(dev, "## PCIe don't use PCIE ON Sleep\n");
			exynos_pcie->use_pcieon_sleep = false;
		} else {
			dev_err(dev, "Invalid use-pcieon-sleep value"
				       "(set to default -> false)\n");
			exynos_pcie->use_pcieon_sleep = false;
		}
	} else {
		exynos_pcie->use_pcieon_sleep = false;
	}

	if (!of_property_read_string(np, "use-sysmmu", &use_sysmmu)) {
		if (!strcmp(use_sysmmu, "true")) {
			dev_info(dev, "PCIe SysMMU is ENABLED.\n");
			exynos_pcie->use_sysmmu = true;
		} else if (!strcmp(use_sysmmu, "false")) {
			dev_info(dev, "PCIe SysMMU is DISABLED.\n");
			exynos_pcie->use_sysmmu = false;
		} else {
			dev_err(dev, "Invalid use-sysmmu value"
				       "(set to default -> false)\n");
			exynos_pcie->use_sysmmu = false;
		}
	} else {
		exynos_pcie->use_sysmmu = false;
	}

	if (!of_property_read_string(np, "use-ia", &use_ia)) {
		if (!strcmp(use_ia, "true")) {
			dev_info(dev, "PCIe I/A is ENABLED.\n");
			exynos_pcie->use_ia = true;
		} else if (!strcmp(use_ia, "false")) {
			dev_info(dev, "PCIe I/A is DISABLED.\n");
			exynos_pcie->use_ia = false;
		} else {
			dev_err(dev, "Invalid use-ia value"
				       "(set to default -> false)\n");
			exynos_pcie->use_ia = false;
		}
	} else {
		exynos_pcie->use_ia = false;
	}

	if (!of_property_read_string(np, "use-nclkoff-en", &use_nclkoff_en)) {
		if (!strcmp(use_nclkoff_en, "true")) {
			dev_info(dev, "PCIe NCLKOFF is ENABLED.\n");
			exynos_pcie->use_nclkoff_en = true;
		} else if (!strcmp(use_nclkoff_en, "false")) {
			dev_info(dev, "PCIe NCLKOFF is DISABLED.\n");
			exynos_pcie->use_nclkoff_en = false;
		} else {
			dev_err(dev, "Invalid use-nclkoff_en value"
				       "(set to default -> false)\n");
			exynos_pcie->use_nclkoff_en = false;
		}
	} else {
		exynos_pcie->use_nclkoff_en = false;
	}
#if IS_ENABLED(CONFIG_EXYNOS_PM_QOS)
	if (of_property_read_u32(np, "pcie-pm-qos-int",
					&exynos_pcie->int_min_lock))
		exynos_pcie->int_min_lock = 0;
	if (exynos_pcie->int_min_lock)
		exynos_pm_qos_add_request(&exynos_pcie_int_qos[exynos_pcie->ch_num],
				PM_QOS_DEVICE_THROUGHPUT, 0);
	dev_info(dev, "%s: pcie int_min_lock = %d\n",
			__func__, exynos_pcie->int_min_lock);
#endif
	exynos_pcie->pmureg = syscon_regmap_lookup_by_phandle(np,
					"samsung,syscon-phandle");
	if (IS_ERR(exynos_pcie->pmureg)) {
		dev_err(dev, "syscon regmap lookup failed.\n");
		return PTR_ERR(exynos_pcie->pmureg);
	}

	exynos_pcie->sysreg = syscon_regmap_lookup_by_phandle(np,
			"samsung,sysreg-phandle");
	/* Check definitions to access SYSREG in DT*/
	if (IS_ERR(exynos_pcie->sysreg) && IS_ERR(exynos_pcie->sysreg_base)) {
		dev_err(dev, "SYSREG is not defined.\n");
		return PTR_ERR(exynos_pcie->sysreg);
	}

	/* SSD & WIFI power control */
	exynos_pcie->wlan_gpio = of_get_named_gpio(np, "pcie,wlan-gpio", 0);
	if (exynos_pcie->wlan_gpio < 0) {
		dev_err(dev, "wlan gpio is not defined -> don't use wifi through pcie#%d\n",
				exynos_pcie->ch_num);
	} else {
		gpio_direction_output(exynos_pcie->wlan_gpio, 0);
	}

	exynos_pcie->ssd_gpio = of_get_named_gpio(np, "pcie,ssd-gpio", 0);
	if (exynos_pcie->ssd_gpio < 0) {
		dev_err(dev, "ssd gpio is not defined -> don't use ssd through pcie#%d\n",
				exynos_pcie->ch_num);
	} else {
		gpio_direction_output(exynos_pcie->ssd_gpio, 0);
	}

	return 0;
}

static int exynos_pcie_rc_get_pin_state(struct platform_device *pdev,
				struct exynos_pcie *exynos_pcie)
{
	struct device *dev = &pdev->dev;
	struct device_node *np = dev->of_node;
	int ret;

	exynos_pcie->perst_gpio = of_get_gpio(np, 0);
	if (exynos_pcie->perst_gpio < 0) {
		dev_err(&pdev->dev, "cannot get perst_gpio\n");
	} else {
		ret = devm_gpio_request_one(dev, exynos_pcie->perst_gpio,
					    GPIOF_OUT_INIT_LOW, dev_name(dev));
		if (ret)
			return -EINVAL;
	}
	/* Get pin state */
	exynos_pcie->pcie_pinctrl = devm_pinctrl_get(&pdev->dev);
	if (IS_ERR(exynos_pcie->pcie_pinctrl)) {
		dev_err(&pdev->dev, "Can't get pcie pinctrl!!!\n");
		return -EINVAL;
	}
	exynos_pcie->pin_state[PCIE_PIN_ACTIVE] =
		pinctrl_lookup_state(exynos_pcie->pcie_pinctrl, "active");
	if (IS_ERR(exynos_pcie->pin_state[PCIE_PIN_ACTIVE])) {
		dev_err(&pdev->dev, "Can't set pcie clkerq to output high!\n");
		return -EINVAL;
	}
	exynos_pcie->pin_state[PCIE_PIN_IDLE] =
		pinctrl_lookup_state(exynos_pcie->pcie_pinctrl, "idle");
	if (IS_ERR(exynos_pcie->pin_state[PCIE_PIN_IDLE]))
		dev_err(&pdev->dev, "No idle pin state(but it's OK)!!\n");
	else
		pinctrl_select_state(exynos_pcie->pcie_pinctrl,
				exynos_pcie->pin_state[PCIE_PIN_IDLE]);

	return 0;
}

static int exynos_pcie_rc_clock_get(struct pcie_port *pp)
{
	struct dw_pcie *pci = to_dw_pcie_from_pp(pp);
	struct device *dev = pci->dev;
	struct exynos_pcie *exynos_pcie = to_exynos_pcie(pci);
	struct exynos_pcie_clks	*clks = &exynos_pcie->clks;
	int i, total_clk_num, phy_count;

	/*
	 * CAUTION - PCIe and phy clock have to define in order.
	 * You must define related PCIe clock first in DT.
	 */
	total_clk_num = exynos_pcie->pcie_clk_num + exynos_pcie->phy_clk_num;

	for (i = 0; i < total_clk_num; i++) {
		if (i < exynos_pcie->pcie_clk_num) {
			clks->pcie_clks[i] = of_clk_get(dev->of_node, i);
			if (IS_ERR(clks->pcie_clks[i])) {
				dev_err(dev, "Failed to get pcie clock\n");
				return -ENODEV;
			}
		} else {
			phy_count = i - exynos_pcie->pcie_clk_num;
			clks->phy_clks[phy_count] =
				of_clk_get(dev->of_node, i);
			if (IS_ERR(clks->phy_clks[i])) {
				dev_err(dev, "Failed to get pcie clock\n");
				return -ENODEV;
			}
		}
	}

	return 0;
}

static int exynos_pcie_rc_nclkoff_ctrl(struct platform_device *pdev,
			struct exynos_pcie *exynos_pcie)
{
	struct device *dev = &pdev->dev;
#if 0
	u32 val;
#endif
	dev_info(dev, "control NCLK OFF to prevent DBI asseccing when PCIE off \n");
#if 0
	/* TBD: need to check base address & offset of each channel's sysreg */
	val = readl(exynos_pcie->sysreg_base + 0x4);
	dev_info(dev, "orig HSI1_PCIE_GEN4_0_BUS_CTRL: 0x%x\n", val);
	val &= ~PCIE_SUB_CTRL_SLV_EN;
	val &= ~PCIE_SLV_BUS_NCLK_OFF;
	val &= ~PCIE_DBI_BUS_NCLK_OFF;
	writel(val, exynos_pcie->sysreg_base);
	dev_info(dev, "aft HSI1_PCIE_GEN4_0_BUS_CTRL: 0x%x\n", val);
#endif
	return 0;
}

static int exynos_pcie_rc_get_resource(struct platform_device *pdev,
			struct exynos_pcie *exynos_pcie)
{
	struct resource *temp_rsc;
	int ret;

	temp_rsc = platform_get_resource_byname(pdev, IORESOURCE_MEM, "elbi");
	exynos_pcie->elbi_base = devm_ioremap_resource(&pdev->dev, temp_rsc);
	if (IS_ERR(exynos_pcie->elbi_base)) {
		ret = PTR_ERR(exynos_pcie->elbi_base);
		return ret;
	}
	temp_rsc = platform_get_resource_byname(pdev, IORESOURCE_MEM, "phy");
	exynos_pcie->phy_base = devm_ioremap_resource(&pdev->dev, temp_rsc);
	if (IS_ERR(exynos_pcie->phy_base)) {
		ret = PTR_ERR(exynos_pcie->phy_base);
		return ret;
	}

	temp_rsc = platform_get_resource_byname(pdev, IORESOURCE_MEM, "sysreg");
	exynos_pcie->sysreg_base = devm_ioremap_resource(&pdev->dev, temp_rsc);
	if (IS_ERR(exynos_pcie->sysreg_base)) {
		ret = PTR_ERR(exynos_pcie->sysreg_base);
		return ret;
	}

	temp_rsc = platform_get_resource_byname(pdev, IORESOURCE_MEM, "dbi");
	exynos_pcie->rc_dbi_base = devm_ioremap_resource(&pdev->dev, temp_rsc);
	if (IS_ERR(exynos_pcie->rc_dbi_base)) {
		ret = PTR_ERR(exynos_pcie->rc_dbi_base);
		return ret;
	}

	temp_rsc = platform_get_resource_byname(pdev, IORESOURCE_MEM, "pcs");
	exynos_pcie->phy_pcs_base = devm_ioremap_resource(&pdev->dev, temp_rsc);
	if (IS_ERR(exynos_pcie->phy_pcs_base)) {
		ret = PTR_ERR(exynos_pcie->phy_pcs_base);
		return ret;
	}

	if (exynos_pcie->use_ia) {
		temp_rsc = platform_get_resource_byname(pdev,
						IORESOURCE_MEM, "ia");
		exynos_pcie->ia_base =
				devm_ioremap_resource(&pdev->dev, temp_rsc);
		if (IS_ERR(exynos_pcie->ia_base)) {
			ret = PTR_ERR(exynos_pcie->ia_base);
			return ret;
		}
	}

	return 0;
}

static void exynos_pcie_rc_enable_interrupts(struct pcie_port *pp, int enable)
{
	u32 val;
	struct dw_pcie *pci = to_dw_pcie_from_pp(pp);
	struct exynos_pcie *exynos_pcie = to_exynos_pcie(pci);

	dev_info(pci->dev, "## %s PCIe INTERRUPT ##\n", enable ? "ENABLE" : "DISABLE");

	if (enable) {
		/* enable INTX interrupt */
		val = IRQ_INTA_ASSERT | IRQ_INTB_ASSERT |
			IRQ_INTC_ASSERT | IRQ_INTD_ASSERT;
		exynos_elbi_write(exynos_pcie, val, PCIE_IRQ0_EN);
		/* enable IRQ1 interrupt - only LINKDOWN */
		exynos_elbi_write(exynos_pcie, 0x400, PCIE_IRQ1_EN);
		/* enable IRQ2 interrupt */
		val = IRQ_RADM_CPL_TIMEOUT;
		exynos_elbi_write(exynos_pcie, val, PCIE_IRQ2_EN);
	} else {
		exynos_elbi_write(exynos_pcie, 0, PCIE_IRQ0_EN);
		exynos_elbi_write(exynos_pcie, 0, PCIE_IRQ1_EN);
		exynos_elbi_write(exynos_pcie, 0, PCIE_IRQ2_EN);
	}

}

static void __maybe_unused exynos_pcie_notify_callback(struct pcie_port *pp,
							int event)
{
	struct dw_pcie *pci = to_dw_pcie_from_pp(pp);
	struct exynos_pcie *exynos_pcie = to_exynos_pcie(pci);

	if (exynos_pcie->event_reg && exynos_pcie->event_reg->callback &&
			(exynos_pcie->event_reg->events & event)) {
		struct exynos_pcie_notify *notify =
			&exynos_pcie->event_reg->notify;
		notify->event = event;
		notify->user = exynos_pcie->event_reg->user;
		dev_info(pci->dev, "Callback for the event : %d\n", event);
		exynos_pcie->event_reg->callback(notify);
	} else {
		dev_info(pci->dev, "Client driver does not have registration "
					"of the event : %d\n", event);
		dev_info(pci->dev, "Force PCIe poweroff --> poweron\n");
		exynos_pcie_rc_poweroff(exynos_pcie->ch_num);
		exynos_pcie_rc_poweron(exynos_pcie->ch_num);
	}
}

void exynos_pcie_rc_register_dump(int ch_num)
{
	struct exynos_pcie *exynos_pcie = &g_pcie_rc[ch_num];
	struct dw_pcie *pci = exynos_pcie->pci;
	struct pcie_port *pp = &pci->pp;
	u32 i, j, val;

	/* Link Reg : 0x0 ~ 0x2C4 */
	pr_err("Print ELBI region...\n");
	for (i = 0; i < 72; i++) {
		for (j = 0; j < 4; j++) {
			if (((i * 0x10) + (j * 4)) < 0x47C) {
				pr_err("ELBI 0x%04x : 0x%08x\n",
					(i * 0x10) + (j * 4),
				exynos_elbi_read(exynos_pcie,
					(i * 0x10) + (j * 4)));
			}
		}
	}
	pr_err("\n");

	/* PHY Reg : 0x0 ~ 0x180 */
	pr_err("Print PHY region...\n");
	for (i = 0; i < 0x200; i += 4) {
		pr_err("PHY PMA 0x%04x : 0x%08x\n",
				i, exynos_phy_read(exynos_pcie, i));

	}
	/* dump phy monitor registers */
	/* common */
	pr_err("PHY PMA 0x3F0 : 0x%08x\n", exynos_phy_read(exynos_pcie, 0x3F0));
	/* lane0 */
	for (i = 0xE00; i < 0xE50; i += 4) {
		pr_err("PHY PMA 0x%04x : 0x%08x\n",
				i, exynos_phy_read(exynos_pcie, i));

	}
	pr_err("PHY PMA 0xE74 : 0x%08x\n", exynos_phy_read(exynos_pcie, 0xE74));
	pr_err("PHY PMA 0xE78 : 0x%08x\n", exynos_phy_read(exynos_pcie, 0xE78));
	pr_err("PHY PMA 0xE7C : 0x%08x\n", exynos_phy_read(exynos_pcie, 0xE7C));
	pr_err("PHY PMA 0xEC4 : 0x%08x\n", exynos_phy_read(exynos_pcie, 0xEC4));
	pr_err("PHY PMA 0xEC8 : 0x%08x\n", exynos_phy_read(exynos_pcie, 0xEC8));
	pr_err("PHY PMA 0xFC0 : 0x%08x\n", exynos_phy_read(exynos_pcie, 0xFC0));
	/* lane1 */
	for (i = 0x1600; i < 0x1650; i += 4) {
		pr_err("PHY PMA 0x%04x : 0x%08x\n",
				i, exynos_phy_read(exynos_pcie, i));

	}
	pr_err("PHY PMA 0x1674 : 0x%08x\n", exynos_phy_read(exynos_pcie, 0x1674));
	pr_err("PHY PMA 0x1678 : 0x%08x\n", exynos_phy_read(exynos_pcie, 0x1678));
	pr_err("PHY PMA 0x167C : 0x%08x\n", exynos_phy_read(exynos_pcie, 0x167C));
	pr_err("PHY PMA 0x16C4 : 0x%08x\n", exynos_phy_read(exynos_pcie, 0x16C4));
	pr_err("PHY PMA 0x16C8 : 0x%08x\n", exynos_phy_read(exynos_pcie, 0x16C8));
	pr_err("PHY PMA 0x17C0 : 0x%08x\n", exynos_phy_read(exynos_pcie, 0x17C0));

	pr_err("\n");

	/* PHY PCS : 0x0 ~ 0x200 */
	pr_err("Print PHY PCS region...\n");
	for (i = 0; i < 0x200; i += 4) {
		pr_err("PHY PCS 0x%04x : 0x%08x\n",
				i, exynos_phy_pcs_read(exynos_pcie, i));

	}
	pr_err("\n");

	pr_err("Print DBI region...\n");
	for (i = 0; i <= 128; i++) {
		for (j = 0; j < 4; j++) {
			if (((i * 0x10) + (j * 4)) < 0xb90) {
				exynos_pcie_rc_rd_own_conf(pp,
						(i * 0x10) + (j * 4), 4, &val);
				pr_err("DBI 0x%04x : 0x%08x\n",
						(i * 0x10) + (j * 4), val);
			}
		}
	}
	pr_err("\n");
}
EXPORT_SYMBOL(exynos_pcie_rc_register_dump);


void exynos_pcie_rc_dump_link_down_status(int ch_num)
{
	struct exynos_pcie *exynos_pcie = &g_pcie_rc[ch_num];
	struct dw_pcie *pci = exynos_pcie->pci;

//	if (exynos_pcie->state == STATE_LINK_UP) {
		dev_info(pci->dev, "LTSSM: 0x%08x\n",
			exynos_elbi_read(exynos_pcie, PCIE_ELBI_RDLH_LINKUP));
		dev_info(pci->dev, "LTSSM_H: 0x%08x\n",
			exynos_elbi_read(exynos_pcie, PCIE_CXPL_DEBUG_INFO_H));
		dev_info(pci->dev, "DMA_MONITOR1: 0x%08x\n",
			exynos_elbi_read(exynos_pcie, PCIE_DMA_MONITOR1));
		dev_info(pci->dev, "DMA_MONITOR2: 0x%08x\n",
			exynos_elbi_read(exynos_pcie, PCIE_DMA_MONITOR2));
		dev_info(pci->dev, "DMA_MONITOR3: 0x%08x\n",
			exynos_elbi_read(exynos_pcie, PCIE_DMA_MONITOR3));
//	} else {
		dev_info(pci->dev, "PCIE link state is %d\n",
				exynos_pcie->state);
//	}
}

void exynos_pcie_rc_dislink_work(struct work_struct *work)
{
	struct exynos_pcie *exynos_pcie =
		container_of(work, struct exynos_pcie, dislink_work.work);
	struct dw_pcie *pci = exynos_pcie->pci;
	struct pcie_port *pp = &pci->pp;
	struct device *dev = pci->dev;

	if (exynos_pcie->state == STATE_LINK_DOWN)
		return;

//	exynos_pcie_rc_print_link_history(pp);
	exynos_pcie_rc_dump_link_down_status(exynos_pcie->ch_num);
	exynos_pcie_rc_register_dump(exynos_pcie->ch_num);

	exynos_pcie->linkdown_cnt++;
	dev_info(dev, "link down and recovery cnt: %d\n",
			exynos_pcie->linkdown_cnt);
	if (exynos_pcie->use_pcieon_sleep) {
		dev_info(dev, "%s, pcie_is_linkup 0\n", __func__);
		pcie_is_linkup = 0;
	}

#ifdef CONFIG_SEC_PANIC_PCIE_ERR
	//panic("[PCIe RC Case#4] PCIe Link down occurred! %s\n", exynos_pcie->ep_device_name);
	dev_err(dev, "[PCIe RC Case#4] PCIe Link down occurred! %s\n", exynos_pcie->ep_device_name);
	modem_force_crash_exit_ext();
#endif
	exynos_pcie_notify_callback(pp, EXYNOS_PCIE_EVENT_LINKDOWN);
}

void exynos_pcie_rc_cpl_timeout_work(struct work_struct *work)
{
	struct exynos_pcie *exynos_pcie =
		container_of(work, struct exynos_pcie, cpl_timeout_work.work);
	struct dw_pcie *pci = exynos_pcie->pci;
	struct pcie_port *pp = &pci->pp;
	struct device *dev = pci->dev;
	u32 afc_code_lane0, afc_code_lane1, cdr_mon_lane0, cdr_mon_lane1;
	u32 i, val, lane_num = 0, count = 0;

	dev_err(dev, "[%s] +++ \n", __func__);

	/* dump phy monitor registers */
	/* common */
	pr_err("PHY PMA 0x3F0 : 0x%08x\n", exynos_phy_read(exynos_pcie, 0x3F0));
	/* lane0 */
	for (i = 0xE00; i < 0xE50; i += 4) {
		pr_err("PHY PMA 0x%04x : 0x%08x\n",
				i, exynos_phy_read(exynos_pcie, i));
	}
	pr_err("PHY PMA 0xE74 : 0x%08x\n", exynos_phy_read(exynos_pcie, 0xE74));
	pr_err("PHY PMA 0xE78 : 0x%08x\n", exynos_phy_read(exynos_pcie, 0xE78));
	pr_err("PHY PMA 0xE7C : 0x%08x\n", exynos_phy_read(exynos_pcie, 0xE7C));
	pr_err("PHY PMA 0xEC4 : 0x%08x\n", exynos_phy_read(exynos_pcie, 0xEC4));
	pr_err("PHY PMA 0xEC8 : 0x%08x\n", exynos_phy_read(exynos_pcie, 0xEC8));
	pr_err("PHY PMA 0xFC0 : 0x%08x\n", exynos_phy_read(exynos_pcie, 0xFC0));
	/* lane1 */
	for (i = 0x1600; i < 0x1650; i += 4) {
		pr_err("PHY PMA 0x%04x : 0x%08x\n",
				i, exynos_phy_read(exynos_pcie, i));
	}
	pr_err("PHY PMA 0x1674 : 0x%08x\n", exynos_phy_read(exynos_pcie, 0x1674));
	pr_err("PHY PMA 0x1678 : 0x%08x\n", exynos_phy_read(exynos_pcie, 0x1678));
	pr_err("PHY PMA 0x167C : 0x%08x\n", exynos_phy_read(exynos_pcie, 0x167C));
	pr_err("PHY PMA 0x16C4 : 0x%08x\n", exynos_phy_read(exynos_pcie, 0x16C4));
	pr_err("PHY PMA 0x16C8 : 0x%08x\n", exynos_phy_read(exynos_pcie, 0x16C8));
	pr_err("PHY PMA 0x17C0 : 0x%08x\n", exynos_phy_read(exynos_pcie, 0x17C0));

	/* 1. PMA sfr dump for debegging */
	/* PHY PMA lane0 */
	afc_code_lane0 = exynos_phy_read(exynos_pcie, 0xE38);
	pr_err("PHY PMA(0x%04x afc_code0): 0x%08x\n", 0xE38, afc_code_lane0);
	afc_code_lane0 &= (0x3f);

	cdr_mon_lane0 = exynos_phy_read(exynos_pcie, 0xFC0);
	pr_err("PHY PMA(0x%04x cdr_mon0): 0x%08x\n", 0xFC0, cdr_mon_lane0);
	cdr_mon_lane0 = (cdr_mon_lane0 >> 4) & 0xf;

	/* PHY PMA lane1 */
	afc_code_lane1 = exynos_phy_read(exynos_pcie, 0x1638);
	pr_err("PHY PMA(0x%04x afc_code1): 0x%08x\n", 0x1638, afc_code_lane1);
	afc_code_lane1 &= (0x3f);
	cdr_mon_lane1 = exynos_phy_read(exynos_pcie, 0x17C0);
	pr_err("PHY PMA(0x%04x cdr_mon1): 0x%08x\n", 0x17C0, cdr_mon_lane1);
	cdr_mon_lane1 = (cdr_mon_lane1 >> 4) & 0xf;

	dev_info(pci->dev, "afc_code_lane0 = 0x%x, cdr_mon_lane0 = 0x%x\n",
			afc_code_lane0, cdr_mon_lane0);
	dev_info(pci->dev, "afc_code_lane1 = 0x%x, cdr_mon_lane1 = 0x%x\n",
			afc_code_lane1, cdr_mon_lane1);

	/* check lane number */
	exynos_pcie_rc_rd_own_conf(pp, PCIE_LINK_CTRL_STAT, 4, &lane_num);
	lane_num = lane_num >> 20;
	lane_num &= PCIE_CAP_NEGO_LINK_WIDTH_MASK;
	dev_info(pci->dev, "Current lane_num(0x80) = %d\n", lane_num);

	/* check LTSSM */
	dev_info(pci->dev, "check LTSSM +++\n");
	count = 0;
	while (count < LNKRCVYWAIT_TIMEOUT) {
		val = exynos_elbi_read(exynos_pcie,
				PCIE_ELBI_RDLH_LINKUP) & 0x3f;
		if (val == 0x11) {/* LTSSM == 0x11 (L0) link ok */
			dev_info(dev, "LTSSM == 0x11(L0) Link OK\n", __func__);
			return;
		}

		count++;
		udelay(10);
	}
	dev_info(pci->dev, "check LTSSM ---, LTSSM: 0x%x\n", val);

	exynos_pcie->state = STATE_LINK_DOWN_TRY;
	exynos_pcie_rc_dump_link_down_status(exynos_pcie->ch_num);
	exynos_pcie_rc_register_dump(exynos_pcie->ch_num);
	dev_info(dev, "printed DUMP STATE after CPL Timeout(6ms) IRQ\n", __func__);

	if (exynos_pcie->use_pcieon_sleep) {
		dev_info(dev, "[%s] pcie_is_linkup = 0\n", __func__);
		pcie_is_linkup = 0;
	}

	dev_info(dev, "[%s] call PCIE_LINKDOWN callback func.\n", __func__);
	exynos_pcie_notify_callback(pp, EXYNOS_PCIE_EVENT_LINKDOWN);
}



static void exynos_pcie_rc_use_ia(struct exynos_pcie *exynos_pcie)
{
	pr_info("[%s] Set I/A for CDR Lock \n", __func__);
	if (exynos_pcie->use_ia) {
		/* PCIE_IA_IRQ Selection */
		exynos_elbi_write(exynos_pcie, 0x400, 0x388);   /* Enable Link up Interrupt for PCIE_IA */

		/* BASE_ADDR */
		exynos_ia_write(exynos_pcie, 0x13D20000, 0x30);	/* 2020 GEN4_HSI2 sub_con */
		exynos_ia_write(exynos_pcie, 0x13D50000, 0x34);	/* 2020 GEN4_HSI2 pma */

		exynos_ia_write(exynos_pcie, 0x13D00000, 0x38);	/* PCIE_IA_GEN4A base */

		/* Loop CTRL */
		exynos_ia_write(exynos_pcie, 0x00023000, 0x40);	/* LOOP Interval, 0x3000(12288) x bus_clk_period */

		/* EVENT: L1.2 EXIT Interrupt happens */

		/* SQ0) UIR_1 : DATA MASK */
		exynos_ia_write(exynos_pcie, 0x50000004, 0x100); /* UIR_1: DATA MASK_REG */
		exynos_ia_write(exynos_pcie, 0x00000100, 0x104); /* ENABLE bit[15:0] */

		// SQ1) BRT
		exynos_ia_write(exynos_pcie, 0x20420008, 0x108); /* READ CDR_DONE REG */
		exynos_ia_write(exynos_pcie, 0x00000100, 0x10C); /* CHECK CDR_DONE */

		// SQ2) UIR_1 : DATA MASK
		exynos_ia_write(exynos_pcie, 0x50000004, 0x110); /* UIR_1: DATA MASK_REG */
		exynos_ia_write(exynos_pcie, 0x00000400, 0x114); /* ENABLE bit[15:0] */

		// SQ3) LOOP
		exynos_ia_write(exynos_pcie, 0x10000008, 0x118); /* CDR_EN 0x18 */
		exynos_ia_write(exynos_pcie, 0x00000000, 0x11C); /* LOOP until L1.2 exit IRQ clear, but loop timeout because of pending still */

		// SQ4) WRITE
		exynos_ia_write(exynos_pcie, 0x40020008, 0x120); /* CDR_EN 0x00 */
		exynos_ia_write(exynos_pcie, 0x00000100, 0x124); /* Write0-Clear LOOP_CNT_OVER_INTR_STATUS */

		// SQ5) UIR_1 : DATA MASK
		exynos_ia_write(exynos_pcie, 0x50000004, 0x128); /* AFC toggle */
		exynos_ia_write(exynos_pcie, 0x000000F0, 0x12C); /* ENABLE bit[15:0] */

		// SQ6) BRT
		exynos_ia_write(exynos_pcie, 0x20D10FC0, 0x130); /* READ CDR_DONE REG */
		exynos_ia_write(exynos_pcie, 0x000000F0, 0x134); /* CHECK CDR_DONE */

		// SQ7) WRITE
		exynos_ia_write(exynos_pcie, 0x40010A48, 0x138); /* CDR_EN 0x20 */
		exynos_ia_write(exynos_pcie, 0x00000020, 0x13C);

		// SQ8) WRITE
		exynos_ia_write(exynos_pcie, 0x40010A48, 0x140); /* CDR_EN 0x30 */
		exynos_ia_write(exynos_pcie, 0x00000030, 0x144);

		// SQ9) WRITE
		exynos_ia_write(exynos_pcie, 0x40010A48, 0x148); /* CDR_EN 0x00 */
		exynos_ia_write(exynos_pcie, 0x00000000, 0x14C);

		// SQ10) WRITE
		exynos_ia_write(exynos_pcie, 0x40010BF4, 0x150); /* AFC toggle */
		exynos_ia_write(exynos_pcie, 0x00000005, 0x154);

		// SQ11) WRITE
		exynos_ia_write(exynos_pcie, 0x40010BF4, 0x158); /* AFC toggle */
		exynos_ia_write(exynos_pcie, 0x00000004, 0x15C);

		// SQ12) BRF
		exynos_ia_write(exynos_pcie, 0x30010FC0, 0x160); /* READ CDR_DONE REG */
		exynos_ia_write(exynos_pcie, 0x000000F0, 0x164); /* CHECK CDR_DONE  */

		// SQ13) WRITE
		exynos_ia_write(exynos_pcie, 0x40010BF4, 0x168); /* AFC toggle */
		exynos_ia_write(exynos_pcie, 0x00000005, 0x16C);

		// SQ14) WRITE
		exynos_ia_write(exynos_pcie, 0x40000008, 0x170);
		exynos_ia_write(exynos_pcie, 0x00000400, 0x174); /* Write1-Clear IRQ  */

		// SQ15) WRITE
		exynos_ia_write(exynos_pcie, 0x80000000, 0x178); /* RETURN to IDLE */
		exynos_ia_write(exynos_pcie, 0x00000000, 0x17C);

		//PCIE_IA_EN
		exynos_ia_write(exynos_pcie, 0x00000001, 0x000);
	} else {
		pr_info("[%s] Not support I/A!!!\n", __func__);
	}
}

static void exynos_pcie_rc_assert_phy_reset(struct pcie_port *pp)
{
	struct dw_pcie *pci = to_dw_pcie_from_pp(pp);
	struct exynos_pcie *exynos_pcie = to_exynos_pcie(pci);
	struct device *dev = pci->dev;
	int ret;

	ret = exynos_pcie_rc_phy_clock_enable(pp, PCIE_ENABLE_CLOCK);
	dev_err(dev, "phy clk enable, ret value = %d\n", ret);
	if (exynos_pcie->phy_ops.phy_config != NULL)
		exynos_pcie->phy_ops.phy_config(exynos_pcie, exynos_pcie->ch_num);

	/* Added for CDR Lock */
	exynos_pcie_rc_use_ia(exynos_pcie);
}

static void exynos_pcie_rc_resumed_phydown(struct pcie_port *pp)
{
	struct dw_pcie *pci = to_dw_pcie_from_pp(pp);
	struct device *dev = pci->dev;
	struct exynos_pcie *exynos_pcie = to_exynos_pcie(pci);
	int ret;

	/* phy all power down during suspend/resume */
	ret = exynos_pcie_rc_clock_enable(pp, PCIE_ENABLE_CLOCK);
	dev_err(dev, "pcie clk enable, ret value = %d\n", ret);

	exynos_pcie_rc_enable_interrupts(pp, 0);
	dev_err(dev, "[%s] ## PCIe PMU regmap update 1 : BYPASS ##\n", __func__);
	regmap_update_bits(exynos_pcie->pmureg,
			   exynos_pcie->pmu_offset,
			   PCIE_PHY_CONTROL_MASK, 1);

	exynos_pcie_rc_assert_phy_reset(pp);

	/* phy all power down */
	if (exynos_pcie->phy_ops.phy_all_pwrdn != NULL) {
		exynos_pcie->phy_ops.phy_all_pwrdn(exynos_pcie, exynos_pcie->ch_num);
	}
	exynos_pcie_rc_phy_clock_enable(pp, PCIE_DISABLE_CLOCK);
	exynos_pcie_rc_clock_enable(pp, PCIE_DISABLE_CLOCK);
}

static void exynos_pcie_setup_rc(struct pcie_port *pp)
{
	struct dw_pcie *pci = to_dw_pcie_from_pp(pp);
	struct exynos_pcie *exynos_pcie = to_exynos_pcie(pci);
	u32 pcie_cap_off = PCIE_CAP_OFFSET;
	u32 pm_cap_off = PM_CAP_OFFSET;
	u32 val;

	/* enable writing to DBI read-only registers */
	exynos_pcie_rc_wr_own_conf(pp, PCIE_MISC_CONTROL, 4, DBI_RO_WR_EN);

	/* Disable BAR and Exapansion ROM BAR */
	exynos_pcie_rc_wr_own_conf(pp, 0x100010, 4, 0);
	exynos_pcie_rc_wr_own_conf(pp, 0x100030, 4, 0);

	/* change vendor ID and device ID for PCIe */
	exynos_pcie_rc_wr_own_conf(pp, PCI_VENDOR_ID, 2, PCI_VENDOR_ID_SAMSUNG);
	exynos_pcie_rc_wr_own_conf(pp, PCI_DEVICE_ID, 2,
			    PCI_DEVICE_ID_EXYNOS + exynos_pcie->ch_num);

	/* set max link width & speed : Gen2, Lane1 */
	exynos_pcie_rc_rd_own_conf(pp, pcie_cap_off + PCI_EXP_LNKCAP, 4, &val);
	val &= ~(PCI_EXP_LNKCAP_L1EL | PCI_EXP_LNKCAP_SLS);
	val |= PCI_EXP_LNKCAP_L1EL_64USEC;
	val |= PCI_EXP_LNKCTL2_TLS_8_0GB;

	exynos_pcie_rc_wr_own_conf(pp, pcie_cap_off + PCI_EXP_LNKCAP, 4, val);

	/* set auxiliary clock frequency: 26MHz */
	exynos_pcie_rc_wr_own_conf(pp, PCIE_AUX_CLK_FREQ_OFF, 4,
			PCIE_AUX_CLK_FREQ_26MHZ);

	/* set duration of L1.2 & L1.2.Entry */
	exynos_pcie_rc_wr_own_conf(pp, PCIE_L1_SUBSTATES_OFF, 4, PCIE_L1_SUB_VAL);

	/* clear power management control and status register */
	exynos_pcie_rc_wr_own_conf(pp, pm_cap_off + PCI_PM_CTRL, 4, 0x0);

	/* set target speed from DT */
	exynos_pcie_rc_rd_own_conf(pp, pcie_cap_off + PCI_EXP_LNKCTL2, 4, &val);
	val &= ~PCI_EXP_LNKCTL2_TLS;
	val |= exynos_pcie->max_link_speed;
	exynos_pcie_rc_wr_own_conf(pp, pcie_cap_off + PCI_EXP_LNKCTL2, 4, val);
#if 0
	/* TBD : */
	/* initiate link retraining */
	exynos_pcie_rc_rd_own_conf(pp, pcie_cap_off + PCI_EXP_LNKCTL, 4, &val);
	val |= PCI_EXP_LNKCTL_RL;
	exynos_pcie_rc_wr_own_conf(pp, pcie_cap_off + PCI_EXP_LNKCTL, 4, val);
#endif

	/* set completion timeout */
	/*
	Range/ Encoding/ Spec Minimum/ Spec Maximum/ PCIe controller Minimum/ PCIe controller Maximum
		Default 0000b 50 us 50 ms 28 ms 44 ms (M-PCIe: 45)
		A 0001b 50 us 100 us 65 us 99 us (M-PCIe: 100)
		A 0010b 1 ms 10 ms 4.1 ms 6.2 ms (M-PCIe: 6.4)
		B 0101b 16 ms 55 ms 28 ms 44 ms (M-PCIe: 45)
		B 0110b 65 ms 210 ms 86 ms 131 ms (M-PCIe: 133)
		C 1001b 260 ms 900 ms 260 ms 390 ms (M-PCIe: 398)
		C 1010b 1s 3.5 s 1.8 s 2.8 s (M-PCIe: 2.8)
		D 1101b 4s 13 s 5.4 s 8.2 s (M-PCIe: 8.4)
		D 1110b 17s 64 s 38 s 58 s (M-PCIe: 59) */

	exynos_pcie_rc_rd_own_conf(pp, pcie_cap_off + PCI_EXP_DEVCTL2, 4, &val);
	pr_info("%s: before device_ctrl_status(0x98) = 0x%x\n", __func__, val);
	val &= ~(PCIE_CAP_CPL_TIMEOUT_VAL_MASK);
	val |= PCIE_CAP_CPL_TIMEOUT_VAL_6_2MS;
	exynos_pcie_rc_wr_own_conf(pp, pcie_cap_off + PCI_EXP_DEVCTL2, 4, val);
	pr_info("%s: after device_ctrl_status(0x98) = 0x%x\n", __func__, val);

	exynos_pcie_rc_rd_own_conf(pp, PCIE_LINK_CTRL_STAT, 4, &val);
	val |= PCIE_CAP_EXTENDED_SYNCH;
	exynos_pcie_rc_wr_own_conf(pp, PCIE_LINK_CTRL_STAT, 4, val);
	pr_info("%s: link_ctrl_status(0x80), extended_synch[7] = 0x%x\n",
					__func__, val);
}

static int exynos_pcie_rc_init(struct pcie_port *pp)
{
	/* Setup RC to avoid initialization faile in PCIe stack */
	dw_pcie_setup_rc(pp);
	return 0;
}

static struct dw_pcie_host_ops exynos_pcie_rc_ops = {
	.rd_own_conf = exynos_pcie_rc_rd_own_conf,
	.wr_own_conf = exynos_pcie_rc_wr_own_conf,
	.rd_other_conf = exynos_pcie_rc_rd_other_conf,
	.wr_other_conf = exynos_pcie_rc_wr_other_conf,
	.host_init = exynos_pcie_rc_init,
};

irqreturn_t exynos_rc_handle_msi_irq(struct pcie_port *pp)
{
	int i, pos, irq;
	unsigned long val;
	u32 status, num_ctrls;
	irqreturn_t ret = IRQ_NONE;

	num_ctrls = pp->num_vectors / MAX_MSI_IRQS_PER_CTRL;

	for (i = 0; i < num_ctrls; i++) {
		exynos_pcie_rc_rd_own_conf(pp, PCIE_MSI_INTR0_STATUS +
				(i * MSI_REG_CTRL_BLOCK_SIZE),
				4, &status);
		if (!status)
			continue;

		ret = IRQ_HANDLED;
		val = status;
		pos = 0;
		while ((pos = find_next_bit(&val, MAX_MSI_IRQS_PER_CTRL,
						pos)) != MAX_MSI_IRQS_PER_CTRL) {
			irq = irq_find_mapping(pp->irq_domain,
					(i * MAX_MSI_IRQS_PER_CTRL) +
					pos);
			generic_handle_irq(irq);
			pos++;
		}
	}

	return ret;
}

static irqreturn_t exynos_pcie_rc_irq_handler(int irq, void *arg)
{
	struct pcie_port *pp = arg;
	u32 val_irq0, val_irq1, val_irq2;
	struct dw_pcie *pci = to_dw_pcie_from_pp(pp);
	struct exynos_pcie *exynos_pcie = to_exynos_pcie(pci);
	struct device *dev = pci->dev;

	/* handle IRQ0 interrupt */
	val_irq0 = exynos_elbi_read(exynos_pcie, PCIE_IRQ0);
	exynos_elbi_write(exynos_pcie, val_irq0, PCIE_IRQ0);

	/* handle IRQ1 interrupt */
	val_irq1 = exynos_elbi_read(exynos_pcie, PCIE_IRQ1);
	exynos_elbi_write(exynos_pcie, val_irq1, PCIE_IRQ1);

	/* handle IRQ2 interrupt */
	val_irq2 = exynos_elbi_read(exynos_pcie, PCIE_IRQ2);
	exynos_elbi_write(exynos_pcie, val_irq2, PCIE_IRQ2);

	/* only support after EXYNOS9820 EVT 1.1 */
	if (val_irq1 & IRQ_LINK_DOWN) {
		dev_info(dev, "!!!PCIE LINK DOWN (irq1 state : 0x%x)!!!\n", val_irq1);
		dev_info(dev, "!!!irq0 = 0x%x, irq1 = 0x%x, irq2 = 0x%x!!!\n",
				val_irq0, val_irq1, val_irq2);
		exynos_pcie->state = STATE_LINK_DOWN_TRY;
		queue_work(exynos_pcie->pcie_wq,
				&exynos_pcie->dislink_work.work);
	}

	if (val_irq2 & IRQ_RADM_CPL_TIMEOUT) {
		dev_info(dev, "!!!PCIE_CPL_TIMEOUT (PCIE_IRQ2: 0x%x)!!!\n", val_irq2);
		dev_info(dev, "!!!irq0 = 0x%x, irq1 = 0x%x, irq2 = 0x%x!!!\n",
				val_irq0, val_irq1, val_irq2);
		val_irq2 = exynos_elbi_read(exynos_pcie, PCIE_IRQ2);
		dev_info(dev, "!!!check pending clear (PCIE_IRQ2: 0x%x)!!!\n", val_irq2);
		/* exynos_pcie->state = STATE_LINK_DOWN_TRY; */
		queue_work(exynos_pcie->pcie_wq,
				&exynos_pcie->cpl_timeout_work.work);

	}

#if IS_ENABLED(CONFIG_PCI_MSI)
	if (val_irq2 & IRQ_MSI_RISING_ASSERT && exynos_pcie->use_msi) {
		exynos_rc_handle_msi_irq(pp);

		/* Mask & Clear MSI to pend MSI interrupt.
		 * After clearing IRQ_PULSE, MSI interrupt can be ignored if
		 * lower MSI status bit is set while processing upper bit.
		 * Through the Mask/Unmask, ignored interrupts will be pended.
		 */
		exynos_pcie_rc_wr_own_conf(pp, PCIE_MSI_INTR0_MASK, 4, 0xffffffff);
		exynos_pcie_rc_wr_own_conf(pp, PCIE_MSI_INTR0_MASK, 4, 0x0);
	}
#endif

	return IRQ_HANDLED;
}

static int exynos_pcie_rc_msi_init(struct pcie_port *pp)
{
	u32 val;
	struct dw_pcie *pci = to_dw_pcie_from_pp(pp);
	struct exynos_pcie *exynos_pcie = to_exynos_pcie(pci);
	struct device *dev = pci->dev;
	struct pci_bus *ep_pci_bus;
#if IS_ENABLED(CONFIG_LINK_DEVICE_PCIE)
	unsigned long msi_addr_from_dt;
#endif
	/*
	 * The following code is added to avoid duplicated allocation.
	 */
	if (!exynos_pcie->probe_ok) {
		dev_info(dev, "%s: allocate MSI data \n", __func__);
		ep_pci_bus = pci_find_bus(exynos_pcie->pci_dev->bus->domain_nr, 1);
		exynos_pcie_rc_rd_other_conf(pp, ep_pci_bus, 0, MSI_CONTROL,
				4, &val);
		dev_info(dev, "%s: EP support %d-bit MSI address (0x%x)\n", __func__,
				(val & MSI_64CAP_MASK) ? 64 : 32 , val);

		if (exynos_pcie->ep_device_type == EP_SAMSUNG_MODEM) {
#if IS_ENABLED(CONFIG_LINK_DEVICE_PCIE)
			/* get the MSI target address from DT */
			msi_addr_from_dt = shm_get_msi_base();

			if (msi_addr_from_dt) {
				dev_info(dev, "%s: MSI target addr. from DT: 0x%lx\n",
						__func__, msi_addr_from_dt);
				pp->msi_data = msi_addr_from_dt;
				goto program_msi_data;
			} else {
				dev_err(dev, "%s: msi_addr_from_dt is null \n", __func__);
				return -EINVAL;
			}
#else
			dev_info(dev, "EP device is Modem but, ModemIF isn't enabled\n");
#endif
		} else {
			dw_pcie_msi_init(pp);

			if ((pp->msi_data >> 32) != 0)
				dev_info(dev, "MSI memory is allocated over 32bit boundary\n");
			dev_info(dev, "%s: msi_data : 0x%llx\n", __func__, pp->msi_data);
		}
	}

#if IS_ENABLED(CONFIG_LINK_DEVICE_PCIE)
program_msi_data:
#endif
	dev_info(dev, "%s: Program the MSI data: %lx (probe ok:%d)\n", __func__,
				(unsigned long int)pp->msi_data, exynos_pcie->probe_ok);
	/* Program the msi_data */
	exynos_pcie_rc_wr_own_conf(pp, PCIE_MSI_ADDR_LO, 4,
			lower_32_bits(pp->msi_data));
	exynos_pcie_rc_wr_own_conf(pp, PCIE_MSI_ADDR_HI, 4,
			upper_32_bits(pp->msi_data));

	val = exynos_elbi_read(exynos_pcie, PCIE_IRQ2_EN);
	val |= IRQ_MSI_CTRL_EN_RISING_EDG;
	val |= IRQ_RADM_CPL_TIMEOUT;
	exynos_elbi_write(exynos_pcie, val, PCIE_IRQ2_EN);

	/* Enable MSI interrupt after PCIe reset */
	val = (u32)(*pp->msi_irq_in_use);
	exynos_pcie_rc_wr_own_conf(pp, PCIE_MSI_INTR0_ENABLE, 4, val);
	exynos_pcie_rc_rd_own_conf(pp, PCIE_MSI_INTR0_ENABLE, 4, &val);
#if IS_ENABLED(CONFIG_LINK_DEVICE_PCIE)
	dev_info(dev, "MSI INIT check INTR0 ENABLE, 0x%x: 0x%x \n",PCIE_MSI_INTR0_ENABLE, val);
	if (val != 0xf1) {
		exynos_pcie_rc_wr_own_conf(pp, PCIE_MSI_INTR0_ENABLE, 4, 0xf1);
		exynos_pcie_rc_rd_own_conf(pp, PCIE_MSI_INTR0_ENABLE, 4, &val);
	}
#endif
	dev_info(dev, "%s: MSI INIT END, 0x%x: 0x%x \n", __func__, PCIE_MSI_INTR0_ENABLE, val);

	return 0;
}

static void exynos_pcie_rc_send_pme_turn_off(struct exynos_pcie *exynos_pcie)
{
	struct dw_pcie *pci = exynos_pcie->pci;
	struct device *dev = pci->dev;
	int count = 0;
	u32 val;

	/* L1.2 enable check */
	dev_info(dev, "Current PM state(PCS + 0x188) : 0x%x \n",
			readl(exynos_pcie->phy_pcs_base + 0x188));
	dev_info(dev, "DBI Link Control Register: 0x%x \n",
			readl(exynos_pcie->rc_dbi_base + 0x80));

	val = exynos_elbi_read(exynos_pcie, PCIE_ELBI_RDLH_LINKUP) & 0x1f;
	dev_info(dev, "%s: link state:%x\n", __func__, val);
	if (!(val >= 0x0d && val <= 0x14)) {
		dev_info(dev, "%s, pcie link is not up\n", __func__);
		return;
	}

	exynos_elbi_write(exynos_pcie, 0x1, PCIE_APP_REQ_EXIT_L1);
	val = exynos_elbi_read(exynos_pcie, PCIE_APP_REQ_EXIT_L1_MODE);
	val &= ~APP_REQ_EXIT_L1_MODE;
	val |= L1_REQ_NAK_CONTROL_MASTER;
	exynos_elbi_write(exynos_pcie, val, PCIE_APP_REQ_EXIT_L1_MODE);

	exynos_elbi_write(exynos_pcie, 0x1, XMIT_PME_TURNOFF);

	while (count < MAX_L2_TIMEOUT) {
		if ((exynos_elbi_read(exynos_pcie, PCIE_IRQ0)
						& IRQ_RADM_PM_TO_ACK)) {
			dev_err(dev, "ack message is ok\n");
			udelay(10);
			break;
		}

		udelay(10);
		count++;
	}
	if (count >= MAX_L2_TIMEOUT)
		dev_err(dev, "cannot receive ack message from EP\n");

	exynos_elbi_write(exynos_pcie, 0x0, XMIT_PME_TURNOFF);

	count = 0;
	do {
		val = exynos_elbi_read(exynos_pcie, PCIE_ELBI_RDLH_LINKUP);
		val = val & 0x1f;
		if (val == 0x15) {
			dev_err(dev, "received Enter_L23_READY DLLP packet\n");
			break;
		}
		udelay(10);
		count++;
	} while (count < MAX_L2_TIMEOUT);

	if (count >= MAX_L2_TIMEOUT) {
		dev_err(dev, "cannot receive L23_READY DLLP packet(0x%x)\n", val);
#ifdef CONFIG_SEC_PANIC_PCIE_ERR
		//panic("[PCIe RC Case#5] L2/3 READY fail! %s\n", exynos_pcie->ep_device_name);
		dev_err(dev, "[PCIe RC Case#5] L2/3 READY fail! %s\n", exynos_pcie->ep_device_name);
		modem_force_crash_exit_ext();
#endif
	}
}

static int exynos_pcie_rc_establish_link(struct pcie_port *pp)
{
	struct dw_pcie *pci = to_dw_pcie_from_pp(pp);
	struct exynos_pcie *exynos_pcie = to_exynos_pcie(pci);
	struct device *dev = pci->dev;
	u32 val, busdev;
	int count = 0, try_cnt = 0;
	unsigned int save_before_state = 0xff;
retry:
	/* avoid checking rx elecidle when access DBI */
	if (exynos_pcie->phy_ops.phy_check_rx_elecidle != NULL)
		exynos_pcie->phy_ops.phy_check_rx_elecidle(
				exynos_pcie->phy_pcs_base, IGNORE_ELECIDLE,
				exynos_pcie->ch_num);

	exynos_pcie_rc_assert_phy_reset(pp);

	val = exynos_elbi_read(exynos_pcie, PCIE_SOFT_RESET);
	val &= ~SOFT_PWR_RESET;
	exynos_elbi_write(exynos_pcie, val, PCIE_SOFT_RESET);
	udelay(20);
	val |= SOFT_PWR_RESET;
	exynos_elbi_write(exynos_pcie, val, PCIE_SOFT_RESET);

	/* EQ Off */
	exynos_pcie_rc_wr_own_conf(pp, 0x890, 4, 0x12000);

	/* set #PERST high */
	gpio_set_value(exynos_pcie->perst_gpio, 1);

	dev_info(dev, "%s: Set PERST to HIGH, gpio val = %d\n",
			__func__, gpio_get_value(exynos_pcie->perst_gpio));
	usleep_range(18000, 20000);

	val = exynos_elbi_read(exynos_pcie, PCIE_APP_REQ_EXIT_L1_MODE);
	val |= APP_REQ_EXIT_L1_MODE;
	val |= L1_REQ_NAK_CONTROL_MASTER;
	exynos_elbi_write(exynos_pcie, val, PCIE_APP_REQ_EXIT_L1_MODE);
	exynos_elbi_write(exynos_pcie, PCIE_LINKDOWN_RST_MANUAL,
			  PCIE_LINKDOWN_RST_CTRL_SEL);

	/* Q-Channel support */
	val = exynos_elbi_read(exynos_pcie, PCIE_QCH_SEL);
	if (exynos_pcie->ip_ver >= 0x889500) {
		val &= ~(CLOCK_GATING_PMU_MASK | CLOCK_GATING_APB_MASK |
				CLOCK_GATING_AXI_MASK);
	} else {
		val &= ~CLOCK_GATING_MASK;
		val |= CLOCK_NOT_GATING;
	}
	exynos_elbi_write(exynos_pcie, val, PCIE_QCH_SEL);

	/* NAK enable when AXI pending */
	exynos_elbi_write(exynos_pcie, NACK_ENABLE, PCIE_MSTR_PEND_SEL_NAK);
	dev_info(dev, "%s: NACK option enable: 0x%x\n", __func__,
			exynos_elbi_read(exynos_pcie, PCIE_MSTR_PEND_SEL_NAK));

	/* setup root complex */
	dw_pcie_setup_rc(pp);
	exynos_pcie_setup_rc(pp);

	if (exynos_pcie->use_cache_coherency)
		exynos_pcie_rc_set_iocc(pp, 1);

	if (exynos_pcie->phy_ops.phy_check_rx_elecidle != NULL)
		exynos_pcie->phy_ops.phy_check_rx_elecidle(
				exynos_pcie->phy_pcs_base, ENABLE_ELECIDLE,
				exynos_pcie->ch_num);

	dev_info(dev, "D state: %x, %x\n",
		 exynos_elbi_read(exynos_pcie, PCIE_PM_DSTATE) & 0x7,
		 exynos_elbi_read(exynos_pcie, PCIE_ELBI_RDLH_LINKUP));

	save_before_state = exynos_elbi_read(exynos_pcie, PCIE_ELBI_RDLH_LINKUP);
	//usleep_range(18000, 20000);
	usleep_range(48000, 50000);

	/* assert LTSSM enable */
	exynos_elbi_write(exynos_pcie, PCIE_ELBI_LTSSM_ENABLE,
				PCIE_APP_LTSSM_ENABLE);
	count = 0;
	while (count < MAX_TIMEOUT) {
		val = exynos_elbi_read(exynos_pcie,
					PCIE_ELBI_RDLH_LINKUP) & 0x3f;
		/*if (val != save_before_state) {
			dev_info(dev, "PCIE_ELBI_RDLH_LINKUP :0x%x \n", val);
			save_before_state = val;
		}*/
		if (val == 0x11)
			break;

		count++;

		udelay(10);
	}

	if (count >= MAX_TIMEOUT) {
		try_cnt++;

		val = exynos_elbi_read(exynos_pcie,
					PCIE_ELBI_RDLH_LINKUP) & 0x3f;
		dev_err(dev, "%s: Link is not up, try count: %d, linksts: %s(0x%x)\n",
			__func__, try_cnt, LINK_STATE_DISP(val), val);

		if (try_cnt < 10) {
			gpio_set_value(exynos_pcie->perst_gpio, 0);
			dev_info(dev, "%s: Set PERST to LOW, gpio val = %d\n",
				__func__,
				gpio_get_value(exynos_pcie->perst_gpio));
			/* LTSSM disable */
			exynos_elbi_write(exynos_pcie, PCIE_ELBI_LTSSM_DISABLE,
					  PCIE_APP_LTSSM_ENABLE);
			exynos_pcie_rc_phy_clock_enable(pp, PCIE_DISABLE_CLOCK);
			goto retry;
		} else {
			//exynos_pcie_host_v1_print_link_history(pp);
#ifdef CONFIG_SEC_PANIC_PCIE_ERR
			//panic("[PCIe RC Case#1] PCIe Link up fail! %s\n", exynos_pcie->ep_device_name);
			dev_err(dev, "[PCIe RC Case#1] PCIe Link up fail! %s\n", exynos_pcie->ep_device_name);
			modem_force_crash_exit_ext();
#endif
			if ((exynos_pcie->ip_ver >= 0x889000) &&
				(exynos_pcie->ep_device_type == EP_BCM_WIFI)) {
				return -EPIPE;
			}
			return -EPIPE;
		}
	} else {
		val = exynos_elbi_read(exynos_pcie,
					PCIE_ELBI_RDLH_LINKUP) & 0xff;
	        dev_info(dev, "%s: %s(0x%x)\n", __func__,
			                        LINK_STATE_DISP(val), val);

		dev_info(dev, "%s: (phy+0xC08)=0x%x, (phy+0x1408=0x%x), (phy+0xC6C=0x%x), (phy+0x146C=0x%x)\n",
				__func__, exynos_phy_read(exynos_pcie, 0xC08),
				exynos_phy_read(exynos_pcie, 0x1408),
				exynos_phy_read(exynos_pcie, 0xC6C),
				exynos_phy_read(exynos_pcie, 0x146C));

		exynos_pcie_rc_rd_own_conf(pp, PCIE_LINK_CTRL_STAT, 4, &val);
		val = (val >> 16) & 0xf;
		dev_info(dev, "Current Link Speed is GEN%d (MAX GEN%d)\n",
				val, exynos_pcie->max_link_speed);

		/* check link training result(speed) */
		if (exynos_pcie->ip_ver >= 0x982000 && val < exynos_pcie->max_link_speed) {
			try_cnt++;
			dev_err(dev, "%s: Link is up. But not GEN3 speed, try count: %d\n",
					__func__, try_cnt);
			if (try_cnt < 10) {
				gpio_set_value(exynos_pcie->perst_gpio, 0);
				dev_info(dev, "%s: Set PERST to LOW, gpio val = %d\n",
						__func__,
						gpio_get_value(exynos_pcie->perst_gpio));
				/* LTSSM disable */
				exynos_elbi_write(exynos_pcie, PCIE_ELBI_LTSSM_DISABLE,
						PCIE_APP_LTSSM_ENABLE);
				exynos_pcie_rc_phy_clock_enable(pp, PCIE_DISABLE_CLOCK);
				goto retry;
			} else {
				dev_info(dev, "Current Link Speed is GEN%d (MAX GEN%d)\n",
						val, exynos_pcie->max_link_speed);
			}
		}

		/* one more check L0 state for Gen3 recovery */
		count = 0;
		dev_info(dev, "%s: check L0 state for Gen3 recovery\n", __func__);
		while (count < MAX_TIMEOUT) {
			val = exynos_elbi_read(exynos_pcie,
					PCIE_ELBI_RDLH_LINKUP) & 0x3f;
			if (val == 0x11)
				break;

			count++;

			udelay(10);
		}

		val = exynos_elbi_read(exynos_pcie, PCIE_IRQ0);
		exynos_elbi_write(exynos_pcie, val, PCIE_IRQ0);
		val = exynos_elbi_read(exynos_pcie, PCIE_IRQ1);
		exynos_elbi_write(exynos_pcie, val, PCIE_IRQ1);
		val = exynos_elbi_read(exynos_pcie, PCIE_IRQ2);
		exynos_elbi_write(exynos_pcie, val, PCIE_IRQ2);

		/* enable IRQ */
		exynos_pcie_rc_enable_interrupts(pp, 1);

		/* setup ATU for cfg/mem outbound */
		busdev = EXYNOS_PCIE_ATU_BUS(1) | EXYNOS_PCIE_ATU_DEV(0) | EXYNOS_PCIE_ATU_FUNC(0);
		exynos_pcie_rc_prog_viewport_cfg0(pp, busdev);
		exynos_pcie_rc_prog_viewport_mem_outbound(pp);

	}
	return 0;
}

int exynos_pcie_rc_speedchange(int ch_num, int spd)
{
	struct exynos_pcie *exynos_pcie = &g_pcie_rc[ch_num];
	struct dw_pcie *pci = exynos_pcie->pci;
	struct pcie_port *pp = &pci->pp;
	struct pci_bus *ep_pci_bus;
	int i;
	u32 val, val1, current_spd;

	if (exynos_pcie->state != STATE_LINK_UP) {
		dev_err(pci->dev, "Link is not up\n");
		return 1;
	}

	dev_err(pci->dev, "%s: force l1ss disable\n", __func__);
	exynos_pcie_rc_l1ss_ctrl(0, PCIE_L1SS_CTRL_TEST);

	if (spd > 3 || spd < 1) {
		dev_err(pci->dev, "Unable to change to GEN%d\n", spd);
		return 1;
	}

	exynos_pcie_rc_rd_own_conf(pp, PCIE_LINK_CTRL_STAT, 4, &current_spd);
	current_spd = current_spd >> 16;
	current_spd &= PCIE_CAP_LINK_SPEED;
	dev_info(pci->dev, "Current link speed(0x80) : from GEN%d\n", current_spd);

	if (current_spd == spd) {
		dev_err(pci->dev, "Already changed to GEN%d\n", spd);
		return 1;
	}

	//modify register to change link speed(GEN#)
	ep_pci_bus = pci_find_bus(exynos_pcie->pci_dev->bus->domain_nr, 1);
	exynos_pcie_rc_rd_other_conf(pp, ep_pci_bus, 0, PCI_VENDOR_ID, 4, &val);

	exynos_pcie_rc_rd_other_conf(pp, ep_pci_bus, 0, LINK_CONTROL2_LINK_STATUS2_REG, 4, &val);
	val = val & PCIE_CAP_TARGET_LINK_SPEED_MASK;
	val = val | 0x3;
	exynos_pcie_rc_wr_other_conf(pp, ep_pci_bus, 0, LINK_CONTROL2_LINK_STATUS2_REG, 4, val);
	dev_info(pci->dev, "Check EP Current Target Speed Val = 0x%x\n", val);

	exynos_pcie_rc_rd_own_conf(pp, LINK_CONTROL2_LINK_STATUS2_REG, 4, &val);
	val = val & PCIE_CAP_TARGET_LINK_SPEED_MASK;
	val = val | spd;
	exynos_pcie_rc_wr_own_conf(pp, LINK_CONTROL2_LINK_STATUS2_REG, 4, val);
	dev_info(pci->dev, "Set RC Target Speed Val = 0x%x\n", val);

	exynos_pcie_rc_rd_own_conf(pp, PCIE_LINK_WIDTH_SPEED_CONTROL, 4, &val);
	val = val & DIRECT_SPEED_CHANGE_MASK;
	exynos_pcie_rc_wr_own_conf(pp, PCIE_LINK_WIDTH_SPEED_CONTROL, 4, val);
	dev_info(pci->dev, "Clear Direct Speed Change Val = 0x%x\n", val);

	exynos_pcie_rc_rd_own_conf(pp, PCIE_LINK_WIDTH_SPEED_CONTROL, 4, &val);
	val = val | DIRECT_SPEED_CHANGE_ENABLE;
	exynos_pcie_rc_wr_own_conf(pp, PCIE_LINK_WIDTH_SPEED_CONTROL, 4, val);
	dev_info(pci->dev, "Set Direct Speed Change Val = 0x%x\n", val);

	for (i = 0; i < MAX_TIMEOUT_SPEEDCHANGE; i++) {
		exynos_pcie_rc_rd_own_conf(pp, PCIE_LINK_CTRL_STAT, 4, &current_spd);
		current_spd = current_spd >> 16;
		current_spd &= PCIE_CAP_LINK_SPEED;

		if (current_spd == spd)
			break;
		udelay(10);
	}

	for (i = 0; i < MAX_TIMEOUT_SPEEDCHANGE; i++) {
		val = exynos_elbi_read(exynos_pcie, PCIE_ELBI_RDLH_LINKUP) & 0x3f;
		val1 = exynos_phy_pcs_read(exynos_pcie, 0x188);

		if ((val == 0x11) || ((val == 0x14) && (val1 == 0x6)))
			break;
		udelay(10);
	}

	if (current_spd != spd) {
		dev_err(pci->dev, "Fail: Unable to change to GEN%d\n", spd);
		return 1;
	}

	dev_info(pci->dev, "Changed link speed(0x80) : to GEN%d\n", current_spd);

	dev_err(pci->dev, "%s: force l1ss enable\n", __func__);
	exynos_pcie_rc_l1ss_ctrl(1, PCIE_L1SS_CTRL_TEST);

	return 0;
}
EXPORT_SYMBOL(exynos_pcie_rc_speedchange);

int exynos_pcie_rc_lanechange(int ch_num, int lane)
{
	struct exynos_pcie *exynos_pcie = &g_pcie_rc[ch_num];
	struct dw_pcie *pci = exynos_pcie->pci;
	struct pcie_port *pp = &pci->pp;
	struct pci_bus *ep_pci_bus;
	int i;
	u32 val, val1, lane_num;

	if (exynos_pcie->state != STATE_LINK_UP) {
		dev_err(pci->dev, "Link is not up\n");
		return 1;
	}

	dev_err(pci->dev, "%s: force l1ss disable\n", __func__);
	exynos_pcie_rc_l1ss_ctrl(0, PCIE_L1SS_CTRL_TEST);

	if (lane > 2 || lane < 1) {
		dev_err(pci->dev, "Unable to change to %d lane\n", lane);
		return 1;
	}

	exynos_pcie_rc_rd_own_conf(pp, PCIE_LINK_CTRL_STAT, 4, &lane_num);
	lane_num = lane_num >> 20;
	lane_num &= PCIE_CAP_NEGO_LINK_WIDTH_MASK;
	dev_info(pci->dev, "Current lane_num(0x80) : from %d lane\n", lane_num);

	if (lane_num == lane) {
		dev_err(pci->dev, "Already changed to %d lane\n", lane);
		return 1;
	}

	//modify register to change lane num
	ep_pci_bus = pci_find_bus(exynos_pcie->pci_dev->bus->domain_nr, 1);
	exynos_pcie_rc_rd_other_conf(pp, ep_pci_bus, 0, PCI_VENDOR_ID, 4, &val);

	exynos_pcie_rc_rd_own_conf(pp, MULTI_LANE_CONTROL_OFF, 4, &val);
	val = val & TARGET_LINK_WIDTH_MASK;
	val = val | lane;
	exynos_pcie_rc_wr_own_conf(pp, MULTI_LANE_CONTROL_OFF, 4, val);

	exynos_pcie_rc_rd_own_conf(pp, MULTI_LANE_CONTROL_OFF, 4, &val);
	val = val | DIRECT_LINK_WIDTH_CHANGE_SET;
	exynos_pcie_rc_wr_own_conf(pp, MULTI_LANE_CONTROL_OFF, 4, val);

	for (i = 0; i < MAX_TIMEOUT_LANECHANGE; i++) {
		exynos_pcie_rc_rd_own_conf(pp, PCIE_LINK_CTRL_STAT, 4, &lane_num);
		lane_num = lane_num >> 20;
		lane_num &= PCIE_CAP_NEGO_LINK_WIDTH_MASK;

		if (lane_num == lane)
			break;
		udelay(10);
	}

	for (i = 0; i < MAX_TIMEOUT_LANECHANGE; i++) {
		val = exynos_elbi_read(exynos_pcie, PCIE_ELBI_RDLH_LINKUP) & 0x3f;
		val1 = exynos_phy_pcs_read(exynos_pcie, 0x188);

		if ((val == 0x11) || ((val == 0x14) && (val1 == 0x6)))
			break;
		udelay(10);
	}

	if (lane_num != lane) {
		dev_err(pci->dev, "Unable to change to %d lane\n", lane);
		return 1;
	}

	dev_info(pci->dev, "Changed lane_num(0x80) : to %d lane\n", lane_num);

	dev_err(pci->dev, "%s: force l1ss enable\n", __func__);
	exynos_pcie_rc_l1ss_ctrl(1, PCIE_L1SS_CTRL_TEST);

	return 0;
}
EXPORT_SYMBOL(exynos_pcie_rc_lanechange);

int exynos_pcie_rc_poweron(int ch_num)
{
	struct exynos_pcie *exynos_pcie = &g_pcie_rc[ch_num];
	struct dw_pcie *pci;
	struct pcie_port *pp;
	struct device *dev;
	u32 val;
	int ret;

	if (!exynos_pcie) {
		pr_err("%s: ch#%d PCIe device is not loaded\n", __func__, ch_num);
		return -ENODEV;
	}

	pci = exynos_pcie->pci;
	pp = &pci->pp;
	dev = pci->dev;

	dev_info(dev, "%s, start of poweron, pcie state: %d\n", __func__,
							 exynos_pcie->state);
	if (exynos_pcie->state == STATE_LINK_DOWN) {
		/* TCXO_FAR on */
		hsi_tcxo_far_control(2, 1);

		if (exynos_pcie->use_pcieon_sleep) {
			dev_info(dev, "%s, pcie_is_linkup 1\n", __func__);
			pcie_is_linkup = 1;
		}
		ret = exynos_pcie_rc_clock_enable(pp, PCIE_ENABLE_CLOCK);
		dev_err(dev, "pcie clk enable, ret value = %d\n", ret);

#if IS_ENABLED(CONFIG_CPU_IDLE)
		if (exynos_pcie->use_sicd) {
			dev_info(dev, "%s, ip idle status : %d, idle_ip_index: %d \n",
					__func__, PCIE_IS_ACTIVE, exynos_pcie->idle_ip_index);
			exynos_update_ip_idle_status(
					exynos_pcie->idle_ip_index,
					PCIE_IS_ACTIVE);
		}
#endif
#if IS_ENABLED(CONFIG_EXYNOS_PM_QOS)
		if (exynos_pcie->int_min_lock) {
			exynos_pm_qos_update_request(&exynos_pcie_int_qos[ch_num],
					exynos_pcie->int_min_lock);
			dev_info(dev, "%s: pcie int_min_lock = %d\n",
					__func__, exynos_pcie->int_min_lock);
		}
#endif
		/* Enable SysMMU */
		if (exynos_pcie->use_sysmmu)
			pcie_sysmmu_enable(ch_num);
		pinctrl_select_state(exynos_pcie->pcie_pinctrl,
				exynos_pcie->pin_state[PCIE_PIN_ACTIVE]);

		dev_err(dev, "[%s] ## PCIe PMU regmap update 1 : BYPASS ##\n", __func__);
		regmap_update_bits(exynos_pcie->pmureg,
				   exynos_pcie->pmu_offset,
				   PCIE_PHY_CONTROL_MASK, 1);

		/* phy all power down clear */
		if (exynos_pcie->phy_ops.phy_all_pwrdn_clear != NULL) {
			exynos_pcie->phy_ops.phy_all_pwrdn_clear(exynos_pcie,
								exynos_pcie->ch_num);
		}

		/* set input clk path change to enable */
		if (exynos_pcie->phy_ops.phy_input_clk_change != NULL) {
			exynos_pcie->phy_ops.phy_input_clk_change(exynos_pcie, 1);
		}

		/* force_pclk_en enable*/
		writel(0x0D, exynos_pcie->phy_pcs_base + 0x0180);

		exynos_pcie->state = STATE_LINK_UP_TRY;

		/* Enable history buffer */
		val = exynos_elbi_read(exynos_pcie, PCIE_STATE_HISTORY_CHECK);
		exynos_elbi_write(exynos_pcie, 0, PCIE_STATE_HISTORY_CHECK);
		val = HISTORY_BUFFER_ENABLE;
		exynos_elbi_write(exynos_pcie, val, PCIE_STATE_HISTORY_CHECK);

		exynos_elbi_write(exynos_pcie, 0x200000, PCIE_STATE_POWER_S);
		exynos_elbi_write(exynos_pcie, 0xffffffff, PCIE_STATE_POWER_M);

		enable_irq(pp->irq);
		if (exynos_pcie_rc_establish_link(pp)) {
			dev_err(dev, "pcie link up fail\n");
			goto poweron_fail;
		}
		exynos_pcie->state = STATE_LINK_UP;

		dev_err(dev, "[%s] exynos_pcie->probe_ok : %d\n", __func__, exynos_pcie->probe_ok);
		if (!exynos_pcie->probe_ok) {
			pci_rescan_bus(exynos_pcie->pci_dev->bus);

			exynos_pcie->ep_pci_dev = exynos_pcie_get_pci_dev(&pci->pp);

			exynos_pcie->ep_pci_dev->dma_mask = DMA_BIT_MASK(36);
			exynos_pcie->ep_pci_dev->dev.coherent_dma_mask = DMA_BIT_MASK(36);
			dma_set_seg_boundary(&(exynos_pcie->ep_pci_dev->dev), DMA_BIT_MASK(36));

			if (exynos_pcie->use_msi) {
				ret = exynos_pcie_rc_msi_init(pp);
				if (ret) {
					dev_err(dev, "%s: failed to MSI initialization(%d)\n",
							__func__, ret);
					return ret;
				}
			}

			if (pci_save_state(exynos_pcie->pci_dev)) {
				dev_err(dev, "Failed to save pcie state\n");
				goto poweron_fail;
			}
			exynos_pcie->pci_saved_configs =
				pci_store_saved_state(exynos_pcie->pci_dev);
			exynos_pcie->probe_ok = 1;
		} else if (exynos_pcie->probe_ok) {
			if (exynos_pcie->use_msi) {
				ret = exynos_pcie_rc_msi_init(pp);
				if (ret) {
					dev_err(dev, "%s: failed to MSI initialization(%d)\n",
							__func__, ret);
					return ret;
				}
			}

			if (pci_load_saved_state(exynos_pcie->pci_dev,
					     exynos_pcie->pci_saved_configs)) {
				dev_err(dev, "Failed to load pcie state\n");
				goto poweron_fail;
			}
			pci_restore_state(exynos_pcie->pci_dev);
		}
	}

	dev_info(dev, "%s, end of poweron, pcie state: %d\n", __func__,
		 exynos_pcie->state);

	return 0;

poweron_fail:
	exynos_pcie->state = STATE_LINK_UP;
	exynos_pcie_rc_poweroff(exynos_pcie->ch_num);

	return -EPIPE;
}

void exynos_pcie_rc_poweroff(int ch_num)
{
	struct exynos_pcie *exynos_pcie = &g_pcie_rc[ch_num];
	struct dw_pcie *pci;
	struct pcie_port *pp;
	struct device *dev;
	unsigned long flags;
	u32 val;

	if (!exynos_pcie) {
		pr_err("%s: ch#%d PCIe device is not loaded\n", __func__, ch_num);
		return;
	}

	pci = exynos_pcie->pci;
	pp = &pci->pp;
	dev = pci->dev;

	dev_info(dev, "%s, start of poweroff, pcie state: %d\n", __func__,
		 exynos_pcie->state);

	if (exynos_pcie->state == STATE_LINK_UP ||
	    exynos_pcie->state == STATE_LINK_DOWN_TRY) {
		exynos_pcie->state = STATE_LINK_DOWN_TRY;

		/* TCXO_FAR on */
		hsi_tcxo_far_control(2, 1);

		disable_irq(pp->irq);

		if (exynos_pcie->ip_ver == 0x982000) {
			/* only support EVT1.1 */
				val = exynos_elbi_read(exynos_pcie, PCIE_IRQ1_EN);
				val &= ~IRQ_LINKDOWN_ENABLE_EVT1_1;
				exynos_elbi_write(exynos_pcie, val, PCIE_IRQ1_EN);
		}

		spin_lock_irqsave(&exynos_pcie->conf_lock, flags);
		exynos_pcie_rc_send_pme_turn_off(exynos_pcie);
		exynos_pcie->state = STATE_LINK_DOWN;

		/* force_pclk_en enable */
		writel(0x0D, exynos_pcie->phy_pcs_base + 0x0180);

		/* Disable SysMMU */
		if (exynos_pcie->use_sysmmu)
			pcie_sysmmu_disable(ch_num);

		/* Disable history buffer */
		val = exynos_elbi_read(exynos_pcie, PCIE_STATE_HISTORY_CHECK);
		val &= ~HISTORY_BUFFER_ENABLE;
		exynos_elbi_write(exynos_pcie, val, PCIE_STATE_HISTORY_CHECK);

		gpio_set_value(exynos_pcie->perst_gpio, 0);
		dev_info(dev, "%s: Set PERST to LOW, gpio val = %d\n",
			__func__, gpio_get_value(exynos_pcie->perst_gpio));

		/* LTSSM disable */
		exynos_elbi_write(exynos_pcie, PCIE_ELBI_LTSSM_DISABLE,
				PCIE_APP_LTSSM_ENABLE);

		/* force SOFT_PWR_RESET */
		dev_info(dev, "%s: Set PERST to LOW, gpio val = %d\n",
			__func__, gpio_get_value(exynos_pcie->perst_gpio));
		val = exynos_elbi_read(exynos_pcie, PCIE_SOFT_RESET);
		val &= ~SOFT_PWR_RESET;
		exynos_elbi_write(exynos_pcie, val, PCIE_SOFT_RESET);
		udelay(20);
		val |= SOFT_PWR_RESET;
		exynos_elbi_write(exynos_pcie, val, PCIE_SOFT_RESET);

		/* phy all power down */
		if (exynos_pcie->phy_ops.phy_all_pwrdn != NULL) {
			exynos_pcie->phy_ops.phy_all_pwrdn(exynos_pcie,
					exynos_pcie->ch_num);
		}

		spin_unlock_irqrestore(&exynos_pcie->conf_lock, flags);

		exynos_pcie_rc_phy_clock_enable(pp, PCIE_DISABLE_CLOCK);
		exynos_pcie_rc_clock_enable(pp, PCIE_DISABLE_CLOCK);
		exynos_pcie->atu_ok = 0;

		if (!IS_ERR(exynos_pcie->pin_state[PCIE_PIN_IDLE]))
			pinctrl_select_state(exynos_pcie->pcie_pinctrl,
					exynos_pcie->pin_state[PCIE_PIN_IDLE]);

#if IS_ENABLED(CONFIG_EXYNOS_PM_QOS)
		if (exynos_pcie->int_min_lock) {
			exynos_pm_qos_update_request(&exynos_pcie_int_qos[ch_num], 0);
			dev_info(dev, "%s: pcie int_min_lock = %d\n",
					__func__, exynos_pcie->int_min_lock);
		}
#endif
#if IS_ENABLED(CONFIG_CPU_IDLE)
		if (exynos_pcie->use_sicd) {
			dev_info(dev, "%s, ip idle status : %d, idle_ip_index: %d \n",
					__func__, PCIE_IS_IDLE,	exynos_pcie->idle_ip_index);
			exynos_update_ip_idle_status(
					exynos_pcie->idle_ip_index,
					PCIE_IS_IDLE);
		}
#endif
	}

	/* to diable force_pclk_en (& cpm_delay): once more to make sure */
	writel(0x0D, exynos_pcie->phy_pcs_base + 0x0180);
	/* set input clk path change to disable */
	if (exynos_pcie->phy_ops.phy_input_clk_change != NULL) {
		exynos_pcie->phy_ops.phy_input_clk_change(exynos_pcie, 0);
	}

	if (exynos_pcie->use_pcieon_sleep) {
		dev_info(dev, "%s, pcie_is_linkup 0\n", __func__);
		pcie_is_linkup = 0;
	}

	/* TCXO_FAR off */
	hsi_tcxo_far_control(2, 0);

	dev_info(dev, "%s, end of poweroff, pcie state: %d\n",  __func__,
		 exynos_pcie->state);

	return;
}

/* Get EP pci_dev structure of BUS 1 */
static struct pci_dev *exynos_pcie_get_pci_dev(struct pcie_port *pp)
{
	int domain_num;
	struct pci_bus *ep_pci_bus;
	static struct pci_dev *ep_pci_dev;
	struct dw_pcie *pci = to_dw_pcie_from_pp(pp);
	struct exynos_pcie *exynos_pcie = to_exynos_pcie(pci);
	u32 val;

	if (ep_pci_dev != NULL)
		return ep_pci_dev;

	/* Get EP vendor/device ID to get pci_dev structure */
	domain_num = exynos_pcie->pci_dev->bus->domain_nr;
	ep_pci_bus = pci_find_bus(domain_num, 1);

	exynos_pcie_rc_rd_other_conf(pp, ep_pci_bus, 0, PCI_VENDOR_ID, 4, &val);
	/* DBG: dev_info(pci->dev, "(%s): ep_pci_device: vendor/device id = 0x%x\n",
	 *          *                      __func__, val);
	 *                   */

	ep_pci_dev = pci_get_device(val & ID_MASK, (val >> 16) & ID_MASK, NULL);

	return ep_pci_dev;
}


static int exynos_pcie_rc_set_l1ss(int enable, struct pcie_port *pp, int id)
{
	struct dw_pcie *pci = to_dw_pcie_from_pp(pp);
	struct exynos_pcie *exynos_pcie = to_exynos_pcie(pci);
	struct device *dev = pci->dev;
	u32 val;
	unsigned long flags;
	struct pci_dev *ep_pci_dev;
	u32 exp_cap_off = PCIE_CAP_OFFSET;

	/* This function is only working with the devices which support L1SS */
	if (exynos_pcie->ep_device_type != EP_SAMSUNG_MODEM) {
		dev_err(dev, "Can't set L1SS!!! (L1SS not supported)\n");

		return -EINVAL;
	}

	dev_info(dev, "%s:L1SS_START(l1ss_ctrl_id_state=0x%x, id=0x%x, enable=%d)\n",
			__func__, exynos_pcie->l1ss_ctrl_id_state, id, enable);

	if (exynos_pcie->state != STATE_LINK_UP || exynos_pcie->atu_ok == 0) {
		spin_lock_irqsave(&exynos_pcie->conf_lock, flags);
		if (enable)
			exynos_pcie->l1ss_ctrl_id_state &= ~(id);
		else
			exynos_pcie->l1ss_ctrl_id_state |= id;
		spin_unlock_irqrestore(&exynos_pcie->conf_lock, flags);
		dev_info(dev, "%s: It's not needed. This will be set later."
				"(state = 0x%x, id = 0x%x)\n",
				__func__, exynos_pcie->l1ss_ctrl_id_state, id);

		return -1;
	} else {
		ep_pci_dev = exynos_pcie_get_pci_dev(pp);
		if (ep_pci_dev == NULL) {
			dev_err(dev, "Failed to set L1SS %s (pci_dev == NULL)!!!\n",
					enable ? "ENABLE" : "FALSE");
			return -EINVAL;
		}
	}

	spin_lock_irqsave(&exynos_pcie->conf_lock, flags);
	if (enable) {	/* enable == 1 */
		dev_info(dev, "###[PCIe: %s] force pclken disable : 0xC\n", __func__);
		/* force_pclk_en disable*/
		writel(0x0c, exynos_pcie->phy_pcs_base + 0x0180);
		/* P1.CPM Entry pclk 32cycle delay option */
		writel(0x18500000, exynos_pcie->phy_pcs_base + 0x0114);

		mdelay(1);
		exynos_pcie->l1ss_ctrl_id_state &= ~(id);

		if (exynos_pcie->l1ss_ctrl_id_state == 0) {

			/* RC & EP L1SS & ASPM setting */

			/* 1-1 RC, set L1SS */
			exynos_pcie_rc_rd_own_conf(pp, PCIE_LINK_L1SS_CONTROL, 4, &val);
			/* Actual TCOMMON value is 42usec (val = 0x2a << 8) */
			val |= PORT_LINK_TCOMMON_32US | PORT_LINK_L1SS_ENABLE;
			dev_info(dev, "RC L1SS_CONTROL(0x19C) = 0x%x\n", val);
			exynos_pcie_rc_wr_own_conf(pp, PCIE_LINK_L1SS_CONTROL, 4, val);

			/* 1-2 RC: set TPOWERON */
			/* Set TPOWERON value for RC: 90->130 usec */
			exynos_pcie_rc_wr_own_conf(pp, PCIE_LINK_L1SS_CONTROL2, 4,
					PORT_LINK_TPOWERON_130US);

			/* exynos_pcie_rc_wr_own_conf(pp, PCIE_L1_SUBSTATES_OFF, 4,
			 * PCIE_L1_SUB_VAL);
			 */

			/* 1-3 RC: set LTR_EN */
			exynos_pcie_rc_wr_own_conf(pp, exp_cap_off + PCI_EXP_DEVCTL2, 4,
					PCI_EXP_DEVCTL2_LTR_EN);

			/* 2-1 EP: set LTR_EN (reg_addr = 0x98) */
			pci_read_config_dword(ep_pci_dev, exp_cap_off + PCI_EXP_DEVCTL2, &val);
			val |= PCI_EXP_DEVCTL2_LTR_EN;
			pci_write_config_dword(ep_pci_dev, exp_cap_off + PCI_EXP_DEVCTL2, val);

			/* 2-2 EP: set TPOWERON */
			/* Set TPOWERON value for EP: 90->130 usec */
			pci_write_config_dword(ep_pci_dev, PCIE_LINK_L1SS_CONTROL2,
					PORT_LINK_TPOWERON_130US);

			/* 2-3 EP: set Entrance latency */
			/* Set L1.2 Enterance Latency for EP: 64 usec */
			pci_read_config_dword(ep_pci_dev, PCIE_ACK_F_ASPM_CONTROL, &val);
			val &= ~PCIE_L1_ENTERANCE_LATENCY;
			val |= PCIE_L1_ENTERANCE_LATENCY_64us;
			pci_write_config_dword(ep_pci_dev, PCIE_ACK_F_ASPM_CONTROL, val);

			/* 2-4 EP: set L1SS */
			pci_read_config_dword(ep_pci_dev, PCIE_LINK_L1SS_CONTROL, &val);
			val |= PORT_LINK_L1SS_ENABLE;
			pci_write_config_dword(ep_pci_dev, PCIE_LINK_L1SS_CONTROL, val);
			dev_info(dev, "Enable EP L1SS_CONTROL(0x19C) = 0x%x\n", val);

			/* 3. RC ASPM Enable*/
			exynos_pcie_rc_rd_own_conf(pp, exp_cap_off + PCI_EXP_LNKCTL, 4, &val);
			val &= ~PCI_EXP_LNKCTL_ASPMC;
			val |= PCI_EXP_LNKCTL_CCC | PCI_EXP_LNKCTL_ASPM_L1;
			exynos_pcie_rc_wr_own_conf(pp, exp_cap_off + PCI_EXP_LNKCTL, 4, val);
			dev_info(dev, "Enable RC ASPM(0x80) = 0x%x\n", val);

			/* 4. EP ASPM Enable */
			pci_read_config_dword(ep_pci_dev, PCIE_LINK_CTRL_STAT, &val);
			val |= PCI_EXP_LNKCTL_CCC | PCI_EXP_LNKCTL_CLKREQ_EN |
				PCI_EXP_LNKCTL_ASPM_L1;
			pci_write_config_dword(ep_pci_dev, PCIE_LINK_CTRL_STAT, val);
			dev_info(dev, "Enable EP ASPM(0x80) = 0x%x\n", val);

			/* DBG:
			 * dev_info(dev, "(%s): l1ss_enabled(l1ss_ctrl_id_state = 0x%x)\n",
			 *			__func__, exynos_pcie->l1ss_ctrl_id_state);
			 */

		}
	} else {	/* enable == 0 */
		if (exynos_pcie->l1ss_ctrl_id_state) {
			exynos_pcie->l1ss_ctrl_id_state |= id;
		} else {
			exynos_pcie->l1ss_ctrl_id_state |= id;

			/* 1. EP ASPM Disable */
			pci_read_config_dword(ep_pci_dev, PCIE_LINK_CTRL_STAT, &val);
			val &= ~(PCI_EXP_LNKCTL_ASPMC);
			pci_write_config_dword(ep_pci_dev, PCIE_LINK_CTRL_STAT, val);
			dev_info(dev, "Disable EP ASPM(0x80) = 0x%x\n", val);

			/* 2. RC ASPM Disable */
			exynos_pcie_rc_rd_own_conf(pp, exp_cap_off + PCI_EXP_LNKCTL, 4, &val);
			val &= ~PCI_EXP_LNKCTL_ASPMC;
			exynos_pcie_rc_wr_own_conf(pp, exp_cap_off + PCI_EXP_LNKCTL, 4, val);
			dev_info(dev, "Disable RC ASPM(0x80) = 0x%x\n", val);

			/* EP: clear L1SS */
			pci_read_config_dword(ep_pci_dev, PCIE_LINK_L1SS_CONTROL, &val);
			val &= ~(PORT_LINK_L1SS_ENABLE);
			dev_dbg(dev, "Clear L1ss: EP L1SS_CONTROL(0x19C) = 0x%x\n", val);
			pci_write_config_dword(ep_pci_dev, PCIE_LINK_L1SS_CONTROL, val);
			dev_info(dev, "Disable L1SS_CONTROL(0x19C) = 0x%x\n", val);

			/* RC: clear L1SS */
			exynos_pcie_rc_rd_own_conf(pp, PCIE_LINK_L1SS_CONTROL, 4, &val);
			val &= ~(PORT_LINK_L1SS_ENABLE);
			exynos_pcie_rc_wr_own_conf(pp, PCIE_LINK_L1SS_CONTROL, 4, val);
			dev_info(dev, "Disable RC L1SS_CONTROL(0x19C) = 0x%x\n", val);

			/* DBG:
			 * dev_info(dev, "(%s): l1ss_disabled(l1ss_ctrl_id_state = 0x%x)\n",
			 *		__func__, exynos_pcie->l1ss_ctrl_id_state);
			 */
		}
	}
	dev_info(dev, "LTSSM: 0x%08x, PM_STATE = 0x%08x\n",
			exynos_elbi_read(exynos_pcie, PCIE_ELBI_RDLH_LINKUP),
			exynos_phy_pcs_read(exynos_pcie, 0x188));

	spin_unlock_irqrestore(&exynos_pcie->conf_lock, flags);

	dev_info(dev, "%s:L1SS_END(l1ss_ctrl_id_state=0x%x, id=0x%x, enable=%d)\n",
			__func__, exynos_pcie->l1ss_ctrl_id_state, id, enable);

	return 0;
}

int exynos_pcie_rc_l1ss_ctrl(int enable, int id)
{
	struct pcie_port *pp = NULL;
	struct exynos_pcie *exynos_pcie;
	struct dw_pcie *pci;

	int i;

	for (i = 0; i < MAX_RC_NUM; i++) {
		if (g_pcie_rc[i].ep_device_type == EP_SAMSUNG_MODEM) {
			exynos_pcie = &g_pcie_rc[i];
			pci = exynos_pcie->pci;
			pp = &pci->pp;
			break;
		}
	}

	if (pp != NULL)
		return	exynos_pcie_rc_set_l1ss(enable, pp, id);
	else
		return -EINVAL;
}
EXPORT_SYMBOL(exynos_pcie_rc_l1ss_ctrl);

int exynos_pcie_host_v1_poweron(int ch_num)
{
	return exynos_pcie_rc_poweron(ch_num);
}
EXPORT_SYMBOL(exynos_pcie_host_v1_poweron);

void exynos_pcie_host_v1_poweroff(int ch_num)
{
	return exynos_pcie_rc_poweroff(ch_num);
}
EXPORT_SYMBOL(exynos_pcie_host_v1_poweroff);

/* PCIe link status checking function from S359 ref. code */
int exynos_pcie_rc_chk_link_status(int ch_num)
{
	struct exynos_pcie *exynos_pcie = &g_pcie_rc[ch_num];
	struct dw_pcie *pci;
	struct device *dev;

	u32 val;
	int link_status;

	if (!exynos_pcie) {
		pr_err("%s: ch#%d PCIe device is not loaded\n", __func__, ch_num);
		return -ENODEV;
	}
	pci = exynos_pcie->pci;
	dev = pci->dev;

	if (exynos_pcie->state == STATE_LINK_DOWN)
		return 0;

	if (exynos_pcie->ep_device_type == EP_SAMSUNG_MODEM) {
		val = readl(exynos_pcie->elbi_base + PCIE_ELBI_RDLH_LINKUP) & 0x1f;
		if (val >= 0x0d && val <= 0x14) {
			link_status = 1;
		} else {
			dev_err(dev, "Check unexpected state - H/W:0x%x, S/W:%d\n",
					val, exynos_pcie->state);
			/* exynos_pcie->state = STATE_LINK_DOWN; */
			link_status = 1;
		}

		return link_status;
	}

	return 0;
}
EXPORT_SYMBOL(exynos_pcie_rc_chk_link_status);

int exynos_pcie_host_v1_register_event(struct exynos_pcie_register_event *reg)
{
	int ret = 0;
	struct pcie_port *pp;
	struct exynos_pcie *exynos_pcie;
	struct dw_pcie *pci;

	if (!reg) {
		pr_err("PCIe: Event registration is NULL\n");
		return -ENODEV;
	}
	if (!reg->user) {
		pr_err("PCIe: User of event registration is NULL\n");
		return -ENODEV;
	}
	pp = PCIE_BUS_PRIV_DATA(((struct pci_dev *)reg->user));
	pci = to_dw_pcie_from_pp(pp);
	exynos_pcie = to_exynos_pcie(pci);

	if (pp) {
		exynos_pcie->event_reg = reg;
		dev_info(pci->dev,
				"Event 0x%x is registered for RC %d\n",
				reg->events, exynos_pcie->ch_num);
	} else {
		pr_err("PCIe: did not find RC for pci endpoint device\n");
		ret = -ENODEV;
	}
	return ret;
}
EXPORT_SYMBOL(exynos_pcie_host_v1_register_event);

int exynos_pcie_host_v1_deregister_event(struct exynos_pcie_register_event *reg)
{
	int ret = 0;
	struct pcie_port *pp;
	struct exynos_pcie *exynos_pcie;
	struct dw_pcie *pci;

	if (!reg) {
		pr_err("PCIe: Event deregistration is NULL\n");
		return -ENODEV;
	}
	if (!reg->user) {
		pr_err("PCIe: User of event deregistration is NULL\n");
		return -ENODEV;
	}

	pp = PCIE_BUS_PRIV_DATA(((struct pci_dev *)reg->user));
	pci = to_dw_pcie_from_pp(pp);
	exynos_pcie = to_exynos_pcie(pci);

	if (pp) {
		exynos_pcie->event_reg = NULL;
		dev_info(pci->dev, "Event is deregistered for RC %d\n",
				exynos_pcie->ch_num);
	} else {
		pr_err("PCIe: did not find RC for pci endpoint device\n");
		ret = -ENODEV;
	}
	return ret;
}
EXPORT_SYMBOL(exynos_pcie_host_v1_deregister_event);

int exynos_pcie_rc_set_affinity(int ch_num, int affinity)
{
	struct exynos_pcie *exynos_pcie = &g_pcie_rc[ch_num];
	struct dw_pcie *pci;
	struct pcie_port *pp;

	if (!exynos_pcie) {
		pr_err("%s: ch#%d PCIe device is not loaded\n", __func__, ch_num);
		return -ENODEV;
	}

	pci = exynos_pcie->pci;
	pp = &pci->pp;

	//irq_set_affinity(pp->irq, cpumask_of(affinity)); //No EXPORT_SYMBOL

	return 0;
}
EXPORT_SYMBOL(exynos_pcie_rc_set_affinity);

#if IS_ENABLED(CONFIG_CPU_IDLE)
static void __maybe_unused exynos_pcie_rc_set_tpoweron(struct pcie_port *pp,
							int max)
{
	void __iomem *ep_dbi_base = pp->va_cfg0_base;
	struct dw_pcie *pci = to_dw_pcie_from_pp(pp);
	struct exynos_pcie *exynos_pcie = to_exynos_pcie(pci);
	u32 val;

	if (exynos_pcie->state != STATE_LINK_UP)
		return;

	/* Disable ASPM */
	val = readl(ep_dbi_base + WIFI_L1SS_LINKCTRL);
	val &= ~(WIFI_ASPM_CONTROL_MASK);
	writel(val, ep_dbi_base + WIFI_L1SS_LINKCTRL);

	val = readl(ep_dbi_base + WIFI_L1SS_CONTROL);
	writel(val & ~(WIFI_ALL_PM_ENABEL),
			ep_dbi_base + WIFI_L1SS_CONTROL);

	if (max) {
		writel(PORT_LINK_TPOWERON_3100US,
				ep_dbi_base + WIFI_L1SS_CONTROL2);
		exynos_pcie_rc_wr_own_conf(pp, PCIE_LINK_L1SS_CONTROL2, 4,
				PORT_LINK_TPOWERON_3100US);
	} else {
		writel(PORT_LINK_TPOWERON_130US,
				ep_dbi_base + WIFI_L1SS_CONTROL2);
		exynos_pcie_rc_wr_own_conf(pp, PCIE_LINK_L1SS_CONTROL2, 4,
				PORT_LINK_TPOWERON_130US);
	}

	/* Enable L1ss */
	val = readl(ep_dbi_base + WIFI_L1SS_LINKCTRL);
	val |= WIFI_ASPM_L1_ENTRY_EN;
	writel(val, ep_dbi_base + WIFI_L1SS_LINKCTRL);

	val = readl(ep_dbi_base + WIFI_L1SS_CONTROL);
	val |= WIFI_ALL_PM_ENABEL;
	writel(val, ep_dbi_base + WIFI_L1SS_CONTROL);
}

/* Temporary remove: Need to enable to use sicd powermode */
#ifdef PCIE_SICD
static int exynos_pcie_rc_power_mode_event(struct notifier_block *nb,
					unsigned long event, void *data)
{
	int ret = NOTIFY_DONE;
	struct exynos_pcie *exynos_pcie = container_of(nb,
				struct exynos_pcie, power_mode_nb);
	u32 val;
	struct dw_pcie *pci = exynos_pcie->pci;
	struct pcie_port *pp = &pci->pp;

	dev_info(pci->dev, "[%s] event: %lx\n", __func__, event);
	switch (event) {
	case SICD_ENTER:
		if (exynos_pcie->use_sicd) {
			if (exynos_pcie->ip_ver >= 0x889500) {
				if (exynos_pcie->state != STATE_LINK_DOWN) {
					val = readl(exynos_pcie->elbi_base +
						PCIE_ELBI_RDLH_LINKUP) & 0x1f;
					if (val == 0x14 || val == 0x15) {
						ret = NOTIFY_DONE;
						/* Change tpower on time to
						 * value
						 */
						exynos_pcie_rc_set_tpoweron(pp, 1);
					} else {
						ret = NOTIFY_BAD;
					}
				}
			}
		}
		break;
	case SICD_EXIT:
		if (exynos_pcie->use_sicd) {
			if (exynos_pcie->ip_ver >= 0x889500) {
				if (exynos_pcie->state != STATE_LINK_DOWN) {
					/* Change tpower on time to NORMAL
					 * value */
					exynos_pcie_rc_set_tpoweron(pp, 0);
				}
			}
		}
		break;
	default:
		ret = NOTIFY_DONE;
	}

	return notifier_from_errno(ret);
}
#endif
#endif
static int exynos_pcie_msi_set_affinity(struct irq_data *irq_data,
				   const struct cpumask *mask, bool force)
{
	return 0;
}

#if IS_ENABLED(CONFIG_EXYNOS_ITMON)
int exynos_pcie_rc_itmon_notifier(struct notifier_block *nb,
		unsigned long action, void *nb_data)
{
	struct exynos_pcie *exynos_pcie = container_of(nb, struct exynos_pcie, itmon_nb);
	struct device *dev = exynos_pcie->pci->dev;
	struct itmon_notifier *itmon_info = nb_data;
	unsigned int val;

	dev_info(dev, "### EXYNOS PCIE ITMON ### \n");

	if (IS_ERR_OR_NULL(itmon_info))
		return NOTIFY_DONE;

	/* only for 9830 HSI2 block */
	if (exynos_pcie->ip_ver == 0x983000) {
		if((itmon_info->port && !strcmp(itmon_info->port, "HSI2")) ||
				(itmon_info->dest && !strcmp(itmon_info->dest, "HSI2"))) {
			regmap_read(exynos_pcie->pmureg, exynos_pcie->pmu_offset, &val);
			dev_info(dev, "### PMU PHY Isolation : 0x%x\n", val);

			exynos_pcie_rc_register_dump(exynos_pcie->ch_num);
		}
	} else {
		dev_info(dev, "skip register dump(ip_ver = 0x%x)\n", exynos_pcie->ip_ver);
	}

	return NOTIFY_DONE;
}
#endif

static int exynos_pcie_rc_add_port(struct platform_device *pdev,
					struct pcie_port *pp)
{
	struct dw_pcie *pci = to_dw_pcie_from_pp(pp);
	struct exynos_pcie *exynos_pcie = to_exynos_pcie(pci);
	struct irq_domain *msi_domain;
	struct msi_domain_info *msi_domain_info;
	int ret;
	u32 val, vendor_id, device_id;

	pp->irq = platform_get_irq(pdev, 0);
	if (!pp->irq) {
		dev_err(&pdev->dev, "failed to get irq\n");
		return -ENODEV;
	}
	ret = devm_request_irq(&pdev->dev, pp->irq, exynos_pcie_rc_irq_handler,
				IRQF_SHARED | IRQF_TRIGGER_HIGH, "exynos-pcie", pp);
	if (ret) {
		dev_err(&pdev->dev, "failed to request irq\n");
		return ret;
	}

	pp->root_bus_nr = 0;
	pp->ops = &exynos_pcie_rc_ops;

	exynos_pcie_setup_rc(pp);

	spin_lock_init(&exynos_pcie->conf_lock);
	ret = dw_pcie_host_init(pp);
	if (ret) {
		dev_err(&pdev->dev, "failed to dw pcie host init\n");
		return ret;
	}

	exynos_pcie_rc_rd_own_conf(pp, PCI_VENDOR_ID, 4, &val);
	vendor_id = val & ID_MASK;
	device_id = (val >> 16) & ID_MASK;
	exynos_pcie->pci_dev = pci_get_device(vendor_id,
			device_id, NULL);
	if (!exynos_pcie->pci_dev) {
		dev_err(pci->dev, "Failed to get pci device\n");
	}
	dev_dbg(pci->dev, "(%s): ep_pci_device: vendor/device id = 0x%x\n", __func__, val);

	exynos_pcie->pci_dev->dma_mask = DMA_BIT_MASK(36);
	exynos_pcie->pci_dev->dev.coherent_dma_mask = DMA_BIT_MASK(36);
	dma_set_seg_boundary(&(exynos_pcie->pci_dev->dev), DMA_BIT_MASK(36));

	if (pp->msi_domain) {
		msi_domain = pp->msi_domain;
		msi_domain_info = (struct msi_domain_info *)msi_domain->host_data;
		msi_domain_info->chip->irq_set_affinity = exynos_pcie_msi_set_affinity;
	}

	return 0;
}

static void exynos_pcie_rc_pcie_ops_init(struct pcie_port *pp)
{
	struct dw_pcie *pci = to_dw_pcie_from_pp(pp);
	struct exynos_pcie *exynos_pcie = to_exynos_pcie(pci);
	struct exynos_pcie_ops *pcie_ops = &exynos_pcie->exynos_pcie_ops;
	struct device *dev = pci->dev;

	dev_info(dev, "Initialize PCIe function.\n");

	pcie_ops->poweron = exynos_pcie_rc_poweron;
	pcie_ops->poweroff = exynos_pcie_rc_poweroff;
	pcie_ops->rd_own_conf = exynos_pcie_rc_rd_own_conf;
	pcie_ops->wr_own_conf = exynos_pcie_rc_wr_own_conf;
	pcie_ops->rd_other_conf = exynos_pcie_rc_rd_other_conf;
	pcie_ops->wr_other_conf = exynos_pcie_rc_wr_other_conf;
}

static int exynos_pcie_rc_make_reg_tb(struct device *dev, struct exynos_pcie *exynos_pcie)
{
	unsigned int pos, val, id;
	int i;

	/* initialize the reg table */
	for (i = 0; i < 48; i++) {
		exynos_pcie->pci_cap[i] = 0;
		exynos_pcie->pci_ext_cap[i] = 0;
	}

	pos = 0xFF & readl(exynos_pcie->rc_dbi_base + PCI_CAPABILITY_LIST);

	while (pos) {
		val = readl(exynos_pcie->rc_dbi_base + pos);
		id = val & CAP_ID_MASK;
		exynos_pcie->pci_cap[id] = pos;
		pos = (readl(exynos_pcie->rc_dbi_base + pos) & CAP_NEXT_OFFSET_MASK) >> 8;
		dev_dbg(dev, "Next Cap pointer : 0x%x\n", pos);
	}

	pos = PCI_CFG_SPACE_SIZE;

	while (pos) {
		val = readl(exynos_pcie->rc_dbi_base + pos);
		if (val == 0) {
			dev_info(dev, "we have no ext capabilities!\n");
			break;
		}
		id = PCI_EXT_CAP_ID(val);
		exynos_pcie->pci_ext_cap[id] = pos;
		pos = PCI_EXT_CAP_NEXT(val);
		dev_dbg(dev, "Next ext Cap pointer : 0x%x\n", pos);
	}

	for (i = 0; i < 48; i++) {
		if (exynos_pcie->pci_cap[i])
			dev_info(dev, "PCIe cap [0x%x][%s]: 0x%x\n", i, CAP_ID_NAME(i), exynos_pcie->pci_cap[i]);
	}
	for (i = 0; i < 48; i++) {
		if (exynos_pcie->pci_ext_cap[i])
			dev_info(dev, "PCIe ext cap [0x%x][%s]: 0x%x\n", i, EXT_CAP_ID_NAME(i), exynos_pcie->pci_ext_cap[i]);
	}
	return 0;
}

u32 pcie_linkup_stat(void)
{
	pr_info("[%s] pcie_is_linkup : %d\n", __func__, pcie_is_linkup);
	return pcie_is_linkup;
}
EXPORT_SYMBOL_GPL(pcie_linkup_stat);

static int exynos_pcie_rc_probe(struct platform_device *pdev)
{
	struct exynos_pcie *exynos_pcie;
	struct dw_pcie *pci;
	struct pcie_port *pp;
	struct device_node *np = pdev->dev.of_node;
	int ret = 0;
	int ch_num;

	dev_info(&pdev->dev, "## PCIe RC PROBE start \n");

	if (create_pcie_sys_file(&pdev->dev))
		dev_err(&pdev->dev, "Failed to create pcie sys file\n");

	if (of_property_read_u32(np, "ch-num", &ch_num)) {
		dev_err(&pdev->dev, "Failed to parse the channel number\n");
		return -EINVAL;
	}

	dev_info(&pdev->dev, "## PCIe ch %d ##\n", ch_num);

	pci = devm_kzalloc(&pdev->dev, sizeof(*pci), GFP_KERNEL);
	if (!pci) {
		dev_err(&pdev->dev, "dw_pcie allocation is failed\n");
		return -ENOMEM;
	}

	exynos_pcie = &g_pcie_rc[ch_num];
	exynos_pcie->pci = pci;

	pci->dev = &pdev->dev;
	pci->ops = &dw_pcie_ops;

	pp = &pci->pp;

	exynos_pcie->ch_num = ch_num;
	exynos_pcie->l1ss_enable = 1;
	exynos_pcie->state = STATE_LINK_DOWN;

	exynos_pcie->linkdown_cnt = 0;
	exynos_pcie->l1ss_ctrl_id_state = 0;
	exynos_pcie->atu_ok = 0;

	exynos_pcie->app_req_exit_l1 = PCIE_APP_REQ_EXIT_L1;
	exynos_pcie->app_req_exit_l1_mode = PCIE_APP_REQ_EXIT_L1_MODE;
	exynos_pcie->linkup_offset = PCIE_ELBI_RDLH_LINKUP;

	pm_runtime_enable(&pdev->dev);
	pm_runtime_get_sync(&pdev->dev);

	dma_set_mask_and_coherent(&pdev->dev, DMA_BIT_MASK(36));
	platform_set_drvdata(pdev, exynos_pcie);

	/* parsing pcie dts data for exynos */
	ret = exynos_pcie_rc_parse_dt(&pdev->dev, exynos_pcie);
	if (ret)
		goto probe_fail;

	ret = exynos_pcie_rc_get_pin_state(pdev, exynos_pcie);
	if (ret)
		goto probe_fail;

	ret = exynos_pcie_rc_clock_get(pp);
	if (ret)
		goto probe_fail;

	ret = exynos_pcie_rc_get_resource(pdev, exynos_pcie);
	if (ret)
		goto probe_fail;
	pci->dbi_base = exynos_pcie->rc_dbi_base;

	/* TCXO_FAR on */
	hsi_tcxo_far_control(2, 1);

	/* NOTE: TDB */
	/* Mapping PHY functions */
	exynos_pcie_rc_phy_init(pp);

	exynos_pcie_rc_pcie_ops_init(pp);

	exynos_pcie_rc_resumed_phydown(pp);

	if (exynos_pcie->use_nclkoff_en)
		exynos_pcie_rc_nclkoff_ctrl(pdev, exynos_pcie);

	/* if it needed for msi init, property should be added on dt */
	//set_dma_ops(&pdev->dev, &exynos_pcie_dma_ops); //why need?
	//dev_info(&pdev->dev, "DMA opertaions are changed\n");

	ret = exynos_pcie_rc_make_reg_tb(&pdev->dev, exynos_pcie);
	if (ret)
		goto probe_fail;

	ret = exynos_pcie_rc_add_port(pdev, pp);
	if (ret)
		goto probe_fail;

	if (exynos_pcie->use_cache_coherency)
		exynos_pcie_rc_set_iocc(pp, 1);

	disable_irq(pp->irq);

#if IS_ENABLED(CONFIG_CPU_IDLE)
	exynos_pcie->idle_ip_index =
			exynos_get_idle_ip_index(dev_name(&pdev->dev), 0);
	if (exynos_pcie->idle_ip_index < 0) {
		dev_err(&pdev->dev, "Cant get idle_ip_dex!!!\n");
	} else {
		dev_err(&pdev->dev, "PCIE idle ip index : %d\n",
				exynos_pcie->idle_ip_index);
	}

	exynos_update_ip_idle_status(exynos_pcie->idle_ip_index, PCIE_IS_IDLE);
	dev_info(&pdev->dev, "%s, ip idle status : %d, idle_ip_index: %d \n",
					__func__, PCIE_IS_IDLE,	exynos_pcie->idle_ip_index);

/* Temporary remove: Need to enable to use sicd powermode */
#ifdef PCIE_SICD
	exynos_pcie->power_mode_nb.notifier_call = exynos_pcie_rc_power_mode_event;
	exynos_pcie->power_mode_nb.next = NULL;
	exynos_pcie->power_mode_nb.priority = 0;

	//ret = exynos_cpupm_notifier_register(&exynos_pcie->power_mode_nb);
	//if (ret) {
	//	dev_err(&pdev->dev, "Failed to register lpa notifier\n");
	//	goto probe_fail;
	//}
#endif
#endif

	exynos_pcie->pcie_wq = create_freezable_workqueue("pcie_wq");
	if (IS_ERR(exynos_pcie->pcie_wq)) {
		dev_err(&pdev->dev, "couldn't create workqueue\n");
		ret = EBUSY;
		goto probe_fail;
	}

	INIT_DELAYED_WORK(&exynos_pcie->dislink_work,
				exynos_pcie_rc_dislink_work);
	INIT_DELAYED_WORK(&exynos_pcie->cpl_timeout_work,
						exynos_pcie_rc_cpl_timeout_work);

#if IS_ENABLED(CONFIG_EXYNOS_ITMON)
	exynos_pcie->itmon_nb.notifier_call = exynos_pcie_rc_itmon_notifier;
	//itmon_notifier_chain_register(&exynos_pcie->itmon_nb);//!!!tmp
#endif
#if 0
	if (exynos_pcie->use_pcieon_sleep) {
		dev_info(&pdev->dev, "## register pcie connection function\n");
		register_pcie_is_connect(pcie_linkup_stat);
	}
#endif

	/* TCXO_FAR on */
	hsi_tcxo_far_control(2, 1);

	/* force set input clk path change to disable */
	if (exynos_pcie->phy_ops.phy_input_clk_change != NULL) {
		exynos_pcie->phy_ops.phy_input_clk_change(exynos_pcie, 0);
	}

	/* force phy all power down */
	if (exynos_pcie->phy_ops.phy_all_pwrdn != NULL) {
		pr_info("%s: force one more all pwrdn", __func__);
		exynos_pcie->phy_ops.phy_all_pwrdn(exynos_pcie, exynos_pcie->ch_num);
	}

	/* TCXO_FAR off */
	hsi_tcxo_far_control(2, 0);

	platform_set_drvdata(pdev, exynos_pcie);

probe_fail:

	if (ret)
		dev_err(&pdev->dev, "## %s: PCIe probe failed\n", __func__);
	else
		dev_info(&pdev->dev, "## %s: PCIe probe success\n", __func__);

	return ret;
}

static int __exit exynos_pcie_rc_remove(struct platform_device *pdev)
{
	dev_info(&pdev->dev, "%s\n", __func__);

	return 0;
}

#if IS_ENABLED(CONFIG_PM)
static int exynos_pcie_rc_suspend_noirq(struct device *dev)
{
	struct exynos_pcie *exynos_pcie = dev_get_drvdata(dev);

	dev_info(dev, "## SUSPEND[%s]: %s(pcie_is_linkup: %d)\n", __func__,
			EXUNOS_PCIE_STATE_NAME(exynos_pcie->state),
			pcie_is_linkup);

	if (exynos_pcie->state == STATE_LINK_DOWN) {
		dev_info(dev, "%s: RC%d already off\n", __func__, exynos_pcie->ch_num);
	}

	return 0;
}

static int exynos_pcie_rc_resume_noirq(struct device *dev)
{
	struct exynos_pcie *exynos_pcie = dev_get_drvdata(dev);
	struct dw_pcie *pci = exynos_pcie->pci;

	dev_info(dev, "## RESUME[%s]: %s(pcie_is_linkup: %d)\n", __func__,
			EXUNOS_PCIE_STATE_NAME(exynos_pcie->state),
			pcie_is_linkup);

	if (!exynos_pcie->pci_dev) {
		dev_err(pci->dev, "Failed to get pci device\n");
	} else {
		exynos_pcie->pci_dev->state_saved = false;
		dev_dbg(&(exynos_pcie->pci_dev->dev), "state_saved flag = false");
	}
	if (exynos_pcie->state == STATE_LINK_DOWN) {
		dev_info(dev, "%s: RC%d Link down state-> phypwr off\n", __func__,
							exynos_pcie->ch_num);

		/* TCXO_FAR on */
		hsi_tcxo_far_control(2, 1);

		exynos_pcie_rc_resumed_phydown(&pci->pp);
	}

	return 0;
}
#endif

static const struct dev_pm_ops exynos_pcie_rc_pm_ops = {
	.suspend_noirq	= exynos_pcie_rc_suspend_noirq,
	.resume_noirq	= exynos_pcie_rc_resume_noirq,
};

static const struct of_device_id exynos_pcie_rc_of_match[] = {
	{ .compatible = "samsung,exynos-pcie-rc", },
	{},
};

static struct platform_driver exynos_pcie_rc_driver = {
	.probe		= exynos_pcie_rc_probe,
	.remove		= exynos_pcie_rc_remove,
	.driver = {
		.name		= "exynos-pcie-rc",
		.owner		= THIS_MODULE,
		.of_match_table = exynos_pcie_rc_of_match,
		.pm		= &exynos_pcie_rc_pm_ops,
	},
};
module_platform_driver(exynos_pcie_rc_driver);

MODULE_AUTHOR("Kyounghye Yun <k-hye.yun@samsung.com>");
MODULE_AUTHOR("Seungo Jang <seungo.jang@samsung.com>");
MODULE_DESCRIPTION("Samsung PCIe RootComplex controller driver");
MODULE_LICENSE("GPL v2");
