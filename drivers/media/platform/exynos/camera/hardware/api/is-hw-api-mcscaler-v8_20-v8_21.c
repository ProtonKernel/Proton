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
#include "sfr/is-sfr-mcsc-v8_20-v8_21.h"
#include "is-hw.h"
#include "is-hw-control.h"
#include "is-param.h"

/* for 8-bit Coefficient */
const struct mcsc_v_coef v_coef_4tap[7] = {
	/* x8/8 */
	{
		{  0,  -4,  -6,  -8,  -8,  -8,  -8,  -7,  -6},
		{128, 127, 124, 118, 111, 102,  92,  81,  70},
		{  0,   5,  11,  19,  27,  37,  48,  59,  70},
		{  0,   0,  -1,  -1,  -2,  -3,  -4,  -5,  -6},
	},
	/* x7/8 */
	{
		{  8,   4,   1,  -2,  -3,  -5,  -5,  -5,  -5},
		{112, 111, 109, 105, 100,  93,  86,  77,  69},
		{  8,  14,  20,  27,  34,  43,  51,  60,  69},
		{  0,  -1,  -2,  -2,  -3,  -3,  -4,  -4,  -5},
	},
	/* x6/8 */
	{
		{ 16,  12,   8,   5,   2,   0,  -1,  -2,  -2},
		{ 96,  97,  96,  93,  89,  84,  79,  73,  66},
		{ 16,  21,  26,  32,  39,  46,  53,  59,  66},
		{  0,  -2,  -2,  -2,  -2,  -2,  -3,  -2,  -2},
	},
	/* x5/8 */
	{
		{ 22,  18,  14,  11,   8,   6,   4,   2,   1},
		{ 84,  85,  84,  82,  79,  76,  72,  68,  63},
		{ 22,  26,  31,  36,  42,  47,  52,  58,  63},
		{  0,  -1,  -1,  -1,  -1,  -1,   0,   0,   1},
	},
	/* x4/8 */
	{
		{ 26,  22,  19,  16,  13,  10,   8,   6,   5},
		{ 76,  76,  75,  73,  71,  69,  66,  63,  59},
		{ 26,  30,  34,  38,  43,  47,  51,  55,  59},
		{  0,   0,   0,   1,   1,   2,   3,   4,   5},
	},
	/* x3/8 */
	{
		{ 29,  26,  23,  20,  17,  15,  12,  10,   8},
		{ 70,  68,  67,  66,  65,  63,  61,  58,  56},
		{ 29,  32,  36,  39,  43,  46,  50,  53,  56},
		{  0,   2,   2,   3,   3,   4,   5,   7,   8},
	},
	/* x2/8 */
	{
		{ 32,  28,  25,  22,  19,  17,  15,  13,  11},
		{ 64,  63,  62,  62,  61,  59,  58,  55,  53},
		{ 32,  34,  37,  40,  43,  46,  48,  51,  53},
		{  0,   3,   4,   4,   5,   6,   7,   9,  11},
	}
};

const struct mcsc_h_coef h_coef_8tap[7] = {
	/* x8/8 */
	{
		{  0,  -1,  -1,  -1,  -1,  -1,  -2,  -1,  -1},
		{  0,   2,   4,   5,   6,   6,   7,   6,   6},
		{  0,  -6, -12, -15, -18, -20, -21, -20, -20},
		{128, 127, 125, 120, 114, 107,  99,  89,  79},
		{  0,   7,  16,  25,  35,  46,  57,  68,  79},
		{  0,  -2,  -5,  -8, -10, -13, -16, -18, -20},
		{  0,   1,   1,   2,   3,   4,   5,   5,   6},
		{  0,   0,   0,   0,  -1,  -1,  -1,  -1,  -1},
	},
	/* x7/8 */
	{
		{  3,   2,   2,   1,   1,   1,   0,   0,   0},
		{ -8,  -6,  -4,  -2,  -1,   1,   2,   3,   3},
		{ 14,   7,   1,  -3,  -7, -11, -13, -15, -16},
		{111, 112, 110, 106, 103,  97,  91,  85,  77},
		{ 13,  21,  28,  36,  44,  53,  61,  69,  77},
		{ -8, -10, -12, -13, -15, -16, -16, -17, -16},
		{  3,   3,   4,   4,   4,   4,   4,   4,   3},
		{  0,  -1,  -1,  -1,  -1,  -1,  -1,  -1,   0},
	},
	/* x6/8 */
	{
		{  2,   2,   2,   2,   2,   2,   2,   1,   1},
		{-11, -10,  -9,  -8,  -7,  -5,  -4,  -3,  -2},
		{ 25,  19,  14,  10,   5,   1,  -2,  -5,  -7},
		{ 96,  96,  94,  92,  90,  86,  82,  77,  72},
		{ 25,  31,  37,  43,  49,  55,  61,  67,  72},
		{-11, -12, -12, -12, -12, -12, -11,  -9,  -7},
		{  2,   2,   2,   1,   1,   0,  -1,  -1,  -2},
		{  0,   0,   0,   0,   0,   1,   1,   1,   1},
	},
	/* x5/8 */
	{
		{ -1,  -1,   0,   0,   0,   0,   1,   1,   1},
		{ -8,  -8,  -8,  -8,  -8,  -7,  -7,  -6,  -6},
		{ 33,  28,  24,  20,  16,  13,  10,   6,   4},
		{ 80,  80,  79,  78,  76,  74,  71,  68,  65},
		{ 33,  37,  41,  46,  50,  54,  58,  62,  65},
		{ -8,  -7,  -7,  -6,  -4,  -3,  -1,   1,   4},
		{ -1,  -2,  -2,  -3,  -3,  -4,  -5,  -5,  -6},
		{  0,   1,   1,   1,   1,   1,   1,   1,   1},
	},
	/* x4/8 */
	{
		{ -3,  -3,  -2,  -2,  -2,  -2,  -1,  -1,  -1},
		{  0,  -1,  -2,  -3,  -3,  -3,  -4,  -4,  -4},
		{ 35,  32,  29,  27,  24,  21,  19,  16,  14},
		{ 64,  64,  63,  63,  61,  60,  59,  57,  55},
		{ 35,  38,  41,  43,  46,  49,  51,  53,  55},
		{  0,   1,   2,   4,   6,   7,   9,  12,  14},
		{ -3,  -3,  -3,  -4,  -4,  -4,  -4,  -4,  -4},
		{  0,   0,   0,   0,   0,   0,  -1,  -1,  -1},
	},
	/* x3/8 */
	{
		{ -1,  -1,  -1,  -1,  -1,  -1,  -1,  -1,  -1},
		{  8,   7,   6,   5,   4,   3,   2,   2,   1},
		{ 33,  31,  30,  28,  26,  24,  23,  21,  19},
		{ 48,  49,  49,  48,  48,  47,  47,  45,  45},
		{ 33,  35,  36,  38,  39,  41,  42,  43,  45},
		{  8,   9,  10,  12,  13,  15,  16,  18,  19},
		{ -1,  -1,  -1,  -1,   0,   0,   0,   1,   1},
		{  0,  -1,  -1,  -1,  -1,  -1,  -1,  -1,  -1},
	},
	/* x2/8 */
	{
		{  2,   2,   2,   2,   1,   1,   1,   1,   1},
		{ 13,  12,  11,  10,  10,   9,   8,   7,   6},
		{ 30,  29,  28,  26,  26,  24,  24,  22,  21},
		{ 38,  38,  38,  38,  37,  37,  37,  36,  36},
		{ 30,  30,  31,  32,  33,  34,  34,  35,  36},
		{ 13,  14,  15,  16,  17,  18,  19,  20,  21},
		{  2,   3,   3,   4,   4,   5,   5,   6,   6},
		{  0,   0,   0,   0,   0,   0,   0,   1,   1},
	}
};

void is_scaler_start(void __iomem *base_addr, u32 hw_id)
{
	/* Qactive must set to "1" before scaler enable */
	is_hw_set_field(base_addr, &mcsc_regs[MCSC_R_SC_QACTIVE_ENABLE], &mcsc_fields[MCSC_F_QACTIVE_ENABLE], 1);

	switch (hw_id) {
	case DEV_HW_MCSC0:
	case DEV_HW_MCSC1:
		is_hw_set_field(base_addr, &mcsc_regs[MCSC_R_SC_GCTRL_0], &mcsc_fields[MCSC_F_SCALER_ENABLE_0], 1);
		break;
	default:
		warn_hw("invalid hw_id(%d) for MCSC api\n", hw_id);
		break;
	}
}

