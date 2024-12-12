/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Samsung Exynos SoC series dsp driver
 *
 * Copyright (c) 2019 Samsung Electronics Co., Ltd.
 *              http://www.samsung.com/
 */

#ifndef __DL_DSP_GPT_MANAGER_H__
#define __DL_DSP_GPT_MANAGER_H__

#include "dl/dsp-common.h"
#include "dl/dsp-lib-manager.h"

struct dsp_lib;

struct dsp_gpt {
	unsigned long addr;
	unsigned int offset;
};

struct dsp_gpt_manager {
	unsigned int *bitmap;
	unsigned long start_addr;
	size_t max_size;
	size_t bitmap_size;
};

int dsp_gpt_manager_init(unsigned long start_addr, size_t max_size);
void dsp_gpt_manager_free(void);
void dsp_gpt_manager_print(void);
int dsp_gpt_manager_alloc_libs(struct dsp_lib **libs, int libs_size,
	int *pm_inv);

struct dsp_gpt *dsp_gpt_alloc(int *pm_inv);
void dsp_gpt_free(struct dsp_lib *lib);

void dsp_gpt_print(struct dsp_gpt *gpt);

#endif
