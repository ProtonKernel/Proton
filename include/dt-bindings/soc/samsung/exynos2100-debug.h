/* SPDX-License-Identifier: GPL-2.0 */

/* Copyright (c) 2020 Samsung Electronics Co., Ltd.
 *		http://www.samsung.com
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 */

#ifndef EXYNOS21000_DEBUG_H
#define EXYNOS21000_DEBUG_H

/*
 * Do not fix this definition. If you want to change the size or address
 * you should change them at exynos-dpm device tree file.
 */
#define SZ_64K			0x00010000
#define SZ_256K			0x00040000
#define SZ_1M			0x00100000

#define DSS_START_ADDR		0xF0000000
#define DSS_HEADER_SIZE		SZ_64K
#define DSS_KERNEL_SIZE         (2 * SZ_1M)
#define DSS_PLATFORM_SIZE	(4 * SZ_1M)
#define DSS_S2D_SIZE		(18 * SZ_1M)
#define DSS_FIRST_SIZE		(2 * SZ_1M)
#define DSS_INIT_TASK_SIZE	(SZ_256K)
#define DSS_ARRAYRESET_SIZE	(11 * SZ_1M)
#define DSS_ARRAYPANIC_SIZE	(6 * SZ_1M)
#define DSS_KEVENTS_SIZE	(6 * SZ_1M)
#define DSS_FATAL_SIZE		(4 * SZ_1M)

#define DSS_HEADER_OFFSET	0

#define DSS_HEADER_ADDR		(DSS_START_ADDR + DSS_HEADER_OFFSET)
#define DSS_KERNEL_ADDR		(DSS_HEADER_ADDR + DSS_HEADER_SIZE)
#define DSS_PLATFORM_ADDR	(DSS_KERNEL_ADDR + DSS_KERNEL_SIZE)
#define DSS_S2D_ADDR		(DSS_PLATFORM_ADDR + DSS_PLATFORM_SIZE)
#define DSS_FIRST_ADDR		(DSS_S2D_ADDR + DSS_S2D_SIZE)
#define DSS_INIT_TASK_ADDR	(DSS_FIRST_ADDR + DSS_FIRST_SIZE)
#define DSS_ARRAYRESET_ADDR	(DSS_INIT_TASK_ADDR + DSS_INIT_TASK_SIZE)
#define DSS_ARRAYPANIC_ADDR	(DSS_ARRAYRESET_ADDR + DSS_ARRAYRESET_SIZE)
#define DSS_KEVENTS_ADDR	(DSS_ARRAYPANIC_ADDR + DSS_ARRAYPANIC_SIZE)
#define DSS_FATAL_ADDR		(DSS_KEVENTS_ADDR + DSS_KEVENTS_SIZE)
#endif
