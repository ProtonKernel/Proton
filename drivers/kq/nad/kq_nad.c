/* SPDX-License-Identifier: GPL-2.0-or-later */
/*
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * KQ(Kernel Quality) NAD driver implementation
 *	: SeungYoung Lee <seung0.lee@samsung.com>
 */

#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/types.h>
#include <linux/moduleparam.h>
#include <linux/init.h>
#include <linux/string.h>
#include <linux/platform_device.h>
#include <linux/kq/kq_nad.h>
#include <linux/sec_class.h>
#include <asm/io.h>

#if defined(CONFIG_SEC_KQ_NAD_API)
#include <linux/kq/kq_nad_api.h>
#endif

static unsigned int nad_status __initdata;
module_param(nad_status, uint, 0440);

static unsigned int nad_inform1 __initdata;
module_param(nad_inform1, uint, 0440);

static unsigned int nad_inform2 __initdata;
module_param(nad_inform2, uint, 0440);

static unsigned int nad_inform3 __initdata;
module_param(nad_inform3, uint, 0440);

static unsigned int nad_inform4 __initdata;
module_param(nad_inform4, uint, 0440);

static unsigned int nad_inform5 __initdata;
module_param(nad_inform5, uint, 0440);

static unsigned int nad_second_inform1 __initdata;
module_param(nad_second_inform1, uint, 0440);

static unsigned int nad_second_inform2 __initdata;
module_param(nad_second_inform2, uint, 0440);

static unsigned int nad_second_inform3 __initdata;
module_param(nad_second_inform3, uint, 0440);

static unsigned int nad_second_inform4 __initdata;
module_param(nad_second_inform4, uint, 0440);

static unsigned int nad_second_inform5 __initdata;
module_param(nad_second_inform5, uint, 0440);

static unsigned int acat_inform1 __initdata;
module_param(acat_inform1, uint, 0440);

static unsigned int acat_inform2 __initdata;
module_param(acat_inform2, uint, 0440);

static unsigned int acat_inform3 __initdata;
module_param(acat_inform3, uint, 0440);

static unsigned int acat_inform4 __initdata;
module_param(acat_inform4, uint, 0440);

static unsigned int acat_inform5 __initdata;
module_param(acat_inform5, uint, 0440);

static unsigned int acat_second_inform1 __initdata;
module_param(acat_second_inform1, uint, 0440);

static unsigned int acat_second_inform2 __initdata;
module_param(acat_second_inform2, uint, 0440);

static unsigned int acat_second_inform3 __initdata;
module_param(acat_second_inform3, uint, 0440);

static unsigned int acat_second_inform4 __initdata;
module_param(acat_second_inform4, uint, 0440);

static unsigned int acat_second_inform5 __initdata;
module_param(acat_second_inform5, uint, 0440);

static unsigned int nadx_inform1 __initdata;
module_param(nadx_inform1, uint, 0440);

static unsigned int nadx_inform2 __initdata;
module_param(nadx_inform2, uint, 0440);

static unsigned int nadx_inform3 __initdata;
module_param(nadx_inform3, uint, 0440);

static unsigned int nadx_inform4 __initdata;
module_param(nadx_inform4, uint, 0440);

static unsigned int nadx_inform5 __initdata;
module_param(nadx_inform5, uint, 0440);

static unsigned int nadx_second_inform1 __initdata;
module_param(nadx_second_inform1, uint, 0440);

static unsigned int nadx_second_inform2 __initdata;
module_param(nadx_second_inform2, uint, 0440);

static unsigned int nadx_second_inform3 __initdata;
module_param(nadx_second_inform3, uint, 0440);

static unsigned int nadx_second_inform4 __initdata;
module_param(nadx_second_inform4, uint, 0440);

static unsigned int nadx_second_inform5 __initdata;
module_param(nadx_second_inform5, uint, 0440);

static unsigned int nadc_inform1 __initdata;
module_param(nadc_inform1, uint, 0440);

static unsigned int nadc_inform2 __initdata;
module_param(nadc_inform2, uint, 0440);

static unsigned int nadc_inform3 __initdata;
module_param(nadc_inform3, uint, 0440);

static unsigned int nadc_second_inform1 __initdata;
module_param(nadc_second_inform1, uint, 0440);

static unsigned int nadc_second_inform2 __initdata;
module_param(nadc_second_inform2, uint, 0440);

static unsigned int nadc_second_inform3 __initdata;
module_param(nadc_second_inform3, uint, 0440);

static unsigned int nad_shutdown __initdata;
module_param(nad_shutdown, uint, 0440);

#if IS_ENABLED(CONFIG_SEC_KQ_NAD_VDD_CAL)
static unsigned int vddcal_data0 __initdata;
module_param(vddcal_data0, uint, 0440);

static unsigned int vddcal_data1 __initdata;
module_param(vddcal_data1, uint, 0440);

static unsigned int vddcal_data2 __initdata;
module_param(vddcal_data2, uint, 0440);
#endif

#if IS_ENABLED(CONFIG_SEC_KQ_NAD_V2)
unsigned int __read_mostly nad_powermeter;
EXPORT_SYMBOL(nad_powermeter);
module_param(nad_powermeter, uint, 0440);

unsigned int __read_mostly nad_rawpowermeter;
EXPORT_SYMBOL(nad_rawpowermeter);
module_param(nad_rawpowermeter, uint, 0440);

unsigned int __read_mostly nad_powermeter_npu;
EXPORT_SYMBOL(nad_powermeter_npu);
module_param(nad_powermeter_npu, uint, 0440);

unsigned int __read_mostly nad_asv;
EXPORT_SYMBOL(nad_asv);
module_param(nad_asv, uint, 0440);

unsigned int __read_mostly nad_xp;
EXPORT_SYMBOL(nad_xp);
module_param(nad_xp, uint, 0440);

unsigned int __read_mostly nad_ey;
EXPORT_SYMBOL(nad_ey);
module_param(nad_ey, uint, 0440);

unsigned int __read_mostly lsi_grade;
EXPORT_SYMBOL(lsi_grade);
module_param(lsi_grade, uint, 0440);

unsigned int __read_mostly nad_powermeter_leakage;
EXPORT_SYMBOL(nad_powermeter_leakage);
module_param(nad_powermeter_leakage, uint, 0440);

unsigned int __read_mostly nad_ecc;
EXPORT_SYMBOL(nad_ecc);
module_param(nad_ecc, uint, 0440);

unsigned int __read_mostly nad_skip;
EXPORT_SYMBOL(nad_skip);
module_param(nad_skip, uint, 0440);
#endif

#if IS_ENABLED(CONFIG_SEC_KQ_NAD_55) || IS_ENABLED(CONFIG_SEC_KQ_NAD_V2)
unsigned int __read_mostly nad_rework;
EXPORT_SYMBOL(nad_rework);
module_param(nad_rework, uint, 0440);
#endif

#if IS_ENABLED(CONFIG_ARM_EXYNOS_ACME_DISABLE_BOOT_LOCK)
int __read_mostly flexable_cpu_boot;
EXPORT_SYMBOL(flexable_cpu_boot);
module_param(flexable_cpu_boot, int, 0440);
#endif

#if IS_ENABLED(CONFIG_ARM_EXYNOS_DEVFREQ_DISABLE_BOOT_LOCK)
int __read_mostly flexable_dev_boot;
EXPORT_SYMBOL(flexable_dev_boot);
module_param(flexable_dev_boot, int, 0440);
#endif

#if IS_ENABLED(CONFIG_SEC_KQ_CORRELATION_RESULT)
static char correlation_info_mparam_cl0[KQ_NAD_CORRELATION_MPARAM_MAX_LEN] __initdata;
module_param_string(correlation_info_cl0, correlation_info_mparam_cl0, sizeof(correlation_info_mparam_cl0), 0440);

static char correlation_info_mparam_cl1[KQ_NAD_CORRELATION_MPARAM_MAX_LEN] __initdata;
module_param_string(correlation_info_cl1, correlation_info_mparam_cl1, sizeof(correlation_info_mparam_cl1), 0440);

static char correlation_info_mparam_cl2[KQ_NAD_CORRELATION_MPARAM_MAX_LEN] __initdata;
module_param_string(correlation_info_cl2, correlation_info_mparam_cl2, sizeof(correlation_info_mparam_cl2), 0440);

static char correlation_info_mparam_mif[KQ_NAD_CORRELATION_MPARAM_MAX_LEN] __initdata;
module_param_string(correlation_info_mif, correlation_info_mparam_mif, sizeof(correlation_info_mparam_mif), 0440);

static char correlation_info_mparam_dsu[KQ_NAD_CORRELATION_MPARAM_MAX_LEN] __initdata;
module_param_string(correlation_info_dsu, correlation_info_mparam_dsu, sizeof(correlation_info_mparam_dsu), 0440);

static char correlation_info_mparam_cp[KQ_NAD_CORRELATION_MPARAM_MAX_LEN] __initdata;
module_param_string(correlation_info_cp, correlation_info_mparam_cp, sizeof(correlation_info_mparam_cp), 0440);

static char correlation_info_mparam_g3d[KQ_NAD_CORRELATION_MPARAM_MAX_LEN] __initdata;
module_param_string(correlation_info_g3d, correlation_info_mparam_g3d, sizeof(correlation_info_mparam_g3d), 0440);

static char correlation_info_mparam_sci[KQ_NAD_CORRELATION_MPARAM_MAX_LEN] __initdata;
module_param_string(correlation_info_sci, correlation_info_mparam_sci, sizeof(correlation_info_mparam_sci), 0440);
#endif

#if IS_ENABLED(CONFIG_SEC_KQ_BPS_RESULT)
static char bps_result_mparam[KQ_NAD_BPS_MPARAM_MAX_LEN];
module_param_string(bps_result, bps_result_mparam, sizeof(bps_result_mparam), 0440);
#endif

#if IS_ENABLED(CONFIG_SEC_KQ_NAD_API)
static char api_result_mparam[KQ_NAD_API_MPARAM_MAX_LEN];
module_param_string(api_result, api_result_mparam, sizeof(api_result_mparam), 0440);
#endif

static ssize_t kq_nad_show_attrs(struct device *dev,
	struct device_attribute *attr, char *buf);
static ssize_t kq_nad_store_attrs(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t count);

