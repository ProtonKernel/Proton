/*
 * linux/drivers/video/fbdev/exynos/panel/s6e3had/s6e3had_aod_panel.h
 *
 * Header file for AOD Driver
 *
 * Copyright (c) 2016 Samsung Electronics
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#ifndef __S6E3HAD_AOD_PANEL_H__
#define __S6E3HAD_AOD_PANEL_H__

#include "s6e3had_aod.h"
//#include "s6e3had_self_icon_img.h"
#include "s6e3had_self_analog_clock_img.h"
#include "s6e3had_self_digital_clock_img.h"
#include "s6e3had_self_mask_img.h"

static u8 S6E3HAD_AOD_KEY1_ENABLE[] = { 0xF0, 0x5A, 0x5A };
static u8 S6E3HAD_AOD_KEY1_DISABLE[] = { 0xF0, 0xA5, 0xA5 };

static u8 S6E3HAD_AOD_KEY2_ENABLE[] = { 0xF1, 0x5A, 0x5A };
static u8 S6E3HAD_AOD_KEY2_DISABLE[] = { 0xF1, 0xA5, 0xA5 };

static u8 S6E3HAD_AOD_KEY3_ENABLE[] = { 0xFC, 0x5A, 0x5A };
static u8 S6E3HAD_AOD_KEY3_DISABLE[] = { 0xFC, 0xA5, 0xA5 };

static DEFINE_STATIC_PACKET(s6e3had_aod_l1_key_enable, DSI_PKT_TYPE_WR, S6E3HAD_AOD_KEY1_ENABLE, 0);
static DEFINE_STATIC_PACKET(s6e3had_aod_l1_key_disable, DSI_PKT_TYPE_WR, S6E3HAD_AOD_KEY1_DISABLE, 0);

static DEFINE_STATIC_PACKET(s6e3had_aod_l2_key_enable, DSI_PKT_TYPE_WR, S6E3HAD_AOD_KEY2_ENABLE, 0);
static DEFINE_STATIC_PACKET(s6e3had_aod_l2_key_disable, DSI_PKT_TYPE_WR, S6E3HAD_AOD_KEY2_DISABLE, 0);

static DEFINE_STATIC_PACKET(s6e3had_aod_l3_key_enable, DSI_PKT_TYPE_WR, S6E3HAD_AOD_KEY3_ENABLE, 0);
static DEFINE_STATIC_PACKET(s6e3had_aod_l3_key_disable, DSI_PKT_TYPE_WR, S6E3HAD_AOD_KEY3_DISABLE, 0);

static DEFINE_PANEL_UDELAY(s6e3had_aod_self_spsram_sel_delay, 1);
static DEFINE_PANEL_UDELAY(s6e3had_aod_self_mask_checksum_1frame_delay, 16700);
static DEFINE_PANEL_UDELAY(s6e3had_aod_self_mask_checksum_2frame_delay, 33400);
static DEFINE_PANEL_UDELAY(s6e3had_aod_time_update_delay, 33400);


static DEFINE_PANEL_KEY(s6e3had_aod_l1_key_enable, CMD_LEVEL_1,
	KEY_ENABLE, &PKTINFO(s6e3had_aod_l1_key_enable));
static DEFINE_PANEL_KEY(s6e3had_aod_l1_key_disable, CMD_LEVEL_1,
	KEY_DISABLE, &PKTINFO(s6e3had_aod_l1_key_disable));

static DEFINE_PANEL_KEY(s6e3had_aod_l2_key_enable, CMD_LEVEL_2,
	KEY_ENABLE, &PKTINFO(s6e3had_aod_l2_key_enable));
static DEFINE_PANEL_KEY(s6e3had_aod_l2_key_disable, CMD_LEVEL_2,
	KEY_DISABLE, &PKTINFO(s6e3had_aod_l2_key_disable));

static DEFINE_PANEL_KEY(s6e3had_aod_l3_key_enable, CMD_LEVEL_3,
	KEY_ENABLE, &PKTINFO(s6e3had_aod_l3_key_enable));
static DEFINE_PANEL_KEY(s6e3had_aod_l3_key_disable, CMD_LEVEL_3,
	KEY_DISABLE, &PKTINFO(s6e3had_aod_l3_key_disable));

static struct keyinfo KEYINFO(s6e3had_aod_l1_key_enable);
static struct keyinfo KEYINFO(s6e3had_aod_l1_key_disable);
static struct keyinfo KEYINFO(s6e3had_aod_l2_key_enable);
static struct keyinfo KEYINFO(s6e3had_aod_l2_key_disable);
static struct keyinfo KEYINFO(s6e3had_aod_l3_key_enable);
static struct keyinfo KEYINFO(s6e3had_aod_l3_key_disable);

static char s6e3had_aod_self_move_pos_tbl[][90] = {
	{
		0x22,
		0x10, 0x10, 0x10, 0x10, 0x01,
		0x50, 0x50, 0x50, 0x50, 0x05,
		0x10, 0x10, 0x10, 0x10, 0x01,
		0x50, 0x50, 0x50, 0x50, 0x05,
		0x10, 0x10, 0x10, 0x10, 0x01,
		0x50, 0x50, 0x50, 0x50, 0x05,
		0x10, 0x10, 0x10, 0x10, 0x01,
		0x50, 0x50, 0x50, 0x50, 0x05,
		0x10, 0x10, 0x10, 0x10, 0x01,
		0x50, 0x50, 0x50, 0x50, 0x05,
		0x10, 0x10, 0x10, 0x10, 0x01,
		0x50, 0x50, 0x50, 0x50, 0x44,
		0x44, 0x1C, 0xCC, 0xC3, 0x44,
		0x44, 0x1C, 0xCC, 0xC3, 0x44,
		0x44, 0x1C, 0xCC, 0xC3, 0x44,
		0x44, 0x1C, 0xCC, 0xC3,	0x44,
		0x44, 0x1C, 0xCC, 0xC3, 0x44,
		0x44, 0x1C, 0xCC, 0xC3
	},
	{
		0x55,
		0x10, 0x10, 0x10, 0x10, 0x01,
		0x50, 0x50, 0x50, 0x50, 0x05,
		0x10, 0x10, 0x10, 0x10, 0x01,
		0x50, 0x50, 0x50, 0x50, 0x05,
		0x10, 0x10, 0x10, 0x10, 0x01,
		0x50, 0x50, 0x50, 0x50, 0x05,
		0x10, 0x10, 0x10, 0x10, 0x01,
		0x50, 0x50, 0x50, 0x50, 0x05,
		0x10, 0x10, 0x10, 0x10, 0x01,
		0x50, 0x50, 0x50, 0x50, 0x05,
		0x10, 0x10, 0x10, 0x10, 0x01,
		0x50, 0x50, 0x50, 0x50, 0x44,
		0x44, 0x1C, 0xCC, 0xC3, 0x44,
		0x44, 0x1C, 0xCC, 0xC3, 0x44,
		0x44, 0x1C, 0xCC, 0xC3, 0x44,
		0x44, 0x1C, 0xCC, 0xC3, 0x44,
		0x44, 0x1C, 0xCC, 0xC3, 0x44,
		0x44, 0x1C, 0xCC, 0xC3
	},
	{
		0xdd,
		0x10, 0x10, 0x10, 0x10, 0x01,
		0x50, 0x50, 0x50, 0x50, 0x05,
		0x10, 0x10, 0x10, 0x10, 0x01,
		0x50, 0x50, 0x50, 0x50, 0x05,
		0x10, 0x10, 0x10, 0x10, 0x01,
		0x50, 0x50, 0x50, 0x50, 0x05,
		0x10, 0x10, 0x10, 0x10, 0x01,
		0x50, 0x50, 0x50, 0x50, 0x05,
		0x10, 0x10, 0x10, 0x10, 0x01,
		0x50, 0x50, 0x50, 0x50, 0x05,
		0x10, 0x10, 0x10, 0x10, 0x01,
		0x50, 0x50, 0x50, 0x50, 0x44,
		0x44, 0x1C, 0xCC, 0xC3, 0x44,
		0x44, 0x1C, 0xCC, 0xC3, 0x44,
		0x44, 0x1C, 0xCC, 0xC3, 0x44,
		0x44, 0x1C, 0xCC, 0xC3, 0x44,
		0x44, 0x1C, 0xCC, 0xC3, 0x44,
		0x44, 0x1C, 0xCC, 0xC3,
	},
	{
		0xdd,
		0x00, 0x10, 0x00, 0x10, 0x00,
		0x10, 0x00, 0x10, 0x00, 0x01,
		0x00, 0x50, 0x00, 0x50, 0x00,
		0x50, 0x00, 0x50, 0x00, 0x05,
		0x00, 0x10, 0x00, 0x10, 0x00,
		0x10, 0x00, 0x10, 0x00, 0x01,
		0x00, 0x50, 0x00, 0x50, 0x00,
		0x50, 0x00, 0x50, 0x00, 0x05,
		0x00, 0x10, 0x00, 0x10, 0x00,
		0x10, 0x00, 0x10, 0x00, 0x01,
		0x00, 0x50, 0x00, 0x50, 0x00,
		0x50, 0x00, 0x50, 0x00, 0x04,
		0x04, 0x04, 0x04, 0x01, 0x0C,
		0x0C, 0x0C, 0x0C, 0x03, 0x04,
		0x04, 0x04, 0x04, 0x01, 0x0C,
		0x0C, 0x0C, 0x0C, 0x03, 0x04,
		0x04, 0x04, 0x04, 0x01, 0x0C,
		0x0C, 0x0C, 0x0C, 0x03,
	},
	{
		0x00,
		0x10, 0x10, 0x10, 0x10, 0x01,
		0x50, 0x50, 0x50, 0x50, 0x05,
		0x10, 0x10, 0x10, 0x10, 0x01,
		0x50, 0x50, 0x50, 0x50, 0x05,
		0x10, 0x10, 0x10, 0x10, 0x01,
		0x50, 0x50, 0x50, 0x50, 0x05,
		0x10, 0x10, 0x10, 0x10, 0x01,
		0x50, 0x50, 0x50, 0x50, 0x05,
		0x10, 0x10, 0x10, 0x10, 0x01,
		0x50, 0x50, 0x50, 0x50, 0x05,
		0x10, 0x10, 0x10, 0x10, 0x01,
		0x50, 0x50, 0x50, 0x50, 0x44,
		0x44, 0x1C, 0xCC, 0xC3, 0x44,
		0x44, 0x1C, 0xCC, 0xC3, 0x44,
		0x44, 0x1C, 0xCC, 0xC3, 0x44,
		0x44, 0x1C, 0xCC, 0xC3, 0x44,
		0x44, 0x1C, 0xCC, 0xC3, 0x44,
		0x44, 0x1C, 0xCC, 0xC3,
	},
};

#ifdef SUPPORT_NORMAL_SELF_MOVE
static char s6e3had_aod_self_move_patt_tbl[][35] = {
	{
		0x7D,
		0x10, 0x01, 0x02, 0x00, 0x00, 0x10, 0x01, 0x50,
		0x05, 0x10, 0x01, 0x50, 0x05, 0x10, 0x01, 0x50,
		0x05, 0x10, 0x01, 0x50, 0x05, 0x10, 0x01, 0x00,
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
		0x00, 0x00,
	},
	{
		0x7D,
		0x10, 0x01, 0x02, 0x00, 0x00, 0x10, 0x01, 0x50,
		0x05, 0x10, 0x01, 0x50, 0x05, 0x10, 0x01, 0x50,
		0x05, 0x10, 0x01, 0x50, 0x05, 0x10, 0x01, 0x00,
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
		0x00, 0x00,
	},
};
#endif

static struct maptbl s6e3had_aod_maptbl[] = {
	[SELF_MASK_CTRL_MAPTBL] = DEFINE_0D_MAPTBL(s6e3had_aod_self_mask, s6e3had_init_self_mask_ctrl, NULL, s6e3had_copy_self_mask_ctrl),
	[DIGITAL_POS_MAPTBL] = DEFINE_0D_MAPTBL(s6e3had_aod_digital_pos, init_common_table, NULL, s6e3had_copy_digital_pos),
	[SET_TIME_MAPTBL] = DEFINE_0D_MAPTBL(s6e3had_aod_time, init_common_table, NULL, s6e3had_copy_time),
	[SELF_MOVE_MAPTBL] = DEFINE_0D_MAPTBL(s6e3had_aod_self_move_on,	init_common_table, NULL, s6e3had_copy_self_move_on_ctrl),
	[ANALOG_POS_MAPTBL] = DEFINE_0D_MAPTBL(s6e3had_aod_analog_pos, init_common_table, NULL, s6e3had_copy_analog_pos),
	[ANALOG_CLK_CTRL_MAPTBL] = DEFINE_0D_MAPTBL(s6e3had_aod_analog_en, init_common_table, NULL, s6e3had_copy_analog_en),
	[DIGITAL_CLK_CTRL_MAPTBL] = DEFINE_0D_MAPTBL(s6e3had_aod_digital_en, init_common_table, NULL, s6e3had_copy_digital_en),
	[SELF_MOVE_POS_MAPTBL] = DEFINE_2D_MAPTBL(s6e3had_aod_self_move_pos_tbl, init_common_table, s6e3had_getidx_self_mode_pos, copy_common_maptbl),
	[SELF_MOVE_RESET_MAPTBL] = DEFINE_0D_MAPTBL(s6e3had_aod_self_move_reset, init_common_table, NULL, s6e3had_copy_self_move_reset),
	[SET_TIME_RATE] = DEFINE_0D_MAPTBL(s6e3had_aod_timer_rate, init_common_table, NULL, s6e3had_copy_timer_rate),
	[SET_DIGITAL_COLOR] = DEFINE_0D_MAPTBL(s6e3had_aod_digital_color, init_common_table, NULL, s6e3had_copy_digital_color),
	[SET_DIGITAL_UN_WIDTH] = DEFINE_0D_MAPTBL(s6e3had_aod_digital_un_width, init_common_table, NULL, s6e3had_copy_digital_un_width),
#if 0
	[SET_PARTIAL_MODE] = DEFINE_0D_MAPTBL(s6e3had_aod_partial_mode, init_common_table, NULL, s6e3had_copy_partial_mode),
#endif
#ifdef SUPPORT_NORMAL_SELF_MOVE
	[SELF_MOVE_PATTERN_MAPTBL] = DEFINE_2D_MAPTBL(s6e3had_aod_self_move_patt_tbl,
		init_common_table, s6e3had_getidx_self_pattern, s6e3had_copy_self_move_pattern),
#endif

};

// --------------------- Image for self mask image ---------------------

static char S6E3HAD_AOD_RESET_SD_PATH[] = {
	0x75,
	0x01,
};
static DEFINE_STATIC_PACKET(s6e3had_aod_reset_sd_path, DSI_PKT_TYPE_WR, S6E3HAD_AOD_RESET_SD_PATH, 0);


static char S6E3HAD_AOD_SELF_MASK_SD_PATH[] = {
	0x75,
	0x10,
};
static DEFINE_STATIC_PACKET(s6e3had_aod_self_mask_sd_path, DSI_PKT_TYPE_WR, S6E3HAD_AOD_SELF_MASK_SD_PATH, 0);

// --------------------- End of self mask image ---------------------

static char S6E3HAD_AOD_SELF_MASK_DISABLE[] = {
	0x7A,
	0x00,
};
static DEFINE_STATIC_PACKET(s6e3had_aod_self_mask_disable, DSI_PKT_TYPE_WR, S6E3HAD_AOD_SELF_MASK_DISABLE, 0);

static void *s6e3had_aod_self_mask_dis_cmdtbl[] = {
	&KEYINFO(s6e3had_aod_l1_key_enable),
	&PKTINFO(s6e3had_aod_self_mask_disable),
	&KEYINFO(s6e3had_aod_l1_key_disable),
};
// --------------------- End of self mask control ---------------------



static char S6E3HAD_AOD_TIMER_OFF_CTRL[] = {
	0x81,
	0x00, 0x00,
};
static DEFINE_STATIC_PACKET(s6e3had_aod_timer_off_ctrl, DSI_PKT_TYPE_WR, S6E3HAD_AOD_TIMER_OFF_CTRL, 0);

static char S6E3HAD_AOD_ANALOG_OFF[] = {
	0x77,
	0x00, 0x00,
};
static DEFINE_STATIC_PACKET(s6e3had_aod_analog_off, DSI_PKT_TYPE_WR, S6E3HAD_AOD_ANALOG_OFF, 0);

static char S6E3HAD_AOD_DIGITAL_OFF[] = {
	0x80,
	0x00, 0x00,
};
static DEFINE_STATIC_PACKET(s6e3had_aod_digital_off, DSI_PKT_TYPE_WR, S6E3HAD_AOD_DIGITAL_OFF, 0);


static char S6E3HAD_AOD_ICON_GRID_ON[] = {
	0x7C,
	0x00, 0x01, 0x00, 0x1f, 0x00, 0x00, 0x00, 0x00,
	0x05, 0x9F, 0x0C, 0x7F
};

static DEFINE_STATIC_PACKET(s6e3had_aod_icon_grid_on, DSI_PKT_TYPE_WR, S6E3HAD_AOD_ICON_GRID_ON, 0);

static void *s6e3had_aod_icon_grid_on_cmdtbl[] = {
	&KEYINFO(s6e3had_aod_l1_key_enable),
	&PKTINFO(s6e3had_aod_icon_grid_on),
	&KEYINFO(s6e3had_aod_l1_key_disable),
};

static char S6E3HAD_AOD_ICON_GRID_OFF[] = {
	0X7C,
	0x00, 0x00
};
static DEFINE_STATIC_PACKET(s6e3had_aod_icon_grid_off, DSI_PKT_TYPE_WR, S6E3HAD_AOD_ICON_GRID_OFF, 0);

static void *s6e3had_aod_icon_grid_off_cmdtbl[] = {
	&KEYINFO(s6e3had_aod_l1_key_enable),
	&PKTINFO(s6e3had_aod_icon_grid_off),
	&KEYINFO(s6e3had_aod_l1_key_disable),
};


// --------------------- End of self icon control ---------------------

static char S6E3HAD_AOD_TIMER_EN[] = {
	0x81,
	0x00,
	0x03, /* s6e3had_timer_en */
};
static DEFINE_STATIC_PACKET(s6e3had_aod_timer_en, DSI_PKT_TYPE_WR, S6E3HAD_AOD_TIMER_EN, 0);

