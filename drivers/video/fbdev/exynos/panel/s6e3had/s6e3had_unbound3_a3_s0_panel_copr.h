/*
 * linux/drivers/video/fbdev/exynos/panel/s6e3had/s6e3had_unbound3_a3_s0_panel_copr.h
 *
 * Header file for COPR Driver
 *
 * Copyright (c) 2016 Samsung Electronics
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#ifndef __S6E3HAD_UNBOUND3_A3_S0_PANEL_COPR_H__
#define __S6E3HAD_UNBOUND3_A3_S0_PANEL_COPR_H__

#include "../panel.h"
#include "../copr.h"
#include "s6e3had_unbound3_a3_s0_panel.h"

#define S6E3HAD_UNBOUND3_A3_S0_COPR_EN	(1)
#define S6E3HAD_UNBOUND3_A3_S0_COPR_PWR	(1)
#define S6E3HAD_UNBOUND3_A3_S0_COPR_MASK	(0)
#define S6E3HAD_UNBOUND3_A3_S0_COPR_ROI_CTRL	(0x1F)
#define S6E3HAD_UNBOUND3_A3_S0_COPR_GAMMA	(0)		/* 0 : GAMMA_1, 1 : GAMMA_2_2 */
#define S6E3HAD_UNBOUND3_A3_S0_COPR_FRAME_COUNT	(255)

#define S6E3HAD_UNBOUND3_A3_S0_COPR_ROI1_ER		(256)
#define S6E3HAD_UNBOUND3_A3_S0_COPR_ROI1_EG		(256)
#define S6E3HAD_UNBOUND3_A3_S0_COPR_ROI1_EB		(256)

#define S6E3HAD_UNBOUND3_A3_S0_COPR_ROI2_ER		(256)
#define S6E3HAD_UNBOUND3_A3_S0_COPR_ROI2_EG		(256)
#define S6E3HAD_UNBOUND3_A3_S0_COPR_ROI2_EB		(256)

#define S6E3HAD_UNBOUND3_A3_S0_COPR_ROI3_ER		(256)
#define S6E3HAD_UNBOUND3_A3_S0_COPR_ROI3_EG		(256)
#define S6E3HAD_UNBOUND3_A3_S0_COPR_ROI3_EB		(256)

#define S6E3HAD_UNBOUND3_A3_S0_COPR_ROI4_ER		(256)
#define S6E3HAD_UNBOUND3_A3_S0_COPR_ROI4_EG		(256)
#define S6E3HAD_UNBOUND3_A3_S0_COPR_ROI4_EB		(256)

#define S6E3HAD_UNBOUND3_A3_S0_COPR_ROI5_ER		(256)
#define S6E3HAD_UNBOUND3_A3_S0_COPR_ROI5_EG		(256)
#define S6E3HAD_UNBOUND3_A3_S0_COPR_ROI5_EB		(256)

#define S6E3HAD_UNBOUND3_A3_S0_COPR_ROI1_X_S	(928)
#define S6E3HAD_UNBOUND3_A3_S0_COPR_ROI1_Y_S	(120)
#define S6E3HAD_UNBOUND3_A3_S0_COPR_ROI1_X_E	(992)
#define S6E3HAD_UNBOUND3_A3_S0_COPR_ROI1_Y_E	(184)

#define S6E3HAD_UNBOUND3_A3_S0_COPR_ROI2_X_S	(0)
#define S6E3HAD_UNBOUND3_A3_S0_COPR_ROI2_Y_S	(0)
#define S6E3HAD_UNBOUND3_A3_S0_COPR_ROI2_X_E	(1439)
#define S6E3HAD_UNBOUND3_A3_S0_COPR_ROI2_Y_E	(3199)

#define S6E3HAD_UNBOUND3_A3_S0_COPR_ROI3_X_S	(0)
#define S6E3HAD_UNBOUND3_A3_S0_COPR_ROI3_Y_S	(0)
#define S6E3HAD_UNBOUND3_A3_S0_COPR_ROI3_X_E	(1439)
#define S6E3HAD_UNBOUND3_A3_S0_COPR_ROI3_Y_E	(3199)

#define S6E3HAD_UNBOUND3_A3_S0_COPR_ROI4_X_S	(0)
#define S6E3HAD_UNBOUND3_A3_S0_COPR_ROI4_Y_S	(0)
#define S6E3HAD_UNBOUND3_A3_S0_COPR_ROI4_X_E	(1439)
#define S6E3HAD_UNBOUND3_A3_S0_COPR_ROI4_Y_E	(3199)

