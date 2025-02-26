/*
 * Samsung EXYNOS FIMC-IS (Imaging Subsystem) driver
 *
 * Copyright (C) 2015 Samsung Electronics Co., Ltd.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#include <linux/kernel.h>

#include <linux/videodev2.h>
#include <linux/videodev2_exynos_camera.h>

#include "is-interface-sensor.h"
#include "is-control-sensor.h"
#include "is-device-sensor.h"
#include "is-device-sensor-peri.h"

int set_actuator_position_table(struct is_sensor_interface *itf,
				u32 *position_table)
{
	int ret = 0;
	int i;
	struct is_actuator *actuator = NULL;
	struct is_actuator_interface *actuator_itf = NULL;
	struct is_device_sensor_peri *sensor_peri = NULL;

	FIMC_BUG(!itf);
	FIMC_BUG(!position_table);
	FIMC_BUG(itf->magic != SENSOR_INTERFACE_MAGIC);

	actuator = get_subdev_actuator(itf);
	FIMC_BUG(!actuator);

	actuator_itf = &itf->actuator_itf;
	FIMC_BUG(!actuator_itf);

	sensor_peri = container_of(itf, struct is_device_sensor_peri, sensor_interface);
	FIMC_BUG(!sensor_peri);

	if (!test_bit(IS_SENSOR_ACTUATOR_AVAILABLE, &sensor_peri->peri_state)) {
		dbg_actuator("%s: IS_SENSOR_ACTUATOR_NOT_AVAILABLE\n", __func__);
		goto p_err;
	}

	for (i = 0; i < ACTUATOR_MAX_FOCUS_POSITIONS; i++) {
		actuator_itf->position_table.hw_table[i] = position_table[i];
		ret = is_actuator_ctl_convert_position(&actuator_itf->position_table.hw_table[i],
					ACTUATOR_POS_SIZE_12BIT,
					ACTUATOR_RANGE_INF_TO_MAC,
					actuator->pos_size_bit,
					actuator->pos_direction);
		if (ret) {
			err("Converted position fail.(%d)\n", ret);
			goto p_err;
		}
		dbg_actuator("%s: converted virtual position(%d) -> hw_position (%d)\n",
				__func__,
				i, actuator_itf->position_table.hw_table[i]);
	}
	actuator_itf->position_table.enable = true;

	if (!actuator_itf->initialized) {
		ret = is_actuator_ctl_search_position(actuator->position,
						actuator_itf->position_table.hw_table,
						actuator->pos_direction,
						&actuator_itf->virtual_pos);
		if (ret) {
			err("Searched position fail.(%d)\n", ret);
			goto p_err;
		}

		actuator_itf->initialized = true;
		dbg_actuator("%s: initial actuator position: %d, algorithm position: %d\n",
				__func__, actuator->position, actuator_itf->virtual_pos);
	}

p_err:

	return ret;
}

int set_soft_landing_config(struct is_sensor_interface *itf,
				u32 step_delay,
				u32 position_num,
				u32 *position_table)
{
	int ret = 0;
	u32 cnt = 0, max_hw_pos = 0, converted_pos = 0;
	struct is_actuator *actuator = NULL;
	struct is_actuator_interface *actuator_itf = NULL;
	struct is_device_sensor_peri *sensor_peri = NULL;

	if (unlikely(!itf)) {
		err("%s, interface in is NULL", __func__);
		ret = -EINVAL;
		goto p_err;
	}

	FIMC_BUG(!position_table);
	FIMC_BUG(itf->magic != SENSOR_INTERFACE_MAGIC);

	actuator = get_subdev_actuator(itf);
	FIMC_BUG(!actuator);

	actuator_itf = &itf->actuator_itf;
	FIMC_BUG(!actuator_itf);

	sensor_peri = container_of(itf, struct is_device_sensor_peri, sensor_interface);
	FIMC_BUG(!sensor_peri);

	if (!test_bit(IS_SENSOR_ACTUATOR_AVAILABLE, &sensor_peri->peri_state)) {
		dbg_actuator("%s: IS_SENSOR_ACTUATOR_NOT_AVAILABLE\n", __func__);
		goto p_err;
	}

	if (position_num > ACTUATOR_MAX_SOFT_LANDING_NUM) {
		err("Position number is bigger than %d\n", ACTUATOR_MAX_SOFT_LANDING_NUM);
		ret = -EINVAL;
		goto p_err;
	}

	actuator_itf->soft_landing_table.step_delay = step_delay;
	actuator_itf->soft_landing_table.position_num = position_num;

	memset(actuator_itf->soft_landing_table.virtual_table, 0, sizeof(u32 [ACTUATOR_MAX_SOFT_LANDING_NUM]));
	memset(actuator_itf->soft_landing_table.hw_table, 0, sizeof(u32 [ACTUATOR_MAX_SOFT_LANDING_NUM]));

	max_hw_pos = (1 << actuator->pos_size_bit);

	for (cnt = 0; cnt < position_num; cnt++) {
		/* Return invalid */
		if (position_table[cnt] >= max_hw_pos) {
			err("Invalid position, max: %d\n", max_hw_pos);
			ret = -EINVAL;
			goto p_err;
		}

		actuator_itf->soft_landing_table.hw_table[cnt] = position_table[cnt];
		converted_pos = actuator_itf->soft_landing_table.hw_table[cnt];

		ret = is_actuator_ctl_convert_position(&converted_pos,
					actuator->pos_size_bit,
					actuator->pos_direction,
					ACTUATOR_POS_SIZE_12BIT,
					ACTUATOR_RANGE_INF_TO_MAC);
		if (ret) {
			err("Converted position fail.(%d)\n", ret);
			goto p_err;
		}

		/* H/W position -> AF position */
		actuator_itf->soft_landing_table.virtual_table[cnt]
					= converted_pos;
	}

	actuator_itf->soft_landing_table.enable = true;
