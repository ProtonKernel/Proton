/*
 * PCIe phy driver for Samsung EXYNOS9830
 *
 * Copyright (C) 2019 Samsung Electronics Co., Ltd.
 *		http://www.samsung.com
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/types.h>
#include <linux/of_gpio.h>
#include <linux/pci.h>
#include <linux/delay.h>
#include <linux/exynos-pci-noti.h>
#include <linux/regmap.h>
#include "pcie-designware.h"
#include "pcie-exynos-common.h"
#include "pcie-exynos-rc.h"

#if IS_ENABLED(CONFIG_EXYNOS_OTP)
#include <linux/exynos_otp.h>
#endif

/* avoid checking rx elecidle when access DBI */
void exynos_pcie_rc_phy_check_rx_elecidle(void *phy_pcs_base_regs, int val, int ch_num)
{
	//Todo need guide
}

/* PHY all power down */
void exynos_pcie_rc_phy_all_pwrdn(struct exynos_pcie *exynos_pcie, int ch_num)
{
	void *phy_base_regs = exynos_pcie->phy_base;

	pr_info("%s: phybase + 0x400 : 0x23\n", __func__);

	writel(0x20, phy_base_regs + 0x408);
	writel(0x0A, phy_base_regs + 0x40C);

	writel(0x0A, phy_base_regs + 0x800);
	writel(0xBF, phy_base_regs + 0x804);
	writel(0x02, phy_base_regs + 0xA40);
	writel(0x2A, phy_base_regs + 0xA44);
	writel(0xAA, phy_base_regs + 0xA48);
	writel(0xA8, phy_base_regs + 0xA4C);
	writel(0x80, phy_base_regs + 0xA50);

	writel(0x0A, phy_base_regs + 0x1000);
	writel(0xBF, phy_base_regs + 0x1004);
	writel(0x02, phy_base_regs + 0x1240);
	writel(0x2A, phy_base_regs + 0x1244);
	writel(0xAA, phy_base_regs + 0x1248);
	writel(0xA8, phy_base_regs + 0x124C);
	writel(0x80, phy_base_regs + 0x1250);
}

/* PHY all power down clear */
void exynos_pcie_rc_phy_all_pwrdn_clear(struct exynos_pcie *exynos_pcie, int ch_num)
{
	void __iomem *phy_base_regs = exynos_pcie->phy_base;

	pr_info("%s: phybase + 0x400 : 0x0\n", __func__);

	writel(0x28, phy_base_regs + 0xD8);
	udelay(10);

	writel(0x00, phy_base_regs + 0x408);
	writel(0x00, phy_base_regs + 0x40C);

	writel(0x00, phy_base_regs + 0x800);
	writel(0x00, phy_base_regs + 0x804);
	writel(0x00, phy_base_regs + 0xA40);
	writel(0x00, phy_base_regs + 0xA44);
	writel(0x00, phy_base_regs + 0xA48);
	writel(0x00, phy_base_regs + 0xA4C);
	writel(0x00, phy_base_regs + 0xA50);

	writel(0x00, phy_base_regs + 0x1000);
	writel(0x00, phy_base_regs + 0x1004);
	writel(0x00, phy_base_regs + 0x1240);
	writel(0x00, phy_base_regs + 0x1244);
	writel(0x00, phy_base_regs + 0x1248);
	writel(0x00, phy_base_regs + 0x124C);
	writel(0x00, phy_base_regs + 0x1250);
}

/* PHY input clk change */
void exynos_pcie_rc_phy_input_clk_change(struct exynos_pcie *exynos_pcie, bool enable)
{
	void __iomem *phy_base_regs = exynos_pcie->phy_base;

	if (enable) {
		/* pr_info("%s: set input clk path to enable\n", __func__); */
		writel(0x28, phy_base_regs + 0xD8);
		udelay(100);
	} else {
		/* pr_info("%s: set input clk path to disable\n", __func__); */
		udelay(100);
		writel(0x6D, phy_base_regs + 0xD8);
	}
	/* pr_info("%s: input clk path change (phy base + 0xD8 = 0x%x\n",
	   __func__, readl(phy_base_regs + 0xD8)); */
}