static char S6E3HAD_AOD_TIME[] = {
	0x81,
	0x0A, 0x0A, 0x1E, 0x00, /* s6e3had_timer_time */
};
static DEFINE_PKTUI(s6e3had_aod_time, &s6e3had_aod_maptbl[SET_TIME_MAPTBL], 0);
static DEFINE_VARIABLE_PACKET(s6e3had_aod_time, DSI_PKT_TYPE_WR, S6E3HAD_AOD_TIME, 2);


static char S6E3HAD_AOD_TIMER_RATE[] = {
	0x81,
	0xDE, 0x13, /* s6e3had_timer_rate - for analog clock */
};
static DEFINE_PKTUI(s6e3had_aod_timer_rate, &s6e3had_aod_maptbl[SET_TIME_RATE], 0);
static DEFINE_VARIABLE_PACKET(s6e3had_aod_timer_rate, DSI_PKT_TYPE_WR, S6E3HAD_AOD_TIMER_RATE, 6);

static char S6E3HAD_AOD_TIMER_LOOP[] = {
	0x81,
	0x00, 0x00,
};
static DEFINE_STATIC_PACKET(s6e3had_aod_timer_loop, DSI_PKT_TYPE_WR, S6E3HAD_AOD_TIMER_LOOP, 8);


static char S6E3HAD_AOD_UPDATE_TIME_CTRL[] = {
	0x78,
	0x41,
};
static DEFINE_STATIC_PACKET(s6e3had_aod_update_time_ctrl, DSI_PKT_TYPE_WR, S6E3HAD_AOD_UPDATE_TIME_CTRL, 0);

