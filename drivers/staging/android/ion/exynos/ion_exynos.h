/* SPDX-License-Identifier: GPL-2.0 */
/*
 * ION Memory Allocator exynos feature support
 *
 * Copyright (c) 2020 Samsung Electronics Co., Ltd.
 */

#ifndef _ION_EXYNOS_H
#define _ION_EXYNOS_H

#include <uapi/linux/ion.h>
#include <linux/of_reserved_mem.h>

#include "ion_exynos_prot.h"

void ion_page_clean(struct page *pages, unsigned long size);

enum ion_exynos_heap_type {
	ION_EXYNOS_HEAP_TYPE_CARVEOUT = ION_HEAP_TYPE_CUSTOM + 1,
};

#if defined(CONFIG_ION_EXYNOS_CMA_HEAP)
int __init ion_exynos_cma_heap_init(void);
void ion_exynos_cma_heap_exit(void);
#else
static inline int __init ion_exynos_cma_heap_init(void)
{
	return 0;
}

#define ion_exynos_cma_heap_exit() do { } while (0)
#endif

#if defined(CONFIG_ION_EXYNOS_CARVEOUT_HEAP)
int __init ion_carveout_heap_init(void);
void ion_carveout_heap_exit(void);
#else
static inline int __init ion_carveout_heap_init(void)
{
	return 0;
}

#define ion_carveout_heap_exit() do { } while (0)
#endif

unsigned int ion_get_heapmask_by_name(const char *heap_name);

struct exynos_fdt_attrs {
	unsigned int alignment;
	unsigned int protection_id;
	bool secure;
	bool untouchable;
};

void exynos_fdt_setup(struct device *dev, struct exynos_fdt_attrs *attrs);

extern const struct dma_buf_ops exynos_dma_buf_ops;

#if IS_ENABLED(CONFIG_EXYNOS_CONTENT_PATH_PROTECTION)
int ion_secure_iova_pool_create(void);
void ion_secure_iova_pool_destroy(void);
void *ion_buffer_protect(struct device *dev, unsigned int protection_id,
			 unsigned int size, unsigned long phys,
			 unsigned int protalign);
int ion_buffer_unprotect(void *priv);
#else
static inline int ion_secure_iova_pool_create(void)
{
	return 0;
}

#define ion_secure_iova_pool_destroy() do { } while (0)

static inline void *ion_buffer_protect(struct device *dev,
				       unsigned int protection_id,
				       unsigned int size, unsigned long phys,
				       unsigned int protalign)
{
	return NULL;
}

static inline int ion_buffer_unprotect(void *priv)
{
	return 0;
}
#endif

#define to_exynos_heap(x) container_of(x, struct ion_exynos_heap, heap)
/*
 * Wrapper heap to manage the buffers of heap.
 */
struct ion_exynos_heap {
	struct ion_heap heap;
	struct device *dev;
	struct list_head list;
	struct mutex buffer_lock;
	phys_addr_t base;
	phys_addr_t size;
	struct exynos_fdt_attrs attrs;
};

void ion_exynos_heap_init(struct ion_exynos_heap *exynos_heap);
void ion_add_exynos_buffer(struct ion_exynos_heap *exynos_heap,
			   struct ion_buffer *buffer);
void ion_remove_exynos_buffer(struct ion_exynos_heap *exynos_heap,
			      struct ion_buffer *buffer);

#define prseq(s, fmt, ...) ((s) ? seq_printf(s, fmt, ##__VA_ARGS__) :\
			    pr_cont(fmt, ##__VA_ARGS__))

#define IONPREFIX "[Exynos][ION] "
#define perr(format, arg...) \
	pr_err(IONPREFIX format "\n", ##arg)

#define perrfn(format, arg...) \
	pr_err(IONPREFIX "%s: " format "\n", __func__, ##arg)

#define perrdev(dev, format, arg...) \
	dev_err(dev, IONPREFIX format "\n", ##arg)

#define perrfndev(dev, format, arg...) \
	dev_err(dev, IONPREFIX "%s: " format "\n", __func__, ##arg)

#endif
