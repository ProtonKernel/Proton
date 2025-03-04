/*
 * Samsung Exynos5 SoC series FIMC-IS driver
 *
 *
 * Copyright (c) 2011 Samsung Electronics Co., Ltd
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#ifndef IS_CMD_H
#define IS_CMD_H

#include "is-config.h"

#define IS_FRAME_DONE	0x1003

enum is_msg_test_id {
	IS_MSG_TEST_SYNC_LOG = 1,
};

#ifdef CONFIG_USE_SENSOR_GROUP
#define MAX_ACTIVE_GROUP 9
#else
#define MAX_ACTIVE_GROUP 7
#endif
struct is_path_info {
	u32 sensor_name;
	u32 mipi_csi;
	u32 reserved;
	/*
	 * uActiveGroup[0] = GROUP_ID_0/GROUP_ID_1/0XFFFFFFFF
	 * uActiveGroup[1] = GROUP_ID_2/GROUP_ID_3/0XFFFFFFFF
	 * uActiveGroup[2] = GROUP_ID_4/0XFFFFFFFF
	 * uActiveGroup[3] = GROUP_ID_5(MCSC0)/GROUP_ID_6(MCSC1)/0XFFFFFFFF
	 * uActiveGroup[4] = GROUP_ID_7/0XFFFFFFFF
	 */
	u32 group[MAX_ACTIVE_GROUP];
};

#endif