static void *s6e3had_aod_set_time_cmdtbl[] = {
	&KEYINFO(s6e3had_aod_l1_key_enable),
	&PKTINFO(s6e3had_aod_timer_loop),
	&PKTINFO(s6e3had_aod_time),
	&PKTINFO(s6e3had_aod_timer_rate),
	&PKTINFO(s6e3had_aod_timer_en),
	&DLYINFO(s6e3had_aod_time_update_delay),
	&PKTINFO(s6e3had_aod_update_time_ctrl),
	&KEYINFO(s6e3had_aod_l1_key_disable),
};

// --------------------- End of timer setting ---------------------

static char S6E3HAD_AOD_CLOCK_SIDEMEM_SET[] = {
	0x75,
	0x00,
};
static DEFINE_STATIC_PACKET(s6e3had_aod_clock_sidemem_set, DSI_PKT_TYPE_WR, S6E3HAD_AOD_CLOCK_SIDEMEM_SET, 0);

// --------------------- Image for analog clock ---------------------
static char S6E3HAD_AOD_SD_PATH_ANALOG[] = {
	0x75,
	0x01,
};
static DEFINE_STATIC_PACKET(s6e3had_aod_sd_path_analog, DSI_PKT_TYPE_WR, S6E3HAD_AOD_SD_PATH_ANALOG, 0);

