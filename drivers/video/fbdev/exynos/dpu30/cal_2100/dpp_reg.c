// SPDX-License-Identifier: GPL-2.0-only
/*
 * dpu30/cal_2100/dpp_regs.c
 *
 * Copyright (c) 2019 Samsung Electronics Co., Ltd.
 *		http://www.samsung.com
 *
 * Jaehoe Yang <jaehoe.yang@samsung.com>
 * Jiun Yu <jiun.yu@samsung.com>
 *
 * Register access functions for Samsung EXYNOS Display Pre-Processor driver
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
#if defined(CONFIG_EXYNOS_HDR_TUNABLE_TONEMAPPING)
#include <video/exynos_hdr_tunables.h>
#endif

#include "../dpp.h"
#include "../dpp_coef.h"
#include "../hdr_lut.h"
#include "../format.h"

#define DPP_SC_RATIO_MAX	((1 << 20) * 8 / 8)
#define DPP_SC_RATIO_7_8	((1 << 20) * 8 / 7)
#define DPP_SC_RATIO_6_8	((1 << 20) * 8 / 6)
#define DPP_SC_RATIO_5_8	((1 << 20) * 8 / 5)
#define DPP_SC_RATIO_4_8	((1 << 20) * 8 / 4)
#define DPP_SC_RATIO_3_8	((1 << 20) * 8 / 3)

/****************** IDMA CAL functions ******************/
static void idma_reg_set_irq_mask_all(u32 id, u32 en)
{
	u32 val = en ? ~0 : 0;

	dma_write_mask(id, RDMA_IRQ, val, IDMA_ALL_IRQ_MASK);
}

static void idma_reg_set_irq_enable(u32 id)
{
	dma_write_mask(id, RDMA_IRQ, ~0, IDMA_IRQ_ENABLE);
}

static void idma_reg_set_irq_disable(u32 id)
{
	dma_write_mask(id, RDMA_IRQ, 0, IDMA_IRQ_ENABLE);
}

static void idma_reg_set_in_qos_lut(u32 id, u32 lut_id, u32 qos_t)
{
	u32 reg_id;

	if (lut_id == 0)
		reg_id = RDMA_QOS_LUT_LOW;
	else
		reg_id = RDMA_QOS_LUT_HIGH;
	dma_write(id, reg_id, qos_t);
}

static void idma_reg_set_sram_clk_gate_en(u32 id, u32 en)
{
	u32 val = en ? ~0 : 0;

	dma_write_mask(id, RDMA_DYNAMIC_GATING_EN, val, IDMA_SRAM_CG_EN);
}

static void idma_reg_set_dynamic_gating_en_all(u32 id, u32 en)
{
	u32 val = en ? ~0 : 0;

	dma_write_mask(id, RDMA_DYNAMIC_GATING_EN, val, IDMA_DG_EN_ALL);
}

static void idma_reg_clear_irq(u32 id, u32 irq)
{
	dma_write_mask(id, RDMA_IRQ, ~0, irq);
}

static void idma_reg_set_sw_reset(u32 id)
{
	dma_write_mask(id, RDMA_ENABLE, ~0, IDMA_SRESET);
}

static void idma_reg_set_assigned_mo(u32 id, u32 mo)
{
	dma_write_mask(id, RDMA_ENABLE, IDMA_ASSIGNED_MO(mo),
			IDMA_ASSIGNED_MO_MASK);
}

static int idma_reg_wait_sw_reset_status(u32 id)
{
	struct dpp_device *dpp = get_dpp_drvdata(id);
	u32 val;
	int ret;

	ret = readl_poll_timeout(dpp->res.dma_regs + RDMA_ENABLE,
			val, !(val & IDMA_SRESET), 10, 2000); /* timeout 2ms */
	if (ret) {
		dpp_err("[idma%d] timeout sw-reset\n", id);
		return ret;
	}

	return 0;
}

static void idma_reg_set_coordinates(u32 id, struct decon_frame *src)
{
	dma_write(id, RDMA_SRC_OFFSET,
			IDMA_SRC_OFFSET_Y(src->y) | IDMA_SRC_OFFSET_X(src->x));
	dma_write(id, RDMA_SRC_SIZE,
			IDMA_SRC_HEIGHT(src->f_h) | IDMA_SRC_WIDTH(src->f_w));
	dma_write(id, RDMA_IMG_SIZE,
			IDMA_IMG_HEIGHT(src->h) | IDMA_IMG_WIDTH(src->w));
}

#if defined(CONFIG_EXYNOS_SUPPORT_8K_SPLIT)
static void idma_reg_set_split_en(u32 id, enum dpp_split_en spl_en)
{
	int en;
	int split_lr = 0;

	if (spl_en == DPP_SPLIT_OFF)
		en = 0;
	else {
		en = ~0;
		split_lr = (spl_en == DPP_SPLIT_LEFT) ? 0 : 1;
	}
	dma_write_mask(id, RDMA_IN_CTRL_1, en, IDMA_SPLIT_8K_EN);
	/* left half is 0, right half is 1 */
	dma_write_mask(id, RDMA_IN_CTRL_1,
		IDMA_SPLIT_8K_LR(split_lr), IDMA_SPLIT_8K_LR_MASK);
	dpp_dbg("dpp%d: split en(%d) split_lr(%d)\n",
			id, (en & IDMA_SPLIT_8K_EN), split_lr);
}
#endif

static void idma_reg_set_frame_alpha(u32 id, u32 alpha)
{
	dma_write_mask(id, RDMA_IN_CTRL_0, IDMA_ALPHA(alpha),
			IDMA_ALPHA_MASK);
}

static void idma_reg_set_ic_max(u32 id, u32 ic_max)
{
	dma_write_mask(id, RDMA_IN_CTRL_0, IDMA_IC_MAX(ic_max),
			IDMA_IC_MAX_MASK);
}

static void idma_reg_set_rotation(u32 id, u32 rot)
{
	dma_write_mask(id, RDMA_IN_CTRL_0, IDMA_ROT(rot), IDMA_ROT_MASK);
}

static void idma_reg_set_block_mode(u32 id, bool en, int x, int y, u32 w, u32 h)
{
	if (!en) {
		dma_write_mask(id, RDMA_IN_CTRL_0, 0, IDMA_BLOCK_EN);
		return;
	}

	dma_write(id, RDMA_BLOCK_OFFSET,
			IDMA_BLK_OFFSET_Y(y) | IDMA_BLK_OFFSET_X(x));
	dma_write(id, RDMA_BLOCK_SIZE, IDMA_BLK_HEIGHT(h) | IDMA_BLK_WIDTH(w));
	dma_write_mask(id, RDMA_IN_CTRL_0, ~0, IDMA_BLOCK_EN);

	dpp_dbg("dpp%d: block x(%d) y(%d) w(%d) h(%d)\n", id, x, y, w, h);
}

static void idma_reg_set_format(u32 id, u32 fmt)
{
	dma_write_mask(id, RDMA_IN_CTRL_0, IDMA_IMG_FORMAT(fmt),
			IDMA_IMG_FORMAT_MASK);
}

#if defined(DMA_BIST)
static void idma_reg_set_test_pattern(u32 id, u32 pat_id, u32 *pat_dat)
{
	u32 map_tlb[6] = {0, 0, 2, 2, 4, 4};
	u32 new_id;

	/* 0=AXI, 3=PAT */
	dma_write_mask(id, RDMA_IN_REQ_DEST, ~0, IDMA_REQ_DEST_SEL_MASK);

	new_id = map_tlb[id];
	if (pat_id == 0) {
		dma_write(new_id, GLB_TEST_PATTERN0_0, pat_dat[0]);
		dma_write(new_id, GLB_TEST_PATTERN0_1, pat_dat[1]);
		dma_write(new_id, GLB_TEST_PATTERN0_2, pat_dat[2]);
		dma_write(new_id, GLB_TEST_PATTERN0_3, pat_dat[3]);
	} else {
		dma_write(new_id, GLB_TEST_PATTERN1_0, pat_dat[4]);
		dma_write(new_id, GLB_TEST_PATTERN1_1, pat_dat[5]);
		dma_write(new_id, GLB_TEST_PATTERN1_2, pat_dat[6]);
		dma_write(new_id, GLB_TEST_PATTERN1_3, pat_dat[7]);
	}
}
#endif

