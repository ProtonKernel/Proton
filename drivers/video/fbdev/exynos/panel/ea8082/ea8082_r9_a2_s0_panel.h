/*
 * linux/drivers/video/fbdev/exynos/panel/ea8082/ea8082_r9_a2_s0_panel.h
 *
 * Header file for EA8082 Dimming Driver
 *
 * Copyright (c) 2016 Samsung Electronics
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#ifndef __EA8082_R9_A2_S0_PANEL_H__
#define __EA8082_R9_A2_S0_PANEL_H__

#include "../panel.h"
#include "../panel_drv.h"
#include "ea8082.h"
#include "ea8082_dimming.h"
#ifdef CONFIG_EXYNOS_DECON_MDNIE_LITE
#include "ea8082_r9_a2_s0_panel_mdnie.h"
#endif
#include "ea8082_r9_a2_s0_panel_dimming.h"
#ifdef CONFIG_SUPPORT_HMD
#include "ea8082_r9_a2_s0_panel_hmd_dimming.h"
#endif
#ifdef CONFIG_SUPPORT_AOD_BL
#include "ea8082_r9_a2_s0_panel_aod_dimming.h"
#endif

#include "ea8082_r9_resol.h"

#ifdef CONFIG_DYNAMIC_MIPI
#include "r9_dm_band.h"
#endif

#undef __pn_name__
#define __pn_name__	r9_a2_s0

#undef __PN_NAME__
#define __PN_NAME__	R9_A2_S0

/* ===================================================================================== */
/* ============================= [EA8082 READ INFO TABLE] ============================= */
/* ===================================================================================== */
/* <READINFO TABLE> IS DEPENDENT ON LDI. IF YOU NEED, DEFINE PANEL's RESOURCE TABLE */


/* ===================================================================================== */
/* ============================= [EA8082 RESOURCE TABLE] ============================== */
/* ===================================================================================== */
/* <RESOURCE TABLE> IS DEPENDENT ON LDI. IF YOU NEED, DEFINE PANEL's RESOURCE TABLE */


/* ===================================================================================== */
/* ============================== [EA8082 MAPPING TABLE] ============================== */
/* ===================================================================================== */

