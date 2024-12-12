/*
 * linux/drivers/video/fbdev/exynos/panel/s6e3had/s6e3had_unbound3_a3_s0_panel.h
 *
 * Header file for S6E3HAD Dimming Driver
 *
 * Copyright (c) 2016 Samsung Electronics
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#ifndef __S6E3HAD_UNBOUND3_A3_S0_PANEL_H__
#define __S6E3HAD_UNBOUND3_A3_S0_PANEL_H__

#include "../panel.h"
#include "../panel_drv.h"
#include "s6e3had.h"
#include "s6e3had_dimming.h"
#ifdef CONFIG_SUPPORT_POC_SPI
#include "../panel_spi.h"
#endif
#ifdef CONFIG_EXYNOS_DECON_MDNIE_LITE
#include "s6e3had_unbound3_a3_s0_panel_mdnie.h"
#endif
#ifdef CONFIG_EXYNOS_DECON_LCD_COPR
#include "s6e3had_unbound3_a3_s0_panel_copr.h"
#endif
#ifdef CONFIG_SUPPORT_DDI_FLASH
#include "s6e3had_unbound3_panel_poc.h"
#endif
#include "s6e3had_unbound3_a3_s0_panel_dimming.h"
#ifdef CONFIG_SUPPORT_HMD
#include "s6e3had_unbound3_a3_s0_panel_hmd_dimming.h"
#endif
#ifdef CONFIG_SUPPORT_AOD_BL
#include "s6e3had_unbound3_a3_s0_panel_aod_dimming.h"
#endif
#ifdef CONFIG_EXTEND_LIVE_CLOCK
#include "s6e3had_unbound3_aod_panel.h"
#include "../aod/aod_drv.h"
#endif

#ifdef CONFIG_SUPPORT_DISPLAY_PROFILER
#include "s6e3had_profiler_panel.h"
#include "../display_profiler/display_profiler.h"
#endif

#if defined(__PANEL_NOT_USED_VARIABLE__)
#include "s6e3had_unbound3_a3_s0_panel_irc.h"
#endif
#include "s6e3had_unbound3_resol.h"
#ifdef CONFIG_SUPPORT_POC_SPI
#include "../spi/w25q80_panel_spi.h"
#include "../spi/mx25r4035_panel_spi.h"
#endif

#ifdef CONFIG_DYNAMIC_MIPI
#include "unbound3_dm_band.h"
#endif

#ifdef CONFIG_SUPPORT_MAFPC
#include "s6e3had_unbound3_abc_data.h"
#endif


#undef __pn_name__
#define __pn_name__	unbound3_a3_s0

#undef __PN_NAME__
#define __PN_NAME__	UNBOUND3_A3_S0

/* ===================================================================================== */
/* ============================= [S6E3HAD READ INFO TABLE] ============================= */
/* ===================================================================================== */
/* <READINFO TABLE> IS DEPENDENT ON LDI. IF YOU NEED, DEFINE PANEL's RESOURCE TABLE */


/* ===================================================================================== */
/* ============================= [S6E3HAD RESOURCE TABLE] ============================== */
/* ===================================================================================== */
/* <RESOURCE TABLE> IS DEPENDENT ON LDI. IF YOU NEED, DEFINE PANEL's RESOURCE TABLE */


/* ===================================================================================== */
/* ============================== [S6E3HAD MAPPING TABLE] ============================== */
/* ===================================================================================== */

static u8 unbound3_a3_s0_brt_table[S6E3HAD_TOTAL_STEP][2] = {
	/* Normal 5x51+1 */
	{ 0x00, 0x08 }, { 0x00, 0x09 }, { 0x00, 0x0B }, { 0x00, 0x0D }, { 0x00, 0x10 },
	{ 0x00, 0x12 }, { 0x00, 0x15 }, { 0x00, 0x18 }, { 0x00, 0x1B }, { 0x00, 0x1E },
	{ 0x00, 0x21 }, { 0x00, 0x25 }, { 0x00, 0x28 }, { 0x00, 0x28 }, { 0x00, 0x2A },
	{ 0x00, 0x2C }, { 0x00, 0x2F }, { 0x00, 0x33 }, { 0x00, 0x33 }, { 0x00, 0x4A },
	{ 0x00, 0x4B }, { 0x00, 0x56 }, { 0x00, 0x56 }, { 0x00, 0x58 }, { 0x00, 0x58 },
	{ 0x00, 0x5C }, { 0x00, 0x60 }, { 0x00, 0x65 }, { 0x00, 0x69 }, { 0x00, 0x6E },
	{ 0x00, 0x73 }, { 0x00, 0x77 }, { 0x00, 0x7C }, { 0x00, 0x81 }, { 0x00, 0x85 },
	{ 0x00, 0x8A }, { 0x00, 0x8F }, { 0x00, 0x94 }, { 0x00, 0x99 }, { 0x00, 0x9E },
	{ 0x00, 0xA3 }, { 0x00, 0xA8 }, { 0x00, 0xAD }, { 0x00, 0xB2 }, { 0x00, 0xB7 },
	{ 0x00, 0xBD }, { 0x00, 0xC2 }, { 0x00, 0xC7 }, { 0x00, 0xCC }, { 0x00, 0xD2 },
	{ 0x00, 0xD7 }, { 0x00, 0xDC }, { 0x00, 0xE2 }, { 0x00, 0xE7 }, { 0x00, 0xED },
	{ 0x00, 0xF2 }, { 0x00, 0xF8 }, { 0x00, 0xFE }, { 0x01, 0x03 }, { 0x01, 0x09 },
	{ 0x01, 0x0E }, { 0x01, 0x14 }, { 0x01, 0x1A }, { 0x01, 0x20 }, { 0x01, 0x25 },
	{ 0x01, 0x2B }, { 0x01, 0x31 }, { 0x01, 0x37 }, { 0x01, 0x3D }, { 0x01, 0x43 },
	{ 0x01, 0x49 }, { 0x01, 0x52 }, { 0x01, 0x58 }, { 0x01, 0x5F }, { 0x01, 0x65 },
	{ 0x01, 0x6B }, { 0x01, 0x72 }, { 0x01, 0x78 }, { 0x01, 0x7F }, { 0x01, 0x85 },
	{ 0x01, 0x8C }, { 0x01, 0x92 }, { 0x01, 0x99 }, { 0x01, 0x9F }, { 0x01, 0xA6 },
	{ 0x01, 0xAD }, { 0x01, 0xB3 }, { 0x01, 0xBA }, { 0x01, 0xC0 }, { 0x01, 0xC7 },
	{ 0x01, 0xCE }, { 0x01, 0xD5 }, { 0x01, 0xDB }, { 0x01, 0xE2 }, { 0x01, 0xE9 },
	{ 0x01, 0xF0 }, { 0x01, 0xF7 }, { 0x01, 0xFE }, { 0x02, 0x05 }, { 0x02, 0x0C },
	{ 0x02, 0x12 }, { 0x02, 0x19 }, { 0x02, 0x20 }, { 0x02, 0x27 }, { 0x02, 0x2E },
	{ 0x02, 0x35 }, { 0x02, 0x3D }, { 0x02, 0x44 }, { 0x02, 0x4B }, { 0x02, 0x52 },
	{ 0x02, 0x59 }, { 0x02, 0x60 }, { 0x02, 0x67 }, { 0x02, 0x6F }, { 0x02, 0x76 },
	{ 0x02, 0x7D }, { 0x02, 0x84 }, { 0x02, 0x8C }, { 0x02, 0x93 }, { 0x02, 0x9A },
	{ 0x02, 0xA2 }, { 0x02, 0xA9 }, { 0x02, 0xB0 }, { 0x02, 0xB8 }, { 0x02, 0xBF },
	{ 0x02, 0xC7 }, { 0x02, 0xCE }, { 0x02, 0xD6 }, { 0x02, 0xDD }, { 0x02, 0xE4 },
	{ 0x02, 0xEC }, { 0x02, 0xF4 }, { 0x02, 0xFB }, { 0x03, 0x02 }, { 0x02, 0xFB },
	{ 0x03, 0x02 }, { 0x03, 0x0A }, { 0x03, 0x11 }, { 0x03, 0x18 }, { 0x03, 0x20 },
	{ 0x03, 0x27 }, { 0x03, 0x2E }, { 0x03, 0x36 }, { 0x03, 0x3D }, { 0x03, 0x45 },
	{ 0x03, 0x4C }, { 0x03, 0x54 }, { 0x03, 0x5B }, { 0x03, 0x63 }, { 0x03, 0x6A },
	{ 0x03, 0x72 }, { 0x03, 0x7A }, { 0x03, 0x81 }, { 0x03, 0x89 }, { 0x03, 0x90 },
	{ 0x03, 0x98 }, { 0x03, 0xA0 }, { 0x03, 0xA7 }, { 0x03, 0xAF }, { 0x03, 0xB7 },
	{ 0x03, 0xBF }, { 0x03, 0xC6 }, { 0x03, 0xCE }, { 0x03, 0xD6 }, { 0x03, 0xDE },
	{ 0x03, 0xE5 }, { 0x03, 0xED }, { 0x03, 0xF5 }, { 0x03, 0xFD }, { 0x04, 0x05 },
	{ 0x04, 0x0C }, { 0x04, 0x14 }, { 0x04, 0x1C }, { 0x04, 0x24 }, { 0x04, 0x2C },
	{ 0x04, 0x34 }, { 0x04, 0x3C }, { 0x04, 0x44 }, { 0x04, 0x4C }, { 0x04, 0x54 },
	{ 0x04, 0x5C }, { 0x04, 0x64 }, { 0x04, 0x6C }, { 0x04, 0x74 }, { 0x04, 0x7C },
	{ 0x04, 0x84 }, { 0x04, 0x8C }, { 0x04, 0x94 }, { 0x04, 0x9C }, { 0x04, 0xA4 },
	{ 0x04, 0xAC }, { 0x04, 0xB5 }, { 0x04, 0xBD }, { 0x04, 0xC5 }, { 0x04, 0xCD },
	{ 0x04, 0xD5 }, { 0x04, 0xDD }, { 0x04, 0xE6 }, { 0x04, 0xEE }, { 0x04, 0xF6 },
	{ 0x04, 0xFE }, { 0x05, 0x07 }, { 0x05, 0x0F }, { 0x05, 0x17 }, { 0x05, 0x20 },
	{ 0x05, 0x28 }, { 0x05, 0x30 }, { 0x05, 0x39 }, { 0x05, 0x41 }, { 0x05, 0x49 },
	{ 0x05, 0x52 }, { 0x05, 0x5A }, { 0x05, 0x62 }, { 0x05, 0x6B }, { 0x05, 0x73 },
	{ 0x05, 0x7C }, { 0x05, 0x84 }, { 0x05, 0x88 }, { 0x05, 0x99 }, { 0x05, 0xA9 },
	{ 0x05, 0xB9 }, { 0x05, 0xCB }, { 0x05, 0xDC }, { 0x05, 0xEC }, { 0x05, 0xFC },
	{ 0x06, 0x0D }, { 0x06, 0x1D }, { 0x06, 0x2F }, { 0x06, 0x3F }, { 0x06, 0x50 },
	{ 0x06, 0x60 }, { 0x06, 0x70 }, { 0x06, 0x81 }, { 0x06, 0x91 }, { 0x06, 0xA3 },
	{ 0x06, 0xB3 }, { 0x06, 0xC4 }, { 0x06, 0xD4 }, { 0x06, 0xE4 }, { 0x06, 0xF4 },
	{ 0x07, 0x07 }, { 0x07, 0x17 }, { 0x07, 0x27 }, { 0x07, 0x38 }, { 0x07, 0x48 },
	{ 0x07, 0x58 }, { 0x07, 0x6A }, { 0x07, 0x7B }, { 0x07, 0x8B }, { 0x07, 0x9B },
	{ 0x07, 0xAC }, { 0x07, 0xBC }, { 0x07, 0xCC }, { 0x07, 0xDE }, { 0x07, 0xEF },
	{ 0x07, 0xFF },
	/* HBM 5x61+1 */
	{ 0x00, 0x00 }, { 0x00, 0x07 }, { 0x00, 0x0D }, { 0x00, 0x14 }, { 0x00, 0x1B },
	{ 0x00, 0x22 }, { 0x00, 0x28 }, { 0x00, 0x2F }, { 0x00, 0x36 }, { 0x00, 0x3C },
	{ 0x00, 0x43 }, { 0x00, 0x4A }, { 0x00, 0x51 }, { 0x00, 0x57 }, { 0x00, 0x5E },
	{ 0x00, 0x65 }, { 0x00, 0x6B }, { 0x00, 0x72 }, { 0x00, 0x79 }, { 0x00, 0x80 },
	{ 0x00, 0x86 }, { 0x00, 0x8D }, { 0x00, 0x94 }, { 0x00, 0x9A }, { 0x00, 0xA1 },
	{ 0x00, 0xA8 }, { 0x00, 0xAE }, { 0x00, 0xB5 }, { 0x00, 0xBC }, { 0x00, 0xC3 },
	{ 0x00, 0xC9 }, { 0x00, 0xD0 }, { 0x00, 0xD7 }, { 0x00, 0xDD }, { 0x00, 0xE4 },
	{ 0x00, 0xEB }, { 0x00, 0xF2 }, { 0x00, 0xF8 }, { 0x00, 0xFF }, { 0x01, 0x06 },
	{ 0x01, 0x0C }, { 0x01, 0x13 }, { 0x01, 0x1A }, { 0x01, 0x21 }, { 0x01, 0x27 },
	{ 0x01, 0x2E }, { 0x01, 0x35 }, { 0x01, 0x3B }, { 0x01, 0x42 }, { 0x01, 0x49 },
	{ 0x01, 0x50 }, { 0x01, 0x56 }, { 0x01, 0x5D }, { 0x01, 0x64 }, { 0x01, 0x6A },
	{ 0x01, 0x71 }, { 0x01, 0x78 }, { 0x01, 0x7F }, { 0x01, 0x85 }, { 0x01, 0x8C },
	{ 0x01, 0x93 }, { 0x01, 0x99 }, { 0x01, 0xA0 }, { 0x01, 0xA7 }, { 0x01, 0xAE },
	{ 0x01, 0xB4 }, { 0x01, 0xBB }, { 0x01, 0xC2 }, { 0x01, 0xC8 }, { 0x01, 0xCF },
	{ 0x01, 0xD6 }, { 0x01, 0xDD }, { 0x01, 0xE3 }, { 0x01, 0xEA }, { 0x01, 0xF1 },
	{ 0x01, 0xF7 }, { 0x01, 0xFE }, { 0x02, 0x05 }, { 0x02, 0x0B }, { 0x02, 0x12 },
	{ 0x02, 0x19 }, { 0x02, 0x20 }, { 0x02, 0x26 }, { 0x02, 0x2D }, { 0x02, 0x34 },
	{ 0x02, 0x3A }, { 0x02, 0x41 }, { 0x02, 0x48 }, { 0x02, 0x4F }, { 0x02, 0x55 },
	{ 0x02, 0x5C }, { 0x02, 0x63 }, { 0x02, 0x69 }, { 0x02, 0x70 }, { 0x02, 0x77 },
	{ 0x02, 0x7E }, { 0x02, 0x84 }, { 0x02, 0x8B }, { 0x02, 0x92 }, { 0x02, 0x98 },
	{ 0x02, 0x9F }, { 0x02, 0xA6 }, { 0x02, 0xAD }, { 0x02, 0xB3 }, { 0x02, 0xBA },
	{ 0x02, 0xC1 }, { 0x02, 0xC7 }, { 0x02, 0xCE }, { 0x02, 0xD5 }, { 0x02, 0xDC },
	{ 0x02, 0xE2 }, { 0x02, 0xE9 }, { 0x02, 0xF0 }, { 0x02, 0xF6 }, { 0x02, 0xFD },
	{ 0x03, 0x04 }, { 0x03, 0x0B }, { 0x03, 0x11 }, { 0x03, 0x18 }, { 0x03, 0x1F },
	{ 0x03, 0x25 }, { 0x03, 0x2C }, { 0x03, 0x33 }, { 0x03, 0x3A }, { 0x03, 0x40 },
	{ 0x03, 0x47 }, { 0x03, 0x4E }, { 0x03, 0x54 }, { 0x03, 0x5B }, { 0x03, 0x62 },
	{ 0x03, 0x68 }, { 0x03, 0x6F }, { 0x03, 0x76 }, { 0x03, 0x7D }, { 0x03, 0x83 },
	{ 0x03, 0x8A }, { 0x03, 0x91 }, { 0x03, 0x97 }, { 0x03, 0x9E }, { 0x03, 0xA5 },
	{ 0x03, 0xAC }, { 0x03, 0xB2 }, { 0x03, 0xB9 }, { 0x03, 0xC0 }, { 0x03, 0xC6 },
	{ 0x03, 0xCD }, { 0x03, 0xD4 }, { 0x03, 0xDB }, { 0x03, 0xE1 }, { 0x03, 0xE8 },
	{ 0x03, 0xEF }, { 0x03, 0xF5 }, { 0x03, 0xFC }, { 0x04, 0x03 }, { 0x04, 0x0A },
	{ 0x04, 0x10 }, { 0x04, 0x17 }, { 0x04, 0x1E }, { 0x04, 0x24 }, { 0x04, 0x2B },
	{ 0x04, 0x32 }, { 0x04, 0x39 }, { 0x04, 0x3F }, { 0x04, 0x46 }, { 0x04, 0x4D },
	{ 0x04, 0x53 }, { 0x04, 0x5A }, { 0x04, 0x61 }, { 0x04, 0x68 }, { 0x04, 0x6E },
	{ 0x04, 0x75 }, { 0x04, 0x7C }, { 0x04, 0x82 }, { 0x04, 0x89 }, { 0x04, 0x90 },
	{ 0x04, 0x97 }, { 0x04, 0x9D }, { 0x04, 0xA4 }, { 0x04, 0xAB }, { 0x04, 0xB1 },
	{ 0x04, 0xB8 }, { 0x04, 0xBF }, { 0x04, 0xC5 }, { 0x04, 0xCC }, { 0x04, 0xD3 },
	{ 0x04, 0xDA }, { 0x04, 0xE0 }, { 0x04, 0xE7 }, { 0x04, 0xEE }, { 0x04, 0xF4 },
	{ 0x04, 0xFB }, { 0x05, 0x02 }, { 0x05, 0x09 }, { 0x05, 0x0F }, { 0x05, 0x16 },
	{ 0x05, 0x1D }, { 0x05, 0x23 }, { 0x05, 0x2A }, { 0x05, 0x31 }, { 0x05, 0x38 },
	{ 0x05, 0x3E }, { 0x05, 0x45 }, { 0x05, 0x4C }, { 0x05, 0x52 }, { 0x05, 0x59 },
	{ 0x05, 0x60 }, { 0x05, 0x67 }, { 0x05, 0x6D }, { 0x05, 0x74 }, { 0x05, 0x7B },
	{ 0x05, 0x81 }, { 0x05, 0x88 }, { 0x05, 0x8F }, { 0x05, 0x96 }, { 0x05, 0x9C },
	{ 0x05, 0xA3 }, { 0x05, 0xAA }, { 0x05, 0xB0 }, { 0x05, 0xB7 }, { 0x05, 0xBE },
	{ 0x05, 0xC5 }, { 0x05, 0xCB }, { 0x05, 0xD2 }, { 0x05, 0xD9 }, { 0x05, 0xDF },
	{ 0x05, 0xE6 }, { 0x05, 0xED }, { 0x05, 0xF4 }, { 0x05, 0xFA }, { 0x06, 0x01 },
	{ 0x06, 0x08 }, { 0x06, 0x0E }, { 0x06, 0x15 }, { 0x06, 0x1C }, { 0x06, 0x22 },
	{ 0x06, 0x29 }, { 0x06, 0x30 }, { 0x06, 0x37 }, { 0x06, 0x3D }, { 0x06, 0x44 },
	{ 0x06, 0x4B }, { 0x06, 0x51 }, { 0x06, 0x58 }, { 0x06, 0x5F }, { 0x06, 0x66 },
	{ 0x06, 0x6C }, { 0x06, 0x73 }, { 0x06, 0x7A }, { 0x06, 0x80 }, { 0x06, 0x87 },
	{ 0x06, 0x8E }, { 0x06, 0x95 }, { 0x06, 0x9B }, { 0x06, 0xA2 }, { 0x06, 0xA9 },
	{ 0x06, 0xAF }, { 0x06, 0xB6 }, { 0x06, 0xBD }, { 0x06, 0xC4 }, { 0x06, 0xCA },
	{ 0x06, 0xD1 }, { 0x06, 0xD8 }, { 0x06, 0xDE }, { 0x06, 0xE5 }, { 0x06, 0xEC },
	{ 0x06, 0xF3 }, { 0x06, 0xF9 }, { 0x07, 0x00 }, { 0x07, 0x07 }, { 0x07, 0x0D },
	{ 0x07, 0x14 }, { 0x07, 0x1B }, { 0x07, 0x22 }, { 0x07, 0x28 }, { 0x07, 0x2F },
	{ 0x07, 0x36 }, { 0x07, 0x3C }, { 0x07, 0x43 }, { 0x07, 0x4A }, { 0x07, 0x50 },
	{ 0x07, 0x57 }, { 0x07, 0x5E }, { 0x07, 0x65 }, { 0x07, 0x6B }, { 0x07, 0x72 },
	{ 0x07, 0x79 }, { 0x07, 0x7F }, { 0x07, 0x86 }, { 0x07, 0x8D }, { 0x07, 0x94 },
	{ 0x07, 0x9A }, { 0x07, 0xA1 }, { 0x07, 0xA8 }, { 0x07, 0xAE }, { 0x07, 0xB5 },
	{ 0x07, 0xBC }, { 0x07, 0xC3 }, { 0x07, 0xC9 }, { 0x07, 0xD0 }, { 0x07, 0xD7 },
	{ 0x07, 0xDD }, { 0x07, 0xE4 }, { 0x07, 0xEB }, { 0x07, 0xF2 }, { 0x07, 0xF8 },
	{ 0x07, 0xFF },
};

static u8 unbound3_a3_s0_elvss_table[S6E3HAD_TOTAL_STEP][1] = {
	/* Normal  8x32 */
	{ 0x16 }, { 0x16 }, { 0x16 }, { 0x16 }, { 0x16 }, { 0x16 }, { 0x16 }, { 0x16 },
	{ 0x16 }, { 0x16 }, { 0x16 }, { 0x16 }, { 0x16 }, { 0x16 }, { 0x16 }, { 0x16 },
	{ 0x16 }, { 0x16 }, { 0x16 }, { 0x16 }, { 0x16 }, { 0x16 }, { 0x16 }, { 0x16 },
	{ 0x16 }, { 0x16 }, { 0x16 }, { 0x16 }, { 0x16 }, { 0x16 }, { 0x16 }, { 0x16 },
	{ 0x16 }, { 0x16 }, { 0x16 }, { 0x16 }, { 0x16 }, { 0x16 }, { 0x16 }, { 0x16 },
	{ 0x16 }, { 0x16 }, { 0x16 }, { 0x16 }, { 0x16 }, { 0x16 }, { 0x16 }, { 0x16 },
	{ 0x16 }, { 0x16 }, { 0x16 }, { 0x16 }, { 0x16 }, { 0x16 }, { 0x16 }, { 0x16 },
	{ 0x16 }, { 0x16 }, { 0x16 }, { 0x16 }, { 0x16 }, { 0x16 }, { 0x16 }, { 0x16 },
	{ 0x16 }, { 0x16 }, { 0x16 }, { 0x16 }, { 0x16 }, { 0x16 }, { 0x16 }, { 0x16 },
	{ 0x16 }, { 0x16 }, { 0x16 }, { 0x16 }, { 0x16 }, { 0x16 }, { 0x16 }, { 0x16 },
	{ 0x16 }, { 0x16 }, { 0x16 }, { 0x16 }, { 0x16 }, { 0x16 }, { 0x16 }, { 0x16 },
	{ 0x16 }, { 0x16 }, { 0x16 }, { 0x16 }, { 0x16 }, { 0x16 }, { 0x16 }, { 0x16 },
	{ 0x16 }, { 0x16 }, { 0x16 }, { 0x16 }, { 0x16 }, { 0x16 }, { 0x16 }, { 0x16 },
	{ 0x16 }, { 0x16 }, { 0x16 }, { 0x16 }, { 0x16 }, { 0x16 }, { 0x16 }, { 0x16 },
	{ 0x16 }, { 0x16 }, { 0x16 }, { 0x16 }, { 0x16 }, { 0x16 }, { 0x16 }, { 0x16 },
	{ 0x16 }, { 0x16 }, { 0x16 }, { 0x16 }, { 0x16 }, { 0x16 }, { 0x16 }, { 0x16 },
	{ 0x16 }, { 0x16 }, { 0x16 }, { 0x16 }, { 0x16 }, { 0x16 }, { 0x16 }, { 0x16 },
	{ 0x16 }, { 0x16 }, { 0x16 }, { 0x16 }, { 0x16 }, { 0x16 }, { 0x16 }, { 0x16 },
	{ 0x16 }, { 0x16 }, { 0x16 }, { 0x16 }, { 0x16 }, { 0x16 }, { 0x16 }, { 0x16 },
	{ 0x16 }, { 0x16 }, { 0x16 }, { 0x16 }, { 0x16 }, { 0x16 }, { 0x16 }, { 0x16 },
	{ 0x16 }, { 0x16 }, { 0x16 }, { 0x16 }, { 0x16 }, { 0x16 }, { 0x16 }, { 0x16 },
	{ 0x16 }, { 0x16 }, { 0x16 }, { 0x16 }, { 0x16 }, { 0x16 }, { 0x16 }, { 0x16 },
	{ 0x16 }, { 0x16 }, { 0x16 }, { 0x16 }, { 0x16 }, { 0x16 }, { 0x16 }, { 0x16 },
	{ 0x16 }, { 0x16 }, { 0x16 }, { 0x16 }, { 0x16 }, { 0x16 }, { 0x16 }, { 0x16 },
	{ 0x16 }, { 0x16 }, { 0x16 }, { 0x16 }, { 0x16 }, { 0x16 }, { 0x16 }, { 0x16 },
	{ 0x16 }, { 0x16 }, { 0x16 }, { 0x16 }, { 0x16 }, { 0x16 }, { 0x16 }, { 0x16 },
	{ 0x16 }, { 0x16 }, { 0x16 }, { 0x16 }, { 0x16 }, { 0x16 }, { 0x16 }, { 0x16 },
	{ 0x16 }, { 0x16 }, { 0x16 }, { 0x16 }, { 0x16 }, { 0x16 }, { 0x16 }, { 0x16 },
	{ 0x16 }, { 0x16 }, { 0x16 }, { 0x16 }, { 0x16 }, { 0x16 }, { 0x16 }, { 0x16 },
	{ 0x16 }, { 0x16 }, { 0x16 }, { 0x16 }, { 0x16 }, { 0x16 }, { 0x16 }, { 0x16 },
	{ 0x16 }, { 0x16 }, { 0x16 }, { 0x16 }, { 0x16 }, { 0x16 }, { 0x16 }, { 0x16 },
	{ 0x16 }, { 0x16 }, { 0x16 }, { 0x16 }, { 0x16 }, { 0x16 }, { 0x16 }, { 0x16 },
	/* HBM 8x38+2 */
	{ 0x27 }, { 0x27 }, { 0x27 }, { 0x27 }, { 0x27 }, { 0x27 }, { 0x27 }, { 0x27 },
	{ 0x26 }, { 0x26 }, { 0x26 }, { 0x26 }, { 0x26 }, { 0x26 }, { 0x26 }, { 0x26 },
	{ 0x26 }, { 0x26 }, { 0x26 }, { 0x26 }, { 0x26 }, { 0x25 }, { 0x25 }, { 0x25 },
	{ 0x25 }, { 0x25 }, { 0x25 }, { 0x25 }, { 0x25 }, { 0x25 }, { 0x25 }, { 0x25 },
	{ 0x25 }, { 0x25 }, { 0x25 }, { 0x25 }, { 0x25 }, { 0x25 }, { 0x25 }, { 0x25 },
	{ 0x25 }, { 0x25 }, { 0x25 }, { 0x25 }, { 0x25 }, { 0x25 }, { 0x25 }, { 0x24 },
	{ 0x24 }, { 0x24 }, { 0x24 }, { 0x24 }, { 0x24 }, { 0x24 }, { 0x24 }, { 0x24 },
	{ 0x24 }, { 0x24 }, { 0x24 }, { 0x24 }, { 0x23 }, { 0x23 }, { 0x23 }, { 0x23 },
	{ 0x23 }, { 0x23 }, { 0x23 }, { 0x23 }, { 0x23 }, { 0x23 }, { 0x23 }, { 0x23 },
	{ 0x23 }, { 0x22 }, { 0x22 }, { 0x22 }, { 0x22 }, { 0x22 }, { 0x22 }, { 0x22 },
	{ 0x22 }, { 0x22 }, { 0x22 }, { 0x22 }, { 0x22 }, { 0x22 }, { 0x22 }, { 0x22 },
	{ 0x22 }, { 0x22 }, { 0x22 }, { 0x22 }, { 0x22 }, { 0x22 }, { 0x22 }, { 0x22 },
	{ 0x22 }, { 0x22 }, { 0x22 }, { 0x21 }, { 0x21 }, { 0x21 }, { 0x21 }, { 0x21 },
	{ 0x21 }, { 0x21 }, { 0x21 }, { 0x21 }, { 0x21 }, { 0x21 }, { 0x21 }, { 0x21 },
	{ 0x20 }, { 0x20 }, { 0x20 }, { 0x20 }, { 0x20 }, { 0x20 }, { 0x20 }, { 0x20 },
	{ 0x20 }, { 0x20 }, { 0x20 }, { 0x20 }, { 0x20 }, { 0x1F }, { 0x1F }, { 0x1F },
	{ 0x1F }, { 0x1F }, { 0x1F }, { 0x1F }, { 0x1F }, { 0x1F }, { 0x1F }, { 0x1F },
	{ 0x1F }, { 0x1F }, { 0x1F }, { 0x1F }, { 0x1F }, { 0x1F }, { 0x1F }, { 0x1F },
	{ 0x1F }, { 0x1F }, { 0x1F }, { 0x1F }, { 0x1F }, { 0x1F }, { 0x1E }, { 0x1E },
	{ 0x1E }, { 0x1E }, { 0x1E }, { 0x1E }, { 0x1E }, { 0x1E }, { 0x1E }, { 0x1E },
	{ 0x1E }, { 0x1E }, { 0x1E }, { 0x1D }, { 0x1D }, { 0x1D }, { 0x1D }, { 0x1D },
	{ 0x1D }, { 0x1D }, { 0x1D }, { 0x1D }, { 0x1D }, { 0x1D }, { 0x1D }, { 0x1D },
	{ 0x1C }, { 0x1C }, { 0x1C }, { 0x1C }, { 0x1C }, { 0x1C }, { 0x1C }, { 0x1C },
	{ 0x1C }, { 0x1C }, { 0x1C }, { 0x1C }, { 0x1C }, { 0x1C }, { 0x1C }, { 0x1C },
	{ 0x1C }, { 0x1C }, { 0x1C }, { 0x1C }, { 0x1C }, { 0x1C }, { 0x1C }, { 0x1C },
	{ 0x1C }, { 0x1C }, { 0x1B }, { 0x1B }, { 0x1B }, { 0x1B }, { 0x1B }, { 0x1B },
	{ 0x1B }, { 0x1B }, { 0x1B }, { 0x1B }, { 0x1B }, { 0x1B }, { 0x1B }, { 0x1A },
	{ 0x1A }, { 0x1A }, { 0x1A }, { 0x1A }, { 0x1A }, { 0x1A }, { 0x1A }, { 0x1A },
	{ 0x1A }, { 0x1A }, { 0x1A }, { 0x1A }, { 0x19 }, { 0x19 }, { 0x19 }, { 0x19 },
	{ 0x19 }, { 0x19 }, { 0x19 }, { 0x19 }, { 0x19 }, { 0x19 }, { 0x19 }, { 0x19 },
	{ 0x19 }, { 0x19 }, { 0x19 }, { 0x19 }, { 0x19 }, { 0x19 }, { 0x19 }, { 0x19 },
	{ 0x19 }, { 0x19 }, { 0x19 }, { 0x19 }, { 0x19 }, { 0x19 }, { 0x18 }, { 0x18 },
	{ 0x18 }, { 0x18 }, { 0x18 }, { 0x18 }, { 0x18 }, { 0x18 }, { 0x18 }, { 0x18 },
	{ 0x18 }, { 0x18 }, { 0x18 }, { 0x17 }, { 0x17 }, { 0x17 }, { 0x17 }, { 0x17 },
	{ 0x17 }, { 0x17 }, { 0x17 }, { 0x17 }, { 0x17 }, { 0x17 }, { 0x17 }, { 0x17 },
	{ 0x16 }, { 0x16 }, { 0x16 }, { 0x16 }, { 0x16 }, { 0x16 }, { 0x16 }, { 0x16 },
	{ 0x16 }, { 0x16 }, { 0x16 }, { 0x16 }, { 0x16 }, { 0x16 }, { 0x16 }, { 0x16 },
	{ 0x16 }, { 0x16 }, { 0x16 }, { 0x16 }, { 0x16 }, { 0x16 }, { 0x16 }, { 0x16 },
	{ 0x16 }, { 0x16 },
};