p_err:
	return ret;
}

int set_position(struct is_sensor_interface *itf, u32 position, int domain_offset)
{
	int ret = 0;
	int index = 0;
	int flag = 0;
	u32 max_real_pos = (1 << ACTUATOR_POS_SIZE_12BIT);
	u32 cur_frame_cnt = 0;
	struct is_device_sensor *device = NULL;
	struct is_module_enum *module = NULL;
	struct v4l2_subdev *subdev_module = NULL;
	struct is_device_sensor_peri *sensor_peri = NULL;
	struct is_actuator_interface *actuator_itf = NULL;
	struct is_actuator *actuator = NULL;
	int position_w_offset = 0;

	FIMC_BUG(!itf);
	FIMC_BUG(itf->magic != SENSOR_INTERFACE_MAGIC);

	sensor_peri = container_of(itf, struct is_device_sensor_peri, sensor_interface);
	FIMC_BUG(!sensor_peri);

	if (!test_bit(IS_SENSOR_ACTUATOR_AVAILABLE, &sensor_peri->peri_state)) {
		dbg_actuator("%s: IS_SENSOR_ACTUATOR_NOT_AVAILABLE\n", __func__);
		position = 0;
		goto p_err;
	}

	module = sensor_peri->module;
	FIMC_BUG(!module);

	subdev_module = module->subdev;
	FIMC_BUG(!subdev_module);

	device = v4l2_get_subdev_hostdata(subdev_module);
	FIMC_BUG(!device);

	actuator_itf = &itf->actuator_itf;
	FIMC_BUG(!actuator_itf);

	actuator = get_subdev_actuator(itf);
	FIMC_BUG(!actuator);

	actuator_itf->virtual_pos = position;

	/* Return invalid */
	if (position >= max_real_pos) {
		err("Invalid position, max: %d\n", max_real_pos);
		ret = -EINVAL;
		goto p_err;
	}

	if (itf->actuator_itf.position_table.enable) {
		dbg_actuator("%s: converted virtual position(%d) -> hw_position (%d)\n",
				__func__, position,
				actuator_itf->position_table.hw_table[position]);
		position = actuator_itf->position_table.hw_table[position];
	} else {
		ret = is_actuator_ctl_convert_position(&position,
				ACTUATOR_POS_SIZE_12BIT,
				ACTUATOR_RANGE_INF_TO_MAC,
				actuator->pos_size_bit,
				actuator->pos_direction);
	}

	if (ret) {
		err("Converted position fail.(%d)\n", ret);
		goto p_err;
	}

	position_w_offset = (int)position + domain_offset;
	position = (u32)MIN(MAX(position_w_offset, 0), MAX_ACT_POSITION);

	/*
	 *  If OTF path this function direct working
	 *  but M2M path other function use
	 *	10 pre_frame_cnt, pre_position possible
	 */
	if (itf->otf_flag_3aa == false) {
		sensor_peri->actuator->actuator_index ++;
		index = sensor_peri->actuator->actuator_index;

		if (index > 9) {
			warn("not setting M2M AF, check M2M mode or M2M AF timer setting\n");
			index = sensor_peri->actuator->actuator_index = 9;
		}

		cur_frame_cnt = get_frame_count(itf);
		sensor_peri->actuator->pre_position[index] = position;
		sensor_peri->actuator->pre_frame_cnt[index] = cur_frame_cnt;

		dbg_actuator("%s: cur_frame_cnt(%d), position(%d), index(%d)\n", __func__,
			cur_frame_cnt, position, index);

	} else {
		if (in_atomic()) {
			is_fpsimd_put_func();
			flag = 1;
		}
		ret = is_actuator_ctl_set_position(device, position);
		if (flag)
			is_fpsimd_get_func();
		if (ret < 0) {
			err("err!!! ret(%d), invalid position(%d)",
					ret, position);
			ret = -EINVAL;
			goto p_err;
		}
		actuator_itf->hw_pos = position;
	}

p_err:
	return ret;
}

