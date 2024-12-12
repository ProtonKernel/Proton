/* SPDX-License-Identifier: GPL-2.0-only
 *
 * Copyright (c) 2019 Samsung Electronics Co., Ltd.
 *		http://www.samsung.com
 *
 * Header file for Exynos2100 DECON CAL
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#ifndef __SAMSUNG_DECON_CAL_H__
#define __SAMSUNG_DECON_CAL_H__

#include "../panels/exynos_panel.h"
#include "../format.h"

#define CEIL(x)			((x-(u32)(x) > 0 ? (u32)(x+1) : (u32)(x)))

#if defined(CONFIG_EXYNOS_LIMIT_ROTATION)
#define ROT_MAX_W		3040
#define ROT_MAX_SZ		3040*1440
#else
#define ROT_MAX_W		4096
#define ROT_MAX_SZ		4096*2160
#endif

#define BTS_DPU_MAX		4 /* DPU0 ~ 3 */
#define BTS_DPP_MAX		(16 + 1) /* DPP0 ~ 15 (+1 = WB/READBACK) */
#define BTS_DFS_MAX		8 /* DPU DVFS Level */

#define ODMA_WB	16

enum decon_dsi_mode {
	DSI_MODE_SINGLE = 0,
	DSI_MODE_DUAL_DSI,
	DSI_MODE_DUAL_DISPLAY,
	DSI_MODE_NONE
};

enum decon_data_path {
	/* No comp - OUTFIFO0 DSIM_IF0 */
	DPATH_NOCOMP_OUTFIFO0_DSIMIF0			= 0x001,
	/* No comp - FF0 - FORMATTER1 - DSIM_IF1 */
	DPATH_NOCOMP_OUTFIFO0_DSIMIF1			= 0x002,
	/* No comp - SPLITTER - FF0/1 - FORMATTER0/1 - DSIM_IF0/1 */
	DPATH_NOCOMP_SPLITTER_OUTFIFO01_DSIMIF01	= 0x003,
	/* No comp - OUTFIFO0/1/2 - WBIF */
	DPATH_NOCOMP_OUTFIFO0_WBIF			= 0x004,

	/* DSC_ENC0 - OUTFIFO0 - DSIM_IF0 */
	DPATH_DSCENC0_OUTFIFO0_DSIMIF0		= 0x011,
	/* DSC_ENC0 - OUTFIFO0 - DSIM_IF1 */
	DPATH_DSCENC0_OUTFIFO0_DSIMIF1		= 0x012,
	/* DSC_ENC0 - OUTFIFO0 - WBIF */
	DPATH_DSCENC0_OUTFIFO0_WBIF		= 0x014,

	/* DSCC,DSC_ENC0/1 - OUTFIFO01 DSIM_IF0 */
	DPATH_DSCC_DSCENC01_OUTFIFO01_DSIMIF0	= 0x0B1,
	/* DSCC,DSC_ENC0/1 - OUTFIFO01 DSIM_IF1 */
	DPATH_DSCC_DSCENC01_OUTFIFO01_DSIMIF1	= 0x0B2,
	/* DSCC,DSC_ENC0/1 - OUTFIFO01 DSIM_IF0/1 */
	DPATH_DSCC_DSCENC01_OUTFIFO01_DSIMIF01	= 0x0B3,
	/* DSCC,DSC_ENC0/1 - OUTFIFO01 - WBIF */
	DPATH_DSCC_DSCENC01_OUTFIFO01_WBIF	= 0x0B4,

	/* No comp - OUTFIFO0 DSIM_IF0 */
	DECON1_NOCOMP_OUTFIFO1_DSIMIF0		= 0x001,
	/* No comp - OUTFIFO0 DP_IF */
	DECON1_NOCOMP_OUTFIFO1_DPIF		= 0x008,
	/* DSC_ENC1 - OUTFIFO0 - DSIM_IF0 */
	DECON1_DSCENC1_OUTFIFO1_DSIMIF0		= 0x021,
	/* DSC_ENC1 - OUTFIFO0 - DP_IF */
	DECON1_DSCENC1_OUTFIFO1_DPIF		= 0x028,
	/* DSC_ENC1 - OUTFIFO1 - WBIF */
	DECON1_DSCENC1_OUTFIFO1_WBIF		= 0x024,

	/* No comp - OUTFIFO2 DP_IF */
	DECON2_NOCOMP_OUTFIFO2_DPIF		= 0x008,
	/* DSC_ENC2 - OUTFIFO2 - DP_IF0 */
	DECON2_DSCENC2_OUTFIFO2_DPIF		= 0x048,
	/* DSC_ENC2 - OUTFIFO2 - WBIF */
	DECON2_DSCENC2_OUTFIFO2_WBIF		= 0x044,
};

