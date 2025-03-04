/*
 * Samsung Exynos9 SoC series Actuator driver
 *
 *
 * Copyright (c) 2018 Samsung Electronics Co., Ltd
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#ifndef IS_DEVICE_DW9839_H
#define IS_DEVICE_DW9839_H

#define DW9839_POS_SIZE_BIT		ACTUATOR_POS_SIZE_10BIT
#define DW9839_POS_MAX_SIZE		((1 << DW9839_POS_SIZE_BIT) - 1)
#define DW9839_POS_DIRECTION		ACTUATOR_RANGE_INF_TO_MAC
#define RINGING_CONTROL			0x2F /* 11ms */
#define PRESCALER				0x01 /* Tvib x 1 */

struct dw9839_actuator_info {
	u16	pcal;
	u16	ncal;
};

#endif