static u8 unbound3_a3_s0_vaint_table[S6E3HAD_TOTAL_STEP][1] = {
	/* Normal  8x32 */
	{ 0x00 }, { 0x00 }, { 0x00 }, { 0x00 }, { 0x00 }, { 0x00 }, { 0x00 }, { 0x00 },
	{ 0x00 }, { 0x00 }, { 0x00 }, { 0x00 }, { 0x00 }, { 0x00 }, { 0x00 }, { 0x00 },
	{ 0x00 }, { 0x00 }, { 0x00 }, { 0x00 }, { 0x00 }, { 0x00 }, { 0x00 }, { 0x00 },
	{ 0x00 }, { 0x00 }, { 0x00 }, { 0x00 }, { 0x00 }, { 0x00 }, { 0x00 }, { 0x00 },
	{ 0x00 }, {  MTP }, {  MTP }, {  MTP }, {  MTP }, {  MTP }, {  MTP }, {  MTP },
	{  MTP }, {  MTP }, {  MTP }, {  MTP }, {  MTP }, {  MTP }, {  MTP }, {  MTP },
	{  MTP }, {  MTP }, {  MTP }, {  MTP }, {  MTP }, {  MTP }, {  MTP }, {  MTP },
	{  MTP }, {  MTP }, {  MTP }, {  MTP }, {  MTP }, {  MTP }, {  MTP }, {  MTP },
	{  MTP }, {  MTP }, {  MTP }, {  MTP }, {  MTP }, {  MTP }, {  MTP }, {  MTP },
	{  MTP }, {  MTP }, {  MTP }, {  MTP }, {  MTP }, {  MTP }, {  MTP }, {  MTP },
	{  MTP }, {  MTP }, {  MTP }, {  MTP }, {  MTP }, {  MTP }, {  MTP }, {  MTP },
	{  MTP }, {  MTP }, {  MTP }, {  MTP }, {  MTP }, {  MTP }, {  MTP }, {  MTP },
	{  MTP }, {  MTP }, {  MTP }, {  MTP }, {  MTP }, {  MTP }, {  MTP }, {  MTP },
	{  MTP }, {  MTP }, {  MTP }, {  MTP }, {  MTP }, {  MTP }, {  MTP }, {  MTP },
	{  MTP }, {  MTP }, {  MTP }, {  MTP }, {  MTP }, {  MTP }, {  MTP }, {  MTP },
	{  MTP }, {  MTP }, {  MTP }, {  MTP }, {  MTP }, {  MTP }, {  MTP }, {  MTP },
	{  MTP }, {  MTP }, {  MTP }, {  MTP }, {  MTP }, {  MTP }, {  MTP }, {  MTP },
	{  MTP }, {  MTP }, {  MTP }, {  MTP }, {  MTP }, {  MTP }, {  MTP }, {  MTP },
	{  MTP }, {  MTP }, {  MTP }, {  MTP }, {  MTP }, {  MTP }, {  MTP }, {  MTP },
	{  MTP }, {  MTP }, {  MTP }, {  MTP }, {  MTP }, {  MTP }, {  MTP }, {  MTP },
	{  MTP }, {  MTP }, {  MTP }, {  MTP }, {  MTP }, {  MTP }, {  MTP }, {  MTP },
	{  MTP }, {  MTP }, {  MTP }, {  MTP }, {  MTP }, {  MTP }, {  MTP }, {  MTP },
	{  MTP }, {  MTP }, {  MTP }, {  MTP }, {  MTP }, {  MTP }, {  MTP }, {  MTP },
	{  MTP }, {  MTP }, {  MTP }, {  MTP }, {  MTP }, {  MTP }, {  MTP }, {  MTP },
	{  MTP }, {  MTP }, {  MTP }, {  MTP }, {  MTP }, {  MTP }, {  MTP }, {  MTP },
	{  MTP }, {  MTP }, {  MTP }, {  MTP }, {  MTP }, {  MTP }, {  MTP }, {  MTP },
	{  MTP }, {  MTP }, {  MTP }, {  MTP }, {  MTP }, {  MTP }, {  MTP }, {  MTP },
	{  MTP }, {  MTP }, {  MTP }, {  MTP }, {  MTP }, {  MTP }, {  MTP }, {  MTP },
	{  MTP }, {  MTP }, {  MTP }, {  MTP }, {  MTP }, {  MTP }, {  MTP }, {  MTP },
	{  MTP }, {  MTP }, {  MTP }, {  MTP }, {  MTP }, {  MTP }, {  MTP }, {  MTP },
	{  MTP }, {  MTP }, {  MTP }, {  MTP }, {  MTP }, {  MTP }, {  MTP }, {  MTP },
	{  MTP }, {  MTP }, {  MTP }, {  MTP }, {  MTP }, {  MTP }, {  MTP }, {  MTP },
	/* HBM 8x38+2 */
	{  MTP }, {  MTP }, {  MTP }, {  MTP }, {  MTP }, {  MTP }, {  MTP }, {  MTP },
	{  MTP }, {  MTP }, {  MTP }, {  MTP }, {  MTP }, {  MTP }, {  MTP }, {  MTP },
	{  MTP }, {  MTP }, {  MTP }, {  MTP }, {  MTP }, {  MTP }, {  MTP }, {  MTP },
	{  MTP }, {  MTP }, {  MTP }, {  MTP }, {  MTP }, {  MTP }, {  MTP }, {  MTP },
	{  MTP }, {  MTP }, {  MTP }, {  MTP }, {  MTP }, {  MTP }, {  MTP }, {  MTP },
	{  MTP }, {  MTP }, {  MTP }, {  MTP }, {  MTP }, {  MTP }, {  MTP }, {  MTP },
	{  MTP }, {  MTP }, {  MTP }, {  MTP }, {  MTP }, {  MTP }, {  MTP }, {  MTP },
	{  MTP }, {  MTP }, {  MTP }, {  MTP }, {  MTP }, {  MTP }, {  MTP }, {  MTP },
	{  MTP }, {  MTP }, {  MTP }, {  MTP }, {  MTP }, {  MTP }, {  MTP }, {  MTP },
	{  MTP }, {  MTP }, {  MTP }, {  MTP }, {  MTP }, {  MTP }, {  MTP }, {  MTP },
	{  MTP }, {  MTP }, {  MTP }, {  MTP }, {  MTP }, {  MTP }, {  MTP }, {  MTP },
	{  MTP }, {  MTP }, {  MTP }, {  MTP }, {  MTP }, {  MTP }, {  MTP }, {  MTP },
	{  MTP }, {  MTP }, {  MTP }, {  MTP }, {  MTP }, {  MTP }, {  MTP }, {  MTP },
	{  MTP }, {  MTP }, {  MTP }, {  MTP }, {  MTP }, {  MTP }, {  MTP }, {  MTP },
	{  MTP }, {  MTP }, {  MTP }, {  MTP }, {  MTP }, {  MTP }, {  MTP }, {  MTP },
	{  MTP }, {  MTP }, {  MTP }, {  MTP }, {  MTP }, {  MTP }, {  MTP }, {  MTP },
	{  MTP }, {  MTP }, {  MTP }, {  MTP }, {  MTP }, {  MTP }, {  MTP }, {  MTP },
	{  MTP }, {  MTP }, {  MTP }, {  MTP }, {  MTP }, {  MTP }, {  MTP }, {  MTP },
	{  MTP }, {  MTP }, {  MTP }, {  MTP }, {  MTP }, {  MTP }, {  MTP }, {  MTP },
	{  MTP }, {  MTP }, {  MTP }, {  MTP }, {  MTP }, {  MTP }, {  MTP }, {  MTP },
	{  MTP }, {  MTP }, {  MTP }, {  MTP }, {  MTP }, {  MTP }, {  MTP }, {  MTP },
	{  MTP }, {  MTP }, {  MTP }, {  MTP }, {  MTP }, {  MTP }, {  MTP }, {  MTP },
	{  MTP }, {  MTP }, {  MTP }, {  MTP }, {  MTP }, {  MTP }, {  MTP }, {  MTP },
	{  MTP }, {  MTP }, {  MTP }, {  MTP }, {  MTP }, {  MTP }, {  MTP }, {  MTP },
	{  MTP }, {  MTP }, {  MTP }, {  MTP }, {  MTP }, {  MTP }, {  MTP }, {  MTP },
	{  MTP }, {  MTP }, {  MTP }, {  MTP }, {  MTP }, {  MTP }, {  MTP }, {  MTP },
	{  MTP }, {  MTP }, {  MTP }, {  MTP }, {  MTP }, {  MTP }, {  MTP }, {  MTP },
	{  MTP }, {  MTP }, {  MTP }, {  MTP }, {  MTP }, {  MTP }, {  MTP }, {  MTP },
	{  MTP }, {  MTP }, {  MTP }, {  MTP }, {  MTP }, {  MTP }, {  MTP }, {  MTP },
	{  MTP }, {  MTP }, {  MTP }, {  MTP }, {  MTP }, {  MTP }, {  MTP }, {  MTP },
	{  MTP }, {  MTP }, {  MTP }, {  MTP }, {  MTP }, {  MTP }, {  MTP }, {  MTP },
	{  MTP }, {  MTP }, {  MTP }, {  MTP }, {  MTP }, {  MTP }, {  MTP }, {  MTP },
	{  MTP }, {  MTP }, {  MTP }, {  MTP }, {  MTP }, {  MTP }, {  MTP }, {  MTP },
	{  MTP }, {  MTP }, {  MTP }, {  MTP }, {  MTP }, {  MTP }, {  MTP }, {  MTP },
	{  MTP }, {  MTP }, {  MTP }, {  MTP }, {  MTP }, {  MTP }, {  MTP }, {  MTP },
	{  MTP }, {  MTP }, {  MTP }, {  MTP }, {  MTP }, {  MTP }, {  MTP }, {  MTP },
	{  MTP }, {  MTP }, {  MTP }, {  MTP }, {  MTP }, {  MTP }, {  MTP }, {  MTP },
	{  MTP }, {  MTP }, {  MTP }, {  MTP }, {  MTP }, {  MTP }, {  MTP }, {  MTP },
	{  MTP }, {  MTP },

};

#ifdef CONFIG_SUPPORT_HMD
static u8 unbound3_a3_s0_hmd_brt_table[S6E3HAD_HMD_TOTAL_STEP][2] = {
	/* Normal 5x51+1 */
	{ 0x01, 0x99 }, { 0x01, 0x9F }, { 0x01, 0xA5 }, { 0x01, 0xAB }, { 0x01, 0xB1 },
	{ 0x01, 0xB7 }, { 0x01, 0xBD }, { 0x01, 0xC3 }, { 0x01, 0xC9 }, { 0x01, 0xCF },
	{ 0x01, 0xD5 }, { 0x01, 0xDB }, { 0x01, 0xE1 }, { 0x01, 0xE7 }, { 0x01, 0xED },
	{ 0x01, 0xF3 }, { 0x01, 0xF9 }, { 0x01, 0xFF }, { 0x02, 0x05 }, { 0x02, 0x0B },
	{ 0x02, 0x11 }, { 0x02, 0x17 }, { 0x02, 0x1D }, { 0x02, 0x23 }, { 0x02, 0x29 },
	{ 0x02, 0x2F }, { 0x02, 0x35 }, { 0x02, 0x3B }, { 0x02, 0x41 }, { 0x02, 0x47 },
	{ 0x02, 0x4D }, { 0x02, 0x53 }, { 0x02, 0x59 }, { 0x02, 0x5F }, { 0x02, 0x65 },
	{ 0x02, 0x6B }, { 0x02, 0x71 }, { 0x02, 0x77 }, { 0x02, 0x7D }, { 0x02, 0x83 },
	{ 0x02, 0x89 }, { 0x02, 0x8F }, { 0x02, 0x95 }, { 0x02, 0x9B }, { 0x02, 0xA1 },
	{ 0x02, 0xA7 }, { 0x02, 0xAD }, { 0x02, 0xB3 }, { 0x02, 0xB9 }, { 0x02, 0xBF },
	{ 0x02, 0xC5 }, { 0x02, 0xCB }, { 0x02, 0xD1 }, { 0x02, 0xD7 }, { 0x02, 0xDD },
	{ 0x02, 0xE3 }, { 0x02, 0xE9 }, { 0x02, 0xEF }, { 0x02, 0xF5 }, { 0x02, 0xFB },
	{ 0x03, 0x01 }, { 0x03, 0x07 }, { 0x03, 0x0D }, { 0x03, 0x13 }, { 0x03, 0x19 },
	{ 0x03, 0x1F }, { 0x03, 0x25 }, { 0x03, 0x2B }, { 0x03, 0x31 }, { 0x03, 0x37 },
	{ 0x03, 0x3D }, { 0x03, 0x43 }, { 0x03, 0x49 }, { 0x03, 0x4F }, { 0x03, 0x55 },
	{ 0x03, 0x5B }, { 0x03, 0x61 }, { 0x03, 0x67 }, { 0x03, 0x6D }, { 0x03, 0x73 },
	{ 0x03, 0x79 }, { 0x03, 0x7F }, { 0x03, 0x85 }, { 0x03, 0x8B }, { 0x03, 0x91 },
	{ 0x03, 0x97 }, { 0x03, 0x9D }, { 0x03, 0xA3 }, { 0x03, 0xA9 }, { 0x03, 0xAF },
	{ 0x03, 0xB5 }, { 0x03, 0xBB }, { 0x03, 0xC1 }, { 0x03, 0xC7 }, { 0x03, 0xCD },
	{ 0x03, 0xD3 }, { 0x03, 0xD9 }, { 0x03, 0xDF }, { 0x03, 0xE5 }, { 0x03, 0xEB },
	{ 0x03, 0xF1 }, { 0x03, 0xF7 }, { 0x03, 0xFD }, { 0x04, 0x03 }, { 0x04, 0x09 },
	{ 0x04, 0x0F }, { 0x04, 0x15 }, { 0x04, 0x1B }, { 0x04, 0x21 }, { 0x04, 0x27 },
	{ 0x04, 0x2D }, { 0x04, 0x33 }, { 0x04, 0x39 }, { 0x04, 0x3F }, { 0x04, 0x45 },
	{ 0x04, 0x4B }, { 0x04, 0x51 }, { 0x04, 0x57 }, { 0x04, 0x5D }, { 0x04, 0x63 },
	{ 0x04, 0x69 }, { 0x04, 0x6F }, { 0x04, 0x75 }, { 0x04, 0x7B }, { 0x04, 0x81 },
	{ 0x04, 0x87 }, { 0x04, 0x8D }, { 0x04, 0x93 }, { 0x04, 0x99 }, { 0x04, 0x9F },
	{ 0x04, 0xA5 }, { 0x04, 0xAB }, { 0x04, 0xB1 }, { 0x04, 0xB7 }, { 0x04, 0xBD },
	{ 0x04, 0xC3 }, { 0x04, 0xC9 }, { 0x04, 0xCF }, { 0x04, 0xD5 }, { 0x04, 0xDB },
	{ 0x04, 0xE1 }, { 0x04, 0xE7 }, { 0x04, 0xED }, { 0x04, 0xF3 }, { 0x04, 0xF9 },
	{ 0x04, 0xFF }, { 0x05, 0x05 }, { 0x05, 0x0B }, { 0x05, 0x12 }, { 0x05, 0x19 },
	{ 0x05, 0x20 }, { 0x05, 0x27 }, { 0x05, 0x2E }, { 0x05, 0x35 }, { 0x05, 0x3C },
	{ 0x05, 0x43 }, { 0x05, 0x4A }, { 0x05, 0x51 }, { 0x05, 0x58 }, { 0x05, 0x5F },
	{ 0x05, 0x66 }, { 0x05, 0x6D }, { 0x05, 0x74 }, { 0x05, 0x7B }, { 0x05, 0x82 },
	{ 0x05, 0x89 }, { 0x05, 0x90 }, { 0x05, 0x97 }, { 0x05, 0x9E }, { 0x05, 0xA5 },
	{ 0x05, 0xAC }, { 0x05, 0xB3 }, { 0x05, 0xBA }, { 0x05, 0xC1 }, { 0x05, 0xC8 },
	{ 0x05, 0xCF }, { 0x05, 0xD6 }, { 0x05, 0xDD }, { 0x05, 0xE4 }, { 0x05, 0xEB },
	{ 0x05, 0xF2 }, { 0x05, 0xF9 }, { 0x06, 0x00 }, { 0x06, 0x07 }, { 0x06, 0x0E },
	{ 0x06, 0x15 }, { 0x06, 0x1C }, { 0x06, 0x23 }, { 0x06, 0x2A }, { 0x06, 0x31 },
	{ 0x06, 0x38 }, { 0x06, 0x3F }, { 0x06, 0x46 }, { 0x06, 0x4D }, { 0x06, 0x54 },
	{ 0x06, 0x5B }, { 0x06, 0x62 }, { 0x06, 0x69 }, { 0x06, 0x70 }, { 0x06, 0x77 },
	{ 0x06, 0x7E }, { 0x06, 0x85 }, { 0x06, 0x8C }, { 0x06, 0x93 }, { 0x06, 0x9A },
	{ 0x06, 0xA1 }, { 0x06, 0xA8 }, { 0x06, 0xAF }, { 0x06, 0xB6 }, { 0x06, 0xBD },
	{ 0x06, 0xC4 }, { 0x06, 0xCB }, { 0x06, 0xD2 }, { 0x06, 0xD9 }, { 0x06, 0xE0 },
	{ 0x06, 0xE7 }, { 0x06, 0xEE }, { 0x06, 0xF5 }, { 0x06, 0xFC }, { 0x07, 0x03 },
	{ 0x07, 0x0A }, { 0x07, 0x11 }, { 0x07, 0x18 }, { 0x07, 0x1F }, { 0x07, 0x26 },
	{ 0x07, 0x2D }, { 0x07, 0x34 }, { 0x07, 0x3B }, { 0x07, 0x42 }, { 0x07, 0x49 },
	{ 0x07, 0x50 }, { 0x07, 0x57 }, { 0x07, 0x5E }, { 0x07, 0x65 }, { 0x07, 0x6C },
	{ 0x07, 0x73 }, { 0x07, 0x7A }, { 0x07, 0x81 }, { 0x07, 0x88 }, { 0x07, 0x8F },
	{ 0x07, 0x96 }, { 0x07, 0x9D }, { 0x07, 0xA4 }, { 0x07, 0xAB }, { 0x07, 0xB2 },
	{ 0x07, 0xB9 }, { 0x07, 0xC0 }, { 0x07, 0xC7 }, { 0x07, 0xCE }, { 0x07, 0xD5 },
	{ 0x07, 0xDC }, { 0x07, 0xE3 }, { 0x07, 0xEA }, { 0x07, 0xF1 }, { 0x07, 0xF8 },
	{ 0x07, 0xFF },
};
#endif

static u8 unbound3_a3_s0_aor_manual_onoff_table[MAX_S6E3HAD_AOR_MANUAL][1] = {
	[S6E3HAD_AOR_MANUAL_OFF] = { 0x00 },
	[S6E3HAD_AOR_MANUAL_ON] = { 0x01 },
};
static u8 unbound3_a3_s0_aor_manual_value_table[MAX_S6E3HAD_UNBOUND3_ID][S6E3HAD_TOTAL_STEP][2] = {
	[S6E3HAD_UNBOUND3_ID_E5 ... S6E3HAD_UNBOUND3_ID_E7] = {
		{ 0x0F, 0x62 }, { 0x0F, 0x62 }, { 0x0F, 0x62 }, { 0x0F, 0x62 }, { 0x0F, 0x62 },
		{ 0x0F, 0x62 }, { 0x0F, 0x46 }, { 0x0F, 0x2B }, { 0x0F, 0x12 }, { 0x0E, 0xF9 },
		{ 0x0E, 0xE0 }, { 0x0E, 0xBF }, { 0x0E, 0xA6 }, { 0x0E, 0xA6 }, { 0x0E, 0x94 },
		{ 0x0E, 0x84 }, { 0x0E, 0x6B }, { 0x0E, 0x49 }, { 0x0E, 0x49 }, { 0x0D, 0x87 },
		{ 0x0D, 0x80 }, { 0x0D, 0x23 }, { 0x0D, 0x23 }, { 0x0D, 0x12 }, { 0x0D, 0x12 },
		{ 0x0C, 0xF0 }, { 0x0C, 0xCE }, { 0x0C, 0xA5 }, { 0x0C, 0x83 }, { 0x0C, 0x59 },
		{ 0x0C, 0x30 }, { 0x0C, 0x0E }, { 0x0B, 0xE3 }, { 0x0B, 0xB9 }, { 0x0B, 0x98 },
		{ 0x0B, 0x6E }, { 0x0B, 0x43 }, { 0x0B, 0x1A }, { 0x0A, 0xF0 }, { 0x0A, 0xC6 },
		{ 0x0A, 0x9C }, { 0x0A, 0x71 }, { 0x0A, 0x48 }, { 0x0A, 0x1E }, { 0x09, 0xF3 },
		{ 0x09, 0xC1 }, { 0x09, 0x97 }, { 0x09, 0x6D }, { 0x09, 0x43 }, { 0x09, 0x11 },
		{ 0x08, 0xE6 }, { 0x08, 0xBD }, { 0x08, 0x8A }, { 0x08, 0x61 }, { 0x08, 0x2D },
		{ 0x08, 0x04 }, { 0x07, 0xD1 }, { 0x07, 0x9F }, { 0x07, 0x74 }, { 0x07, 0x42 },
		{ 0x07, 0x19 }, { 0x06, 0xE6 }, { 0x06, 0xB4 }, { 0x06, 0x82 }, { 0x06, 0x57 },
		{ 0x06, 0x25 }, { 0x05, 0xF2 }, { 0x05, 0xC0 }, { 0x05, 0x8E }, { 0x05, 0x5B },
		{ 0x05, 0x29 }, { 0x04, 0xDD }, { 0x04, 0xAB }, { 0x04, 0x70 }, { 0x04, 0x3E },
		{ 0x04, 0x0B }, { 0x03, 0xD0 }, { 0x03, 0x9E }, { 0x03, 0x63 }, { 0x03, 0x30 },
		{ 0x02, 0xF5 }, { 0x02, 0xC3 },
		[82 ... S6E3HAD_TOTAL_STEP - 1] = { 0x02, 0x88 },
	},
	[S6E3HAD_UNBOUND3_ID_E8] = {
		{ 0x0F, 0x62 }, { 0x0F, 0x62 }, { 0x0F, 0x62 }, { 0x0F, 0x62 }, { 0x0F, 0x62 },
		{ 0x0F, 0x62 }, { 0x0F, 0x62 }, { 0x0F, 0x62 }, { 0x0F, 0x62 }, { 0x0F, 0x62 },
		{ 0x0F, 0x62 }, { 0x0F, 0x62 }, { 0x0F, 0x62 }, { 0x0F, 0x62 }, { 0x0F, 0x51 },
		{ 0x0F, 0x3E }, { 0x0F, 0x24 }, { 0x0F, 0x01 }, { 0x0F, 0x01 }, { 0x0E, 0x33 },
		{ 0x0E, 0x2A }, { 0x0D, 0xC8 }, { 0x0D, 0xC8 }, { 0x0D, 0xB7 }, { 0x0D, 0xB7 },
		{ 0x0D, 0x93 }, { 0x0D, 0x70 }, { 0x0D, 0x43 }, { 0x0D, 0x1E }, { 0x0C, 0xF3 },
		{ 0x0C, 0xC6 }, { 0x0C, 0xA3 }, { 0x0C, 0x76 }, { 0x0C, 0x49 }, { 0x0C, 0x26 },
		{ 0x0B, 0xF9 }, { 0x0B, 0xCC }, { 0x0B, 0xA0 }, { 0x0B, 0x73 }, { 0x0B, 0x46 },
		{ 0x0B, 0x1A }, { 0x0A, 0xED }, { 0x0A, 0xC0 }, { 0x0A, 0x94 }, { 0x0A, 0x67 },
		{ 0x0A, 0x32 }, { 0x0A, 0x06 }, { 0x09, 0xD9 }, { 0x09, 0xAC }, { 0x09, 0x76 },
		{ 0x09, 0x4A }, { 0x09, 0x1D }, { 0x08, 0xE8 }, { 0x08, 0xBC }, { 0x08, 0x86 },
		{ 0x08, 0x59 }, { 0x08, 0x23 }, { 0x07, 0xEF }, { 0x07, 0xC2 }, { 0x07, 0x8C },
		{ 0x07, 0x5F }, { 0x07, 0x2B }, { 0x06, 0xF5 }, { 0x06, 0xBF }, { 0x06, 0x92 },
		{ 0x06, 0x5E }, { 0x06, 0x28 }, { 0x05, 0xF2 }, { 0x05, 0xBC }, { 0x05, 0x87 },
		{ 0x05, 0x52 }, { 0x05, 0x01 }, { 0x04, 0xCC }, { 0x04, 0x8E }, { 0x04, 0x58 },
		{ 0x04, 0x22 }, { 0x03, 0xE4 }, { 0x03, 0xAE }, { 0x03, 0x70 }, { 0x03, 0x3B },
		{ 0x02, 0xFD }, { 0x02, 0xC7 },
		[82 ... S6E3HAD_TOTAL_STEP - 1] = { 0x02, 0x88 },
	},
};

static u8 unbound3_a3_s0_read_normal_addr_table[MAX_S6E3HAD_UNBOUND3_FLASH_ADDR_ID][3] = {
	[S6E3HAD_UNBOUND3_FLASH_ADDR_ID_LT_E3] = { 0x0E, 0x14, 0xEC },
	[S6E3HAD_UNBOUND3_FLASH_ADDR_ID_LT_E5] = { 0x0E, 0x15, 0x32 },
	[S6E3HAD_UNBOUND3_FLASH_ADDR_ID_LT_E8] = { 0x0E, 0x15, 0x32 },
	[S6E3HAD_UNBOUND3_FLASH_ADDR_ID_GTE_E8] = { 0x0E, 0x15, 0xBE },
};

static u8 unbound3_a3_s0_read_hbm_addr_table[MAX_S6E3HAD_UNBOUND3_FLASH_ADDR_ID][3] = {
	[S6E3HAD_UNBOUND3_FLASH_ADDR_ID_LT_E3] = { 0x0E, 0x15, 0x32 },
	[S6E3HAD_UNBOUND3_FLASH_ADDR_ID_LT_E5] = { 0x0E, 0x15, 0x78 },
	[S6E3HAD_UNBOUND3_FLASH_ADDR_ID_LT_E8] = { 0x0E, 0x15, 0xBE },
	[S6E3HAD_UNBOUND3_FLASH_ADDR_ID_GTE_E8] = { 0x0E, 0x16, 0x04 },
};

static u8 unbound3_a3_s0_hbm_transition_table[MAX_PANEL_HBM][SMOOTH_TRANS_MAX][1] = {
	/* HBM off */
	{
		/* Normal */
		{ 0x20 },
		/* Smooth */
		{ 0x28 },
	},
	/* HBM on */
	{
		/* Normal */
		{ 0xE0 },
		/* Smooth */
		{ 0xE8 },
	}
};

static u8 unbound3_a3_s0_hbm_transition_lt_e5_table[MAX_PANEL_HBM][SMOOTH_TRANS_MAX][1] = {
	/* HBM off */
	{
		/* Normal */
		{ 0x20 },
		/* Smooth */
		{ 0x28 },
	},
	/* HBM on */
	{
		/* Normal */
		{ 0xE0 },
		/* Smooth */
		{ 0xE0 },
	}
};

static u8 unbound3_a3_s0_acl_opr_table[MAX_PANEL_HBM][MAX_S6E3HAD_ACL_OPR][1] = {
	[PANEL_HBM_OFF] = {
		[S6E3HAD_ACL_OPR_0] = { 0x00 }, /* adaptive_control 0, 0% */
		[S6E3HAD_ACL_OPR_1] = { 0x02 }, /* adaptive_control 1, 15% */
		[S6E3HAD_ACL_OPR_2] = { 0x03 }, /* adaptive_control 2, 30% */
	},
	[PANEL_HBM_ON] = {
		[S6E3HAD_ACL_OPR_0] = { 0x00 }, /* adaptive_control 0, 0% */
		[S6E3HAD_ACL_OPR_1] = { 0x01 }, /* adaptive_control 1, 8% */
		[S6E3HAD_ACL_OPR_2] = { 0x01 }, /* adaptive_control 2, 8% */
	},
};

