/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Samsung Exynos SoC series dsp driver
 *
 * Copyright (c) 2019 Samsung Electronics Co., Ltd.
 *              http://www.samsung.com/
 */

#ifndef __DSP_NPU_H__
#define __DSP_NPU_H__

#include <linux/dma-buf.h>

int dsp_npu_release(bool boot, dma_addr_t fw_iova);

#endif
