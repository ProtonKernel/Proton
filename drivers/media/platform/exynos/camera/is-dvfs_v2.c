/*
 * Samsung Exynos SoC series Pablo driver
 *
 * Exynos Pablo DVFS v2 functions
 *
 * Copyright (c) 2020 Samsung Electronics Co., Ltd
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#include <linux/slab.h>
#if IS_ENABLED(CONFIG_EXYNOS_MSN)
#include <linux/ems.h>
#endif
#include "is-core.h"
#include "is-dvfs.h"
#include "is-hw-dvfs.h"
#include <linux/videodev2_exynos_camera.h>
#include "is-time.h"

#ifdef CONFIG_PM_DEVFREQ

#if defined(QOS_INTCAM)
extern struct is_pm_qos_request exynos_isp_qos_int_cam;
#endif
#if defined(QOS_TNR)
extern struct is_pm_qos_request exynos_isp_qos_tnr;
#endif
#if defined(QOS_CSIS)
extern struct is_pm_qos_request exynos_isp_qos_csis;
#endif
#if defined(QOS_ISP)
extern struct is_pm_qos_request exynos_isp_qos_isp;
#endif
extern struct is_pm_qos_request exynos_isp_qos_int;
extern struct is_pm_qos_request exynos_isp_qos_mem;
extern struct is_pm_qos_request exynos_isp_qos_cam;
extern struct is_pm_qos_request exynos_isp_qos_hpg;

#if IS_ENABLED(CONFIG_EXYNOS_MSN)
extern struct emstune_mode_request emstune_req;
#endif

bool is_dvfs_is_fast_ae(struct is_dvfs_ctrl *dvfs_ctrl)
{
	return false;
}

int is_dvfs_init(struct is_resourcemgr *resourcemgr)
{
	int ret = 0;
	struct is_dvfs_ctrl *dvfs_ctrl;

	FIMC_BUG(!resourcemgr);

	dvfs_ctrl = &resourcemgr->dvfs_ctrl;

#if defined(QOS_INTCAM)
	dvfs_ctrl->cur_int_cam_qos = 0;
#endif
#if defined(QOS_TNR)
	dvfs_ctrl->cur_tnr_qos = 0;
#endif
#if defined(QOS_CSIS)
	dvfs_ctrl->cur_csis_qos = 0;
#endif
#if defined(QOS_ISP)
	dvfs_ctrl->cur_isp_qos = 0;
#endif
	dvfs_ctrl->cur_int_qos = 0;
	dvfs_ctrl->cur_mif_qos = 0;
	dvfs_ctrl->cur_cam_qos = 0;
	dvfs_ctrl->cur_i2c_qos = 0;
	dvfs_ctrl->cur_disp_qos = 0;
	dvfs_ctrl->cur_hpg_qos = 0;
	dvfs_ctrl->cur_hmp_bst = 0;
	dvfs_ctrl->thr_int_qos = 0;
	dvfs_ctrl->thr_int_cam_qos = 0;
	dvfs_ctrl->thr_mif_qos = 0;
	dvfs_ctrl->cur_cpus = NULL;
	atomic_set(&dvfs_ctrl->thrott_state, 0);

	/* init spin_lock for clock gating */
	mutex_init(&dvfs_ctrl->lock);

	if (!(dvfs_ctrl->static_ctrl))
		dvfs_ctrl->static_ctrl =
			kzalloc(sizeof(struct is_dvfs_scenario_ctrl), GFP_KERNEL);
	if (!(dvfs_ctrl->dynamic_ctrl))
		dvfs_ctrl->dynamic_ctrl =
			kzalloc(sizeof(struct is_dvfs_scenario_ctrl), GFP_KERNEL);
	if (!(dvfs_ctrl->external_ctrl))
		dvfs_ctrl->external_ctrl =
			kzalloc(sizeof(struct is_dvfs_scenario_ctrl), GFP_KERNEL);

	if (!dvfs_ctrl->static_ctrl || !dvfs_ctrl->dynamic_ctrl || !dvfs_ctrl->external_ctrl) {
		err("dvfs_ctrl alloc is failed!!\n");
		return -ENOMEM;
	}

	/* assign static / dynamic scenario check logic data */
	ret = is_hw_dvfs_init((void *)dvfs_ctrl);
	if (ret) {
		err("is_hw_dvfs_init is failed(%d)\n", ret);
		return -EINVAL;
	}

	/* default value is 0 */
	dvfs_ctrl->dvfs_table_idx = 0;
	clear_bit(IS_DVFS_SEL_TABLE, &dvfs_ctrl->state);

	/* initialize bundle operating variable */
	dvfs_ctrl->bundle_operating = false;

	return 0;
}