void exynos_pcie_rc_pcie_phy_otp_config(void *phy_base_regs, int ch_num)
{
#if IS_ENABLED(CONFIG_EXYNOS_OTP)
#else
	return ;
#endif
}
#define LCPLL_REF_CLK_SEL	(0x3 << 4)
void exynos_pcie_rc_pcie_phy_config(struct exynos_pcie *exynos_pcie, int ch_num)
{
	void __iomem *elbi_base_regs = exynos_pcie->elbi_base;
	void __iomem *phy_base_regs = exynos_pcie->phy_base;
	void __iomem *phy_pcs_base_regs = exynos_pcie->phy_pcs_base;
	void __iomem *sysreg_base_regs = exynos_pcie->sysreg_base;
	u32 val;
	u32 i = 0;
	u32 temp = 0;
	u32 lane = 2;

	val = readl(sysreg_base_regs);

	writel(0x28, phy_base_regs + 0xD8);
	udelay(100);
	pr_info("%s: input clk path change init(phy base + 0xD8 = 0x%x\n",
			__func__, readl(phy_base_regs + 0xD8));

	if (exynos_pcie->chip_ver == 1)
	{
		val = readl(phy_pcs_base_regs + 0x008);
		val = val | 0x00000080;
		writel(val, phy_pcs_base_regs + 0x008);
	}
	/* pma_setting */

	//Common
	writel(0x50, phy_base_regs + 0x018);
	writel(0x33, phy_base_regs + 0x048);
	writel(0x01, phy_base_regs + 0x068);
	writel(0x12, phy_base_regs + 0x070);
	writel(0x00, phy_base_regs + 0x08C);
	writel(0x21, phy_base_regs + 0x090);
	writel(0x14, phy_base_regs + 0x0B0);
	writel(0x50, phy_base_regs + 0x0B8);
	writel(0x51, phy_base_regs + 0x0E0);
	writel(0x00, phy_base_regs + 0x100);
	writel(0x80, phy_base_regs + 0x104);
	writel(0x38, phy_base_regs + 0x140);
	writel(0xA4, phy_base_regs + 0x180);
	writel(0x03, phy_base_regs + 0x188);	//LCPLL 100MHz no divide
	writel(0x38, phy_base_regs + 0x2A8);
	writel(0x12, phy_base_regs + 0x2E4);
	writel(0xC0, phy_base_regs + 0x400); //PMA enable and bifurcation mode enable
	writel(0x20, phy_base_regs + 0x408);
	writel(0x00, phy_base_regs + 0x550);
	writel(0x00, phy_base_regs + 0x5A8);
	writel(0xFF, phy_base_regs + 0x5EC);


	//if (PCIe_TYPE == RC) {
	writel(0x02, phy_base_regs + 0x458); //100MHz CLK on
	writel(0x34, phy_base_regs + 0x5B0);	//diff, control REFCLK source (XO)
	writel(0x20, phy_base_regs + 0x450);	//when entering L1.2, add delay.
	//} else {
	//	writel(0x2C, phy_base_regs + 0x340);
	//	writel(0x0A, phy_base_regs + 0x440);
	//	writel(0x55, phy_base_regs + 0x47C);
	//	writel(0x35, phy_base_regs + 0x5B0);	//diff, control REFCLK source (ERIO)
	//	writel(0x1F, phy_base_regs + 0x7D4);
	//}

	//lane
	for (i = 0; i < lane; i++) {
		phy_base_regs += (i * 0x800);

		writel(0x80, phy_base_regs + 0x878);
		writel(0x40, phy_base_regs + 0x894);
		writel(0x00, phy_base_regs + 0x8C0);
		writel(0x30, phy_base_regs + 0x8F4);
		writel(0x05, phy_base_regs + 0x908);
		writel(0xE0, phy_base_regs + 0x90C);
		writel(0xD3, phy_base_regs + 0x91C);
		writel(0x01, phy_base_regs + 0x924);
		writel(0x41, phy_base_regs + 0x930);
		writel(0x15, phy_base_regs + 0x934);
		writel(0x13, phy_base_regs + 0x938);
		writel(0xFC, phy_base_regs + 0x94C);
		writel(0x10, phy_base_regs + 0x954);
		writel(0x69, phy_base_regs + 0x958);
		writel(0x40, phy_base_regs + 0x964);
		writel(0xF6, phy_base_regs + 0x9B4);
		writel(0x2D, phy_base_regs + 0x9C0);
		writel(0x7E, phy_base_regs + 0x9DC);
		writel(0x02, phy_base_regs + 0xA40);
		writel(0x26, phy_base_regs + 0xA70);
		writel(0x00, phy_base_regs + 0xA74);
		writel(0x1B, phy_base_regs + 0xC10);
		writel(0x10, phy_base_regs + 0xC44);
		writel(0x10, phy_base_regs + 0xC48);
		writel(0x10, phy_base_regs + 0xC4C);
		writel(0x10, phy_base_regs + 0xC50);
		writel(0x02, phy_base_regs + 0xC54);
		writel(0x02, phy_base_regs + 0xC58);
		writel(0x02, phy_base_regs + 0xC5C);
		writel(0x02, phy_base_regs + 0xC60);
		writel(0x02, phy_base_regs + 0xC6C);
		writel(0x02, phy_base_regs + 0xC70);
		writel(0xE7, phy_base_regs + 0xCA8);
		writel(0x00, phy_base_regs + 0xCAC);
		writel(0x0E, phy_base_regs + 0xCB0);
		writel(0x1C, phy_base_regs + 0xCCC);
		writel(0x05, phy_base_regs + 0xCD4);
		writel(0x2F, phy_base_regs + 0xDB4);

		/* TX idrv setting for TX termination resistor */
		writel(0x00, phy_base_regs + 0x9CC);
		writel(0xFF, phy_base_regs + 0xCD8);
		writel(0x6E, phy_base_regs + 0xCDC);
		writel(0x0F, phy_base_regs + 0x82C);
		writel(0x60, phy_base_regs + 0x830);
		writel(0x7E, phy_base_regs + 0x834);

		/* TX pre, post lvl setting */
		writel(0x10, phy_base_regs + 0x824);
		writel(0x4F, phy_base_regs + 0x82C);
		writel(0x1F, phy_base_regs + 0x818);
		writel(0x00, phy_base_regs + 0x820);

		/* RX tuning */
		writel(0xF4, phy_base_regs + 0x914);
		writel(0xCA, phy_base_regs + 0x920);
		writel(0x3D, phy_base_regs + 0x928);
		writel(0xB8, phy_base_regs + 0x92C);
		writel(0x17, phy_base_regs + 0x934);
		writel(0x4C, phy_base_regs + 0x93C);
		writel(0x73, phy_base_regs + 0x948);
		writel(0x55, phy_base_regs + 0x96C);
		writel(0x78, phy_base_regs + 0x988);
		writel(0x3B, phy_base_regs + 0x994);
		writel(0xFF, phy_base_regs + 0x9C4);
		writel(0x20, phy_base_regs + 0x9C8);
		writel(0x2F, phy_base_regs + 0xA08);
		writel(0x3F, phy_base_regs + 0xB9C);
		writel(0x05, phy_base_regs + 0xC08);
		writel(0x04, phy_base_regs + 0xC3C);
		writel(0x04, phy_base_regs + 0xC40);

		writel(0x06, phy_base_regs + 0xB40);
		writel(0x06, phy_base_regs + 0xB44);
		writel(0x01, phy_base_regs + 0xB48);
		writel(0x00, phy_base_regs + 0xB4C);
		writel(0x03, phy_base_regs + 0xB50);
		writel(0x03, phy_base_regs + 0xB54);
		writel(0x01, phy_base_regs + 0xB58);
		writel(0x00, phy_base_regs + 0xB5C);
	}

	/* pma init, pma cmn rst */
	writel(0x1, elbi_base_regs + 0x1404);
	writel(0x1, elbi_base_regs + 0x1408);
	writel(0x0, elbi_base_regs + 0x1404);
	writel(0x0, elbi_base_regs + 0x1408);
	udelay(1);
	writel(0x1, elbi_base_regs + 0x1404);
	writel(0x1, elbi_base_regs + 0x1408);

	/* offset calibration w/a */
	phy_base_regs = exynos_pcie->phy_base;
	for (i = 0; i < lane; i++) {
		phy_base_regs += (i * 0x800);
		writel(0xF7, phy_base_regs + 0x9B4);
		writel(0x25, phy_base_regs + 0x9AC);
		writel(0x7C, phy_base_regs + 0x97C);
		writel(0x80, phy_base_regs + 0x9CC);
	}

	phy_base_regs = exynos_pcie->phy_base;
	for (i = 0; i < 20; i++) {
		mdelay(1);
		temp = (readl(phy_base_regs + 0x760) & 0x4) >> 2;
		if (temp == 1)
			break;
	}
	if (temp == 0)
		printk("PLL_LOCK: fail");

	phy_base_regs = exynos_pcie->phy_base;
	for (i = 0; i < lane; i++) {
		phy_base_regs += (i * 0x800);
		writel(0xF6, phy_base_regs + 0x9B4);
		writel(0x24, phy_base_regs + 0x9AC);
		writel(0x5C, phy_base_regs + 0x97C);
		writel(0x00, phy_base_regs + 0x9CC);
	}

	/* pma_rst */
	writel(0x1, elbi_base_regs + 0x1400);
	writel(0x0, elbi_base_regs + 0x1400);
	udelay(1);
	writel(0x1, elbi_base_regs + 0x1400);

	/* soft_pwr_rst */
	writel(0xF, elbi_base_regs + 0x3A4);
	writel(0xD, elbi_base_regs + 0x3A4);
	udelay(1);
	writel(0xF, elbi_base_regs + 0x3A4);

	/* PCS setting */
	//if (PCIe_TYPE == RC) {
	writel(0x700D5, phy_pcs_base_regs + 0x154);
	if (exynos_pcie->chip_ver == 0)
	{
		writel(0x25, phy_pcs_base_regs + 0x008); //ungating Link CLK with PIPE for REFCLK gating at L1.2 -> after linkup, [2] reset
	}
	else if (exynos_pcie->chip_ver == 1)
	{
		val = readl(phy_pcs_base_regs + 0x008);
		val = val & 0xffffffdf;
		val = val | 0x00000010;
		writel(val, phy_pcs_base_regs + 0x008);
	}
	//}
	writel(0xC, phy_pcs_base_regs + 0x180); //force_en_pipe_pclk
	writel(0x300FF, phy_pcs_base_regs + 0x150); //add for L2 entry and power
	udelay(1);
}

