/* SPDX-License-Identifier: GPL-2.0-or-later */
/*
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * KQ(Kernel Quality) NAD driver implementation
 *	: SeungYoung Lee <seung0.lee@samsung.com>
 */

#ifndef __KQ_NAD_H__
#define __KQ_NAD_H__

#if IS_ENABLED(CONFIG_SOC_EXYNOS2100)
#include <linux/kq/kq_nad_exynos2100.h>
#elif IS_ENABLED(CONFIG_SOC_EXYNOS3830) || IS_ENABLED(CONFIG_SOC_S5E3830)
#include <linux/kq/kq_nad_exynos3830.h>
#elif IS_ENABLED(CONFIG_SOC_S5E9925)
#include <linux/kq/kq_nad_exynos9925.h>
#elif IS_ENABLED(CONFIG_SOC_S5E8825)
#include <linux/kq/kq_nad_exynos8825.h>
#elif IS_ENABLED(CONFIG_SEC_KQ_NAD_TEST)
#include <linux/kq/kq_nad_exynos2100.h>
#else
#include <linux/kq/kq_nad_default.h>
#endif

#include <linux/kq/kq_nad_result.h>

#define KQ_NAD_MAX_CMD_NAME		10
#define KQ_NAD_MAX_CMD_SIZE		3

#define KQ_NADC_MAX_CMD_SIZE		2

#define KQ_NAD_FLAG_SMD_FIRST		1005
#define KQ_NAD_FLAG_SMD_SECOND		1006
#define KQ_NAD_FLAG_ACAT_FIRST		5001
#define KQ_NAD_FLAG_ACAT_SECOND		5002
#define KQ_NAD_FLAG_EXTEND_FIRST	1007
#define KQ_NAD_FLAG_EXTEND_SECOND	1008
#define KQ_NAD_FLAG_CUSTOM_FIRST	1010
#define KQ_NAD_FLAG_CUSTOM_SECOND	1011

#if IS_ENABLED(CONFIG_SEC_KQ_NAD_V2)
#define KQ_NAD_BIT_PASS			0x80000000
#define KQ_NAD_CMD_MAGIC		0xDCBA

enum {
	NAD_MAGIC_ERASE,
	NAD_MAGIC_ERASE_REWORK,
	NAD_MAGIC_ACAT,
	NAD_MAGIC_DRAM_TEST,
	NAD_MAGIC_ACAT_DRAM_TEST,
	NAD_MAGIC_NADX_TEST,
	NAD_MAGIC_REBOOT,
	NAD_MAGIC_NADC,
	NAD_MAGIC_NADX_SHORT,
	NAD_MAGIC_NADO,
	NAD_MAGIC_MAINTENANCE,
};

#define KQ_NAD_MAGIC_ERASE				BIT(NAD_MAGIC_ERASE)
#define KQ_NAD_MAGIC_ERASE_REWORK		BIT(NAD_MAGIC_ERASE_REWORK)
#define KQ_NAD_MAGIC_ACAT				BIT(NAD_MAGIC_ACAT)
#define KQ_NAD_MAGIC_DRAM_TEST			BIT(NAD_MAGIC_DRAM_TEST)
#define KQ_NAD_MAGIC_ACAT_DRAM_TEST		BIT(NAD_MAGIC_ACAT_DRAM_TEST)
#define KQ_NAD_MAGIC_NADX_TEST			BIT(NAD_MAGIC_NADX_TEST)
#define KQ_NAD_MAGIC_REBOOT				BIT(NAD_MAGIC_REBOOT)
#define KQ_NAD_MAGIC_NADC_TEST			BIT(NAD_MAGIC_NADC)
#define KQ_NAD_MAGIC_NADX_SHORT			BIT(NAD_MAGIC_NADX_SHORT)
#define KQ_NAD_MAGIC_NADO				BIT(NAD_MAGIC_NADO)
#define KQ_NAD_MAGIC_MAINTENANCE		BIT(NAD_MAGIC_MAINTENANCE)

#define KQ_NAD_MAGIC_NADX_DONE			0x5A
#define KQ_NAD_LOOP_CNT_MAINTENANCE	55555
#else
#define KQ_NAD_BIT_PASS			0x00002000

