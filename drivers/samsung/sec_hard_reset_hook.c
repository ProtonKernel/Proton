/*
 * Copyright (c) 2016 Samsung Electronics Co., Ltd.
 *      http://www.samsung.com
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#include <linux/kernel.h>
#include <linux/input.h>
#include <linux/hrtimer.h>
#include <linux/ktime.h>
#include <linux/delay.h>
#include <linux/atomic.h>
#include <linux/sec_hard_reset_hook.h>
#include <linux/module.h>
#ifdef CONFIG_OF
#include <linux/of.h>
#include <linux/of_gpio.h>
#endif
#include <linux/moduleparam.h>
#include "sec_key_notifier.h"

#ifndef ARRAY_SIZE
#define ARRAY_SIZE(a) (sizeof(a) / sizeof((a)[0]))
#endif

#if IS_ENABLED(CONFIG_SEC_PMIC_PWRKEY)
int (*pmic_key_is_pwron)(void);
EXPORT_SYMBOL(pmic_key_is_pwron);
#endif

typedef bool (*keystate_fn_t)(int gpio, bool active_low);

struct gpio_key_info {
	unsigned int keycode;
	int gpio;
	bool active_low;
	keystate_fn_t is_pressed_key;
};

static unsigned int hard_reset_keys[] = { KEY_POWER, KEY_VOLUMEDOWN };
static struct gpio_key_info keys_info[ARRAY_SIZE(hard_reset_keys)];

static atomic_t hold_keys = ATOMIC_INIT(0);
static ktime_t hold_time;
static struct hrtimer hard_reset_hook_timer;
static bool hard_reset_occurred;
static int all_pressed;
static int num_keys;

/* Proc node to enable hard reset */
static bool hard_reset_hook_enable = 1;
module_param_named(hard_reset_hook_enable, hard_reset_hook_enable, bool, 0664);
MODULE_PARM_DESC(hard_reset_hook_enable, "1: Enabled, 0: Disabled");

static bool is_hard_reset_key(unsigned int code)
{
	size_t i;

	for (i = 0; i < ARRAY_SIZE(hard_reset_keys); i++)
		if (code == hard_reset_keys[i])
			return true;
	return false;
}

static int hard_reset_key_set(unsigned int code)
{
	size_t i;

	for (i = 0; i < ARRAY_SIZE(hard_reset_keys); i++)
		if (code == hard_reset_keys[i])
			atomic_or(0x1 << i, &hold_keys);
	return atomic_read(&hold_keys);
}

static int hard_reset_key_unset(unsigned int code)
{
	size_t i;

	for (i = 0; i < ARRAY_SIZE(hard_reset_keys); i++)
		if (code == hard_reset_keys[i])
			atomic_and(~(0x1) << i, &hold_keys);

	return atomic_read(&hold_keys);
}

static int hard_reset_key_all_pressed(void)
{
	return (atomic_read(&hold_keys) == all_pressed);
}

static int get_gpio_info(unsigned int code, struct gpio_key_info *k_info)
{
	size_t i;

	for (i = 0; i < ARRAY_SIZE(keys_info); i++)
		if (code == keys_info[i].keycode) {
			k_info->keycode = code;
			k_info->gpio = keys_info[i].gpio;
			k_info->active_low = keys_info[i].active_low;
			k_info->is_pressed_key = keys_info[i].is_pressed_key;
			return 0;
		}
	return -1;
}

static bool is_pressed_gpio_key(int gpio, bool active_low)
{
	if (!gpio_is_valid(gpio))
		return false;

	if ((gpio_get_value(gpio) ? 1 : 0) ^ active_low)
		return true;
	else
		return false;
}

#if IS_ENABLED(CONFIG_SEC_PMIC_PWRKEY)
static bool is_pressed_pmic_key(int gpio, bool active_low)
{
	if (pmic_key_is_pwron())
		return true;
	return false;
}
#endif

static bool is_gpio_keys_all_pressed(void)
{
	size_t i;

	for (i = 0; i < ARRAY_SIZE(hard_reset_keys); i++) {
		struct gpio_key_info k_info;

		if (get_gpio_info(hard_reset_keys[i], &k_info))
			return false;
		pr_info("%s:key:%d, gpio:%d, active:%d\n", __func__,
			k_info.keycode, k_info.gpio, k_info.active_low);

		if (!k_info.is_pressed_key(k_info.gpio, k_info.active_low)) {
			pr_warn("[%d] is not pressed\n", k_info.keycode);
			return false;
		}
	}
	return true;
}

static enum hrtimer_restart hard_reset_hook_callback(struct hrtimer *hrtimer)
{
	if (!is_gpio_keys_all_pressed()) {
		pr_warn("All gpio keys are not pressed\n");
		return HRTIMER_NORESTART;
	}

	pr_err("Hard Reset\n");
	hard_reset_occurred = true;
	panic("Hard Reset Hook");
	return HRTIMER_RESTART;
}

