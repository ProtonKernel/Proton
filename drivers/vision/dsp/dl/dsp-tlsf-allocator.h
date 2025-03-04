/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Samsung Exynos SoC series dsp driver
 *
 * Copyright (c) 2019 Samsung Electronics Co., Ltd.
 *              http://www.samsung.com/
 */

#ifndef __DL_DSP_TLSF_ALLOCATOR_H__
#define __DL_DSP_TLSF_ALLOCATOR_H__

#include "dl/dsp-common.h"
#include "dl/dsp-list.h"

#define TLSF_MIN_BLOCK_SHIFT	(5)
#define TLSF_MIN_BLOCK_SIZE	(1 << TLSF_MIN_BLOCK_SHIFT)

#define TLSF_SL_SHIFT		(3)
#define TLSF_SL_SIZE		(1 << TLSF_SL_SHIFT)

struct dsp_lib;

struct dsp_tlsf_idx {
	int fl;
	int sl;
};

enum dsp_tlsf_mem_type {
	MEM_EMPTY,
	MEM_USE
};

struct dsp_tlsf_mem {
	enum dsp_tlsf_mem_type type;
	unsigned long start_addr;
	size_t size;

	struct dsp_lib *lib;

	struct dsp_list_node mem_list_node;
	struct dsp_list_node tlsf_node;
	struct dsp_tlsf_idx tlsf_idx;
};

struct dsp_tlsf {
	unsigned int fl;
	unsigned char *sl;
	struct dsp_list_head (*fb)[TLSF_SL_SIZE];
	struct dsp_list_head mem_list;
	unsigned int align;

	size_t max_size;
	unsigned char max_sh;
};

const char *dsp_tlsf_mem_type_to_str(enum dsp_tlsf_mem_type type);

void dsp_tlsf_mem_init(struct dsp_tlsf_mem *mem);
void dsp_tlsf_mem_print(struct dsp_tlsf_mem *mem);
struct dsp_tlsf_mem *dsp_tlsf_mem_empty_merge(struct dsp_tlsf_mem *mem1,
	struct dsp_tlsf_mem *mem2,
	struct dsp_list_head *mem_list);

int dsp_tlsf_insert_block(struct dsp_tlsf_mem *mem, struct dsp_tlsf *tlsf);
int dsp_tlsf_find_block(size_t size, struct dsp_tlsf_mem **mem,
	struct dsp_tlsf *tlsf);
void dsp_tlsf_remove_block(struct dsp_tlsf_mem *mem, struct dsp_tlsf *tlsf);

int dsp_tlsf_init(struct dsp_tlsf *tlsf, unsigned long start_addr,
	size_t size, unsigned int align);
void dsp_tlsf_delete(struct dsp_tlsf *tlsf);

int dsp_tlsf_is_prev_empty(struct dsp_tlsf_mem *mem);
int dsp_tlsf_is_next_empty(struct dsp_tlsf_mem *mem);

int dsp_tlsf_malloc(size_t size, struct dsp_tlsf_mem **mem,
	struct dsp_tlsf *tlsf);
int dsp_tlsf_free(struct dsp_tlsf_mem *mem, struct dsp_tlsf *tlsf);

void dsp_tlsf_print(struct dsp_tlsf *tlsf);

int dsp_tlsf_can_be_loaded(struct dsp_tlsf *tlsf, size_t size);

#endif
