/*
 * Samsung EXYNOS FIMC-IS (Imaging Subsystem) driver
 *
 * Copyright (C) 2014 Samsung Electronics Co., Ltd.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#ifndef IS_INTERFACE_ISCHAIN_H
#define IS_INTERFACE_ISCHAIN_H
#include "../hardware/is-hw-control.h"

#define IRQ_NAME_LENGTH 	16

struct is_hardware;

enum is_interface_ischain_state {
	IS_CHAIN_IF_STATE_INIT,
	IS_CHAIN_IF_STATE_OPEN,
	IS_CHAIN_IF_STATE_REGISTERED,
	IS_CHAIN_IF_STATE_MAX
};

typedef int (*hwip_handler)(u32 id, void *context);

struct hwip_intr_handler {
	u32 valid;
	u32 priority;
	u32 id;
	void *ctx;
	hwip_handler handler;
	u32 chain_id;
};

struct is_interface_hwip {
	int				id;
	ulong				state;

	/* interrupt */
	int				irq[INTR_HWIP_MAX];
	char				irq_name[INTR_HWIP_MAX][IRQ_NAME_LENGTH];
	struct hwip_intr_handler	handler[INTR_HWIP_MAX];

	struct is_hw_ip		*hw_ip;
};

/**
 * struct is_interface_ischain - Sub IPs in ischain interrupt interface structure
 * @state: is chain interface state
 */
struct is_interface_ischain {
	ulong				state;
	struct is_interface_hwip	itf_ip[HW_SLOT_MAX];

	struct is_minfo		*minfo;
};

int is_interface_ischain_probe(struct is_interface_ischain *this,
	struct is_hardware *hardware, struct is_resourcemgr *resourcemgr,
	struct platform_device *pdev, ulong core_regs);
#endif