/*
 * AF algorithm need a real position at cur frame.
 * Consider to the frame delay in M2M scenario.
 * Because algorithm frame and sensor frame are different by M2M delay.
 * This get_cur_frame_position is frame position for AF algorithm view.
 */
int get_cur_frame_position(struct is_sensor_interface *itf, u32 *position)
{
	int ret = 0;
	u32 cur_frame_cnt = 0, index = 0;
	struct is_actuator_interface *actuator_itf = NULL;
	struct is_device_sensor_peri *sensor_peri = NULL;

	if (unlikely(!itf)) {
		err("%s, interface in is NULL", __func__);
		ret = -EINVAL;
		goto p_err;
	}

	FIMC_BUG(!position);
	FIMC_BUG(itf->magic != SENSOR_INTERFACE_MAGIC);

	sensor_peri = container_of(itf, struct is_device_sensor_peri, sensor_interface);
	FIMC_BUG(!sensor_peri);

	if (!test_bit(IS_SENSOR_ACTUATOR_AVAILABLE, &sensor_peri->peri_state)) {
		dbg_actuator("%s: IS_SENSOR_ACTUATOR_NOT_AVAILABLE\n", __func__);
		*position = 0;
		goto p_err;
	}

	actuator_itf = &itf->actuator_itf;
	FIMC_BUG(!actuator_itf);

	cur_frame_cnt = get_frame_count(itf);
	index = cur_frame_cnt % EXPECT_DM_NUM;
	*position = sensor_peri->cis.expecting_lens_udm[index].pos;

	dbg_actuator("%s: cur_frame(%d), position(%d)\n",
			__func__, cur_frame_cnt, *position);

p_err:

	return ret;
}

/*
 * Get actuator actual position.
 * The position is applied actuator.
 */
int get_applied_actual_position(struct is_sensor_interface *itf, u32 *position)
{
	int ret = 0;
	struct is_actuator_interface *actuator_itf = NULL;
	struct is_device_sensor_peri *sensor_peri = NULL;

	if (unlikely(!itf)) {
		err("%s, interface in is NULL", __func__);
		ret = -EINVAL;
		goto p_err;
	}

	FIMC_BUG(!position);
	FIMC_BUG(itf->magic != SENSOR_INTERFACE_MAGIC);

	sensor_peri = container_of(itf, struct is_device_sensor_peri, sensor_interface);
	FIMC_BUG(!sensor_peri);

	if (!test_bit(IS_SENSOR_ACTUATOR_AVAILABLE, &sensor_peri->peri_state)) {
		dbg_actuator("%s: IS_SENSOR_ACTUATOR_NOT_AVAILABLE\n", __func__);
		*position = 0;
		goto p_err;
	}

	actuator_itf = &itf->actuator_itf;
	FIMC_BUG(!actuator_itf);

	*position = actuator_itf->virtual_pos;

p_err:

	return ret;
}

