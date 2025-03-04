/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Samsung Exynos SoC series dsp driver
 *
 * Copyright (c) 2019 Samsung Electronics Co., Ltd.
 *              http://www.samsung.com/
 */

#ifndef __HW_DSP_INTERFACE_H__
#define __HW_DSP_INTERFACE_H__

#include "dsp-config.h"
#ifdef ENABLE_DSP_VELOCE
#include <linux/timer.h>
#endif

struct dsp_system;
struct dsp_interface;

enum dsp_to_cc_int_num {
	DSP_TO_CC_INT_RESET,
	DSP_TO_CC_INT_MAILBOX,
	DSP_TO_CC_INT_NUM,
};

enum dsp_to_host_int_num {
	DSP_TO_HOST_INT_BOOT,
	DSP_TO_HOST_INT_MAILBOX,
	DSP_TO_HOST_INT_RESET_DONE,
	DSP_TO_HOST_INT_RESET_REQUEST,
	DSP_TO_HOST_INT_NUM,
};

struct dsp_interface_ops {
	int (*send_irq)(struct dsp_interface *itf, int status);
	int (*check_irq)(struct dsp_interface *itf);

	int (*start)(struct dsp_interface *itf);
	int (*stop)(struct dsp_interface *itf);
	int (*open)(struct dsp_interface *itf);
	int (*close)(struct dsp_interface *itf);
	int (*probe)(struct dsp_system *sys);
	void (*remove)(struct dsp_interface *itf);
};

struct dsp_interface {
	void __iomem			*sfr;
	void				*sub_data;
	spinlock_t			irq_lock;
#ifdef ENABLE_DSP_VELOCE
	struct timer_list		isr_timer;
#endif

	const struct dsp_interface_ops	*ops;
	struct dsp_system		*sys;
};

int dsp_interface_set_ops(struct dsp_interface *itf, unsigned int dev_id);
int dsp_interface_register_ops(unsigned int dev_id,
		const struct dsp_interface_ops *ops);

#endif
