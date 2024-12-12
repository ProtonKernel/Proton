// SPDX-License-Identifier: GPL-2.0-only
/*
 * dpu30/cal_2100/decon_reg.c
 *
 * Copyright (c) 2019 Samsung Electronics Co., Ltd.
 *		http://www.samsung.com
 *
 * Jaehoe Yang <jaehoe.yang@samsung.com>
 * Jiun Yu <jiun.yu@samsung.com>
 *
 * Register access functions for Samsung EXYNOS DECON driver
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#include "../decon.h"
#include "../format.h"
#include <linux/iopoll.h>

#if defined(CONFIG_EXYNOS_DECON_DQE)
#include "../dqe.h"
#endif

#define VERBOSE	0

#if defined(CONFIG_SOC_EXYNOS2100)
#define DECON_MAIN_SHADOW_DUMP_OFFSET	0x20
#define DECON_MAIN_SHADOW_SFR_COUNT	((0x2D8 - DECON_MAIN_SHADOW_DUMP_OFFSET) / 4)
#define DECON_WIN_SHADOW_SFR_COUNT	(0x20 / 4)
u32 decon_main_shadow_sfr[DECON_MAIN_SHADOW_SFR_COUNT];
u32 decon_win_shadow_sfr[MAX_DECON_WIN][DECON_WIN_SHADOW_SFR_COUNT];
#endif

/******************* DECON CAL functions *************************/
int decon_reg_reset(u32 id)
{

	struct decon_device *decon = get_decon_drvdata(id);
	u32 val;
	int ret;
	int offset = 0;

	switch (id) {
	case 0:
		offset = 0x0000;
		break;
	case 1:
		offset = 0x1000;
		break;
	case 2:
		offset = 0x2000;
		break;
#if !defined(CONFIG_SOC_EXYNOS2100_EVT0)
	case 3:
		offset = 0x3000;
		break;
#endif
	default:
		offset = 0x0000;
	}

	decon_write_mask(id, GLOBAL_CON, ~0, GLOBAL_CON_SRESET);
	ret = readl_poll_timeout(decon->res.regs + offset + GLOBAL_CON, val,
			!(val & GLOBAL_CON_SRESET), 10, 2000);
	if (ret)
		decon_err("failed to reset decon%d\n", id);

	return ret;
}

/* select op mode */
static void decon_reg_set_operation_mode(u32 id, enum decon_psr_mode mode)
{
	u32 val, mask;

	mask = GLOBAL_CON_OPERATION_MODE_F;
	if (mode == DECON_MIPI_COMMAND_MODE)
		val = GLOBAL_CON_OPERATION_MODE_CMD_F;
	else
		val = GLOBAL_CON_OPERATION_MODE_VIDEO_F;
	decon_write_mask(id, GLOBAL_CON, val, mask);
}

static void decon_reg_direct_on_off(u32 id, u32 en)
{
	u32 val, mask;

	val = en ? ~0 : 0;
	mask = (GLOBAL_CON_DECON_EN | GLOBAL_CON_DECON_EN_F);
	decon_write_mask(id, GLOBAL_CON, val, mask);
}

static void decon_reg_per_frame_off(u32 id)
{
	decon_write_mask(id, GLOBAL_CON, 0, GLOBAL_CON_DECON_EN_F);
}

static u32 decon_reg_get_idle_status(u32 id)
{
	u32 val;

	val = decon_read(id, GLOBAL_CON);
	if (val & GLOBAL_CON_IDLE_STATUS)
		return 1;

	return 0;
}

u32 decon_reg_get_run_status(u32 id)
{
	u32 val;

	val = decon_read(id, GLOBAL_CON);
	if (val & GLOBAL_CON_RUN_STATUS)
		return 1;

	return 0;
}

static int decon_reg_wait_run_status_timeout(u32 id, unsigned long timeout)
{
	struct decon_device *decon = get_decon_drvdata(id);
	u32 val;
	int ret;
	int offset = 0;

	switch (id) {
	case 0:
		offset = 0x0000;
		break;
	case 1:
		offset = 0x1000;
		break;
	case 2:
		offset = 0x2000;
		break;
	case 3:
		offset = 0x3000;
		break;
	default:
		offset = 0x0000;
	}

	ret = readl_poll_timeout(decon->res.regs + offset + GLOBAL_CON, val,
			(val & GLOBAL_CON_RUN_STATUS), 10, timeout);
	if (ret) {
		decon_err("failed to change running status of DECON%d\n", id);
		return ret;
	}

	return 0;
}

/* Determine that DECON is perfectly shut off through checking this function */
static int decon_reg_wait_run_is_off_timeout(u32 id, unsigned long timeout)
{
	struct decon_device *decon = get_decon_drvdata(id);
	u32 val;
	int ret;
	int offset = 0;

	switch (id) {
	case 0:
		offset = 0x0000;
		break;
	case 1:
		offset = 0x1000;
		break;
	case 2:
		offset = 0x2000;
		break;
	case 3:
		offset = 0x3000;
		break;
	default:
		offset = 0x0000;
	}

	ret = readl_poll_timeout(decon->res.regs + offset + GLOBAL_CON, val,
			!(val & GLOBAL_CON_RUN_STATUS), 10, timeout);
	if (ret) {
		decon_err("failed to change off status of DECON%d\n", id);
		return ret;
	}

	return 0;
}

/* In bring-up, all bits are disabled */
static void decon_reg_set_clkgate_mode(u32 id, u32 en)
{
	u32 val, mask;

	val = en ? ~0 : 0;
	/* all unmask */
	mask = CLOCK_CON_AUTO_CG_MASK | CLOCK_CON_QACTIVE_MASK;
	decon_write_mask(0, CLOCK_CON, val, mask);
}

void decon_reg_set_te_qactive_pll_mode(u32 id, u32 en)
{
	u32 val, mask;

	val = en ? ~0 : 0;
	/* all unmask */
	mask = CLOCK_CON_QACTIVE_PLL_ON;
	decon_write_mask(0, CLOCK_CON, val, mask);
}

static void decon_reg_set_ewr_mode(u32 id, u32 en)
{
	u32 val, mask;

	val = en ? ~0 : 0;
	mask = EWR_EN_F;
	decon_write_mask(id, EWR_CON, val, mask);
}

static void decon_reg_set_sram_enable(u32 id)
{
	if (id == 0) {
		decon_write_mask(id, SRAM_EN_OF_PRI_0, ~0,
				SRAM0_EN_F | SRAM1_EN_F);
	} else if (id == 1)
		decon_write_mask(id, SRAM_EN_OF_PRI_0, ~0,
				SRAM0_EN_F | SRAM1_EN_F);
	else if (id == 2)
		decon_write_mask(id, SRAM_EN_OF_PRI_0, ~0,
				SRAM0_EN_F | SRAM1_EN_F | SRAM2_EN_F); /* DP default decon */
	else if (id == 3)
		decon_write_mask(id, SRAM_EN_OF_PRI_0, ~0,
				SRAM0_EN_F | SRAM1_EN_F | SRAM2_EN_F);
}

static void decon_reg_set_outfifo_size_ctl0(u32 id, u32 width, u32 height)
{
	u32 val;
	u32 th, mask;

	/* OUTFIFO */
	val = OUTFIFO_HEIGHT_F(height) | OUTFIFO_WIDTH_F(width);
	decon_write(id, OF_SIZE_0, val);

	/* may be implemented later by considering 1/2H transfer */
	th = OUTFIFO_TH_1H_F; /* 1H transfer */
	mask = OUTFIFO_TH_MASK;
	decon_write_mask(id, OF_TH_TYPE, th, mask);
}

static void decon_reg_set_outfifo_size_ctl1(u32 id, u32 width)
{
	u32 val;

	val = OUTFIFO_1_WIDTH_F(width);
	decon_write(0, OF_SIZE_1, val);
}

static void decon_reg_set_outfifo_size_ctl2(u32 id, u32 width, u32 height)
{
	u32 val;

	val = OUTFIFO_COMPRESSED_SLICE_HEIGHT_F(height) |
			OUTFIFO_COMPRESSED_SLICE_WIDTH_F(width);

	decon_write(id, OF_SIZE_2, val);
}

static void decon_reg_set_rgb_order(u32 id, enum decon_rgb_order order)
{
	u32 val, mask;

	val = OUTFIFO_PIXEL_ORDER_SWAP_F(order);
	mask = OUTFIFO_PIXEL_ORDER_SWAP_MASK;
	decon_write_mask(id, OF_PIXEL_ORDER, val, mask);
}

u32 decon_reg_get_outfifo_level(u32 id)
{
	u32 val, level;

	/* get level (1 unit = 4 pixels) */
	val = decon_read(id, OF_LEVEL);
	level =  OUTFIFO_FIFO_LEVEL_GET(val);

	decon_dbg("outfifo level = %d(%d pixels)\n", level, level * 4);

	return level;
}

/* need to set once at init time */
void decon_reg_set_latency_monitor_enable(u32 id, u32 en)
{
	u32 val, mask;

	val = en ? ~0 : 0;
	mask = LATENCY_COUNTER_ENABLE;
	decon_write_mask(id, OF_LAT_MON, val, mask);
}

/*
 * @framedone : read -> clear
 * return : ACLK cycle count
 */
u32 decon_reg_get_latency_monitor_value(u32 id)
{
	u32 val, mask;
	u32 count;

	/* get count */
	val = decon_read(id, OF_LAT_MON);
	count =  LATENCY_COUNTER_VALUE_GET(val);

	/* clear */
	val = LATENCY_COUNTER_CLEAR;
	mask = LATENCY_COUNTER_CLEAR;
	decon_write_mask(id, OF_LAT_MON, val, mask);

	decon_dbg("latency_count = %d\n", count);

	return count;
}

void decon_reg_get_data_path(u32 id, enum decon_data_path *d_path,
		enum decon_enhance_path *e_path)
{
	u32 val;

	val = decon_read(id, DATA_PATH_CON);
	*d_path = COMP_OUTIF_PATH_GET(val);
	*e_path = ENHANCE_PATH_GET(val);
}

void decon_reg_set_data_path(u32 id, enum decon_data_path d_path,
		enum decon_enhance_path e_path)
{
	u32 val, mask;

	val = ENHANCE_PATH_F(e_path) | COMP_OUTIF_PATH_F(d_path);
	mask = ENHANCE_PATH_MASK | COMP_OUTIF_PATH_MASK;
	decon_write_mask(id, DATA_PATH_CON, val, mask);
}

void decon_reg_set_cwb_enable(u32 id, u32 en)
{
	u32 val, mask, d_path;

	val = decon_read(id, DATA_PATH_CON);
	d_path = COMP_OUTIF_PATH_GET(val);

	if (en)
		d_path |= CWB_PATH_EN;
	else
		d_path &= ~CWB_PATH_EN;

	mask = COMP_OUTIF_PATH_MASK;
	decon_write_mask(id, DATA_PATH_CON, d_path, mask);

	if (en) {
#if defined(CONFIG_SOC_EXYNOS2100_EVT0)
		decon_write_mask(id, SRAM_EN_OF_SEC_0, ~0,
				SRAM2_EN_F);
#else
		decon_write_mask(id, DATA_PATH_CON,
				CWB_PATH_F(CWB_OUTFIFO1), CWB_PATH_MASK);
		decon_write_mask(id, SRAM_EN_OF_SEC_0, ~0,
				SRAM0_EN_F | SRAM1_EN_F);
#endif
	} else {
#if defined(CONFIG_SOC_EXYNOS2100_EVT0)
		decon_write_mask(id, SRAM_EN_OF_SEC_0, 0,
				SRAM2_EN_F);
#else
		decon_write_mask(id, DATA_PATH_CON,
				0, CWB_PATH_MASK);
		decon_write_mask(id, SRAM_EN_OF_SEC_0, 0,
				SRAM0_EN_F | SRAM1_EN_F);
#endif
	}
}

void decon_reg_cmp_shadow_update_req(u32 id)
{
	u32 mask;

	mask = SHD_REG_UP_REQ_CMP;

	decon_write_mask(id, SHD_REG_UP_REQ, ~0, mask);
}

