/*
 * linux/drivers/video/fbdev/exynos/panel/ea8082/ea8082.h
 *
 * Header file for EA8082 Dimming Driver
 *
 * Copyright (c) 2016 Samsung Electronics
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#ifndef __EA8082_H__
#define __EA8082_H__

#include <linux/types.h>
#include <linux/kernel.h>
#ifdef CONFIG_SUPPORT_DDI_FLASH
#include "../panel_poc.h"
#endif

/*
 * OFFSET ==> OFS means N-param - 1
 * <example>
 * XXX 1st param => EA8082_XXX_OFS (0)
 * XXX 2nd param => EA8082_XXX_OFS (1)
 * XXX 36th param => EA8082_XXX_OFS (35)
 */

#define AID_INTERPOLATION
#define EA8082_GAMMA_CMD_CNT (35)

#define EA8082_IRC_ONOFF_OFS	(0)
#define EA8082_IRC_VALUE_OFS	(10)
#define EA8082_IRC_VALUE_LEN	(33)

#define EA8082_ADDR_OFS	(0)
#define EA8082_ADDR_LEN	(1)
#define EA8082_DATA_OFS	(EA8082_ADDR_OFS + EA8082_ADDR_LEN)

#define EA8082_MTP_REG				0xC8
#define EA8082_MTP_OFS				0
#define EA8082_MTP_LEN				34

#define EA8082_DATE_REG			0xA1
#define EA8082_DATE_OFS			4
#define EA8082_DATE_LEN			(PANEL_DATE_LEN)

#define EA8082_COORDINATE_REG		0xA1
#define EA8082_COORDINATE_OFS		0
#define EA8082_COORDINATE_LEN		(PANEL_COORD_LEN)

#define EA8082_ID_REG				0xE1
#define EA8082_ID_OFS				0
#define EA8082_ID_LEN				(PANEL_ID_LEN)

#define EA8082_CODE_REG			0xD1
#define EA8082_CODE_OFS			95
#define EA8082_CODE_LEN			(PANEL_CODE_LEN + 1)	// ea8082 code len is 6

#define EA8082_OCTA_ID_REG			0xA1
#define EA8082_OCTA_ID_OFS			11
#define EA8082_OCTA_ID_LEN			(PANEL_OCTA_ID_LEN)

#define EA8082_CHIP_ID_REG			0xD6
#define EA8082_CHIP_ID_OFS			0
#define EA8082_CHIP_ID_LEN			5

/* for panel dump */
#define EA8082_RDDPM_REG			0x0A
#define EA8082_RDDPM_OFS			0
#define EA8082_RDDPM_LEN			(PANEL_RDDPM_LEN)

#define EA8082_RDDSM_REG			0x0E
#define EA8082_RDDSM_OFS			0
#define EA8082_RDDSM_LEN			(PANEL_RDDSM_LEN)

#define EA8082_ERR_REG				0xEA
#define EA8082_ERR_OFS				0
#define EA8082_ERR_LEN				5

#define EA8082_ERR_FG_REG			0xEE
#define EA8082_ERR_FG_OFS			0
#define EA8082_ERR_FG_LEN			1

#define EA8082_DSI_ERR_REG			0x05
#define EA8082_DSI_ERR_OFS			0
#define EA8082_DSI_ERR_LEN			1

#define EA8082_SELF_DIAG_REG			0x0F
#define EA8082_SELF_DIAG_OFS			0
#define EA8082_SELF_DIAG_LEN			1

#ifdef CONFIG_SUPPORT_DDI_CMDLOG
#define EA8082_CMDLOG_REG			0x9C
#define EA8082_CMDLOG_OFS			0
#define EA8082_CMDLOG_LEN			0x80
#endif

#ifdef CONFIG_SUPPORT_CCD_TEST
#define EA8082_CCD_STATE_REG				0xF6
#define EA8082_CCD_STATE_OFS				0x73
#define EA8082_CCD_STATE_LEN				1
#endif

#ifdef CONFIG_SUPPORT_GRAYSPOT_TEST
#define EA8082_GRAYSPOT_CAL_REG			0xB4
#define EA8082_GRAYSPOT_CAL_OFS			0x10
#define EA8082_GRAYSPOT_CAL_LEN			4
#endif


