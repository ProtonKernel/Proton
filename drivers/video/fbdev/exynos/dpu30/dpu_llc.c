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

#if IS_ENABLED(CONFIG_EXYNOS_SCI)

#include "decon.h"
#include "dpu_llc.h"

void dpu_llc_region_alloc(int id, bool on)
{
	static bool allocated = 0;

	if (!id) {
		if (on && !allocated) {
			llc_region_alloc(LLC_REGION_DPU, 1, 2);
			decon_info("decon-%d 2-way llc for DPU is allocated.\n", id);
			allocated = true;
		} else if (!on && allocated) {
			llc_region_alloc(LLC_REGION_DPU, 0, 0);
			decon_info("decon-%d 2-way llc for DPU is released.\n", id);
			allocated = false;
		}
	}
}

#endif
