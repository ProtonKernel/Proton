/*
 * Samsung EXYNOS FIMC-IS (Imaging Subsystem) driver
 *
 * Copyright (C) 2014 Samsung Electronics Co., Ltd.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#ifndef IS_HW_API_COMMON_H
#define IS_HW_API_COMMON_H

#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/errno.h>
#include <linux/slab.h>

enum  regdata_type {
	/* read write */
	RW			= 0,
	/* read only */
	RO			= 1,
	/* write only */
	WO			= 2,
	/* write input */
	WI			= 2,
	/* clear after read */
	RAC			= 3,
	/* write 1 -> clear */
	W1C			= 4,
	/* write read input */
	WRI			= 5,
	/* write input */
	RWI			= 5,
	/* only scaler */
	R_W			= 6,
	/* Read/Write Corex(SRAM) buffered register */
	RWC			= 7,
	/* Read/Write shadowed/double buffered register */
	RWS			= 8,
	/* write only*/
	RIW			= 9,
	/* read only latched implementation register */
	ROL			= 10,
	/* Write-to-Clear - pulse generation on Write */
	WTC			= 11,
	/* Data Write-to-Clear - pulse generation on Write only if the Write data equals the default SFR value */
	XWTC			= 12,
};

struct is_reg {
	unsigned int	 sfr_offset;
	char		 *reg_name;
};

struct is_field {
	char			*field_name;
	unsigned int		bit_start;
	unsigned int		bit_width;
	enum regdata_type	type;
	int			reset;
};

u32 is_hw_get_reg(void __iomem *base_addr, const struct is_reg *reg);
void is_hw_set_reg(void __iomem *base_addr, const struct is_reg *reg, u32 val);
u8 is_hw_get_reg_u8(void __iomem *base_addr, const struct is_reg *reg);
void is_hw_set_reg_u8(void __iomem *base_addr, const struct is_reg *reg, u8 val);
u32 is_hw_get_field(void __iomem *base_addr, const struct is_reg *reg, const struct is_field *field);
void is_hw_set_field(void __iomem *base_addr, const struct is_reg *reg, const struct is_field *field, u32 val);
u32 is_hw_get_field_value(u32 reg_value, const struct is_field *field);
u32 is_hw_set_field_value(u32 reg_value, const struct is_field *field, u32 val);
void is_hw_dump_regs(void __iomem *base_addr, const struct is_reg *regs, u32 total_cnt);
void is_hw_cdump_regs(void __iomem *base_addr, const struct is_reg *regs, u32 total_cnt);
#endif
