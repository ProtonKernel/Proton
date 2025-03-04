/*
 * Copyright (c) 2016 Samsung Electronics Co., Ltd.
 *		http://www.samsung.com
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#include "../decon.h"
#include "../dqe.h"
#include "regs-dqe.h"

#define PREFIX_LEN      40
#define ROW_LEN         32
static void dqe_print_hex_dump(void __iomem *regs, const void *buf, size_t len)
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

void __dqe_dump(u32 id, void __iomem *dqe_regs)
{
	dqe_info("=== DQE_TOP SFR DUMP ===\n");
	dqe_print_hex_dump(dqe_regs, dqe_regs + 0x0000, 0x100);
	dqe_info("=== DQE_TOP SHADOW SFR DUMP ===\n");
	dqe_print_hex_dump(dqe_regs, dqe_regs + 0x0000 + SHADOW_DQE_OFFSET, 0x100);

	dqe_info("=== DQE_GAMMA_MATRIX SFR DUMP ===\n");
	dqe_print_hex_dump(dqe_regs, dqe_regs + 0x1400, 0x20);
	dqe_info("=== DQE_GAMMA_MATRIX SHADOW SFR DUMP ===\n");
	dqe_print_hex_dump(dqe_regs, dqe_regs + 0x1400 + SHADOW_DQE_OFFSET, 0x20);

	dqe_info("=== DQE_DEGAMMA SFR DUMP ===\n");
	dqe_print_hex_dump(dqe_regs, dqe_regs + 0x1800, 0x100);
	dqe_info("=== DQE_DEGAMMA SHADOW SFR DUMP ===\n");
	dqe_print_hex_dump(dqe_regs, dqe_regs + 0x1800 + SHADOW_DQE_OFFSET, 0x100);

	dqe_info("=== DQE_CGC SFR DUMP ===\n");
	dqe_print_hex_dump(dqe_regs, dqe_regs + 0x2000, 0x10);
	dqe_info("=== DQE_CGC SHADOW SFR DUMP ===\n");
	dqe_print_hex_dump(dqe_regs, dqe_regs + 0x2000 + SHADOW_DQE_OFFSET, 0x10);

	dqe_info("=== DQE_REGAMMA SFR DUMP ===\n");
	dqe_print_hex_dump(dqe_regs, dqe_regs + 0x2400, 0x200);
	dqe_info("=== DQE_REGAMMA SHADOW SFR DUMP ===\n");
	dqe_print_hex_dump(dqe_regs, dqe_regs + 0x2400 + SHADOW_DQE_OFFSET, 0x200);

	dqe_info("=== DQE_CGC_DITHER SFR DUMP ===\n");
	dqe_print_hex_dump(dqe_regs, dqe_regs + 0x2800, 0x20);
	dqe_info("=== DQE_CGC_DITHER SHADOW SFR DUMP ===\n");
	dqe_print_hex_dump(dqe_regs, dqe_regs + 0x2800 + SHADOW_DQE_OFFSET, 0x20);

	dqe_info("=== DQE_CGC_LUT SFR DUMP ===\n");
	dqe_info("== [ RED CGC LUT ] ==\n");
	dqe_print_hex_dump(dqe_regs, dqe_regs + 0x4000, 0x2664);
	dqe_info("== [ GREEN CGC LUT ] ==\n");
	dqe_print_hex_dump(dqe_regs, dqe_regs + 0x8000, 0x2664);
	dqe_info("== [ BLUE CGC LUT ] ==\n");
	dqe_print_hex_dump(dqe_regs, dqe_regs + 0xC000, 0x2664);
}

void dqe_reg_set_gamma_matrix_on(u32 on)
{
	dqe_write_mask(DQE0_GAMMA_MATRIX_CON, GAMMA_MATRIX_EN(on),
	                GAMMA_MATRIX_EN_MASK);
}

u32 dqe_reg_get_gamma_matrix_on(void)
{
        return dqe_read_mask(DQE0_GAMMA_MATRIX_CON, GAMMA_MATRIX_EN_MASK);
}

void dqe_reg_set_degamma_on(u32 on)
{
	dqe_write_mask(DQE0_DEGAMMA_CON, DEGAMMA_EN(on), DEGAMMA_EN_MASK);
}

u32 dqe_reg_get_degamma_on(void)
{
	return dqe_read_mask(DQE0_DEGAMMA_CON, DEGAMMA_EN_MASK);
}

void dqe_reg_set_cgc_on(u32 on)
{
	dqe_write_mask(DQE0_CGC_CON, CGC_EN(on), CGC_EN_MASK);
}

u32 dqe_reg_get_cgc_on(void)
{
	return dqe_read_mask(DQE0_CGC_CON, CGC_EN_MASK);
}

void dqe_reg_set_regamma_on(u32 on)
{
	dqe_write_mask(DQE0_REGAMMA_CON, REGAMMA_EN(on), REGAMMA_EN_MASK);
}

u32 dqe_reg_get_regamma_on(void)
{
	return dqe_read_mask(DQE0_REGAMMA_CON, REGAMMA_EN_MASK);
}

void dqe_reg_set_cgc_dither_on(u32 on)
{
	dqe_write_mask(DQE0_CGC_DITHER, DITHER_EN(on), DITHER_EN_MASK);
}

u32 dqe_reg_get_cgc_dither_on(void)
{
	return dqe_read_mask(DQE0_CGC_DITHER, DITHER_EN_MASK);
}

void dqe_reg_set_img_size(u32 id, u32 xres, u32 yres)
{
	u32 val;

	if (id != 0)
		return;

	val = DQE_FULL_IMG_VSIZE(yres) |
		DQE_FULL_IMG_HSIZE(xres);
	dqe_write(DQE0_TOP_FRM_SIZE, val);

	val = DQE_FULL_PXL_NUM(yres * xres);
	dqe_write(DQE0_TOP_FRM_PXL_NUM, val);

	val = DQE_PARTIAL_START_Y(0) | DQE_PARTIAL_START_X(0);
	dqe_write(DQE0_TOP_PARTIAL_START, val);

	val = DQE_IMG_VSIZE(yres) | DQE_IMG_HSIZE(xres);
	dqe_write(DQE0_TOP_IMG_SIZE, val);

	val = DQE_PARTIAL_SAME(0) | DQE_PARTIAL_UPDATE_EN(0);
	dqe_write(DQE0_TOP_PARTIAL_CON, val);
}

void dqe_reg_set_data_path_enable(u32 id, u32 en)
{
	u32 val;
	enum decon_data_path d_path = DPATH_DSCENC0_OUTFIFO0_DSIMIF0;
	enum decon_enhance_path e_path = ENHANCEPATH_ENHANCE_ALL_OFF;

	if (id != 0)
		return;

	val = en ? ENHANCEPATH_DQE_ON : ENHANCEPATH_ENHANCE_ALL_OFF;

	decon_reg_get_data_path(id, &d_path, &e_path);
	decon_reg_set_data_path(id, d_path, val);
}

void dqe_reg_start(u32 id, struct exynos_panel_info *lcd_info)
{
	dqe_reg_set_img_size(id, lcd_info->xres, lcd_info->yres);
	dqe_reg_set_data_path_enable(id, 1);
}

void dqe_reg_stop(u32 id)
{
	dqe_reg_set_data_path_enable(id, 0);
}

