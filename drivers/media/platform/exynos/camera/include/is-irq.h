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

#ifndef IS_IRQ_H
#define IS_IRQ_H

#include <linux/interrupt.h>

void is_set_affinity_irq(unsigned int irq, bool enable);
int is_request_irq(unsigned int irq,
		irqreturn_t (*func)(int, void *),
		const char *name,
		unsigned int added_irq_flags,
		void *dev);
void is_free_irq(unsigned int irq, void *dev);

#endif /* IS_IRQ_H */
