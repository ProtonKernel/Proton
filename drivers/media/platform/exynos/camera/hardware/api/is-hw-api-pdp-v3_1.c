/*
 * Samsung Exynos SoC series Pablo driver
 *
 * Copyright (c) 2020 Samsung Electronics Co., Ltd
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#include <linux/kernel.h>
#include <linux/delay.h>
#include <dt-bindings/camera/exynos_is_dt.h>

#include "is-hw-api-pdp-v1.h"
#include "sfr/is-sfr-pdp-v3_1.h"
#include "is-device-sensor.h"
#include "is-device-sensor-peri.h"
#include "is-debug.h"

static int ver_idx = 0;
static int pdp_tnum = 3;

#define PDP_VERSION_V3_0		(0x3000000) /* v3.0 */
#define PDP_VERSION_V3_1		(0x3010000) /* v3.1 */
#define PDP_VERSION_V3_3		(0x3030000) /* v3.3 */

#define PDP_SET_F(base, R, F, val) \
	is_hw_set_field(base, &pdp_regs_corex[R], &pdp_fields[F], val)
#define PDP_SET_F_DIRECT(base, R, F, val) \
	is_hw_set_field(base, &pdp_regs[R], &pdp_fields[F], val)
#define PDP_SET_R(base, R, val) \
	is_hw_set_reg(base, &pdp_regs_corex[R], val)
#define PDP_SET_R_DIRECT(base, R, val) \
	is_hw_set_reg(base, &pdp_regs[R], val)
#define PDP_SET_V(reg_val, F, val) \
	is_hw_set_field_value(reg_val, &pdp_fields[F], val)

#define PDP_GET_F(base, R, F) \
	is_hw_get_field(base, &pdp_regs[R], &pdp_fields[F])
#define PDP_GET_R(base, R) \
	is_hw_get_reg(base, &pdp_regs[R])
#define PDP_GET_R_COREX(base, R) \
	is_hw_get_reg(base, &pdp_regs_corex[R])
#define PDP_GET_V(reg_val, F) \
	is_hw_get_field_value(reg_val, &pdp_fields[F])

#define PDP_RDMA_MO_TICK	10

#define PDP_RDMA_MO_DEFAULT	3
#define PDP_RDMA_MO_FPS60	5
#define PDP_RDMA_MO_FPS240	10
#define PDP_RDMA_MO_FPS480	20

#define USE_PDP_VOTF_LEGACY_RING	0

#define REORDER_BPC_SRAM_MAX		2879

#define MIN_CONFIG_LOCK_TIME		5000 /* us */
#define OSC_CLOCK			26 * MHZ
#define MAX_CLOCK			600 * MHZ

#define IS_DUALFPS_MODE(ex_mode) \
		((ex_mode == EX_DUALFPS_480) ||	\
		 (ex_mode == EX_DUALFPS_960))
#define NEED_TO_ADJUST_LINE_GAP(ex_mode, cl_margin)	\
		((cl_margin < MIN_CONFIG_LOCK_TIME) &&	\
		((cl_margin > 0) ||	\
		IS_DUALFPS_MODE(ex_mode)))

/*
 * Context: O
 * CR type: No Corex
 */
static __always_inline void _pdp_hw_wait_corex_idle(void __iomem *base)
{
	u32 busy;
	u32 try_cnt = 0;

	busy = PDP_GET_F(base, PDP_R_COREX_STATUS_0, PDP_F_COREX_BUSY_0);
	while (busy) {
		udelay(1);

		try_cnt++;
		if (try_cnt >= PDP_TRY_COUNT) {
			err_hw("[PDP] fail to wait corex idle");
			break;
		}

		busy = PDP_GET_F(base, PDP_R_COREX_STATUS_0, PDP_F_COREX_BUSY_0);
		dbg_hw(2, "[PDP] %s busy(%d)\n", __func__, busy);
	}
}

static void _pdp_hw_s_corex_init(void __iomem *base, bool enable)
{
	int i;

	/*
	 * Check COREX idleness
	 */
	if (!enable) {
		PDP_SET_F(base, PDP_R_COREX_UPDATE_MODE_0, PDP_F_COREX_UPDATE_MODE_0, SW_TRIGGER);
		PDP_SET_F(base, PDP_R_COREX_UPDATE_MODE_1, PDP_F_COREX_UPDATE_MODE_1, SW_TRIGGER);

		_pdp_hw_wait_corex_idle(base);

		PDP_SET_F(base, PDP_R_COREX_ENABLE, PDP_F_COREX_ENABLE, 0);

		if (!in_irq())
			info_hw("[PDP] %s disable done\n", __func__);

		return;
	}

	/*
	 * Set Fixed Value
	 */
	PDP_SET_F(base, PDP_R_COREX_ENABLE, PDP_F_COREX_ENABLE, 1);
	PDP_SET_F(base, PDP_R_COREX_RESET, PDP_F_COREX_RESET, 1);

	/*
	 * Type selection
	 * Only type0 will be used.
	 */
	PDP_SET_F(base, PDP_R_COREX_TYPE_WRITE_TRIGGER, PDP_F_COREX_TYPE_WRITE_TRIGGER, 1);

	/*
	 * EVT0 only
	 * 0x2300 ~ 0x2380 set corex type to "1" to ignore corex type0 setting.
	 * write COREX_TYPE_WRITE to 1 means set type size in 0x80(128) range.
	 */
	for (i = 0; i < DIV_ROUND_UP(pdp_regs[PDP_REG_CNT - 1].sfr_offset, 128); i++) {
		if (i == 0x2300 / 128 && ver_idx == 0)
			PDP_SET_F(base, PDP_R_COREX_TYPE_WRITE, PDP_F_COREX_TYPE_WRITE, 0xffffffff);
		else
			PDP_SET_F(base, PDP_R_COREX_TYPE_WRITE, PDP_F_COREX_TYPE_WRITE, 0);
	}
	/*
	 * config type0
	 * @PDP_R_COREX_UPDATE_TYPE_0: 0: ignore, 1: copy from SRAM, 2: swap
	 * @PDP_R_COREX_UPDATE_MODE_0: 0: HW trigger, 1: SW trigger
	 */
	PDP_SET_F(base, PDP_R_COREX_UPDATE_TYPE_0, PDP_F_COREX_UPDATE_TYPE_0, COREX_COPY);
	PDP_SET_F(base, PDP_R_COREX_UPDATE_TYPE_1, PDP_F_COREX_UPDATE_TYPE_1, COREX_IGNORE);
	PDP_SET_F(base, PDP_R_COREX_UPDATE_MODE_0, PDP_F_COREX_UPDATE_MODE_0, SW_TRIGGER);
	PDP_SET_F(base, PDP_R_COREX_UPDATE_MODE_1, PDP_F_COREX_UPDATE_MODE_1, SW_TRIGGER);

	/* intialize type0 */
	PDP_SET_F(base, PDP_R_COREX_COPY_FROM_IP_0, PDP_F_COREX_COPY_FROM_IP_0, 1);

	/*
	 * Check COREX idleness, again.
	 */
	_pdp_hw_wait_corex_idle(base);

	info_hw("[PDP] %s done\n", __func__);
}

/*
 * Context: O
 * CR type: No Corex
 */
static void _pdp_hw_s_corex_start(void __iomem *base, bool enable)
{
	if (!enable)
		return;
	/*
	 * Set Fixed Value
	 *
	 * Type0 only:
	 *
	 * @PDP_R_COREX_START_0 - 1: puls generation
	 * @PDP_R_COREX_UPDATE_MODE_0 - 0: HW trigger, 1: SW tirgger
	 *
	 * SW trigger should be needed before stream on
	 * becuse there is no HW trigger before stream on.
	 */
	PDP_SET_F(base, PDP_R_COREX_UPDATE_MODE_0, PDP_F_COREX_UPDATE_MODE_0, SW_TRIGGER);
	PDP_SET_F(base, PDP_R_COREX_START_0, PDP_F_COREX_START_0, 1);

	_pdp_hw_wait_corex_idle(base);

	PDP_SET_F(base, PDP_R_COREX_UPDATE_MODE_0, PDP_F_COREX_UPDATE_MODE_0, HW_TRIGGER);

	info_hw("[PDP] %s done\n", __func__);
}

/*
 * Context: O
 * CR type: Corex + No Corex
 */
static void _pdp_hw_s_cout_fifo(void __iomem *base, u32 path)
{
	u32 val;

	/*
	 * Keep Reset Value
	 *
	 * PDP_R_CINFIFO_OUTPUT_T1_INTERVAL ~ PDP_R_CINFIFO_OUTPUT_T7_INTERVAL
	 * PDP_R_CINFIFO_OUTPUT_T2_WAIT_FOR_FS
	 * PDP_R_CINFIFO_OUTPUT_START_STALL
	 * PDP_R_CINFIFO_OUTPUT_STOP_STALL
	 * PDP_R_CINFIFO_OUTPUT_STALL_EN_THR
	 */

	/*
	 * Set Pameter Value
	 *
	 * cinfifo_output_count_at_stall: rdma:0, otf:1
	 */

	/*
	 * If PDP_R_CINFIFO_OUTPUT_COUNT_AT_STALL == 0, line gap is inclued when stall signal occurr from 3AA.
	 * So, in case of fast sensor, it is should be set as "0".
	 */
	val = (path == OTF) ? 0 : 0;
	PDP_SET_R(base, PDP_R_CINFIFO_OUTPUT_COUNT_AT_STALL, val);

	/*
	 * Set Fixed Value
	 */
	PDP_SET_R(base, PDP_R_CINFIFO_OUTPUT_T3_INTERVAL, HBLANK_CYCLE);

	val = 0;
	val = PDP_SET_V(val, PDP_F_CINFIFO_OUTPUT_COL_ERROR_EN, 1);
	val = PDP_SET_V(val, PDP_F_CINFIFO_OUTPUT_LINE_ERROR_EN, 1);
	val = PDP_SET_V(val, PDP_F_CINFIFO_OUTPUT_TOTAL_SIZE_ERROR_EN, 1);
	PDP_SET_R(base, PDP_R_CINFIFO_OUTPUT_ERROR_ENABLE, val);
	PDP_SET_F(base, PDP_R_CINFIFO_OUTPUT_CINFIFO_ENABLE, PDP_F_CINFIFO_OUTPUT_ENABLE, 1);
}

/*
 * Context: O
 * CR type: Corex
 */
