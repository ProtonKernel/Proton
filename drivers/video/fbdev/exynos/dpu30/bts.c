 /*
 * Copyright (c) 2016 Samsung Electronics Co., Ltd.
 *		http://www.samsung.com
 *
 * BTS file for Samsung EXYNOS DPU driver
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#include "decon.h"
#include "dpp.h"
#include "format.h"

#include <soc/samsung/bts.h>
#include <media/v4l2-subdev.h>
#if !defined(CONFIG_UML)
#include <soc/samsung/cal-if.h>
#endif
#include <dt-bindings/soc/samsung/exynos2100-devfreq.h>
#include <soc/samsung/exynos-devfreq.h>
#include "mcd_decon.h"

#define DISP_FACTOR		100UL
#define LCD_REFRESH_RATE	60U
#define MULTI_FACTOR 		(1U << 10)
#define ROT_READ_BYTE		(32)

#define ACLK_100MHZ_PERIOD	10000UL
#define FRAME_TIME_NSEC		1000000000UL	/* 1sec */

#define BTS_MAX_XRES		4096
#define BTS_MAX_YRES		2160

#define XRES_FOR_HIGH_INTCLK	1440
#define YRES_FOR_HIGH_INTCLK	2960
#define FPS_FOR_HIGH_INTCLK	63

#define BW_WEIGHT_FOR_HIGH_FPS	110UL
#define MAX_BW_FOR_BW_WEIGHT	8120000

/*
 * 1. function clock
 *    clk[i] = dst_w * dst_h * scale_ratio_h * scale_ratio_h * fps * 1.1 / ppc
 *    alck1 = max(clk[i])
 * 2. AXI throughput clock
 *    1) fps based
 *       clk_bw[i] = src_w * src_h * fps * (bpp/8) / (bus_width * bus_util)
 *    2) rotation throughput for initial latency
 *       clk_r[i] = src_h * 32 * (bpp/8) / (bus_width * rot_util) / (v_blank)
 *       # v_blank : command - TE_hi_pulse
 *                   video - (vbp) @initial-frame, (vbp+vfp) @inter-frame
 *    if (clk_bw[i] < clk_r[i])
 *       clk_bw[i] = clk_r[i]
 *    aclk2 = max(clk for sum(same axi bw[i])) --> clk of peak_bw
 *
 * => aclk_dpu = max(aclk1, aclk2)
 */

/* unit : usec x 1000 -> 5592 (5.592us) for WQHD+ case */
#if IS_ENABLED(CONFIG_MCD_PANEL)
static inline u32 dpu_bts_get_one_line_time(int tot_v, int fps)
{
	int tmp;

	tmp = (FRAME_TIME_NSEC + fps - 1) / fps;

	return (tmp / tot_v);
}
#else
static inline u32 dpu_bts_get_one_line_time(struct exynos_panel_info *lcd_info)
{
	u32 tot_v;
	int tmp;

	tot_v = lcd_info->yres + lcd_info->vfp + lcd_info->vsa + lcd_info->vbp;
	tmp = (FRAME_TIME_NSEC + lcd_info->fps - 1) / lcd_info->fps;

	return (tmp / tot_v);
}
#endif

/* framebuffer compressor(AFBC, SBWC) line delay is usually 4 */
static inline u32 dpu_bts_comp_latency(u32 src_w, u32 ppc, u32 line_delay)
{
	return ((src_w * line_delay) / ppc);
}

/* scaler line delay is usually 3
 * scaling order : horizontal -> vertical scale
 * -> need to reflect scale-ratio
 */
static inline u32 dpu_bts_scale_latency(u32 src_w, u32 dst_w, u32 ppc,
		u32 line_delay)
{
	u32 lat_scale = 0;
	u32 scaled_w;

	if (src_w > dst_w)
		scaled_w = src_w * (src_w * 1000U) / dst_w;
	else
		scaled_w = src_w * 1000U;
	lat_scale = ((scaled_w * line_delay) / (ppc * 1000U));

	return lat_scale;
}

/* rotator ppc is usually 4 or 8
 * 1-read : 32BYTE (pixel)
 */
static inline u32 dpu_bts_rotate_latency(u32 src_w, u32 r_ppc)
{
	return (src_w * (ROT_READ_BYTE / r_ppc));
}

/*
 * [DSC]
 * Line memory is necessary like followings.
 *  1EA(1ppc) : 2-line for 2-slice, 1-line for 1-slice
 *  2EA(2ppc) : 3.5-line for 4-slice (DSCC 0.5-line + DSC 3-line)
 *        2.5-line for 2-slice (DSCC 0.5-line + DSC 2-line)
 *
 * [DECON] none
 * When 1H is filled at OUT_FIFO, it immediately transfers to DSIM.
 */
static inline u32 dpu_bts_dsc_latency(u32 slice_num, u32 dsc_cnt,
		u32 dst_w, u32 ppc)
{
	u32 lat_dsc = dst_w;

	switch (slice_num) {
	case 1:
		/* DSC: 1EA */
		lat_dsc = dst_w * 1;
		break;
	case 2:
		if (dsc_cnt == 1)
			lat_dsc = dst_w * 2;
		else
			lat_dsc = (dst_w * 25) / (10 * ppc);
		break;
	case 4:
		/* DSC: 2EA */
		lat_dsc = (dst_w * 35) / (10 * ppc);
		break;
	default:
		break;
	}

	return lat_dsc;
}

/*
 * unit : nsec x 1000
 * reference aclk : 100MHz (-> 10ns x 1000)
 * # cycles = usec * aclk_mhz
 */
static inline u32 dpu_bts_convert_aclk_to_ns(u32 aclk_mhz)
{
	return ((ACLK_100MHZ_PERIOD * 100) / aclk_mhz);
}

/*
 * This function is introduced due to VRR feature.
 * return : kHz value based on 1-pixel processing pipe-line
 */
static u64 dpu_bts_get_resol_clock(u32 xres, u32 yres, u32 fps)
{
	u32 op_fps;
	u64 margin;
	u64 resol_khz;

	/*
	 * check lower limit of fps
	 * this can be removed if there is no stuck issue
	 */
	op_fps = (fps < LCD_REFRESH_RATE) ? LCD_REFRESH_RATE : fps;

	/*
	 * aclk_khz = vclk_1pix * ( 1.1 + (48+20)/WIDTH ) : x1000
	 * @ (1.1)   : BUS Latency Considerable Margin (10%)
	 * @ (48+20) : HW bubble cycles
	 *      - 48 : 12 cycles per slice, total 4 slice
	 *      - 20 : hblank cycles for other HW module
	 */
	margin = 1100 + (48000 + 20000) / xres;
	/* convert to kHz unit */
	resol_khz = (xres * yres * (u64)op_fps * margin / 1000) / 1000;

	return resol_khz;
}