static u8 unbound3_a3_s0_acl_start_point_table[MAX_S6E3HAD_ACL_OPR][5] = {
	[S6E3HAD_ACL_OPR_0] = { 0x80, 0x01, 0x29, 0x4D, 0x9B }, /* adaptive_control 0, 60% */
	[S6E3HAD_ACL_OPR_1] = { 0x80, 0x01, 0x29, 0x4D, 0x9B }, /* adaptive_control 1, 60% */
	[S6E3HAD_ACL_OPR_2] = { 0x80, 0x01, 0x29, 0x4D, 0x9B }, /* adaptive_control 2, 30% */
};

#ifdef CONFIG_SUPPORT_XTALK_MODE
static u8 unbound3_a3_s0_vgh_table[][1] = {
	{ 0x0C },	/* off */
	{ 0x05 },	/* on */
};
#endif

static u8 unbound3_a3_s0_glut_table[][1] = {
	{ 0x1C },	/* normal */
	{ 0x14 },	/* hbm */
};

static u8 unbound3_a3_s0_sync_control_table[SMOOTH_TRANS_MAX][1] = {
	[SMOOTH_TRANS_OFF] = { 0x20 },	/* smooth trans off */
	[SMOOTH_TRANS_ON] = { 0x60 },	/* smooth trans on */
};

static u8 unbound3_a3_s0_dsc_table[][1] = {
	{ 0x00 },
	{ 0x01 },
};

static u8 unbound3_a3_s0_pps_table[][MAX_S6E3HAD_SCALER][128] = {
	{
		{
			// PPS For DSU MODE 1 : 1440x3200 slice 720*40
			0x11, 0x00, 0x00, 0x89, 0x30, 0x80, 0x0C, 0x80,
			0x05, 0xA0, 0x00, 0x28, 0x02, 0xD0, 0x02, 0xD0,
			0x02, 0x00, 0x02, 0x68, 0x00, 0x20, 0x04, 0x6C,
			0x00, 0x0A, 0x00, 0x0C, 0x02, 0x77, 0x01, 0xE9,
			0x18, 0x00, 0x10, 0xF0, 0x03, 0x0C, 0x20, 0x00,
			0x06, 0x0B, 0x0B, 0x33, 0x0E, 0x1C, 0x2A, 0x38,
			0x46, 0x54, 0x62, 0x69, 0x70, 0x77, 0x79, 0x7B,
			0x7D, 0x7E, 0x01, 0x02, 0x01, 0x00, 0x09, 0x40,
			0x09, 0xBE, 0x19, 0xFC, 0x19, 0xFA, 0x19, 0xF8,
			0x1A, 0x38, 0x1A, 0x78, 0x1A, 0xB6, 0x2A, 0xF6,
			0x2B, 0x34, 0x2B, 0x74, 0x3B, 0x74, 0x6B, 0xF4,
			0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
			0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
			0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
			0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
			0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
		},
		{
			// PPS For DSU MODE 2 : 1080x2400 Slice Info : 540x40
			0x11, 0x00, 0x00, 0x89, 0x30, 0x80, 0x09, 0x60,
			0x04, 0x38, 0x00, 0x28, 0x02, 0x1C, 0x02, 0x1C,
			0x02, 0x00, 0x02, 0x0E, 0x00, 0x20, 0x03, 0xDD,
			0x00, 0x07, 0x00, 0x0C, 0x02, 0x77, 0x02, 0x8B,
			0x18, 0x00, 0x10, 0xF0, 0x03, 0x0C, 0x20, 0x00,
			0x06, 0x0B, 0x0B, 0x33, 0x0E, 0x1C, 0x2A, 0x38,
			0x46, 0x54, 0x62, 0x69, 0x70, 0x77, 0x79, 0x7B,
			0x7D, 0x7E, 0x01, 0x02, 0x01, 0x00, 0x09, 0x40,
			0x09, 0xBE, 0x19, 0xFC, 0x19, 0xFA, 0x19, 0xF8,
			0x1A, 0x38, 0x1A, 0x78, 0x1A, 0xB6, 0x2A, 0xF6,
			0x2B, 0x34, 0x2B, 0x74, 0x3B, 0x74, 0x6B, 0xF4,
			0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
			0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
			0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
			0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
			0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
		},
		{
			// PPS For DSU MODE 3 : 720x1600 Slice Info : 360x80
			0x11, 0x00, 0x00, 0x89, 0x30, 0x80, 0x06, 0x40,
			0x02, 0xD0, 0x00, 0x50, 0x01, 0x68, 0x01, 0x68,
			0x02, 0x00, 0x01, 0xB4, 0x00, 0x20, 0x06, 0x2F,
			0x00, 0x05, 0x00, 0x0C, 0x01, 0x38, 0x01, 0xE9,
			0x18, 0x00, 0x10, 0xF0, 0x03, 0x0C, 0x20, 0x00,
			0x06, 0x0B, 0x0B, 0x33, 0x0E, 0x1C, 0x2A, 0x38,
			0x46, 0x54, 0x62, 0x69, 0x70, 0x77, 0x79, 0x7B,
			0x7D, 0x7E, 0x01, 0x02, 0x01, 0x00, 0x09, 0x40,
			0x09, 0xBE, 0x19, 0xFC, 0x19, 0xFA, 0x19, 0xF8,
			0x1A, 0x38, 0x1A, 0x78, 0x1A, 0xB6, 0x2A, 0xF6,
			0x2B, 0x34, 0x2B, 0x74, 0x3B, 0x74, 0x6B, 0xF4,
			0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
			0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
			0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
			0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
			0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
		},
	}
};

static u8 unbound3_a3_s0_scaler_table[][MAX_S6E3HAD_SCALER][1] = {
	{
		[S6E3HAD_SCALER_OFF] = { 0x01 },
		[S6E3HAD_SCALER_x1_78] = { 0x02 },
		[S6E3HAD_SCALER_x4] = { 0x00 },
	}
};

static u8 unbound3_a3_s0_caset_table[][MAX_S6E3HAD_SCALER][4] = {
	{
		[S6E3HAD_SCALER_OFF] = { 0x00, 0x00, 0x05, 0x9F },
		[S6E3HAD_SCALER_x1_78] = { 0x00, 0x00, 0x04, 0x37 },
		[S6E3HAD_SCALER_x4] = { 0x00, 0x00, 0x02, 0xCF },
	}
};

static u8 unbound3_a3_s0_paset_table[][MAX_S6E3HAD_SCALER][4] = {
	{
		[S6E3HAD_SCALER_OFF] = { 0x00, 0x00, 0x0C, 0x7F },
		[S6E3HAD_SCALER_x1_78] = { 0x00, 0x00, 0x09, 0x5F },
		[S6E3HAD_SCALER_x4] = { 0x00, 0x00, 0x06, 0x3F },
	}
};

static u8 unbound3_a3_s0_lfd_mode_table[MAX_VRR_MODE][1] = {
	[VRR_NORMAL_MODE] = { 0x00 },
	[VRR_HS_MODE] = { 0x01 },
};

static u8 unbound3_a3_s0_lfd_frame_insertion_table[MAX_S6E3HAD_VRR_LFD_FRAME_IDX][MAX_S6E3HAD_VRR_LFD_FRAME_IDX][3] = {
	[S6E3HAD_VRR_LFD_FRAME_IDX_120_HS] = {
		[S6E3HAD_VRR_LFD_FRAME_IDX_120_HS ... S6E3HAD_VRR_LFD_FRAME_IDX_60_HS] = {0x10, 0x00, 0x01},
		[S6E3HAD_VRR_LFD_FRAME_IDX_48_HS ... S6E3HAD_VRR_LFD_FRAME_IDX_10_HS] = {0x14, 0x00, 0x01},
		[S6E3HAD_VRR_LFD_FRAME_IDX_1_HS] = {0xEF, 0x00, 0x01},
	},
	[S6E3HAD_VRR_LFD_FRAME_IDX_96_HS] = {
		[S6E3HAD_VRR_LFD_FRAME_IDX_96_HS ... S6E3HAD_VRR_LFD_FRAME_IDX_60_HS] = {0x10, 0x00, 0x01},
		[S6E3HAD_VRR_LFD_FRAME_IDX_48_HS ... S6E3HAD_VRR_LFD_FRAME_IDX_10_HS] = {0x14, 0x00, 0x01},
		[S6E3HAD_VRR_LFD_FRAME_IDX_1_HS] = {0xEF, 0x00, 0x01},
	},
	[S6E3HAD_VRR_LFD_FRAME_IDX_60_HS] = {
		[S6E3HAD_VRR_LFD_FRAME_IDX_60_HS] = {0x10, 0x00, 0x01},
		[S6E3HAD_VRR_LFD_FRAME_IDX_48_HS ... S6E3HAD_VRR_LFD_FRAME_IDX_10_HS] = {0x00, 0x00, 0x02},
		[S6E3HAD_VRR_LFD_FRAME_IDX_1_HS] = {0xEF, 0x00, 0x01},
	},
	[S6E3HAD_VRR_LFD_FRAME_IDX_48_HS] = {
		[S6E3HAD_VRR_LFD_FRAME_IDX_48_HS ... S6E3HAD_VRR_LFD_FRAME_IDX_10_HS] = {0x00, 0x00, 0x02},
		[S6E3HAD_VRR_LFD_FRAME_IDX_1_HS] = {0xEF, 0x00, 0x01},
	},
	[S6E3HAD_VRR_LFD_FRAME_IDX_32_HS] = {
		[S6E3HAD_VRR_LFD_FRAME_IDX_32_HS ... S6E3HAD_VRR_LFD_FRAME_IDX_10_HS] = {0x00, 0x00, 0x02},
		[S6E3HAD_VRR_LFD_FRAME_IDX_1_HS] = {0xEF, 0x00, 0x01},
	},
	[S6E3HAD_VRR_LFD_FRAME_IDX_30_HS] = {
		[S6E3HAD_VRR_LFD_FRAME_IDX_30_HS ... S6E3HAD_VRR_LFD_FRAME_IDX_10_HS] = {0x00, 0x00, 0x02},
		[S6E3HAD_VRR_LFD_FRAME_IDX_1_HS] = {0xEF, 0x00, 0x01},
	},
	[S6E3HAD_VRR_LFD_FRAME_IDX_24_HS] = {
		[S6E3HAD_VRR_LFD_FRAME_IDX_24_HS ... S6E3HAD_VRR_LFD_FRAME_IDX_10_HS] = {0x00, 0x00, 0x02},
		[S6E3HAD_VRR_LFD_FRAME_IDX_1_HS] = {0xEF, 0x00, 0x01},
	},
	[S6E3HAD_VRR_LFD_FRAME_IDX_10_HS] = {
		[S6E3HAD_VRR_LFD_FRAME_IDX_10_HS] = {0x00, 0x00, 0x02},
		[S6E3HAD_VRR_LFD_FRAME_IDX_1_HS] = {0xEF, 0x00, 0x01},
	},
	[S6E3HAD_VRR_LFD_FRAME_IDX_1_HS] = {
		[S6E3HAD_VRR_LFD_FRAME_IDX_1_HS] = {0xEF, 0x00, 0x01},
	},
	[S6E3HAD_VRR_LFD_FRAME_IDX_60_NS] = {
		[S6E3HAD_VRR_LFD_FRAME_IDX_60_NS ... S6E3HAD_VRR_LFD_FRAME_IDX_30_NS] = {0x00, 0x00, 0x01},
	},
	[S6E3HAD_VRR_LFD_FRAME_IDX_48_NS] = {
		[S6E3HAD_VRR_LFD_FRAME_IDX_48_NS ... S6E3HAD_VRR_LFD_FRAME_IDX_30_NS] = {0x00, 0x00, 0x01},
	},
	[S6E3HAD_VRR_LFD_FRAME_IDX_30_NS] = {
		[S6E3HAD_VRR_LFD_FRAME_IDX_30_NS] = {0x00, 0x00, 0x01},
	},
};

static u8 unbound3_a3_s0_osc_1_table[][1] = {
	[S6E3HAD_VRR_MODE_NS] = { 0x50 },
	[S6E3HAD_VRR_MODE_HS] = { 0x00 },
};

static u8 unbound3_a3_s0_osc_2_table[][1] = {
	[S6E3HAD_VRR_MODE_NS] = { 0x80 },
	[S6E3HAD_VRR_MODE_HS] = { 0x00 },
};

static u8 unbound3_a3_s0_osc_sel_lt_e4_table[][1] = {
	[S6E3HAD_VRR_MODE_NS] = { 0x00 },
	[S6E3HAD_VRR_MODE_HS] = { 0x18 },
};

#ifdef CONFIG_SUPPORT_GRAM_CHECKSUM
static u8 unbound3_a3_s0_vddm_table[][1] = {
	{0x00}, // VDDM ORIGINAL
	{0x01}, // VDDM LV
	{0x02}, // VDDM HV
};

static u8 unbound3_a3_s0_vddm_2_table[][1] = {
	{0x00}, // VDDM ORIGINAL
	{0x07}, // VDDM LV
	{0x28}, // VDDM HV
};

static u8 unbound3_a3_s0_gram_img_pattern_table[][1] = {
	{0x00}, // GCT_PATTERN_NONE
	{0x07}, // GCT_PATTERN_1
	{0x05}, // GCT_PATTERN_2
};
/* switch pattern_1 and pattern_2 */
static u8 unbound3_a3_s0_gram_inv_img_pattern_table[][1] = {
	{0x00}, // GCT_PATTERN_NONE
	{0x05}, // GCT_PATTERN_2
	{0x07}, // GCT_PATTERN_1
};
#endif



static u8 unbound3_a3_s0_lpm_nit_table[4][2] = {
	/* LPM 2NIT */
	{ 0x0C, 0x4C },
	/* LPM 10NIT */
	{ 0x0A, 0xB4 },
	/* LPM 30NIT */
	{ 0x06, 0xB8 },
	/* LPM 60NIT */
	{ 0x00, 0xC0 },
};

static u8 unbound3_a3_s0_lpm_mode_table[2][1] = {
	[ALPM_MODE] = { 0x2B },
	[HLPM_MODE] = { 0x29 },
};

static u8 unbound3_a3_s0_lpm_off_table[2][1] = {
	[ALPM_MODE] = { 0x23 },
	[HLPM_MODE] = { 0x21 },
};

static u8 unbound3_a3_s0_lpm_fps_table[2][2] = {
	[LPM_LFD_1HZ] = { 0x00, 0x00 },  /* always 30Hz */
	[LPM_LFD_30HZ] = { 0x00, 0x00 },  /* always 30Hz */
};

static u8 unbound3_a3_s0_dia_onoff_table[][1] = {
	{ 0x00 }, /* dia off */
	{ 0x02 }, /* dia on */
};

static u8 unbound3_a3_s0_irc_mode_table[][4] = {
	{ 0x61, 0xFF, 0x31, 0x00 }, /* irc moderato */
	{ 0x21, 0xF4, 0x33, 0x6B }, /* irc flat */
};

static u8 unbound3_a3_s0_irc_mode_lt_e3_table[][1] = {
	{ 0x61 }, /* irc moderato */
	{ 0x21 }, /* irc flat */
};

#define UNBOUND3_VFP_60NS_1ST	(0x00)
#define UNBOUND3_VFP_60NS_2ND	(0x10)
#define UNBOUND3_VFP_48NS_1ST	(0x03)
#define UNBOUND3_VFP_48NS_2ND	(0x3C)

#define UNBOUND3_VTT_60NS_1ST	(0x0C)
#define UNBOUND3_VTT_60NS_2ND	(0x60)
#define UNBOUND3_VTT_48NS_1ST	(0x0F)
#define UNBOUND3_VTT_48NS_2ND	(0x50)

#define UNBOUND3_VFP_OPT_60NS_1ST	(0x0C)
#define UNBOUND3_VFP_OPT_60NS_2ND	(0x22)
#define UNBOUND3_VFP_OPT_48NS_1ST	(0x0F)
#define UNBOUND3_VFP_OPT_48NS_2ND	(0x04)

#define UNBOUND3_AOR_60NS_1ST	(0x02)
#define UNBOUND3_AOR_60NS_2ND	(0x00)
#define UNBOUND3_AOR_48NS_1ST	(0x02)
#define UNBOUND3_AOR_48NS_2ND	(0x50)

#define UNBOUND3_VFP_120HS_1ST	(0x00)
#define UNBOUND3_VFP_120HS_2ND	(0x10)
#define UNBOUND3_VFP_96HS_1ST	(0x03)
#define UNBOUND3_VFP_96HS_2ND	(0x3C)

#define UNBOUND3_VTT_120HS_1ST	(0x0C)
#define UNBOUND3_VTT_120HS_2ND	(0x40)
#define UNBOUND3_VTT_96HS_1ST	(0x0F)
#define UNBOUND3_VTT_96HS_2ND	(0x50)

#define UNBOUND3_VFP_OPT_120HS_1ST	(0x0C)
#define UNBOUND3_VFP_OPT_120HS_2ND	(0x02)
#define UNBOUND3_VFP_OPT_96HS_1ST	(0x0F)
#define UNBOUND3_VFP_OPT_96HS_2ND	(0x04)

#define UNBOUND3_AOR_120HS_1ST	(0x01)
#define UNBOUND3_AOR_120HS_2ND	(0xF8)
#define UNBOUND3_AOR_96HS_1ST	(0x02)
#define UNBOUND3_AOR_96HS_2ND	(0x50)

static u8 unbound3_a3_s0_vfp_ns_table[MAX_S6E3HAD_VRR][2] = {
	[S6E3HAD_VRR_60NS] = { UNBOUND3_VFP_60NS_1ST, UNBOUND3_VFP_60NS_2ND },
	[S6E3HAD_VRR_48NS] = { UNBOUND3_VFP_48NS_1ST, UNBOUND3_VFP_48NS_2ND },
	[S6E3HAD_VRR_120HS] = { UNBOUND3_VFP_120HS_1ST, UNBOUND3_VFP_120HS_2ND },
	[S6E3HAD_VRR_96HS] = { UNBOUND3_VFP_96HS_1ST, UNBOUND3_VFP_96HS_2ND },
	[S6E3HAD_VRR_60HS_120HS_TE_SW_SKIP_1] = { UNBOUND3_VFP_120HS_1ST, UNBOUND3_VFP_120HS_2ND },
	[S6E3HAD_VRR_60HS_120HS_TE_HW_SKIP_1] = { UNBOUND3_VFP_120HS_1ST, UNBOUND3_VFP_120HS_2ND },
	[S6E3HAD_VRR_48HS_96HS_TE_SW_SKIP_1] = { UNBOUND3_VFP_96HS_1ST, UNBOUND3_VFP_96HS_2ND },
	[S6E3HAD_VRR_48HS_96HS_TE_HW_SKIP_1] = { UNBOUND3_VFP_96HS_1ST, UNBOUND3_VFP_96HS_2ND },
};

static u8 unbound3_a3_s0_vfp_hs_table[MAX_S6E3HAD_VRR][2] = {
	[S6E3HAD_VRR_60NS] = { UNBOUND3_VFP_120HS_1ST, UNBOUND3_VFP_120HS_2ND },
	[S6E3HAD_VRR_48NS] = { UNBOUND3_VFP_120HS_1ST, UNBOUND3_VFP_120HS_2ND },
	[S6E3HAD_VRR_120HS] = { UNBOUND3_VFP_120HS_1ST, UNBOUND3_VFP_120HS_2ND },
	[S6E3HAD_VRR_96HS] = { UNBOUND3_VFP_96HS_1ST, UNBOUND3_VFP_96HS_2ND },
	[S6E3HAD_VRR_60HS_120HS_TE_SW_SKIP_1] = { UNBOUND3_VFP_120HS_1ST, UNBOUND3_VFP_120HS_2ND },
	[S6E3HAD_VRR_60HS_120HS_TE_HW_SKIP_1] = { UNBOUND3_VFP_120HS_1ST, UNBOUND3_VFP_120HS_2ND },
	[S6E3HAD_VRR_48HS_96HS_TE_SW_SKIP_1] = { UNBOUND3_VFP_96HS_1ST, UNBOUND3_VFP_96HS_2ND },
	[S6E3HAD_VRR_48HS_96HS_TE_HW_SKIP_1] = { UNBOUND3_VFP_96HS_1ST, UNBOUND3_VFP_96HS_2ND },
};

static u8 unbound3_a3_s0_vtotal_ns_table[MAX_S6E3HAD_VRR][2] = {
	[S6E3HAD_VRR_60NS] = { UNBOUND3_VTT_60NS_1ST, UNBOUND3_VTT_60NS_2ND },
	[S6E3HAD_VRR_48NS] = { UNBOUND3_VTT_48NS_1ST, UNBOUND3_VTT_48NS_2ND },
	[S6E3HAD_VRR_120HS] = { UNBOUND3_VTT_60NS_1ST, UNBOUND3_VTT_60NS_2ND },
	[S6E3HAD_VRR_96HS] = { UNBOUND3_VTT_60NS_1ST, UNBOUND3_VTT_60NS_2ND },
	[S6E3HAD_VRR_60HS_120HS_TE_SW_SKIP_1] = { UNBOUND3_VTT_60NS_1ST, UNBOUND3_VTT_60NS_2ND },
	[S6E3HAD_VRR_60HS_120HS_TE_HW_SKIP_1] = { UNBOUND3_VTT_60NS_1ST, UNBOUND3_VTT_60NS_2ND },
	[S6E3HAD_VRR_48HS_96HS_TE_SW_SKIP_1] = { UNBOUND3_VTT_60NS_1ST, UNBOUND3_VTT_60NS_2ND },
	[S6E3HAD_VRR_48HS_96HS_TE_HW_SKIP_1] = { UNBOUND3_VTT_60NS_1ST, UNBOUND3_VTT_60NS_2ND },
};

static u8 unbound3_a3_s0_vtotal_hs_table[MAX_S6E3HAD_VRR][2] = {
	[S6E3HAD_VRR_60NS] = { UNBOUND3_VTT_120HS_1ST, UNBOUND3_VTT_120HS_2ND },
	[S6E3HAD_VRR_48NS] = { UNBOUND3_VTT_120HS_1ST, UNBOUND3_VTT_120HS_2ND },
	[S6E3HAD_VRR_120HS] = { UNBOUND3_VTT_120HS_1ST, UNBOUND3_VTT_120HS_2ND },
	[S6E3HAD_VRR_96HS] = { UNBOUND3_VTT_96HS_1ST, UNBOUND3_VTT_96HS_2ND },
	[S6E3HAD_VRR_60HS_120HS_TE_SW_SKIP_1] = { UNBOUND3_VTT_120HS_1ST, UNBOUND3_VTT_120HS_2ND },
	[S6E3HAD_VRR_60HS_120HS_TE_HW_SKIP_1] = { UNBOUND3_VTT_120HS_1ST, UNBOUND3_VTT_120HS_2ND },
	[S6E3HAD_VRR_48HS_96HS_TE_SW_SKIP_1] = { UNBOUND3_VTT_96HS_1ST, UNBOUND3_VTT_96HS_2ND },
	[S6E3HAD_VRR_48HS_96HS_TE_HW_SKIP_1] = { UNBOUND3_VTT_96HS_1ST, UNBOUND3_VTT_96HS_2ND },
};

static u8 unbound3_a3_s0_vfp_opt_ns_table[MAX_S6E3HAD_VRR][2] = {
	[S6E3HAD_VRR_60NS] = { UNBOUND3_VFP_OPT_60NS_1ST, UNBOUND3_VFP_OPT_60NS_2ND },
	[S6E3HAD_VRR_48NS] = { UNBOUND3_VFP_OPT_48NS_1ST, UNBOUND3_VFP_OPT_48NS_2ND },
	[S6E3HAD_VRR_120HS] = { UNBOUND3_VFP_OPT_60NS_1ST, UNBOUND3_VFP_OPT_60NS_2ND },
	[S6E3HAD_VRR_96HS] = { UNBOUND3_VFP_OPT_60NS_1ST, UNBOUND3_VFP_OPT_60NS_2ND },
	[S6E3HAD_VRR_60HS_120HS_TE_SW_SKIP_1] = { UNBOUND3_VFP_OPT_60NS_1ST, UNBOUND3_VFP_OPT_60NS_2ND },
	[S6E3HAD_VRR_60HS_120HS_TE_HW_SKIP_1] = { UNBOUND3_VFP_OPT_60NS_1ST, UNBOUND3_VFP_OPT_60NS_2ND },
	[S6E3HAD_VRR_48HS_96HS_TE_SW_SKIP_1] = { UNBOUND3_VFP_OPT_60NS_1ST, UNBOUND3_VFP_OPT_60NS_2ND },
	[S6E3HAD_VRR_48HS_96HS_TE_HW_SKIP_1] = { UNBOUND3_VFP_OPT_60NS_1ST, UNBOUND3_VFP_OPT_60NS_2ND },
};

static u8 unbound3_a3_s0_vfp_opt_hs_table[MAX_S6E3HAD_VRR][2] = {
	[S6E3HAD_VRR_60NS] = { UNBOUND3_VFP_OPT_120HS_1ST, UNBOUND3_VFP_OPT_120HS_2ND },
	[S6E3HAD_VRR_48NS] = { UNBOUND3_VFP_OPT_120HS_1ST, UNBOUND3_VFP_OPT_120HS_2ND },
	[S6E3HAD_VRR_120HS] = { UNBOUND3_VFP_OPT_120HS_1ST, UNBOUND3_VFP_OPT_120HS_2ND },
	[S6E3HAD_VRR_96HS] = { UNBOUND3_VFP_OPT_96HS_1ST, UNBOUND3_VFP_OPT_96HS_2ND },
	[S6E3HAD_VRR_60HS_120HS_TE_SW_SKIP_1] = { UNBOUND3_VFP_OPT_120HS_1ST, UNBOUND3_VFP_OPT_120HS_2ND },
	[S6E3HAD_VRR_60HS_120HS_TE_HW_SKIP_1] = { UNBOUND3_VFP_OPT_120HS_1ST, UNBOUND3_VFP_OPT_120HS_2ND },
	[S6E3HAD_VRR_48HS_96HS_TE_SW_SKIP_1] = { UNBOUND3_VFP_OPT_96HS_1ST, UNBOUND3_VFP_OPT_96HS_2ND },
	[S6E3HAD_VRR_48HS_96HS_TE_HW_SKIP_1] = { UNBOUND3_VFP_OPT_96HS_1ST, UNBOUND3_VFP_OPT_96HS_2ND },
};

static u8 unbound3_a3_s0_gamma_write_96hs_0_table[S6E3HAD_TOTAL_STEP][S6E3HAD_GAMMA_WRITE_0_LEN];
static u8 unbound3_a3_s0_gamma_write_96hs_1_table[S6E3HAD_TOTAL_STEP][S6E3HAD_GAMMA_WRITE_1_LEN];


static u8 unbound3_a3_s0_gamma_select_table[MAX_S6E3HAD_VRR][1] = {
	[S6E3HAD_VRR_60NS] = { 0x00 },
	[S6E3HAD_VRR_48NS] = { 0x00 },
	[S6E3HAD_VRR_120HS] = { 0x00 },
	[S6E3HAD_VRR_96HS] = { 0x01 },
	[S6E3HAD_VRR_60HS_120HS_TE_SW_SKIP_1] = { 0x00 },
	[S6E3HAD_VRR_60HS_120HS_TE_HW_SKIP_1] = { 0x00 },
	[S6E3HAD_VRR_48HS_96HS_TE_SW_SKIP_1] = { 0x01 },
	[S6E3HAD_VRR_48HS_96HS_TE_HW_SKIP_1] = { 0x01 },
};


static u8 unbound3_a3_s0_te_frame_sel_table[MAX_S6E3HAD_VRR][1] = {
	[S6E3HAD_VRR_60NS] = { 0x09 },
	[S6E3HAD_VRR_48NS] = { 0x09 },
	[S6E3HAD_VRR_120HS] = { 0x09 },
	[S6E3HAD_VRR_96HS] = { 0x09 },
	[S6E3HAD_VRR_60HS_120HS_TE_SW_SKIP_1] = { 0x09 },
	[S6E3HAD_VRR_60HS_120HS_TE_HW_SKIP_1] = { 0x19 },
	[S6E3HAD_VRR_48HS_96HS_TE_SW_SKIP_1] = { 0x09 },
	[S6E3HAD_VRR_48HS_96HS_TE_HW_SKIP_1] = { 0x19 },
};
static u8 unbound3_a3_s0_lfd_frame_sel_table[MAX_S6E3HAD_VRR_MODE][5] = {
	[S6E3HAD_VRR_MODE_NS] = { 0x00, 0x04, 0x00, 0x00, 0x01 },
	[S6E3HAD_VRR_MODE_HS] = { 0x00, 0x2C, 0x14, 0x00, 0x01 },
};

static u8 UNBOUND3_A3_S0_FFC_DEFAULT[] = {
	0xC5,
	0x11, 0x10, 0x50, 0x05, 0x49, 0x23 /* default 1443 */
};

#ifdef CONFIG_SUPPORT_MAFPC
static u8 unbound3_a3_s0_mafpc_ena_table[][1] = {
	{ 0x00 },
	{ 0x51 },
};
#endif

#ifdef CONFIG_SUPPORT_BRIGHTDOT_TEST
static u8 unbound3_a3_s0_brightdot_aor_table[2][1] = {
	{ 0x00 },    /* off */
	{ 0x01 },    /* on */
};
#endif

