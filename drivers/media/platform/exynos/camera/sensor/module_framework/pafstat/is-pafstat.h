/*
 * Samsung Exynos5 SoC series Sensor driver
 *
 *
 * Copyright (c) 2017 Samsung Electronics Co., Ltd
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#ifndef IS_PAFSTAT_H
#define IS_PAFSTAT_H

#include "is-device-sensor.h"
#include "is-interface-sensor.h"

extern int debug_pafstat;

#define dbg_pafstat(level, fmt, args...) \
	dbg_common(((debug_pafstat) >= (level)), "[PAFSTAT]", fmt, ##args)

#define MAX_NUM_OF_PAFSTAT 2

int pafstat_register(struct is_module_enum *module, int pafstat_ch);
int pafstat_unregister(struct is_module_enum *module);
int is_pafstat_reset_recovery(struct v4l2_subdev *subdev, u32 reset_mode, int pd_mode);

#endif
