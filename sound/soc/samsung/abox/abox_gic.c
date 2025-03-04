/* sound/soc/samsung/abox/abox_gic.c
 *
 * ALSA SoC Audio Layer - Samsung ABOX GIC driver
 *
 * Copyright (c) 2016 Samsung Electronics Co. Ltd.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#include <linux/clk.h>
#include <linux/io.h>
#include <linux/module.h>
#include <linux/of.h>
#include <linux/interrupt.h>
#include <linux/platform_device.h>
#include <linux/pm_runtime.h>
#include <linux/smc.h>
#include <linux/irqchip/arm-gic.h>
#include <linux/delay.h>

#include <soc/samsung/exynos-smc.h>

#include "abox_util.h"
#include "abox_gic.h"

#define VERBOSE 0

static int gicd_xwritel(struct abox_gic_data *data, u32 value,
		unsigned int offset)
{
	unsigned long arg1;
	int ret = 0;

	if (is_secure_gic()) {
		offset += data->gicd_base_phys;
		arg1 = SMC_REG_ID_SFR_W(offset);
		ret = exynos_smc(SMC_CMD_REG, arg1, value, 0);
		if (ret < 0)
			dev_err(data->dev, "write fail %#x: %d\n", offset, ret);
		if (VERBOSE) {
			unsigned long val = 0xfafafafa;

			exynos_smc_readsfr(offset, &val);
			dev_dbg(data->dev, "%#x %#lx\n", offset, val);
		}
	} else {
		writel(value, data->gicd_base + offset);
	}

	return ret;
}

static int gicc_xwritel(struct abox_gic_data *data, u32 value,
		unsigned int offset)
{
	unsigned long arg1;
	int ret = 0;

	if (is_secure_gic()) {
		offset += data->gicc_base_phys;
		arg1 = SMC_REG_ID_SFR_W(offset);
		ret = exynos_smc(SMC_CMD_REG, arg1, value, 0);
		if (ret < 0)
			dev_err(data->dev, "write fail %#x: %d\n", offset, ret);
		if (VERBOSE) {
			unsigned long val = 0xfafafafa;

			exynos_smc_readsfr(offset, &val);
			dev_dbg(data->dev, "%#x %#lx\n", offset, val);
		}
	} else {
		writel(value, data->gicc_base + offset);
	}

	return ret;
}

static u32 gicd_xreadl(struct abox_gic_data *data, unsigned int offset)
{
	u32 value;

	if (is_secure_gic()) {
		unsigned long val;
		int ret;

		offset += data->gicd_base_phys;
		ret = exynos_smc_readsfr(offset, &val);
		if (ret < 0) {
			dev_err(data->dev, "read fail %#x: %d\n", offset, ret);
			return 0;
		}

		value = (u32)val;

		if (VERBOSE)
			dev_dbg(data->dev, "%#x %#lx\n", offset, val);
	} else {
		value = readl(data->gicd_base + offset);
	}

	return value;
}

static u32 gicc_xreadl(struct abox_gic_data *data, unsigned int offset)
{
	u32 value;

	if (is_secure_gic()) {
		unsigned long val;
		int ret;

		offset += data->gicc_base_phys;
		ret = exynos_smc_readsfr(offset, &val);
		if (ret < 0) {
			dev_err(data->dev, "read fail %#x: %d\n", offset, ret);
			return 0;
		}

		value = (u32)val;

		if (VERBOSE)
			dev_dbg(data->dev, "%#x %#lx\n", offset, val);
	} else {
		value = readl(data->gicc_base + offset);
	}

	return value;
}

int abox_gicd_dump(struct device *dev, char *dump, size_t off, size_t size)
{
	struct abox_gic_data *data = dev_get_drvdata(dev);
	size_t limit = min(off + size, data->gicd_size);
	u32 *buf = (u32 *)dump;

	if (off % 4 || size % 4)
		return -EINVAL;

	for (; off + 3 < limit; off += 4)
		*buf++ = gicd_xreadl(data, off);

	return 0;
}
EXPORT_SYMBOL(abox_gicd_dump);

void abox_gic_enable(struct device *dev, unsigned int irq, bool en)
{
	struct abox_gic_data *data = dev_get_drvdata(dev);
	unsigned int base = en ? GIC_DIST_ENABLE_SET : GIC_DIST_ENABLE_CLEAR;
	unsigned int offset = base + (irq / 32 * 4);
	unsigned int shift = irq % 32;
	unsigned int mask = 0x1 << shift;
	static DEFINE_SPINLOCK(lock);
	unsigned long flags;

	spin_lock_irqsave(&lock, flags);
	gicd_xwritel(data, mask, offset);
	spin_unlock_irqrestore(&lock, flags);
}
EXPORT_SYMBOL(abox_gic_enable);

void abox_gic_target(struct device *dev, unsigned int irq,
		enum abox_gic_target target)
{
	struct abox_gic_data *data = dev_get_drvdata(dev);
	unsigned int offset = GIC_DIST_TARGET + (irq & 0xfffffffc);
	unsigned int shift = (irq & 0x3) * 8;
	unsigned int mask = 0xff << shift;
	unsigned int val;
	static DEFINE_SPINLOCK(lock);
	unsigned long flags;

	dev_dbg(dev, "%s(%d, %d)\n", __func__, irq, target);

	spin_lock_irqsave(&lock, flags);
	val = gicd_xreadl(data, offset);
	val &= ~mask;
	val |= ((0x1 << target) << shift) & mask;
	gicd_xwritel(data, val, offset);
	spin_unlock_irqrestore(&lock, flags);
}
EXPORT_SYMBOL(abox_gic_target);

void abox_gic_generate_interrupt(struct device *dev, unsigned int irq)
{
	struct abox_gic_data *data = dev_get_drvdata(dev);

	dev_dbg(dev, "%s(%d)\n", __func__, irq);

	gicd_xwritel(data, (0x1 << 16) | (irq & 0xf), GIC_DIST_SOFTINT);
}
EXPORT_SYMBOL(abox_gic_generate_interrupt);

int abox_gic_register_irq_handler(struct device *dev, unsigned int irq,
		irq_handler_t handler, void *dev_id)
{
	struct abox_gic_data *data = dev_get_drvdata(dev);

	dev_dbg(dev, "%s(%u, %ps)\n", __func__, irq, handler);

	if (irq >= ARRAY_SIZE(data->handler)) {
		dev_err(dev, "invalid irq: %d\n", irq);
		return -EINVAL;
	}

	WRITE_ONCE(data->handler[irq].handler, handler);
	WRITE_ONCE(data->handler[irq].dev_id, dev_id);

	return 0;
}
EXPORT_SYMBOL(abox_gic_register_irq_handler);

int abox_gic_unregister_irq_handler(struct device *dev, unsigned int irq)
{
	struct abox_gic_data *data = dev_get_drvdata(dev);

	dev_dbg(dev, "%s(%u)\n", __func__, irq);

	if (irq >= ARRAY_SIZE(data->handler)) {
		dev_err(dev, "invalid irq: %d\n", irq);
		return -EINVAL;
	}

	WRITE_ONCE(data->handler[irq].handler, NULL);
	WRITE_ONCE(data->handler[irq].dev_id, NULL);

	return 0;
}
EXPORT_SYMBOL(abox_gic_unregister_irq_handler);

static irqreturn_t __abox_gic_irq_handler(struct abox_gic_data *data, u32 irqnr)
{
	irq_handler_t handler;
	void *dev_id;

	if (irqnr >= ARRAY_SIZE(data->handler))
		return IRQ_NONE;

	dev_id = READ_ONCE(data->handler[irqnr].dev_id);
	handler = READ_ONCE(data->handler[irqnr].handler);
	if (!handler)
		return IRQ_NONE;

	return handler(irqnr, dev_id);
}

static irqreturn_t abox_gic_irq_handler(int irq, void *dev_id)
{
	struct device *dev = dev_id;
	struct abox_gic_data *data = dev_get_drvdata(dev);
	irqreturn_t ret = IRQ_NONE;
	u32 irqstat, irqnr;

	dev_dbg(dev, "%s\n", __func__);

	do {
		irqstat = gicc_xreadl(data, GIC_CPU_INTACK);
		irqnr = irqstat & GICC_IAR_INT_ID_MASK;
		dev_dbg(dev, "IAR: %08X\n", irqstat);

		if (irqnr < 16) {
			gicc_xwritel(data, irqstat, GIC_CPU_EOI);
			gicc_xwritel(data, irqstat, GIC_CPU_DEACTIVATE);
			ret |= __abox_gic_irq_handler(data, irqnr);
			continue;
		} else if (irqnr > 15 && irqnr < 1021) {
			gicc_xwritel(data, irqstat, GIC_CPU_EOI);
			ret |= __abox_gic_irq_handler(data, irqnr);
			continue;
		}
		break;
	} while (1);

	if (ret == IRQ_NONE) {
		if (irqnr < 1021)
			dev_warn(dev, "unhandled irq: %u\n", irqnr);
		ret = IRQ_HANDLED;
	}

	return ret;
}

static void abox_gicd_enable(struct device *dev, bool en)
{
	struct abox_gic_data *data = dev_get_drvdata(dev);
	void __iomem *gicd_base = data->gicd_base;

	writel(en ? 0x1 : 0x0, gicd_base + GIC_DIST_CTRL);
}

void abox_gic_init_gic(struct device *dev)
{
	struct abox_gic_data *data = dev_get_drvdata(dev);

	dev_dbg(dev, "%s\n", __func__);

	gicc_xwritel(data, 0xff, GIC_CPU_PRIMASK);
	gicd_xwritel(data, 0x3, GIC_DIST_CTRL);
	gicc_xwritel(data, 0x3, GIC_CPU_CTRL);
}
EXPORT_SYMBOL(abox_gic_init_gic);

int abox_gic_enable_irq(struct device *dev)
{
	struct abox_gic_data *data = dev_get_drvdata(dev);

	if (likely(data->disabled)) {
		dev_dbg(dev, "%s\n", __func__);

		data->disabled = false;
		enable_irq(data->irq);
		abox_gicd_enable(dev, true);
	}
	return 0;
}
EXPORT_SYMBOL(abox_gic_enable_irq);

int abox_gic_disable_irq(struct device *dev)
{
	struct abox_gic_data *data = dev_get_drvdata(dev);

	if (likely(!data->disabled)) {
		dev_dbg(dev, "%s\n", __func__);

		data->disabled = true;
		disable_irq(data->irq);
	}

	return 0;
}
EXPORT_SYMBOL(abox_gic_disable_irq);

static int samsung_abox_gic_probe(struct platform_device *pdev)
{
	struct device *dev = &pdev->dev;
	struct abox_gic_data *data;
	unsigned long irqflags = IRQF_TRIGGER_RISING;
	int ret;

	dev_dbg(dev, "%s\n", __func__);

	data = devm_kzalloc(dev, sizeof(struct abox_gic_data), GFP_KERNEL);
	if (!data)
		return -ENOMEM;
	platform_set_drvdata(pdev, data);

	data->dev = &pdev->dev;

	data->gicd_base = devm_get_request_ioremap(pdev, "gicd",
			&data->gicd_base_phys, &data->gicd_size);
	if (IS_ERR(data->gicd_base))
		return PTR_ERR(data->gicd_base);

	data->gicc_base = devm_get_request_ioremap(pdev, "gicc",
			&data->gicc_base_phys, &data->gicc_size);
	if (IS_ERR(data->gicc_base))
		return PTR_ERR(data->gicc_base);

	data->irq = platform_get_irq(pdev, 0);
	if (data->irq < 0) {
		dev_err(dev, "Failed to get irq\n");
		return data->irq;
	}

	ret = devm_request_irq(dev, data->irq, abox_gic_irq_handler,
			irqflags, pdev->name, dev);
	if (ret < 0) {
		dev_err(dev, "Failed to request irq\n");
		return ret;
	}

	ret = enable_irq_wake(data->irq);
	if (ret < 0)
		dev_err(dev, "Failed to enable irq wake\n");

	/* Check and cache whether the gic is secure or not */
	is_secure_gic();

	dev_dbg(dev, "%s: probe complete\n", __func__);

	return 0;
}

static int samsung_abox_gic_remove(struct platform_device *pdev)
{
	dev_dbg(&pdev->dev, "%s\n", __func__);

	return 0;
}

static const struct of_device_id samsung_abox_gic_of_match[] = {
	{
		.compatible = "samsung,abox-gic",
	},
	{},
};
MODULE_DEVICE_TABLE(of, samsung_abox_gic_of_match);

static struct platform_driver samsung_abox_gic_driver = {
	.probe  = samsung_abox_gic_probe,
	.remove = samsung_abox_gic_remove,
	.driver = {
		.name = "abox-gic",
		.owner = THIS_MODULE,
		.of_match_table = of_match_ptr(samsung_abox_gic_of_match),
	},
};

module_platform_driver(samsung_abox_gic_driver);

/* Module information */
MODULE_AUTHOR("Gyeongtaek Lee, <gt82.lee@samsung.com>");
MODULE_DESCRIPTION("Samsung ASoC A-Box GIC Driver");
MODULE_ALIAS("platform:abox-gic");
MODULE_LICENSE("GPL");
