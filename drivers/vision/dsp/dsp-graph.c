// SPDX-License-Identifier: GPL-2.0
/*
 * Samsung Exynos SoC series dsp driver
 *
 * Copyright (c) 2019 Samsung Electronics Co., Ltd.
 *              http://www.samsung.com/
 */

#include <linux/slab.h>

#include "dsp-log.h"
#include "dsp-device.h"
#include "hardware/dsp-memory.h"
#include "dsp-context.h"
#include "dsp-core.h"
#include "dsp-graph.h"

static void __dsp_graph_unmap_buffer(struct dsp_graph *graph,
		struct dsp_buffer *buf)
{
	struct dsp_memory *mem;

	dsp_enter();
	mem = &graph->owner->core->dspdev->system.memory;

	mem->ops->unmap_buffer(mem, buf);
	list_del(&buf->list);
	kfree(buf);
	dsp_leave();
}

static int __dsp_graph_map_buffer(struct dsp_graph *graph, void *ubuf)
{
	int ret;
	struct dsp_memory *mem;
	struct dsp_buffer *buf;
	struct dsp_common_param_v1 *ubuf1;
	struct dsp_common_param_v2 *ubuf2;
	struct dsp_common_param_v3 *ubuf3;
	int ubuf_param_mem_fd;
	int ubuf_param_mem_size;
	unsigned int *ubuf_param_mem_iova;
	unsigned char ubuf_param_mem_attr;
	unsigned int ubuf_param_type;
	unsigned int ubuf_param_offset;

	dsp_enter();
	mem = &graph->owner->core->dspdev->system.memory;

	if (graph->version == DSP_IOC_V1) {
		ubuf1 = ubuf;
		ubuf_param_mem_fd = ubuf1->param_mem.fd;
		ubuf_param_mem_size = ubuf1->param_mem.size;
		ubuf_param_mem_iova = &ubuf1->param_mem.iova;
		ubuf_param_mem_attr = ubuf1->param_mem.mem_attr;
		ubuf_param_type = ubuf1->param_type;
		ubuf_param_offset = 0;
	} else if (graph->version == DSP_IOC_V2) {
		ubuf2 = ubuf;
		ubuf_param_mem_fd = ubuf2->param_mem.fd;
		ubuf_param_mem_size = ubuf2->param_mem.size;
		ubuf_param_mem_iova = &ubuf2->param_mem.iova;
		ubuf_param_mem_attr = ubuf2->param_mem.mem_attr;
		ubuf_param_type = ubuf2->param_type;
		ubuf_param_offset = 0;
	} else if (graph->version == DSP_IOC_V3) {
		ubuf3 = ubuf;
		ubuf_param_mem_fd = ubuf3->param_mem.fd;
		ubuf_param_mem_size = ubuf3->param_mem.size;
		ubuf_param_mem_iova = &ubuf3->param_mem.iova;
		ubuf_param_mem_attr = ubuf3->param_mem.mem_attr;
		ubuf_param_type = ubuf3->param_type;
		ubuf_param_offset = ubuf3->param_mem.offset;
	} else {
		ret = -EINVAL;
		dsp_err("Failed to map buffer due to invalid version(%u)\n",
				graph->version);
		goto p_err;
	}
	dsp_dbg("ofi_mem (%d/%d/%u/%u)\n",
			ubuf_param_mem_fd, ubuf_param_mem_size,
			ubuf_param_mem_attr, ubuf_param_type);

	buf = kzalloc(sizeof(*buf), GFP_KERNEL);
	if (!buf) {
		ret = -ENOMEM;
		dsp_err("Failed to alloc buffer(%x)\n", graph->global_id);
		goto  p_err;
	}

	buf->fd = ubuf_param_mem_fd;
	buf->size = ubuf_param_mem_size;
	buf->offset = ubuf_param_offset;
	buf->dir = DMA_BIDIRECTIONAL;

	ret = mem->ops->map_buffer(mem, buf);
	if (ret) {
		kfree(buf);
		goto p_err;
	}

	*ubuf_param_mem_iova = buf->iova + buf->offset;

	if (ubuf_param_type == DSP_COMMON_PARAM_UPDATE) {
		graph->update_count++;
		list_add_tail(&buf->list, &graph->update_list);
	} else {
		graph->buf_count++;
		list_add_tail(&buf->list, &graph->buf_list);
	}

	dsp_leave();
	return 0;
p_err:
	return ret;
}

