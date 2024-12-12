// SPDX-License-Identifier: GPL-2.0
/*
 * Samsung Exynos SoC series dsp driver
 *
 * Copyright (c) 2020 Samsung Electronics Co., Ltd.
 *              http://www.samsung.com/
 */

#include "dsp-log.h"
#include "dsp-hw-dummy-system.h"
#include "dsp-hw-dummy-memory.h"

int dsp_hw_dummy_memory_map_buffer(struct dsp_memory *mem,
		struct dsp_buffer *buf)
{
	dsp_enter();
	dsp_leave();
	return 0;
}

int dsp_hw_dummy_memory_unmap_buffer(struct dsp_memory *mem,
		struct dsp_buffer *buf)
{
	dsp_enter();
	dsp_leave();
	return 0;
}

int dsp_hw_dummy_memory_sync_for_device(struct dsp_memory *mem,
		struct dsp_buffer *buf)
{
	dsp_enter();
	dsp_leave();
	return 0;
}

int dsp_hw_dummy_memory_sync_for_cpu(struct dsp_memory *mem,
		struct dsp_buffer *buf)
{
	dsp_enter();
	dsp_leave();
	return 0;
}

int dsp_hw_dummy_memory_alloc(struct dsp_memory *mem,
		struct dsp_priv_mem *pmem)
{
	dsp_enter();
	dsp_leave();
	return 0;
}

void dsp_hw_dummy_memory_free(struct dsp_memory *mem,
		struct dsp_priv_mem *pmem)
{
	dsp_enter();
	dsp_leave();
}

int dsp_hw_dummy_memory_ion_alloc(struct dsp_memory *mem,
		struct dsp_priv_mem *pmem)
{
	dsp_enter();
	dsp_leave();
	return 0;
}

void dsp_hw_dummy_memory_ion_free(struct dsp_memory *mem,
		struct dsp_priv_mem *pmem)
{
	dsp_enter();
	dsp_leave();
}

int dsp_hw_dummy_memory_ion_alloc_secure(struct dsp_memory *mem,
		struct dsp_priv_mem *pmem)
{
	dsp_enter();
	dsp_leave();
	return 0;
}

void dsp_hw_dummy_memory_ion_free_secure(struct dsp_memory *mem,
		struct dsp_priv_mem *pmem)
{
	dsp_enter();
	dsp_leave();
}

int dsp_hw_dummy_memory_open(struct dsp_memory *mem)
{
	dsp_enter();
	dsp_leave();
	return 0;
}

int dsp_hw_dummy_memory_close(struct dsp_memory *mem)
{
	dsp_enter();
	dsp_leave();
	return 0;
}

int dsp_hw_dummy_memory_probe(struct dsp_system *sys)
{
	dsp_enter();
	dsp_leave();
	return 0;
}

void dsp_hw_dummy_memory_remove(struct dsp_memory *mem)
{
	dsp_enter();
	dsp_leave();
}