enum decon_enhance_path {
	ENHANCEPATH_ENHANCE_ALL_OFF	= 0x0,
	ENHANCEPATH_DITHER_ON		= 0x1,
	ENHANCEPATH_DQE_ON		= 0x2,
	ENHANCEPATH_DQE_DITHER_ON	= 0x3, /* post ENH stage is 8bpc */
};

enum decon_path_cfg {
	PATH_CON_ID_DSIM_IF0 = 0,
	PATH_CON_ID_DSIM_IF1 = 1,
	PATH_CON_ID_WB = 2,
	PATH_CON_ID_DP = 3,
	PATH_CON_ID_DUAL_DSC = 4,
	PATH_CON_ID_DSC0 = 4,
	PATH_CON_ID_DSC1 = 5,
	PATH_CON_ID_DSC2 = 6,
	PATH_CON_ID_DSCC_EN = 7,
};

enum decon_trig_mode {
	DECON_HW_TRIG = 0,
	DECON_SW_TRIG
};

enum decon_out_type {
	DECON_OUT_DSI = 0,
	DECON_OUT_EDP,
	DECON_OUT_DP,
	DECON_OUT_WB
};

enum decon_rgb_order {
	DECON_RGB = 0x0,
	DECON_GBR = 0x1,
	DECON_BRG = 0x2,
	DECON_BGR = 0x4,
	DECON_RBG = 0x5,
	DECON_GRB = 0x6,
};

enum decon_win_func {
	PD_FUNC_CLEAR			= 0x0,
	PD_FUNC_COPY			= 0x1,
	PD_FUNC_DESTINATION		= 0x2,
	PD_FUNC_SOURCE_OVER		= 0x3,
	PD_FUNC_DESTINATION_OVER	= 0x4,
	PD_FUNC_SOURCE_IN		= 0x5,
	PD_FUNC_DESTINATION_IN		= 0x6,
	PD_FUNC_SOURCE_OUT		= 0x7,
	PD_FUNC_DESTINATION_OUT		= 0x8,
	PD_FUNC_SOURCE_A_TOP		= 0x9,
	PD_FUNC_DESTINATION_A_TOP	= 0xa,
	PD_FUNC_XOR			= 0xb,
	PD_FUNC_PLUS			= 0xc,
	PD_FUNC_USER_DEFINED		= 0xd,
};

enum decon_blending {
	DECON_BLENDING_NONE = 0,
	DECON_BLENDING_PREMULT = 1,
	DECON_BLENDING_COVERAGE = 2,
	DECON_BLENDING_MAX = 3,
};

enum decon_set_trig {
	DECON_TRIG_DISABLE = 0,
	DECON_TRIG_ENABLE
};

enum dpu_bts_scen {
	DPU_BS_DEFAULT		= 0,
	DPU_BS_UHD,		/* for UHD 8-bit rotation case */
	DPU_BS_UHD_10B,		/* for UHD 10-bit rotation case */
	DPU_BS_DP_DEFAULT,
	/* add scenario index if necessary */
	DPU_BS_MAX
};

struct decon_mode_info {
	enum decon_psr_mode psr_mode;
	enum decon_trig_mode trig_mode;
	enum decon_out_type out_type;
	enum decon_dsi_mode dsi_mode;
};

struct decon_param {
	struct decon_mode_info psr;
	struct exynos_panel_info *lcd_info;
	u32 nr_windows;
	void __iomem *disp_ss_regs; /* TODO: remove ? */
};

struct decon_window_regs {
	u32 wincon;
	u32 start_pos;
	u32 end_pos;
	u32 colormap;
	u32 start_time;
	u32 pixel_count;
	u32 whole_w;
	u32 whole_h;
	u32 offset_x;
	u32 offset_y;
	u32 winmap_state;
	int ch;
	int plane_alpha;
	enum decon_pixel_format format;
	enum decon_blending blend;
};

struct decon_bts_bw {
	u32 val;
	u32 ch_num;
};

struct decon_dsc {
	unsigned int comp_cfg;
	unsigned int bit_per_pixel;
	unsigned int pic_height;
	unsigned int pic_width;
	unsigned int slice_height;
	unsigned int slice_width;
	unsigned int chunk_size;
	unsigned int initial_xmit_delay;
	unsigned int initial_dec_delay;
	unsigned int initial_scale_value;
	unsigned int scale_increment_interval;
	unsigned int scale_decrement_interval;
	unsigned int first_line_bpg_offset;
	unsigned int nfl_bpg_offset;
	unsigned int slice_bpg_offset;
	unsigned int initial_offset;
	unsigned int final_offset;
	unsigned int rc_range_parameters;
	unsigned int overlap_w;
	unsigned int width_per_enc;
	unsigned char *dec_pps_t;
};