u32 dpu_bts_get_vblank_time_ns(struct decon_device *decon)
{
	u32 line_t_ns, v_blank_t_ns;

#if IS_ENABLED(CONFIG_MCD_PANEL)
	line_t_ns = dpu_bts_get_one_line_time(decon->lcd_info->yres +
			decon->lcd_info->vfp + decon->lcd_info->vsa + decon->lcd_info->vbp,
			decon->bts.fps);
#else
	line_t_ns = dpu_bts_get_one_line_time(decon->lcd_info);
#endif
	if (decon->lcd_info->mode == DECON_VIDEO_MODE)
		v_blank_t_ns = (decon->lcd_info->vbp + decon->lcd_info->vfp) *
					line_t_ns;
	else
		v_blank_t_ns = decon->lcd_info->v_blank_t * 1000U;

	/* v_blank should be over minimum v total porch */
	if (v_blank_t_ns < (3 * line_t_ns)) {
		v_blank_t_ns = 3 * line_t_ns;
		DPU_DEBUG_BTS("\t-WARN: v_blank_t_ns is abnormal!(-> %d%)\n",
				v_blank_t_ns);
	}

	DPU_DEBUG_BTS("\t-line_t_ns(%d) v_blank_t_ns(%d)\n",
			line_t_ns, v_blank_t_ns);

	return v_blank_t_ns;
}

u32 dpu_bts_find_nearest_high_freq(struct decon_device *decon, u32 aclk_base)
{
	int i;

	for (i = (decon->bts.dfs_lv_cnt - 1); i >= 0; i--) {
		if (aclk_base <= decon->bts.dfs_lv[i])
			break;
	}
	if (i < 0) {
		DPU_DEBUG_BTS("\taclk_base is over L0 frequency!");
		i = 0;
	}
	DPU_DEBUG_BTS("\tNearest DFS: %d KHz @L%d\n", decon->bts.dfs_lv[i], i);

	return i;
}

/*
 * [caution] src_w/h is rotated size info
 * - src_w : src_h @original input image
 * - src_h : src_w @original input image
 */
static u32 dpu_bts_get_rotate_tx_allow_t(struct decon_device *decon, u32 aclk_base,
		u32 ppc, u32 src_w, u32 dst_w, u32 dst_y,
		bool is_comp, bool is_scale, bool is_dsc)
{
	u32 dfs_idx = 0;
	u32 dpu_cycle, basic_cycle, dsi_cycle, module_cycle = 0;
	u32 comp_cycle = 0, rot_cycle = 0, scale_cycle = 0, dsc_cycle = 0;
	u64 rot_clk;
	u32 aclk_x_1k_ns, dpu_lat_t_ns, max_lat_t_ns, tx_allow_t_ns;
	s32 start_margin_t_ns;

	dfs_idx = dpu_bts_find_nearest_high_freq(decon, aclk_base);
	rot_clk = decon->bts.dfs_lv[dfs_idx];

	/* post DECON OUTFIFO based on 1H transfer */
	dsi_cycle = decon->lcd_info->xres;

	/* get additional pipeline latency */
	rot_cycle = dpu_bts_rotate_latency(src_w,
		decon->bts.ppc_rotator);
	DPU_DEBUG_BTS("\tROT: lat_cycle(%d)\n", rot_cycle);
	module_cycle += rot_cycle;
	if (is_comp) {
		comp_cycle = dpu_bts_comp_latency(src_w, ppc,
			decon->bts.delay_comp);
		DPU_DEBUG_BTS("\tCOMP: lat_cycle(%d)\n", comp_cycle);
		module_cycle += comp_cycle;
	}
	if (is_scale) {
		scale_cycle = dpu_bts_scale_latency(src_w, dst_w,
			decon->bts.ppc_scaler, decon->bts.delay_scaler);
		DPU_DEBUG_BTS("\tSCALE: lat_cycle(%d)\n", scale_cycle);
		module_cycle += scale_cycle;
	}
	if (is_dsc) {
		dsc_cycle = dpu_bts_dsc_latency(decon->lcd_info->dsc.slice_num,
			decon->lcd_info->dsc.cnt, dst_w, ppc);
		DPU_DEBUG_BTS("\tDSC: lat_cycle(%d)\n", dsc_cycle);
		module_cycle += dsc_cycle;
		dsi_cycle = (dsi_cycle + 2) / 3;
	}

	/*
	 * basic cycle(+ bubble: 10%) + additional cycle based on function
	 * cycle count increases when ACLK goes up due to other conditions
	 * At latency monitor experiment using unit test,
	 *  cycles at 400Mhz were increased by about 800 compared to 200Mhz.
	 * Here, (aclk_mhz * 2) cycles are reflected referring to the result
	 *  because the exact value is unknown.
	 */
	basic_cycle = (decon->lcd_info->xres * 11 / 10 + dsi_cycle) / ppc;
	dpu_cycle = (basic_cycle + module_cycle) + rot_clk * 2 / 1000U;
	aclk_x_1k_ns = dpu_bts_convert_aclk_to_ns(rot_clk / 1000U);
	dpu_lat_t_ns = (dpu_cycle * aclk_x_1k_ns) / 1000U;
#if IS_ENABLED(CONFIG_MCD_PANEL)
	start_margin_t_ns = (s32)dpu_bts_get_one_line_time(decon->lcd_info->yres +
			decon->lcd_info->vfp + decon->lcd_info->vsa + decon->lcd_info->vbp,
			decon->bts.fps) * (dst_y - 1);
#else
	start_margin_t_ns = (s32)dpu_bts_get_one_line_time(decon->lcd_info) * (dst_y - 1);
#endif
	max_lat_t_ns = dpu_bts_get_vblank_time_ns(decon) + start_margin_t_ns;
	if (max_lat_t_ns > dpu_lat_t_ns) {
		tx_allow_t_ns = max_lat_t_ns - dpu_lat_t_ns;
	} else {
		tx_allow_t_ns = max_lat_t_ns;
		DPU_DEBUG_BTS("\tWARN: latency calc result is over tx_allow_t_ns!\n");
	}
	tx_allow_t_ns = tx_allow_t_ns * decon->bts.rot_util / 100;

	DPU_DEBUG_BTS("\t-dpu_cycle(%d) aclk_x_1k_ns(%d) dpu_lat_t_ns(%d)\n",
			dpu_cycle, aclk_x_1k_ns, dpu_lat_t_ns);
	DPU_DEBUG_BTS("\t-start_margin_t_ns(%d) tx_allow_t_ns(%d)\n",
			start_margin_t_ns, tx_allow_t_ns);

	return tx_allow_t_ns;
}

/*
 * [caution] src_w/h is rotated size info
 * - src_w : src_h @original input image
 * - src_h : src_w @original input image
 */
