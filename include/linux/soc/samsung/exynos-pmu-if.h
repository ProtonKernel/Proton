/*
 * Copyright (c) 2014 Samsung Electronics Co., Ltd.
 *		http://www.samsung.com
 *
 * Header for EXYNOS PMU Driver support
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#ifndef __LINUX_SOC_EXYNOS_PMU_H
#define __LINUX_SOC_EXYNOS_PMU_H

struct regmap;

enum sys_powerdown {
	SYS_AFTR,
	SYS_LPA,
	SYS_SLEEP,
	NUM_SYS_POWERDOWN,
};

extern void exynos_sys_powerdown_conf(enum sys_powerdown mode);
#if defined(CONFIG_EXYNOS_PMU_IF) || defined(CONFIG_EXYNOS_PMU_IF_MODULE) 
extern struct regmap *exynos_get_pmuregmap(void);
#else
static inline struct regmap *exynos_get_pmuregmap(void)
{
	return ERR_PTR(-ENODEV);
}
#endif

#endif /* __LINUX_SOC_EXYNOS_PMU_H */
