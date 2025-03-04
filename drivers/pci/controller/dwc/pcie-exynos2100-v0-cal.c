/*
 * PCIe phy driver for Samsung EXYNOS2100
 *
 * Copyright (C) 2013 Samsung Electronics Co., Ltd.
 *		http://www.samsung.com
 *
 * Author: Kyoungil Kim <ki0351.kim@samsung.com>
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
#include "pcie-designware.h"
#include "pcie-exynos-common.h"
#include "pcie-exynos-host-v0.h"

#if IS_ENABLED(CONFIG_EXYNOS_OTP)
#include <linux/exynos_otp.h>
#endif
extern struct exynos_pcie g_pcie[MAX_RC_NUM];
//extern struct exynos_pcie g_pcie_host_v1[MAX_RC_NUM];

/* avoid checking rx elecidle when access DBI */
void exynos_phy_check_rx_elecidle(void *phy_pcs_base_regs, int val, int ch_num)
{
	u32 reg_val;

	reg_val = readl(phy_pcs_base_regs + 0xEC);
	reg_val &= ~(0x1 << 3);
	reg_val |= (val << 3);
	writel(reg_val, phy_pcs_base_regs + 0xEC);
}

void exynos_phy_all_pwrdn(struct exynos_pcie *exynos_pcie, int ch_num)
{
	void __iomem *phy_base_regs = exynos_pcie->phy_base;
	void __iomem *phy_pcs_base_regs = exynos_pcie->phy_pcs_base;
	struct dw_pcie *pci = exynos_pcie->pci;

	/* if you want to use channel 1, you need to set below register */
	u32 __maybe_unused val;

	if (ch_num == 1)
		return ;

	/* Trsv */
	writel(0xFE, phy_base_regs + (0x57 * 4));
	dev_dbg(pci->dev, "[%s] phy_base + 0x15C = 0x%x\n", __func__,
			readl(phy_base_regs + (0x57 * 4)));

	/* Common */
	writel(0xC1, phy_base_regs + (0x20 * 4));
	dev_dbg(pci->dev, "[%s] phy_base + 0x80 = 0x%x\n", __func__,
			readl(phy_base_regs + (0x20 * 4)));

	/* Set PCS values */
	val = readl(phy_pcs_base_regs + 0x100);
	val |= (0x1 << 6);
	writel(val, phy_pcs_base_regs + 0x100);

	val = readl(phy_pcs_base_regs + 0x104);
	val |= (0x1 << 7);
	writel(val, phy_pcs_base_regs + 0x104);
}

/* PHY all power down clear */
void exynos_phy_all_pwrdn_clear(struct exynos_pcie *exynos_pcie, int ch_num)
{
	void __iomem *phy_base_regs = exynos_pcie->phy_base;
	void __iomem *phy_pcs_base_regs = exynos_pcie->phy_pcs_base;
	void __iomem *sysreg_base_regs = exynos_pcie->sysreg_base;
	struct dw_pcie *pci = exynos_pcie->pci;

	/* if you want to use channel 1, you need to set below register */
	u32 __maybe_unused val;

	if (ch_num == 1)
		return ;

	/* enable Lane0 */
	writel(readl(sysreg_base_regs + 0xC) | (0x1 << 12),
			sysreg_base_regs + 0xC);
	dev_dbg(pci->dev, "[%s] sysreg + 0x1050 = 0x%x\n", __func__,
			readl(sysreg_base_regs + 0xC));

	/* Set PCS values */
	val = readl(phy_pcs_base_regs + 0x100);
	val &= ~(0x1 << 6);
	writel(val, phy_pcs_base_regs + 0x100);

	val = readl(phy_pcs_base_regs + 0x104);
	val &= ~(0x1 << 7);
	writel(val, phy_pcs_base_regs + 0x104);

	/* Common */
	writel(0xC0, phy_base_regs + (0x20 * 4));

	/* Trsv */
	writel(0x7E, phy_base_regs + (0x57 * 4));
}

