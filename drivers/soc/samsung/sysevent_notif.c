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

#include <linux/notifier.h>
#include <linux/init.h>
#include <linux/debugfs.h>
#include <linux/module.h>
#include <linux/workqueue.h>
#include <linux/stringify.h>
#include <linux/delay.h>
#include <linux/slab.h>
#include <soc/samsung/sysevent_notif.h>

/**
 * The callbacks that are registered in this data structure as early
 * notification callbacks will be called as soon as the SSR framework is
 * informed that the syseventtem has crashed. This means that these functions will
 * be invoked as part of an IRQ handler, and thus, will be called in an atomic
 * context. Therefore, functions that are registered as early notification
 * callback must obey to the same constraints as interrupt handlers
 * (i.e. these functions must not sleep or block, etc).
 */
struct sysevent_early_notif_info {
	spinlock_t cb_lock;
	void (*early_notif_cb[NUM_EARLY_NOTIFS])(void *);
	void *data[NUM_EARLY_NOTIFS];
};

struct sysevent_notif_info {
	char name[50];
	struct srcu_notifier_head sysevent_notif_rcvr_list;
	struct sysevent_early_notif_info early_notif_info;
	struct list_head list;
};

static LIST_HEAD(syseventtem_list);
static DEFINE_MUTEX(notif_lock);
static DEFINE_MUTEX(notif_add_lock);

#if defined(sysevent_RESTART_DEBUG)
static void sysevent_notif_reg_test_notifier(const char *);
#endif

static struct sysevent_notif_info *_notif_find_sysevent(const char *sysevent_name)
{
	struct sysevent_notif_info *sysevent;

	mutex_lock(&notif_lock);
	list_for_each_entry(sysevent, &syseventtem_list, list)
		if (!strcmp(sysevent->name, sysevent_name)) {
			mutex_unlock(&notif_lock);
			return sysevent;
		}
	mutex_unlock(&notif_lock);

	return NULL;
}

void *sysevent_notif_register_notifier(
			const char *sysevent_name, struct notifier_block *nb)
{
	int ret;
	struct sysevent_notif_info *sysevent = _notif_find_sysevent(sysevent_name);

	if (!sysevent) {

		/* Possible first time reference to this syseventtem. Add it. */
		sysevent = (struct sysevent_notif_info *)
				sysevent_notif_add_sysevent(sysevent_name);

		if (!sysevent)
			return ERR_PTR(-EINVAL);
	}

	ret = srcu_notifier_chain_register(
		&sysevent->sysevent_notif_rcvr_list, nb);

	if (ret < 0)
		return ERR_PTR(ret);

	return sysevent;
}
EXPORT_SYMBOL(sysevent_notif_register_notifier);

int sysevent_notif_unregister_notifier(void *sysevent_handle,
				struct notifier_block *nb)
{
	int ret;
	struct sysevent_notif_info *sysevent =
			(struct sysevent_notif_info *)sysevent_handle;

	if (!sysevent)
		return -EINVAL;

	ret = srcu_notifier_chain_unregister(
		&sysevent->sysevent_notif_rcvr_list, nb);

	return ret;
}
EXPORT_SYMBOL(sysevent_notif_unregister_notifier);

void send_early_notifications(void *early_notif_handle)
{
	struct sysevent_early_notif_info *early_info = early_notif_handle;
	unsigned long flags;
	unsigned int i;
	void (*notif_cb)(void *);

	if (!early_notif_handle)
		return;

	spin_lock_irqsave(&early_info->cb_lock, flags);
	for (i = 0; i < NUM_EARLY_NOTIFS; i++) {
		notif_cb = early_info->early_notif_cb[i];
		if (notif_cb)
			notif_cb(early_info->data[i]);
	}
	spin_unlock_irqrestore(&early_info->cb_lock, flags);
}
EXPORT_SYMBOL(send_early_notifications);

static bool valid_early_notif(enum early_sysevent_notif_type notif_type)
{
	return  notif_type >= 0 && notif_type < NUM_EARLY_NOTIFS;
}

/**
 * The early_notif_cb parameter must point to a function that conforms to the
 * same constraints placed upon interrupt handlers, as the function will be
 * called in an atomic context (i.e. these functions must not sleep or block).
 */
int sysevent_register_early_notifier(const char *sysevent_name,
				   enum early_sysevent_notif_type notif_type,
				   void (*early_notif_cb)(void *), void *data)
{
	struct sysevent_notif_info *sysevent;
	struct sysevent_early_notif_info *early_notif_info;
	unsigned long flags;
	int rc = 0;

	if (!sysevent_name || !early_notif_cb || !valid_early_notif(notif_type))
		return -EINVAL;

	sysevent = _notif_find_sysevent(sysevent_name);
	if (!sysevent)
		return -EINVAL;

	early_notif_info = &sysevent->early_notif_info;
	spin_lock_irqsave(&early_notif_info->cb_lock, flags);
	if (early_notif_info->early_notif_cb[notif_type]) {
		rc = -EEXIST;
		goto out;
	}
	early_notif_info->early_notif_cb[notif_type] = early_notif_cb;
	early_notif_info->data[notif_type] = data;
out:
	spin_unlock_irqrestore(&early_notif_info->cb_lock, flags);
	return rc;
}
EXPORT_SYMBOL(sysevent_register_early_notifier);

