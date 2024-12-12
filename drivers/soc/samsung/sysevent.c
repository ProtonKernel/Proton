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

#define pr_fmt(fmt) "sysevent: %s(): " fmt, __func__

#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/uaccess.h>
#include <linux/module.h>
#include <linux/fs.h>
#include <linux/delay.h>
#include <linux/list.h>
#include <linux/io.h>
#include <linux/kthread.h>
#include <linux/time.h>
#include <linux/rtc.h>
#include <linux/suspend.h>
#include <linux/mutex.h>
#include <linux/slab.h>
#include <linux/spinlock.h>
#include <linux/device.h>
#include <linux/idr.h>
#include <linux/interrupt.h>
#include <linux/cdev.h>
#include <linux/platform_device.h>
#include <linux/of_irq.h>
#include <linux/of.h>
#include <linux/timekeeping.h>
#include <asm/current.h>
#include <linux/timer.h>

#include <soc/samsung/imgloader.h>
#include <soc/samsung/sysevent.h>
#include <soc/samsung/sysevent_notif.h>

static uint disable_sysevent_support;
module_param(disable_sysevent_support, uint, 0644);

/**
 * enum p_sysevent_state - state of a sysevent (private)
 * @SYSTEM_EVENT_NORMAL: sysevent is operating normally
 * @SYSTEM_EVENT_CRASHED: sysevent has crashed and hasn't been shutdown
 * @SYSTEM_EVENT_RESTARTING: sysevent has been shutdown and is now restarting
 *
 * The 'private' side of the subsytem state used to determine where in the
 * restart process the sysevent is.
 */
enum p_sysevent_state {
	SYSTEM_EVENT_NORMAL,
	SYSTEM_EVENT_CRASHED,
	SYSTEM_EVENT_RESTARTING,
};

/**
 * enum sysevent_state - state of a sysevent (public)
 * @SYSTEM_EVENT_OFFLINING: sysevent is offlining
 * @SYSTEM_EVENT_OFFLINE: sysevent is offline
 * @sysevent_ONLINE: sysevent is online
 *
 * The 'public' side of the subsytem state, exposed to userspace.
 */
enum sysevent_state {
	SYSTEM_EVENT_OFFLINING,
	SYSTEM_EVENT_OFFLINE,
	SYSTEM_EVENT_ONLINE,
};

static const char * const sysevent_states[] = {
	[SYSTEM_EVENT_OFFLINING] = "OFFLINING",
	[SYSTEM_EVENT_OFFLINE] = "OFFLINE",
	[SYSTEM_EVENT_ONLINE] = "ONLINE",
};

static const char * const restart_levels[] = {
	[RESET_SOC] = "SYSTEM",
	[RESET_SYSTEM_EVENT_COUPLED] = "RELATED",
};

/**
 * struct sysevent_tracking - track state of a sysevent or restart order
 * @p_state: private state of sysevent/order
 * @state: public state of sysevent/order
 * @s_lock: protects p_state
 * @lock: protects sysevent/order callbacks and state
 *
 * Tracks the state of a sysevent or a set of sysevents (restart order).
 * Doing this avoids the need to grab each sysevent's lock and update
 * each sysevents state when restarting an order.
 */
struct sysevent_tracking {
	enum p_sysevent_state p_state;
	spinlock_t s_lock;
	enum sysevent_state state;
	struct mutex lock;
};

struct restart_log {
	struct timeval time;
	struct sysevent_device *dev;
	struct list_head list;
};

/**
 * struct sysevent_device - sysevent device
 * @desc: sysevent descriptor
 * @work: context for sysevent_restart_wq_func() for this device
 * @sysevent_wlock: prevents suspend during sysevent_restart()
 * @device_restart_work: work struct for device restart
 * @track: state tracking and locking
 * @notify: sysevent notify handle
 * @dev: device
 * @owner: module that provides @desc
 * @count: reference count of sysevent_get()/sysevent_put()
 * @id: ida
 * @restart_level: restart level (0 - panic, 1 - related, 2 - independent, etc.)
 * @restart_order: order of other devices this devices restarts with
 * @crash_count: number of times the device has crashed
 * @do_ramdump_on_put: ramdump on sysevent_put() if true
 * @err_ready: completion variable to record error ready from sysevent
 * @crashed: indicates if sysevent has crashed
 * @notif_state: current state of sysevent in terms of sysevent notifications
 */
struct sysevent_device {
	struct sysevent_desc *desc;
	struct work_struct work;
	struct wakeup_source *sysevent_wlock;
	char wlname[64];
	struct work_struct device_restart_work;
	struct sysevent_tracking track;

	void *notify;
	void *early_notify;
	struct device dev;
	struct module *owner;
	int count;
	int id;
	int restart_level;
	int crash_count;
	bool do_ramdump_on_put;
	struct cdev char_dev;
	dev_t dev_no;
	enum crash_status crashed;
	int notif_state;
	struct list_head list;
};

static struct sysevent_device *to_sysevent(struct device *d)
{
	return container_of(d, struct sysevent_device, dev);
}

static struct sysevent_tracking *sysevent_get_track(struct sysevent_device *sysevent)
{
	return &sysevent->track;
}

static ssize_t name_show(struct device *dev, struct device_attribute *attr,
		char *buf)
{
	return snprintf(buf, PAGE_SIZE, "%s\n", to_sysevent(dev)->desc->name);
}
static DEVICE_ATTR_RO(name);

static ssize_t state_show(struct device *dev, struct device_attribute *attr,
		char *buf)
{
	enum sysevent_state state = to_sysevent(dev)->track.state;

	return snprintf(buf, PAGE_SIZE, "%s\n", sysevent_states[state]);
}
static DEVICE_ATTR_RO(state);