void exynos_pcie_phy_otp_config(void *phy_base_regs, int ch_num)
{
#if IS_ENABLED(CONFIG_EXYNOS_OTP)
	u8 utype;
	u8 uindex_count;
	struct tune_bits *data;
	u32 index;
	u32 value;
	u16 magic_code;
	u32 i;

	struct pcie_port *pp = &g_pcie[ch_num].pp;

	if (ch_num == 0)
		magic_code = 0x5030;
	else if (ch_num == 1)
		magic_code = 0x5031;
	else
		return ;

	if (!otp_tune_bits_parsed(magic_code, &utype, &uindex_count, &data)) {
		dev_err(pp->dev, "%s: [OTP] uindex_count %d", __func__,
				uindex_count);
		for (i = 0; i < uindex_count; i++) {
			index = data[i].index;
			value = data[i].value;

			dev_err(pp->dev, "%s: [OTP][Return Value] index = "
					"0x%x, value = 0x%x\n",
					__func__, index, value);
			dev_err(pp->dev, "%s: [OTP][Before Reg Value] offset "
					"0x%x = 0x%x\n", __func__, index * 4,
					readl(phy_base_regs + (index * 4)));
			if (readl(phy_base_regs + (index * 4)) != value)
				writel(value, phy_base_regs + (index * 4));
			else
				return;

			dev_err(pp->dev, "%s: [OTP][After Reg Value] offset "
					"0x%x = 0x%x\n", __func__, index * 4,
					readl(phy_base_regs + (index * 4)));
		}
	}
#else
	return ;
#endif
}