static DEFINE_STATIC_PACKET(s6e3had_aod_analog_img, DSI_PKT_TYPE_WR_SR, S6E3HAD_ANALOG_IMG, 0);

static void *s6e3had_aod_analog_img_cmdtbl[] = {
	&KEYINFO(s6e3had_aod_l1_key_enable),
	&PKTINFO(s6e3had_aod_analog_off),
	&PKTINFO(s6e3had_aod_timer_off_ctrl),
	&DLYINFO(s6e3had_aod_time_update_delay),

	&PKTINFO(s6e3had_aod_sd_path_analog),
	&DLYINFO(s6e3had_aod_self_spsram_sel_delay),
	&PKTINFO(s6e3had_aod_analog_img),
	&DLYINFO(s6e3had_aod_self_spsram_sel_delay),
	&PKTINFO(s6e3had_aod_reset_sd_path),
	&KEYINFO(s6e3had_aod_l1_key_disable),
};

static char S6E3HAD_AOD_ANALOG_EN[] = {
	0x77,
	0x00, 0x00
};
static DEFINE_PKTUI(s6e3had_aod_analog_en, &s6e3had_aod_maptbl[ANALOG_CLK_CTRL_MAPTBL], 0);
static DEFINE_VARIABLE_PACKET(s6e3had_aod_analog_en, DSI_PKT_TYPE_WR, S6E3HAD_AOD_ANALOG_EN, 0);

