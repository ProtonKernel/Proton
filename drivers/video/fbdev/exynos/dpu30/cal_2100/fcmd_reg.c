// SPDX-License-Identifier: GPL-2.0-only
/*
 * dpu30/cal_2100/fcmd_regs.c
 *
 * Copyright (c) 2019 Samsung Electronics Co., Ltd.
 *		http://www.samsung.com
 *
 * Haksoo Kim <herb@samsung.com>
 *
 * Register access functions for Samsung Display DMA DSIM Fast Command driver
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published
 * by the Free Software Foundation, either version 2 of the License,
 * or (at your option) any later version.
 */

#include <linux/io.h>
#include <linux/delay.h>
#include <linux/ktime.h>
#include <linux/iopoll.h>

#include "../fcmd.h"

/****************** RDMA FCMD CAL functions ******************/
static void fcmd_reg_set_sw_reset(u32 id)
{
	fcmd_write_mask(id, DMA_DSIMFC_ENABLE, ~0, DSIMFC_SRESET);
}

static int fcmd_reg_wait_sw_reset_status(u32 id)
{
	struct fcmd_device *fcmd = get_fcmd_drvdata(id);
	u32 val;
	int ret;

	ret = readl_poll_timeout(fcmd->res.regs + DMA_DSIMFC_ENABLE,
			val, !(val & DSIMFC_SRESET), 10, 2000); /* timeout 2ms */
	if (ret) {
		fcmd_err("[fcmd%d] timeout sw-reset\n", id);
		return ret;
	}

	return 0;
}

void fcmd_reg_set_start(u32 id)
{
	fcmd_write_mask(id, DMA_DSIMFC_ENABLE, 1, DSIMFC_START);
}

u32 fcmd_reg_get_op_status(u32 id)
{
	u32 val;

	val = fcmd_read(id, DMA_DSIMFC_ENABLE);
	return DSIMFC_OP_STATUS_GET(val);
}

static void fcmd_reg_set_irq_mask_all(u32 id, u32 en)
{
	u32 val = en ? ~0 : 0;

	fcmd_write_mask(id, DMA_DSIMFC_IRQ, val, DSIMFC_ALL_IRQ_MASK);
}

static void fcmd_reg_set_irq_enable(u32 id)
{
	fcmd_write_mask(id, DMA_DSIMFC_IRQ, ~0, DSIMFC_IRQ_ENABLE);
}

static void fcmd_reg_set_irq_disable(u32 id)
{
	fcmd_write_mask(id, DMA_DSIMFC_IRQ, 0, DSIMFC_IRQ_ENABLE);
}

static void fcmd_reg_clear_irq(u32 id, u32 irq)
{
	fcmd_write_mask(id, DMA_DSIMFC_IRQ, ~0, irq);
}

static void fcmd_reg_set_ic_max(u32 id, u32 ic_max)
{
	fcmd_write_mask(id, DMA_DSIMFC_IN_CTRL, DSIMFC_IC_MAX(ic_max),
			DSIMFC_IC_MAX_MASK);
}

static void fcmd_reg_set_bta_type(u32 id, u32 type)
{
	fcmd_write_mask(id, DMA_DSIMFC_PKT_HDR,
			DSIMFC_BTA_TYPE(type), DSIMFC_BTA_TYPE_MASK);
}

static void fcmd_reg_set_pkt_type(u32 id, u32 type)
{
	fcmd_write_mask(id, DMA_DSIMFC_PKT_HDR,
			DSIMFC_PKT_TYPE(type), DSIMFC_PKT_TYPE_MASK);
}

static void fcmd_reg_set_pkt_di(u32 id, u32 di)
{
	fcmd_write_mask(id, DMA_DSIMFC_PKT_HDR,
			DSIMFC_PKT_DI(di), DSIMFC_PKT_DI_MASK);
}


static void fcmd_reg_set_pkt_cmd(u32 id, u32 cmd)
{
	fcmd_write_mask(id, DMA_DSIMFC_PKT_HDR,
			DSIMFC_PKT_CMD(cmd), DSIMFC_PKT_CMD_MASK);
}

static void fcmd_reg_set_pkt_size(u32 id, u32 size)
{
	fcmd_write_mask(id, DMA_DSIMFC_PKT_SIZE,
			DSIMFC_PKT_SIZE(size), DSIMFC_PKT_SIZE_MASK);
}

static void fcmd_reg_set_pkt_unit(u32 id, u32 unit)
{
	fcmd_write_mask(id, DMA_DSIMFC_PKT_CTRL,
			DSIMFC_PKT_UNIT(unit), DSIMFC_PKT_UNIT_MASK);
}

static void fcmd_reg_set_base_addr(u32 id, u32 addr)
{
	fcmd_write_mask(id, DMA_DSIMFC_BASE_ADDR,
			DSIMFC_BASE_ADDR(addr), DSIMFC_BASE_ADDR_MASK);
}

