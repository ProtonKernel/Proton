/*
 * Samsung Exynos5 SoC series FIMC-IS driver
 *
 *
 * Copyright (c) 2018 Samsung Electronics Co., Ltd
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

/* 3AA0 */
#define SOC_30S
#define SOC_30C
#define SOC_30P

/* 3AA1 */
#define SOC_31S
#define SOC_31C
#define SOC_31P

/* ITP0 */
#define SOC_I0S
#define SOC_I0C
#define SOC_I0P

/* MCSC */
#define SOC_MCS
#define SOC_MCS0
#define SOC_MCS1

/* CSIS_DMA */
#define SOC_SSVC0
#define SOC_SSVC1
#define SOC_SSVC2
#define SOC_SSVC3

#define SOC_VRA

/* Only use for interface sync with DDK (param_set) */
#define SOC_ORBMCH
#define SOC_TNR_MERGER

/*
 * =================================================================================================
 * CONFIG - SW configurations
 * =================================================================================================
 */
#define CHAIN_USE_STRIPE_PROCESSING		1
#define CHAIN_USE_STRIPE_REGION_NUM_META	1
#define STRIPE_MARGIN_WIDTH		(384)
#define STRIPE_WIDTH_ALIGN		(128)
#define STRIPE_RATIO_PRECISION		(1000)

/* #define ENABLE_TNR */
#define NUM_OF_TNR_BUF	10	/* quadro(4 + 1) & double buffering(2) */
/* #define ENABLE_ORBMCH	1 */
/* #define ENABLE_10BIT_MCSC */
/* #define ENABLE_DJAG_IN_MCSC */

#define ENABLE_VRA		1
#define ENABLE_REPROCESSING_FD		1
#define ENABLE_VRA_NETARRAY

#define USE_ONE_BINARY
#define USE_RTA_BINARY
#define USE_BINARY_PADDING_DATA_ADDED	/* for DDK signature */
#define USE_DDK_SHUT_DOWN_FUNC
#define ENABLE_IRQ_MULTI_TARGET
#define IS_ONLINE_CPU_MIN	4
#define ENABLE_3AA_LIC_OFFSET	1
#define PDP_RDMA_LINE_GAP		(0x64)
#define IS_MAX_HW_3AA_SRAM		(7552)

#define ENABLE_MODECHANGE_CAPTURE

/*
 * 3AA0: 3AA0, ZSL/STRIP DMA0
 * 3AA1: 3AA1, ZSL/STRIP DMA1
 * ITP0: ITP0
 * MCSC0:
 * MCSC1:
 * VRA:
 *
 */
#define HW_SLOT_MAX            (10)
#define valid_hw_slot_id(slot_id) \
	(slot_id >= 0 && slot_id < HW_SLOT_MAX)
#define HW_NUM_LIB_OFFSET	(2)
/* #define DISABLE_SETFILE */
/* #define DISABLE_LIB */

#define USE_SENSOR_IF_DPHY
/* #define ENABLE_HMP_BOOST */

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
 */
#define LIC_CHAIN_OFFSET	(0x10000)
#define LIC_CHAIN_NUM		(2)
#define LIC_CHAIN_OFFSET_NUM	(6)
/*
 * =================================================================================================
 * CONFIG - FEATURE ENABLE
 * =================================================================================================
 */

#define IS_MAX_TASK		(40)

#define OVERFLOW_PANIC_ENABLE_ISCHAIN
#define OVERFLOW_PANIC_ENABLE_CSIS
#if defined(CONFIG_ARM_EXYNOS_DEVFREQ)
#define CONFIG_IS_BUS_DEVFREQ
#endif
#if defined(OVERFLOW_PANIC_ENABLE_ISCHAIN)
#define DDK_OVERFLOW_RECOVERY		(0)	/* 0: do not execute recovery, 1: execute recovery */
#else
#define DDK_OVERFLOW_RECOVERY		(1)	/* 0: do not execute recovery, 1: execute recovery */
#endif
#define CAPTURE_NODE_MAX		12
#define OTF_YUV_FORMAT			(OTF_INPUT_FORMAT_YUV422)
#define MSB_OF_3AA_DMA_OUT		(7)
#define MSB_OF_DNG_DMA_OUT		(7)
/* #define USE_YUV_RANGE_BY_ISP */
/* #define ENABLE_3AA_DMA_CROP */

/* use bcrop1 to support DNG capture for pure bayer reporcessing */
#define USE_3AA_CROP_AFTER_BDS

/* #define ENABLE_ULTRA_FAST_SHOT */
/* #define ENABLE_HWFC */
/* #define TPU_COMPRESSOR */
#define USE_I2C_LOCK
#define SENSOR_REQUEST_DELAY		2

#ifdef ENABLE_IRQ_MULTI_TARGET
#define IS_HW_IRQ_FLAG     IRQF_GIC_MULTI_TARGET
#else
#define IS_HW_IRQ_FLAG     0
#endif

