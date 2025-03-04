// SPDX-License-Identifier: GPL-2.0
/*
 * Samsung Exynos SoC series Pablo driver
 *
 * ypp HW control APIs
 *
 * Copyright (C) 2020 Samsung Electronics Co., Ltd.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */


#include <linux/delay.h>
#include <linux/soc/samsung/exynos-soc.h>
#include "is-hw-api-yuvpp-v2_1.h"
#include "is-hw-common-dma.h"
#include "is-hw.h"
#include "is-hw-control.h"
#include "sfr/is-sfr-yuvpp-v2_1.h"

#define YPP_SET_F(base, R, F, val) \
	is_hw_set_field(base, &yuvpp_regs[R], &yuvpp_fields[F], val)
#define YPP_SET_F_DIRECT(base, R, F, val) \
	is_hw_set_field(base, &yuvpp_regs[R], &yuvpp_fields[F], val)
#define YPP_SET_R(base, R, val) \
	is_hw_set_reg(base, &yuvpp_regs[R], val)
#define YPP_SET_R_DIRECT(base, R, val) \
	is_hw_set_reg(base, &yuvpp_regs[R], val)
#define YPP_SET_V(reg_val, F, val) \
	is_hw_set_field_value(reg_val, &yuvpp_fields[F], val)

#define YPP_GET_F(base, R, F) \
	is_hw_get_field(base, &yuvpp_regs[R], &yuvpp_fields[F])
#define YPP_GET_R(base, R) \
	is_hw_get_reg(base, &yuvpp_regs[R])
#define YPP_GET_R_COREX(base, R) \
	is_hw_get_reg(base, &yuvpp_regs[R])
#define YPP_GET_V(reg_val, F) \
	is_hw_get_field_value(reg_val, &yuvpp_fields[F])

unsigned int ypp_hw_is_occured(unsigned int status, enum ypp_event_type type)
{
	u32 mask;

	switch (type) {
	case INTR_FRAME_START:
		mask = 1 << INTR_YUVPP_OP_START;
		break;
	case INTR_FRAME_END:
		mask = 1 << INTR_YUVPP_FRAME_END;
		break;
	case INTR_COREX_END_0:
		mask = 1 << INTR_YUVPP_COREX_END_0;
		break;
	case INTR_COREX_END_1:
		mask = 1 << INTR_YUVPP_COREX_END_1;
		break;
	case INTR_ERR:
		mask = INT_ERR_MASK;
		break;
	default:
		return 0;
	}

	return status & mask;
}

int ypp_hw_s_reset(void __iomem *base)
{
	u32 reset_count = 0;
	u32 temp = 0;

	YPP_SET_R(base, YUVPP_R_SW_RESET, 0x1);

	while (1) {
		temp = YPP_GET_R(base, YUVPP_R_SW_RESET);
		if (temp == 0)
			break;
		if (reset_count > YUVPP_TRY_COUNT)
			return reset_count;
		reset_count++;
	}

	return 0;
}

void ypp_hw_s_init(void __iomem *base)
{
	YPP_SET_F(base, YUVPP_R_AUTO_MASK_PREADY, YUVPP_F_AUTO_MASK_PREADY, 1);
	YPP_SET_F(base, YUVPP_R_IP_PROCESSING, YUVPP_F_IP_PROCESSING, 1);
}

int ypp_hw_wait_idle(void __iomem *base)
{
	int ret = SET_SUCCESS;
	u32 idle;
	u32 int_all;
	u32 try_cnt = 0;

	idle = YPP_GET_F(base, YUVPP_R_IDLENESS_STATUS, YUVPP_F_IDLENESS_STATUS);
	int_all = YPP_GET_R(base, YUVPP_R_CONTINT_INT);

	info_hw("[YUVPP] idle status before disable (idle:%d, int1:0x%X)\n",
			idle, int_all);

	while (!idle) {
		idle = YPP_GET_F(base, YUVPP_R_IDLENESS_STATUS, YUVPP_F_IDLENESS_STATUS);

		try_cnt++;
		if (try_cnt >= YUVPP_TRY_COUNT) {
			err_hw("[YUVPP] timeout waiting idle - disable fail");
			ypp_hw_dump(base);
			ret = -ETIME;
			break;
		}

		usleep_range(3, 4);
	};

	int_all = YPP_GET_R(base, YUVPP_R_CONTINT_INT);

	info_hw("[YUVPP] idle status after disable (total:%d, curr:%d, idle:%d, int1:0x%X)\n",
			idle, int_all);

	return ret;
}

void ypp_hw_dump(void __iomem *base)
{
	info_hw("[YUVPP] SFR DUMP (v2.0)\n");

	is_hw_dump_regs(base, yuvpp_regs, YUVPP_REG_CNT);
}

void ypp_hw_s_core(void __iomem *base, u32 num_buffers, u32 set_id)
{
	_ypp_hw_s_cout_fifo(base, set_id);
	_ypp_hw_s_common(base);
	_ypp_hw_s_int_mask(base);
	_ypp_hw_s_secure_id(base, set_id);
	_ypp_hw_s_fro(base, num_buffers, set_id);
}

void _ypp_hw_s_cout_fifo(void __iomem *base, u32 set_id)
{
	u32 val;

	val = 0;
	YPP_SET_R(base, YUVPP_R_COUTFIFO_OUTPUT_COUNT_AT_STALL, val);

	YPP_SET_R(base + GET_COREX_OFFSET(set_id), YUVPP_R_COUTFIFO_OUTPUT_T3_INTERVAL, HBLANK_CYCLE);

	val = 0;
	val = YPP_SET_V(val, YUVPP_F_COUTFIFO_OUTPUT_COL_ERROR_EN, 1);
	val = YPP_SET_V(val, YUVPP_F_COUTFIFO_OUTPUT_LINE_ERROR_EN, 1);
	val = YPP_SET_V(val, YUVPP_F_COUTFIFO_OUTPUT_TOTAL_SIZE_ERROR_EN, 1);
	YPP_SET_R(base, YUVPP_R_COUTFIFO_OUTPUT_ERROR_ENABLE, val);
	YPP_SET_F(base + GET_COREX_OFFSET(set_id),
			YUVPP_R_COUTFIFO_OUTPUT_COUTFIFO_ENABLE,
			YUVPP_F_COUTFIFO_OUTPUT_ENABLE, 1);
}