static void idma_reg_set_afbc(u32 id, enum dpp_comp_type ct, u32 rcv_num)
{
	u32 afbc_en = 0;
	u32 rcv_en = 0;

	if (ct == COMP_TYPE_AFBC)
		afbc_en = IDMA_AFBC_EN;
	if (ct != COMP_TYPE_NONE)
		rcv_en = IDMA_RECOVERY_EN;

	dma_write_mask(id, RDMA_IN_CTRL_0, afbc_en, IDMA_AFBC_EN);
	dma_write_mask(id, RDMA_RECOVERY_CTRL, rcv_en, IDMA_RECOVERY_EN);
	dma_write_mask(id, RDMA_RECOVERY_CTRL, IDMA_RECOVERY_NUM(rcv_num),
				IDMA_RECOVERY_NUM_MASK);
}

static void idma_reg_set_sbwc(u32 id, enum dpp_comp_type ct, u32 size_32x, u32 rcv_num)
{
	u32 sbwc_en = 0;
	u32 rcv_en = 0;

	if (ct == COMP_TYPE_SBWC)
		sbwc_en = IDMA_SBWC_EN;
	if (ct != COMP_TYPE_NONE)
		rcv_en = IDMA_RECOVERY_EN;

	dma_write_mask(id, RDMA_IN_CTRL_0, sbwc_en, IDMA_SBWC_EN);
	dma_write_mask(id, RDMA_SBWC_PARAM,
			IDMA_CHM_BLK_BYTENUM(size_32x) | IDMA_LUM_BLK_BYTENUM(size_32x),
			IDMA_CHM_BLK_BYTENUM_MASK | IDMA_LUM_BLK_BYTENUM_MASK);
	dma_write_mask(id, RDMA_RECOVERY_CTRL, rcv_en, IDMA_RECOVERY_EN);
	dma_write_mask(id, RDMA_RECOVERY_CTRL, IDMA_RECOVERY_NUM(rcv_num),
				IDMA_RECOVERY_NUM_MASK);
}

static void idma_reg_set_sbwcl(u32 id, enum dpp_comp_type ct, u32 size_32x, u32 rcv_num)
{
	u32 sbwc_en = 0;
	u32 rcv_en = 0;

	if (ct == COMP_TYPE_SBWCL)
		sbwc_en = IDMA_SBWC_LOSSY | IDMA_SBWC_EN;
	if (ct != COMP_TYPE_NONE)
		rcv_en = IDMA_RECOVERY_EN;

	dma_write_mask(id, RDMA_IN_CTRL_0, sbwc_en, IDMA_SBWC_LOSSY | IDMA_SBWC_EN);
	dma_write_mask(id, RDMA_SBWC_PARAM,
			IDMA_CHM_BLK_BYTENUM(size_32x) | IDMA_LUM_BLK_BYTENUM(size_32x),
			IDMA_CHM_BLK_BYTENUM_MASK | IDMA_LUM_BLK_BYTENUM_MASK);
	dma_write_mask(id, RDMA_RECOVERY_CTRL, rcv_en, IDMA_RECOVERY_EN);
	dma_write_mask(id, RDMA_RECOVERY_CTRL, IDMA_RECOVERY_NUM(rcv_num),
				IDMA_RECOVERY_NUM_MASK);
}

static void idma_reg_set_deadlock(u32 id, u32 en, u32 dl_num)
{
	u32 val = en ? ~0 : 0;

	dma_write_mask(id, RDMA_DEADLOCK_CTRL, val, IDMA_DEADLOCK_NUM_EN);
	dma_write_mask(id, RDMA_DEADLOCK_CTRL, IDMA_DEADLOCK_NUM(dl_num),
				IDMA_DEADLOCK_NUM_MASK);
}

#if defined(CONFIG_EXYNOS_SUPPORT_VOTF_IN)
static void idma_reg_set_votf_init(void)
{
	struct dpp_device *dpp = NULL;
	u32 votf_idx = 0;
	u32 offset_val = 0;
	int i = 0;

	if (votf_read(0, VOTF_RING_CLK_ENABLE) == 0) {
		for (i = 0; i < 16;  i++) {
			dpp = get_dpp_drvdata(i);

			votf_idx = i % DPUF1_FIRST_DPP_ID;

			votf_write(i, VOTF_RING_CLK_ENABLE, 1);
			votf_write(i, VOTF_RING_ENABLE, 1);
			votf_write(i, VOTF_LOCAL_IP, dpp->res.votf_base_addr >> 16);
			votf_write(i, VOTF_REGISTER_MODE, 1);
			votf_write(i, VOTF_SEL_REGISTER, 1);

			if (votf_idx < 5)
				offset_val = VOTF_OFFSET_0_4(votf_idx);
			else
				offset_val = VOTF_OFFSET_5_8(votf_idx % 5);

			votf_write(i, VOTF_TRS_ENABLE + offset_val, 1);
			votf_write(i, VOTF_CON_LOST_RECOVER_CONFIG + offset_val,
					VOTF_CON_LOST_FLUSH_EN | VOTF_CON_LOST_RECOVER_EN);
			votf_write(i, VOTF_TRS_LIMIT + offset_val, 0xFF);
			votf_write(i, VOTF_TOKEN_CROP_START + offset_val, 0);
			votf_write(i, VOTF_CROP_ENABLE + offset_val, 0);
			votf_write(i, VOTF_LINE_IN_FIRST_TOKEN + offset_val, 0x01);
			votf_write(i, VOTF_LINE_IN_TOKEN + offset_val, 0x01);
			votf_write(i, VOTF_LINE_COUNT + offset_val, 0x3FFF);
		}
	}
}

static void idma_reg_set_votf(u32 id, u32 en, u32 buf_idx, u32 rcv_num)
{
	if (en) {
		dma_write_mask(id, RDMA_RECOVERY_CTRL, 1, IDMA_RECOVERY_EN);
		dma_write_mask(id, RDMA_RECOVERY_CTRL, IDMA_RECOVERY_NUM(rcv_num),
				IDMA_RECOVERY_NUM_MASK);

		dma_write_mask(id, RDMA_VOTF_EN,
				IDMA_VOTF_SLV_IDX(buf_idx),
				IDMA_VOTF_SLV_IDX_MASK);

		dma_write_mask(id, RDMA_VOTF_EN, IDMA_VOTF_SLV_MODE_EN, IDMA_VOTF_EN);
	} else
		dma_write_mask(id, RDMA_VOTF_EN, 0, IDMA_VOTF_EN);
}
#endif

/****************** ODMA CAL functions ******************/
static void odma_reg_set_irq_mask_all(u32 id, u32 en)
{
	u32 val = en ? ~0 : 0;

	dma_write_mask(id, WDMA_IRQ, val, ODMA_ALL_IRQ_MASK);
}

static void odma_reg_set_irq_enable(u32 id)
{
	dma_write_mask(id, WDMA_IRQ, ~0, ODMA_IRQ_ENABLE);
}

static void odma_reg_set_irq_disable(u32 id)
{
	dma_write_mask(id, WDMA_IRQ, 0, ODMA_IRQ_ENABLE);
}

static void odma_reg_set_in_qos_lut(u32 id, u32 lut_id, u32 qos_t)
{
	u32 reg_id;

	if (lut_id == 0)
		reg_id = WDMA_QOS_LUT_LOW;
	else
		reg_id = WDMA_QOS_LUT_HIGH;
	dma_write(id, reg_id, qos_t);
}

static void odma_reg_set_dynamic_gating_en_all(u32 id, u32 en)
{
	u32 val = en ? ~0 : 0;

	dma_write_mask(id, WDMA_DYNAMIC_GATING_EN, val, ODMA_DG_EN_ALL);
}

static void odma_reg_set_frame_alpha(u32 id, u32 alpha)
{
	dma_write_mask(id, WDMA_OUT_CTRL_0, ODMA_ALPHA(alpha),
			ODMA_ALPHA_MASK);
}

static void odma_reg_clear_irq(u32 id, u32 irq)
{
	dma_write_mask(id, WDMA_IRQ, ~0, irq);
}

static void odma_reg_set_sw_reset(u32 id)
{
	dma_write_mask(id, WDMA_ENABLE, ~0, ODMA_SRESET);
}

static int odma_reg_wait_sw_reset_status(u32 id)
{
	struct dpp_device *dpp = get_dpp_drvdata(id);
	u32 val;
	int ret;

	ret = readl_poll_timeout(dpp->res.dma_regs + WDMA_ENABLE,
			val, !(val & ODMA_SRESET), 10, 2000); /* timeout 2ms */
	if (ret) {
		dpp_err("[odma%d] timeout sw-reset\n", id);
		return ret;
	}

	return 0;
}

static void odma_reg_set_coordinates(u32 id, struct decon_frame *dst)
{
	dma_write(id, WDMA_DST_OFFSET,
			ODMA_DST_OFFSET_Y(dst->y) | ODMA_DST_OFFSET_X(dst->x));
	dma_write(id, WDMA_DST_SIZE,
			ODMA_DST_HEIGHT(dst->f_h) | ODMA_DST_WIDTH(dst->f_w));
	dma_write(id, WDMA_IMG_SIZE,
			ODMA_IMG_HEIGHT(dst->h) | ODMA_IMG_WIDTH(dst->w));
}

