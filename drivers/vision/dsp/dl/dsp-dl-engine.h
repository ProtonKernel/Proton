/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Samsung Exynos SoC series dsp driver
 *
 * Copyright (c) 2019 Samsung Electronics Co., Ltd.
 *              http://www.samsung.com/
 */

#ifndef __DL_DSP_ENGINE_H__
#define __DL_DSP_ENGINE_H__

#include "dl/dsp-common.h"

enum dsp_dl_status {
	DSP_DL_FAIL = -1,
	DSP_DL_SUCCESS = 0,
};

struct dsp_dl_load_status {
	enum dsp_dl_status status;
	int pm_inv;
};

struct dsp_dl_param_mem {
	unsigned long addr;
	size_t size;
};

struct dsp_dl_param {
	const char *lib_path;

	struct dsp_dl_lib_file gkt;
	struct dsp_dl_lib_file rule;

	struct dsp_dl_lib_info *common_libs;
	int common_size;

	struct dsp_dl_param_mem pm;
	unsigned int pm_offset;

	struct dsp_dl_param_mem gpt;
	struct dsp_dl_param_mem dl_out;
};

enum dsp_dl_status dsp_dl_init(struct dsp_dl_param *param);
enum dsp_dl_status dsp_dl_close(void);

struct dsp_dl_load_status dsp_dl_load_libraries(
	struct dsp_dl_lib_info *infos, int size);

enum dsp_dl_status dsp_dl_unload_libraries(
	struct dsp_dl_lib_info *infos, int size);

void dsp_dl_print_status(void);

#endif
