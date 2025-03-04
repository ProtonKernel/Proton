/* SPDX-License-Identifier: GPL-2.0-or-later */
/*
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * KQ(Kernel Quality) NAD driver implementation
 *	: SeungYoung Lee <seung0.lee@samsung.com>
 */

#ifndef __KQ_NAD_EXYNOS3830_H__
#define __KQ_NAD_EXYNOS3830_H__

#define KQ_NAD_SUPPORT_MAX_BLOCK		64
#define KQ_NAD_SUPPORT_MAX_NAME	16

#define KQ_NAD_MAX_BLOCK_NAME		8
#define KQ_NAD_MAX_DAS_NAME		8

#define KQ_NAD_STR_NAD_PASS "OK_3.1_L"
#define KQ_NAD_STR_NAD_FAIL "NG_3.1_L"
#define KQ_NAD_STR_ACAT_PASS "OK_3.0_L"
#define KQ_NAD_STR_ACAT_FAIL "NG_3.0_L"
#define KQ_NAD_STR_NADX_PASS "OK_3.2_L"
#define KQ_NAD_STR_NADX_FAIL "NG_3.2_L"

#define KQ_NAD_INFORM4_MAGIC 0x1186084c
#define KQ_NAD_INFORM5_MAGIC 0x11860850

enum {
	KQ_NAD_BLOCK_START = 0,
	KQ_NAD_BLOCK_DRAM = KQ_NAD_BLOCK_START,
	KQ_NAD_BLOCK_BIG,
	KQ_NAD_BLOCK_LITT,
	KQ_NAD_BLOCK_MIF,
	KQ_NAD_BLOCK_G3D,
	KQ_NAD_BLOCK_FUNC,
	KQ_NAD_BLOCK_INT,
	KQ_NAD_BLOCK_CP,
	KQ_NAD_BLOCK_NONE,

	KQ_NAD_BLOCK_END,
};