#define S6E3HAD_UNBOUND3_A3_S0_COPR_ROI5_X_S	(0)
#define S6E3HAD_UNBOUND3_A3_S0_COPR_ROI5_Y_S	(0)
#define S6E3HAD_UNBOUND3_A3_S0_COPR_ROI5_X_E	(1439)
#define S6E3HAD_UNBOUND3_A3_S0_COPR_ROI5_Y_E	(3199)

static struct seqinfo unbound3_a3_s0_copr_seqtbl[MAX_COPR_SEQ];
static struct pktinfo PKTINFO(unbound3_a3_s0_level2_key_enable);
static struct pktinfo PKTINFO(unbound3_a3_s0_level2_key_disable);

/* ===================================================================================== */
/* ============================== [S6E3HAD MAPPING TABLE] ============================== */
/* ===================================================================================== */
static struct maptbl unbound3_a3_s0_copr_maptbl[] = {
	[COPR_MAPTBL] = DEFINE_0D_MAPTBL(unbound3_a3_s0_copr_table, init_common_table, NULL, copy_copr_maptbl),
};

/* ===================================================================================== */
/* ============================== [S6E3HAD COMMAND TABLE] ============================== */
/* ===================================================================================== */
static u8 UNBOUND3_A3_S0_COPR[] = {
	0xE1,
	0x03, 0x1F, 0x1F, 0x00, 0x00, 0x01, 0x00, 0x01, 0x00, 0x01,
	0x00, 0x01, 0x00, 0x01, 0x00, 0x01, 0x00, 0x01, 0x00, 0x01,
	0x00, 0x01, 0x00, 0x01, 0x00, 0x01, 0x00, 0x01, 0x00, 0x01,
	0x00, 0x01, 0x00, 0x01, 0x00, 0x03, 0xA0, 0x00, 0x78, 0x03,
	0xE0, 0x00, 0xB8, 0x00, 0x00, 0x00, 0x00, 0x05, 0x9F, 0x0C,
	0x7F, 0x00, 0x00, 0x00, 0x00, 0x05, 0x9F, 0x0C, 0x7F, 0x00,
	0x00, 0x00, 0x00, 0x05, 0x9F, 0x0C, 0x7F, 0x00, 0x00, 0x00,
	0x00, 0x05, 0x9F, 0x0C, 0x7F
};

static DEFINE_PKTUI(unbound3_a3_s0_copr, &unbound3_a3_s0_copr_maptbl[COPR_MAPTBL], 1);
static DEFINE_VARIABLE_PACKET(unbound3_a3_s0_copr, DSI_PKT_TYPE_WR, UNBOUND3_A3_S0_COPR, 0);

static void *unbound3_a3_s0_set_copr_cmdtbl[] = {
	&PKTINFO(unbound3_a3_s0_level2_key_enable),
	&PKTINFO(unbound3_a3_s0_copr),
	&PKTINFO(unbound3_a3_s0_level2_key_disable),
};

static void *unbound3_a3_s0_get_copr_spi_cmdtbl[] = {
	&s6e3had_restbl[RES_COPR_SPI],
};

static void *unbound3_a3_s0_get_copr_dsi_cmdtbl[] = {
	&s6e3had_restbl[RES_COPR_DSI],
};

static struct seqinfo unbound3_a3_s0_copr_seqtbl[MAX_COPR_SEQ] = {
	[COPR_SET_SEQ] = SEQINFO_INIT("set-copr-seq", unbound3_a3_s0_set_copr_cmdtbl),
	[COPR_SPI_GET_SEQ] = SEQINFO_INIT("get-copr-spi-seq", unbound3_a3_s0_get_copr_spi_cmdtbl),
	[COPR_DSI_GET_SEQ] = SEQINFO_INIT("get-copr-dsi-seq", unbound3_a3_s0_get_copr_dsi_cmdtbl),
};

