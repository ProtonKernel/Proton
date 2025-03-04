/* SPDX-License-Identifier: GPL-2.0-only
 *
 * Copyright (c) 2019 Samsung Electronics Co., Ltd.
 *		http://www.samsung.com
 *
 * Header file for Exynos1000 DMA DSIM FCMD CAL
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#ifndef __SAMSUNG_FCMD_CAL_H__
#define __SAMSUNG_FCMD_CAL_H__

#include "../decon.h"

/* FCMD CAL APIs exposed to FCMD driver */
void fcmd_reg_init(u32 id);
void fcmd_reg_set_config(u32 id);
void fcmd_reg_irq_enable(u32 id);
int fcmd_reg_deinit(u32 id, bool reset);
void fcmd_reg_set_start(u32 id);

/* DMA_FCMD DEBUG */
void __fcmd_dump(u32 id, void __iomem *regs);

/* DMA_FCMD interrupt handler */
u32 fcmd_reg_get_irq_and_clear(u32 id);

#endif /* __SAMSUNG_FCMD_CAL_H__ */
