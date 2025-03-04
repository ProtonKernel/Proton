// SPDX-License-Identifier: GPL-2.0
/*
 * ION Memory Allocator HPA heap exporter
 *
 * Copyright (c) 2020 Samsung Electronics Co., Ltd.
 */

#include <linux/platform_device.h>
#include <linux/device.h>
#include <linux/ion.h>
#include <linux/slab.h>
#include <linux/errno.h>
#include <linux/err.h>
#include <linux/scatterlist.h>
#include <linux/highmem.h>
#include <linux/mod_devicetable.h>
#include <linux/of_device.h>
#include <linux/of_reserved_mem.h>
#include <linux/kernel.h>
#include <linux/sort.h>
#include <linux/gfp.h>
#include <linux/genalloc.h>
#include <linux/smc.h>
#include <linux/kmemleak.h>
#include <linux/dma-mapping.h>
#include <soc/samsung/exynos-smc.h>

#include "ion_exynos_prot.h"
#include "../ion_bltin.h"

static struct device hpa_dev;

#if IS_ENABLED(CONFIG_EXYNOS_CONTENT_PATH_PROTECTION)

#define HPA_SECURE_DMA_BASE	0x40000000
#define HPA_SECURE_DMA_END	0x80000000

static struct gen_pool *hpa_iova_pool;
static DEFINE_SPINLOCK(hpa_iova_pool_lock);

static int hpa_secure_iova_pool_create(void)
{
	hpa_iova_pool = gen_pool_create(PAGE_SHIFT, -1);
	if (!hpa_iova_pool) {
		pr_err("%s: failed to create Secure IOVA pool\n", __func__);
		return -ENOMEM;
	}

	if (gen_pool_add(hpa_iova_pool, HPA_SECURE_DMA_BASE,
			 HPA_SECURE_DMA_END - HPA_SECURE_DMA_BASE, -1)) {
		pr_err("failed to set address range of Secure IOVA pool\n",
		       __func__);
		return -ENOMEM;
	}

	return 0;
}

#define MAX_IOVA_ALIGNMENT      12
static unsigned long find_first_fit_with_align(unsigned long *map,
					       unsigned long size,
					       unsigned long start,
					       unsigned int nr, void *data,
					       struct gen_pool *pool,
					       unsigned long start_addr)
{
	unsigned long align = ((*(unsigned int *)data) >> PAGE_SHIFT);

	if (align > (1 << MAX_IOVA_ALIGNMENT))
		align = (1 << MAX_IOVA_ALIGNMENT);

	return bitmap_find_next_zero_area(map, size, start, nr, (align - 1));
}

static int hpa_iova_alloc(unsigned long *addr, unsigned long size,
			  unsigned int align)
{
	unsigned long out_addr;

	if (!hpa_iova_pool) {
		pr_err("%s: Secure IOVA pool is not created\n", __func__);
		return -ENODEV;
	}

	spin_lock(&hpa_iova_pool_lock);
	if (align > PAGE_SIZE) {
		gen_pool_set_algo(hpa_iova_pool,
				  find_first_fit_with_align, &align);
		out_addr = gen_pool_alloc(hpa_iova_pool, size);
		gen_pool_set_algo(hpa_iova_pool, NULL, NULL);
	} else {
		out_addr = gen_pool_alloc(hpa_iova_pool, size);
	}
	spin_unlock(&hpa_iova_pool_lock);

	if (out_addr == 0) {
		pr_err("%s: failed alloc secure iova. %zu/%zu bytes used\n",
		       __func__, gen_pool_avail(hpa_iova_pool),
		       gen_pool_size(hpa_iova_pool));
		return -ENOMEM;
	}

	*addr = out_addr;

	return 0;
}

static void hpa_iova_free(unsigned long addr, unsigned long size)
{
	if (!hpa_iova_pool) {
		pr_err("%s: Secure IOVA pool is not created\n", __func__);
		return;
	}

	spin_lock(&hpa_iova_pool_lock);
	gen_pool_free(hpa_iova_pool, addr, size);
	spin_unlock(&hpa_iova_pool_lock);
}

