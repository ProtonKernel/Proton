// SPDX-License-Identifier: GPL-2.0
/*
 * Samsung Exynos SoC series Pablo driver
 *
 * Copyright (c) 2021 Samsung Electronics Co., Ltd
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#ifndef IS_DEVICE_CAMIF_DMA_H
#define IS_DEVICE_CAMIF_DMA_H

#define WDMA_IRQ_NAME_LENGTH	16

/* camera interface WDMA channel */
struct is_camif_wdma_module;

struct is_camif_wdma {
	struct device *dev;
	struct is_camif_dma_data *data;
	int index;
	void __iomem *regs_ctl;
	void __iomem *regs_vc[CSI_VIRTUAL_CH_MAX];
	void __iomem *regs_mux;
	int irq;
	char irq_name[WDMA_IRQ_NAME_LENGTH];
	struct is_camif_wdma_module *mod;
	int active_cnt;
};

/* camera interface WDMA module */
struct is_camif_wdma_module {
	struct device *dev;
	struct is_camif_dma_data *data;
	int index;
	void __iomem *regs;
	unsigned long quirks;
	int num_of_chs;
	struct is_camif_wdma **chs;
	atomic_t active_cnt;
	spinlock_t slock;
};

struct is_camif_dma_data {
	unsigned int type;
	spinlock_t slock;
	char *alias_stem;

	int (*probe)(struct platform_device *pdev,
			struct is_camif_dma_data *data);
	int (*remove)(struct platform_device *pdev,
			struct is_camif_dma_data *data);
};

struct is_camif_wdma *is_camif_wdma_get(unsigned int wdma_ch_hint);
int is_camif_wdma_put(struct is_camif_wdma *wdma);
struct is_camif_wdma_module *is_camif_wdma_module_get(unsigned int wdma_ch);
void is_camif_wdma_init(void);

/* This quirks code is derived from sound/soc/samsung/abox/abox.c */
#define IS_CAMIF_WDMA_MODULE_QUIRK_BIT_HAS_TEST_PATTERN_GEN BIT(1)
#define IS_CAMIF_WDMA_MODULE_QUIRK_STR_HAS_TEST_PATTERN_GEN "has test pattern gen"

static inline bool is_camif_wdma_module_test_quirk(struct is_camif_wdma_module *wm,
							unsigned long quirk)
{
	return !!(wm->quirks & quirk);
}

#endif
