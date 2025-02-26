/*
 * Samsung EXYNOS FIMC-IS (Imaging Subsystem) driver
 *
 * Copyright (C) 2014 Samsung Electronics Co., Ltd.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#include <linux/kernel.h>

#include <linux/videodev2.h>
#include <linux/videodev2_exynos_camera.h>

#include "is-control-sensor.h"
#include "is-device-sensor.h"
#include "is-device-sensor-peri.h"

/* helper */
u64 is_sensor_convert_us_to_ns(u32 usec)
{
	u64 nsec = 0;

	nsec = (u64)usec * (u64)1000;

	return nsec;
}

u32 is_sensor_convert_ns_to_us(u64 nsec)
{
	u64 usec = 0;

	usec = nsec;
	do_div(usec, 1000);

	return (u32)usec;
}

u32 is_sensor_calculate_tgain(u32 dgain, u32 again)
{
	u32 tgain;

	if (dgain > 1000)
		tgain = dgain * (again / 1000);
	else
		tgain = again;

	return tgain;
}

u32 is_sensor_calculate_sensitivity_by_tgain(u32 tgain)
{
	/* ISO(sensitivity) 40 = gain 1x */
	return (tgain * 40) / 1000;
}

u32 is_sensor_ctl_get_csi_vsync_cnt(struct is_device_sensor *device)
{
	struct is_device_csi *csi = NULL;

	FIMC_BUG(!device);

	csi = (struct is_device_csi *)v4l2_get_subdevdata(device->subdev_csi);
	if (unlikely(!csi)) {
		err("%s, csi in is NULL", __func__);
		return 0;
	}

	return atomic_read(&csi->fcount);
}

void is_sensor_ctl_update_exposure_to_uctl(camera2_sensor_uctl_t *sensor_uctl,
	enum is_exposure_gain_count num_data,
	u32 *exposure)
{
	FIMC_BUG_VOID(!sensor_uctl);
	FIMC_BUG_VOID(!exposure);

	switch (num_data) {
	case EXPOSURE_GAIN_COUNT_1:
		sensor_uctl->exposureTime =
			is_sensor_convert_us_to_ns(exposure[EXPOSURE_GAIN_LONG]);
		sensor_uctl->longExposureTime = 0;
		sensor_uctl->shortExposureTime = 0;
		sensor_uctl->midExposureTime = 0;
		break;
	case EXPOSURE_GAIN_COUNT_2:
		sensor_uctl->exposureTime =
			is_sensor_convert_us_to_ns(exposure[EXPOSURE_GAIN_SHORT]);
		sensor_uctl->longExposureTime =
			is_sensor_convert_us_to_ns(exposure[EXPOSURE_GAIN_LONG]);
		sensor_uctl->shortExposureTime =
			is_sensor_convert_us_to_ns(exposure[EXPOSURE_GAIN_SHORT]);
		sensor_uctl->midExposureTime = 0;
		break;
	case EXPOSURE_GAIN_COUNT_3:
		sensor_uctl->exposureTime =
			is_sensor_convert_us_to_ns(exposure[EXPOSURE_GAIN_SHORT]);
		sensor_uctl->longExposureTime =
			is_sensor_convert_us_to_ns(exposure[EXPOSURE_GAIN_LONG]);
		sensor_uctl->shortExposureTime =
			is_sensor_convert_us_to_ns(exposure[EXPOSURE_GAIN_SHORT]);
		sensor_uctl->midExposureTime =
			is_sensor_convert_us_to_ns(exposure[EXPOSURE_GAIN_MIDDLE]);
		break;
	default:
		err("invalid exp_gain num_data(%d)", num_data);
		break;
	}
}

void is_sensor_ctl_update_gain_to_uctl(camera2_sensor_uctl_t *sensor_uctl,
	enum is_exposure_gain_count num_data,
	u32 *analog_gain, u32 *digital_gain)
{
	FIMC_BUG_VOID(!sensor_uctl);
	FIMC_BUG_VOID(!analog_gain);
	FIMC_BUG_VOID(!digital_gain);

	switch (num_data) {
	case EXPOSURE_GAIN_COUNT_1:
		sensor_uctl->analogGain = analog_gain[EXPOSURE_GAIN_LONG];
		sensor_uctl->digitalGain = digital_gain[EXPOSURE_GAIN_LONG];
		sensor_uctl->longAnalogGain = 0;
		sensor_uctl->longDigitalGain = 0;
		sensor_uctl->shortAnalogGain = 0;
		sensor_uctl->shortDigitalGain = 0;
		sensor_uctl->midAnalogGain = 0;
		sensor_uctl->midDigitalGain = 0;
		break;
	case EXPOSURE_GAIN_COUNT_2:
		sensor_uctl->analogGain = analog_gain[EXPOSURE_GAIN_SHORT];
		sensor_uctl->digitalGain = digital_gain[EXPOSURE_GAIN_SHORT];
		sensor_uctl->longAnalogGain = analog_gain[EXPOSURE_GAIN_LONG];
		sensor_uctl->longDigitalGain = digital_gain[EXPOSURE_GAIN_LONG];
		sensor_uctl->shortAnalogGain = analog_gain[EXPOSURE_GAIN_SHORT];
		sensor_uctl->shortDigitalGain = digital_gain[EXPOSURE_GAIN_SHORT];
		sensor_uctl->midAnalogGain = 0;
		sensor_uctl->midDigitalGain = 0;
		break;
	case EXPOSURE_GAIN_COUNT_3:
		sensor_uctl->analogGain = analog_gain[EXPOSURE_GAIN_SHORT];
		sensor_uctl->digitalGain = digital_gain[EXPOSURE_GAIN_SHORT];
		sensor_uctl->longAnalogGain = analog_gain[EXPOSURE_GAIN_LONG];
		sensor_uctl->longDigitalGain = digital_gain[EXPOSURE_GAIN_LONG];
		sensor_uctl->shortAnalogGain = analog_gain[EXPOSURE_GAIN_SHORT];
		sensor_uctl->shortDigitalGain = digital_gain[EXPOSURE_GAIN_SHORT];
		sensor_uctl->midAnalogGain = analog_gain[EXPOSURE_GAIN_MIDDLE];
		sensor_uctl->midDigitalGain = digital_gain[EXPOSURE_GAIN_MIDDLE];
		break;
	default:
		err("invalid exp_gain num_data(%d)", num_data);
		break;
	}
}

