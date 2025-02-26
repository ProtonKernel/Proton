// SPDX-License-Identifier: GPL-2.0
/*
 * Samsung Exynos SoC series Pablo driver
 *
 * Copyright (c) 2020 Samsung Electronics Co., Ltd
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#ifndef IS_HW_VRA_H
#define IS_HW_VRA_H

#include "is-hw-api-vra.h"
#include "is-interface-vra-v2.h"

struct is_hw_vra {
	struct is_lib_vra		lib_vra;
};

int is_hw_vra_probe(struct is_hw_ip *hw_ip, struct is_interface *itf,
	struct is_interface_ischain *itfc, int id, const char *name);
int is_hw_vra_frame_end_final_out_ready_done(struct is_lib_vra *lib_vra);
#endif