static void _pdp_hw_s_lic_context(void __iomem *base,
	u32 pixelsize, u32 sensor_type)
{
	u32 mode;
	u32 val;
	u32 ppc_dual;
	u32 bit_mode;

	/*
	 * Keep Reset Value
	 *
	 * @PDP_F_LIC_OUTPUT_DISABLE_MASK
	 */

	/*
	 * Set Parameter Value
	 *
	 * sensor config
	 */
	ppc_dual = 0; /* TODO: get CSIS 2ppc mode */

	if (pixelsize == OTF_INPUT_BIT_WIDTH_14BIT || pixelsize == DMA_INOUT_BIT_WIDTH_14BIT)
		bit_mode = 2;
	else if (pixelsize == OTF_INPUT_BIT_WIDTH_12BIT || pixelsize == DMA_INOUT_BIT_WIDTH_12BIT)
		bit_mode = 1;
	else
		bit_mode = 0;

	val = 0;
	val = PDP_SET_V(val, PDP_F_LIC_INPUT_CONTEXT_MODE, sensor_type);
	val = PDP_SET_V(val, PDP_F_LIC_INPUT_2PPC_EN, ppc_dual);
	val = PDP_SET_V(val, PDP_F_LIC_INPUT_BIT_MODE, bit_mode);
	PDP_SET_R(base, PDP_R_LIC_INPUT_CONFIG1, val);

	/*
	 * Set LIC Virtual Line
	 */
	val = 0;
	val = PDP_SET_V(val, PDP_F_LIC_VL_NUM_LINES_IMAGE, PDP_LIC_VL_IMAGE);
	val = PDP_SET_V(val, PDP_F_LIC_VL_NUM_LINES_PDPXL, PDP_LIC_VL_PDPXL);
	PDP_SET_R(base, PDP_R_LIC_VIRTUAL_LINE_CONFIG_0, val); /* PDP_R_LIC_VIRTUAL_LINE_CONFIG_COMMON in v3.0 */

	mode = PDP_GET_R(base, PDP_R_LIC_OPERATION_MODE);
	val = 0;
	val = PDP_SET_V(val, PDP_F_LIC_VL_PREEMPTION_EN, 1);
	val = PDP_SET_V(val, PDP_F_LIC_VL_PREEMPTION_THRESHOLD,
		mode == PDP_LIC_MODE_DYNAMIC ? PDP_LIC_VL_PREEMPTION_THRES_DYNAMIC : PDP_LIC_VL_PREEMPTION_THRES_STATIC);
	PDP_SET_R(base, PDP_R_LIC_VIRTUAL_LINE_CONFIG_1, val); /* PDP_R_LIC_VIRTUAL_LINE_CONFIG_DYNAMIC in v3.0 */

	/*
	 * Set Fixed Value
	 */
	val = 0;
	val = PDP_SET_V(val, PDP_F_LIC_OUTPUT_HBLANK_CYCLE, HBLANK_CYCLE);
	val = PDP_SET_V(val, PDP_F_LIC_OUTPUT_HOLD_AT_COREX_BUSY, 1);
	PDP_SET_R(base, PDP_R_LIC_OUTPUT_CONFIG, val);

	val = 0;
	val = PDP_SET_V(val, PDP_F_LIC_ROL_ENABLE, 1);
	val = PDP_SET_V(val, PDP_F_LIC_ROL_CONDITION, 0x7);
	PDP_SET_R(base, PDP_R_LIC_DEBUG_CONFIG, val);
}

void pdp_hw_s_lic_bit_mode(void __iomem *base, u32 pixelsize)
{
	u32 bit_mode;

	if (pixelsize == DMA_INOUT_BIT_WIDTH_14BIT)
		bit_mode = 2;
	else if (pixelsize == DMA_INOUT_BIT_WIDTH_12BIT)
		bit_mode = 1;
	else
		bit_mode = 0;

	PDP_SET_F(base, PDP_R_LIC_INPUT_CONFIG1, PDP_F_LIC_INPUT_BIT_MODE, bit_mode);
}

/*
 * Context: X (ch0 only)
 * CR type: Corex
 */
static u32 dyn_cfg;
static u32 sta_cfg;
static void _pdp_hw_s_lic_ch0(void __iomem *base, u32 curr_ch, u32 lic_mode, u32 lic_14bit,
	struct pdp_lic_lut *lut)
{
	u32 val;
	u32 mode;
	u32 size_0, size_1, size_2, size_3;
	u32 sum;
	u32 total_size;

	if (lic_14bit)
		total_size = PDP_LIC_TOTAL_SIZE_14BIT;
	else
		total_size = PDP_LIC_TOTAL_SIZE_10BIT;

	/*
	 * Set Parameter Value
	 *
	 * scenario: use LUT
	 * buffer size: DMA path should have more limitation than OTF path.
	 * Operation mode: 0: Dynamic allocation, 1: Static allocation, 2: Single Buffer Mode
	 */
	if (lut) {
		mode = lut->mode;
		PDP_SET_F(base, PDP_R_LIC_OPERATION_MODE, PDP_F_LIC_OPERATION_MODE, mode);

		if (mode == PDP_LIC_MODE_DYNAMIC) {
			dyn_cfg = 0;
			/* DMA: enable, OTF: disable */
			dyn_cfg = PDP_SET_V(dyn_cfg, PDP_F_LIC_LIMIT_INPUT_LINE_EN_CONTEXT_0, lut->param0);
			dyn_cfg = PDP_SET_V(dyn_cfg, PDP_F_LIC_LIMIT_INPUT_LINE_EN_CONTEXT_1, lut->param1);
			dyn_cfg = PDP_SET_V(dyn_cfg, PDP_F_LIC_LIMIT_INPUT_LINE_EN_CONTEXT_2, lut->param2);
			dyn_cfg = PDP_SET_V(dyn_cfg, PDP_F_LIC_LIMIT_INPUT_LINE_EN_CONTEXT_3, lut->param3);

			dyn_cfg = PDP_SET_V(dyn_cfg, PDP_F_LIC_MAX_INPUT_LINE_CONTEXT_0, 1);
			dyn_cfg = PDP_SET_V(dyn_cfg, PDP_F_LIC_MAX_INPUT_LINE_CONTEXT_1, 1);
			dyn_cfg = PDP_SET_V(dyn_cfg, PDP_F_LIC_MAX_INPUT_LINE_CONTEXT_2, 1);
			dyn_cfg = PDP_SET_V(dyn_cfg, PDP_F_LIC_MAX_INPUT_LINE_CONTEXT_3, 1);
			PDP_SET_R(base, PDP_R_LIC_DYNAMIC_ALLOC_CONFIG, dyn_cfg);
		} else if (mode == PDP_LIC_MODE_STATIC) {
			sta_cfg = 0;
			sta_cfg = PDP_SET_V(sta_cfg, PDP_F_LIC_PREV_CENTRIC_SCHEDULE_EN, 0);
			sta_cfg = PDP_SET_V(sta_cfg, PDP_F_LIC_PREV_CENTRIC_SCHEDULE_SEL_PREVIEW, 0);
			sta_cfg = PDP_SET_V(sta_cfg, PDP_F_LIC_PREV_CENTRIC_SCHEDULE_NUM_LINE, 0);
			sta_cfg = PDP_SET_V(sta_cfg, PDP_F_LIC_WEIGHT_CONTEXT_0, 1);
			sta_cfg = PDP_SET_V(sta_cfg, PDP_F_LIC_WEIGHT_CONTEXT_1, 1);
			sta_cfg = PDP_SET_V(sta_cfg, PDP_F_LIC_WEIGHT_CONTEXT_2, 1);
			sta_cfg = PDP_SET_V(sta_cfg, PDP_F_LIC_WEIGHT_CONTEXT_3, 1);
			PDP_SET_R(base, PDP_R_LIC_STATIC_ALLOC_CONFIG1, sta_cfg);

			/* min ~ max: 1 ~ 7168 */
			sum = lut->param0 + lut->param1 + lut->param2 + lut->param3;
			if (sum > total_size || sum < 4)
				warn("LIC LUT size is invalid(%d, %d, %d, %d)", lut->param0, lut->param1, lut->param2, lut->param3);

			size_0 = lut->param0;
			size_1 = lut->param1;
			size_2 = lut->param2;
			size_3 = lut->param3;
			PDP_SET_F(base, PDP_R_LIC_STATIC_ALLOC_CONFIG2, PDP_F_LIC_BUFFERSIZE_CONTEXT_0, size_0);
			PDP_SET_F(base, PDP_R_LIC_STATIC_ALLOC_CONFIG2, PDP_F_LIC_BUFFERSIZE_CONTEXT_1, size_1);
			PDP_SET_F(base, PDP_R_LIC_STATIC_ALLOC_CONFIG3, PDP_F_LIC_BUFFERSIZE_CONTEXT_2, size_2);
			PDP_SET_F(base, PDP_R_LIC_STATIC_ALLOC_CONFIG3, PDP_F_LIC_BUFFERSIZE_CONTEXT_3, size_3);
		} else if (mode == PDP_LIC_MODE_SINGLE) {
			PDP_SET_F(base, PDP_R_LIC_OPERATION_MODE, PDP_F_LIC_SEL_SINGLE_INPUT, curr_ch);
		}
	} else {
		PDP_SET_F(base, PDP_R_LIC_OPERATION_MODE, PDP_F_LIC_OPERATION_MODE, lic_mode);

		switch (lic_mode) {
		case PDP_LIC_MODE_DYNAMIC:
		case PDP_LIC_MODE_STATIC:
			/* DYNAMIC mode: default limitation is disabled. */
			dyn_cfg = 0;
			dyn_cfg = PDP_SET_V(dyn_cfg, PDP_F_LIC_LIMIT_INPUT_LINE_EN_CONTEXT_0, 0);
			dyn_cfg = PDP_SET_V(dyn_cfg, PDP_F_LIC_LIMIT_INPUT_LINE_EN_CONTEXT_1, 0);
			dyn_cfg = PDP_SET_V(dyn_cfg, PDP_F_LIC_LIMIT_INPUT_LINE_EN_CONTEXT_2, 0);
			dyn_cfg = PDP_SET_V(dyn_cfg, PDP_F_LIC_LIMIT_INPUT_LINE_EN_CONTEXT_3, 0);

			dyn_cfg = PDP_SET_V(dyn_cfg, PDP_F_LIC_MAX_INPUT_LINE_CONTEXT_0, 1);
			dyn_cfg = PDP_SET_V(dyn_cfg, PDP_F_LIC_MAX_INPUT_LINE_CONTEXT_1, 1);
			dyn_cfg = PDP_SET_V(dyn_cfg, PDP_F_LIC_MAX_INPUT_LINE_CONTEXT_2, 1);
			dyn_cfg = PDP_SET_V(dyn_cfg, PDP_F_LIC_MAX_INPUT_LINE_CONTEXT_3, 1);

			PDP_SET_R(base, PDP_R_LIC_DYNAMIC_ALLOC_CONFIG, dyn_cfg);

			/* STATIC mode - default weight is MAX. */
			sta_cfg = 0;
			sta_cfg = PDP_SET_V(sta_cfg, PDP_F_LIC_PREV_CENTRIC_SCHEDULE_EN, 0);
			sta_cfg = PDP_SET_V(sta_cfg, PDP_F_LIC_PREV_CENTRIC_SCHEDULE_SEL_PREVIEW, 0);
			sta_cfg = PDP_SET_V(sta_cfg, PDP_F_LIC_PREV_CENTRIC_SCHEDULE_NUM_LINE, 0);
			sta_cfg = PDP_SET_V(sta_cfg, PDP_F_LIC_VL_OPERATION_LINE_NUM, 0);
			PDP_SET_R(base, PDP_R_LIC_STATIC_ALLOC_CONFIG1, sta_cfg);
			sta_cfg = 0;
			sta_cfg = PDP_SET_V(sta_cfg, PDP_F_LIC_WEIGHT_CONTEXT_0, PDP_LIC_WEIGHT_MAX);
			sta_cfg = PDP_SET_V(sta_cfg, PDP_F_LIC_WEIGHT_CONTEXT_1, PDP_LIC_WEIGHT_MAX);
			sta_cfg = PDP_SET_V(sta_cfg, PDP_F_LIC_WEIGHT_CONTEXT_2, PDP_LIC_WEIGHT_MAX);
			sta_cfg = PDP_SET_V(sta_cfg, PDP_F_LIC_WEIGHT_CONTEXT_3, PDP_LIC_WEIGHT_MAX);
			PDP_SET_R(base, PDP_R_LIC_STATIC_ALLOC_CONFIG4, sta_cfg);

			/*
			 * min ~ max: 1 ~ 4608
			 * TODO: If use 14bit sensor, need set to 4096
			 */
			if (pdp_tnum == 4)
				size_3 = (total_size) / pdp_tnum;
			else
				size_3 = 0;
			size_1 = (total_size) / pdp_tnum;
			size_2 = (total_size) / pdp_tnum;
			size_0 = total_size - size_1 - size_2 - size_3;
			PDP_SET_F(base, PDP_R_LIC_STATIC_ALLOC_CONFIG2, PDP_F_LIC_BUFFERSIZE_CONTEXT_0, size_0);
			PDP_SET_F(base, PDP_R_LIC_STATIC_ALLOC_CONFIG2, PDP_F_LIC_BUFFERSIZE_CONTEXT_1, size_1);
			PDP_SET_F(base, PDP_R_LIC_STATIC_ALLOC_CONFIG3, PDP_F_LIC_BUFFERSIZE_CONTEXT_2, size_2);
			PDP_SET_F(base, PDP_R_LIC_STATIC_ALLOC_CONFIG3, PDP_F_LIC_BUFFERSIZE_CONTEXT_3, size_3);
			break;
		case PDP_LIC_MODE_SINGLE:
			PDP_SET_F(base, PDP_R_LIC_OPERATION_MODE, PDP_F_LIC_SEL_SINGLE_INPUT, curr_ch);
			break;
		default:
			break;
		}
	}

