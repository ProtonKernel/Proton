/*
 * linux/drivers/video/fbdev/exynos/panel/s6e3fc3/s6e3fc3.h
 *
 * Header file for S6E3FC3 Dimming Driver
 *
 * Copyright (c) 2016 Samsung Electronics
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#ifndef __S6E3FC3_H__
#define __S6E3FC3_H__

#include <linux/types.h>
#include <linux/kernel.h>
#ifdef CONFIG_SUPPORT_DDI_FLASH
#include "../panel_poc.h"
#endif

/*
 * OFFSET ==> OFS means N-param - 1
 * <example>
 * XXX 1st param => S6E3FC3_XXX_OFS (0)
 * XXX 2nd param => S6E3FC3_XXX_OFS (1)
 * XXX 36th param => S6E3FC3_XXX_OFS (35)
 */

#define S6E3FC3_ADDR_OFS	(0)
#define S6E3FC3_ADDR_LEN	(1)
#define S6E3FC3_DATA_OFS	(S6E3FC3_ADDR_OFS + S6E3FC3_ADDR_LEN)

#define S6E3FC3_DATE_REG			0xA1
#define S6E3FC3_DATE_OFS			4
#define S6E3FC3_DATE_LEN			(PANEL_DATE_LEN)

#define S6E3FC3_COORDINATE_REG		0xA1
#define S6E3FC3_COORDINATE_OFS		0
#define S6E3FC3_COORDINATE_LEN		(PANEL_COORD_LEN)

#define S6E3FC3_ID_REG				0x04
#define S6E3FC3_ID_OFS				0
#define S6E3FC3_ID_LEN				(PANEL_ID_LEN)

#define S6E3FC3_CODE_REG			0xD6
#define S6E3FC3_CODE_OFS			0
#define S6E3FC3_CODE_LEN			5

#define S6E3FC3_OCTA_ID_REG			0xA1
#define S6E3FC3_OCTA_ID_OFS			11
#define S6E3FC3_OCTA_ID_LEN			20

#if 0
#define S6E3FC3_OCTA_ID_1_REG			0x92
#define S6E3FC3_OCTA_ID_1_OFS			2
#define S6E3FC3_OCTA_ID_1_LEN			16
#define S6E3FC3_OCTA_ID_LEN			(S6E3FC3_OCTA_ID_0_LEN + S6E3FC3_OCTA_ID_1_LEN)
#endif

/* for panel dump */
#define S6E3FC3_RDDPM_REG			0x0A
#define S6E3FC3_RDDPM_OFS			0
#define S6E3FC3_RDDPM_LEN			(PANEL_RDDPM_LEN)

#define S6E3FC3_RDDSM_REG			0x0E
#define S6E3FC3_RDDSM_OFS			0
#define S6E3FC3_RDDSM_LEN			(PANEL_RDDSM_LEN)

#define S6E3FC3_ERR_REG				0xE9
#define S6E3FC3_ERR_OFS				0
#define S6E3FC3_ERR_LEN				5

#define S6E3FC3_ERR_FG_REG			0xEE
#define S6E3FC3_ERR_FG_OFS			0
#define S6E3FC3_ERR_FG_LEN			1

#define S6E3FC3_DSI_ERR_REG			0x05
#define S6E3FC3_DSI_ERR_OFS			0
#define S6E3FC3_DSI_ERR_LEN			1

#define S6E3FC3_SELF_DIAG_REG			0x0F
#define S6E3FC3_SELF_DIAG_OFS			0
#define S6E3FC3_SELF_DIAG_LEN			1

#define S6E3FC3_SELF_MASK_CHECKSUM_REG		0xFB
#define S6E3FC3_SELF_MASK_CHECKSUM_OFS		15
#define S6E3FC3_SELF_MASK_CHECKSUM_LEN		2

#define S6E3FC3_SELF_MASK_CRC_REG		0x7F
#define S6E3FC3_SELF_MASK_CRC_OFS	6
#define S6E3FC3_SELF_MASK_CRC_LEN		4

#ifdef CONFIG_SUPPORT_GRAYSPOT_TEST
#define S6E3FC3_GRAYSPOT_CAL_REG			0x63
#define S6E3FC3_GRAYSPOT_CAL_OFS			0
#define S6E3FC3_GRAYSPOT_CAL_LEN			3
#endif