void is_sensor_ctl_update_cis_data(cis_shared_data *cis_data, camera2_sensor_uctl_t *sensor_uctrl)
{
	FIMC_BUG_VOID(!sensor_uctrl);
	FIMC_BUG_VOID(!cis_data);

	memcpy(&cis_data->auto_exposure[CURRENT_FRAME], &cis_data->auto_exposure[NEXT_FRAME], sizeof(ae_setting));
	cis_data->auto_exposure[NEXT_FRAME].exposure =
					is_sensor_convert_ns_to_us(sensor_uctrl->exposureTime);
	cis_data->auto_exposure[NEXT_FRAME].analog_gain = sensor_uctrl->analogGain;
	cis_data->auto_exposure[NEXT_FRAME].digital_gain = sensor_uctrl->digitalGain;
	cis_data->auto_exposure[NEXT_FRAME].long_exposure =
					is_sensor_convert_ns_to_us(sensor_uctrl->longExposureTime);
	cis_data->auto_exposure[NEXT_FRAME].long_analog_gain = sensor_uctrl->longAnalogGain;
	cis_data->auto_exposure[NEXT_FRAME].long_digital_gain = sensor_uctrl->longDigitalGain;
	cis_data->auto_exposure[NEXT_FRAME].short_exposure =
					is_sensor_convert_ns_to_us(sensor_uctrl->shortExposureTime);
	cis_data->auto_exposure[NEXT_FRAME].short_analog_gain = sensor_uctrl->shortAnalogGain;
	cis_data->auto_exposure[NEXT_FRAME].short_digital_gain = sensor_uctrl->shortDigitalGain;
	cis_data->auto_exposure[NEXT_FRAME].middle_exposure =
					is_sensor_convert_ns_to_us(sensor_uctrl->midExposureTime);
	cis_data->auto_exposure[NEXT_FRAME].middle_analog_gain = sensor_uctrl->midAnalogGain;
	cis_data->auto_exposure[NEXT_FRAME].middle_digital_gain = sensor_uctrl->midDigitalGain;
}

void is_sensor_ctl_get_ae_index(struct is_device_sensor *device,
					u32 *expo_index,
					u32 *again_index,
					u32 *dgain_index)
{
	struct is_module_enum *module = NULL;
	struct is_device_sensor_peri *sensor_peri;
	struct is_cis *cis = NULL;

	FIMC_BUG_VOID(!device);
	FIMC_BUG_VOID(!expo_index);
	FIMC_BUG_VOID(!again_index);
	FIMC_BUG_VOID(!dgain_index);

	module = (struct is_module_enum *)v4l2_get_subdevdata(device->subdev_module);
	FIMC_BUG_VOID(!module);
	FIMC_BUG_VOID(!module->private_data);

	sensor_peri = (struct is_device_sensor_peri *)module->private_data;
	cis = (struct is_cis *)v4l2_get_subdevdata(sensor_peri->subdev_cis);

	if (cis->ctrl_delay == N_PLUS_TWO_FRAME) {
		*expo_index = NEXT_FRAME;
		*again_index = NEXT_FRAME;
		*dgain_index = NEXT_FRAME;
	} else {
		*expo_index = NEXT_FRAME;
		*again_index = CURRENT_FRAME;
		*dgain_index = CURRENT_FRAME;
	}
}

void is_sensor_ctl_adjust_ae_setting(struct is_device_sensor *device,
					ae_setting *setting, cis_shared_data *cis_data)
{
	struct is_module_enum *module = NULL;
	struct is_device_sensor_peri *sensor_peri = NULL;
	u32 exposure_index = 0;
	u32 again_index = 0;
	u32 dgain_index = 0;

	FIMC_BUG_VOID(!device);
	FIMC_BUG_VOID(!setting);
	FIMC_BUG_VOID(!cis_data);

	module = (struct is_module_enum *)v4l2_get_subdevdata(device->subdev_module);
	FIMC_BUG_VOID(!module);

	sensor_peri = (struct is_device_sensor_peri *)module->private_data;

	is_sensor_ctl_get_ae_index(device, &exposure_index, &again_index, &dgain_index);

	if (sensor_peri->sensor_interface.cis_mode == ITF_CIS_SMIA_WDR) {
		setting->long_exposure = cis_data->auto_exposure[exposure_index].long_exposure;
		setting->long_analog_gain = cis_data->auto_exposure[again_index].long_analog_gain;
		setting->long_digital_gain = cis_data->auto_exposure[dgain_index].long_digital_gain;
		setting->short_exposure = cis_data->auto_exposure[exposure_index].short_exposure;
		setting->short_analog_gain = cis_data->auto_exposure[again_index].short_analog_gain;
		setting->short_digital_gain = cis_data->auto_exposure[dgain_index].short_digital_gain;
		setting->middle_exposure = cis_data->auto_exposure[exposure_index].middle_exposure;
		setting->middle_analog_gain = cis_data->auto_exposure[again_index].middle_analog_gain;
		setting->middle_digital_gain = cis_data->auto_exposure[dgain_index].middle_digital_gain;
	} else {
		setting->exposure = cis_data->auto_exposure[exposure_index].exposure;
		setting->analog_gain = cis_data->auto_exposure[again_index].analog_gain;
		setting->digital_gain = cis_data->auto_exposure[dgain_index].digital_gain;
	}
}