static void odma_reg_set_ic_max(u32 id, u32 ic_max)
{
	dma_write_mask(id, WDMA_OUT_CTRL_0, ODMA_IC_MAX(ic_max),
			ODMA_IC_MAX_MASK);
}

static void odma_reg_set_format(u32 id, u32 fmt)
{
	dma_write_mask(id, WDMA_OUT_CTRL_0, ODMA_IMG_FORMAT(fmt),
			ODMA_IMG_FORMAT_MASK);
}

static void odma_reg_set_sub_con(u32 id)
{
	u32 val, mask;

	val = DPP_UV_OFFSET_X(0x2) | DPP_UV_OFFSET_Y(0x2);
	mask = DPP_UV_OFFSET_Y_MASK | DPP_UV_OFFSET_X_MASK;

	dpp_write_mask(id, DPP_COM_SUB_CON, val, mask);
	dpp_dbg("Sub sampling 0x%08x\n", dpp_read(id, DPP_COM_SUB_CON));
}

/****************** DPP CAL functions ******************/
static void dpp_reg_set_irq_mask_all(u32 id, u32 en)
{
	u32 val = en ? ~0 : 0;

	dpp_write_mask(id, DPP_COM_IRQ_MASK, val, DPP_ALL_IRQ_MASK);
}

static void dpp_reg_set_irq_enable(u32 id)
{
	dpp_write_mask(id, DPP_COM_IRQ_CON, ~0, DPP_IRQ_EN);
}

static void dpp_reg_set_irq_disable(u32 id)
{
	dpp_write_mask(id, DPP_COM_IRQ_CON, 0, DPP_IRQ_EN);
}

static void dpp_reg_set_linecnt(u32 id, u32 en)
{
	if (en)
		dpp_write_mask(id, DPP_COM_LC_CON,
				DPP_LC_MODE(0) | DPP_LC_EN(1),
				DPP_LC_MODE_MASK | DPP_LC_EN_MASK);
	else
		dpp_write_mask(id, DPP_COM_LC_CON, DPP_LC_EN(0),
				DPP_LC_EN_MASK);
}

static void dpp_reg_clear_irq(u32 id, u32 irq)
{
	dpp_write_mask(id, DPP_COM_IRQ_STATUS, ~0, irq);
}

static void dpp_reg_set_sw_reset(u32 id)
{
	dpp_write_mask(id, DPP_COM_SWRST_CON, ~0, DPP_SRESET);
}

static int dpp_reg_wait_sw_reset_status(u32 id)
{
	struct dpp_device *dpp = get_dpp_drvdata(id);
	u32 val;
	int ret;

	ret = readl_poll_timeout(dpp->res.regs + DPP_COM_SWRST_CON,
			val, !(val & DPP_SRESET), 10, 2000); /* timeout 2ms */
	if (ret) {
		dpp_err("[dpp%d] timeout sw-reset\n", id);
		return ret;
	}

	return 0;
}

static void dpp_reg_set_csc_coef(u32 id, u32 csc_std, u32 csc_rng,
		const unsigned long attr)
{
	u32 val, mask;
	u32 csc_id = CSC_CUSTOMIZED_START; /* CSC_BT601/625/525 */
	u32 c00, c01, c02;
	u32 c10, c11, c12;
	u32 c20, c21, c22;
	const u16 (*csc_arr)[3][3];

	if (test_bit(DPP_ATTR_ODMA, &attr))
		csc_arr = csc_r2y_3x3_t;
	else
		csc_arr = csc_y2r_3x3_t;

	if ((csc_std > CSC_DCI_P3) && (csc_std <= CSC_DCI_P3_C))
		csc_id = (csc_std - CSC_CUSTOMIZED_START) * 2 + csc_rng;
	else
		dpp_dbg("[DPP%d] Undefined CSC Type!(std=%d)\n", id, csc_std);

	c00 = csc_arr[csc_id][0][0];
	c01 = csc_arr[csc_id][0][1];
	c02 = csc_arr[csc_id][0][2];

	c10 = csc_arr[csc_id][1][0];
	c11 = csc_arr[csc_id][1][1];
	c12 = csc_arr[csc_id][1][2];

	c20 = csc_arr[csc_id][2][0];
	c21 = csc_arr[csc_id][2][1];
	c22 = csc_arr[csc_id][2][2];

	mask = (DPP_CSC_COEF_H_MASK | DPP_CSC_COEF_L_MASK);
	val = (DPP_CSC_COEF_H(c01) | DPP_CSC_COEF_L(c00));
	dpp_write_mask(id, DPP_COM_CSC_COEF0, val, mask);

	val = (DPP_CSC_COEF_H(c10) | DPP_CSC_COEF_L(c02));
	dpp_write_mask(id, DPP_COM_CSC_COEF1, val, mask);

	val = (DPP_CSC_COEF_H(c12) | DPP_CSC_COEF_L(c11));
	dpp_write_mask(id, DPP_COM_CSC_COEF2, val, mask);

	val = (DPP_CSC_COEF_H(c21) | DPP_CSC_COEF_L(c20));
	dpp_write_mask(id, DPP_COM_CSC_COEF3, val, mask);

	mask = DPP_CSC_COEF_L_MASK;
	val = DPP_CSC_COEF_L(c22);
	dpp_write_mask(id, DPP_COM_CSC_COEF4, val, mask);

	dpp_dbg("---[DPP%d Y2R CSC Type: std=%d, rng=%d]---\n",
		id, csc_std, csc_rng);
	dpp_dbg("0x%4x  0x%4x  0x%4x\n", c00, c01, c02);
	dpp_dbg("0x%4x  0x%4x  0x%4x\n", c10, c11, c12);
	dpp_dbg("0x%4x  0x%4x  0x%4x\n", c20, c21, c22);
}

static void dpp_reg_set_csc_params(u32 id, u32 csc_eq,
		const unsigned long attr)
{
	u32 type = (csc_eq >> CSC_STANDARD_SHIFT) & 0x3F;
	u32 range = (csc_eq >> CSC_RANGE_SHIFT) & 0x7;
	u32 mode = (type <= CSC_DCI_P3) ?
		CSC_COEF_HARDWIRED : CSC_COEF_CUSTOMIZED;
	u32 val, mask;

	if (type == CSC_STANDARD_UNSPECIFIED) {
		dpp_dbg("unspecified CSC type! -> BT_601\n");
		type = CSC_BT_601;
		mode = CSC_COEF_HARDWIRED;
	}
	if (range == CSC_RANGE_UNSPECIFIED) {
		dpp_dbg("unspecified CSC range! -> LIMITED\n");
		range = CSC_RANGE_LIMITED;
	}

	if (test_bit(DPP_ATTR_ODMA, &attr))
		mode = CSC_COEF_CUSTOMIZED;

	val = (DPP_CSC_TYPE(type) | DPP_CSC_RANGE(range) | DPP_CSC_MODE(mode));
	mask = (DPP_CSC_TYPE_MASK | DPP_CSC_RANGE_MASK | DPP_CSC_MODE_MASK);
	dpp_write_mask(id, DPP_COM_CSC_CON, val, mask);

	if (mode == CSC_COEF_CUSTOMIZED)
		dpp_reg_set_csc_coef(id, type, range, attr);

	dpp_dbg("DPP%d CSC info: type=%d, range=%d, mode=%d\n",
		id, type, range, mode);
}

static void dpp_reg_set_h_coef(u32 id, u32 h_ratio)
{
	int i, j, k, sc_ratio;

	if (h_ratio <= DPP_SC_RATIO_MAX)
		sc_ratio = 0;
	else if (h_ratio <= DPP_SC_RATIO_7_8)
		sc_ratio = 1;
	else if (h_ratio <= DPP_SC_RATIO_6_8)
		sc_ratio = 2;
	else if (h_ratio <= DPP_SC_RATIO_5_8)
		sc_ratio = 3;
	else if (h_ratio <= DPP_SC_RATIO_4_8)
		sc_ratio = 4;
	else if (h_ratio <= DPP_SC_RATIO_3_8)
		sc_ratio = 5;
	else
		sc_ratio = 6;

	for (i = 0; i < 9; i++)
		for (j = 0; j < 8; j++)
			for (k = 0; k < 2; k++)
				dpp_write_relaxed(id, DPP_H_COEF(i, j, k),
						h_coef_8t[sc_ratio][i][j]);
}