void _ypp_hw_s_common(void __iomem *base)
{
	YPP_SET_F(base, YUVPP_R_INTERRUPT_AUTO_MASK, YUVPP_F_INTERRUPT_AUTO_MASK, 1);
}

void _ypp_hw_s_int_mask(void __iomem *base)
{
	YPP_SET_F(base, YUVPP_R_CONTINT_LEVEL_PULSE_N_SEL, YUVPP_F_CONTINT_LEVEL_PULSE_N_SEL, INT_LEVEL);

	YPP_SET_F(base, YUVPP_R_CONTINT_INT_ENABLE, YUVPP_F_CONTINT_INT_ENABLE, INT_EN_MASK);
}

/*
 * Context: O
 * CR type: Shadow/Dual
 */
void _ypp_hw_s_secure_id(void __iomem *base, u32 set_id)
{
	/*
	 * Set Paramer Value
	 *
	 * scenario
	 * 0: Non-secure,  1: Secure
	 */
	YPP_SET_F(base + GET_COREX_OFFSET(set_id),
			YUVPP_R_SECU_CTRL_SEQID,
			YUVPP_F_SECU_CTRL_SEQID, 0); /* TODO: get secure scenario */
}

void _ypp_hw_s_fro(void __iomem *base, u32 num_buffers, u32 set_id)
{
	if (num_buffers > 1) {
		YPP_SET_F(base + GET_COREX_OFFSET(set_id), YUVPP_R_FRO_MODE_EN, YUVPP_F_FRO_MODE_EN, 1);
	} else {
		YPP_SET_F(base + GET_COREX_OFFSET(set_id), YUVPP_R_FRO_MODE_EN, YUVPP_F_FRO_MODE_EN, 0);
		return;
	}

	/* Param: frame */
	YPP_SET_F(base, YUVPP_R_FRO_FRAME_COUNT_TO_RUN_MINUS1_SHADOW,
			YUVPP_F_FRO_FRAME_COUNT_TO_RUN_MINUS1_SHADOW,
				num_buffers - 1);
}

void ypp_hw_g_hist_grid_num(void __iomem *base, u32 set_id, u32 *grid_x_num, u32 *grid_y_num)
{
	*grid_x_num = YPP_GET_F(base + GET_COREX_OFFSET(set_id), YUVPP_R_CLAHE_GRID_NUM, YUVPP_F_CLAHE_GRID_X_NUM);
	*grid_y_num = YPP_GET_F(base + GET_COREX_OFFSET(set_id), YUVPP_R_CLAHE_GRID_NUM, YUVPP_F_CLAHE_GRID_Y_NUM);
}

u32 ypp_hw_g_nadd_use_noise_rdma(void __iomem *base, u32 set_id)
{
	u32 nadd_bypass = YPP_GET_F(base + GET_COREX_OFFSET(set_id), YUVPP_R_NADD_BYPASS, YUVPP_F_NADD_BYPASS);
	u32 noise_select = YPP_GET_F(base + GET_COREX_OFFSET(set_id),
		YUVPP_R_NADD_NOISE_SELECT, YUVPP_F_NADD_NOISE_SELECT);
	u32 noise_gen_bypass = YPP_GET_F(base + GET_COREX_OFFSET(set_id),
		YUVPP_R_NOISE_GEN_BYPASS, YUVPP_F_NOISE_GEN_BYPASS);

	if (nadd_bypass)
		return 0;

	/* check olympus evt1.1 or orange evt0.1 */
	if (CHECK_YPP_NOISE_RDMA_DISALBE(exynos_soc_info))
		if (noise_select == 1 && noise_gen_bypass == 0)
			return 0;

	return 1;
}

u32 ypp_hw_g_in_crop_size_x(void __iomem *base, u32 set_id)
{
	u32 in_crop_size_x = 0;
	in_crop_size_x = YPP_GET_F(base + GET_COREX_OFFSET(set_id),
			YUVPP_R_YUVNR_IN_CROP_CTRL,
			YUVPP_F_YUVNR_IN_CROP_SIZE_X);
	return in_crop_size_x;
}

void ypp_hw_dma_dump (struct is_common_dma *dma)
{
	CALL_DMA_OPS(dma, dma_print_info, 0);
}

