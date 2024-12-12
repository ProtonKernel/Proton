/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Samsung Exynos SoC series dsp driver
 *
 * Copyright (c) 2019 Samsung Electronics Co., Ltd.
 *              http://www.samsung.com/
 */

#ifndef __DL_DSP_PM_MANAGER_H__
#define __DL_DSP_PM_MANAGER_H__

#include "dl/dsp-common.h"
#include "dl/dsp-tlsf-allocator.h"
#include "dl/dsp-lib-manager.h"

unsigned long dsp_pm_manager_get_pm_start_addr(void);
size_t dsp_pm_manager_get_pm_total_size(void);
int dsp_pm_manager_init(unsigned long start_addr, size_t size,
	unsigned int pm_offset);
void dsp_pm_manager_free(void);
void dsp_pm_manager_print(void);

int dsp_pm_manager_alloc_libs(struct dsp_lib **libs, int libs_size,
	int *pm_inv);

int dsp_pm_alloc(size_t size, struct dsp_lib *lib, int *pm_inv);
void dsp_pm_free(struct dsp_lib *lib);

void dsp_pm_print(struct dsp_lib *lib);

#endif
