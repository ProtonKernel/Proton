/*
 * drivers/samsung/sec_reboot.c
 *
 * Copyright (c) 2020 Samsung Electronics Co., Ltd.
 *		http://www.samsung.com
 *
 * Implementation of Exynos specific power domain control which is used in
 * conjunction with runtime-pm.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#include <linux/of.h>
#include <linux/module.h>
#include <linux/regmap.h>
#include <linux/mfd/syscon.h>
#include <linux/platform_device.h>
#include <linux/reboot.h>
#include <linux/reset/exynos-reset.h>
#include <soc/samsung/debug-snapshot.h>
#include <soc/samsung/exynos-pmu-if.h>
#include <linux/battery/sec_battery_common.h>
#include <linux/notifier.h>
#include <linux/string.h>
#if IS_ENABLED(CONFIG_SEC_ABC)
#include <linux/sti/abc_common.h>
#endif
#if IS_ENABLED(CONFIG_SEC_PARAM)
extern unsigned int lpcharge;
#endif
extern void hard_reset_delay(void);

/* MULTICMD
 * reserve 9bit | clk_change 1bit | dumpsink 2bit | param 1bit | dram_test 1bit | cp_debugmem 2bit | debuglevel 2bit | forceupload 2bit
 */
#define FORCEUPLOAD_ON                          (0x5)
#define FORCEUPLOAD_OFF                         (0x0)
#define DEBUGLEVEL_LOW                          (0x4f4c)
#define DEBUGLEVEL_MID                          (0x494d)
#define DEBUGLEVEL_HIGH                         (0x4948)
#define DUMPSINK_USB                            (0x0)
#define DUMPSINK_BOOTDEV                        (0x42544456)
#define DUMPSINK_SDCARD                         (0x73646364)
#define MULTICMD_CNT_MAX                        10
#define MULTICMD_LEN_MAX                        50
#define MULTICMD_FORCEUPLOAD_SHIFT              0
#define MULTICMD_FORCEUPLOAD_ON                 (0x1)
#define MULTICMD_FORCEUPLOAD_OFF                (0x2)
#define MULTICMD_DEBUGLEVEL_SHIFT               (MULTICMD_FORCEUPLOAD_SHIFT + 2)
#define MULTICMD_DEBUGLEVEL_LOW                 (0x1)
#define MULTICMD_DEBUGLEVEL_MID                 (0x2)
#define MULTICMD_DEBUGLEVEL_HIGH                (0x3)
#define MULTICMD_CPMEM_SHIFT                    (MULTICMD_DEBUGLEVEL_SHIFT + 2)
#define MULTICMD_CPMEM_ON                       (0x1)
#define MULTICMD_CPMEM_OFF                      (0x2)
#define MULTICMD_DRAMTEST_SHIFT                 (MULTICMD_CPMEM_SHIFT + 2)
#define MULTICMD_DRAMTEST_ON                    (0x1)
#define MULTICMD_PARAM_SHIFT                    (MULTICMD_DRAMTEST_SHIFT + 1)
#define MULTICMD_PARAM_ON                       (0x1)
#define MULTICMD_DUMPSINK_SHIFT                 (MULTICMD_PARAM_SHIFT + 1)
#define MULTICMD_DUMPSINK_USB                   (0x1)
#define MULTICMD_DUMPSINK_BOOT                  (0x2)
#define MULTICMD_DUMPSINK_SD                    (0x3)
#define MULTICMD_CLKCHANGE_SHIFT                (MULTICMD_DUMPSINK_SHIFT + 2)
#define MULTICMD_CLKCHANGE_ON                   (0x1)

extern void cache_flush_all(void);
extern void exynos_mach_restart(const char *cmd);
extern struct atomic_notifier_head panic_notifier_list;
extern struct exynos_reboot_helper_ops exynos_reboot_ops;
extern int exynos_reboot_pwrkey_status(void);

#define PS_HOLD_EN                              (1 << 8)

/* MINFORM */
#define SEC_REBOOT_START_OFFSET		(24)
#define SEC_REBOOT_END_OFFSET		(16)

enum sec_power_flags {
	SEC_REBOOT_DEFAULT = 0x30,
	SEC_REBOOT_NORMAL = 0x4E,
	SEC_REBOOT_LPM = 0x70,
};

#define SEC_DUMPSINK_MASK 0x0000FFFF

