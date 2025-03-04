// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright (c) 2020 Samsung Electronics Co., Ltd.
 *		http://www.samsung.com
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 and
 * only version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 */

#ifndef __SYSTEM_EVENT_H
#define __SYSTEM_EVENT_H

#include <linux/spinlock.h>
#include <linux/interrupt.h>

#define MAX_SYSTEM_EVENT_REASON_LEN  256U
#define MAX_CRASH_TIMESTAMP_LEN  30U

struct sysevent_device;
extern struct bus_type sysevent_bus_type;

enum {
	RESET_SOC = 0,
	RESET_SYSTEM_EVENT_COUPLED,
	RESET_LEVEL_MAX
};

enum crash_status {
	CRASH_STATUS_NO_CRASH = 0,
	CRASH_STATUS_ERR_FATAL,
	CRASH_STATUS_WATCHDOG,
};

struct device;
struct module;

/**
 * struct sysevent_desc - sysevent descriptor
 * @name: name of sysevent
 * @fw_name: firmware name
 * @dev: parent device
 * @owner: module the descriptor belongs to
 * @shutdown: Stop a sysevent
 * @powerup: Start a sysevent
 * @crash_shutdown: Shutdown a sysevent when the system crashes (can't sleep)
 * @ramdump: Collect a ramdump of the sysevent
 * @free_memory: Free the memory associated with this sysevent
 * @no_auth: Set if sysevent does not rely on PIL to authenticate and bring
 * it out of reset
 * @ignore_sysevent_failure: SSR failures are usually fatal and results in panic. If
 * set will ignore failure.
 * @last_crash_reason: reason of the last crash
 * @last_crash_timestamp: timestamp of the last crash
 * @sysevent_sysfs_lock: protects ssr sysfs nodes access
 */
struct sysevent_desc {
	const char *name;
	char fw_name[256];
	struct device *dev;
	struct module *owner;

	int (*shutdown)(const struct sysevent_desc *desc, bool force_stop);
	int (*powerup)(const struct sysevent_desc *desc);
	void (*crash_shutdown)(const struct sysevent_desc *desc);
	int (*ramdump)(int, const struct sysevent_desc *desc);
	void (*free_memory)(const struct sysevent_desc *desc);
	irqreturn_t (*err_fatal_handler)(int irq, void *dev_id);
	irqreturn_t (*watchdog_handler)(int irq, void *dev_id);
	irqreturn_t (*generic_handler)(int irq, void *dev_id);
	unsigned int err_fatal_irq;
	unsigned int watchdog_irq;
	unsigned int generic_irq;
	bool no_auth;
	bool ignore_sysevent_failure;
	char last_crash_reason[MAX_SYSTEM_EVENT_REASON_LEN];
	char last_crash_timestamp[MAX_CRASH_TIMESTAMP_LEN];
	spinlock_t sysevent_sysfs_lock;
};

/**
 * struct notif_data - additional notif information
 * @crashed: indicates if sysevent has crashed due to wdog bite or err fatal
 * @enable_ramdump: ramdumps disabled if set to 0
 * @no_auth: set if sysevent does not use PIL to bring it out of reset
 * @pdev: sysevent platform device pointer
 */
struct notif_data {
	enum crash_status crashed;
	int enable_ramdump;
	bool no_auth;
	struct platform_device *pdev;
};

#if IS_ENABLED(CONFIG_EXYNOS_SYSTEM_EVENT)
extern int sysevent_get_restart_level(struct sysevent_device *dev);
extern int sysevent_restart_dev(struct sysevent_device *dev);
extern int sysevent_restart(const char *name);
extern int sysevent_crashed(const char *name);

extern void *sysevent_get(const char *name);
extern void *sysevent_get_with_fwname(const char *name, const char *fw_name);
extern int sysevent_set_fwname(const char *name, const char *fw_name);
extern void sysevent_put(void *sysevent);

extern struct sysevent_device *sysevent_register(struct sysevent_desc *desc);
extern void sysevent_unregister(struct sysevent_device *dev);

extern void sysevent_default_online(struct sysevent_device *dev);
extern void sysevent_set_crash_status(struct sysevent_device *dev,
					enum crash_status crashed);
extern enum crash_status sysevent_get_crash_status(struct sysevent_device *dev);
void complete_shutdown_ack(struct sysevent_device *sysevent);
struct sysevent_device *find_sysevent_device(const char *str);
#else

static inline int sysevent_get_restart_level(struct sysevent_device *dev)
{
	return 0;
}

static inline int sysevent_restart_dev(struct sysevent_device *dev)
{
	return 0;
}

static inline int sysevent_restart(const char *name)
{
	return 0;
}

static inline int sysevent_crashed(const char *name)
{
	return 0;
}

static inline void *sysevent_get(const char *name)
{
	return NULL;
}

static inline void *sysevent_get_with_fwname(const char *name,
				const char *fw_name) {
	return NULL;
}

static inline int sysevent_set_fwname(const char *name,
				const char *fw_name) {
	return 0;
}

static inline void sysevent_put(void *sysevent) { }

static inline
struct sysevent_device *sysevent_register(struct sysevent_desc *desc)
{
	return NULL;
}

static inline void sysevent_unregister(struct sysevent_device *dev) { }

static inline void sysevent_default_online(struct sysevent_device *dev) { }
static inline void sysevent_set_crash_status(struct sysevent_device *dev,
						enum crash_status crashed) { }
static inline
enum crash_status sysevent_get_crash_status(struct sysevent_device *dev)
{
	return false;
}
#endif /* CONFIG_EXYNOS_SYSTEM_EVENT */
#endif
