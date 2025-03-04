// SPDX-License-Identifier: GPL-2.0
/*
 * Samsung Exynos SoC series Pablo driver
 *
 * Copyright (c) 2020 Samsung Electronics Co., Ltd
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#include <asm/fpsimd.h>
#include <asm/neon.h>
#include <trace/hooks/fpsimd.h>
#include <linux/sched.h>

#include "is-hw.h"
#include "is-config.h"

/* Using android vendor hook to save and restore fpsimd regs */
#if defined(ENABLE_FPSIMD_FOR_USER) && defined(VH_FPSIMD_API)
#define NUM_OF_MAX_DEPTH	10

struct is_percpu_fpsimd_state_struct {
	struct is_fpsimd_state isr_fpsimd_state;
	struct is_fpsimd_state func_fpsimd_state[NUM_OF_MAX_DEPTH];
	atomic_t func_fpsimd_depth;
};
static struct is_percpu_fpsimd_state_struct __percpu *is_percpu_fpsimd_state;

void is_fpsimd_get_isr(void)
{
	struct is_percpu_fpsimd_state_struct *p = this_cpu_ptr(is_percpu_fpsimd_state);

	is_fpsimd_save_state(&p->isr_fpsimd_state);
}

void is_fpsimd_put_isr(void)
{
	struct is_percpu_fpsimd_state_struct *p = this_cpu_ptr(is_percpu_fpsimd_state);

	is_fpsimd_load_state(&p->isr_fpsimd_state);
}

void is_fpsimd_get_func(void)
{
	ulong flags;
	struct is_percpu_fpsimd_state_struct *p;

	preempt_disable();

	local_irq_save(flags);
	p = this_cpu_ptr(is_percpu_fpsimd_state);
	is_fpsimd_save_state(&p->func_fpsimd_state[atomic_read(&p->func_fpsimd_depth)]);
	atomic_inc(&p->func_fpsimd_depth);
	BUG_ON(atomic_read(&p->func_fpsimd_depth) >= NUM_OF_MAX_DEPTH);
	local_irq_restore(flags);
}

void is_fpsimd_put_func(void)
{
	ulong flags;
	struct is_percpu_fpsimd_state_struct *p;

	local_irq_save(flags);
	p = this_cpu_ptr(is_percpu_fpsimd_state);
	atomic_dec(&p->func_fpsimd_depth);
	is_fpsimd_load_state(&p->func_fpsimd_state[atomic_read(&p->func_fpsimd_depth)]);
	local_irq_restore(flags);

	preempt_enable();
}

void is_fpsimd_get_task(void)
{

}

void is_fpsimd_put_task(void)
{

}

static void is_fpsimd_save(void *data, struct task_struct *prev, struct task_struct *next)
{
	struct is_kernel_fpsimd_state *cur_kst =
		(struct is_kernel_fpsimd_state *)prev->thread.android_vendor_data1;
	struct is_kernel_fpsimd_state  *nxt_kst =
		(struct is_kernel_fpsimd_state *)next->thread.android_vendor_data1;

	if (!IS_ERR_OR_NULL(cur_kst) && atomic_read(&cur_kst->depth) == 0x12345) {
		is_fpsimd_save_state(&cur_kst->cur);
		is_fpsimd_load_state(&cur_kst->pre);
		dbg("[@][%d][VH]  prev(%d:%s), cur(%d:%s), next(%d:%s)\n",
			smp_processor_id(),
			prev->pid, prev->comm,
			current->pid, current->comm,
			next->pid, next->comm);
	}

	if (!IS_ERR_OR_NULL(nxt_kst) && atomic_read(&nxt_kst->depth) == 0x12345) {
		is_fpsimd_save_state(&nxt_kst->pre);
		is_fpsimd_load_state(&nxt_kst->cur);
		dbg("[@][%d][VH]  prev(%d:%s), cur(%d:%s), next(%d:%s)\n",
			smp_processor_id(),
			prev->pid, prev->comm,
			current->pid, current->comm,
			next->pid, next->comm);
	}
}

int is_fpsimd_probe(void)
{
	int i;
	struct is_percpu_fpsimd_state_struct *p;

	register_trace_android_vh_is_fpsimd_save(is_fpsimd_save, NULL);

	is_percpu_fpsimd_state = alloc_percpu(struct is_percpu_fpsimd_state_struct);
	if (!is_percpu_fpsimd_state) {
		probe_err("is_percpu_fpsimd_state is NULL");
		return -ENOMEM;
	}

	for_each_possible_cpu(i) {
		p = per_cpu_ptr(is_percpu_fpsimd_state, i);
		atomic_set(&p->func_fpsimd_depth, 0);
	}

	return 0;
}

void is_fpsimd_set_task_using(struct task_struct *t, struct is_kernel_fpsimd_state *kst)
{
	if (t->thread.android_vendor_data1 || !kst)
		return;

	atomic_set(&kst->depth, 0x12345);
	t->thread.android_vendor_data1 = (u64)kst;

	dbg_lib(5, "[@][%d][VH] %s: pid %d is set as camera task\n",
		smp_processor_id(), __func__, t->pid);
}
#endif
