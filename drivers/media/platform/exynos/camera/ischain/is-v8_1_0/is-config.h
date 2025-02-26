// SPDX-License-Identifier: GPL-2.0
/*
 * Samsung Exynos SoC series Pablo driver
 *
 * Copyright (c) 2019 Samsung Electronics Co., Ltd
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#ifndef IS_CONFIG_H
#define IS_CONFIG_H

#include <is-common-config.h>
#include <is-vendor-config.h>

/*
 * =================================================================================================
 * CONFIG - GLOBAL OPTIONS
 * =================================================================================================
 */
#define IS_SENSOR_COUNT		6
#define IS_STREAM_COUNT		9

/*
 * =================================================================================================
 * CONFIG - Chain IP configurations
 * =================================================================================================
 */
/* Sensor VC */
#define SOC_SSVC0
#define SOC_SSVC1
#define SOC_SSVC2
#define SOC_SSVC3

/* 3AA0 */
#define SOC_30S
#define SOC_30C
#define SOC_30P
#define SOC_30F
#define SOC_30G
#define SOC_ZSL_STRIP_DMA0

/* 3AA1 */
#define SOC_31S
#define SOC_31C
#define SOC_31P
#define SOC_31F
#define SOC_31G
#define SOC_ZSL_STRIP_DMA1

/* 3AA2 */
#define SOC_32S
#define SOC_32C
#define SOC_32P
#define SOC_32F
#define SOC_32G
#define SOC_ZSL_STRIP_DMA2

/* TNR */
#define SOC_TNR
#define SOC_TNR_MERGER

/* ITP0 */
#define SOC_I0S
#define SOC_DNS0S

/* MEIP/ORB-MCH */
#define SOC_ME0S
#define SOC_ME0C
#define SOC_ME1S
#define SOC_ME1C
#define SOC_ORBMCH

/* MCSC */
#define SOC_MCS
#define SOC_MCS0

/* CLAHE */
#define SOC_CLH

/* Frame id decoder */
#define SYSREG_FRAME_ID_DEC		(0x17020440)

/*
 * =================================================================================================
 * CONFIG - SW configurations
 * =================================================================================================
 */
#define CHAIN_USE_STRIPE_PROCESSING	1
#define CHAIN_USE_STRIPE_REGION_NUM_META	1
#define STRIPE_MARGIN_WIDTH		(512)
#define STRIPE_WIDTH_ALIGN		(512)
#define STRIPE_RATIO_PRECISION		(1000)
#define ENABLE_TNR
#define ENABLE_ORBMCH	1
#define ENABLE_10BIT_MCSC
#define ENABLE_DJAG_IN_MCSC
#define ENABLE_MCSC_CENTER_CROP
#define USE_MCSC_STRIP_OUT_CROP		/* use for MCSC stripe */

#define USE_ONE_BINARY
#define USE_RTA_BINARY
#define USE_BINARY_PADDING_DATA_ADDED	/* for DDK signature */
#define USE_DDK_SHUT_DOWN_FUNC
#define ENABLE_IRQ_MULTI_TARGET
#define IS_ONLINE_CPU_MIN	4
#define ENABLE_3AA_LIC_OFFSET	1
#define PDP_RDMA_LINE_GAP      (0x64)
#define IS_MAX_HW_3AA_SRAM		(13696)

#define ENABLE_MODECHANGE_CAPTURE
/*
 * PDP0: RDMA
 * PDP1: RDMA
 * PDP2: RDMA
 * 3AA0: 3AA0, ZSL/STRIP DMA0, MEIP0
 * 3AA1: 3AA1, ZSL/STRIP DMA1, MEIP1
 * 3AA2: 3AA2, ZSL/STRIP DMA2, MEIP2
 * ITP0: TNR, ITP0, DNS0
 * MCSC0:
 *
 */
#define HW_SLOT_MAX            (10)
#define valid_hw_slot_id(slot_id) \
	(slot_id >= 0 && slot_id < HW_SLOT_MAX)
#define HW_NUM_LIB_OFFSET	(2)
/* #define DISABLE_SETFILE */
/* #define DISABLE_LIB */

