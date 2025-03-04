/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Samsung Exynos SoC series dsp driver
 *
 * Copyright (c) 2020 Samsung Electronics Co., Ltd.
 *              http://www.samsung.com/
 */

#ifndef __HW_DUMMY_DSP_HW_DUMMY_DUMP_H__
#define __HW_DUMMY_DSP_HW_DUMMY_DUMP_H__

#include "hardware/dsp-dump.h"

void dsp_hw_dummy_dump_set_value(struct dsp_dump *dump,
		unsigned int dump_value);
void dsp_hw_dummy_dump_print_value(struct dsp_dump *dump);
void dsp_hw_dummy_dump_print_status_user(struct dsp_dump *dump,
		struct seq_file *file);
void dsp_hw_dummy_dump_ctrl(struct dsp_dump *dump);
void dsp_hw_dummy_dump_ctrl_user(struct dsp_dump *dump, struct seq_file *file);

void dsp_hw_dummy_dump_mailbox_pool_error(struct dsp_dump *dump,
		struct dsp_mailbox_pool *pool);
void dsp_hw_dummy_dump_mailbox_pool_debug(struct dsp_dump *dump,
		struct dsp_mailbox_pool *pool);
void dsp_hw_dummy_dump_task_manager_count(struct dsp_dump *dump,
		struct dsp_task_manager *tmgr);
void dsp_hw_dummy_dump_kernel(struct dsp_dump *dump,
		struct dsp_kernel_manager *kmgr);

int dsp_hw_dummy_dump_open(struct dsp_dump *dump);
int dsp_hw_dummy_dump_close(struct dsp_dump *dump);
int dsp_hw_dummy_dump_probe(struct dsp_system *sys);
void dsp_hw_dummy_dump_remove(struct dsp_dump *dump);

#endif
