/*
 * PCIe host controller driver for Samsung EXYNOS SoCs
 *
 * Copyright (C) 2013 Samsung Electronics Co., Ltd.
 *		http://www.samsung.com
 *
 * Author: Kisang Lee <kisang80.lee@samsung.com>
 *         Kwangho Kim <kwangho2.kim@samsung.com>
 *         Seungo Jang <seungo.jang@samsung.com>
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
#include <linux/exynos-pci-noti.h>
#include <linux/pm_qos.h>
#include <linux/exynos-pci-ctrl.h>
#include <dt-bindings/pci/pci.h>
#include <linux/of_reserved_mem.h>
#include <linux/of_fdt.h>
#if IS_ENABLED(CONFIG_CPU_IDLE)
//#include <soc/samsung/exynos-powermode.h>
#include <soc/samsung/exynos_pm_qos.h>
#include <soc/samsung/exynos-pm.h>
#include <soc/samsung/exynos-cpupm.h>
#endif

#include "pcie-designware.h"
#include "pcie-exynos-common.h"
#include "pcie-exynos-host-v0.h"
#include "pcie-exynos-dbg.h"

#include <linux/kthread.h>
#include <linux/random.h>

#if IS_ENABLED(CONFIG_EXYNOS_PCIE_GEN2_S2MPU)
#include <soc/samsung/exynos-s2mpu.h>

static spinlock_t s2mpu_lock;
#endif

/* #define CONFIG_DYNAMIC_PHY_OFF */
/* #define CONFIG_SEC_PANIC_PCIE_ERR */

#if IS_ENABLED(CONFIG_SOC_EXYNOS9810) || IS_ENABLED(CONFIG_SOC_EXYNOS9820)
/*
 * NCLK_OFF_CONTROL is to avoid system hang by speculative access.
 * It is for EXYNOS9810
 */
#define NCLK_OFF_CONTROL
void __iomem *elbi_nclk_reg[MAX_RC_NUM + 1];
#endif

#if IS_ENABLED(CONFIG_SOC_EXYNOS9820)
/*
 * GEN3_PHY_OFF is to avoid power cosumption.
 * GEN3 PHY is enabled on init state.
 * So GEN3 PHY  should be disabled when GEN3 is not used.
 *
 *
 * It is only used for SMDK board.
 */
#undef GEN3_PHY_OFF
bool g_gen3_phy_off;
#endif

struct exynos_pcie g_pcie[MAX_RC_NUM];
#if IS_ENABLED(CONFIG_EXYNOS_PM_QOS)
static struct exynos_pm_qos_request exynos_pcie_int_qos[MAX_RC_NUM];
#endif

#if IS_ENABLED(CONFIG_CPU_IDLE)
static int exynos_pci_power_mode_event(struct notifier_block *nb,
		unsigned long event, void *data);
#endif

static void exynos_pcie_resumed_phydown(struct pcie_port *pp);
static void exynos_pcie_assert_phy_reset(struct pcie_port *pp);
void exynos_pcie_send_pme_turn_off(struct exynos_pcie *exynos_pcie);
void exynos_pcie_poweroff(int ch_num);
int exynos_pcie_poweron(int ch_num);
static int exynos_pcie_wr_own_conf(struct pcie_port *pp, int where, int size,
				u32 val);
static int exynos_pcie_rd_own_conf(struct pcie_port *pp, int where, int size,
				u32 *val);
static int exynos_pcie_rd_other_conf(struct pcie_port *pp,
		struct pci_bus *bus, u32 devfn, int where, int size, u32 *val);
static int exynos_pcie_wr_other_conf(struct pcie_port *pp,
		struct pci_bus *bus, u32 devfn, int where, int size, u32 val);
static void exynos_pcie_prog_viewport_cfg0(struct pcie_port *pp, u32 busdev);
static void exynos_pcie_prog_viewport_mem_outbound(struct pcie_port *pp);

#if IS_ENABLED(CONFIG_EXYNOS_PCIE_GEN2_IOMMU) || IS_ENABLED(CONFIG_EXYNOS_PCIE_GEN2_S2MPU)
#define to_pci_dev_from_dev(dev) container_of((dev), struct pci_dev, dev)

#if IS_ENABLED(CONFIG_EXYNOS_PCIE_GEN2_IOMMU)
extern int pcie_iommu_map(int ch_num, unsigned long iova, phys_addr_t paddr,
		size_t size, int prot);
extern size_t pcie_iommu_unmap(int ch_num, unsigned long iova, size_t size);
dma_addr_t dma_pool_start;
#endif /* CONFIG_EXYNOS_PCIE_GEN2_IOMMU */

#if IS_ENABLED(CONFIG_EXYNOS_PCIE_GEN2_S2MPU)
static char refer_table[1048580] = {0};//0~524299, 524300~8388700

uint64_t exynos_pcie_set_dev_stage2_ap(const char *s2mpu_name, uint64_t base,
		uint64_t size, enum stage2_ap ap)
{
	uint64_t start_idx, num, i;
	uint64_t ret = 0;
	unsigned long flags;

	/* Make sure start address align least 32KB */
	if ((base & (SZ_32K - 1)) != 0) {
		size += base & (SZ_32K - 1);
		base &= ~(SZ_32K - 1);
	}

	/* Make sure size align least 32KB */
	size = (size + SZ_32K - 1) & ~(SZ_32K - 1);

	if (base < 0x100000000 && base >= 0x80000000) {
		start_idx = (base - 0x80000000) / SZ_32K;
		/* maybe, need to add condition (base + size >= 0x100000000) */
	} else if (base < 0x1000000000 && base >= 0x880000000) {
		start_idx = ((base - 0x880000000) / SZ_32K) + 65540;
	} else {
		pr_err("NO DRAM Zone!(WiFi code)\n");
		return 1;
	}

	/* num  */
	num = size / SZ_32K;//num = size / SZ_32K;

	spin_lock_irqsave(&s2mpu_lock, flags);

	/* table set */
	for (i = 0; i < num; i++) {
		(refer_table[start_idx + i])++;

		/*printk("[harx] pcie_set, refer_table[%ld] = %d, dev_addr = 0x%lx\n",
		  start_idx + i, refer_table[start_idx + i], base + (i * 0x1000));*/

		if (refer_table[start_idx + i] == 1) {
			ret = exynos_set_dev_stage2_ap(s2mpu_name,
					base + (i * SZ_32K), SZ_32K,
					ATTR_RW);
			if (ret != 0) {
				pr_err("[pcie_set] set_ap error return: %d\n", ret);
				goto map_finish;
			}
			/* Need to fix "goto map_finish" like "goto retry" for the other loop*/
		}
	}

map_finish:
	spin_unlock_irqrestore(&s2mpu_lock, flags);
	return ret;
}

uint64_t exynos_pcie_unset_dev_stage2_ap(const char *s2mpu_name, uint64_t base,
		uint64_t size, enum stage2_ap ap)
{
	uint64_t start_idx, num, i;
	uint64_t ret = 0;
	unsigned long flags;

	/* Make sure start address align least 32KB */
	if ((base & (SZ_32K - 1)) != 0) {
		size += base & (SZ_32K - 1);
		base &= ~(SZ_32K - 1);
	}

	/* Make sure size align least 32KB */
	size = (size + SZ_32K - 1) & ~(SZ_32K - 1);

	if (base < 0x100000000 && base >= 0x80000000) {
		start_idx = (base - 0x80000000) / SZ_32K;
		/* maybe, need to add condition (base + size >= 0x100000000) */
	} else if (base < 0x1000000000 && base >= 0x880000000) {
		start_idx = ((base - 0x880000000) / SZ_32K) + 65540;
	} else {
		pr_err("NO DRAM Zone!(WiFi code)\n");
		return 1;
	}

	num = size / SZ_32K;//num = size / SZ_32K;

	spin_lock_irqsave(&s2mpu_lock, flags);

	/* reference counter--  */
	for (i = 0; i < num; i++) {
		(refer_table[start_idx + i])--;

		if (refer_table[start_idx + i] < 1) {
			refer_table[start_idx + i] = 0;

			/*printk("[harx] pcie_unset, refer_table[%ld] = %d, dev_addr = 0x%lx\n",
			  start_idx + i, refer_table[start_idx + i], base + (i * 0x1000));*/

			ret = exynos_set_dev_stage2_ap(s2mpu_name,
					base + (i * SZ_32K), SZ_32K,
					ATTR_NO_ACCESS);
			if (ret != 0) {
				pr_err("[pcie_unset] set_ap error return: %d\n", ret);
				goto unmap_finish;
			}
			/* Need to fix "goto map_finish" like "goto retry" for the other loop*/
		}
	}

unmap_finish:
	spin_unlock_irqrestore(&s2mpu_lock, flags);
	return ret;
}
#endif /* CONFIG_EXYNOS_PCIE_GEN2_S2MPU */

static void *exynos_pcie_dma_direct_alloc(struct device *dev, size_t size,
		dma_addr_t *dma_handle, gfp_t flags,
		unsigned long attrs)
{
	void *coherent_ptr;
	struct pci_dev *epdev = to_pci_dev_from_dev(dev);
	int ch_num = 0;
	int ret = 0;

	//dev_info(dev, "[%s] size: 0x%x\n", __func__, (unsigned int)size);
	if (dev == NULL) {
		pr_err("EP device is NULL!!!\n");
		return NULL;
	} else {
		ch_num = pci_domain_nr(epdev->bus);
	}

	coherent_ptr = dma_direct_alloc(dev, size, dma_handle, flags, attrs);

#if IS_ENABLED(CONFIG_EXYNOS_PCIE_GEN2_IOMMU)
	if (coherent_ptr != NULL) {
		ret = pcie_iommu_map(ch_num, *dma_handle, *dma_handle, size,
				DMA_BIDIRECTIONAL);
		if (ret != 0) {
			pr_err("DMA alloc - Can't map PCIe SysMMU table!!!\n");
			dma_direct_free(dev, size, coherent_ptr, *dma_handle, attrs);
			return NULL;
		}
	}
#endif /* CONFIG_EXYNOS_PCIE_GEN2_IOMMU */

#if IS_ENABLED(CONFIG_EXYNOS_PCIE_GEN2_S2MPU)
	if (coherent_ptr != NULL) {
		ret = exynos_pcie_set_dev_stage2_ap("PCIE_GEN2", *dma_handle,
				size, ATTR_RW);
		if (ret) {
			pr_err("[harx] ATTR_RW setting fail\n");
			dma_direct_free(dev, size, coherent_ptr, *dma_handle, attrs);
			return NULL;
		}
	}
#endif /* CONFIG_EXYNOS_PCIE_GEN2_S2MPU */
	return coherent_ptr;

}

static void exynos_pcie_dma_direct_free(struct device *dev, size_t size,
		void *vaddr, dma_addr_t dma_handle,
		unsigned long attrs)
{
#if IS_ENABLED(CONFIG_PCI_DOMAINS_GENERIC)
	struct pci_dev *epdev = to_pci_dev_from_dev(dev);
#endif /* CONFIG_PCI_DOMAINS_GENERIC */
#if IS_ENABLED(CONFIG_PCI_DOMAINS_GENERIC) || IS_ENABLED(CONFIG_EXYNOS_PCIE_GEN2_IOMMU)
	int ch_num = 0;
#endif /* CONFIG_PCI_DOMAINS_GENERIC || CONFIG_EXYNOS_PCIE_GEN2_IOMMU */

#if IS_ENABLED(CONFIG_EXYNOS_PCIE_GEN2_S2MPU)
	int ret = 0;
#endif /* CONFIG_EXYNOS_PCIE_GEN2_S2MPU */
	if (dev == NULL) {
		pr_err("EP device is NULL!!!\n");
		return;
	} else {
#if IS_ENABLED(CONFIG_PCI_DOMAINS_GENERIC)
		ch_num = pci_domain_nr(epdev->bus);
#endif /* CONFIG_PCI_DOMAINS_GENERIC */
	}

	dma_direct_free(dev, size, vaddr, dma_handle, attrs);

#if IS_ENABLED(CONFIG_EXYNOS_PCIE_GEN2_IOMMU)
	pcie_iommu_unmap(ch_num, dma_handle, size);
#endif /* CONFIG_EXYNOS_PCIE_GEN2_IOMMU */

#if IS_ENABLED(CONFIG_EXYNOS_PCIE_GEN2_S2MPU)
	ret = exynos_pcie_unset_dev_stage2_ap("PCIE_GEN2", dma_handle,
			size, ATTR_NO_ACCESS);
	if (ret) {
		pr_err("[harx] ATTR_NO_ACCESS setting fail\n");
	}
#endif /* CONFIG_EXYNOS_PCIE_GEN2_S2MPU */
}

static dma_addr_t exynos_pcie_dma_direct_map_page(struct device *dev,
		struct page *page, unsigned long offset,
		size_t size, enum dma_data_direction dir,
		unsigned long attrs)
{
	dma_addr_t dev_addr;
#if IS_ENABLED(CONFIG_PCI_DOMAINS_GENERIC) || IS_ENABLED(CONFIG_EXYNOS_PCIE_GEN2_IOMMU)
	int ch_num = 0;
#endif /* CONFIG_PCI_DOMAINS_GENERIC || CONFIG_EXYNOS_PCIE_GEN2_IOMMU */
#if IS_ENABLED(CONFIG_PCI_DOMAINS_GENERIC)
	struct pci_dev *epdev = to_pci_dev_from_dev(dev);
#endif /* CONFIG_PCI_DOMAINS_GENERIC */
	int ret = 0;
	//dev_info(dev, "[%s] size: 0x%x\n", __func__,(unsigned int)size);
	if (dev == NULL) {
		pr_err("EP device is NULL!!!\n");
		return -EINVAL;
	} else {
#if IS_ENABLED(CONFIG_PCI_DOMAINS_GENERIC)
		ch_num = pci_domain_nr(epdev->bus);
#endif /* CONFIG_PCI_DOMAINS_GENERIC */
	}

	dev_addr = dma_direct_map_page(dev, page, offset, size, dir, attrs);

#if IS_ENABLED(CONFIG_EXYNOS_PCIE_GEN2_IOMMU)
	ret = pcie_iommu_map(ch_num, dev_addr, dev_addr, size, dir);
	if (ret != 0) {
		pr_err("DMA map - Can't map PCIe SysMMU table!!!\n");

		return 0;
	}
#endif /* CONFIG_EXYNOS_PCIE_GEN2_IOMMU */

#if IS_ENABLED(CONFIG_EXYNOS_PCIE_GEN2_S2MPU)
	ret = exynos_pcie_set_dev_stage2_ap("PCIE_GEN2", dev_addr,
			size, ATTR_RW);
	if (ret) {
		pr_err("[harx] ATTR_RW setting fail\n");
	}
#endif /* CONFIG_EXYNOS_PCIE_GEN2_S2MPU */
	return dev_addr;
}

static void exynos_pcie_dma_direct_unmap_page(struct device *dev,
		dma_addr_t dev_addr,
		size_t size, enum dma_data_direction dir,
		unsigned long attrs)
{
#if IS_ENABLED(CONFIG_PCI_DOMAINS_GENERIC) || IS_ENABLED(CONFIG_EXYNOS_PCIE_GEN2_IOMMU)
	int ch_num = 0;
#endif /* CONFIG_PCI_DOMAINS_GENERIC || CONFIG_EXYNOS_PCIE_GEN2_IOMMU */
#if IS_ENABLED(CONFIG_PCI_DOMAINS_GENERIC)
	struct pci_dev *epdev = to_pci_dev_from_dev(dev);
#endif /* CONFIG_PCI_DOMAINS_GENERIC */
#if IS_ENABLED(CONFIG_EXYNOS_PCIE_GEN2_S2MPU)
	int ret = 0;
#endif /* CONFIG_EXYNOS_PCIE_GEN2_S2MPU */
	if (dev == NULL) {
		pr_err("EP device is NULL!!!\n");
		return;
	} else {
#if IS_ENABLED(CONFIG_PCI_DOMAINS_GENERIC)
		ch_num = pci_domain_nr(epdev->bus);
#endif /* CONFIG_PCI_DOMAINS_GENERIC */
	}

	dma_direct_unmap_page(dev, dev_addr, size, dir, attrs);

#if IS_ENABLED(CONFIG_EXYNOS_PCIE_GEN2_IOMMU)
	pcie_iommu_unmap(ch_num, dev_addr, size);
#endif /* CONFIG_EXYNOS_PCIE_GEN2_IOMMU */

#if IS_ENABLED(CONFIG_EXYNOS_PCIE_GEN2_S2MPU)
	ret = exynos_pcie_unset_dev_stage2_ap("PCIE_GEN2", dev_addr,
			size, ATTR_NO_ACCESS);
	if (ret) {
		pr_err("[harx] ATTR_NO_ACCESS setting fail\n");
	}
#endif /* CONFIG_EXYNOS_PCIE_GEN2_S2MPU */
	return;
}

#if IS_ENABLED(CONFIG_EXYNOS_PCIE_GEN2_IOMMU)
void get_atomic_pool_info(dma_addr_t *paddr, size_t *size)
{
	*paddr = dma_pool_start;
	*size = gen_pool_size(atomic_pool);
}
EXPORT_SYMBOL(get_atomic_pool_info);
#endif /* CONFIG_EXYNOS_PCIE_GEN2_IOMMU */

struct dma_map_ops exynos_pcie_gen2_dma_ops = {
	.alloc = exynos_pcie_dma_direct_alloc,
	.free = exynos_pcie_dma_direct_free,
	.mmap = dma_common_mmap,
	.get_sgtable = dma_common_get_sgtable,
	.map_page = exynos_pcie_dma_direct_map_page,
	.unmap_page = exynos_pcie_dma_direct_unmap_page,
	.map_sg = dma_direct_map_sg,
	.unmap_sg = dma_direct_unmap_sg,
	.map_resource = dma_direct_map_resource,
	.sync_single_for_cpu = dma_direct_sync_single_for_cpu,
	.sync_single_for_device = dma_direct_sync_single_for_device,
	.sync_sg_for_cpu = dma_direct_sync_sg_for_cpu,
	.sync_sg_for_device = dma_direct_sync_sg_for_device,
	.get_required_mask = dma_direct_get_required_mask,
};
EXPORT_SYMBOL(exynos_pcie_gen2_dma_ops);
#endif /* CONFIG_EXYNOS_PCIE_GEN2_IOMMU || CONFIG_EXYNOS_PCIE_GEN2_S2MPU */

/*
 * Reserved memory
 */
#define MAX_PCIE_RMEM	1

struct pcie_reserved_mem {
	char *name;
	unsigned long base;
	u32 size;
};

static struct pcie_reserved_mem _pcie_rmem[MAX_PCIE_RMEM];

static int __init pcie_rmem_setup(struct reserved_mem *rmem)
{
	pr_info("%s: +++\n", __func__);

	_pcie_rmem[0].name = (char *)rmem->name;
	_pcie_rmem[0].base = rmem->base;
	_pcie_rmem[0].size = rmem->size;

	pr_info("%s: [rmem] base = 0x%x, size = 0x%x\n", __func__,
			rmem->base, rmem->size);
	pr_info("%s: [_pcie_rmem] name = %s, base = 0x%llx, size = 0x%x\n",
			__func__, _pcie_rmem[0].name,
			_pcie_rmem[0].base, _pcie_rmem[0].size);
	return 0;
}
RESERVEDMEM_OF_DECLARE(wifi_msi_rmem, "exynos,pcie_rmem", pcie_rmem_setup);

static int save_pcie_rmem(int ch_num)
{
	struct exynos_pcie *exynos_pcie = &g_pcie[ch_num];

	exynos_pcie->rmem_msi_name = _pcie_rmem[0].name;
	exynos_pcie->rmem_msi_base = _pcie_rmem[0].base;
	exynos_pcie->rmem_msi_size = _pcie_rmem[0].size;

	pr_info("%s: [rmem_msi] name=%s, base=0x%llx, size=0x%x\n", __func__,
			exynos_pcie->rmem_msi_name, exynos_pcie->rmem_msi_base,
			exynos_pcie->rmem_msi_size);

	return 0;
}

static unsigned long get_msi_base(int ch_num)
{
	struct exynos_pcie *exynos_pcie = &g_pcie[ch_num];

	pr_info("%s: [rmem_msi] name=%s, base=0x%llx, size=0x%x\n", __func__,
			exynos_pcie->rmem_msi_name, exynos_pcie->rmem_msi_base,
			exynos_pcie->rmem_msi_size);

	return exynos_pcie->rmem_msi_base;
}

#if IS_ENABLED(CONFIG_SOC_EXYNOS2100)
static void exynos_gen4_nclkoff(void)
{
	void __iomem *nclk_gen4;
	u32 nclk_val;

	nclk_gen4 = ioremap(0x11021064, 0x4);
	nclk_val = readl(nclk_gen4);
	nclk_val = nclk_val & ~(0x1 << 1);
	writel(nclk_val, nclk_gen4);
	iounmap(nclk_gen4);
}
#endif

