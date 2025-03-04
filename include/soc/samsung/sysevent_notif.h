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

#ifndef _SYSTEM_EVENT_NOTIFIER_H
#define _SYSTEM_EVENT_NOTIFIER_H

#include <linux/notifier.h>

enum sysevent_notif_type {
	SYSTEM_EVENT_BEFORE_SHUTDOWN,
	SYSTEM_EVENT_AFTER_SHUTDOWN,
	SYSTEM_EVENT_BEFORE_POWERUP,
	SYSTEM_EVENT_AFTER_POWERUP,
	SYSTEM_EVENT_RAMDUMP_NOTIFICATION,
	SYSTEM_EVENT_POWERUP_FAILURE,
	SYSTEM_EVENT_SOC_RESET,
	SYSTEM_EVENT_NOTIF_TYPE_COUNT
};

enum early_sysevent_notif_type {
	SYSTEM_EVENT_EARLY_NORMAL,
	NUM_EARLY_NOTIFS
};

#if IS_ENABLED(CONFIG_EXYNOS_SYSTEM_EVENT)
/* Use the sysevent_notif_register_notifier API to register for notifications for
 * a particular syseventtem. This API will return a handle that can be used to
 * un-reg for notifications using the sysevent_notif_unregister_notifier API by
 * passing in that handle as an argument.
 *
 * On receiving a notification, the second (unsigned long) argument of the
 * notifier callback will contain the notification type, and the third (void *)
 * argument will contain the handle that was returned by
 * sysevent_notif_register_notifier.
 */
void *sysevent_notif_register_notifier(
			const char *sysevent_name, struct notifier_block *nb);
int sysevent_notif_unregister_notifier(void *sysevent_handle,
				struct notifier_block *nb);

/* Use the sysevent_notif_init_sysevent API to initialize the notifier chains form
 * a particular syseventtem. This API will return a handle that can be used to
 * queue notifications using the sysevent_notif_queue_notification API by passing
 * in that handle as an argument.
 */
void *sysevent_notif_add_sysevent(const char *sysevent_name);
int sysevent_notif_queue_notification(void *sysevent_handle,
					enum sysevent_notif_type notif_type,
					void *data);
void *sysevent_get_early_notif_info(const char *sysevent_name);
int sysevent_register_early_notifier(const char *sysevent_name,
				   enum early_sysevent_notif_type notif_type,
				   void (*early_notif_cb)(void *),
				   void *data);
int sysevent_unregister_early_notifier(const char *sysevent_name, enum
				     early_sysevent_notif_type notif_type);
void send_early_notifications(void *early_notif_handle);
#else

static inline void *sysevent_notif_register_notifier(
			const char *sysevent_name, struct notifier_block *nb)
{
	return NULL;
}

static inline int sysevent_notif_unregister_notifier(void *sysevent_handle,
					struct notifier_block *nb)
{
	return 0;
}

static inline void *sysevent_notif_add_sysevent(const char *sysevent_name)
{
	return NULL;
}

static inline int sysevent_notif_queue_notification(void *sysevent_handle,
					enum sysevent_notif_type notif_type,
					void *data)
{
	return 0;
}

static inline void *sysevent_get_early_notif_info(const char *sysevent_name)
{
	return NULL;
}

static inline int sysevent_register_early_notifier(const char *sysevent_name,
				   enum early_sysevent_notif_type notif_type,
				   void (*early_notif_cb)(void *),
				   void *data)
{
	return -ENOTSUPP;
}

static inline int sysevent_unregister_early_notifier(const char *sysevent_name,
						   enum early_sysevent_notif_type
						   notif_type)
{
	return -ENOTSUPP;
}

static inline void send_early_notifications(void *early_notif_handle)
{
}
#endif /* CONFIG_EXYNOS_SYSTEM_EVENT */
#endif
