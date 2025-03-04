/* SPDX-License-Identifier: GPL-2.0-or-later */
/*
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * KQ(Kernel Quality) NAD driver implementation
 *	: SeungYoung Lee <seung0.lee@samsung.com>
 */

#ifndef __KQ_NAD_EXYNOS8825_H__
#define __KQ_NAD_EXYNOS8825_H__

#define KQ_NAD_SUPPORT_MAX_BLOCK	64
#define KQ_NAD_SUPPORT_MAX_NAME		16
#define KQ_NAD_MAX_BLOCK_NAME		10
#define KQ_NAD_MAX_DAS_NAME			10

#define KQ_NAD_STR_NAD_PASS "OK_3.1_L"
#define KQ_NAD_STR_NAD_FAIL "NG_3.1_L"
#define KQ_NAD_STR_ACAT_PASS "OK_3.0_L"
#define KQ_NAD_STR_ACAT_FAIL "NG_3.0_L"
#define KQ_NAD_STR_NADX_PASS "OK_3.2_L"
#define KQ_NAD_STR_NADX_FAIL "NG_3.2_L"
#define KQ_NAD_STR_NAD_SKIP "OK_3.1"
#define KQ_NAD_STR_NAD_REWORK "RE_WORK"

#define KQ_NAD_INFORM4_MAGIC 0x1186084c
#define KQ_NAD_INFORM5_MAGIC 0x11860850
#define KQ_NAD_INFORM9_MAGIC 0x15860a30
#define KQ_NAD_INFORM10_MAGIC 0x15860a34

enum {
	KQ_NAD_BLOCK_START = 0,
	KQ_NAD_BLOCK_NONE = 0,
	KQ_NAD_BLOCK_BIG = KQ_NAD_BLOCK_START,
	KQ_NAD_BLOCK_LITT,
	KQ_NAD_BLOCK_DSU,
	KQ_NAD_BLOCK_G3D,
	KQ_NAD_BLOCK_MIF,
	KQ_NAD_BLOCK_AUD,
	KQ_NAD_BLOCK_INT,
	KQ_NAD_BLOCK_MFC0,
	KQ_NAD_BLOCK_DISP,
	KQ_NAD_BLOCK_CAM,
	KQ_NAD_BLOCK_ISP,
	KQ_NAD_BLOCK_NPU,
	KQ_NAD_BLOCK_CP_CPU,
	KQ_NAD_BLOCK_CP,
	KQ_NAD_BLOCK_FUNC,
	KQ_NAD_BLOCK_DRAM,

	KQ_NAD_BLOCK_END,
};

enum {
	KQ_NAD_REWORK_START = 0,
	KQ_NAD_REWORK_FIRST_FAIL = 1,
	KQ_NAD_REWORK_SUDDEN_POWER_OFF = 2,
	KQ_NAD_REWORK_RTC_TIME_OVER = 3,
	KQ_NAD_REWORK_END = 4,
};

enum {
	KQ_NAD_SKIP_ENABLE = 0xABC,
};

static char kq_nad_block_name[KQ_NAD_BLOCK_END][KQ_NAD_MAX_BLOCK_NAME] = {
	"BIG",	"LITT",	"DSU",	"G3D",	"MIF",	"AUD",	"INT",	"MFC0",	"DISP",	"CAM",
	"ISP",	"NPU",	"CP_CPU",	"CP",	"FUNC",	"DRAM"
};

enum {
	KQ_NAD_DAS_START = 0,
	KQ_NAD_DAS_NONE = KQ_NAD_DAS_START,
	KQ_NAD_DAS_DRAM,
	KQ_NAD_DAS_AP,
	KQ_NAD_DAS_SET,
	KQ_NAD_DAS_CP,

	KQ_NAD_DAS_END,
};

static char kq_nad_das_name[KQ_NAD_DAS_END][KQ_NAD_MAX_DAS_NAME] = {
	"NONE", "DRAM", "AP", "SET", "CP" };

struct kq_nad_block {
	char block[KQ_NAD_SUPPORT_MAX_BLOCK][KQ_NAD_SUPPORT_MAX_NAME];
};