#ifdef CONFIG_EXYNOS_DECON_MDNIE_LITE
#define NR_EA8082_MDNIE_REG	(5)

#define EA8082_MDNIE_0_REG		(0x80)
#define EA8082_MDNIE_0_OFS		(0)
#define EA8082_MDNIE_0_LEN		(7)

#define EA8082_MDNIE_1_REG		(0xB1)
#define EA8082_MDNIE_1_OFS		(EA8082_MDNIE_0_OFS + EA8082_MDNIE_0_LEN)
#define EA8082_MDNIE_1_LEN		(154)

#define EA8082_MDNIE_2_REG		(0xB2)
#define EA8082_MDNIE_2_OFS		(EA8082_MDNIE_1_OFS + EA8082_MDNIE_1_LEN)
#define EA8082_MDNIE_2_LEN		(166)

#define EA8082_MDNIE_3_REG		(0xB0)
#define EA8082_MDNIE_3_OFS		(EA8082_MDNIE_2_OFS + EA8082_MDNIE_2_LEN)
#define EA8082_MDNIE_3_LEN		(2)

#define EA8082_MDNIE_4_REG		(0xB1)
#define EA8082_MDNIE_4_OFS		(EA8082_MDNIE_3_OFS + EA8082_MDNIE_3_LEN)
#define EA8082_MDNIE_4_LEN		(47)
#define EA8082_MDNIE_LEN		(EA8082_MDNIE_0_LEN + EA8082_MDNIE_1_LEN + EA8082_MDNIE_2_LEN + EA8082_MDNIE_3_LEN + EA8082_MDNIE_4_LEN)

#ifdef CONFIG_SUPPORT_AFC
#define EA8082_AFC_REG			(0xE2)
#define EA8082_AFC_OFS			(0)
#define EA8082_AFC_LEN			(70)
#define EA8082_AFC_ROI_OFS		(55)
#define EA8082_AFC_ROI_LEN		(12)
#endif

#define EA8082_SCR_CR_OFS	(1)
#define EA8082_SCR_WR_OFS	(19)
#define EA8082_SCR_WG_OFS	(20)
#define EA8082_SCR_WB_OFS	(21)
#define EA8082_NIGHT_MODE_OFS	(EA8082_SCR_CR_OFS)
#define EA8082_NIGHT_MODE_LEN	(21)
#define EA8082_COLOR_LENS_OFS	(EA8082_SCR_CR_OFS)
#define EA8082_COLOR_LENS_LEN	(21)
#endif /* CONFIG_EXYNOS_DECON_MDNIE_LITE */


