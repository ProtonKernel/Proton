/*
 * Copyright (c) 2020 Samsung Electronics Co., Ltd.
 *		http://www.samsung.com
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/of_address.h>
#include <linux/of_platform.h>
#include <linux/debugfs.h>
#include <linux/fs.h>
#include <linux/slab.h>

#include <soc/samsung/acpm_ipc_ctrl.h>
#include <linux/soc/samsung/exynos-soc.h>

struct dentry *plg_dbg_root = NULL;

struct acpm_plg_dbg_info {
	unsigned int plugin_num;
	struct device *dev;
	struct device_node *np;
	unsigned int command[4];
};

static int data0_get(void *data, unsigned long long *val)
{
	struct acpm_plg_dbg_info *plg_dbg_info = data;
	*val = plg_dbg_info->command[0];
	return 0;
}

static int data0_set(void *data, unsigned long long val)
{
	struct acpm_plg_dbg_info *plg_dbg_info = data;
	plg_dbg_info->command[0]  = val;
	return 0;
}

static int data1_get(void *data, unsigned long long *val)
{
	struct acpm_plg_dbg_info *plg_dbg_info = data;
	*val = plg_dbg_info->command[1];
	return 0;
}

static int data1_set(void *data, unsigned long long val)
{
	struct acpm_plg_dbg_info *plg_dbg_info = data;
	plg_dbg_info->command[1]  = val;
	return 0;
}

static int data2_get(void *data, unsigned long long *val)
{
	struct acpm_plg_dbg_info *plg_dbg_info = data;
	*val = plg_dbg_info->command[2];
	return 0;
}

static int data2_set(void *data, unsigned long long val)
{
	struct acpm_plg_dbg_info *plg_dbg_info = data;
	plg_dbg_info->command[2]  = val;
	return 0;
}

static int data3_get(void *data, unsigned long long *val)
{
	struct acpm_plg_dbg_info *plg_dbg_info = data;
	*val = plg_dbg_info->command[3];
	return 0;
}

static int data3_set(void *data, unsigned long long val)
{
	struct acpm_plg_dbg_info *plg_dbg_info = data;
	plg_dbg_info->command[3]  = val;
	return 0;
}

static int send_get(void *data, unsigned long long *val)
{
	struct acpm_plg_dbg_info *plg_dbg_info = data;
	unsigned int channel_num, size;
	struct ipc_config config;
	int ret = 0;

	if (!acpm_ipc_request_channel(plg_dbg_info->np, NULL, &channel_num, &size)) {
		config.cmd = plg_dbg_info->command;
		config.response = true;
		config.indirection = false;

		ret = acpm_ipc_send_data(channel_num, &config);
		if (ret) {
			pr_err("%s - acpm_ipc_send_data fail.\n", __func__);
			return ret;
		}
		acpm_ipc_release_channel(plg_dbg_info->np, channel_num);
	} else {
		pr_err("%s ipc request_channel fail, id:%u, size:%u\n",
				__func__, channel_num, size);
		ret = -EBUSY;
	}

	return 0;
}

static int send_set(void *data, unsigned long long val)
{
	return 0;
}

DEFINE_SIMPLE_ATTRIBUTE(data0_fops, data0_get, data0_set, "%llu\n");
DEFINE_SIMPLE_ATTRIBUTE(data1_fops, data1_get, data1_set, "%llu\n");
DEFINE_SIMPLE_ATTRIBUTE(data2_fops, data2_get, data2_set, "%llu\n");
DEFINE_SIMPLE_ATTRIBUTE(data3_fops, data3_get, data3_set, "%llu\n");
DEFINE_SIMPLE_ATTRIBUTE(send_fops, send_get, send_set, "%llu\n");

static int acpm_plg_dbg_probe(struct platform_device *pdev)
{
	struct dentry *den = NULL;
	struct acpm_plg_dbg_info *acpm_plg_dbg;
	struct device_node *node = pdev->dev.of_node;
	int ret = 0;

	dev_info(&pdev->dev, "acpm_plg_dbg probe\n");

	if (!node) {
		dev_err(&pdev->dev, "driver doesnt supportnon-dt devices\n");
		return -ENODEV;
	}

	acpm_plg_dbg = devm_kzalloc(&pdev->dev,
			sizeof(struct acpm_plg_dbg_info), GFP_KERNEL);
	if (IS_ERR(acpm_plg_dbg))
		return PTR_ERR(acpm_plg_dbg);
	if (!acpm_plg_dbg)
		return -ENOMEM;

	if (!plg_dbg_root) {
		plg_dbg_root = debugfs_create_dir("acpm_plg_dbg", NULL);
		if (!plg_dbg_root) {
			pr_err("%s : could not create debufs dir\n",
					__func__);
			ret = -ENOMEM;
			goto err_dbgfs_root;
		}
	}

	acpm_plg_dbg->dev = &pdev->dev;
	acpm_plg_dbg->np = pdev->dev.of_node;

	den = debugfs_create_dir(dev_name(&pdev->dev), plg_dbg_root);
	debugfs_create_file("data0", 0644, den, acpm_plg_dbg, &data0_fops);
	debugfs_create_file("data1", 0644, den, acpm_plg_dbg, &data1_fops);
	debugfs_create_file("data2", 0644, den, acpm_plg_dbg, &data2_fops);
	debugfs_create_file("data3", 0644, den, acpm_plg_dbg, &data3_fops);
	debugfs_create_file("send", 0644, den, acpm_plg_dbg, &send_fops);

	dev_info(&pdev->dev, "acpm_plg_dbg probe done\n");
	return 0;

err_dbgfs_root:
	kfree(acpm_plg_dbg);
	return ret;
}

static int acpm_plg_dbg_remove(struct platform_device *pdev)
{
	return 0;
}

static const struct of_device_id of_acpm_plg_dbg_match[] = {
	{ .compatible = "samsung,exynos-acpm-plg-dbg" },
	{},
};
MODULE_DEVICE_TABLE(of, of_acpm_plg_dbg_match);

static struct platform_driver samsung_acpm_plg_dbg_driver = {
	.probe	= acpm_plg_dbg_probe,
	.remove	= acpm_plg_dbg_remove,
	.driver	= {
		.name = "exynos-acpm-plg-dbg",
		.owner	= THIS_MODULE,
		.of_match_table = of_acpm_plg_dbg_match,
	},
};

static int exynos_acpm_plg_dbg_init(void)
{
	pr_info("%s	register\n", __func__);
	return platform_driver_register(&samsung_acpm_plg_dbg_driver);
}
late_initcall_sync(exynos_acpm_plg_dbg_init);

static void exynos_acpm_plg_dbg_exit(void)
{
	return platform_driver_unregister(&samsung_acpm_plg_dbg_driver);
}
module_exit(exynos_acpm_plg_dbg_exit);

MODULE_LICENSE("GPL");
