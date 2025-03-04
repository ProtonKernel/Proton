/*
 * Samsung Exynos SoC series NPU driver
 *
 * Copyright (c) 2017 Samsung Electronics Co., Ltd.
 *              http://www.samsung.com/
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 */

#ifndef _NPU_SYSTEM_SOC_H_
#define _NPU_SYSTEM_SOC_H_
#include "npu-system.h"

int npu_system_soc_probe(struct npu_system *system, struct platform_device *pdev);
int npu_system_soc_release(struct npu_system *system, struct platform_device *pdev);
int npu_system_soc_resume(struct npu_system *system, u32 mode);
int npu_system_soc_suspend(struct npu_system *system);
void npu_soc_status_report(struct npu_system *system);
u32 npu_get_hw_info(void);
int npu_hwacg(struct npu_system *system, bool on);
int npu_soc_core_on(struct npu_system *system, int core);
int npu_soc_core_off(struct npu_system *system, int core);
void npu_memory_sync_for_cpu(void);
void npu_memory_sync_for_device(void);

struct npu_iomem_init_data {
	const char*	heapname;
	const char*	name;
	void *		area_info;       /* Save iomem result */
};

#endif	/* _NPU_SYSTEM_SOC_H_ */
