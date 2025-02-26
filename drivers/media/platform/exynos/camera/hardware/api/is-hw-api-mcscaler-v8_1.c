// SPDX-License-Identifier: GPL-2.0
/*
 * Samsung Exynos SoC series Pablo driver
 *
 * MCSC HW control APIs
 *
 * Copyright (C) 2019 Samsung Electronics Co., Ltd.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#include "is-hw-api-mcscaler-v3.h"
#include "sfr/is-sfr-mcsc-v8_1.h"
#include "is-hw.h"
#include "is-hw-control.h"
#include "is-param.h"

const struct mcsc_v_coef v_coef_4tap[7] = {
	/* x8/8 */
	{
		{  0, -15, -25, -31, -33, -33, -31, -27, -23},
		{512, 508, 495, 473, 443, 408, 367, 324, 279},
		{  0,  20,  45,  75, 110, 148, 190, 234, 279},
		{  0,  -1,  -3,  -5,  -8, -11, -14, -19, -23},
	},
	/* x7/8 */
	{
		{ 32,  17,   3,  -7, -14, -18, -20, -20, -19},
		{448, 446, 437, 421, 399, 373, 343, 310, 275},
		{ 32,  55,  79, 107, 138, 170, 204, 240, 275},
		{  0,  -6,  -7,  -9, -11, -13, -15, -18, -19},
	},
	/* x6/8 */
	{
		{ 61,  46,  31,  19,   9,   2,  -3,  -7,  -9},
		{390, 390, 383, 371, 356, 337, 315, 291, 265},
		{ 61,  83, 106, 130, 156, 183, 210, 238, 265},
		{  0,  -7,  -8,  -8,  -9, -10, -10, -10,  -9},
	},
	/* x5/8 */
	{
		{ 85,  71,  56,  43,  32,  23,  16,   9,   5},
		{341, 341, 336, 328, 317, 304, 288, 271, 251},
		{ 86, 105, 124, 145, 166, 187, 209, 231, 251},
		{  0,  -5,  -4,  -4,  -3,  -2,  -1,   1,   5},
	},
	/* x4/8 */
	{
		{104,  89,  76,  63,  52,  42,  33,  26,  20},
		{304, 302, 298, 293, 285, 275, 264, 251, 236},
		{104, 120, 136, 153, 170, 188, 205, 221, 236},
		{  0,   1,   2,   3,   5,   7,  10,  14,  20},
	},
	/* x3/8 */
	{
		{118, 103,  90,  78,  67,  57,  48,  40,  33},
		{276, 273, 270, 266, 260, 253, 244, 234, 223},
		{118, 129, 143, 157, 171, 185, 199, 211, 223},
		{  0,   7,   9,  11,  14,  17,  21,  27,  33},
	},
	/* x2/8 */
	{
		{127, 111, 100,  88,  78,  68,  59,  50,  43},
		{258, 252, 250, 247, 242, 237, 230, 222, 213},
		{127, 135, 147, 159, 171, 182, 193, 204, 213},
		{  0,  14,  15,  18,  21,  25,  30,  36,  43},
	}
};

const struct mcsc_h_coef h_coef_8tap[7] = {
	/* x8/8 */
	{
		{  0,  -2,  -4,  -5,  -6,  -6,  -6,  -6,  -5},
		{  0,   8,  14,  20,  23,  25,  26,  25,  23},
		{  0, -25, -46, -62, -73, -80, -83, -82, -78},
		{512, 509, 499, 482, 458, 429, 395, 357, 316},
		{  0,  30,  64, 101, 142, 185, 228, 273, 316},
		{  0,  -9, -19, -30, -41, -53, -63, -71, -78},
		{  0,   2,   5,   8,  12,  15,  19,  21,  23},
		{  0,  -1,  -1,  -2,  -3,  -3,  -4,  -5,  -5},
	},
	/* x7/8 */
	{
		{ 12,   9,   7,   5,   3,   2,   1,   0,  -1},
		{-32, -24, -16,  -9,  -3,   2,   7,  10,  13},
		{ 56,  29,   6, -14, -30, -43, -53, -60, -65},
		{444, 445, 438, 426, 410, 390, 365, 338, 309},
		{ 52,  82, 112, 144, 177, 211, 244, 277, 309},
		{-32, -39, -46, -52, -58, -63, -66, -66, -65},
		{ 12,  13,  14,  15,  16,  16,  16,  15,  13},
		{  0,  -3,	-3,  -3,  -3,  -3,  -2,  -2,  -1},
	},
	/* x6/8 */
	{
		{  8,   9,   8,   8,   8,   7,   7,   5,   5},
		{-44, -40, -36, -32, -27, -22, -18, -13,  -9},
		{100,  77,  57,  38,  20,   5,  -9, -20, -30},
		{384, 382, 377, 369, 358, 344, 329, 310, 290},
		{100, 123, 147, 171, 196, 221, 245, 268, 290},
		{-44, -47, -49, -49, -48, -47, -43, -37, -30},
		{  8,   8,   7,   5,   3,   1,  -2,  -5,  -9},
		{  0,   0,   1,   2,   2,   3,   3,   4,   5},
	},
	/* x5/8 */
	{
		{ -3,  -3,  -1,   0,   1,   2,   2,   3,   3},
		{-31, -32, -33, -32, -31, -30, -28, -25, -23},
		{130, 113,  97,  81,  66,  52,  38,  26,  15},
		{320, 319, 315, 311, 304, 296, 286, 274, 261},
		{130, 147, 165, 182, 199, 216, 232, 247, 261},
		{-31, -29, -26, -22, -17, -11,  -3,   5,  15},
		{ -3,  -6,  -8, -11, -13, -16, -18, -21, -23},
		{  0,   3,   3,   3,   3,   3,   3,   3,   3},
	},
	/* x4/8 */
	{
		{-11, -10,  -9,  -8,  -7,  -6,  -5,  -5,  -4},
		{  0,  -4,  -7, -10, -12, -14, -15, -16, -17},
		{140, 129, 117, 106,  95,  85,  74,  64,  55},
		{255, 254, 253, 250, 246, 241, 236, 229, 222},
		{140, 151, 163, 174, 185, 195, 204, 214, 222},
		{  0,   5,  10,  16,  22,  29,  37,  46,  55},
		{-12, -13, -14, -15, -16, -16, -17, -17, -17},
		{  0,   0,  -1,  -1,  -1,  -2,  -2,  -3,  -4},
	},
	/* x3/8 */
	{
		{ -5,  -5,  -5,  -5,  -5,  -5,  -5,  -5,  -5},
		{ 31,  27,  23,  19,  16,  12,  10,   7,   5},
		{133, 126, 119, 112, 105,  98,  91,  84,  78},
		{195, 195, 194, 193, 191, 189, 185, 182, 178},
		{133, 139, 146, 152, 158, 163, 169, 174, 178},
		{ 31,  37,  41,  47,  53,  59,  65,  71,  78},
		{ -6,  -4,  -3,  -2,  -2,   0,   1,   3,   5},
		{  0,  -3,  -3,  -4,  -4,  -4,  -4,  -4,  -5},
	},
	/* x2/8 */
	{
		{ 10,   9,   7,   6,   5,   4,   4,   3,   2},
		{ 52,  48,  45,  41,  38,  35,  31,  29,  26},
		{118, 114, 110, 106, 102,  98,  94,  89,  85},
		{152, 152, 151, 150, 149, 148, 146, 145, 143},
		{118, 122, 125, 129, 132, 135, 138, 140, 143},
		{ 52,  56,  60,  64,  68,  72,  77,  81,  85},
		{ 10,  11,  13,  15,  17,  19,  21,  23,  26},
		{  0,   0,   1,   1,   1,   1,   1,   2,   2},
	}
};

void is_scaler_start(void __iomem *base_addr, u32 hw_id)
{
	/* Qactive must set to "1" before scaler enable */
	is_hw_set_field(base_addr, &mcsc_regs[MCSC_R_APB_CLK_GATE_CTRL], &mcsc_fields[MCSC_F_QACTIVE_ENABLE], 1);

	switch (hw_id) {
	case DEV_HW_MCSC0:
		is_hw_set_field(base_addr, &mcsc_regs[MCSC_R_SC_GCTRL_0], &mcsc_fields[MCSC_F_SCALER_ENABLE_0], 1);
		break;
	default:
		warn_hw("invalid hw_id(%d) for MCSC api\n", hw_id);
		break;
	}
}

void is_scaler_stop(void __iomem *base_addr, u32 hw_id)
{
	switch (hw_id) {
	case DEV_HW_MCSC0:
		is_hw_set_field(base_addr, &mcsc_regs[MCSC_R_SC_GCTRL_0], &mcsc_fields[MCSC_F_SCALER_ENABLE_0], 0);
		break;
	default:
		warn_hw("invalid hw_id(%d) for MCSC api\n", hw_id);
		break;
	}

#if defined(ENABLE_HWACG_CONTROL)
	/* Qactive must set to "0" for ip clock gating */
	if (!is_hw_get_reg(base_addr, &mcsc_regs[MCSC_R_SC_GCTRL_0])
		&& is_hw_get_field(base_addr,
			&mcsc_regs[MCSC_R_SCALER_RUNNING_STATUS], &mcsc_fields[MCSC_F_SCALER_IDLE_0]))
		is_hw_set_field(base_addr,
			&mcsc_regs[MCSC_R_APB_CLK_GATE_CTRL], &mcsc_fields[MCSC_F_QACTIVE_ENABLE], 0);
#endif
}

#define IS_RESET_DONE(addr, reg, field)	\
	(is_hw_get_field(addr, &mcsc_regs[reg], &mcsc_fields[field]) != 0)

static u32 is_scaler_sw_reset_global(void __iomem *base_addr)
{
	u32 reset_count = 0;

	/* request scaler reset */
	is_hw_set_field(base_addr, &mcsc_regs[MCSC_R_SC_RESET_CTRL_GLOBAL],
		&mcsc_fields[MCSC_F_SW_RESET_GLOBAL], 1);

	/* wait reset complete */
	do {
		reset_count++;
		if (reset_count > 10000)
			return reset_count;
	} while (IS_RESET_DONE(base_addr, MCSC_R_SCALER_RESET_STATUS, MCSC_F_SW_RESET_GLOBAL_STATUS));

	return 0;
}

static u32 is_scaler0_sw_reset(void __iomem *base_addr, u32 partial)
{
	u32 reset_count = 0;

	/* request scaler reset */
	is_hw_set_field(base_addr, &mcsc_regs[MCSC_R_SC_RESET_CTRL_0], &mcsc_fields[MCSC_F_SW_RESET_0], 1);

	/* wait reset complete */
	do {
		reset_count++;
		if (reset_count > 10000)
			return reset_count;
	} while (IS_RESET_DONE(base_addr, MCSC_R_SCALER_RESET_STATUS, MCSC_F_SW_RESET_0_STATUS));

	return 0;
}

u32 is_scaler_sw_reset(void __iomem *base_addr, u32 hw_id, u32 global, u32 partial)
{
	int ret = 0;

	if (global) {
		ret = is_scaler_sw_reset_global(base_addr);
		return ret;
	}

	switch (hw_id) {
	case DEV_HW_MCSC0:
		ret = is_scaler0_sw_reset(base_addr, partial);
		break;
	default:
		warn_hw("invalid hw_id(%d) for MCSC api\n", hw_id);
		break;
	}

	return ret;
}

void is_scaler_clear_intr_all(void __iomem *base_addr, u32 hw_id)
{
	u32 reg_val = 0;

	switch (hw_id) {
	case DEV_HW_MCSC0:
		reg_val = is_hw_set_field_value(reg_val,
			&mcsc_fields[MCSC_F_SHADOW_COPY_FINISH_OVER_INT_0], 1);
		reg_val = is_hw_set_field_value(reg_val, &mcsc_fields[MCSC_F_SHADOW_COPY_FINISH_INT_0], 1);
		reg_val = is_hw_set_field_value(reg_val, &mcsc_fields[MCSC_F_SCALER_OVERFLOW_INT_0], 1);
		reg_val = is_hw_set_field_value(reg_val, &mcsc_fields[MCSC_F_INPUT_VERTICAL_UNF_INT_0], 1);
		reg_val = is_hw_set_field_value(reg_val, &mcsc_fields[MCSC_F_INPUT_VERTICAL_OVF_INT_0], 1);
		reg_val = is_hw_set_field_value(reg_val, &mcsc_fields[MCSC_F_INPUT_HORIZONTAL_UNF_INT_0], 1);
		reg_val = is_hw_set_field_value(reg_val, &mcsc_fields[MCSC_F_INPUT_HORIZONTAL_OVF_INT_0], 1);
		reg_val = is_hw_set_field_value(reg_val, &mcsc_fields[MCSC_F_CORE_FINISH_INT_0], 1);
		reg_val = is_hw_set_field_value(reg_val, &mcsc_fields[MCSC_F_WDMA_FINISH_INT_0], 1);
		reg_val = is_hw_set_field_value(reg_val, &mcsc_fields[MCSC_F_FRAME_START_INT_0], 1);
		reg_val = is_hw_set_field_value(reg_val, &mcsc_fields[MCSC_F_FRAME_END_INT_0], 1);

		is_hw_set_reg(base_addr, &mcsc_regs[MCSC_R_SCALER_INTERRUPT_0], reg_val);
		break;
	default:
		warn_hw("invalid hw_id(%d) for MCSC api\n", hw_id);
		break;
	}
}

void is_scaler_disable_intr(void __iomem *base_addr, u32 hw_id)
{
	u32 reg_val = 0;

	switch (hw_id) {
	case DEV_HW_MCSC0:
		reg_val = is_hw_set_field_value(reg_val,
			&mcsc_fields[MCSC_F_SHADOW_COPY_FINISH_OVER_INT_MASK_0], 1);
		reg_val = is_hw_set_field_value(reg_val,
			&mcsc_fields[MCSC_F_SHADOW_COPY_FINISH_INT_MASK_0], 1);
		reg_val = is_hw_set_field_value(reg_val,
			&mcsc_fields[MCSC_F_SCALER_OVERFLOW_INT_MASK_0], 1);
		reg_val = is_hw_set_field_value(reg_val,
			&mcsc_fields[MCSC_F_INPUT_VERTICAL_UNF_INT_MASK_0], 1);
		reg_val = is_hw_set_field_value(reg_val,
			&mcsc_fields[MCSC_F_INPUT_VERTICAL_OVF_INT_MASK_0], 1);
		reg_val = is_hw_set_field_value(reg_val,
			&mcsc_fields[MCSC_F_INPUT_HORIZONTAL_UNF_INT_MASK_0], 1);
		reg_val = is_hw_set_field_value(reg_val,
			&mcsc_fields[MCSC_F_INPUT_HORIZONTAL_OVF_INT_MASK_0], 1);
		reg_val = is_hw_set_field_value(reg_val, &mcsc_fields[MCSC_F_CORE_FINISH_INT_MASK_0], 1);
		reg_val = is_hw_set_field_value(reg_val, &mcsc_fields[MCSC_F_WDMA_FINISH_INT_MASK_0], 1);
		reg_val = is_hw_set_field_value(reg_val, &mcsc_fields[MCSC_F_FRAME_START_INT_MASK_0], 1);
		reg_val = is_hw_set_field_value(reg_val, &mcsc_fields[MCSC_F_FRAME_END_INT_MASK_0], 1);

		is_hw_set_reg(base_addr, &mcsc_regs[MCSC_R_SCALER_INTERRUPT_MASK_0], reg_val);
		break;
	default:
		warn_hw("invalid hw_id(%d) for MCSC api\n", hw_id);
		break;
	}
}

void is_scaler_mask_intr(void __iomem *base_addr, u32 hw_id, u32 intr_mask)
{
	switch (hw_id) {
	case DEV_HW_MCSC0:
		is_hw_set_reg(base_addr, &mcsc_regs[MCSC_R_SCALER_INTERRUPT_MASK_0], intr_mask);
		break;
	default:
		warn_hw("invalid hw_id(%d) for MCSC api\n", hw_id);
		break;
	}
}

void is_scaler_set_stop_req_post_en_ctrl(void __iomem *base_addr, u32 hw_id, u32 value)
{
	/* not support */
}

static void is_scaler0_set_shadow_ctrl(void __iomem *base_addr, enum mcsc_shadow_ctrl ctrl)
{
	u32 reg_val = 0;

	switch (ctrl) {
	case SHADOW_WRITE_START:
		reg_val = is_hw_set_field_value(reg_val, &mcsc_fields[MCSC_F_SHADOW_WR_START_0], 1);
		reg_val = is_hw_set_field_value(reg_val, &mcsc_fields[MCSC_F_SHADOW_WR_FINISH_0], 0);
		is_hw_set_reg(base_addr, &mcsc_regs[MCSC_R_SHADOW_REG_CTRL_0], reg_val);
		break;
	case SHADOW_WRITE_FINISH:
		reg_val = is_hw_set_field_value(reg_val, &mcsc_fields[MCSC_F_SHADOW_WR_START_0], 0);
		reg_val = is_hw_set_field_value(reg_val, &mcsc_fields[MCSC_F_SHADOW_WR_FINISH_0], 1);
		is_hw_set_reg(base_addr, &mcsc_regs[MCSC_R_SHADOW_REG_CTRL_0], reg_val);
		break;
	default:
		break;
	}
}

void is_scaler_set_shadow_ctrl(void __iomem *base_addr, u32 hw_id, enum mcsc_shadow_ctrl ctrl)
{
	switch (hw_id) {
	case DEV_HW_MCSC0:
		is_scaler0_set_shadow_ctrl(base_addr, ctrl);
		break;
	default:
		break;
	}
}

void is_scaler_clear_shadow_ctrl(void __iomem *base_addr, u32 hw_id)
{
	switch (hw_id) {
	case DEV_HW_MCSC0:
		is_hw_set_reg(base_addr, &mcsc_regs[MCSC_R_SHADOW_REG_CTRL_0], 0x0);
		break;
	default:
		break;
	}
}

void is_scaler_get_input_status(void __iomem *base_addr, u32 hw_id, u32 *hl, u32 *vl)
{
	switch (hw_id) {
	case DEV_HW_MCSC0:
		*hl = is_hw_get_field(base_addr, &mcsc_regs[MCSC_R_SCALER_INPUT_STATUS_0],
			&mcsc_fields[MCSC_F_CUR_HORIZONTAL_CNT_0]);
		*vl = is_hw_get_field(base_addr, &mcsc_regs[MCSC_R_SCALER_INPUT_STATUS_0],
			&mcsc_fields[MCSC_F_CUR_VERTICAL_CNT_0]);
		break;
	default:
		warn_hw("invalid hw_id(%d) for MCSC api\n", hw_id);
		break;
	}
}

void is_scaler_set_input_source(void __iomem *base_addr, u32 hw_id, u32 rdma)
{
	/*  0: otf input_0, 1: otf input_1, 2: rdma input */
	switch (hw_id) {
	case DEV_HW_MCSC0:
		break;
	default:
		warn_hw("invalid hw_id(%d) for MCSC api\n", hw_id);
		break;
	}
}

u32 is_scaler_get_input_source(void __iomem *base_addr, u32 hw_id)
{
	/* not support */
	return 0;
}

void is_scaler_set_dither(void __iomem *base_addr, u32 hw_id, bool dither_en)
{
	/* not support */
}

void is_scaler_set_input_img_size(void __iomem *base_addr, u32 hw_id, u32 width, u32 height)
{
	u32 reg_val = 0;

	if (width % MCSC_WIDTH_ALIGN != 0)
		err_hw("INPUT_IMG_HSIZE_%d(%d) should be multiple of 4", hw_id, width);

	switch (hw_id) {
	case DEV_HW_MCSC0:
		reg_val = is_hw_set_field_value(reg_val, &mcsc_fields[MCSC_F_INPUT_IMG_HSIZE_0], width);
		reg_val = is_hw_set_field_value(reg_val, &mcsc_fields[MCSC_F_INPUT_IMG_VSIZE_0], height);
		is_hw_set_reg(base_addr, &mcsc_regs[MCSC_R_INPUT_IMG_SIZE_0], reg_val);
		break;
	default:
		warn_hw("invalid hw_id(%d) for MCSC api\n", hw_id);
		break;
	}

}

void is_scaler_get_input_img_size(void __iomem *base_addr, u32 hw_id, u32 *width, u32 *height)
{
	switch (hw_id) {
	case DEV_HW_MCSC0:
		*width = is_hw_get_field(base_addr, &mcsc_regs[MCSC_R_INPUT_IMG_SIZE_0],
			&mcsc_fields[MCSC_F_INPUT_IMG_HSIZE_0]);
		*height = is_hw_get_field(base_addr, &mcsc_regs[MCSC_R_INPUT_IMG_SIZE_0],
			&mcsc_fields[MCSC_F_INPUT_IMG_VSIZE_0]);
		break;
	default:
		warn_hw("invalid hw_id(%d) for MCSC api\n", hw_id);
		break;
	}

}

u32 is_scaler_get_scaler_path(void __iomem *base_addr, u32 hw_id, u32 output_id)
{
	u32 input = 0, enable_poly = 0, enable_post = 0, enable_dma = 0;

	switch (output_id) {
	case MCSC_OUTPUT0:
		enable_poly = is_hw_get_field(base_addr,
			&mcsc_regs[MCSC_R_SC0_CTRL], &mcsc_fields[MCSC_F_SC0_ENABLE]);
		enable_post = is_hw_get_field(base_addr,
			&mcsc_regs[MCSC_R_PC0_CTRL], &mcsc_fields[MCSC_F_PC0_ENABLE]);
		enable_dma = is_hw_get_field(base_addr,
			&mcsc_regs[MCSC_R_PC0_DMA_OUT_CTRL], &mcsc_fields[MCSC_F_PC0_DMA_OUT_ENABLE]);
		break;
	case MCSC_OUTPUT1:
		enable_poly = is_hw_get_field(base_addr,
			&mcsc_regs[MCSC_R_SC1_CTRL], &mcsc_fields[MCSC_F_SC1_ENABLE]);
		enable_post = is_hw_get_field(base_addr,
			&mcsc_regs[MCSC_R_PC1_CTRL], &mcsc_fields[MCSC_F_PC1_ENABLE]);
		enable_dma = is_hw_get_field(base_addr,
			&mcsc_regs[MCSC_R_PC1_DMA_OUT_CTRL], &mcsc_fields[MCSC_F_PC1_DMA_OUT_ENABLE]);
		break;
	case MCSC_OUTPUT2:
		enable_poly = is_hw_get_field(base_addr,
			&mcsc_regs[MCSC_R_SC2_CTRL], &mcsc_fields[MCSC_F_SC2_ENABLE]);
		enable_post = is_hw_get_field(base_addr,
			&mcsc_regs[MCSC_R_PC2_CTRL], &mcsc_fields[MCSC_F_PC2_ENABLE]);
		enable_dma = is_hw_get_field(base_addr,
			&mcsc_regs[MCSC_R_PC2_DMA_OUT_CTRL], &mcsc_fields[MCSC_F_PC2_DMA_OUT_ENABLE]);
		break;
	case MCSC_OUTPUT3:
		enable_poly = is_hw_get_field(base_addr,
			&mcsc_regs[MCSC_R_SC3_CTRL], &mcsc_fields[MCSC_F_SC3_ENABLE]);
		enable_post = 0;
		enable_dma = is_hw_get_field(base_addr,
			&mcsc_regs[MCSC_R_PC3_DMA_OUT_CTRL], &mcsc_fields[MCSC_F_PC3_DMA_OUT_ENABLE]);
		break;
	case MCSC_OUTPUT4:
		enable_poly = is_hw_get_field(base_addr,
			&mcsc_regs[MCSC_R_SC4_CTRL], &mcsc_fields[MCSC_F_SC4_ENABLE]);
		enable_post = 0;
		enable_dma = is_hw_get_field(base_addr,
			&mcsc_regs[MCSC_R_PC4_DMA_OUT_CTRL], &mcsc_fields[MCSC_F_PC4_DMA_OUT_ENABLE]);
		break;
	default:
		break;
	}
	dbg_hw(2, "[ID:%d]%s:[SRC:%d]->[OUT:%d], en(poly:%d, post:%d), dma(%d)\n",
		hw_id, __func__, input, output_id, enable_poly, enable_post, enable_dma);

	return (DEV_HW_MCSC0 + input);
}

void is_scaler_set_poly_scaler_enable(void __iomem *base_addr, u32 hw_id, u32 output_id, u32 enable)
{
	switch (output_id) {
	case MCSC_OUTPUT0:
		is_hw_set_field(base_addr, &mcsc_regs[MCSC_R_SC0_CTRL], &mcsc_fields[MCSC_F_SC0_ENABLE], enable);
		break;
	case MCSC_OUTPUT1:
		is_hw_set_field(base_addr, &mcsc_regs[MCSC_R_SC1_CTRL], &mcsc_fields[MCSC_F_SC1_ENABLE], enable);
		break;
	case MCSC_OUTPUT2:
		is_hw_set_field(base_addr, &mcsc_regs[MCSC_R_SC2_CTRL], &mcsc_fields[MCSC_F_SC2_ENABLE], enable);
		break;
	case MCSC_OUTPUT3:
		is_hw_set_field(base_addr, &mcsc_regs[MCSC_R_SC3_CTRL], &mcsc_fields[MCSC_F_SC3_ENABLE], enable);
		break;
	case MCSC_OUTPUT4:
		is_hw_set_field(base_addr, &mcsc_regs[MCSC_R_SC4_CTRL], &mcsc_fields[MCSC_F_SC4_ENABLE], enable);
		break;
	default:
		break;
	}
}

void is_scaler_set_poly_scaler_bypass(void __iomem *base_addr, u32 output_id, u32 bypass)
{
	switch (output_id) {
	case MCSC_OUTPUT0:
		is_hw_set_field(base_addr, &mcsc_regs[MCSC_R_SC0_CTRL], &mcsc_fields[MCSC_F_SC0_BYPASS], bypass);
		break;
	case MCSC_OUTPUT1:
		is_hw_set_field(base_addr, &mcsc_regs[MCSC_R_SC1_CTRL], &mcsc_fields[MCSC_F_SC1_BYPASS], bypass);
		break;
	case MCSC_OUTPUT2:
		is_hw_set_field(base_addr, &mcsc_regs[MCSC_R_SC2_CTRL], &mcsc_fields[MCSC_F_SC2_BYPASS], bypass);
		break;
	case MCSC_OUTPUT3:
		is_hw_set_field(base_addr, &mcsc_regs[MCSC_R_SC3_CTRL], &mcsc_fields[MCSC_F_SC3_BYPASS], bypass);
		break;
	case MCSC_OUTPUT4:
		is_hw_set_field(base_addr, &mcsc_regs[MCSC_R_SC4_CTRL], &mcsc_fields[MCSC_F_SC4_BYPASS], bypass);
		break;
	default:
		break;
	}
}