#ifdef GEN3_PHY_OFF
static void exynos_gen3_phy_pwrdn(struct exynos_pcie *exynos_pcie)
{
	void __iomem *gen3_phy_base;
	void __iomem *cmu_fsys0_base;

	gen3_phy_base = ioremap(0x13150000, 0x2000);
	cmu_fsys0_base = ioremap(0x13000000, 0x4000);

	/*Isolation Bypass*/
	regmap_update_bits(exynos_pcie->pmureg,
			exynos_pcie->pmu_offset,
			PCIE_PHY_CONTROL_MASK, 1);

	mdelay(100);

	writel(0x3, gen3_phy_base + 0x400);

	writel(0x3F0003, cmu_fsys0_base + 0x304C);
	writel(0x3F0003, cmu_fsys0_base + 0x3050);
	writel(0x3F0003, cmu_fsys0_base + 0x3054);

	/* After setting for PHY power-down, it needs to set PMU isolation setting with delay*/
	mdelay(100);
	/*Isolation*/
	regmap_update_bits(exynos_pcie->pmureg,
			exynos_pcie->pmu_offset,
			PCIE_PHY_CONTROL_MASK, 0);

	iounmap(gen3_phy_base);
	iounmap(cmu_fsys0_base);
}
#endif

int exynos_pcie_get_irq_num(int ch_num)
{
	struct exynos_pcie *exynos_pcie = &g_pcie[ch_num];
	struct dw_pcie *pci = exynos_pcie->pci;
	struct pcie_port *pp = &pci->pp;

	dev_info(pci->dev, "%s: pp->irq = %d\n", __func__, pp->irq);
	return pp->irq;
}
EXPORT_SYMBOL(exynos_pcie_get_irq_num);

/* Get EP pci_dev structure of BUS 1 */
static struct pci_dev *exynos_pcie_get_pci_dev(struct pcie_port *pp)
{
	struct dw_pcie *pci = to_dw_pcie_from_pp(pp);
	struct exynos_pcie *exynos_pcie = to_exynos_pcie(pci);
	static struct pci_dev *ep_pci_dev;
	int domain_num;
	u32 val;

	if (ep_pci_dev != NULL)
		return ep_pci_dev;

	/* Get EP vendor/device ID to get pci_dev structure */
	domain_num = exynos_pcie->pci_dev->bus->domain_nr;

	if (exynos_pcie->ep_pci_bus == NULL)
		exynos_pcie->ep_pci_bus = pci_find_bus(domain_num, 1);

	exynos_pcie_rd_other_conf(pp, exynos_pcie->ep_pci_bus, 0, PCI_VENDOR_ID, 4, &val);

	ep_pci_dev = pci_get_device(val & ID_MASK, (val >> 16) & ID_MASK, NULL);

	exynos_pcie->ep_pcie_cap_off = ep_pci_dev->pcie_cap + PCI_EXP_LNKCTL;
	exynos_pcie->ep_l1ss_cap_off = pci_find_ext_capability(ep_pci_dev, PCI_EXT_CAP_ID_L1SS);
	if (!exynos_pcie->ep_l1ss_cap_off)
		pr_info("%s: cannot find l1ss capability offset\n", __func__);

	return ep_pci_dev;

}

static int exynos_pcie_set_l1ss(int enable, struct pcie_port *pp, int id)
{
	struct dw_pcie *pci = to_dw_pcie_from_pp(pp);
	struct exynos_pcie *exynos_pcie = to_exynos_pcie(pci);
	u32 val;
	unsigned long flags;
	struct pci_dev *ep_pci_dev;
	u32 exp_cap_off = PCIE_CAP_OFFSET;

	/* This function is only for BCM WIFI devices */
	if ((exynos_pcie->ep_device_type != EP_BCM_WIFI) && (exynos_pcie->ep_device_type != EP_QC_WIFI)) {
		dev_err(pci->dev, "Can't set L1SS(It's not WIFI device)!!!\n");
		return -EINVAL;
	}

	dev_info(pci->dev, "%s: START (state = 0x%x, id = 0x%x, enable = %d)\n",
			__func__, exynos_pcie->l1ss_ctrl_id_state, id, enable);
	spin_lock_irqsave(&exynos_pcie->conf_lock, flags);
	if (exynos_pcie->state != STATE_LINK_UP || exynos_pcie->atu_ok == 0) {
		if (enable)
			exynos_pcie->l1ss_ctrl_id_state &= ~(id);
		else
			exynos_pcie->l1ss_ctrl_id_state |= id;
		spin_unlock_irqrestore(&exynos_pcie->conf_lock, flags);
		dev_info(pci->dev, "%s: It's not needed. This will be set later."
				"(state = 0x%x, id = 0x%x)\n",
				__func__, exynos_pcie->l1ss_ctrl_id_state, id);
		return -1;
	}

	ep_pci_dev = exynos_pcie_get_pci_dev(pp);
	if (ep_pci_dev == NULL) {
		spin_unlock_irqrestore(&exynos_pcie->conf_lock, flags);
		dev_err(pci->dev, "Failed to set L1SS %s (pci_dev == NULL)!!!\n",
				enable ? "ENABLE" : "FALSE");
		return -EINVAL;
	}

	if (enable) {
		exynos_pcie->l1ss_ctrl_id_state &= ~(id);
		if (exynos_pcie->l1ss_ctrl_id_state == 0) {
			if (exynos_pcie->ep_device_type == EP_BCM_WIFI) {
				/* RC L1ss Enable */
				exynos_pcie_rd_own_conf(pp, PCIE_LINK_L1SS_CONTROL, 4, &val);
				val |= PORT_LINK_L1SS_ENABLE;
				exynos_pcie_wr_own_conf(pp, PCIE_LINK_L1SS_CONTROL, 4, val);
				dev_err(pci->dev, "Enable L1ss: L1SS_CONTROL (RC_read)=0x%x\n", val);

				/* EP L1ss Enable */
				pci_read_config_dword(ep_pci_dev, WIFI_L1SS_CONTROL, &val);
				val |= WIFI_ALL_PM_ENABEL;
				pci_write_config_dword(ep_pci_dev, WIFI_L1SS_CONTROL, val);
				dev_err(pci->dev, "Enable L1ss: EP L1SS_CONTROL = 0x%x\n", val);
			} else {
				/* RC L1ss Enable */
				exynos_pcie_rd_own_conf(pp, PCIE_LINK_L1SS_CONTROL, 4, &val);
				val |= PORT_LINK_L12_ENABLE;
				exynos_pcie_wr_own_conf(pp, PCIE_LINK_L1SS_CONTROL, 4, val);
				dev_err(pci->dev, "Enable L1ss: L1SS_CONTROL (RC_read)=0x%x\n", val);

				/* EP L1ss Enable */
				pci_read_config_dword(ep_pci_dev, exynos_pcie->ep_l1ss_ctrl1_off, &val);
				val |= (WIFI_ASPM_L12_EN | WIFI_PCIPM_L12_EN);
				pci_write_config_dword(ep_pci_dev, exynos_pcie->ep_l1ss_ctrl1_off, val);
				dev_err(pci->dev, "Enable L1ss: EP L1SS_CONTROL = 0x%x\n", val);
			}

			/* RC ASPM Enable */
			exynos_pcie_rd_own_conf(pp,
					exp_cap_off + PCI_EXP_LNKCTL, 4, &val);
			val &= ~PCI_EXP_LNKCTL_ASPMC;
			val |= PCI_EXP_LNKCTL_CCC | PCI_EXP_LNKCTL_ASPM_L1;
			exynos_pcie_wr_own_conf(pp,
					exp_cap_off + PCI_EXP_LNKCTL, 4, val);

			if (exynos_pcie->ep_device_type == EP_BCM_WIFI) {
				/* EP ASPM Enable */
				pci_read_config_dword(ep_pci_dev, WIFI_L1SS_LINKCTRL,
						&val);
				val |= WIFI_ASPM_L1_ENTRY_EN;
				pci_write_config_dword(ep_pci_dev, WIFI_L1SS_LINKCTRL,
						val);
			} else {
				/* EP ASPM Enable */
				pci_read_config_dword(ep_pci_dev, exynos_pcie->ep_link_ctrl_off,
						&val);
				val |= WIFI_ASPM_L1_ENTRY_EN;
				pci_write_config_dword(ep_pci_dev, exynos_pcie->ep_link_ctrl_off,
						val);
			}
		}
	} else {
		if (exynos_pcie->l1ss_ctrl_id_state) {
			exynos_pcie->l1ss_ctrl_id_state |= id;
		} else {
			exynos_pcie->l1ss_ctrl_id_state |= id;

			if (exynos_pcie->ep_device_type == EP_BCM_WIFI) {
				/* EP ASPM Disable */
				pci_read_config_dword(ep_pci_dev,
						WIFI_L1SS_LINKCTRL, &val);
				val &= ~(WIFI_ASPM_CONTROL_MASK);
				pci_write_config_dword(ep_pci_dev,
						WIFI_L1SS_LINKCTRL, val);
			} else {
				/* EP ASPM Disable */
				pci_read_config_dword(ep_pci_dev,
						exynos_pcie->ep_link_ctrl_off, &val);
				val &= ~(WIFI_ASPM_CONTROL_MASK);
				pci_write_config_dword(ep_pci_dev,
						exynos_pcie->ep_link_ctrl_off, val);
			}

			/* RC ASPM Disable */
			exynos_pcie_rd_own_conf(pp,
					exp_cap_off + PCI_EXP_LNKCTL, 4, &val);
			val &= ~PCI_EXP_LNKCTL_ASPMC;
			exynos_pcie_wr_own_conf(pp,
					exp_cap_off + PCI_EXP_LNKCTL, 4, val);

			if (exynos_pcie->ep_device_type == EP_BCM_WIFI) {
				/* EP L1ss Disable */
				pci_read_config_dword(ep_pci_dev, WIFI_L1SS_CONTROL, &val);
				val &= ~(WIFI_ALL_PM_ENABEL);
				pci_write_config_dword(ep_pci_dev, WIFI_L1SS_CONTROL, val);
				dev_err(pci->dev, "Disable L1ss: EP L1SS_CONTROL = 0x%x\n", val);
			} else {
				/* EP L1ss Disable */
				pci_read_config_dword(ep_pci_dev, exynos_pcie->ep_l1ss_ctrl1_off, &val);
				val &= ~(WIFI_ALL_PM_ENABEL);
				pci_write_config_dword(ep_pci_dev, exynos_pcie->ep_l1ss_ctrl1_off, val);
				dev_err(pci->dev, "Disable L1ss: EP L1SS_CONTROL = 0x%x\n", val);
			}

			/* RC L1ss Disable */
			exynos_pcie_rd_own_conf(pp, PCIE_LINK_L1SS_CONTROL, 4, &val);
			val &= ~(PORT_LINK_L1SS_ENABLE);
			exynos_pcie_wr_own_conf(pp, PCIE_LINK_L1SS_CONTROL, 4, val);
			dev_err(pci->dev, "Disable L1ss: L1SS_CONTROL (RC_read)=0x%x\n", val);
		}
	}
	spin_unlock_irqrestore(&exynos_pcie->conf_lock, flags);
	dev_info(pci->dev, "%s: END (state = 0x%x, id = 0x%x, enable = %d)\n",
			__func__, exynos_pcie->l1ss_ctrl_id_state, id, enable);
	return 0;
}

int exynos_pcie_l1_exit(int ch_num)
{
	struct exynos_pcie *exynos_pcie = &g_pcie[ch_num];
	u32 count = 0, ret = 0, val = 0;
	unsigned long flags;

	spin_lock_irqsave(&exynos_pcie->pcie_l1_exit_lock, flags);

	if (exynos_pcie->l1ss_ctrl_id_state == 0) {
		/* Set s/w L1 exit mode */
		exynos_elbi_write(exynos_pcie, 0x1, PCIE_APP_REQ_EXIT_L1);
		val = exynos_elbi_read(exynos_pcie, PCIE_APP_REQ_EXIT_L1_MODE);
		val &= ~APP_REQ_EXIT_L1_MODE;
		exynos_elbi_write(exynos_pcie, val, PCIE_APP_REQ_EXIT_L1_MODE);

		/* Max timeout = 3ms (300 * 10us) */
		while (count < MAX_L1_EXIT_TIMEOUT) {
			val = exynos_elbi_read(exynos_pcie,
					PCIE_ELBI_RDLH_LINKUP) & 0x1f;
			if (val == 0x11)
				break;

			count++;

			udelay(10);
		}

		if (count >= MAX_L1_EXIT_TIMEOUT) {
			pr_err("%s: cannot change to L0(LTSSM = 0x%x, cnt = %d)\n",
					__func__, val, count);
			ret = -EPIPE;
		} else {
			/* remove too much print */
			/* pr_err("%s: L0 state(LTSSM = 0x%x, cnt = %d)\n",
					__func__, val, count); */
		}

		/* Set h/w L1 exit mode */
		val = exynos_elbi_read(exynos_pcie, PCIE_APP_REQ_EXIT_L1_MODE);
		val |= APP_REQ_EXIT_L1_MODE;
		exynos_elbi_write(exynos_pcie, val, PCIE_APP_REQ_EXIT_L1_MODE);
		exynos_elbi_write(exynos_pcie, 0x0, PCIE_APP_REQ_EXIT_L1);
	} else {
		/* remove too much print */
		/* pr_err("%s: skip!!! l1.2 is already diabled(id = 0x%x)\n",
				__func__, exynos_pcie->l1ss_ctrl_id_state); */
	}

	spin_unlock_irqrestore(&exynos_pcie->pcie_l1_exit_lock, flags);

	return ret;

}
EXPORT_SYMBOL(exynos_pcie_l1_exit);

int exynos_pcie_l1ss_ctrl(int enable, int id)
{
	struct pcie_port *pp = NULL;
	struct exynos_pcie *exynos_pcie;
	struct dw_pcie *pci;

	int i;

	for (i = 0; i < MAX_RC_NUM; i++) {
		if (g_pcie[i].ep_device_type == EP_BCM_WIFI || g_pcie[i].ep_device_type == EP_QC_WIFI) {
			exynos_pcie = &g_pcie[i];
			pci = exynos_pcie->pci;
			pp = &pci->pp;
			break;
		}
	}

	if (pp != NULL)
		return	exynos_pcie_set_l1ss(enable, pp, id);
	else
		return -EINVAL;
}
EXPORT_SYMBOL(exynos_pcie_l1ss_ctrl);

void exynos_pcie_register_dump(int ch_num)
{
	struct exynos_pcie *exynos_pcie = &g_pcie[ch_num];
	u32 i;

	hsi_tcxo_far_control(1, 1);
	pr_err("%s: +++\n", __func__);
	/* ---------------------- */
	/* Link Reg : 0x0 ~ 0x47C */
	/* ---------------------- */
	pr_err("[Print SUB_CTRL region]\n");
	pr_err("offset:             0x0               0x4               0x8               0xC\n");
	for (i = 0; i < 0x2D0; i += 0x10) {
		pr_err("ELBI 0x%04x:    0x%08x    0x%08x    0x%08x    0x%08x\n",
				i,
				exynos_elbi_read(exynos_pcie, i + 0x0),
				exynos_elbi_read(exynos_pcie, i + 0x4),
				exynos_elbi_read(exynos_pcie, i + 0x8),
				exynos_elbi_read(exynos_pcie, i + 0xC));
	}
	pr_err("\n");

	hsi_tcxo_far_control(1, 1);
	/* ---------------------- */
	/* PHY Reg : 0x0 ~ 0x19C */
	/* ---------------------- */
	pr_err("[Print PHY region]\n");
	pr_err("offset:             0x0               0x4               0x8               0xC\n");
	for (i = 0; i < 0x200; i += 0x10) {
		pr_err("PHY 0x%04x:    0x%08x    0x%08x    0x%08x    0x%08x\n",
				i,
				exynos_phy_read(exynos_pcie, i + 0x0),
				exynos_phy_read(exynos_pcie, i + 0x4),
				exynos_phy_read(exynos_pcie, i + 0x8),
				exynos_phy_read(exynos_pcie, i + 0xC));
	}

	hsi_tcxo_far_control(1, 1);
	/* ---------------------- */
	/* PHY PCS : 0x0 ~ 0x19C */
	/* ---------------------- */
	pr_err("[Print PHY_PCS region]\n");
	pr_err("offset:             0x0               0x4               0x8               0xC\n");
	for (i = 0; i < 0x200; i += 0x10) {
		pr_err("PCS 0x%04x:    0x%08x    0x%08x    0x%08x    0x%08x\n",
				i,
				exynos_phy_pcs_read(exynos_pcie, i + 0x0),
				exynos_phy_pcs_read(exynos_pcie, i + 0x4),
				exynos_phy_pcs_read(exynos_pcie, i + 0x8),
				exynos_phy_pcs_read(exynos_pcie, i + 0xC));
	}
	pr_err("\n");

	pr_err("%s: ---\n", __func__);
}
EXPORT_SYMBOL(exynos_pcie_register_dump);

static int l1ss_test_thread(void *arg)
{
	u32 msec, rnum;

	while (!kthread_should_stop()) {
		get_random_bytes(&rnum, sizeof(int));
		msec = rnum % 10000;

		if (msec % 2 == 0)
			msec %= 1000;

		pr_err("%d msec L1SS disable!!!\n", msec);
		exynos_pcie_l1ss_ctrl(0, (0x1 << 31));
		msleep(msec);

		pr_err("%d msec L1SS enable!!!\n", msec);
		exynos_pcie_l1ss_ctrl(1, (0x1 << 31));
		msleep(msec);

	}
	return 0;
}

static ssize_t show_pcie(struct device *dev,
		struct device_attribute *attr,
		char *buf)
{
	int ret = 0;

	ret += snprintf(buf + ret, PAGE_SIZE - ret, ">>>> PCIe Test <<<<\n");
	ret += snprintf(buf + ret, PAGE_SIZE - ret, "0 : PCIe Unit Test\n");
	ret += snprintf(buf + ret, PAGE_SIZE - ret, "1 : Link Test\n");
	ret += snprintf(buf + ret, PAGE_SIZE - ret, "2 : DisLink Test\n");
	ret += snprintf(buf + ret, PAGE_SIZE - ret,
				"3 : Runtime L1.2 En/Disable Test\n");
	ret += snprintf(buf + ret, PAGE_SIZE - ret,
				"4 : Stop Runtime L1.2 En/Disable Test\n");
	ret += snprintf(buf + ret, PAGE_SIZE - ret,
				"5 : PCIE supports to enter ASPM L1 state test\\n");
	ret += snprintf(buf + ret, PAGE_SIZE - ret,
				"6 : Check PCIe LTSSM\n");

	return ret;
}

static ssize_t store_pcie(struct device *dev,
		struct device_attribute *attr,
		const char *buf, size_t count)
{
	int op_num;
	struct exynos_pcie *exynos_pcie = dev_get_drvdata(dev);
	struct dw_pcie *pci = exynos_pcie->pci;
	struct pcie_port *pp = &pci->pp;
	int wlan_gpio = of_get_named_gpio(dev->of_node, "pcie,wlan-gpio", 0);
	static struct task_struct *task;
	u64 iommu_addr = 0, iommu_size = 0;
	int ret;
	u32 val;

	if (sscanf(buf, "%10d%llx%llx", &op_num, &iommu_addr, &iommu_size) == 0)
		return -EINVAL;

	switch (op_num) {
	case 0:
		dev_info(dev, "PCIe UNIT test START.\n");
		ret = exynos_pcie_dbg_unit_test(dev, exynos_pcie);
		if (ret) {
			dev_err(dev, "PCIe UNIT test failed (%d)\n", ret);
			break;
		}
		dev_err(dev, "## PCIe UNIT test SUCCESS!!##\n");
		break;
	case 1:
		exynos_pcie_dbg_link_test(dev, exynos_pcie, 1);
		break;

	case 2:
		exynos_pcie_dbg_link_test(dev, exynos_pcie, 0);
		if (wlan_gpio >= 0)
			gpio_set_value(wlan_gpio, 0);
		break;

	case 3:
		dev_info(dev, "L1.2 En/Disable thread Start!!!\n");
		task = kthread_run(l1ss_test_thread, NULL, "L1SS_Test");
		break;

	case 4:
		dev_info(dev, "L1.2 En/Disable thread Stop!!!\n");
		if (task != NULL)
			kthread_stop(task);
		else
			dev_err(dev, "task is NULL!!!!\n");
		break;

	case 5:
		dev_info(dev, " PCIE supports to enter ASPM L1 state test\n");

		exynos_pcie_rd_own_conf(pp, 0x0158, 4, &val);
		val |= PORT_LINK_TCOMMON_32US | PORT_LINK_L1SS_ENABLE;
		val = val & 0xffffff00;
		exynos_pcie_wr_own_conf(pp, 0x0158, 4, val);
		exynos_pcie_rd_own_conf(pp, 0x0158, 4, &val);
		dev_info(dev, "dbi + 0x158 :0x%08x\n", val);

		mdelay(50);

		dev_info(dev, "LTSSM: 0x%08x\n",
				exynos_elbi_read(exynos_pcie, PCIE_ELBI_RDLH_LINKUP));
		dev_info(dev, "PHY PLL LOCK: 0x%08x (check last2 hex = 0x11)\n",
				readl(exynos_pcie->phy_base + 0xBC));
		break;
	case 6:
		dev_info(dev, " PCIE Check ");
		dev_info(dev, "LTSSM: 0x%08x\n",
				exynos_elbi_read(exynos_pcie, PCIE_ELBI_RDLH_LINKUP));
		break;
	case 9:
		dev_info(dev, "Enable SysMMU feature.\n");
		exynos_pcie->use_sysmmu = true;
		pcie_sysmmu_enable(0);

		break;
	case 10:
		dev_info(dev, "Disable SysMMU feature.\n");
		exynos_pcie->use_sysmmu = false;
		pcie_sysmmu_disable(0);
		break;

	case 11:
		dev_info(dev, "SysMMU MAP - 0x%llx(sz:0x%llx)\n",
				iommu_addr, iommu_size);
		pcie_iommu_map(0, iommu_addr, iommu_addr, iommu_size, 0x3);
		break;

	case 12:
		dev_info(dev, "SysMMU UnMAP - 0x%llx(sz:0x%llx)\n",
			iommu_addr, iommu_size);
		pcie_iommu_unmap(0, iommu_addr, iommu_size);
		break;

	case 13:
		dev_info(dev, "L1.2 Disable....\n");
		exynos_pcie_l1ss_ctrl(0, 0x40000000);
		break;
	case 14:
		dev_info(dev, "L1.2 Enable....\n");
		exynos_pcie_l1ss_ctrl(1, 0x40000000);
		break;
	case 15:
		dev_info(dev, "l1ss_ctrl_id_state = 0x%08x\n",
				exynos_pcie->l1ss_ctrl_id_state);
		dev_info(dev, "LTSSM: 0x%08x\n",
				exynos_elbi_read(exynos_pcie, PCIE_ELBI_RDLH_LINKUP));
		dev_info(dev, "PHY PLL LOCK: 0x%08x (check last2 hex =0x11)\n",
				readl(exynos_pcie->phy_base + 0xBC));
		break;
	case 16:
		{
			void __iomem *pppmu;

			pppmu = ioremap(0x15863e10, 0x10);

			dev_info(dev, "TCXO_FAR: 0x%x", readl(pppmu));

			iounmap(pppmu);
		}
		break;
	case 17:
		{
			void __iomem *testmem;

			testmem = ioremap(iommu_addr, iommu_size);

			dev_info(dev, "readl(0x%llx): 0x%llx",
					iommu_addr, readl(testmem));

			iounmap(testmem);
		}
		break;
	default:
		dev_err(dev, "Unsupported Test Number(%d)...\n", op_num);
	}

	return count;
}

