/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) Samsung Electronics Co., Ltd.
 * Gwanghui Lee <gwanghui.lee@samsung.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#ifndef __MCD_HELPER_H__
#define __MCD_HELPER_H__

#include <linux/fb.h>
#include <linux/time.h>
#include <linux/rtc.h>


#include "mcd_decon.h"
#include "decon.h"

#ifdef CONFIG_DYNAMIC_MIPI
struct dynamic_mipi_info *mcd_dm_get_info(struct v4l2_subdev *panel_sd);
#else
static inline struct dynamic_mipi_info *mcd_dm_get_info(struct v4l2_subdev *panel_sd) { return NULL; }
#endif

/* call from pan_display */
#if IS_ENABLED(CONFIG_MCD_PANEL)
int mcd_decon_set_win(struct decon_device *decon, struct fb_info *info);
#else
static inline int mcd_decon_set_win(struct decon_device *decon, struct fb_info *info) { return 0; }
#endif
#if IS_ENABLED(CONFIG_RTC_LIB)
int get_str_cur_rtc(char *buf, size_t size);
#else
static inline int get_str_cur_rtc(char *buf, size_t size) { return -EINVAL; }
#endif
#endif //__MCD_HELPER_H__