int is_dvfs_sel_table(struct is_resourcemgr *resourcemgr)
{
	int ret = 0;
	struct is_dvfs_ctrl *dvfs_ctrl;
	u32 dvfs_table_idx = 0;

	FIMC_BUG(!resourcemgr);

	dvfs_ctrl = &resourcemgr->dvfs_ctrl;

	if (test_bit(IS_DVFS_SEL_TABLE, &dvfs_ctrl->state))
		return 0;

#if defined(EXPANSION_DVFS_TABLE)
	switch(resourcemgr->hal_version) {
	case IS_HAL_VER_1_0:
		dvfs_table_idx = 0;
		break;
	case IS_HAL_VER_3_2:
		dvfs_table_idx = 1;
		break;
	default:
		err("hal version is unknown");
		dvfs_table_idx = 0;
		ret = -EINVAL;
		break;
	}
#endif

	if (dvfs_table_idx >= dvfs_ctrl->dvfs_table_max) {
		err("dvfs index(%d) is invalid", dvfs_table_idx);
		ret = -EINVAL;
		goto p_err;
	}

	resourcemgr->dvfs_ctrl.dvfs_table_idx = dvfs_table_idx;
	set_bit(IS_DVFS_SEL_TABLE, &dvfs_ctrl->state);

p_err:
	info("[RSC] %s(%d):%d\n", __func__, dvfs_table_idx, ret);
	return ret;
}

int is_dvfs_sel_static(struct is_device_ischain *device)
{
	struct is_dvfs_ctrl *dvfs_ctrl;
	struct is_dvfs_scenario_ctrl *static_ctrl;
	struct is_resourcemgr *resourcemgr;
	int scenario_id;

	TIME_STR(1);

	FIMC_BUG(!device);
	FIMC_BUG(!device->interface);

	resourcemgr = device->resourcemgr;
	dvfs_ctrl = &(resourcemgr->dvfs_ctrl);
	static_ctrl = dvfs_ctrl->static_ctrl;

	if (!test_bit(IS_DVFS_SEL_TABLE, &dvfs_ctrl->state)) {
		err("dvfs table is NOT selected");
		return -EINVAL;
	}

	/* static scenario */
	if (!static_ctrl) {
		err("static_dvfs_ctrl is NULL");
		return -EINVAL;
	}

	dbg_dvfs(1, "[DVFS] %s setfile: %d, scen: %d, common: %d, vendor: %d\n", device, __func__,
		device->setfile & IS_SETFILE_MASK, (device->setfile & IS_SCENARIO_MASK) >> IS_SCENARIO_SHIFT,
		device->dvfs_scenario & IS_DVFS_SCENARIO_COMMON_MODE_MASK,
		(device->dvfs_scenario >> IS_DVFS_SCENARIO_VENDOR_SHIFT) & IS_DVFS_SCENARIO_VENDOR_MASK);

	scenario_id = is_hw_dvfs_get_scenario(device, IS_DVFS_STATIC);
	if (scenario_id >= 0) {
		static_ctrl->cur_scenario_id = scenario_id;
		static_ctrl->cur_frame_tick = -1;
		static_ctrl->cur_scenario_idx = 0;
		static_ctrl->scenarios[static_ctrl->cur_scenario_idx].scenario_nm =
			is_hw_dvfs_get_scenario_name(scenario_id, IS_DVFS_STATIC);
	}
	TIME_END(1, __func__);
	return  scenario_id;
}

