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

#include <linux/of_platform.h>

#include "is-hw.h"
#include "is-device-camif-dma.h"

#define CAMIF_WDMA		0x01
#define CAMIF_WDMA_MODULE	0x02

static struct is_camif_wdma **wdma_chs;
static int max_num_of_wdma;
static struct is_camif_dma_data is_camif_wdma;

struct is_camif_wdma *is_camif_wdma_get(unsigned int wdma_ch_hint)
{
	int i = 0;
	struct is_camif_wdma *wdma;
	unsigned long flags;

	spin_lock_irqsave(&is_camif_wdma.slock, flags);

	if (wdma_ch_hint >= max_num_of_wdma) {
		for (i = 0; i < max_num_of_wdma; i++) {
			wdma = wdma_chs[i];
			if (!wdma->active_cnt) {
				wdma->active_cnt++;
				break;
			}
		}
	} else {
		wdma = wdma_chs[wdma_ch_hint];
		if (!wdma->active_cnt)
			wdma->active_cnt++;
		else
			i = max_num_of_wdma; /* error detect */
	}

	spin_unlock_irqrestore(&is_camif_wdma.slock, flags);

	if (i >= max_num_of_wdma) {
		err("all wdmas are already used (hint %d)", wdma_ch_hint);
		return NULL;
	}

	info("%s: ch(mod%d, dma%d) hint %d\n", __func__, wdma->mod->index,
		wdma->index, wdma_ch_hint);

	return wdma;
}

int is_camif_wdma_put(struct is_camif_wdma *wdma)
{
	int active_cnt;
	unsigned long flags;

	if (!wdma)
		return -EINVAL;

	spin_lock_irqsave(&is_camif_wdma.slock, flags);

	active_cnt = wdma->active_cnt;
	wdma->active_cnt = 0;

	spin_unlock_irqrestore(&is_camif_wdma.slock, flags);

	if (active_cnt != 1) {
		warn("invalid active_cnt(%d) ch(mod%d, dma%d)",
			active_cnt, wdma->mod->index, wdma->index);
		return 0;
	}

	info("%s: ch(mod%d, dma%d)\n", __func__, wdma->mod->index, wdma->index);

	return 0;
}

struct is_camif_wdma_module *is_camif_wdma_module_get(unsigned int wdma_ch)
{
	struct is_camif_wdma_module *wdma_mod;

	if (wdma_ch >= max_num_of_wdma) {
		err(" invalid wdma_ch(%d)", wdma_ch);
		return NULL;
	}

	wdma_mod = wdma_chs[wdma_ch]->mod;
	if (!wdma_mod)
		warn(" wdma_module is null");

	return wdma_mod;
}

void is_camif_wdma_init(void)
{
	int i;
	struct is_camif_wdma *wdma;

	for (i = 0; i < max_num_of_wdma; i++) {
		wdma = wdma_chs[i];
		if (!wdma)
			continue;

		writel(0xFFFFFFFF, wdma->regs_mux);
		wdma->active_cnt = 0;
	}
}

static void __iomem *is_camif_ioremap(struct platform_device *pdev,
					const char *name)
{
	struct device *dev = &pdev->dev;
	struct resource *res;

	res = platform_get_resource_byname(pdev, IORESOURCE_MEM, name);
	if (!res) {
		dev_err(dev, "failed to get memory resource for %s", name);
		return ERR_PTR(-ENODEV);
	}

	return devm_ioremap(dev, res->start, resource_size(res));
}

static int is_camif_wdma_probe(struct platform_device *pdev,
				struct is_camif_dma_data *data)
{
	struct device *dev = &pdev->dev;
	struct device_node *node = dev->of_node;
	struct is_camif_wdma *wdma;
	int ret, vc;
	char *name;
	size_t name_len;
	struct device_node *module_node;
	struct platform_device *module_pdev;
	struct is_camif_wdma_module *wdma_module;

	if (data->type != CAMIF_WDMA) {
		dev_err(dev, "invalid probe function\n");
		return -EINVAL;
	}

	wdma = devm_kzalloc(dev, sizeof(*wdma), GFP_KERNEL);
	if (!wdma) {
		dev_err(dev, "failed to allocate driver data\n");
		return -ENOMEM;
	}

	wdma->dev = &pdev->dev;
	wdma->data = data;
	wdma->index = of_alias_get_id(node, data->alias_stem);
	if ((wdma->index < 0) || (wdma->index >= (max_num_of_wdma))) {
		dev_err(dev, "invalid global index for wdma: %d\n", wdma->index);
		ret = wdma->index;
		goto err_get_global_index;
	}

	/* DMA input mux register */
	wdma->regs_mux = is_camif_ioremap(pdev, "mux");
	if (IS_ERR_OR_NULL(wdma->regs_mux)) {
		dev_err(dev, "couldn't ioremap for wdma%d mux\n", wdma->index);
		ret = -ENOMEM;
		goto err_ioremap_mux;
	}