void is_sensor_ctl_compensate_expo_gain(struct is_device_sensor *device,
						ae_setting *setting,
						struct ae_param *adj_again,
						struct ae_param *adj_dgain)
{
	struct is_module_enum *module = NULL;
	struct is_device_sensor_peri *sensor_peri = NULL;

	FIMC_BUG_VOID(!device);
	FIMC_BUG_VOID(!setting);
	FIMC_BUG_VOID(!adj_again);
	FIMC_BUG_VOID(!adj_dgain);

	module = (struct is_module_enum *)v4l2_get_subdevdata(device->subdev_module);
	FIMC_BUG_VOID(!module);

	sensor_peri = (struct is_device_sensor_peri *)module->private_data;

	/* Compensate gain under extremely brightly Illumination */
	if (sensor_peri->sensor_interface.cis_mode == ITF_CIS_SMIA_WDR) {
		is_sensor_peri_compensate_gain_for_ext_br(device, setting->long_exposure,
								&adj_again->long_val,
								&adj_dgain->long_val);
		is_sensor_peri_compensate_gain_for_ext_br(device, setting->short_exposure,
								&adj_again->short_val,
								&adj_dgain->short_val);
		is_sensor_peri_compensate_gain_for_ext_br(device, setting->middle_exposure,
								&adj_again->middle_val,
								&adj_dgain->middle_val);
	} else {
		is_sensor_peri_compensate_gain_for_ext_br(device, setting->exposure,
								&adj_again->val,
								&adj_dgain->val);
		adj_again->short_val = adj_again->val;
		adj_dgain->short_val = adj_dgain->val;
	}

}

int is_sensor_ctl_set_frame_rate(struct is_device_sensor *device,
					u32 frame_duration,
					u32 dynamic_duration)
{
	int ret = 0;
	struct v4l2_control v4l2_ctrl;
	struct is_module_enum *module = NULL;
	struct is_device_sensor_peri *sensor_peri = NULL;
	u32 cur_frame_duration = 0;
	u32 cur_dynamic_duration = 0;

	FIMC_BUG(!device);

	module = (struct is_module_enum *)v4l2_get_subdevdata(device->subdev_module);
	if (unlikely(!module)) {
		err("%s, module in is NULL", __func__);
		module = NULL;
		goto p_err;
	}
	FIMC_BUG(!module);
	sensor_peri = (struct is_device_sensor_peri *)module->private_data;

	cur_frame_duration = is_sensor_convert_ns_to_us(sensor_peri->cis.cur_sensor_uctrl.frameDuration);
	cur_dynamic_duration = is_sensor_convert_ns_to_us(sensor_peri->cis.cur_sensor_uctrl.dynamicFrameDuration);

	/* Set min frame rate(static) */
	if (frame_duration != 0 && cur_frame_duration != frame_duration) {
		v4l2_ctrl.id = V4L2_CID_SENSOR_SET_FRAME_RATE;
		v4l2_ctrl.value = DURATION_US_TO_FPS(frame_duration);
		ret = is_sensor_s_ctrl(device, &v4l2_ctrl);
		if (ret < 0) {
			err("[%s] SET FRAME RATE fail\n", __func__);
			goto p_err;
		}
		sensor_peri->cis.cur_sensor_uctrl.frameDuration =
				is_sensor_convert_us_to_ns(frame_duration);
	} else {
		dbg_sensor(1, "[%s] skip min frame duration(%d)\n", __func__, frame_duration);
	}

	/* Set dynamic frame duration */
	if (dynamic_duration != 0) {
		v4l2_ctrl.id = V4L2_CID_SENSOR_SET_FRAME_DURATION;
		v4l2_ctrl.value = dynamic_duration;
		ret = is_sensor_s_ctrl(device, &v4l2_ctrl);
		if (ret < 0) {
			err("[%s] SET FRAME DURATION fail\n", __func__);
			goto p_err;
		}
		sensor_peri->cis.cur_sensor_uctrl.dynamicFrameDuration =
				is_sensor_convert_us_to_ns(dynamic_duration);
	} else {
		dbg_sensor(1, "[%s] skip dynamic frame duration(%d)\n", __func__, dynamic_duration);
	}
p_err:
	return ret;
}

static int is_sensor_ctl_adjust_gains(struct is_device_sensor *device,
				ae_setting *applied_ae_setting,
				struct ae_param *adj_again,
				struct ae_param *adj_dgain)
{
	int ret = 0;
	struct is_module_enum *module = NULL;
	struct is_device_sensor_peri *sensor_peri = NULL;

	FIMC_BUG(!device);
	FIMC_BUG(!applied_ae_setting);
	FIMC_BUG(!adj_again);
	FIMC_BUG(!adj_dgain);

	module = (struct is_module_enum *)v4l2_get_subdevdata(device->subdev_module);
	if (unlikely(!module)) {
		err("%s, module in is NULL", __func__);
		module = NULL;
		goto p_err;
	}
	FIMC_BUG(!module);
	sensor_peri = (struct is_device_sensor_peri *)module->private_data;

	if (sensor_peri->sensor_interface.cis_mode == ITF_CIS_SMIA_WDR) {
		adj_again->long_val = applied_ae_setting->long_analog_gain;
		adj_dgain->long_val = applied_ae_setting->long_digital_gain;
		adj_again->short_val = applied_ae_setting->short_analog_gain;
		adj_dgain->short_val = applied_ae_setting->short_digital_gain;
		adj_again->middle_val = applied_ae_setting->middle_analog_gain;
		adj_dgain->middle_val = applied_ae_setting->middle_digital_gain;
	} else {
		adj_again->val = adj_again->short_val = applied_ae_setting->analog_gain;
		adj_dgain->val = adj_dgain->short_val = applied_ae_setting->digital_gain;
		adj_again->middle_val = 0;
		adj_dgain->middle_val = 0;
	}

