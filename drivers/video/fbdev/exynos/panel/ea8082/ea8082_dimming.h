/*
 * linux/drivers/video/fbdev/exynos/panel/s6e3fab/s6e3fab_dimming.h
 *
 * Header file for EA8082 Dimming Driver
 *
 * Copyright (c) 2016 Samsung Electronics
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#ifndef __EA8082_DIMMING_H__
#define __EA8082_DIMMING_H__
#include <linux/types.h>
#include <linux/kernel.h>
#include "../dimming.h"
#include "ea8082.h"

#define EA8082_NR_TP (11)

#define EA8082_NR_LUMINANCE (256)
#define EA8082_TARGET_LUMINANCE (420)
#define EA8082_NR_HBM_LUMINANCE (231)
#define EA8082_TARGET_HBM_LUMINANCE (800)

#ifdef CONFIG_SUPPORT_HMD
#define EA8082_UNBOUND_HMD_NR_LUMINANCE (256)
#define EA8082_UNBOUND_HMD_TARGET_LUMINANCE (105)
#endif

#ifdef CONFIG_SUPPORT_AOD_BL
#define EA8082_AOD_NR_LUMINANCE (4)
#define EA8082_AOD_TARGET_LUMINANCE (60)
#endif

#define EA8082_TOTAL_NR_LUMINANCE (EA8082_NR_LUMINANCE + EA8082_NR_HBM_LUMINANCE)

#endif /* __EA8082_DIMMING_H__ */