static u8 r9_a2_s0_brt_table[EA8082_TOTAL_STEP][2] = {
	/* Normal 5x51+1 */
	{ 0x00, 0x02 }, { 0x00, 0x03 }, { 0x00, 0x04 }, { 0x00, 0x05 }, { 0x00, 0x07 },
	{ 0x00, 0x08 }, { 0x00, 0x0A }, { 0x00, 0x0C }, { 0x00, 0x0E }, { 0x00, 0x0F },
	{ 0x00, 0x11 }, { 0x00, 0x13 }, { 0x00, 0x16 }, { 0x00, 0x18 }, { 0x00, 0x1A },
	{ 0x00, 0x1C }, { 0x00, 0x1E }, { 0x00, 0x21 }, { 0x00, 0x23 }, { 0x00, 0x26 },
	{ 0x00, 0x28 }, { 0x00, 0x2B }, { 0x00, 0x2D }, { 0x00, 0x30 }, { 0x00, 0x32 },
	{ 0x00, 0x35 }, { 0x00, 0x38 }, { 0x00, 0x3A }, { 0x00, 0x3D }, { 0x00, 0x40 },
	{ 0x00, 0x42 }, { 0x00, 0x45 }, { 0x00, 0x48 }, { 0x00, 0x4B }, { 0x00, 0x4E },
	{ 0x00, 0x51 }, { 0x00, 0x54 }, { 0x00, 0x57 }, { 0x00, 0x5A }, { 0x00, 0x5D },
	{ 0x00, 0x60 }, { 0x00, 0x63 }, { 0x00, 0x66 }, { 0x00, 0x69 }, { 0x00, 0x6C },
	{ 0x00, 0x6F }, { 0x00, 0x72 }, { 0x00, 0x76 }, { 0x00, 0x79 }, { 0x00, 0x7C },
	{ 0x00, 0x7F }, { 0x00, 0x83 }, { 0x00, 0x86 }, { 0x00, 0x89 }, { 0x00, 0x8C },
	{ 0x00, 0x90 }, { 0x00, 0x93 }, { 0x00, 0x97 }, { 0x00, 0x9A }, { 0x00, 0x9D },
	{ 0x00, 0xA1 }, { 0x00, 0xA4 }, { 0x00, 0xA8 }, { 0x00, 0xAB }, { 0x00, 0xAF },
	{ 0x00, 0xB2 }, { 0x00, 0xB6 }, { 0x00, 0xB9 }, { 0x00, 0xBD }, { 0x00, 0xC0 },
	{ 0x00, 0xC4 }, { 0x00, 0xC8 }, { 0x00, 0xCB }, { 0x00, 0xCF }, { 0x00, 0xD3 },
	{ 0x00, 0xD6 }, { 0x00, 0xDA }, { 0x00, 0xDE }, { 0x00, 0xE1 }, { 0x00, 0xE5 },
	{ 0x00, 0xE9 }, { 0x00, 0xED }, { 0x00, 0xF0 }, { 0x00, 0xF4 }, { 0x00, 0xF8 },
	{ 0x00, 0xFC }, { 0x01, 0x00 }, { 0x01, 0x03 }, { 0x01, 0x07 }, { 0x01, 0x0B },
	{ 0x01, 0x0F }, { 0x01, 0x13 }, { 0x01, 0x17 }, { 0x01, 0x1B }, { 0x01, 0x1F },
	{ 0x01, 0x23 }, { 0x01, 0x26 }, { 0x01, 0x2A }, { 0x01, 0x2E }, { 0x01, 0x32 },
	{ 0x01, 0x36 }, { 0x01, 0x3A }, { 0x01, 0x3E }, { 0x01, 0x43 }, { 0x01, 0x47 },
	{ 0x01, 0x4B }, { 0x01, 0x4F }, { 0x01, 0x53 }, { 0x01, 0x57 }, { 0x01, 0x5B },
	{ 0x01, 0x5F }, { 0x01, 0x63 }, { 0x01, 0x67 }, { 0x01, 0x6C }, { 0x01, 0x70 },
	{ 0x01, 0x74 }, { 0x01, 0x78 }, { 0x01, 0x7C }, { 0x01, 0x80 }, { 0x01, 0x85 },
	{ 0x01, 0x89 }, { 0x01, 0x8D }, { 0x01, 0x91 }, { 0x01, 0x96 }, { 0x01, 0x9A },
	{ 0x01, 0x9E }, { 0x01, 0xA3 }, { 0x01, 0xA7 }, { 0x01, 0xAB }, { 0x01, 0xAF },
	{ 0x01, 0xB4 }, { 0x01, 0xB8 }, { 0x01, 0xBD }, { 0x01, 0xC1 }, { 0x01, 0xC5 },
	{ 0x01, 0xC9 }, { 0x01, 0xCE }, { 0x01, 0xD2 }, { 0x01, 0xD6 }, { 0x01, 0xDA },
	{ 0x01, 0xDF }, { 0x01, 0xE3 }, { 0x01, 0xE7 }, { 0x01, 0xEC }, { 0x01, 0xF0 },
	{ 0x01, 0xF4 }, { 0x01, 0xF9 }, { 0x01, 0xFD }, { 0x02, 0x01 }, { 0x02, 0x06 },
	{ 0x02, 0x0A }, { 0x02, 0x0E }, { 0x02, 0x13 }, { 0x02, 0x17 }, { 0x02, 0x1C },
	{ 0x02, 0x20 }, { 0x02, 0x24 }, { 0x02, 0x29 }, { 0x02, 0x2D }, { 0x02, 0x32 },
	{ 0x02, 0x36 }, { 0x02, 0x3B }, { 0x02, 0x3F }, { 0x02, 0x44 }, { 0x02, 0x48 },
	{ 0x02, 0x4D }, { 0x02, 0x51 }, { 0x02, 0x56 }, { 0x02, 0x5A }, { 0x02, 0x5F },
	{ 0x02, 0x63 }, { 0x02, 0x68 }, { 0x02, 0x6C }, { 0x02, 0x71 }, { 0x02, 0x75 },
	{ 0x02, 0x7A }, { 0x02, 0x7F }, { 0x02, 0x83 }, { 0x02, 0x88 }, { 0x02, 0x8C },
	{ 0x02, 0x91 }, { 0x02, 0x96 }, { 0x02, 0x9A }, { 0x02, 0x9F }, { 0x02, 0xA3 },
	{ 0x02, 0xA8 }, { 0x02, 0xAD }, { 0x02, 0xB1 }, { 0x02, 0xB6 }, { 0x02, 0xBB },
	{ 0x02, 0xBF }, { 0x02, 0xC4 }, { 0x02, 0xC9 }, { 0x02, 0xCE }, { 0x02, 0xD2 },
	{ 0x02, 0xD7 }, { 0x02, 0xDC }, { 0x02, 0xE0 }, { 0x02, 0xE5 }, { 0x02, 0xEA },
	{ 0x02, 0xEF }, { 0x02, 0xF4 }, { 0x02, 0xF8 }, { 0x02, 0xFD }, { 0x03, 0x02 },
	{ 0x03, 0x07 }, { 0x03, 0x0B }, { 0x03, 0x10 }, { 0x03, 0x15 }, { 0x03, 0x1A },
	{ 0x03, 0x1F }, { 0x03, 0x24 }, { 0x03, 0x28 }, { 0x03, 0x2D }, { 0x03, 0x32 },
	{ 0x03, 0x37 }, { 0x03, 0x3C }, { 0x03, 0x41 }, { 0x03, 0x46 }, { 0x03, 0x4B },
	{ 0x03, 0x4F }, { 0x03, 0x54 }, { 0x03, 0x59 }, { 0x03, 0x5E }, { 0x03, 0x63 },
	{ 0x03, 0x68 }, { 0x03, 0x6D }, { 0x03, 0x72 }, { 0x03, 0x77 }, { 0x03, 0x7C },
	{ 0x03, 0x81 }, { 0x03, 0x86 }, { 0x03, 0x8B }, { 0x03, 0x90 }, { 0x03, 0x95 },
	{ 0x03, 0x9A }, { 0x03, 0x9F }, { 0x03, 0xA4 }, { 0x03, 0xA9 }, { 0x03, 0xAE },
	{ 0x03, 0xB3 }, { 0x03, 0xB8 }, { 0x03, 0xBD }, { 0x03, 0xC2 }, { 0x03, 0xC7 },
	{ 0x03, 0xCC }, { 0x03, 0xD1 }, { 0x03, 0xD6 }, { 0x03, 0xDB }, { 0x03, 0xE0 },
	{ 0x03, 0xE5 }, { 0x03, 0xEB }, { 0x03, 0xF0 }, { 0x03, 0xF5 }, { 0x03, 0xFA },
	{ 0x03, 0xFF },


	/* HBM 5x46+1 */
	{ 0x00, 0x28 }, { 0x00, 0x28 }, { 0x00, 0x28 }, { 0x00, 0x28 }, { 0x00, 0x28 },
	{ 0x00, 0x28 }, { 0x00, 0x28 }, { 0x00, 0x28 }, { 0x00, 0x28 }, { 0x00, 0x28 },
	{ 0x00, 0x28 }, { 0x00, 0x28 }, { 0x00, 0x28 }, { 0x00, 0x28 }, { 0x00, 0x28 },
	{ 0x00, 0x28 }, { 0x00, 0x28 }, { 0x00, 0x2B }, { 0x00, 0x2D }, { 0x00, 0x30 },
	{ 0x00, 0x32 }, { 0x00, 0x35 }, { 0x00, 0x37 }, { 0x00, 0x3A }, { 0x00, 0x3C },
	{ 0x00, 0x3F }, { 0x00, 0x41 }, { 0x00, 0x44 }, { 0x00, 0x46 }, { 0x00, 0x49 },
	{ 0x00, 0x4B }, { 0x00, 0x4E }, { 0x00, 0x50 }, { 0x00, 0x53 }, { 0x00, 0x55 },
	{ 0x00, 0x58 }, { 0x00, 0x5A }, { 0x00, 0x5D }, { 0x00, 0x5F }, { 0x00, 0x62 },
	{ 0x00, 0x64 }, { 0x00, 0x67 }, { 0x00, 0x69 }, { 0x00, 0x6C }, { 0x00, 0x6E },
	{ 0x00, 0x71 }, { 0x00, 0x73 }, { 0x00, 0x76 }, { 0x00, 0x78 }, { 0x00, 0x7B },
	{ 0x00, 0x7D }, { 0x00, 0x80 }, { 0x00, 0x82 }, { 0x00, 0x85 }, { 0x00, 0x87 },
	{ 0x00, 0x8A }, { 0x00, 0x8C }, { 0x00, 0x8F }, { 0x00, 0x91 }, { 0x00, 0x94 },
	{ 0x00, 0x96 }, { 0x00, 0x99 }, { 0x00, 0x9B }, { 0x00, 0x9E }, { 0x00, 0xA0 },
	{ 0x00, 0xA3 }, { 0x00, 0xA5 }, { 0x00, 0xA8 }, { 0x00, 0xAA }, { 0x00, 0xAD },
	{ 0x00, 0xAF }, { 0x00, 0xB2 }, { 0x00, 0xB4 }, { 0x00, 0xB7 }, { 0x00, 0xB9 },
	{ 0x00, 0xBC }, { 0x00, 0xBE }, { 0x00, 0xC1 }, { 0x00, 0xC3 }, { 0x00, 0xC6 },
	{ 0x00, 0xC8 }, { 0x00, 0xCB }, { 0x00, 0xCD }, { 0x00, 0xD0 }, { 0x00, 0xD2 },
	{ 0x00, 0xD5 }, { 0x00, 0xD7 }, { 0x00, 0xDA }, { 0x00, 0xDC }, { 0x00, 0xDF },
	{ 0x00, 0xE1 }, { 0x00, 0xE4 }, { 0x00, 0xE6 }, { 0x00, 0xE9 }, { 0x00, 0xEB },
	{ 0x00, 0xEE }, { 0x00, 0xF0 }, { 0x00, 0xF3 }, { 0x00, 0xF5 }, { 0x00, 0xF8 },
	{ 0x00, 0xFA }, { 0x00, 0xFD }, { 0x00, 0xFF }, { 0x01, 0x02 }, { 0x01, 0x04 },
	{ 0x01, 0x07 }, { 0x01, 0x09 }, { 0x01, 0x0C }, { 0x01, 0x0E }, { 0x01, 0x11 },
	{ 0x01, 0x13 }, { 0x01, 0x16 }, { 0x01, 0x18 }, { 0x01, 0x1B }, { 0x01, 0x1D },
	{ 0x01, 0x20 }, { 0x01, 0x22 }, { 0x01, 0x25 }, { 0x01, 0x28 }, { 0x01, 0x2A },
	{ 0x01, 0x2D }, { 0x01, 0x2F }, { 0x01, 0x32 }, { 0x01, 0x34 }, { 0x01, 0x37 },
	{ 0x01, 0x39 }, { 0x01, 0x3C }, { 0x01, 0x3E }, { 0x01, 0x41 }, { 0x01, 0x43 },
	{ 0x01, 0x46 }, { 0x01, 0x48 }, { 0x01, 0x4B }, { 0x01, 0x4D }, { 0x01, 0x50 },
	{ 0x01, 0x52 }, { 0x01, 0x55 }, { 0x01, 0x57 }, { 0x01, 0x5A }, { 0x01, 0x5C },
	{ 0x01, 0x5F }, { 0x01, 0x61 }, { 0x01, 0x64 }, { 0x01, 0x66 }, { 0x01, 0x69 },
	{ 0x01, 0x6B }, { 0x01, 0x6E }, { 0x01, 0x70 }, { 0x01, 0x73 }, { 0x01, 0x75 },
	{ 0x01, 0x78 }, { 0x01, 0x7A }, { 0x01, 0x7D }, { 0x01, 0x7F }, { 0x01, 0x82 },
	{ 0x01, 0x84 }, { 0x01, 0x87 }, { 0x01, 0x89 }, { 0x01, 0x8C }, { 0x01, 0x8E },
	{ 0x01, 0x91 }, { 0x01, 0x93 }, { 0x01, 0x96 }, { 0x01, 0x98 }, { 0x01, 0x9B },
	{ 0x01, 0x9D }, { 0x01, 0xA0 }, { 0x01, 0xA2 }, { 0x01, 0xA5 }, { 0x01, 0xA7 },
	{ 0x01, 0xAA }, { 0x01, 0xAC }, { 0x01, 0xAF }, { 0x01, 0xB1 }, { 0x01, 0xB4 },
	{ 0x01, 0xB6 }, { 0x01, 0xB9 }, { 0x01, 0xBB }, { 0x01, 0xBE }, { 0x01, 0xC0 },
	{ 0x01, 0xC3 }, { 0x01, 0xC5 }, { 0x01, 0xC8 }, { 0x01, 0xCA }, { 0x01, 0xCD },
	{ 0x01, 0xCF }, { 0x01, 0xD2 }, { 0x01, 0xD4 }, { 0x01, 0xD7 }, { 0x01, 0xD9 },
	{ 0x01, 0xDC }, { 0x01, 0xDE }, { 0x01, 0xE1 }, { 0x01, 0xE3 }, { 0x01, 0xE6 },
	{ 0x01, 0xE8 }, { 0x01, 0xEB }, { 0x01, 0xED }, { 0x01, 0xF0 }, { 0x01, 0xF2 },
	{ 0x01, 0xF5 }, { 0x01, 0xF7 }, { 0x01, 0xFA }, { 0x01, 0xFC }, { 0x01, 0xFF },
	{ 0x02, 0x01 }, { 0x02, 0x04 }, { 0x02, 0x06 }, { 0x02, 0x09 }, { 0x02, 0x0B },
	{ 0x02, 0x0E }, { 0x02, 0x10 }, { 0x02, 0x13 }, { 0x02, 0x15 }, { 0x02, 0x18 },
	{ 0x02, 0x1A }, { 0x02, 0x1D }, { 0x02, 0x1F }, { 0x02, 0x22 }, { 0x02, 0x24 },
	{ 0x02, 0x27 }, { 0x02, 0x29 }, { 0x02, 0x2C }, { 0x02, 0x2E }, { 0x02, 0x31 },
	{ 0x02, 0x33 }, { 0x02, 0x36 }, { 0x02, 0x38 }, { 0x02, 0x3B }, { 0x02, 0x3D },
	{ 0x02, 0x40 },
};