	/*
	 * Set Fiexed Value
	 *
	 * TODO: The number of sensor mode & format in all context is needed
	 * before first context is streaming.
	 */
	val = 0;
	val = PDP_SET_V(val, PDP_F_LIC_NUM_14BIT_SENSOR, lic_14bit);
	PDP_SET_R(base, PDP_R_LIC_BUFFER_COMMON_CONFIG, val);
}

/*
 * Context: X (ch0 only)
 * CR type: Corex
 */
static void _pdp_hw_s_lic_ch0_priority(void __iomem *base, u32 curr_ch, u32 curr_path,
	u32 dma_only)
{
	u32 mode;
	u32 field_enum;
	u32 bit_mask, bit_start;
	u32 val, read_val;
	u32 try_cnt;
	u32 retry_cnt = 0;
	u32 cfg_0, cfg_1, cfg_2, cfg_3, cfg_4;
	u32 corex_mode, corex_cfg_0, corex_cfg_1, corex_cfg_2, corex_cfg_3, corex_cfg_4;

retry_lic_priority:
	/*
	 * Set Parameter Value
	 *
	 * Path: DMA path should have more limitation than OTF path.
	 */

	mode = PDP_GET_F(base, PDP_R_LIC_OPERATION_MODE, PDP_F_LIC_OPERATION_MODE);
	info_hw("[PDP] LIC mode(%s), ch(%d), Path(%s)\n",
		mode == PDP_LIC_MODE_DYNAMIC ? "DYNAMIC" :
		(mode == PDP_LIC_MODE_STATIC ? "STATIC" :
		(mode == PDP_LIC_MODE_SINGLE ? "SINGLE" : "INVALID")),
		curr_ch,
		curr_path == OTF ? "OTF" : "DMA");

	switch (mode) {
	case PDP_LIC_MODE_DYNAMIC:
	case PDP_LIC_MODE_STATIC:
		/* DYNAMIC mode - DMA: limitation enable(1), OTF: limitation disable(0) */
		if (!dma_only && curr_path == DMA)
			val = 1;
		else
			val = 0;

		field_enum = PDP_F_LIC_LIMIT_INPUT_LINE_EN_CONTEXT_0 + curr_ch;
		bit_mask = (1 << pdp_fields[field_enum].bit_width) - 1;
		bit_start = pdp_fields[field_enum].bit_start;

		dyn_cfg &= ~(bit_mask << bit_start);
		dyn_cfg |= (val << bit_start);

		PDP_SET_R_DIRECT(base, PDP_R_LIC_DYNAMIC_ALLOC_CONFIG, dyn_cfg);
		PDP_SET_R(base, PDP_R_LIC_DYNAMIC_ALLOC_CONFIG, dyn_cfg);

		/* STATIC mode - DMA: low priority(0), OTF: high priority(7) */
		if (curr_path == DMA)
			val = 0;
		else
			val = PDP_LIC_WEIGHT_MAX;

		field_enum = PDP_F_LIC_WEIGHT_CONTEXT_0 + curr_ch;
		bit_mask = (1 << pdp_fields[field_enum].bit_width) - 1;
		bit_start = pdp_fields[field_enum].bit_start;

		sta_cfg &= ~(bit_mask << bit_start);
		sta_cfg |= (val << bit_start);

		PDP_SET_R_DIRECT(base, PDP_R_LIC_STATIC_ALLOC_CONFIG4, sta_cfg);
		PDP_SET_R(base, PDP_R_LIC_STATIC_ALLOC_CONFIG4, sta_cfg);

		/* wait for register update */
		try_cnt = 0;
		read_val = PDP_GET_R(base, PDP_R_LIC_STATIC_ALLOC_CONFIG4);
		while (sta_cfg != read_val) {
			dbg_hw(2, "[PDP] %s: write(%d) != read(%d)\n", __func__, sta_cfg, read_val);
			udelay(5);

			try_cnt++;
			if (try_cnt >= PDP_TRY_COUNT) {
				err_hw("[PDP] fail to wait updating LIC priority (%x != %x)",
					sta_cfg, read_val);
				break;
			}

			read_val = PDP_GET_R(base, PDP_R_LIC_STATIC_ALLOC_CONFIG1);
		}
		break;
	case PDP_LIC_MODE_SINGLE:
		info_hw("[PDP] LIC mode single. Priority is not needed.\n");
		break;
	default:
		info_hw("[PDP] LIC mode is invalid.\n");
		break;
	}

	/* For debug */
	mode = PDP_GET_R(base, PDP_R_LIC_OPERATION_MODE);
	cfg_0 = PDP_GET_R(base, PDP_R_LIC_BUFFER_COMMON_CONFIG);
	cfg_1 = PDP_GET_R(base, PDP_R_LIC_DYNAMIC_ALLOC_CONFIG);
	cfg_2 = PDP_GET_R(base, PDP_R_LIC_STATIC_ALLOC_CONFIG1);
	cfg_3 = PDP_GET_R(base, PDP_R_LIC_STATIC_ALLOC_CONFIG2);
	cfg_4 = PDP_GET_R(base, PDP_R_LIC_STATIC_ALLOC_CONFIG3);

	info_hw("[PDP][DBG] LIC mode(%x), buf_cfg(%x), dyn_cfg(%x), sta_cfg(%x, %x, %x)\n",
		mode, cfg_0, cfg_1, cfg_2, cfg_3, cfg_4);

	corex_mode = PDP_GET_R_COREX(base, PDP_R_LIC_OPERATION_MODE);
	corex_cfg_0 = PDP_GET_R_COREX(base, PDP_R_LIC_BUFFER_COMMON_CONFIG);
	corex_cfg_1 = PDP_GET_R_COREX(base, PDP_R_LIC_DYNAMIC_ALLOC_CONFIG);
	corex_cfg_2 = PDP_GET_R_COREX(base, PDP_R_LIC_STATIC_ALLOC_CONFIG1);
	corex_cfg_3 = PDP_GET_R_COREX(base, PDP_R_LIC_STATIC_ALLOC_CONFIG2);
	corex_cfg_4 = PDP_GET_R_COREX(base, PDP_R_LIC_STATIC_ALLOC_CONFIG3);

	info_hw("[PDP][DBG][COREX] LIC mode(%x), buf_cfg(%x), dyn_cfg(%x), sta_cfg(%x, %x, %x)\n",
			corex_mode, corex_cfg_0, corex_cfg_1, corex_cfg_2, corex_cfg_3, corex_cfg_4);

	/* Checking same value between direct and corex */
	if (!retry_cnt && ((cfg_1 != corex_cfg_1) || (cfg_2 != corex_cfg_2))) {
		retry_cnt++;
		info_hw("[PDP] retry_lic_priority\n");
		goto retry_lic_priority;
	}
}

/*
 * Context: O
 * CR type: Corex + No Corex
 */
void pdp_hw_s_line_row(void __iomem *base, bool pd_enable, int sensor_mode, u32 binning)
{
	int tmp, line_row_max, line_row = 1, max_pos_end_y = 0;
	int i;
	int margin = 100;
	u32 val = 0;
	u32 num_of_turn_on_sroi = 0;
	u32 density = 0, tail_density = 0;
	u32 mpd_on;
	u32 mpd_vbin;
	u32 bayer_height, tail_height, total_h;
	bool col_row_en = true;

	bayer_height = PDP_GET_F(base, PDP_R_LIC_INPUT_CONFIG2, PDP_F_LIC_INPUT_IMAGE_HEIGHT);
	tail_height = PDP_GET_F(base, PDP_R_LIC_INPUT_CONFIG3, PDP_F_LIC_INPUT_PDPXL_HEIGHT);

	/*
	 * If bayer_height + tail_height is biggger than 8192,
	 * line interrupt can occurs twice in one frame.
	 * So, line_row shoule be adjust not to occur twice.
	 */
	total_h = bayer_height + tail_height + PDP_LIC_VL_IMAGE;
	if (total_h > (BIT(13) - 1))
		line_row = total_h - (BIT(13) - 1);

	dbg_hw(2, "[PDP] %s: total_h(%d), line_row(%d)", __func__, total_h, line_row);

	/*
	 * stat end interrupt can't use hw limitation
	 * so lint interrupt(cin row) used instead.
	 */
	if (IS_ENABLED(USE_PDP_LINE_INTR_FOR_PDAF) && pd_enable) {
		max_pos_end_y = PDP_GET_R_COREX(base, PDP_R_PDSTAT_ROI_MAIN_MWM_EY);

		num_of_turn_on_sroi = PDP_GET_R_COREX(base, PDP_R_PDSTAT_ROI_MAIN_SROI);
		mpd_on = PDP_GET_R_COREX(base, PDP_R_I_MPD_ON);
		mpd_vbin = PDP_GET_R_COREX(base, PDP_R_I_MPD_VBIN);

		for (i = 0; i < num_of_turn_on_sroi; i++) {
			tmp = PDP_GET_R_COREX(base, PDP_R_PDSTAT_ROI_MAIN_S0EY + 4 * i);

			if (tmp > max_pos_end_y)
				max_pos_end_y = tmp;
		}

		switch (sensor_mode) {
		case VC_SENSOR_MODE_SUPER_PD_2_NORMAL:
			density = 2;
			break;
		case VC_SENSOR_MODE_ULTRA_PD_NORMAL:
		case VC_SENSOR_MODE_ULTRA_PD_2_NORMAL:
			density = 8;
			break;
		case VC_SENSOR_MODE_SUPER_PD_2_TAIL:
		case VC_SENSOR_MODE_IMX_2X1OCL_2_TAIL:
			switch (binning) {
			case 2000:
				density = 2;
				tail_density = 2;
				break;
			case 1000:
				density = 4;
				tail_density = 4;
				break;
			default:
				break;
			}
			break;
		case VC_SENSOR_MODE_2PD_MODE3:
			density = 4;
			if (mpd_on)
				max_pos_end_y = max_pos_end_y << mpd_vbin;
			break;
		case VC_SENSOR_MODE_ULTRA_PD_TAIL:
		case VC_SENSOR_MODE_ULTRA_PD_2_TAIL:
			density = 8;
			tail_density = density / 2;
			break;
		case VC_SENSOR_MODE_IMX_2X1OCL_1_TAIL:
			density = 2;
			tail_density = density / 2;
			break;
		case VC_SENSOR_MODE_SUPER_PD_3_TAIL:
			density = 8;
			tail_density = 4;
			break;
		case VC_SENSOR_MODE_SUPER_PD_4_TAIL:
			switch(binning) {
			case 1000:
				density = 6;
				tail_density = 6;
				break;
			case 3000:
				density = 4;
				tail_density = 4;
				break;
			case 6000:
				density = 4;
				tail_density = 4;
				break;
			default:
				break;
			}
			break;
		case VC_SENSOR_MODE_SUPER_PD_5_TAIL:
			switch (binning) {
			case 2000:
				density = 2;
				tail_density = 1;
				break;
			case 4000:
				density = 4;
				tail_density = 2;
				break;
			default:
				break;
			}
			break;
		default:
			err("check for sensor pd mode\n");
			density = 16;
			break;
		}

		if (tail_density) {
			line_row = max_pos_end_y * density;
			line_row += (line_row + tail_density / 2) / tail_density;
			line_row += margin;

			line_row_max = bayer_height + tail_height - 10;
		} else {
			line_row = max_pos_end_y * density + margin;

			line_row_max = bayer_height - 10;
		}

		if (line_row > line_row_max)
			line_row = line_row_max;

		info_hw("[PDP] LINE_IRQ for pd sensor mode(%d), density(%d, tail:%d), max_pos_end_y: %d, line_row: %d, num_sroi: %d",
			sensor_mode, density, tail_density, max_pos_end_y, line_row, num_of_turn_on_sroi);
	}

	val = PDP_SET_V(val, PDP_F_IP_INT_COL_CORD, 0);
	val = PDP_SET_V(val, PDP_F_IP_INT_ROW_CORD, line_row);
	PDP_SET_R(base, PDP_R_IP_INT_ON_COL_ROW_CORD, val);
	PDP_SET_F(base, PDP_R_IP_COREX_HW_TRIGGER_GAP, PDP_F_IP_COREX_HW_TRIGGER_GAP, 0); /* TODO: @Long V-blank */

	val = 0;
	val = PDP_SET_V(val, PDP_F_IP_INT_COL_EN, col_row_en);
	val = PDP_SET_V(val, PDP_F_IP_INT_ROW_EN, col_row_en);
	PDP_SET_R(base, PDP_R_IP_INT_ON_COL_ROW, val);

}