enum {
	GAMMA_MAPTBL,
	GAMMA_MODE2_MAPTBL,
	VBIAS_MAPTBL,
	AOR_MAPTBL,
	MPS_MAPTBL,
	TSET_MAPTBL,
	ELVSS_MAPTBL,
	ELVSS_CAL_OFFSET_MAPTBL,
	ELVSS_TEMP_MAPTBL,
	SMOOTH_TRANSITION_FRAME_MAPTBL,
#ifdef CONFIG_SUPPORT_XTALK_MODE
	VGH_MAPTBL,
#endif
	VINT_MAPTBL,
	VINT_VRR_120HZ_MAPTBL,
	ACL_ONOFF_MAPTBL,
	ACL_FRAME_AVG_MAPTBL,
	ACL_START_POINT_MAPTBL,
	ACL_OPR_MAPTBL,
	IRC_MAPTBL,
	IRC_MODE_MAPTBL,
#ifdef CONFIG_SUPPORT_HMD
	/* HMD MAPTBL */
	HMD_GAMMA_MAPTBL,
	HMD_AOR_MAPTBL,
	HMD_ELVSS_MAPTBL,
#endif /* CONFIG_SUPPORT_HMD */
	DSC_MAPTBL,
	PPS_MAPTBL,
	SCALER_MAPTBL,
	CASET_MAPTBL,
	PASET_MAPTBL,
	SSD_IMPROVE_MAPTBL,
	AID_MAPTBL,
	OSC_MAPTBL,
	VFP_FQ_LOW_MAPTBL,
	VFP_FQ_HIGH_MAPTBL,
	OSC_0_MAPTBL,
	OSC_1_MAPTBL,
	LTPS_0_MAPTBL,
	LTPS_1_MAPTBL,
	PWR_GEN_MAPTBL,
	SRC_AMP_MAPTBL,
	AOR_FQ_LOW_MAPTBL,
	AOR_FQ_HIGH_MAPTBL,
	MTP_MAPTBL,
	FPS_MAPTBL,
	TE_FRAME_SEL_MAPTBL,
	LPM_ON_MAPTBL,
	LPM_NIT_MAPTBL,
	LPM_MODE_MAPTBL,
	LPM_DYN_VLIN_MAPTBL,
	LPM_OFF_MAPTBL,
	LPM_AOR_OFF_MAPTBL,
	LPM_WRDISBV_MAPTBL,
#ifdef CONFIG_SUPPORT_GRAYSPOT_TEST
	GRAYSPOT_CAL_MAPTBL,
#endif
#ifdef CONFIG_SUPPORT_GRAM_CHECKSUM
	VDDM_MAPTBL,
	GRAM_IMG_MAPTBL,
	GRAM_INV_IMG_MAPTBL,
#endif
#ifdef CONFIG_DYNAMIC_MIPI
	DM_SET_FFC_MAPTBL,
	DM_DDI_OSC_MAPTBL,
#endif
	POC_ONOFF_MAPTBL,
	GAMMA_INTER_CONTROL_MAPTBL,
	POC_COMP_MAPTBL,
	DBV_MAPTBL,
	HBM_CYCLE_MAPTBL,
	HBM_AND_TRANSITION_MAPTBL,
	DIA_ONOFF_MAPTBL,
	GAMMA_MTP_0_MAPTBL,
	GAMMA_MTP_1_MAPTBL,
	GAMMA_MTP_2_MAPTBL,
	GAMMA_MTP_3_MAPTBL,
	GAMMA_MTP_4_MAPTBL,
#if defined(CONFIG_SEC_FACTORY) && defined(CONFIG_SUPPORT_FAST_DISCHARGE)
	FAST_DISCHARGE_MAPTBL,
#endif
	MAX_MAPTBL,
};

enum {
	READ_ID,
	READ_COORDINATE,
	READ_CODE,
	READ_MTP,
	READ_DATE,
	READ_OCTA_ID,
	READ_CHIP_ID,
	READ_RDDPM,
	READ_RDDSM,
	READ_ERR,
	READ_ERR_FG,
	READ_DSI_ERR,
	READ_SELF_DIAG,
#ifdef CONFIG_SUPPORT_DDI_CMDLOG
	READ_CMDLOG,
#endif
#ifdef CONFIG_SUPPORT_DDI_CMDLOG
	READ_CMDLOG,
#endif
#ifdef CONFIG_SUPPORT_CCD_TEST
	READ_CCD_STATE,
#endif
#ifdef CONFIG_SUPPORT_GRAYSPOT_TEST
	READ_GRAYSPOT_CAL,
#endif
};

enum {
	RES_ID,
	RES_COORDINATE,
	RES_CODE,
	RES_MTP,
	RES_DATE,
	RES_OCTA_ID,
	RES_CHIP_ID,
	RES_RDDPM,
	RES_RDDSM,
	RES_ERR,
	RES_ERR_FG,
	RES_DSI_ERR,
	RES_SELF_DIAG,
#ifdef CONFIG_SUPPORT_DDI_CMDLOG
	RES_CMDLOG,
#endif
#ifdef CONFIG_SUPPORT_GRAYSPOT_TEST
	RES_GRAYSPOT_CAL,
#endif
#ifdef CONFIG_SUPPORT_CCD_TEST
	RES_CCD_STATE,
	RES_CCD_CHKSUM_FAIL,
#endif
};

static u8 EA8082_ID[EA8082_ID_LEN];
static u8 EA8082_COORDINATE[EA8082_COORDINATE_LEN];
static u8 EA8082_CODE[EA8082_CODE_LEN];
static u8 EA8082_MTP[EA8082_MTP_LEN];
static u8 EA8082_DATE[EA8082_DATE_LEN];
static u8 EA8082_OCTA_ID[EA8082_OCTA_ID_LEN];

