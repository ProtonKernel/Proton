/*
 * linux/drivers/video/fbdev/exynos/panel/s6e3had/s6e3had_unbound3_aod_panel.h
 *
 * Header file for AOD Driver
 *
 * Copyright (c) 2016 Samsung Electronics
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#ifndef __S6E3HAD_UNBOUND3_AOD_PANEL_H__
#define __S6E3HAD_UNBOUND3_AOD_PANEL_H__

#include "s6e3had_aod.h"
#include "s6e3had_aod_panel.h"

#include "s6e3had_unbound3_self_mask_img.h"
#include "s6e3had_self_icon_img.h"
#include "s6e3had_self_analog_clock_img.h"
#include "s6e3had_self_digital_clock_img.h"

/* UNBOUND3 */
static DEFINE_STATIC_PACKET_WITH_OPTION(s6e3had_unbound3_aod_self_mask_img_pkt,
	DSI_PKT_TYPE_WR_SR, UNBOUND3_SELF_MASK_IMG, 0, PKT_OPTION_SR_ALIGN_16);

static void *s6e3had_unbound3_aod_self_mask_img_cmdtbl[] = {
	&KEYINFO(s6e3had_aod_l1_key_enable),
	&PKTINFO(s6e3had_aod_self_mask_sd_path),
	&DLYINFO(s6e3had_aod_self_spsram_sel_delay),
	&PKTINFO(s6e3had_unbound3_aod_self_mask_img_pkt),
	&PKTINFO(s6e3had_aod_reset_sd_path),
	&KEYINFO(s6e3had_aod_l1_key_disable),
};

// --------------------- Image for self mask control ---------------------
#ifdef CONFIG_SELFMASK_FACTORY
static char S6E3HAD_UNBOUND3_AOD_SELF_MASK_ENA[] = {
	0x7A,
	0x01, 0x00, 0x00, 0x00, 0x0C, 0x80, 0x0C, 0x81,
	0x0C, 0x82, 0x0C, 0x83, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x09, 0x20, 0x00, 0x00,
	0x00, 0x00
};
static char S6E3HAD_UNBOUND3_AOD_SELF_MASK_ENA_LT_E2[] = {
	0x7A,
	0x01, 0x00, 0x00, 0x00, 0x0C, 0x80, 0x0C, 0x81,
	0x0C, 0x82, 0x0C, 0x83, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x09, 0x1F, 0x00, 0x00,
	0x00, 0x00
};
#else
static char S6E3HAD_UNBOUND3_AOD_SELF_MASK_ENA[] = {
	0x7A,
	0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0x01, 0x2b,
	0x0b, 0x54, 0x0C, 0x7f, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x09, 0x20, 0x00, 0x00,
	0x00, 0x00
};
static char S6E3HAD_UNBOUND3_AOD_SELF_MASK_ENA_LT_E2[] = {
	0x7A,
	0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0x01, 0x2b,
	0x0b, 0x54, 0x0C, 0x7f, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x09, 0x1F, 0x00, 0x00,
	0x00, 0x00
};
#endif

static DEFINE_STATIC_PACKET(s6e3had_unbound3_aod_self_mask_ctrl_ena,
	DSI_PKT_TYPE_WR, S6E3HAD_UNBOUND3_AOD_SELF_MASK_ENA, 0);
static DEFINE_STATIC_PACKET(s6e3had_unbound3_aod_self_mask_ctrl_ena_lt_e2,
	DSI_PKT_TYPE_WR, S6E3HAD_UNBOUND3_AOD_SELF_MASK_ENA_LT_E2, 0);

static DEFINE_COND(unbound3_a3_s0_cond_aod_is_id_gte_e2, is_id_gte_e2);

static void *s6e3had_aod_self_mask_ena_cmdtbl[] = {
	&KEYINFO(s6e3had_aod_l1_key_enable),
	&CONDINFO_IF(unbound3_a3_s0_cond_aod_is_id_gte_e2),
		&PKTINFO(s6e3had_unbound3_aod_self_mask_ctrl_ena),
	&CONDINFO_EL(unbound3_a3_s0_cond_aod_is_id_gte_e2),
		&PKTINFO(s6e3had_unbound3_aod_self_mask_ctrl_ena_lt_e2),
	&CONDINFO_FI(unbound3_a3_s0_cond_aod_is_id_gte_e2),
	&KEYINFO(s6e3had_aod_l1_key_disable),
};

// --------------------- self mask checksum ----------------------------
static DEFINE_STATIC_PACKET(s6e3had_aod_self_mask_img_pkt, DSI_PKT_TYPE_WR_SR, S6E3HAD_CRC_SELF_MASK_IMG, 0);