/*
 * Check major configuration of data_path_control
 *    DSCC[7]
 *    DSC_ENC2[6] DSC_ENC1[5] DSC_ENC0[4]
 *    DPIF[3]
 *    WBIF[2]
 *    DSIMIF1[1] DSIMIF0[0]
 */
static u32 decon_reg_get_data_path_cfg(u32 id, enum decon_path_cfg con_id)
{
	u32 val;
	u32 d_path;
	u32 bRet = 0;

	val = decon_read(id, DATA_PATH_CON);
	d_path = COMP_OUTIF_PATH_GET(val);

	switch (con_id) {
	case PATH_CON_ID_DSCC_EN:
		if (d_path & (0x1 << PATH_CON_ID_DSCC_EN))
			bRet = 1;
		break;
	case PATH_CON_ID_DUAL_DSC:
		if ((d_path & (0x3 << PATH_CON_ID_DUAL_DSC)) == 0x30)
			bRet = 1;
		break;
	case PATH_CON_ID_DP:
		if (d_path & (0x1 << PATH_CON_ID_DP))
			bRet = 1;
		break;
	case PATH_CON_ID_WB:
		if (d_path & (0x1 << PATH_CON_ID_WB))
			bRet = 1;
		break;
	case PATH_CON_ID_DSIM_IF0:
		if (d_path & (0x1 << PATH_CON_ID_DSIM_IF0))
			bRet = 1;
		break;
	case PATH_CON_ID_DSIM_IF1:
		if (d_path & (0x1 << PATH_CON_ID_DSIM_IF1))
			bRet = 1;
		break;
	default:
		break;
	}

	return bRet;
}

/*
 * width : width of updated LCD region
 * height : height of updated LCD region
 * is_dsc : 1: DSC is enabled 0: DSC is disabled
 */
static void decon_reg_set_data_path_size(u32 id, u32 width, u32 height,
		bool is_dsc, u32 dsc_cnt, u32 slice_w, u32 slice_h,
		u32 ds_en[2])
{
	u32 outfifo_w;

	if (is_dsc)
		outfifo_w = slice_w << ds_en[0];
	else
		outfifo_w = width;

	/* OUTFIFO size is compressed size if DSC is enabled */
	decon_reg_set_outfifo_size_ctl0(id, outfifo_w, height);
	if (dsc_cnt == 2)
		decon_reg_set_outfifo_size_ctl1(id, outfifo_w);
	if (is_dsc)
		decon_reg_set_outfifo_size_ctl2(id, slice_w, slice_h);

}

/*
 * 'DATA_PATH_CON' SFR must be set before calling this function!!
 * [width]
 * - no compression  : x-resolution
 * - dsc compression : width_per_enc
 */
static void decon_reg_config_data_path_size(u32 id,
	u32 width, u32 height, u32 overlap_w,
	struct decon_dsc *p, struct decon_param *param)
{
	u32 width_f;
	u32 sw;

	/* OUTFIFO */
	if (param->lcd_info->dsc.en) {
		width_f = p->width_per_enc;
		sw = param->lcd_info->dsc.enc_sw;
		/* DSC 1EA */
		if (param->lcd_info->dsc.cnt == 1) {
			decon_reg_set_outfifo_size_ctl0(id, width_f, height);
			decon_reg_set_outfifo_size_ctl2(id,
					sw, p->slice_height);
		} else if (param->lcd_info->dsc.cnt == 2) {	/* DSC 2EA */
			decon_reg_set_outfifo_size_ctl0(id, width_f, height);
			decon_reg_set_outfifo_size_ctl1(id, width_f);
			decon_reg_set_outfifo_size_ctl2(id,
					sw, p->slice_height);
		}
	} else {
		decon_reg_set_outfifo_size_ctl0(id, width, height);
	}
}

/*
 * [ CAUTION ]
 * 'DATA_PATH_CON' SFR must be set before calling this function!!
 *
 * [ Implementation Info about CONNECTION ]
 * 1) DECON 0 - DSIMIF0
 * 2) DECON 0 - DSIMIF1
 * 3) DECON 1 - DSIMIF0
 * --> modify code if you want to change connection between dsim_if and dsim
 *
 * < Later check >
 * - Dual DSI connection
 *
 */

static void decon_reg_set_interface_dsi(u32 id)
{
	/* connection sfrs are changed in Lhotse */
	u32 dsim_if0 = 1;
	u32 dsim_if1 = 0;
	u32 dual_dsi = 0;

	dsim_if0 = decon_reg_get_data_path_cfg(id, PATH_CON_ID_DSIM_IF0);
	dsim_if1 = decon_reg_get_data_path_cfg(id, PATH_CON_ID_DSIM_IF1);
	if (dsim_if0 && dsim_if1)
		dual_dsi = 1;

	if (dual_dsi) {
		/* decon0 - (dsim_if0-dsim0) */
		dsimif_write(DSIMIF_SEL(0), SEL_DSIM(0));
		dsimif_write(DSIMIF_TE_TIMEOUT_CON(0), 0xffff);
		/* decon0 - (dsim_if1-dsim1) */
		dsimif_write(DSIMIF_SEL(1), SEL_DSIM(1));
		dsimif_write(DSIMIF_TE_TIMEOUT_CON(1), 0xffff);
	} else { /* single dsi : DSIM0 only */
		if (dsim_if0) {
			if (id == 0) {
				/* DECON0 - DSIMIF0 - DSIM0 */
				dsimif_write(DSIMIF_SEL(0), SEL_DSIM(0));
			} else if (id == 1) {
				/* DECON1 - DSIMIF0 - DSIM0 */
				dsimif_write(DSIMIF_SEL(0), SEL_DSIM(2));
			}
			dsimif_write(DSIMIF_TE_TIMEOUT_CON(0), 0xffff);
		}
		if (dsim_if1) {
			if (id == 0) {
				/* DECON0 - DSIMIF1 - DSIM0 */
				dsimif_write(DSIMIF_SEL(1), SEL_DSIM(0));
			}
			dsimif_write(DSIMIF_TE_TIMEOUT_CON(1), 0xffff);
		}
	}
}

#if defined(CONFIG_EXYNOS_DISPLAYPORT)
static void decon_reg_set_interface_dp(u32 id)
{
	/* for MST support */
	if (id == SST1_DECON_ID) {
		decon_write(id, DATA_PATH_CON, NOCOMP_DPIF);
		/* DECON2 - DPIF2 - DP0 */
		dpif_write(DPIF_SEL(2), SEL_DP(2));
	} else if (id == SST2_DECON_ID) {
		decon_write(id, DATA_PATH_CON, NOCOMP_DPIF);
		/* DECON3 - DPIF3 - DP1 */
		dpif_write(DPIF_SEL(3), SEL_DP(3));
	}

	decon_dbg("DPIF2_SEL = 0x%x\n", dpif_read(DPIF_SEL(2)));
	decon_dbg("DPIF3_SEL = 0x%x\n", dpif_read(DPIF_SEL(3)));
}
#else
static inline void decon_reg_set_interface_dp(u32 id) { }
#endif

static void decon_reg_set_bpc(u32 id, struct exynos_panel_info *lcd_info)
{
	u32 val = 0, mask;

	if (lcd_info->bpc == 10)
		val = GLOBAL_CON_TEN_BPC_MODE_F;

	mask = GLOBAL_CON_TEN_BPC_MODE_MASK;

	decon_write_mask(id, GLOBAL_CON, val, mask);
}

static void decon_reg_config_win_channel(u32 id, u32 win_idx, int ch)
{
	u32 val, mask;

	val = WIN_CHMAP_F(ch);
	mask = WIN_CHMAP_MASK;
	wincon_write_mask(id, DECON_CON_WIN(win_idx), val, mask);
}

static void decon_reg_configure_trigger(u32 id, enum decon_trig_mode mode)
{
	u32 val, mask;

	mask = HW_TRIG_EN;
	val = (mode == DECON_SW_TRIG) ? 0 : ~0;
	decon_write_mask(id, TRIG_CON, val, mask);
}

/*
 * wakeup_us : usec unit
 * cnt : TE rising ~ expire
 * (example)
 *    if 60fps, TE period = 16666us(=1/fps) & wakeup_us = 100
 *    cnt = (16666 - 100) time = 16566us
 *    <meaning> wakeup at 16.566ms after TE rising
 */
#if defined(CONFIG_EXYNOS_EWR)
static u32 decon_get_ewr_cycle(int fps, int wakeup_us)
{
	u32 cnt;

	cnt = ((1000000 / fps) - wakeup_us) * 26;
	decon_dbg("%s: ewr_cnt = %d @ %dfps\n", __func__, cnt, fps);

	return cnt;
}

static void decon_reg_set_ewr_enable(u32 id, u32 en)
{
	u32 val, mask;

	mask = EWR_EN_F;
	val = en ? ~0 : 0;
	decon_write_mask(id, EWR_CON, val, mask);
}

static void decon_reg_set_ewr_timer(u32 id, u32 cnt)
{
	decon_write(id, EWR_TIMER, cnt);
}

static void decon_reg_set_ewr_control(u32 id, u32 cnt, u32 en)
{
	decon_reg_set_ewr_timer(id, cnt);
	decon_reg_set_ewr_enable(id, en);
}
void decon_reg_update_ewr_control(u32 id, u32 fps)
{
	u32 cnt = decon_get_ewr_cycle(fps, 100);

	decon_reg_set_ewr_timer(id, cnt);
}
#endif

static void dsc_reg_swreset(u32 dsc_id)
{
	dsc_write_mask(DSC_CONTROL1(dsc_id), 1, DSC_SW_RESET);
}

static void dsc_reg_set_slice_mode_change(u32 dsc_id, u32 en)
{
	u32 val;

	val = DSC_SLICE_MODE_CH_F(en);
	dsc_write_mask(DSC_CONTROL1(dsc_id), val, DSC_SLICE_MODE_CH_MASK);
}

static void dsc_reg_set_dual_slice(u32 dsc_id, u32 en)
{
	u32 val;

	val = DSC_DUAL_SLICE_EN_F(en);
	dsc_write_mask(DSC_CONTROL1(dsc_id), val, DSC_DUAL_SLICE_EN_MASK);
}

/*
 * dsc PPS Configuration
 */

/*
 * APIs which user setting or calculation is required are implemented
 * - PPS04 ~ PPS35 except reserved
 * - PPS58 ~ PPS59
 */
static void dsc_reg_set_pps_06_07_picture_height(u32 dsc_id, u32 height)
{
	u32 val, mask;

	val = PPS06_07_PIC_HEIGHT(height);
	mask = PPS06_07_PIC_HEIGHT_MASK;
	dsc_write_mask(DSC_PPS04_07(dsc_id), val, mask);
}

static void dsc_reg_set_pps_58_59_rc_range_param0(u32 dsc_id,
		u32 rc_range_param)
{
	u32 val, mask;

	val = PPS58_59_RC_RANGE_PARAM(rc_range_param);
	mask = PPS58_59_RC_RANGE_PARAM_MASK;
	dsc_write_mask(DSC_PPS56_59(dsc_id), val, mask);
}

/* full size default value */
static u32 dsc_get_dual_slice_mode(struct exynos_panel_info *lcd_info)
{
	u32 dual_slice_en = 0;

	if (lcd_info->dsc.cnt == 1) {
		if (lcd_info->dsc.slice_num == 2)
			dual_slice_en = 1;
	} else if (lcd_info->dsc.cnt == 2) {
		if (lcd_info->dsc.slice_num == 4)
			dual_slice_en = 1;
	} else {
		dual_slice_en = 0;
	}

	return dual_slice_en;
}

/* full size default value */
static u32 dsc_get_slice_mode_change(struct exynos_panel_info *lcd_info)
{
	u32 slice_mode_ch = 0;

	if (lcd_info->dsc.cnt == 2) {
		if (lcd_info->dsc.slice_num == 2)
			slice_mode_ch = 1;
	}

	return slice_mode_ch;
}