static void __dsp_graph_unmap_list(struct dsp_graph *graph,
		unsigned int param_type)
{
	struct dsp_buffer *buf, *temp;

	dsp_enter();
	if (param_type == DSP_COMMON_PARAM_UPDATE) {
		list_for_each_entry_safe(buf, temp, &graph->update_list, list) {
			__dsp_graph_unmap_buffer(graph, buf);
			graph->update_count--;
		}
	} else {
		list_for_each_entry_safe(buf, temp, &graph->buf_list, list) {
			__dsp_graph_unmap_buffer(graph, buf);
			graph->buf_count--;
		}
	}

	dsp_leave();
}

static int __dsp_graph_map_list(struct dsp_graph *graph,
		void *list, unsigned long long count, unsigned int param_type)
{
	int ret;
	int idx;
	struct dsp_common_param_v1 *list1;
	struct dsp_common_param_v2 *list2;
	struct dsp_common_param_v3 *list3;

	dsp_enter();

	if (graph->version == DSP_IOC_V1) {
		list1 = list;
		for (idx = 0; idx < count; ++idx) {
			if (list1[idx].param_type == DSP_COMMON_PARAM_EMPTY)
				continue;

			if (((param_type == DSP_COMMON_PARAM_UPDATE) &&
					(list1[idx].param_type !=
						DSP_COMMON_PARAM_UPDATE)) ||
				((param_type != DSP_COMMON_PARAM_UPDATE) &&
					(list1[idx].param_type ==
						DSP_COMMON_PARAM_UPDATE))) {
				ret = -EINVAL;
				dsp_err("param type is invalid(%u/%u)\n",
						list1[idx].param_type,
						param_type);
				goto p_err_map;
			}

			ret = __dsp_graph_map_buffer(graph, &list1[idx]);
			if (ret)
				goto p_err_map;
		}
	} else if (graph->version == DSP_IOC_V2) {
		list2 = list;
		for (idx = 0; idx < count; ++idx) {
			if (list2[idx].param_type == DSP_COMMON_PARAM_EMPTY)
				continue;

			if (((param_type == DSP_COMMON_PARAM_UPDATE) &&
					(list2[idx].param_type !=
						DSP_COMMON_PARAM_UPDATE)) ||
				((param_type != DSP_COMMON_PARAM_UPDATE) &&
					(list2[idx].param_type ==
						DSP_COMMON_PARAM_UPDATE))) {
				ret = -EINVAL;
				dsp_err("param type is invalid(%u/%u)\n",
						list2[idx].param_type,
						param_type);
				goto p_err_map;
			}

			ret = __dsp_graph_map_buffer(graph, &list2[idx]);
			if (ret)
				goto p_err_map;
		}
	} else if (graph->version == DSP_IOC_V3) {
		list3 = list;
		for (idx = 0; idx < count; ++idx) {
			if (list3[idx].param_type == DSP_COMMON_PARAM_EMPTY)
				continue;

			if (((param_type == DSP_COMMON_PARAM_UPDATE) &&
					(list3[idx].param_type !=
						DSP_COMMON_PARAM_UPDATE)) ||
				((param_type != DSP_COMMON_PARAM_UPDATE) &&
					(list3[idx].param_type ==
						DSP_COMMON_PARAM_UPDATE))) {
				ret = -EINVAL;
				dsp_err("param type is invalid(%u/%u)\n",
						list3[idx].param_type,
						param_type);
				goto p_err_map;
			}

			ret = __dsp_graph_map_buffer(graph, &list3[idx]);
			if (ret)
				goto p_err_map;
		}
	} else {
		ret = -EINVAL;
		dsp_err("Failed to map list due to invalid version(%u)\n",
				graph->version);
		goto p_err_version;
	}
	dsp_leave();
	return 0;
p_err_map:
	__dsp_graph_unmap_list(graph, param_type);
p_err_version:
	return ret;
}