#ifdef CONFIG_SUPPORT_HMD
static u8 r9_a2_s0_hmd_brt_table[EA8082_HMD_TOTAL_STEP][2] = {
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

static u8 r9_a2_s0_hbm_and_transition_table[MAX_PANEL_HBM][SMOOTH_TRANS_MAX][1] = {
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

static u8 r9_a2_s0_acl_opr_table[MAX_PANEL_HBM][MAX_EA8082_ACL_OPR][1] = {
	[PANEL_HBM_OFF] = {
		[EA8082_ACL_OPR_0] = { 0x00 }, /* adaptive_control 0, 0% */
		[EA8082_ACL_OPR_1] = { 0x01 }, /* adaptive_control 1, 8% */
		[EA8082_ACL_OPR_2] = { 0x01 }, /* adaptive_control 2, 8% */
	},
	[PANEL_HBM_ON] = {
		[EA8082_ACL_OPR_0 ... EA8082_ACL_OPR_2] = { 0x01 }, /* adaptive_control 0, 8% */
	},
};

static u8 r9_a2_s0_lpm_on_table[4][1] = {
	/* LPM 2NIT */
	{ 0x27 },
	/* LPM 10NIT */
	{ 0x26 },
	/* LPM 30NIT */
	{ 0x25 },
	/* LPM 60NIT */
	{ 0x24 },
};

static u8 r9_a2_s0_dia_onoff_table[][1] = {
	{ 0x00 }, /* dia off */
	{ 0x02 }, /* dia on */
};

static u8 r9_a2_s0_fps_table[][2] = {
	[EA8082_VRR_120HS] = { 0x08, 0x00 },
	[EA8082_VRR_60HS] = { 0x00, 0x00 },
};

#if defined(CONFIG_SUPPORT_FAST_DISCHARGE)
static u8 r9_a2_s0_fast_discharge_table[][1] = {
	{ 0x00 },	//fd 0ff
	{ 0x40 },	//fd on
};
#endif

#ifdef CONFIG_SUPPORT_XTALK_MODE
static u8 r9_a2_s0_vgh_table[][1] = {
	{ 0x13 }, /* VGH (x-talk off)*/
	{ 0x0D }, /* VGH (x-talk on) */
};
#endif

static char R9_A2_S0_FFC_DEFAULT[] = {
	0xDE,
	0x11, 0x50, 0xA4, 0xF5, 0xBB, 0x54, 0x29,
	0xC8, 0xA4, 0xF5, 0xBB, 0x54, 0x29, 0xC8,
	0x00, 0x15, 0x7C
};

static struct maptbl r9_a2_s0_maptbl[MAX_MAPTBL] = {
	[GAMMA_MODE2_MAPTBL] = DEFINE_2D_MAPTBL(r9_a2_s0_brt_table, init_gamma_mode2_brt_table, getidx_gamma_mode2_brt_table, copy_common_maptbl),
#ifdef CONFIG_SUPPORT_HMD
	[HMD_GAMMA_MAPTBL] = DEFINE_2D_MAPTBL(r9_a2_s0_hmd_brt_table, init_gamma_mode2_hmd_brt_table, getidx_gamma_mode2_hmd_brt_table, copy_common_maptbl),
#endif
	[TSET_MAPTBL] = DEFINE_0D_MAPTBL(r9_a2_s0_tset_table, init_common_table, NULL, copy_tset_maptbl),
	[HBM_AND_TRANSITION_MAPTBL] = DEFINE_3D_MAPTBL(r9_a2_s0_hbm_and_transition_table, init_common_table, getidx_hbm_and_transition_table, copy_common_maptbl),
	[ACL_OPR_MAPTBL] = DEFINE_3D_MAPTBL(r9_a2_s0_acl_opr_table, init_common_table, getidx_acl_opr_table, copy_common_maptbl),
	[DIA_ONOFF_MAPTBL] = DEFINE_2D_MAPTBL(r9_a2_s0_dia_onoff_table, init_common_table, getidx_dia_onoff_table, copy_common_maptbl),
	[LPM_ON_MAPTBL] = DEFINE_2D_MAPTBL(r9_a2_s0_lpm_on_table, init_lpm_brt_table, getidx_lpm_brt_table, copy_common_maptbl),
	[FPS_MAPTBL] = DEFINE_2D_MAPTBL(r9_a2_s0_fps_table, init_common_table, getidx_vrr_table, copy_common_maptbl),
#ifdef CONFIG_SUPPORT_GRAYSPOT_TEST
	[GRAYSPOT_CAL_MAPTBL] = DEFINE_0D_MAPTBL(r9_a2_s0_grayspot_cal_table, init_common_table, NULL, copy_grayspot_cal_maptbl),
#endif
#ifdef CONFIG_SUPPORT_XTALK_MODE
	[VGH_MAPTBL] = DEFINE_2D_MAPTBL(r9_a2_s0_vgh_table, init_common_table, getidx_vgh_table, copy_common_maptbl),
#endif
#ifdef CONFIG_DYNAMIC_MIPI
	[DM_SET_FFC_MAPTBL] = DEFINE_1D_MAPTBL(R9_A2_S0_FFC_DEFAULT, ARRAY_SIZE(R9_A2_S0_FFC_DEFAULT), init_common_table, NULL, dynamic_mipi_set_ffc),
#endif
#if defined(CONFIG_SUPPORT_FAST_DISCHARGE)
	[FAST_DISCHARGE_MAPTBL] = DEFINE_2D_MAPTBL(r9_a2_s0_fast_discharge_table, init_common_table, getidx_fast_discharge_table, copy_common_maptbl),
#endif

};

/* ===================================================================================== */
/* ============================== [EA8082 COMMAND TABLE] ============================== */
/* ===================================================================================== */
static u8 R9_A2_S0_KEY1_ENABLE[] = { 0xF0, 0x5A, 0x5A };
static u8 R9_A2_S0_KEY2_ENABLE[] = { 0xFC, 0x5A, 0x5A };

static u8 R9_A2_S0_KEY1_DISABLE[] = { 0xF0, 0xA5, 0xA5 };
static u8 R9_A2_S0_KEY2_DISABLE[] = { 0xFC, 0xA5, 0xA5 };


static u8 R9_A2_S0_SLEEP_OUT[] = { 0x11 };
static u8 R9_A2_S0_SLEEP_IN[] = { 0x10 };
static u8 R9_A2_S0_DISPLAY_OFF[] = { 0x28 };
static u8 R9_A2_S0_DISPLAY_ON[] = { 0x29 };

static u8 R9_A2_S0_DSC[] = { 0x01 };
static u8 R9_A2_S0_PPS[] = {
	// FHD : 1080x2340
	0x11, 0x00, 0x00, 0x89, 0x30, 0x80, 0x09, 0x24,
	0x04, 0x38, 0x00, 0x1E, 0x02, 0x1C, 0x02, 0x1C,
	0x02, 0x00, 0x02, 0x0E, 0x00, 0x20, 0x02, 0xE3,
	0x00, 0x07, 0x00, 0x0C, 0x03, 0x50, 0x03, 0x64,
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
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00
};

static u8 R9_A2_S0_TE_ON[] = { 0x35, 0x00 };

static u8 R9_A2_S0_TSP_HSYNC1[] = {
	0xD9,
	0x01, 0x98, 0x0F, 0x00, 0x00, 0x00, 0x0C
};

static u8 R9_A2_S0_TSP_HSYNC2[] = {
	0xF4,
	0x63
};

static u8 R9_A2_S0_ETC_SET1[] = {
	0xD3,
	0x83, 0x00, 0x00, 0x03, 0x00, 0x00, 0x83,
	0x00, 0x00, 0x03, 0x00, 0x00
};

static u8 R9_A2_S0_ETC_SET2[] = {
	0xDD,
	0x30
};

static u8 R9_A2_S0_ETC_SET3[] = {
	0xD1,
	0x2D
};

static u8 R9_A2_S0_ASWIRE_OFF[] = {
	0xDF,
	0x00, 0x00
};

static u8 R9_A2_S0_TIMING_OFFSET1[] = {
	0xC1,
	0x01
};

static u8 R9_A2_S0_TIMING_OFFSET2[] = {
	0xD1,
	0x01
};

static u8 R9_A2_S0_TE_SHIFT[] = {
	0xD9,
	0x01, 0x91, 0xC0, 0x0C				// 20h
};

static u8 R9_A2_S0_TSET[] = {
	0xDF,
	0x19,	/* temperature 25 */
};

static u8 R9_A2_S0_FAST_DISCHARGE[] = {
	0xB1, 0x40
};

static u8 R9_A2_S0_ERR_FG_1[] = {
	0xD9, 0x01, 0x00, 0x70, 0x30
};

static u8 R9_A2_S0_ERR_FG_2[] = {
	0xD9, 0x20
};

static u8 R9_A2_S0_ACL_DEFAULT[] = {
	0xBE,
	0x55, 0x00, 0xB0, 0x51, 0x66, 0x98, 0x15, 0x55,
	0x55, 0x55, 0x08, 0xF1, 0xC6, 0x48, 0x40, 0x00,
	0x20, 0x10, 0x09, 0x9A
};

static u8 R9_A2_S0_ISC[] = {
	0xF6, 0x40
};

static u8 R9_A2_S0_LPM_ON[] = {
	0x53, 0x24
};

static u8 R9_A2_S0_LPM_OFF[] = {
	0x53, 0x20
};

static u8 R9_A2_S0_LPM_EXIT_SWIRE_NO_PULSE[] = {
	0xB1, 0x00
};

#ifdef CONFIG_SUPPORT_DDI_CMDLOG
static u8 R9_A2_S0_CMDLOG_ENABLE[] = { 0xF7, 0x80 };
static u8 R9_A2_S0_CMDLOG_DISABLE[] = { 0xF7, 0x00 };
static u8 R9_A2_S0_GAMMA_UPDATE_ENABLE[] = { 0xF7, 0x8F };
#else
static u8 R9_A2_S0_GAMMA_UPDATE_ENABLE[] = { 0xF7, 0x0F };
#endif

static u8 R9_A2_S0_MCD_ON_01[] = { 0xD6, 0x0B, 0x00, 0x02 };
static u8 R9_A2_S0_MCD_ON_02[] = { 0xD6, 0x00, 0x71 };
static u8 R9_A2_S0_MCD_ON_03[] = { 0xD4, 0x04 };
static u8 R9_A2_S0_MCD_ON_04[] = { 0xD4, 0x86, 0x00 };
static u8 R9_A2_S0_MCD_ON_05[] = { 0xD4, 0x84, 0x3C, 0x00 };

static u8 R9_A2_S0_MCD_OFF_01[] = { 0xD4, 0x14 };
static u8 R9_A2_S0_MCD_OFF_02[] = { 0xD4, 0x86, 0x8E };
static u8 R9_A2_S0_MCD_OFF_03[] = { 0xD4, 0x84, 0x3C, 0x8E };
static u8 R9_A2_S0_MCD_OFF_04[] = { 0xD6, 0x07, 0x00, 0x06 };
static u8 R9_A2_S0_MCD_OFF_05[] = { 0xD6, 0x00, 0x00 };

#ifdef CONFIG_SUPPORT_GRAYSPOT_TEST
#define GRAYSPOT_START_SWIRE		0x35
#define GRAYSPOT_CAL_OFFSET			0x00

static u8 R9_A2_S0_GRAYSPOT_ON_01[] = {
	0xD4, 0x04
};
static u8 R9_A2_S0_GRAYSPOT_ON_02[] = {
	0xD4, 0x86, 0x00
};
static u8 R9_A2_S0_GRAYSPOT_ON_03[] = {
	0xD4, 0x84, 0x3C, 0x00
};
static u8 R9_A2_S0_GRAYSPOT_ELVSS_SET[] = {
	0xB4, 0x35, 0x00, 0x00, 0x00
};

static u8 R9_A2_S0_GRAYSPOT_OFF_01[] = {
	0xD4, 0x14
};
static u8 R9_A2_S0_GRAYSPOT_OFF_02[] = {
	0xD4, 0x86, 0x8E
};
static u8 R9_A2_S0_GRAYSPOT_OFF_03[] = {
	0xD4, 0x84, 0x3C, 0x8E
};
#endif

/* Micro Crack Test Sequence */
#ifdef CONFIG_SUPPORT_MST
static u8 R9_A2_S0_MST_ON_01[] = {
	0xD4,
	0x04
};
static u8 R9_A2_S0_MST_ON_02[] = {
	0xD4,
	0x87
};
static u8 R9_A2_S0_MST_ON_03[] = {
	0xE7,
	0x8A, 0x50, 0x80, 0xFF, 0x00, 0xFF, 0x00, 0xFF, 0x00
};

static u8 R9_A2_S0_MST_OFF_01[] = {
	0xD4,
	0x14
};
static u8 R9_A2_S0_MST_OFF_02[] = {
	0xD4,
	0x8E
};
static u8 R9_A2_S0_MST_OFF_03[] = {
	0xBF,
	0x00, 0x50, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00
};
#endif

#ifdef CONFIG_SUPPORT_GRAM_CHECKSUM
static u8 R9_A2_S0_SW_RESET[] = { 0x01 };
static u8 R9_A2_S0_GCT_DSC[] = { 0x9D, 0x01 };
static u8 R9_A2_S0_GCT_PPS[] = { 0x9E,
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
static u8 R9_A2_S0_FUSING_UPDATE_ON[] = { 0xC5, 0x05 };
static u8 R9_A2_S0_FUSING_UPDATE_OFF[] = { 0xC5, 0x04 };
static u8 R9_A2_S0_VDDM_INIT[] = { 0xFE, 0x14 };
static u8 R9_A2_S0_VDDM_ORIG[] = { 0xF4, 0x00 };
static u8 R9_A2_S0_VDDM_VOLT[] = { 0xF4, 0x00 };
static u8 R9_A2_S0_GRAM_CHKSUM_START[] = { 0x2C, 0x00 };
static u8 R9_A2_S0_GRAM_IMG_PATTERN_ON[] = { 0xBE, 0x00 };
static u8 R9_A2_S0_GRAM_INV_IMG_PATTERN_ON[] = { 0xBE, 0x00 };
static u8 R9_A2_S0_GRAM_IMG_PATTERN_OFF[] = { 0xBE, 0x00 };
#endif
#ifdef CONFIG_SUPPORT_XTALK_MODE
static u8 R9_A2_S0_XTALK_MODE[] = { 0xD3, 0x0D };
#endif
#ifdef CONFIG_SUPPORT_CCD_TEST
static u8 R9_A2_S0_CCD_ENABLE1[] = { 0xF2, 0x80 };
static u8 R9_A2_S0_CCD_ENABLE2[] = { 0xD9, 0x54, 0x0A };

static u8 R9_A2_S0_CCD_DISABLE[] = { 0xF2, 0x00 };
#endif

static DEFINE_STATIC_PACKET(r9_a2_s0_level1_key_enable, DSI_PKT_TYPE_WR, R9_A2_S0_KEY1_ENABLE, 0);
static DEFINE_STATIC_PACKET(r9_a2_s0_level2_key_enable, DSI_PKT_TYPE_WR, R9_A2_S0_KEY2_ENABLE, 0);

static DEFINE_STATIC_PACKET(r9_a2_s0_level1_key_disable, DSI_PKT_TYPE_WR, R9_A2_S0_KEY1_DISABLE, 0);
static DEFINE_STATIC_PACKET(r9_a2_s0_level2_key_disable, DSI_PKT_TYPE_WR, R9_A2_S0_KEY2_DISABLE, 0);

static DEFINE_STATIC_PACKET(r9_a2_s0_sleep_out, DSI_PKT_TYPE_WR, R9_A2_S0_SLEEP_OUT, 0);
static DEFINE_STATIC_PACKET(r9_a2_s0_sleep_in, DSI_PKT_TYPE_WR, R9_A2_S0_SLEEP_IN, 0);
static DEFINE_STATIC_PACKET(r9_a2_s0_display_on, DSI_PKT_TYPE_WR, R9_A2_S0_DISPLAY_ON, 0);
static DEFINE_STATIC_PACKET(r9_a2_s0_display_off, DSI_PKT_TYPE_WR, R9_A2_S0_DISPLAY_OFF, 0);

static DEFINE_STATIC_PACKET(r9_a2_s0_te_on, DSI_PKT_TYPE_WR, R9_A2_S0_TE_ON, 0);

static DEFINE_STATIC_PACKET(r9_a2_s0_tsp_hsync1, DSI_PKT_TYPE_WR, R9_A2_S0_TSP_HSYNC1, 0x21);
static DEFINE_STATIC_PACKET(r9_a2_s0_tsp_hsync2, DSI_PKT_TYPE_WR, R9_A2_S0_TSP_HSYNC2, 0x04);

static DEFINE_STATIC_PACKET(r9_a2_s0_etc_set1, DSI_PKT_TYPE_WR, R9_A2_S0_ETC_SET1, 0x33);
static DEFINE_STATIC_PACKET(r9_a2_s0_etc_set2, DSI_PKT_TYPE_WR, R9_A2_S0_ETC_SET2, 0x4D);
static DEFINE_STATIC_PACKET(r9_a2_s0_etc_set3, DSI_PKT_TYPE_WR, R9_A2_S0_ETC_SET3, 0x15);

static DEFINE_STATIC_PACKET(r9_a2_s0_aswire_off, DSI_PKT_TYPE_WR, R9_A2_S0_ASWIRE_OFF, 0x09);

static DEFINE_STATIC_PACKET(r9_a2_s0_timing_offset1, DSI_PKT_TYPE_WR, R9_A2_S0_TIMING_OFFSET1, 0xC9);
static DEFINE_STATIC_PACKET(r9_a2_s0_timing_offset2, DSI_PKT_TYPE_WR, R9_A2_S0_TIMING_OFFSET2, 0x5C);

static DEFINE_STATIC_PACKET(r9_a2_s0_te_shift, DSI_PKT_TYPE_WR, R9_A2_S0_TE_SHIFT, 0x2A);

static DEFINE_STATIC_PACKET(r9_a2_s0_dsc, DSI_PKT_TYPE_COMP, R9_A2_S0_DSC, 0);
static DEFINE_STATIC_PACKET(r9_a2_s0_pps, DSI_PKT_TYPE_PPS, R9_A2_S0_PPS, 0);

static DEFINE_COND(r9_a2_s0_cond_is_id_gte_03, is_id_gte_03);
static DEFINE_COND(r9_a2_s0_cond_is_first_set_bl, is_first_set_bl);
static DEFINE_COND(r9_a2_s0_cond_is_wait_vsync_needed, is_wait_vsync_needed);

static DEFINE_PKTUI(r9_a2_s0_lpm_nit, &r9_a2_s0_maptbl[LPM_NIT_MAPTBL], 1);

static DEFINE_COND(r9_a2_s0_cond_is_panel_state_not_lpm, is_panel_state_not_lpm);

static DEFINE_PKTUI(r9_a2_s0_lpm_on, &r9_a2_s0_maptbl[LPM_ON_MAPTBL], 1);
static DEFINE_VARIABLE_PACKET(r9_a2_s0_lpm_on, DSI_PKT_TYPE_WR, R9_A2_S0_LPM_ON, 0);
static DEFINE_STATIC_PACKET(r9_a2_s0_lpm_off, DSI_PKT_TYPE_WR, R9_A2_S0_LPM_OFF, 0);

static u8 R9_A2_S0_DIA_ONOFF[] = { 0xC1, 0x00 };
static DEFINE_PKTUI(r9_a2_s0_dia_onoff, &r9_a2_s0_maptbl[DIA_ONOFF_MAPTBL], 1);
static DEFINE_VARIABLE_PACKET(r9_a2_s0_dia_onoff, DSI_PKT_TYPE_WR, R9_A2_S0_DIA_ONOFF, 0x0D);

static DEFINE_PKTUI(r9_a2_s0_tset, &r9_a2_s0_maptbl[TSET_MAPTBL], 1);
static DEFINE_VARIABLE_PACKET(r9_a2_s0_tset, DSI_PKT_TYPE_WR, R9_A2_S0_TSET, 0x99);

static DEFINE_STATIC_PACKET(r9_a2_s0_err_fg_1, DSI_PKT_TYPE_WR, R9_A2_S0_ERR_FG_1, 0x06);
static DEFINE_STATIC_PACKET(r9_a2_s0_err_fg_2, DSI_PKT_TYPE_WR, R9_A2_S0_ERR_FG_2, 0x0F);
static DEFINE_STATIC_PACKET(r9_a2_s0_isc, DSI_PKT_TYPE_WR, R9_A2_S0_ISC, 0x07);
static DEFINE_STATIC_PACKET(r9_a2_s0_acl_default, DSI_PKT_TYPE_WR, R9_A2_S0_ACL_DEFAULT, 0x86);

#if defined(CONFIG_SUPPORT_FAST_DISCHARGE)
static DEFINE_PKTUI(r9_a2_s0_fast_discharge, &r9_a2_s0_maptbl[FAST_DISCHARGE_MAPTBL], 1);
static DEFINE_VARIABLE_PACKET(r9_a2_s0_fast_discharge, DSI_PKT_TYPE_WR, R9_A2_S0_FAST_DISCHARGE, 0x0C);
#endif

static DEFINE_STATIC_PACKET(r9_a2_s0_mcd_on_01, DSI_PKT_TYPE_WR, R9_A2_S0_MCD_ON_01, 0x01);
static DEFINE_STATIC_PACKET(r9_a2_s0_mcd_on_02, DSI_PKT_TYPE_WR, R9_A2_S0_MCD_ON_02, 0x72);
static DEFINE_STATIC_PACKET(r9_a2_s0_mcd_on_03, DSI_PKT_TYPE_WR, R9_A2_S0_MCD_ON_03, 0x17);
static DEFINE_STATIC_PACKET(r9_a2_s0_mcd_on_04, DSI_PKT_TYPE_WR, R9_A2_S0_MCD_ON_04, 0x28);
static DEFINE_STATIC_PACKET(r9_a2_s0_mcd_on_05, DSI_PKT_TYPE_WR, R9_A2_S0_MCD_ON_05, 0x41);

static DEFINE_STATIC_PACKET(r9_a2_s0_mcd_off_01, DSI_PKT_TYPE_WR, R9_A2_S0_MCD_OFF_01, 0x17);
static DEFINE_STATIC_PACKET(r9_a2_s0_mcd_off_02, DSI_PKT_TYPE_WR, R9_A2_S0_MCD_OFF_02, 0x28);
static DEFINE_STATIC_PACKET(r9_a2_s0_mcd_off_03, DSI_PKT_TYPE_WR, R9_A2_S0_MCD_OFF_03, 0x41);
static DEFINE_STATIC_PACKET(r9_a2_s0_mcd_off_04, DSI_PKT_TYPE_WR, R9_A2_S0_MCD_OFF_04, 0x01);
static DEFINE_STATIC_PACKET(r9_a2_s0_mcd_off_05, DSI_PKT_TYPE_WR, R9_A2_S0_MCD_OFF_05, 0x72);

#ifdef CONFIG_SUPPORT_GRAYSPOT_TEST
static DEFINE_STATIC_PACKET(r9_a2_s0_grayspot_on_01, DSI_PKT_TYPE_WR, R9_A2_S0_GRAYSPOT_ON_01, 0x17);
static DEFINE_STATIC_PACKET(r9_a2_s0_grayspot_on_02, DSI_PKT_TYPE_WR, R9_A2_S0_GRAYSPOT_ON_02, 0x28);
static DEFINE_STATIC_PACKET(r9_a2_s0_grayspot_on_03, DSI_PKT_TYPE_WR, R9_A2_S0_GRAYSPOT_ON_03, 0x41);

static DEFINE_STATIC_PACKET(r9_a2_s0_grayspot_off_01, DSI_PKT_TYPE_WR, R9_A2_S0_GRAYSPOT_OFF_01, 0x17);
static DEFINE_STATIC_PACKET(r9_a2_s0_grayspot_off_02, DSI_PKT_TYPE_WR, R9_A2_S0_GRAYSPOT_OFF_02, 0x28);
static DEFINE_STATIC_PACKET(r9_a2_s0_grayspot_off_03, DSI_PKT_TYPE_WR, R9_A2_S0_GRAYSPOT_OFF_02, 0x41);

static DEFINE_PKTUI(r9_a2_s0_grayspot_elvss_set, &r9_a2_s0_maptbl[GRAYSPOT_CAL_MAPTBL], 1);
static DEFINE_VARIABLE_PACKET(r9_a2_s0_grayspot_elvss_set, DSI_PKT_TYPE_WR, R9_A2_S0_GRAYSPOT_ELVSS_SET, 0x11);

#endif

#ifdef CONFIG_SUPPORT_MST
static DEFINE_STATIC_PACKET(r9_a2_s0_mst_on_01, DSI_PKT_TYPE_WR, R9_A2_S0_MST_ON_01, 0x17);
static DEFINE_STATIC_PACKET(r9_a2_s0_mst_on_02, DSI_PKT_TYPE_WR, R9_A2_S0_MST_ON_02, 0x29);
static DEFINE_STATIC_PACKET(r9_a2_s0_mst_on_03, DSI_PKT_TYPE_WR, R9_A2_S0_MST_ON_03, 0x0A);

static DEFINE_STATIC_PACKET(r9_a2_s0_mst_off_01, DSI_PKT_TYPE_WR, R9_A2_S0_MST_OFF_01, 0x17);
static DEFINE_STATIC_PACKET(r9_a2_s0_mst_off_02, DSI_PKT_TYPE_WR, R9_A2_S0_MST_OFF_02, 0x29);
static DEFINE_STATIC_PACKET(r9_a2_s0_mst_off_03, DSI_PKT_TYPE_WR, R9_A2_S0_MST_OFF_03, 0x0A);
#endif

#ifdef CONFIG_SUPPORT_XTALK_MODE
static DEFINE_PKTUI(r9_a2_s0_xtalk_mode, &r9_a2_s0_maptbl[VGH_MAPTBL], 1);
static DEFINE_VARIABLE_PACKET(r9_a2_s0_xtalk_mode, DSI_PKT_TYPE_WR, R9_A2_S0_XTALK_MODE, 0x0C);
#endif
#ifdef CONFIG_SUPPORT_CCD_TEST
static DEFINE_STATIC_PACKET(r9_a2_s0_ccd_test_enable1, DSI_PKT_TYPE_WR, R9_A2_S0_CCD_ENABLE1, 0x4A);
static DEFINE_STATIC_PACKET(r9_a2_s0_ccd_test_enable2, DSI_PKT_TYPE_WR, R9_A2_S0_CCD_ENABLE2, 0x00);
static DEFINE_STATIC_PACKET(r9_a2_s0_ccd_test_disable, DSI_PKT_TYPE_WR, R9_A2_S0_CCD_DISABLE, 0x4A);
#endif

#ifdef CONFIG_SUPPORT_DDI_CMDLOG
static DEFINE_STATIC_PACKET(r9_a2_s0_cmdlog_enable, DSI_PKT_TYPE_WR, R9_A2_S0_CMDLOG_ENABLE, 0);
static DEFINE_STATIC_PACKET(r9_a2_s0_cmdlog_disable, DSI_PKT_TYPE_WR, R9_A2_S0_CMDLOG_DISABLE, 0);
#endif
static DEFINE_STATIC_PACKET(r9_a2_s0_gamma_update_enable, DSI_PKT_TYPE_WR, R9_A2_S0_GAMMA_UPDATE_ENABLE, 0);

static DEFINE_PANEL_MDELAY(r9_a2_s0_wait_7msec, 7);
static DEFINE_PANEL_MDELAY(r9_a2_s0_wait_10msec, 10);
static DEFINE_PANEL_MDELAY(r9_a2_s0_wait_30msec, 30);
static DEFINE_PANEL_MDELAY(r9_a2_s0_wait_90msec, 90);
static DEFINE_PANEL_MDELAY(r9_a2_s0_wait_17msec, 17);
static DEFINE_PANEL_MDELAY(r9_a2_s0_wait_20msec, 20);
static DEFINE_PANEL_MDELAY(r9_a2_s0_wait_34msec, 34);
static DEFINE_PANEL_MDELAY(r9_a2_s0_wait_100msec, 100);
static DEFINE_PANEL_MDELAY(r9_a2_s0_wait_mcd_180msec, 180);
static DEFINE_PANEL_MDELAY(r9_a2_s0_wait_sleep_out_20msec, 20);
static DEFINE_PANEL_MDELAY(r9_a2_s0_wait_fastdischarge_120msec, 120);
static DEFINE_PANEL_MDELAY(r9_a2_s0_wait_sleep_in, 120);
static DEFINE_PANEL_FRAME_DELAY(r9_a2_s0_wait_2_frame, 2);
static DEFINE_PANEL_FRAME_DELAY(r9_a2_s0_wait_1_frame, 1);
static DEFINE_PANEL_UDELAY(r9_a2_s0_wait_1usec, 1);
static DEFINE_PANEL_UDELAY(r9_a2_s0_wait_100usec, 100);
static DEFINE_PANEL_MDELAY(r9_a2_s0_wait_1msec, 1);
static DEFINE_PANEL_VSYNC_DELAY(r9_a2_s0_wait_1_vsync, 1);

static DEFINE_PANEL_TIMER_MDELAY(r9_a2_s0_wait_sleep_out_120msec, 120);
static DEFINE_PANEL_TIMER_BEGIN(r9_a2_s0_wait_sleep_out_120msec,
		TIMER_DLYINFO(&r9_a2_s0_wait_sleep_out_120msec));

static DEFINE_PANEL_KEY(r9_a2_s0_level1_key_enable, CMD_LEVEL_1, KEY_ENABLE, &PKTINFO(r9_a2_s0_level1_key_enable));
static DEFINE_PANEL_KEY(r9_a2_s0_level2_key_enable, CMD_LEVEL_2, KEY_ENABLE, &PKTINFO(r9_a2_s0_level2_key_enable));
static DEFINE_PANEL_KEY(r9_a2_s0_level1_key_disable, CMD_LEVEL_1, KEY_DISABLE, &PKTINFO(r9_a2_s0_level1_key_disable));
static DEFINE_PANEL_KEY(r9_a2_s0_level2_key_disable, CMD_LEVEL_2, KEY_DISABLE, &PKTINFO(r9_a2_s0_level2_key_disable));

#ifdef CONFIG_DYNAMIC_MIPI
static DEFINE_PKTUI(r9_a2_s0_set_ffc, &r9_a2_s0_maptbl[DM_SET_FFC_MAPTBL], 0);
static DEFINE_VARIABLE_PACKET(r9_a2_s0_set_ffc, DSI_PKT_TYPE_WR, R9_A2_S0_FFC_DEFAULT, 0);
#else
static DEFINE_STATIC_PACKET(r9_a2_s0_set_ffc, DSI_PKT_TYPE_WR, R9_A2_S0_FFC_DEFAULT, 0);
#endif

static char R9_A2_S0_OFF_FFC[] = {
	0xDE,
	0x00
};
static DEFINE_STATIC_PACKET(r9_a2_s0_off_ffc, DSI_PKT_TYPE_WR, R9_A2_S0_OFF_FFC, 0);

static void *r9_a2_s0_dm_set_ffc_cmdtbl[] = {
	&KEYINFO(r9_a2_s0_level1_key_enable),
	&KEYINFO(r9_a2_s0_level2_key_enable),
	&PKTINFO(r9_a2_s0_set_ffc),
	&PKTINFO(r9_a2_s0_gamma_update_enable),
	&KEYINFO(r9_a2_s0_level1_key_disable),
	&KEYINFO(r9_a2_s0_level2_key_disable),
};

static void *r9_a2_s0_dm_off_ffc_cmdtbl[] = {
	&KEYINFO(r9_a2_s0_level1_key_enable),
	&KEYINFO(r9_a2_s0_level2_key_enable),
	&PKTINFO(r9_a2_s0_off_ffc),
	&PKTINFO(r9_a2_s0_gamma_update_enable),
	&KEYINFO(r9_a2_s0_level2_key_disable),
	&KEYINFO(r9_a2_s0_level1_key_disable),
};

#ifdef CONFIG_SUPPORT_TIG
static char R9_A2_S0_TIG_ENABLE[] = {
	0xE7,
	0x81
};
static DEFINE_STATIC_PACKET(r9_a2_s0_tig_enable, DSI_PKT_TYPE_WR, R9_A2_S0_TIG_ENABLE, 0x0A);

static char R9_A2_S0_TIG_DISABLE[] = {
	0xE7,
	0x01
};
static DEFINE_STATIC_PACKET(r9_a2_s0_tig_disable, DSI_PKT_TYPE_WR, R9_A2_S0_TIG_DISABLE, 0x0A);

static void *r9_a2_s0_tig_enable_cmdtbl[] = {
	&KEYINFO(r9_a2_s0_level1_key_enable),
	&PKTINFO(r9_a2_s0_tig_enable),
	&KEYINFO(r9_a2_s0_level1_key_disable),
};

static void *r9_a2_s0_tig_disable_cmdtbl[] = {
	&KEYINFO(r9_a2_s0_level1_key_enable),
	&PKTINFO(r9_a2_s0_tig_disable),
	&KEYINFO(r9_a2_s0_level1_key_disable),
};
#endif

/* temporary bl code start */

static u8 R9_A2_S0_HBM_AND_TRANSITION[] = {
	0x53, 0x20
};
static DEFINE_PKTUI(r9_a2_s0_hbm_and_transition, &r9_a2_s0_maptbl[HBM_AND_TRANSITION_MAPTBL], 1);
static DEFINE_VARIABLE_PACKET(r9_a2_s0_hbm_and_transition, DSI_PKT_TYPE_WR, R9_A2_S0_HBM_AND_TRANSITION, 0);

static u8 R9_A2_S0_ACL_CONTROL[] = {
	0x55,
	0x00
};
static DEFINE_PKTUI(r9_a2_s0_acl_control, &r9_a2_s0_maptbl[ACL_OPR_MAPTBL], 1);
static DEFINE_VARIABLE_PACKET(r9_a2_s0_acl_control, DSI_PKT_TYPE_WR, R9_A2_S0_ACL_CONTROL, 0x00);

static u8 R9_A2_S0_WRDISBV[] = {
	0x51, 0x03, 0xFF
};
static DEFINE_PKTUI(r9_a2_s0_wrdisbv, &r9_a2_s0_maptbl[GAMMA_MODE2_MAPTBL], 1);
static DEFINE_VARIABLE_PACKET(r9_a2_s0_wrdisbv, DSI_PKT_TYPE_WR, R9_A2_S0_WRDISBV, 0);

static u8 R9_A2_S0_CASET[] = { 0x2A, 0x00, 0x00, 0x04, 0x37 };
static u8 R9_A2_S0_PASET[] = { 0x2B, 0x00, 0x00, 0x09, 0x23 };

static DEFINE_STATIC_PACKET(r9_a2_s0_caset, DSI_PKT_TYPE_WR, R9_A2_S0_CASET, 0);
static DEFINE_STATIC_PACKET(r9_a2_s0_paset, DSI_PKT_TYPE_WR, R9_A2_S0_PASET, 0);

static u8 R9_A2_S0_FPS[] = { 0x60, 0x00, 0x00};
static DEFINE_PKTUI(r9_a2_s0_fps, &r9_a2_s0_maptbl[FPS_MAPTBL], 1);
static DEFINE_VARIABLE_PACKET(r9_a2_s0_fps, DSI_PKT_TYPE_WR, R9_A2_S0_FPS, 0);

#ifdef CONFIG_SUPPORT_MASK_LAYER
static char R9_A2_S0_FOD_SET[] = {
	0xB4,
	0x20
};
static DEFINE_STATIC_PACKET(r9_a2_s0_fod_set, DSI_PKT_TYPE_WR, R9_A2_S0_FOD_SET, 0x0C);
#endif

static struct seqinfo SEQINFO(r9_a2_s0_set_bl_param_seq);
static struct seqinfo SEQINFO(r9_a2_s0_set_fps_param_seq);

static void *r9_a2_s0_init_cmdtbl[] = {
	&DLYINFO(r9_a2_s0_wait_10msec),
	&PKTINFO(r9_a2_s0_sleep_out),
	&KEYINFO(r9_a2_s0_level1_key_enable),
	&KEYINFO(r9_a2_s0_level2_key_enable),
	&DLYINFO(r9_a2_s0_wait_30msec),
	&PKTINFO(r9_a2_s0_dsc),
	&PKTINFO(r9_a2_s0_pps),
	&PKTINFO(r9_a2_s0_caset),
	&PKTINFO(r9_a2_s0_paset),
	&PKTINFO(r9_a2_s0_err_fg_1),
	&PKTINFO(r9_a2_s0_err_fg_2),
	&PKTINFO(r9_a2_s0_acl_default),
	&PKTINFO(r9_a2_s0_timing_offset1),
	&PKTINFO(r9_a2_s0_timing_offset2),
	&PKTINFO(r9_a2_s0_te_shift),
	&PKTINFO(r9_a2_s0_aswire_off),
	&PKTINFO(r9_a2_s0_etc_set1),
	&PKTINFO(r9_a2_s0_etc_set2),
	&PKTINFO(r9_a2_s0_etc_set3),
	&PKTINFO(r9_a2_s0_tsp_hsync1),
	&PKTINFO(r9_a2_s0_tsp_hsync2),
	&PKTINFO(r9_a2_s0_dia_onoff),
	&PKTINFO(r9_a2_s0_set_ffc),
	/* set brightness & fps */
	&SEQINFO(r9_a2_s0_set_fps_param_seq),
	&SEQINFO(r9_a2_s0_set_bl_param_seq),
	&PKTINFO(r9_a2_s0_gamma_update_enable),
	&PKTINFO(r9_a2_s0_te_on),
	&KEYINFO(r9_a2_s0_level2_key_disable),
	&KEYINFO(r9_a2_s0_level1_key_disable),
#if defined(CONFIG_SUPPORT_FAST_DISCHARGE)
	&KEYINFO(r9_a2_s0_level2_key_enable),
	&PKTINFO(r9_a2_s0_fast_discharge),
	&KEYINFO(r9_a2_s0_level2_key_disable),
	&DLYINFO(r9_a2_s0_wait_fastdischarge_120msec),
#endif
	&DLYINFO(r9_a2_s0_wait_90msec),
};

static void *r9_a2_s0_id_read_cmdtbl[] = {
	&KEYINFO(r9_a2_s0_level1_key_enable),
	&KEYINFO(r9_a2_s0_level2_key_enable),
	&ea8082_restbl[RES_ID],
	&KEYINFO(r9_a2_s0_level2_key_disable),
	&KEYINFO(r9_a2_s0_level1_key_disable),
};

static void *r9_a2_s0_res_init_cmdtbl[] = {
	&KEYINFO(r9_a2_s0_level1_key_enable),
	&KEYINFO(r9_a2_s0_level2_key_enable),
	&ea8082_restbl[RES_COORDINATE],
	&ea8082_restbl[RES_CODE],
	&ea8082_restbl[RES_DATE],
	&ea8082_restbl[RES_OCTA_ID],
#ifdef CONFIG_SUPPORT_GRAYSPOT_TEST
	&ea8082_restbl[RES_GRAYSPOT_CAL],
#endif
#ifdef CONFIG_DISPLAY_USE_INFO
	&ea8082_restbl[RES_CHIP_ID],
	&ea8082_restbl[RES_SELF_DIAG],
	&ea8082_restbl[RES_ERR_FG],
	&ea8082_restbl[RES_DSI_ERR],
#endif
	&KEYINFO(r9_a2_s0_level2_key_disable),
	&KEYINFO(r9_a2_s0_level1_key_disable),
};

static void *r9_a2_s0_set_bl_param_cmdtbl[] = {
	&PKTINFO(r9_a2_s0_hbm_and_transition),
	&PKTINFO(r9_a2_s0_acl_control),
	&PKTINFO(r9_a2_s0_tset),
	&PKTINFO(r9_a2_s0_wrdisbv),
#ifdef CONFIG_SUPPORT_XTALK_MODE
	&PKTINFO(r9_a2_s0_xtalk_mode),
#endif
};

static DEFINE_SEQINFO(r9_a2_s0_set_bl_param_seq,
		r9_a2_s0_set_bl_param_cmdtbl);

static void *r9_a2_s0_set_bl_cmdtbl[] = {
	&KEYINFO(r9_a2_s0_level1_key_enable),
	&SEQINFO(r9_a2_s0_set_fps_param_seq),
	&SEQINFO(r9_a2_s0_set_bl_param_seq),
	&PKTINFO(r9_a2_s0_gamma_update_enable),
	&KEYINFO(r9_a2_s0_level1_key_disable),
};

static void *r9_a2_s0_set_fps_param_cmdtbl[] = {
	/* fps & osc setting */
	&PKTINFO(r9_a2_s0_fps),
};

static DEFINE_SEQINFO(r9_a2_s0_set_fps_param_seq,
		r9_a2_s0_set_fps_param_cmdtbl);

static void *r9_a2_s0_set_fps_cmdtbl[] = {
	&CONDINFO_IF(r9_a2_s0_cond_is_wait_vsync_needed),
		&DLYINFO(r9_a2_s0_wait_1_vsync),
	&CONDINFO_FI(r9_a2_s0_cond_is_wait_vsync_needed),
	&KEYINFO(r9_a2_s0_level1_key_enable),
	&SEQINFO(r9_a2_s0_set_bl_param_seq),
	&SEQINFO(r9_a2_s0_set_fps_param_seq),
	&PKTINFO(r9_a2_s0_gamma_update_enable),
	&KEYINFO(r9_a2_s0_level1_key_disable),
};

#if defined(CONFIG_PANEL_DISPLAY_MODE)
static void *r9_a2_s0_display_mode_cmdtbl[] = {
	&CONDINFO_IF(r9_a2_s0_cond_is_panel_state_not_lpm),
		&CONDINFO_IF(r9_a2_s0_cond_is_wait_vsync_needed),
			&DLYINFO(r9_a2_s0_wait_1_vsync),
		&CONDINFO_FI(r9_a2_s0_cond_is_wait_vsync_needed),
		&KEYINFO(r9_a2_s0_level1_key_enable),
		&SEQINFO(r9_a2_s0_set_bl_param_seq),
		&SEQINFO(r9_a2_s0_set_fps_param_seq),
		&PKTINFO(r9_a2_s0_gamma_update_enable),
		&KEYINFO(r9_a2_s0_level1_key_disable),
	&CONDINFO_FI(r9_a2_s0_cond_is_panel_state_not_lpm),
};
#endif

static void *r9_a2_s0_display_on_cmdtbl[] = {
	&KEYINFO(r9_a2_s0_level1_key_enable),
	&PKTINFO(r9_a2_s0_display_on),
	&KEYINFO(r9_a2_s0_level1_key_disable),
};

static void *r9_a2_s0_display_off_cmdtbl[] = {
	&KEYINFO(r9_a2_s0_level1_key_enable),
	&PKTINFO(r9_a2_s0_display_off),
	&KEYINFO(r9_a2_s0_level1_key_disable),
};

static void *r9_a2_s0_exit_cmdtbl[] = {
 	&KEYINFO(r9_a2_s0_level1_key_enable),
	&KEYINFO(r9_a2_s0_level2_key_enable),
	&ea8082_dmptbl[DUMP_RDDPM_SLEEP_IN],
#ifdef CONFIG_DISPLAY_USE_INFO
	&ea8082_dmptbl[DUMP_RDDSM],
	&ea8082_dmptbl[DUMP_DSI_ERR],
	&ea8082_dmptbl[DUMP_SELF_DIAG],
#endif
	&KEYINFO(r9_a2_s0_level2_key_disable),
	&PKTINFO(r9_a2_s0_sleep_in),
	&KEYINFO(r9_a2_s0_level1_key_disable),
	&DLYINFO(r9_a2_s0_wait_sleep_in),
};

static void *r9_a2_s0_dia_onoff_cmdtbl[] = {
	&KEYINFO(r9_a2_s0_level1_key_enable),
	&PKTINFO(r9_a2_s0_dia_onoff),
	&KEYINFO(r9_a2_s0_level1_key_disable),
};

#if defined(CONFIG_SUPPORT_FAST_DISCHARGE)
static void *r9_a2_s0_fast_discharge_cmdtbl[] = {
	&KEYINFO(r9_a2_s0_level1_key_enable),
	&PKTINFO(r9_a2_s0_fast_discharge),
	&KEYINFO(r9_a2_s0_level1_key_disable),
	&DLYINFO(r9_a2_s0_wait_fastdischarge_120msec),
};
#endif

static void *r9_a2_s0_alpm_enter_cmdtbl[] = {
	&KEYINFO(r9_a2_s0_level1_key_enable),
	&PKTINFO(r9_a2_s0_lpm_on),
	&DLYINFO(r9_a2_s0_wait_1msec),
	&KEYINFO(r9_a2_s0_level1_key_disable),
};

static void *r9_a2_s0_alpm_exit_cmdtbl[] = {
	&KEYINFO(r9_a2_s0_level1_key_enable),
	&PKTINFO(r9_a2_s0_lpm_off),
	&DLYINFO(r9_a2_s0_wait_34msec),
	&KEYINFO(r9_a2_s0_level1_key_disable),
};

static void *r9_a2_s0_mcd_on_cmdtbl[] = {
	&KEYINFO(r9_a2_s0_level1_key_enable),
	&PKTINFO(r9_a2_s0_mcd_on_01),
	&PKTINFO(r9_a2_s0_mcd_on_02),
	&PKTINFO(r9_a2_s0_gamma_update_enable),
	&DLYINFO(r9_a2_s0_wait_mcd_180msec),
	&PKTINFO(r9_a2_s0_mcd_on_03),
	&PKTINFO(r9_a2_s0_mcd_on_04),
	&PKTINFO(r9_a2_s0_mcd_on_05),
	&PKTINFO(r9_a2_s0_gamma_update_enable),
	&DLYINFO(r9_a2_s0_wait_100msec),
	&KEYINFO(r9_a2_s0_level1_key_disable),
};

static void *r9_a2_s0_mcd_off_cmdtbl[] = {
	&KEYINFO(r9_a2_s0_level1_key_enable),
	&PKTINFO(r9_a2_s0_mcd_off_01),
	&PKTINFO(r9_a2_s0_mcd_off_02),
	&PKTINFO(r9_a2_s0_mcd_off_03),
	&PKTINFO(r9_a2_s0_gamma_update_enable),
	&DLYINFO(r9_a2_s0_wait_mcd_180msec),
	&PKTINFO(r9_a2_s0_mcd_off_04),
	&PKTINFO(r9_a2_s0_mcd_off_05),
	&PKTINFO(r9_a2_s0_gamma_update_enable),
	&DLYINFO(r9_a2_s0_wait_100msec),
	&KEYINFO(r9_a2_s0_level1_key_disable),
};

#ifdef CONFIG_SUPPORT_GRAYSPOT_TEST
static void *r9_a2_s0_grayspot_on_cmdtbl[] = {
	&KEYINFO(r9_a2_s0_level1_key_enable),
	&PKTINFO(r9_a2_s0_grayspot_on_01),
	&PKTINFO(r9_a2_s0_grayspot_on_02),
	&PKTINFO(r9_a2_s0_grayspot_on_03),
	&PKTINFO(r9_a2_s0_grayspot_elvss_set),
	&PKTINFO(r9_a2_s0_gamma_update_enable),
	&DLYINFO(r9_a2_s0_wait_100msec),
	&KEYINFO(r9_a2_s0_level1_key_disable),
};

static void *r9_a2_s0_grayspot_off_cmdtbl[] = {
	&KEYINFO(r9_a2_s0_level1_key_enable),
	&PKTINFO(r9_a2_s0_grayspot_off_01),
	&PKTINFO(r9_a2_s0_grayspot_off_02),
	&PKTINFO(r9_a2_s0_grayspot_off_03),
	&PKTINFO(r9_a2_s0_grayspot_elvss_set),
	&PKTINFO(r9_a2_s0_gamma_update_enable),
	&DLYINFO(r9_a2_s0_wait_100msec),
	&KEYINFO(r9_a2_s0_level1_key_disable),
};
#endif

#ifdef CONFIG_SUPPORT_MST
static void *r9_a2_s0_mst_on_cmdtbl[] = {
	&KEYINFO(r9_a2_s0_level1_key_enable),
	&PKTINFO(r9_a2_s0_mst_on_01),
	&PKTINFO(r9_a2_s0_mst_on_02),
	&PKTINFO(r9_a2_s0_mst_on_03),
	&PKTINFO(r9_a2_s0_gamma_update_enable),
	&KEYINFO(r9_a2_s0_level1_key_disable),
};

static void *r9_a2_s0_mst_off_cmdtbl[] = {
	&KEYINFO(r9_a2_s0_level1_key_enable),
	&PKTINFO(r9_a2_s0_mst_off_01),
	&PKTINFO(r9_a2_s0_mst_off_02),
	&PKTINFO(r9_a2_s0_mst_off_03),
	&PKTINFO(r9_a2_s0_gamma_update_enable),
	&KEYINFO(r9_a2_s0_level1_key_disable),
};
#endif

#ifdef CONFIG_SUPPORT_CCD_TEST
static void *r9_a2_s0_ccd_test_cmdtbl[] = {
	&KEYINFO(r9_a2_s0_level1_key_enable),
	&PKTINFO(r9_a2_s0_ccd_test_enable1),
	&PKTINFO(r9_a2_s0_ccd_test_enable2),
	&DLYINFO(r9_a2_s0_wait_1msec),
	&ea8082_restbl[RES_CCD_STATE],
	&PKTINFO(r9_a2_s0_ccd_test_disable),
	&KEYINFO(r9_a2_s0_level1_key_disable),
};
#endif

#ifdef CONFIG_SUPPORT_DDI_CMDLOG
static void *r9_a2_s0_cmdlog_dump_cmdtbl[] = {
	&KEYINFO(r9_a2_s0_level2_key_enable),
	&ea8082_dmptbl[DUMP_CMDLOG],
	&KEYINFO(r9_a2_s0_level2_key_disable),
};
#endif


static void *r9_a2_s0_check_condition_cmdtbl[] = {
	&KEYINFO(r9_a2_s0_level1_key_enable),
	&KEYINFO(r9_a2_s0_level2_key_enable),
	&ea8082_dmptbl[DUMP_RDDPM],
	&ea8082_dmptbl[DUMP_SELF_DIAG],
	&KEYINFO(r9_a2_s0_level2_key_disable),
	&KEYINFO(r9_a2_s0_level1_key_disable),
};

static void *r9_a2_s0_dump_cmdtbl[] = {
	&KEYINFO(r9_a2_s0_level1_key_enable),
	&KEYINFO(r9_a2_s0_level2_key_enable),
	&ea8082_dmptbl[DUMP_RDDPM],
	&ea8082_dmptbl[DUMP_RDDSM],
	&ea8082_dmptbl[DUMP_DSI_ERR],
	&ea8082_dmptbl[DUMP_SELF_DIAG],
	&KEYINFO(r9_a2_s0_level2_key_disable),
	&KEYINFO(r9_a2_s0_level1_key_disable),
};

#ifdef CONFIG_SUPPORT_MASK_LAYER
static void *r9s_mask_layer_workaround_cmdtbl[] = {
	&PKTINFO(r9_a2_s0_wrdisbv),
	&PKTINFO(r9_a2_s0_hbm_and_transition),
	&DLYINFO(r9_a2_s0_wait_2_frame),
};

static void *r9s_mask_layer_enter_br_cmdtbl[] = {
	&KEYINFO(r9_a2_s0_level1_key_enable),
	&PKTINFO(r9_a2_s0_fod_set),
	&SEQINFO(r9_a2_s0_set_bl_param_seq),
	&SEQINFO(r9_a2_s0_set_fps_param_seq),
	&PKTINFO(r9_a2_s0_gamma_update_enable),
	&KEYINFO(r9_a2_s0_level1_key_disable),
	&DLYINFO(r9_a2_s0_wait_7msec),
};

static void *r9s_mask_layer_exit_br_cmdtbl[] = {
	&KEYINFO(r9_a2_s0_level1_key_enable),
	&PKTINFO(r9_a2_s0_acl_control),
	&PKTINFO(r9_a2_s0_fod_set),
	&PKTINFO(r9_a2_s0_hbm_and_transition),
	&PKTINFO(r9_a2_s0_wrdisbv),
	&PKTINFO(r9_a2_s0_tset),
	&DLYINFO(r9_a2_s0_wait_1_frame),
	&SEQINFO(r9_a2_s0_set_fps_param_seq),
	&PKTINFO(r9_a2_s0_gamma_update_enable),
	&KEYINFO(r9_a2_s0_level1_key_disable),
};
#endif


static void *r9_a2_s0_dummy_cmdtbl[] = {
	NULL,
};

static struct seqinfo r9_a2_s0_seqtbl[MAX_PANEL_SEQ] = {
	[PANEL_INIT_SEQ] = SEQINFO_INIT("init-seq", r9_a2_s0_init_cmdtbl),
	[PANEL_RES_INIT_SEQ] = SEQINFO_INIT("resource-init-seq", r9_a2_s0_res_init_cmdtbl),
	[PANEL_ID_READ_SEQ] = SEQINFO_INIT("id-read-seq", r9_a2_s0_id_read_cmdtbl),
	[PANEL_SET_BL_SEQ] = SEQINFO_INIT("set-bl-seq", r9_a2_s0_set_bl_cmdtbl),
#ifdef CONFIG_SUPPORT_HMD
	[PANEL_HMD_ON_SEQ] = SEQINFO_INIT("hmd-on-seq", r9_a2_s0_hmd_on_cmdtbl),
	[PANEL_HMD_OFF_SEQ] = SEQINFO_INIT("hmd-off-seq", r9_a2_s0_hmd_off_cmdtbl),
	[PANEL_HMD_BL_SEQ] = SEQINFO_INIT("hmd-bl-seq", r9_a2_s0_hmd_bl_cmdtbl),
#endif
	[PANEL_DISPLAY_ON_SEQ] = SEQINFO_INIT("display-on-seq", r9_a2_s0_display_on_cmdtbl),
	[PANEL_DISPLAY_OFF_SEQ] = SEQINFO_INIT("display-off-seq", r9_a2_s0_display_off_cmdtbl),
	[PANEL_EXIT_SEQ] = SEQINFO_INIT("exit-seq", r9_a2_s0_exit_cmdtbl),
	[PANEL_ALPM_ENTER_SEQ] = SEQINFO_INIT("alpm-enter-seq", r9_a2_s0_alpm_enter_cmdtbl),
	[PANEL_ALPM_EXIT_SEQ] = SEQINFO_INIT("alpm-exit-seq", r9_a2_s0_alpm_exit_cmdtbl),
	[PANEL_FPS_SEQ] = SEQINFO_INIT("set-fps-seq", r9_a2_s0_set_fps_cmdtbl),
	[PANEL_DIA_ONOFF_SEQ] = SEQINFO_INIT("dia-onoff-seq", r9_a2_s0_dia_onoff_cmdtbl),
#if defined(CONFIG_PANEL_DISPLAY_MODE)
	[PANEL_DISPLAY_MODE_SEQ] = SEQINFO_INIT("display-mode-seq", r9_a2_s0_display_mode_cmdtbl),
#endif
	[PANEL_MCD_ON_SEQ] = SEQINFO_INIT("mcd-on-seq", r9_a2_s0_mcd_on_cmdtbl),
	[PANEL_MCD_OFF_SEQ] = SEQINFO_INIT("mcd-off-seq", r9_a2_s0_mcd_off_cmdtbl),
#ifdef CONFIG_SUPPORT_GRAYSPOT_TEST
	[PANEL_GRAYSPOT_ON_SEQ] = SEQINFO_INIT("grayspot-on-seq", r9_a2_s0_grayspot_on_cmdtbl),
	[PANEL_GRAYSPOT_OFF_SEQ] = SEQINFO_INIT("grayspot-off-seq", r9_a2_s0_grayspot_off_cmdtbl),
#endif
#ifdef CONFIG_SUPPORT_MST
	[PANEL_MST_ON_SEQ] = SEQINFO_INIT("mst-on-seq", r9_a2_s0_mst_on_cmdtbl),
	[PANEL_MST_OFF_SEQ] = SEQINFO_INIT("mst-off-seq", r9_a2_s0_mst_off_cmdtbl),
#endif
#ifdef CONFIG_SUPPORT_CCD_TEST
	[PANEL_CCD_TEST_SEQ] = SEQINFO_INIT("ccd-test-seq", r9_a2_s0_ccd_test_cmdtbl),
#endif
	[PANEL_CHECK_CONDITION_SEQ] = SEQINFO_INIT("check-condition-seq", r9_a2_s0_check_condition_cmdtbl),
	[PANEL_DUMP_SEQ] = SEQINFO_INIT("dump-seq", r9_a2_s0_dump_cmdtbl),
#ifdef CONFIG_SUPPORT_DDI_CMDLOG
	[PANEL_CMDLOG_DUMP_SEQ] = SEQINFO_INIT("cmdlog-dump-seq", r9_a2_s0_cmdlog_dump_cmdtbl),
#endif
#ifdef CONFIG_DYNAMIC_MIPI
	[PANEL_DM_SET_FFC_SEQ] = SEQINFO_INIT("dm-set_ffc-seq", r9_a2_s0_dm_set_ffc_cmdtbl),
	[PANEL_DM_OFF_FFC_SEQ] = SEQINFO_INIT("dm-off_ffc-seq", r9_a2_s0_dm_off_ffc_cmdtbl),
#endif
#if defined(CONFIG_SUPPORT_FAST_DISCHARGE)
	[PANEL_FD_SEQ] = SEQINFO_INIT("fast-discharge-seq", r9_a2_s0_fast_discharge_cmdtbl),
#endif
#ifdef CONFIG_SUPPORT_MASK_LAYER
	[PANEL_MASK_LAYER_STOP_DIMMING_SEQ] = SEQINFO_INIT("mask-layer-workaround-seq", r9s_mask_layer_workaround_cmdtbl),
	[PANEL_MASK_LAYER_ENTER_BR_SEQ] = SEQINFO_INIT("mask-layer-enter-br-seq", r9s_mask_layer_enter_br_cmdtbl), //temp br
	[PANEL_MASK_LAYER_EXIT_BR_SEQ] = SEQINFO_INIT("mask-layer-exit-br-seq", r9s_mask_layer_exit_br_cmdtbl),
#endif
#ifdef CONFIG_SUPPORT_TIG
	[PANEL_TIG_ENABLE_SEQ] = SEQINFO_INIT("tig-enable-seq", r9_a2_s0_tig_enable_cmdtbl),
	[PANEL_TIG_DISABLE_SEQ] = SEQINFO_INIT("tig-disable-seq", r9_a2_s0_tig_disable_cmdtbl),
#endif
	[PANEL_DUMMY_SEQ] = SEQINFO_INIT("dummy-seq", r9_a2_s0_dummy_cmdtbl),
};

struct common_panel_info ea8082_r9_a2_s0_panel_info = {
	.ldi_name = "ea8082",
	.name = "ea8082_r9_a2_s0",
	.model = "AMB675TG01",
	.vendor = "SDC",
	.id = 0x810000,
	.rev = 0,
	.ddi_props = {
		.gpara = (DDI_SUPPORT_WRITE_GPARA |
				DDI_SUPPORT_READ_GPARA | DDI_SUPPORT_POINT_GPARA),
		.err_fg_recovery = false,
		.support_vrr = true,
		.after_panel_reset = true,
	},
	.ddi_ops = {
#ifdef CONFIG_SUPPORT_DDI_FLASH
		.gamma_flash_checksum = do_gamma_flash_checksum,
		.mtp_gamma_check = ea8082_mtp_gamma_check,
#endif
	},
#ifdef CONFIG_SUPPORT_DSU
	.mres = {
		.nr_resol = ARRAY_SIZE(ea8082_r9_default_resol),
		.resol = ea8082_r9_default_resol,
	},
#endif
#if defined(CONFIG_PANEL_DISPLAY_MODE)
	.common_panel_modes = &ea8082_r9_display_modes,
#endif
	.vrrtbl = ea8082_r9_default_vrrtbl,
	.nr_vrrtbl = ARRAY_SIZE(ea8082_r9_default_vrrtbl),
	.maptbl = r9_a2_s0_maptbl,
	.nr_maptbl = ARRAY_SIZE(r9_a2_s0_maptbl),
	.seqtbl = r9_a2_s0_seqtbl,
	.nr_seqtbl = ARRAY_SIZE(r9_a2_s0_seqtbl),
	.rditbl = ea8082_rditbl,
	.nr_rditbl = ARRAY_SIZE(ea8082_rditbl),
	.restbl = ea8082_restbl,
	.nr_restbl = ARRAY_SIZE(ea8082_restbl),
	.dumpinfo = ea8082_dmptbl,
	.nr_dumpinfo = ARRAY_SIZE(ea8082_dmptbl),
#ifdef CONFIG_EXYNOS_DECON_MDNIE_LITE
	.mdnie_tune = &ea8082_r9_a2_s0_mdnie_tune,
#endif
	.panel_dim_info = {
		[PANEL_BL_SUBDEV_TYPE_DISP] = &ea8082_r9_a2_s0_panel_dimming_info,
#ifdef CONFIG_SUPPORT_HMD
		[PANEL_BL_SUBDEV_TYPE_HMD] = &ea8082_r9_a2_s0_panel_hmd_dimming_info,
#endif
#ifdef CONFIG_SUPPORT_AOD_BL
		[PANEL_BL_SUBDEV_TYPE_AOD] = &ea8082_r9_a2_s0_panel_aod_dimming_info,
#endif
	},
#ifdef CONFIG_DYNAMIC_MIPI
	.dm_total_band = r9_dynamic_freq_set,
#endif
};
#endif /* __EA8082_R9_A2_S0_PANEL_H__ */
