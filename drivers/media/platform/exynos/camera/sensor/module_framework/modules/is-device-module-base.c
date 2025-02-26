/*
 * Samsung Exynos5 SoC series Sensor driver
 *
 *
 * Copyright (c) 2011 Samsung Electronics Co., Ltd
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#include <linux/i2c.h>
#include <linux/slab.h>
#include <linux/irq.h>
#include <linux/interrupt.h>
#include <linux/delay.h>
#include <linux/version.h>
#include <linux/gpio.h>
#include <linux/clk.h>
#include <linux/regulator/consumer.h>
#include <linux/videodev2.h>
#include <linux/videodev2_exynos_camera.h>
#include <linux/module.h>
#include <linux/moduleparam.h>
#include <linux/platform_device.h>
#include <linux/of_gpio.h>
#include <asm/neon.h>
#include <media/v4l2-ctrls.h>
#include <media/v4l2-device.h>
#include <media/v4l2-subdev.h>

#include <exynos-is-sensor.h>
#include "is-hw.h"
#include "is-core.h"
#include "is-device-sensor.h"
#include "is-resourcemgr.h"
#include "is-dt.h"
#include "is-device-module-base.h"
#include "interface/is-interface-library.h"
#if defined(CONFIG_CAMERA_PAFSTAT)
#include "pafstat/is-pafstat.h"
#endif
#include "is-sec-define.h"
#include "is-vender-specific.h"

static void update_sensor_id_by_position(u32 position, u32 module_id)
{
	struct is_core *core = (struct is_core *)dev_get_drvdata(is_dev);
	struct is_vender_specific *priv;
	u32 *sensor_id;

	FIMC_BUG_VOID(!core);

	priv = core->vender.private_data;

	FIMC_BUG_VOID(!priv);

	switch (position) {
		case SENSOR_POSITION_REAR:
			sensor_id = &priv->rear_sensor_id;
			break;
		case SENSOR_POSITION_FRONT:
			sensor_id = &priv->front_sensor_id;
			break;
		case SENSOR_POSITION_REAR2:
			sensor_id = &priv->rear2_sensor_id;
			break;
		case SENSOR_POSITION_FRONT2:
			sensor_id = &priv->front2_sensor_id;
			break;
		case SENSOR_POSITION_REAR3:
			sensor_id = &priv->rear3_sensor_id;
			break;
		case SENSOR_POSITION_REAR4:
			sensor_id = &priv->rear4_sensor_id;
			break;
#if defined(SECURE_CAMERA_IRIS)
		case SENSOR_POSITION_SECURE:
			sensor_id = &priv->secure_sensor_id;
			break;
#endif
		case SENSOR_POSITION_REAR_TOF:
			sensor_id = &priv->rear_tof_sensor_id;
			break;
		case SENSOR_POSITION_FRONT_TOF:
			sensor_id = &priv->front_tof_sensor_id;
			break;
		default:
			err("invalid module position(%d)", position);
			goto p_err;
	}

	if (*sensor_id == 0)
		*sensor_id = module_id;
p_err:
	return;
}

static int get_sensor_by_model_id(struct v4l2_subdev *subdev_cis, int model_id)
{
	int sensor_id = -1;
	int i;
	struct is_device_sensor *device = NULL;

	FIMC_BUG(!subdev_cis);

	device = (struct is_device_sensor *)v4l2_get_subdev_hostdata(subdev_cis);
	FIMC_BUG(!device);

	for (i = 0; i < device->pdata->module_sel_cnt; i++) {
		if (model_id == device->pdata->module_sel_val[i]) {
			sensor_id = device->pdata->module_sel_sensor_id[i];
			break;
		}
	}

	if (i >= device->pdata->module_sel_cnt)
		warn("cannot find sensor by model id");

	return sensor_id;
}

int sensor_module_power_reset(struct v4l2_subdev *subdev, struct is_device_sensor *device)
{
	int ret = 0;
	struct is_module_enum *module;
	struct is_device_sensor_peri *sensor_peri = NULL;

	module = (struct is_module_enum *)v4l2_get_subdevdata(subdev);
	FIMC_BUG(!module);

	sensor_peri = (struct is_device_sensor_peri *)module->private_data;
	FIMC_BUG(!sensor_peri);

	ret = is_sensor_gpio_off(device);
	if (ret)
		err("gpio off is fail(%d)", ret);

	usleep_range(10000, 10000);

	sensor_peri->mode_change_first = true;
	sensor_peri->cis_global_complete = false;
	sensor_peri->check_auto_framing = false;

	ret = is_sensor_gpio_on(device);
	if (ret)
		err("gpio on is fail(%d)", ret);

	usleep_range(10000, 10000);

	return ret;
}

int sensor_module_init(struct v4l2_subdev *subdev, u32 val)
{
	int ret = 0;
	struct is_module_enum *module;
	struct is_device_sensor_peri *sensor_peri = NULL;
	struct exynos_platform_is_module *pdata = NULL;
	struct v4l2_subdev *subdev_cis = NULL;
	struct v4l2_subdev *subdev_actuator = NULL;
	struct v4l2_subdev *subdev_flash = NULL;
	struct v4l2_subdev *subdev_eeprom = NULL;
	struct v4l2_subdev *subdev_laser_af = NULL;
	struct is_device_sensor *device = NULL;
#ifdef USE_CAMERA_HW_BIG_DATA
	struct cam_hw_param *hw_param = NULL;
#endif
	int retry = 10;

	FIMC_BUG(!subdev);

	module = (struct is_module_enum *)v4l2_get_subdevdata(subdev);
	FIMC_BUG(!module);

	sensor_peri = (struct is_device_sensor_peri *)module->private_data;
	FIMC_BUG(!sensor_peri);

	memset(&sensor_peri->sensor_interface, 0, sizeof(struct is_sensor_interface));

	if (sensor_peri->actuator) {
		memset(&sensor_peri->actuator->pre_position, 0, sizeof(u32 [EXPECT_DM_NUM]));
		memset(&sensor_peri->actuator->pre_frame_cnt, 0, sizeof(u32 [EXPECT_DM_NUM]));
	}

	ret = init_sensor_interface(&sensor_peri->sensor_interface);
	if (ret) {
		err("failed in init_sensor_interface, return: %d", ret);
		goto p_err;
	}

	subdev_cis = sensor_peri->subdev_cis;
	FIMC_BUG(!subdev_cis);

	pdata = module->pdata;
	FIMC_BUG(!pdata);

	device = (struct is_device_sensor *)v4l2_get_subdev_hostdata(subdev_cis);
	FIMC_BUG(!device);

	ret = CALL_CISOPS(&sensor_peri->cis, cis_check_rev_on_init, subdev_cis);
	if (ret < 0) {
		if (device != NULL && ret == -EAGAIN) {
			err("Checking sensor revision is fail. So retry camera power sequence.");
			if (module->ext.use_retention_mode == SENSOR_RETENTION_ACTIVATED) {
				err("Retention is temporarily off during rev_check");
				module->ext.use_retention_mode = SENSOR_RETENTION_INACTIVE;
			}
			sensor_module_power_reset(subdev, device);
			ret = CALL_CISOPS(&sensor_peri->cis, cis_check_rev_on_init, subdev_cis);
			if (ret < 0) {
#ifdef USE_CAMERA_HW_BIG_DATA
				is_sec_get_hw_param(&hw_param, sensor_peri->module->position);
				if (hw_param)
					hw_param->i2c_sensor_err_cnt++;
#endif
				goto p_err;
			}
		}
	}

	ret = CALL_CISOPS(&sensor_peri->cis, cis_init, subdev_cis);
	if (ret) {
		err("v4l2_subdev_call(init) is fail(%d)", ret);
		goto p_err;
	}

	/* set initial ae setting if initial_ae feature is supported */
	ret = CALL_CISOPS(&sensor_peri->cis, cis_set_initial_exposure, subdev_cis);
	if (ret) {
		err("v4l2_subdev_call(set_initial_exposure) is fail(%d)", ret);
		goto p_err;
	}

	subdev_flash = sensor_peri->subdev_flash;
	if (subdev_flash != NULL) {
		ret = v4l2_subdev_call(subdev_flash, core, init, 0);
		if (ret) {
			err("v4l2_subdev_call(init) is fail(%d)", ret);
			goto p_err;
		}
	}

	subdev_eeprom = sensor_peri->subdev_eeprom;
	if (subdev_eeprom != NULL) {
		ret = CALL_EEPROMOPS(sensor_peri->eeprom, eeprom_read, subdev_eeprom);
		if (ret) {
			err("[%s] sensor eeprom read fail\n", __func__);
			ret = 0;
		}
	}

	subdev_laser_af = sensor_peri->subdev_laser_af;
	if (subdev_laser_af != NULL) {
		ret = v4l2_subdev_call(subdev_laser_af, core, init, 0);
		if (ret) {
			err("v4l2_subdev_call(init) is fail(%d)", ret);
			goto p_err;
		}
	}

	if (test_bit(IS_SENSOR_ACTUATOR_AVAILABLE, &sensor_peri->peri_state) &&
			pdata->af_product_name != ACTUATOR_NAME_NOTHING && sensor_peri->actuator != NULL) {

		sensor_peri->actuator->actuator_data.actuator_init = true;
		sensor_peri->actuator->actuator_index = -1;
		sensor_peri->actuator->left_x = 0;
		sensor_peri->actuator->left_y = 0;
		sensor_peri->actuator->right_x = 0;
		sensor_peri->actuator->right_y = 0;
		sensor_peri->actuator->actuator_data.afwindow_timer.function = is_actuator_m2m_af_set;

		subdev_actuator = sensor_peri->subdev_actuator;
		FIMC_BUG(!subdev_actuator);

		if (!sensor_peri->reuse_3a_value)
			sensor_peri->actuator->position = 0;

		ret = v4l2_subdev_call(subdev_actuator, core, init, 0);
		if (ret) {
			while (--retry && ret) {
				msleep(100);
				err("v4l2_actuator_call(init) is fail(%d) retry again! (%d)", ret, retry);
				ret = v4l2_subdev_call(subdev_actuator, core, init, 0);
			}

			if (retry == 0) {
				err("v4l2_actuator_call(init) is fail(%d) retry(%d)", ret, retry);
				goto p_err;
			}
		}
	}

	if (device->pdata->scenario == SENSOR_SCENARIO_G_ACTIVE_CAMERA) {
		ret = CALL_CISOPS(&sensor_peri->cis, cis_active_test, subdev_cis);
		if (ret) {
			err("cis_active_test is fail(%d)", ret);
			goto p_err;
		}
	}

	/* If use CIS_GLOBAL_WORK feature,
	 * cis global setting need to start after other peri initialize finished
	 */
	if (IS_ENABLED(USE_CIS_GLOBAL_WORK) && device->pdata->scenario == SENSOR_SCENARIO_NORMAL)
		schedule_work(&sensor_peri->cis.global_setting_work);

	pr_info("[MOD:%s] %s(%d)\n", module->sensor_name, __func__, val);