static struct maptbl unbound3_a3_s0_maptbl[MAX_MAPTBL] = {
	[GAMMA_MODE2_MAPTBL] = DEFINE_2D_MAPTBL(unbound3_a3_s0_brt_table, init_gamma_mode2_brt_table, getidx_gamma_mode2_brt_table, copy_common_maptbl),
#ifdef CONFIG_SUPPORT_HMD
	[HMD_GAMMA_MAPTBL] = DEFINE_2D_MAPTBL(unbound3_a3_s0_hmd_brt_table, init_gamma_mode2_hmd_brt_table, getidx_gamma_mode2_hmd_brt_table, copy_common_maptbl),
#endif
	[TSET_MAPTBL] = DEFINE_0D_MAPTBL(unbound3_a3_s0_tset_table, init_common_table, NULL, copy_tset_maptbl),
	[VAINT_MAPTBL] = DEFINE_2D_MAPTBL(unbound3_a3_s0_vaint_table, init_common_table, getidx_gm2_elvss_table, copy_vaint_maptbl),
	[ELVSS_MAPTBL] = DEFINE_2D_MAPTBL(unbound3_a3_s0_elvss_table, init_common_table, getidx_gm2_elvss_table, copy_common_maptbl),
	[HBM_ONOFF_MAPTBL] = DEFINE_3D_MAPTBL(unbound3_a3_s0_hbm_transition_table, init_common_table, getidx_hbm_transition_table, copy_common_maptbl),
	[HBM_ONOFF_LT_E5_MAPTBL] = DEFINE_3D_MAPTBL(unbound3_a3_s0_hbm_transition_lt_e5_table, init_common_table, getidx_hbm_transition_table, copy_common_maptbl),
	[ACL_OPR_MAPTBL] = DEFINE_3D_MAPTBL(unbound3_a3_s0_acl_opr_table, init_common_table, getidx_acl_opr_table, copy_common_maptbl),
	[ACL_START_POINT_MAPTBL] = DEFINE_2D_MAPTBL(unbound3_a3_s0_acl_start_point_table, init_common_table, getidx_acl_start_point_table, copy_common_maptbl),
	[LPM_NIT_MAPTBL] = DEFINE_2D_MAPTBL(unbound3_a3_s0_lpm_nit_table, init_lpm_brt_table, getidx_lpm_brt_table, copy_common_maptbl),
	[LPM_MODE_MAPTBL] = DEFINE_2D_MAPTBL(unbound3_a3_s0_lpm_mode_table, init_common_table, getidx_lpm_mode_table, copy_common_maptbl),
	[LPM_FPS_MAPTBL] = DEFINE_2D_MAPTBL(unbound3_a3_s0_lpm_fps_table, init_common_table, getidx_lpm_fps_table, copy_common_maptbl),
	[LPM_OFF_MAPTBL] = DEFINE_2D_MAPTBL(unbound3_a3_s0_lpm_off_table, init_common_table, getidx_lpm_mode_table, copy_common_maptbl),
#ifdef CONFIG_SUPPORT_XTALK_MODE
	[VGH_MAPTBL] = DEFINE_2D_MAPTBL(unbound3_a3_s0_vgh_table, init_common_table, getidx_vgh_table, copy_common_maptbl),
#endif
	[GLUT_MAPTBL] = DEFINE_2D_MAPTBL(unbound3_a3_s0_glut_table, init_common_table, getidx_hbm_table, copy_common_maptbl),
	[SYNC_CONTROL_MAPTBL] = DEFINE_2D_MAPTBL(unbound3_a3_s0_sync_control_table, init_common_table, getidx_sync_control_table, copy_common_maptbl),
	[DSC_MAPTBL] = DEFINE_2D_MAPTBL(unbound3_a3_s0_dsc_table, init_common_table, getidx_dsc_table, copy_common_maptbl),
	[PPS_MAPTBL] = DEFINE_3D_MAPTBL(unbound3_a3_s0_pps_table, init_common_table, getidx_resolution_table, copy_common_maptbl),
	[SCALER_MAPTBL] = DEFINE_3D_MAPTBL(unbound3_a3_s0_scaler_table, init_common_table, getidx_resolution_table, copy_common_maptbl),
	[CASET_MAPTBL] = DEFINE_3D_MAPTBL(unbound3_a3_s0_caset_table, init_common_table, getidx_resolution_table, copy_common_maptbl),
	[PASET_MAPTBL] = DEFINE_3D_MAPTBL(unbound3_a3_s0_paset_table, init_common_table, getidx_resolution_table, copy_common_maptbl),
	[VFP_NS_MAPTBL] = DEFINE_2D_MAPTBL(unbound3_a3_s0_vfp_ns_table, init_common_table, getidx_vrr_fps_table, copy_common_maptbl),
	[VFP_HS_MAPTBL] = DEFINE_2D_MAPTBL(unbound3_a3_s0_vfp_hs_table, init_common_table, getidx_vrr_fps_table, copy_common_maptbl),
	[VTOTAL_NS_MAPTBL] = DEFINE_2D_MAPTBL(unbound3_a3_s0_vtotal_ns_table, init_common_table, getidx_vrr_fps_table, copy_common_maptbl),
	[VTOTAL_HS_MAPTBL] = DEFINE_2D_MAPTBL(unbound3_a3_s0_vtotal_hs_table, init_common_table, getidx_vrr_fps_table, copy_common_maptbl),
	[VFP_OPT_NS_MAPTBL] = DEFINE_2D_MAPTBL(unbound3_a3_s0_vfp_opt_ns_table, init_common_table, getidx_vrr_fps_table, copy_common_maptbl),
	[VFP_OPT_HS_MAPTBL] = DEFINE_2D_MAPTBL(unbound3_a3_s0_vfp_opt_hs_table, init_common_table, getidx_vrr_fps_table, copy_common_maptbl),
	[AOR_MANUAL_ONOFF_MAPTBL] = DEFINE_2D_MAPTBL(unbound3_a3_s0_aor_manual_onoff_table, init_common_table, getidx_aor_manual_onoff_table, copy_common_maptbl),
	[AOR_MANUAL_VALUE_MAPTBL] = DEFINE_3D_MAPTBL(unbound3_a3_s0_aor_manual_value_table, init_common_table, getidx_aor_manual_value_table, copy_common_maptbl),
	[OSC_1_MAPTBL] = DEFINE_2D_MAPTBL(unbound3_a3_s0_osc_1_table, init_common_table, getidx_vrr_mode_table, copy_common_maptbl),
	[OSC_2_MAPTBL] = DEFINE_2D_MAPTBL(unbound3_a3_s0_osc_2_table, init_common_table, getidx_vrr_mode_table, copy_common_maptbl),
	[OSC_SEL_MAPTBL] = DEFINE_2D_MAPTBL(unbound3_a3_s0_osc_sel_lt_e4_table, init_common_table, getidx_vrr_mode_table, copy_common_maptbl),
	[LFD_MIN_MAPTBL] = DEFINE_0D_MAPTBL(unbound3_a3_s0_lfd_min_table, init_common_table, NULL, copy_lfd_min_maptbl),
	[LFD_MAX_MAPTBL] = DEFINE_0D_MAPTBL(unbound3_a3_s0_lfd_max_table, init_common_table, NULL, copy_lfd_max_maptbl),
	[LFD_FRAME_INSERTION_MAPTBL] = DEFINE_3D_MAPTBL(unbound3_a3_s0_lfd_frame_insertion_table,
		init_common_table, getidx_lfd_frame_insertion_table, copy_common_maptbl),
	[LFD_MCA_DITHER_MAPTBL] = DEFINE_2D_MAPTBL(unbound3_a3_s0_lfd_mode_table, init_common_table, getidx_vrr_mode_table, copy_common_maptbl),
	[DIA_ONOFF_MAPTBL] = DEFINE_2D_MAPTBL(unbound3_a3_s0_dia_onoff_table, init_common_table, getidx_dia_onoff_table, copy_common_maptbl),
	[IRC_MODE_MAPTBL] = DEFINE_2D_MAPTBL(unbound3_a3_s0_irc_mode_table, init_common_table, getidx_irc_mode_table, copy_common_maptbl),
	[IRC_MODE_LT_E3_MAPTBL] = DEFINE_2D_MAPTBL(unbound3_a3_s0_irc_mode_lt_e3_table, init_common_table, getidx_irc_mode_table, copy_common_maptbl),
#ifdef CONFIG_SUPPORT_GRAYSPOT_TEST
	[GRAYSPOT_CAL_MAPTBL] = DEFINE_0D_MAPTBL(unbound3_a3_s0_grayspot_cal_table, init_common_table, NULL, copy_grayspot_cal_maptbl),
#endif
#ifdef CONFIG_SUPPORT_GRAM_CHECKSUM
	[VDDM_MAPTBL] = DEFINE_2D_MAPTBL(unbound3_a3_s0_vddm_table, init_common_table, s6e3had_getidx_vddm_table, copy_common_maptbl),
	[VDDM_2_MAPTBL] = DEFINE_2D_MAPTBL(unbound3_a3_s0_vddm_2_table, init_common_table, s6e3had_getidx_vddm_table, copy_common_maptbl),
	[GRAM_IMG_MAPTBL] = DEFINE_2D_MAPTBL(unbound3_a3_s0_gram_img_pattern_table, init_common_table, s6e3had_getidx_gram_img_pattern_table, copy_common_maptbl),
	[GRAM_INV_IMG_MAPTBL] = DEFINE_2D_MAPTBL(unbound3_a3_s0_gram_inv_img_pattern_table, init_common_table, s6e3had_getidx_gram_img_pattern_table, copy_common_maptbl),
#endif
#ifdef CONFIG_DYNAMIC_MIPI
	[DM_SET_FFC_MAPTBL] = DEFINE_1D_MAPTBL(UNBOUND3_A3_S0_FFC_DEFAULT, ARRAY_SIZE(UNBOUND3_A3_S0_FFC_DEFAULT), init_common_table, NULL, dynamic_mipi_set_ffc),
#endif
#ifdef CONFIG_SUPPORT_MAFPC
	[MAFPC_ENA_MAPTBL] = DEFINE_0D_MAPTBL(unbound3_a3_s0_mafpc_enable, init_common_table, NULL, copy_mafpc_enable_maptbl),
	[MAFPC_CTRL_MAPTBL] = DEFINE_0D_MAPTBL(unbound3_a3_s0_mafpc_ctrl, init_common_table, NULL, copy_mafpc_ctrl_maptbl),
	[MAFPC_SCALE_MAPTBL] = DEFINE_0D_MAPTBL(unbound3_a3_s0_mafpc_scale, init_common_table, NULL, copy_mafpc_scale_maptbl),
#endif
	[NORMAL_ADDR_MAPTBL] = DEFINE_2D_MAPTBL(unbound3_a3_s0_read_normal_addr_table, init_common_table, getidx_read_addr_table, copy_common_maptbl),
	[HBM_ADDR_MAPTBL] = DEFINE_2D_MAPTBL(unbound3_a3_s0_read_hbm_addr_table, init_common_table, getidx_read_addr_table, copy_common_maptbl),
	[GAMMA_WRITE_0_MAPTBL] = DEFINE_2D_MAPTBL(unbound3_a3_s0_gamma_write_96hs_0_table, init_gamma_write_96hs_0_table, getidx_gamma_mode2_brt_table, copy_common_maptbl),
	[GAMMA_WRITE_1_MAPTBL] = DEFINE_2D_MAPTBL(unbound3_a3_s0_gamma_write_96hs_1_table, init_gamma_write_96hs_1_table, getidx_gamma_mode2_brt_table, copy_common_maptbl),
	[GAMMA_SELECT_MAPTBL] = DEFINE_2D_MAPTBL(unbound3_a3_s0_gamma_select_table, init_common_table, getidx_vrr_fps_table, copy_common_maptbl),
	[TE_FRAME_SEL_MAPTBL] = DEFINE_2D_MAPTBL(unbound3_a3_s0_te_frame_sel_table, init_common_table, getidx_vrr_fps_table, copy_common_maptbl),
	[LFD_FRAME_SEL_MAPTBL] = DEFINE_2D_MAPTBL(unbound3_a3_s0_lfd_frame_sel_table, init_common_table, getidx_vrr_mode_table, copy_common_maptbl),
#ifdef CONFIG_SUPPORT_BRIGHTDOT_TEST
	[BRIGHTDOT_AOR_MAPTBL] = DEFINE_2D_MAPTBL(unbound3_a3_s0_brightdot_aor_table, init_common_table, s6e3had_getidx_brightdot_aor_table, copy_common_maptbl),
#endif
};

/* ===================================================================================== */
/* ============================== [S6E3HAD COMMAND TABLE] ============================== */
/* ===================================================================================== */

static u8 UNBOUND3_A3_S0_KEY1_ENABLE[] = { 0x9F, 0xA5, 0xA5 };
static u8 UNBOUND3_A3_S0_KEY2_ENABLE[] = { 0xF0, 0x5A, 0x5A };
static u8 UNBOUND3_A3_S0_KEY3_ENABLE[] = { 0xFC, 0x5A, 0x5A };
static u8 UNBOUND3_A3_S0_KEY1_DISABLE[] = { 0x9F, 0x5A, 0x5A };
static u8 UNBOUND3_A3_S0_KEY2_DISABLE[] = { 0xF0, 0xA5, 0xA5 };
static u8 UNBOUND3_A3_S0_KEY3_DISABLE[] = { 0xFC, 0xA5, 0xA5 };
static u8 UNBOUND3_A3_S0_SLEEP_OUT[] = { 0x11 };
static u8 UNBOUND3_A3_S0_SLEEP_IN[] = { 0x10 };
static u8 UNBOUND3_A3_S0_DISPLAY_OFF[] = { 0x28 };
static u8 UNBOUND3_A3_S0_DISPLAY_ON[] = { 0x29 };
static u8 UNBOUND3_A3_S0_DUMMY_2C[] = { 0x2C, 0x00, 0x00, 0x00 };

#ifdef CONFIG_SUPPORT_GRAM_CHECKSUM
static u8 UNBOUND3_A3_S0_SW_RESET[] = { 0x01 };
static u8 UNBOUND3_A3_S0_GCT_DSC[] = { 0x9D, 0x01 };
static u8 UNBOUND3_A3_S0_GCT_PPS[] = { 0x9E,
	0x11, 0x00, 0x00, 0x89, 0x30, 0x80, 0x0C, 0x80,
	0x05, 0xA0, 0x00, 0x28, 0x02, 0xD0, 0x02, 0xD0,
	0x02, 0x00, 0x02, 0x68, 0x00, 0x20, 0x04, 0x6C,
	0x00, 0x0A, 0x00, 0x0C, 0x02, 0x77, 0x01, 0xE9,
	0x18, 0x00, 0x10, 0xF0, 0x03, 0x0C, 0x20, 0x00,
	0x06, 0x0B, 0x0B, 0x33, 0x0E, 0x1C, 0x2A, 0x38,
	0x46, 0x54, 0x62, 0x69, 0x70, 0x77, 0x79, 0x7B,
	0x7D, 0x7E, 0x01, 0x02, 0x01, 0x00, 0x09, 0x40,
	0x09, 0xBE, 0x19, 0xFC, 0x19, 0xFA, 0x19, 0xF8,
	0x1A, 0x38, 0x1A, 0x78, 0x1A, 0xB6, 0x2A, 0xF6,
	0x2B, 0x34, 0x2B, 0x74, 0x3B, 0x74, 0x6B, 0xF4,
	0x00
};
#endif
static u8 UNBOUND3_A3_S0_DSC[] = { 0x01 };
static u8 UNBOUND3_A3_S0_PPS[] = {
	// WQHD : 1440x3200
	0x11, 0x00, 0x00, 0x89, 0x30, 0x80, 0x0C, 0x80,
	0x05, 0xA0, 0x00, 0x28, 0x02, 0xD0, 0x02, 0xD0,
	0x02, 0x00, 0x02, 0x68, 0x00, 0x20, 0x04, 0x6C,
	0x00, 0x0A, 0x00, 0x0C, 0x02, 0x77, 0x01, 0xE9,
	0x18, 0x00, 0x10, 0xF0, 0x03, 0x0C, 0x20, 0x00,
	0x06, 0x0B, 0x0B, 0x33, 0x0E, 0x1C, 0x2A, 0x38,
	0x46, 0x54, 0x62, 0x69, 0x70, 0x77, 0x79, 0x7B,
	0x7D, 0x7E, 0x01, 0x02, 0x01, 0x00, 0x09, 0x40,
	0x09, 0xBE, 0x19, 0xFC, 0x19, 0xFA, 0x19, 0xF8,
	0x1A, 0x38, 0x1A, 0x78, 0x1A, 0xB6, 0x2A, 0xF6,
	0x2B, 0x34, 0x2B, 0x74, 0x3B, 0x74, 0x6B, 0xF4,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
};

static u8 UNBOUND3_A3_S0_TE_ON[] = { 0x35, 0x00 };
static u8 UNBOUND3_A3_S0_ERR_FG_1[] = { 0xF4, 0x18 };
static u8 UNBOUND3_A3_S0_ERR_FG_2[] = { 0xED, 0x40, 0x20 };

static u8 UNBOUND3_A3_S0_TSP_HSYNC[] = {
	0xB9, 0xB1, 0x21
};

static u8 UNBOUND3_A3_S0_SET_TE_FRAME_SEL[] = {
	0xB9,
	0x09,	/* to be updated by TE_FRAME_SEL_MAPTBL */
	0x0B, 0xB8, 0x00, 0x18
};

static u8 UNBOUND3_A3_S0_CLR_TE_FRAME_SEL[] = {
	0xB9,
	0x08,	/* clear TE_FRAME_SEL & TE_SEL */
};

static u8 UNBOUND3_A3_S0_ETC_SET_SLPOUT[] = {
	0xCB, 0x25
};

static u8 UNBOUND3_A3_S0_ETC_SET_SLPIN[] = {
	0xCB, 0x65
};

#ifdef CONFIG_SUPPORT_XTALK_MODE
static u8 UNBOUND3_A3_S0_XTALK_VGH[] = {
	0xF4, 0x0C
};
#endif

static u8 UNBOUND3_A3_S0_LFD_SETTING[] = {
	0xBD,
	0x00, 0x00, 0x00, 0x00, 0x01,
};

static u8 UNBOUND3_A3_S0_LPM_FPS[] = {
	0xBD,
	0x00, 0x00
};

static u8 UNBOUND3_A3_S0_LPM_LFD[] = {
	0xBD,
	0x40
};

static u8 UNBOUND3_A3_S0_LFD_ON_1[] = {
	0xBD, 0x01
};

static u8 UNBOUND3_A3_S0_LFD_ON_2[] = {
	0xBD, 0x40
};

static u8 UNBOUND3_A3_S0_IRC_MODE[] = {
	0x91, 0x61, 0xFF, 0x31, 0x00
};

static u8 UNBOUND3_A3_S0_IRC_MODE_LT_E3[] = {
	0x91, 0x61
};

static u8 UNBOUND3_A3_S0_DIA_SETTING[] = {
	0x91,
	0x81	/* default on */
};

static u8 UNBOUND3_A3_S0_DIA_ONOFF[] = {
	0x91,
	0x02	/* default on */
};

static u8 UNBOUND3_A3_S0_SMOOTH_DIMMING_INIT[] = {
	0xC2, 0x01
};

static u8 UNBOUND3_A3_S0_SMOOTH_DIMMING[] = {
	0xC2, 0x18
};

static u8 UNBOUND3_A3_S0_CHARGE_PUMP[] = {
	0xF4, 0xC0, 0x56
};

static u8 UNBOUND3_A3_S0_WACOM_SYNC_1[] = {
	0xB9, 0xB1, 0x21,
};

static u8 UNBOUND3_A3_S0_WACOM_SYNC_2[] = {
	0xB9, 0x11
};

static u8 UNBOUND3_A3_S0_GLUT[] = {
	0x92,
	0x1C
};

static u8 UNBOUND3_A3_S0_SYNC_CONTROL[] = {
	0xC2,
	0x60
};

static u8 UNBOUND3_A3_S0_DITHER[] = {
	0x6A,
	0x18, 0x86, 0xB1, 0x18, 0x86, 0xB1, 0x18, 0x86,
	0xB1, 0x00, 0x00, 0x00, 0x00, 0x0F
};

static u8 UNBOUND3_A3_S0_TSET[] = {
	0xB1, 0x19,
};

static u8 UNBOUND3_A3_S0_ELVSS[] = {
	0xC2, 0x16,
};

static u8 UNBOUND3_A3_S0_VAINT[] = {
	0xF4, 0x21,
};

static u8 UNBOUND3_A3_S0_MCA_SET_1[] = {
	0x95, 0x51,
};

static u8 UNBOUND3_A3_S0_MCA_SET_2[] = {
	0x96, 0x05,
};

#ifdef CONFIG_SUPPORT_DDI_CMDLOG
static u8 UNBOUND3_A3_S0_CMDLOG_ENABLE[] = { 0xF7, 0x80 };
static u8 UNBOUND3_A3_S0_CMDLOG_DISABLE[] = { 0xF7, 0x00 };
static u8 UNBOUND3_A3_S0_GAMMA_UPDATE_ENABLE[] = { 0xF7, 0x8F };
#else
static u8 UNBOUND3_A3_S0_GAMMA_UPDATE_ENABLE[] = { 0xF7, 0x0F };
#endif
static u8 UNBOUND3_A3_S0_SCALER[] = { 0xBA, 0x01 };
static u8 UNBOUND3_A3_S0_CASET[] = { 0x2A, 0x00, 0x00, 0x05, 0x9F };
static u8 UNBOUND3_A3_S0_PASET[] = { 0x2B, 0x00, 0x00, 0x0C, 0x7F };

static u8 UNBOUND3_A3_S0_OMOK_WA_MOVE_ON[] = { 0x7D, 0x01, 0x01 };
static u8 UNBOUND3_A3_S0_OMOK_WA_MOVE_OFF[] = { 0x7D, 0x00, 0x00 };
static u8 UNBOUND3_A3_S0_OMOK_WA_MOVE_SYNC[] = { 0xF8, 0x00 };

static u8 UNBOUND3_A3_S0_LPM_NIT_PRE[] = { 0xC7, 0x01, 0x0C, 0x4C, 0x02, 0x00, 0xC0 };
static u8 UNBOUND3_A3_S0_LPM_NIT[] = { 0xC7, 0x01, 0x0C, 0x4C, 0x02, 0x00, 0xC0 };
static u8 UNBOUND3_A3_S0_LPM_MODE[] = { 0xBB, 0x29 };
static u8 UNBOUND3_A3_S0_LPM_EXIT[] = { 0xBB, 0x21 };
static u8 UNBOUND3_A3_S0_LPM_CONTROL[] = { 0xC7, 0xFF, 0xFF, 0xFF, 0xFF };
static u8 UNBOUND3_A3_S0_LPM_NIT_DISABLE[] = { 0xC7, 0x00 };
static u8 UNBOUND3_A3_S0_LPM_ON[] = { 0x53, 0x24 };
static u8 UNBOUND3_A3_S0_LPM_ELVSS_ON[] = { 0xC2, 0x2F };
static u8 UNBOUND3_A3_S0_LPM_ELVSS_OFF[] = { 0xC2, 0x16 };
static u8 UNBOUND3_A3_S0_LPM_LFD_OFF[] = { 0xBD, 0x40 };
static u8 UNBOUND3_A3_S0_LPM_SWIRE_NO_PULSE[] = { 0xB1, 0x00 };
static u8 UNBOUND3_A3_S0_LPM_AOR_OFF[] = { 0xC7, 0x00 };

static u8 UNBOUND3_A3_S0_MCD_ON_01[] = { 0xF6, 0x88 };
static u8 UNBOUND3_A3_S0_MCD_ON_02[] = { 0xF6, 0x00 };
static u8 UNBOUND3_A3_S0_MCD_ON_03[] = { 0xF2, 0x34, 0xE1 };
static u8 UNBOUND3_A3_S0_MCD_ON_04[] = {
	0xCB,
	0x00, 0x25, 0x3B, 0x3B, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x50, 0x50, 0x50, 0xD0, 0xD0, 0x50, 0x63,
	0x65, 0x4F, 0x1F, 0x1D, 0x12, 0x1B, 0x19, 0x07,
	0x02, 0x08, 0x03, 0x10, 0x10, 0x10, 0x10, 0x10,
	0x10, 0x23, 0x25, 0x0F, 0x1F, 0x1D, 0x12, 0x1B,
	0x19, 0x07, 0x02, 0x08, 0x03, 0xFC, 0xFD, 0xFF,
	0x55, 0xF7, 0xF0, 0xBF, 0xFF, 0xBF, 0xFF, 0xFF,
	0xF0, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x08, 0x00, 0x10, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x1F, 0x00, 0x02, 0x00, 0x06, 0x02, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x13, 0x00, 0x08, 0x00, 0x00,
	0x00, 0x02, 0x00, 0x1F, 0x00, 0x02, 0x00, 0x06,
	0x02, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x02,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x50, 0x08, 0x54, 0x00, 0x00, 0x00, 0x40,
	0x08, 0x54, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x04,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x2F, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0xF8, 0x30, 0x26, 0x00,
	0x00, 0x00, 0xD8, 0x30, 0x26, 0x00, 0x00, 0x00,
	0xF8, 0x11, 0x26, 0x00, 0x00, 0x00, 0xD8, 0x11,
	0x26, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x1F, 0x00, 0x02, 0x00, 0x06, 0x02, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x13, 0x00, 0x08, 0x00, 0x00,
	0x00, 0x02, 0x00, 0x1F, 0x00, 0x02, 0x00, 0x06,
	0x02, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x02,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x10, 0x0F, 0xA8, 0x00, 0x00, 0x00, 0x00,
	0x0F, 0xA8, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x04,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x2F, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x38, 0x5F, 0x4C, 0x00,
	0x00, 0x00, 0x18, 0x5F, 0x4C, 0x00, 0x00, 0x00,
	0x38, 0x21, 0x4C, 0x00, 0x00, 0x00, 0x18, 0x21,
	0x4C, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x51, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x06,
	0x1F, 0x00, 0x02, 0x00, 0x2E, 0x02, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x13, 0x00, 0x08, 0x00, 0x00,
	0x00, 0x00, 0x06, 0x1F, 0x00, 0x02, 0x00, 0x2E,
	0x02, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x02,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x10, 0x39, 0x2A, 0x00, 0x00, 0x00, 0x00,
	0x39, 0x2A, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x04,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x2B, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x38, 0x18, 0x48, 0x00,
	0x00, 0x00, 0x18, 0x18, 0x48, 0x00, 0x00, 0x00,
	0x38, 0x08, 0x48, 0x00, 0x00, 0x00, 0x18, 0x08,
	0x48, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x0A, 0xAD, 0x31, 0x31, 0x00, 0x2A, 0xAD, 0x19,
	0x19, 0x01, 0x2A, 0xAD, 0x0C, 0x0C, 0x01,
};

static u8 UNBOUND3_A3_S0_MCD_OFF_01[] = { 0xF6, 0x80 };
static u8 UNBOUND3_A3_S0_MCD_OFF_02[] = { 0xF6, 0xAD };
static u8 UNBOUND3_A3_S0_MCD_OFF_03[] = { 0xF2, 0x1A, 0x71 };
static u8 UNBOUND3_A3_S0_MCD_OFF_04[] = {
	0xCB,
	0x00, 0x15, 0x3B, 0x3B, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x50, 0x50, 0x50, 0xD0, 0xD0, 0x50, 0x63,
	0x65, 0x4F, 0x1F, 0x1D, 0x12, 0x1B, 0x19, 0x07,
	0x02, 0x08, 0x03, 0x10, 0x10, 0x10, 0x10, 0x10,
	0x10, 0x23, 0x25, 0x0F, 0x1F, 0x1D, 0x12, 0x1B,
	0x19, 0x07, 0x02, 0x08, 0x03, 0xFC, 0xFD, 0xFF,
	0x55, 0xF7, 0xF0, 0xBF, 0xFF, 0xBF, 0xFF, 0xFF,
	0xF0, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x08, 0x00, 0x10, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x1F, 0x00, 0x02, 0x00, 0x06, 0x02, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x13, 0x00, 0x08, 0x00, 0x00,
	0x00, 0x02, 0x00, 0x1F, 0x00, 0x02, 0x00, 0x06,
	0x02, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x02,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x50, 0x0F, 0xA8, 0x00, 0x00, 0x00, 0x40,
	0x0F, 0xA8, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x04,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x2B, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0xF8, 0x5F, 0x4C, 0x00,
	0x00, 0x00, 0xD8, 0x5F, 0x4C, 0x00, 0x00, 0x00,
	0xF8, 0x21, 0x4C, 0x00, 0x00, 0x00, 0xD8, 0x21,
	0x4C, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x1F, 0x00, 0x02, 0x00, 0x06, 0x02, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x13, 0x00, 0x08, 0x00, 0x00,
	0x00, 0x02, 0x00, 0x1F, 0x00, 0x02, 0x00, 0x06,
	0x02, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x02,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x10, 0x1E, 0x94, 0x00, 0x00, 0x00, 0x00,
	0x1E, 0x94, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x04,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x2B, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x38, 0xA5, 0x16, 0x00,
	0x00, 0x00, 0x18, 0xA5, 0x16, 0x00, 0x00, 0x00,
	0x38, 0x42, 0x16, 0x00, 0x00, 0x00, 0x18, 0x42,
	0x16, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x06,
	0x1F, 0x00, 0x02, 0x00, 0x56, 0x02, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x13, 0x00, 0x08, 0x00, 0x00,
	0x00, 0x12, 0x06, 0x1F, 0x00, 0x02, 0x00, 0x56,
	0x02, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x02,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x10, 0x71, 0x54, 0x00, 0x00, 0x00, 0x00,
	0x71, 0x54, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x04,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x2B, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x38, 0x30, 0x90, 0x00,
	0x00, 0x00, 0x18, 0x30, 0x90, 0x00, 0x00, 0x00,
	0x38, 0x11, 0x90, 0x00, 0x00, 0x00, 0x18, 0x11,
	0x90, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x0A, 0xAD, 0x31, 0x31, 0x00, 0x2A, 0xAD, 0x19,
	0x19, 0x01, 0x2A, 0xAD, 0x0C, 0x0C, 0x01
};

#ifdef CONFIG_SUPPORT_DYNAMIC_HLPM
static u8 UNBOUND3_A3_S0_DYNAMIC_HLPM_ENABLE[] = {
	0x85,
	0x03, 0x0B, 0x01, 0x01, 0x00, 0x00, 0x00, 0x00, 0x02, 0x26, 0x02, 0xB2, 0x07, 0xBC, 0x09, 0xEB
};

static u8 UNBOUND3_A3_S0_DYNAMIC_HLPM_DISABLE[] = {
	0x85,
	0x00
};
#endif

#ifdef CONFIG_SUPPORT_GRAM_CHECKSUM
static u8 UNBOUND3_A3_S0_VDDM_ORIG[] = { 0xD7, 0x00 };
static u8 UNBOUND3_A3_S0_VDDM_ORIG_2[] = { 0xF4, 0x00 };
static u8 UNBOUND3_A3_S0_VDDM_VOLT[] = { 0xD7, 0x00 };
static u8 UNBOUND3_A3_S0_VDDM_VOLT_2[] = { 0xF4, 0x00 };
static u8 UNBOUND3_A3_S0_VDDM_SET_1[] = { 0xFE, 0xB0 };
static u8 UNBOUND3_A3_S0_VDDM_SET_2[] = { 0xFE, 0x30 };
static u8 UNBOUND3_A3_S0_VDDM_INIT[] = { 0xFE, 0x14 };
static u8 UNBOUND3_A3_S0_HOP_CHKSUM_ON[] = { 0xFE, 0x7A };
static u8 UNBOUND3_A3_S0_GRAM_CHKSUM_START[] = { 0x2C, 0x00 };
static u8 UNBOUND3_A3_S0_GRAM_IMG_PATTERN_ON[] = { 0xBE, 0x00 };
static u8 UNBOUND3_A3_S0_GRAM_INV_IMG_PATTERN_ON[] = { 0xBE, 0x00 };
static u8 UNBOUND3_A3_S0_GRAM_IMG_PATTERN_OFF[] = { 0xBE, 0x00 };
static u8 UNBOUND3_A3_S0_GRAM_DBV_MAX[] = { 0x51, 0x07, 0xFF };
static u8 UNBOUND3_A3_S0_GRAM_BCTRL_ON[] = { 0x53, 0x20 };
#endif

