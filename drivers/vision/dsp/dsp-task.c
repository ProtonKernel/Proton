// SPDX-License-Identifier: GPL-2.0
/*
 * Samsung Exynos SoC series dsp driver
 *
 * Copyright (c) 2019 Samsung Electronics Co., Ltd.
 *              http://www.samsung.com/
 */

#include <linux/slab.h>

#include "dsp-log.h"
#include "dsp-util.h"
#include "hardware/dsp-system.h"
#include "dsp-task.h"

static void __dsp_task_set_ready(struct dsp_task_manager *tmgr,
		struct dsp_task *task)
{
	dsp_enter();
	task->state = DSP_TASK_STATE_READY;
	list_add_tail(&task->state_list, &tmgr->ready_list);
	tmgr->ready_count++;
	dsp_leave();
}

static int __dsp_task_del_ready(struct dsp_task_manager *tmgr,
		struct dsp_task *task)
{
	dsp_enter();
	if (task->state != DSP_TASK_STATE_READY) {
		dsp_warn("task state(%u) is not ready(%u)\n",
				task->state, task->id);
		return -EINVAL;
	}

	if (!tmgr->ready_count) {
		dsp_warn("task manager doesn't have ready task(%u)\n",
				task->id);
		return -EINVAL;
	}

	list_del(&task->state_list);
	tmgr->ready_count--;

	dsp_leave();
	return 0;
}

static void __dsp_task_set_process(struct dsp_task_manager *tmgr,
		struct dsp_task *task)
{
	dsp_enter();
	task->state = DSP_TASK_STATE_PROCESS;
	list_add_tail(&task->state_list, &tmgr->process_list);
	tmgr->process_count++;
	dsp_leave();
}

static int __dsp_task_del_process(struct dsp_task_manager *tmgr,
		struct dsp_task *task)
{
	dsp_enter();
	if (task->state != DSP_TASK_STATE_PROCESS) {
		dsp_warn("task state(%u) is not process(%u)\n",
				task->state, task->id);
		return -EINVAL;
	}

	if (!tmgr->process_count) {
		dsp_warn("task manager doesn't have process task(%u)\n",
				task->id);
		return -EINVAL;
	}

	list_del(&task->state_list);
	tmgr->process_count--;

	dsp_leave();
	return 0;
}

static void __dsp_task_set_complete(struct dsp_task_manager *tmgr,
		struct dsp_task *task)
{
	dsp_enter();
	task->state = DSP_TASK_STATE_COMPLETE;
	list_add_tail(&task->state_list, &tmgr->complete_list);
	tmgr->complete_count++;
	dsp_leave();
}

static int __dsp_task_del_complete(struct dsp_task_manager *tmgr,
		struct dsp_task *task)
{
	dsp_enter();
	if (task->state != DSP_TASK_STATE_COMPLETE) {
		dsp_warn("task state(%u) is not complete(%u)\n",
				task->state, task->id);
		return -EINVAL;
	}

	if (!tmgr->complete_count) {
		dsp_warn("task manager doesn't have complete task(%u)\n",
				task->id);
		return -EINVAL;
	}

	list_del(&task->state_list);
	tmgr->complete_count--;

	dsp_leave();
	return 0;
}

int dsp_task_trans_ready_to_process(struct dsp_task *task)
{
	int ret;
	struct dsp_task_manager *tmgr = task->owner;

	dsp_enter();
	if (tmgr->block && !task->force) {
		ret = -ENOSTR;
		dsp_warn("task manager is blocked\n");
		goto p_err;
	}

	ret = __dsp_task_del_ready(tmgr, task);
	if (ret)
		goto p_err;

	__dsp_task_set_process(tmgr, task);

	dsp_leave();
	return 0;
p_err:
	return ret;
}

int dsp_task_trans_ready_to_complete(struct dsp_task *task)
{
	int ret;
	struct dsp_task_manager *tmgr = task->owner;

	dsp_enter();
	ret = __dsp_task_del_ready(tmgr, task);
	if (ret)
		goto p_err;

	__dsp_task_set_complete(tmgr, task);

	dsp_leave();
	return 0;
p_err:
	return ret;
}

int dsp_task_trans_process_to_complete(struct dsp_task *task)
{
	int ret;
	struct dsp_task_manager *tmgr = task->owner;

	dsp_enter();
	ret = __dsp_task_del_process(tmgr, task);
	if (ret)
		goto p_err;

	__dsp_task_set_complete(tmgr, task);

	dsp_leave();
	return 0;
p_err:
	return ret;
}

static int __dsp_task_destroy_state(struct dsp_task_manager *tmgr,
		struct dsp_task *task)
{
	int ret;

	dsp_enter();
	switch (task->state) {
	case DSP_TASK_STATE_READY:
		ret = __dsp_task_del_ready(tmgr, task);
		break;
	case DSP_TASK_STATE_PROCESS:
		ret = __dsp_task_del_process(tmgr, task);
		break;
	case DSP_TASK_STATE_COMPLETE:
		ret = __dsp_task_del_complete(tmgr, task);
		break;
	default:
		ret = -EINVAL;
		dsp_warn("task state(%u) is invalid(%u)\n",
				task->state, task->id);
	}
	dsp_leave();
	return ret;
}

int dsp_task_trans_any_to_complete(struct dsp_task *task)
{
	int ret;
	struct dsp_task_manager *tmgr = task->owner;

	dsp_enter();
	if (task->state != DSP_TASK_STATE_COMPLETE) {
		ret = __dsp_task_destroy_state(tmgr, task);
		__dsp_task_set_complete(tmgr, task);
	} else {
		ret = 0;
	}

	dsp_leave();
	return ret;
}