u64 dpu_bts_calc_rotate_aclk(struct decon_device *decon, u32 aclk_base,
		u32 ppc, u32 bpp, u32 src_w, u32 dst_w, u32 dst_y,
		bool is_comp, bool is_scale, bool is_dsc)
{
	u32 dfs_idx = 0;
	u32 dpu_cycle, basic_cycle, dsi_cycle, module_cycle = 0;
	u32 comp_cycle = 0, rot_cycle = 0, scale_cycle = 0, dsc_cycle = 0;
	u32 rot_init_bw = 0;
	u64 rot_clk, rot_need_clk;
	u32 aclk_x_1k_ns, dpu_lat_t_ns, max_lat_t_ns, tx_allow_t_ns;
	s32 start_margin_t_ns;
	u32 temp_clk;
	bool retry_flag = false;

	DPU_DEBUG_BTS("[ROT+] BEFORE latency check: %d KHz\n", aclk_base);

	dfs_idx = dpu_bts_find_nearest_high_freq(decon, aclk_base);
	rot_clk = decon->bts.dfs_lv[dfs_idx];

	/* post DECON OUTFIFO based on 1H transfer */
	dsi_cycle = decon->lcd_info->xres;

	/* get additional pipeline latency */
	rot_cycle = dpu_bts_rotate_latency(src_w,
		decon->bts.ppc_rotator);
	DPU_DEBUG_BTS("\tROT: lat_cycle(%d)\n", rot_cycle);
	module_cycle += rot_cycle;
	if (is_comp) {
		comp_cycle = dpu_bts_comp_latency(src_w, ppc,
			decon->bts.delay_comp);
		DPU_DEBUG_BTS("\tCOMP: lat_cycle(%d)\n", comp_cycle);
		module_cycle += comp_cycle;
	}
	if (is_scale) {
		scale_cycle = dpu_bts_scale_latency(src_w, dst_w,
			decon->bts.ppc_scaler, decon->bts.delay_scaler);
		DPU_DEBUG_BTS("\tSCALE: lat_cycle(%d)\n", scale_cycle);
		module_cycle += scale_cycle;
	}
	if (is_dsc) {
		dsc_cycle = dpu_bts_dsc_latency(decon->lcd_info->dsc.slice_num,
			decon->lcd_info->dsc.cnt, dst_w, ppc);
		DPU_DEBUG_BTS("\tDSC: lat_cycle(%d)\n", dsc_cycle);
		module_cycle += dsc_cycle;
		dsi_cycle = (dsi_cycle + 2) / 3;
	}

	/*
	 * basic cycle(+ bubble: 10%) + additional cycle based on function
	 * cycle count increases when ACLK goes up due to other conditions
	 * At latency monitor experiment using unit test,
	 *  cycles at 400Mhz were increased by about 800 compared to 200Mhz.
	 * Here, (aclk_mhz * 2) cycles are reflected referring to the result
	 *  because the exact value is unknown.
	 */
	basic_cycle = (decon->lcd_info->xres * 11 / 10 + dsi_cycle) / ppc;

retry_hi_freq:
	dpu_cycle = (basic_cycle + module_cycle) + rot_clk * 2 / 1000U;
	aclk_x_1k_ns = dpu_bts_convert_aclk_to_ns(rot_clk / 1000U);
	dpu_lat_t_ns = (dpu_cycle * aclk_x_1k_ns) / 1000U;
#if IS_ENABLED(CONFIG_MCD_PANEL)
	start_margin_t_ns = (s32)dpu_bts_get_one_line_time(decon->lcd_info->yres +
			decon->lcd_info->vfp + decon->lcd_info->vsa + decon->lcd_info->vbp,
			decon->bts.fps) * (dst_y - 1);
#else
	start_margin_t_ns = (s32)dpu_bts_get_one_line_time(decon->lcd_info) * (dst_y - 1);
#endif
	max_lat_t_ns = dpu_bts_get_vblank_time_ns(decon) + start_margin_t_ns;
	if (max_lat_t_ns > dpu_lat_t_ns) {
		tx_allow_t_ns = max_lat_t_ns - dpu_lat_t_ns;
	} else {
		tx_allow_t_ns = max_lat_t_ns;
		DPU_DEBUG_BTS("\tWARN: latency calc result is over tx_allow_t_ns!\n");
	}

	tx_allow_t_ns = tx_allow_t_ns * decon->bts.rot_util / 100;
	rot_init_bw = (u64)src_w * ROT_READ_BYTE * bpp / 8 * 1000U * 1000U /
				tx_allow_t_ns;
	rot_need_clk = rot_init_bw / decon->bts.bus_width;

	if (rot_need_clk > rot_clk) {
		/* not max level */
		if ((int)dfs_idx > 0) {
			/* check if calc_clk is greater than 1-step */
			dfs_idx--;
			temp_clk = decon->bts.dfs_lv[dfs_idx];
			if ((rot_need_clk > temp_clk) && (!retry_flag)) {
				DPU_DEBUG_BTS("\t-allow_ns(%d) dpu_ns(%d)\n",
					tx_allow_t_ns, dpu_lat_t_ns);
				rot_clk = temp_clk;
				retry_flag = true;
				goto retry_hi_freq;
			}
		}
		rot_clk = rot_need_clk;
	}

	DPU_DEBUG_BTS("\t-dpu_cycle(%d) aclk_x_1k_ns(%d) dpu_lat_t_ns(%d)\n",
			dpu_cycle, aclk_x_1k_ns, dpu_lat_t_ns);
	DPU_DEBUG_BTS("\t-start_margin_t_ns(%d) tx_allow_t_ns(%d)\n",
			start_margin_t_ns, tx_allow_t_ns);
	DPU_DEBUG_BTS("\t-rot_init_bw(%d) rot_need_clk(%d)\n",
			rot_init_bw, (u32)rot_need_clk);
	DPU_DEBUG_BTS("[ROT-] AFTER latency check: %d KHz\n", (u32)rot_clk);

	return rot_clk;
}

u64 dpu_bts_calc_aclk_disp(struct decon_device *decon,
		struct decon_win_config *config, u64 resol_clk, u32 max_clk)
{
	u64 s_ratio_h, s_ratio_v;
	u64 aclk_disp, aclk_base;
	u64 ppc;
	struct decon_frame *src = &config->src;
	struct decon_frame *dst = &config->dst;
	const struct dpu_fmt *fmt_info = dpu_find_fmt_info(config->format);
	u32 src_w, src_h, bpp;
	bool is_rotate = is_rotation(config) ? true : false;
	bool is_comp = false;
	bool is_scale = false;
	bool is_dsc = false;

	if (is_rotate) {
		src_w = src->h;
		src_h = src->w;
	} else {
		src_w = src->w;
		src_h = src->h;
	}

	s_ratio_h = (src_w <= dst->w) ? MULTI_FACTOR : MULTI_FACTOR * (u64)src_w / (u64)dst->w;
	s_ratio_v = (src_h <= dst->h) ? MULTI_FACTOR : MULTI_FACTOR * (u64)src_h / (u64)dst->h;

	/* case for using dsc encoder 1ea at decon0 or decon1 */
	if ((decon->id != 2) && (decon->lcd_info->dsc.cnt == 1))
		ppc = ((decon->bts.ppc / 2UL) >= 1UL) ? (decon->bts.ppc / 2UL) : 1UL;
	else
		ppc = decon->bts.ppc;

	if (decon->bts.ppc_scaler && (decon->bts.ppc_scaler < ppc))
		ppc = decon->bts.ppc_scaler;

	aclk_disp = resol_clk * s_ratio_h * s_ratio_v * DISP_FACTOR  / 100UL
		/ ppc / (MULTI_FACTOR * MULTI_FACTOR);

	if (aclk_disp < (resol_clk / ppc))
		aclk_disp = resol_clk / ppc;

	if (!is_rotate)
		return aclk_disp;

