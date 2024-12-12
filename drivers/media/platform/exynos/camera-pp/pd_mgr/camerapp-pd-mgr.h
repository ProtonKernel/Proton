/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2020 Samsung Electronics Co., Ltd.
 *		http://www.samsung.com
 *
 * Header file for Exynos CAMERA-PP POWER DOMAIN MANAGER driver
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#ifndef CAMERAPP_PD_MGR_H
#define CAMERAPP_PD_MGR_H

#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/device.h>
#include <linux/version.h>
#include <linux/platform_device.h>
#include <linux/of.h>
#include <linux/io.h>
#include <linux/pm_runtime.h>

#define ON 1
#define OFF 0

struct camerapp_pd_mgr_dev {
	u32 slave_addr;
	atomic_t ref_cnt;
	struct device *dev;
};

#if IS_ENABLED(CONFIG_VIDEO_EXYNOS_CAMERA_POSTPROCESS_PD_MGR)
int camerapp_pd_mgr_on(void);
int camerapp_pd_mgr_off(void);
#else
int camerapp_pd_mgr_on(void) {return 0;}
int camerapp_pd_mgr_off(void) {return 0;}
#endif
#endif /* CAMERAPP_PD_MGR_H */
