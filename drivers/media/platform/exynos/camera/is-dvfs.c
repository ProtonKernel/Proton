/*
 * Samsung Exynos5 SoC series FIMC-IS driver
 *
 * exynos5 fimc-is core functions
 *
 * Copyright (c) 2011 Samsung Electronics Co., Ltd
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#include <linux/slab.h>
#if defined(CONFIG_SCHED_EHMP)
#include <linux/ehmp.h>
#elif defined(CONFIG_SCHED_EMS)
#include <linux/ems.h>
#endif
#include "is-core.h"
#include "is-dvfs.h"
#include "is-hw-dvfs.h"
#include <linux/videodev2_exynos_camera.h>

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

#if defined(CONFIG_SCHED_EHMP) || defined(CONFIG_SCHED_EMS)
#if defined(CONFIG_SCHED_EMS_TUNE)
extern struct emstune_mode_request emstune_req;
#else
extern struct gb_qos_request gb_req;
#endif
#endif

static inline int is_get_start_sensor_cnt(struct is_core *core)
{
	int i, sensor_cnt = 0;

	for (i = 0; i < IS_SENSOR_COUNT; i++)
		if (test_bit(IS_SENSOR_OPEN, &(core->sensor[i].state)))
			sensor_cnt++;

	return sensor_cnt;
}

#ifdef BDS_DVFS
static int is_get_bds_size(struct is_device_ischain *device, struct is_dual_info *dual_info)
{
	int resol = 0;
	struct is_group *group;

	group = &device->group_3aa;

	if (!test_bit(IS_GROUP_INIT, &group->state))
		return resol;

	if (dual_info->mode == IS_DUAL_MODE_SYNC)
		resol = dual_info->max_bds_width * dual_info->max_bds_height;
	else
		resol = device->txp.output.width * device->txp.output.height;

	return resol;
}
#else
static int is_get_target_resol(struct is_device_ischain *device)
{
	int resol = 0;
#ifdef SOC_MCS
	int i = 0;
	struct is_group *group;

	group = &device->group_mcs;

	if (!test_bit(IS_GROUP_INIT, &group->state))
		return resol;

	for (i = ENTRY_M0P; i <= ENTRY_M4P; i++)
		if (group->subdev[i] && test_bit(IS_SUBDEV_START, &(group->subdev[i]->state)))
			resol = max_t(int, resol, group->subdev[i]->output.width * group->subdev[i]->output.height);
#else
	resol = device->scp.output.width * device->scp.output.height;
#endif
	return resol;
}
#endif

bool is_dvfs_is_fast_ae(struct is_dvfs_ctrl *dvfs_ctrl)
{
	return (dvfs_ctrl->static_ctrl->cur_scenario_id != IS_SN_PREVIEW_HIGH_SPEED_FPS
			&& dvfs_ctrl->static_ctrl->cur_scenario_id != IS_SN_FRONT_PREVIEW_HIGH_SPEED_FPS
			&& dvfs_ctrl->dynamic_ctrl->cur_scenario_id == IS_SN_PREVIEW_HIGH_SPEED_FPS
			&& dvfs_ctrl->dynamic_ctrl->cur_frame_tick > 0);
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
	struct is_core *core;
	struct is_dvfs_ctrl *dvfs_ctrl;
	struct is_dvfs_scenario_ctrl *static_ctrl, *dynamic_ctrl;
	struct is_dvfs_scenario *scenarios;
	struct is_resourcemgr *resourcemgr;
	struct is_dual_info *dual_info;
	int i, scenario_id, scenario_cnt, streaming_cnt;
	int position, resol, fps, stream_cnt;
	unsigned long sensor_map;
	int sel;
	struct is_device_sensor *sensor;
	struct is_sensor_cfg *sensor_cfg;

	FIMC_BUG(!device);
	FIMC_BUG(!device->interface);
	FIMC_BUG(!device->sensor);

	core = (struct is_core *)device->interface->core;
	resourcemgr = device->resourcemgr;
	dvfs_ctrl = &(resourcemgr->dvfs_ctrl);
	static_ctrl = dvfs_ctrl->static_ctrl;
	dynamic_ctrl = dvfs_ctrl->dynamic_ctrl;

	if (!test_bit(IS_DVFS_SEL_TABLE, &dvfs_ctrl->state)) {
		err("dvfs table is NOT selected");
		return -EINVAL;
	}

	/* static scenario */
	if (!static_ctrl) {
		err("static_dvfs_ctrl is NULL");
		return -EINVAL;
	}

	if (static_ctrl->scenario_cnt == 0) {
		pr_debug("static_scenario's count is zero");
		return -EINVAL;
	}

	sensor = device->sensor;
	sensor_cfg = sensor->cfg;
	if (!sensor_cfg) {
		merr("sensor_cfg is NULL", device);
		return -EINVAL;
	}

	scenarios = static_ctrl->scenarios;
	scenario_cnt = static_ctrl->scenario_cnt;
	position = is_sensor_g_position(device->sensor);
	fps = sensor_cfg->max_fps;

	/*
	 * stream_cnt : number of open sensors
	 * streaming_cnt : number of sensors in operation
	 */
	stream_cnt = is_get_start_sensor_cnt(core);
	streaming_cnt = resourcemgr->streaming_cnt;
	sensor_map = core->sensor_map;
	dual_info = &core->dual_info;