static DEVICE_ATTR(pcie_sysfs, S_IWUSR | S_IWGRP | S_IRUSR | S_IRGRP,
			show_pcie, store_pcie);

static inline int create_pcie_sys_file(struct device *dev)
{
	return device_create_file(dev, &dev_attr_pcie_sysfs);
}

static inline void remove_pcie_sys_file(struct device *dev)
{
	device_remove_file(dev, &dev_attr_pcie_sysfs);
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
		exynos_pcie_poweroff(exynos_pcie->ch_num);
		exynos_pcie_poweron(exynos_pcie->ch_num);
	}
}

int exynos_pcie_dump_link_down_status(int ch_num)
{
	struct exynos_pcie *exynos_pcie = &g_pcie[ch_num];
	struct dw_pcie *pci = exynos_pcie->pci;

	if (exynos_pcie->state == STATE_LINK_UP) {
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
	} else
		dev_info(pci->dev, "PCIE link state is %d\n",
				exynos_pcie->state);

	return 0;
}
EXPORT_SYMBOL(exynos_pcie_dump_link_down_status);

#ifdef USE_PANIC_NOTIFIER
static int exynos_pcie_dma_mon_event(struct notifier_block *nb,
				unsigned long event, void *data)
{
	int ret;
	struct exynos_pcie *exynos_pcie =
		container_of(nb, struct exynos_pcie, ss_dma_mon_nb);

	ret = exynos_pcie_dump_link_down_status(exynos_pcie->ch_num);
	if (exynos_pcie->state == STATE_LINK_UP)
		exynos_pcie_register_dump(exynos_pcie->ch_num);

	return notifier_from_errno(ret);
}
#endif

static int exynos_pcie_clock_get(struct pcie_port *pp)
{
	struct dw_pcie *pci = to_dw_pcie_from_pp(pp);
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
			clks->pcie_clks[i] = of_clk_get(pci->dev->of_node, i);
			if (IS_ERR(clks->pcie_clks[i])) {
				dev_err(pci->dev, "Failed to get pcie clock\n");
				return -ENODEV;
			}
		} else {
			phy_count = i - exynos_pcie->pcie_clk_num;
			clks->phy_clks[phy_count] =
				of_clk_get(pci->dev->of_node, i);
			if (IS_ERR(clks->phy_clks[i])) {
				dev_err(pci->dev, "Failed to get pcie clock\n");
				return -ENODEV;
			}
		}
	}

	return 0;
}

static int exynos_pcie_clock_enable(struct pcie_port *pp, int enable)
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
				panic("[PCIe Case#2] pcie clk fail! %s\n", exynos_pcie->ep_device_name);
#endif
		}
	} else {
		for (i = 0; i < exynos_pcie->pcie_clk_num; i++)
			clk_disable_unprepare(clks->pcie_clks[i]);
	}

	return ret;
}

static int exynos_pcie_phy_clock_enable(struct pcie_port *pp, int enable)
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
				panic("[PCIe Case#2] PHY clk fail! %s\n", exynos_pcie->ep_device_name);
#endif
		}
	} else {
		for (i = 0; i < exynos_pcie->phy_clk_num; i++)
			clk_disable_unprepare(clks->phy_clks[i]);
	}

	return ret;
}
void exynos_pcie_print_link_history(struct pcie_port *pp)
{
	struct dw_pcie *pci = to_dw_pcie_from_pp(pp);
	struct device *dev = pci->dev;
	struct exynos_pcie *exynos_pcie = to_exynos_pcie(pci);
	u32 history_buffer[32];
	int i;

	for (i = 31; i >= 0; i--)
		history_buffer[i] = exynos_elbi_read(exynos_pcie,
				PCIE_HISTORY_REG(i));
	for (i = 31; i >= 0; i--)
		dev_info(dev, "LTSSM: 0x%02x, L1sub: 0x%x, D state: 0x%x\n",
				LTSSM_STATE(history_buffer[i]),
				L1SUB_STATE(history_buffer[i]),
				PM_DSTATE(history_buffer[i]));
}

static void exynos_pcie_setup_rc(struct pcie_port *pp)
{
	struct dw_pcie *pci = to_dw_pcie_from_pp(pp);
	struct exynos_pcie *exynos_pcie = to_exynos_pcie(pci);
	u32 pcie_cap_off = PCIE_CAP_OFFSET;
	u32 pm_cap_off = PM_CAP_OFFSET;
	u32 val;

	/* enable writing to DBI read-only registers */
	exynos_pcie_wr_own_conf(pp, PCIE_MISC_CONTROL, 4, DBI_RO_WR_EN);

	/* change vendor ID and device ID for PCIe */
	exynos_pcie_wr_own_conf(pp, PCI_VENDOR_ID, 2, PCI_VENDOR_ID_SAMSUNG);
	exynos_pcie_wr_own_conf(pp, PCI_DEVICE_ID, 2,
			    PCI_DEVICE_ID_EXYNOS + exynos_pcie->ch_num);

	/* set max link width & speed : Gen2, Lane1 */
	exynos_pcie_rd_own_conf(pp, pcie_cap_off + PCI_EXP_LNKCAP, 4, &val);
	val &= ~(PCI_EXP_LNKCAP_L1EL | PCI_EXP_LNKCAP_MLW | PCI_EXP_LNKCAP_SLS);
	val |= PCI_EXP_LNKCAP_L1EL_64USEC | PCI_EXP_LNKCAP_MLW_X1;

	if (exynos_pcie->ip_ver >= 0x981000 && exynos_pcie->ch_num == 1)
		val |= PCI_EXP_LNKCTL2_TLS_8_0GB;
	else
		val |= PCI_EXP_LNKCAP_SLS_5_0GB;

	exynos_pcie_wr_own_conf(pp, pcie_cap_off + PCI_EXP_LNKCAP, 4, val);

	/* set auxiliary clock frequency: 26MHz */
	exynos_pcie_wr_own_conf(pp, PCIE_AUX_CLK_FREQ_OFF, 4,
			PCIE_AUX_CLK_FREQ_26MHZ);

	/* set duration of L1.2 & L1.2.Entry */
	exynos_pcie_wr_own_conf(pp, PCIE_L1_SUBSTATES_OFF, 4, PCIE_L1_SUB_VAL);

	/* clear power management control and status register */
	exynos_pcie_wr_own_conf(pp, pm_cap_off + PCI_PM_CTRL, 4, 0x0);

	/* initiate link retraining */
	exynos_pcie_rd_own_conf(pp, pcie_cap_off + PCI_EXP_LNKCTL, 4, &val);
	val |= PCI_EXP_LNKCTL_RL;
	exynos_pcie_wr_own_conf(pp, pcie_cap_off + PCI_EXP_LNKCTL, 4, val);

	/* set target speed from DT */
	exynos_pcie_rd_own_conf(pp, pcie_cap_off + PCI_EXP_LNKCTL2, 4, &val);
	val &= ~PCI_EXP_LNKCTL2_TLS;
	val |= exynos_pcie->max_link_speed;
	exynos_pcie_wr_own_conf(pp, pcie_cap_off + PCI_EXP_LNKCTL2, 4, val);

	/* For bifurcation PHY */
	if (exynos_pcie->ip_ver == 0x889500) {
		/* enable PHY 2Lane Bifurcation mode */
		val = exynos_sysreg_read(exynos_pcie,
				PCIE_WIFI0_PCIE_PHY_CONTROL);
		val &= ~BIFURCATION_MODE_DISABLE;
		val |= LINK0_ENABLE | PCS_LANE0_ENABLE;
		exynos_sysreg_write(exynos_pcie, val,
				PCIE_WIFI0_PCIE_PHY_CONTROL);
	}
}

static void exynos_pcie_prog_viewport_cfg0(struct pcie_port *pp, u32 busdev)
{
	struct dw_pcie *pci = to_dw_pcie_from_pp(pp);
	struct exynos_pcie *exynos_pcie = to_exynos_pcie(pci);

	/* Program viewport 0 : OUTBOUND : CFG0 */
	exynos_pcie_wr_own_conf(pp, PCIE_ATU_VIEWPORT, 4,
			    EXYNOS_PCIE_ATU_REGION_OUTBOUND | EXYNOS_PCIE_ATU_REGION_INDEX1);
	exynos_pcie_wr_own_conf(pp, PCIE_ATU_LOWER_BASE, 4, pp->cfg0_base);
	exynos_pcie_wr_own_conf(pp, PCIE_ATU_UPPER_BASE, 4,
					(pp->cfg0_base >> 32));
	exynos_pcie_wr_own_conf(pp, PCIE_ATU_LIMIT, 4,
					pp->cfg0_base + pp->cfg0_size - 1);
	exynos_pcie_wr_own_conf(pp, PCIE_ATU_LOWER_TARGET, 4, busdev);
	exynos_pcie_wr_own_conf(pp, PCIE_ATU_UPPER_TARGET, 4, 0);
	exynos_pcie_wr_own_conf(pp, PCIE_ATU_CR1, 4, EXYNOS_PCIE_ATU_TYPE_CFG0);
	exynos_pcie_wr_own_conf(pp, PCIE_ATU_CR2, 4, EXYNOS_PCIE_ATU_ENABLE);

	exynos_pcie->atu_ok = 1;
}

static void exynos_pcie_prog_viewport_cfg1(struct pcie_port *pp, u32 busdev)
{
	/* Program viewport 1 : OUTBOUND : CFG1 */
	exynos_pcie_wr_own_conf(pp, PCIE_ATU_VIEWPORT, 4,
			    EXYNOS_PCIE_ATU_REGION_OUTBOUND | EXYNOS_PCIE_ATU_REGION_INDEX1);
	exynos_pcie_wr_own_conf(pp, PCIE_ATU_CR1, 4, EXYNOS_PCIE_ATU_TYPE_CFG1);
	exynos_pcie_wr_own_conf(pp, PCIE_ATU_LOWER_BASE, 4, pp->cfg1_base);
	exynos_pcie_wr_own_conf(pp, PCIE_ATU_UPPER_BASE, 4,
					(pp->cfg1_base >> 32));
	exynos_pcie_wr_own_conf(pp, PCIE_ATU_LIMIT, 4,
					pp->cfg1_base + pp->cfg1_size - 1);
	exynos_pcie_wr_own_conf(pp, PCIE_ATU_LOWER_TARGET, 4, busdev);
	exynos_pcie_wr_own_conf(pp, PCIE_ATU_UPPER_TARGET, 4, 0);
	exynos_pcie_wr_own_conf(pp, PCIE_ATU_CR2, 4, EXYNOS_PCIE_ATU_ENABLE);
}

#if IS_ENABLED(CONFIG_SOC_EXYNOS8895)
/*
 * The below code is only for 8895 to solve memory shortage problem
 * CAUTION - If you want to use the below code properly, you should
 * remove code which use Outbound ATU index2 in pcie-designware.c
 */
void dw_pcie_prog_viewport_mem_outbound_index2(struct pcie_port *pp)
{
	/* Program viewport 0 : OUTBOUND : MEM */
	exynos_pcie_wr_own_conf(pp, PCIE_ATU_VIEWPORT, 4,
			EXYNOS_PCIE_ATU_REGION_OUTBOUND | EXYNOS_PCIE_ATU_REGION_INDEX2);
	exynos_pcie_wr_own_conf(pp, PCIE_ATU_CR1, 4, EXYNOS_PCIE_ATU_TYPE_MEM);
	exynos_pcie_wr_own_conf(pp, PCIE_ATU_LOWER_BASE, 4, 0x11B00000);
	exynos_pcie_wr_own_conf(pp, PCIE_ATU_UPPER_BASE, 4, 0x0);
	exynos_pcie_wr_own_conf(pp, PCIE_ATU_LIMIT, 4, 0x11B0ffff);
	exynos_pcie_wr_own_conf(pp, PCIE_ATU_LOWER_TARGET, 4, 0x11C00000);
	exynos_pcie_wr_own_conf(pp, PCIE_ATU_UPPER_TARGET, 4, 0x0);
	exynos_pcie_wr_own_conf(pp, PCIE_ATU_CR2, 4, EXYNOS_PCIE_ATU_ENABLE);
}
#endif

static void exynos_pcie_prog_viewport_mem_outbound(struct pcie_port *pp)
{
	/* Program viewport 0 : OUTBOUND : MEM */
	exynos_pcie_wr_own_conf(pp, PCIE_ATU_VIEWPORT, 4,
			    EXYNOS_PCIE_ATU_REGION_OUTBOUND | EXYNOS_PCIE_ATU_REGION_INDEX0);
	exynos_pcie_wr_own_conf(pp, PCIE_ATU_CR1, 4, EXYNOS_PCIE_ATU_TYPE_MEM);
	exynos_pcie_wr_own_conf(pp, PCIE_ATU_LOWER_BASE, 4, pp->mem_base);
	exynos_pcie_wr_own_conf(pp, PCIE_ATU_UPPER_BASE, 4,
					(pp->mem_base >> 32));
#if IS_ENABLED(CONFIG_SOC_EXYNOS8895)
	exynos_pcie_wr_own_conf(pp, PCIE_ATU_LIMIT, 4,
				pp->mem_base + pp->mem_size - 1 - SZ_1M);
#else
	exynos_pcie_wr_own_conf(pp, PCIE_ATU_LIMIT, 4,
				pp->mem_base + pp->mem_size - 1);
#endif
	exynos_pcie_wr_own_conf(pp, PCIE_ATU_LOWER_TARGET, 4, pp->mem_bus_addr);
	exynos_pcie_wr_own_conf(pp, PCIE_ATU_UPPER_TARGET, 4,
					upper_32_bits(pp->mem_bus_addr));
	exynos_pcie_wr_own_conf(pp, PCIE_ATU_CR2, 4, EXYNOS_PCIE_ATU_ENABLE);

#if IS_ENABLED(CONFIG_SOC_EXYNOS8895)
	dw_pcie_prog_viewport_mem_outbound_index2(pp);
#endif
}

static int exynos_pcie_establish_link(struct pcie_port *pp)
{
	struct dw_pcie *pci = to_dw_pcie_from_pp(pp);
	struct exynos_pcie *exynos_pcie = to_exynos_pcie(pci);
	struct device *dev = pci->dev;
	u32 val, busdev;
	int count = 0, try_cnt = 0;

retry:
	/* avoid checking rx elecidle when access DBI */
	if (exynos_pcie->phy_ops.phy_check_rx_elecidle != NULL)
		exynos_pcie->phy_ops.phy_check_rx_elecidle(
				exynos_pcie->phy_pcs_base, IGNORE_ELECIDLE,
				exynos_pcie->ch_num);

	exynos_pcie_assert_phy_reset(pp);

	exynos_elbi_write(exynos_pcie, 0x0, PCIE_SOFT_CORE_RESET);
	udelay(20);
	exynos_elbi_write(exynos_pcie, 0x1, PCIE_SOFT_CORE_RESET);

	if (exynos_pcie->ep_device_type == EP_QC_WIFI)
					usleep_range(10000, 12000);
	else if (exynos_pcie->ep_device_type == EP_BCM_WIFI)
					usleep_range(1000, 1200);

	usleep_range(1000, 1200);

	/* set #PERST high */
	gpio_set_value(exynos_pcie->perst_gpio, 1);
	dev_info(dev, "%s: Set PERST to HIGH, gpio val = %d\n",
			__func__, gpio_get_value(exynos_pcie->perst_gpio));
	if (exynos_pcie->ep_ver == 0x04389) {
		dev_dbg(dev, "ep_ver: 0x%x (usleep 20~22ms)\n", exynos_pcie->ep_ver);
		usleep_range(20000, 22000);
	} else {
		dev_dbg(dev, "ep_ver: 0x%x (usleep 18~20ms)\n", exynos_pcie->ep_ver);
		usleep_range(18000, 20000);
	}

	/* APP_REQ_EXIT_L1_MODE : BIT0 (0x0 : S/W mode, 0x1 : H/W mode) */
	val = exynos_elbi_read(exynos_pcie, PCIE_APP_REQ_EXIT_L1_MODE);
	val |= APP_REQ_EXIT_L1_MODE;
	val |= L1_REQ_NAK_CONTROL_MASTER;
	exynos_elbi_write(exynos_pcie, val, PCIE_APP_REQ_EXIT_L1_MODE);
	exynos_elbi_write(exynos_pcie, PCIE_LINKDOWN_RST_MANUAL,
			  PCIE_LINKDOWN_RST_CTRL_SEL);

	/* Q-Channel support and L1 NAK enable when AXI pending */
	val = exynos_elbi_read(exynos_pcie, PCIE_QCH_SEL);
	if (exynos_pcie->ip_ver >= 0x889500) {
		val &= ~(CLOCK_GATING_PMU_MASK | CLOCK_GATING_APB_MASK |
				CLOCK_GATING_AXI_MASK);
	} else {
		val &= ~CLOCK_GATING_MASK;
		val |= CLOCK_NOT_GATING;
	}
	exynos_elbi_write(exynos_pcie, val, PCIE_QCH_SEL);

	if (exynos_pcie->ip_ver == 0x889000) {
		exynos_elbi_write(exynos_pcie, 0x1, PCIE_L1_BUG_FIX_ENABLE);
	}

	{
		dev_info(dev, "2) TCXO_FAR: 0x%x, Lane0: 0x%x, isol: 0x%x, PLL Lock: 0x%x\n",
				readl(exynos_pcie->dbg_pmu1),
				exynos_sysreg_read(exynos_pcie,	PCIE_WIFI0_PCIE_PHY_CONTROL),
				readl(exynos_pcie->dbg_pmu2),
				readl(exynos_pcie->phy_base + 0xBC));
	}

	/* setup root complex */
	dw_pcie_setup_rc(pp);
	exynos_pcie_setup_rc(pp);

	if (exynos_pcie->phy_ops.phy_check_rx_elecidle != NULL)
		exynos_pcie->phy_ops.phy_check_rx_elecidle(
				exynos_pcie->phy_pcs_base, ENABLE_ELECIDLE,
				exynos_pcie->ch_num);

	dev_dbg(dev, "D state: %x, %x\n",
		 exynos_pcie->dstate = (exynos_elbi_read(exynos_pcie, PCIE_PM_DSTATE) & 0x7),
		 exynos_elbi_read(exynos_pcie, PCIE_ELBI_RDLH_LINKUP));

	/* assert LTSSM enable */
	exynos_elbi_write(exynos_pcie, PCIE_ELBI_LTSSM_ENABLE,
				PCIE_APP_LTSSM_ENABLE);
	count = 0;
	while (count < MAX_TIMEOUT) {
		val = exynos_elbi_read(exynos_pcie,
					PCIE_ELBI_RDLH_LINKUP) & 0x1f;
		if (val == 0x11)
			break;

		count++;

		udelay(10);
	}

	if (count >= MAX_TIMEOUT) {
		try_cnt++;
		dev_err(dev, "%s: Link is not up, try count: %d, %x\n",
			__func__, try_cnt,
			exynos_elbi_read(exynos_pcie, PCIE_ELBI_RDLH_LINKUP));
		if (try_cnt < 10) {
			gpio_set_value(exynos_pcie->perst_gpio, 0);
			dev_err(dev, "%s: Set PERST to LOW, gpio val = %d\n",
				__func__,
				gpio_get_value(exynos_pcie->perst_gpio));
			/* LTSSM disable */
			exynos_elbi_write(exynos_pcie, PCIE_ELBI_LTSSM_DISABLE,
					  PCIE_APP_LTSSM_ENABLE);
			exynos_pcie_phy_clock_enable(pp, PCIE_DISABLE_CLOCK);
			goto retry;
		} else {
			exynos_pcie_print_link_history(pp);
			exynos_pcie_register_dump(exynos_pcie->ch_num);
#ifdef CONFIG_SEC_PANIC_PCIE_ERR
			panic("[PCIe Case#1] PCIe Link up fail! %s\n", exynos_pcie->ep_device_name);
#endif
			return -EPIPE;
		}
	} else {
		dev_dbg(dev, "%s: Link up:%x\n", __func__,
			 exynos_elbi_read(exynos_pcie, PCIE_ELBI_RDLH_LINKUP));

		exynos_pcie_rd_own_conf(pp, PCIE_LINK_CTRL_STAT, 4, &val);
		val = (val >> 16) & 0xf;
		exynos_pcie->current_speed = val;
		dev_dbg(dev, "Current Link Speed is GEN%d (MAX GEN%d)\n",
				val, exynos_pcie->max_link_speed);

		val = exynos_elbi_read(exynos_pcie, PCIE_IRQ_PULSE);
		exynos_elbi_write(exynos_pcie, val, PCIE_IRQ_PULSE);
		val = exynos_elbi_read(exynos_pcie, PCIE_IRQ_EN_PULSE);
		val |= IRQ_LINKDOWN_ENABLE;
		exynos_elbi_write(exynos_pcie, val, PCIE_IRQ_EN_PULSE);

		/* setup ATU for cfg/mem outbound */
		busdev = EXYNOS_PCIE_ATU_BUS(1) | EXYNOS_PCIE_ATU_DEV(0) | EXYNOS_PCIE_ATU_FUNC(0);
		exynos_pcie_prog_viewport_cfg0(pp, busdev);
		exynos_pcie_prog_viewport_mem_outbound(pp);

	}
	return 0;
}

