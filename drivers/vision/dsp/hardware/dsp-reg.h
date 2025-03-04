/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Samsung Exynos SoC series dsp driver
 *
 * Copyright (c) 2019 Samsung Electronics Co., Ltd.
 *              http://www.samsung.com/
 */

#ifndef __HW_DSP_REG_H__
#define __HW_DSP_REG_H__

#define REG_SEC_R		(0x1 << 0)
#define REG_SEC_W		(0x1 << 1)
#define REG_SEC_RW		(REG_SEC_R | REG_SEC_W)
#define REG_SEC_DENY		(0x1 << 2)
#define REG_RDONLY		(0x1 << 3)
#define REG_WRONLY		(0x1 << 4)

struct dsp_reg_format {
	const unsigned int	base;
	const unsigned int	offset;
	const unsigned int	flag;
	const unsigned int	count;
	const unsigned int	interval;
	const char		*name;
};

#endif
