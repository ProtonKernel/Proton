/* SPDX-License-Identifier: GPL-2.0-or-later
 * sound/soc/samsung/vts/vts_irq.h
 *
 * ALSA SoC - Samsung VTS IRQ
 *
 * Copyright (c) 2020 Samsung Electronics Co. Ltd.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#ifndef __VTS_IRQ_H
#define __VTS_IRQ_H

void vts_irq_gc_ack_set_bit(struct irq_data *d);
void vts_irq_gc_mask_set_bit(struct irq_data *d);
void vts_irq_gc_mask_clr_bit(struct irq_data *d);
void vts_irq_remove_generic_chip(struct irq_chip_generic *gc, u32 msk,
			     unsigned int clr, unsigned int set);
struct irq_chip_generic *vts_irq_get_domain_generic_chip(
	struct irq_domain *d, unsigned int hw_irq);
int vts_irq_alloc_domain_generic_chips(struct irq_domain *d, int irqs_per_chip,
	int num_ct, const char *name,
	irq_flow_handler_t handler,
	unsigned int clr, unsigned int set,
	enum irq_gc_flags gcflags);

int vts_irq_map_generic_chip(struct irq_domain *d, unsigned int virq,
			 irq_hw_number_t hw_irq);
void vts_irq_unmap_generic_chip(struct irq_domain *d, unsigned int virq);

const struct irq_domain_ops vts_irq_generic_chip_ops = {
	.map	= vts_irq_map_generic_chip,
	.unmap  = vts_irq_unmap_generic_chip,
	.xlate	= irq_domain_xlate_onetwocell,
};
EXPORT_SYMBOL_GPL(vts_irq_generic_chip_ops);

#endif /* __VTS_IRQ_H */
