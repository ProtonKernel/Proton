/*
 * Samsung EXYNOS FIMC-IS (Imaging Subsystem) driver
 *
 * Copyright (C) 2014 Samsung Electronics Co., Ltd.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#ifndef IS_HW_API_VRA_H
#define IS_HW_API_VRA_H

#include "is-hw-api-common.h"

u32 is_vra_chain0_get_all_intr(void __iomem *base_addr);
void is_vra_chain0_set_clear_intr(void __iomem *base_addr, u32 value);
u32 is_vra_chain0_get_status_intr(void __iomem *base_addr);
u32 is_vra_chain0_get_enable_intr(void __iomem *base_addr);

u32 is_vra_chain1_get_all_intr(void __iomem *base_addr);
u32 is_vra_chain1_get_status_intr(void __iomem *base_addr);
u32 is_vra_chain1_get_enable_intr(void __iomem *base_addr);
void is_vra_chain1_set_clear_intr(void __iomem *base_addr, u32 value);
u32 is_vra_chain1_get_image_mode(void __iomem *base_addr);

u32 is_vra_get_all_intr(void __iomem *base_addr);
u32 is_vra_get_status_intr(void __iomem *base_addr);
u32 is_vra_get_enable_intr(void __iomem *base_addr);
void is_vra_set_clear_intr(void __iomem *base_addr, u32 value);
#endif