/* PANIC INFORM */
#define SEC_RESET_REASON_PREFIX         0x12345600
#define SEC_RESET_SET_PREFIX            0xabc00000
#define SEC_RESET_MULTICMD_PREFIX       0xa5600000
enum sec_reset_reason {
	SEC_RESET_REASON_UNKNOWN   = (SEC_RESET_REASON_PREFIX | 0x00),
	SEC_RESET_REASON_DOWNLOAD  = (SEC_RESET_REASON_PREFIX | 0x01),
	SEC_RESET_REASON_UPLOAD    = (SEC_RESET_REASON_PREFIX | 0x02),
	SEC_RESET_REASON_CHARGING  = (SEC_RESET_REASON_PREFIX | 0x03),
	SEC_RESET_REASON_RECOVERY  = (SEC_RESET_REASON_PREFIX | 0x04),
	SEC_RESET_REASON_FOTA      = (SEC_RESET_REASON_PREFIX | 0x05),
	SEC_RESET_REASON_FOTA_BL   = (SEC_RESET_REASON_PREFIX | 0x06), /* update bootloader */
	SEC_RESET_REASON_SECURE    = (SEC_RESET_REASON_PREFIX | 0x07), /* image secure check fail */
	SEC_RESET_REASON_FWUP      = (SEC_RESET_REASON_PREFIX | 0x09), /* emergency firmware update */
	SEC_RESET_REASON_EM_FUSE   = (SEC_RESET_REASON_PREFIX | 0x0a), /* EMC market fuse */
	SEC_RESET_REASON_BOOTLOADER   = (SEC_RESET_REASON_PREFIX | 0x0d), /* go to download mode */
	SEC_RESET_REASON_EMERGENCY = 0x0,

	SEC_RESET_SET_DPRM         = (SEC_RESET_SET_PREFIX | 0x20000),
	SEC_RESET_SET_FORCE_UPLOAD = (SEC_RESET_SET_PREFIX | 0x40000),
	SEC_RESET_SET_DEBUG        = (SEC_RESET_SET_PREFIX | 0xd0000),
	SEC_RESET_SET_SWSEL        = (SEC_RESET_SET_PREFIX | 0xe0000),
	SEC_RESET_SET_SUD          = (SEC_RESET_SET_PREFIX | 0xf0000),
	SEC_RESET_CP_DBGMEM        = (SEC_RESET_SET_PREFIX | 0x50000), /* cpmem_on: CP RAM logging */
#if IS_ENABLED(CONFIG_SEC_ABC)
	SEC_RESET_USER_DRAM_TEST   = (SEC_RESET_SET_PREFIX | 0x60000), /* USER DRAM TEST */
#endif
#if defined(CONFIG_SEC_SYSUP)
	SEC_RESET_SET_PARAM   = (SEC_RESET_SET_PREFIX | 0x70000),
#endif
	SEC_RESET_SET_DUMPSINK	   = (SEC_RESET_SET_PREFIX | 0x80000),
	SEC_RESET_SET_MULTICMD     = SEC_RESET_MULTICMD_PREFIX,
};

static struct regmap *pmureg;
static u32 magic_inform, panic_inform;
static u32 shutdown_offset, shutdown_trigger;

static int sec_reboot_on_panic;
static char panic_str[10] = "panic";

ATOMIC_NOTIFIER_HEAD(sec_power_off_notifier_list);
EXPORT_SYMBOL(sec_power_off_notifier_list);

static char * sec_strtok(char *s1, const char *delimit)
{
	static char *lastToken = NULL;
	char *tmp;

	if (s1 == NULL) {
		s1 = lastToken;

		if (s1 == NULL)
			return NULL;
	} else {
		s1 += strspn(s1, delimit);
	}

	tmp = strpbrk(s1, delimit);
	if (tmp) {
		*tmp = '\0';
		lastToken = tmp + 1;
	} else {
		lastToken = NULL;
	}

	return s1;
}