#define USE_SENSOR_IF_DPHY
#define ENABLE_HMP_BOOST

/* FIMC-IS task priority setting */
#define TASK_SENSOR_WORK_PRIO		(IS_MAX_PRIO - 48) /* 52 */
#define TASK_GRP_OTF_INPUT_PRIO		(IS_MAX_PRIO - 49) /* 51 */
#define TASK_GRP_DMA_INPUT_PRIO		(IS_MAX_PRIO - 50) /* 50 */
#define TASK_MSHOT_WORK_PRIO		(IS_MAX_PRIO - 43) /* 57 */
#define TASK_LIB_OTF_PRIO		(IS_MAX_PRIO - 44) /* 56 */
#define TASK_LIB_AF_PRIO		(IS_MAX_PRIO - 45) /* 55 */
#define TASK_LIB_ISP_DMA_PRIO		(IS_MAX_PRIO - 46) /* 54 */
#define TASK_LIB_3AA_DMA_PRIO		(IS_MAX_PRIO - 47) /* 53 */
#define TASK_LIB_AA_PRIO		(IS_MAX_PRIO - 48) /* 52 */
#define TASK_LIB_RTA_PRIO		(IS_MAX_PRIO - 49) /* 51 */
#define TASK_LIB_VRA_PRIO		(IS_MAX_PRIO - 45) /* 55 */

/* EXTRA chain for 3AA and ITP
 * Each IP has 4 slot
 * 3AA: 0~3	ITP: 0~3
 * MEIP: 4~7	TNR: 4~7
 * DMA: 8~11	DNS: 8~11
 */
#define LIC_CHAIN_OFFSET	(0x10000)
#define LIC_CHAIN_NUM		(2)
#define LIC_CHAIN_OFFSET_NUM	(6)
/*
 * =================================================================================================
 * CONFIG - FEATURE ENABLE
 * =================================================================================================
 */
#define DISABLE_CHECK_PERFRAME_FMT_SIZE
#define IS_MAX_TASK		(40)

#define HW_TIMEOUT_PANIC_ENABLE
#if defined(CONFIG_ARM_EXYNOS_DEVFREQ)
#define CONFIG_IS_BUS_DEVFREQ
#endif

#undef OVERFLOW_PANIC_ENABLE_ISCHAIN
#undef OVERFLOW_PANIC_ENABLE_CSIS
#if defined(OVERFLOW_PANIC_ENABLE_ISCHAIN)
#define DDK_OVERFLOW_RECOVERY		(0)	/* 0: do not execute recovery, 1: execute recovery */
#else
#define DDK_OVERFLOW_RECOVERY		(1)	/* 0: do not execute recovery, 1: execute recovery */
#endif

#define CAPTURE_NODE_MAX		12
#define OTF_YUV_FORMAT			(OTF_INPUT_FORMAT_YUV422)
#define MSB_OF_3AA_DMA_OUT		(11)
#define MSB_OF_DNG_DMA_OUT		(9)
/* #define USE_YUV_RANGE_BY_ISP */
/* #define ENABLE_3AA_DMA_CROP */

/* use bcrop1 to support DNG capture for pure bayer reporcessing */
#define USE_3AA_CROP_AFTER_BDS

/* #define ENABLE_ULTRA_FAST_SHOT */
#define ENABLE_HWFC
#define TPU_COMPRESSOR
#define USE_I2C_LOCK
#define SENSOR_REQUEST_DELAY		2

#ifdef ENABLE_IRQ_MULTI_TARGET
#define IS_HW_IRQ_FLAG     IRQF_GIC_MULTI_TARGET
#else
#define IS_HW_IRQ_FLAG     0
#endif

/* #define MULTI_SHOT_KTHREAD */
#define MULTI_SHOT_TASKLET
/* #define ENABLE_EARLY_SHOT */
/* #define ENABLE_HW_FAST_READ_OUT */
#define FULL_OTF_TAIL_GROUP_ID		GROUP_ID_MCS0

