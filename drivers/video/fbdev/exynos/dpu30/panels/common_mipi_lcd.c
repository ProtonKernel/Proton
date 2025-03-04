/* drivers/video/exynos/decon/panels/common_mipi_lcd.c
 *
 * Samsung SoC MIPI LCD driver.
 *
 * Copyright (c) 2017 Samsung Electronics
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
*/

#include <linux/delay.h>
#include <linux/gpio.h>
#include <linux/mutex.h>
#include <video/mipi_display.h>
#include <linux/platform_device.h>

#include "../disp_err.h"
#include "../decon.h"
#include "../dsim.h"
#include "../mcd_decon.h"
#include "panel.h"
#include "panel_drv.h"
#include "panel_debug.h"
#include "exynos_panel_drv.h"
#include "exynos_panel.h"

#ifdef CONFIG_PANEL_DISPLAY_MODE
#include "exynos_panel_modes.h"
#endif

static DEFINE_MUTEX(cmd_lock);
struct panel_state *panel_state;
struct panel_mres *mres;

static int common_panel_set_display_mode(struct exynos_panel_device *panel, void *data);

static int panel_drv_ioctl(struct exynos_panel_device *panel, u32 cmd, void *arg)
{
	int ret;

	if (unlikely(!panel || !panel->panel_drv_sd))
		return -EINVAL;

	ret = v4l2_subdev_call(panel->panel_drv_sd, core, ioctl, cmd, arg);

	return ret;
}

static int panel_drv_notify(struct v4l2_subdev *sd,
		unsigned int notification, void *arg)
{
	struct v4l2_event *ev = (struct v4l2_event *)arg;
	int ret;

	if (notification != V4L2_DEVICE_NOTIFY_EVENT) {
		DPU_DEBUG_PANEL("%s: unknown event\n", __func__);
		return -EINVAL;
	}

	switch (ev->type) {
	case V4L2_EVENT_DECON_FRAME_DONE:
		ret = v4l2_subdev_call(sd, core, ioctl,
				PANEL_IOC_EVT_FRAME_DONE, &ev->timestamp);
		if (ret) {
			DPU_ERR_PANEL("%s: failed to notify FRAME_DONE\n", __func__);
			return ret;
		}
		break;
	case V4L2_EVENT_DECON_VSYNC:
		ret = v4l2_subdev_call(sd, core, ioctl,
				PANEL_IOC_EVT_VSYNC, &ev->timestamp);
		if (ret) {
			DPU_ERR_PANEL("%s: failed to notify VSYNC\n", __func__);
			return ret;
		}
		break;
	default:
		DPU_ERR_PANEL("%s: unknown event type %d\n", __func__, ev->type);
		break;
	}

	DPU_DEBUG_PANEL("%s: event type %d timestamp %ld %ld nsec\n", __func__,
			ev->type, ev->timestamp.tv_sec,
			ev->timestamp.tv_nsec);

	return 0;
}

static int common_panel_set_error_cb(struct exynos_panel_device *panel, void *arg)
{
	int ret;

	v4l2_set_subdev_hostdata(panel->panel_drv_sd, arg);
	ret = panel_drv_ioctl(panel, PANEL_IOC_REG_RESET_CB, NULL);
	if (ret) {
		DPU_ERR_PANEL("%s: failed to set panel error callback\n", __func__);
		return ret;
	}

	return 0;
}

static int panel_drv_probe(struct exynos_panel_device *panel)
{
	int ret;

	ret = panel_drv_ioctl(panel, PANEL_IOC_DSIM_PROBE, (void *)&panel->id);
	if (ret) {
		DPU_ERR_PANEL("%s: failed to panel dsim probe\n", __func__);
		return ret;
	}

	return ret;
}

static int panel_drv_get_state(struct exynos_panel_device *panel)
{
	int ret = 0;

	ret = panel_drv_ioctl(panel, PANEL_IOC_GET_PANEL_STATE, NULL);
	if (ret) {
		DPU_ERR_PANEL("%s: failed to get panel state", __func__);
		return ret;
	}

	panel_state = (struct panel_state *)
		v4l2_get_subdev_hostdata(panel->panel_drv_sd);
	if (IS_ERR_OR_NULL(panel_state)) {
		DPU_ERR_PANEL("%s: failed to get lcd information\n", __func__);
		return -EINVAL;
	}

	return ret;
}