	is_sensor_ctl_compensate_expo_gain(device, applied_ae_setting, adj_again, adj_dgain);

p_err:
	return ret;
}

static int is_sensor_ctl_set_gains(struct is_device_sensor *device,
				struct ae_param adj_again,
				struct ae_param adj_dgain)
{
	int ret = 0;

	FIMC_BUG(!device);

	if (adj_again.val == 0 || adj_dgain.val == 0) {
		dbg_sensor(1, "[%s] Skip set gain (%d,%d)\n",
				__func__, adj_again.val, adj_dgain.val);
		return ret;
	}

	/* Set gain */
	ret = is_sensor_peri_s_analog_gain(device, adj_again);
	if (ret < 0) {
		dbg_sensor(1, "[%s] SET analog gain fail\n", __func__);
		goto p_err;
	}

	ret = is_sensor_peri_s_digital_gain(device, adj_dgain);
	if (ret < 0)
		dbg_sensor(1, "[%s] SET digital gain fail\n", __func__);

p_err:
	return ret;
}

int is_sensor_ctl_update_gains(struct is_device_sensor *device,
				struct is_sensor_ctl *module_ctl,
				u32 *dm_index,
				struct ae_param adj_again,
				struct ae_param adj_dgain)
{
	int ret = 0;
	struct is_module_enum *module = NULL;
	struct is_device_sensor_peri *sensor_peri = NULL;
	u32 sensitivity = 0;
	camera2_sensor_ctl_t *sensor_ctrl = NULL;
	camera2_sensor_uctl_t *sensor_uctrl = NULL;
	u32 loop_cnt, i;

	FIMC_BUG(!device);
	FIMC_BUG(!module_ctl);

	module = (struct is_module_enum *)v4l2_get_subdevdata(device->subdev_module);
	if (unlikely(!module)) {
		err("%s, module in is NULL", __func__);
		module = NULL;
		goto p_err;
	}

	FIMC_BUG(!module);
	sensor_peri = (struct is_device_sensor_peri *)module->private_data;

	sensor_ctrl = &module_ctl->cur_cam20_sensor_ctrl;
	sensor_uctrl = &module_ctl->cur_cam20_sensor_udctrl;

	/* detect manual sensor control or Auto mode
	 * if use manual control, apply sensitivity to ctl meta
	 * else, apply to uctl meta
	 */
	if (sensor_ctrl->sensitivity != 0 && module_ctl->valid_sensor_ctrl == true) {
		sensitivity = sensor_ctrl->sensitivity;
	} else {
		if (sensor_uctrl->sensitivity != 0)
			sensitivity = sensor_uctrl->sensitivity;
		else
			err("[SSDRV] Invalid sensitivity\n");
	}

	loop_cnt = sensor_peri->cis.cis_data->num_of_frame * SENSOR_DM_UPDATE_MARGIN;

	if (adj_again.val != 0 && adj_dgain.val != 0 && sensitivity != 0) {
		sensor_peri->cis.cur_sensor_uctrl.sensitivity = sensitivity;

		sensor_peri->cis.cur_sensor_uctrl.analogGain = adj_again.short_val;
		sensor_peri->cis.cur_sensor_uctrl.digitalGain = adj_dgain.short_val;
		sensor_peri->cis.cur_sensor_uctrl.longAnalogGain = adj_again.long_val;
		sensor_peri->cis.cur_sensor_uctrl.longDigitalGain = adj_dgain.long_val;
		sensor_peri->cis.cur_sensor_uctrl.shortAnalogGain = adj_again.short_val;
		sensor_peri->cis.cur_sensor_uctrl.shortDigitalGain = adj_dgain.short_val;
		sensor_peri->cis.cur_sensor_uctrl.midAnalogGain = adj_again.middle_val;
		sensor_peri->cis.cur_sensor_uctrl.midDigitalGain = adj_dgain.middle_val;

		for (i = 0; i < loop_cnt; i++) {
			sensor_peri->cis.expecting_sensor_dm[(dm_index[0] + i) % EXPECT_DM_NUM].sensitivity
				= sensitivity;
			sensor_peri->cis.expecting_sensor_udm[(dm_index[0] + i) % EXPECT_DM_NUM].analogGain
				= adj_again.short_val;
			sensor_peri->cis.expecting_sensor_udm[(dm_index[0] + i) % EXPECT_DM_NUM].digitalGain
				= adj_dgain.short_val;
			sensor_peri->cis.expecting_sensor_udm[(dm_index[0] + i) % EXPECT_DM_NUM].longAnalogGain
				= adj_again.long_val;
			sensor_peri->cis.expecting_sensor_udm[(dm_index[0] + i) % EXPECT_DM_NUM].longDigitalGain
				= adj_dgain.long_val;
			sensor_peri->cis.expecting_sensor_udm[(dm_index[0] + i) % EXPECT_DM_NUM].shortAnalogGain
				= adj_again.short_val;
			sensor_peri->cis.expecting_sensor_udm[(dm_index[0] + i) % EXPECT_DM_NUM].shortDigitalGain
				= adj_dgain.short_val;
			sensor_peri->cis.expecting_sensor_udm[(dm_index[0] + i) % EXPECT_DM_NUM].midAnalogGain
				= adj_again.middle_val;
			sensor_peri->cis.expecting_sensor_udm[(dm_index[0] + i) % EXPECT_DM_NUM].midDigitalGain
				= adj_dgain.middle_val;
		}
	} else {
		for (i = 0; i < loop_cnt; i++) {
			sensor_peri->cis.expecting_sensor_dm[(dm_index[0] + i) % EXPECT_DM_NUM].sensitivity =
				sensor_peri->cis.expecting_sensor_dm[dm_index[1]].sensitivity;

			sensor_peri->cis.expecting_sensor_udm[(dm_index[0] + i) % EXPECT_DM_NUM].analogGain =
				sensor_peri->cis.expecting_sensor_udm[dm_index[1]].analogGain;
			sensor_peri->cis.expecting_sensor_udm[(dm_index[0] + i) % EXPECT_DM_NUM].digitalGain =
				sensor_peri->cis.expecting_sensor_udm[dm_index[1]].digitalGain;
			sensor_peri->cis.expecting_sensor_udm[(dm_index[0] + i) % EXPECT_DM_NUM].longAnalogGain =
				sensor_peri->cis.expecting_sensor_udm[dm_index[1]].longAnalogGain;
			sensor_peri->cis.expecting_sensor_udm[(dm_index[0] + i) % EXPECT_DM_NUM].longDigitalGain =
				sensor_peri->cis.expecting_sensor_udm[dm_index[1]].longDigitalGain;
			sensor_peri->cis.expecting_sensor_udm[(dm_index[0] + i) % EXPECT_DM_NUM].shortAnalogGain =
				sensor_peri->cis.expecting_sensor_udm[dm_index[1]].shortAnalogGain;
			sensor_peri->cis.expecting_sensor_udm[(dm_index[0] + i) % EXPECT_DM_NUM].shortDigitalGain =
				sensor_peri->cis.expecting_sensor_udm[dm_index[1]].shortDigitalGain;
		}
	}
p_err:
	return ret;
}

