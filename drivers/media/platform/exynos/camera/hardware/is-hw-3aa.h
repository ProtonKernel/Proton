/*
 * Samsung EXYNOS FIMC-IS (Imaging Subsystem) driver
 *
 * Copyright (C) 2014 Samsung Electronics Co., Ltd.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#ifndef IS_HW_3AA_H
#define IS_HW_3AA_H

#include "is-hw-control.h"
#include "is-interface-ddk.h"

#define MIN(a,b) (((a) < (b)) ? (a) : (b))
#define MAX(a,b) (((a) > (b)) ? (a) : (b))

enum sram_config_type {
	SRAM_CFG_N = 0,
	SRAM_CFG_R,
	SRAM_CFG_MODE_CHANGE_R,
	SRAM_CFG_BLOCK,
	SRAM_CFG_S,
	SRAM_CFG_MAX
};

struct is_hw_3aa {
	struct is_lib_isp		lib[IS_STREAM_COUNT];
	struct is_lib_support	*lib_support;
	struct lib_interface_func	*lib_func;
	struct taa_param_set		param_set[IS_STREAM_COUNT];
};

int is_hw_3aa_probe(struct is_hw_ip *hw_ip, struct is_interface *itf,
	struct is_interface_ischain *itfc, int id, const char *name);
void is_hw_3aa_update_param(struct is_hw_ip *hw_ip, struct is_param_region *region,
	struct taa_param_set *param_set, u32 lindex, u32 hindex, u32 instance);
void is_hw_3aa_dump(void);
#endif