	/* rotation case: check if latency conditions are met */
	if (aclk_disp > max_clk)
		aclk_base = aclk_disp;
	else
		aclk_base = max_clk;

	bpp = (u32)(fmt_info->bpp + fmt_info->padding);
	if (fmt_info->ct)
		is_comp = true;
	if ((s_ratio_h != MULTI_FACTOR) || (s_ratio_v != MULTI_FACTOR))
		is_scale = true;
	if (decon->lcd_info->dsc.en)
		is_dsc = true;

	aclk_disp = dpu_bts_calc_rotate_aclk(decon, (u32)aclk_base, (u32)ppc,
			bpp, src_w, dst->w, dst->y, is_comp, is_scale, is_dsc);

	return aclk_disp;
}

static void dpu_bts_sum_all_decon_bw(struct decon_device *decon, u32 ch_bw[])
{
	int i, j;

	if (decon->id < 0 || decon->id >= decon->dt.decon_cnt) {
		decon_warn("[%s] undefined decon id(%d)!\n", __func__, decon->id);
		return;
	}

	for (i = 0; i < BTS_DPU_MAX; ++i)
		decon->bts.ch_bw[decon->id][i] = ch_bw[i];

	for (i = 0; i < decon->dt.decon_cnt; ++i) {
		if (decon->id == i)
			continue;

		for (j = 0; j < BTS_DPU_MAX; ++j)
			ch_bw[j] += decon->bts.ch_bw[i][j];
	}
}

static void dpu_bts_find_max_disp_freq(struct decon_device *decon,
		struct decon_reg_data *regs)
{
	int i, j;
	u32 disp_ch_bw[BTS_DPU_MAX];
	u32 max_disp_ch_bw;
	u32 disp_op_freq = 0, freq = 0;
	struct decon_win_config *config = regs->dpp_config;
	u32 wclk;

	memset(disp_ch_bw, 0, sizeof(disp_ch_bw));

	for (i = 0; i < BTS_DPP_MAX; ++i)
		for (j = 0; j < BTS_DPU_MAX; ++j)
			if (decon->bts.bw[i].ch_num == j)
				disp_ch_bw[j] += decon->bts.bw[i].val;

	/* must be considered other decon's bw */
	dpu_bts_sum_all_decon_bw(decon, disp_ch_bw);

	for (i = 0; i < BTS_DPU_MAX; ++i)
		if (disp_ch_bw[i])
			DPU_DEBUG_BTS("\tAXI_DPU%d = %d\n", i, disp_ch_bw[i]);

	max_disp_ch_bw = disp_ch_bw[0];
	for (i = 1; i < BTS_DPU_MAX; ++i)
		if (max_disp_ch_bw < disp_ch_bw[i])
			max_disp_ch_bw = disp_ch_bw[i];

	decon->bts.peak = max_disp_ch_bw;
	if (max_disp_ch_bw < decon->bts.write_bw)
		decon->bts.peak = decon->bts.write_bw;
	decon->bts.max_disp_freq = decon->bts.peak / decon->bts.inner_width;
	disp_op_freq = decon->bts.max_disp_freq;

	for (i = 0; i < decon->dt.max_win; ++i) {
		if ((config[i].state != DECON_WIN_STATE_BUFFER) &&
				(config[i].state != DECON_WIN_STATE_COLOR))
			continue;

		freq = dpu_bts_calc_aclk_disp(decon, &config[i],
				(u64)decon->bts.resol_clk, disp_op_freq);
		if (disp_op_freq < freq)
			disp_op_freq = freq;
	}

	DPU_DEBUG_BTS("\tDISP bus freq(%d), operating freq(%d)\n",
			decon->bts.max_disp_freq, disp_op_freq);

	if (decon->bts.max_disp_freq < disp_op_freq)
		decon->bts.max_disp_freq = disp_op_freq;

	/*
	 * disp-aclk should be at least higher than wclk.
	 * otherwise, dsim link may mal-function.
	 */
	if (decon->dt.out_type == DECON_OUT_DSI) {
		wclk = DIV_ROUND_UP(decon->lcd_info->hs_clk, 16);
		if (decon->bts.max_disp_freq < wclk * 1000)
			decon->bts.max_disp_freq = wclk * 1000;
	}

#if defined(CONFIG_EXYNOS_DISPLAYPORT)
	if (decon->dt.out_type == DECON_OUT_DP) {
		if (decon->bts.max_disp_freq < 200000)
			decon->bts.max_disp_freq = 200000;
	}
#endif

	DPU_DEBUG_BTS("\tMAX DISP FREQ = %d\n", decon->bts.max_disp_freq);
}

static void dpu_bts_share_bw_info(int id)
{
	int i, j;
	struct decon_device *decon[MAX_DECON_CNT];
	int decon_cnt;

	decon_cnt = get_decon_drvdata(0)->dt.decon_cnt;

	for (i = 0; i < MAX_DECON_CNT; i++)
		decon[i] = NULL;

	for (i = 0; i < decon_cnt; i++)
		decon[i] = get_decon_drvdata(i);

	for (i = 0; i < decon_cnt; ++i) {
		if (id == i || decon[i] == NULL)
			continue;

		for (j = 0; j < BTS_DPU_MAX; ++j)
			decon[i]->bts.ch_bw[id][j] = decon[id]->bts.ch_bw[id][j];
	}
}

static void dpu_bts_set_bus_qos(struct decon_device *decon)
{
	u32 wclk;

	/*
	 * normally min INT freq in LCD screen on state : 200Mhz
	 * wqhd(+) high fps case min INT freq in screen on state : 400Mhz
	 */
	if ((decon->lcd_info->xres >= XRES_FOR_HIGH_INTCLK)
		&& (decon->lcd_info->yres >= YRES_FOR_HIGH_INTCLK)
		&& (decon->bts.fps >= FPS_FOR_HIGH_INTCLK)) {
		exynos_pm_qos_update_request(&decon->bts.int_qos, 400 * 1000);
		exynos_pm_qos_update_request(&decon->bts.mif_qos, 546 * 1000);
	} else
		exynos_pm_qos_update_request(&decon->bts.int_qos, 200 * 1000);

	/*
	 * disp-aclk should be at least higher than wclk.
	 * otherwise, dsim link may mal-function.
	 */
	if (decon->dt.out_type == DECON_OUT_DSI) {
		wclk = DIV_ROUND_UP(decon->lcd_info->hs_clk, 16);
		exynos_pm_qos_update_request(&decon->bts.disp_qos, wclk * 1000);
	}

}

