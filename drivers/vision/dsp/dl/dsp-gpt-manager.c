// SPDX-License-Identifier: GPL-2.0
/*
 * Samsung Exynos SoC series dsp driver
 *
 * Copyright (c) 2019 Samsung Electronics Co., Ltd.
 *              http://www.samsung.com/
 */

#include "dl/dsp-gpt-manager.h"
#include "dl/dsp-lib-manager.h"

#define DL_GPT_SIZE		(sizeof(unsigned int) * 3)
#define DL_BIT_PER_BITMAP	(sizeof(unsigned int) * 8)

static struct dsp_gpt_manager *gpt_manager;

int dsp_gpt_manager_init(unsigned long start_addr, size_t max_size)
{
	size_t max_bit_size = max_size / DL_GPT_SIZE;
	unsigned int idx;
	unsigned int bitmap_top_bit;

	gpt_manager = (struct dsp_gpt_manager *)dsp_dl_malloc(
			sizeof(struct dsp_gpt_manager),
			"GPT manager");

	gpt_manager->start_addr = start_addr;
	gpt_manager->max_size = max_size;

	if (max_bit_size == 0) {
		DL_ERROR("Size is too small\n");
		return -1;
	}

	gpt_manager->bitmap_size = (max_bit_size + DL_BIT_PER_BITMAP - 1) /
		DL_BIT_PER_BITMAP;
	gpt_manager->bitmap = (unsigned int *)dsp_dl_malloc(
			sizeof(*gpt_manager->bitmap) *
			gpt_manager->bitmap_size, "GPT bitmap");

	for (idx = 0; idx < gpt_manager->bitmap_size - 1; idx++)
		gpt_manager->bitmap[idx] = 0;

	bitmap_top_bit = max_bit_size - DL_BIT_PER_BITMAP *
		(gpt_manager->bitmap_size - 1);
	gpt_manager->bitmap[gpt_manager->bitmap_size - 1] =
		0xFFFFFFFF << bitmap_top_bit;
	return 0;
}

void dsp_gpt_manager_free(void)
{
	dsp_dl_free(gpt_manager->bitmap);
	dsp_dl_free(gpt_manager);
}

void dsp_gpt_manager_print(void)
{
	int idx, jdx;

	DL_INFO(DL_BORDER);
	DL_INFO("Global pointer table manager\n");
	DL_INFO("Start address: 0x%lx\n", gpt_manager->start_addr);
	DL_INFO("Table memory size: %zu\n", gpt_manager->max_size);
	DL_INFO("Table max: %zu\n", gpt_manager->max_size / DL_GPT_SIZE);
	DL_INFO("Table rows: %zu\n", gpt_manager->bitmap_size);
	DL_INFO("\n");
	DL_INFO("Bitmap table\n");

	for (idx = gpt_manager->bitmap_size - 1; idx >= 0; idx--) {
		DL_BUF_STR("[%d] ", idx);
		for (jdx = DL_BIT_PER_BITMAP - 1; jdx >= 0; jdx--) {
			unsigned int mask = 1 << jdx;

			DL_BUF_STR("%d ", (gpt_manager->bitmap[idx] & mask) >>
				jdx);
			if (jdx % 4 == 0 && jdx != 0)
				DL_BUF_STR(" ");
		}

		DL_BUF_STR("\n");
		DL_PRINT_BUF(INFO);
	}
}

int dsp_gpt_manager_alloc_libs(struct dsp_lib **libs, int libs_size,
	int *pm_inv)
{
	int idx;

	DL_DEBUG("Alloc GPT\n");

	for (idx = 0; idx < libs_size; idx++) {
		if (!libs[idx]->gpt) {
			DL_DEBUG("Alloc GPT for library %s\n",
				libs[idx]->name);
			libs[idx]->gpt = dsp_gpt_alloc(pm_inv);

			if (libs[idx]->gpt == NULL) {
				DL_ERROR("GPT manager allocation failed\n");
				return -1;
			}
		}
	}

	return 0;
}

struct dsp_gpt *dsp_gpt_alloc(int *pm_inv)
{
	unsigned int idx, jdx;
	struct dsp_gpt *ret;

	for (idx = 0; idx < gpt_manager->bitmap_size; idx++) {
		if (gpt_manager->bitmap[idx] != 0xFFFFFFFF) {
			for (jdx = 0; jdx < DL_BIT_PER_BITMAP; jdx++) {
				unsigned int mask = 1 << jdx;

				if (!(gpt_manager->bitmap[idx] & mask)) {
					ret = (struct dsp_gpt *)dsp_dl_malloc(
							sizeof(*ret),
							"GPT");
					ret->offset = idx * DL_BIT_PER_BITMAP +
						jdx;
					ret->addr = gpt_manager->start_addr +
						ret->offset * DL_GPT_SIZE;

					gpt_manager->bitmap[idx] |= mask;
					return ret;
				}
			}
		}
	}

	DL_DEBUG("GPT is full\n");

	if (dsp_lib_manager_delete_no_ref()) {
		DL_DEBUG("GPT realloc\n");
		*pm_inv = 1;
		return dsp_gpt_alloc(pm_inv);
	}

	return NULL;
}

void dsp_gpt_free(struct dsp_lib *lib)
{
	if (lib->gpt) {
		struct dsp_gpt *gpt = lib->gpt;

		gpt_manager->bitmap[gpt->offset / DL_BIT_PER_BITMAP] &=
			~(1 << (gpt->offset % DL_BIT_PER_BITMAP));
		dsp_dl_free(gpt);
		lib->gpt = NULL;
	}
}

void dsp_gpt_print(struct dsp_gpt *gpt)
{
	DL_INFO("Address: 0x%lx\n", gpt->addr);
	DL_INFO("Offset: %u\n", gpt->offset);
}
