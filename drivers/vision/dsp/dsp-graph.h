/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Samsung Exynos SoC series dsp driver
 *
 * Copyright (c) 2019 Samsung Electronics Co., Ltd.
 *              http://www.samsung.com/
 */

#ifndef __DSP_GRAPH_H__
#define __DSP_GRAPH_H__

#include <linux/mutex.h>

#include "dsp-common-type.h"
#include "dsp-kernel.h"
#include "hardware/dsp-mailbox.h"
#include "dl/dsp-common.h"

struct dsp_core;

struct dsp_graph {
	unsigned int			global_id;
	unsigned int			graph_count;
	unsigned int			param_count;
	unsigned int			kernel_count;

	unsigned int			buf_count;
	struct list_head		buf_list;
	unsigned int			update_count;
	struct list_head		update_list;
	struct list_head		kernel_list;
	void				*kernel_name;
	struct dsp_dl_lib_info		*dl_libs;
	bool				kernel_loaded;
	bool				loaded;
	unsigned int			version;
	struct dsp_mailbox_pool		*pool;

	struct dsp_graph_manager	*owner;
	struct list_head		list;
};

struct dsp_graph_manager {
	struct list_head		list;
	unsigned int			count;
	struct mutex			lock;
	struct mutex			lock_for_unload;

	struct dsp_kernel_manager	kernel_manager;
	struct dsp_core			*core;
};

void dsp_graph_manager_recovery(struct dsp_graph_manager *gmgr);
struct dsp_graph *dsp_graph_get(struct dsp_graph_manager *gmgr,
		unsigned int global_id);

struct dsp_graph *dsp_graph_load(struct dsp_graph_manager *gmgr,
		struct dsp_mailbox_pool *pool, void *kernel_name,
		unsigned int kernel_count, unsigned int version);
void dsp_graph_unload(struct dsp_graph *graph, struct dsp_mailbox_pool *pool);
int dsp_graph_execute(struct dsp_graph *graph, struct dsp_mailbox_pool *pool);

void dsp_graph_manager_stop(struct dsp_graph_manager *gmgr, unsigned int id);
int dsp_graph_manager_open(struct dsp_graph_manager *gmgr);
void dsp_graph_manager_close(struct dsp_graph_manager *gmgr,
		unsigned int count);

int dsp_graph_manager_probe(struct dsp_core *core);
void dsp_graph_manager_remove(struct dsp_graph_manager *gmgr);

#endif
