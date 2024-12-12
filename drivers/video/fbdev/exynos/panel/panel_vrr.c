// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) Samsung Electronics Co., Ltd.
 * Gwanghui Lee <gwanghui.lee@samsung.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#include "panel.h"
#include "panel_drv.h"
#include "panel_vrr.h"
#include "panel_debug.h"

const char *vrr_lfd_client_name[MAX_VRR_LFD_CLIENT] = {
	[VRR_LFD_CLIENT_FAC] = "fac",
	[VRR_LFD_CLIENT_DISP] = "disp",
	[VRR_LFD_CLIENT_INPUT] = "input",
	[VRR_LFD_CLIENT_AOD] = "aod",
	[VRR_LFD_CLIENT_VID] = "vid",
};

const char *vrr_lfd_scope_name[MAX_VRR_LFD_SCOPE] = {
	[VRR_LFD_SCOPE_NORMAL] = "normal",
	[VRR_LFD_SCOPE_HMD] = "hmd",
	[VRR_LFD_SCOPE_LPM] = "lpm",
};

const char *get_vrr_lfd_client_name(int index)
{
	if (index < 0 || index >= MAX_VRR_LFD_CLIENT)
		return NULL;

	return vrr_lfd_client_name[index];
}

int find_vrr_lfd_client_name(const char *name)
{
	int i;

	if (name == NULL)
		return -EINVAL;

	for (i = 0; i < MAX_VRR_LFD_CLIENT; i++)
		if (!strcmp(vrr_lfd_client_name[i], name))
			break;

	if (i == MAX_VRR_LFD_CLIENT)
		return -EINVAL;

	return i;
}

const char *get_vrr_lfd_scope_name(int index)
{
	if (index < 0 || index >= MAX_VRR_LFD_SCOPE)
		return NULL;

	return vrr_lfd_scope_name[index];
}

int find_vrr_lfd_scope_name(const char *name)
{
	int i;

	if (name == NULL)
		return -EINVAL;

	for (i = 0; i < MAX_VRR_LFD_SCOPE; i++)
		if (!strcmp(vrr_lfd_scope_name[i], name))
			break;

	if (i == MAX_VRR_LFD_SCOPE)
		return -EINVAL;

	return i;
}