#if defined(CONFIG_PANEL_DISPLAY_MODE)
static int panel_drv_get_panel_display_mode(struct exynos_panel_device *panel)
{
	struct exynos_panel_info *info;
	struct panel_display_modes *panel_modes;
	struct exynos_display_modes *exynos_modes;
	int ret;

	if (!panel)
		return -EINVAL;

	info = &panel->lcd_info;
	ret = panel_drv_ioctl(panel, PANEL_IOC_GET_DISPLAY_MODE, NULL);
	if (ret < 0) {
		DPU_ERR_PANEL("%s: failed to ioctl(PANEL_IOC_GET_DISPLAY_MODE)\n", __func__);
		return ret;
	}

	panel_modes = (struct panel_display_modes *)
		v4l2_get_subdev_hostdata(panel->panel_drv_sd);
	if (IS_ERR_OR_NULL(panel_modes)) {
		DPU_ERR_PANEL("%s: failed to get panel_display_modes using v4l2_subdev\n", __func__);
		return -EINVAL;
	}

	if (!panel_modes->modes ||
			panel_modes->num_modes <= panel_modes->native_mode) {
		DPU_ERR_PANEL("%s: invalid panel_modes\n", __func__);
		return -EINVAL;
	}

	/* create unique exynos_display_mode array using panel_display_modes */
	exynos_modes =
		exynos_display_modes_create_from_panel_display_modes(panel, panel_modes);
	if (!exynos_modes) {
		DPU_ERR_PANEL("%s: could not create exynos_display_modes\n", __func__);
		return -ENOMEM;
	}

	/*
	 * initialize display mode information of exynos_panel_info
	 * using exynos_display_modes.
	 */
	exynos_display_modes_update_panel_info(panel, exynos_modes);
	info->panel_modes = panel_modes;
	info->panel_mode_idx = panel_modes->native_mode;
	info->cmd_lp_ref =
		panel_modes->modes[panel_modes->native_mode]->cmd_lp_ref;

	return ret;
}
#endif

static int panel_drv_get_mres(struct exynos_panel_device *panel)
{
	int ret = 0;

	ret = panel_drv_ioctl(panel, PANEL_IOC_GET_MRES, NULL);
	if (ret) {
		DPU_ERR_PANEL("%s: failed to get panel mres", __func__);
		return ret;
	}

	mres = (struct panel_mres *)
		v4l2_get_subdev_hostdata(panel->panel_drv_sd);
	if (IS_ERR_OR_NULL(mres)) {
		DPU_ERR_PANEL("%s: failed to get lcd information\n", __func__);
		return -EINVAL;
	}

	return ret;
}

static int common_panel_vrr_changed(void *data, void *arg)
{
	struct exynos_panel_info *lcd_info;
	struct vrr_config_data *vrr_config;
	struct exynos_panel_device *panel = data;
	struct panel_display_modes *panel_modes;
	struct panel_display_mode *pdm;

#if !defined(CONFIG_UML)
	if (!panel || !arg)
		return -EINVAL;
#else
	if (!panel || !arg)
		return -EINVAL;
#endif

	lcd_info = &panel->lcd_info;
	panel_modes = lcd_info->panel_modes;
	vrr_config = arg;

	if (!panel_modes->modes ||
			panel_modes->num_modes <= panel_modes->native_mode) {
		DPU_ERR_PANEL("%s: invalid panel_modes\n", __func__);
		return -EINVAL;
	}

	pdm = panel_modes->modes[lcd_info->panel_mode_idx];
	if (!pdm)
		return -EINVAL;

	if (pdm->panel_refresh_rate != vrr_config->fps ||
		pdm->panel_refresh_mode != vrr_config->mode)
		DPU_ERR_PANEL("%s: [VRR] decon(%d%s) panel(%d%s) mismatch\n", __func__,
				pdm->panel_refresh_rate, EXYNOS_VRR_MODE_STR(pdm->panel_refresh_mode),
				vrr_config->fps, EXYNOS_VRR_MODE_STR(vrr_config->mode));
	else
		DPU_ERR_PANEL("%s: [VRR] panel(%d%s) updated\n", __func__,
				vrr_config->fps, EXYNOS_VRR_MODE_STR(vrr_config->mode));

	return 0;
}

static int panel_drv_set_vrr_cb(struct exynos_panel_device *panel)
{
	int ret;
	struct disp_cb_info vrr_cb_info = {
		.cb = (disp_cb *)common_panel_vrr_changed,
		.data = panel,
	};

	v4l2_set_subdev_hostdata(panel->panel_drv_sd, &vrr_cb_info);
	ret = panel_drv_ioctl(panel, PANEL_IOC_REG_VRR_CB, NULL);
	if (ret < 0) {
		DPU_ERR_PANEL("%s: failed to set panel error callback\n", __func__);
		return ret;
	}

	return 0;
}

