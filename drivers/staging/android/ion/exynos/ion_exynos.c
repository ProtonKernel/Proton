// SPDX-License-Identifier: GPL-2.0
/*
 * ION Memory Allocator exynos feature support
 *
 * Copyright (c) 2020 Samsung Electronics Co., Ltd.
 */

#include <linux/mm.h>
#include <linux/device.h>
#include <linux/of_device.h>
#include <linux/module.h>
#include <linux/highmem.h>
#include <linux/slab.h>
#include <linux/ion.h>
#include <linux/dma-mapping.h>
#include <linux/scatterlist.h>
#include <linux/seq_file.h>
#include <linux/dma-noncoherent.h>
#include <linux/iommu.h>
#include <uapi/linux/dma-buf.h>

#include "ion_exynos.h"
#include "ion_exynos_prot.h"
#include "ion_debug.h"

void ion_exynos_heap_init(struct ion_exynos_heap *exynos_heap)
{
	INIT_LIST_HEAD(&exynos_heap->list);
	mutex_init(&exynos_heap->buffer_lock);

	ion_heap_debug_init(exynos_heap);
}

void ion_add_exynos_buffer(struct ion_exynos_heap *exynos_heap,
			   struct ion_buffer *buffer)
{
	mutex_lock(&exynos_heap->buffer_lock);
	list_add(&buffer->list, &exynos_heap->list);
	mutex_unlock(&exynos_heap->buffer_lock);
}

void ion_remove_exynos_buffer(struct ion_exynos_heap *exynos_heap,
			      struct ion_buffer *buffer)
{
	mutex_lock(&exynos_heap->buffer_lock);
	list_del(&buffer->list);
	mutex_unlock(&exynos_heap->buffer_lock);
}

void ion_page_clean(struct page *pages, unsigned long size)
{
	unsigned long nr_pages, i;

	if (!PageHighMem(pages)) {
		memset(page_address(pages), 0, size);
		return;
	}

	nr_pages = PAGE_ALIGN(size) >> PAGE_SHIFT;

	for (i = 0; i < nr_pages; i++) {
		void *vaddr = kmap_atomic(&pages[i]);

		memset(vaddr, 0, PAGE_SIZE);
		kunmap_atomic(vaddr);
	}
}

unsigned int ion_get_heapmask_by_name(const char *heap_name)
{
	struct ion_heap_data data[ION_NUM_MAX_HEAPS];
	int i, cnt = ion_query_heaps_kernel(NULL, 0);

	ion_query_heaps_kernel((struct ion_heap_data *)data, cnt);
	for (i = 0; i < cnt; i++) {
		if (!strncmp(data[i].name, heap_name, MAX_HEAP_NAME))
			break;
	}

	if (i == cnt)
		return 0;

	return 1 << data[i].heap_id;
}

static inline bool ion_buffer_cached(struct ion_buffer *buffer)
{
	return !!(buffer->flags & ION_FLAG_CACHED);
}

void exynos_fdt_setup(struct device *dev, struct exynos_fdt_attrs *attrs)
{
	unsigned int alignment, order;

	of_property_read_u32(dev->of_node, "ion,alignment", &alignment);
	order = get_order(alignment);

	if (order > MAX_ORDER)
		order = 0;

	attrs->alignment = 1 << (order + PAGE_SHIFT);
	attrs->secure = of_property_read_bool(dev->of_node, "ion,secure");
	attrs->untouchable = of_property_read_bool(dev->of_node, "ion,untouchable");
	of_property_read_u32(dev->of_node, "ion,protection_id",
			     &attrs->protection_id);
}

static int exynos_dma_buf_mmap(struct dma_buf *dmabuf,
			       struct vm_area_struct *vma)
{
	struct ion_buffer *buffer = dmabuf->priv;
	int ret;

	ion_event_begin();

	if (ion_buffer_protected(buffer)) {
		perr("mmap() to protected buffer is not allowed");
		return -EACCES;
	}