#ifdef USE_OIS_HALL_DATA_FOR_VDIS
int is_sensor_ctl_update_hall_data(struct is_device_sensor *device,
				struct is_sensor_ctl *module_ctl,
				u32 *dm_index, struct is_ois_hall_data *halldata)
{
	int ret = 0;
	struct is_module_enum *module = NULL;
	struct is_device_sensor_peri *sensor_peri = NULL;

	FIMC_BUG(!device);
	FIMC_BUG(!dm_index);

	module = (struct is_module_enum *)v4l2_get_subdevdata(device->subdev_module);
	if (unlikely(!module)) {
		err("%s, module in is NULL", __func__);
		module = NULL;
		goto p_err;
	}

	sensor_peri = (struct is_device_sensor_peri *)module->private_data;

	sensor_peri->cis.expecting_aa_dm[dm_index[2]].vendor_oisHallData.readTimeStamp = halldata->readTimeStamp;
	sensor_peri->cis.expecting_aa_dm[dm_index[2]].vendor_oisHallData.counter = halldata->counter;
	sensor_peri->cis.expecting_aa_dm[dm_index[2]].vendor_oisHallData.X_AngVel[0] = halldata->X_AngVel[0];
	sensor_peri->cis.expecting_aa_dm[dm_index[2]].vendor_oisHallData.Y_AngVel[0] = halldata->Y_AngVel[0];
	sensor_peri->cis.expecting_aa_dm[dm_index[2]].vendor_oisHallData.Z_AngVel[0] = halldata->Z_AngVel[0];
	sensor_peri->cis.expecting_aa_dm[dm_index[2]].vendor_oisHallData.X_AngVel[1]  = halldata->X_AngVel[1];
	sensor_peri->cis.expecting_aa_dm[dm_index[2]].vendor_oisHallData.Y_AngVel[1] = halldata->Y_AngVel[1];
	sensor_peri->cis.expecting_aa_dm[dm_index[2]].vendor_oisHallData.Z_AngVel[1] = halldata->Z_AngVel[1];
	sensor_peri->cis.expecting_aa_dm[dm_index[2]].vendor_oisHallData.X_AngVel[2]  = halldata->X_AngVel[2];
	sensor_peri->cis.expecting_aa_dm[dm_index[2]].vendor_oisHallData.Y_AngVel[2] = halldata->Y_AngVel[2];
	sensor_peri->cis.expecting_aa_dm[dm_index[2]].vendor_oisHallData.Z_AngVel[2] = halldata->Z_AngVel[2];
	sensor_peri->cis.expecting_aa_dm[dm_index[2]].vendor_oisHallData.X_AngVel[3]  = halldata->X_AngVel[3];
	sensor_peri->cis.expecting_aa_dm[dm_index[2]].vendor_oisHallData.Y_AngVel[3] = halldata->Y_AngVel[3];
	sensor_peri->cis.expecting_aa_dm[dm_index[2]].vendor_oisHallData.Z_AngVel[3] = halldata->Z_AngVel[3];

p_err:
	return ret;
}
#endif

static int is_sensor_ctl_adjust_exposure(struct is_device_sensor *device,
				struct is_sensor_ctl *module_ctl,
				ae_setting *applied_ae_setting,
				struct ae_param *expo)
{
	int ret = 0;
	struct is_module_enum *module = NULL;
	struct is_device_sensor_peri *sensor_peri = NULL;

	FIMC_BUG(!device);
	FIMC_BUG(!module_ctl);
	FIMC_BUG(!applied_ae_setting);
	FIMC_BUG(!expo);

	module = (struct is_module_enum *)v4l2_get_subdevdata(device->subdev_module);
	if (unlikely(!module)) {
		err("%s, module in is NULL", __func__);
		module = NULL;
		goto p_err;
	}
	FIMC_BUG(!module);

	sensor_peri = (struct is_device_sensor_peri *)module->private_data;

	if (sensor_peri->sensor_interface.cis_mode == ITF_CIS_SMIA_WDR) {
		expo->long_val = applied_ae_setting->long_exposure;
		expo->short_val = applied_ae_setting->short_exposure;
		expo->middle_val = applied_ae_setting->middle_exposure;
	} else {
		expo->val = expo->short_val = applied_ae_setting->exposure;
		expo->middle_val = 0;
	}

p_err:
	return ret;
}

