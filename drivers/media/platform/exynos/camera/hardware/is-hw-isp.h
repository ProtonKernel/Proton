/*
 * Samsung EXYNOS FIMC-IS (Imaging Subsystem) driver
 *
 * Copyright (C) 2014 Samsung Electronics Co., Ltd.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#ifndef IS_HW_ISP_H
#define IS_HW_ISP_H

#include "is-hw-control.h"
#include "is-interface-ddk.h"

struct is_hw_isp {
	struct is_lib_isp		lib[IS_STREAM_COUNT];
	struct is_lib_support	*lib_support;
	struct lib_interface_func	*lib_func;
	struct isp_param_set		param_set[IS_STREAM_COUNT];
};

int is_hw_isp_probe(struct is_hw_ip *hw_ip, struct is_interface *itf,
	struct is_interface_ischain *itfc, int id, const char *name);
#endif
