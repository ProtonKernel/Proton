/*
 * Copyright (c) 2020 Samsung Electronics Co., Ltd.
 *		http://www.samsung.com
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 */

#ifndef __EXYNOS_MIGOV_DT_H
#define __EXYNOS_MIGOV_DT_H

#define	MIGOV_CL0	0
#define	MIGOV_CL1	1
#define	MIGOV_CL2	2
#define	MIGOV_GPU	3
#define	MIGOV_MIF	4
#define	NUM_OF_DOMAIN	5
#define NUM_OF_CPU_DOMAIN	(MIGOV_CL2 + 1)

#define MAXNUM_OF_CPU		8
#define MAXNUM_OF_DVFS		30

#endif	/* End __EXYNOS_MIGOV_DT_H */