#ifdef BDS_DVFS
	resol = is_get_bds_size(device, dual_info);
#else
	resol = is_get_target_resol(device);
#endif

	dbg_dvfs(1, "pos(%d), res(%d), fps(%d), sc(%d, %d), map(%d), dual(%d, %d, %d, %d, %d)(%d), setfile(%d)\n",
		device,	position, resol, fps, stream_cnt, streaming_cnt, sensor_map,
		dual_info->max_fps[SENSOR_POSITION_REAR],
		dual_info->max_fps[SENSOR_POSITION_REAR2],
		dual_info->max_fps[SENSOR_POSITION_REAR3],
		dual_info->max_fps[SENSOR_POSITION_FRONT],
		dual_info->max_fps[SENSOR_POSITION_REAR_TOF],
		dual_info->mode,
		(device->setfile & IS_SETFILE_MASK));

	for (i = 0; i < scenario_cnt; i++) {
		if (!scenarios[i].check_func) {
			warn("check_func[%d] is NULL\n", i);
			continue;
		}

		sel = scenarios[i].check_func(device, NULL, position, resol, fps,
			stream_cnt, streaming_cnt, sensor_map, dual_info);

		dbg_dvfs(1, "sel(%d) idx(%d) [%s]\n",
			device, sel, i, scenarios[i].scenario_nm);

		switch (sel) {
		case DVFS_MATCHED:
			scenario_id = scenarios[i].scenario_id;
			static_ctrl->cur_scenario_id = scenario_id;
			static_ctrl->cur_scenario_idx = i;
			static_ctrl->cur_frame_tick = scenarios[i].keep_frame_tick;
			/* use dynaimc control to improve launching time */
			if (scenario_id == IS_SN_PREVIEW_HIGH_SPEED_FPS
					|| scenario_id == IS_SN_FRONT_PREVIEW_HIGH_SPEED_FPS) {
				dynamic_ctrl->cur_scenario_id = IS_SN_PREVIEW_HIGH_SPEED_FPS;
				dynamic_ctrl->cur_frame_tick = scenarios[i].keep_frame_tick;
			}
			return scenario_id;
		case DVFS_SKIP:
			return -EAGAIN;
		case DVFS_NOT_MATCHED:
		default:
			continue;
		}
	}

	warn("couldn't find static dvfs scenario [sensor:(%d/%d)/fps:%d/setfile:%d/resol:(%d)]\n",
		is_get_start_sensor_cnt(core),
		device->sensor->pdev->id,
		fps, (device->setfile & IS_SETFILE_MASK), resol);

	static_ctrl->cur_scenario_id = IS_SN_DEFAULT;
	static_ctrl->cur_scenario_idx = -1;
	static_ctrl->cur_frame_tick = -1;

	return IS_SN_DEFAULT;
}