static void dsc_get_partial_update_info(u32 slice_cnt, u32 dsc_cnt,
		bool in_slice[4], u32 ds_en[2], u32 sm_ch[2])
{
	switch (slice_cnt) {
	case 4:
		if ((in_slice[0] + in_slice[1]) % 2) {
			ds_en[DECON_DSC_ENC0] = 0;
			sm_ch[DECON_DSC_ENC0] = 1;
		} else {
			ds_en[DECON_DSC_ENC0] = 1;
			sm_ch[DECON_DSC_ENC0] = 0;
		}

		if ((in_slice[2] + in_slice[3]) % 2) {
			ds_en[DECON_DSC_ENC1] = 0;
			sm_ch[DECON_DSC_ENC1] = 1;
		} else {
			ds_en[DECON_DSC_ENC1] = 1;
			sm_ch[DECON_DSC_ENC1] = 0;
		}

		break;
	case 2:
		if (dsc_cnt == 2) {
			ds_en[DECON_DSC_ENC0] = 0;
			sm_ch[DECON_DSC_ENC0] = 1;

			ds_en[DECON_DSC_ENC1] = 0;
			sm_ch[DECON_DSC_ENC1] = 1;
		} else {
			ds_en[DECON_DSC_ENC0] = 1;
			sm_ch[DECON_DSC_ENC0] = 0;
			ds_en[DECON_DSC_ENC1] = ds_en[DECON_DSC_ENC0];
			sm_ch[DECON_DSC_ENC1] = sm_ch[DECON_DSC_ENC0];
		}
		break;
	case 1:
		ds_en[DECON_DSC_ENC0] = 0;
		sm_ch[DECON_DSC_ENC0] = 0;

		ds_en[DECON_DSC_ENC1] = 0;
		sm_ch[DECON_DSC_ENC1] = 0;
		break;
	default:
		decon_err("Not specified case for Partial Update in DSC!\n");
		break;
	}
}

static void dsc_reg_config_control(u32 dsc_id, u32 ds_en, u32 sm_ch,
		u32 slice_width)
{
	u32 val;
	u32 remainder, grpcntline;

	val = DSC_SWAP(0x0, 0x1, 0x0);
	val |= DSC_DUAL_SLICE_EN_F(ds_en);
	val |= DSC_SLICE_MODE_CH_F(sm_ch);
	val |= DSC_FLATNESS_DET_TH_F(0x2);
	dsc_write(DSC_CONTROL1(dsc_id), val);

	if (slice_width % 3)
		remainder = slice_width % 3;
	else
		remainder = 3;

	grpcntline = (slice_width + 2) / 3;

	val = DSC_REMAINDER_F(remainder) | DSC_GRPCNTLINE_F(grpcntline);
	dsc_write(DSC_CONTROL3(dsc_id), val);
}

/*
 * overlap_w
 * - default : 0
 * - range : [0, 32] & (multiples of 2)
 *    if non-zero value is applied, this means slice_w increasing.
 *    therefore, DECON & DSIM setting must also be aligned.
 *    --> must check if DDI module is supporting this feature !!!
 */
#define NUM_EXTRA_MUX_BITS	246
static void dsc_calc_pps_info(struct exynos_panel_info *lcd_info, u32 dscc_en,
	struct decon_dsc *dsc_enc)
{
	u32 width, height;
	u32 slice_width, slice_height;
	u32 pic_width, pic_height;
	u32 width_eff;
	u32 dual_slice_en = 0;
	u32 bpp, chunk_size;
	u32 slice_bits;
	u32 groups_per_line, groups_total;

	/* initial values, also used for other pps calcualtion */
	const u32 rc_model_size = 0x2000;
	u32 num_extra_mux_bits = NUM_EXTRA_MUX_BITS;
	const u32 initial_xmit_delay = 0x200;
	const u32 initial_dec_delay = 0x4c0;
	/* when 'slice_w >= 70' */
	const u32 initial_scale_value = 0x20;
	const u32 first_line_bpg_offset = 0x0c;
	const u32 initial_offset = 0x1800;
	const u32 rc_range_parameters = 0x0102;

	u32 final_offset, final_scale;
	u32 flag, nfl_bpg_offset, slice_bpg_offset;
	u32 scale_increment_interval, scale_decrement_interval;
	u32 slice_width_byte_unit, comp_slice_width_byte_unit;
	u32 comp_slice_width_pixel_unit;
	u32 overlap_w = 0;
	u32 dsc_enc0_w = 0;
	u32 dsc_enc1_w = 0;
	u32 i, j;

	width = lcd_info->xres;
	height = lcd_info->yres;

	overlap_w = dsc_enc->overlap_w;

	if (dscc_en)
		/* OVERLAP can be used in the dual-slice case (if one ENC) */
		width_eff = (width >> 1) + overlap_w;
	else
		width_eff = width + overlap_w;

	pic_width = width_eff;
	dual_slice_en = dsc_get_dual_slice_mode(lcd_info);
	if (dual_slice_en)
		slice_width = width_eff >> 1;
	else
		slice_width = width_eff;

	pic_height = height;
	slice_height = lcd_info->dsc.slice_h;

	bpp = 8;
	chunk_size = slice_width;
	slice_bits = 8 * chunk_size * slice_height;

	while ((slice_bits - num_extra_mux_bits) % 48)
		num_extra_mux_bits--;

	groups_per_line = (slice_width + 2) / 3;
	groups_total = groups_per_line * slice_height;

	final_offset = rc_model_size - ((initial_xmit_delay * (8<<4) + 8)>>4)
		+ num_extra_mux_bits;
	final_scale = 8 * rc_model_size / (rc_model_size - final_offset);

	flag = (first_line_bpg_offset * 2048) % (slice_height - 1);
	nfl_bpg_offset = (first_line_bpg_offset * 2048) / (slice_height - 1);
	if (flag)
		nfl_bpg_offset = nfl_bpg_offset + 1;

	flag = 2048 * (rc_model_size - initial_offset + num_extra_mux_bits)
		% groups_total;
	slice_bpg_offset = 2048
		* (rc_model_size - initial_offset + num_extra_mux_bits)
		/ groups_total;
	if (flag)
		slice_bpg_offset = slice_bpg_offset + 1;

	scale_increment_interval = (2048 * final_offset) / ((final_scale - 9)
		* (nfl_bpg_offset + slice_bpg_offset));
	scale_decrement_interval = groups_per_line / (initial_scale_value - 8);

	/* 3bytes per pixel */
	slice_width_byte_unit = slice_width * 3;
	/* integer value, /3 for 1/3 compression */
	comp_slice_width_byte_unit = slice_width_byte_unit / 3;
	/* integer value, /3 for pixel unit */
	comp_slice_width_pixel_unit = comp_slice_width_byte_unit / 3;

	i = comp_slice_width_byte_unit % 3;
	j = comp_slice_width_pixel_unit % 2;

	if (i == 0 && j == 0) {
		dsc_enc0_w = comp_slice_width_pixel_unit;
		if (dscc_en)
			dsc_enc1_w = comp_slice_width_pixel_unit;
	} else if (i == 0 && j != 0) {
		dsc_enc0_w = comp_slice_width_pixel_unit + 1;
		if (dscc_en)
			dsc_enc1_w = comp_slice_width_pixel_unit + 1;
	} else if (i != 0) {
		while (1) {
			comp_slice_width_pixel_unit++;
			j = comp_slice_width_pixel_unit % 2;
			if (j == 0)
				break;
		}
		dsc_enc0_w = comp_slice_width_pixel_unit;
		if (dscc_en)
			dsc_enc1_w = comp_slice_width_pixel_unit;
	}

	if (dual_slice_en) {
		dsc_enc0_w = dsc_enc0_w * 2;
		if (dscc_en)
			dsc_enc1_w = dsc_enc1_w * 2;
	}

	/* Save information to structure variable */
	dsc_enc->comp_cfg = 0x30;
	dsc_enc->bit_per_pixel = bpp << 4;
	dsc_enc->pic_height = pic_height;
	dsc_enc->pic_width = pic_width;
	dsc_enc->slice_height = slice_height;
	dsc_enc->slice_width = slice_width;
	dsc_enc->chunk_size = chunk_size;
	dsc_enc->initial_xmit_delay = initial_xmit_delay;
	dsc_enc->initial_dec_delay = initial_dec_delay;
	dsc_enc->initial_scale_value = initial_scale_value;
	dsc_enc->scale_increment_interval = scale_increment_interval;
	dsc_enc->scale_decrement_interval = scale_decrement_interval;
	dsc_enc->first_line_bpg_offset = first_line_bpg_offset;
	dsc_enc->nfl_bpg_offset = nfl_bpg_offset;
	dsc_enc->slice_bpg_offset = slice_bpg_offset;
	dsc_enc->initial_offset = initial_offset;
	dsc_enc->final_offset = final_offset;
	dsc_enc->rc_range_parameters = rc_range_parameters;

	dsc_enc->width_per_enc = dsc_enc0_w;
}

static void dsc_reg_set_pps(u32 dsc_id, struct decon_dsc *dsc_enc)
{
	u32 val;
	u32 initial_dec_delay;

	val = PPS04_COMP_CFG(dsc_enc->comp_cfg);
	val |= PPS05_BPP(dsc_enc->bit_per_pixel);
	val |= PPS06_07_PIC_HEIGHT(dsc_enc->pic_height);
	dsc_write(DSC_PPS04_07(dsc_id), val);

	val = PPS08_09_PIC_WIDHT(dsc_enc->pic_width);
	val |= PPS10_11_SLICE_HEIGHT(dsc_enc->slice_height);
	dsc_write(DSC_PPS08_11(dsc_id), val);

	val = PPS12_13_SLICE_WIDTH(dsc_enc->slice_width);
	val |= PPS14_15_CHUNK_SIZE(dsc_enc->chunk_size);
	dsc_write(DSC_PPS12_15(dsc_id), val);

#ifndef VESA_SCR_V4
	initial_dec_delay = 0x01B4;
#else
	initial_dec_delay = dsc_enc->initial_dec_delay;
#endif
	val = PPS18_19_INIT_DEC_DELAY(initial_dec_delay);
	val |= PPS16_17_INIT_XMIT_DELAY(dsc_enc->initial_xmit_delay);
	dsc_write(DSC_PPS16_19(dsc_id), val);

	val = PPS21_INIT_SCALE_VALUE(dsc_enc->initial_scale_value);
	val |= PPS22_23_SCALE_INC_INTERVAL(dsc_enc->scale_increment_interval);
	dsc_write(DSC_PPS20_23(dsc_id), val);

	val = PPS24_25_SCALE_DEC_INTERVAL(dsc_enc->scale_decrement_interval);
	val |= PPS27_FL_BPG_OFFSET(dsc_enc->first_line_bpg_offset);
	dsc_write(DSC_PPS24_27(dsc_id), val);

	val = PPS28_29_NFL_BPG_OFFSET(dsc_enc->nfl_bpg_offset);
	val |= PPS30_31_SLICE_BPG_OFFSET(dsc_enc->slice_bpg_offset);
	dsc_write(DSC_PPS28_31(dsc_id), val);

	val = PPS32_33_INIT_OFFSET(dsc_enc->initial_offset);
	val |= PPS34_35_FINAL_OFFSET(dsc_enc->final_offset);
	dsc_write(DSC_PPS32_35(dsc_id), val);

	/* min_qp0 = 0 , max_qp0 = 4 , bpg_off0 = 2 */
	dsc_reg_set_pps_58_59_rc_range_param0(dsc_id,
		dsc_enc->rc_range_parameters);

#ifndef VESA_SCR_V4
	/* PPS79 ~ PPS87 : 3HF4 is different with VESA SCR v4 */
	dsc_write(DSC_PPS76_79(dsc_id), 0x1AB62AF6);
	dsc_write(DSC_PPS80_83(dsc_id), 0x2B342B74);
	dsc_write(DSC_PPS84_87(dsc_id), 0x3B746BF4);
#endif
}