#define DSIM_TX_FLOW_CONTROL
static void print_tx(u8 cmd_id, const u8 *cmd, int size)
{
	char data[256];
	int i, len;
	bool newline = false;

	len = snprintf(data, ARRAY_SIZE(data), "(%02X) ", cmd_id);
	for (i = 0; i < min((int)size, 256); i++) {
		if (newline)
			len += snprintf(data + len, ARRAY_SIZE(data) - len, "     ");
		newline = (!((i + 1) % 16) || (i + 1 == size)) ? true : false;
		len += snprintf(data + len, ARRAY_SIZE(data) - len,
				"%02X%s", cmd[i], newline ? "\n" : " ");
		if (newline) {
			DPU_INFO_PANEL("%s: %s", __func__, data);
			len = 0;
		}
	}
}

static void print_rx(u8 addr, u8 *buf, int size)
{
	char data[256];
	int i, len;
	bool newline = false;

	len = snprintf(data, ARRAY_SIZE(data), "(%02X) ", addr);
	for (i = 0; i < min((int)size, 256); i++) {
		if (newline)
			len += snprintf(data + len, ARRAY_SIZE(data) - len, "	  ");
		newline = (!((i + 1) % 16) || (i + 1 == size)) ? true : false;
		len += snprintf(data + len, ARRAY_SIZE(data) - len,
				"%02X ", buf[i]);
		if (newline) {
			DPU_INFO_PANEL("%s: %s", __func__, data);
			len = 0;
		}
	}
	DPU_INFO_PANEL("%s: %s\n", __func__, data);
}

static void print_dsim_cmd(const struct exynos_dsim_cmd *cmd_set, int size)
{
	int i;

	for (i = 0; i < size; i++)
		print_tx(cmd_set[i].type,
				cmd_set[i].data_buf,
				cmd_set[i].data_len);
}

static int mipi_write(u32 id, u8 cmd_id, const u8 *cmd, u32 offset, int size, u32 option)
{
	int ret, retry = 3;
	unsigned long d0;
	u32 type, d1;
	bool block = (option & DSIM_OPTION_WAIT_TX_DONE);
	struct dsim_device *dsim = get_dsim_drvdata(id);

	if (!cmd) {
		DPU_ERR_PANEL("%s: cmd is null\n", __func__);
		return -EINVAL;
	}

	if (cmd_id == MIPI_DSI_WR_DSC_CMD) {
		type = MIPI_DSI_DSC_PRA;
		d0 = (unsigned long)cmd[0];
		d1 = 0;
	} else if (cmd_id == MIPI_DSI_WR_PPS_CMD) {
		type = MIPI_DSI_DSC_PPS;
		d0 = (unsigned long)cmd;
		d1 = size;
	} else if (cmd_id == MIPI_DSI_WR_GEN_CMD) {
		if (size == 1) {
			type = MIPI_DSI_DCS_SHORT_WRITE;
			d0 = (unsigned long)cmd[0];
			d1 = 0;
		} else {
			type = MIPI_DSI_DCS_LONG_WRITE;
			d0 = (unsigned long)cmd;
			d1 = size;
		}
	} else {
		DPU_INFO_PANEL("%s: invalid cmd_id %d\n", __func__, cmd_id);
		return -EINVAL;
	}

	mutex_lock(&cmd_lock);
	while (--retry >= 0) {
		if (offset > 0) {
			int gpara_len = 1;
			u8 gpara[4] = { 0xB0, 0x00 };

			/* gpara 16bit offset */
			if (option & DSIM_OPTION_2BYTE_GPARA)
				gpara[gpara_len++] = (offset >> 8) & 0xFF;

			gpara[gpara_len++] = offset & 0xFF;

			/* pointing gpara */
			if (option & DSIM_OPTION_POINT_GPARA)
				gpara[gpara_len++] = cmd[0];

			if (panel_cmd_log_enabled(PANEL_CMD_LOG_DSI_TX))
				print_tx(MIPI_DSI_DCS_LONG_WRITE, gpara, gpara_len);
			if (dsim_write_data(dsim, MIPI_DSI_DCS_LONG_WRITE,
						(unsigned long)gpara, gpara_len, false)) {
				DPU_ERR_PANEL("%s: failed to write gpara %d (retry %d)\n", __func__,
						offset, retry);
				continue;
			}
		}
		if (panel_cmd_log_enabled(PANEL_CMD_LOG_DSI_TX))
			print_tx(type, cmd, size);
		if (dsim_write_data(dsim, type, d0, d1, block)) {
			DPU_ERR_PANEL("%s: failed to write cmd %02X size %d(retry %d)\n", __func__,
					cmd[0], size, retry);
			continue;
		}

		break;
	}

	if (retry < 0) {
		DPU_ERR_PANEL("%s: failed: exceed retry count (cmd %02X)\n", __func__,
				cmd[0]);
		ret = -EIO;
		goto error;
	}

	DPU_DEBUG_PANEL("%s: cmd_id %d, addr %02X offset %d size %d\n", __func__,
			cmd_id, cmd[0], offset, size);
	ret = size;

error:
	mutex_unlock(&cmd_lock);
	return ret;
}