void is_scaler_rdma_start(void __iomem *base_addr, u32 hw_id)
{
	switch (hw_id) {
	case DEV_HW_MCSC0:
		warn_hw("hw_id(%d) is not supported RDMA\n", hw_id);
		break;
	case DEV_HW_MCSC1:
		dbg_hw(2, "[ID:%d]RDMA start\n", hw_id);
		break;
	default:
		warn_hw("invalid hw_id(%d) for MCSC rdma api\n", hw_id);
		break;
	}
}

void is_scaler_stop(void __iomem *base_addr, u32 hw_id)
{
	switch (hw_id) {
	case DEV_HW_MCSC0:
	case DEV_HW_MCSC1:
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
			&mcsc_regs[MCSC_R_SC_QACTIVE_ENABLE], &mcsc_fields[MCSC_F_QACTIVE_ENABLE], 0);
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
	case DEV_HW_MCSC1:
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
	case DEV_HW_MCSC1:
		reg_val = is_hw_set_field_value(reg_val, &mcsc_fields[MCSC_F_SCALER_OVERFLOW_INT_0], 1);
		reg_val = is_hw_set_field_value(reg_val, &mcsc_fields[MCSC_F_INPUT_VERTICAL_UNF_INT_0], 1);
		reg_val = is_hw_set_field_value(reg_val, &mcsc_fields[MCSC_F_INPUT_VERTICAL_OVF_INT_0], 1);
		reg_val = is_hw_set_field_value(reg_val, &mcsc_fields[MCSC_F_INPUT_HORIZONTAL_UNF_INT_0], 1);
		reg_val = is_hw_set_field_value(reg_val, &mcsc_fields[MCSC_F_INPUT_HORIZONTAL_OVF_INT_0], 1);
		reg_val = is_hw_set_field_value(reg_val, &mcsc_fields[MCSC_F_INPUT_PROTOCOL_ERR_INT_0], 1);
		reg_val = is_hw_set_field_value(reg_val, &mcsc_fields[MCSC_F_SHADOW_TRIGGER_INT_0], 1);
		reg_val = is_hw_set_field_value(reg_val, &mcsc_fields[MCSC_F_SHADOW_HW_TRIGGER_INT_0], 1);
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
	case DEV_HW_MCSC1:
		reg_val = is_hw_set_field_value(reg_val,
			&mcsc_fields[MCSC_F_SCALER_OVERFLOW_INT_0_MASK], 1);
		reg_val = is_hw_set_field_value(reg_val,
			&mcsc_fields[MCSC_F_INPUT_VERTICAL_UNF_INT_0_MASK], 1);
		reg_val = is_hw_set_field_value(reg_val,
			&mcsc_fields[MCSC_F_INPUT_VERTICAL_OVF_INT_0_MASK], 1);
		reg_val = is_hw_set_field_value(reg_val,
			&mcsc_fields[MCSC_F_INPUT_HORIZONTAL_UNF_INT_0_MASK], 1);
		reg_val = is_hw_set_field_value(reg_val,
			&mcsc_fields[MCSC_F_INPUT_HORIZONTAL_OVF_INT_0_MASK], 1);
		reg_val = is_hw_set_field_value(reg_val,
			&mcsc_fields[MCSC_F_INPUT_PROTOCOL_ERR_INT_0_MASK], 1);
		reg_val = is_hw_set_field_value(reg_val,
			&mcsc_fields[MCSC_F_SHADOW_TRIGGER_INT_0_MASK], 1);
		reg_val = is_hw_set_field_value(reg_val,
			&mcsc_fields[MCSC_F_SHADOW_HW_TRIGGER_INT_0_MASK], 1);
		reg_val = is_hw_set_field_value(reg_val, &mcsc_fields[MCSC_F_CORE_FINISH_INT_0_MASK], 1);
		reg_val = is_hw_set_field_value(reg_val, &mcsc_fields[MCSC_F_WDMA_FINISH_INT_0_MASK], 1);
		reg_val = is_hw_set_field_value(reg_val, &mcsc_fields[MCSC_F_FRAME_START_INT_0_MASK], 1);
		reg_val = is_hw_set_field_value(reg_val, &mcsc_fields[MCSC_F_FRAME_END_INT_0_MASK], 1);

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
	case DEV_HW_MCSC1:
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
	/* NOT SUPPORT : SHADOW_WRITE_START, SHADOW_WRITE_FINISH */
}

void is_scaler_set_shadow_ctrl(void __iomem *base_addr, u32 hw_id, enum mcsc_shadow_ctrl ctrl)
{
	switch (hw_id) {
	case DEV_HW_MCSC0:
	case DEV_HW_MCSC1:
		is_scaler0_set_shadow_ctrl(base_addr, ctrl);
		break;
	default:
		break;
	}
}

void is_scaler_clear_shadow_ctrl(void __iomem *base_addr, u32 hw_id)
{
	/* not support */
}

void is_scaler_read_shadow_ctrl(void __iomem *base_addr, u32 hw_id)
{
	/* When this register is 1, user can read shadow registers */
	switch (hw_id) {
	case DEV_HW_MCSC0:
	case DEV_HW_MCSC1:
		is_hw_set_reg(base_addr, &mcsc_regs[MCSC_R_READ_SHADOW_REG_CTRL], 0x1);
		break;
	default:
		break;
	}
}

void is_scaler_get_input_status(void __iomem *base_addr, u32 hw_id, u32 *hl, u32 *vl)
{
	switch (hw_id) {
	case DEV_HW_MCSC0:
	case DEV_HW_MCSC1:
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
	/*
	 * Input OTF format : 0 = CbCr first, 1 = Cb first, 2 = Cr first, 3 = CbCr second
	 * Need to check ISP Output
	 * If it does not match, INPUT_PROTOCOL_ERR_INT_0 occurs.
	 * ITP OTF out format setting
	 * (conv_convregs_format_sel = 2 / conv_convregs_merge_uv = 0 /
	 * conv_convregs_pos_u = 0 / conv_convregs_pos_v = 1)
	 */
	switch (hw_id) {
	case DEV_HW_MCSC0:
		is_hw_set_field(base_addr, &mcsc_regs[MCSC_R_INPUT_SRC0_CTRL],
			&mcsc_fields[MCSC_F_INPUT_SRC0_FORMAT], 0x1);	/* reset value = 1 */
		break;
	case DEV_HW_MCSC1:
		/* not support */
		break;
	default:
		warn_hw("invalid hw_id(%d) for MCSC api\n", hw_id);
		break;
	}
}

u32 is_scaler_get_input_source(void __iomem *base_addr, u32 hw_id)
{
	int ret = 0;

	/*  0: otf input_0 => Only One OTF input */
	return ret;
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
	case DEV_HW_MCSC1:
		reg_val = is_hw_set_field_value(reg_val, &mcsc_fields[MCSC_F_POLY_SC0_IMG_HSIZE], width);
		reg_val = is_hw_set_field_value(reg_val, &mcsc_fields[MCSC_F_POLY_SC0_IMG_VSIZE], height);
		is_hw_set_reg(base_addr, &mcsc_regs[MCSC_R_POLY_SC0_IMG_SIZE], reg_val);
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
	case DEV_HW_MCSC1:
		*width = is_hw_get_field(base_addr, &mcsc_regs[MCSC_R_POLY_SC0_IMG_SIZE],
			&mcsc_fields[MCSC_F_POLY_SC0_IMG_HSIZE]);
		*height = is_hw_get_field(base_addr, &mcsc_regs[MCSC_R_POLY_SC0_IMG_SIZE],
			&mcsc_fields[MCSC_F_POLY_SC0_IMG_VSIZE]);
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
			&mcsc_regs[MCSC_R_POLY_SC0_P0_CTRL], &mcsc_fields[MCSC_F_POLY_SC0_P0_ENABLE]);
		enable_post = is_hw_get_field(base_addr,
			&mcsc_regs[MCSC_R_POST_CHAIN0_POST_SC_CTRL], &mcsc_fields[MCSC_F_POST_CHAIN0_POST_SC_ENABLE]);
		enable_dma = is_hw_get_field(base_addr,
			&mcsc_regs[MCSC_R_POST_CHAIN0_DMA_OUT_CTRL], &mcsc_fields[MCSC_F_POST_CHAIN0_DMA_OUT_ENABLE]);
		break;
	case MCSC_OUTPUT1:
	case MCSC_OUTPUT2:
	case MCSC_OUTPUT3:
	case MCSC_OUTPUT4:
		/* not support */;
		break;
	default:
		break;
	}
	dbg_hw(2, "[ID:%d]%s:[OUT:%d], en(poly:%d, post:%d), dma(%d)\n",
		hw_id, __func__, output_id, enable_poly, enable_post, enable_dma);

	return (DEV_HW_MCSC0 + input);
}

