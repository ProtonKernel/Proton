// SPDX-License-Identifier: GPL-2.0-only
/*
 *  exynos-reboot.c - Samsung Exynos SoC reset code
 *
 * Copyright (c) 2019-2020 Samsung Electronics Co., Ltd.
 *
 * Author: Hyunki Koo <hyunki00.koo@samsung.com>
 */

#include <linux/delay.h>
#include <linux/io.h>
#include <linux/module.h>
#include <linux/notifier.h>
#include <linux/of_address.h>
#include <linux/regmap.h>
#include <linux/mfd/syscon.h>
#include <linux/platform_device.h>
#include <linux/reboot.h>
#include <linux/reset/exynos-reset.h>
#include <linux/soc/samsung/exynos-soc.h>
#include <soc/samsung/debug-snapshot.h>
#include <soc/samsung/exynos-pmu-if.h>

#include <asm/proc-fns.h>

static struct regmap *pmureg;
struct exynos_reboot_helper_ops exynos_reboot_ops;
EXPORT_SYMBOL(exynos_reboot_ops);

static u32 reboot_offset, reboot_trigger;
static u32 reboot_cmd_offset;
static u32 shutdown_offset, shutdown_trigger;

extern void exynos_acpm_reboot(void);

void exynos_reboot_register_acpm_ops(void *acpm_reboot_func)
{
	if (acpm_reboot_func)
		exynos_reboot_ops.acpm_reboot = acpm_reboot_func;

	pr_info("Exynos Reboot: Add %s funtions\n",
			acpm_reboot_func ? "acpm_reboot" : "NULL");
}
EXPORT_SYMBOL(exynos_reboot_register_acpm_ops);

void exynos_reboot_register_pmic_ops(void *main_wa, void *sub_wa, void *seq_wa, void *pwrkey)
{
	if (main_wa)
		exynos_reboot_ops.pmic_off_main_wa = main_wa;

	if (sub_wa)
		exynos_reboot_ops.pmic_off_sub_wa = sub_wa;

	if (seq_wa)
		exynos_reboot_ops.pmic_off_seq_wa = seq_wa;

	if (pwrkey)
		exynos_reboot_ops.pmic_pwrkey_status = pwrkey;

	pr_info("Exynos Reboot: Add %s%s%s%s funtions\n",
			main_wa ? "main_wa, " : "",
			sub_wa ? "sub_wa, " : "",
			seq_wa ? "seq_wa, " : "",
			pwrkey ? "pwrkey" : "");
}
EXPORT_SYMBOL(exynos_reboot_register_pmic_ops);

static void exynos_reboot_print_socinfo(void)
{
	u32 soc_id, revision;

	/* Check by each SoC */
	soc_id = exynos_soc_info.product_id;
	revision = exynos_soc_info.revision;
	pr_info("Exynos Reboot: SOC ID %X. Revision: %x\n", soc_id, revision);
}

int exynos_reboot_pwrkey_status(void)
{
	if (exynos_reboot_ops.pmic_pwrkey_status)
		return exynos_reboot_ops.pmic_pwrkey_status();

	pr_err("Exynos reboot, No pwrkey func. enforce release state\n");
	return 0;
}
EXPORT_SYMBOL(exynos_reboot_pwrkey_status);

#if !IS_ENABLED(CONFIG_SEC_REBOOT)
static void exynos_power_off(void)
{
	u32 poweroff_try = 0;
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

	exynos_reboot_print_socinfo();

	pr_info("Exynos reboot, PWR Key(%d)\n", exynos_reboot_pwrkey_status());
	while (1) {
		/* wait for power button release.
		 * but after exynos_acpm_reboot is called
		 * power on status cannot be read */
		if ((poweroff_try) || (!exynos_reboot_pwrkey_status())) {

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

			++poweroff_try;
			pr_emerg("Should not reach here! (poweroff_try:%d)\n", poweroff_try);
		} else {
			pr_info("PWR Key is not released (%d)\n", exynos_reboot_pwrkey_status());
		}
		mdelay(1000);
	}
}
#endif

#define EXYNOS_PMU_SYSIP_DAT0		(0x0810)
#define REBOOT_MODE_NORMAL		(0x00)
#define REBOOT_MODE_CHARGE		(0x0A)
#define REBOOT_MODE_FASTBOOT		(0xFC)
#define REBOOT_MODE_FACTORY             (0xFD)
#define REBOOT_MODE_RECOVERY		(0xFF)