#define MAX_DSIM_PH_SIZE (32)
#define MAX_DSIM_PL_SIZE (DSIM_PL_FIFO_THRESHOLD)
#define MAX_CMD_SET_SIZE (1024)
static struct exynos_dsim_cmd cmd_set[MAX_CMD_SET_SIZE];
static int mipi_write_table(u32 id, const struct cmd_set *cmd, int size, u32 option)
{
	int ret, total_size = 0;
	struct dsim_device *dsim = get_dsim_drvdata(id);
	int i, from = 0, sz_pl = 0;
	s64 elapsed_usec;
	struct timespec64 cur_ts, last_ts, delta_ts;

	if (!cmd) {
		DPU_ERR_PANEL("%s: cmd is null\n", __func__);
		return -EINVAL;
	}

	if (size <= 0) {
		DPU_ERR_PANEL("%s: invalid cmd size %d\n", __func__, size);
		return -EINVAL;
	}

	if (size > MAX_CMD_SET_SIZE) {
		DPU_ERR_PANEL("%s: exceeded MAX_CMD_SET_SIZE(%d) %d\n", __func__,
				MAX_CMD_SET_SIZE, size);
		return -EINVAL;
	}

	ktime_get_ts64(&last_ts);
	mutex_lock(&cmd_lock);
	for (i = 0; i < size; i++) {
		if (cmd[i].buf == NULL) {
			DPU_ERR_PANEL("%s: cmd[%d].buf is null\n", __func__, i);
			continue;
		}

		if (cmd[i].cmd_id == MIPI_DSI_WR_DSC_CMD) {
			cmd_set[i].type = MIPI_DSI_DSC_PRA;
			cmd_set[i].data_buf = cmd[i].buf;
			cmd_set[i].data_len = 1;
		} else if (cmd[i].cmd_id == MIPI_DSI_WR_PPS_CMD) {
			cmd_set[i].type = MIPI_DSI_DSC_PPS;
			cmd_set[i].data_buf = cmd[i].buf;
			cmd_set[i].data_len = cmd[i].size;
		} else if (cmd[i].cmd_id == MIPI_DSI_WR_GEN_CMD) {
			if (cmd[i].size == 1) {
				cmd_set[i].type = MIPI_DSI_DCS_SHORT_WRITE;
				cmd_set[i].data_buf = cmd[i].buf;
				cmd_set[i].data_len = 1;
			} else {
				cmd_set[i].type = MIPI_DSI_DCS_LONG_WRITE;
				cmd_set[i].data_buf = cmd[i].buf;
				cmd_set[i].data_len = cmd[i].size;
			}
		} else {
			DPU_INFO_PANEL("%s: invalid cmd_id %d\n", __func__, cmd[i].cmd_id);
			ret = -EINVAL;
			goto error;
		}

#if defined(DSIM_TX_FLOW_CONTROL)
		if ((i - from >= MAX_DSIM_PH_SIZE) ||
			(sz_pl + ALIGN(cmd_set[i].data_len, 4) >= MAX_DSIM_PL_SIZE)) {
			if (dsim_write_cmd_set(dsim, &cmd_set[from], i - from, false)) {
				DPU_ERR_PANEL("%s: failed to write cmd_set\n", __func__);
				ret = -EIO;
				goto error;
			}
			DPU_DEBUG_PANEL("%s: cmd_set:%d pl:%d\n", __func__, i - from, sz_pl);
			if (panel_cmd_log_enabled(PANEL_CMD_LOG_DSI_TX))
				print_dsim_cmd(&cmd_set[from], i - from);
			from = i;
			sz_pl = 0;
		}
#endif
		sz_pl += ALIGN(cmd_set[i].data_len, 4);
		total_size += cmd_set[i].data_len;
	}

	if (dsim_write_cmd_set(dsim, &cmd_set[from], i - from, false)) {
		DPU_ERR_PANEL("%s: failed to write cmd_set\n", __func__);
		ret = -EIO;
		goto error;
	}

	ktime_get_ts64(&cur_ts);
	delta_ts = timespec64_sub(cur_ts, last_ts);
	elapsed_usec = timespec64_to_ns(&delta_ts) / 1000;
	DPU_DEBUG_PANEL("%s: done (cmd_set:%d size:%d elapsed %2lld.%03lld msec)\n", __func__,
			size, total_size,
			elapsed_usec / 1000, elapsed_usec % 1000);
	if (panel_cmd_log_enabled(PANEL_CMD_LOG_DSI_TX))
		print_dsim_cmd(&cmd_set[from], i - from);

	ret = total_size;

error:
	mutex_unlock(&cmd_lock);

	return ret;
}