int is_dvfs_sel_dynamic(struct is_device_ischain *device, struct is_group *group,
	struct is_frame *frame)
{
	int ret;
	struct is_core *core;
	struct is_dvfs_ctrl *dvfs_ctrl;
	struct is_dvfs_scenario_ctrl *dynamic_ctrl;
	struct is_dvfs_scenario_ctrl *static_ctrl;
	struct is_dvfs_scenario *scenarios;
	struct is_resourcemgr *resourcemgr;
	struct is_dual_info *dual_info;
	int i, scenario_id, scenario_cnt;
	int position, resol, fps;
	unsigned long sensor_map;
	struct is_device_sensor *sensor;
	struct is_sensor_cfg *sensor_cfg;

	FIMC_BUG(!device);
	FIMC_BUG(!device->sensor);
	FIMC_BUG(!frame);

	core = (struct is_core *)device->interface->core;
	resourcemgr = device->resourcemgr;
	dvfs_ctrl = &(resourcemgr->dvfs_ctrl);
	dynamic_ctrl = dvfs_ctrl->dynamic_ctrl;
	static_ctrl = dvfs_ctrl->static_ctrl;

	if (static_ctrl->cur_frame_tick == IS_DVFS_SKIP_DYNAMIC)
		return 0;

	if (!test_bit(IS_DVFS_SEL_TABLE, &dvfs_ctrl->state)) {
		err("dvfs table is NOT selected");
		return -EINVAL;
	}

	/* dynamic scenario */
	if (!dynamic_ctrl) {
		err("dynamic_dvfs_ctrl is NULL");
		return -EINVAL;
	}

	if (dynamic_ctrl->scenario_cnt == 0) {
		pr_debug("dynamic_scenario's count is zero");
		return -EINVAL;
	}

	scenarios = dynamic_ctrl->scenarios;

	if (test_bit(IS_ISCHAIN_REPROCESSING, &device->state))
		scenario_cnt = dynamic_ctrl->scenario_cnt;
	else
		scenario_cnt = dynamic_ctrl->fixed_scenario_cnt;

	if (dynamic_ctrl->cur_frame_tick >= 0) {
		(dynamic_ctrl->cur_frame_tick)--;
		/*
		 * when cur_frame_tick is lower than 0, clear current scenario.
		 * This means that current frame tick to keep dynamic scenario
		 * was expired.
		 */
		if (dynamic_ctrl->cur_frame_tick < 0) {
			dynamic_ctrl->cur_scenario_id = -1;
			dynamic_ctrl->cur_scenario_idx = -1;
		}
	}

	if (group->id == GROUP_ID_VRA0)
		return -EAGAIN;

	sensor = device->sensor;
	sensor_cfg = sensor->cfg;
	if (!sensor_cfg) {
		merr("sensor_cfg is NULL", device);
		return -EINVAL;
	}

	position = is_sensor_g_position(device->sensor);
	fps = sensor_cfg->max_fps;
	sensor_map = core->sensor_map;
	dual_info = &core->dual_info;
#ifdef BDS_DVFS
	resol = is_get_bds_size(device, dual_info);
#else
	resol = is_get_target_resol(device);
#endif

	for (i = 0; i < scenario_cnt; i++) {
		if (!scenarios[i].check_func) {
			warn("check_func[%d] is NULL\n", i);
			continue;
		}

		ret = scenarios[i].check_func(device, frame->shot, position, resol, fps, 0, 0, sensor_map, dual_info);
		switch (ret) {
		case DVFS_MATCHED:
			scenario_id = scenarios[i].scenario_id;
			dynamic_ctrl->cur_scenario_id = scenario_id;
			dynamic_ctrl->cur_scenario_idx = i;
			dynamic_ctrl->cur_frame_tick = scenarios[i].keep_frame_tick;

			return scenario_id;
		case DVFS_SKIP:
			goto p_again;
		case DVFS_NOT_MATCHED:
		default:
			continue;
		}
	}

p_again:
	return  -EAGAIN;
}

