// SPDX-License-Identifier: GPL-2.0
/*
 * Samsung Exynos SoC series Pablo driver
 *
 * Exynos pablo group manager configurations
 *
 * Copyright (c) 2020 Samsung Electronics Co., Ltd
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#ifndef IS_VOTF_ID_TABLE_H
#define IS_VOTF_ID_TABLE_H

#include "is_groupmgr_config.h"
#include "is-subdev-ctrl.h"
#include "is-groupmgr.h"
#include "is-device-sensor.h"
#include "votf/camerapp-votf.h"

extern unsigned int is_votf_ip[GROUP_ID_MAX];

extern unsigned int is_votf_id[GROUP_ID_MAX][ENTRY_END];

/* for AXI-APB interface */
#define HIST_WIDTH 2048
#define HIST_HEIGHT 144

enum IS_VOTF_IP {
	VOTF_CSIS0	= 0x1738,
	VOTF_CSIS1	= 0x1739,
	VOTF_PDP	= 0x173A,
	VOTF_IPP_STAT	= 0x16F0,
	VOTF_IPP_DS	= 0x16F1,
	VOTF_ORBMCH0	= 0x17A4,
	VOTF_ORBMCH1	= 0x17A6,
	VOTF_DNS0	= 0x1764, /* DNS_REPRO_RDMA */
	VOTF_DNS_IP	= 0x1765,
	VOTF_REPRO1_IP	= 0x1766,
	VOTF_YUVPP_IP	= 0x1807,
	VOTF_GDC_W	= 0x15E1,
	VOTF_GDC_R	= 0x15E2,
	VOTF_MCSC_W	= 0x15E4,
	VOTF_MCSC_R	= 0x15E5,
};

enum ISP_VOTF_ID {
	ISP_L0_UV,
	ISP_L2_Y,
	ISP_L2_UV,
	ISP_NOISE,
	ISP_HIST,
	ISP_DRC_GAIN
};

enum YUVPP_VOTF_ID {
	YUVPP_YUVNR_L0_UV = 1,
	YUVPP_YUVNR_L2_Y,
	YUVPP_YUVNR_L2_UV = 5,
	YUVPP_NOISE = 8,
	YUVPP_YUVNR_DRC_GAIN,
	YUVPP_CLAHE_HIST,
};

int is_votf_link_set_service_cfg(struct votf_info *src_info,
		struct votf_info *dst_info,
		u32 width, u32 height, u32 change);
int __is_votf_flush(struct votf_info *src_info,
		struct votf_info *dst_info);
int __is_votf_force_flush(struct votf_info *src_info,
		struct votf_info *dst_info);

u32 is_votf_get_token_size(struct votf_info *vinfo);

void is_votf_subdev_flush(struct is_group *group);
int is_votf_subdev_create_link(struct is_group *group);
#if defined(USE_VOTF_AXI_APB)
void is_votf_subdev_destroy_link(struct is_group *group);
#endif

int is_hw_pdp_set_votf_config(struct is_group *group, struct is_sensor_cfg *s_cfg);
int is_hw_ypp_set_votf_size_config(u32 width, u32 height);
int is_hw_ypp_reset_votf(void);
#endif