void dpu_bts_calc_bw(struct decon_device *decon, struct decon_reg_data *regs)
{
	struct decon_win_config *config = regs->dpp_config;
	struct bts_decon_info bts_info;
	const struct dpu_fmt *fmt_info;
	enum dpp_rotate rot;
	int idx, i;
	u32 write_bw = 0, total_bw = 0;
	u64 resol_clock;
	u64 ch_bw = 0, rot_bw;
	u32 src_w, src_h;
	u32 dst_w, dst_h;
	u32 bpp, fps;
	u32 tx_allow_ns;
	u64 s_ratio_h, s_ratio_v;
	u64 ppc;
	u32 aclk_base;
	bool is_comp, is_scale, is_dsc;

	if (!decon->bts.enabled)
		return;

	DPU_DEBUG_BTS("\n");
	DPU_DEBUG_BTS("%s + : DECON%d\n", __func__, decon->id);

	memset(&bts_info, 0, sizeof(struct bts_decon_info));

	if (decon->dt.out_type == DECON_OUT_WB) {
		decon->lcd_info->xres = regs->dpp_config[decon->dt.wb_win].dst.w;
		decon->lcd_info->yres = regs->dpp_config[decon->dt.wb_win].dst.h;
		if (regs->lib_requested)
			decon->lcd_info->fps = decon->bts.sbwc_lib_boost;
	}

#if defined(CONFIG_DECON_BTS_VRR_ASYNC)
	if (decon->dt.out_type == DECON_OUT_DSI) {
		u32 bts_fps =
			decon_bts_get_bts_fps(&decon->bts, decon->vsync.count);

		if (decon->bts.fps != bts_fps) {
			DPU_DEBUG_BTS("\tupdate fps(%d->%d) timeline(%llu)\n",
					decon->bts.fps, bts_fps, decon->vsync.count);
			decon->bts.fps = bts_fps;
		}
	} else {
		decon->bts.fps = decon->lcd_info->fps;
	}
#else
	decon->bts.fps = decon->lcd_info->fps;
#endif

	resol_clock = dpu_bts_get_resol_clock(decon->lcd_info->xres,
				decon->lcd_info->yres, decon->bts.fps);
	decon->bts.resol_clk = (u32)resol_clock;
	DPU_DEBUG_BTS("[Run: D%d] resol clock = %d Khz @%d fps\n",
		decon->id, decon->bts.resol_clk, decon->bts.fps);

	bts_info.vclk = decon->bts.resol_clk;
	bts_info.lcd_w = decon->lcd_info->xres;
	bts_info.lcd_h = decon->lcd_info->yres;

	for (i = 0; i < decon->dt.max_win + 2; ++i) {
		if (config[i].state == DECON_WIN_STATE_BUFFER) {
			idx = config[i].channel;
			bts_info.dpp[idx].used = true;
		} else {
			continue;
		}

		fmt_info = dpu_find_fmt_info(config[i].format);
		bts_info.dpp[idx].bpp = fmt_info->bpp + fmt_info->padding;
		bts_info.dpp[idx].src_w = config[i].src.w;
		bts_info.dpp[idx].src_h = config[i].src.h;
		bts_info.dpp[idx].dst.x1 = config[i].dst.x;
		bts_info.dpp[idx].dst.x2 = config[i].dst.x + config[i].dst.w;
		bts_info.dpp[idx].dst.y1 = config[i].dst.y;
		bts_info.dpp[idx].dst.y2 = config[i].dst.y + config[i].dst.h;
		rot = config[i].dpp_parm.rot;
		bts_info.dpp[idx].rotation = (rot > DPP_ROT_180) ? true : false;
		bts_info.dpp[idx].compression = config[i].compression;

		if (rot > DPP_ROT_180) {
			src_w = config[i].src.h;
			src_h = config[i].src.w;
		} else {
			src_w = config[i].src.w;
			src_h = config[i].src.h;
		}
		dst_w = config[i].dst.w;
		dst_h = config[i].dst.h;
		fps = decon->bts.fps;
		bpp = (u32)(fmt_info->bpp + fmt_info->padding);

		s_ratio_h = (src_w <= dst_w) ? MULTI_FACTOR : MULTI_FACTOR * (u64)src_w / (u64)dst_w;
		s_ratio_v = (src_h <= dst_h) ? MULTI_FACTOR : MULTI_FACTOR * (u64)src_h / (u64)dst_h;

		/* BW(KB) : s_ratio_h * s_ratio_v * (bpp/8) * resol_clk (* dst_w / xres) */
		if (decon->bts.fps >= FPS_FOR_HIGH_INTCLK)
			ch_bw = s_ratio_h * s_ratio_v * bpp / 8 * resol_clock
				/ (MULTI_FACTOR * MULTI_FACTOR);
		else
			ch_bw = s_ratio_h * s_ratio_v * bpp / 8 * resol_clock * (u64)dst_w / bts_info.lcd_w
				/ (MULTI_FACTOR * MULTI_FACTOR);

		if (rot > DPP_ROT_180) {
			/* case for using dsc encoder 1ea at decon0 or decon1 */
			if ((decon->id != 2) && (decon->lcd_info->dsc.cnt == 1))
				ppc = ((decon->bts.ppc / 2UL) >= 1UL) ? (decon->bts.ppc / 2UL) : 1UL;
			else
				ppc = decon->bts.ppc;
#if IS_ENABLED(CONFIG_EXYNOS_PM_QOS) || IS_ENABLED(CONFIG_EXYNOS_PM_QOS_MODULE)
			aclk_base = exynos_devfreq_get_domain_freq(DEVFREQ_DISP);
			if (aclk_base < (decon->bts.resol_clk / ppc))
				aclk_base = decon->bts.resol_clk / ppc;
#else
			aclk_base = decon->bts.resol_clk / ppc;
#endif
			is_comp = fmt_info->ct ? true : false;
			is_scale = (src_w != dst_w) || (src_h != dst_h) ? true : false;
			is_dsc = decon->lcd_info->dsc.en;
			tx_allow_ns = dpu_bts_get_rotate_tx_allow_t(decon, aclk_base, (u32)ppc,
					src_w, dst_w, config[i].dst.y, is_comp, is_scale, is_dsc);
			/* BW(KB) : sh * 32B * (bpp/8) / tx_allow_t */
			rot_bw = (u64)src_w * ROT_READ_BYTE * bpp / 8 * 1000000U /
					tx_allow_ns;
			if (rot_bw > ch_bw) {
				DPU_DEBUG_BTS("\tDPP%d ch_bw = %d, rot_bw = %d\n", idx, ch_bw, rot_bw);
				ch_bw = rot_bw;
			}
		}

		bts_info.dpp[idx].bw = (u32)ch_bw;

		/* check write(ODMA) case */
		if (i == decon->dt.wb_win)
			write_bw += bts_info.dpp[idx].bw;
		total_bw += bts_info.dpp[idx].bw;

		DPU_DEBUG_BTS("\tDPP%d BW = %d\n", idx, bts_info.dpp[idx].bw);
		DPU_DEBUG_BTS("\t\tSRC w(%d) h(%d) rot(%d) fmt(%s) bpp(%d) afbc(%d)\n",
				bts_info.dpp[idx].src_w,
				bts_info.dpp[idx].src_h,
				bts_info.dpp[idx].rotation,
				fmt_info->name, bts_info.dpp[idx].bpp,
				config[i].compression);
		DPU_DEBUG_BTS("\t\tDST x(%d) right(%d) y(%d) bottom(%d)\n",
				bts_info.dpp[idx].dst.x1,
				bts_info.dpp[idx].dst.x2,
				bts_info.dpp[idx].dst.y1,
				bts_info.dpp[idx].dst.y2);
	}

	decon->bts.read_bw = total_bw - write_bw;
	decon->bts.write_bw = write_bw;
	decon->bts.total_bw = total_bw;
	memcpy(&decon->bts.bts_info, &bts_info, sizeof(struct bts_decon_info));

	for (i = 0; i < BTS_DPP_MAX; ++i)
		decon->bts.bw[i].val = bts_info.dpp[i].bw;

	DPU_DEBUG_BTS("\tDECON%d Total.BW(KB) = %d, Rd.BW = %d, Wr.BW = %d\n",
			decon->id, decon->bts.total_bw,
			decon->bts.read_bw, decon->bts.write_bw);

	dpu_bts_find_max_disp_freq(decon, regs);

	/* update bw for other decons */
	dpu_bts_share_bw_info(decon->id);

	DPU_DEBUG_BTS("%s -\n", __func__);
}

