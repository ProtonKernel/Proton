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

#include "is-hw-api-vra.h"
#include "sfr/is-sfr-vra-v3_2.h"

u32 is_vra_get_all_intr(void __iomem *base_addr)
{
	return is_hw_get_field(base_addr,
			&vra_regs[VRA_R_CURRENT_INT],
			&vra_fields[VRA_F_CURRENT_INT]);
}

u32 is_vra_get_status_intr(void __iomem *base_addr)
{
	return is_hw_get_field(base_addr,
			&vra_regs[VRA_R_CNN_IDLE_STATUS],
			&vra_fields[VRA_F_CNN_IDLE_STATUS]);
}

u32 is_vra_get_enable_intr(void __iomem *base_addr)
{
	return is_hw_get_field(base_addr,
			&vra_regs[VRA_R_ENABLE_INT],
			&vra_fields[VRA_F_ENABLE_INT]);
}

void is_vra_set_clear_intr(void __iomem *base_addr, u32 value)
{
	is_hw_set_field(base_addr,
			&vra_regs[VRA_R_CURRENT_INT],
			&vra_fields[VRA_F_CURRENT_INT],
			value);
}