static void dpp_reg_set_v_coef(u32 id, u32 v_ratio)
{
	int i, j, k, sc_ratio;

	if (v_ratio <= DPP_SC_RATIO_MAX)
		sc_ratio = 0;
	else if (v_ratio <= DPP_SC_RATIO_7_8)
		sc_ratio = 1;
	else if (v_ratio <= DPP_SC_RATIO_6_8)
		sc_ratio = 2;
	else if (v_ratio <= DPP_SC_RATIO_5_8)
		sc_ratio = 3;
	else if (v_ratio <= DPP_SC_RATIO_4_8)
		sc_ratio = 4;
	else if (v_ratio <= DPP_SC_RATIO_3_8)
		sc_ratio = 5;
	else
		sc_ratio = 6;

	for (i = 0; i < 9; i++)
		for (j = 0; j < 4; j++)
			for (k = 0; k < 2; k++)
				dpp_write_relaxed(id, DPP_V_COEF(i, j, k),
						v_coef_4t[sc_ratio][i][j]);
}

static void dpp_reg_set_scale_ratio(u32 id, struct dpp_params_info *p)
{
	u32 prev_h_ratio, prev_v_ratio;

	prev_h_ratio = dpp_read_mask(id, DPP_SCL_MAIN_H_RATIO, DPP_H_RATIO_MASK);
	prev_v_ratio = dpp_read_mask(id, DPP_SCL_MAIN_V_RATIO, DPP_V_RATIO_MASK);

	if (prev_h_ratio != p->h_ratio) {
		dpp_write(id, DPP_SCL_MAIN_H_RATIO, DPP_H_RATIO(p->h_ratio));
		dpp_reg_set_h_coef(id, p->h_ratio);
	}

	if (prev_v_ratio != p->v_ratio) {
		dpp_write(id, DPP_SCL_MAIN_V_RATIO, DPP_V_RATIO(p->v_ratio));
		dpp_reg_set_v_coef(id, p->v_ratio);
	}

	dpp_dbg("h_ratio[%#x:%#x], v_ratio : [%#x:%#x]\n",
			prev_h_ratio, p->h_ratio, prev_v_ratio, p->v_ratio);
}

static void dpp_reg_set_scl_pos(u32 id, struct dpp_params_info *p)
{
#if defined(CONFIG_EXYNOS_SUPPORT_8K_SPLIT)
	const struct dpu_fmt *fmt_info = dpu_find_fmt_info(p->format);
#endif
	u32 padd = 0;

	/* Initialize setting of initial phase */
	dpp_write_mask(id, DPP_SCL_YVPOSITION, DPP_SCL_YVPOS_I(padd),
			DPP_SCL_YVPOS_I_MASK);
	dpp_write_mask(id, DPP_SCL_CVPOSITION, DPP_SCL_CVPOS_I(padd),
			DPP_SCL_CVPOS_I_MASK);
	dpp_write_mask(id, DPP_SCL_YHPOSITION, DPP_SCL_YHPOS_I(padd),
			DPP_SCL_YHPOS_I_MASK);
	dpp_write_mask(id, DPP_SCL_CHPOSITION, DPP_SCL_CHPOS_I(padd),
			DPP_SCL_CHPOS_I_MASK);

#if defined(CONFIG_EXYNOS_SUPPORT_8K_SPLIT)
	if (p->aux_src.spl_en == DPP_SPLIT_OFF ||
		p->aux_src.spl_en == DPP_SPLIT_LEFT)
		return;

	if (p->aux_src.padd_w > 0 && p->aux_src.padd_h == 0)
		padd = p->aux_src.padd_w;
	else if (p->aux_src.padd_w == 0 && p->aux_src.padd_h > 0)
		padd = p->aux_src.padd_h;
	else
		padd = 0;

	if (IS_YUV(fmt_info)) {
		dpp_write_mask(id, DPP_SCL_YHPOSITION, DPP_SCL_YHPOS_I(padd),
				DPP_SCL_YHPOS_I_MASK);
		dpp_write_mask(id, DPP_SCL_CHPOSITION, DPP_SCL_CHPOS_I(padd),
				DPP_SCL_CHPOS_I_MASK);
	}
	else {
		if (p->aux_src.spl_drtn == DPP_SPLIT_VERTICAL)
			dpp_write_mask(id, DPP_SCL_YVPOSITION, DPP_SCL_YHPOS_I(padd),
					DPP_SCL_YVPOS_I_MASK);
		else
			dpp_err("%s: not support horizontal split for RGB format\n",
					__func__);
	}
#endif
}

static void dpp_reg_set_img_size(u32 id, u32 w, u32 h)
{
	dpp_write(id, DPP_COM_IMG_SIZE, DPP_IMG_HEIGHT(h) | DPP_IMG_WIDTH(w));
}

static void dpp_reg_set_scaled_img_size(u32 id, u32 w, u32 h)
{
	dpp_write(id, DPP_SCL_SCALED_IMG_SIZE,
			DPP_SCALED_IMG_HEIGHT(h) | DPP_SCALED_IMG_WIDTH(w));
}

#ifndef CONFIG_MCDHDR
static void dpp_reg_set_eotf_lut(u32 id, struct dpp_params_info *p)
{
	u32 i = 0;
	u32 *lut_x = NULL;
	u32 *lut_y = NULL;

	if (p->hdr == DPP_HDR_ST2084) {
		if (p->max_luminance > 1000) {
			lut_x = eotf_x_axis_st2084_4000;
			lut_y = eotf_y_axis_st2084_4000;
		} else {
			lut_x = eotf_x_axis_st2084_1000;
			lut_y = eotf_y_axis_st2084_1000;
		}
	} else if (p->hdr == DPP_HDR_HLG) {
		lut_x = eotf_x_axis_hlg;
		lut_y = eotf_y_axis_hlg;
	} else {
		dpp_err("Undefined HDR standard Type!!!\n");
		return;
	}

	for (i = 0; i < MAX_EOTF; i++) {
		dpp_write_mask(id,
			DPP_HDR_EOTF_X_AXIS_ADDR(i),
			DPP_HDR_EOTF_X_AXIS_VAL(i, lut_x[i]),
			DPP_HDR_EOTF_MASK(i));
		dpp_write_mask(id,
			DPP_HDR_EOTF_Y_AXIS_ADDR(i),
			DPP_HDR_EOTF_Y_AXIS_VAL(i, lut_y[i]),
			DPP_HDR_EOTF_MASK(i));
	}
}

static void dpp_reg_set_gm_lut(u32 id, struct dpp_params_info *p)
{
	u32 i = 0;
	u32 *lut_gm = NULL;

	if (p->eq_mode == CSC_BT_2020) {
		lut_gm = gm_coef_2020_p3;
	} else {
		dpp_err("Undefined HDR CSC Type!!!\n");
		return;
	}

	for (i = 0; i < MAX_GM; i++) {
		dpp_write_mask(id,
			DPP_HDR_GM_COEF_ADDR(i),
			lut_gm[i],
			DPP_HDR_GM_COEF_MASK);
	}
}

static void dpp_reg_set_tm_lut(u32 id, struct dpp_params_info *p)
{
	u32 i = 0;
	u32 *lut_x = NULL;
	u32 *lut_y = NULL;

	if ((p->max_luminance > 1000) && (p->max_luminance < 10000)) {
		lut_x = tm_x_axis_gamma_2P2_4000;
		lut_y = tm_y_axis_gamma_2P2_4000;
	} else {
		lut_x = tm_x_axis_gamma_2P2_1000;
		lut_y = tm_y_axis_gamma_2P2_1000;
	}

#if defined(CONFIG_EXYNOS_HDR_TUNABLE_TONEMAPPING)
	if (exynos_hdr_get_tm_lut_xy(tm_x_tune, tm_y_tune)) {
		lut_x = tm_x_tune;
		lut_y = tm_y_tune;
	}
#endif

	for (i = 0; i < MAX_TM; i++) {
		dpp_write_mask(id,
			DPP_HDR_TM_X_AXIS_ADDR(i),
			DPP_HDR_TM_X_AXIS_VAL(i, lut_x[i]),
			DPP_HDR_TM_MASK(i));
		dpp_write_mask(id,
			DPP_HDR_TM_Y_AXIS_ADDR(i),
			DPP_HDR_TM_Y_AXIS_VAL(i, lut_y[i]),
			DPP_HDR_TM_MASK(i));
	}
}

static void dpp_reg_set_hdr_params(u32 id, struct dpp_params_info *p)
{
	u32 val, val2, mask;

	val = (p->hdr == DPP_HDR_ST2084 || p->hdr == DPP_HDR_HLG) ? ~0 : 0;
	mask = DPP_HDR_ON_MASK | DPP_EOTF_ON_MASK | DPP_TM_ON_MASK;
	dpp_write_mask(id, DPP_VGRF_HDR_CON, val, mask);

	val2 = (p->eq_mode != CSC_DCI_P3) ? ~0 : 0;
	dpp_write_mask(id, DPP_VGRF_HDR_CON, val2,  DPP_GM_ON_MASK);

	if (val) {
		dpp_reg_set_eotf_lut(id, p);
		dpp_reg_set_gm_lut(id, p);
		dpp_reg_set_tm_lut(id, p);
	}
}
#endif

