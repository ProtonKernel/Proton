// SPDX-License-Identifier: GPL-2.0
/*
 * ION Memory debug exynos feature support
 *
 * Copyright (c) 2020 Samsung Electronics Co., Ltd.
 */

#include <linux/slab.h>
#include <linux/ion.h>
#include <linux/scatterlist.h>
#include <linux/seq_file.h>
#include <linux/debugfs.h>
#include <linux/list_sort.h>

#include "ion_exynos.h"
#include "ion_debug.h"
#include "ion_exynos_prot.h"

static const char *ion_heap_dump_type(enum ion_heap_type type)
{
	if (type == ION_EXYNOS_HEAP_TYPE_CARVEOUT)
		return "carveout";

	if (type == ION_HEAP_TYPE_DMA)
		return "cma";

	return "undefined";
}

static void ion_buffer_dump_flags(struct seq_file *s, unsigned long flags)
{
	if (flags & ION_FLAG_CACHED)
		prseq(s, "cached");
	else
		prseq(s, "noncached");

	if (flags & ION_EXYNOS_FLAG_PROTECTED)
		prseq(s, "|protected");
}

void ion_noncontig_heap_show(struct seq_file *s,
			     struct ion_exynos_heap *exynos_heap)
{
	struct ion_buffer *buffer;
	size_t total = 0;
	int i = 0;

	prseq(s, "[%8s] %12s %5s\n", "index", "size", "flags");

	mutex_lock(&exynos_heap->buffer_lock);
	list_for_each_entry(buffer, &exynos_heap->list, list) {
		prseq(s, "[%#8d] %#12zu  ", i++, buffer->size);
		ion_buffer_dump_flags(s, buffer->flags);
		total += buffer->size;

		prseq(s, "\n");
	}

	prseq(s, "Total: %zu Kbytes\n", total >> 10);

	mutex_unlock(&exynos_heap->buffer_lock);
}

static int ion_contig_heap_compare(void *priv, struct list_head *a,
				   struct list_head *b)
{
	struct ion_buffer *l = container_of(a, struct ion_buffer, list);
	struct ion_buffer *r = container_of(b, struct ion_buffer, list);
	dma_addr_t left = sg_phys(l->sg_table->sgl);
	dma_addr_t right = sg_phys(r->sg_table->sgl);

	if (left > right)
		return 1;
	else if (left < right)
		return -1;
	else
		return 0;
}

void ion_contig_heap_show(struct seq_file *s,
			  struct ion_exynos_heap *exynos_heap)
{
	struct ion_buffer *cur, *next, *last;
	size_t begin, end, base = 0;
	unsigned long size = 0;
	size_t total = 0;

	base = exynos_heap->base;
	size = exynos_heap->size;

	mutex_lock(&exynos_heap->buffer_lock);
	list_sort(NULL, &exynos_heap->list, ion_contig_heap_compare);

	last = list_last_entry(&exynos_heap->list, struct ion_buffer, list);

	prseq(s, "%5s %10s %8s %10s\n",
	      "flags", "offset", "size", "free");
	list_for_each_entry_safe(cur, next, &exynos_heap->list, list) {
		begin = sg_phys(cur->sg_table->sgl) - base;

		if (cur == last)
			end = size;
		else
			end = sg_phys(next->sg_table->sgl) - base;

		prseq(s, "%#5lx %#10lx %8zu %10zu\n", cur->flags,
		      begin, cur->size, end - begin - cur->size);

		total += cur->size;
	}

	prseq(s, "Total: %zu Kbytes\n", total >> 10);

	mutex_unlock(&exynos_heap->buffer_lock);
}

static int ion_debug_heap_show(struct seq_file *s, void *unused)
{
	struct ion_exynos_heap *exynos_heap = s->private;
	struct ion_heap *heap = &exynos_heap->heap;

	prseq(s, "ION heap '%s' of type %s and id %u\n",
	      heap->name, ion_heap_dump_type(heap->type), heap->id);

	if (heap->type == ION_HEAP_TYPE_DMA ||
	    heap->type == ION_EXYNOS_HEAP_TYPE_CARVEOUT)
		ion_contig_heap_show(s, exynos_heap);
	else
		ion_noncontig_heap_show(s, exynos_heap);

	return 0;
}

