// SPDX-License-Identifier: GPL-2.0
/*
 * ION Memory Allocator - dmabuf interface
 *
 * Copyright (c) 2020 Samsung Electronics Co., Ltd.
 */

#include <linux/device.h>
#include <linux/mm.h>
#include <linux/scatterlist.h>
#include <linux/slab.h>
#include <linux/vmalloc.h>
#include <linux/dma-mapping.h>
#include <linux/dma-noncoherent.h>
#include <linux/iommu.h>
#include <uapi/linux/dma-buf.h>

#include "ion_private.h"
#include "ion_bltin.h"
#include "exynos/ion_exynos_prot.h"

struct ion_iovm_map {
	struct list_head list;
	struct device *dev;
	struct sg_table table;
	unsigned long attrs;
	unsigned int mapcnt;
};

static struct ion_iovm_map *ion_iova_create(struct dma_buf_attachment *a)
{
	struct ion_buffer *buffer = a->dmabuf->priv;
	struct ion_iovm_map *iovm_map;
	struct scatterlist *sg, *new_sg;
	struct sg_table *table = buffer->sg_table;
	int i;

	iovm_map = kzalloc(sizeof(*iovm_map), GFP_KERNEL);
	if (!iovm_map)
		return NULL;

	if (sg_alloc_table(&iovm_map->table, table->orig_nents, GFP_KERNEL)) {
		kfree(iovm_map);
		return NULL;
	}

	new_sg = iovm_map->table.sgl;
	for_each_sg(table->sgl, sg, table->orig_nents, i) {
		memcpy(new_sg, sg, sizeof(*sg));
		new_sg->dma_address = 0;
		new_sg = sg_next(new_sg);
	}

	iovm_map->dev = a->dev;
	iovm_map->attrs = a->dma_map_attrs;

	return iovm_map;
}

static void ion_iova_remove(struct ion_iovm_map *iovm_map)
{
	sg_free_table(&iovm_map->table);
	kfree(iovm_map);
}

static bool is_ion_tzmp_buffer(struct ion_buffer *buffer, struct device *dev)
{
	return ion_buffer_protected(buffer) &&
		buffer->priv_virt && is_dev_samsung_tzmp(dev);
}

static void ion_iova_release(struct dma_buf *dmabuf)
{
	struct ion_buffer *buffer = dmabuf->priv;
	struct ion_iovm_map *iovm_map, *tmp;

	list_for_each_entry_safe(iovm_map, tmp, &buffer->attachments, list) {
		if (iovm_map->mapcnt)
			WARN(1, "iova_map refcount leak found for %s\n",
			     dev_name(iovm_map->dev));

		list_del(&iovm_map->list);
		if (!is_ion_tzmp_buffer(buffer, iovm_map->dev))
			dma_unmap_sg_attrs(iovm_map->dev, iovm_map->table.sgl,
					   iovm_map->table.orig_nents,
					   DMA_TO_DEVICE,
					   DMA_ATTR_SKIP_CPU_SYNC);
		ion_iova_remove(iovm_map);
	}
}

/*
 * If dma_map_attrs affects device virtual address mapping
 * that should be added on here.
 *
 * DMA_ATTR_PRIVILEGED : No sharable mapping.
 */
#define DMA_MAP_ATTRS_MASK	DMA_ATTR_PRIVILEGED
#define DMA_MAP_ATTRS(attrs)	((attrs) & DMA_MAP_ATTRS_MASK)

static void ion_set_noncached_attrs(struct dma_buf_attachment *a)
{
	struct ion_buffer *buffer = a->dmabuf->priv;

	if (buffer->flags & ION_FLAG_CACHED)
		return;

	/*
	 * Non-cached buffer is mapped with DMA_ATTR_PRIVILEGED for
	 * unsharable mapping because non-cached buffer is flushed when
	 * allocated and mapped as non-cached.
	 * The device of sharable domain doesn't need to observe the memory.
	 */
	a->dma_map_attrs |= (DMA_ATTR_PRIVILEGED | DMA_ATTR_SKIP_CPU_SYNC);
}

/* this function should only be called while buffer->lock is held */
static struct ion_iovm_map *ion_find_iovm_map(struct dma_buf_attachment *a)
{
	struct ion_buffer *buffer = a->dmabuf->priv;
	struct ion_iovm_map *iovm_map;
	unsigned long attrs;