#ifdef CONFIG_SUPPORT_MST
static u8 UNBOUND3_A3_S0_MST_ON_01[] = {
	0xCB,
	0x16, 0x94, 0x00, 0x00, 0x00, 0x00, 0x16, 0x94
};
static u8 UNBOUND3_A3_S0_MST_ON_02[] = {
	0xF6,
	0x90
};
static u8 UNBOUND3_A3_S0_MST_ON_03[] = {
	0xBF,
	0x33, 0x25, 0xFF, 0x00, 0x00, 0x10
};

static u8 UNBOUND3_A3_S0_MST_OFF_01[] = {
	0xCB,
	0x1E, 0x94, 0x00, 0x00, 0x00, 0x00, 0x1E, 0x94
};
static u8 UNBOUND3_A3_S0_MST_OFF_02[] = {
	0xF6,
	0xBE
};
static u8 UNBOUND3_A3_S0_MST_OFF_03[] = {
	0xBF,
	0x00, 0x07, 0xFF, 0x00, 0x00, 0x10
};
#endif

#ifdef CONFIG_SUPPORT_CCD_TEST
static u8 UNBOUND3_A3_S0_CCD_ENABLE[] = { 0xCC, 0x01 };
static u8 UNBOUND3_A3_S0_CCD_DISABLE[] = { 0xCC, 0x00 };
static u8 UNBOUND3_A3_S0_CCD_SET[] = { 0xFE, 0x20, 0x20, 0x20, 0x20 };

#endif

#ifdef CONFIG_SUPPORT_GRAYSPOT_TEST
static u8 UNBOUND3_A3_S0_GRAYSPOT_ON_00[] = {
	0xF6, 0x88
};
static u8 UNBOUND3_A3_S0_GRAYSPOT_ON_01[] = {
	0xF6, 0x00
};
static u8 UNBOUND3_A3_S0_GRAYSPOT_ON_02[] = {
	0xB1, 0x19
};
static u8 UNBOUND3_A3_S0_GRAYSPOT_ON_03[] = {
	0xC2, 0x1F
};

static u8 UNBOUND3_A3_S0_GRAYSPOT_OFF_00[] = {
	0xF6, 0x80
};
static u8 UNBOUND3_A3_S0_GRAYSPOT_OFF_01[] = {
	0xF6, 0xAD
};
static u8 UNBOUND3_A3_S0_GRAYSPOT_CAL[] = {
	0xC2, 0x1F
};
#endif

static DEFINE_STATIC_PACKET(unbound3_a3_s0_level1_key_enable, DSI_PKT_TYPE_WR, UNBOUND3_A3_S0_KEY1_ENABLE, 0);
static DEFINE_STATIC_PACKET(unbound3_a3_s0_level2_key_enable, DSI_PKT_TYPE_WR, UNBOUND3_A3_S0_KEY2_ENABLE, 0);
static DEFINE_STATIC_PACKET(unbound3_a3_s0_level3_key_enable, DSI_PKT_TYPE_WR, UNBOUND3_A3_S0_KEY3_ENABLE, 0);
static DEFINE_STATIC_PACKET(unbound3_a3_s0_level1_key_disable, DSI_PKT_TYPE_WR, UNBOUND3_A3_S0_KEY1_DISABLE, 0);
static DEFINE_STATIC_PACKET(unbound3_a3_s0_level2_key_disable, DSI_PKT_TYPE_WR, UNBOUND3_A3_S0_KEY2_DISABLE, 0);
static DEFINE_STATIC_PACKET(unbound3_a3_s0_level3_key_disable, DSI_PKT_TYPE_WR, UNBOUND3_A3_S0_KEY3_DISABLE, 0);

static DEFINE_STATIC_PACKET(unbound3_a3_s0_sleep_out, DSI_PKT_TYPE_WR, UNBOUND3_A3_S0_SLEEP_OUT, 0);
static DEFINE_STATIC_PACKET(unbound3_a3_s0_sleep_in, DSI_PKT_TYPE_WR, UNBOUND3_A3_S0_SLEEP_IN, 0);
static DEFINE_STATIC_PACKET(unbound3_a3_s0_display_on, DSI_PKT_TYPE_WR, UNBOUND3_A3_S0_DISPLAY_ON, 0);
static DEFINE_STATIC_PACKET(unbound3_a3_s0_display_off, DSI_PKT_TYPE_WR, UNBOUND3_A3_S0_DISPLAY_OFF, 0);
static DEFINE_STATIC_PACKET(unbound3_a3_s0_dummy_2c, DSI_PKT_TYPE_WR, UNBOUND3_A3_S0_DUMMY_2C, 0);
static DEFINE_STATIC_PACKET(unbound3_a3_s0_te_on, DSI_PKT_TYPE_WR, UNBOUND3_A3_S0_TE_ON, 0);

static DEFINE_STATIC_PACKET(unbound3_a3_s0_err_fg_1, DSI_PKT_TYPE_WR, UNBOUND3_A3_S0_ERR_FG_1, 0x2C);
static DEFINE_STATIC_PACKET(unbound3_a3_s0_err_fg_2, DSI_PKT_TYPE_WR, UNBOUND3_A3_S0_ERR_FG_2, 0x01);
static DEFINE_STATIC_PACKET(unbound3_a3_s0_dither, DSI_PKT_TYPE_WR, UNBOUND3_A3_S0_DITHER, 0x06);

static DECLARE_PKTUI(unbound3_a3_s0_tset) = {
	{ .offset = 1, .maptbl = &unbound3_a3_s0_maptbl[TSET_MAPTBL] },
};
static DEFINE_VARIABLE_PACKET(unbound3_a3_s0_tset, DSI_PKT_TYPE_WR, UNBOUND3_A3_S0_TSET, 0x0D);

static DEFINE_PKTUI(unbound3_a3_s0_elvss, &unbound3_a3_s0_maptbl[ELVSS_MAPTBL], 1);
static DEFINE_VARIABLE_PACKET(unbound3_a3_s0_elvss, DSI_PKT_TYPE_WR, UNBOUND3_A3_S0_ELVSS, 0x11);
static DEFINE_PKTUI(unbound3_a3_s0_vaint, &unbound3_a3_s0_maptbl[VAINT_MAPTBL], 1);
static DEFINE_VARIABLE_PACKET(unbound3_a3_s0_vaint, DSI_PKT_TYPE_WR, UNBOUND3_A3_S0_VAINT, 0x50);

static DEFINE_STATIC_PACKET(unbound3_a3_s0_tsp_hsync, DSI_PKT_TYPE_WR, UNBOUND3_A3_S0_TSP_HSYNC, 0x0D);
static DEFINE_PKTUI(unbound3_a3_s0_set_te_frame_sel, &unbound3_a3_s0_maptbl[TE_FRAME_SEL_MAPTBL], 1);
static DEFINE_VARIABLE_PACKET(unbound3_a3_s0_set_te_frame_sel, DSI_PKT_TYPE_WR, UNBOUND3_A3_S0_SET_TE_FRAME_SEL, 0);
#if defined(__PANEL_NOT_USED_VARIABLE__)
static DEFINE_PKTUI(unbound3_a3_s0_tsp_hsync, &unbound3_a3_s0_maptbl[TE_FRAME_SEL_MAPTBL], 1);
#endif
static DEFINE_STATIC_PACKET(unbound3_a3_s0_clr_te_frame_sel, DSI_PKT_TYPE_WR, UNBOUND3_A3_S0_CLR_TE_FRAME_SEL, 0);
static DEFINE_STATIC_PACKET(unbound3_a3_s0_etc_set_slpout, DSI_PKT_TYPE_WR, UNBOUND3_A3_S0_ETC_SET_SLPOUT, 0x0A);
static DEFINE_STATIC_PACKET(unbound3_a3_s0_etc_set_slpin, DSI_PKT_TYPE_WR, UNBOUND3_A3_S0_ETC_SET_SLPIN, 0x0A);
static DEFINE_STATIC_PACKET(unbound3_a3_s0_smooth_dimming_init, DSI_PKT_TYPE_WR, UNBOUND3_A3_S0_SMOOTH_DIMMING_INIT, 0x0D);
static DEFINE_STATIC_PACKET(unbound3_a3_s0_smooth_dimming, DSI_PKT_TYPE_WR, UNBOUND3_A3_S0_SMOOTH_DIMMING, 0x0D);
static DEFINE_STATIC_PACKET(unbound3_a3_s0_charge_pump, DSI_PKT_TYPE_WR, UNBOUND3_A3_S0_CHARGE_PUMP, 0x0C);
static DEFINE_STATIC_PACKET(unbound3_a3_s0_wacom_sync_1, DSI_PKT_TYPE_WR, UNBOUND3_A3_S0_WACOM_SYNC_1, 0x0D);
static DEFINE_STATIC_PACKET(unbound3_a3_s0_wacom_sync_2, DSI_PKT_TYPE_WR, UNBOUND3_A3_S0_WACOM_SYNC_2, 0x10);

static DEFINE_STATIC_PACKET(unbound3_a3_s0_mca_set_1, DSI_PKT_TYPE_WR, UNBOUND3_A3_S0_MCA_SET_1, 0x02);
static DEFINE_STATIC_PACKET(unbound3_a3_s0_mca_set_2, DSI_PKT_TYPE_WR, UNBOUND3_A3_S0_MCA_SET_2, 0x05);

static DEFINE_STATIC_PACKET(unbound3_a3_s0_lfd_on_1, DSI_PKT_TYPE_WR, UNBOUND3_A3_S0_LFD_ON_1, 0);
static DEFINE_STATIC_PACKET(unbound3_a3_s0_lfd_on_2, DSI_PKT_TYPE_WR, UNBOUND3_A3_S0_LFD_ON_2, 0x17);

#ifdef CONFIG_SUPPORT_XTALK_MODE
static DEFINE_PKTUI(unbound3_a3_s0_xtalk_vgh, &unbound3_a3_s0_maptbl[VGH_MAPTBL], 1);
static DEFINE_VARIABLE_PACKET(unbound3_a3_s0_xtalk_vgh, DSI_PKT_TYPE_WR, UNBOUND3_A3_S0_XTALK_VGH, 0x1E);
#endif

static DEFINE_PKTUI(unbound3_a3_s0_irc_mode_lt_e3, &unbound3_a3_s0_maptbl[IRC_MODE_LT_E3_MAPTBL], 1);
static DEFINE_VARIABLE_PACKET(unbound3_a3_s0_irc_mode_lt_e3, DSI_PKT_TYPE_WR, UNBOUND3_A3_S0_IRC_MODE_LT_E3, 0xC3);

static DEFINE_PKTUI(unbound3_a3_s0_irc_mode, &unbound3_a3_s0_maptbl[IRC_MODE_MAPTBL], 1);
static DEFINE_VARIABLE_PACKET(unbound3_a3_s0_irc_mode, DSI_PKT_TYPE_WR, UNBOUND3_A3_S0_IRC_MODE, 0xC3);

static DEFINE_STATIC_PACKET(unbound3_a3_s0_dia_setting, DSI_PKT_TYPE_WR, UNBOUND3_A3_S0_DIA_SETTING, 0x67);

static DEFINE_PKTUI(unbound3_a3_s0_dia_onoff, &unbound3_a3_s0_maptbl[DIA_ONOFF_MAPTBL], 1);
static DEFINE_VARIABLE_PACKET(unbound3_a3_s0_dia_onoff, DSI_PKT_TYPE_WR, UNBOUND3_A3_S0_DIA_ONOFF, 0x3E);

static u8 UNBOUND3_A3_S0_AOR_MANUAL_ONOFF[S6E3HAD_AOR_MANUAL_ONOFF_LEN + 1] = { S6E3HAD_AOR_MANUAL_ONOFF_REG, };
static DEFINE_PKTUI(unbound3_a3_s0_aor_manual_onoff, &unbound3_a3_s0_maptbl[AOR_MANUAL_ONOFF_MAPTBL], 1);
static DEFINE_VARIABLE_PACKET(unbound3_a3_s0_aor_manual_onoff, DSI_PKT_TYPE_WR, UNBOUND3_A3_S0_AOR_MANUAL_ONOFF, S6E3HAD_AOR_MANUAL_ONOFF_OFS);

static u8 UNBOUND3_A3_S0_AOR_MANUAL_VALUE[S6E3HAD_AOR_MANUAL_VALUE_LEN + 1] = { S6E3HAD_AOR_MANUAL_VALUE_REG, };
static DEFINE_PKTUI(unbound3_a3_s0_aor_manual_value, &unbound3_a3_s0_maptbl[AOR_MANUAL_VALUE_MAPTBL], 1);
static DEFINE_VARIABLE_PACKET(unbound3_a3_s0_aor_manual_value, DSI_PKT_TYPE_WR, UNBOUND3_A3_S0_AOR_MANUAL_VALUE, S6E3HAD_AOR_MANUAL_VALUE_OFS);

static u8 UNBOUND3_A3_S0_VFP_OPT_NS[S6E3HAD_VFP_OPT_NS_LEN + 1] = { S6E3HAD_VFP_OPT_NS_REG, };
static DEFINE_PKTUI(unbound3_a3_s0_vfp_opt_ns, &unbound3_a3_s0_maptbl[VFP_OPT_NS_MAPTBL], 1);
static DEFINE_VARIABLE_PACKET(unbound3_a3_s0_vfp_opt_ns, DSI_PKT_TYPE_WR, UNBOUND3_A3_S0_VFP_OPT_NS, S6E3HAD_VFP_OPT_NS_OFS);

static u8 UNBOUND3_A3_S0_VFP_OPT_HS[S6E3HAD_VFP_OPT_HS_LEN + 1] = { S6E3HAD_VFP_OPT_HS_REG, };
static DEFINE_PKTUI(unbound3_a3_s0_vfp_opt_hs, &unbound3_a3_s0_maptbl[VFP_OPT_HS_MAPTBL], 1);
static DEFINE_VARIABLE_PACKET(unbound3_a3_s0_vfp_opt_hs, DSI_PKT_TYPE_WR, UNBOUND3_A3_S0_VFP_OPT_HS, S6E3HAD_VFP_OPT_HS_OFS);

static u8 UNBOUND3_A3_S0_VTOTAL_NS_0[S6E3HAD_VTOTAL_NS_0_LEN + 1] = { S6E3HAD_VTOTAL_NS_0_REG, };
static DEFINE_PKTUI(unbound3_a3_s0_vtotal_ns_0, &unbound3_a3_s0_maptbl[VTOTAL_NS_MAPTBL], 1);
static DEFINE_VARIABLE_PACKET(unbound3_a3_s0_vtotal_ns_0, DSI_PKT_TYPE_WR, UNBOUND3_A3_S0_VTOTAL_NS_0, S6E3HAD_VTOTAL_NS_0_OFS);

static u8 UNBOUND3_A3_S0_VTOTAL_NS_1[S6E3HAD_VTOTAL_NS_1_LEN + 1] = { S6E3HAD_VTOTAL_NS_1_REG, };
static DEFINE_PKTUI(unbound3_a3_s0_vtotal_ns_1, &unbound3_a3_s0_maptbl[VTOTAL_NS_MAPTBL], 1);
static DEFINE_VARIABLE_PACKET(unbound3_a3_s0_vtotal_ns_1, DSI_PKT_TYPE_WR, UNBOUND3_A3_S0_VTOTAL_NS_1, S6E3HAD_VTOTAL_NS_1_OFS);

static u8 UNBOUND3_A3_S0_VTOTAL_HS_0[S6E3HAD_VTOTAL_HS_0_LEN + 1] = { S6E3HAD_VTOTAL_HS_0_REG, };
static DEFINE_PKTUI(unbound3_a3_s0_vtotal_hs_0, &unbound3_a3_s0_maptbl[VTOTAL_HS_MAPTBL], 1);
static DEFINE_VARIABLE_PACKET(unbound3_a3_s0_vtotal_hs_0, DSI_PKT_TYPE_WR, UNBOUND3_A3_S0_VTOTAL_HS_0, S6E3HAD_VTOTAL_HS_0_OFS);

static u8 UNBOUND3_A3_S0_VTOTAL_HS_1[S6E3HAD_VTOTAL_HS_1_LEN + 1] = { S6E3HAD_VTOTAL_HS_1_REG, };
static DEFINE_PKTUI(unbound3_a3_s0_vtotal_hs_1, &unbound3_a3_s0_maptbl[VTOTAL_HS_MAPTBL], 1);
static DEFINE_VARIABLE_PACKET(unbound3_a3_s0_vtotal_hs_1, DSI_PKT_TYPE_WR, UNBOUND3_A3_S0_VTOTAL_HS_1, S6E3HAD_VTOTAL_HS_1_OFS);

static u8 UNBOUND3_A3_S0_FPS[] = { 0x60, 0x00, 0x00 };
static DEFINE_PKTUI(unbound3_a3_s0_fps, &unbound3_a3_s0_maptbl[LFD_MAX_MAPTBL], 1);
static DEFINE_VARIABLE_PACKET(unbound3_a3_s0_fps, DSI_PKT_TYPE_WR, UNBOUND3_A3_S0_FPS, 0);

static u8 UNBOUND3_A3_S0_GAMMA_WRITE_0[S6E3HAD_GAMMA_WRITE_0_LEN + 1] = { S6E3HAD_GAMMA_WRITE_0_REG, 0x00, };
static DEFINE_PKTUI(unbound3_a3_s0_gamma_write_0, &unbound3_a3_s0_maptbl[GAMMA_WRITE_0_MAPTBL], 1);
static DEFINE_VARIABLE_PACKET(unbound3_a3_s0_gamma_write_0, DSI_PKT_TYPE_WR, UNBOUND3_A3_S0_GAMMA_WRITE_0, S6E3HAD_GAMMA_WRITE_0_OFS);

static u8 UNBOUND3_A3_S0_GAMMA_WRITE_1[S6E3HAD_GAMMA_WRITE_1_LEN + 1] = { S6E3HAD_GAMMA_WRITE_1_REG, 0x00, };
static DEFINE_PKTUI(unbound3_a3_s0_gamma_write_1, &unbound3_a3_s0_maptbl[GAMMA_WRITE_1_MAPTBL], 1);
static DEFINE_VARIABLE_PACKET(unbound3_a3_s0_gamma_write_1, DSI_PKT_TYPE_WR, UNBOUND3_A3_S0_GAMMA_WRITE_1, S6E3HAD_GAMMA_WRITE_1_OFS);

static u8 UNBOUND3_A3_S0_GAMMA_SELECT[] = { 0xC6, 0x00, };
static DEFINE_PKTUI(unbound3_a3_s0_gamma_select, &unbound3_a3_s0_maptbl[GAMMA_SELECT_MAPTBL], 1);
static DEFINE_VARIABLE_PACKET(unbound3_a3_s0_gamma_select, DSI_PKT_TYPE_WR, UNBOUND3_A3_S0_GAMMA_SELECT, 0x3E3);

static u8 UNBOUND3_A3_S0_GAMMA_SELECT_OFF[] = { 0xC6, 0x00 };
static DEFINE_STATIC_PACKET(unbound3_a3_s0_gamma_select_off, DSI_PKT_TYPE_WR, UNBOUND3_A3_S0_GAMMA_SELECT_OFF, 0x3E3);

static DEFINE_PKTUI(unbound3_a3_s0_dsc, &unbound3_a3_s0_maptbl[DSC_MAPTBL], 0);
static DEFINE_VARIABLE_PACKET(unbound3_a3_s0_dsc, DSI_PKT_TYPE_COMP, UNBOUND3_A3_S0_DSC, 0);

static DEFINE_PKTUI(unbound3_a3_s0_pps, &unbound3_a3_s0_maptbl[PPS_MAPTBL], 0);
static DEFINE_VARIABLE_PACKET(unbound3_a3_s0_pps, DSI_PKT_TYPE_PPS, UNBOUND3_A3_S0_PPS, 0);

static DEFINE_PKTUI(unbound3_a3_s0_scaler, &unbound3_a3_s0_maptbl[SCALER_MAPTBL], 1);
static DEFINE_VARIABLE_PACKET(unbound3_a3_s0_scaler, DSI_PKT_TYPE_WR, UNBOUND3_A3_S0_SCALER, 0);
static DEFINE_PKTUI(unbound3_a3_s0_caset, &unbound3_a3_s0_maptbl[CASET_MAPTBL], 1);
static DEFINE_VARIABLE_PACKET(unbound3_a3_s0_caset, DSI_PKT_TYPE_WR, UNBOUND3_A3_S0_CASET, 0);
static DEFINE_PKTUI(unbound3_a3_s0_paset, &unbound3_a3_s0_maptbl[PASET_MAPTBL], 1);
static DEFINE_VARIABLE_PACKET(unbound3_a3_s0_paset, DSI_PKT_TYPE_WR, UNBOUND3_A3_S0_PASET, 0);
static DEFINE_PKTUI(unbound3_a3_s0_glut, &unbound3_a3_s0_maptbl[GLUT_MAPTBL], 1);
static DEFINE_VARIABLE_PACKET(unbound3_a3_s0_glut, DSI_PKT_TYPE_WR, UNBOUND3_A3_S0_GLUT, 0x04);
static DEFINE_PKTUI(unbound3_a3_s0_sync_control, &unbound3_a3_s0_maptbl[SYNC_CONTROL_MAPTBL], 1);
static DEFINE_VARIABLE_PACKET(unbound3_a3_s0_sync_control, DSI_PKT_TYPE_WR, UNBOUND3_A3_S0_SYNC_CONTROL, 0x0C);

static DEFINE_STATIC_PACKET(unbound3_a3_s0_omok_wa_move_on, DSI_PKT_TYPE_WR, UNBOUND3_A3_S0_OMOK_WA_MOVE_ON, 0x0);
static DEFINE_STATIC_PACKET(unbound3_a3_s0_omok_wa_move_off, DSI_PKT_TYPE_WR, UNBOUND3_A3_S0_OMOK_WA_MOVE_OFF, 0x0);
static DEFINE_STATIC_PACKET(unbound3_a3_s0_omok_wa_move_sync, DSI_PKT_TYPE_WR, UNBOUND3_A3_S0_OMOK_WA_MOVE_SYNC, 0x0B);

static DEFINE_COND(unbound3_a3_s0_cond_is_panel_mres_updated, is_panel_mres_updated);
static DEFINE_COND(unbound3_a3_s0_cond_is_panel_mres_updated_bigger, is_panel_mres_updated_bigger);

/* LPM MODE SETTING */
static DEFINE_PKTUI(unbound3_a3_s0_lpm_exit_mode, &unbound3_a3_s0_maptbl[LPM_OFF_MAPTBL], 1);
static DEFINE_VARIABLE_PACKET(unbound3_a3_s0_lpm_exit_mode, DSI_PKT_TYPE_WR, UNBOUND3_A3_S0_LPM_EXIT, 0);
//static DEFINE_PKTUI(unbound3_a3_s0_lpm_enter_mode, &unbound3_a3_s0_maptbl[LPM_MODE_MAPTBL], 1);
//static DEFINE_VARIABLE_PACKET(unbound3_a3_s0_lpm_enter_mode, DSI_PKT_TYPE_WR, UNBOUND3_A3_S0_LPM_MODE, 0);
static DEFINE_STATIC_PACKET(unbound3_a3_s0_lpm_enter_mode, DSI_PKT_TYPE_WR, UNBOUND3_A3_S0_LPM_MODE, 0);

static DEFINE_STATIC_PACKET(unbound3_a3_s0_lpm_nit_pre, DSI_PKT_TYPE_WR, UNBOUND3_A3_S0_LPM_NIT_PRE, 0x53);
static DEFINE_PKTUI(unbound3_a3_s0_lpm_nit, &unbound3_a3_s0_maptbl[LPM_NIT_MAPTBL], 2);
static DEFINE_VARIABLE_PACKET(unbound3_a3_s0_lpm_nit, DSI_PKT_TYPE_WR, UNBOUND3_A3_S0_LPM_NIT, 0x53);
static DEFINE_STATIC_PACKET(unbound3_a3_s0_lpm_control, DSI_PKT_TYPE_WR, UNBOUND3_A3_S0_LPM_CONTROL, 0x4E);
static DEFINE_STATIC_PACKET(unbound3_a3_s0_lpm_nit_disable, DSI_PKT_TYPE_WR, UNBOUND3_A3_S0_LPM_NIT_DISABLE, 0x53);
static DEFINE_STATIC_PACKET(unbound3_a3_s0_lpm_on, DSI_PKT_TYPE_WR, UNBOUND3_A3_S0_LPM_ON, 0);
static DEFINE_STATIC_PACKET(unbound3_a3_s0_lpm_elvss_on, DSI_PKT_TYPE_WR, UNBOUND3_A3_S0_LPM_ELVSS_ON, 0x11);
static DEFINE_STATIC_PACKET(unbound3_a3_s0_lpm_elvss_off, DSI_PKT_TYPE_WR, UNBOUND3_A3_S0_LPM_ELVSS_OFF, 0x11);
static DEFINE_STATIC_PACKET(unbound3_a3_s0_lpm_swire_no_pulse, DSI_PKT_TYPE_WR,
	UNBOUND3_A3_S0_LPM_SWIRE_NO_PULSE, 0x14);
static DEFINE_STATIC_PACKET(unbound3_a3_s0_lpm_aor_off, DSI_PKT_TYPE_WR, UNBOUND3_A3_S0_LPM_AOR_OFF, 0x53);

/* lpm always 30Hz */
static DEFINE_STATIC_PACKET(unbound3_a3_s0_lpm_fps, DSI_PKT_TYPE_WR, UNBOUND3_A3_S0_LPM_FPS, 0x04);
static DEFINE_STATIC_PACKET(unbound3_a3_s0_lpm_lfd, DSI_PKT_TYPE_WR, UNBOUND3_A3_S0_LPM_LFD, 0x17);
static DEFINE_STATIC_PACKET(unbound3_a3_s0_lpm_lfd_off, DSI_PKT_TYPE_WR, UNBOUND3_A3_S0_LPM_LFD_OFF, 0x17);

#ifdef CONFIG_SUPPORT_GRAM_CHECKSUM
static DEFINE_STATIC_PACKET(unbound3_a3_s0_sw_reset, DSI_PKT_TYPE_WR, UNBOUND3_A3_S0_SW_RESET, 0);
static DEFINE_STATIC_PACKET(unbound3_a3_s0_gct_dsc, DSI_PKT_TYPE_WR, UNBOUND3_A3_S0_GCT_DSC, 0);
static DEFINE_STATIC_PACKET(unbound3_a3_s0_gct_pps, DSI_PKT_TYPE_WR, UNBOUND3_A3_S0_GCT_PPS, 0);
static DEFINE_STATIC_PACKET(unbound3_a3_s0_vddm_orig, DSI_PKT_TYPE_WR, UNBOUND3_A3_S0_VDDM_ORIG, 0x03);
static DEFINE_STATIC_PACKET(unbound3_a3_s0_vddm_orig_2, DSI_PKT_TYPE_WR, UNBOUND3_A3_S0_VDDM_ORIG_2, 0x10);
static DEFINE_PKTUI(unbound3_a3_s0_vddm_volt, &unbound3_a3_s0_maptbl[VDDM_MAPTBL], 1);
static DEFINE_VARIABLE_PACKET(unbound3_a3_s0_vddm_volt, DSI_PKT_TYPE_WR, UNBOUND3_A3_S0_VDDM_VOLT, 0x03);
static DEFINE_PKTUI(unbound3_a3_s0_vddm_volt_2, &unbound3_a3_s0_maptbl[VDDM_2_MAPTBL], 1);
static DEFINE_VARIABLE_PACKET(unbound3_a3_s0_vddm_volt_2, DSI_PKT_TYPE_WR, UNBOUND3_A3_S0_VDDM_VOLT_2, 0x10);
static DEFINE_STATIC_PACKET(unbound3_a3_s0_vddm_set_1, DSI_PKT_TYPE_WR, UNBOUND3_A3_S0_VDDM_SET_1, 0);
static DEFINE_STATIC_PACKET(unbound3_a3_s0_vddm_set_2, DSI_PKT_TYPE_WR, UNBOUND3_A3_S0_VDDM_SET_2, 0);
static DEFINE_STATIC_PACKET(unbound3_a3_s0_vddm_init, DSI_PKT_TYPE_WR, UNBOUND3_A3_S0_VDDM_INIT, 0x0B);
static DEFINE_STATIC_PACKET(unbound3_a3_s0_hop_chksum_on, DSI_PKT_TYPE_WR, UNBOUND3_A3_S0_HOP_CHKSUM_ON, 0x27);

static DEFINE_STATIC_PACKET(unbound3_a3_s0_gram_chksum_start, DSI_PKT_TYPE_WR, UNBOUND3_A3_S0_GRAM_CHKSUM_START, 0);
static DEFINE_PKTUI(unbound3_a3_s0_gram_img_pattern_on, &unbound3_a3_s0_maptbl[GRAM_IMG_MAPTBL], 1);
static DEFINE_VARIABLE_PACKET(unbound3_a3_s0_gram_img_pattern_on, DSI_PKT_TYPE_WR, UNBOUND3_A3_S0_GRAM_IMG_PATTERN_ON, 0);
static DEFINE_PKTUI(unbound3_a3_s0_gram_inv_img_pattern_on, &unbound3_a3_s0_maptbl[GRAM_INV_IMG_MAPTBL], 1);
static DEFINE_VARIABLE_PACKET(unbound3_a3_s0_gram_inv_img_pattern_on, DSI_PKT_TYPE_WR, UNBOUND3_A3_S0_GRAM_INV_IMG_PATTERN_ON, 0);
static DEFINE_STATIC_PACKET(unbound3_a3_s0_gram_img_pattern_off, DSI_PKT_TYPE_WR, UNBOUND3_A3_S0_GRAM_IMG_PATTERN_OFF, 0);
static DEFINE_STATIC_PACKET(unbound3_a3_s0_gram_dbv_max, DSI_PKT_TYPE_WR, UNBOUND3_A3_S0_GRAM_DBV_MAX, 0);
static DEFINE_STATIC_PACKET(unbound3_a3_s0_gram_bctrl_on, DSI_PKT_TYPE_WR, UNBOUND3_A3_S0_GRAM_BCTRL_ON, 0);

