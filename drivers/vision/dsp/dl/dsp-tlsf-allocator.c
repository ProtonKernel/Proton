// SPDX-License-Identifier: GPL-2.0
/*
 * Samsung Exynos SoC series dsp driver
 *
 * Copyright (c) 2019 Samsung Electronics Co., Ltd.
 *              http://www.samsung.com/
 */

#include "dl/dsp-tlsf-allocator.h"
#include "dl/dsp-lib-manager.h"

static void __set_fl(struct dsp_tlsf *tlsf, unsigned char fl)
{
	tlsf->fl |= 1 << fl;
}

static void __unset_fl(struct dsp_tlsf *tlsf, unsigned char fl)
{
	tlsf->fl &= ~(1 << fl);
}

static void __set_sl(struct dsp_tlsf *tlsf, unsigned char fl, unsigned char sl)
{
	tlsf->sl[fl] |= 1 << sl;
}

static void __unset_sl(struct dsp_tlsf *tlsf, unsigned char fl,
	unsigned char sl)
{
	tlsf->sl[fl] &= ~(1 << sl);
}

static unsigned char __get_fl_size(struct dsp_tlsf *tlsf)
{
	return tlsf->max_sh - TLSF_MIN_BLOCK_SHIFT + 1;
}

static struct dsp_list_head *__get_head_from_idx(struct dsp_tlsf *tlsf,
	struct dsp_tlsf_idx *idx)
{
	DL_DEBUG("Idx (%d, %d)\n", idx->fl, idx->sl);
	return &tlsf->fb[idx->fl][idx->sl];
}

static unsigned int __floor_log2(size_t size)
{
	size_t ret = 0, pow2;

	if (size < 1)
		return 0;

	for (pow2 = 1; pow2 <= size; ret++)
		pow2 <<= 1;

	return (unsigned int)(ret - 1);
}

static unsigned int __find_last_set(size_t size)
{
	return __floor_log2(size);
}

static unsigned char __get_max_sh(size_t size)
{
	unsigned char sh;

	for (sh = 1; sh < (1 << 5) - 1; sh++) {
		unsigned int tmp_size = (1 << (sh + 1)) - 4;

		if (tmp_size >= size)
			return sh;
	}

	return 0;
}

static int __align_mem_size(struct dsp_tlsf *tlsf, size_t size)
{
	if (size < TLSF_MIN_BLOCK_SIZE)
		size = TLSF_MIN_BLOCK_SIZE;

	size += tlsf->align - 1;
	return size & ~(tlsf->align - 1);
}

static int __get_tlsf_insert_index(size_t size, struct dsp_tlsf_idx *idx)
{
	idx->fl = __find_last_set(size);
	if (idx->fl < TLSF_MIN_BLOCK_SHIFT) {
		DL_ERROR("fl(%u)/size(%zu) is invalid\n", idx->fl, size);
		return -1;
	}
	idx->sl = (size >> (idx->fl - TLSF_SL_SHIFT)) - TLSF_SL_SIZE;
	idx->fl -= TLSF_MIN_BLOCK_SHIFT;
	DL_DEBUG("TLSF insert idx : (%u, %u)\n",
		(unsigned int)idx->fl, (unsigned int)idx->sl);
	return 0;
}

static int __get_tlsf_search_index(size_t size, struct dsp_tlsf_idx *idx)
{
	unsigned int fl;

	DL_DEBUG("TLSF search\n");
	fl = __find_last_set(size);
	if (fl < TLSF_MIN_BLOCK_SHIFT) {
		DL_ERROR("fl(%u)/size(%zu) is invalid\n", fl, size);
		return -1;
	}
	size = size + (1 << (fl - TLSF_SL_SHIFT)) - 1;
	return __get_tlsf_insert_index(size, idx);
}

static int __find_bit_upper(unsigned int bitmap, size_t bitmap_size,
	unsigned int index)
{
	for (; index < bitmap_size; index++) {
		unsigned int mask = 1 << index;

		if (bitmap & mask)
			return index;
	}

	DL_DEBUG("No bit in bitmap\n");
	return -1;
}