static struct kq_nad_block kq_nad_block_data[] = {
	/* BIG */
	{"NONE",		"UNZIP",		"LZMA",			"TBD1",			"C2",
	"TBD2",			"CACHE",		"TBD3",			"TBD4",			"DIJKSTRA",
	"CRYPTO",		"TBD5",			"SHA",			"TBD6",			"FFT",
	"TBD7",			"TBD8",			"AES",			"TBD9",			"MEMBAND",
	"SGEMM",		"MEMTESTER",	"TBD10",		"TBD11",		"TBD12",
	"TBD13",		"NONE",			"NONE",			"NONE",			"NONE",
	"NONE",			"NONE",			"NONE",			"NONE",			"NONE",
	"NONE",			"NONE",			"NONE",			"NONE",			"NONE",
	"NONE",			"NONE",			"NONE",			"NONE",			"NONE",
	"NONE",			"NONE",			"NONE",			"NONE",			"NONE",
	"NONE",			"NONE",			"NONE",			"NONE",			"NONE",
	"NONE",			"NONE",			"NONE",			"NONE",			"NONE",
	"NONE",			"NONE",			"NONE",			"NONE"},

	/* LITT */
	{"NONE",		"UNZIP",		"LZMA",			"TBD1",			"C2",
	"TBD2",			"CACHE",		"TBD3",			"TBD4",			"DIJKSTRA",
	"CRYPTO",		"TBD5",			"SHA",			"TBD6",			"FFT",
	"TBD7",			"TBD8",			"AES",			"TBD9",			"MEMBAND",
	"SGEMM",		"MEMTESTER",	"TBD10",		"TBD11",		"TBD12",
	"TBD13",		"NONE",			"NONE",			"NONE",			"NONE",
	"NONE",			"NONE",			"NONE",			"NONE",			"NONE",
	"NONE",			"NONE",			"NONE",			"NONE",			"NONE",
	"NONE",			"NONE",			"NONE",			"NONE",			"NONE",
	"NONE",			"NONE",			"NONE",			"NONE",			"NONE",
	"NONE",			"NONE",			"NONE",			"NONE",			"NONE",
	"NONE",			"NONE",			"NONE",			"NONE",			"NONE",
	"NONE",			"NONE",			"NONE",			"NONE"},

	/* DSU */
	{"NONE",		"UNZIP",		"LZMA",			"TBD1",			"C2",
	"TBD2",			"CACHE",		"TBD3",			"TBD4",			"DIJKSTRA",
	"CRYPTO",		"TBD5",			"SHA",			"TBD6",			"FFT",
	"TBD7",			"TBD8",			"AES",			"TBD9",			"MEMBAND",
	"SGEMM",		"MEMTESTER",	"TBD10",		"TBD11",		"TBD12",
	"TBD13",		"NONE",			"NONE",			"NONE",			"NONE",
	"NONE",			"NONE",			"NONE",			"NONE",			"NONE",
	"NONE",			"NONE",			"NONE",			"NONE",			"NONE",
	"NONE",			"NONE",			"NONE",			"NONE",			"NONE",
	"NONE",			"NONE",			"NONE",			"NONE",			"NONE",
	"NONE",			"NONE",			"NONE",			"NONE",			"NONE",
	"NONE",			"NONE",			"NONE",			"NONE",			"NONE",
	"NONE",			"NONE",			"NONE",			"NONE"},

	/* G3D */
	{"NONE",		"MANHATTAN",	"AZTEC_NORMAL",	"3D_MARK",		"VGT_TMZ",
	"NONE",			"NONE",			"NONE",			"NONE",			"NONE",
	"NONE",			"NONE",			"NONE",			"NONE",			"NONE",
	"NONE",			"NONE",			"NONE",			"NONE",			"NONE",
	"NONE",			"NONE",			"NONE",			"NONE",			"NONE",
	"NONE",			"NONE",			"NONE",			"NONE",			"NONE",
	"NONE",			"NONE",			"NONE",			"NONE",			"NONE",
	"NONE",			"NONE",			"NONE",			"NONE",			"NONE",
	"NONE",			"NONE",			"NONE",			"NONE",			"NONE",
	"NONE",			"NONE",			"NONE",			"NONE",			"NONE",
	"NONE",			"NONE",			"NONE",			"NONE",			"NONE",
	"NONE",			"NONE",			"NONE",			"NONE",			"NONE",
	"NONE",			"NONE",			"NONE",			"NONE"},

	/* MIF */
	{"NONE",		"MEMTESTER",	"SED",			"VWM",			"RANDOM_DVFS",
	"NONE",			"NONE",			"NONE",			"NONE",			"NONE",
	"NONE",			"NONE",			"NONE",			"NONE",			"NONE",
	"NONE",			"NONE",			"NONE",			"NONE",			"NONE",
	"NONE",			"NONE",			"NONE",			"NONE",			"NONE",
	"NONE",			"NONE",			"NONE",			"NONE",			"NONE",
	"NONE",			"NONE",			"NONE",			"NONE",			"NONE",
	"NONE",			"NONE",			"NONE",			"NONE",			"NONE",
	"NONE",			"NONE",			"NONE",			"NONE",			"NONE",
	"NONE",			"NONE",			"NONE",			"NONE",			"NONE",
	"NONE",			"NONE",			"NONE",			"NONE",			"NONE",
	"NONE",			"NONE",			"NONE",			"NONE",			"NONE",
	"NONE",			"NONE",			"NONE",			"NONE"},

