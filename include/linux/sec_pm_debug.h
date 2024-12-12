/*
 * sec_pm_debug.h - SEC Power Management Debug Support
 *
 *  Copyright (c) 2018 Samsung Electronics Co., Ltd.
 *      http://www.samsung.com
 *  Minsung Kim <ms925.kim@samsung.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */
#ifndef __LINUX_SEC_PM_DEBUG_H
#define __LINUX_SEC_PM_DEBUG_H __FILE__

#if IS_ENABLED(CONFIG_REGULATOR_S2MPS23)
extern int pmic_get_onsrc(u8 *onsrc, size_t size);
extern int pmic_get_offsrc(u8 *offsrc, size_t size);
#else
static inline int pmic_get_onsrc(u8 *onsrc, size_t size) { return 0; }
static inline int pmic_get_offsrc(u8 *offsrc, size_t size) { return 0; }
#endif

#endif /* __LINUX_SEC_PM_DEBUG_H */