/********** IDMA and ODMA combination CAL functions **********/
/*
 * Y8 : Y8 or RGB base, AFBC or SBWC-Y header
 * C8 : C8 base,        AFBC or SBWC-Y payload
 * Y2 : (SBWC disable - Y2 base), (SBWC enable - C header)
 * C2 : (SBWC disable - C2 base), (SBWC enable - C payload)
 *
 * PLANE_0_STRIDE : Y-HD (or Y-2B) stride -> yhd_y2_strd
 * PLANE_1_STRIDE : Y-PL (or C-2B) stride -> ypl_c2_strd
 * PLANE_2_STRIDE : C-HD stride           -> chd_strd
 * PLANE_3_STRIDE : C-PL stride           -> cpl_strd
 *
 * [ MFC encoder: buffer for SBWC - similar to 8+2 ]
 * plane[0] fd : Y payload(base addr) + Y header => Y header calc.
 * plane[1] fd : C payload(base addr) + C header => C header calc.
 */
static void dma_reg_set_base_addr(u32 id, struct dpp_params_info *p,
		const unsigned long attr)
{
	const struct dpu_fmt *fmt_info = dpu_find_fmt_info(p->format);

	if (test_bit(DPP_ATTR_IDMA, &attr)) {
		dma_write(id, RDMA_BASEADDR_Y8, p->addr[0]);
		if (p->comp_type == COMP_TYPE_AFBC)
			dma_write(id, RDMA_BASEADDR_C8, p->addr[0]);
		else
			dma_write(id, RDMA_BASEADDR_C8, p->addr[1]);

		if (fmt_info->num_planes == 4) { /* use 4 base addresses */
			dma_write(id, RDMA_BASEADDR_Y2, p->addr[2]);
			dma_write(id, RDMA_BASEADDR_C2, p->addr[3]);
			dma_write_mask(id, RDMA_SRC_STRIDE_1,
					IDMA_STRIDE_0(p->yhd_y2_strd),
					IDMA_STRIDE_0_MASK);
			dma_write_mask(id, RDMA_SRC_STRIDE_1,
					IDMA_STRIDE_1(p->ypl_c2_strd),
					IDMA_STRIDE_1_MASK);

			/* C-stride of SBWC: valid if STRIDE_SEL is enabled */
			dma_write_mask(id, RDMA_SRC_STRIDE_2,
					IDMA_STRIDE_2(p->chd_strd),
					IDMA_STRIDE_2_MASK);
			dma_write_mask(id, RDMA_SRC_STRIDE_2,
					IDMA_STRIDE_3(p->cpl_strd),
					IDMA_STRIDE_3_MASK);
		}
	} else if (test_bit(DPP_ATTR_ODMA, &attr)) {
		dma_write(id, WDMA_BASEADDR_Y8, p->addr[0]);
		dma_write(id, WDMA_BASEADDR_C8, p->addr[1]);

		if (fmt_info->num_planes == 4) {
			dma_write(id, WDMA_BASEADDR_Y2, p->addr[2]);
			dma_write(id, WDMA_BASEADDR_C2, p->addr[3]);
			dma_write_mask(id, WDMA_STRIDE_1,
					ODMA_STRIDE_0(p->yhd_y2_strd),
					ODMA_STRIDE_0_MASK);
			dma_write_mask(id, WDMA_STRIDE_1,
					ODMA_STRIDE_1(p->ypl_c2_strd),
					ODMA_STRIDE_1_MASK);

			/* C-stride of SBWC: valid if STRIDE_SEL is enabled */
			dma_write_mask(id, WDMA_STRIDE_2,
					ODMA_STRIDE_2(p->chd_strd),
					ODMA_STRIDE_2_MASK);
			dma_write_mask(id, WDMA_STRIDE_2,
					ODMA_STRIDE_3(p->cpl_strd),
					ODMA_STRIDE_3_MASK);
		}
	}

	dpp_dbg("dpp%d: base addr 1p(0x%lx) 2p(0x%lx) 3p(0x%lx) 4p(0x%lx)\n",
			id,
			(unsigned long)p->addr[0], (unsigned long)p->addr[1],
			(unsigned long)p->addr[2], (unsigned long)p->addr[3]);
	if ((p->comp_type == COMP_TYPE_SBWC) || (p->comp_type == COMP_TYPE_SBWCL))
		dpp_dbg("dpp%d:[stride] yh(0x%lx)yp(0x%lx)ch(0x%lx)cp(0x%lx)\n",
			id,
			(unsigned long)p->yhd_y2_strd,
			(unsigned long)p->ypl_c2_strd,
			(unsigned long)p->chd_strd,
			(unsigned long)p->cpl_strd);
}

/********** IDMA, ODMA, DPP and WB MUX combination CAL functions **********/
static void dma_dpp_reg_set_coordinates(u32 id, struct dpp_params_info *p,
		const unsigned long attr)
{
	if (test_bit(DPP_ATTR_IDMA, &attr)) {
#if defined(CONFIG_EXYNOS_SUPPORT_8K_SPLIT)
		idma_reg_set_split_en(id, p->aux_src.spl_en);
#endif
		idma_reg_set_coordinates(id, &p->src);

		if (test_bit(DPP_ATTR_DPP, &attr)) {
			if (p->rot > DPP_ROT_180)
				dpp_reg_set_img_size(id, p->src.h, p->src.w);
			else
				dpp_reg_set_img_size(id, p->src.w, p->src.h);
		}

		if (test_bit(DPP_ATTR_SCALE, &attr)) {
			dpp_reg_set_scl_pos(id, p);
			dpp_reg_set_scaled_img_size(id, p->dst.w, p->dst.h);
		}
	} else if (test_bit(DPP_ATTR_ODMA, &attr)) {
		odma_reg_set_coordinates(id, &p->src);
		if (test_bit(DPP_ATTR_DPP, &attr))
			dpp_reg_set_img_size(id, p->dst.w, p->dst.h);
	}
}

static int dma_dpp_reg_set_format(u32 id, struct dpp_params_info *p,
		const unsigned long attr)
{
	u32 dma_fmt;
	u32 alpha_type = 0; /* 0: per-frame, 1: per-pixel */
	u32 val, mask;
	const struct dpu_fmt *fmt_info = dpu_find_fmt_info(p->format);

	if (fmt_info->fmt == DECON_PIXEL_FORMAT_RGB_565 && p->is_comp)
		dma_fmt = IDMA_IMG_FORMAT_BGR565;
	else if (fmt_info->fmt == DECON_PIXEL_FORMAT_BGR_565 && p->is_comp)
		dma_fmt = IDMA_IMG_FORMAT_RGB565;
	else
		dma_fmt = fmt_info->dma_fmt;

	alpha_type = (fmt_info->len_alpha > 0) ? 1 : 0;

	if (test_bit(DPP_ATTR_IDMA, &attr)) {
		idma_reg_set_format(id, dma_fmt);
		if (test_bit(DPP_ATTR_DPP, &attr)) {
			val = DPP_ALPHA_SEL(alpha_type) |
				DPP_IMG_FORMAT(fmt_info->dpp_fmt);
			mask = DPP_ALPHA_SEL_MASK | DPP_IMG_FORMAT_MASK;
			dpp_write_mask(id, DPP_COM_IO_CON, val, mask);
		}
	} else if (test_bit(DPP_ATTR_ODMA, &attr)) {
		odma_reg_set_format(id, dma_fmt);
		//wb_mux_reg_set_format(id, fmt_info);
		//wb_mux_reg_set_uv_offset(id, 0, 0);
		if (test_bit(DPP_ATTR_DPP, &attr)) {
			val = DPP_ALPHA_SEL(alpha_type) |
				DPP_IMG_FORMAT(fmt_info->dpp_fmt);
			mask = DPP_ALPHA_SEL_MASK | DPP_IMG_FORMAT_MASK;
			dpp_write_mask(id, DPP_COM_IO_CON, val, mask);
		}
		if (IS_YUV(fmt_info)) {
#if IS_ENABLED(CONFIG_EXYNOS_SBWC_LIBREQ)
			if (p->lib_requested == false)
#endif
				odma_reg_set_sub_con(id);
		}
	}

	return 0;
}


