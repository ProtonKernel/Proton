/* Copyright (c) 2012-2013, The Linux Foundation. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 and
 * only version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */

#ifndef __PCIE_EXYNOS_HOST_V0_H
#define __PCIE_EXYNOS_HOST_V0_H

/* PCIe ELBI registers */
#define PCIE_IRQ_PULSE			0x000
#define IRQ_INTA_ASSERT			(0x1 << 0)
#define IRQ_INTB_ASSERT			(0x1 << 2)
#define IRQ_INTC_ASSERT			(0x1 << 4)
#define IRQ_INTD_ASSERT			(0x1 << 6)
#define IRQ_MSI_RISING_ASSERT	(0x1 << 29)
#define IRQ_MSI_FALLING_ASSERT	(0x1 << 28)
#define IRQ_RADM_PM_TO_ACK		(0x1 << 18)
#define IRQ_L1_EXIT			(0x1 << 24)
#define PCIE_IRQ_LEVEL			0x004
#define IRQ_MSI_CTRL			(0x1 << 1)
#define PCIE_IRQ_SPECIAL		0x008
#define PCIE_IRQ_EN_PULSE		0x00c
#define IRQ_MSI_CTRL_EN_RISING_EDG		(0x1 << 29)
#define IRQ_MSI_CTRL_EN_FALLING_EDG		(0x1 << 28)
#define PCIE_IRQ_EN_LEVEL		0x010
#define IRQ_MSI_ENABLE			(0x1 << 1)
#define IRQ_LINK_DOWN			(0x1 << 30)
#define IRQ_LINKDOWN_ENABLE		(0x1 << 30)
#define PCIE_IRQ_EN_SPECIAL		0x014
#define PCIE_SW_WAKE			0x018
#define PCIE_IRQ_LEVEL_FOR_READ		0x020
#define L1_2_IDLE_STATE			(0x1 << 23)
#define IRQ_MSI_EN_CHECK(x)		(((x) >> 6) & 0x1)
#define PCIE_APP_LTSSM_ENABLE		0x02c
#define PCIE_IRQ_TOE_EN_PULSE		0x030
#define PCIE_IRQ_TOE_EN_PULSE2		0x034
#define PCIE_IRQ_TOE_EN_LEVEL		0x038
#define PCIE_L1_BUG_FIX_ENABLE		0x038
#define PCIE_APP_REQ_EXIT_L1		0x040
#define PCIE_CXPL_DEBUG_INFO_H		0x070
#define PCIE_ELBI_RDLH_LINKUP		0x074
#define PCIE_ELBI_LTSSM_DISABLE		0x0
#define PCIE_ELBI_LTSSM_ENABLE		0x1
#define PCIE_PM_DSTATE			0x88
#define PCIE_D0_UNINIT_STATE		0x4
#define PCIE_APP_REQ_EXIT_L1_MODE	0xF4
#define L1_REQ_NAK_CONTROL		(0x3 << 4)
#define L1_REQ_NAK_CONTROL_MASTER	(0x1 << 4)
#define PCIE_HISTORY_REG(x)		(0x138 + ((x) * 0x4))
#define LTSSM_STATE(x)			(((x) >> 16) & 0x3f)
#define PM_DSTATE(x)			(((x) >> 8) & 0x7)
#define L1SUB_STATE(x)			(((x) >> 0) & 0x7)
#define PCIE_LINKDOWN_RST_CTRL_SEL	0x1B8
#define PCIE_LINKDOWN_RST_MANUAL	(0x1 << 1)
#define PCIE_LINKDOWN_RST_FSM		(0x1 << 0)
#define PCIE_SOFT_AUXCLK_SEL_CTRL	0x1C4
#define CORE_CLK_GATING			(0x1 << 0)
#define PCIE_SOFT_CORE_RESET		0x1D0
#define PCIE_STATE_HISTORY_CHECK	0x274
#define PCIE_STATE_POWER_S		0x2BC
#define PCIE_STATE_POWER_M		0x2C0
#define HISTORY_BUFFER_ENABLE		0x3
#define HISTORY_BUFFER_CLEAR		(0x1 << 1)
#define PCIE_QCH_SEL			0x2C8
#define CLOCK_GATING_IN_L12		0x1
#define CLOCK_NOT_GATING		0x3
#define CLOCK_GATING_MASK		0x3
#define CLOCK_GATING_PMU_L1		(0x1 << 11)
#define CLOCK_GATING_PMU_L23READY	(0x1 << 10)
#define CLOCK_GATING_PMU_DETECT_QUIET	(0x1 << 9)
#define CLOCK_GATING_PMU_L12		(0x1 << 8)
#define CLOCK_GATING_PMU_ALL		(0xF << 8)
#define CLOCK_GATING_PMU_MASK		(0xF << 8)
#define CLOCK_GATING_APB_L1		(0x1 << 7)
#define CLOCK_GATING_APB_L23READY	(0x1 << 6)
#define CLOCK_GATING_APB_DETECT_QUIET	(0x1 << 5)
#define CLOCK_GATING_APB_L12		(0X1 << 4)
#define CLOCK_GATING_APB_ALL		(0xF << 4)
#define CLOCK_GATING_APB_MASK		(0xF << 4)
#define CLOCK_GATING_AXI_L1		(0x1 << 3)
#define CLOCK_GATING_AXI_L23READY	(0x1 << 2)
#define CLOCK_GATING_AXI_DETECT_QUIET	(0x1 << 1)
#define CLOCK_GATING_AXI_L12		(0x1 << 0)
#define CLOCK_GATING_AXI_ALL		(0xF << 0)
#define CLOCK_GATING_AXI_MASK		(0xF << 0)
#define PCIE_DMA_MONITOR1		0x2CC
#define PCIE_DMA_MONITOR2		0x2D0
#define PCIE_DMA_MONITOR3		0x2D4
#define FSYS1_MON_SEL_MASK		0xf
#define PCIE_MON_SEL_MASK		0xff
#define PCIE_IRQ_CP_EN_PULSE		0x2DC
#define PCIE_IRQ_CP_EN_PULSE2		0x2E0
#define PCIE_IRQ_CP_LEVEL		0x2E4
#define PCIE_MSI_STATUS_IRQ_AP_ENABLE	0x2E8
#define PCIE_MSI_STATUS_IRQ_CP_ENABLE	0x2EC
#define VENDOR_MSG_REQ			0xA8
#define VENDOR_MSG_FORMAT		0xAC
#define VENDOR_MSG_TYPE			0xB0
#define VENDOR_MSG_CODE			0xD0
#define PME_TURNOFF_CODE		0x19
#define TLP_TYPE_MSG			0x13
#define XMIT_PME_TURNOFF		0x4C

