// SPDX-License-Identifier: GPL-2.0
/*
 * Samsung Exynos SoC series dsp driver
 *
 * Copyright (c) 2019 Samsung Electronics Co., Ltd.
 *              http://www.samsung.com/
 */

#include <linux/slab.h>
#include <linux/uaccess.h>

#include "dsp-log.h"
#include "dsp-util.h"
#include "dsp-device.h"
#include "hardware/dsp-mailbox.h"
#include "dsp-common-type.h"
#include "dsp-control.h"
#include "dsp-context.h"

static int dsp_context_boot(struct dsp_context *dctx, struct dsp_ioc_boot *args)
{
	int ret;
	struct dsp_core *core;

	dsp_enter();
	dsp_dbg("boot start\n");
	core = dctx->core;

	ret = dsp_graph_manager_open(&core->graph_manager);
	if (ret)
		goto p_err_graph;

	mutex_lock(&dctx->lock);
	if (dctx->boot_count + 1 < dctx->boot_count) {
		ret = -EINVAL;
		dsp_err("boot count is overflowed\n");
		goto p_err_count;
	}

	ret = dsp_device_start(core->dspdev, args->pm_level);
	if (ret)
		goto p_err_device;

	dctx->boot_count++;
	mutex_unlock(&dctx->lock);

	dsp_dbg("boot end\n");
	dsp_leave();
	return 0;
p_err_device:
p_err_count:
	mutex_unlock(&dctx->lock);
	dsp_graph_manager_close(&core->graph_manager, 1);
p_err_graph:
	return ret;
}

static int dsp_context_boot_secure(struct dsp_context *dctx,
		struct dsp_ioc_boot_secure *args)
{
	int ret;
	struct dsp_device *dspdev;
	struct dsp_memory *mem;

	dsp_enter();
	dsp_dbg("boot secure start\n");
	dspdev = dctx->core->dspdev;
	mem = &dspdev->system.memory;

	mutex_lock(&dctx->lock);
	if (dctx->sec_boot_count + 1 < dctx->sec_boot_count) {
		ret = -EINVAL;
		dsp_err("sec_boot count is overflowed\n");
		goto p_err_count;
	}

	ret = dsp_device_start_secure(dspdev, args->pm_level, args->mem_size);
	if (ret)
		goto p_err_device;

	if (args->mem_size) {
		if (args->mem_size != mem->sec_mem.size)
			dsp_warn("secure memory size is invalid(%u/%zu)\n",
					args->mem_size, mem->sec_mem.size);
		else
			args->mem_addr = mem->sec_mem.iova;
	}

	dctx->sec_boot_count++;
	mutex_unlock(&dctx->lock);

	dsp_dbg("boot secure end\n");
	dsp_leave();
	return 0;
p_err_device:
p_err_count:
	mutex_unlock(&dctx->lock);
	return ret;
}

static int __dsp_context_get_graph_info(struct dsp_context *dctx,
		struct dsp_ioc_load_graph *load, void *ginfo, void *kernel_name)
{
	int ret;

	dsp_enter();
	if (copy_from_user(ginfo, (void __user *)load->param_addr,
				load->param_size)) {
		ret = -EFAULT;
		dsp_err("Failed to copy from user param_addr(%d)\n", ret);
		goto p_err;
	}

	if (copy_from_user(kernel_name, (void __user *)load->kernel_addr,
				load->kernel_size)) {
		ret = -EFAULT;
		dsp_err("Failed to copy from user kernel_addr(%d)\n", ret);
		goto p_err;
	}

	dsp_leave();
	return 0;
p_err:
	return ret;
}

static void __dsp_context_put_graph_info(struct dsp_context *dctx, void *ginfo)
{
	dsp_enter();
	dsp_leave();
}