static ssize_t crash_count_show(struct device *dev,
				struct device_attribute *attr, char *buf)
{
	return snprintf(buf, PAGE_SIZE, "%d\n", to_sysevent(dev)->crash_count);
}
static DEVICE_ATTR_RO(crash_count);

static ssize_t
restart_level_show(struct device *dev, struct device_attribute *attr, char *buf)
{
	int level = to_sysevent(dev)->restart_level;

	return snprintf(buf, PAGE_SIZE, "%s\n", restart_levels[level]);
}

static ssize_t crash_reason_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	int ret;
	unsigned long flags;
	struct sysevent_device *sysevent = to_sysevent(dev);

	spin_lock_irqsave(&sysevent->desc->sysevent_sysfs_lock, flags);
	ret = snprintf(buf, PAGE_SIZE, "%s\n",
		to_sysevent(dev)->desc->last_crash_reason);
	spin_unlock_irqrestore(&sysevent->desc->sysevent_sysfs_lock, flags);
	return ret;
}
static DEVICE_ATTR_RO(crash_reason);

static ssize_t crash_timestamp_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	int ret;
	unsigned long flags;
	struct sysevent_device *sysevent = to_sysevent(dev);

	spin_lock_irqsave(&sysevent->desc->sysevent_sysfs_lock, flags);
	ret = snprintf(buf, PAGE_SIZE, "%s\n",
		to_sysevent(dev)->desc->last_crash_timestamp);
	spin_unlock_irqrestore(&sysevent->desc->sysevent_sysfs_lock, flags);
	return ret;
}
static DEVICE_ATTR_RO(crash_timestamp);

static ssize_t restart_level_store(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t count)
{
	struct sysevent_device *sysevent = to_sysevent(dev);
	const char *p;
	int i, orig_count = count;

	p = memchr(buf, '\n', count);
	if (p)
		count = p - buf;

	for (i = 0; i < ARRAY_SIZE(restart_levels); i++)
		if (!strncasecmp(buf, restart_levels[i], count)) {
			sysevent->restart_level = i;
			return orig_count;
		}
	return -EPERM;
}
static DEVICE_ATTR_RW(restart_level);

static ssize_t firmware_name_show(struct device *dev,
				struct device_attribute *attr, char *buf)
{
	return snprintf(buf, PAGE_SIZE, "%s\n", to_sysevent(dev)->desc->fw_name);
}

static ssize_t firmware_name_store(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t count)
{
	struct sysevent_device *sysevent = to_sysevent(dev);
	struct sysevent_tracking *track = sysevent_get_track(sysevent);
	const char *p;
	int orig_count = count;

	p = memchr(buf, '\n', count);
	if (p)
		count = p - buf;

	pr_info("Changing sysevent fw_name to %s\n", buf);
	mutex_lock(&track->lock);
	strlcpy(sysevent->desc->fw_name, buf,
			min(count + 1, sizeof(sysevent->desc->fw_name)));
	mutex_unlock(&track->lock);
	return orig_count;
}
static DEVICE_ATTR_RW(firmware_name);

int sysevent_get_restart_level(struct sysevent_device *dev)
{
	return dev->restart_level;
}
EXPORT_SYMBOL(sysevent_get_restart_level);

static void sysevent_set_state(struct sysevent_device *sysevent,
			     enum sysevent_state state)
{
	unsigned long flags;

	spin_lock_irqsave(&sysevent->track.s_lock, flags);
	if (sysevent->track.state != state) {
		sysevent->track.state = state;
		spin_unlock_irqrestore(&sysevent->track.s_lock, flags);
		sysfs_notify(&sysevent->dev.kobj, NULL, "state");
		return;
	}
	spin_unlock_irqrestore(&sysevent->track.s_lock, flags);
}

/**
 * subsytem_default_online() - Mark a sysevent as online by default
 * @dev: sysevent to mark as online
 *
 * Marks a sysevent as "online" without increasing the reference count
 * on the sysevent. This is typically used by sysevents that are already
 * online when the kernel boots up.
 */
void sysevent_default_online(struct sysevent_device *dev)
{
	sysevent_set_state(dev, SYSTEM_EVENT_ONLINE);
}
EXPORT_SYMBOL(sysevent_default_online);

static struct attribute *sysevent_attrs[] = {
	&dev_attr_name.attr,
	&dev_attr_state.attr,
	&dev_attr_crash_count.attr,
	&dev_attr_crash_reason.attr,
	&dev_attr_crash_timestamp.attr,
	&dev_attr_restart_level.attr,
	&dev_attr_firmware_name.attr,
	NULL,
};

ATTRIBUTE_GROUPS(sysevent);

struct bus_type sysevent_bus_type = {
	.name		= "exynos_sysevent",
	.dev_groups	= sysevent_groups,
};
EXPORT_SYMBOL(sysevent_bus_type);

static DEFINE_IDA(sysevent_ida);

static int enable_ramdumps;
module_param(enable_ramdumps, int, 0644);

struct workqueue_struct *sysevent_wq;
static struct class *char_class;

static LIST_HEAD(restart_log_list);
static LIST_HEAD(sysevent_list);
static DEFINE_MUTEX(restart_log_mutex);
static DEFINE_MUTEX(sysevent_list_lock);
static DEFINE_MUTEX(char_device_lock);

static int max_restarts;
module_param(max_restarts, int, 0644);

static long max_history_time = 3600;
module_param(max_history_time, long, 0644);