static int is_sensor_ctl_set_exposure(struct is_device_sensor *device,
					struct ae_param expo)
{
	int ret = 0;

	FIMC_BUG(!device);

	if (expo.val == 0) {
		dbg_sensor(1, "[%s] Skip set expo (%d)\n",
				__func__, expo.val);
		return ret;
	}

	ret = is_sensor_peri_s_exposure_time(device, expo);
	if (ret < 0)
		err("[%s] SET exposure time fail\n", __func__);

	return ret;
}

int is_sensor_ctl_update_exposure(struct is_device_sensor *device,
					u32 *dm_index,
					struct ae_param expo)
{
	int ret = 0;
	struct is_module_enum *module = NULL;
	struct is_device_sensor_peri *sensor_peri = NULL;
	u32 loop_cnt, i;

	FIMC_BUG(!device);
	FIMC_BUG(!dm_index);

	module = (struct is_module_enum *)v4l2_get_subdevdata(device->subdev_module);
	if (unlikely(!module)) {
		err("%s, module in is NULL", __func__);
		module = NULL;
		goto p_err;
	}
	FIMC_BUG(!module);
	sensor_peri = (struct is_device_sensor_peri *)module->private_data;

	loop_cnt = sensor_peri->cis.cis_data->num_of_frame * SENSOR_DM_UPDATE_MARGIN;

	if (expo.long_val != 0 && expo.short_val != 0) {
		sensor_peri->cis.cur_sensor_uctrl.exposureTime = is_sensor_convert_us_to_ns(expo.short_val);
		sensor_peri->cis.cur_sensor_uctrl.longExposureTime = is_sensor_convert_us_to_ns(expo.long_val);
		sensor_peri->cis.cur_sensor_uctrl.shortExposureTime = is_sensor_convert_us_to_ns(expo.short_val);
		sensor_peri->cis.cur_sensor_uctrl.midExposureTime = is_sensor_convert_us_to_ns(expo.middle_val);

		for (i = 0; i < loop_cnt; i++) {
			sensor_peri->cis.expecting_sensor_dm[(dm_index[0] + i) % EXPECT_DM_NUM].exposureTime =
				sensor_peri->cis.cur_sensor_uctrl.exposureTime;
			sensor_peri->cis.expecting_sensor_udm[(dm_index[0] + i) % EXPECT_DM_NUM].longExposureTime =
				sensor_peri->cis.cur_sensor_uctrl.longExposureTime;
			sensor_peri->cis.expecting_sensor_udm[(dm_index[0] + i) % EXPECT_DM_NUM].shortExposureTime =
				sensor_peri->cis.cur_sensor_uctrl.shortExposureTime;
			sensor_peri->cis.expecting_sensor_udm[(dm_index[0] + i) % EXPECT_DM_NUM].midExposureTime =
				sensor_peri->cis.cur_sensor_uctrl.midExposureTime;
		}
	} else {
		for (i = 0; i < loop_cnt; i++) {
			sensor_peri->cis.expecting_sensor_dm[(dm_index[0] + i) % EXPECT_DM_NUM].exposureTime =
				sensor_peri->cis.expecting_sensor_dm[dm_index[1]].exposureTime;
			sensor_peri->cis.expecting_sensor_udm[(dm_index[0] + i) % EXPECT_DM_NUM].longExposureTime =
				sensor_peri->cis.expecting_sensor_udm[dm_index[1]].longExposureTime;
			sensor_peri->cis.expecting_sensor_udm[(dm_index[0] + i) % EXPECT_DM_NUM].shortExposureTime =
				sensor_peri->cis.expecting_sensor_udm[dm_index[1]].shortExposureTime;
			sensor_peri->cis.expecting_sensor_udm[(dm_index[0] + i) % EXPECT_DM_NUM].midExposureTime =
				sensor_peri->cis.expecting_sensor_udm[dm_index[1]].midExposureTime;
		}
	}

p_err:
	return ret;
}

static int is_sensor_ctl_set_totalgain(struct is_device_sensor *device,
					struct ae_param expo,
					struct ae_param adj_again,
					struct ae_param adj_dgain)
{
	int ret = 0;

	FIMC_BUG(!device);

	if (expo.val == 0 || adj_again.val == 0 || adj_dgain.val == 0) {
		dbg_sensor(1, "[%s] Skip set total gain (%d, %d, %d))\n",
				__func__, expo, adj_again.val, adj_dgain.val);
		return ret;
	}

	ret = is_sensor_peri_s_totalgain(device, expo, adj_again, adj_dgain);
	if (ret < 0)
		err("[%s] SET totalgain fail\n", __func__);

	return ret;
}

