/*
 * linux/drivers/video/fbdev/exynos/panel/s6e3fab/s6e3fab_unbound1_a3_s0_panel.h
 *
 * Header file for S6E3FAB Dimming Driver
 *
 * Copyright (c) 2016 Samsung Electronics
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#ifndef __S6E3FAB_UNBOUND1_A3_S0_PANEL_H__
#define __S6E3FAB_UNBOUND1_A3_S0_PANEL_H__

#include "../panel.h"
#include "../panel_drv.h"
#include "s6e3fab.h"
#include "s6e3fab_dimming.h"
#ifdef CONFIG_EXYNOS_DECON_MDNIE_LITE
#include "s6e3fab_unbound1_a3_s0_panel_mdnie.h"
#endif
#ifdef CONFIG_EXYNOS_DECON_LCD_COPR
#include "s6e3fab_unbound1_a3_s0_panel_copr.h"
#endif
#include "s6e3fab_unbound1_a3_s0_panel_dimming.h"
#ifdef CONFIG_SUPPORT_HMD
#include "s6e3fab_unbound1_a3_s0_panel_hmd_dimming.h"
#endif
#ifdef CONFIG_SUPPORT_AOD_BL
#include "s6e3fab_unbound1_a3_s0_panel_aod_dimming.h"
#endif
#ifdef CONFIG_EXTEND_LIVE_CLOCK
#include "s6e3fab_unbound1_aod_panel.h"
#include "../aod/aod_drv.h"
#endif

#include "s6e3fab_unbound1_resol.h"
#ifdef CONFIG_SUPPORT_DISPLAY_PROFILER
#include "s6e3fab_profiler_panel.h"
#include "../display_profiler/display_profiler.h"
#endif

#ifdef CONFIG_DYNAMIC_MIPI
#include "unbound_dm_band.h"
#endif

#ifdef CONFIG_SUPPORT_MAFPC
#include "s6e3fab_unbound_abc_data.h"
#endif

#ifdef CONFIG_SUPPORT_DDI_FLASH
#include "s6e3fab_unbound_panel_poc.h"
#endif

#ifdef CONFIG_SUPPORT_POC_SPI
#include "../panel_spi.h"
#include "../spi/w25q80_panel_spi.h"
#include "../spi/mx25r4035_panel_spi.h"
#endif

#undef __pn_name__
#define __pn_name__	unbound1_a3_s0

#undef __PN_NAME__
#define __PN_NAME__	UNBOUND1_A3_S0

/* ===================================================================================== */
/* ============================= [S6E3FAB READ INFO TABLE] ============================= */
/* ===================================================================================== */
/* <READINFO TABLE> IS DEPENDENT ON LDI. IF YOU NEED, DEFINE PANEL's RESOURCE TABLE */


/* ===================================================================================== */
/* ============================= [S6E3FAB RESOURCE TABLE] ============================== */
/* ===================================================================================== */
/* <RESOURCE TABLE> IS DEPENDENT ON LDI. IF YOU NEED, DEFINE PANEL's RESOURCE TABLE */


/* ===================================================================================== */
/* ============================== [S6E3FAB MAPPING TABLE] ============================== */
/* ===================================================================================== */

static u8 unbound1_a3_s0_brt_table[S6E3FAB_TOTAL_STEP][2] = {
	/* Normal 5x51+1 */
	{ 0x00, 0x08 }, { 0x00, 0x09 }, { 0x00, 0x0B }, { 0x00, 0x0D }, { 0x00, 0x10 },
	{ 0x00, 0x13 }, { 0x00, 0x15 }, { 0x00, 0x18 }, { 0x00, 0x1B }, { 0x00, 0x1F },
	{ 0x00, 0x22 }, { 0x00, 0x25 }, { 0x00, 0x29 }, { 0x00, 0x2C }, { 0x00, 0x30 },
	{ 0x00, 0x34 }, { 0x00, 0x38 }, { 0x00, 0x3C }, { 0x00, 0x40 }, { 0x00, 0x44 },
	{ 0x00, 0x48 }, { 0x00, 0x4C }, { 0x00, 0x50 }, { 0x00, 0x54 }, { 0x00, 0x59 },
	{ 0x00, 0x5D }, { 0x00, 0x62 }, { 0x00, 0x66 }, { 0x00, 0x6B }, { 0x00, 0x6F },
	{ 0x00, 0x74 }, { 0x00, 0x79 }, { 0x00, 0x7D }, { 0x00, 0x82 }, { 0x00, 0x87 },
	{ 0x00, 0x8C }, { 0x00, 0x91 }, { 0x00, 0x96 }, { 0x00, 0x9B }, { 0x00, 0xA0 },
	{ 0x00, 0xA5 }, { 0x00, 0xAA }, { 0x00, 0xAF }, { 0x00, 0xB4 }, { 0x00, 0xB9 },
	{ 0x00, 0xBF }, { 0x00, 0xC4 }, { 0x00, 0xC9 }, { 0x00, 0xCF }, { 0x00, 0xD4 },
	{ 0x00, 0xDA }, { 0x00, 0xDF }, { 0x00, 0xE5 }, { 0x00, 0xEA }, { 0x00, 0xF0 },
	{ 0x00, 0xF5 }, { 0x00, 0xFB }, { 0x01, 0x00 }, { 0x01, 0x06 }, { 0x01, 0x0C },
	{ 0x01, 0x12 }, { 0x01, 0x17 }, { 0x01, 0x1D }, { 0x01, 0x23 }, { 0x01, 0x29 },
	{ 0x01, 0x2F }, { 0x01, 0x35 }, { 0x01, 0x3B }, { 0x01, 0x41 }, { 0x01, 0x47 },
	{ 0x01, 0x4D }, { 0x01, 0x53 }, { 0x01, 0x59 }, { 0x01, 0x5F }, { 0x01, 0x65 },
	{ 0x01, 0x6B }, { 0x01, 0x71 }, { 0x01, 0x77 }, { 0x01, 0x7E }, { 0x01, 0x84 },
	{ 0x01, 0x8A }, { 0x01, 0x90 }, { 0x01, 0x97 }, { 0x01, 0x9D }, { 0x01, 0xA3 },
	{ 0x01, 0xAA }, { 0x01, 0xB0 }, { 0x01, 0xB7 }, { 0x01, 0xBD }, { 0x01, 0xC3 },
	{ 0x01, 0xCA }, { 0x01, 0xD0 }, { 0x01, 0xD7 }, { 0x01, 0xDE }, { 0x01, 0xE4 },
	{ 0x01, 0xEB }, { 0x01, 0xF1 }, { 0x01, 0xF8 }, { 0x01, 0xFF }, { 0x02, 0x05 },
	{ 0x02, 0x0C }, { 0x02, 0x13 }, { 0x02, 0x19 }, { 0x02, 0x20 }, { 0x02, 0x27 },
	{ 0x02, 0x2E }, { 0x02, 0x35 }, { 0x02, 0x3B }, { 0x02, 0x42 }, { 0x02, 0x49 },
	{ 0x02, 0x50 }, { 0x02, 0x57 }, { 0x02, 0x5E }, { 0x02, 0x65 }, { 0x02, 0x6C },
	{ 0x02, 0x73 }, { 0x02, 0x7A }, { 0x02, 0x81 }, { 0x02, 0x88 }, { 0x02, 0x8F },
	{ 0x02, 0x96 }, { 0x02, 0x9D }, { 0x02, 0xA4 }, { 0x02, 0xAB }, { 0x02, 0xB2 },
	{ 0x02, 0xBA }, { 0x02, 0xC1 }, { 0x02, 0xC8 }, { 0x02, 0xCF }, { 0x02, 0xD6 },
	{ 0x02, 0xDE }, { 0x02, 0xE5 }, { 0x02, 0xEC }, { 0x02, 0xF4 }, { 0x02, 0xFB },
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
	/* HBM 5x40+4 */
	{ 0x00, 0x3A }, { 0x00, 0x44 }, { 0x00, 0x4D }, { 0x00, 0x57 }, { 0x00, 0x61 },
	{ 0x00, 0x6B }, { 0x00, 0x75 }, { 0x00, 0x7E }, { 0x00, 0x88 }, { 0x00, 0x92 },
	{ 0x00, 0x9C }, { 0x00, 0xA6 }, { 0x00, 0xAF }, { 0x00, 0xB9 }, { 0x00, 0xC3 },
	{ 0x00, 0xCD }, { 0x00, 0xD7 }, { 0x00, 0xE0 }, { 0x00, 0xEA }, { 0x00, 0xF4 },
	{ 0x00, 0xFE }, { 0x01, 0x08 }, { 0x01, 0x11 }, { 0x01, 0x1B }, { 0x01, 0x25 },
	{ 0x01, 0x2F }, { 0x01, 0x39 }, { 0x01, 0x42 }, { 0x01, 0x4C }, { 0x01, 0x56 },
	{ 0x01, 0x60 }, { 0x01, 0x6A }, { 0x01, 0x73 }, { 0x01, 0x7D }, { 0x01, 0x87 },
	{ 0x01, 0x91 }, { 0x01, 0x9B }, { 0x01, 0xA4 }, { 0x01, 0xAE }, { 0x01, 0xB8 },
	{ 0x01, 0xC2 }, { 0x01, 0xCC }, { 0x01, 0xD5 }, { 0x01, 0xDF }, { 0x01, 0xE9 },
	{ 0x01, 0xF3 }, { 0x01, 0xFD }, { 0x02, 0x06 }, { 0x02, 0x10 }, { 0x02, 0x1A },
	{ 0x02, 0x24 }, { 0x02, 0x2E }, { 0x02, 0x37 }, { 0x02, 0x41 }, { 0x02, 0x4B },
	{ 0x02, 0x55 }, { 0x02, 0x5F }, { 0x02, 0x68 }, { 0x02, 0x72 }, { 0x02, 0x7C },
	{ 0x02, 0x86 }, { 0x02, 0x90 }, { 0x02, 0x99 }, { 0x02, 0xA3 }, { 0x02, 0xAD },
	{ 0x02, 0xB7 }, { 0x02, 0xC0 }, { 0x02, 0xCA }, { 0x02, 0xD4 }, { 0x02, 0xDE },
	{ 0x02, 0xE8 }, { 0x02, 0xF1 }, { 0x02, 0xFB }, { 0x03, 0x05 }, { 0x03, 0x0F },
	{ 0x03, 0x19 }, { 0x03, 0x22 }, { 0x03, 0x2C }, { 0x03, 0x36 }, { 0x03, 0x40 },
	{ 0x03, 0x4A }, { 0x03, 0x53 }, { 0x03, 0x5D }, { 0x03, 0x67 }, { 0x03, 0x71 },
	{ 0x03, 0x7B }, { 0x03, 0x84 }, { 0x03, 0x8E }, { 0x03, 0x98 }, { 0x03, 0xA2 },
	{ 0x03, 0xAC }, { 0x03, 0xB5 }, { 0x03, 0xBF }, { 0x03, 0xC9 }, { 0x03, 0xD3 },
	{ 0x03, 0xDD }, { 0x03, 0xE6 }, { 0x03, 0xF0 }, { 0x03, 0xFA }, { 0x04, 0x04 },
	{ 0x04, 0x0E }, { 0x04, 0x17 }, { 0x04, 0x21 }, { 0x04, 0x2B }, { 0x04, 0x35 },
	{ 0x04, 0x3F }, { 0x04, 0x48 }, { 0x04, 0x52 }, { 0x04, 0x5C }, { 0x04, 0x66 },
	{ 0x04, 0x70 }, { 0x04, 0x79 }, { 0x04, 0x83 }, { 0x04, 0x8D }, { 0x04, 0x97 },
	{ 0x04, 0xA1 }, { 0x04, 0xAA }, { 0x04, 0xB4 }, { 0x04, 0xBE }, { 0x04, 0xC8 },
	{ 0x04, 0xD2 }, { 0x04, 0xDB }, { 0x04, 0xE5 }, { 0x04, 0xEF }, { 0x04, 0xF9 },
	{ 0x05, 0x03 }, { 0x05, 0x0C }, { 0x05, 0x16 }, { 0x05, 0x20 }, { 0x05, 0x2A },
	{ 0x05, 0x34 }, { 0x05, 0x3D }, { 0x05, 0x47 }, { 0x05, 0x51 }, { 0x05, 0x5B },
	{ 0x05, 0x65 }, { 0x05, 0x6E }, { 0x05, 0x78 }, { 0x05, 0x82 }, { 0x05, 0x8C },
	{ 0x05, 0x96 }, { 0x05, 0x9F }, { 0x05, 0xA9 }, { 0x05, 0xB3 }, { 0x05, 0xBD },
	{ 0x05, 0xC7 }, { 0x05, 0xD0 }, { 0x05, 0xDA }, { 0x05, 0xE4 }, { 0x05, 0xEE },
	{ 0x05, 0xF8 }, { 0x06, 0x01 }, { 0x06, 0x0B }, { 0x06, 0x15 }, { 0x06, 0x1F },
	{ 0x06, 0x29 }, { 0x06, 0x32 }, { 0x06, 0x3C }, { 0x06, 0x46 }, { 0x06, 0x50 },
	{ 0x06, 0x5A }, { 0x06, 0x63 }, { 0x06, 0x6D }, { 0x06, 0x77 }, { 0x06, 0x81 },
	{ 0x06, 0x8B }, { 0x06, 0x94 }, { 0x06, 0x9E }, { 0x06, 0xA8 }, { 0x06, 0xB2 },
	{ 0x06, 0xBC }, { 0x06, 0xC5 }, { 0x06, 0xCF }, { 0x06, 0xD9 }, { 0x06, 0xE3 },
	{ 0x06, 0xED }, { 0x06, 0xF6 }, { 0x07, 0x00 }, { 0x07, 0x0A }, { 0x07, 0x14 },
	{ 0x07, 0x1E }, { 0x07, 0x27 }, { 0x07, 0x31 }, { 0x07, 0x3B }, { 0x07, 0x45 },
	{ 0x07, 0x4F }, { 0x07, 0x58 }, { 0x07, 0x62 }, { 0x07, 0x6C }, { 0x07, 0x76 },
	{ 0x07, 0x80 }, { 0x07, 0x89 }, { 0x07, 0x93 }, { 0x07, 0x9D }, { 0x07, 0xA7 },
	{ 0x07, 0xB1 }, { 0x07, 0xBA }, { 0x07, 0xC4 }, { 0x07, 0xCE }, { 0x07, 0xD8 },
	{ 0x07, 0xE2 }, { 0x07, 0xEB }, { 0x07, 0xF5 }, { 0x07, 0xFF },
};

static u8 unbound1_a3_s0_elvss_table[S6E3FAB_TOTAL_STEP][1] = {
	/* Normal  8x32 */
	{ 0x96 }, { 0x96 }, { 0x96 }, { 0x96 }, { 0x96 }, { 0x96 }, { 0x96 }, { 0x96 },
	{ 0x96 }, { 0x96 }, { 0x96 }, { 0x96 }, { 0x96 }, { 0x96 }, { 0x96 }, { 0x96 },
	{ 0x96 }, { 0x96 }, { 0x96 }, { 0x96 }, { 0x96 }, { 0x96 }, { 0x96 }, { 0x96 },
	{ 0x96 }, { 0x96 }, { 0x96 }, { 0x96 }, { 0x96 }, { 0x96 }, { 0x96 }, { 0x96 },
	{ 0x96 }, { 0x96 }, { 0x96 }, { 0x96 }, { 0x96 }, { 0x96 }, { 0x96 }, { 0x96 },
	{ 0x96 }, { 0x96 }, { 0x96 }, { 0x96 }, { 0x96 }, { 0x96 }, { 0x96 }, { 0x96 },
	{ 0x96 }, { 0x96 }, { 0x96 }, { 0x96 }, { 0x96 }, { 0x96 }, { 0x96 }, { 0x96 },
	{ 0x96 }, { 0x96 }, { 0x96 }, { 0x96 }, { 0x96 }, { 0x96 }, { 0x96 }, { 0x96 },
	{ 0x96 }, { 0x96 }, { 0x96 }, { 0x96 }, { 0x96 }, { 0x96 }, { 0x96 }, { 0x96 },
	{ 0x96 }, { 0x96 }, { 0x96 }, { 0x96 }, { 0x96 }, { 0x96 }, { 0x96 }, { 0x96 },
	{ 0x96 }, { 0x96 }, { 0x96 }, { 0x96 }, { 0x96 }, { 0x96 }, { 0x96 }, { 0x96 },
	{ 0x96 }, { 0x96 }, { 0x96 }, { 0x96 }, { 0x96 }, { 0x96 }, { 0x96 }, { 0x96 },
	{ 0x96 }, { 0x96 }, { 0x96 }, { 0x96 }, { 0x96 }, { 0x96 }, { 0x96 }, { 0x96 },
	{ 0x96 }, { 0x96 }, { 0x96 }, { 0x96 }, { 0x96 }, { 0x96 }, { 0x96 }, { 0x96 },
	{ 0x96 }, { 0x96 }, { 0x96 }, { 0x96 }, { 0x96 }, { 0x96 }, { 0x96 }, { 0x96 },
	{ 0x96 }, { 0x96 }, { 0x96 }, { 0x96 }, { 0x96 }, { 0x96 }, { 0x96 }, { 0x96 },
	{ 0x96 }, { 0x96 }, { 0x96 }, { 0x96 }, { 0x96 }, { 0x96 }, { 0x96 }, { 0x96 },
	{ 0x96 }, { 0x96 }, { 0x96 }, { 0x96 }, { 0x96 }, { 0x96 }, { 0x96 }, { 0x96 },
	{ 0x96 }, { 0x96 }, { 0x96 }, { 0x96 }, { 0x96 }, { 0x96 }, { 0x96 }, { 0x96 },
	{ 0x96 }, { 0x96 }, { 0x96 }, { 0x96 }, { 0x96 }, { 0x96 }, { 0x96 }, { 0x96 },
	{ 0x96 }, { 0x96 }, { 0x96 }, { 0x96 }, { 0x96 }, { 0x96 }, { 0x96 }, { 0x96 },
	{ 0x96 }, { 0x96 }, { 0x96 }, { 0x96 }, { 0x96 }, { 0x96 }, { 0x96 }, { 0x96 },
	{ 0x96 }, { 0x96 }, { 0x96 }, { 0x96 }, { 0x96 }, { 0x96 }, { 0x96 }, { 0x96 },
	{ 0x96 }, { 0x96 }, { 0x96 }, { 0x96 }, { 0x96 }, { 0x96 }, { 0x96 }, { 0x96 },
	{ 0x96 }, { 0x96 }, { 0x96 }, { 0x96 }, { 0x96 }, { 0x96 }, { 0x96 }, { 0x96 },
	{ 0x96 }, { 0x96 }, { 0x96 }, { 0x96 }, { 0x96 }, { 0x96 }, { 0x96 }, { 0x96 },
	{ 0x96 }, { 0x96 }, { 0x96 }, { 0x96 }, { 0x96 }, { 0x96 }, { 0x96 }, { 0x96 },
	{ 0x96 }, { 0x96 }, { 0x96 }, { 0x96 }, { 0x96 }, { 0x96 }, { 0x96 }, { 0x96 },
	{ 0x96 }, { 0x96 }, { 0x96 }, { 0x96 }, { 0x96 }, { 0x96 }, { 0x96 }, { 0x96 },
	{ 0x96 }, { 0x96 }, { 0x96 }, { 0x96 }, { 0x96 }, { 0x96 }, { 0x96 }, { 0x96 },
	{ 0x96 }, { 0x96 }, { 0x96 }, { 0x96 }, { 0x96 }, { 0x96 }, { 0x96 }, { 0x96 },
	{ 0x96 }, { 0x96 }, { 0x96 }, { 0x96 }, { 0x96 }, { 0x96 }, { 0x96 }, { 0x96 },
	/* HBM 8x25+4 */
	{ 0xA5 }, { 0xA5 }, { 0xA5 }, { 0xA5 }, { 0xA5 }, { 0xA5 }, { 0xA5 }, { 0xA5 },
	{ 0xA4 }, { 0xA4 }, { 0xA4 }, { 0xA4 }, { 0xA4 }, { 0xA4 }, { 0xA4 }, { 0xA4 },
	{ 0xA4 }, { 0xA4 }, { 0xA4 }, { 0xA4 }, { 0xA4 }, { 0xA3 }, { 0xA3 }, { 0xA3 },
	{ 0xA3 }, { 0xA3 }, { 0xA3 }, { 0xA3 }, { 0xA3 }, { 0xA3 }, { 0xA3 }, { 0xA3 },
	{ 0xA3 }, { 0xA3 }, { 0xA2 }, { 0xA2 }, { 0xA2 }, { 0xA2 }, { 0xA2 }, { 0xA2 },
	{ 0xA2 }, { 0xA2 }, { 0xA2 }, { 0xA2 }, { 0xA2 }, { 0xA2 }, { 0xA2 }, { 0xA1 },
	{ 0xA1 }, { 0xA1 }, { 0xA1 }, { 0xA1 }, { 0xA1 }, { 0xA1 }, { 0xA1 }, { 0xA1 },
	{ 0xA1 }, { 0xA1 }, { 0xA1 }, { 0xA1 }, { 0xA0 }, { 0xA0 }, { 0xA0 }, { 0xA0 },
	{ 0xA0 }, { 0xA0 }, { 0xA0 }, { 0xA0 }, { 0xA0 }, { 0xA0 }, { 0xA0 }, { 0xA0 },
	{ 0xA0 }, { 0x9F }, { 0x9F }, { 0x9F }, { 0x9F }, { 0x9F }, { 0x9F }, { 0x9F },
	{ 0x9F }, { 0x9F }, { 0x9F }, { 0x9F }, { 0x9F }, { 0x9F }, { 0x9E }, { 0x9E },
	{ 0x9E }, { 0x9E }, { 0x9E }, { 0x9E }, { 0x9E }, { 0x9E }, { 0x9E }, { 0x9E },
	{ 0x9E }, { 0x9E }, { 0x9E }, { 0x9D }, { 0x9D }, { 0x9D }, { 0x9D }, { 0x9D },
	{ 0x9D }, { 0x9D }, { 0x9D }, { 0x9D }, { 0x9D }, { 0x9D }, { 0x9D }, { 0x9D },
	{ 0x9C }, { 0x9C }, { 0x9C }, { 0x9C }, { 0x9C }, { 0x9C }, { 0x9C }, { 0x9C },
	{ 0x9C }, { 0x9C }, { 0x9C }, { 0x9C }, { 0x9C }, { 0x9B }, { 0x9B }, { 0x9B },
	{ 0x9B }, { 0x9B }, { 0x9B }, { 0x9B }, { 0x9B }, { 0x9B }, { 0x9B }, { 0x9B },
	{ 0x9B }, { 0x9B }, { 0x9A }, { 0x9A }, { 0x9A }, { 0x9A }, { 0x9A }, { 0x9A },
	{ 0x9A }, { 0x9A }, { 0x9A }, { 0x9A }, { 0x9A }, { 0x9A }, { 0x9A }, { 0x99 },
	{ 0x99 }, { 0x99 }, { 0x99 }, { 0x99 }, { 0x99 }, { 0x99 }, { 0x99 }, { 0x99 },
	{ 0x99 }, { 0x99 }, { 0x99 }, { 0x99 }, { 0x98 }, { 0x98 }, { 0x98 }, { 0x98 },
	{ 0x98 }, { 0x98 }, { 0x98 }, { 0x98 }, { 0x98 }, { 0x98 }, { 0x98 }, { 0x98 },
	{ 0x98 }, { 0x98 }, { 0x97 }, { 0x97 }, { 0x97 }, { 0x97 }, { 0x97 }, { 0x97 },
	{ 0x97 }, { 0x97 }, { 0x97 }, { 0x97 }, { 0x97 }, { 0x97 }, { 0x97 }, { 0x96 },
	{ 0x96 }, { 0x96 }, { 0x96 }, { 0x96 }, { 0x96 }, { 0x96 }, { 0x96 }, { 0x96 },
	{ 0x96 }, { 0x96 }, { 0x96 }, { 0x96 },
};

