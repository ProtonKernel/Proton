// SPDX-License-Identifier: GPL-2.0
/*
 * ION Memory Allocator carveout heap exporter
 *
 * Copyright (c) 2020 Samsung Electronics Co., Ltd.
 */

#include <linux/dma-mapping.h>
#include <linux/err.h>
#include <linux/genalloc.h>
#include <linux/io.h>
#include <linux/mm.h>
#include <linux/scatterlist.h>
#include <linux/slab.h>
#include <linux/mod_devicetable.h>
#include <linux/of_device.h>
#include <linux/of_reserved_mem.h>
#include <linux/highmem.h>
#include <linux/ion.h>

#include "ion_exynos.h"
#include "ion_debug.h"

struct ion_carveout_heap {
	struct ion_exynos_heap exynos_heap;
	struct platform_device *pdev;
	struct gen_pool *pool;
};

#define to_carveout_heap(x) (container_of(to_exynos_heap(x), \
			     struct ion_carveout_heap, exynos_heap))

static phys_addr_t ion_carveout_allocate(struct ion_heap *heap,
					 unsigned long size)
{
	struct ion_carveout_heap *carveout_heap = to_carveout_heap(heap);

	return gen_pool_alloc(carveout_heap->pool, size);
}

static void ion_carveout_free(struct ion_heap *heap, phys_addr_t addr,
			      unsigned long size)
{
	struct ion_carveout_heap *carveout_heap = to_carveout_heap(heap);

	gen_pool_free(carveout_heap->pool, addr, size);
}

static int ion_carveout_heap_allocate(struct ion_heap *heap,
				      struct ion_buffer *buffer,
				      unsigned long size,
				      unsigned long flags)
{
	struct ion_carveout_heap *carveout_heap = to_carveout_heap(heap);
	struct exynos_fdt_attrs *attrs = &carveout_heap->exynos_heap.attrs;
	unsigned long alloc_size = ALIGN(size, attrs->alignment);
	struct sg_table *table;
	phys_addr_t paddr;
	int ret;

	ion_event_begin();

	if (attrs->untouchable && !ion_buffer_protected(buffer)) {
		perrfn("ION_FLAG_PROTECTED needed by untouchable heap %s",
		       heap->name);
		return -EACCES;
	}

	table = kmalloc(sizeof(*table), GFP_KERNEL);
	if (!table)
		return -ENOMEM;
	ret = sg_alloc_table(table, 1, GFP_KERNEL);
	if (ret) {
		perr("failed to alloc sgtable with 1 entry (err %d)", -ret);
		goto err_free;
	}

	paddr = ion_carveout_allocate(heap, alloc_size);
	if (!paddr) {
		perr("failed to allocate from %s(id %u/type %d), size %lu",
		       heap->name, heap->id, heap->type, size);
		ret = -ENOMEM;
		goto err_free_table;
	}

	sg_set_page(table->sgl, phys_to_page(paddr), alloc_size, 0);

	buffer->sg_table = table;

	/*
	 * No need to flush more than the requiered size. But clearing dirty
	 * data from the CPU caches should be performed on the entire area
	 * to be protected because writing back from the CPU caches with non-
	 * secure property to the protected area results system error.
	 */
	if (!attrs->untouchable)
		ion_buffer_prep_noncached(buffer);

	if (attrs->secure && ion_buffer_protected(buffer)) {
		buffer->priv_virt =
			ion_buffer_protect(&carveout_heap->pdev->dev,
					   attrs->protection_id,
					   (unsigned int)alloc_size, paddr,
					   attrs->alignment);
		if (IS_ERR(buffer->priv_virt)) {
			ret = PTR_ERR(buffer->priv_virt);
			goto err_prot;
		}
	}

	ion_add_exynos_buffer(to_exynos_heap(heap), buffer);
	ion_event_record(ION_EVENT_TYPE_ALLOC, buffer, begin);

	return 0;
err_prot:
	ion_carveout_free(heap, paddr, buffer->size);
err_free_table:
	sg_free_table(table);
err_free:
	kfree(table);

	ion_contig_heap_show(NULL, to_exynos_heap(heap));

	return ret;
}

static void ion_carveout_heap_free(struct ion_buffer *buffer)
{
	struct ion_heap *heap = buffer->heap;
	struct ion_carveout_heap *carveout_heap = to_carveout_heap(heap);
	struct sg_table *table = buffer->sg_table;
	struct page *page = sg_page(table->sgl);
	phys_addr_t paddr = PFN_PHYS(page_to_pfn(page));
	struct exynos_fdt_attrs *attrs = &carveout_heap->exynos_heap.attrs;
	int unprot_err = 0;
	unsigned long alloc_size = ALIGN(buffer->size, attrs->alignment);

	ion_event_begin();

	ion_remove_exynos_buffer(to_exynos_heap(heap), buffer);

	if (attrs->secure && ion_buffer_protected(buffer))
		unprot_err = ion_buffer_unprotect(buffer->priv_virt);

	/*
	 * If releasing H/W protection to a buffer fails, the buffer might not
	 * be unusable in Linux forever because we do not have an idea to
	 * determine if the buffer is accessible in Linux. So, we should mark
	 * that buffer unusable with holding allocated buffer.
	 */
	if (!unprot_err) {
		/*
		 * There is no need to map and memset with write combine for
		 * non-cachable buffer because that is flushed when allocation
		 */
		if (!attrs->untouchable)
			ion_page_clean(page, buffer->size);
		ion_carveout_free(heap, paddr, alloc_size);
	}

	sg_free_table(table);
	kfree(table);

	ion_event_record(ION_EVENT_TYPE_FREE, buffer, begin);
}