static void exynos_reboot_parse(void *cmd)
{
	if (cmd) {
		pr_info("Reboot command: (%s)\n", (char *)cmd);

		if (!strcmp((char *)cmd, "charge")) {
			regmap_write(pmureg, reboot_cmd_offset, REBOOT_MODE_CHARGE);
		} else if (!strcmp((char *)cmd, "bootloader") ||
			   !strcmp((char *)cmd, "fastboot") ||
			   !strcmp((char *)cmd, "bl") ||
			   !strcmp((char *)cmd, "fb")) {
			regmap_write(pmureg, reboot_cmd_offset, REBOOT_MODE_FASTBOOT);
		} else if (!strcmp((char *)cmd, "recovery")) {
			regmap_write(pmureg, reboot_cmd_offset, REBOOT_MODE_RECOVERY);
		} else if (!strcmp((char *)cmd, "sfactory")) {
			regmap_write(pmureg, reboot_cmd_offset, REBOOT_MODE_FACTORY);
		}
	}
}

static int exynos_restart_handler(struct notifier_block *this,
				unsigned long mode, void *cmd)
{
	if (exynos_reboot_ops.acpm_reboot)
		exynos_reboot_ops.acpm_reboot();
	else
		pr_err("Exynos reboot, acpm_reboot not registered\n");

	exynos_reboot_parse(cmd);
	exynos_reboot_print_socinfo();
	/* Do S/W Reset */
	pr_emerg("%s: Exynos SoC reset right now\n", __func__);
	regmap_write(pmureg, reboot_offset, reboot_trigger);

	while (1)
		wfi();

	return NOTIFY_DONE;
}

void exynos_mach_restart(const char *cmd)
{
	exynos_restart_handler(NULL, 0, (void *)cmd);
}
EXPORT_SYMBOL_GPL(exynos_mach_restart);

static struct notifier_block exynos_restart_nb = {
	.notifier_call = exynos_restart_handler,
	.priority = 128,
};

static int exynos_reboot_probe(struct platform_device *pdev)
{
	struct device *dev = &pdev->dev;
	struct device_node *np = pdev->dev.of_node;
	int err;

	pmureg = syscon_regmap_lookup_by_phandle(dev->of_node,
						"samsung,syscon-phandle");
	if (IS_ERR(pmureg)) {
		pr_err("Fail to get regmap of PMU\n");
		return PTR_ERR(pmureg);
	}

	if (of_property_read_u32(np, "reboot-offset", &reboot_offset) < 0) {
		pr_err("failed to find reboot-offset property\n");
		return -EINVAL;
	}

	if (of_property_read_u32(np, "reboot-trigger", &reboot_trigger) < 0) {
		pr_err("failed to find reboot-trigger property\n");
		return -EINVAL;
	}

	if (of_property_read_u32(np, "shutdown-offset", &shutdown_offset) < 0) {
		pr_err("failed to find shutdown-offset property\n");
		return -EINVAL;
	}

	if (of_property_read_u32(np, "shutdown-trigger", &shutdown_trigger) < 0) {
		pr_err("failed to find shutdown-trigger property\n");
		return -EINVAL;
	}

	if (of_property_read_u32(np, "reboot-cmd-offset", &reboot_cmd_offset) < 0) {
		pr_info("failed to find reboot-offset property, using default\n");
		reboot_cmd_offset = EXYNOS_PMU_SYSIP_DAT0;
	}

	err = register_restart_handler(&exynos_restart_nb);
	if (err) {
		dev_err(&pdev->dev, "cannot register restart handler (err=%d)\n",
			err);
	}
#if !IS_ENABLED(CONFIG_SEC_REBOOT)
	pm_power_off = exynos_power_off;
#endif

	dev_info(&pdev->dev, "register restart handler successfully\n");

	return err;
}

static const struct of_device_id exynos_reboot_of_match[] = {
	{ .compatible = "samsung,exynos-reboot" },
	{}
};

static struct platform_driver exynos_reboot_driver = {
	.probe = exynos_reboot_probe,
	.driver = {
		.name = "exynos-reboot",
		.of_match_table = exynos_reboot_of_match,
	},
};
module_platform_driver(exynos_reboot_driver);

MODULE_SOFTDEP("pre: exynos-chipid_v2");
MODULE_DESCRIPTION("Exynos Reboot driver");
MODULE_LICENSE("GPL v2");
MODULE_ALIAS("platform:exynos-reboot");