static u8 EA8082_CHIP_ID[EA8082_CHIP_ID_LEN];
static u8 EA8082_RDDPM[EA8082_RDDPM_LEN];
static u8 EA8082_RDDSM[EA8082_RDDSM_LEN];
static u8 EA8082_ERR[EA8082_ERR_LEN];
static u8 EA8082_ERR_FG[EA8082_ERR_FG_LEN];
static u8 EA8082_DSI_ERR[EA8082_DSI_ERR_LEN];
static u8 EA8082_SELF_DIAG[EA8082_SELF_DIAG_LEN];
#ifdef CONFIG_SUPPORT_DDI_CMDLOG
static u8 EA8082_CMDLOG[EA8082_CMDLOG_LEN];
#endif

#ifdef CONFIG_SUPPORT_CCD_TEST
static u8 EA8082_CCD_STATE[EA8082_CCD_STATE_LEN];
static u8 EA8082_CCD_CHKSUM_FAIL[EA8082_CCD_STATE_LEN] = { 0x80 };
#endif
#ifdef CONFIG_SUPPORT_GRAYSPOT_TEST
static u8 EA8082_GRAYSPOT_CAL[EA8082_GRAYSPOT_CAL_LEN];
#endif

static struct rdinfo ea8082_rditbl[] = {
	[READ_ID] = RDINFO_INIT(id, DSI_PKT_TYPE_RD, EA8082_ID_REG, EA8082_ID_OFS, EA8082_ID_LEN),
	[READ_COORDINATE] = RDINFO_INIT(coordinate, DSI_PKT_TYPE_RD, EA8082_COORDINATE_REG, EA8082_COORDINATE_OFS, EA8082_COORDINATE_LEN),
	[READ_CODE] = RDINFO_INIT(code, DSI_PKT_TYPE_RD, EA8082_CODE_REG, EA8082_CODE_OFS, EA8082_CODE_LEN),
	[READ_MTP] = RDINFO_INIT(mtp, DSI_PKT_TYPE_RD, EA8082_MTP_REG, EA8082_MTP_OFS, EA8082_MTP_LEN),
	[READ_DATE] = RDINFO_INIT(date, DSI_PKT_TYPE_RD, EA8082_DATE_REG, EA8082_DATE_OFS, EA8082_DATE_LEN),
	[READ_OCTA_ID] = RDINFO_INIT(octa_id, DSI_PKT_TYPE_RD, EA8082_OCTA_ID_REG, EA8082_OCTA_ID_OFS, EA8082_OCTA_ID_LEN),
	/* for brightness debugging */
	[READ_CHIP_ID] = RDINFO_INIT(chip_id, DSI_PKT_TYPE_RD, EA8082_CHIP_ID_REG, EA8082_CHIP_ID_OFS, EA8082_CHIP_ID_LEN),
	[READ_RDDPM] = RDINFO_INIT(rddpm, DSI_PKT_TYPE_RD, EA8082_RDDPM_REG, EA8082_RDDPM_OFS, EA8082_RDDPM_LEN),
	[READ_RDDSM] = RDINFO_INIT(rddsm, DSI_PKT_TYPE_RD, EA8082_RDDSM_REG, EA8082_RDDSM_OFS, EA8082_RDDSM_LEN),
	[READ_ERR] = RDINFO_INIT(err, DSI_PKT_TYPE_RD, EA8082_ERR_REG, EA8082_ERR_OFS, EA8082_ERR_LEN),
	[READ_ERR_FG] = RDINFO_INIT(err_fg, DSI_PKT_TYPE_RD, EA8082_ERR_FG_REG, EA8082_ERR_FG_OFS, EA8082_ERR_FG_LEN),
	[READ_DSI_ERR] = RDINFO_INIT(dsi_err, DSI_PKT_TYPE_RD, EA8082_DSI_ERR_REG, EA8082_DSI_ERR_OFS, EA8082_DSI_ERR_LEN),
	[READ_SELF_DIAG] = RDINFO_INIT(self_diag, DSI_PKT_TYPE_RD, EA8082_SELF_DIAG_REG, EA8082_SELF_DIAG_OFS, EA8082_SELF_DIAG_LEN),
#ifdef CONFIG_SUPPORT_DDI_CMDLOG
	[READ_CMDLOG] = RDINFO_INIT(cmdlog, DSI_PKT_TYPE_RD, EA8082_CMDLOG_REG, EA8082_CMDLOG_OFS, EA8082_CMDLOG_LEN),
#endif
#ifdef CONFIG_SUPPORT_CCD_TEST
	[READ_CCD_STATE] = RDINFO_INIT(ccd_state, DSI_PKT_TYPE_RD, EA8082_CCD_STATE_REG, EA8082_CCD_STATE_OFS, EA8082_CCD_STATE_LEN),
#endif
#ifdef CONFIG_SUPPORT_GRAYSPOT_TEST
	[READ_GRAYSPOT_CAL] = RDINFO_INIT(grayspot_cal, DSI_PKT_TYPE_RD, EA8082_GRAYSPOT_CAL_REG, EA8082_GRAYSPOT_CAL_OFS, EA8082_GRAYSPOT_CAL_LEN),
#endif
};

