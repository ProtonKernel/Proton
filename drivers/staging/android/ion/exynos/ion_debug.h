/* SPDX-License-Identifier: GPL-2.0 */
/*
 * ION Memory debug exynos feature support
 *
 * Copyright (c) 2020 Samsung Electronics Co., Ltd.
 */

#ifndef _ION_DEBUG_H
#define _ION_DEBUG_H

#include <linux/seq_file.h>

#include "ion_exynos.h"

enum ion_event_type {
	ION_EVENT_TYPE_ALLOC = 0,
	ION_EVENT_TYPE_FREE,
	ION_EVENT_TYPE_MMAP,
	ION_EVENT_TYPE_VMAP,
	ION_EVENT_TYPE_DMA_MAP,
	ION_EVENT_TYPE_DMA_UNMAP,
	ION_EVENT_TYPE_CPU_BEGIN,
	ION_EVENT_TYPE_CPU_END,
	ION_EVENT_TYPE_CPU_BEGIN_PARTIAL,
	ION_EVENT_TYPE_CPU_END_PARTIAL,
};

void ion_event_record(enum ion_event_type type,
		      struct ion_buffer *buffer, ktime_t begin);
#define ion_event_begin()	ktime_t begin  = ktime_get()

void ion_contig_heap_show(struct seq_file *s,
			  struct ion_exynos_heap *exynos_heap);
void ion_heap_debug_init(struct ion_exynos_heap *exynos_heap);
void ion_debug_init(void);

#endif