int ypp_hw_s_rdma_init(struct is_common_dma *dma, struct ypp_param_set *param_set,
	u32 enable, u32 grid_x_num, u32 grid_y_num, u32 in_crop_size_x, u32 cache_hint,
	u32 *sbwc_en, u32 *payload_size)
{
	struct param_dma_input *dma_input;
	u32 comp_sbwc_en = 0, comp_64b_align = 1, lossy_byte32num = 0;
	u32 stride_1p = 0, header_stride_1p = 0;
	u32 hwformat, memory_bitwidth, pixelsize, sbwc_type;
	u32 strip_width, width, height;
	u32 format, en_votf, bus_info;
	u32 strip_enable = (param_set->stripe_input.total_count == 0) ? 0 : 1;
	int ret = SET_SUCCESS;

	ret = CALL_DMA_OPS(dma, dma_enable, enable);
	if (enable == 0)
		return 0;

	switch (dma->id) {
	case YUVPP_RDMA_HIST:
		width = 2048;
		height = grid_x_num * grid_y_num;
		break;
	case YUVPP_RDMA_L2_Y:
	case YUVPP_RDMA_L2_1_Y:
	case YUVPP_RDMA_L2_2_Y:
	case YUVPP_RDMA_DRCGAIN:
		strip_width = (strip_enable) ? in_crop_size_x : param_set->dma_input.width;
		width = ((strip_width + 2) >> 2) + 1;
		height = ((param_set->dma_input.height + 2) >> 2) + 1;
		break;
	case YUVPP_RDMA_L2_UV:
	case YUVPP_RDMA_L2_1_UV:
	case YUVPP_RDMA_L2_2_UV:
		strip_width = (strip_enable) ? in_crop_size_x : param_set->dma_input.width;
		if ((strip_width % 8) == 0)
			width = (((strip_width + 4) >> 3) + 1) << 1;
		else
			width = ((strip_width + 4) >> 3) << 1;
		if ((param_set->dma_input.height % 8) == 0 ||
			(param_set->dma_input.height % 8) == 6)
			height = ((param_set->dma_input.height + 7) >> 3) + 1;
		else
			height = (param_set->dma_input.height + 7) >> 3;
		break;
	default:
		width = param_set->dma_input.width;
		height = param_set->dma_input.height;
		break;
	}

	switch (dma->id) {
	case YUVPP_RDMA_L0_Y:
	case YUVPP_RDMA_L0_UV:
		dma_input = &param_set->dma_input;
		lossy_byte32num = COMP_LOSSYBYTE32NUM_256X1_U10;
		break;
	case YUVPP_RDMA_L2_Y:
	case YUVPP_RDMA_L2_UV:
	case YUVPP_RDMA_L2_1_Y:
	case YUVPP_RDMA_L2_1_UV:
	case YUVPP_RDMA_L2_2_Y:
	case YUVPP_RDMA_L2_2_UV:
		dma_input = &param_set->dma_input_lv2;
		break;
	case YUVPP_RDMA_NOISE:
		dma_input = &param_set->dma_input_noise;
		lossy_byte32num = COMP_LOSSYBYTE32NUM_256X1_U10;
		break;
	case YUVPP_RDMA_DRCGAIN:
		dma_input = &param_set->dma_input_drc;
		break;
	case YUVPP_RDMA_HIST:
		dma_input = &param_set->dma_input_hist;
		break;
	default:
		err_hw("[YPP] invalid dma_id[%d]", dma->id);
		return SET_ERROR;
	}

	en_votf = dma_input->v_otf_enable;
	hwformat = dma_input->format;
	sbwc_type = dma_input->sbwc_type;
	memory_bitwidth = dma_input->bitwidth;
	pixelsize = dma_input->msb + 1;
	bus_info = en_votf ? (cache_hint << 4) : 0x00000000UL; /* cache hint [6:4] */

	*sbwc_en = comp_sbwc_en = is_hw_dma_get_comp_sbwc_en(sbwc_type, &comp_64b_align);
	format = is_hw_dma_get_bayer_format(memory_bitwidth, pixelsize, hwformat, comp_sbwc_en, true);
	if (comp_sbwc_en == 0)
		stride_1p = (dma->id == YUVPP_RDMA_HIST) ? 2048 : is_hw_dma_get_img_stride(
			memory_bitwidth, pixelsize, hwformat, width, true);
	else if (comp_sbwc_en == 1 || comp_sbwc_en == 2){
		stride_1p = is_hw_dma_get_payload_stride(comp_sbwc_en, pixelsize, width,
			comp_64b_align, lossy_byte32num, COMP_BLOCK_WIDTH, COMP_BLOCK_HEIGHT);
		header_stride_1p = (comp_sbwc_en == 1) ? is_hw_dma_get_header_stride(width, COMP_BLOCK_WIDTH) : 0;
	} else {
		return SET_ERROR;
	}

	ret |= CALL_DMA_OPS(dma, dma_set_format, format, DMA_FMT_BAYER);
	ret |= CALL_DMA_OPS(dma, dma_set_comp_sbwc_en, comp_sbwc_en);
	ret |= CALL_DMA_OPS(dma, dma_set_size, width, height);
	ret |= CALL_DMA_OPS(dma, dma_set_img_stride, stride_1p, 0, 0);
	ret |= CALL_DMA_OPS(dma, dma_votf_enable, en_votf, 0);
	ret |= CALL_DMA_OPS(dma, dma_set_bus_info, bus_info);

	*payload_size = 0;
	switch (comp_sbwc_en) {
	case 1:
		ret |= CALL_DMA_OPS(dma, dma_set_comp_64b_align, comp_64b_align);
		ret |= CALL_DMA_OPS(dma, dma_set_header_stride, header_stride_1p, 0);
		*payload_size = ((height + COMP_BLOCK_HEIGHT - 1) / COMP_BLOCK_HEIGHT) * stride_1p;
		break;
	case 2:
		ret |= CALL_DMA_OPS(dma, dma_set_comp_64b_align, comp_64b_align);
		ret |= CALL_DMA_OPS(dma, dma_set_comp_rate, lossy_byte32num);
		break;
	default:
		break;
	}

	return ret;
}

