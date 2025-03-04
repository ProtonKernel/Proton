/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Samsung Exynos SoC series dsp driver
 *
 * Copyright (c) 2020 Samsung Electronics Co., Ltd.
 *              http://www.samsung.com/
 */

#ifndef __HW_DUMMY_DSP_HW_DUMMY_PM_H__
#define __HW_DUMMY_DSP_HW_DUMMY_PM_H__

#include "hardware/dsp-pm.h"

int dsp_hw_dummy_pm_devfreq_active(struct dsp_pm *pm);
int dsp_hw_dummy_pm_update_devfreq_nolock(struct dsp_pm *pm, int id, int val);
int dsp_hw_dummy_pm_update_devfreq_nolock_async(struct dsp_pm *pm, int id,
		int val);
int dsp_hw_dummy_pm_update_extra_devfreq(struct dsp_pm *pm, int id, int val);
int dsp_hw_dummy_pm_set_force_qos(struct dsp_pm *pm, int id, int val);
int dsp_hw_dummy_pm_dvfs_enable(struct dsp_pm *pm, int val);
int dsp_hw_dummy_pm_dvfs_disable(struct dsp_pm *pm, int val);
int dsp_hw_dummy_pm_update_devfreq_busy(struct dsp_pm *pm, int val);
int dsp_hw_dummy_pm_update_devfreq_idle(struct dsp_pm *pm, int val);
int dsp_hw_dummy_pm_update_devfreq_boot(struct dsp_pm *pm);
int dsp_hw_dummy_pm_update_devfreq_max(struct dsp_pm *pm);
int dsp_hw_dummy_pm_update_devfreq_min(struct dsp_pm *pm);
int dsp_hw_dummy_pm_set_boot_qos(struct dsp_pm *pm, int val);
int dsp_hw_dummy_pm_boost_enable(struct dsp_pm *pm);
int dsp_hw_dummy_pm_boost_disable(struct dsp_pm *pm);
int dsp_hw_dummy_pm_enable(struct dsp_pm *pm);
int dsp_hw_dummy_pm_disable(struct dsp_pm *pm);

int dsp_hw_dummy_pm_open(struct dsp_pm *pm);
int dsp_hw_dummy_pm_close(struct dsp_pm *pm);
int dsp_hw_dummy_pm_probe(struct dsp_system *sys);
void dsp_hw_dummy_pm_remove(struct dsp_pm *pm);

#endif