int is_dvfs_sel_dynamic(struct is_device_ischain *device, struct is_group *group, struct is_frame *frame)
{
	struct is_dvfs_ctrl *dvfs_ctrl;
	struct is_dvfs_scenario_ctrl *dynamic_ctrl;
	struct is_dvfs_scenario_ctrl *static_ctrl;
	struct is_resourcemgr *resourcemgr;
	int scenario_id;

	FIMC_BUG(!device);

	resourcemgr = device->resourcemgr;
	dvfs_ctrl = &(resourcemgr->dvfs_ctrl);
	dynamic_ctrl = dvfs_ctrl->dynamic_ctrl;
	static_ctrl = dvfs_ctrl->static_ctrl;

	if (!test_bit(IS_DVFS_SEL_TABLE, &dvfs_ctrl->state)) {
		err("dvfs table is NOT selected");
		return -EINVAL;
	}

	/* dynamic scenario */
	if (!dynamic_ctrl) {
		err("dynamic_dvfs_ctrl is NULL");
		return -EINVAL;
	}

	if (dynamic_ctrl->cur_frame_tick >= 0) {
		(dynamic_ctrl->cur_frame_tick)--;
		/*
		 * when cur_frame_tick is lower than 0, clear current scenario.
		 * This means that current frame tick to keep dynamic scenario
		 * was expired.
		 */
		if (dynamic_ctrl->cur_frame_tick < 0) {
			dynamic_ctrl->cur_scenario_id = -1;
		}
	}

	if (!test_bit(IS_ISCHAIN_REPROCESSING, &device->state) || group->id == GROUP_ID_VRA0
		|| static_ctrl->cur_scenario_id == IS_DVFS_SN_REAR_SINGLE_WIDE_SSM
		|| static_ctrl->cur_scenario_id == IS_DVFS_SN_REAR_SINGLE_ULTRAWIDE_SSM)
		return -EAGAIN;

	scenario_id = is_hw_dvfs_get_scenario(device, IS_DVFS_DYNAMIC);
	if (scenario_id >= 0) {
		dynamic_ctrl->cur_scenario_id = scenario_id;
		dynamic_ctrl->cur_frame_tick = is_hw_dvfs_get_frame_tick(scenario_id);
		dynamic_ctrl->cur_scenario_idx = 0;
		dynamic_ctrl->scenarios[dynamic_ctrl->cur_scenario_idx].scenario_nm =
			is_hw_dvfs_get_scenario_name(scenario_id, IS_DVFS_DYNAMIC);
	}

	return	scenario_id;
}

int is_dvfs_sel_external(struct is_device_sensor *device)
{
	struct is_dvfs_ctrl *dvfs_ctrl;
	struct is_dvfs_scenario_ctrl *external_ctrl;
	struct is_resourcemgr *resourcemgr;
	int scenario_id;

	FIMC_BUG(!device);

	resourcemgr = device->resourcemgr;
	dvfs_ctrl = &(resourcemgr->dvfs_ctrl);
	external_ctrl = dvfs_ctrl->external_ctrl;

	if (!test_bit(IS_DVFS_SEL_TABLE, &dvfs_ctrl->state)) {
		err("dvfs table is NOT selected");
		return -EINVAL;
	}

	/* external scenario */
	if (!external_ctrl) {
		warn("external_dvfs_ctrl is NULL, default max dvfs lv");
		return IS_SN_MAX;
	}

	scenario_id = is_hw_dvfs_get_ext_scenario(device->ischain, IS_DVFS_EXTERNAL);
	if (scenario_id >= 0) {
		external_ctrl->cur_scenario_id = scenario_id;
		external_ctrl->cur_frame_tick = -1;
		external_ctrl->cur_scenario_idx = 0;
		external_ctrl->scenarios[external_ctrl->cur_scenario_idx].scenario_nm =
			is_hw_dvfs_get_scenario_name(scenario_id, IS_DVFS_EXTERNAL);
	}

	return	scenario_id;
}