	ion_set_noncached_attrs(a);
	attrs = DMA_MAP_ATTRS(a->dma_map_attrs);

	list_for_each_entry(iovm_map, &buffer->attachments, list) {
		// device virtual mapping doesn't consider direction currently.
		if ((iommu_get_domain_for_dev(iovm_map->dev) ==
		    iommu_get_domain_for_dev(a->dev)) &&
		    (DMA_MAP_ATTRS(iovm_map->attrs) == attrs)) {
			return iovm_map;
		}
	}
	return NULL;
}

static struct ion_iovm_map *ion_put_iovm_map(struct dma_buf_attachment *a)
{
	struct ion_iovm_map *iovm_map;
	struct ion_buffer *buffer = a->dmabuf->priv;

	mutex_lock(&buffer->lock);
	iovm_map = ion_find_iovm_map(a);
	if (iovm_map) {
		iovm_map->mapcnt--;

		if (!iovm_map->mapcnt && (a->dma_map_attrs & DMA_ATTR_SKIP_LAZY_UNMAP)) {
			list_del(&iovm_map->list);
			if (!is_ion_tzmp_buffer(buffer, iovm_map->dev))
				dma_unmap_sg_attrs(iovm_map->dev, iovm_map->table.sgl,
						   iovm_map->table.orig_nents,
						   DMA_TO_DEVICE,
						   DMA_ATTR_SKIP_CPU_SYNC);
			ion_iova_remove(iovm_map);
			iovm_map = NULL;
		}
	}
	mutex_unlock(&buffer->lock);

	return iovm_map;
}

static struct ion_iovm_map *ion_get_iovm_map(struct dma_buf_attachment *a,
		enum dma_data_direction direction)
{
	struct ion_buffer *buffer = a->dmabuf->priv;
	struct ion_iovm_map *iovm_map, *dup_iovm_map;

	mutex_lock(&buffer->lock);
	iovm_map = ion_find_iovm_map(a);
	if (iovm_map) {
		iovm_map->mapcnt++;
		mutex_unlock(&buffer->lock);
		return iovm_map;
	}
	mutex_unlock(&buffer->lock);

	iovm_map = ion_iova_create(a);
	if (!iovm_map)
		return NULL;

	if (is_ion_tzmp_buffer(buffer, a->dev)) {
		struct ion_buffer_prot_info *info = buffer->priv_virt;

		sg_dma_address(iovm_map->table.sgl) = info->dma_addr;
		sg_dma_len(iovm_map->table.sgl) =
			info->chunk_count * info->chunk_size;
		iovm_map->table.nents = 1;
	} else {
		iovm_map->table.nents =
			dma_map_sg_attrs(iovm_map->dev, iovm_map->table.sgl,
					 iovm_map->table.nents, direction,
					 iovm_map->attrs | DMA_ATTR_SKIP_CPU_SYNC);

		if (!iovm_map->table.nents) {
			ion_iova_remove(iovm_map);
			return NULL;
		}
	}

	mutex_lock(&buffer->lock);
	dup_iovm_map = ion_find_iovm_map(a);
	if (!dup_iovm_map) {
		list_add(&iovm_map->list, &buffer->attachments);
	} else {
		if (!is_ion_tzmp_buffer(buffer, iovm_map->dev))
			dma_unmap_sg_attrs(iovm_map->dev, iovm_map->table.sgl,
					   iovm_map->table.orig_nents,
					   direction, DMA_ATTR_SKIP_CPU_SYNC);
		ion_iova_remove(iovm_map);

		iovm_map = dup_iovm_map;
	}
	iovm_map->mapcnt++;

	mutex_unlock(&buffer->lock);

	return iovm_map;
}

static bool ion_skip_cpu_sync(struct dma_buf *dmabuf, unsigned long map_attrs)
{
	struct ion_buffer *buffer = dmabuf->priv;

	if (ion_buffer_protected(buffer))
	       return true;

	if (!(buffer->flags & ION_FLAG_CACHED))
		return true;

	if (map_attrs & DMA_ATTR_SKIP_CPU_SYNC)
		return true;

	return false;
}

static struct sg_table *ion_map_dma_buf(struct dma_buf_attachment *attachment,
					enum dma_data_direction direction)
{
	struct ion_iovm_map *iovm_map;
	struct ion_buffer *buffer = attachment->dmabuf->priv;
	struct ion_heap *heap = buffer->heap;