int sysevent_unregister_early_notifier(const char *sysevent_name, enum
				     early_sysevent_notif_type notif_type)
{
	struct sysevent_notif_info *sysevent;
	struct sysevent_early_notif_info *early_notif_info;
	unsigned long flags;

	if (!sysevent_name || !valid_early_notif(notif_type))
		return -EINVAL;

	sysevent = _notif_find_sysevent(sysevent_name);
	if (!sysevent)
		return -EINVAL;

	early_notif_info = &sysevent->early_notif_info;
	spin_lock_irqsave(&early_notif_info->cb_lock, flags);
	early_notif_info->early_notif_cb[notif_type] = NULL;
	early_notif_info->data[notif_type] = NULL;
	spin_unlock_irqrestore(&early_notif_info->cb_lock, flags);
	return 0;
}
EXPORT_SYMBOL(sysevent_unregister_early_notifier);

void *sysevent_get_early_notif_info(const char *sysevent_name)
{
	struct sysevent_notif_info *sysevent;

	if (!sysevent_name)
		return ERR_PTR(-EINVAL);

	sysevent = _notif_find_sysevent(sysevent_name);

	if (!sysevent)
		return ERR_PTR(-EINVAL);

	return &sysevent->early_notif_info;
}
EXPORT_SYMBOL(sysevent_get_early_notif_info);

void *sysevent_notif_add_sysevent(const char *sysevent_name)
{
	struct sysevent_notif_info *sysevent = NULL;

	if (!sysevent_name)
		goto done;

	mutex_lock(&notif_add_lock);

	sysevent = _notif_find_sysevent(sysevent_name);

	if (sysevent) {
		mutex_unlock(&notif_add_lock);
		goto done;
	}

	sysevent = kmalloc(sizeof(struct sysevent_notif_info), GFP_KERNEL);

	if (!sysevent) {
		mutex_unlock(&notif_add_lock);
		return ERR_PTR(-EINVAL);
	}

	strlcpy(sysevent->name, sysevent_name, ARRAY_SIZE(sysevent->name));

	srcu_init_notifier_head(&sysevent->sysevent_notif_rcvr_list);

	memset(&sysevent->early_notif_info, 0, sizeof(struct
						    sysevent_early_notif_info));
	spin_lock_init(&sysevent->early_notif_info.cb_lock);
	INIT_LIST_HEAD(&sysevent->list);

	mutex_lock(&notif_lock);
	list_add_tail(&sysevent->list, &syseventtem_list);
	mutex_unlock(&notif_lock);

	#if defined(SYSTEM_EVENT_DEBUG)
	sysevent_notif_reg_test_notifier(sysevent->name);
	#endif

	mutex_unlock(&notif_add_lock);

done:
	return sysevent;
}
EXPORT_SYMBOL(sysevent_notif_add_sysevent);

int sysevent_notif_queue_notification(void *sysevent_handle,
					enum sysevent_notif_type notif_type,
					void *data)
{
	struct sysevent_notif_info *sysevent = sysevent_handle;

	if (!sysevent)
		return -EINVAL;

	if (notif_type < 0 || notif_type >= SYSTEM_EVENT_NOTIF_TYPE_COUNT)
		return -EINVAL;

	return srcu_notifier_call_chain(&sysevent->sysevent_notif_rcvr_list,
				       notif_type, data);
}
EXPORT_SYMBOL(sysevent_notif_queue_notification);

#if defined(SYSTEM_EVENT_DEBUG)
static const char *notif_to_string(enum sysevent_notif_type notif_type)
{
	switch (notif_type) {

	case	SYSTEM_EVENT_BEFORE_SHUTDOWN:
		return __stringify(SYSTEM_EVENT_BEFORE_SHUTDOWN);

	case	SYSTEM_EVENT_AFTER_SHUTDOWN:
		return __stringify(SYSTEM_EVENT_AFTER_SHUTDOWN);

	case	SYSTEM_EVENT_BEFORE_POWERUP:
		return __stringify(SYSTEM_EVENT_BEFORE_POWERUP);

	case	SYSTEM_EVENT_AFTER_POWERUP:
		return __stringify(SYSTEM_EVENT_AFTER_POWERUP);

	default:
		return "unknown";
	}
}

static int sysevent_notifier_test_call(struct notifier_block *this,
				  unsigned long code,
				  void *data)
{
	switch (code) {

	default:
		pr_warn("%s: Notification %s from syseventtem %pK\n",
			__func__, notif_to_string(code), data);
	break;

	}

	return NOTIFY_DONE;
}

static struct notifier_block nb = {
	.notifier_call = sysevent_notifier_test_call,
};

static void sysevent_notif_reg_test_notifier(const char *sysevent_name)
{
	void *handle = sysevent_notif_register_notifier(sysevent_name, &nb);

	pr_warn("%s: Registered test notifier, handle=%pK",
			__func__, handle);
}
#endif

MODULE_AUTHOR("Hosung Kim <hosung0.kim@samsung.com>");
MODULE_AUTHOR("Changki Kim <changki.kim@samsung.com>");
MODULE_AUTHOR("Donghyeok Choe <d7271.choe@samsung.com>");
MODULE_DESCRIPTION("System Event Framework Notifier");
MODULE_LICENSE("GPL v2");