const char *__dsp_tlsf_mem_type_to_str(enum dsp_tlsf_mem_type type)
{
	switch (type) {
	case MEM_EMPTY:
		return "MEM_EMPTY";
	case MEM_USE:
		return "MEM_USE";
	}

	return NULL;
}

void dsp_tlsf_mem_init(struct dsp_tlsf_mem *mem)
{
	mem->lib = NULL;
	dsp_list_node_init(&mem->mem_list_node);
	dsp_list_node_init(&mem->tlsf_node);
}

void dsp_tlsf_mem_print(struct dsp_tlsf_mem *mem)
{
	DL_BUF_STR("[0x%lx] %s size(%zu) idx(fl:%u, sl:%u)", mem->start_addr,
		__dsp_tlsf_mem_type_to_str(mem->type), mem->size,
		mem->tlsf_idx.fl, mem->tlsf_idx.sl);

	if (mem->lib)
		DL_BUF_STR(" lib(%s) ref(%u) %s", mem->lib->name,
			mem->lib->ref_cnt,
			(mem->lib->loaded) ? "loaded" : "unloaded");

	DL_BUF_STR("\n");
	DL_PRINT_BUF(INFO);
}

struct dsp_tlsf_mem *dsp_tlsf_mem_empty_merge(struct dsp_tlsf_mem *mem1,
	struct dsp_tlsf_mem *mem2, struct dsp_list_head *mem_list)
{
	if (mem1->type != MEM_EMPTY || mem2->type != MEM_EMPTY) {
		DL_DEBUG("TLSF mem is not empty\n");
		return NULL;
	}

	dsp_list_node_remove(mem_list, &mem2->mem_list_node);
	mem1->size = mem1->size + mem2->size;
	dsp_dl_free(mem2);
	return mem1;
}

int dsp_tlsf_insert_block(struct dsp_tlsf_mem *mem, struct dsp_tlsf *tlsf)
{
	int ret;
	struct dsp_list_head *head;

	DL_DEBUG("TLSF insert block\n");

	if (mem->size & (tlsf->align - 1)) {
		DL_ERROR("size(0x%zx) align error\n", mem->size);
		return -1;
	}

	ret = __get_tlsf_insert_index(mem->size, &mem->tlsf_idx);
	if (ret == -1) {
		DL_ERROR("[%s] CHK_ERR\n", __func__);
		return -1;
	}

	head = __get_head_from_idx(tlsf, &mem->tlsf_idx);

	dsp_list_node_init(&mem->tlsf_node);
	dsp_list_node_push_back(head, &mem->tlsf_node);

	__set_fl(tlsf, mem->tlsf_idx.fl);
	__set_sl(tlsf, mem->tlsf_idx.fl, mem->tlsf_idx.sl);
	return 0;
}

int dsp_tlsf_find_block(size_t size, struct dsp_tlsf_mem **mem,
	struct dsp_tlsf *tlsf)
{
	int ret;
	struct dsp_tlsf_idx tlsf_idx;
	struct dsp_list_head *head;

	if (size < TLSF_MIN_BLOCK_SIZE || size > tlsf->max_size) {
		DL_ERROR("size(%zu) is invalid\n", size);
		return -1;
	}

	if (size & (tlsf->align - 1)) {
		DL_ERROR("size(0x%zx) align error\n", size);
		return -1;
	}

	ret = __get_tlsf_search_index(size, &tlsf_idx);
	if (ret == -1) {
		DL_ERROR("[%s] CHK_ERR\n", __func__);
		return -1;
	}

	tlsf_idx.sl = __find_bit_upper(tlsf->sl[tlsf_idx.fl],
			TLSF_SL_SIZE, tlsf_idx.sl);
	if (tlsf_idx.sl == -1) {
		tlsf_idx.fl = __find_bit_upper(tlsf->fl, __get_fl_size(tlsf),
				tlsf_idx.fl + 1);
		if (tlsf_idx.fl == -1) {
			DL_DEBUG("Cannot find block due to fl\n");
			return -1;
		}

		tlsf_idx.sl = __find_bit_upper(tlsf->sl[tlsf_idx.fl],
				TLSF_SL_SIZE, 0);
		if (tlsf_idx.sl == -1) {
			DL_DEBUG("Cannot find block due to sl\n");
			return -1;
		}
	}

