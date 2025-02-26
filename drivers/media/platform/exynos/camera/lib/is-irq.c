// SPDX-License-Identifier: GPL-2.0
/*
 * Samsung EXYNOS IS (Imaging Subsystem) driver
 *
 *
 * Copyright (c) 2020 Samsung Electronics Co., Ltd
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#include <linux/kernel.h>
#include <linux/list.h>
#include <linux/spinlock.h>
#include <linux/wait.h>
#include <linux/errno.h>

#include "is-irq.h"
#include "is-debug.h"
#include "is-common-config.h"

void is_set_affinity_irq(unsigned int irq, bool enable)
{
#if (LINUX_VERSION_CODE >= KERNEL_VERSION(4, 20, 0))
	const char *buf = "0-3";
	struct cpumask cpumask;
#endif

	if (IS_ENABLED(IRQ_MULTI_TARGET_CL0)) {
#if (LINUX_VERSION_CODE >= KERNEL_VERSION(4, 20, 0))
		cpulist_parse(buf, &cpumask);
		irq_set_affinity_hint(irq, enable ? &cpumask : NULL);
#else
		irq_force_affinity(irq, cpumask_of(0));
#endif
	}
}

int is_request_irq(unsigned int irq,
	irqreturn_t (*func)(int, void *),
	const char *name,
	unsigned int added_irq_flags,
	void *dev)
{
	int ret = 0;

	ret = request_irq(irq, func,
			IS_HW_IRQ_FLAG | added_irq_flags,
			name,
			dev);
	if (ret) {
		err("%s, request_irq fail", name);
		return ret;
	}

	is_set_affinity_irq(irq, true);

	return ret;
}

void is_free_irq(unsigned int irq, void *dev)
{
	is_set_affinity_irq(irq, false);
	free_irq(irq, dev);
}
