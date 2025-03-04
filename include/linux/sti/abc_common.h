/* abc_common.h
 *
 * Abnormal Behavior Catcher Common Driver
 *
 * Copyright (C) 2017 Samsung Electronics
 *
 * Hyeokseon Yu <hyeokseon.yu@samsung.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 */

#ifndef SEC_ABC_H
#define SEC_ABC_H
#include <linux/kconfig.h>
#include <linux/kernel.h>
#include <linux/module.h>
#if IS_ENABLED(CONFIG_DRV_SAMSUNG)
#include <linux/sec_class.h>
#else
extern struct class *sec_class;
#endif
#include <linux/device.h>
#include <linux/platform_device.h>
#include <linux/of.h>
#include <linux/of_device.h>
#include <linux/err.h>
#include <linux/kthread.h>
#include <linux/delay.h>
#include <linux/io.h>
#include <linux/slab.h>
#include <linux/suspend.h>
#include <linux/workqueue.h>
#include <linux/rtc.h>
#include <linux/version.h>
#if IS_ENABLED(CONFIG_SEC_KUNIT)
#include <kunit/test.h>
#include <kunit/mock.h>
#else
#define __visible_for_testing static
#endif
#if (KERNEL_VERSION(4, 11, 0) <= LINUX_VERSION_CODE)
#include <linux/sched/clock.h>
#else
#include <linux/sched.h>
#endif
#if IS_ENABLED(CONFIG_SEC_ABC_MOTTO)
#include <linux/sti/abc_motto.h>
#endif
#ifndef EXPORT_SYMBOL_KUNIT
#define EXPORT_SYMBOL_KUNIT(sym)	/* nothing */
#endif
#define ABC_UEVENT_MAX		20
#define ABC_BUFFER_MAX		256
#define ABC_LOG_STR_LEN		50
#define ABC_LOG_MAX			80
#define ABC_DISABLED		0
#define ERROR_REPORT_MODE_BIT	(1<<0)
#define ALL_REPORT_MODE_BIT		(1<<1)
#define ABC_WAIT_ENABLE_TIMEOUT	10000

#define ABC_PRINT(format, ...) pr_info("[sec_abc] " format, ##__VA_ARGS__)

struct abc_fault_info {
	unsigned long cur_time;
	int cur_cnt;
};

struct abc_buffer {
	int size;
	int rear;
	int front;

	struct abc_fault_info *abc_element;
};

struct abc_qdata {
	const char *desc;
	int queue_size;
	int threshold_cnt;
	int threshold_time;
	int fail_cnt;

	struct abc_buffer buffer;
};



struct abc_platform_data {
	struct abc_qdata *gpu_items;
	struct abc_qdata *gpu_page_items;
	struct abc_qdata *aicl_items;
#if IS_ENABLED(CONFIG_SEC_ABC_MOTTO)
	struct abc_motto_data *motto_data;
#endif

	unsigned int nItem;
	unsigned int nGpu;
	unsigned int nGpuPage;
	unsigned int nAicl;
};

struct abc_log_entry {
	struct list_head node;
	char abc_log_str[ABC_LOG_STR_LEN];
};

struct abc_info {
	struct device *dev;
	struct workqueue_struct *workqueue;
	struct work_struct work;
	struct list_head log_list;
	struct completion enable_done;
	int log_list_cnt;
	char abc_str[ABC_BUFFER_MAX];
	struct abc_platform_data *pdata;
	struct mutex log_mutex;
};

extern void sec_abc_send_event(char *str);
extern int sec_abc_get_enabled(void);
extern int sec_abc_wait_enabled(void);

#endif