static void sec_multicmd(const char *cmd)
{
	unsigned long value = 0;
	char *multicmd_ptr;
	char *multicmd_cmd[MULTICMD_CNT_MAX];
	char copy_cmd[100] = {0,};
	unsigned long multicmd_value = 0;
	int i, cnt = 0;

	strcpy(copy_cmd, cmd);
	multicmd_ptr = sec_strtok(copy_cmd, ":");
	while (multicmd_ptr != NULL) {
		if (cnt >= MULTICMD_CNT_MAX)
			break;

		multicmd_cmd[cnt++] = multicmd_ptr;
		multicmd_ptr = sec_strtok(NULL, ":");
	}

	for (i = 1; i < cnt; i++) {
		if (strlen(multicmd_cmd[i]) < MULTICMD_LEN_MAX) {
			if (!strncmp(multicmd_cmd[i], "forceupload", 11) && !kstrtoul(multicmd_cmd[i] + 11, 0, &value)) {
				if (value == FORCEUPLOAD_ON)
					multicmd_value |= (MULTICMD_FORCEUPLOAD_ON << MULTICMD_FORCEUPLOAD_SHIFT);
				else if (value == FORCEUPLOAD_OFF)
					multicmd_value |= (MULTICMD_FORCEUPLOAD_OFF << MULTICMD_FORCEUPLOAD_SHIFT);
			}
			else if (!strncmp(multicmd_cmd[i], "debug", 5) && !kstrtoul(multicmd_cmd[i] + 5, 0, &value)) {
				if (value == DEBUGLEVEL_HIGH)
					multicmd_value |= (MULTICMD_DEBUGLEVEL_HIGH << MULTICMD_DEBUGLEVEL_SHIFT);
				else if (value == DEBUGLEVEL_MID)
					multicmd_value |= (MULTICMD_DEBUGLEVEL_MID << MULTICMD_DEBUGLEVEL_SHIFT);
				else if (value == DEBUGLEVEL_LOW)
					multicmd_value |= (MULTICMD_DEBUGLEVEL_LOW << MULTICMD_DEBUGLEVEL_SHIFT);
			}
			else if (!strncmp(multicmd_cmd[i], "cpmem_on", 8))
				multicmd_value |= (MULTICMD_CPMEM_ON << MULTICMD_CPMEM_SHIFT);
			else if (!strncmp(multicmd_cmd[i], "cpmem_off", 9))
				multicmd_value |= (MULTICMD_CPMEM_OFF << MULTICMD_CPMEM_SHIFT);
#if defined(CONFIG_SEC_ABC)
			else if (!strncmp(multicmd_cmd[i], "user_dram_test", 14) && sec_abc_get_enabled())
				multicmd_value |= (MULTICMD_DRAMTEST_ON << MULTICMD_DRAMTEST_SHIFT);
#endif
#if defined(CONFIG_SEC_SYSUP)
			else if (!strncmp(multicmd_cmd[i], "param", 5))
				multicmd_value |= (MULTICMD_PARAM_ON << MULTICMD_PARAM_SHIFT);
#endif
			else if (!strncmp(multicmd_cmd[i], "dump_sink", 9) && !kstrtoul(multicmd_cmd[i] + 9, 0, &value)) {
				if (value == DUMPSINK_USB)
					multicmd_value |= (MULTICMD_DUMPSINK_USB << MULTICMD_DUMPSINK_SHIFT);
				else if (value == DUMPSINK_BOOTDEV)
					multicmd_value |= (MULTICMD_DUMPSINK_BOOT << MULTICMD_DUMPSINK_SHIFT);
				else if (value == DUMPSINK_SDCARD)
					multicmd_value |= (MULTICMD_DUMPSINK_SD << MULTICMD_DUMPSINK_SHIFT);
			}
#if defined(CONFIG_ARM_EXYNOS_ACME_DISABLE_BOOT_LOCK) && defined(CONFIG_ARM_EXYNOS_DEVFREQ_DISABLE_BOOT_LOCK)
			else if (!strncmp(multicmd_cmd[i], "clkchange_test", 14))
				multicmd_value |= (MULTICMD_CLKCHANGE_ON << MULTICMD_CLKCHANGE_SHIFT);
#endif
		}
	}
	pr_emerg("%s: multicmd_value: %lu\n", __func__, multicmd_value);
	regmap_write(pmureg, panic_inform, SEC_RESET_SET_MULTICMD | multicmd_value);
}

void sec_set_reboot_magic(int magic, int offset, int mask)
{
	u32 tmp = 0;

	regmap_read(pmureg, magic_inform, &tmp);
	pr_info("%s: prev: %x\n", __func__, tmp);
	mask <<= offset;
	tmp &= (~mask);
	tmp |= magic << offset;
	pr_info("%s: set as: %x\n", __func__, tmp);
	regmap_write(pmureg, magic_inform, tmp); 
}
EXPORT_SYMBOL(sec_set_reboot_magic);