	ion_bltin_event_begin();

	if (heap->buf_ops.map_dma_buf)
		return heap->buf_ops.map_dma_buf(attachment, direction);

	iovm_map = ion_get_iovm_map(attachment, direction);
	if (!iovm_map)
		return ERR_PTR(-ENOMEM);

	if (!ion_skip_cpu_sync(attachment->dmabuf, attachment->dma_map_attrs))
		dma_sync_sg_for_device(iovm_map->dev,
				       iovm_map->table.sgl,
				       iovm_map->table.orig_nents,
				       direction);

	ion_bltin_event_record(ION_BLTIN_EVENT_DMA_MAP, buffer, begin);

	return &iovm_map->table;
}

static void ion_unmap_dma_buf(struct dma_buf_attachment *attachment,
			      struct sg_table *table,
			      enum dma_data_direction direction)
{
	struct ion_buffer *buffer = attachment->dmabuf->priv;
	struct ion_heap *heap = buffer->heap;

	ion_bltin_event_begin();

	if (heap->buf_ops.unmap_dma_buf)
		return heap->buf_ops.unmap_dma_buf(attachment, table,
						   direction);

	if (!ion_skip_cpu_sync(attachment->dmabuf, attachment->dma_map_attrs))
		dma_sync_sg_for_cpu(attachment->dev, table->sgl, table->orig_nents, direction);

	ion_put_iovm_map(attachment);

	ion_bltin_event_record(ION_BLTIN_EVENT_DMA_UNMAP, buffer, begin);
}

static void ion_dma_buf_release(struct dma_buf *dmabuf)
{
	struct ion_buffer *buffer = dmabuf->priv;
	struct ion_heap *heap = buffer->heap;

	if (heap->buf_ops.release)
		return heap->buf_ops.release(dmabuf);
	/*
	 * If heap has flags of ION_HEAP_FLAG_DEFER_FREE, the list is used
	 * by freelist of heap. System heap uses the buffer list by debugging
	 * so we have to delete here before ion_free.
	 */
	ion_remove_bltin_buffer(to_bltin_heap(heap), buffer);

	ion_iova_release(dmabuf);
	ion_free(buffer);
}

static int ion_dma_buf_begin_cpu_access(struct dma_buf *dmabuf,
					enum dma_data_direction direction)
{
	struct ion_buffer *buffer = dmabuf->priv;
	struct ion_heap *heap = buffer->heap;
	struct ion_iovm_map *iovm_map;

	ion_bltin_event_begin();

	if (heap->buf_ops.begin_cpu_access)
		return heap->buf_ops.begin_cpu_access(dmabuf, direction);

	mutex_lock(&buffer->lock);

	if (ion_skip_cpu_sync(dmabuf, 0))
		goto unlock;

	list_for_each_entry(iovm_map, &buffer->attachments, list) {
		if (iovm_map->mapcnt && !dev_is_dma_coherent(iovm_map->dev)) {
			dma_sync_sg_for_cpu(iovm_map->dev, iovm_map->table.sgl,
					    iovm_map->table.orig_nents,
					    direction);
			break;
		}
	}

	ion_bltin_event_record(ION_BLTIN_EVENT_CPU_BEGIN, buffer, begin);
unlock:
	mutex_unlock(&buffer->lock);
	return 0;
}

static void dma_sync_sg_partial(struct device *dev, struct sg_table *sgt,
				unsigned int offset, unsigned int len,
				enum dma_data_direction dir, unsigned int flag)
{
	int i;
	unsigned int size;
	struct scatterlist *sg;

	for_each_sg(sgt->sgl, sg, sgt->orig_nents, i) {
		if (offset >= sg->length) {
			offset -= sg->length;
			continue;
		}

		size = min_t(unsigned int, len, sg->length - offset);
		len -= size;

		if (flag & DMA_BUF_SYNC_END)
			dma_sync_single_range_for_device(dev, sg_phys(sg), offset, size, dir);
		else
			dma_sync_single_range_for_cpu(dev, sg_phys(sg), offset, size, dir);

		offset = 0;

		if (!len)
			break;
	}
}