p_err:
	return ret;
}

int sensor_module_deinit(struct v4l2_subdev *subdev)
{
	int ret = 0;
	struct is_module_enum *module = NULL;
	struct is_device_sensor_peri *sensor_peri = NULL;

	FIMC_BUG(!subdev);

	module = (struct is_module_enum *)v4l2_get_subdevdata(subdev);
	FIMC_BUG(!module);

	sensor_peri = (struct is_device_sensor_peri *)module->private_data;

	is_sensor_deinit_sensor_thread(sensor_peri);

	cancel_work_sync(&sensor_peri->cis.global_setting_work);
	cancel_work_sync(&sensor_peri->cis.mode_setting_work);

	if (sensor_peri->subdev_cis) {
		CALL_CISOPS(&sensor_peri->cis, cis_deinit, sensor_peri->subdev_cis);
	}

#if defined(CONFIG_CAMERA_USE_INTERNAL_MCU)
	if (sensor_peri->mcu && sensor_peri->mcu->ois) {
		ret = CALL_OISOPS(sensor_peri->mcu->ois, ois_deinit, sensor_peri->subdev_mcu);
		if (ret < 0) {
			err("v4l2_subdev_call(ois_deinit) is fail(%d)", ret);
		}
	}
#endif

	if (sensor_peri->flash != NULL) {
		sensor_peri->flash->flash_data.mode = CAM2_FLASH_MODE_OFF;
		if (sensor_peri->flash->flash_data.flash_fired == true) {
			ret = is_sensor_flash_fire(sensor_peri, 0);
			if (ret) {
				err("failed to turn off flash at flash expired handler\n");
			}
		}
	}

#if 0 // No need to vendor code for specific actuator_softlanding
	if (sensor_peri->actuator) {
		ret = is_sensor_peri_actuator_softlanding(sensor_peri);
		if (ret)
			err("failed to soft landing control of actuator driver\n");
	}
#endif

	if (sensor_peri->flash != NULL) {
		cancel_work_sync(&sensor_peri->flash->flash_data.flash_fire_work);
		cancel_work_sync(&sensor_peri->flash->flash_data.flash_expire_work);
	}
	cancel_work_sync(&sensor_peri->cis.cis_status_dump_work);

	clear_bit(IS_SENSOR_PREPROCESSOR_AVAILABLE, &sensor_peri->peri_state);
	clear_bit(IS_SENSOR_ACTUATOR_AVAILABLE, &sensor_peri->peri_state);
	clear_bit(IS_SENSOR_FLASH_AVAILABLE, &sensor_peri->peri_state);
	clear_bit(IS_SENSOR_OIS_AVAILABLE, &sensor_peri->peri_state);
	clear_bit(IS_SENSOR_LASER_AF_AVAILABLE, &sensor_peri->peri_state);

	pr_info("[MOD:%s] %s\n", module->sensor_name, __func__);

	return ret;
}