static char S6E3HAD_AOD_ANALOG_POS[] = {
	0x77,
	0x02, 0x00, 0x02, 0x00, 0x00, /*s6e3had_aod_analog_pos*/

};
static DEFINE_PKTUI(s6e3had_aod_analog_pos, &s6e3had_aod_maptbl[ANALOG_POS_MAPTBL], 0);
static DEFINE_VARIABLE_PACKET(s6e3had_aod_analog_pos, DSI_PKT_TYPE_WR, S6E3HAD_AOD_ANALOG_POS, 2);

static char S6E3HAD_AOD_ANALOG_MASK[] = {
	0x77,
	0x00,
	0x50, 0x13, 0x50, 0x13, 0x50, 0x13,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, /*s6e3had_aod_analog_mask*/
};
static DEFINE_STATIC_PACKET(s6e3had_aod_analog_mask, DSI_PKT_TYPE_WR, S6E3HAD_AOD_ANALOG_MASK, 7);

static char S6E3HAD_AOD_ANALOG_MEM[] = {
	0x77,
	0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, /*s6e3had_aod_analog_mem*/
	0x00,
	0x07
};
static DEFINE_STATIC_PACKET(s6e3had_aod_analog_mem, DSI_PKT_TYPE_WR, S6E3HAD_AOD_ANALOG_MEM, 26);

