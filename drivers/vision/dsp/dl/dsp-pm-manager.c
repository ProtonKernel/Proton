// SPDX-License-Identifier: GPL-2.0
/*
 * Samsung Exynos SoC series dsp driver
 *
 * Copyright (c) 2019 Samsung Electronics Co., Ltd.
 *              http://www.samsung.com/
 */

#include "dl/dsp-pm-manager.h"
#include "dl/dsp-tlsf-allocator.h"
#include "dl/dsp-lib-manager.h"

#define DL_PM_ALIGN	(64)

static struct dsp_tlsf *pm_manager;
static unsigned long dsp_pm_start_addr;
static size_t dsp_pm_total_size;
static struct dsp_lib *pm_init_lib;

unsigned long dsp_pm_manager_get_pm_start_addr(void)
{
	return dsp_pm_start_addr;
}

size_t dsp_pm_manager_get_pm_total_size(void)
{
	return dsp_pm_total_size;
}

int dsp_pm_manager_init(unsigned long start_addr, size_t size,
	unsigned int pm_offset)
{
	int ret;
	struct dsp_tlsf_mem *init_mem;

	DL_DEBUG("PM manager init\n");

	pm_manager = (struct dsp_tlsf *)dsp_dl_malloc(
			sizeof(struct dsp_tlsf),
			"PM manager");
	pm_init_lib = (struct dsp_lib *)dsp_dl_malloc(
			sizeof(struct dsp_lib),
			"PM Boot lib");

	dsp_pm_start_addr = start_addr;
	dsp_pm_total_size = size;

	ret = dsp_tlsf_init(pm_manager, start_addr, size, DL_PM_ALIGN);
	if (ret == -1) {
		DL_ERROR("TLSF init is failed\n");
		dsp_dl_free(pm_manager);
		dsp_dl_free(pm_init_lib);
		return -1;
	}

	DL_DEBUG("PM init mem size : %u\n", pm_offset);
	ret = dsp_tlsf_malloc(pm_offset, &init_mem, pm_manager);
	if (ret == -1) {
		DL_ERROR("[%s] CHK_ERR\n", __func__);
		dsp_dl_free(pm_manager);
		dsp_dl_free(pm_init_lib);
		return -1;
	}

	pm_init_lib->name = (char *)dsp_dl_malloc(
			strlen("Boot/main pm") + 1,
			"Boot/main pm");
	strcpy(pm_init_lib->name, "Boot/main pm");

	pm_init_lib->ref_cnt = 1;
	pm_init_lib->loaded = 1;
	init_mem->lib = pm_init_lib;
	return 0;
}

void dsp_pm_manager_free(void)
{
	dsp_tlsf_delete(pm_manager);
	dsp_dl_free(pm_manager);
	dsp_dl_free(pm_init_lib->name);
	dsp_dl_free(pm_init_lib);
}

void dsp_pm_manager_print(void)
{
	DL_INFO(DL_BORDER);
	DL_INFO("Program memory manager\n");
	DL_INFO("Start address: %#lx, size %#zx\n",
			dsp_pm_start_addr, dsp_pm_total_size);
	DL_INFO("\n");
	dsp_tlsf_print(pm_manager);
}

int dsp_pm_manager_alloc_libs(struct dsp_lib **libs, int libs_size,
	int *pm_inv)
{
	int ret, idx;

	DL_DEBUG("Alloc PM\n");

	*pm_inv = 0;
	for (idx = 0; idx < libs_size; idx++) {
		if (!libs[idx]->pm) {
			size_t text_size;

			DL_DEBUG("Alloc PM for library %s\n",
				libs[idx]->name);
			text_size = dsp_elf32_get_text_size(libs[idx]->elf);
			if (text_size == UINT_MAX) {
				DL_ERROR("getting text size failed\n");
				return -1;
			}
			DL_DEBUG("PM alloc libs_size : %zu\n", text_size);

			ret = dsp_pm_alloc(text_size, libs[idx], pm_inv);

			if (ret == -1) {
				DL_ERROR("PM manager allocation failed\n");
				return -1;
			}
		}
	}

	return 0;
}

int dsp_pm_alloc(size_t size, struct dsp_lib *lib,
	int *pm_inv)
{
	int alloc_ret;

	if (!lib) {
		DL_ERROR("struct dsp_lib is null\n");
		return -1;
	}

	DL_DEBUG("Allocate PM pm for lib %s(size : %zu)\n",
		lib->name, size);
	alloc_ret = dsp_tlsf_malloc(size, &lib->pm, pm_manager);

	if (alloc_ret == -1) {
		if (dsp_tlsf_can_be_loaded(pm_manager, size)) {
			DL_INFO("PM I-cache invalidation set\n");
			dsp_lib_manager_delete_no_ref();
			*pm_inv = 1;
			return dsp_pm_alloc(size, lib, pm_inv);
		}

		DL_ERROR("Can not be loaded\n");
		return -1;
	}

	DL_DEBUG("Lib(%s) allocation success\n", lib->name);
	lib->pm->lib = lib;
	return 0;
}

void dsp_pm_free(struct dsp_lib *lib)
{
	if (lib->pm) {
		//DL_DEBUG("PM free for %s\n", lib->info->name);
		dsp_tlsf_free(lib->pm, pm_manager);
		lib->pm = NULL;
	}
}

void dsp_pm_print(struct dsp_lib *lib)
{
	unsigned int *text = (unsigned int *)lib->pm->start_addr;
	unsigned long offset = ((unsigned long)text - dsp_pm_start_addr) / 4;
	unsigned int idx;

	for (idx = 0; idx < lib->pm->size / sizeof(unsigned int);
		idx++, offset++) {
		if (idx % 4 == 0) {
			if (idx != 0) {
				DL_BUF_STR("\n");
				DL_PRINT_BUF(DEBUG);
			}
			DL_BUF_STR("0x%lx : ", offset);
		}

		DL_BUF_STR("0x");
		DL_BUF_STR("%08x ", *(unsigned int *)(text + idx));
	}

	DL_BUF_STR("\n");
	DL_PRINT_BUF(DEBUG);
}
