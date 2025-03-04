// SPDX-License-Identifier: GPL-2.0
/*
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
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/cpumask.h>

#include "mcps_logger.h"
#include "mcps_cpu.h"

int mcps_alloc_cpumask_andnot(cpumask_var_t *dst, const cpumask_var_t base, const cpumask_var_t except)
{
	if (!zalloc_cpumask_var(dst, GFP_KERNEL)) {
		mcps_err("failed to alloc kmem");
		return -ENOMEM;
	}

	cpumask_andnot(*dst, base, except);
	return 0;
}

int mcps_alloc_cpumask_copy(cpumask_var_t *dst, const cpumask_var_t src)
{
	if (!zalloc_cpumask_var(dst, GFP_KERNEL)) {
		mcps_err("failed to alloc kmem");
		return -ENOMEM;
	}

	cpumask_copy(*dst, src);
	return 0;
}

int mcps_alloc_cpumask_and(cpumask_var_t *dst, const cpumask_var_t src1, const cpumask_var_t src2)
{
	if (!zalloc_cpumask_var(dst, GFP_KERNEL)) {
		mcps_err("Failed to alloc kmem");
		return -ENOMEM;
	}

	cpumask_and(*dst, src1, src2);
	return 0;
}

int mcps_parse_atocpumask(const char *buf, cpumask_var_t *mask)
{
	int len, err;

	if (!zalloc_cpumask_var(mask, GFP_KERNEL)) {
		mcps_err("failed to alloc kmem");
		return -ENOMEM;
	}

	len = strlen(buf);
	err = bitmap_parse(buf, len, cpumask_bits(*mask), mcps_nr_cpus);
	if (err) {
		mcps_err("failed to parse %s", buf);
		free_cpumask_var(*mask);
		return -EINVAL;
	}

	return 0;
}