static void *s6e3had_aod_analog_init_cmdtbl[] = {
	&KEYINFO(s6e3had_aod_l1_key_enable),
	&PKTINFO(s6e3had_aod_clock_sidemem_set),
	&PKTINFO(s6e3had_aod_analog_mem),
	&PKTINFO(s6e3had_aod_analog_mask),
	&PKTINFO(s6e3had_aod_analog_pos),
	&PKTINFO(s6e3had_aod_analog_en),
	&DLYINFO(s6e3had_aod_time_update_delay),
	&PKTINFO(s6e3had_aod_timer_rate),
	&KEYINFO(s6e3had_aod_l1_key_disable),
};

// --------------------- End of analog clock ---------------------

static char S6E3HAD_AOD_DIGITAL_EN[] = {
	0x80,
	0x00, 0x03, 0x02, 0x0F,
};

static DEFINE_PKTUI(s6e3had_aod_digital_en, &s6e3had_aod_maptbl[DIGITAL_CLK_CTRL_MAPTBL], 0);
static DEFINE_VARIABLE_PACKET(s6e3had_aod_digital_en, DSI_PKT_TYPE_WR, S6E3HAD_AOD_DIGITAL_EN, 0);


static char S6E3HAD_AOD_DIGITAL_POS[] = {
	0x80,
	0x01, 0x2C, 0x01, 0x90,
	0x01, 0xF4, 0x01, 0x90,
	0x01, 0x2C, 0x02, 0xF4,
	0x02, 0xF4, 0x01, 0xF4,
	0x00, 0xC8, 0x01, 0x64,
};
static DEFINE_PKTUI(s6e3had_aod_digital_pos, &s6e3had_aod_maptbl[DIGITAL_POS_MAPTBL], 0);
static DEFINE_VARIABLE_PACKET(s6e3had_aod_digital_pos, DSI_PKT_TYPE_WR, S6E3HAD_AOD_DIGITAL_POS, 4);

static char S6E3HAD_AOD_DIGITAL_COLOR[] = {
	0x80,
	0xFF, 0xFF, 0xFF, 0xFF
};
static DEFINE_PKTUI(s6e3had_aod_digital_color, &s6e3had_aod_maptbl[SET_DIGITAL_COLOR], 0);
static DEFINE_VARIABLE_PACKET(s6e3had_aod_digital_color, DSI_PKT_TYPE_WR, S6E3HAD_AOD_DIGITAL_COLOR, 24);


static char S6E3HAD_AOD_DIGITAL_UN_WIDTH[] = {
	0x80,
	0x00, 0x00,
	0x00, 0x00, 0x00, /*SC_D_ALIGN*/
	0x00, 0x00 /*SC_D_SPACE*/
};
static DEFINE_PKTUI(s6e3had_aod_digital_un_width, &s6e3had_aod_maptbl[SET_DIGITAL_UN_WIDTH], 0);
static DEFINE_VARIABLE_PACKET(s6e3had_aod_digital_un_width, DSI_PKT_TYPE_WR, S6E3HAD_AOD_DIGITAL_UN_WIDTH, 28);

static void *s6e3had_aod_digital_init_cmdtbl[] = {
	&KEYINFO(s6e3had_aod_l1_key_enable),
	&PKTINFO(s6e3had_aod_clock_sidemem_set),
	&PKTINFO(s6e3had_aod_digital_pos),
	&PKTINFO(s6e3had_aod_digital_color),
	&PKTINFO(s6e3had_aod_digital_un_width),
	&PKTINFO(s6e3had_aod_digital_en),
	&KEYINFO(s6e3had_aod_l1_key_disable),
};

static char S6E3HAD_AOD_SD_PATH_DIGITAL[] = {
	0x75,
	0x02,
};

static DEFINE_STATIC_PACKET(s6e3had_aod_sd_path_digital, DSI_PKT_TYPE_WR, S6E3HAD_AOD_SD_PATH_DIGITAL, 0);
static DEFINE_STATIC_PACKET(s6e3had_aod_digital_img, DSI_PKT_TYPE_WR_SR, S6E3HAD_DIGITAL_IMG, 0);