/*
 * Following PPS SFRs will be set from DDI PPS Table (DSC Decoder)
 * : not 'fix' type
 *   - PPS04 ~ PPS35
 *   - PPS58 ~ PPS59
 *   <PPS Table e.g.> SEQ_PPS_SLICE4[] @ s6e3hf4_param.h
 */
static void dsc_get_decoder_pps_info(struct decon_dsc *dsc_dec,
		const unsigned char pps_t[90])
{
	dsc_dec->comp_cfg = (u32) pps_t[4];
	dsc_dec->bit_per_pixel = (u32) pps_t[5];
	dsc_dec->pic_height = (u32) (pps_t[6] << 8 | pps_t[7]);
	dsc_dec->pic_width = (u32) (pps_t[8] << 8 | pps_t[9]);
	dsc_dec->slice_height = (u32) (pps_t[10] << 8 | pps_t[11]);
	dsc_dec->slice_width = (u32) (pps_t[12] << 8 | pps_t[13]);
	dsc_dec->chunk_size = (u32) (pps_t[14] << 8 | pps_t[15]);
	dsc_dec->initial_xmit_delay = (u32) (pps_t[16] << 8 | pps_t[17]);
	dsc_dec->initial_dec_delay = (u32) (pps_t[18] << 8 | pps_t[19]);
	dsc_dec->initial_scale_value = (u32) pps_t[21];
	dsc_dec->scale_increment_interval = (u32) (pps_t[22] << 8 | pps_t[23]);
	dsc_dec->scale_decrement_interval = (u32) (pps_t[24] << 8 | pps_t[25]);
	dsc_dec->first_line_bpg_offset = (u32) pps_t[27];
	dsc_dec->nfl_bpg_offset = (u32) (pps_t[28] << 8 | pps_t[29]);
	dsc_dec->slice_bpg_offset = (u32) (pps_t[30] << 8 | pps_t[31]);
	dsc_dec->initial_offset = (u32) (pps_t[32] << 8 | pps_t[33]);
	dsc_dec->final_offset = (u32) (pps_t[34] << 8 | pps_t[35]);
	dsc_dec->rc_range_parameters = (u32) (pps_t[58] << 8 | pps_t[59]);
}

static u32 dsc_cmp_pps_enc_dec(struct decon_dsc *p_enc, struct decon_dsc *p_dec)
{
	u32 diff_cnt = 0;

	if (p_enc->comp_cfg != p_dec->comp_cfg) {
		diff_cnt++;
		decon_dbg("[dsc_pps] comp_cfg (enc:dec = %d:%d)\n",
			p_enc->comp_cfg, p_dec->comp_cfg);
	}
	if (p_enc->bit_per_pixel != p_dec->bit_per_pixel) {
		diff_cnt++;
		decon_dbg("[dsc_pps] bit_per_pixel (enc:dec = %d:%d)\n",
			p_enc->bit_per_pixel, p_dec->bit_per_pixel);
	}
	if (p_enc->pic_height != p_dec->pic_height) {
		diff_cnt++;
		decon_dbg("[dsc_pps] pic_height (enc:dec = %d:%d)\n",
			p_enc->pic_height, p_dec->pic_height);
	}
	if (p_enc->pic_width != p_dec->pic_width) {
		diff_cnt++;
		decon_dbg("[dsc_pps] pic_width (enc:dec = %d:%d)\n",
			p_enc->pic_width, p_dec->pic_width);
	}
	if (p_enc->slice_height != p_dec->slice_height) {
		diff_cnt++;
		decon_dbg("[dsc_pps] slice_height (enc:dec = %d:%d)\n",
			p_enc->slice_height, p_dec->slice_height);
	}
	if (p_enc->slice_width != p_dec->slice_width) {
		diff_cnt++;
		decon_dbg("[dsc_pps] slice_width (enc:dec = %d:%d)\n",
			p_enc->slice_width, p_dec->slice_width);
	}
	if (p_enc->chunk_size != p_dec->chunk_size) {
		diff_cnt++;
		decon_dbg("[dsc_pps] chunk_size (enc:dec = %d:%d)\n",
			p_enc->chunk_size, p_dec->chunk_size);
	}
	if (p_enc->initial_xmit_delay != p_dec->initial_xmit_delay) {
		diff_cnt++;
		decon_dbg("[dsc_pps] initial_xmit_delay (enc:dec = %d:%d)\n",
			p_enc->initial_xmit_delay, p_dec->initial_xmit_delay);
	}
	if (p_enc->initial_dec_delay != p_dec->initial_dec_delay) {
		diff_cnt++;
		decon_dbg("[dsc_pps] initial_dec_delay (enc:dec = %d:%d)\n",
			p_enc->initial_dec_delay, p_dec->initial_dec_delay);
	}
	if (p_enc->initial_scale_value != p_dec->initial_scale_value) {
		diff_cnt++;
		decon_dbg("[dsc_pps] initial_scale_value (enc:dec = %d:%d)\n",
			p_enc->initial_scale_value,
			p_dec->initial_scale_value);
	}
	if (p_enc->scale_increment_interval !=
			p_dec->scale_increment_interval) {
		diff_cnt++;
		decon_dbg("[dsc_pps] scale_inc_interval (enc:dec = %d:%d)\n",
					p_enc->scale_increment_interval,
					p_dec->scale_increment_interval);
	}
	if (p_enc->scale_decrement_interval !=
			p_dec->scale_decrement_interval) {
		diff_cnt++;
		decon_dbg("[dsc_pps] scale_dec_interval (enc:dec = %d:%d)\n",
					p_enc->scale_decrement_interval,
					p_dec->scale_decrement_interval);
	}
	if (p_enc->first_line_bpg_offset != p_dec->first_line_bpg_offset) {
		diff_cnt++;
		decon_dbg("[dsc_pps] first_line_bpg_offset (enc:dec = %d:%d)\n",
					p_enc->first_line_bpg_offset,
					p_dec->first_line_bpg_offset);
	}
	if (p_enc->nfl_bpg_offset != p_dec->nfl_bpg_offset) {
		diff_cnt++;
		decon_dbg("[dsc_pps] nfl_bpg_offset (enc:dec = %d:%d)\n",
			p_enc->nfl_bpg_offset, p_dec->nfl_bpg_offset);
	}
	if (p_enc->slice_bpg_offset != p_dec->slice_bpg_offset) {
		diff_cnt++;
		decon_dbg("[dsc_pps] slice_bpg_offset (enc:dec = %d:%d)\n",
			p_enc->slice_bpg_offset, p_dec->slice_bpg_offset);
	}
	if (p_enc->initial_offset != p_dec->initial_offset) {
		diff_cnt++;
		decon_dbg("[dsc_pps] initial_offset (enc:dec = %d:%d)\n",
			p_enc->initial_offset, p_dec->initial_offset);
	}
	if (p_enc->final_offset != p_dec->final_offset) {
		diff_cnt++;
		decon_dbg("[dsc_pps] final_offset (enc:dec = %d:%d)\n",
			p_enc->final_offset, p_dec->final_offset);
	}
	if (p_enc->rc_range_parameters != p_dec->rc_range_parameters) {
		diff_cnt++;
		decon_dbg("[dsc_pps] rc_range_parameters (enc:dec = %d:%d)\n",
						p_enc->rc_range_parameters,
						p_dec->rc_range_parameters);
	}

	decon_dbg("[dsc_pps] total different count : %d\n", diff_cnt);

	return diff_cnt;
}

static void dsc_reg_set_partial_update(u32 dsc_id, u32 dual_slice_en,
	u32 slice_mode_ch, u32 pic_h)
{
	/*
	 * Following SFRs must be considered
	 * - dual_slice_en
	 * - slice_mode_change
	 * - picture_height
	 * - picture_width (don't care @KC) : decided by DSI (-> dual: /2)
	 */
	dsc_reg_set_dual_slice(dsc_id, dual_slice_en);
	dsc_reg_set_slice_mode_change(dsc_id, slice_mode_ch);
	dsc_reg_set_pps_06_07_picture_height(dsc_id, pic_h);
}

/*
 * This table is only used to check DSC setting value when debugging
 * Copy or Replace table's data from current using LCD information
 * ( e.g. : SEQ_PPS_SLICE4 @ s6e3hf4_param.h )
 */
static const unsigned char DDI_PPS_INFO[] = {
	0x11, 0x00, 0x00, 0x89, 0x30,
	0x80, 0x0A, 0x00, 0x05, 0xA0,
	0x00, 0x40, 0x01, 0x68, 0x01,
	0x68, 0x02, 0x00, 0x01, 0xB4,

	0x00, 0x20, 0x04, 0xF2, 0x00,
	0x05, 0x00, 0x0C, 0x01, 0x87,
	0x02, 0x63, 0x18, 0x00, 0x10,
	0xF0, 0x03, 0x0C, 0x20, 0x00,

	0x06, 0x0B, 0x0B, 0x33, 0x0E,
	0x1C, 0x2A, 0x38, 0x46, 0x54,
	0x62, 0x69, 0x70, 0x77, 0x79,
	0x7B, 0x7D, 0x7E, 0x01, 0x02,

	0x01, 0x00, 0x09, 0x40, 0x09,
	0xBE, 0x19, 0xFC, 0x19, 0xFA,
	0x19, 0xF8, 0x1A, 0x38, 0x1A,
	0x78, 0x1A, 0xB6, 0x2A, 0xF6,

	0x2B, 0x34, 0x2B, 0x74, 0x3B,
	0x74, 0x6B, 0xF4, 0x00, 0x00
};

static void dsc_reg_set_encoder(u32 id, struct decon_param *p,
	struct decon_dsc *dsc_enc, u32 chk_en)
{
	u32 dsc_id;
	u32 ds_en = 0;
	u32 sm_ch = 0;
	struct exynos_panel_info *lcd_info = p->lcd_info;
	/* DDI PPS table : for compare with ENC PPS value */
	struct decon_dsc dsc_dec;
	/* set corresponding table like 'SEQ_PPS_SLICE4' */
	const unsigned char *pps_t = DDI_PPS_INFO;

	ds_en = dsc_get_dual_slice_mode(lcd_info);
	decon_dbg("dual slice(%d)\n", ds_en);

	sm_ch = dsc_get_slice_mode_change(lcd_info);
	decon_dbg("slice mode change(%d)\n", sm_ch);

	dsc_calc_pps_info(lcd_info, (lcd_info->dsc.cnt == 2) ? 1 : 0, dsc_enc);

	if (id == 1) {
		dsc_reg_config_control(DECON_DSC_ENC1, ds_en, sm_ch,
				dsc_enc->slice_width);
		dsc_reg_set_pps(DECON_DSC_ENC1, dsc_enc);
	} else if (id == 2) {	/* only for DP */
		dsc_reg_config_control(DECON_DSC_ENC2, ds_en, sm_ch,
				dsc_enc->slice_width);
		dsc_reg_set_pps(DECON_DSC_ENC2, dsc_enc);
	} else {
		for (dsc_id = 0; dsc_id < lcd_info->dsc.cnt; dsc_id++) {
			dsc_reg_config_control(dsc_id, ds_en, sm_ch,
					dsc_enc->slice_width);
			dsc_reg_set_pps(dsc_id, dsc_enc);
		}
	}

	if (chk_en) {
		dsc_get_decoder_pps_info(&dsc_dec, pps_t);
		if (dsc_cmp_pps_enc_dec(dsc_enc, &dsc_dec))
			decon_dbg("[WARNING] Check PPS value!!\n");
	}

}

static int dsc_reg_init(u32 id, struct decon_param *p, u32 overlap_w, u32 swrst)
{
	u32 dsc_id;
	struct exynos_panel_info *lcd_info = p->lcd_info;
	struct decon_dsc dsc_enc;

	/* Basically, all SW-resets in DPU are not necessary */
	if (swrst) {
		for (dsc_id = 0; dsc_id < lcd_info->dsc.cnt; dsc_id++)
			dsc_reg_swreset(dsc_id);
	}

	dsc_enc.overlap_w = overlap_w;
	dsc_reg_set_encoder(id, p, &dsc_enc, 0);
	decon_reg_config_data_path_size(id,
		dsc_enc.width_per_enc, lcd_info->yres, overlap_w, &dsc_enc, p);

	return 0;
}

