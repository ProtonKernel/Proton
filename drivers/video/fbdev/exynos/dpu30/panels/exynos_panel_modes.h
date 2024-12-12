/* 
 * include/linux/panel_modes.h
 *
 * Header file for Samsung Common LCD Driver.
 *
 * Copyright (c) 2020 Samsung Electronics
 * Gwanghui Lee <gwanghui.lee@samsung.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#ifndef __EXYNOS_PANEL_MODES_H__
#define __EXYNOS_PANEL_MODES_H__

#include "../panel_modes.h"
#include "exynos_panel.h"

#define EXYNOS_MODE_FMT    "%dx%d@%dHz(%dx%dmm)"
#define EXYNOS_MODE_ARG(m) \
	(m)->width, (m)->height, (m)->fps, (m)->mm_width, (m)->mm_height

#define EXYNOS_MODE_INFO_FMT   EXYNOS_MODE_FMT "cmd_lp_ref:%d dsc(en:%d size:%dx%d enc_sw:%d dec_sw:%d)"
#define EXYNOS_MODE_INFO_ARG(m) \
	EXYNOS_MODE_ARG(&m->mode), (m)->cmd_lp_ref, \
	(m)->dsc_en, (m)->dsc_width, (m)->dsc_height, \
	(m)->dsc_enc_sw, (m)->dsc_dec_sw

void exynos_mode_debug_printmodeline(const struct exynos_display_mode *mode);
void exynos_mode_info_debug_printmodeline(const struct exynos_display_mode_info *mode_info);
struct exynos_display_modes *
exynos_display_modes_create_from_panel_display_modes(
		struct exynos_panel_device *panel,
		struct panel_display_modes *panel_modes);
void exynos_display_modes_update_panel_info(
		struct exynos_panel_device *panel,
		struct exynos_display_modes *exynos_modes);
#endif /* __EXYNOS_PANEL_MODES_H__ */