static char kq_nad_block_name[KQ_NAD_BLOCK_END][KQ_NAD_MAX_BLOCK_NAME] = {
	"DRAM", "BIG", "LITT", "MIF", "G3D", "FUNC", "INT", "CP", "NONE" };

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
	//DRAM
	{"NONE",   "PATTERN1",   "PATTERN2",   "SELF_WRITE", "SELF_READ",
	"NONE",     "NONE",       "NONE",       "NONE",       "NONE",
	"WRITE",    "READ",       "SMALL_EYE",  "NONE",       "NONE",
	"NONE",     "NONE",       "NONE",       "NONE",       "NONE",
	"NONE",     "NONE",       "NONE",       "NONE",       "NONE",
	"NONE",     "NONE",       "NONE",       "NONE",       "NONE",
	"NONE",     "NONE",       "NONE",       "NONE",       "NONE",
	"NONE",     "NONE",       "NONE",       "NONE",       "NONE",
	"NONE",     "NONE",       "NONE",       "NONE",       "NONE",
	"NONE",     "NONE",       "NONE",       "NONE",       "NONE",
	"NONE",     "NONE",       "NONE",       "NONE",       "NONE",
	"NONE",     "NONE",       "NONE",       "NONE",       "NONE",
	"NONE",     "NONE",       "NONE",       "NONE"},
	//BIG
	{"NONE",       "UNZIP",        "C2",         "CACHE",      "ICACHE",
	"CRYPTO",       "SHA",          "CPU_MEM",    "LZMA",        "STR_REPEAT",
	"DVFS_MIF",     "UNZIP_DVFS",   "C2_DVFS",    "CACHE_DVFS",  "ICACHE_DVFS",
	"NONE",         "NONE",         "NONE",       "NONE",        "NONE",
	"NONE",         "NONE",         "NONE",       "NONE",        "NONE",
	"NONE",         "NONE",         "NONE",       "NONE",        "NONE",
	"NONE",         "NONE",         "NONE",       "NONE",        "NONE",
	"NONE",         "NONE",         "NONE",       "NONE",        "NONE",
	"NONE",         "NONE",         "NONE",       "NONE",        "NONE",
	"NONE",         "NONE",         "NONE",       "NONE",        "NONE",
	"NONE",         "NONE",         "NONE",       "NONE",        "NONE",
	"NONE",         "NONE",         "NONE",       "NONE",        "NONE",
	"NONE",         "NONE",         "NONE",       "NONE"},
	//LITT
	{"NONE",       "UNZIP",        "C2",         "CACHE",       "ICACHE",
	"CRYPTO",       "SHA",          "CPU_MEM",    "LZMA",        "STR_REPEAT",
	"DVFS_MIF",     "UNZIP_DVFS",   "C2_DVFS",    "CACHE_DVFS",  "ICACHE_DVFS",
	"NONE",         "NONE",         "NONE",       "NONE",        "NONE",
	"NONE",         "NONE",         "NONE",       "NONE",        "NONE",
	"NONE",         "NONE",         "NONE",       "NONE",        "NONE",
	"NONE",         "NONE",         "NONE",       "NONE",        "NONE",
	"NONE",         "NONE",         "NONE",       "NONE",        "NONE",
	"NONE",         "NONE",         "NONE",       "NONE",        "NONE",
	"NONE",         "NONE",         "NONE",       "NONE",        "NONE",
	"NONE",         "NONE",         "NONE",       "NONE",        "NONE",
	"NONE",         "NONE",         "NONE",       "NONE",        "NONE",
	"NONE",         "NONE",         "NONE",       "NONE"},
	//MIF
	{"NONE",   "MEMTESTER",  "VWM",        "SFR",        "RANDOM_DVFS",
	"DLL_LOCK", "NONE",       "NONE",       "NONE",       "NONE",
	"NONE",     "NONE",       "NONE",       "NONE",       "NONE",
	"NONE",     "NONE",       "NONE",       "NONE",       "NONE",
	"NONE",     "NONE",       "NONE",       "NONE",       "NONE",
	"NONE",     "NONE",       "NONE",       "NONE",       "NONE",
	"NONE",     "NONE",       "NONE",       "NONE",       "NONE",
	"NONE",     "NONE",       "NONE",       "NONE",       "NONE",
	"NONE",     "NONE",       "NONE",       "NONE",       "NONE",
	"NONE",     "NONE",       "NONE",       "NONE",       "NONE",
	"NONE",     "NONE",       "NONE",       "NONE",       "NONE",
	"NONE",     "NONE",       "NONE",       "NONE",       "NONE",
	"NONE",     "NONE",       "NONE",       "NONE"},
	//G3D
	{"NONE",    "NONE",      "NONE",       "NONE",       "NONE",
	"NONE",     "NONE",       "NONE",       "NONE",       "NONE",
	"NONE",     "NONE",       "NONE",       "NONE",       "NONE",
	"NONE",     "NONE",       "NONE",       "NONE",       "NONE",
	"NONE",     "NONE",       "NONE",       "NONE",       "NONE",
	"NONE",     "NONE",       "NONE",       "NONE",       "NONE",
	"NONE",     "NONE",       "NONE",       "NONE",       "NONE",
	"NONE",     "NONE",       "NONE",       "NONE",       "NONE",
	"NONE",     "NONE",       "NONE",       "NONE",       "NONE",
	"NONE",     "NONE",       "NONE",       "NONE",       "NONE",
	"NONE",     "NONE",       "NONE",       "NONE",       "NONE",
	"NONE",     "NONE",       "NONE",       "NONE",       "NONE",
	"NONE",     "NONE",       "NONE",       "NONE"},
	//FUNC
	{"NONE",   "LOADING",    "G3D_UNZIP",  "OTP",        "MCT",
	"ADC",      "ABOX_INT",   "ABOX_ASB",   "HPM",       "NONE",
	"NONE",     "NONE",       "NONE",       "NONE",       "NONE",
	"NONE",     "NONE",       "NONE",       "NONE",       "NONE",
	"NONE",     "NONE",       "NONE",       "NONE",       "NONE",
	"NONE",     "NONE",       "NONE",       "NONE",       "NONE",
	"NONE",     "NONE",       "NONE",       "NONE",       "NONE",
	"NONE",     "NONE",       "NONE",       "NONE",       "NONE",
	"NONE",     "NONE",       "NONE",       "NONE",       "NONE",
	"NONE",     "NONE",       "NONE",       "NONE",       "NONE",
	"NONE",     "NONE",       "NONE",       "NONE",       "NONE",
	"NONE",     "NONE",       "NONE",       "NONE",       "NONE",
	"NONE",     "NONE",       "NONE",       "NONE"},
	//INT
	{"NONE",    "SSS",       "RTIC",       "DMA",        "SICD",
	"NONE",      "NONE",      "NONE",       "NONE",       "NONE",
	"NONE",     "NONE",       "NONE",       "NONE",       "NONE",
	"NONE",     "NONE",       "NONE",       "NONE",       "NONE",
	"NONE",     "NONE",       "NONE",       "NONE",       "NONE",
	"NONE",     "NONE",       "NONE",       "NONE",       "NONE",
	"NONE",     "NONE",       "NONE",       "NONE",       "NONE",
	"NONE",     "NONE",       "NONE",       "NONE",       "NONE",
	"NONE",     "NONE",       "NONE",       "NONE",       "NONE",
	"NONE",     "NONE",       "NONE",       "NONE",       "NONE",
	"NONE",     "NONE",       "NONE",       "NONE",       "NONE",
	"NONE",     "NONE",       "NONE",       "NONE",       "NONE",
	"NONE",     "NONE",       "NONE",       "NONE"},
	//CP
	{"NONE",     "AUTOTEST",     "BOOT",     "DVFS_CPSYS", "UCPU_UNZIP",
	"LCPU_UNZIP", "NONE",        "NONE",     "NONE",       "NONE",
	"NONE",       "NONE",        "NONE",     "NONE",       "NONE",
	"NONE",       "NONE",        "NONE",     "NONE",       "NONE",
	"NONE",       "NONE",        "NONE",     "NONE",       "NONE",
	"NONE",       "NONE",        "NONE",     "NONE",       "NONE",
	"NONE",       "NONE",        "NONE",     "NONE",       "NONE",
	"NONE",       "NONE",        "NONE",     "NONE",       "NONE",
	"NONE",       "NONE",        "NONE",     "NONE",       "NONE",
	"NONE",       "NONE",        "NONE",     "NONE",       "NONE",
	"NONE",       "NONE",        "NONE",     "NONE",       "NONE",
	"NONE",       "NONE",        "NONE",     "NONE",       "NONE",
	"NONE",       "NONE",        "NONE",     "NONE"},
};

#endif /* __KQ_NAD_EXYNOS3830_H__ */