int sensor_module_g_ctrl(struct v4l2_subdev *subdev, struct v4l2_control *ctrl)
{
	int ret = 0;
	struct is_module_enum *module = NULL;
	struct is_device_sensor *device = NULL;
	struct is_device_sensor_peri *sensor_peri = NULL;
	cis_setting_info info;
	info.param = NULL;
	info.return_value = 0;

	FIMC_BUG(!subdev);

	module = (struct is_module_enum *)v4l2_get_subdevdata(subdev);
	FIMC_BUG(!module);

	device = (struct is_device_sensor *)v4l2_get_subdev_hostdata(subdev);
	FIMC_BUG(!device);

	sensor_peri = (struct is_device_sensor_peri *)module->private_data;
	FIMC_BUG(!sensor_peri);

	switch(ctrl->id) {
	case V4L2_CID_SENSOR_ADJUST_FRAME_DURATION:
		/* TODO: v4l2 g_ctrl cannot support adjust function */
#if 0
		info.param = (void *)&ctrl->value;
		ret = CALL_CISOPS(&sensor_peri->cis, cis_adjust_frame_duration, sensor_peri->subdev_cis, &info);
		if (ret < 0 || info.return_value == 0) {
			err("err!!! ret(%d), frame duration(%d)", ret, info.return_value);
			ctrl->value = 0;
			ret = -EINVAL;
			goto p_err;
		}
		ctrl->value = info.return_value;
#endif
		break;
	case V4L2_CID_SENSOR_GET_MIN_EXPOSURE_TIME:
		ret = CALL_CISOPS(&sensor_peri->cis, cis_get_min_exposure_time, sensor_peri->subdev_cis, &ctrl->value);
		if (ret < 0) {
			err("err!!! ret(%d), min exposure time(%d)", ret, info.return_value);
			ret = -EINVAL;
			goto p_err;
		}
		break;
	case V4L2_CID_SENSOR_GET_MAX_EXPOSURE_TIME:
		ret = CALL_CISOPS(&sensor_peri->cis, cis_get_max_exposure_time, sensor_peri->subdev_cis, &ctrl->value);
		if (ret < 0) {
			err("err!!! ret(%d)", ret);
			ctrl->value = 0;
			ret = -EINVAL;
			goto p_err;
		}
		break;
	case V4L2_CID_SENSOR_GET_PD_VALUE:
		ret = CALL_CISOPS(&sensor_peri->cis, cis_get_laser_photo_diode, sensor_peri->subdev_cis, (u16*)&ctrl->value);
		info("%s value :%d",__func__,ctrl->value);
		if (ret < 0) {
			err("err!!! ret(%d)", ret);
			ret = -EINVAL;
			goto p_err;
		}
		break;
	case V4L2_CID_SENSOR_ADJUST_ANALOG_GAIN:
		/* TODO: v4l2 g_ctrl cannot support adjust function */
#if 0
		info.param = (void *)&ctrl->value;
		ret = CALL_CISOPS(&sensor_peri->cis, cis_adjust_analog_gain, sensor_peri->subdev_cis, &info);
		if (ret < 0 || info.return_value == 0) {
			err("err!!! ret(%d), adjust analog gain(%d)", ret, info.return_value);
			ctrl->value = 0;
			ret = -EINVAL;
			goto p_err;
		}
		ctrl->value = info.return_value;
#endif
		break;
	case V4L2_CID_SENSOR_GET_ANALOG_GAIN:
		ret = CALL_CISOPS(&sensor_peri->cis, cis_get_analog_gain, sensor_peri->subdev_cis, &ctrl->value);
		if (ret < 0) {
			err("err!!! ret(%d)", ret);
			ret = -EINVAL;
			goto p_err;
		}
		break;
	case V4L2_CID_SENSOR_GET_MIN_ANALOG_GAIN:
		/* index 1 means caculated gain value per mile */
		if (sensor_peri->cis.cis_data->min_analog_gain[1]) {
			ctrl->value = sensor_peri->cis.cis_data->min_analog_gain[1];
		} else {
			ret = CALL_CISOPS(&sensor_peri->cis, cis_get_min_analog_gain,
					sensor_peri->subdev_cis, &ctrl->value);
			if (ret < 0) {
				err("err!!! ret(%d)", ret);
				ret = -EINVAL;
				goto p_err;
			}
		}
		break;
	case V4L2_CID_SENSOR_GET_MAX_ANALOG_GAIN:
		/* index 1 means caculated gain value per mile */
		if (sensor_peri->cis.cis_data->max_analog_gain[1]) {
			ctrl->value = sensor_peri->cis.cis_data->max_analog_gain[1];
		} else {
			ret = CALL_CISOPS(&sensor_peri->cis, cis_get_max_analog_gain,
					sensor_peri->subdev_cis, &ctrl->value);
			if (ret < 0) {
				err("err!!! ret(%d)", ret);
				ret = -EINVAL;
				goto p_err;
			}
		}
		break;
	case V4L2_CID_SENSOR_GET_DIGITAL_GAIN:
		ret = CALL_CISOPS(&sensor_peri->cis, cis_get_digital_gain, sensor_peri->subdev_cis, &ctrl->value);
		if (ret < 0) {
			err("err!!! ret(%d)", ret);
			ret = -EINVAL;
			goto p_err;
		}
		break;
	case V4L2_CID_SENSOR_GET_MIN_DIGITAL_GAIN:
		/* index 1 means caculated gain value per mile */
		if (sensor_peri->cis.cis_data->min_digital_gain[1]) {
			ctrl->value = sensor_peri->cis.cis_data->min_digital_gain[1];
		} else {
			ret = CALL_CISOPS(&sensor_peri->cis, cis_get_min_digital_gain,
					sensor_peri->subdev_cis, &ctrl->value);
			if (ret < 0) {
				err("err!!! ret(%d)", ret);
				ret = -EINVAL;
				goto p_err;
			}
		}
		break;
	case V4L2_CID_SENSOR_GET_MAX_DIGITAL_GAIN:
		/* index 1 means caculated gain value per mile */
		if (sensor_peri->cis.cis_data->max_digital_gain[1]) {
			ctrl->value = sensor_peri->cis.cis_data->max_digital_gain[1];
		} else {
			ret = CALL_CISOPS(&sensor_peri->cis, cis_get_max_digital_gain,
					sensor_peri->subdev_cis, &ctrl->value);
			if (ret < 0) {
				err("err!!! ret(%d)", ret);
				ret = -EINVAL;
				goto p_err;
			}
		}
		break;
	case V4L2_CID_ACTUATOR_GET_STATUS:
		ret = v4l2_subdev_call(sensor_peri->subdev_actuator, core, ioctl, SENSOR_IOCTL_ACT_G_CTRL, ctrl);
		if (ret) {
			err("[MOD:%s] v4l2_subdev_call(g_ctrl, id:%d) is fail(%d)",
					module->sensor_name, ctrl->id, ret);
			goto p_err;
		}
		break;
	case V4L2_CID_SENSOR_GET_SSM_THRESHOLD:
		ret = CALL_CISOPS(&sensor_peri->cis, cis_get_super_slow_motion_threshold,
				sensor_peri->subdev_cis, &ctrl->value);
		if (ret < 0) {
			err("err!!! ret(%d)", ret);
			ret = -EINVAL;
			goto p_err;
		}
		break;
	case V4L2_CID_SENSOR_GET_SSM_GMC:
		ret = CALL_CISOPS(&sensor_peri->cis, cis_get_super_slow_motion_gmc,
				sensor_peri->subdev_cis, &ctrl->value);
		if (ret < 0) {
			err("err!!! ret(%d)", ret);
			ret = -EINVAL;
			goto p_err;
		}
		break;
	case V4L2_CID_EEPROM_GET_SENSOR_ID:
		ret = CALL_EEPROMOPS(sensor_peri->eeprom, eeprom_get_sensor_id,
			sensor_peri->subdev_eeprom, &ctrl->value);
		if (ret < 0) {
			err("err!!! ret(%d)", ret);
			ret = -EINVAL;
			goto p_err;
		}
		break;
	case V4L2_CID_SENSOR_GET_SSM_FRAMEID:
		ret = CALL_CISOPS(&sensor_peri->cis, cis_get_super_slow_motion_frame_id,
				sensor_peri->subdev_cis, &ctrl->value);
		if (ret < 0) {
			err("err!!! ret(%d)", ret);
			ret = -EINVAL;
			goto p_err;
		}
		break;
	case V4L2_CID_SENSOR_GET_SSM_MD_THRESHOLD:
		ret = CALL_CISOPS(&sensor_peri->cis, cis_get_super_slow_motion_md_threshold,
				sensor_peri->subdev_cis, &ctrl->value);
		if (ret < 0) {
			err("err!!! ret(%d)", ret);
			ret = -EINVAL;
			goto p_err;
		}
		break;
	case V4L2_CID_SENSOR_GET_SSM_FLICKER:
		ret = CALL_CISOPS(&sensor_peri->cis, cis_get_super_slow_motion_flicker,
				sensor_peri->subdev_cis, &ctrl->value);
		if (ret < 0) {
			err("err!!! ret(%d)", ret);
			ret = -EINVAL;
			goto p_err;
		}
		break;
	case V4L2_CID_SENSOR_GET_MODEL_ID:
		ret = CALL_CISOPS(&sensor_peri->cis, cis_check_model_id,
			sensor_peri->subdev_cis);
		if (ret < 0) {
			err("err!!! ret(%d)", ret);
			ret = -EINVAL;
			goto p_err;
		}
		ctrl->value = get_sensor_by_model_id(sensor_peri->subdev_cis,
					sensor_peri->cis.cis_data->cis_model_id);
		break;
	case V4L2_CID_SENSOR_GET_BINNING_RATIO:
		if (!device->cfg) {
			err("get sensor_cfg is fail");
			ret = -EINVAL;
			goto p_err;
		}
		ret = CALL_CISOPS(&sensor_peri->cis, cis_get_binning_ratio,
				sensor_peri->subdev_cis, device->cfg->mode, &ctrl->value);
		if (ret < 0) {
			err("err!!! ret(%d)", ret);
			ret = -EINVAL;
			goto p_err;
		}
		break;
	default:
		err("err!!! Unknown CID(%#x)", ctrl->id);
		ret = -EINVAL;
		goto p_err;
	}

p_err:
	return ret;
}

