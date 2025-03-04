/*
 * Samsung Exynos SoC series NPU driver
 *
 * Copyright (c) 2020 Samsung Electronics Co., Ltd.
 *              http://www.samsung.com/
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 */

#ifndef _NPU_CONFIG_H_
#define _NPU_CONFIG_H_

#include <linux/of.h>
#include <linux/device.h>

#define DEBUG_CONFIG		(1)
#define BRING_UP		(0)

#define K_SIZE			1024

#define NCP_VERSION		24
#define MAILBOX_VERSION		7
#define BUFFER_Q_LIST_SIZE	1024
#define NCP_MGMT_LIST_SIZE	256
#define NPU_MAX_MSG_ID_CNT	32

#ifdef CONFIG_NPU_HARDWARE
#define FORCE_HWACG_DISABLE
//#define FORCE_WDT_DISABLE
#define ENABLE_PWR_ON
#define ENABLE_RUNTIME_PM
#define CLEAR_SRAM_ON_FIRMWARE_LOADING

#ifdef CONFIG_NPU_ZEBU_EMULATION
#define T32_GROUP_TRACE_SUPPORT
#endif /* CONFIC_NPU_ZEBU_EMULATION */

#endif /* CONFIG_NPU_HARDWARE */


#if BRING_UP
#define TMP_NPU_MAX_BUFFER			16
#define TMP_NPU_MAX_PLANE			3
#define TMP_NPU_MAX_GRAPH			32
#define TMP_NPU_MAX_FRAME			TMP_NPU_MAX_BUFFER
#define TMP_NPU_MINOR				10
#define TMP_NW_Q_SIZE				(1 << 10)
#define TMP_FRAME_Q_SIZE			(1 << 10)
#define TMP_NPU_FW_LOG_KEEP_SIZE		(4096 * 1024)
#define TMP_NPU_MAILBOX_DEFAULT_TID        	 0
#define TMP_TCNTO0_OFF                    	  0x0014
#define TMP_NPU_MAILBOX_HDR_SECTION_LEN		(4 * K_SIZE)
#define TMP_NPU_MAILBOX_SIZE			(32 * K_SIZE)
#define TMP_NPU_MAILBOX_BASE			0x80000
#define TMP_POWER_DOWN_DELAY_ON_EMERGENCY	(300u)
#define TMP_STREAMOFF_DELAY_ON_EMERGENCY	(12000u)
#define TMP_NPU_FW_BASE_ADDR			0
#define	TMP_NPU_CM7_RELEASE_HACK		0
#define TMP_NPU_SHARED_MEM_PAYLOAD		0
#define TMP_NPU_C2AGENT_0			0
#define TMP_NPU_C2AGENT_1			0
#define TMP_NPU_VOTF				0
#define TMP_NPU_VOTF_SIZE			0
#define TMP_NPU_PBHA_HINT_00			0
#define TMP_NPU_PBHA_HINT_01			0
#define TMP_NPU_PBHA_HINT_10			0
#define TMP_NPU_PBHA_HINT_11			0
#define TMP_TRANSACTIONS_PER_CORE		0x80000000
#define TMP_CMDQ_COMPLEXITY_PER_CORE		0x02000000
#define TMP_LASTQ_TIME_THRESHOLD		1000000
#endif


enum {
	DFN_NCP_VERSION = 0,
	DFN_MAILBOX_VERSION,
	DFN_BUFFER_Q_LIST_SIZE,
	DFN_NCP_MGMT_LIST_SIZE,
	DFN_NPU_MAX_MSG_ID_CNT,
	DFN_NPU_CONFIG_NUM,
};

enum {
	NPU_MAX_BUFFER = 0,
	NPU_MAX_PLANE,
	NPU_MAX_GRAPH,
	NPU_MAX_FRAME,
	NPU_MINOR = 0x4,
	NW_Q_SIZE,
	FRAME_Q_SIZE,
	NPU_FW_LOG_KEEP_SIZE,
        NPU_MAILBOX_DEFAULT_TID = 0x8,
	TCNTO0_OFF,
        NPU_MAILBOX_HDR_SECTION_LEN,
        NPU_MAILBOX_SIZE,
        NPU_MAILBOX_BASE = 0xC,
        POWER_DOWN_DELAY_ON_EMERGENCY,
        STREAMOFF_DELAY_ON_EMERGENCY,
        NPU_FW_BASE_ADDR,
	NPU_CM7_RELEASE_HACK = 0x10,
	NPU_SHARED_MEM_PAYLOAD,
	NPU_C2AGENT_0,
	NPU_C2AGENT_1,
	NPU_VOTF = 0x14,
	NPU_VOTF_SIZE,
	NPU_PBHA_HINT_00,
	NPU_PBHA_HINT_01,
	NPU_PBHA_HINT_10,
	NPU_PBHA_HINT_11,
	TRANSACTIONS_PER_CORE = 0x1A,
	CMDQ_COMPLEXITY_PER_CORE,
	LASTQ_TIME_THRESHOLD,
	NPU_CONFIG_NUM = LASTQ_TIME_THRESHOLD + 1,

	NPU_CONFIG_NUM_MAX = 0xFFFFFFFF
};

extern unsigned int configs[NPU_CONFIG_NUM];
extern const char *npu_vertex_name;

int npu_create_config(struct device *dev);
void npu_print_config(void);


#endif /* _NPU_CONFIG_H_ */
