// SPDX-License-Identifier: GPL-2.0
/*
 * ION Memory debug bltin feature support
 *
 * Copyright (c) 2020 Samsung Electronics Co., Ltd.
 */

#include <linux/slab.h>
#include <linux/ion.h>
#include <linux/scatterlist.h>
#include <linux/seq_file.h>
#include <linux/debugfs.h>
#include <linux/list_sort.h>

#include "ion_bltin.h"

#define ION_EXYNOS_FLAG_PROTECTED BIT(16)

static const char *heap_dump_type(enum ion_heap_type type)
{
	if (type == ION_HEAP_TYPE_CUSTOM)
		return "hpa";

	if (type == ION_HEAP_TYPE_SYSTEM)
		return "system";

	return "undefined";
}

static void buffer_dump_flags(struct seq_file *s, unsigned long flags)
{
	if (flags & ION_FLAG_CACHED)
		seq_puts(s, "cached");
	else
		seq_puts(s, "noncached");

	if (flags & ION_EXYNOS_FLAG_PROTECTED)
		seq_puts(s, "|protected");
}

static int ion_bltin_debug_heap_show(struct seq_file *s, void *unused)
{
	struct ion_bltin_heap *bltin_heap = s->private;
	struct ion_heap *heap = &bltin_heap->heap;
	struct ion_buffer *buffer;
	size_t total = 0;
	int i = 0;

	seq_printf(s, "ION heap '%s' of type %s and id %u\n",
		   heap->name, heap_dump_type(heap->type), heap->id);

	seq_printf(s, "[%8s] %12s %5s\n", "index", "size", "flags");

	mutex_lock(&bltin_heap->buffer_lock);
	list_for_each_entry(buffer, &bltin_heap->list, list) {
		seq_printf(s, "[%#8d] %#12zu  ", i++, buffer->size);
		buffer_dump_flags(s, buffer->flags);
		total += buffer->size;

		seq_puts(s, "\n");
	}

	seq_printf(s, "Total: %zu Kbytes\n", total >> 10);

	mutex_unlock(&bltin_heap->buffer_lock);

	return 0;
}

static int ion_bltin_debug_heap_open(struct inode *inode, struct file *file)
{
	return single_open(file, ion_bltin_debug_heap_show, inode->i_private);
}

static const struct file_operations debug_heap_fops = {
	.open = ion_bltin_debug_heap_open,
	.read = seq_read,
	.llseek = seq_lseek,
	.release = single_release,
};

/*
 * It should be called after ion_device_add_heap to get debugfs_dir of heap.
 */
void ion_bltin_heap_init(struct ion_bltin_heap *bltin_heap)
{
	INIT_LIST_HEAD(&bltin_heap->list);
	mutex_init(&bltin_heap->buffer_lock);

	if (IS_ERR(debugfs_create_file("buffers", 0444,
				       bltin_heap->heap.debugfs_dir,
				       bltin_heap, &debug_heap_fops)))
		pr_err("failed to create file for %s",  bltin_heap->heap.name);
}

void ion_add_bltin_buffer(struct ion_bltin_heap *bltin_heap,
			  struct ion_buffer *buffer)
{
	mutex_lock(&bltin_heap->buffer_lock);
	list_add(&buffer->list, &bltin_heap->list);
	mutex_unlock(&bltin_heap->buffer_lock);
}

void ion_remove_bltin_buffer(struct ion_bltin_heap *bltin_heap,
			     struct ion_buffer *buffer)
{
	mutex_lock(&bltin_heap->buffer_lock);
	list_del(&buffer->list);
	mutex_unlock(&bltin_heap->buffer_lock);
}

#define MAX_EVENT_LOG	2048
#define EVENT_CLAMP_ID(id) ((id) & (MAX_EVENT_LOG - 1))

static atomic_t bltin_eventid;

static char * const bltin_event_name[] = {
	"alloc",
	"free",
	"mmap",
	"vmap",
	"map_dma_buf",
	"unmap_dma_buf",
	"begin_cpu_access",
	"end_cpu_access",
	"begin_cpu_partial",
	"end_cpu_partial",
};

static struct ion_bltin_event {
	unsigned char heapname[8];
	unsigned long data;
	ktime_t begin;
	ktime_t done;
	size_t size;
	enum ion_bltin_event_type type;
} bltin_event[MAX_EVENT_LOG];

void ion_bltin_event_record(enum ion_bltin_event_type type,
			    struct ion_buffer *buffer, ktime_t begin)
{
	int idx = EVENT_CLAMP_ID(atomic_inc_return(&bltin_eventid));
	struct ion_bltin_event *event = &bltin_event[idx];

	event->type = type;
	event->begin = begin;
	event->done = ktime_get();
	event->size = buffer->size;
	event->data = (type == ION_BLTIN_EVENT_FREE) ?
		buffer->private_flags & ION_PRIV_FLAG_SHRINKER_FREE :
		buffer->flags;
	strlcpy(event->heapname, buffer->heap->name, sizeof(event->heapname));
}

static int ion_bltin_event_show(struct seq_file *s, void *unused)
{
	int index = EVENT_CLAMP_ID(atomic_read(&bltin_eventid) + 1);
	int i = index;

	seq_printf(s, "%13s %17s %18s %13s %10s %24s\n", "timestamp", "type",
		   "heap", "size (kb)", "time (us)", "remarks");

	do {
		struct ion_bltin_event *event = &bltin_event[EVENT_CLAMP_ID(i)];
		struct timeval tv = ktime_to_timeval(event->begin);
		long elapsed = ktime_us_delta(event->done, event->begin);

		if (elapsed == 0)
			continue;

		seq_printf(s, "%06ld.%06ld ", tv.tv_sec, tv.tv_usec);
		seq_printf(s, "%17s %18s %13zd %10ld",
			   bltin_event_name[event->type], event->heapname,
			   event->size / SZ_1K, elapsed);

		if (elapsed > 100 * USEC_PER_MSEC)
			seq_puts(s, " *");

		switch (event->type) {
		case ION_BLTIN_EVENT_ALLOC:
			seq_puts(s, "  ");
			buffer_dump_flags(s, event->data);
			break;
		case ION_BLTIN_EVENT_FREE:
			if (event->data)
				seq_puts(s, " shrinker");
			break;
		default:
			break;
		}
		seq_puts(s, "\n");
	} while (EVENT_CLAMP_ID(++i) != index);

	return 0;
}

static int ion_bltin_event_open(struct inode *inode, struct file *file)
{
	return single_open(file, ion_bltin_event_show, inode->i_private);
}

static const struct file_operations ion_bltin_event_fops = {
	.open = ion_bltin_event_open,
	.read = seq_read,
	.llseek = seq_lseek,
	.release = single_release,
};

void ion_bltin_debug_init(struct dentry *root)
{
	debugfs_create_file("event", 0444, root, NULL, &ion_bltin_event_fops);
}