/*
 * Context: O
 * CR type: Corex + No Corex
 */
static void _pdp_hw_s_common(void __iomem *base)
{
	u32 val;

	/*
	 * Keep Reset Value
	 *
	 * @PDP_R_IP_POST_FRAME_GAP, PDP_F_IP_POST_FRAME_GAP
	 * @PDP_R_IP_COUTFIFO_END_ON_VSYNC_FALL, PDP_F_IP_COUTFIFO_END_ON_VSYNC_FALL
	 */

	/*
	 * Set Paramer Value
	 *
	 * sensor config
	 */

	/*
	 * Set Fiexed Value
	 *
	 * 1: alows interupt, 0: masks interupt
	 */
	PDP_SET_F(base, PDP_R_INTERRUPT_AUTO_MASK, PDP_F_INTERRUPT_AUTO_MASK, 1);

	val = 0;
	val = PDP_SET_V(val, PDP_F_IP_INT_COL_EN, 1);
	val = PDP_SET_V(val, PDP_F_IP_INT_ROW_EN, 1);
	PDP_SET_R(base, PDP_R_IP_INT_ON_COL_ROW, val);

	/*
	 * If this CR is set to START_ASAP, start & end intterrupt is overllaped.
	 * In other words, there is no v-vblank.
	 */
	PDP_SET_F(base, PDP_R_IP_USE_INPUT_FRAME_START_IN, PDP_F_IP_USE_INPUT_FRAME_START_IN, START_VVALID_RISE);
}

/*
 * Context: O
 * CR type: Corex + No Corex
 */
static void _pdp_hw_s_int_mask(void __iomem *base, u32 sensor_type,
		u32 path, u32 ex_mode)
{
	u32 corrupted_int_mask, int2_mask;
	u32 int1_mask = INT1_EN_MASK;

	PDP_SET_F(base, PDP_R_CONTINT_LEVEL_PULSE_N_SEL, PDP_F_CONTINT_LEVEL_PULSE_N_SEL, INT_LEVEL);

	/*
	 * Set Fiexed Value
	 *
	 * 1: alows interupt, 0: masks interupt
	 */

	/*
	 * The IP_END_INTERRUPT_ENABLE makes and condition.
	 * If some bits are set, frame end interrupt is generated when all designated blocks must be finished.
	 * So, unused block must be not set.
	 * Usually all bit doesn't have to set becase we don't care about each block end.
	 * We are only interested in core end interrupt.
	 */
	PDP_SET_F(base, PDP_R_IP_USE_END_INTERRUPT_ENABLE, PDP_F_IP_USE_END_INTERRUPT_ENABLE, 0);
	PDP_SET_F(base, PDP_R_IP_END_INTERRUPT_ENABLE, PDP_F_IP_END_INTERRUPT_ENABLE, 0);

	/* IP_CORRUPTED_INTERRUPT_ENABLE makes or condition. */
	if (path == DMA && sensor_type == SENSOR_TYPE_MOD3) {
		corrupted_int_mask = 0x3F9F;
		int2_mask = INT2_EN_MASK & ~((1 << CINFIFO_LINES_ERROR) | (1 << CINFIFO_COLUMNS_ERROR));
	} else {
		corrupted_int_mask = 0x3FFF;
		int2_mask = INT2_EN_MASK;
	}

	/**
	 * FRO mode interrupt masking policy for PDP v3.1
	 *
	 * 1. INT1
	 * For FRO mode with frame ID decoder in CSIS DMA,
	 * the VOTF token is only generated for every 16msec which sends preview frame.
	 * CSIS VC0, however, still generate VVALID signal
	 * because it receives 960fps output from sensor.
	 * It makes PDP asserts frame start event.
	 * To avoid this issue, it uses line interrupt instead of frame start.
	 *
	 * 2. INT2
	 * For FRO mode with frame ID decoder in CSIS DMA,
	 * it asserts 'votf_lostcon_img' interrupt for every frame.
	 * It's the intended error signal to be ignored for getting
	 * VOTF operation stability.
	 */
	if (IS_DUALFPS_MODE(ex_mode)) {
		int1_mask &= ~(1 << FRAME_START);
		int1_mask |= (1 << FRAME_INT_ON_ROW_COL_INFO);
		int2_mask &= ~(1 << VOTF_LOST_CON_IMG);
	}

	PDP_SET_F(base, PDP_R_IP_CORRUPTED_INTERRUPT_ENABLE,
		PDP_F_IP_CORRUPTED_INTERRUPT_ENABLE, corrupted_int_mask);

	PDP_SET_F(base, PDP_R_CONTINT_INT1_ENABLE, PDP_F_CONTINT_INT1_ENABLE, int1_mask);
	PDP_SET_F(base, PDP_R_CONTINT_INT2_ENABLE, PDP_F_CONTINT_INT2_ENABLE, int2_mask);
}

/*
 * Context: O
 * CR type: Shadow/Dual
 */
static void _pdp_hw_s_secure_id(void __iomem *base)
{
	/*
	 * Set Paramer Value
	 *
	 * scenario
	 * 0: Non-secure,  1: Secure
	 */
	PDP_SET_F(base, PDP_R_SECU_CTRL_SEQID, PDP_F_SECU_CTRL_SEQID, 0); /* TODO: get secure scenario */
}

/*
 * Context: O
 * CR type: No Corex
 */
static void _pdp_hw_s_shadow(void __iomem *base)
{
	/*
	 * Set Fixed Value
	 *
	 * @PDP_R_SELREGISTER - 0: setB, 1: setA
	 * @PDP_R_SELREGISTERMODE - 0: Shadow, 1: immediately
	 * @PDP_R_SHADOW_CONTROL - 0: shadow endable, 1: shadow disable
	 * @PDP_R_SHADOW_SW_TRIGGER - 0: no SW trigger, 1: SW trigger pulse
	 *
	 * Only setB is valid.
	 * And SW trigger for shadow should be needed before stream on
	 * because there is no HW trigger before stream on.
	 */
	PDP_SET_F(base, PDP_R_SELREGISTER, PDP_F_SELREGISTER, PDP_SHADOW_SET_B);
	PDP_SET_F(base, PDP_R_SELREGISTERMODE, PDP_F_SELREGISTERMODE, PDP_SHADOW_MODE);
	PDP_SET_F(base, PDP_R_SHADOW_CONTROL, PDP_F_SHADOW_DISABLE, PDP_SHADOW_EN);
	PDP_SET_F(base, PDP_R_SHADOW_SW_TRIGGER, PDP_F_SHADOW_SW_TRIGGER, 1);
	PDP_SET_F(base, PDP_R_SHADOW_CONTROL, PDP_F_SHADOW_DISABLE, PDP_SHADOW_DIS);
}

/*
 * Context: O
 * CR type: Corex
 */
static void pdp_hw_s_sdc(void __iomem *base, u32 width, u32 height, u32 comp_width)
{
	int i;
	u32 index;
	u32 val;
	int count;
	const struct is_pdp_reg *pdp_sdc_setfile;

	if (width == SDC_WIDTH_HD) {
		pdp_sdc_setfile = pdp_sdc_setfile_hd;
		count = ARRAY_SIZE(pdp_sdc_setfile_hd);
	} else if (width == SDC_WIDTH_FHD) {
		if (comp_width == SDC_COMP_WIDTH_FHD_50) {
			pdp_sdc_setfile = pdp_sdc_setfile_fhd;
			count = ARRAY_SIZE(pdp_sdc_setfile_fhd);
		} else if (comp_width == SDC_COMP_WIDTH_FHD_70) {
			pdp_sdc_setfile = pdp_sdc_setfile_fhd_70;
			count = ARRAY_SIZE(pdp_sdc_setfile_fhd_70);
		} else {
			count = 0;
			err_hw("[PDP] invalid SDC compression width (width: %d, comp_width: %d)",
				width, comp_width);
		}
	} else {
		count = 0;
		err_hw("[PDP] invalide SDC size (width: %d)", width);
	}

	for (i = 0; i < count; i++) {
		index = pdp_sdc_setfile[i].index;
		val = pdp_sdc_setfile[i].init_value;

		writel(val, base + index);
	}

	/* Set original image size for SDC */
	val = 0;
	val = PDP_SET_V(val, PDP_F_CINFIFO_OUTPUT_IMAGE_WIDTH, height);
	val = PDP_SET_V(val, PDP_F_CINFIFO_OUTPUT_IMAGE_HEIGHT, width);
	PDP_SET_R(base, PDP_R_IMAGE_SIZE, val);
}

/*
 * Context: O
 * CR type: Corex
 */
static void _pdp_hw_s_rdma_init(void __iomem *base, struct is_pdp *pdp,
		u32 height, u32 rmo, u32 en_sdc, u32 en_votf, u32 en_dma,
		struct is_sensor_cfg *sensor_cfg)
{
	if (en_dma == 0) {
		PDP_SET_R(base, PDP_R_RDMA_BAYER_EN, 0);
		return;
	}

	/*
	 * Keep Reset Value
	 *
	 * @PDP_R_RDMA_BAYER_THRESHOLD,
	 */
	PDP_SET_R(base, PDP_R_RDMA_BAYER_MAX_MO, rmo);
	info("[HW][PDP] set value of RDMA_MO: %d", rmo);

	/* For v3.1 */
	if (ver_idx == 1) {
		u32 en_rdma_vvalid = 0;

		if (en_votf && !IS_DUALFPS_MODE(sensor_cfg->ex_mode))
			en_rdma_vvalid = 1;

		/* Set Start frame upon VVALID rise detection */
		PDP_SET_R(base, PDP_R_IP_RDMA_VVALID_START_ENABLE, en_rdma_vvalid);
		/* SDC in pdp core path on (core0/core1 only available */
		PDP_SET_R(base, PDP_R_IP_SDC_PATH_ON, en_sdc);
	}

