/* sound/soc/samsung/abox/abox_pci.c
 *
 * ALSA SoC Audio Layer - Samsung Abox PCI driver
 *
 * Copyright (c) 2019 Samsung Electronics Co. Ltd.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

/* #define DEBUG */
#include <linux/io.h>
#include <linux/device.h>
#include <linux/debugfs.h>
#include <linux/module.h>
#include <linux/iommu.h>
#include <linux/of_reserved_mem.h>
#include <linux/pm_runtime.h>
#include <linux/sched/clock.h>
#include <linux/mm_types.h>
#include <asm/cacheflush.h>
#include <linux/pinctrl/consumer.h>
#include "abox_pci.h"
#if IS_ENABLED(CONFIG_LINK_DEVICE_PCIE_S2MPU)
#include <soc/samsung/exynos-s2mpu.h>
#endif

static struct reserved_mem *abox_pci_rmem;
static struct abox_pci_data *p_abox_pci_data;

static int __init abox_pci_rmem_setup(struct reserved_mem *rmem)
{
	pr_info("%s: base=%pa, size=%pa\n", __func__, &rmem->base, &rmem->size);
	abox_pci_rmem = rmem;
	return 0;
}

RESERVEDMEM_OF_DECLARE(abox_rmem, "exynos,abox_pci_rmem", abox_pci_rmem_setup);

static void *abox_rmem_pci_phys_addr_vmap(phys_addr_t addr_phys,
		size_t addr_size)
{
	phys_addr_t phys = addr_phys;
	size_t size = addr_size;
	unsigned int num_pages = (unsigned int)DIV_ROUND_UP(size, PAGE_SIZE);
	pgprot_t prot = pgprot_writecombine(PAGE_KERNEL);
	struct page **pages, **page;
	void *vaddr = NULL;

	pages = kcalloc(num_pages, sizeof(pages[0]), GFP_KERNEL);
	if (!pages)
		goto out;

	for (page = pages; (page - pages < num_pages); page++) {
		*page = phys_to_page(phys);
		phys += PAGE_SIZE;
	}

	vaddr = vmap(pages, num_pages, VM_MAP, prot);
	kfree(pages);
out:
	return vaddr;
}

static void *abox_rmem_pci_vmap(struct reserved_mem *rmem)
{
	phys_addr_t phys = rmem->base;
	size_t size = rmem->size;

	return abox_rmem_pci_phys_addr_vmap(phys, size);
}

static int abox_pci_cfg_gpio(struct device *dev, const char *name)
{
	struct abox_pci_data *data = dev_get_drvdata(dev);
	struct pinctrl_state *pin_state;
	int ret = 0;

	dev_info(dev, "%s(%s)\n", __func__, name);

	pin_state = pinctrl_lookup_state(data->pinctrl, name);
	if (IS_ERR(pin_state)) {
		dev_err(dev, "Couldn't find pinctrl %s\n", name);
	} else {
		ret = pinctrl_select_state(data->pinctrl, pin_state);
		if (ret < 0)
			dev_err(dev, "Unable to configure pinctrl %s\n", name);
	}

	return ret;
}

bool abox_pci_doorbell_paddr_set(phys_addr_t addr)
{
	struct abox_pci_data *data = p_abox_pci_data;

	if (data == NULL)
		return false;

	dev_info(data->dev, "%s\n", __func__);

	data->pci_doorbell_base_phys = addr + ABOX_PCI_DOORBELL_OFFSET;
	data->pci_dram_base = devm_ioremap(data->dev_abox,
			data->pci_doorbell_base_phys, ABOX_PCI_DOORBELL_SIZE);
	abox_iommu_map(data->dev_abox, IOVA_VSS_PCI_DOORBELL,
			data->pci_doorbell_base_phys,
			ABOX_PCI_DOORBELL_SIZE, data->pci_dram_base);

	return true;
}
EXPORT_SYMBOL(abox_pci_doorbell_paddr_set);

