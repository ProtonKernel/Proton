// SPDX-License-Identifier: GPL-2.0
/*
 * Samsung Exynos SoC series dsp driver
 *
 * Copyright (c) 2019 Samsung Electronics Co., Ltd.
 *              http://www.samsung.com/
 */

#include "dsp-device.h"
#include "dsp-npu.h"

int dsp_npu_release(bool boot, dma_addr_t fw_iova)
{
	return dsp_device_npu_start(boot, fw_iova);
}