int update_vrr_lfd(struct vrr_lfd_info *vrr_lfd_info)
{
	int i, scope;
	u32 lfd_fix, lfd_min, lfd_max, lfd_scalability;
	static struct vrr_lfd_info old_vrr_lfd_info;
	int updated = VRR_LFD_NOT_UPDATED;

	if (vrr_lfd_info == NULL)
		return -EINVAL;

	/* fix */
	for (scope = 0; scope < MAX_VRR_LFD_SCOPE; scope++) {
		lfd_fix = VRR_LFD_FREQ_NONE;
		for (i = 0; i < MAX_VRR_LFD_CLIENT; i++) {
			if (vrr_lfd_info->req[i][scope].fix != VRR_LFD_FREQ_NONE) {
				lfd_fix = vrr_lfd_info->req[i][scope].fix;
				panel_info("client:%s scope:%s fix:%d\n",
						get_vrr_lfd_client_name(i),
						get_vrr_lfd_scope_name(scope),
						vrr_lfd_info->req[i][scope].fix);
				break;
			}
		}

		vrr_lfd_info->cur[scope].fix = lfd_fix;
		if (old_vrr_lfd_info.cur[scope].fix != vrr_lfd_info->cur[scope].fix) {
			panel_info("scope:%s fix:%d->%d\n",
					get_vrr_lfd_scope_name(scope),
					old_vrr_lfd_info.cur[scope].fix,
					vrr_lfd_info->cur[scope].fix);
			updated = VRR_LFD_UPDATED;
		}
	}

	/* scalability */
	for (scope = 0; scope < MAX_VRR_LFD_SCOPE; scope++) {
		lfd_scalability = VRR_LFD_SCALABILITY_MAX;
		for (i = 0; i < MAX_VRR_LFD_CLIENT; i++) {
			/* scalability changed */
			if (vrr_lfd_info->req[i][scope].scalability != VRR_LFD_SCALABILITY_NONE) {
				lfd_scalability = min(lfd_scalability,
						vrr_lfd_info->req[i][scope].scalability);
				panel_info("client:%s scope:%s scalability:%d\n",
						get_vrr_lfd_client_name(i),
						get_vrr_lfd_scope_name(scope),
						vrr_lfd_info->req[i][scope].scalability);
			}
		}

		vrr_lfd_info->cur[scope].scalability =
			(lfd_scalability == VRR_LFD_SCALABILITY_MAX) ?
			VRR_LFD_SCALABILITY_NONE : lfd_scalability;
		if (old_vrr_lfd_info.cur[scope].scalability !=
				vrr_lfd_info->cur[scope].scalability) {
			panel_info("scope:%s scalability:%d->%d\n",
					get_vrr_lfd_scope_name(scope),
					old_vrr_lfd_info.cur[scope].scalability,
					vrr_lfd_info->cur[scope].scalability);
			updated = VRR_LFD_UPDATED;
		}
	}

	/* min */
	for (scope = 0; scope < MAX_VRR_LFD_SCOPE; scope++) {
		lfd_min = 0;
		for (i = 0; i < MAX_VRR_LFD_CLIENT; i++) {
			if (vrr_lfd_info->req[i][scope].min != 0)
				panel_info("client:%s scope:%s min:%d\n",
						get_vrr_lfd_client_name(i),
						get_vrr_lfd_scope_name(scope),
						vrr_lfd_info->req[i][scope].min);
			lfd_min = max(lfd_min, vrr_lfd_info->req[i][scope].min);
		}

		vrr_lfd_info->cur[scope].min = lfd_min;
		if (old_vrr_lfd_info.cur[scope].min !=
				vrr_lfd_info->cur[scope].min) {
			panel_info("scope:%s min:%d->%d\n",
					get_vrr_lfd_scope_name(scope),
					old_vrr_lfd_info.cur[scope].min,
					vrr_lfd_info->cur[scope].min);
			updated = VRR_LFD_UPDATED;
		}
	}

	/* max */
	for (scope = 0; scope < MAX_VRR_LFD_SCOPE; scope++) {
		lfd_max = 0;
		for (i = 0; i < MAX_VRR_LFD_CLIENT; i++) {
			if (vrr_lfd_info->req[i][scope].max != 0)
				panel_info("client:%s scope:%s max:%d\n",
						get_vrr_lfd_client_name(i),
						get_vrr_lfd_scope_name(scope),
						vrr_lfd_info->req[i][scope].max);
			lfd_max = max(lfd_max, vrr_lfd_info->req[i][scope].max);
		}

		vrr_lfd_info->cur[scope].max = lfd_max;
		if (old_vrr_lfd_info.cur[scope].max !=
				vrr_lfd_info->cur[scope].max) {
			panel_info("scope:%s max:%d->%d\n",
					get_vrr_lfd_scope_name(scope),
					old_vrr_lfd_info.cur[scope].max,
					vrr_lfd_info->cur[scope].max);
			updated = VRR_LFD_UPDATED;
		}
	}

	memcpy(&old_vrr_lfd_info,
			vrr_lfd_info, sizeof(struct vrr_lfd_info));

	return updated;
}

bool panel_vrr_is_supported(struct panel_device *panel)
{
	struct panel_info *data = &panel->panel_data;

	panel_dbg("support_vrr %s vrrtbl %s nr_vrrtbl %d\n",
			data->ddi_props.support_vrr ? "true" : "false",
			data->vrrtbl == NULL ? "is null" : "is not null",
			data->nr_vrrtbl);

	if (!data->ddi_props.support_vrr)
		return false;

	if (data->vrrtbl == NULL || data->nr_vrrtbl < 1)
		return false;

	return true;
}