static int mipi_sr_write(u32 id, u8 cmd_id, const u8 *cmd, u32 offset, int size, u32 option)
{
	int ret = 0;
	struct dsim_device *dsim = get_dsim_drvdata(id);
	s64 elapsed_usec;
	struct timespec64 cur_ts, last_ts, delta_ts;
	int align = 0;

	if (!cmd) {
		DPU_ERR_PANEL("%s: cmd is null\n", __func__);
		return -EINVAL;
	}

	if (option & PKT_OPTION_SR_ALIGN_12)
		align = 12;
	else if (option & PKT_OPTION_SR_ALIGN_16)
		align = 16;

	if (align == 0) {
		/* protect for already released panel: 16byte align */
		DPU_ERR_PANEL("%s: sram packets need to align option, set force to 16\n", __func__);
		align = 16;
	}
	ktime_get_ts64(&last_ts);

	mutex_lock(&cmd_lock);
	ret = dsim_sr_write_data(dsim, cmd, size, align);
	mutex_unlock(&cmd_lock);

	ktime_get_ts64(&cur_ts);
	delta_ts = timespec64_sub(cur_ts, last_ts);
	elapsed_usec = timespec64_to_ns(&delta_ts) / 1000;
	DPU_DEBUG_PANEL("%s: done (size:%d elapsed %2lld.%03lld msec)\n", __func__,
			size, elapsed_usec / 1000, elapsed_usec % 1000);

	return ret;
}

static int mipi_read(u32 id, u8 addr, u32 offset, u8 *buf, int size, u32 option)
{
	int ret, retry = 3;
	struct dsim_device *dsim = get_dsim_drvdata(id);

	if (!buf) {
		DPU_ERR_PANEL("%s: buf is null\n", __func__);
		return -EINVAL;
	}

	mutex_lock(&cmd_lock);
	while (--retry >= 0) {
		if (offset > 0) {
			int gpara_len = 1;
			u8 gpara[4] = { 0xB0, 0x00 };

			/* gpara 16bit offset */
			if (option & DSIM_OPTION_2BYTE_GPARA)
				gpara[gpara_len++] = (offset >> 8) & 0xFF;

			gpara[gpara_len++] = offset & 0xFF;

			/* pointing gpara */
			if (option & DSIM_OPTION_POINT_GPARA)
				gpara[gpara_len++] = addr;

			if (panel_cmd_log_enabled(PANEL_CMD_LOG_DSI_TX))
				print_tx(MIPI_DSI_DCS_LONG_WRITE, gpara, gpara_len);
			if (dsim_write_data(dsim, MIPI_DSI_DCS_LONG_WRITE,
						(unsigned long)gpara, gpara_len, false)) {
				DPU_ERR_PANEL("%s: failed to write gpara %d (retry %d)\n", __func__,
						offset, retry);
				continue;
			}
		}

		if (panel_cmd_log_enabled(PANEL_CMD_LOG_DSI_TX)) {
			u8 read_cmd1[] = { size };
			u8 read_cmd2[] = { addr };

			print_tx(MIPI_DSI_SET_MAXIMUM_RETURN_PACKET_SIZE,
					read_cmd1, ARRAY_SIZE(read_cmd1));
			print_tx(MIPI_DSI_DCS_READ, read_cmd2, ARRAY_SIZE(read_cmd2));
		}
		ret = dsim_read_data(dsim, MIPI_DSI_DCS_READ,
				(u32)addr, size, buf);
		if (ret != size) {
			DPU_ERR_PANEL("%s: failed to read addr %02X ofs %d size %d (ret %d, retry %d)\n", __func__,
					addr, offset, size, ret, retry);
			continue;
		}
		if (panel_cmd_log_enabled(PANEL_CMD_LOG_DSI_RX))
			print_rx(addr, buf, size);
		break;
	}

	if (retry < 0) {
		DPU_ERR_PANEL("%s: failed: exceed retry count (addr %02X)\n", __func__, addr);
		ret = -EIO;
		goto error;
	}

	DPU_DEBUG_PANEL("%s: addr %02X ofs %d size %d, buf %02X done\n", __func__,
			addr, offset, size, buf[0]);

	ret = size;

error:
	mutex_unlock(&cmd_lock);
	return ret;
}

enum dsim_state get_dsim_state(u32 id)
{
	struct dsim_device *dsim = get_dsim_drvdata(id);

	if (dsim == NULL) {
		DPU_ERR_PANEL("%s: dsim is NULL\n", __func__);
		return -ENODEV;
	}
	return dsim->state;
}

static struct exynos_panel_info *get_lcd_info(u32 id)
{
	return &get_panel_drvdata(id)->lcd_info;
}

static int wait_for_vsync(u32 id, u32 timeout)
{
	struct decon_device *decon = get_decon_drvdata(0);
	int ret;

	if (!decon)
		return -EINVAL;

	decon_hiber_block_exit(decon);
	ret = decon_wait_for_vsync(decon, timeout);
	decon_hiber_unblock(decon);

	return ret;
}

