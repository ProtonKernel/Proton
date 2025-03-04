/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Samsung Exynos SoC series dsp driver
 *
 * Copyright (c) 2019 Samsung Electronics Co., Ltd.
 *              http://www.samsung.com/
 */

#ifndef __DL_DSP_LIB_MANAGER_H__
#define __DL_DSP_LIB_MANAGER_H__

#include "dl/dsp-common.h"
#include "dl/dsp-elf-loader.h"
#include "dl/dsp-tlsf-allocator.h"
#include "dl/dsp-gpt-manager.h"
#include "dl/dsp-dl-out-manager.h"
#include "dl/dsp-dl-linker.h"

struct dsp_lib {
	char *name;
	struct dsp_elf32 *elf;

	struct dsp_tlsf_mem *pm;
	struct dsp_gpt *gpt;

	struct dsp_dl_out *dl_out;
	size_t dl_out_data_size;
	struct dsp_tlsf_mem *dl_out_mem;

	struct dsp_link_info *link_info;

	unsigned int ref_cnt;
	int loaded;
};

void dsp_lib_init(struct dsp_lib *lib, struct dsp_dl_lib_info *info);
void dsp_lib_unload(struct dsp_lib *lib);
void dsp_lib_free(struct dsp_lib *lib);
void dsp_lib_print(struct dsp_lib *lib);

void dsp_lib_manager_init(const char *lib_path);
void dsp_lib_manager_free(void);
void dsp_lib_manager_print(void);

struct dsp_lib **dsp_lib_manager_get_libs(struct dsp_dl_lib_info *infos,
	size_t size);

void dsp_lib_manager_delete_unloaded_libs(struct dsp_lib **libs, size_t size);
void dsp_lib_manager_delete_lib(struct dsp_lib *lib);
int dsp_lib_manager_delete_no_ref(void);

void dsp_lib_manager_inc_ref_cnt(struct dsp_lib **libs, size_t size);
void dsp_lib_manager_dec_ref_cnt(struct dsp_lib **libs, size_t size);

int dsp_lib_manager_load_libs(struct dsp_lib **libs, size_t size);
void dsp_lib_manager_unload_libs(struct dsp_lib **libs, size_t size);

#endif