int sensor_module_s_ctrl(struct v4l2_subdev *subdev, struct v4l2_control *ctrl)
{
	int ret = 0;
	struct is_device_sensor *device = NULL;
	struct is_module_enum *module = NULL;
	struct is_device_sensor_peri *sensor_peri = NULL;
	struct ae_param ae_val;

	FIMC_BUG(!subdev);

	module = (struct is_module_enum *)v4l2_get_subdevdata(subdev);
	FIMC_BUG(!module);

	device = (struct is_device_sensor *)v4l2_get_subdev_hostdata(subdev);
	FIMC_BUG(!device);

	sensor_peri = (struct is_device_sensor_peri *)module->private_data;
	FIMC_BUG(!sensor_peri);

	switch(ctrl->id) {
	case V4L2_CID_SENSOR_SET_AE_TARGET:
		ae_val.val = ae_val.short_val = ctrl->value;
		/* long_exposure_time and short_exposure_time is same value */
		ret = is_sensor_peri_s_exposure_time(device, ae_val);
		if (ret < 0) {
			err("failed to set exposure time : %d\n - %d",
					ctrl->value, ret);
			goto p_err;
		}
		break;
	case V4L2_CID_SENSOR_SET_FRAME_RATE:
		ret = CALL_CISOPS(&sensor_peri->cis, cis_set_frame_rate, sensor_peri->subdev_cis, ctrl->value);
		if (ret < 0) {
			err("err!!! ret(%d)", ret);
			goto p_err;
		}
		break;
	case V4L2_CID_SENSOR_SET_FRAME_DURATION:
		ret = is_sensor_peri_s_frame_duration(device, ctrl->value);
		if (ret < 0) {
			err("failed to set frame duration : %d\n - %d",
					ctrl->value, ret);
			goto p_err;
		}
		break;
	case V4L2_CID_SENSOR_SET_GAIN:
	case V4L2_CID_SENSOR_SET_ANALOG_GAIN:
	if (sensor_peri->cis.cis_data->analog_gain[1] != ctrl->value) {
		/* long_analog_gain and short_analog_gain is same value */
		ae_val.val = ae_val.short_val = ctrl->value;
		ret = is_sensor_peri_s_analog_gain(device, ae_val);
		if (ret < 0) {
			err("failed to set analog gain : %d\n - %d",
					ctrl->value, ret);
			goto p_err;
		}
		break;
	}
	case V4L2_CID_SENSOR_SET_DIGITAL_GAIN:
		/* long_digital_gain and short_digital_gain is same value */
		ae_val.val = ae_val.short_val = ctrl->value;
		ret = is_sensor_peri_s_digital_gain(device, ae_val);
		if (ret < 0) {
			err("failed to set digital gain : %d\n - %d",
					ctrl->value, ret);
			goto p_err;
		}
		break;
	case V4L2_CID_ACTUATOR_SET_POSITION:
		if (sensor_peri->mcu && sensor_peri->mcu->ois) {
			ret = CALL_OISOPS(sensor_peri->mcu->ois, ois_shift_compensation, sensor_peri->subdev_mcu, ctrl->value,
								sensor_peri->actuator->pos_size_bit);
			if (ret < 0) {
				err("ois shift compensation fail");
				goto p_err;
			}
		}

		ret = v4l2_subdev_call(sensor_peri->subdev_actuator, core, ioctl, SENSOR_IOCTL_ACT_S_CTRL, ctrl);
		if (ret < 0) {
			err("[MOD:%s] v4l2_subdev_call(s_ctrl, id:%d) is fail(%d)",
					module->sensor_name, ctrl->id, ret);
			goto p_err;
		}
		break;
	case V4L2_CID_FLASH_SET_CAL_EN:
	case V4L2_CID_FLASH_SET_BY_CAL_CH0:
	case V4L2_CID_FLASH_SET_BY_CAL_CH1:
	case V4L2_CID_FLASH_SET_INTENSITY:
	case V4L2_CID_FLASH_SET_FIRING_TIME:
		ret = v4l2_subdev_call(sensor_peri->subdev_flash, core, ioctl, SENSOR_IOCTL_FLS_S_CTRL, ctrl);
		if (ret) {
			err("[MOD:%s] v4l2_subdev_call(s_ctrl, id:%d) is fail(%d)",
					module->sensor_name, ctrl->id, ret);
			goto p_err;
		}
		break;
	case V4L2_CID_FLASH_SET_MODE:
		if (ctrl->value < CAM2_FLASH_MODE_OFF || ctrl->value > CAM2_FLASH_MODE_BEST) {
			err("failed to flash set mode: %d, \n", ctrl->value);
			ret = -EINVAL;
			goto p_err;
		}
		if (sensor_peri->flash->flash_data.mode != ctrl->value) {
			ret = is_sensor_flash_fire(sensor_peri, 0);
			if (ret) {
				err("failed to flash fire: %d\n", ctrl->value);
				ret = -EINVAL;
				goto p_err;
			}
			sensor_peri->flash->flash_data.mode = ctrl->value;
		}
		break;
	case V4L2_CID_FLASH_SET_FIRE:
		ret = is_sensor_flash_fire(sensor_peri, ctrl->value);
		if (ret) {
			err("failed to flash fire: %d\n", ctrl->value);
			ret = -EINVAL;
			goto p_err;
		}
		break;
	case V4L2_CID_SENSOR_SET_FRS_CONTROL:
		ret = CALL_CISOPS(&sensor_peri->cis, cis_set_frs_control,
			sensor_peri->subdev_cis, ctrl->value);
		if (ret < 0) {
			err("failed to control super slow motion: %d\n", ctrl->value);
			goto p_err;
		}
		break;
	case V4L2_CID_SENSOR_SET_SSM_THRESHOLD:
		ret = CALL_CISOPS(&sensor_peri->cis, cis_set_super_slow_motion_threshold,
			sensor_peri->subdev_cis, ctrl->value);
		if (ret < 0) {
			err("failed to control super slow motion: %d\n", ctrl->value);
			goto p_err;
		}
		break;
	case V4L2_CID_SENSOR_SET_SSM_FLICKER:
		ret = CALL_CISOPS(&sensor_peri->cis, cis_set_super_slow_motion_flicker,
			sensor_peri->subdev_cis, ctrl->value);
		if (ret < 0) {
			err("failed to control super slow motion: %d\n", ctrl->value);
			goto p_err;
		}
		break;
	case V4L2_CID_SENSOR_SET_SSM_GMC_TABLE_IDX:
		ret = CALL_CISOPS(&sensor_peri->cis, cis_set_super_slow_motion_gmc_table_idx,
			sensor_peri->subdev_cis, ctrl->value);
		if (ret < 0) {
			err("failed to control super slow motion: %d\n", ctrl->value);
			goto p_err;
		}
		break;
	case V4L2_CID_SENSOR_SET_SSM_GMC_BLOCK:
		ret = CALL_CISOPS(&sensor_peri->cis, cis_set_super_slow_motion_gmc_block_with_md_low,
			sensor_peri->subdev_cis, ctrl->value);
		if (ret < 0) {
			err("failed to control super slow motion: %d\n", ctrl->value);
			goto p_err;
		}
		break;
	case V4L2_CID_SENSOR_SET_FACTORY_CONTROL:
		ret = CALL_CISOPS(&sensor_peri->cis, cis_set_factory_control, sensor_peri->subdev_cis, ctrl->value);
		if (ret < 0) {
			err("failed to factory control: %d\n", ctrl->value);
			goto p_err;
		}
		break;
#if 0 // TO DO
	case V4L2_CID_SENSOR_SET_LASER_CONTORL:
		if (sensor_peri->laser_af) {
			if (ctrl->value)
				set_bit(IS_SENSOR_LASER_AF_AVAILABLE, &sensor_peri->peri_state);
			else
				clear_bit(IS_SENSOR_LASER_AF_AVAILABLE, &sensor_peri->peri_state);

			info("use laser_af: %d\n", ctrl->value);
		} else {
			ret = CALL_CISOPS(&sensor_peri->cis, cis_set_laser_control, sensor_peri->subdev_cis, ctrl->value);
			if (ret < 0) {
				err("failed to laser control: %d\n", ctrl->value);
				goto p_err;
			}
		}
		break;
#endif
	case V4L2_CID_SENSOR_SET_LASER_MODE:
		ret = CALL_CISOPS(&sensor_peri->cis, cis_set_laser_mode, sensor_peri->subdev_cis, ctrl->value);
		if (ret < 0) {
			err("failed to laser control: %d\n", ctrl->value);
			goto p_err;
		}
		break;
	case V4L2_CID_SENSOR_SET_SHUTTER:
		{
			struct ae_param exposure;
			exposure.long_val = 0;
			exposure.short_val = ctrl->value;
			ret = CALL_CISOPS(&sensor_peri->cis, cis_set_exposure_time, sensor_peri->subdev_cis, &exposure);
			if (ret < 0) {
				err("failed to set_exposure_time: %d\n", ctrl->value);
				goto p_err;
			}
		}
		break;
	case V4L2_CID_IS_HW_SYNC_CAMERA:
		if (ctrl->value >= AA_SENSORPLACE_END) {
			err("wrong value for hw sync:%d\n", ctrl->value);
			goto p_err;
		}
		sensor_peri->cis.dual_sync_mode =
			ctrl->value == module->position ? DUAL_SYNC_MASTER : DUAL_SYNC_SLAVE;
		info("[MOD:%s] Dual sync mode set to %s", module->sensor_name,
			sensor_peri->cis.dual_sync_mode == DUAL_SYNC_MASTER ? "Master" : "Slave");
		break;
	case V4L2_CID_SENSOR_SET_AUTO_FRAMING:
		sensor_peri->check_auto_framing = ctrl->value;
		info("%s Auto framing set. val = %d, sensor id = %d", __func__,
			sensor_peri->check_auto_framing, sensor_peri->module->sensor_id);
		break;
	default:
		err("err!!! Unknown CID(%#x)", ctrl->id);
		ret = -EINVAL;
		goto p_err;
	}

p_err:
	return ret;
}