int ypp_hw_dma_create(struct is_common_dma *dma, void __iomem *base, u32 input_id)
{
	void __iomem *base_reg;
	ulong available_bayer_format_map;
	int ret = SET_SUCCESS;
	char name[64];

	switch (input_id) {
	case YUVPP_RDMA_L0_Y:
		base_reg = base + yuvpp_regs[YUVPP_R_RDMA_YUV_L0_Y_EN].sfr_offset;
		available_bayer_format_map = 0x77 & IS_BAYER_FORMAT_MASK;
		strncpy(name, "YUVPP_RDMA_L0_Y", sizeof(name) - 1);
		break;
	case YUVPP_RDMA_L0_UV:
		base_reg = base + yuvpp_regs[YUVPP_R_RDMA_YUV_L0_UV_EN].sfr_offset;
		available_bayer_format_map = 0x77 & IS_BAYER_FORMAT_MASK;
		strncpy(name, "YUVPP_RDMA_L0_UV", sizeof(name) - 1);
		break;
	case YUVPP_RDMA_L2_Y:
		base_reg = base + yuvpp_regs[YUVPP_R_RDMA_YUV_L2_Y_EN].sfr_offset;
		available_bayer_format_map = 0x77 & IS_BAYER_FORMAT_MASK;
		strncpy(name, "YUVPP_RDMA_L2_Y", sizeof(name) - 1);
		break;
	case YUVPP_RDMA_L2_UV:
		base_reg = base + yuvpp_regs[YUVPP_R_RDMA_YUV_L2_UV_EN].sfr_offset;
		available_bayer_format_map = 0x77 & IS_BAYER_FORMAT_MASK;
		strncpy(name, "YUVPP_RDMA_L2_UV", sizeof(name) - 1);
		break;
	case YUVPP_RDMA_L2_1_Y:
		base_reg = base + yuvpp_regs[YUVPP_R_RDMA_YUV_L2_1_Y_EN].sfr_offset;
		available_bayer_format_map = 0x77 & IS_BAYER_FORMAT_MASK;
		strncpy(name, "YUVPP_RDMA_L2_1_Y", sizeof(name) - 1);
		break;
	case YUVPP_RDMA_L2_1_UV:
		base_reg = base + yuvpp_regs[YUVPP_R_RDMA_YUV_L2_1_UV_EN].sfr_offset;
		available_bayer_format_map = 0x77 & IS_BAYER_FORMAT_MASK;
		strncpy(name, "YUVPP_RDMA_L2_1_UV", sizeof(name) - 1);
		break;
	case YUVPP_RDMA_L2_2_Y:
		base_reg = base + yuvpp_regs[YUVPP_R_RDMA_YUV_L2_2_Y_EN].sfr_offset;
		available_bayer_format_map = 0x77 & IS_BAYER_FORMAT_MASK;
		strncpy(name, "YUVPP_RDMA_L2_2_Y", sizeof(name) - 1);
		break;
	case YUVPP_RDMA_L2_2_UV:
		base_reg = base + yuvpp_regs[YUVPP_R_RDMA_YUV_L2_2_UV_EN].sfr_offset;
		available_bayer_format_map = 0x77 & IS_BAYER_FORMAT_MASK;
		strncpy(name, "YUVPP_RDMA_L2_2_UV", sizeof(name) - 1);
		break;
	case YUVPP_RDMA_NOISE:
		base_reg = base + yuvpp_regs[YUVPP_R_RDMA_NOISE_EN].sfr_offset;
		available_bayer_format_map = 0x77 & IS_BAYER_FORMAT_MASK;
		strncpy(name, "YUVPP_RDMA_NOISE", sizeof(name) - 1);
		break;
	case YUVPP_RDMA_DRCGAIN:
		base_reg = base + yuvpp_regs[YUVPP_R_RDMA_DRCGAIN_EN].sfr_offset;
		available_bayer_format_map = 0x1 & IS_BAYER_FORMAT_MASK;
		strncpy(name, "YUVPP_RDMA_DRCGAIN", sizeof(name) - 1);
		break;
	case YUVPP_RDMA_HIST:
		base_reg = base + yuvpp_regs[YUVPP_R_RDMA_HIST_EN].sfr_offset;
		available_bayer_format_map = 0x1 & IS_BAYER_FORMAT_MASK;
		strncpy(name, "YUVPP_RDMA_HIST", sizeof(name) - 1);
		break;
	default:
		err_hw("[YPP] invalid input_id[%d]", input_id);
		return SET_ERROR;
	}
	ret = is_hw_dma_set_ops(dma);
	ret |= is_hw_dma_create(dma, base_reg, input_id, name, available_bayer_format_map, 0, 0, 0);
	return ret;
}

void ypp_hw_s_rdma_corex_id(struct is_common_dma *dma, u32 set_id)
{
	CALL_DMA_OPS(dma, dma_set_corex_id, set_id);
}

int ypp_hw_s_rdma_addr(struct is_common_dma *dma, u32 *addr, u32 plane,
	u32 num_buffers, int buf_idx, u32 comp_sbwc_en, u32 payload_size)
{
	int ret = SET_SUCCESS, i = 0;
	dma_addr_t address[IS_MAX_FRO];
	dma_addr_t hdr_addr[IS_MAX_FRO];

	switch (dma->id) {
	case YUVPP_RDMA_L0_Y:
	case YUVPP_RDMA_L2_Y:
	case YUVPP_RDMA_L2_1_Y:
	case YUVPP_RDMA_L2_2_Y:
		for (i = 0; i < num_buffers; i++)
			address[i] = (dma_addr_t)*(addr + (2 * i));
		ret = CALL_DMA_OPS(dma, dma_set_img_addr, address, plane, buf_idx, num_buffers);
		break;
	case YUVPP_RDMA_L0_UV:
	case YUVPP_RDMA_L2_UV:
	case YUVPP_RDMA_L2_1_UV:
	case YUVPP_RDMA_L2_2_UV:
		for (i = 0; i < num_buffers; i++)
			address[i] = (dma_addr_t)*(addr + (2 * i + 1));
		ret = CALL_DMA_OPS(dma, dma_set_img_addr, address, plane, buf_idx, num_buffers);
		break;
	case YUVPP_RDMA_NOISE:
	case YUVPP_RDMA_DRCGAIN:
	case YUVPP_RDMA_HIST:
		for (i = 0; i < num_buffers; i++)
			address[i] = (dma_addr_t)*(addr + i);
		ret = CALL_DMA_OPS(dma, dma_set_img_addr,
			(dma_addr_t *)address, plane, buf_idx, num_buffers);
		break;
	default:
		err_hw("[YPP] invalid dma_id[%d]", dma->id);
		return SET_ERROR;
	}

	if (comp_sbwc_en == 1) {
		/* Lossless, need to set header base address */
		switch (dma->id) {
		case YUVPP_RDMA_L0_Y:
		case YUVPP_RDMA_L0_UV:
			for (i = 0; i < num_buffers; i++)
				hdr_addr[i] = address[i] + payload_size;
			break;
		case YUVPP_RDMA_NOISE:
			for (i = 0; i < num_buffers; i++)
				hdr_addr[i] = addr[i] + payload_size;
			break;
		default:
			break;
		}

		ret = CALL_DMA_OPS(dma, dma_set_header_addr, hdr_addr,
			plane, buf_idx, num_buffers);
	}

	return ret;
}