static int
ion_dma_buf_begin_cpu_access_partial(struct dma_buf *dmabuf,
				     enum dma_data_direction direction,
				     unsigned int offset, unsigned int len)
{
	struct ion_buffer *buffer = dmabuf->priv;
	struct ion_heap *heap = buffer->heap;
	struct ion_iovm_map *iovm_map;

	ion_bltin_event_begin();

	if (heap->buf_ops.begin_cpu_access_partial)
		return heap->buf_ops.begin_cpu_access_partial(dmabuf,
							      direction,
							      offset, len);
	if (ion_skip_cpu_sync(dmabuf, 0))
		return 0;

	mutex_lock(&buffer->lock);
	list_for_each_entry(iovm_map, &buffer->attachments, list) {
		if (iovm_map->mapcnt && !dev_is_dma_coherent(iovm_map->dev)) {
			dma_sync_sg_partial(to_bltin_heap(heap)->dev,
					    &iovm_map->table,
					    offset, len, direction,
					    DMA_BUF_SYNC_START);
			break;
		}
	}
	mutex_unlock(&buffer->lock);
	ion_bltin_event_record(ION_BLTIN_EVENT_CPU_BEGIN_PARTIAL, buffer,
			       begin);

	return 0;
}

static int ion_dma_buf_end_cpu_access(struct dma_buf *dmabuf,
				      enum dma_data_direction direction)
{
	struct ion_buffer *buffer = dmabuf->priv;
	struct ion_heap *heap = buffer->heap;
	struct ion_iovm_map *iovm_map;

	ion_bltin_event_begin();

	if (heap->buf_ops.end_cpu_access)
		return heap->buf_ops.end_cpu_access(dmabuf, direction);

	mutex_lock(&buffer->lock);

	if (ion_skip_cpu_sync(dmabuf, 0))
		goto unlock;

	list_for_each_entry(iovm_map, &buffer->attachments, list) {
		if (iovm_map->mapcnt && !dev_is_dma_coherent(iovm_map->dev)) {
			dma_sync_sg_for_device(iovm_map->dev,
					       iovm_map->table.sgl,
					       iovm_map->table.orig_nents,
					       direction);
			break;
		}
	}
	ion_bltin_event_record(ION_BLTIN_EVENT_CPU_END, buffer, begin);
unlock:
	mutex_unlock(&buffer->lock);

	return 0;
}

static int ion_dma_buf_end_cpu_access_partial(struct dma_buf *dmabuf,
					      enum dma_data_direction direction,
					      unsigned int offset,
					      unsigned int len)
{
	struct ion_buffer *buffer = dmabuf->priv;
	struct ion_heap *heap = buffer->heap;
	struct ion_iovm_map *iovm_map;

	ion_bltin_event_begin();

	if (heap->buf_ops.end_cpu_access_partial)
		return heap->buf_ops.end_cpu_access_partial(dmabuf, direction,
							    offset, len);

	if (ion_skip_cpu_sync(dmabuf, 0))
		return 0;

	mutex_lock(&buffer->lock);
	list_for_each_entry(iovm_map, &buffer->attachments, list) {
		if (iovm_map->mapcnt && !dev_is_dma_coherent(iovm_map->dev)) {
			dma_sync_sg_partial(to_bltin_heap(heap)->dev,
					    &iovm_map->table,
					    offset, len, direction,
					    DMA_BUF_SYNC_END);
			break;
		}
	}
	mutex_unlock(&buffer->lock);
	ion_bltin_event_record(ION_BLTIN_EVENT_CPU_END_PARTIAL, buffer, begin);

	return 0;
}

static void *ion_dma_buf_map(struct dma_buf *dmabuf, unsigned long offset)
{
	struct ion_buffer *buffer = dmabuf->priv;
	struct ion_heap *heap = buffer->heap;

	if (heap->buf_ops.map)
		return heap->buf_ops.map(dmabuf, offset);

	return ion_buffer_kmap_get(buffer) + offset * PAGE_SIZE;
}

static int ion_dma_buf_mmap(struct dma_buf *dmabuf, struct vm_area_struct *vma)
{
	struct ion_buffer *buffer = dmabuf->priv;
	struct ion_heap *heap = buffer->heap;
	int ret;

	ion_bltin_event_begin();

	/* now map it to userspace */
	if (heap->buf_ops.mmap) {
		ret = heap->buf_ops.mmap(dmabuf, vma);
	} else {
		if (ion_buffer_protected(buffer)) {
			pr_err("%s: mmap() to secure buffer is not allowed\n",
			       __func__);
			return -EACCES;
		}

		mutex_lock(&buffer->lock);
		if (!(buffer->flags & ION_FLAG_CACHED))
			vma->vm_page_prot =
				pgprot_writecombine(vma->vm_page_prot);

		ret = ion_heap_map_user(heap, buffer, vma);
		mutex_unlock(&buffer->lock);
	}

	if (ret)
		pr_err("%s: failure mapping buffer to userspace\n", __func__);

	ion_bltin_event_record(ION_BLTIN_EVENT_MMAP, buffer, begin);

	return ret;
}