static int __dsp_context_check_graph_info(struct dsp_context *dctx,
		struct dsp_ioc_load_graph *load, unsigned long long ginfo_size,
		unsigned long long param_list_size, void *kernel_name)
{
	int ret;
	unsigned int *check;
	unsigned int idx, kernel_size;

	dsp_enter();
	ginfo_size += param_list_size;
	if (ginfo_size < param_list_size) {
		ret = -EINVAL;
		dsp_err("ginfo_size(%llu/%llu) is overflowed\n",
				ginfo_size, param_list_size);
		goto p_err;
	}

	if (ginfo_size != load->param_size) {
		ret = -EINVAL;
		dsp_err("param_size(%u/%llu) is invalid\n",
				load->param_size, ginfo_size);
		goto p_err;
	}

	if (load->kernel_count > DSP_MAX_KERNEL_COUNT) {
		ret = -EINVAL;
		dsp_err("kernel_count(%u/%u) is invalid\n",
				load->kernel_count, DSP_MAX_KERNEL_COUNT);
		goto p_err;
	}

	kernel_size = load->kernel_count * sizeof(int);
	if (kernel_size < load->kernel_count) {
		ret = -EINVAL;
		dsp_err("kernel_size(%u/%u) is overflowed\n",
				kernel_size, load->kernel_count);
		goto p_err;
	}

	if (kernel_size > load->kernel_size) {
		ret = -EINVAL;
		dsp_err("kernel_count(%u/%u) is invalid\n",
				load->kernel_count, load->kernel_size);
		goto p_err;
	}

	check = kernel_name;
	for (idx = 0; idx < load->kernel_count; ++idx) {
		if (check[idx] == 0) {
			ret = -EINVAL;
			dsp_err("kernel_size(%u/%u/%u/%u) is wrong\n",
					kernel_size, check[idx],
					idx, load->kernel_count);
			goto p_err;
		}
		kernel_size += check[idx];
		if (kernel_size < check[idx]) {
			ret = -EINVAL;
			dsp_err("kernel_size(%u/%u/%u/%u) is overflowed\n",
					kernel_size, check[idx],
					idx, load->kernel_count);
			goto p_err;
		}
	}

	if (kernel_size != load->kernel_size) {
		ret = -EINVAL;
		dsp_err("kernel_size(%u/%u/%u) is invalid\n",
				load->kernel_count, load->kernel_size,
				kernel_size);
		goto p_err;
	}

	dsp_leave();
	return 0;
p_err:
	return ret;
}

static int dsp_context_load_graph(struct dsp_context *dctx,
		struct dsp_ioc_load_graph *args)
{
	int ret;
	unsigned int booted;
	struct dsp_system *sys;
	void *kernel_name;
	struct dsp_mailbox_pool *pool;
	struct dsp_common_graph_info_v1 *ginfo1;
	struct dsp_common_graph_info_v2 *ginfo2;
	struct dsp_common_graph_info_v3 *ginfo3;
	struct dsp_graph *graph;
	unsigned int version;
	unsigned long long ginfo_size, param_list_size;

	dsp_enter();
	dsp_dbg("load start\n");
	mutex_lock(&dctx->lock);
	booted = dctx->boot_count;
	mutex_unlock(&dctx->lock);
	if (!booted) {
		ret = -EINVAL;
		dsp_err("device is not normal booted\n");
		goto p_err;
	}

	sys = &dctx->core->dspdev->system;
	version = args->version;

	if (!args->kernel_size || args->kernel_size > DSP_MAX_KERNEL_SIZE) {
		ret = -EINVAL;
		dsp_err("size for kernel_name is invalid(%u/%zu)\n",
				args->kernel_size, DSP_MAX_KERNEL_SIZE);
		goto p_err;
	}

	kernel_name = kmalloc(args->kernel_size, GFP_KERNEL);
	if (!kernel_name) {
		ret = -ENOMEM;
		dsp_err("Failed to allocate kernel_name(%u)\n",
				args->kernel_size);
		goto p_err_name;
	}

	pool = dsp_mailbox_alloc_pool(&sys->mailbox, args->param_size);
	if (IS_ERR(pool)) {
		ret = PTR_ERR(pool);
		goto p_err_pool;
	}
	pool->pm_qos = args->request_qos;