int is_get_qos(struct is_core *core, u32 type, u32 scenario_id)
{
	struct exynos_platform_is	*pdata = NULL;
	int qos = 0;
	u32 dvfs_idx = core->resourcemgr.dvfs_ctrl.dvfs_table_idx;

	pdata = core->pdata;
	if (pdata == NULL) {
		err("pdata is NULL\n");
		return -EINVAL;
	}

	if (type >= IS_DVFS_END) {
		err("Cannot find DVFS value");
		return -EINVAL;
	}

	if (dvfs_idx >= IS_DVFS_TABLE_IDX_MAX) {
		err("invalid dvfs index(%d)", dvfs_idx);
		dvfs_idx = 0;
	}

	qos = pdata->dvfs_data[dvfs_idx][scenario_id][type];

	return qos;
}

const char *is_get_cpus(struct is_core *core, u32 scenario_id)
{
	struct exynos_platform_is *pdata;
	u32 dvfs_idx = core->resourcemgr.dvfs_ctrl.dvfs_table_idx;

	pdata = core->pdata;
	if (pdata == NULL) {
		err("pdata is NULL\n");
		return NULL;
	}

	if (dvfs_idx >= IS_DVFS_TABLE_IDX_MAX) {
		err("invalid dvfs index(%d)", dvfs_idx);
		dvfs_idx = 0;
	}

	return pdata->dvfs_cpu[dvfs_idx][scenario_id];
}

static void is_pm_qos_update_bundle(struct is_core *core,
		struct is_device_ischain *device, u32 scenario_id)
{
	struct is_resourcemgr *resourcemgr;
	struct is_dvfs_ctrl *dvfs_ctrl;
	u32 nums = 0;
	u32 types[8] = {0, };
	u32 scns[8] = {0, };
	u32 qos, cam_qos, csis_qos;
	int i;

	/* By adopting CSIS_PDP_VOTF_GLOBAL_WA, it doesn't need this code anymore */
	return;

	resourcemgr = &core->resourcemgr;
	dvfs_ctrl = &(resourcemgr->dvfs_ctrl);

	cam_qos = is_get_qos(core, IS_DVFS_CAM, scenario_id);
	csis_qos = is_get_qos(core, IS_DVFS_CSIS, scenario_id);

	if (dvfs_ctrl->cur_csis_qos == csis_qos && dvfs_ctrl->cur_cam_qos == cam_qos)
		return;

	if (is_hw_dvfs_get_bundle_update_seq(scenario_id, &nums, types,
				scns, &dvfs_ctrl->bundle_operating)) {
		for (i = 0; i < nums; i++) {
			qos = is_get_qos(core, types[i], scns[i]);

			minfo("DVFS bundle[%d]: type %d, scn:%d, qos:%d", device,
					i, types[i], scns[i], qos);
			switch (types[i]) {
			case IS_DVFS_CSIS:
#if defined(QOS_CSIS)
				is_pm_qos_update_request(&exynos_isp_qos_csis, qos);
				dvfs_ctrl->cur_csis_qos = qos;
#endif
				break;
			case IS_DVFS_CAM:
				is_pm_qos_update_request(&exynos_isp_qos_cam, qos);
				dvfs_ctrl->cur_cam_qos = qos;
				break;
			case IS_DVFS_INT_CAM:
#if defined(QOS_INTCAM)
				is_pm_qos_update_request(&exynos_isp_qos_int_cam, qos);
				dvfs_ctrl->cur_int_cam_qos = qos;
#endif
				break;
			case IS_DVFS_TNR:
#if defined(QOS_TNR)
				is_pm_qos_update_request(&exynos_isp_qos_tnr, qos);
				dvfs_ctrl->cur_tnr_qos = qos;
#endif
				break;
			case IS_DVFS_ISP:
#if defined(QOS_ISP)
				is_pm_qos_update_request(&exynos_isp_qos_isp, qos);
				dvfs_ctrl->cur_isp_qos = qos;
#endif
				break;
			default:
				break;
			}
		}
	}
}

int is_set_int_dvfs(struct is_device_ischain *device, int i2c_qos, int int_qos)
{
	int ret = 0;
	if (i2c_qos && device) {
		ret = is_itf_i2c_lock(device, i2c_qos, true);
		if (ret) {
			err("is_itf_i2_clock fail\n");
			goto exit;
		}
	}

	is_pm_qos_update_request(&exynos_isp_qos_int, int_qos);

	if (i2c_qos && device) {
		/* i2c unlock */
		ret = is_itf_i2c_lock(device, i2c_qos, false);
		if (ret)
			err("is_itf_i2c_unlock fail\n");
	}
exit:
	return ret;
}

