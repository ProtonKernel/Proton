/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) Samsung Electronics Co., Ltd.
 * Gwanghui Lee <gwanghui.lee@samsung.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#include "mcd_helper.h"
#include "panel_drv.h"

#ifdef CONFIG_DYNAMIC_MIPI
struct dynamic_mipi_info *mcd_dm_get_info(struct v4l2_subdev *panel_sd)
{
	int ret;
	struct dynamic_mipi_info *dm = NULL;

	ret = v4l2_subdev_call(panel_sd, core, ioctl, PANEL_IOC_DM_GET_INFO, NULL);
	if (ret < 0) {
		decon_err("ERR:%s:failed v4l2_subdev_call PANEL_IOC_DM_GET_INFO\n", __func__);
		goto exit_get_dm;
	}

	dm = (struct dynamic_mipi_info *)v4l2_get_subdev_hostdata(panel_sd);
	if (dm == NULL) {
		decon_err("ERR:%s:failed to get dm info\n", __func__);
		goto exit_get_dm;
	}

exit_get_dm:
	return dm;
}
#endif

#if IS_ENABLED(CONFIG_MCD_PANEL)
int mcd_decon_set_win(struct decon_device *decon, struct fb_info *info)
{
	int ret = 0;

	struct fb_var_screeninfo *var = &info->var;
	struct decon_win *win = info->par;
	struct decon_window_regs win_regs;
	int win_no = win->idx;

	memset(&win_regs, 0, sizeof(struct decon_window_regs));

	win_regs.wincon |= wincon(win_no);
	win_regs.start_pos = win_start_pos(0, 0);
	win_regs.end_pos = win_end_pos(0, 0, var->xres, var->yres);
	win_regs.pixel_count = (var->xres * var->yres);
	win_regs.whole_w = var->xoffset + var->xres;
	win_regs.whole_h = var->yoffset + var->yres;
	win_regs.offset_x = var->xoffset;
	win_regs.offset_y = var->yoffset;
	win_regs.ch = decon->dt.dft_ch;

	decon_reg_set_window_control(decon->id, win_no, &win_regs, false);
	decon_reg_all_win_shadow_update_req(decon->id);

	return ret;
}
#endif /* CONFIG_MCD_PANEL */

#if IS_ENABLED(CONFIG_RTC_LIB)
#define MAX_RTC_STR_BUF	16
int get_str_cur_rtc(char *buf, size_t size)
{
	int str_size;
	struct rtc_time tm;
	struct timespec ts_rtc;
	unsigned long local_time;

	if ((!buf) || (size < MAX_RTC_STR_BUF)) {
		decon_err("%s: invalid parameter size: %d\n", __func__, size);
		return -EINVAL;
	}

	getnstimeofday(&ts_rtc);
	local_time = (ts_rtc.tv_sec - (sys_tz.tz_minuteswest * 60));
	rtc_time_to_tm(local_time, &tm);
	str_size = sprintf(buf, "%02d-%02d %02d:%02d:%02d", tm.tm_mon + 1, tm.tm_mday, tm.tm_hour, tm.tm_min, tm.tm_sec);

	return 0;
}
#endif