int ypp_hw_s_corex_update_type(void __iomem *base, u32 set_id)
{
	int ret = 0;
	switch (set_id) {
	case COREX_SET_A:
		YPP_SET_F(base, YUVPP_R_COREX_UPDATE_TYPE_0_SETA, YUVPP_F_COREX_UPDATE_TYPE_0_SETA, 1);
		/* YPP_SET_F(base, YUVPP_R_COREX_UPDATE_TYPE_1_SETA, YUVPP_F_COREX_UPDATE_TYPE_1_SETA, 1); */
		break;
	case COREX_SET_B:
		YPP_SET_F(base, YUVPP_R_COREX_UPDATE_TYPE_0_SETB, YUVPP_F_COREX_UPDATE_TYPE_0_SETB, 1);
		/*  YPP_SET_F(base, YUVPP_R_COREX_UPDATE_TYPE_1_SETB, YUVPP_F_COREX_UPDATE_TYPE_1_SETB, 1); */
		break;
	case COREX_SET_C:
		YPP_SET_F(base, YUVPP_R_COREX_UPDATE_TYPE_0_SETC, YUVPP_F_COREX_UPDATE_TYPE_0_SETC, 1);
		/* YPP_SET_F(base, YUVPP_R_COREX_UPDATE_TYPE_1_SETC, YUVPP_F_COREX_UPDATE_TYPE_1_SETC, 1); */
		break;
	case COREX_SET_D:
		YPP_SET_F(base, YUVPP_R_COREX_UPDATE_TYPE_0_SETD, YUVPP_F_COREX_UPDATE_TYPE_0_SETD, 1);
		/* YPP_SET_F(base, YUVPP_R_COREX_UPDATE_TYPE_1_SETD, YUVPP_F_COREX_UPDATE_TYPE_1_SETD, 1); */
		break;
	case COREX_DIRECT:
		YPP_SET_F(base, YUVPP_R_COREX_UPDATE_TYPE_0_SETA, YUVPP_F_COREX_UPDATE_TYPE_0_SETA, 0);
		YPP_SET_F(base, YUVPP_R_COREX_UPDATE_TYPE_0_SETB, YUVPP_F_COREX_UPDATE_TYPE_0_SETB, 0);
		YPP_SET_F(base, YUVPP_R_COREX_UPDATE_TYPE_0_SETC, YUVPP_F_COREX_UPDATE_TYPE_0_SETC, 0);
		YPP_SET_F(base, YUVPP_R_COREX_UPDATE_TYPE_0_SETD, YUVPP_F_COREX_UPDATE_TYPE_0_SETD, 0);
		break;
	default:
		err_hw("[YUVPP] invalid corex set_id");
		ret = -EINVAL;
		break;
	}

	return ret;
}

void ypp_hw_s_cmdq(void __iomem *base, u32 set_id)
{
	u32 val = 0;
	YPP_SET_F(base, YUVPP_R_CQ_FRM_START_TYPE, YUVPP_F_CQ_FRM_START_TYPE, 0);
	if (set_id >= COREX_SET_A || set_id <= COREX_SET_D)
		val = (set_id & 0x3) + (1 << 4) + (1 << 8);
	YPP_SET_F(base, YUVPP_R_MS_ADD_TO_QUEUE, YUVPP_F_MS_ADD_TO_QUEUE, val);
}

void ypp_hw_s_corex_init(void __iomem *base, bool enable)
{
	/*
	 * Check COREX idleness
	 */
	if (!enable) {
		_ypp_hw_wait_corex_idle(base);

		YPP_SET_F(base, YUVPP_R_COREX_ENABLE, YUVPP_F_COREX_ENABLE, 0);

		info_hw("[YUVPP] %s disable done\n", __func__);

		return;
	}

	/*
	 * Set Fixed Value
	 */
	YPP_SET_F(base, YUVPP_R_COREX_ENABLE, YUVPP_F_COREX_ENABLE, 1);
	YPP_SET_F(base, YUVPP_R_COREX_RESET, YUVPP_F_COREX_RESET, 1);
	YPP_SET_F(base, YUVPP_R_COREX_FAST_MODE, YUVPP_F_COREX_FAST_MODE, 1);

	YPP_SET_R(base, YUVPP_R_COREX_MS_COPY_FROM_IP_MULTI, 0xF);

	YPP_SET_R(base, YUVPP_R_COREX_COPY_FROM_IP_0, 1);
	YPP_SET_R(base, YUVPP_R_COREX_COPY_FROM_IP_1, 1);
	/*
	 * Type selection
	 * Only type0 will be used.
	 */
	/*
	type_write_cnt = ((yuvpp_regs[YUVPP_REG_CNT-1].sfr_offset/ 4) + 1) / 32;
	for (set_id = COREX_SET_A; set_id < COREX_SET_D; set_id++) {
		YPP_SET_F(base, YUVPP_R_COREX_MS_TYPE_WRITE_ID, YUVPP_F_COREX_MS_TYPE_WRITE_ID, set_id);
		YPP_SET_F(base, YUVPP_R_COREX_TYPE_WRITE_TRIGGER, YUVPP_F_COREX_TYPE_WRITE_TRIGGER, 1);
		for (i = 0; i < type_write_cnt; i++)
			YPP_SET_F(base, YUVPP_R_COREX_TYPE_WRITE, YUVPP_F_COREX_TYPE_WRITE, 0);
	}
	*/

	YPP_SET_F(base, YUVPP_R_COREX_UPDATE_TYPE_0_SETA, YUVPP_F_COREX_UPDATE_TYPE_0_SETA, 0);
	YPP_SET_F(base, YUVPP_R_COREX_UPDATE_TYPE_0_SETB, YUVPP_F_COREX_UPDATE_TYPE_0_SETB, 0);
	YPP_SET_F(base, YUVPP_R_COREX_UPDATE_TYPE_0_SETC, YUVPP_F_COREX_UPDATE_TYPE_0_SETC, 0);
	YPP_SET_F(base, YUVPP_R_COREX_UPDATE_TYPE_0_SETD, YUVPP_F_COREX_UPDATE_TYPE_0_SETD, 0);

	YPP_SET_F(base, YUVPP_R_COREX_UPDATE_TYPE_1_SETA, YUVPP_F_COREX_UPDATE_TYPE_1_SETA, 0);
	YPP_SET_F(base, YUVPP_R_COREX_UPDATE_TYPE_1_SETB, YUVPP_F_COREX_UPDATE_TYPE_1_SETB, 0);
	YPP_SET_F(base, YUVPP_R_COREX_UPDATE_TYPE_1_SETC, YUVPP_F_COREX_UPDATE_TYPE_1_SETC, 0);
	YPP_SET_F(base, YUVPP_R_COREX_UPDATE_TYPE_1_SETD, YUVPP_F_COREX_UPDATE_TYPE_1_SETD, 0);
	/*
	 * Check COREX idleness, again.
	 */
	_ypp_hw_wait_corex_idle(base);

	info_hw("[YUVPP] %s done\n", __func__);
}

