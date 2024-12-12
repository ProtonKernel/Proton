/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) Samsung Electronics Co., Ltd.
 * Gwanghui Lee <gwanghui.lee@samsung.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */


#ifndef __MCD_DECON_H__
#define __MCD_DECON_H__

#include <linux/console.h>
#include <media/v4l2-device.h>
#include "decon.h"
#include "dpp.h"
#include "mcd_helper.h"
#ifdef CONFIG_DYNAMIC_MIPI
#include "dynamic_mipi/band_info.h"
#endif

#ifdef CONFIG_DYNAMIC_MIPI
void mcd_set_freq_hop(struct decon_device *decon, struct decon_reg_data *regs, bool en);
#else
static inline void mcd_set_freq_hop(struct decon_device *decon, struct decon_reg_data *regs, bool en) { return; }
#endif


#ifdef CONFIG_MCDHDR
int mcd_prepare_hdr_config(struct decon_device *decon, struct decon_win_config_data *win_data, struct decon_reg_data *regs);
void mcd_clear_hdr_info(struct decon_device *decon, struct decon_reg_data *regs);
void mcd_free_hdr_info(struct decon_device *decon, struct decon_reg_data *regs);
inline void mcd_fill_hdr_info(int win_idx, struct dpp_config *config, struct decon_reg_data *regs);
void mcd_unmap_all_hdr_info(struct decon_device *decon);
#else
static inline int mcd_prepare_hdr_config(struct decon_device *decon, struct decon_win_config_data *win_data, struct decon_reg_data *regs) { return 0; }
static inline void mcd_clear_hdr_info(struct decon_device *decon, struct decon_reg_data *regs) { return; }
static inline void mcd_free_hdr_info(struct decon_device *decon, struct decon_reg_data *regs) { return; }
static inline void mcd_fill_hdr_info(int win_idx, struct dpp_config *config, struct decon_reg_data *regs) { return; };
static inline void mcd_unmap_all_hdr_info(struct decon_device *decon) {};
#endif

bool is_hmd_not_running(struct decon_device *decon);
int mcd_decon_create_debug_sysfs(struct decon_device *decon);

void mcd_decon_panel_dump(struct decon_device *decon);
void mcd_decon_flush_image(struct decon_device *decon);

#if defined(CONFIG_DECON_BTS_VRR_ASYNC)
void decon_bts_print_fps_sync(struct decon_bts *bts);
void decon_bts_init_fps_sync(struct decon_bts *bts);
void decon_bts_set_applied_fps(struct decon_bts *bts, u32 fps);
int decon_bts_get_bts_fps(struct decon_bts *bts, u64 timeline);
int decon_bts_add_fps_sync(struct decon_bts *bts, u64 seqno, u32 fps);
int decon_bts_clear_fps_sync(struct decon_bts *bts);
int decon_bts_set_candidate_fps(struct decon_bts *bts, u32 fps);
int decon_bts_clear_candidate_fps(struct decon_bts *bts);
int decon_bts_accept_candidate_fps(struct decon_bts *bts, u64 seqno);
#endif
#endif //__MCD_DECON_H__
