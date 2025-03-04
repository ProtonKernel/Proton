/* SPDX-License-Identifier: GPL-2.0
 *
 * Copyright (C) 2021 Samsung Electronics.
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
#ifndef __MCPS_CPU_H__
#define __MCPS_CPU_H__

#include <linux/cpumask.h>

#include "mcps_logger.h"

#if defined(CONFIG_MCPS_MOCK)
#include "../kunit_test/mock/mock_dev.h"

#define mcps_nr_cpus 1U
static inline bool mcps_is_cpu_online(unsigned int cpu) { return mock_cpu_online(cpu); }
static inline bool mcps_is_cpu_possible(unsigned int cpu) { return mock_cpu_possible(cpu); }
#else
#define mcps_nr_cpus nr_cpumask_bits
static inline bool mcps_is_cpu_online(unsigned int cpu) { return (cpu < mcps_nr_cpus) && cpu_online(cpu); }
static inline bool mcps_is_cpu_possible(unsigned int cpu) { return (cpu < mcps_nr_cpus) && cpu_possible(cpu); }
#endif

int mcps_alloc_cpumask_andnot(cpumask_var_t *dst, const cpumask_var_t base, const cpumask_var_t except);
int mcps_alloc_cpumask_copy(cpumask_var_t *dst, const cpumask_var_t src);
int mcps_alloc_cpumask_and(cpumask_var_t *dst, const cpumask_var_t src1, const cpumask_var_t src2);
int mcps_parse_atocpumask(const char *buf, cpumask_var_t *mask);

#endif
