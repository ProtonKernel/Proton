/* SPDX-License-Identifier: GPL-2.0-only */
/*
 * Copyright (c) 2020 Samsung Electronics Co., Ltd.
 */
#ifndef __EXYNOS_PASR_H
#define __EXYNOS_PASR_H

#include <linux/kthread.h>

#define MODULE_NAME		"exynos-pasr"

struct pasr_segment {
	int index;
	unsigned long addr;
};

struct pasr_thread {
	struct task_struct *thread;
	struct kthread_worker worker;
	struct kthread_work work;
	struct completion completion;
};

struct pasr_dev {
	struct device *dev;

	struct pasr_segment *segment_info;
	struct pasr_thread on_thread;
	struct pasr_thread off_thread;

	unsigned long offline_flag;
	unsigned int pasr_mask;
	int num_segment;
};
#endif /* __EXYNOS_PASR_H */
