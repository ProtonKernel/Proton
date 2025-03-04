/*
 * Copyright (c) 2020 Samsung Electronics Co., Ltd.
 *		http://www.samsung.com
 *
 * Samsung S6E3HAD Panel driver.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#include <video/mipi_display.h>
#include "exynos_panel_drv.h"
#include "../dsim.h"

#define MAX_SUPPORT_MRES	3

static unsigned char SEQ_PPS_SLICE2[] = {
	// WQHD+ :1440x3200
	0x11, 0x00, 0x00, 0x89, 0x30, 0x80, 0x0C, 0x80,
	0x05, 0xA0, 0x00, 0x28, 0x02, 0xD0, 0x02, 0xD0,
	0x02, 0x00, 0x02, 0x68, 0x00, 0x20, 0x04, 0x6C,
	0x00, 0x0A, 0x00, 0x0C, 0x02, 0x77, 0x01, 0xE9,
	0x18, 0x00, 0x10, 0xF0, 0x03, 0x0C, 0x20, 0x00,
	0x06, 0x0B, 0x0B, 0x33, 0x0E, 0x1C, 0x2A, 0x38,
	0x46, 0x54, 0x62, 0x69, 0x70, 0x77, 0x79, 0x7B,
	0x7D, 0x7E, 0x01, 0x02, 0x01, 0x00, 0x09, 0x40,
	0x09, 0xBE, 0x19, 0xFC, 0x19, 0xFA, 0x19, 0xF8,
	0x1A, 0x38, 0x1A, 0x78, 0x1A, 0xB6, 0x2A, 0xF6,
	0x2B, 0x34, 0x2B, 0x74, 0x3B, 0x74, 0x6B, 0xF4,
};

/* 1362 Mbps */
static unsigned char SEQ_FFC[] = {
	0xC5,
	0x09, 0x10, 0x68, 0x45, 0x3B, 0x9A, 0x3B, 0x9A,
	0x3B, 0x9A, 0x3B, 0x9A,
};