	if (version == DSP_IOC_V1) {
		ginfo1 = pool->kva;
		ret = __dsp_context_get_graph_info(dctx, args, ginfo1,
				kernel_name);
		if (ret)
			goto p_err_info;

		ginfo_size = sizeof(*ginfo1);
		if (args->param_size < ginfo_size) {
			ret = -EINVAL;
			dsp_err("param size is invalid(%u/%llu)\n",
					args->param_size, ginfo_size);
			goto p_err_info;
		}

		param_list_size = (unsigned long long)ginfo1->n_tsgd +
			(unsigned long long)ginfo1->n_param;
		param_list_size *= sizeof(ginfo1->param_list[0]);

		ret = __dsp_context_check_graph_info(dctx, args, ginfo_size,
				param_list_size, kernel_name);
		if (ret)
			goto p_err_info;

		SET_COMMON_CONTEXT_ID(&ginfo1->global_id, dctx->id);
	} else if (version == DSP_IOC_V2) {
		ginfo2 = pool->kva;
		ret = __dsp_context_get_graph_info(dctx, args, ginfo2,
				kernel_name);
		if (ret)
			goto p_err_info;

		ginfo_size = sizeof(*ginfo2);
		if (args->param_size < ginfo_size) {
			ret = -EINVAL;
			dsp_err("param size is invalid(%u/%llu)\n",
					args->param_size, ginfo_size);
			goto p_err_info;
		}

		param_list_size = (unsigned long long)ginfo2->n_tsgd +
			(unsigned long long)ginfo2->n_param;
		param_list_size *= sizeof(ginfo2->param_list[0]);

		ret = __dsp_context_check_graph_info(dctx, args, ginfo_size,
				param_list_size, kernel_name);
		if (ret)
			goto p_err_info;

		SET_COMMON_CONTEXT_ID(&ginfo2->global_id, dctx->id);
	} else if (version == DSP_IOC_V3) {
		ginfo3 = pool->kva;
		ret = __dsp_context_get_graph_info(dctx, args, ginfo3,
				kernel_name);
		if (ret)
			goto p_err_info;

		ginfo_size = sizeof(*ginfo3);
		if (args->param_size < ginfo_size) {
			ret = -EINVAL;
			dsp_err("param size is invalid(%u/%llu)\n",
					args->param_size, ginfo_size);
			goto p_err_info;
		}

		param_list_size = (unsigned long long)ginfo3->n_tsgd +
			(unsigned long long)ginfo3->n_param;
		param_list_size *= sizeof(ginfo3->param_list[0]);

		ret = __dsp_context_check_graph_info(dctx, args, ginfo_size,
				param_list_size, kernel_name);
		if (ret)
			goto p_err_info;

		SET_COMMON_CONTEXT_ID(&ginfo3->global_id, dctx->id);
	} else {
		ret = -EINVAL;
		dsp_err("Failed to load graph due to invalid version(%u)\n",
				version);
		goto p_err_info;
	}

	graph = dsp_graph_load(&dctx->core->graph_manager, pool,
			kernel_name, args->kernel_count, version);
	if (IS_ERR(graph)) {
		ret = PTR_ERR(graph);
		goto p_err_graph;
	}

	args->timestamp[0] = pool->time.start;
	args->timestamp[1] = pool->time.end;

	if (version == DSP_IOC_V1)
		__dsp_context_put_graph_info(dctx, ginfo1);
	else if (version == DSP_IOC_V2)
		__dsp_context_put_graph_info(dctx, ginfo2);
	else if (version == DSP_IOC_V3)
		__dsp_context_put_graph_info(dctx, ginfo3);

	dsp_dbg("load end\n");
	dsp_leave();
	return 0;
p_err_graph:
	if (version == DSP_IOC_V1)
		__dsp_context_put_graph_info(dctx, ginfo1);
	else if (version == DSP_IOC_V2)
		__dsp_context_put_graph_info(dctx, ginfo2);
	else if (version == DSP_IOC_V3)
		__dsp_context_put_graph_info(dctx, ginfo3);
p_err_info:
	dsp_mailbox_free_pool(pool);
p_err_pool:
	kfree(kernel_name);
p_err_name:
p_err:
	return ret;
}

