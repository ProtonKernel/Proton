/*
 * Copyright (C) 2012-2019, Samsung Electronics Co., Ltd.
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

#ifndef __TZ_PROFILER_H__
#define __TZ_PROFILER_H__

#include <linux/kconfig.h>

#if IS_MODULE(CONFIG_TZDEV)
int tzprofiler_init(void);
#endif

#ifdef CONFIG_TZPROFILER
void tzprofiler_pre_smc_call_direct(void);
void tzprofiler_post_smc_call_direct(void);
#else /* CONFIG_TZPROFILER */
static inline void tzprofiler_pre_smc_call_direct(void) {}
static inline void tzprofiler_post_smc_call_direct(void) {}
#endif /* CONFIG_TZPROFILER */

#endif /* __TZ_PROFILER_H__ */