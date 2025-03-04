/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2013-2018 TRUSTONIC LIMITED
 * All Rights Reserved.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 */
/*
 * Header file of MobiCore Driver Kernel Module Platform
 * specific structures
 *
 * Internal structures of the McDrvModule
 *
 * Header file the MobiCore Driver Kernel Module,
 * its internal structures and defines.
 */
#ifndef _MC_DRV_PLATFORM_H_
#define _MC_DRV_PLATFORM_H_

/* MobiCore Interrupt. */
#if defined(CONFIG_SOC_EXYNOS5430) || defined(CONFIG_SOC_EXYNOS5433) || \
	defined(CONFIG_SOC_EXYNOS7870) || defined(CONFIG_SOC_EXYNOS7880) || \
	defined(CONFIG_SOC_EXYNOS8890)
#define MC_INTR_SSIQ	255
/* Notify Interrupt cannot wakeup the core on this platform */
#define MC_DISABLE_IRQ_WAKEUP
#elif defined(CONFIG_SOC_EXYNOSAUTO9)
#define MC_DEVICE_PROPNAME "samsung,exynos-tee"
/* #define MC_USE_VLX_PMEM */
#elif defined(CONFIG_SOC_EXYNOS7885)
#define MC_INTR_SSIQ	97
#elif defined(CONFIG_SOC_EXYNOS7420) || defined(CONFIG_SOC_EXYNOS7580)
#define MC_INTR_SSIQ	246
#endif

/* Force usage of xenbus_map_ring_valloc as of Linux v4.1 */
#define MC_XENBUS_MAP_RING_VALLOC_4_1

#endif /* _MC_DRV_PLATFORM_H_ */