void dpu_bts_update_bw(struct decon_device *decon, struct decon_reg_data *regs,
		u32 is_after)
{
	struct bts_bw bw = { 0, };
	int i;
#if defined(CONFIG_EXYNOS_DISPLAYPORT)
	struct displayport_device *displayport = get_displayport_drvdata();
	videoformat cur = V640X480P60;
	__u64 pixelclock = 0;
	u32 sst_id = SST1;

	if (decon->dt.out_type == DECON_OUT_DP) {
		sst_id = displayport_get_sst_id_with_decon_id(decon->id);
		cur = displayport->sst[sst_id]->cur_video;
		pixelclock = supported_videos[cur].dv_timings.bt.pixelclock;
	}
#endif

	DPU_DEBUG_BTS("%s +\n", __func__);

	if (!decon->bts.enabled)
		return;

	/* update peak & R/W bandwidth per DPU port */
	bw.peak = (u64)decon->bts.peak * decon->bts.bw_weight / 100UL;
	bw.read = (u64)decon->bts.read_bw * decon->bts.bw_weight / 100UL;
	bw.write = (u64)decon->bts.write_bw * decon->bts.bw_weight / 100UL;
	DPU_DEBUG_BTS("\tpeak = %d, read = %d, write = %d, lib_requested = %d\n",
			bw.peak, bw.read, bw.write, regs->lib_requested);

	/* for high fps, add weight to read bw to prevent underrun */
	if ((decon->bts.fps >= FPS_FOR_HIGH_INTCLK)
		&& ((bw.read + bw.write) < MAX_BW_FOR_BW_WEIGHT)) {
		bw.read = (u64)bw.read * BW_WEIGHT_FOR_HIGH_FPS
					/ DISP_FACTOR;
		bw.write = (u64)bw.write * BW_WEIGHT_FOR_HIGH_FPS
					/ DISP_FACTOR;
		DPU_DEBUG_BTS("\tread = %d, write = %d after %d%% weight for high fps\n",
				bw.read, bw.write, (BW_WEIGHT_FOR_HIGH_FPS - 100));
	}

	if (is_after) { /* after DECON h/w configuration */
		if (regs->lib_requested) {
			for (i = 0; i < BTS_DPU_MAX; i++)
				decon->bts.ch_bw[decon->id][i] = 0;
			/* update bw for other decons */
			dpu_bts_share_bw_info(decon->id);
			decon->bts.total_bw = 0;
			bw.peak = 0;
			bw.read = 0;
			bw.write = 0;
			decon->bts.max_disp_freq = 0;
			decon->lcd_info->fps = LCD_REFRESH_RATE;
			regs->lib_requested = false;
			decon->bts.fps = decon->lcd_info->fps;
		}
		if (decon->bts.total_bw <= decon->bts.prev_total_bw)
			bts_update_bw(decon->bts.bw_idx, bw);

#if defined(CONFIG_EXYNOS_DISPLAYPORT)
		if ((displayport->sst[sst_id]->state == DISPLAYPORT_STATE_ON)
			&& (pixelclock >= 533000000)) /* 4K DP case */
			return;
#endif

#if IS_ENABLED(CONFIG_EXYNOS_PM_QOS) || IS_ENABLED(CONFIG_EXYNOS_PM_QOS_MODULE)
		if (decon->bts.max_disp_freq <= decon->bts.prev_max_disp_freq)
			exynos_pm_qos_update_request(&decon->bts.disp_qos,
					decon->bts.max_disp_freq);
#endif

		decon->bts.prev_total_bw = decon->bts.total_bw;
		decon->bts.prev_max_disp_freq = decon->bts.max_disp_freq;
	} else {
		if (decon->bts.total_bw > decon->bts.prev_total_bw)
			bts_update_bw(decon->bts.bw_idx, bw);

#if defined(CONFIG_EXYNOS_DISPLAYPORT)
		if ((displayport->sst[sst_id]->state == DISPLAYPORT_STATE_ON)
			&& (pixelclock >= 533000000)) /* 4K DP case */
			return;
#endif

#if IS_ENABLED(CONFIG_EXYNOS_PM_QOS) || IS_ENABLED(CONFIG_EXYNOS_PM_QOS_MODULE)
		if (decon->bts.max_disp_freq > decon->bts.prev_max_disp_freq)
			exynos_pm_qos_update_request(&decon->bts.disp_qos,
					decon->bts.max_disp_freq);
#endif
	}

	DPU_DEBUG_BTS("%s -\n", __func__);
}