static int set_bypass(u32 id, bool on)
{
	struct decon_device *decon = get_decon_drvdata(0);

	if (!decon)
		return -EINVAL;

	if (on)
		decon_bypass_on(decon);
	else
		decon_bypass_off(decon);

	return 0;
}

static int get_bypass(u32 id)
{
	struct decon_device *decon = get_decon_drvdata(0);

	if (!decon)
		return -EINVAL;

	return atomic_read(&decon->bypass);
}

static int wake_lock(u32 id, unsigned long timeout)
{
	struct decon_device *decon = get_decon_drvdata(0);

	if (!decon)
		return -EINVAL;

	return decon_wake_lock(decon, timeout);
}

static int wake_unlock(u32 id)
{
	struct decon_device *decon = get_decon_drvdata(0);

	if (!decon)
		return -EINVAL;

	decon_wake_unlock(decon);
	return 0;
}

static int flush_image(u32 id)
{
	struct decon_device *decon = get_decon_drvdata(0);

	if (!decon)
		return -EINVAL;

	mcd_decon_flush_image(decon);
	return 0;
}

static int panel_drv_put_ops(struct exynos_panel_device *panel)
{
	int ret = 0;
	struct mipi_drv_ops mipi_ops;

	mipi_ops.read = mipi_read;
	mipi_ops.write = mipi_write;
	mipi_ops.write_table = mipi_write_table;
	mipi_ops.sr_write = mipi_sr_write;
	mipi_ops.get_state = get_dsim_state;
	mipi_ops.parse_dt = parse_lcd_info;
	mipi_ops.get_lcd_info = get_lcd_info;
	mipi_ops.wait_for_vsync = wait_for_vsync;
	/* TODO: seperate mipi_drv ops and decon_drv_ops */
	mipi_ops.set_bypass = set_bypass;
	mipi_ops.get_bypass = get_bypass;
	mipi_ops.wake_lock = wake_lock;
	mipi_ops.wake_unlock = wake_unlock;
	mipi_ops.flush_image = flush_image;

	v4l2_set_subdev_hostdata(panel->panel_drv_sd, &mipi_ops);

	ret = panel_drv_ioctl(panel, PANEL_IOC_DSIM_PUT_MIPI_OPS, NULL);
	if (ret) {
		DPU_ERR_PANEL("%s: failed to set mipi ops\n", __func__);
		return ret;
	}

	return ret;
}

static int panel_drv_init(struct exynos_panel_device *panel)
{
	int ret;

	ret = panel_drv_put_ops(panel);
	if (ret) {
		DPU_ERR_PANEL("%s: failed to put ops\n", __func__);
		goto do_exit;
	}

	ret = panel_drv_probe(panel);
	if (ret) {
		DPU_ERR_PANEL("%s: failed to probe panel", __func__);
		goto do_exit;
	}

	ret = panel_drv_get_state(panel);
	if (ret) {
		DPU_ERR_PANEL("%s: failed to get panel state\n", __func__);
		goto do_exit;
	}

#if defined(CONFIG_PANEL_DISPLAY_MODE)
	ret = panel_drv_get_panel_display_mode(panel);
	if (ret < 0) {
		DPU_ERR_PANEL("%s: failed to get panel_display_modes\n", __func__);
		goto do_exit;
	}
#endif

	ret = panel_drv_get_mres(panel);
	if (ret) {
		DPU_ERR_PANEL("%s: failed to get panel mres\n", __func__);
		goto do_exit;
	}

	ret = panel_drv_set_vrr_cb(panel);
	if (ret) {
		DPU_ERR_PANEL("%s: failed to set vrr callback\n", __func__);
		goto do_exit;
	}

	return ret;

do_exit:
	return ret;
}

static int common_panel_connected(struct exynos_panel_device *panel)
{
	int ret;

	ret = panel_drv_get_state(panel);
	if (ret) {
		DPU_ERR_PANEL("%s: failed to get panel state\n", __func__);
		return ret;
	}

	ret = !(panel_state->connect_panel == PANEL_DISCONNECT);
	DPU_INFO_PANEL("%s: panel %s\n", __func__,
			ret ? "connected" : "disconnected");

	return ret;
}

static int common_panel_init(struct exynos_panel_device *panel)
{
	int ret;

	ret = panel_drv_init(panel);
	if (ret) {
		DPU_ERR_PANEL("%s: failed to init common panel\n", __func__);
		return ret;
	}

	return 0;
}

static int common_panel_probe(struct exynos_panel_device *panel)
{
	int ret;

	ret = panel_drv_ioctl(panel, PANEL_IOC_PANEL_PROBE, &panel->id);
	if (ret) {
		DPU_ERR_PANEL("%s: failed to probe panel\n", __func__);
		return ret;
	}

	return 0;
}