int exynos_pcie_rc_eom(struct device *dev, void *phy_base_regs)
{
	struct exynos_pcie *exynos_pcie = dev_get_drvdata(dev);
	struct exynos_pcie_ops *pcie_ops
		= &exynos_pcie->exynos_pcie_ops;
	struct dw_pcie *pci = exynos_pcie->pci;
	struct pcie_port *pp = &pci->pp;
	struct device_node *np = dev->of_node;
	unsigned int val;
	unsigned int speed_rate, num_of_smpl;
	unsigned int lane_width = 1;
	int i, ret;
	int test_cnt = 0;
	struct pcie_eom_result **eom_result;

	u32 phase_sweep = 0;
	u32 phase_step = 1;
	u32 phase_loop;
	u32 vref_sweep = 0;
	u32 vref_step = 1;
	u32 err_cnt = 0;
	u32 cdr_value = 0;
	u32 eom_done = 0;
	u32 err_cnt_13_8;
	u32 err_cnt_7_0;

	dev_info(dev, "[%s] START! \n", __func__);

	ret = of_property_read_u32(np, "num-lanes", &lane_width);
	if (ret) {
		dev_err(dev, "[%s] failed to get num of lane, lane width = 0\n", __func__);
		lane_width = 0;
	} else
		dev_info(dev, "[%s] num-lanes : %d\n", __func__, lane_width);

	/* eom_result[lane_num][test_cnt] */
	eom_result = kzalloc(sizeof(struct pcie_eom_result*) * lane_width, GFP_KERNEL);
	for (i = 0; i < lane_width; i ++) {
		eom_result[i] = kzalloc(sizeof(struct pcie_eom_result) *
				EOM_PH_SEL_MAX * EOM_DEF_VREF_MAX, GFP_KERNEL);
	}
	if (eom_result == NULL) {
		return -ENOMEM;
	}
	exynos_pcie->eom_result = eom_result;

	pcie_ops->rd_own_conf(pp, PCIE_LINK_CTRL_STAT, 4, &val);
	speed_rate = (val >> 16) & 0xf;

	num_of_smpl = 13;

	for (i = 0; i < lane_width; i++)
	{
		if (speed_rate == 3 || speed_rate == 4) {
			/* rx_efom_settle_time [7:4] = 0xE, rx_efom_bit_width_sel[3:2] = 0 */
			writel(0xE7, phy_base_regs + RX_EFOM_BIT_WIDTH_SEL);
		} else {
			writel(0xE3, phy_base_regs + RX_EFOM_BIT_WIDTH_SEL);
		}
		writel(0xF, phy_base_regs + ANA_RX_DFE_EOM_PI_STR_CTRL);
		writel(0x14, phy_base_regs + ANA_RX_DFE_EOM_PI_DIVSEL_G12);
		writel(0x24, phy_base_regs + ANA_RX_DFE_EOM_PI_DIVSEL_G32);

		val = readl(phy_base_regs + RX_CDR_LOCK) >> 2;
		cdr_value = val & 0x1;
		eom_done = readl(phy_base_regs + RX_EFOM_DONE) & 0x1;
		dev_info(dev, "eom_done 0x%x , cdr_value : 0x%x \n", eom_done, cdr_value);

		writel(0x0, phy_base_regs + RX_EFOM_NUMOF_SMPL_13_8);
		writel(num_of_smpl, phy_base_regs + RX_EFOM_NUMOF_SMPL_7_0);

		if (speed_rate == 1)
			phase_loop = 2;
		else
			phase_loop = 1;

		for (phase_sweep = 0; phase_sweep <= 0x47 * phase_loop; phase_sweep = phase_sweep + phase_step)
		{
			val = (phase_sweep % 72) << 1;
			writel(val, phy_base_regs + RX_EFOM_EOM_PH_SEL);

			for (vref_sweep = 0; vref_sweep <= 255; vref_sweep = vref_sweep + vref_step)
			{
				writel(0x12, phy_base_regs + RX_EFOM_MODE);
				writel(vref_sweep, phy_base_regs + RX_EFOM_DFE_VREF_CTRL);
				writel(0x13, phy_base_regs + RX_EFOM_MODE);

				val = readl(phy_base_regs + RX_EFOM_DONE) & 0x1;
				while (val != 0x1) {
					udelay(1);
					val = readl(phy_base_regs + RX_EFOM_DONE) & 0x1;
				}

				err_cnt_13_8 = readl(phy_base_regs + MON_RX_EFOM_ERR_CNT_13_8) << 8;
				err_cnt_7_0 = readl(phy_base_regs + MON_RX_EFOM_ERR_CNT_7_0);
				err_cnt = err_cnt_13_8 + err_cnt_7_0;

				//if (vref_sweep == 128)
				printk("%d,%d : %d %d %d\n", i, test_cnt, phase_sweep, vref_sweep, err_cnt);

				//save result
				eom_result[i][test_cnt].phase = phase_sweep;
				eom_result[i][test_cnt].vref = vref_sweep;
				eom_result[i][test_cnt].err_cnt = err_cnt;
				test_cnt++;

			}
		}
		/* goto next lane */
		phy_base_regs += 0x800;
		test_cnt = 0;
	}

	return 0;
}