static void decon_reg_clear_int_all(u32 id)
{
	u32 mask;

	mask = (INT_EN_FRAME_DONE
			| INT_EN_FRAME_START);
	decon_write_mask(id, DECON_INT_PEND, ~0, mask);

	mask = (INT_EN_RESOURCE_CONFLICT | INT_EN_TIME_OUT);
	decon_write_mask(id, DECON_INT_PEND_EXTRA, ~0, mask);
}

static void decon_reg_configure_lcd(u32 id, struct decon_param *p)
{
	u32 overlap_w = 0;
	enum decon_data_path d_path = DPATH_DSCENC0_OUTFIFO0_DSIMIF0;
	enum decon_enhance_path e_path = ENHANCEPATH_ENHANCE_ALL_OFF;

	struct exynos_panel_info *lcd_info = p->lcd_info;
	struct decon_mode_info *psr = &p->psr;
	enum decon_dsi_mode dsi_mode = psr->dsi_mode;
	enum decon_rgb_order rgb_order = DECON_RGB;

	if ((psr->out_type == DECON_OUT_DSI)
		&& !(lcd_info->dsc.en))
		rgb_order = DECON_BGR;
	else
		rgb_order = DECON_RGB;
	decon_reg_set_rgb_order(id, rgb_order);

	if (lcd_info->dsc.en) {
		if (lcd_info->dsc.cnt == 1)
			d_path = (id == 0) ?
				DPATH_DSCENC0_OUTFIFO0_DSIMIF0 :
				DECON2_DSCENC2_OUTFIFO2_DPIF;
		else if (lcd_info->dsc.cnt == 2 && !id)
			d_path = DPATH_DSCC_DSCENC01_OUTFIFO01_DSIMIF0;
		else
			decon_err("[decon%d] dsc_cnt=%d : not supported\n",
				id, lcd_info->dsc.cnt);

		decon_reg_set_data_path(id, d_path, e_path);
		/* call decon_reg_config_data_path_size () inside */
		dsc_reg_init(id, p, overlap_w, 0);

		decon_reg_cmp_shadow_update_req(id);
	} else {
		if (dsi_mode == DSI_MODE_DUAL_DSI)
			d_path = DPATH_NOCOMP_SPLITTER_OUTFIFO01_DSIMIF01;
		else
			d_path = (id == 0) ?
				DPATH_NOCOMP_OUTFIFO0_DSIMIF0 :
				DECON2_NOCOMP_OUTFIFO2_DPIF;

		decon_reg_set_data_path(id, d_path, e_path);

		decon_reg_config_data_path_size(id,
			lcd_info->xres, lcd_info->yres, overlap_w, NULL, p);

		if (psr->out_type == DECON_OUT_DP)
			decon_reg_set_bpc(id, lcd_info);
	}

	decon_reg_per_frame_off(id);
}

static void decon_reg_set_blender_bg_size(u32 id, enum decon_dsi_mode dsi_mode,
		u32 bg_w, u32 bg_h)
{
	u32 width, val;

	width = bg_w;
	if (dsi_mode == DSI_MODE_DUAL_DSI)
		width = width * 2;

	val = BLENDER_BG_HEIGHT_F(bg_h) | BLENDER_BG_WIDTH_F(width);
	decon_write(id, BLD_BG_IMG_SIZE_PRI, val);
}

static void decon_reg_init_probe(u32 id, u32 dsi_idx, struct decon_param *p)
{
	struct exynos_panel_info *lcd_info = p->lcd_info;
	struct decon_mode_info *psr = &p->psr;
	enum decon_data_path d_path = DPATH_DSCENC0_OUTFIFO0_DSIMIF0;
	enum decon_enhance_path e_path = ENHANCEPATH_ENHANCE_ALL_OFF;
	enum decon_rgb_order rgb_order = DECON_RGB;
	enum decon_dsi_mode dsi_mode = psr->dsi_mode;
	u32 overlap_w = 0; /* default=0 : range=[0, 32] & (multiples of 2) */

	decon_reg_set_clkgate_mode(id, 0);

	decon_reg_set_sram_enable(id);

	decon_reg_set_operation_mode(id, psr->psr_mode);

	decon_reg_set_blender_bg_size(id, psr->dsi_mode,
					lcd_info->xres, lcd_info->yres);

#if defined(CONFIG_EXYNOS_LATENCY_MONITOR)
	/* once enable at init */
	decon_reg_set_latency_monitor_enable(id, 1);
#endif

	/*
	 * same as decon_reg_configure_lcd(...) function
	 * except using decon_reg_update_req_global(id)
	 * instead of decon_reg_direct_on_off(id, 0)
	 */
	if (lcd_info->dsc.en)
		rgb_order = DECON_RGB;
	else
		rgb_order = DECON_BGR;
	decon_reg_set_rgb_order(id, rgb_order);

	if (lcd_info->dsc.en) {
		if (lcd_info->dsc.cnt == 1)
			d_path = (id == 0) ?
				DPATH_DSCENC0_OUTFIFO0_DSIMIF0 :
				DECON2_DSCENC2_OUTFIFO2_DPIF;
		else if (lcd_info->dsc.cnt == 2 && !id)
			d_path = DPATH_DSCC_DSCENC01_OUTFIFO01_DSIMIF0;
		else
			decon_err("[decon%d] dsc_cnt=%d : not supported\n",
				id, lcd_info->dsc.cnt);

		decon_reg_set_data_path(id, d_path, e_path);
		/* call decon_reg_config_data_path_size () inside */
		dsc_reg_init(id, p, overlap_w, 0);
	} else {
		if (dsi_mode == DSI_MODE_DUAL_DSI)
			d_path = DPATH_NOCOMP_SPLITTER_OUTFIFO01_DSIMIF01;
		else
			d_path = (id == 0) ?
				DPATH_NOCOMP_OUTFIFO0_DSIMIF0 :
				DECON2_NOCOMP_OUTFIFO2_DPIF;

		decon_reg_set_data_path(id, d_path, e_path);

		decon_reg_config_data_path_size(id,
			lcd_info->xres, lcd_info->yres, overlap_w, NULL, p);
	}
}

static int decon_reg_stop_perframe(u32 id, u32 dsi_idx,
		struct decon_mode_info *psr, u32 fps)
{
	int ret = 0;
	int timeout_value = 0;
#if defined(CONFIG_EXYNOS_DISPLAYPORT)
	u32 sst_id = SST1;
#endif

	decon_dbg("%s +\n", __func__);

	if ((psr->psr_mode == DECON_MIPI_COMMAND_MODE) &&
			(psr->trig_mode == DECON_HW_TRIG)) {
		decon_reg_set_trigger(id, psr, DECON_TRIG_DISABLE);
	}

	/* perframe stop */
	decon_reg_per_frame_off(id);
	decon_reg_update_req_global(id);

	/* timeout : 1 / fps + 20% margin */
	timeout_value = 1000 / fps * 12 / 10 + 5;
	ret = decon_reg_wait_run_is_off_timeout(id, timeout_value * MSEC);

#if defined(CONFIG_EXYNOS_DISPLAYPORT)
	if (psr->out_type == DECON_OUT_DP) {
		sst_id = displayport_get_sst_id_with_decon_id(id);

		displayport_reg_lh_p_ch_power(sst_id, 0);
	}
#endif

	decon_dbg("%s -\n", __func__);
	return ret;
}

int decon_reg_stop_inst(u32 id, u32 dsi_idx, struct decon_mode_info *psr,
		u32 fps)
{
	int ret = 0;
	int timeout_value = 0;
#if defined(CONFIG_EXYNOS_DISPLAYPORT)
	u32 sst_id = SST1;
#endif

	decon_dbg("%s +\n", __func__);

	if ((psr->psr_mode == DECON_MIPI_COMMAND_MODE) &&
			(psr->trig_mode == DECON_HW_TRIG)) {
		decon_reg_set_trigger(id, psr, DECON_TRIG_DISABLE);
	}

	/* instant stop */
	decon_reg_direct_on_off(id, 0);
	decon_reg_update_req_global(id);

#if defined(CONFIG_EXYNOS_DISPLAYPORT)
	if (psr->out_type == DECON_OUT_DP) {
		sst_id = displayport_get_sst_id_with_decon_id(id);

		displayport_reg_lh_p_ch_power(sst_id, 0);
	}
#endif

	/* timeout : 1 / fps + 20% margin */
	timeout_value = 1000 / fps * 12 / 10 + 5;
	ret = decon_reg_wait_run_is_off_timeout(id, timeout_value * MSEC);

	decon_dbg("%s -\n", __func__);
	return ret;
}

void decon_reg_set_win_enable(u32 id, u32 win_idx, u32 en)
{
	u32 val, mask;

	val = en ? ~0 : 0;
	mask = _WIN_EN_F;
	wincon_write_mask(id, DECON_CON_WIN(win_idx), val, mask);
	decon_dbg("%s: 0x%x\n", __func__,
			wincon_read(id, DECON_CON_WIN(win_idx)));
}

/*
 * argb_color : 32-bit
 * A[31:24] - R[23:16] - G[15:8] - B[7:0]
 */
static void decon_reg_set_win_mapcolor(u32 id, u32 win_idx, u32 argb_color)
{
	u32 val, mask;
	u32 mc_alpha = 0, mc_red = 0;
	u32 mc_green = 0, mc_blue = 0;

	mc_alpha = (argb_color >> 24) & 0xFF;
	mc_red = (argb_color >> 16) & 0xFF;
	mc_green = (argb_color >> 8) & 0xFF;
	mc_blue = (argb_color >> 0) & 0xFF;

	val = WIN_MAPCOLOR_A_F(mc_alpha) | WIN_MAPCOLOR_R_F(mc_red);
	mask = WIN_MAPCOLOR_A_MASK | WIN_MAPCOLOR_R_MASK;
	win_write_mask(id, WIN_COLORMAP_0(win_idx), val, mask);

	val = WIN_MAPCOLOR_G_F(mc_green) | WIN_MAPCOLOR_B_F(mc_blue);
	mask = WIN_MAPCOLOR_G_MASK | WIN_MAPCOLOR_B_MASK;
	win_write_mask(id, WIN_COLORMAP_1(win_idx), val, mask);
}

static void decon_reg_set_win_plane_alpha(u32 id, u32 win_idx, u32 a0, u32 a1)
{
	u32 val, mask;

	val = WIN_ALPHA1_F(a1) | WIN_ALPHA0_F(a0);
	mask = WIN_ALPHA1_MASK | WIN_ALPHA0_MASK;
	win_write_mask(id, WIN_FUNC_CON_0(win_idx), val, mask);
}

static void decon_reg_set_winmap(u32 id, u32 win_idx, u32 color, u32 en)
{
	u32 val, mask;

	/* Enable */
	val = en ? ~0 : 0;
	mask = WIN_MAPCOLOR_EN_F;
	wincon_write_mask(id, DECON_CON_WIN(win_idx), val, mask);
	decon_dbg("%s: 0x%x\n", __func__,
			wincon_read(id, DECON_CON_WIN(win_idx)));

	/* Color Set */
	decon_reg_set_win_mapcolor(id, win_idx, color);
}

/* ALPHA_MULT selection used in (a',b',c',d') coefficient */
static void decon_reg_set_win_alpha_mult(u32 id, u32 win_idx, u32 a_sel)
{
	u32 val, mask;

	val = WIN_ALPHA_MULT_SRC_SEL_F(a_sel);
	mask = WIN_ALPHA_MULT_SRC_SEL_MASK;
	win_write_mask(id, WIN_FUNC_CON_0(win_idx), val, mask);
}