static char S6E3HAD_AOD_SELF_MASK_CRC_ON1[] = {
	0xD8,
	0x15,
};
static DEFINE_STATIC_PACKET(s6e3had_aod_self_mask_crc_on1, DSI_PKT_TYPE_WR, S6E3HAD_AOD_SELF_MASK_CRC_ON1, 0x27);

static char S6E3HAD_AOD_SELF_MASK_DBIST_ON[] = {
	0xBF,
	0x01, 0x07, 0x00, 0x00, 0x00, 0x10, 0x00, 0x00, 0x00
};
static DEFINE_STATIC_PACKET(s6e3had_aod_self_mask_dbist_on, DSI_PKT_TYPE_WR, S6E3HAD_AOD_SELF_MASK_DBIST_ON, 0);

static char S6E3HAD_AOD_SELF_MASK_DBIST_OFF[] = {
	0xBF,
	0x00, 0x07, 0xFF, 0x00, 0x00, 0x10, 0x00, 0x00, 0x00
};
static DEFINE_STATIC_PACKET(s6e3had_aod_self_mask_dbist_off, DSI_PKT_TYPE_WR, S6E3HAD_AOD_SELF_MASK_DBIST_OFF, 0);

static char S6E3HAD_AOD_SELF_MASK_ENABLE_FOR_CHECKSUM[] = {
	0x7A,
	0x05, 0x00, 0x00, 0x00, 0x01,
	0xF4, 0x02, 0x33, 0x0C, 0x80,
	0x0C, 0x81, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00,
	0x09, 0x20, 0x3F, 0xFF, 0xFF,
	0xFF,
};

static DEFINE_STATIC_PACKET(s6e3had_aod_self_mask_for_checksum, DSI_PKT_TYPE_WR, S6E3HAD_AOD_SELF_MASK_ENABLE_FOR_CHECKSUM, 0);

static char S6E3HAD_AOD_SELF_MASK_ENABLE_FOR_CHECKSUM_LT_E2[] = {
	0x7A,
	0x05, 0x00, 0x00, 0x00, 0x01,
	0xF4, 0x02, 0x33, 0x0C, 0x80,
	0x0C, 0x81, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00,
	0x09, 0x1F, 0x3F, 0xFF, 0xFF,
	0xFF,
};

static DEFINE_STATIC_PACKET(s6e3had_aod_self_mask_for_checksum_lt_e2, DSI_PKT_TYPE_WR, S6E3HAD_AOD_SELF_MASK_ENABLE_FOR_CHECKSUM_LT_E2, 0);

static char S6E3HAD_AOD_SELF_MASK_RESTORE[] = {
	0x7A,
	0x05, 0x00, 0x00, 0x00, 0x0C,
	0x80, 0x0C, 0x81, 0x0C, 0x82,
	0x0C, 0x83, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00,
	0x09, 0x20, 0x3F, 0xFF, 0xFF,
	0xFF,
};
static DEFINE_STATIC_PACKET(s6e3had_aod_self_mask_restore, DSI_PKT_TYPE_WR, S6E3HAD_AOD_SELF_MASK_RESTORE, 0);

static char S6E3HAD_AOD_SELF_MASK_RESTORE_LT_E2[] = {
	0x7A,
	0x05, 0x00, 0x00, 0x00, 0x0C,
	0x80, 0x0C, 0x81, 0x0C, 0x82,
	0x0C, 0x83, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00,
	0x09, 0x1F, 0x3F, 0xFF, 0xFF,
	0xFF,
};
static DEFINE_STATIC_PACKET(s6e3had_aod_self_mask_restore_lt_e2, DSI_PKT_TYPE_WR, S6E3HAD_AOD_SELF_MASK_RESTORE_LT_E2, 0);