#ifdef CONFIG_SUPPORT_CCD_TEST
#define S6E3FC3_CCD_STATE_REG				0xCD
#define S6E3FC3_CCD_STATE_OFS				0x01
#define S6E3FC3_CCD_STATE_LEN				1
#endif

#ifdef CONFIG_EXYNOS_DECON_MDNIE_LITE
#define NR_S6E3FC3_MDNIE_REG	(4)

#define S6E3FC3_MDNIE_0_REG		(0x5D)
#define S6E3FC3_MDNIE_0_OFS		(0)
#define S6E3FC3_MDNIE_0_LEN		(7)

#define S6E3FC3_MDNIE_1_REG		(0x62)
#define S6E3FC3_MDNIE_1_OFS		(S6E3FC3_MDNIE_0_OFS + S6E3FC3_MDNIE_0_LEN)
#define S6E3FC3_MDNIE_1_LEN		(25)

#define S6E3FC3_MDNIE_2_REG		(0xB0)
#define S6E3FC3_MDNIE_2_OFS		(S6E3FC3_MDNIE_1_OFS + S6E3FC3_MDNIE_1_LEN)
#define S6E3FC3_MDNIE_2_LEN		(3)

#define S6E3FC3_MDNIE_3_REG		(0x62)
#define S6E3FC3_MDNIE_3_OFS		(S6E3FC3_MDNIE_2_OFS + S6E3FC3_MDNIE_2_LEN)
#define S6E3FC3_MDNIE_3_LEN		(218)
#define S6E3FC3_MDNIE_LEN		(S6E3FC3_MDNIE_0_LEN + S6E3FC3_MDNIE_1_LEN + S6E3FC3_MDNIE_2_LEN + S6E3FC3_MDNIE_3_LEN )

#ifdef CONFIG_SUPPORT_AFC
#define S6E3FC3_AFC_REG			(0xE2)
#define S6E3FC3_AFC_OFS			(0)
#define S6E3FC3_AFC_LEN			(70)
#define S6E3FC3_AFC_ROI_OFS		(55)
#define S6E3FC3_AFC_ROI_LEN		(12)
#endif

#define S6E3FC3_SCR_CR_OFS	(1)
#define S6E3FC3_SCR_WR_OFS	(19)
#define S6E3FC3_SCR_WG_OFS	(20)
#define S6E3FC3_SCR_WB_OFS	(21)
#define S6E3FC3_NIGHT_MODE_OFS	(S6E3FC3_SCR_CR_OFS)
#define S6E3FC3_NIGHT_MODE_LEN	(21)
#define S6E3FC3_COLOR_LENS_OFS	(S6E3FC3_SCR_CR_OFS)
#define S6E3FC3_COLOR_LENS_LEN	(21)
#endif /* CONFIG_EXYNOS_DECON_MDNIE_LITE */


#ifdef CONFIG_DYNAMIC_FREQ
#define S6E3FC3_MAX_MIPI_FREQ			3
#define S6E3FC3_DEFAULT_MIPI_FREQ		0
enum {
	S6E3FC3_OSC_DEFAULT,
	MAX_S6E3FC3_OSC,
};
#endif


