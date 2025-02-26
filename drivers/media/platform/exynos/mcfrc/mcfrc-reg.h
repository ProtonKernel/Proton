/* linux/drivers/media/platform/exynos/mcfrc/mcfrc-regs.h
 *
 * Register definition file for Samsung JPEG Squeezer driver
 *
 * Copyright (c) 2020 Samsung Electronics Co., Ltd.
 *		http://www.samsung.com
 *
 * Authors: Jungik Seo <jungik.seo@samsung.com>
 *          Igor Kovliga <i.kovliga@samsung.com>
 *          Sergei Ilichev <s.ilichev@samsung.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#ifndef MCFRC_REGS_H_
#define MCFRC_REGS_H_

#ifndef __DEBUG_WANT_ONLY_MACROSES_
#include <linux/io.h>
#include <linux/delay.h>
#include <linux/module.h>
#include "mcfrc-core.h"
#endif

#define CAVY_GET_MASK(hi, low)                      ( (((u32)0xffffffff)>>(31-hi)) & (((u32)0xffffffff)<<(low)) )
#define CAVY_SHIFT_CLIP_VAL(var, hi, low)           ( ((u32)(var)<<low) & CAVY_GET_MASK(hi,low) )
#define CAVY_INSERT_VAL(var, val, hi, low)          ( var = (var & (~CAVY_GET_MASK(hi, low))) | CAVY_SHIFT_CLIP_VAL(val,hi,low) )
#define CAVY_PICK_VAL(var, val, hi, low)            ( var = (val & CAVY_GET_MASK(hi,low)) >> low)


#define REG_TOP_0x0000_FRC_MC_START   (0x0000)
////    0x0000	WC		FRC_MC_START
////            WC[0]	FRC_MC_START
#define SET_FRC_MC_START_REG(base_addr, val)            writel(val, base_addr+REG_TOP_0x0000_FRC_MC_START)

#define REG_TOP_0x0004_FRC_APB_UPDATE   (0x0004)
////    0x0004	WC		FRC_APB_UPDATE
////            WC[0]	FRC_APB_UPDATE
#define SET_FRC_APB_UPDATE_REG(base_addr, val)          writel(val, base_addr+REG_TOP_0x0004_FRC_APB_UPDATE)

#define REG_TOP_0x0008_MC_SW_RESET   (0x0008)
////    0x0008	RW		SW Reset
////            RW[0]	MC_SW_RESET
#define SET_MC_SW_RESET_REG(base_addr, val)             writel(val, base_addr+REG_TOP_0x0008_MC_SW_RESET)
#define GET_MC_SW_RESET_REG(base_addr, val)             (val = readl(base_addr+REG_TOP_0x0008_MC_SW_RESET))

#define REG_TOP_0x000C_MC_INTR_CLEAR   (0x000C)
////    0x000C	WC		MC_INTR_CLEAR
////            WC[0]	MC_INTR_CLEAR
#define SET_MC_INTR_CLEAR_REG(base_addr, val)           writel(val, base_addr+REG_TOP_0x000C_MC_INTR_CLEAR)

#define REG_TOP_0x0010_MC_TIMEOUT   (0x0010)
////    0x0010	RW		MC_TIMEOUT
////            RW[31]	MC_TIMEOUT_EN
////            RW[23:8]	MC_TIMEOUT_THRESHOLD
////            RW[7:0]	MC_OUT_TILE_THRESHOLD
#define INSERT_MC_TIMEOUT_EN(var, val)                  CAVY_INSERT_VAL(var, val, 31, 31)
#define INSERT_MC_TIMEOUT_THRESHOLD(var, val)           CAVY_INSERT_VAL(var, val, 23, 8)
#define INSERT_MC_OUT_TILE_THRESHOLD(var, val)          CAVY_INSERT_VAL(var, val, 7, 0)
#define SET_MC_TIMEOUT_REG(base_addr, val)              writel(val, base_addr+REG_TOP_0x0010_MC_TIMEOUT)
#define GET_MC_TIMEOUT_REG(base_addr, val)              (val = readl(base_addr+REG_TOP_0x0010_MC_TIMEOUT))

#define REG_TOP_0x0014_MC_CLKREQ_CONTROL   (0x0014)
////    0x0014	RW		MC_CLKREQ_CONTROL
////            RW[0]	MC_CLKREQ_CONTROL
#define SET_MC_CLKREQ_CONTROL_REG(base_addr, val)       writel(val, base_addr+REG_TOP_0x0014_MC_CLKREQ_CONTROL)
#define GET_MC_CLKREQ_CONTROL_REG(base_addr, val)       (val = readl(base_addr+REG_TOP_0x0014_MC_CLKREQ_CONTROL))


#define REG_TOP_0x0018_MC_FRAME_SIZE   (0x0018)
////    0x0018	RW		MC_FRAME_SIZE
////            RW[31:16]	MC_FRAME_WIDTH
////            RW[15:0]	MC_FRAME_HEIGHT
#define INSERT_MC_FRAME_WIDTH(var, val)                 CAVY_INSERT_VAL(var, val, 31, 16)
#define INSERT_MC_FRAME_HEIGHT(var, val)                CAVY_INSERT_VAL(var, val, 15, 0)
#define SET_MC_FRAME_SIZE_REG(base_addr, val)           writel(val, base_addr+REG_TOP_0x0018_MC_FRAME_SIZE)
#define GET_MC_FRAME_SIZE_REG(base_addr, val)           (val = readl(base_addr+REG_TOP_0x0018_MC_FRAME_SIZE))

#define REG_TOP_0x001C_MC_BYPASS_PHASE   (0x001C)
////    0x001C	RW		MC_BYPASS_PHASE
////            RW[9:8]	MC_BYPASS_TYPE
////            RW[5:0]	MC_FRAME_PHASE
#define INSERT_MC_BYPASS_TYPE(var, val)                 CAVY_INSERT_VAL(var, val, 9, 8)
#define INSERT_MC_FRAME_PHASE(var, val)                 CAVY_INSERT_VAL(var, val, 5, 0)
#define SET_MC_BYPASS_PHASE_REG(base_addr, val)         writel(val, base_addr+REG_TOP_0x001C_MC_BYPASS_PHASE)
#define GET_MC_BYPASS_PHASE_REG(base_addr, val)         (val = readl(base_addr+REG_TOP_0x001C_MC_BYPASS_PHASE))

#define REG_TOP_0x0020_MC_BATCH_MODE   (0x0020)
////    0x0020	RW		MC_BATCH_MODE
////            RW[16]	MC_BATCH_EN
////            RW[15:0]	MC_BATCH_NUMBER
#define INSERT_MC_BATCH_EN(var, val)                    CAVY_INSERT_VAL(var, val, 16, 16)
#define INSERT_MC_BATCH_NUMBER(var, val)                CAVY_INSERT_VAL(var, val, 15, 0)
#define SET_MC_BATCH_MODE_REG(base_addr, val)           writel(val, base_addr+REG_TOP_0x0020_MC_BATCH_MODE)
#define GET_MC_BATCH_MODE_REG(base_addr, val)           (val = readl(base_addr+REG_TOP_0x0020_MC_BATCH_MODE))

#define REG_TOP_0x0024_MC_FRAME_DONE_CHECK   (0x0024)
////    0x0024	RW		MC_FRAME_DONE_CHECK
////            RW[31]	MC_FRAME_DONE_CHECK_EN
////            RW[27:0]	MC_FRAME_DONE_COUNT_TH
#define INSERT_MC_FRAME_DONE_CHECK_EN(var, val)         CAVY_INSERT_VAL(var, val, 31, 31)
#define INSERT_MC_FRAME_DONE_COUNT_TH(var, val)         CAVY_INSERT_VAL(var, val, 27, 0)
#define SET_MC_FRAME_DONE_CHECK_REG(base_addr, val)     writel(val, base_addr+REG_TOP_0x0024_MC_FRAME_DONE_CHECK)
#define GET_MC_FRAME_DONE_CHECK_REG(base_addr, val)     (val = readl(base_addr+REG_TOP_0x0024_MC_FRAME_DONE_CHECK))

#define REG_TOP_0x0028_MC_INTR_MASKING   (0x0028)
////    0x0028	RW		MC_INTR_MASKING
////            [16]	o_FRC_MC_DONE_IRQ' Mask
////            [12]	o_FRC_MC_DBL_ERR_IRQ' Fuction All Mask
////            [8]	└ SRST_DONE_IRQ Mask
////            [4]	└ FRC_DBL_M_DONE_IRQ Mask
////            [0]	└ FRC_MC_ERROR_IRQ Mask
#define INSERT_FRC_MC_DONE_IRQ(var, val)                CAVY_INSERT_VAL(var, val, 16, 16)
#define INSERT_FRC_MC_DBL_ERR_IRQ(var, val)             CAVY_INSERT_VAL(var, val, 12, 12)
#define INSERT_SRST_DONE_IRQ(var, val)                  CAVY_INSERT_VAL(var, val, 8 , 8 )
#define INSERT_FRC_DBL_M_DONE_IRQ(var, val)             CAVY_INSERT_VAL(var, val, 4 , 4 )
#define INSERT_FRC_MC_ERROR_IRQ(var, val)               CAVY_INSERT_VAL(var, val, 0 , 0 )
#define SET_MC_INTR_MASKING_REG(base_addr, val)         writel(val, base_addr+REG_TOP_0x0028_MC_INTR_MASKING)
#define GET_MC_INTR_MASKING_REG(base_addr, val)         (val = readl(base_addr+REG_TOP_0x0028_MC_INTR_MASKING))

#define REG_TOP_0x002C_MC_RSVD3   (0x002C)
////    0x002C	RW		MC_RSVD3

#define REG_TOP_0x0030_FRC_DBL_START   (0x0030)
////    0x0030	WC		FRC_DBL_START
////            WC[0]	FRC_DBL_START
#define SET_FRC_DBL_START_REG(base_addr, val)           writel(val, base_addr+REG_TOP_0x0030_FRC_DBL_START)

#define REG_TOP_0x0034_FRC_DBL_EN   (0x0034)
////    0x0034	RW		FRC_DBL_EN
////            [0]	FRC_DBL_EN
#define SET_FRC_DBL_EN_REG(base_addr, val)              writel(val, base_addr+REG_TOP_0x0034_FRC_DBL_EN)
#define GET_FRC_DBL_EN_REG(base_addr, val)              (val = readl(base_addr+REG_TOP_0x0034_FRC_DBL_EN))

#define REG_TOP_0x0038_FRC_DBL_MODE   (0x0038)
////    0x0038	RW		FRC_DBL_MODE
////            [1:0]	FRC_DBL_MODE
#define FRC_DBL_MODE_PARALLEL           0x0
#define FRC_DBL_MODE_SERIAL             0x2
#define FRC_DBL_MODE_MANUAL             0x3
#define SET_FRC_DBL_MODE_REG(base_addr, val)            writel(val, base_addr+REG_TOP_0x0038_FRC_DBL_MODE)
#define GET_FRC_DBL_MODE_REG(base_addr, val)            (val = readl(base_addr+REG_TOP_0x0038_FRC_DBL_MODE))

#define REG_TOP_0x003C_DMA_SBI_RD_CONFIG   (0x003C)
////    0x003C	RW		DMA_SBI_RD_CONFIG
////            RW[16]	SBI_RD_STOP_REQ
////            RW[15:0]	SBI_RD_CONFIG
#define INSERT_SBI_RD_STOP_REQ(var, val)                CAVY_INSERT_VAL(var, val, 16, 16)
#define INSERT_SBI_RD_CONFIG(var, val)                  CAVY_INSERT_VAL(var, val, 15, 0)
#define SET_DMA_SBI_RD_CONFIG_REG(base_addr, val)       writel(val, base_addr+REG_TOP_0x003C_DMA_SBI_RD_CONFIG)
#define GET_DMA_SBI_RD_CONFIG_REG(base_addr, val)       (val = readl(base_addr+REG_TOP_0x003C_DMA_SBI_RD_CONFIG))

#define REG_TOP_0x0040_DMA_SBI_RD_STATUS   (0x0040)
////    0x0040	RO		DMA_SBI_RD_STATUS
////            RO[16]	SBI_RD_STOP_ACK
////            RO[15:0]	SBI_RD_STATUS
#define PICK_SBI_RD_STOP_ACK(var, val)                  CAVY_PICK_VAL(var, val, 16, 16)
#define PICK_SBI_RD_STATUS(var, val)                    CAVY_PICK_VAL(var, val, 15, 0)
#define GET_DMA_SBI_RD_STATUS_REG(base_addr, val)       (val = readl(base_addr+REG_TOP_0x0040_DMA_SBI_RD_STATUS))

#define REG_TOP_0x0044_DMA_SBI_WR_CONFIG   (0x0044)
////    0x0044	RW		DMA_SBI_WR_CONFIG
////            RW[16]	SBI_WR_STOP_REQ
////            RW[15:0]	SBI_WR_CONFIG
#define INSERT_SBI_WR_STOP_REQ(var, val)                CAVY_INSERT_VAL(var, val, 16, 16)
#define INSERT_SBI_WR_CONFIG(var, val)                  CAVY_INSERT_VAL(var, val, 15, 0)
#define SET_DMA_SBI_WR_CONFIG_REG(base_addr, val)       writel(val, base_addr+REG_TOP_0x0044_DMA_SBI_WR_CONFIG)
#define GET_DMA_SBI_WR_CONFIG_REG(base_addr, val)       (val = readl(base_addr+REG_TOP_0x0044_DMA_SBI_WR_CONFIG))

#define REG_TOP_0x0048_DMA_SBI_WR_STATUS   (0x0048)
////    0x0048	RO		DMA_SBI_WR_STATUS
////            RO[16]	SBI_WR_STOP_ACK
////            RO[15:0]	SBI_WR_STATUS
#define PICK_SBI_WR_STOP_ACK(var, val)                  CAVY_PICK_VAL(var, val, 16, 16)
#define PICK_SBI_WR_STATUS(var, val)                    CAVY_PICK_VAL(var, val, 15, 0)
#define GET_DMA_SBI_WR_STATUS_REG(base_addr, val)       (val = readl(base_addr+REG_TOP_0x0048_DMA_SBI_WR_STATUS))

#define REG_TOP_0x004C_MC_AXI_USER   (0x004C)
////    0x004C	RW		MC_AXI_USER
////            RW[7:4]	MC_AXI_AWUSER
////            RW[3:0]	MC_AXI_ARUSER
#define INSERT_MC_AXI_AWUSER(var, val)                  CAVY_INSERT_VAL(var, val, 7, 4)
#define INSERT_MC_AXI_ARUSER(var, val)                  CAVY_INSERT_VAL(var, val, 3, 0)
#define SET_MC_AXI_USER_REG(base_addr, val)             writel(val, base_addr+REG_TOP_0x004C_MC_AXI_USER)
#define GET_MC_AXI_USER_REG(base_addr, val)             (val = readl(base_addr+REG_TOP_0x004C_MC_AXI_USER))

#define REG_TOP_0x0050_DMA_SW_RESET   (0x0050)
////    0x0050	RW		DMA_SW_RESET
////            RW[0]	DMA_SW_RESET
#define SET_DMA_SW_RESET_REG(base_addr, val)            writel(val, base_addr+REG_TOP_0x0050_DMA_SW_RESET)
#define GET_DMA_SW_RESET_REG(base_addr, val)            (val = readl(base_addr+REG_TOP_0x0050_DMA_SW_RESET))

#define REG_TOP_0x0054_INTR_MASK_DBG_STATUS   (0x0054)
////    0x0054	RO		INTR_MASK_DBG_STATUS
////            [20]	FRC_MC_SRST_DONE_IRQ
////            [16]	FRC_DBL_M_DONE_IRQ
////            [12]	FRC_MC_ERROR_IRQ
////            [8]	FRC_MC_DONE_FB_IRQ
////            [4]	FRC_MC_DONE_TIME_IRQ
////            [0]	FRC_MC_DONE_NORM_IRQ
#define PICK_DBG_FRC_MC_SRST_DONE_IRQ(var, val)         CAVY_PICK_VAL(var, val, 20, 20)
#define PICK_DBG_FRC_DBL_M_DONE_IRQ(var, val)           CAVY_PICK_VAL(var, val, 16, 16)
#define PICK_DBG_FRC_MC_ERROR_IRQ(var, val)             CAVY_PICK_VAL(var, val, 12, 12)
#define PICK_DBG_FRC_MC_DONE_FB_IRQ(var, val)           CAVY_PICK_VAL(var, val, 8 , 8 )
#define PICK_DBG_FRC_MC_DONE_TIME_IRQ(var, val)         CAVY_PICK_VAL(var, val, 4 , 4 )
#define PICK_DBG_FRC_MC_DONE_NORM_IRQ(var, val)         CAVY_PICK_VAL(var, val, 0 , 0 )
#define GET_INTR_MASK_DBG_STATUS_REG(base_addr, val)    (val = readl(base_addr+REG_TOP_0x0054_INTR_MASK_DBG_STATUS))

#define REG_TOP_0x0058_FRC_MC_DONE_IRQ_STATUS   (0x0058)
////    0x0058	RO		FRC_MC_DONE_IRQ_STATUS
////            [8]	FRC_MC_DONE_FB_IRQ
////            [4]	FRC_MC_DONE_TIME_IRQ
////            [0]	FRC_MC_DONE_NORM_IRQ
#define IRQ_MC_DONE_FB              (0x1<<8)
#define IRQ_MC_DONE_TIME            (0x1<<4)
#define IRQ_MC_DONE_NORM            (0x1<<0)
#define PICK_FRC_MC_DONE_FB_IRQ(var, val)               CAVY_PICK_VAL(var, val, 8 , 8 )
#define PICK_FRC_MC_DONE_TIME_IRQ(var, val)             CAVY_PICK_VAL(var, val, 4 , 4 )
#define PICK_FRC_MC_DONE_NORM_IRQ(var, val)             CAVY_PICK_VAL(var, val, 0 , 0 )
#define GET_FRC_MC_DONE_IRQ_STATUS_REG(base_addr, val)  (val = readl(base_addr+REG_TOP_0x0058_FRC_MC_DONE_IRQ_STATUS))

#define REG_TOP_0x005C_FRC_MC_DBL_ERR_IRQ_STATUS   (0x005C)
////    0x005C	RO		FRC_MC_DBL_ERR_IRQ_STATUS
////            [8]	FRC_MC_SRST_DONE_IRQ
////            [4]	FRC_DBL_M_DONE_IRQ
////            [0]	FRC_MC_ERROR_IRQ
#define IRQ_ERRRST_STATE_RST        (0x1<<8)
#define IRQ_ERRRST_STATE_DMAN       (0x1<<4)
#define IRQ_ERRRST_STATE_TERR       (0x1<<0)
#define PICK_FRC_MC_SRST_DONE_IRQ(var, val)             CAVY_PICK_VAL(var, val, 8 , 8 )
#define PICK_FRC_DBL_M_DONE_IRQ(var, val)               CAVY_PICK_VAL(var, val, 4 , 4 )
#define PICK_FRC_MC_ERROR_IRQ(var, val)                 CAVY_PICK_VAL(var, val, 0 , 0 )
#define GET_FRC_MC_DBL_ERR_IRQ_STATUS_REG(base_addr, val)   (val = readl(base_addr+REG_TOP_0x005C_FRC_MC_DBL_ERR_IRQ_STATUS))

#define REG_TOP_0x0060_DBL_INTR_CLEAR   (0x0060)
////    0x0060	WC		DBL_INTR_CLEAR
////            WC[0]	DBL_INTR_CLEAR
#define SET_DBL_INTR_CLEAR_REG(base_addr, val)          writel(val, base_addr+REG_TOP_0x0060_DBL_INTR_CLEAR)

#define REG_TOP_0x0064_MC_ERROR_DEBUG_ENABLE   (0x0064)
////    0x0064	RW		MC_ERROR_DEBUG_ENABLE
////            RW[12]	DEBUG_MONITORING_EN
////            RW[8]	DEBUG_APB_READ_SEL
////            RW[3:0]	MC_ERROR_ENABLE
#define INSERT_DEBUG_MONITORING_EN(var, val)            CAVY_INSERT_VAL(var, val, 12, 12)
#define INSERT_DEBUG_APB_READ_SEL(var, val)             CAVY_INSERT_VAL(var, val, 8, 8)
#define INSERT_MC_ERROR_ENABLE(var, val)                CAVY_INSERT_VAL(var, val, 3, 0)
#define SET_MC_ERROR_DEBUG_ENABLE_REG(base_addr, val)             writel(val, base_addr+REG_TOP_0x0064_MC_ERROR_DEBUG_ENABLE)
#define GET_MC_ERROR_DEBUG_ENABLE_REG(base_addr, val)             (val = readl(base_addr+REG_TOP_0x0064_MC_ERROR_DEBUG_ENABLE))

#define REG_TOP_0x0068_MC_ERROR_CLEAR   (0x0068)
////    0x0068	WC		MC_ERROR_CLEAR
////            WC[31:0]	MC_ERROR_CLEAR
#define SET_MC_ERROR_CLEAR_REG(base_addr, val)           writel(val, base_addr+REG_TOP_0x0068_MC_ERROR_CLEAR)

#define REG_TOP_0x006C_MC_ERROR_STATUS   (0x006C)
////    0x006C	RO		MC_ERROR_STATUS
////            RO[31:16]	DMA_ERROR_STATUS
////            RO[11:8]	DBL_CORE_ERROR_STATUS
////            RO[7:4]	    MC_CORE_ERROR_STATUS
////            RO[1:0]	    TOP_ERROR_STATUS
#define PICK_DMA_ERROR_STATUS(var, val)                 CAVY_PICK_VAL(var, val, 31 , 16 )
#define PICK_DBL_CORE_ERROR_STATUS(var, val)            CAVY_PICK_VAL(var, val, 11, 8 )
#define PICK_MC_CORE_ERROR_STATUS(var, val)             CAVY_PICK_VAL(var, val, 7 , 4 )
#define PICK_TOP_ERROR_STATUS(var, val)                 CAVY_PICK_VAL(var, val, 1 , 0 )
#define GET_MC_ERROR_STATUS_REG(base_addr, val)         (val = readl(base_addr+REG_TOP_0x006C_MC_ERROR_STATUS))

#define REG_TOP_0x0070_MC_CORE_STATUS_0   (0x0070)
////    0x0070	RO		MC_CORE_STATUS_0
////            RO[31:0]	MC_CORE_TOTAL_READ_REQ
#define GET_MC_CORE_STATUS_0_REG(base_addr, val)            (val = readl(base_addr + REG_TOP_0x0070_MC_CORE_STATUS_0))

#define REG_TOP_0x0074_MC_CORE_STATUS_1   (0x0074)
////    0x0074	RO		MC_CORE_STATUS_1
////            RO[31:20]	MC_CORE_MAX_READ_REQUEST
////            RO[19:0]	MC_PROCESSED_TILE_NUM
#define PICK_MC_CORE_MAX_READ_REQUEST(var, val)         CAVY_PICK_VAL(var, val, 31, 20 )
#define PICK_MC_PROCESSED_TILE_NUM(var, val)            CAVY_PICK_VAL(var, val, 19, 0 )
#define GET_MC_CORE_STATUS_1_REG(base_addr, val)        (val = readl(base_addr+REG_TOP_0x0074_MC_CORE_STATUS_1))

#define REG_TOP_0x0078_MC_BATCH_STATUS_0   (0x0078)
////    0x0078	RO		MC_BATCH_STATUS_0
////            RO[31:30]	FRC_MC_BATCH00_DONE_STATUS
////            RO[01:00]	FRC_MC_BATCH15_DONE_STATUS
#define PICK_FRC_MC_BATCH00_DONE_STATUS(var, val)       CAVY_PICK_VAL(var, val, 31, 30 )
#define PICK_FRC_MC_BATCH15_DONE_STATUS(var, val)       CAVY_PICK_VAL(var, val, 1, 0 )
#define GET_MC_BATCH_STATUS_0_REG(base_addr, val)       (val = readl(base_addr+REG_TOP_0x0078_MC_BATCH_STATUS_0))




#define REG_RDMA_CH_BASE(ch)                (0x0400 + 0x80*ch)
//#define REG_RDMA_CH_BASE(1)              (0x0480)
//#define REG_RDMA_CH_BASE(2)              (0x0500)
//#define REG_RDMA_CH_BASE(3)              (0x0580)
//#define REG_RDMA_CH_BASE(4)              (0x0600)
//#define REG_RDMA_CH_BASE(5)              (0x0680)

#define RDMA_FRAME_START           (0x00)
////0x0400	WC		RDMA0_CH0_FRAME_START
////        WC[0]	RDMA0_CH0_FRAME_START
#define SET_RDMA_FRAME_START_REG(base_addr, ch, val)            writel(val, base_addr+REG_RDMA_CH_BASE(ch)+RDMA_FRAME_START)

#define RDMA_APB_READ_SEL           (0x04)
#define RDMA_APB_READ_SEL_FLAG_SHADOW                           (0)
#define RDMA_APB_READ_SEL_FLAG_CORE                             (1)
////0x0404	RW		RDMA0_CH0_APB_READ_SEL
////        RW[0]	RDMA0_CH0_APB_READ_SEL
#define SET_RDMA_APB_READ_SEL_REG(base_addr, ch, val)           writel(val, base_addr+REG_RDMA_CH_BASE(ch)+RDMA_APB_READ_SEL)
#define GET_RDMA_APB_READ_SEL_REG(base_addr, ch, val)           (val = readl(base_addr+REG_RDMA_CH_BASE(ch)+RDMA_APB_READ_SEL))

#define RDMA_STATUS           (0x28)
////0x0428	RO		RDMA0_CH0_STATUS
////        RO[31:16]	RDMA0_CH0_DBG_PIXEL
////        RO[15:0]	RDMA0_CH0_DBG_LINE
#define PICK_RDMA_CHn_DBG_PIXEL(var, val)                       CAVY_PICK_VAL(var, val, 31, 16)
#define PICK_RDMA_CHn_DBG_LINE(var, val)                        CAVY_PICK_VAL(var, val, 15, 0)
#define GET_RDMA_STATUS_REG(base_addr, ch, val)                 (val = readl(base_addr+REG_RDMA_CH_BASE(ch)+RDMA_STATUS))

#define RDMA_IDLE           (0x2C)
////0x042C	RO		RDMA0_CH0_IDLE
////        RO[16]	RDMA0_CH0_DBG_IDLE
#define PICK_RDMA_CHn_IDLE(var, val)                            CAVY_PICK_VAL(var, val, 16, 16)
#define GET_RDMA_IDLE_REG(base_addr, ch, val)                   (val = readl(base_addr+REG_RDMA_CH_BASE(ch)+RDMA_IDLE))

#define RDMA_DBG_STATE           (0x30)
////0x0430	RO		RDMA0_CH0_DBG_STATE
////        RO[18:16]	RDMA0_CH0_DBG_STATE
////        RO[9:0]	RDMA0_CH0_FIFO_LEVEL
#define PICK_RDMA_CHn_DBG_STATE(var, val)                       CAVY_PICK_VAL(var, val, 18, 16)
#define PICK_RDMA_CHn_FIFO_LEVEL(var, val)                      CAVY_PICK_VAL(var, val, 9, 0)
#define GET_RDMA_DBG_STATE_REG(base_addr, ch, val)              (val = readl(base_addr+REG_RDMA_CH_BASE(ch)+RDMA_DBG_STATE))

#define RDMA_BASEADDR           (0x40)
////0x0440	RW		RDMA0_CH0_BASEADDR
////        RW[31:0]	RDMA0_CH0_BASEADDR
#define SET_RDMA_BASEADDR_REG(base_addr, ch, val)           writel(val, base_addr+REG_RDMA_CH_BASE(ch)+RDMA_BASEADDR)
#define GET_RDMA_BASEADDR_REG(base_addr, ch, val)           (val = readl(base_addr+REG_RDMA_CH_BASE(ch)+RDMA_BASEADDR))

#define RDMA_STRIDE           (0x44)
////0x0444	RW		RDMA0_CH0_STRIDE
////        RW[28]	RDMA0_CH0_STRIDE_NEG
////        RW[18:0]	RDMA0_CH0_STRIDE
#define INSERT_RDMA_CHn_STRIDE_NEG(var, val)                CAVY_INSERT_VAL(var, val, 28, 28)
#define INSERT_RDMA_CHn_STRIDE(var, val)                    CAVY_INSERT_VAL(var, val, 18, 0)
#define SET_RDMA_STRIDE_REG(base_addr, ch, val)             writel(val, base_addr+REG_RDMA_CH_BASE(ch)+RDMA_STRIDE)
#define GET_RDMA_STRIDE_REG(base_addr, ch, val)             (val = readl(base_addr+REG_RDMA_CH_BASE(ch)+RDMA_STRIDE))

#define RDMA_CONTROL           (0x4C)
////0x044C	RW		RDMA0_CH0_CONTROL
////        RW[25:16]	RDMA0_CH0_CTRL_FIFO_SIZE
////        RW[9:0]	RDMA0_CH0_CTRL_FIFO_ADDR
#define INSERT_RDMA_CHn_CTRL_FIFO_SIZE(var, val)            CAVY_INSERT_VAL(var, val, 25, 16)
#define INSERT_RDMA_CHn_CTRL_FIFO_ADDR(var, val)            CAVY_INSERT_VAL(var, val, 9, 0)
#define SET_RDMA_CONTROL_REG(base_addr, ch, val)            writel(val, base_addr+REG_RDMA_CH_BASE(ch)+RDMA_CONTROL)
#define GET_RDMA_CONTROL_REG(base_addr, ch, val)            (val = readl(base_addr+REG_RDMA_CH_BASE(ch)+RDMA_CONTROL))

#define RDMA_ERROR_ENABLE           (0x50)
////0x0450	RW		RDMA0_CH0_ERROR_ENABLE
////        RW[15:0]	RDMA0_CH0_ERROR_ENABLE
#define RDMA_ERROR_FLAG_DMA_OTF_ERROR                       (1<<0 )
#define RDMA_ERROR_FLAG_FIFO_OVERFLOW                       (1<<1 )
#define RDMA_ERROR_FLAG_FIFO_UNDERFLOW                      (1<<2 )
#define RDMA_ERROR_FLAG_SBI_ERROR                           (1<<3 )
#define RDMA_ERROR_FLAG_BASE_ADDRESS_STRIDE_ALIGN_ERROR     (1<<7 )
#define RDMA_ERROR_FLAG_FORMAT_ERROR                        (1<<8 )
#define RDMA_ERROR_FLAG_DMA_SETTING_VALUE_CHANGE_ERROR      (1<<12)
#define RDMA_ERROR_FLAG_DMA_START_ERROR                     (1<<13)
#define SET_RDMA_ERROR_ENABLE_REG(base_addr, ch, val)       writel(val, base_addr+REG_RDMA_CH_BASE(ch)+RDMA_ERROR_ENABLE)
#define GET_RDMA_ERROR_ENABLE_REG(base_addr, ch, val)       (val = readl(base_addr+REG_RDMA_CH_BASE(ch)+RDMA_ERROR_ENABLE))

#define RDMA_ERROR_CLEAR           (0x54)
////0x0454	WC		RDMA0_CH0_ERROR_CLEAR
////        WC[15:0]	RDMA0_CH0_ERROR_CLEAR
#define SET_RDMA_ERROR_CLEAR_REG(base_addr, ch, val)        writel(val, base_addr+REG_RDMA_CH_BASE(ch)+RDMA_ERROR_CLEAR)

#define RDMA_ERROR_STATUS           (0x58)
////0x0458	RO		RDMA0_CH0_ERROR_STATUS
////        RO[15:0]	RDMA0_CH0_ERROR_STATUS
#define GET_RDMA_ERROR_STATUS_REG(base_addr, ch, val)       (val = readl(base_addr+REG_RDMA_CH_BASE(ch)+RDMA_ERROR_STATUS))

#define RDMA_OFFSET_ADDR           (0x5C)
////0x045C	RW		RDMA0_CH0_OFFSET_ADDR
////        RW[31:0]	RDMA0_CH0_OFFSET_ADDR
#define SET_RDMA_OFFSET_ADDR_REG(base_addr, ch, val)        writel(val, base_addr+REG_RDMA_CH_BASE(ch)+RDMA_OFFSET_ADDR)
#define GET_RDMA_OFFSET_ADDR_REG(base_addr, ch, val)        (val = readl(base_addr+REG_RDMA_CH_BASE(ch)+RDMA_OFFSET_ADDR))

// special names for CH5_0 and CH5_1
#define RDMA_CH5_BASEADDR_0               (0x40)
////0x06C0	RW		RDMA0_CH5_BASEADDR_0
////        RW[31:0]	RDMA0_CH5_BASEADDR_0
#define SET_RDMA_CH5_BASEADDR_0_REG(base_addr, val)         writel(val, base_addr+REG_RDMA_CH_BASE(5)+RDMA_CH5_BASEADDR_0)
#define GET_RDMA_CH5_BASEADDR_0_REG(base_addr, val)         (val = readl(base_addr+REG_RDMA_CH_BASE(5)+RDMA_CH5_BASEADDR_0))

#define RDMA_CH5_STRIDE_0               (0x44)
////0x06C4	RW		RDMA0_CH5_STRIDE_0
////        RW[28]	RDMA0_CH5_STRIDE_0_NEG
////        RW[18:0]	RDMA0_CH5_STRIDE_0
#define SET_RDMA_CH5_STRIDE_0_REG(base_addr, val)           writel(val, base_addr+REG_RDMA_CH_BASE(5)+RDMA_CH5_STRIDE_0)
#define GET_RDMA_CH5_STRIDE_0_REG(base_addr, val)           (val = readl(base_addr+REG_RDMA_CH_BASE(5)+RDMA_CH5_STRIDE_0))

#define RDMA_CH5_BASEADDR_1               (0x48)
////0x06C8	RW		RDMA0_CH5_BASEADDR_1
////        RW[31:0]	RDMA0_CH5_BASEADDR_1
#define SET_RDMA_CH5_BASEADDR_1_REG(base_addr, val)         writel(val, base_addr+REG_RDMA_CH_BASE(5)+RDMA_CH5_BASEADDR_1)
#define GET_RDMA_CH5_BASEADDR_1_REG(base_addr, val)         (val = readl(base_addr+REG_RDMA_CH_BASE(5)+RDMA_CH5_BASEADDR_1))

#define RDMA_CH5_STRIDE_1               (0x4C)
////0x06CC	RW		RDMA0_CH5_STRIDE_1
////        RW[28]	RDMA0_CH5_STRIDE_1_NEG
////        RW[18:0]	RDMA0_CH5_STRIDE_1
#define SET_RDMA_CH5_STRIDE_1_REG(base_addr, val)           writel(val, base_addr+REG_RDMA_CH_BASE(5)+RDMA_CH5_STRIDE_1)
#define GET_RDMA_CH5_STRIDE_1_REG(base_addr, val)           (val = readl(base_addr+REG_RDMA_CH_BASE(5)+RDMA_CH5_STRIDE_1))

#define RDMA_CH5_CONTROL_01               (0x50)
////0x06D0	RW		RDMA0_CH5_CONTROL
////        RW[25:16]	RDMA0_CH5_CTRL_FIFO_SIZE
////        RW[9:0]	RDMA0_CH5_CTRL_FIFO_ADDR
#define SET_RDMA_CH5_CONTROL_01_REG(base_addr, val)         writel(val, base_addr+REG_RDMA_CH_BASE(5)+RDMA_CH5_CONTROL_01)
#define GET_RDMA_CH5_CONTROL_01_REG(base_addr, val)         (val = readl(base_addr+REG_RDMA_CH_BASE(5)+RDMA_CH5_CONTROL_01))

#define RDMA_CH5_ERROR_ENABLE_01               (0x54)
////0x06D4	RW		RDMA0_CH5_ERROR_ENABLE
////        RW[15:0]	RDMA0_CH5_ERROR_ENABLE
#define SET_RDMA_CH5_ERROR_ENABLE_01_REG(base_addr, val)    writel(val, base_addr+REG_RDMA_CH_BASE(5)+RDMA_CH5_ERROR_ENABLE_01)
#define GET_RDMA_CH5_ERROR_ENABLE_01_REG(base_addr, val)    (val = readl(base_addr+REG_RDMA_CH_BASE(5)+RDMA_CH5_ERROR_ENABLE_01))

#define RDMA_CH5_ERROR_CLEAR_01               (0x58)
////0x06D8	WC		RDMA0_CH5_ERROR_CLEAR
////        WC[15:0]	RDMA0_CH5_ERROR_CLEAR
#define SET_RDMA0_CH5_ERROR_CLEAR_REG(base_addr, val)       writel(val, base_addr+REG_RDMA_CH_BASE(5)+RDMA_CH5_ERROR_CLEAR_01)

#define RDMA_CH5_ERROR_STATUS_01               (0x5C)
////0x06DC	RO		RDMA0_CH5_ERROR_STATUS
////        RO[15:0]	RDMA0_CH5_ERROR_STATUS
#define GET_RDMA_CH5_ERROR_STATUS_01_REG(base_addr, val)    (val = readl(base_addr+REG_RDMA_CH_BASE(5)+RDMA_CH5_ERROR_STATUS_01))

#define RDMA_CH5_OFFSET_ADDR_0               (0x60)
////0x06E0	RW		RDMA0_CH5_OFFSET_ADDR_0
////        RW[31:0]	RDMA0_CH5_OFFSET_ADDR_0
#define SET_RDMA_CH5_OFFSET_ADDR_0(base_addr, val)          writel(val, base_addr+REG_RDMA_CH_BASE(5)+RDMA_CH5_OFFSET_ADDR_0)
#define GET_RDMA_CH5_OFFSET_ADDR_0(base_addr, val)          (val = readl(base_addr+REG_RDMA_CH_BASE(5)+RDMA_CH5_OFFSET_ADDR_0))

#define RDMA_CH5_OFFSET_ADDR_1               (0x64)
////0x06E4	RW		RDMA0_CH5_OFFSET_ADDR_1
////        RW[31:0]	RDMA0_CH5_OFFSET_ADDR_1
#define SET_RDMA_CH5_OFFSET_ADDR_1(base_addr, val)          writel(val, base_addr+REG_RDMA_CH_BASE(5)+RDMA_CH5_OFFSET_ADDR_1)
#define GET_RDMA_CH5_OFFSET_ADDR_1(base_addr, val)          (val = readl(base_addr+REG_RDMA_CH_BASE(5)+RDMA_CH5_OFFSET_ADDR_1))



#define REG_WDMA_CH_BASE(ch)              (0x0800 + 0x80*ch)
//#define REG_RDMA_CH_BASE(1)              (0x0880)
//#define REG_RDMA_CH_BASE(2)              (0x0900)
//#define REG_RDMA_CH_BASE(3)              (0x0980)

#define WDMA_FRAME_START           (0x00)
////0x0800	WC		WDMA0_CH0_FRAME_START
////        WC[0]	WDMA0_CH0_FRAME_START
#define SET_WDMA_FRAME_START_REG(base_addr, ch, val)            writel(val, base_addr+REG_WDMA_CH_BASE(ch)+WDMA_FRAME_START)

#define WDMA_TILE_CTRL_FRAME_SIZE           (0x04)
////0x0804	RW		WDMA0_CH0_TILE_CTRL_FRAME_SIZE
////        RW[31:16]	WDMA0_CH0_FRAME_SIZE_H
////        RW[15:0]	WDMA0_CH0_FRAME_SIZE_V
#define INSERT_WDMA_CHn_FRAME_SIZE_H(var, val)                  CAVY_INSERT_VAL(var, val, 31, 16)
#define INSERT_WDMA_CHn_FRAME_SIZE_V(var, val)                  CAVY_INSERT_VAL(var, val, 15, 0)
#define SET_WDMA_TILE_CTRL_FRAME_SIZE_REG(base_addr, ch, val)   writel(val, base_addr+REG_WDMA_CH_BASE(ch)+WDMA_TILE_CTRL_FRAME_SIZE)
#define GET_WDMA_TILE_CTRL_FRAME_SIZE_REG(base_addr, ch, val)   (val = readl(base_addr+REG_WDMA_CH_BASE(ch)+WDMA_TILE_CTRL_FRAME_SIZE))

#define WDMA_TILE_CTRL_FRAME_START           (0x08)
////0x0808	RW		WDMA0_CH0_TILE_CTRL_FRAME_START
////        RW[31:16]	WDMA0_CH0_FRAME_START_X
////        RW[15:0]	WDMA0_CH0_FRAME_START_Y
#define INSERT_WDMA_CHn_FRAME_START_X(var, val)                 CAVY_INSERT_VAL(var, val, 31, 16)
#define INSERT_WDMA_CHn_FRAME_START_Y(var, val)                 CAVY_INSERT_VAL(var, val, 15, 0)
#define SET_WDMA_TILE_CTRL_FRAME_START_REG(base_addr, ch, val)  writel(val, base_addr+REG_WDMA_CH_BASE(ch)+WDMA_TILE_CTRL_FRAME_START)
#define GET_WDMA_TILE_CTRL_FRAME_START_REG(base_addr, ch, val)  (val = readl(base_addr+REG_WDMA_CH_BASE(ch)+WDMA_TILE_CTRL_FRAME_START))

#define WDMA_TILE_CTRL_TILE_SIZE           (0x0C)
////0x080C	RW		WDMA0_CH0_TILE_CTRL_TILE_SIZE
////        RW[31:16]	WDMA0_CH0_TILE_SIZE_H
////        RW[15:0]	WDMA0_CH0_TILE_SIZE_V
#define INSERT_WDMA_CHn_TILE_SIZE_H(var, val)                   CAVY_INSERT_VAL(var, val, 31, 16)
#define INSERT_WDMA_CHn_TILE_SIZE_V(var, val)                   CAVY_INSERT_VAL(var, val, 15, 0)
#define SET_WDMA_TILE_CTRL_TILE_SIZE_REG(base_addr, ch, val)    writel(val, base_addr+REG_WDMA_CH_BASE(ch)+WDMA_TILE_CTRL_TILE_SIZE)
#define GET_WDMA_TILE_CTRL_TILE_SIZE_REG(base_addr, ch, val)    (val = readl(base_addr+REG_WDMA_CH_BASE(ch)+WDMA_TILE_CTRL_TILE_SIZE))

#define WDMA_TILE_CTRL_TILE_NUM           (0x10)
////0x0810	RW		WDMA0_CH0_TILE_CTRL_TILE_NUM
////        RW[25:16]	WDMA0_CH0_TILE_NUM_H
////        RW[9:0]	WDMA0_CH0_TILE_NUM_V
#define INSERT_WDMA_CHn_TILE_NUM_H(var, val)                    CAVY_INSERT_VAL(var, val, 25, 16)
#define INSERT_WDMA_CHn_TILE_NUM_V(var, val)                    CAVY_INSERT_VAL(var, val, 9, 0)
#define SET_WDMA_TILE_CTRL_TILE_NUM_REG(base_addr, ch, val)     writel(val, base_addr+REG_WDMA_CH_BASE(ch)+WDMA_TILE_CTRL_TILE_NUM)
#define GET_WDMA_TILE_CTRL_TILE_NUM_REG(base_addr, ch, val)     (val = readl(base_addr+REG_WDMA_CH_BASE(ch)+WDMA_TILE_CTRL_TILE_NUM))

#define WDMA_APB_READ_SEL           (0x20)
////0x0820	RW		WDMA0_CH0_APB_READ_SEL
////        RW[0]	WDMA0_CH0_APB_READ_SEL
#define WDMA_APB_READ_SEL_FLAG_SHADOW                           (0)
#define WDMA_APB_READ_SEL_FLAG_CORE                             (1)
#define SET_WDMA_APB_READ_SEL_REG(base_addr, ch, val)           writel(val, base_addr+REG_WDMA_CH_BASE(ch)+WDMA_APB_READ_SEL)
#define GET_WDMA_APB_READ_SEL_REG(base_addr, ch, val)           (val = readl(base_addr+REG_WDMA_CH_BASE(ch)+WDMA_APB_READ_SEL))

#define WDMA_TILE_CTRL_DBG           (0x28)
////0x0828	RO		WDMA0_CH0_TILE_CTRL_DBG
////        RO[31:16]	WDMA0_CH0_DBG_PIXEL
////        RO[15:0]	WDMA0_CH0_DBG_LINE
#define PICK_WDMA_CHn_DBG_PIXEL(var, val)                       CAVY_PICK_VAL(var, val, 31, 16)
#define PICK_WDMA_CHn_DBG_LINE(var, val)                        CAVY_PICK_VAL(var, val, 15, 0)
#define GET_WDMA_TILE_CTRL_DBG_REG(base_addr, ch, val)          (val = readl(base_addr+REG_WDMA_CH_BASE(ch)+WDMA_TILE_CTRL_DBG))

#define WDMA_TILE_CTRL_DBG_TILE           (0x2C)
////0x082C	RO		WDMA0_CH0_TILE_CTRL_DBG_TILE
////        RO[20]	    WDMA0_CH0_DBG_IDLE
////        RO[19:10]	WDMA0_CH0_DBG_TILE_H
////        RO[9:0]	    WDMA0_CH0_DBG_TILE_V
#define PICK_WDMA_CHn_DBG_IDLE(var, val)                        CAVY_PICK_VAL(var, val, 20, 20)
#define PICK_WDMA_CHn_DBG_TILE_H(var, val)                      CAVY_PICK_VAL(var, val, 19, 10)
#define PICK_WDMA_CHn_DBG_TILE_V(var, val)                      CAVY_PICK_VAL(var, val, 9, 0)
#define GET_WDMA_TILE_CTRL_DBG_TILE_REG(base_addr, ch, val)     (val = readl(base_addr+REG_WDMA_CH_BASE(ch)+WDMA_TILE_CTRL_DBG_TILE))

#define WDMA_DBG_STATE           (0x30)
////0x0830	RO		WDMA0_CH0_DBG_STATE
////        RO[18:16]	WDMA0_CH0_DBG_STATE
////        RO[9:0]	    WDMA0_CH0_DBG_FIFO_LEVEL
#define PICK_WDMA_CHn_DBG_STATE(var, val)                       CAVY_PICK_VAL(var, val, 18, 16)
#define PICK_WDMA_CHn_DBG_FIFO_LEVEL(var, val)                  CAVY_PICK_VAL(var, val, 9, 0)
#define GET_WDMA_DBG_STATE_REG(base_addr, ch, val)              (val = readl(base_addr+REG_WDMA_CH_BASE(ch)+WDMA_DBG_STATE))

#define WDMA_BASEADDR           (0x40)
////0x0840	RW		WDMA0_CH0_BASEADDR
////        RW[31:0]	WDMA0_CH0_BASEADDR
#define SET_WDMA_BASEADDR_REG(base_addr, ch, val)               writel(val, base_addr+REG_WDMA_CH_BASE(ch)+WDMA_BASEADDR)
#define GET_WDMA_BASEADDR_REG(base_addr, ch, val)               (val = readl(base_addr+REG_WDMA_CH_BASE(ch)+WDMA_BASEADDR))

#define WDMA_STRIDE           (0x44)
////0x0844	RW		WDMA0_CH0_STRIDE
////        RW[28]	    WDMA0_CH0_STRIDE_NEG
////        RW[18:0]	WDMA0_CH0_STRIDE
#define INSERT_WDMA_CHn_STRIDE_NEG(var, val)                    CAVY_INSERT_VAL(var, val, 28, 28)
#define INSERT_WDMA_CHn_STRIDE(var, val)                        CAVY_INSERT_VAL(var, val, 18, 0)
#define SET_WDMA_STRIDE_REG(base_addr, ch, val)                 writel(val, base_addr+REG_WDMA_CH_BASE(ch)+WDMA_STRIDE)
#define GET_WDMA_STRIDE_REG(base_addr, ch, val)                 (val = readl(base_addr+REG_WDMA_CH_BASE(ch)+WDMA_STRIDE))

#define WDMA_FORMAT           (0x48)
////0x0848	RW		WDMA0_CH0_FORMAT
////        RW[9:8]	WDMA0_CH0_FMT_PPC
////        RW[6:0]	WDMA0_CH0_FMT_BPP
#define WDMA_FORMAT_FLAG_1PPC                                   (0)
#define WDMA_FORMAT_FLAG_2PPC                                   (1)
#define WDMA_FORMAT_FLAG_4PPC                                   (2)
#define INSERT_WDMA0_CH_FMT_PPC(var, val)                       CAVY_INSERT_VAL(var, val, 9, 8)
#define INSERT_WDMA0_CH_FMT_BPP(var, val)                       CAVY_INSERT_VAL(var, val, 6, 0)
#define SET_WDMA_FORMAT_REG(base_addr, ch, val)                 writel(val, base_addr+REG_WDMA_CH_BASE(ch)+WDMA_FORMAT)
#define GET_WDMA_FORMAT_REG(base_addr, ch, val)                 (val = readl(base_addr+REG_WDMA_CH_BASE(ch)+WDMA_FORMAT))

#define WDMA_CONTROL           (0x4C)
////0x084C	RW		WDMA0_CH0_CONTROL
////        RW[25:16]	WDMA0_CH0_CTRL_FIFO_SIZE
////        RW[9:0]	    WDMA0_CH0_CTRL_FIFO_ADDR
#define INSERT_WDMA_CHn_CTRL_FIFO_SIZE(var, val)                CAVY_INSERT_VAL(var, val, 25, 16)
#define INSERT_WDMA_CHn_CTRL_FIFO_ADDR(var, val)                CAVY_INSERT_VAL(var, val, 9, 0)
#define SET_WDMA_CONTROL_REG(base_addr, ch, val)                writel(val, base_addr+REG_WDMA_CH_BASE(ch)+WDMA_CONTROL)
#define GET_WDMA_CONTROL_REG(base_addr, ch, val)                (val = readl(base_addr+REG_WDMA_CH_BASE(ch)+WDMA_CONTROL))

#define WDMA_ERROR_ENABLE           (0x50)
////0x0850	RW		WDMA0_CH0_ERROR_ENABLE
////        RW[15:0]	WDMA0_CH0_ERROR_ENABLE
#define WDMA_ERROR_FLAG_DMA_OTF_ERROR                           (1<<0 )
#define WDMA_ERROR_FLAG_FIFO_OVERFLOW                           (1<<1 )
#define WDMA_ERROR_FLAG_FIFO_UNDERFLOW                          (1<<2 )
#define WDMA_ERROR_FLAG_SBI_ERROR                               (1<<3 )
#define WDMA_ERROR_FLAG_DXI_EOL ERROR                           (1<<4 )
#define WDMA_ERROR_FLAG_DXI_EOT ERROR                           (1<<5 )
#define WDMA_ERROR_FLAG_2PPC_4PPC_SETTING_VALUE_ERROR           (1<<6 )
#define WDMA_ERROR_FLAG_BASE_ADDRESS_STRIDE_ALIGN_ERROR         (1<<7 )
#define WDMA_ERROR_FLAG_FORMAT_ERROR                            (1<<8 )
#define WDMA_ERROR_FLAG_YC420_LINE_ERROR                        (1<<9 )
#define WDMA_ERROR_FLAG_WRITE_BIT_OFFSET_ERROR                  (1<<11)
#define WDMA_ERROR_FLAG_DDMA_SETTING_VALUE_CHANGE_ERROR         (1<<12)
#define WDMA_ERROR_FLAG_DDMA_START_ERROR                        (1<<13)
#define SET_WDMA_ERROR_ENABLE_REG(base_addr, ch, val)           writel(val, base_addr+REG_WDMA_CH_BASE(ch)+WDMA_ERROR_ENABLE)
#define GET_WDMA_ERROR_ENABLE_REG(base_addr, ch, val)           (val = readl(base_addr+REG_WDMA_CH_BASE(ch)+WDMA_ERROR_ENABLE))

#define WDMA_ERROR_CLEAR           (0x54)
////0x0854	WC		WDMA0_CH0_ERROR_CLEAR
////        WC[15:0]	WDMA0_CH0_ERROR_CLEAR
#define SET_WDMA_ERROR_CLEAR_REG(base_addr, ch, val)                writel(val, base_addr+REG_WDMA_CH_BASE(ch)+WDMA_ERROR_CLEAR)

#define WDMA_ERROR_STATUS           (0x58)
////0x0858	RO		WDMA0_CH0_ERROR_STATUS
////        RO[15:0]	WDMA0_CH0_ERROR_STATUS
#define GET_WDMA_ERROR_STATUS_REG(base_addr, ch, val)                (val = readl(base_addr+REG_WDMA_CH_BASE(ch)+WDMA_ERROR_STATUS))

#define WDMA_OFFSET_ADDR           (0x5C)
////0x085C	RW		WDMA0_CH0_OFFSET_ADDR
////        RW[31:0]	WDMA0_CH0_OFFSET_ADDR
#define SET_WDMA_OFFSET_ADDR_REG(base_addr, ch, val)           writel(val, base_addr+REG_WDMA_CH_BASE(ch)+WDMA_OFFSET_ADDR)
#define GET_WDMA_OFFSET_ADDR_REG(base_addr, ch, val)           (val = readl(base_addr+REG_WDMA_CH_BASE(ch)+WDMA_OFFSET_ADDR))



#ifdef DEBUG
static inline void mcfrc_print_all_regs(struct mcfrc_dev *mcfrc, void __iomem *base_addr)
{
	unsigned int val, ch;
	// TOP
	dev_dbg(mcfrc->dev, "%s: TOP:    %04X: 0x%08x\n", __func__, REG_TOP_0x0008_MC_SW_RESET, GET_MC_SW_RESET_REG(base_addr, val));
	dev_dbg(mcfrc->dev, "%s: TOP:    %04X: 0x%08x\n", __func__, REG_TOP_0x0010_MC_TIMEOUT, GET_MC_TIMEOUT_REG(base_addr, val));
	dev_dbg(mcfrc->dev, "%s: TOP:    %04X: 0x%08x\n", __func__, REG_TOP_0x0014_MC_CLKREQ_CONTROL, GET_MC_CLKREQ_CONTROL_REG(base_addr, val));
	dev_dbg(mcfrc->dev, "%s: TOP:    %04X: 0x%08x\n", __func__, REG_TOP_0x0018_MC_FRAME_SIZE, GET_MC_FRAME_SIZE_REG(base_addr, val));
	dev_dbg(mcfrc->dev, "%s: TOP:    %04X: 0x%08x\n", __func__, REG_TOP_0x001C_MC_BYPASS_PHASE, GET_MC_BYPASS_PHASE_REG(base_addr, val));
	dev_dbg(mcfrc->dev, "%s: TOP:    %04X: 0x%08x\n", __func__, REG_TOP_0x0020_MC_BATCH_MODE, GET_MC_BATCH_MODE_REG(base_addr, val));
	dev_dbg(mcfrc->dev, "%s: TOP:    %04X: 0x%08x\n", __func__, REG_TOP_0x0024_MC_FRAME_DONE_CHECK, GET_MC_FRAME_DONE_CHECK_REG(base_addr, val));
	dev_dbg(mcfrc->dev, "%s: TOP:    %04X: 0x%08x\n", __func__, REG_TOP_0x0028_MC_INTR_MASKING, GET_MC_INTR_MASKING_REG(base_addr, val));
	dev_dbg(mcfrc->dev, "%s: TOP:    %04X: 0x%08x\n", __func__, REG_TOP_0x0034_FRC_DBL_EN, GET_FRC_DBL_EN_REG(base_addr, val));
	dev_dbg(mcfrc->dev, "%s: TOP:    %04X: 0x%08x\n", __func__, REG_TOP_0x0038_FRC_DBL_MODE, GET_FRC_DBL_MODE_REG(base_addr, val));
	dev_dbg(mcfrc->dev, "%s: TOP:    %04X: 0x%08x\n", __func__, REG_TOP_0x003C_DMA_SBI_RD_CONFIG, GET_DMA_SBI_RD_CONFIG_REG(base_addr, val));
	dev_dbg(mcfrc->dev, "%s: TOP:    %04X: 0x%08x\n", __func__, REG_TOP_0x0040_DMA_SBI_RD_STATUS, GET_DMA_SBI_RD_STATUS_REG(base_addr, val));
	dev_dbg(mcfrc->dev, "%s: TOP:    %04X: 0x%08x\n", __func__, REG_TOP_0x0044_DMA_SBI_WR_CONFIG, GET_DMA_SBI_WR_CONFIG_REG(base_addr, val));
	dev_dbg(mcfrc->dev, "%s: TOP:    %04X: 0x%08x\n", __func__, REG_TOP_0x0048_DMA_SBI_WR_STATUS, GET_DMA_SBI_WR_STATUS_REG(base_addr, val));
	dev_dbg(mcfrc->dev, "%s: TOP:    %04X: 0x%08x\n", __func__, REG_TOP_0x004C_MC_AXI_USER, GET_MC_AXI_USER_REG(base_addr, val));
	dev_dbg(mcfrc->dev, "%s: TOP:    %04X: 0x%08x\n", __func__, REG_TOP_0x0050_DMA_SW_RESET, GET_DMA_SW_RESET_REG(base_addr, val));
	dev_dbg(mcfrc->dev, "%s: TOP:    %04X: 0x%08x\n", __func__, REG_TOP_0x0054_INTR_MASK_DBG_STATUS, GET_INTR_MASK_DBG_STATUS_REG(base_addr, val));
	dev_dbg(mcfrc->dev, "%s: TOP:    %04X: 0x%08x\n", __func__, REG_TOP_0x0058_FRC_MC_DONE_IRQ_STATUS, GET_FRC_MC_DONE_IRQ_STATUS_REG(base_addr, val));
	dev_dbg(mcfrc->dev, "%s: TOP:    %04X: 0x%08x\n", __func__, REG_TOP_0x005C_FRC_MC_DBL_ERR_IRQ_STATUS, GET_FRC_MC_DBL_ERR_IRQ_STATUS_REG(base_addr, val));
	dev_dbg(mcfrc->dev, "%s: TOP:    %04X: 0x%08x\n", __func__, REG_TOP_0x0064_MC_ERROR_DEBUG_ENABLE, GET_MC_ERROR_DEBUG_ENABLE_REG(base_addr, val));
	dev_dbg(mcfrc->dev, "%s: TOP:    %04X: 0x%08x\n", __func__, REG_TOP_0x006C_MC_ERROR_STATUS, GET_MC_ERROR_STATUS_REG(base_addr, val));
	dev_dbg(mcfrc->dev, "%s: TOP:    %04X: 0x%08x\n", __func__, REG_TOP_0x0070_MC_CORE_STATUS_0, GET_MC_CORE_STATUS_0_REG(base_addr, val));
	dev_dbg(mcfrc->dev, "%s: TOP:    %04X: 0x%08x\n", __func__, REG_TOP_0x0074_MC_CORE_STATUS_1, GET_MC_CORE_STATUS_1_REG(base_addr, val));
	dev_dbg(mcfrc->dev, "%s: TOP:    %04X: 0x%08x\n", __func__, REG_TOP_0x0078_MC_BATCH_STATUS_0, GET_MC_BATCH_STATUS_0_REG(base_addr, val));

	// RDMA
	for (ch = 0; ch <= 5; ch++) {
		dev_dbg(mcfrc->dev, "%s: RDMA(%d): %04X: 0x%08x\n", __func__, ch, REG_RDMA_CH_BASE(ch) + RDMA_APB_READ_SEL, GET_RDMA_APB_READ_SEL_REG(base_addr, ch, val));
		dev_dbg(mcfrc->dev, "%s: RDMA(%d): %04X: 0x%08x\n", __func__, ch, REG_RDMA_CH_BASE(ch) + RDMA_STATUS, GET_RDMA_STATUS_REG(base_addr, ch, val));
		dev_dbg(mcfrc->dev, "%s: RDMA(%d): %04X: 0x%08x\n", __func__, ch, REG_RDMA_CH_BASE(ch) + RDMA_IDLE, GET_RDMA_IDLE_REG(base_addr, ch, val));
		dev_dbg(mcfrc->dev, "%s: RDMA(%d): %04X: 0x%08x\n", __func__, ch, REG_RDMA_CH_BASE(ch) + RDMA_DBG_STATE, GET_RDMA_DBG_STATE_REG(base_addr, ch, val));
		dev_dbg(mcfrc->dev, "%s: RDMA(%d): %04X: 0x%08x\n", __func__, ch, REG_RDMA_CH_BASE(ch) + RDMA_BASEADDR, GET_RDMA_BASEADDR_REG(base_addr, ch, val));
		dev_dbg(mcfrc->dev, "%s: RDMA(%d): %04X: 0x%08x\n", __func__, ch, REG_RDMA_CH_BASE(ch) + RDMA_STRIDE, GET_RDMA_STRIDE_REG(base_addr, ch, val));
		if (5 != ch) {
			dev_dbg(mcfrc->dev, "%s: RDMA(%d): %04X: 0x%08x\n", __func__, ch, REG_RDMA_CH_BASE(ch) + RDMA_CONTROL, GET_RDMA_CONTROL_REG(base_addr, ch, val));
			dev_dbg(mcfrc->dev, "%s: RDMA(%d): %04X: 0x%08x\n", __func__, ch, REG_RDMA_CH_BASE(ch) + RDMA_ERROR_ENABLE, GET_RDMA_ERROR_ENABLE_REG(base_addr, ch, val));
			dev_dbg(mcfrc->dev, "%s: RDMA(%d): %04X: 0x%08x\n", __func__, ch, REG_RDMA_CH_BASE(ch) + RDMA_ERROR_STATUS, GET_RDMA_ERROR_STATUS_REG(base_addr, ch, val));
			dev_dbg(mcfrc->dev, "%s: RDMA(%d): %04X: 0x%08x\n", __func__, ch, REG_RDMA_CH_BASE(ch) + RDMA_OFFSET_ADDR, GET_RDMA_OFFSET_ADDR_REG(base_addr, ch, val));
		}
	}
	ch = 5;
	dev_dbg(mcfrc->dev, "%s: RDMA(%d): %04X: 0x%08x\n", __func__, ch, REG_RDMA_CH_BASE(5) + RDMA_CH5_BASEADDR_1, GET_RDMA_CH5_BASEADDR_1_REG(base_addr, val));
	dev_dbg(mcfrc->dev, "%s: RDMA(%d): %04X: 0x%08x\n", __func__, ch, REG_RDMA_CH_BASE(5) + RDMA_CH5_STRIDE_1, GET_RDMA_CH5_STRIDE_1_REG(base_addr, val));
	dev_dbg(mcfrc->dev, "%s: RDMA(%d): %04X: 0x%08x\n", __func__, ch, REG_RDMA_CH_BASE(5) + RDMA_CH5_CONTROL_01, GET_RDMA_CH5_CONTROL_01_REG(base_addr, val));
	dev_dbg(mcfrc->dev, "%s: RDMA(%d): %04X: 0x%08x\n", __func__, ch, REG_RDMA_CH_BASE(5) + RDMA_CH5_ERROR_ENABLE_01, GET_RDMA_CH5_ERROR_ENABLE_01_REG(base_addr, val));
	dev_dbg(mcfrc->dev, "%s: RDMA(%d): %04X: 0x%08x\n", __func__, ch, REG_RDMA_CH_BASE(5) + RDMA_CH5_ERROR_STATUS_01, GET_RDMA_CH5_ERROR_STATUS_01_REG(base_addr, val));
	dev_dbg(mcfrc->dev, "%s: RDMA(%d): %04X: 0x%08x\n", __func__, ch, REG_RDMA_CH_BASE(5) + RDMA_CH5_OFFSET_ADDR_0, GET_RDMA_CH5_OFFSET_ADDR_0(base_addr, val));
	dev_dbg(mcfrc->dev, "%s: RDMA(%d): %04X: 0x%08x\n", __func__, ch, REG_RDMA_CH_BASE(5) + RDMA_CH5_OFFSET_ADDR_1, GET_RDMA_CH5_OFFSET_ADDR_1(base_addr, val));

	// WDMA
	for (ch = 0; ch <= 3; ch++) {
		dev_dbg(mcfrc->dev, "%s: WDMA(%d): %04X: 0x%08x\n", __func__, ch, REG_WDMA_CH_BASE(ch) + WDMA_TILE_CTRL_FRAME_SIZE, GET_WDMA_TILE_CTRL_FRAME_SIZE_REG(base_addr, ch, val));
		dev_dbg(mcfrc->dev, "%s: WDMA(%d): %04X: 0x%08x\n", __func__, ch, REG_WDMA_CH_BASE(ch) + WDMA_TILE_CTRL_FRAME_START, GET_WDMA_TILE_CTRL_FRAME_START_REG(base_addr, ch, val));
		dev_dbg(mcfrc->dev, "%s: WDMA(%d): %04X: 0x%08x\n", __func__, ch, REG_WDMA_CH_BASE(ch) + WDMA_TILE_CTRL_TILE_SIZE, GET_WDMA_TILE_CTRL_TILE_SIZE_REG(base_addr, ch, val));
		dev_dbg(mcfrc->dev, "%s: WDMA(%d): %04X: 0x%08x\n", __func__, ch, REG_WDMA_CH_BASE(ch) + WDMA_TILE_CTRL_TILE_NUM, GET_WDMA_TILE_CTRL_TILE_NUM_REG(base_addr, ch, val));
		dev_dbg(mcfrc->dev, "%s: WDMA(%d): %04X: 0x%08x\n", __func__, ch, REG_WDMA_CH_BASE(ch) + WDMA_APB_READ_SEL, GET_WDMA_APB_READ_SEL_REG(base_addr, ch, val));
		dev_dbg(mcfrc->dev, "%s: WDMA(%d): %04X: 0x%08x\n", __func__, ch, REG_WDMA_CH_BASE(ch) + WDMA_TILE_CTRL_DBG, GET_WDMA_TILE_CTRL_DBG_REG(base_addr, ch, val));
		dev_dbg(mcfrc->dev, "%s: WDMA(%d): %04X: 0x%08x\n", __func__, ch, REG_WDMA_CH_BASE(ch) + WDMA_TILE_CTRL_DBG_TILE, GET_WDMA_TILE_CTRL_DBG_TILE_REG(base_addr, ch, val));
		dev_dbg(mcfrc->dev, "%s: WDMA(%d): %04X: 0x%08x\n", __func__, ch, REG_WDMA_CH_BASE(ch) + WDMA_DBG_STATE, GET_WDMA_DBG_STATE_REG(base_addr, ch, val));
		dev_dbg(mcfrc->dev, "%s: WDMA(%d): %04X: 0x%08x\n", __func__, ch, REG_WDMA_CH_BASE(ch) + WDMA_BASEADDR, GET_WDMA_BASEADDR_REG(base_addr, ch, val));
		dev_dbg(mcfrc->dev, "%s: WDMA(%d): %04X: 0x%08x\n", __func__, ch, REG_WDMA_CH_BASE(ch) + WDMA_STRIDE, GET_WDMA_STRIDE_REG(base_addr, ch, val));
		dev_dbg(mcfrc->dev, "%s: WDMA(%d): %04X: 0x%08x\n", __func__, ch, REG_WDMA_CH_BASE(ch) + WDMA_FORMAT, GET_WDMA_FORMAT_REG(base_addr, ch, val));
		dev_dbg(mcfrc->dev, "%s: WDMA(%d): %04X: 0x%08x\n", __func__, ch, REG_WDMA_CH_BASE(ch) + WDMA_CONTROL, GET_WDMA_CONTROL_REG(base_addr, ch, val));
		dev_dbg(mcfrc->dev, "%s: WDMA(%d): %04X: 0x%08x\n", __func__, ch, REG_WDMA_CH_BASE(ch) + WDMA_ERROR_ENABLE, GET_WDMA_ERROR_ENABLE_REG(base_addr, ch, val));
		dev_dbg(mcfrc->dev, "%s: WDMA(%d): %04X: 0x%08x\n", __func__, ch, REG_WDMA_CH_BASE(ch) + WDMA_ERROR_STATUS, GET_WDMA_ERROR_STATUS_REG(base_addr, ch, val));
		dev_dbg(mcfrc->dev, "%s: WDMA(%d): %04X: 0x%08x\n", __func__, ch, REG_WDMA_CH_BASE(ch) + WDMA_OFFSET_ADDR, GET_WDMA_OFFSET_ADDR_REG(base_addr, ch, val));
	}
}
#endif



/*
// SFR
#define     REG_0_Y_ADDR                0x000
#define     REG_1_U_ADDR                0x004
#define     REG_2_V_ADDR                0x008
#define     REG_3_INPUT_SIZE            0x00C
#define     REG_4_INPUT_TYPE            0x010
#define     REG_5_OP_MODE               0x014
#define     REG_6_CONFIG_DC             0x018

#define     REG_7_TO_22_Y_Q_MAT         0x01C
#define     REG_23_TO_38_C_Q_MAT        0x05C

#define     REG_39_SW_RESET             0x09C
#define     REG_40_INTERRUPT_EN         0x0A0
#define     REG_41_INTERRUPT_CLEAR      0x0A4
#define     REG_42_MMSQZ_HW_START       0x0A8
#define     REG_43_MMSQZ_HW_DONE        0x0AC
#define     REG_44_CONFIG_STRIDE        0x0B0
#define     REG_45_CONFIG_TIMEOUT       0x0B4
#define     REG_46_HW_DEBUG             0x0B8
#define     REG_47_ERROR_FLAG           0x0BC

#define     REG_48_TO_63_INIT_Y_Q		0x0C0
#define     REG_64_TO_79_INIT_C_Q		0x100

#define     REG_80_VELOCITY             0x140
#define     REG_81_TUNE_DC              0x144
#define     REG_82_TUNE_ALPHA           0x148
#define     REG_83_TUNE_DQP             0x14C
#define     REG_84_FRM_AVG_DQP          0x150
#define     REG_85_DQP_ADDR             0x154
*/
// define APIs
/*
static inline void jsqz_sw_reset(void __iomem *base)
{
	writel(0x0, base + REG_39_SW_RESET);
	writel(0x1, base + REG_39_SW_RESET);
}

static inline void jsqz_interrupt_enable(void __iomem *base)
{
	writel(0x1, base + REG_40_INTERRUPT_EN);
	//writel(0x0, base + REG_41_INTERRUPT_CLEAR);
}

static inline void jsqz_interrupt_disable(void __iomem *base)
{
	writel(0x0, base + REG_40_INTERRUPT_EN);
}

static inline void jsqz_interrupt_clear(void __iomem *base)
{
	writel(0x1, base + REG_41_INTERRUPT_CLEAR);
}

static inline u32 jsqz_get_interrupt_status(void __iomem *base)
{
	return readl(base + REG_41_INTERRUPT_CLEAR);
}

static inline void jsqz_hw_start(void __iomem *base)
{
	writel(0x1, base + REG_42_MMSQZ_HW_START);
}

static inline u32 jsqz_check_done(void __iomem *base)
{
	return readl(base + REG_43_MMSQZ_HW_DONE) & 0x1;
}
static inline void jsqz_on_off_time_out(void __iomem *base, u32 time)
{
	u32 sfr = 0;
	if (time == 0)
		writel(0x0, base + REG_45_CONFIG_TIMEOUT);
	else {
		sfr = (time * 533) >> 7;
		writel((sfr | 0x00020000), base + REG_45_CONFIG_TIMEOUT);
	}
}

static inline void jsqz_set_stride_on_n_value(void __iomem *base, u32 value)
{
	if (value == 0)
		writel(0x0, base + REG_44_CONFIG_STRIDE);
	else
		writel((value | 0x00010000), base + REG_44_CONFIG_STRIDE);
}

static inline void jsqz_set_input_size(void __iomem *base, u32 size)
{
	writel(size, base + REG_3_INPUT_SIZE);
}

static inline void jsqz_set_input_configs(void __iomem *base, u32 type, u32 mode, u32 use_dc)
{
	writel(type, base + REG_4_INPUT_TYPE);
	writel(mode, base + REG_5_OP_MODE);
	writel(use_dc, base + REG_6_CONFIG_DC);
}

static inline void jsqz_set_input_addr_luma(void __iomem *base, dma_addr_t y_addr)
{
	writel(y_addr, base + REG_0_Y_ADDR);
}

static inline void jsqz_set_input_addr_chroma(void __iomem *base, dma_addr_t u_addr, dma_addr_t v_addr)
{
	writel(u_addr, base + REG_1_U_ADDR);
	writel(v_addr, base + REG_2_V_ADDR);
}

static inline void jsqz_set_input_qtbl(void __iomem *base, u32 * input_qt)
{
	int i;
	for (i = 0; i < 16; i++)
	{
		writel(input_qt[i], base + REG_48_TO_63_INIT_Y_Q + (i * 0x4));
		writel(input_qt[i+16], base + REG_64_TO_79_INIT_C_Q + (i * 0x4));
	}
}
static inline u32 jsqz_get_error_flags(void __iomem *base)
{
	return readl(base + REG_47_ERROR_FLAG);
}

static inline void jsqz_get_init_qtbl(void __iomem *base, u32 * init_qt)
{
	int i;
	for (i = 0; i < 16; i++)
	{
		init_qt[i] = readl(base + REG_48_TO_63_INIT_Y_Q + (i * 0x4));
		init_qt[i+16] = readl(base + REG_64_TO_79_INIT_C_Q + (i * 0x4));
	}
}

static inline void jsqz_get_output_regs(void __iomem *base, u32 * output_qt)
{
	int i;
	for (i = 0; i < 16; i++)
	{
		output_qt[i] = readl(base + REG_7_TO_22_Y_Q_MAT + (i * 0x4));
		output_qt[i+16] = readl(base + REG_23_TO_38_C_Q_MAT + (i * 0x4));
	}
}

static inline void jsqz_set_output_addr(void __iomem *base, dma_addr_t dqp_addr)
{
	writel(dqp_addr, base + REG_85_DQP_ADDR);
}

static inline void jsqz_set_velocity(void __iomem *base, u32 vel_xy)
{
	writel(vel_xy, base + REG_80_VELOCITY);
}

static inline void jsqz_set_tune_dc(void __iomem *base, u32 dc)
{
	writel(dc, base + REG_81_TUNE_DC);
}

static inline void jsqz_set_tune_alpha(void __iomem *base, u32 alpha)
{
	writel(alpha, base + REG_82_TUNE_ALPHA);
}

static inline void jsqz_set_tune_dqp(void __iomem *base, u32 dqp)
{
	writel(dqp, base + REG_83_TUNE_DQP);
}

static inline u32 jsqz_get_frame_dqp(void __iomem *base)
{
	return readl(base + REG_84_FRM_AVG_DQP);
}

#ifdef DEBUG
static inline void jsqz_print_all_regs(struct jsqz_dev *jsqz)
{
	int i;
	void __iomem *base = jsqz->regs;
	dev_dbg(jsqz->dev, "%s: BEGIN\n", __func__);
	for (i = 0; i < 7; i++)
	{
		dev_dbg(jsqz->dev, "%s: 0x%08x : %08x\n", __func__, (i*0x4), readl(base + (i*0x4)));
	}
	for (i = 0; i < 7; i++)
	{
		dev_dbg(jsqz->dev, "%s: 0x%08x : %08x\n", __func__, (REG_39_SW_RESET + (i*0x4)), readl(base + REG_39_SW_RESET + (i*0x4)));
	}
	dev_dbg(jsqz->dev, "%s: 0x%08x : %08x\n", __func__, REG_47_ERROR_FLAG, readl(base + REG_47_ERROR_FLAG));
	for (i = 0; i < 6; i++)
	{
		dev_dbg(jsqz->dev, "%s: 0x%08x : %08x\n", __func__, (REG_80_VELOCITY + (i*0x4)), readl(base + REG_80_VELOCITY + (i*0x4)));
	}
	dev_dbg(jsqz->dev, "%s: END\n", __func__);
}
#endif
*/




/*
static inline int get_hw_enc_status(void __iomem *base)
{
	unsigned int status = 0;

	status = readl(base + MMSQZ_ENC_STAT_REG) & (KBit0 | KBit1);
	return (status != 0 ? -1:0);
}
*/


#endif /* MCFRC_REGS_H_ */