static DEFINE_RESUI(id, &ea8082_rditbl[READ_ID], 0);
static DEFINE_RESUI(coordinate, &ea8082_rditbl[READ_COORDINATE], 0);
static DEFINE_RESUI(code, &ea8082_rditbl[READ_CODE], 0);
static DEFINE_RESUI(mtp, &ea8082_rditbl[READ_MTP], 0);
static DEFINE_RESUI(date, &ea8082_rditbl[READ_DATE], 0);
static DEFINE_RESUI(octa_id, &ea8082_rditbl[READ_OCTA_ID], 0);

static DEFINE_RESUI(chip_id, &ea8082_rditbl[READ_CHIP_ID], 0);
static DEFINE_RESUI(rddpm, &ea8082_rditbl[READ_RDDPM], 0);
static DEFINE_RESUI(rddsm, &ea8082_rditbl[READ_RDDSM], 0);
static DEFINE_RESUI(err, &ea8082_rditbl[READ_ERR], 0);
static DEFINE_RESUI(err_fg, &ea8082_rditbl[READ_ERR_FG], 0);
static DEFINE_RESUI(dsi_err, &ea8082_rditbl[READ_DSI_ERR], 0);
static DEFINE_RESUI(self_diag, &ea8082_rditbl[READ_SELF_DIAG], 0);

#ifdef CONFIG_SUPPORT_DDI_CMDLOG
static DEFINE_RESUI(cmdlog, &ea8082_rditbl[READ_CMDLOG], 0);
#endif

#ifdef CONFIG_SUPPORT_CCD_TEST
static DEFINE_RESUI(ccd_state, &ea8082_rditbl[READ_CCD_STATE], 0);
#endif
#ifdef CONFIG_SUPPORT_GRAYSPOT_TEST
static DEFINE_RESUI(grayspot_cal, &ea8082_rditbl[READ_GRAYSPOT_CAL], 0);
#endif

static struct resinfo ea8082_restbl[] = {
	[RES_ID] = RESINFO_INIT(id, EA8082_ID, RESUI(id)),
	[RES_COORDINATE] = RESINFO_INIT(coordinate, EA8082_COORDINATE, RESUI(coordinate)),
	[RES_CODE] = RESINFO_INIT(code, EA8082_CODE, RESUI(code)),
	[RES_MTP] = RESINFO_INIT(mtp, EA8082_MTP, RESUI(mtp)),
	[RES_DATE] = RESINFO_INIT(date, EA8082_DATE, RESUI(date)),
	[RES_OCTA_ID] = RESINFO_INIT(octa_id, EA8082_OCTA_ID, RESUI(octa_id)),
	[RES_CHIP_ID] = RESINFO_INIT(chip_id, EA8082_CHIP_ID, RESUI(chip_id)),
	[RES_RDDPM] = RESINFO_INIT(rddpm, EA8082_RDDPM, RESUI(rddpm)),
	[RES_RDDSM] = RESINFO_INIT(rddsm, EA8082_RDDSM, RESUI(rddsm)),
	[RES_ERR] = RESINFO_INIT(err, EA8082_ERR, RESUI(err)),
	[RES_ERR_FG] = RESINFO_INIT(err_fg, EA8082_ERR_FG, RESUI(err_fg)),
	[RES_DSI_ERR] = RESINFO_INIT(dsi_err, EA8082_DSI_ERR, RESUI(dsi_err)),
	[RES_SELF_DIAG] = RESINFO_INIT(self_diag, EA8082_SELF_DIAG, RESUI(self_diag)),
#ifdef CONFIG_SUPPORT_DDI_CMDLOG
	[RES_CMDLOG] = RESINFO_INIT(cmdlog, EA8082_CMDLOG, RESUI(cmdlog)),
#endif
#ifdef CONFIG_SUPPORT_CCD_TEST
	[RES_CCD_STATE] = RESINFO_INIT(ccd_state, EA8082_CCD_STATE, RESUI(ccd_state)),
	[RES_CCD_CHKSUM_FAIL] = {
		.name = "ccd_chksum_fail",
		.type = CMD_TYPE_RES,
		.state = RES_UNINITIALIZED,
		.data = EA8082_CCD_CHKSUM_FAIL,
		.dlen = ARRAY_SIZE(EA8082_CCD_CHKSUM_FAIL),
		.resui = NULL,
		.nr_resui = 0,
	},
#endif
#ifdef CONFIG_SUPPORT_GRAYSPOT_TEST
	[RES_GRAYSPOT_CAL] = RESINFO_INIT(grayspot_cal, EA8082_GRAYSPOT_CAL, RESUI(grayspot_cal)),
#endif
};