static void do_epoch_check(struct sysevent_device *dev)
{
	int n = 0;
	struct timeval *time_first = NULL, *curr_time;
	struct timespec64 now;
	struct restart_log *r_log, *temp;
	static int max_restarts_check;
	static long max_history_time_check;

	mutex_lock(&restart_log_mutex);

	max_restarts_check = max_restarts;
	max_history_time_check = max_history_time;

	/* Check if epoch checking is enabled */
	if (!max_restarts_check)
		goto out;

	r_log = kmalloc(sizeof(struct restart_log), GFP_KERNEL);
	if (!r_log)
		goto out;
	r_log->dev = dev;

	ktime_get_real_ts64(&now);
	r_log->time.tv_sec = now.tv_sec;
	r_log->time.tv_usec = now.tv_nsec / 1000;

	curr_time = &r_log->time;
	INIT_LIST_HEAD(&r_log->list);

	list_add_tail(&r_log->list, &restart_log_list);

	list_for_each_entry_safe(r_log, temp, &restart_log_list, list) {

		if ((curr_time->tv_sec - r_log->time.tv_sec) >
				max_history_time_check) {

			pr_debug("Deleted node with restart_time = %ld\n",
					r_log->time.tv_sec);
			list_del(&r_log->list);
			kfree(r_log);
			continue;
		}
		if (!n) {
			time_first = &r_log->time;
			pr_debug("Time_first: %ld\n", time_first->tv_sec);
		}
		n++;
		pr_debug("Restart_time: %ld\n", r_log->time.tv_sec);
	}

	if (time_first && n >= max_restarts_check) {
		if ((curr_time->tv_sec - time_first->tv_sec) <
				max_history_time_check)
			panic("sysevents have crashed %d times in less than %ld seconds!",
				max_restarts_check, max_history_time_check);
	}

out:
	mutex_unlock(&restart_log_mutex);
}

static int is_ramdump_enabled(struct sysevent_device *dev)
{
	return enable_ramdumps;
}

static int for_each_sysevent_device(struct sysevent_device **list,
		unsigned int count, void *data,
		int (*fn)(struct sysevent_device *, void *))
{
	int ret;

	while (count--) {
		struct sysevent_device *dev = *list++;

		if (!dev)
			continue;
		ret = fn(dev, data);
		if (ret)
			return ret;
	}
	return 0;
}

static void sysevent_notif_uevent(struct sysevent_desc *desc,
				enum sysevent_notif_type notif)
{
	char *envp[3];

	if (notif == SYSTEM_EVENT_AFTER_POWERUP) {
		envp[0] = kasprintf(GFP_KERNEL, "SYSTEM_EVENT=%s", desc->name);
		envp[1] = kasprintf(GFP_KERNEL, "NOTIFICATION=%d", notif);
		envp[2] = NULL;
		kobject_uevent_env(&desc->dev->kobj, KOBJ_CHANGE, envp);
		pr_debug("%s %s sent\n", envp[0], envp[1]);
		kfree(envp[1]);
		kfree(envp[0]);
	}
}

static void notify_each_sysevent_device(struct sysevent_device **list,
		unsigned int count,
		enum sysevent_notif_type notif, void *data)
{
	while (count--) {
		struct sysevent_device *dev = *list++;
		struct notif_data notif_data;
		struct platform_device *pdev;

		if (!dev)
			continue;

		pdev = container_of(dev->desc->dev, struct platform_device,
									dev);
		dev->notif_state = notif;

		notif_data.crashed = sysevent_get_crash_status(dev);
		notif_data.enable_ramdump = is_ramdump_enabled(dev);
		notif_data.no_auth = dev->desc->no_auth;
		notif_data.pdev = pdev;

		sysevent_notif_queue_notification(dev->notify, notif,
								&notif_data);
		if (0)
			sysevent_notif_uevent(dev->desc, notif);
	}
}

static void enable_all_irqs(struct sysevent_device *dev)
{
	if (dev->desc->watchdog_irq && dev->desc->watchdog_handler) {
		enable_irq(dev->desc->watchdog_irq);
		irq_set_irq_wake(dev->desc->watchdog_irq, 1);
	}
	if (dev->desc->err_fatal_irq && dev->desc->err_fatal_handler)
		enable_irq(dev->desc->err_fatal_irq);
	if (dev->desc->generic_irq && dev->desc->generic_handler) {
		enable_irq(dev->desc->generic_irq);
		irq_set_irq_wake(dev->desc->generic_irq, 1);
	}
}

static void disable_all_irqs(struct sysevent_device *dev)
{
	if (dev->desc->watchdog_irq && dev->desc->watchdog_handler) {
		disable_irq(dev->desc->watchdog_irq);
		irq_set_irq_wake(dev->desc->watchdog_irq, 0);
	}
	if (dev->desc->err_fatal_irq && dev->desc->err_fatal_handler)
		disable_irq(dev->desc->err_fatal_irq);
	if (dev->desc->generic_irq && dev->desc->generic_handler) {
		disable_irq(dev->desc->generic_irq);
		irq_set_irq_wake(dev->desc->generic_irq, 0);
	}
}

static int sysevent_shutdown(struct sysevent_device *dev, void *data)
{
	const char *name = dev->desc->name;
	char *timestamp = dev->desc->last_crash_timestamp;
	int ret;
	struct timespec ts_rtc;
	struct rtc_time tm;
	unsigned long flags;

	pr_info("[%s:%d]: Shutting down %s\n",
			current->comm, current->pid, name);
	ret = dev->desc->shutdown(dev->desc, true);
	if (ret < 0) {
		if (!dev->desc->ignore_sysevent_failure) {
			panic("sysevent-restart: [%s:%d]: Failed to shutdown %s!",
				current->comm, current->pid, name);
		} else {
			pr_err("Shutdown failure on %s\n", name);
			return ret;
		}
	}

	spin_lock_irqsave(&dev->desc->sysevent_sysfs_lock, flags);
	/* record crash time */
	getnstimeofday(&ts_rtc);
	rtc_time_to_tm(ts_rtc.tv_sec - (sys_tz.tz_minuteswest * 60), &tm);
	snprintf(timestamp, MAX_CRASH_TIMESTAMP_LEN,
			"%d-%02d-%02d_%02d-%02d-%02d",
			tm.tm_year + 1900, tm.tm_mon + 1, tm.tm_mday,
			tm.tm_hour, tm.tm_min, tm.tm_sec);
	spin_unlock_irqrestore(&dev->desc->sysevent_sysfs_lock, flags);

	dev->crash_count++;
	sysevent_set_state(dev, SYSTEM_EVENT_OFFLINE);
	disable_all_irqs(dev);

	return 0;
}