static void ion_dma_buf_unmap(struct dma_buf *dmabuf, unsigned long offset,
			      void *addr)
{
	struct ion_buffer *buffer = dmabuf->priv;
	struct ion_heap *heap = buffer->heap;

	if (!heap->buf_ops.unmap)
		return;
	heap->buf_ops.unmap(dmabuf, offset, addr);
}

static void *ion_dma_buf_vmap(struct dma_buf *dmabuf)
{
	struct ion_buffer *buffer = dmabuf->priv;
	struct ion_heap *heap = buffer->heap;
	void *vaddr;

	ion_bltin_event_begin();

	if (heap->buf_ops.vmap)
		return heap->buf_ops.vmap(dmabuf);

	mutex_lock(&buffer->lock);
	vaddr = ion_buffer_kmap_get(buffer);
	mutex_unlock(&buffer->lock);

	ion_bltin_event_record(ION_BLTIN_EVENT_VMAP, buffer, begin);

	return vaddr;
}

static void ion_dma_buf_vunmap(struct dma_buf *dmabuf, void *vaddr)
{
	struct ion_buffer *buffer = dmabuf->priv;
	struct ion_heap *heap = buffer->heap;

	if (heap->buf_ops.vunmap) {
		heap->buf_ops.vunmap(dmabuf, vaddr);
		return;
	}

	mutex_lock(&buffer->lock);
	ion_buffer_kmap_put(buffer);
	mutex_unlock(&buffer->lock);
}

static int ion_dma_buf_get_flags(struct dma_buf *dmabuf, unsigned long *flags)
{
	struct ion_buffer *buffer = dmabuf->priv;
	struct ion_heap *heap = buffer->heap;

	if (heap->buf_ops.get_flags)
		return heap->buf_ops.get_flags(dmabuf, flags);

	*flags = (unsigned long)ION_HEAP_MASK(buffer->heap->type) << ION_HEAP_SHIFT;
	*flags |= ION_BUFFER_MASK(buffer->flags);

	return 0;
}

static const struct dma_buf_ops dma_buf_ops = {
	.map_dma_buf = ion_map_dma_buf,
	.unmap_dma_buf = ion_unmap_dma_buf,
	.release = ion_dma_buf_release,
	.begin_cpu_access = ion_dma_buf_begin_cpu_access,
	.begin_cpu_access_partial = ion_dma_buf_begin_cpu_access_partial,
	.end_cpu_access = ion_dma_buf_end_cpu_access,
	.end_cpu_access_partial = ion_dma_buf_end_cpu_access_partial,
	.mmap = ion_dma_buf_mmap,
	.map = ion_dma_buf_map,
	.unmap = ion_dma_buf_unmap,
	.vmap = ion_dma_buf_vmap,
	.vunmap = ion_dma_buf_vunmap,
	.get_flags = ion_dma_buf_get_flags,
};

struct dma_buf *ion_dmabuf_alloc(struct ion_device *dev, size_t len,
				 unsigned int heap_id_mask,
				 unsigned int flags)
{
	struct ion_buffer *buffer;
	DEFINE_DMA_BUF_EXPORT_INFO(exp_info);
	struct dma_buf *dmabuf;

	pr_debug("%s: len %zu heap_id_mask %u flags %x\n", __func__,
		 len, heap_id_mask, flags);

	buffer = ion_buffer_alloc(dev, len, heap_id_mask, flags);
	if (IS_ERR(buffer))
		return ERR_CAST(buffer);

	exp_info.ops = &dma_buf_ops;
	exp_info.size = buffer->size;
	exp_info.flags = O_RDWR;
	exp_info.priv = buffer;

	dmabuf = dma_buf_export(&exp_info);
	if (IS_ERR(dmabuf))
		ion_buffer_destroy(dev, buffer);

	return dmabuf;
}