int sensor_module_g_ext_ctrls(struct v4l2_subdev *subdev, struct v4l2_ext_controls *ctrls)
{
	int ret = 0;
	struct is_module_enum *module;

	FIMC_BUG(!subdev);

	module = (struct is_module_enum *)v4l2_get_subdevdata(subdev);

	/* TODO */
	pr_info("[MOD:%s] %s Not implemented\n", module->sensor_name, __func__);

	return ret;
}

int sensor_module_s_ext_ctrls(struct v4l2_subdev *subdev, struct v4l2_ext_controls *ctrls)
{
	int ret = 0;
	int i;
	struct is_module_enum *module;
	struct v4l2_ext_control *ext_ctrl;
	struct v4l2_control ctrl;
	struct is_device_sensor_peri *sensor_peri = NULL;
	struct v4l2_rect ssm_roi;
	struct is_device_sensor *device;
	struct is_core *core;

	FIMC_BUG(!subdev);
	FIMC_BUG(!ctrls);

	module = (struct is_module_enum *)v4l2_get_subdevdata(subdev);
	sensor_peri = (struct is_device_sensor_peri *)module->private_data;
	WARN_ON(!sensor_peri);

	device = (struct is_device_sensor *)v4l2_get_subdev_hostdata(subdev);
	WARN_ON(!device);

	core = device->private_data;
	WARN_ON(!core);

	for (i = 0; i < ctrls->count; i++) {
		ext_ctrl = (ctrls->controls + i);

		switch (ext_ctrl->id) {
		case V4L2_CID_SENSOR_SET_MODE_CHANGE:
		{
			struct seamless_mode_change_info mode_change;
			ret = copy_from_user(&mode_change, ext_ctrl->ptr, sizeof(struct seamless_mode_change_info));
			if (ret) {
				err("copy_from_user of seamless_mode_change_info is fail(%d)", ret);
				goto p_err;
			}

			ret = is_sensor_peri_s_mode_change(device, &mode_change);
			if (ret < 0) {
				err("failed to set mode change : %d\n - %d",
						device->ex_mode, ret);
				goto p_err;
			}
			break;
		}
		case V4L2_CID_SENSOR_SET_SSM_ROI:
			ret = copy_from_user(&ssm_roi, ext_ctrl->ptr, sizeof(struct v4l2_rect));
			if (ret) {
				err("fail to copy_from_user, ret(%d)\n", ret);
				goto p_err;
			}

			ret = CALL_CISOPS(&sensor_peri->cis, cis_set_super_slow_motion_roi,
				sensor_peri->subdev_cis, &ssm_roi);
			if (ret < 0) {
				err("failed to set super slow motion roi, ret(%d)\n", ret);
				goto p_err;
			}
			break;

		case V4L2_CID_SENSOR_SET_SSM_DEBUG_CONTROL:
			ret = copy_from_user(&ssm_roi, ext_ctrl->ptr, sizeof(struct v4l2_rect));
			if (ret) {
				err("fail to copy_from_user, ret(%d)\n", ret);
				goto p_err;
			}

			ret = CALL_CISOPS(&sensor_peri->cis, cis_set_super_slow_motion_setting,
				sensor_peri->subdev_cis, &ssm_roi);
			if (ret < 0) {
				err("failed to set super slow motion setting, ret(%d)\n", ret);
				goto p_err;
			}
			break;

		case V4L2_CID_IS_GET_DUAL_CAL:
		{
#ifdef CONFIG_CAMERA_VENDER_MCD
			char *dual_cal = NULL;
			int cal_size = 0;
			int rom_type;
			int rom_dualcal_id;
			int rom_dualcal_index;
			struct is_rom_info *finfo = NULL;

			is_vendor_get_rom_dualcal_info_from_position(device->position, &rom_type, &rom_dualcal_id, &rom_dualcal_index);
			if (rom_type == ROM_TYPE_NONE) {
				err("[rom_dualcal_id:%d pos:%d] not support, no rom for camera", rom_dualcal_id, device->position);
				return -EINVAL;
			} else if (rom_dualcal_id == ROM_ID_NOTHING) {
				err("[rom_dualcal_id:%d pos:%d] invalid ROM ID", rom_dualcal_id, device->position);
				return -EINVAL;
			}

			is_sec_get_sysfs_finfo(&finfo, rom_dualcal_id);
			if (test_bit(IS_CRC_ERROR_ALL_SECTION, &finfo->crc_error) ||
				test_bit(IS_CRC_ERROR_DUAL_CAMERA, &finfo->crc_error)) {
				err("[rom_dualcal_id:%d pos:%d] ROM Cal CRC is wrong. Cannot load dual cal.",
					rom_dualcal_id, device->position);
				return -EINVAL;
			}

			if (finfo->header_ver[FW_VERSION_INFO] >= 'B') {
				ret = is_get_dual_cal_buf(device->position, &dual_cal, &cal_size);
				if (ret == 0) {
					info("dual cal[%d] : ver[%d]", device->position, *((s32 *)dual_cal));
					ret = copy_to_user(ext_ctrl->ptr, dual_cal, cal_size);
					if (ret) {
						err("failed copying %d bytes of data\n", ret);
						ret = -EINVAL;
						goto p_err;
					}
				} else {
					err("failed to is_get_dual_cal_buf : %d\n", ret);
					ret = -EINVAL;
					goto p_err;
				}
			} else {
				err("module version is %c.", finfo->header_ver[FW_VERSION_INFO]);
				ret = -EINVAL;
				goto p_err;
			}
#endif
			break;
		}
		case V4L2_CID_SENSOR_SET_TOF_AUTO_FOCUS:
		{
#ifdef USE_TOF_AF
			struct is_vender *vender;

			vender = &core->vender;
			is_vender_store_af(vender, (struct tof_data_t*)ext_ctrl->ptr);
#endif
			break;
		}

		default:
			ctrl.id = ext_ctrl->id;
			ctrl.value = ext_ctrl->value;

			ret = sensor_module_s_ctrl(subdev, &ctrl);
			if (ret) {
				err("v4l2_s_ctrl is fail(%d)\n", ret);
				goto p_err;
			}
			break;
		}
	}

p_err:
	return ret;
}

