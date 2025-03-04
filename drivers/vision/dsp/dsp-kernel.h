/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Samsung Exynos SoC series dsp driver
 *
 * Copyright (c) 2019 Samsung Electronics Co., Ltd.
 *              http://www.samsung.com/
 */

#ifndef __DSP_KERNEL_H__
#define __DSP_KERNEL_H__

#include <linux/mutex.h>

#include "dsp-util.h"
#include "dl/dsp-dl-engine.h"

#define DSP_KERNEL_MAX_COUNT		(256)

struct dsp_graph_manager;

struct dsp_kernel {
	int				id;
	unsigned int			name_length;
	char				*name;
	unsigned int			ref_count;
	void				*elf;
	size_t				elf_size;

	struct list_head		list;
	struct list_head		graph_list;
	struct dsp_kernel_manager	*owner;
};

struct dsp_kernel_manager {
	struct list_head		kernel_list;
	unsigned int			kernel_count;

	struct mutex			lock;
	unsigned int			dl_init;
	struct dsp_dl_param		dl_param;
	struct dsp_util_bitmap		kernel_map;
	struct dsp_graph_manager	*gmgr;
};

struct dsp_kernel *dsp_kernel_alloc(struct dsp_kernel_manager *kmgr,
		unsigned int name_length, struct dsp_dl_lib_info *dl_lib);
void dsp_kernel_free(struct dsp_kernel *kernel);

int dsp_kernel_load(struct dsp_kernel_manager *kmgr,
		struct dsp_dl_lib_info *dl_libs, unsigned int kernel_count);
int dsp_kernel_unload(struct dsp_kernel_manager *kmgr,
		struct dsp_dl_lib_info *dl_libs, unsigned int kernel_count);
void dsp_kernel_dump(struct dsp_kernel_manager *kmgr);

int dsp_kernel_manager_open(struct dsp_kernel_manager *kmgr);
void dsp_kernel_manager_close(struct dsp_kernel_manager *kmgr,
		unsigned int count);
int dsp_kernel_manager_probe(struct dsp_graph_manager *gmgr);
void dsp_kernel_manager_remove(struct dsp_kernel_manager *kmgr);

#endif