enum {
	GAMMA_MAPTBL,
	GAMMA_MODE2_MAPTBL,
	VBIAS_MAPTBL,
	AOR_MAPTBL,
	MPS_MAPTBL,
	TSET_MAPTBL,
	ELVSS_MAPTBL,
	ELVSS_OTP_MAPTBL,
	ELVSS_TEMP_MAPTBL,
#ifdef CONFIG_SUPPORT_XTALK_MODE
	VGH_MAPTBL,
#endif
	VINT_MAPTBL,
	VINT_VRR_120HZ_MAPTBL,
	ACL_ONOFF_MAPTBL,
	ACL_FRAME_AVG_MAPTBL,
	ACL_START_POINT_MAPTBL,
	ACL_DIM_SPEED_MAPTBL,
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
	VFP_NM_MAPTBL,
	VFP_HS_MAPTBL,
	PWR_GEN_MAPTBL,
	SRC_AMP_MAPTBL,
	MTP_MAPTBL,
	FPS_MAPTBL_1,
	DIMMING_SPEED,
	FPS_FREQ_MAPTBL,
	LPM_NIT_MAPTBL,
	LPM_MODE_MAPTBL,
	LPM_DYN_VLIN_MAPTBL,
	LPM_ON_MAPTBL,
	LPM_AOR_OFF_MAPTBL,
#ifdef CONFIG_SUPPORT_DDI_FLASH
	POC_ON_MAPTBL,
	POC_WR_ADDR_MAPTBL,
	POC_RD_ADDR_MAPTBL,
	POC_WR_DATA_MAPTBL,
#endif
#ifdef CONFIG_SUPPORT_POC_FLASH
	POC_ER_ADDR_MAPTBL,
#endif
#ifdef CONFIG_SUPPORT_POC_SPI
	POC_SPI_READ_ADDR_MAPTBL,
	POC_SPI_WRITE_ADDR_MAPTBL,
	POC_SPI_WRITE_DATA_MAPTBL,
	POC_SPI_ERASE_ADDR_MAPTBL,
#endif
	POC_ONOFF_MAPTBL,
#ifdef CONFIG_SUPPORT_TDMB_TUNE
	TDMB_TUNE_MAPTBL,
#endif
#ifdef CONFIG_DYNAMIC_FREQ
	DYN_FFC_1_MAPTBL,
	DYN_FFC_2_MAPTBL,
#endif

#ifdef CONFIG_SUPPORT_GRAYSPOT_TEST
	GRAYSPOT_CAL_MAPTBL,
#endif
	GAMMA_INTER_CONTROL_MAPTBL,
	POC_COMP_MAPTBL,
	DBV_MAPTBL,
	HBM_CYCLE_MAPTBL,
	HBM_ONOFF_MAPTBL,
	DIA_ONOFF_MAPTBL,
#if defined(CONFIG_SEC_FACTORY) && defined(CONFIG_SUPPORT_FAST_DISCHARGE)
	FAST_DISCHARGE_MAPTBL,
#endif
	MAX_MAPTBL,
};

enum {
#ifdef CONFIG_EXYNOS_DECON_LCD_COPR
	READ_COPR_SPI,
	READ_COPR_DSI,
#endif
	READ_ID,
	READ_COORDINATE,
	READ_CODE,
	READ_ELVSS,
	READ_ELVSS_TEMP_0,
	READ_ELVSS_TEMP_1,
	READ_MTP,
	READ_DATE,
	READ_OCTA_ID,
	READ_CHIP_ID,
	/* for brightness debugging */
	READ_AOR,
	READ_VINT,
	READ_ELVSS_T,
	READ_IRC,

	READ_RDDPM,
	READ_RDDSM,
	READ_ERR,
	READ_ERR_FG,
	READ_DSI_ERR,
	READ_SELF_DIAG,
	READ_SELF_MASK_CHECKSUM,
	READ_SELF_MASK_CRC,
#ifdef CONFIG_SUPPORT_POC_SPI
	READ_POC_SPI_READ,
	READ_POC_SPI_STATUS1,
	READ_POC_SPI_STATUS2,
#endif
#ifdef CONFIG_SUPPORT_POC_FLASH
	READ_POC_MCA_CHKSUM,
#endif
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
	MAX_READTBL,
};

enum {
#ifdef CONFIG_EXYNOS_DECON_LCD_COPR
	RES_COPR_SPI,
	RES_COPR_DSI,
#endif
	RES_ID,
	RES_COORDINATE,
	RES_CODE,
	RES_ELVSS,
	RES_ELVSS_TEMP_0,
	RES_ELVSS_TEMP_1,
	RES_MTP,
	RES_DATE,
	RES_OCTA_ID,
	RES_CHIP_ID,
	/* for brightness debugging */
	RES_AOR,
	RES_VINT,
	RES_ELVSS_T,
	RES_IRC,
	RES_RDDPM,
	RES_RDDSM,
	RES_ERR,
	RES_ERR_FG,
	RES_DSI_ERR,
	RES_SELF_DIAG,
#ifdef CONFIG_SUPPORT_DDI_CMDLOG
	RES_CMDLOG,
#endif
#ifdef CONFIG_SUPPORT_POC_SPI
	RES_POC_SPI_READ,
	RES_POC_SPI_STATUS1,
	RES_POC_SPI_STATUS2,
#endif
#ifdef CONFIG_SUPPORT_POC_FLASH
	RES_POC_MCA_CHKSUM,
#endif
#ifdef CONFIG_SUPPORT_CCD_TEST
	RES_CCD_STATE,
	RES_CCD_CHKSUM_FAIL,
#endif
#ifdef CONFIG_SUPPORT_GRAYSPOT_TEST
	RES_GRAYSPOT_CAL,
#endif
	RES_SELF_MASK_CHECKSUM,
	RES_SELF_MASK_CRC,
	MAX_RESTBL
};