static int sysevent_ramdump(struct sysevent_device *dev, void *data)
{
	const char *name = dev->desc->name;

	if (dev->desc->ramdump)
		if (dev->desc->ramdump(is_ramdump_enabled(dev), dev->desc) < 0)
			pr_warn("%s[%s:%d]: Ramdump failed.\n",
				name, current->comm, current->pid);

	dev->do_ramdump_on_put = false;
	return 0;
}

static int sysevent_free_memory(struct sysevent_device *dev, void *data)
{
	if (dev->desc->free_memory)
		dev->desc->free_memory(dev->desc);
	return 0;
}

static int sysevent_powerup(struct sysevent_device *dev, void *data)
{
	const char *name = dev->desc->name;
	int ret;

	pr_info("[%s:%d]: Powering up %s\n", current->comm, current->pid, name);

	ret = dev->desc->powerup(dev->desc);
	if (ret < 0) {
		notify_each_sysevent_device(&dev, 1, SYSTEM_EVENT_POWERUP_FAILURE,
								NULL);
		if (system_state == SYSTEM_RESTART
			|| system_state == SYSTEM_POWER_OFF)
			WARN(1, "sysevent aborted: %s, system reboot/shutdown is under way\n",
				name);
		else {
			if (!dev->desc->ignore_sysevent_failure) {
				/*
				 * There is a slight window between reboot and
				 * system_state changing to SYSTEM_RESTART or
				 * SYSTEM_POWER_OFF. Add a delay before panic
				 * to ensure sysevent that happens during reboot
				 * will not result in a kernel panic.
				 */
				msleep(3000);
				if (system_state != SYSTEM_RESTART
					&& system_state != SYSTEM_POWER_OFF)
					panic("[%s:%d]: Powerup error: %s!",
						current->comm,
						current->pid, name);
			}
			pr_err("Powerup failure on %s\n", name);
		}
		return ret;
	}
	enable_all_irqs(dev);
	sysevent_set_state(dev, SYSTEM_EVENT_ONLINE);
	sysevent_set_crash_status(dev, CRASH_STATUS_NO_CRASH);

	return 0;
}

static int __find_sysevent_device(struct device *dev, const void *data)
{
	struct sysevent_device *sysevent = to_sysevent(dev);

	return !strcmp(sysevent->desc->name, data);
}

struct sysevent_device *find_sysevent_device(const char *str)
{
	struct device *dev;

	if (!str)
		return NULL;

	dev = bus_find_device(&sysevent_bus_type, NULL, (void *)str,
			__find_sysevent_device);
	return dev ? to_sysevent(dev) : NULL;
}
EXPORT_SYMBOL(find_sysevent_device);

static int sysevent_start(struct sysevent_device *sysevent)
{
	int ret;

	notify_each_sysevent_device(&sysevent, 1, SYSTEM_EVENT_BEFORE_POWERUP,
								NULL);

	ret = sysevent->desc->powerup(sysevent->desc);
	if (ret) {
		notify_each_sysevent_device(&sysevent, 1, SYSTEM_EVENT_POWERUP_FAILURE,
									NULL);
		return ret;
	}
	enable_all_irqs(sysevent);

	sysevent_set_state(sysevent, SYSTEM_EVENT_ONLINE);
	sysevent_set_crash_status(sysevent, CRASH_STATUS_NO_CRASH);

	notify_each_sysevent_device(&sysevent, 1, SYSTEM_EVENT_AFTER_POWERUP,
								NULL);
	return ret;
}

static void sysevent_stop(struct sysevent_device *sysevent)
{
	notify_each_sysevent_device(&sysevent, 1, SYSTEM_EVENT_BEFORE_SHUTDOWN, NULL);
	sysevent->desc->shutdown(sysevent->desc, false);
	sysevent_set_state(sysevent, SYSTEM_EVENT_OFFLINE);
	disable_all_irqs(sysevent);
	notify_each_sysevent_device(&sysevent, 1, SYSTEM_EVENT_AFTER_SHUTDOWN, NULL);
}

int sysevent_set_fwname(const char *name, const char *fw_name)
{
	struct sysevent_device *sysevent;

	if (!name)
		return -EINVAL;

	if (!fw_name)
		return -EINVAL;

	sysevent = find_sysevent_device(name);
	if (!sysevent)
		return -EINVAL;

	pr_debug("Changing sysevent [%s] fw_name to [%s]\n", name, fw_name);
	strlcpy(sysevent->desc->fw_name, fw_name,
		sizeof(sysevent->desc->fw_name));

	return 0;
}
EXPORT_SYMBOL(sysevent_set_fwname);

void *__sysevent_get(const char *name, const char *fw_name)
{
	struct sysevent_device *sysevent;
	int ret;
	void *retval;
	struct sysevent_tracking *track;

	if (!name)
		return NULL;

	sysevent = retval = find_sysevent_device(name);
	if (!sysevent)
		return ERR_PTR(-ENODEV);
	if (!try_module_get(sysevent->owner)) {
		retval = ERR_PTR(-ENODEV);
		goto err_module;
	}

	track = sysevent_get_track(sysevent);
	mutex_lock(&track->lock);
	if (!sysevent->count) {
		if (fw_name) {
			pr_info("Changing sysevent fw_name to %s\n", fw_name);
			strlcpy(sysevent->desc->fw_name, fw_name,
				sizeof(sysevent->desc->fw_name));
		}
		ret = sysevent_start(sysevent);
		if (ret) {
			retval = ERR_PTR(ret);
			goto err_start;
		}
	}
	sysevent->count++;
	mutex_unlock(&track->lock);
	return retval;
err_start:
	mutex_unlock(&track->lock);
	module_put(sysevent->owner);
err_module:
	put_device(&sysevent->dev);
	return retval;
}

