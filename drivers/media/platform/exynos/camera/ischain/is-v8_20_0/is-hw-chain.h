/*
 * Samsung Exynos SoC series FIMC-IS2 driver
 *
 * Copyright (c) 2018 Samsung Electronics Co., Ltd
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#ifndef IS_HW_CHAIN_V8_10_H
#define IS_HW_CHAIN_V8_10_H

#include "is-hw-api-common.h"
#include "is-groupmgr.h"
#include "is-config.h"

enum sysreg_is_reg_name {
	SYSREG_R_IS_USER_GLUE_CON0,
	SYSREG_R_IS_USER_GLUE_CON2,
	SYSREG_R_IS_USER_GLUE_CON4,
	SYSREG_R_IS_USER_GLUE_CON6,
	SYSREG_R_IS_USER_GLUE_CON8,
	SYSREG_R_IS_USER_ACIC_CON7,
	SYSREG_R_MIPI_PHY_CON0,
	SYSREG_IS_REG_CNT
};

enum sysreg_is_reg_field {
	SYSREG_F_MUX_FOR_OTF0,
	SYSREG_F_MUX_FOR_OTF1,
	SYSREG_F_MASK_FOR_CSIS0_OTF,
	SYSREG_F_MASK_FOR_CSIS1_OTF,
	SYSREG_F_MASK_FOR_CSIS2_OTF,
	SYSREG_F_AW_AC_TARGET_GDC,
	SYSREG_F_AR_AC_TARGET_GDC,
	SYSREG_F_MIPI_RESETN_DPHY_S2,
	SYSREG_F_MIPI_RESETN_DPHY_S4_1,
	SYSREG_F_MIPI_RESETN_DPHY_S4_0,
	SYSREG_IS_REG_FIELD_CNT
};

#define GROUP_HW_MAX (GROUP_SLOT_MAX)

#define IORESOURCE_CSIS_DMA	0

#define IORESOURCE_3AA0		1
#define IORESOURCE_3AA1		2
#define IORESOURCE_ITP0		3
#define IORESOURCE_MCSC		4
#define IORESOURCE_VRA0		5
#define IORESOURCE_MCSC_MFC		6

#define GROUP_SENSOR_MAX_WIDTH	12000
#define GROUP_SENSOR_MAX_HEIGHT	9000
#define GROUP_3AA_MAX_WIDTH	6560
#define GROUP_3AA_MAX_HEIGHT	4928
#define GROUP_ITP_MAX_WIDTH	3280
#define GROUP_ITP_MAX_HEIGHT	2464
#define GROUP_VRA_MAX_WIDTH	320
#define GROUP_VRA_MAX_HEIGHT	240
#define GROUP_MCSC_MFC_MAX_WIDTH	5760
#define GROUP_MCSC_MFC_MAX_HEIGHT	4320

/* RTA HEAP: 6MB */
#define IS_RESERVE_LIB_SIZE	(0x00600000)
/* DDK DMA: 5.065MB */
#define IS_TAAISP_SIZE		(0x00510000)
/* ME/DRC DMA: 7.5MB */
#define TAAISP_MEDRC_SIZE		(0x00780000)
/* ORBMCH DMA: Do not Use */
#define TAAISP_ORBMCH_SIZE		(0)
/* CLAHE DMA: Do not Use */
#define IS_CLAHE_SIZE		(0)
/* VRA: 8MB */
#define IS_VRA_SIZE		(0x00800000) /* Need to Fix */
/* VRA NET ARRAY : 4MB */
#define VRA_NET_ARR_SIZE		(0x00400000)
/* DDK HEAP: 45MB */
#define IS_HEAP_SIZE		(0x02D00000)

/* SETFILE: 7MB */
#define IS_SETFILE_SIZE		(0x00700000)

/* Rule checker size for DDK */
#define IS_RCHECKER_SIZE_RO	(0)
#define IS_RCHECKER_SIZE_RW	(0)

#define SYSREG_IS_BASE_ADDR	(0x14520000)

#define HWFC_INDEX_RESET_ADDR	(0x12CA0850)