enum {
	HLPM_ON_LOW,
	HLPM_ON,
	MAX_HLPM_ON
};


enum {
	S6E3FC3_VRR_FPS_120,
	S6E3FC3_VRR_FPS_60,
	MAX_S6E3FC3_VRR_FPS,
};


enum {
	S6E3FC3_SMOOTH_DIMMING_OFF,
	S6E3FC3_SMOOTH_DIMMING_ON,
	MAX_S6E3FC3_SMOOTH_DIMMING,
};

/* use according to adaptive_control sysfs */
enum {
	S6E3FC3_ACL_OPR_0,
	S6E3FC3_ACL_OPR_1,
	S6E3FC3_ACL_OPR_2,
	MAX_S6E3FC3_ACL_OPR
};

static u8 S6E3FC3_ID[S6E3FC3_ID_LEN];
static u8 S6E3FC3_COORDINATE[S6E3FC3_COORDINATE_LEN];
static u8 S6E3FC3_CODE[S6E3FC3_CODE_LEN];
static u8 S6E3FC3_DATE[S6E3FC3_DATE_LEN];
static u8 S6E3FC3_OCTA_ID[S6E3FC3_OCTA_ID_LEN];
/* for brightness debugging */
static u8 S6E3FC3_RDDPM[S6E3FC3_RDDPM_LEN];
static u8 S6E3FC3_RDDSM[S6E3FC3_RDDSM_LEN];
static u8 S6E3FC3_ERR[S6E3FC3_ERR_LEN];
static u8 S6E3FC3_ERR_FG[S6E3FC3_ERR_FG_LEN];
static u8 S6E3FC3_DSI_ERR[S6E3FC3_DSI_ERR_LEN];
static u8 S6E3FC3_SELF_DIAG[S6E3FC3_SELF_DIAG_LEN];

#ifdef CONFIG_SUPPORT_GRAYSPOT_TEST
static u8 S6E3FC3_GRAYSPOT_CAL[S6E3FC3_GRAYSPOT_CAL_LEN];
#endif
#ifdef CONFIG_SUPPORT_CCD_TEST
static u8 S6E3FC3_CCD_STATE[S6E3FC3_CCD_STATE_LEN];
static u8 S6E3FC3_CCD_CHKSUM_FAIL[S6E3FC3_CCD_STATE_LEN] = { 0x40 };
#endif