int is_set_dvfs(struct is_core *core, struct is_device_ischain *device, int scenario_id)
{
	int ret = 0;
#if defined(QOS_INTCAM)
	int int_cam_qos;
#endif
#if defined(QOS_TNR)
	int tnr_qos;
#endif
#if defined(QOS_CSIS)
	int csis_qos;
#endif
#if defined(QOS_ISP)
	int isp_qos;
#endif
	int int_qos, mif_qos, i2c_qos, cam_qos, disp_qos, hpg_qos;
	char *qos_info;
	const char *cpus;
	struct is_resourcemgr *resourcemgr;
	struct is_dvfs_ctrl *dvfs_ctrl;

	TIME_STR(0);

	if (core == NULL) {
		err("core is NULL\n");
		return -EINVAL;
	}

	if (scenario_id < 0) {
		err("scenario is not valid\n");
		return -EINVAL;
	}

	resourcemgr = &core->resourcemgr;
	dvfs_ctrl = &(resourcemgr->dvfs_ctrl);
	atomic_set(&dvfs_ctrl->thrott_state, 0);

#if defined(QOS_INTCAM)
	int_cam_qos = is_get_qos(core, IS_DVFS_INT_CAM, scenario_id);
#endif
#if defined(QOS_TNR)
	tnr_qos = is_get_qos(core, IS_DVFS_TNR, scenario_id);
#endif
#if defined(QOS_CSIS)
	csis_qos = is_get_qos(core, IS_DVFS_CSIS, scenario_id);
#endif
#if defined(QOS_ISP)
	isp_qos = is_get_qos(core, IS_DVFS_ISP, scenario_id);
#endif
	int_qos = is_get_qos(core, IS_DVFS_INT, scenario_id);
	mif_qos = is_get_qos(core, IS_DVFS_MIF, scenario_id);
	i2c_qos = is_get_qos(core, IS_DVFS_I2C, scenario_id);
	cam_qos = is_get_qos(core, IS_DVFS_CAM, scenario_id);
	disp_qos = is_get_qos(core, IS_DVFS_DISP, scenario_id);
	hpg_qos = is_get_qos(core, IS_DVFS_HPG, scenario_id);
	cpus = is_get_cpus(core, scenario_id);

#if defined(QOS_INTCAM)
	if (int_cam_qos < 0) {
		err("getting int_cam_qos value is failed!!\n");
		return -EINVAL;
	}
#endif
#if defined(QOS_TNR)
	if (tnr_qos < 0) {
		err("getting tnr_qos value is failed!!\n");
		return -EINVAL;
	}
#endif
#if defined(QOS_CSIS)
	if (csis_qos < 0) {
		err("getting csis_qos value is failed!!\n");
		return -EINVAL;
	}
#endif
#if defined(QOS_ISP)
	if (isp_qos < 0) {
		err("getting isp_qos value is failed!!\n");
		return -EINVAL;
	}
#endif
	if ((int_qos < 0) || (mif_qos < 0) || (i2c_qos < 0)
	|| (cam_qos < 0) || (disp_qos < 0)) {
		err("getting qos value is failed!!\n");
		return -EINVAL;
	}

	/* check current qos */
	if (IS_ENABLED(THROTTLING_INT_ENABLE) && resourcemgr->limited_fps) {
		int_qos = THROTTLING_INT_LEVEL;
		if (dvfs_ctrl->thr_int_qos != int_qos) {
			is_set_int_dvfs(device, i2c_qos, int_qos);
			dvfs_ctrl->thr_int_qos = int_qos;
		}
	} else {
		if (int_qos && dvfs_ctrl->cur_int_qos != int_qos) {
			is_set_int_dvfs(device, i2c_qos, int_qos);
			dvfs_ctrl->cur_int_qos = int_qos;
		}
	}

	is_pm_qos_update_bundle(core, device, scenario_id);

	if (cam_qos && dvfs_ctrl->cur_cam_qos != cam_qos) {
		is_pm_qos_update_request(&exynos_isp_qos_cam, cam_qos);
		dvfs_ctrl->cur_cam_qos = cam_qos;
	}

	is_pm_qos_update_bundle(core, device, scenario_id);

	if (cam_qos && dvfs_ctrl->cur_cam_qos != cam_qos) {
		is_pm_qos_update_request(&exynos_isp_qos_cam, cam_qos);
		dvfs_ctrl->cur_cam_qos = cam_qos;
	}

#if defined(QOS_INTCAM)
	/* check current qos */
	if (IS_ENABLED(THROTTLING_INTCAM_ENABLE) && resourcemgr->limited_fps) {
		int_cam_qos = THROTTLING_INTCAM_LEVEL;
		if (dvfs_ctrl->thr_int_cam_qos != int_cam_qos) {
			is_pm_qos_update_request(&exynos_isp_qos_int_cam, int_cam_qos);
			dvfs_ctrl->thr_int_cam_qos = int_cam_qos;
		}

	} else {
		if (int_cam_qos && dvfs_ctrl->cur_int_cam_qos != int_cam_qos) {
			is_pm_qos_update_request(&exynos_isp_qos_int_cam, int_cam_qos);
			dvfs_ctrl->cur_int_cam_qos = int_cam_qos;
		}
	}
#endif
#if defined(QOS_TNR)
	/* check current qos */
	if (tnr_qos && dvfs_ctrl->cur_tnr_qos != tnr_qos) {
		is_pm_qos_update_request(&exynos_isp_qos_tnr, tnr_qos);
		dvfs_ctrl->cur_tnr_qos = tnr_qos;
	}
#endif
#if defined(QOS_CSIS)
	/* check current qos */
	if (csis_qos && dvfs_ctrl->cur_csis_qos != csis_qos) {
		is_pm_qos_update_request(&exynos_isp_qos_csis, csis_qos);
		dvfs_ctrl->cur_csis_qos = csis_qos;
	}
#endif
#if defined(QOS_ISP)
	/* check current qos */
	if (isp_qos && dvfs_ctrl->cur_isp_qos != isp_qos) {
		is_pm_qos_update_request(&exynos_isp_qos_isp, isp_qos);
		dvfs_ctrl->cur_isp_qos = isp_qos;
	}
#endif
	if (IS_ENABLED(THROTTLING_MIF_ENABLE) && resourcemgr->limited_fps) {
		mif_qos = THROTTLING_MIF_LEVEL;
		if (dvfs_ctrl->thr_mif_qos != mif_qos) {
			is_pm_qos_update_request(&exynos_isp_qos_mem, mif_qos);
			dvfs_ctrl->thr_mif_qos = mif_qos;
		}
	} else {
		if (mif_qos && dvfs_ctrl->cur_mif_qos != mif_qos) {
			is_pm_qos_update_request(&exynos_isp_qos_mem, mif_qos);
			dvfs_ctrl->cur_mif_qos = mif_qos;
		}
	}

#if defined(ENABLE_HMP_BOOST)
	/* hpg_qos : number of minimum online CPU */
	if (hpg_qos && device && (dvfs_ctrl->cur_hpg_qos != hpg_qos)
		&& !test_bit(IS_ISCHAIN_REPROCESSING, &device->state)) {
		is_pm_qos_update_request(&exynos_isp_qos_hpg, hpg_qos);
		dvfs_ctrl->cur_hpg_qos = hpg_qos;

		/* for migration to big core */
		if (hpg_qos > 4) {
			if (!dvfs_ctrl->cur_hmp_bst) {
				minfo("BOOST on\n", device);
#if IS_ENABLED(CONFIG_EXYNOS_MSN)
				emstune_boost(&emstune_req, 1);
#endif
				dvfs_ctrl->cur_hmp_bst = 1;
			}
		} else {
			if (dvfs_ctrl->cur_hmp_bst) {
				minfo("BOOST off\n", device);
#if IS_ENABLED(CONFIG_EXYNOS_MSN)
				emstune_boost(&emstune_req, 0);
#endif
				dvfs_ctrl->cur_hmp_bst = 0;
			}
		}
	}
#endif

	if (cpus && (!dvfs_ctrl->cur_cpus || strcmp(dvfs_ctrl->cur_cpus, cpus)))
		dvfs_ctrl->cur_cpus = cpus;

	qos_info = __getname();
	if (unlikely(!qos_info)) {
		ret = -ENOMEM;
		goto exit;
	}
	snprintf(qos_info, PATH_MAX, "[RSC:%d]: New QoS [", device ? device->instance : 0);
#if defined(QOS_INTCAM)
	snprintf(qos_info + strlen(qos_info),
				PATH_MAX, " INT_CAM(%d),", int_cam_qos);
#endif
#if defined(QOS_TNR)
	snprintf(qos_info + strlen(qos_info),
				PATH_MAX, " TNR(%d),", tnr_qos);
#endif
#if defined(QOS_CSIS)
	snprintf(qos_info + strlen(qos_info),
				PATH_MAX, " CSIS(%d),", csis_qos);
#endif
#if defined(QOS_ISP)
	snprintf(qos_info + strlen(qos_info),
				PATH_MAX, " ISP(%d),", isp_qos);
#endif
	info("%s INT(%d), MIF(%d), CAM(%d), DISP(%d), I2C(%d), HPG(%d, %d), CPU(%s)], L_FPS(%d)\n",
			qos_info, int_qos, mif_qos, cam_qos, disp_qos,
			i2c_qos, hpg_qos, dvfs_ctrl->cur_hmp_bst,
			cpus ? cpus : "",
			resourcemgr->limited_fps);

	__putname(qos_info);

exit:
	/* Trigger updating clock rate of each clock source */
	exynos_is_clk_get_rate(&core->pdev->dev);

	TIME_END(0, __func__);

	return ret;
}

