/*
 * Samsung EXYNOS Pablo (Imaging Subsystem) driver
 *
 * Copyright (C) 2019 Samsung Electronics Co., Ltd.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#ifndef IS_SFR_VRA_V3_2_H
#define IS_SFR_VRA_V3_2_H

#include "is-hw-api-common.h"

enum is_vra_reg_name {
	VRA_R_SWRESET,
	VRA_R_CNN_IDLE_STATUS,
	VRA_R_CURRENT_INT,
	VRA_R_ENABLE_INT,
	VRA_REG_CNT
};

static const struct is_reg vra_regs[VRA_REG_CNT] = {
	{0x0010, "SWRESET"},
	{0x0018, "CNN_IDLE_STATUS"},
	{0x0104, "CURRENT_INT"},
	{0x0108, "ENABLE_INT"},
};

enum is_vra_reg_field {
	VRA_F_SWRESET,
	VRA_F_CNN_IDLE_STATUS,
	VRA_F_CURRENT_INT,
	VRA_F_ENABLE_INT,
	VRA_REG_FIELD_CNT
};

static const struct is_field vra_fields[VRA_REG_FIELD_CNT] = {
	/* 1. field_name 2. bit start 3. bit width 4. access type 5. reset */
	{"SWRESET",			0,	1,	RIW,	0},		/* SWRESET */
	{"CNN_IDLE_STATUS",	0,	1,	RWI,	0},		/* CNN_IDLE_STATUS */
	{"CURRENT_INT",		0,	5,	RW,		0},		/* CURRENT_INT */
	{"ENABLE_INT",		0,	5,	RW,		0},		/* ENABLE_INT */
};

#endif