static void fcmd_reg_set_deadlock(u32 id, u32 en, u32 dl_num)
{
	u32 val = en ? ~0 : 0;

	fcmd_write_mask(id, DMA_DSIMFC_DEADLOCK_CTRL, val, DSIMFC_DEADLOCK_NUM_EN);
	fcmd_write_mask(id, DMA_DSIMFC_DEADLOCK_CTRL, DSIMFC_DEADLOCK_NUM(dl_num),
				DSIMFC_DEADLOCK_NUM_MASK);
}

static void fcmd_reg_set_in_qos_lut(u32 id, u32 lut_id, u32 qos_t)
{
	u32 reg_id;

	if (lut_id == 0)
		reg_id = DMA_DSIMFC_QOS_LUT_LOW;
	else
		reg_id = DMA_DSIMFC_QOS_LUT_HIGH;
	fcmd_write(id, reg_id, qos_t);
}

static void fcmd_reg_set_sram_clk_gate_en(u32 id, u32 en)
{
	u32 val = en ? ~0 : 0;

	fcmd_write_mask(id, DMA_DSIMFC_DYNAMIC_GATING, val, DSIMFC_SRAM_CG_EN);
}

static void fcmd_reg_set_dynamic_gating_en_all(u32 id, u32 en)
{
	u32 val = en ? ~0 : 0;

	fcmd_write_mask(id, DMA_DSIMFC_DYNAMIC_GATING, val, DSIMFC_DG_EN_ALL);
}

u32 fcmd_reg_get_pkt_transfer(u32 id)
{
	u32 val;

	val = fcmd_read(id, DMA_DSIMFC_PKT_TRANSFER);
	return DSIMFC_PKT_TR_SIZE_GET(val);
}

void fcmd_reg_init(u32 id)
{
	fcmd_reg_set_irq_mask_all(id, 1);
	fcmd_reg_set_irq_disable(id);
	fcmd_reg_set_in_qos_lut(id, 0, 0x44444444);
	fcmd_reg_set_in_qos_lut(id, 1, 0x44444444);
	fcmd_reg_set_sram_clk_gate_en(id, 0);
	fcmd_reg_set_dynamic_gating_en_all(id, 0);
	fcmd_reg_set_ic_max(id, 0x40);
	fcmd_reg_set_deadlock(id, 1, 0x7fffffff);
}

void fcmd_reg_set_config(u32 id)
{
	struct fcmd_device *fcmd = get_fcmd_drvdata(id);
	struct fcmd_config *config = fcmd->fcmd_config;

	fcmd_reg_set_bta_type(id, config->bta);
	fcmd_reg_set_pkt_type(id, config->type);
	fcmd_reg_set_pkt_di(id, config->di);
	fcmd_reg_set_pkt_cmd(id, config->cmd);
	fcmd_reg_set_pkt_size(id, config->size);
	fcmd_reg_set_pkt_unit(id, config->unit);
	fcmd_reg_set_base_addr(id, config->buf);
}

void fcmd_reg_irq_enable(u32 id)
{
	fcmd_reg_set_irq_mask_all(id, 0);
	fcmd_reg_set_irq_enable(id);
}

int fcmd_reg_deinit(u32 id, bool reset)
{
	fcmd_reg_clear_irq(id, DSIMFC_ALL_IRQ_CLEAR);
	fcmd_reg_set_irq_mask_all(id, 1);

	if (reset) {
		fcmd_reg_set_sw_reset(id);
		if (fcmd_reg_wait_sw_reset_status(id))
			return -1;
	}

	return 0;
}

u32 fcmd_reg_get_irq_and_clear(u32 id)
{
	u32 val, cfg_err;

	val = fcmd_read(id, DMA_DSIMFC_IRQ);
	if (val & DSIMFC_CONFIG_ERR_IRQ) {
		cfg_err = fcmd_read(id, DMA_DSIMFC_S_CONFIG_ERR_STATUS);
		fcmd_err("fcmd%d config error occur(0x%x)\n", id, cfg_err);
	}
	fcmd_reg_clear_irq(id, val);

	return val;
}

#define PREFIX_LEN	40
#define ROW_LEN		32
static void fcmd_print_hex_dump(void __iomem *regs, const void *buf, size_t len)
{
	char prefix_buf[PREFIX_LEN];
	unsigned long p;
	size_t i, row;

	for (i = 0; i < len; i += ROW_LEN) {
		p = buf - regs + i;

		if (len - i < ROW_LEN)
			row = len - i;
		else
			row = ROW_LEN;

		snprintf(prefix_buf, sizeof(prefix_buf), "[%08lX] ", p);
		print_hex_dump(KERN_NOTICE, prefix_buf, DUMP_PREFIX_NONE,
				32, 4, buf + i, row, false);
	}
}

void __fcmd_dump(u32 id, void __iomem *regs)
{
	fcmd_info("\n=== DMA_DSIMFC%d SFR DUMP ===\n", id);
	fcmd_print_hex_dump(regs, regs + 0x0000, 0x310);
	fcmd_print_hex_dump(regs, regs + 0x0420, 0x4);
	fcmd_print_hex_dump(regs, regs + 0x0740, 0x4);
}