static void decon_reg_set_win_sub_coeff(u32 id, u32 win_idx,
		u32 fgd, u32 bgd, u32 fga, u32 bga)
{
	u32 val, mask;

	/*
	 * [ Blending Equation ]
	 * Color : Cr = (a x Cf) + (b x Cb)  <Cf=FG pxl_C, Cb=BG pxl_C>
	 * Alpha : Ar = (c x Af) + (d x Ab)  <Af=FG pxl_A, Ab=BG pxl_A>
	 *
	 * [ User-defined ]
	 * a' = WINx_FG_ALPHA_D_SEL : Af' that is multiplied by FG Pixel Color
	 * b' = WINx_BG_ALPHA_D_SEL : Ab' that is multiplied by BG Pixel Color
	 * c' = WINx_FG_ALPHA_A_SEL : Af' that is multiplied by FG Pixel Alpha
	 * d' = WINx_BG_ALPHA_A_SEL : Ab' that is multiplied by BG Pixel Alpha
	 */

	val = (WIN_FG_ALPHA_D_SEL_F(fgd)
		| WIN_BG_ALPHA_D_SEL_F(bgd)
		| WIN_FG_ALPHA_A_SEL_F(fga)
		| WIN_BG_ALPHA_A_SEL_F(bga));
	mask = (WIN_FG_ALPHA_D_SEL_MASK
		| WIN_BG_ALPHA_D_SEL_MASK
		| WIN_FG_ALPHA_A_SEL_MASK
		| WIN_BG_ALPHA_A_SEL_MASK);
	win_write_mask(id, WIN_FUNC_CON_1(win_idx), val, mask);
}

static void decon_reg_set_win_func(u32 id, u32 win_idx,
		enum decon_win_func pd_func)
{
	u32 val, mask;

	val = WIN_FUNC_F(pd_func);
	mask = WIN_FUNC_MASK;
	win_write_mask(id, WIN_FUNC_CON_0(win_idx), val, mask);
}

static void decon_reg_set_win_bnd_function(u32 id, u32 win_idx,
		struct decon_window_regs *regs)
{
	int plane_a = regs->plane_alpha;
	enum decon_blending blend = regs->blend;
	enum decon_win_func pd_func = PD_FUNC_USER_DEFINED;
	u8 alpha0 = 0xff;
	u8 alpha1 = 0xff;
	bool is_plane_a = false;
	u32 af_d = BND_COEF_ONE, ab_d = BND_COEF_ZERO,
		af_a = BND_COEF_ONE, ab_a = BND_COEF_ZERO;

	if ((plane_a >= 0) && (plane_a <= 0xff)) {
		alpha0 = plane_a;
		alpha1 = 0;
		is_plane_a = true;
	} else
		decon_warn("%s:plane_a(%d) is out of range(0~255)\n", __func__, plane_a);

	if ((blend == DECON_BLENDING_NONE) && (is_plane_a && plane_a)) {
		af_d = BND_COEF_PLNAE_ALPHA0;
		ab_d = BND_COEF_ZERO;
		af_a = BND_COEF_PLNAE_ALPHA0;
		ab_a = BND_COEF_ZERO;
	} else if ((blend == DECON_BLENDING_COVERAGE) && !is_plane_a) {
		af_d = BND_COEF_AF;
		ab_d = BND_COEF_1_M_AF;
		af_a = BND_COEF_AF;
		ab_a = BND_COEF_1_M_AF;
	} else if ((blend == DECON_BLENDING_COVERAGE) && is_plane_a) {
		af_d = BND_COEF_ALPHA_MULT;
		ab_d = BND_COEF_1_M_ALPHA_MULT;
		af_a = BND_COEF_ALPHA_MULT;
		ab_a = BND_COEF_1_M_ALPHA_MULT;
	} else if ((blend == DECON_BLENDING_PREMULT) && !is_plane_a) {
		af_d = BND_COEF_ONE;
		ab_d = BND_COEF_1_M_AF;
		af_a = BND_COEF_ONE;
		ab_a = BND_COEF_1_M_AF;
	} else if ((blend == DECON_BLENDING_PREMULT) && is_plane_a) {
		af_d = BND_COEF_PLNAE_ALPHA0;
		ab_d = BND_COEF_1_M_ALPHA_MULT;
		af_a = BND_COEF_PLNAE_ALPHA0;
		ab_a = BND_COEF_1_M_ALPHA_MULT;
	} else {
		decon_dbg("%s:%d undefined blending mode\n",
				__func__, __LINE__);
	}

	decon_reg_set_win_plane_alpha(id, win_idx, alpha0, alpha1);
	decon_reg_set_win_alpha_mult(id, win_idx, ALPHA_MULT_SRC_SEL_AF);
	decon_reg_set_win_func(id, win_idx, pd_func);
	if (pd_func == PD_FUNC_USER_DEFINED)
		decon_reg_set_win_sub_coeff(id,
				win_idx, af_d, ab_d, af_a, ab_a);
}

/*
 * SLEEP_CTRL_MODE_F
 * 0 = Bypass shadow update request to DSIMIF
 * 1 = Postpone shadow update request to DSIMIF until PLL lock
 */
void decon_reg_set_pll_sleep(u32 id, u32 en)
{
	u32 val, mask;

	if (id >= 2) {
		decon_info("%s:%d decon(%d) not allowed\n",
				__func__, __LINE__, id);
		return;
	}
	val = en ? ~0 : 0;
	mask = (id == 0) ? PLL_SLEEP_EN_OUTIF0_F : PLL_SLEEP_EN_OUTIF1_F;
	mask |= SLEEP_CTRL_MODE_F;
	decon_write_mask(id, PLL_SLEEP_CON, val, mask);
}

/*
 * If decon is in the process of pll sleep,
 *  a delay corresponding to the pll lock time is required
 *  to ensure exit from pll sleep.
 */

void decon_reg_set_pll_wakeup(u32 id, u32 en)
{
	u32 val, mask;
	u32 delay_val = 0;
	struct decon_device *decon = get_decon_drvdata(id);

	if (id >= 2) {
		decon_info("%s:%d decon(%d) not allowed\n",
				__func__, __LINE__, id);
		return;
	}
	val = en ? ~0 : 0;
	mask = (id == 0) ? PLL_SLEEP_MASK_OUTIF0 : PLL_SLEEP_MASK_OUTIF1;
	decon_write_mask(id, PLL_SLEEP_CON, val, mask);

	if (en) {
		delay_val = 20 * decon->lcd_info->dphy_pms.p;
		usleep_range(delay_val, delay_val + 1);
	}
}

/******************** EXPORTED DECON CAL APIs ********************/
/* TODO: maybe this function will be moved to internal DECON CAL function */
void decon_reg_update_req_global(u32 id)
{
	decon_write_mask(id, SHD_REG_UP_REQ, ~0, SHD_REG_UP_REQ_GLOBAL);
}

void decon_reg_update_req_dqe(u32 id)
{
#if defined(CONFIG_EXYNOS_DECON_DQE)
	struct decon_device *decon = get_decon_drvdata(id);
#endif
	if (id != 0)
		return;

	decon_write_mask(id, SHD_REG_UP_REQ, ~0, SHD_REG_UP_REQ_DQE);

#if defined(CONFIG_EXYNOS_DECON_DQE)
	decon_dqe_restore_cgc_context(decon);
	decon->dqe_cgc_idx = !decon->dqe_cgc_idx;
#endif
}

int decon_reg_init(u32 id, u32 dsi_idx, struct decon_param *p)
{
	struct exynos_panel_info *lcd_info = p->lcd_info;
	struct decon_mode_info *psr = &p->psr;
	enum decon_enhance_path e_path = ENHANCEPATH_ENHANCE_ALL_OFF;

	/*
	 * DECON does not need to start, if DECON is already
	 * running(enabled in LCD_ON_LK)
	 */
	if (decon_reg_get_run_status(id)) {
		decon_info("%s already called by BOOTLOADER\n", __func__);
		decon_reg_init_probe(id, dsi_idx, p);
		if (psr->psr_mode == DECON_MIPI_COMMAND_MODE)
			decon_reg_set_trigger(id, psr, DECON_TRIG_DISABLE);
		return -EBUSY;
	}

	decon_reg_set_clkgate_mode(id, 0);

	if (psr->out_type == DECON_OUT_DP) {
		/* Can not use qactive pll at 2100
		 * Must use ewr instead of qactive at 2100
		 * decon_reg_set_te_qactive_pll_mode(id, 1);
		 */
		decon_reg_set_ewr_mode(id, 1);
	}

	decon_reg_set_sram_enable(id);

	decon_reg_set_operation_mode(id, psr->psr_mode);

	decon_reg_set_blender_bg_size(id, psr->dsi_mode,
					lcd_info->xres, lcd_info->yres);

#if defined(CONFIG_EXYNOS_LATENCY_MONITOR)
	/* enable once at init time */
	decon_reg_set_latency_monitor_enable(id, 1);
#endif

	if (id == 3) {
		/* Set a TRIG mode */
		/* This code is for only DECON 2 s/w trigger mode */
		decon_reg_configure_trigger(id, psr->trig_mode);
		decon_reg_configure_lcd(id, p);
	} else {
		decon_reg_configure_lcd(id, p);
		if (psr->psr_mode == DECON_MIPI_COMMAND_MODE) {
#if defined(CONFIG_EXYNOS_EWR)
			/* 60fps: request wakeup at 16.566ms after TE rising */
			decon_reg_set_ewr_control(id,
				decon_get_ewr_cycle(lcd_info->fps, 100), 1);
#endif
			decon_reg_set_trigger(id, psr, DECON_TRIG_DISABLE);
		}
	}

	/* FIXME: DECON_T dedicated to PRE_WB */
	if (p->psr.out_type == DECON_OUT_WB)
		decon_reg_set_data_path(id, DPATH_NOCOMP_OUTFIFO0_WBIF, e_path);

	/* asserted interrupt should be cleared before initializing decon hw */
	decon_reg_clear_int_all(id);

	/* Configure DECON dsim connection  : 'data_path' setting is required */
	if (psr->out_type == DECON_OUT_DSI)
		decon_reg_set_interface_dsi(id);
	else if (psr->out_type == DECON_OUT_DP)
		decon_reg_set_interface_dp(id);



#if defined(CONFIG_EXYNOS_PLL_SLEEP)
	/* TODO : register for outfifo2 doesn't exist, needs a confirm */
	if (psr->psr_mode == DECON_MIPI_COMMAND_MODE &&
			psr->dsi_mode != DSI_MODE_DUAL_DSI)
		decon_reg_set_pll_sleep(id, 1);
#endif

	return 0;
}

int decon_reg_start(u32 id, struct decon_mode_info *psr)
{
	int ret = 0;
	struct decon_device *decon = get_decon_drvdata(id);
	struct exynos_panel_info *lcd_info = decon->lcd_info;

#if defined(CONFIG_EXYNOS_DISPLAYPORT)
	u32 sst_id = SST1;

	if (psr->out_type == DECON_OUT_DP) {
		sst_id = displayport_get_sst_id_with_decon_id(id);

		displayport_reg_lh_p_ch_power(sst_id, 1);
	}
#endif

	decon_reg_direct_on_off(id, 1);
#if defined(CONFIG_EXYNOS_DECON_DQE)
	decon_reg_update_req_dqe(id);
#endif

	if (lcd_info->dsc.en)
		decon_reg_cmp_shadow_update_req(id);
	decon_reg_update_req_global(id);

	/*
	 * DECON goes to run-status as soon as
	 * request shadow update without HW_TE
	 */
	ret = decon_reg_wait_run_status_timeout(id, 2 * 1000); /* timeout 2ms */

	/* wait until run-status, then trigger */
	if (psr->psr_mode == DECON_MIPI_COMMAND_MODE)
		decon_reg_set_trigger(id, psr, DECON_TRIG_ENABLE);
	return ret;
}

/*
 * stop sequence should be carefully for stability
 * try sequecne
 *	1. perframe off
 *	2. instant off
 */