#define KQ_NAD_ATTR(_name)	\
{	\
	.attr = { .name = #_name, .mode = 0664 },	\
	.show = kq_nad_show_attrs,	\
	.store = kq_nad_store_attrs,	\
}

static struct device_attribute kq_nad_attrs[] = {
	KQ_NAD_ATTR(nad_stat),
	KQ_NAD_ATTR(nad_erase),
	KQ_NAD_ATTR(nad_acat),
	KQ_NAD_ATTR(nad_dram),
	KQ_NAD_ATTR(nad_all),
	KQ_NAD_ATTR(nad_support),
#if IS_ENABLED(CONFIG_SEC_KQ_NAD_API)
	KQ_NAD_ATTR(nad_api),
#endif
	KQ_NAD_ATTR(nad_version),
#if IS_ENABLED(CONFIG_SEC_KQ_NAD_X) || IS_ENABLED(CONFIG_SEC_KQ_NAD_V2)
	KQ_NAD_ATTR(nad_fac_result),
	KQ_NAD_ATTR(nad_x_run),
#endif
	KQ_NAD_ATTR(nadc_fac_result),
	KQ_NAD_ATTR(nad_c_run),
	KQ_NAD_ATTR(nad_reboot),
};

static bool kq_nad_state_is_nad_fail(void)
{
	if (kq_sec_nad_env.smd.result
		&& kq_sec_nad_env.smd_second.result)
		return true;
	else if ((kq_sec_nad_env.smd.result == KQ_NAD_RESULT_PASS)
		&& kq_sec_nad_env.smd_second.result)
		return true;
	return false;
}

static bool kq_nad_state_is_dram_executed(void)
{
	if ((kq_sec_nad_env.status >> KQ_NAD_PHASE_DRAM) & 0x1)
		return true;
	return false;
}

static bool kq_nad_state_is_nadc_executed(void)
{
	if ((kq_sec_nad_env.status >> KQ_NAD_PHASE_CUSTOM_FIRST) & 0x1 ||
		(kq_sec_nad_env.status >> KQ_NAD_PHASE_CUSTOM_SECOND) & 0x1)
		return true;
	return false;
}

static bool kq_nad_state_is_acat_executed(void)
{
	if ((kq_sec_nad_env.status >> KQ_NAD_PHASE_ACAT_FIRST) & 0x1 ||
		(kq_sec_nad_env.status >> KQ_NAD_PHASE_ACAT_SECOND) & 0x1)
		return true;
	return false;
}

static bool kq_nad_state_is_acat_info_exist(void)
{
	if (kq_sec_nad_env.acat.result
		&& kq_sec_nad_env.acat_second.result)
		return true;
	else if ((kq_sec_nad_env.acat.result == KQ_NAD_RESULT_PASS)
		&& kq_sec_nad_env.acat_second.result)
		return true;
	return false;
}

#if IS_ENABLED(CONFIG_SEC_KQ_NAD_55) || IS_ENABLED(CONFIG_SEC_KQ_NAD_V2)
static bool kq_nad_state_is_nad_need_rework(void)
{
	if (kq_sec_nad_env.rework > 0)
		return true;
	return false;
}

static bool kq_nad_state_is_nad_sudden_power(void)
{
	if ((kq_sec_nad_env.smd.inform1 == 0)
		&& (kq_sec_nad_env.smd_second.inform1 == 0))
		return true;
	return false;
}

static bool kq_nad_state_is_nad_first_fail_second_pass(void)
{
	if ((kq_sec_nad_env.smd.result == KQ_NAD_RESULT_FAIL)
		&& (kq_sec_nad_env.smd_second.result == KQ_NAD_RESULT_PASS))
		return true;
	return false;
}

static bool kq_nad_state_is_rtc_timeout(void)
{
	if (((kq_sec_nad_env.smd.inform1 >> KQ_NAD_INFORM1_STATE) & 0x7) == KQ_NAD_RTC_TIMEOUT) {
		if ((kq_sec_nad_env.smd.inform3 & 0xFF) <= KQ_NAD_REWORK_CHECK_TN)
			return true;
		else
			return false;
	}
	return false;
}

static bool kq_nad_state_is_nad_wdt(void)
{
	if ((((kq_sec_nad_env.smd.inform1 >> KQ_NAD_INFORM1_MAGIC) & 0xFF) == KQ_NAD_FIRST_START_MAGIC)
		&& (((kq_sec_nad_env.smd_second.inform1 >> KQ_NAD_INFORM1_MAGIC) & 0xFF) == KQ_NAD_SECOND_START_MAGIC))
			return true;
	return false;
}
#endif

#if IS_ENABLED(CONFIG_SEC_KQ_NAD_X) || IS_ENABLED(CONFIG_SEC_KQ_NAD_V2)
static bool kq_nad_state_is_nadx_pass(void)
{
	if (kq_sec_nad_env.nadx.result == KQ_NAD_RESULT_PASS)
		return true;
	return false;
}

static bool kq_nad_state_is_nadx_fail(void)
{
	if (kq_sec_nad_env.nadx.result
		&& kq_sec_nad_env.nadx_second.result)
		return true;
	else if ((kq_sec_nad_env.nadx.result == KQ_NAD_RESULT_PASS)
		&& kq_sec_nad_env.nadx_second.result)
		return true;
	return false;
}

static bool kq_nad_state_is_main_nad_fail(void)
{
	if (kq_sec_nad_env.nadx.result == KQ_NAD_RESULT_MAIN)
		return true;
	return false;
}

static bool kq_nad_state_is_nado_shutdown(void)
{
	if (kq_sec_nad_env.shutdown == KQ_NAD_SHUTDOWN_ENABLE)
		return true;
	return false;
}

static bool kq_nad_state_is_nadx_first_fail_second_pass(void)
{
	if ((kq_sec_nad_env.nadx.result == KQ_NAD_RESULT_FAIL)
		&& (kq_sec_nad_env.nadx_second.result == KQ_NAD_RESULT_PASS))
		return true;
	return false;
}
#endif

static bool kq_nad_state_is_nadc_fail(void)
{
	if (kq_sec_nad_env.nadc.result
		&& kq_sec_nad_env.nadc_second.result)
		return true;
	else if ((kq_sec_nad_env.nadc.result == KQ_NAD_RESULT_PASS)
		&& kq_sec_nad_env.nadc_second.result)
		return true;
	return false;
}

#if IS_ENABLED(CONFIG_SEC_KQ_CORRELATION_RESULT)
static bool kq_nad_state_is_correlation_magic_invalid(void)
{
	if (kq_sec_nad_env.correlation_magic != KQ_NAD_CORRELATION_VALID_MAGIC)
		return true;
	return false;
}
#endif

static bool kq_nad_result_type_is_version(int type)
{
	if (type == KQ_NAD_RESULT_INFO_NAD_PASS ||
		type == KQ_NAD_RESULT_INFO_NAD_FAIL ||
		type == KQ_NAD_RESULT_INFO_ACAT_PASS ||
		type == KQ_NAD_RESULT_INFO_ACAT_FAIL ||
		type == KQ_NAD_RESULT_INFO_NADX_PASS ||
		type == KQ_NAD_RESULT_INFO_NADX_FAIL ||
		type == KQ_NAD_RESULT_INFO_NADC_PASS ||
		type == KQ_NAD_RESULT_INFO_NADC_FAIL
		)
		return true;
	return false;
}

static bool kq_nad_result_type_is_special(int type)
{
	if (type == KQ_NAD_RESULT_INFO_FNS
		|| type == KQ_NAD_RESULT_INFO_FN
		|| type == KQ_NAD_RESULT_INFO_NADC_SHUTDOWN
		|| type == KQ_NAD_RESULT_INFO_NADC_DETAIL
		)
		return true;
	return false;
}

static bool kq_nad_result_type_is_constant(int type)
{
	if (type == KQ_NAD_RESULT_INFO_NAD_INFORM1 ||
		type == KQ_NAD_RESULT_INFO_NAD_INFORM2 ||
		type == KQ_NAD_RESULT_INFO_NAD_INFORM3 ||
		type == KQ_NAD_RESULT_INFO_NAD_INFORM4 ||
		type == KQ_NAD_RESULT_INFO_NAD_INFORM5 ||
		type == KQ_NAD_RESULT_INFO_SNAD_INFORM1 ||
		type == KQ_NAD_RESULT_INFO_SNAD_INFORM2 ||
		type == KQ_NAD_RESULT_INFO_SNAD_INFORM3 ||
		type == KQ_NAD_RESULT_INFO_SNAD_INFORM4 ||
		type == KQ_NAD_RESULT_INFO_SNAD_INFORM5 ||
		type == KQ_NAD_RESULT_INFO_IT ||
		type == KQ_NAD_RESULT_INFO_MT ||
		type == KQ_NAD_RESULT_INFO_TN
#if IS_ENABLED(CONFIG_SEC_KQ_NAD_VDD_CAL)
		|| type == KQ_NAD_RESULT_INFO_VDD_CAL0
		|| type == KQ_NAD_RESULT_INFO_VDD_CAL1
		|| type == KQ_NAD_RESULT_INFO_VDD_CAL2
#endif
#if IS_ENABLED(CONFIG_SEC_KQ_NAD_55) || IS_ENABLED(CONFIG_SEC_KQ_NAD_V2)
		|| type == KQ_NAD_RESULT_INFO_REWORK
		|| type == KQ_NAD_RESULT_INFO_REWORK_RESULT
		|| type == KQ_NAD_RESULT_INFO_ECC0
		|| type == KQ_NAD_RESULT_INFO_ECC1
		|| type == KQ_NAD_RESULT_INFO_ECC2
		|| type == KQ_NAD_RESULT_INFO_ECC3
		|| type == KQ_NAD_RESULT_INFO_ECC4
		|| type == KQ_NAD_RESULT_INFO_ECC5
		|| type == KQ_NAD_RESULT_INFO_ECC6
		|| type == KQ_NAD_RESULT_INFO_ECC7
		|| type == KQ_NAD_RESULT_INFO_POWERMETER
		|| type == KQ_NAD_RESULT_INFO_RAWPOWERMETER
		|| type == KQ_NAD_RESULT_INFO_POWERMETER_NPU
		|| type == KQ_NAD_RESULT_INFO_POWERMETER_LEAKAGE
		|| type == KQ_NAD_RESULT_INFO_GT
		|| type == KQ_NAD_RESULT_INFO_NAD_ASV
		|| type == KQ_NAD_RESULT_INFO_NAD_XP
		|| type == KQ_NAD_RESULT_INFO_NAD_EY
		|| type == KQ_NAD_RESULT_INFO_LSI_GRADE
#endif
	)
		return true;
	return false;
}

static bool kq_nad_result_type_is_string(int type)
{
#if IS_ENABLED(CONFIG_SEC_KQ_CORRELATION_RESULT)
	if (type == KQ_NAD_RESULT_INFO_CORRELATION_CL0 ||
		type == KQ_NAD_RESULT_INFO_CORRELATION_CL1 ||
		type == KQ_NAD_RESULT_INFO_CORRELATION_CL2 ||
		type == KQ_NAD_RESULT_INFO_CORRELATION_MIF ||
		type == KQ_NAD_RESULT_INFO_CORRELATION_DSU ||
		type == KQ_NAD_RESULT_INFO_CORRELATION_CP ||
		type == KQ_NAD_RESULT_INFO_CORRELATION_G3D ||
		type == KQ_NAD_RESULT_INFO_CORRELATION_SCI)
		return true;
#endif
	return false;
}

static int kq_nad_add_version_type_data(char *buf, int maxlen,
	struct kq_nad_result *kq_nad_str_type)
{
	int len = 0;

	len = snprintf(buf, maxlen, "%s", kq_nad_str_type->format);

	return len;
}

static struct kq_nad_mparam_inform *kq_nad_get_inform_ptr(int type)
{
	struct kq_nad_mparam_inform *tmp = NULL;

	switch (type) {
	case KQ_NAD_PHASE_SMD_FIRST:
		tmp = &kq_sec_nad_env.smd;
		break;
	case KQ_NAD_PHASE_SMD_SECOND:
		tmp = &kq_sec_nad_env.smd_second;
		break;
	case KQ_NAD_PHASE_ACAT_FIRST:
		tmp = &kq_sec_nad_env.acat;
		break;
	case KQ_NAD_PHASE_ACAT_SECOND:
		tmp = &kq_sec_nad_env.acat_second;
		break;
#if IS_ENABLED(CONFIG_SEC_KQ_NAD_V2)
	case KQ_NAD_PHASE_EXTEND_FIRST:
	case KQ_NAD_PHASE_NADX_SHORT_FIRST:
		tmp = &kq_sec_nad_env.nadx;
		break;
	case KQ_NAD_PHASE_EXTEND_SECOND:
	case KQ_NAD_PHASE_NADX_SHORT_SECOND:
		tmp = &kq_sec_nad_env.nadx_second;
		break;
#elif IS_ENABLED(CONFIG_SEC_KQ_NAD_X)
	case KQ_NAD_PHASE_EXTEND_FIRST:
		tmp = &kq_sec_nad_env.nadx;
		break;
	case KQ_NAD_PHASE_EXTEND_SECOND:
		tmp = &kq_sec_nad_env.nadx_second;
		break;
	case KQ_NAD_PHASE_CUSTOM_FIRST:
		tmp = &kq_sec_nad_env.nadc;
		break;
	case KQ_NAD_PHASE_CUSTOM_SECOND:
		tmp = &kq_sec_nad_env.nadc_second;
		break;
#endif
	if (WARN_ON(!tmp))
		panic("invalid pointer of kq_nad phase\n");
	}

	return tmp;
}

static int kq_nad_add_constant_type_data(char *buf, int maxlen,
	struct kq_nad_result *kq_nad_constant_type)
{
	struct kq_nad_mparam_inform *nminfo = NULL;
	int len = 0;

#if IS_ENABLED(CONFIG_SEC_KQ_NAD_VDD_CAL)
	if (!(kq_nad_constant_type->type == KQ_NAD_RESULT_INFO_VDD_CAL0 ||
		kq_nad_constant_type->type == KQ_NAD_RESULT_INFO_VDD_CAL1 ||
		kq_nad_constant_type->type == KQ_NAD_RESULT_INFO_VDD_CAL2))
		nminfo = kq_nad_get_inform_ptr(kq_nad_constant_type->phase);
#else
	nminfo = kq_nad_get_inform_ptr(kq_nad_constant_type->phase);
#endif

	if (kq_nad_constant_type->type == KQ_NAD_RESULT_INFO_NAD_INFORM1)
		len = snprintf(buf, maxlen, kq_nad_constant_type->format,
			nminfo->inform1 ^ KQ_NAD_MAGIC_CONSTANT_XOR);
	else if (kq_nad_constant_type->type == KQ_NAD_RESULT_INFO_NAD_INFORM2)
		len = snprintf(buf, maxlen, kq_nad_constant_type->format,
			nminfo->inform2 ^ KQ_NAD_MAGIC_CONSTANT_XOR);
	else if (kq_nad_constant_type->type == KQ_NAD_RESULT_INFO_NAD_INFORM3)
		len = snprintf(buf, maxlen, kq_nad_constant_type->format,
			nminfo->inform3 ^ KQ_NAD_MAGIC_CONSTANT_XOR);
	else if (kq_nad_constant_type->type == KQ_NAD_RESULT_INFO_NAD_INFORM4)
		len = snprintf(buf, maxlen, kq_nad_constant_type->format,
			nminfo->inform4 ^ KQ_NAD_MAGIC_CONSTANT_XOR);
	else if (kq_nad_constant_type->type == KQ_NAD_RESULT_INFO_NAD_INFORM5)
		len = snprintf(buf, maxlen, kq_nad_constant_type->format,
			nminfo->inform5 ^ KQ_NAD_MAGIC_CONSTANT_XOR);
	else if (kq_nad_constant_type->type == KQ_NAD_RESULT_INFO_SNAD_INFORM1)
		len = snprintf(buf, maxlen, kq_nad_constant_type->format,
			nminfo->inform1 ^ KQ_NAD_MAGIC_CONSTANT_XOR);
	else if (kq_nad_constant_type->type == KQ_NAD_RESULT_INFO_SNAD_INFORM2)
		len = snprintf(buf, maxlen, kq_nad_constant_type->format,
			nminfo->inform2 ^ KQ_NAD_MAGIC_CONSTANT_XOR);
	else if (kq_nad_constant_type->type == KQ_NAD_RESULT_INFO_SNAD_INFORM3)
		len = snprintf(buf, maxlen, kq_nad_constant_type->format,
			nminfo->inform3 ^ KQ_NAD_MAGIC_CONSTANT_XOR);
	else if (kq_nad_constant_type->type == KQ_NAD_RESULT_INFO_SNAD_INFORM4)
		len = snprintf(buf, maxlen, kq_nad_constant_type->format,
			nminfo->inform4 ^ KQ_NAD_MAGIC_CONSTANT_XOR);
	else if (kq_nad_constant_type->type == KQ_NAD_RESULT_INFO_SNAD_INFORM5)
		len = snprintf(buf, maxlen, kq_nad_constant_type->format,
			nminfo->inform5 ^ KQ_NAD_MAGIC_CONSTANT_XOR);
	else if (kq_nad_constant_type->type == KQ_NAD_RESULT_INFO_IT)
		len = snprintf(buf, maxlen, kq_nad_constant_type->format,
			nminfo->inform2 & 0xFF);
	else if (kq_nad_constant_type->type == KQ_NAD_RESULT_INFO_MT)
		len = snprintf(buf, maxlen, kq_nad_constant_type->format,
			(nminfo->inform2 >> 8) & 0xFF);
#if IS_ENABLED(CONFIG_SEC_KQ_NAD_V2)
	else if (kq_nad_constant_type->type == KQ_NAD_RESULT_INFO_TN)
		len = snprintf(buf, maxlen, kq_nad_constant_type->format,
			nminfo->inform5 & 0xFF);
#else
	else if (kq_nad_constant_type->type == KQ_NAD_RESULT_INFO_TN)
		len = snprintf(buf, maxlen, kq_nad_constant_type->format,
			nminfo->inform3 & 0xFF);
#endif
#if IS_ENABLED(CONFIG_SEC_KQ_NAD_55) || IS_ENABLED(CONFIG_SEC_KQ_NAD_V2)
	else if (kq_nad_constant_type->type == KQ_NAD_RESULT_INFO_REWORK)
		len = snprintf(buf, maxlen, kq_nad_constant_type->format,
			kq_sec_nad_env.rework);
	else if (kq_nad_constant_type->type == KQ_NAD_RESULT_INFO_REWORK_RESULT)
		len = snprintf(buf, maxlen, kq_nad_constant_type->format,
			nad_rework);
	else if (kq_nad_constant_type->type == KQ_NAD_RESULT_INFO_ECC0)
		len = snprintf(buf, maxlen, kq_nad_constant_type->format,
			(nad_ecc >> KQ_NAD_ECC0) & 0xF);
	else if (kq_nad_constant_type->type == KQ_NAD_RESULT_INFO_ECC1)
		len = snprintf(buf, maxlen, kq_nad_constant_type->format,
			(nad_ecc >> KQ_NAD_ECC1) & 0xF);
	else if (kq_nad_constant_type->type == KQ_NAD_RESULT_INFO_ECC2)
		len = snprintf(buf, maxlen, kq_nad_constant_type->format,
			(nad_ecc >> KQ_NAD_ECC2) & 0xF);
	else if (kq_nad_constant_type->type == KQ_NAD_RESULT_INFO_ECC3)
		len = snprintf(buf, maxlen, kq_nad_constant_type->format,
			(nad_ecc >> KQ_NAD_ECC3) & 0xF);
	else if (kq_nad_constant_type->type == KQ_NAD_RESULT_INFO_ECC4)
		len = snprintf(buf, maxlen, kq_nad_constant_type->format,
			(nad_ecc >> KQ_NAD_ECC4) & 0xF);
	else if (kq_nad_constant_type->type == KQ_NAD_RESULT_INFO_ECC5)
		len = snprintf(buf, maxlen, kq_nad_constant_type->format,
			(nad_ecc >> KQ_NAD_ECC5) & 0xF);
	else if (kq_nad_constant_type->type == KQ_NAD_RESULT_INFO_ECC6)
		len = snprintf(buf, maxlen, kq_nad_constant_type->format,
			(nad_ecc >> KQ_NAD_ECC6) & 0xF);
	else if (kq_nad_constant_type->type == KQ_NAD_RESULT_INFO_ECC7)
		len = snprintf(buf, maxlen, kq_nad_constant_type->format,
			(nad_ecc >> KQ_NAD_ECC7) & 0xF);
	else if (kq_nad_constant_type->type == KQ_NAD_RESULT_INFO_POWERMETER)
		len = snprintf(buf, maxlen, kq_nad_constant_type->format,
			nad_powermeter);
	else if (kq_nad_constant_type->type == KQ_NAD_RESULT_INFO_RAWPOWERMETER)
		len = snprintf(buf, maxlen, kq_nad_constant_type->format,
			nad_rawpowermeter);
	else if (kq_nad_constant_type->type == KQ_NAD_RESULT_INFO_POWERMETER_NPU)
		len = snprintf(buf, maxlen, kq_nad_constant_type->format,
			nad_powermeter_npu);
	else if (kq_nad_constant_type->type == KQ_NAD_RESULT_INFO_POWERMETER_LEAKAGE)
		len = snprintf(buf, maxlen, kq_nad_constant_type->format,
			nad_powermeter_leakage);
	else if (kq_nad_constant_type->type == KQ_NAD_RESULT_INFO_GT)
		len = snprintf(buf, maxlen, kq_nad_constant_type->format,
			(nminfo->inform5 >> 16) & 0xFF);
	else if (kq_nad_constant_type->type == KQ_NAD_RESULT_INFO_NAD_ASV)
		len = snprintf(buf, maxlen, kq_nad_constant_type->format,
			nad_asv);
	else if (kq_nad_constant_type->type == KQ_NAD_RESULT_INFO_NAD_XP)
		len = snprintf(buf, maxlen, kq_nad_constant_type->format,
			nad_xp);
	else if (kq_nad_constant_type->type == KQ_NAD_RESULT_INFO_NAD_EY)
		len = snprintf(buf, maxlen, kq_nad_constant_type->format,
			nad_ey);
	else if (kq_nad_constant_type->type == KQ_NAD_RESULT_INFO_LSI_GRADE)
		len = snprintf(buf, maxlen, kq_nad_constant_type->format,
			lsi_grade);

#endif
#if IS_ENABLED(CONFIG_SEC_KQ_NAD_VDD_CAL)
	else if (kq_nad_constant_type->type == KQ_NAD_RESULT_INFO_VDD_CAL0)
		len = snprintf(buf, maxlen, kq_nad_constant_type->format,
			kq_sec_nad_env.vddcal.lit ^ KQ_NAD_MAGIC_CONSTANT_XOR);
	else if (kq_nad_constant_type->type == KQ_NAD_RESULT_INFO_VDD_CAL1)
		len = snprintf(buf, maxlen, kq_nad_constant_type->format,
			kq_sec_nad_env.vddcal.mid ^ KQ_NAD_MAGIC_CONSTANT_XOR);
	else if (kq_nad_constant_type->type == KQ_NAD_RESULT_INFO_VDD_CAL2)
		len = snprintf(buf, maxlen, kq_nad_constant_type->format,
			kq_sec_nad_env.vddcal.big ^ KQ_NAD_MAGIC_CONSTANT_XOR);
#endif

	return len;
}

static char *kq_nad_get_das_name(struct kq_nad_mparam_inform *kq_nad_result_type)
{
	if (kq_nad_result_type->fail.das >= KQ_NAD_DAS_START &&
		kq_nad_result_type->fail.das < KQ_NAD_DAS_END)
		return kq_nad_das_name[kq_nad_result_type->fail.das];
	return "None";
}

static char *kq_nad_get_block_name(struct kq_nad_mparam_inform *kq_nad_result_type)
{
	if (kq_nad_result_type->fail.block >= KQ_NAD_BLOCK_START &&
		kq_nad_result_type->fail.block < KQ_NAD_BLOCK_END)
		return kq_nad_block_name[kq_nad_result_type->fail.block];
	return "None";
}

static char *kq_nad_get_nadc_shutdown(struct kq_nad_mparam_inform *kq_nad_result_type)
{
	if (kq_sec_nad_env.shutdown == KQ_NAD_SHUTDOWN_ENABLE)
		return "_SHUTDOWN_";
	else
		return "_";
}

static char *kq_nad_get_nadc_detail(struct kq_nad_mparam_inform *kq_nad_result_type)
{
	if (kq_nad_result_type->result == KQ_NAD_RESULT_FAIL)
		return kq_nad_das_name[kq_nad_result_type->fail.das];
	return "DEF";
}

static int kq_nad_add_special_type_data(char *buf, int maxlen,
	struct kq_nad_result *kq_nad_special_type)
{
	struct kq_nad_mparam_inform *nminfo = NULL;
	int len = 0;

	nminfo = kq_nad_get_inform_ptr(kq_nad_special_type->phase);

	if (kq_nad_special_type->type == KQ_NAD_RESULT_INFO_FNS)
		len = snprintf(buf, maxlen,
			kq_nad_special_type->format,
			kq_nad_get_das_name(nminfo),
			kq_nad_get_block_name(nminfo),
			nminfo->fail.level,
			kq_nad_block_data[nminfo->fail.block].block[nminfo->fail.vector]);
	else if (kq_nad_special_type->type == KQ_NAD_RESULT_INFO_FN)
		len = snprintf(buf, maxlen,
			kq_nad_special_type->format,
			kq_nad_get_das_name(nminfo),
			kq_nad_get_block_name(nminfo),
			nminfo->fail.level,
			kq_nad_block_data[nminfo->fail.block].block[nminfo->fail.vector]);
	else if (kq_nad_special_type->type == KQ_NAD_RESULT_INFO_NADC_SHUTDOWN)
		len = snprintf(buf, maxlen,
			kq_nad_special_type->format,
			kq_nad_get_nadc_shutdown(nminfo));
	else if (kq_nad_special_type->type == KQ_NAD_RESULT_INFO_NADC_DETAIL)
		len = snprintf(buf, maxlen,
			kq_nad_special_type->format,
			kq_nad_get_nadc_detail(nminfo));

	return len;
}

static int kq_nad_add_string_type_data(char *buf, int maxlen,
	struct kq_nad_result *kq_nad_string_type)
{
	int len = 0;

#if IS_ENABLED(CONFIG_SEC_KQ_CORRELATION_RESULT)
	/* correlation result is valid only if magic equals valid magic */
	if (kq_nad_state_is_correlation_magic_invalid())
		return -1;

	if (kq_nad_string_type->type == KQ_NAD_RESULT_INFO_CORRELATION_CL0)
		len = snprintf(buf, maxlen, kq_nad_string_type->format,
			kq_sec_nad_env.correlation[KQ_NAD_MPARAM_CORRELATION_LOGIC_BLOCK_CL0].info);
	else if (kq_nad_string_type->type == KQ_NAD_RESULT_INFO_CORRELATION_CL1)
		len = snprintf(buf, maxlen, kq_nad_string_type->format,
			kq_sec_nad_env.correlation[KQ_NAD_MPARAM_CORRELATION_LOGIC_BLOCK_CL1].info);
	else if (kq_nad_string_type->type == KQ_NAD_RESULT_INFO_CORRELATION_CL2)
		len = snprintf(buf, maxlen, kq_nad_string_type->format,
			kq_sec_nad_env.correlation[KQ_NAD_MPARAM_CORRELATION_LOGIC_BLOCK_CL2].info);
	else if (kq_nad_string_type->type == KQ_NAD_RESULT_INFO_CORRELATION_MIF)
		len = snprintf(buf, maxlen, kq_nad_string_type->format,
			kq_sec_nad_env.correlation[KQ_NAD_MPARAM_CORRELATION_LOGIC_BLOCK_MIF].info);
	else if (kq_nad_string_type->type == KQ_NAD_RESULT_INFO_CORRELATION_DSU)
		len = snprintf(buf, maxlen, kq_nad_string_type->format,
			kq_sec_nad_env.correlation[KQ_NAD_MPARAM_CORRELATION_LOGIC_BLOCK_DSU].info);
	else if (kq_nad_string_type->type == KQ_NAD_RESULT_INFO_CORRELATION_CP)
		len = snprintf(buf, maxlen, kq_nad_string_type->format,
			kq_sec_nad_env.correlation[KQ_NAD_MPARAM_CORRELATION_LOGIC_BLOCK_CP].info);
	else if (kq_nad_string_type->type == KQ_NAD_RESULT_INFO_CORRELATION_G3D)
		len = snprintf(buf, maxlen, kq_nad_string_type->format,
			kq_sec_nad_env.correlation[KQ_NAD_MPARAM_CORRELATION_LOGIC_BLOCK_G3D].info);
	else if (kq_nad_string_type->type == KQ_NAD_RESULT_INFO_CORRELATION_SCI)
		len = snprintf(buf, maxlen, kq_nad_string_type->format,
			kq_sec_nad_env.correlation[KQ_NAD_MPARAM_CORRELATION_LOGIC_BLOCK_SCI].info);
#endif

	return len;
}

static int kq_nad_add_result_delimiter(char *buf, int maxlen)
{
	int len = 0;

	len = snprintf(buf, maxlen, "%c", ',');

	return len;
}

#if IS_ENABLED(CONFIG_SEC_KQ_BPS_RESULT)
static bool kq_nad_bps_mparam_is_invalid(char *token)
{
	int i = 0;
	int delimiter_cnt = 0;
	int token_len = strnlen(token, KQ_NAD_BPS_MPARAM_MAX_LEN);

	pr_info("%s : %d\n", __func__, token_len);

	/* if size equal 0, it means there's no bps information */
	if (token_len == 0)
		return true;

	for (i = 0; i < token_len; i++) {
		if (token[i] == '_')
			delimiter_cnt += 1;
	}

	if (delimiter_cnt != KQ_NAD_BPS_MPARAM_MAX_DELIMITER)
		return true;

	return false;
}

static int kq_nad_add_result_if_bps_exist(char *buf, int maxlen)
{
	int len = 0;

	/* check if valid format */
	if (kq_nad_bps_mparam_is_invalid(bps_result_mparam))
		return len;

	len = snprintf(buf, maxlen, ",BPS(%s)", bps_result_mparam);

	return len;
}
#endif

#if IS_ENABLED(CONFIG_SEC_KQ_NAD_V2)
static int kq_nad_add_result_nad_skip(char *buf, int maxlen)
{
	int len = 0;
	int i;
	int maxloop = sizeof(kq_nad_result_nad_skip)/sizeof(struct kq_nad_result);

	for (i = 0; i < maxloop; i++) {
		if (kq_nad_result_type_is_version(kq_nad_result_nad_skip[i].type))
			len += kq_nad_add_version_type_data(buf + len, maxlen - len, &kq_nad_result_nad_skip[i]);
	}

	return len;
}

static bool kq_nad_state_is_nad_skipping(void)
{
    if(kq_sec_nad_env.skip == KQ_NAD_SKIP_ENABLE)
        return true;
    else
        return false;
}
#endif

static int kq_nad_add_result_nad_pass(char *buf, int maxlen)
{
	int len = 0;
	int i;
	int maxloop = sizeof(kq_nad_result_nad_pass)/sizeof(struct kq_nad_result);

	for (i = 0; i < maxloop; i++) {
		if (kq_nad_result_type_is_version(kq_nad_result_nad_pass[i].type))
			len += kq_nad_add_version_type_data(buf + len, maxlen - len, &kq_nad_result_nad_pass[i]);
		else if (kq_nad_result_type_is_special(kq_nad_result_nad_pass[i].type))
			len += kq_nad_add_special_type_data(buf + len, maxlen - len, &kq_nad_result_nad_pass[i]);
		else if (kq_nad_result_type_is_constant(kq_nad_result_nad_pass[i].type))
			len += kq_nad_add_constant_type_data(buf + len, maxlen - len, &kq_nad_result_nad_pass[i]);
		else if (kq_nad_result_type_is_string(kq_nad_result_nad_pass[i].type))
			len += kq_nad_add_string_type_data(buf + len, maxlen - len, &kq_nad_result_nad_pass[i]);

		if (i != (maxloop - 1))
			len += kq_nad_add_result_delimiter(buf + len, maxlen - len);
	}
#if IS_ENABLED(CONFIG_SEC_KQ_BPS_RESULT)
	len += kq_nad_add_result_if_bps_exist(buf + len, maxlen - len);
#endif

	return len;
}

static int kq_nad_add_result_nad_fail(char *buf, int maxlen)
{
	int len = 0;
	int i;
	int maxloop = sizeof(kq_nad_result_nad_fail)/sizeof(struct kq_nad_result);

	for (i = 0; i < maxloop; i++) {
		if (kq_nad_result_type_is_version(kq_nad_result_nad_fail[i].type))
			len += kq_nad_add_version_type_data(buf + len, maxlen - len, &kq_nad_result_nad_fail[i]);
		else if (kq_nad_result_type_is_special(kq_nad_result_nad_fail[i].type))
			len += kq_nad_add_special_type_data(buf + len, maxlen - len, &kq_nad_result_nad_fail[i]);
		else if (kq_nad_result_type_is_constant(kq_nad_result_nad_fail[i].type))
			len += kq_nad_add_constant_type_data(buf + len, maxlen - len, &kq_nad_result_nad_fail[i]);
		else if (kq_nad_result_type_is_string(kq_nad_result_nad_fail[i].type))
			len += kq_nad_add_string_type_data(buf + len, maxlen - len, &kq_nad_result_nad_fail[i]);

		if (i != (maxloop - 1))
			len += kq_nad_add_result_delimiter(buf + len, maxlen - len);
	}
#if IS_ENABLED(CONFIG_SEC_KQ_BPS_RESULT)
	len += kq_nad_add_result_if_bps_exist(buf + len, maxlen - len);
#endif

	return len;
}

static int kq_nad_add_result_acat_pass(char *buf, int maxlen)
{
	int len = 0;
	int i;
	int maxloop = sizeof(kq_nad_result_acat_pass)/sizeof(struct kq_nad_result);

	for (i = 0; i < maxloop; i++) {
		if (kq_nad_result_type_is_version(kq_nad_result_acat_pass[i].type))
			len += kq_nad_add_version_type_data(buf + len, maxlen - len, &kq_nad_result_acat_pass[i]);
		else if (kq_nad_result_type_is_special(kq_nad_result_acat_pass[i].type))
			len += kq_nad_add_special_type_data(buf + len, maxlen - len, &kq_nad_result_acat_pass[i]);
		else if (kq_nad_result_type_is_constant(kq_nad_result_acat_pass[i].type))
			len += kq_nad_add_constant_type_data(buf + len, maxlen - len, &kq_nad_result_acat_pass[i]);
		else if (kq_nad_result_type_is_string(kq_nad_result_acat_pass[i].type))
			len += kq_nad_add_string_type_data(buf + len, maxlen - len, &kq_nad_result_acat_pass[i]);

		if (i != (maxloop - 1))
			len += kq_nad_add_result_delimiter(buf + len, maxlen - len);
	}
#if IS_ENABLED(CONFIG_SEC_KQ_BPS_RESULT)
	len += kq_nad_add_result_if_bps_exist(buf + len, maxlen - len);
#endif

	return len;
}

static int kq_nad_add_result_acat_fail(char *buf, int maxlen)
{
	int len = 0;
	int i;
	int maxloop = sizeof(kq_nad_result_acat_fail)/sizeof(struct kq_nad_result);

	for (i = 0; i < maxloop; i++) {
		if (kq_nad_result_type_is_version(kq_nad_result_acat_fail[i].type))
			len += kq_nad_add_version_type_data(buf + len, maxlen - len, &kq_nad_result_acat_fail[i]);
		else if (kq_nad_result_type_is_special(kq_nad_result_acat_fail[i].type))
			len += kq_nad_add_special_type_data(buf + len, maxlen - len, &kq_nad_result_acat_fail[i]);
		else if (kq_nad_result_type_is_constant(kq_nad_result_acat_fail[i].type))
			len += kq_nad_add_constant_type_data(buf + len, maxlen - len, &kq_nad_result_acat_fail[i]);
		else if (kq_nad_result_type_is_string(kq_nad_result_acat_fail[i].type))
			len += kq_nad_add_string_type_data(buf + len, maxlen - len, &kq_nad_result_acat_fail[i]);

		if (i != (maxloop - 1))
			len += kq_nad_add_result_delimiter(buf + len, maxlen - len);
	}
#if IS_ENABLED(CONFIG_SEC_KQ_BPS_RESULT)
	len += kq_nad_add_result_if_bps_exist(buf + len, maxlen - len);
#endif

	return len;
}

#if IS_ENABLED(CONFIG_SEC_KQ_NAD_55) || IS_ENABLED(CONFIG_SEC_KQ_NAD_V2)
static int kq_nad_add_result_nad_rework(char *buf, int maxlen)
{
	int len = 0;
	int i;
	int maxloop = sizeof(kq_nad_result_nad_rework)/sizeof(struct kq_nad_result);

	for (i = 0; i < maxloop; i++) {
		if (kq_nad_result_type_is_version(kq_nad_result_nad_rework[i].type))
			len += kq_nad_add_version_type_data(buf + len, maxlen - len, &kq_nad_result_nad_rework[i]);
		else if (kq_nad_result_type_is_constant(kq_nad_result_nad_rework[i].type))
			len += kq_nad_add_constant_type_data(buf + len, maxlen - len, &kq_nad_result_nad_rework[i]);

		if (i != (maxloop - 1))
			len += kq_nad_add_result_delimiter(buf + len, maxlen - len);
	}

	return len;
}
#endif

#if IS_ENABLED(CONFIG_SEC_KQ_NAD_X) || IS_ENABLED(CONFIG_SEC_KQ_NAD_V2)
static int kq_nad_add_result_nadx_pass(char *buf, int maxlen)
{
	int len = 0;
	int i;
	int maxloop = sizeof(kq_nad_result_nadx_pass)/sizeof(struct kq_nad_result);

	for (i = 0; i < maxloop; i++) {
		if (kq_nad_result_type_is_version(kq_nad_result_nadx_pass[i].type))
			len += kq_nad_add_version_type_data(buf + len, maxlen - len, &kq_nad_result_nadx_pass[i]);
		else if (kq_nad_result_type_is_special(kq_nad_result_nadx_pass[i].type))
			len += kq_nad_add_special_type_data(buf + len, maxlen - len, &kq_nad_result_nadx_pass[i]);
		else if (kq_nad_result_type_is_constant(kq_nad_result_nadx_pass[i].type))
			len += kq_nad_add_constant_type_data(buf + len, maxlen - len, &kq_nad_result_nadx_pass[i]);
		else if (kq_nad_result_type_is_string(kq_nad_result_nadx_pass[i].type))
			len += kq_nad_add_string_type_data(buf + len, maxlen - len, &kq_nad_result_nadx_pass[i]);

		if (i != (maxloop - 1))
			len += kq_nad_add_result_delimiter(buf + len, maxlen - len);
	}
#if IS_ENABLED(CONFIG_SEC_KQ_BPS_RESULT)
	len += kq_nad_add_result_if_bps_exist(buf + len, maxlen - len);
#endif

	return len;
}

static int kq_nad_add_result_nadx_fail(char *buf, int maxlen)
{
	int len = 0;
	int i;
	int maxloop = sizeof(kq_nad_result_nadx_fail)/sizeof(struct kq_nad_result);

	for (i = 0; i < maxloop; i++) {
		if (kq_nad_result_type_is_version(kq_nad_result_nadx_fail[i].type))
			len += kq_nad_add_version_type_data(buf + len, maxlen - len, &kq_nad_result_nadx_fail[i]);
		else if (kq_nad_result_type_is_special(kq_nad_result_nadx_fail[i].type))
			len += kq_nad_add_special_type_data(buf + len, maxlen - len, &kq_nad_result_nadx_fail[i]);
		else if (kq_nad_result_type_is_constant(kq_nad_result_nadx_fail[i].type))
			len += kq_nad_add_constant_type_data(buf + len, maxlen - len, &kq_nad_result_nadx_fail[i]);
		else if (kq_nad_result_type_is_string(kq_nad_result_nadx_fail[i].type))
			len += kq_nad_add_string_type_data(buf + len, maxlen - len, &kq_nad_result_nadx_fail[i]);

		if (i != (maxloop - 1))
			len += kq_nad_add_result_delimiter(buf + len, maxlen - len);
	}
#if IS_ENABLED(CONFIG_SEC_KQ_BPS_RESULT)
	len += kq_nad_add_result_if_bps_exist(buf + len, maxlen - len);
#endif

	return len;
}
#endif

static int kq_nad_add_result_nadc_fail(char *buf, int maxlen)
{
	int len = 0;
	int i;
	int maxloop = sizeof(kq_nad_result_nadc_fail)/sizeof(struct kq_nad_result);

	for (i = 0; i < maxloop; i++) {
		if (kq_nad_result_type_is_version(kq_nad_result_nadc_fail[i].type))
			len += kq_nad_add_version_type_data(buf + len, maxlen - len, &kq_nad_result_nadc_fail[i]);
		else if (kq_nad_result_type_is_special(kq_nad_result_nadc_fail[i].type))
			len += kq_nad_add_special_type_data(buf + len, maxlen - len, &kq_nad_result_nadc_fail[i]);
		else if (kq_nad_result_type_is_constant(kq_nad_result_nadc_fail[i].type))
			len += kq_nad_add_constant_type_data(buf + len, maxlen - len, &kq_nad_result_nadc_fail[i]);

		if (i != (maxloop - 1))
			len += kq_nad_add_result_delimiter(buf + len, maxlen - len);
	}

	return len;
}

#if IS_ENABLED(CONFIG_SEC_KQ_NAD_55) || IS_ENABLED(CONFIG_SEC_KQ_NAD_V2)
static void kq_nad_set_rework_to_inform(void)
{
	u32 inform4_data;

	inform4_data = KQ_NAD_MAGIC_ERASE_REWORK;
#if IS_ENABLED(CONFIG_SEC_KQ_NAD_V2)
	inform4_data |= KQ_NAD_CMD_MAGIC << 16;
#endif
	writel(inform4_data, kq_sec_nad_env.inform4);
	writel(kq_sec_nad_env.rework, kq_sec_nad_env.inform5);
}

static void kq_nad_check_rework(void)
{
	if (kq_nad_state_is_nad_sudden_power())
		kq_sec_nad_env.rework = KQ_NAD_REWORK_SUDDEN_POWER_OFF;
	else if (kq_nad_state_is_nad_first_fail_second_pass())
		kq_sec_nad_env.rework = KQ_NAD_REWORK_FIRST_FAIL;
	else if (kq_nad_state_is_rtc_timeout())
		kq_sec_nad_env.rework = KQ_NAD_REWORK_RTC_TIME_OVER;
	else if (kq_nad_state_is_nad_wdt())
		kq_sec_nad_env.rework = KQ_NAD_REWORK_NAD_WDT;
	else
		kq_sec_nad_env.rework = 0;

	pr_info("%s: %d\n", __func__, kq_sec_nad_env.rework);
	pr_info("%s: nad_rework : %d\n", __func__, nad_rework);
}
#endif

static ssize_t kq_nad_show_stat_attr(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	int len = 0;

	if (kq_nad_state_is_nad_fail())
		len = kq_nad_add_result_nad_fail(buf, PAGE_SIZE);
#if IS_ENABLED(CONFIG_SEC_KQ_NAD_V2)
	else if (kq_nad_state_is_nad_skipping())
		len = kq_nad_add_result_nad_skip(buf, PAGE_SIZE);
#endif
#if IS_ENABLED(CONFIG_SEC_KQ_NAD_55) || IS_ENABLED(CONFIG_SEC_KQ_NAD_V2)
	else if (kq_nad_state_is_nad_need_rework())
		len = kq_nad_add_result_nad_rework(buf, PAGE_SIZE);
#endif
	else
		len = kq_nad_add_result_nad_pass(buf, PAGE_SIZE);

	return len;
}

static ssize_t kq_nad_show_erase_attr(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	u32 inform4_data;

	inform4_data = readl(kq_sec_nad_env.inform4);

	if (inform4_data == KQ_NAD_MAGIC_ERASE)
		return sprintf(buf, "OK\n");
	else
		return sprintf(buf, "NG\n");
}

static ssize_t kq_nad_show_acat_attr(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	int len = 0;

	/* Check status if ACAT NAD was excuted */
	if (kq_nad_state_is_acat_executed()) {
		if (kq_nad_state_is_acat_info_exist())
			len = kq_nad_add_result_acat_fail(buf, PAGE_SIZE);
		else
			len = kq_nad_add_result_acat_pass(buf, PAGE_SIZE);
	} else
		len = sprintf(buf, "NO_3.0_L_NADTEST\n");

	return len;
}

static ssize_t kq_nad_show_dram_attr(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	if (kq_nad_state_is_dram_executed())
		return sprintf(buf, "OK_DRAM\n");
	else
		return sprintf(buf, "NO_DRAMTEST\n");
}

static ssize_t kq_nad_show_all_attr(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	return 0;
}

static ssize_t kq_nad_show_support_attr(struct device *dev,
	struct device_attribute *attr, char *buf)
{
#if IS_ENABLED(CONFIG_SOC_EXYNOS2100)
		return sprintf(buf, "SUPPORT_NADC\n");
#else
		return sprintf(buf, "SUPPORT\n");
#endif
}

#if IS_ENABLED(CONFIG_SEC_KQ_NAD_API)
/*
 * api_result_mparam {api_result}_{pin_cnt}_{result_each_pins[0: success, 1: fail]}
 * : OK_14_00000000000000, NG_14_00000100000000
 */
static ssize_t kq_nad_show_api_attr(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	int i = 0;
	int ret = 0;
	int len = 0;
	int cur_idx = 0;
	int pin_idx = 0;
	int delimiter_cnt = 0;
	int first_delimiter_idx = 0;
	int second_delimiter_idx = 0;
	int str_len = strnlen(api_result_mparam, KQ_NAD_API_MPARAM_MAX_LEN);
	char tmp_api_result[KQ_NAD_API_MPARAM_MAX_LEN] = { 0 };

	pr_info("%s mparam: %s, str_len: %d\n", __func__, api_result_mparam, str_len);

	if (str_len == 0 || str_len == KQ_NAD_API_MPARAM_MAX_LEN)
		return sprintf(buf, "NONE\n");

	for (i = 0; i < str_len; i++) {
		if (api_result_mparam[i] == '_') {
			delimiter_cnt++;
			switch (delimiter_cnt) {
			case 1:
				first_delimiter_idx = i;
				break;
			case 2:
				second_delimiter_idx = i;
				break;
			default:
				return sprintf(buf, "NONE\n");
			}
		}
	}

	if (delimiter_cnt == KQ_NAD_API_MPARAM_MAX_DELIMITER) {
		len = first_delimiter_idx;
		strncpy(tmp_api_result, api_result_mparam, len);
		ret = (strncmp(tmp_api_result, "OK", 2) == 0) ? KQ_NAD_API_SUCCESS : KQ_NAD_API_FAIL;

		len = second_delimiter_idx - first_delimiter_idx - 1;
		strncpy(tmp_api_result, api_result_mparam + first_delimiter_idx + 1, len);

		cur_idx += snprintf(buf, 4, (ret == KQ_NAD_API_SUCCESS) ? "OK_" : "NG_");
		cur_idx += snprintf(buf + cur_idx, strlen(tmp_api_result) + 1, "%s", tmp_api_result);

		if (ret == KQ_NAD_API_FAIL) {
			for (i = second_delimiter_idx + 1, pin_idx = 0; i < str_len; i++, pin_idx++) {
				if (api_result_mparam[i] == '1') {
					cur_idx += snprintf(buf + cur_idx, 2, ",");
					cur_idx += snprintf(buf + cur_idx, strlen(kq_nad_api_pin_list[pin_idx].name) + 1,
						"%s", kq_nad_api_pin_list[pin_idx].name);
				}
			}
		}
		snprintf(buf + cur_idx, 2, "\n");
		return strlen(buf);
	} else
		return sprintf(buf, "NONE\n");
}
#endif

static ssize_t kq_nad_show_version_attr(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	return 0;
}

#if IS_ENABLED(CONFIG_SEC_KQ_NAD_X) || IS_ENABLED(CONFIG_SEC_KQ_NAD_V2)
static ssize_t kq_nad_show_fac_result_attr(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	if (kq_nad_state_is_nadx_pass()) {
		if (kq_nad_state_is_nado_shutdown())
			return sprintf(buf, "%s\n", "OFF,MAINNAD");
		else
			return kq_nad_add_result_nadx_pass(buf, PAGE_SIZE);
	} else if (kq_nad_state_is_main_nad_fail())
		return sprintf(buf, "%s\n", "NG,MAIN_NAD");
	else if (kq_nad_state_is_nadx_fail())
		return kq_nad_add_result_nadx_fail(buf, PAGE_SIZE);
	else if (kq_nad_state_is_nadx_first_fail_second_pass())
		return kq_nad_add_result_nadx_pass(buf, PAGE_SIZE);
	else
		return sprintf(buf, "%s\n", "NONE");
}

static ssize_t kq_nad_show_nad_x_run_attr(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	u32 inform4_data;

	inform4_data = readl(kq_sec_nad_env.inform4);

	if (inform4_data == KQ_NAD_MAGIC_NADX_DONE) {
		writel(0x0, kq_sec_nad_env.inform4);
		return sprintf(buf, "%s\n", "RUN");
	} else
		return sprintf(buf, "%s\n", "NORUN");
}
#endif

static ssize_t kq_nad_show_nadc_result_attr(struct device *dev,
	struct device_attribute *attr, char *buf)
{

	if (kq_nad_state_is_nadc_executed()) {
		if (kq_nad_state_is_nadc_fail())
			return kq_nad_add_result_nadc_fail(buf, PAGE_SIZE);
		else {
			if (kq_sec_nad_env.shutdown == KQ_NAD_SHUTDOWN_ENABLE)
				return sprintf(buf, "OK_5.0_L_SHUTDOWN_CASE(NULL)DETAIL(NULL)\n");
			else
				return sprintf(buf, "OK_5.0_L_CASE(NULL)DETAIL(NULL)\n");
		}
	} else
		return sprintf(buf, "%s\n", "NONE");

	return 0;
}

static ssize_t kq_nad_show_nad_c_run_attr(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	if (kq_nad_state_is_nadc_executed())
		return sprintf(buf, "RUN\n");
	else
		return sprintf(buf, "NORUN\n");
}

static ssize_t kq_nad_show_reboot_attr(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	return 0;
}

static ssize_t kq_nad_show_attrs(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	const ptrdiff_t offset = attr - kq_nad_attrs;
	int i = 0;

	switch (offset) {
	case KQ_NAD_SYSFS_STAT:
		i = kq_nad_show_stat_attr(dev, attr, buf);
		break;
	case KQ_NAD_SYSFS_ERASE:
		i = kq_nad_show_erase_attr(dev, attr, buf);
		break;
	case KQ_NAD_SYSFS_ACAT:
		i = kq_nad_show_acat_attr(dev, attr, buf);
		break;
	case KQ_NAD_SYSFS_DRAM:
		i = kq_nad_show_dram_attr(dev, attr, buf);
		break;
	case KQ_NAD_SYSFS_ALL:
		i = kq_nad_show_all_attr(dev, attr, buf);
		break;
	case KQ_NAD_SYSFS_SUPPORT:
		i = kq_nad_show_support_attr(dev, attr, buf);
		break;
#if IS_ENABLED(CONFIG_SEC_KQ_NAD_API)
	case KQ_NAD_SYSFS_API:
		i = kq_nad_show_api_attr(dev, attr, buf);
		break;
#endif
	case KQ_NAD_SYSFS_VERSION:
		i = kq_nad_show_version_attr(dev, attr, buf);
		break;
#if IS_ENABLED(CONFIG_SEC_KQ_NAD_X) || IS_ENABLED(CONFIG_SEC_KQ_NAD_V2)
	case KQ_NAD_SYSFS_FAC_RESULT:
		i = kq_nad_show_fac_result_attr(dev, attr, buf);
		break;
	case KQ_NAD_SYSFS_NADX_RUN:
		i = kq_nad_show_nad_x_run_attr(dev, attr, buf);
		break;
#endif
	case KQ_NAD_SYSFS_NADC_RESULT:
		i = kq_nad_show_nadc_result_attr(dev, attr, buf);
		break;
	case KQ_NAD_SYSFS_NADC_RUN:
		i = kq_nad_show_nad_c_run_attr(dev, attr, buf);
		break;
	case KQ_NAD_SYSFS_REBOOT:
		i = kq_nad_show_reboot_attr(dev, attr, buf);
		break;
	default:
		pr_err("%s show kq nad out of range\n", __func__);
		i = -EINVAL;
		break;
	}
	return i;
}

static ssize_t kq_nad_store_stat_attr(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t count)
{
	return count;
}

static ssize_t kq_nad_store_erase_attr(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t count)
{
	u32 inform4_data;

	if (!strncmp(buf, "erase", 5)) {
		inform4_data = KQ_NAD_MAGIC_ERASE;
#if IS_ENABLED(CONFIG_SEC_KQ_NAD_V2)
		inform4_data |= KQ_NAD_CMD_MAGIC << 16;
#endif
		writel(inform4_data, kq_sec_nad_env.inform4);
	}
	return count;
}

static ssize_t kq_nad_store_acat_attr(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t count)
{
	int ret = -1;
	int idx = 0;
	char temp[KQ_NAD_MAX_CMD_SIZE * KQ_NAD_MAX_CMD_NAME + KQ_NAD_MAX_CMD_SIZE + 1];
	char nad_cmd[KQ_NAD_MAX_CMD_SIZE][KQ_NAD_MAX_CMD_NAME];
	char *nad_ptr, *string;
	unsigned int len = 0;
	unsigned int loop_count;
	u32 inform4_data, inform5_data;

	pr_info("%s buf : %s count : %d\n", __func__, buf, (int)count);

	if ((int)count < KQ_NAD_MAX_CMD_NAME)
		return -EINVAL;

	/* Copy buf to nad temp */
	len = (unsigned int)min(count, sizeof(temp) - 1);
	strncpy(temp, buf, len);
	temp[len] = '\0';
	string = temp;

	while (idx < KQ_NAD_MAX_CMD_SIZE) {
		nad_ptr = strsep(&string, ",");
		if ((nad_ptr ==  NULL) ||
			(strlen(nad_ptr) >= KQ_NAD_MAX_CMD_NAME)) {
			pr_info(" %s: invalid input\n", __func__);
			return -EINVAL;
		}
		strcpy(nad_cmd[idx++], nad_ptr);
	}

/*
 * ACAT NAD Write Data format
 *
 * nad_acat,20,0 : ACAT Loop test (20 cycles)
 * nad_acat,20,1 : ACAT Loop & DRAM test (20 cycles & 1 DRAM test)
 * nad_acat,0,1  : ACAT DRAM test (1 DRAM test)
 *
 */
	if (!strncmp(buf, "nad_acat", 8)) {
		/* Get NAD loop count */
		ret = sscanf(nad_cmd[1], "%d\n", &loop_count);
		if (ret != 1)
			return -EINVAL;
		/* case 1 : ACAT NAD */
		if (loop_count > 0) {
			pr_info("%s ACAT NAD test command.\n", __func__);
			inform5_data = loop_count;
			inform4_data = KQ_NAD_MAGIC_ACAT;
#if IS_ENABLED(CONFIG_SEC_KQ_NAD_V2)
			/* case 2 : DRAM test */
			if (!strncmp(nad_cmd[2], "1", 1)) {
				pr_info("%s DRAM test command.\n", __func__);
				inform4_data = KQ_NAD_MAGIC_ACAT_DRAM_TEST;
			}

			inform4_data |= KQ_NAD_CMD_MAGIC << 16;
#else
			/* case 2 : DRAM test */
			if (!strncmp(nad_cmd[2], "1", 1)) {
				pr_info("%s DRAM test command.\n", __func__);
				inform4_data |= KQ_NAD_MAGIC_DRAM_TEST;
			}
#endif
			writel(inform4_data, kq_sec_nad_env.inform4);
			writel(inform5_data, kq_sec_nad_env.inform5);
		}
#if IS_ENABLED(CONFIG_SEC_KQ_NAD_55) || IS_ENABLED(CONFIG_SEC_KQ_NAD_V2)
		else if (loop_count == 0)
			kq_nad_set_rework_to_inform();
#endif

		if (loop_count == KQ_NAD_LOOP_COUNT_NADX) {
			pr_info("%s NADX test command.\n", __func__);
			inform4_data = KQ_NAD_MAGIC_NADX_TEST;
#if IS_ENABLED(CONFIG_SEC_KQ_NAD_V2)
			inform4_data |= KQ_NAD_CMD_MAGIC << 16;
#endif
			writel(inform4_data, kq_sec_nad_env.inform4);
		}

#if IS_ENABLED(CONFIG_SEC_KQ_NAD_V2)
		if (loop_count == KQ_NAD_LOOP_CNT_MAINTENANCE) {
			pr_info("%s NAD Maintenance command.\n", __func__);
			inform4_data = KQ_NAD_MAGIC_MAINTENANCE;
			inform4_data |= KQ_NAD_CMD_MAGIC << 16;
			inform5_data = 5;

			writel(inform4_data, kq_sec_nad_env.inform4);
			writel(inform5_data, kq_sec_nad_env.inform5);
		}
#endif

	}

	return count;
}

static ssize_t kq_nad_store_dram_attr(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t count)
{
	return count;
}

static ssize_t kq_nad_store_all_attr(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t count)
{
	return count;
}

static ssize_t kq_nad_store_support_attr(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t count)
{
	return count;
}

static ssize_t kq_nad_store_version_attr(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t count)
{
	return count;
}

#if IS_ENABLED(CONFIG_SEC_KQ_NAD_X) || IS_ENABLED(CONFIG_SEC_KQ_NAD_V2)
static ssize_t kq_nad_store_fac_result_attr(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t count)
{
	u32 inform4_data;

	pr_info("%s buf : %s count = %d\n", __func__, buf, (int)count);

	/* check cmd */
	if (!strncmp(buf, "nads", 4)) {
		pr_info("%s run NADX SHORT command.\n", __func__);
		inform4_data = KQ_NAD_MAGIC_NADX_SHORT;
		inform4_data |= KQ_NAD_CMD_MAGIC << 16;

		writel(inform4_data, kq_sec_nad_env.inform4);
	} else if (!strncmp(buf, "nado", 4)) {
		pr_info("%s run NADX SHORT POWER OFF command.\n", __func__);
		inform4_data = KQ_NAD_MAGIC_NADO;
		inform4_data |= KQ_NAD_CMD_MAGIC << 16;

		writel(inform4_data, kq_sec_nad_env.inform4);
	} else if (!strncmp(buf, "nadx", 4)) {
		pr_info("%s run NADX command.\n", __func__);
		inform4_data = KQ_NAD_MAGIC_NADX_TEST;
#if IS_ENABLED(CONFIG_SEC_KQ_NAD_V2)
		inform4_data |= KQ_NAD_CMD_MAGIC << 16;
#endif
		writel(inform4_data, kq_sec_nad_env.inform4);
	} else if (!strncmp(buf, "mainfail", 8)) {
		pr_info("%s update failed main nad result\n", __func__);
		kq_sec_nad_env.nadx.result = KQ_NAD_RESULT_MAIN;
	}

	return count;
}

static ssize_t kq_nad_store_nad_x_run_attr(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t count)
{
	return count;
}
#endif

static ssize_t kq_nad_store_nadc_result_attr(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t count)
{
	int ret = -1;
	int idx = 0;
	char temp[KQ_NAD_MAX_CMD_SIZE * KQ_NAD_MAX_CMD_NAME + KQ_NAD_MAX_CMD_SIZE + 1];
	char nad_cmd[KQ_NAD_MAX_CMD_SIZE][KQ_NAD_MAX_CMD_NAME];
	char *nad_ptr, *string;
	unsigned int len = 0;
	unsigned int loop_count;
	u32 inform4_data;

	pr_info("%s buf : %s count : %d\n", __func__, buf, (int)count);

	/* Copy buf to nad temp */
	len = (unsigned int)min(count, sizeof(temp) - 1);
	strncpy(temp, buf, len);
	temp[len] = '\0';
	string = temp;

	while (idx < KQ_NADC_MAX_CMD_SIZE) {
		nad_ptr = strsep(&string, ",");
		if ((nad_ptr ==  NULL) ||
			(strlen(nad_ptr) >= KQ_NAD_MAX_CMD_NAME)) {
			pr_info(" %s: invalid input\n", __func__);
			return -EINVAL;
		}
		strcpy(nad_cmd[idx++], nad_ptr);
	}

	if (!strncmp(buf, "nadc", 4)) {
		/* Get NAD loop count */
		ret = sscanf(nad_cmd[1], "%d\n", &loop_count);
		if (ret != 1)
			return -EINVAL;
		/* case 1 : Custom NAD LOOP*/
		if (loop_count > 0) {
			pr_info("%s NADC test command loop_count : %d\n", __func__, loop_count);
			inform4_data = loop_count;
			inform4_data |= KQ_NAD_MAGIC_NADC_TEST;
#if IS_ENABLED(CONFIG_SEC_KQ_NAD_V2)
			inform4_data |= KQ_NAD_CMD_MAGIC << 16;
#endif
			writel(inform4_data, kq_sec_nad_env.inform4);
		}
	}

	return count;
}

static ssize_t kq_nad_store_nad_c_run_attr(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t count)
{
	return count;
}

static ssize_t kq_nad_store_reboot_attr(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t count)
{
	u32 inform4_data;

	/* check commnad */
	pr_info("%s: START\n", __func__);

	if (!strncmp(buf, "COM", 3)) {
		inform4_data = KQ_NAD_MAGIC_REBOOT;
#if IS_ENABLED(CONFIG_SEC_KQ_NAD_V2)
		inform4_data |= KQ_NAD_CMD_MAGIC << 16;
#endif
		writel(inform4_data, kq_sec_nad_env.inform4);
	}

	return count;
}

static ssize_t kq_nad_store_attrs(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t count)
{
	const ptrdiff_t offset = attr - kq_nad_attrs;

	switch (offset) {
	case KQ_NAD_SYSFS_STAT:
		count = kq_nad_store_stat_attr(dev, attr, buf, count);
		break;
	case KQ_NAD_SYSFS_ERASE:
		count = kq_nad_store_erase_attr(dev, attr, buf, count);
		break;
	case KQ_NAD_SYSFS_ACAT:
		count = kq_nad_store_acat_attr(dev, attr, buf, count);
		break;
	case KQ_NAD_SYSFS_DRAM:
		count = kq_nad_store_dram_attr(dev, attr, buf, count);
		break;
	case KQ_NAD_SYSFS_ALL:
		count = kq_nad_store_all_attr(dev, attr, buf, count);
		break;
	case KQ_NAD_SYSFS_SUPPORT:
		count = kq_nad_store_support_attr(dev, attr, buf, count);
		break;
	case KQ_NAD_SYSFS_VERSION:
		count = kq_nad_store_version_attr(dev, attr, buf, count);
		break;
#if IS_ENABLED(CONFIG_SEC_KQ_NAD_X) || IS_ENABLED(CONFIG_SEC_KQ_NAD_V2)
	case KQ_NAD_SYSFS_FAC_RESULT:
		count = kq_nad_store_fac_result_attr(dev, attr, buf, count);
		break;
	case KQ_NAD_SYSFS_NADX_RUN:
		count = kq_nad_store_nad_x_run_attr(dev, attr, buf, count);
		break;
#endif
	case KQ_NAD_SYSFS_NADC_RESULT:
		count = kq_nad_store_nadc_result_attr(dev, attr, buf, count);
		break;
	case KQ_NAD_SYSFS_NADC_RUN:
		count = kq_nad_store_nad_c_run_attr(dev, attr, buf, count);
		break;
	case KQ_NAD_SYSFS_REBOOT:
		count = kq_nad_store_reboot_attr(dev, attr, buf, count);
		break;
	default:
		pr_err("%s store kq nad out of range\n", __func__);
		break;
	}

	return count;
}

static int __init kq_nad_create_attr(struct device *dev)
{
	int i;
	int ret;

	for (i = 0; i < ARRAY_SIZE(kq_nad_attrs); i++) {
		pr_info("%s create %dth file\n", __func__, i+1);
		ret = device_create_file(dev, &kq_nad_attrs[i]);
		if (ret)
			goto create_nad_attr_fail;
	}

	return 0;

create_nad_attr_fail:
	while (i--) {
		pr_err("%s remove %dth file\n", __func__, i);
		device_remove_file(dev, &kq_nad_attrs[i]);
	}

	return -EINVAL;
}

static bool __init kq_nad_if_sudden_poweroff_set_result_pass(
	struct kq_nad_mparam_inform *nminfo)
{
	if (nminfo->inform1 == 0) {
		nminfo->result = KQ_NAD_RESULT_PASS;
		nminfo->fail.das = KQ_NAD_DAS_NAME_NONE;
		nminfo->fail.block = KQ_NAD_BLOCK_NONE;
		return true;
	}
	return false;
}

#if IS_ENABLED(CONFIG_SEC_KQ_NAD_V2)
static bool __init kq_nad_if_nad_wdt_set_result_pass(
	struct kq_nad_mparam_inform *nminfo)
{
	if ((((nminfo->inform1 >> KQ_NAD_INFORM1_MAGIC) & 0xFF) == KQ_NAD_FIRST_START_MAGIC)
		|| (((nminfo->inform1 >> KQ_NAD_INFORM1_MAGIC) & 0xFF) == KQ_NAD_SECOND_START_MAGIC)) {
		nminfo->result = KQ_NAD_RESULT_PASS;
		nminfo->fail.das = KQ_NAD_DAS_NAME_NONE;
		nminfo->fail.block = KQ_NAD_BLOCK_NONE;
		return true;
	}
	return false;
}
#endif

static void __init kq_nad_set_update_first_fail_info(struct kq_nad_mparam_inform *nminfo)
{
	if (kq_nad_if_sudden_poweroff_set_result_pass(nminfo))
		return;
#if IS_ENABLED(CONFIG_SEC_KQ_NAD_V2)
	if (kq_nad_if_nad_wdt_set_result_pass(nminfo))
		return;
#endif
#if IS_ENABLED(CONFIG_SEC_KQ_NAD_V2)
	nminfo->fail.block = nminfo->inform1 & 0x1F;
	nminfo->fail.level = (nminfo->inform1 >> 8) & 0x1F;
	nminfo->fail.vector = (nminfo->inform1 >> 24) & 0x3F;
#else
#if IS_ENABLED(CONFIG_SOC_EXYNOS3830) || IS_ENABLED(CONFIG_SOC_S5E3830)
	nminfo->fail.block = (nminfo->inform1 >> 5) & 0x7;
#else
	nminfo->fail.block = (nminfo->inform1 >> 8) & 0xF;
#endif
	nminfo->fail.level = nminfo->inform1 & 0x1F;
	nminfo->fail.vector = (nminfo->inform2 >> 24) & 0x3F;
#endif

	if (nminfo->fail.block == KQ_NAD_BLOCK_CP)
		nminfo->fail.das = KQ_NAD_DAS_NAME_CP;
	else
		nminfo->fail.das = (nminfo->inform1 >> 14) & 0x3;
}

static void __init kq_nad_set_update_second_fail_info(struct kq_nad_mparam_inform *nminfo)
{
	if (kq_nad_if_sudden_poweroff_set_result_pass(nminfo))
		return;
#if IS_ENABLED(CONFIG_SEC_KQ_NAD_V2)
	if (kq_nad_if_nad_wdt_set_result_pass(nminfo))
		return;
#endif
#if IS_ENABLED(CONFIG_SEC_KQ_NAD_V2)
	nminfo->fail.block = nminfo->inform3 & 0x1F;
	nminfo->fail.level = (nminfo->inform3 >> 8) & 0x1F;
	nminfo->fail.vector = (nminfo->inform3 >> 24) & 0x3F;
#else
#if IS_ENABLED(CONFIG_SOC_EXYNOS3830) || IS_ENABLED(CONFIG_SOC_S5E3830)
	nminfo->fail.block = (nminfo->inform1 >> 5) & 0x7;
#else
	nminfo->fail.block = (nminfo->inform1 >> 8) & 0xF;
#endif
	nminfo->fail.level = nminfo->inform1 & 0x1F;
	nminfo->fail.vector = (nminfo->inform2 >> 24) & 0x3F; 
#endif

	if (nminfo->fail.block == KQ_NAD_BLOCK_CP)
		nminfo->fail.das = KQ_NAD_DAS_NAME_CP;
	else
		nminfo->fail.das = (nminfo->inform1 >> 14) & 0x3;
}

static void __init kq_nad_set_data_result(struct kq_nad_mparam_inform *nminfo)
{
	if (nminfo->inform1 & KQ_NAD_BIT_PASS)
		nminfo->result = KQ_NAD_RESULT_PASS;
	else {
		nminfo->result = KQ_NAD_RESULT_FAIL;
		kq_nad_set_update_first_fail_info(nminfo);
	}
}

static void __init kq_nad_set_2nd_data_result(struct kq_nad_mparam_inform *nminfo)
{
#if IS_ENABLED(CONFIG_SOC_EXYNOS3830) || IS_ENABLED(CONFIG_SOC_S5E3830)
	if (nminfo->inform1 & KQ_NAD_BIT_PASS)
#else
	if (nminfo->inform3 & KQ_NAD_BIT_PASS)
#endif
		nminfo->result = KQ_NAD_RESULT_PASS;
	else {
		nminfo->result = KQ_NAD_RESULT_FAIL;
		kq_nad_set_update_second_fail_info(nminfo);
	}
}

static void __init kq_nad_set_data(int idx)
{
	switch (idx) {
	case KQ_NAD_PHASE_SMD_FIRST:
		pr_info("%s smd nad data exist\n", __func__);
		kq_sec_nad_env.smd.inform1 = nad_inform1;
		kq_sec_nad_env.smd.inform2 = nad_inform2;
		kq_sec_nad_env.smd.inform3 = nad_inform3;
		kq_sec_nad_env.smd.inform4 = nad_inform4;
		kq_sec_nad_env.smd.inform5 = nad_inform5;
		kq_nad_set_data_result(&kq_sec_nad_env.smd);
		break;
	case KQ_NAD_PHASE_SMD_SECOND:
		pr_info("%s smd nad second data exist\n", __func__);
		kq_sec_nad_env.smd_second.inform1 = nad_second_inform1;
		kq_sec_nad_env.smd_second.inform2 = nad_second_inform2;
		kq_sec_nad_env.smd_second.inform3 = nad_second_inform3;
		kq_sec_nad_env.smd_second.inform4 = nad_second_inform4;
		kq_sec_nad_env.smd_second.inform5 = nad_second_inform5;
		kq_nad_set_2nd_data_result(&kq_sec_nad_env.smd_second);
		break;
	case KQ_NAD_PHASE_ACAT_FIRST:
		pr_info("%s acat data exist\n", __func__);
		kq_sec_nad_env.acat.inform1 = acat_inform1;
		kq_sec_nad_env.acat.inform2 = acat_inform2;
		kq_sec_nad_env.acat.inform3 = acat_inform3;
		kq_sec_nad_env.acat.inform4 = acat_inform4;
		kq_sec_nad_env.acat.inform5 = acat_inform5;
		kq_nad_set_data_result(&kq_sec_nad_env.acat);
		break;
	case KQ_NAD_PHASE_ACAT_SECOND:
		pr_info("%s acat second data exist\n", __func__);
		kq_sec_nad_env.acat_second.inform1 = acat_second_inform1;
		kq_sec_nad_env.acat_second.inform2 = acat_second_inform2;
		kq_sec_nad_env.acat_second.inform3 = acat_second_inform3;
		kq_sec_nad_env.acat_second.inform4 = acat_second_inform4;
		kq_sec_nad_env.acat_second.inform5 = acat_second_inform5;
		kq_nad_set_2nd_data_result(&kq_sec_nad_env.acat_second);
		break;
#if IS_ENABLED(CONFIG_SEC_KQ_NAD_V2)
	case KQ_NAD_PHASE_EXTEND_FIRST:
		pr_info("%s nadx data exist\n", __func__);
		kq_sec_nad_env.nadx.inform1 = nadx_inform1;
		kq_sec_nad_env.nadx.inform2 = nadx_inform2;
		kq_sec_nad_env.nadx.inform3 = nadx_inform3;
		kq_sec_nad_env.nadx.inform4 = nadx_inform4;
		kq_sec_nad_env.nadx.inform5 = nadx_inform5;
		kq_nad_set_data_result(&kq_sec_nad_env.nadx);
		break;
	case KQ_NAD_PHASE_EXTEND_SECOND:
		pr_info("%s nadx second data exist\n", __func__);
		kq_sec_nad_env.nadx_second.inform1 = nadx_second_inform1;
		kq_sec_nad_env.nadx_second.inform2 = nadx_second_inform2;
		kq_sec_nad_env.nadx_second.inform3 = nadx_second_inform3;
		kq_sec_nad_env.nadx_second.inform4 = nadx_second_inform4;
		kq_sec_nad_env.nadx_second.inform5 = nadx_second_inform5;
		kq_nad_set_data_result(&kq_sec_nad_env.nadx_second);
		break;
	case KQ_NAD_PHASE_NADX_SHORT_FIRST:
		pr_info("%s nadx short data exist\n", __func__);
		kq_sec_nad_env.nadx.inform1 = nadx_inform1;
		kq_sec_nad_env.nadx.inform2 = nadx_inform2;
		kq_sec_nad_env.nadx.inform3 = nadx_inform3;
		kq_sec_nad_env.nadx.inform4 = nadx_inform4;
		kq_sec_nad_env.nadx.inform5 = nadx_inform5;
		kq_nad_set_data_result(&kq_sec_nad_env.nadx);
		break;
	case KQ_NAD_PHASE_NADX_SHORT_SECOND:
		pr_info("%s nadx second data exist\n", __func__);
		kq_sec_nad_env.nadx_second.inform1 = nadx_second_inform1;
		kq_sec_nad_env.nadx_second.inform2 = nadx_second_inform2;
		kq_sec_nad_env.nadx_second.inform3 = nadx_second_inform3;
		kq_sec_nad_env.nadx_second.inform4 = nadx_second_inform4;
		kq_sec_nad_env.nadx_second.inform5 = nadx_second_inform5;
		kq_nad_set_data_result(&kq_sec_nad_env.nadx_second);
		break;
#elif IS_ENABLED(CONFIG_SEC_KQ_NAD_X)
	case KQ_NAD_PHASE_EXTEND_FIRST:
		pr_info("%s nadx data exist\n", __func__);
		kq_sec_nad_env.nadx.inform1 = nadx_inform1;
		kq_sec_nad_env.nadx.inform2 = nadx_inform2;
		kq_sec_nad_env.nadx.inform3 = nadx_inform3;
		kq_sec_nad_env.nadx.inform4 = nadx_inform4;
		kq_sec_nad_env.nadx.inform5 = nadx_inform5;
		kq_nad_set_data_result(&kq_sec_nad_env.nadx);
		break;
	case KQ_NAD_PHASE_EXTEND_SECOND:
		pr_info("%s nadx second data exist\n", __func__);
		kq_sec_nad_env.nadx_second.inform1 = nadx_second_inform1;
		kq_sec_nad_env.nadx_second.inform2 = nadx_second_inform2;
		kq_sec_nad_env.nadx_second.inform3 = nadx_second_inform3;
		kq_sec_nad_env.nadx_second.inform4 = nadx_second_inform4;
		kq_sec_nad_env.nadx_second.inform5 = nadx_second_inform5;
		kq_nad_set_data_result(&kq_sec_nad_env.nadx_second);
		break;
	case KQ_NAD_PHASE_CUSTOM_FIRST:
		pr_err("%s nadc data exist\n", __func__);
		kq_sec_nad_env.nadc.inform1 = nadc_inform1;
		kq_sec_nad_env.nadc.inform2 = nadc_inform2;
		kq_sec_nad_env.nadc.inform3 = nadc_inform3;
		kq_nad_set_data_result(&kq_sec_nad_env.nadc);
		break;
	case KQ_NAD_PHASE_CUSTOM_SECOND:
		pr_err("%s nadc second data exist\n", __func__);
		kq_sec_nad_env.nadc_second.inform1 = nadc_second_inform1;
		kq_sec_nad_env.nadc_second.inform2 = nadc_second_inform2;
		kq_sec_nad_env.nadc_second.inform3 = nadc_second_inform3;
		kq_nad_set_data_result(&kq_sec_nad_env.nadc_second);
		break;
#endif
	}
}

#if IS_ENABLED(CONFIG_SEC_KQ_NAD_VDD_CAL)
static void __init kq_nad_set_vddcal_data(void)
{
	pr_info("%s vddcal data exist\n", __func__);
	kq_sec_nad_env.vddcal.lit = vddcal_data0;
	kq_sec_nad_env.vddcal.mid = vddcal_data1;
	kq_sec_nad_env.vddcal.big = vddcal_data2;
}
#endif

#if IS_ENABLED(CONFIG_SEC_KQ_CORRELATION_RESULT)
static bool __init kq_nad_correlation_mparam_is_empty(char *token)
{
	if (strlen(token) == 0)
		return true;
	return false;
}

static bool __init kq_nad_correlation_mparam_is_valid_format(void)
{
	if (kq_nad_correlation_mparam_is_empty(correlation_info_mparam_cl0)) {
		pr_info("%s cl0 correlation empty\n", __func__);
		return false;
	}
	pr_info("%s cl0 : %s\n", __func__, correlation_info_mparam_cl0);

	if (kq_nad_correlation_mparam_is_empty(correlation_info_mparam_cl1)) {
		pr_info("%s cl1 correlation empty\n", __func__);
		return false;
	}
	pr_info("%s cl1 : %s\n", __func__, correlation_info_mparam_cl1);

	if (kq_nad_correlation_mparam_is_empty(correlation_info_mparam_cl2)) {
		pr_info("%s cl2 correlation empty\n", __func__);
		return false;
	}
	pr_info("%s cl2 : %s\n", __func__, correlation_info_mparam_cl2);

	if (kq_nad_correlation_mparam_is_empty(correlation_info_mparam_mif)) {
		pr_info("%s cl0 correlation empty\n", __func__);
		return false;
	}
	pr_info("%s mif : %s\n", __func__, correlation_info_mparam_mif);

	if (kq_nad_correlation_mparam_is_empty(correlation_info_mparam_dsu)) {
		pr_info("%s dsu correlation empty\n", __func__);
		return false;
	}
	pr_info("%s dsu : %s\n", __func__, correlation_info_mparam_dsu);

	if (kq_nad_correlation_mparam_is_empty(correlation_info_mparam_cp)) {
		pr_info("%s cp correlation empty\n", __func__);
		return false;
	}
	pr_info("%s cp : %s\n", __func__, correlation_info_mparam_cp);

	if (kq_nad_correlation_mparam_is_empty(correlation_info_mparam_g3d)) {
		pr_info("%s g3d correlation empty\n", __func__);
		return false;
	}
	pr_info("%s g3d : %s\n", __func__, correlation_info_mparam_g3d);

	if (kq_nad_correlation_mparam_is_empty(correlation_info_mparam_sci)) {
		pr_info("%s sci correlation empty\n", __func__);
		return false;
	}
	pr_info("%s sci : %s\n", __func__, correlation_info_mparam_sci);

	return true;
}

static void __init kq_nad_correlation_mparam_to_driver_var(void)
{
	pr_info("%s copy data\n", __func__);

	sprintf(kq_sec_nad_env.correlation[KQ_NAD_MPARAM_CORRELATION_LOGIC_BLOCK_CL0].info,
		"%s", correlation_info_mparam_cl0);
	sprintf(kq_sec_nad_env.correlation[KQ_NAD_MPARAM_CORRELATION_LOGIC_BLOCK_CL1].info,
		"%s", correlation_info_mparam_cl1);
	sprintf(kq_sec_nad_env.correlation[KQ_NAD_MPARAM_CORRELATION_LOGIC_BLOCK_CL2].info,
		"%s", correlation_info_mparam_cl2);
	sprintf(kq_sec_nad_env.correlation[KQ_NAD_MPARAM_CORRELATION_LOGIC_BLOCK_MIF].info,
		"%s", correlation_info_mparam_mif);
	sprintf(kq_sec_nad_env.correlation[KQ_NAD_MPARAM_CORRELATION_LOGIC_BLOCK_DSU].info,
		"%s", correlation_info_mparam_dsu);
	sprintf(kq_sec_nad_env.correlation[KQ_NAD_MPARAM_CORRELATION_LOGIC_BLOCK_CP].info,
		"%s", correlation_info_mparam_cp);
	sprintf(kq_sec_nad_env.correlation[KQ_NAD_MPARAM_CORRELATION_LOGIC_BLOCK_G3D].info,
		"%s", correlation_info_mparam_g3d);
	sprintf(kq_sec_nad_env.correlation[KQ_NAD_MPARAM_CORRELATION_LOGIC_BLOCK_SCI].info,
		"%s", correlation_info_mparam_sci);
}

static void __init kq_nad_set_correlation_data(void)
{
	if (kq_nad_correlation_mparam_is_valid_format()) {
		kq_nad_correlation_mparam_to_driver_var();
		kq_sec_nad_env.correlation_magic = KQ_NAD_CORRELATION_VALID_MAGIC;
	} else {
		pr_err("%s : correlation mparam invalid or not exist!\n", __func__);
	}
}
#endif

static void __init kq_nad_data_from_cmdline(void)
{
	int idx = 0;

	kq_sec_nad_env.status = nad_status;
	kq_sec_nad_env.shutdown = nad_shutdown;
	kq_sec_nad_env.inform4 = ioremap(KQ_NAD_INFORM4_MAGIC, 0x10);
	kq_sec_nad_env.inform5 = ioremap(KQ_NAD_INFORM5_MAGIC, 0x10);

	for (idx = 0; idx < KQ_NAD_PHASE_END; idx++) {
		if ((kq_sec_nad_env.status >> idx) & 0x1)
			kq_nad_set_data(idx);
	}

#if IS_ENABLED(CONFIG_SEC_KQ_NAD_VDD_CAL)
	kq_nad_set_vddcal_data();
#endif

#if IS_ENABLED(CONFIG_SEC_KQ_CORRELATION_RESULT)
	kq_nad_set_correlation_data();
#endif

#if IS_ENABLED(CONFIG_SEC_KQ_NAD_55) || IS_ENABLED(CONFIG_SEC_KQ_NAD_V2)
	kq_nad_check_rework();
#endif
#if IS_ENABLED(CONFIG_SEC_KQ_NAD_V2)
	kq_sec_nad_env.skip = nad_skip;
	kq_sec_nad_env.ecc = nad_ecc;
	kq_sec_nad_env.powermeter = nad_powermeter;
#endif

}

static int __init kq_nad_create_sysfs(void)
{
	struct device *sdev;
	int ret;

	sdev = sec_device_create(NULL, "sec_nad");
	if (IS_ERR(sdev)) {
		pr_err("%s failed to create /sys/class/sec/sec_nad\n",
			__func__);
		return PTR_ERR(sdev);
	}

	ret = kq_nad_create_attr(sdev);
	if (ret) {
		pr_err("%s failed to create sec_nad attr\n", __func__);
		goto error_create_nad_sysfs;
	}
	return 0;

error_create_nad_sysfs:
	sec_device_destroy(sdev->devt);
	return -EINVAL;
}

static int __init kq_nad_init(void)
{
	int ret = 0;

	ret = kq_nad_create_sysfs();
	if (ret) {
		pr_err("%s failed to create sysfs\n", __func__);
		return -EINVAL;
	}

	kq_nad_data_from_cmdline();

	pr_info("%s nad_shutdown : %d\n", __func__, nad_shutdown);
	pr_info("%s init nad_status : 0x%x\n", __func__, nad_status);

	return ret;
}

static void __exit kq_nad_exit(void)
{
	pr_info("%s do nothing\n", __func__);

	iounmap(kq_sec_nad_env.inform4);
	iounmap(kq_sec_nad_env.inform5);
}

module_init(kq_nad_init);
module_exit(kq_nad_exit);

MODULE_DESCRIPTION("kernel quality nad driver");
MODULE_AUTHOR("seung0.lee");
MODULE_LICENSE("GPL");