/**
 * subsytem_get() - Boot a sysevent
 * @name: pointer to a string containing the name of the sysevent to boot
 *
 * This function returns a pointer if it succeeds. If an error occurs an
 * ERR_PTR is returned.
 *
 * If this feature is disable, the value %NULL will be returned.
 */
void *sysevent_get(const char *name)
{
	return __sysevent_get(name, NULL);
}
EXPORT_SYMBOL(sysevent_get);

/**
 * sysevent_get_with_fwname() - Boot a sysevent using the firmware name passed
 * @name: pointer to a string containing the name of the sysevent to boot
 * @fw_name: pointer to a string containing the sysevent firmware image name
 *
 * This function returns a pointer if it succeeds. If an error occurs an
 * ERR_PTR is returned.
 *
 * If this feature is disable, the value %NULL will be returned.
 */
void *sysevent_get_with_fwname(const char *name, const char *fw_name)
{
	return __sysevent_get(name, fw_name);
}
EXPORT_SYMBOL(sysevent_get_with_fwname);

/**
 * sysevent_put() - Shutdown a sysevent
 * @peripheral_handle: pointer from a previous call to sysevent_get()
 *
 * This doesn't imply that a sysevent is shutdown until all callers of
 * sysevent_get() have called sysevent_put().
 */
void sysevent_put(void *sys)
{
	struct sysevent_device *sysevent = sys;
	struct sysevent_tracking *track;

	if (IS_ERR_OR_NULL(sysevent))
		return;

	track = sysevent_get_track(sysevent);
	mutex_lock(&track->lock);
	if (WARN(!sysevent->count, "%s: %s: Reference count mismatch\n",
			sysevent->desc->name, __func__))
		goto err_out;
	if (!--sysevent->count) {
		sysevent_stop(sysevent);
		if (sysevent->do_ramdump_on_put)
			sysevent_ramdump(sysevent, NULL);
		sysevent_free_memory(sysevent, NULL);
	}
	mutex_unlock(&track->lock);

	module_put(sysevent->owner);
	put_device(&sysevent->dev);
	return;
err_out:
	mutex_unlock(&track->lock);
}
EXPORT_SYMBOL(sysevent_put);

static void sysevent_restart_wq_func(struct work_struct *work)
{
	struct sysevent_device *dev = container_of(work,
						struct sysevent_device, work);
	struct sysevent_device **list;
	struct sysevent_desc *desc = dev->desc;
	struct sysevent_tracking *track;
	unsigned int count;
	unsigned long flags;
	int ret;

	/*
	 * It's OK to not take the registration lock at this point.
	 * This is because the sysevent list inside the relevant
	 * restart order is not being traversed.
	 */
	list = &dev;
	count = 1;
	track = &dev->track;

	/*
	 * If a system reboot/shutdown is under way, ignore sysevent errors.
	 * However, print a message so that we know that a sysevent behaved
	 * unexpectedly here.
	 */
	if (system_state == SYSTEM_RESTART
		|| system_state == SYSTEM_POWER_OFF) {
		WARN(1, "sysevent aborted: %s, system reboot/shutdown is under way\n",
			desc->name);
		return;
	}

	mutex_lock(&track->lock);
	do_epoch_check(dev);

	if (dev->track.state == SYSTEM_EVENT_OFFLINE) {
		mutex_unlock(&track->lock);
		WARN(1, "sysevent aborted: %s sysevent not online\n", desc->name);
		return;
	}

	/*
	 * It's necessary to take the registration lock because the sysevent
	 * list in the SoC restart order will be traversed and it shouldn't be
	 * changed until _this_ restart sequence completes.
	 */
	pr_debug("[%s:%d]: Starting restart sequence for %s\n",
			current->comm, current->pid, desc->name);
	notify_each_sysevent_device(list, count, SYSTEM_EVENT_BEFORE_SHUTDOWN, NULL);
	ret = for_each_sysevent_device(list, count, NULL, sysevent_shutdown);
	if (ret)
		goto err;
	notify_each_sysevent_device(list, count, SYSTEM_EVENT_AFTER_SHUTDOWN, NULL);

	notify_each_sysevent_device(list, count, SYSTEM_EVENT_RAMDUMP_NOTIFICATION,
									NULL);

	spin_lock_irqsave(&track->s_lock, flags);
	track->p_state = SYSTEM_EVENT_RESTARTING;
	spin_unlock_irqrestore(&track->s_lock, flags);

	/* Collect ram dumps for all sysevents in order here */
	for_each_sysevent_device(list, count, NULL, sysevent_ramdump);

	for_each_sysevent_device(list, count, NULL, sysevent_free_memory);

	notify_each_sysevent_device(list, count, SYSTEM_EVENT_BEFORE_POWERUP, NULL);
	ret = for_each_sysevent_device(list, count, NULL, sysevent_powerup);
	if (ret)
		goto err;
	notify_each_sysevent_device(list, count, SYSTEM_EVENT_AFTER_POWERUP, NULL);

	pr_info("[%s:%d]: Restart sequence for %s completed.\n",
			current->comm, current->pid, desc->name);

err:
	/* Reset sysevent count */
	if (ret)
		dev->count = 0;

	mutex_unlock(&track->lock);

	spin_lock_irqsave(&track->s_lock, flags);
	track->p_state = SYSTEM_EVENT_NORMAL;
	__pm_relax(dev->sysevent_wlock);
	spin_unlock_irqrestore(&track->s_lock, flags);
}