static unsigned char PPS_TABLE[][88] = {
	{
		/* PPS MODE0 : 1440x3200, Slice Info : 720x40 */
		0x11, 0x00, 0x00, 0x89, 0x30, 0x80, 0x0C, 0x80,
		0x05, 0xA0, 0x00, 0x28, 0x02, 0xD0, 0x02, 0xD0,
		0x02, 0x00, 0x02, 0x68, 0x00, 0x20, 0x04, 0x6C,
		0x00, 0x0A, 0x00, 0x0C, 0x02, 0x77, 0x01, 0xE9,
		0x18, 0x00, 0x10, 0xF0, 0x03, 0x0C, 0x20, 0x00,
		0x06, 0x0B, 0x0B, 0x33, 0x0E, 0x1C, 0x2A, 0x38,
		0x46, 0x54, 0x62, 0x69, 0x70, 0x77, 0x79, 0x7B,
		0x7D, 0x7E, 0x01, 0x02, 0x01, 0x00, 0x09, 0x40,
		0x09, 0xBE, 0x19, 0xFC, 0x19, 0xFA, 0x19, 0xF8,
		0x1A, 0x38, 0x1A, 0x78, 0x1A, 0xB6, 0x2A, 0xF6,
		0x2B, 0x34, 0x2B, 0x74, 0x3B, 0x74, 0x6B, 0xF4
	},
	{
		/* PPS MODE1 : 1080x2400, Slice Info : 540x40 */
		0x11, 0x00, 0x00, 0x89, 0x30, 0x80, 0x09, 0x60,
		0x04, 0x38, 0x00, 0x28, 0x02, 0x1C, 0x02, 0x1C,
		0x02, 0x00, 0x02, 0x0E, 0x00, 0x20, 0x03, 0xDD,
		0x00, 0x07, 0x00, 0x0C, 0x02, 0x77, 0x02, 0x8B,
		0x18, 0x00, 0x10, 0xF0, 0x03, 0x0C, 0x20, 0x00,
		0x06, 0x0B, 0x0B, 0x33, 0x0E, 0x1C, 0x2A, 0x38,
		0x46, 0x54, 0x62, 0x69, 0x70, 0x77, 0x79, 0x7B,
		0x7D, 0x7E, 0x01, 0x02, 0x01, 0x00, 0x09, 0x40,
		0x09, 0xBE, 0x19, 0xFC, 0x19, 0xFA, 0x19, 0xF8,
		0x1A, 0x38, 0x1A, 0x78, 0x1A, 0xB6, 0x2A, 0xF6,
		0x2B, 0x34, 0x2B, 0x74, 0x3B, 0x74, 0x6B, 0xF4
	},
	{
		/* PPS MODE2 : 720x1600, Slice Info : 360x80 */
		0x11, 0x00, 0x00, 0x89, 0x30, 0x80, 0x06, 0x40,
		0x02, 0xD0, 0x00, 0x50, 0x01, 0x68, 0x01, 0x68,
		0x02, 0x00, 0x01, 0xB4, 0x00, 0x20, 0x06, 0x2F,
		0x00, 0x05, 0x00, 0x0C, 0x01, 0x38, 0x01, 0xE9,
		0x18, 0x00, 0x10, 0xF0, 0x03, 0x0C, 0x20, 0x00,
		0x06, 0x0B, 0x0B, 0x33, 0x0E, 0x1C, 0x2A, 0x38,
		0x46, 0x54, 0x62, 0x69, 0x70, 0x77, 0x79, 0x7B,
		0x7D, 0x7E, 0x01, 0x02, 0x01, 0x00, 0x09, 0x40,
		0x09, 0xBE, 0x19, 0xFC, 0x19, 0xFA, 0x19, 0xF8,
		0x1A, 0x38, 0x1A, 0x78, 0x1A, 0xB6, 0x2A, 0xF6,
		0x2B, 0x34, 0x2B, 0x74, 0x3B, 0x74, 0x6B, 0xF4
	},
};

static unsigned char SCALER_TABLE[][2] = {
	/* scaler off, 1440x3200 */
	{0xBA, 0x01},
	/* 1.78x scaler on, 1080x2400 */
	{0xBA, 0x02},
	/* 4x scaler on, 720x1600 */
	{0xBA, 0x00},
};

static unsigned char CASET_TABLE[][5] = {
	/* scaler off, 1440x3200 */
	{0x2A, 0x00, 0x00, 0x05, 0x9F},
	/* 1.78x scaler on, 1080x2400 */
	{0x2A, 0x00, 0x00, 0x04, 0x37},
	/* 4x scaler on, 720x1600 */
	{0x2A, 0x00, 0x00, 0x02, 0xCF},
};

static unsigned char PASET_TABLE[][5] = {
	/* scaler off, 1440x3200 */
	{0x2B, 0x00, 0x00, 0x0C, 0x7F},
	/* 1.78x scaler on, 1080x2400 */
	{0x2B, 0x00, 0x00, 0x09, 0x5F},
	/* 4x scaler on, 720x1600 */
	{0x2B, 0x00, 0x00, 0x06, 0x3F},
};

static bool s6e3had_aod_state = false;

static int s6e3had_suspend(struct exynos_panel_device *panel)
{
	struct dsim_device *dsim = get_dsim_drvdata(panel->id);

	DPU_INFO_PANEL("%s (aod_state : %d) +\n", __func__, s6e3had_aod_state);

	dsim_write_data_seq(dsim, false, 0xf0, 0x5a, 0x5a);

	dsim_write_data_seq(dsim, 10, 0x28); /* DISPOFF */
	dsim_write_data_seq(dsim, 120, 0x10); /* SLPIN */

	s6e3had_aod_state = false;

	DPU_INFO_PANEL("%s -\n", __func__);
	return 0;
}

