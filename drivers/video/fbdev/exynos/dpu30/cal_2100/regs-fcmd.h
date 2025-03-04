/* SPDX-License-Identifier: GPL-2.0-only
 *
 * dpu30/cal_2100/regs-fcmd.h
 *
 * Copyright (c) 2019 Samsung Electronics Co., Ltd.
 *		http://www.samsung.com
 *
 * Haksoo Kim <herb@samsung.com>
 *
 * Register definition file for Samsung Display DMA DSIM Fast Command driver
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#ifndef FCMD_REGS_H_
#define FCMD_REGS_H_

/* SHADOW: 0x400 ~ 0x800 */
#define DMA_DSIMFC_SHD_OFFSET				0x0400

/*
 *-------------------------------------------------------------------
 * FCMD(DPU_F0_L12, DPU_F0_L13, DPU_F1_L12) SFR list
 * DPU_F0_L12 base address : 0x19CB_C000
 * DPU_F0_L13 base address : 0x19CB_D000
 * DPU_F1_L12 base address : 0x1AEB_C000
 *-------------------------------------------------------------------
 */

#define DMA_DSIMFC_ENABLE			0x0000
#define DSIMFC_SRESET				(1 << 8)
#define DSIMFC_OP_STATUS				(1 << 2)
#define DSIMFC_OP_STATUS_IDLE			(0)
#define DSIMFC_OP_STATUS_BUSY			(1)
#define DSIMFC_OP_STATUS_GET(_v)			(((_v) >> 2) & 0x1)
#define DSIMFC_START				(1 << 0)

#define DMA_DSIMFC_IRQ				0x0004
#define DSIMFC_CONFIG_ERR_IRQ			(1 << 21)
#define DSIMFC_READ_SLAVE_ERR_IRQ			(1 << 19)
#define DSIMFC_DEADLOCK_IRQ			(1 << 17)
#define DSIMFC_FRAME_DONE_IRQ			(1 << 16)
#define DSIMFC_ALL_IRQ_CLEAR			(0x2B << 16)
#define DSIMFC_CONFIG_ERR_MASK			(1 << 6)
#define DSIMFC_READ_SLAVE_ERR_MASK		(1 << 4)
#define DSIMFC_DEADLOCK_MASK			(1 << 2)
#define DSIMFC_FRAME_DONE_MASK			(1 << 1)
#define DSIMFC_ALL_IRQ_MASK			(0x2B << 1)
#define DSIMFC_IRQ_ENABLE				(1 << 0)

#define DMA_DSIMFC_IN_CTRL			0x0008
#define DSIMFC_IC_MAX(_v)				((_v) << 16)
#define DSIMFC_IC_MAX_MASK			(0xff << 16)

#define DMA_DSIMFC_PKT_HDR			0x0010
#define DSIMFC_BTA_TYPE(_v)			((_v) << 20)
#define DSIMFC_BTA_TYPE_MASK			(0x1 << 20)
#define DSIMFC_PKT_TYPE(_v)			((_v) << 16)
#define DSIMFC_PKT_TYPE_MASK			(0x1 << 16)
#define DSIMFC_PKT_TYPE_GENERIC_LONG_WRITE	(0)
#define DSIMFC_PKT_TYPE_DCS_LONG_WRITE		(1)
#define DSIMFC_PKT_DI(_v)				((_v) << 8)
#define DSIMFC_PKT_DI_MASK			(0xff << 8)
#define DSIMFC_PKT_CMD(_v)			((_v) << 0)
#define DSIMFC_PKT_CMD_MASK			(0xff << 0)

#define DMA_DSIMFC_PKT_SIZE			0x0014
#define DSIMFC_PKT_SIZE(_v)			((_v) << 0)
#define DSIMFC_PKT_SIZE_MASK			(0xffffffff << 0)

#define DMA_DSIMFC_PKT_CTRL			0x0018
#define DSIMFC_PKT_UNIT(_v)			((_v) << 0)
#define DSIMFC_PKT_UNIT_MASK			(0xfff << 0)

#define DMA_DSIMFC_BASE_ADDR			0x0040
#define DSIMFC_BASE_ADDR(_v)			((_v) << 0)
#define DSIMFC_BASE_ADDR_MASK			(0xffffffff << 0)

#define DMA_DSIMFC_DEADLOCK_CTRL		0x0100
#define DSIMFC_DEADLOCK_NUM(_v)			((_v) << 1)
#define DSIMFC_DEADLOCK_NUM_MASK			(0x7fffffff << 1)
#define DSIMFC_DEADLOCK_NUM_EN			(1 << 0)

#define DMA_DSIMFC_BUS_CTRL			0x0110
#define DSIMFC_ARCACHE(_v)			((_v) << 0)
#define DSIMFC_ARCACHE_MASK			(0xf << 0)

#define DMA_DSIMFC_LLC_CTRL			0x0114
#define DSIMFC_DATA_SAHRE_TYPE_P0(_v)		((_v) << 4)
#define DSIMFC_DATA_SAHRE_TYPE_P0_MASK		(0x3 << 4)
#define DSIMFC_LLC_HINT_P0(_v)			((_v) << 0)
#define DSIMFC_LLC_HINT_P0_MASK			(0x7 << 0)

#define DMA_DSIMFC_PERF_CTRL			0x0120
#define DSIMFC_DEGRADATION_TIME(_v)		((_v) << 16)
#define DSIMFC_DEGRADATION_TIME_MASK		(0xFFFF << 16)
#define DSIMFC_IC_MAX_DEG(_v)			((_v) << 4)
#define DSIMFC_IC_MAX_DEG_MASK			(0xFF << 4)
#define DSIMFC_DEGRADATION_EN			(1 << 0)

/* _n: [0,7], _v: [0x0, 0xF] */
#define DMA_DSIMFC_QOS_LUT_LOW			0x0130
#define DMA_DSIMFC_QOS_LUT_HIGH			0x0134
#define DSIMFC_QOS_LUT(_n, _v)			((_v) << (4*(_n)))
#define DSIMFC_QOS_LUT_MASK(_n)			(0xF << (4*(_n)))

#define DMA_DSIMFC_DYNAMIC_GATING		0x0140
#define DSIMFC_SRAM_CG_EN				(1 << 31)
#define DSIMFC_DG_EN(_n, _v)			((_v) << (_n))
#define DSIMFC_DG_EN_MASK(_n)			(1 << (_n))
#define DSIMFC_DG_EN_ALL				(0x7FFFFFFF << 0)

#define DMA_DSIMFC_MST_SECURITY			0x200

#define DMA_DSIMFC_SLV_SECURITY			0x204

#define DMA_DSIMFC_DEBUG_CTRL			0x0300
#define DSIMFC_DEBUG_SEL(_v)			((_v) << 16)
#define DSIMFC_DEBUG_EN				(0x1 << 0)

#define DMA_DSIMFC_DEBUG_DATA			0x0304

#define DMA_DSIMFC_PSLV_ERR_CTRL			0x030c
#define DSIMFC_PSLVERR_CTRL			(1 << 0)

#define DMA_DSIMFC_PKT_TRANSFER			0x0420
#define DSIMFC_PKT_TR_SIZE_GET(_v)		(((_v) >> 0) & 0xffffffff)

#define DMA_DSIMFC_S_CONFIG_ERR_STATUS		0x0740

#endif
