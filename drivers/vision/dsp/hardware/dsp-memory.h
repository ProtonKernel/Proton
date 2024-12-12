/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Samsung Exynos SoC series dsp driver
 *
 * Copyright (c) 2019 Samsung Electronics Co., Ltd.
 *              http://www.samsung.com/
 */

#ifndef __HW_DSP_MEMORY_H__
#define __HW_DSP_MEMORY_H__

#include <linux/dma-buf.h>

#define DSP_MEM_NAME_LEN		(16)

struct dsp_system;
struct dsp_memory;

struct dsp_buffer {
	int				fd;
	size_t				size;
	unsigned int			offset;
	bool				cached;
	struct list_head		list;

	struct dma_buf			*dbuf;
	size_t				dbuf_size;
	enum dma_data_direction		dir;
	struct dma_buf_attachment	*attach;
	struct sg_table			*sgt;
	dma_addr_t			iova;
	void				*kvaddr;
};

struct dsp_priv_mem {
	char				name[DSP_MEM_NAME_LEN];
	size_t				size;
	size_t				min_size;
	size_t				max_size;
	size_t				used_size;
	long				flags;
	bool				kmap;
	bool				fixed_iova;
	bool				backup;

	struct dma_buf			*dbuf;
	size_t				dbuf_size;
	enum dma_data_direction		dir;
	struct dma_buf_attachment	*attach;
	struct sg_table			*sgt;
	dma_addr_t			iova;
	void				*kvaddr;
	void				*bac_kvaddr;
};

struct dsp_reserved_mem {
	char				name[DSP_MEM_NAME_LEN];
	phys_addr_t			paddr;
	size_t				size;
	size_t				used_size;
	dma_addr_t			iova;
	long				flags;
	bool				kmap;
	void				*kvaddr;
};

struct dsp_memory_ops {
	int (*map_buffer)(struct dsp_memory *mem, struct dsp_buffer *buf);
	int (*unmap_buffer)(struct dsp_memory *mem, struct dsp_buffer *buf);
	int (*sync_for_device)(struct dsp_memory *mem, struct dsp_buffer *buf);
	int (*sync_for_cpu)(struct dsp_memory *mem, struct dsp_buffer *buf);
	int (*alloc)(struct dsp_memory *mem, struct dsp_priv_mem *pmem);
	void (*free)(struct dsp_memory *mem, struct dsp_priv_mem *pmem);
	int (*ion_alloc)(struct dsp_memory *mem, struct dsp_priv_mem *pmem);
	void (*ion_free)(struct dsp_memory *mem, struct dsp_priv_mem *pmem);
	int (*ion_alloc_secure)(struct dsp_memory *mem,
			struct dsp_priv_mem *pmem);
	void (*ion_free_secure)(struct dsp_memory *mem,
			struct dsp_priv_mem *pmem);

	int (*open)(struct dsp_memory *mem);
	int (*close)(struct dsp_memory *mem);
	int (*probe)(struct dsp_system *sys);
	void (*remove)(struct dsp_memory *mem);
};

struct dsp_memory {
	struct device			*dev;
	struct iommu_domain		*domain;
	struct dsp_priv_mem		*priv_mem;
	struct dsp_reserved_mem		*reserved_mem;
	struct dsp_priv_mem		sec_mem;

	unsigned int			priv_mem_count;
	unsigned int			priv_ivp_pm_id;
	unsigned int			priv_dl_out_id;
	const struct dsp_memory_ops	*ops;
};

int dsp_memory_set_ops(struct dsp_memory *mem, unsigned int dev_id);
int dsp_memory_register_ops(unsigned int dev_id,
		const struct dsp_memory_ops *ops);

#endif