void is_scaler_set_poly_scaler_enable(void __iomem *base_addr, u32 hw_id, u32 output_id, u32 enable)
{
	switch (output_id) {
	case MCSC_OUTPUT0:
		is_hw_set_field(base_addr, &mcsc_regs[MCSC_R_POLY_SC0_P0_CTRL], &mcsc_fields[MCSC_F_POLY_SC0_P0_ENABLE], enable);
		break;
	case MCSC_OUTPUT1:
	case MCSC_OUTPUT2:
	case MCSC_OUTPUT3:
	case MCSC_OUTPUT4:
		/* not support */;
		break;
	default:
		break;
	}
}

void is_scaler_set_poly_scaler_bypass(void __iomem *base_addr, u32 output_id, u32 bypass)
{
	switch (output_id) {
	case MCSC_OUTPUT0:
		is_hw_set_field(base_addr, &mcsc_regs[MCSC_R_POLY_SC0_P0_CTRL], &mcsc_fields[MCSC_F_POLY_SC0_P0_BYPASS], bypass);
		break;
	case MCSC_OUTPUT1:
	case MCSC_OUTPUT2:
	case MCSC_OUTPUT3:
	case MCSC_OUTPUT4:
		/* not support */;
		break;
	default:
		break;
	}
}

void is_scaler_set_poly_src_size(void __iomem *base_addr, u32 output_id,
	u32 pos_x, u32 pos_y, u32 width, u32 height)
{
	u32 reg_val = 0;

	if (pos_x % MCSC_WIDTH_ALIGN != 0)
		err_hw("SC%d_SRC_HPOS(%d) should be multiple of 4", output_id, pos_x);

	if (width % MCSC_WIDTH_ALIGN != 0)
		err_hw("SC%d_SRC_WIDTH(%d) should be multiple of 4", output_id, width);

	switch (output_id) {
	case MCSC_OUTPUT0:
		reg_val = is_hw_set_field_value(reg_val, &mcsc_fields[MCSC_F_POLY_SC0_P0_SRC_HPOS], pos_x);
		reg_val = is_hw_set_field_value(reg_val, &mcsc_fields[MCSC_F_POLY_SC0_P0_SRC_VPOS], pos_y);
		is_hw_set_reg(base_addr, &mcsc_regs[MCSC_R_POLY_SC0_P0_SRC_POS], reg_val);

		reg_val = 0;
		reg_val = is_hw_set_field_value(reg_val, &mcsc_fields[MCSC_F_POLY_SC0_P0_SRC_HSIZE], width);
		reg_val = is_hw_set_field_value(reg_val, &mcsc_fields[MCSC_F_POLY_SC0_P0_SRC_VSIZE], height);
		is_hw_set_reg(base_addr, &mcsc_regs[MCSC_R_POLY_SC0_P0_SRC_SIZE], reg_val);
		break;
	case MCSC_OUTPUT1:
	case MCSC_OUTPUT2:
	case MCSC_OUTPUT3:
	case MCSC_OUTPUT4:
		/* not support */;
		break;
	default:
		break;
	}
}

void is_scaler_get_poly_src_size(void __iomem *base_addr, u32 output_id, u32 *width, u32 *height)
{
	switch (output_id) {
	case MCSC_OUTPUT0:
		*width = is_hw_get_field(base_addr, &mcsc_regs[MCSC_R_POLY_SC0_P0_SRC_SIZE],
			&mcsc_fields[MCSC_F_POLY_SC0_P0_SRC_HSIZE]);
		*height = is_hw_get_field(base_addr, &mcsc_regs[MCSC_R_POLY_SC0_P0_SRC_SIZE],
			&mcsc_fields[MCSC_F_POLY_SC0_P0_SRC_VSIZE]);
		break;
	case MCSC_OUTPUT1:
	case MCSC_OUTPUT2:
	case MCSC_OUTPUT3:
	case MCSC_OUTPUT4:
		/* not support */;
		break;
	default:
		break;
	}
}

void is_scaler_set_poly_dst_size(void __iomem *base_addr, u32 output_id, u32 width, u32 height)
{
	u32 reg_val = 0;

	if (width % MCSC_WIDTH_ALIGN != 0)
		err_hw("SC%d_DST_WIDTH(%d) should be multiple of 4", output_id, width);

	switch (output_id) {
	case MCSC_OUTPUT0:
		reg_val = is_hw_set_field_value(reg_val, &mcsc_fields[MCSC_F_POLY_SC0_P0_DST_HSIZE], width);
		reg_val = is_hw_set_field_value(reg_val, &mcsc_fields[MCSC_F_POLY_SC0_P0_DST_VSIZE], height);
		is_hw_set_reg(base_addr, &mcsc_regs[MCSC_R_POLY_SC0_P0_DST_SIZE], reg_val);
		break;
	case MCSC_OUTPUT1:
	case MCSC_OUTPUT2:
	case MCSC_OUTPUT3:
	case MCSC_OUTPUT4:
		/* not support */;
		break;
	default:
		break;
	}
}

void is_scaler_get_poly_dst_size(void __iomem *base_addr, u32 output_id, u32 *width, u32 *height)
{
	switch (output_id) {
	case MCSC_OUTPUT0:
		*width = is_hw_get_field(base_addr, &mcsc_regs[MCSC_R_POLY_SC0_P0_DST_SIZE],
			&mcsc_fields[MCSC_F_POLY_SC0_P0_DST_HSIZE]);
		*height = is_hw_get_field(base_addr, &mcsc_regs[MCSC_R_POLY_SC0_P0_DST_SIZE],
			&mcsc_fields[MCSC_F_POLY_SC0_P0_DST_VSIZE]);
		break;
	case MCSC_OUTPUT1:
	case MCSC_OUTPUT2:
	case MCSC_OUTPUT3:
	case MCSC_OUTPUT4:
		/* not support */;
		break;
	default:
		break;
	}
}

void is_scaler_set_poly_scaling_ratio(void __iomem *base_addr, u32 output_id, u32 hratio, u32 vratio)
{
	switch (output_id) {
	case MCSC_OUTPUT0:
		is_hw_set_field(base_addr, &mcsc_regs[MCSC_R_POLY_SC0_P0_H_RATIO],
			&mcsc_fields[MCSC_F_POLY_SC0_P0_H_RATIO], hratio);
		is_hw_set_field(base_addr, &mcsc_regs[MCSC_R_POLY_SC0_P0_V_RATIO],
			&mcsc_fields[MCSC_F_POLY_SC0_P0_V_RATIO], vratio);
		break;
	case MCSC_OUTPUT1:
	case MCSC_OUTPUT2:
	case MCSC_OUTPUT3:
	case MCSC_OUTPUT4:
		/* not support */;
		break;
	default:
		break;
	}
}

void is_scaler_set_h_init_phase_offset(void __iomem *base_addr, u32 output_id, u32 h_offset)
{
	switch (output_id) {
	case MCSC_OUTPUT0:
		is_hw_set_field(base_addr, &mcsc_regs[MCSC_R_POLY_SC0_P0_H_INIT_PHASE_OFFSET],
			&mcsc_fields[MCSC_F_POLY_SC0_P0_H_INIT_PHASE_OFFSET], h_offset);
		break;
	case MCSC_OUTPUT1:
	case MCSC_OUTPUT2:
	case MCSC_OUTPUT3:
	case MCSC_OUTPUT4:
		/* not support */;
		break;
	default:
		break;
	}
}

void is_scaler_set_v_init_phase_offset(void __iomem *base_addr, u32 output_id, u32 v_offset)
{
	switch (output_id) {
	case MCSC_OUTPUT0:
		is_hw_set_field(base_addr, &mcsc_regs[MCSC_R_POLY_SC0_P0_V_INIT_PHASE_OFFSET],
			&mcsc_fields[MCSC_F_POLY_SC0_P0_V_INIT_PHASE_OFFSET], v_offset);
		break;
	case MCSC_OUTPUT1:
	case MCSC_OUTPUT2:
	case MCSC_OUTPUT3:
	case MCSC_OUTPUT4:
		/* not support */;
		break;
	default:
		break;
	}
}