static int hpa_secure_protect(struct ion_buffer_prot_info *protdesc,
			      unsigned int protalign)
{
	unsigned long size = protdesc->chunk_count * protdesc->chunk_size;
	unsigned long drmret = 0, dma_addr = 0;
	int ret;

	ret = hpa_iova_alloc(&dma_addr, size,
			     max_t(u32, protalign, PAGE_SIZE));
	if (ret)
		goto err_iova;

	protdesc->dma_addr = (unsigned int)dma_addr;

	dma_map_single(&hpa_dev, protdesc, sizeof(*protdesc), DMA_TO_DEVICE);
	if (protdesc->chunk_count > 1)
		dma_map_single(&hpa_dev, phys_to_virt(protdesc->bus_address),
			       sizeof(unsigned long) * protdesc->chunk_count,
			       DMA_TO_DEVICE);

	drmret = exynos_smc(SMC_DRM_PPMP_PROT, virt_to_phys(protdesc), 0, 0);
	if (drmret != DRMDRV_OK) {
		ret = -EACCES;
		goto err_smc;
	}

	return 0;
err_smc:
	hpa_iova_free(dma_addr, size);
err_iova:
	pr_err("%s: PROT:%#x (err=%d,va=%#lx,len=%#lx,cnt=%u,flg=%u)\n",
	       __func__, SMC_DRM_PPMP_PROT, drmret, dma_addr, size,
	       protdesc->chunk_count, protdesc->flags);

	return ret;
}

static int hpa_secure_unprotect(struct ion_buffer_prot_info *protdesc)
{
	unsigned long size = protdesc->chunk_count * protdesc->chunk_size;
	unsigned long ret;
	/*
	 * No need to flush protdesc for unprotection because it is never
	 * modified since the buffer is protected.
	 */
	ret = exynos_smc(SMC_DRM_PPMP_UNPROT, virt_to_phys(protdesc), 0, 0);

	if (ret != DRMDRV_OK) {
		pr_err("%s: UNPROT:%d(err=%d,va=%#x,len=%#lx,cnt=%u,flg=%u)\n",
		       __func__, SMC_DRM_PPMP_UNPROT, ret, protdesc->dma_addr,
		       size, protdesc->chunk_count, protdesc->flags);
		return -EACCES;
	}
	/*
	 * retain the secure device address if unprotection to its area fails.
	 * It might be unusable forever since we do not know the state o ft he
	 * secure world before returning error from exynos_smc() above.
	 */
	hpa_iova_free(protdesc->dma_addr, size);

	return 0;
}

static void *ion_hpa_protect(unsigned int protection_id, unsigned int count,
			     unsigned int chunk_size, unsigned long *phys_arr,
			     unsigned int protalign)
{
	struct ion_buffer_prot_info *protdesc;
	int ret;

	protdesc = kmalloc(sizeof(*protdesc), GFP_KERNEL);
	if (!protdesc)
		return ERR_PTR(-ENOMEM);

	/*
	 * The address pointed by phys_arr is stored to the protection metadata
	 * after conversion to its physical address.
	 */
	kmemleak_ignore(phys_arr);

	protdesc->chunk_count = count,
	protdesc->flags = protection_id;
	protdesc->chunk_size = chunk_size;

	if (count == 1)
		protdesc->bus_address = phys_arr[0];
	else
		protdesc->bus_address = virt_to_phys(phys_arr);

	ret = hpa_secure_protect(protdesc, protalign);
	if (ret) {
		pr_err("%s: protection failure (id%u,chk%u,count%u,align%#x\n",
		       __func__, protection_id, chunk_size, count, protalign);
		kfree(protdesc);
		return ERR_PTR(ret);
	}

	return protdesc;
}

static int ion_hpa_unprotect(void *priv)
{
	struct ion_buffer_prot_info *protdesc = priv;
	int ret = 0;

	if (!priv)
		return 0;

	ret = hpa_secure_unprotect(protdesc);
	if (protdesc->chunk_count > 1)
		kfree(phys_to_virt(protdesc->bus_address));
	kfree(protdesc);

	return ret;
}
#else
static int hpa_secure_iova_pool_create(void)
{
	return 0;
}

static void *ion_hpa_protect(unsigned int protection_id, unsigned int count,
			     unsigned int chunk_size, unsigned long *phys_arr,
			     unsigned int protalign)
{
	return NULL;
}