/* PCIe PMU registers */
#define IDLE_IP3_STATE			0x3EC
#define IDLE_IP_RC1_SHIFT		(31)
#define IDLE_IP_RC0_SHIFT		(30)
#define IDLE_IP3_MASK			0x3FC
#define WAKEUP_MASK			0x3944
#define WAKEUP_MASK_PCIE_WIFI		16
#define PCIE_PHY_CONTROL		0x0718
#define PCIE_PHY_CONTROL_MASK		0x1

/* PCIe DBI registers */
#define PM_CAP_OFFSET			0x40
#define PCIE_CAP_OFFSET			0x70
#define PCIE_LINK_CTRL_STAT		0x80
#define PCI_EXP_LNKCAP_MLW_X1		(0x1 << 4)
#define PCI_EXP_LNKCAP_L1EL_64USEC	(0x7 << 15)
/* previous definition is in 'include/uapi/linux/pci_regs.h:661' */
//#define PCI_EXP_LNKCTL2_TLS		0xf
#define PCI_EXP_LNKCTL2_TLS_2_5GB	0x1
#define PCI_EXP_LNKCTL2_TLS_5_0GB	0x2
#define PCI_EXP_LNKCTL2_TLS_8_0GB	0x3
#define PCIE_LINK_L1SS_CAP		0x154
#define PCIE_LINK_L1SS_CONTROL		0x158
#define PORT_LINK_TCOMMON_MASK		(0xFF << 8)
#define PORT_LINK_TCOMMON_32US		(0x20 << 8)
#define PCIE_LINK_L1SS_CONTROL2		0x15C
#define PORT_LINK_L1SS_ENABLE		(0xf << 0)
#define PORT_LINK_L11_ENABLE		(0xa << 0)
#define PORT_LINK_L12_ENABLE		(0x5 << 0)
#define PORT_LINK_L12_LTR_THRESHOLD	(0x40a0 << 16)
#define PORT_LINK_L12_LTR_THRESHOLD_0US (0x4000 << 16)
#define PORT_LINK_TPOWERON_130US	(0x69 << 0)
#define PORT_LINK_TPOWERON_150US	(0x79 << 0)
#define PORT_LINK_TPOWERON_3100US	(0xfa << 0)
#define PORT_LINK_L1SS_T_PCLKACK	(0x3 << 6)
#define PORT_LINK_L1SS_T_L1_2		(0x4 << 2)
#define PORT_LINK_L1SS_T_POWER_OFF	(0x2 << 0)
#define PCIE_ACK_F_ASPM_CONTROL		0x70C
#define PCIE_PORT_LINK_CONTROL		0x710