void _ypp_hw_wait_corex_idle(void __iomem *base)
{
	u32 busy;
	u32 try_cnt = 0;

	do {
		udelay(1);

		try_cnt++;
		if (try_cnt >= YUVPP_TRY_COUNT) {
			err_hw("[YUVPP] fail to wait corex idle");
			break;
		}

		busy = YPP_GET_F(base, YUVPP_R_COREX_STATUS_0, YUVPP_F_COREX_BUSY_0);
		dbg_hw(1, "[YUVPP] %s busy(%d)\n", __func__, busy);

	} while (busy);
}

unsigned int ypp_hw_g_int_state(void __iomem *base, bool clear, u32 num_buffers, u32 *irq_state)
{
	u32 src, src_all, src_fro, src_err;

	/*
	 * src_all: per-frame based ypp IRQ status
	 * src_fro: FRO based ypp IRQ status
	 *
	 * final normal status: src_fro (start, line, end)
	 * final error status(src_err): src_all & ERR_MASK
	 */
	src_all = YPP_GET_R(base, YUVPP_R_CONTINT_INT);
	src_fro = YPP_GET_R(base, YUVPP_R_FRO_INT);

	if (clear) {
		YPP_SET_R(base, YUVPP_R_CONTINT_INT_CLEAR, src_all);
		YPP_SET_R(base, YUVPP_R_FRO_INT_CLEAR, src_fro);
	}

	src_err = src_all & INT_ERR_MASK;

	*irq_state = src_all;

	src = (num_buffers > 1) ? src_fro : src_all;


	return src | src_err;
}

unsigned int ypp_hw_g_int_mask(void __iomem *base)
{
	return YPP_GET_R(base, YUVPP_R_CONTINT_INT_ENABLE);
}

void ypp_hw_s_block_bypass(void __iomem *base, u32 set_id)
{
	YPP_SET_F(base + GET_COREX_OFFSET(set_id), YUVPP_R_YUVNR_IN_CROP_CTRL, YUVPP_F_YUVNR_IN_CROP_BYPASS, 1);
	YPP_SET_F(base + GET_COREX_OFFSET(set_id), YUVPP_R_YUVNR_OUT_CROP_CTRL, YUVPP_F_YUVNR_OUT_CROP_BYPASS, 1);
	YPP_SET_R(base + GET_COREX_OFFSET(set_id), YUVPP_R_YUVNR_BYPASS, 1);
	YPP_SET_R(base + GET_COREX_OFFSET(set_id), YUVPP_R_YUV422TORGB_BYPASS, 1);
	YPP_SET_R(base + GET_COREX_OFFSET(set_id), YUVPP_R_RGBTOYUV422_BYPASS, 1);
	YPP_SET_R(base + GET_COREX_OFFSET(set_id), YUVPP_R_CLAHE_BYPASS, 1);
	YPP_SET_R(base + GET_COREX_OFFSET(set_id), YUVPP_R_PRC_BYPASS, 1);
	YPP_SET_R(base + GET_COREX_OFFSET(set_id), YUVPP_R_SKIND_BYPASS, 1);
	YPP_SET_R(base + GET_COREX_OFFSET(set_id), YUVPP_R_NOISE_GEN_BYPASS, 1);
	YPP_SET_R(base + GET_COREX_OFFSET(set_id), YUVPP_R_NADD_BYPASS, 1);
	YPP_SET_R(base + GET_COREX_OFFSET(set_id), YUVPP_R_OETF_GAMMA_BYPASS, 1);
}

int ypp_hw_s_yuvnr_incrop(void __iomem *base, u32 set_id,
	bool strip_enable, u32 region_id, u32 total_count, u32 dma_width)
{
	u32 val;
	u32 in_crop_start_x, in_crop_size_x;
	u32 in_crop_pixel_num;

	val = 0;
	val = YPP_SET_V(val, YUVPP_F_YUVNR_IN_CROP_BYPASS, 1);

	if (strip_enable) {
		in_crop_pixel_num = STRIPE_MARGIN_WIDTH - (MCSC_MARGIN_WIDTH + RGB2YUV422_MARGIN_WIDTH + YUVNR_MARGIN_WIDTH);
		in_crop_start_x = 0;
		/*
		if (!region_id)
			in_crop_start_x = 0;
		else
			in_crop_start_x = in_crop_pixel_num;
		*/
		val = YPP_SET_V(val, YUVPP_F_YUVNR_IN_CROP_START_X, in_crop_start_x);
		in_crop_size_x = dma_width;
		/*
		if (!region_id)
			in_crop_size_x = dma_width - in_crop_pixel_num;
		else if (region_id < total_count - 1)
			in_crop_size_x = dma_width - in_crop_pixel_num * 2;
		else
			in_crop_size_x = dma_width - in_crop_pixel_num;
		*/
		val = YPP_SET_V(val, YUVPP_F_YUVNR_IN_CROP_SIZE_X, in_crop_size_x);
	}
	YPP_SET_R(base + GET_COREX_OFFSET(set_id), YUVPP_R_YUVNR_IN_CROP_CTRL, val);

	return 0;
}

int ypp_hw_s_yuvnr_outcrop(void __iomem *base, u32 set_id, bool strip_enable, u32 region_id, u32 total_count)
{
	u32 val;
	u32 out_crop_start_x;
	u32 out_crop_size_x;
	u32 yuvnr_x_full_size;

	val = 0;
	val = YPP_SET_V(val, YUVPP_F_YUVNR_OUT_CROP_BYPASS, !strip_enable);

	if (strip_enable) {
		yuvnr_x_full_size = YPP_GET_F(base + GET_COREX_OFFSET(set_id),
			YUVPP_R_YUVNR_FULL_SIZE, YUVPP_F_YUVNR_X_FULL_SIZE);

		if (!region_id)
			out_crop_start_x = 0;
		else
			out_crop_start_x = (STRIPE_MARGIN_WIDTH - STRIPE_MCSC_MARGIN_WIDTH);

		val = YPP_SET_V(val, YUVPP_F_YUVNR_OUT_CROP_START_X, out_crop_start_x);

		if (!region_id)
			out_crop_size_x = yuvnr_x_full_size - (STRIPE_MARGIN_WIDTH - STRIPE_MCSC_MARGIN_WIDTH);
		else if (region_id < total_count - 1)
			out_crop_size_x = yuvnr_x_full_size - (STRIPE_MARGIN_WIDTH - STRIPE_MCSC_MARGIN_WIDTH) * 2;
		else
			out_crop_size_x = yuvnr_x_full_size - (STRIPE_MARGIN_WIDTH - STRIPE_MCSC_MARGIN_WIDTH);

		val = YPP_SET_V(val, YUVPP_F_YUVNR_OUT_CROP_SIZE_X, out_crop_size_x);
	}
	YPP_SET_R(base + GET_COREX_OFFSET(set_id), YUVPP_R_YUVNR_OUT_CROP_CTRL, val);

	return 0;
}