void is_scaler_set_poly_src_size(void __iomem *base_addr, u32 output_id,
	u32 pos_x, u32 pos_y, u32 width, u32 height)
{
	u32 reg_val = 0;

	switch (output_id) {
	case MCSC_OUTPUT0:
		reg_val = is_hw_set_field_value(reg_val, &mcsc_fields[MCSC_F_SC0_SRC_HPOS], pos_x);
		reg_val = is_hw_set_field_value(reg_val, &mcsc_fields[MCSC_F_SC0_SRC_VPOS], pos_y);
		is_hw_set_reg(base_addr, &mcsc_regs[MCSC_R_SC0_SRC_POS], reg_val);

		reg_val = 0;
		reg_val = is_hw_set_field_value(reg_val, &mcsc_fields[MCSC_F_SC0_SRC_HSIZE], width);
		reg_val = is_hw_set_field_value(reg_val, &mcsc_fields[MCSC_F_SC0_SRC_VSIZE], height);
		is_hw_set_reg(base_addr, &mcsc_regs[MCSC_R_SC0_SRC_SIZE], reg_val);
		break;
	case MCSC_OUTPUT1:
		reg_val = is_hw_set_field_value(reg_val, &mcsc_fields[MCSC_F_SC1_SRC_HPOS], pos_x);
		reg_val = is_hw_set_field_value(reg_val, &mcsc_fields[MCSC_F_SC1_SRC_VPOS], pos_y);
		is_hw_set_reg(base_addr, &mcsc_regs[MCSC_R_SC1_SRC_POS], reg_val);

		reg_val = 0;
		reg_val = is_hw_set_field_value(reg_val, &mcsc_fields[MCSC_F_SC1_SRC_HSIZE], width);
		reg_val = is_hw_set_field_value(reg_val, &mcsc_fields[MCSC_F_SC1_SRC_VSIZE], height);
		is_hw_set_reg(base_addr, &mcsc_regs[MCSC_R_SC1_SRC_SIZE], reg_val);
		break;
	case MCSC_OUTPUT2:
		reg_val = is_hw_set_field_value(reg_val, &mcsc_fields[MCSC_F_SC2_SRC_HPOS], pos_x);
		reg_val = is_hw_set_field_value(reg_val, &mcsc_fields[MCSC_F_SC2_SRC_VPOS], pos_y);
		is_hw_set_reg(base_addr, &mcsc_regs[MCSC_R_SC2_SRC_POS], reg_val);

		reg_val = 0;
		reg_val = is_hw_set_field_value(reg_val, &mcsc_fields[MCSC_F_SC2_SRC_HSIZE], width);
		reg_val = is_hw_set_field_value(reg_val, &mcsc_fields[MCSC_F_SC2_SRC_VSIZE], height);
		is_hw_set_reg(base_addr, &mcsc_regs[MCSC_R_SC2_SRC_SIZE], reg_val);
		break;
	case MCSC_OUTPUT3:
		reg_val = is_hw_set_field_value(reg_val, &mcsc_fields[MCSC_F_SC3_SRC_HPOS], pos_x);
		reg_val = is_hw_set_field_value(reg_val, &mcsc_fields[MCSC_F_SC3_SRC_VPOS], pos_y);
		is_hw_set_reg(base_addr, &mcsc_regs[MCSC_R_SC3_SRC_POS], reg_val);

		reg_val = 0;
		reg_val = is_hw_set_field_value(reg_val, &mcsc_fields[MCSC_F_SC3_SRC_HSIZE], width);
		reg_val = is_hw_set_field_value(reg_val, &mcsc_fields[MCSC_F_SC3_SRC_VSIZE], height);
		is_hw_set_reg(base_addr, &mcsc_regs[MCSC_R_SC3_SRC_SIZE], reg_val);
		break;
	case MCSC_OUTPUT4:
		reg_val = is_hw_set_field_value(reg_val, &mcsc_fields[MCSC_F_SC4_SRC_HPOS], pos_x);
		reg_val = is_hw_set_field_value(reg_val, &mcsc_fields[MCSC_F_SC4_SRC_VPOS], pos_y);
		is_hw_set_reg(base_addr, &mcsc_regs[MCSC_R_SC4_SRC_POS], reg_val);

		reg_val = 0;
		reg_val = is_hw_set_field_value(reg_val, &mcsc_fields[MCSC_F_SC4_SRC_HSIZE], width);
		reg_val = is_hw_set_field_value(reg_val, &mcsc_fields[MCSC_F_SC4_SRC_VSIZE], height);
		is_hw_set_reg(base_addr, &mcsc_regs[MCSC_R_SC4_SRC_SIZE], reg_val);
		break;
	default:
		break;
	}
}

void is_scaler_get_poly_src_size(void __iomem *base_addr, u32 output_id, u32 *width, u32 *height)
{
	switch (output_id) {
	case MCSC_OUTPUT0:
		*width = is_hw_get_field(base_addr, &mcsc_regs[MCSC_R_SC0_SRC_SIZE],
			&mcsc_fields[MCSC_F_SC0_SRC_HSIZE]);
		*height = is_hw_get_field(base_addr, &mcsc_regs[MCSC_R_SC0_SRC_SIZE],
			&mcsc_fields[MCSC_F_SC0_SRC_VSIZE]);
		break;
	case MCSC_OUTPUT1:
		*width = is_hw_get_field(base_addr, &mcsc_regs[MCSC_R_SC1_SRC_SIZE],
			&mcsc_fields[MCSC_F_SC1_SRC_HSIZE]);
		*height = is_hw_get_field(base_addr, &mcsc_regs[MCSC_R_SC1_SRC_SIZE],
			&mcsc_fields[MCSC_F_SC1_SRC_VSIZE]);
		break;
	case MCSC_OUTPUT2:
		*width = is_hw_get_field(base_addr, &mcsc_regs[MCSC_R_SC2_SRC_SIZE],
			&mcsc_fields[MCSC_F_SC2_SRC_HSIZE]);
		*height = is_hw_get_field(base_addr, &mcsc_regs[MCSC_R_SC2_SRC_SIZE],
			&mcsc_fields[MCSC_F_SC2_SRC_VSIZE]);
		break;
	case MCSC_OUTPUT3:
		*width = is_hw_get_field(base_addr, &mcsc_regs[MCSC_R_SC3_SRC_SIZE],
			&mcsc_fields[MCSC_F_SC3_SRC_HSIZE]);
		*height = is_hw_get_field(base_addr, &mcsc_regs[MCSC_R_SC3_SRC_SIZE],
			&mcsc_fields[MCSC_F_SC3_SRC_VSIZE]);
		break;
	case MCSC_OUTPUT4:
		*width = is_hw_get_field(base_addr, &mcsc_regs[MCSC_R_SC4_SRC_SIZE],
			&mcsc_fields[MCSC_F_SC4_SRC_HSIZE]);
		*height = is_hw_get_field(base_addr, &mcsc_regs[MCSC_R_SC4_SRC_SIZE],
			&mcsc_fields[MCSC_F_SC4_SRC_VSIZE]);
		break;
	default:
		break;
	}
}

void is_scaler_set_poly_dst_size(void __iomem *base_addr, u32 output_id, u32 width, u32 height)
{
	u32 reg_val = 0;

	switch (output_id) {
	case MCSC_OUTPUT0:
		reg_val = is_hw_set_field_value(reg_val, &mcsc_fields[MCSC_F_SC0_DST_HSIZE], width);
		reg_val = is_hw_set_field_value(reg_val, &mcsc_fields[MCSC_F_SC0_DST_VSIZE], height);
		is_hw_set_reg(base_addr, &mcsc_regs[MCSC_R_SC0_DST_SIZE], reg_val);
		break;
	case MCSC_OUTPUT1:
		reg_val = is_hw_set_field_value(reg_val, &mcsc_fields[MCSC_F_SC1_DST_HSIZE], width);
		reg_val = is_hw_set_field_value(reg_val, &mcsc_fields[MCSC_F_SC1_DST_VSIZE], height);
		is_hw_set_reg(base_addr, &mcsc_regs[MCSC_R_SC1_DST_SIZE], reg_val);
		break;
	case MCSC_OUTPUT2:
		reg_val = is_hw_set_field_value(reg_val, &mcsc_fields[MCSC_F_SC2_DST_HSIZE], width);
		reg_val = is_hw_set_field_value(reg_val, &mcsc_fields[MCSC_F_SC2_DST_VSIZE], height);
		is_hw_set_reg(base_addr, &mcsc_regs[MCSC_R_SC2_DST_SIZE], reg_val);
		break;
	case MCSC_OUTPUT3:
		reg_val = is_hw_set_field_value(reg_val, &mcsc_fields[MCSC_F_SC3_DST_HSIZE], width);
		reg_val = is_hw_set_field_value(reg_val, &mcsc_fields[MCSC_F_SC3_DST_VSIZE], height);
		is_hw_set_reg(base_addr, &mcsc_regs[MCSC_R_SC3_DST_SIZE], reg_val);
		break;
	case MCSC_OUTPUT4:
		reg_val = is_hw_set_field_value(reg_val, &mcsc_fields[MCSC_F_SC4_DST_HSIZE], width);
		reg_val = is_hw_set_field_value(reg_val, &mcsc_fields[MCSC_F_SC4_DST_VSIZE], height);
		is_hw_set_reg(base_addr, &mcsc_regs[MCSC_R_SC4_DST_SIZE], reg_val);
		break;
	default:
		break;
	}
}

void is_scaler_get_poly_dst_size(void __iomem *base_addr, u32 output_id, u32 *width, u32 *height)
{
	switch (output_id) {
	case MCSC_OUTPUT0:
		*width = is_hw_get_field(base_addr, &mcsc_regs[MCSC_R_SC0_DST_SIZE],
			&mcsc_fields[MCSC_F_SC0_DST_HSIZE]);
		*height = is_hw_get_field(base_addr, &mcsc_regs[MCSC_R_SC0_DST_SIZE],
			&mcsc_fields[MCSC_F_SC0_DST_VSIZE]);
		break;
	case MCSC_OUTPUT1:
		*width = is_hw_get_field(base_addr, &mcsc_regs[MCSC_R_SC1_DST_SIZE],
			&mcsc_fields[MCSC_F_SC1_DST_HSIZE]);
		*height = is_hw_get_field(base_addr, &mcsc_regs[MCSC_R_SC1_DST_SIZE],
			&mcsc_fields[MCSC_F_SC1_DST_VSIZE]);
		break;
	case MCSC_OUTPUT2:
		*width = is_hw_get_field(base_addr, &mcsc_regs[MCSC_R_SC2_DST_SIZE],
			&mcsc_fields[MCSC_F_SC2_DST_HSIZE]);
		*height = is_hw_get_field(base_addr, &mcsc_regs[MCSC_R_SC2_DST_SIZE],
			&mcsc_fields[MCSC_F_SC2_DST_VSIZE]);
		break;
	case MCSC_OUTPUT3:
		*width = is_hw_get_field(base_addr, &mcsc_regs[MCSC_R_SC3_DST_SIZE],
			&mcsc_fields[MCSC_F_SC3_DST_HSIZE]);
		*height = is_hw_get_field(base_addr, &mcsc_regs[MCSC_R_SC3_DST_SIZE],
			&mcsc_fields[MCSC_F_SC3_DST_VSIZE]);
		break;
	case MCSC_OUTPUT4:
		*width = is_hw_get_field(base_addr, &mcsc_regs[MCSC_R_SC4_DST_SIZE],
			&mcsc_fields[MCSC_F_SC4_DST_HSIZE]);
		*height = is_hw_get_field(base_addr, &mcsc_regs[MCSC_R_SC4_DST_SIZE],
			&mcsc_fields[MCSC_F_SC4_DST_VSIZE]);
		break;
	default:
		break;
	}
}

void is_scaler_set_poly_scaling_ratio(void __iomem *base_addr, u32 output_id, u32 hratio, u32 vratio)
{
	switch (output_id) {
	case MCSC_OUTPUT0:
		is_hw_set_field(base_addr, &mcsc_regs[MCSC_R_SC0_H_RATIO],
			&mcsc_fields[MCSC_F_SC0_H_RATIO], hratio);
		is_hw_set_field(base_addr, &mcsc_regs[MCSC_R_SC0_V_RATIO],
			&mcsc_fields[MCSC_F_SC0_V_RATIO], vratio);
		break;
	case MCSC_OUTPUT1:
		is_hw_set_field(base_addr, &mcsc_regs[MCSC_R_SC1_H_RATIO],
			&mcsc_fields[MCSC_F_SC1_H_RATIO], hratio);
		is_hw_set_field(base_addr, &mcsc_regs[MCSC_R_SC1_V_RATIO],
			&mcsc_fields[MCSC_F_SC1_V_RATIO], vratio);
		break;
	case MCSC_OUTPUT2:
		is_hw_set_field(base_addr, &mcsc_regs[MCSC_R_SC2_H_RATIO],
			&mcsc_fields[MCSC_F_SC2_H_RATIO], hratio);
		is_hw_set_field(base_addr, &mcsc_regs[MCSC_R_SC2_V_RATIO],
			&mcsc_fields[MCSC_F_SC2_V_RATIO], vratio);
		break;
	case MCSC_OUTPUT3:
		is_hw_set_field(base_addr, &mcsc_regs[MCSC_R_SC3_H_RATIO],
			&mcsc_fields[MCSC_F_SC3_H_RATIO], hratio);
		is_hw_set_field(base_addr, &mcsc_regs[MCSC_R_SC3_V_RATIO],
			&mcsc_fields[MCSC_F_SC3_V_RATIO], vratio);
		break;
	case MCSC_OUTPUT4:
		is_hw_set_field(base_addr, &mcsc_regs[MCSC_R_SC4_H_RATIO],
			&mcsc_fields[MCSC_F_SC4_H_RATIO], hratio);
		is_hw_set_field(base_addr, &mcsc_regs[MCSC_R_SC4_V_RATIO],
			&mcsc_fields[MCSC_F_SC4_V_RATIO], vratio);
		break;
	default:
		break;
	}
}

void is_scaler_set_h_init_phase_offset(void __iomem *base_addr, u32 output_id, u32 h_offset)
{
	switch (output_id) {
	case MCSC_OUTPUT0:
		is_hw_set_field(base_addr, &mcsc_regs[MCSC_R_SC0_H_INIT_PHASE_OFFSET],
			&mcsc_fields[MCSC_F_SC0_H_INIT_PHASE_OFFSET], h_offset);
		break;
	case MCSC_OUTPUT1:
		is_hw_set_field(base_addr, &mcsc_regs[MCSC_R_SC1_H_INIT_PHASE_OFFSET],
			&mcsc_fields[MCSC_F_SC1_H_INIT_PHASE_OFFSET], h_offset);
		break;
	case MCSC_OUTPUT2:
		is_hw_set_field(base_addr, &mcsc_regs[MCSC_R_SC2_H_INIT_PHASE_OFFSET],
			&mcsc_fields[MCSC_F_SC2_H_INIT_PHASE_OFFSET], h_offset);
		break;
	case MCSC_OUTPUT3:
		is_hw_set_field(base_addr, &mcsc_regs[MCSC_R_SC3_H_INIT_PHASE_OFFSET],
			&mcsc_fields[MCSC_F_SC3_H_INIT_PHASE_OFFSET], h_offset);
		break;
	case MCSC_OUTPUT4:
		is_hw_set_field(base_addr, &mcsc_regs[MCSC_R_SC4_H_INIT_PHASE_OFFSET],
			&mcsc_fields[MCSC_F_SC4_H_INIT_PHASE_OFFSET], h_offset);
		break;
	default:
		break;
	}
}

void is_scaler_set_v_init_phase_offset(void __iomem *base_addr, u32 output_id, u32 v_offset)
{
	switch (output_id) {
	case MCSC_OUTPUT0:
		is_hw_set_field(base_addr, &mcsc_regs[MCSC_R_SC0_V_INIT_PHASE_OFFSET],
			&mcsc_fields[MCSC_F_SC0_V_INIT_PHASE_OFFSET], v_offset);
		break;
	case MCSC_OUTPUT1:
		is_hw_set_field(base_addr, &mcsc_regs[MCSC_R_SC1_V_INIT_PHASE_OFFSET],
			&mcsc_fields[MCSC_F_SC1_V_INIT_PHASE_OFFSET], v_offset);
		break;
	case MCSC_OUTPUT2:
		is_hw_set_field(base_addr, &mcsc_regs[MCSC_R_SC2_V_INIT_PHASE_OFFSET],
			&mcsc_fields[MCSC_F_SC2_V_INIT_PHASE_OFFSET], v_offset);
		break;
	case MCSC_OUTPUT3:
		is_hw_set_field(base_addr, &mcsc_regs[MCSC_R_SC3_V_INIT_PHASE_OFFSET],
			&mcsc_fields[MCSC_F_SC3_V_INIT_PHASE_OFFSET], v_offset);
		break;
	case MCSC_OUTPUT4:
		is_hw_set_field(base_addr, &mcsc_regs[MCSC_R_SC4_V_INIT_PHASE_OFFSET],
			&mcsc_fields[MCSC_F_SC4_V_INIT_PHASE_OFFSET], v_offset);
		break;
	default:
		break;
	}
}

void is_scaler_set_poly_scaler_h_coef(void __iomem *base_addr, u32 output_id, u32 h_sel)
{
	int index;
	u32 reg_val = 0;

	switch (output_id) {
	case MCSC_OUTPUT0:
		for (index = 0; index <= 8; index++) {
			reg_val = 0;
			reg_val = is_hw_set_field_value(reg_val,
				&mcsc_fields[MCSC_F_SC0_H_COEFF_0A + (8 * index)], h_coef_8tap[h_sel].h_coef_a[index]);
			reg_val = is_hw_set_field_value(reg_val,
				&mcsc_fields[MCSC_F_SC0_H_COEFF_0B + (8 * index)], h_coef_8tap[h_sel].h_coef_b[index]);
			is_hw_set_reg(base_addr, &mcsc_regs[MCSC_R_SC0_H_COEFF_0AB + (4 * index)], reg_val);
			reg_val = 0;
			reg_val = is_hw_set_field_value(reg_val,
				&mcsc_fields[MCSC_F_SC0_H_COEFF_0C + (8 * index)], h_coef_8tap[h_sel].h_coef_c[index]);
			reg_val = is_hw_set_field_value(reg_val,
				&mcsc_fields[MCSC_F_SC0_H_COEFF_0D + (8 * index)], h_coef_8tap[h_sel].h_coef_d[index]);
			is_hw_set_reg(base_addr, &mcsc_regs[MCSC_R_SC0_H_COEFF_0CD + (4 * index)], reg_val);
			reg_val = 0;
			reg_val = is_hw_set_field_value(reg_val,
				&mcsc_fields[MCSC_F_SC0_H_COEFF_0E + (8 * index)], h_coef_8tap[h_sel].h_coef_e[index]);
			reg_val = is_hw_set_field_value(reg_val,
				&mcsc_fields[MCSC_F_SC0_H_COEFF_0F + (8 * index)], h_coef_8tap[h_sel].h_coef_f[index]);
			is_hw_set_reg(base_addr, &mcsc_regs[MCSC_R_SC0_H_COEFF_0EF + (4 * index)], reg_val);
			reg_val = 0;
			reg_val = is_hw_set_field_value(reg_val,
				&mcsc_fields[MCSC_F_SC0_H_COEFF_0G + (8 * index)], h_coef_8tap[h_sel].h_coef_g[index]);
			reg_val = is_hw_set_field_value(reg_val,
				&mcsc_fields[MCSC_F_SC0_H_COEFF_0H + (8 * index)], h_coef_8tap[h_sel].h_coef_h[index]);
			is_hw_set_reg(base_addr, &mcsc_regs[MCSC_R_SC0_H_COEFF_0GH + (4 * index)], reg_val);
		}
		break;
	case MCSC_OUTPUT1:
		for (index = 0; index <= 8; index++) {
			reg_val = 0;
			reg_val = is_hw_set_field_value(reg_val,
				&mcsc_fields[MCSC_F_SC1_H_COEFF_0A + (8 * index)], h_coef_8tap[h_sel].h_coef_a[index]);
			reg_val = is_hw_set_field_value(reg_val,
				&mcsc_fields[MCSC_F_SC1_H_COEFF_0B + (8 * index)], h_coef_8tap[h_sel].h_coef_b[index]);
			is_hw_set_reg(base_addr, &mcsc_regs[MCSC_R_SC1_H_COEFF_0AB + (4 * index)], reg_val);
			reg_val = 0;
			reg_val = is_hw_set_field_value(reg_val,
				&mcsc_fields[MCSC_F_SC1_H_COEFF_0C + (8 * index)], h_coef_8tap[h_sel].h_coef_c[index]);
			reg_val = is_hw_set_field_value(reg_val,
				&mcsc_fields[MCSC_F_SC1_H_COEFF_0D + (8 * index)], h_coef_8tap[h_sel].h_coef_d[index]);
			is_hw_set_reg(base_addr, &mcsc_regs[MCSC_R_SC1_H_COEFF_0CD + (4 * index)], reg_val);
			reg_val = 0;
			reg_val = is_hw_set_field_value(reg_val,
				&mcsc_fields[MCSC_F_SC1_H_COEFF_0E + (8 * index)], h_coef_8tap[h_sel].h_coef_e[index]);
			reg_val = is_hw_set_field_value(reg_val,
				&mcsc_fields[MCSC_F_SC1_H_COEFF_0F + (8 * index)], h_coef_8tap[h_sel].h_coef_f[index]);
			is_hw_set_reg(base_addr, &mcsc_regs[MCSC_R_SC1_H_COEFF_0EF + (4 * index)], reg_val);
			reg_val = 0;
			reg_val = is_hw_set_field_value(reg_val,
				&mcsc_fields[MCSC_F_SC1_H_COEFF_0G + (8 * index)], h_coef_8tap[h_sel].h_coef_g[index]);
			reg_val = is_hw_set_field_value(reg_val,
				&mcsc_fields[MCSC_F_SC1_H_COEFF_0H + (8 * index)], h_coef_8tap[h_sel].h_coef_h[index]);
			is_hw_set_reg(base_addr, &mcsc_regs[MCSC_R_SC1_H_COEFF_0GH + (4 * index)], reg_val);
		}
		break;
	case MCSC_OUTPUT2:
		for (index = 0; index <= 8; index++) {
			reg_val = 0;
			reg_val = is_hw_set_field_value(reg_val,
				&mcsc_fields[MCSC_F_SC2_H_COEFF_0A + (8 * index)], h_coef_8tap[h_sel].h_coef_a[index]);
			reg_val = is_hw_set_field_value(reg_val,
				&mcsc_fields[MCSC_F_SC2_H_COEFF_0B + (8 * index)], h_coef_8tap[h_sel].h_coef_b[index]);
			is_hw_set_reg(base_addr, &mcsc_regs[MCSC_R_SC2_H_COEFF_0AB + (4 * index)], reg_val);
			reg_val = 0;
			reg_val = is_hw_set_field_value(reg_val,
				&mcsc_fields[MCSC_F_SC2_H_COEFF_0C + (8 * index)], h_coef_8tap[h_sel].h_coef_c[index]);
			reg_val = is_hw_set_field_value(reg_val,
				&mcsc_fields[MCSC_F_SC2_H_COEFF_0D + (8 * index)], h_coef_8tap[h_sel].h_coef_d[index]);
			is_hw_set_reg(base_addr, &mcsc_regs[MCSC_R_SC2_H_COEFF_0CD + (4 * index)], reg_val);
			reg_val = 0;
			reg_val = is_hw_set_field_value(reg_val,
				&mcsc_fields[MCSC_F_SC2_H_COEFF_0E + (8 * index)], h_coef_8tap[h_sel].h_coef_e[index]);
			reg_val = is_hw_set_field_value(reg_val,
				&mcsc_fields[MCSC_F_SC2_H_COEFF_0F + (8 * index)], h_coef_8tap[h_sel].h_coef_f[index]);
			is_hw_set_reg(base_addr, &mcsc_regs[MCSC_R_SC2_H_COEFF_0EF + (4 * index)], reg_val);
			reg_val = 0;
			reg_val = is_hw_set_field_value(reg_val,
				&mcsc_fields[MCSC_F_SC2_H_COEFF_0G + (8 * index)], h_coef_8tap[h_sel].h_coef_g[index]);
			reg_val = is_hw_set_field_value(reg_val,
				&mcsc_fields[MCSC_F_SC2_H_COEFF_0H + (8 * index)], h_coef_8tap[h_sel].h_coef_h[index]);
			is_hw_set_reg(base_addr, &mcsc_regs[MCSC_R_SC2_H_COEFF_0GH + (4 * index)], reg_val);
		}
		break;
	case MCSC_OUTPUT3:
		for (index = 0; index <= 8; index++) {
			reg_val = 0;
			reg_val = is_hw_set_field_value(reg_val,
				&mcsc_fields[MCSC_F_SC3_H_COEFF_0A + (8 * index)], h_coef_8tap[h_sel].h_coef_a[index]);
			reg_val = is_hw_set_field_value(reg_val,
				&mcsc_fields[MCSC_F_SC3_H_COEFF_0B + (8 * index)], h_coef_8tap[h_sel].h_coef_b[index]);
			is_hw_set_reg(base_addr, &mcsc_regs[MCSC_R_SC3_H_COEFF_0AB + (4 * index)], reg_val);
			reg_val = 0;
			reg_val = is_hw_set_field_value(reg_val,
				&mcsc_fields[MCSC_F_SC3_H_COEFF_0C + (8 * index)], h_coef_8tap[h_sel].h_coef_c[index]);
			reg_val = is_hw_set_field_value(reg_val,
				&mcsc_fields[MCSC_F_SC3_H_COEFF_0D + (8 * index)], h_coef_8tap[h_sel].h_coef_d[index]);
			is_hw_set_reg(base_addr, &mcsc_regs[MCSC_R_SC3_H_COEFF_0CD + (4 * index)], reg_val);
			reg_val = 0;
			reg_val = is_hw_set_field_value(reg_val,
				&mcsc_fields[MCSC_F_SC3_H_COEFF_0E + (8 * index)], h_coef_8tap[h_sel].h_coef_e[index]);
			reg_val = is_hw_set_field_value(reg_val,
				&mcsc_fields[MCSC_F_SC3_H_COEFF_0F + (8 * index)], h_coef_8tap[h_sel].h_coef_f[index]);
			is_hw_set_reg(base_addr, &mcsc_regs[MCSC_R_SC3_H_COEFF_0EF + (4 * index)], reg_val);
			reg_val = 0;
			reg_val = is_hw_set_field_value(reg_val,
				&mcsc_fields[MCSC_F_SC3_H_COEFF_0G + (8 * index)], h_coef_8tap[h_sel].h_coef_g[index]);
			reg_val = is_hw_set_field_value(reg_val,
				&mcsc_fields[MCSC_F_SC3_H_COEFF_0H + (8 * index)], h_coef_8tap[h_sel].h_coef_h[index]);
			is_hw_set_reg(base_addr, &mcsc_regs[MCSC_R_SC3_H_COEFF_0GH + (4 * index)], reg_val);
		}
		break;
	case MCSC_OUTPUT4:
		for (index = 0; index <= 8; index++) {
			reg_val = 0;
			reg_val = is_hw_set_field_value(reg_val,
				&mcsc_fields[MCSC_F_SC4_H_COEFF_0A + (8 * index)], h_coef_8tap[h_sel].h_coef_a[index]);
			reg_val = is_hw_set_field_value(reg_val,
				&mcsc_fields[MCSC_F_SC4_H_COEFF_0B + (8 * index)], h_coef_8tap[h_sel].h_coef_b[index]);
			is_hw_set_reg(base_addr, &mcsc_regs[MCSC_R_SC4_H_COEFF_0AB + (4 * index)], reg_val);
			reg_val = 0;
			reg_val = is_hw_set_field_value(reg_val,
				&mcsc_fields[MCSC_F_SC4_H_COEFF_0C + (8 * index)], h_coef_8tap[h_sel].h_coef_c[index]);
			reg_val = is_hw_set_field_value(reg_val,
				&mcsc_fields[MCSC_F_SC4_H_COEFF_0D + (8 * index)], h_coef_8tap[h_sel].h_coef_d[index]);
			is_hw_set_reg(base_addr, &mcsc_regs[MCSC_R_SC4_H_COEFF_0CD + (4 * index)], reg_val);
			reg_val = 0;
			reg_val = is_hw_set_field_value(reg_val,
				&mcsc_fields[MCSC_F_SC4_H_COEFF_0E + (8 * index)], h_coef_8tap[h_sel].h_coef_e[index]);
			reg_val = is_hw_set_field_value(reg_val,
				&mcsc_fields[MCSC_F_SC4_H_COEFF_0F + (8 * index)], h_coef_8tap[h_sel].h_coef_f[index]);
			is_hw_set_reg(base_addr, &mcsc_regs[MCSC_R_SC4_H_COEFF_0EF + (4 * index)], reg_val);
			reg_val = 0;
			reg_val = is_hw_set_field_value(reg_val,
				&mcsc_fields[MCSC_F_SC4_H_COEFF_0G + (8 * index)], h_coef_8tap[h_sel].h_coef_g[index]);
			reg_val = is_hw_set_field_value(reg_val,
				&mcsc_fields[MCSC_F_SC4_H_COEFF_0H + (8 * index)], h_coef_8tap[h_sel].h_coef_h[index]);
			is_hw_set_reg(base_addr, &mcsc_regs[MCSC_R_SC4_H_COEFF_0GH + (4 * index)], reg_val);
		}
		break;
	default:
		break;
	}
}

