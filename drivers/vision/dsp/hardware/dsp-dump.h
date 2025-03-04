/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Samsung Exynos SoC series dsp driver
 *
 * Copyright (c) 2019 Samsung Electronics Co., Ltd.
 *              http://www.samsung.com/
 */

#ifndef __HW_DSP_DUMP_H__
#define __HW_DSP_DUMP_H__

#include "dsp-kernel.h"
#include "dsp-task.h"
#include "hardware/dsp-ctrl.h"
#include "hardware/dsp-mailbox.h"

struct dsp_system;
struct dsp_dump;

struct dsp_dump_ops {
	void (*set_value)(struct dsp_dump *dump, unsigned int dump_value);
	void (*print_value)(struct dsp_dump *dump);
	void (*print_status_user)(struct dsp_dump *dump, struct seq_file *file);
	void (*ctrl)(struct dsp_dump *dump);
	void (*ctrl_user)(struct dsp_dump *dump, struct seq_file *file);

	void (*mailbox_pool_error)(struct dsp_dump *dump,
			struct dsp_mailbox_pool *pool);
	void (*mailbox_pool_debug)(struct dsp_dump *dump,
			struct dsp_mailbox_pool *pool);
	void (*task_manager_count)(struct dsp_dump *dump,
			struct dsp_task_manager *tmgr);
	void (*kernel)(struct dsp_dump *dump, struct dsp_kernel_manager *kmgr);

	int (*open)(struct dsp_dump *dump);
	int (*close)(struct dsp_dump *dump);
	int (*probe)(struct dsp_system *sys);
	void (*remove)(struct dsp_dump *dump);
};

struct dsp_dump {
	unsigned int			dump_value;

	const struct dsp_dump_ops	*ops;
};

void dsp_dump_set_value(unsigned int dump_value);
void dsp_dump_print_value(void);
void dsp_dump_print_status_user(struct seq_file *file);
void dsp_dump_ctrl(void);
void dsp_dump_ctrl_user(struct seq_file *file);
void dsp_dump_mailbox_pool_error(struct dsp_mailbox_pool *pool);
void dsp_dump_mailbox_pool_debug(struct dsp_mailbox_pool *pool);
void dsp_dump_task_manager_count(struct dsp_task_manager *tmgr);
void dsp_dump_kernel(struct dsp_kernel_manager *kmgr);

int dsp_dump_set_ops(struct dsp_dump *dump, unsigned int dev_id);
int dsp_dump_register_ops(unsigned int dev_id, const struct dsp_dump_ops *ops);

#endif
