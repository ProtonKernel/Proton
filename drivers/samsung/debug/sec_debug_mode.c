// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright (c) 2020 Samsung Electronics Co., Ltd.
 *      http://www.samsung.com
 *
 * Samsung TN debugging code
 *
 */

#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/moduleparam.h>
#include <linux/platform_device.h>
#include <linux/sec_debug.h>
#include <soc/samsung/debug-snapshot.h>

/* upload mode en/disable */
int __read_mostly force_upload;
module_param(force_upload, int, 0440);

static int secdbg_mode_panic_handler(struct notifier_block *nb,
				   unsigned long l, void *buf)
{
	if (!secdbg_mode_enter_upload()) {
		dbg_snapshot_scratch_clear();
		pr_info("%s: dbg_snapshot_scratch_clear done.. (force_upload: %d)\n", __func__, force_upload);
	}

	return NOTIFY_DONE;
}

static struct notifier_block nb_panic_block = {
	.notifier_call = secdbg_mode_panic_handler,
	.priority = INT_MAX,
};

int secdbg_mode_enter_upload(void)
{
	return force_upload;
}
EXPORT_SYMBOL(secdbg_mode_enter_upload);

static int __init secdbg_mode_init(void)
{
	pr_info("%s: force_upload is %d\n", __func__, force_upload);

	atomic_notifier_chain_register(&panic_notifier_list, &nb_panic_block);

	return 0;
}
module_init(secdbg_mode_init);

static void __exit secdbg_mode_exit(void)
{
	atomic_notifier_chain_unregister(&panic_notifier_list, &nb_panic_block);
}
module_exit(secdbg_mode_exit);

MODULE_DESCRIPTION("Samsung Debug mode driver");
MODULE_LICENSE("GPL v2");
