/* drivers/video/exynos/decon/panels/exynos_panel_modes.c
 *
 * Samsung SoC display driver.
 *
 * Copyright (c) 2020 Samsung Electronics
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
*/

#include <linux/of.h>
#include <linux/types.h>
#include <linux/kernel.h>
#include <linux/slab.h>
#include <linux/errno.h>
#include "../panel_modes.h"
#include <linux/sort.h>
#include "exynos_panel_drv.h"
#include "exynos_panel.h"
#include "exynos_panel_modes.h"

/*
 * comparison function used to sort exynos display mode
 * (order:descending)
 */
static int compare_exynos_display_mode(const void *a, const void *b)
{
	int v1, v2;

	/* compare mm_width */
	v1 = (*(struct exynos_display_mode_info **)a)->mode.mm_width;
	v2 = (*(struct exynos_display_mode_info **)b)->mode.mm_width;
	if (v1 != v2)
		return v2 - v1;

	/* compare mm_height */
	v1 = (*(struct exynos_display_mode_info **)a)->mode.mm_height;
	v2 = (*(struct exynos_display_mode_info **)b)->mode.mm_height;
	if (v1 != v2)
		return v2 - v1;

	/* compare width */
	v1 = (*(struct exynos_display_mode_info **)a)->mode.width;
	v2 = (*(struct exynos_display_mode_info **)b)->mode.width;
	if (v1 != v2)
		return v2 - v1;

	/* compare height */
	v1 = (*(struct exynos_display_mode_info **)a)->mode.height;
	v2 = (*(struct exynos_display_mode_info **)b)->mode.height;
	if (v1 != v2)
		return v2 - v1;

	/* compare vrr fps */
	v1 = (*(struct exynos_display_mode_info **)a)->mode.fps;
	v2 = (*(struct exynos_display_mode_info **)b)->mode.fps;
	if (v1 != v2)
		return v2 - v1;

	return 0;
}

void exynos_mode_debug_printmodeline(const struct exynos_display_mode *mode)
{
	DPU_INFO_PANEL("Modeline " EXYNOS_MODE_FMT "\n", EXYNOS_MODE_ARG(mode));
}

void exynos_mode_info_debug_printmodeline(const struct exynos_display_mode_info *mode_info)
{
	DPU_INFO_PANEL("Modeline " EXYNOS_MODE_INFO_FMT "\n",
			EXYNOS_MODE_INFO_ARG(mode_info));
}

static void exynos_display_mode_from_panel_display_mode(struct exynos_panel_device *panel,
		struct panel_display_mode *pdm, struct exynos_display_mode *edm)
{
	struct exynos_panel_info *info = &panel->lcd_info;

	edm->width = pdm->width;
	edm->height = pdm->height;
	edm->mm_width = info->width;
	edm->mm_height = info->height;
	edm->fps = pdm->refresh_rate;
}

/* panel_display_modes to exynos_display_mode_info */
static void exynos_display_mode_info_from_panel_display_mode(struct exynos_panel_device *panel,
		struct panel_display_mode *pdm, struct exynos_display_mode_info *edmi)
{
	exynos_display_mode_from_panel_display_mode(panel,
			pdm, &edmi->mode);
	edmi->dsc_en = pdm->dsc_en;
	edmi->dsc_width = pdm->dsc_slice_w;
	edmi->dsc_height = pdm->dsc_slice_h;
	edmi->dsc_enc_sw =
		exynos_panel_calc_slice_width(pdm->dsc_cnt,
				pdm->dsc_slice_num, pdm->width);
	edmi->dsc_dec_sw = pdm->width / pdm->dsc_slice_num;
	edmi->cmd_lp_ref = pdm->cmd_lp_ref;
}

static void exynos_display_modes_release(struct exynos_display_modes *disp)
{
	if (disp->modes)
		kfree(disp->modes);

	if (disp->mode_infos) {
		unsigned int i;

		for (i = 0; i < disp->num_mode_infos; i++)
			kfree(disp->mode_infos[i]);
		kfree(disp->mode_infos);
	}

	kfree(disp);
}

