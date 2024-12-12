/*
 * Copyright (C) 2012-2020, Samsung Electronics Co., Ltd.
 *
 * This software is licensed under the terms of the GNU General Public
 * License version 2, as published by the Free Software Foundation, and
 * may be copied, distributed, and modified under those terms.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */

#ifndef __SUBSYSTEM_H__
#define __SUBSYSTEM_H__

#include <linux/kconfig.h>
#include <linux/module.h>

#if IS_MODULE(CONFIG_TZDEV)
#define tzdev_initcall(fn)
#define tzdev_exitcall(fn)
#define tzdev_early_initcall(fn)
#define tzdev_late_initcall(fn)

int tzdev_call_init_array(void);
#else
#define tzdev_initcall(fn)          module_init(fn)
#define tzdev_exitcall(fn)          module_exit(fn)
#define tzdev_early_initcall(fn)    early_initcall(fn)
#define tzdev_late_initcall(fn)     late_initcall(fn)

static inline int tzdev_call_init_array(void)
{
    return 0;
}
#endif

#endif /* __SUBSYSTEM_H__ */