#endif

#ifdef CONFIG_SUPPORT_CCD_TEST
static DEFINE_STATIC_PACKET(unbound3_a3_s0_ccd_test_enable, DSI_PKT_TYPE_WR, UNBOUND3_A3_S0_CCD_ENABLE, 0);
static DEFINE_STATIC_PACKET(unbound3_a3_s0_ccd_test_disable, DSI_PKT_TYPE_WR, UNBOUND3_A3_S0_CCD_DISABLE, 0);
static DEFINE_STATIC_PACKET(unbound3_a3_s0_ccd_set, DSI_PKT_TYPE_WR, UNBOUND3_A3_S0_CCD_SET, 0x29);
#endif

#ifdef CONFIG_SUPPORT_DYNAMIC_HLPM
static DEFINE_STATIC_PACKET(unbound3_a3_s0_dynamic_hlpm_enable, DSI_PKT_TYPE_WR, UNBOUND3_A3_S0_DYNAMIC_HLPM_ENABLE, 0);
static DEFINE_STATIC_PACKET(unbound3_a3_s0_dynamic_hlpm_disable, DSI_PKT_TYPE_WR, UNBOUND3_A3_S0_DYNAMIC_HLPM_DISABLE, 0);
#endif

#ifdef CONFIG_SUPPORT_DDI_CMDLOG
static DEFINE_STATIC_PACKET(unbound3_a3_s0_cmdlog_enable, DSI_PKT_TYPE_WR, UNBOUND3_A3_S0_CMDLOG_ENABLE, 0);
static DEFINE_STATIC_PACKET(unbound3_a3_s0_cmdlog_disable, DSI_PKT_TYPE_WR, UNBOUND3_A3_S0_CMDLOG_DISABLE, 0);
#endif
static DEFINE_STATIC_PACKET(unbound3_a3_s0_gamma_update_enable, DSI_PKT_TYPE_WR, UNBOUND3_A3_S0_GAMMA_UPDATE_ENABLE, 0);

static DEFINE_STATIC_PACKET(unbound3_a3_s0_mcd_on_01, DSI_PKT_TYPE_WR, UNBOUND3_A3_S0_MCD_ON_01, 0x0C);
static DEFINE_STATIC_PACKET(unbound3_a3_s0_mcd_on_02, DSI_PKT_TYPE_WR, UNBOUND3_A3_S0_MCD_ON_02, 0x1C);
static DEFINE_STATIC_PACKET(unbound3_a3_s0_mcd_on_03, DSI_PKT_TYPE_WR, UNBOUND3_A3_S0_MCD_ON_03, 0x08);
static DEFINE_STATIC_PACKET(unbound3_a3_s0_mcd_on_04, DSI_PKT_TYPE_WR, UNBOUND3_A3_S0_MCD_ON_04, 0);

static DEFINE_STATIC_PACKET(unbound3_a3_s0_mcd_off_01, DSI_PKT_TYPE_WR, UNBOUND3_A3_S0_MCD_OFF_01, 0x0C);
static DEFINE_STATIC_PACKET(unbound3_a3_s0_mcd_off_02, DSI_PKT_TYPE_WR, UNBOUND3_A3_S0_MCD_OFF_02, 0x1C);
static DEFINE_STATIC_PACKET(unbound3_a3_s0_mcd_off_03, DSI_PKT_TYPE_WR, UNBOUND3_A3_S0_MCD_OFF_03, 0x08);
static DEFINE_STATIC_PACKET(unbound3_a3_s0_mcd_off_04, DSI_PKT_TYPE_WR, UNBOUND3_A3_S0_MCD_OFF_04, 0);

#ifdef CONFIG_SUPPORT_MST
static DEFINE_STATIC_PACKET(unbound3_a3_s0_mst_on_01, DSI_PKT_TYPE_WR, UNBOUND3_A3_S0_MST_ON_01, 0xFA);
static DEFINE_STATIC_PACKET(unbound3_a3_s0_mst_on_02, DSI_PKT_TYPE_WR, UNBOUND3_A3_S0_MST_ON_02, 0x1C);
static DEFINE_STATIC_PACKET(unbound3_a3_s0_mst_on_03, DSI_PKT_TYPE_WR, UNBOUND3_A3_S0_MST_ON_03, 0);

static DEFINE_STATIC_PACKET(unbound3_a3_s0_mst_off_01, DSI_PKT_TYPE_WR, UNBOUND3_A3_S0_MST_OFF_01, 0xFA);
static DEFINE_STATIC_PACKET(unbound3_a3_s0_mst_off_02, DSI_PKT_TYPE_WR, UNBOUND3_A3_S0_MST_OFF_02, 0x1C);
static DEFINE_STATIC_PACKET(unbound3_a3_s0_mst_off_03, DSI_PKT_TYPE_WR, UNBOUND3_A3_S0_MST_OFF_03, 0);

#endif

#ifdef CONFIG_SUPPORT_GRAYSPOT_TEST
static DEFINE_STATIC_PACKET(unbound3_a3_s0_grayspot_on_00, DSI_PKT_TYPE_WR, UNBOUND3_A3_S0_GRAYSPOT_ON_00, 0x0C);
static DEFINE_STATIC_PACKET(unbound3_a3_s0_grayspot_on_01, DSI_PKT_TYPE_WR, UNBOUND3_A3_S0_GRAYSPOT_ON_01, 0x1C);
static DEFINE_STATIC_PACKET(unbound3_a3_s0_grayspot_on_02, DSI_PKT_TYPE_WR, UNBOUND3_A3_S0_GRAYSPOT_ON_02, 0x0D);
static DEFINE_STATIC_PACKET(unbound3_a3_s0_grayspot_on_03, DSI_PKT_TYPE_WR, UNBOUND3_A3_S0_GRAYSPOT_ON_03, 0x14);

static DEFINE_STATIC_PACKET(unbound3_a3_s0_grayspot_off_00, DSI_PKT_TYPE_WR, UNBOUND3_A3_S0_GRAYSPOT_OFF_00, 0x0C);
static DEFINE_STATIC_PACKET(unbound3_a3_s0_grayspot_off_01, DSI_PKT_TYPE_WR, UNBOUND3_A3_S0_GRAYSPOT_OFF_01, 0x1C);
static DEFINE_PKTUI(unbound3_a3_s0_grayspot_cal, &unbound3_a3_s0_maptbl[GRAYSPOT_CAL_MAPTBL], 1);
static DEFINE_VARIABLE_PACKET(unbound3_a3_s0_grayspot_cal, DSI_PKT_TYPE_WR, UNBOUND3_A3_S0_GRAYSPOT_CAL, 0x14);
#endif

#ifdef CONFIG_SUPPORT_BRIGHTDOT_TEST
static u8 UNBOUND3_A3_S0_BRIGHTDOT_MAX_BRIGHTNESS[] = {
	0x51, 0x07, 0xFF
};
static DEFINE_STATIC_PACKET(unbound3_a3_s0_brightdot_max_brightness,
	DSI_PKT_TYPE_WR, UNBOUND3_A3_S0_BRIGHTDOT_MAX_BRIGHTNESS, 0);

static u8 UNBOUND3_A3_S0_BRIGHTDOT_AOR[] = {
	0xC7,
	0x0C, 0xB0, 0x02, 0x00, 0xC0
};

static DEFINE_STATIC_PACKET(unbound3_a3_s0_brightdot_aor, DSI_PKT_TYPE_WR, UNBOUND3_A3_S0_BRIGHTDOT_AOR, 0x54);

static u8 UNBOUND3_A3_S0_BRIGHTDOT_AOR_ENABLE[] = {
	0xC7,
	0x00,
};

static DEFINE_PKTUI(unbound3_a3_s0_brightdot_aor_enable, &unbound3_a3_s0_maptbl[BRIGHTDOT_AOR_MAPTBL], 1);
static DEFINE_VARIABLE_PACKET(unbound3_a3_s0_brightdot_aor_enable, DSI_PKT_TYPE_WR, UNBOUND3_A3_S0_BRIGHTDOT_AOR_ENABLE, 0x53);

static DEFINE_COND(unbound3_a3_s0_cond_is_brightdot_enabled, s6e3had_is_brightdot_enabled);
#endif

static DEFINE_PANEL_UDELAY(unbound3_a3_s0_wait_1usec, 1);
static DEFINE_PANEL_UDELAY(unbound3_a3_s0_wait_100usec, 100);
static DEFINE_PANEL_MDELAY(unbound3_a3_s0_wait_1msec, 1);
static DEFINE_PANEL_MDELAY(unbound3_a3_s0_wait_10msec, 10);
static DEFINE_PANEL_MDELAY(unbound3_a3_s0_wait_20msec, 20);
static DEFINE_PANEL_MDELAY(unbound3_a3_s0_wait_34msec, 34);
static DEFINE_PANEL_MDELAY(unbound3_a3_s0_wait_40msec, 40);
static DEFINE_PANEL_MDELAY(unbound3_a3_s0_wait_50msec, 50);
static DEFINE_PANEL_MDELAY(unbound3_a3_s0_wait_100msec, 100);
static DEFINE_PANEL_MDELAY(unbound3_a3_s0_wait_124msec, 124);
static DEFINE_PANEL_TIMER_MDELAY(unbound3_a3_s0_wait_sleep_out_120msec, 120);
static DEFINE_PANEL_TIMER_BEGIN(unbound3_a3_s0_wait_sleep_out_120msec,
		TIMER_DLYINFO(&unbound3_a3_s0_wait_sleep_out_120msec));

#ifdef CONFIG_SUPPORT_AFC
static DEFINE_PANEL_MDELAY(unbound3_a3_s0_wait_afc_off, 20);
#endif
static DEFINE_PANEL_MDELAY(unbound3_a3_s0_wait_sleep_in, 120);
static DEFINE_PANEL_VSYNC_DELAY(unbound3_a3_s0_wait_1_vsync, 1);
static DEFINE_PANEL_FRAME_DELAY(unbound3_a3_s0_wait_1_frame, 1);

#ifdef CONFIG_SUPPORT_GRAM_CHECKSUM
static DEFINE_PANEL_MDELAY(unbound3_a3_s0_wait_120msec, 120);
static DEFINE_PANEL_MDELAY(unbound3_a3_s0_wait_vddm_update, 50);
static DEFINE_PANEL_MDELAY(unbound3_a3_s0_wait_20msec_gram_img_update, 20);
static DEFINE_PANEL_MDELAY(unbound3_a3_s0_wait_gram_img_update, 150);
#endif

static DEFINE_PANEL_KEY(unbound3_a3_s0_level1_key_enable, CMD_LEVEL_1, KEY_ENABLE, &PKTINFO(unbound3_a3_s0_level1_key_enable));
static DEFINE_PANEL_KEY(unbound3_a3_s0_level2_key_enable, CMD_LEVEL_2, KEY_ENABLE, &PKTINFO(unbound3_a3_s0_level2_key_enable));
static DEFINE_PANEL_KEY(unbound3_a3_s0_level3_key_enable, CMD_LEVEL_3, KEY_ENABLE, &PKTINFO(unbound3_a3_s0_level3_key_enable));
static DEFINE_PANEL_KEY(unbound3_a3_s0_level1_key_disable, CMD_LEVEL_1, KEY_DISABLE, &PKTINFO(unbound3_a3_s0_level1_key_disable));
static DEFINE_PANEL_KEY(unbound3_a3_s0_level2_key_disable, CMD_LEVEL_2, KEY_DISABLE, &PKTINFO(unbound3_a3_s0_level2_key_disable));
static DEFINE_PANEL_KEY(unbound3_a3_s0_level3_key_disable, CMD_LEVEL_3, KEY_DISABLE, &PKTINFO(unbound3_a3_s0_level3_key_disable));

static u8 UNBOUND3_A3_S0_F1_KEY_ENABLE[] = {
	0xF1, 0xF1, 0xA2,
};
static u8 UNBOUND3_A3_S0_F1_KEY_DISABLE[] = {
	0xF1, 0xA5, 0xA5,
};
static DEFINE_STATIC_PACKET(unbound3_a3_s0_f1_key_enable, DSI_PKT_TYPE_WR, UNBOUND3_A3_S0_F1_KEY_ENABLE, 0);
static DEFINE_STATIC_PACKET(unbound3_a3_s0_f1_key_disable, DSI_PKT_TYPE_WR, UNBOUND3_A3_S0_F1_KEY_DISABLE, 0);

#if defined(__PANEL_NOT_USED_VARIABLE__)
static u8 UNBOUND3_A3_S0_OSC[] = {
	0xF2,
	0xC3, 0xB4, 0x04, 0x14, 0x01, 0x20, 0x00, 0x10,
	0x10, 0x00, 0x14, 0xBC, 0x20, 0x00, 0x30, 0x0F,
	0xE0
};
static DECLARE_PKTUI(unbound3_a3_s0_osc) = {
	{ .offset = 1, .maptbl = &unbound3_a3_s0_maptbl[OSC_MAPTBL] },
	{ .offset = 1 + S6E3HAD_VFP_NS_OFS, .maptbl = &unbound3_a3_s0_maptbl[VFP_NS_MAPTBL] },
	{ .offset = 1 + S6E3HAD_VFP_HS_OFS, .maptbl = &unbound3_a3_s0_maptbl[VFP_HS_MAPTBL] },
};
static DEFINE_VARIABLE_PACKET(unbound3_a3_s0_osc, DSI_PKT_TYPE_WR, UNBOUND3_A3_S0_OSC, 0);
#else
static u8 UNBOUND3_A3_S0_OSC_1[] = {
	0xF2, 0x50
};
static DEFINE_PKTUI(unbound3_a3_s0_osc_1, &unbound3_a3_s0_maptbl[OSC_1_MAPTBL], 1);
static DEFINE_VARIABLE_PACKET(unbound3_a3_s0_osc_1, DSI_PKT_TYPE_WR, UNBOUND3_A3_S0_OSC_1, 0x03);

static u8 UNBOUND3_A3_S0_OSC_2[] = {
	0xF2, 0x80
};
static DEFINE_PKTUI(unbound3_a3_s0_osc_2, &unbound3_a3_s0_maptbl[OSC_2_MAPTBL], 1);
static DEFINE_VARIABLE_PACKET(unbound3_a3_s0_osc_2, DSI_PKT_TYPE_WR, UNBOUND3_A3_S0_OSC_2, 0x44);
#endif
static u8 UNBOUND3_A3_S0_OSC_SEL_LT_E4[] = {
	0x6A,
	0x00,
};
static DEFINE_PKTUI(unbound3_a3_s0_osc_sel_lt_e4, &unbound3_a3_s0_maptbl[OSC_SEL_MAPTBL], 1);
static DEFINE_VARIABLE_PACKET(unbound3_a3_s0_osc_sel_lt_e4, DSI_PKT_TYPE_WR, UNBOUND3_A3_S0_OSC_SEL_LT_E4, 0x34);

static u8 UNBOUND3_A3_S0_VFP_HS[] = { 0xF2, 0x00, 0x10, };
static DEFINE_PKTUI(unbound3_a3_s0_vfp_hs, &unbound3_a3_s0_maptbl[VFP_HS_MAPTBL], 1);
static DEFINE_VARIABLE_PACKET(unbound3_a3_s0_vfp_hs, DSI_PKT_TYPE_WR, UNBOUND3_A3_S0_VFP_HS, S6E3HAD_VFP_HS_OFS);

static u8 UNBOUND3_A3_S0_VFP_NS[] = { 0xF2, 0x00, 0x10, };
static DEFINE_PKTUI(unbound3_a3_s0_vfp_ns, &unbound3_a3_s0_maptbl[VFP_NS_MAPTBL], 1);
static DEFINE_VARIABLE_PACKET(unbound3_a3_s0_vfp_ns, DSI_PKT_TYPE_WR, UNBOUND3_A3_S0_VFP_NS, S6E3HAD_VFP_NS_OFS);

static u8 UNBOUND3_A3_S0_OSC_SEL[] = {
	0x6A,
	0x00,
};
static DEFINE_STATIC_PACKET(unbound3_a3_s0_osc_sel, DSI_PKT_TYPE_WR, UNBOUND3_A3_S0_OSC_SEL, 0x34);

static u8 UNBOUND3_A3_S0_LFD_FRAME_SEL[] = {
	0xBD,
	0x00, 0x2C, 0x14, 0x00, 0x01
};
static DECLARE_PKTUI(unbound3_a3_s0_lfd_frame_sel) = {
	{ .offset = 1, .maptbl = &unbound3_a3_s0_maptbl[LFD_MIN_MAPTBL] },
	{ .offset = 3, .maptbl = &unbound3_a3_s0_maptbl[LFD_FRAME_INSERTION_MAPTBL] },
};
static DEFINE_VARIABLE_PACKET(unbound3_a3_s0_lfd_frame_sel, DSI_PKT_TYPE_WR, UNBOUND3_A3_S0_LFD_FRAME_SEL, 0x06);

static u8 UNBOUND3_A3_S0_HBM_TRANSITION[] = {
	0x53, 0x28
};

static DEFINE_PKTUI(unbound3_a3_s0_hbm_transition, &unbound3_a3_s0_maptbl[HBM_ONOFF_MAPTBL], 1);
static DEFINE_VARIABLE_PACKET(unbound3_a3_s0_hbm_transition, DSI_PKT_TYPE_WR, UNBOUND3_A3_S0_HBM_TRANSITION, 0);

static u8 UNBOUND3_A3_S0_HBM_TRANSITION_LT_E5[] = {
	0x53, 0x28
};
static DEFINE_PKTUI(unbound3_a3_s0_hbm_transition_lt_e5, &unbound3_a3_s0_maptbl[HBM_ONOFF_LT_E5_MAPTBL], 1);
static DEFINE_VARIABLE_PACKET(unbound3_a3_s0_hbm_transition_lt_e5, DSI_PKT_TYPE_WR, UNBOUND3_A3_S0_HBM_TRANSITION_LT_E5, 0);

static u8 UNBOUND3_A3_S0_ACL[] = {
	0x55, 0x02
};

static DEFINE_PKTUI(unbound3_a3_s0_acl_control, &unbound3_a3_s0_maptbl[ACL_OPR_MAPTBL], 1);
static DEFINE_VARIABLE_PACKET(unbound3_a3_s0_acl_control, DSI_PKT_TYPE_WR, UNBOUND3_A3_S0_ACL, 0);

static u8 UNBOUND3_A3_S0_ACL_SETTING_1[] = {
	0xC7,
	0x55, 0x80, 0x01, 0x29, 0x4D, 0x9B
};
static DEFINE_PKTUI(unbound3_a3_s0_acl_setting_1, &unbound3_a3_s0_maptbl[ACL_START_POINT_MAPTBL], 2);
static DEFINE_VARIABLE_PACKET(unbound3_a3_s0_acl_setting_1, DSI_PKT_TYPE_WR, UNBOUND3_A3_S0_ACL_SETTING_1, 0x291);

static u8 UNBOUND3_A3_S0_ACL_SETTING_2[] = {
	0xC7,
	0x00, 0x00, 0x1F, 0x00
};
static DEFINE_STATIC_PACKET(unbound3_a3_s0_acl_setting_2, DSI_PKT_TYPE_WR, UNBOUND3_A3_S0_ACL_SETTING_2, 0x29F);

static u8 UNBOUND3_A3_S0_WRDISBV[] = {
	0x51, 0x03, 0xFF
};
static DEFINE_PKTUI(unbound3_a3_s0_wrdisbv, &unbound3_a3_s0_maptbl[GAMMA_MODE2_MAPTBL], 1);
static DEFINE_VARIABLE_PACKET(unbound3_a3_s0_wrdisbv, DSI_PKT_TYPE_WR, UNBOUND3_A3_S0_WRDISBV, 0);

static u8 UNBOUND3_A3_S0_WRDISBV_EXIT_LPM[] = {
	0x51, 0x03, 0xFF
};
static DEFINE_PKTUI(unbound3_a3_s0_wrdisbv_exit_lpm, &unbound3_a3_s0_maptbl[GAMMA_MODE2_MAPTBL], 1);
static DEFINE_VARIABLE_PACKET(unbound3_a3_s0_wrdisbv_exit_lpm, DSI_PKT_TYPE_WR, UNBOUND3_A3_S0_WRDISBV_EXIT_LPM, 0);

static u8 UNBOUND3_A3_S0_LPM_OFF_WRDISBV[] = {
	0x51, 0x03, 0xFF
};
static DEFINE_STATIC_PACKET(unbound3_a3_s0_lpm_off_wrdisbv, DSI_PKT_TYPE_WR, UNBOUND3_A3_S0_LPM_OFF_WRDISBV, 0);

static u8 UNBOUND3_A3_S0_LPM_OFF_TRANSITION[] = {
	0x53, 0x29
};
static DEFINE_STATIC_PACKET(unbound3_a3_s0_lpm_off_transition, DSI_PKT_TYPE_WR, UNBOUND3_A3_S0_LPM_OFF_TRANSITION, 0);

static DEFINE_COND(unbound3_a3_s0_cond_is_panel_state_not_lpm, is_panel_state_not_lpm);

#ifdef CONFIG_SUPPORT_HMD
static u8 UNBOUND3_A3_S0_HMD_WRDISBV[] = {
	0x51, 0x03, 0xFF
};
static DEFINE_PKTUI(unbound3_a3_s0_hmd_wrdisbv, &unbound3_a3_s0_maptbl[HMD_GAMMA_MAPTBL], 1);
static DEFINE_VARIABLE_PACKET(unbound3_a3_s0_hmd_wrdisbv, DSI_PKT_TYPE_WR, UNBOUND3_A3_S0_HMD_WRDISBV, 0);


static u8 UNBOUND3_A3_S0_HMD_ON_AID[] = {
	0xC7,
	0x01, 0x0A, 0xA8, 0x00, 0x00, 0xC0
};
static DEFINE_STATIC_PACKET(unbound3_a3_s0_hmd_on_aid, DSI_PKT_TYPE_WR, UNBOUND3_A3_S0_HMD_ON_AID, 0x53);

static u8 UNBOUND3_A3_S0_HMD_OFF_AID[] = {
	0xC7, 0x00
};
static DEFINE_STATIC_PACKET(unbound3_a3_s0_hmd_off_aid, DSI_PKT_TYPE_WR, UNBOUND3_A3_S0_HMD_OFF_AID, 0x53);

#endif


#ifdef CONFIG_DYNAMIC_MIPI
static DEFINE_PKTUI(unbound3_a3_s0_set_ffc, &unbound3_a3_s0_maptbl[DM_SET_FFC_MAPTBL], 0);
static DEFINE_VARIABLE_PACKET(unbound3_a3_s0_set_ffc, DSI_PKT_TYPE_WR, UNBOUND3_A3_S0_FFC_DEFAULT, 0x12);
#else
static DEFINE_STATIC_PACKET(unbound3_a3_s0_set_ffc, DSI_PKT_TYPE_WR, UNBOUND3_A3_S0_FFC_DEFAULT, 0x12);
#endif

static char UNBOUND3_A3_S0_OFF_FFC[] = {
	0xC5,
	0x11, 0x10, 0x50, 0x05, 0x49, 0x23 /* default 1443 */
};
static DEFINE_STATIC_PACKET(unbound3_a3_s0_off_ffc, DSI_PKT_TYPE_WR, UNBOUND3_A3_S0_OFF_FFC, 0x12);

static void *unbound3_a3_s0_dm_set_ffc_cmdtbl[] = {
	&KEYINFO(unbound3_a3_s0_level3_key_enable),
	&PKTINFO(unbound3_a3_s0_set_ffc),
	&KEYINFO(unbound3_a3_s0_level3_key_disable),
};

static void *unbound3_a3_s0_dm_off_ffc_cmdtbl[] = {
	&KEYINFO(unbound3_a3_s0_level3_key_enable),
	&PKTINFO(unbound3_a3_s0_off_ffc),
	&KEYINFO(unbound3_a3_s0_level3_key_disable),
};

static DEFINE_COND(unbound3_a3_s0_cond_is_id_gte_e2, is_id_gte_e2);
static DEFINE_COND(unbound3_a3_s0_cond_is_id_gte_e3, is_id_gte_e3);
static DEFINE_COND(unbound3_a3_s0_cond_is_id_gte_e4, is_id_gte_e4);
static DEFINE_COND(unbound3_a3_s0_cond_is_id_gte_e5, is_id_gte_e5);
static DEFINE_COND(unbound3_a3_s0_cond_is_id_gte_e7, is_id_gte_e7);
static DEFINE_COND(unbound3_a3_s0_cond_is_first_set_bl, is_first_set_bl);
static DEFINE_COND(unbound3_a3_s0_cond_is_wait_vsync_needed, is_wait_vsync_needed);
static DEFINE_COND(unbound3_a3_s0_cond_is_vrr_96hs_mode, is_vrr_96hs_mode);
static DEFINE_COND(unbound3_a3_s0_cond_is_vrr_96hs_hbm_enter, is_vrr_96hs_hbm_enter);
static DEFINE_COND(unbound3_a3_s0_cond_is_gamma_select_off_brt, is_gamma_select_off_brt);

#ifdef CONFIG_SUPPORT_MAFPC

static u8 UNBOUND3_A3_S0_MAFPC_SR_PATH[] = {
	0x75, 0x40,
};
static DEFINE_STATIC_PACKET(unbound3_a3_s0_mafpc_sr_path, DSI_PKT_TYPE_WR, UNBOUND3_A3_S0_MAFPC_SR_PATH, 0);


static u8 UNBOUND3_A3_S0_DEFAULT_SR_PATH[] = {
	0x75, 0x01,
};
static DEFINE_STATIC_PACKET(unbound3_a3_s0_default_sr_path, DSI_PKT_TYPE_WR, UNBOUND3_A3_S0_DEFAULT_SR_PATH, 0);

static u8 UNBOUND3_A3_S0_MAFPC_MEMORY_SIZE[] = {
	0xFE,
	0x5A, 0x81, 0x19, 0x40,
};
static DEFINE_STATIC_PACKET(unbound3_a3_s0_mafpc_memory_size, DSI_PKT_TYPE_WR, UNBOUND3_A3_S0_MAFPC_MEMORY_SIZE, 0x67);

static DEFINE_STATIC_PACKET_WITH_OPTION(unbound3_mafpc_default_img, DSI_PKT_TYPE_WR_SR,
	S6E3HAD_MAFPC_DEFAULT_IMG, 0, PKT_OPTION_SR_ALIGN_12);

static u8 UNBOUND3_A3_S0_MAFPC_DISABLE[] = {
	0x87, 0x00,
};
static DEFINE_STATIC_PACKET(unbound3_a3_s0_mafpc_disable, DSI_PKT_TYPE_WR, UNBOUND3_A3_S0_MAFPC_DISABLE, 0);


static u8 UNBOUND3_A3_S0_MAFPC_CRC_ON[] = {
	0xD8, 0x15,
};
static DEFINE_STATIC_PACKET(unbound3_a3_s0_mafpc_crc_on, DSI_PKT_TYPE_WR, UNBOUND3_A3_S0_MAFPC_CRC_ON, 0x27);

static u8 UNBOUND3_A3_S0_MAFPC_CRC_BIST_ON[] = {
	0xBF,
	0x01, 0x07, 0xFF, 0x00, 0x00, 0x10, 0x00, 0x00, 0x00
};
static DEFINE_STATIC_PACKET(unbound3_a3_s0_mafpc_crc_bist_on, DSI_PKT_TYPE_WR, UNBOUND3_A3_S0_MAFPC_CRC_BIST_ON, 0x00);


static u8 UNBOUND3_A3_S0_MAFPC_CRC_BIST_OFF[] = {
	0xBF,
	0x00, 0x07, 0xFF, 0x00, 0x00, 0x10, 0x00, 0x00, 0x00
};
static DEFINE_STATIC_PACKET(unbound3_a3_s0_mafpc_crc_bist_off, DSI_PKT_TYPE_WR, UNBOUND3_A3_S0_MAFPC_CRC_BIST_OFF, 0x00);


static u8 UNBOUND3_A3_S0_MAFPC_CRC_ENABLE[] = {
	0x87,
	0x51, 0x09, 0x20, 0x04, 0x00, 0x00, 0x66, 0x66,
	0x66, 0xff, 0xff, 0xff, 0x33, 0x33, 0x33, 0x33,
	0x33, 0x33, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
	0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
	0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
	0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
	0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
	0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
	0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
};
static DEFINE_STATIC_PACKET(unbound3_a3_s0_mafpc_crc_enable, DSI_PKT_TYPE_WR, UNBOUND3_A3_S0_MAFPC_CRC_ENABLE, 0);

static u8 UNBOUND3_A3_S0_MAFPC_CRC_ENABLE_LT_E2[] = {
	0x87,
	0x51, 0x09, 0x1f, 0x04, 0x00, 0x00, 0x66, 0x66,
	0x66, 0xff, 0xff, 0xff, 0x33, 0x33, 0x33, 0x33,
	0x33, 0x33, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
	0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
	0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
	0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
	0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
	0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
	0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
};
static DEFINE_STATIC_PACKET(unbound3_a3_s0_mafpc_crc_enable_lt_e2, DSI_PKT_TYPE_WR, UNBOUND3_A3_S0_MAFPC_CRC_ENABLE_LT_E2, 0);

static u8 UNBOUND3_A3_S0_MAFPC_LUMINANCE_UPDATE[] = {
	0xF7, 0x01
};
static DEFINE_STATIC_PACKET(unbound3_a3_s0_mafpc_luminance_update, DSI_PKT_TYPE_WR, UNBOUND3_A3_S0_MAFPC_LUMINANCE_UPDATE, 0);

static u8 UNBOUND3_A3_S0_MAFPC_CRC_MDNIE_OFF[] = {
	0xDD,
	0x00
};
static DEFINE_STATIC_PACKET(unbound3_a3_s0_mafpc_crc_mdnie_off, DSI_PKT_TYPE_WR, UNBOUND3_A3_S0_MAFPC_CRC_MDNIE_OFF, 0);


static u8 UNBOUND3_A3_S0_MAFPC_CRC_MDNIE_ON[] = {
	0xDD,
	0x01
};
static DEFINE_STATIC_PACKET(unbound3_a3_s0_mafpc_crc_mdnie_on, DSI_PKT_TYPE_WR, UNBOUND3_A3_S0_MAFPC_CRC_MDNIE_ON, 0);

static u8 UNBOUND3_A3_S0_MAFPC_SELF_MASK_OFF[] = {
	0x7A,
	0x00
};
static DEFINE_STATIC_PACKET(unbound3_a3_s0_mafpc_self_mask_off, DSI_PKT_TYPE_WR, UNBOUND3_A3_S0_MAFPC_SELF_MASK_OFF, 0);