static u32 s6e3had_find_table_index(u32 yres)
{
	u32 i, val;

	for (i = 0; i < MAX_SUPPORT_MRES; i++) {
		val = (PPS_TABLE[i][6] << 8) | (PPS_TABLE[i][7] << 0);
		if (yres == val) {
			DPU_INFO_PANEL("%s: found index=%d\n", __func__, i);
			return i;
		}
	}

	DPU_INFO_PANEL("%s: no match for yres(%d) -> forcing FHD+ mode\n",
			__func__, yres);
	/* FHD+ mode */
	return 1;
}

static int s6e3had_displayon(struct exynos_panel_device *panel)
{
	bool dsc_en;
	u32 yres, tab_idx;
	struct exynos_panel_info *lcd = &panel->lcd_info;
	struct dsim_device *dsim = get_dsim_drvdata(panel->id);
#if IS_ENABLED(CONFIG_EXYNOS_WINDOW_UPDATE)
	struct decon_rect rect;
	char column[5];
	char page[5];
#endif

	dsc_en = lcd->dsc.en;
	yres = lcd->yres;
	tab_idx = s6e3had_find_table_index(yres);

	DPU_INFO_PANEL("%s (aod_state : %d) +\n", __func__, s6e3had_aod_state);

	mutex_lock(&panel->ops_lock);

	dsim_write_data_seq(dsim, false, 0xf0, 0x5a, 0x5a);
	dsim_write_data_seq(dsim, false, 0xfc, 0x5a, 0x5a);

	if (s6e3had_aod_state) {
#if 0
		/* aod exit sequence guided from a customer */
		dsim_write_data_seq(dsim, false, 0xbb, 0x01);
		dsim_write_data_seq(dsim, false, 0xb0, 0x53, 0xb5);
		dsim_write_data_seq(dsim, false, 0xb5, 0x00);
		dsim_write_data_seq(dsim, false, 0xb0, 0x09, 0xf4);
		dsim_write_data_seq(dsim, false, 0xf4, 0xca);
		dsim_write_data_seq(dsim, false, 0x53, 0x00);
#else
		/* disable HLPM */
		dsim_write_data_seq(dsim, false, 0x53, 0x20); /* d5:BCTRL, d1:HLPM_ON, d0:HLPM_MODE */
#endif

#if IS_ENABLED(CONFIG_EXYNOS_WINDOW_UPDATE)
		/* send full area command */
		DPU_FULL_RECT(&rect, lcd);
		column[0] = MIPI_DCS_SET_COLUMN_ADDRESS;
		column[1] = (rect.left >> 8) & 0xff;
		column[2] = rect.left & 0xff;
		column[3] = (rect.right >> 8) & 0xff;
		column[4] = rect.right & 0xff;

		page[0] = MIPI_DCS_SET_PAGE_ADDRESS;
		page[1] = (rect.top >> 8) & 0xff;
		page[2] = rect.top & 0xff;
		page[3] = (rect.bottom >> 8) & 0xff;
		page[4] = rect.bottom & 0xff;

		if (dsim_write_data(dsim, MIPI_DSI_DCS_LONG_WRITE,
				(unsigned long)column, ARRAY_SIZE(column), true) != 0)
			dsim_err("failed to write COLUMN_ADDRESS\n");

		if (dsim_write_data(dsim, MIPI_DSI_DCS_LONG_WRITE,
				(unsigned long)page, ARRAY_SIZE(page), true) != 0)
			dsim_err("failed to write PAGE_ADDRESS\n");
#endif

		s6e3had_aod_state = false;
	} else {
		/* DSC related configuration */
		dsim_write_data_type_seq(dsim, MIPI_DSI_DSC_PRA, 0x1);
		if (lcd->dsc.slice_num == 2)
			dsim_write_data_type_table(dsim, MIPI_DSI_DSC_PPS, SEQ_PPS_SLICE2);
		else
			DPU_ERR_PANEL("fail to set MIPI_DSI_DSC_PPS command\n");

		dsim_write_data_seq_delay(dsim, 120, 0x11); /* sleep out: 120ms delay */
		dsim_write_data_seq(dsim, false, 0xB9, 0x00, 0xC0, 0x8C, 0x09, 0x00, 0x00,
				0x00, 0x11, 0x03);

		/* enable brightness control */
		dsim_write_data_seq_delay(dsim, false, 0x53, 0x20); /* BCTRL on */
		/* WRDISBV(51h) = 1st[7:0], 2nd[15:8] */
		dsim_write_data_seq_delay(dsim, false, 0x51, 0xff, 0x7f);

		dsim_write_data_seq(dsim, false, 0x35); /* TE on */

		/* ESD flag: [2]=VLIN3, [6]=VLIN1 error check*/
		dsim_write_data_seq(dsim, false, 0xED, 0x04, 0x44);


#if defined(CONFIG_EXYNOS_PLL_SLEEP) && defined(CONFIG_SOC_EXYNOS9830_EVT0)
		/* TE start timing is advanced due to latency for the PLL_SLEEP
		 *      default value : 3199(active line) + 15(vbp+1) - 2 = 0xC8C
		 *      modified value : default value - 11(modifying line) = 0xC81
		 */
		dsim_write_data_seq(dsim, false, 0xB9, 0x01, 0xC0, 0x81, 0x09);
#else
		/* Typical high duration: 123.57 (122~125us) */
		dsim_write_data_seq(dsim, false, 0xB9, 0x00, 0xC0, 0x8C, 0x09);
#endif

		dsim_write_data_table(dsim, SEQ_FFC);

		/* vrefresh rate configuration */
		if (panel->lcd_info.fps == 60)
			dsim_write_data_seq(dsim, false, 0x60, 0x00);
		else if (panel->lcd_info.fps == 120)
			dsim_write_data_seq(dsim, false, 0x60, 0x20);
		/* Panelupdate for vrefresh */
		dsim_write_data_seq(dsim, false, 0xF7, 0x0F);

		dsim_write_data_seq(dsim, false, 0x29); /* display on */


		/* for Non-WQHD+ mode */
		if (tab_idx != 0) {
			/*
			 * To prevent the screen noise display in the multi-resolution mode.
			 * If the last mode is FHD+ or HD+,
			 * noise can be seen during LCD on because WQHD+ mode
			 *
			 * It seems that a frame update is required for the SCALER_TABLE.
			 */
			dsim_write_data_seq(dsim, false, 0x9F, 0xA5, 0xA5);
			/* DSC related configuration */
			if (dsc_en) {
				dsim_write_data_type_seq(dsim, MIPI_DSI_DSC_PRA, 0x1);
				dsim_write_data_type_table(dsim, MIPI_DSI_DSC_PPS,
						PPS_TABLE[tab_idx]);
			} else {
				dsim_write_data_type_seq(dsim, MIPI_DSI_DSC_PRA, 0x0);
			}
			dsim_write_data_seq(dsim, false, 0x9F, 0x5A, 0x5A);

			/* partial update configuration */
			dsim_write_data_table(dsim, CASET_TABLE[tab_idx]);
			dsim_write_data_table(dsim, PASET_TABLE[tab_idx]);

			dsim_write_data_seq(dsim, false, 0xF0, 0x5A, 0x5A);
			/* DDI scaling configuration */
			dsim_write_data_table(dsim, SCALER_TABLE[tab_idx]);
			dsim_write_data_seq(dsim, false, 0xF0, 0xA5, 0xA5);
		}
	}

	mutex_unlock(&panel->ops_lock);

	DPU_INFO_PANEL("%s -\n", __func__);
	return 0;
}