	/* AUD */
	{"NONE",		"ABOX",			"NONE",			"NONE",			"NONE",
	"NONE",			"NONE",			"NONE",			"NONE",			"NONE",
	"NONE",			"NONE",			"NONE",			"NONE",			"NONE",
	"NONE",			"NONE",			"NONE",			"NONE",			"NONE",
	"NONE",			"NONE",			"NONE",			"NONE",			"NONE",
	"NONE",			"NONE",			"NONE",			"NONE",			"NONE",
	"NONE",			"NONE",			"NONE",			"NONE",			"NONE",
	"NONE",			"NONE",			"NONE",			"NONE",			"NONE",
	"NONE",			"NONE",			"NONE",			"NONE",			"NONE",
	"NONE",			"NONE",			"NONE",			"NONE",			"NONE",
	"NONE",			"NONE",			"NONE",			"NONE",			"NONE",
	"NONE",			"NONE",			"NONE",			"NONE",			"NONE",
	"NONE",			"NONE",			"NONE",			"NONE"},

	/* INT */
	{"NONE",		"UFS",			"SSS",			"JPEG",			"SSP",
	"G2D",			"GIC",			"F_SHAR",			"NONE",			"NONE",
	"NONE",			"NONE",			"NONE",			"NONE",			"NONE",
	"NONE",			"NONE",			"NONE",			"NONE",			"NONE",
	"NONE",			"NONE",			"NONE",			"NONE",			"NONE",
	"NONE",			"NONE",			"NONE",			"NONE",			"NONE",
	"NONE",			"NONE",			"NONE",			"NONE",			"NONE",
	"NONE",			"NONE",			"NONE",			"NONE",			"NONE",
	"NONE",			"NONE",			"NONE",			"NONE",			"NONE",
	"NONE",			"NONE",			"NONE",			"NONE",			"NONE",
	"NONE",			"NONE",			"NONE",			"NONE",			"NONE",
	"NONE",			"NONE",			"NONE",			"NONE",			"NONE",
	"NONE",			"NONE",			"NONE",			"NONE"},

	/* MFC0 */
	{"NONE",		"MFC0",			"MFC1",			"NONE",			"NONE",
	"NONE",			"NONE",			"NONE",			"NONE",			"NONE",
	"NONE",			"NONE",			"NONE",			"NONE",			"NONE",
	"NONE",			"NONE",			"NONE",			"NONE",			"NONE",
	"NONE",			"NONE",			"NONE",			"NONE",			"NONE",
	"NONE",			"NONE",			"NONE",			"NONE",			"NONE",
	"NONE",			"NONE",			"NONE",			"NONE",			"NONE",
	"NONE",			"NONE",			"NONE",			"NONE",			"NONE",
	"NONE",			"NONE",			"NONE",			"NONE",			"NONE",
	"NONE",			"NONE",			"NONE",			"NONE",			"NONE",
	"NONE",			"NONE",			"NONE",			"NONE",			"NONE",
	"NONE",			"NONE",			"NONE",			"NONE",			"NONE",
	"NONE",			"NONE",			"NONE",			"NONE"},

	/* DISP */
	{"NONE",		"DISP",			"NONE",			"NONE",			"NONE",
	"NONE",			"NONE",			"NONE",			"NONE",			"NONE",
	"NONE",			"NONE",			"NONE",			"NONE",			"NONE",
	"NONE",			"NONE",			"NONE",			"NONE",			"NONE",
	"NONE",			"NONE",			"NONE",			"NONE",			"NONE",
	"NONE",			"NONE",			"NONE",			"NONE",			"NONE",
	"NONE",			"NONE",			"NONE",			"NONE",			"NONE",
	"NONE",			"NONE",			"NONE",			"NONE",			"NONE",
	"NONE",			"NONE",			"NONE",			"NONE",			"NONE",
	"NONE",			"NONE",			"NONE",			"NONE",			"NONE",
	"NONE",			"NONE",			"NONE",			"NONE",			"NONE",
	"NONE",			"NONE",			"NONE",			"NONE",			"NONE",
	"NONE",			"NONE",			"NONE",			"NONE"},

