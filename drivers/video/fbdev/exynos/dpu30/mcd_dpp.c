/*
 * Copyright (c) 2016 Samsung Electronics Co., Ltd.
 *		http://www.samsung.com
 *
 * Samsung EXYNOS MCD HDR driver

 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */


#include "mcd_dpp.h"
#include <linux/platform_device.h>
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/device.h>

#ifdef CONFIG_MCDHDR
#include "mcdhdr/mcdhdr_drv.h"
#endif

#ifdef CONFIG_MCDHDR
static int hdr_match_handler(struct device *dev, const void *data)
{
	struct mcdhdr_device *hdr;
	struct dpp_device *dpp = (struct dpp_device *)data;

	hdr = (struct mcdhdr_device *)dev_get_drvdata(dev);
	if (hdr == NULL) {
		dpp_err("DDP:ERR:%s:NULL HDR structure\n", __func__);
		return -EINVAL;
	}

	if (dpp->id == hdr->id) {
		dpp_info("dpp:%d get mcdhdr subdev: %d.. done\n", dpp->id, hdr->id);
		dpp->mcd_dpp.hdr_sd = &hdr->sd;
	}

	return 0;
}

static int get_mcdhdr_capability(struct dpp_device *dpp)
{
	int ret = 0;
	u32 hdr_capa;
	struct mcd_dpp_device *mcd_dpp;

	if (!dpp) {
		dpp_err("%s: dpp is null\n", __func__);
		return -EINVAL;
	}

	mcd_dpp = &dpp->mcd_dpp;
	if (!mcd_dpp->hdr_sd) {
		dpp_err("%s: dpp:%d's hdr sd is null\n", __func__, dpp->id);
		return -EINVAL;
	}

	ret = v4l2_subdev_call(mcd_dpp->hdr_sd, core, ioctl, HDR_GET_CAPA, &hdr_capa);
	if (ret) {
		dpp_err("%s:dpp:%d failed to get hdr capa", __func__, dpp->id);
		return ret;
	}

	if (IS_SUPPORT_HDR10(hdr_capa))
		dpp->attr |= (1 << DPP_ATTR_C_HDR);

	if (IS_SUPPORT_HDR10P(hdr_capa))
		dpp->attr |= (1 << DPP_ATTR_C_HDR10_PLUS);

	if (IS_SUPPORT_WCG(hdr_capa))
		dpp->attr |= (1 << DPP_ATTR_WCG);

	dpp_info("dpp:%d hdr capaility wcg: %s, hdr10: %s, hdr10+: %s\n", dpp->id,
		IS_SUPPORT_WCG(hdr_capa) ? "OK" : "NG",
		IS_SUPPORT_HDR10(hdr_capa) ? "OK" : "NG",
		IS_SUPPORT_HDR10P(hdr_capa) ? "OK" : "NG");

	return ret;
}


int dpp_set_mcdhdr_params(struct dpp_device *dpp, struct dpp_params_info *p)
{
	int ret = 0;
	struct mcd_dpp_device *mcd_dpp;
	struct dpp_config *dpp_config;
	struct mcdhdr_win_config mcdhdr_config;

	if (!dpp) {
		dpp_err("%s dpp is null\n", __func__);
		ret = -EINVAL;
		goto exit_set;
	}

	dpp_config = dpp->dpp_config;
	if (!dpp_config) {
		dpp_err("%s dpp config is null\n", __func__);
		ret = -EINVAL;
		goto exit_set;
	}
	if (!dpp_config->mcd_config.hdr_info)
		goto exit_set;

	mcd_dpp = &dpp->mcd_dpp;
	if (!mcd_dpp->hdr_sd) {
		/*dpp for WB not supprot hdr*/
		if (dpp->id == ODMA_WB)
			return 0;
		dpp_err("%s dpp hdr sd is null\n", __func__);
		goto exit_set;
	}

	mcdhdr_config.hdr_info = dpp_config->mcd_config.hdr_info;
	mcdhdr_config.hdr = p->hdr;
	mcdhdr_config.eq_mode = p->eq_mode;

	ret = v4l2_subdev_call(mcd_dpp->hdr_sd, core, ioctl, HDR_WIN_CONFIG, &mcdhdr_config);
	if (ret) {
		dpp_err("%s:dpp:%d failed to hdr win config", dpp->id, __func__);
		return ret;
	}

exit_set:
	return ret;
}

void dpp_stop_mcdhdr(struct dpp_device *dpp)
{
	int ret = 0;
	struct mcd_dpp_device *mcd_dpp;

	if (!dpp) {
		dpp_err("%s dpp is null\n", __func__);
		return;
	}

	mcd_dpp = &dpp->mcd_dpp;
	if (!mcd_dpp->hdr_sd) {
		/*dpp for WB not supprot hdr*/
		if (dpp->id == ODMA_WB)
			return;
		dpp_err("%s dpp hdr sd is null\n", __func__);
		return;
	}

	ret = v4l2_subdev_call(mcd_dpp->hdr_sd, core, ioctl, HDR_STOP, NULL);
	if (ret)
		dpp_err("%s:dpp:%d failed to stop hdr", dpp->id, __func__);

}

#endif

int get_v4l2_subdev_sd(char *dev_name, void *data, int (*match)(struct device *dev, const void *data))
{
	int ret = 0;
	struct device_driver *drv;

	drv = driver_find(dev_name, &platform_bus_type);
	if (IS_ERR_OR_NULL(drv)) {
		dpp_err("%s: failed to find driver\n", __func__);
		return -ENODEV;
	}

	driver_find_device(drv, NULL, data, match);

	return ret;
}


int dpp_get_mcdhdr_info(struct dpp_device *dpp)
{
	int ret = 0;

#ifdef CONFIG_MCDHDR
	dpp_info("%s: %d\n", __func__, dpp->id);

	if (dpp->id >= MAX_MCDHDR_CNT) {
		dpp_err("%s : dpp%d not supporing hdr\n", __func__, dpp->id);
		return ret;
	}

	ret = get_v4l2_subdev_sd(MCDHDR_DRV_NAME, dpp, hdr_match_handler);
	if ((ret) || (dpp->mcd_dpp.hdr_sd == NULL)) {
		dpp_err("%s: failed to get mcdhdr subdev\n", __func__);
		return ret;
	}

	ret = get_mcdhdr_capability(dpp);
	if (ret) {
		dpp_err("%s: failed to get mcdhdr attributes\n", __func__);
		return ret;
	}
#endif
	return ret;
}