int get_prev_frame_position(struct is_sensor_interface *itf,
		u32 *position, u32 frame_diff)
{
	int ret = 0;
	struct is_device_sensor_peri *sensor_peri = NULL;
	u32 cur_frame_cnt, prev_frame_cnt, index;

	FIMC_BUG(!itf);
	FIMC_BUG(!position);
	FIMC_BUG(itf->magic != SENSOR_INTERFACE_MAGIC);

	sensor_peri = container_of(itf, struct is_device_sensor_peri, sensor_interface);
	FIMC_BUG(!sensor_peri);

	if (!test_bit(IS_SENSOR_ACTUATOR_AVAILABLE, &sensor_peri->peri_state)) {
		dbg_actuator("%s: IS_SENSOR_ACTUATOR_NOT_AVAILABLE\n", __func__);
		*position = 0;
		goto p_err;
	}

	cur_frame_cnt = get_frame_count(itf);
	prev_frame_cnt = cur_frame_cnt - frame_diff;
	index = (cur_frame_cnt - frame_diff) % EXPECT_DM_NUM;
	*position = sensor_peri->cis.expecting_lens_udm[index].pos;

	dbg_actuator("%s: cur_frame(%d), prev_frame(%d), prev_position(%d)\n",
			__func__, cur_frame_cnt, prev_frame_cnt, *position);

p_err:

	return ret;
}

int set_af_window_position(struct is_sensor_interface *itf, u32 left_x, u32 left_y, u32 right_x, u32 right_y)
{
	int ret = 0;
	struct is_device_sensor_peri *sensor_peri = NULL;

	FIMC_BUG(!itf);

	sensor_peri = container_of(itf, struct is_device_sensor_peri, sensor_interface);
	FIMC_BUG(!sensor_peri);

	sensor_peri->actuator->left_x = left_x;
	sensor_peri->actuator->left_y = left_y;
	sensor_peri->actuator->right_x = right_x;
	sensor_peri->actuator->right_y = right_y;

	dbg_actuator("%s: leftX(%d), leftY(%d), rightX(%d), rightY(%d)\n", __func__, left_x, left_y, right_x, right_y);
	return ret;
}

int get_status(struct is_sensor_interface *itf, u32 *status,
		u32 *temperature_valid, int *temperature, u32 *voltage_valid, int *voltage)
{
	int ret = 0;
	struct is_device_sensor_peri *sensor_peri = NULL;

	if (unlikely(!itf)) {
		err("%s, interface in is NULL", __func__);
		ret = -EINVAL;
		goto p_err;
	}

	FIMC_BUG(!status);
	FIMC_BUG(itf->magic != SENSOR_INTERFACE_MAGIC);

	sensor_peri = container_of(itf, struct is_device_sensor_peri, sensor_interface);
	FIMC_BUG(!sensor_peri);

	if (!test_bit(IS_SENSOR_ACTUATOR_AVAILABLE, &sensor_peri->peri_state)) {
		dbg_actuator("%s: IS_SENSOR_ACTUATOR_NOT_AVAILABLE\n", __func__);
		*status = ACTUATOR_STATUS_NO_BUSY;
		goto p_err;
	}

	ret = sensor_get_ctrl(itf, V4L2_CID_ACTUATOR_GET_STATUS, status);
	if (ret < 0) {
		err("Actuator get status fail. ret(%d), return_value(%d)", ret, *status);
		ret = -EINVAL;
		goto p_err;
	}

	if (sensor_peri->actuator->vendor_use_temperature && sensor_peri->actuator->state == ACTUATOR_STATE_ACTIVE) {
		*temperature_valid = sensor_peri->actuator->temperature_available;
		*temperature = sensor_peri->actuator->temperature;
	} else {
		*temperature_valid = 0;
		*temperature = 0;
	}

	if (sensor_peri->actuator->vendor_use_voltage && sensor_peri->actuator->state == ACTUATOR_STATE_ACTIVE) {
		*voltage_valid = sensor_peri->actuator->voltage_available;
		*voltage = sensor_peri->actuator->voltage;
	} else {
		*voltage_valid = 0;
		*voltage = 0;
	}

p_err:
	return ret;
}