static int __dsp_graph_send_task(struct dsp_graph *graph,
		unsigned int message_id, struct dsp_mailbox_pool *pool,
		bool wait, bool recovery, bool force)
{
	int ret;
	struct dsp_system *sys;
	struct dsp_task *task;

	dsp_enter();
	sys = &graph->owner->core->dspdev->system;

	task = dsp_task_create(&sys->task_manager, force);
	if (IS_ERR(task)) {
		ret = PTR_ERR(task);
		goto p_err_task;
	}

	task->message_id = message_id;
	task->message_version = graph->version;
	task->pool = pool;
	task->wait = wait;
	task->recovery = recovery;

	ret = sys->ops->execute_task(sys, task);
	if (ret)
		goto p_err_execute;

	dsp_task_destroy(task);
	dsp_leave();
	return 0;
p_err_execute:
	dsp_task_destroy(task);
p_err_task:
	return ret;
}

static void __dsp_graph_remove_kernel(struct dsp_graph *graph)
{
	struct dsp_kernel_manager *kmgr;
	struct dsp_kernel *kernel, *t;

	dsp_enter();
	kmgr = &graph->owner->kernel_manager;

	if (graph->kernel_loaded) {
		dsp_kernel_unload(kmgr, graph->dl_libs, graph->kernel_count);
		kfree(graph->dl_libs);
	} else {
		kfree(graph->dl_libs);
	}

	list_for_each_entry_safe(kernel, t, &graph->kernel_list, graph_list) {
		list_del(&kernel->graph_list);
		dsp_kernel_free(kernel);
	}
	dsp_leave();
}

static int __dsp_graph_check_kernel(struct dsp_graph *graph, char *str,
		unsigned int length)
{
	int ret, idx;

	dsp_enter();
	str[length - 1] = '\0';

	for (idx = 0; idx < length; ++idx) {
		if (str[idx] == '/') {
			ret = -EINVAL;
			dsp_err("Path in file name isn't supported(%s)\n", str);
			goto p_err;
		}
	}

	dsp_leave();
	return 0;
p_err:
	return ret;
}

static int __dsp_graph_add_kernel(struct dsp_graph *graph, void *kernel_name)
{
	int ret;
	struct dsp_kernel_manager *kmgr;
	unsigned int kernel_count;
	unsigned int *length;
	unsigned long offset;
	int idx;
	struct dsp_kernel *kernel;

	dsp_enter();
	kmgr = &graph->owner->kernel_manager;
	kernel_count = graph->kernel_count;
	length = kernel_name;
	offset = (unsigned long)&length[kernel_count];

	if (kernel_count > DSP_MAX_KERNEL_COUNT) {
		ret = -EINVAL;
		dsp_err("kernel_count(%u/%u) is invalid\n",
				kernel_count, DSP_MAX_KERNEL_COUNT);
		goto p_err;
	}

	graph->dl_libs = kcalloc(kernel_count, sizeof(*graph->dl_libs),
			GFP_KERNEL);
	if (!graph->dl_libs) {
		ret = -ENOMEM;
		dsp_err("Failed to alloc dl_libs(%u)\n", kernel_count);
		goto p_err;
	}

	for (idx = 0; idx < kernel_count; ++idx) {
		ret = __dsp_graph_check_kernel(graph, (char *)offset,
				length[idx]);
		if (ret) {
			dsp_err("Failed to check kernel(%u/%u)\n",
					idx, kernel_count);
			goto p_err_alloc;
		}

		graph->dl_libs[idx].name = (const char *)offset;

		kernel = dsp_kernel_alloc(kmgr, length[idx],
				&graph->dl_libs[idx]);
		if (IS_ERR(kernel)) {
			ret = PTR_ERR(kernel);
			dsp_err("Failed to alloc kernel(%u/%u)\n",
					idx, kernel_count);
			goto p_err_alloc;
		}

		list_add_tail(&kernel->graph_list, &graph->kernel_list);
		offset += length[idx];
	}

	ret = dsp_kernel_load(kmgr, graph->dl_libs, kernel_count);
	if (ret)
		goto p_err_load;

	graph->kernel_loaded = true;
	dsp_leave();
	return 0;
p_err_load:
p_err_alloc:
	__dsp_graph_remove_kernel(graph);
p_err:
	return ret;
}