bool panel_vrr_is_valid(struct panel_device *panel)
{
	struct panel_info *data = &panel->panel_data;
	struct panel_properties *props = &data->props;

	if (panel_vrr_is_supported(panel) == false)
		return false;

	if (data->nr_vrrtbl <= props->vrr_idx) {
		panel_warn("vrr_idx(%d) exceed number of vrrtbl(%d)\n",
				props->vrr_idx, data->nr_vrrtbl);
		return false;
	}

	return true;
}

struct panel_vrr *get_panel_vrr(struct panel_device *panel)
{
	struct panel_properties *props = &panel->panel_data.props;

	if (panel_vrr_is_valid(panel) == false)
		return NULL;

	return panel->panel_data.vrrtbl[props->vrr_idx];
}
EXPORT_SYMBOL(get_panel_vrr);

int get_panel_refresh_rate(struct panel_device *panel)
{
	struct panel_vrr *vrr;

	vrr = get_panel_vrr(panel);
	if (vrr == NULL)
		return -EINVAL;

	return vrr->fps;
}
EXPORT_SYMBOL(get_panel_refresh_rate);

int get_panel_refresh_mode(struct panel_device *panel)
{
	struct panel_vrr *vrr;

	vrr = get_panel_vrr(panel);
	if (vrr == NULL)
		return -EINVAL;

	return vrr->mode;
}
EXPORT_SYMBOL(get_panel_refresh_mode);

#ifdef CONFIG_PANEL_VRR_BRIDGE
bool panel_vrr_bridge_is_supported(struct panel_device *panel)
{
	struct common_panel_display_modes *common_panel_modes =
		panel->panel_data.common_panel_modes;

	if (!panel_display_mode_is_supported(panel)) {
		panel_warn("panel_display_mode not supported\n");
		return false;
	}

	if (!panel_vrr_is_supported(panel))
		return false;

	if (!common_panel_modes->bridges)
		return false;

	return true;
}

bool panel_vrr_bridge_changeable(struct panel_device *panel)
{
	struct common_panel_display_modes *common_panel_modes =
		panel->panel_data.common_panel_modes;
	struct common_panel_display_mode_bridge_ops *bridge_ops;
	struct panel_properties *props =
		&panel->panel_data.props;

	if (!panel_vrr_bridge_is_supported(panel)) {
		panel_warn("panel_vrr_bridge not supported\n");
		return false;
	}

	if (!common_panel_modes ||
		!props->vrr_bridge_enable ||
		panel->state.cur_state != PANEL_STATE_NORMAL)
		return false;

	bridge_ops = common_panel_modes->bridge_ops;
	if (!bridge_ops || !bridge_ops->check_changeable)
		return false;

	return bridge_ops->check_changeable(panel);
}

bool panel_vrr_bridge_is_reached_target_nolock(struct panel_device *panel)
{
	struct panel_properties *props =
		&panel->panel_data.props;

	return (props->target_panel_mode == props->panel_mode);
}

bool panel_vrr_bridge_is_reached_target(struct panel_device *panel)
{
	bool reached;

	mutex_lock(&panel->op_lock);
	reached = panel_vrr_bridge_is_reached_target_nolock(panel);
	mutex_unlock(&panel->op_lock);

	return reached;
}

static struct common_panel_display_mode_bridge *
panel_display_mode_get_bridge(struct panel_device *panel, u32 from, u32 to)
{
	struct common_panel_display_modes *common_panel_modes =
		panel->panel_data.common_panel_modes;
	struct common_panel_display_mode_bridge *bridge;

	if (!panel_display_mode_is_supported(panel)) {
		panel_err("panel_display_mode not supported\n");
		return NULL;
	}

	if (!common_panel_modes->bridges)
		return NULL;

	if (from >= common_panel_modes->num_modes ||
			to >= common_panel_modes->num_modes)
		return NULL;


	bridge = common_panel_modes->bridges +
		(from * common_panel_modes->num_modes) + to;
	if (bridge->mode == NULL)
		return NULL;

	return bridge;
}