#define PCIE_MISC_CONTROL		0x8BC
#define DBI_RO_WR_EN			0x1

#define PCIE_AUX_CLK_FREQ_OFF		0xB40
#define PCIE_AUX_CLK_FREQ_24MHZ		0x18
#define PCIE_AUX_CLK_FREQ_26MHZ		0x1A
#define PCIE_L1_SUBSTATES_OFF		0xB44
#define PCIE_L1_SUB_VAL			0xEA

#define PCIE_LINK_WIDTH_SPEED_CONTROL	0x80C
#define EXYNOS_PORT_LOGIC_SPEED_CHANGE	(0x1 << 17)

#define PCIE_ATU_VIEWPORT		0x900
#define EXYNOS_PCIE_ATU_REGION_INBOUND	(0x1 << 31)
#define EXYNOS_PCIE_ATU_REGION_OUTBOUND	(0x0 << 31)
#define EXYNOS_PCIE_ATU_REGION_INDEX2	(0x2 << 0)
#define EXYNOS_PCIE_ATU_REGION_INDEX1	(0x1 << 0)
#define EXYNOS_PCIE_ATU_REGION_INDEX0	(0x0 << 0)
#define PCIE_ATU_CR1			0x904
#define EXYNOS_PCIE_ATU_TYPE_MEM	(0x0 << 0)
#define EXYNOS_PCIE_ATU_TYPE_IO		(0x2 << 0)
#define EXYNOS_PCIE_ATU_TYPE_CFG0	(0x4 << 0)
#define EXYNOS_PCIE_ATU_TYPE_CFG1	(0x5 << 0)
#define PCIE_ATU_CR2			0x908
#define EXYNOS_PCIE_ATU_ENABLE		(0x1 << 31)
#define EXYNOS_PCIE_ATU_BAR_MODE_ENABLE	(0x1 << 30)
#define PCIE_ATU_LOWER_BASE		0x90C
#define PCIE_ATU_UPPER_BASE		0x910
#define PCIE_ATU_LIMIT			0x914
#define PCIE_ATU_LOWER_TARGET		0x918
#define EXYNOS_PCIE_ATU_BUS(x)		(((x) & 0xff) << 24)
#define EXYNOS_PCIE_ATU_DEV(x)		(((x) & 0x1f) << 19)
#define EXYNOS_PCIE_ATU_FUNC(x)		(((x) & 0x7) << 16)
#define PCIE_ATU_UPPER_TARGET		0x91C

#define PCIE_MSI_ADDR_LO		0x820
#define PCIE_MSI_ADDR_HI		0x824
#define PCIE_MSI_INTR0_ENABLE		0x828
#define PCIE_MSI_INTR0_MASK		0x82C
#define PCIE_MSI_INTR0_STATUS		0x830

/* PCIe SYSREG registers */
#define PCIE_WIFI0_PCIE_PHY_CONTROL	0xC
#define BIFURCATION_MODE_DISABLE	(0x1 << 16)
#define LINK1_ENABLE			(0x1 << 15)
#define LINK0_ENABLE			(0x1 << 14)
#define PCS_LANE1_ENABLE		(0x1 << 13)
#define PCS_LANE0_ENABLE		(0x1 << 12)

#define PCIE_SYSREG_SHARABILITY_CTRL	0x700
#define PCIE_SYSREG_SHARABLE_OFFSET	8
#define PCIE_SYSREG_SHARABLE_ENABLE	0x3
#define PCIE_SYSREG_SHARABLE_DISABLE	0x0

/* Definitions for WIFI L1.2 */
#define WIFI_L1SS_CAPID			0x240
#define WIFI_L1SS_CAP			0x244
#define WIFI_L1SS_CONTROL		0x248
#define WIFI_L1SS_CONTROL2		0x24C
#define WIFI_L1SS_LINKCTRL		0xBC
#define WIFI_LINK_STATUS		0xBE
#define WIFI_PM_MNG_STATUS_CON		0x4C

/* LINK Control Register */
#define WIFI_ASPM_CONTROL_MASK		(0x3 << 0)
#define WIFI_ASPM_L1_ENTRY_EN		(0x2 << 0)
#define WIFI_USE_SAME_REF_CLK		(0x1 << 6)
#define WIFI_CLK_REQ_EN			(0x1 << 8)

/* L1SSS Control Register */
#define WIFI_ALL_PM_ENABEL		(0xf << 0)
#define WIFI_PCIPM_L12_EN		(0x1 << 0)
#define WIFI_PCIPM_L11_EN		(0x1 << 1)
#define WIFI_ASPM_L12_EN		(0x1 << 2)
#define WIFI_ASPM_L11_EN		(0x1 << 3)
#define WIFI_COMMON_RESTORE_TIME	(0xa << 8) /* Default Value */
#define WIFI_COMMON_RESTORE_TIME_70US	(0x46 << 8)