void is_scaler_set_poly_scaler_v_coef(void __iomem *base_addr, u32 output_id, u32 v_sel)
{
	int index;
	u32 reg_val = 0;

	switch (output_id) {
	case MCSC_OUTPUT0:
		for (index = 0; index <= 8; index++) {
			reg_val = 0;
			reg_val = is_hw_set_field_value(reg_val,
				&mcsc_fields[MCSC_F_SC0_V_COEFF_0A + (4 * index)], v_coef_4tap[v_sel].v_coef_a[index]);
			reg_val = is_hw_set_field_value(reg_val,
				&mcsc_fields[MCSC_F_SC0_V_COEFF_0B + (4 * index)], v_coef_4tap[v_sel].v_coef_b[index]);
			is_hw_set_reg(base_addr, &mcsc_regs[MCSC_R_SC0_V_COEFF_0AB + (2 * index)], reg_val);
			reg_val = 0;
			reg_val = is_hw_set_field_value(reg_val,
				&mcsc_fields[MCSC_F_SC0_V_COEFF_0C + (4 * index)], v_coef_4tap[v_sel].v_coef_c[index]);
			reg_val = is_hw_set_field_value(reg_val,
				&mcsc_fields[MCSC_F_SC0_V_COEFF_0D + (4 * index)], v_coef_4tap[v_sel].v_coef_d[index]);
			is_hw_set_reg(base_addr, &mcsc_regs[MCSC_R_SC0_V_COEFF_0CD + (2 * index)], reg_val);
		}
		break;
	case MCSC_OUTPUT1:
		for (index = 0; index <= 8; index++) {
			reg_val = 0;
			reg_val = is_hw_set_field_value(reg_val,
				&mcsc_fields[MCSC_F_SC1_V_COEFF_0A + (4 * index)], v_coef_4tap[v_sel].v_coef_a[index]);
			reg_val = is_hw_set_field_value(reg_val,
				&mcsc_fields[MCSC_F_SC1_V_COEFF_0B + (4 * index)], v_coef_4tap[v_sel].v_coef_b[index]);
			is_hw_set_reg(base_addr, &mcsc_regs[MCSC_R_SC1_V_COEFF_0AB + (2 * index)], reg_val);
			reg_val = 0;
			reg_val = is_hw_set_field_value(reg_val,
				&mcsc_fields[MCSC_F_SC1_V_COEFF_0C + (4 * index)], v_coef_4tap[v_sel].v_coef_c[index]);
			reg_val = is_hw_set_field_value(reg_val,
				&mcsc_fields[MCSC_F_SC1_V_COEFF_0D + (4 * index)], v_coef_4tap[v_sel].v_coef_d[index]);
			is_hw_set_reg(base_addr, &mcsc_regs[MCSC_R_SC1_V_COEFF_0CD + (2 * index)], reg_val);
		}
		break;
	case MCSC_OUTPUT2:
		for (index = 0; index <= 8; index++) {
			reg_val = 0;
			reg_val = is_hw_set_field_value(reg_val,
				&mcsc_fields[MCSC_F_SC2_V_COEFF_0A + (4 * index)], v_coef_4tap[v_sel].v_coef_a[index]);
			reg_val = is_hw_set_field_value(reg_val,
				&mcsc_fields[MCSC_F_SC2_V_COEFF_0B + (4 * index)], v_coef_4tap[v_sel].v_coef_b[index]);
			is_hw_set_reg(base_addr, &mcsc_regs[MCSC_R_SC2_V_COEFF_0AB + (2 * index)], reg_val);
			reg_val = 0;
			reg_val = is_hw_set_field_value(reg_val,
				&mcsc_fields[MCSC_F_SC2_V_COEFF_0C + (4 * index)], v_coef_4tap[v_sel].v_coef_c[index]);
			reg_val = is_hw_set_field_value(reg_val,
				&mcsc_fields[MCSC_F_SC2_V_COEFF_0D + (4 * index)], v_coef_4tap[v_sel].v_coef_d[index]);
			is_hw_set_reg(base_addr, &mcsc_regs[MCSC_R_SC2_V_COEFF_0CD + (2 * index)], reg_val);
		}
		break;
	case MCSC_OUTPUT3:
		for (index = 0; index <= 8; index++) {
			reg_val = 0;
			reg_val = is_hw_set_field_value(reg_val,
				&mcsc_fields[MCSC_F_SC3_V_COEFF_0A + (4 * index)], v_coef_4tap[v_sel].v_coef_a[index]);
			reg_val = is_hw_set_field_value(reg_val,
				&mcsc_fields[MCSC_F_SC3_V_COEFF_0B + (4 * index)], v_coef_4tap[v_sel].v_coef_b[index]);
			is_hw_set_reg(base_addr, &mcsc_regs[MCSC_R_SC3_V_COEFF_0AB + (2 * index)], reg_val);
			reg_val = 0;
			reg_val = is_hw_set_field_value(reg_val,
				&mcsc_fields[MCSC_F_SC3_V_COEFF_0C + (4 * index)], v_coef_4tap[v_sel].v_coef_c[index]);
			reg_val = is_hw_set_field_value(reg_val,
				&mcsc_fields[MCSC_F_SC3_V_COEFF_0D + (4 * index)], v_coef_4tap[v_sel].v_coef_d[index]);
			is_hw_set_reg(base_addr, &mcsc_regs[MCSC_R_SC3_V_COEFF_0CD + (2 * index)], reg_val);
		}
		break;
	case MCSC_OUTPUT4:
		for (index = 0; index <= 8; index++) {
			reg_val = 0;
			reg_val = is_hw_set_field_value(reg_val,
				&mcsc_fields[MCSC_F_SC4_V_COEFF_0A + (4 * index)], v_coef_4tap[v_sel].v_coef_a[index]);
			reg_val = is_hw_set_field_value(reg_val,
				&mcsc_fields[MCSC_F_SC4_V_COEFF_0B + (4 * index)], v_coef_4tap[v_sel].v_coef_b[index]);
			is_hw_set_reg(base_addr, &mcsc_regs[MCSC_R_SC4_V_COEFF_0AB + (2 * index)], reg_val);
			reg_val = 0;
			reg_val = is_hw_set_field_value(reg_val,
				&mcsc_fields[MCSC_F_SC4_V_COEFF_0C + (4 * index)], v_coef_4tap[v_sel].v_coef_c[index]);
			reg_val = is_hw_set_field_value(reg_val,
				&mcsc_fields[MCSC_F_SC4_V_COEFF_0D + (4 * index)], v_coef_4tap[v_sel].v_coef_d[index]);
			is_hw_set_reg(base_addr, &mcsc_regs[MCSC_R_SC4_V_COEFF_0CD + (2 * index)], reg_val);
		}
		break;
	default:
		break;
	}
}

u32 get_scaler_coef_ver1(u32 ratio, bool adjust_coef)
{
	u32 coef;

	if (ratio <= RATIO_X8_8)
		coef = MCSC_COEFF_x8_8;
	else if (ratio > RATIO_X8_8 && ratio <= RATIO_X7_8)
		coef = MCSC_COEFF_x7_8;
	else if (ratio > RATIO_X7_8 && ratio <= RATIO_X6_8)
		coef = adjust_coef == true ? MCSC_COEFF_x7_8 : MCSC_COEFF_x6_8;
	else if (ratio > RATIO_X6_8 && ratio <= RATIO_X5_8)
		coef = adjust_coef == true ? MCSC_COEFF_x7_8 : MCSC_COEFF_x5_8;
	else if (ratio > RATIO_X5_8 && ratio <= RATIO_X4_8)
		coef = MCSC_COEFF_x4_8;
	else if (ratio > RATIO_X4_8 && ratio <= RATIO_X3_8)
		coef = MCSC_COEFF_x3_8;
	else if (ratio > RATIO_X3_8 && ratio <= RATIO_X2_8)
		coef = MCSC_COEFF_x2_8;
	else
		coef = MCSC_COEFF_x2_8;

	return coef;
}

u32 get_scaler_coef_ver2(u32 ratio, struct scaler_coef_cfg *sc_coef)
{
	u32 coef;

	if (ratio <= RATIO_X8_8)
		coef = sc_coef->ratio_x8_8;
	else if (ratio > RATIO_X8_8 && ratio <= RATIO_X7_8)
		coef = sc_coef->ratio_x7_8;
	else if (ratio > RATIO_X7_8 && ratio <= RATIO_X6_8)
		coef = sc_coef->ratio_x6_8;
	else if (ratio > RATIO_X6_8 && ratio <= RATIO_X5_8)
		coef = sc_coef->ratio_x5_8;
	else if (ratio > RATIO_X5_8 && ratio <= RATIO_X4_8)
		coef = sc_coef->ratio_x4_8;
	else if (ratio > RATIO_X4_8 && ratio <= RATIO_X3_8)
		coef = sc_coef->ratio_x3_8;
	else if (ratio > RATIO_X3_8 && ratio <= RATIO_X2_8)
		coef = sc_coef->ratio_x2_8;
	else
		coef = sc_coef->ratio_x2_8;

	return coef;
}

void is_scaler_set_poly_scaler_coef(void __iomem *base_addr, u32 output_id,
	u32 hratio, u32 vratio, struct scaler_coef_cfg *sc_coef,
	enum exynos_sensor_position sensor_position)
{
	u32 h_coef = 0, v_coef = 0;
	/* this value equals 0 - scale-down operation */
	u32 h_phase_offset = 0, v_phase_offset = 0;

	if (sc_coef) {
		h_coef = get_scaler_coef_ver2(hratio, sc_coef);
		v_coef = get_scaler_coef_ver2(vratio, sc_coef);
	}

	/* scale up case */
	if (hratio < RATIO_X8_8)
		h_phase_offset = hratio >> 1;
	if (vratio < RATIO_X8_8)
		v_phase_offset = vratio >> 1;

	is_scaler_set_h_init_phase_offset(base_addr, output_id, h_phase_offset);
	is_scaler_set_v_init_phase_offset(base_addr, output_id, v_phase_offset);
	is_scaler_set_poly_scaler_h_coef(base_addr, output_id, h_coef);
	is_scaler_set_poly_scaler_v_coef(base_addr, output_id, v_coef);
}

void is_scaler_set_poly_round_mode(void __iomem *base_addr, u32 output_id, u32 mode)
{
	switch (output_id) {
	case MCSC_OUTPUT0:
		is_hw_set_field(base_addr, &mcsc_regs[MCSC_R_SC0_ROUND_MODE],
			&mcsc_fields[MCSC_F_SC0_ROUND_MODE], mode);
		break;
	case MCSC_OUTPUT1:
		is_hw_set_field(base_addr, &mcsc_regs[MCSC_R_SC1_ROUND_MODE],
			&mcsc_fields[MCSC_F_SC1_ROUND_MODE], mode);
		break;
	case MCSC_OUTPUT2:
		is_hw_set_field(base_addr, &mcsc_regs[MCSC_R_SC2_ROUND_MODE],
			&mcsc_fields[MCSC_F_SC2_ROUND_MODE], mode);
		break;
	case MCSC_OUTPUT3:
		is_hw_set_field(base_addr, &mcsc_regs[MCSC_R_SC3_ROUND_MODE],
			&mcsc_fields[MCSC_F_SC3_ROUND_MODE], mode);
		break;
	case MCSC_OUTPUT4:
		is_hw_set_field(base_addr, &mcsc_regs[MCSC_R_SC4_ROUND_MODE],
			&mcsc_fields[MCSC_F_SC4_ROUND_MODE], mode);
		break;
	default:
		break;
	}
}

void is_scaler_set_post_scaler_enable(void __iomem *base_addr, u32 output_id, u32 enable)
{
	switch (output_id) {
	case MCSC_OUTPUT0:
		is_hw_set_field(base_addr, &mcsc_regs[MCSC_R_PC0_CTRL], &mcsc_fields[MCSC_F_PC0_ENABLE], enable);
		break;
	case MCSC_OUTPUT1:
		is_hw_set_field(base_addr, &mcsc_regs[MCSC_R_PC1_CTRL], &mcsc_fields[MCSC_F_PC1_ENABLE], enable);
		break;
	case MCSC_OUTPUT2:
		is_hw_set_field(base_addr, &mcsc_regs[MCSC_R_PC2_CTRL], &mcsc_fields[MCSC_F_PC2_ENABLE], enable);
		break;
	case MCSC_OUTPUT3:
		/* not support */
		break;
	case MCSC_OUTPUT4:
		/* not support */
		break;
	default:
		break;
	}
}

void is_scaler_set_post_img_size(void __iomem *base_addr, u32 output_id, u32 width, u32 height)
{
	u32 reg_val = 0;

	switch (output_id) {
	case MCSC_OUTPUT0:
		reg_val = is_hw_set_field_value(reg_val, &mcsc_fields[MCSC_F_PC0_IMG_HSIZE], width);
		reg_val = is_hw_set_field_value(reg_val, &mcsc_fields[MCSC_F_PC0_IMG_VSIZE], height);
		is_hw_set_reg(base_addr, &mcsc_regs[MCSC_R_PC0_IMG_SIZE], reg_val);
		break;
	case MCSC_OUTPUT1:
		reg_val = is_hw_set_field_value(reg_val, &mcsc_fields[MCSC_F_PC1_IMG_HSIZE], width);
		reg_val = is_hw_set_field_value(reg_val, &mcsc_fields[MCSC_F_PC1_IMG_VSIZE], height);
		is_hw_set_reg(base_addr, &mcsc_regs[MCSC_R_PC1_IMG_SIZE], reg_val);
		break;
	case MCSC_OUTPUT2:
		reg_val = is_hw_set_field_value(reg_val, &mcsc_fields[MCSC_F_PC2_IMG_HSIZE], width);
		reg_val = is_hw_set_field_value(reg_val, &mcsc_fields[MCSC_F_PC2_IMG_VSIZE], height);
		is_hw_set_reg(base_addr, &mcsc_regs[MCSC_R_PC2_IMG_SIZE], reg_val);
		break;
	case MCSC_OUTPUT3:
		/* not support */
		break;
	case MCSC_OUTPUT4:
		/* not support */
		break;
	default:
		break;
	}
}

void is_scaler_get_post_img_size(void __iomem *base_addr, u32 output_id, u32 *width, u32 *height)
{
	switch (output_id) {
	case MCSC_OUTPUT0:
		*width = is_hw_get_field(base_addr, &mcsc_regs[MCSC_R_PC0_IMG_SIZE],
			&mcsc_fields[MCSC_F_PC0_IMG_HSIZE]);
		*height = is_hw_get_field(base_addr, &mcsc_regs[MCSC_R_PC0_IMG_SIZE],
			&mcsc_fields[MCSC_F_PC0_IMG_VSIZE]);
		break;
	case MCSC_OUTPUT1:
		*width = is_hw_get_field(base_addr, &mcsc_regs[MCSC_R_PC1_IMG_SIZE],
			&mcsc_fields[MCSC_F_PC1_IMG_HSIZE]);
		*height = is_hw_get_field(base_addr, &mcsc_regs[MCSC_R_PC1_IMG_SIZE],
			&mcsc_fields[MCSC_F_PC1_IMG_VSIZE]);
		break;
	case MCSC_OUTPUT2:
		*width = is_hw_get_field(base_addr, &mcsc_regs[MCSC_R_PC2_IMG_SIZE],
			&mcsc_fields[MCSC_F_PC2_IMG_HSIZE]);
		*height = is_hw_get_field(base_addr, &mcsc_regs[MCSC_R_PC2_IMG_SIZE],
			&mcsc_fields[MCSC_F_PC2_IMG_VSIZE]);
		break;
	case MCSC_OUTPUT3:
		/* not support */
		break;
	case MCSC_OUTPUT4:
		/* not support */
		break;
	default:
		break;
	}
}

void is_scaler_set_post_dst_size(void __iomem *base_addr, u32 output_id, u32 width, u32 height)
{
	u32 reg_val = 0;

	switch (output_id) {
	case MCSC_OUTPUT0:
		reg_val = is_hw_set_field_value(reg_val, &mcsc_fields[MCSC_F_PC0_DST_HSIZE], width);
		reg_val = is_hw_set_field_value(reg_val, &mcsc_fields[MCSC_F_PC0_DST_VSIZE], height);
		is_hw_set_reg(base_addr, &mcsc_regs[MCSC_R_PC0_DST_SIZE], reg_val);
		break;
	case MCSC_OUTPUT1:
		reg_val = is_hw_set_field_value(reg_val, &mcsc_fields[MCSC_F_PC1_DST_HSIZE], width);
		reg_val = is_hw_set_field_value(reg_val, &mcsc_fields[MCSC_F_PC1_DST_VSIZE], height);
		is_hw_set_reg(base_addr, &mcsc_regs[MCSC_R_PC1_DST_SIZE], reg_val);
		break;
	case MCSC_OUTPUT2:
		reg_val = is_hw_set_field_value(reg_val, &mcsc_fields[MCSC_F_PC2_DST_HSIZE], width);
		reg_val = is_hw_set_field_value(reg_val, &mcsc_fields[MCSC_F_PC2_DST_VSIZE], height);
		is_hw_set_reg(base_addr, &mcsc_regs[MCSC_R_PC2_DST_SIZE], reg_val);
		break;
	case MCSC_OUTPUT3:
		/* not support */
		break;
	case MCSC_OUTPUT4:
		/* not support */
		break;
	default:
		break;
	}
}

void is_scaler_get_post_dst_size(void __iomem *base_addr, u32 output_id, u32 *width, u32 *height)
{
	switch (output_id) {
	case MCSC_OUTPUT0:
		*width = is_hw_get_field(base_addr, &mcsc_regs[MCSC_R_PC0_DST_SIZE],
			&mcsc_fields[MCSC_F_PC0_DST_HSIZE]);
		*height = is_hw_get_field(base_addr, &mcsc_regs[MCSC_R_PC0_DST_SIZE],
			&mcsc_fields[MCSC_F_PC0_DST_VSIZE]);
		break;
	case MCSC_OUTPUT1:
		*width = is_hw_get_field(base_addr, &mcsc_regs[MCSC_R_PC1_DST_SIZE],
			&mcsc_fields[MCSC_F_PC1_DST_HSIZE]);
		*height = is_hw_get_field(base_addr, &mcsc_regs[MCSC_R_PC1_DST_SIZE],
			&mcsc_fields[MCSC_F_PC1_DST_VSIZE]);
		break;
	case MCSC_OUTPUT2:
		*width = is_hw_get_field(base_addr, &mcsc_regs[MCSC_R_PC2_DST_SIZE],
			&mcsc_fields[MCSC_F_PC2_DST_HSIZE]);
		*height = is_hw_get_field(base_addr, &mcsc_regs[MCSC_R_PC2_DST_SIZE],
			&mcsc_fields[MCSC_F_PC2_DST_VSIZE]);
		break;
	case MCSC_OUTPUT3:
		/* not support */
		break;
	case MCSC_OUTPUT4:
		/* not support */
		break;
	default:
		break;
	}
}

void is_scaler_set_post_scaling_ratio(void __iomem *base_addr, u32 output_id, u32 hratio, u32 vratio)
{
	switch (output_id) {
	case MCSC_OUTPUT0:
		is_hw_set_field(base_addr, &mcsc_regs[MCSC_R_PC0_H_RATIO],
			&mcsc_fields[MCSC_F_PC0_H_RATIO], hratio);
		is_hw_set_field(base_addr, &mcsc_regs[MCSC_R_PC0_V_RATIO],
			&mcsc_fields[MCSC_F_PC0_V_RATIO], vratio);
		break;
	case MCSC_OUTPUT1:
		is_hw_set_field(base_addr, &mcsc_regs[MCSC_R_PC1_H_RATIO],
			&mcsc_fields[MCSC_F_PC1_H_RATIO], hratio);
		is_hw_set_field(base_addr, &mcsc_regs[MCSC_R_PC1_V_RATIO],
			&mcsc_fields[MCSC_F_PC1_V_RATIO], vratio);
		break;
	case MCSC_OUTPUT2:
		is_hw_set_field(base_addr, &mcsc_regs[MCSC_R_PC2_H_RATIO],
			&mcsc_fields[MCSC_F_PC2_H_RATIO], hratio);
		is_hw_set_field(base_addr, &mcsc_regs[MCSC_R_PC2_V_RATIO],
			&mcsc_fields[MCSC_F_PC2_V_RATIO], vratio);
		break;
	case MCSC_OUTPUT3:
		/* not support */
		break;
	case MCSC_OUTPUT4:
		/* not support */
		break;
	default:
		break;
	}
}


void is_scaler_set_post_h_init_phase_offset(void __iomem *base_addr, u32 output_id, u32 h_offset)
{
	switch (output_id) {
	case MCSC_OUTPUT0:
		is_hw_set_field(base_addr, &mcsc_regs[MCSC_R_PC0_H_INIT_PHASE_OFFSET],
			&mcsc_fields[MCSC_F_PC0_H_INIT_PHASE_OFFSET], h_offset);
		break;
	case MCSC_OUTPUT1:
		is_hw_set_field(base_addr, &mcsc_regs[MCSC_R_PC1_H_INIT_PHASE_OFFSET],
			&mcsc_fields[MCSC_F_PC1_H_INIT_PHASE_OFFSET], h_offset);
		break;
	case MCSC_OUTPUT2:
		is_hw_set_field(base_addr, &mcsc_regs[MCSC_R_PC2_H_INIT_PHASE_OFFSET],
			&mcsc_fields[MCSC_F_PC2_H_INIT_PHASE_OFFSET], h_offset);
		break;
	case MCSC_OUTPUT3:
		/* not support */
		break;
	case MCSC_OUTPUT4:
		/* not support */
		break;
	default:
		break;
	}
}

void is_scaler_set_post_v_init_phase_offset(void __iomem *base_addr, u32 output_id, u32 v_offset)
{
	switch (output_id) {
	case MCSC_OUTPUT0:
		is_hw_set_field(base_addr, &mcsc_regs[MCSC_R_PC0_V_INIT_PHASE_OFFSET],
			&mcsc_fields[MCSC_F_PC0_V_INIT_PHASE_OFFSET], v_offset);
		break;
	case MCSC_OUTPUT1:
		is_hw_set_field(base_addr, &mcsc_regs[MCSC_R_PC1_V_INIT_PHASE_OFFSET],
			&mcsc_fields[MCSC_F_PC1_V_INIT_PHASE_OFFSET], v_offset);
		break;
	case MCSC_OUTPUT2:
		is_hw_set_field(base_addr, &mcsc_regs[MCSC_R_PC2_V_INIT_PHASE_OFFSET],
			&mcsc_fields[MCSC_F_PC2_V_INIT_PHASE_OFFSET], v_offset);
		break;
	case MCSC_OUTPUT3:
		/* not support */
		break;
	case MCSC_OUTPUT4:
		/* not support */
		break;
	default:
		break;
	}
}

void is_scaler_set_post_scaler_h_v_coef(void __iomem *base_addr, u32 output_id, u32 h_sel, u32 v_sel)
{
	u32 reg_val = 0;

	switch (output_id) {
	case MCSC_OUTPUT0:
		reg_val = 0;
		reg_val = is_hw_set_field_value(reg_val, &mcsc_fields[MCSC_F_PC0_H_COEFF_SEL], h_sel);
		reg_val = is_hw_set_field_value(reg_val, &mcsc_fields[MCSC_F_PC0_V_COEFF_SEL], v_sel);
		is_hw_set_reg(base_addr, &mcsc_regs[MCSC_R_PC0_COEFF_CTRL], reg_val);
		break;
	case MCSC_OUTPUT1:
		reg_val = 0;
		reg_val = is_hw_set_field_value(reg_val, &mcsc_fields[MCSC_F_PC1_H_COEFF_SEL], h_sel);
		reg_val = is_hw_set_field_value(reg_val, &mcsc_fields[MCSC_F_PC1_V_COEFF_SEL], v_sel);
		is_hw_set_reg(base_addr, &mcsc_regs[MCSC_R_PC1_COEFF_CTRL], reg_val);
		break;
	case MCSC_OUTPUT2:
		reg_val = 0;
		reg_val = is_hw_set_field_value(reg_val, &mcsc_fields[MCSC_F_PC2_H_COEFF_SEL], h_sel);
		reg_val = is_hw_set_field_value(reg_val, &mcsc_fields[MCSC_F_PC2_V_COEFF_SEL], v_sel);
		is_hw_set_reg(base_addr, &mcsc_regs[MCSC_R_PC2_COEFF_CTRL], reg_val);
		break;
	case MCSC_OUTPUT3:
		/* not support */
		break;
	case MCSC_OUTPUT4:
		/* not support */
		break;
	default:
		break;
	}
}

void is_scaler_set_post_scaler_coef(void __iomem *base_addr, u32 output_id,
	u32 hratio, u32 vratio, struct scaler_coef_cfg *sc_coef)
{
	u32 h_coef = 0, v_coef = 0;
	/* this value equals 0 - scale-down operation */
	u32 h_phase_offset = 0, v_phase_offset = 0;

	if (sc_coef) {
		h_coef = get_scaler_coef_ver2(hratio, sc_coef);
		v_coef = get_scaler_coef_ver2(vratio, sc_coef);
	}

	/* scale up case */
	if (hratio < RATIO_X8_8)
		h_phase_offset = hratio >> 1;
	if (vratio < RATIO_X8_8)
		v_phase_offset = vratio >> 1;

	is_scaler_set_post_h_init_phase_offset(base_addr, output_id, h_phase_offset);
	is_scaler_set_post_v_init_phase_offset(base_addr, output_id, v_phase_offset);
	is_scaler_set_post_scaler_h_v_coef(base_addr, output_id, h_coef, v_coef);
}

void is_scaler_set_post_round_mode(void __iomem *base_addr, u32 output_id, u32 mode)
{
	switch (output_id) {
	case MCSC_OUTPUT0:
		is_hw_set_field(base_addr, &mcsc_regs[MCSC_R_PC0_ROUND_MODE],
			&mcsc_fields[MCSC_F_PC0_ROUND_MODE], mode);
		break;
	case MCSC_OUTPUT1:
		is_hw_set_field(base_addr, &mcsc_regs[MCSC_R_PC1_ROUND_MODE],
			&mcsc_fields[MCSC_F_PC1_ROUND_MODE], mode);
		break;
	case MCSC_OUTPUT2:
		is_hw_set_field(base_addr, &mcsc_regs[MCSC_R_PC2_ROUND_MODE],
			&mcsc_fields[MCSC_F_PC2_ROUND_MODE], mode);
		break;
	case MCSC_OUTPUT3:
		/* not support */
		break;
	case MCSC_OUTPUT4:
		/* not support */
		break;
		break;
	default:
		break;
	}
}