static int samsung_abox_pci_probe(struct platform_device *pdev)
{
	struct device *dev = &pdev->dev;
	struct abox_pci_data *data;
#if defined(CONFIG_LINK_DEVICE_PCIE_S2MPU)
	struct device_node *np = dev->of_node;
#endif
	int ret = 0;

	data = devm_kzalloc(dev, sizeof(*data), GFP_KERNEL);
	if (!data)
		return -ENOMEM;

	platform_set_drvdata(pdev, data);
	data->dev = dev;
	p_abox_pci_data = data;

	data->dev_abox = pdev->dev.parent;
	if (!data->dev_abox) {
		dev_err(dev, "Failed to get abox device\n");
		return -EPROBE_DEFER;
	}
	data->abox_data = dev_get_drvdata(data->dev_abox);

	if (!abox_pci_rmem) {
		struct device_node *np_tmp;

		np_tmp = of_parse_phandle(dev->of_node, "memory-region", 0);
		if (np_tmp)
			abox_pci_rmem = of_reserved_mem_lookup(np_tmp);
	}
	if (abox_pci_rmem) {
		data->pci_dram_base = abox_rmem_pci_vmap(abox_pci_rmem);
		abox_iommu_map(data->dev_abox, IOVA_VSS_PCI,
				abox_pci_rmem->base, abox_pci_rmem->size,
				data->pci_dram_base);
		memset(data->pci_dram_base, 0x0, abox_pci_rmem->size);
#if defined(CONFIG_LINK_DEVICE_PCIE_S2MPU)
		ret = (int) exynos_set_dev_stage2_ap("hsi2", 0,
				abox_pci_rmem->base, /* phys ?? */
				abox_pci_rmem->size,
				ATTR_RW);
		if (ret < 0)
			dev_err(dev, "Failed to exynos_set_dev_stage2_ap(%d): %d\n",
				__LINE__, ret);
#endif
	}

/* HACK: vts mailbox */
#if defined(CONFIG_LINK_DEVICE_PCIE_S2MPU)
	ret = of_property_read_u32(np, "s2mpu_mailbox",
			&data->abox_pci_s2mpu_vts_mailbox_base);
	if (ret) {
		dev_err(dev, "can't get mailbox base for S2MPU\n");

		return -EINVAL;
	}

	dev_info(dev, "S2MPU (%d)\n", data->abox_pci_s2mpu_vts_mailbox_base);
	ret = (int) exynos_set_dev_stage2_ap("hsi2", 0,
			data->abox_pci_s2mpu_vts_mailbox_base,
			SZ_4K, ATTR_RW);
	if (ret < 0) {
		dev_err(dev, "Failed to exynos_set_dev_stage2_ap(%d): %d\n",
			__LINE__, ret);

		return -EINVAL;
	}
#endif

	data->pinctrl = devm_pinctrl_get(dev);
	if (IS_ERR(data->pinctrl)) {
		dev_err(dev, "Couldn't get pins (%li)\n",
				PTR_ERR(data->pinctrl));
		return -EINVAL;
	}

	ret = abox_pci_cfg_gpio(data->dev, "pci_on");
	if (ret < 0)
		dev_err(dev, "Failed to turn on pci gpio cfg(%d): %d\n",
			__LINE__, ret);

	dev_info(dev, "%s (%d)\n", __func__, __LINE__);
	dev_info(data->dev_abox, "%s (%d)\n", __func__, __LINE__);

	return ret;
}

static int samsung_abox_pci_remove(struct platform_device *pdev)
{
	struct device *dev = &pdev->dev;

	dev_dbg(dev, "%s\n", __func__);

	return 0;
}

static const struct of_device_id samsung_abox_pci_match[] = {
	{
		.compatible = "samsung,abox-pci",
	},
	{},
};
MODULE_DEVICE_TABLE(of, samsung_abox_pci_match);

struct platform_driver samsung_abox_pci_driver = {
	.probe  = samsung_abox_pci_probe,
	.remove = samsung_abox_pci_remove,
	.driver = {
		.name = "abox-pci",
		.owner = THIS_MODULE,
		.of_match_table = of_match_ptr(samsung_abox_pci_match),
	},
};