	/* BAYER */
	PDP_SET_R(base, PDP_R_RDMA_BAYER_BUSINFO,
			en_votf ? 0x3 : 0); /* cache_hint[2:0] cache-noalloc-type for DRAM update mode */
	PDP_SET_R(base, PDP_R_RDMA_BAYER_VOTF_EN,
			en_votf ? (USE_PDP_VOTF_LEGACY_RING << 2 | 1) : 0);
	PDP_SET_R(base, PDP_R_RDMA_BAYER_EN, en_dma);
}

/*
 * Context: O
 * CR type: Corex
 */
static void _pdp_hw_s_af_rdma_init(void __iomem *base, u32 rmo,
	u32 en_votf, u32 en_dma)
{
	if (en_dma == 0) {
		PDP_SET_R(base, PDP_R_RDMA_AF_EN, 0);
		return;
	}

	/*
	 * Keep Reset Value
	 *
	 * @PDP_R_RDMA_AF_MO,
	 * @PDP_R_RDMA_AF_THRESHOLD,
	 */
	PDP_SET_R(base, PDP_R_RDMA_AF_MAX_MO, rmo);

	/* AF */
	PDP_SET_R(base, PDP_R_RDMA_AF_VOTF_EN,
			en_votf ? (USE_PDP_VOTF_LEGACY_RING << 2 | 1) : 0);

	/*  rdma image height and af height are irrelevant */
	if (ver_idx == 1)
		PDP_SET_R(base, PDP_R_RDMA_IMG_AF_VARIABLE, 1);

	PDP_SET_R(base, PDP_R_RDMA_AF_EN, en_dma);
}

/*
 * When using AF RDMA, line ratio between bayer DMA and af DMA must be 4:1.
 * 4 If the raio is not 4:1, tail count must be reset at frame end time.
 */
void pdp_hw_s_af_rdma_tail_count_reset(void __iomem *base)
{
	u32 enable;

	/* AF */
	enable = PDP_GET_R(base, PDP_R_RDMA_AF_EN);
	if (enable) {
		PDP_SET_R_DIRECT(base, PDP_R_RDMA_AF_EN, 0);

		PDP_SET_R_DIRECT(base, PDP_R_RDMA_AF_EN, 1);
	}
}
/*============= Global Function =============*/
u32 pdp_hw_g_ip_version(void __iomem *base)
{
	u32 version = PDP_GET_R(base, PDP_R_IP_VERSION);

	if (version == PDP_VERSION_V3_0) {
		ver_idx = 0;
		pdp_tnum = 4;
	} else if (version == PDP_VERSION_V3_1) {
		ver_idx = 1;
		pdp_tnum = 4;
	} else if (version == PDP_VERSION_V3_3) {
		ver_idx = 1;
		pdp_tnum = 3;
	} else {
		err_hw("[PDP] wrong version info(%08X)", version);
		version = -EINVAL;
	}

	info_hw("[PDP] use V3.%d", ver_idx);

	return version;
}

unsigned int pdp_hw_g_idle_state(void __iomem *base)
{
	u32 idle;

	idle = PDP_GET_F(base, PDP_R_IDLENESS_STATUS, PDP_F_IDLENESS_STATUS);

	return idle;
}

void pdp_hw_get_line(void __iomem *base, u32 *total_line, u32 *curr_line)
{
	*total_line = PDP_GET_F(base, PDP_R_CINFIFO_OUTPUT_IMAGE_DIMENSIONS,
		PDP_F_CINFIFO_OUTPUT_IMAGE_HEIGHT);

	*curr_line = PDP_GET_R(base, PDP_R_CINFIFO_OUTPUT_LINE_CNT);

	return;
}

/* config */
void pdp_hw_s_before_sensor_start(void __iomem *base)
{
#if defined(CSIS_PDP_VOTF_GLOBAL_WA)
	/* Disable rdma_vvalid_enable if W/A applied */
	PDP_SET_R(base, PDP_R_IP_RDMA_VVALID_START_ENABLE, 0);
#endif
}

/*
 * Context: O
 * CR type: No Corex
 */
void pdp_hw_s_global_enable(void __iomem *base, bool enable)
{
	/*
	 * CAUTION
	 * @sequence: The corex must be enabled before global enable.
	 */
	_pdp_hw_s_corex_init(base, enable);
	_pdp_hw_s_corex_start(base, enable);

	/*
	 * Set Fixed Value
	 */
	PDP_SET_F(base, PDP_R_GLOBAL_ENABLE_STOP_CRPT, PDP_F_GLOBAL_ENABLE_STOP_CRPT, enable);

	PDP_SET_F(base, PDP_R_FRO_GLOBAL_ENABLE, PDP_F_FRO_GLOBAL_ENABLE, enable);
	PDP_SET_F(base, PDP_R_GLOBAL_ENABLE, PDP_F_GLOBAL_ENABLE, enable);
}

void pdp_hw_s_global_enable_clear(void __iomem *base)
{
	/* Clearing registers that is affected by global_enable signal */
	if (ver_idx == 1)
		PDP_SET_F(base, PDP_R_GLOBAL_ENABLE, PDP_F_GLOBAL_ENABLE_CLEAR, 1);
}

/*
 * Context: O
 * CR type: No Corex
 */
int pdp_hw_s_one_shot_enable(struct is_pdp *pdp)
{
	int ret = 0;
	unsigned long flag;
	u32 idle;
	u32 try_cnt = 0;
	u32 total_line;
	u32 prev_line, prev_col;
	u32 curr_line, curr_col;
	u32 rmo;
	void __iomem *base = pdp->base;

	PDP_SET_F(base, PDP_R_FRO_GLOBAL_ENABLE, PDP_F_FRO_GLOBAL_ENABLE, 0);
	PDP_SET_F(base, PDP_R_GLOBAL_ENABLE, PDP_F_GLOBAL_ENABLE, 0);

	total_line = PDP_GET_F(base, PDP_R_CINFIFO_OUTPUT_IMAGE_DIMENSIONS,
		PDP_F_CINFIFO_OUTPUT_IMAGE_HEIGHT);
	prev_line = curr_line = PDP_GET_R(base, PDP_R_CINFIFO_OUTPUT_LINE_CNT);
	prev_col = curr_col = PDP_GET_R(base, PDP_R_CINFIFO_OUTPUT_COL_CNT);

	spin_lock_irqsave(&pdp->slock_oneshot, flag);
	idle = PDP_GET_F(base, PDP_R_IDLENESS_STATUS, PDP_F_IDLENESS_STATUS);
	while (!idle) {
		/* Increase RMO */
		if (!try_cnt) {
			rmo = PDP_GET_R(base, PDP_R_RDMA_BAYER_MAX_MO);
			rmo = max(rmo, pdp->rmo) + pdp->rmo;
			rmo = min(rmo, (u32)((1 << pdp_fields[PDP_F_RDMA_BAYER_MAX_MO].bit_width) - 1));

			PDP_SET_R(base, PDP_R_RDMA_BAYER_MAX_MO, rmo);
			PDP_SET_R(base, PDP_R_RDMA_AF_MAX_MO, rmo);
			pdp->rmo_tick = PDP_RDMA_MO_TICK;
		}

		info_hw("[PDP%d] oneshot busy(RMO:%d, total:%d, curr:%d,%d, try:%d)\n",
			pdp->id, rmo, total_line, curr_line, curr_col, try_cnt);

		try_cnt++;
		if (try_cnt >= 3) {
			if (prev_col == curr_col && prev_line == curr_line) {
				set_bit(IS_PDP_ONESHOT_PENDING, &pdp->state);
				err_hw("[PDP%d] PDP stuck", pdp->id);
				ret = -EBUSY;
			} else {
				set_bit(IS_PDP_ONESHOT_PENDING, &pdp->state);
				info_hw("[PDP%d] set oneshot pending\n", pdp->id);
				ret = 0;
			}

			spin_unlock_irqrestore(&pdp->slock_oneshot, flag);

			return ret;
		}

		udelay(30); /* 3us * 10000 = 30 ms */

		curr_line = PDP_GET_R(base, PDP_R_CINFIFO_OUTPUT_LINE_CNT);
		curr_col = PDP_GET_R(base, PDP_R_CINFIFO_OUTPUT_COL_CNT);
		idle = PDP_GET_F(base, PDP_R_IDLENESS_STATUS, PDP_F_IDLENESS_STATUS);
	}
	spin_unlock_irqrestore(&pdp->slock_oneshot, flag);

	/* Restore RMO */
	if (!try_cnt && (--pdp->rmo_tick <= 0)) {
		rmo = PDP_GET_R(base, PDP_R_RDMA_BAYER_MAX_MO);
		if (rmo != pdp->rmo) {
			PDP_SET_R(base, PDP_R_RDMA_BAYER_MAX_MO, pdp->rmo);
			PDP_SET_R(base, PDP_R_RDMA_AF_MAX_MO, pdp->rmo);
			info_hw("[PDP%d] RMO:%d->%d", pdp->id, rmo, pdp->rmo);
		}
	}

	pdp_hw_s_af_rdma_tail_count_reset(base);

	PDP_SET_F(base, PDP_R_FRO_ONE_SHOT_ENABLE, PDP_F_FRO_ONE_SHOT_ENABLE, 0);
	PDP_SET_F(base, PDP_R_FRO_ONE_SHOT_ENABLE, PDP_F_FRO_ONE_SHOT_ENABLE, 1);

	PDP_SET_F(base, PDP_R_ONE_SHOT_ENABLE, PDP_F_ONE_SHOT_ENABLE, 0);
	PDP_SET_F(base, PDP_R_ONE_SHOT_ENABLE, PDP_F_ONE_SHOT_ENABLE, 1);

	return ret;
}

void pdp_hw_s_corex_enable(void __iomem *base, bool enable)
{
	/*
	 * CAUTION
	 * @sequence: The corex must be enabled before global enable.
	 */
	_pdp_hw_s_corex_init(base, enable);
	_pdp_hw_s_corex_start(base, enable);

#ifdef CONFIG_CHECK_PDP_COREX_UPDATE
	/* for debugging */
	if (0x00094301 != PDP_GET_R(base, PDP_R_BBBSIZE_FLAG)) {
		info_hw("[PDP] SFR DUMP (v3.1.0)\n");
		is_hw_dump_regs(base, pdp_regs, PDP_REG_CNT);

		info_hw("[PDP] COREX SFR DUMP (v3.1.0)\n");
		is_hw_dump_regs(base, pdp_regs_corex, PDP_REG_CNT);

		is_debug_s2d(true, "CR shift");
	}
#endif
}

void pdp_hw_s_corex_type(void __iomem *base, u32 type)
{
	PDP_SET_F(base, PDP_R_COREX_UPDATE_TYPE_0, PDP_F_COREX_UPDATE_TYPE_0, type);

	if (type == COREX_IGNORE)
		_pdp_hw_wait_corex_idle(base);
}

void pdp_hw_g_corex_state(void __iomem *base, u32 *corex_enable)
{
	*corex_enable = PDP_GET_F(base, PDP_R_COREX_ENABLE, PDP_F_COREX_ENABLE);
}

/* state */
int pdp_hw_g_stat0(void __iomem *base, void *buf, size_t len)
{
	return 0;
}

void pdp_hw_g_pdstat_size(u32 *width, u32 *height, u32 *bits_per_pixel)
{
	*width = PDP_STAT_DMA_WIDTH;
	*height = PDP_STAT_TOTAL_SIZE / PDP_STAT_DMA_WIDTH;
	*bits_per_pixel = 8;
}