enum taaisp_chain_id {
	ID_3AA_0 = 0,	/* MEIP/DMA/3AA0 */
	ID_3AA_1 = 1,	/* MEIP/DMA/3AA1 */
	ID_ISP_0 = 2,	/* TNR/DNS/ITP0 */
	ID_ISP_1 = 3,	/* not used */
	ID_TPU_0 = 4,	/* not used */
	ID_TPU_1 = 5,	/* not used */
	ID_DCP	 = 6,	/* not used */
	ID_3AA_2 = 7,	/* not used */
	ID_CLH_0 = 8,	/* not used */
	ID_3AA_3 = 9,	/* not used */
	ID_YPP   = 10,	/* not used */
	ID_3AAISP_MAX
};

/* the number of interrupt source at each IP */
enum hwip_interrupt_map {
	INTR_HWIP1 = 0,
	INTR_HWIP2 = 1,
	INTR_HWIP3 = 2,
	INTR_HWIP4 = 3,
	INTR_HWIP5 = 4,
	INTR_HWIP6 = 5,
	INTR_HWIP_MAX
};

#define INTR_ID_BASE_OFFSET	(INTR_HWIP_MAX)
#define GET_IRQ_ID(y, x)	(x - (INTR_ID_BASE_OFFSET * y))
#define valid_3aaisp_intr_index(intr_index) \
	(intr_index >= 0 && intr_index < INTR_HWIP_MAX)

/* TODO: update below for 9630 */
/* Specific interrupt map belonged to each IP */

/* MC-Scaler */
#define USE_DMA_BUFFER_INDEX		(0) /* 0 ~ 7 */
#define MCSC_WIDTH_ALIGN		(2)
#define MCSC_HEIGHT_ALIGN		(2)
#define MCSC_PRECISION			(20)
#define MCSC_POLY_RATIO_UP		(14)
#define MCSC_POLY_QUALITY_RATIO_DOWN	(4)
#define MCSC_POLY_RATIO_DOWN		(16)
#define MCSC_POLY_MAX_RATIO_DOWN       	(24)
#define MCSC_POST_RATIO_DOWN		(16)
#define MCSC_POST_MAX_WIDTH		(1060)
/* #define MCSC_POST_WA */
/* #define MCSC_POST_WA_SHIFT	(8)*/	/* 256 = 2^8 */
#define MCSC_USE_DEJAG_TUNING_PARAM	(true)		/* Only Use interface sync with DDK (MCSC setfile) */
/* #define MCSC_DNR_USE_TUNING		(true) */	/* DNR and DJAG TUNING PARAM are used exclusively. */
#define MCSC_SETFILE_VERSION		(0x14027434)
/* #define MCSC_DJAG_ENABLE_SENSOR_BRATIO	(2000) */
#define MCSC_LINE_BUF_SIZE		(3280)
#define HWFC_DMA_ID_OFFSET   	(7)
#define ENTRY_HF                        ENTRY_M5P       /* Subdev ID of MCSC port for High Frequency */

enum mc_scaler_interrupt_map {
	INTR_MC_SCALER_FRAME_END		= 0,
	INTR_MC_SCALER_FRAME_START		= 1,
	INTR_MC_SCALER_WDMA_FINISH		= 3,
	INTR_MC_SCALER_CORE_FINISH		= 4,
	INTR_MC_SCALER_SHADOW_HW_TRIGGER	= 5,
	INTR_MC_SCALER_SHADOW_TRIGGER	= 6,
	INTR_MC_SCALER_INPUT_PROTOCOL_ERR	= 7,
	INTR_MC_SCALER_INPUT_HORIZONTAL_OVF	= 8,
	INTR_MC_SCALER_INPUT_HORIZONTAL_UNF	= 9,
	INTR_MC_SCALER_INPUT_VERTICAL_OVF	= 10,
	INTR_MC_SCALER_INPUT_VERTICAL_UNF	= 11,
	INTR_MC_SCALER_OVERFLOW			= 13,
	/* UNUSED : for compatibility */
	INTR_MC_SCALER_OUTSTALL		= 17,
};
#define MCSC_INTR_MASK		((1 << INTR_MC_SCALER_WDMA_FINISH) \
				| (1 << INTR_MC_SCALER_CORE_FINISH) \
				| (1 << INTR_MC_SCALER_SHADOW_HW_TRIGGER) \
				| (1 << INTR_MC_SCALER_SHADOW_TRIGGER))

