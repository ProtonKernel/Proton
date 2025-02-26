/*
 * Samsung EXYNOS FIMC-IS (Imaging Subsystem) driver
 *
 * Copyright (C) 2014 Samsung Electronics Co., Ltd.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#ifndef IS_CONTROL_SENSOR_H
#define IS_CONTROL_SENSOR_H

#include <linux/workqueue.h>

#include "is-interface-sensor.h"

#define NUM_OF_FRAME_30FPS	(1)
#define NUM_OF_FRAME_60FPS	(2)
#define NUM_OF_FRAME_120FPS	(4)
#define NUM_OF_FRAME_240FPS	(12)
#define NUM_OF_FRAME_480FPS	(16)

#define CAM2P0_UCTL_LIST_SIZE   (NUM_OF_FRAME_480FPS + 1)	/* This value must be larger than NUM_OF_FRAME */
#define EXPECT_DM_NUM		(CAM2P0_UCTL_LIST_SIZE)

#define SENSOR_DM_UPDATE_MARGIN	(2)

/* Helper function */
u64 is_sensor_convert_us_to_ns(u32 usec);
u32 is_sensor_convert_ns_to_us(u64 nsec);
u32 is_sensor_calculate_tgain(u32 dgain, u32 again);
u32 is_sensor_calculate_sensitivity_by_tgain(u32 tgain);

struct is_device_sensor;
void is_sensor_ctl_frame_evt(struct is_device_sensor *device);
int is_sensor_ctl_adjust_sync(struct is_device_sensor *device, u32 adjust_sync);
int is_sensor_ctl_low_noise_mode(struct is_device_sensor *device, u32 mode);

void is_sensor_ctl_update_exposure_to_uctl(camera2_sensor_uctl_t *sensor_uctl,
	enum is_exposure_gain_count num_data,
	u32 *exposure);
void is_sensor_ctl_update_gain_to_uctl(camera2_sensor_uctl_t *sensor_uctl,
	enum is_exposure_gain_count num_data,
	u32 *analog_gain, u32 *digital_gain);

int is_sensor_ctl_update_gains(struct is_device_sensor *device,
		struct is_sensor_ctl *module_ctl,
		u32 *dm_index,
		struct ae_param adj_again,
		struct ae_param adj_dgain);
int is_sensor_ctl_update_exposure(struct is_device_sensor *device,
		u32 *dm_index,
		struct ae_param expo);

/* Actuator funtion */
int is_actuator_ctl_set_position(struct is_device_sensor *device, u32 position);
int is_actuator_ctl_convert_position(u32 *pos,
				u32 src_max_pos, u32 src_direction,
				u32 tgt_max_pos, u32 tgt_direction);
int is_actuator_ctl_search_position(u32 position,
				u32 *position_table,
				u32 direction,
				u32 *searched_pos);
#endif