void is_scaler_set_poly_scaler_h_coef(void __iomem *base_addr, u32 output_id, u32 h_sel)
{
	switch (output_id) {
	case MCSC_OUTPUT0:
		is_hw_set_field(base_addr, &mcsc_regs[MCSC_R_POLY_SC0_P0_COEFF_CTRL],
			&mcsc_fields[MCSC_F_POLY_SC0_P0_H_COEFF_SEL], h_sel);
		break;
	case MCSC_OUTPUT1:
	case MCSC_OUTPUT2:
	case MCSC_OUTPUT3:
	case MCSC_OUTPUT4:
		/* not support */;
		break;
	default:
		break;
	}
}

void is_scaler_set_poly_scaler_v_coef(void __iomem *base_addr, u32 output_id, u32 v_sel)
{
	switch (output_id) {
	case MCSC_OUTPUT0:
		is_hw_set_field(base_addr, &mcsc_regs[MCSC_R_POLY_SC0_P0_COEFF_CTRL],
			&mcsc_fields[MCSC_F_POLY_SC0_P0_V_COEFF_SEL], v_sel);
		break;
	case MCSC_OUTPUT1:
	case MCSC_OUTPUT2:
	case MCSC_OUTPUT3:
	case MCSC_OUTPUT4:
		/* not support */;
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
	} else {
		h_coef = get_scaler_coef_ver1(hratio, false);
		v_coef = get_scaler_coef_ver1(vratio, false);
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
		is_hw_set_field(base_addr, &mcsc_regs[MCSC_R_POLY_SC0_P0_ROUND_MODE],
			&mcsc_fields[MCSC_F_POLY_SC0_P0_ROUND_MODE], mode);
		break;
	case MCSC_OUTPUT1:
	case MCSC_OUTPUT2:
	case MCSC_OUTPUT3:
	case MCSC_OUTPUT4:
		/* not support */;
		break;
	default:
		break;
	}
}

