/*
 * Copyright (C) 2012-2020 Samsung Electronics, Inc.
 *
 * This software is licensed under the terms of the GNU General Public
 * License version 2, as published by the Free Software Foundation, and
 * may be copied, distributed, and modified under those terms.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */

#ifndef _TZ_CPUFREQ_TRANS_H
#define _TZ_CPUFREQ_TRANS_H

#include <linux/kconfig.h>

#if IS_MODULE(CONFIG_TZDEV)
int cpufreq_trans_init(void);
#endif

#endif /* _TZ_CPUFREQ_TRANS_H */
