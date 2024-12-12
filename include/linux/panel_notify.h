/*
 * linux/drivers/video/fbdev/exynos/panel/panel_notify.h
 *
 * Header file for Samsung Common LCD Driver.
 *
 * Copyright (c) 2018 Samsung Electronics
 * Gwanghui Lee <gwanghui.lee@samsung.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#ifndef __PANEL_NOTIFY_H__
#define __PANEL_NOTIFY_H__

enum panel_notifier_event_t {
	PANEL_EVENT_LCD_CHANGED,
	PANEL_EVENT_BL_CHANGED,
	PANEL_EVENT_VRR_CHANGED,
	PANEL_EVENT_STATE_CHANGED,
	PANEL_EVENT_LFD_CHANGED,
	PANEL_EVENT_UB_CON_CHANGED,
	PANEL_EVENT_POWER_STATE_CHANGED,
	PANEL_EVENT_COPR_STATE_CHANGED,
};

enum panel_notifier_event_ub_con_state {
	PANEL_EVENT_UB_CON_CONNECTED = 0,
	PANEL_EVENT_UB_CON_DISCONNECTED = 1,
};

enum panel_notifier_event_panel_power_state {
	PANEL_EVENT_PANEL_POWER_OFF = 0,
	PANEL_EVENT_PANEL_POWER_ON = 1,
	/*
	TO DO: 	doze, hiber and so on
	*/
};

enum panel_notifier_event_copr_state {
	PANEL_EVENT_COPR_DISABLED = 0,
	PANEL_EVENT_COPR_ENABLED = 1,
};

struct panel_power_state_event_data {
   enum panel_notifier_event_panel_power_state state;
};

struct panel_ub_con_event_data {
   enum panel_notifier_event_ub_con_state state;
};

struct panel_copr_event_data {
   enum panel_notifier_event_copr_state state;
};

struct panel_bl_event_data {
	int brightness;
	int aor_ratio;
};

struct panel_dms_data {
	int fps;
	int lfd_min_freq;
	int lfd_max_freq;
};

#ifdef CONFIG_PANEL_NOTIFY
extern int panel_notifier_register(struct notifier_block *nb);
extern int panel_notifier_unregister(struct notifier_block *nb);
extern int panel_notifier_call_chain(unsigned long val, void *v);
#else
static inline int panel_notifier_register(struct notifier_block *nb)
{
	return 0;
};

static inline int panel_notifier_unregister(struct notifier_block *nb)
{
	return 0;
};

static inline int panel_notifier_call_chain(unsigned long val, void *v)
{
	return 0;
};
#endif /* CONFIG_PANEL_NOTIFY */
#endif /* __PANEL_NOTIFY_H__ */