	mutex_lock(&buffer->lock);
	if (!ion_buffer_cached(buffer))
		vma->vm_page_prot = pgprot_writecombine(vma->vm_page_prot);

	ret = ion_heap_map_user(buffer->heap, buffer, vma);
	mutex_unlock(&buffer->lock);

	if (ret)
		perr("failure mapping buffer to userspace");

	ion_event_record(ION_EVENT_TYPE_MMAP, buffer, begin);

	return ret;
}

static int exynos_dma_buf_attach(struct dma_buf *dmabuf,
				 struct dma_buf_attachment *att)
{
	return 0;
}

static void exynos_dma_buf_detach(struct dma_buf *dmabuf,
				   struct dma_buf_attachment *att)
{
}

struct exynos_iovm_map {
	struct list_head list;
	struct device *dev;
	struct sg_table table;
	unsigned long attrs;
	unsigned int mapcnt;
};

static struct exynos_iovm_map *exynos_iova_create(struct dma_buf_attachment *a)
{
	struct ion_buffer *buffer = a->dmabuf->priv;
	struct exynos_iovm_map *iovm_map;
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

static void exynos_iova_remove(struct exynos_iovm_map *iovm_map)
{
	sg_free_table(&iovm_map->table);
	kfree(iovm_map);
}

static bool is_exynos_tzmp_buffer(struct ion_buffer *buffer, struct device *dev)
{
	return ion_buffer_protected(buffer) &&
		buffer->priv_virt && is_dev_samsung_tzmp(dev);
}

static void exynos_iova_release(struct dma_buf *dmabuf)
{
	struct ion_buffer *buffer = dmabuf->priv;
	struct exynos_iovm_map *iovm_map, *tmp;

	list_for_each_entry_safe(iovm_map, tmp, &buffer->attachments, list) {
		if (iovm_map->mapcnt)
			WARN(1, "iova_map refcount leak found for %s\n",
			     dev_name(iovm_map->dev));

		list_del(&iovm_map->list);
		if (!is_exynos_tzmp_buffer(buffer, iovm_map->dev))
			dma_unmap_sg_attrs(iovm_map->dev, iovm_map->table.sgl,
					   iovm_map->table.orig_nents,
					   DMA_TO_DEVICE,
					   DMA_ATTR_SKIP_CPU_SYNC);
		exynos_iova_remove(iovm_map);
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

static void exynos_set_noncached_attrs(struct dma_buf_attachment *a)
{
	struct ion_buffer *buffer = a->dmabuf->priv;

	if (ion_buffer_cached(buffer))
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
static struct exynos_iovm_map *find_iovm_map(struct dma_buf_attachment *a)
{
	struct ion_buffer *buffer = a->dmabuf->priv;
	struct exynos_iovm_map *iovm_map;
	unsigned long attrs;

	exynos_set_noncached_attrs(a);
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

static struct exynos_iovm_map *exynos_put_iovm_map(struct dma_buf_attachment *a)
{
	struct exynos_iovm_map *iovm_map;
	struct ion_buffer *buffer = a->dmabuf->priv;

	mutex_lock(&buffer->lock);
	iovm_map = find_iovm_map(a);
	if (iovm_map) {
		iovm_map->mapcnt--;

		if (!iovm_map->mapcnt && (a->dma_map_attrs & DMA_ATTR_SKIP_LAZY_UNMAP)) {
			list_del(&iovm_map->list);
			if (!is_exynos_tzmp_buffer(buffer, iovm_map->dev))
				dma_unmap_sg_attrs(iovm_map->dev, iovm_map->table.sgl,
						   iovm_map->table.orig_nents,
						   DMA_TO_DEVICE,
						   DMA_ATTR_SKIP_CPU_SYNC);
			exynos_iova_remove(iovm_map);
			iovm_map = NULL;
		}
	}
	mutex_unlock(&buffer->lock);

	return iovm_map;
}

static struct exynos_iovm_map *exynos_get_iovm_map(struct dma_buf_attachment *a,
						   enum dma_data_direction dir)
{
	struct ion_buffer *buffer = a->dmabuf->priv;
	struct exynos_iovm_map *iovm_map, *dup_iovm_map;

	mutex_lock(&buffer->lock);
	iovm_map = find_iovm_map(a);
	if (iovm_map) {
		iovm_map->mapcnt++;
		mutex_unlock(&buffer->lock);
		return iovm_map;
	}
	mutex_unlock(&buffer->lock);

	iovm_map = exynos_iova_create(a);
	if (!iovm_map)
		return NULL;

	if (is_exynos_tzmp_buffer(buffer, a->dev)) {
		struct ion_buffer_prot_info *info = buffer->priv_virt;

		sg_dma_address(iovm_map->table.sgl) = info->dma_addr;
		sg_dma_len(iovm_map->table.sgl) =
			info->chunk_count * info->chunk_size;
		iovm_map->table.nents = 1;
	} else {
		iovm_map->table.nents =
			dma_map_sg_attrs(iovm_map->dev, iovm_map->table.sgl,
					 iovm_map->table.orig_nents, dir,
					 iovm_map->attrs | DMA_ATTR_SKIP_CPU_SYNC);

		if (!iovm_map->table.nents) {
			exynos_iova_remove(iovm_map);
			return NULL;
		}
	}

	mutex_lock(&buffer->lock);
	/*
	 * check if another thread has already push the iovm map
	 * for the same device and buffer before pushing one more.
	 */
	dup_iovm_map = find_iovm_map(a);
	if (!dup_iovm_map) {
		list_add(&iovm_map->list, &buffer->attachments);
	} else {
		if (!is_exynos_tzmp_buffer(buffer, iovm_map->dev))
			dma_unmap_sg_attrs(iovm_map->dev, iovm_map->table.sgl,
					   iovm_map->table.orig_nents,
					   dir, DMA_ATTR_SKIP_CPU_SYNC);
		exynos_iova_remove(iovm_map);
		iovm_map = dup_iovm_map;
	}
	iovm_map->mapcnt++;

	mutex_unlock(&buffer->lock);

	return iovm_map;
}

static bool skip_cpu_sync(struct dma_buf *dmabuf, unsigned long map_attrs)
{
	struct ion_buffer *buffer = dmabuf->priv;

	if (ion_buffer_protected(buffer))
	       return true;

	if (!ion_buffer_cached(buffer))
		return true;

	if (map_attrs & DMA_ATTR_SKIP_CPU_SYNC)
		return true;

	return false;
}

static struct sg_table *exynos_map_dma_buf(struct dma_buf_attachment *att,
					   enum dma_data_direction direction)
{
	struct ion_buffer *buffer = att->dmabuf->priv;
	struct exynos_iovm_map *iovm_map;

	ion_event_begin();

	iovm_map = exynos_get_iovm_map(att, direction);
	if (!iovm_map)
		return ERR_PTR(-ENOMEM);

	if (!skip_cpu_sync(att->dmabuf, att->dma_map_attrs))
		dma_sync_sg_for_device(iovm_map->dev,
				       iovm_map->table.sgl,
				       iovm_map->table.orig_nents,
				       direction);

	ion_event_record(ION_EVENT_TYPE_DMA_MAP, buffer, begin);

	return &iovm_map->table;
}

static void exynos_unmap_dma_buf(struct dma_buf_attachment *att,
				 struct sg_table *table,
				 enum dma_data_direction direction)
{
	struct ion_buffer *buffer = att->dmabuf->priv;

	ion_event_begin();

	if (!skip_cpu_sync(att->dmabuf, att->dma_map_attrs))
		dma_sync_sg_for_cpu(att->dev, table->sgl, table->orig_nents, direction);

	exynos_put_iovm_map(att);

	ion_event_record(ION_EVENT_TYPE_DMA_UNMAP, buffer, begin);
}

static void exynos_sync_sg_partial(struct device *dev, struct sg_table *sgt,
				unsigned int offset, unsigned int len,
				enum dma_data_direction dir, unsigned int flag)
{
	int i;
	size_t size;
	struct scatterlist *sg;

	for_each_sg(sgt->sgl, sg, sgt->orig_nents, i) {
		if (offset >= sg->length) {
			offset -= sg->length;
			continue;
		}

		size = min_t(size_t, len, sg->length - offset);
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
exynos_dma_buf_begin_cpu_access_partial(struct dma_buf *dmabuf,
				     enum dma_data_direction direction,
				     unsigned int offset, unsigned int len)
{
	struct ion_buffer *buffer = dmabuf->priv;
	struct exynos_iovm_map *iovm_map;

	ion_event_begin();

	if (skip_cpu_sync(dmabuf, 0))
		return 0;

	mutex_lock(&buffer->lock);
	list_for_each_entry(iovm_map, &buffer->attachments, list) {
		if (iovm_map->mapcnt && !dev_is_dma_coherent(iovm_map->dev)) {
			exynos_sync_sg_partial(to_exynos_heap(buffer->heap)->dev,
					       &iovm_map->table, offset, len,
					       direction, DMA_BUF_SYNC_START);
			break;
		}
	}
	mutex_unlock(&buffer->lock);
	ion_event_record(ION_EVENT_TYPE_CPU_BEGIN_PARTIAL, buffer, begin);

	return 0;
}

static int exynos_dma_buf_end_cpu_access_partial(struct dma_buf *dmabuf,
					      enum dma_data_direction direction,
					      unsigned int offset,
					      unsigned int len)
{
	struct ion_buffer *buffer = dmabuf->priv;
	struct exynos_iovm_map *iovm_map;

	ion_event_begin();

	if (skip_cpu_sync(dmabuf, 0))
		return 0;

	mutex_lock(&buffer->lock);
	list_for_each_entry(iovm_map, &buffer->attachments, list) {
		if (iovm_map->mapcnt && !dev_is_dma_coherent(iovm_map->dev)) {
			exynos_sync_sg_partial(to_exynos_heap(buffer->heap)->dev,
					       &iovm_map->table, offset, len,
					       direction, DMA_BUF_SYNC_END);
			break;
		}
	}
	mutex_unlock(&buffer->lock);
	ion_event_record(ION_EVENT_TYPE_CPU_END_PARTIAL, buffer, begin);

	return 0;
}

static void exynos_dma_buf_release(struct dma_buf *dmabuf)
{
	struct ion_buffer *buffer = dmabuf->priv;

	exynos_iova_release(dmabuf);

	ion_free(buffer);
}

void *exynos_buffer_kmap_get(struct ion_buffer *buffer)
{
	void *vaddr;

	if (buffer->kmap_cnt) {
		buffer->kmap_cnt++;
		return buffer->vaddr;
	}
	vaddr = ion_heap_map_kernel(buffer->heap, buffer);
	if (IS_ERR(vaddr)) {
		perrfn("failed to alloc kernel address of %zu buffer\n",
		       buffer->size);
		return vaddr;
	}
	buffer->vaddr = vaddr;
	buffer->kmap_cnt++;
	return vaddr;
}

void exynos_buffer_kmap_put(struct ion_buffer *buffer)
{
	buffer->kmap_cnt--;
	if (!buffer->kmap_cnt) {
		ion_heap_unmap_kernel(buffer->heap, buffer);
		buffer->vaddr = NULL;
	}
}

static int exynos_dma_buf_begin_cpu_access(struct dma_buf *dmabuf,
					   enum dma_data_direction direction)
{
	struct ion_buffer *buffer = dmabuf->priv;
	struct exynos_iovm_map *iovm_map;

	ion_event_begin();

	mutex_lock(&buffer->lock);

	if (skip_cpu_sync(dmabuf, 0))
		goto unlock;

	list_for_each_entry(iovm_map, &buffer->attachments, list) {
		if (iovm_map->mapcnt && !dev_is_dma_coherent(iovm_map->dev)) {
			dma_sync_sg_for_cpu(iovm_map->dev, iovm_map->table.sgl,
					    iovm_map->table.orig_nents,
					    direction);
			break;
		}
	}

	ion_event_record(ION_EVENT_TYPE_CPU_BEGIN, buffer, begin);
unlock:
	mutex_unlock(&buffer->lock);

	return 0;
}

static int exynos_dma_buf_end_cpu_access(struct dma_buf *dmabuf,
					 enum dma_data_direction direction)
{
	struct ion_buffer *buffer = dmabuf->priv;
	struct exynos_iovm_map *iovm_map;

	ion_event_begin();

	mutex_lock(&buffer->lock);

	if (skip_cpu_sync(dmabuf, 0))
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
	ion_event_record(ION_EVENT_TYPE_CPU_END, buffer, begin);
unlock:
	mutex_unlock(&buffer->lock);

	return 0;
}

static void *exynos_dma_buf_vmap(struct dma_buf *dmabuf)
{
	struct ion_buffer *buffer = dmabuf->priv;
	struct ion_exynos_heap *exynos_heap = to_exynos_heap(buffer->heap);
	void *vaddr;

	ion_event_begin();

	if (exynos_heap->attrs.untouchable) {
		perrfn("mmap of %s heap unallowed", buffer->heap->name);
		return ERR_PTR(-EACCES);
	}

	mutex_lock(&buffer->lock);
	vaddr = exynos_buffer_kmap_get(buffer);
	mutex_unlock(&buffer->lock);

	ion_event_record(ION_EVENT_TYPE_VMAP, buffer, begin);

	return vaddr;
}

static void exynos_dma_buf_vunmap(struct dma_buf *dmabuf, void *vaddr)
{
	struct ion_buffer *buffer = dmabuf->priv;

	mutex_lock(&buffer->lock);
	exynos_buffer_kmap_put(buffer);
	mutex_unlock(&buffer->lock);
}

static int exynos_dma_buf_get_flags(struct dma_buf *dmabuf, unsigned long *flags)
{
	struct ion_buffer *buffer = dmabuf->priv;
	unsigned long prot_id = to_exynos_heap(buffer->heap)->attrs.protection_id;

	*flags = (unsigned long)ION_HEAP_MASK(buffer->heap->type) << ION_HEAP_SHIFT;
	*flags |= ION_HEAP_MASK(prot_id) << ION_HEAP_PROTID_SHIFT;
	*flags |= ION_BUFFER_MASK(buffer->flags);

	return 0;
}

const struct dma_buf_ops exynos_dma_buf_ops = {
	.attach = exynos_dma_buf_attach,
	.detach = exynos_dma_buf_detach,
	.map_dma_buf = exynos_map_dma_buf,
	.unmap_dma_buf = exynos_unmap_dma_buf,
	.release = exynos_dma_buf_release,
	.begin_cpu_access = exynos_dma_buf_begin_cpu_access,
	.begin_cpu_access_partial = exynos_dma_buf_begin_cpu_access_partial,
	.end_cpu_access = exynos_dma_buf_end_cpu_access,
	.end_cpu_access_partial = exynos_dma_buf_end_cpu_access_partial,
	.vmap = exynos_dma_buf_vmap,
	.vunmap = exynos_dma_buf_vunmap,
	.mmap = exynos_dma_buf_mmap,
	.get_flags = exynos_dma_buf_get_flags,
};

static int __init ion_exynos_init(void)
{
	int ret;

	ret = ion_secure_iova_pool_create();
	if (ret)
		return ret;

	ret = ion_exynos_cma_heap_init();
	if (ret)
		goto err_cma;

	ret = ion_carveout_heap_init();
	if (ret)
		goto err_carveout;

	ion_debug_init();

	return 0;
err_carveout:
	ion_exynos_cma_heap_exit();
err_cma:
	ion_secure_iova_pool_destroy();

	return ret;
}

static void __exit ion_exynos_exit(void)
{
	ion_carveout_heap_exit();
	ion_exynos_cma_heap_exit();
	ion_secure_iova_pool_destroy();
}

module_init(ion_exynos_init);
module_exit(ion_exynos_exit);
MODULE_LICENSE("GPL v2");