void is_sensor_ctl_frame_evt(struct is_device_sensor *device)
{
	int ret = 0;
	u32 vsync_count = 0;
	u32 applied_frame_number = 0;
	u32 uctl_frame_index = 0;
	u32 dm_index[3];
	ae_setting applied_ae_setting;
	u32 frame_duration = 0;
	struct ae_param expo, adj_again, adj_dgain;
	struct is_module_enum *module = NULL;
	struct is_device_sensor_peri *sensor_peri = NULL;
	struct is_sensor_ctl *module_ctl = NULL;
	cis_shared_data *cis_data = NULL;
#ifdef USE_OIS_HALL_DATA_FOR_VDIS
	u32 hashkey;
	uint64_t timestamp = 0;
	struct is_ois_hall_data hall_data;
	u32 cur_frame_duration = 0;
#endif

	camera2_sensor_ctl_t *sensor_ctrl = NULL;
	camera2_sensor_uctl_t *sensor_uctrl = NULL;

	struct v4l2_control ctrl;

	FIMC_BUG_VOID(!device);
	FIMC_BUG_VOID(!device->subdev_module);

	vsync_count = is_sensor_ctl_get_csi_vsync_cnt(device);

	module = (struct is_module_enum *)v4l2_get_subdevdata(device->subdev_module);
	if (unlikely(!module)) {
		err("%s, module in is NULL", __func__);
		return;
	}
	sensor_peri = (struct is_device_sensor_peri *)module->private_data;

	if(vsync_count < sensor_peri->sensor_interface.diff_bet_sen_isp + 1) {
		info("%s: vsync_count(%d) < DIFF_BET_SEN_ISP + 1(%d)\n",
			__func__, vsync_count,
			sensor_peri->sensor_interface.diff_bet_sen_isp + 1);
		return;
	}

	applied_frame_number = vsync_count - sensor_peri->sensor_interface.diff_bet_sen_isp;
	uctl_frame_index = applied_frame_number % CAM2P0_UCTL_LIST_SIZE;
	/* dm_index[0] : index for update dm
	 * dm_index[1] : previous updated index for error case
	 * dm_index[2] : index for using ois hall data
	 */
	dm_index[0] = (applied_frame_number + 2) % EXPECT_DM_NUM;
	dm_index[1] = (applied_frame_number + 1) % EXPECT_DM_NUM;
	dm_index[2] = (applied_frame_number + 0) % EXPECT_DM_NUM;

	module_ctl = &sensor_peri->cis.sensor_ctls[uctl_frame_index];
	cis_data = sensor_peri->cis.cis_data;
	FIMC_BUG_VOID(!cis_data);

	if ((module_ctl->valid_sensor_ctrl == true) ||
		(module_ctl->force_update) ||
		(module_ctl->sensor_frame_number == applied_frame_number && module_ctl->alg_reset_flag == true)) {
		sensor_ctrl = &module_ctl->cur_cam20_sensor_ctrl;
		sensor_uctrl = &module_ctl->cur_cam20_sensor_udctrl;

		if (module_ctl->ctl_frame_number != module_ctl->sensor_frame_number) {
			dbg_sensor(1, "Sen frame number is not match ctl/sensor(%d/%d)\n",
					module_ctl->ctl_frame_number, module_ctl->sensor_frame_number);
		}

		/* update cis_date */
		is_sensor_ctl_update_cis_data(cis_data, sensor_uctrl);

		/* set applied AE setting */
		memset(&applied_ae_setting, 0, sizeof(ae_setting));
		is_sensor_ctl_adjust_ae_setting(device, &applied_ae_setting, cis_data);

		/* 1. set frame rate : Limit of max frame duration */
		if (sensor_ctrl->frameDuration != 0 && module_ctl->valid_sensor_ctrl == true)
			frame_duration = is_sensor_convert_ns_to_us(sensor_ctrl->frameDuration);
		else
			frame_duration = is_sensor_convert_ns_to_us(sensor_uctrl->frameDuration);

		ret = is_sensor_ctl_set_frame_rate(device, frame_duration, 0);
		if (ret < 0) {
			err("[%s] frame number(%d) set frame duration fail\n", __func__, applied_frame_number);
		}

		/* 2. set exposureTime */
		ret = is_sensor_ctl_adjust_exposure(device, module_ctl, &applied_ae_setting, &expo);
		if (ret < 0)
			err("[%s] frame number(%d) adjust exposure fail\n", __func__, applied_frame_number);

		/* 3. set dynamic duration */
		ctrl.id = V4L2_CID_SENSOR_ADJUST_FRAME_DURATION;
		ctrl.value = 0;
		ret = is_sensor_peri_adj_ctrl(device, expo.long_val, &ctrl);
		if (ret < 0)
			err("err!!! ret(%d)", ret);

		sensor_uctrl->dynamicFrameDuration = is_sensor_convert_us_to_ns(ctrl.value);

		ret = is_sensor_ctl_set_frame_rate(device, 0,
						is_sensor_convert_ns_to_us(sensor_uctrl->dynamicFrameDuration));
		if (ret < 0) {
			err("[%s] frame number(%d) set frame duration fail\n", __func__, applied_frame_number);
		}

		if (sensor_peri->cis.use_vendor_total_gain) {
			/* 4. set Total Gain : ExposureTime, Analog & Digital gain */
			ret = is_sensor_ctl_adjust_gains(device, &applied_ae_setting, &adj_again, &adj_dgain);
			if (ret < 0) {
				err("[%s] frame number(%d) adjust gains fail\n", __func__, applied_frame_number);
				goto p_err;
			}
			ret = is_sensor_ctl_set_totalgain(device, expo, adj_again, adj_dgain);
			if (ret < 0)
				err("[%s] frame number(%d) set Total gain fail\n", __func__, applied_frame_number);

			/* 5. update Total Gain : ExposureTime, Analog & Digital gain */
			ret = is_sensor_ctl_update_exposure(device, dm_index, expo);
			if (ret < 0)
				err("[%s] frame number(%d) update exposure fail\n", __func__, applied_frame_number);

			ret = is_sensor_ctl_update_gains(device, module_ctl, dm_index, adj_again, adj_dgain);
			if (ret < 0)
				err("[%s] frame number(%d) update gains fail\n", __func__, applied_frame_number);
		} else {
			/* 4. update exposureTime */
			ret = is_sensor_ctl_set_exposure(device, expo);
			if (ret < 0)
				err("[%s] frame number(%d) set exposure fail\n", __func__, applied_frame_number);
			ret = is_sensor_ctl_update_exposure(device, dm_index, expo);
			if (ret < 0)
				err("[%s] frame number(%d) update exposure fail\n", __func__, applied_frame_number);

			/* 5. set analog & digital gains */
			ret = is_sensor_ctl_adjust_gains(device, &applied_ae_setting, &adj_again, &adj_dgain);
			if (ret < 0) {
				err("[%s] frame number(%d) adjust gains fail\n", __func__, applied_frame_number);
				goto p_err;
			}

			ret = is_sensor_ctl_set_gains(device, adj_again, adj_dgain);
			if (ret < 0)
				err("[%s] frame number(%d) set gains fail\n", __func__, applied_frame_number);

			ret = is_sensor_ctl_update_gains(device, module_ctl, dm_index, adj_again, adj_dgain);
			if (ret < 0)
				err("[%s] frame number(%d) update gains fail\n", __func__, applied_frame_number);
		}

		if (module_ctl->update_wb_gains) {
			ret = is_sensor_peri_s_wb_gains(device, module_ctl->wb_gains);
			if (ret < 0)
				err("[%s] frame number(%d) set exposure fail\n", __func__, applied_frame_number);

			module_ctl->update_wb_gains = false;
		}

		ret = is_sensor_set_test_pattern(device, sensor_ctrl);
		if (ret < 0)
			err("[%s] frame number(%d) set test pattern fail\n", __func__, applied_frame_number);

		if (module_ctl->update_3hdr_stat || module_ctl->update_roi ||
			module_ctl->update_tone || module_ctl->update_ev) {
			ret = is_sensor_peri_s_sensor_stats(device, true, module_ctl, NULL);
			if (ret < 0)
				err("[%s] frame number(%d) set exposure fail\n", __func__, applied_frame_number);

			module_ctl->update_roi = false;
			module_ctl->update_3hdr_stat = false;
			module_ctl->update_tone = false;
			module_ctl->update_ev = false;
		}

		module_ctl->force_update = false;
	} else {
		if (module_ctl->alg_reset_flag == false) {
			dbg_sensor(1, "[%s] frame number(%d)  alg_reset_flag (%d)\n", __func__,
					applied_frame_number, module_ctl->alg_reset_flag);
		} else if (module_ctl->sensor_frame_number == applied_frame_number) {
			err("[%s] frame number(%d) Shot process of AE is too slow, alg_reset_flag (%d)\n", __func__,
					applied_frame_number, module_ctl->alg_reset_flag);
		} else {
			info("module->sen_framenum(%d), applied_frame_num(%d), alg_reset_flag (%d)\n",
					module_ctl->sensor_frame_number, applied_frame_number, module_ctl->alg_reset_flag);
		}
	}

	if (sensor_peri->subdev_flash != NULL) {
		/* Pre-Flash on, Torch on/off */
		ret = is_sensor_peri_pre_flash_fire(device->subdev_module, &vsync_count);
	}

	/* Warning! Aperture mode should be set before setting ois mode */
	if (sensor_peri->mcu && sensor_peri->mcu->ois) {
		if (CALL_OISOPS(sensor_peri->mcu->ois, ois_get_active)) {
			ret = CALL_OISOPS(sensor_peri->mcu->ois, ois_set_mode, sensor_peri->subdev_mcu, sensor_peri->mcu->ois->ois_mode);
			if (ret < 0) {
				err("[SEN:%d] v4l2_subdev_call(ois_mode_change, mode:%d) is fail(%d)",
					module->sensor_id, sensor_peri->mcu->ois->ois_mode, ret);
				goto p_err;
			}

			ret = CALL_OISOPS(sensor_peri->mcu->ois, ois_set_coef, sensor_peri->subdev_mcu, sensor_peri->mcu->ois->coef);
			if (ret < 0) {
				err("[SEN:%d] v4l2_subdev_call(ois_set_coef, coef:%d) is fail(%d)",
					module->sensor_id, sensor_peri->mcu->ois->coef, ret);
				goto p_err;
			}

#ifdef USE_OIS_HALL_DATA_FOR_VDIS
			cur_frame_duration = is_sensor_convert_ns_to_us(sensor_peri->cis.cur_sensor_uctrl.frameDuration);
			/* in case of under 60fps, enable ois hall data */
			if (cur_frame_duration >= 16666) {
				if(cur_frame_duration == 16666) /* in case of 60fps - fix frame index error*/
					dm_index[2] = (applied_frame_number + 1) % EXPECT_DM_NUM;

				memset(&hall_data, 0, sizeof(hall_data));
				ret = CALL_OISOPS(sensor_peri->mcu->ois, ois_get_hall_data, sensor_peri->subdev_mcu, &hall_data);
				if (ret < 0) {
					err("[SEN:%d] v4l2_subdev_call(ois_get_hall_data) is fail(%d)", ret);
					goto p_err;
				}

				hashkey = module_ctl->sensor_frame_number % IS_TIMESTAMP_HASH_KEY;
				timestamp = device->timestampboot[hashkey];
				hall_data.readTimeStamp = timestamp;
				is_sensor_ctl_update_hall_data(device, module_ctl, dm_index, &hall_data);
			}
#endif
		}
	}

	/* TODO */
	/* FuncCompanionChangeConfig */

p_err:
	return;
}

int is_sensor_ctl_adjust_sync(struct is_device_sensor *device, u32 sync)
{
	int ret = 0;
	struct is_module_enum *module = NULL;
	struct is_device_sensor_peri *sensor_peri = NULL;
	cis_shared_data *cis_data = NULL;

	FIMC_BUG(!device);

	module = (struct is_module_enum *)v4l2_get_subdevdata(device->subdev_module);
	if (unlikely(!module)) {
		err("%s, module in is NULL", __func__);
		module = NULL;
		goto p_err;
	}
	FIMC_BUG(!module);
	sensor_peri = (struct is_device_sensor_peri *)module->private_data;

	cis_data = sensor_peri->cis.cis_data;
	FIMC_BUG(!cis_data);
	ret = CALL_CISOPS(&sensor_peri->cis, cis_set_adjust_sync, sensor_peri->subdev_cis, sync);

p_err:
	return ret;
}