	/* CAM */
	{"NONE",		"NONE",			"NONE",			"NONE",			"NONE",
	"NONE",			"NONE",			"NONE",			"NONE",			"NONE",
	"NONE",			"NONE",			"NONE",			"NONE",			"NONE",
	"NONE",			"NONE",			"NONE",			"NONE",			"NONE",
	"NONE",			"NONE",			"NONE",			"NONE",			"NONE",
	"NONE",			"NONE",			"NONE",			"NONE",			"NONE",
	"NONE",			"NONE",			"NONE",			"NONE",			"NONE",
	"NONE",			"NONE",			"NONE",			"NONE",			"NONE",
	"NONE",			"NONE",			"NONE",			"NONE",			"NONE",
	"NONE",			"NONE",			"NONE",			"NONE",			"NONE",
	"NONE",			"NONE",			"NONE",			"NONE",			"NONE",
	"NONE",			"NONE",			"NONE",			"NONE",			"NONE",
	"NONE",			"NONE",			"NONE",			"NONE"},

	/* ISP */
	{"NONE",		"NONE",			"NONE",			"NONE",			"NONE",
	"NONE",			"NONE",			"NONE",			"NONE",			"NONE",
	"NONE",			"NONE",			"NONE",			"NONE",			"NONE",
	"NONE",			"NONE",			"NONE",			"NONE",			"NONE",
	"NONE",			"NONE",			"NONE",			"NONE",			"NONE",
	"NONE",			"NONE",			"NONE",			"NONE",			"NONE",
	"NONE",			"NONE",			"NONE",			"NONE",			"NONE",
	"NONE",			"NONE",			"NONE",			"NONE",			"NONE",
	"NONE",			"NONE",			"NONE",			"NONE",			"NONE",
	"NONE",			"NONE",			"NONE",			"NONE",			"NONE",
	"NONE",			"NONE",			"NONE",			"NONE",			"NONE",
	"NONE",			"NONE",			"NONE",			"NONE",			"NONE",
	"NONE",			"NONE",			"NONE",			"NONE"},

	/* NPU */
	{"NONE",		"ASB1",			"ASB2",			"NONE",			"NONE",
	"NONE",			"NONE",			"NONE",			"NONE",			"NONE",
	"NONE",			"NONE",			"NONE",			"NONE",			"NONE",
	"NONE",			"NONE",			"NONE",			"NONE",			"NONE",
	"NONE",			"NONE",			"NONE",			"NONE",			"NONE",
	"NONE",			"NONE",			"NONE",			"NONE",			"NONE",
	"NONE",			"NONE",			"NONE",			"NONE",			"NONE",
	"NONE",			"NONE",			"NONE",			"NONE",			"NONE",
	"NONE",			"NONE",			"NONE",			"NONE",			"NONE",
	"NONE",			"NONE",			"NONE",			"NONE",			"NONE",
	"NONE",			"NONE",			"NONE",			"NONE",			"NONE",
	"NONE",			"NONE",			"NONE",			"NONE",			"NONE",
	"NONE",			"NONE",			"NONE",			"NONE"},

	/* CP_CPU */
	{"NONE",		"NONE",			"NONE",			"NONE",			"NONE",
	"NONE",			"NONE",			"NONE",			"NONE",			"NONE",
	"NONE",			"NONE",			"NONE",			"NONE",			"NONE",
	"NONE",			"NONE",			"NONE",			"NONE",			"NONE",
	"NONE",			"NONE",			"NONE",			"NONE",			"NONE",
	"NONE",			"NONE",			"NONE",			"NONE",			"NONE",
	"NONE",			"NONE",			"NONE",			"NONE",			"NONE",
	"NONE",			"NONE",			"NONE",			"NONE",			"NONE",
	"NONE",			"NONE",			"NONE",			"NONE",			"NONE",
	"NONE",			"NONE",			"NONE",			"NONE",			"NONE",
	"NONE",			"NONE",			"NONE",			"NONE",			"NONE",
	"NONE",			"NONE",			"NONE",			"NONE",			"NONE",
	"NONE",			"NONE",			"NONE",			"NONE"},

