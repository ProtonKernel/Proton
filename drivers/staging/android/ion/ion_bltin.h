/* SPDX-License-Identifier: GPL-2.0 */
/*
 * ION Memory Allocator feature support for bultin heaps
 *
 * Copyright (c) 2020 Samsung Electronics Co., Ltd.
 */

#ifndef _ION_BLTIN_H
#define _ION_BLTIN_H

#define to_bltin_heap(x) container_of(x, struct ion_bltin_heap, heap)
struct ion_bltin_heap {
	struct device *dev;
	struct ion_heap heap;
	struct list_head list;
	struct mutex buffer_lock; /* protects the buffer list */
};

enum ion_bltin_event_type {
	ION_BLTIN_EVENT_ALLOC = 0,
	ION_BLTIN_EVENT_FREE,
	ION_BLTIN_EVENT_MMAP,
	ION_BLTIN_EVENT_VMAP,
	ION_BLTIN_EVENT_DMA_MAP,
	ION_BLTIN_EVENT_DMA_UNMAP,
	ION_BLTIN_EVENT_CPU_BEGIN,
	ION_BLTIN_EVENT_CPU_END,
	ION_BLTIN_EVENT_CPU_BEGIN_PARTIAL,
	ION_BLTIN_EVENT_CPU_END_PARTIAL,
};

void ion_bltin_heap_init(struct ion_bltin_heap *bltin_heap);
void ion_add_bltin_buffer(struct ion_bltin_heap *bltin_heap,
			  struct ion_buffer *buffer);
void ion_remove_bltin_buffer(struct ion_bltin_heap *bltin_heap,
			     struct ion_buffer *buffer);

void ion_bltin_event_record(enum ion_bltin_event_type type,
			    struct ion_buffer *buffer, ktime_t begin);
#define ion_bltin_event_begin() ktime_t begin  = ktime_get()

void ion_bltin_debug_init(struct dentry *root);

#endif