static u8 unbound1_a3_s0_elvss_cal_offset_table[][1] = {
	{ 0x00 },
};

#ifdef CONFIG_SUPPORT_HMD
static u8 unbound1_a3_s0_hmd_brt_table[S6E3FAB_HMD_TOTAL_STEP][2] = {
	/* Normal 5x51+1 */
	{ 0x01, 0x99 }, { 0x01, 0x9F }, { 0x01, 0xA6 }, { 0x01, 0xAC }, { 0x01, 0xB3 },
	{ 0x01, 0xB9 }, { 0x01, 0xC0 }, { 0x01, 0xC6 }, { 0x01, 0xCC }, { 0x01, 0xD3 },
	{ 0x01, 0xD9 }, { 0x01, 0xE0 }, { 0x01, 0xE6 }, { 0x01, 0xED }, { 0x01, 0xF3 },
	{ 0x01, 0xF9 }, { 0x02, 0x00 }, { 0x02, 0x06 }, { 0x02, 0x0D }, { 0x02, 0x13 },
	{ 0x02, 0x19 }, { 0x02, 0x20 }, { 0x02, 0x26 }, { 0x02, 0x2D }, { 0x02, 0x33 },
	{ 0x02, 0x3A }, { 0x02, 0x40 }, { 0x02, 0x46 }, { 0x02, 0x4D }, { 0x02, 0x53 },
	{ 0x02, 0x5A }, { 0x02, 0x60 }, { 0x02, 0x67 }, { 0x02, 0x6D }, { 0x02, 0x73 },
	{ 0x02, 0x7A }, { 0x02, 0x80 }, { 0x02, 0x87 }, { 0x02, 0x8D }, { 0x02, 0x94 },
	{ 0x02, 0x9A }, { 0x02, 0xA0 }, { 0x02, 0xA7 }, { 0x02, 0xAD }, { 0x02, 0xB4 },
	{ 0x02, 0xBA }, { 0x02, 0xC0 }, { 0x02, 0xC7 }, { 0x02, 0xCD }, { 0x02, 0xD4 },
	{ 0x02, 0xDA }, { 0x02, 0xE1 }, { 0x02, 0xE7 }, { 0x02, 0xED }, { 0x02, 0xF4 },
	{ 0x02, 0xFA }, { 0x03, 0x01 }, { 0x03, 0x07 }, { 0x03, 0x0E }, { 0x03, 0x14 },
	{ 0x03, 0x1A }, { 0x03, 0x21 }, { 0x03, 0x27 }, { 0x03, 0x2E }, { 0x03, 0x34 },
	{ 0x03, 0x3B }, { 0x03, 0x41 }, { 0x03, 0x47 }, { 0x03, 0x4E }, { 0x03, 0x54 },
	{ 0x03, 0x5B }, { 0x03, 0x61 }, { 0x03, 0x67 }, { 0x03, 0x6E }, { 0x03, 0x74 },
	{ 0x03, 0x7B }, { 0x03, 0x81 }, { 0x03, 0x88 }, { 0x03, 0x8E }, { 0x03, 0x94 },
	{ 0x03, 0x9B }, { 0x03, 0xA1 }, { 0x03, 0xA8 }, { 0x03, 0xAE }, { 0x03, 0xB5 },
	{ 0x03, 0xBB }, { 0x03, 0xC1 }, { 0x03, 0xC8 }, { 0x03, 0xCE }, { 0x03, 0xD5 },
	{ 0x03, 0xDB }, { 0x03, 0xE2 }, { 0x03, 0xE8 }, { 0x03, 0xEE }, { 0x03, 0xF5 },
	{ 0x03, 0xFB }, { 0x04, 0x02 }, { 0x04, 0x08 }, { 0x04, 0x0F }, { 0x04, 0x15 },
	{ 0x04, 0x1B }, { 0x04, 0x22 }, { 0x04, 0x28 }, { 0x04, 0x2F }, { 0x04, 0x35 },
	{ 0x04, 0x3B }, { 0x04, 0x42 }, { 0x04, 0x48 }, { 0x04, 0x4F }, { 0x04, 0x55 },
	{ 0x04, 0x5C }, { 0x04, 0x62 }, { 0x04, 0x68 }, { 0x04, 0x6F }, { 0x04, 0x75 },
	{ 0x04, 0x7C }, { 0x04, 0x82 }, { 0x04, 0x89 }, { 0x04, 0x8F }, { 0x04, 0x95 },
	{ 0x04, 0x9C }, { 0x04, 0xA2 }, { 0x04, 0xA9 }, { 0x04, 0xAF }, { 0x04, 0xB6 },
	{ 0x04, 0xBC }, { 0x04, 0xC2 }, { 0x04, 0xC9 }, { 0x04, 0xCF }, { 0x04, 0xD6 },
	{ 0x04, 0xDC }, { 0x04, 0xE2 }, { 0x04, 0xE9 }, { 0x04, 0xEF }, { 0x04, 0xF6 },
	{ 0x04, 0xFC }, { 0x05, 0x03 }, { 0x05, 0x09 }, { 0x05, 0x0F }, { 0x05, 0x16 },
	{ 0x05, 0x1C }, { 0x05, 0x23 }, { 0x05, 0x29 }, { 0x05, 0x30 }, { 0x05, 0x36 },
	{ 0x05, 0x3C }, { 0x05, 0x43 }, { 0x05, 0x49 }, { 0x05, 0x50 }, { 0x05, 0x56 },
	{ 0x05, 0x5D }, { 0x05, 0x63 }, { 0x05, 0x69 }, { 0x05, 0x70 }, { 0x05, 0x76 },
	{ 0x05, 0x7D }, { 0x05, 0x83 }, { 0x05, 0x89 }, { 0x05, 0x90 }, { 0x05, 0x96 },
	{ 0x05, 0x9D }, { 0x05, 0xA3 }, { 0x05, 0xAA }, { 0x05, 0xB0 }, { 0x05, 0xB6 },
	{ 0x05, 0xBD }, { 0x05, 0xC3 }, { 0x05, 0xCA }, { 0x05, 0xD0 }, { 0x05, 0xD7 },
	{ 0x05, 0xDD }, { 0x05, 0xE3 }, { 0x05, 0xEA }, { 0x05, 0xF0 }, { 0x05, 0xF7 },
	{ 0x05, 0xFD }, { 0x06, 0x04 }, { 0x06, 0x0A }, { 0x06, 0x10 }, { 0x06, 0x17 },
	{ 0x06, 0x1D }, { 0x06, 0x24 }, { 0x06, 0x2A }, { 0x06, 0x31 }, { 0x06, 0x37 },
	{ 0x06, 0x3D }, { 0x06, 0x44 }, { 0x06, 0x4A }, { 0x06, 0x51 }, { 0x06, 0x57 },
	{ 0x06, 0x5D }, { 0x06, 0x64 }, { 0x06, 0x6A }, { 0x06, 0x71 }, { 0x06, 0x77 },
	{ 0x06, 0x7E }, { 0x06, 0x84 }, { 0x06, 0x8A }, { 0x06, 0x91 }, { 0x06, 0x97 },
	{ 0x06, 0x9E }, { 0x06, 0xA4 }, { 0x06, 0xAB }, { 0x06, 0xB1 }, { 0x06, 0xB7 },
	{ 0x06, 0xBE }, { 0x06, 0xC4 }, { 0x06, 0xCB }, { 0x06, 0xD1 }, { 0x06, 0xD8 },
	{ 0x06, 0xDE }, { 0x06, 0xE4 }, { 0x06, 0xEB }, { 0x06, 0xF1 }, { 0x06, 0xF8 },
	{ 0x06, 0xFE }, { 0x07, 0x04 }, { 0x07, 0x0B }, { 0x07, 0x11 }, { 0x07, 0x18 },
	{ 0x07, 0x1E }, { 0x07, 0x25 }, { 0x07, 0x2B }, { 0x07, 0x31 }, { 0x07, 0x38 },
	{ 0x07, 0x3E }, { 0x07, 0x45 }, { 0x07, 0x4B }, { 0x07, 0x52 }, { 0x07, 0x58 },
	{ 0x07, 0x5E }, { 0x07, 0x65 }, { 0x07, 0x6B }, { 0x07, 0x72 }, { 0x07, 0x78 },
	{ 0x07, 0x7F }, { 0x07, 0x85 }, { 0x07, 0x8B }, { 0x07, 0x92 }, { 0x07, 0x98 },
	{ 0x07, 0x9F }, { 0x07, 0xA5 }, { 0x07, 0xAB }, { 0x07, 0xB2 }, { 0x07, 0xB8 },
	{ 0x07, 0xBF }, { 0x07, 0xC5 }, { 0x07, 0xCC }, { 0x07, 0xD2 }, { 0x07, 0xD8 },
	{ 0x07, 0xDF }, { 0x07, 0xE5 }, { 0x07, 0xEC }, { 0x07, 0xF2 }, { 0x07, 0xF9 },
	{ 0x07, 0xFF },
};
#endif