static u8 UNBOUND3_A3_S0_MAFPC_SELF_MASK_ON[] = {
	0x7A,
	0x21
};
static DEFINE_STATIC_PACKET(unbound3_a3_s0_mafpc_self_mask_on, DSI_PKT_TYPE_WR, UNBOUND3_A3_S0_MAFPC_SELF_MASK_ON, 0);

static DEFINE_PANEL_MDELAY(unbound3_a3_s0_crc_wait, 34);


static void *unbound3_a3_s0_mafpc_image_cmdtbl[] = {
	&KEYINFO(unbound3_a3_s0_level2_key_enable),
	&KEYINFO(unbound3_a3_s0_level3_key_enable),
	&PKTINFO(unbound3_a3_s0_mafpc_sr_path),
	&DLYINFO(unbound3_a3_s0_wait_1usec),
	&PKTINFO(unbound3_a3_s0_mafpc_disable),
	&DLYINFO(unbound3_a3_s0_wait_1_frame),
	&PKTINFO(unbound3_a3_s0_mafpc_memory_size),
	&PKTINFO(unbound3_mafpc_default_img),
	&DLYINFO(unbound3_a3_s0_wait_100usec),
	&PKTINFO(unbound3_a3_s0_default_sr_path),
	&KEYINFO(unbound3_a3_s0_level3_key_disable),
	&KEYINFO(unbound3_a3_s0_level2_key_disable),
};

static void *unbound3_a3_s0_mafpc_check_cmdtbl[] = {
	&KEYINFO(unbound3_a3_s0_level1_key_enable),
	&KEYINFO(unbound3_a3_s0_level2_key_enable),
	&KEYINFO(unbound3_a3_s0_level3_key_enable),

	&PKTINFO(unbound3_a3_s0_dummy_2c),
	&PKTINFO(unbound3_a3_s0_mafpc_crc_on),
	&PKTINFO(unbound3_a3_s0_mafpc_crc_bist_on),
	&CONDINFO_IF(unbound3_a3_s0_cond_is_id_gte_e2),
		&PKTINFO(unbound3_a3_s0_mafpc_crc_enable),
	&CONDINFO_EL(unbound3_a3_s0_cond_is_id_gte_e2),
		&PKTINFO(unbound3_a3_s0_mafpc_crc_enable_lt_e2),
	&CONDINFO_FI(unbound3_a3_s0_cond_is_id_gte_e2),
	&PKTINFO(unbound3_a3_s0_mafpc_luminance_update),
	&PKTINFO(unbound3_a3_s0_mafpc_self_mask_off),
	&PKTINFO(unbound3_a3_s0_mafpc_crc_mdnie_off),
	&DLYINFO(unbound3_a3_s0_crc_wait),
	&s6e3had_restbl[RES_MAFPC_CRC],
	&PKTINFO(unbound3_a3_s0_mafpc_crc_bist_off),
	&PKTINFO(unbound3_a3_s0_mafpc_disable),
	&PKTINFO(unbound3_a3_s0_mafpc_self_mask_on),
	&PKTINFO(unbound3_a3_s0_mafpc_crc_mdnie_on),
	&KEYINFO(unbound3_a3_s0_level3_key_disable),
	&KEYINFO(unbound3_a3_s0_level2_key_disable),
	&KEYINFO(unbound3_a3_s0_level1_key_disable),
};

static u8 UNBOUND3_A3_S0_MAFPC_ENABLE[] = {
	0x87,
	0x00, 0x09, 0x20, 0x04, 0x00, 0x00, 0x66, 0x66,
	0x66, 0xff, 0xff, 0xff, 0x33, 0x33, 0x33, 0x33,
	0x33, 0x33, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
	0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
	0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
	0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
	0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
	0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
	0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff
};
static DEFINE_PKTUI(unbound3_a3_s0_mafpc_enable, &unbound3_a3_s0_maptbl[MAFPC_ENA_MAPTBL], 1);
static DEFINE_VARIABLE_PACKET(unbound3_a3_s0_mafpc_enable, DSI_PKT_TYPE_WR, UNBOUND3_A3_S0_MAFPC_ENABLE, 0);


static u8 UNBOUND3_A3_S0_MAFPC_SCALE_GPARAM[] = {
	0xB0,
	0x00, 0x09, 0x87,
};
static DEFINE_STATIC_PACKET(unbound3_a3_s0_mafpc_scale_gparam, DSI_PKT_TYPE_WR, UNBOUND3_A3_S0_MAFPC_SCALE_GPARAM, 0);


static u8 UNBOUND3_A3_S0_MAFPC_SCALE[] = {
	0x87,
	0xFF, 0xFF, 0xFF,
};
static DEFINE_PKTUI(unbound3_a3_s0_mafpc_scale, &unbound3_a3_s0_maptbl[MAFPC_SCALE_MAPTBL], 1);
static DEFINE_VARIABLE_PACKET(unbound3_a3_s0_mafpc_scale, DSI_PKT_TYPE_WR, UNBOUND3_A3_S0_MAFPC_SCALE, 0);

static void *unbound3_a3_s0_mafpc_on_cmdtbl[] = {
	&KEYINFO(unbound3_a3_s0_level2_key_enable),
	&PKTINFO(unbound3_a3_s0_mafpc_enable),
	&PKTINFO(unbound3_a3_s0_mafpc_scale_gparam),
	&PKTINFO(unbound3_a3_s0_mafpc_scale),
	&PKTINFO(unbound3_a3_s0_mafpc_luminance_update),
	&KEYINFO(unbound3_a3_s0_level2_key_disable),
};

static void *unbound3_a3_s0_mafpc_off_cmdtbl[] = {
	&KEYINFO(unbound3_a3_s0_level2_key_enable),
	&PKTINFO(unbound3_a3_s0_mafpc_disable),
	&KEYINFO(unbound3_a3_s0_level2_key_disable),
};

#endif

static struct seqinfo SEQINFO(unbound3_a3_s0_set_bl_param_seq);
static struct seqinfo SEQINFO(unbound3_a3_s0_set_fps_param_seq);

static void *unbound3_a3_s0_err_fg_cmdtbl[] = {
	&PKTINFO(unbound3_a3_s0_err_fg_1),
	&PKTINFO(unbound3_a3_s0_err_fg_2),
};
static DEFINE_SEQINFO(unbound3_a3_s0_err_fg_seq, unbound3_a3_s0_err_fg_cmdtbl);

static u8 UNBOUND3_A3_S0_GR_GRID_ON[] = {
	0x7C,
	0x00, 0x01, 0x00, 0x1F, 0x00, 0x00, 0x00, 0x00, 0x05, 0x9F, 0x0C, 0x7F
};

static u8 UNBOUND3_A3_S0_FR_C0_02[] = {
	0xC0, 0x02
};
static DEFINE_STATIC_PACKET(unbound3_a3_s0_fr_c0_02, DSI_PKT_TYPE_WR, UNBOUND3_A3_S0_FR_C0_02, 0);

static u8 UNBOUND3_A3_S0_FR_C0_00[] = {
	0xC0, 0x00
};
static DEFINE_STATIC_PACKET(unbound3_a3_s0_fr_c0_00, DSI_PKT_TYPE_WR, UNBOUND3_A3_S0_FR_C0_00, 0);

static u8 UNBOUND3_A3_S0_FR_71[] = {
	0x71,
	0x03, 0x00, 0x00
};
static DEFINE_STATIC_PACKET(unbound3_a3_s0_fr_71, DSI_PKT_TYPE_WR, UNBOUND3_A3_S0_FR_71, 0);

static u8 UNBOUND3_A3_S0_FR_EXECUTE[] = {
	0xC0, 0x03
};
static DEFINE_STATIC_PACKET(unbound3_a3_s0_fr_execute, DSI_PKT_TYPE_WR, UNBOUND3_A3_S0_FR_EXECUTE, 0);

static u8 UNBOUND3_A3_S0_FR_WRITE_ENABLE[] = {
	0xC1,
	0x00, 0x00, 0x06, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00
};
static DEFINE_STATIC_PACKET(unbound3_a3_s0_fr_write_enable, DSI_PKT_TYPE_WR, UNBOUND3_A3_S0_FR_WRITE_ENABLE, 0);

static u8 UNBOUND3_A3_S0_FR_STATUS_REG_CHANGE[] = {
	0xC1,
	0x00, 0x00, 0x01, 0x5C, 0x02, 0x00, 0x80, 0x00,
	0x00
};
static DEFINE_STATIC_PACKET(unbound3_a3_s0_fr_status_reg_change, DSI_PKT_TYPE_WR, UNBOUND3_A3_S0_FR_STATUS_REG_CHANGE, 0);

static u8 UNBOUND3_A3_S0_FR_READ_HBM_ADDR[] = {
	0xC1,
	0x00, 0x00, 0x6B, 0x0E, 0x16, 0x04, 0xC0, 0x00,
	0x46
};
static DEFINE_PKTUI(unbound3_a3_s0_fr_read_hbm_addr, &unbound3_a3_s0_maptbl[HBM_ADDR_MAPTBL], 4);
static DEFINE_VARIABLE_PACKET(unbound3_a3_s0_fr_read_hbm_addr, DSI_PKT_TYPE_WR, UNBOUND3_A3_S0_FR_READ_HBM_ADDR, 0);

static u8 UNBOUND3_A3_S0_FR_READ_NORMAL_ADDR[] = {
	0xC1,
	0x00, 0x00, 0x6B, 0x0E, 0x15, 0xBE, 0xC0, 0x00,
	0x46
};
static DEFINE_PKTUI(unbound3_a3_s0_fr_read_normal_addr, &unbound3_a3_s0_maptbl[NORMAL_ADDR_MAPTBL], 4);
static DEFINE_VARIABLE_PACKET(unbound3_a3_s0_fr_read_normal_addr, DSI_PKT_TYPE_WR, UNBOUND3_A3_S0_FR_READ_NORMAL_ADDR, 0);


static void *unbound3_a3_s0_init_cmdtbl[] = {
	&DLYINFO(unbound3_a3_s0_wait_10msec),
	&KEYINFO(unbound3_a3_s0_level1_key_enable),
	&KEYINFO(unbound3_a3_s0_level2_key_enable),
	&PKTINFO(unbound3_a3_s0_dsc),
	&PKTINFO(unbound3_a3_s0_pps),
	&KEYINFO(unbound3_a3_s0_level3_key_enable),
	&PKTINFO(unbound3_a3_s0_sleep_out),
	&DLYINFO(unbound3_a3_s0_wait_10msec),
	&TIMER_DLYINFO_BEGIN(unbound3_a3_s0_wait_sleep_out_120msec),
#ifdef CONFIG_SUPPORT_MAFPC
	&PKTINFO(unbound3_a3_s0_mafpc_sr_path),
	&DLYINFO(unbound3_a3_s0_wait_1usec),
	&PKTINFO(unbound3_a3_s0_mafpc_disable),
	&DLYINFO(unbound3_a3_s0_wait_1_frame),
	&PKTINFO(unbound3_a3_s0_mafpc_memory_size),
	&PKTINFO(unbound3_mafpc_default_img),
	&DLYINFO(unbound3_a3_s0_wait_100usec),
	&PKTINFO(unbound3_a3_s0_default_sr_path),
#endif
	&TIMER_DLYINFO(unbound3_a3_s0_wait_sleep_out_120msec),
	&PKTINFO(unbound3_a3_s0_caset),
	&PKTINFO(unbound3_a3_s0_paset),
	&PKTINFO(unbound3_a3_s0_scaler),
	&SEQINFO(unbound3_a3_s0_err_fg_seq),
	&PKTINFO(unbound3_a3_s0_charge_pump),
	&PKTINFO(unbound3_a3_s0_wacom_sync_1),
	&PKTINFO(unbound3_a3_s0_wacom_sync_2),
	&PKTINFO(unbound3_a3_s0_smooth_dimming_init),
	&PKTINFO(unbound3_a3_s0_dia_setting),
	&PKTINFO(unbound3_a3_s0_dia_onoff),
	&PKTINFO(unbound3_a3_s0_set_ffc),
	&PKTINFO(unbound3_a3_s0_mca_set_1),
	&PKTINFO(unbound3_a3_s0_mca_set_2),

	/* set brightness & fps */
	&SEQINFO(unbound3_a3_s0_set_bl_param_seq),
	&SEQINFO(unbound3_a3_s0_set_fps_param_seq),
	&PKTINFO(unbound3_a3_s0_gamma_update_enable),

	&PKTINFO(unbound3_a3_s0_te_on),
	&PKTINFO(unbound3_a3_s0_tsp_hsync),

	&KEYINFO(unbound3_a3_s0_level3_key_disable),
	&KEYINFO(unbound3_a3_s0_level2_key_disable),
	&KEYINFO(unbound3_a3_s0_level1_key_disable),
#ifdef CONFIG_EXYNOS_DECON_LCD_COPR
	&SEQINFO(unbound3_a3_s0_copr_seqtbl[COPR_SET_SEQ]),
#endif
};

static void *unbound3_a3_s0_res_gamma_init_cmdtbl[] = {
	&PKTINFO(unbound3_a3_s0_f1_key_enable),
	&PKTINFO(unbound3_a3_s0_fr_c0_02),
	&PKTINFO(unbound3_a3_s0_fr_71),
	&DLYINFO(unbound3_a3_s0_wait_1msec),

	&PKTINFO(unbound3_a3_s0_fr_write_enable),
	&PKTINFO(unbound3_a3_s0_fr_execute),
	&DLYINFO(unbound3_a3_s0_wait_1msec),

	&PKTINFO(unbound3_a3_s0_fr_status_reg_change),
	&PKTINFO(unbound3_a3_s0_fr_execute),
	&DLYINFO(unbound3_a3_s0_wait_10msec),

	&PKTINFO(unbound3_a3_s0_fr_read_hbm_addr),
	&PKTINFO(unbound3_a3_s0_fr_execute),
	&DLYINFO(unbound3_a3_s0_wait_10msec),
	&s6e3had_restbl[RES_GAMMA_FLASH_120HS_HBM],

	&PKTINFO(unbound3_a3_s0_fr_read_normal_addr),
	&PKTINFO(unbound3_a3_s0_fr_execute),
	&DLYINFO(unbound3_a3_s0_wait_10msec),
	&s6e3had_restbl[RES_GAMMA_FLASH_120HS],

	&PKTINFO(unbound3_a3_s0_fr_c0_00),
	&PKTINFO(unbound3_a3_s0_f1_key_disable),
	&unbound3_a3_s0_maptbl[GAMMA_WRITE_0_MAPTBL],
	&unbound3_a3_s0_maptbl[GAMMA_WRITE_1_MAPTBL],
};
static DEFINE_SEQINFO(unbound3_a3_s0_res_gamma_init_seq, unbound3_a3_s0_res_gamma_init_cmdtbl);

static void *unbound3_a3_s0_id_read_cmdtbl[] = {
	&KEYINFO(unbound3_a3_s0_level1_key_enable),
	&KEYINFO(unbound3_a3_s0_level2_key_enable),
	&KEYINFO(unbound3_a3_s0_level3_key_enable),
	&s6e3had_restbl[RES_ID],
	&KEYINFO(unbound3_a3_s0_level3_key_disable),
	&KEYINFO(unbound3_a3_s0_level2_key_disable),
	&KEYINFO(unbound3_a3_s0_level1_key_disable),
};

static void *unbound3_a3_s0_res_init_cmdtbl[] = {
	&KEYINFO(unbound3_a3_s0_level1_key_enable),
	&KEYINFO(unbound3_a3_s0_level2_key_enable),
	&KEYINFO(unbound3_a3_s0_level3_key_enable),
	&s6e3had_restbl[RES_COORDINATE],
	&s6e3had_restbl[RES_CODE],
	&s6e3had_restbl[RES_DATE],
	&s6e3had_restbl[RES_OCTA_ID],
#ifdef CONFIG_SUPPORT_GRAYSPOT_TEST
	&s6e3had_restbl[RES_GRAYSPOT_CAL],
#endif
#ifdef CONFIG_DISPLAY_USE_INFO
	&s6e3had_restbl[RES_CHIP_ID],
	&s6e3had_restbl[RES_SELF_DIAG],
	&s6e3had_restbl[RES_ERR_FG],
	&s6e3had_restbl[RES_DSI_ERR],
#endif
	&s6e3had_restbl[RES_VAINT],
#ifdef CONFIG_SUPPORT_DDI_FLASH
	&s6e3had_restbl[RES_POC_CHKSUM],
#endif
	&s6e3had_restbl[RES_FLASH_LOADED],
	&SEQINFO(unbound3_a3_s0_res_gamma_init_seq),
	&KEYINFO(unbound3_a3_s0_level3_key_disable),
	&KEYINFO(unbound3_a3_s0_level2_key_disable),
	&KEYINFO(unbound3_a3_s0_level1_key_disable),
};

static void *unbound3_a3_s0_wrdisbv_param_cmdtbl[] = {
#ifdef CONFIG_SUPPORT_BRIGHTDOT_TEST
	&CONDINFO_IF(unbound3_a3_s0_cond_is_brightdot_enabled),
		&PKTINFO(unbound3_a3_s0_brightdot_aor),
		&PKTINFO(unbound3_a3_s0_brightdot_aor_enable),
		&PKTINFO(unbound3_a3_s0_brightdot_max_brightness),
	&CONDINFO_EL(unbound3_a3_s0_cond_is_brightdot_enabled),
		&PKTINFO(unbound3_a3_s0_wrdisbv),
	&CONDINFO_FI(unbound3_a3_s0_cond_is_brightdot_enabled),
#else
	&PKTINFO(unbound3_a3_s0_wrdisbv),
#endif
	&PKTINFO(unbound3_a3_s0_elvss),
};
static DEFINE_SEQINFO(unbound3_a3_s0_wrdisbv_param_seq, unbound3_a3_s0_wrdisbv_param_cmdtbl);

static void *unbound3_a3_s0_set_bl_param_cmdtbl[] = {
	&PKTINFO(unbound3_a3_s0_gamma_select_off),
	&SEQINFO(unbound3_a3_s0_wrdisbv_param_seq),

	&CONDINFO_IF(unbound3_a3_s0_cond_is_id_gte_e5),
		&PKTINFO(unbound3_a3_s0_sync_control),
		&PKTINFO(unbound3_a3_s0_hbm_transition),
	&CONDINFO_EL(unbound3_a3_s0_cond_is_id_gte_e5),
		&PKTINFO(unbound3_a3_s0_glut),
		&PKTINFO(unbound3_a3_s0_hbm_transition_lt_e5),
	&CONDINFO_FI(unbound3_a3_s0_cond_is_id_gte_e5),

	&PKTINFO(unbound3_a3_s0_gamma_write_0),
	&PKTINFO(unbound3_a3_s0_gamma_write_1),
	&PKTINFO(unbound3_a3_s0_gamma_update_enable),

	&PKTINFO(unbound3_a3_s0_aor_manual_value),
	&PKTINFO(unbound3_a3_s0_vaint),

	&CONDINFO_IF(unbound3_a3_s0_cond_is_id_gte_e3),
		&PKTINFO(unbound3_a3_s0_irc_mode),
	&CONDINFO_EL(unbound3_a3_s0_cond_is_id_gte_e3),
		&PKTINFO(unbound3_a3_s0_irc_mode_lt_e3),
	&CONDINFO_FI(unbound3_a3_s0_cond_is_id_gte_e3),

	&PKTINFO(unbound3_a3_s0_tset),
	&PKTINFO(unbound3_a3_s0_acl_setting_1),
	&PKTINFO(unbound3_a3_s0_acl_setting_2),
	&PKTINFO(unbound3_a3_s0_acl_control),
#ifdef CONFIG_SUPPORT_XTALK_MODE
	&PKTINFO(unbound3_a3_s0_xtalk_vgh),
#endif
	&CONDINFO_IF(unbound3_a3_s0_cond_is_gamma_select_off_brt),
		&PKTINFO(unbound3_a3_s0_gamma_select_off),
	&CONDINFO_EL(unbound3_a3_s0_cond_is_gamma_select_off_brt),
		&PKTINFO(unbound3_a3_s0_gamma_select),
	&CONDINFO_FI(unbound3_a3_s0_cond_is_gamma_select_off_brt),
	&PKTINFO(unbound3_a3_s0_gamma_update_enable),
};
static DEFINE_SEQINFO(unbound3_a3_s0_set_bl_param_seq, unbound3_a3_s0_set_bl_param_cmdtbl);

static void *unbound3_a3_s0_set_bl_cmdtbl[] = {
	&KEYINFO(unbound3_a3_s0_level1_key_enable),
	&KEYINFO(unbound3_a3_s0_level2_key_enable),
	&SEQINFO(unbound3_a3_s0_set_bl_param_seq),
	&PKTINFO(unbound3_a3_s0_gamma_update_enable),
#ifdef CONFIG_SUPPORT_MAFPC
	&PKTINFO(unbound3_a3_s0_mafpc_scale_gparam),
	&PKTINFO(unbound3_a3_s0_mafpc_scale),
#endif
	&KEYINFO(unbound3_a3_s0_level2_key_disable),
	&KEYINFO(unbound3_a3_s0_level1_key_disable),
	&CONDINFO_IF(unbound3_a3_s0_cond_is_first_set_bl),
		&DLYINFO(unbound3_a3_s0_wait_20msec),
		&KEYINFO(unbound3_a3_s0_level2_key_enable),
		&PKTINFO(unbound3_a3_s0_smooth_dimming),
		&KEYINFO(unbound3_a3_s0_level2_key_disable),
	&CONDINFO_FI(unbound3_a3_s0_cond_is_first_set_bl),
};

/* set_fps_param for latest */
static void *unbound3_a3_s0_set_fps_param_cmdtbl[] = {
	&CONDINFO_IF(unbound3_a3_s0_cond_is_id_gte_e2),
		&PKTINFO(unbound3_a3_s0_lfd_frame_sel),
		&PKTINFO(unbound3_a3_s0_lfd_on_1),
		&PKTINFO(unbound3_a3_s0_lfd_on_2),
	&CONDINFO_FI(unbound3_a3_s0_cond_is_id_gte_e2),
	&PKTINFO(unbound3_a3_s0_osc_1),
	&PKTINFO(unbound3_a3_s0_osc_2),
	&PKTINFO(unbound3_a3_s0_set_te_frame_sel),

	&PKTINFO(unbound3_a3_s0_vfp_hs),
	&PKTINFO(unbound3_a3_s0_vfp_ns),

#ifdef CONFIG_SUPPORT_BRIGHTDOT_TEST
	&CONDINFO_IF(unbound3_a3_s0_cond_is_brightdot_enabled),
		&PKTINFO(unbound3_a3_s0_brightdot_aor_enable),
	&CONDINFO_EL(unbound3_a3_s0_cond_is_brightdot_enabled),
		&PKTINFO(unbound3_a3_s0_aor_manual_onoff),
	&CONDINFO_FI(unbound3_a3_s0_cond_is_brightdot_enabled),
#else
	&PKTINFO(unbound3_a3_s0_aor_manual_onoff),
#endif
	&PKTINFO(unbound3_a3_s0_gamma_select),

	&CONDINFO_IF(unbound3_a3_s0_cond_is_id_gte_e2),
		&PKTINFO(unbound3_a3_s0_fps),
	&CONDINFO_FI(unbound3_a3_s0_cond_is_id_gte_e2),
	&CONDINFO_IF(unbound3_a3_s0_cond_is_id_gte_e4),
		&PKTINFO(unbound3_a3_s0_osc_sel),
	&CONDINFO_EL(unbound3_a3_s0_cond_is_id_gte_e4),
		&PKTINFO(unbound3_a3_s0_osc_sel_lt_e4),
	&CONDINFO_FI(unbound3_a3_s0_cond_is_id_gte_e4),
};
static DEFINE_SEQINFO(unbound3_a3_s0_set_fps_param_seq,
		unbound3_a3_s0_set_fps_param_cmdtbl);

static void *unbound3_a3_s0_set_fps_cmdtbl[] = {
	&KEYINFO(unbound3_a3_s0_level2_key_enable),
	&CONDINFO_IF(unbound3_a3_s0_cond_is_wait_vsync_needed),
		&DLYINFO(unbound3_a3_s0_wait_1_vsync),
	&CONDINFO_FI(unbound3_a3_s0_cond_is_wait_vsync_needed),
	&SEQINFO(unbound3_a3_s0_set_bl_param_seq),
	&SEQINFO(unbound3_a3_s0_set_fps_param_seq),
	&PKTINFO(unbound3_a3_s0_gamma_update_enable),
	&KEYINFO(unbound3_a3_s0_level2_key_disable),
};

#if defined(CONFIG_PANEL_DISPLAY_MODE)
static void *unbound3_a3_s0_display_mode_cmdtbl[] = {
	&KEYINFO(unbound3_a3_s0_level1_key_enable),
	&KEYINFO(unbound3_a3_s0_level2_key_enable),
	&CONDINFO_IF(unbound3_a3_s0_cond_is_panel_state_not_lpm),
	&CONDINFO_IF(unbound3_a3_s0_cond_is_wait_vsync_needed),
		&DLYINFO(unbound3_a3_s0_wait_1_vsync),
	&CONDINFO_FI(unbound3_a3_s0_cond_is_wait_vsync_needed),
	&SEQINFO(unbound3_a3_s0_set_bl_param_seq),
	&SEQINFO(unbound3_a3_s0_set_fps_param_seq),
	&PKTINFO(unbound3_a3_s0_gamma_update_enable),
	&CONDINFO_FI(unbound3_a3_s0_cond_is_panel_state_not_lpm),
#ifdef CONFIG_SUPPORT_DSU
	&CONDINFO_IF(unbound3_a3_s0_cond_is_panel_mres_updated),
	/* +++ w/a clear move on setting +++ */
	&KEYINFO(unbound3_a3_s0_level3_key_enable),
	&PKTINFO(unbound3_a3_s0_omok_wa_move_off),
	&PKTINFO(unbound3_a3_s0_omok_wa_move_sync),
	&DLYINFO(unbound3_a3_s0_wait_1_vsync),
	&KEYINFO(unbound3_a3_s0_level3_key_disable),
	/* --- w/a clear move on setting --- */
	&PKTINFO(unbound3_a3_s0_dsc),
	&PKTINFO(unbound3_a3_s0_pps),
	&PKTINFO(unbound3_a3_s0_caset),
	&PKTINFO(unbound3_a3_s0_paset),
	&PKTINFO(unbound3_a3_s0_scaler),
	&CONDINFO_FI(unbound3_a3_s0_cond_is_panel_mres_updated),
	/* +++ w/a change to resolution bigger +++ */
	&CONDINFO_IF(unbound3_a3_s0_cond_is_panel_mres_updated_bigger),
	&KEYINFO(unbound3_a3_s0_level3_key_enable),
	&PKTINFO(unbound3_a3_s0_omok_wa_move_on),
	&PKTINFO(unbound3_a3_s0_omok_wa_move_sync),
	&KEYINFO(unbound3_a3_s0_level3_key_disable),
	&CONDINFO_FI(unbound3_a3_s0_cond_is_panel_mres_updated_bigger),
	/* --- w/a change to resolution bigger --- */
#endif
	&KEYINFO(unbound3_a3_s0_level2_key_disable),
	&KEYINFO(unbound3_a3_s0_level1_key_disable),
};
#endif

#ifdef CONFIG_SUPPORT_HMD
static void *unbound3_a3_s0_hmd_on_cmdtbl[] = {
	&KEYINFO(unbound3_a3_s0_level2_key_enable),
	&PKTINFO(unbound3_a3_s0_hmd_on_aid),
	&PKTINFO(unbound3_a3_s0_gamma_update_enable),
	&KEYINFO(unbound3_a3_s0_level2_key_disable),
};

static void *unbound3_a3_s0_hmd_off_cmdtbl[] = {
	&KEYINFO(unbound3_a3_s0_level2_key_enable),
	&PKTINFO(unbound3_a3_s0_hmd_off_aid),
	&PKTINFO(unbound3_a3_s0_gamma_update_enable),
	&KEYINFO(unbound3_a3_s0_level2_key_disable),
};

static void *unbound3_a3_s0_hmd_bl_cmdtbl[] = {
	&KEYINFO(unbound3_a3_s0_level1_key_enable),
	&KEYINFO(unbound3_a3_s0_level2_key_enable),
	&PKTINFO(unbound3_a3_s0_hmd_wrdisbv),
	&KEYINFO(unbound3_a3_s0_level2_key_disable),
	&KEYINFO(unbound3_a3_s0_level1_key_disable),
};
#endif

static void *unbound3_a3_s0_display_on_cmdtbl[] = {
#ifdef CONFIG_SUPPORT_MAFPC
	&KEYINFO(unbound3_a3_s0_level2_key_enable),
	&PKTINFO(unbound3_a3_s0_mafpc_enable),
	&PKTINFO(unbound3_a3_s0_mafpc_scale_gparam),
	&PKTINFO(unbound3_a3_s0_mafpc_scale),
	&PKTINFO(unbound3_a3_s0_mafpc_luminance_update),
	&KEYINFO(unbound3_a3_s0_level2_key_disable),
#endif
	&KEYINFO(unbound3_a3_s0_level1_key_enable),
	&PKTINFO(unbound3_a3_s0_display_on),
	&KEYINFO(unbound3_a3_s0_level1_key_disable),
};

static void *unbound3_a3_s0_display_off_cmdtbl[] = {
	&KEYINFO(unbound3_a3_s0_level1_key_enable),
	&PKTINFO(unbound3_a3_s0_display_off),
	&DLYINFO(unbound3_a3_s0_wait_20msec),
	&KEYINFO(unbound3_a3_s0_level1_key_disable),
};

static void *unbound3_a3_s0_exit_cmdtbl[] = {
#ifdef CONFIG_SUPPORT_AFC
	&SEQINFO(unbound3_a3_s0_mdnie_seqtbl[MDNIE_AFC_OFF_SEQ]),
	&DLYINFO(unbound3_a3_s0_wait_afc_off),
#endif
	&KEYINFO(unbound3_a3_s0_level1_key_enable),
	&KEYINFO(unbound3_a3_s0_level2_key_enable),
	&s6e3had_dmptbl[DUMP_RDDPM_SLEEP_IN],
#ifdef CONFIG_DISPLAY_USE_INFO
	&s6e3had_dmptbl[DUMP_ERR],
	&s6e3had_dmptbl[DUMP_ERR_FG],
	&s6e3had_dmptbl[DUMP_DSI_ERR],
	&s6e3had_dmptbl[DUMP_SELF_DIAG],
#endif
	&PKTINFO(unbound3_a3_s0_sleep_in),
	&DLYINFO(unbound3_a3_s0_wait_10msec),
	//&PKTINFO(unbound3_a3_s0_etc_set_slpin),
	&KEYINFO(unbound3_a3_s0_level2_key_disable),
	&KEYINFO(unbound3_a3_s0_level1_key_disable),
	&DLYINFO(unbound3_a3_s0_wait_sleep_in),
};