int ypp_hw_s_yuvnr_size(void __iomem *base, u32 set_id, u32 yuvpp_strip_start_pos, u32 frame_width,
	u32 dma_width, u32 dma_height, u32 crop_x, u32 crop_y, u32 strip_enable,
	u32 sensor_full_width, u32 sensor_full_height,
	u32 sensor_binning_x, u32 sensor_binning_y, u32 sensor_crop_x, u32 sensor_crop_y,
	u32 taa_crop_x, u32 taa_crop_y, u32 taa_crop_width, u32 taa_crop_height)
{
	u32 val;
	u32 yuvnr_radial_gain_en;
	u32 in_crop_start_pos, in_crop_size_x;
	u32 yuvnr_in_y_l2_width, yuvnr_in_y_l2_height;
	u32 yuvnr_in_uv_l2_width, yuvnr_in_uv_l2_height;
	u32 yuvnr_strip_input_width, yuvnr_strip_start_pos = 0, yuvnr_strip_l2_start_pos = 0;
	u32 yuvnr_x_full_size, yuvnr_y_full_size;
	u32 yuvnr_crop_x, yuvnr_crop_y;
	u32 yuvnr_l2_crop_x, yuvnr_l2_crop_y;
	u32 total_binning_x, total_binning_y;
	u32 x_image_size, y_image_size;
	u32 bin_x, bin_y;

	yuvnr_radial_gain_en = YPP_GET_F(base + GET_COREX_OFFSET(set_id),
		YUVPP_R_YUVNR_RADIAL_CFG, YUVPP_F_YUVNR_RADIAL_GAIN_EN);
	yuvnr_strip_input_width = dma_width;

	total_binning_x = sensor_binning_x * taa_crop_width / frame_width * 4;
	total_binning_y = sensor_binning_y * taa_crop_height / dma_height * 4;

	x_image_size = sensor_full_width / 4;
	y_image_size = sensor_full_height / 4;
	bin_x = total_binning_x * 1024 / 4 / 1000;
	bin_y = total_binning_y * 1024 / 4 / 1000;

	if (strip_enable) {
		in_crop_size_x = YPP_GET_F(base + GET_COREX_OFFSET(set_id),
			YUVPP_R_YUVNR_IN_CROP_CTRL, YUVPP_F_YUVNR_IN_CROP_SIZE_X);
		in_crop_start_pos = YPP_GET_F(base + GET_COREX_OFFSET(set_id),
			YUVPP_R_YUVNR_IN_CROP_CTRL, YUVPP_F_YUVNR_IN_CROP_START_X);
		yuvnr_strip_input_width = in_crop_size_x;
	}

	yuvnr_in_y_l2_width = ((yuvnr_strip_input_width + 2) >> 2) + 1;
	yuvnr_in_y_l2_height = ((dma_height + 2) >> 2) + 1;
	if (dma_width % 8 == 0)
		yuvnr_in_uv_l2_width = (((yuvnr_strip_input_width + 4) >> 3) + 1) << 1 ;
	else
		yuvnr_in_uv_l2_width = ((yuvnr_strip_input_width + 4) >> 3) << 1 ;

	if (dma_height % 8 == 0 || dma_height % 8 == 6)
		yuvnr_in_uv_l2_height = ((dma_height + 7) >> 3) + 1 ;
	else
		yuvnr_in_uv_l2_height = ((dma_height + 7) >> 3);

	if (yuvnr_radial_gain_en) {
		val = 0;
		val = YPP_SET_V(val, YUVPP_F_YUVNR_X_IMAGE_SIZE, x_image_size);
		val = YPP_SET_V(val, YUVPP_F_YUVNR_Y_IMAGE_SIZE, y_image_size);
		YPP_SET_R(base + GET_COREX_OFFSET(set_id), YUVPP_R_YUVNR_IMAGE_SIZE, val);
	}

	yuvnr_x_full_size = strip_enable ? yuvnr_strip_input_width : frame_width;
	yuvnr_y_full_size = dma_height;

	val = 0;
	val = YPP_SET_V(val, YUVPP_F_YUVNR_X_FULL_SIZE, yuvnr_x_full_size);
	val = YPP_SET_V(val, YUVPP_F_YUVNR_Y_FULL_SIZE, yuvnr_y_full_size);
	YPP_SET_R(base + GET_COREX_OFFSET(set_id), YUVPP_R_YUVNR_FULL_SIZE, val);

	val = 0;
	val = YPP_SET_V(val, YUVPP_F_YUVNR_X_DS_IMAGE_SIZE_Y, yuvnr_in_y_l2_width);
	val = YPP_SET_V(val, YUVPP_F_YUVNR_Y_DS_IMAGE_SIZE_Y, yuvnr_in_y_l2_height);
	YPP_SET_R(base + GET_COREX_OFFSET(set_id), YUVPP_R_YUVNR_DS_IMAGE_SIZE_Y, val);

	val = 0;
	val = YPP_SET_V(val, YUVPP_F_YUVNR_X_DS_IMAGE_SIZE_UV, yuvnr_in_uv_l2_width);
	val = YPP_SET_V(val, YUVPP_F_YUVNR_Y_DS_IMAGE_SIZE_UV, yuvnr_in_uv_l2_height);
	YPP_SET_R(base + GET_COREX_OFFSET(set_id), YUVPP_R_YUVNR_DS_IMAGE_SIZE_UV, val);

	val = 0;
	val = YPP_SET_V(val, YUVPP_F_YUVNR_XBIN, bin_x);
	val = YPP_SET_V(val, YUVPP_F_YUVNR_YBIN, bin_y);
	YPP_SET_R(base + GET_COREX_OFFSET(set_id), YUVPP_R_YUVNR_BIN, val);

	if (strip_enable) {
		yuvnr_strip_start_pos = yuvpp_strip_start_pos + in_crop_start_pos;
		yuvnr_strip_l2_start_pos = (yuvnr_strip_start_pos + 2) >> 2;
		yuvnr_l2_crop_x = yuvnr_strip_l2_start_pos;
		yuvnr_l2_crop_y = 0;
	} else {
		yuvnr_l2_crop_x = yuvnr_radial_gain_en ? crop_x : 0;
		yuvnr_l2_crop_y = yuvnr_radial_gain_en ? crop_y : 0;
	}

	yuvnr_crop_x = ((sensor_crop_x + taa_crop_x) * sensor_binning_x + yuvnr_l2_crop_x * total_binning_x) / 1000 / 4;
	yuvnr_crop_y = ((sensor_crop_y + taa_crop_y) * sensor_binning_y + yuvnr_l2_crop_y * total_binning_y) / 1000 / 4;

	val = 0;
	val = YPP_SET_V(val, YUVPP_F_YUVNR_CROPX, yuvnr_crop_x);
	val = YPP_SET_V(val, YUVPP_F_YUVNR_CROPY, yuvnr_crop_y);
	YPP_SET_R(base + GET_COREX_OFFSET(set_id), YUVPP_R_YUVNR_CROP, val);

	return 0;
}