static int ion_debug_heap_open(struct inode *inode, struct file *file)
{
	return single_open(file, ion_debug_heap_show, inode->i_private);
}

static const struct file_operations debug_heap_fops = {
	.open = ion_debug_heap_open,
	.read = seq_read,
	.llseek = seq_lseek,
	.release = single_release,
};

/*
 * It should be called after ion_device_add_heap to get debugfs_dir of heap.
 */
void ion_heap_debug_init(struct ion_exynos_heap *exynos_heap)
{
	if (!debugfs_create_file("buffers", 0444,
				 exynos_heap->heap.debugfs_dir,
				 exynos_heap, &debug_heap_fops))
		perrfn("failed to create debugfs for %s",
		       exynos_heap->heap.name);
}

#define ION_MAX_EVENT_LOG	2048
#define ION_EVENT_CLAMP_ID(id) ((id) & (ION_MAX_EVENT_LOG - 1))

static atomic_t eventid;

static char * const ion_event_name[] = {
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

static struct ion_event {
	unsigned char heapname[8];
	unsigned long data;
	ktime_t begin;
	ktime_t done;
	size_t size;
	enum ion_event_type type;
} eventlog[ION_MAX_EVENT_LOG];

void ion_event_record(enum ion_event_type type,
		      struct ion_buffer *buffer, ktime_t begin)
{
	int idx = ION_EVENT_CLAMP_ID(atomic_inc_return(&eventid));
	struct ion_event *event = &eventlog[idx];

	event->type = type;
	event->begin = begin;
	event->done = ktime_get();
	event->size = buffer->size;
	event->data = (type == ION_EVENT_TYPE_FREE) ?
		buffer->private_flags & ION_PRIV_FLAG_SHRINKER_FREE :
		buffer->flags;
	strlcpy(event->heapname, buffer->heap->name, sizeof(event->heapname));
}

static int ion_debug_event_show(struct seq_file *s, void *unused)
{
	int index = ION_EVENT_CLAMP_ID(atomic_read(&eventid) + 1);
	int i = index;

	seq_printf(s, "%13s %17s %18s %13s %10s %24s\n", "timestamp", "type",
		   "heap", "size (kb)", "time (us)", "remarks");

	do {
		struct ion_event *event = &eventlog[ION_EVENT_CLAMP_ID(i)];
		struct timeval tv = ktime_to_timeval(event->begin);
		long elapsed = ktime_us_delta(event->done, event->begin);

		if (elapsed == 0)
			continue;

		seq_printf(s, "%06ld.%06ld ", tv.tv_sec, tv.tv_usec);
		seq_printf(s, "%17s %18s %13zd %10ld",
			   ion_event_name[event->type], event->heapname,
			   event->size / SZ_1K, elapsed);

		if (elapsed > 100 * USEC_PER_MSEC)
			seq_puts(s, " *");

		switch (event->type) {
		case ION_EVENT_TYPE_ALLOC:
			seq_puts(s, "  ");
			ion_buffer_dump_flags(s, event->data);
			break;
		case ION_EVENT_TYPE_FREE:
			if (event->data)
				seq_puts(s, " shrinker");
			break;
		default:
			break;

		}
		seq_puts(s, "\n");
	} while (ION_EVENT_CLAMP_ID(++i) != index);

	return 0;
}

static int ion_debug_event_open(struct inode *inode, struct file *file)
{
	return single_open(file, ion_debug_event_show, inode->i_private);
}

static const struct file_operations debug_event_fops = {
	.open = ion_debug_event_open,
	.read = seq_read,
	.llseek = seq_lseek,
	.release = single_release,
};

static struct dentry *ion_exynos_debugfs;

void ion_debug_init(void)
{
	ion_exynos_debugfs = debugfs_create_dir("ion_exynos", NULL);
	debugfs_create_file("event", 0444, ion_exynos_debugfs, NULL,
			    &debug_event_fops);

}