void dpu_bts_acquire_bw(struct decon_device *decon)
{
#if defined(CONFIG_DECON_BTS_LEGACY) && defined(CONFIG_EXYNOS_DISPLAYPORT)
	struct displayport_device *displayport = get_displayport_drvdata();
	videoformat cur = V640X480P60;
	__u64 pixelclock = 0;
	u32 sst_id = SST1;
#endif
	struct decon_win_config config;
	u64 resol_clock;
	u32 aclk_freq = 0;

#if defined(CONFIG_DECON_BTS_LEGACY) && defined(CONFIG_EXYNOS_DISPLAYPORT)
	if (decon->dt.out_type == DECON_OUT_DP) {
		sst_id = displayport_get_sst_id_with_decon_id(decon->id);
		cur = displayport->sst[sst_id]->cur_video;
		pixelclock = supported_videos[cur].dv_timings.bt.pixelclock;
	}
#endif

	DPU_DEBUG_BTS("%s +\n", __func__);

	if (!decon->bts.enabled)
		return;

	decon->bts.fps = decon->lcd_info->fps;
#if defined(CONFIG_DECON_BTS_VRR_ASYNC)
	if (decon->dt.out_type == DECON_OUT_DSI) {
		decon_bts_clear_fps_sync(&decon->bts);
		decon_bts_set_applied_fps(&decon->bts, decon->lcd_info->fps);
	}
#endif

#if IS_ENABLED(CONFIG_EXYNOS_PM_QOS) || IS_ENABLED(CONFIG_EXYNOS_PM_QOS_MODULE)
	dpu_bts_set_bus_qos(decon);
	DPU_DEBUG_BTS("Get initial INT freq(%lu)\n",
			exynos_devfreq_get_domain_freq(DEVFREQ_INT));
#endif

	if (decon->dt.out_type == DECON_OUT_DSI) {
		u32 bts_fps;

		/* 
		 * command mode panel trigger will be enabled
		 * until first winconfig. To prevent underrun,
		 * set bts_fps as maximum refresh-rate.
		 */
		bts_fps = IS_EXYNOS_VRR_HS_MODE(decon->lcd_info->vrr_mode) ?
			120 : decon->bts.fps;

		memset(&config, 0, sizeof(struct decon_win_config));
		config.src.w = config.dst.w = decon->lcd_info->xres;
		config.src.h = config.dst.h = decon->lcd_info->yres;

		resol_clock = dpu_bts_get_resol_clock(decon->lcd_info->xres,
				decon->lcd_info->yres, bts_fps);
		decon->bts.resol_clk = (u32)resol_clock;

		aclk_freq = dpu_bts_calc_aclk_disp(decon, &config,
				resol_clock, resol_clock);
		DPU_DEBUG_BTS("Initial calculated disp freq(%lu) @%d fps\n",
				aclk_freq, bts_fps);
#if IS_ENABLED(CONFIG_EXYNOS_PM_QOS) || IS_ENABLED(CONFIG_EXYNOS_PM_QOS_MODULE)
		/*
		 * If current disp freq is higher than calculated freq,
		 * it must not be set. if not, underrun can occur.
		 */
		if (exynos_devfreq_get_domain_freq(DEVFREQ_DISP) < aclk_freq)
			exynos_pm_qos_update_request(&decon->bts.disp_qos, aclk_freq);

		DPU_DEBUG_BTS("%s -: Get initial disp freq(%lu)\n", __func__,
				exynos_devfreq_get_domain_freq(DEVFREQ_DISP));
#endif
		return;
	}

#if defined(CONFIG_DECON_BTS_LEGACY) && defined(CONFIG_EXYNOS_DISPLAYPORT)
	if (decon->dt.out_type != DECON_OUT_DP)
		return;

#if IS_ENABLED(CONFIG_EXYNOS_PM_QOS) || IS_ENABLED(CONFIG_EXYNOS_PM_QOS_MODULE)
	if (pixelclock >= 500000000) { /*UHD 60Hz*/
		if (exynos_pm_qos_request_active(&decon->bts.mif_qos))
			exynos_pm_qos_update_request(&decon->bts.mif_qos, 1794 * 1000);
		else
			DPU_ERR_BTS("%s mif qos setting error\n", __func__);

		if (exynos_pm_qos_request_active(&decon->bts.int_qos))
			exynos_pm_qos_update_request(&decon->bts.int_qos, 534 * 1000);
		else
			DPU_ERR_BTS("%s int qos setting error\n", __func__);

		if (exynos_pm_qos_request_active(&decon->bts.disp_qos))
			exynos_pm_qos_update_request(&decon->bts.disp_qos, 533 * 1000);
		else
			DPU_ERR_BTS("%s int qos setting error\n", __func__);

		bts_add_scenario(decon->bts.scen_idx[DPU_BS_DP_DEFAULT]);
	} else {
		if (exynos_pm_qos_request_active(&decon->bts.mif_qos))
			exynos_pm_qos_update_request(&decon->bts.mif_qos, 1352 * 1000);
		else
			DPU_ERR_BTS("%s mif qos setting error\n", __func__);
	}

	DPU_DEBUG_BTS("%s: decon%d, pixelclock(%u)\n",
			__func__, decon->id, pixelclock);
#endif
#endif
}

void dpu_bts_hiber_acquire_bw(struct decon_device *decon)
{
	DPU_DEBUG_BTS("%s +\n", __func__);

	if (!decon->bts.enabled)
		return;

	if (decon->dt.out_type == DECON_OUT_DSI) {
#if IS_ENABLED(CONFIG_EXYNOS_PM_QOS) || IS_ENABLED(CONFIG_EXYNOS_PM_QOS_MODULE)
		dpu_bts_set_bus_qos(decon);
#endif
	}
	DPU_DEBUG_BTS("%s -\n", __func__);
}

void dpu_bts_release_bw(struct decon_device *decon)
{
	struct bts_bw bw = { 0, };
	DPU_DEBUG_BTS("%s +\n", __func__);

	if (!decon->bts.enabled)
		return;

	if ((decon->dt.out_type == DECON_OUT_DSI) ||
		(decon->dt.out_type == DECON_OUT_WB)) {
		bts_update_bw(decon->bts.bw_idx, bw);
		decon->bts.prev_total_bw = 0;
#if IS_ENABLED(CONFIG_EXYNOS_PM_QOS) || IS_ENABLED(CONFIG_EXYNOS_PM_QOS_MODULE)
		exynos_pm_qos_update_request(&decon->bts.disp_qos, 0);
#endif
		decon->bts.prev_max_disp_freq = 0;
#if IS_ENABLED(CONFIG_EXYNOS_PM_QOS) || IS_ENABLED(CONFIG_EXYNOS_PM_QOS_MODULE)
		exynos_pm_qos_update_request(&decon->bts.int_qos, 0);
#endif
	} else if (decon->dt.out_type == DECON_OUT_DP) {
#if IS_ENABLED(CONFIG_EXYNOS_PM_QOS) || IS_ENABLED(CONFIG_EXYNOS_PM_QOS_MODULE)
#if defined(CONFIG_DECON_BTS_LEGACY) && defined(CONFIG_EXYNOS_DISPLAYPORT)
		if (exynos_pm_qos_request_active(&decon->bts.mif_qos))
			exynos_pm_qos_update_request(&decon->bts.mif_qos, 0);
		else
			DPU_ERR_BTS("%s mif qos setting error\n", __func__);

		if (exynos_pm_qos_request_active(&decon->bts.int_qos))
			exynos_pm_qos_update_request(&decon->bts.int_qos, 0);
		else
			DPU_ERR_BTS("%s int qos setting error\n", __func__);

		if (exynos_pm_qos_request_active(&decon->bts.disp_qos))
			exynos_pm_qos_update_request(&decon->bts.disp_qos, 0);
		else
			DPU_ERR_BTS("%s int qos setting error\n", __func__);

		bts_del_scenario(decon->bts.scen_idx[DPU_BS_DP_DEFAULT]);
#endif
#endif
	}

	DPU_DEBUG_BTS("%s -\n", __func__);
}

void dpu_bts_hiber_release_bw(struct decon_device *decon)
{
	struct bts_bw bw = { 0, };
	DPU_DEBUG_BTS("%s +\n", __func__);

	if (!decon->bts.enabled)
		return;

	if (decon->dt.out_type == DECON_OUT_DSI) {
		bts_update_bw(decon->bts.bw_idx, bw);
		decon->bts.prev_total_bw = 0;
#if IS_ENABLED(CONFIG_EXYNOS_PM_QOS) || IS_ENABLED(CONFIG_EXYNOS_PM_QOS_MODULE)
		exynos_pm_qos_update_request(&decon->bts.disp_qos, 0);
		exynos_pm_qos_update_request(&decon->bts.int_qos, 0);
#endif
		decon->bts.prev_max_disp_freq = 0;
	}
	DPU_DEBUG_BTS("%s -\n", __func__);
}

