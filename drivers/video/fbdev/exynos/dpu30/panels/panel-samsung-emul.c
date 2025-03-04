/*
 * Copyright (c) 2018 Samsung Electronics Co., Ltd.
 *		http://www.samsung.com
 *
 * Samsung EMUL Panel driver.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#include <video/mipi_display.h>
#include "exynos_panel_drv.h"
#include "../dsim.h"

int emul_suspend(struct exynos_panel_device *panel)
{
	return 0;
}

int emul_displayon(struct exynos_panel_device *panel)
{
	struct dsim_device *dsim = get_dsim_drvdata(panel->id);

	mutex_lock(&panel->ops_lock);

	dsim_info("%s was called\n", __func__);

	dsim_write_data_seq_delay(dsim, 25, 0x11); /* sleep out */

	dsim_write_data_seq(dsim, false, 0x29); /* display on */
	mutex_unlock(&panel->ops_lock);

	return 0;
}

int emul_mres(struct exynos_panel_device *panel, u32 mode_idx)
{
	return 0;
}

int emul_doze(struct exynos_panel_device *panel)
{
	return 0;
}

int emul_doze_suspend(struct exynos_panel_device *panel)
{
	return 0;
}

struct exynos_panel_ops panel_emul_ops = {
	.id		= {0xFFFFFF, 0xffffff, 0xffffff, 0xffffff},
	.suspend	= emul_suspend,
	.displayon	= emul_displayon,
	.mres		= emul_mres,
	.doze		= emul_doze,
	.doze_suspend	= emul_doze_suspend,
};
