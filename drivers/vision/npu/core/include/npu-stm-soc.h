/* SPDX-License-Identifier: GPL-2.0 WITH Linux-syscall-note */
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

#ifndef _NPU_STM_SOC_H_
#define _NPU_STM_SOC_H_

#include "npu-system.h"

#ifdef CONFIG_NPU_STM
/* STM is available */
int npu_stm_enable(struct npu_system *system);
int npu_stm_disable(struct npu_system *system);
int npu_stm_probe(struct npu_system *system);
int npu_stm_release(struct npu_system *system);

#else	/* CONFIG_NPU_STM is not defined */
#define npu_stm_enable(p)	(0)
#define npu_stm_disable(p)	(0)
#define npu_stm_probe(p)	(0)
#define npu_stm_release(p)	(0)
#endif

#endif	/* _NPU_STM_SOC_H_ */