int ypp_hw_s_clahe_size(void __iomem *base, u32 set_id,
	u32 frame_width, u32 dma_height, u32 yuvpp_strip_start_pos, u32 strip_enable)
{
	u32 val;
	u32 clahe_strip_start_pos;
	u32 in_crop_start_pos, out_crop_start_pos;
	u32 out_crop_size_x;

	if (strip_enable) {
		in_crop_start_pos = YPP_GET_F(base + GET_COREX_OFFSET(set_id),
			YUVPP_R_YUVNR_IN_CROP_CTRL, YUVPP_F_YUVNR_IN_CROP_START_X);
		out_crop_start_pos = YPP_GET_F(base + GET_COREX_OFFSET(set_id),
			YUVPP_R_YUVNR_OUT_CROP_CTRL, YUVPP_F_YUVNR_OUT_CROP_START_X);
		out_crop_size_x = YPP_GET_F(base + GET_COREX_OFFSET(set_id),
			YUVPP_R_YUVNR_OUT_CROP_CTRL, YUVPP_F_YUVNR_OUT_CROP_SIZE_X);

		clahe_strip_start_pos = yuvpp_strip_start_pos
			+ in_crop_start_pos + out_crop_start_pos;

		YPP_SET_F(base + GET_COREX_OFFSET(set_id), YUVPP_R_CLAHE_STRIP_START_POSITION,
			YUVPP_F_CLAHE_STRIP_START_POSITION, clahe_strip_start_pos);
		YPP_SET_F(base + GET_COREX_OFFSET(set_id), YUVPP_R_CLAHE_STRIP_WIDTH,
			YUVPP_F_CLAHE_STRIP_INPUT_WIDTH, out_crop_size_x);
	} else {
		YPP_SET_F(base + GET_COREX_OFFSET(set_id), YUVPP_R_CLAHE_STRIP_START_POSITION,
			YUVPP_F_CLAHE_STRIP_START_POSITION, 0);
		YPP_SET_F(base + GET_COREX_OFFSET(set_id), YUVPP_R_CLAHE_STRIP_WIDTH,
			YUVPP_F_CLAHE_STRIP_INPUT_WIDTH, 0);
	}

	val = 0;
	val = YPP_SET_V(val, YUVPP_F_CLAHE_FRAME_WIDTH, frame_width);
	val = YPP_SET_V(val, YUVPP_F_CLAHE_FRAME_HEIGHT, dma_height);

	YPP_SET_R(base + GET_COREX_OFFSET(set_id), YUVPP_R_CLAHE_FRAME_SIZE, val);
	return 0;
}

void ypp_hw_s_nadd_size(void __iomem *base, u32 set_id, u32 yuvpp_strip_start_pos, u32 strip_enable)
{
	u32 nadd_strip_start_pos;
	u32 in_crop_start_pos, out_crop_start_pos;
	u32 out_crop_size_x;

	if (strip_enable) {
		in_crop_start_pos = YPP_GET_F(base + GET_COREX_OFFSET(set_id),
			YUVPP_R_YUVNR_IN_CROP_CTRL, YUVPP_F_YUVNR_IN_CROP_START_X);
		out_crop_start_pos = YPP_GET_F(base + GET_COREX_OFFSET(set_id),
			YUVPP_R_YUVNR_OUT_CROP_CTRL, YUVPP_F_YUVNR_OUT_CROP_START_X);
		out_crop_size_x = YPP_GET_F(base + GET_COREX_OFFSET(set_id),
			YUVPP_R_YUVNR_OUT_CROP_CTRL, YUVPP_F_YUVNR_OUT_CROP_SIZE_X);

		nadd_strip_start_pos = yuvpp_strip_start_pos
			+ in_crop_start_pos + out_crop_start_pos;

		YPP_SET_F(base + GET_COREX_OFFSET(set_id), YUVPP_R_NADD_STRIP_PARAMS,
			YUVPP_F_NADD_STRIP_START_POSITION, nadd_strip_start_pos);
	} else {
		YPP_SET_F(base + GET_COREX_OFFSET(set_id), YUVPP_R_NADD_STRIP_PARAMS,
			YUVPP_F_NADD_STRIP_START_POSITION, 0);
	}
}

void ypp_hw_s_coutfifo_size(void __iomem *base, u32 set_id, u32 dma_width, u32 dma_height, bool strip_enable)
{
	u32 val;
	u32 coutfifo_width = dma_width;

	if (strip_enable)
		coutfifo_width = YPP_GET_F(base + GET_COREX_OFFSET(set_id),
			YUVPP_R_YUVNR_OUT_CROP_CTRL, YUVPP_F_YUVNR_OUT_CROP_SIZE_X);

	val = 0;
	val = YPP_SET_V(val, YUVPP_F_COUTFIFO_OUTPUT_IMAGE_WIDTH, coutfifo_width);
	val = YPP_SET_V(val, YUVPP_F_COUTFIFO_OUTPUT_IMAGE_HEIGHT, dma_height);
	YPP_SET_R(base + GET_COREX_OFFSET(set_id), YUVPP_R_COUTFIFO_OUTPUT_IMAGE_DIMENSIONS, val);
}