static int s6e3had_mres(struct exynos_panel_device *panel, u32 mode_idx)
{
	bool dsc_en;
	u32 yres, tab_idx;
	struct dsim_device *dsim = get_dsim_drvdata(panel->id);

	dsc_en = panel->lcd_info.display_mode[mode_idx].dsc_en;
	yres = panel->lcd_info.display_mode[mode_idx].mode.height;
	tab_idx = s6e3had_find_table_index(yres);

	DPU_INFO_PANEL("%s +\n", __func__);

	mutex_lock(&panel->ops_lock);

	dsim_write_data_seq(dsim, false,  0x9F, 0xA5, 0xA5);
	/* DSC related configuration */
	if (dsc_en) {
		dsim_write_data_type_seq(dsim, MIPI_DSI_DSC_PRA, 0x1);
		dsim_write_data_type_table(dsim, MIPI_DSI_DSC_PPS,
				PPS_TABLE[tab_idx]);
	} else {
		dsim_write_data_type_seq(dsim, MIPI_DSI_DSC_PRA, 0x0);
	}
	dsim_write_data_seq(dsim, false,  0x9F, 0x5A, 0x5A);

	/* partial update configuration */
	dsim_write_data_table(dsim, CASET_TABLE[tab_idx]);
	dsim_write_data_table(dsim, PASET_TABLE[tab_idx]);

	dsim_write_data_seq(dsim, false,  0xF0, 0x5A, 0x5A);
	/* DDI scaling configuration */
	dsim_write_data_table(dsim, SCALER_TABLE[tab_idx]);
	dsim_write_data_seq(dsim, false,  0xF0, 0xA5, 0xA5);

	mutex_unlock(&panel->ops_lock);
	DPU_INFO_PANEL("%s -\n", __func__);

	return 0;
}