#if defined(USE_I2C_LOCK)
#define I2C_MUTEX_LOCK(lock)	mutex_lock(lock)
#define I2C_MUTEX_UNLOCK(lock)	mutex_unlock(lock)
#else
#define I2C_MUTEX_LOCK(lock)
#define I2C_MUTEX_UNLOCK(lock)
#endif

#define ENABLE_DBG_EVENT_PRINT
/* #define ENABLE_DBG_CLK_PRINT */

#if IS_ENABLED(CONFIG_SECURE_CAMERA_USE)
#ifdef SECURE_CAMERA_IRIS
#undef SECURE_CAMERA_IRIS
#endif
#define SECURE_CAMERA_FACE	/* For face Detection and face authentication */
#define SECURE_CAMERA_CH		((1 << CSI_ID_C) | (1 << CSI_ID_E))
#define SECURE_CAMERA_HEAP_ID		(11)
#define SECURE_CAMERA_MEM_ADDR		(0x96000000)	/* secure_camera_heap */
#define SECURE_CAMERA_MEM_SIZE		(0x02800000)
#define NON_SECURE_CAMERA_MEM_ADDR	(0x0)	/* camera_heap */
#define NON_SECURE_CAMERA_MEM_SIZE	(0x0)

#undef SECURE_CAMERA_FACE_SEQ_CHK	/* To check sequence before applying secure protection */
#endif

#define QOS_INTCAM
#define QOS_TNR
#define USE_NEW_PER_FRAME_CONTROL

#define ENABLE_HWACG_CONTROL

#define BDS_DVFS
/* #define ENABLE_VIRTUAL_OTF */
/* #define ENABLE_DCF */

/* #define ENABLE_PDP_STAT_DMA */

#define USE_PDP_LINE_INTR_FOR_PDAF	1
#define USE_DEBUG_PD_DUMP	0
#define DEBUG_PD_DUMP_CH	0  /* set a bit corresponding to each channel */

#define FAST_FDAE
#undef RESERVED_MEM_IN_DT

/* init AWB */
#define ENABLE_INIT_AWB
#define WB_GAIN_COUNT		(4)
#define INIT_AWB_COUNT_REAR	(3)
#define INIT_AWB_COUNT_FRONT	(8)

/* use CIS global work for enhance launching time */
#define USE_CIS_GLOBAL_WORK	1

#define USE_CAMIF_FIX_UP	1
#define CHAIN_TAG_SENSOR_IN_SOFTIRQ_CONTEXT	0
#define CHAIN_TAG_VC0_DMA_IN_HARDIRQ_CONTEXT	1

#define USE_SKIP_DUMP_LIC_OVERFLOW	1

/* default LIC value for 9830 */
/*
 * MAX: 16383
 * CTX0: 25M(5804), 25M(5804), 8M (3328)
 * CTX1: 25M(5804), 25M(5804), 8M (3328)
 */
#define DEFAULT_LIC_CTX0_OFFSET0	(5824)
#define DEFAULT_LIC_CTX0_OFFSET1	(5824)
#define DEFAULT_LIC_CTX1_OFFSET0	(5824)
#define DEFAULT_LIC_CTX1_OFFSET1	(5824)

#define CONFIG_VOTF_ONESHOT	1	/* oneshot mode is used when using VOTF in PDP input.  */
#define VOTF_BACK_FIRST_OFF	0	/* This is only for v8.1. next AP don't have to use thie. */

#define SDC_HEADER_GEN

/* BTS */
#define DISABLE_BTS_CALC	/* This is only for v8.1. next AP don't have to use this */

#define THROTTLING_MIF_ENABLE   1
#define THROTTLING_MIF_LEVEL	(1539000)

#define AP_PHY_LDO_ALL_SAME  	1

#define SYNC_SHOT_ALWAYS	/* This is a feature for reducing late shot. */
/*
 * ======================================================
 * CONFIG - Interface version configs
 * ======================================================
 */

#define SETFILE_VERSION_INFO_HEADER1	(0xF85A20B4)
#define SETFILE_VERSION_INFO_HEADER2	(0xCA539ADF)

#define META_ITF_VER_20192003

#endif /* #ifndef IS_CONFIG_H */