/******************** EXPORTED DPP CAL APIs ********************/
void dpp_constraints_params(struct dpp_size_constraints *vc,
		struct dpp_img_format *vi, struct dpp_restriction *res)
{
	u32 sz_align = 1;
	const struct dpu_fmt *fmt_info = dpu_find_fmt_info(vi->format);

	if (IS_YUV(fmt_info))
		sz_align = 2;

	vc->src_mul_w = res->src_f_w.align * sz_align;
	vc->src_mul_h = res->src_f_h.align * sz_align;
	vc->src_w_min = res->src_f_w.min * sz_align;
	vc->src_w_max = res->src_f_w.max;
	vc->src_h_min = res->src_f_h.min;
	vc->src_h_max = res->src_f_h.max;
#if defined(CONFIG_SOC_EXYNOS2100_EVT0)
	vc->img_mul_w = res->src_w.align * sz_align;
	vc->img_mul_h = res->src_h.align * sz_align;
	vc->img_w_min = res->src_w.min * sz_align;
	vc->img_h_min = res->src_h.min * sz_align;
#else
	vc->img_mul_w = res->src_w.align * 1;
	vc->img_mul_h = res->src_h.align * 1;
	vc->img_w_min = res->src_w.min * 1;
	vc->img_h_min = res->src_h.min * 1;
#endif
	if (vi->rot > DPP_ROT_180) {
		vc->img_w_max = res->reserved[SRC_W_ROT_MAX];
		vc->img_h_max = res->src_h_rot_max;
	} else {
		vc->img_w_max = res->src_w.max;
		vc->img_h_max = res->src_h.max;
	}
#if defined(CONFIG_SOC_EXYNOS2100_EVT0)
	vc->src_mul_x = res->src_x_align * sz_align;
	vc->src_mul_y = res->src_y_align * sz_align;
#else
	vc->src_mul_x = res->src_x_align * 1;
	vc->src_mul_y = res->src_y_align * 1;
#endif

	vc->sca_w_min = res->dst_w.min;
	vc->sca_w_max = res->dst_w.max;
	vc->sca_h_min = res->dst_h.min;
	vc->sca_h_max = res->dst_h.max;
	vc->sca_mul_w = res->dst_w.align;
	vc->sca_mul_h = res->dst_h.align;

	vc->blk_w_min = res->blk_w.min;
	vc->blk_w_max = res->blk_w.max;
	vc->blk_h_min = res->blk_h.min;
	vc->blk_h_max = res->blk_h.max;
	vc->blk_mul_w = res->blk_w.align;
	vc->blk_mul_h = res->blk_h.align;

	if (vi->wb) {
		vc->src_mul_w = res->dst_f_w.align * sz_align;
		vc->src_mul_h = res->dst_f_h.align * sz_align;
		vc->src_w_min = res->dst_f_w.min;
		vc->src_w_max = res->dst_f_w.max;
		vc->src_h_min = res->dst_f_h.min;
		vc->src_h_max = res->dst_f_h.max;
		vc->img_mul_w = res->dst_w.align * sz_align;
		vc->img_mul_h = res->dst_h.align * sz_align;
		vc->img_w_min = res->dst_w.min;
		vc->img_w_max = res->dst_w.max;
		vc->img_h_min = res->dst_h.min;
		vc->img_h_max = res->dst_h.max;
		vc->src_mul_x = res->dst_x_align * sz_align;
		vc->src_mul_y = res->dst_y_align * sz_align;
	}
}

void dpp_reg_init(u32 id, const unsigned long attr)
{
	if (test_bit(DPP_ATTR_IDMA, &attr)) {
		idma_reg_set_irq_mask_all(id, 1);
		idma_reg_set_irq_disable(id);
		idma_reg_set_in_qos_lut(id, 0, 0x44444444);
		idma_reg_set_in_qos_lut(id, 1, 0x44444444);
		/* TODO: clock gating will be enabled */
		idma_reg_set_sram_clk_gate_en(id, 0);
		idma_reg_set_dynamic_gating_en_all(id, 0);
		idma_reg_set_frame_alpha(id, 0xFF);
		idma_reg_set_ic_max(id, 0x40);
		idma_reg_set_assigned_mo(id, 0x40);
	}

	if (test_bit(DPP_ATTR_DPP, &attr)) {
		dpp_reg_set_irq_mask_all(id, 1);
		dpp_reg_set_irq_disable(id);
		dpp_reg_set_linecnt(id, 1);
	}

	if (test_bit(DPP_ATTR_ODMA, &attr)) {
		odma_reg_set_irq_mask_all(id, 1); /* irq unmask */
		odma_reg_set_irq_disable(id);
		odma_reg_set_in_qos_lut(id, 0, 0x44444444);
		odma_reg_set_in_qos_lut(id, 1, 0x44444444);
		odma_reg_set_dynamic_gating_en_all(id, 0);
		odma_reg_set_frame_alpha(id, 0xFF);
		odma_reg_set_ic_max(id, 0x40);
	}

#if defined(CONFIG_EXYNOS_SUPPORT_VOTF_IN)
	idma_reg_set_votf_init();
#endif
}

void dpp_reg_irq_enable(u32 id, const unsigned long attr)
{
	if (test_bit(DPP_ATTR_IDMA, &attr)) {
		idma_reg_set_irq_mask_all(id, 0);
		idma_reg_set_irq_enable(id);
	}

	if (test_bit(DPP_ATTR_DPP, &attr)) {
		dpp_reg_set_irq_mask_all(id, 0);
		dpp_reg_set_irq_enable(id);
	}

	if (test_bit(DPP_ATTR_ODMA, &attr)) {
		odma_reg_set_irq_mask_all(id, 0); /* irq unmask */
		odma_reg_set_irq_enable(id);
	}

}

int dpp_reg_deinit(u32 id, bool reset, const unsigned long attr)
{
	if (test_bit(DPP_ATTR_IDMA, &attr)) {
		idma_reg_clear_irq(id, IDMA_ALL_IRQ_CLEAR);
		idma_reg_set_irq_mask_all(id, 1);
	}

	if (test_bit(DPP_ATTR_DPP, &attr)) {
		dpp_reg_clear_irq(id, DPP_ALL_IRQ_CLEAR);
		dpp_reg_set_irq_mask_all(id, 1);
	}

	if (test_bit(DPP_ATTR_ODMA, &attr)) {
		odma_reg_clear_irq(id, ODMA_ALL_IRQ_CLEAR);
		odma_reg_set_irq_mask_all(id, 1); /* irq mask */
	}

	if (reset) {
		if (test_bit(DPP_ATTR_IDMA, &attr) &&
				!test_bit(DPP_ATTR_DPP, &attr)) { /* IDMA */
			idma_reg_set_sw_reset(id);
			if (idma_reg_wait_sw_reset_status(id))
				return -1;
		} else if (test_bit(DPP_ATTR_IDMA, &attr) &&
				test_bit(DPP_ATTR_DPP, &attr)) { /* IDMA/DPP */
			idma_reg_set_sw_reset(id);
			dpp_reg_set_sw_reset(id);
			if (idma_reg_wait_sw_reset_status(id) ||
					dpp_reg_wait_sw_reset_status(id))
				return -1;
		} else if (test_bit(DPP_ATTR_ODMA, &attr)) { /* writeback */
			odma_reg_set_sw_reset(id);
			dpp_reg_set_sw_reset(id);
			if (odma_reg_wait_sw_reset_status(id) ||
					dpp_reg_wait_sw_reset_status(id))
				return -1;
		} else {
			dpp_err("%s: not support attribute case(0x%lx)\n",
					__func__, attr);
		}
	}

	return 0;
}

#if defined(DMA_BIST)
static u32 pattern_data[] = {
	0xffffffff,
	0xffffffff,
	0xffffffff,
	0xffffffff,
	0x000000ff,
	0x000000ff,
	0x000000ff,
	0x000000ff,
};
#endif