int panel_vrr_bridge_set_display_mode(struct panel_device *panel)
{
	struct panel_properties *props =
		&panel->panel_data.props;
	int i, ret = 0, panel_mode, nframe_duration = 1;
	struct common_panel_display_mode_bridge *bridge;

	/*
	 * change display_mode at once caese
	 * normal case:
	 *  a. brightness changed.
	 *
	 * error case:
	 *  a. could not find next bridge cpdm.
	 *  b. brightness too low to change vrr seamlessly.
	 */

	mutex_lock(&panel->op_lock);
	panel_mode = props->target_panel_mode;
	if (panel_vrr_bridge_is_reached_target_nolock(panel)) {
		panel_info("display_mode(%d) reached to target\n",
				panel_mode);
		mutex_unlock(&panel->op_lock);
		return 0;
	}

	if (!panel_vrr_bridge_changeable(panel)) {
		panel_info("apply directly\n");
		goto apply_refresh_rate;
	}

	/*
	 * vrr-bridge action: 1. find next panel_mode
	 * a. detemine bridge-refresh-rate.
	 *    find bridge-refresh-rate.
	 *    if true, determine next bridge-refresh-rate.
	 *    if false, goto target-refresh-rate directly.
	 * b. determine duration(n-vsync with bridge-refresh-rate)
	 */

	bridge = panel_display_mode_get_bridge(panel,
			props->panel_mode, props->target_panel_mode);
	if (bridge && bridge->mode) {
		panel_dbg("found bridge %s\n", bridge->mode->name);
		panel_mode =
			find_panel_mode_by_common_panel_display_mode(panel, bridge->mode);
		nframe_duration = bridge->nframe_duration;
		if (panel_mode < 0) {
			panel_err("cpdm bridge not found\n");
			panel_mode = props->target_panel_mode;
			nframe_duration = 1;
		}
	}

	/*
	 * vrr-bridge action: 2. apply next panel_mode
	 * a. apply bridge-refresh-rate
	 * b. wait for n-vsync under bridge-refresh-rate
	 */

apply_refresh_rate:
	panel_info("update panel_mode %d->%d\n",
			props->panel_mode, panel_mode);
	props->panel_mode = panel_mode;
	mutex_unlock(&panel->op_lock);

	ret = panel_update_display_mode(panel);
	if (ret < 0) {
		panel_err("failed to panel_update_display_mode\n");
		return ret;
	}

	for (i = 0; i < nframe_duration; i++) {
		ret = panel_dsi_wait_for_vsync(panel, 500);
		if (ret < 0) {
			panel_err("failed to panel_dsi_wait_for_vsync\n");
			return ret;
		}
	}

	return 0;
}

int panel_vrr_bridge_thread(void *data)
{
	struct panel_device *panel = data;
	int ret;
	bool should_stop = false;

	if (unlikely(!panel)) {
		panel_warn("panel is null\n");
		return 0;
	}

	if (panel->state.connect_panel == PANEL_DISCONNECT) {
		panel_warn("panel no use\n");
		return -ENODEV;
	}

	while (!kthread_should_stop()) {
		ret = wait_event_interruptible(
				panel->thread[PANEL_THREAD_VRR_BRIDGE].wait,
				(should_stop =
				 panel->thread[PANEL_THREAD_VRR_BRIDGE].should_stop ||
				 kthread_should_stop()) ||
				!panel_vrr_bridge_is_reached_target(panel));

		if (should_stop)
			break;

		panel_wake_lock(panel, WAKE_TIMEOUT_MSEC);
		ret = panel_vrr_bridge_set_display_mode(panel);
		if (ret < 0)
			panel_err("failed to panel_vrr_bridge_set_display_mode\n");
		panel_wake_unlock(panel);
	}

	return 0;
}
#endif
