/*
 * Samsung Exynos SoC series NPU driver
 *
 * Copyright (c) 2020 Samsung Electronics Co., Ltd.
 *	http://www.samsung.com/
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 */

#ifndef _NPU_WAKELOCK_H_
#define _NPU_WAKELOCK_H_

#include <linux/ktime.h>
#include <linux/device.h>

/* A npu_wake_lock prevents the system from entering suspend or other low power
 * states when active. If the type is set to WAKE_LOCK_SUSPEND, the npu_wake_lock
 * prevents a full system suspend.
 */

enum {
	NPU_WAKE_LOCK_SUSPEND, /* Prevent suspend */
	NPU_WAKE_LOCK_TYPE_COUNT
};

//struct npu_wake_lock {
//	struct wakeup_source ws;
//};

static inline void npu_wake_lock_init(struct device *dev, struct wakeup_source **ws,
					int type, const char *name)
{
	*ws = wakeup_source_register(dev, name);
}

static inline void npu_wake_lock_destroy(struct wakeup_source *ws)
{
	wakeup_source_unregister(ws);
}

static inline void npu_wake_lock(struct wakeup_source *ws)
{
	__pm_stay_awake(ws);
}

static inline void npu_wake_lock_timeout(struct wakeup_source *ws, long timeout)
{
	__pm_wakeup_event(ws, jiffies_to_msecs(timeout));
}

static inline void npu_wake_unlock(struct wakeup_source *ws)
{
	__pm_relax(ws);
}

static inline int npu_wake_lock_active(struct wakeup_source *ws)
{
	return ws->active;
}

#endif