	DL_DEBUG("Find block fl(%d), sl(%d)\n", tlsf_idx.fl, tlsf_idx.sl);

	head = __get_head_from_idx(tlsf, &tlsf_idx);
	if (head->num <= 0) {
		DL_ERROR("Block head is empty\n");
		return -1;
	}

	*mem = container_of(head->next, struct dsp_tlsf_mem, tlsf_node);
	return 0;
}

void dsp_tlsf_remove_block(struct dsp_tlsf_mem *mem, struct dsp_tlsf *tlsf)
{
	struct dsp_list_head *tlsf_head;

	DL_DEBUG("TLSF remove idx : (%u, %u)\n",
		mem->tlsf_idx.fl, mem->tlsf_idx.sl);
	tlsf_head = __get_head_from_idx(tlsf, &mem->tlsf_idx);
	dsp_list_node_remove(tlsf_head, &mem->tlsf_node);

	if (dsp_list_is_empty(tlsf_head)) {
		__unset_sl(tlsf, mem->tlsf_idx.fl, mem->tlsf_idx.sl);

		if (tlsf->sl[mem->tlsf_idx.fl] == 0)
			__unset_fl(tlsf, mem->tlsf_idx.fl);
	}
}

int dsp_tlsf_init(struct dsp_tlsf *tlsf, unsigned long start_addr,
	size_t size, unsigned int align)
{
	int idx, jdx;
	struct dsp_tlsf_mem *init_mem;

	size = size & ~(align - 1);
	DL_DEBUG("Tlsf max size aligned : %zu\n", size);

	tlsf->align = align;
	tlsf->max_size = size;
	tlsf->max_sh = __get_max_sh(size);

	DL_DEBUG("max_sh : %d\n", tlsf->max_sh);

	tlsf->fl = 0;
	DL_DEBUG("fl size : %u\n", (unsigned int)__get_fl_size(tlsf));

	tlsf->sl = (unsigned char *)dsp_dl_malloc(
			sizeof(*tlsf->sl) * __get_fl_size(tlsf),
			"TLSF sl");

	for (idx = 0; idx < __get_fl_size(tlsf); idx++)
		tlsf->sl[idx] = 0;

	DL_DEBUG("sl alloced\n");

	tlsf->fb = (struct dsp_list_head (*)[8])dsp_dl_malloc(
			sizeof(*tlsf->fb) * __get_fl_size(tlsf),
			"TLSF fb");

	for (idx = 0; idx < __get_fl_size(tlsf); idx++)
		for (jdx = 0; jdx < TLSF_SL_SIZE; jdx++)
			dsp_list_head_init(&tlsf->fb[idx][jdx]);

	DL_DEBUG("fb (%d, %d) alloced\n", __get_fl_size(tlsf), TLSF_SL_SIZE);

	dsp_list_head_init(&tlsf->mem_list);
	init_mem = (struct dsp_tlsf_mem *)dsp_dl_malloc(
			sizeof(*init_mem), "Init mem");
	dsp_tlsf_mem_init(init_mem);
	init_mem->type = MEM_EMPTY;
	init_mem->start_addr = start_addr;
	init_mem->size = size;

	DL_DEBUG("init mem size : %zu\n", init_mem->size);

	dsp_list_node_push_back(&tlsf->mem_list, &init_mem->mem_list_node);
	dsp_tlsf_insert_block(init_mem, tlsf);

	return 0;
}

void dsp_tlsf_delete(struct dsp_tlsf *tlsf)
{
	dsp_list_free(&tlsf->mem_list, struct dsp_tlsf_mem, mem_list_node);
	dsp_dl_free(tlsf->sl);
	dsp_dl_free(tlsf->fb);
}