void is_scaler_set_420_conversion(void __iomem *base_addr, u32 output_id, u32 conv420_weight, u32 conv420_en)
{
	switch (output_id) {
	case MCSC_OUTPUT0:
		is_hw_set_field(base_addr, &mcsc_regs[MCSC_R_PC0_CONV420_WEIGHT],
			&mcsc_fields[MCSC_F_PC0_CONV420_WEIGHT], conv420_weight);
		is_hw_set_field(base_addr, &mcsc_regs[MCSC_R_PC0_CONV420_CTRL],
			&mcsc_fields[MCSC_F_PC0_CONV420_ENABLE], conv420_en);
		break;
	case MCSC_OUTPUT1:
		is_hw_set_field(base_addr, &mcsc_regs[MCSC_R_PC1_CONV420_WEIGHT],
			&mcsc_fields[MCSC_F_PC1_CONV420_WEIGHT], conv420_weight);
		is_hw_set_field(base_addr, &mcsc_regs[MCSC_R_PC1_CONV420_CTRL],
			&mcsc_fields[MCSC_F_PC1_CONV420_ENABLE], conv420_en);
		break;
	case MCSC_OUTPUT2:
		is_hw_set_field(base_addr, &mcsc_regs[MCSC_R_PC2_CONV420_WEIGHT],
			&mcsc_fields[MCSC_F_PC2_CONV420_WEIGHT], conv420_weight);
		is_hw_set_field(base_addr, &mcsc_regs[MCSC_R_PC2_CONV420_CTRL],
			&mcsc_fields[MCSC_F_PC2_CONV420_ENABLE], conv420_en);
		break;
	case MCSC_OUTPUT3:
		is_hw_set_field(base_addr, &mcsc_regs[MCSC_R_PC3_CONV420_WEIGHT],
			&mcsc_fields[MCSC_F_PC3_CONV420_WEIGHT], conv420_weight);
		is_hw_set_field(base_addr, &mcsc_regs[MCSC_R_PC3_CONV420_CTRL],
			&mcsc_fields[MCSC_F_PC3_CONV420_ENABLE], conv420_en);
		break;
	case MCSC_OUTPUT4:
		is_hw_set_field(base_addr, &mcsc_regs[MCSC_R_PC4_CONV420_WEIGHT],
			&mcsc_fields[MCSC_F_PC4_CONV420_WEIGHT], conv420_weight);
		is_hw_set_field(base_addr, &mcsc_regs[MCSC_R_PC4_CONV420_CTRL],
			&mcsc_fields[MCSC_F_PC4_CONV420_ENABLE], conv420_en);
		break;
	default:
		break;
	}
}

void is_scaler_set_bchs_enable(void __iomem *base_addr, u32 output_id, bool bchs_en)
{
	switch (output_id) {
	case MCSC_OUTPUT0:
		is_hw_set_field(base_addr, &mcsc_regs[MCSC_R_PC0_BCHS_CTRL],
			&mcsc_fields[MCSC_F_PC0_BCHS_ENABLE], bchs_en);

		/* default BCHS clamp value */
		is_hw_set_reg(base_addr, &mcsc_regs[MCSC_R_PC0_BCHS_CLAMP_Y], 0x03FF0000);
		is_hw_set_reg(base_addr, &mcsc_regs[MCSC_R_PC0_BCHS_CLAMP_C], 0x03FF0000);
		break;
	case MCSC_OUTPUT1:
		is_hw_set_field(base_addr, &mcsc_regs[MCSC_R_PC1_BCHS_CTRL],
			&mcsc_fields[MCSC_F_PC1_BCHS_ENABLE], bchs_en);

		/* default BCHS clamp value */
		is_hw_set_reg(base_addr, &mcsc_regs[MCSC_R_PC1_BCHS_CLAMP_Y], 0x03FF0000);
		is_hw_set_reg(base_addr, &mcsc_regs[MCSC_R_PC1_BCHS_CLAMP_C], 0x03FF0000);
		break;
	case MCSC_OUTPUT2:
		is_hw_set_field(base_addr, &mcsc_regs[MCSC_R_PC2_BCHS_CTRL],
			&mcsc_fields[MCSC_F_PC2_BCHS_ENABLE], bchs_en);

		/* default BCHS clamp value */
		is_hw_set_reg(base_addr, &mcsc_regs[MCSC_R_PC2_BCHS_CLAMP_Y], 0x03FF0000);
		is_hw_set_reg(base_addr, &mcsc_regs[MCSC_R_PC2_BCHS_CLAMP_C], 0x03FF0000);
		break;
	case MCSC_OUTPUT3:
		is_hw_set_field(base_addr, &mcsc_regs[MCSC_R_PC3_BCHS_CTRL],
			&mcsc_fields[MCSC_F_PC3_BCHS_ENABLE], bchs_en);

		/* default BCHS clamp value */
		is_hw_set_reg(base_addr, &mcsc_regs[MCSC_R_PC3_BCHS_CLAMP_Y], 0x03FF0000);
		is_hw_set_reg(base_addr, &mcsc_regs[MCSC_R_PC3_BCHS_CLAMP_C], 0x03FF0000);
		break;
	case MCSC_OUTPUT4:
		is_hw_set_field(base_addr, &mcsc_regs[MCSC_R_PC4_BCHS_CTRL],
			&mcsc_fields[MCSC_F_PC4_BCHS_ENABLE], bchs_en);

		/* default BCHS clamp value */
		is_hw_set_reg(base_addr, &mcsc_regs[MCSC_R_PC4_BCHS_CLAMP_Y], 0x03FF0000);
		is_hw_set_reg(base_addr, &mcsc_regs[MCSC_R_PC4_BCHS_CLAMP_C], 0x03FF0000);
		break;
	default:
		break;
	}
}

/* brightness/contrast control */
void is_scaler_set_b_c(void __iomem *base_addr, u32 output_id, u32 y_offset, u32 y_gain)
{
	u32 reg_val = 0;

	switch (output_id) {
	case MCSC_OUTPUT0:
		reg_val = is_hw_set_field_value(reg_val, &mcsc_fields[MCSC_F_PC0_BCHS_Y_OFFSET], y_offset);
		reg_val = is_hw_set_field_value(reg_val, &mcsc_fields[MCSC_F_PC0_BCHS_Y_GAIN], y_gain);
		is_hw_set_reg(base_addr, &mcsc_regs[MCSC_R_PC0_BCHS_BC], reg_val);
		break;
	case MCSC_OUTPUT1:
		reg_val = is_hw_set_field_value(reg_val, &mcsc_fields[MCSC_F_PC1_BCHS_Y_OFFSET], y_offset);
		reg_val = is_hw_set_field_value(reg_val, &mcsc_fields[MCSC_F_PC1_BCHS_Y_GAIN], y_gain);
		is_hw_set_reg(base_addr, &mcsc_regs[MCSC_R_PC1_BCHS_BC], reg_val);
		break;
	case MCSC_OUTPUT2:
		reg_val = is_hw_set_field_value(reg_val, &mcsc_fields[MCSC_F_PC2_BCHS_Y_OFFSET], y_offset);
		reg_val = is_hw_set_field_value(reg_val, &mcsc_fields[MCSC_F_PC2_BCHS_Y_GAIN], y_gain);
		is_hw_set_reg(base_addr, &mcsc_regs[MCSC_R_PC2_BCHS_BC], reg_val);
		break;
	case MCSC_OUTPUT3:
		reg_val = is_hw_set_field_value(reg_val, &mcsc_fields[MCSC_F_PC3_BCHS_Y_OFFSET], y_offset);
		reg_val = is_hw_set_field_value(reg_val, &mcsc_fields[MCSC_F_PC3_BCHS_Y_GAIN], y_gain);
		is_hw_set_reg(base_addr, &mcsc_regs[MCSC_R_PC3_BCHS_BC], reg_val);
		break;
	case MCSC_OUTPUT4:
		reg_val = is_hw_set_field_value(reg_val, &mcsc_fields[MCSC_F_PC4_BCHS_Y_OFFSET], y_offset);
		reg_val = is_hw_set_field_value(reg_val, &mcsc_fields[MCSC_F_PC4_BCHS_Y_GAIN], y_gain);
		is_hw_set_reg(base_addr, &mcsc_regs[MCSC_R_PC4_BCHS_BC], reg_val);
		break;
	default:
		break;
	}
}

/* hue/saturation control */
void is_scaler_set_h_s(void __iomem *base_addr, u32 output_id,
	u32 c_gain00, u32 c_gain01, u32 c_gain10, u32 c_gain11)
{
	u32 reg_val = 0;

	switch (output_id) {
	case MCSC_OUTPUT0:
		reg_val = is_hw_set_field_value(reg_val, &mcsc_fields[MCSC_F_PC0_BCHS_C_GAIN_00], c_gain00);
		reg_val = is_hw_set_field_value(reg_val, &mcsc_fields[MCSC_F_PC0_BCHS_C_GAIN_01], c_gain01);
		is_hw_set_reg(base_addr, &mcsc_regs[MCSC_R_PC0_BCHS_HS1], reg_val);
		reg_val = 0;
		reg_val = is_hw_set_field_value(reg_val, &mcsc_fields[MCSC_F_PC0_BCHS_C_GAIN_10], c_gain10);
		reg_val = is_hw_set_field_value(reg_val, &mcsc_fields[MCSC_F_PC0_BCHS_C_GAIN_11], c_gain11);
		is_hw_set_reg(base_addr, &mcsc_regs[MCSC_R_PC0_BCHS_HS2], reg_val);
		break;
	case MCSC_OUTPUT1:
		reg_val = is_hw_set_field_value(reg_val, &mcsc_fields[MCSC_F_PC1_BCHS_C_GAIN_00], c_gain00);
		reg_val = is_hw_set_field_value(reg_val, &mcsc_fields[MCSC_F_PC1_BCHS_C_GAIN_01], c_gain01);
		is_hw_set_reg(base_addr, &mcsc_regs[MCSC_R_PC1_BCHS_HS1], reg_val);
		reg_val = 0;
		reg_val = is_hw_set_field_value(reg_val, &mcsc_fields[MCSC_F_PC1_BCHS_C_GAIN_10], c_gain10);
		reg_val = is_hw_set_field_value(reg_val, &mcsc_fields[MCSC_F_PC1_BCHS_C_GAIN_11], c_gain11);
		is_hw_set_reg(base_addr, &mcsc_regs[MCSC_R_PC1_BCHS_HS2], reg_val);
		break;
	case MCSC_OUTPUT2:
		reg_val = is_hw_set_field_value(reg_val, &mcsc_fields[MCSC_F_PC2_BCHS_C_GAIN_00], c_gain00);
		reg_val = is_hw_set_field_value(reg_val, &mcsc_fields[MCSC_F_PC2_BCHS_C_GAIN_01], c_gain01);
		is_hw_set_reg(base_addr, &mcsc_regs[MCSC_R_PC2_BCHS_HS1], reg_val);
		reg_val = 0;
		reg_val = is_hw_set_field_value(reg_val, &mcsc_fields[MCSC_F_PC2_BCHS_C_GAIN_10], c_gain10);
		reg_val = is_hw_set_field_value(reg_val, &mcsc_fields[MCSC_F_PC2_BCHS_C_GAIN_11], c_gain11);
		is_hw_set_reg(base_addr, &mcsc_regs[MCSC_R_PC2_BCHS_HS2], reg_val);
		break;
	case MCSC_OUTPUT3:
		reg_val = is_hw_set_field_value(reg_val, &mcsc_fields[MCSC_F_PC3_BCHS_C_GAIN_00], c_gain00);
		reg_val = is_hw_set_field_value(reg_val, &mcsc_fields[MCSC_F_PC3_BCHS_C_GAIN_01], c_gain01);
		is_hw_set_reg(base_addr, &mcsc_regs[MCSC_R_PC3_BCHS_HS1], reg_val);
		reg_val = 0;
		reg_val = is_hw_set_field_value(reg_val, &mcsc_fields[MCSC_F_PC3_BCHS_C_GAIN_10], c_gain10);
		reg_val = is_hw_set_field_value(reg_val, &mcsc_fields[MCSC_F_PC3_BCHS_C_GAIN_11], c_gain11);
		is_hw_set_reg(base_addr, &mcsc_regs[MCSC_R_PC3_BCHS_HS2], reg_val);
		break;
	case MCSC_OUTPUT4:
		reg_val = is_hw_set_field_value(reg_val, &mcsc_fields[MCSC_F_PC4_BCHS_C_GAIN_00], c_gain00);
		reg_val = is_hw_set_field_value(reg_val, &mcsc_fields[MCSC_F_PC4_BCHS_C_GAIN_01], c_gain01);
		is_hw_set_reg(base_addr, &mcsc_regs[MCSC_R_PC4_BCHS_HS1], reg_val);
		reg_val = 0;
		reg_val = is_hw_set_field_value(reg_val, &mcsc_fields[MCSC_F_PC4_BCHS_C_GAIN_10], c_gain10);
		reg_val = is_hw_set_field_value(reg_val, &mcsc_fields[MCSC_F_PC4_BCHS_C_GAIN_11], c_gain11);
		is_hw_set_reg(base_addr, &mcsc_regs[MCSC_R_PC4_BCHS_HS2], reg_val);
		break;
	default:
		break;
	}
}

void is_scaler_set_bchs_clamp(void __iomem *base_addr, u32 output_id,
	u32 y_max, u32 y_min, u32 c_max, u32 c_min)
{
	u32 reg_val_y = 0, reg_idx_y = 0;
	u32 reg_val_c = 0, reg_idx_c = 0;

	switch (output_id) {
	case MCSC_OUTPUT0:
		reg_val_y = is_hw_set_field_value(reg_val_y, &mcsc_fields[MCSC_F_PC0_BCHS_Y_CLAMP_MAX], y_max);
		reg_val_y = is_hw_set_field_value(reg_val_y, &mcsc_fields[MCSC_F_PC0_BCHS_Y_CLAMP_MIN], y_min);
		reg_val_c = is_hw_set_field_value(reg_val_c, &mcsc_fields[MCSC_F_PC0_BCHS_C_CLAMP_MAX], c_max);
		reg_val_c = is_hw_set_field_value(reg_val_c, &mcsc_fields[MCSC_F_PC0_BCHS_C_CLAMP_MIN], c_min);
		reg_idx_y = MCSC_R_PC0_BCHS_CLAMP_Y;
		reg_idx_c = MCSC_R_PC0_BCHS_CLAMP_C;
		break;
	case MCSC_OUTPUT1:
		reg_val_y = is_hw_set_field_value(reg_val_y, &mcsc_fields[MCSC_F_PC1_BCHS_Y_CLAMP_MAX], y_max);
		reg_val_y = is_hw_set_field_value(reg_val_y, &mcsc_fields[MCSC_F_PC1_BCHS_Y_CLAMP_MIN], y_min);
		reg_val_c = is_hw_set_field_value(reg_val_c, &mcsc_fields[MCSC_F_PC1_BCHS_C_CLAMP_MAX], c_max);
		reg_val_c = is_hw_set_field_value(reg_val_c, &mcsc_fields[MCSC_F_PC1_BCHS_C_CLAMP_MIN], c_min);
		reg_idx_y = MCSC_R_PC1_BCHS_CLAMP_Y;
		reg_idx_c = MCSC_R_PC1_BCHS_CLAMP_C;
		break;
	case MCSC_OUTPUT2:
		reg_val_y = is_hw_set_field_value(reg_val_y, &mcsc_fields[MCSC_F_PC2_BCHS_Y_CLAMP_MAX], y_max);
		reg_val_y = is_hw_set_field_value(reg_val_y, &mcsc_fields[MCSC_F_PC2_BCHS_Y_CLAMP_MIN], y_min);
		reg_val_c = is_hw_set_field_value(reg_val_c, &mcsc_fields[MCSC_F_PC2_BCHS_C_CLAMP_MAX], c_max);
		reg_val_c = is_hw_set_field_value(reg_val_c, &mcsc_fields[MCSC_F_PC2_BCHS_C_CLAMP_MIN], c_min);
		reg_idx_y = MCSC_R_PC2_BCHS_CLAMP_Y;
		reg_idx_c = MCSC_R_PC2_BCHS_CLAMP_C;
		break;
	case MCSC_OUTPUT3:
		reg_val_y = is_hw_set_field_value(reg_val_y, &mcsc_fields[MCSC_F_PC3_BCHS_Y_CLAMP_MAX], y_max);
		reg_val_y = is_hw_set_field_value(reg_val_y, &mcsc_fields[MCSC_F_PC3_BCHS_Y_CLAMP_MIN], y_min);
		reg_val_c = is_hw_set_field_value(reg_val_c, &mcsc_fields[MCSC_F_PC3_BCHS_C_CLAMP_MAX], c_max);
		reg_val_c = is_hw_set_field_value(reg_val_c, &mcsc_fields[MCSC_F_PC3_BCHS_C_CLAMP_MIN], c_min);
		reg_idx_y = MCSC_R_PC3_BCHS_CLAMP_Y;
		reg_idx_c = MCSC_R_PC3_BCHS_CLAMP_C;
		break;
	case MCSC_OUTPUT4:
		reg_val_y = is_hw_set_field_value(reg_val_y, &mcsc_fields[MCSC_F_PC4_BCHS_Y_CLAMP_MAX], y_max);
		reg_val_y = is_hw_set_field_value(reg_val_y, &mcsc_fields[MCSC_F_PC4_BCHS_Y_CLAMP_MIN], y_min);
		reg_val_c = is_hw_set_field_value(reg_val_c, &mcsc_fields[MCSC_F_PC4_BCHS_C_CLAMP_MAX], c_max);
		reg_val_c = is_hw_set_field_value(reg_val_c, &mcsc_fields[MCSC_F_PC4_BCHS_C_CLAMP_MIN], c_min);
		reg_idx_y = MCSC_R_PC4_BCHS_CLAMP_Y;
		reg_idx_c = MCSC_R_PC4_BCHS_CLAMP_C;
		break;
	default:
		return;
	}

	is_hw_set_reg(base_addr, &mcsc_regs[reg_idx_y], reg_val_y);
	is_hw_set_reg(base_addr, &mcsc_regs[reg_idx_c], reg_val_c);
	dbg_hw(2, "[OUT:%d]set_bchs_clamp: Y:[%#x]%#x, C:[%#x]%#x\n", output_id,
			mcsc_regs[reg_idx_y].sfr_offset, is_hw_get_reg(base_addr, &mcsc_regs[reg_idx_y]),
			mcsc_regs[reg_idx_c].sfr_offset, is_hw_get_reg(base_addr, &mcsc_regs[reg_idx_c]));
}

void is_scaler_set_rdma_enable(void __iomem *base_addr, u32 hw_id, bool dma_in_en)
{
	/* not support */
}

void is_scaler_set_dma_out_enable(void __iomem *base_addr, u32 output_id, bool dma_out_en)
{
	switch (output_id) {
	case MCSC_OUTPUT0:
		is_hw_set_field(base_addr, &mcsc_regs[MCSC_R_PC0_DMA_OUT_CTRL],
			&mcsc_fields[MCSC_F_PC0_DMA_OUT_ENABLE], (u32)dma_out_en);
		break;
	case MCSC_OUTPUT1:
		is_hw_set_field(base_addr, &mcsc_regs[MCSC_R_PC1_DMA_OUT_CTRL],
			&mcsc_fields[MCSC_F_PC1_DMA_OUT_ENABLE], (u32)dma_out_en);
		break;
	case MCSC_OUTPUT2:
		is_hw_set_field(base_addr, &mcsc_regs[MCSC_R_PC2_DMA_OUT_CTRL],
			&mcsc_fields[MCSC_F_PC2_DMA_OUT_ENABLE], (u32)dma_out_en);
		break;
	case MCSC_OUTPUT3:
		is_hw_set_field(base_addr, &mcsc_regs[MCSC_R_PC3_DMA_OUT_CTRL],
			&mcsc_fields[MCSC_F_PC3_DMA_OUT_ENABLE], (u32)dma_out_en);
		break;
	case MCSC_OUTPUT4:
		is_hw_set_field(base_addr, &mcsc_regs[MCSC_R_PC4_DMA_OUT_CTRL],
			&mcsc_fields[MCSC_F_PC4_DMA_OUT_ENABLE], (u32)dma_out_en);
		break;
	default:
		break;
	}
}

void is_scaler_set_otf_out_enable(void __iomem *base_addr, u32 output_id, bool otf_out_en)
{
	/* not support at this version */
}

void is_scaler_set_wdma_pri(void __iomem *base_addr, u32 output_id, u32 plane)
{
	/* not support at this version */
}

void is_scaler_set_wdma_axi_pri(void __iomem *base_addr)
{
	/* not support at this version */
}

u32 is_scaler_get_dma_out_enable(void __iomem *base_addr, u32 output_id)
{
	int ret = 0;

	switch (output_id) {
	case MCSC_OUTPUT0:
		ret = is_hw_get_field(base_addr, &mcsc_regs[MCSC_R_PC0_DMA_OUT_CTRL],
			&mcsc_fields[MCSC_F_PC0_DMA_OUT_ENABLE]);
		break;
	case MCSC_OUTPUT1:
		ret = is_hw_get_field(base_addr, &mcsc_regs[MCSC_R_PC1_DMA_OUT_CTRL],
			&mcsc_fields[MCSC_F_PC1_DMA_OUT_ENABLE]);
		break;
	case MCSC_OUTPUT2:
		ret = is_hw_get_field(base_addr, &mcsc_regs[MCSC_R_PC2_DMA_OUT_CTRL],
			&mcsc_fields[MCSC_F_PC2_DMA_OUT_ENABLE]);
		break;
	case MCSC_OUTPUT3:
		ret = is_hw_get_field(base_addr, &mcsc_regs[MCSC_R_PC3_DMA_OUT_CTRL],
			&mcsc_fields[MCSC_F_PC3_DMA_OUT_ENABLE]);
		break;
	case MCSC_OUTPUT4:
		ret = is_hw_get_field(base_addr, &mcsc_regs[MCSC_R_PC4_DMA_OUT_CTRL],
			&mcsc_fields[MCSC_F_PC4_DMA_OUT_ENABLE]);
		break;
	default:
		break;
	}

	return ret;
}

u32 is_scaler_get_otf_out_enable(void __iomem *base_addr, u32 output_id)
{
	/* not support at this version */
	return 0;
}

void is_scaler_set_otf_out_path(void __iomem *base_addr, u32 output_id)
{
	/* not support */
}

u32 is_scaler_get_rdma_format(void __iomem *base_addr, u32 hw_id)
{
	/* not support */
	return 0;
}

void is_scaler_set_rdma_format(void __iomem *base_addr, u32 hw_id, u32 in_fmt)
{
	/* not support */
}

u32 is_scaler_get_mono_ctrl(void __iomem *base_addr, u32 hw_id)
{
	u32 mono = 0, fmt = 0;

	switch (hw_id) {
	case DEV_HW_MCSC0:
		mono = is_hw_get_field(base_addr, &mcsc_regs[MCSC_R_SCALER_MONO_CTRL_0],
			&mcsc_fields[MCSC_F_MONO_ENABLE_0]);
		break;
	default:
		warn_hw("invalid hw_id(%d) for MCSC api\n", hw_id);
		break;
	}

	if (mono)
		fmt = MCSC_MONO_Y8;

	return fmt;
}

void is_scaler_set_mono_ctrl(void __iomem *base_addr, u32 hw_id, u32 in_fmt)
{
	u32 reg_offset = MCSC_R_SCALER_MONO_CTRL_0, field_offset = MCSC_F_MONO_ENABLE_0, en = 0;

	switch (hw_id) {
	case DEV_HW_MCSC0:
		reg_offset = MCSC_R_SCALER_MONO_CTRL_0;
		field_offset = MCSC_F_MONO_ENABLE_0;
		break;
	default:
		warn_hw("invalid hw_id(%d) for MCSC api\n", hw_id);
		break;
	}

	if (in_fmt == MCSC_MONO_Y8)
		en = 1;

	is_hw_set_field(base_addr, &mcsc_regs[reg_offset], &mcsc_fields[field_offset], en);
}

void is_scaler_set_rdma_10bit_type(void __iomem *base_addr, u32 dma_in_10bit_type)
{
	/* not support */
}

void is_scaler_set_wdma_mono_ctrl(void __iomem *base_addr, u32 output_id, u32 fmt, bool en)
{
	u32 reg_val = 0;

	switch (output_id) {
	case MCSC_OUTPUT0:
		reg_val = is_hw_set_field_value(reg_val, &mcsc_fields[MCSC_F_WDMA0_MONO_EN], en);
		is_hw_set_reg(base_addr, &mcsc_regs[MCSC_R_WDMA0_MONO_CTRL], reg_val);
		break;
	case MCSC_OUTPUT1:
		reg_val = is_hw_set_field_value(reg_val, &mcsc_fields[MCSC_F_WDMA1_MONO_EN], en);
		is_hw_set_reg(base_addr, &mcsc_regs[MCSC_R_WDMA1_MONO_CTRL], reg_val);
		break;
	case MCSC_OUTPUT2:
		reg_val = is_hw_set_field_value(reg_val, &mcsc_fields[MCSC_F_WDMA2_MONO_EN], en);
		is_hw_set_reg(base_addr, &mcsc_regs[MCSC_R_WDMA2_MONO_CTRL], reg_val);
		break;
	case MCSC_OUTPUT3:
		reg_val = is_hw_set_field_value(reg_val, &mcsc_fields[MCSC_F_WDMA3_MONO_EN], en);
		is_hw_set_reg(base_addr, &mcsc_regs[MCSC_R_WDMA3_MONO_CTRL], reg_val);
		break;
	case MCSC_OUTPUT4:
		reg_val = is_hw_set_field_value(reg_val, &mcsc_fields[MCSC_F_WDMA4_MONO_EN], en);
		is_hw_set_reg(base_addr, &mcsc_regs[MCSC_R_WDMA4_MONO_CTRL], reg_val);
		break;
	default:
		break;
	}
}

void is_scaler_set_wdma_rgb_coefficient(void __iomem *base_addr)
{
	u32 reg_val = 0;

	/* this value is valid only YUV422 only.  2020 wide coefficient is used in JPEG, Pictures, preview. */
	reg_val = is_hw_set_field_value(reg_val, &mcsc_fields[MCSC_F_RGB_SRC_Y_OFFSET], 0);
	is_hw_set_reg(base_addr, &mcsc_regs[MCSC_R_WDMA_RGB_OFFSET], reg_val);

	reg_val = 0;
	reg_val = is_hw_set_field_value(reg_val, &mcsc_fields[MCSC_F_RGB_COEF_C00], 512);
	reg_val = is_hw_set_field_value(reg_val, &mcsc_fields[MCSC_F_RGB_COEF_C10], 0);
	is_hw_set_reg(base_addr, &mcsc_regs[MCSC_R_WDMA_RGB_COEF_0], reg_val);
	reg_val = 0;
	reg_val = is_hw_set_field_value(reg_val, &mcsc_fields[MCSC_F_RGB_COEF_C20], 738);
	reg_val = is_hw_set_field_value(reg_val, &mcsc_fields[MCSC_F_RGB_COEF_C01], 512);
	is_hw_set_reg(base_addr, &mcsc_regs[MCSC_R_WDMA_RGB_COEF_1], reg_val);
	reg_val = 0;
	reg_val = is_hw_set_field_value(reg_val, &mcsc_fields[MCSC_F_RGB_COEF_C11], -82);
	reg_val = is_hw_set_field_value(reg_val, &mcsc_fields[MCSC_F_RGB_COEF_C21], -286);
	is_hw_set_reg(base_addr, &mcsc_regs[MCSC_R_WDMA_RGB_COEF_2], reg_val);
	reg_val = 0;
	reg_val = is_hw_set_field_value(reg_val, &mcsc_fields[MCSC_F_RGB_COEF_C02], 512);
	reg_val = is_hw_set_field_value(reg_val, &mcsc_fields[MCSC_F_RGB_COEF_C12], 942);
	is_hw_set_reg(base_addr, &mcsc_regs[MCSC_R_WDMA_RGB_COEF_3], reg_val);
	is_hw_set_field(base_addr, &mcsc_regs[MCSC_R_WDMA_RGB_COEF_4],
		&mcsc_fields[MCSC_F_RGB_COEF_C22], 0);

}

