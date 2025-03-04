/*
 * Copyright (c) 2016 Samsung Electronics Co., Ltd.
 *		http://www.samsung.com
 *
 * Samsung EXYNOS8 SoC series DMA DSIM Fast Command driver
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
*/

#include <linux/module.h>
#include <linux/clk-provider.h>
#include <linux/clk.h>
#include <linux/of.h>
#include <linux/iommu.h>
#include <linux/console.h>

#include "fcmd.h"
#if defined(SYSFS_UNITTEST_INTERFACE)
#include "sysfs_error.h"
#endif

int fcmd_log_level = 6;
module_param(fcmd_log_level, int, 0644);

struct fcmd_device *fcmd_drvdata[MAX_FCMD_CNT];

void fcmd_dump(struct fcmd_device *fcmd)
{
	int acquired = console_trylock();

	__fcmd_dump(fcmd->id, fcmd->res.regs);

	if (acquired)
		console_unlock();
}

static int fcmd_set_config(struct fcmd_device *fcmd)
{
	int ret = 0;

	mutex_lock(&fcmd->lock);

	fcmd_reg_init(fcmd->id);
	fcmd_reg_set_config(fcmd->id);

	fcmd_dbg("fcmd%d configuration\n", fcmd->id);

	mutex_unlock(&fcmd->lock);
	return ret;
}

static int fcmd_start(struct fcmd_device *fcmd)
{
	int ret = 0;

	mutex_lock(&fcmd->lock);

	fcmd_reg_set_start(fcmd->id);
	enable_irq(fcmd->res.irq);
	fcmd_reg_irq_enable(fcmd->id);

	fcmd_dbg("fcmd%d is started\n", fcmd->id);

	mutex_unlock(&fcmd->lock);
	return ret;
}

static int fcmd_stop(struct fcmd_device *fcmd)
{
	int ret = 0;

	mutex_lock(&fcmd->lock);

	ret = fcmd_reg_deinit(fcmd->id, 1);
	if (ret)
		fcmd_err("%s: fcmd%d stop error\n", __func__, fcmd->id);
	disable_irq(fcmd->res.irq);

	fcmd_dbg("fcmd%d is stopped\n", fcmd->id);

	mutex_unlock(&fcmd->lock);
	return ret;
}
static int fcmd_parse_dt(struct fcmd_device *fcmd, struct device *dev)
{
	struct device_node *node = dev->of_node;

	if (IS_ERR_OR_NULL(dev->of_node)) {
		fcmd_warn("no device tree information\n");
		return -EINVAL;
	}

	fcmd->id = of_alias_get_id(dev->of_node, "fcmd");
	fcmd_info("fcmd(%d) probe start..\n", fcmd->id);
	of_property_read_u32(node, "port", (u32 *)&fcmd->port);
	fcmd_info("AXI port = %d\n", fcmd->port);

	fcmd->dev = dev;

	return 0;
}

static irqreturn_t fcmd_irq_handler(int irq, void *priv)
{
	struct fcmd_device *fcmd = priv;
	u32 irqs;

	spin_lock(&fcmd->slock);

	irqs = fcmd_reg_get_irq_and_clear(fcmd->id);

	if (irqs & DSIMFC_CONFIG_ERR_IRQ) {
		fcmd_err("fcmd%d config error irq occurs\n", fcmd->id);
		goto irq_end;
	}
	if (irqs & DSIMFC_READ_SLAVE_ERR_IRQ) {
		fcmd_err("fcmd%d read slave error irq occurs\n", fcmd->id);
		fcmd_dump(fcmd);
		goto irq_end;
	}
	if (irqs & DSIMFC_DEADLOCK_IRQ) {
		fcmd_err("fcmd%d deadlock irq occurs\n", fcmd->id);
		fcmd_dump(fcmd);
		goto irq_end;
	}
	if (irqs & DSIMFC_FRAME_DONE_IRQ) {
		fcmd_dbg("fcmd%d frame done irq occurs\n", fcmd->id);
		fcmd->fcmd_config->done = 1;
		wake_up_interruptible_all(&fcmd->xferdone_wq);
		goto irq_end;
	}

irq_end:
	spin_unlock(&fcmd->slock);
	return IRQ_HANDLED;
}

static int fcmd_init_resources(struct fcmd_device *fcmd, struct platform_device *pdev)
{
	struct resource *res;
	int ret;

	res = platform_get_resource(pdev, IORESOURCE_MEM, 0);
	if (!res) {
		fcmd_err("failed to get mem resource\n");
		return -ENOENT;
	}
	fcmd_info("res: start(0x%x), end(0x%x)\n",
			(u32)res->start, (u32)res->end);

	fcmd->res.regs = devm_ioremap_resource(fcmd->dev, res);
	if (IS_ERR_OR_NULL(fcmd->res.regs)) {
		fcmd_err("failed to remap fcmd sfr region\n");
		return -EINVAL;
	}

	res = platform_get_resource(pdev, IORESOURCE_IRQ, 0);
	if (!res) {
		fcmd_err("failed to get fcmd irq resource\n");
		return -ENOENT;
	}
	fcmd_info("fcmd irq no = %lld\n", res->start);

	fcmd->res.irq = res->start;
	ret = devm_request_irq(fcmd->dev, res->start, fcmd_irq_handler, 0,
			pdev->name, fcmd);
	if (ret) {
		fcmd_err("failed to install fcmd irq\n");
		return -EINVAL;
	}
	disable_irq(fcmd->res.irq);

	return 0;
}