struct exynos_display_modes *
exynos_display_modes_create_from_panel_display_modes(struct exynos_panel_device *panel,
		struct panel_display_modes *panel_modes)
{
	struct exynos_display_modes *exynos_modes;
	struct exynos_display_mode *native_mode = NULL;
	int i, j;

	exynos_modes = kzalloc(sizeof(*exynos_modes), GFP_KERNEL);
	if (!exynos_modes) {
		DPU_ERR_PANEL("%s: could not allocate struct exynos_display_modes\n", __func__);
		return NULL;
	}

	exynos_modes->modes = kcalloc(panel_modes->num_modes,
				sizeof(struct exynos_display_mode *),
				GFP_KERNEL);
	if (!exynos_modes->modes) {
		DPU_ERR_PANEL("%s: could not allocate exynos_display_mode array\n", __func__);
		goto modefail;
	}

	exynos_modes->mode_infos = kcalloc(panel_modes->num_modes,
				sizeof(struct exynos_display_mode_info *),
				GFP_KERNEL);
	if (!exynos_modes->mode_infos) {
		DPU_ERR_PANEL("%s: could not allocate exynos_display_mode_info array\n", __func__);
		goto modefail;
	}

	for (i = 0; i < panel_modes->num_modes; i++) {
		struct exynos_display_mode_info *edmi; 

		edmi = kzalloc(sizeof(*edmi), GFP_KERNEL);
		if (!edmi) {
			DPU_ERR_PANEL("%s: could not allocate exynos_display_mode_info struct\n", __func__);
			goto modefail;
		}

		exynos_display_mode_info_from_panel_display_mode(panel,
				panel_modes->modes[i], edmi);
		edmi->pdata = panel_modes->modes[i];
		if (panel_modes->native_mode == i) {
			exynos_modes->native_mode_info = i;
			native_mode = &edmi->mode;
		}

		/* push unique exynos_display_mode */
		for (j = 0; j < exynos_modes->num_modes; j++)
			if (!memcmp(&edmi->mode, exynos_modes->modes[j],
						sizeof(struct exynos_display_mode)))
				break;

		if (j == exynos_modes->num_modes)
			exynos_modes->modes[exynos_modes->num_modes++] = &edmi->mode;

		exynos_modes->mode_infos[exynos_modes->num_mode_infos] = edmi;
		exynos_modes->num_mode_infos++;
	}

	if (!native_mode)
		DPU_ERR_PANEL("%s: native_mode not found\n", __func__);

	/*
	 * sorting exynos_display_mode list
	 */
	sort(exynos_modes->modes, exynos_modes->num_modes,
			sizeof(struct exynos_display_mode *),
			compare_exynos_display_mode, NULL);

	/*
	 * print sorted exynos_display_mode list
	 */
	for (i = 0; i < exynos_modes->num_modes; i++)
		exynos_mode_debug_printmodeline(exynos_modes->modes[i]);

	/* find default mode in sorted exynos_display_mode */
	for (i = 0; native_mode && i < exynos_modes->num_modes; i++) {
		if (!memcmp(native_mode, exynos_modes->modes[i],
					sizeof(struct exynos_display_mode))) {
			exynos_modes->native_mode = i;
			break;
		}
	}

	return exynos_modes;

modefail:
	exynos_display_modes_release(exynos_modes);
	exynos_modes = NULL;
	return NULL;
}

void exynos_display_modes_update_panel_info(struct exynos_panel_device *panel,
	struct exynos_display_modes *exynos_modes)
{
	struct exynos_panel_info *info = &panel->lcd_info;
	struct exynos_display_mode_info *edmi;
	int i;

	info->exynos_modes = exynos_modes;

	for (i = 0; i < exynos_modes->num_modes; i++) {
		/* copy to display_mode array for compatibility */
		edmi = container_of(exynos_modes->modes[i],
				struct exynos_display_mode_info, mode);
		memcpy(&info->display_mode[i], edmi, sizeof(*edmi));
		info->display_mode[i].mode.index = i;
		exynos_mode_info_debug_printmodeline(&info->display_mode[i]);
	}
	info->cur_mode_idx = exynos_modes->native_mode;
	DPU_INFO_PANEL("%s: default display mode index(%d)\n", __func__, info->cur_mode_idx);
	info->display_mode_count = exynos_modes->num_modes;
}