static void __sysevent_restart_dev(struct sysevent_device *dev)
{
	struct sysevent_desc *desc = dev->desc;
	const char *name = dev->desc->name;
	struct sysevent_tracking *track;
	unsigned long flags;

	pr_debug("Restarting %s [level=%s]!\n", desc->name,
			restart_levels[dev->restart_level]);

	track = sysevent_get_track(dev);
	/*
	 * Allow drivers to call sysevent_restart{_dev}() as many times as
	 * they want up until the point where the sysevent is shutdown.
	 */
	spin_lock_irqsave(&track->s_lock, flags);
	if (track->p_state != SYSTEM_EVENT_CRASHED &&
					dev->track.state == SYSTEM_EVENT_ONLINE) {
		if (track->p_state != SYSTEM_EVENT_RESTARTING) {
			track->p_state = SYSTEM_EVENT_CRASHED;
			__pm_stay_awake(dev->sysevent_wlock);
			queue_work(sysevent_wq, &dev->work);
		} else {
			panic("sysevent %s crashed during sysevent!", name);
		}
	} else
		WARN(dev->track.state == SYSTEM_EVENT_OFFLINE,
			"sysevent aborted: %s sysevent not online\n", name);
	spin_unlock_irqrestore(&track->s_lock, flags);
}

static void device_restart_work_hdlr(struct work_struct *work)
{
	struct sysevent_device *dev = container_of(work, struct sysevent_device,
							device_restart_work);

	notify_each_sysevent_device(&dev, 1, SYSTEM_EVENT_SOC_RESET, NULL);
	/*
	 * Temporary workaround until ramdump userspace application calls
	 * sync() and fclose() on atpting the dump.
	 */
	msleep(100);
	panic("sysevent-restart: Resetting the SoC - %s crashed.",
							dev->desc->name);
}

int sysevent_restart_dev(struct sysevent_device *dev)
{
	const char *name;

	if (!get_device(&dev->dev))
		return -ENODEV;

	if (!try_module_get(dev->owner)) {
		put_device(&dev->dev);
		return -ENODEV;
	}

	name = dev->desc->name;

	send_early_notifications(dev->early_notify);

	/*
	 * If a system reboot/shutdown is underway, ignore sysevent errors.
	 * However, print a message so that we know that a sysevent behaved
	 * unexpectedly here.
	 */
	if (system_state == SYSTEM_RESTART
		|| system_state == SYSTEM_POWER_OFF) {
		pr_err("%s crashed during a system poweroff/shutdown.\n", name);
		return -EBUSY;
	}

	pr_info("Restart sequence requested for %s, restart_level = %s.\n",
		name, restart_levels[dev->restart_level]);


	/* TODO - If sysevent is not support, This function must be needed */
	if (disable_sysevent_support == true) {
		pr_warn("sysevent-restart: Ignoring restart request for %s\n",
									name);
		return 0;
	}

	/* TODO - modify for exynos */
	switch (dev->restart_level) {

	case RESET_SYSTEM_EVENT_COUPLED:
		__sysevent_restart_dev(dev);
		break;
	case RESET_SOC:
		__pm_stay_awake(dev->sysevent_wlock);
		schedule_work(&dev->device_restart_work);
		return 0;
	default:
		panic("sysevent-restart: Unknown restart level!\n");
		break;
	}
	/* Till here */

	module_put(dev->owner);
	put_device(&dev->dev);

	return 0;
}
EXPORT_SYMBOL(sysevent_restart_dev);

int sysevent_restart(const char *name)
{
	int ret;
	struct sysevent_device *dev = find_sysevent_device(name);

	if (!dev)
		return -ENODEV;

	ret = sysevent_restart_dev(dev);
	put_device(&dev->dev);
	return ret;
}
EXPORT_SYMBOL(sysevent_restart);

int sysevent_crashed(const char *name)
{
	struct sysevent_device *dev = find_sysevent_device(name);
	struct sysevent_tracking *track;

	if (!dev)
		return -ENODEV;

	if (!get_device(&dev->dev))
		return -ENODEV;

	track = sysevent_get_track(dev);

	mutex_lock(&track->lock);
	dev->do_ramdump_on_put = true;
	/*
	 * TODO: Make this work with multiple consumers where one is calling
	 * sysevent_restart() and another is calling this function. To do
	 * so would require updating private state, etc.
	 */
	mutex_unlock(&track->lock);

	put_device(&dev->dev);
	return 0;
}
EXPORT_SYMBOL(sysevent_crashed);

void sysevent_set_crash_status(struct sysevent_device *dev,
				enum crash_status crashed)
{
	dev->crashed = crashed;
}
EXPORT_SYMBOL(sysevent_set_crash_status);

enum crash_status sysevent_get_crash_status(struct sysevent_device *dev)
{
	return dev->crashed;
}

static int sysevent_device_open(struct inode *inode, struct file *file)
{
	struct sysevent_device *device, *sysevent_dev = 0;
	void *retval;

	mutex_lock(&sysevent_list_lock);
	list_for_each_entry(device, &sysevent_list, list)
		if (MINOR(device->dev_no) == iminor(inode))
			sysevent_dev = device;
	mutex_unlock(&sysevent_list_lock);

	if (!sysevent_dev)
		return -EINVAL;

	retval = sysevent_get_with_fwname(sysevent_dev->desc->name,
					sysevent_dev->desc->fw_name);
	if (IS_ERR(retval))
		return PTR_ERR(retval);

	return 0;
}

static int sysevent_device_close(struct inode *inode, struct file *file)
{
	struct sysevent_device *device, *sysevent_dev = 0;

	mutex_lock(&sysevent_list_lock);
	list_for_each_entry(device, &sysevent_list, list)
		if (MINOR(device->dev_no) == iminor(inode))
			sysevent_dev = device;
	mutex_unlock(&sysevent_list_lock);

	if (!sysevent_dev)
		return -EINVAL;

	sysevent_put(sysevent_dev);
	return 0;
}