int decon_reg_stop(u32 id, u32 dsi_idx, struct decon_mode_info *psr, bool rst,
		u32 fps)
{
	int ret = 0;

#if defined(CONFIG_EXYNOS_PLL_SLEEP)
	/* when pll is asleep, need to wake it up before stopping */
	if (psr->psr_mode == DECON_MIPI_COMMAND_MODE &&
			psr->dsi_mode != DSI_MODE_DUAL_DSI)
		decon_reg_set_pll_wakeup(id, 1);
#endif

	if (psr->out_type == DECON_OUT_DP) {
		/* Can not use qactive pll at 2100
		 * Must use ewr instead of qactive at 2100
		 * decon_reg_set_te_qactive_pll_mode(id, 0);
		 */
		decon_reg_set_ewr_mode(id, 0);
	}

	/* call perframe stop */
	ret = decon_reg_stop_perframe(id, dsi_idx, psr, fps);
	if (ret < 0) {
		decon_err("%s, failed to perframe_stop\n", __func__);
		/* if fails, call decon instant off */
		ret = decon_reg_stop_inst(id, dsi_idx, psr, fps);
		if (ret < 0)
			decon_err("%s, failed to instant_stop\n", __func__);
	}

	/* assert reset when stopped normally or requested */
	if (!ret && rst)
		decon_reg_reset(id);

	decon_reg_clear_int_all(id);

	return ret;
}

void decon_reg_win_enable_and_update(u32 id, u32 win_idx, u32 en)
{
	decon_reg_set_win_enable(id, win_idx, en);
	decon_reg_update_req_window(id, win_idx);
}

void decon_reg_all_win_shadow_update_req(u32 id)
{
	u32 mask;

	mask = SHD_REG_UP_REQ_FOR_DECON;

	decon_write_mask(id, SHD_REG_UP_REQ, ~0, mask);
}

void decon_reg_set_window_control(u32 id, int win_idx,
		struct decon_window_regs *regs, u32 winmap_en)
{
	u32 win_en = regs->wincon & _WIN_EN_F ? 1 : 0;

	if (win_en) {
		decon_dbg("%s: win id = %d\n", __func__, win_idx);
		decon_reg_set_win_bnd_function(0, win_idx, regs);
		win_write(id, WIN_START_POSITION(win_idx), regs->start_pos);
		win_write(id, WIN_END_POSITION(win_idx), regs->end_pos);
		win_write(id, WIN_START_TIME_CON(win_idx), regs->start_time);
		decon_reg_set_winmap(id, win_idx, regs->colormap, winmap_en);
	}

	decon_reg_config_win_channel(id, win_idx, regs->ch);
	decon_reg_set_win_enable(id, win_idx, win_en);

	decon_dbg("%s: regs->ch(%d)\n", __func__, regs->ch);
}

void decon_reg_update_req_window_mask(u32 id, u32 win_idx)
{
	u32 mask;

	mask = SHD_REG_UP_REQ_FOR_DECON;
	mask &= ~(SHD_REG_UP_REQ_WIN(win_idx));
	decon_write_mask(id, SHD_REG_UP_REQ, ~0, mask);
}

void decon_reg_update_req_window(u32 id, u32 win_idx)
{
	u32 mask;

	mask = SHD_REG_UP_REQ_WIN(win_idx);
	decon_write_mask(id, SHD_REG_UP_REQ, ~0, mask);
}

void decon_reg_set_trigger(u32 id, struct decon_mode_info *psr,
		enum decon_set_trig en)
{
	u32 val, mask;

	if (psr->psr_mode == DECON_VIDEO_MODE)
		return;

	if (psr->trig_mode == DECON_SW_TRIG) {
		val = (en == DECON_TRIG_ENABLE) ?
				(SW_TRIG_EN | SW_TRIG_DET_EN) : 0;
		mask = HW_TRIG_EN | SW_TRIG_EN | SW_TRIG_DET_EN;
	} else { /* DECON_HW_TRIG */
		val = HW_TRIG_EN;
		if (en == DECON_TRIG_DISABLE)
			val |= HW_TRIG_MASK_DECON;
		mask = HW_TRIG_EN | HW_TRIG_MASK_DECON;
	}

	decon_write_mask(id, TRIG_CON, val, mask);
}

void decon_reg_update_req_and_unmask(u32 id, struct decon_mode_info *psr)
{
	decon_reg_update_req_global(id);

	if (psr->psr_mode == DECON_MIPI_COMMAND_MODE)
		decon_reg_set_trigger(id, psr, DECON_TRIG_ENABLE);
}

int decon_reg_wait_update_done_timeout(u32 id, unsigned long timeout)
{
	unsigned long delay_time = 100;
	unsigned long cnt = timeout / delay_time;

	while (decon_read(id, SHD_REG_UP_REQ) && --cnt)
		usleep_range(delay_time, delay_time + 1);

	if (!cnt) {
		decon_err("decon%d timeout of updating decon registers\n", id);
		return -EBUSY;
	}

	return 0;
}

int decon_reg_wait_update_done_and_mask(u32 id,
		struct decon_mode_info *psr, u32 timeout)
{
	int result;
#if defined(CONFIG_SOC_EXYNOS2100)
	unsigned long i, win;
#endif

	result = decon_reg_wait_update_done_timeout(id, timeout);

	if (psr->psr_mode == DECON_MIPI_COMMAND_MODE)
		decon_reg_set_trigger(id, psr, DECON_TRIG_DISABLE);

#if defined(CONFIG_SOC_EXYNOS2100)
		if (!result) {
			for (i = 0; i < DECON_MAIN_SHADOW_SFR_COUNT; i++)
				decon_main_shadow_sfr[i] = decon_read(id, DECON_MAIN_SHADOW_DUMP_OFFSET + 4 * i);
			for (win = 0; win < MAX_DECON_WIN; win++)
				for (i = 0; i < DECON_WIN_SHADOW_SFR_COUNT; i++)
					decon_win_shadow_sfr[win][i] = win_read(id, WIN_OFFSET(win) + 4 * i);
		}
#endif

	return result;
}

int decon_reg_wait_idle_status_timeout(u32 id, unsigned long timeout)
{
	unsigned long delay_time = 10;
	unsigned long cnt = timeout / delay_time;
	u32 status;

	do {
		status = decon_reg_get_idle_status(id);
		cnt--;
		usleep_range(delay_time, delay_time + 1);
	} while (!status && cnt);

	if (!cnt) {
		decon_err("decon%d wait timeout decon idle status(%u)\n",
								id, status);
		return -EBUSY;
	}

	return 0;
}

void decon_reg_set_partial_update(u32 id, enum decon_dsi_mode dsi_mode,
		struct exynos_panel_info *lcd_info, bool in_slice[],
		u32 partial_w, u32 partial_h)
{
	u32 dual_slice_en[2] = {1, 1};
	u32 slice_mode_ch[2] = {0, 0};

	/* Here, lcd_info contains the size to be updated */
	decon_reg_set_blender_bg_size(id, dsi_mode, partial_w, partial_h);

	if (lcd_info->dsc.en) {
		/* get correct DSC configuration */
		dsc_get_partial_update_info(lcd_info->dsc.slice_num,
				lcd_info->dsc.cnt, in_slice,
				dual_slice_en, slice_mode_ch);
		/* To support dual-display : DECON1 have to set DSC1 */
		dsc_reg_set_partial_update(id, dual_slice_en[0],
				slice_mode_ch[0], partial_h);
		if (lcd_info->dsc.cnt == 2)
			dsc_reg_set_partial_update(1, dual_slice_en[1],
					slice_mode_ch[1], partial_h);
	}

	decon_reg_set_data_path_size(id, partial_w, partial_h,
		lcd_info->dsc.en, lcd_info->dsc.cnt,
		lcd_info->dsc.enc_sw, lcd_info->dsc.slice_h, dual_slice_en);

	if (lcd_info->dsc.en)
		decon_reg_cmp_shadow_update_req(id);
}

void decon_reg_set_mres(u32 id, struct decon_param *p)
{
	struct exynos_panel_info *lcd_info = p->lcd_info;
	struct decon_mode_info *psr = &p->psr;
	u32 overlap_w = 0;

	if (lcd_info->mode != DECON_MIPI_COMMAND_MODE) {
		dsim_info("%s: mode[%d] doesn't support multi resolution\n",
				__func__, lcd_info->mode);
		return;
	}

	decon_reg_set_blender_bg_size(id, psr->dsi_mode,
					lcd_info->xres, lcd_info->yres);

	if (lcd_info->dsc.en)
		dsc_reg_init(id, p, overlap_w, 0);
	else
		decon_reg_config_data_path_size(id, lcd_info->xres,
				lcd_info->yres, overlap_w, NULL, p);
}

void decon_reg_release_resource(u32 id, struct decon_mode_info *psr)
{
	decon_reg_per_frame_off(id);
	decon_reg_update_req_global(id);
}

void decon_reg_config_wb_size(u32 id, struct exynos_panel_info *lcd_info,
		struct decon_param *param)
{
	decon_reg_set_blender_bg_size(id, DSI_MODE_SINGLE,
					lcd_info->xres, lcd_info->yres);
	decon_reg_config_data_path_size(id, lcd_info->xres,
			lcd_info->yres, 0, NULL, param);
}

void decon_reg_set_int(u32 id, struct decon_mode_info *psr, u32 en)
{
	u32 val, mask;

	decon_reg_clear_int_all(id);

	if (en) {
		val = (INT_EN_FRAME_DONE
			| INT_EN_FRAME_START
			| INT_EN_EXTRA
			| INT_EN);

		decon_write_mask(id, DECON_INT_EN, val, INT_EN_MASK);
		decon_dbg("decon %d, interrupt val = %x\n", id, val);

		val = (INT_EN_RESOURCE_CONFLICT | INT_EN_TIME_OUT);
		decon_write(id, DECON_INT_EN_EXTRA, val);
	} else {
		mask = (INT_EN_EXTRA | INT_EN);
		decon_write_mask(id, DECON_INT_EN, 0, mask);
	}
}

/* opt: 1 = print SEL_SRAM */
static void decon_reg_read_resource_status(u32 id, u32 opt)
{
	u32 i;

	decon_warn("decon%d RSC_STATUS_0: SEL_CH  = 0x%x\n",
		id, decon_read(id, RSC_STATUS_0));
	decon_warn("decon%d RSC_STATUS_2: SEL_WIN = 0x%x\n",
		id, decon_read(id, RSC_STATUS_2));
	decon_warn("decon%d RSC_STATUS_4: SEL_DSC = 0x%x\n",
		id, decon_read(id, RSC_STATUS_4));
	decon_warn("decon%d RSC_STATUS_5: SEL_DSCC = 0x%x\n",
		id, decon_read(id, RSC_STATUS_5));

	if (opt) {
		for (i = 0; i < 5; i++) {
			decon_warn("decon%d RSC_STATUS_%d = 0x%x\n",
				id, (7 + i),
				decon_read(id, RSC_STATUS_7 + (i * 4)));
		}
	}
}

int decon_reg_get_interrupt_and_clear(u32 id, u32 *ext_irq)
{
	u32 val, val1;
	u32 reg_id;

	reg_id = DECON_INT_PEND;
	val = decon_read(id, reg_id);

	if (val & INT_PEND_FRAME_START)
		decon_write(id, reg_id, INT_PEND_FRAME_START);

	if (val & INT_PEND_FRAME_DONE)
		decon_write(id, reg_id, INT_PEND_FRAME_DONE);

	if (val & INT_PEND_EXTRA) {
		decon_write(id, reg_id, INT_PEND_EXTRA);

		reg_id = DECON_INT_PEND_EXTRA;
		val1 = decon_read(id, reg_id);
		*ext_irq = val1;

		if (val1 & INT_PEND_RESOURCE_CONFLICT) {
			decon_write(id, reg_id, INT_PEND_RESOURCE_CONFLICT);
			decon_reg_read_resource_status(id, 0);
		}

		if (val1 & INT_EN_TIME_OUT)
			decon_write(id, reg_id, INT_EN_TIME_OUT);
	}

	return val;
}

/* id: dsim_id */
void decon_reg_set_start_crc(u32 id, u32 dsim_id, u32 en)
{
	dsimif_write_mask(DSIMIF_CRC_CON(dsim_id), en ? ~0 : 0, CRC_START);
}