/* TODO: remove this, compile check only */
/* VRA */
#define VRA_CH1_INTR_CNT_PER_FRAME	(6)

/* Deprecated register. These are maintained for backward compatibility */
enum vra_interrupt_map {
	CNN_END_INT			= 1,	/* CNN end interrupt */
	CNN_AXI_ERR			= 2,	/* CNN BRESP/RRESP errors */
	CNN_ECC_ERR			= 3,	/* CNN memory ECC error */
	CNN_END_OF_LAYER	= 4,	/* CNN layer end indication */
};

enum vra_chain0_interrupt_map {
	CH0INT_CIN_FR_ST			= 0,
	CH0INT_CIN_FR_END			= 1,
	CH0INT_CIN_LINE_ST			= 2,
	CH0INT_CIN_LINE_END			= 3,
	CH0INT_CIN_SP_LINE			= 4,
	CH0INT_CIN_ERR_SIZES			= 5,
	CH0INT_CIN_ERR_YUV_FORMAT		= 6,
	CH0INT_CIN_FR_ST_NO_ACTIVE		= 7,
	CH0INT_DMA_IN_ERROR			= 8,
	CH0INT_DMA_IN_FLUSH_DONE		= 9,
	CH0INT_DMA_IN_FR_END			= 10,
	CH0INT_DMA_IN_INFO			= 11,
	CH0INT_OUT_DMA_ERROR			= 12,
	CH0INT_OUT_DMA_FLUSH_DONE		= 13,
	CH0INT_OUT_DMA_FR_END			= 14,
	CH0INT_OUT_DMA_INFO			= 15,
	CH0INT_RWS_TRIGGER			= 16,
	CH0INT_END_FRAME			= 17,
	CH0INT_END_ISP_DMA_OUT			= 18,
	CH0INT_END_ISP_INPUT			= 19,
	CH0INT_FRAME_SIZE_ERROR			= 20,
	CH0INT_ERR_FR_ST_BEF_FR_END		= 21,
	CH0INT_ERR_FR_ST_WHILE_FLUSH		= 22,
	CH0INT_ERR_VRHR_INTERVAL_VIOLATION	= 23,
	CH0INT_ERR_HFHR_INTERVAL_VIOLATION	= 24,
	CH0INT_ERR_PYRAMID_OVERFLOW		= 25
};

enum vra_chain1_interrupt_map {
	CH1INT_IN_CONT_SP_LINE		= 0,
	CH1INT_IN_STOP_IMMED_DONE	= 1,
	CH1INT_IN_END_OF_CONTEXT	= 2,
	CH1INT_IN_START_OF_CONTEXT	= 3,
	CH1INT_END_LOAD_FEATURES	= 4,
	CH1INT_SHADOW_TRIGGER		= 5,
	CH1INT_OUT_OVERFLOW		= 6,
	CH1INT_MAX_NUM_RESULTS		= 7,

	CH1INT_IN_ERROR			= 8,
	CH1INT_IN_FLUSH_DONE		= 9,
	CH1INT_IN_FR_END		= 10,
	CH1INT_IN_INFO			= 11,

	CH1INT_OUT_ERROR		= 12,
	CH1INT_OUT_FLUSH_DONE		= 13,
	CH1INT_OUT_FR_END		= 14,
	CH1INT_OUT_INFO			= 15,
	CH1INT_WATCHDOG			= 16
};

#define CSIS0_QCH_EN_ADDR		(0x14400004)
#define CSIS1_QCH_EN_ADDR		(0x14410004)
#define CSIS2_QCH_EN_ADDR		(0x14420004)

#define ALIGN_UPDOWN_STRIPE_WIDTH(w) \
	(w & STRIPE_WIDTH_ALIGN >> 1 ? ALIGN(w, STRIPE_WIDTH_ALIGN) : ALIGN_DOWN(w, STRIPE_WIDTH_ALIGN))

int exynos3830_is_dump_clk(struct device *dev);

#endif