	/* CP */
	{"NONE",		"MODEM_IF",		"NONE",			"NONE",			"NONE",
	"NONE",			"NONE",			"NONE",			"NONE",			"NONE",
	"NONE",			"NONE",			"NONE",			"NONE",			"NONE",
	"NONE",			"NONE",			"NONE",			"NONE",			"NONE",
	"NONE",			"NONE",			"NONE",			"NONE",			"NONE",
	"NONE",			"NONE",			"NONE",			"NONE",			"NONE",
	"NONE",			"NONE",			"NONE",			"NONE",			"NONE",
	"NONE",			"NONE",			"NONE",			"NONE",			"NONE",
	"NONE",			"NONE",			"NONE",			"NONE",			"NONE",
	"NONE",			"NONE",			"NONE",			"NONE",			"NONE",
	"NONE",			"NONE",			"NONE",			"NONE",			"NONE",
	"NONE",			"NONE",			"NONE",			"NONE",			"NONE",
	"NONE",			"NONE",			"NONE",			"NONE"},

	/* FUNC */
	{"NONE",		"OTP",			"NONE",			"NONE",			"NONE",
	"NONE",			"NONE",			"NONE",			"NONE",			"NONE",
	"NONE",			"NONE",			"NONE",			"NONE",			"NONE",
	"NONE",			"NONE",			"NONE",			"NONE",			"NONE",
	"NONE",			"NONE",			"NONE",			"NONE",			"NONE",
	"NONE",			"NONE",			"NONE",			"NONE",			"NONE",
	"NONE",			"NONE",			"NONE",			"NONE",			"NONE",
	"NONE",			"NONE",			"NONE",			"NONE",			"NONE",
	"NONE",			"NONE",			"NONE",			"NONE",			"NONE",
	"NONE",			"NONE",			"NONE",			"NONE",			"NONE",
	"NONE",			"NONE",			"NONE",			"NONE",			"NONE",
	"NONE",			"NONE",			"NONE",			"NONE",			"NONE",
	"NONE",			"NONE",			"NONE",			"NONE"},

	/* DRAM */
	{"NONE",		"PATTERN",		"NONE",			"NONE",			"NONE",
	"NONE",			"NONE",			"NONE",			"NONE",			"NONE",
	"NONE",			"NONE",			"NONE",			"NONE",			"NONE",
	"NONE",			"NONE",			"NONE",			"NONE",			"NONE",
	"NONE",			"NONE",			"NONE",			"NONE",			"NONE",
	"NONE",			"NONE",			"NONE",			"NONE",			"NONE",
	"NONE",			"NONE",			"NONE",			"NONE",			"NONE",
	"NONE",			"NONE",			"NONE",			"NONE",			"NONE",
	"NONE",			"NONE",			"NONE",			"NONE",			"NONE",
	"NONE",			"NONE",			"NONE",			"NONE",			"NONE",
	"NONE",			"NONE",			"NONE",			"NONE",			"NONE",
	"NONE",			"NONE",			"NONE",			"NONE",			"NONE",
	"NONE",			"NONE",			"NONE",			"NONE"},
};

#if IS_ENABLED(CONFIG_SEC_KQ_CORRELATION_RESULT)
#define KQ_NAD_CORRELATION_MPARAM_MAX_LEN (32 * 2 + 1)
#define KQ_NAD_CORRELATION_VALID_MAGIC 0x1234ABCD

/* CL0, CL1, CL2, MIF, DSU, CP, G3D, SCI */
enum {
	KQ_NAD_MPARAM_CORRELATION_LOGIC_BLOCK_CL0,
	KQ_NAD_MPARAM_CORRELATION_LOGIC_BLOCK_CL1,
	KQ_NAD_MPARAM_CORRELATION_LOGIC_BLOCK_CL2,
	KQ_NAD_MPARAM_CORRELATION_LOGIC_BLOCK_MIF,
	KQ_NAD_MPARAM_CORRELATION_LOGIC_BLOCK_DSU,
	KQ_NAD_MPARAM_CORRELATION_LOGIC_BLOCK_CP,
	KQ_NAD_MPARAM_CORRELATION_LOGIC_BLOCK_G3D,
	KQ_NAD_MPARAM_CORRELATION_LOGIC_BLOCK_SCI,

	KQ_NAD_MPARAM_CORRELATION_LOGIC_BLOCK_END,
};
#endif //IS_ENABLED(CONFIG_SEC_KQ_CORRELATION_RESULT)

#if IS_ENABLED(CONFIG_SEC_KQ_BPS_RESULT)
#define KQ_NAD_BPS_MPARAM_MAX_DELIMITER	14
#define KQ_NAD_BPS_MPARAM_MAX_LEN (11 * (KQ_NAD_BPS_MPARAM_MAX_DELIMITER + 1) + KQ_NAD_BPS_MPARAM_MAX_DELIMITER)
#endif //IS_ENABLED(CONFIG_SEC_KQ_BPS_RESULT)

#endif /* __KQ_NAD_EXYNOS8825_H__ */