static int s6e3had_aod_displayon(struct exynos_panel_device *panel)
{
	bool dsc_en;
	u32 yres, tab_idx;
	struct exynos_panel_info *lcd = &panel->lcd_info;
	struct dsim_device *dsim = get_dsim_drvdata(panel->id);

	dsc_en = lcd->dsc.en;
	yres = lcd->yres;
	tab_idx = s6e3had_find_table_index(yres);

	DPU_INFO_PANEL("%s (aod_state : %d) +\n", __func__, s6e3had_aod_state);

	mutex_lock(&panel->ops_lock);

	dsim_write_data_seq(dsim, false, 0xf0, 0x5a, 0x5a);
	dsim_write_data_seq(dsim, false, 0xfc, 0x5a, 0x5a);

	/* DSC related configuration */
	dsim_write_data_type_seq(dsim, MIPI_DSI_DSC_PRA, 0x1);
	if (lcd->dsc.slice_num == 2)
		dsim_write_data_type_table(dsim, MIPI_DSI_DSC_PPS, SEQ_PPS_SLICE2);
	else
		DPU_ERR_PANEL("fail to set MIPI_DSI_DSC_PPS command\n");

	dsim_write_data_seq_delay(dsim, 120, 0x11); /* sleep out: 120ms delay */
	dsim_write_data_seq(dsim, false, 0xB9, 0x00, 0xC0, 0x8C, 0x09, 0x00, 0x00,
			0x00, 0x11, 0x03);

#if 0
	/* aod entry sequence guided from a customer */
	dsim_write_data_seq(dsim, false, 0xf0, 0x5a, 0x5a);
	dsim_write_data_seq(dsim, false, 0xb1, 0x0c, 0x65);
	dsim_write_data_seq(dsim, false, 0xf7, 0x0f);
	dsim_write_data_seq(dsim, false, 0xf0, 0xa5, 0xa5);
	dsim_write_data_seq(dsim, false, 0xf0, 0x5a, 0x5a);
	dsim_write_data_seq(dsim, false, 0xbb, 0x09, 0x0c, 0x0c, 0x43, 0x0c, 0x43, 0x80);
	dsim_write_data_seq(dsim, false, 0xfc, 0x5a, 0x5a);
	dsim_write_data_seq(dsim, false, 0xb0, 0x10, 0xf6);
	dsim_write_data_seq(dsim, false, 0xf6, 0xff);
	dsim_write_data_seq(dsim, false, 0xb0, 0x28, 0xfd);
	dsim_write_data_seq(dsim, false, 0xfd, 0x0c);
	dsim_write_data_seq(dsim, false, 0xfc, 0xa5, 0xa5);
	dsim_write_data_seq(dsim, false, 0x53, 0x03);   /* d1:HLPM_ON, d0:HLPM_MODE */
	dsim_write_data_seq(dsim, false, 0xb0, 0x09, 0xf4);
	dsim_write_data_seq(dsim, false, 0xf4, 0x8a);
	dsim_write_data_seq(dsim, false, 0xf7, 0x0f);
	dsim_write_data_seq(dsim, false, 0xf0, 0xa5, 0xa5);
#else
	/* enable brightness control & HLPM */
	dsim_write_data_seq_delay(dsim, false, 0x53, 0x22); /* d5:BCTRL, d1:HLPM_ON, d0:HLPM_MODE */
	/* WRDISBV(51h) = 1st[7:0], 2nd[15:8] */
	dsim_write_data_seq_delay(dsim, false, 0x51, 0xff, 0x7f);
#endif

	dsim_write_data_seq(dsim, false, 0x35); /* TE on */

	/* ESD flag: [2]=VLIN3, [6]=VLIN1 error check*/
	dsim_write_data_seq(dsim, false, 0xED, 0x04, 0x44);

#if defined(CONFIG_EXYNOS_PLL_SLEEP) && defined(CONFIG_SOC_EXYNOS9830_EVT0)
	/* TE start timing is advanced due to latency for the PLL_SLEEP
	 *      default value : 3199(active line) + 15(vbp+1) - 2 = 0xC8C
	 *      modified value : default value - 11(modifying line) = 0xC81
	 */
	dsim_write_data_seq(dsim, false, 0xB9, 0x01, 0xC0, 0x81, 0x09);
#else
	/* Typical high duration: 123.57 (122~125us) */
	dsim_write_data_seq(dsim, false, 0xB9, 0x00, 0xC0, 0x8C, 0x09);
#endif

	dsim_write_data_table(dsim, SEQ_FFC);

	/* vrefresh rate configuration */
	if (panel->lcd_info.fps == 60)
		dsim_write_data_seq(dsim, false, 0x60, 0x00);
	else if (panel->lcd_info.fps == 120)
		dsim_write_data_seq(dsim, false, 0x60, 0x20);
	/* Panelupdate for vrefresh */
	dsim_write_data_seq(dsim, false, 0xF7, 0x0F);

	dsim_write_data_seq(dsim, false, 0x29); /* display on */


	/* for Non-WQHD+ mode */
	if (tab_idx != 0) {
		/*
		 * To prevent the screen noise display in the multi-resolution mode.
		 * If the last mode is FHD+ or HD+,
		 * noise can be seen during LCD on because WQHD+ mode
		 *
		 * It seems that a frame update is required for the SCALER_TABLE.
		 */
		dsim_write_data_seq(dsim, false, 0x9F, 0xA5, 0xA5);
		/* DSC related configuration */
		if (dsc_en) {
			dsim_write_data_type_seq(dsim, MIPI_DSI_DSC_PRA, 0x1);
			dsim_write_data_type_table(dsim, MIPI_DSI_DSC_PPS,
					PPS_TABLE[tab_idx]);
		} else {
			dsim_write_data_type_seq(dsim, MIPI_DSI_DSC_PRA, 0x0);
		}
		dsim_write_data_seq(dsim, false, 0x9F, 0x5A, 0x5A);

		/* partial update configuration */
		dsim_write_data_table(dsim, CASET_TABLE[tab_idx]);
		dsim_write_data_table(dsim, PASET_TABLE[tab_idx]);

		dsim_write_data_seq(dsim, false, 0xF0, 0x5A, 0x5A);
		/* DDI scaling configuration */
		dsim_write_data_table(dsim, SCALER_TABLE[tab_idx]);
		dsim_write_data_seq(dsim, false, 0xF0, 0xA5, 0xA5);
	}

	s6e3had_aod_state = true;

	mutex_unlock(&panel->ops_lock);

	DPU_INFO_PANEL("%s -\n", __func__);
	return 0;
}