long sensor_module_ioctl(struct v4l2_subdev *subdev, unsigned int cmd, void *arg)
{
	int ret = 0;
	struct v4l2_control *ctrl;
	struct v4l2_ext_controls *ext_ctrl;

	FIMC_BUG(!subdev);

	switch(cmd) {
	case V4L2_CID_SENSOR_DEINIT:
		ret = sensor_module_deinit(subdev);
		if (ret) {
			err("err!!! ret(%d), sensor module deinit fail", ret);
			goto p_err;
		}
		break;
	case V4L2_CID_SENSOR_NOTIFY_VSYNC:
		ret = is_sensor_peri_notify_vsync(subdev, arg);
		if (ret) {
			err("err!!! ret(%d), sensor notify vsync fail", ret);
			goto p_err;
		}
		break;
	case V4L2_CID_SENSOR_NOTIFY_VBLANK:
		ret = is_sensor_peri_notify_vblank(subdev, arg);
		if (ret) {
			err("err!!! ret(%d), sensor notify vblank fail", ret);
			goto p_err;
		}
		break;
	case V4L2_CID_SENSOR_NOTIFY_FLASH_FIRE:
		/* Do not use */
		break;
	case V4L2_CID_SENSOR_NOTIFY_ACTUATOR:
		ret = is_sensor_peri_notify_actuator(subdev, arg);
		if (ret) {
			err("err!!! ret(%d), sensor notify actuator fail", ret);
			goto p_err;
		}
		break;
	case V4L2_CID_SENSOR_NOTIFY_M2M_ACTUATOR:
		ret = is_actuator_notify_m2m_actuator(subdev);
		if (ret) {
			err("err!!! ret(%d), sensor notify M2M actuator fail", ret);
			goto p_err;
		}
		break;
	case V4L2_CID_SENSOR_NOTIFY_ACTUATOR_INIT:
		ret = is_sensor_peri_notify_actuator_init(subdev);
		if (ret) {
			err("err!!! ret(%d), actuator init fail\n", ret);
			goto p_err;
		}
		break;
	case SENSOR_IOCTL_MOD_S_CTRL:
		ctrl = (struct v4l2_control *)arg;
		ret = sensor_module_s_ctrl(subdev, ctrl);
		if (ret) {
			err("err!! ret(%d), sensor_module_s_ctrl fail", ret);
			goto p_err;
		}
		break;
	case SENSOR_IOCTL_MOD_G_CTRL:
		ctrl = (struct v4l2_control *)arg;
		ret = sensor_module_g_ctrl(subdev, ctrl);
		if (ret) {
			err("err!! ret(%d), sensor_module_g_ctrl fail", ret);
			goto p_err;
		}
		break;
	case SENSOR_IOCTL_MOD_S_EXT_CTRL:
		ext_ctrl = (struct v4l2_ext_controls *)arg;
		ret = sensor_module_s_ext_ctrls(subdev, ext_ctrl);
		if (ret) {
			err("err!! ret(%d), sensor_module_s_ext_ctrl fail", ret);
			goto p_err;
		}
		break;
	case SENSOR_IOCTL_MOD_G_EXT_CTRL:
		ext_ctrl = (struct v4l2_ext_controls *)arg;
		ret = sensor_module_g_ext_ctrls(subdev, ext_ctrl);
		if (ret) {
			err("err!! ret(%d), sensor_module_g_ext_ctrl fail", ret);
			goto p_err;
		}
		break;
	default:
		err("err!!! Unknown CID(%#x)", cmd);
		ret = -EINVAL;
		goto p_err;
	}

p_err:
	return ret;
}

int sensor_module_s_routing(struct v4l2_subdev *sd, u32 input, u32 output, u32 config)
{
	int ret = 0;

	return ret;
}

int sensor_module_s_stream(struct v4l2_subdev *sd, int enable)
{
	int ret, paf_ch;
	struct is_device_sensor *sensor;
	struct is_sensor_cfg *cfg;
	struct is_module_enum *module;
	struct is_device_sensor_peri *sensor_peri;

	sensor = (struct is_device_sensor *)v4l2_get_subdev_hostdata(sd);
	module = (struct is_module_enum *)v4l2_get_subdevdata(sd);
	sensor_peri = (struct is_device_sensor_peri *)module->private_data;
	paf_ch = (sensor->ischain->group_3aa.id == GROUP_ID_3AA0) ? 0 :
		(sensor->ischain->group_3aa.id == GROUP_ID_3AA1) ? 1 : 2;
	cfg = sensor->cfg;

	if (enable) {
#if defined(CONFIG_CAMERA_PAFSTAT)
		if (test_bit(IS_SENSOR_PAFSTAT_AVAILABLE, &sensor_peri->peri_state)) {
			ret = CALL_PAFSTATOPS(sensor_peri->pafstat, set_num_buffers,
				sensor_peri->subdev_pafstat,
				sensor->num_buffers, sensor);
			if (ret) {
				err("pafstat_set_num_buffers is fail");
				return ret;
			}
		}
#endif

		/*
		 * Camera first mode set high speed recording and maintain 120fps
		 * not setting exposure so need to this check
		 */
		if ((sensor_peri->use_sensor_work)
			|| (sensor_peri->cis.cis_data->video_mode == true && cfg->framerate >= 60)) {
			ret = is_sensor_init_sensor_thread(sensor_peri);
			if (ret) {
				err("is_sensor_init_sensor_thread is fail");
				return ret;
			}
		}

		if (sensor_peri->cis.cis_data->video_mode == true && cfg->framerate >= 60) {
			sensor_peri->sensor_interface.diff_bet_sen_isp
				= sensor_peri->sensor_interface.otf_flag_3aa ? DIFF_OTF_DELAY + 1 : DIFF_M2M_DELAY;
		} else {
			sensor_peri->sensor_interface.diff_bet_sen_isp
				= sensor_peri->sensor_interface.otf_flag_3aa ? DIFF_OTF_DELAY : DIFF_M2M_DELAY;
		}
	} else {
		is_sensor_deinit_sensor_thread(sensor_peri);
	}

	ret = is_sensor_peri_s_stream(sensor, enable);
	if (ret) {
		err("[MOD] is_sensor_peri_s_stream is fail(%d)", ret);
		goto err_peri_s_stream;
	}

	return 0;

err_peri_s_stream:

	return ret;
}