/* exynos_power_off drivers/power/reset/exynos-reboot.c */
static void sec_power_off(void)
{
	u32 poweroff_try = 0;
	union power_supply_propval ac_val = {0, }, usb_val = {0, }, wpc_val = {0, }, water_val = {0, };
	u32 reboot_charging = 0;
	u32 npu_retry = 0;
	u32 val = 0;

	/* PMIC EVT0 : power-off issue */
	/* PWREN_MIF 0 after setting regulator controlled by PWREN_MIF to always-on */
	if (exynos_reboot_ops.pmic_off_sub_wa) {
		if (exynos_reboot_ops.pmic_off_sub_wa() < 0)
			pr_err("pmic_off_sub_wa error\n");
	}
	if (exynos_reboot_ops.pmic_off_main_wa) {
		if (exynos_reboot_ops.pmic_off_main_wa() < 0)
			pr_err("pmic_off_main_wa error\n");
	}

	sec_set_reboot_magic(SEC_REBOOT_LPM, SEC_REBOOT_END_OFFSET, 0xFF);
	psy_do_property("ac", get, POWER_SUPPLY_PROP_ONLINE, ac_val);
	psy_do_property("ac", get, POWER_SUPPLY_EXT_PROP_WATER_DETECT, water_val);
	psy_do_property("usb", get, POWER_SUPPLY_PROP_ONLINE, usb_val);
	psy_do_property("wireless", get, POWER_SUPPLY_PROP_ONLINE, wpc_val);
	reboot_charging = ac_val.intval || water_val.intval || usb_val.intval || wpc_val.intval;
	pr_info("[%s] reboot_charging(%d), AC[%d], USB[%d], WPC[%d], WATER[%d]\n",
			__func__, reboot_charging, ac_val.intval, usb_val.intval, wpc_val.intval, water_val.intval);

	pr_info("Exynos reboot, PWR Key(%d)\n", exynos_reboot_pwrkey_status());

	cache_flush_all();

	/* before power off */
	pr_crit("%s: call pre-power_off notifiers\n", __func__);
	atomic_notifier_call_chain(&sec_power_off_notifier_list, 0, NULL);

	while (1) {
		/* Check reboot charging */
#if IS_ENABLED(CONFIG_SEC_PARAM)
		if ((reboot_charging || (poweroff_try >= 5)) && !lpcharge) {
#else
		if (reboot_charging || (poweroff_try >= 5)) {
#endif
			regmap_write(pmureg, panic_inform, SEC_RESET_REASON_UNKNOWN);
			pr_emerg("%s: charger connected or power off failed(%d), reboot!\n", __func__, poweroff_try);
			/* To enter LP charging */

			exynos_mach_restart("sw reset");

			pr_emerg("%s: waiting for reboot\n", __func__);
			while (1);
		}
		/* wait for power button release.
		 * but after exynos_acpm_reboot is called
		 * power on status cannot be read */
		if (exynos_reboot_pwrkey_status()) 
			pr_info("PWR Key is not released (%d)(poweroff_try:%d)\n", exynos_reboot_pwrkey_status(), poweroff_try);
		else {
			/* PMIC EVT1: Fix off-sequence */
			if (exynos_reboot_ops.pmic_off_seq_wa) {
				if (exynos_reboot_ops.pmic_off_seq_wa() < 0)
					pr_err("pmic_off_seq_wa error\n");
			}

			if (exynos_reboot_ops.acpm_reboot)
				exynos_reboot_ops.acpm_reboot();
			else
				pr_err("Exynos reboot, acpm_reboot not registered\n");

			/* Add NFC Card mode workaround */
			exynos_pmu_read(EXYNOS_PMU_TOP_OUT_OFFSET, &val);
			val |= EXYNOS_PMU_TOP_OUT_PWRRGTON_NPU;
			pr_err("Exynos reboot, Write PWR 0x%08X\n", val);
			exynos_pmu_write(EXYNOS_PMU_TOP_OUT_OFFSET, val);

			while (1) {
				exynos_pmu_read(EXYNOS_PMU_VGPIO_TX_MONITOR_OFFSET, &val);
				val &= EXYNOS_PMU_VGPIO_TX_MONITOR_TXDONE;
				if ((val == EXYNOS_PMU_VGPIO_TX_MONITOR_TXDONE) ||
				    (npu_retry >= 1000)) {
					pr_err("Exynos reboot, npu_try is %d\n", npu_retry);
					break;
				} else {
					npu_retry ++;
					udelay(50);
				}
			}
			udelay(200);

			pr_emerg("Set PS_HOLD Low.\n");
			regmap_update_bits(pmureg, shutdown_offset, shutdown_trigger, 0);
			pr_emerg("Should not reach here! Device will be restarted after 3 sec.\n");
			mdelay(3000);
			regmap_write(pmureg, panic_inform, SEC_RESET_REASON_UNKNOWN);
			exynos_mach_restart("sw reset");
			pr_emerg("%s: waiting for reboot\n", __func__);
			while (1);
		}
		++poweroff_try;
		mdelay(1000);
	}
}

static int sec_reboot(struct notifier_block *this,
				unsigned long mode, void *cmd)
{
	local_irq_disable();

	hard_reset_delay();

	if (sec_reboot_on_panic && !cmd)
		cmd = panic_str;

	pr_emerg("%s (%d, %s)\n", __func__, reboot_mode, cmd ? cmd : "(null)");

	//secdbg_base_clear_magic_rambase();

	/* LPM mode prevention */
	sec_set_reboot_magic(SEC_REBOOT_NORMAL, SEC_REBOOT_END_OFFSET, 0xFF);

	if (cmd) {
		unsigned long value;
		if (!strcmp(cmd, "recovery-update"))
			regmap_write(pmureg, panic_inform, SEC_RESET_REASON_FOTA);
		else if (!strcmp(cmd, "fota_bl"))
			regmap_write(pmureg, panic_inform, SEC_RESET_REASON_FOTA_BL);
		else if (!strcmp(cmd, "recovery"))
			regmap_write(pmureg, panic_inform, SEC_RESET_REASON_RECOVERY);
		else if (!strcmp(cmd, "download"))
			regmap_write(pmureg, panic_inform, SEC_RESET_REASON_DOWNLOAD);
		else if (!strcmp(cmd, "bootloader"))
			regmap_write(pmureg, panic_inform, SEC_RESET_REASON_BOOTLOADER);
		else if (!strcmp(cmd, "upload"))
			regmap_write(pmureg, panic_inform, SEC_RESET_REASON_UPLOAD);
		else if (!strcmp(cmd, "secure"))
			regmap_write(pmureg, panic_inform, SEC_RESET_REASON_SECURE);
		else if (!strcmp(cmd, "fwup"))
			regmap_write(pmureg, panic_inform, SEC_RESET_REASON_FWUP);
		else if (!strcmp(cmd, "em_mode_force_user"))
			regmap_write(pmureg, panic_inform, SEC_RESET_REASON_EM_FUSE);
#if IS_ENABLED(CONFIG_SEC_ABC)
		else if (!strcmp(cmd, "user_dram_test") && sec_abc_get_enabled())
			regmap_write(pmureg, panic_inform, SEC_RESET_USER_DRAM_TEST);
#endif
		else if (!strncmp(cmd, "emergency", 9))
			regmap_write(pmureg, panic_inform, SEC_RESET_REASON_EMERGENCY);
		else if (!strncmp(cmd, "debug", 5) && !kstrtoul(cmd + 5, 0, &value))
			regmap_write(pmureg, panic_inform, SEC_RESET_SET_DEBUG | value);
		else if (!strncmp(cmd, "dump_sink", 9) && !kstrtoul(cmd + 9, 0, &value))
			regmap_write(pmureg, panic_inform, SEC_RESET_SET_DUMPSINK | (SEC_DUMPSINK_MASK & value));
		else if (!strncmp(cmd, "forceupload", 11) && !kstrtoul(cmd + 11, 0, &value))
			regmap_write(pmureg, panic_inform, SEC_RESET_SET_FORCE_UPLOAD | value);
		else if (!strncmp(cmd, "dprm", 4))
			regmap_write(pmureg, panic_inform, SEC_RESET_SET_DPRM);
		else if (!strncmp(cmd, "swsel", 5) && !kstrtoul(cmd + 5, 0, &value))
			regmap_write(pmureg, panic_inform, SEC_RESET_SET_SWSEL | value);
		else if (!strncmp(cmd, "sud", 3) && !kstrtoul(cmd + 3, 0, &value))
			regmap_write(pmureg, panic_inform, SEC_RESET_SET_SUD | value);
		else if (!strncmp(cmd, "multicmd:", 9))
			sec_multicmd(cmd);
#if defined(CONFIG_SEC_SYSUP)
		else if (!strncmp(cmd, "param", 5))
			regmap_write(pmureg, panic_inform, SEC_RESET_SET_PARAM);
#endif
		else if (!strncmp(cmd, "cpmem_on", 8))
			regmap_write(pmureg, panic_inform, SEC_RESET_CP_DBGMEM | 0x1);
		else if (!strncmp(cmd, "cpmem_off", 9))
			regmap_write(pmureg, panic_inform, SEC_RESET_CP_DBGMEM | 0x2);
		else if (!strncmp(cmd, "mbsmem_on", 9))
			regmap_write(pmureg, panic_inform, SEC_RESET_CP_DBGMEM | 0x1);
		else if (!strncmp(cmd, "mbsmem_off", 10))
			regmap_write(pmureg, panic_inform, SEC_RESET_CP_DBGMEM | 0x2);
		else if (!strncmp(cmd, "panic", 5)) {
			/*
			 * This line is intentionally blanked because the PANIC INFORM is used for upload cause
			 * in sec_debug_set_upload_cause() only in case of  panic() .
			 */
		} else
			regmap_write(pmureg, panic_inform, SEC_RESET_REASON_UNKNOWN);
	} else {
		regmap_write(pmureg, panic_inform, SEC_RESET_REASON_UNKNOWN);
	}

	cache_flush_all();

	return NOTIFY_DONE;
}

static int sec_reboot_panic_handler(struct notifier_block *nb,
				unsigned long l, void *buf)
{
	pr_emerg("sec_reboot: %s\n", __func__);
	sec_reboot_on_panic = 1;

	return NOTIFY_DONE;
}

static struct notifier_block nb_panic_block = {
	.notifier_call = sec_reboot_panic_handler,
	.priority = 128,
};

static struct notifier_block sec_restart_nb = {
	.notifier_call = sec_reboot,
	.priority = 130,
};

static int sec_reboot_probe(struct platform_device *pdev)
{
	struct device *dev = &pdev->dev;
	struct device_node *np = pdev->dev.of_node;
	int err;

	pmureg = syscon_regmap_lookup_by_phandle(dev->of_node,
						"samsung,syscon-phandle");
	if (IS_ERR(pmureg)) {
		pr_err("%s: failed to get regmap of PMU\n", __func__);
		return PTR_ERR(pmureg);
	}

	if (of_property_read_u32(np, "magic-inform", &magic_inform) < 0) {
		pr_err("%s: failed to find magic-inform property\n", __func__);
		return -EINVAL;
	}

	if (of_property_read_u32(np, "panic-inform", &panic_inform) < 0) {
		pr_err("%s: failed to find panic-inform property\n", __func__);
		return -EINVAL;
	}

	if (of_property_read_u32(np, "shutdown-offset", &shutdown_offset) < 0) {
		pr_err("%s: failed to find shutdown-offset property\n", __func__);
		return -EINVAL;
	}

	if (of_property_read_u32(np, "shutdown-trigger", &shutdown_trigger) < 0) {
		pr_err("%s: failed to find shutdown-trigger property\n", __func__);
		return -EINVAL;
	}

	err = atomic_notifier_chain_register(&panic_notifier_list, &nb_panic_block);
	if (err) {
		dev_err(&pdev->dev, "cannot register panic handler (err=%d)\n",
			err);
	}

	err = register_restart_handler(&sec_restart_nb);
	if (err) {
		dev_err(&pdev->dev, "cannot register restart handler (err=%d)\n",
			err);
	}
	pm_power_off = sec_power_off;

	dev_info(&pdev->dev, "register restart handler successfully\n");

	return err;
}

static const struct of_device_id sec_reboot_of_match[] = {
	{ .compatible = "samsung,sec-reboot" },
	{}
};

static struct platform_driver sec_reboot_driver = {
	.probe = sec_reboot_probe,
	.driver = {
		.name = "sec-reboot",
		.of_match_table = sec_reboot_of_match,
	},
};
module_platform_driver(sec_reboot_driver);

MODULE_DESCRIPTION("Samsung Reboot driver");
MODULE_LICENSE("GPL v2");
MODULE_ALIAS("platform:sec-reboot");