enum {
	DUMP_RDDPM = 0,
	DUMP_RDDPM_SLEEP_IN,
	DUMP_RDDSM,
	DUMP_DSI_ERR,
	DUMP_SELF_DIAG,
	DUMP_SELF_MASK_CRC,
#ifdef CONFIG_SUPPORT_DDI_CMDLOG
	DUMP_CMDLOG,
#endif
};

static void show_rddpm(struct dumpinfo *info);
static void show_rddpm_before_sleep_in(struct dumpinfo *info);
static void show_rddsm(struct dumpinfo *info);
static void show_dsi_err(struct dumpinfo *info);
static void show_self_diag(struct dumpinfo *info);
#ifdef CONFIG_SUPPORT_DDI_CMDLOG
static void show_cmdlog(struct dumpinfo *info);
#endif

static struct dumpinfo ea8082_dmptbl[] = {
	[DUMP_RDDPM] = DUMPINFO_INIT(rddpm, &ea8082_restbl[RES_RDDPM], show_rddpm),
	[DUMP_RDDPM_SLEEP_IN] = DUMPINFO_INIT(rddpm_sleep_in, &ea8082_restbl[RES_RDDPM], show_rddpm_before_sleep_in),
	[DUMP_RDDSM] = DUMPINFO_INIT(rddsm, &ea8082_restbl[RES_RDDSM], show_rddsm),
	[DUMP_DSI_ERR] = DUMPINFO_INIT(dsi_err, &ea8082_restbl[RES_DSI_ERR], show_dsi_err),
	[DUMP_SELF_DIAG] = DUMPINFO_INIT(self_diag, &ea8082_restbl[RES_SELF_DIAG], show_self_diag),
#ifdef CONFIG_SUPPORT_DDI_CMDLOG
	[DUMP_CMDLOG] = DUMPINFO_INIT(cmdlog, &ea8082_restbl[RES_CMDLOG], show_cmdlog),
#endif
};

/* use according to adaptive_control sysfs */
enum {
	EA8082_ACL_OPR_0,
	EA8082_ACL_OPR_1,
	EA8082_ACL_OPR_2,
	MAX_EA8082_ACL_OPR
};

/* Variable Refresh Rate */
enum {
	EA8082_VRR_MODE_NS,
	EA8082_VRR_MODE_HS,
	MAX_EA8082_VRR_MODE,
};

enum {
	EA8082_VRR_120HS,
	EA8082_VRR_60HS,
	MAX_EA8082_VRR,
};

enum {
	EA8082_RESOL_1080x2340,
};

enum {
	EA8082_DISPLAY_MODE_1080x2340_120HS,
	EA8082_DISPLAY_MODE_1080x2340_60HS,
	MAX_EA8082_DISPLAY_MODE,
};

enum {
	EA8082_VRR_KEY_REFRESH_RATE,
	EA8082_VRR_KEY_REFRESH_MODE,
	EA8082_VRR_KEY_TE_SW_SKIP_COUNT,
	EA8082_VRR_KEY_TE_HW_SKIP_COUNT,
	MAX_EA8082_VRR_KEY,
};