int dsp_tlsf_is_prev_empty(struct dsp_tlsf_mem *mem)
{
	struct dsp_list_node *mem_node = &mem->mem_list_node;
	struct dsp_tlsf_mem *prev_mem;

	if (mem_node->prev == NULL)
		return 0;

	prev_mem = container_of(mem_node->prev, struct dsp_tlsf_mem,
			mem_list_node);
	if (prev_mem->type != MEM_EMPTY)
		return 0;
	else
		return 1;
}

int dsp_tlsf_is_next_empty(struct dsp_tlsf_mem *mem)
{
	struct dsp_list_node *mem_node = &mem->mem_list_node;
	struct dsp_tlsf_mem *next_mem;

	if (mem_node->next == NULL)
		return 0;

	next_mem = container_of(mem_node->next, struct dsp_tlsf_mem,
			mem_list_node);
	if (next_mem->type != MEM_EMPTY)
		return 0;
	else
		return 1;
}

int dsp_tlsf_malloc(size_t size, struct dsp_tlsf_mem **mem,
	struct dsp_tlsf *tlsf)
{
	int ret;
	struct dsp_tlsf_mem *new_mem;

	size = __align_mem_size(tlsf, size);
	ret = dsp_tlsf_find_block(size, mem, tlsf);
	if (ret == -1) {
		DL_DEBUG("[%s] CHK_ERR\n", __func__);
		return -1;
	}

	(*mem)->type = MEM_USE;
	dsp_tlsf_remove_block(*mem, tlsf);

	if ((*mem)->size < size) {
		DL_ERROR("Overflow will happen.\n");
		return -1;
	}

	if ((*mem)->size - size >= TLSF_MIN_BLOCK_SIZE) {
		new_mem = (struct dsp_tlsf_mem *)dsp_dl_malloc(
				sizeof(*new_mem), "TLSF New mem");
		dsp_tlsf_mem_init(new_mem);
		new_mem->type = MEM_EMPTY;

		if (size > (unsigned int)(*mem)->start_addr + size) {
			DL_ERROR("Overflow happened.\n");
			return -1;
		}

		new_mem->start_addr = (*mem)->start_addr + size;
		new_mem->size = (*mem)->size - size;

		dsp_list_node_insert_back(&tlsf->mem_list,
			&(*mem)->mem_list_node,
			&new_mem->mem_list_node);
		dsp_tlsf_insert_block(new_mem, tlsf);

		(*mem)->size = size;
	}

	DL_DEBUG("TLSF malloc end\n");
	return 0;
}

int dsp_tlsf_free(struct dsp_tlsf_mem *mem, struct dsp_tlsf *tlsf)
{
	int ret;
	struct dsp_tlsf_mem *merge;

	if (mem->type == MEM_EMPTY) {
		DL_ERROR("struct dsp_tlsf_mem is already empty\n");
		return -1;
	}

	mem->type = MEM_EMPTY;
	mem->lib = NULL;
	DL_DEBUG("0x%lx free\n", mem->start_addr);

	merge = mem;

	if (dsp_tlsf_is_prev_empty(merge)) {
		struct dsp_list_node *node = &merge->mem_list_node;
		struct dsp_tlsf_mem *prev_mem = container_of(node->prev,
				struct dsp_tlsf_mem, mem_list_node);

		DL_DEBUG("Merge prev\n");
		DL_DEBUG("prev_mem : 0x%lx\n", prev_mem->start_addr);
		dsp_tlsf_remove_block(prev_mem, tlsf);

		merge = dsp_tlsf_mem_empty_merge(prev_mem, merge,
				&tlsf->mem_list);

		if (!merge) {
			DL_ERROR("struct dsp_tlsf_mem Merge failed\n");
			return -1;
		}
	}

	if (dsp_tlsf_is_next_empty(merge)) {
		struct dsp_list_node *node = &merge->mem_list_node;
		struct dsp_tlsf_mem *next_mem = container_of(node->next,
				struct dsp_tlsf_mem, mem_list_node);

		DL_DEBUG("Merge next\n");
		DL_DEBUG("next_mem : 0x%lx\n", next_mem->start_addr);
		dsp_tlsf_remove_block(next_mem, tlsf);

		merge = dsp_tlsf_mem_empty_merge(merge, next_mem,
				&tlsf->mem_list);

		if (!merge) {
			DL_ERROR("struct dsp_tlsf_mem Merge failed\n");
			return -1;
		}
	}

	ret = dsp_tlsf_insert_block(merge, tlsf);
	if (ret == -1) {
		DL_ERROR("[%s] CHK_ERR\n", __func__);
		return -1;
	}

	return 0;
}