void exynos_pcie_phy_config(struct exynos_pcie *exynos_pcie, int ch_num)
{
	void __iomem *elbi_base_regs = exynos_pcie->elbi_base;
	void __iomem *phy_base_regs = exynos_pcie->phy_base;
	void __iomem *phy_pcs_base_regs = exynos_pcie->phy_pcs_base;
	void __iomem *sysreg_base_regs = exynos_pcie->sysreg_base;

	int i;

	/* 26MHz gen2 */
	u32 cmn_config_val[48] = {
		0x01, 0xE1, 0x05, 0x00, 0x88, 0x88, 0x88, 0x0C,
		0x91, 0x45, 0x65, 0x24, 0x33, 0x18, 0xE3, 0xFC,
		0xD8, 0x05, 0xE6, 0x80, 0x00, 0x00, 0x00, 0x00,
		0x60, 0x11, 0x01, 0xA0, 0x10, 0x04, 0x27, 0x88,
		0xC0, 0xFF, 0x9B, 0x52, 0x22, 0x30, 0x4F, 0xDC,
		0x40, 0x00, 0x00, 0x00, 0x00, 0x0B, 0x00, 0x80,
	}; /* Ch0 HS1 Gen2*/
	u32 trsv_config_val[80] = {
		0x31, 0x40, 0x37, 0x99, 0x85, 0x00, 0xC0, 0xFF,
		0xFF, 0x3F, 0x8C, 0xC8, 0x02, 0x01, 0x88, 0x80,
		0x06, 0x90, 0x6C, 0x66, 0x09, 0x60, 0x42, 0x48,
		0xC8, 0x50, 0x0E, 0x33, 0x58, 0xE7, 0x20, 0x22,
		0x80, 0x38, 0x05, 0x85, 0x00, 0x00, 0x00, 0x7E,
		0x00, 0x00, 0x55, 0x15, 0xAC, 0xAA, 0x3E, 0x00,
		0x00, 0x00, 0x20, 0x3F, 0x00, 0x03, 0x40, 0x00,
		0x10, 0x07, 0x01, 0x00, 0x00, 0x00, 0x00, 0x00,
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x40,
		0x05, 0x85, 0xFF, 0x00, 0x00, 0x00, 0x00, 0x00,
	}; /* Ch0 Gen2 */
	/* Set PCS_HIGH_SPEED */
	writel(readl(sysreg_base_regs) | (0x1 << 1), sysreg_base_regs);
	writel((((readl(sysreg_base_regs + 0xC)
						& ~(0xf << 4)) & ~(0xf << 2))
				| (0x3 << 2)) & ~(0x1 << 1),
			sysreg_base_regs + 0xC);
	/* Lane0_enable */
	writel(readl(sysreg_base_regs + 0xC) | (0x1 << 12),
			sysreg_base_regs + 0xC);

	/* PHY reset */
	writel(0x1, elbi_base_regs + 0x288);
	writel(0x1, elbi_base_regs + 0x290);
	writel(0x1, elbi_base_regs + 0x294);
	writel(0x1, elbi_base_regs + 0x28C);
	udelay(10);
	writel(0x0, elbi_base_regs + 0x288);
	writel(0x0, elbi_base_regs + 0x290);
	writel(0x0, elbi_base_regs + 0x294);
	writel(0x0, elbi_base_regs + 0x28C);
	udelay(10);
	writel(0x1, elbi_base_regs + 0x288);

	/* PHY Common block Setting */
	{
		/* PHY Common block Setting */
		for (i = 0; i < sizeof(cmn_config_val) / 4; i++)
			writel(cmn_config_val[i], phy_base_regs + (i * 4));

		/* PHY Tranceiver/Receiver block Setting */
		for (i = 0; i < sizeof(trsv_config_val) / 4; i++)
			writel(trsv_config_val[i],
					phy_base_regs + ((0x30 + i) * 4));
	}

#if IS_ENABLED(CONFIG_EXYNOS_OTP)
	/* PHY OTP Tuning bit configuration Setting */
	exynos_pcie_phy_otp_config(phy_base_regs, ch_num);
#endif

	/* tx amplitude control */
	/* writel(0x14, phy_base_regs + (0x5C * 4)); */

	{
		u32 reg_val;
		/* tx latency */
		writel(0x70, phy_pcs_base_regs + 0xF8);
		/* pcs refclk out control */
		writel(0x87, phy_pcs_base_regs + 0x100);
		writel(0x50, phy_pcs_base_regs + 0x104);
		/* PRGM_TIMEOUT_L1SS_VAL Setting */
		reg_val = readl(phy_pcs_base_regs + 0xC);
		reg_val &= ~(0x1 << 1);
		reg_val |= (0x1 << 4);
		writel(reg_val, phy_pcs_base_regs + 0xC);
	}

	/* PCIE_MAC RST */
	mdelay(1);
	writel(0x1, elbi_base_regs + 0x290);
	writel(0x1, elbi_base_regs + 0x294);
	writel(0x1, elbi_base_regs + 0x28C);

	mdelay(5);
}

void exynos_pcie_phy_init(struct pcie_port *pp)
{
	/*1234
	  struct exynos_pcie *exynos_pcie = to_exynos_pcie(pp);
	 */

	struct dw_pcie *pci = to_dw_pcie_from_pp(pp);
	struct exynos_pcie *exynos_pcie = to_exynos_pcie(pci);

	dev_info(pci->dev, "Initialize PHY functions.\n");

	exynos_pcie->phy_ops.phy_check_rx_elecidle =
		exynos_phy_check_rx_elecidle;
	exynos_pcie->phy_ops.phy_all_pwrdn = exynos_phy_all_pwrdn;
	exynos_pcie->phy_ops.phy_all_pwrdn_clear = exynos_phy_all_pwrdn_clear;
	exynos_pcie->phy_ops.phy_config = exynos_pcie_phy_config;
}
EXPORT_SYMBOL(exynos_pcie_phy_init);

static void exynos_pcie_quirks(struct pci_dev *dev)
{
#if IS_ENABLED(CONFIG_EXYNOS_PCI_PM_ASYNC)
	device_enable_async_suspend(&dev->dev);
	pr_info("[%s:pcie_0] enable async_suspend\n", __func__);
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
