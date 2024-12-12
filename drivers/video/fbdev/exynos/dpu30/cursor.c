/*
 * Copyright (c) 2017 Samsung Electronics Co., Ltd.
 *		http://www.samsung.com
 *
 * Cursor Async file for Samsung EXYNOS DPU driver
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
*/

#include "decon.h"
#include "dpp.h"

void decon_set_cursor_reset(struct decon_device *decon,
		struct decon_reg_data *regs)
{
	if (!decon->cursor.enabled)
		return;

	mutex_lock(&decon->cursor.unmask_lock);
	decon->cursor.unmask = false;
	memcpy(&decon->cursor.regs, regs, sizeof(struct decon_reg_data));
	mutex_unlock(&decon->cursor.unmask_lock);
}

void decon_set_cursor_unmask(struct decon_device *decon, bool unmask)
{
	if (!decon->cursor.enabled)
		return;

	mutex_lock(&decon->cursor.unmask_lock);
	decon->cursor.unmask = unmask;
	mutex_unlock(&decon->cursor.unmask_lock);
}

static void decon_set_cursor_pos(struct decon_device *decon, int x, int y)
{
	if (!decon->cursor.enabled)
		return;

	if (x < 0)
		x = 0;
	if (y < 0)
		y = 0;

	spin_lock(&decon->cursor.pos_lock);
	decon->cursor.xpos = x;
	decon->cursor.ypos = y;
	spin_unlock(&decon->cursor.pos_lock);
}

static int decon_set_cursor_dpp_config(struct decon_device *decon,
		struct decon_reg_data *regs, unsigned long aclk_khz)
{
	int i, ret = 0, err_cnt = 0;
	struct v4l2_subdev *sd;
	struct decon_win *win;
	struct dpp_config dpp_config;

	if (!decon->cursor.enabled)
		return 0;

	if (!regs->is_cursor_win[regs->cursor_win])
		return -1;

	i = regs->cursor_win;
	win = decon->win[i];
	if (!test_bit(win->dpp_id, &decon->cur_using_dpp))
		return -2;

	sd = decon->dpp_sd[win->dpp_id];
	memcpy(&dpp_config.config, &regs->dpp_config[i],
			sizeof(struct decon_win_config));
	dpp_config.rcv_num = aclk_khz;
	ret = v4l2_subdev_call(sd, core, ioctl, DPP_CURSOR_WIN_CONFIG, &dpp_config);
	if (ret) {
		decon_err("failed to config (WIN%d : DPP%d)\n",
						i, win->dpp_id);
		regs->win_regs[i].wincon &= (~WIN_EN_F(i));
		decon_reg_set_win_enable(decon->id, i, false);
		if (regs->num_of_window != 0)
			regs->num_of_window--;

		clear_bit(win->dpp_id, &decon->cur_using_dpp);
		set_bit(win->dpp_id, &decon->dpp_err_stat);
		err_cnt++;
	}
	return err_cnt;
}

void dpu_cursor_win_update_config(struct decon_device *decon,
		struct decon_reg_data *regs)
{
	unsigned short cur = regs->cursor_win;
	struct decon_frame *dst;
	u32 x, y;

	if (!decon->cursor.enabled)
		return;

	if (!(regs->win_regs[cur].wincon & WIN_EN_F(cur))) {
		decon_err("%s, window[%d] is not enabled\n", __func__, cur);
		return;
	}

	if (!regs->is_cursor_win[cur]) {
		decon_err("%s, window[%d] is not cursor layer\n",
				__func__, cur);
		return;
	}

	spin_lock(&decon->cursor.pos_lock);
	x = decon->cursor.xpos;
	y = decon->cursor.ypos;
	spin_unlock(&decon->cursor.pos_lock);

	dst = &regs->dpp_config[cur].dst;

	if ((x + dst->w) > decon->lcd_info->xres) {
		decon_dbg("cursor out of range: x(%d), w(%d), xres(%d)",
				x, dst->w, decon->lcd_info->xres);
		return;
	}
	if ((y + dst->h) > decon->lcd_info->yres) {
		decon_dbg("cursor out of range: y(%d), y(%d), yres(%d)",
				y, dst->h, decon->lcd_info->yres);
		return;
	}

	regs->win_regs[cur].start_pos = win_start_pos(x, y);
	regs->win_regs[cur].end_pos = win_end_pos(x, y, dst->w, dst->h);

	regs->dpp_config[cur].dst.x = x;
	regs->dpp_config[cur].dst.y = y;
}

int decon_cursor_check_limitation(struct decon_device *decon,
		struct decon_reg_data *regs)
{
	unsigned short cur = regs->cursor_win;

	if ((regs->dpp_config[cur].dst.w + decon->cursor.xpos + 16) >
			(decon->lcd_info->xres + regs->dpp_config[cur].dst.w)) {
		return -EINVAL;
	}

	if ((regs->dpp_config[cur].dst.h + decon->cursor.ypos + 8) >
			(decon->lcd_info->yres + regs->dpp_config[cur].dst.h)) {
		return -EINVAL;
	}

	return 0;
}

int decon_reg_set_cursor(struct decon_device *decon,
		struct decon_reg_data *regs, struct decon_mode_info *psr)
{
	unsigned long regset_flags;
	u32 cur_linecnt;
	u32 regset_margin = 0;
	u32 cursor_regset_margin = 0;
	int i;
	int ret = 0;
	int err_cnt = 0;
	unsigned long aclk_khz;

