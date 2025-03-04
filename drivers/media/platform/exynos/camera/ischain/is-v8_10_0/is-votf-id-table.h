/*
 * Samsung Exynos SoC series Pablo driver
 *
 * Exynos pablo group manager configurations
 *
 * Copyright (c) 2019 Samsung Electronics Co., Ltd
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#ifndef IS_VOTF_ID_TABLE_H
#define IS_VOTF_ID_TABLE_H

#include "is_groupmgr_config.h"
#include "is-subdev-ctrl.h"
#include "votf/camerapp-votf.h"

const unsigned int is_votf_ip[GROUP_ID_MAX] = {
	[GROUP_ID_3AA0 ... GROUP_ID_MAX-1]	= 0xFFFFFFFF,

	[GROUP_ID_SS0] = 0,
	[GROUP_ID_SS1] = 0,
	[GROUP_ID_SS2] = 0,
	[GROUP_ID_SS3] = 0,
	[GROUP_ID_SS4] = 0,
	[GROUP_ID_SS5] = 0,

	[GROUP_ID_PAF0] = 1,
	[GROUP_ID_PAF1] = 2,

	[GROUP_ID_3AA0] = 0,
	[GROUP_ID_3AA1] = 0,
	[GROUP_ID_3AA2] = 0,

	[GROUP_ID_MCS0] = 6,
};

const unsigned int is_votf_id[GROUP_ID_MAX][ENTRY_END] = {
	[GROUP_ID_3AA0 ... GROUP_ID_MAX-1][ENTRY_SENSOR ... ENTRY_END-1]	= 0xFFFFFFFF,

	[GROUP_ID_SS0][ENTRY_SSVC0] = 0,
	[GROUP_ID_SS0][ENTRY_SSVC1] = 1,
	[GROUP_ID_SS1][ENTRY_SSVC0] = 2,
	[GROUP_ID_SS1][ENTRY_SSVC1] = 3,
	[GROUP_ID_SS2][ENTRY_SSVC0] = 4,
	[GROUP_ID_SS2][ENTRY_SSVC1] = 5,
	[GROUP_ID_SS3][ENTRY_SSVC0] = 6,
	[GROUP_ID_SS3][ENTRY_SSVC1] = 7,

	[GROUP_ID_PAF0][ENTRY_PAF] = 0,
	[GROUP_ID_PAF0][ENTRY_PDAF] = 1,
	[GROUP_ID_PAF1][ENTRY_PAF] = 0,
	[GROUP_ID_PAF1][ENTRY_PDAF] = 1,

	[GROUP_ID_3AA0][ENTRY_3AC] = 8,
	[GROUP_ID_3AA0][ENTRY_3AP] = 11,
	[GROUP_ID_3AA1][ENTRY_3AC] = 9,
	[GROUP_ID_3AA1][ENTRY_3AP] = 12,
	[GROUP_ID_3AA2][ENTRY_3AC] = 10,
	[GROUP_ID_3AA2][ENTRY_3AP] = 13,

	[GROUP_ID_MCS0][ENTRY_M0P] = 0,
};

int is_votf_link_set_service_cfg(struct votf_info *src_info,
		struct votf_info *dst_info,
		u32 width, u32 height, u32 change);
int __is_votf_flush(struct votf_info *src_info,
		struct votf_info *dst_info);

static inline u32 is_votf_get_token_size(struct votf_info *vinfo) {return 0x1F; };

static inline void is_votf_subdev_flush(struct is_group *group) { return; };
static inline int is_votf_subdev_create_link(struct is_group *group) { return 0; };
#if defined(USE_VOTF_AXI_APB)
static void is_votf_subdev_destroy_link(struct is_group *group) { return; };
#endif

static inline int is_hw_pdp_set_votf_config(struct is_group *group, struct is_sensor_cfg *s_cfg) { return 0; };

static inline int is_hw_ypp_set_votf_size_config(u32 width, u32 height) { return 0; };

#endif