static int ion_hpa_unprotect(void *priv)
{
	return 0;
}
#endif
static phys_addr_t hpa_exception_areas[][2] = {
	{ 0x880000000, 0x89FFFFFFF }, /* cp linear */
	{ 0xC00000000, -1 }, /* hpa limit */
};

struct ion_hpa_heap {
	struct ion_bltin_heap bltin_heap;
	unsigned int protection_id;
	bool secure;
};

#define to_hpa_heap(x) (container_of(to_bltin_heap(x), \
			struct ion_hpa_heap, bltin_heap))

#define ION_EXYNOS_HEAP_TYPE_HPA ION_HEAP_TYPE_CUSTOM

struct ion_hpa_heap hpa_heaps[] = {
	{
		.bltin_heap = {
			.heap = {
				.name = "vfw_heap",
				.type = ION_EXYNOS_HEAP_TYPE_HPA,
			},
		},
		.protection_id = 2,
		.secure = true,
	}, {
		.bltin_heap = {
			.heap = {
				.name = "vframe_heap",
				.type = ION_EXYNOS_HEAP_TYPE_HPA,
			},
		},
		.protection_id = 5,
		.secure = true,
	}, {
		.bltin_heap = {
			.heap = {
				.name = "vscaler_heap",
				.type = ION_EXYNOS_HEAP_TYPE_HPA,
			},
		},
		.protection_id = 6,
		.secure = true,
	}, {
		.bltin_heap = {
			.heap = {
				.name = "gpu_buffer",
				.type = ION_EXYNOS_HEAP_TYPE_HPA,
			},
		},
		.protection_id = 9,
		.secure = true,
	},
};

#define ION_HPA_DEFAULT_ORDER 4
#define ION_HPA_CHUNK_SIZE  (PAGE_SIZE << ION_HPA_DEFAULT_ORDER)
#define ION_HPA_PAGE_COUNT(len) \
	(ALIGN(len, ION_HPA_CHUNK_SIZE) / ION_HPA_CHUNK_SIZE)
#define HPA_MAX_CHUNK_COUNT ((PAGE_SIZE * 2) / sizeof(struct page *))

static int ion_hpa_compare_pages(const void *p1, const void *p2)
{
	if (*((unsigned long *)p1) > (*((unsigned long *)p2)))
		return 1;
	else if (*((unsigned long *)p1) < (*((unsigned long *)p2)))
		return -1;
	return 0;
}

static int ion_hpa_allocate(struct ion_heap *heap,
			    struct ion_buffer *buffer, unsigned long len,
			    unsigned long flags)
{
	struct ion_hpa_heap *hpa_heap = to_hpa_heap(heap);
	unsigned int count =
		(unsigned int)ION_HPA_PAGE_COUNT((unsigned int)len);
	size_t desc_size = sizeof(struct page *) * count;
	struct page **pages;
	unsigned long *phys;
	struct sg_table *sgt;
	struct scatterlist *sg;
	int i, ret;
	bool protected = hpa_heap->secure && ion_buffer_protected(buffer);

	ion_bltin_event_begin();

	if (count > HPA_MAX_CHUNK_COUNT) {
		pr_info("ION HPA heap does not allow buffers > %zu\n",
			HPA_MAX_CHUNK_COUNT * ION_HPA_CHUNK_SIZE);
		return -ENOMEM;
	}

	pages = kmalloc(desc_size, GFP_KERNEL);
	if (!pages)
		return -ENOMEM;

	sgt = kmalloc(sizeof(*sgt), GFP_KERNEL);
	if (!sgt) {
		ret = -ENOMEM;
		goto err_sgt;
	}

	ret = sg_alloc_table(sgt, count, GFP_KERNEL);
	if (ret)
		goto err_sg;

	ret = alloc_pages_highorder_except(ION_HPA_DEFAULT_ORDER, pages, count,
					   hpa_exception_areas,
					   protected ?
					   ARRAY_SIZE(hpa_exception_areas) : 0);
	if (ret)
		goto err_pages;

	sort(pages, count, sizeof(*pages), ion_hpa_compare_pages, NULL);

	/*
	 * convert a page descriptor into its corresponding physical address
	 * in place to reduce memory allocation
	 */
	phys = (unsigned long *)pages;

	for_each_sg(sgt->sgl, sg, sgt->orig_nents, i) {
		memset(page_address(pages[i]), 0, ION_HPA_CHUNK_SIZE);
		sg_set_page(sg, pages[i], ION_HPA_CHUNK_SIZE, 0);
		phys[i] = page_to_phys(pages[i]);
	}

	buffer->sg_table = sgt;
	ion_buffer_prep_noncached(buffer);

	if (protected) {
		buffer->priv_virt = ion_hpa_protect(hpa_heap->protection_id,
						    count, ION_HPA_CHUNK_SIZE,
						    phys, ION_HPA_CHUNK_SIZE);
		if (IS_ERR(buffer->priv_virt)) {
			ret = PTR_ERR(buffer->priv_virt);
			goto err_prot;
		}
		/*
		 * If it is a single chunk, the array of pages are not needed.
		 * The physical address for one chunk is stored directly
		 * in bus_address of ion_buffer_prot_info.
		 */
		if (count == 1)
			kfree(pages);
	} else {
		kfree(pages);
	}

	ion_add_bltin_buffer(&hpa_heap->bltin_heap, buffer);
	ion_bltin_event_record(ION_BLTIN_EVENT_ALLOC, buffer, begin);

	return 0;
err_prot:
	for_each_sg(sgt->sgl, sg, sgt->orig_nents, i)
		__free_pages(sg_page(sg), ION_HPA_DEFAULT_ORDER);
err_pages:
	sg_free_table(sgt);
err_sg:
	kfree(sgt);
err_sgt:
	kfree(pages);

	return ret;
}