int sensor_module_s_format(struct v4l2_subdev *subdev,
	struct v4l2_subdev_pad_config *cfg,
	struct v4l2_subdev_format *fmt)
{
	int ret = 0;
	struct is_device_sensor *device = NULL;
	struct is_module_enum *module = NULL;
	struct is_device_sensor_peri *sensor_peri = NULL;
	struct v4l2_subdev *sd = NULL;
	struct is_cis *cis = NULL;
	struct is_core *core = NULL;

	FIMC_BUG(!subdev);

	module = (struct is_module_enum *)v4l2_get_subdevdata(subdev);
	FIMC_BUG(!module);

	sensor_peri = (struct is_device_sensor_peri *)module->private_data;
	FIMC_BUG(!sensor_peri);

	sd = sensor_peri->subdev_cis;
	if (!sd) {
		err("[MOD:%s] no subdev_cis(set_fmt)", module->sensor_name);
		ret = -ENXIO;
		goto p_err;
	}
	cis = (struct is_cis *)v4l2_get_subdevdata(sd);
	FIMC_BUG(!cis);
	FIMC_BUG(!cis->cis_data);

	device = (struct is_device_sensor *)v4l2_get_subdev_hostdata(subdev);
	FIMC_BUG(!device);

	core = (struct is_core *)platform_get_drvdata(device->pdev);
	BUG_ON(!core);

	if (cis->cis_data->sens_config_index_cur != device->cfg->mode
		|| cis->cis_data->sens_config_ex_mode_cur != device->cfg->ex_mode
		|| sensor_peri->mode_change_first == true) {
		dbg_sensor(1, "[%s] mode changed(%d->%d)\n", __func__,
				cis->cis_data->sens_config_index_cur, device->cfg->mode);

		cis->cis_data->sens_config_index_cur = device->cfg->mode;
		cis->cis_data->sens_config_ex_mode_cur = device->cfg->ex_mode;
		cis->cis_data->cur_width = fmt->format.width;
		cis->cis_data->cur_height = fmt->format.height;

		/* check wdr sensor mode */
		CALL_CISOPS(cis, cis_check_wdr_mode, sensor_peri->subdev_cis, device->cfg->mode);

		ret = is_sensor_mode_change(cis, device->cfg->mode);
		if (ret) {
			err("[MOD:%s] sensor_mode_change(cis_mode_change) is fail(%d)",
					module->sensor_name, ret);
			goto p_err;
		}

		/* get crop coordinates from cis */
		device->image.window.offs_h = cis->cis_data->crop_x;
		device->image.window.offs_v = cis->cis_data->crop_y;
	}

	dbg_sensor(1, "[%s] set format done, size(%dx%d), code(%#x)\n", __func__,
			fmt->format.width, fmt->format.height, fmt->format.code);

p_err:
	return ret;
}

int sensor_module_log_status(struct v4l2_subdev *subdev)
{
	int ret = 0;
	struct is_module_enum *module = NULL;
	struct is_device_sensor_peri *sensor_peri = NULL;

	if (unlikely(!subdev)) {
		goto p_err;
	}

	module = (struct is_module_enum *)v4l2_get_subdevdata(subdev);
	if (unlikely(!module)) {
		goto p_err;
	}

	sensor_peri = (struct is_device_sensor_peri *)module->private_data;
	if (unlikely(!sensor_peri)) {
		goto p_err;
	}

	schedule_work(&sensor_peri->cis.cis_status_dump_work);

p_err:
	return ret;
}

/* check read value is same with match seq */
static int sensor_module_check_match_seq(struct is_device_sensor *device, struct exynos_platform_is_module *pdata)
{
	int i, j;
	int idx_prop = 0;
	struct exynos_sensor_module_match *entry;
	u32 slave_addr, reg, reg_type, expected_data, data_type;
	u32 ret = 0;
	u8 val8 = 0;
	u16 val16 = 0;
	bool result_of_match;

	FIMC_BUG(!device);
	FIMC_BUG(!pdata);

	/*
	 * check each group of match seq is satisfied
	 * "and" conditions apply for all groups
	 * If there is a group that does not satisfy, final result is FAIL.
	 */
	for (i = 0; i < pdata->num_of_match_groups; i++) {
		result_of_match = false;
		/*
		 * check each entry of a group is satisfied
		 * "or" conditions apply for all entry in a group
		 * If there is a entry that does satisfy, result of group is PASS.
		 */
		for (j = 0; j < pdata->num_of_match_entry[i]; j++) {
			entry = &pdata->match_entry[i][j];
			idx_prop = j * 5;
			slave_addr = entry->slave_addr;
			reg = entry->reg;
			reg_type = entry->reg_type;
			expected_data = entry->expected_data;
			data_type = entry->data_type;

			device->client->addr = (slave_addr & 0xFFFF);
			if (reg_type == 2) {
				ret = is_sensor_read16(device->client, reg, &val16);
				if (ret)
					return ret;

				if ((u16)(expected_data & 0xFFFF) != val16) {
					info("[FAIL] 0x%04x: 0x%04x != 0x%04x", reg, expected_data, val16);
				} else {
					info("[PASS] 0x%04x: 0x%04x == 0x%04x", reg, expected_data, val16);
					result_of_match = true;
					break;
				}
			} else {
				ret = is_sensor_read8(device->client, reg, &val8);
				if (ret)
					return ret;

				if ((u8)(expected_data & 0xFF) != val8) {
					info("[FAIL] 0x%04x: 0x%02x != 0x%02x", reg, expected_data, val8);
				} else {
					info("[PASS] 0x%04x: 0x%02x == 0x%02x", reg, expected_data, val8);
					result_of_match = true;
					break;
				}
			}
		}
		if (result_of_match == false)
			return -EINVAL;
	}
	return ret;
}

static const struct v4l2_subdev_core_ops core_ops = {
	.init = sensor_module_init,
	.ioctl = sensor_module_ioctl,
	.log_status = sensor_module_log_status,
};

static const struct v4l2_subdev_video_ops video_ops = {
	.s_routing = sensor_module_s_routing,
	.s_stream = sensor_module_s_stream,
};

static const struct v4l2_subdev_pad_ops pad_ops = {
	.set_fmt = sensor_module_s_format
};

static const struct v4l2_subdev_ops subdev_ops = {
	.core = &core_ops,
	.video = &video_ops,
	.pad = &pad_ops
};