#define KQ_NAD_MAGIC_ERASE				0xA5000000
#define KQ_NAD_MAGIC_ERASE_REWORK		0x3C000000
#define KQ_NAD_MAGIC_ACAT				0x00A50000
#define KQ_NAD_MAGIC_DRAM_TEST			0x0000A500
#define KQ_NAD_MAGIC_ACAT_DRAM_TEST		0x00A5A500
#define KQ_NAD_MAGIC_NADX_TEST			0x000000A5
#define KQ_NAD_MAGIC_NADX_DONE			0x0000005A
#define KQ_NAD_MAGIC_REBOOT				0x5A000000

#define KQ_NAD_MAGIC_NADC_TEST			0x003C0000
#endif

#define KQ_NAD_MAGIC_CONSTANT_XOR		0xABCDABCD

#define KQ_NAD_CHECK_NAD_STATUS			(0x1)
#define KQ_NAD_CHECK_NAD_SECOND_STATUS	(0x2)
#define KQ_NAD_CHECK_ACAT_STATUS		(0x4)
#define KQ_NAD_CHECK_ACAT_SECOND_STATUS	(0x8)
#define KQ_NAD_CHECK_DRAM_STATUS		(0x10)
#define KQ_NAD_CHECK_NADX_STATUS		(0x20)
#define KQ_NAD_CHECK_NADX_SECOND_STATUS	(0x40)
#define KQ_NAD_CHECK_NADC_STATUS		(0x80)
#define KQ_NAD_CHECK_NADC_SECOND_STATUS	(0x100)

#define KQ_NAD_LOOP_COUNT_NADX		444

#define KQ_NAD_REWORK_CHECK_TN 50
#define KQ_NAD_FIRST_START_MAGIC (0x4E)
#define KQ_NAD_SECOND_START_MAGIC (0x5E)

#if IS_ENABLED(CONFIG_ARM_EXYNOS_ACME_DISABLE_BOOT_LOCK)
/*
 * disable cpu boot qos lock : flexable.cpuboot value is valid ( 1, 2, 3, 4 )
 * cpufreq_disable_boot_qos_lock does not apply max lock
 */
enum {
	FLEXBOOT_LIT = 1,
	FLEXBOOT_MID,
	FLEXBOOT_BIG,
	FLEXBOOT_ALL,
};
extern int flexable_cpu_boot;
#endif //IS_ENABLED(CONFIG_ARM_EXYNOS_ACME_DISABLE_BOOT_LOCK)

#if IS_ENABLED(CONFIG_ARM_EXYNOS_DEVFREQ_DISABLE_BOOT_LOCK)
/*
 * disable exynos devfreq boot qos lock
 */
extern int flexable_dev_boot;
#endif //IS_ENABLED(CONFIG_ARM_EXYNOS_DEVFREQ_DISABLE_BOOT_LOCK)

enum {
	KQ_NAD_DAS_NAME_NONE = 0,
	KQ_NAD_DAS_NAME_DRAM,
	KQ_NAD_DAS_NAME_AP,
	KQ_NAD_DAS_NAME_SET,
	KQ_NAD_DAS_NAME_CP,
};

enum {
	KQ_NAD_RESULT_PASS = 0,
	KQ_NAD_RESULT_FAIL,
	KQ_NAD_RESULT_MAIN,

	KQ_NAD_RESULT_END,
};

enum {
	KQ_NAD_SHUTDOWN_DISABLE = 0,
	KQ_NAD_SHUTDOWN_ENABLE,
};

#if IS_ENABLED(CONFIG_SEC_KQ_NAD_V2)
enum {
	KQ_NAD_INFORM1_BLOCK = 0,
	KQ_NAD_INFORM1_LEVEL = 8,
	KQ_NAD_INFORM1_STATE = 13,
	KQ_NAD_INFORM1_MAGIC = 16,
	KQ_NAD_INFORM1_END,
};
#else
enum {
	KQ_NAD_INFORM1_LEVEL = 0,
	KQ_NAD_INFORM1_STATE = 5,
	KQ_NAD_INFORM1_BLOCK = 8,
	KQ_NAD_INFORM1_MAGIC = 16,
	KQ_NAD_INFORM1_END,
};
#endif