static int common_panel_displayon(struct exynos_panel_device *panel)
{
	int ret;
	int disp_on = 1;

	ret = panel_drv_ioctl(panel, PANEL_IOC_DISP_ON, (void *)&disp_on);
	if (ret) {
		DPU_ERR_PANEL("%s: failed to display on\n", __func__);
		return ret;
	}

	return 0;
}

static int common_panel_suspend(struct exynos_panel_device *panel)
{
	int ret;

	ret = panel_drv_ioctl(panel, PANEL_IOC_SLEEP_IN, NULL);
	if (ret) {
		DPU_ERR_PANEL("%s: failed to sleep in\n", __func__);
		return ret;
	}

	return 0;
}

static int common_panel_resume(struct exynos_panel_device *panel)
{
	return 0;
}

static int common_panel_dump(struct exynos_panel_device *panel)
{
	int ret;

	ret = panel_drv_ioctl(panel, PANEL_IOC_PANEL_DUMP, NULL);
	if (ret) {
		DPU_ERR_PANEL("%s: failed to dump panel\n", __func__);
		return ret;
	}

	return 0;
}

static int common_panel_setarea(struct exynos_panel_device *panel, u32 l, u32 r, u32 t, u32 b)
{
	int ret = 0;
	char column[5];
	char page[5];
	int retry;
	struct dsim_device *dsim = get_dsim_drvdata(panel->id);

	column[0] = MIPI_DCS_SET_COLUMN_ADDRESS;
	column[1] = (l >> 8) & 0xff;
	column[2] = l & 0xff;
	column[3] = (r >> 8) & 0xff;
	column[4] = r & 0xff;

	page[0] = MIPI_DCS_SET_PAGE_ADDRESS;
	page[1] = (t >> 8) & 0xff;
	page[2] = t & 0xff;
	page[3] = (b >> 8) & 0xff;
	page[4] = b & 0xff;

	mutex_lock(&cmd_lock);
	retry = 2;

	if (panel_cmd_log_enabled(PANEL_CMD_LOG_DSI_TX))
		print_tx(MIPI_DSI_DCS_LONG_WRITE, column, ARRAY_SIZE(column));
	while (dsim_write_data(dsim, MIPI_DSI_DCS_LONG_WRITE,
				(unsigned long)column, ARRAY_SIZE(column), false) != 0) {
		DPU_ERR_PANEL("%s: failed to write COLUMN_ADDRESS\n", __func__);
		if (--retry <= 0) {
			DPU_ERR_PANEL("%s: COLUMN_ADDRESS is failed: exceed retry count\n", __func__);
			ret = -EINVAL;
			goto error;
		}
	}

	retry = 2;
	if (panel_cmd_log_enabled(PANEL_CMD_LOG_DSI_TX))
		print_tx(MIPI_DSI_DCS_LONG_WRITE, page, ARRAY_SIZE(page));
	while (dsim_write_data(dsim, MIPI_DSI_DCS_LONG_WRITE,
				(unsigned long)page, ARRAY_SIZE(page), true) != 0) {
		DPU_ERR_PANEL("%s: failed to write PAGE_ADDRESS\n", __func__);
		if (--retry <= 0) {
			DPU_ERR_PANEL("%s: PAGE_ADDRESS is failed: exceed retry count\n", __func__);
			ret = -EINVAL;
			goto error;
		}
	}

	DPU_DEBUG_PANEL("%s: RECT [l:%d r:%d t:%d b:%d w:%d h:%d]\n", __func__,
			l, r, t, b, r - l + 1, b - t + 1);

error:
	mutex_unlock(&cmd_lock);
	return ret;
}

static int common_panel_power(struct exynos_panel_device *panel, int on)
{
	int ret;

	ret = panel_drv_ioctl(panel, PANEL_IOC_SET_POWER, (void *)&on);
	if (ret) {
		DPU_ERR_PANEL("%s: failed to panel %s\n", __func__, on ? "on" : "off");
		return ret;
	}
	DPU_DEBUG_PANEL("%s: power %s\n", __func__, on ? "on" : "off");

	return 0;
}

static int common_panel_poweron(struct exynos_panel_device *panel)
{
	return common_panel_power(panel, 1);
}

static int common_panel_poweroff(struct exynos_panel_device *panel)
{
	return common_panel_power(panel, 0);
}

static int common_panel_sleepin(struct exynos_panel_device *panel)
{
	int ret;

	ret = panel_drv_ioctl(panel, PANEL_IOC_SLEEP_IN, NULL);
	if (ret) {
		DPU_ERR_PANEL("%s: failed to sleep in\n", __func__);
		return ret;
	}

	return 0;
}