void exynos_pcie_dislink_work(struct work_struct *work)
{
	struct exynos_pcie *exynos_pcie =
		container_of(work, struct exynos_pcie, dislink_work.work);
	struct dw_pcie *pci = exynos_pcie->pci;
	struct pcie_port *pp = &pci->pp;
	struct device *dev = pci->dev;

	if (exynos_pcie->state == STATE_LINK_DOWN)
		return;

	exynos_pcie->state = STATE_PHY_OPT_OFF;

	exynos_pcie_print_link_history(pp);
	exynos_pcie_dump_link_down_status(exynos_pcie->ch_num);
	exynos_pcie_register_dump(exynos_pcie->ch_num);

	exynos_pcie->state = STATE_LINK_DOWN_TRY;

	if (exynos_pcie->ep_device_type == EP_BCM_WIFI) {
		exynos_pcie->linkdown_cnt++;
		dev_info(dev, "link down and recovery cnt: %d\n",
				exynos_pcie->linkdown_cnt);

#ifdef CONFIG_SEC_PANIC_PCIE_ERR
		panic("[PCIe Case#4] PCIe Link down occurred! %s\n", exynos_pcie->ep_device_name);
#endif

		exynos_pcie_notify_callback(pp, EXYNOS_PCIE_EVENT_LINKDOWN);
	}
}

void exynos_pcie_work_l1ss(struct work_struct *work)
{
	struct exynos_pcie *exynos_pcie =
			container_of(work, struct exynos_pcie,
					l1ss_boot_delay_work.work);

	struct dw_pcie *pci = exynos_pcie->pci;
	struct pcie_port *pp = &pci->pp;
	struct device *dev = pci->dev;


	if (exynos_pcie->work_l1ss_cnt == 0) {
		dev_info(dev, "[%s]boot_cnt: %d, work_l1ss_cnt: %d\n",
				__func__, exynos_pcie->boot_cnt,
				exynos_pcie->work_l1ss_cnt);
		exynos_pcie_set_l1ss(1, pp, PCIE_L1SS_CTRL_BOOT);
		exynos_pcie->work_l1ss_cnt++;
	} else {
		dev_info(dev, "[%s]boot_cnt: %d, work_l1ss_cnt: %d\n",
				__func__, exynos_pcie->boot_cnt,
				exynos_pcie->work_l1ss_cnt);
		return;
	}
}

static void exynos_pcie_assert_phy_reset(struct pcie_port *pp)
{
	struct dw_pcie *pci = to_dw_pcie_from_pp(pp);
	struct exynos_pcie *exynos_pcie = to_exynos_pcie(pci);
	u32 val;
	int ret;

	ret = exynos_pcie_phy_clock_enable(&pci->pp, PCIE_ENABLE_CLOCK);
	dev_dbg(pci->dev, "phy clk enable, ret value = %d\n", ret);

	if (exynos_pcie->phy_ops.phy_config != NULL)
		exynos_pcie->phy_ops.phy_config(exynos_pcie, exynos_pcie->ch_num);

	if (exynos_pcie->use_ia) {
		dev_dbg(pci->dev, "Set I/A for CDR reset\n");
		/* PCIE_SUB_CON setting */
		exynos_elbi_write(exynos_pcie, 0x10, 0x34); /* Enable TOE */
#if IS_ENABLED(CONFIG_SOC_EXYNOS9810)
		/* PCIE_IA SFR Setting */
		exynos_ia_write(exynos_pcie, 0x116a0000, 0x30); /* Base addr0 : ELBI */
		exynos_ia_write(exynos_pcie, 0x116d0000, 0x34); /* Base addr1 : PMA */
		exynos_ia_write(exynos_pcie, 0x116c0000, 0x38); /* Base addr2 : PCS */
		exynos_ia_write(exynos_pcie, 0x11680000, 0x3C); /* Base addr3 : PCIE_IA */
#elif IS_ENABLED(CONFIG_SOC_EXYNOS9820)
		/* PCIE_IA SFR Setting */
		exynos_ia_write(exynos_pcie, 0x13ed0000, 0x30); /* Base addr0 : ELBI */
		exynos_ia_write(exynos_pcie, 0x13ef0000, 0x34); /* Base addr1 : PMA */
		exynos_ia_write(exynos_pcie, 0x13ee0000, 0x38); /* Base addr2 : PCS */
		exynos_ia_write(exynos_pcie, 0x13ec0000, 0x3C); /* Base addr3 : PCIE_IA */
#elif IS_ENABLED(CONFIG_SOC_EXYNOS9830)
		/* PCIE_IA SFR Setting */
		exynos_ia_write(exynos_pcie, 0x133B0000, 0x30); /* Base addr0 : ELBI */
		exynos_ia_write(exynos_pcie, 0x133E0000, 0x34); /* Base addr1 : PMA */
		exynos_ia_write(exynos_pcie, 0x133D0000, 0x38); /* Base addr2 : PCS */
		exynos_ia_write(exynos_pcie, 0x13390000, 0x3C); /* Base addr3 : PCIE_IA */
#elif IS_ENABLED(CONFIG_SOC_EXYNOS991) || IS_ENABLED(CONFIG_PCI_EXYNOS_2100)
		/* PCIE_IA SFR Setting */
		exynos_ia_write(exynos_pcie, 0x113B0000, 0x30); /* Base addr0 : ELBI */
		exynos_ia_write(exynos_pcie, 0x113E0000, 0x34); /* Base addr1 : PMA */
		exynos_ia_write(exynos_pcie, 0x113D0000, 0x38); /* Base addr2 : PCS */
		exynos_ia_write(exynos_pcie, 0x11390000, 0x3C); /* Base addr3 : PCIE_IA */
#endif
		/* IA_CNT_TRG 10 : Repeat 10 */
		exynos_ia_write(exynos_pcie, 0xb, 0x10);
		exynos_ia_write(exynos_pcie, 0x2ffff, 0x40);

		/* L1.2 EXIT Interrupt Happens */
		/* SQ0) UIR_1 : DATA MASK */
		exynos_ia_write(exynos_pcie, 0x50000004, 0x100); /* DATA MASK_REG */
		exynos_ia_write(exynos_pcie, 0xffff, 0x104); /* Enable bit 5 */

		/* SQ1) BRT : Count check and exit*/
		exynos_ia_write(exynos_pcie, 0x20930014, 0x108); /* READ AFC_DONE */
		exynos_ia_write(exynos_pcie, 0xa, 0x10C);

		/* SQ2) Write : Count increment */
		exynos_ia_write(exynos_pcie, 0x40030044, 0x110);
		exynos_ia_write(exynos_pcie, 0x1, 0x114);

		/* SQ3) DATA MASK : Mask AFC_DONE bit*/
		exynos_ia_write(exynos_pcie, 0x50000004, 0x118);
		exynos_ia_write(exynos_pcie, 0x20, 0x11C);

		/* SQ4) BRT : Read and check AFC_DONE */
		exynos_ia_write(exynos_pcie, 0x209101ec, 0x120);
		exynos_ia_write(exynos_pcie, 0x20, 0x124);

		/* SQ5) WRITE : CDR_EN 0xC0 */
		exynos_ia_write(exynos_pcie, 0x400200d0, 0x128);
		exynos_ia_write(exynos_pcie, 0xc0, 0x12C);

		/* SQ6) WRITE : CDR_EN 0x80*/
		exynos_ia_write(exynos_pcie, 0x400200d0, 0x130);
		exynos_ia_write(exynos_pcie, 0x80, 0x134);

		/* SQ7) WRITE : CDR_EN 0x0*/
		exynos_ia_write(exynos_pcie, 0x400200d0, 0x138);
		exynos_ia_write(exynos_pcie, 0x0, 0x13C);

		/* SQ8) LOOP */
		exynos_ia_write(exynos_pcie, 0x100101ec, 0x140);
		exynos_ia_write(exynos_pcie, 0x20, 0x144);

		/* SQ9) Clear L1.2 EXIT Interrupt */
		exynos_ia_write(exynos_pcie, 0x70000004, 0x148);
		exynos_ia_write(exynos_pcie, 0x10, 0x14C);

		/* SQ10) WRITE : IA_CNT Clear*/
		exynos_ia_write(exynos_pcie, 0x40030010, 0x150);
		exynos_ia_write(exynos_pcie, 0x1000b, 0x154);

		/* SQ11) EOP : Return to idle */
		exynos_ia_write(exynos_pcie, 0x80000000, 0x158);
		exynos_ia_write(exynos_pcie, 0x0, 0x15C);

		/* PCIE_IA Enable */
		exynos_ia_write(exynos_pcie, 0x1, 0x0);
	} else {
		dev_info(pci->dev, "Not set I/A for CDR reset\n");
	}

	/* Bus number enable */
	val = exynos_elbi_read(exynos_pcie, PCIE_SW_WAKE);
	val &= ~(0x1 << 1);
	exynos_elbi_write(exynos_pcie, val, PCIE_SW_WAKE);
}

static void exynos_pcie_enable_interrupts(struct pcie_port *pp)
{
	u32 val;
	struct dw_pcie *pci = to_dw_pcie_from_pp(pp);
	struct exynos_pcie *exynos_pcie = to_exynos_pcie(pci);

	/* enable INTX interrupt */
	val = IRQ_INTA_ASSERT | IRQ_INTB_ASSERT |
		IRQ_INTC_ASSERT | IRQ_INTD_ASSERT;
	exynos_elbi_write(exynos_pcie, val, PCIE_IRQ_EN_PULSE);

	/* disable TOE PULSE2 interrupt */
	exynos_elbi_write(exynos_pcie, 0x0, PCIE_IRQ_TOE_EN_PULSE2);
	/* disable TOE LEVEL interrupt */
	exynos_elbi_write(exynos_pcie, 0x0, PCIE_IRQ_TOE_EN_LEVEL);

	/* disable LEVEL interrupt */
	exynos_elbi_write(exynos_pcie, 0x0, PCIE_IRQ_EN_LEVEL);
	/* disable SPECIAL interrupt */
	exynos_elbi_write(exynos_pcie, 0x0, PCIE_IRQ_EN_SPECIAL);
}

