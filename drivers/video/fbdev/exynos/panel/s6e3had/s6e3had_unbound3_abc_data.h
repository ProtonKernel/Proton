/*
 * linux/drivers/video/fbdev/exynos/panel/s6e3had/s6e3had_unbound3_mafpc_panel.h
 *
 * Header file for mAFPC Driver
 *
 * Copyright (c) 2020 Samsung Electronics
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#ifndef __S6E3HAD_UNBOUND3_ABC_DATA_H__
#define __S6E3HAD_UNBOUND3_ABC_DATA_H__

#include "s6e3had_dimming.h"

#ifdef CONFIG_SUPPORT_MAFPC
#if defined(CONFIG_SEC_FACTORY)
#include "s6e3had_unbound3_abc_fac_img.h"
#else
#include "s6e3had_unbound3_abc_img.h"
#endif
#endif

static u8 S6E3HAD_UNBOUND3_MAFPC_DEFAULT_SCALE_FACTOR[225] = {
	0xFF, 0xFF, 0xFF,
	0xFF, 0xFF, 0xFF,
	0xFF, 0xFF, 0xFF,
	0xFF, 0xFF, 0xFF,
	0xFF, 0xFF, 0xFF,
	0xFF, 0xFF, 0xFF,
	0xFF, 0xFF, 0xFF,
	0xFF, 0xFF, 0xFF,
	0xFF, 0xFF, 0xFF,
	0xFF, 0xFF, 0xFF,
	0xFF, 0xFF, 0xFF,
	0xFF, 0xFF, 0xFF,
	0xFF, 0xFF, 0xFF,
	0xFF, 0xFF, 0xFF,
	0xFF, 0xFF, 0xFF,
	0xFF, 0xFF, 0xFF,
	0xFF, 0xFF, 0xFF,
	0xFF, 0xFF, 0xFF,
	0xFF, 0xFF, 0xFF,
	0xFF, 0xFF, 0xFF,
	0xFF, 0xFF, 0xFF,
	0xFF, 0xFF, 0xFF,
	0xFF, 0xFF, 0xFF,
	0xFF, 0xFF, 0xFF,
	0xFF, 0xFF, 0xFF,
	0xFF, 0xFF, 0xFF,
	0xFF, 0xFF, 0xFF,
	0xFF, 0xFF, 0xFF,
	0xFF, 0xFF, 0xFF,
	0xFF, 0xFF, 0xFF,
	0xFF, 0xFF, 0xFF,
	0xFF, 0xFF, 0xFF,
	0xFF, 0xFF, 0xFF,
	0xFF, 0xFF, 0xFF,
	0xFF, 0xFF, 0xFF,
	0xFF, 0xFF, 0xFF,
	0xFF, 0xFF, 0xFF,
	0xFF, 0xFF, 0xFF,
	0xFF, 0xFF, 0xFF,
	0xFF, 0xFF, 0xFF,
	0xFF, 0xFF, 0xFF,
	0xFF, 0xFF, 0xFF,
	0xFF, 0xFF, 0xFF,
	0xFF, 0xFF, 0xFF,
	0xFF, 0xFF, 0xFF,
	0xFF, 0xFF, 0xFF,
	0xFF, 0xFF, 0xFF,
	0xFF, 0xFF, 0xFF,
	0xFF, 0xFF, 0xFF,
	0xFF, 0xFF, 0xFF,
	0xFF, 0xFF, 0xFF,
	0xFF, 0xFF, 0xFF,
	0xFF, 0xFF, 0xFF,
	0xFF, 0xFF, 0xFF,
	0xFF, 0xFF, 0xFF,
	0xFF, 0xFF, 0xFF,
	0xFF, 0xFF, 0xFF,
	0xFF, 0xFF, 0xFF,
	0xFF, 0xFF, 0xFF,
	0xFF, 0xFF, 0xFF,
	0xFF, 0xFF, 0xFF,
	0xFF, 0xFF, 0xFF,
	0xFF, 0xFF, 0xFF,
	0xFF, 0xFF, 0xFF,
	0xFF, 0xFF, 0xFF,
	0xFF, 0xFF, 0xFF,
	0xFF, 0xFF, 0xFF,
	0xFF, 0xFF, 0xFF,
	0xFF, 0xFF, 0xFF,
	0xFF, 0xFF, 0xFF,
	0xFF, 0xFF, 0xFF,
	0xFF, 0xFF, 0xFF,
	0xFF, 0xFF, 0xFF,
	0xFF, 0xFF, 0xFF,
	0xFF, 0xFF, 0xFF,
};


static u8 s6e3had_unbound3_mafpc_crc_value[] = {
	0x30, 0xAC
};

static u8 s6e3had_unbound3_mafpc_scale_map_tbl[S6E3HAD_UNBOUND3_TOTAL_NR_LUMINANCE] = {
     0,  1,  2,  3,  4,  5,  6,  7,  8,  9,
    10, 10, 11, 12, 13, 13, 14, 14, 15, 16,
    17, 18, 18, 19, 19, 20, 21, 22, 22, 22,
    23, 23, 24, 24, 25, 25, 26, 26, 27, 27,
    27, 28, 28, 28, 29, 29, 30, 30, 30, 30,
    31, 31, 31, 32, 32, 32, 32, 33, 33, 33,
    34, 34, 34, 34, 35, 35, 35, 35, 36, 36,
    36, 36, 36, 37, 37, 37, 37, 38, 38, 38,
    38, 38, 38, 39, 39, 39, 39, 39, 40, 40,
    40, 40, 40, 41, 41, 41, 41, 41, 41, 41,
    42, 42, 42, 42, 42, 42, 43, 43, 43, 43,
    43, 43, 43, 44, 44, 44, 44, 44, 44, 44,
    45, 45, 45, 45, 45, 45, 45, 45, 46, 46,
    46, 46, 46, 46, 47, 47, 47, 47, 47, 47,
    47, 48, 48, 48, 48, 48, 48, 48, 49, 49,
    49, 49, 49, 49, 49, 49, 50, 50, 50, 50,
    51, 51, 51, 51, 52, 52, 52, 52, 53, 53,
    53, 53, 53, 54, 54, 54, 54, 55, 55, 55,
    55, 55, 56, 56, 56, 56, 56, 56, 56, 56,
    56, 56, 57, 57, 57, 57, 57, 57, 57, 57,
    58, 58, 58, 58, 58, 58, 58, 58, 58, 58,
    59, 59, 59, 59, 59, 59, 59, 59, 60, 61,
    61, 62, 62, 63, 63, 64, 64, 65, 65, 66,
    66, 67, 67, 68, 68, 69, 69, 69, 69, 69,
    70, 70, 70, 70, 70, 71, 71, 71, 71, 71,
    72, 72, 72, 72, 72, 73,
    /* hbm 10x20+4 */
    74, 74, 74, 74, 74, 74, 74, 74, 74, 74,
    74, 74, 74, 74, 74, 74, 74, 74, 74, 74,
    74, 74, 74, 74, 74, 74, 74, 74, 74, 74,
    74, 74, 74, 74, 74, 74, 74, 74, 74, 74,
    74, 74, 74, 74, 74, 74, 74, 74, 74, 74,
    74, 74, 74, 74, 74, 74, 74, 74, 74, 74,
    74, 74, 74, 74, 74, 74, 74, 74, 74, 74,
    74, 74, 74, 74, 74, 74, 74, 74, 74, 74,
    74, 74, 74, 74, 74, 74, 74, 74, 74, 74,
    74, 74, 74, 74, 74, 74, 74, 74, 74, 74,
    74, 74, 74, 74, 74, 74, 74, 74, 74, 74,
    74, 74, 74, 74, 74, 74, 74, 74, 74, 74,
    74, 74, 74, 74, 74, 74, 74, 74, 74, 74,
    74, 74, 74, 74, 74, 74, 74, 74, 74, 74,
    74, 74, 74, 74, 74, 74, 74, 74, 74, 74,
    74, 74, 74, 74, 74, 74, 74, 74, 74, 74,
    74, 74, 74, 74, 74, 74, 74, 74, 74, 74,
    74, 74, 74, 74, 74, 74, 74, 74, 74, 74,
    74, 74, 74, 74, 74, 74, 74, 74, 74, 74,
    74, 74, 74, 74, 74, 74, 74, 74, 74, 74,
    74, 74, 74, 74,
};

