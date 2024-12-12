/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Samsung Exynos SoC series dsp driver
 *
 * Copyright (c) 2019 Samsung Electronics Co., Ltd.
 *              http://www.samsung.com/
 */

#ifndef __DSP_DEVICE_H__
#define __DSP_DEVICE_H__

#include <linux/device.h>
#include <linux/platform_device.h>
#include <dt-bindings/soc/samsung/exynos-dsp.h>

#include "dsp-debug.h"
#include "hardware/dsp-system.h"
#include "dsp-core.h"

#define DSP_DRIVER_VERSION(a, b, c)	(((a) << 24) + ((b) << 16) + (c))
#define GET_DRIVER_MAJOR_VERSION(a)	(((a) >> 24) & 0xff)
#define GET_DRIVER_MINOR_VERSION(a)	(((a) >> 16) & 0xff)
#define GET_DRIVER_BUILD_VERSION(a)	((a) & 0xffff)

struct dsp_device {
	struct device			*dev;
	unsigned int			dev_id;
	struct mutex			lock;
	unsigned int			open_count;
	unsigned int			start_count;
	unsigned int			power_count;
	unsigned int			sec_start_count;
	int				sec_pm_level;

	unsigned int			version;
	struct dsp_system		system;
	struct dsp_core			core;
	struct dsp_debug		debug;
};

int dsp_device_npu_start(bool boot, dma_addr_t fw_iova);
int dsp_device_power_active(struct dsp_device *dspdev);
int dsp_device_power_on(struct dsp_device *dspdev, unsigned int pm_level);
void dsp_device_power_off(struct dsp_device *dspdev);

int dsp_device_start(struct dsp_device *dspdev, unsigned int pm_level);
int dsp_device_stop(struct dsp_device *dspdev, unsigned int count);
int dsp_device_start_secure(struct dsp_device *dspdev, unsigned int pm_level,
		unsigned int mem_size);
int dsp_device_stop_secure(struct dsp_device *dspdev, unsigned int count);
int dsp_device_open(struct dsp_device *dspdev);
int dsp_device_close(struct dsp_device *dspdev);

#endif