static int sensor_module_probe(struct platform_device *pdev)
{
	int ret = 0;
	struct is_core *core;
	struct v4l2_subdev *subdev_module;
	struct is_module_enum *module;
	struct is_device_sensor *device;
	struct sensor_open_extended *ext;
	struct exynos_platform_is_module *pdata;
	struct device *dev;
	int t;
	struct pinctrl_state *s;
	u32 match_result = 0;
	is_moudle_callback module_callback = NULL;

	if (is_dev == NULL) {
		warn("is_dev is not yet probed(sensor_module)");
		return -EPROBE_DEFER;
	}

	core = (struct is_core *)dev_get_drvdata(is_dev);

	dev = &pdev->dev;

#ifdef CONFIG_OF
	ret = is_module_parse_dt(dev, module_callback);
	if (ret)
		return ret;
#endif

	pdata = dev_get_platdata(dev);
	device = &core->sensor[pdata->id];

	if (device->pdata->i2c_dummy_enable && atomic_read(&device->module_count)) {
		probe_info("%s: there is already probe done module, skip probe", __func__);
		ret = -EINVAL;
		goto p_err;
	}

	subdev_module = devm_kzalloc(&pdev->dev, sizeof(*subdev_module), GFP_KERNEL);
	if (!subdev_module) {
		dev_err(&pdev->dev, "subdev_module is NULL");
		ret = -ENOMEM;
		goto p_err;
	}

	probe_info("%s(%s) pdata->id(%d), module_count = %d\n", __func__,
			pdata->sensor_name,
			pdata->id,
			atomic_read(&device->module_count));
	module = &device->module_enum[atomic_read(&device->module_count)];
	clear_bit(IS_MODULE_GPIO_ON, &module->state);
	module->pdata = pdata;
	module->dev = dev;
	module->subdev = subdev_module;

	/* DT data */
	module->sensor_id = pdata->sensor_id;
	module->position = pdata->position;
	module->device = pdata->id;
	module->active_width = pdata->active_width;
	module->active_height = pdata->active_height;
	module->margin_left = pdata->margin_left;
	module->margin_right = pdata->margin_right;
	module->margin_top = pdata->margin_top;
	module->margin_bottom = pdata->margin_bottom;
	module->max_framerate = pdata->max_framerate;
	module->bitwidth = pdata->bitwidth;
	module->sensor_maker = pdata->sensor_maker;
	module->sensor_name = pdata->sensor_name;
	module->setfile_name = pdata->setfile_name;
	module->cfgs = pdata->cfgs;
	module->cfg = pdata->cfg;

	/* not DT data */
	module->pixel_width = module->active_width + module->margin_left + module->margin_right;
	module->pixel_height = pdata->active_height + module->margin_top + module->margin_bottom;
	module->ops = NULL;
	module->client = NULL;

	for (t = VC_BUF_DATA_TYPE_SENSOR_STAT1; t < VC_BUF_DATA_TYPE_MAX; t++) {
		if (IS_ENABLED(CONFIG_CAMERA_PDP) || IS_ENABLED(CONFIG_SW_PDAF)) {
			module->vc_extra_info[t].stat_type = pdata->vc_extra_info[t].stat_type;
			module->vc_extra_info[t].sensor_mode = pdata->vc_extra_info[t].sensor_mode;
			module->vc_extra_info[t].max_width = pdata->vc_extra_info[t].max_width;
			module->vc_extra_info[t].max_height = pdata->vc_extra_info[t].max_height;
			module->vc_extra_info[t].max_element = pdata->vc_extra_info[t].max_element;
		} else {
			module->vc_extra_info[t].stat_type = VC_STAT_TYPE_INVALID;
			module->vc_extra_info[t].sensor_mode = VC_SENSOR_MODE_INVALID;
			module->vc_extra_info[t].max_width = 0;
			module->vc_extra_info[t].max_height = 0;
			module->vc_extra_info[t].max_element = 0;
		}
	}

	/* Sensor peri */
	module->private_data = devm_kzalloc(&pdev->dev,
		sizeof(struct is_device_sensor_peri), GFP_KERNEL);
	if (!module->private_data) {
		dev_err(&pdev->dev, "is_device_sensor_peri is NULL");
		ret = -ENOMEM;
		goto p_err;
	}
	is_sensor_peri_probe((struct is_device_sensor_peri *)module->private_data);
	PERI_SET_MODULE(module);

	ext = &module->ext;
#ifdef USE_CAMERA_SENSOR_RETENTION
	ext->use_retention_mode = pdata->use_retention_mode;
#endif

	ext->sensor_con.product_name = module->sensor_id;
	ext->sensor_con.peri_setting.i2c.channel = pdata->sensor_i2c_ch;

	ext->actuator_con.product_name = ACTUATOR_NAME_NOTHING;
	ext->flash_con.product_name = FLADRV_NAME_NOTHING;
	ext->from_con.product_name = FROMDRV_NAME_NOTHING;
	ext->ois_con.product_name = OIS_NAME_NOTHING;

	if (pdata->af_product_name != ACTUATOR_NAME_NOTHING) {
		ext->actuator_con.product_name = pdata->af_product_name;
		ext->actuator_con.peri_setting.i2c.channel = pdata->af_i2c_ch;
	}

	if (pdata->flash_product_name != FLADRV_NAME_NOTHING)
		ext->flash_con.product_name = pdata->flash_product_name;

	if (pdata->ois_product_name != OIS_NAME_NOTHING) {
		ext->ois_con.product_name = pdata->ois_product_name;
		ext->ois_con.peri_setting.i2c.channel = pdata->ois_i2c_ch;
	} else {
		ext->ois_con.product_name = pdata->ois_product_name;
	}

	if (pdata->mcu_product_name != MCU_NAME_NOTHING) {
		ext->mcu_con.product_name = pdata->mcu_product_name;
		ext->mcu_con.peri_setting.i2c.channel = pdata->mcu_i2c_ch;
	} else {
		ext->mcu_con.product_name = pdata->mcu_product_name;
	}

	if (pdata->eeprom_product_name != EEPROM_NAME_NOTHING) {
		ext->eeprom_con.product_name = pdata->eeprom_product_name;
		ext->eeprom_con.peri_setting.i2c.channel = pdata->eeprom_i2c_ch;
	} else {
		ext->eeprom_con.product_name = pdata->eeprom_product_name;
	}

	ext->laser_af_con.product_name = pdata->laser_af_product_name;

	v4l2_subdev_init(subdev_module, &subdev_ops);

	v4l2_set_subdevdata(subdev_module, module);
	v4l2_set_subdev_hostdata(subdev_module, device);
	snprintf(subdev_module->name, V4L2_SUBDEV_NAME_SIZE, "sensor-subdev.%s", module->sensor_name);

	if (device->pdata->i2c_dummy_enable) {
		probe_info("%s: try to use match seq", __func__);
		ret = is_sensor_mclk_on(device, SENSOR_SCENARIO_MATCH_SEQ, module->pdata->mclk_ch);
		if (ret) {
			merr("is_sensor_mclk_on is fail(%d)", device, ret);
			goto p_err;
		}

		module->pdata->gpio_cfg(module, SENSOR_SCENARIO_MATCH_SEQ, GPIO_SCENARIO_ON);

		match_result = sensor_module_check_match_seq(device, pdata);

		module->pdata->gpio_cfg(module, SENSOR_SCENARIO_MATCH_SEQ, GPIO_SCENARIO_OFF);

		ret = is_sensor_mclk_off(device, SENSOR_SCENARIO_MATCH_SEQ, module->pdata->mclk_ch);
		if (ret) {
			merr("is_sensor_mclk_off is fail(%d)", device, ret);
			ret = 0;
		}

		if (match_result) {
			probe_info("%s: final result of match seq is FAIL", __func__);
			ret = -EINVAL;
			goto p_err;
		}

		probe_info("%s: final result of match seq is PASS", __func__);
	}

	atomic_inc(&device->module_count);

	s = pinctrl_lookup_state(pdata->pinctrl, "release");

	if (pinctrl_select_state(pdata->pinctrl, s) < 0)
		probe_err("pinctrl_select_state is fail\n");

	update_sensor_id_by_position(module->position, module->sensor_id);

	probe_info("%s(%d)\n", __func__, ret);

	return ret;

p_err:
	probe_info("%s(%d)\n", __func__, ret);
	kfree(pdata);

	return ret;
}

static const struct of_device_id exynos_is_sensor_module_match[] = {
	{
		.compatible = "samsung,sensor-module",
		.data = NULL,
	},
	{ /* sentinel */ },
};
MODULE_DEVICE_TABLE(of, exynos_is_sensor_module_match);

struct platform_driver sensor_module_driver = {
	.probe = sensor_module_probe,
	.driver = {
		.name   = "FIMC-IS-SENSOR-MODULE",
		.owner  = THIS_MODULE,
		.of_match_table = exynos_is_sensor_module_match,
	}
};

#ifndef MODULE
static int __init is_sensor_module_init(void)
{
	int ret;

	ret = platform_driver_probe(&sensor_module_driver,
				sensor_module_probe);
	if (ret)
		err("failed to probe %s driver: %d\n",
			sensor_module_driver.driver.name, ret);

	return ret;
}
late_initcall(is_sensor_module_init);
#endif