static void *s6e3had_aod_self_mask_checksum_cmdtbl[] = {
	&KEYINFO(s6e3had_aod_l1_key_enable),
	&KEYINFO(s6e3had_aod_l2_key_enable),
	&KEYINFO(s6e3had_aod_l3_key_enable),
	&PKTINFO(s6e3had_aod_self_mask_crc_on1),
	&PKTINFO(s6e3had_aod_self_mask_dbist_on),
	&PKTINFO(s6e3had_aod_self_mask_disable),
	&DLYINFO(s6e3had_aod_self_mask_checksum_1frame_delay),
	&PKTINFO(s6e3had_aod_self_mask_sd_path),
	&DLYINFO(s6e3had_aod_self_spsram_sel_delay),
	&PKTINFO(s6e3had_aod_self_mask_img_pkt),
	&PKTINFO(s6e3had_aod_sd_path_analog),
	&DLYINFO(s6e3had_aod_self_spsram_sel_delay),
	&CONDINFO_IF(unbound3_a3_s0_cond_aod_is_id_gte_e2),
		&PKTINFO(s6e3had_aod_self_mask_for_checksum),
	&CONDINFO_EL(unbound3_a3_s0_cond_aod_is_id_gte_e2),
		&PKTINFO(s6e3had_aod_self_mask_for_checksum_lt_e2),
	&CONDINFO_FI(unbound3_a3_s0_cond_aod_is_id_gte_e2),
	&DLYINFO(s6e3had_aod_self_mask_checksum_2frame_delay),
	&s6e3had_restbl[RES_SELF_MASK_CHECKSUM],
	&CONDINFO_IF(unbound3_a3_s0_cond_aod_is_id_gte_e2),
		&PKTINFO(s6e3had_aod_self_mask_restore),
	&CONDINFO_EL(unbound3_a3_s0_cond_aod_is_id_gte_e2),
		&PKTINFO(s6e3had_aod_self_mask_restore_lt_e2),
	&CONDINFO_FI(unbound3_a3_s0_cond_aod_is_id_gte_e2),
	&PKTINFO(s6e3had_aod_self_mask_dbist_off),
	&KEYINFO(s6e3had_aod_l3_key_disable),
	&KEYINFO(s6e3had_aod_l2_key_disable),
	&KEYINFO(s6e3had_aod_l1_key_disable),
};

// --------------------- end of check sum control ----------------------------

static struct seqinfo s6e3had_unbound3_aod_seqtbl[MAX_AOD_SEQ] = {
	[SELF_MASK_IMG_SEQ] = SEQINFO_INIT("self_mask_img", s6e3had_unbound3_aod_self_mask_img_cmdtbl),
	[SELF_MASK_ENA_SEQ] = SEQINFO_INIT("self_mask_ena", s6e3had_aod_self_mask_ena_cmdtbl),
	[SELF_MASK_DIS_SEQ] = SEQINFO_INIT("self_mask_dis", s6e3had_aod_self_mask_dis_cmdtbl),
	[ANALOG_IMG_SEQ] = SEQINFO_INIT("analog_img", s6e3had_aod_analog_img_cmdtbl),
	[ANALOG_CTRL_SEQ] = SEQINFO_INIT("analog_ctrl", s6e3had_aod_analog_init_cmdtbl),
	[DIGITAL_IMG_SEQ] = SEQINFO_INIT("digital_img", s6e3had_aod_digital_img_cmdtbl),
	[DIGITAL_CTRL_SEQ] = SEQINFO_INIT("digital_ctrl", s6e3had_aod_digital_init_cmdtbl),
	[ENTER_AOD_SEQ] = SEQINFO_INIT("enter_aod", s6e3had_aod_enter_aod_cmdtbl),
	[EXIT_AOD_SEQ] = SEQINFO_INIT("exit_aod", s6e3had_aod_exit_aod_cmdtbl),
	[SELF_MOVE_ON_SEQ] = SEQINFO_INIT("self_move_on", s6e3had_aod_self_move_on_cmdtbl),
	[SELF_MOVE_OFF_SEQ] = SEQINFO_INIT("self_move_off", s6e3had_aod_self_move_off_cmdtbl),
	[SELF_MOVE_RESET_SEQ] = SEQINFO_INIT("self_move_reset", s6e3had_aod_self_move_reset_cmdtbl),
	[ICON_GRID_ON_SEQ] = SEQINFO_INIT("icon_grid_on", s6e3had_aod_icon_grid_on_cmdtbl),
	[ICON_GRID_OFF_SEQ] = SEQINFO_INIT("icon_grid_off", s6e3had_aod_icon_grid_off_cmdtbl),
	[SET_TIME_SEQ] = SEQINFO_INIT("SET_TIME", s6e3had_aod_set_time_cmdtbl),
#ifdef SUPPORT_NORMAL_SELF_MOVE
	[ENABLE_SELF_MOVE_SEQ] = SEQINFO_INIT("enable_self_move", s6e3had_enable_self_move),
	[DISABLE_SELF_MOVE_SEQ] = SEQINFO_INIT("disable_self_move", s6e3had_disable_self_move),
#endif
	[SELF_MASK_CHECKSUM_SEQ] = SEQINFO_INIT("self_mask_checksum", s6e3had_aod_self_mask_checksum_cmdtbl),
};

static struct aod_tune s6e3had_unbound3_aod = {
	.name = "s6e3had_unbound3_aod",
	.nr_seqtbl = ARRAY_SIZE(s6e3had_unbound3_aod_seqtbl),
	.seqtbl = s6e3had_unbound3_aod_seqtbl,
	.nr_maptbl = ARRAY_SIZE(s6e3had_aod_maptbl),
	.maptbl = s6e3had_aod_maptbl,
	.self_mask_en = true,
};

#endif //__S6E3HAD_UNBOUND3_AOD_PANEL_H__