void dpp_reg_configure_params(u32 id, struct dpp_params_info *p,
		const unsigned long attr)
{
	u32 size_32x = 0;
	const struct dpu_fmt *fmt_info = dpu_find_fmt_info(p->format);

	if (test_bit(DPP_ATTR_CSC, &attr) && test_bit(DPP_ATTR_DPP, &attr)
			&& IS_YUV(fmt_info))
		dpp_reg_set_csc_params(id, p->eq_mode, attr);

	if (test_bit(DPP_ATTR_SCALE, &attr))
		dpp_reg_set_scale_ratio(id, p);

	/* configure coordinates and size of IDMA, DPP, ODMA and WB MUX */
	dma_dpp_reg_set_coordinates(id, p, attr);

	if (test_bit(DPP_ATTR_ROT, &attr) || test_bit(DPP_ATTR_FLIP, &attr))
		idma_reg_set_rotation(id, p->rot);

	/* configure base address of IDMA and ODMA */
	dma_reg_set_base_addr(id, p, attr);

	if (test_bit(DPP_ATTR_BLOCK, &attr))
		idma_reg_set_block_mode(id, p->is_block, p->block.x, p->block.y,
				p->block.w, p->block.h);

	/* configure image format of IDMA, DPP, ODMA and WB MUX */
	dma_dpp_reg_set_format(id, p, attr);

#ifndef CONFIG_MCDHDR
	if (test_bit(DPP_ATTR_HDR, &attr))
		dpp_reg_set_hdr_params(id, p);
#endif
	if (test_bit(DPP_ATTR_AFBC, &attr))
		idma_reg_set_afbc(id, p->comp_type, p->rcv_num);

	if (test_bit(DPP_ATTR_SBWC, &attr)) {
		if (p->comp_type == COMP_TYPE_SBWCL) {
			switch (p->format) {
			case DECON_PIXEL_FORMAT_NV12M_SBWC_8B_L50:
			case DECON_PIXEL_FORMAT_NV12N_SBWC_8B_L50:
			case DECON_PIXEL_FORMAT_NV12M_SBWC_10B_L40:
			case DECON_PIXEL_FORMAT_NV12N_SBWC_10B_L40:
				size_32x = 2;
				break;
			case DECON_PIXEL_FORMAT_NV12M_SBWC_8B_L75:
			case DECON_PIXEL_FORMAT_NV12N_SBWC_8B_L75:
			case DECON_PIXEL_FORMAT_NV12M_SBWC_10B_L60:
			case DECON_PIXEL_FORMAT_NV12N_SBWC_10B_L60:
				size_32x = 3;
				break;
			case DECON_PIXEL_FORMAT_NV12M_SBWC_10B_L80:
			case DECON_PIXEL_FORMAT_NV12N_SBWC_10B_L80:
				size_32x = 4;
				break;
			default:
				dpp_err("dpp%d idma lossy sbwc config error occur\n", id);
				break;
			}
			idma_reg_set_sbwcl(id, p->comp_type, size_32x, p->rcv_num);
		} else if (p->comp_type == COMP_TYPE_SBWC) {
			switch (p->format) {
			case DECON_PIXEL_FORMAT_NV12M_SBWC_8B:
			case DECON_PIXEL_FORMAT_NV21M_SBWC_8B:
			case DECON_PIXEL_FORMAT_NV12N_SBWC_8B:
				size_32x = 4;
				break;
			case DECON_PIXEL_FORMAT_NV12M_SBWC_10B:
			case DECON_PIXEL_FORMAT_NV21M_SBWC_10B:
			case DECON_PIXEL_FORMAT_NV12N_SBWC_10B:
				size_32x = 5;
				break;
			default:
				dpp_err("dpp%d idma lossless sbwc config error occur\n", id);
				break;
			}
			idma_reg_set_sbwc(id, p->comp_type, size_32x, p->rcv_num);
		} else
			idma_reg_set_sbwc(id, p->comp_type, size_32x, p->rcv_num);
	}

	/*
	 * To check HW stuck
	 * dead_lock min: 17ms (17ms: 1-frame time, rcv_time: 1ms)
	 * but, considered DVFS 3x level switch (ex: 200 <-> 600 Mhz)
	 */
	idma_reg_set_deadlock(id, 1, p->rcv_num * 51);

#if defined(DMA_BIST)
	idma_reg_set_test_pattern(id, 0, pattern_data);
#endif

#if defined(CONFIG_EXYNOS_SUPPORT_HWFC)
	if (test_bit(DPP_ATTR_ODMA, &attr))
		odma_reg_set_hwfc(id, p->hwfc_enable, p->hwfc_idx);
#endif

#if defined(CONFIG_EXYNOS_SUPPORT_VOTF_IN)
	idma_reg_set_votf(id, p->votf_enable, p->votf_buffer_idx, p->rcv_num);
#endif
}

u32 dpp_reg_get_irq_and_clear(u32 id)
{
	u32 val, cfg_err;

	val = dpp_read(id, DPP_COM_IRQ_STATUS);
	if (val & DPP_CFG_ERROR_IRQ) {
		cfg_err = dpp_read(id, DPP_COM_CFG_ERROR_STATUS);
		dpp_err("dpp%d config error occur(0x%x)\n", id, cfg_err);
	}
	dpp_reg_clear_irq(id, val);

	return val;
}

/*
 * CFG_ERR is cleared when clearing pending bits
 * So, get cfg_err first, then clear pending bits
 */
u32 idma_reg_get_irq_and_clear(u32 id)
{
	u32 val, cfg_err;

	val = dma_read(id, RDMA_IRQ);
	if (val & IDMA_CONFIG_ERR_IRQ) {
		cfg_err = dma_read(id, RDMA_CONFIG_ERR_STATUS);
		dpp_err("dma%d config error occur(0x%x)\n", id, cfg_err);
	}
	idma_reg_clear_irq(id, val);

	return val;
}

u32 odma_reg_get_irq_and_clear(u32 id)
{
	u32 val, cfg_err;

	val = dma_read(id, WDMA_IRQ);

	if (val & ODMA_CONFIG_ERR_IRQ) {
		cfg_err = dma_read(id, WDMA_CONFIG_ERR_STATUS);
		dpp_err("dma%d config error occur(0x%x)\n", id, cfg_err);
	}
	odma_reg_clear_irq(id, val);

	return val;
}

#if defined(CONFIG_EXYNOS_SUPPORT_HWFC)
void odma_reg_set_hwfc(u32 id, u32 en, u32 hwfc_idx)
{
	u32 val = en ? ~0 : 0;

	if (en) {
		dma_write_mask(id, WDMA_VOTF_EN, 0, ODMA_VOTF_EN);
		dma_write_mask(id, WDMA_HWFC_CTRL,
				ODMA_HWFC_LINE_NUM(HWFC_LINE_NUM),
				ODMA_HWFC_LINE_NUM_MASK);
		dma_write_mask(id, WDMA_HWFC_EN,
				ODMA_HWFC_PRODUCE_IDX(hwfc_idx),
				ODMA_HWFC_PRODUCE_IDX_MASK);
	}

	dma_write_mask(id, WDMA_HWFC_EN, val, ODMA_HWFC_EN);
}
#endif

void dma_reg_get_shd_addr(u32 id, u32 shd_addr[], const unsigned long attr)
{
	if (test_bit(DPP_ATTR_IDMA, &attr)) {
		shd_addr[0] = dma_read(id, RDMA_BASEADDR_Y8 + DMA_SHD_OFFSET);
		shd_addr[1] = dma_read(id, RDMA_BASEADDR_C8 + DMA_SHD_OFFSET);
		shd_addr[2] = dma_read(id, RDMA_BASEADDR_Y2 + DMA_SHD_OFFSET);
		shd_addr[3] = dma_read(id, RDMA_BASEADDR_C2 + DMA_SHD_OFFSET);
	} else if (test_bit(DPP_ATTR_ODMA, &attr)) {
		shd_addr[0] = dma_read(id, WDMA_BASEADDR_Y8 + DMA_SHD_OFFSET);
		shd_addr[1] = dma_read(id, WDMA_BASEADDR_C8 + DMA_SHD_OFFSET);
		shd_addr[2] = dma_read(id, WDMA_BASEADDR_Y2 + DMA_SHD_OFFSET);
		shd_addr[3] = dma_read(id, WDMA_BASEADDR_C2 + DMA_SHD_OFFSET);
	}
	dpp_dbg("dpp%d: shadow addr 1p(0x%x) 2p(0x%x) 3p(0x%x) 4p(0x%x)\n",
			id, shd_addr[0], shd_addr[1], shd_addr[2], shd_addr[3]);
}

static void dpp_reg_dump_ch_data(int id, enum dpp_reg_area reg_area,
					const u32 sel[], u32 cnt)
{
	/* TODO: This will be implemented in the future */
}

