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

#ifndef __PANEL_DISPLAY_MODES_H__
#define __PANEL_DISPLAY_MODES_H__

#define PANEL_DISPLAY_MODE_NAME_LEN	(64)

/**
 * @REFRESH_MODE_NS: normal speed refresh mode
 * @REFRESH_MODE_HS: high speed refresh mode
 */
enum refresh_mode {
	REFRESH_MODE_NS,
	REFRESH_MODE_HS,
	MAX_REFRESH_MODE,
};

#define REFRESH_MODE_STR(_refresh_mode_) \
	((_refresh_mode_ == (REFRESH_MODE_NS)) ? "NS" : "HS")

struct panel_display_mode {
	/**
	 * @head:
	 *
	 * struct list_head for mode lists.
	 */
	struct list_head head;

	/**
	 * @name:
	 *
	 * Human-readable name of the mode, filled out with panel_display_mode_set_name().
	 */
	char name[PANEL_DISPLAY_MODE_NAME_LEN];

	unsigned int width;
	unsigned int height;
	unsigned int refresh_rate;
	unsigned int refresh_mode;
	unsigned int panel_refresh_rate;
	unsigned int panel_refresh_mode;
	unsigned int panel_te_st;
	unsigned int panel_te_ed;
	unsigned int panel_te_sw_skip_count;
	unsigned int panel_te_hw_skip_count;
	/* dsc parameters */
	bool dsc_en;
	unsigned int dsc_cnt;
	unsigned int dsc_slice_num;
	unsigned int dsc_slice_w;
	unsigned int dsc_slice_h;
	/*
	 * TODO: move to display controller's display mode structure.
	 *
	 * panel_display_mode contains common parameters
	 * between display controller and panel driver.
	 * @cmd_lp_ref is only necessary in display controller
	 * especially for dsim driver. so it need to move to
	 * display controller's display mode structure.
	 */
	/* dsi parameters */
	unsigned int cmd_lp_ref;

	void *pdata;
};

struct panel_display_modes {
	unsigned int num_modes;
	unsigned int native_mode;

	struct panel_display_mode **modes;
};

#define PANEL_MODE_FMT    "\"%s\" %d %d %d %d %d %d %d %d %d %d %d %d %d %d %d %d"
#define PANEL_MODE_ARG(m) \
	    (m)->name, (m)->width, (m)->height, \
    (m)->refresh_rate, (m)->refresh_mode, \
    (m)->panel_refresh_rate, (m)->panel_refresh_mode, \
    (m)->panel_te_st, (m)->panel_te_ed, \
    (m)->panel_te_sw_skip_count, (m)->panel_te_hw_skip_count, \
    (m)->dsc_en, (m)->dsc_cnt, (m)->dsc_slice_num, (m)->dsc_slice_w, (m)->dsc_slice_h, \
    (m)->cmd_lp_ref

void panel_mode_set_name(struct panel_display_mode *mode);
struct panel_display_mode *panel_mode_create(void);
struct panel_display_modes *of_get_panel_display_modes(const struct device_node *np);
#endif /* __PANEL_DISPLAY_MODES_H__ */
