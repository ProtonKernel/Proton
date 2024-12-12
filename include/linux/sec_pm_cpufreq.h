/* drivers/samsung/pm/sec_pm_cpufreq.c
 *
 * Copyright (c) 2020 Samsung Electronics Co., Ltd.
 * Author: Minsung Kim <ms925.kim@samsung.com>
 *
 * This software is licensed under the terms of the GNU General Public
 * License version 2, as published by the Free Software Foundation, and
 * may be copied, distributed, and modified under those terms.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 */

#ifndef _LINUX_SEC_PM_CPUFREQ_H
#define _LINUX_SEC_PM_CPUFREQ_H

#include <linux/cpufreq.h>

struct sec_pm_cpufreq_dev {
	struct cpufreq_frequency_table *freq_table;
	unsigned int max_freq;
	unsigned int max_level;
	unsigned int cur_level;
	struct cpufreq_policy *policy;
	struct list_head node;
	struct freq_qos_request qos_req;
};

#if IS_ENABLED(CONFIG_SEC_PM_CPUFREQ)
struct sec_pm_cpufreq_dev *
sec_pm_cpufreq_register(struct cpufreq_policy *policy);
void sec_pm_cpufreq_unregister(struct sec_pm_cpufreq_dev *cdev);
int sec_pm_cpufreq_throttle_by_one_step(void);
void sec_pm_cpufreq_unthrottle(void);
#else
static inline struct sec_pm_cpufreq_dev *
sec_pm_cpufreq_register(struct cpufreq_policy *policy) { return NULL; }
static inline void sec_pm_cpufreq_unregister(struct sec_pm_cpufreq_dev *cdev) {}
static inline int sec_pm_cpufreq_throttle_by_one_step(void) { return 0; }
static inline void sec_pm_cpufreq_unthrottle(void) {}
#endif /* CONFIG_SEC_PM_FREQ */
#endif /* _LINUX_SEC_PM_CPUFREQ_H */