void is_scaler_set_wdma_color_type(void __iomem *base_addr, u32 output_id, u32 color_type)
{
	switch (output_id) {
	case MCSC_OUTPUT0:
		is_hw_set_field(base_addr, &mcsc_regs[MCSC_R_WDMA0_COLOR_TYPE],
			&mcsc_fields[MCSC_F_WDMA0_COLOR_TYPE], color_type);
		break;
	case MCSC_OUTPUT1:
		is_hw_set_field(base_addr, &mcsc_regs[MCSC_R_WDMA1_COLOR_TYPE],
			&mcsc_fields[MCSC_F_WDMA1_COLOR_TYPE], color_type);
		break;
	case MCSC_OUTPUT2:
		is_hw_set_field(base_addr, &mcsc_regs[MCSC_R_WDMA2_COLOR_TYPE],
			&mcsc_fields[MCSC_F_WDMA2_COLOR_TYPE], color_type);
		break;
	case MCSC_OUTPUT3:
		is_hw_set_field(base_addr, &mcsc_regs[MCSC_R_WDMA3_COLOR_TYPE],
			&mcsc_fields[MCSC_F_WDMA3_COLOR_TYPE], color_type);
		break;
	case MCSC_OUTPUT4:
		is_hw_set_field(base_addr, &mcsc_regs[MCSC_R_WDMA4_COLOR_TYPE],
			&mcsc_fields[MCSC_F_WDMA4_COLOR_TYPE], color_type);
		break;
	default:
		break;
	}
}
void is_scaler_set_wdma_rgb_format(void __iomem *base_addr, u32 output_id, u32 out_fmt)
{
	switch (output_id) {
	case MCSC_OUTPUT0:
		is_hw_set_field(base_addr, &mcsc_regs[MCSC_R_WDMA0_RGB_FORMAT],
			&mcsc_fields[MCSC_F_WDMA0_RGB_FORMAT], out_fmt);
		break;
	case MCSC_OUTPUT1:
		is_hw_set_field(base_addr, &mcsc_regs[MCSC_R_WDMA1_RGB_FORMAT],
			&mcsc_fields[MCSC_F_WDMA1_RGB_FORMAT], out_fmt);
		break;
	case MCSC_OUTPUT2:
		is_hw_set_field(base_addr, &mcsc_regs[MCSC_R_WDMA2_RGB_FORMAT],
			&mcsc_fields[MCSC_F_WDMA2_RGB_FORMAT], out_fmt);
		break;
	case MCSC_OUTPUT3:
		is_hw_set_field(base_addr, &mcsc_regs[MCSC_R_WDMA3_RGB_FORMAT],
			&mcsc_fields[MCSC_F_WDMA3_RGB_FORMAT], out_fmt);
		break;
	case MCSC_OUTPUT4:
		is_hw_set_field(base_addr, &mcsc_regs[MCSC_R_WDMA4_RGB_FORMAT],
			&mcsc_fields[MCSC_F_WDMA4_RGB_FORMAT], out_fmt);
		break;
	default:
		break;
	}
}

void is_scaler_set_wdma_yuv_format(void __iomem *base_addr, u32 output_id, u32 out_fmt)
{
	switch (output_id) {
	case MCSC_OUTPUT0:
		is_hw_set_field(base_addr, &mcsc_regs[MCSC_R_WDMA0_YUV_FORMAT],
			&mcsc_fields[MCSC_F_WDMA0_YUV_FORMAT], out_fmt);
		break;
	case MCSC_OUTPUT1:
		is_hw_set_field(base_addr, &mcsc_regs[MCSC_R_WDMA1_YUV_FORMAT],
			&mcsc_fields[MCSC_F_WDMA1_YUV_FORMAT], out_fmt);
		break;
	case MCSC_OUTPUT2:
		is_hw_set_field(base_addr, &mcsc_regs[MCSC_R_WDMA2_YUV_FORMAT],
			&mcsc_fields[MCSC_F_WDMA2_YUV_FORMAT], out_fmt);
		break;
	case MCSC_OUTPUT3:
		is_hw_set_field(base_addr, &mcsc_regs[MCSC_R_WDMA3_YUV_FORMAT],
			&mcsc_fields[MCSC_F_WDMA3_YUV_FORMAT], out_fmt);
		break;
	case MCSC_OUTPUT4:
		is_hw_set_field(base_addr, &mcsc_regs[MCSC_R_WDMA4_YUV_FORMAT],
			&mcsc_fields[MCSC_F_WDMA4_YUV_FORMAT], out_fmt);
		break;
	default:
		break;
	}
}

void is_scaler_set_wdma_format(void __iomem *base_addr, u32 hw_id, u32 output_id, u32 plane, u32 out_fmt)
{
	u32 in_fmt;
	u32 color_type = 0;

	in_fmt = is_scaler_get_mono_ctrl(base_addr, hw_id);
	if (in_fmt == MCSC_MONO_Y8 && out_fmt != MCSC_MONO_Y8) {
		warn_hw("[ID:%d]Input format(MONO), out_format(%d) is changed to MONO format!\n",
			hw_id, out_fmt);
		out_fmt = MCSC_MONO_Y8;
	}

	/* color_type 0 : YUV , 1 : RGB */
	switch (out_fmt) {
	case MCSC_RGB_ARGB8888:
	case MCSC_RGB_BGRA8888:
	case MCSC_RGB_RGBA8888:
	case MCSC_RGB_ABGR8888:
	case MCSC_RGB_ABGR1010102:
	case MCSC_RGB_RGBA1010102:
		is_scaler_set_wdma_rgb_coefficient(base_addr);
		out_fmt = out_fmt - MCSC_RGB_RGBA8888 + 1;
		color_type = 1;
		break;
	default:
		color_type = 0;
		break;
	}

	/* 1 plane or RGB : mono ctrl should be disabled */
	if (plane == 1 || color_type == 1)
		is_scaler_set_wdma_mono_ctrl(base_addr, output_id, out_fmt, false);
	else if (out_fmt == MCSC_MONO_Y8)
		is_scaler_set_wdma_mono_ctrl(base_addr, output_id, out_fmt, true);

	is_scaler_set_wdma_color_type(base_addr, output_id, color_type);
	if (color_type)
		is_scaler_set_wdma_rgb_format(base_addr, output_id, out_fmt);
	else
		is_scaler_set_wdma_yuv_format(base_addr, output_id, out_fmt);

}

void is_scaler_set_wdma_dither(void __iomem *base_addr, u32 output_id,
	u32 fmt, u32 bitwidth, u32 plane, enum exynos_sensor_position sensor_position)
{
	u32 reg_val = 0;
	u32 round_en = 0;
	u32 dither_en = 0;

	if ((fmt == DMA_INOUT_FORMAT_RGB && bitwidth == DMA_INOUT_BIT_WIDTH_8BIT)
		|| (sensor_position == SENSOR_POSITION_FRONT)) {
		dither_en = 0;
		round_en = 1;
	} else if (bitwidth == DMA_INOUT_BIT_WIDTH_8BIT || plane == 4) {
		dither_en = 1;
		round_en = 0;
	} else {
		dither_en = 0;
		round_en = 0;
	}

	switch (output_id) {
	case MCSC_OUTPUT0:
		reg_val = is_hw_set_field_value(reg_val, &mcsc_fields[MCSC_F_WDMA0_DITHER_EN_Y], dither_en);
		reg_val = is_hw_set_field_value(reg_val, &mcsc_fields[MCSC_F_WDMA0_DITHER_EN_C], dither_en);
		reg_val = is_hw_set_field_value(reg_val, &mcsc_fields[MCSC_F_WDMA0_ROUND_EN], round_en);
		is_hw_set_reg(base_addr, &mcsc_regs[MCSC_R_WDMA0_DITHER], reg_val);
		break;
	case MCSC_OUTPUT1:
		reg_val = is_hw_set_field_value(reg_val, &mcsc_fields[MCSC_F_WDMA1_DITHER_EN_Y], dither_en);
		reg_val = is_hw_set_field_value(reg_val, &mcsc_fields[MCSC_F_WDMA1_DITHER_EN_C], dither_en);
		reg_val = is_hw_set_field_value(reg_val, &mcsc_fields[MCSC_F_WDMA1_ROUND_EN], round_en);
		is_hw_set_reg(base_addr, &mcsc_regs[MCSC_R_WDMA1_DITHER], reg_val);
		break;
	case MCSC_OUTPUT2:
		reg_val = is_hw_set_field_value(reg_val, &mcsc_fields[MCSC_F_WDMA2_DITHER_EN_Y], dither_en);
		reg_val = is_hw_set_field_value(reg_val, &mcsc_fields[MCSC_F_WDMA2_DITHER_EN_C], dither_en);
		reg_val = is_hw_set_field_value(reg_val, &mcsc_fields[MCSC_F_WDMA2_ROUND_EN], round_en);
		is_hw_set_reg(base_addr, &mcsc_regs[MCSC_R_WDMA2_DITHER], reg_val);
		break;
	case MCSC_OUTPUT3:
		reg_val = is_hw_set_field_value(reg_val, &mcsc_fields[MCSC_F_WDMA3_DITHER_EN_Y], dither_en);
		reg_val = is_hw_set_field_value(reg_val, &mcsc_fields[MCSC_F_WDMA3_DITHER_EN_C], dither_en);
		reg_val = is_hw_set_field_value(reg_val, &mcsc_fields[MCSC_F_WDMA3_ROUND_EN], round_en);
		is_hw_set_reg(base_addr, &mcsc_regs[MCSC_R_WDMA3_DITHER], reg_val);
		break;
	case MCSC_OUTPUT4:
		reg_val = is_hw_set_field_value(reg_val, &mcsc_fields[MCSC_F_WDMA4_DITHER_EN_Y], dither_en);
		reg_val = is_hw_set_field_value(reg_val, &mcsc_fields[MCSC_F_WDMA4_DITHER_EN_C], dither_en);
		reg_val = is_hw_set_field_value(reg_val, &mcsc_fields[MCSC_F_WDMA4_ROUND_EN], round_en);
		is_hw_set_reg(base_addr, &mcsc_regs[MCSC_R_WDMA4_DITHER], reg_val);
		break;
	default:
		break;
	}

}

void is_scaler_get_wdma_format(void __iomem *base_addr, u32 output_id, u32 color_type, u32 *out_fmt)
{
	/* color_type 0 : YUV, 1 : RGB */
	switch (output_id) {
	case MCSC_OUTPUT0:
		*out_fmt = is_hw_get_field(base_addr, &mcsc_regs[MCSC_R_WDMA0_YUV_FORMAT + color_type],
			&mcsc_fields[MCSC_F_WDMA0_YUV_FORMAT + color_type]);
		break;
	case MCSC_OUTPUT1:
		*out_fmt = is_hw_get_field(base_addr, &mcsc_regs[MCSC_R_WDMA1_YUV_FORMAT + color_type],
			&mcsc_fields[MCSC_F_WDMA1_YUV_FORMAT + color_type]);
		break;
	case MCSC_OUTPUT2:
		*out_fmt = is_hw_get_field(base_addr, &mcsc_regs[MCSC_R_WDMA2_YUV_FORMAT + color_type],
			&mcsc_fields[MCSC_F_WDMA2_YUV_FORMAT + color_type]);
		break;
	case MCSC_OUTPUT3:
		*out_fmt = is_hw_get_field(base_addr, &mcsc_regs[MCSC_R_WDMA3_YUV_FORMAT + color_type],
			&mcsc_fields[MCSC_F_WDMA3_YUV_FORMAT + color_type]);
		break;
	case MCSC_OUTPUT4:
		*out_fmt = is_hw_get_field(base_addr, &mcsc_regs[MCSC_R_WDMA4_YUV_FORMAT + color_type],
			&mcsc_fields[MCSC_F_WDMA4_YUV_FORMAT + color_type]);
		break;
	default:
		break;
	}
}

void is_scaler_set_swap_mode(void __iomem *base_addr, u32 output_id, u32 swap)
{
	switch (output_id) {
	case MCSC_OUTPUT0:
		is_hw_set_field(base_addr, &mcsc_regs[MCSC_R_WDMA0_SWAP_EN],
			&mcsc_fields[MCSC_F_WDMA0_SWAP_EN], swap);
		break;
	case MCSC_OUTPUT1:
		is_hw_set_field(base_addr, &mcsc_regs[MCSC_R_WDMA1_SWAP_EN],
			&mcsc_fields[MCSC_F_WDMA1_SWAP_EN], swap);
		break;
	case MCSC_OUTPUT2:
		is_hw_set_field(base_addr, &mcsc_regs[MCSC_R_WDMA2_SWAP_EN],
			&mcsc_fields[MCSC_F_WDMA2_SWAP_EN], swap);
		break;
	case MCSC_OUTPUT3:
		is_hw_set_field(base_addr, &mcsc_regs[MCSC_R_WDMA3_SWAP_EN],
			&mcsc_fields[MCSC_F_WDMA3_SWAP_EN], swap);
		break;
	case MCSC_OUTPUT4:
		is_hw_set_field(base_addr, &mcsc_regs[MCSC_R_WDMA4_SWAP_EN],
			&mcsc_fields[MCSC_F_WDMA4_SWAP_EN], swap);
		break;
	default:
		break;
	}
}

void is_scaler_set_max_mo(void __iomem *base_addr, u32 output_id, u32 mo)
{
	switch (output_id) {
	case MCSC_OUTPUT0:
		is_hw_set_field(base_addr, &mcsc_regs[MCSC_R_WDMA0_MAX_MO],
			&mcsc_fields[MCSC_F_WDMA0_MAX_MO], mo);
		break;
	case MCSC_OUTPUT1:
		is_hw_set_field(base_addr, &mcsc_regs[MCSC_R_WDMA1_MAX_MO],
			&mcsc_fields[MCSC_F_WDMA1_MAX_MO], mo);
		break;
	case MCSC_OUTPUT2:
		is_hw_set_field(base_addr, &mcsc_regs[MCSC_R_WDMA2_MAX_MO],
			&mcsc_fields[MCSC_F_WDMA2_MAX_MO], mo);
		break;
	case MCSC_OUTPUT3:
		is_hw_set_field(base_addr, &mcsc_regs[MCSC_R_WDMA3_MAX_MO],
			&mcsc_fields[MCSC_F_WDMA3_MAX_MO], mo);
		break;
	case MCSC_OUTPUT4:
		is_hw_set_field(base_addr, &mcsc_regs[MCSC_R_WDMA4_MAX_MO],
			&mcsc_fields[MCSC_F_WDMA4_MAX_MO], mo);
		break;
	default:
		break;
	}
}

void is_scaler_set_flip_mode(void __iomem *base_addr, u32 output_id, u32 flip)
{
	switch (output_id) {
	case MCSC_OUTPUT0:
		is_hw_set_field(base_addr, &mcsc_regs[MCSC_R_WDMA0_FLIP_CONTROL],
			&mcsc_fields[MCSC_F_WDMA0_FLIP_CONTROL], flip);
		break;
	case MCSC_OUTPUT1:
		is_hw_set_field(base_addr, &mcsc_regs[MCSC_R_WDMA1_FLIP_CONTROL],
			&mcsc_fields[MCSC_F_WDMA1_FLIP_CONTROL], flip);
		break;
	case MCSC_OUTPUT2:
		is_hw_set_field(base_addr, &mcsc_regs[MCSC_R_WDMA2_FLIP_CONTROL],
			&mcsc_fields[MCSC_F_WDMA2_FLIP_CONTROL], flip);
		break;
	case MCSC_OUTPUT3:
		is_hw_set_field(base_addr, &mcsc_regs[MCSC_R_WDMA3_FLIP_CONTROL],
			&mcsc_fields[MCSC_F_WDMA3_FLIP_CONTROL], flip);
		break;
	case MCSC_OUTPUT4:
		is_hw_set_field(base_addr, &mcsc_regs[MCSC_R_WDMA4_FLIP_CONTROL],
			&mcsc_fields[MCSC_F_WDMA4_FLIP_CONTROL], flip);
		break;
	default:
		break;
	}
}

void is_scaler_set_rdma_size(void __iomem *base_addr, u32 width, u32 height)
{
	/* not support */
}

void is_scaler_get_rdma_size(void __iomem *base_addr, u32 *width, u32 *height)
{
	/* not support */
}

void is_scaler_set_wdma_size(void __iomem *base_addr, u32 output_id, u32 width, u32 height)
{
	u32 reg_val = 0;

	switch (output_id) {
	case MCSC_OUTPUT0:
		reg_val = is_hw_set_field_value(reg_val, &mcsc_fields[MCSC_F_WDMA0_WIDTH], width);
		reg_val = is_hw_set_field_value(reg_val, &mcsc_fields[MCSC_F_WDMA0_HEIGHT], height);
		is_hw_set_reg(base_addr, &mcsc_regs[MCSC_R_WDMA0_IMG_SIZE], reg_val);
		break;
	case MCSC_OUTPUT1:
		reg_val = is_hw_set_field_value(reg_val, &mcsc_fields[MCSC_F_WDMA1_WIDTH], width);
		reg_val = is_hw_set_field_value(reg_val, &mcsc_fields[MCSC_F_WDMA1_HEIGHT], height);
		is_hw_set_reg(base_addr, &mcsc_regs[MCSC_R_WDMA1_IMG_SIZE], reg_val);
		break;
	case MCSC_OUTPUT2:
		reg_val = is_hw_set_field_value(reg_val, &mcsc_fields[MCSC_F_WDMA2_WIDTH], width);
		reg_val = is_hw_set_field_value(reg_val, &mcsc_fields[MCSC_F_WDMA2_HEIGHT], height);
		is_hw_set_reg(base_addr, &mcsc_regs[MCSC_R_WDMA2_IMG_SIZE], reg_val);
		break;
	case MCSC_OUTPUT3:
		reg_val = is_hw_set_field_value(reg_val, &mcsc_fields[MCSC_F_WDMA3_WIDTH], width);
		reg_val = is_hw_set_field_value(reg_val, &mcsc_fields[MCSC_F_WDMA3_HEIGHT], height);
		is_hw_set_reg(base_addr, &mcsc_regs[MCSC_R_WDMA3_IMG_SIZE], reg_val);
		break;
	case MCSC_OUTPUT4:
		reg_val = is_hw_set_field_value(reg_val, &mcsc_fields[MCSC_F_WDMA4_WIDTH], width);
		reg_val = is_hw_set_field_value(reg_val, &mcsc_fields[MCSC_F_WDMA4_HEIGHT], height);
		is_hw_set_reg(base_addr, &mcsc_regs[MCSC_R_WDMA4_IMG_SIZE], reg_val);
		break;
	default:
		break;
	}
}

void is_scaler_get_wdma_size(void __iomem *base_addr, u32 output_id, u32 *width, u32 *height)
{
	switch (output_id) {
	case MCSC_OUTPUT0:
		*width = is_hw_get_field(base_addr, &mcsc_regs[MCSC_R_WDMA0_IMG_SIZE],
			&mcsc_fields[MCSC_F_WDMA0_WIDTH]);
		*height = is_hw_get_field(base_addr, &mcsc_regs[MCSC_R_WDMA0_IMG_SIZE],
			&mcsc_fields[MCSC_F_WDMA0_HEIGHT]);
		break;
	case MCSC_OUTPUT1:
		*width = is_hw_get_field(base_addr, &mcsc_regs[MCSC_R_WDMA1_IMG_SIZE],
			&mcsc_fields[MCSC_F_WDMA1_WIDTH]);
		*height = is_hw_get_field(base_addr, &mcsc_regs[MCSC_R_WDMA1_IMG_SIZE],
			&mcsc_fields[MCSC_F_WDMA1_HEIGHT]);
		break;
	case MCSC_OUTPUT2:
		*width = is_hw_get_field(base_addr, &mcsc_regs[MCSC_R_WDMA2_IMG_SIZE],
			&mcsc_fields[MCSC_F_WDMA2_WIDTH]);
		*height = is_hw_get_field(base_addr, &mcsc_regs[MCSC_R_WDMA2_IMG_SIZE],
			&mcsc_fields[MCSC_F_WDMA2_HEIGHT]);
		break;
	case MCSC_OUTPUT3:
		*width = is_hw_get_field(base_addr, &mcsc_regs[MCSC_R_WDMA3_IMG_SIZE],
			&mcsc_fields[MCSC_F_WDMA3_WIDTH]);
		*height = is_hw_get_field(base_addr, &mcsc_regs[MCSC_R_WDMA3_IMG_SIZE],
			&mcsc_fields[MCSC_F_WDMA3_HEIGHT]);
		break;
	case MCSC_OUTPUT4:
		*width = is_hw_get_field(base_addr, &mcsc_regs[MCSC_R_WDMA4_IMG_SIZE],
			&mcsc_fields[MCSC_F_WDMA4_WIDTH]);
		*height = is_hw_get_field(base_addr, &mcsc_regs[MCSC_R_WDMA4_IMG_SIZE],
			&mcsc_fields[MCSC_F_WDMA4_HEIGHT]);
		break;
	default:
		break;
	}
}

void is_scaler_set_rdma_stride(void __iomem *base_addr, u32 y_stride, u32 uv_stride)
{
	/* not support */
}

void is_scaler_set_rdma_2bit_stride(void __iomem *base_addr, u32 y_2bit_stride, u32 uv_2bit_stride)
{
	/* not support */
}

void is_scaler_get_rdma_stride(void __iomem *base_addr, u32 *y_stride, u32 *uv_stride)
{
	/* not support */
}

void is_scaler_set_wdma_stride(void __iomem *base_addr, u32 output_id, u32 y_stride, u32 uv_stride)
{
	switch (output_id) {
	case MCSC_OUTPUT0:
		is_hw_set_field(base_addr, &mcsc_regs[MCSC_R_WDMA0_STRIDE_0],
			&mcsc_fields[MCSC_F_WDMA0_STRIDE_0], y_stride);
		is_hw_set_field(base_addr, &mcsc_regs[MCSC_R_WDMA0_STRIDE_1],
			&mcsc_fields[MCSC_F_WDMA0_STRIDE_1], uv_stride);
		is_hw_set_field(base_addr, &mcsc_regs[MCSC_R_WDMA0_STRIDE_2],
			&mcsc_fields[MCSC_F_WDMA0_STRIDE_2], uv_stride);
		break;
	case MCSC_OUTPUT1:
		is_hw_set_field(base_addr, &mcsc_regs[MCSC_R_WDMA1_STRIDE_0],
			&mcsc_fields[MCSC_F_WDMA1_STRIDE_0], y_stride);
		is_hw_set_field(base_addr, &mcsc_regs[MCSC_R_WDMA1_STRIDE_1],
			&mcsc_fields[MCSC_F_WDMA1_STRIDE_1], uv_stride);
		is_hw_set_field(base_addr, &mcsc_regs[MCSC_R_WDMA1_STRIDE_2],
			&mcsc_fields[MCSC_F_WDMA1_STRIDE_2], uv_stride);
		break;
	case MCSC_OUTPUT2:
		is_hw_set_field(base_addr, &mcsc_regs[MCSC_R_WDMA2_STRIDE_0],
			&mcsc_fields[MCSC_F_WDMA2_STRIDE_0], y_stride);
		is_hw_set_field(base_addr, &mcsc_regs[MCSC_R_WDMA2_STRIDE_1],
			&mcsc_fields[MCSC_F_WDMA2_STRIDE_1], uv_stride);
		is_hw_set_field(base_addr, &mcsc_regs[MCSC_R_WDMA2_STRIDE_2],
			&mcsc_fields[MCSC_F_WDMA2_STRIDE_2], uv_stride);

		break;
	case MCSC_OUTPUT3:
		is_hw_set_field(base_addr, &mcsc_regs[MCSC_R_WDMA3_STRIDE_0],
			&mcsc_fields[MCSC_F_WDMA3_STRIDE_0], y_stride);
		is_hw_set_field(base_addr, &mcsc_regs[MCSC_R_WDMA3_STRIDE_1],
			&mcsc_fields[MCSC_F_WDMA3_STRIDE_1], uv_stride);
		is_hw_set_field(base_addr, &mcsc_regs[MCSC_R_WDMA3_STRIDE_2],
			&mcsc_fields[MCSC_F_WDMA3_STRIDE_2], uv_stride);
		break;
	case MCSC_OUTPUT4:
		is_hw_set_field(base_addr, &mcsc_regs[MCSC_R_WDMA4_STRIDE_0],
			&mcsc_fields[MCSC_F_WDMA4_STRIDE_0], y_stride);
		is_hw_set_field(base_addr, &mcsc_regs[MCSC_R_WDMA4_STRIDE_1],
			&mcsc_fields[MCSC_F_WDMA4_STRIDE_1], uv_stride);
		is_hw_set_field(base_addr, &mcsc_regs[MCSC_R_WDMA4_STRIDE_2],
			&mcsc_fields[MCSC_F_WDMA4_STRIDE_2], uv_stride);
		break;
	default:
		break;
	}
}

void is_scaler_set_wdma_2bit_stride(void __iomem *base_addr, u32 output_id,
	u32 y_2bit_stride, u32 uv_2bit_stride)
{
	switch (output_id) {
	case MCSC_OUTPUT0:
		is_hw_set_field(base_addr, &mcsc_regs[MCSC_R_WDMA0_STRIDE_3],
			&mcsc_fields[MCSC_F_WDMA0_STRIDE_3], y_2bit_stride);
		is_hw_set_field(base_addr, &mcsc_regs[MCSC_R_WDMA0_STRIDE_4],
			&mcsc_fields[MCSC_F_WDMA0_STRIDE_4], uv_2bit_stride);
		break;
	case MCSC_OUTPUT1:
		is_hw_set_field(base_addr, &mcsc_regs[MCSC_R_WDMA1_STRIDE_3],
			&mcsc_fields[MCSC_F_WDMA1_STRIDE_3], y_2bit_stride);
		is_hw_set_field(base_addr, &mcsc_regs[MCSC_R_WDMA1_STRIDE_4],
			&mcsc_fields[MCSC_F_WDMA1_STRIDE_4], uv_2bit_stride);
		break;
	case MCSC_OUTPUT2:
		is_hw_set_field(base_addr, &mcsc_regs[MCSC_R_WDMA2_STRIDE_3],
			&mcsc_fields[MCSC_F_WDMA2_STRIDE_3], y_2bit_stride);
		is_hw_set_field(base_addr, &mcsc_regs[MCSC_R_WDMA2_STRIDE_4],
			&mcsc_fields[MCSC_F_WDMA2_STRIDE_4], uv_2bit_stride);
		break;
	case MCSC_OUTPUT3:
		is_hw_set_field(base_addr, &mcsc_regs[MCSC_R_WDMA3_STRIDE_3],
			&mcsc_fields[MCSC_F_WDMA3_STRIDE_3], y_2bit_stride);
		is_hw_set_field(base_addr, &mcsc_regs[MCSC_R_WDMA3_STRIDE_4],
			&mcsc_fields[MCSC_F_WDMA3_STRIDE_4], uv_2bit_stride);
		break;
	case MCSC_OUTPUT4:
		is_hw_set_field(base_addr, &mcsc_regs[MCSC_R_WDMA4_STRIDE_3],
			&mcsc_fields[MCSC_F_WDMA4_STRIDE_3], y_2bit_stride);
		is_hw_set_field(base_addr, &mcsc_regs[MCSC_R_WDMA4_STRIDE_4],
			&mcsc_fields[MCSC_F_WDMA4_STRIDE_4], uv_2bit_stride);
		break;
	default:
		break;
	}
}


void is_scaler_get_wdma_stride(void __iomem *base_addr, u32 output_id, u32 *y_stride, u32 *uv_stride)
{
	switch (output_id) {
	case MCSC_OUTPUT0:
		*y_stride = is_hw_get_field(base_addr, &mcsc_regs[MCSC_R_WDMA0_STRIDE_0],
			&mcsc_fields[MCSC_F_WDMA0_STRIDE_0]);
		*uv_stride = is_hw_get_field(base_addr, &mcsc_regs[MCSC_R_WDMA0_STRIDE_1],
			&mcsc_fields[MCSC_F_WDMA0_STRIDE_1]);
		break;
	case MCSC_OUTPUT1:
		*y_stride = is_hw_get_field(base_addr, &mcsc_regs[MCSC_R_WDMA1_STRIDE_0],
			&mcsc_fields[MCSC_F_WDMA1_STRIDE_0]);
		*uv_stride = is_hw_get_field(base_addr, &mcsc_regs[MCSC_R_WDMA1_STRIDE_1],
			&mcsc_fields[MCSC_F_WDMA1_STRIDE_1]);
		break;
	case MCSC_OUTPUT2:
		*y_stride = is_hw_get_field(base_addr, &mcsc_regs[MCSC_R_WDMA2_STRIDE_0],
			&mcsc_fields[MCSC_F_WDMA2_STRIDE_0]);
		*uv_stride = is_hw_get_field(base_addr, &mcsc_regs[MCSC_R_WDMA2_STRIDE_1],
			&mcsc_fields[MCSC_F_WDMA2_STRIDE_1]);
		break;
	case MCSC_OUTPUT3:
		*y_stride = is_hw_get_field(base_addr, &mcsc_regs[MCSC_R_WDMA3_STRIDE_0],
			&mcsc_fields[MCSC_F_WDMA3_STRIDE_0]);
		*uv_stride = is_hw_get_field(base_addr, &mcsc_regs[MCSC_R_WDMA3_STRIDE_1],
			&mcsc_fields[MCSC_F_WDMA3_STRIDE_1]);
		break;
	case MCSC_OUTPUT4:
		*y_stride = is_hw_get_field(base_addr, &mcsc_regs[MCSC_R_WDMA4_STRIDE_0],
			&mcsc_fields[MCSC_F_WDMA4_STRIDE_0]);
		*uv_stride = is_hw_get_field(base_addr, &mcsc_regs[MCSC_R_WDMA4_STRIDE_1],
			&mcsc_fields[MCSC_F_WDMA4_STRIDE_1]);
		break;
	default:
		break;
	}
}