static struct rdinfo s6e3fc3_rditbl[MAX_READTBL] = {
	[READ_ID] = RDINFO_INIT(id, DSI_PKT_TYPE_RD, S6E3FC3_ID_REG, S6E3FC3_ID_OFS, S6E3FC3_ID_LEN),
	[READ_COORDINATE] = RDINFO_INIT(coordinate, DSI_PKT_TYPE_RD, S6E3FC3_COORDINATE_REG, S6E3FC3_COORDINATE_OFS, S6E3FC3_COORDINATE_LEN),
	[READ_CODE] = RDINFO_INIT(code, DSI_PKT_TYPE_RD, S6E3FC3_CODE_REG, S6E3FC3_CODE_OFS, S6E3FC3_CODE_LEN),
	[READ_DATE] = RDINFO_INIT(date, DSI_PKT_TYPE_RD, S6E3FC3_DATE_REG, S6E3FC3_DATE_OFS, S6E3FC3_DATE_LEN),
	[READ_OCTA_ID] = RDINFO_INIT(octa_id, DSI_PKT_TYPE_RD, S6E3FC3_OCTA_ID_REG, S6E3FC3_OCTA_ID_OFS, S6E3FC3_OCTA_ID_LEN),
	[READ_RDDPM] = RDINFO_INIT(rddpm, DSI_PKT_TYPE_RD, S6E3FC3_RDDPM_REG, S6E3FC3_RDDPM_OFS, S6E3FC3_RDDPM_LEN),
	[READ_RDDSM] = RDINFO_INIT(rddsm, DSI_PKT_TYPE_RD, S6E3FC3_RDDSM_REG, S6E3FC3_RDDSM_OFS, S6E3FC3_RDDSM_LEN),
	[READ_ERR] = RDINFO_INIT(err, DSI_PKT_TYPE_RD, S6E3FC3_ERR_REG, S6E3FC3_ERR_OFS, S6E3FC3_ERR_LEN),
	[READ_ERR_FG] = RDINFO_INIT(err_fg, DSI_PKT_TYPE_RD, S6E3FC3_ERR_FG_REG, S6E3FC3_ERR_FG_OFS, S6E3FC3_ERR_FG_LEN),
	[READ_DSI_ERR] = RDINFO_INIT(dsi_err, DSI_PKT_TYPE_RD, S6E3FC3_DSI_ERR_REG, S6E3FC3_DSI_ERR_OFS, S6E3FC3_DSI_ERR_LEN),
	[READ_SELF_DIAG] = RDINFO_INIT(self_diag, DSI_PKT_TYPE_RD, S6E3FC3_SELF_DIAG_REG, S6E3FC3_SELF_DIAG_OFS, S6E3FC3_SELF_DIAG_LEN),
#ifdef CONFIG_SUPPORT_GRAYSPOT_TEST
	[READ_GRAYSPOT_CAL] = RDINFO_INIT(grayspot_cal, DSI_PKT_TYPE_RD, S6E3FC3_GRAYSPOT_CAL_REG, S6E3FC3_GRAYSPOT_CAL_OFS, S6E3FC3_GRAYSPOT_CAL_LEN),
#endif
#ifdef CONFIG_SUPPORT_CCD_TEST
	[READ_CCD_STATE] = RDINFO_INIT(ccd_state, DSI_PKT_TYPE_RD, S6E3FC3_CCD_STATE_REG, S6E3FC3_CCD_STATE_OFS, S6E3FC3_CCD_STATE_LEN),
#endif
};

static DEFINE_RESUI(id, &s6e3fc3_rditbl[READ_ID], 0);
static DEFINE_RESUI(coordinate, &s6e3fc3_rditbl[READ_COORDINATE], 0);
static DEFINE_RESUI(code, &s6e3fc3_rditbl[READ_CODE], 0);
static DEFINE_RESUI(elvss, &s6e3fc3_rditbl[READ_ELVSS], 0);
static DEFINE_RESUI(mtp, &s6e3fc3_rditbl[READ_MTP], 0);
static DEFINE_RESUI(date, &s6e3fc3_rditbl[READ_DATE], 0);
static DEFINE_RESUI(octa_id, &s6e3fc3_rditbl[READ_OCTA_ID], 0);

/* for brightness debugging */
static DEFINE_RESUI(aor, &s6e3fc3_rditbl[READ_AOR], 0);
static DEFINE_RESUI(vint, &s6e3fc3_rditbl[READ_VINT], 0);
static DEFINE_RESUI(elvss_t, &s6e3fc3_rditbl[READ_ELVSS_T], 0);
static DEFINE_RESUI(rddpm, &s6e3fc3_rditbl[READ_RDDPM], 0);
static DEFINE_RESUI(rddsm, &s6e3fc3_rditbl[READ_RDDSM], 0);
static DEFINE_RESUI(err, &s6e3fc3_rditbl[READ_ERR], 0);
static DEFINE_RESUI(err_fg, &s6e3fc3_rditbl[READ_ERR_FG], 0);
static DEFINE_RESUI(dsi_err, &s6e3fc3_rditbl[READ_DSI_ERR], 0);
static DEFINE_RESUI(self_diag, &s6e3fc3_rditbl[READ_SELF_DIAG], 0);

#ifdef CONFIG_SUPPORT_GRAYSPOT_TEST
static DEFINE_RESUI(grayspot_cal, &s6e3fc3_rditbl[READ_GRAYSPOT_CAL], 0);
#endif

#ifdef CONFIG_SUPPORT_CCD_TEST
static DEFINE_RESUI(ccd_state, &s6e3fc3_rditbl[READ_CCD_STATE], 0);
#endif