static int s6e3had_doze(struct exynos_panel_device *panel)
{
	DPU_INFO_PANEL("%s (aod_state : %d) +\n", __func__, s6e3had_aod_state);

	if (!s6e3had_aod_state)
		s6e3had_aod_displayon(panel);

	DPU_INFO_PANEL("%s -\n", __func__);

	return 0;
}

static int s6e3had_doze_suspend(struct exynos_panel_device *panel)
{
	DPU_INFO_PANEL("%s (aod_state : %d)\n", __func__, s6e3had_aod_state);

	return 0;
}

static int s6e3had_dump(struct exynos_panel_device *panel)
{
	return 0;
}

static int s6e3had_read_state(struct exynos_panel_device *panel)
{
	return 0;
}

static int s6e3had_set_light(struct exynos_panel_device *panel, u32 br_val)
{
	u8 data[2] = {0, };
	struct dsim_device *dsim = get_dsim_drvdata(panel->id);

	DPU_DEBUG_PANEL("%s +\n", __func__);

	mutex_lock(&panel->ops_lock);

	DPU_INFO_PANEL("requested brightness value = %d\n", br_val);

	data[0] = br_val & 0xFF;
	dsim_write_data_seq(dsim, false, 0x51, data[0]);

	mutex_unlock(&panel->ops_lock);

	DPU_DEBUG_PANEL("%s -\n", __func__);
	return 0;
}

