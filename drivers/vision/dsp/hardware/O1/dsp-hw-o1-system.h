/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Samsung Exynos SoC series dsp driver
 *
 * Copyright (c) 2019 Samsung Electronics Co., Ltd.
 *              http://www.samsung.com/
 */

#ifndef __HW_O1_DSP_HW_O1_SYSTEM_H__
#define __HW_O1_DSP_HW_O1_SYSTEM_H__

#if IS_ENABLED(CONFIG_EXYNOS_IMGLOADER)
#include <soc/samsung/imgloader.h>
#endif
#if IS_ENABLED(CONFIG_EXYNOS_SYSTEM_EVENT)
#include <soc/samsung/sysevent.h>
#include <soc/samsung/sysevent_notif.h>
#endif
#if IS_ENABLED(CONFIG_EXYNOS_MEMORY_LOGGER)
#include <soc/samsung/memlogger.h>
#endif

#include "hardware/dsp-system.h"

enum dsp_hw_o1_system_wait_mode {
	DSP_HW_O1_WAIT_MODE_INTERRUPT,
	DSP_HW_O1_WAIT_MODE_BUSY_WAITING,
	DSP_HW_O1_WAIT_MODE_NUM,
};

struct dsp_hw_o1_system {
	void			*boot_bin;
	size_t			boot_bin_size;
	void __iomem		*product_id;
	void __iomem		*chip_id;
	unsigned int		wait_mode;
#if IS_ENABLED(CONFIG_EXYNOS_IMGLOADER)
	struct imgloader_desc	loader_desc;
#endif
#if IS_ENABLED(CONFIG_EXYNOS_SYSTEM_EVENT)
	struct sysevent_desc	sysevent_desc;
	struct sysevent_device	*sysevent_dev;
	bool			sysevent_get;
#endif
#if IS_ENABLED(CONFIG_EXYNOS_MEMORY_LOGGER)
	struct memlog		*memlog_desc;
#endif
};

int dsp_hw_o1_system_init(void);

#endif