static void dma_reg_dump_com_debug_regs(int id)
{
	static bool checked;
	const u32 sel_glb[99] = {
		0x0000, 0x0001, 0x0005, 0x0009, 0x000D, 0x000E, 0x0020, 0x0021,
		0x0025, 0x0029, 0x002D, 0x002E, 0x0040, 0x0041, 0x0045, 0x0049,
		0x004D, 0x004E, 0x0060, 0x0061, 0x0065, 0x0069, 0x006D, 0x006E,
		0x0080, 0x0081, 0x0082, 0x0083, 0x00C0, 0x00C1, 0x00C2, 0x00C3,
		0x0100, 0x0101, 0x0200, 0x0201, 0x0202, 0x0300, 0x0301, 0x0302,
		0x0303, 0x0304, 0x0400, 0x4000, 0x4001, 0x4005, 0x4009, 0x400D,
		0x400E, 0x4020, 0x4021, 0x4025, 0x4029, 0x402D, 0x402E, 0x4040,
		0x4041, 0x4045, 0x4049, 0x404D, 0x404E, 0x4060, 0x4061, 0x4065,
		0x4069, 0x406D, 0x406E, 0x4100, 0x4101, 0x4200, 0x4201, 0x4300,
		0x4301, 0x4302, 0x4303, 0x4304, 0x4400, 0x8080, 0x8081, 0x8082,
		0x8083, 0x80C0, 0x80C1, 0x80C2, 0x80C3, 0x8100, 0x8101, 0x8201,
		0x8202, 0x8300, 0x8301, 0x8302, 0x8303, 0x8304, 0x8400, 0xC000,
		0xC001, 0xC002, 0xC005
	};

	dpp_info("%s: checked = %d\n", __func__, checked);
	if (checked)
		return;

	dpp_info("-< DMA COMMON DEBUG SFR >-\n");
	dpp_reg_dump_ch_data(id, REG_AREA_DMA_COM, sel_glb, 99);

	checked = true;
}

static void dma_reg_dump_debug_regs(int id)
{
	/* TODO: This will be implemented in the future */
}

static void dpp_reg_dump_debug_regs(int id)
{
	/* TODO: This will be implemented in the future */
}

#define PREFIX_LEN	40
#define ROW_LEN		32
static void dpp_print_hex_dump(void __iomem *regs, const void *buf, size_t len)
{
	char prefix_buf[PREFIX_LEN];
	unsigned long p;
	int i, row;

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

static void dma_dump_regs(u32 id, void __iomem *dma_regs)
{
	dpp_info("\n=== DPU_DMA%d SFR DUMP ===\n", id);
	dpp_print_hex_dump(dma_regs, dma_regs + 0x0000, 0x144);
	dpp_print_hex_dump(dma_regs, dma_regs + 0x0200, 0x8);
	dpp_print_hex_dump(dma_regs, dma_regs + 0x0300, 0x24);
	dpp_print_hex_dump(dma_regs, dma_regs + 0x0730, 0x4);
	dpp_print_hex_dump(dma_regs, dma_regs + 0x0740, 0x4);
	dpp_print_hex_dump(dma_regs, dma_regs + 0x0D00, 0x28);

	/* L0,4,8,12 only */
	if ((id % 4) == 0) {
		dpp_print_hex_dump(dma_regs, dma_regs + 0x0E00, 0x14);
		dpp_print_hex_dump(dma_regs, dma_regs + 0x0F00, 0x40);
	}

	dpp_info("=== DPU_DMA%d SHADOW SFR DUMP ===\n", id);
	dpp_print_hex_dump(dma_regs, dma_regs + 0x0000 + DMA_SHD_OFFSET, 0x144);
	dpp_print_hex_dump(dma_regs, dma_regs + 0x0200 + DMA_SHD_OFFSET, 0x8);
	dpp_print_hex_dump(dma_regs, dma_regs + 0x0300 + DMA_SHD_OFFSET, 0x24);
	dpp_print_hex_dump(dma_regs, dma_regs + 0x0D00 + 0x80, 0x28);

}

static void dpp_dump_regs(u32 id, void __iomem *regs, unsigned long attr)
{
	dpp_info("=== DPP%d SFR DUMP ===\n", id);
	dpp_print_hex_dump(regs, regs + 0x0000, 0x64);
	if ((id % 2) && (id != 5) && (id != 13)) {
		/* L1,3,7,9,11,15 only */
		dpp_print_hex_dump(regs, regs + 0x0200, 0xC);
		// skip coef : 0x210 ~ 0x56C
		dpp_print_hex_dump(regs, regs + 0x0570, 0x30);
	}

	dpp_info("=== DPP%d SHADOW SFR DUMP ===\n", id);
	dpp_print_hex_dump(regs, regs + DPP_COM_SHD_OFFSET, 0x64);
	if ((id % 2) && (id != 5) && (id != 13)) {
		/* L1,3,7,9,11,15 only */
		dpp_print_hex_dump(regs, regs + 0x0200 + DPP_SCL_SHD_OFFSET,
				0xC);
		// skip coef : (0x210 ~ 0x56C) + DPP_SCL_SHD_OFFSET
		dpp_print_hex_dump(regs, regs + 0x0570 + DPP_SCL_SHD_OFFSET,
				0x30);
	}
}

void __dpp_dump(u32 id, void __iomem *regs, void __iomem *dma_regs,
		unsigned long attr)
{
	dma_reg_dump_com_debug_regs(id);

	dma_dump_regs(id, dma_regs);
	dma_reg_dump_debug_regs(id);

	dpp_dump_regs(id, regs, attr);
	dpp_reg_dump_debug_regs(id);
}

static const struct dpu_fmt dpu_cal_formats_list[] = {
	{
		.name = "NV16",
		.fmt = DECON_PIXEL_FORMAT_NV16,
		.dma_fmt = IDMA_IMG_FORMAT_YUV422_2P,
		.dpp_fmt = DPP_IMG_FORMAT_YUV422_8P,
		.bpp = 16,
		.padding = 0,
		.bpc = 8,
		.num_planes = 2,
		.num_buffers = 2,
		.num_meta_planes = 0,
		.len_alpha = 0,
		.cs = DPU_COLORSPACE_YUV422,
		.ct = COMP_TYPE_NONE,
	}, {
		.name = "NV61",
		.fmt = DECON_PIXEL_FORMAT_NV61,
		.dma_fmt = IDMA_IMG_FORMAT_YVU422_2P,
		.dpp_fmt = DPP_IMG_FORMAT_YUV422_8P,
		.bpp = 16,
		.padding = 0,
		.bpc = 8,
		.num_planes = 2,
		.num_buffers = 2,
		.num_meta_planes = 0,
		.len_alpha = 0,
		.cs = DPU_COLORSPACE_YUV422,
		.ct = COMP_TYPE_NONE,
	}, {
		.name = "NV16M_P210",
		.fmt = DECON_PIXEL_FORMAT_NV16M_P210,
		.dma_fmt = IDMA_IMG_FORMAT_YUV422_P210,
		.dpp_fmt = DPP_IMG_FORMAT_YUV422_P210,
		.bpp = 20,
		.padding = 12,
		.bpc = 10,
		.num_planes = 2,
		.num_buffers = 2,
		.num_meta_planes = 1,
		.len_alpha = 0,
		.cs = DPU_COLORSPACE_YUV422,
		.ct = COMP_TYPE_NONE,
	}, {
		.name = "NV61M_P210",
		.fmt = DECON_PIXEL_FORMAT_NV61M_P210,
		.dma_fmt = IDMA_IMG_FORMAT_YVU422_P210,
		.dpp_fmt = DPP_IMG_FORMAT_YUV422_P210,
		.bpp = 20,
		.padding = 12,
		.bpc = 10,
		.num_planes = 2,
		.num_buffers = 2,
		.num_meta_planes = 1,
		.len_alpha = 0,
		.cs = DPU_COLORSPACE_YUV422,
		.ct = COMP_TYPE_NONE,
	}, {
		.name = "NV16M_S10B",
		.fmt = DECON_PIXEL_FORMAT_NV16M_S10B,
		.dma_fmt = IDMA_IMG_FORMAT_YUV422_8P2,
		.dpp_fmt = DPP_IMG_FORMAT_YUV422_8P2,
		.bpp = 20,
		.padding = 0,
		.bpc = 10,
		.num_planes = 4,
		.num_buffers = 2,
		.num_meta_planes = 1,
		.len_alpha = 0,
		.cs = DPU_COLORSPACE_YUV422,
		.ct = COMP_TYPE_NONE,
	}, {
		.name = "NV61M_S10B",
		.fmt = DECON_PIXEL_FORMAT_NV61M_S10B,
		.dma_fmt = IDMA_IMG_FORMAT_YVU422_8P2,
		.dpp_fmt = DPP_IMG_FORMAT_YUV422_8P2,
		.bpp = 20,
		.padding = 0,
		.bpc = 10,
		.num_planes = 4,
		.num_buffers = 2,
		.num_meta_planes = 1,
		.len_alpha = 0,
		.cs = DPU_COLORSPACE_YUV422,
		.ct = COMP_TYPE_NONE,
	},
};

const struct dpu_fmt *dpu_find_cal_fmt_info(enum decon_pixel_format fmt)
{
	int i;

	for (i = 0; i < ARRAY_SIZE(dpu_cal_formats_list); i++)
		if (dpu_cal_formats_list[i].fmt == fmt)
			return &dpu_cal_formats_list[i];

	return NULL;
}