static void __dsp_graph_unload(struct dsp_graph_manager *gmgr,
		struct dsp_graph *graph)
{
	dsp_enter();
	gmgr->count--;
	list_del(&graph->list);
	__dsp_graph_remove_kernel(graph);
	__dsp_graph_unmap_list(graph, DSP_COMMON_PARAM_UPDATE);
	__dsp_graph_unmap_list(graph, DSP_COMMON_PARAM_TSGD);

	kfree(graph->kernel_name);
	kfree(graph);
	dsp_leave();
}

static void __dsp_graph_recovery(struct dsp_graph *graph)
{
	int ret;

	dsp_enter();
	ret = __dsp_graph_send_task(graph, DSP_COMMON_LOAD_GRAPH, graph->pool,
			true, false, true);
	if (ret) {
		dsp_mailbox_free_pool(graph->pool);
		__dsp_graph_unload(graph->owner, graph);
	}
	dsp_leave();
}

void dsp_graph_manager_recovery(struct dsp_graph_manager *gmgr)
{
	struct dsp_graph *graph, *temp;

	dsp_enter();
	mutex_lock(&gmgr->lock);

	list_for_each_entry_safe(graph, temp, &gmgr->list, list) {
		if (graph->loaded)
			__dsp_graph_recovery(graph);
	}
	mutex_unlock(&gmgr->lock);
	dsp_leave();
}

struct dsp_graph *dsp_graph_get(struct dsp_graph_manager *gmgr,
		unsigned int global_id)
{
	struct dsp_graph *graph, *temp;

	dsp_enter();
	mutex_lock(&gmgr->lock);

	list_for_each_entry_safe(graph, temp, &gmgr->list, list) {
		if (GET_COMMON_GRAPH_ID(graph->global_id) ==
				GET_COMMON_GRAPH_ID(global_id)) {
			mutex_unlock(&gmgr->lock);
			dsp_leave();
			return graph;
		}
	}

	mutex_unlock(&gmgr->lock);
	return NULL;
}

