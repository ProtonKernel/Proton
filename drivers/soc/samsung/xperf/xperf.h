/*
 * Exynos Performance
 * Author: Jungwook Kim, jwook1.kim@samsung.com
 * Created: 2014
 * Updated: 2015, 2019
 */

#ifndef EXYNOS_PERF_H
#define EXYNOS_PERF_H __FILE__

extern int xperf_prof_init(struct platform_device *pdev, struct kobject *kobj);
extern int xperf_core_init(struct kobject *kobj);
extern int xperf_memcpy_init(struct platform_device *pdev, struct kobject *kobj);
extern int xperf_gmc_init(struct platform_device *pdev, struct kobject *kobj);

#endif