void pdp_hw_s_pdstat_path(void __iomem *base, bool enable)
{
	/* always set true due to always map between LIC and core, only for v3.0 */
	if (ver_idx == 0)
		PDP_SET_F(base, PDP_R_IP_PDSTAT_PATH_ON, PDP_F_IP_PDSTAT_PATH_ON, true);

	if (!enable) {
		PDP_SET_R(base, PDP_R_RDMA_AF_EN, 0);
		PDP_SET_R(base, PDP_R_REORDER_ON, 0);
		PDP_SET_R(base, PDP_R_I_MPD_ON, 0);
	}
}

void pdp_hw_s_pd_size(void __iomem *base, u32 width, u32 height, u32 hwformat,
	u32 img_pixelsize)
{
	u32 val;
	u32 format;
	u32 byte_per_line;

	switch (hwformat) {
	case HW_FORMAT_RAW10:
		if (img_pixelsize == DMA_INOUT_BIT_WIDTH_10BIT)
			format = PDP_DMA_FMT_U10BIT_UNPACK_MSB_ZERO;
		else if (img_pixelsize == DMA_INOUT_BIT_WIDTH_12BIT)
			format = PDP_DMA_FMT_U12BIT_UNPACK_MSB_ZERO;
		else if (img_pixelsize == DMA_INOUT_BIT_WIDTH_14BIT)
			format = PDP_DMA_FMT_U14BIT_UNPACK_MSB_ZERO;
		else
			format = PDP_DMA_FMT_U10BIT_UNPACK_MSB_ZERO;

		byte_per_line = ALIGN(width * 16 / BITS_PER_BYTE, 16);
		break;
	default:
		info_hw("[%s][PDP] invalid af format (%02X)", __func__, hwformat);
		return;
	}

	/* AF RDMA */
	PDP_SET_F(base, PDP_R_RDMA_AF_DATA_FORMAT, PDP_F_RDMA_AF_DATA_FORMAT_AF, format);
	PDP_SET_F(base, PDP_R_RDMA_AF_WIDTH, PDP_F_RDMA_AF_WIDTH, width);
	PDP_SET_F(base, PDP_R_RDMA_AF_HEIGHT, PDP_F_RDMA_AF_HEIGHT, height);
	PDP_SET_F(base, PDP_R_RDMA_AF_IMG_STRIDE_1P, PDP_F_RDMA_AF_IMG_STRIDE_1P, byte_per_line);

	/* LIC */
	val = 0;
	val = PDP_SET_V(val, PDP_F_LIC_INPUT_PDPXL_WIDTH, ALIGN(width, 4)); /* Mode3 Y size */
	val = PDP_SET_V(val, PDP_F_LIC_INPUT_PDPXL_HEIGHT, height); /* Mode3 Y size */
	PDP_SET_R(base, PDP_R_LIC_INPUT_CONFIG3, val);

}

void pdp_hw_s_img_size(void __iomem *base,
	struct is_crop full_size,
	struct is_crop crop_size,
	struct is_crop comp_size,
	u32 hwformat, u32 pixelsize,
	struct is_sensor_cfg *sensor_cfg)
{
	u32 val;
	u32 format;
	u32 byte_per_line;
	u32 h;
	u32 adjust_h;

	/*
	 * Set Parameter Value
	 *
	 * sesnro config
	 */
	switch (hwformat) {
	case DMA_INOUT_FORMAT_BAYER_PACKED:
		if (pixelsize == DMA_INOUT_BIT_WIDTH_10BIT) {
			format = PDP_DMA_FMT_U10BIT_PACK;
		} else if (pixelsize == DMA_INOUT_BIT_WIDTH_12BIT) {
			format = PDP_DMA_FMT_U12BIT_PACK;
			format = PDP_SET_V(format, PDP_F_RDMA_BAYER_DATA_FORMAT_MSBALIGN, 1);
		} else if (pixelsize == DMA_INOUT_BIT_WIDTH_14BIT) {
			format = PDP_DMA_FMT_U14BIT_PACK;
		} else {
			err_hw("[PDP] invalid packed pixelsize(%d)", pixelsize);
			return;
		}

		byte_per_line = ALIGN(full_size.w * pixelsize / BITS_PER_BYTE, 16);
		break;
	case DMA_INOUT_FORMAT_BAYER:
		/*
		 * TODO: Is it used LSB_ZERO case?
		 * In case of old version chip, MSB zero case is only supported
		 */
		if (pixelsize == OTF_INPUT_BIT_WIDTH_8BIT) {
			format = PDP_DMA_FMT_U8BIT_UNPACK_MSB_ZERO;
		} else if (pixelsize == DMA_INOUT_BIT_WIDTH_10BIT) {
			format = PDP_DMA_FMT_U10BIT_UNPACK_MSB_ZERO;
		} else if (pixelsize == DMA_INOUT_BIT_WIDTH_12BIT) {
			format = PDP_DMA_FMT_U12BIT_UNPACK_MSB_ZERO;
		} else if (pixelsize == DMA_INOUT_BIT_WIDTH_14BIT) {
			format = PDP_DMA_FMT_U14BIT_UNPACK_MSB_ZERO;
		} else {
			err_hw("[PDP] invalid unpacked pixelsize(%d)", pixelsize);
			return;
		}
		byte_per_line = ALIGN(full_size.w * 2, 16);
		break;
	case DMA_INOUT_FORMAT_ANDROID:
		if (pixelsize == DMA_INOUT_BIT_WIDTH_10BIT) {
			format = PDP_DMA_FMT_ANDROID10;
		} else if (pixelsize == DMA_INOUT_BIT_WIDTH_12BIT) {
			format = PDP_DMA_FMT_ANDROID12;
		} else {
			err_hw("[PDP] invalid android pixelsize(%d)", pixelsize);
			return;
		}
		byte_per_line = ALIGN(full_size.w * pixelsize / BITS_PER_BYTE, 16);
		break;
	default:
		err_hw("[PDP] invalid bayer format (%02X)", hwformat);
		return;
	}

	/*
	 * PDP constraint:
	 * In case of (image_h : pd_h == 1 : 1),
	 * the height of RDMA & LIC must be twice of the real height.
	 */
	adjust_h = (sensor_cfg && sensor_cfg->votf &&
			sensor_cfg->img_pd_ratio == IS_IMG_PD_RATIO_1_1) ? 1 : 0;
	h = adjust_h ? (crop_size.h * 2) : crop_size.h;

	/* Bayer RDMA */
	PDP_SET_F(base, PDP_R_RDMA_BAYER_DATA_FORMAT, PDP_F_RDMA_BAYER_DATA_FORMAT_BAYER, format);
	PDP_SET_F(base, PDP_R_RDMA_BAYER_WIDTH, PDP_F_RDMA_BAYER_WIDTH, comp_size.w);
	PDP_SET_F(base, PDP_R_RDMA_BAYER_HEIGHT, PDP_F_RDMA_BAYER_HEIGHT, h);
	PDP_SET_F(base, PDP_R_RDMA_BAYER_IMG_STRIDE_1P, PDP_F_RDMA_BAYER_IMG_STRIDE_1P, byte_per_line);

	/* LIC */
	val = 0;
	val = PDP_SET_V(val, PDP_F_LIC_INPUT_IMAGE_WIDTH, ALIGN(crop_size.w, 4));
	val = PDP_SET_V(val, PDP_F_LIC_INPUT_IMAGE_HEIGHT, h);
	PDP_SET_R(base, PDP_R_LIC_INPUT_CONFIG2, val);

	/* CINFIFO_OUTPUT */
	PDP_SET_R(base, PDP_R_OUT_SIZE_V, crop_size.h);

	h += PDP_LIC_VL_IMAGE;
	val = 0;
	val = PDP_SET_V(val, PDP_F_CINFIFO_OUTPUT_IMAGE_WIDTH, crop_size.w);
	val = PDP_SET_V(val, PDP_F_CINFIFO_OUTPUT_IMAGE_HEIGHT, h);
	PDP_SET_R(base, PDP_R_CINFIFO_OUTPUT_IMAGE_DIMENSIONS, val);
}

static void _pdp_hw_s_bpc_reorder_sram(void __iomem *base, int sensor_type, u32 offset, u32 size)
{
	if (ver_idx == 0)
		return;

	if (offset + size > REORDER_BPC_SRAM_MAX)
		warn("BPC/Reorder sram set is wrong (offset:%d + size:%d > %d)",
				offset, size, REORDER_BPC_SRAM_MAX);

	if (sensor_type == SENSOR_TYPE_MSPD || sensor_type == SENSOR_TYPE_MSPD_TAIL) {
		PDP_SET_R(base, PDP_R_REORDER_SRAM_OFFSET, offset);
		PDP_SET_R(base, PDP_R_BPC_SRAM_OFFSET, offset);
	}
}

int pdp_hw_get_bpc_reorder_sram_size(u32 width, int vc_sensor_mode)
{
	int size = 0;

	switch (vc_sensor_mode) {
	case VC_SENSOR_MODE_MSPD_TAIL:
	case VC_SENSOR_MODE_MSPD_GLOBAL_TAIL:
		size = width * 2;
		break;
	case VC_SENSOR_MODE_ULTRA_PD_TAIL:
	case VC_SENSOR_MODE_ULTRA_PD_2_TAIL:
	case VC_SENSOR_MODE_ULTRA_PD_3_TAIL:
	case VC_SENSOR_MODE_SUPER_PD_3_TAIL:
	case VC_SENSOR_MODE_SUPER_PD_5_TAIL:
	case VC_SENSOR_MODE_IMX_2X1OCL_1_TAIL:
		size = width;
		break;
	case VC_SENSOR_MODE_SUPER_PD_TAIL:
	case VC_SENSOR_MODE_SUPER_PD_2_TAIL:
	case VC_SENSOR_MODE_SUPER_PD_4_TAIL:
	case VC_SENSOR_MODE_IMX_2X1OCL_2_TAIL:
		size = width / 2;
		break;
	default:
		warn("invalid vc_sensor_mode(%d) for bpc/reorder sram", vc_sensor_mode);
		break;
	}

	return size;
}

