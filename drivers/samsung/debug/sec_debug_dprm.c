// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright (c) 2020 Samsung Electronics Co., Ltd.
 *      http://www.samsung.com
 *
 * Samsung TN debugging code
 *
 */

#include <linux/of.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/ctype.h>
#include <linux/moduleparam.h>
#include <linux/platform_device.h>
#include <linux/notifier.h>
#include <linux/reboot.h>
#include <linux/string.h>

#include <linux/sec_debug.h>
#include "sec_debug_internal.h"

#define DPRM_MAGIC			0xABC
#define DPRM_MAGIC_SHIFT	16

struct sec_debug_kcnst *kcnst;

static void set_sec_dprm_value(uint64_t value)
{
	if (!kcnst) {
		pr_crit("%s: No kcnst buffer\n", __func__);
		return;
	}

	kcnst->target_dprm_mask = value;
	kcnst->target_dprm_mask |= (DPRM_MAGIC << DPRM_MAGIC_SHIFT);
	pr_crit("%s: target_dprm_mask: 0x%llx\n", __func__, kcnst->target_dprm_mask);
}

static int sec_dprm_reboot_handler(struct notifier_block *this,
				unsigned long mode, void *cmd)
{
	unsigned long value;

	if (cmd && !strncmp(cmd, "dprm", 4) && !kstrtoul(cmd + 4, 0, &value))
		set_sec_dprm_value((uint64_t) value);

	return 0;
}

static int sec_debug_get_clear_target_dprm_mask(char *buffer, const struct kernel_param *kp)
{
	return sprintf(buffer, "0x%llx\n", kcnst->target_dprm_mask);
}

static int sec_debug_set_clear_target_dprm_mask(const char *val, const struct kernel_param *kp)
{
	int ret;
	int argc = 0;
	char **argv;
	unsigned long mask;

	argv = argv_split(GFP_KERNEL, val, &argc);

	if (!strcmp(argv[0], "dprm")) {
		pr_crit("%s() arg : %s\n", __func__, val);

		if (argc > 1) {
			ret = kstrtoul(argv[1], 16, &mask);
			if (!ret)
				set_sec_dprm_value((uint64_t) mask);
		}
	}

	argv_free(argv);

	return 0;
}

static const struct kernel_param_ops sec_debug_clear_target_dprm_ops = {
		.set	= sec_debug_set_clear_target_dprm_mask,
		.get	= sec_debug_get_clear_target_dprm_mask,
};

module_param_cb(mask, &sec_debug_clear_target_dprm_ops, NULL, 0600);

static struct notifier_block sec_dprm_reboot_notifier = {
	.notifier_call = sec_dprm_reboot_handler,
};

static int __init secdbg_dprm_init(void)
{
	int ret;

	kcnst = secdbg_base_get_kcnst_base();
	if (!kcnst) {
		pr_crit("%s: No kcnst buffer\n", __func__);
		return 0;
	}

	ret = register_reboot_notifier(&sec_dprm_reboot_notifier);
	if (ret)
		pr_crit("cannot register reboot handler");

	return 0;
}
module_init(secdbg_dprm_init);

static void __exit secdbg_dprm_exit(void)
{
	unregister_reboot_notifier(&sec_dprm_reboot_notifier);
}
module_exit(secdbg_dprm_exit);

MODULE_DESCRIPTION("Samsung Debug Dprm driver");
MODULE_LICENSE("GPL v2");
