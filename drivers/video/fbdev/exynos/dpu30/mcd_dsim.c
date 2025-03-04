/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) Samsung Electronics Co., Ltd.
 * Gwanghui Lee <gwanghui.lee@samsung.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#include <asm/byteorder.h>
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/platform_device.h>

#include "mcd_dsim.h"
#include "dsim.h"
#include "panel_drv.h"
#ifdef CONFIG_DYNAMIC_MIPI
#include "dynamic_mipi/dynamic_mipi.h"
#endif

#ifdef CONFIG_DYNAMIC_MIPI
void mcd_dsim_md_set_default_freq(struct mcd_dsim_device *mcd_dsim, int context)
{
	struct dynamic_mipi_info *dm;
	struct dm_status_info *dm_status;

	if (!mcd_dsim)
		return;

	dm = mcd_dsim->dm_info;
	if (!dm) {
		dsim_err("ERR:%s:dm is null\n");
		return;
	}

	dm_status = &dm->dm_status;
	if ((dm_status->target_df != dm->dm_dt.dm_default) ||
		(dm_status->current_df != dm->dm_dt.dm_default)) {

		dm_status->target_df = dm->dm_dt.dm_default;
		dm_status->current_df = dm->dm_dt.dm_default;

		/* To set FFC after panel power on*/
		if (context == DM_REQ_CTX_PWR_ON)
			dm_status->ffc_df = dm->dm_dt.dm_default;

		dm_status->req_ctx = context;
	}

	if (context == DM_REQ_CTX_PWR_ON)
		atomic_set(&dm_status->frame_cnt, 0);

}

int mcd_dsim_md_set_pre_freq(struct mcd_dsim_device *mcd_dsim, struct dm_param_info *param)
{
	int ret = 0;
	struct dsim_device *dsim;

	if (!mcd_dsim) {
		dsim_err("MCD:DM:ERR:%s:dsim is null\n", __func__);
		goto exit_set_pre;
	}

	dsim = container_of(mcd_dsim, struct dsim_device, mcd_dsim);

	if (param->req_ctx)
		dsim_dbg("MCD:DM:INFO:%s:p:%d,m:%d,s:%d\n",
			__func__, param->pms.p, param->pms.m, param->pms.k);

	dsim_reg_set_dphy_freq_hopping(dsim->id,
		param->pms.p, param->pms.m, param->pms.k, 1);

exit_set_pre:
	return ret;
}

int mcd_dsim_md_set_post_freq(struct mcd_dsim_device *mcd_dsim, struct dm_param_info *param)
{
	int ret = 0;
	struct dsim_device *dsim;

	if (!mcd_dsim) {
		dsim_err("MCD:DM:ERR:%s:dsim is null\n", __func__);
		goto exit_set_post;
	}

	dsim = container_of(mcd_dsim, struct dsim_device, mcd_dsim);

	if (param->req_ctx)
		dsim_info("MCD:DM:INFO:%s:p,m,k:%d,%d,%d\n",
			__func__, param->pms.p, param->pms.m, param->pms.k);

	dsim_reg_set_dphy_freq_hopping(dsim->id,
		param->pms.p, param->pms.m, param->pms.k, 0);

exit_set_post:
	return ret;
}


static int __match_panel_v4l2_subdev(struct device *dev, const void *data)
{
	struct panel_device *panel_drv;
	struct mcd_dsim_device *mcd_dsim;

	mcd_dsim = (struct mcd_dsim_device *)data;
	if (mcd_dsim == NULL) {
		dsim_err("MCD:DM:%s:failed to get panel's v4l2 subdev\n");
		return -EINVAL;
	}

	panel_drv = (struct panel_device *)dev_get_drvdata(dev);
	mcd_dsim->panel_drv_sd = &panel_drv->sd;

	return 0;
}


int mcd_dsim_get_panel_v4l2_subdev(struct mcd_dsim_device *mcd_dsim)
{
	struct device_driver *drv;
	struct device *dev;

	if (!mcd_dsim) {
		dsim_err("MCD:DM:ERR:%s mcd_dsim is null\n");
		return -ENODEV;
	}

	drv = driver_find(PANEL_DRV_NAME, &platform_bus_type);
	if (IS_ERR_OR_NULL(drv)) {
		dsim_err("MCD:DM:ERR:%s:failed to find driver\n", __func__);
		return -ENODEV;
	}

	dev = driver_find_device(drv, NULL, mcd_dsim, __match_panel_v4l2_subdev);
	if (mcd_dsim->panel_drv_sd == NULL) {
		dsim_err("MCD:DM:ERR:%s:failed to fined panel's v4l2 subdev\n", __func__);
		return -ENODEV;
	}

	return 0;
}
#endif
