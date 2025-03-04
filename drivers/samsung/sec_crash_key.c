/*
 * Copyright (c) 2018 Samsung Electronics Co., Ltd.
 *      http://www.samsung.com
 *
 * Samsung TN debugging code
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/notifier.h>
#include <linux/sec_debug.h>
#include <linux/input.h>
#include "sec_key_notifier.h"

//static spinlock_t key_crash_lock;

static void sec_check_crash_key(unsigned int code, int value)
{
	static bool volup_p = false;
	static bool voldown_p = false;
	static int loopcount = 0;

	if (code == KEY_POWER)
		pr_info("%s: KEY(%d) %s\n", __func__ , code, value ? "pressed" : "released");

	/* Enter Forced Upload. Hold volume down key first
	 * and then press power key twice. Volume up key should not be pressed.
	 */
	if (value) {
		if (code == KEY_VOLUMEUP)
			volup_p = true;
		if (code == KEY_VOLUMEDOWN)
			voldown_p = true;
		if (!volup_p && voldown_p) {
			if (code == KEY_POWER) {
				pr_info("%s: count for entering forced upload [%d]\n",
					 __func__, ++loopcount);
				if (loopcount == 2)
					panic("Crash Key");
			}
		}
	} else {
		if (code == KEY_VOLUMEUP)
			volup_p = false;
		if (code == KEY_VOLUMEDOWN) {
			loopcount = 0;
			voldown_p = false;
		}
	}
}

static int sec_crash_key_check_keys(struct notifier_block *nb,
				unsigned long type, void *data)
{
	struct sec_key_notifier_param *param = data;
	unsigned int code = param->keycode;
	int state = param->down;

	sec_check_crash_key(code, state);

	return NOTIFY_DONE;
}

static struct notifier_block seccmn_crash_key_notifier = {
	.notifier_call = sec_crash_key_check_keys,
};

static int __init sec_crash_key_init(void)
{
	int ret = 0;

	/* only work when upload enabled*/
	if (secdbg_mode_enter_upload()) {
		ret = sec_kn_register_notifier(&seccmn_crash_key_notifier);
		pr_info("%s: force upload key registered (%d)\n", __func__, ret);
	}

	return ret;
}

static void sec_crash_key_exit(void)
{
	sec_kn_unregister_notifier(&seccmn_crash_key_notifier);
}

module_init(sec_crash_key_init);
module_exit(sec_crash_key_exit);

MODULE_DESCRIPTION("Samsung Crash-key driver");
MODULE_LICENSE("GPL v2");