void is_scaler_set_rdma_frame_seq(void __iomem *base_addr, u32 frame_seq)
{
	/* not support */
}

void is_scaler_set_wdma_frame_seq(void __iomem *base_addr, u32 output_id, u32 frame_seq)
{
	/* not support */
}

void is_scaler_set_rdma_addr(void __iomem *base_addr,
	u32 y_addr, u32 cb_addr, u32 cr_addr, int buf_index)
{
	/* not support */
}

void is_scaler_set_rdma_2bit_addr(void __iomem *base_addr,
	u32 y_2bit_addr, u32 cbcr_2bit_addr, int buf_index)
{
	/* not support */
}

void get_wdma_addr_arr(u32 output_id, u32 *addr)
{
	switch (output_id) {
	case MCSC_OUTPUT0:
		addr[0] = MCSC_R_WDMA0_BASE_ADDR_0;
		addr[1] = MCSC_R_WDMA0_BASE_ADDR_0_CNT1;
		addr[2] = MCSC_R_WDMA0_BASE_ADDR_0_CNT2;
		addr[3] = MCSC_R_WDMA0_BASE_ADDR_0_CNT3;
		addr[4] = MCSC_R_WDMA0_BASE_ADDR_0_CNT4;
		addr[5] = MCSC_R_WDMA0_BASE_ADDR_0_CNT5;
		addr[6] = MCSC_R_WDMA0_BASE_ADDR_0_CNT6;
		addr[7] = MCSC_R_WDMA0_BASE_ADDR_0_CNT7;
		break;
	case MCSC_OUTPUT1:
		addr[0] = MCSC_R_WDMA1_BASE_ADDR_0;
		addr[1] = MCSC_R_WDMA1_BASE_ADDR_0_CNT1;
		addr[2] = MCSC_R_WDMA1_BASE_ADDR_0_CNT2;
		addr[3] = MCSC_R_WDMA1_BASE_ADDR_0_CNT3;
		addr[4] = MCSC_R_WDMA1_BASE_ADDR_0_CNT4;
		addr[5] = MCSC_R_WDMA1_BASE_ADDR_0_CNT5;
		addr[6] = MCSC_R_WDMA1_BASE_ADDR_0_CNT6;
		addr[7] = MCSC_R_WDMA1_BASE_ADDR_0_CNT7;
		break;
	case MCSC_OUTPUT2:
		addr[0] = MCSC_R_WDMA2_BASE_ADDR_0;
		addr[1] = MCSC_R_WDMA2_BASE_ADDR_0_CNT1;
		addr[2] = MCSC_R_WDMA2_BASE_ADDR_0_CNT2;
		addr[3] = MCSC_R_WDMA2_BASE_ADDR_0_CNT3;
		addr[4] = MCSC_R_WDMA2_BASE_ADDR_0_CNT4;
		addr[5] = MCSC_R_WDMA2_BASE_ADDR_0_CNT5;
		addr[6] = MCSC_R_WDMA2_BASE_ADDR_0_CNT6;
		addr[7] = MCSC_R_WDMA2_BASE_ADDR_0_CNT7;
		break;
	case MCSC_OUTPUT3:
		addr[0] = MCSC_R_WDMA3_BASE_ADDR_0;
		addr[1] = MCSC_R_WDMA3_BASE_ADDR_0_CNT1;
		addr[2] = MCSC_R_WDMA3_BASE_ADDR_0_CNT2;
		addr[3] = MCSC_R_WDMA3_BASE_ADDR_0_CNT3;
		addr[4] = MCSC_R_WDMA3_BASE_ADDR_0_CNT4;
		addr[5] = MCSC_R_WDMA3_BASE_ADDR_0_CNT5;
		addr[6] = MCSC_R_WDMA3_BASE_ADDR_0_CNT6;
		addr[7] = MCSC_R_WDMA3_BASE_ADDR_0_CNT7;
		break;
	case MCSC_OUTPUT4:
		addr[0] = MCSC_R_WDMA4_BASE_ADDR_0;
		addr[1] = MCSC_R_WDMA4_BASE_ADDR_0_CNT1;
		addr[2] = MCSC_R_WDMA4_BASE_ADDR_0_CNT2;
		addr[3] = MCSC_R_WDMA4_BASE_ADDR_0_CNT3;
		addr[4] = MCSC_R_WDMA4_BASE_ADDR_0_CNT4;
		addr[5] = MCSC_R_WDMA4_BASE_ADDR_0_CNT5;
		addr[6] = MCSC_R_WDMA4_BASE_ADDR_0_CNT6;
		addr[7] = MCSC_R_WDMA4_BASE_ADDR_0_CNT7;
		break;
	default:
		panic("invalid output_id(%d)", output_id);
		break;
	}
}

void is_scaler_set_wdma_addr(void __iomem *base_addr, u32 output_id,
	u32 y_addr, u32 cb_addr, u32 cr_addr, int buf_index)
{
	u32 addr[8] = {0, };

	get_wdma_addr_arr(output_id, addr);
	if (!addr[0])
		return;

	is_hw_set_reg(base_addr, &mcsc_regs[addr[buf_index]], y_addr);
	is_hw_set_reg(base_addr, &mcsc_regs[addr[buf_index] + 1], cb_addr);
	is_hw_set_reg(base_addr, &mcsc_regs[addr[buf_index] + 2], cr_addr);
}

void is_scaler_set_wdma_2bit_addr(void __iomem *base_addr, u32 output_id,
	u32 y_2bit_addr, u32 cbcr_2bit_addr, int buf_index)
{
	u32 addr[8] = {0, };

	get_wdma_addr_arr(output_id, addr);
	if (!addr[0])
		return;

	is_hw_set_reg(base_addr, &mcsc_regs[addr[buf_index] + 3], y_2bit_addr);
	is_hw_set_reg(base_addr, &mcsc_regs[addr[buf_index] + 4], cbcr_2bit_addr);
}

void is_scaler_get_wdma_addr(void __iomem *base_addr, u32 output_id,
	u32 *y_addr, u32 *cb_addr, u32 *cr_addr, int buf_index)
{
	u32 addr[8] = {0, };

	get_wdma_addr_arr(output_id, addr);
	if (!addr[0])
		return;

	*y_addr = is_hw_get_reg(base_addr, &mcsc_regs[addr[buf_index]]);
	*cb_addr = is_hw_get_reg(base_addr, &mcsc_regs[addr[buf_index] + 1]);
	*cr_addr = is_hw_get_reg(base_addr, &mcsc_regs[addr[buf_index] + 2]);
}

void is_scaler_clear_rdma_addr(void __iomem *base_addr)
{
	/* not support */
}

void is_scaler_clear_wdma_addr(void __iomem *base_addr, u32 output_id)
{
	u32 addr[8] = {0, }, buf_index;

	get_wdma_addr_arr(output_id, addr);
	if (!addr[0])
		return;

	for (buf_index = 0; buf_index < 8; buf_index++) {
		is_hw_set_reg(base_addr, &mcsc_regs[addr[buf_index]], 0x0);
		is_hw_set_reg(base_addr, &mcsc_regs[addr[buf_index] + 1], 0x0);
		is_hw_set_reg(base_addr, &mcsc_regs[addr[buf_index] + 2], 0x0);

		/* DMA 2bit Y, CR address clear */
		is_hw_set_reg(base_addr, &mcsc_regs[addr[buf_index] + 3], 0x0);
		is_hw_set_reg(base_addr, &mcsc_regs[addr[buf_index] + 4], 0x0);
	}
}

/* for tdnr : Not supported in makalu */
void is_scaler_set_tdnr_rdma_addr(void __iomem *base_addr, enum tdnr_buf_type type,
	u32 y_addr, u32 cb_addr, u32 cr_addr, int buf_index)
{
	/* not supported */
}

void is_scaler_clear_tdnr_rdma_addr(void __iomem *base_addr, enum tdnr_buf_type type)
{
	/* not supported */
}

void is_scaler_set_tdnr_wdma_addr(void __iomem *base_addr, enum tdnr_buf_type type,
	u32 y_addr, u32 cb_addr, u32 cr_addr, int buf_index)
{
	/* not supported */
}

void is_scaler_get_tdnr_wdma_addr(void __iomem *base_addr, enum tdnr_buf_type type,
	u32 *y_addr, u32 *cb_addr, u32 *cr_addr, int buf_index)
{
	/* not supported */
}

void is_scaler_clear_tdnr_wdma_addr(void __iomem *base_addr, enum tdnr_buf_type type)
{
	/* not supported */
}

void is_scaler_set_tdnr_wdma_size(void __iomem *base_addr, enum tdnr_buf_type type, u32 width, u32 height)
{
	/* not supported */
}

void is_scaler_get_tdnr_wdma_size(void __iomem *base_addr, enum tdnr_buf_type type, u32 *width, u32 *height)
{
	/* not supported */
}

void is_scaler_set_tdnr_rdma_size(void __iomem *base_addr, enum tdnr_buf_type type, u32 width, u32 height)
{
	/* not supported */
}

void is_scaler_set_tdnr_rdma_format(void __iomem *base_addr, enum tdnr_buf_type type, u32 in_fmt)
{
	/* not supported */
}

void is_scaler_set_tdnr_wdma_format(void __iomem *base_addr, enum tdnr_buf_type type, u32 out_fmt)
{
	/* not supported */
}

void is_scaler_set_tdnr_rdma_stride(void __iomem *base_addr, enum tdnr_buf_type type,
	u32 y_stride, u32 uv_stride)
{
	/* not supported */
}

void is_scaler_get_tdnr_rdma_stride(void __iomem *base_addr, enum tdnr_buf_type type,
	u32 *y_stride, u32 *uv_stride)
{
	/* not supported */
}

void is_scaler_set_tdnr_wdma_stride(void __iomem *base_addr, enum tdnr_buf_type type,
	u32 y_stride, u32 uv_stride)
{
	/* not supported */
}

void is_scaler_get_tdnr_wdma_stride(void __iomem *base_addr, enum tdnr_buf_type type,
	u32 *y_stride, u32 *uv_stride)
{
	/* not supported */
}

void is_scaler_set_tdnr_wdma_sram_base(void __iomem *base_addr, enum tdnr_buf_type type)
{
	/* not supported */
}

void is_scaler_set_tdnr_wdma_enable(void __iomem *base_addr, enum tdnr_buf_type type, bool dma_out_en)
{
	/* not supported */
}

void is_scaler_set_tdnr_image_size(void __iomem *base_addr, u32 width, u32 height)
{
	/* not supported */
}

void is_scaler_set_tdnr_mode_select(void __iomem *base_addr, enum tdnr_mode mode)
{
	/* not supported */
}

void is_scaler_set_tdnr_rdma_start(void __iomem *base_addr, enum tdnr_mode mode)
{
	/* not supported */
}

void is_scaler_set_tdnr_first(void __iomem *base_addr, u32 tdnr_first)
{
	/* not supported */
}

void is_scaler_set_tdnr_yic_ctrl(void __iomem *base_addr, enum yic_mode yic_enable)
{
	/* not supported */

}

void is_scaler_set_tdnr_tuneset_general(void __iomem *base_addr, struct general_config config)
{
	/* not supported */
}

void is_scaler_set_tdnr_tuneset_yuvtable(void __iomem *base_addr, struct yuv_table_config config)
{
	/* not supported */
}

void is_scaler_set_tdnr_tuneset_temporal(void __iomem *base_addr,
	struct temporal_ni_dep_config dep_config,
	struct temporal_ni_indep_config indep_config)
{
	/* not supported */
}

void is_scaler_set_tdnr_tuneset_constant_lut_coeffs(void __iomem *base_addr, u32 *config)
{
	/* not supported */
}

void is_scaler_set_tdnr_tuneset_refine_control(void __iomem *base_addr,
	struct refine_control_config config)
{
	/* not supported */
}

void is_scaler_set_tdnr_tuneset_regional_feature(void __iomem *base_addr,
	struct regional_ni_dep_config dep_config,
	struct regional_ni_indep_config indep_config)
{
	/* not supported */
}

void is_scaler_set_tdnr_tuneset_spatial(void __iomem *base_addr,
	struct spatial_ni_dep_config dep_config,
	struct spatial_ni_indep_config indep_config)
{
	/* not supported */
}

/* for hwfc */
void is_scaler_set_hwfc_auto_clear(void __iomem *base_addr, u32 output_id, bool auto_clear)
{
	is_hw_set_field(base_addr, &mcsc_regs[MCSC_R_HWFC_ENABLE_AUTO_CLEAR],
			&mcsc_fields[MCSC_F_HWFC_ENABLE_AUTO_CLEAR], auto_clear);
}

void is_scaler_set_hwfc_idx_reset(void __iomem *base_addr, u32 output_id, bool reset)
{
	is_hw_set_field(base_addr, &mcsc_regs[MCSC_R_HWFC_INDEX_RESET],
			&mcsc_fields[MCSC_F_HWFC_INDEX_RESET], reset);
}

void is_scaler_set_hwfc_mode(void __iomem *base_addr, u32 hwfc_output_ids)
{
	u32 val = MCSC_HWFC_MODE_OFF;
	u32 read_val;

	if (hwfc_output_ids & (1 << MCSC_OUTPUT3))
		is_hw_set_field(base_addr, &mcsc_regs[MCSC_R_HWFC_FRAME_START_SELECT],
			&mcsc_fields[MCSC_F_HWFC_FRAME_START_SELECT], 0x0);

	read_val = is_hw_get_field(base_addr, &mcsc_regs[MCSC_R_HWFC_MODE],
			&mcsc_fields[MCSC_F_HWFC_MODE]);

	if ((hwfc_output_ids & (1 << MCSC_OUTPUT3)) && (hwfc_output_ids & (1 << MCSC_OUTPUT4))) {
		val = MCSC_HWFC_MODE_REGION_A_B_PORT;
	} else if (hwfc_output_ids & (1 << MCSC_OUTPUT3)) {
		val = MCSC_HWFC_MODE_REGION_A_PORT;
	} else if (hwfc_output_ids & (1 << MCSC_OUTPUT4)) {
		err_hw("set_hwfc_mode: invalid output_ids(0x%x)\n", hwfc_output_ids);
		return;
	}

	is_hw_set_field(base_addr, &mcsc_regs[MCSC_R_HWFC_MODE],
			&mcsc_fields[MCSC_F_HWFC_MODE], val);

	dbg_hw(2, "set_hwfc_mode: regs(0x%x)(0x%x), hwfc_ids(0x%x)\n",
		read_val, val, hwfc_output_ids);
}

void is_scaler_set_hwfc_config(void __iomem *base_addr,
	u32 output_id, u32 fmt, u32 plane, u32 dma_idx, u32 width, u32 height)
{
	u32 val = 0;
	u32 img_resol = width * height;
	u32 total_img_byte0 = 0;
	u32 total_img_byte1 = 0;
	u32 total_img_byte2 = 0;
	u32 total_width_byte0 = 0;
	u32 total_width_byte1 = 0;
	u32 total_width_byte2 = 0;
	enum is_mcsc_hwfc_format hwfc_fmt = 0;

	switch (fmt) {
	case DMA_INOUT_FORMAT_YUV422:
		switch (plane) {
		case 3:
			total_img_byte0 = img_resol;
			total_img_byte1 = img_resol >> 1;
			total_img_byte2 = img_resol >> 1;
			total_width_byte0 = width;
			total_width_byte1 = width >> 1;
			total_width_byte2 = width >> 1;
			break;
		case 2:
			total_img_byte0 = img_resol;
			total_img_byte1 = img_resol;
			total_width_byte0 = width;
			total_width_byte1 = width;
			break;
		case 1:
			total_img_byte0 = img_resol << 1;
			total_width_byte0 = width << 1;
			break;
		default:
			err_hw("invalid hwfc plane (%d, %d, %dx%d)",
				fmt, plane, width, height);
			return;
		}

		hwfc_fmt = MCSC_HWFC_FMT_YUV444_YUV422;
		break;
	case DMA_INOUT_FORMAT_YUV420:
		switch (plane) {
		case 3:
			total_img_byte0 = img_resol;
			total_img_byte1 = img_resol >> 2;
			total_img_byte2 = img_resol >> 2;
			total_width_byte0 = width;
			total_width_byte1 = width >> 2;
			total_width_byte2 = width >> 2;
			break;
		case 2:
			total_img_byte0 = img_resol;
			total_img_byte1 = img_resol >> 1;
			total_width_byte0 = width;
			total_width_byte1 = width >> 1;
			break;
		default:
			err_hw("invalid hwfc plane (%d, %d, %dx%d)",
				fmt, plane, width, height);
			return;
		}

		hwfc_fmt = MCSC_HWFC_FMT_YUV420;
		break;
	default:
		err_hw("invalid hwfc format (%d, %d, %dx%d)",
			fmt, plane, width, height);
		return;
	}

	switch (output_id) {
	case MCSC_OUTPUT0:
		break;
	case MCSC_OUTPUT1:
		break;
	case MCSC_OUTPUT2:
		break;
	case MCSC_OUTPUT3:
		val = is_hw_get_reg(base_addr, &mcsc_regs[MCSC_R_HWFC_CONFIG_IMAGE_A]);
		/* format */
		val = is_hw_set_field_value(val, &mcsc_fields[MCSC_F_HWFC_FORMAT_A], hwfc_fmt);

		/* plane */
		val = is_hw_set_field_value(val, &mcsc_fields[MCSC_F_HWFC_PLANE_A], plane);

		/* dma idx */
		val = is_hw_set_field_value(val, &mcsc_fields[MCSC_F_HWFC_ID0_A], dma_idx);
		val = is_hw_set_field_value(val, &mcsc_fields[MCSC_F_HWFC_ID1_A], dma_idx+1);
		val = is_hw_set_field_value(val, &mcsc_fields[MCSC_F_HWFC_ID2_A], dma_idx+2);

		is_hw_set_reg(base_addr, &mcsc_regs[MCSC_R_HWFC_CONFIG_IMAGE_A], val);

		/* total pixel/byte */
		is_hw_set_reg(base_addr, &mcsc_regs[MCSC_R_HWFC_TOTAL_IMAGE_BYTE0_A], total_img_byte0);
		is_hw_set_reg(base_addr, &mcsc_regs[MCSC_R_HWFC_TOTAL_IMAGE_BYTE1_A], total_img_byte1);
		is_hw_set_reg(base_addr, &mcsc_regs[MCSC_R_HWFC_TOTAL_IMAGE_BYTE2_A], total_img_byte2);
		is_hw_set_reg(base_addr, &mcsc_regs[MCSC_R_HWFC_TOTAL_WIDTH_BYTE0_A], total_width_byte0);
		is_hw_set_reg(base_addr, &mcsc_regs[MCSC_R_HWFC_TOTAL_WIDTH_BYTE1_A], total_width_byte1);
		is_hw_set_reg(base_addr, &mcsc_regs[MCSC_R_HWFC_TOTAL_WIDTH_BYTE2_A], total_width_byte2);
		break;
	case MCSC_OUTPUT4:
		val = is_hw_get_reg(base_addr, &mcsc_regs[MCSC_R_HWFC_CONFIG_IMAGE_B]);
		/* format */
		val = is_hw_set_field_value(val, &mcsc_fields[MCSC_F_HWFC_FORMAT_B], hwfc_fmt);

		/* plane */
		val = is_hw_set_field_value(val, &mcsc_fields[MCSC_F_HWFC_PLANE_B], plane);

		/* dma idx */
		val = is_hw_set_field_value(val, &mcsc_fields[MCSC_F_HWFC_ID0_B], dma_idx);
		val = is_hw_set_field_value(val, &mcsc_fields[MCSC_F_HWFC_ID1_B], dma_idx+1);
		val = is_hw_set_field_value(val, &mcsc_fields[MCSC_F_HWFC_ID2_B], dma_idx+2);

		is_hw_set_reg(base_addr, &mcsc_regs[MCSC_R_HWFC_CONFIG_IMAGE_B], val);

		/* total pixel/byte */
		is_hw_set_reg(base_addr, &mcsc_regs[MCSC_R_HWFC_TOTAL_IMAGE_BYTE0_B], total_img_byte0);
		is_hw_set_reg(base_addr, &mcsc_regs[MCSC_R_HWFC_TOTAL_IMAGE_BYTE1_B], total_img_byte1);
		is_hw_set_reg(base_addr, &mcsc_regs[MCSC_R_HWFC_TOTAL_IMAGE_BYTE2_B], total_img_byte2);
		is_hw_set_reg(base_addr, &mcsc_regs[MCSC_R_HWFC_TOTAL_WIDTH_BYTE0_B], total_width_byte0);
		is_hw_set_reg(base_addr, &mcsc_regs[MCSC_R_HWFC_TOTAL_WIDTH_BYTE1_B], total_width_byte1);
		is_hw_set_reg(base_addr, &mcsc_regs[MCSC_R_HWFC_TOTAL_WIDTH_BYTE2_B], total_width_byte2);
		break;
	default:
		break;
	}
}

u32 is_scaler_get_hwfc_idx_bin(void __iomem *base_addr, u32 output_id)
{
	u32 val = 0;

	switch (output_id) {
	case MCSC_OUTPUT0:
		break;
	case MCSC_OUTPUT1:
		break;
	case MCSC_OUTPUT2:
		break;
	case MCSC_OUTPUT3:
		val = is_hw_get_field(base_addr, &mcsc_regs[MCSC_R_HWFC_REGION_IDX_BIN],
			&mcsc_fields[MCSC_F_HWFC_REGION_IDX_BIN_A]);
		break;
	case MCSC_OUTPUT4:
		val = is_hw_get_field(base_addr, &mcsc_regs[MCSC_R_HWFC_REGION_IDX_BIN],
			&mcsc_fields[MCSC_F_HWFC_REGION_IDX_BIN_B]);
		break;
	default:
		break;
	}

	return val;
}

u32 is_scaler_get_hwfc_cur_idx(void __iomem *base_addr, u32 output_id)
{
	u32 val = 0;

	switch (output_id) {
	case MCSC_OUTPUT0:
		break;
	case MCSC_OUTPUT1:
		break;
	case MCSC_OUTPUT2:
		break;
	case MCSC_OUTPUT3:
		val = is_hw_get_field(base_addr, &mcsc_regs[MCSC_R_HWFC_CURR_REGION],
			&mcsc_fields[MCSC_F_HWFC_CURR_REGION_A]);
		break;
	case MCSC_OUTPUT4:
		 val = is_hw_get_field(base_addr, &mcsc_regs[MCSC_R_HWFC_CURR_REGION],
			&mcsc_fields[MCSC_F_HWFC_CURR_REGION_B]);
		break;
	default:
		break;
	}

	return val;
}

/* for DJAG */
void is_scaler_set_djag_enable(void __iomem *base_addr, u32 djag_enable)
{
	u32 reg_val = 0;

	reg_val = is_hw_get_reg(base_addr, &mcsc_regs[MCSC_R_DJAG0_CTRL]);
	reg_val = is_hw_set_field_value(reg_val, &mcsc_fields[MCSC_F_DJAG0_ENABLE], djag_enable);
	reg_val = is_hw_set_field_value(reg_val, &mcsc_fields[MCSC_F_DJAG0_PS_ENABLE], djag_enable);
	is_hw_set_reg(base_addr, &mcsc_regs[MCSC_R_DJAG0_CTRL], reg_val);
}

void is_scaler_set_djag_src_size(void __iomem *base_addr, u32 width, u32 height)
{
	u32 reg_val = 0;

	reg_val = is_hw_set_field_value(reg_val, &mcsc_fields[MCSC_F_DJAG0_INPUT_IMG_HSIZE], width);
	reg_val = is_hw_set_field_value(reg_val, &mcsc_fields[MCSC_F_DJAG0_INPUT_IMG_VSIZE], height);
	is_hw_set_reg(base_addr, &mcsc_regs[MCSC_R_DJAG0_IMG_SIZE], reg_val);

	reg_val = 0;
	reg_val = is_hw_set_field_value(reg_val, &mcsc_fields[MCSC_F_DJAG0_PS_SRC_HPOS], 0);
	reg_val = is_hw_set_field_value(reg_val, &mcsc_fields[MCSC_F_DJAG0_PS_SRC_VPOS], 0);
	is_hw_set_reg(base_addr, &mcsc_regs[MCSC_R_DJAG0_PS_SRC_POS], reg_val);

	reg_val = 0;
	reg_val = is_hw_set_field_value(reg_val, &mcsc_fields[MCSC_F_DJAG0_PS_SRC_HSIZE], width);
	reg_val = is_hw_set_field_value(reg_val, &mcsc_fields[MCSC_F_DJAG0_PS_SRC_VSIZE], height);
	is_hw_set_reg(base_addr, &mcsc_regs[MCSC_R_DJAG0_PS_SRC_SIZE], reg_val);
}

void is_scaler_set_djag_dst_size(void __iomem *base_addr, u32 width, u32 height)
{
	u32 reg_val = 0;

	reg_val = is_hw_set_field_value(reg_val, &mcsc_fields[MCSC_F_DJAG0_PS_DST_HSIZE], width);
	reg_val = is_hw_set_field_value(reg_val, &mcsc_fields[MCSC_F_DJAG0_PS_DST_VSIZE], height);
	is_hw_set_reg(base_addr, &mcsc_regs[MCSC_R_DJAG0_PS_DST_SIZE], reg_val);
}

void is_scaler_set_djag_scaling_ratio(void __iomem *base_addr, u32 hratio, u32 vratio)
{
	is_hw_set_field(base_addr, &mcsc_regs[MCSC_R_DJAG0_PS_H_RATIO],
		&mcsc_fields[MCSC_F_DJAG0_PS_H_RATIO], hratio);
	is_hw_set_field(base_addr, &mcsc_regs[MCSC_R_DJAG0_PS_V_RATIO],
		&mcsc_fields[MCSC_F_DJAG0_PS_V_RATIO], vratio);
}

void is_scaler_set_djag_init_phase_offset(void __iomem *base_addr, u32 h_offset, u32 v_offset)
{
	is_hw_set_field(base_addr, &mcsc_regs[MCSC_R_DJAG0_PS_H_INIT_PHASE_OFFSET],
		&mcsc_fields[MCSC_F_DJAG0_PS_H_INIT_PHASE_OFFSET], h_offset);
	is_hw_set_field(base_addr, &mcsc_regs[MCSC_R_DJAG0_PS_V_INIT_PHASE_OFFSET],
		&mcsc_fields[MCSC_F_DJAG0_PS_V_INIT_PHASE_OFFSET], v_offset);
}

void is_scaler_set_djag_round_mode(void __iomem *base_addr, u32 round_enable)
{
	is_hw_set_field(base_addr, &mcsc_regs[MCSC_R_DJAG0_PS_ROUND_MODE],
		&mcsc_fields[MCSC_F_DJAG0_PS_ROUND_MODE], round_enable);
}