int is_dvfs_sel_external(struct is_device_sensor *device)
{
	int ret;
	struct is_core *core;
	struct is_dvfs_ctrl *dvfs_ctrl;
	struct is_dvfs_scenario_ctrl *external_ctrl;
	struct is_dvfs_scenario *scenarios;
	struct is_resourcemgr *resourcemgr;
	struct is_dual_info *dual_info;
	int i, scenario_id, scenario_cnt;
	int position, resol, fps, stream_cnt, streaming_cnt;
	unsigned long sensor_map;

	FIMC_BUG(!device);

	core = device->private_data;
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

	scenarios = external_ctrl->scenarios;
	scenario_cnt = external_ctrl->scenario_cnt;
	position = is_sensor_g_position(device);
	resol = is_sensor_g_width(device) * is_sensor_g_height(device);
	fps = is_sensor_g_framerate(device);

	/*
	 * stream_cnt : number of open sensors
	 * streaming_cnt : number of sensors in operation
	 */
	stream_cnt = is_get_start_sensor_cnt(core);
	streaming_cnt = resourcemgr->streaming_cnt;
	sensor_map = core->sensor_map;
	dual_info = &core->dual_info;

	for (i = 0; i < scenario_cnt; i++) {
		if (!scenarios[i].ext_check_func) {
			warn("check_func[%d] is NULL\n", i);
			continue;
		}

		ret = scenarios[i].ext_check_func(device, position, resol, fps,
				stream_cnt, streaming_cnt, sensor_map, dual_info);
		switch (ret) {
		case DVFS_MATCHED:
			scenario_id = scenarios[i].scenario_id;
			external_ctrl->cur_scenario_id = scenario_id;
			external_ctrl->cur_scenario_idx = i;
			external_ctrl->cur_frame_tick = scenarios[i].keep_frame_tick;
			return scenario_id;
		case DVFS_SKIP:
			return -EAGAIN;
		case DVFS_NOT_MATCHED:
		default:
			continue;
		}
	}

	warn("couldn't find external dvfs scenario [sensor:(%d/%d/%d)/fps:%d/resol:(%d)]\n",
		stream_cnt, streaming_cnt, position, fps, resol);

	external_ctrl->cur_scenario_id = IS_SN_MAX;
	external_ctrl->cur_scenario_idx = -1;
	external_ctrl->cur_frame_tick = -1;

	return IS_SN_MAX;
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
	struct is_resourcemgr *resourcemgr;
	struct is_dvfs_ctrl *dvfs_ctrl;

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
			int_qos = THROTTLING_INT_LEVEL;
			if (dvfs_ctrl->thr_int_qos != int_qos) {
				is_set_int_dvfs(device, i2c_qos, int_qos);
				dvfs_ctrl->thr_int_qos = int_qos;
			}
		}
	} else {
		if (int_qos && dvfs_ctrl->cur_int_qos != int_qos) {
			is_set_int_dvfs(device, i2c_qos, int_qos);
			dvfs_ctrl->cur_int_qos = int_qos;
		}
	}
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
		if (dvfs_ctrl->thr_int_cam_qos != mif_qos) {
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

#if defined(CONFIG_HMP_VARIABLE_SCALE)
		/* for migration to big core */
		if (hpg_qos > 4) {
			if (!dvfs_ctrl->cur_hmp_bst) {
				set_hmp_boost(1);
				dvfs_ctrl->cur_hmp_bst = 1;
			}
		} else {
			if (dvfs_ctrl->cur_hmp_bst) {
				set_hmp_boost(0);
				dvfs_ctrl->cur_hmp_bst = 0;
			}
		}
#elif defined(CONFIG_SCHED_EHMP) || defined(CONFIG_SCHED_EMS)
		/* for migration to big core */
		if (hpg_qos > 4) {
			if (!dvfs_ctrl->cur_hmp_bst) {
#if defined(CONFIG_SCHED_EMS_TUNE)
				emstune_boost(&emstune_req, 1);
#else
				gb_qos_update_request(&gb_req, 100);
#endif
				dvfs_ctrl->cur_hmp_bst = 1;
			}
		} else {
			if (dvfs_ctrl->cur_hmp_bst) {
#if defined(CONFIG_SCHED_EMS_TUNE)
				emstune_boost(&emstune_req, 0);
#else
				gb_qos_update_request(&gb_req, 0);
#endif
				dvfs_ctrl->cur_hmp_bst = 0;
			}
		}
#endif
	}