static void *s6e3had_aod_digital_img_cmdtbl[] = {
	&KEYINFO(s6e3had_aod_l1_key_enable),
	&PKTINFO(s6e3had_aod_analog_off),
	&PKTINFO(s6e3had_aod_timer_off_ctrl),
	&DLYINFO(s6e3had_aod_time_update_delay),

	&PKTINFO(s6e3had_aod_sd_path_digital),
	&DLYINFO(s6e3had_aod_self_spsram_sel_delay),
	&PKTINFO(s6e3had_aod_digital_img),
	&PKTINFO(s6e3had_aod_reset_sd_path),
	&KEYINFO(s6e3had_aod_l1_key_disable),
};
// --------------------- End of digital clock ---------------------

/* setting for aod porch */
static char S6E3HAD_AOD_PORCH[] = {
	0x76,
	0x08, 0x1F, 0x00, 0x00, 0x00
};
static DEFINE_STATIC_PACKET(s6e3had_aod_porch, DSI_PKT_TYPE_WR, S6E3HAD_AOD_PORCH, 0);

static char S6E3HAD_AOD_ENABLE[] = {
	0x76,
	0x11,
};
static DEFINE_STATIC_PACKET(s6e3had_aod_enable, DSI_PKT_TYPE_WR, S6E3HAD_AOD_ENABLE, 5);

static char S6E3HAD_AOD_DISABLE[] = {
	0x76,
	0x00,
};
static DEFINE_STATIC_PACKET(s6e3had_aod_disable, DSI_PKT_TYPE_WR, S6E3HAD_AOD_DISABLE, 5);


static char S6E3HAD_PARTIAL_MODE[] = {
	0x85,
	0xA7, /* enable partial scan & enable partial hlpm*/
	0x00, /*enable partial hlpm area*/
	0x0F, 0x0F, 0x08, 0x34, 0x1F,
};
DEFINE_STATIC_PACKET(s6e3had_enable_partial_mode, DSI_PKT_TYPE_WR, S6E3HAD_PARTIAL_MODE, 0);


static char S6E3HAD_DIS_PARTIAL_SCAN[] = {
	0x85,
	0x00,
};
DEFINE_STATIC_PACKET(s6e3had_disable_partial_mode, DSI_PKT_TYPE_WR, S6E3HAD_DIS_PARTIAL_SCAN, 0);


static void *s6e3had_aod_enter_aod_cmdtbl[] = {
	&KEYINFO(s6e3had_aod_l3_key_enable),
	&PKTINFO(s6e3had_aod_porch),
	&PKTINFO(s6e3had_aod_enable),
	&PKTINFO(s6e3had_enable_partial_mode),
	&KEYINFO(s6e3had_aod_l3_key_disable),
};

static void *s6e3had_aod_exit_aod_cmdtbl[] = {
	&KEYINFO(s6e3had_aod_l1_key_enable),
	&PKTINFO(s6e3had_aod_disable),
	&PKTINFO(s6e3had_disable_partial_mode),
	&PKTINFO(s6e3had_aod_analog_off),
	&PKTINFO(s6e3had_aod_digital_off),
	&PKTINFO(s6e3had_aod_timer_off_ctrl),
	&PKTINFO(s6e3had_aod_reset_sd_path),
	&KEYINFO(s6e3had_aod_l1_key_disable),
};

/* -------------------------- Setting for self move --------------------------*/

static char S6E3HAD_AOD_SELF_MOVE_RESET[] = {
	0x7D,
	0x10,
};
DEFINE_STATIC_PACKET(s6e3had_aod_self_move_reset, DSI_PKT_TYPE_WR, S6E3HAD_AOD_SELF_MOVE_RESET, 0);


static char S6E3HAD_AOD_SELF_MOVE_ON[] = {
	0x7D,
	0x00, 0x00, 0x00, 0x00,
};

static DEFINE_PKTUI(s6e3had_aod_self_move_on, &s6e3had_aod_maptbl[SELF_MOVE_MAPTBL], 0);
static DEFINE_VARIABLE_PACKET(s6e3had_aod_self_move_on, DSI_PKT_TYPE_WR, S6E3HAD_AOD_SELF_MOVE_ON, 0);

char S6E3HAD_AOD_SELF_MOVE_POS[] = {
	0x7D,
	0x23, 0x27, 0x23, 0x77, 0x73, 0x27, 0x33, 0x17,
	0x73, 0x77, 0x23, 0x27, 0x23, 0x77, 0x73, 0x27,
	0x33, 0x17, 0x73, 0x77, 0x23, 0x27, 0x23, 0x77,
	0x73, 0x27, 0x33, 0x17, 0x73, 0x44, 0x44, 0x1C,
	0xCC, 0xC3, 0x44, 0x44, 0x1C, 0xCC, 0xC3, 0x44,
	0x44, 0x1C, 0xCC, 0xC3, 0x10, 0x10, 0x10, 0x10,
	0x01, 0x50, 0x50, 0x50, 0x50, 0x05, 0x10, 0x10,
	0x10, 0x10, 0x01, 0x50, 0x50, 0x50, 0x50, 0x05,
	0x10, 0x10, 0x10, 0x10, 0x01, 0x50, 0x50, 0x50,
	0x50,
};