static void ion_hpa_free(struct ion_buffer *buffer)
{
	struct ion_hpa_heap *hpa_heap = to_hpa_heap(buffer->heap);
	struct sg_table *sgt = buffer->sg_table;
	struct scatterlist *sg;
	int i;
	int unprot_err = 0;

	ion_bltin_event_begin();

	if (hpa_heap->secure && ion_buffer_protected(buffer))
		unprot_err = ion_hpa_unprotect(buffer->priv_virt);

	if (!unprot_err)
		for_each_sg(sgt->sgl, sg, sgt->orig_nents, i)
			__free_pages(sg_page(sg), ION_HPA_DEFAULT_ORDER);
	sg_free_table(sgt);
	kfree(sgt);

	ion_bltin_event_record(ION_BLTIN_EVENT_FREE, buffer, begin);
}

static struct ion_heap_ops ion_hpa_ops = {
	.allocate = ion_hpa_allocate,
	.free = ion_hpa_free,
};

static int ion_hpa_get_flags(struct dma_buf *dmabuf, unsigned long *flags)
{
	struct ion_buffer *buffer = dmabuf->priv;
	unsigned long prot_id = to_hpa_heap(buffer->heap)->protection_id;

	*flags = (unsigned long)ION_HEAP_MASK(buffer->heap->type) << ION_HEAP_SHIFT;
	*flags |= ION_HEAP_MASK(prot_id) << ION_HEAP_PROTID_SHIFT;
	*flags |= ION_BUFFER_MASK(buffer->flags);

	return 0;
}

const struct dma_buf_ops ion_hpa_buf_ops = {
	.get_flags = ion_hpa_get_flags,
};

static int __init exynos_hpa_init(void)
{
	int ret, i = 0;

	ret = hpa_secure_iova_pool_create();
	if (ret)
		return ret;

	device_initialize(&hpa_dev);
	arch_setup_dma_ops(&hpa_dev, 0x0ULL, 1ULL << 36, NULL, false);
	dma_coerce_mask_and_coherent(&hpa_dev, DMA_BIT_MASK(36));

	for (i = 0; i < ARRAY_SIZE(hpa_heaps); i++) {
		hpa_heaps[i].bltin_heap.heap.ops = &ion_hpa_ops;
		hpa_heaps[i].bltin_heap.heap.buf_ops = ion_hpa_buf_ops;
		hpa_heaps[i].bltin_heap.dev = &hpa_dev;

		ret = ion_device_add_heap(&hpa_heaps[i].bltin_heap.heap);
		if (ret)
			return ret;

		ion_bltin_heap_init(&hpa_heaps[i].bltin_heap);

		pr_info("Register ion hpa heap %s successfully\n",
			hpa_heaps[i].bltin_heap.heap.name);
	}

	return 0;
}
module_init(exynos_hpa_init);
MODULE_LICENSE("GPL v2");
