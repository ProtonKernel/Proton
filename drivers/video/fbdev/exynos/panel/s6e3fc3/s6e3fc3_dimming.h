/*
 * linux/drivers/video/fbdev/exynos/panel/s6e3fab/s6e3fab_dimming.h
 *
 * Header file for S6E3FC3 Dimming Driver
 *
 * Copyright (c) 2016 Samsung Electronics
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#ifndef __S6E3FC3_DIMMING_H__
#define __S6E3FC3_DIMMING_H__
#include <linux/types.h>
#include <linux/kernel.h>
#include "../dimming.h"
#include "s6e3fc3.h"

#define S6E3FC3_NR_TP (11)

#define S6E3FC3_NR_LUMINANCE (256)
#define S6E3FC3_TARGET_LUMINANCE (420)
#define S6E3FC3_NR_HBM_LUMINANCE (231)
#define S6E3FC3_TARGET_HBM_LUMINANCE (800)

#ifdef CONFIG_SUPPORT_HMD
#define S6E3FC3_UNBOUND_HMD_NR_LUMINANCE (256)
#define S6E3FC3_UNBOUND_HMD_TARGET_LUMINANCE (105)
#endif

#ifdef CONFIG_SUPPORT_AOD_BL
#define S6E3FC3_AOD_NR_LUMINANCE (4)
#define S6E3FC3_AOD_TARGET_LUMINANCE (60)
#endif

#define S6E3FC3_TOTAL_NR_LUMINANCE (S6E3FC3_NR_LUMINANCE + S6E3FC3_NR_HBM_LUMINANCE)

#endif /* __S6E3FC3_DIMMING_H__ */
