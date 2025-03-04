// SPDX-License-Identifier: GPL-2.0
/*
 * Samsung Exynos SoC series dsp driver
 *
 * Copyright (c) 2020 Samsung Electronics Co., Ltd.
 *              http://www.samsung.com/
 */

#include "dsp-log.h"
#include "dsp-hw-dummy-system.h"
#include "dsp-hw-dummy-dump.h"

void dsp_hw_dummy_dump_set_value(struct dsp_dump *dump, unsigned int dump_value)
{
	dsp_enter();
	dsp_leave();
}

void dsp_hw_dummy_dump_print_value(struct dsp_dump *dump)
{
	dsp_enter();
	dsp_leave();
}

void dsp_hw_dummy_dump_print_status_user(struct dsp_dump *dump,
		struct seq_file *file)
{
	dsp_enter();
	dsp_leave();
}

void dsp_hw_dummy_dump_ctrl(struct dsp_dump *dump)
{
	dsp_enter();
	dsp_leave();
}

void dsp_hw_dummy_dump_ctrl_user(struct dsp_dump *dump, struct seq_file *file)
{
	dsp_enter();
	dsp_leave();
}

void dsp_hw_dummy_dump_mailbox_pool_error(struct dsp_dump *dump,
		struct dsp_mailbox_pool *pool)
{
	dsp_enter();
	dsp_leave();
}

void dsp_hw_dummy_dump_mailbox_pool_debug(struct dsp_dump *dump,
		struct dsp_mailbox_pool *pool)
{
	dsp_enter();
	dsp_leave();
}

void dsp_hw_dummy_dump_task_manager_count(struct dsp_dump *dump,
		struct dsp_task_manager *tmgr)
{
	dsp_enter();
	dsp_leave();
}

void dsp_hw_dummy_dump_kernel(struct dsp_dump *dump,
		struct dsp_kernel_manager *kmgr)
{
	dsp_enter();
	dsp_leave();
}

int dsp_hw_dummy_dump_open(struct dsp_dump *dump)
{
	dsp_enter();
	dsp_leave();
	return 0;
}

int dsp_hw_dummy_dump_close(struct dsp_dump *dump)
{
	dsp_enter();
	dsp_leave();
	return 0;
}

int dsp_hw_dummy_dump_probe(struct dsp_system *sys)
{
	dsp_enter();
	dsp_leave();
	return 0;
}

void dsp_hw_dummy_dump_remove(struct dsp_dump *dump)
{
	dsp_enter();
	dsp_leave();
}