#define S6E3HAD_MAFPC_CTRL_SIZE	66

static struct mafpc_info s6e3had_unbound3_mafpc = {
	.name = "s6e3had_unbound3_mafpc",
	.abc_img = S6E3HAD_MAFPC_DEFAULT_IMG,
	.abc_img_len = ARRAY_SIZE(S6E3HAD_MAFPC_DEFAULT_IMG),
	.abc_scale_factor = S6E3HAD_UNBOUND3_MAFPC_DEFAULT_SCALE_FACTOR,
	.abc_scale_factor_len = ARRAY_SIZE(S6E3HAD_UNBOUND3_MAFPC_DEFAULT_SCALE_FACTOR),
	.abc_crc = s6e3had_unbound3_mafpc_crc_value,
	.abc_crc_len = ARRAY_SIZE(s6e3had_unbound3_mafpc_crc_value),
	.abc_scale_map_tbl = s6e3had_unbound3_mafpc_scale_map_tbl,
	.abc_scale_map_tbl_len = S6E3HAD_UNBOUND3_TOTAL_NR_LUMINANCE, 
	.abc_ctrl_cmd_len = S6E3HAD_MAFPC_CTRL_SIZE,
};

#endif //__S6E3HAD_UNBOUND3_MAFPC_PANEL_H__