void is_scaler_set_djag_tunning_param(void __iomem *base_addr,
	const struct djag_scaling_ratio_depended_config *djag_tune)
{
	u32 reg_val = 0;

	reg_val = is_hw_set_field_value(reg_val, &mcsc_fields[MCSC_F_DJAG0_XFILTER_DEJAGGING_WEIGHT0],
		djag_tune->xfilter_dejagging_coeff_cfg.xfilter_dejagging_weight0);
	reg_val = is_hw_set_field_value(reg_val, &mcsc_fields[MCSC_F_DJAG0_XFILTER_DEJAGGING_WEIGHT1],
		djag_tune->xfilter_dejagging_coeff_cfg.xfilter_dejagging_weight1);
	reg_val = is_hw_set_field_value(reg_val, &mcsc_fields[MCSC_F_DJAG0_XFILTER_HF_BOOST_WEIGHT],
		djag_tune->xfilter_dejagging_coeff_cfg.xfilter_hf_boost_weight);
	reg_val = is_hw_set_field_value(reg_val, &mcsc_fields[MCSC_F_DJAG0_CENTER_HF_BOOST_WEIGHT],
		djag_tune->xfilter_dejagging_coeff_cfg.center_hf_boost_weight);
	reg_val = is_hw_set_field_value(reg_val, &mcsc_fields[MCSC_F_DJAG0_DIAGONAL_HF_BOOST_WEIGHT],
		djag_tune->xfilter_dejagging_coeff_cfg.diagonal_hf_boost_weight);
	reg_val = is_hw_set_field_value(reg_val, &mcsc_fields[MCSC_F_DJAG0_CENTER_WEIGHTED_MEAN_WEIGHT],
		djag_tune->xfilter_dejagging_coeff_cfg.center_weighted_mean_weight);
	is_hw_set_reg(base_addr, &mcsc_regs[MCSC_R_DJAG0_XFILTER_DEJAGGING_COEFF], reg_val);

	reg_val = 0;
	reg_val = is_hw_set_field_value(reg_val, &mcsc_fields[MCSC_F_DJAG0_THRES_1X5_MATCHING_SAD],
		djag_tune->thres_1x5_matching_cfg.thres_1x5_matching_sad);
	reg_val = is_hw_set_field_value(reg_val, &mcsc_fields[MCSC_F_DJAG0_THRES_1X5_ABSHF],
		djag_tune->thres_1x5_matching_cfg.thres_1x5_abshf);
	is_hw_set_reg(base_addr, &mcsc_regs[MCSC_R_DJAG0_THRES_1X5_MATCHING], reg_val);

	reg_val = 0;
	reg_val = is_hw_set_field_value(reg_val, &mcsc_fields[MCSC_F_DJAG0_THRES_SHOOTING_LLCRR],
		djag_tune->thres_shooting_detect_cfg.thres_shooting_llcrr);
	reg_val = is_hw_set_field_value(reg_val, &mcsc_fields[MCSC_F_DJAG0_THRES_SHOOTING_LCR],
		djag_tune->thres_shooting_detect_cfg.thres_shooting_lcr);
	reg_val = is_hw_set_field_value(reg_val, &mcsc_fields[MCSC_F_DJAG0_THRES_SHOOTING_NEIGHBOR],
		djag_tune->thres_shooting_detect_cfg.thres_shooting_neighbor);
	is_hw_set_reg(base_addr, &mcsc_regs[MCSC_R_DJAG0_THRES_SHOOTING_DETECT_0], reg_val);

	reg_val = 0;
	reg_val = is_hw_set_field_value(reg_val, &mcsc_fields[MCSC_F_DJAG0_THRES_SHOOTING_UUCDD],
		djag_tune->thres_shooting_detect_cfg.thres_shooting_uucdd);
	reg_val = is_hw_set_field_value(reg_val, &mcsc_fields[MCSC_F_DJAG0_THRES_SHOOTING_UCD],
		djag_tune->thres_shooting_detect_cfg.thres_shooting_ucd);
	reg_val = is_hw_set_field_value(reg_val, &mcsc_fields[MCSC_F_DJAG0_MIN_MAX_WEIGHT],
		djag_tune->thres_shooting_detect_cfg.min_max_weight);
	is_hw_set_reg(base_addr, &mcsc_regs[MCSC_R_DJAG0_THRES_SHOOTING_DETECT_1], reg_val);

	is_hw_set_field(base_addr, &mcsc_regs[MCSC_R_DJAG0_LFSR_SEED_0], &mcsc_fields[MCSC_F_DJAG0_LFSR_SEED_0],
		djag_tune->lfsr_seed_cfg.lfsr_seed_0);
	is_hw_set_field(base_addr, &mcsc_regs[MCSC_R_DJAG0_LFSR_SEED_1], &mcsc_fields[MCSC_F_DJAG0_LFSR_SEED_1],
		djag_tune->lfsr_seed_cfg.lfsr_seed_1);
	is_hw_set_field(base_addr, &mcsc_regs[MCSC_R_DJAG0_LFSR_SEED_2], &mcsc_fields[MCSC_F_DJAG0_LFSR_SEED_2],
		djag_tune->lfsr_seed_cfg.lfsr_seed_2);

	reg_val = 0;
	reg_val = is_hw_set_field_value(reg_val, &mcsc_fields[MCSC_F_DJAG0_DITHER_VALUE_0],
		djag_tune->dither_cfg.dither_value[0]);
	reg_val = is_hw_set_field_value(reg_val, &mcsc_fields[MCSC_F_DJAG0_DITHER_VALUE_1],
		djag_tune->dither_cfg.dither_value[1]);
	reg_val = is_hw_set_field_value(reg_val, &mcsc_fields[MCSC_F_DJAG0_DITHER_VALUE_2],
		djag_tune->dither_cfg.dither_value[2]);
	reg_val = is_hw_set_field_value(reg_val, &mcsc_fields[MCSC_F_DJAG0_DITHER_VALUE_3],
		djag_tune->dither_cfg.dither_value[3]);
	reg_val = is_hw_set_field_value(reg_val, &mcsc_fields[MCSC_F_DJAG0_DITHER_VALUE_4],
		djag_tune->dither_cfg.dither_value[4]);
	is_hw_set_reg(base_addr, &mcsc_regs[MCSC_R_DJAG0_DITHER_VALUE_04], reg_val);

	reg_val = 0;
	reg_val = is_hw_set_field_value(reg_val, &mcsc_fields[MCSC_F_DJAG0_DITHER_VALUE_5],
		djag_tune->dither_cfg.dither_value[5]);
	reg_val = is_hw_set_field_value(reg_val, &mcsc_fields[MCSC_F_DJAG0_DITHER_VALUE_6],
		djag_tune->dither_cfg.dither_value[6]);
	reg_val = is_hw_set_field_value(reg_val, &mcsc_fields[MCSC_F_DJAG0_DITHER_VALUE_7],
		djag_tune->dither_cfg.dither_value[7]);
	reg_val = is_hw_set_field_value(reg_val, &mcsc_fields[MCSC_F_DJAG0_DITHER_VALUE_8],
		djag_tune->dither_cfg.dither_value[8]);
	is_hw_set_reg(base_addr, &mcsc_regs[MCSC_R_DJAG0_DITHER_VALUE_58], reg_val);

	reg_val = 0;
	reg_val = is_hw_set_field_value(reg_val, &mcsc_fields[MCSC_F_DJAG0_SAT_CTRL],
		djag_tune->dither_cfg.sat_ctrl);
	reg_val = is_hw_set_field_value(reg_val, &mcsc_fields[MCSC_F_DJAG0_DITHER_THRES],
		djag_tune->dither_cfg.dither_thres);
	is_hw_set_reg(base_addr, &mcsc_regs[MCSC_R_DJAG0_DITHER_THRES], reg_val);

	is_hw_set_field(base_addr, &mcsc_regs[MCSC_R_DJAG0_CP_HF_THRES], &mcsc_fields[MCSC_F_DJAG0_CP_HF_THRES],
		djag_tune->cp_cfg.cp_hf_thres);

	reg_val = 0;
	reg_val = is_hw_set_field_value(reg_val, &mcsc_fields[MCSC_F_DJAG0_CP_ARBI_MAX_COV_OFFSET],
		djag_tune->cp_cfg.cp_arbi_max_cov_offset);
	reg_val = is_hw_set_field_value(reg_val, &mcsc_fields[MCSC_F_DJAG0_CP_ARBI_MAX_COV_SHIFT],
		djag_tune->cp_cfg.cp_arbi_max_cov_shift);
	reg_val = is_hw_set_field_value(reg_val, &mcsc_fields[MCSC_F_DJAG0_CP_ARBI_DENOM],
		djag_tune->cp_cfg.cp_arbi_denom);
	reg_val = is_hw_set_field_value(reg_val, &mcsc_fields[MCSC_F_DJAG0_CP_ARBI_MODE],
		djag_tune->cp_cfg.cp_arbi_mode);
	is_hw_set_reg(base_addr, &mcsc_regs[MCSC_R_DJAG0_CP_ARBI], reg_val);
}

void is_scaler_set_djag_dither_wb(void __iomem *base_addr, struct djag_wb_thres_cfg *djag_wb, u32 wht, u32 blk)
{
	u32 reg_val = 0;

#if !defined(CONFIG_SOC_EXYNOS9820_EVT0)
	if (!djag_wb)
#endif
		return;

	reg_val = is_hw_get_reg(base_addr, &mcsc_regs[MCSC_R_DJAG0_DITHER_WB]);

	reg_val = is_hw_set_field_value(reg_val, &mcsc_fields[MCSC_F_DJAG0_DITHER_WHITE_LEVEL], wht);
	reg_val = is_hw_set_field_value(reg_val, &mcsc_fields[MCSC_F_DJAG0_DITHER_BLACK_LEVEL], blk);
	reg_val = is_hw_set_field_value(reg_val, &mcsc_fields[MCSC_F_DJAG0_DITHER_WB_THRES],
		djag_wb->dither_wb_thres);
	is_hw_set_reg(base_addr, &mcsc_regs[MCSC_R_DJAG0_DITHER_WB], reg_val);
}

/* for CAC */
void is_scaler_set_cac_enable(void __iomem *base_addr, u32 en)
{
	u32 reg_val = 0;

	reg_val = is_hw_get_reg(base_addr, &mcsc_regs[MCSC_R_CAC0_CTRL]);
	reg_val = is_hw_set_field_value(reg_val, &mcsc_fields[MCSC_F_CAC0_ENABLE], en);
	is_hw_set_reg(base_addr, &mcsc_regs[MCSC_R_CAC0_CTRL], reg_val);
}

void is_scaler_set_cac_map_crt_thr(void __iomem *base_addr, struct cac_cfg_by_ni *cfg)
{
	u32 reg_val = 0;

	reg_val = is_hw_set_field_value(reg_val, &mcsc_fields[MCSC_F_CAC0_MAP_SPOT_THR_L],
			cfg->map_thr_cfg.map_spot_thr_l);
	reg_val = is_hw_set_field_value(reg_val, &mcsc_fields[MCSC_F_CAC0_MAP_SPOT_THR_H],
			cfg->map_thr_cfg.map_spot_thr_h);
	reg_val = is_hw_set_field_value(reg_val, &mcsc_fields[MCSC_F_CAC0_MAP_SPOT_THR],
			cfg->map_thr_cfg.map_spot_thr);
	reg_val = is_hw_set_field_value(reg_val, &mcsc_fields[MCSC_F_CAC0_MAP_SPOT_NR_STRENGTH],
			cfg->map_thr_cfg.map_spot_nr_strength);
	is_hw_set_reg(base_addr, &mcsc_regs[MCSC_R_CAC0_MAP_THR], reg_val);


	reg_val = 0;
	reg_val = is_hw_set_field_value(reg_val, &mcsc_fields[MCSC_F_CAC0_CRT_COLOR_THR_L_DOT],
			cfg->crt_thr_cfg.crt_color_thr_l_dot);
	reg_val = is_hw_set_field_value(reg_val, &mcsc_fields[MCSC_F_CAC0_CRT_COLOR_THR_L_LINE],
			cfg->crt_thr_cfg.crt_color_thr_l_line);
	reg_val = is_hw_set_field_value(reg_val, &mcsc_fields[MCSC_F_CAC0_CRT_COLOR_THR_H],
			cfg->crt_thr_cfg.crt_color_thr_h);
	is_hw_set_reg(base_addr, &mcsc_regs[MCSC_R_CAC0_CRT_THR], reg_val);
}

/* LFRO : Less Fast Read Out */
void is_scaler_set_lfro_mode_enable(void __iomem *base_addr, u32 hw_id, u32 lfro_enable, u32 lfro_total_fnum)
{
	u32 reg_value = 0;

	switch (hw_id) {
	case DEV_HW_MCSC0:
		reg_value = is_hw_set_field_value(reg_value, &mcsc_fields[MCSC_F_FAST_MODE_NUM_MINUS1_0],
			lfro_total_fnum - 1);
		reg_value = is_hw_set_field_value(reg_value, &mcsc_fields[MCSC_F_FAST_MODE_EN_0],
			lfro_enable);
		is_hw_set_reg(base_addr, &mcsc_regs[MCSC_R_SCALER_FAST_MODE_CTRL_0], reg_value);
		break;
	default:
		warn_hw("invalid hw_id(%d) for MCSC api\n", hw_id);
		break;
	}
}

u32 is_scaler_get_lfro_mode_status(void __iomem *base_addr, u32 hw_id)
{
	u32 ret = 0;
	u32 fcnt = 0;

	switch (hw_id) {
	case DEV_HW_MCSC0:
		fcnt = is_hw_get_field(base_addr, &mcsc_regs[MCSC_R_SCALER_FAST_MODE_STATUS_0],
			&mcsc_fields[MCSC_F_FAST_MODE_FRAME_CNT_0]);
		ret = is_hw_get_field(base_addr, &mcsc_regs[MCSC_R_SCALER_FAST_MODE_STATUS_0],
			&mcsc_fields[MCSC_F_FAST_MODE_ERROR_STATUS_0]);
		break;
	default:
		warn_hw("invalid hw_id(%d) for MCSC api\n", hw_id);
		break;
	}

	if (ret)
		warn_hw("[FRO:%d]frame status: (0x%x)\n", fcnt, ret);

	return ret;
}

/* for Strip */

u32 is_scaler_get_djag_strip_enable(void __iomem *base_addr, u32 output_id)
{
	int ret = 0;

	ret = is_hw_get_field(base_addr, &mcsc_regs[MCSC_R_DJAG0_CTRL], &mcsc_fields[MCSC_F_DJAG0_PS_STRIP_ENABLE]);
	return ret;
}

void is_scaler_set_djag_strip_enable(void __iomem *base_addr, u32 output_id, u32 enable)
{
	is_hw_set_field(base_addr, &mcsc_regs[MCSC_R_DJAG0_CTRL], &mcsc_fields[MCSC_F_DJAG0_PS_STRIP_ENABLE], enable);
}

u32 is_scaler_get_djag_out_crop_enable(void __iomem *base_addr, u32 output_id)
{
	int ret = 0;

	ret = is_hw_get_field(base_addr, &mcsc_regs[MCSC_R_DJAG0_CTRL], &mcsc_fields[MCSC_F_DJAG0_OUT_CROP_ENABLE]);
	return ret;
}

void is_scaler_set_djag_out_crop_enable(void __iomem *base_addr, u32 output_id, u32 enable, u32 pre_dst_h, u32 start_pos_h)
{
	is_hw_set_field(base_addr, &mcsc_regs[MCSC_R_DJAG0_CTRL], &mcsc_fields[MCSC_F_DJAG0_OUT_CROP_ENABLE], enable);
}

void is_scaler_set_djag_out_crop_size(void __iomem *base_addr, u32 output_id,
	u32 pos_x, u32 pos_y, u32 width, u32 height)
{
	u32 reg_val = 0;

	reg_val = is_hw_set_field_value(reg_val, &mcsc_fields[MCSC_F_DJAG0_OUT_CROP_POS_H], pos_x);
	reg_val = is_hw_set_field_value(reg_val, &mcsc_fields[MCSC_F_DJAG0_OUT_CROP_POS_V], pos_y);
	is_hw_set_reg(base_addr, &mcsc_regs[MCSC_R_DJAG0_OUT_CROP_POS], reg_val);

	reg_val = 0;
	reg_val = is_hw_set_field_value(reg_val, &mcsc_fields[MCSC_F_DJAG0_OUT_CROP_SIZE_H], width);
	reg_val = is_hw_set_field_value(reg_val, &mcsc_fields[MCSC_F_DJAG0_OUT_CROP_SIZE_V], height);
	is_hw_set_reg(base_addr, &mcsc_regs[MCSC_R_DJAG0_OUT_CROP_SIZE], reg_val);
}

void is_scaler_get_djag_strip_config(void __iomem *base_addr, u32 output_id, u32 *pre_dst_h, u32 *start_pos_h)
{
	*pre_dst_h = is_hw_get_field(base_addr, &mcsc_regs[MCSC_R_DJAG0_PS_STRIP_PRE_DST_SIZE],
			&mcsc_fields[MCSC_F_DJAG0_PS_STRIP_PRE_DST_SIZE_H]);
	*start_pos_h = is_hw_get_field(base_addr, &mcsc_regs[MCSC_R_DJAG0_PS_STRIP_IN_START_POS],
			&mcsc_fields[MCSC_F_DJAG0_PS_STRIP_IN_START_POS_H]);
}

void is_scaler_set_djag_strip_config(void __iomem *base_addr, u32 output_id, u32 pre_dst_h, u32 start_pos_h)
{
	is_hw_set_field(base_addr, &mcsc_regs[MCSC_R_DJAG0_PS_STRIP_PRE_DST_SIZE], &mcsc_fields[MCSC_F_DJAG0_PS_STRIP_PRE_DST_SIZE_H], pre_dst_h);
	is_hw_set_field(base_addr, &mcsc_regs[MCSC_R_DJAG0_PS_STRIP_IN_START_POS], &mcsc_fields[MCSC_F_DJAG0_PS_STRIP_IN_START_POS_H], start_pos_h);
}

u32 is_scaler_get_poly_strip_enable(void __iomem *base_addr, u32 output_id)
{
	int ret = 0;

	switch (output_id) {
	case MCSC_OUTPUT0:
		ret = is_hw_get_field(base_addr, &mcsc_regs[MCSC_R_SC0_CTRL],
			&mcsc_fields[MCSC_F_SC0_STRIP_ENABLE]);
		break;
	case MCSC_OUTPUT1:
		ret = is_hw_get_field(base_addr, &mcsc_regs[MCSC_R_SC1_CTRL],
			&mcsc_fields[MCSC_F_SC1_STRIP_ENABLE]);
		break;
	case MCSC_OUTPUT2:
		ret = is_hw_get_field(base_addr, &mcsc_regs[MCSC_R_SC2_CTRL],
			&mcsc_fields[MCSC_F_SC2_STRIP_ENABLE]);
		break;
	case MCSC_OUTPUT3:
		ret = is_hw_get_field(base_addr, &mcsc_regs[MCSC_R_SC3_CTRL],
			&mcsc_fields[MCSC_F_SC3_STRIP_ENABLE]);
		break;
	case MCSC_OUTPUT4:
		ret = is_hw_get_field(base_addr, &mcsc_regs[MCSC_R_SC4_CTRL],
			&mcsc_fields[MCSC_F_SC4_STRIP_ENABLE]);
		break;
	default:
		break;
	}

	return ret;
}

void is_scaler_set_poly_strip_enable(void __iomem *base_addr, u32 output_id, u32 enable)
{
	switch (output_id) {
	case MCSC_OUTPUT0:
		is_hw_set_field(base_addr, &mcsc_regs[MCSC_R_SC0_CTRL], &mcsc_fields[MCSC_F_SC0_STRIP_ENABLE], enable);
		break;
	case MCSC_OUTPUT1:
		is_hw_set_field(base_addr, &mcsc_regs[MCSC_R_SC1_CTRL], &mcsc_fields[MCSC_F_SC1_STRIP_ENABLE], enable);
		break;
	case MCSC_OUTPUT2:
		is_hw_set_field(base_addr, &mcsc_regs[MCSC_R_SC2_CTRL], &mcsc_fields[MCSC_F_SC2_STRIP_ENABLE], enable);
		break;
	case MCSC_OUTPUT3:
		is_hw_set_field(base_addr, &mcsc_regs[MCSC_R_SC3_CTRL], &mcsc_fields[MCSC_F_SC3_STRIP_ENABLE], enable);
		break;
	case MCSC_OUTPUT4:
		is_hw_set_field(base_addr, &mcsc_regs[MCSC_R_SC4_CTRL], &mcsc_fields[MCSC_F_SC4_STRIP_ENABLE], enable);
		break;
	default:
		break;
	}
}

void is_scaler_get_poly_strip_config(void __iomem *base_addr, u32 output_id, u32 *pre_dst_h, u32 *start_pos_h)
{
	switch (output_id) {
	case MCSC_OUTPUT0:
		*pre_dst_h = is_hw_get_field(base_addr, &mcsc_regs[MCSC_R_SC0_STRIP_PRE_DST_SIZE],
			&mcsc_fields[MCSC_F_SC0_STRIP_PRE_DST_SIZE_H]);
		*start_pos_h = is_hw_get_field(base_addr, &mcsc_regs[MCSC_R_SC0_STRIP_IN_START_POS],
			&mcsc_fields[MCSC_F_SC0_STRIP_IN_START_POS_H]);
		break;
	case MCSC_OUTPUT1:
		*pre_dst_h = is_hw_get_field(base_addr, &mcsc_regs[MCSC_R_SC1_STRIP_PRE_DST_SIZE],
			&mcsc_fields[MCSC_F_SC1_STRIP_PRE_DST_SIZE_H]);
		*start_pos_h = is_hw_get_field(base_addr, &mcsc_regs[MCSC_R_SC1_STRIP_IN_START_POS],
			&mcsc_fields[MCSC_F_SC1_STRIP_IN_START_POS_H]);
		break;
	case MCSC_OUTPUT2:
		*pre_dst_h = is_hw_get_field(base_addr, &mcsc_regs[MCSC_R_SC2_STRIP_PRE_DST_SIZE],
			&mcsc_fields[MCSC_F_SC2_STRIP_PRE_DST_SIZE_H]);
		*start_pos_h = is_hw_get_field(base_addr, &mcsc_regs[MCSC_R_SC2_STRIP_IN_START_POS],
			&mcsc_fields[MCSC_F_SC2_STRIP_IN_START_POS_H]);
		break;
	case MCSC_OUTPUT3:
		*pre_dst_h = is_hw_get_field(base_addr, &mcsc_regs[MCSC_R_SC3_STRIP_PRE_DST_SIZE],
			&mcsc_fields[MCSC_F_SC3_STRIP_PRE_DST_SIZE_H]);
		*start_pos_h = is_hw_get_field(base_addr, &mcsc_regs[MCSC_R_SC3_STRIP_IN_START_POS],
			&mcsc_fields[MCSC_F_SC3_STRIP_IN_START_POS_H]);
		break;
	case MCSC_OUTPUT4:
		*pre_dst_h = is_hw_get_field(base_addr, &mcsc_regs[MCSC_R_SC4_STRIP_PRE_DST_SIZE],
			&mcsc_fields[MCSC_F_SC4_STRIP_PRE_DST_SIZE_H]);
		*start_pos_h = is_hw_get_field(base_addr, &mcsc_regs[MCSC_R_SC4_STRIP_IN_START_POS],
			&mcsc_fields[MCSC_F_SC4_STRIP_IN_START_POS_H]);
		break;
	default:
		break;
	}
}

void is_scaler_set_poly_strip_config(void __iomem *base_addr, u32 output_id, u32 pre_dst_h, u32 start_pos_h)
{
	switch (output_id) {
	case MCSC_OUTPUT0:
		is_hw_set_field(base_addr, &mcsc_regs[MCSC_R_SC0_STRIP_PRE_DST_SIZE], &mcsc_fields[MCSC_F_SC0_STRIP_PRE_DST_SIZE_H], pre_dst_h);
		is_hw_set_field(base_addr, &mcsc_regs[MCSC_R_SC0_STRIP_IN_START_POS], &mcsc_fields[MCSC_F_SC0_STRIP_IN_START_POS_H], start_pos_h);
		break;
	case MCSC_OUTPUT1:
		is_hw_set_field(base_addr, &mcsc_regs[MCSC_R_SC1_STRIP_PRE_DST_SIZE], &mcsc_fields[MCSC_F_SC1_STRIP_PRE_DST_SIZE_H], pre_dst_h);
		is_hw_set_field(base_addr, &mcsc_regs[MCSC_R_SC1_STRIP_IN_START_POS], &mcsc_fields[MCSC_F_SC1_STRIP_IN_START_POS_H], start_pos_h);
		break;
	case MCSC_OUTPUT2:
		is_hw_set_field(base_addr, &mcsc_regs[MCSC_R_SC2_STRIP_PRE_DST_SIZE], &mcsc_fields[MCSC_F_SC2_STRIP_PRE_DST_SIZE_H], pre_dst_h);
		is_hw_set_field(base_addr, &mcsc_regs[MCSC_R_SC2_STRIP_IN_START_POS], &mcsc_fields[MCSC_F_SC2_STRIP_IN_START_POS_H], start_pos_h);
		break;
	case MCSC_OUTPUT3:
		is_hw_set_field(base_addr, &mcsc_regs[MCSC_R_SC3_STRIP_PRE_DST_SIZE], &mcsc_fields[MCSC_F_SC3_STRIP_PRE_DST_SIZE_H], pre_dst_h);
		is_hw_set_field(base_addr, &mcsc_regs[MCSC_R_SC3_STRIP_IN_START_POS], &mcsc_fields[MCSC_F_SC3_STRIP_IN_START_POS_H], start_pos_h);
		break;
	case MCSC_OUTPUT4:
		is_hw_set_field(base_addr, &mcsc_regs[MCSC_R_SC4_STRIP_PRE_DST_SIZE], &mcsc_fields[MCSC_F_SC4_STRIP_PRE_DST_SIZE_H], pre_dst_h);
		is_hw_set_field(base_addr, &mcsc_regs[MCSC_R_SC4_STRIP_IN_START_POS], &mcsc_fields[MCSC_F_SC4_STRIP_IN_START_POS_H], start_pos_h);
		break;
	default:
		break;
	}
}

u32 is_scaler_get_poly_out_crop_enable(void __iomem *base_addr, u32 output_id)
{
	int ret = 0;

	switch (output_id) {
	case MCSC_OUTPUT0:
		ret = is_hw_get_field(base_addr, &mcsc_regs[MCSC_R_SC0_CTRL],
			&mcsc_fields[MCSC_F_SC0_OUT_CROP_ENABLE]);
		break;
	case MCSC_OUTPUT1:
		ret = is_hw_get_field(base_addr, &mcsc_regs[MCSC_R_SC1_CTRL],
			&mcsc_fields[MCSC_F_SC1_OUT_CROP_ENABLE]);
		break;
	case MCSC_OUTPUT2:
		ret = is_hw_get_field(base_addr, &mcsc_regs[MCSC_R_SC2_CTRL],
			&mcsc_fields[MCSC_F_SC2_OUT_CROP_ENABLE]);
		break;
	case MCSC_OUTPUT3:
		ret = is_hw_get_field(base_addr, &mcsc_regs[MCSC_R_SC3_CTRL],
			&mcsc_fields[MCSC_F_SC3_OUT_CROP_ENABLE]);
		break;
	case MCSC_OUTPUT4:
		ret = is_hw_get_field(base_addr, &mcsc_regs[MCSC_R_SC4_CTRL],
			&mcsc_fields[MCSC_F_SC4_OUT_CROP_ENABLE]);
		break;
	default:
		break;
	}

	return ret;
}