void pdp_hw_s_core(struct is_pdp *pdp, bool pd_enable, struct is_sensor_cfg *sensor_cfg,
	struct is_crop img_full_size,
	struct is_crop img_crop_size,
	struct is_crop img_comp_size,
	u32 img_hwformat, u32 img_pixelsize,
	u32 pd_width, u32 pd_height, u32 pd_hwformat,
	u32 sensor_type, u32 path, int sensor_mode, u32 fps, u32 en_sdc, u32 en_votf,
	u32 num_buffers, u32 binning, u32 position)
{
	u32 rmo = PDP_RDMA_MO_DEFAULT;
	u32 en_dma, en_afdma;
	void __iomem *base = pdp->base;

	if (en_sdc) {
		if (pdp->id == 0 || pdp->id == 1) {
			pdp_hw_s_sdc(base,
				img_crop_size.w, img_crop_size.h, img_comp_size.w);
		} else {
			warn("[PDP] Can't use SDC in CH%d, disable forcely", pdp->id);
			en_sdc = 0;
		}
	}

	pdp_hw_s_img_size(base, img_full_size, img_crop_size, img_comp_size,
		img_hwformat, img_pixelsize, sensor_cfg);
	pdp_hw_s_pd_size(base, pd_width, pd_height, pd_hwformat, img_pixelsize);

	/* EVT0 only, when use PDP CH2, always off pdstat_path_on to not map pdp core
	 * It will removed at EVT1
	 */
	if (ver_idx == 0 && pdp->id != 2)
		pdp_hw_s_pdstat_path(base, pd_enable);
	_pdp_hw_s_bpc_reorder_sram(base, sensor_type,
			pdp->bpc_rod_offset, pdp->bpc_rod_size);

	_pdp_hw_s_cout_fifo(base, path);
	_pdp_hw_s_lic_context(base, img_pixelsize, sensor_type);
	_pdp_hw_s_common(base);
	pdp_hw_s_line_row(base, pd_enable, sensor_mode, binning);
	_pdp_hw_s_int_mask(base, sensor_type, path, sensor_cfg->ex_mode);

	_pdp_hw_s_secure_id(base);
	_pdp_hw_s_shadow(base);

	if (path == DMA) {
		en_dma = 1;

		if (fps >= 480) {
			rmo = PDP_RDMA_MO_FPS480; /* This is HW guide value. */
		} else if (fps >= 240) {
			rmo = PDP_RDMA_MO_FPS240; /* This is HW guide value. */
		} else if (fps >= 60) {
			rmo = PDP_RDMA_MO_FPS60; /* This is HW guide value. */
		} else if (position == SENSOR_POSITION_FRONT) {
#ifdef PDP_RDMA_MO_FRONT
			rmo = PDP_RDMA_MO_FRONT;
#else
			rmo = 2;
#endif
			info("[PDP][DBG][POS:%d] RDMA MO set %d", position, rmo);
		}

#if defined(PDP_RDMA_MO_LIMIT) /* Fixed RDMA MO */
		rmo = PDP_RDMA_MO_LIMIT;
#endif

		pdp->rmo = rmo;

		if (sensor_type == SENSOR_TYPE_MOD3 || sensor_type == SENSOR_TYPE_MSPD_TAIL)
			en_afdma = 1;
		else
			en_afdma = 0;
	} else {
		en_dma = 0;
		en_afdma = 0;
	}

	_pdp_hw_s_af_rdma_init(base, rmo, en_votf, en_afdma);
	_pdp_hw_s_rdma_init(base, pdp, img_crop_size.h,
		rmo, en_sdc, en_votf, en_dma, sensor_cfg);

	pdp_hw_s_fro(base, num_buffers);
}

/*
 * Context: X (ch0 only)
 * CR type: No Corex
 */
void pdp_hw_s_init(void __iomem *base)
{
	PDP_SET_F(base, PDP_R_AUTO_MASK_PREADY, PDP_F_AUTO_MASK_PREADY, 1);
	PDP_SET_F(base, PDP_R_IP_PROCESSING, PDP_F_IP_PROCESSING, 1);
}

void pdp_hw_s_reset(void __iomem *base)
{
	PDP_SET_R(base, PDP_R_SW_RESET, 0x1);
}

/*
 * @base: ch0 base only
 * @ch: each context channel
 */
void pdp_hw_s_global(void __iomem *base, u32 ch, u32 lic_mode, u32 lic_14bit, void *data)
{
	_pdp_hw_s_lic_ch0(base, ch, lic_mode, lic_14bit, (struct pdp_lic_lut *)data);
}

/*
 * @base: ch0 base only
 * @ch: each context channel
 */
void pdp_hw_s_context(void __iomem *base, u32 ch, u32 path, u32 dma_only)
{
	_pdp_hw_s_lic_ch0_priority(base, ch, path, dma_only);
}

void pdp_hw_s_path(void __iomem *base, u32 path)
{
	/*
	 * Set Paramer Value
	 *
	 * path
	 * 0: OTF,  1: strgen, 2: RDMA, 3: n/a
	 */
	PDP_SET_F(base, PDP_R_IP_CHAIN_INPUT_SELECT, PDP_F_IP_CHAIN_INPUT_SELECT, path);
}

/*
 * Context: O
 * CR type: Corex
 */
void pdp_hw_s_wdma_init(void __iomem *base, u32 ch)
{
	u32 total_size;
	u32 width = PDP_STAT_DMA_WIDTH;
	u32 height;
	u32 stride;
	u32 format = PDP_DMA_FMT_U8BIT_PACK;
	u32 stat0_size = 2500;
	u32 mroi_on, mroi_no_x, mroi_no_y;
	u32 roi_w, roi_h;

	/*
	 * Set Fixed Value
	 *
	 * tatal_size = width * height
	 */

	mroi_on = PDP_GET_R(base, PDP_R_PDSTAT_ROI_MAIN_MWS_ON);
	mroi_no_x = PDP_GET_R(base, PDP_R_PDSTAT_ROI_MAIN_MWS_NO_X);
	mroi_no_y = PDP_GET_R(base, PDP_R_PDSTAT_ROI_MAIN_MWS_NO_Y);
	roi_w = PDP_GET_R(base, PDP_R_PDSTAT_IN_SIZE_X);
	roi_h = PDP_GET_R(base, PDP_R_PDSTAT_IN_SIZE_Y);

	total_size = stat0_size + (mroi_on * (mroi_no_x * mroi_no_y * 192 + 4));
	height = (total_size + (width - 1)) / width;
	if (IS_ENABLED(USE_DEBUG_PD_DUMP) && ((DEBUG_PD_DUMP_CH >> ch) & 0x1)) {
		/* TODO: get detail guide */

		width = roi_w * 2;
		height = roi_h;
		stride = (((roi_w * 2 * 10 + 7) / 8 + 15 ) / 16) * 16;
		format = PDP_DMA_FMT_U10BIT_PACK;

		PDP_SET_R(base, PDP_R_PDSTAT_DUMP_ON, 1);
	} else {
		height = (total_size + (width - 1)) / width;
		stride = width;
	}

	PDP_SET_F(base, PDP_R_WDMA_STAT_AUTO_FLUSH_EN, PDP_F_WDMA_STAT_AUTO_FLUSH_EN, 0);
	PDP_SET_F(base, PDP_R_WDMA_STAT_DATA_FORMAT, PDP_F_WDMA_STAT_DATA_FORMAT_BAYER, format);
	PDP_SET_F(base, PDP_R_WDMA_STAT_WIDTH, PDP_F_WDMA_STAT_WIDTH, width);
	PDP_SET_F(base, PDP_R_WDMA_STAT_HEIGHT, PDP_F_WDMA_STAT_HEIGHT, height);
	PDP_SET_F(base, PDP_R_WDMA_STAT_IMG_STRIDE_1P, PDP_F_WDMA_STAT_IMG_STRIDE_1P, stride);

	info("[PDP%d] pdstat_in_size (x:%d, y:%d)", ch, roi_w, roi_h);
}

u32 pdp_hw_g_wdma_addr(void __iomem *base)
{
	/*
	 * Set Param Value
	 */
	return PDP_GET_R(base, PDP_R_WDMA_STAT_IMG_BASE_ADDR_1P_FRO0);
}

/*
 * Context: O
 * CR type: Corex
 */
void pdp_hw_s_wdma_enable(void __iomem *base, dma_addr_t address)
{
	/*
	 * Set Param Value
	 */
	PDP_SET_F(base, PDP_R_WDMA_STAT_IMG_BASE_ADDR_1P_FRO0, PDP_F_WDMA_STAT_IMG_BASE_ADDR_1P_FRO0, (u32)address);

	/*
	 * Set Fixed Value
	 */
	PDP_SET_F(base, PDP_R_WDMA_STAT_EN, PDP_F_WDMA_STAT_EN, 1);
}

void pdp_hw_s_wdma_disable(void __iomem *base)
{
	PDP_SET_F(base, PDP_R_WDMA_STAT_EN, PDP_F_WDMA_STAT_EN, 0);
}

/*
 * Context: O
 * CR type: Corex
 */
void pdp_hw_s_rdma_addr(void __iomem *base, dma_addr_t *address, u32 num_buffers, u32 direct,
	struct is_sensor_cfg *sensor_cfg)
{
	int i = 0;
	u32 width, heigth, byte_per_line, total_size, align_size;
	u32 offset = 0;

	if (IS_ENABLED(USE_IOVA_EXTENSION) &&
	    sensor_cfg && sensor_cfg->votf &&
	    sensor_cfg->img_pd_ratio == IS_IMG_PD_RATIO_1_1) {
		width = sensor_cfg->input[0].width;
		heigth = sensor_cfg->input[0].height;
		byte_per_line = ALIGN(width * 10 / BITS_PER_BYTE, 16);
		total_size = byte_per_line * heigth;
		align_size = ALIGN(total_size, SZ_4K);
		offset = align_size - total_size;
	}

	do {
		PDP_SET_R(base, PDP_R_RDMA_BAYER_IMG_BASE_ADDR_1P_FRO0 + i,
			(u32)address[i] + offset);
		if (direct)
			PDP_SET_R_DIRECT(base, PDP_R_RDMA_BAYER_IMG_BASE_ADDR_1P_FRO0 + i,
				(u32)address[i] + offset);
	} while (++i < num_buffers);
}

/*
 * Context: O
 * CR type: Corex
 */
void pdp_hw_update_rdma_linegap(void __iomem *base,
		struct is_sensor_cfg *sensor_cfg,
		struct is_pdp *pdp,
		u32 en_votf, u32 min_fps, u32 max_fps)
{
	u32 line_gap = PDP_RDMA_LINE_GAP; /* pixel per line */
	u32 ex_mode = sensor_cfg->ex_mode;
	u32 vvalid_time = sensor_cfg->vvalid_time;
	u32 framerate = sensor_cfg->framerate;
	u32 cl_margin, weight;
	u32 freq;
	u32 total_gap; /* us */
	u32 req_vvalid_time = sensor_cfg->req_vvalid_time;

	/*
	 * 'clk_hw_get_rate()' just returns the lately calculated value
	 * which is stored in its clk structure.
	 * To get the curretnly working clock rate, 'clk_get_rate()' should be called first.
	 */
	freq = is_get_hw_rate(pdp->dev, pdp->aclk_name);
	if (!freq) {
		warn("[HW][PDP%d] failed to get PDP clock", pdp->id);
		goto set_linegap;
	} else if (freq < OSC_CLOCK) {
		freq = MAX_CLOCK;
	}

	/* Put different CL margin for framerate */
	if (IS_DUALFPS_MODE(ex_mode)) {
		/* early config lock */
		framerate = 60;
		cl_margin = vvalid_time;
		weight = 1;
	} else {
		if (framerate > 30) {
			/* 66% vvalid margin */
			cl_margin = vvalid_time * 2 / 3;
			weight = 1;
		} else  {
			/* 33% vvalid margin */
			cl_margin = vvalid_time / 3;
			weight = 2; /* Heuristic value */
		}
	}

	/*
	 * fixed
	 * PDP_R_RDMA_LINE_GAP should be bigger than 3AA minimum line gap.
	 * 3AA line gap is 0x32.
	 */
	if (req_vvalid_time && (min_fps < max_fps)) {
		u32 ppc = 25; /* 2.5 ppc(Heuristic value) */

		vvalid_time = max(vvalid_time,
			sensor_cfg->width * sensor_cfg->height / (freq / MHZ) / ppc * 10);
		if (req_vvalid_time > vvalid_time) {
			total_gap = req_vvalid_time - vvalid_time;
			line_gap = (freq / MHZ) * (total_gap * weight) / sensor_cfg->height;
		}
	} else if (en_votf && NEED_TO_ADJUST_LINE_GAP(ex_mode, cl_margin)) {
		u32 f_duration = MHZ / framerate; /* us */

		total_gap = MIN_CONFIG_LOCK_TIME - cl_margin; /* us */
		if (f_duration > MIN_CONFIG_LOCK_TIME)
			line_gap = (freq / MHZ) * (total_gap * weight) / sensor_cfg->height;
	}

set_linegap:
	if (pdp->r_line_gap != line_gap) {
		info("[HW][PDP%d] add line gap %u px (freq %u MHz vvalid %u us, req %u us)\n",
				pdp->id, line_gap, freq / MHZ, vvalid_time, req_vvalid_time);

		PDP_SET_R(base, PDP_R_RDMA_BAYER_LINEGAP, line_gap);
		pdp->r_line_gap = line_gap;
	}
}