struct dsp_graph *dsp_graph_load(struct dsp_graph_manager *gmgr,
		struct dsp_mailbox_pool *pool, void *kernel_name,
		unsigned int kernel_count, unsigned int version)
{
	int ret;
	struct dsp_graph *graph, *temp;
	struct dsp_common_graph_info_v1 *ginfo1;
	struct dsp_common_graph_info_v2 *ginfo2;
	struct dsp_common_graph_info_v3 *ginfo3;
	unsigned int ginfo_global_id;
	void *ginfo_param_list;
	unsigned int ginfo_n_tsgd;
	unsigned int ginfo_n_param;
	unsigned int ginfo_n_kernel;

	dsp_enter();
	if (version == DSP_IOC_V1) {
		ginfo1 = pool->kva;
		ginfo_global_id  = ginfo1->global_id;
		ginfo_param_list = ginfo1->param_list;
		ginfo_n_tsgd     = ginfo1->n_tsgd;
		ginfo_n_param    = ginfo1->n_param;
		ginfo_n_kernel   = ginfo1->n_kernel;
	} else if (version == DSP_IOC_V2) {
		ginfo2 = pool->kva;
		ginfo_global_id  = ginfo2->global_id;
		ginfo_param_list = ginfo2->param_list;
		ginfo_n_tsgd     = ginfo2->n_tsgd;
		ginfo_n_param    = ginfo2->n_param;
		ginfo_n_kernel   = ginfo2->n_kernel;
	} else if (version == DSP_IOC_V3) {
		ginfo3 = pool->kva;
		ginfo_global_id  = ginfo3->global_id;
		ginfo_param_list = ginfo3->param_list;
		ginfo_n_tsgd     = ginfo3->n_tsgd;
		ginfo_n_param    = ginfo3->n_param;
		ginfo_n_kernel   = ginfo3->n_kernel;
	} else {
		ret = -EINVAL;
		dsp_err("Failed to load graph due to invalid version(%u)\n",
			version);
		goto p_err_version;
	}

	if (ginfo_n_kernel != kernel_count) {
		ret = -EINVAL;
		dsp_err("kernel_cnt is different from value of ginfo(%u/%u)\n",
				kernel_count, ginfo_n_kernel);
		goto p_err_count;
	}

	mutex_lock(&gmgr->lock);
	list_for_each_entry_safe(graph, temp, &gmgr->list, list) {
		if (GET_COMMON_GRAPH_ID(graph->global_id) ==
				GET_COMMON_GRAPH_ID(ginfo_global_id) &&
				GET_COMMON_CONTEXT_ID(graph->global_id) ==
				GET_COMMON_CONTEXT_ID(ginfo_global_id)) {
			ret = -EINVAL;
			dsp_warn("graph has already loaded(%x)\n",
					ginfo_global_id);
			goto p_err_graph;
		}
	}

	graph = kzalloc(sizeof(*graph), GFP_KERNEL);
	if (!graph) {
		ret = -ENOMEM;
		dsp_err("Failed to alloc graph(%x)\n", ginfo_global_id);
		goto p_err_graph;
	}

	graph->owner = gmgr;
	graph->global_id = ginfo_global_id;
	INIT_LIST_HEAD(&graph->buf_list);
	INIT_LIST_HEAD(&graph->update_list);
	INIT_LIST_HEAD(&graph->kernel_list);
	graph->pool = pool;
	graph->version = version;

	ret = __dsp_graph_map_list(graph, ginfo_param_list,
			(unsigned long long)ginfo_n_tsgd +
			(unsigned long long)ginfo_n_param, DSP_COMMON_PARAM_TSGD);
	if (ret)
		goto p_err_map;

	graph->graph_count = ginfo_n_tsgd;
	graph->param_count = ginfo_n_param;
	graph->kernel_count = ginfo_n_kernel;

	ret = __dsp_graph_add_kernel(graph, kernel_name);
	if (ret)
		goto p_err_kernel;

	list_add_tail(&graph->list, &gmgr->list);
	gmgr->count++;

	mutex_unlock(&gmgr->lock);

	ret = __dsp_graph_send_task(graph, DSP_COMMON_LOAD_GRAPH, pool,
			true, true, false);
	if (ret) {
		mutex_lock(&gmgr->lock);
		__dsp_graph_unload(gmgr, graph);
		goto p_err_graph;
	}

	graph->kernel_name = kernel_name;
	graph->loaded = true;
	dsp_leave();
	return graph;
p_err_kernel:
	__dsp_graph_unmap_list(graph, DSP_COMMON_PARAM_TSGD);
p_err_map:
	kfree(graph);
p_err_graph:
	mutex_unlock(&gmgr->lock);
p_err_count:
p_err_version:
	return ERR_PTR(ret);
}

void dsp_graph_unload(struct dsp_graph *graph, struct dsp_mailbox_pool *pool)
{
	struct dsp_graph_manager *gmgr;

	dsp_enter();
	gmgr = graph->owner;

	/* free pool remained for re-load */
	dsp_mailbox_free_pool(graph->pool);
	graph->loaded = false;

	__dsp_graph_send_task(graph, DSP_COMMON_UNLOAD_GRAPH, pool,
			true, true, false);

	mutex_lock(&gmgr->lock);
	__dsp_graph_unload(gmgr, graph);
	mutex_unlock(&gmgr->lock);
	dsp_leave();
}