	aclk_khz = v4l2_subdev_call(decon->out_sd[0], core, ioctl,
			EXYNOS_DPU_GET_ACLK, NULL) / 1000U;

	regset_margin = decon->cursor.regset_margin;
	cursor_regset_margin = (decon->lcd_info->yres * (100 - regset_margin)) / 100;

	local_irq_save(regset_flags);
	preempt_disable();

	cur_linecnt = decon_processed_linecnt(decon);
	if (cur_linecnt > cursor_regset_margin) {
		decon_dbg("%s:%d cur_linecnt(%d) cursor margin not enough\n",
				__func__, __LINE__, cur_linecnt);
		goto regset_end;
	} else if (cur_linecnt == 0) {
		if (psr->psr_mode == DECON_VIDEO_MODE) {
			udelay(1);
		} else {
			decon_dbg("%s:%d cur_linecnt(%d) cursor margin not enough\n",
				__func__, __LINE__, cur_linecnt);
			goto regset_end;
		}
	}
	decon_dbg("%s:%d cur_linecnt(%d) cursor margin enough\n",
			__func__, __LINE__, cur_linecnt);
	decon_reg_update_req_window_mask(decon->id, regs->cursor_win);

	err_cnt = decon_set_cursor_dpp_config(decon, regs, aclk_khz);
	if (err_cnt) {
		decon_err("decon%d: cursor win(%d) during dpp_config(err_cnt:%d)\n",
			decon->id, regs->cursor_win, err_cnt);
		if (regs->num_of_window == 0) {
			if (psr->psr_mode == DECON_VIDEO_MODE) {
				decon_err("decon%d: num_of_window = 0\n",
						decon->id);
				for (i = 0; i < decon->dt.max_win; i++)
					decon_reg_set_win_enable(
						decon->id, i, false);
				decon_reg_all_win_shadow_update_req(decon->id);
				decon_reg_update_req_global(decon->id);
			} else if (psr->psr_mode == DECON_MIPI_COMMAND_MODE) {
				decon_reg_set_trigger(
					decon->id, psr, DECON_TRIG_DISABLE);
			}
		}
		ret = -EINVAL;
		goto regset_end;
	}

	/* set decon registers for each window */
	decon_reg_set_window_control(decon->id, regs->cursor_win,
				&regs->win_regs[regs->cursor_win],
				regs->win_regs[regs->cursor_win].winmap_state);

	decon_reg_all_win_shadow_update_req(decon->id);

	decon_reg_start(decon->id, psr);

regset_end:
	local_irq_restore(regset_flags);
	preempt_enable();

	return ret;
}

int decon_set_cursor_win_config(struct decon_device *decon, int x, int y)
{
	struct decon_reg_data *regs;
	struct decon_mode_info psr;
	int ret = 0;

	DPU_EVENT_START();

	if (!decon->cursor.enabled)
		return 0;

	decon_set_cursor_pos(decon, x, y);

	if (decon->dt.psr_mode == DECON_MIPI_COMMAND_MODE)
		return 0;

	mutex_lock(&decon->cursor.unmask_lock);

	decon_to_psr_info(decon, &psr);

	if (IS_DECON_OFF_STATE(decon) ||
		decon->state == DECON_STATE_TUI) {
		decon_info("decon%d: cursor win is not active :%d\n",
			decon->id, decon->state);
		ret = -EINVAL;
		goto unmask_end;
	}

	regs = &decon->cursor.regs;
	if (!regs) {
		decon_err("decon%d: cursor regs is null\n",
			decon->id);
		ret = -EINVAL;
		goto unmask_end;
	}

	if (!regs->is_cursor_win[regs->cursor_win]) {
		decon_dbg("decon%d: cursor win(%d) disable\n",
			decon->id, regs->cursor_win);
		ret = -EINVAL;
		goto unmask_end;
	}

	if (!decon->cursor.unmask) {
		decon_dbg("decon%d: cursor win(%d) update not ready\n",
			decon->id, regs->cursor_win);
		ret = -EINVAL;
		goto unmask_end;
	}

	if (decon_cursor_check_limitation(decon, regs)) {
		decon_err("decon cursor check limitation violated\n");
		ret = -EINVAL;
		goto unmask_end;
	}

	dpu_cursor_win_update_config(decon, regs);

	ret = decon_reg_set_cursor(decon, regs, &psr);
	decon_dbg("%s:%d x(%d), y(%d)\n", __func__, __LINE__, x, y);

unmask_end:
	if (psr.psr_mode == DECON_MIPI_COMMAND_MODE)
		decon->cursor.unmask = false;

	mutex_unlock(&decon->cursor.unmask_lock);

	DPU_EVENT_LOG(DPU_EVT_CURSOR_POS, &decon->sd, start);

	return ret;
}

void dpu_init_cursor_mode(struct decon_device *decon)
{
	decon->cursor.enabled = false;

	if (!IS_ENABLED(CONFIG_EXYNOS_CURSOR)) {
		decon_info("display doesn't support cursor async mode\n");
		return;
	}

	decon->cursor.enabled = true;
	decon_info("display supports cursor async mode\n");
}