void dpu_bts_acquire_dqe_bw(struct decon_device *decon)
{
	DPU_DEBUG_BTS("%s +\n", __func__);

	if (!decon->bts.enabled)
		return;

	if (decon->dt.out_type == DECON_OUT_DSI) {
#if IS_ENABLED(CONFIG_EXYNOS_PM_QOS) || IS_ENABLED(CONFIG_EXYNOS_PM_QOS_MODULE)
#if defined(CONFIG_EXYNOS_DECON_DQE)
		exynos_pm_qos_update_request(&decon->bts.disp_qos, 267 * 1000);
#endif
#endif
	}
	DPU_DEBUG_BTS("%s -\n", __func__);
}

void dpu_bts_release_dqe_bw(struct decon_device *decon)
{
	u32 wclk;

	DPU_DEBUG_BTS("%s +\n", __func__);

	if (!decon->bts.enabled)
		return;

	if (decon->dt.out_type == DECON_OUT_DSI) {
#if IS_ENABLED(CONFIG_EXYNOS_PM_QOS) || IS_ENABLED(CONFIG_EXYNOS_PM_QOS_MODULE)
#if defined(CONFIG_EXYNOS_DECON_DQE)
		/*
		 * disp-aclk should be at least higher than wclk.
		 * otherwise, dsim link may mal-function.
		 */
		wclk = DIV_ROUND_UP(decon->lcd_info->hs_clk, 16);
		exynos_pm_qos_update_request(&decon->bts.disp_qos, wclk * 1000);
#endif
#endif
	}
	DPU_DEBUG_BTS("%s -\n", __func__);
}

void dpu_bts_init(struct decon_device *decon)
{
	int i;
	u64 resol_clock;
	struct v4l2_subdev *sd = NULL;
	const char *scen_name[DPU_BS_MAX] = {
		"default",
		"mfc_uhd",
		"mfc_uhd_10bit",
		"dp_default",
		/* add scenario & update index of [decon_cal.h] */
	};

	DPU_DEBUG_BTS("%s +\n", __func__);

	decon->bts.enabled = false;

	if (!IS_ENABLED(CONFIG_EXYNOS_BTS)
			&& !IS_ENABLED(CONFIG_EXYNOS_BTS_MODULE)) {
		DPU_ERR_BTS("decon%d bts feature is disabled\n", decon->id);
		return;
	}

	if (decon->id == 1)
		decon->bts.bw_idx = bts_get_bwindex("DECON1");
	else if (decon->id == 2)
		decon->bts.bw_idx = bts_get_bwindex("DECON2");
	else if (decon->id == 3)
		decon->bts.bw_idx = bts_get_bwindex("DECON3");
	else
		decon->bts.bw_idx = bts_get_bwindex("DECON0");

	/*
	 * Get scenario index from BTS driver
	 * Don't try to get index value of "default" scenario
	 */
	for (i = 1; i < DPU_BS_MAX; i++) {
		if (scen_name[i] != NULL)
			decon->bts.scen_idx[i] =
				bts_get_scenindex(scen_name[i]);
	}

	for (i = 0; i < BTS_DPU_MAX; i++)
		decon->bts.ch_bw[decon->id][i] = 0;

	DPU_DEBUG_BTS("BTS_BW_TYPE(%d) -\n", decon->bts.bw_idx);

	if (decon->dt.out_type == DECON_OUT_DP) {
		decon->bts.fps = LCD_REFRESH_RATE;
		resol_clock = dpu_bts_get_resol_clock(BTS_MAX_XRES,
				BTS_MAX_YRES, LCD_REFRESH_RATE);
	} else {
		decon->bts.fps = decon->lcd_info->fps;
#if defined(CONFIG_DECON_BTS_VRR_ASYNC)
		if (decon->dt.out_type == DECON_OUT_DSI) {
			decon_bts_init_fps_sync(&decon->bts);
			decon_bts_set_applied_fps(&decon->bts, decon->lcd_info->fps);
		}
#endif
		resol_clock = dpu_bts_get_resol_clock(decon->lcd_info->xres,
				decon->lcd_info->yres, decon->bts.fps);
	}

	decon->bts.resol_clk = (u32)resol_clock;
	DPU_DEBUG_BTS("[Init: D%d] resol clock = %d Khz @%d fps\n",
		decon->id, decon->bts.resol_clk, decon->bts.fps);
#if IS_ENABLED(CONFIG_EXYNOS_PM_QOS) || IS_ENABLED(CONFIG_EXYNOS_PM_QOS_MODULE)
	exynos_pm_qos_add_request(&decon->bts.mif_qos, PM_QOS_BUS_THROUGHPUT, 0);
	exynos_pm_qos_add_request(&decon->bts.int_qos, PM_QOS_DEVICE_THROUGHPUT, 0);
	exynos_pm_qos_add_request(&decon->bts.disp_qos, PM_QOS_DISPLAY_THROUGHPUT, 0);
#endif
	decon->bts.scen_updated = 0;

	if (decon->id == 0) {
#if IS_ENABLED(CONFIG_EXYNOS_PM_QOS) || IS_ENABLED(CONFIG_EXYNOS_PM_QOS_MODULE)
		/* This is necessary because of bootloader display case */
		dpu_bts_set_bus_qos(decon);
#endif
		DPU_DEBUG_BTS("[Init] INT freq(%lu)\n",
				exynos_devfreq_get_domain_freq(DEVFREQ_INT));
	}

	for (i = 0; i < BTS_DPP_MAX; ++i) {
		sd = decon->dpp_sd[i];
		v4l2_subdev_call(sd, core, ioctl, DPP_GET_PORT_NUM,
				&decon->bts.bw[i].ch_num);
		DPU_INFO_BTS(" CH(%d) Port(%d)\n", i,
				decon->bts.bw[i].ch_num);
	}

	decon->bts.enabled = true;

	DPU_INFO_BTS("%s -: decon%d bts feature is enabled\n",
			__func__, decon->id);
}

void dpu_bts_deinit(struct decon_device *decon)
{
	if (!decon->bts.enabled)
		return;

	DPU_DEBUG_BTS("%s +\n", __func__);
#if IS_ENABLED(CONFIG_EXYNOS_PM_QOS) || IS_ENABLED(CONFIG_EXYNOS_PM_QOS_MODULE)
	exynos_pm_qos_remove_request(&decon->bts.disp_qos);
	exynos_pm_qos_remove_request(&decon->bts.int_qos);
	exynos_pm_qos_remove_request(&decon->bts.mif_qos);
#endif
	DPU_DEBUG_BTS("%s -\n", __func__);
}

struct decon_bts_ops decon_bts_control = {
	.bts_init		= dpu_bts_init,
	.bts_calc_bw		= dpu_bts_calc_bw,
	.bts_update_bw		= dpu_bts_update_bw,
	.bts_acquire_bw		= dpu_bts_acquire_bw,
	.bts_hiber_acquire_bw	= dpu_bts_hiber_acquire_bw,
	.bts_release_bw		= dpu_bts_release_bw,
	.bts_hiber_release_bw	= dpu_bts_hiber_release_bw,
	.bts_acquire_dqe_bw	= dpu_bts_acquire_dqe_bw,
	.bts_release_dqe_bw	= dpu_bts_release_dqe_bw,
	.bts_deinit		= dpu_bts_deinit,
};
