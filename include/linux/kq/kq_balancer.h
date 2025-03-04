/* SPDX-License-Identifier: GPL-2.0-or-later */
/*
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * Copyright (c) 2020 Samsung Electronics Co., Ltd
 *
 * KQ(Kernel Quality) Balancer driver implementation
 *	: Jaecheol Kim <jc22.kim@samsung.com>
 */

#ifndef __KQ_BALANCER_H__
#define __KQ_BALANCER_H__

#include <linux/kobject.h>
#include <linux/notifier.h>
#include <linux/device.h>
#include <linux/kthread.h>
#include <linux/pm_qos.h>
#include <soc/samsung/exynos_pm_qos.h>
#include <soc/samsung/exynos-devfreq.h>

#define KQ_BALANCER_DEFAULT_OPER_TIMEOUT	400
#define KQ_BALANCER_DEFAULT_QOS_DELAY		10

#define KQ_BALANCER_MAX_FREQ_LEN		50
#define KQ_BALANCER_MAX_DELIMITER_LEN		6
#define KQ_BALANCER_MAX_MANUAL_DELIMITER_LEN	3
#define KQ_BALANCER_MAX_CMD_LEN			8
#define KQ_BALANCER_MAX_BUFFER	(KQ_BALANCER_MAX_CMD_LEN * \
	KQ_BALANCER_MAX_DELIMITER_LEN + KQ_BALANCER_MAX_DELIMITER_LEN)
#define KQ_BALANCER_MAX_MANUAL_BUFFER (KQ_BALANCER_MAX_CMD_LEN * \
	KQ_BALANCER_MAX_MANUAL_DELIMITER_LEN + KQ_BALANCER_MAX_MANUAL_DELIMITER_LEN)

/*
 * supported sysfs nodes in /sys/class/sec/sec_nad_balancer/
 */
enum {
	KQ_BALANCER_SYSFS_BALANCER = 0,
	KQ_BALANCER_SYSFS_STATUS,
	KQ_BALANCER_SYSFS_PRINTLOG,
	KQ_BALANCER_SYSFS_TIMEOUT,
	KQ_BALANCER_SYSFS_TABLES,
	KQ_BALANCER_SYSFS_MANUAL,

	KQ_BALANCER_SYSFS_END,
};

/*
 * currently balancer support only following format
 *
 * path : /sys/class/sec/sec_nad_balancer/balancer
 * var : start,timeout,cpu-lit,cpu-mid,cpu-big,dev-mif
 */
enum {
	KQ_BALANCER_CMD_OPER = 0,
	KQ_BALANCER_CMD_TIMEOUT,

	KQ_BALANCER_CMD_LIT,
	KQ_BALANCER_TYPE_LIT = KQ_BALANCER_CMD_LIT,
	KQ_BALANCER_CMD_MID,
	KQ_BALANCER_TYPE_MID = KQ_BALANCER_CMD_MID,
	KQ_BALANCER_CMD_BIG,
	KQ_BALANCER_TYPE_BIG = KQ_BALANCER_CMD_BIG,
	KQ_BALANCER_CMD_MIF,
	KQ_BALANCER_TYPE_MIF = KQ_BALANCER_CMD_MIF,
	KQ_BALANCER_CMD_INT,
	KQ_BALANCER_TYPE_INT = KQ_BALANCER_CMD_INT,

	KQ_BALANCER_TYPE_UNSUPPORTED,
	KQ_BALANER_CMD_END,
};

/*
 * balancer support 2 types of device
 * cpu : cpus in cpufreq driver
 * dev : devices in devfreq driver
 */
enum {
	KQ_BALANCER_DOMAIN_TYPE_CPU = 0,
	KQ_BALANCER_DOMAIN_TYPE_DEV,

	KQ_BALANCER_TYPE_END,
};

/*
 * balancer operation
 * stop & run
 */
enum {
	KQ_BALANCER_OPER_STOP = 0,
	KQ_BALANCER_OPER_RUN,

	KQ_BALANCER_OPER_END,
};

/*
 * There's only 2 type of qos policy supported in linux Driver
 */
enum {
	KQ_BALANCER_QOS_POLICY_NONE = 0,
	KQ_BALANCER_QOS_POLICY_MIN,
	KQ_BALANCER_QOS_POLICY_MAX,