static struct ion_heap_ops carveout_heap_ops = {
	.allocate = ion_carveout_heap_allocate,
	.free = ion_carveout_heap_free,
};

/*
 * reserve_bootmem_region marks the page PageReserved
 * from PFN_DOWN(start_address) to end_pfn PFN_UP(end_address)
 * If rmem base and size is not aligned with PAGE SIZE,
 * the pages containing start address, and end address are
 * also reserved.
 */
static void ion_carveout_reserved_free(struct reserved_mem *rmem)
{
	struct page *first = phys_to_page(rmem->base & PAGE_MASK);
	struct page *last = phys_to_page(PAGE_ALIGN(rmem->base + rmem->size));
	struct page *page;

	for (page = first; page != last; page++)
		free_reserved_page(page);
}

static int ion_carveout_heap_probe(struct platform_device *pdev)
{
	struct ion_carveout_heap *carveout_heap;
	struct reserved_mem *rmem;
	struct device_node *rmem_np;
	int ret;

	rmem_np = of_parse_phandle(pdev->dev.of_node, "memory-region", 0);
	rmem = of_reserved_mem_lookup(rmem_np);
	if (!rmem) {
		perrfn("%s: memory-region handle not found", pdev->name);
		return -ENODEV;
	}

	if (ion_get_heapmask_by_name(rmem->name)) {
		ion_carveout_reserved_free(rmem);
		return 0;
	}

	carveout_heap = devm_kzalloc(&pdev->dev, sizeof(*carveout_heap),
				     GFP_KERNEL);
	if (!carveout_heap)
		return -ENOMEM;

	carveout_heap->pool = gen_pool_create(PAGE_SHIFT, -1);
	if (!carveout_heap->pool)
		return -ENOMEM;

	gen_pool_add(carveout_heap->pool, rmem->base, rmem->size, -1);

	carveout_heap->exynos_heap.heap.ops = &carveout_heap_ops;
	carveout_heap->exynos_heap.heap.name = rmem->name;
	carveout_heap->exynos_heap.heap.type =
		(enum ion_heap_type)ION_EXYNOS_HEAP_TYPE_CARVEOUT;
	carveout_heap->exynos_heap.heap.buf_ops = exynos_dma_buf_ops;

	carveout_heap->exynos_heap.base = rmem->base;
	carveout_heap->exynos_heap.size = rmem->size;
	carveout_heap->exynos_heap.dev = &pdev->dev;

	carveout_heap->pdev = pdev;

	arch_setup_dma_ops(&pdev->dev, 0x0ULL, 1ULL << 36, NULL, false);
	dma_coerce_mask_and_coherent(&pdev->dev, DMA_BIT_MASK(36));

	exynos_fdt_setup(&pdev->dev, &carveout_heap->exynos_heap.attrs);

	if (!carveout_heap->exynos_heap.attrs.untouchable)
		ion_page_clean(phys_to_page(rmem->base), rmem->size);

	ret = ion_device_add_heap(&carveout_heap->exynos_heap.heap);
	if (ret) {
		gen_pool_destroy(carveout_heap->pool);
		return ret;
	}

	platform_set_drvdata(pdev, carveout_heap);

	ion_exynos_heap_init(&carveout_heap->exynos_heap);

	pr_info("Register ion carveout heap %s successfully\n", pdev->name);

	return 0;
}

static int ion_carveout_heap_remove(struct platform_device *pdev)
{
	struct ion_carveout_heap *carveout_heap = platform_get_drvdata(pdev);

	ion_device_remove_heap(&carveout_heap->exynos_heap.heap);
	gen_pool_destroy(carveout_heap->pool);

	return 0;
}

static const struct of_device_id carveout_heap_of_match[] = {
	{ .compatible = "exynos,ion,carveout_heap", },
	{ },
};

MODULE_DEVICE_TABLE(of, carveout_heap_of_match);

static struct platform_driver ion_carveout_heap_driver = {
	.driver		= {
		.name	= "ion_carveout_heap",
		.of_match_table = carveout_heap_of_match,
	},
	.probe		= ion_carveout_heap_probe,
	.remove		= ion_carveout_heap_remove,
};

int __init ion_carveout_heap_init(void)
{
	return platform_driver_register(&ion_carveout_heap_driver);
}

void ion_carveout_heap_exit(void)
{
	platform_driver_unregister(&ion_carveout_heap_driver);
}