enum {
	KQ_NAD_CHECK_TEMPERATURE = 1,
	KQ_NAD_STEP_WATCHDOG,
	KQ_NAD_SET_PRECONDITION,
	KQ_NAD_SET_VOLTAGE,
	KQ_NAD_VECTOR_START,
	KQ_NAD_RECOVER_VOLTAGE,
	KQ_NAD_RTC_TIMEOUT,
	KQ_NAD_STATE_END,
};

enum {
	KQ_NAD_SYSFS_STAT = 0,
	KQ_NAD_SYSFS_ERASE,
	KQ_NAD_SYSFS_ACAT,
	KQ_NAD_SYSFS_DRAM,
	KQ_NAD_SYSFS_ALL,
	KQ_NAD_SYSFS_SUPPORT,
#if IS_ENABLED(CONFIG_SEC_KQ_NAD_API)
	KQ_NAD_SYSFS_API,
#endif
	KQ_NAD_SYSFS_VERSION,
#if IS_ENABLED(CONFIG_SEC_KQ_NAD_X) || IS_ENABLED(CONFIG_SEC_KQ_NAD_V2)
	KQ_NAD_SYSFS_FAC_RESULT,
	KQ_NAD_SYSFS_NADX_RUN,
#endif
	KQ_NAD_SYSFS_NADC_RESULT,
	KQ_NAD_SYSFS_NADC_RUN,
	KQ_NAD_SYSFS_REBOOT,
	KQ_NAD_SYSFS_END,
};

struct kq_nad_fail {
	unsigned int das;
	unsigned int block;
	unsigned int level;
	unsigned int vector;
};

struct kq_nad_mparam_inform {
	unsigned int result;
	unsigned int inform1;
	unsigned int inform2;
	unsigned int inform3;
	unsigned int inform4;
	unsigned int inform5;
	struct kq_nad_fail fail;
};

#if IS_ENABLED(CONFIG_SEC_KQ_NAD_VDD_CAL)
struct kq_nad_mparam_vddcal {
	unsigned int lit;
	unsigned int mid;
	unsigned int big;
};
#endif //IS_ENABLED(CONFIG_SEC_KQ_NAD_VDD_CAL)

#if IS_ENABLED(CONFIG_SEC_KQ_CORRELATION_RESULT)
struct kq_nad_mparam_correlation {
	char info[KQ_NAD_CORRELATION_MPARAM_MAX_LEN];
};
#endif //IS_ENABLED(CONFIG_SEC_KQ_CORRELATION_RESULT)

struct kq_nad_env {
	unsigned int status;
	unsigned int shutdown;
	void __iomem *inform4;
	void __iomem *inform5;
	struct kq_nad_mparam_inform smd;
	struct kq_nad_mparam_inform smd_second;
	struct kq_nad_mparam_inform acat;
	struct kq_nad_mparam_inform acat_second;
	struct kq_nad_mparam_inform nadx;
	struct kq_nad_mparam_inform nadx_second;
	struct kq_nad_mparam_inform nadc;
	struct kq_nad_mparam_inform nadc_second;

	unsigned int rework;
	unsigned int skip;
	unsigned int ecc;
	unsigned int powermeter;

#if IS_ENABLED(CONFIG_SEC_KQ_NAD_VDD_CAL)
	struct kq_nad_mparam_vddcal vddcal;
#endif //IS_ENABLED(CONFIG_SEC_KQ_NAD_VDD_CAL)

#if IS_ENABLED(CONFIG_SEC_KQ_CORRELATION_RESULT)
	unsigned int correlation_magic;
	struct kq_nad_mparam_correlation correlation[KQ_NAD_MPARAM_CORRELATION_LOGIC_BLOCK_END];
#endif //IS_ENABLED(CONFIG_SEC_KQ_CORRELATION_RESULT)
};

static struct kq_nad_env kq_sec_nad_env;

#endif /* __KQ_NAD_H__ */