static struct panel_copr_data s6e3had_unbound3_a3_s0_copr_data = {
	.seqtbl = unbound3_a3_s0_copr_seqtbl,
	.nr_seqtbl = ARRAY_SIZE(unbound3_a3_s0_copr_seqtbl),
	.maptbl = (struct maptbl *)unbound3_a3_s0_copr_maptbl,
	.nr_maptbl = (sizeof(unbound3_a3_s0_copr_maptbl) / sizeof(struct maptbl)),
	.version = COPR_VER_6,
	.options = {
		.thread_on = false,
		.check_avg = false,
	},
	.reg.v6 = {
		.copr_mask = S6E3HAD_UNBOUND3_A3_S0_COPR_MASK,
		.copr_pwr = S6E3HAD_UNBOUND3_A3_S0_COPR_PWR,
		.copr_en = S6E3HAD_UNBOUND3_A3_S0_COPR_EN,
		.copr_gamma = S6E3HAD_UNBOUND3_A3_S0_COPR_GAMMA,
		.copr_frame_count = S6E3HAD_UNBOUND3_A3_S0_COPR_FRAME_COUNT,
		.roi_on = S6E3HAD_UNBOUND3_A3_S0_COPR_ROI_CTRL,
		.roi = {
			[0] = {
				.roi_er = S6E3HAD_UNBOUND3_A3_S0_COPR_ROI1_ER,.roi_eg = S6E3HAD_UNBOUND3_A3_S0_COPR_ROI1_EG,
				.roi_eb = S6E3HAD_UNBOUND3_A3_S0_COPR_ROI1_EB,
				.roi_xs = S6E3HAD_UNBOUND3_A3_S0_COPR_ROI1_X_S, .roi_ys = S6E3HAD_UNBOUND3_A3_S0_COPR_ROI1_Y_S,
				.roi_xe = S6E3HAD_UNBOUND3_A3_S0_COPR_ROI1_X_E, .roi_ye = S6E3HAD_UNBOUND3_A3_S0_COPR_ROI1_Y_E,
			},
			[1] = {
				.roi_er = S6E3HAD_UNBOUND3_A3_S0_COPR_ROI2_ER, .roi_eg = S6E3HAD_UNBOUND3_A3_S0_COPR_ROI2_EG,
				.roi_eb = S6E3HAD_UNBOUND3_A3_S0_COPR_ROI2_EB,
				.roi_xs = S6E3HAD_UNBOUND3_A3_S0_COPR_ROI2_X_S, .roi_ys = S6E3HAD_UNBOUND3_A3_S0_COPR_ROI2_Y_S,
				.roi_xe = S6E3HAD_UNBOUND3_A3_S0_COPR_ROI2_X_E, .roi_ye = S6E3HAD_UNBOUND3_A3_S0_COPR_ROI2_Y_E,
			},
			[2] = {
				.roi_er = S6E3HAD_UNBOUND3_A3_S0_COPR_ROI3_ER, .roi_eg = S6E3HAD_UNBOUND3_A3_S0_COPR_ROI3_EG,
				.roi_eb = S6E3HAD_UNBOUND3_A3_S0_COPR_ROI3_EB,
				.roi_xs = S6E3HAD_UNBOUND3_A3_S0_COPR_ROI3_X_S, .roi_ys = S6E3HAD_UNBOUND3_A3_S0_COPR_ROI3_Y_S,
				.roi_xe = S6E3HAD_UNBOUND3_A3_S0_COPR_ROI3_X_E, .roi_ye = S6E3HAD_UNBOUND3_A3_S0_COPR_ROI3_Y_E,
			},
			[3] = {
				.roi_er = S6E3HAD_UNBOUND3_A3_S0_COPR_ROI4_ER, .roi_eg = S6E3HAD_UNBOUND3_A3_S0_COPR_ROI4_EG,
				.roi_eb = S6E3HAD_UNBOUND3_A3_S0_COPR_ROI4_EB,
				.roi_xs = S6E3HAD_UNBOUND3_A3_S0_COPR_ROI4_X_S, .roi_ys = S6E3HAD_UNBOUND3_A3_S0_COPR_ROI4_Y_S,
				.roi_xe = S6E3HAD_UNBOUND3_A3_S0_COPR_ROI4_X_E, .roi_ye = S6E3HAD_UNBOUND3_A3_S0_COPR_ROI4_Y_E,
			},
			[4] = {
				.roi_er = S6E3HAD_UNBOUND3_A3_S0_COPR_ROI5_ER, .roi_eg = S6E3HAD_UNBOUND3_A3_S0_COPR_ROI5_EG,
				.roi_eb = S6E3HAD_UNBOUND3_A3_S0_COPR_ROI5_EB,
				.roi_xs = S6E3HAD_UNBOUND3_A3_S0_COPR_ROI5_X_S, .roi_ys = S6E3HAD_UNBOUND3_A3_S0_COPR_ROI5_Y_S,
				.roi_xe = S6E3HAD_UNBOUND3_A3_S0_COPR_ROI5_X_E, .roi_ye = S6E3HAD_UNBOUND3_A3_S0_COPR_ROI5_Y_E,
			},
		},
	},
	.nr_roi = 5,
};

#endif /* __S6E3HAD_UNBOUND3_A3_S0_PANEL_COPR_H__ */