	/* control register */
	wdma->regs_ctl = is_camif_ioremap(pdev, "ctl");
	if (IS_ERR_OR_NULL(wdma->regs_ctl)) {
		dev_err(dev, "couldn't ioremap for wdma%d control\n", wdma->index);
		ret = -ENOMEM;
		goto err_ioremap_ctl;
	}

	/* each VC register */
	name = __getname();
	if (!name) {
		ret = -ENOMEM;
		goto err_alloc_name;
	}

	for (vc = 0; vc < CSI_VIRTUAL_CH_MAX; vc++) {
		snprintf(name, PATH_MAX, "vc%d", vc);
		wdma->regs_vc[vc] = is_camif_ioremap(pdev, name);
		if (IS_ERR_OR_NULL(wdma->regs_vc[vc])) {
			dev_err(dev, "couldn't ioremap for wdma%d vc%d\n", wdma->index, vc);
			ret = -ENOMEM;
			goto err_ioremap_vc;
		}
	}

	wdma->irq = platform_get_irq_byname(pdev, "dma");
	if (wdma->irq < 0) {
		dev_err(dev, "couldn't get IRQ for wdma%d\n", wdma->index);
		ret = wdma->irq;
		goto err_get_irq;
	}

	name_len = sizeof(wdma->irq_name);
	snprintf(wdma->irq_name, name_len, "%s-%d", "CAMIF.DMA", wdma->index);

	wdma_chs[wdma->index] = wdma;

	/* link wdma module and each channel */
	module_node = of_parse_phandle(node, "module", 0);
	if (!module_node) {
		dev_err(dev, "failed to find module_node\n");
		ret = -ENODEV;
		goto err_find_module_node;
	}

	module_pdev = of_find_device_by_node(module_node);
	if (!module_pdev) {
		dev_err(dev, "failed to find module_pdev\n");
		ret = -ENODEV;
		goto err_find_module_pdev;
	}

	wdma_module = (struct is_camif_wdma_module *)platform_get_drvdata(module_pdev);
	if (!wdma_module) {
		dev_err(dev, "failed to get wdma_module\n");
		ret = -EINVAL;
		goto err_wdma_module;
	}
	wdma_module->chs[wdma->index] = wdma;
	wdma->mod = wdma_module;

	of_node_put(module_node);

	__putname(name);

	dev_info(dev, "WDMA%d was added successfully\n", wdma->index);

	platform_set_drvdata(pdev, wdma);

	return 0;

err_wdma_module:
err_find_module_pdev:
	of_node_put(module_node);

err_find_module_node:
err_get_irq:
err_ioremap_vc:
	__putname(name);

	while (vc-- > 0)
		devm_iounmap(dev, wdma->regs_vc[vc]);

err_alloc_name:
	devm_iounmap(dev, wdma->regs_ctl);

err_ioremap_ctl:
	devm_iounmap(dev, wdma->regs_mux);

err_ioremap_mux:
err_get_global_index:
	devm_kfree(dev, wdma);

	return ret;
}

static struct is_camif_dma_data is_camif_wdma = {
	.type = CAMIF_WDMA,
	.slock = __SPIN_LOCK_UNLOCKED(is_camif_wdma.slock),
	.alias_stem = "wdma",

	.probe = is_camif_wdma_probe,
};

/* This quirks code is derived from sound/soc/samsung/abox/abox.c */
static void is_camif_wdma_module_probe_quirks(struct device_node *node,
					struct is_camif_wdma_module *wm)
{
	#define QUIRKS "samsung,quirks"
	#define DEC_MAP(id) {IS_CAMIF_WDMA_MODULE_QUIRK_STR_##id, \
				IS_CAMIF_WDMA_MODULE_QUIRK_BIT_##id}

	static const struct {
		const char *str;
		unsigned long bit;
	} map[] = {
		DEC_MAP(HAS_TEST_PATTERN_GEN),
	};

	int i;

	for (i = 0; i < ARRAY_SIZE(map); i++) {
		if (of_property_match_string(node, QUIRKS, map[i].str) >= 0) {
			wm->quirks |= map[i].bit;
			dev_info(wm->dev, " %s\n", map[i].str);
		}
	}
}

static int is_camif_wdma_module_probe(struct platform_device *pdev,
					struct is_camif_dma_data *data)
{
	struct device *dev = &pdev->dev;
	struct device_node *node = dev->of_node;
	struct is_camif_wdma_module *wdma_module;
	int ret;
	int num_of_chs;;

	if (data->type != CAMIF_WDMA_MODULE) {
		dev_err(dev, "invalid probe function\n");
		return -EINVAL;
	}

	wdma_module = devm_kzalloc(dev, sizeof(*wdma_module), GFP_KERNEL);
	if (!wdma_module) {
		dev_err(dev, "failed to allocate driver data\n");
		return -ENOMEM;
	}

	atomic_set(&wdma_module->active_cnt, 0);
	spin_lock_init(&wdma_module->slock);

	wdma_module->dev = &pdev->dev;
	wdma_module->data = data;
	wdma_module->index = of_alias_get_id(node, data->alias_stem);
	if (wdma_module->index < 0) {
		dev_err(dev, "invalid global index for wdma-module\n");
		ret = wdma_module->index;
		goto err_get_global_index;
	}