static int dsp_context_unload_graph(struct dsp_context *dctx,
		struct dsp_ioc_unload_graph *args)
{
	int ret;
	unsigned int booted;
	struct dsp_system *sys;
	struct dsp_graph *graph;
	struct dsp_mailbox_pool *pool;
	unsigned int *global_id;
	struct dsp_graph_manager *gmgr;

	dsp_enter();
	dsp_dbg("unload start\n");
	gmgr = &dctx->core->graph_manager;
	mutex_lock(&dctx->lock);
	booted = dctx->boot_count;
	mutex_unlock(&dctx->lock);
	if (!booted) {
		ret = -EINVAL;
		dsp_err("device is not normal booted\n");
		goto p_err;
	}

	sys = &dctx->core->dspdev->system;

	mutex_lock(&gmgr->lock_for_unload);
	SET_COMMON_CONTEXT_ID(&args->global_id, dctx->id);
	graph = dsp_graph_get(&dctx->core->graph_manager, args->global_id);
	if (!graph) {
		ret = -EINVAL;
		dsp_err("graph is not loaded(%x)\n", args->global_id);
		mutex_unlock(&gmgr->lock_for_unload);
		goto p_err;
	}

	pool = dsp_mailbox_alloc_pool(&sys->mailbox, sizeof(args->global_id));
	if (IS_ERR(pool)) {
		ret = PTR_ERR(pool);
		mutex_unlock(&gmgr->lock_for_unload);
		goto p_err;
	}
	pool->pm_qos = args->request_qos;
	global_id = pool->kva;
	*global_id = args->global_id;

	dsp_graph_unload(graph, pool);

	args->timestamp[0] = pool->time.start;
	args->timestamp[1] = pool->time.end;
	mutex_unlock(&gmgr->lock_for_unload);

	dsp_mailbox_free_pool(pool);

	dsp_dbg("unload end\n");
	dsp_leave();
	return 0;
p_err:
	return ret;
}

static int __dsp_context_get_execute_info(struct dsp_context *dctx,
		struct dsp_ioc_execute_msg *execute, void *einfo)
{
	int ret;
	unsigned int size;

	dsp_enter();
	size = execute->size;

	if (!size) {
		ret = -EINVAL;
		dsp_err("size for execute_msg is invalid(%u)\n", size);
		goto p_err;
	}

	if (copy_from_user(einfo, (void __user *)execute->addr, size)) {
		ret = -EFAULT;
		dsp_err("Failed to copy from user addr(%d)\n", ret);
		goto p_err_copy;
	}

	dsp_leave();
	return 0;
p_err_copy:
p_err:
	return ret;
}

static void __dsp_context_put_execute_info(struct dsp_context *dctx, void *info)
{
	dsp_enter();
	dsp_leave();
}

static int dsp_context_execute_msg(struct dsp_context *dctx,
		struct dsp_ioc_execute_msg *args)
{
	int ret;
	unsigned int booted;
	struct dsp_system *sys;
	struct dsp_mailbox_pool *pool;
	struct dsp_common_execute_info_v1 *einfo1;
	struct dsp_common_execute_info_v2 *einfo2;
	struct dsp_common_execute_info_v3 *einfo3;
	struct dsp_graph *graph;
	unsigned int version;
	unsigned int einfo_global_id;

	dsp_enter();
	dsp_dbg("execute start\n");
	mutex_lock(&dctx->lock);
	booted = dctx->boot_count;
	mutex_unlock(&dctx->lock);
	if (!booted) {
		ret = -EINVAL;
		dsp_err("device is not normal booted\n");
		goto p_err;
	}

	sys = &dctx->core->dspdev->system;
	version = args->version;

	pool = dsp_mailbox_alloc_pool(&sys->mailbox, args->size);
	if (IS_ERR(pool)) {
		ret = PTR_ERR(pool);
		goto p_err_pool;
	}
	pool->pm_qos = args->request_qos;

