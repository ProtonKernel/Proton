/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Samsung Exynos SoC series dsp driver
 *
 * Copyright (c) 2019 Samsung Electronics Co., Ltd.
 *              http://www.samsung.com/
 */

#ifndef __DSP_TASK_H__
#define __DSP_TASK_H__

#include <linux/spinlock.h>
#include <linux/wait.h>

#include "dsp-util.h"

#define DSP_TASK_MAX_COUNT		(128)

struct dsp_system;
struct dsp_mailbox_pool;

enum dsp_task_state {
	DSP_TASK_STATE_READY = 1,
	DSP_TASK_STATE_PROCESS,
	DSP_TASK_STATE_COMPLETE
};

struct dsp_task {
	unsigned int			state;
	int				id;
	unsigned int			message_id;
	unsigned int			message_version;
	struct dsp_mailbox_pool		*pool;
	int				result;
	bool				wait;
	bool				recovery;
	bool				force;

	struct dsp_task_manager		*owner;
	struct list_head		total_list;
	struct list_head		state_list;
};

struct dsp_task_manager {
	struct list_head		total_list;
	unsigned int			total_count;
	struct list_head		ready_list;
	unsigned int			ready_count;
	struct list_head		process_list;
	unsigned int			process_count;
	struct list_head		complete_list;
	unsigned int			complete_count;
	unsigned int			normal_count;
	unsigned int			error_count;

	spinlock_t			slock;
	struct dsp_util_bitmap		task_map;
	wait_queue_head_t		done_wq;
	bool				block;
};

int dsp_task_trans_ready_to_process(struct dsp_task *task);
int dsp_task_trans_ready_to_complete(struct dsp_task *task);
int dsp_task_trans_process_to_complete(struct dsp_task *task);
int dsp_task_trans_any_to_complete(struct dsp_task *task);

struct dsp_task *dsp_task_get_process_by_id(struct dsp_task_manager *tmgr,
		unsigned int id);
struct dsp_task *dsp_task_get_any_by_id(struct dsp_task_manager *tmgr,
		unsigned int id);

struct dsp_task *dsp_task_create(struct dsp_task_manager *tmgr, bool force);
void dsp_task_destroy(struct dsp_task *task);

void dsp_task_manager_set_block_mode(struct dsp_task_manager *tmgr, bool block);
int dsp_task_manager_flush_process(struct dsp_task_manager *tmgr, int result);
void dsp_task_manager_dump_count(struct dsp_task_manager *tmgr);

int dsp_task_manager_probe(struct dsp_system *sys);
void dsp_task_manager_remove(struct dsp_task_manager *tmgr);

#endif