/* #define MULTI_SHOT_KTHREAD */
/* #define MULTI_SHOT_TASKLET */
/* #define ENABLE_EARLY_SHOT */
#define ENABLE_HW_FAST_READ_OUT
#define FULL_OTF_TAIL_GROUP_ID		GROUP_ID_MCS0

#if defined(USE_I2C_LOCK)
#define I2C_MUTEX_LOCK(lock)	mutex_lock(lock)
#define I2C_MUTEX_UNLOCK(lock)	mutex_unlock(lock)
#else
#define I2C_MUTEX_LOCK(lock)
#define I2C_MUTEX_UNLOCK(lock)
#endif

#define ENABLE_DBG_EVENT_PRINT

#if IS_ENABLED(CONFIG_SECURE_CAMERA_USE)
#ifdef SECURE_CAMERA_IRIS
#undef SECURE_CAMERA_IRIS
#endif
#define SECURE_CAMERA_FACE              (1) /* For face Detection and face authentication */
#define SECURE_CAMERA_CH                ((1 << CSI_ID_A))
#define SECURE_CAMERA_HEAP_ID           (11)
#define SECURE_CAMERA_MEM_SHARE
#define SECURE_CAMERA_TAAISP
#define SECURE_CAMERA_MEDRC
#define SECURE_CAMERA_MEM_ADDR          (0xE1900000)    /* secure_camera_heap */
#define SECURE_CAMERA_MEM_SIZE          (0x01520000)
#define NON_SECURE_CAMERA_MEM_ADDR      (0xDB900000)    /* camera_heap */
#define NON_SECURE_CAMERA_MEM_SIZE      (0x00B00000)
#define PROTECT_SYSREG_IS               /* If sysreg_is needs to be protected in secure scenario */

#define SECURE_CAMERA_USE_EXT_HEAP
#define SECURE_CAMERA_HEAP_ID_EXT      (14)
#define SECURE_CAMERA_MEM_ADDR_EXT     (0xE2E20000)    /* secure_camera_ext_heap */
#define SECURE_CAMERA_MEM_SIZE_EXT     (0x00780000)

#define CONVERT_BUFFER_SECURE_TO_NON_SECURE
#undef SECURE_CAMERA_FACE_SEQ_CHK       /* To check sequence before applying secure protection */
#endif

/* #define QOS_INTCAM */
/* #define QOS_TNR */
#define USE_NEW_PER_FRAME_CONTROL

#define ENABLE_HWACG_CONTROL

/* load calibation data */
#define ENABLE_CAL_LOAD

/* HACK : to support FULL device */
#define DISABLE_CHECK_PERFRAME_FMT_SIZE

#define BDS_DVFS
/* #define ENABLE_VIRTUAL_OTF */
/* #define ENABLE_DCF */

/* #define ENABLE_PDP_STAT_DMA */
#define USE_DEBUG_PD_DUMP	0
#define DEBUG_PD_DUMP_CH	0  /* set a bit corresponding to each channel */

#define FAST_FDAE
#undef RESERVED_MEM_IN_DT

#define CHAIN_SKIP_GFRAME

/* init AWB */
#define ENABLE_INIT_AWB
#define WB_GAIN_COUNT		(4)
#define INIT_AWB_COUNT_REAR	(3)
#define INIT_AWB_COUNT_FRONT	(8)

/* use OIS init thread option */
#define USE_OIS_INIT_WORK

/* #define USE_CAMIF_FIX_UP	1 */
#define CHAIN_TAG_SENSOR_IN_SOFTIRQ_CONTEXT    0
#define CHAIN_TAG_VC0_DMA_IN_HARDIRQ_CONTEXT   1

/* default LIC value for 9630 */
/*
 * MAX: 16383
 * CTX0: 25M(5804), 25M(5804), 8M (3328)
 * CTX1: 25M(5804), 25M(5804), 8M (3328)
 */
#define DEFAULT_LIC_CTX0_OFFSET0	(5824)
#define DEFAULT_LIC_CTX0_OFFSET1	(5824)
#define DEFAULT_LIC_CTX1_OFFSET0	(5824)
#define DEFAULT_LIC_CTX1_OFFSET1	(5824)

#define DISABLE_CORE_IDLE_STATE

/* for bcm monitor : this must be enabled when cdh binary is added to DDK. */
#define USE_CDH_BINARY

#define AP_PHY_LDO_ALL_SAME  	1

/*
 * ======================================================
 * CONFIG - Interface version configs
 * ======================================================
 */

#define SETFILE_VERSION_INFO_HEADER1	(0xF85A20B4)
#define SETFILE_VERSION_INFO_HEADER2	(0xCA539ADF)

#define META_ITF_VER_20192003

#define ENABLE_STRIPE_SYNC_PROCESSING
#define ENABLE_SYNC_REPROCESSING

#define CONFIG_SW_PDAF		1

#endif /* #ifndef IS_CONFIG_H */