int dsp_graph_execute(struct dsp_graph *graph, struct dsp_mailbox_pool *pool)
{
	int ret;
	struct dsp_graph_manager *gmgr;
	struct dsp_common_execute_info_v1 *einfo1;
	struct dsp_common_execute_info_v2 *einfo2;
	struct dsp_common_execute_info_v3 *einfo3;
	unsigned int einfo_n_update_param;
	void *einfo_param_list;

	dsp_enter();
	gmgr = graph->owner;

	if (graph->version == DSP_IOC_V1) {
		einfo1 = pool->kva;
		einfo_n_update_param = einfo1->n_update_param;
		einfo_param_list = einfo1->param_list;
	} else if (graph->version == DSP_IOC_V2) {
		einfo2 = pool->kva;
		einfo_n_update_param = einfo2->n_update_param;
		einfo_param_list = einfo2->param_list;
	} else if (graph->version == DSP_IOC_V3) {
		einfo3 = pool->kva;
		einfo_n_update_param = einfo3->n_update_param;
		einfo_param_list = einfo3->param_list;
	} else {
		ret = -EINVAL;
		dsp_err("Failed to execute graph due to invalid version(%u)\n",
			graph->version);
		goto p_err;
	}

	mutex_lock(&gmgr->lock);

	if (einfo_n_update_param) {
		ret = __dsp_graph_map_list(graph, einfo_param_list,
				(unsigned long long)einfo_n_update_param,
				DSP_COMMON_PARAM_UPDATE);
		if (ret) {
			mutex_unlock(&gmgr->lock);
			goto p_err;
		}
	}

	mutex_unlock(&gmgr->lock);

	ret = __dsp_graph_send_task(graph, DSP_COMMON_EXECUTE_MSG, pool,
			true, true, false);

	mutex_lock(&gmgr->lock);
	__dsp_graph_unmap_list(graph, DSP_COMMON_PARAM_UPDATE);
	mutex_unlock(&gmgr->lock);

	if (ret)
		goto p_err;

	dsp_leave();
	return 0;
p_err:
	return ret;
}

void dsp_graph_manager_stop(struct dsp_graph_manager *gmgr, unsigned int id)
{
	struct dsp_graph *graph, *temp;

	dsp_enter();
	mutex_lock(&gmgr->lock);

	list_for_each_entry_safe(graph, temp, &gmgr->list, list) {
		if (GET_COMMON_CONTEXT_ID(graph->global_id) == id) {
			dsp_warn("unreleased graph remains(%x/%u)\n",
					graph->global_id, gmgr->count);
			__dsp_graph_unload(gmgr, graph);
		}
	}

	mutex_unlock(&gmgr->lock);
	dsp_leave();
}

int dsp_graph_manager_open(struct dsp_graph_manager *gmgr)
{
	int ret;

	dsp_enter();
	ret = dsp_kernel_manager_open(&gmgr->kernel_manager);
	if (ret)
		goto p_err;

	dsp_leave();
	return 0;
p_err:
	return ret;
}

void dsp_graph_manager_close(struct dsp_graph_manager *gmgr, unsigned int count)
{
	dsp_enter();
	dsp_kernel_manager_close(&gmgr->kernel_manager, count);
	dsp_leave();
}

int dsp_graph_manager_probe(struct dsp_core *core)
{
	int ret;
	struct dsp_graph_manager *gmgr;

	dsp_enter();
	gmgr = &core->graph_manager;
	gmgr->core = core;

	ret = dsp_kernel_manager_probe(gmgr);
	if (ret)
		goto p_err_kernel;

	INIT_LIST_HEAD(&gmgr->list);
	mutex_init(&gmgr->lock);
	mutex_init(&gmgr->lock_for_unload);
	dsp_leave();
	return 0;
p_err_kernel:
	return ret;
}

void dsp_graph_manager_remove(struct dsp_graph_manager *gmgr)
{
	struct dsp_graph *graph, *temp;

	dsp_enter();
	mutex_lock(&gmgr->lock);

	list_for_each_entry_safe(graph, temp, &gmgr->list, list) {
		dsp_warn("unreleased graph remains(%x/%u)\n",
				graph->global_id, gmgr->count);
		__dsp_graph_unload(gmgr, graph);
	}

	mutex_unlock(&gmgr->lock);

	dsp_kernel_manager_remove(&gmgr->kernel_manager);
	dsp_leave();
}