static const struct file_operations sysevent_device_fops = {
		.owner = THIS_MODULE,
		.open = sysevent_device_open,
		.release = sysevent_device_close,
};

static void sysevent_device_release(struct device *dev)
{
	struct sysevent_device *sysevent = to_sysevent(dev);

	wakeup_source_unregister(sysevent->sysevent_wlock);

	mutex_destroy(&sysevent->track.lock);
	ida_simple_remove(&sysevent_ida, sysevent->id);
	kfree(sysevent);
}

static int sysevent_char_device_add(struct sysevent_device *sysevent_dev)
{
	int ret = 0;
	static int major, minor;
	dev_t dev_no;

	mutex_lock(&char_device_lock);
	if (!major) {
		ret = alloc_chrdev_region(&dev_no, 0, 4, "sysevent");
		if (ret < 0) {
			pr_err("Failed to alloc sysevent_dev region, err %d\n",
									ret);
			goto fail;
		}
		major = MAJOR(dev_no);
		minor = MINOR(dev_no);
	} else
		dev_no = MKDEV(major, minor);

	if (!device_create(char_class, sysevent_dev->desc->dev, dev_no,
			NULL, "sysevent_%s", sysevent_dev->desc->name)) {
		pr_err("Failed to create sysevent_%s device\n",
						sysevent_dev->desc->name);
		goto fail_unregister_cdev_region;
	}

	cdev_init(&sysevent_dev->char_dev, &sysevent_device_fops);
	sysevent_dev->char_dev.owner = THIS_MODULE;
	ret = cdev_add(&sysevent_dev->char_dev, dev_no, 1);
	if (ret < 0)
		goto fail_destroy_device;

	sysevent_dev->dev_no = dev_no;
	minor++;
	mutex_unlock(&char_device_lock);

	return 0;

fail_destroy_device:
	device_destroy(char_class, dev_no);
fail_unregister_cdev_region:
	unregister_chrdev_region(dev_no, 1);
fail:
	mutex_unlock(&char_device_lock);
	return ret;
}

static void sysevent_char_device_remove(struct sysevent_device *sysevent_dev)
{
	cdev_del(&sysevent_dev->char_dev);
	device_destroy(char_class, sysevent_dev->dev_no);
	unregister_chrdev_region(sysevent_dev->dev_no, 1);
}

static int __get_irq(struct sysevent_desc *desc, const char *prop,
		unsigned int *irq)
{
	int irql = 0;
	struct device_node *dnode = desc->dev->of_node;

	if (of_property_match_string(dnode, "interrupt-names", prop) < 0)
		return -ENOENT;

	irql = of_irq_get_byname(dnode, prop);
	if (irql < 0) {
		pr_err("[%s]: Error getting IRQ \"%s\"\n", desc->name,
		prop);
		return irql;
	}
	*irq = irql;
	return 0;
}

static int sysevent_parse_devicetree(struct sysevent_desc *desc)
{
	int ret;
	struct platform_device *pdev = container_of(desc->dev,
					struct platform_device, dev);

	ret = __get_irq(desc, "samsung,err-fatal", &desc->err_fatal_irq);
	if (ret && ret != -ENOENT)
		return ret;

	ret = __get_irq(desc, "samsung,watchdog", &desc->watchdog_irq);
	if (ret && ret != -ENOENT)
		return ret;

	if (of_property_read_bool(pdev->dev.of_node,
					"samsung,sysevent-generic-irq-handler")) {
		ret = platform_get_irq(pdev, 0);
		if (ret > 0)
			desc->generic_irq = ret;
	}

	desc->ignore_sysevent_failure = of_property_read_bool(pdev->dev.of_node,
						"samsung,ignore-sysevent-failure");

	return 0;
}

static int sysevent_setup_irqs(struct sysevent_device *sysevent)
{
	struct sysevent_desc *desc = sysevent->desc;
	int ret;

	if (desc->err_fatal_irq && desc->err_fatal_handler) {
		ret = devm_request_threaded_irq(desc->dev, desc->err_fatal_irq,
				NULL,
				desc->err_fatal_handler,
				IRQF_TRIGGER_RISING, desc->name, desc);
		if (ret < 0) {
			dev_err(desc->dev, "[%s]: Unable to register error fatal IRQ handler: %d, irq is %d\n",
				desc->name, ret, desc->err_fatal_irq);
			return ret;
		}
		disable_irq(desc->err_fatal_irq);
	}

	if (desc->watchdog_irq && desc->watchdog_handler) {
		ret = devm_request_irq(desc->dev, desc->watchdog_irq,
			desc->watchdog_handler,
			IRQF_TRIGGER_RISING, desc->name, desc);
		if (ret < 0) {
			dev_err(desc->dev, "[%s]: Unable to register watchdog bite handler: %d\n",
				desc->name, ret);
			return ret;
		}
		disable_irq(desc->watchdog_irq);
	}

	if (desc->generic_irq && desc->generic_handler) {
		ret = devm_request_irq(desc->dev, desc->generic_irq,
			desc->generic_handler,
			IRQF_TRIGGER_HIGH, desc->name, desc);
		if (ret < 0) {
			dev_err(desc->dev, "[%s]: Unable to register generic irq handler: %d\n",
				desc->name, ret);
			return ret;
		}
		disable_irq(desc->generic_irq);
	}

	return 0;
}

static void sysevent_free_irqs(struct sysevent_device *sysevent)
{
	struct sysevent_desc *desc = sysevent->desc;

	if (desc->err_fatal_irq && desc->err_fatal_handler)
		devm_free_irq(desc->dev, desc->err_fatal_irq, desc);
	if (desc->watchdog_irq && desc->watchdog_handler)
		devm_free_irq(desc->dev, desc->watchdog_irq, desc);
}