void is_dual_mode_update(struct is_device_ischain *device,
	struct is_group *group,
	struct is_frame *frame)
{
	struct is_core *core = (struct is_core *)device->interface->core;
	struct is_dual_info *dual_info = &core->dual_info;
	struct is_device_sensor *sensor = device->sensor;
	struct is_resourcemgr *resourcemgr;
	int i, streaming_cnt = 0;

	/* Continue if wide and tele/s-wide complete is_sensor_s_input(). */
	if (!(test_bit(SENSOR_POSITION_REAR, &core->sensor_map) &&
		(test_bit(SENSOR_POSITION_REAR2, &core->sensor_map) ||
		 test_bit(SENSOR_POSITION_REAR3, &core->sensor_map) ||
		 test_bit(SENSOR_POSITION_REAR4, &core->sensor_map))))
		return;

	if (group->head->device_type != IS_DEVICE_SENSOR)
		return;

	/* Update max fps of dual sensor device with reference to shot meta. */
	dual_info->max_fps[sensor->position] = frame->shot->ctl.aa.aeTargetFpsRange[1];
	resourcemgr = sensor->resourcemgr;

	/*
	 * bypass - master_max_fps : 30fps, slave_max_fps : 0fps (sensor standby)
	 * sync - master_max_fps : 30fps, slave_max_fps : 30fps (fusion)
	 * switch - master_max_fps : 5ps, slave_max_fps : 30fps (post standby)
	 * nothing - invalid mode
	 */
	for (i = 0; i < SENSOR_POSITION_REAR_TOF; i++) {
		if (resourcemgr->limited_fps && dual_info->max_fps[i])
			streaming_cnt++;
		else {
			if (IS_ENABLED(IS_DVFS_USE_3AA_STNADBY)) {
				if (dual_info->max_fps[i] >= 10)
					streaming_cnt++;
			} else {
				if (dual_info->max_fps[i] >= 5)
					streaming_cnt++;
			}
		}
	}

