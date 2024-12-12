// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright (c) 2014-2020 Samsung Electronics Co., Ltd.
 *      http://www.samsung.com
 *
 * sec_debug_wdd_info.c
 */

#include <linux/module.h>
#include <linux/sched/clock.h>
#include <linux/rtc.h>
#include "sec_debug_internal.h"

extern int secdbg_wdd_register_ping_notifier(struct notifier_block *nb);
struct watchdogd_info *secdbg_base_get_wdd_info(void);
static struct watchdogd_info *wdd_info;
static struct rtc_time wdd_info_tm;

static int secdbg_wdd_ping_handler(struct notifier_block *nb,
					   unsigned long l, void *p)
{
	time64_t sec;

	if (!wdd_info) {
		pr_info("%s: wdd_info not initialized\n", __func__);
		return NOTIFY_DONE;
	}

	if (wdd_info->init_done == false) {
		pr_info("%s: wdd_info->init_done: %s\n", __func__, wdd_info->init_done ? "true" : "false");
		wdd_info->tsk = current;
		wdd_info->thr = current_thread_info();
		wdd_info->init_done = true;
	}

	pr_debug("%s: wdd_info: 0x%p\n", __func__, wdd_info);
	wdd_info->last_ping_cpu = raw_smp_processor_id();
	wdd_info->last_ping_time = sched_clock();

	sec = ktime_get_real_seconds();
	rtc_time_to_tm(sec, wdd_info->tm);
	pr_info("Watchdog: %s RTC %d-%02d-%02d %02d:%02d:%02d UTC\n",
			__func__,
			wdd_info->tm->tm_year + 1900, wdd_info->tm->tm_mon + 1,
			wdd_info->tm->tm_mday, wdd_info->tm->tm_hour,
			wdd_info->tm->tm_min, wdd_info->tm->tm_sec);

	return NOTIFY_DONE;
}

static struct notifier_block secdbg_wdd_ping_block = {
	.notifier_call = secdbg_wdd_ping_handler,
};

static int __init secdbg_wdd_init(void)
{
	wdd_info = secdbg_base_get_wdd_info();
	if (wdd_info) {
		wdd_info->init_done = false;
		wdd_info->tm = &wdd_info_tm;
		wdd_info->emerg_addr = 0;
	}

	secdbg_wdd_register_ping_notifier(&secdbg_wdd_ping_block);

	return 0;
}
subsys_initcall_sync(secdbg_wdd_init);

MODULE_DESCRIPTION("Samsung Debug Watchdog debug driver");
MODULE_LICENSE("GPL v2");