struct sysevent_device *sysevent_register(struct sysevent_desc *desc)
{
	struct sysevent_device *sysevent;
	struct device_node *ofnode = desc->dev->of_node;
	int ret;

	sysevent = kzalloc(sizeof(*sysevent), GFP_KERNEL);
	if (!sysevent)
		return ERR_PTR(-ENOMEM);

	sysevent->desc = desc;
	sysevent->owner = desc->owner;
	sysevent->dev.parent = desc->dev;
	sysevent->dev.bus = &sysevent_bus_type;
	sysevent->dev.release = sysevent_device_release;
	sysevent->notif_state = -1;
	strlcpy(sysevent->desc->fw_name, desc->name,
			sizeof(sysevent->desc->fw_name));

	sysevent->notify = sysevent_notif_add_sysevent(desc->name);
	sysevent->early_notify = sysevent_get_early_notif_info(desc->name);

	snprintf(sysevent->wlname, sizeof(sysevent->wlname), "sysevent(%s)", desc->name);
	sysevent->sysevent_wlock = wakeup_source_register(&sysevent->dev, sysevent->wlname);

	INIT_WORK(&sysevent->work, sysevent_restart_wq_func);
	INIT_WORK(&sysevent->device_restart_work, device_restart_work_hdlr);
	spin_lock_init(&sysevent->track.s_lock);
	spin_lock_init(&sysevent->desc->sysevent_sysfs_lock);

	sysevent->id = ida_simple_get(&sysevent_ida, 0, 0, GFP_KERNEL);
	if (sysevent->id < 0) {
		ret = sysevent->id;
		kfree(sysevent);
		return ERR_PTR(ret);
	}

	dev_set_name(&sysevent->dev, "sysevent%d", sysevent->id);

	mutex_init(&sysevent->track.lock);

	ret = device_register(&sysevent->dev);
	if (ret) {
		put_device(&sysevent->dev);
		return ERR_PTR(ret);
	}

	ret = sysevent_char_device_add(sysevent);
	if (ret)
		goto err_register;

	if (ofnode) {
		ret = sysevent_parse_devicetree(desc);
		if (ret)
			goto err_register;
	}

	mutex_lock(&sysevent_list_lock);
	INIT_LIST_HEAD(&sysevent->list);
	list_add_tail(&sysevent->list, &sysevent_list);
	mutex_unlock(&sysevent_list_lock);

	if (ofnode) {
		ret = sysevent_setup_irqs(sysevent);
		if (ret < 0)
			goto err_setup_irqs;
	}

	return sysevent;
err_setup_irqs:
err_register:
	device_unregister(&sysevent->dev);
	return ERR_PTR(ret);
}
EXPORT_SYMBOL(sysevent_register);

void sysevent_unregister(struct sysevent_device *sysevent)
{
	struct sysevent_device *sysevent_dev, *tmp;
	struct device_node *device = sysevent->desc->dev->of_node;

	if (IS_ERR_OR_NULL(sysevent))
		return;

	if (get_device(&sysevent->dev)) {
		mutex_lock(&sysevent_list_lock);
		list_for_each_entry_safe(sysevent_dev, tmp, &sysevent_list, list)
			if (sysevent_dev == sysevent)
				list_del(&sysevent->list);
		mutex_unlock(&sysevent_list_lock);

		if (device)
			sysevent_free_irqs(sysevent);

		mutex_lock(&sysevent->track.lock);
		WARN_ON(sysevent->count);
		device_unregister(&sysevent->dev);
		mutex_unlock(&sysevent->track.lock);
		sysevent_char_device_remove(sysevent);
		put_device(&sysevent->dev);
	}
}
EXPORT_SYMBOL(sysevent_unregister);

static int sysevent_panic(struct device *dev, void *data)
{
	struct sysevent_device *sysevent = to_sysevent(dev);

	if (sysevent->desc->crash_shutdown)
		sysevent->desc->crash_shutdown(sysevent->desc);
	return 0;
}

static int sysevent_panic_handler(struct notifier_block *this,
				unsigned long event, void *ptr)
{
	bus_for_each_dev(&sysevent_bus_type, NULL, NULL, sysevent_panic);
	return NOTIFY_DONE;
}

static struct notifier_block panic_nb = {
	.notifier_call  = sysevent_panic_handler,
};

static int __init sysevent_init(void)
{
	int ret;

	sysevent_wq = alloc_workqueue("sysevent_wq",
			WQ_UNBOUND | WQ_HIGHPRI | WQ_CPU_INTENSIVE, 0);
	BUG_ON(!sysevent_wq);

	ret = bus_register(&sysevent_bus_type);
	if (ret)
		goto err_bus;

	char_class = class_create(THIS_MODULE, "sysevent");
	if (IS_ERR(char_class)) {
		ret = -ENOMEM;
		pr_err("Failed to create sysevent_dev class\n");
		goto err_class;
	}

	ret = atomic_notifier_chain_register(&panic_notifier_list,
						&panic_nb);
	if (ret)
		goto err_soc;

	return 0;

err_soc:
	class_destroy(char_class);
err_class:
	bus_unregister(&sysevent_bus_type);
err_bus:
	destroy_workqueue(sysevent_wq);
	return ret;
}
arch_initcall(sysevent_init);

MODULE_AUTHOR("Hosung Kim <hosung0.kim@samsung.com>");
MODULE_AUTHOR("Changki Kim <changki.kim@samsung.com>");
MODULE_AUTHOR("Donghyeok Choe <d7271.choe@samsung.com>");
MODULE_DESCRIPTION("System Event Framework Driver");
MODULE_LICENSE("GPL v2");