/* MSI int handler */
irqreturn_t exynos_v0_handle_msi_irq(struct pcie_port *pp)
{
	int i, pos, irq;
	unsigned long val;
	u32 status, num_ctrls;
	irqreturn_t ret = IRQ_NONE;

	num_ctrls = pp->num_vectors / MAX_MSI_IRQS_PER_CTRL;

	for (i = 0; i < num_ctrls; i++) {
		exynos_pcie_rd_own_conf(pp, PCIE_MSI_INTR0_STATUS +
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

static irqreturn_t exynos_pcie_irq_handler(int irq, void *arg)
{
	struct pcie_port *pp = arg;
	u32 val;
	struct dw_pcie *pci = to_dw_pcie_from_pp(pp);
	struct exynos_pcie *exynos_pcie = to_exynos_pcie(pci);

	/* handle PULSE interrupt */
	val = exynos_elbi_read(exynos_pcie, PCIE_IRQ_PULSE);
	exynos_elbi_write(exynos_pcie, val, PCIE_IRQ_PULSE);

	if (val & IRQ_LINK_DOWN) {
		dev_info(pci->dev, "!!!PCIE LINK DOWN (state : 0x%x)!!!\n", val);
		exynos_pcie->state = STATE_LINK_DOWN_TRY;

		exynos_pcie_print_link_history(pp);

		dev_info(pci->dev, "Current LTSSM: 0x%08x, PHY PLL LOCK: 0x%08x\n",
				exynos_elbi_read(exynos_pcie, PCIE_ELBI_RDLH_LINKUP),
				exynos_phy_read(exynos_pcie, 0xBC));

		if (exynos_pcie->ep_device_type == EP_QC_WIFI) {
			exynos_pcie->linkdown_cnt++;
			dev_info(pci->dev, "link down and recovery cnt: %d\n",
					exynos_pcie->linkdown_cnt);

			exynos_pcie_notify_callback(pp, EXYNOS_PCIE_EVENT_LINKDOWN);
		}

		queue_work(exynos_pcie->pcie_wq,
				&exynos_pcie->dislink_work.work);
	}
#if IS_ENABLED(CONFIG_PCI_MSI)
	if (val & IRQ_MSI_RISING_ASSERT && exynos_pcie->use_msi) {
		exynos_v0_handle_msi_irq(pp);

		/* Mask & Clear MSI to pend MSI interrupt.
		 * After clearing IRQ_PULSE, MSI interrupt can be ignored if
		 * lower MSI status bit is set while processing upper bit.
		 * Through the Mask/Unmask, ignored interrupts will be pended.
		 */

		exynos_pcie_rd_own_conf(pp, PCIE_MSI_INTR0_MASK, 4, &val);
		exynos_pcie_wr_own_conf(pp, PCIE_MSI_INTR0_MASK, 4, 0xffffffff);
		exynos_pcie_wr_own_conf(pp, PCIE_MSI_INTR0_MASK, 4, val);
	}
#endif

	/* handle LEVEL interrupt */
	val = exynos_elbi_read(exynos_pcie, PCIE_IRQ_LEVEL);
	exynos_elbi_write(exynos_pcie, val, PCIE_IRQ_LEVEL);

	/* handle SPECIAL interrupt */
	val = exynos_elbi_read(exynos_pcie, PCIE_IRQ_SPECIAL);
	exynos_elbi_write(exynos_pcie, val, PCIE_IRQ_SPECIAL);

	return IRQ_HANDLED;
}

#if IS_ENABLED(CONFIG_PCI_MSI)
static void exynos_pcie_msi_init(struct pcie_port *pp)
{
	u32 val;
	u64 msi_target = 0;
	struct dw_pcie *pci = to_dw_pcie_from_pp(pp);
	struct exynos_pcie *exynos_pcie = to_exynos_pcie(pci);
	unsigned long msi_addr_from_dt = 0;

	if (!exynos_pcie->use_msi)
		return;

	if (exynos_pcie->ep_device_type == EP_QC_WIFI) {
		msi_addr_from_dt = get_msi_base(exynos_pcie->ch_num);

		pp->msi_data = msi_addr_from_dt;

		pr_info("%s: msi_addr_from_dt: 0x%llx, pp->msi_data: 0x%llx\n",
				__func__, msi_addr_from_dt, pp->msi_data);

		/* program the msi_data */
		exynos_pcie_wr_own_conf(pp, PCIE_MSI_ADDR_LO, 4,
				lower_32_bits(pp->msi_data));
		exynos_pcie_wr_own_conf(pp, PCIE_MSI_ADDR_HI, 4,
				upper_32_bits(pp->msi_data));

		/* enable MSI interrupt */
		val = exynos_elbi_read(exynos_pcie, PCIE_IRQ_EN_PULSE);
		val |= IRQ_MSI_CTRL_EN_RISING_EDG;
		exynos_elbi_write(exynos_pcie, val, PCIE_IRQ_EN_PULSE);

		/* Enable MSI interrupt after PCIe reset */
		val = (u32)(*pp->msi_irq_in_use);
		exynos_pcie_wr_own_conf(pp, PCIE_MSI_INTR0_ENABLE, 4, val);
		exynos_pcie_rd_own_conf(pp, PCIE_MSI_INTR0_ENABLE, 4, &val);
		pr_err("MSI INIT check INTR0 ENABLE, 0x%x: 0x%x \n", PCIE_MSI_INTR0_ENABLE, val);
		exynos_pcie_wr_own_conf(pp, PCIE_MSI_INTR0_ENABLE, 4, 0xffff0001);
		exynos_pcie_rd_own_conf(pp, PCIE_MSI_INTR0_ENABLE, 4, &val);
		pr_err("%s: MSI INIT END, 0x%x: 0x%x \n", __func__, PCIE_MSI_INTR0_ENABLE, val);
	} else {
		/*
		 * dw_pcie_msi_init() function allocates msi_data.
		 * The following code is added to avoid duplicated allocation.
		 */
		msi_target = virt_to_phys((void *)pp->msi_data);

		/* program the msi_data */
		exynos_pcie_wr_own_conf(pp, PCIE_MSI_ADDR_LO, 4,
				(u32)(msi_target & 0xffffffff));
		exynos_pcie_wr_own_conf(pp, PCIE_MSI_ADDR_HI, 4,
				(u32)(msi_target >> 32 & 0xffffffff));

		/* enable MSI interrupt */
		val = exynos_elbi_read(exynos_pcie, PCIE_IRQ_EN_PULSE);
		val |= IRQ_MSI_CTRL_EN_RISING_EDG;
		exynos_elbi_write(exynos_pcie, val, PCIE_IRQ_EN_PULSE);

		/* Enable MSI interrupt after PCIe reset */
		val = (u32)(*pp->msi_irq_in_use);
		exynos_pcie_wr_own_conf(pp, PCIE_MSI_INTR0_ENABLE, 4, val);
	}
}
#endif

static u32 exynos_pcie_read_dbi(struct dw_pcie *pci, void __iomem *base,
				u32 reg, size_t size)
{
	struct pcie_port *pp = &pci->pp;
	u32 val;
	exynos_pcie_rd_own_conf(pp, reg, size, &val);
	return val;
}

static void exynos_pcie_write_dbi(struct dw_pcie *pci, void __iomem *base,
				  u32 reg, size_t size, u32 val)
{
	struct pcie_port *pp = &pci->pp;

	exynos_pcie_wr_own_conf(pp, reg, size, val);
}

static int exynos_pcie_rd_own_conf(struct pcie_port *pp, int where, int size,
				u32 *val)
{
	struct dw_pcie *pci = to_dw_pcie_from_pp(pp);
	struct exynos_pcie *exynos_pcie = to_exynos_pcie(pci);
	int is_linked = 0;
	int ret = 0;
	u32 __maybe_unused reg_val;
	unsigned long flags;

	spin_lock_irqsave(&exynos_pcie->reg_lock, flags);

#if IS_ENABLED(CONFIG_DYNAMIC_PHY_OFF)
	ret = regmap_read(exynos_pcie->pmureg,
			exynos_pcie->pmu_offset,
			&reg_val);
	if (reg_val == 0 || ret != 0) {
		dev_info(pci->dev, "PCIe PHY is disabled...\n");
		*val = 0xffffffff;
		return PCIBIOS_DEVICE_NOT_FOUND;
	}
#endif
	if (exynos_pcie->state == STATE_LINK_UP ||
			exynos_pcie->state == STATE_LINK_UP_TRY)
		is_linked = 1;

	if (is_linked == 0) {
		/* TCXO_FAR on */
		hsi_tcxo_far_control(1, 1);

		exynos_pcie_clock_enable(pp, PCIE_ENABLE_CLOCK);
		exynos_pcie_phy_clock_enable(&pci->pp,
				PCIE_ENABLE_CLOCK);
		/* force enable Lane0 */
		reg_val = exynos_sysreg_read(exynos_pcie,
				PCIE_WIFI0_PCIE_PHY_CONTROL);
		reg_val |= PCS_LANE0_ENABLE;
		exynos_sysreg_write(exynos_pcie, reg_val,
				PCIE_WIFI0_PCIE_PHY_CONTROL);
		udelay(100);
#ifdef NCLK_OFF_CONTROL
		if (exynos_pcie->ip_ver >= 0x981000)
			exynos_elbi_write(exynos_pcie, 0x0, PCIE_L12ERR_CTRL);
#endif

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

#ifdef NCLK_OFF_CONTROL
		if (exynos_pcie->ip_ver >= 0x981000)
			exynos_elbi_write(exynos_pcie, (0x1 << NCLK_OFF_OFFSET),
							PCIE_L12ERR_CTRL);
#endif
		if (exynos_pcie->state != STATE_PHY_OPT_OFF) {
			/* force disable Lane0 */
			udelay(100);
			reg_val = exynos_sysreg_read(exynos_pcie,
					PCIE_WIFI0_PCIE_PHY_CONTROL);
			reg_val &= ~(PCS_LANE0_ENABLE);
			exynos_sysreg_write(exynos_pcie, reg_val,
					PCIE_WIFI0_PCIE_PHY_CONTROL);
			exynos_pcie_phy_clock_enable(pp, PCIE_DISABLE_CLOCK);
			exynos_pcie_clock_enable(pp, PCIE_DISABLE_CLOCK);

			/* TCXO_FAR off */
			hsi_tcxo_far_control(1, 0);
		}
	}

	spin_unlock_irqrestore(&exynos_pcie->reg_lock, flags);

	return ret;
}

static int exynos_pcie_wr_own_conf(struct pcie_port *pp, int where, int size,
				u32 val)
{
	struct dw_pcie *pci = to_dw_pcie_from_pp(pp);
	struct exynos_pcie *exynos_pcie = to_exynos_pcie(pci);
	int is_linked = 0;
	int ret = 0;
	u32 __maybe_unused reg_val;
	unsigned long flags;

	spin_lock_irqsave(&exynos_pcie->reg_lock, flags);

#if IS_ENABLED(CONFIG_DYNAMIC_PHY_OFF)
	ret = regmap_read(exynos_pcie->pmureg,
			exynos_pcie->pmu_offset,
			&reg_val);
	if (reg_val == 0 || ret != 0) {
		dev_info(pci->dev, "PCIe PHY is disabled...\n");
		return PCIBIOS_DEVICE_NOT_FOUND;
	}
#endif
	if (exynos_pcie->state == STATE_LINK_UP ||
			exynos_pcie->state == STATE_LINK_UP_TRY)
		is_linked = 1;

	if (is_linked == 0) {
		/* TCXO_FAR on */
		hsi_tcxo_far_control(1, 1);

		exynos_pcie_clock_enable(pp, PCIE_ENABLE_CLOCK);
		/*exynos_pcie_phy_clock_enable(&exynos_pcie->pp,*/
		exynos_pcie_phy_clock_enable(&pci->pp,
				PCIE_ENABLE_CLOCK);
		/* force enable Lane0 */
		reg_val = exynos_sysreg_read(exynos_pcie,
				PCIE_WIFI0_PCIE_PHY_CONTROL);
		reg_val |= PCS_LANE0_ENABLE;
		exynos_sysreg_write(exynos_pcie, reg_val,
				PCIE_WIFI0_PCIE_PHY_CONTROL);
		udelay(100);
#ifdef NCLK_OFF_CONTROL
		if (exynos_pcie->ip_ver >= 0x981000)
			exynos_elbi_write(exynos_pcie, 0x0, PCIE_L12ERR_CTRL);
#endif

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

#ifdef NCLK_OFF_CONTROL
		if (exynos_pcie->ip_ver >= 0x981000)
			exynos_elbi_write(exynos_pcie, (0x1 << NCLK_OFF_OFFSET),
							PCIE_L12ERR_CTRL);
#endif
		if (exynos_pcie->state != STATE_PHY_OPT_OFF) {
			/* force disable Lane0 */
			udelay(100);
			reg_val = exynos_sysreg_read(exynos_pcie,
					PCIE_WIFI0_PCIE_PHY_CONTROL);
			reg_val &= ~(PCS_LANE0_ENABLE);
			exynos_sysreg_write(exynos_pcie, reg_val,
					PCIE_WIFI0_PCIE_PHY_CONTROL);
			exynos_pcie_phy_clock_enable(pp, PCIE_DISABLE_CLOCK);
			exynos_pcie_clock_enable(pp, PCIE_DISABLE_CLOCK);

			/* TCXO_FAR off */
			hsi_tcxo_far_control(1, 0);
		}
	}

	spin_unlock_irqrestore(&exynos_pcie->reg_lock, flags);

	return ret;
}

static int exynos_pcie_rd_other_conf(struct pcie_port *pp,
		struct pci_bus *bus, u32 devfn, int where, int size, u32 *val)
{
	int ret, type;
	u32 busdev, cfg_size;
	u64 cpu_addr;
	void __iomem *va_cfg_base;
	struct dw_pcie *pci = to_dw_pcie_from_pp(pp);
	struct exynos_pcie *exynos_pcie = to_exynos_pcie(pci);
	u32 reg_val;
	int is_linked = 0;

	if (exynos_pcie->state == STATE_LINK_UP)
		is_linked = 1;

	if (is_linked == 0) {
		/* tcxo_far on */
		hsi_tcxo_far_control(1, 1);

		/* force enable Lane0 */
		reg_val = exynos_sysreg_read(exynos_pcie,
				PCIE_WIFI0_PCIE_PHY_CONTROL);
		reg_val |= PCS_LANE0_ENABLE;
		exynos_sysreg_write(exynos_pcie, reg_val,
				PCIE_WIFI0_PCIE_PHY_CONTROL);
		udelay(100);
	}
	busdev = EXYNOS_PCIE_ATU_BUS(bus->number) | EXYNOS_PCIE_ATU_DEV(PCI_SLOT(devfn)) |
		 EXYNOS_PCIE_ATU_FUNC(PCI_FUNC(devfn));
	if (bus->parent->number == pp->root_bus_nr) {
		type = EXYNOS_PCIE_ATU_TYPE_CFG0;
		cpu_addr = pp->cfg0_base;
		cfg_size = pp->cfg0_size;
		va_cfg_base = pp->va_cfg0_base;
		/* setup ATU for cfg/mem outbound */
		exynos_pcie_prog_viewport_cfg0(pp, busdev);
	} else {
		type = EXYNOS_PCIE_ATU_TYPE_CFG1;
		cpu_addr = pp->cfg1_base;
		cfg_size = pp->cfg1_size;
		va_cfg_base = pp->va_cfg1_base;
		/* setup ATU for cfg/mem outbound */
		exynos_pcie_prog_viewport_cfg1(pp, busdev);
	}
	ret = dw_pcie_read(va_cfg_base + where, size, val);
	if (is_linked == 0) {
		if (exynos_pcie->state != STATE_PHY_OPT_OFF) {
			/* force disable Lane0 */
			udelay(100);
			reg_val = exynos_sysreg_read(exynos_pcie,
					PCIE_WIFI0_PCIE_PHY_CONTROL);
			reg_val &= ~(PCS_LANE0_ENABLE);
			exynos_sysreg_write(exynos_pcie, reg_val,
					PCIE_WIFI0_PCIE_PHY_CONTROL);

			/* tcxo_far off */
			hsi_tcxo_far_control(1, 0);
		}
	}

	return ret;
}

static int exynos_pcie_wr_other_conf(struct pcie_port *pp,
		struct pci_bus *bus, u32 devfn, int where, int size, u32 val)
{

	int ret, type;
	u32 busdev, cfg_size;
	u64 cpu_addr;
	void __iomem *va_cfg_base;
	struct dw_pcie *pci = to_dw_pcie_from_pp(pp);
	struct exynos_pcie *exynos_pcie = to_exynos_pcie(pci);
	u32 reg_val;
	int is_linked = 0;

	if (exynos_pcie->state == STATE_LINK_UP)
		is_linked = 1;

	if (is_linked == 0) {
		/* tcxo_far on */
		hsi_tcxo_far_control(1, 1);

		/* force enable Lane0 */
		reg_val = exynos_sysreg_read(exynos_pcie,
				PCIE_WIFI0_PCIE_PHY_CONTROL);
		reg_val |= PCS_LANE0_ENABLE;
		exynos_sysreg_write(exynos_pcie, reg_val,
				PCIE_WIFI0_PCIE_PHY_CONTROL);
		udelay(100);
	}
	busdev = EXYNOS_PCIE_ATU_BUS(bus->number) | EXYNOS_PCIE_ATU_DEV(PCI_SLOT(devfn)) |
		 EXYNOS_PCIE_ATU_FUNC(PCI_FUNC(devfn));

	if (bus->parent->number == pp->root_bus_nr) {
		type = EXYNOS_PCIE_ATU_TYPE_CFG0;
		cpu_addr = pp->cfg0_base;
		cfg_size = pp->cfg0_size;
		va_cfg_base = pp->va_cfg0_base;
		/* setup ATU for cfg/mem outbound */
		exynos_pcie_prog_viewport_cfg0(pp, busdev);
	} else {
		type = EXYNOS_PCIE_ATU_TYPE_CFG1;
		cpu_addr = pp->cfg1_base;
		cfg_size = pp->cfg1_size;
		va_cfg_base = pp->va_cfg1_base;
		/* setup ATU for cfg/mem outbound */
		exynos_pcie_prog_viewport_cfg1(pp, busdev);
	}

	ret = dw_pcie_write(va_cfg_base + where, size, val);
	if (is_linked == 0) {
		if (exynos_pcie->state != STATE_PHY_OPT_OFF) {
			/* force disable Lane0 */
			udelay(100);
			reg_val = exynos_sysreg_read(exynos_pcie,
					PCIE_WIFI0_PCIE_PHY_CONTROL);
			reg_val &= ~(PCS_LANE0_ENABLE);
			exynos_sysreg_write(exynos_pcie, reg_val,
					PCIE_WIFI0_PCIE_PHY_CONTROL);

			/* tcxo_far off */
			hsi_tcxo_far_control(1, 0);
		}
	}

	return ret;
}

static int exynos_pcie_link_up(struct dw_pcie *pci)
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

static int exynos_pcie_host_init(struct pcie_port *pp)
{
	/* Setup RC to avoid initialization faile in PCIe stack */
	dw_pcie_setup_rc(pp);
	return 0;
}

static struct dw_pcie_host_ops exynos_pcie_host_ops = {
	.rd_own_conf = exynos_pcie_rd_own_conf,
	.wr_own_conf = exynos_pcie_wr_own_conf,
	.rd_other_conf = exynos_pcie_rd_other_conf,
	.wr_other_conf = exynos_pcie_wr_other_conf,
	.host_init = exynos_pcie_host_init,
};

static int exynos_pcie_hostv0_msi_set_affinity(struct irq_data *irq_data,
					const struct cpumask *mask, bool force)
{
	return 0;
}

static int add_pcie_port(struct pcie_port *pp,
				struct platform_device *pdev)
{
	struct dw_pcie *pci = to_dw_pcie_from_pp(pp);
	struct exynos_pcie *exynos_pcie = to_exynos_pcie(pci);
	struct irq_domain *msi_domain;
	struct msi_domain_info *msi_domain_info;
	int ret;
	u32 val, vendor_id, device_id;
	u64 __maybe_unused msi_target;
#if IS_ENABLED(CONFIG_PCI_MSI)
		unsigned long msi_addr_from_dt = 0;
#endif

	pp->irq = platform_get_irq(pdev, 0);
	if (!pp->irq) {
		dev_err(&pdev->dev, "failed to get irq\n");
		return -ENODEV;
	}
	ret = devm_request_irq(&pdev->dev, pp->irq, exynos_pcie_irq_handler,
				IRQF_SHARED | IRQF_TRIGGER_HIGH, "exynos-pcie", pp);
	if (ret) {
		dev_err(&pdev->dev, "failed to request irq\n");
		return ret;
	}

	pp->root_bus_nr = 0;
	pp->ops = &exynos_pcie_host_ops;

#if IS_ENABLED(CONFIG_EXYNOS_PCIE_GEN2_S2MPU) //please check to insert s2mpu_lock init(pcie probe)
	spin_lock_init(&s2mpu_lock);
#endif

	spin_lock_init(&exynos_pcie->pcie_l1_exit_lock);
	spin_lock_init(&exynos_pcie->reg_lock);
	exynos_pcie_setup_rc(pp);
	spin_lock_init(&exynos_pcie->conf_lock);
	ret = dw_pcie_host_init(pp);

	exynos_pcie_rd_own_conf(pp, PCI_VENDOR_ID, 4, &val);
	vendor_id = val & ID_MASK;
	device_id = (val >> 16) & ID_MASK;
	exynos_pcie->pci_dev = pci_get_device(vendor_id,
			device_id, NULL);
	if (!exynos_pcie->pci_dev) {
		dev_err(pci->dev, "Failed to get pci device\n");
	}

	exynos_pcie->pci_dev->dma_mask = DMA_BIT_MASK(36);
	exynos_pcie->pci_dev->dev.coherent_dma_mask = DMA_BIT_MASK(36);
	dma_set_seg_boundary(&(exynos_pcie->pci_dev->dev), DMA_BIT_MASK(36));

#if IS_ENABLED(CONFIG_PCI_MSI)
	if (exynos_pcie->use_msi) {
		dw_pcie_msi_init(pp);

		if (exynos_pcie->ep_device_type == EP_QC_WIFI) {
			msi_addr_from_dt = get_msi_base(exynos_pcie->ch_num);

			pp->msi_data = msi_addr_from_dt;

			pr_info("%s: msi_addr_from_dt: 0x%llx, pp->msi_data: 0x%llx\n", __func__,
					msi_addr_from_dt, pp->msi_data);

			exynos_pcie_wr_own_conf(pp, PCIE_MSI_ADDR_LO, 4,
					lower_32_bits(pp->msi_data));
			exynos_pcie_wr_own_conf(pp, PCIE_MSI_ADDR_HI, 4,
					upper_32_bits(pp->msi_data));
		} else {
#ifdef MODIFY_MSI_ADDR
			/* Change MSI address to fit within a 32bit address boundary */
			free_page(pp->msi_data);

			pp->msi_data = (u64)kmalloc(4, GFP_DMA);
			msi_target = virt_to_phys((void *)pp->msi_data);

			if ((msi_target >> 32) != 0)
				dev_info(&pdev->dev,
						"MSI memory is allocated over 32bit boundary\n");

			/* program the msi_data */
			exynos_pcie_wr_own_conf(pp, PCIE_MSI_ADDR_LO, 4,
					(u32)(msi_target & 0xffffffff));
			exynos_pcie_wr_own_conf(pp, PCIE_MSI_ADDR_HI, 4,
					(u32)(msi_target >> 32 & 0xffffffff));
#endif
		}
	}
#endif
	if (ret) {
		dev_err(&pdev->dev, "failed to initialize host\n");
		return ret;
	}

	if (pp->msi_domain) {
		msi_domain = pp->msi_domain;
		msi_domain_info = (struct msi_domain_info *)msi_domain->host_data;
		msi_domain_info->chip->irq_set_affinity = exynos_pcie_hostv0_msi_set_affinity;
	}

	dev_dbg(&pdev->dev, "MSI Message Address : 0x%llx\n",
			virt_to_phys((void *)pp->msi_data));

	return 0;
}

static void enable_cache_cohernecy(struct pcie_port *pp, int enable)
{
	int set_val;
	struct dw_pcie *pci = to_dw_pcie_from_pp(pp);
	struct exynos_pcie *exynos_pcie = to_exynos_pcie(pci);

	if (exynos_pcie->ip_ver > 0x889500) {
		/* Set enable value */
		if (enable) {
			dev_info(pci->dev, "Enable cache coherency.\n");
			set_val = PCIE_SYSREG_SHARABLE_ENABLE;
		} else {
			dev_info(pci->dev, "Disable cache coherency.\n");
			set_val = PCIE_SYSREG_SHARABLE_DISABLE;
		}

		/* Set configurations */
		regmap_update_bits(exynos_pcie->sysreg,
			PCIE_SYSREG_SHARABILITY_CTRL, /* SYSREG address */
			(0x3 << (PCIE_SYSREG_SHARABLE_OFFSET
				+ exynos_pcie->ch_num * 4)), /* Mask */
			(set_val << (PCIE_SYSREG_SHARABLE_OFFSET
				     + exynos_pcie->ch_num * 4))); /* Set val */
	}
}
static int exynos_pcie_parse_dt(struct device_node *np, struct pcie_port *pp)
{
	struct dw_pcie *pci = to_dw_pcie_from_pp(pp);
	struct exynos_pcie *exynos_pcie = to_exynos_pcie(pci);
	const char *use_cache_coherency;
	const char *use_msi;
	const char *use_sicd;
	const char *use_sysmmu;
	const char *use_ia;

	if (of_property_read_u32(np, "ip-ver",
					&exynos_pcie->ip_ver)) {
		dev_err(pci->dev, "Failed to parse the number of ip-ver\n");
		return -EINVAL;
	}

	if (of_property_read_u32(np, "ep-ver",
				&exynos_pcie->ep_ver)) {
		dev_err(pci->dev, "Failed to parse the number of ep-ver\n");
		return -EINVAL;
	}

	if (of_property_read_u32(np, "pcie-clk-num",
					&exynos_pcie->pcie_clk_num)) {
		dev_err(pci->dev, "Failed to parse the number of pcie clock\n");
		return -EINVAL;
	}

	if (of_property_read_u32(np, "phy-clk-num",
					&exynos_pcie->phy_clk_num)) {
		dev_err(pci->dev, "Failed to parse the number of phy clock\n");
		return -EINVAL;
	}

	if (of_property_read_u32(np, "pmu-offset",
					&exynos_pcie->pmu_offset)) {
		dev_err(pci->dev, "Failed to parse the number of linkup-offset\n");
		return -EINVAL;
	}

	if (of_property_read_u32(np, "ep-device-type",
				&exynos_pcie->ep_device_type)) {
		dev_err(pci->dev, "EP device type is NOT defined...(WIFI?)\n");
		/* Default EP device is BCM WIFI */
		exynos_pcie->ep_device_type = EP_BCM_WIFI;
	}

	if (of_property_read_u32(np, "max-link-speed",
				&exynos_pcie->max_link_speed)) {
		dev_err(pci->dev, "MAX Link Speed is NOT defined...(GEN1)\n");
		/* Default Link Speed is GEN1 */
		exynos_pcie->max_link_speed = LINK_SPEED_GEN1;
	}

	if (!of_property_read_string(np, "use-cache-coherency",
						&use_cache_coherency)) {
		if (!strcmp(use_cache_coherency, "true")) {
			dev_info(pci->dev, "Cache Coherency unit is ENABLED.\n");
			exynos_pcie->use_cache_coherency = true;
		} else if (!strcmp(use_cache_coherency, "false")) {
			exynos_pcie->use_cache_coherency = false;
		} else {
			dev_err(pci->dev, "Invalid use-cache-coherency value"
					"(Set to default -> false)\n");
			exynos_pcie->use_cache_coherency = false;
		}
	} else {
		exynos_pcie->use_cache_coherency = false;
	}

#ifdef CONFIG_SEC_PANIC_PCIE_ERR
	if (!of_property_read_string(np, "ep-device-name", (const char**)&exynos_pcie->ep_device_name)) {
		dev_info(pci->dev, "EP device name is %s\n", exynos_pcie->ep_device_name);
	} else {
		dev_err(pci->dev, "EP device name is NOT defined, device node name is %s\n", np->name);
		exynos_pcie->ep_device_name = np->name;
	}
#endif

	if (!of_property_read_string(np, "use-msi", &use_msi)) {
		if (!strcmp(use_msi, "true")) {
			exynos_pcie->use_msi = true;
		} else if (!strcmp(use_msi, "false")) {
			exynos_pcie->use_msi = NULL;
			dev_err(pci->dev, "Don'use MSI\n");
		} else {
			dev_err(pci->dev, "Invalid use-msi value"
					"(Set to default -> true)\n");
			exynos_pcie->use_msi = true;
		}
	} else {
		exynos_pcie->use_msi = false;
	}

	if (!of_property_read_string(np, "use-sicd", &use_sicd)) {
		if (!strcmp(use_sicd, "true")) {
			exynos_pcie->use_sicd = true;
			dev_info(pci->dev, "PCIe use SICD\n");
		} else if (!strcmp(use_sicd, "false")) {
			exynos_pcie->use_sicd = false;
			dev_info(pci->dev, "PCIe don't use SICD\n");
		} else {
			dev_err(pci->dev, "Invalid use-sicd value"
				       "(set to default -> false)\n");
			exynos_pcie->use_sicd = false;
		}
	} else {
		exynos_pcie->use_sicd = false;
	}

	if (!of_property_read_string(np, "use-sysmmu", &use_sysmmu)) {
		if (!strcmp(use_sysmmu, "true")) {
			dev_info(pci->dev, "PCIe SysMMU is ENABLED.\n");
			exynos_pcie->use_sysmmu = true;
		} else if (!strcmp(use_sysmmu, "false")) {
			dev_info(pci->dev, "PCIe SysMMU is DISABLED.\n");
			exynos_pcie->use_sysmmu = false;
		} else {
			dev_err(pci->dev, "Invalid use-sysmmu value"
				       "(set to default -> false)\n");
			exynos_pcie->use_sysmmu = false;
		}
	} else {
		exynos_pcie->use_sysmmu = false;
	}

	if (!of_property_read_string(np, "use-ia", &use_ia)) {
		if (!strcmp(use_ia, "true")) {
			dev_info(pci->dev, "PCIe I/A is ENABLED.\n");
			exynos_pcie->use_ia = true;
		} else if (!strcmp(use_ia, "false")) {
			dev_info(pci->dev, "PCIe I/A is DISABLED.\n");
			exynos_pcie->use_ia = false;
		} else {
			dev_err(pci->dev, "Invalid use-ia value"
				       "(set to default -> false)\n");
			exynos_pcie->use_ia = false;
		}
	} else {
		exynos_pcie->use_ia = false;
	}

#if IS_ENABLED(CONFIG_EXYNOS_PM_QOS)
	if (of_property_read_u32(np, "pcie-pm-qos-int",
					&exynos_pcie->int_min_lock))
		exynos_pcie->int_min_lock = 0;
	if (exynos_pcie->int_min_lock)
		exynos_pm_qos_add_request(&exynos_pcie_int_qos[exynos_pcie->ch_num],
				PM_QOS_DEVICE_THROUGHPUT, 0);
	dev_info(pci->dev, "%s: pcie int_min_lock = %d\n",
			__func__, exynos_pcie->int_min_lock);
#endif
	exynos_pcie->pmureg = syscon_regmap_lookup_by_phandle(np,
					"samsung,syscon-phandle");
	if (IS_ERR(exynos_pcie->pmureg)) {
		dev_err(pci->dev, "syscon regmap lookup failed.\n");
		return PTR_ERR(exynos_pcie->pmureg);
	}

	exynos_pcie->sysreg = syscon_regmap_lookup_by_phandle(np,
			"samsung,sysreg-phandle");
	/* Check definitions to access SYSREG in DT*/
	if (IS_ERR(exynos_pcie->sysreg) && IS_ERR(exynos_pcie->sysreg_base)) {
		dev_err(pci->dev, "SYSREG is not defined.\n");
		return PTR_ERR(exynos_pcie->sysreg);
	}

	exynos_pcie->wlan_gpio = of_get_named_gpio(np, "pcie,wlan-gpio", 0);
	if (exynos_pcie->wlan_gpio < 0) {
		dev_err(pci->dev, "wlan gpio is not defined -> don't use wifi through pcie#%d\n",
				exynos_pcie->ch_num);
	} else {
		gpio_direction_output(exynos_pcie->wlan_gpio, 0);
	}

	return 0;
}

static const struct dw_pcie_ops dw_pcie_ops = {
	.read_dbi = exynos_pcie_read_dbi,
	.write_dbi = exynos_pcie_write_dbi,
        .link_up = exynos_pcie_link_up,
};

static void exynos_pcie_ops_init(struct pcie_port *pp)
{
	struct dw_pcie *dw_pcie = to_dw_pcie_from_pp(pp);
	struct exynos_pcie *exynos_pcie = to_exynos_pcie(dw_pcie);
	struct exynos_pcie_ops *pcie_ops = &exynos_pcie->exynos_pcie_ops;
	struct device *dev = dw_pcie->dev;

	dev_info(dev, "Initialize PCIe function.\n");

	pcie_ops->poweron = exynos_pcie_poweron;
	pcie_ops->poweroff = exynos_pcie_poweroff;
	pcie_ops->rd_own_conf = exynos_pcie_rd_own_conf;
	pcie_ops->wr_own_conf = exynos_pcie_wr_own_conf;
	pcie_ops->rd_other_conf = exynos_pcie_rd_other_conf;
	pcie_ops->wr_other_conf = exynos_pcie_wr_other_conf;
}

static int exynos_pcie_make_reg_tb(struct device *dev, struct exynos_pcie *exynos_pcie)
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

static int exynos_pcie_probe(struct platform_device *pdev)
{
	struct exynos_pcie *exynos_pcie;
	struct device *dev = &pdev->dev;
	struct dw_pcie *pci;
	struct pcie_port *pp;
	struct device_node *np = pdev->dev.of_node;
	struct resource *temp_rsc;
	int ret = 0;
	int ch_num;
	u32 val;

	dev_info(&pdev->dev, "## PCIE0 PROBE start\n");

	if (create_pcie_sys_file(&pdev->dev))
		dev_err(&pdev->dev, "Failed to create pcie sys file\n");

	if (of_property_read_u32(np, "ch-num", &ch_num)) {
		dev_err(&pdev->dev, "Failed to parse the channel number\n");
		return -EINVAL;
	}

	pci = devm_kzalloc(dev, sizeof(*pci), GFP_KERNEL);
	if (!pci)
		return -ENOMEM;

	exynos_pcie = &g_pcie[ch_num];

	exynos_pcie->pci = pci;
	pci->dev = dev;
	pci->ops = &dw_pcie_ops;
	pp = &pci->pp;

	exynos_pcie->ch_num = ch_num;
	exynos_pcie->l1ss_enable = 1;
	exynos_pcie->state = STATE_LINK_DOWN;

	exynos_pcie->linkdown_cnt = 0;
	exynos_pcie->boot_cnt = 0;
	exynos_pcie->work_l1ss_cnt = 0;
	exynos_pcie->l1ss_ctrl_id_state = 0;
	exynos_pcie->atu_ok = 0;
	exynos_pcie->shutdown = 0;

	exynos_pcie->app_req_exit_l1 = PCIE_APP_REQ_EXIT_L1;
	exynos_pcie->app_req_exit_l1_mode = PCIE_APP_REQ_EXIT_L1_MODE;
	exynos_pcie->linkup_offset = PCIE_ELBI_RDLH_LINKUP;

	dma_set_mask_and_coherent(&pdev->dev, DMA_BIT_MASK(36));
	platform_set_drvdata(pdev, exynos_pcie);

	/* parsing pcie dts data for exynos */
	ret = exynos_pcie_parse_dt(pdev->dev.of_node, pp);
	if (ret)
		goto probe_fail;
	exynos_pcie->perst_gpio = of_get_gpio(np, 0);
	if (exynos_pcie->perst_gpio < 0) {
		dev_err(&pdev->dev, "cannot get perst_gpio\n");
	} else {
		ret = devm_gpio_request_one(pci->dev, exynos_pcie->perst_gpio,
					    GPIOF_OUT_INIT_LOW,
					    dev_name(pci->dev));
		if (ret)
			goto probe_fail;
	}
	/* Get pin state */
	exynos_pcie->pcie_pinctrl = devm_pinctrl_get(&pdev->dev);
	if (IS_ERR(exynos_pcie->pcie_pinctrl)) {
		dev_err(&pdev->dev, "Can't get pcie pinctrl!!!\n");
		goto probe_fail;
	}
	exynos_pcie->pin_state[PCIE_PIN_ACTIVE] =
		pinctrl_lookup_state(exynos_pcie->pcie_pinctrl, "active");
	if (IS_ERR(exynos_pcie->pin_state[PCIE_PIN_ACTIVE])) {
		dev_err(&pdev->dev, "Can't set pcie clkerq to output high!\n");
		goto probe_fail;
	}
	exynos_pcie->pin_state[PCIE_PIN_IDLE] =
		pinctrl_lookup_state(exynos_pcie->pcie_pinctrl, "idle");
	if (IS_ERR(exynos_pcie->pin_state[PCIE_PIN_IDLE]))
		dev_err(&pdev->dev, "No idle pin state(but it's OK)!!\n");
	else
		pinctrl_select_state(exynos_pcie->pcie_pinctrl,
				exynos_pcie->pin_state[PCIE_PIN_IDLE]);
	ret = exynos_pcie_clock_get(pp);
	if (ret)
		goto probe_fail;
	temp_rsc = platform_get_resource_byname(pdev, IORESOURCE_MEM, "elbi");
	exynos_pcie->elbi_base = devm_ioremap_resource(&pdev->dev, temp_rsc);
	if (IS_ERR(exynos_pcie->elbi_base)) {
		ret = PTR_ERR(exynos_pcie->elbi_base);
		goto probe_fail;
	}
	temp_rsc = platform_get_resource_byname(pdev, IORESOURCE_MEM, "phy");
	exynos_pcie->phy_base = devm_ioremap_resource(&pdev->dev, temp_rsc);
	if (IS_ERR(exynos_pcie->phy_base)) {
		ret = PTR_ERR(exynos_pcie->phy_base);
		goto probe_fail;
	}

	temp_rsc = platform_get_resource_byname(pdev, IORESOURCE_MEM, "sysreg");
	exynos_pcie->sysreg_base = devm_ioremap_resource(&pdev->dev, temp_rsc);
	if (IS_ERR(exynos_pcie->sysreg_base)) {
		ret = PTR_ERR(exynos_pcie->sysreg_base);
		goto probe_fail;
	}

	temp_rsc = platform_get_resource_byname(pdev, IORESOURCE_MEM, "dbi");
	exynos_pcie->rc_dbi_base = devm_ioremap_resource(&pdev->dev, temp_rsc);
	if (IS_ERR(exynos_pcie->rc_dbi_base)) {
		ret = PTR_ERR(exynos_pcie->rc_dbi_base);
		goto probe_fail;
	}

	pci->dbi_base = exynos_pcie->rc_dbi_base;

	temp_rsc = platform_get_resource_byname(pdev, IORESOURCE_MEM, "pcs");
	exynos_pcie->phy_pcs_base = devm_ioremap_resource(&pdev->dev, temp_rsc);
	if (IS_ERR(exynos_pcie->phy_pcs_base)) {
		ret = PTR_ERR(exynos_pcie->phy_pcs_base);
		goto probe_fail;
	}

	if (exynos_pcie->use_ia) {
		temp_rsc = platform_get_resource_byname(pdev,
						IORESOURCE_MEM, "ia");
		exynos_pcie->ia_base =
				devm_ioremap_resource(&pdev->dev, temp_rsc);
		if (IS_ERR(exynos_pcie->ia_base)) {
			ret = PTR_ERR(exynos_pcie->ia_base);
			goto probe_fail;
		}
	}

	exynos_pcie->dbg_pmu1 = devm_ioremap(&pdev->dev, 0x15863e10, 0x4);
	exynos_pcie->dbg_pmu2 = devm_ioremap(&pdev->dev, 0x1586071c, 0x4);

	/* TCXO_FAR on */
	hsi_tcxo_far_control(1, 1);

	/* Mapping PHY functions */
	exynos_pcie_phy_init(pp);

	exynos_pcie_ops_init(pp);

	if (exynos_pcie->use_cache_coherency)
		enable_cache_cohernecy(pp, 1);

#ifdef GEN3_PHY_OFF
	if (g_gen3_phy_off)
		exynos_gen3_phy_pwrdn(exynos_pcie);
#endif
	exynos_pcie_resumed_phydown(pp);

	regmap_update_bits(exynos_pcie->pmureg,
			   exynos_pcie->pmu_offset,
			   PCIE_PHY_CONTROL_MASK, 1);

	//set_dma_ops(&pdev->dev, &exynos_pcie_dma_ops);

	exynos_pcie_make_reg_tb(&pdev->dev, exynos_pcie);

	save_pcie_rmem(exynos_pcie->ch_num);

	ret = add_pcie_port(pp, pdev);
	if (ret)
		goto probe_fail;

	/* TCXO_FAR on */
	hsi_tcxo_far_control(1, 1);

	/* force disable Lane0 */
	dev_dbg(&pdev->dev, "%s: force disable Lane0", __func__);
	udelay(100);
	val = exynos_sysreg_read(exynos_pcie,
			PCIE_WIFI0_PCIE_PHY_CONTROL);
	val &= ~(PCS_LANE0_ENABLE);
	exynos_sysreg_write(exynos_pcie, val,
			PCIE_WIFI0_PCIE_PHY_CONTROL);

	/* force phy all power down */
	dev_dbg(&pdev->dev, "%s: force one more all pwrdn", __func__);
	if (exynos_pcie->phy_ops.phy_all_pwrdn != NULL) {
		exynos_pcie->phy_ops.phy_all_pwrdn(exynos_pcie, exynos_pcie->ch_num);
	}
	disable_irq(pp->irq);

#if IS_ENABLED(CONFIG_CPU_IDLE)
	exynos_pcie->idle_ip_index =
			exynos_get_idle_ip_index(dev_name(&pdev->dev), 0);
	if (exynos_pcie->idle_ip_index < 0) {
		dev_err(&pdev->dev, "Cant get idle_ip_dex!!!\n");
		ret = -EINVAL;
		goto probe_fail;
	}
	exynos_update_ip_idle_status(exynos_pcie->idle_ip_index, PCIE_IS_IDLE);
	exynos_pcie->power_mode_nb.notifier_call = exynos_pci_power_mode_event;
	exynos_pcie->power_mode_nb.next = NULL;
	exynos_pcie->power_mode_nb.priority = 0;

	//ret = exynos_cpupm_notifier_register(&exynos_pcie->power_mode_nb); //no exynos_cpupm_notifier_register
	//if (ret) {
	//	dev_err(&pdev->dev, "Failed to register lpa notifier\n");
	//	goto probe_fail;
	//}
#endif

#ifdef USE_PANIC_NOTIFIER
	/* Register Panic notifier */
	exynos_pcie->ss_dma_mon_nb.notifier_call = exynos_pcie_dma_mon_event;
	exynos_pcie->ss_dma_mon_nb.next = NULL;
	exynos_pcie->ss_dma_mon_nb.priority = 0;
	atomic_notifier_chain_register(&panic_notifier_list,
				&exynos_pcie->ss_dma_mon_nb);
#endif

	exynos_pcie->pcie_wq = create_freezable_workqueue("pcie_wq");
	if (IS_ERR(exynos_pcie->pcie_wq)) {
		dev_err(pci->dev, "couldn't create workqueue\n");
		ret = EBUSY;
		goto probe_fail;
	}
	INIT_DELAYED_WORK(&exynos_pcie->dislink_work,
				exynos_pcie_dislink_work);

	INIT_DELAYED_WORK(&exynos_pcie->l1ss_boot_delay_work,
				exynos_pcie_work_l1ss);

	/* TCXO_FAR off */
	hsi_tcxo_far_control(1, 0);

	platform_set_drvdata(pdev, exynos_pcie);

probe_fail:
	if (ret)
		dev_err(&pdev->dev, "%s: pcie probe failed\n", __func__);
	else
		dev_info(&pdev->dev, "%s: pcie probe success\n", __func__);

	return ret;
}

static int __exit exynos_pcie_remove(struct platform_device *pdev)
{
	struct exynos_pcie *exynos_pcie = platform_get_drvdata(pdev);

#ifdef NCLK_OFF_CONTROL
	/* Set NCLK_OFF for Speculative access issue after resume. */
	int i;

	for (i = 0; i < MAX_RC_NUM + 1; i++) {
		if (elbi_nclk_reg[i] != NULL)
			iounmap(elbi_nclk_reg[i]);
	}

	if (exynos_pcie->ip_ver >= 0x981000)
		exynos_elbi_write(exynos_pcie, (0x1 << NCLK_OFF_OFFSET),
							PCIE_L12ERR_CTRL);
#endif

#if IS_ENABLED(CONFIG_CPU_IDLE)
#if 0
	exynos_pm_unregister_notifier(&exynos_pcie->power_mode_nb);
#endif
#endif

	if (exynos_pcie->state != STATE_LINK_DOWN) {
		exynos_pcie_poweroff(exynos_pcie->ch_num);
	}

	remove_pcie_sys_file(&pdev->dev);

	return 0;
}

static void exynos_pcie_shutdown(struct platform_device *pdev)
{
	struct exynos_pcie *exynos_pcie = platform_get_drvdata(pdev);
#ifdef USE_PANIC_NOTIFIER
	int ret;

	ret = atomic_notifier_chain_unregister(&panic_notifier_list,
			&exynos_pcie->ss_dma_mon_nb);
	if (ret)
		dev_err(&pdev->dev,
			"Failed to unregister snapshot panic notifier\n");
#endif
	exynos_pcie->shutdown = 1;
}

static const struct of_device_id exynos_pcie_of_match[] = {
	{ .compatible = "samsung,exynos8890-pcie", },
	{ .compatible = "samsung,exynos-pcie", },
	{},
};
MODULE_DEVICE_TABLE(of, exynos_pcie_of_match);

static void exynos_pcie_resumed_phydown(struct pcie_port *pp)
{
	struct dw_pcie *pci = to_dw_pcie_from_pp(pp);
	struct exynos_pcie *exynos_pcie = to_exynos_pcie(pci);
	int ret;

	/* phy all power down on wifi off during suspend/resume */
	ret = exynos_pcie_clock_enable(pp, PCIE_ENABLE_CLOCK);
	dev_dbg(pci->dev, "pcie clk enable, ret value = %d\n", ret);

	exynos_pcie_enable_interrupts(pp);
	regmap_update_bits(exynos_pcie->pmureg,
			   exynos_pcie->pmu_offset,
			   PCIE_PHY_CONTROL_MASK, 1);

	exynos_pcie_assert_phy_reset(pp);

	/* phy all power down */
	if (exynos_pcie->phy_ops.phy_all_pwrdn != NULL) {
		exynos_pcie->phy_ops.phy_all_pwrdn(exynos_pcie, exynos_pcie->ch_num);
	}

	exynos_pcie_phy_clock_enable(pp, PCIE_DISABLE_CLOCK);
#if IS_ENABLED(CONFIG_DYNAMIC_PHY_OFF)
	regmap_update_bits(exynos_pcie->pmureg,
			   exynos_pcie->pmu_offset,
			   PCIE_PHY_CONTROL_MASK, 0);
#endif
	exynos_pcie_clock_enable(pp, PCIE_DISABLE_CLOCK);
}

void exynos_pcie_config_l1ss_bcm_wifi(struct pcie_port *pp)
{
	struct dw_pcie *pci = to_dw_pcie_from_pp(pp);
	struct exynos_pcie *exynos_pcie = to_exynos_pcie(pci);
	struct pci_dev *ep_pci_dev;
	u32 exp_cap_off = PCIE_CAP_OFFSET;
	u32 val;

	dev_info(pci->dev, "%s: +++\n", __func__);

	ep_pci_dev = exynos_pcie_get_pci_dev(pp);
	if (ep_pci_dev == NULL) {
		dev_err(pci->dev,
				"Failed to set L1SS Enable (pci_dev == NULL)!!!\n");
#ifdef CONFIG_SEC_PANIC_PCIE_ERR
		panic("[PCIe Case#3] No ep_pci_dev found! %s\n", exynos_pcie->ep_device_name);
#endif
		return ;
	}

	pci_write_config_dword(ep_pci_dev, WIFI_L1SS_CONTROL2,
			PORT_LINK_TPOWERON_130US);
	pci_write_config_dword(ep_pci_dev, 0x1B4, 0x10031003);
	pci_read_config_dword(ep_pci_dev, 0xD4, &val);
	pci_write_config_dword(ep_pci_dev, 0xD4, val | (1 << 10));

	exynos_pcie_rd_own_conf(pp, PCIE_LINK_L1SS_CONTROL, 4, &val);
	val |= PORT_LINK_TCOMMON_32US | PORT_LINK_L1SS_ENABLE |
		PORT_LINK_L12_LTR_THRESHOLD;
	exynos_pcie_wr_own_conf(pp, PCIE_LINK_L1SS_CONTROL, 4, val);
	exynos_pcie_wr_own_conf(pp, PCIE_LINK_L1SS_CONTROL2, 4,
			PORT_LINK_TPOWERON_130US);
	/*
	   val = PORT_LINK_L1SS_T_PCLKACK | PORT_LINK_L1SS_T_L1_2 |
	   PORT_LINK_L1SS_T_POWER_OFF;
	 */
	exynos_pcie_wr_own_conf(pp, PCIE_L1_SUBSTATES_OFF, 4, PCIE_L1_SUB_VAL);
	exynos_pcie_wr_own_conf(pp, exp_cap_off + PCI_EXP_DEVCTL2, 4,
			PCI_EXP_DEVCTL2_LTR_EN);

	/* Enable L1SS on Root Complex */
	exynos_pcie_rd_own_conf(pp, exp_cap_off + PCI_EXP_LNKCTL, 4, &val);
	val &= ~PCI_EXP_LNKCTL_ASPMC;
	val |= PCI_EXP_LNKCTL_CCC | PCI_EXP_LNKCTL_ASPM_L1;
	exynos_pcie_wr_own_conf(pp, exp_cap_off + PCI_EXP_LNKCTL, 4, val);

	/* Enable supporting ASPM/PCIPM */
	pci_read_config_dword(ep_pci_dev, WIFI_L1SS_CONTROL, &val);
	pci_write_config_dword(ep_pci_dev, WIFI_L1SS_CONTROL,
			val | WIFI_COMMON_RESTORE_TIME | WIFI_ALL_PM_ENABEL |
			PORT_LINK_L12_LTR_THRESHOLD);

	if (exynos_pcie->l1ss_ctrl_id_state == 0) {
		pci_read_config_dword(ep_pci_dev, WIFI_L1SS_LINKCTRL, &val);
		val &= ~(WIFI_ASPM_CONTROL_MASK);
		val |= WIFI_CLK_REQ_EN | WIFI_USE_SAME_REF_CLK |
			WIFI_ASPM_L1_ENTRY_EN;
		pci_write_config_dword(ep_pci_dev, WIFI_L1SS_LINKCTRL, val);

		dev_err(pci->dev, "l1ss enabled(0x%x)\n",
				exynos_pcie->l1ss_ctrl_id_state);
	} else {
		pci_read_config_dword(ep_pci_dev, WIFI_L1SS_LINKCTRL, &val);
		val &= ~(WIFI_ASPM_CONTROL_MASK);
		val |= WIFI_CLK_REQ_EN | WIFI_USE_SAME_REF_CLK;
		pci_write_config_dword(ep_pci_dev, WIFI_L1SS_LINKCTRL, val);

		exynos_pcie_rd_own_conf(pp, exp_cap_off + PCI_EXP_LNKCTL,
				4, &val);
		val &= ~PCI_EXP_LNKCTL_ASPMC;
		exynos_pcie_wr_own_conf(pp, exp_cap_off + PCI_EXP_LNKCTL,
				4, val);
		dev_info(pci->dev, "l1ss disabled(0x%x)\n",
				exynos_pcie->l1ss_ctrl_id_state);
	}
	dev_info(pci->dev, "%s: ---\n", __func__);
}

void exynos_pcie_config_l1ss_common(struct pcie_port *pp)
{
	struct dw_pcie *pci = to_dw_pcie_from_pp(pp);
	struct exynos_pcie *exynos_pcie = to_exynos_pcie(pci);
	struct pci_dev *ep_pci_dev;
	u32 exp_cap_off = PCIE_CAP_OFFSET;
	u32 val;

	ep_pci_dev = exynos_pcie_get_pci_dev(pp);
	if (ep_pci_dev == NULL) {
		dev_err(pci->dev,
				"Failed to set L1SS Enable (pci_dev == NULL)!!!\n");
		return ;
	}
	pci_write_config_dword(ep_pci_dev, exynos_pcie->ep_l1ss_ctrl2_off,
			PORT_LINK_TPOWERON_130US);

	/* Get COMMON_MODE Value */
	exynos_pcie_rd_own_conf(pp, PCIE_LINK_L1SS_CAP, 4, &val);
	val &= ~(PORT_LINK_TCOMMON_MASK);
	val |= WIFI_COMMON_RESTORE_TIME_70US;
	exynos_pcie_wr_own_conf(pp, PCIE_LINK_L1SS_CAP, 4, val);
	exynos_pcie_rd_own_conf(pp, PCIE_LINK_L1SS_CAP, 4, &val);
	dev_dbg(pci->dev, "%s:RC l1ss cap: 0x%x\n", __func__, val);

	exynos_pcie_wr_own_conf(pp, PCIE_L1_SUBSTATES_OFF, 4, PCIE_L1_SUB_VAL);
	exynos_pcie_wr_own_conf(pp, exp_cap_off + PCI_EXP_DEVCTL2, 4,
			PCI_EXP_DEVCTL2_LTR_EN);

	if (exynos_pcie->l1ss_ctrl_id_state == 0) {
		/* RC L1ss Enable */
		exynos_pcie_rd_own_conf(pp, PCIE_LINK_L1SS_CONTROL, 4, &val);
		val &= ~(PORT_LINK_TCOMMON_MASK);
		val |= WIFI_COMMON_RESTORE_TIME_70US | PORT_LINK_L12_ENABLE |
			PORT_LINK_L12_LTR_THRESHOLD_0US;
		exynos_pcie_wr_own_conf(pp, PCIE_LINK_L1SS_CONTROL, 4, val);
		exynos_pcie_wr_own_conf(pp, PCIE_LINK_L1SS_CONTROL2, 4,
				PORT_LINK_TPOWERON_130US);

		exynos_pcie_rd_own_conf(pp, PCIE_LINK_L1SS_CONTROL, 4, &val);
		dev_dbg(pci->dev, "%s: RC l1ss control: 0x%x\n", __func__, val);

		/* EP L1ss Enable */
		pci_read_config_dword(ep_pci_dev, exynos_pcie->ep_l1ss_ctrl1_off, &val);
		pci_write_config_dword(ep_pci_dev, exynos_pcie->ep_l1ss_ctrl1_off,
				val | WIFI_ASPM_L12_EN | WIFI_PCIPM_L12_EN | PORT_LINK_L12_LTR_THRESHOLD_0US);
		dev_dbg(pci->dev, "%s: EP l1ss control: 0x%x\n", __func__, val);

		/* RC ASPM Enable */
		exynos_pcie_rd_own_conf(pp, exp_cap_off + PCI_EXP_LNKCTL, 4, &val);
		val &= ~PCI_EXP_LNKCTL_ASPMC;
		val |= PCI_EXP_LNKCTL_CCC | PCI_EXP_LNKCTL_ASPM_L1;
		exynos_pcie_wr_own_conf(pp, exp_cap_off + PCI_EXP_LNKCTL, 4, val);

		/* EP ASPM Enable */
		pci_read_config_dword(ep_pci_dev, exynos_pcie->ep_link_ctrl_off, &val);
		val &= ~(WIFI_ASPM_CONTROL_MASK);
		val |= WIFI_USE_SAME_REF_CLK |
			WIFI_ASPM_L1_ENTRY_EN;
		pci_write_config_dword(ep_pci_dev, exynos_pcie->ep_link_ctrl_off, val);

		dev_info(pci->dev, "l1ss enabled(0x%x)\n",
				exynos_pcie->l1ss_ctrl_id_state);
	} else {
		/* EP ASPM Dsiable */
		pci_read_config_dword(ep_pci_dev, exynos_pcie->ep_link_ctrl_off, &val);
		val &= ~(WIFI_ASPM_CONTROL_MASK);
		val |= WIFI_USE_SAME_REF_CLK;
		pci_write_config_dword(ep_pci_dev, exynos_pcie->ep_link_ctrl_off, val);

		/* RC ASPM Disable */
		exynos_pcie_rd_own_conf(pp, exp_cap_off + PCI_EXP_LNKCTL,
				4, &val);
		val &= ~PCI_EXP_LNKCTL_ASPMC;
		exynos_pcie_wr_own_conf(pp, exp_cap_off + PCI_EXP_LNKCTL,
				4, val);

		/* EP L1ss Disable */
		pci_read_config_dword(ep_pci_dev, exynos_pcie->ep_l1ss_ctrl1_off, &val);
		val &= ~(WIFI_ALL_PM_ENABEL);
		pci_write_config_dword(ep_pci_dev, exynos_pcie->ep_l1ss_ctrl1_off, val);
		dev_dbg(pci->dev, "%s: EP l1ss control: 0x%x\n", __func__, val);

		/* RC L1ss Disable */
		exynos_pcie_rd_own_conf(pp, PCIE_LINK_L1SS_CONTROL, 4, &val);
		val &= ~(PORT_LINK_L1SS_ENABLE);
		exynos_pcie_wr_own_conf(pp, PCIE_LINK_L1SS_CONTROL, 4, val);
		dev_dbg(pci->dev, "%s: RC l1ss control: 0x%x\n", __func__, val);

		dev_info(pci->dev, "l1ss disabled(0x%x)\n",
				exynos_pcie->l1ss_ctrl_id_state);
	}
}

void exynos_pcie_config_l1ss(struct pcie_port *pp)
{
	struct dw_pcie *pci = to_dw_pcie_from_pp(pp);
	struct exynos_pcie *exynos_pcie = to_exynos_pcie(pci);
	unsigned long flags;
	struct pci_dev *ep_pci_dev;

	/* This function is only for BCM WIFI devices */
	if ((exynos_pcie->ep_device_type != EP_BCM_WIFI) && (exynos_pcie->ep_device_type != EP_QC_WIFI)) {
		dev_err(pci->dev, "Can't set L1SS(It's not WIFI device)!!!\n");
		return ;
	}

	ep_pci_dev = exynos_pcie_get_pci_dev(pp);
	if (ep_pci_dev == NULL) {
		dev_err(pci->dev,
				"Failed to set L1SS Enable (pci_dev == NULL)!!!\n");
		return ;
	}

	exynos_pcie->ep_link_ctrl_off = ep_pci_dev->pcie_cap + PCI_EXP_LNKCTL;
	exynos_pcie->ep_l1ss_ctrl1_off = exynos_pcie->ep_l1ss_cap_off + PCI_L1SS_CTL1;
	exynos_pcie->ep_l1ss_ctrl2_off = exynos_pcie->ep_l1ss_cap_off + PCI_L1SS_CTL2;
	dev_dbg(pci->dev, "%s: (offset)link_ctrl: 0x%x, l1ss_ctrl1: 0x%x, l1ss_ctrl2: 0x%x\n",
			__func__,
			exynos_pcie->ep_link_ctrl_off,
			exynos_pcie->ep_l1ss_ctrl1_off,
			exynos_pcie->ep_l1ss_ctrl2_off);

	spin_lock_irqsave(&exynos_pcie->conf_lock, flags);
	if (exynos_pcie->ep_device_type == EP_BCM_WIFI) {
		exynos_pcie_config_l1ss_bcm_wifi(pp);
	} else if (exynos_pcie->ep_device_type == EP_QC_WIFI) {
		exynos_pcie_config_l1ss_common(pp);
	}
	spin_unlock_irqrestore(&exynos_pcie->conf_lock, flags);
}

int exynos_pcie_poweron(int ch_num)
{
	struct exynos_pcie *exynos_pcie = &g_pcie[ch_num];
	struct dw_pcie *pci = exynos_pcie->pci;
	struct pcie_port *pp = &pci->pp;
	int ret;
	u32 val;
	struct irq_desc *exynos_pcie_desc = irq_to_desc(pp->irq);
	unsigned long flags;

	dev_info(pci->dev, "%s, start of poweron, pcie state: %d\n", __func__,
							 exynos_pcie->state);

	if (exynos_pcie->shutdown == 1) {
		dev_info(pci->dev, "%s, already shutdown, so skip poweron %d\n", __func__,
				exynos_pcie->shutdown);

		return -EPIPE;
	}

	if (exynos_pcie->state == STATE_LINK_DOWN) {
		spin_lock_irqsave(&exynos_pcie->reg_lock, flags);
		exynos_pcie->state = STATE_PHY_OPT_OFF;
		spin_unlock_irqrestore(&exynos_pcie->reg_lock, flags);

		/* TCXO_FAR on */
		hsi_tcxo_far_control(1, 1);

#if IS_ENABLED(CONFIG_SOC_EXYNOS2100)
		exynos_gen4_nclkoff();
#endif

		ret = exynos_pcie_clock_enable(pp, PCIE_ENABLE_CLOCK);
		dev_dbg(pci->dev, "pcie clk enable, ret value = %d\n", ret);

		/* It's not able to use after exynos9810, PMU is chagned to SW PMU */
		if (exynos_pcie->ip_ver < 0x982000) {
			/* Release wakeup maks */
			regmap_update_bits(exynos_pcie->pmureg, WAKEUP_MASK,
				(0x1 << (WAKEUP_MASK_PCIE_WIFI + exynos_pcie->ch_num)),
				(0x1 << (WAKEUP_MASK_PCIE_WIFI + exynos_pcie->ch_num)));
		}

#if IS_ENABLED(CONFIG_CPU_IDLE)
		if (exynos_pcie->use_sicd)
			exynos_update_ip_idle_status(
					exynos_pcie->idle_ip_index,
					PCIE_IS_ACTIVE);
#endif
#if IS_ENABLED(CONFIG_EXYNOS_PM_QOS)
		if (exynos_pcie->int_min_lock) {
			exynos_pm_qos_update_request(&exynos_pcie_int_qos[ch_num],
					exynos_pcie->int_min_lock);
			dev_dbg(pci->dev, "%s: pcie int_min_lock = %d\n",
					__func__, exynos_pcie->int_min_lock);
		}
#endif
		/* Enable SysMMU */
		if (exynos_pcie->use_sysmmu)
			pcie_sysmmu_enable(ch_num);
		pinctrl_select_state(exynos_pcie->pcie_pinctrl,
				exynos_pcie->pin_state[PCIE_PIN_ACTIVE]);
		regmap_update_bits(exynos_pcie->pmureg,
				exynos_pcie->pmu_offset,
				   PCIE_PHY_CONTROL_MASK, 1);

		hsi_tcxo_far_control(1, 1);

		/* phy all power down clear */
		if (exynos_pcie->phy_ops.phy_all_pwrdn_clear != NULL) {
			exynos_pcie->phy_ops.phy_all_pwrdn_clear(exynos_pcie,
					exynos_pcie->ch_num);
		}

		/* force enable Lane0 */
		dev_dbg(pci->dev, "%s: force enable Lane0", __func__);
		val = exynos_sysreg_read(exynos_pcie,
				PCIE_WIFI0_PCIE_PHY_CONTROL);
		val |= PCS_LANE0_ENABLE;
		exynos_sysreg_write(exynos_pcie, val,
				PCIE_WIFI0_PCIE_PHY_CONTROL);
		udelay(100);

		{
			dev_info(pci->dev, "1) TCXO_FAR: 0x%x, Lane0: 0x%x, isol: 0x%x, PLL Lock: 0x%x\n",
					readl(exynos_pcie->dbg_pmu1),
					exynos_sysreg_read(exynos_pcie,	PCIE_WIFI0_PCIE_PHY_CONTROL),
					readl(exynos_pcie->dbg_pmu2),
					readl(exynos_pcie->phy_base + 0xBC));
		}

		/* Enable history buffer */
		val = exynos_elbi_read(exynos_pcie, PCIE_STATE_HISTORY_CHECK);
		exynos_elbi_write(exynos_pcie, 0, PCIE_STATE_HISTORY_CHECK);
		val = HISTORY_BUFFER_ENABLE;
		exynos_elbi_write(exynos_pcie, val, PCIE_STATE_HISTORY_CHECK);

		exynos_elbi_write(exynos_pcie, 0x200000, PCIE_STATE_POWER_S);
		exynos_elbi_write(exynos_pcie, 0xffffffff, PCIE_STATE_POWER_M);

		if ((exynos_pcie_desc) && (exynos_pcie_desc->depth > 0)) {
			enable_irq(pp->irq);
		} else {
			dev_info(pci->dev, "%s, already enable_irq, so skip\n", __func__);
		}

		if (exynos_pcie_establish_link(pp)) {
			dev_err(pci->dev, "pcie link up fail\n");
			goto poweron_fail;
		}
#ifdef NCLK_OFF_CONTROL
		if (exynos_pcie->ip_ver >= 0x981000)
			exynos_elbi_write(exynos_pcie, 0x0, PCIE_L12ERR_CTRL);
#endif
		spin_lock_irqsave(&exynos_pcie->reg_lock, flags);
		exynos_pcie->state = STATE_LINK_UP;
		spin_unlock_irqrestore(&exynos_pcie->reg_lock, flags);

		if (!exynos_pcie->probe_ok) {
			pci_rescan_bus(exynos_pcie->pci_dev->bus);

			exynos_pcie->ep_pci_dev = exynos_pcie_get_pci_dev(&pci->pp);

			exynos_pcie->ep_pci_dev->dma_mask = DMA_BIT_MASK(36);
			exynos_pcie->ep_pci_dev->dev.coherent_dma_mask = DMA_BIT_MASK(36);
			dma_set_seg_boundary(&(exynos_pcie->ep_pci_dev->dev), DMA_BIT_MASK(36));

#if IS_ENABLED(CONFIG_EXYNOS_PCIE_GEN2_IOMMU) || IS_ENABLED(CONFIG_EXYNOS_PCIE_GEN2_S2MPU)
			/* This function is only for BCM WIFI devices */

			set_dma_ops(&exynos_pcie->ep_pci_dev->dev, &exynos_pcie_gen2_dma_ops);
			dev_dbg(pci->dev, "DMA operations are changed to support SysMMU or S2MPU\n");
#endif

			/* L1.2 ASPM enable */
			exynos_pcie_config_l1ss(&pci->pp);

#if IS_ENABLED(CONFIG_PCI_MSI)
			exynos_pcie_msi_init(pp);
#endif

			if (pci_save_state(exynos_pcie->pci_dev)) {
				dev_err(pci->dev, "Failed to save pcie state\n");
				goto poweron_fail;
			}
			exynos_pcie->pci_saved_configs =
				pci_store_saved_state(exynos_pcie->pci_dev);
			exynos_pcie->probe_ok = 1;
		} else if (exynos_pcie->probe_ok) {
			if (exynos_pcie->boot_cnt == 0) {
				schedule_delayed_work(
					&exynos_pcie->l1ss_boot_delay_work,
					msecs_to_jiffies(40000));
				exynos_pcie->boot_cnt++;
				exynos_pcie->l1ss_ctrl_id_state |=
							PCIE_L1SS_CTRL_BOOT;
			}
#if IS_ENABLED(CONFIG_PCI_MSI)
			exynos_pcie_msi_init(pp);
#endif
			if (pci_load_saved_state(exynos_pcie->pci_dev,
					     exynos_pcie->pci_saved_configs)) {
				dev_err(pci->dev, "Failed to load pcie state\n");
				goto poweron_fail;
			}

			{
				dev_info(pci->dev, "3) TCXO_FAR: 0x%x, Lane0: 0x%x, isol: 0x%x, PLL Lock: 0x%x\n",
						readl(exynos_pcie->dbg_pmu1),
						exynos_sysreg_read(exynos_pcie,	PCIE_WIFI0_PCIE_PHY_CONTROL),
						readl(exynos_pcie->dbg_pmu2),
						readl(exynos_pcie->phy_base + 0xBC));
			}

			pci_restore_state(exynos_pcie->pci_dev);

			/* L1.2 ASPM enable */
			exynos_pcie_config_l1ss(pp);
		}
	}

	dev_info(pci->dev, "%s, end, D sate: 0x%x, GEN%d (MAX GEN%d), pcie state: %d\n", __func__,
			exynos_pcie->dstate, exynos_pcie->current_speed, exynos_pcie->max_link_speed,
			exynos_pcie->state);

	return 0;

poweron_fail:
	exynos_pcie->state = STATE_LINK_UP;
	exynos_pcie_poweroff(exynos_pcie->ch_num);

	return -EPIPE;
}
EXPORT_SYMBOL(exynos_pcie_poweron);

void exynos_pcie_poweroff(int ch_num)
{
	struct exynos_pcie *exynos_pcie = &g_pcie[ch_num];
	struct dw_pcie *pci = exynos_pcie->pci;
	struct pcie_port *pp = &pci->pp;
	unsigned long flags, flags1;
	u32 val;

	dev_info(pci->dev, "%s, start of poweroff, pcie state: %d\n", __func__,
		 exynos_pcie->state);

	if (exynos_pcie->state == STATE_LINK_UP ||
	    exynos_pcie->state == STATE_LINK_DOWN_TRY) {
		spin_lock_irqsave(&exynos_pcie->reg_lock, flags1);
		exynos_pcie->state = STATE_PHY_OPT_OFF;
		spin_unlock_irqrestore(&exynos_pcie->reg_lock, flags1);

		/* TCXO_FAR on */
		hsi_tcxo_far_control(1, 1);

		disable_irq(pp->irq);

		val = exynos_elbi_read(exynos_pcie, PCIE_IRQ_EN_PULSE);
		val &= ~IRQ_LINKDOWN_ENABLE;
		exynos_elbi_write(exynos_pcie, val, PCIE_IRQ_EN_PULSE);

		dev_dbg(pci->dev, "%s, move pme_turn_off to before spin_lock\n", __func__);
		exynos_pcie_send_pme_turn_off(exynos_pcie);

		spin_lock_irqsave(&exynos_pcie->conf_lock, flags);
		/* Disable SysMMU */
		if (exynos_pcie->use_sysmmu)
			pcie_sysmmu_disable(ch_num);

		/* Disable history buffer */
		val = exynos_elbi_read(exynos_pcie, PCIE_STATE_HISTORY_CHECK);
		val &= ~HISTORY_BUFFER_ENABLE;
		exynos_elbi_write(exynos_pcie, val, PCIE_STATE_HISTORY_CHECK);

		gpio_set_value(exynos_pcie->perst_gpio, 0);
		dev_info(pci->dev, "%s: check ack message, L23_Ready and Set PERST to LOW, gpio val = %d\n",
			__func__, gpio_get_value(exynos_pcie->perst_gpio));

		/* LTSSM disable */
		exynos_elbi_write(exynos_pcie, PCIE_ELBI_LTSSM_DISABLE,
				PCIE_APP_LTSSM_ENABLE);

		spin_unlock_irqrestore(&exynos_pcie->conf_lock, flags);

		hsi_tcxo_far_control(1, 1);

		spin_lock_irqsave(&exynos_pcie->reg_lock, flags1);

		/* phy all power down */
		if (exynos_pcie->phy_ops.phy_all_pwrdn != NULL) {
			exynos_pcie->phy_ops.phy_all_pwrdn(exynos_pcie,
					exynos_pcie->ch_num);
		}

		/* force disable Lane0 */
		dev_dbg(pci->dev, "%s: force disable Lane0", __func__);
		udelay(100);
		val = exynos_sysreg_read(exynos_pcie,
				PCIE_WIFI0_PCIE_PHY_CONTROL);
		val &= ~(PCS_LANE0_ENABLE);
		exynos_sysreg_write(exynos_pcie, val,
				PCIE_WIFI0_PCIE_PHY_CONTROL);
		dev_info(pci->dev, "%s: force disable Lane0: 0x%x, state: %x\n", __func__,
				exynos_sysreg_read(exynos_pcie,	PCIE_WIFI0_PCIE_PHY_CONTROL),
				exynos_pcie->state);

		spin_unlock_irqrestore(&exynos_pcie->reg_lock, flags1);

		exynos_pcie_phy_clock_enable(pp, PCIE_DISABLE_CLOCK);
#if IS_ENABLED(CONFIG_DYNAMIC_PHY_OFF)
		/* EP device could try to save PCIe state after DisLink
		 * in suspend time. The below code keep PHY power up in this
		 * time.
		 */
		if (pp->dev->power.is_prepared == false)
			regmap_update_bits(exynos_pcie->pmureg,
				exynos_pcie->pmu_offset,
				   PCIE_PHY_CONTROL_MASK, 0);
#endif
		exynos_pcie_clock_enable(pp, PCIE_DISABLE_CLOCK);

		exynos_pcie->atu_ok = 0;

		if (!IS_ERR(exynos_pcie->pin_state[PCIE_PIN_IDLE]))
			pinctrl_select_state(exynos_pcie->pcie_pinctrl,
					exynos_pcie->pin_state[PCIE_PIN_IDLE]);

#if IS_ENABLED(CONFIG_EXYNOS_PM_QOS)
		if (exynos_pcie->int_min_lock) {
			exynos_pm_qos_update_request(&exynos_pcie_int_qos[ch_num], 0);
			dev_dbg(pci->dev, "%s: pcie int_min_lock = %d\n",
					__func__, exynos_pcie->int_min_lock);
		}
#endif
#if IS_ENABLED(CONFIG_CPU_IDLE)
		if (exynos_pcie->use_sicd)
			exynos_update_ip_idle_status(
					exynos_pcie->idle_ip_index,
					PCIE_IS_IDLE);
#endif
#ifdef NCLK_OFF_CONTROL
		if (exynos_pcie->ip_ver >= 0x981000)
			exynos_elbi_write(exynos_pcie, (0x1 << NCLK_OFF_OFFSET),
							PCIE_L12ERR_CTRL);
#endif
	}

	/* It's not able to use after exynos9810, PMU is chagned to SW PMU */
	if (exynos_pcie->ip_ver < 0x982000) {
		/* Set wakeup mask */
		regmap_update_bits(exynos_pcie->pmureg,
				WAKEUP_MASK,
				(0x1 << (WAKEUP_MASK_PCIE_WIFI + exynos_pcie->ch_num)),
				(0x0 << (WAKEUP_MASK_PCIE_WIFI + exynos_pcie->ch_num)));
	}

#if IS_ENABLED(CONFIG_SOC_EXYNOS2100)
	exynos_gen4_nclkoff();
#endif

	spin_lock_irqsave(&exynos_pcie->reg_lock, flags1);
	exynos_pcie->state = STATE_LINK_DOWN;

	/* TCXO_FAR off */
	hsi_tcxo_far_control(1, 0);
	spin_unlock_irqrestore(&exynos_pcie->reg_lock, flags1);

	dev_info(pci->dev, "%s, end of poweroff, pcie state: %d\n",  __func__,
		 exynos_pcie->state);
}
EXPORT_SYMBOL(exynos_pcie_poweroff);

void exynos_pcie_send_pme_turn_off(struct exynos_pcie *exynos_pcie)
{
	struct dw_pcie *pci = exynos_pcie->pci;
	struct device *dev = pci->dev;
	int __maybe_unused count = 0;
	int __maybe_unused retry_cnt = 0;
	u32 __maybe_unused val;

	dev_dbg(dev, "%s: sub_ctrl+0x0 = 0x%x\n", __func__,
			exynos_elbi_read(exynos_pcie, PCIE_IRQ_PULSE));
	dev_dbg(dev, "%s: sub_ctrl+0x4 = 0x%x\n", __func__,
			exynos_elbi_read(exynos_pcie, PCIE_IRQ_LEVEL));
	dev_dbg(dev, "%s: sub_ctrl+0x8 = 0x%x\n", __func__,
			exynos_elbi_read(exynos_pcie, PCIE_IRQ_SPECIAL));

	val = exynos_elbi_read(exynos_pcie, PCIE_ELBI_RDLH_LINKUP) & 0x1f;
	dev_dbg(dev, "%s: link state:%x\n", __func__, val);
	if (!(val >= 0x0d && val <= 0x14)) {
		dev_err(dev, "%s, pcie link is not up\n", __func__);
		return;
	}

	exynos_elbi_write(exynos_pcie, 0x1, PCIE_APP_REQ_EXIT_L1);
	val = exynos_elbi_read(exynos_pcie, PCIE_APP_REQ_EXIT_L1_MODE);
	val &= ~APP_REQ_EXIT_L1_MODE;
	val |= L1_REQ_NAK_CONTROL_MASTER;
	exynos_elbi_write(exynos_pcie, val, PCIE_APP_REQ_EXIT_L1_MODE);

retry_pme_turnoff:
	if (retry_cnt > 0) {
		val = exynos_elbi_read(exynos_pcie, PCIE_ELBI_RDLH_LINKUP) & 0x1f;
		dev_err(dev, "Current LTSSM State is 0x%x with retry_cnt =%d.\n",
				val, retry_cnt);
	}

	exynos_elbi_write(exynos_pcie, 0x1, XMIT_PME_TURNOFF);

	while (count < MAX_L2_TIMEOUT) {
		if ((exynos_elbi_read(exynos_pcie, PCIE_IRQ_PULSE)
					& IRQ_RADM_PM_TO_ACK)) {
			dev_dbg(dev, "ack message is ok\n");
			udelay(10);
			break;
		}

		udelay(10);
		count++;
	}

	if (count >= MAX_L2_TIMEOUT)
		dev_err(dev, "cannot receive ack message from wifi\n");

	exynos_elbi_write(exynos_pcie, 0x0, XMIT_PME_TURNOFF);

	count = 0;
	do {
		val = exynos_elbi_read(exynos_pcie, PCIE_ELBI_RDLH_LINKUP);
		val = val & 0x1f;
		if (val == 0x15) {
			dev_dbg(dev, "received Enter_L23_READY DLLP packet\n");
			break;
		}
		udelay(10);
		count++;
	} while (count < MAX_L2_TIMEOUT);

	if (count >= MAX_L2_TIMEOUT) {
		if (retry_cnt < 5) {
			retry_cnt++;
			goto retry_pme_turnoff;
		}
		dev_err(dev, "cannot receive L23_READY DLLP packet(0x%x)\n", val);
#ifdef CONFIG_SEC_PANIC_PCIE_ERR
		panic("[PCIe Case#5] L2/3 READY fail! %s\n", exynos_pcie->ep_device_name);
#endif
	}
}

void exynos_pcie_pm_suspend(int ch_num)
{
	struct exynos_pcie *exynos_pcie = &g_pcie[ch_num];
	struct dw_pcie *pci = exynos_pcie->pci;
	unsigned long flags;

	dev_info(pci->dev, "## exynos_pcie_pm_suspend(state: %d)\n", exynos_pcie->state);

	if (exynos_pcie->state == STATE_LINK_DOWN) {
		dev_info(pci->dev, "RC%d already off\n", exynos_pcie->ch_num);
		return;
	}

	spin_lock_irqsave(&exynos_pcie->conf_lock, flags);
	exynos_pcie->state = STATE_LINK_DOWN_TRY;
	spin_unlock_irqrestore(&exynos_pcie->conf_lock, flags);

	exynos_pcie_poweroff(ch_num);
}
EXPORT_SYMBOL(exynos_pcie_pm_suspend);

int exynos_pcie_pm_resume(int ch_num)
{
	return exynos_pcie_poweron(ch_num);
}
EXPORT_SYMBOL(exynos_pcie_pm_resume);

#if IS_ENABLED(CONFIG_PM)
static int exynos_pcie_suspend_noirq(struct device *dev)
{
	struct exynos_pcie *exynos_pcie = dev_get_drvdata(dev);
	struct dw_pcie *pci = exynos_pcie->pci;

	dev_info(pci->dev, "## exynos_pcie_suspend_noirq(state: %d)\n", exynos_pcie->state);

	if (exynos_pcie->state == STATE_LINK_DOWN) {
		dev_info(dev, "RC%d already off\n", exynos_pcie->ch_num);
		return 0;
	}

	pr_err("WARNNING - Unexpected PCIe state(try to power OFF)\n");
	exynos_pcie_poweroff(exynos_pcie->ch_num);

	return 0;
}

static int exynos_pcie_resume_noirq(struct device *dev)
{
	struct exynos_pcie *exynos_pcie = dev_get_drvdata(dev);
	struct dw_pcie *pci = exynos_pcie->pci;
#ifdef NCLK_OFF_CONTROL
	/* Set NCLK_OFF for Speculative access issue after resume. */
	int i;
#endif

	dev_info(pci->dev, "## exynos_pcie_resume_noirq(state: %d)\n", exynos_pcie->state);

#ifdef NCLK_OFF_CONTROL
	for (i = 0; i < MAX_RC_NUM + 1; i++) {
		if (elbi_nclk_reg[i] != NULL) {
#if IS_ENABLED(CONFIG_SOC_EXYNOS9810)
			writel((0x1 << NCLK_OFF_OFFSET), elbi_nclk_reg[i]);
#elif IS_ENABLED(CONFIG_SOC_EXYNOS9820)
			if (i ==  0) {
				writel((0x1 << NCLK_OFF_OFFSET), elbi_nclk_reg[i]);
			} else {
				/* CH1 is GEN3A or GEN3B(bifurcation) and
				these registers are able to access after PMU PHY power on */
				regmap_update_bits(exynos_pcie->pmureg,
						exynos_pcie->pmu_offset,
						PCIE_PHY_CONTROL_MASK, 1);

				writel((0x1 << NCLK_OFF_OFFSET), elbi_nclk_reg[i]);

				regmap_update_bits(exynos_pcie->pmureg,
						exynos_pcie->pmu_offset,
						PCIE_PHY_CONTROL_MASK, 0);
			}
#endif
		}
	}
#endif
#if 0
	if (exynos_pcie->ip_ver >= 0x981000)
		exynos_elbi_write(exynos_pcie, (0x1 << NCLK_OFF_OFFSET),
							PCIE_L12ERR_CTRL);
#endif

#if IS_ENABLED(CONFIG_SOC_EXYNOS2100)
	exynos_gen4_nclkoff();
#endif

#ifdef GEN3_PHY_OFF
	if (g_gen3_phy_off)
		exynos_gen3_phy_pwrdn(exynos_pcie);
#endif
	if (!exynos_pcie->pci_dev) {
		dev_err(pci->dev, "Failed to get pci device\n");
	} else {
		exynos_pcie->pci_dev->state_saved = false;
		dev_dbg(&(exynos_pcie->pci_dev->dev), "state_saved flag = false");
	}

	if (exynos_pcie->state == STATE_LINK_DOWN) {
		/* TCXO_FAR on */
		hsi_tcxo_far_control(1, 1);

		exynos_pcie_resumed_phydown(&pci->pp);

#if IS_ENABLED(CONFIG_DYNAMIC_PHY_OFF)
		regmap_update_bits(exynos_pcie->pmureg,
			   exynos_pcie->pmu_offset,
			   PCIE_PHY_CONTROL_MASK, 1);
#endif
		return 0;
	}
	pr_err("WARNNING - Unexpected PCIe state(try to power ON)\n");
#if 0
	regmap_update_bits(exynos_pcie->pmureg,
			   exynos_pcie->pmu_offset,
			   PCIE_PHY_CONTROL_MASK, 1);
	exynos_pcie_poweron(exynos_pcie->ch_num);
#endif
	return 0;
}

#if IS_ENABLED(CONFIG_DYNAMIC_PHY_OFF)
static int exynos_pcie_suspend_prepare(struct device *dev)
{
	struct exynos_pcie *exynos_pcie = dev_get_drvdata(dev);

	regmap_update_bits(exynos_pcie->pmureg,
			   exynos_pcie->pmu_offset,
			   PCIE_PHY_CONTROL_MASK, 1);
	return 0;
}

static void exynos_pcie_resume_complete(struct device *dev)
{
	struct exynos_pcie *exynos_pcie = dev_get_drvdata(dev);

	if (exynos_pcie->state == STATE_LINK_DOWN)
		regmap_update_bits(exynos_pcie->pmureg,
			   exynos_pcie->pmu_offset,
			   PCIE_PHY_CONTROL_MASK, 0);
}
#endif

#else
#define exynos_pcie_suspend_noirq	NULL
#define exynos_pcie_resume_noirq	NULL
#endif

static const struct dev_pm_ops exynos_pcie_dev_pm_ops = {
	.suspend_noirq	= exynos_pcie_suspend_noirq,
	.resume_noirq	= exynos_pcie_resume_noirq,
#if IS_ENABLED(CONFIG_DYNAMIC_PHY_OFF)
	.prepare	= exynos_pcie_suspend_prepare,
	.complete	= exynos_pcie_resume_complete,
#endif
};

static struct platform_driver exynos_pcie_driver = {
	.probe          = exynos_pcie_probe,
	.remove         = exynos_pcie_remove,
	.shutdown       = exynos_pcie_shutdown,
	.driver = {
		.name   = "exynos-pcie",
		.owner  = THIS_MODULE,
		.pm     = &exynos_pcie_dev_pm_ops,
		.of_match_table = of_match_ptr(exynos_pcie_of_match),
	},
};

#if IS_ENABLED(CONFIG_CPU_IDLE)
static void __maybe_unused exynos_pcie_set_tpoweron(struct pcie_port *pp,
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
		exynos_pcie_wr_own_conf(pp, PCIE_LINK_L1SS_CONTROL2, 4,
				PORT_LINK_TPOWERON_3100US);
	} else {
		writel(PORT_LINK_TPOWERON_130US,
				ep_dbi_base + WIFI_L1SS_CONTROL2);
		exynos_pcie_wr_own_conf(pp, PCIE_LINK_L1SS_CONTROL2, 4,
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

static int exynos_pci_power_mode_event(struct notifier_block *nb,
					unsigned long event, void *data)
{
	int ret = NOTIFY_DONE;
	struct exynos_pcie *exynos_pcie = container_of(nb,
				struct exynos_pcie, power_mode_nb);
	u32 val;
	struct dw_pcie *pci = exynos_pcie->pci;
	struct pcie_port *pp = &pci->pp;

	switch (event) {
	case SICD_ENTER:
		if (exynos_pcie->use_sicd) {
			if (exynos_pcie->ip_ver >= 0x889500) {
				if (exynos_pcie->state != STATE_LINK_DOWN) {
					val = exynos_elbi_read(exynos_pcie,
						PCIE_ELBI_RDLH_LINKUP) & 0x1f;
					if (val == 0x14 || val == 0x15) {
						ret = NOTIFY_DONE;
						/* Change tpower on time to
						 * value
						 */
						exynos_pcie_set_tpoweron(pp, 1);
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
					exynos_pcie_set_tpoweron(pp, 0);
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

/* Exynos PCIe driver does not allow module unload */

static int __init pcie_init(void)
{
	struct device_node __maybe_unused *np;
	const char __maybe_unused *off;
	char __maybe_unused *pci_name;

#ifdef NCLK_OFF_CONTROL
	/*
	 * This patch is to avoid system hang by speculative access at disabled
	 * channel. Configuration of NCLK_OFF will return bus error when someone
	 * access PCIe outbound memory of disabled channel.
	 */
	const char *status;
	struct resource elbi_res;
	int i, elbi_index;
#if IS_ENABLED(CONFIG_SOC_EXYNOS9820)
	/* shoudl enable PMU PHY cntroller for accessing GEN3AB sub link controller registser*/
	struct regmap *pmureg;
#endif

	pci_name = kmalloc(EXYNOS_PCIE_MAX_NAME_LEN, GFP_KERNEL);

	for (i = 0; i < MAX_RC_NUM; i++) {
		snprintf(pci_name, EXYNOS_PCIE_MAX_NAME_LEN, "pcie%d", i);
		np = of_find_node_by_name(NULL, pci_name);
		if (np == NULL) {
			pr_err("PCIe%d node is NULL!!!\n", i);
			continue;
		}

		if (of_property_read_string(np, "status", &status)) {
			pr_err("Can't Read PCIe%d Status!!!\n", i);
			continue;
		}

		if (!strncmp(status, "disabled", 8)) {
			pr_info("PCIe%d is Disabled - Set NCLK_OFF...\n", i);
			elbi_index = of_property_match_string(np,
							"reg-names", "elbi");
			if (of_address_to_resource(np, elbi_index, &elbi_res)) {
				pr_err("Can't get ELBI resource!!!\n");
				continue;
			}
#if IS_ENABLED(CONFIG_SOC_EXYNOS9810)
			elbi_nclk_reg[i] = ioremap(elbi_res.start +
						PCIE_L12ERR_CTRL, SZ_4);
#elif IS_ENABLED(CONFIG_SOC_EXYNOS9820)
			if (i == 0) {
				elbi_nclk_reg[i] = ioremap(elbi_res.start +
						PCIE_L12ERR_CTRL, SZ_4);
			} else if (i == 1) {
				elbi_nclk_reg[i] = ioremap(GEN3A_L12ERR_CRTRL_SFR, SZ_4);
			}
#endif
			if (elbi_nclk_reg[i] == NULL) {
				pr_err("Can't get elbi addr!!!\n");
				continue;
			}
		}
	}

#if IS_ENABLED(CONFIG_SOC_EXYNOS9820)
	/* GEN3B should be always NCLK_OFF */
	elbi_nclk_reg[2] = ioremap(GEN3B_L12ERR_CRTRL_SFR, SZ_4);
	pmureg = syscon_regmap_lookup_by_phandle(np,
					"samsung,syscon-phandle");
#endif

	for (i = 0; i < MAX_RC_NUM + 1; i++) {
		if (elbi_nclk_reg[i] != NULL) {
#if IS_ENABLED(CONFIG_SOC_EXYNOS9810)
			writel((0x1 << NCLK_OFF_OFFSET), elbi_nclk_reg[i]);
#elif IS_ENABLED(CONFIG_SOC_EXYNOS9820)
			if (i ==  0) {
				writel((0x1 << NCLK_OFF_OFFSET), elbi_nclk_reg[i]);
			} else {
				/* CH1 is GEN3A or GEN3B(bifurcation) and
				these registers are able to access after PMU PHY power on */
				if (IS_ERR(pmureg)) {
					pr_err("syscon regmap lookup failed. so can't set NCLK_OFF of PCIe1\n");
				} else {
					regmap_update_bits(pmureg,
							exynos_pcie->pmu_offset,
							PCIE_PHY_CONTROL_MASK, 1);

					writel((0x1 << NCLK_OFF_OFFSET), elbi_nclk_reg[i]);

					regmap_update_bits(pmureg,
							exynos_pcie->pmu_offset,
							PCIE_PHY_CONTROL_MASK, 0);
				}
			}
#endif
		}
	}
	kfree(pci_name);
#endif

#ifdef GEN3_PHY_OFF
	/*
	 * This patch is to avoid PHY power consumption of GEN3.
	 */
	pci_name = kmalloc(EXYNOS_PCIE_MAX_NAME_LEN, GFP_KERNEL);
	snprintf(pci_name, EXYNOS_PCIE_MAX_NAME_LEN, "pcie1");
	np = of_find_node_by_name(NULL, pci_name);
	if (np == NULL) {
		pr_err("PCIe1 node is NULL!!!\n");
	} else {
		if (of_property_read_string(np, "phy-power-off", &off)) {
			pr_err("Can't Read PCIe1 PHY setting!!!\n");
		} else {
			if (!strncmp(off, "true", 4)) {
				g_gen3_phy_off = true;
				pr_info("PCIe1-GEN3 PHY power will be down by DT(phy-power-off = true).\n");
			} else
				g_gen3_phy_off = false;
		}
	}

	kfree(pci_name);
#endif

#if IS_ENABLED(CONFIG_SOC_EXYNOS2100)
	exynos_gen4_nclkoff();
#endif

	return platform_driver_register(&exynos_pcie_driver);
}
device_initcall(pcie_init);

int exynos_pcie_register_event(struct exynos_pcie_register_event *reg)
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
EXPORT_SYMBOL(exynos_pcie_register_event);

int exynos_pcie_deregister_event(struct exynos_pcie_register_event *reg)
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
EXPORT_SYMBOL(exynos_pcie_deregister_event);

MODULE_AUTHOR("Kisang Lee <kisang80.lee@samsung.com>");
MODULE_AUTHOR("Kwangho Kim <kwangho2.kim@samsung.com>");
MODULE_AUTHOR("Seungo Jang <seungo.jang@samsung.com>");
MODULE_DESCRIPTION("Samsung PCIe host controller driver");
MODULE_LICENSE("GPL v2");