static u32 EA8082_VRR_FPS[MAX_EA8082_VRR][MAX_EA8082_VRR_KEY] = {
	[EA8082_VRR_120HS] = { 120, VRR_HS_MODE, 0, 0 },
	[EA8082_VRR_60HS] = { 60, VRR_HS_MODE, 0, 0 },
};

#ifdef CONFIG_DYNAMIC_MIPI

#define MCD_DM_DDI_OSC_96_5 0x00
#define MCD_DM_DDI_OSC_94_5 0x10
#define MCD_DM_DDI_OSC_92_0 0x20

static void dynamic_mipi_set_ffc(struct maptbl *tbl, u8 *dst);
void dynamic_mipi_set_ddi_osc(struct maptbl *tbl, u8 *dst);
#endif

static int init_common_table(struct maptbl *);
static int getidx_common_maptbl(struct maptbl *);
static int init_gamma_mode2_brt_table(struct maptbl *tbl);
static int getidx_gamma_mode2_brt_table(struct maptbl *);
static int getidx_hbm_and_transition_table(struct maptbl *tbl);
static int getidx_dia_onoff_table(struct maptbl *tbl);
static int getidx_acl_opr_table(struct maptbl *);

static int init_lpm_brt_table(struct maptbl *tbl);
static int getidx_lpm_brt_table(struct maptbl *);

#if defined(CONFIG_SEC_FACTORY) && defined(CONFIG_SUPPORT_FAST_DISCHARGE)
static int getidx_fast_discharge_table(struct maptbl *tbl);
#endif
#ifdef CONFIG_SUPPORT_XTALK_MODE
static int getidx_vgh_table(struct maptbl *tbl);
#endif

#ifdef CONFIG_EXYNOS_DECON_MDNIE_LITE
static void copy_dummy_maptbl(struct maptbl *tbl, u8 *dst);
#endif
static void copy_common_maptbl(struct maptbl *, u8 *);
static void copy_tset_maptbl(struct maptbl *tbl, u8 *);
#ifdef CONFIG_SUPPORT_GRAYSPOT_TEST
static void copy_grayspot_cal_maptbl(struct maptbl *tbl, u8 *dst);
#endif
static int getidx_vrr_table(struct maptbl *tbl);

#ifdef CONFIG_EXYNOS_DECON_MDNIE_LITE
static int init_color_blind_table(struct maptbl *tbl);
static int getidx_mdnie_scenario_maptbl(struct maptbl *tbl);
static int getidx_mdnie_hdr_maptbl(struct maptbl *tbl);
static int init_mdnie_night_mode_table(struct maptbl *tbl);
static int getidx_mdnie_night_mode_maptbl(struct maptbl *tbl);
static int init_mdnie_color_lens_table(struct maptbl *tbl);
static int getidx_color_lens_maptbl(struct maptbl *tbl);
static int init_color_coordinate_table(struct maptbl *tbl);
static int init_sensor_rgb_table(struct maptbl *tbl);
static int getidx_adjust_ldu_maptbl(struct maptbl *tbl);
static int getidx_color_coordinate_maptbl(struct maptbl *tbl);
static void copy_color_coordinate_maptbl(struct maptbl *tbl, u8 *dst);
static void copy_scr_white_maptbl(struct maptbl *tbl, u8 *dst);
static void copy_adjust_ldu_maptbl(struct maptbl *tbl, u8 *dst);
static int getidx_mdnie_0_maptbl(struct pkt_update_info *pktui);
static int getidx_mdnie_1_maptbl(struct pkt_update_info *pktui);
static int getidx_mdnie_2_maptbl(struct pkt_update_info *pktui);
static int getidx_mdnie_3_maptbl(struct pkt_update_info *pktui);
static int getidx_mdnie_4_maptbl(struct pkt_update_info *pktui);

static int getidx_mdnie_scr_white_maptbl(struct pkt_update_info *pktui);
static void update_current_scr_white(struct maptbl *tbl, u8 *dst);
#endif /* CONFIG_EXYNOS_DECON_MDNIE_LITE */


static bool is_panel_state_not_lpm(struct panel_device *panel);
static inline bool is_id_gte_03(struct panel_device *panel);
static bool is_first_set_bl(struct panel_device *panel);
static bool is_wait_vsync_needed(struct panel_device *panel);

#endif /* __EA8082_H__ */