void is_scaler_set_poly_out_crop_enable(void __iomem *base_addr, u32 output_id, u32 enable)
{
	switch (output_id) {
	case MCSC_OUTPUT0:
		is_hw_set_field(base_addr, &mcsc_regs[MCSC_R_SC0_CTRL], &mcsc_fields[MCSC_F_SC0_OUT_CROP_ENABLE], enable);
		break;
	case MCSC_OUTPUT1:
		is_hw_set_field(base_addr, &mcsc_regs[MCSC_R_SC1_CTRL], &mcsc_fields[MCSC_F_SC1_OUT_CROP_ENABLE], enable);
		break;
	case MCSC_OUTPUT2:
		is_hw_set_field(base_addr, &mcsc_regs[MCSC_R_SC2_CTRL], &mcsc_fields[MCSC_F_SC2_OUT_CROP_ENABLE], enable);
		break;
	case MCSC_OUTPUT3:
		is_hw_set_field(base_addr, &mcsc_regs[MCSC_R_SC3_CTRL], &mcsc_fields[MCSC_F_SC3_OUT_CROP_ENABLE], enable);
		break;
	case MCSC_OUTPUT4:
		is_hw_set_field(base_addr, &mcsc_regs[MCSC_R_SC4_CTRL], &mcsc_fields[MCSC_F_SC4_OUT_CROP_ENABLE], enable);
		break;
	default:
		break;
	}
}

void is_scaler_get_poly_out_crop_size(void __iomem *base_addr, u32 output_id, u32 *width, u32 *height)
{
	switch (output_id) {
	case MCSC_OUTPUT0:
		*width = is_hw_get_field(base_addr, &mcsc_regs[MCSC_R_SC0_OUT_CROP_SIZE],
			&mcsc_fields[MCSC_F_SC0_OUT_CROP_SIZE_H]);
		*height = is_hw_get_field(base_addr, &mcsc_regs[MCSC_R_SC0_OUT_CROP_SIZE],
			&mcsc_fields[MCSC_F_SC0_OUT_CROP_SIZE_V]);
		break;
	case MCSC_OUTPUT1:
		*width = is_hw_get_field(base_addr, &mcsc_regs[MCSC_R_SC1_OUT_CROP_SIZE],
			&mcsc_fields[MCSC_F_SC1_OUT_CROP_SIZE_H]);
		*height = is_hw_get_field(base_addr, &mcsc_regs[MCSC_R_SC1_OUT_CROP_SIZE],
			&mcsc_fields[MCSC_F_SC1_OUT_CROP_SIZE_V]);
		break;
	case MCSC_OUTPUT2:
		*width = is_hw_get_field(base_addr, &mcsc_regs[MCSC_R_SC2_OUT_CROP_SIZE],
			&mcsc_fields[MCSC_F_SC2_OUT_CROP_SIZE_H]);
		*height = is_hw_get_field(base_addr, &mcsc_regs[MCSC_R_SC2_OUT_CROP_SIZE],
			&mcsc_fields[MCSC_F_SC2_OUT_CROP_SIZE_V]);
		break;
	case MCSC_OUTPUT3:
		*width = is_hw_get_field(base_addr, &mcsc_regs[MCSC_R_SC3_OUT_CROP_SIZE],
			&mcsc_fields[MCSC_F_SC3_OUT_CROP_SIZE_H]);
		*height = is_hw_get_field(base_addr, &mcsc_regs[MCSC_R_SC3_OUT_CROP_SIZE],
			&mcsc_fields[MCSC_F_SC3_OUT_CROP_SIZE_V]);
		break;
	case MCSC_OUTPUT4:
		*width = is_hw_get_field(base_addr, &mcsc_regs[MCSC_R_SC4_OUT_CROP_SIZE],
			&mcsc_fields[MCSC_F_SC4_OUT_CROP_SIZE_H]);
		*height = is_hw_get_field(base_addr, &mcsc_regs[MCSC_R_SC4_OUT_CROP_SIZE],
			&mcsc_fields[MCSC_F_SC4_OUT_CROP_SIZE_V]);
		break;
	default:
		break;
	}
}

void is_scaler_set_poly_out_crop_size(void __iomem *base_addr, u32 output_id,
	u32 pos_x, u32 pos_y, u32 width, u32 height)
{
	u32 reg_val = 0;

	switch (output_id) {
	case MCSC_OUTPUT0:
		reg_val = is_hw_set_field_value(reg_val, &mcsc_fields[MCSC_F_SC0_OUT_CROP_POS_H], pos_x);
		reg_val = is_hw_set_field_value(reg_val, &mcsc_fields[MCSC_F_SC0_OUT_CROP_POS_V], pos_y);
		is_hw_set_reg(base_addr, &mcsc_regs[MCSC_R_SC0_OUT_CROP_POS], reg_val);

		reg_val = 0;
		reg_val = is_hw_set_field_value(reg_val, &mcsc_fields[MCSC_F_SC0_OUT_CROP_SIZE_H], width);
		reg_val = is_hw_set_field_value(reg_val, &mcsc_fields[MCSC_F_SC0_OUT_CROP_SIZE_V], height);
		is_hw_set_reg(base_addr, &mcsc_regs[MCSC_R_SC0_OUT_CROP_SIZE], reg_val);
		break;
	case MCSC_OUTPUT1:
		reg_val = is_hw_set_field_value(reg_val, &mcsc_fields[MCSC_F_SC1_OUT_CROP_POS_H], pos_x);
		reg_val = is_hw_set_field_value(reg_val, &mcsc_fields[MCSC_F_SC1_OUT_CROP_POS_V], pos_y);
		is_hw_set_reg(base_addr, &mcsc_regs[MCSC_R_SC1_OUT_CROP_POS], reg_val);

		reg_val = 0;
		reg_val = is_hw_set_field_value(reg_val, &mcsc_fields[MCSC_F_SC1_OUT_CROP_SIZE_H], width);
		reg_val = is_hw_set_field_value(reg_val, &mcsc_fields[MCSC_F_SC1_OUT_CROP_SIZE_V], height);
		is_hw_set_reg(base_addr, &mcsc_regs[MCSC_R_SC1_OUT_CROP_SIZE], reg_val);
		break;
	case MCSC_OUTPUT2:
		reg_val = is_hw_set_field_value(reg_val, &mcsc_fields[MCSC_F_SC2_OUT_CROP_POS_H], pos_x);
		reg_val = is_hw_set_field_value(reg_val, &mcsc_fields[MCSC_F_SC2_OUT_CROP_POS_V], pos_y);
		is_hw_set_reg(base_addr, &mcsc_regs[MCSC_R_SC2_OUT_CROP_POS], reg_val);

		reg_val = 0;
		reg_val = is_hw_set_field_value(reg_val, &mcsc_fields[MCSC_F_SC2_OUT_CROP_SIZE_H], width);
		reg_val = is_hw_set_field_value(reg_val, &mcsc_fields[MCSC_F_SC2_OUT_CROP_SIZE_V], height);
		is_hw_set_reg(base_addr, &mcsc_regs[MCSC_R_SC2_OUT_CROP_SIZE], reg_val);
		break;
	case MCSC_OUTPUT3:
		reg_val = is_hw_set_field_value(reg_val, &mcsc_fields[MCSC_F_SC3_OUT_CROP_POS_H], pos_x);
		reg_val = is_hw_set_field_value(reg_val, &mcsc_fields[MCSC_F_SC3_OUT_CROP_POS_V], pos_y);
		is_hw_set_reg(base_addr, &mcsc_regs[MCSC_R_SC3_OUT_CROP_POS], reg_val);

		reg_val = 0;
		reg_val = is_hw_set_field_value(reg_val, &mcsc_fields[MCSC_F_SC3_OUT_CROP_SIZE_H], width);
		reg_val = is_hw_set_field_value(reg_val, &mcsc_fields[MCSC_F_SC3_OUT_CROP_SIZE_V], height);
		is_hw_set_reg(base_addr, &mcsc_regs[MCSC_R_SC3_OUT_CROP_SIZE], reg_val);
		break;
	case MCSC_OUTPUT4:
		reg_val = is_hw_set_field_value(reg_val, &mcsc_fields[MCSC_F_SC4_OUT_CROP_POS_H], pos_x);
		reg_val = is_hw_set_field_value(reg_val, &mcsc_fields[MCSC_F_SC4_OUT_CROP_POS_V], pos_y);
		is_hw_set_reg(base_addr, &mcsc_regs[MCSC_R_SC4_OUT_CROP_POS], reg_val);

		reg_val = 0;
		reg_val = is_hw_set_field_value(reg_val, &mcsc_fields[MCSC_F_SC4_OUT_CROP_SIZE_H], width);
		reg_val = is_hw_set_field_value(reg_val, &mcsc_fields[MCSC_F_SC4_OUT_CROP_SIZE_V], height);
		is_hw_set_reg(base_addr, &mcsc_regs[MCSC_R_SC4_OUT_CROP_SIZE], reg_val);
		break;
	default:
		break;
	}
}

u32 is_scaler_get_post_strip_enable(void __iomem *base_addr, u32 output_id)
{
	int ret = 0;

	switch (output_id) {
	case MCSC_OUTPUT0:
		ret = is_hw_get_field(base_addr, &mcsc_regs[MCSC_R_PC0_CTRL],
			&mcsc_fields[MCSC_F_PC0_STRIP_ENABLE]);
		break;
	case MCSC_OUTPUT1:
		ret = is_hw_get_field(base_addr, &mcsc_regs[MCSC_R_PC1_CTRL],
			&mcsc_fields[MCSC_F_PC1_STRIP_ENABLE]);
		break;
	case MCSC_OUTPUT2:
		ret = is_hw_get_field(base_addr, &mcsc_regs[MCSC_R_PC2_CTRL],
			&mcsc_fields[MCSC_F_PC2_STRIP_ENABLE]);
		break;
	case MCSC_OUTPUT3:
		/* not support */
		break;
	case MCSC_OUTPUT4:
		/* not support */
		break;
	default:
		break;
	}

	return ret;
}

void is_scaler_set_post_strip_enable(void __iomem *base_addr, u32 output_id, u32 enable)
{
	switch (output_id) {
	case MCSC_OUTPUT0:
		is_hw_set_field(base_addr, &mcsc_regs[MCSC_R_PC0_CTRL], &mcsc_fields[MCSC_F_PC0_STRIP_ENABLE], enable);
		break;
	case MCSC_OUTPUT1:
		is_hw_set_field(base_addr, &mcsc_regs[MCSC_R_PC1_CTRL], &mcsc_fields[MCSC_F_PC1_STRIP_ENABLE], enable);
		break;
	case MCSC_OUTPUT2:
		is_hw_set_field(base_addr, &mcsc_regs[MCSC_R_PC2_CTRL], &mcsc_fields[MCSC_F_PC2_STRIP_ENABLE], enable);
		break;
	case MCSC_OUTPUT3:
		/* not support */
		break;
	case MCSC_OUTPUT4:
		/* not support */
		break;
	default:
		break;
	}
}

void is_scaler_get_post_strip_config(void __iomem *base_addr, u32 output_id, u32 *pre_dst_h, u32 *start_pos_h)
{
	switch (output_id) {
	case MCSC_OUTPUT0:
		*pre_dst_h = is_hw_get_field(base_addr, &mcsc_regs[MCSC_R_PC0_STRIP_PRE_DST_SIZE],
			&mcsc_fields[MCSC_F_PC0_STRIP_PRE_DST_SIZE_H]);
		*start_pos_h = is_hw_get_field(base_addr, &mcsc_regs[MCSC_R_PC0_STRIP_IN_START_POS],
			&mcsc_fields[MCSC_F_PC0_STRIP_IN_START_POS_H]);
		break;
	case MCSC_OUTPUT1:
		*pre_dst_h = is_hw_get_field(base_addr, &mcsc_regs[MCSC_R_PC1_STRIP_PRE_DST_SIZE],
			&mcsc_fields[MCSC_F_PC1_STRIP_PRE_DST_SIZE_H]);
		*start_pos_h = is_hw_get_field(base_addr, &mcsc_regs[MCSC_R_PC1_STRIP_IN_START_POS],
			&mcsc_fields[MCSC_F_PC1_STRIP_IN_START_POS_H]);
		break;
	case MCSC_OUTPUT2:
		*pre_dst_h = is_hw_get_field(base_addr, &mcsc_regs[MCSC_R_PC2_STRIP_PRE_DST_SIZE],
			&mcsc_fields[MCSC_F_PC2_STRIP_PRE_DST_SIZE_H]);
		*start_pos_h = is_hw_get_field(base_addr, &mcsc_regs[MCSC_R_PC2_STRIP_IN_START_POS],
			&mcsc_fields[MCSC_F_PC2_STRIP_IN_START_POS_H]);
		break;
	case MCSC_OUTPUT3:
		/* not support */
		break;
	case MCSC_OUTPUT4:
		/* not support */
		break;
	default:
		break;
	}
}

void is_scaler_set_post_strip_config(void __iomem *base_addr, u32 output_id, u32 pre_dst_h, u32 start_pos_h)
{
	switch (output_id) {
	case MCSC_OUTPUT0:
		is_hw_set_field(base_addr, &mcsc_regs[MCSC_R_PC0_STRIP_PRE_DST_SIZE], &mcsc_fields[MCSC_F_PC0_STRIP_PRE_DST_SIZE_H], pre_dst_h);
		is_hw_set_field(base_addr, &mcsc_regs[MCSC_R_PC0_STRIP_IN_START_POS], &mcsc_fields[MCSC_F_PC0_STRIP_IN_START_POS_H], start_pos_h);
		break;
	case MCSC_OUTPUT1:
		is_hw_set_field(base_addr, &mcsc_regs[MCSC_R_PC1_STRIP_PRE_DST_SIZE], &mcsc_fields[MCSC_F_PC1_STRIP_PRE_DST_SIZE_H], pre_dst_h);
		is_hw_set_field(base_addr, &mcsc_regs[MCSC_R_PC1_STRIP_IN_START_POS], &mcsc_fields[MCSC_F_PC1_STRIP_IN_START_POS_H], start_pos_h);
		break;
	case MCSC_OUTPUT2:
		is_hw_set_field(base_addr, &mcsc_regs[MCSC_R_PC2_STRIP_PRE_DST_SIZE], &mcsc_fields[MCSC_F_PC2_STRIP_PRE_DST_SIZE_H], pre_dst_h);
		is_hw_set_field(base_addr, &mcsc_regs[MCSC_R_PC2_STRIP_IN_START_POS], &mcsc_fields[MCSC_F_PC2_STRIP_IN_START_POS_H], start_pos_h);
		break;
	case MCSC_OUTPUT3:
		/* not support */
		break;
	case MCSC_OUTPUT4:
		/* not support */
		break;
	default:
		break;
	}
}

u32 is_scaler_get_post_out_crop_enable(void __iomem *base_addr, u32 output_id)
{
	int ret = 0;

	switch (output_id) {
	case MCSC_OUTPUT0:
		ret = is_hw_get_field(base_addr, &mcsc_regs[MCSC_R_PC0_CTRL],
			&mcsc_fields[MCSC_F_PC0_OUT_CROP_ENABLE]);
		break;
	case MCSC_OUTPUT1:
		ret = is_hw_get_field(base_addr, &mcsc_regs[MCSC_R_PC1_CTRL],
			&mcsc_fields[MCSC_F_PC1_OUT_CROP_ENABLE]);
		break;
	case MCSC_OUTPUT2:
		ret = is_hw_get_field(base_addr, &mcsc_regs[MCSC_R_PC2_CTRL],
			&mcsc_fields[MCSC_F_PC2_OUT_CROP_ENABLE]);
		break;
	case MCSC_OUTPUT3:
		/* not support */
		break;
	case MCSC_OUTPUT4:
		/* not support */
		break;
	default:
		break;
	}

	return ret;
}

void is_scaler_set_post_out_crop_enable(void __iomem *base_addr, u32 output_id, u32 enable)
{
	switch (output_id) {
	case MCSC_OUTPUT0:
		is_hw_set_field(base_addr, &mcsc_regs[MCSC_R_PC0_CTRL], &mcsc_fields[MCSC_F_PC0_OUT_CROP_ENABLE], enable);
		break;
	case MCSC_OUTPUT1:
		is_hw_set_field(base_addr, &mcsc_regs[MCSC_R_PC1_CTRL], &mcsc_fields[MCSC_F_PC1_OUT_CROP_ENABLE], enable);
		break;
	case MCSC_OUTPUT2:
		is_hw_set_field(base_addr, &mcsc_regs[MCSC_R_PC2_CTRL], &mcsc_fields[MCSC_F_PC2_OUT_CROP_ENABLE], enable);
		break;
	case MCSC_OUTPUT3:
		/* not support */
		break;
	case MCSC_OUTPUT4:
		/* not support */
		break;
	default:
		break;
	}
}

void is_scaler_get_post_out_crop_size(void __iomem *base_addr, u32 output_id, u32 *width, u32 *height)
{
	switch (output_id) {
	case MCSC_OUTPUT0:
		*width = is_hw_get_field(base_addr, &mcsc_regs[MCSC_R_PC0_OUT_CROP_SIZE],
			&mcsc_fields[MCSC_F_PC0_OUT_CROP_SIZE_H]);
		*height = is_hw_get_field(base_addr, &mcsc_regs[MCSC_R_PC0_OUT_CROP_SIZE],
			&mcsc_fields[MCSC_F_PC0_OUT_CROP_SIZE_V]);
		break;
	case MCSC_OUTPUT1:
		*width = is_hw_get_field(base_addr, &mcsc_regs[MCSC_R_PC1_OUT_CROP_SIZE],
			&mcsc_fields[MCSC_F_PC1_OUT_CROP_SIZE_H]);
		*height = is_hw_get_field(base_addr, &mcsc_regs[MCSC_R_PC1_OUT_CROP_SIZE],
			&mcsc_fields[MCSC_F_PC1_OUT_CROP_SIZE_V]);
		break;
	case MCSC_OUTPUT2:
		*width = is_hw_get_field(base_addr, &mcsc_regs[MCSC_R_PC2_OUT_CROP_SIZE],
			&mcsc_fields[MCSC_F_PC2_OUT_CROP_SIZE_H]);
		*height = is_hw_get_field(base_addr, &mcsc_regs[MCSC_R_PC2_OUT_CROP_SIZE],
			&mcsc_fields[MCSC_F_PC2_OUT_CROP_SIZE_V]);
		break;
	case MCSC_OUTPUT3:
		/* not support */
		break;
	case MCSC_OUTPUT4:
		/* not support */
		break;
	default:
		break;
	}
}

void is_scaler_set_post_out_crop_size(void __iomem *base_addr, u32 output_id,
	u32 pos_x, u32 pos_y, u32 width, u32 height)
{
	u32 reg_val = 0;

	switch (output_id) {
	case MCSC_OUTPUT0:
		reg_val = is_hw_set_field_value(reg_val, &mcsc_fields[MCSC_F_PC0_OUT_CROP_POS_H], pos_x);
		reg_val = is_hw_set_field_value(reg_val, &mcsc_fields[MCSC_F_PC0_OUT_CROP_POS_V], pos_y);
		is_hw_set_reg(base_addr, &mcsc_regs[MCSC_R_PC0_OUT_CROP_POS], reg_val);

		reg_val = 0;
		reg_val = is_hw_set_field_value(reg_val, &mcsc_fields[MCSC_F_PC0_OUT_CROP_SIZE_H], width);
		reg_val = is_hw_set_field_value(reg_val, &mcsc_fields[MCSC_F_PC0_OUT_CROP_SIZE_V], height);
		is_hw_set_reg(base_addr, &mcsc_regs[MCSC_R_PC0_OUT_CROP_SIZE], reg_val);
		break;
	case MCSC_OUTPUT1:
		reg_val = is_hw_set_field_value(reg_val, &mcsc_fields[MCSC_F_PC1_OUT_CROP_POS_H], pos_x);
		reg_val = is_hw_set_field_value(reg_val, &mcsc_fields[MCSC_F_PC1_OUT_CROP_POS_V], pos_y);
		is_hw_set_reg(base_addr, &mcsc_regs[MCSC_R_PC1_OUT_CROP_POS], reg_val);

		reg_val = 0;
		reg_val = is_hw_set_field_value(reg_val, &mcsc_fields[MCSC_F_PC1_OUT_CROP_SIZE_H], width);
		reg_val = is_hw_set_field_value(reg_val, &mcsc_fields[MCSC_F_PC1_OUT_CROP_SIZE_V], height);
		is_hw_set_reg(base_addr, &mcsc_regs[MCSC_R_PC1_OUT_CROP_SIZE], reg_val);
		break;
	case MCSC_OUTPUT2:
		reg_val = is_hw_set_field_value(reg_val, &mcsc_fields[MCSC_F_PC2_OUT_CROP_POS_H], pos_x);
		reg_val = is_hw_set_field_value(reg_val, &mcsc_fields[MCSC_F_PC2_OUT_CROP_POS_V], pos_y);
		is_hw_set_reg(base_addr, &mcsc_regs[MCSC_R_PC2_OUT_CROP_POS], reg_val);

		reg_val = 0;
		reg_val = is_hw_set_field_value(reg_val, &mcsc_fields[MCSC_F_PC2_OUT_CROP_SIZE_H], width);
		reg_val = is_hw_set_field_value(reg_val, &mcsc_fields[MCSC_F_PC2_OUT_CROP_SIZE_V], height);
		is_hw_set_reg(base_addr, &mcsc_regs[MCSC_R_PC2_OUT_CROP_SIZE], reg_val);
		break;
	case MCSC_OUTPUT3:
		/* not support */
		break;
	case MCSC_OUTPUT4:
		/* not support */
		break;
	default:
		break;
	}
}

/* HF: High Frequency */
void is_scaler_set_djag_hf_enable(void __iomem *base_addr, u32 hf_enable, u32 hf_scaler_enable)
{
	/* not support */
}

void is_scaler_set_hf_scaler_enable(void __iomem *base_addr, u32 hw_id, u32 enable)
{
	/* not support */
}

void is_scaler_set_djag_hf_cfg(void __iomem *base_addr, struct hf_cfg_by_ni *hf_cfg)
{
	/* not support */
}

int is_scaler_set_hf_config(void __iomem *base_addr, u32 hw_id,
	bool hf_enable,
	struct param_mcs_output param,
	struct scaler_coef_cfg sc_coef, enum exynos_sensor_position sensor_position)
{
	/* not support */
	return 0;
}

static void is_scaler0_clear_intr_src(void __iomem *base_addr, u32 status)
{
	if (status & (1 << INTR_MC_SCALER_SHADOW_COPY_FINISH_OVF))
		is_hw_set_reg(base_addr, &mcsc_regs[MCSC_R_SCALER_INTERRUPT_0],
			(u32)1 << INTR_MC_SCALER_SHADOW_COPY_FINISH_OVF);

	if (status & (1 << INTR_MC_SCALER_SHADOW_COPY_FINISH))
		is_hw_set_reg(base_addr, &mcsc_regs[MCSC_R_SCALER_INTERRUPT_0],
			(u32)1 << INTR_MC_SCALER_SHADOW_COPY_FINISH);

	if (status & (1 << INTR_MC_SCALER_OVERFLOW))
		is_hw_set_reg(base_addr, &mcsc_regs[MCSC_R_SCALER_INTERRUPT_0],
			(u32)1 << INTR_MC_SCALER_OVERFLOW);

	if (status & (1 << INTR_MC_SCALER_INPUT_VERTICAL_UNF))
		is_hw_set_reg(base_addr, &mcsc_regs[MCSC_R_SCALER_INTERRUPT_0],
			(u32)1 << INTR_MC_SCALER_INPUT_VERTICAL_UNF);

	if (status & (1 << INTR_MC_SCALER_INPUT_VERTICAL_OVF))
		is_hw_set_reg(base_addr, &mcsc_regs[MCSC_R_SCALER_INTERRUPT_0],
			(u32)1 << INTR_MC_SCALER_INPUT_VERTICAL_OVF);

	if (status & (1 << INTR_MC_SCALER_INPUT_HORIZONTAL_UNF))
		is_hw_set_reg(base_addr, &mcsc_regs[MCSC_R_SCALER_INTERRUPT_0],
			(u32)1 << INTR_MC_SCALER_INPUT_HORIZONTAL_UNF);

	if (status & (1 << INTR_MC_SCALER_INPUT_HORIZONTAL_OVF))
		is_hw_set_reg(base_addr, &mcsc_regs[MCSC_R_SCALER_INTERRUPT_0],
			(u32)1 << INTR_MC_SCALER_INPUT_HORIZONTAL_OVF);

	if (status & (1 << INTR_MC_SCALER_CORE_FINISH))
		is_hw_set_reg(base_addr, &mcsc_regs[MCSC_R_SCALER_INTERRUPT_0],
			(u32)1 << INTR_MC_SCALER_CORE_FINISH);

	if (status & (1 << INTR_MC_SCALER_WDMA_FINISH))
		is_hw_set_reg(base_addr, &mcsc_regs[MCSC_R_SCALER_INTERRUPT_0],
			(u32)1 << INTR_MC_SCALER_WDMA_FINISH);

	if (status & (1 << INTR_MC_SCALER_FRAME_START))
		is_hw_set_reg(base_addr, &mcsc_regs[MCSC_R_SCALER_INTERRUPT_0],
			(u32)1 << INTR_MC_SCALER_FRAME_START);

	if (status & (1 << INTR_MC_SCALER_FRAME_END))
		is_hw_set_reg(base_addr, &mcsc_regs[MCSC_R_SCALER_INTERRUPT_0],
			(u32)1 << INTR_MC_SCALER_FRAME_END);
}

void is_scaler_clear_intr_src(void __iomem *base_addr, u32 hw_id, u32 status)
{
	switch (hw_id) {
	case DEV_HW_MCSC0:
		is_scaler0_clear_intr_src(base_addr, status);
		break;
	default:
		warn_hw("invalid hw_id(%d) for MCSC api\n", hw_id);
		break;
	}
}

u32 is_scaler_get_intr_mask(void __iomem *base_addr, u32 hw_id)
{
	int ret = 0;

	switch (hw_id) {
	case DEV_HW_MCSC0:
		ret = is_hw_get_reg(base_addr, &mcsc_regs[MCSC_R_SCALER_INTERRUPT_MASK_0]);
		break;
	default:
		warn_hw("invalid hw_id(%d) for MCSC api\n", hw_id);
		break;
	}

	return ret;
}

u32 is_scaler_get_intr_status(void __iomem *base_addr, u32 hw_id)
{
	int ret = 0;

	switch (hw_id) {
	case DEV_HW_MCSC0:
		ret = is_hw_get_reg(base_addr, &mcsc_regs[MCSC_R_SCALER_INTERRUPT_0]);
		break;
	default:
		warn_hw("invalid hw_id(%d) for MCSC api\n", hw_id);
		break;
	}

	return ret;
}

u32 is_scaler_handle_extended_intr(u32 status)
{
	int ret = 0;

	if (status & (1 << INTR_MC_SCALER_SHADOW_COPY_FINISH_OVF)) {
		err_hw("[MCSC]Shadow Register Copy Overflow!! (0x%x)", status);
		FIMC_BUG(1);

		/* TODO: Shadow copy overflow recovery logic */
	}

	return ret;
}

u32 is_scaler_get_version(void __iomem *base_addr)
{
	return is_hw_get_reg(base_addr, &mcsc_regs[MCSC_R_SCALER_VERSION]);
}

u32 is_scaler_get_idle_status(void __iomem *base_addr, u32 hw_id)
{
	int ret = 0;

	if (hw_id == DEV_HW_MCSC0)
		ret = is_hw_get_field(base_addr,
			&mcsc_regs[MCSC_R_SCALER_RUNNING_STATUS], &mcsc_fields[MCSC_F_SCALER_IDLE_0]);
	else
		warn_hw("invalid hw_id(%d) for MCSC api\n", hw_id);

	return ret;

}

void is_scaler_dump(void __iomem *base_addr)
{
	u32 i = 0;
	u32 reg_val = 0;

	info_hw("MCSC ver 8.1");

	for (i = 0; i < MCSC_REG_CNT; i++) {
		reg_val = readl(base_addr + mcsc_regs[i].sfr_offset);
		sfrinfo("[DUMP] reg:[%s][0x%04X], value:[0x%08X]\n",
			mcsc_regs[i].reg_name, mcsc_regs[i].sfr_offset, reg_val);
	}
}