static long fcmd_ioctl(struct v4l2_subdev *sd, unsigned int cmd, void *arg)
{
	struct fcmd_device *fcmd = v4l2_get_subdevdata(sd);
	int ret = 0;

	switch (cmd) {
	case FCMD_SET_CONFIG:
		if (!arg) {
			fcmd_err("failed to get fcmd_config\n");
			ret = -EINVAL;
			break;
		}
		fcmd->fcmd_config = (struct fcmd_config *)arg;
		ret = fcmd_set_config(fcmd);
		if (ret)
			fcmd_err("failed to configure fcmd%d\n", fcmd->id);
		break;

	case FCMD_START:
		ret = fcmd_start(fcmd);
		if (ret)
			fcmd_err("failed to start fcmd%d\n", fcmd->id);
		break;

	case FCMD_STOP:
		ret = fcmd_stop(fcmd);
		if (ret)
			fcmd_err("failed to stop fcmd%d\n", fcmd->id);
		break;

	case FCMD_DUMP:
		fcmd_dump(fcmd);
		break;

	case FCMD_GET_PORT_NUM:
		if (!arg) {
			fcmd_err("failed to get AXI port number\n");
			ret = -EINVAL;
			break;
		}
		*(int *)arg = fcmd->port;
		break;

	default:
		break;
	}

	return ret;
}

static const struct v4l2_subdev_core_ops fcmd_sd_core_ops = {
	.ioctl = fcmd_ioctl,
};

static const struct v4l2_subdev_ops fcmd_subdev_ops = {
	.core = &fcmd_sd_core_ops,
};

static void fcmd_init_subdev(struct fcmd_device *fcmd)
{
	struct v4l2_subdev *sd = &fcmd->sd;

	v4l2_subdev_init(sd, &fcmd_subdev_ops);
	sd->owner = THIS_MODULE;
	sd->grp_id = fcmd->id;
	snprintf(sd->name, sizeof(sd->name), "%s.%d", "fcmd-sd", fcmd->id);
	v4l2_set_subdevdata(sd, fcmd);
}

static int fcmd_probe(struct platform_device *pdev)
{
	struct device *dev = &pdev->dev;
	struct fcmd_device *fcmd;
	int ret = 0;

	fcmd = devm_kzalloc(dev, sizeof(*fcmd), GFP_KERNEL);
	if (!fcmd) {
		fcmd_err("failed to allocate fcmd device.\n");
		ret = -ENOMEM;
		goto err;
	}

	dma_set_mask(dev, DMA_BIT_MASK(32)); /* herb : consider about necessity */

	ret = fcmd_parse_dt(fcmd, dev);
	if (ret)
		goto err_dt;

	fcmd_drvdata[fcmd->id] = fcmd;

	spin_lock_init(&fcmd->slock);
	mutex_init(&fcmd->lock);

	ret = fcmd_init_resources(fcmd, pdev);
	if (ret)
		goto err_dt;

	init_waitqueue_head(&fcmd->xferdone_wq);

	fcmd_init_subdev(fcmd);
	platform_set_drvdata(pdev, fcmd);

	fcmd_info("fcmd%d is probed successfully\n", fcmd->id);

	return 0;

err_dt:
	kfree(fcmd);
err:
	return ret;
}

static int fcmd_remove(struct platform_device *pdev)
{
	fcmd_info("%s driver unloaded\n", pdev->name);
	return 0;
}

static int fcmd_runtime_suspend(struct device *dev)
{
	struct fcmd_device *fcmd = dev_get_drvdata(dev);

	fcmd_dbg("%s +\n", __func__);
	clk_disable_unprepare(fcmd->res.aclk);
	fcmd_dbg("%s -\n", __func__);

	return 0;
}

static int fcmd_runtime_resume(struct device *dev)
{
	struct fcmd_device *fcmd = dev_get_drvdata(dev);

	fcmd_dbg("%s +\n", __func__);
	clk_prepare_enable(fcmd->res.aclk);
	fcmd_dbg("%s -\n", __func__);

	return 0;
}

static const struct of_device_id fcmd_of_match[] = {
	{ .compatible = "samsung,exynos9-fcmd" },
	{},
};
MODULE_DEVICE_TABLE(of, fcmd_of_match);

static const struct dev_pm_ops fcmd_pm_ops = {
	.runtime_suspend	= fcmd_runtime_suspend,
	.runtime_resume		= fcmd_runtime_resume,
};

struct platform_driver fcmd_driver __refdata = {
	.probe		= fcmd_probe,
	.remove		= fcmd_remove,
	.driver = {
		.name	= FCMD_MODULE_NAME,
		.owner	= THIS_MODULE,
		.pm	= &fcmd_pm_ops,
		.of_match_table = of_match_ptr(fcmd_of_match),
		.suppress_bind_attrs = true,
	}
};

MODULE_AUTHOR("Haksoo Kim <herb@samsung.com>");
MODULE_DESCRIPTION("Samsung EXYNOS DMA DSIM Fast Command driver");
MODULE_LICENSE("GPL");