static void *unbound3_a3_s0_alpm_enter_delay_cmdtbl[] = {
	&DLYINFO(unbound3_a3_s0_wait_124msec),
};

static void *unbound3_a3_s0_alpm_enter_cmdtbl[] = {
	&KEYINFO(unbound3_a3_s0_level1_key_enable),
	&KEYINFO(unbound3_a3_s0_level2_key_enable),
	/* disable h/w te modulation */
	&PKTINFO(unbound3_a3_s0_clr_te_frame_sel),
	&PKTINFO(unbound3_a3_s0_gamma_select_off),
	&PKTINFO(unbound3_a3_s0_lpm_enter_mode),
	&PKTINFO(unbound3_a3_s0_lpm_control),
	&CONDINFO_IF(unbound3_a3_s0_cond_is_id_gte_e2),
		&PKTINFO(unbound3_a3_s0_lpm_lfd),
	&CONDINFO_FI(unbound3_a3_s0_cond_is_id_gte_e2),
	&PKTINFO(unbound3_a3_s0_lpm_fps),
	&PKTINFO(unbound3_a3_s0_lpm_nit_pre),
	&PKTINFO(unbound3_a3_s0_lpm_elvss_on),
	&PKTINFO(unbound3_a3_s0_lpm_nit),
	&PKTINFO(unbound3_a3_s0_lpm_on),
	&PKTINFO(unbound3_a3_s0_gamma_update_enable),
	&KEYINFO(unbound3_a3_s0_level2_key_disable),
	&KEYINFO(unbound3_a3_s0_level1_key_disable),
	&DLYINFO(unbound3_a3_s0_wait_1_frame),
};

static void *unbound3_a3_s0_alpm_exit_cmdtbl[] = {
	&KEYINFO(unbound3_a3_s0_level1_key_enable),
	&KEYINFO(unbound3_a3_s0_level2_key_enable),
	&PKTINFO(unbound3_a3_s0_lpm_swire_no_pulse),
	&PKTINFO(unbound3_a3_s0_lpm_exit_mode),
	&PKTINFO(unbound3_a3_s0_lpm_elvss_off),
	&CONDINFO_IF(unbound3_a3_s0_cond_is_id_gte_e2),
		&PKTINFO(unbound3_a3_s0_lpm_lfd_off),
	&CONDINFO_FI(unbound3_a3_s0_cond_is_id_gte_e2),
	&PKTINFO(unbound3_a3_s0_smooth_dimming_init),
	&PKTINFO(unbound3_a3_s0_wrdisbv),
	&CONDINFO_IF(unbound3_a3_s0_cond_is_id_gte_e5),
		&PKTINFO(unbound3_a3_s0_sync_control),
		&PKTINFO(unbound3_a3_s0_hbm_transition),
	&CONDINFO_EL(unbound3_a3_s0_cond_is_id_gte_e5),
		&PKTINFO(unbound3_a3_s0_glut),
		&PKTINFO(unbound3_a3_s0_hbm_transition_lt_e5),
	&CONDINFO_FI(unbound3_a3_s0_cond_is_id_gte_e5),
	&DLYINFO(unbound3_a3_s0_wait_34msec),
	&PKTINFO(unbound3_a3_s0_lpm_aor_off),
	&KEYINFO(unbound3_a3_s0_level2_key_disable),
	&KEYINFO(unbound3_a3_s0_level1_key_disable),
};

static void *unbound3_a3_s0_dia_onoff_cmdtbl[] = {
	&KEYINFO(unbound3_a3_s0_level2_key_enable),
	&PKTINFO(unbound3_a3_s0_dia_setting),
	&PKTINFO(unbound3_a3_s0_dia_onoff),
	&KEYINFO(unbound3_a3_s0_level2_key_disable),
};


static void *unbound3_a3_s0_check_condition_cmdtbl[] = {
	&KEYINFO(unbound3_a3_s0_level2_key_enable),
	&s6e3had_dmptbl[DUMP_RDDPM],
	&s6e3had_dmptbl[DUMP_SELF_DIAG],
#ifdef CONFIG_SUPPORT_MAFPC
	&KEYINFO(unbound3_a3_s0_level3_key_enable),
	&s6e3had_dmptbl[DUMP_MAFPC],
	&s6e3had_dmptbl[DUMP_MAFPC_FLASH],
	&s6e3had_dmptbl[DUMP_SELF_MASK_CRC],
	&KEYINFO(unbound3_a3_s0_level3_key_disable),
#endif
	&KEYINFO(unbound3_a3_s0_level2_key_disable),
};


#ifdef CONFIG_SUPPORT_DSU
static void *unbound3_a3_s0_dsu_mode_cmdtbl[] = {
	&KEYINFO(unbound3_a3_s0_level1_key_enable),
	&KEYINFO(unbound3_a3_s0_level2_key_enable),
	&PKTINFO(unbound3_a3_s0_dsc),
	&PKTINFO(unbound3_a3_s0_pps),
	&PKTINFO(unbound3_a3_s0_caset),
	&PKTINFO(unbound3_a3_s0_paset),
	&PKTINFO(unbound3_a3_s0_scaler),
	&KEYINFO(unbound3_a3_s0_level2_key_disable),
	&KEYINFO(unbound3_a3_s0_level1_key_disable),
};
#endif

static void *unbound3_a3_s0_mcd_on_cmdtbl[] = {
	&KEYINFO(unbound3_a3_s0_level2_key_enable),
	&PKTINFO(unbound3_a3_s0_mcd_on_01),
	&PKTINFO(unbound3_a3_s0_mcd_on_02),
	&PKTINFO(unbound3_a3_s0_mcd_on_03),
	&PKTINFO(unbound3_a3_s0_mcd_on_04),
	&PKTINFO(unbound3_a3_s0_gamma_update_enable),
	&DLYINFO(unbound3_a3_s0_wait_100msec),
	&KEYINFO(unbound3_a3_s0_level2_key_disable),
};

static void *unbound3_a3_s0_mcd_off_cmdtbl[] = {
	&KEYINFO(unbound3_a3_s0_level2_key_enable),
	&PKTINFO(unbound3_a3_s0_mcd_off_01),
	&PKTINFO(unbound3_a3_s0_mcd_off_02),
	&PKTINFO(unbound3_a3_s0_mcd_off_03),
	&PKTINFO(unbound3_a3_s0_mcd_off_04),
	&PKTINFO(unbound3_a3_s0_gamma_update_enable),
	&DLYINFO(unbound3_a3_s0_wait_100msec),
	&KEYINFO(unbound3_a3_s0_level2_key_disable),
};

#ifdef CONFIG_SUPPORT_MST
static void *unbound3_a3_s0_mst_on_cmdtbl[] = {
	&KEYINFO(unbound3_a3_s0_level2_key_enable),
	&PKTINFO(unbound3_a3_s0_mst_on_01),
	&PKTINFO(unbound3_a3_s0_mst_on_02),
	&PKTINFO(unbound3_a3_s0_gamma_update_enable),
	&KEYINFO(unbound3_a3_s0_level2_key_disable),
};

static void *unbound3_a3_s0_mst_off_cmdtbl[] = {
	&KEYINFO(unbound3_a3_s0_level2_key_enable),
	&PKTINFO(unbound3_a3_s0_mst_off_01),
	&PKTINFO(unbound3_a3_s0_mst_off_02),
	&PKTINFO(unbound3_a3_s0_gamma_update_enable),
	&KEYINFO(unbound3_a3_s0_level2_key_disable),
};
#endif

#ifdef CONFIG_SUPPORT_CCD_TEST
static void *unbound3_a3_s0_ccd_test_cmdtbl[] = {
	&KEYINFO(unbound3_a3_s0_level2_key_enable),
	&KEYINFO(unbound3_a3_s0_level3_key_enable),
	&PKTINFO(unbound3_a3_s0_ccd_test_disable),
	&PKTINFO(unbound3_a3_s0_ccd_set),
	&PKTINFO(unbound3_a3_s0_ccd_test_enable),
	&DLYINFO(unbound3_a3_s0_wait_1msec),
	&s6e3had_restbl[RES_CCD_STATE],
	&PKTINFO(unbound3_a3_s0_ccd_test_disable),
	&KEYINFO(unbound3_a3_s0_level3_key_disable),
	&KEYINFO(unbound3_a3_s0_level2_key_disable),
};
#endif

#ifdef CONFIG_SUPPORT_DYNAMIC_HLPM
static void *unbound3_a3_s0_dynamic_hlpm_on_cmdtbl[] = {
	&KEYINFO(unbound3_a3_s0_level2_key_enable),
	&PKTINFO(unbound3_a3_s0_dynamic_hlpm_enable),
	&KEYINFO(unbound3_a3_s0_level2_key_disable),
};

static void *unbound3_a3_s0_dynamic_hlpm_off_cmdtbl[] = {
	&KEYINFO(unbound3_a3_s0_level2_key_enable),
	&PKTINFO(unbound3_a3_s0_dynamic_hlpm_disable),
	&KEYINFO(unbound3_a3_s0_level2_key_disable),
};
#endif

#ifdef CONFIG_SUPPORT_GRAM_CHECKSUM
static void *unbound3_a3_s0_gct_enter_cmdtbl[] = {
	&KEYINFO(unbound3_a3_s0_level1_key_enable),
	&PKTINFO(unbound3_a3_s0_sw_reset),
	&DLYINFO(unbound3_a3_s0_wait_120msec),
	&KEYINFO(unbound3_a3_s0_level1_key_enable),
	&KEYINFO(unbound3_a3_s0_level2_key_enable),
	&KEYINFO(unbound3_a3_s0_level3_key_enable),
	&PKTINFO(unbound3_a3_s0_vddm_init),
	&PKTINFO(unbound3_a3_s0_sleep_out),
	&DLYINFO(unbound3_a3_s0_wait_120msec),
	&PKTINFO(unbound3_a3_s0_hop_chksum_on),
	&PKTINFO(unbound3_a3_s0_gram_dbv_max),
	&PKTINFO(unbound3_a3_s0_gram_bctrl_on),
	&PKTINFO(unbound3_a3_s0_gct_dsc),
	&PKTINFO(unbound3_a3_s0_gct_pps),
	&KEYINFO(unbound3_a3_s0_level3_key_disable),
	&KEYINFO(unbound3_a3_s0_level2_key_disable),
	&KEYINFO(unbound3_a3_s0_level1_key_disable),
};

static void *unbound3_a3_s0_gct_vddm_cmdtbl[] = {
	&KEYINFO(unbound3_a3_s0_level1_key_enable),
	&KEYINFO(unbound3_a3_s0_level2_key_enable),
	&KEYINFO(unbound3_a3_s0_level3_key_enable),
	&PKTINFO(unbound3_a3_s0_vddm_volt),
	&DLYINFO(unbound3_a3_s0_wait_vddm_update),
	&PKTINFO(unbound3_a3_s0_vddm_volt_2),
	&PKTINFO(unbound3_a3_s0_vddm_set_1),
	&PKTINFO(unbound3_a3_s0_vddm_set_2),
	&DLYINFO(unbound3_a3_s0_wait_vddm_update),
	&KEYINFO(unbound3_a3_s0_level3_key_disable),
	&KEYINFO(unbound3_a3_s0_level2_key_disable),
	&KEYINFO(unbound3_a3_s0_level1_key_disable),
};

static void *unbound3_a3_s0_gct_img_0_update_cmdtbl[] = {
	&KEYINFO(unbound3_a3_s0_level1_key_enable),
	&KEYINFO(unbound3_a3_s0_level2_key_enable),
	&KEYINFO(unbound3_a3_s0_level3_key_enable),
	&PKTINFO(unbound3_a3_s0_gram_chksum_start),
	&PKTINFO(unbound3_a3_s0_gram_inv_img_pattern_on),
	&DLYINFO(unbound3_a3_s0_wait_20msec_gram_img_update),
	&PKTINFO(unbound3_a3_s0_gram_img_pattern_off),
	&DLYINFO(unbound3_a3_s0_wait_20msec_gram_img_update),
	&PKTINFO(unbound3_a3_s0_gram_img_pattern_on),
	&DLYINFO(unbound3_a3_s0_wait_gram_img_update),
	&s6e3had_restbl[RES_GRAM_CHECKSUM],
	&KEYINFO(unbound3_a3_s0_level3_key_disable),
	&KEYINFO(unbound3_a3_s0_level2_key_disable),
	&KEYINFO(unbound3_a3_s0_level1_key_disable),
};

static void *unbound3_a3_s0_gct_img_1_update_cmdtbl[] = {
	&KEYINFO(unbound3_a3_s0_level1_key_enable),
	&KEYINFO(unbound3_a3_s0_level2_key_enable),
	&KEYINFO(unbound3_a3_s0_level3_key_enable),
	&PKTINFO(unbound3_a3_s0_gram_img_pattern_off),
	&DLYINFO(unbound3_a3_s0_wait_20msec_gram_img_update),
	&PKTINFO(unbound3_a3_s0_gram_img_pattern_on),
	&DLYINFO(unbound3_a3_s0_wait_gram_img_update),
	&s6e3had_restbl[RES_GRAM_CHECKSUM],
	&PKTINFO(unbound3_a3_s0_gram_img_pattern_off),
	&KEYINFO(unbound3_a3_s0_level3_key_disable),
	&KEYINFO(unbound3_a3_s0_level2_key_disable),
	&KEYINFO(unbound3_a3_s0_level1_key_disable),
};

static void *unbound3_a3_s0_gct_exit_cmdtbl[] = {
	&KEYINFO(unbound3_a3_s0_level1_key_enable),
	&KEYINFO(unbound3_a3_s0_level2_key_enable),
	&KEYINFO(unbound3_a3_s0_level3_key_enable),
	&PKTINFO(unbound3_a3_s0_vddm_orig),
	&PKTINFO(unbound3_a3_s0_vddm_orig_2),
	&PKTINFO(unbound3_a3_s0_vddm_set_1),
	&PKTINFO(unbound3_a3_s0_vddm_set_2),
	&KEYINFO(unbound3_a3_s0_level3_key_disable),
	&KEYINFO(unbound3_a3_s0_level2_key_disable),
	&KEYINFO(unbound3_a3_s0_level1_key_disable),
};
#endif

#ifdef CONFIG_SUPPORT_GRAYSPOT_TEST
static void *unbound3_a3_s0_grayspot_on_cmdtbl[] = {
	&KEYINFO(unbound3_a3_s0_level2_key_enable),
	&PKTINFO(unbound3_a3_s0_grayspot_on_00),
	&PKTINFO(unbound3_a3_s0_grayspot_on_01),
	&PKTINFO(unbound3_a3_s0_grayspot_on_02),
	&PKTINFO(unbound3_a3_s0_grayspot_on_03),
	&PKTINFO(unbound3_a3_s0_gamma_update_enable),
	&DLYINFO(unbound3_a3_s0_wait_100msec),
	&KEYINFO(unbound3_a3_s0_level2_key_disable),
};

static void *unbound3_a3_s0_grayspot_off_cmdtbl[] = {
	&KEYINFO(unbound3_a3_s0_level2_key_enable),
	&PKTINFO(unbound3_a3_s0_grayspot_off_00),
	&PKTINFO(unbound3_a3_s0_grayspot_off_01),
	&PKTINFO(unbound3_a3_s0_tset),
	&PKTINFO(unbound3_a3_s0_grayspot_cal),
	&PKTINFO(unbound3_a3_s0_gamma_update_enable),
	&DLYINFO(unbound3_a3_s0_wait_100msec),
	&KEYINFO(unbound3_a3_s0_level2_key_disable),
};
#endif

#ifdef CONFIG_SUPPORT_DDI_CMDLOG
static void *unbound3_a3_s0_cmdlog_dump_cmdtbl[] = {
	&KEYINFO(unbound3_a3_s0_level2_key_enable),
	&s6e3had_dmptbl[DUMP_CMDLOG],
	&KEYINFO(unbound3_a3_s0_level2_key_disable),
};
#endif
#ifdef CONFIG_SUPPORT_BRIGHTDOT_TEST
static void *unbound3_a3_s0_brightdot_test_cmdtbl[] = {
	&KEYINFO(unbound3_a3_s0_level1_key_enable),
	&KEYINFO(unbound3_a3_s0_level2_key_enable),
	&SEQINFO(unbound3_a3_s0_wrdisbv_param_seq),
	&PKTINFO(unbound3_a3_s0_brightdot_aor_enable),
	&PKTINFO(unbound3_a3_s0_gamma_update_enable),
	&KEYINFO(unbound3_a3_s0_level2_key_disable),
	&KEYINFO(unbound3_a3_s0_level1_key_disable),
};
#endif
static void *unbound3_a3_s0_dump_cmdtbl[] = {
	&KEYINFO(unbound3_a3_s0_level1_key_enable),
	&KEYINFO(unbound3_a3_s0_level2_key_enable),
	&KEYINFO(unbound3_a3_s0_level3_key_enable),
	&s6e3had_dmptbl[DUMP_RDDPM],
	&s6e3had_dmptbl[DUMP_RDDSM],
	&s6e3had_dmptbl[DUMP_ERR],
	&s6e3had_dmptbl[DUMP_ERR_FG],
	&s6e3had_dmptbl[DUMP_DSI_ERR],
	&s6e3had_dmptbl[DUMP_SELF_DIAG],
	&KEYINFO(unbound3_a3_s0_level3_key_disable),
	&KEYINFO(unbound3_a3_s0_level2_key_disable),
	&KEYINFO(unbound3_a3_s0_level1_key_disable),
};

static void *unbound3_a3_s0_dummy_cmdtbl[] = {
	NULL,
};

#ifdef CONFIG_SUPPORT_GM2_FLASH
static void *unbound3_a3_s0_gm2_flash_res_init_cmdtbl[] = {
	&s6e3had_restbl[RES_GM2_FLASH_VBIAS1],
	&s6e3had_restbl[RES_GM2_FLASH_VBIAS2],
};
#endif

static struct seqinfo unbound3_a3_s0_seqtbl[MAX_PANEL_SEQ] = {
	[PANEL_INIT_SEQ] = SEQINFO_INIT("init-seq", unbound3_a3_s0_init_cmdtbl),
	[PANEL_RES_INIT_SEQ] = SEQINFO_INIT("resource-init-seq", unbound3_a3_s0_res_init_cmdtbl),
	[PANEL_ID_READ_SEQ] = SEQINFO_INIT("id-read-seq", unbound3_a3_s0_id_read_cmdtbl),
	[PANEL_SET_BL_SEQ] = SEQINFO_INIT("set-bl-seq", unbound3_a3_s0_set_bl_cmdtbl),
#ifdef CONFIG_SUPPORT_HMD
	[PANEL_HMD_ON_SEQ] = SEQINFO_INIT("hmd-on-seq", unbound3_a3_s0_hmd_on_cmdtbl),
	[PANEL_HMD_OFF_SEQ] = SEQINFO_INIT("hmd-off-seq", unbound3_a3_s0_hmd_off_cmdtbl),
	[PANEL_HMD_BL_SEQ] = SEQINFO_INIT("hmd-bl-seq", unbound3_a3_s0_hmd_bl_cmdtbl),
#endif
	[PANEL_DISPLAY_ON_SEQ] = SEQINFO_INIT("display-on-seq", unbound3_a3_s0_display_on_cmdtbl),
	[PANEL_DISPLAY_OFF_SEQ] = SEQINFO_INIT("display-off-seq", unbound3_a3_s0_display_off_cmdtbl),
	[PANEL_EXIT_SEQ] = SEQINFO_INIT("exit-seq", unbound3_a3_s0_exit_cmdtbl),
	[PANEL_MCD_ON_SEQ] = SEQINFO_INIT("mcd-on-seq", unbound3_a3_s0_mcd_on_cmdtbl),
	[PANEL_MCD_OFF_SEQ] = SEQINFO_INIT("mcd-off-seq", unbound3_a3_s0_mcd_off_cmdtbl),
	[PANEL_DIA_ONOFF_SEQ] = SEQINFO_INIT("dia-onoff-seq", unbound3_a3_s0_dia_onoff_cmdtbl),
	[PANEL_ALPM_ENTER_SEQ] = SEQINFO_INIT("alpm-enter-seq", unbound3_a3_s0_alpm_enter_cmdtbl),
	[PANEL_ALPM_DELAY_SEQ] = SEQINFO_INIT("alpm-enter-delay-seq", unbound3_a3_s0_alpm_enter_delay_cmdtbl),
	[PANEL_ALPM_EXIT_SEQ] = SEQINFO_INIT("alpm-exit-seq", unbound3_a3_s0_alpm_exit_cmdtbl),
	[PANEL_CHECK_CONDITION_SEQ] = SEQINFO_INIT("check-condition-seq", unbound3_a3_s0_check_condition_cmdtbl),
	[PANEL_DUMP_SEQ] = SEQINFO_INIT("dump-seq", unbound3_a3_s0_dump_cmdtbl),
#ifdef CONFIG_SUPPORT_GM2_FLASH
	[PANEL_GM2_FLASH_RES_INIT_SEQ] = SEQINFO_INIT("gm2-flash-resource-init-seq", unbound3_a3_s0_gm2_flash_res_init_cmdtbl),
#endif
#ifdef CONFIG_SUPPORT_DYNAMIC_HLPM
	[PANEL_DYNAMIC_HLPM_ON_SEQ] = SEQINFO_INIT("dynamic-hlpm-on-seq", unbound3_a3_s0_dynamic_hlpm_on_cmdtbl),
	[PANEL_DYNAMIC_HLPM_OFF_SEQ] = SEQINFO_INIT("dynamic-hlpm-off-seq", unbound3_a3_s0_dynamic_hlpm_off_cmdtbl),
#endif
#ifdef CONFIG_SUPPORT_DSU
	[PANEL_DSU_SEQ] = SEQINFO_INIT("dsu-mode-seq", unbound3_a3_s0_dsu_mode_cmdtbl),
#endif
	[PANEL_FPS_SEQ] = SEQINFO_INIT("set-fps-seq", unbound3_a3_s0_set_fps_cmdtbl),
#if defined(CONFIG_PANEL_DISPLAY_MODE)
	[PANEL_DISPLAY_MODE_SEQ] = SEQINFO_INIT("display-mode-seq", unbound3_a3_s0_display_mode_cmdtbl),
#endif
#ifdef CONFIG_DYNAMIC_MIPI
	[PANEL_DM_SET_FFC_SEQ] = SEQINFO_INIT("dm-set_ffc-seq", unbound3_a3_s0_dm_set_ffc_cmdtbl),
	[PANEL_DM_OFF_FFC_SEQ] = SEQINFO_INIT("dm-off_ffc-seq", unbound3_a3_s0_dm_off_ffc_cmdtbl),
#endif
#ifdef CONFIG_SUPPORT_MAFPC
	[PANEL_MAFPC_IMG_SEQ] = SEQINFO_INIT("mafpc-img-seq", unbound3_a3_s0_mafpc_image_cmdtbl),
	[PANEL_MAFPC_CHECKSUM_SEQ] = SEQINFO_INIT("mafpc-check-seq", unbound3_a3_s0_mafpc_check_cmdtbl),
	[PANEL_MAFPC_ON_SEQ] = SEQINFO_INIT("mafpc-on-seq", unbound3_a3_s0_mafpc_on_cmdtbl),
	[PANEL_MAFPC_OFF_SEQ] = SEQINFO_INIT("mafpc-off-seq", unbound3_a3_s0_mafpc_off_cmdtbl),
#endif
#ifdef CONFIG_SUPPORT_MST
	[PANEL_MST_ON_SEQ] = SEQINFO_INIT("mst-on-seq", unbound3_a3_s0_mst_on_cmdtbl),
	[PANEL_MST_OFF_SEQ] = SEQINFO_INIT("mst-off-seq", unbound3_a3_s0_mst_off_cmdtbl),
#endif
#ifdef CONFIG_SUPPORT_GRAM_CHECKSUM
	[PANEL_GCT_ENTER_SEQ] = SEQINFO_INIT("gct-enter-seq", unbound3_a3_s0_gct_enter_cmdtbl),
	[PANEL_GCT_VDDM_SEQ] = SEQINFO_INIT("gct-vddm-seq", unbound3_a3_s0_gct_vddm_cmdtbl),
	[PANEL_GCT_IMG_0_UPDATE_SEQ] = SEQINFO_INIT("gct-img-0-update-seq", unbound3_a3_s0_gct_img_0_update_cmdtbl),
	[PANEL_GCT_IMG_1_UPDATE_SEQ] = SEQINFO_INIT("gct-img-1-update-seq", unbound3_a3_s0_gct_img_1_update_cmdtbl),
	[PANEL_GCT_EXIT_SEQ] = SEQINFO_INIT("gct-exit-seq", unbound3_a3_s0_gct_exit_cmdtbl),
#endif
#ifdef CONFIG_SUPPORT_CCD_TEST
	[PANEL_CCD_TEST_SEQ] = SEQINFO_INIT("ccd-test-seq", unbound3_a3_s0_ccd_test_cmdtbl),
#endif
#ifdef CONFIG_SUPPORT_GRAYSPOT_TEST
	[PANEL_GRAYSPOT_ON_SEQ] = SEQINFO_INIT("grayspot-on-seq", unbound3_a3_s0_grayspot_on_cmdtbl),
	[PANEL_GRAYSPOT_OFF_SEQ] = SEQINFO_INIT("grayspot-off-seq", unbound3_a3_s0_grayspot_off_cmdtbl),
#endif
#ifdef CONFIG_SUPPORT_DDI_CMDLOG
	[PANEL_CMDLOG_DUMP_SEQ] = SEQINFO_INIT("cmdlog-dump-seq", unbound3_a3_s0_cmdlog_dump_cmdtbl),
#endif
#ifdef CONFIG_SUPPORT_BRIGHTDOT_TEST
	[PANEL_BRIGHTDOT_TEST_SEQ] = SEQINFO_INIT("brightdot-seq", unbound3_a3_s0_brightdot_test_cmdtbl),
#endif
//	[PANEL_INIT_BOOT_SEQ] = SEQINFO_INIT("init-boot-seq", unbound3_a3_s0_gamma_read_cmdtbl),
	[PANEL_DUMMY_SEQ] = SEQINFO_INIT("dummy-seq", unbound3_a3_s0_dummy_cmdtbl),
};

#ifdef CONFIG_SUPPORT_POC_SPI
struct spi_data *s6e3had_unbound3_a3_s0_spi_data_list[] = {
	&w25q80_spi_data,
	&mx25r4035_spi_data,
};
#endif

struct common_panel_info s6e3had_unbound3_a3_s0_panel_info = {
	.ldi_name = "s6e3had",
	.name = "s6e3had_unbound3_a3_s0_default",
	.model = "AMB681XV01",
	.vendor = "SDC",
	.id = 0x8126C0,
	.rev = 0,
	.ddi_props = {
		.gpara = (DDI_SUPPORT_WRITE_GPARA |
				DDI_SUPPORT_READ_GPARA | DDI_SUPPORT_POINT_GPARA | DDI_SUPPORT_2BYTE_GPARA),
		.err_fg_recovery = false,
		.err_fg_powerdown = false,
		.support_vrr = true,
		.support_vrr_lfd = true,
	},
	.ddi_ops = {
		.gamma_flash_checksum = do_gamma_flash_checksum,
	},
#ifdef CONFIG_SUPPORT_DSU
	.mres = {
		.nr_resol = ARRAY_SIZE(s6e3had_unbound3_default_resol),
		.resol = s6e3had_unbound3_default_resol,
	},
#endif
#if defined(CONFIG_PANEL_DISPLAY_MODE)
	.common_panel_modes = &s6e3had_unbound3_display_modes,
#endif
	.vrrtbl = s6e3had_unbound3_default_vrrtbl,
	.nr_vrrtbl = ARRAY_SIZE(s6e3had_unbound3_default_vrrtbl),
	.maptbl = unbound3_a3_s0_maptbl,
	.nr_maptbl = ARRAY_SIZE(unbound3_a3_s0_maptbl),
	.seqtbl = unbound3_a3_s0_seqtbl,
	.nr_seqtbl = ARRAY_SIZE(unbound3_a3_s0_seqtbl),
	.rditbl = s6e3had_rditbl,
	.nr_rditbl = ARRAY_SIZE(s6e3had_rditbl),
	.restbl = s6e3had_restbl,
	.nr_restbl = ARRAY_SIZE(s6e3had_restbl),
	.dumpinfo = s6e3had_dmptbl,
	.nr_dumpinfo = ARRAY_SIZE(s6e3had_dmptbl),
#ifdef CONFIG_EXYNOS_DECON_MDNIE_LITE
	.mdnie_tune = &s6e3had_unbound3_a3_s0_mdnie_tune,
#endif
	.panel_dim_info = {
		[PANEL_BL_SUBDEV_TYPE_DISP] = &s6e3had_unbound3_a3_s0_panel_dimming_info,
#ifdef CONFIG_SUPPORT_HMD
		[PANEL_BL_SUBDEV_TYPE_HMD] = &s6e3had_unbound3_a3_s0_panel_hmd_dimming_info,
#endif
#ifdef CONFIG_SUPPORT_AOD_BL
		[PANEL_BL_SUBDEV_TYPE_AOD] = &s6e3had_unbound3_a3_s0_panel_aod_dimming_info,
#endif
	},
#ifdef CONFIG_EXYNOS_DECON_LCD_COPR
	.copr_data = &s6e3had_unbound3_a3_s0_copr_data,
#endif
#ifdef CONFIG_EXTEND_LIVE_CLOCK
	.aod_tune = &s6e3had_unbound3_aod,
#endif
#ifdef CONFIG_SUPPORT_DDI_FLASH
	.poc_data = &s6e3had_unbound3_poc_data,
#endif
#ifdef CONFIG_SUPPORT_POC_SPI
	.spi_data_tbl = s6e3had_unbound3_a3_s0_spi_data_list,
	.nr_spi_data_tbl = ARRAY_SIZE(s6e3had_unbound3_a3_s0_spi_data_list),
#endif
#ifdef CONFIG_DYNAMIC_MIPI
	.dm_total_band = u3_dynamic_freq_set,
#endif
#ifdef CONFIG_SUPPORT_DISPLAY_PROFILER
	.profile_tune = &s6e3had_profiler_tune,
#endif
#ifdef CONFIG_SUPPORT_MAFPC
	.mafpc_info = &s6e3had_unbound3_mafpc,
#endif

};
#endif /* __S6E3HAD_UNBOUND3_A3_S0_PANEL_H__ */