DEFINE_STATIC_PACKET(s6e3had_aod_self_move_pos, DSI_PKT_TYPE_WR, S6E3HAD_AOD_SELF_MOVE_POS, 4);

static void *s6e3had_aod_self_move_on_cmdtbl[] = {
	&KEYINFO(s6e3had_aod_l1_key_enable),
	&PKTINFO(s6e3had_aod_self_move_pos),
	&PKTINFO(s6e3had_aod_self_move_on),
	&KEYINFO(s6e3had_aod_l1_key_disable),
};

static char S6E3HAD_AOD_SELF_MOVE_OFF[] = {
	0x7D,
	0x00, 0x00
};
DEFINE_STATIC_PACKET(s6e3had_aod_self_move_off, DSI_PKT_TYPE_WR, S6E3HAD_AOD_SELF_MOVE_OFF, 0);

#if defined(__PANEL_NOT_USED_VARIABLE__)
static DEFINE_PANEL_MDELAY(s6e3had_aod_move_off_delay, 34);
#endif

static void *s6e3had_aod_self_move_off_cmdtbl[] = {
	&KEYINFO(s6e3had_aod_l1_key_enable),
	&PKTINFO(s6e3had_aod_self_move_off),
	&KEYINFO(s6e3had_aod_l1_key_disable),
};

static void *s6e3had_aod_self_move_reset_cmdtbl[] = {
	&KEYINFO(s6e3had_aod_l1_key_enable),
	&PKTINFO(s6e3had_aod_self_move_reset),
	&KEYINFO(s6e3had_aod_l1_key_disable),
};

#ifdef SUPPORT_NORMAL_SELF_MOVE
char S6E3HAD_DEF_MOVE_PATTERN[] = {
	0x7D,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x55,
	0x10, 0x01, 0x50, 0x05, 0x10, 0x01, 0x50, 0x05,
	0x10, 0x01, 0x50, 0x05, 0x10, 0x01, 0x50, 0x05,
	0x10, 0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x41, 0xC3, 0x41, 0xC3, 0x41,
	0xC3, 0x41, 0xC3, 0x41,

};

DEFINE_PKTUI(s6e3had_self_move_pattern, &s6e3had_aod_maptbl[SELF_MOVE_PATTERN_MAPTBL], 0);
DEFINE_VARIABLE_PACKET(s6e3had_self_move_pattern, DSI_PKT_TYPE_WR, S6E3HAD_DEF_MOVE_PATTERN, 0);

static char S6E3HAD_SELF_TIMER_ON[] = {
	0x81,
	0x00, 0x03, 0x00, 0x00, 0x00, 0x00, 0x03, 0x00,
	0x00, 0x00
};

#if 0
DEFINE_PKTUI(s6e3had_self_timer_on, &s6e3had_aod_maptbl[SELF_MOVE_TIMER_ON_MAPTBL], 0);
DEFINE_VARIABLE_PACKET(s6e3had_self_timer_on, DSI_PKT_TYPE_WR, S6E3HAD_SELF_TIMER_ON, 0);
#else
DEFINE_STATIC_PACKET(s6e3had_self_timer_on, DSI_PKT_TYPE_WR, S6E3HAD_SELF_TIMER_ON, 0);
#endif


static char S6E3HAD_AOD_TIMER_OFF[] = {
	0x81,
	0x00, 0x00,
};
DEFINE_STATIC_PACKET(s6e3had_aod_timer_off, DSI_PKT_TYPE_WR, S6E3HAD_AOD_TIMER_OFF, 0);

static void *s6e3had_enable_self_move[] = {
	&KEYINFO(s6e3had_aod_l3_key_enable),
	&KEYINFO(s6e3had_aod_l1_key_enable),
	&PKTINFO(s6e3had_aod_porch),
	&PKTINFO(s6e3had_self_timer_on),
	&PKTINFO(s6e3had_self_move_pattern),
	&KEYINFO(s6e3had_aod_l1_key_disable),
	&KEYINFO(s6e3had_aod_l3_key_disable),
};

static void *s6e3had_disable_self_move[] = {
	&KEYINFO(s6e3had_aod_l1_key_enable),
	&PKTINFO(s6e3had_aod_timer_off),
	&PKTINFO(s6e3had_aod_self_move_off),
	//&PKTINFO(s6e3had_aod_self_move_sync_off),
	&KEYINFO(s6e3had_aod_l1_key_disable),
};
#endif

/* -------------------------- End self move --------------------------*/
#endif //__S6E3HAD_AOD_PANEL_H__
