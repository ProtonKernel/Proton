/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) Samsung Electronics Co., Ltd.
 * Gwanghui Lee <gwanghui.lee@samsung.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#ifndef __PANEL_VRR_H__
#define __PANEL_VRR_H__

#define MIN_VRR_DIV_COUNT	(1U)

#define VRR_LFD_ARG_KEY_CLIENT		("client")
#define VRR_LFD_ARG_KEY_SCOPE		("scope")
#define VRR_LFD_ARG_KEY_FIX			("fix")
#define VRR_LFD_ARG_KEY_SCALABILITY	("scalability")
#define VRR_LFD_ARG_KEY_MIN			("min")
#define VRR_LFD_ARG_KEY_MAX			("max")

enum {
	VRR_LFD_FREQ_NONE = 0,
	VRR_LFD_FREQ_HIGH = 1,
	VRR_LFD_FREQ_LOW = 2,
	MAX_VRR_LFD_FREQ
};

enum {
	VRR_LFD_CLIENT_FAC,
	VRR_LFD_CLIENT_DISP,
	VRR_LFD_CLIENT_INPUT,
	VRR_LFD_CLIENT_AOD,
	VRR_LFD_CLIENT_VID,
	MAX_VRR_LFD_CLIENT,
};

/*
 * vrr lfd applying scopes are separated
 * according to panel-bl sub-device.
 */
enum {
	VRR_LFD_SCOPE_NORMAL,
	VRR_LFD_SCOPE_HMD,
	VRR_LFD_SCOPE_LPM,
	MAX_VRR_LFD_SCOPE,
};

enum {
	VRR_LFD_NOT_UPDATED = 0,
	VRR_LFD_UPDATED = 1,
};

/* TODO: determined by panel config */
enum {
	VRR_LFD_SCALABILITY_NONE = 0,
	VRR_LFD_SCALABILITY_MIN = 1,
	VRR_LFD_SCALABILITY_1 = 1,
	VRR_LFD_SCALABILITY_2 = 2,
	VRR_LFD_SCALABILITY_3 = 3,
	VRR_LFD_SCALABILITY_4 = 4,
	VRR_LFD_SCALABILITY_5 = 5,
	VRR_LFD_SCALABILITY_6 = 6,
	VRR_LFD_SCALABILITY_7 = 7,
	VRR_LFD_SCALABILITY_8 = 8,
	VRR_LFD_SCALABILITY_9 = 9,
	VRR_LFD_SCALABILITY_10 = 10,
	VRR_LFD_SCALABILITY_MAX = 128,
};

#define VRR_LFD_SCOPE_MASK(x)   (1U << x)
#define VRR_LFD_SCOPE_NORMAL_MASK   VRR_LFD_SCOPE_MASK(VRR_LFD_SCOPE_NORMAL)
#define VRR_LFD_SCOPE_HMD_MASK  VRR_LFD_SCOPE_MASK(VRR_LFD_SCOPE_HMD)
#define VRR_LFD_SCOPE_LPM_MASK  VRR_LFD_SCOPE_MASK(VRR_LFD_SCOPE_LPM)
#define VRR_LFD_SCOPE_ALL_MASK  \
	    (VRR_LFD_SCOPE_NORMAL_MASK | VRR_LFD_SCOPE_HMD_MASK | VRR_LFD_SCOPE_LPM_MASK)

struct vrr_lfd_config {
	u32 fix;
	u32 scalability;
	u32 min;
	u32 max;
};

struct vrr_lfd_status {
	int lfd_min_freq;
	int lfd_max_freq;
	int lfd_min_freq_div;
	int lfd_max_freq_div;
};

struct vrr_lfd_info {
	/* requested lfd config */
	struct vrr_lfd_config req[MAX_VRR_LFD_CLIENT][MAX_VRR_LFD_SCOPE];

	/* applied lfd config */
	struct vrr_lfd_config cur[MAX_VRR_LFD_SCOPE];

	/* lfd status */
	struct vrr_lfd_status status[MAX_VRR_LFD_SCOPE];
};

bool panel_vrr_is_supported(struct panel_device *panel);
const char *get_vrr_lfd_client_name(int index);
int find_vrr_lfd_client_name(const char *name);
const char *get_vrr_lfd_scope_name(int index);
int find_vrr_lfd_scope_name(const char *name);
int update_vrr_lfd(struct vrr_lfd_info *vrr_lfd_info);
struct panel_vrr *get_panel_vrr(struct panel_device *panel);
int get_panel_refresh_rate(struct panel_device *panel);
int get_panel_refresh_mode(struct panel_device *panel);
#ifdef CONFIG_PANEL_VRR_BRIDGE
bool panel_vrr_bridge_is_reached_target_nolock(struct panel_device *panel);
bool panel_vrr_bridge_is_supported(struct panel_device *panel);
bool panel_vrr_bridge_changeable(struct panel_device *panel);
int panel_vrr_bridge_thread(void *data);
#else
static inline int panel_vrr_bridge_thread(void *data) { return 0; }
#endif
#endif /* __PANEL_VRR_H__ */
