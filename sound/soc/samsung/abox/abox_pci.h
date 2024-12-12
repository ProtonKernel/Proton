/* sound/soc/samsung/abox/abox_pci.h
 *
 * ALSA SoC - Samsung Abox PCI driver
 *
 * Copyright (c) 2019 Samsung Electronics Co. Ltd.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#ifndef __SND_SOC_ABOX_PCI_H
#define __SND_SOC_ABOX_PCI_H

#include "abox.h"

struct abox_pci_data {
	struct device *dev;
	unsigned int id;
	struct device *dev_abox;
	void *pci_dram_base;
	void *pci_doorbell_base;
	phys_addr_t pci_doorbell_base_phys;
	struct pinctrl *pinctrl;
	struct abox_data *abox_data;
	u32 abox_pci_s2mpu_vts_mailbox_base;
};

#endif /* __SND_SOC_ABOX_PCI_H */
