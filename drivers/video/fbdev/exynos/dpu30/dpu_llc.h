/*
 * Copyright (c) 2018 Samsung Electronics Co., Ltd.
 *		http://www.samsung.com
 *
 * Format Header file for Exynos DPU driver
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
*/

#ifndef __DPU_LLC_H__
#define __DPU_LLC_H__

#if IS_ENABLED(CONFIG_EXYNOS_SCI)
#include <soc/samsung/exynos-sci.h>
#endif


#if IS_ENABLED(CONFIG_EXYNOS_SCI)
void dpu_llc_region_alloc(int id, bool on);
#else
#define dpu_llc_region_alloc(id, on)		do {} while (0)
#endif

#endif /* __DPU_LLC_H__ */