static int s6e3had_set_vrefresh(struct exynos_panel_device *panel, u32 refresh)
{
	struct dsim_device *dsim = get_dsim_drvdata(panel->id);

	DPU_DEBUG_PANEL("%s +\n", __func__);
	DPU_DEBUG_PANEL("applied vrefresh(%d), requested vrefresh(%d) resol(%dx%d)\n",
			panel->lcd_info.fps, refresh,
			panel->lcd_info.xres, panel->lcd_info.yres);

	if (panel->lcd_info.fps == refresh) {
		DPU_INFO_PANEL("prev and req fps are same(%d)\n", refresh);
		return 0;
	}

	mutex_lock(&panel->ops_lock);

	dsim_write_data_seq(dsim, false, 0xF0, 0x5A, 0x5A);

	if (refresh == 60) {
		dsim_write_data_seq(dsim, false, 0x60, 0x00);
	} else if (refresh == 120) {
		dsim_write_data_seq(dsim, false, 0x60, 0x08);
	} else {
		DPU_INFO_PANEL("not supported fps(%d)\n", refresh);
		goto end;
	}

	/* Panelupdate : gamma set, ltps set, transition control update */
	dsim_write_data_seq(dsim, false, 0xF7, 0x0F);

	panel->lcd_info.fps = refresh;

end:
	dsim_write_data_seq(dsim, true, 0xF0, 0xA5, 0xA5);

	mutex_unlock(&panel->ops_lock);
	DPU_DEBUG_PANEL("%s -\n", __func__);

	return 0;
}

struct exynos_panel_ops panel_s6e3had_ops = {
	.id		= {0xE12680, 0xE52691, 0xE32690, 0xE22690},
	.suspend	= s6e3had_suspend,
	.displayon	= s6e3had_displayon,
	.mres		= s6e3had_mres,
	.doze		= s6e3had_doze,
	.doze_suspend	= s6e3had_doze_suspend,
	.dump		= s6e3had_dump,
	.read_state	= s6e3had_read_state,
	.set_light	= s6e3had_set_light,
	.set_vrefresh	= s6e3had_set_vrefresh,
};
