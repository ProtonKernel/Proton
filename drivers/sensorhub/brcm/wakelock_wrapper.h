/*
 *  Copyright (C) 2011, Samsung Electronics Co. Ltd. All Rights Reserved.
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 */

#ifndef __WAKELOCK_WRAPPER_H__
#define __WAKELOCK_WRAPPER_H__

#include <linux/version.h>
#include <linux/pm_wakeup.h>

/*
#if LINUX_VERSION_CODE < KERNEL_VERSION(5, 4, 0)
    wakeup_source_init(ac_data->ws, "gps_geofence_wakelock");   // 4.19 R
    if (!(ac_data->ws)) {
        ac_data->ws = wakeup_source_create("gps_geofence_wakelock"); // 4.19 Q
        if (ac_data->ws)
            wakeup_source_add(ac_data->ws);
    }
#else
    ac_data->ws = wakeup_source_register(NULL, "gps_geofence_wakelock"); // 5.4 R
#endif
*/

#if (LINUX_VERSION_CODE  >= KERNEL_VERSION(5, 4, 0))
#define wake_lock_init(wakeup_source, dev, name) \
do { \
	wakeup_source = wakeup_source_register(dev, name); \
} while (0);
#else
#define wake_lock_init(wakeup_source, dev, name) \
do { \
    wakeup_source_init(wakeup_source, name);	\
    if (!(wakeup_source)) {	\
        wakeup_source = wakeup_source_create(name);	\
        if (wakeup_source)	\
            wakeup_source_add(wakeup_source);	\
    }	\
} while (0);
#endif /* LINUX_VERSION >= 5.4.0 */

#define wake_lock_destroy(wakeup_source) \
do { \
    wakeup_source_unregister(wakeup_source); \
    (wakeup_source) = NULL; \
} while (0);

#define wake_lock(wakeup_source)			__pm_stay_awake(wakeup_source)
#define wake_unlock(wakeup_source)			__pm_relax(wakeup_source)
#define wake_lock_active(wakeup_source)		((wakeup_source)->active)
#define wake_lock_timeout(wakeup_source, timeout)	\
	__pm_wakeup_event(wakeup_source, jiffies_to_msecs(timeout))


#endif