struct dsp_task *dsp_task_get_process_by_id(struct dsp_task_manager *tmgr,
		unsigned int id)
{
	struct dsp_task *task, *temp;

	dsp_enter();
	list_for_each_entry_safe(task, temp, &tmgr->process_list, state_list) {
		if (task->id == id)
			return task;
	}
	dsp_leave();
	return NULL;
}

struct dsp_task *dsp_task_get_any_by_id(struct dsp_task_manager *tmgr,
		unsigned int id)
{
	struct dsp_task *task, *temp;

	dsp_enter();
	list_for_each_entry_safe(task, temp, &tmgr->total_list, total_list) {
		if (task->id == id)
			return task;
	}
	dsp_leave();
	return NULL;
}

struct dsp_task *dsp_task_create(struct dsp_task_manager *tmgr, bool force)
{
	int ret;
	unsigned long flags;
	struct dsp_task *task;

	dsp_enter();
	task = kzalloc(sizeof(*task), GFP_KERNEL);
	if (!task) {
		ret = -ENOMEM;
		dsp_err("Failed to allocate task\n");
		return ERR_PTR(ret);
	}
	task->owner = tmgr;
	task->force = force;

	spin_lock_irqsave(&tmgr->slock, flags);

	if (tmgr->block && !force) {
		ret = -ENOSTR;
		dsp_warn("task manager is blocked\n");
		goto p_err;
	}

	task->id = dsp_util_bitmap_set_region(&tmgr->task_map, 1);
	if (task->id < 0) {
		ret = task->id;
		dsp_err("Failed to allocate task bitmap(%d)\n", ret);
		goto p_err;
	}

	list_add_tail(&task->total_list, &tmgr->total_list);
	tmgr->total_count++;
	__dsp_task_set_ready(tmgr, task);
	spin_unlock_irqrestore(&tmgr->slock, flags);

	dsp_leave();
	return task;
p_err:
	spin_unlock_irqrestore(&tmgr->slock, flags);
	kfree(task);
	return ERR_PTR(ret);
}

static void __dsp_task_destroy(struct dsp_task_manager *tmgr,
		struct dsp_task *task)
{
	unsigned long flags;

	dsp_enter();
	spin_lock_irqsave(&tmgr->slock, flags);
	__dsp_task_destroy_state(tmgr, task);
	tmgr->total_count--;
	list_del(&task->total_list);
	dsp_util_bitmap_clear_region(&tmgr->task_map, task->id, 1);
	spin_unlock_irqrestore(&tmgr->slock, flags);

	kfree(task);
	dsp_leave();
}

void dsp_task_destroy(struct dsp_task *task)
{
	__dsp_task_destroy(task->owner, task);
}

void dsp_task_manager_set_block_mode(struct dsp_task_manager *tmgr, bool block)
{
	dsp_enter();
	tmgr->block = block;
	dsp_leave();
}

int dsp_task_manager_flush_process(struct dsp_task_manager *tmgr, int result)
{
	struct dsp_task *task, *temp;

	dsp_enter();
	list_for_each_entry_safe(task, temp, &tmgr->process_list, state_list) {
		dsp_warn("process task[%u] is flushed(count:%u)\n",
				task->id, tmgr->process_count);
		task->result = result;
		dsp_task_trans_process_to_complete(task);
	}

	dsp_leave();
	return 0;
}

void dsp_task_manager_dump_count(struct dsp_task_manager *tmgr)
{
	dsp_enter();

	dsp_notice("[DSP_TASK_DUMP_COUNT] %u/%u/%u/%u/%u/%u\n",
		tmgr->total_count, tmgr->ready_count,
		tmgr->process_count, tmgr->complete_count,
		tmgr->normal_count, tmgr->error_count);

	dsp_leave();
}

int dsp_task_manager_probe(struct dsp_system *sys)
{
	int ret;
	struct dsp_task_manager *tmgr;

	dsp_enter();
	tmgr = &sys->task_manager;

	INIT_LIST_HEAD(&tmgr->total_list);
	tmgr->total_count = 0;
	INIT_LIST_HEAD(&tmgr->ready_list);
	tmgr->ready_count = 0;
	INIT_LIST_HEAD(&tmgr->process_list);
	tmgr->process_count = 0;
	INIT_LIST_HEAD(&tmgr->complete_list);
	tmgr->complete_count = 0;

	tmgr->normal_count = 0;
	tmgr->error_count = 0;

	spin_lock_init(&tmgr->slock);
	init_waitqueue_head(&tmgr->done_wq);
	tmgr->block = false;

	ret = dsp_util_bitmap_init(&tmgr->task_map, "task_bitmap",
			DSP_TASK_MAX_COUNT);
	if (ret)
		goto p_err;

	dsp_leave();
	return 0;
p_err:
	return ret;
}

void dsp_task_manager_remove(struct dsp_task_manager *tmgr)
{
	struct dsp_task *task, *temp;

	dsp_enter();
	list_for_each_entry_safe(task, temp, &tmgr->total_list,
			total_list) {
		dsp_warn("task[%u] is destroyed(count:%u)\n",
				task->id, tmgr->total_count);
		__dsp_task_destroy(tmgr, task);
	}
	dsp_util_bitmap_deinit(&tmgr->task_map);
	dsp_leave();
}
