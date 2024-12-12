// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright (c) 2020 Samsung Electronics Co., Ltd.
 *      http://www.samsung.com
 */

#include <linux/cpumask.h>
#include <linux/slab.h>
#include <linux/tick.h>
#include <linux/cpu.h>
#include <linux/cpuhotplug.h>
#include <linux/cpu_pm.h>
#include <linux/cpuidle.h>
#include <linux/module.h>
#include <linux/platform_device.h>
#include <linux/of.h>

#include "../../../kernel/power/power.h"
#include <linux/cpumask.h>

static struct cpumask fast_cpu_mask;
static struct cpumask backup_cpu_mask;
static u32 start_cpu, end_cpu;

static void init_pm_cpumask(void)
{
	int i;

	cpumask_clear(&fast_cpu_mask);
	for (i = start_cpu; i <= end_cpu; i++)
		cpumask_set_cpu(i, &fast_cpu_mask);
}

static int sec_wakeup_cpu_allocator_suspend_noirq(struct device *dev)
{
	pr_info("%s\n", __func__);

	return 0;
}

static int sec_wakeup_cpu_allocator_resume_noirq(struct device *dev)
{
	pr_info("%s: cpu mask\n", __func__);

	cpumask_copy(&backup_cpu_mask, current->cpus_ptr);
	set_cpus_allowed_ptr(current, &fast_cpu_mask);

	return 0;
}

static void sec_wakeup_cpu_allocator_complete(struct device *dev)
{
	pr_info("%s: cpu mask\n", __func__);

	if (!cpumask_empty(&backup_cpu_mask)) {
		set_cpus_allowed_ptr(current, &backup_cpu_mask);
		cpumask_clear(&backup_cpu_mask);
	}
}

static const struct dev_pm_ops wakeupcpu_pm_ops = {
	.suspend_noirq = sec_wakeup_cpu_allocator_suspend_noirq,
	.resume_noirq = sec_wakeup_cpu_allocator_resume_noirq,
	.complete = sec_wakeup_cpu_allocator_complete,
};


static int sec_wakeup_cpu_allocator_setup(struct device *dev)
{
	int ret = 0;
	struct device_node *np = of_find_node_by_name(NULL, "sec_wakeup_cpu_allocator");

	pr_info("%s\n", __func__);

	if (of_property_read_u32(np, "start_cpu_num", &start_cpu) < 0) {
		pr_info("%s: start_cpu_num not found\n", __func__);
		ret = -ENODEV;
	}

	if (of_property_read_u32(np, "end_cpu_num", &end_cpu) < 0) {
		pr_info("%s: end_cpu_num not found\n", __func__);
		ret = -ENODEV;
	}

	return ret;
}

static int sec_wakeup_cpu_allocator_probe(struct platform_device *pdev)
{
	pr_info("%s\n", __func__);

	if (sec_wakeup_cpu_allocator_setup(&pdev->dev) < 0) {
		pr_crit("%s: failed\n", __func__);
		return -ENODEV;
	}

	init_pm_cpumask();

	return 0;
}

static const struct of_device_id of_sec_wakeup_cpu_allocator_match[] = {
	{ .compatible = "samsung,sec_wakeup_cpu_allocator", },
	{ },
};
MODULE_DEVICE_TABLE(of, of_sec_wakeup_cpu_allocator_match);

static struct platform_driver sec_wakeup_cpu_allocator_driver = {
	.probe = sec_wakeup_cpu_allocator_probe,
	.driver = {
		.name = "sec_wakeup_cpu_allocator",
		.of_match_table = of_match_ptr(of_sec_wakeup_cpu_allocator_match),
#if IS_ENABLED(CONFIG_PM)
		.pm = &wakeupcpu_pm_ops,
#endif
	},

};

static int __init sec_wakeup_cpu_allocator_driver_init(void)
{
	pr_info("%s\n", __func__);
	return platform_driver_register(&sec_wakeup_cpu_allocator_driver);
}
core_initcall(sec_wakeup_cpu_allocator_driver_init);

static void __exit sec_wakeup_cpu_allocator_driver_exit(void)
{
	pr_info("%s\n", __func__);
	return platform_driver_unregister(&sec_wakeup_cpu_allocator_driver);
}
module_exit(sec_wakeup_cpu_allocator_driver_exit);

MODULE_DESCRIPTION("SEC WAKEUP CPU ALLOCATOR driver");
MODULE_LICENSE("GPL");
