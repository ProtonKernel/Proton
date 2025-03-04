/*
 * Copyright (c) 2018 Samsung Electronics Co., Ltd.
 *		http://www.samsung.com
 *
 * Samsung S6E3FA0 Panel driver.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#include <video/mipi_display.h>
#include "exynos_panel_drv.h"
#include "../dsim.h"

int s6e3fa0_suspend(struct exynos_panel_device *panel)
{
	struct dsim_device *dsim = get_dsim_drvdata(panel->id);

	mutex_lock(&panel->ops_lock);
	dsim_write_data_seq_delay(dsim, 20, 0x28, 0x00, 0x00);
	mutex_unlock(&panel->ops_lock);

	return 0;
}

static int s6e3fa0_cabc_mode_unlocked(u32 panel_id, int mode)
{
	int ret = 0;
	int count;
	unsigned char buf[] = {0x0, 0x0};
	unsigned char SEQ_CABC_CMD[] = {0x55, 0x00, 0x00};
	unsigned char cmd = MIPI_DCS_WRITE_POWER_SAVE; /* 0x55 */
	struct dsim_device *dsim = get_dsim_drvdata(panel_id);

	DPU_DEBUG_PANEL("%s: CABC mode[%d] write/read\n", __func__, mode);

	switch (mode) {
	/* read */
	case POWER_MODE_READ:
		cmd = MIPI_DCS_GET_POWER_SAVE; /* 0x56 */
		ret = dsim_read_data(dsim, MIPI_DSI_DCS_READ, cmd, 0x1, buf);
		if (ret < 0) {
			DPU_ERR_PANEL("CABC REG(0x%02X) read failure!\n", cmd);
			count = 0;
		} else {
			DPU_INFO_PANEL("CABC REG(0x%02X) read success: 0x%02x\n",
				cmd, *(unsigned int *)buf & 0xFF);
			count = 1;
		}
		return count;

	/* write */
	case POWER_SAVE_OFF:
		SEQ_CABC_CMD[1] = CABC_OFF;
		break;
	case POWER_SAVE_LOW:
		SEQ_CABC_CMD[1] = CABC_USER_IMAGE;
		break;
	case POWER_SAVE_MEDIUM:
		SEQ_CABC_CMD[1] = CABC_STILL_PICTURE;
		break;
	case POWER_SAVE_HIGH:
		SEQ_CABC_CMD[1] = CABC_MOVING_IMAGE;
		break;
	default:
		DPU_ERR_PANEL("Unavailable CABC mode(%d)!\n", mode);
		return -EINVAL;
	}

	dsim_write_data_table(dsim, SEQ_CABC_CMD);

	return ret;
}

static int s6e3fa0_cabc_mode(struct exynos_panel_device *panel, int mode)
{
	int ret = 0;

	if (!panel->cabc_enabled) {
		ret = -EPERM;
		return ret;
	}

	mutex_lock(&panel->ops_lock);
	s6e3fa0_cabc_mode_unlocked(panel->id, mode);
	mutex_unlock(&panel->ops_lock);
	return ret;
}

int s6e3fa0_displayon(struct exynos_panel_device *panel)
{
	struct exynos_panel_info *lcd = &panel->lcd_info;
	struct dsim_device *dsim = get_dsim_drvdata(panel->id);

	mutex_lock(&panel->ops_lock);
	dsim_write_data_seq_delay(dsim, 12, 0xF0, 0x5A, 0x5A);
	dsim_write_data_seq_delay(dsim, 12, 0xF1, 0x5A, 0x5A);
	dsim_write_data_seq_delay(dsim, 12, 0xFC, 0x5A, 0x5A);
	dsim_write_data_seq_delay(dsim, 12, 0xED, 0x01, 0x00);

	if (lcd->mode == DECON_MIPI_COMMAND_MODE) {
		dsim_write_data_seq_delay(dsim, 12, 0xFD, 0x16, 0x80);
		dsim_write_data_seq_delay(dsim, 12, 0xF6, 0x08);
	} else {
		dsim_write_data_seq_delay(dsim, 120, 0xE7, 0xED, 0xC7, 0x23,
				0x57, 0xA5);
	}

	dsim_write_data_seq_delay(dsim, 20, 0x11); /* sleep out */

	if (lcd->mode == DECON_VIDEO_MODE)
		dsim_write_data_seq_delay(dsim, 12, 0xF2, 0x02);

	dsim_write_data_seq_delay(dsim, 12, 0xEB, 0x01, 0x00);
	dsim_write_data_seq_delay(dsim, 12, 0xC0, 0x63, 0x02, 0x03, 0x32, 0xFF,
			0x44, 0x44, 0xC0, 0x00, 0x40);

	if (panel->cabc_enabled)
		s6e3fa0_cabc_mode_unlocked(panel->id, panel->power_mode);

	/* enable brightness control */
	dsim_write_data_seq_delay(dsim, 12, 0x53, 0x20);
	dsim_write_data_seq_delay(dsim, 12, 0x51, 0x7f);

	if (lcd->mode == DECON_MIPI_COMMAND_MODE)
		dsim_write_data_seq_delay(dsim, 12, 0x35); /* TE on */

	if (lcd->mode == DECON_VIDEO_MODE)
		mdelay(120);

	dsim_write_data_seq(dsim, false, 0x29); /* display on */
	mutex_unlock(&panel->ops_lock);

	return 0;
}

int s6e3fa0_mres(struct exynos_panel_device *panel, u32 mode_idx)
{
	return 0;
}

int s6e3fa0_doze(struct exynos_panel_device *panel)
{
	return 0;
}

int s6e3fa0_doze_suspend(struct exynos_panel_device *panel)
{
	return 0;
}

int s6e3fa0_dump(struct exynos_panel_device *panel)
{
	return 0;
}

int s6e3fa0_read_state(struct exynos_panel_device *panel)
{
	return 0;
}

static int s6e3fa0_set_light(struct exynos_panel_device *panel, u32 br_val)
{
	u8 data = 0;
	struct dsim_device *dsim = get_dsim_drvdata(panel->id);

	DPU_DEBUG_PANEL("%s +\n", __func__);

	mutex_lock(&panel->ops_lock);

	/* WRDISBV(8bit): 1st DBV[7:0] */
	data = br_val & 0xFF;
	dsim_write_data_seq(dsim, 12, 0x51, data);

	mutex_unlock(&panel->ops_lock);

	DPU_DEBUG_PANEL("%s -\n", __func__);
	return 0;
}

struct exynos_panel_ops panel_s6e3fa0_ops = {
	.id		= {0x244040, 0xffffff, 0xffffff, 0xffffff},
	.suspend	= s6e3fa0_suspend,
	.displayon	= s6e3fa0_displayon,
	.mres		= s6e3fa0_mres,
	.doze		= s6e3fa0_doze,
	.doze_suspend	= s6e3fa0_doze_suspend,
	.dump		= s6e3fa0_dump,
	.read_state	= s6e3fa0_read_state,
	.set_cabc_mode	= s6e3fa0_cabc_mode,
	.set_light	= s6e3fa0_set_light,
};