static int load_gpio_key_info(void)
{
#ifdef CONFIG_OF
	size_t i;
	struct device_node *np, *pp;

	np = of_find_node_by_path("/gpio_keys");
	if (!np)
		return -1;
	for_each_child_of_node(np, pp) {
		uint keycode = 0;

		if (!of_find_property(pp, "gpios", NULL))
			continue;
		if (of_property_read_u32(pp, "linux,code", &keycode))
			continue;
		for (i = 0; i < ARRAY_SIZE(hard_reset_keys); ++i) {
			if (keycode == hard_reset_keys[i]) {
				enum of_gpio_flags flags;

				keys_info[num_keys].keycode = keycode;
				keys_info[num_keys].gpio = of_get_gpio_flags(pp, 0, &flags);
				if (gpio_is_valid(keys_info[num_keys].gpio))
					keys_info[num_keys].active_low = flags & OF_GPIO_ACTIVE_LOW;
				keys_info[num_keys].is_pressed_key = is_pressed_gpio_key;
				num_keys++;
				break;
			}
		}
	}
	of_node_put(np);
#endif
	return 0;
}

#if IS_ENABLED(CONFIG_SEC_PMIC_PWRKEY)
static int load_pmic_key_info(void)
{
#ifdef CONFIG_OF
	size_t i;
	struct device_node *np, *pp;
#ifdef CONFIG_SEC_PMIC_PWRKEY_DTNAME
	const char *dtname = CONFIG_SEC_PMIC_PWRKEY_DTNAME;

	if (dtname[0] == '\0')
		return -1;
#else
	const char *dtname = "s2mps23-keys";
#endif
	np = of_find_node_by_name(NULL, dtname);
	if (!np) {
		pr_err("could not find node by name(%s)\n", dtname);
		return -1;
	}

	for_each_child_of_node(np, pp) {
		uint keycode = 0;

		if (of_property_read_u32(pp, "linux,code", &keycode))
			continue;
		for (i = 0; i < ARRAY_SIZE(hard_reset_keys); ++i) {
			if (keycode == hard_reset_keys[i]) {
				keys_info[num_keys].keycode = keycode;
				keys_info[num_keys].gpio = -1;
				keys_info[num_keys].is_pressed_key = is_pressed_pmic_key;
				num_keys++;
				break;
			}
		}
	}
	of_node_put(np);
#endif
	return 0;
}
#else
#define load_pmic_key_info()	(0)
#endif

static int load_keys_info(void)
{
	load_gpio_key_info();
	load_pmic_key_info();
	return 0;
}

static int hard_reset_hook(struct notifier_block *nb,
			   unsigned long type, void *data)
{
	struct sec_key_notifier_param *param = data;
	unsigned int code = param->keycode;
	int pressed = param->down;

	if (unlikely(!hard_reset_hook_enable))
		return NOTIFY_DONE;

	if (!is_hard_reset_key(code))
		return NOTIFY_DONE;

	if (pressed)
		hard_reset_key_set(code);
	else
		hard_reset_key_unset(code);

	if (hard_reset_key_all_pressed()) {
		hrtimer_start(&hard_reset_hook_timer,
			hold_time, HRTIMER_MODE_REL);
		pr_info("%s : hrtimer_start\n", __func__);
	}
	else {
		hrtimer_try_to_cancel(&hard_reset_hook_timer);
	}

	return NOTIFY_OK;
}

static struct notifier_block seccmn_hard_reset_notifier = {
	.notifier_call = hard_reset_hook
};

void hard_reset_delay(void)
{
	/* HQE team request hard reset key should guarantee 7 seconds.
	 * To get proper stack, hard reset hook starts after 6 seconds.
	 * And it will reboot before 7 seconds.
	 * Add delay to keep the 7 seconds
	 */
	if (hard_reset_occurred) {
		pr_err("wait until warm or manual reset is triggered\n");
		mdelay(2000); // TODO: change to dev_mdelay
	}
}
EXPORT_SYMBOL(hard_reset_delay);

static int __init hard_reset_hook_init(void)
{
	size_t i;
	int ret = 0;

	hrtimer_init(&hard_reset_hook_timer, CLOCK_MONOTONIC, HRTIMER_MODE_ABS);
	hard_reset_hook_timer.function = hard_reset_hook_callback;
	hold_time = ktime_set(6, 0); /* 6 seconds */

	for (i = 0; i < ARRAY_SIZE(hard_reset_keys); i++)
		all_pressed |= 0x1 << i;
	load_keys_info();
	ret = sec_kn_register_notifier(&seccmn_hard_reset_notifier);

	return ret;
}

static void hard_reset_hook_exit(void)
{
	sec_kn_unregister_notifier(&seccmn_hard_reset_notifier);
}

module_init(hard_reset_hook_init);
module_exit(hard_reset_hook_exit);
#if IS_ENABLED(CONFIG_PINCTRL_SAMSUNG)
MODULE_SOFTDEP("pre: pinctrl-samsung-core");
#endif

MODULE_DESCRIPTION("Samsung Hard_reset_hook driver");
MODULE_LICENSE("GPL v2");