	if (streaming_cnt == 1)
		dual_info->mode = IS_DUAL_MODE_BYPASS;
	else if (streaming_cnt == 2)
		dual_info->mode = IS_DUAL_MODE_SYNC;
	else if (streaming_cnt >= 3)
		dual_info->mode = IS_DUAL_MODE_OVERRUN;
	else
		dual_info->mode = IS_DUAL_MODE_NOTHING;

	if (dual_info->mode == IS_DUAL_MODE_SYNC || dual_info->mode == IS_DUAL_MODE_OVERRUN) {
		dual_info->max_bds_width =
			max_t(int, dual_info->max_bds_width, device->txp.output.width);
		dual_info->max_bds_height =
			max_t(int, dual_info->max_bds_height, device->txp.output.height);
	} else {
		dual_info->max_bds_width = device->txp.output.width;
		dual_info->max_bds_height = device->txp.output.height;
	}
}

void is_dual_dvfs_update(struct is_device_ischain *device,
	struct is_group *group,
	struct is_frame *frame)
{
	struct is_core *core = (struct is_core *)device->interface->core;
	struct is_dual_info *dual_info = &core->dual_info;
	struct is_resourcemgr *resourcemgr = device->resourcemgr;
	struct is_dvfs_scenario_ctrl *static_ctrl
				= resourcemgr->dvfs_ctrl.static_ctrl;
	int scenario_id, pre_scenario_id;