void is_scaler_set_post_scaler_enable(void __iomem *base_addr, u32 output_id, u32 enable)
{
	switch (output_id) {
	case MCSC_OUTPUT0:
		is_hw_set_field(base_addr, &mcsc_regs[MCSC_R_POST_CHAIN0_POST_SC_CTRL],
			&mcsc_fields[MCSC_F_POST_CHAIN0_POST_SC_ENABLE], enable);
		break;
	case MCSC_OUTPUT1:
	case MCSC_OUTPUT2:
	case MCSC_OUTPUT3:
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

	if (width % MCSC_WIDTH_ALIGN != 0)
		err_hw("PC%d_IMG_WIDTH(%d) should be multiple of 4", output_id, width);

	switch (output_id) {
	case MCSC_OUTPUT0:
		reg_val = is_hw_set_field_value(reg_val, &mcsc_fields[MCSC_F_POST_CHAIN0_POST_SC_IMG_HSIZE], width);
		reg_val = is_hw_set_field_value(reg_val, &mcsc_fields[MCSC_F_POST_CHAIN0_POST_SC_IMG_VSIZE], height);
		is_hw_set_reg(base_addr, &mcsc_regs[MCSC_R_POST_CHAIN0_POST_SC_IMG_SIZE], reg_val);
		break;
	case MCSC_OUTPUT1:
	case MCSC_OUTPUT2:
	case MCSC_OUTPUT3:
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
		*width = is_hw_get_field(base_addr, &mcsc_regs[MCSC_R_POST_CHAIN0_POST_SC_IMG_SIZE],
			&mcsc_fields[MCSC_F_POST_CHAIN0_POST_SC_IMG_HSIZE]);
		*height = is_hw_get_field(base_addr, &mcsc_regs[MCSC_R_POST_CHAIN0_POST_SC_IMG_SIZE],
			&mcsc_fields[MCSC_F_POST_CHAIN0_POST_SC_IMG_VSIZE]);
		break;
	case MCSC_OUTPUT1:
	case MCSC_OUTPUT2:
	case MCSC_OUTPUT3:
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

	if (width % MCSC_WIDTH_ALIGN != 0)
		err_hw("PC%d_DST_WIDTH(%d) should be multiple of 4", output_id, width);

	switch (output_id) {
	case MCSC_OUTPUT0:
		reg_val = is_hw_set_field_value(reg_val, &mcsc_fields[MCSC_F_POST_CHAIN0_POST_SC_DST_HSIZE], width);
		reg_val = is_hw_set_field_value(reg_val, &mcsc_fields[MCSC_F_POST_CHAIN0_POST_SC_DST_VSIZE], height);
		is_hw_set_reg(base_addr, &mcsc_regs[MCSC_R_POST_CHAIN0_POST_SC_DST_SIZE], reg_val);
		break;
	case MCSC_OUTPUT1:
	case MCSC_OUTPUT2:
	case MCSC_OUTPUT3:
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
		*width = is_hw_get_field(base_addr, &mcsc_regs[MCSC_R_POST_CHAIN0_POST_SC_DST_SIZE],
			&mcsc_fields[MCSC_F_POST_CHAIN0_POST_SC_DST_HSIZE]);
		*height = is_hw_get_field(base_addr, &mcsc_regs[MCSC_R_POST_CHAIN0_POST_SC_DST_SIZE],
			&mcsc_fields[MCSC_F_POST_CHAIN0_POST_SC_DST_VSIZE]);
		break;
	case MCSC_OUTPUT1:
	case MCSC_OUTPUT2:
	case MCSC_OUTPUT3:
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
		is_hw_set_field(base_addr, &mcsc_regs[MCSC_R_POST_CHAIN0_POST_SC_H_RATIO],
			&mcsc_fields[MCSC_F_POST_CHAIN0_POST_SC_H_RATIO], hratio);
		is_hw_set_field(base_addr, &mcsc_regs[MCSC_R_POST_CHAIN0_POST_SC_V_RATIO],
			&mcsc_fields[MCSC_F_POST_CHAIN0_POST_SC_V_RATIO], vratio);
		break;
	case MCSC_OUTPUT1:
	case MCSC_OUTPUT2:
	case MCSC_OUTPUT3:
	case MCSC_OUTPUT4:
		/* not support */
		break;
	default:
		break;
	}
}


void is_scaler_set_post_h_init_phase_offset(void __iomem *base_addr, u32 output_id, u32 h_offset)
{
	/* not support */
}

void is_scaler_set_post_v_init_phase_offset(void __iomem *base_addr, u32 output_id, u32 v_offset)
{
	/* not support */
}

void is_scaler_set_post_scaler_h_v_coef(void __iomem *base_addr, u32 output_id, u32 h_sel, u32 v_sel)
{
	/* not support */
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
	} else {
		h_coef = get_scaler_coef_ver1(hratio, false);
		v_coef = get_scaler_coef_ver1(vratio, false);
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
	/* not support */
}

void is_scaler_set_420_conversion(void __iomem *base_addr, u32 output_id, u32 conv420_weight, u32 conv420_en)
{
	switch (output_id) {
	case MCSC_OUTPUT0:
		is_hw_set_field(base_addr, &mcsc_regs[MCSC_R_POST_CHAIN0_CONV420_WEIGHT],
			&mcsc_fields[MCSC_F_POST_CHAIN0_CONV420_WEIGHT], conv420_weight);
		is_hw_set_field(base_addr, &mcsc_regs[MCSC_R_POST_CHAIN0_CONV420_CTRL],
			&mcsc_fields[MCSC_F_POST_CHAIN0_CONV420_ENABLE], conv420_en);
		break;
	case MCSC_OUTPUT1:
	case MCSC_OUTPUT2:
	case MCSC_OUTPUT3:
	case MCSC_OUTPUT4:
		/* not support */;
		break;
	default:
		break;
	}
}

void is_scaler_set_bchs_enable(void __iomem *base_addr, u32 output_id, bool bchs_en)
{
	switch (output_id) {
	case MCSC_OUTPUT0:
		is_hw_set_field(base_addr, &mcsc_regs[MCSC_R_POST_CHAIN0_BCHS_CTRL],
			&mcsc_fields[MCSC_F_POST_CHAIN0_BCHS_ENABLE], bchs_en);

		/* default BCHS clamp value */
		is_hw_set_reg(base_addr, &mcsc_regs[MCSC_R_POST_CHAIN0_BCHS_CLAMP], 0xFF00FF00);
		break;
	case MCSC_OUTPUT1:
	case MCSC_OUTPUT2:
	case MCSC_OUTPUT3:
	case MCSC_OUTPUT4:
		/* not support */;
		break;
	default:
		break;
	}
}

/* brightness/contrast control */
void is_scaler_set_b_c(void __iomem *base_addr, u32 output_id, u32 y_offset, u32 y_gain)
{
	u32 reg_val = 0;

	/* Need to check MCSC setfile value : 8bit MCSC */
	if (y_gain > 0x100) {
		dbg_hw(2, "Need to check MCSC bchs gain range: y_gain(%x)", y_gain);
		y_offset = 0;
		y_gain = 256;
	}

	switch (output_id) {
	case MCSC_OUTPUT0:
		reg_val = is_hw_set_field_value(reg_val, &mcsc_fields[MCSC_F_POST_CHAIN0_BCHS_YOFFSET], y_offset);
		reg_val = is_hw_set_field_value(reg_val, &mcsc_fields[MCSC_F_POST_CHAIN0_BCHS_YGAIN], y_gain);
		is_hw_set_reg(base_addr, &mcsc_regs[MCSC_R_POST_CHAIN0_BCHS_BC], reg_val);
		break;
	case MCSC_OUTPUT1:
	case MCSC_OUTPUT2:
	case MCSC_OUTPUT3:
	case MCSC_OUTPUT4:
		/* not support */;
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

	/* Need to check MCSC setfile value : 8bit MCSC */
	if (c_gain00 > 0x100 || c_gain11 > 0x100) {
		dbg_hw(2, "Need to check MCSC bchs gain range: c_gain00(%x), c_gain11(%x)", c_gain00, c_gain11);
		c_gain00 = 256;
		c_gain01 = 0;
		c_gain10 = 0;
		c_gain11 = 256;
	}

	switch (output_id) {
	case MCSC_OUTPUT0:
		reg_val = is_hw_set_field_value(reg_val, &mcsc_fields[MCSC_F_POST_CHAIN0_BCHS_C_GAIN_00], c_gain00);
		reg_val = is_hw_set_field_value(reg_val, &mcsc_fields[MCSC_F_POST_CHAIN0_BCHS_C_GAIN_01], c_gain01);
		is_hw_set_reg(base_addr, &mcsc_regs[MCSC_R_POST_CHAIN0_BCHS_HS1], reg_val);
		reg_val = 0;
		reg_val = is_hw_set_field_value(reg_val, &mcsc_fields[MCSC_F_POST_CHAIN0_BCHS_C_GAIN_10], c_gain10);
		reg_val = is_hw_set_field_value(reg_val, &mcsc_fields[MCSC_F_POST_CHAIN0_BCHS_C_GAIN_11], c_gain11);
		is_hw_set_reg(base_addr, &mcsc_regs[MCSC_R_POST_CHAIN0_BCHS_HS2], reg_val);
		break;
	case MCSC_OUTPUT1:
	case MCSC_OUTPUT2:
	case MCSC_OUTPUT3:
	case MCSC_OUTPUT4:
		/* not support */;
		break;
	default:
		break;
	}
}

void is_scaler_set_bchs_clamp(void __iomem *base_addr, u32 output_id,
	u32 y_max, u32 y_min, u32 c_max, u32 c_min)
{
	u32 reg_val = 0;

	switch (output_id) {
	case MCSC_OUTPUT0:
		reg_val = is_hw_set_field_value(reg_val, &mcsc_fields[MCSC_F_POST_CHAIN0_BCHS_Y_CLAMP_MAX], y_max);
		reg_val = is_hw_set_field_value(reg_val, &mcsc_fields[MCSC_F_POST_CHAIN0_BCHS_Y_CLAMP_MIN], y_min);
		reg_val = is_hw_set_field_value(reg_val, &mcsc_fields[MCSC_F_POST_CHAIN0_BCHS_C_CLAMP_MAX], c_max);
		reg_val = is_hw_set_field_value(reg_val, &mcsc_fields[MCSC_F_POST_CHAIN0_BCHS_C_CLAMP_MIN], c_min);
		break;
	case MCSC_OUTPUT1:
	case MCSC_OUTPUT2:
	case MCSC_OUTPUT3:
	case MCSC_OUTPUT4:
		/* not support */;
		break;
	default:
		return;
	}

	is_hw_set_reg(base_addr, &mcsc_regs[MCSC_R_POST_CHAIN0_BCHS_CLAMP], reg_val);
	dbg_hw(2, "[OUT:%d]set_bchs_clamp: [%#x]%#x, C:[%#x]%#x\n", output_id,
			mcsc_regs[MCSC_R_POST_CHAIN0_BCHS_CLAMP].sfr_offset,
			is_hw_get_reg(base_addr, &mcsc_regs[MCSC_R_POST_CHAIN0_BCHS_CLAMP]));
}

void is_scaler_set_rdma_enable(void __iomem *base_addr, u32 hw_id, bool dma_in_en)
{
	/* not support */
}

void is_scaler_set_dma_out_enable(void __iomem *base_addr, u32 output_id, bool dma_out_en)
{
	switch (output_id) {
	case MCSC_OUTPUT0:
		is_hw_set_field(base_addr, &mcsc_regs[MCSC_R_POST_CHAIN0_DMA_OUT_CTRL],
			&mcsc_fields[MCSC_F_POST_CHAIN0_DMA_OUT_ENABLE], (u32)dma_out_en);
		break;
	case MCSC_OUTPUT1:
	case MCSC_OUTPUT2:
	case MCSC_OUTPUT3:
	case MCSC_OUTPUT4:
		/* not support */;
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
		ret = is_hw_get_field(base_addr, &mcsc_regs[MCSC_R_POST_CHAIN0_DMA_OUT_CTRL],
			&mcsc_fields[MCSC_F_POST_CHAIN0_DMA_OUT_ENABLE]);
		break;
	case MCSC_OUTPUT1:
	case MCSC_OUTPUT2:
	case MCSC_OUTPUT3:
	case MCSC_OUTPUT4:
		/* not support */;
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
	u32 rdma_format = MCSC_YUV422_1P_YUYV;

	switch (hw_id) {
	case DEV_HW_MCSC0:
		warn_hw("invalid rdma hw_id(%d)\n", hw_id);
		break;
	case DEV_HW_MCSC1:
		rdma_format = is_hw_get_field(base_addr, &mcsc_regs[MCSC_R_RDMA_DATA_FORMAT],
			&mcsc_fields[MCSC_F_RDMA_DATA_FORMAT]);
		break;
	default:
		warn_hw("invalid hw_id(%d) for MCSC api\n", hw_id);
		break;
	}

	return rdma_format;
}

void is_scaler_set_rdma_format(void __iomem *base_addr, u32 hw_id, u32 in_fmt)
{
	switch (hw_id) {
	case DEV_HW_MCSC0:
		warn_hw("invalid rdma hw_id(%d)\n", hw_id);
		break;
	case DEV_HW_MCSC1:
		is_hw_set_field(base_addr, &mcsc_regs[MCSC_R_RDMA_DATA_FORMAT],
			&mcsc_fields[MCSC_F_RDMA_DATA_FORMAT], in_fmt);
		break;
	default:
		warn_hw("invalid hw_id(%d) for MCSC api\n", hw_id);
		break;
	}
}

u32 is_scaler_get_mono_ctrl(void __iomem *base_addr, u32 hw_id)
{
	/* not support */
	return 0;
}

void is_scaler_set_mono_ctrl(void __iomem *base_addr, u32 hw_id, u32 in_fmt)
{
	/* not support */
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
		reg_val = is_hw_set_field_value(reg_val, &mcsc_fields[MCSC_F_WDMA0_MONO_ENABLE], en);
		is_hw_set_reg(base_addr, &mcsc_regs[MCSC_R_WDMA0_MONO_ENABLE], reg_val);
		break;
	case MCSC_OUTPUT1:
	case MCSC_OUTPUT2:
	case MCSC_OUTPUT3:
	case MCSC_OUTPUT4:
		/* not support */;
		break;
	default:
		break;
	}
}

void is_scaler_set_wdma_rgb_coefficient(void __iomem *base_addr)
{
	/* not support */;
}

void is_scaler_set_wdma_color_type(void __iomem *base_addr, u32 output_id, u32 color_type)
{
	/* not support */;
}
void is_scaler_set_wdma_rgb_format(void __iomem *base_addr, u32 output_id, u32 out_fmt)
{
	/* not support */;
}

void is_scaler_set_wdma_yuv_format(void __iomem *base_addr, u32 output_id, u32 out_fmt)
{
	switch (output_id) {
	case MCSC_OUTPUT0:
		is_hw_set_field(base_addr, &mcsc_regs[MCSC_R_WDMA0_DATA_FORMAT],
			&mcsc_fields[MCSC_F_WDMA0_DATA_FORMAT], out_fmt);
		break;
	case MCSC_OUTPUT1:
	case MCSC_OUTPUT2:
	case MCSC_OUTPUT3:
	case MCSC_OUTPUT4:
		/* not support */;
		break;
	default:
		break;
	}
}

void is_scaler_set_wdma_format(void __iomem *base_addr, u32 hw_id, u32 output_id, u32 plane, u32 out_fmt)
{
	u32 in_fmt;

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
		warn_hw("[ID:%d]wdma format(%d): RGB format is not supported!\n",
			hw_id, out_fmt);
		break;
	default:
		break;
	}

	if (out_fmt == MCSC_MONO_Y8) {
		is_scaler_set_wdma_mono_ctrl(base_addr, output_id, out_fmt, true);
		/* 1 plane : mono ctrl should be disabled */
		is_scaler_set_wdma_yuv_format(base_addr, output_id, MCSC_YUV422_2P_UFIRST);
	} else {
		is_scaler_set_wdma_mono_ctrl(base_addr, output_id, out_fmt, false);
		is_scaler_set_wdma_yuv_format(base_addr, output_id, out_fmt);
	}
}

void is_scaler_set_wdma_dither(void __iomem *base_addr, u32 output_id,
	u32 fmt, u32 bitwidth, u32 plane, enum exynos_sensor_position sensor_position)
{
	/* not support */;
}

void is_scaler_get_wdma_format(void __iomem *base_addr, u32 output_id, u32 color_type, u32 *out_fmt)
{
	/* color_type 0 : YUV, 1 : RGB */
	switch (output_id) {
	case MCSC_OUTPUT0:
		*out_fmt = is_hw_get_field(base_addr, &mcsc_regs[MCSC_R_WDMA0_DATA_FORMAT],
			&mcsc_fields[MCSC_F_WDMA0_DATA_FORMAT]);
		break;
	case MCSC_OUTPUT1:
	case MCSC_OUTPUT2:
	case MCSC_OUTPUT3:
	case MCSC_OUTPUT4:
		/* not support */;
		break;
	default:
		break;
	}
}

void is_scaler_set_swap_mode(void __iomem *base_addr, u32 output_id, u32 swap)
{
	switch (output_id) {
	case MCSC_OUTPUT0:
		is_hw_set_field(base_addr, &mcsc_regs[MCSC_R_WDMA0_SWAP_TABLE],
			&mcsc_fields[MCSC_F_WDMA0_SWAP_TABLE], swap);
		break;
	case MCSC_OUTPUT1:
	case MCSC_OUTPUT2:
	case MCSC_OUTPUT3:
	case MCSC_OUTPUT4:
		/* not support */;
		break;
	default:
		break;
	}
}

void is_scaler_set_max_mo(void __iomem *base_addr, u32 output_id, u32 mo)
{
	/* not support */;
}

void is_scaler_set_flip_mode(void __iomem *base_addr, u32 output_id, u32 flip)
{
	switch (output_id) {
	case MCSC_OUTPUT0:
		is_hw_set_field(base_addr, &mcsc_regs[MCSC_R_WDMA0_FLIP_CONTROL],
			&mcsc_fields[MCSC_F_WDMA0_FLIP_CONTROL], flip);
		break;
	case MCSC_OUTPUT1:
	case MCSC_OUTPUT2:
	case MCSC_OUTPUT3:
	case MCSC_OUTPUT4:
		/* not support */;
		break;
	default:
		break;
	}
}

void is_scaler_set_rdma_size(void __iomem *base_addr, u32 width, u32 height)
{
	is_hw_set_field(base_addr, &mcsc_regs[MCSC_R_RDMA_WIDTH],
		&mcsc_fields[MCSC_F_RDMA_WIDTH], width);
	is_hw_set_field(base_addr, &mcsc_regs[MCSC_R_RDMA_HEIGHT],
		&mcsc_fields[MCSC_F_RDMA_HEIGHT], height);
}

void is_scaler_get_rdma_size(void __iomem *base_addr, u32 *width, u32 *height)
{
	*width = is_hw_get_field(base_addr, &mcsc_regs[MCSC_R_RDMA_WIDTH],
		&mcsc_fields[MCSC_F_RDMA_WIDTH]);
	*height = is_hw_get_field(base_addr, &mcsc_regs[MCSC_R_RDMA_HEIGHT],
		&mcsc_fields[MCSC_F_RDMA_HEIGHT]);
}

void is_scaler_set_wdma_size(void __iomem *base_addr, u32 output_id, u32 width, u32 height)
{
	switch (output_id) {
	case MCSC_OUTPUT0:
		is_hw_set_field(base_addr, &mcsc_regs[MCSC_R_WDMA0_WIDTH],
			&mcsc_fields[MCSC_F_WDMA0_WIDTH], width);
		is_hw_set_field(base_addr, &mcsc_regs[MCSC_R_WDMA0_HEIGHT],
			&mcsc_fields[MCSC_F_WDMA0_HEIGHT], height);
		break;
	case MCSC_OUTPUT1:
	case MCSC_OUTPUT2:
	case MCSC_OUTPUT3:
	case MCSC_OUTPUT4:
		/* not support */;
		break;
	default:
		break;
	}
}

void is_scaler_get_wdma_size(void __iomem *base_addr, u32 output_id, u32 *width, u32 *height)
{
	switch (output_id) {
	case MCSC_OUTPUT0:
		*width = is_hw_get_field(base_addr, &mcsc_regs[MCSC_R_WDMA0_WIDTH],
			&mcsc_fields[MCSC_F_WDMA0_WIDTH]);
		*height = is_hw_get_field(base_addr, &mcsc_regs[MCSC_R_WDMA0_HEIGHT],
			&mcsc_fields[MCSC_F_WDMA0_HEIGHT]);
		break;
	case MCSC_OUTPUT1:
	case MCSC_OUTPUT2:
	case MCSC_OUTPUT3:
	case MCSC_OUTPUT4:
		/* not support */;
		break;
	default:
		break;
	}
}

void is_scaler_set_rdma_stride(void __iomem *base_addr, u32 y_stride, u32 uv_stride)
{
	u32 reg_val = 0;

	reg_val = is_hw_set_field_value(reg_val, &mcsc_fields[MCSC_F_RDMA_C_STRIDE], uv_stride);
	reg_val = is_hw_set_field_value(reg_val, &mcsc_fields[MCSC_F_RDMA_Y_STRIDE], y_stride);

	is_hw_set_reg(base_addr, &mcsc_regs[MCSC_R_RDMA_STRIDE], reg_val);
}

void is_scaler_set_rdma_2bit_stride(void __iomem *base_addr, u32 y_2bit_stride, u32 uv_2bit_stride)
{
	/* not support */
}

void is_scaler_get_rdma_stride(void __iomem *base_addr, u32 *y_stride, u32 *uv_stride)
{
	*y_stride = is_hw_get_field(base_addr, &mcsc_regs[MCSC_R_RDMA_STRIDE],
		&mcsc_fields[MCSC_F_RDMA_Y_STRIDE]);
	*uv_stride = is_hw_get_field(base_addr, &mcsc_regs[MCSC_R_RDMA_STRIDE],
		&mcsc_fields[MCSC_F_RDMA_C_STRIDE]);
}

void is_scaler_set_wdma_stride(void __iomem *base_addr, u32 output_id, u32 y_stride, u32 uv_stride)
{
	switch (output_id) {
	case MCSC_OUTPUT0:
		is_hw_set_field(base_addr, &mcsc_regs[MCSC_R_WDMA0_STRIDE],
			&mcsc_fields[MCSC_F_WDMA0_Y_STRIDE], y_stride);
		is_hw_set_field(base_addr, &mcsc_regs[MCSC_R_WDMA0_STRIDE],
			&mcsc_fields[MCSC_F_WDMA0_C_STRIDE], uv_stride);
		break;
	case MCSC_OUTPUT1:
	case MCSC_OUTPUT2:
	case MCSC_OUTPUT3:
	case MCSC_OUTPUT4:
		/* not support */;
		break;
	default:
		break;
	}
}

void is_scaler_set_wdma_2bit_stride(void __iomem *base_addr, u32 output_id,
	u32 y_2bit_stride, u32 uv_2bit_stride)
{
	/* not support */
}


void is_scaler_get_wdma_stride(void __iomem *base_addr, u32 output_id, u32 *y_stride, u32 *uv_stride)
{
	switch (output_id) {
	case MCSC_OUTPUT0:
		*y_stride = is_hw_get_field(base_addr, &mcsc_regs[MCSC_R_WDMA0_STRIDE],
			&mcsc_fields[MCSC_F_WDMA0_Y_STRIDE]);
		*uv_stride = is_hw_get_field(base_addr, &mcsc_regs[MCSC_R_WDMA0_STRIDE],
			&mcsc_fields[MCSC_F_WDMA0_C_STRIDE]);
		break;
	case MCSC_OUTPUT1:
	case MCSC_OUTPUT2:
	case MCSC_OUTPUT3:
	case MCSC_OUTPUT4:
		/* not support */;
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

void get_rdma_addr_arr(u32 *addr)
{
	addr[0] = MCSC_R_RDMA_BASE_ADDR00;
	addr[1] = MCSC_R_RDMA_BASE_ADDR01;
	addr[2] = MCSC_R_RDMA_BASE_ADDR02;
	addr[3] = MCSC_R_RDMA_BASE_ADDR03;
	addr[4] = MCSC_R_RDMA_BASE_ADDR04;
	addr[5] = MCSC_R_RDMA_BASE_ADDR05;
	addr[6] = MCSC_R_RDMA_BASE_ADDR06;
	addr[7] = MCSC_R_RDMA_BASE_ADDR07;
}

void is_scaler_set_rdma_addr(void __iomem *base_addr,
	u32 y_addr, u32 cb_addr, u32 cr_addr, int buf_index)
{
	u32 addr[8] = {0, };

	get_rdma_addr_arr(addr);
	if (!addr[0])
		return;

	is_hw_set_reg(base_addr, &mcsc_regs[addr[buf_index]], y_addr);
	is_hw_set_reg(base_addr, &mcsc_regs[addr[buf_index] + 8], cb_addr);
	is_hw_set_reg(base_addr, &mcsc_regs[addr[buf_index] + 16], cr_addr);

	is_hw_set_reg(base_addr, &mcsc_regs[MCSC_R_RDMA_BASE_ADDR_EN], 1 << buf_index);
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
		addr[0] = MCSC_R_WDMA0_BASE_ADDR00;
		addr[1] = MCSC_R_WDMA0_BASE_ADDR01;
		addr[2] = MCSC_R_WDMA0_BASE_ADDR02;
		addr[3] = MCSC_R_WDMA0_BASE_ADDR03;
		addr[4] = MCSC_R_WDMA0_BASE_ADDR04;
		addr[5] = MCSC_R_WDMA0_BASE_ADDR05;
		addr[6] = MCSC_R_WDMA0_BASE_ADDR06;
		addr[7] = MCSC_R_WDMA0_BASE_ADDR07;
		break;
	case MCSC_OUTPUT1:
	case MCSC_OUTPUT2:
	case MCSC_OUTPUT3:
	case MCSC_OUTPUT4:
		/* not support */;
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
	is_hw_set_reg(base_addr, &mcsc_regs[addr[buf_index] + 8], cb_addr);
	is_hw_set_reg(base_addr, &mcsc_regs[addr[buf_index] + 16], cr_addr);

	is_hw_set_reg(base_addr, &mcsc_regs[MCSC_R_WDMA0_BASE_ADDR_EN], 1 << buf_index);
}

void is_scaler_set_wdma_2bit_addr(void __iomem *base_addr, u32 output_id,
	u32 y_2bit_addr, u32 cbcr_2bit_addr, int buf_index)
{
	/* not support */
}

void is_scaler_get_wdma_addr(void __iomem *base_addr, u32 output_id,
	u32 *y_addr, u32 *cb_addr, u32 *cr_addr, int buf_index)
{
	u32 addr[8] = {0, };

	get_wdma_addr_arr(output_id, addr);
	if (!addr[0])
		return;

	*y_addr = is_hw_get_reg(base_addr, &mcsc_regs[addr[buf_index]]);
	*cb_addr = is_hw_get_reg(base_addr, &mcsc_regs[addr[buf_index] + 8]);
	*cr_addr = is_hw_get_reg(base_addr, &mcsc_regs[addr[buf_index] + 16]);
}

void is_scaler_clear_rdma_addr(void __iomem *base_addr)
{
	u32 addr[8] = {0, }, buf_index;

	get_rdma_addr_arr(addr);
	if (!addr[0])
		return;

	for (buf_index = 0; buf_index < 8; buf_index++) {
		is_hw_set_reg(base_addr, &mcsc_regs[addr[buf_index]], 0x0);
		is_hw_set_reg(base_addr, &mcsc_regs[addr[buf_index] + 8], 0x0);
		is_hw_set_reg(base_addr, &mcsc_regs[addr[buf_index] + 16], 0x0);
	}
}

void is_scaler_clear_wdma_addr(void __iomem *base_addr, u32 output_id)
{
	u32 addr[8] = {0, }, buf_index;

	get_wdma_addr_arr(output_id, addr);
	if (!addr[0])
		return;

	for (buf_index = 0; buf_index < 8; buf_index++) {
		is_hw_set_reg(base_addr, &mcsc_regs[addr[buf_index]], 0x0);
		is_hw_set_reg(base_addr, &mcsc_regs[addr[buf_index] + 8], 0x0);
		is_hw_set_reg(base_addr, &mcsc_regs[addr[buf_index] + 16], 0x0);
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
			&mcsc_fields[MCSC_F_INDEX_RESET], reset);
}

void is_scaler_set_hwfc_mode(void __iomem *base_addr, u32 hwfc_output_ids)
{
	u32 val = MCSC_HWFC_MODE_OFF;
	u32 read_val;

	read_val = is_hw_get_field(base_addr, &mcsc_regs[MCSC_R_HWFC_MODE],
			&mcsc_fields[MCSC_F_HWFC_MODE]);

	/* Only One Output port */
	val = MCSC_HWFC_MODE_REGION_A_B_PORT;

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
		val = is_hw_get_reg(base_addr, &mcsc_regs[MCSC_R_HWFC_CONFIG_IMAGE_A]);
		/* format */
		val = is_hw_set_field_value(val, &mcsc_fields[MCSC_F_FORMAT_A], hwfc_fmt);

		/* plane */
		val = is_hw_set_field_value(val, &mcsc_fields[MCSC_F_PLANE_A], plane);

		/* dma idx */
		val = is_hw_set_field_value(val, &mcsc_fields[MCSC_F_ID0_A], dma_idx);
		val = is_hw_set_field_value(val, &mcsc_fields[MCSC_F_ID1_A], dma_idx+1);
		val = is_hw_set_field_value(val, &mcsc_fields[MCSC_F_ID2_A], dma_idx+2);

		is_hw_set_reg(base_addr, &mcsc_regs[MCSC_R_HWFC_CONFIG_IMAGE_A], val);

		/* total pixel/byte */
		is_hw_set_reg(base_addr, &mcsc_regs[MCSC_R_HWFC_TOTAL_IMAGE_BYTE0_A], total_img_byte0);
		is_hw_set_reg(base_addr, &mcsc_regs[MCSC_R_HWFC_TOTAL_IMAGE_BYTE1_A], total_img_byte1);
		is_hw_set_reg(base_addr, &mcsc_regs[MCSC_R_HWFC_TOTAL_IMAGE_BYTE2_A], total_img_byte2);
		is_hw_set_reg(base_addr, &mcsc_regs[MCSC_R_HWFC_TOTAL_WIDTH_BYTE0_A], total_width_byte0);
		is_hw_set_reg(base_addr, &mcsc_regs[MCSC_R_HWFC_TOTAL_WIDTH_BYTE1_A], total_width_byte1);
		is_hw_set_reg(base_addr, &mcsc_regs[MCSC_R_HWFC_TOTAL_WIDTH_BYTE2_A], total_width_byte2);
		break;
	case MCSC_OUTPUT1:
	case MCSC_OUTPUT2:
	case MCSC_OUTPUT3:
	case MCSC_OUTPUT4:
		/* not support */;
		break;
	}

}

u32 is_scaler_get_hwfc_idx_bin(void __iomem *base_addr, u32 output_id)
{
	u32 val = 0;

	switch (output_id) {
	case MCSC_OUTPUT0:
		val = is_hw_get_field(base_addr, &mcsc_regs[MCSC_R_HWFC_REGION_IDX_BIN],
			&mcsc_fields[MCSC_F_REGION_IDX_BIN_A]);
		break;
	case MCSC_OUTPUT1:
	case MCSC_OUTPUT2:
	case MCSC_OUTPUT3:
	case MCSC_OUTPUT4:
		/* not support */;
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
		val = is_hw_get_field(base_addr, &mcsc_regs[MCSC_R_HWFC_CURR_REGION],
			&mcsc_fields[MCSC_F_CURR_REGION_A]);
		break;
	case MCSC_OUTPUT1:
	case MCSC_OUTPUT2:
	case MCSC_OUTPUT3:
	case MCSC_OUTPUT4:
		/* not support */;
		break;
	default:
		break;
	}

	return val;
}

/* for DJAG */
void is_scaler_set_djag_enable(void __iomem *base_addr, u32 djag_enable)
{
	/* not support */
}

void is_scaler_set_djag_src_size(void __iomem *base_addr, u32 width, u32 height)
{
	/* not support */
}

void is_scaler_set_djag_dst_size(void __iomem *base_addr, u32 width, u32 height)
{
	/* not support */
}

void is_scaler_set_djag_scaling_ratio(void __iomem *base_addr, u32 hratio, u32 vratio)
{
	/* not support */
}

void is_scaler_set_djag_init_phase_offset(void __iomem *base_addr, u32 h_offset, u32 v_offset)
{
	/* not support */
}

void is_scaler_set_djag_round_mode(void __iomem *base_addr, u32 round_enable)
{
	/* not support */
}

void is_scaler_set_djag_tunning_param(void __iomem *base_addr,
	const struct djag_scaling_ratio_depended_config *djag_tune)
{
	/* not support */
}

void is_scaler_set_djag_dither_wb(void __iomem *base_addr, struct djag_wb_thres_cfg *djag_wb, u32 wht, u32 blk)
{
	/* not support */
}

/* for CAC */
void is_scaler_set_cac_enable(void __iomem *base_addr, u32 en)
{
	/* not support */
}

void is_scaler_set_cac_map_crt_thr(void __iomem *base_addr, struct cac_cfg_by_ni *cfg)
{
	/* not support */
}

/* LFRO : Less Fast Read Out */
void is_scaler_set_lfro_mode_enable(void __iomem *base_addr, u32 hw_id, u32 lfro_enable, u32 lfro_total_fnum)
{
	/* not support */
}

u32 is_scaler_get_lfro_mode_status(void __iomem *base_addr, u32 hw_id)
{
	/* not support */
	return 0;
}

/* for Strip */
u32 is_scaler_get_djag_strip_enable(void __iomem *base_addr, u32 output_id)
{
	/* not support */
	return 0;
}

void is_scaler_set_djag_strip_enable(void __iomem *base_addr, u32 output_id, u32 enable)
{
	/* not support */
}

u32 is_scaler_get_djag_out_crop_enable(void __iomem *base_addr, u32 output_id)
{
	/* not support */
	return 0;
}

void is_scaler_set_djag_out_crop_enable(void __iomem *base_addr, u32 output_id, u32 enable, u32 pre_dst_h, u32 start_pos_h)
{
	/* not support */
}

void is_scaler_set_djag_out_crop_size(void __iomem *base_addr, u32 output_id,
	u32 pos_x, u32 pos_y, u32 width, u32 height)
{
	/* not support */
}

void is_scaler_get_djag_strip_config(void __iomem *base_addr, u32 output_id, u32 *pre_dst_h, u32 *start_pos_h)
{
	/* not support */
}

void is_scaler_set_djag_strip_config(void __iomem *base_addr, u32 output_id, u32 pre_dst_h, u32 start_pos_h)
{
	/* not support */
}

u32 is_scaler_get_poly_strip_enable(void __iomem *base_addr, u32 output_id)
{
	/* not support */
	return 0;
}

void is_scaler_set_poly_strip_enable(void __iomem *base_addr, u32 output_id, u32 enable)
{
	/* not support */
}

void is_scaler_get_poly_strip_config(void __iomem *base_addr, u32 output_id, u32 *pre_dst_h, u32 *start_pos_h)
{
	/* not support */
}

void is_scaler_set_poly_strip_config(void __iomem *base_addr, u32 output_id, u32 pre_dst_h, u32 start_pos_h)
{
	/* not support */
}

u32 is_scaler_get_poly_out_crop_enable(void __iomem *base_addr, u32 output_id)
{
	/* not support */
	return 0;
}

void is_scaler_set_poly_out_crop_enable(void __iomem *base_addr, u32 output_id, u32 enable)
{
	/* not support */
}

void is_scaler_get_poly_out_crop_size(void __iomem *base_addr, u32 output_id, u32 *width, u32 *height)
{
	/* not support */
}

void is_scaler_set_poly_out_crop_size(void __iomem *base_addr, u32 output_id,
	u32 pos_x, u32 pos_y, u32 width, u32 height)
{
	/* not support */
}

u32 is_scaler_get_post_strip_enable(void __iomem *base_addr, u32 output_id)
{
	/* not support */
	return 0;
}

void is_scaler_set_post_strip_enable(void __iomem *base_addr, u32 output_id, u32 enable)
{
	/* not support */
}

void is_scaler_get_post_strip_config(void __iomem *base_addr, u32 output_id, u32 *pre_dst_h, u32 *start_pos_h)
{
	/* not support */
}

void is_scaler_set_post_strip_config(void __iomem *base_addr, u32 output_id, u32 pre_dst_h, u32 start_pos_h)
{
	/* not support */
}

u32 is_scaler_get_post_out_crop_enable(void __iomem *base_addr, u32 output_id)
{
	/* not support */
	return 0;
}

void is_scaler_set_post_out_crop_enable(void __iomem *base_addr, u32 output_id, u32 enable)
{
	/* not support */
}

void is_scaler_get_post_out_crop_size(void __iomem *base_addr, u32 output_id, u32 *width, u32 *height)
{
	/* not support */
}

void is_scaler_set_post_out_crop_size(void __iomem *base_addr, u32 output_id,
	u32 pos_x, u32 pos_y, u32 width, u32 height)
{
	/* not support */
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

	if (status & (1 << INTR_MC_SCALER_INPUT_PROTOCOL_ERR))
		is_hw_set_reg(base_addr, &mcsc_regs[MCSC_R_SCALER_INTERRUPT_0],
			(u32)1 << INTR_MC_SCALER_INPUT_PROTOCOL_ERR);

	if (status & (1 << INTR_MC_SCALER_SHADOW_TRIGGER))
		is_hw_set_reg(base_addr, &mcsc_regs[MCSC_R_SCALER_INTERRUPT_0],
			(u32)1 << INTR_MC_SCALER_SHADOW_TRIGGER);

	if (status & (1 << INTR_MC_SCALER_SHADOW_HW_TRIGGER))
		is_hw_set_reg(base_addr, &mcsc_regs[MCSC_R_SCALER_INTERRUPT_0],
			(u32)1 << INTR_MC_SCALER_SHADOW_HW_TRIGGER);

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
	case DEV_HW_MCSC1:
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
	case DEV_HW_MCSC1:
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
	case DEV_HW_MCSC1:
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

	/* not support */

	return ret;
}

u32 is_scaler_get_version(void __iomem *base_addr)
{
	return is_hw_get_reg(base_addr, &mcsc_regs[MCSC_R_SCALER_VERSION]);
}

u32 is_scaler_get_idle_status(void __iomem *base_addr, u32 hw_id)
{
	int ret = 0;

	ret = is_hw_get_field(base_addr,
		&mcsc_regs[MCSC_R_SCALER_RUNNING_STATUS], &mcsc_fields[MCSC_F_SCALER_IDLE_0]);

	return ret;
}

void is_scaler_dump(void __iomem *base_addr)
{
	u32 i = 0;
	u32 reg_val = 0;

	info_hw("MCSC ver 8.20/8.21");

	for (i = 0; i < MCSC_REG_CNT; i++) {
		reg_val = readl(base_addr + mcsc_regs[i].sfr_offset);
		sfrinfo("[DUMP] reg:[%s][0x%04X], value:[0x%08X]\n",
			mcsc_regs[i].reg_name, mcsc_regs[i].sfr_offset, reg_val);
	}
}