#endif

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
	info("%s INT(%d), MIF(%d), CAM(%d), DISP(%d), I2C(%d), HPG(%d, %d)], L_FPS(%d)\n",
			qos_info, int_qos, mif_qos, cam_qos, disp_qos,
			i2c_qos, hpg_qos, dvfs_ctrl->cur_hmp_bst,
			resourcemgr->limited_fps);
	__putname(qos_info);
exit:
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

	if (group->head->device_type != IS_DEVICE_SENSOR)
		return;

	/* Update max fps of dual sensor device with reference to shot meta. */
	dual_info->max_fps[sensor->position] = frame->shot->ctl.aa.aeTargetFpsRange[1];

	/* Check the number of active sensors for all PIP sensor combinations */
	for (i = 0; i < SENSOR_POSITION_REAR_TOF; i++) {
		if (dual_info->max_fps[i] >= 10)
			streaming_cnt++;
	}

	resourcemgr = sensor->resourcemgr;
	resourcemgr->streaming_cnt = streaming_cnt;
	streaming_cnt = 0;

	/* Continue if wide and tele/s-wide complete is_sensor_s_input(). */
	if (!(test_bit(SENSOR_POSITION_REAR, &core->sensor_map) &&
		(test_bit(SENSOR_POSITION_REAR2, &core->sensor_map) ||
		 test_bit(SENSOR_POSITION_REAR3, &core->sensor_map))))
		return;

	/*
	 * bypass - master_max_fps : 30fps, slave_max_fps : 0fps (sensor standby)
	 * sync - master_max_fps : 30fps, slave_max_fps : 30fps (fusion)
	 * switch - master_max_fps : 5ps, slave_max_fps : 30fps (post standby)
	 * nothing - invalid mode
	 */
	for (i = 0; i < SENSOR_POSITION_REAR_TOF; i++) {
		if (dual_info->max_fps[i] >= 10)
			streaming_cnt++;
	}

	if (streaming_cnt == 1)
		dual_info->mode = IS_DUAL_MODE_BYPASS;
	else if (streaming_cnt >= 2)
		dual_info->mode = IS_DUAL_MODE_SYNC;
	else
		dual_info->mode = IS_DUAL_MODE_NOTHING;

	if (dual_info->mode == IS_DUAL_MODE_SYNC) {
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
		 test_bit(SENSOR_POSITION_REAR3, &core->sensor_map))))
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
		if (scenario_id >= 0 && scenario_id != pre_scenario_id && !is_dvfs_is_fast_ae(&resourcemgr->dvfs_ctrl)) {
			struct is_dvfs_scenario_ctrl *static_ctrl = resourcemgr->dvfs_ctrl.static_ctrl;

			mgrinfo("tbl[%d] dual static scenario(%d)-[%s]\n", device, group, frame,
				resourcemgr->dvfs_ctrl.dvfs_table_idx,
				static_ctrl->cur_scenario_id,
				static_ctrl->scenarios[static_ctrl->cur_scenario_idx].scenario_nm);
			is_set_dvfs((struct is_core *)device->interface->core, device, scenario_id);
		} else {
			dual_info->tick_count++;
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
