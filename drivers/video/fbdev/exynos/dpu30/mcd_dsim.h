/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) Samsung Electronics Co., Ltd.
 * Gwanghui Lee <gwanghui.lee@samsung.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#ifndef __MCD_DSIM_H__
#define __MCD_DSIM_H__

#include <media/v4l2-device.h>
#ifdef CONFIG_DYNAMIC_MIPI
#include "dynamic_mipi/band_info.h"
#endif

struct mcd_dsim_device {
	struct v4l2_subdev *panel_drv_sd;
#ifdef CONFIG_DYNAMIC_MIPI
	struct dynamic_mipi_info *dm_info;
#endif
};

#ifdef CONFIG_DYNAMIC_MIPI
int mcd_dsim_get_panel_v4l2_subdev(struct mcd_dsim_device *mcd_dsim);
int mcd_dsim_md_set_pre_freq(struct mcd_dsim_device *mcd_dsim, struct dm_param_info *param);
int mcd_dsim_md_set_post_freq(struct mcd_dsim_device *mcd_dsim, struct dm_param_info *param);
void mcd_dsim_md_set_default_freq(struct mcd_dsim_device *mcd_dsim, int context);
#else
static inline int mcd_dsim_get_panel_v4l2_subdev(struct mcd_dsim_device *mcd_dsim) { return 0; }
static inline void mcd_dsim_md_set_default_freq(struct mcd_dsim_device *mcd_dsim, int context) { return; }
#endif
#endif //__MCD_DSIM_H__