static struct resinfo s6e3fc3_restbl[MAX_RESTBL] = {
	[RES_ID] = RESINFO_INIT(id, S6E3FC3_ID, RESUI(id)),
	[RES_COORDINATE] = RESINFO_INIT(coordinate, S6E3FC3_COORDINATE, RESUI(coordinate)),
	[RES_CODE] = RESINFO_INIT(code, S6E3FC3_CODE, RESUI(code)),
	[RES_DATE] = RESINFO_INIT(date, S6E3FC3_DATE, RESUI(date)),
	[RES_OCTA_ID] = RESINFO_INIT(octa_id, S6E3FC3_OCTA_ID, RESUI(octa_id)),
	[RES_RDDPM] = RESINFO_INIT(rddpm, S6E3FC3_RDDPM, RESUI(rddpm)),
	[RES_RDDSM] = RESINFO_INIT(rddsm, S6E3FC3_RDDSM, RESUI(rddsm)),
	[RES_ERR] = RESINFO_INIT(err, S6E3FC3_ERR, RESUI(err)),
	[RES_ERR_FG] = RESINFO_INIT(err_fg, S6E3FC3_ERR_FG, RESUI(err_fg)),
	[RES_DSI_ERR] = RESINFO_INIT(dsi_err, S6E3FC3_DSI_ERR, RESUI(dsi_err)),
	[RES_SELF_DIAG] = RESINFO_INIT(self_diag, S6E3FC3_SELF_DIAG, RESUI(self_diag)),
#ifdef CONFIG_SUPPORT_GRAYSPOT_TEST
	[RES_GRAYSPOT_CAL] = RESINFO_INIT(grayspot_cal, S6E3FC3_GRAYSPOT_CAL, RESUI(grayspot_cal)),
#endif
#ifdef CONFIG_SUPPORT_CCD_TEST
	[RES_CCD_STATE] = RESINFO_INIT(ccd_state, S6E3FC3_CCD_STATE, RESUI(ccd_state)),
	[RES_CCD_CHKSUM_FAIL] = {
		.name = "ccd_chksum_fail",
		.type = CMD_TYPE_RES,
		.state = RES_UNINITIALIZED,
		.data = S6E3FC3_CCD_CHKSUM_FAIL,
		.dlen = ARRAY_SIZE(S6E3FC3_CCD_CHKSUM_FAIL),
		.resui = NULL,
		.nr_resui = 0,
	},
#endif
};

