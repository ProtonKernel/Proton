/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Samsung Exynos SoC series dsp driver
 *
 * Copyright (c) 2020 Samsung Electronics Co., Ltd.
 *              http://www.samsung.com/
 */

#ifndef __HW_DUMMY_DSP_HW_DUMMY_MEMORY_H__
#define __HW_DUMMY_DSP_HW_DUMMY_MEMORY_H__

#include "hardware/dsp-memory.h"

int dsp_hw_dummy_memory_map_buffer(struct dsp_memory *mem,
		struct dsp_buffer *buf);
int dsp_hw_dummy_memory_unmap_buffer(struct dsp_memory *mem,
		struct dsp_buffer *buf);
int dsp_hw_dummy_memory_sync_for_device(struct dsp_memory *mem,
		struct dsp_buffer *buf);
int dsp_hw_dummy_memory_sync_for_cpu(struct dsp_memory *mem,
		struct dsp_buffer *buf);
int dsp_hw_dummy_memory_alloc(struct dsp_memory *mem,
		struct dsp_priv_mem *pmem);
void dsp_hw_dummy_memory_free(struct dsp_memory *mem,
		struct dsp_priv_mem *pmem);
int dsp_hw_dummy_memory_ion_alloc(struct dsp_memory *mem,
		struct dsp_priv_mem *pmem);
void dsp_hw_dummy_memory_ion_free(struct dsp_memory *mem,
		struct dsp_priv_mem *pmem);
int dsp_hw_dummy_memory_ion_alloc_secure(struct dsp_memory *mem,
		struct dsp_priv_mem *pmem);
void dsp_hw_dummy_memory_ion_free_secure(struct dsp_memory *mem,
		struct dsp_priv_mem *pmem);

int dsp_hw_dummy_memory_open(struct dsp_memory *mem);
int dsp_hw_dummy_memory_close(struct dsp_memory *mem);
int dsp_hw_dummy_memory_probe(struct dsp_system *sys);
void dsp_hw_dummy_memory_remove(struct dsp_memory *mem);

#endif