void decon_reg_get_crc_data(u32 id, u32 dsim_id, u32 crc_data[])
{
	crc_data[0] = dsimif_read(DSIMIF_CRC_DATA_R(dsim_id));
	crc_data[1] = dsimif_read(DSIMIF_CRC_DATA_G(dsim_id));
	crc_data[2] = dsimif_read(DSIMIF_CRC_DATA_B(dsim_id));
}

int decon_check_supported_formats(enum decon_pixel_format format)
{
	switch (format) {
	case DECON_PIXEL_FORMAT_ARGB_8888:
	case DECON_PIXEL_FORMAT_ABGR_8888:
	case DECON_PIXEL_FORMAT_RGBA_8888:
	case DECON_PIXEL_FORMAT_BGRA_8888:
	case DECON_PIXEL_FORMAT_XRGB_8888:
	case DECON_PIXEL_FORMAT_XBGR_8888:
	case DECON_PIXEL_FORMAT_RGBX_8888:
	case DECON_PIXEL_FORMAT_BGRX_8888:
	case DECON_PIXEL_FORMAT_RGB_565:
	case DECON_PIXEL_FORMAT_BGR_565:
	case DECON_PIXEL_FORMAT_NV12:
	case DECON_PIXEL_FORMAT_NV12M:
	case DECON_PIXEL_FORMAT_NV21:
	case DECON_PIXEL_FORMAT_NV21M:
	case DECON_PIXEL_FORMAT_NV12N:
	case DECON_PIXEL_FORMAT_NV12N_10B:

	case DECON_PIXEL_FORMAT_ARGB_2101010:
	case DECON_PIXEL_FORMAT_ABGR_2101010:
	case DECON_PIXEL_FORMAT_RGBA_1010102:
	case DECON_PIXEL_FORMAT_BGRA_1010102:

	case DECON_PIXEL_FORMAT_NV12M_P010:
	case DECON_PIXEL_FORMAT_NV21M_P010:
	case DECON_PIXEL_FORMAT_NV12M_S10B:
	case DECON_PIXEL_FORMAT_NV21M_S10B:
	case DECON_PIXEL_FORMAT_NV12_P010:

	case DECON_PIXEL_FORMAT_NV16:
	case DECON_PIXEL_FORMAT_NV61:
	case DECON_PIXEL_FORMAT_NV16M_P210:
	case DECON_PIXEL_FORMAT_NV61M_P210:
	case DECON_PIXEL_FORMAT_NV16M_S10B:
	case DECON_PIXEL_FORMAT_NV61M_S10B:

	case DECON_PIXEL_FORMAT_NV12M_SBWC_8B:
	case DECON_PIXEL_FORMAT_NV12M_SBWC_10B:
	case DECON_PIXEL_FORMAT_NV21M_SBWC_8B:
	case DECON_PIXEL_FORMAT_NV21M_SBWC_10B:
	case DECON_PIXEL_FORMAT_NV12N_SBWC_8B:
	case DECON_PIXEL_FORMAT_NV12N_SBWC_10B:
	case DECON_PIXEL_FORMAT_NV12M_SBWC_8B_L50:
	case DECON_PIXEL_FORMAT_NV12M_SBWC_8B_L75:
	case DECON_PIXEL_FORMAT_NV12M_SBWC_10B_L40:
	case DECON_PIXEL_FORMAT_NV12M_SBWC_10B_L60:
	case DECON_PIXEL_FORMAT_NV12M_SBWC_10B_L80:
	case DECON_PIXEL_FORMAT_NV12N_SBWC_8B_L50:
	case DECON_PIXEL_FORMAT_NV12N_SBWC_8B_L75:
	case DECON_PIXEL_FORMAT_NV12N_SBWC_10B_L40:
	case DECON_PIXEL_FORMAT_NV12N_SBWC_10B_L60:
	case DECON_PIXEL_FORMAT_NV12N_SBWC_10B_L80:
		return 0;
	default:
		break;
	}

	return -EINVAL;
}

#define PREFIX_LEN	40
#define ROW_LEN		32
static void decon_print_hex_dump(void __iomem *regs, const void *buf,
		size_t len)
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

static void decon_print_hex_dump_shadow(void __iomem *regs, const void *buf, void *buf1, size_t len)
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
				32, 4, buf1 + i, row, false);
	}
}

void __decon_dump(u32 id, void __iomem *main_regs, void __iomem *win_regs,
		void __iomem *sub_regs, void __iomem *wincon_regs,
		void __iomem *dqe_regs, u32 dsc_en)
{
	int i;

	/* decon_main */
	decon_info("\n=== DECON%d_MAIN SFR DUMP ===\n", id);
	decon_print_hex_dump(main_regs, main_regs + 0x0000, 0x704);
	/* shadow */
#if defined(CONFIG_SOC_EXYNOS2100)
	decon_info("=== DECON%d_MAIN psedu-SHADOW SFR DUMP ===\n", id);
	decon_print_hex_dump_shadow(main_regs, main_regs + SHADOW_OFFSET + DECON_MAIN_SHADOW_DUMP_OFFSET,
					decon_main_shadow_sfr, DECON_MAIN_SHADOW_SFR_COUNT * 4);
#else
	decon_info("=== DECON%d_MAIN SHADOW SFR DUMP ===\n", id);
	decon_print_hex_dump(main_regs, main_regs + SHADOW_OFFSET, 0x2B0);
#endif

	/* decon_win & decon_wincon : 6EA */
	for (i = 0; i < MAX_DECON_WIN; i++) {
		decon_info("\n=== DECON_WIN%d SFR DUMP ===\n", i);
		decon_print_hex_dump(win_regs, win_regs + WIN_OFFSET(i), 0x20);
		decon_info("=== DECON_WINCON%d SFR DUMP ===\n", i);
		decon_print_hex_dump(wincon_regs, wincon_regs + WIN_OFFSET(i),
				0x4);
		/* shadow */
#if defined(CONFIG_SOC_EXYNOS2100)
		decon_info("=== DECON_WIN%d psedu-SHADOW SFR DUMP ===\n", i);
		decon_print_hex_dump_shadow(win_regs,
				win_regs + WIN_OFFSET(i) + SHADOW_OFFSET, decon_win_shadow_sfr[i],
				DECON_WIN_SHADOW_SFR_COUNT * 4);
#else
		decon_info("=== DECON_WIN%d SHADOW SFR DUMP ===\n", i);
		decon_print_hex_dump(win_regs,
				win_regs + WIN_OFFSET(i) + SHADOW_OFFSET, 0x20);
#endif
		decon_info("=== DECON_WINCON%d SHADOW SFR DUMP ===\n", i);
		decon_print_hex_dump(wincon_regs,
				wincon_regs + WIN_OFFSET(i) + SHADOW_OFFSET,
				0x4);
	}

	/* dsimif : 2EA */
	for (i = 0; i < 2; i++) {
		decon_info("\n=== DECON_SUB.DSIMIF%d SFR DUMP ===\n", i);
		decon_print_hex_dump(sub_regs,
				sub_regs + DSIMIF_OFFSET(i), 0x10);
		decon_print_hex_dump(sub_regs,
				sub_regs + DSIMIF_OFFSET(i) + 0x80, 0x10);
		/* shadow */
		decon_info("=== DECON_SUB.DSIMIF%d SHADOW SFR DUMP ===\n", i);
		decon_print_hex_dump(sub_regs,
				sub_regs + DSIMIF_OFFSET(i) + SHADOW_OFFSET,
				0x10);
		decon_print_hex_dump(sub_regs,
				sub_regs + DSIMIF_OFFSET(i) + SHADOW_OFFSET +
				0x80, 0x10);
	}

	/* dpif : 2EA */
	for (i = 0; i < 2; i++) {
		decon_info("\n=== DECON_SUB.DPIF%d SFR DUMP ===\n", i);
		decon_print_hex_dump(sub_regs, sub_regs + DPIF_OFFSET(i), 0x4);
		decon_print_hex_dump(sub_regs,
				sub_regs + DPIF_OFFSET(i) + 0x80, 0x10);
		/* shadow */
		decon_info("=== DECON_SUB.DPIF%d SHADOW SFR DUMP ===\n", i);
		decon_print_hex_dump(sub_regs,
				sub_regs + DPIF_OFFSET(i) + SHADOW_OFFSET, 0x4);
		decon_print_hex_dump(sub_regs,
				sub_regs + DPIF_OFFSET(i) + SHADOW_OFFSET +
				0x80, 0x10);
	}

	/* dsc : 3EA */
	if (dsc_en) {
		for (i = 0; i < 3; i++) {
			decon_info("\n=== DECON_SUB.DSC%d SFR DUMP ===\n", i);
			decon_print_hex_dump(sub_regs,
					sub_regs + DSC_OFFSET(i), 0x88);
			/* shadow */
			decon_info("=== DECON_SUB.DSC%d SHADOW SFR DUMP ===\n",
					i);
			decon_print_hex_dump(sub_regs,
					sub_regs + DSC_OFFSET(i) +
					SHADOW_OFFSET, 0x88);
		}
	}

#if defined(CONFIG_EXYNOS_DECON_DQE)
	decon_info("\n=== DECON_DQE0 DQE_TOP SFR DUMP ===\n");
	decon_print_hex_dump(dqe_regs, dqe_regs + DQE0_DQE_TOP, 0x100);
	decon_info("\n=== DECON_DQE0 DQE_TOP SHADOW SFR DUMP ===\n");
	decon_print_hex_dump(dqe_regs,
			dqe_regs + DQE0_DQE_TOP + DQE0_SHADOW_OFFSET, 0x100);

	decon_info("\n=== DECON_DQE0 DQE_GAMMA_MATRIX SFR DUMP ===\n");
	decon_print_hex_dump(dqe_regs, dqe_regs + DQE0_DQE_GAMMA_MATRIX, 0x20);
	decon_info("\n=== DECON_DQE0 DQE_GAMMA_MATRIX SHADOW SFR DUMP ===\n");
	decon_print_hex_dump(dqe_regs,
			dqe_regs + DQE0_DQE_GAMMA_MATRIX + DQE0_SHADOW_OFFSET, 0x20);
#endif
}

/* Exynos2100 chip dependent HW limitation
 *	: returns 0 if no error
 *	: otherwise returns -EPERM for HW-wise not permitted
 */
int decon_reg_check_global_limitation(struct decon_device *decon,
		struct decon_win_config *config)
{
	int ret = 0;
	int i; //, j;
	/*
	 * DPU_DMA0 CH0 : L0, L1, L2, L3, WB
	 * DPU_DMA0 CH1 : L4, L5, L6, L7
	 * DPU_DMA1 CH0 : L8, L9, L10, L11
	 * DPU_DMA1 CH1 : L12, L13, L14, L15
	 */
	int cursor_cnt = 0;
#if defined(CONFIG_EXYNOS_LIMIT_ROTATION)
	const struct dpu_fmt *fmt_info;
#endif

	if ((config[decon->dt.wb_win].state == DECON_WIN_STATE_BUFFER) &&
			config[decon->dt.wb_win].channel != (decon->dt.dpp_cnt - 1)) {
		ret = -EINVAL;
		goto err;
	}

	for (i = 0; i < MAX_DECON_WIN; i++) {
		if (config[i].state != DECON_WIN_STATE_BUFFER)
			continue;

		if (config[i].state == DECON_WIN_STATE_CURSOR)
			cursor_cnt++;

		/* window cannot be connected to writeback channel */
		if (config[i].channel >= decon->dt.dpp_cnt - 1) {
			ret = -EINVAL;
			decon_err("invalid channel(%d) + window(%d)\n",
					config[i].channel, i);
			goto err;
		}
	}

	if (cursor_cnt >= 2)
		ret = -EPERM;

err:
	return ret;
}

static u32 decon_reg_outfifo_y_couter(u32 id)
{
	u32 val;

	val = decon_read(0, DBG_INFO_OF_CNT0(id));
	return DBG_INFO_OF_YCNT(val);
}

u32 decon_processed_linecnt(struct decon_device *decon)
{
	return decon_reg_outfifo_y_couter(decon->id);
}