static u8 unbound1_a3_s0_hbm_and_transition_table[MAX_PANEL_HBM][SMOOTH_TRANS_MAX][1] = {
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

static u8 unbound1_a3_s0_acl_onoff_table[][1] = {
	{ 0x00 }, /* acl off */
	{ 0x01 }, /* acl on */
};

static u8 unbound1_a3_s0_acl_opr_table[MAX_PANEL_HBM][MAX_S6E3FAB_ACL_OPR][2] = {
	[PANEL_HBM_OFF] = {
		[S6E3FAB_ACL_OPR_0] = { 0x02, 0x61 }, /* adaptive_control 0, 8% */
		[S6E3FAB_ACL_OPR_1] = { 0x04, 0x91 }, /* adaptive_control 1, 15% */
		[S6E3FAB_ACL_OPR_2] = { 0x09, 0x91 }, /* adaptive_control 2, 30% */
	},
	[PANEL_HBM_ON] = {
		[S6E3FAB_ACL_OPR_0 ... S6E3FAB_ACL_OPR_2] = { 0x02, 0x61 },
	},
};

static u8 unbound1_a3_s0_lpm_on_table[4][1] = {
	/* LPM 2NIT */
	{ 0x23 },
	/* LPM 10NIT */
	{ 0x22 },
	/* LPM 30NIT */
	{ 0x22 },
	/* LPM 60NIT */
	{ 0x22 },
};

static u8 unbound1_a3_s0_lpm_nit_table[4][3] = {
	/* LPM 2NIT */
	{ 0x08, 0x08, 0x40 },
	/* LPM 10NIT */
	{ 0x08, 0x08, 0x40 },
	/* LPM 30NIT */
	{ 0x05, 0x08, 0x40 },
	/* LPM 60NIT */
	{ 0x00, 0x18, 0x00 },
};

static u8 unbound1_a3_s0_lpm_wrdisbv_table[4][2] = {
	/* LPM 2NIT */
	{ 0x00, 0x08 },
	/* LPM 10NIT */
	{ 0x00, 0x29 },
	/* LPM 30NIT */
	{ 0x00, 0x7D },
	/* LPM 60NIT */
	{ 0x00, 0xF5 },
};

static u8 unbound1_a3_s0_lpm_mode_table[2][1] = {
	[ALPM_MODE] = { 0x20 },
	[HLPM_MODE] = { 0x00 },
};

static u8 unbound1_a3_s0_lpm_off_table[][1] = {
	/* LPM 2NIT */
	{ 0x21 },
	/* LPM 10NIT */
	{ 0x20 },
	/* LPM 30NIT */
	{ 0x20 },
	/* LPM 60NIT */
	{ 0x20 },
};

static u8 unbound1_a3_s0_dia_onoff_table[][1] = {
	{ 0x01 }, /* dia off */
	{ 0x02 }, /* dia on */
};

static u8 unbound1_a3_s0_irc_mode_table[][1] = {
	{ 0x6F },	/* moderato */
	{ 0x2F },	/* flat gamma */
};

static u8 unbound1_a3_s0_aid_table[][1] = {
	[S6E3FAB_VRR_MODE_NS] = { 0x41 },
	[S6E3FAB_VRR_MODE_HS] = { 0x21 },
};

static u8 unbound1_a3_s0_osc_table[][1] = {
	[S6E3FAB_VRR_MODE_NS] = { 0xB2 },
	[S6E3FAB_VRR_MODE_HS] = { 0xB0 },
};

static u8 unbound1_a3_s0_fps_table[][1] = {
	[S6E3FAB_VRR_60NS] = { 0x00 },
	[S6E3FAB_VRR_48NS] = { 0x00 },
	[S6E3FAB_VRR_120HS] = { 0x20 },
	[S6E3FAB_VRR_60HS_120HS_TE_HW_SKIP_1] = { 0x20 },
	[S6E3FAB_VRR_96HS] = { 0x20 },
	[S6E3FAB_VRR_48HS_96HS_TE_HW_SKIP_1] = { 0x20 },
	[S6E3FAB_VRR_60HS] = { 0x00 },
	[S6E3FAB_VRR_48HS] = { 0x00 },
};

static u8 unbound1_a3_s0_vfp_fq_low_table[][2] = {
	[S6E3FAB_VRR_60NS] = { 0x00, 0x10 },
	[S6E3FAB_VRR_48NS] = { 0x02, 0x70 },
	[S6E3FAB_VRR_120HS] = { 0x09, 0x90 },
	[S6E3FAB_VRR_60HS_120HS_TE_HW_SKIP_1] = { 0x00, 0x10 },
	[S6E3FAB_VRR_96HS] = { 0x00, 0x10 },
	[S6E3FAB_VRR_48HS_96HS_TE_HW_SKIP_1] = { 0x00, 0x10 },
	[S6E3FAB_VRR_60HS] = { 0x09, 0x90 },
	[S6E3FAB_VRR_48HS] = { 0x0E, 0x50 },
};

static u8 unbound1_a3_s0_vfp_fq_high_table[][2] = {
	[S6E3FAB_VRR_60NS] = { 0x00, 0x10 },
	[S6E3FAB_VRR_48NS] = { 0x00, 0x10 },
	[S6E3FAB_VRR_120HS] = { 0x00, 0x10 },
	[S6E3FAB_VRR_60HS_120HS_TE_HW_SKIP_1] = { 0x00, 0x10 },
	[S6E3FAB_VRR_96HS] = { 0x02, 0x70 },
	[S6E3FAB_VRR_48HS_96HS_TE_HW_SKIP_1] = { 0x02, 0x70 },
	[S6E3FAB_VRR_60HS] = { 0x00, 0x10 },
	[S6E3FAB_VRR_48HS] = { 0x00, 0x10 },
};

static u8 unbound1_a3_s0_osc_0_table[][2] = {
	[S6E3FAB_VRR_MODE_NS] = { 0x14, 0xAB },
	[S6E3FAB_VRR_MODE_HS] = { 0x14, 0xAB },
	[S6E3FAB_VRR_MODE_NS_OSC1] = { 0x14, 0x3D },
	[S6E3FAB_VRR_MODE_HS_OSC1] = { 0x14, 0x3D },
};

static u8 unbound1_a3_s0_osc_1_table[][2] = {
	[S6E3FAB_VRR_MODE_NS] = { 0x14, 0xAB },
	[S6E3FAB_VRR_MODE_HS] = { 0x14, 0xAB },
	[S6E3FAB_VRR_MODE_NS_OSC1] = { 0x14, 0x3D },
	[S6E3FAB_VRR_MODE_HS_OSC1] = { 0x14, 0x3D },
};

static u8 unbound1_a3_s0_ltps_0_table[][8] = {
	[S6E3FAB_VRR_MODE_NS] = { 0x11, 0x89, 0x11, 0x89, 0x11, 0x89, 0x11, 0x89 },
	[S6E3FAB_VRR_MODE_HS] = { 0x16, 0x77, 0x16, 0x77, 0x16, 0x77, 0x16, 0x77 },
	[S6E3FAB_VRR_MODE_NS_OSC1] = { 0x11, 0x86, 0x11, 0x86, 0x11, 0x86, 0x11, 0x86 },
	[S6E3FAB_VRR_MODE_HS_OSC1] = { 0x16, 0x75, 0x16, 0x75, 0x16, 0x75, 0x16, 0x75 },
};

static u8 unbound1_a3_s0_ltps_1_table[][8] = {
	[S6E3FAB_VRR_MODE_NS] = { 0x0C, 0x75, 0x0C, 0x75, 0x0C, 0x75, 0x0C, 0x75 },
	[S6E3FAB_VRR_MODE_HS] = { 0x18, 0x45, 0x18, 0x45, 0x18, 0x45, 0x18, 0x45 },
	[S6E3FAB_VRR_MODE_NS_OSC1] = { 0x0C, 0x73, 0x0C, 0x73, 0x0C, 0x73, 0x0C, 0x73 },
	[S6E3FAB_VRR_MODE_HS_OSC1] = { 0x18, 0x44, 0x18, 0x44, 0x18, 0x44, 0x18, 0x44 },
};

static u8 unbound1_a3_s0_src_amp_table[][1] = {
	[S6E3FAB_VRR_MODE_NS] = { 0x01 },
	[S6E3FAB_VRR_MODE_HS] = { 0x81 },
};

static u8 unbound1_a3_s0_aor_table[][1] = {
	[S6E3FAB_VRR_60NS] = { 0x00 },
	[S6E3FAB_VRR_48NS] = { 0x03 },
	[S6E3FAB_VRR_120HS] = { 0x00 },
	[S6E3FAB_VRR_60HS_120HS_TE_HW_SKIP_1] = { 0x00 },
	[S6E3FAB_VRR_96HS] = { 0x03 },
	[S6E3FAB_VRR_48HS_96HS_TE_HW_SKIP_1] = { 0x03 },
	[S6E3FAB_VRR_60HS] = { 0x00 },
	[S6E3FAB_VRR_48HS] = { 0x03 },
};

static u8 unbound1_a3_s0_aor_fq_low_table[][S6E3FAB_TOTAL_STEP][2] = {
	[S6E3FAB_VRR_60NS] = {
		[0 ... S6E3FAB_TOTAL_STEP - 1] = { 0x09, 0x50 },
	},
	[S6E3FAB_VRR_48NS] = {
		[0 ... S6E3FAB_TOTAL_STEP - 1] = { 0x09, 0x50 },
	},
	[S6E3FAB_VRR_120HS] = {
		[0 ... S6E3FAB_TOTAL_STEP - 1] = { 0x09, 0x50 },
	},
	[S6E3FAB_VRR_60HS_120HS_TE_HW_SKIP_1] = {
		[0 ... S6E3FAB_TOTAL_STEP - 1] = { 0x09, 0x50 },
	},
	[S6E3FAB_VRR_96HS] = {
		[0 ... 1] = { 0x09, 0x4F },
		[2 ... S6E3FAB_TOTAL_STEP - 1] = { 0x09, 0x50 },
	},
	[S6E3FAB_VRR_48HS_96HS_TE_HW_SKIP_1] = {
		[0 ... 1] = { 0x09, 0x4F },
		[2 ... S6E3FAB_TOTAL_STEP - 1] = { 0x09, 0x50 },
	},
	[S6E3FAB_VRR_60HS] = {
		[0 ... S6E3FAB_TOTAL_STEP - 1] = { 0x09, 0x50 },
	},
	[S6E3FAB_VRR_48HS] = {
		[0 ... S6E3FAB_TOTAL_STEP - 1] = { 0x09, 0x50 },
	},
};

static u8 unbound1_a3_s0_aor_fq_high_table[][S6E3FAB_TOTAL_STEP][2] = {
	[S6E3FAB_VRR_60NS] = {
		[0 ... S6E3FAB_TOTAL_STEP - 1] = { 0x09, 0x50 },
	},
	[S6E3FAB_VRR_48NS] = {
		[0 ... S6E3FAB_TOTAL_STEP - 1] = { 0x09, 0x50 },
	},
	[S6E3FAB_VRR_120HS] = {
		[0 ... S6E3FAB_TOTAL_STEP - 1] = { 0x09, 0x50 },
	},
	[S6E3FAB_VRR_60HS_120HS_TE_HW_SKIP_1] = {
		[0 ... S6E3FAB_TOTAL_STEP - 1] = { 0x09, 0x50 },
	},
	[S6E3FAB_VRR_96HS] = {
		[0 ... 1] = { 0x09, 0x4F },
		[2 ... S6E3FAB_TOTAL_STEP - 1] = { 0x09, 0x50 },
	},
	[S6E3FAB_VRR_48HS_96HS_TE_HW_SKIP_1] = {
		[0 ... 1] = { 0x09, 0x4F },
		[2 ... S6E3FAB_TOTAL_STEP - 1] = { 0x09, 0x50 },
	},
	[S6E3FAB_VRR_60HS] = {
		[0 ... S6E3FAB_TOTAL_STEP - 1] = { 0x09, 0x50 },
	},
	[S6E3FAB_VRR_48HS] = {
		[0 ... S6E3FAB_TOTAL_STEP - 1] = { 0x09, 0x50 },
	},
};

static u8 unbound1_a3_s0_te_frame_sel_table[MAX_S6E3FAB_VRR][1] = {
	[S6E3FAB_VRR_60NS] = { 0x09 },
	[S6E3FAB_VRR_48NS] = { 0x09 },
	[S6E3FAB_VRR_120HS] = { 0x09 },
	[S6E3FAB_VRR_60HS_120HS_TE_HW_SKIP_1] = { 0x19 },
	[S6E3FAB_VRR_96HS] = { 0x09 },
	[S6E3FAB_VRR_48HS_96HS_TE_HW_SKIP_1] = { 0x19 },
	[S6E3FAB_VRR_60HS] = { 0x09 },
	[S6E3FAB_VRR_48HS] = { 0x09 },
};

#if defined(CONFIG_SUPPORT_FAST_DISCHARGE)
static u8 unbound1_a3_s0_fast_discharge_table[][1] = {
	{ 0x00 },	//fd 0ff
	{ 0x40 },	//fd on
};
#endif

#ifdef CONFIG_SUPPORT_GRAM_CHECKSUM
static u8 unbound1_a3_s0_vddm_table[][1] = {
	{0x00}, // VDDM ORIGINAL
	{0x12}, // VDDM LOW VOLTAGE
	{0x31}, // VDDM HIGH VOLTAGE
};
static u8 unbound1_a3_s0_gram_img_pattern_table[][1] = {
	{0x00}, // GCT_PATTERN_NONE
	{0x07}, // GCT_PATTERN_1
	{0x05}, // GCT_PATTERN_2
};
/* switch pattern_1 and pattern_2 */
static u8 unbound1_a3_s0_gram_inv_img_pattern_table[][1] = {
	{0x00}, // GCT_PATTERN_NONE
	{0x05}, // GCT_PATTERN_2
	{0x07}, // GCT_PATTERN_1
};
#endif
#ifdef CONFIG_SUPPORT_XTALK_MODE
static u8 unbound1_a3_s0_vgh_table[][1] = {
	{ 0x0C }, /* VGH (x-talk off)*/
	{ 0x05 }, /* VGH (x-talk on) */
};
#endif

static char UNBOUND1_A3_S0_FFC_DEFAULT[] = {
	0xC5,
	0x11, 0x10, 0x50, 0x2D, 0x58, 0x2D, 0x58,
};

static u8 unbound1_a3_s0_gamma_mtp_0_table[MAX_S6E3FAB_VRR][MAX_S6E3FAB_UNBOUND1_GAMMA_BR_INDEX][S6E3FAB_GAMMA_0_LEN];
static u8 unbound1_a3_s0_gamma_mtp_1_table[MAX_S6E3FAB_VRR][MAX_S6E3FAB_UNBOUND1_GAMMA_BR_INDEX][S6E3FAB_GAMMA_1_LEN];
static u8 unbound1_a3_s0_gamma_mtp_2_table[MAX_S6E3FAB_VRR][MAX_S6E3FAB_UNBOUND1_GAMMA_BR_INDEX][S6E3FAB_GAMMA_2_LEN];
static u8 unbound1_a3_s0_gamma_mtp_3_table[MAX_S6E3FAB_VRR][MAX_S6E3FAB_UNBOUND1_GAMMA_BR_INDEX][S6E3FAB_GAMMA_3_LEN];
static u8 unbound1_a3_s0_gamma_mtp_4_table[MAX_S6E3FAB_VRR][MAX_S6E3FAB_UNBOUND1_GAMMA_BR_INDEX][S6E3FAB_GAMMA_4_LEN];

#ifdef CONFIG_SUPPORT_MAFPC
static u8 unbound1_a3_s0_mafpc_ena_table[][1] = {
	{ 0x00 },
	{ 0x11 },
};
#endif

static struct maptbl unbound1_a3_s0_maptbl[MAX_MAPTBL] = {
	[GAMMA_MODE2_MAPTBL] = DEFINE_2D_MAPTBL(unbound1_a3_s0_brt_table, init_gamma_mode2_brt_table, getidx_gamma_mode2_brt_table, copy_common_maptbl),
#ifdef CONFIG_SUPPORT_HMD
	[HMD_GAMMA_MAPTBL] = DEFINE_2D_MAPTBL(unbound1_a3_s0_hmd_brt_table, init_gamma_mode2_hmd_brt_table, getidx_gamma_mode2_hmd_brt_table, copy_common_maptbl),
#endif
	[TSET_MAPTBL] = DEFINE_0D_MAPTBL(unbound1_a3_s0_tset_table, init_common_table, NULL, copy_tset_maptbl),
	[HBM_AND_TRANSITION_MAPTBL] = DEFINE_3D_MAPTBL(unbound1_a3_s0_hbm_and_transition_table, init_common_table, getidx_hbm_and_transition_table, copy_common_maptbl),
	[ACL_ONOFF_MAPTBL] = DEFINE_2D_MAPTBL(unbound1_a3_s0_acl_onoff_table, init_common_table, getidx_acl_onoff_table, copy_common_maptbl),
	[ACL_OPR_MAPTBL] = DEFINE_3D_MAPTBL(unbound1_a3_s0_acl_opr_table, init_common_table, getidx_acl_opr_table, copy_common_maptbl),
	[ELVSS_MAPTBL] = DEFINE_2D_MAPTBL(unbound1_a3_s0_elvss_table, init_common_table, getidx_gm2_elvss_table, copy_common_maptbl),
	[ELVSS_CAL_OFFSET_MAPTBL] = DEFINE_2D_MAPTBL(unbound1_a3_s0_elvss_cal_offset_table, init_elvss_cal_offset_table, getidx_common_maptbl, copy_common_maptbl),
	[DIA_ONOFF_MAPTBL] = DEFINE_2D_MAPTBL(unbound1_a3_s0_dia_onoff_table, init_common_table, getidx_dia_onoff_table, copy_common_maptbl),
	[IRC_MODE_MAPTBL] = DEFINE_2D_MAPTBL(unbound1_a3_s0_irc_mode_table, init_common_table, getidx_irc_mode_table, copy_common_maptbl),
	[LPM_ON_MAPTBL] = DEFINE_2D_MAPTBL(unbound1_a3_s0_lpm_on_table, init_common_table, getidx_lpm_brt_table, copy_common_maptbl),
	[LPM_NIT_MAPTBL] = DEFINE_2D_MAPTBL(unbound1_a3_s0_lpm_nit_table, init_lpm_brt_table, getidx_lpm_brt_table, copy_common_maptbl),
	[LPM_MODE_MAPTBL] = DEFINE_2D_MAPTBL(unbound1_a3_s0_lpm_mode_table, init_common_table, getidx_lpm_mode_table, copy_common_maptbl),
	[LPM_OFF_MAPTBL] = DEFINE_2D_MAPTBL(unbound1_a3_s0_lpm_off_table, init_common_table, getidx_lpm_brt_table, copy_common_maptbl),
	[LPM_WRDISBV_MAPTBL] = DEFINE_2D_MAPTBL(unbound1_a3_s0_lpm_wrdisbv_table, init_common_table, getidx_lpm_brt_table, copy_common_maptbl),
	[FPS_MAPTBL] = DEFINE_2D_MAPTBL(unbound1_a3_s0_fps_table, init_common_table, getidx_vrr_table, copy_common_maptbl),
	[AID_MAPTBL] = DEFINE_2D_MAPTBL(unbound1_a3_s0_aid_table, init_common_table, getidx_vrr_mode_table, copy_common_maptbl),
	[OSC_MAPTBL] = DEFINE_2D_MAPTBL(unbound1_a3_s0_osc_table, init_common_table, getidx_vrr_mode_table, copy_common_maptbl),
	[VFP_FQ_LOW_MAPTBL] = DEFINE_2D_MAPTBL(unbound1_a3_s0_vfp_fq_low_table, init_common_table, getidx_vrr_table, copy_common_maptbl),
	[VFP_FQ_HIGH_MAPTBL] = DEFINE_2D_MAPTBL(unbound1_a3_s0_vfp_fq_high_table, init_common_table, getidx_vrr_table, copy_common_maptbl),
	[OSC_0_MAPTBL] = DEFINE_2D_MAPTBL(unbound1_a3_s0_osc_0_table, init_common_table, getidx_vrr_osc_mode_table, copy_common_maptbl),
	[OSC_1_MAPTBL] = DEFINE_2D_MAPTBL(unbound1_a3_s0_osc_1_table, init_common_table, getidx_vrr_osc_mode_table, copy_common_maptbl),
	[LTPS_0_MAPTBL] = DEFINE_2D_MAPTBL(unbound1_a3_s0_ltps_0_table, init_common_table, getidx_vrr_osc_mode_table, copy_common_maptbl),
	[LTPS_1_MAPTBL] = DEFINE_2D_MAPTBL(unbound1_a3_s0_ltps_1_table, init_common_table, getidx_vrr_osc_mode_table, copy_common_maptbl),
	[SRC_AMP_MAPTBL] = DEFINE_2D_MAPTBL(unbound1_a3_s0_src_amp_table, init_common_table, getidx_vrr_mode_table, copy_common_maptbl),
	[AOR_MAPTBL] = DEFINE_2D_MAPTBL(unbound1_a3_s0_aor_table, init_common_table, getidx_vrr_table, copy_common_maptbl),
	[AOR_FQ_LOW_MAPTBL] = DEFINE_3D_MAPTBL(unbound1_a3_s0_aor_fq_low_table, init_common_table, getidx_vrr_aor_fq_brt_table, copy_common_maptbl),
	[AOR_FQ_HIGH_MAPTBL] = DEFINE_3D_MAPTBL(unbound1_a3_s0_aor_fq_high_table, init_common_table, getidx_vrr_aor_fq_brt_table, copy_common_maptbl),
	[TE_FRAME_SEL_MAPTBL] = DEFINE_2D_MAPTBL(unbound1_a3_s0_te_frame_sel_table, init_common_table, getidx_vrr_table, copy_common_maptbl),
#ifdef CONFIG_SUPPORT_GRAM_CHECKSUM
	[VDDM_MAPTBL] = DEFINE_2D_MAPTBL(unbound1_a3_s0_vddm_table, init_common_table, s6e3fab_getidx_vddm_table, copy_common_maptbl),
	[GRAM_IMG_MAPTBL] = DEFINE_2D_MAPTBL(unbound1_a3_s0_gram_img_pattern_table, init_common_table, s6e3fab_getidx_gram_img_pattern_table, copy_common_maptbl),
	[GRAM_INV_IMG_MAPTBL] = DEFINE_2D_MAPTBL(unbound1_a3_s0_gram_inv_img_pattern_table, init_common_table, s6e3fab_getidx_gram_img_pattern_table, copy_common_maptbl),
#endif
#ifdef CONFIG_SUPPORT_XTALK_MODE
	[VGH_MAPTBL] = DEFINE_2D_MAPTBL(unbound1_a3_s0_vgh_table, init_common_table, getidx_vgh_table, copy_common_maptbl),
#endif
#ifdef CONFIG_DYNAMIC_MIPI
	[DM_SET_FFC_MAPTBL] = DEFINE_1D_MAPTBL(UNBOUND1_A3_S0_FFC_DEFAULT, ARRAY_SIZE(UNBOUND1_A3_S0_FFC_DEFAULT), init_common_table, NULL, dynamic_mipi_set_ffc),
	[DM_DDI_OSC_MAPTBL] = DEFINE_0D_MAPTBL(unbound1_a3_s0_ddi_osc, init_common_table, NULL, dynamic_mipi_set_ddi_osc),
#endif
	[GAMMA_MTP_0_MAPTBL] = DEFINE_3D_MAPTBL(unbound1_a3_s0_gamma_mtp_0_table, init_gamma_mtp_all_table, getidx_gamma_brt_index_table, copy_common_maptbl),
	[GAMMA_MTP_1_MAPTBL] = DEFINE_3D_MAPTBL(unbound1_a3_s0_gamma_mtp_1_table, init_common_table, getidx_gamma_brt_index_table, copy_common_maptbl),
	[GAMMA_MTP_2_MAPTBL] = DEFINE_3D_MAPTBL(unbound1_a3_s0_gamma_mtp_2_table, init_common_table, getidx_gamma_brt_index_table, copy_common_maptbl),
	[GAMMA_MTP_3_MAPTBL] = DEFINE_3D_MAPTBL(unbound1_a3_s0_gamma_mtp_3_table, init_common_table, getidx_gamma_brt_index_table, copy_common_maptbl),
	[GAMMA_MTP_4_MAPTBL] = DEFINE_3D_MAPTBL(unbound1_a3_s0_gamma_mtp_4_table, init_common_table, getidx_gamma_brt_index_table, copy_common_maptbl),
#ifdef CONFIG_SUPPORT_MAFPC
	[MAFPC_ENA_MAPTBL] = DEFINE_0D_MAPTBL(unbound1_a3_s0_mafpc_enable, init_common_table, NULL, copy_mafpc_enable_maptbl),
	[MAFPC_CTRL_MAPTBL] = DEFINE_0D_MAPTBL(unbound1_a3_s0_mafpc_ctrl, init_common_table, NULL, copy_mafpc_ctrl_maptbl),
	[MAFPC_SCALE_MAPTBL] = DEFINE_0D_MAPTBL(unbound1_a3_s0_mafpc_scale, init_common_table, NULL, copy_mafpc_scale_maptbl),
#endif
#if defined(CONFIG_SUPPORT_FAST_DISCHARGE)
	[FAST_DISCHARGE_MAPTBL] = DEFINE_2D_MAPTBL(unbound1_a3_s0_fast_discharge_table, init_common_table, getidx_fast_discharge_table, copy_common_maptbl),
#endif

};

/* ===================================================================================== */
/* ============================== [S6E3FAB COMMAND TABLE] ============================== */
/* ===================================================================================== */
static u8 UNBOUND1_A3_S0_KEY1_ENABLE[] = { 0x9F, 0xA5, 0xA5 };
static u8 UNBOUND1_A3_S0_KEY2_ENABLE[] = { 0xF0, 0x5A, 0x5A };
static u8 UNBOUND1_A3_S0_KEY3_ENABLE[] = { 0xFC, 0x5A, 0x5A };
static u8 UNBOUND1_A3_S0_KEY1_DISABLE[] = { 0x9F, 0x5A, 0x5A };
static u8 UNBOUND1_A3_S0_KEY2_DISABLE[] = { 0xF0, 0xA5, 0xA5 };
static u8 UNBOUND1_A3_S0_KEY3_DISABLE[] = { 0xFC, 0xA5, 0xA5 };
static u8 UNBOUND1_A3_S0_SLEEP_OUT[] = { 0x11 };
static u8 UNBOUND1_A3_S0_SLEEP_IN[] = { 0x10 };
static u8 UNBOUND1_A3_S0_DISPLAY_OFF[] = { 0x28 };
static u8 UNBOUND1_A3_S0_DISPLAY_ON[] = { 0x29 };

static u8 UNBOUND1_A3_S0_DSC[] = { 0x01 };
static u8 UNBOUND1_A3_S0_PPS[] = {
	// FHD : 1080x2400
	0x11, 0x00, 0x00, 0x89, 0x30, 0x80, 0x09, 0x60,
	0x04, 0x38, 0x00 ,0x78, 0x02, 0x1C, 0x02, 0x1C,
	0x02, 0x00, 0x02, 0x0E, 0x00, 0x20, 0x0B, 0xAF,
	0x00, 0x07, 0x00, 0x0C, 0x00, 0xCF, 0x00, 0xD9,
	0x18, 0x00, 0x10, 0xF0, 0x03, 0x0C, 0x20, 0x00,
	0x06, 0x0B, 0x0B, 0x33, 0x0E, 0x1C, 0x2A, 0x38,
	0x46, 0x54, 0x62, 0x69, 0x70, 0x77, 0x79, 0x7B,
	0x7D, 0x7E, 0x01, 0x02, 0x01, 0x00, 0x09, 0x40,
	0x09, 0xBE, 0x19, 0xFC, 0x19, 0xFA, 0x19, 0xF8,
	0x1A ,0x38, 0x1A, 0x78, 0x1A, 0xB6, 0x2A, 0xF6,
	0x2B, 0x34, 0x2B, 0x74, 0x3B, 0x74, 0x6B, 0xF4,
	0x00
};

static u8 UNBOUND1_A3_S0_TE_ON[] = { 0x35, 0x00 };

static u8 UNBOUND1_A3_S0_TSP_HSYNC[] = {
	0xB9,
	0x00,   /* to be updated by TE_FRAME_SEL_MAPTBL */
	0x90, 0x61, 0x09, 0x00, 0x00, 0x00, 0x11, 0x03
};

static u8 UNBOUND1_A3_S0_SET_TE_FRAME_SEL[] = {
	0xB9,
	0x09,   /* to be updated by TE_FRAME_SEL_MAPTBL */
	0x90, 0x61, 0x09,
};

static u8 UNBOUND1_A3_S0_CLR_TE_FRAME_SEL[] = {
	0xB9,
	0x08,   /* to be updated by TE_FRAME_SEL_MAPTBL */
};

static u8 UNBOUND1_A3_S0_ELVSS[] = {
	0xC6,
	0x96,
};

static u8 UNBOUND1_A3_S0_SMOOTH_DIMMING_INIT[] = {
	0xC6,
	0x01,
};

static u8 UNBOUND1_A3_S0_SMOOTH_DIMMING[] = {
	0xC6,
	0x18,
};

static u8 UNBOUND1_A3_S0_TSET[] = {
	0xC6,
	0x19,	/* temperature 25 */
};

static u8 UNBOUND1_A3_S0_DUMMY_2C[] = {
	0x2C, 0x00, 0x00, 0x00
};

static u8 UNBOUND1_A3_S0_FAST_DISCHARGE[] = {
	0xB1, 0x40
};

static u8 UNBOUND1_A3_S0_ERR_FG_1[] = {
	0xF4, 0x31
};

static u8 UNBOUND1_A3_S0_ERR_FG_2[] = {
	0xED, 0x04, 0x40, 0x20
};

static u8 UNBOUND1_A3_S0_ISC[] = {
	0xF6, 0x40
};

static u8 UNBOUND1_A3_S0_GAMMA_NORMALIZE[] = {
	0x69, 0x05
};

static u8 UNBOUND1_A3_S0_LPM_ENTER_AOR[] = {
	0x9A, 0x13, 0x00,
};

static u8 UNBOUND1_A3_S0_LPM_EXIT_AOR[] = {
	0x9A, 0x01, 0x86,
};

static u8 UNBOUND1_A3_S0_LPM_ENTER_OSC[] = {
	0xF2, 0xB2
};

static u8 UNBOUND1_A3_S0_LPM_ENTER_ELVSS[] = {
	0xC6, 0x01, 0x77, 0xFF, 0x0D, 0xC0, 0x2F
};

static u8 UNBOUND1_A3_S0_LPM_ENTER_VFP_60NS[] = {
	0xF2, 0x00, 0x10
};

static u8 UNBOUND1_A3_S0_LPM_EXIT_ELVSS[] = {
	0xC6, 0x01, 0x77, 0xFF, 0x0D, 0xC0, 0x96, 0x01
};

static u8 UNBOUND1_A3_S0_LPM_ENTER_FPS[] = {
	0x60, 0x00
};

static u8 UNBOUND1_A3_S0_LPM_ENTER_SEAMLESS_CTRL[] = {
	0xBB, 0x28
};

static u8 UNBOUND1_A3_S0_LPM_EXIT_SEAMLESS_CTRL[] = {
	0xBB, 0x20
};

static u8 UNBOUND1_A3_S0_LPM_NIT[] = {
	0x9A,
	0x08, 0x08, 0x40
};

static u8 UNBOUND1_A3_S0_LPM_MODE[] = {
	0x69, 0x00
};

static u8 UNBOUND1_A3_S0_LPM_AVS_ON[] = {
	0xFD, 0xC0
};

static u8 UNBOUND1_A3_S0_LPM_ON[] = {
	0x53, 0x23
};

static u8 UNBOUND1_A3_S0_LPM_AVC2_OFF[] = {
	0xF4, 0x8A
};

static u8 UNBOUND1_A3_S0_LPM_AVC2_ON[] = {
	0xF4, 0xCA
};

static u8 UNBOUND1_A3_S0_LPM_OFF[] = {
	0x53, 0x21
};

static u8 UNBOUND1_A3_S0_LPM_EXIT_SWIRE_NO_PULSE[] = {
	0xB1, 0x00
};

#ifdef CONFIG_SUPPORT_DDI_CMDLOG
static u8 UNBOUND1_A3_S0_CMDLOG_ENABLE[] = { 0xF7, 0x80 };
static u8 UNBOUND1_A3_S0_CMDLOG_DISABLE[] = { 0xF7, 0x00 };
static u8 UNBOUND1_A3_S0_GAMMA_UPDATE_ENABLE[] = { 0xF7, 0x8F };
#else
static u8 UNBOUND1_A3_S0_GAMMA_UPDATE_ENABLE[] = { 0xF7, 0x0F };
#endif

#ifdef CONFIG_SUPPORT_DYNAMIC_HLPM
static u8 UNBOUND1_A3_S0_DYNAMIC_HLPM_ENABLE[] = {
	0x85,
	0x03, 0x0B, 0x01, 0x01, 0x00, 0x00, 0x00, 0x00, 0x02, 0x26, 0x02, 0xB2, 0x07, 0xBC, 0x09, 0xEB
};

static u8 UNBOUND1_A3_S0_DYNAMIC_HLPM_DISABLE[] = {
	0x85,
	0x00
};
#endif

static u8 UNBOUND1_A3_S0_MCD_ON_01[] = { 0xF4, 0x0E };
static u8 UNBOUND1_A3_S0_MCD_ON_02[] = { 0xCB, 0x0F, 0x00, 0x02 };
static u8 UNBOUND1_A3_S0_MCD_ON_03[] = { 0xCB, 0x79 };
static u8 UNBOUND1_A3_S0_MCD_ON_04[] = { 0xF6, 0x00 };
static u8 UNBOUND1_A3_S0_MCD_ON_05[] = { 0xCC, 0x12 };

static u8 UNBOUND1_A3_S0_MCD_OFF_01[] = { 0xF4, 0x0C };
static u8 UNBOUND1_A3_S0_MCD_OFF_02[] = { 0xCB, 0x0B, 0x00, 0x06 };
static u8 UNBOUND1_A3_S0_MCD_OFF_03[] = { 0xCB, 0x00 };
static u8 UNBOUND1_A3_S0_MCD_OFF_04[] = { 0xF6, 0x92 };
static u8 UNBOUND1_A3_S0_MCD_OFF_05[] = { 0xCC, 0x00 };

#ifdef CONFIG_SUPPORT_GRAYSPOT_TEST
static u8 UNBOUND1_A3_S0_GRAYSPOT_ON_01[] = {
	0xF6, 0x00
};
static u8 UNBOUND1_A3_S0_GRAYSPOT_ON_02[] = {
	0xF2, 0xB0, 0x00, 0x00
};
static u8 UNBOUND1_A3_S0_GRAYSPOT_ON_03[] = {
	0xC6, 0x19
};
static u8 UNBOUND1_A3_S0_GRAYSPOT_ON_04[] = {
	0xC6,
	0x01, 0x77, 0xFF, 0x0D, 0xC0, 0x16, 0x04, 0x5F
};

static u8 UNBOUND1_A3_S0_GRAYSPOT_OFF_01[] = {
	0xF6, 0x92
};
static u8 UNBOUND1_A3_S0_GRAYSPOT_OFF_02[] = {
	0xF2, 0xB0, 0x00, 0x87
};
static u8 UNBOUND1_A3_S0_GRAYSPOT_OFF_03[] = {
	0xC6, 0x00
};
static u8 UNBOUND1_A3_S0_GRAYSPOT_OFF_04[] = {
	0xC6,
	0x01, 0x77, 0xFF, 0x0D, 0xC0, 0x16, 0x04, 0x00
};
#endif

/* Micro Crack Test Sequence */
#ifdef CONFIG_SUPPORT_MST
static u8 UNBOUND1_A3_S0_MST_ON_01[] = {
	0xCB,
	0x2A, 0x64, 0x2A, 0x64, 0x2A, 0x64, 0x2A, 0x64,
};
static u8 UNBOUND1_A3_S0_MST_ON_02[] = {
	0xF6,
	0xFF, 0x92, 0x94, 0x9C, 0x82
};
static u8 UNBOUND1_A3_S0_MST_ON_03[] = {
	0xBF,
	0x33, 0x25, 0xFF, 0x00, 0x00, 0x10
};

static u8 UNBOUND1_A3_S0_MST_OFF_01[] = {
	0xCB,
	0x16, 0x77, 0x16, 0x77, 0x16, 0x77, 0x16, 0x77,
};
static u8 UNBOUND1_A3_S0_MST_OFF_02[] = {
	0xF6,
	0xFF, 0x92, 0x94, 0x9C, 0x92
};
static u8 UNBOUND1_A3_S0_MST_OFF_03[] = {
	0xBF,
	0x00, 0x07, 0xFF, 0x00, 0x00, 0x10
};
#endif

#ifdef CONFIG_SUPPORT_GRAM_CHECKSUM
static u8 UNBOUND1_A3_S0_SW_RESET[] = { 0x01 };
static u8 UNBOUND1_A3_S0_GCT_DSC[] = { 0x9D, 0x01 };
static u8 UNBOUND1_A3_S0_GCT_PPS[] = { 0x9E,
	0x11, 0x00, 0x00, 0x89, 0x30, 0x80, 0x09, 0x60,
	0x04, 0x38, 0x00, 0x78, 0x02, 0x1C, 0x02, 0x1C,
	0x02, 0x00, 0x02, 0x0E, 0x00, 0x20, 0x0B, 0xAF,
	0x00, 0x07, 0x00, 0x0C, 0x00, 0xCF, 0x00, 0xD9,
	0x18, 0x00, 0x10, 0xF0, 0x03, 0x0C, 0x20, 0x00,
	0x06, 0x0B, 0x0B, 0x33, 0x0E, 0x1C, 0x2A, 0x38,
	0x46, 0x54, 0x62, 0x69, 0x70, 0x77, 0x79, 0x7B,
	0x7D, 0x7E, 0x01, 0x02, 0x01, 0x00, 0x09, 0x40,
	0x09, 0xBE, 0x19, 0xFC, 0x19, 0xFA, 0x19, 0xF8,
	0x1A, 0x38, 0x1A, 0x78, 0x1A, 0xB6, 0x2A, 0xF6,
	0x2B, 0x34, 0x2B, 0x74, 0x3B, 0x74, 0x6B, 0xF4,
	0x00
};
static u8 UNBOUND1_A3_S0_FUSING_UPDATE_ON[] = { 0xC5, 0x05 };
static u8 UNBOUND1_A3_S0_FUSING_UPDATE_OFF[] = { 0xC5, 0x04 };
static u8 UNBOUND1_A3_S0_VDDM_INIT[] = { 0xFE, 0x14 };
static u8 UNBOUND1_A3_S0_VDDM_ORIG[] = { 0xF4, 0x00 };
static u8 UNBOUND1_A3_S0_VDDM_VOLT[] = { 0xF4, 0x00 };
static u8 UNBOUND1_A3_S0_GRAM_CHKSUM_START[] = { 0x2C, 0x00 };
static u8 UNBOUND1_A3_S0_GRAM_IMG_PATTERN_ON[] = { 0xBE, 0x00 };
static u8 UNBOUND1_A3_S0_GRAM_INV_IMG_PATTERN_ON[] = { 0xBE, 0x00 };
static u8 UNBOUND1_A3_S0_GRAM_IMG_PATTERN_OFF[] = { 0xBE, 0x00 };
#endif
#ifdef CONFIG_SUPPORT_XTALK_MODE
static u8 UNBOUND1_A3_S0_XTALK_MODE[] = { 0xF4, 0x2C };
#endif
#ifdef CONFIG_SUPPORT_CCD_TEST
static u8 UNBOUND1_A3_S0_CCD_ENABLE[] = { 0xCC, 0x01 };
static u8 UNBOUND1_A3_S0_CCD_DISABLE[] = { 0xCC, 0x00 };
#endif

static DEFINE_STATIC_PACKET(unbound1_a3_s0_level1_key_enable, DSI_PKT_TYPE_WR, UNBOUND1_A3_S0_KEY1_ENABLE, 0);
static DEFINE_STATIC_PACKET(unbound1_a3_s0_level2_key_enable, DSI_PKT_TYPE_WR, UNBOUND1_A3_S0_KEY2_ENABLE, 0);
static DEFINE_STATIC_PACKET(unbound1_a3_s0_level3_key_enable, DSI_PKT_TYPE_WR, UNBOUND1_A3_S0_KEY3_ENABLE, 0);
static DEFINE_STATIC_PACKET(unbound1_a3_s0_level1_key_disable, DSI_PKT_TYPE_WR, UNBOUND1_A3_S0_KEY1_DISABLE, 0);
static DEFINE_STATIC_PACKET(unbound1_a3_s0_level2_key_disable, DSI_PKT_TYPE_WR, UNBOUND1_A3_S0_KEY2_DISABLE, 0);
static DEFINE_STATIC_PACKET(unbound1_a3_s0_level3_key_disable, DSI_PKT_TYPE_WR, UNBOUND1_A3_S0_KEY3_DISABLE, 0);

static DEFINE_STATIC_PACKET(unbound1_a3_s0_sleep_out, DSI_PKT_TYPE_WR, UNBOUND1_A3_S0_SLEEP_OUT, 0);
static DEFINE_STATIC_PACKET(unbound1_a3_s0_sleep_in, DSI_PKT_TYPE_WR, UNBOUND1_A3_S0_SLEEP_IN, 0);
static DEFINE_STATIC_PACKET(unbound1_a3_s0_display_on, DSI_PKT_TYPE_WR, UNBOUND1_A3_S0_DISPLAY_ON, 0);
static DEFINE_STATIC_PACKET(unbound1_a3_s0_display_off, DSI_PKT_TYPE_WR, UNBOUND1_A3_S0_DISPLAY_OFF, 0);

static DEFINE_STATIC_PACKET(unbound1_a3_s0_te_on, DSI_PKT_TYPE_WR, UNBOUND1_A3_S0_TE_ON, 0);

static DEFINE_PKTUI(unbound1_a3_s0_tsp_hsync, &unbound1_a3_s0_maptbl[TE_FRAME_SEL_MAPTBL], 1);
static DEFINE_VARIABLE_PACKET(unbound1_a3_s0_tsp_hsync, DSI_PKT_TYPE_WR, UNBOUND1_A3_S0_TSP_HSYNC, 0);

static DEFINE_PKTUI(unbound1_a3_s0_set_te_frame_sel, &unbound1_a3_s0_maptbl[TE_FRAME_SEL_MAPTBL], 1);
static DEFINE_VARIABLE_PACKET(unbound1_a3_s0_set_te_frame_sel, DSI_PKT_TYPE_WR, UNBOUND1_A3_S0_SET_TE_FRAME_SEL, 0);
static DEFINE_STATIC_PACKET(unbound1_a3_s0_clr_te_frame_sel, DSI_PKT_TYPE_WR, UNBOUND1_A3_S0_CLR_TE_FRAME_SEL, 0);

static DEFINE_STATIC_PACKET(unbound1_a3_s0_dsc, DSI_PKT_TYPE_COMP, UNBOUND1_A3_S0_DSC, 0);
static DEFINE_STATIC_PACKET(unbound1_a3_s0_pps, DSI_PKT_TYPE_PPS, UNBOUND1_A3_S0_PPS, 0);

static u8 UNBOUND1_A3_S0_GAMMA_0[S6E3FAB_GAMMA_0_LEN + 1] = { S6E3FAB_GAMMA_0_REG };
static DEFINE_PKTUI(unbound1_a3_s0_gamma_0, &unbound1_a3_s0_maptbl[GAMMA_MTP_0_MAPTBL], 1);
static DEFINE_VARIABLE_PACKET(unbound1_a3_s0_gamma_0, DSI_PKT_TYPE_WR, UNBOUND1_A3_S0_GAMMA_0, S6E3FAB_GAMMA_0_OFS);

static u8 UNBOUND1_A3_S0_GAMMA_1[S6E3FAB_GAMMA_1_LEN + 1] = { S6E3FAB_GAMMA_1_REG };
static DEFINE_PKTUI(unbound1_a3_s0_gamma_1, &unbound1_a3_s0_maptbl[GAMMA_MTP_1_MAPTBL], 1);
static DEFINE_VARIABLE_PACKET(unbound1_a3_s0_gamma_1, DSI_PKT_TYPE_WR, UNBOUND1_A3_S0_GAMMA_1, S6E3FAB_GAMMA_1_OFS);

static u8 UNBOUND1_A3_S0_GAMMA_2[S6E3FAB_GAMMA_2_LEN + 1] = { S6E3FAB_GAMMA_2_REG };
static DEFINE_PKTUI(unbound1_a3_s0_gamma_2, &unbound1_a3_s0_maptbl[GAMMA_MTP_2_MAPTBL], 1);
static DEFINE_VARIABLE_PACKET(unbound1_a3_s0_gamma_2, DSI_PKT_TYPE_WR, UNBOUND1_A3_S0_GAMMA_2, S6E3FAB_GAMMA_2_OFS);

static u8 UNBOUND1_A3_S0_GAMMA_3[S6E3FAB_GAMMA_3_LEN + 1] = { S6E3FAB_GAMMA_3_REG };
static DEFINE_PKTUI(unbound1_a3_s0_gamma_3, &unbound1_a3_s0_maptbl[GAMMA_MTP_3_MAPTBL], 1);
static DEFINE_VARIABLE_PACKET(unbound1_a3_s0_gamma_3, DSI_PKT_TYPE_WR, UNBOUND1_A3_S0_GAMMA_3, S6E3FAB_GAMMA_3_OFS);

static u8 UNBOUND1_A3_S0_GAMMA_4[S6E3FAB_GAMMA_4_LEN + 1] = { S6E3FAB_GAMMA_4_REG };
static DEFINE_PKTUI(unbound1_a3_s0_gamma_4, &unbound1_a3_s0_maptbl[GAMMA_MTP_4_MAPTBL], 1);
static DEFINE_VARIABLE_PACKET(unbound1_a3_s0_gamma_4, DSI_PKT_TYPE_WR, UNBOUND1_A3_S0_GAMMA_4, S6E3FAB_GAMMA_4_OFS);

static DEFINE_COND(unbound1_a3_s0_cond_is_id_gte_03, is_id_gte_03);
static DEFINE_COND(unbound1_a3_s0_cond_is_first_set_bl, is_first_set_bl);
static DEFINE_COND(unbound1_a3_s0_cond_is_wait_vsync_needed, is_wait_vsync_needed);

static DEFINE_STATIC_PACKET(unbound1_a3_s0_lpm_enter_aor, DSI_PKT_TYPE_WR, UNBOUND1_A3_S0_LPM_ENTER_AOR, 0xAF);
static DEFINE_STATIC_PACKET(unbound1_a3_s0_lpm_exit_aor, DSI_PKT_TYPE_WR, UNBOUND1_A3_S0_LPM_EXIT_AOR, 0xAF);
static DEFINE_STATIC_PACKET(unbound1_a3_s0_lpm_enter_osc, DSI_PKT_TYPE_WR, UNBOUND1_A3_S0_LPM_ENTER_OSC, 0);
static DEFINE_STATIC_PACKET(unbound1_a3_s0_lpm_enter_elvss, DSI_PKT_TYPE_WR, UNBOUND1_A3_S0_LPM_ENTER_ELVSS, 0);
static DEFINE_STATIC_PACKET(unbound1_a3_s0_lpm_enter_vfp_60ns, DSI_PKT_TYPE_WR, UNBOUND1_A3_S0_LPM_ENTER_VFP_60NS, 0x0B);
static DEFINE_PKTUI(unbound1_a3_s0_lpm_exit_elvss, &unbound1_a3_s0_maptbl[ELVSS_MAPTBL], 6);
static DEFINE_VARIABLE_PACKET(unbound1_a3_s0_lpm_exit_elvss, DSI_PKT_TYPE_WR, UNBOUND1_A3_S0_LPM_EXIT_ELVSS, 0);
static DEFINE_STATIC_PACKET(unbound1_a3_s0_lpm_exit_swire_no_pulse, DSI_PKT_TYPE_WR,
	UNBOUND1_A3_S0_LPM_EXIT_SWIRE_NO_PULSE, 0x0B);

static DEFINE_STATIC_PACKET(unbound1_a3_s0_lpm_enter_fps, DSI_PKT_TYPE_WR, UNBOUND1_A3_S0_LPM_ENTER_FPS, 0);
static DEFINE_STATIC_PACKET(unbound1_a3_s0_lpm_enter_seamless_ctrl, DSI_PKT_TYPE_WR, UNBOUND1_A3_S0_LPM_ENTER_SEAMLESS_CTRL, 0);
static DEFINE_STATIC_PACKET(unbound1_a3_s0_lpm_exit_seamless_ctrl, DSI_PKT_TYPE_WR, UNBOUND1_A3_S0_LPM_EXIT_SEAMLESS_CTRL, 0);
static DEFINE_PKTUI(unbound1_a3_s0_lpm_nit, &unbound1_a3_s0_maptbl[LPM_NIT_MAPTBL], 1);
static DEFINE_VARIABLE_PACKET(unbound1_a3_s0_lpm_nit, DSI_PKT_TYPE_WR, UNBOUND1_A3_S0_LPM_NIT, 0xD1);
static DEFINE_STATIC_PACKET(unbound1_a3_s0_lpm_mode, DSI_PKT_TYPE_WR, UNBOUND1_A3_S0_LPM_MODE, 0);
static DEFINE_STATIC_PACKET(unbound1_a3_s0_lpm_avs_on, DSI_PKT_TYPE_WR, UNBOUND1_A3_S0_LPM_AVS_ON, 0x22);

static DEFINE_COND(unbound1_a3_s0_cond_is_panel_state_not_lpm, is_panel_state_not_lpm);

static DEFINE_PKTUI(unbound1_a3_s0_lpm_on, &unbound1_a3_s0_maptbl[LPM_ON_MAPTBL], 1);
static DEFINE_VARIABLE_PACKET(unbound1_a3_s0_lpm_on, DSI_PKT_TYPE_WR, UNBOUND1_A3_S0_LPM_ON, 0);
static DEFINE_PKTUI(unbound1_a3_s0_lpm_off, &unbound1_a3_s0_maptbl[LPM_OFF_MAPTBL], 1);
static DEFINE_VARIABLE_PACKET(unbound1_a3_s0_lpm_off, DSI_PKT_TYPE_WR, UNBOUND1_A3_S0_LPM_OFF, 0);

static DEFINE_STATIC_PACKET(unbound1_a3_s0_lpm_avc2_off, DSI_PKT_TYPE_WR, UNBOUND1_A3_S0_LPM_AVC2_OFF, 0x09);
static DEFINE_STATIC_PACKET(unbound1_a3_s0_lpm_avc2_on, DSI_PKT_TYPE_WR, UNBOUND1_A3_S0_LPM_AVC2_ON, 0x09);

#ifdef CONFIG_SUPPORT_DYNAMIC_HLPM
static DEFINE_STATIC_PACKET(unbound1_a3_s0_dynamic_hlpm_enable, DSI_PKT_TYPE_WR, UNBOUND1_A3_S0_DYNAMIC_HLPM_ENABLE, 0);
static DEFINE_STATIC_PACKET(unbound1_a3_s0_dynamic_hlpm_disable, DSI_PKT_TYPE_WR, UNBOUND1_A3_S0_DYNAMIC_HLPM_DISABLE, 0);
#endif

static DEFINE_PKTUI(unbound1_a3_s0_elvss, &unbound1_a3_s0_maptbl[ELVSS_MAPTBL], 1);
static DEFINE_VARIABLE_PACKET(unbound1_a3_s0_elvss, DSI_PKT_TYPE_WR, UNBOUND1_A3_S0_ELVSS, 0x05);

static DEFINE_STATIC_PACKET(unbound1_a3_s0_smooth_dimming_init,
	DSI_PKT_TYPE_WR, UNBOUND1_A3_S0_SMOOTH_DIMMING_INIT, 0x06);
static DEFINE_STATIC_PACKET(unbound1_a3_s0_smooth_dimming,
	DSI_PKT_TYPE_WR, UNBOUND1_A3_S0_SMOOTH_DIMMING, 0x06);

static u8 UNBOUND1_A3_S0_DIA_ONOFF[] = { 0x91, 0x00 };
static DEFINE_PKTUI(unbound1_a3_s0_dia_onoff, &unbound1_a3_s0_maptbl[DIA_ONOFF_MAPTBL], 1);
static DEFINE_VARIABLE_PACKET(unbound1_a3_s0_dia_onoff, DSI_PKT_TYPE_WR, UNBOUND1_A3_S0_DIA_ONOFF, 0x3B);

static u8 UNBOUND1_A3_S0_IRC_MODE[] = { 0x92, 0x00 };
static DEFINE_PKTUI(unbound1_a3_s0_irc_mode, &unbound1_a3_s0_maptbl[IRC_MODE_MAPTBL], 1);
static DEFINE_VARIABLE_PACKET(unbound1_a3_s0_irc_mode, DSI_PKT_TYPE_WR, UNBOUND1_A3_S0_IRC_MODE, 0x0B);

static DEFINE_PKTUI(unbound1_a3_s0_tset, &unbound1_a3_s0_maptbl[TSET_MAPTBL], 1);
static DEFINE_VARIABLE_PACKET(unbound1_a3_s0_tset, DSI_PKT_TYPE_WR, UNBOUND1_A3_S0_TSET, 0x2D);

static DEFINE_STATIC_PACKET(unbound1_a3_s0_dummy_2c, DSI_PKT_TYPE_WR, UNBOUND1_A3_S0_DUMMY_2C, 0);

static DEFINE_STATIC_PACKET(unbound1_a3_s0_err_fg_1, DSI_PKT_TYPE_WR, UNBOUND1_A3_S0_ERR_FG_1, 0x23);
static DEFINE_STATIC_PACKET(unbound1_a3_s0_err_fg_2, DSI_PKT_TYPE_WR, UNBOUND1_A3_S0_ERR_FG_2, 0);
static DEFINE_STATIC_PACKET(unbound1_a3_s0_isc, DSI_PKT_TYPE_WR, UNBOUND1_A3_S0_ISC, 0x07);
static DEFINE_STATIC_PACKET(unbound1_a3_s0_gamma_normalize, DSI_PKT_TYPE_WR, UNBOUND1_A3_S0_GAMMA_NORMALIZE, 0x19);

#if defined(CONFIG_SUPPORT_FAST_DISCHARGE)
static DEFINE_PKTUI(unbound1_a3_s0_fast_discharge, &unbound1_a3_s0_maptbl[FAST_DISCHARGE_MAPTBL], 1);
static DEFINE_VARIABLE_PACKET(unbound1_a3_s0_fast_discharge, DSI_PKT_TYPE_WR, UNBOUND1_A3_S0_FAST_DISCHARGE, 0x0C);
#endif

static DEFINE_STATIC_PACKET(unbound1_a3_s0_mcd_on_01, DSI_PKT_TYPE_WR, UNBOUND1_A3_S0_MCD_ON_01, 0x19);
static DEFINE_STATIC_PACKET(unbound1_a3_s0_mcd_on_02, DSI_PKT_TYPE_WR, UNBOUND1_A3_S0_MCD_ON_02, 0x32);
static DEFINE_STATIC_PACKET(unbound1_a3_s0_mcd_on_03, DSI_PKT_TYPE_WR, UNBOUND1_A3_S0_MCD_ON_03, 0x59);
static DEFINE_STATIC_PACKET(unbound1_a3_s0_mcd_on_04, DSI_PKT_TYPE_WR, UNBOUND1_A3_S0_MCD_ON_04, 0x13);
static DEFINE_STATIC_PACKET(unbound1_a3_s0_mcd_on_05, DSI_PKT_TYPE_WR, UNBOUND1_A3_S0_MCD_ON_05, 0x04);

static DEFINE_STATIC_PACKET(unbound1_a3_s0_mcd_off_01, DSI_PKT_TYPE_WR, UNBOUND1_A3_S0_MCD_OFF_01, 0x19);
static DEFINE_STATIC_PACKET(unbound1_a3_s0_mcd_off_02, DSI_PKT_TYPE_WR, UNBOUND1_A3_S0_MCD_OFF_02, 0x32);
static DEFINE_STATIC_PACKET(unbound1_a3_s0_mcd_off_03, DSI_PKT_TYPE_WR, UNBOUND1_A3_S0_MCD_OFF_03, 0x59);
static DEFINE_STATIC_PACKET(unbound1_a3_s0_mcd_off_04, DSI_PKT_TYPE_WR, UNBOUND1_A3_S0_MCD_OFF_04, 0x13);
static DEFINE_STATIC_PACKET(unbound1_a3_s0_mcd_off_05, DSI_PKT_TYPE_WR, UNBOUND1_A3_S0_MCD_OFF_05, 0x04);

#ifdef CONFIG_SUPPORT_GRAYSPOT_TEST
static DEFINE_STATIC_PACKET(unbound1_a3_s0_grayspot_on_01, DSI_PKT_TYPE_WR, UNBOUND1_A3_S0_GRAYSPOT_ON_01, 0x13);
static DEFINE_STATIC_PACKET(unbound1_a3_s0_grayspot_on_02, DSI_PKT_TYPE_WR, UNBOUND1_A3_S0_GRAYSPOT_ON_02, 0);
static DEFINE_STATIC_PACKET(unbound1_a3_s0_grayspot_on_03, DSI_PKT_TYPE_WR, UNBOUND1_A3_S0_GRAYSPOT_ON_03, 0x2D);
static DEFINE_STATIC_PACKET(unbound1_a3_s0_grayspot_on_04, DSI_PKT_TYPE_WR, UNBOUND1_A3_S0_GRAYSPOT_ON_04, 0);

static DEFINE_STATIC_PACKET(unbound1_a3_s0_grayspot_off_01, DSI_PKT_TYPE_WR, UNBOUND1_A3_S0_GRAYSPOT_OFF_01, 0x13);
static DEFINE_STATIC_PACKET(unbound1_a3_s0_grayspot_off_02, DSI_PKT_TYPE_WR, UNBOUND1_A3_S0_GRAYSPOT_OFF_02, 0);
static DEFINE_PKTUI(unbound1_a3_s0_grayspot_off_03, &unbound1_a3_s0_maptbl[TSET_MAPTBL], 1);
static DEFINE_VARIABLE_PACKET(unbound1_a3_s0_grayspot_off_03, DSI_PKT_TYPE_WR, UNBOUND1_A3_S0_GRAYSPOT_OFF_03, 0x2D);
static DECLARE_PKTUI(unbound1_a3_s0_grayspot_off_04) = {
	{ .offset = 6, .maptbl = &unbound1_a3_s0_maptbl[ELVSS_MAPTBL] },
	{ .offset = 8, .maptbl = &unbound1_a3_s0_maptbl[ELVSS_CAL_OFFSET_MAPTBL] },
};
static DEFINE_VARIABLE_PACKET(unbound1_a3_s0_grayspot_off_04, DSI_PKT_TYPE_WR, UNBOUND1_A3_S0_GRAYSPOT_OFF_04, 0);
#endif

#ifdef CONFIG_SUPPORT_MST
static DEFINE_STATIC_PACKET(unbound1_a3_s0_mst_on_01, DSI_PKT_TYPE_WR, UNBOUND1_A3_S0_MST_ON_01, 0x42);
static DEFINE_STATIC_PACKET(unbound1_a3_s0_mst_on_02, DSI_PKT_TYPE_WR, UNBOUND1_A3_S0_MST_ON_02, 0x0F);
static DEFINE_STATIC_PACKET(unbound1_a3_s0_mst_on_03, DSI_PKT_TYPE_WR, UNBOUND1_A3_S0_MST_ON_03, 0x00);

static DEFINE_STATIC_PACKET(unbound1_a3_s0_mst_off_01, DSI_PKT_TYPE_WR, UNBOUND1_A3_S0_MST_OFF_01, 0x54);
static DEFINE_STATIC_PACKET(unbound1_a3_s0_mst_off_02, DSI_PKT_TYPE_WR, UNBOUND1_A3_S0_MST_OFF_02, 0x0F);
static DEFINE_STATIC_PACKET(unbound1_a3_s0_mst_off_03, DSI_PKT_TYPE_WR, UNBOUND1_A3_S0_MST_OFF_03, 0x00);
#endif

#ifdef CONFIG_SUPPORT_GRAM_CHECKSUM
static DEFINE_STATIC_PACKET(unbound1_a3_s0_sw_reset, DSI_PKT_TYPE_WR, UNBOUND1_A3_S0_SW_RESET, 0);
static DEFINE_STATIC_PACKET(unbound1_a3_s0_gct_dsc, DSI_PKT_TYPE_WR, UNBOUND1_A3_S0_GCT_DSC, 0);
static DEFINE_STATIC_PACKET(unbound1_a3_s0_gct_pps, DSI_PKT_TYPE_WR, UNBOUND1_A3_S0_GCT_PPS, 0);
static DEFINE_STATIC_PACKET(unbound1_a3_s0_vddm_orig, DSI_PKT_TYPE_WR, UNBOUND1_A3_S0_VDDM_ORIG, 0x0E);
static DEFINE_PKTUI(unbound1_a3_s0_vddm_volt, &unbound1_a3_s0_maptbl[VDDM_MAPTBL], 1);
static DEFINE_VARIABLE_PACKET(unbound1_a3_s0_vddm_volt, DSI_PKT_TYPE_WR, UNBOUND1_A3_S0_VDDM_VOLT, 0x0E);
static DEFINE_STATIC_PACKET(unbound1_a3_s0_fusing_update_on, DSI_PKT_TYPE_WR, UNBOUND1_A3_S0_FUSING_UPDATE_ON, 0x1A);
static DEFINE_STATIC_PACKET(unbound1_a3_s0_fusing_update_off, DSI_PKT_TYPE_WR, UNBOUND1_A3_S0_FUSING_UPDATE_OFF, 0x1A);
static DEFINE_STATIC_PACKET(unbound1_a3_s0_vddm_init, DSI_PKT_TYPE_WR, UNBOUND1_A3_S0_VDDM_INIT, 0x0A);
static DEFINE_STATIC_PACKET(unbound1_a3_s0_gram_chksum_start, DSI_PKT_TYPE_WR, UNBOUND1_A3_S0_GRAM_CHKSUM_START, 0);
static DEFINE_PKTUI(unbound1_a3_s0_gram_img_pattern_on, &unbound1_a3_s0_maptbl[GRAM_IMG_MAPTBL], 1);
static DEFINE_VARIABLE_PACKET(unbound1_a3_s0_gram_img_pattern_on, DSI_PKT_TYPE_WR, UNBOUND1_A3_S0_GRAM_IMG_PATTERN_ON, 0);
static DEFINE_PKTUI(unbound1_a3_s0_gram_inv_img_pattern_on, &unbound1_a3_s0_maptbl[GRAM_INV_IMG_MAPTBL], 1);
static DEFINE_VARIABLE_PACKET(unbound1_a3_s0_gram_inv_img_pattern_on, DSI_PKT_TYPE_WR, UNBOUND1_A3_S0_GRAM_INV_IMG_PATTERN_ON, 0);
static DEFINE_STATIC_PACKET(unbound1_a3_s0_gram_img_pattern_off, DSI_PKT_TYPE_WR, UNBOUND1_A3_S0_GRAM_IMG_PATTERN_OFF, 0);
#endif
#ifdef CONFIG_SUPPORT_XTALK_MODE
static DEFINE_PKTUI(unbound1_a3_s0_xtalk_mode, &unbound1_a3_s0_maptbl[VGH_MAPTBL], 1);
static DEFINE_VARIABLE_PACKET(unbound1_a3_s0_xtalk_mode, DSI_PKT_TYPE_WR, UNBOUND1_A3_S0_XTALK_MODE, 0x19);
#endif
#ifdef CONFIG_SUPPORT_CCD_TEST
static DEFINE_STATIC_PACKET(unbound1_a3_s0_ccd_test_enable, DSI_PKT_TYPE_WR, UNBOUND1_A3_S0_CCD_ENABLE, 0x00);
static DEFINE_STATIC_PACKET(unbound1_a3_s0_ccd_test_disable, DSI_PKT_TYPE_WR, UNBOUND1_A3_S0_CCD_DISABLE, 0x00);
#endif

#ifdef CONFIG_SUPPORT_DDI_CMDLOG
static DEFINE_STATIC_PACKET(unbound1_a3_s0_cmdlog_enable, DSI_PKT_TYPE_WR, UNBOUND1_A3_S0_CMDLOG_ENABLE, 0);
static DEFINE_STATIC_PACKET(unbound1_a3_s0_cmdlog_disable, DSI_PKT_TYPE_WR, UNBOUND1_A3_S0_CMDLOG_DISABLE, 0);
#endif
static DEFINE_STATIC_PACKET(unbound1_a3_s0_gamma_update_enable, DSI_PKT_TYPE_WR, UNBOUND1_A3_S0_GAMMA_UPDATE_ENABLE, 0);

static DEFINE_PANEL_MDELAY(unbound1_a3_s0_wait_10msec, 10);
static DEFINE_PANEL_MDELAY(unbound1_a3_s0_wait_17msec, 17);
static DEFINE_PANEL_MDELAY(unbound1_a3_s0_wait_20msec, 20);
static DEFINE_PANEL_MDELAY(unbound1_a3_s0_wait_34msec, 34);
static DEFINE_PANEL_MDELAY(unbound1_a3_s0_wait_100msec, 100);
static DEFINE_PANEL_MDELAY(unbound1_a3_s0_wait_sleep_out_20msec, 20);
static DEFINE_PANEL_MDELAY(unbound1_a3_s0_wait_fastdischarge_120msec, 120);
static DEFINE_PANEL_MDELAY(unbound1_a3_s0_wait_sleep_in, 120);
static DEFINE_PANEL_FRAME_DELAY(unbound1_a3_s0_wait_1_frame, 1);
static DEFINE_PANEL_UDELAY(unbound1_a3_s0_wait_1usec, 1);
static DEFINE_PANEL_UDELAY(unbound1_a3_s0_wait_100usec, 100);
static DEFINE_PANEL_MDELAY(unbound1_a3_s0_wait_1msec, 1);
static DEFINE_PANEL_VSYNC_DELAY(unbound1_a3_s0_wait_1_vsync, 1);

static DEFINE_PANEL_TIMER_MDELAY(unbound1_a3_s0_wait_sleep_out_120msec, 120);
static DEFINE_PANEL_TIMER_BEGIN(unbound1_a3_s0_wait_sleep_out_120msec,
		TIMER_DLYINFO(&unbound1_a3_s0_wait_sleep_out_120msec));

#ifdef CONFIG_SUPPORT_GRAM_CHECKSUM
static DEFINE_PANEL_MDELAY(unbound1_a3_s0_wait_120msec, 120);
static DEFINE_PANEL_MDELAY(unbound1_a3_s0_wait_vddm_update, 50);
static DEFINE_PANEL_MDELAY(unbound1_a3_s0_wait_20msec_gram_img_update, 20);
static DEFINE_PANEL_MDELAY(unbound1_a3_s0_wait_gram_img_update, 150);
#endif

static DEFINE_PANEL_KEY(unbound1_a3_s0_level1_key_enable, CMD_LEVEL_1, KEY_ENABLE, &PKTINFO(unbound1_a3_s0_level1_key_enable));
static DEFINE_PANEL_KEY(unbound1_a3_s0_level2_key_enable, CMD_LEVEL_2, KEY_ENABLE, &PKTINFO(unbound1_a3_s0_level2_key_enable));
static DEFINE_PANEL_KEY(unbound1_a3_s0_level3_key_enable, CMD_LEVEL_3, KEY_ENABLE, &PKTINFO(unbound1_a3_s0_level3_key_enable));
static DEFINE_PANEL_KEY(unbound1_a3_s0_level1_key_disable, CMD_LEVEL_1, KEY_DISABLE, &PKTINFO(unbound1_a3_s0_level1_key_disable));
static DEFINE_PANEL_KEY(unbound1_a3_s0_level2_key_disable, CMD_LEVEL_2, KEY_DISABLE, &PKTINFO(unbound1_a3_s0_level2_key_disable));
static DEFINE_PANEL_KEY(unbound1_a3_s0_level3_key_disable, CMD_LEVEL_3, KEY_DISABLE, &PKTINFO(unbound1_a3_s0_level3_key_disable));


#ifdef CONFIG_DYNAMIC_MIPI
static DEFINE_PKTUI(unbound1_a3_s0_set_ffc, &unbound1_a3_s0_maptbl[DM_SET_FFC_MAPTBL], 0);
static DEFINE_VARIABLE_PACKET(unbound1_a3_s0_set_ffc, DSI_PKT_TYPE_WR, UNBOUND1_A3_S0_FFC_DEFAULT, 0);


static char UNBOUND1_A3_S0_DDI_OSC[] = {
	0xF2, 0x00
};

static DEFINE_PKTUI(unbound1_a3_s0_ddi_osc, &unbound1_a3_s0_maptbl[DM_DDI_OSC_MAPTBL], 0);
static DEFINE_VARIABLE_PACKET(unbound1_a3_s0_ddi_osc, DSI_PKT_TYPE_WR, UNBOUND1_A3_S0_DDI_OSC, 0x41);


static char UNBOUND1_A3_S0_FOSC_UPDATE1[] = {
	0xF2, 0x90
};
static DEFINE_STATIC_PACKET(unbound1_a3_s0_fosc_update1, DSI_PKT_TYPE_WR, UNBOUND1_A3_S0_FOSC_UPDATE1, 0);

static char UNBOUND1_A3_S0_FOSC_UPDATE2[] = {
	0xF2, 0xB0
};
static DEFINE_STATIC_PACKET(unbound1_a3_s0_fosc_update2, DSI_PKT_TYPE_WR, UNBOUND1_A3_S0_FOSC_UPDATE2, 0);


#else
static DEFINE_STATIC_PACKET(unbound1_a3_s0_set_ffc, DSI_PKT_TYPE_WR, UNBOUND1_A3_S0_FFC_DEFAULT, 0);
#endif

static char UNBOUND1_A3_S0_OFF_FFC[] = {
	0xC5,
	0x09, 0x10, 0x68, 0x22, 0x02, 0x22, 0x02,
};
static DEFINE_STATIC_PACKET(unbound1_a3_s0_off_ffc, DSI_PKT_TYPE_WR, UNBOUND1_A3_S0_OFF_FFC, 0);

static void *unbound1_a3_s0_dm_set_ffc_cmdtbl[] = {
	&KEYINFO(unbound1_a3_s0_level3_key_enable),
	&PKTINFO(unbound1_a3_s0_set_ffc),
	&KEYINFO(unbound1_a3_s0_level3_key_disable),
};

static void *unbound1_a3_s0_dm_off_ffc_cmdtbl[] = {
	&KEYINFO(unbound1_a3_s0_level3_key_enable),
	&PKTINFO(unbound1_a3_s0_off_ffc),
	&KEYINFO(unbound1_a3_s0_level3_key_disable),
};

/* temporary bl code start */

static u8 UNBOUND1_A3_S0_HBM_AND_TRANSITION[] = {
	0x53, 0x20
};
static DEFINE_PKTUI(unbound1_a3_s0_hbm_and_transition, &unbound1_a3_s0_maptbl[HBM_AND_TRANSITION_MAPTBL], 1);
static DEFINE_VARIABLE_PACKET(unbound1_a3_s0_hbm_and_transition, DSI_PKT_TYPE_WR, UNBOUND1_A3_S0_HBM_AND_TRANSITION, 0);

static u8 UNBOUND1_A3_S0_ACL_ONOFF[] = {
	0x55,
	0x01
};
static DEFINE_PKTUI(unbound1_a3_s0_acl_onoff, &unbound1_a3_s0_maptbl[ACL_ONOFF_MAPTBL], 1);
static DEFINE_VARIABLE_PACKET(unbound1_a3_s0_acl_onoff, DSI_PKT_TYPE_WR, UNBOUND1_A3_S0_ACL_ONOFF, 0);

static u8 UNBOUND1_A3_S0_ACL_CONTROL[] = {
	0x9B,
	0x0B, 0x04, 0x91, 0x24, 0x63, 0x41, 0xFF
};
static DEFINE_PKTUI(unbound1_a3_s0_acl_control, &unbound1_a3_s0_maptbl[ACL_OPR_MAPTBL], 2);
static DEFINE_VARIABLE_PACKET(unbound1_a3_s0_acl_control, DSI_PKT_TYPE_WR, UNBOUND1_A3_S0_ACL_CONTROL, 0x90);

static u8 UNBOUND1_A3_S0_ACL_DIM[] = {
	0x9B, 0x20
};
static DEFINE_STATIC_PACKET(unbound1_a3_s0_acl_dim, DSI_PKT_TYPE_WR, UNBOUND1_A3_S0_ACL_DIM, 0x9F);

static u8 UNBOUND1_A3_S0_WRDISBV[] = {
	0x51, 0x03, 0xFF
};
static DEFINE_PKTUI(unbound1_a3_s0_wrdisbv, &unbound1_a3_s0_maptbl[GAMMA_MODE2_MAPTBL], 1);
static DEFINE_VARIABLE_PACKET(unbound1_a3_s0_wrdisbv, DSI_PKT_TYPE_WR, UNBOUND1_A3_S0_WRDISBV, 0);

static u8 UNBOUND1_A3_S0_LPM_WRDISBV[] = {
	0x51, 0x03, 0xFF
};
static DEFINE_PKTUI(unbound1_a3_s0_lpm_wrdisbv, &unbound1_a3_s0_maptbl[LPM_WRDISBV_MAPTBL], 1);
static DEFINE_VARIABLE_PACKET(unbound1_a3_s0_lpm_wrdisbv, DSI_PKT_TYPE_WR, UNBOUND1_A3_S0_LPM_WRDISBV, 0);

static u8 UNBOUND1_A3_S0_CASET[] = { 0x2A, 0x00, 0x00, 0x04, 0x37 };
static u8 UNBOUND1_A3_S0_PASET[] = { 0x2B, 0x00, 0x00, 0x09, 0x5F };

static DEFINE_STATIC_PACKET(unbound1_a3_s0_caset, DSI_PKT_TYPE_WR, UNBOUND1_A3_S0_CASET, 0);
static DEFINE_STATIC_PACKET(unbound1_a3_s0_paset, DSI_PKT_TYPE_WR, UNBOUND1_A3_S0_PASET, 0);

#ifdef CONFIG_SUPPORT_HMD
static u8 UNBOUND1_A3_S0_HMD_WRDISBV[] = {
	0x51, 0x03, 0xFF
};
static DEFINE_PKTUI(unbound1_a3_s0_hmd_wrdisbv, &unbound1_a3_s0_maptbl[HMD_GAMMA_MAPTBL], 1);
static DEFINE_VARIABLE_PACKET(unbound1_a3_s0_hmd_wrdisbv, DSI_PKT_TYPE_WR, UNBOUND1_A3_S0_HMD_WRDISBV, 0);

static u8 UNBOUND1_A3_S0_HMD_SETTING[] = {
	0xCB,
	0xCF, 0xDB, 0x5B, 0x5B
};
static DEFINE_STATIC_PACKET(unbound1_a3_s0_hmd_setting, DSI_PKT_TYPE_WR, UNBOUND1_A3_S0_HMD_SETTING, 0x14);

static u8 UNBOUND1_A3_S0_HMD_ON_AOR[] = {
	0x9A,
	0x07, 0x9A
};
static DEFINE_STATIC_PACKET(unbound1_a3_s0_hmd_on_aor, DSI_PKT_TYPE_WR, UNBOUND1_A3_S0_HMD_ON_AOR, 0xAF);

static u8 UNBOUND1_A3_S0_HMD_ON_AOR_CYCLE[] = {
	0x9A, 0x01
};
static DEFINE_STATIC_PACKET(unbound1_a3_s0_hmd_on_aor_cycle, DSI_PKT_TYPE_WR, UNBOUND1_A3_S0_HMD_ON_AOR_CYCLE, 0xC9);

static u8 UNBOUND1_A3_S0_HMD_ON_LTPS[] = {
	0xCB,
	0x00, 0x11, 0x9D, 0x9D, 0x00, 0x00, 0x00, 0x00,
	0x00, 0xDB, 0xDB, 0x9B, 0x8D, 0xDE, 0xD3, 0xD1,
	0xC3, 0xCF, 0xC4, 0xC1, 0xCE, 0xDB, 0x5B, 0x5B,
	0xDB, 0x9B, 0xCD, 0x9E, 0xD3, 0xD1, 0xC3, 0xCF,
	0xC4, 0xC1, 0xDB, 0xDB, 0xDB, 0xC0, 0x80, 0x00,
	0x00, 0x00, 0x00, 0x80, 0x80, 0x80, 0x80, 0x80,
	0x80, 0x00, 0x03, 0x00, 0x0E, 0x00, 0x00, 0x00
};
static DEFINE_STATIC_PACKET(unbound1_a3_s0_hmd_on_ltps, DSI_PKT_TYPE_WR, UNBOUND1_A3_S0_HMD_ON_LTPS, 0);

static u8 UNBOUND1_A3_S0_HMD_OFF_AOR[] = {
	0x9A,
	0x01, 0x86
};
static DEFINE_STATIC_PACKET(unbound1_a3_s0_hmd_off_aor, DSI_PKT_TYPE_WR, UNBOUND1_A3_S0_HMD_OFF_AOR, 0xAF);

static u8 UNBOUND1_A3_S0_HMD_OFF_AOR_CYCLE[] = {
	0x9A, 0x41
};
static DEFINE_STATIC_PACKET(unbound1_a3_s0_hmd_off_aor_cycle, DSI_PKT_TYPE_WR, UNBOUND1_A3_S0_HMD_OFF_AOR_CYCLE, 0xC9);

static u8 UNBOUND1_A3_S0_HMD_OFF_LTPS[] = {
	0xCB,
	0x00, 0x11, 0x9D, 0x9D, 0x00, 0x00, 0x00, 0x00,
	0x00, 0xDB, 0xDB, 0x9B, 0x8D, 0xDE, 0xD3, 0xD1,
	0xC3, 0xCE, 0xC4, 0xC1, 0xCF, 0xDB, 0x5B, 0x5B,
	0xDB, 0x9B, 0xCD, 0x9E, 0xD3, 0xD1, 0xC3, 0xCE,
	0xC4, 0xC1, 0xDB, 0xDB, 0xDB, 0xC0, 0x80, 0x00,
	0x00, 0x00, 0x00, 0x80, 0x80, 0x80, 0x80, 0x80,
	0x80, 0x00, 0x0B, 0x00, 0x06, 0x00, 0x00, 0x00
};
static DEFINE_STATIC_PACKET(unbound1_a3_s0_hmd_off_ltps, DSI_PKT_TYPE_WR, UNBOUND1_A3_S0_HMD_OFF_LTPS, 0);
#endif

static u8 UNBOUND1_A3_S0_FPS[] = { 0x60, 0x00, };
static DEFINE_PKTUI(unbound1_a3_s0_fps, &unbound1_a3_s0_maptbl[FPS_MAPTBL], 1);
static DEFINE_VARIABLE_PACKET(unbound1_a3_s0_fps, DSI_PKT_TYPE_WR, UNBOUND1_A3_S0_FPS, 0);

static u8 UNBOUND1_A3_S0_AID[] = { 0x9A, 0x21, };
static DEFINE_PKTUI(unbound1_a3_s0_aid, &unbound1_a3_s0_maptbl[AID_MAPTBL], 1);
static DEFINE_VARIABLE_PACKET(unbound1_a3_s0_aid, DSI_PKT_TYPE_WR, UNBOUND1_A3_S0_AID, 0xC9);

static u8 UNBOUND1_A3_S0_AID_HBM[] = { 0x9A, 0x21, };
static DEFINE_PKTUI(unbound1_a3_s0_aid_hbm, &unbound1_a3_s0_maptbl[AID_MAPTBL], 1);
static DEFINE_VARIABLE_PACKET(unbound1_a3_s0_aid_hbm, DSI_PKT_TYPE_WR, UNBOUND1_A3_S0_AID_HBM, 0xCD);

static u8 UNBOUND1_A3_S0_OSC[] = { 0xF2, 0x00, };
static DEFINE_PKTUI(unbound1_a3_s0_osc, &unbound1_a3_s0_maptbl[OSC_MAPTBL], 1);
static DEFINE_VARIABLE_PACKET(unbound1_a3_s0_osc, DSI_PKT_TYPE_WR, UNBOUND1_A3_S0_OSC, 0);

static u8 UNBOUND1_A3_S0_VFP_FQ_LOW[] = { 0xF2, 0x00, 0x00 };
static DEFINE_PKTUI(unbound1_a3_s0_vfp_fq_low, &unbound1_a3_s0_maptbl[VFP_FQ_LOW_MAPTBL], 1);
static DEFINE_VARIABLE_PACKET(unbound1_a3_s0_vfp_fq_low, DSI_PKT_TYPE_WR, UNBOUND1_A3_S0_VFP_FQ_LOW, 0x0B);

static u8 UNBOUND1_A3_S0_VFP_FQ_HIGH[] = { 0xF2, 0x00, 0x00 };
static DEFINE_PKTUI(unbound1_a3_s0_vfp_fq_high, &unbound1_a3_s0_maptbl[VFP_FQ_HIGH_MAPTBL], 1);
static DEFINE_VARIABLE_PACKET(unbound1_a3_s0_vfp_fq_high, DSI_PKT_TYPE_WR, UNBOUND1_A3_S0_VFP_FQ_HIGH, 0x06);

static u8 UNBOUND1_A3_S0_OSC_0[] = { 0xF2, 0x14, 0xAB };
static DEFINE_PKTUI(unbound1_a3_s0_osc_0, &unbound1_a3_s0_maptbl[OSC_0_MAPTBL], 1);
static DEFINE_VARIABLE_PACKET(unbound1_a3_s0_osc_0, DSI_PKT_TYPE_WR, UNBOUND1_A3_S0_OSC_0, 0x08);

static u8 UNBOUND1_A3_S0_OSC_1[] = { 0xF2, 0x14, 0xAB };
static DEFINE_PKTUI(unbound1_a3_s0_osc_1, &unbound1_a3_s0_maptbl[OSC_1_MAPTBL], 1);
static DEFINE_VARIABLE_PACKET(unbound1_a3_s0_osc_1, DSI_PKT_TYPE_WR, UNBOUND1_A3_S0_OSC_1, 0x0D);

static u8 UNBOUND1_A3_S0_LTPS_0[] = { 0xCB, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 };
static DEFINE_PKTUI(unbound1_a3_s0_ltps_0, &unbound1_a3_s0_maptbl[LTPS_0_MAPTBL], 1);
static DEFINE_VARIABLE_PACKET(unbound1_a3_s0_ltps_0, DSI_PKT_TYPE_WR, UNBOUND1_A3_S0_LTPS_0, 0x42);

static u8 UNBOUND1_A3_S0_LTPS_1[] = { 0xCB, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 };
static DEFINE_PKTUI(unbound1_a3_s0_ltps_1, &unbound1_a3_s0_maptbl[LTPS_1_MAPTBL], 1);
static DEFINE_VARIABLE_PACKET(unbound1_a3_s0_ltps_1, DSI_PKT_TYPE_WR, UNBOUND1_A3_S0_LTPS_1, 0x62);

static u8 UNBOUND1_A3_S0_SRC_AMP[] = { 0xF6, 0x00 };
static DEFINE_PKTUI(unbound1_a3_s0_src_amp, &unbound1_a3_s0_maptbl[SRC_AMP_MAPTBL], 1);
static DEFINE_VARIABLE_PACKET(unbound1_a3_s0_src_amp, DSI_PKT_TYPE_WR, UNBOUND1_A3_S0_SRC_AMP, 0x16);

static u8 UNBOUND1_A3_S0_AOR[] = { 0xF2, 0x00 };
static DEFINE_PKTUI(unbound1_a3_s0_aor, &unbound1_a3_s0_maptbl[AOR_MAPTBL], 1);
static DEFINE_VARIABLE_PACKET(unbound1_a3_s0_aor, DSI_PKT_TYPE_WR, UNBOUND1_A3_S0_AOR, 0x01);

static u8 UNBOUND1_A3_S0_AOR_FQ_LOW[] = { 0x9A, 0x00, 0x00 };
static DEFINE_PKTUI(unbound1_a3_s0_aor_fq_low, &unbound1_a3_s0_maptbl[AOR_FQ_LOW_MAPTBL], 1);
static DEFINE_VARIABLE_PACKET(unbound1_a3_s0_aor_fq_low, DSI_PKT_TYPE_WR, UNBOUND1_A3_S0_AOR_FQ_LOW, 0xAD);

static u8 UNBOUND1_A3_S0_AOR_FQ_HIGH[] = { 0x9A, 0x00, 0x00 };
static DEFINE_PKTUI(unbound1_a3_s0_aor_fq_high, &unbound1_a3_s0_maptbl[AOR_FQ_HIGH_MAPTBL], 1);
static DEFINE_VARIABLE_PACKET(unbound1_a3_s0_aor_fq_high, DSI_PKT_TYPE_WR, UNBOUND1_A3_S0_AOR_FQ_HIGH, 0xC5);

#ifdef CONFIG_SUPPORT_MAFPC

static u8 UNBOUND1_A3_S0_MAFPC_SR_PATH[] = {
	0x75, 0x40,
};
static DEFINE_STATIC_PACKET(unbound1_a3_s0_mafpc_sr_path, DSI_PKT_TYPE_WR, UNBOUND1_A3_S0_MAFPC_SR_PATH, 0);


static u8 UNBOUND1_A3_S0_DEFAULT_SR_PATH[] = {
	0x75, 0x01,
};
static DEFINE_STATIC_PACKET(unbound1_a3_s0_default_sr_path, DSI_PKT_TYPE_WR, UNBOUND1_A3_S0_DEFAULT_SR_PATH, 0);

static DEFINE_STATIC_PACKET_WITH_OPTION(unbound1_mafpc_default_img, DSI_PKT_TYPE_WR_SR,
	S6E3FAB_MAFPC_DEFAULT_IMG, 0, PKT_OPTION_SR_ALIGN_12);


static u8 UNBOUND1_A3_S0_MAFPC_DISABLE[] = {
	0x87, 0x00,
};
static DEFINE_STATIC_PACKET(unbound1_a3_s0_mafpc_disable, DSI_PKT_TYPE_WR, UNBOUND1_A3_S0_MAFPC_DISABLE, 0);


static u8 UNBOUND1_A3_S0_MAFPC_CRC_ON[] = {
	0xD8, 0x15,
};
static DEFINE_STATIC_PACKET(unbound1_a3_s0_mafpc_crc_on, DSI_PKT_TYPE_WR, UNBOUND1_A3_S0_MAFPC_CRC_ON, 0x27);

static u8 UNBOUND1_A3_S0_MAFPC_CRC_BIST_ON[] = {
	0xBF,
	0x01, 0x07, 0xFF, 0x00, 0x00, 0x10, 0x00, 0x00, 0x00
};
static DEFINE_STATIC_PACKET(unbound1_a3_s0_mafpc_crc_bist_on, DSI_PKT_TYPE_WR, UNBOUND1_A3_S0_MAFPC_CRC_BIST_ON, 0x00);


static u8 UNBOUND1_A3_S0_MAFPC_CRC_BIST_OFF[] = {
	0xBF,
	0x00, 0x07, 0xFF, 0x00, 0x00, 0x10, 0x00, 0x00, 0x00
};
static DEFINE_STATIC_PACKET(unbound1_a3_s0_mafpc_crc_bist_off, DSI_PKT_TYPE_WR, UNBOUND1_A3_S0_MAFPC_CRC_BIST_OFF, 0x00);


static u8 UNBOUND1_A3_S0_MAFPC_CRC_ENABLE[] = {
	0x87,
	0x11, 0x09, 0x0F, 0x00, 0x00, 0x81, 0x81, 0x81,
	0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
	0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
	0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
	0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
	0xff, 0xff, 0xff, 0xff,
};
static DEFINE_STATIC_PACKET(unbound1_a3_s0_mafpc_crc_enable, DSI_PKT_TYPE_WR, UNBOUND1_A3_S0_MAFPC_CRC_ENABLE, 0);

static u8 UNBOUND1_A3_S0_MAFPC_LUMINANCE_UPDATE[] = {
	0xF7, 0x01
};
static DEFINE_STATIC_PACKET(unbound1_a3_s0_mafpc_luminance_update, DSI_PKT_TYPE_WR, UNBOUND1_A3_S0_MAFPC_LUMINANCE_UPDATE, 0);

static u8 UNBOUND1_A3_S0_MAFPC_CRC_MDNIE_OFF[] = {
	0xDD,
	0x00
};
static DEFINE_STATIC_PACKET(unbound1_a3_s0_mafpc_crc_mdnie_off, DSI_PKT_TYPE_WR, UNBOUND1_A3_S0_MAFPC_CRC_MDNIE_OFF, 0);


static u8 UNBOUND1_A3_S0_MAFPC_CRC_MDNIE_ON[] = {
	0xDD,
	0x01
};
static DEFINE_STATIC_PACKET(unbound1_a3_s0_mafpc_crc_mdnie_on, DSI_PKT_TYPE_WR, UNBOUND1_A3_S0_MAFPC_CRC_MDNIE_ON, 0);

static u8 UNBOUND1_A3_S0_MAFPC_SELF_MASK_OFF[] = {
	0x7A,
	0x00
};
static DEFINE_STATIC_PACKET(unbound1_a3_s0_mafpc_self_mask_off, DSI_PKT_TYPE_WR, UNBOUND1_A3_S0_MAFPC_SELF_MASK_OFF, 0);

static u8 UNBOUND1_A3_S0_MAFPC_SELF_MASK_ON[] = {
	0x7A,
	0x21
};
static DEFINE_STATIC_PACKET(unbound1_a3_s0_mafpc_self_mask_on, DSI_PKT_TYPE_WR, UNBOUND1_A3_S0_MAFPC_SELF_MASK_ON, 0);

static DEFINE_PANEL_MDELAY(unbound1_a3_s0_crc_wait, 34);

static void *unbound1_a3_s0_mafpc_image_cmdtbl[] = {
	&KEYINFO(unbound1_a3_s0_level2_key_enable),
	&KEYINFO(unbound1_a3_s0_level3_key_enable),
	&PKTINFO(unbound1_a3_s0_mafpc_sr_path),
	&DLYINFO(unbound1_a3_s0_wait_1usec),
	&PKTINFO(unbound1_a3_s0_mafpc_disable),
	&DLYINFO(unbound1_a3_s0_wait_1_frame),
	&PKTINFO(unbound1_mafpc_default_img),
	&DLYINFO(unbound1_a3_s0_wait_100usec),
	&PKTINFO(unbound1_a3_s0_default_sr_path),
	&KEYINFO(unbound1_a3_s0_level3_key_disable),
	&KEYINFO(unbound1_a3_s0_level2_key_disable),
};

static void *unbound1_a3_s0_mafpc_check_cmdtbl[] = {
	&KEYINFO(unbound1_a3_s0_level1_key_enable),
	&KEYINFO(unbound1_a3_s0_level2_key_enable),
	&KEYINFO(unbound1_a3_s0_level3_key_enable),

	&PKTINFO(unbound1_a3_s0_dummy_2c),
	&PKTINFO(unbound1_a3_s0_mafpc_crc_on),
	&PKTINFO(unbound1_a3_s0_mafpc_crc_bist_on),
	&PKTINFO(unbound1_a3_s0_mafpc_crc_enable),
	&PKTINFO(unbound1_a3_s0_mafpc_luminance_update),
	&PKTINFO(unbound1_a3_s0_mafpc_self_mask_off),
	&PKTINFO(unbound1_a3_s0_mafpc_crc_mdnie_off),
//	&PKTINFO(unbound1_a3_s0_display_on),
	&DLYINFO(unbound1_a3_s0_crc_wait),
	&s6e3fab_restbl[RES_MAFPC_CRC],
//	&PKTINFO(unbound1_a3_s0_display_off),
	&PKTINFO(unbound1_a3_s0_mafpc_crc_bist_off),
	&PKTINFO(unbound1_a3_s0_mafpc_disable),
	&PKTINFO(unbound1_a3_s0_mafpc_self_mask_on),
	&PKTINFO(unbound1_a3_s0_mafpc_crc_mdnie_on),
	&KEYINFO(unbound1_a3_s0_level3_key_disable),
	&KEYINFO(unbound1_a3_s0_level2_key_disable),
	&KEYINFO(unbound1_a3_s0_level1_key_disable),
};

static DEFINE_PANEL_TIMER_MDELAY(unbound1_a3_s0_mafpc_delay, 120);
static DEFINE_PANEL_TIMER_BEGIN(unbound1_a3_s0_mafpc_delay,
		TIMER_DLYINFO(&unbound1_a3_s0_mafpc_delay));

static u8 UNBOUND1_A3_S0_MAFPC_ENABLE[] = {
	0x87,
	0x00, 0x09, 0x0f, 0x00, 0x00, 0x81, 0x81, 0x81,
	0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
	0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
	0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
	0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
	0xff, 0xff, 0xff, 0xff,
};

static DEFINE_PKTUI(unbound1_a3_s0_mafpc_enable, &unbound1_a3_s0_maptbl[MAFPC_ENA_MAPTBL], 1);
static DEFINE_VARIABLE_PACKET(unbound1_a3_s0_mafpc_enable, DSI_PKT_TYPE_WR, UNBOUND1_A3_S0_MAFPC_ENABLE, 0);
static u8 UNBOUND1_A3_S0_MAFPC_SCALE[] = {
	0x87,
	0xFF, 0xFF, 0xFF,
};
static DEFINE_PKTUI(unbound1_a3_s0_mafpc_scale, &unbound1_a3_s0_maptbl[MAFPC_SCALE_MAPTBL], 1);
static DEFINE_VARIABLE_PACKET(unbound1_a3_s0_mafpc_scale, DSI_PKT_TYPE_WR, UNBOUND1_A3_S0_MAFPC_SCALE, 0x08);
static void *unbound1_a3_s0_mafpc_on_cmdtbl[] = {
	&KEYINFO(unbound1_a3_s0_level2_key_enable),
	&PKTINFO(unbound1_a3_s0_mafpc_enable),
	&PKTINFO(unbound1_a3_s0_mafpc_scale),
	//&PKTINFO(unbound1_a3_s0_mafpc_luminance_update),
	&KEYINFO(unbound1_a3_s0_level2_key_disable),
};

static void *unbound1_a3_s0_mafpc_off_cmdtbl[] = {
	&KEYINFO(unbound1_a3_s0_level2_key_enable),
	&PKTINFO(unbound1_a3_s0_mafpc_disable),
	&KEYINFO(unbound1_a3_s0_level2_key_disable),
};
#endif


static struct seqinfo SEQINFO(unbound1_a3_s0_set_bl_param_seq);
static struct seqinfo SEQINFO(unbound1_a3_s0_set_fps_param_seq);

static void *unbound1_a3_s0_init_cmdtbl[] = {
	&DLYINFO(unbound1_a3_s0_wait_10msec),
	&KEYINFO(unbound1_a3_s0_level1_key_enable),
	&PKTINFO(unbound1_a3_s0_dsc),
	&PKTINFO(unbound1_a3_s0_pps),
	&KEYINFO(unbound1_a3_s0_level2_key_enable),
	&KEYINFO(unbound1_a3_s0_level3_key_enable),
	&PKTINFO(unbound1_a3_s0_dummy_2c),
#ifdef CONFIG_SUPPORT_HMD
	&PKTINFO(unbound1_a3_s0_hmd_setting),
	&PKTINFO(unbound1_a3_s0_gamma_update_enable),
#endif
	&PKTINFO(unbound1_a3_s0_sleep_out),
	&DLYINFO(unbound1_a3_s0_wait_10msec),
	&TIMER_DLYINFO_BEGIN(unbound1_a3_s0_wait_sleep_out_120msec),
#ifdef CONFIG_SUPPORT_MAFPC
	&PKTINFO(unbound1_a3_s0_mafpc_sr_path),
	&DLYINFO(unbound1_a3_s0_wait_1usec),
	&PKTINFO(unbound1_a3_s0_mafpc_disable),
	&DLYINFO(unbound1_a3_s0_wait_1_frame),
	&PKTINFO(unbound1_mafpc_default_img),
	&DLYINFO(unbound1_a3_s0_wait_100usec),
	&PKTINFO(unbound1_a3_s0_default_sr_path),
#endif
	&TIMER_DLYINFO(unbound1_a3_s0_wait_sleep_out_120msec),
	&PKTINFO(unbound1_a3_s0_caset),
	&PKTINFO(unbound1_a3_s0_paset),

#ifdef CONFIG_DYNAMIC_MIPI
	&PKTINFO(unbound1_a3_s0_ddi_osc),
	&PKTINFO(unbound1_a3_s0_fosc_update1),
	&PKTINFO(unbound1_a3_s0_fosc_update2),
#endif
	&PKTINFO(unbound1_a3_s0_te_on),
	&PKTINFO(unbound1_a3_s0_err_fg_1),
	&PKTINFO(unbound1_a3_s0_err_fg_2),
	&PKTINFO(unbound1_a3_s0_tsp_hsync),
	&PKTINFO(unbound1_a3_s0_isc),
	&PKTINFO(unbound1_a3_s0_dia_onoff),
	&PKTINFO(unbound1_a3_s0_set_ffc),
	&PKTINFO(unbound1_a3_s0_smooth_dimming_init),
	&PKTINFO(unbound1_a3_s0_gamma_normalize),

	/* set brightness & fps */
	&SEQINFO(unbound1_a3_s0_set_fps_param_seq),
	&SEQINFO(unbound1_a3_s0_set_bl_param_seq),
#ifdef CONFIG_SUPPORT_HMD
	&PKTINFO(unbound1_a3_s0_hmd_setting),
#endif
	&PKTINFO(unbound1_a3_s0_gamma_update_enable),
#ifdef CONFIG_EXYNOS_DECON_LCD_COPR
	&SEQINFO(unbound1_a3_s0_copr_seqtbl[COPR_SET_SEQ]),
#endif
	&KEYINFO(unbound1_a3_s0_level3_key_disable),
	&KEYINFO(unbound1_a3_s0_level2_key_disable),
	&KEYINFO(unbound1_a3_s0_level1_key_disable),
#if defined(CONFIG_SUPPORT_FAST_DISCHARGE)
	&KEYINFO(unbound1_a3_s0_level2_key_enable),
	&PKTINFO(unbound1_a3_s0_fast_discharge),
	&KEYINFO(unbound1_a3_s0_level2_key_disable),
	&DLYINFO(unbound1_a3_s0_wait_fastdischarge_120msec),
#endif
};

static void *unbound1_a3_s0_id_read_cmdtbl[] = {
	&KEYINFO(unbound1_a3_s0_level1_key_enable),
	&KEYINFO(unbound1_a3_s0_level2_key_enable),
	&KEYINFO(unbound1_a3_s0_level3_key_enable),
	&s6e3fab_restbl[RES_ID],
	&KEYINFO(unbound1_a3_s0_level3_key_disable),
	&KEYINFO(unbound1_a3_s0_level2_key_disable),
	&KEYINFO(unbound1_a3_s0_level1_key_disable),
};

static void *unbound1_a3_s0_res_init_cmdtbl[] = {
	&KEYINFO(unbound1_a3_s0_level1_key_enable),
	&KEYINFO(unbound1_a3_s0_level2_key_enable),
	&KEYINFO(unbound1_a3_s0_level3_key_enable),
	&s6e3fab_restbl[RES_COORDINATE],
	&s6e3fab_restbl[RES_CODE],
	&s6e3fab_restbl[RES_DATE],
	&s6e3fab_restbl[RES_OCTA_ID],
	&s6e3fab_restbl[RES_ELVSS_CAL_OFFSET],
#ifdef CONFIG_DISPLAY_USE_INFO
	&s6e3fab_restbl[RES_CHIP_ID],
	&s6e3fab_restbl[RES_SELF_DIAG],
	&s6e3fab_restbl[RES_ERR_FG],
	&s6e3fab_restbl[RES_DSI_ERR],
#endif
	&s6e3fab_restbl[RES_GAMMA_MTP_120HS_0],
	&s6e3fab_restbl[RES_GAMMA_MTP_120HS_1],
	&s6e3fab_restbl[RES_GAMMA_MTP_120HS_2],
	&s6e3fab_restbl[RES_GAMMA_MTP_120HS_3],
	&s6e3fab_restbl[RES_GAMMA_MTP_120HS_4],
	&s6e3fab_restbl[RES_GAMMA_MTP_120HS_5],
	&s6e3fab_restbl[RES_GAMMA_MTP_120HS_6],
	&s6e3fab_restbl[RES_GAMMA_MTP_120HS_7],
	&s6e3fab_restbl[RES_GAMMA_MTP_60HS_0],
	&s6e3fab_restbl[RES_GAMMA_MTP_60HS_1],
	&s6e3fab_restbl[RES_GAMMA_MTP_60HS_2],
	&s6e3fab_restbl[RES_GAMMA_MTP_60HS_3],
	&s6e3fab_restbl[RES_GAMMA_MTP_60HS_4],
	&s6e3fab_restbl[RES_GAMMA_MTP_60HS_5],
	&s6e3fab_restbl[RES_GAMMA_MTP_60HS_6],
	&s6e3fab_restbl[RES_GAMMA_MTP_60HS_7],
	&KEYINFO(unbound1_a3_s0_level3_key_disable),
	&KEYINFO(unbound1_a3_s0_level2_key_disable),
	&KEYINFO(unbound1_a3_s0_level1_key_disable),
};

#ifdef CONFIG_SUPPORT_GM2_FLASH
static void *unbound1_a3_s0_gm2_flash_res_init_cmdtbl[] = {
	&s6e3fab_restbl[RES_GAMMA_MTP_60NS_0],
	&s6e3fab_restbl[RES_GAMMA_MTP_60NS_1],
	&s6e3fab_restbl[RES_GAMMA_MTP_60NS_2],
	&s6e3fab_restbl[RES_GAMMA_MTP_60NS_3],
	&s6e3fab_restbl[RES_GAMMA_MTP_60NS_4],
	&s6e3fab_restbl[RES_GAMMA_MTP_60NS_5],
	&s6e3fab_restbl[RES_GAMMA_MTP_60NS_6],
	&s6e3fab_restbl[RES_GAMMA_MTP_60NS_7],
};
#endif

static void *unbound1_a3_s0_set_bl_param_cmdtbl[] = {
	&PKTINFO(unbound1_a3_s0_gamma_0),
	&PKTINFO(unbound1_a3_s0_gamma_1),
	&PKTINFO(unbound1_a3_s0_gamma_2),
	&PKTINFO(unbound1_a3_s0_gamma_3),
	&PKTINFO(unbound1_a3_s0_gamma_4),
	//&PKTINFO(unbound1_a3_s0_aor_fq_low),
	&PKTINFO(unbound1_a3_s0_aor_fq_high),
	&PKTINFO(unbound1_a3_s0_hbm_and_transition),
	&PKTINFO(unbound1_a3_s0_acl_control),
	&PKTINFO(unbound1_a3_s0_acl_dim),
	&PKTINFO(unbound1_a3_s0_acl_onoff),
	&PKTINFO(unbound1_a3_s0_elvss),
	&PKTINFO(unbound1_a3_s0_irc_mode),
	&PKTINFO(unbound1_a3_s0_tset),
	&PKTINFO(unbound1_a3_s0_wrdisbv),
#ifdef CONFIG_SUPPORT_MAFPC
	&PKTINFO(unbound1_a3_s0_mafpc_scale),
#endif
#ifdef CONFIG_SUPPORT_XTALK_MODE
	&PKTINFO(unbound1_a3_s0_xtalk_mode),
#endif
};

static DEFINE_SEQINFO(unbound1_a3_s0_set_bl_param_seq,
		unbound1_a3_s0_set_bl_param_cmdtbl);

static void *unbound1_a3_s0_set_bl_cmdtbl[] = {
	&KEYINFO(unbound1_a3_s0_level1_key_enable),
	&KEYINFO(unbound1_a3_s0_level2_key_enable),
	&SEQINFO(unbound1_a3_s0_set_bl_param_seq),
	&PKTINFO(unbound1_a3_s0_gamma_update_enable),
	&KEYINFO(unbound1_a3_s0_level2_key_disable),
	&KEYINFO(unbound1_a3_s0_level1_key_disable),
	&CONDINFO_IF(unbound1_a3_s0_cond_is_first_set_bl),
		&DLYINFO(unbound1_a3_s0_wait_20msec),
		&KEYINFO(unbound1_a3_s0_level2_key_enable),
		&PKTINFO(unbound1_a3_s0_smooth_dimming),
		&KEYINFO(unbound1_a3_s0_level2_key_disable),
	&CONDINFO_FI(unbound1_a3_s0_cond_is_first_set_bl),
};

static void *unbound1_a3_s0_set_fps_param_cmdtbl[] = {
	/* enable h/w te modulation if necessary */
	&PKTINFO(unbound1_a3_s0_set_te_frame_sel),
	/* fps & osc setting */
	&PKTINFO(unbound1_a3_s0_aor),
	&PKTINFO(unbound1_a3_s0_osc_0),
	&PKTINFO(unbound1_a3_s0_osc_1),
	&CONDINFO_IF(unbound1_a3_s0_cond_is_id_gte_03),
		&PKTINFO(unbound1_a3_s0_ltps_0),
		&PKTINFO(unbound1_a3_s0_ltps_1),
	&CONDINFO_FI(unbound1_a3_s0_cond_is_id_gte_03),
	&PKTINFO(unbound1_a3_s0_osc),
	&PKTINFO(unbound1_a3_s0_fps),
	&PKTINFO(unbound1_a3_s0_vfp_fq_low),
	&PKTINFO(unbound1_a3_s0_vfp_fq_high),
	&PKTINFO(unbound1_a3_s0_aid),
	&PKTINFO(unbound1_a3_s0_aid_hbm),
	&PKTINFO(unbound1_a3_s0_src_amp),
};

static DEFINE_SEQINFO(unbound1_a3_s0_set_fps_param_seq,
		unbound1_a3_s0_set_fps_param_cmdtbl);

static void *unbound1_a3_s0_set_fps_cmdtbl[] = {
	&CONDINFO_IF(unbound1_a3_s0_cond_is_wait_vsync_needed),
		&DLYINFO(unbound1_a3_s0_wait_1_vsync),
	&CONDINFO_FI(unbound1_a3_s0_cond_is_wait_vsync_needed),
	&KEYINFO(unbound1_a3_s0_level2_key_enable),
	&SEQINFO(unbound1_a3_s0_set_bl_param_seq),
	&SEQINFO(unbound1_a3_s0_set_fps_param_seq),
	&PKTINFO(unbound1_a3_s0_gamma_update_enable),
	&KEYINFO(unbound1_a3_s0_level2_key_disable),
};

#if defined(CONFIG_PANEL_DISPLAY_MODE)
static void *unbound1_a3_s0_display_mode_cmdtbl[] = {
	&CONDINFO_IF(unbound1_a3_s0_cond_is_panel_state_not_lpm),
		&CONDINFO_IF(unbound1_a3_s0_cond_is_wait_vsync_needed),
			&DLYINFO(unbound1_a3_s0_wait_1_vsync),
		&CONDINFO_FI(unbound1_a3_s0_cond_is_wait_vsync_needed),
		&KEYINFO(unbound1_a3_s0_level1_key_enable),
		&KEYINFO(unbound1_a3_s0_level2_key_enable),
		&SEQINFO(unbound1_a3_s0_set_bl_param_seq),
		&SEQINFO(unbound1_a3_s0_set_fps_param_seq),
		&PKTINFO(unbound1_a3_s0_gamma_update_enable),
		&KEYINFO(unbound1_a3_s0_level2_key_disable),
		&KEYINFO(unbound1_a3_s0_level1_key_disable),
	&CONDINFO_FI(unbound1_a3_s0_cond_is_panel_state_not_lpm),
};
#endif

#ifdef CONFIG_SUPPORT_HMD
static void *unbound1_a3_s0_hmd_on_cmdtbl[] = {
	&KEYINFO(unbound1_a3_s0_level2_key_enable),
	&PKTINFO(unbound1_a3_s0_hmd_on_aor),
	&PKTINFO(unbound1_a3_s0_hmd_on_aor_cycle),
	&PKTINFO(unbound1_a3_s0_hmd_on_ltps),
	&PKTINFO(unbound1_a3_s0_gamma_update_enable),
	&KEYINFO(unbound1_a3_s0_level2_key_disable),
};

static void *unbound1_a3_s0_hmd_off_cmdtbl[] = {
	&KEYINFO(unbound1_a3_s0_level2_key_enable),
	&PKTINFO(unbound1_a3_s0_hmd_off_aor),
	&PKTINFO(unbound1_a3_s0_hmd_off_aor_cycle),
	&PKTINFO(unbound1_a3_s0_hmd_off_ltps),
	&PKTINFO(unbound1_a3_s0_gamma_update_enable),
	&KEYINFO(unbound1_a3_s0_level2_key_disable),
};

static void *unbound1_a3_s0_hmd_bl_cmdtbl[] = {
	&KEYINFO(unbound1_a3_s0_level1_key_enable),
	&KEYINFO(unbound1_a3_s0_level2_key_enable),
	&PKTINFO(unbound1_a3_s0_hmd_wrdisbv),
	&KEYINFO(unbound1_a3_s0_level2_key_disable),
	&KEYINFO(unbound1_a3_s0_level1_key_disable),
};
#endif

static void *unbound1_a3_s0_display_on_cmdtbl[] = {
#ifdef CONFIG_SUPPORT_MAFPC
	&KEYINFO(unbound1_a3_s0_level2_key_enable),
	&PKTINFO(unbound1_a3_s0_mafpc_enable),
	&PKTINFO(unbound1_a3_s0_mafpc_scale),
	&KEYINFO(unbound1_a3_s0_level2_key_disable),
#endif
	&KEYINFO(unbound1_a3_s0_level1_key_enable),
	&PKTINFO(unbound1_a3_s0_display_on),
	&KEYINFO(unbound1_a3_s0_level1_key_disable),
};

static void *unbound1_a3_s0_display_off_cmdtbl[] = {
	&KEYINFO(unbound1_a3_s0_level1_key_enable),
	&PKTINFO(unbound1_a3_s0_display_off),
	&KEYINFO(unbound1_a3_s0_level1_key_disable),
};

static void *unbound1_a3_s0_exit_cmdtbl[] = {
 	&KEYINFO(unbound1_a3_s0_level1_key_enable),
	&KEYINFO(unbound1_a3_s0_level2_key_enable),
	&s6e3fab_dmptbl[DUMP_RDDPM_SLEEP_IN],
#ifdef CONFIG_DISPLAY_USE_INFO
	&s6e3fab_dmptbl[DUMP_RDDSM],
	&s6e3fab_dmptbl[DUMP_ERR],
	&s6e3fab_dmptbl[DUMP_ERR_FG],
	&s6e3fab_dmptbl[DUMP_DSI_ERR],
	&s6e3fab_dmptbl[DUMP_SELF_DIAG],
#endif
	&KEYINFO(unbound1_a3_s0_level2_key_disable),
	&PKTINFO(unbound1_a3_s0_sleep_in),
	&KEYINFO(unbound1_a3_s0_level1_key_disable),
	&DLYINFO(unbound1_a3_s0_wait_sleep_in),
};

static void *unbound1_a3_s0_dia_onoff_cmdtbl[] = {
	&KEYINFO(unbound1_a3_s0_level2_key_enable),
	&PKTINFO(unbound1_a3_s0_dia_onoff),
	&KEYINFO(unbound1_a3_s0_level2_key_disable),
};

#if defined(CONFIG_SUPPORT_FAST_DISCHARGE)
static void *unbound1_a3_s0_fast_discharge_cmdtbl[] = {
	&KEYINFO(unbound1_a3_s0_level2_key_enable),
	&PKTINFO(unbound1_a3_s0_fast_discharge),
	&KEYINFO(unbound1_a3_s0_level2_key_disable),
	&DLYINFO(unbound1_a3_s0_wait_fastdischarge_120msec),
};
#endif

static void *unbound1_a3_s0_alpm_enter_cmdtbl[] = {
	&KEYINFO(unbound1_a3_s0_level1_key_enable),
	&KEYINFO(unbound1_a3_s0_level2_key_enable),
	&PKTINFO(unbound1_a3_s0_clr_te_frame_sel),
	&PKTINFO(unbound1_a3_s0_lpm_enter_aor),
	&PKTINFO(unbound1_a3_s0_lpm_enter_elvss),
	&PKTINFO(unbound1_a3_s0_lpm_enter_vfp_60ns),
	&PKTINFO(unbound1_a3_s0_lpm_enter_osc),
	&PKTINFO(unbound1_a3_s0_lpm_enter_fps),
	&PKTINFO(unbound1_a3_s0_gamma_update_enable),
	&DLYINFO(unbound1_a3_s0_wait_17msec),
	&PKTINFO(unbound1_a3_s0_lpm_enter_seamless_ctrl),
	&PKTINFO(unbound1_a3_s0_lpm_nit),
	&PKTINFO(unbound1_a3_s0_lpm_mode),
	&KEYINFO(unbound1_a3_s0_level3_key_enable),
	&PKTINFO(unbound1_a3_s0_lpm_avs_on),
	&KEYINFO(unbound1_a3_s0_level3_key_disable),
	&PKTINFO(unbound1_a3_s0_lpm_on),
	&PKTINFO(unbound1_a3_s0_lpm_avc2_off),
	&PKTINFO(unbound1_a3_s0_lpm_wrdisbv),
	&PKTINFO(unbound1_a3_s0_gamma_update_enable),
	&KEYINFO(unbound1_a3_s0_level2_key_disable),
	&KEYINFO(unbound1_a3_s0_level1_key_disable),
};

static void *unbound1_a3_s0_alpm_exit_cmdtbl[] = {
	&KEYINFO(unbound1_a3_s0_level1_key_enable),
	&KEYINFO(unbound1_a3_s0_level2_key_enable),
	&PKTINFO(unbound1_a3_s0_lpm_exit_swire_no_pulse),
	&PKTINFO(unbound1_a3_s0_lpm_exit_seamless_ctrl),
	&PKTINFO(unbound1_a3_s0_lpm_avc2_on),
	&PKTINFO(unbound1_a3_s0_lpm_off),
	&PKTINFO(unbound1_a3_s0_acl_control),
	&PKTINFO(unbound1_a3_s0_acl_onoff),
	&PKTINFO(unbound1_a3_s0_gamma_update_enable),
	&PKTINFO(unbound1_a3_s0_osc),
	&PKTINFO(unbound1_a3_s0_fps),
	&PKTINFO(unbound1_a3_s0_lpm_exit_elvss),
	&PKTINFO(unbound1_a3_s0_lpm_exit_aor),
	&PKTINFO(unbound1_a3_s0_gamma_update_enable),
	&DLYINFO(unbound1_a3_s0_wait_34msec),
	&KEYINFO(unbound1_a3_s0_level2_key_disable),
	&KEYINFO(unbound1_a3_s0_level1_key_disable),
};

#ifdef CONFIG_SUPPORT_DYNAMIC_HLPM
static void *unbound1_a3_s0_dynamic_hlpm_on_cmdtbl[] = {
	&KEYINFO(unbound1_a3_s0_level2_key_enable),
	&PKTINFO(unbound1_a3_s0_dynamic_hlpm_enable),
	&KEYINFO(unbound1_a3_s0_level2_key_disable),
};

static void *unbound1_a3_s0_dynamic_hlpm_off_cmdtbl[] = {
	&KEYINFO(unbound1_a3_s0_level2_key_enable),
	&PKTINFO(unbound1_a3_s0_dynamic_hlpm_disable),
	&KEYINFO(unbound1_a3_s0_level2_key_disable),
};
#endif

static void *unbound1_a3_s0_mcd_on_cmdtbl[] = {
	&KEYINFO(unbound1_a3_s0_level2_key_enable),
	&PKTINFO(unbound1_a3_s0_mcd_on_01),
	&PKTINFO(unbound1_a3_s0_mcd_on_02),
	&PKTINFO(unbound1_a3_s0_mcd_on_03),
	&PKTINFO(unbound1_a3_s0_mcd_on_04),
	&PKTINFO(unbound1_a3_s0_mcd_on_05),
	&PKTINFO(unbound1_a3_s0_gamma_update_enable),
	&DLYINFO(unbound1_a3_s0_wait_100msec),
	&KEYINFO(unbound1_a3_s0_level2_key_disable),
};

static void *unbound1_a3_s0_mcd_off_cmdtbl[] = {
	&KEYINFO(unbound1_a3_s0_level2_key_enable),
	&PKTINFO(unbound1_a3_s0_mcd_off_01),
	&PKTINFO(unbound1_a3_s0_mcd_off_02),
	&PKTINFO(unbound1_a3_s0_mcd_off_03),
	&PKTINFO(unbound1_a3_s0_mcd_off_04),
	&PKTINFO(unbound1_a3_s0_mcd_off_05),
	&PKTINFO(unbound1_a3_s0_gamma_update_enable),
	&DLYINFO(unbound1_a3_s0_wait_100msec),
	&KEYINFO(unbound1_a3_s0_level2_key_disable),
};

#ifdef CONFIG_SUPPORT_GRAYSPOT_TEST
static void *unbound1_a3_s0_grayspot_on_cmdtbl[] = {
	&KEYINFO(unbound1_a3_s0_level2_key_enable),
	&PKTINFO(unbound1_a3_s0_grayspot_on_01),
	&PKTINFO(unbound1_a3_s0_grayspot_on_02),
	&PKTINFO(unbound1_a3_s0_grayspot_on_03),
	&PKTINFO(unbound1_a3_s0_grayspot_on_04),
	&PKTINFO(unbound1_a3_s0_gamma_update_enable),
	&DLYINFO(unbound1_a3_s0_wait_100msec),
	&KEYINFO(unbound1_a3_s0_level2_key_disable),
};

static void *unbound1_a3_s0_grayspot_off_cmdtbl[] = {
	&KEYINFO(unbound1_a3_s0_level2_key_enable),
	&PKTINFO(unbound1_a3_s0_grayspot_off_01),
	&PKTINFO(unbound1_a3_s0_grayspot_off_02),
	&PKTINFO(unbound1_a3_s0_grayspot_off_03),
	&PKTINFO(unbound1_a3_s0_grayspot_off_04),
	&PKTINFO(unbound1_a3_s0_gamma_update_enable),
	&DLYINFO(unbound1_a3_s0_wait_100msec),
	&KEYINFO(unbound1_a3_s0_level2_key_disable),
};
#endif

#ifdef CONFIG_SUPPORT_MST
static void *unbound1_a3_s0_mst_on_cmdtbl[] = {
	&KEYINFO(unbound1_a3_s0_level2_key_enable),
	&PKTINFO(unbound1_a3_s0_mst_on_01),
	&PKTINFO(unbound1_a3_s0_mst_on_02),
	&PKTINFO(unbound1_a3_s0_mst_on_03),
	&PKTINFO(unbound1_a3_s0_gamma_update_enable),
	&KEYINFO(unbound1_a3_s0_level2_key_disable),
};

static void *unbound1_a3_s0_mst_off_cmdtbl[] = {
	&KEYINFO(unbound1_a3_s0_level2_key_enable),
	&PKTINFO(unbound1_a3_s0_mst_off_01),
	&PKTINFO(unbound1_a3_s0_mst_off_02),
	&PKTINFO(unbound1_a3_s0_mst_off_03),
	&PKTINFO(unbound1_a3_s0_gamma_update_enable),
	&KEYINFO(unbound1_a3_s0_level2_key_disable),
};
#endif

#ifdef CONFIG_SUPPORT_GRAM_CHECKSUM
static void *unbound1_a3_s0_gct_enter_cmdtbl[] = {
	&KEYINFO(unbound1_a3_s0_level1_key_enable),
	&PKTINFO(unbound1_a3_s0_sw_reset),
	&DLYINFO(unbound1_a3_s0_wait_120msec),
	&KEYINFO(unbound1_a3_s0_level1_key_disable),
	&KEYINFO(unbound1_a3_s0_level1_key_enable),
	&KEYINFO(unbound1_a3_s0_level2_key_enable),
	&KEYINFO(unbound1_a3_s0_level3_key_enable),
	&PKTINFO(unbound1_a3_s0_vddm_init),
	&PKTINFO(unbound1_a3_s0_sleep_out),
	&DLYINFO(unbound1_a3_s0_wait_sleep_out_20msec),
	&PKTINFO(unbound1_a3_s0_display_on),
	&PKTINFO(unbound1_a3_s0_gct_dsc),
	&PKTINFO(unbound1_a3_s0_gct_pps),
	&PKTINFO(unbound1_a3_s0_fusing_update_on),
	&KEYINFO(unbound1_a3_s0_level3_key_disable),
	&KEYINFO(unbound1_a3_s0_level2_key_disable),
	&KEYINFO(unbound1_a3_s0_level1_key_disable),
};

static void *unbound1_a3_s0_gct_vddm_cmdtbl[] = {
	&KEYINFO(unbound1_a3_s0_level2_key_enable),
	&KEYINFO(unbound1_a3_s0_level3_key_enable),
	&PKTINFO(unbound1_a3_s0_vddm_volt),
	&KEYINFO(unbound1_a3_s0_level2_key_disable),
	&KEYINFO(unbound1_a3_s0_level3_key_disable),
	&DLYINFO(unbound1_a3_s0_wait_vddm_update),
};

static void *unbound1_a3_s0_gct_img_0_update_cmdtbl[] = {
	&KEYINFO(unbound1_a3_s0_level2_key_enable),
	&KEYINFO(unbound1_a3_s0_level3_key_enable),
	&PKTINFO(unbound1_a3_s0_gram_chksum_start),
	&PKTINFO(unbound1_a3_s0_gram_inv_img_pattern_on),
	&DLYINFO(unbound1_a3_s0_wait_20msec_gram_img_update),
	&PKTINFO(unbound1_a3_s0_gram_img_pattern_off),
	&DLYINFO(unbound1_a3_s0_wait_20msec_gram_img_update),
	&PKTINFO(unbound1_a3_s0_gram_img_pattern_on),
	&DLYINFO(unbound1_a3_s0_wait_gram_img_update),
	&s6e3fab_restbl[RES_GRAM_CHECKSUM],
	&KEYINFO(unbound1_a3_s0_level3_key_disable),
	&KEYINFO(unbound1_a3_s0_level2_key_disable),
};

static void *unbound1_a3_s0_gct_img_1_update_cmdtbl[] = {
	&KEYINFO(unbound1_a3_s0_level2_key_enable),
	&KEYINFO(unbound1_a3_s0_level3_key_enable),
	&PKTINFO(unbound1_a3_s0_gram_img_pattern_off),
	&DLYINFO(unbound1_a3_s0_wait_20msec_gram_img_update),
	&PKTINFO(unbound1_a3_s0_gram_img_pattern_on),
	&DLYINFO(unbound1_a3_s0_wait_gram_img_update),
	&s6e3fab_restbl[RES_GRAM_CHECKSUM],
	&PKTINFO(unbound1_a3_s0_gram_img_pattern_off),
	&KEYINFO(unbound1_a3_s0_level3_key_disable),
	&KEYINFO(unbound1_a3_s0_level2_key_disable),
};

static void *unbound1_a3_s0_gct_exit_cmdtbl[] = {
	&KEYINFO(unbound1_a3_s0_level2_key_enable),
	&KEYINFO(unbound1_a3_s0_level3_key_enable),
	&PKTINFO(unbound1_a3_s0_vddm_orig),
	&PKTINFO(unbound1_a3_s0_fusing_update_off),
	&KEYINFO(unbound1_a3_s0_level3_key_disable),
	&KEYINFO(unbound1_a3_s0_level2_key_disable),
};
#endif

#ifdef CONFIG_SUPPORT_CCD_TEST
static void *unbound1_a3_s0_ccd_test_cmdtbl[] = {
	&KEYINFO(unbound1_a3_s0_level2_key_enable),
	&PKTINFO(unbound1_a3_s0_ccd_test_enable),
	&DLYINFO(unbound1_a3_s0_wait_1msec),
	&s6e3fab_restbl[RES_CCD_STATE],
	&PKTINFO(unbound1_a3_s0_ccd_test_disable),
	&KEYINFO(unbound1_a3_s0_level2_key_disable),
};
#endif

#ifdef CONFIG_SUPPORT_DDI_CMDLOG
static void *unbound1_a3_s0_cmdlog_dump_cmdtbl[] = {
	&KEYINFO(unbound1_a3_s0_level2_key_enable),
	&s6e3fab_dmptbl[DUMP_CMDLOG],
	&KEYINFO(unbound1_a3_s0_level2_key_disable),
};
#endif


static void *unbound1_a3_s0_check_condition_cmdtbl[] = {
	&KEYINFO(unbound1_a3_s0_level1_key_enable),
	&KEYINFO(unbound1_a3_s0_level2_key_enable),
	&s6e3fab_dmptbl[DUMP_RDDPM],
	&s6e3fab_dmptbl[DUMP_SELF_DIAG],
#ifdef CONFIG_SUPPORT_MAFPC
	&KEYINFO(unbound1_a3_s0_level3_key_enable),
	&s6e3fab_dmptbl[DUMP_MAFPC],
	&s6e3fab_dmptbl[DUMP_MAFPC_FLASH],
	&s6e3fab_dmptbl[DUMP_SELF_MASK_CRC],
	&KEYINFO(unbound1_a3_s0_level3_key_disable),
#endif
	&KEYINFO(unbound1_a3_s0_level2_key_disable),
	&KEYINFO(unbound1_a3_s0_level1_key_disable),
};

static void *unbound1_a3_s0_dump_cmdtbl[] = {
	&KEYINFO(unbound1_a3_s0_level1_key_enable),
	&KEYINFO(unbound1_a3_s0_level2_key_enable),
	&KEYINFO(unbound1_a3_s0_level3_key_enable),
	&s6e3fab_dmptbl[DUMP_RDDPM],
	&s6e3fab_dmptbl[DUMP_RDDSM],
	&s6e3fab_dmptbl[DUMP_ERR],
	&s6e3fab_dmptbl[DUMP_ERR_FG],
	&s6e3fab_dmptbl[DUMP_DSI_ERR],
	&s6e3fab_dmptbl[DUMP_SELF_DIAG],
	&KEYINFO(unbound1_a3_s0_level3_key_disable),
	&KEYINFO(unbound1_a3_s0_level2_key_disable),
	&KEYINFO(unbound1_a3_s0_level1_key_disable),
};

static void *unbound1_a3_s0_dummy_cmdtbl[] = {
	NULL,
};

static struct seqinfo unbound1_a3_s0_seqtbl[MAX_PANEL_SEQ] = {
	[PANEL_INIT_SEQ] = SEQINFO_INIT("init-seq", unbound1_a3_s0_init_cmdtbl),
	[PANEL_RES_INIT_SEQ] = SEQINFO_INIT("resource-init-seq", unbound1_a3_s0_res_init_cmdtbl),
	[PANEL_ID_READ_SEQ] = SEQINFO_INIT("id-read-seq", unbound1_a3_s0_id_read_cmdtbl),
	[PANEL_SET_BL_SEQ] = SEQINFO_INIT("set-bl-seq", unbound1_a3_s0_set_bl_cmdtbl),
#ifdef CONFIG_SUPPORT_HMD
	[PANEL_HMD_ON_SEQ] = SEQINFO_INIT("hmd-on-seq", unbound1_a3_s0_hmd_on_cmdtbl),
	[PANEL_HMD_OFF_SEQ] = SEQINFO_INIT("hmd-off-seq", unbound1_a3_s0_hmd_off_cmdtbl),
	[PANEL_HMD_BL_SEQ] = SEQINFO_INIT("hmd-bl-seq", unbound1_a3_s0_hmd_bl_cmdtbl),
#endif
	[PANEL_DISPLAY_ON_SEQ] = SEQINFO_INIT("display-on-seq", unbound1_a3_s0_display_on_cmdtbl),
	[PANEL_DISPLAY_OFF_SEQ] = SEQINFO_INIT("display-off-seq", unbound1_a3_s0_display_off_cmdtbl),
	[PANEL_EXIT_SEQ] = SEQINFO_INIT("exit-seq", unbound1_a3_s0_exit_cmdtbl),
	[PANEL_ALPM_ENTER_SEQ] = SEQINFO_INIT("alpm-enter-seq", unbound1_a3_s0_alpm_enter_cmdtbl),
	[PANEL_ALPM_EXIT_SEQ] = SEQINFO_INIT("alpm-exit-seq", unbound1_a3_s0_alpm_exit_cmdtbl),
	[PANEL_FPS_SEQ] = SEQINFO_INIT("set-fps-seq", unbound1_a3_s0_set_fps_cmdtbl),
	[PANEL_DIA_ONOFF_SEQ] = SEQINFO_INIT("dia-onoff-seq", unbound1_a3_s0_dia_onoff_cmdtbl),
#if defined(CONFIG_PANEL_DISPLAY_MODE)
	[PANEL_DISPLAY_MODE_SEQ] = SEQINFO_INIT("display-mode-seq", unbound1_a3_s0_display_mode_cmdtbl),
#endif
#ifdef CONFIG_SUPPORT_DYNAMIC_HLPM
	[PANEL_DYNAMIC_HLPM_ON_SEQ] = SEQINFO_INIT("dynamic-hlpm-on-seq", unbound1_a3_s0_dynamic_hlpm_on_cmdtbl),
	[PANEL_DYNAMIC_HLPM_OFF_SEQ] = SEQINFO_INIT("dynamic-hlpm-off-seq", unbound1_a3_s0_dynamic_hlpm_off_cmdtbl),
#endif
	[PANEL_MCD_ON_SEQ] = SEQINFO_INIT("mcd-on-seq", unbound1_a3_s0_mcd_on_cmdtbl),
	[PANEL_MCD_OFF_SEQ] = SEQINFO_INIT("mcd-off-seq", unbound1_a3_s0_mcd_off_cmdtbl),
#ifdef CONFIG_SUPPORT_GRAYSPOT_TEST
	[PANEL_GRAYSPOT_ON_SEQ] = SEQINFO_INIT("grayspot-on-seq", unbound1_a3_s0_grayspot_on_cmdtbl),
	[PANEL_GRAYSPOT_OFF_SEQ] = SEQINFO_INIT("grayspot-off-seq", unbound1_a3_s0_grayspot_off_cmdtbl),
#endif
#ifdef CONFIG_SUPPORT_MAFPC
	[PANEL_MAFPC_IMG_SEQ] = SEQINFO_INIT("mafpc-img-seq", unbound1_a3_s0_mafpc_image_cmdtbl),
	[PANEL_MAFPC_CHECKSUM_SEQ] = SEQINFO_INIT("mafpc-check-seq", unbound1_a3_s0_mafpc_check_cmdtbl),
	[PANEL_MAFPC_ON_SEQ] = SEQINFO_INIT("mafpc-on-seq", unbound1_a3_s0_mafpc_on_cmdtbl),
	[PANEL_MAFPC_OFF_SEQ] = SEQINFO_INIT("mafpc-off-seq", unbound1_a3_s0_mafpc_off_cmdtbl),
#endif
#ifdef CONFIG_SUPPORT_MST
	[PANEL_MST_ON_SEQ] = SEQINFO_INIT("mst-on-seq", unbound1_a3_s0_mst_on_cmdtbl),
	[PANEL_MST_OFF_SEQ] = SEQINFO_INIT("mst-off-seq", unbound1_a3_s0_mst_off_cmdtbl),
#endif
#ifdef CONFIG_SUPPORT_GRAM_CHECKSUM
	[PANEL_GCT_ENTER_SEQ] = SEQINFO_INIT("gct-enter-seq", unbound1_a3_s0_gct_enter_cmdtbl),
	[PANEL_GCT_VDDM_SEQ] = SEQINFO_INIT("gct-vddm-seq", unbound1_a3_s0_gct_vddm_cmdtbl),
	[PANEL_GCT_IMG_0_UPDATE_SEQ] = SEQINFO_INIT("gct-img-0-update-seq", unbound1_a3_s0_gct_img_0_update_cmdtbl),
	[PANEL_GCT_IMG_1_UPDATE_SEQ] = SEQINFO_INIT("gct-img-1-update-seq", unbound1_a3_s0_gct_img_1_update_cmdtbl),
	[PANEL_GCT_EXIT_SEQ] = SEQINFO_INIT("gct-exit-seq", unbound1_a3_s0_gct_exit_cmdtbl),
#endif
#ifdef CONFIG_SUPPORT_CCD_TEST
	[PANEL_CCD_TEST_SEQ] = SEQINFO_INIT("ccd-test-seq", unbound1_a3_s0_ccd_test_cmdtbl),
#endif
	[PANEL_CHECK_CONDITION_SEQ] = SEQINFO_INIT("check-condition-seq", unbound1_a3_s0_check_condition_cmdtbl),
	[PANEL_DUMP_SEQ] = SEQINFO_INIT("dump-seq", unbound1_a3_s0_dump_cmdtbl),
#ifdef CONFIG_SUPPORT_DDI_CMDLOG
	[PANEL_CMDLOG_DUMP_SEQ] = SEQINFO_INIT("cmdlog-dump-seq", unbound1_a3_s0_cmdlog_dump_cmdtbl),
#endif
#ifdef CONFIG_DYNAMIC_MIPI
	[PANEL_DM_SET_FFC_SEQ] = SEQINFO_INIT("dm-set_ffc-seq", unbound1_a3_s0_dm_set_ffc_cmdtbl),
	[PANEL_DM_OFF_FFC_SEQ] = SEQINFO_INIT("dm-off_ffc-seq", unbound1_a3_s0_dm_off_ffc_cmdtbl),
#endif
#if defined(CONFIG_SUPPORT_FAST_DISCHARGE)
	[PANEL_FD_SEQ] = SEQINFO_INIT("fast-discharge-seq", unbound1_a3_s0_fast_discharge_cmdtbl),
#endif
#ifdef CONFIG_SUPPORT_GM2_FLASH
	[PANEL_GM2_FLASH_RES_INIT_SEQ] = SEQINFO_INIT("gm2-flash-resource-init-seq", unbound1_a3_s0_gm2_flash_res_init_cmdtbl),
#endif
	[PANEL_DUMMY_SEQ] = SEQINFO_INIT("dummy-seq", unbound1_a3_s0_dummy_cmdtbl),
};

#ifdef CONFIG_SUPPORT_POC_SPI
struct spi_data *s6e3fab_unbound1_a3_s0_spi_data_list[] = {
	&w25q80_spi_data,
	&mx25r4035_spi_data,
};
#endif

struct common_panel_info s6e3fab_unbound1_a3_s0_panel_info = {
	.ldi_name = "s6e3fab",
	.name = "s6e3fab_unbound1_a3_s0",
	.model = "AMB675TG01",
	.vendor = "SDC",
	.id = 0x810000,
	.rev = 0,
	.ddi_props = {
		.gpara = (DDI_SUPPORT_WRITE_GPARA |
				DDI_SUPPORT_READ_GPARA | DDI_SUPPORT_POINT_GPARA),
		.err_fg_recovery = false,
		.support_vrr = true,
	},
	.ddi_ops = {
#ifdef CONFIG_SUPPORT_DDI_FLASH
		.gamma_flash_checksum = do_gamma_flash_checksum,
		.mtp_gamma_check = s6e3fab_mtp_gamma_check,
#endif
	},
#ifdef CONFIG_SUPPORT_DSU
	.mres = {
		.nr_resol = ARRAY_SIZE(s6e3fab_unbound1_default_resol),
		.resol = s6e3fab_unbound1_default_resol,
	},
#endif
#if defined(CONFIG_PANEL_DISPLAY_MODE)
	.common_panel_modes = &s6e3fab_unbound1_display_modes,
#endif
	.vrrtbl = s6e3fab_unbound1_default_vrrtbl,
	.nr_vrrtbl = ARRAY_SIZE(s6e3fab_unbound1_default_vrrtbl),
	.maptbl = unbound1_a3_s0_maptbl,
	.nr_maptbl = ARRAY_SIZE(unbound1_a3_s0_maptbl),
	.seqtbl = unbound1_a3_s0_seqtbl,
	.nr_seqtbl = ARRAY_SIZE(unbound1_a3_s0_seqtbl),
	.rditbl = s6e3fab_rditbl,
	.nr_rditbl = ARRAY_SIZE(s6e3fab_rditbl),
	.restbl = s6e3fab_restbl,
	.nr_restbl = ARRAY_SIZE(s6e3fab_restbl),
	.dumpinfo = s6e3fab_dmptbl,
	.nr_dumpinfo = ARRAY_SIZE(s6e3fab_dmptbl),
#ifdef CONFIG_EXYNOS_DECON_MDNIE_LITE
	.mdnie_tune = &s6e3fab_unbound1_a3_s0_mdnie_tune,
#endif
	.panel_dim_info = {
		[PANEL_BL_SUBDEV_TYPE_DISP] = &s6e3fab_unbound1_a3_s0_panel_dimming_info,
#ifdef CONFIG_SUPPORT_HMD
		[PANEL_BL_SUBDEV_TYPE_HMD] = &s6e3fab_unbound1_a3_s0_panel_hmd_dimming_info,
#endif
#ifdef CONFIG_SUPPORT_AOD_BL
		[PANEL_BL_SUBDEV_TYPE_AOD] = &s6e3fab_unbound1_a3_s0_panel_aod_dimming_info,
#endif
	},
#ifdef CONFIG_EXYNOS_DECON_LCD_COPR
	.copr_data = &s6e3fab_unbound1_a3_s0_copr_data,
#endif
#ifdef CONFIG_EXTEND_LIVE_CLOCK
	.aod_tune = &s6e3fab_unbound1_aod,
#endif
#ifdef CONFIG_SUPPORT_DISPLAY_PROFILER
	.profile_tune = &fab_profiler_tune,
#endif
#ifdef CONFIG_DYNAMIC_MIPI
	.dm_total_band = unbound_dynamic_freq_set,
#endif
#ifdef CONFIG_SUPPORT_MAFPC
	.mafpc_info = &s6e3fab_unbound_mafpc,
#endif

#ifdef CONFIG_SUPPORT_DDI_FLASH
	.poc_data = &s6e3fab_unbound_poc_data,
#endif
#ifdef CONFIG_SUPPORT_POC_SPI
	.spi_data_tbl = s6e3fab_unbound1_a3_s0_spi_data_list,
	.nr_spi_data_tbl = ARRAY_SIZE(s6e3fab_unbound1_a3_s0_spi_data_list),
#endif
};
#endif /* __S6E3FAB_UNBOUND1_A3_S0_PANEL_H__ */