enum {
	DUMP_RDDPM = 0,
	DUMP_RDDPM_SLEEP_IN,
	DUMP_RDDSM,
	DUMP_ERR,
	DUMP_ERR_FG,
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
static void show_err_fg(struct dumpinfo *info);
static void show_err(struct dumpinfo *info);
static void show_dsi_err(struct dumpinfo *info);
static void show_self_diag(struct dumpinfo *info);
#ifdef CONFIG_SUPPORT_DDI_CMDLOG
static void show_cmdlog(struct dumpinfo *info);
#endif
static void show_self_mask_crc(struct dumpinfo *info);

static struct dumpinfo s6e3fc3_dmptbl[] = {
	[DUMP_RDDPM] = DUMPINFO_INIT(rddpm, &s6e3fc3_restbl[RES_RDDPM], show_rddpm),
	[DUMP_RDDPM_SLEEP_IN] = DUMPINFO_INIT(rddpm_sleep_in, &s6e3fc3_restbl[RES_RDDPM], show_rddpm_before_sleep_in),
	[DUMP_RDDSM] = DUMPINFO_INIT(rddsm, &s6e3fc3_restbl[RES_RDDSM], show_rddsm),
	[DUMP_ERR_FG] = DUMPINFO_INIT(err_fg, &s6e3fc3_restbl[RES_ERR_FG], show_err_fg),
	[DUMP_ERR] = DUMPINFO_INIT(err, &s6e3fc3_restbl[RES_ERR], show_err),
	[DUMP_DSI_ERR] = DUMPINFO_INIT(dsi_err, &s6e3fc3_restbl[RES_DSI_ERR], show_dsi_err),
	[DUMP_SELF_DIAG] = DUMPINFO_INIT(self_diag, &s6e3fc3_restbl[RES_SELF_DIAG], show_self_diag),
#ifdef CONFIG_SUPPORT_DDI_CMDLOG
	[DUMP_CMDLOG] = DUMPINFO_INIT(cmdlog, &s6e3fc3_restbl[RES_CMDLOG], show_cmdlog),
#endif
	[DUMP_SELF_MASK_CRC] = DUMPINFO_INIT(self_mask_crc, &s6e3fc3_restbl[RES_SELF_MASK_CRC], show_self_mask_crc),
};

/* Variable Refresh Rate */
enum {
	S6E3FC3_VRR_MODE_NS,
	S6E3FC3_VRR_MODE_HS,
	MAX_S6E3FC3_VRR_MODE,
};

enum {
	S6E3FC3_VRR_120HS,
	S6E3FC3_VRR_60HS,
	MAX_S6E3FC3_VRR,
};

enum {
	S6E3FC3_RESOL_1080x2340,
};

enum {
	S6E3FC3_DISPLAY_MODE_1080x2340_120HS,
	S6E3FC3_DISPLAY_MODE_1080x2340_60HS,
	MAX_S6E3FC3_DISPLAY_MODE,
};

enum {
	S6E3FC3_VRR_KEY_REFRESH_RATE,
	S6E3FC3_VRR_KEY_REFRESH_MODE,
	S6E3FC3_VRR_KEY_TE_SW_SKIP_COUNT,
	S6E3FC3_VRR_KEY_TE_HW_SKIP_COUNT,
	MAX_S6E3FC3_VRR_KEY,
};


static int init_common_table(struct maptbl *tbl);
#ifdef CONFIG_EXYNOS_DECON_MDNIE_LITE
static int getidx_common_maptbl(struct maptbl *tbl);
#endif
static int init_gamma_mode2_brt_table(struct maptbl *tbl);
static int getidx_gamma_mode2_brt_table(struct maptbl *tbl);
static int getidx_hbm_transition_table(struct maptbl *tbl);
static int getidx_smooth_transition_table(struct maptbl *tbl);

static int getidx_acl_opr_table(struct maptbl *tbl);
static int getidx_hbm_onoff_table(struct maptbl *);
static int getidx_acl_onoff_table(struct maptbl *);
static int getidx_acl_dim_onoff_table(struct maptbl *tbl);

static int init_lpm_brt_table(struct maptbl *tbl);
static int getidx_lpm_brt_table(struct maptbl *tbl);
static bool s6e3fc3_is_120hz(struct panel_device *panel);
static bool s6e3fc3_is_60hz(struct panel_device *panel);
static bool is_panel_state_not_lpm(struct panel_device *panel);

#ifdef CONFIG_SUPPORT_GRAYSPOT_TEST
static void copy_grayspot_cal_maptbl(struct maptbl *tbl, u8 *dst);
#endif

#ifdef CONFIG_EXYNOS_DECON_MDNIE_LITE
static void copy_dummy_maptbl(struct maptbl *tbl, u8 *dst);
#endif
static void copy_common_maptbl(struct maptbl *tbl, u8 *dst);
static void copy_tset_maptbl(struct maptbl *tbl, u8 *dst);
#ifdef CONFIG_SUPPORT_XTALK_MODE
static int getidx_vgh_table(struct maptbl *tbl);
#endif
#if defined(CONFIG_SEC_FACTORY) && defined(CONFIG_SUPPORT_FAST_DISCHARGE)
static int getidx_fast_discharge_table(struct maptbl *tbl);
#endif
static int getidx_vrr_fps_table(struct maptbl *);
#if defined(__PANEL_NOT_USED_VARIABLE__)
static int getidx_vrr_gamma_table(struct maptbl *);
#endif
static bool s6e3fc3_is_120hz(struct panel_device *panel);
static bool s6e3fc3_is_60hz(struct panel_device *panel);
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

#if defined(__PANEL_NOT_USED_VARIABLE__)
static int init_gamma_mtp_all_table(struct maptbl *tbl);
#endif

static int getidx_mdnie_scr_white_maptbl(struct pkt_update_info *pktui);
static void update_current_scr_white(struct maptbl *tbl, u8 *dst);
#endif /* CONFIG_EXYNOS_DECON_MDNIE_LITE */
#ifdef CONFIG_DYNAMIC_FREQ
static int getidx_dyn_ffc_table(struct maptbl *tbl);
#endif
static bool is_panel_state_not_lpm(struct panel_device *panel);
#endif /* __S6E3FC3_H__ */