	wdma_module->regs = is_camif_ioremap(pdev, "ctl");
	if (IS_ERR_OR_NULL(wdma_module->regs)) {
		dev_err(dev, "couldn't ioremap for wdma-module%d\n", wdma_module->index);
		ret = -ENOMEM;
		goto err_ioremap;
	}

	if (!of_find_property(node, "channels", NULL)) {
		dev_err(dev, "a wdma-module has no channel\n");
		ret = -EINVAL;
		goto err_no_chs;
	}

	num_of_chs = of_count_phandle_with_args(node, "channels", NULL);
	wdma_module->num_of_chs = num_of_chs;

	wdma_module->chs = devm_kzalloc(dev, sizeof(*(wdma_module->chs)) * num_of_chs,
					GFP_KERNEL);
	if (!wdma_module->chs) {
		dev_err(dev, "failed to allocate wdma_module->chs\n");
		ret = -ENOMEM;
		goto err_alloc_chs;
	}

	max_num_of_wdma = of_alias_get_highest_id(is_camif_wdma.alias_stem) + 1;
	if (!wdma_chs) {
		wdma_chs = devm_kzalloc(dev, sizeof(*wdma_chs) * max_num_of_wdma,
					GFP_KERNEL);
		if (!wdma_chs) {
			dev_err(dev, "failed to allocate wdma_chs\n");
			ret = -ENOMEM;
			goto err_alloc_wdma_chs;
		}
	}

	dev_info(dev, "WDMA-MODULE%d was added successfully with\n", wdma_module->index);

	is_camif_wdma_module_probe_quirks(node, wdma_module);

	platform_set_drvdata(pdev, wdma_module);

	return 0;

err_alloc_wdma_chs:
	devm_kfree(dev, wdma_module->chs);

err_alloc_chs:
err_no_chs:
err_ioremap:
err_get_global_index:
	devm_kfree(dev, wdma_module);

	return ret;
}

static struct is_camif_dma_data is_camif_wdma_module = {
	.type = CAMIF_WDMA_MODULE,
	.slock = __SPIN_LOCK_UNLOCKED(is_camif_wdma_module.slock),
	.alias_stem = "wdma-module",

	.probe = is_camif_wdma_module_probe,
};

static const struct of_device_id is_camif_dma_of_table[] = {
	{
		.name = "camif-wdma",
		.compatible = "samsung,camif-wdma",
		.data = &is_camif_wdma,
	},
	{
		.name = "camif-wdma-module",
		.compatible = "samsung,camif-wdma-module",
		.data = &is_camif_wdma_module,
	},
	{},
};
MODULE_DEVICE_TABLE(of, is_camif_dma_of_table);

static int is_camif_dma_suspend(struct device *dev)
{
	return 0;
}

static int is_camif_dma_resume(struct device *dev)
{
	return 0;
}

int is_camif_dma_runtime_suspend(struct device *dev)
{
	return 0;
}

int is_camif_dma_runtime_resume(struct device *dev)
{
	return 0;
}

static const struct dev_pm_ops is_camif_dma_pm_ops = {
	.suspend		= is_camif_dma_suspend,
	.resume			= is_camif_dma_resume,
	.runtime_suspend	= is_camif_dma_runtime_suspend,
	.runtime_resume		= is_camif_dma_runtime_resume,
};

static int is_camif_dma_probe(struct platform_device *pdev)
{
	struct device *dev = &pdev->dev;
	const struct of_device_id *of_id;
	struct is_camif_dma_data *data;

	of_id = of_match_device(of_match_ptr(is_camif_dma_of_table), dev);
	if (!of_id)
		return -EINVAL;

	data = (struct is_camif_dma_data *)of_id->data;

	return data->probe(pdev, data);
}

static int is_camif_dma_remove(struct platform_device *pdev)
{
	struct device *dev = &pdev->dev;
	const struct of_device_id *of_id;
	struct is_camif_dma_data *data;

	of_id = of_match_device(of_match_ptr(is_camif_dma_of_table), dev);
	if (!of_id)
		return -EINVAL;

	data = (struct is_camif_dma_data *)of_id->data;

	return data->remove(pdev, data);
}

struct platform_driver is_camif_dma_driver = {
	.probe = is_camif_dma_probe,
	.remove = is_camif_dma_remove,
	.driver = {
		.name	= "is-camif-dma",
		.owner	= THIS_MODULE,
		.of_match_table = is_camif_dma_of_table,
		.pm	= &is_camif_dma_pm_ops,
	}
};

#ifndef MODULE
static int __init is_camif_dma_init(void)
{
	int ret;

	ret = platform_driver_probe(&is_camif_dma_driver,
		is_camif_dma_probe);
	if (ret)
		pr_err("failed to probe %s driver: %d\n",
			"is-camif-dma", ret);

	return ret;
}

device_initcall_sync(is_camif_dma_init);
#endif

MODULE_DESCRIPTION("Samsung EXYNOS SoC Pablo CAMIF DMA driver");
MODULE_ALIAS("platform:samsung-is-camif-dma");
MODULE_LICENSE("GPL v2");