	if (version == DSP_IOC_V1) {
		einfo1 = pool->kva;
		ret = __dsp_context_get_execute_info(dctx, args, einfo1);
		if (ret)
			goto p_err_info;
		SET_COMMON_CONTEXT_ID(&einfo1->global_id, dctx->id);
		einfo_global_id = einfo1->global_id;
	} else if (version == DSP_IOC_V2) {
		einfo2 = pool->kva;
		ret = __dsp_context_get_execute_info(dctx, args, einfo2);
		if (ret)
			goto p_err_info;
		SET_COMMON_CONTEXT_ID(&einfo2->global_id, dctx->id);
		einfo_global_id = einfo2->global_id;
	} else if (version == DSP_IOC_V3) {
		einfo3 = pool->kva;
		ret = __dsp_context_get_execute_info(dctx, args, einfo3);
		if (ret)
			goto p_err_info;
		SET_COMMON_CONTEXT_ID(&einfo3->global_id, dctx->id);
		einfo_global_id = einfo3->global_id;
	} else {
		ret = -EINVAL;
		dsp_err("Failed to execute msg due to invalid version(%u)\n",
				version);
		goto p_err_info;
	}

	graph = dsp_graph_get(&dctx->core->graph_manager, einfo_global_id);
	if (!graph) {
		ret = -EINVAL;
		dsp_err("graph is not loaded(%x)\n", einfo_global_id);
		goto p_err_graph_get;
	}

	if (graph->version != version) {
		ret = -EINVAL;
		dsp_err("graph has different message_version(%u/%u)\n",
				graph->version, version);
		goto p_err_graph_version;
	}

	ret = dsp_graph_execute(graph, pool);
	if (ret)
		goto p_err_graph_execute;

	args->timestamp[0] = pool->time.start;
	args->timestamp[1] = pool->time.end;

	if (version == DSP_IOC_V1)
		__dsp_context_put_execute_info(dctx, einfo1);
	else if (version == DSP_IOC_V2)
		__dsp_context_put_execute_info(dctx, einfo2);
	else if (version == DSP_IOC_V3)
		__dsp_context_put_execute_info(dctx, einfo3);

	dsp_mailbox_free_pool(pool);

	dsp_dbg("execute end\n");
	dsp_leave();
	return 0;
p_err_graph_execute:
p_err_graph_version:
p_err_graph_get:
	if (version == DSP_IOC_V1)
		__dsp_context_put_execute_info(dctx, einfo1);
	else if (version == DSP_IOC_V2)
		__dsp_context_put_execute_info(dctx, einfo2);
	else if (version == DSP_IOC_V3)
		__dsp_context_put_execute_info(dctx, einfo3);
p_err_info:
	dsp_mailbox_free_pool(pool);
p_err_pool:
p_err:
	return ret;
}

static int __dsp_context_get_control(struct dsp_context *dctx,
		struct dsp_ioc_control *control, union dsp_control *cmd)
{
	int ret;

	dsp_enter();
	dsp_dbg("control(%u/%u/%#lx)\n",
			control->control_id, control->size, control->addr);

	switch (control->control_id) {
	case DSP_CONTROL_ENABLE_DVFS:
	case DSP_CONTROL_DISABLE_DVFS:
		if (control->size != sizeof(cmd->dvfs.pm_qos)) {
			ret = -EINVAL;
			dsp_err("user cmd size is invalid(%u/%zu)\n",
					control->size,
					sizeof(cmd->dvfs.pm_qos));
			goto p_err;
		}

		if (copy_from_user(&cmd->dvfs.pm_qos,
					(void __user *)control->addr,
					control->size)) {
			ret = -EFAULT;
			dsp_err("Failed to copy from user cmd(%u/%d)\n",
					control->control_id, ret);
			goto p_err;
		}

		break;
	case DSP_CONTROL_ENABLE_BOOST:
	case DSP_CONTROL_DISABLE_BOOST:
		break;
	case DSP_CONTROL_REQUEST_MO:
	case DSP_CONTROL_RELEASE_MO:
		if (!control->size || control->size >=
				sizeof(cmd->mo.scenario_name)) {
			ret = -EINVAL;
			dsp_err("user cmd size is invalid(%u/%u)\n",
					control->size, SCENARIO_NAME_MAX);
			goto p_err;
		}

		if (copy_from_user(cmd->mo.scenario_name,
					(void __user *)control->addr,
					control->size)) {
			ret = -EFAULT;
			dsp_err("Failed to copy from user cmd(%u/%d)\n",
					control->control_id, ret);
			goto p_err;
		}

		cmd->mo.scenario_name[control->size] = '\0';
		break;
	case DSP_CONTROL_REQUEST_PRESET:
		if (control->size != sizeof(cmd->preset)) {
			ret = -EINVAL;
			dsp_err("user cmd size is invalid(%u/%zu)\n",
					control->size, sizeof(cmd->preset));
			goto p_err;
		}

		if (copy_from_user(&cmd->preset, (void __user *)control->addr,
					control->size)) {
			ret = -EFAULT;
			dsp_err("Failed to copy from user cmd(%u/%d)\n",
					control->control_id, ret);
			goto p_err;
		}

		break;
	default:
		ret = -EINVAL;
		dsp_err("control id is invalid(%u)\n", control->control_id);
		goto p_err;
	}

