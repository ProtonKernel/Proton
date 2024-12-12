// SPDX-License-Identifier: GPL-2.0
/**
 * host.c - DesignWare USB3 DRD Controller Host Glue
 *
 * Copyright (C) 2011 Texas Instruments Incorporated - http://www.ti.com
 *
 * Authors: Felipe Balbi <balbi@ti.com>,
 */

#include <linux/platform_device.h>
#ifdef CONFIG_SND_EXYNOS_USB_AUDIO
#include <linux/usb/exynos_usb_audio.h>
#endif

#include "core.h"

#ifdef CONFIG_SND_EXYNOS_USB_AUDIO
struct host_data xhci_data;
EXPORT_SYMBOL_GPL(xhci_data);
#endif

struct usb_xhci_pre_alloc {
	u8 *pre_dma_alloc;
	u64 offset;

	dma_addr_t	dma;
};

struct usb_xhci_pre_alloc xhci_pre_alloc;
EXPORT_SYMBOL_GPL(xhci_pre_alloc);

static int dwc3_host_get_irq(struct dwc3 *dwc)
{
	struct platform_device	*dwc3_pdev = to_platform_device(dwc->dev);
	int irq;

	irq = platform_get_irq_byname_optional(dwc3_pdev, "host");
	if (irq > 0)
		goto out;

	if (irq == -EPROBE_DEFER)
		goto out;

	irq = platform_get_irq_byname_optional(dwc3_pdev, "dwc_usb3");
	if (irq > 0)
		goto out;

	if (irq == -EPROBE_DEFER)
		goto out;

	irq = platform_get_irq(dwc3_pdev, 0);
	if (irq > 0)
		goto out;

	if (!irq)
		irq = -EINVAL;

out:
	return irq;
}

int dwc3_host_init(struct dwc3 *dwc)
{
	struct property_entry	props[4];
	struct platform_device	*xhci;
	int			ret, irq;
	struct resource		*res;
	struct platform_device	*dwc3_pdev = to_platform_device(dwc->dev);
	int			prop_idx = 0;
#ifdef CONFIG_SND_EXYNOS_USB_AUDIO
	dma_addr_t	dma;
#endif

	irq = dwc3_host_get_irq(dwc);
	if (irq < 0)
		return irq;

	res = platform_get_resource_byname(dwc3_pdev, IORESOURCE_IRQ, "host");
	if (!res)
		res = platform_get_resource_byname(dwc3_pdev, IORESOURCE_IRQ,
				"dwc_usb3");
	if (!res)
		res = platform_get_resource(dwc3_pdev, IORESOURCE_IRQ, 0);
	if (!res)
		return -ENOMEM;

	dwc->xhci_resources[1].start = irq;
	dwc->xhci_resources[1].end = irq;
	dwc->xhci_resources[1].flags = res->flags;
	dwc->xhci_resources[1].name = res->name;

	xhci = platform_device_alloc("xhci-hcd", PLATFORM_DEVID_AUTO);
	if (!xhci) {
		dev_err(dwc->dev, "couldn't allocate xHCI device\n");
		return -ENOMEM;
	}

	xhci->dev.parent	= dwc->dev;
	//xhci->dev.dma_mask	= dwc->dev->dma_mask;
	//xhci->dev.dma_parms	= dwc->dev->dma_parms;
	//xhci->dev.archdata.dma_ops = dwc->dev->archdata.dma_ops;

	dma_set_coherent_mask(&xhci->dev, dwc->dev->coherent_dma_mask);

	dwc->xhci = xhci;

	ret = platform_device_add_resources(xhci, dwc->xhci_resources,
						DWC3_XHCI_RESOURCES_NUM);
	if (ret) {
		dev_err(dwc->dev, "couldn't add resources to xHCI device\n");
		goto err;
	}

	memset(props, 0, sizeof(struct property_entry) * ARRAY_SIZE(props));

	if (dwc->usb3_lpm_capable)
		props[prop_idx++] = PROPERTY_ENTRY_BOOL("usb3-lpm-capable");

	if (dwc->usb2_lpm_disable)
		props[prop_idx++] = PROPERTY_ENTRY_BOOL("usb2-lpm-disable");

	/* pre dma_alloc */
	xhci_pre_alloc.pre_dma_alloc = dma_alloc_coherent(dwc->dev, SZ_2M,
					&xhci_pre_alloc.dma, GFP_KERNEL);
	if (!xhci_pre_alloc.pre_dma_alloc) {
		dev_err(dwc->dev, "%s: dma_alloc fail!!!!\n", __func__);
		goto err;
	}

	/**
	 * WORKAROUND: dwc3 revisions <=3.00a have a limitation
	 * where Port Disable command doesn't work.
	 *
	 * The suggested workaround is that we avoid Port Disable
	 * completely.
	 *
	 * This following flag tells XHCI to do just that.
	 */
	if (dwc->revision <= DWC3_REVISION_300A)
		props[prop_idx++] = PROPERTY_ENTRY_BOOL("quirk-broken-port-ped");

	if (prop_idx) {
		ret = platform_device_add_properties(xhci, props);
		if (ret) {
			dev_err(dwc->dev, "failed to add properties to xHCI\n");
			goto err;
		}
	}

#ifdef CONFIG_SND_EXYNOS_USB_AUDIO
	/* In data buf alloc */
	xhci_data.in_data_addr = dma_alloc_coherent(dwc->dev,
			(PAGE_SIZE * 256), &dma, GFP_KERNEL);
	xhci_data.in_data_dma = dma;
	dev_info(dwc->dev, "// IN Data address = 0x%llx (DMA), %p (virt)",
		(unsigned long long)xhci_data.in_data_dma, xhci_data.in_data_addr);

	/* Out data buf alloc */
	xhci_data.out_data_addr = dma_alloc_coherent(dwc->dev,
			(PAGE_SIZE * 256), &dma, GFP_KERNEL);
	xhci_data.out_data_dma = dma;
	dev_info(dwc->dev, "// OUT Data address = 0x%llx (DMA), %p (virt)",
		(unsigned long long)xhci_data.out_data_dma, xhci_data.out_data_addr);
#endif

#if 0 // block that host enable is called in booting time
	ret = platform_device_add(xhci);
	if (ret) {
		dev_err(dwc->dev, "failed to register xHCI device\n");
		goto err;
	}
#endif

	return 0;
err:
	platform_device_put(xhci);
	return ret;
}

void dwc3_host_exit(struct dwc3 *dwc)
{
	platform_device_unregister(dwc->xhci);
	dwc->xhci = NULL;
}
