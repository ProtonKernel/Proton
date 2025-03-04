/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Samsung Exynos SoC series dsp driver
 *
 * Copyright (c) 2019 Samsung Electronics Co., Ltd.
 *              http://www.samsung.com/
 */

#ifndef __HW_DSP_SYSTEM_H__
#define __HW_DSP_SYSTEM_H__

#include <linux/device.h>
#include <linux/wait.h>

#include "hardware/dsp-pm.h"
#include "hardware/dsp-clk.h"
#include "hardware/dsp-bus.h"
#include "hardware/dsp-llc.h"
#include "hardware/dsp-memory.h"
#include "hardware/dsp-interface.h"
#include "hardware/dsp-ctrl.h"
#include "dsp-task.h"
#include "hardware/dsp-mailbox.h"
#include "hardware/dsp-governor.h"
#include "hardware/dsp-debug.h"
#include "hardware/dsp-dump.h"
#include "dsp-control.h"

#define DSP_SET_DEFAULT_LAYER	(0xffffffff)

struct dsp_device;

enum dsp_system_flag {
	DSP_SYSTEM_BOOT,
	DSP_SYSTEM_RESET,
};

enum dsp_system_boot_init {
	DSP_SYSTEM_DSP_INIT,
	DSP_SYSTEM_NPU_INIT,
};

enum dsp_system_wait {
	DSP_SYSTEM_WAIT_BOOT,
	DSP_SYSTEM_WAIT_MAILBOX,
	DSP_SYSTEM_WAIT_RESET,
	DSP_SYSTEM_WAIT_NUM,
};

struct dsp_system_ops {
	int (*request_control)(struct dsp_system *sys, unsigned int id,
			union dsp_control *cmd);
	int (*execute_task)(struct dsp_system *sys, struct dsp_task *task);
	void (*iovmm_fault_dump)(struct dsp_system *sys);
	int (*boot)(struct dsp_system *sys);
	int (*reset)(struct dsp_system *sys);
	int (*power_active)(struct dsp_system *sys);
	int (*set_boot_qos)(struct dsp_system *sys, int val);
	int (*runtime_resume)(struct dsp_system *sys);
	int (*runtime_suspend)(struct dsp_system *sys);
	int (*resume)(struct dsp_system *sys);
	int (*suspend)(struct dsp_system *sys);

	int (*npu_start)(struct dsp_system *sys, bool boot, dma_addr_t fw_iova);
	int (*start)(struct dsp_system *sys);
	int (*stop)(struct dsp_system *sys);

	int (*open)(struct dsp_system *sys);
	int (*close)(struct dsp_system *sys);
	int (*probe)(struct dsp_device *dspdev);
	void (*remove)(struct dsp_system *sys);
};

struct dsp_system {
	struct device			*dev;
	phys_addr_t			sfr_pa;
	void __iomem			*sfr;
	resource_size_t			sfr_size;
	void				*sub_data;

	unsigned long			boot_init;
	wait_queue_head_t		system_wq;
	unsigned int			system_flag;
	unsigned int			wait[DSP_SYSTEM_WAIT_NUM];
	bool				boost;
	struct mutex			boost_lock;
	char				fw_postfix[32];
	unsigned int			layer_start;
	unsigned int			layer_end;
	unsigned int			debug_mode;

	const struct dsp_system_ops	*ops;
	struct dsp_pm			pm;
	struct dsp_clk			clk;
	struct dsp_bus			bus;
	struct dsp_llc			llc;
	struct dsp_memory		memory;
	struct dsp_interface		interface;
	struct dsp_ctrl			ctrl;
	struct dsp_task_manager		task_manager;
	struct dsp_mailbox		mailbox;
	struct dsp_governor		governor;
	struct dsp_hw_debug		debug;
	struct dsp_dump			dump;
	struct dsp_device		*dspdev;
};

int dsp_system_set_ops(struct dsp_system *sys, unsigned int dev_id);
int dsp_system_register_ops(unsigned int dev_id,
		const struct dsp_system_ops *ops);
int dsp_system_init(void);

#endif
