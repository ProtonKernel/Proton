/*
 * Copyright (c) 2018 Samsung Electronics Co., Ltd.
 *		http://www.samsung.com/
 *
 * EXYNOS - CPU Hotplug CONTROL support
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#ifndef __EXYNOS_CPU_HP_H
#define __EXYNOS_CPU_HP_H __FILE__

#if IS_ENABLED(CONFIG_EXYNOS_CPUHP)
extern int exynos_cpuhp_unregister(char *name, struct cpumask mask);
extern int exynos_cpuhp_register(char *name, struct cpumask mask);
extern int exynos_cpuhp_request(char *name, struct cpumask mask);
#else
static inline int exynos_cpuhp_unregister(char *name, struct cpumask mask)
{ return -1; }
static inline int exynos_cpuhp_register(char *name, struct cpumask mask)
{ return -1; }
static inline int exynos_cpuhp_request(char *name, struct cpumask mask)
{ return -1; }
#endif

#endif /* __EXYNOS_CPU_HP_H */