	KQ_BALANCER_QOS_POLICY_END,
};

enum {
	KQ_BALANCER_CMD_MANUAL_DOMAIN = 0,
	KQ_BALANCER_CMD_MANUAL_QOS_TYPE,
	KQ_BALANCER_CMD_MANUAL_FREQ_IDX,

	KQ_BALANCER_CMD_MANUAL_END,
};

/*
 * Weird qos control of exynos2100 that's because of GKI.
 * cpu use freq_qos related function that exist in linux/pm_qos.h
 * devices use exynos_pm_qos related function that exist in linux/exynos_pm_qos.h
 * warning : please check which part of qos function used in your system before using it.
 */
enum {
	KQ_BALANCER_QOS_POLICY_TYPE_NONE = 0,

	KQ_BALANCER_QOS_POLICY_TYPE_LIT_MIN = PM_QOS_CLUSTER0_FREQ_MIN,
	KQ_BALANCER_QOS_POLICY_TYPE_LIT_MAX = PM_QOS_CLUSTER0_FREQ_MAX,

	KQ_BALANCER_QOS_POLICY_TYPE_MID_MIN = PM_QOS_CLUSTER1_FREQ_MIN,
	KQ_BALANCER_QOS_POLICY_TYPE_MID_MAX = PM_QOS_CLUSTER1_FREQ_MAX,

	KQ_BALANCER_QOS_POLICY_TYPE_BIG_MIN = PM_QOS_CLUSTER2_FREQ_MIN,
	KQ_BALANCER_QOS_POLICY_TYPE_BIG_MAX = PM_QOS_CLUSTER2_FREQ_MAX,

	KQ_BALANCER_QOS_POLICY_TYPE_MIF_MIN = PM_QOS_BUS_THROUGHPUT,
	KQ_BALANCER_QOS_POLICY_TYPE_MIF_MAX = PM_QOS_BUS_THROUGHPUT_MAX,

	KQ_BALANCER_QOS_POLICY_TYPE_END,
};

enum {
	KQ_BALANCER_QOS_UNLOCK = 0,
	KQ_BALANCER_QOS_LOCK,

	KQ_BALANCER_QOS_END,
};

enum {
	KQ_BALANCER_QOS_CONTROL_RANDOM = 0,
	KQ_BALANCER_QOS_CONTROL_MANUAL,

	KQ_BALANCER_QOS_CONTROL_END,
};

enum {
	KQ_BALANCER_TASK_DELAYED_WAKEUP = 0,
	KQ_BALANCER_TASK_NO_WAKEUP,

	KQ_BALANCER_TASK_END,
};

static ssize_t kq_balancer_show_attrs(struct device *dev,
	struct device_attribute *attr, char *buf);
static ssize_t kq_balancer_store_attrs(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t count);

#define KQ_BALANCER_ATTR(_name)	\
{	\
	.attr = { .name = #_name, .mode = 0664 },	\
	.show = kq_balancer_show_attrs,	\
	.store = kq_balancer_store_attrs,	\
}

struct balancer_freq_table {
	unsigned int frequency;
};

struct balancer_domain {
	/* list of balnacer domains */
	struct list_head list;

	/* balancer domain information */
	char *name;
	int type;
	int num;
	unsigned int max_freq;
	unsigned int min_freq;
	unsigned int table_size;
	struct balancer_freq_table *freq_table;

	/* balancer domain qos information */
	struct task_struct *kthread;
	struct exynos_pm_qos_request dev_qos;
	struct freq_qos_request cpu_qos;
	int qos_delay;
	int qos_status;
	int qos_type;
	int qos_policy;
	int qos_manual;
	int qos_manual_index;

	/* domain running status */
	int run;
};

/*
 * timeout : automatic qos timeout. default value - 400 seconds.
 * max_domain_count : balancer max support system domain count
 *	if system support cpu-lit, cpu-mid, cpu-big, it's value set as 3.
 * run : balancer qos thread status (running or stop)
 * sdev : /sys/class/sec/ device class pointer
 * dwork : delayed work for handling qos thread stop.
 */
struct balancer_info {
	int timeout;
	int max_domain_count;
	int run;
	struct device *sdev;
	struct delayed_work dwork;
};

#endif /* __KQ_BALANCER_H__ */
