/*
 * Exynos Performance
 * Author: Jungwook Kim, jwook1.kim@samsung.com
 * Created: 2014
 * Updated: 2016, 2019, 2020
 */

#include <linux/of.h>
#include <linux/module.h>
#include <linux/platform_device.h>

#include "xperf.h"

static const char *prefix = "xperf";

static int xperf_probe(struct platform_device *pdev)
{
	struct kobject *kobj = &pdev->dev.kobj;

	if (sysfs_create_link(kernel_kobj, kobj, "xperf"))
		pr_warn("[%s] failed to link xperf\n", prefix);

	xperf_prof_init(pdev, kobj);
	xperf_core_init(kobj);
	xperf_memcpy_init(pdev, kobj);
	xperf_gmc_init(pdev, kobj);

	pr_info("[%s] Exynos Perf initialized\n", prefix);

	return 0;
}

static const struct of_device_id of_xperf_match[] = {
	{ .compatible = "samsung,exynos-perf", },
	{ },
};
MODULE_DEVICE_TABLE(of, of_xperf_match);

static struct platform_driver xperf_driver = {
	.driver = {
		.name	= "exynos-perf",
		.owner = THIS_MODULE,
		.of_match_table = of_xperf_match,
	},
	.probe		= xperf_probe,
};

module_platform_driver(xperf_driver);

MODULE_AUTHOR("Jungwook Kim");
MODULE_DESCRIPTION("Exynos Performance driver");
MODULE_LICENSE("GPL");