/*
 * Context: O
 * CR type: Corex
 */
void pdp_hw_s_af_rdma_addr(void __iomem *base, dma_addr_t *address, u32 num_buffers, u32 direct)
{
	int i = 0;

	do {
		PDP_SET_F(base, PDP_R_RDMA_AF_IMG_BASE_ADDR_1P_FRO0 + i, PDP_F_RDMA_AF_IMG_BASE_ADDR_1P_FRO0, (u32)address[i]);
		if (direct)
			PDP_SET_F_DIRECT(base, PDP_R_RDMA_AF_IMG_BASE_ADDR_1P_FRO0 + i,
				PDP_F_RDMA_AF_IMG_BASE_ADDR_1P_FRO0, (u32)address[i]);
	} while (++i < num_buffers);
}

/*
 * Context: O
 * CR type: Corex
 */
void pdp_hw_s_post_frame_gap(void __iomem *base, u32 interval)
{
	PDP_SET_R(base, PDP_R_IP_POST_FRAME_GAP, interval); /* increase V-blank interval */
}

/*
 * Context: O
 * CR type: No Corex
 */
void pdp_hw_s_fro(void __iomem *base, u32 num_buffers)
{
	u32 center_frame_num;

	if (num_buffers > 1) {
		PDP_SET_F(base, PDP_R_FRO_MODE_EN, PDP_F_FRO_MODE_EN, 1);
	} else {
		PDP_SET_F(base, PDP_R_FRO_MODE_EN, PDP_F_FRO_MODE_EN, 0);

		return;
	}

	center_frame_num = num_buffers / 2 - 1;

	/* Param: frame */
	PDP_SET_F(base, PDP_R_FRO_FRAME_COUNT_TO_RUN_MINUS1_SHADOW,
		PDP_F_FRO_FRAME_COUNT_TO_RUN_MINUS1_SHADOW, num_buffers - 1);
	PDP_SET_F(base, PDP_R_FRO_RUN_FRAME_NUMBER_FOR_COL_ROW_INT,
		PDP_F_FRO_RUN_FRAME_NUMBER_FOR_COL_ROW_INT, center_frame_num);
}

int pdp_hw_wait_idle(void __iomem *base, unsigned long state, u32 frame_time)
{
	int ret = 0;
	u32 total_line;
	u32 curr_line;
	u32 idle;
	u32 int1_all;
	u32 try_cnt = 0;
	u32 max_try_cnt = (frame_time > PDP_TRY_COUNT) ? frame_time : PDP_TRY_COUNT;

	total_line = PDP_GET_F(base, PDP_R_CINFIFO_OUTPUT_IMAGE_DIMENSIONS,
		PDP_F_CINFIFO_OUTPUT_IMAGE_HEIGHT);
	curr_line = PDP_GET_R(base, PDP_R_CINFIFO_OUTPUT_LINE_CNT);
	idle = PDP_GET_F(base, PDP_R_IDLENESS_STATUS, PDP_F_IDLENESS_STATUS);
	int1_all = PDP_GET_R(base, PDP_R_CONTINT_INT1);

	info_hw("[PDP] idle status before disable (total:%d, curr:%d, idle:%d, int1:0x%X)\n",
			total_line, curr_line, idle, int1_all);

	if (!test_bit(IS_PDP_DISABLE_REQ_IN_FS, &state))
		pdp_hw_s_global_enable(base, false);

	while (!idle) {
		idle = PDP_GET_F(base, PDP_R_IDLENESS_STATUS, PDP_F_IDLENESS_STATUS);

		try_cnt++;
		if (try_cnt >= max_try_cnt) {
			err_hw("[PDP] timeout waiting idle - disable fail, state=%x, frame_time=%d", state, frame_time);
			pdp_hw_dump(base);
			ret = -ETIME;
			break;
		}

		usleep_range(10, 11);
	};

	curr_line = PDP_GET_R(base, PDP_R_CINFIFO_OUTPUT_LINE_CNT);
	int1_all = PDP_GET_R(base, PDP_R_CONTINT_INT1);

	info_hw("[PDP] idle status after disable (total:%d, curr:%d, idle:%d, int1:0x%X)\n",
			total_line, curr_line, idle, int1_all);

	return ret;
}

int pdp_hw_rdma_wait_idle(void __iomem *base)
{
	int ret = 0;
	u32 busy_byr, busy_af;
	u32 try_cnt = 0;
	u32 max_try_cnt = PDP_TRY_COUNT;

	busy_byr = PDP_GET_R(base, PDP_R_RDMA_BAYER_MON_STATUS3);
	busy_af = PDP_GET_R(base, PDP_R_RDMA_AF_MON_STATUS3);
	while (busy_byr || busy_af) {
		busy_byr = PDP_GET_R(base, PDP_R_RDMA_BAYER_MON_STATUS3);
		busy_af = PDP_GET_R(base, PDP_R_RDMA_AF_MON_STATUS3);

		try_cnt++;
		if (try_cnt >= max_try_cnt) {
			err_hw("[PDP] timeout rdma waiting idle(byr:0x%x, af:0x%x)",
				busy_byr, busy_af);
			pdp_hw_dump(base);
			ret = -ETIME;
			break;
		}

		usleep_range(10, 11);
	};

	return ret;
}

/* sensor type */
void pdp_hw_s_sensor_type(void __iomem *base, u32 sensor_type)
{
}

bool pdp_hw_to_sensor_type(u32 pd_mode, u32 *sensor_type)
{
	bool enable;

	switch (pd_mode) {
	case PD_MSPD:
		*sensor_type = SENSOR_TYPE_MSPD;
		enable = true;
		break;
	case PD_MOD1:
		*sensor_type = SENSOR_TYPE_MOD1;
		enable = true;
		break;
	case PD_MOD2:
	case PD_NONE:
		*sensor_type = SENSOR_TYPE_MOD2;
		enable = false;
		break;
	case PD_MOD3:
		*sensor_type = SENSOR_TYPE_MOD3;
		enable = true;
		break;
	case PD_MSPD_TAIL:
		*sensor_type = SENSOR_TYPE_MSPD_TAIL;
		enable = true;
		break;
	default:
		warn("PD MODE(%d) is invalid", pd_mode);
		*sensor_type = SENSOR_TYPE_MOD2;
		enable = false;
		break;
	}

	return enable;
}

/* pattern generator */
void pdp_hw_strgen_enable(void __iomem *base)
{
}

void pdp_hw_strgen_disable(void __iomem *base)
{
}

/* IRQ function */
unsigned int pdp_hw_g_int1_state(void __iomem *base, bool clear, u32 *irq_state)
{
	u32 src_all, src_fro, src_err;

	/*
	 * src_all: per-frame based PDP IRQ status
	 * src_fro: FRO based PDP IRQ status
	 *
	 * final normal status: src_fro (start, line, end)
	 * final error status(src_err): src_all & ERR_MASK
	 */
	src_all = PDP_GET_R(base, PDP_R_CONTINT_INT1);
	src_fro = PDP_GET_R(base, PDP_R_FRO_INT0);

	if (clear) {
		PDP_SET_R(base, PDP_R_CONTINT_INT1_CLEAR, src_all);
		PDP_SET_R(base, PDP_R_FRO_INT0_CLEAR, src_fro);
	}

	src_err = src_all & INT1_ERR_MASK;

	*irq_state = src_all;

	return src_fro | src_err;
}

unsigned int pdp_hw_g_int1_mask(void __iomem *base)
{
	return PDP_GET_R(base, PDP_R_CONTINT_INT1_ENABLE);
}

unsigned int pdp_hw_g_int2_state(void __iomem *base, bool clear, u32 *irq_state)
{
	u32 src_all, src;

	src_all = PDP_GET_R(base, PDP_R_CONTINT_INT2);

	src = PDP_GET_R(base, PDP_R_CONTINT_INT2_STATUS);

	if (clear)
		PDP_SET_R(base, PDP_R_CONTINT_INT2_CLEAR, src);

	*irq_state = src_all;

	return src;
}

unsigned int pdp_hw_g_int2_rdma_state(void __iomem *base, bool clear)
{
	/* PDP v3.0/v3.1 not exist RDMA state Register */
	return 0;
}

unsigned int pdp_hw_g_int2_mask(void __iomem *base)
{
	return PDP_GET_R(base, PDP_R_CONTINT_INT2_ENABLE);
}

unsigned int pdp_hw_is_occurred(unsigned int state, enum pdp_event_type type)
{
	u32 mask;

	switch (type) {
	case PE_START:
		mask = (1 << FRAME_INT_ON_ROW_COL_INFO);
		break;
	case PE_END:
		mask = 1 << FRAME_END_INTERRUPT;
		break;
	case PE_PAF_STAT0:
		mask = 1 << PDAF_STAT_INT;
		break;
	case PE_PAF_STAT1:
		mask = 1 << FRAME_END_INTERRUPT;
		break;
	case PE_PAF_STAT2:
		return 0;
	case PE_ERR_INT1:
		mask = INT1_ERR_MASK;
		break;
	case PE_PAF_OVERFLOW:
		mask = (1 << COUTFIFO_OVERFLOW_ERROR) | (1 << CINFIFO_STREAM_OVERFLOW);
		break;
	case PE_ERR_INT2:
		mask = INT2_ERR_MASK;
#ifdef CSIS_VOTF_EARLY_TERMINATION_LINE
		/*
		 * When CSIS DMA termiates VOTF connection earlier than PDP,
		 * it always asserts lost connection error.
		 * It should be ignored for preventing error log storm.
		 */
		mask &= ~((1 << VOTF_LOST_CON_IMG) | (1 << VOTF_LOST_CON_AF));
#endif
		break;
	case PE_SPECIAL:
		mask = (1 << FRAME_START);
		break;
	default:
		return 0;
	}

	return state & mask;
}

void pdp_hw_g_int1_str(const char **int_str)
{
	int i;

	for (i = 0; i < PDP_INT1_CNT; i++)
		int_str[i] = pdp_int1_str[i];
}

void pdp_hw_g_int2_str(const char **int_str)
{
	int i;

	for (i = 0; i < PDP_INT2_CNT; i++)
		int_str[i] = pdp_int2_str[i];
}

void pdp_hw_g_int2_rdma_str(const char **int_str)
{
	/* PDP v3.0/v3.1 not exist RDMA state Register */
}

/* Debug function */
void pdp_hw_s_config_default(void __iomem *base)
{
	int i;
	u32 index;
	u32 value;
	u32 corex_enable;
	int count = ARRAY_SIZE(pdp_global_init_table);

	corex_enable = PDP_GET_F(base, PDP_R_COREX_ENABLE, PDP_F_COREX_ENABLE);
	PDP_SET_F(base, PDP_R_COREX_ENABLE, PDP_F_COREX_ENABLE, 0);

	for (i = 0; i < count; i++) {
		index = pdp_global_init_table[i].index;
		value = pdp_global_init_table[i].init_value;

		writel(value, base + index);
	}

	PDP_SET_F(base, PDP_R_COREX_ENABLE, PDP_F_COREX_ENABLE, corex_enable);
}

int pdp_hw_dump(void __iomem *base)
{
	info_hw("[PDP] SFR DUMP (v3.0)\n");

	is_hw_dump_regs(base, pdp_regs, PDP_REG_CNT);

	return 0;
}