	/* Continue if wide and tele/s-wide complete is_sensor_s_input(). */
	if (!(test_bit(SENSOR_POSITION_REAR, &core->sensor_map) &&
		(test_bit(SENSOR_POSITION_REAR2, &core->sensor_map) ||
		 test_bit(SENSOR_POSITION_REAR3, &core->sensor_map) ||
		 test_bit(SENSOR_POSITION_REAR4, &core->sensor_map))))
		return;

	if (group->head->device_type != IS_DEVICE_SENSOR)
		return;

	/*
	 * tick_count : Add dvfs update margin for dvfs update when mode is changed
	 * from fusion(sync) to standby(bypass, switch) because H/W does not apply
	 * immediately even if mode is dropped from hal.
	 * tick_count == 0 : dvfs update
	 * tick_count > 0 : tick count decrease
	 * tick count < 0 : ignore
	 */
	if (dual_info->tick_count >= 0)
		dual_info->tick_count--;

	/* If pre_mode and mode are different, tick_count setup. */
	if (dual_info->pre_mode != dual_info->mode) {

		/* If current state is IS_DUAL_NOTHING, do not do DVFS update. */
		if (dual_info->mode == IS_DUAL_MODE_NOTHING)
			dual_info->tick_count = -1;

		switch (dual_info->pre_mode) {
		case IS_DUAL_MODE_BYPASS:
		case IS_DUAL_MODE_SWITCH:
		case IS_DUAL_MODE_NOTHING:
			dual_info->tick_count = 0;
			break;
		case IS_DUAL_MODE_SYNC:
		case IS_DUAL_MODE_OVERRUN:
			dual_info->tick_count = IS_DVFS_DUAL_TICK;
			break;
		default:
			err("invalid dual mode %d -> %d\n", dual_info->pre_mode, dual_info->mode);
			dual_info->tick_count = -1;
			dual_info->pre_mode = IS_DUAL_MODE_NOTHING;
			dual_info->mode = IS_DUAL_MODE_NOTHING;
			break;
		}
	}

	/* Only if tick_count is 0 dvfs update. */
	if (dual_info->tick_count == 0) {
		pre_scenario_id = static_ctrl->cur_scenario_id;
		scenario_id = is_dvfs_sel_static(device);
		if (scenario_id >= 0 && scenario_id != pre_scenario_id) {
			mgrinfo("tbl[%d] dual static scenario(%d)-[%s]\n", device, group, frame,
				resourcemgr->dvfs_ctrl.dvfs_table_idx,
				static_ctrl->cur_scenario_id,
				static_ctrl->scenarios[static_ctrl->cur_scenario_idx].scenario_nm);
			is_set_dvfs((struct is_core *)device->interface->core, device, scenario_id);
		} else {
			mgrinfo("tbl[%d] dual DVFS update skip %d -> %d\n", device, group, frame,
				resourcemgr->dvfs_ctrl.dvfs_table_idx,
				pre_scenario_id, scenario_id);
		}
	}

	/* Update current mode to pre_mode. */
	dual_info->pre_mode = dual_info->mode;
}

unsigned int is_get_bit_count(unsigned long bits)
{
	unsigned int count = 0;

	while (bits) {
		bits &= (bits - 1);
		count++;
	}

	return count;
}
#endif
