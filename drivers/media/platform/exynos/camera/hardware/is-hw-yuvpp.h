// SPDX-License-Identifier: GPL-2.0
/*
 * Samsung Exynos SoC series Pablo driver
 *
 * Exynos Pablo image subsystem functions
 *
 * Copyright (c) 2020 Samsung Electronics Co., Ltd
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#ifndef IS_HW_YUVPP_H
#define IS_HW_YUVPP_H

#include "is-hw-control.h"
#include "is-param.h"
#include "is-hw-common-dma.h"
#include "is-interface-ddk.h"
#include "sfr/is-sfr-yuvpp-v2_1.h"

enum is_hw_ypp_lib_mode {
	USE_ONLY_DDK	= 0,
	USE_DRIVER	= 1,
};

enum is_hw_ypp_rdma_index {
	YUVPP_RDMA_L0_Y,
	YUVPP_RDMA_L0_UV,
	YUVPP_RDMA_L2_Y,
	YUVPP_RDMA_L2_UV,
	YUVPP_RDMA_L2_1_Y,
	YUVPP_RDMA_L2_1_UV,
	YUVPP_RDMA_L2_2_Y,
	YUVPP_RDMA_L2_2_UV,
	YUVPP_RDMA_NOISE,
	YUVPP_RDMA_DRCGAIN,
	YUVPP_RDMA_HIST,
	YUVPP_RDMA_MAX
};

enum is_hw_ypp_irq_src {
	YUVPP_INTR,
	YUVPP_INTR_MAX,
};

struct is_hw_ypp_iq {
	struct cr_set	regs[YUVPP_REG_CNT];
	u32		size;
	spinlock_t	slock;

	u32		fcount;
	unsigned long	state;
};

enum is_hw_ypp_event_id {
     EVENT_FRAME_START = 1,
     EVENT_FRAME_END
};

struct is_hw_ypp {
	struct is_lib_isp		lib[IS_STREAM_COUNT];
	struct is_lib_support		*lib_support;
	struct lib_interface_func	*lib_func;
	struct ypp_param_set		param_set[IS_STREAM_COUNT];
	struct is_common_dma		rdma[YUVPP_RDMA_MAX];
	struct is_yuvpp_config		config;
	u32				irq_state[YUVPP_INTR_MAX];
	u32				instance;
	unsigned long 			state;
	atomic_t			strip_index;

	struct is_hw_ypp_iq		iq_set[IS_STREAM_COUNT];
	struct is_hw_ypp_iq		cur_hw_iq_set[COREX_MAX];
};

int is_hw_ypp_probe(struct is_hw_ip *hw_ip, struct is_interface *itf,
	struct is_interface_ischain *itfc, int id, const char *name);
int is_hw_ypp_set_regs(struct is_hw_ip *hw_ip, u32 instance, u32 fcount, struct cr_set *regs, u32 regs_size);
int is_hw_ypp_set_config(struct is_hw_ip *hw_ip, u32 instance, u32 fcount, struct is_yuvpp_config *conf);
#endif