/* ETC definitions */
#define IGNORE_ELECIDLE			1
#define ENABLE_ELECIDLE			0
#define	PCIE_DISABLE_CLOCK		0
#define	PCIE_ENABLE_CLOCK		1
#define PCIE_IS_IDLE			1
#define PCIE_IS_ACTIVE			0

/* PCIe PHY definitions */
#define PHY_PLL_STATE			0xBC
#define CHK_PHY_PLL_LOCK		0x3

/* For Set NCLK OFF to avoid system hang */
#define EXYNOS_PCIE_MAX_NAME_LEN        10
#define PCIE_L12ERR_CTRL                0x2F0
#define PCIE_GEN3_L12ERR_CTRL		0x3B0
#define NCLK_OFF_OFFSET                 0x2
#define GEN3A_L12ERR_CRTRL_SFR		0x131203B0
#define GEN3B_L12ERR_CRTRL_SFR          0x131303B0

void exynos_pcie_phy_init(struct pcie_port *pp);
#if IS_ENABLED(CONFIG_EXYNOS_PCIE_GEN2_IOMMU) || IS_ENABLED(CONFIG_EXYNOS_PCIE_GEN2_S2MPU)
extern void *dma_direct_alloc(struct device *dev, size_t size,
		dma_addr_t *dma_handle, gfp_t gfp, unsigned long attrs);
extern void dma_direct_free(struct device *dev, size_t size,
		void *cpu_addr, dma_addr_t dma_addr, unsigned long attrs);
extern int dma_common_mmap(struct device *dev, struct vm_area_struct *vma,
		void *cpu_addr, dma_addr_t dma_addr, size_t size,
		unsigned long attrs);
extern int dma_common_get_sgtable(struct device *dev, struct sg_table *sgt,
		 void *cpu_addr, dma_addr_t dma_addr, size_t size,
		 unsigned long attrs);
extern dma_addr_t dma_direct_map_page(struct device *dev, struct page *page,
		unsigned long offset, size_t size, enum dma_data_direction dir,
		unsigned long attrs);
extern void dma_direct_unmap_page(struct device *dev, dma_addr_t addr,
		size_t size, enum dma_data_direction dir, unsigned long attrs);
extern int dma_direct_map_sg(struct device *dev, struct scatterlist *sgl, int nents,
		enum dma_data_direction dir, unsigned long attrs);
extern void dma_direct_unmap_sg(struct device *dev, struct scatterlist *sgl,
		int nents, enum dma_data_direction dir, unsigned long attrs);
extern dma_addr_t dma_direct_map_resource(struct device *dev, phys_addr_t paddr,
		size_t size, enum dma_data_direction dir, unsigned long attrs);
extern void dma_direct_sync_single_for_cpu(struct device *dev,
		dma_addr_t addr, size_t size, enum dma_data_direction dir);
extern void dma_direct_sync_single_for_device(struct device *dev,
		dma_addr_t addr, size_t size, enum dma_data_direction dir);
extern void dma_direct_sync_sg_for_cpu(struct device *dev,
		struct scatterlist *sgl, int nents, enum dma_data_direction dir);
extern void dma_direct_sync_sg_for_device(struct device *dev,
		struct scatterlist *sgl, int nents, enum dma_data_direction dir);
extern u64 dma_direct_get_required_mask(struct device *dev);
#endif

#if IS_ENABLED(CONFIG_EXYNOS_PCIE_IOMMU)
extern void pcie_sysmmu_enable(int ch_num);
extern void pcie_sysmmu_disable(int ch_num);
extern int pcie_iommu_map(int ch_num, unsigned long iova, phys_addr_t paddr,
		size_t size, int prot);
extern size_t pcie_iommu_unmap(int ch_num, unsigned long iova, size_t size);
#else
static void __maybe_unused pcie_sysmmu_enable(int ch_num)
{
	pr_err("PCIe SysMMU is NOT Enabled!!!\n");
}
static void __maybe_unused pcie_sysmmu_disable(int ch_num)
{
	pr_err("PCIe SysMMU is NOT Enabled!!!\n");
}
static int __maybe_unused pcie_iommu_map(int ch_num, unsigned long iova, phys_addr_t paddr,
				size_t size, int prot)
{
	pr_err("PCIe SysMMU is NOT Enabled!!!\n");

	return -ENODEV;
}
static size_t __maybe_unused pcie_iommu_unmap(int ch_num, unsigned long iova, size_t size)
{
	pr_err("PCIe SysMMU is NOT Enabled!!!\n");

	return -ENODEV;
}
#endif

#endif