	dsp_leave();
	return 0;
p_err:
	return ret;
}

static int dsp_context_control(struct dsp_context *dctx,
		struct dsp_ioc_control *args)
{
	int ret;
	unsigned int booted;
	struct dsp_system *sys;
	union dsp_control cmd;

	dsp_enter();
	dsp_dbg("control start\n");
	mutex_lock(&dctx->lock);
	booted = dctx->boot_count;
	mutex_unlock(&dctx->lock);
	if (!booted) {
		ret = -EINVAL;
		dsp_err("device is not normal booted\n");
		goto p_err;
	}

	sys = &dctx->core->dspdev->system;

	ret = __dsp_context_get_control(dctx, args, &cmd);
	if (ret)
		goto p_err;

	ret = sys->ops->request_control(sys, args->control_id, &cmd);
	if (ret)
		goto p_err;

	dsp_dbg("control end\n");
	dsp_leave();
	return 0;
p_err:
	return ret;
}

const struct dsp_ioctl_ops dsp_ioctl_ops = {
	.boot			= dsp_context_boot,
	.load_graph		= dsp_context_load_graph,
	.unload_graph		= dsp_context_unload_graph,
	.execute_msg		= dsp_context_execute_msg,
	.control		= dsp_context_control,
	.boot_secure		= dsp_context_boot_secure,
};

struct dsp_context *dsp_context_create(struct dsp_core *core)
{
	int ret;
	struct dsp_context *dctx;
	unsigned long flags;

	dsp_enter();
	dctx = kzalloc(sizeof(*dctx), GFP_KERNEL);
	if (!dctx) {
		ret = -ENOMEM;
		dsp_err("Failed to alloc context\n");
		goto p_err;
	}
	dctx->core = core;

	spin_lock_irqsave(&core->dctx_slock, flags);

	dctx->id = dsp_util_bitmap_set_region(&core->context_map, 1);
	if (dctx->id < 0) {
		spin_unlock_irqrestore(&core->dctx_slock, flags);
		ret = dctx->id;
		dsp_err("Failed to allocate context bitmap(%d)\n", ret);
		goto p_err_alloc_id;
	}

	list_add_tail(&dctx->list, &core->dctx_list);
	core->dctx_count++;
	spin_unlock_irqrestore(&core->dctx_slock, flags);

	mutex_init(&dctx->lock);
	dsp_leave();
	return dctx;
p_err_alloc_id:
	kfree(dctx);
p_err:
	return ERR_PTR(ret);
}

void dsp_context_destroy(struct dsp_context *dctx)
{
	struct dsp_core *core;
	unsigned long flags;

	dsp_enter();
	core = dctx->core;

	mutex_destroy(&dctx->lock);

	spin_lock_irqsave(&core->dctx_slock, flags);
	core->dctx_count--;
	list_del(&dctx->list);
	dsp_util_bitmap_clear_region(&core->context_map, dctx->id, 1);
	spin_unlock_irqrestore(&core->dctx_slock, flags);

	kfree(dctx);
	dsp_leave();
}

const struct dsp_ioctl_ops *dsp_context_get_ops(void)
{
	dsp_check();
	return &dsp_ioctl_ops;
}
