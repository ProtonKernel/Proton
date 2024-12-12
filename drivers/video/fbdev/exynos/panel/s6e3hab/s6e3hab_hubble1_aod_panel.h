/*
 * linux/drivers/video/fbdev/exynos/panel/s6e3hab/s6e3hab_hubble1_aod_panel.h
 *
 * Header file for AOD Driver
 *
 * Copyright (c) 2016 Samsung Electronics
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#ifndef __S6E3HAB_HUBBLE1_AOD_PANEL_H__
#define __S6E3HAB_HUBBLE1_AOD_PANEL_H__

#include "s6e3hab_aod.h"
#include "s6e3hab_aod_panel.h"

#include "s6e3hab_hubble1_self_mask_img.h"
#include "s6e3hab_self_icon_img.h"
#include "s6e3hab_self_analog_clock_img.h"
#include "s6e3hab_self_digital_clock_img.h"

/* HUBBLE1 */
static DEFINE_STATIC_PACKET_WITH_OPTION(s6e3hab_hubble1_aod_self_mask_img_pkt,
	DSI_PKT_TYPE_WR_SR, HUBBLE1_SELF_MASK_IMG, 0, PKT_OPTION_SR_ALIGN_16);

static void *s6e3hab_hubble1_aod_self_mask_img_cmdtbl[] = {
	&KEYINFO(s6e3hab_aod_l2_key_enable),
	&PKTINFO(s6e3hab_aod_self_mask_sd_path),
	&DLYINFO(s6e3hab_aod_self_spsram_sel_delay),
	&PKTINFO(s6e3hab_hubble1_aod_self_mask_img_pkt),
	&PKTINFO(s6e3hab_aod_reset_sd_path),
	&KEYINFO(s6e3hab_aod_l2_key_disable),
};

static struct seqinfo s6e3hab_hubble1_aod_seqtbl[MAX_AOD_SEQ] = {
	[SELF_MASK_IMG_SEQ] = SEQINFO_INIT("self_mask_img", s6e3hab_hubble1_aod_self_mask_img_cmdtbl),
	[SELF_MASK_ENA_SEQ] = SEQINFO_INIT("self_mask_ena", s6e3hab_aod_self_mask_ena_cmdtbl),
	[SELF_MASK_DIS_SEQ] = SEQINFO_INIT("self_mask_dis", s6e3hab_aod_self_mask_dis_cmdtbl),
	[ANALOG_IMG_SEQ] = SEQINFO_INIT("analog_img", s6e3hab_aod_analog_img_cmdtbl),
	[ANALOG_CTRL_SEQ] = SEQINFO_INIT("analog_ctrl", s6e3hab_aod_analog_init_cmdtbl),
	[DIGITAL_IMG_SEQ] = SEQINFO_INIT("digital_img", s6e3hab_aod_digital_img_cmdtbl),
	[DIGITAL_CTRL_SEQ] = SEQINFO_INIT("digital_ctrl", s6e3hab_aod_digital_init_cmdtbl),
	[ENTER_AOD_SEQ] = SEQINFO_INIT("enter_aod", s6e3hab_aod_enter_aod_cmdtbl),
	[EXIT_AOD_SEQ] = SEQINFO_INIT("exit_aod", s6e3hab_aod_exit_aod_cmdtbl),
	[SELF_MOVE_ON_SEQ] = SEQINFO_INIT("self_move_on", s6e3hab_aod_self_move_on_cmdtbl),
	[SELF_MOVE_OFF_SEQ] = SEQINFO_INIT("self_move_off", s6e3hab_aod_self_move_off_cmdtbl),
	[SELF_MOVE_RESET_SEQ] = SEQINFO_INIT("self_move_reset", s6e3hab_aod_self_move_reset_cmdtbl),
	[ICON_GRID_ON_SEQ] = SEQINFO_INIT("icon_grid_on", s6e3hab_aod_icon_grid_on_cmdtbl),
	[ICON_GRID_OFF_SEQ] = SEQINFO_INIT("icon_grid_off", s6e3hab_aod_icon_grid_off_cmdtbl),
	[SET_TIME_SEQ] = SEQINFO_INIT("SET_TIME", s6e3hab_aod_set_time_cmdtbl),
	[ENABLE_PARTIAL_SCAN] = SEQINFO_INIT("ENA_PARTIAL_SCAN", s6e3hab_aod_partial_enable_cmdtbl),
	[DISABLE_PARTIAL_SCAN] = SEQINFO_INIT("DIS_PARTIAL_SCAN", s6e3hab_aod_partial_disable_cmdtbl),
#ifdef SUPPORT_NORMAL_SELF_MOVE
	[ENABLE_SELF_MOVE_SEQ] = SEQINFO_INIT("enable_self_move", s6e3hab_enable_self_move),
	[DISABLE_SELF_MOVE_SEQ] = SEQINFO_INIT("disable_self_move", s6e3hab_disable_self_move),
#endif
	[SELF_MASK_CHECKSUM_SEQ] = SEQINFO_INIT("self_mask_checksum", s6e3hab_aod_self_mask_checksum_cmdtbl),
};

static struct aod_tune s6e3hab_hubble1_aod = {
	.name = "s6e3hab_hubble1_aod",
	.nr_seqtbl = ARRAY_SIZE(s6e3hab_hubble1_aod_seqtbl),
	.seqtbl = s6e3hab_hubble1_aod_seqtbl,
	.nr_maptbl = ARRAY_SIZE(s6e3hab_aod_maptbl),
	.maptbl = s6e3hab_aod_maptbl,
	.self_mask_en = true,
};
#endif //__S6E3HAB_HUBBLE1_AOD_PANEL_H__