static int common_panel_sleepout(struct exynos_panel_device *panel)
{
	int ret;

	ret = panel_drv_ioctl(panel, PANEL_IOC_SLEEP_OUT, NULL);
	if (ret) {
		DPU_ERR_PANEL("%s: failed to sleep out\n", __func__);
		return ret;
	}

	return 0;
}

static int common_panel_notify(struct exynos_panel_device *panel, void *data)
{
	int ret;

	ret = panel_drv_notify(panel->panel_drv_sd,
			V4L2_DEVICE_NOTIFY_EVENT, data);
	if (ret) {
		DPU_ERR_PANEL("%s: failed to notify\n", __func__);
		return ret;
	}

	return 0;
}

static int common_panel_reset_panel(struct exynos_panel_device *panel)
{
	int ret;

	ret = panel_drv_ioctl(panel, PANEL_IOC_PANEL_RESET, NULL);
	if (ret) {
		DPU_ERR_PANEL("%s: failed to reset panel\n", __func__);
		return ret;
	}

	return 0;
}

static int common_panel_read_state(struct exynos_panel_device *panel)
{
	return true;
}

#ifdef CONFIG_EXYNOS_DOZE
static int common_panel_doze(struct exynos_panel_device *panel)
{
#ifdef CONFIG_EXYNOS_DOZE
	int ret;

	ret = panel_drv_ioctl(panel, PANEL_IOC_DOZE, NULL);
	if (ret) {
		DPU_ERR_PANEL("%s: failed to sleep out\n", __func__);
		return ret;
	}
#endif

	return 0;
}

static int common_panel_doze_suspend(struct exynos_panel_device *panel)
{
#ifdef CONFIG_EXYNOS_DOZE
	int ret;

	ret = panel_drv_ioctl(panel, PANEL_IOC_DOZE_SUSPEND, NULL);
	if (ret) {
		DPU_ERR_PANEL("%s: failed to sleep out\n", __func__);
		return ret;
	}
#endif

	return 0;
}
#endif

#if defined(CONFIG_PANEL_DISPLAY_MODE)
static int common_panel_get_display_mode(struct exynos_panel_device *panel, void *data)
{
	int ret;

	ret = panel_drv_get_panel_display_mode(panel);
	if (ret < 0) {
		DPU_ERR_PANEL("%s: failed to get panel_display_modes\n", __func__);
		return ret;
	}

	/*
	 * TODO : implement get current panel_display_mode
	 */
	return 0;
}

static int common_panel_set_display_mode(struct exynos_panel_device *panel, void *data)
{
	struct panel_display_modes *panel_modes;
	struct exynos_panel_info *lcd_info;
	int ret, panel_mode_idx;

	if (!data)
		return -EINVAL;

	lcd_info = &panel->lcd_info;
	panel_modes = lcd_info->panel_modes;
	if (!panel_modes) {
		DPU_ERR_PANEL("%s: panel_modes not prepared\n", __func__);
		return -EPERM;
	}

	panel_mode_idx = *(int *)data;
	if (panel_mode_idx < 0 ||
		panel_mode_idx >= panel_modes->num_modes) {
		DPU_ERR_PANEL("%s: invalid panel_mode_idx(%d)\n", __func__, panel_mode_idx);
		return -EINVAL;
	}

	ret = panel_drv_ioctl(panel,
			PANEL_IOC_SET_DISPLAY_MODE,
			panel_modes->modes[panel_mode_idx]);
	if (ret < 0) {
		DPU_ERR_PANEL("%s: failed to set panel_display_mode(%d)\n", __func__,
				panel_mode_idx);
		return ret;
	}

	return 0;
}
#endif

struct exynos_panel_ops common_panel_ops = {
	.init		= common_panel_init,
	.probe		= common_panel_probe,
	.suspend	= common_panel_suspend,
	.resume		= common_panel_resume,
	.dump		= common_panel_dump,
	.connected	= common_panel_connected,
	.setarea	= common_panel_setarea,
	.poweron	= common_panel_poweron,
	.poweroff	= common_panel_poweroff,
	.sleepin	= common_panel_sleepin,
	.sleepout	= common_panel_sleepout,
	.displayon	= common_panel_displayon,
	.notify		= common_panel_notify,
	.read_state	= common_panel_read_state,
	.set_error_cb	= common_panel_set_error_cb,
	.reset_panel = common_panel_reset_panel,
#ifdef CONFIG_EXYNOS_DOZE
	.doze		= common_panel_doze,
	.doze_suspend	= common_panel_doze_suspend,
#endif
	.mres = NULL,
	.set_vrefresh = NULL,
#if defined(CONFIG_PANEL_DISPLAY_MODE)
	.get_display_mode = common_panel_get_display_mode,
	.set_display_mode = common_panel_set_display_mode,
#endif
};
