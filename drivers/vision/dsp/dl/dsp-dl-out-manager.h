/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Samsung Exynos SoC series dsp driver
 *
 * Copyright (c) 2019 Samsung Electronics Co., Ltd.
 *              http://www.samsung.com/
 */

#ifndef __DL_DSP_DL_OUT_MANAGER_H__
#define __DL_DSP_DL_OUT_MANAGER_H__

#include "dl/dsp-tlsf-allocator.h"

struct dsp_lib;

#pragma pack(push, 4)
struct dsp_dl_out_section {
	unsigned int offset;
	unsigned int size;
};

struct dsp_dl_kernel_table {
	unsigned int pre;
	unsigned int exe;
	unsigned int post;
};

struct dsp_dl_out {
	unsigned int hash_next;
	unsigned int gpt_addr;
	struct dsp_dl_out_section kernel_table;
	struct dsp_dl_out_section DM_sh;
	struct dsp_dl_out_section DM_local;
	struct dsp_dl_out_section TCM_sh;
	struct dsp_dl_out_section TCM_local;
	struct dsp_dl_out_section sh_mem;
	char data[0];
};
#pragma pack(pop)

int dsp_dl_out_create(struct dsp_lib *lib);
size_t dsp_dl_out_get_size(struct dsp_dl_out *dl_out);
void dsp_dl_out_print(struct dsp_dl_out *dl_out);

unsigned int dsp_dl_hash_get_key(char *k);
void dsp_dl_hash_init(void);
void dsp_dl_hash_push(struct dsp_dl_out *dl_out);
void dsp_dl_hash_pop(char *k);
void dsp_dl_hash_print(void);


int dsp_dl_out_manager_init(unsigned long start_addr, size_t size);
int dsp_dl_out_manager_free(void);
void dsp_dl_out_manager_print(void);
int dsp_dl_out_manager_alloc_libs(struct dsp_lib **libs, int libs_size,
	int *pm_inv);

int dsp_dl_out_alloc(struct dsp_lib *lib, int *pm_inv);
void dsp_dl_out_free(struct dsp_lib *lib);

#endif