void dsp_tlsf_print(struct dsp_tlsf *tlsf)
{
	int idx, jdx;
	struct dsp_list_node *node;

	DL_INFO("TLSF table\n");
	DL_BUF_STR("First level: ");

	for (idx = __get_fl_size(tlsf) - 1; idx >= 0; idx--) {
		DL_BUF_STR("%d ", (tlsf->fl & (1 << idx)) >> idx);
		if (idx % 4 == 0 && idx != 0)
			DL_BUF_STR(" ");
	}

	DL_BUF_STR("\n");
	DL_PRINT_BUF(INFO);

	DL_INFO("Second level\n");

	for (idx = __get_fl_size(tlsf) - 1; idx >= 0; idx--) {
		DL_BUF_STR("[%d] ", idx);

		for (jdx = TLSF_SL_SIZE - 1; jdx >= 0; jdx--) {
			DL_BUF_STR("%d ", (tlsf->sl[idx] & (1 << jdx)) >> jdx);
			if (jdx % 4 == 0 && jdx != 0)
				DL_BUF_STR(" ");
		}

		DL_BUF_STR("\n");
		DL_PRINT_BUF(INFO);
	}

	DL_INFO("\n");

	DL_INFO("Memory remained\n");
	for (idx = 0; idx < __get_fl_size(tlsf); idx++) {
		for (jdx = 0; jdx < TLSF_SL_SIZE; jdx++) {
			if (tlsf->sl[idx] & (1 << jdx)) {
				struct dsp_tlsf_idx tlsf_idx;
				struct dsp_list_head *head;

				tlsf_idx.fl = idx;
				tlsf_idx.sl = jdx;
				head = __get_head_from_idx(tlsf, &tlsf_idx);
				dsp_list_for_each(node, head) {
					struct dsp_tlsf_mem *mem;

					mem = container_of(node,
							struct dsp_tlsf_mem,
							tlsf_node);
					dsp_tlsf_mem_print(mem);
				}
			}
		}
	}

	DL_INFO("\n");

	DL_INFO("Memory list\n");
	dsp_list_for_each(node, &tlsf->mem_list) {
		struct dsp_tlsf_mem *mem =
			container_of(node, struct dsp_tlsf_mem, mem_list_node);

		dsp_tlsf_mem_print(mem);
	}
}

int dsp_tlsf_can_be_loaded(struct dsp_tlsf *tlsf, size_t size)
{
	struct dsp_list_node *node;
	unsigned int max_alloc = 0;
	unsigned int local_alloc = 0;
	int accum_flag = 0;

	dsp_list_for_each(node, &tlsf->mem_list) {
		struct dsp_tlsf_mem *cur_mem =
			container_of(node, struct dsp_tlsf_mem, mem_list_node);

		if (accum_flag) {
			if (cur_mem->type == MEM_USE &&
				cur_mem->lib->ref_cnt > 0) {
				accum_flag = 0;
				DL_DEBUG("Local alloc : %u\n", local_alloc);

				if (local_alloc > max_alloc)
					max_alloc = local_alloc;

				local_alloc = 0;
			} else
				local_alloc += cur_mem->size;
		} else {
			if (cur_mem->type != MEM_USE ||
				cur_mem->lib->ref_cnt == 0) {
				accum_flag = 1;
				local_alloc = cur_mem->size;
			}
		}
	}

	DL_DEBUG("Local alloc : %u\n", local_alloc);

	if (accum_flag) {
		if (local_alloc > max_alloc)
			max_alloc = local_alloc;
	}

	DL_DEBUG("Max alloc : %u, size : %zu\n", max_alloc, size);

	if (max_alloc >= size)
		return 1;

	return 0;
}