u32 DPU_DMA2CH(u32 dma);
u32 DPU_CH2DMA(u32 ch);
int decon_check_supported_formats(enum decon_pixel_format format);

/*************** DECON CAL APIs exposed to DECON driver ***************/
/* DECON control */
int decon_reg_init(u32 id, u32 dsi_idx, struct decon_param *p);
int decon_reg_start(u32 id, struct decon_mode_info *psr);
int decon_reg_stop(u32 id, u32 dsi_idx, struct decon_mode_info *psr, bool rst,
		u32 fps);
int decon_reg_stop_inst(u32 id, u32 dsi_idx, struct decon_mode_info * psr,
		u32 fps);
int decon_reg_reset(u32 id);

/* DECON window control */
void decon_reg_set_win_enable(u32 id, u32 win_idx, u32 en);
void decon_reg_win_enable_and_update(u32 id, u32 win_idx, u32 en);
void decon_reg_all_win_shadow_update_req(u32 id);
void decon_reg_set_window_control(u32 id, int win_idx,
		struct decon_window_regs *regs, u32 winmap_en);
void decon_reg_update_req_window_mask(u32 id, u32 win_idx);
void decon_reg_update_req_window(u32 id, u32 win_idx);

/* DECON shadow update and trigger control */
void decon_reg_set_trigger(u32 id, struct decon_mode_info *psr,
		enum decon_set_trig en);
void decon_reg_update_req_and_unmask(u32 id, struct decon_mode_info *psr);
int decon_reg_wait_update_done_timeout(u32 id, unsigned long timeout);
int decon_reg_wait_update_done_and_mask(u32 id,
		struct decon_mode_info *psr, u32 timeout);
#if defined(CONFIG_EXYNOS_DECON_DQE)
void decon_reg_update_req_dqe(u32 id);
#endif

/* For window update and multi resolution feature */
int decon_reg_wait_idle_status_timeout(u32 id, unsigned long timeout);
void decon_reg_set_partial_update(u32 id, enum decon_dsi_mode dsi_mode,
		struct exynos_panel_info *lcd_info, bool in_slice[],
		u32 partial_w, u32 partial_h);
void decon_reg_set_mres(u32 id, struct decon_param *p);
#if defined(CONFIG_EXYNOS_EWR)
void decon_reg_update_ewr_control(u32 id, u32 fps);
#endif

/* For writeback configuration */
void decon_reg_release_resource(u32 id, struct decon_mode_info *psr);
void decon_reg_config_wb_size(u32 id, struct exynos_panel_info *lcd_info,
		struct decon_param *param);
void decon_reg_set_cwb_enable(u32 id, u32 en);

/* DECON interrupt control */
void decon_reg_set_int(u32 id, struct decon_mode_info *psr, u32 en);
int decon_reg_get_interrupt_and_clear(u32 id, u32 *ext_irq);

/* DECON SFR dump */
void __decon_dump(u32 id, void __iomem *main_regs, void __iomem *win_regs,
		void __iomem *sub_regs, void __iomem *wincon_regs,
		void __iomem *dqe_regs, u32 dsc_en);

void decon_reg_set_start_crc(u32 id, u32 dsim_id, u32 en);
//void decon_reg_set_select_crc_bits(u32 id, u32 bit_sel);
void decon_reg_get_crc_data(u32 id, u32 dsim_id, u32 crc_data[]);

/* DPU hw limitation check */
struct decon_device;
struct decon_win_config;
int decon_reg_check_global_limitation(struct decon_device *decon,
		struct decon_win_config *config);

void decon_reg_get_data_path(u32 id, enum decon_data_path *d_path,
		enum decon_enhance_path *e_path);
void decon_reg_set_data_path(u32 id, enum decon_data_path d_path,
		enum decon_enhance_path e_path);

/* TODO: this will be removed later */
void decon_reg_update_req_global(u32 id);

/* PLL sleep related functions */
void decon_reg_set_pll_sleep(u32 id, u32 en);
void decon_reg_set_pll_wakeup(u32 id, u32 en);
u32 decon_reg_get_run_status(u32 id);


/* Latency monitoring */
u32 decon_reg_get_latency_monitor_value(u32 id);
/*********************************************************************/

#endif /* __SAMSUNG_DECON_CAL_H__ */