void exynos_pcie_rc_phy_init(struct pcie_port *pp)
{
	struct dw_pcie *pci = to_dw_pcie_from_pp(pp);
	struct exynos_pcie *exynos_pcie = to_exynos_pcie(pci);

	dev_info(pci->dev, "Initialize PHY functions.\n");

	exynos_pcie->phy_ops.phy_check_rx_elecidle =
		exynos_pcie_rc_phy_check_rx_elecidle;
	exynos_pcie->phy_ops.phy_all_pwrdn = exynos_pcie_rc_phy_all_pwrdn;
	exynos_pcie->phy_ops.phy_all_pwrdn_clear = exynos_pcie_rc_phy_all_pwrdn_clear;
	exynos_pcie->phy_ops.phy_config = exynos_pcie_rc_pcie_phy_config;
	exynos_pcie->phy_ops.phy_eom = exynos_pcie_rc_eom;
	exynos_pcie->phy_ops.phy_input_clk_change = exynos_pcie_rc_phy_input_clk_change;
}
EXPORT_SYMBOL(exynos_pcie_rc_phy_init);

static void exynos_pcie_quirks(struct pci_dev *dev)
{
#if IS_ENABLED(CONFIG_EXYNOS_PCI_PM_ASYNC)
	device_enable_async_suspend(&dev->dev);
	pr_info("[%s:pcie_1] enable async_suspend\n", __func__);
#else
	device_disable_async_suspend(&dev->dev);
	pr_info("[%s] async suspend disabled\n", __func__);
#endif
}
DECLARE_PCI_FIXUP_FINAL(PCI_ANY_ID, PCI_ANY_ID, exynos_pcie_quirks);

MODULE_AUTHOR("Kisang Lee <kisang80.lee@samsung.com>");
MODULE_AUTHOR("Kwangho Kim <kwangho2.kim@samsung.com>");
MODULE_AUTHOR("Seungo Jang <seungo.jang@samsung.com>");
MODULE_DESCRIPTION("Samsung PCIe host controller driver");
MODULE_LICENSE("GPL v2");
