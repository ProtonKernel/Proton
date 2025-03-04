/*
 * linux/drivers/video/fbdev/exynos/panel/s6e3had/s6e3had_aod.c
 *
 * Source file for AOD Driver
 *
 * Copyright (c) 2016 Samsung Electronics
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#include "../panel_drv.h"
#include "../panel_debug.h"
#include "s6e3had_aod.h"

#ifdef PANEL_PR_TAG
#undef PANEL_PR_TAG
#define PANEL_PR_TAG	"self"
#endif

void s6e3had_copy_self_mask_ctrl(struct maptbl *tbl, u8 *dst)
{
	panel_info("was called\n");
	panel_info("%x %x %x\n", dst[0], dst[1], dst[2]);
}

int s6e3had_init_self_mask_ctrl(struct maptbl *tbl)
{
	struct aod_dev_info *aod = tbl->pdata;
	struct aod_ioctl_props *props = &aod->props;

	props->self_mask_checksum_len = SELFMASK_CHECKSUM_LEN;
	props->self_mask_checksum = kmalloc(sizeof(u8) * props->self_mask_checksum_len, GFP_KERNEL);
	if (!props->self_mask_checksum) {
		panel_err("failed to mem alloc\n");
		return -ENOMEM;
	}
	props->self_mask_checksum[0] = SELFMASK_CHECKSUM_VALID1;
	props->self_mask_checksum[1] = SELFMASK_CHECKSUM_VALID2;
	panel_info("was called\n");
	return 0;
}

void s6e3had_copy_digital_pos(struct maptbl *tbl, u8 *dst)
{
	struct aod_dev_info *aod = tbl->pdata;
	struct aod_ioctl_props *props = &aod->props;

	if (props->digital.en == 0) {
		panel_info("digital clk was disabled\n");
		return;
	}

	dst[DIG_CLK_POS1_X1_REG] = (u8)(props->digital.pos1_x >> 8);
	dst[DIG_CLK_POS1_X2_REG] = (u8)(props->digital.pos1_x & 0xff);
	dst[DIG_CLK_POS1_Y1_REG] = (u8)(props->digital.pos1_y >> 8);
	dst[DIG_CLK_POS1_Y2_REG] = (u8)(props->digital.pos1_y & 0xff);

	dst[DIG_CLK_POS2_X1_REG] = (u8)(props->digital.pos2_x >> 8);
	dst[DIG_CLK_POS2_X2_REG] =  (u8)(props->digital.pos2_x & 0xff);
	dst[DIG_CLK_POS2_Y1_REG] = (u8)(props->digital.pos2_y >> 8);
	dst[DIG_CLK_POS2_Y2_REG] = (u8)(props->digital.pos2_y & 0xff);

	dst[DIG_CLK_POS3_X1_REG] = (u8)(props->digital.pos3_x >> 8);
	dst[DIG_CLK_POS3_X2_REG] =  (u8)(props->digital.pos3_x & 0xff);
	dst[DIG_CLK_POS3_Y1_REG] = (u8)(props->digital.pos3_y >> 8);
	dst[DIG_CLK_POS3_Y2_REG] = (u8)(props->digital.pos3_y & 0xff);

	dst[DIG_CLK_POS4_X1_REG] = (u8)(props->digital.pos4_x >> 8);
	dst[DIG_CLK_POS4_X2_REG] =  (u8)(props->digital.pos4_x & 0xff);
	dst[DIG_CLK_POS4_Y1_REG] = (u8)(props->digital.pos4_y >> 8);
	dst[DIG_CLK_POS4_Y2_REG] = (u8)(props->digital.pos4_y & 0xff);

	dst[DIG_CLK_WIDTH1] = (u8)(props->digital.img_width >> 8);
	dst[DIG_CLK_WIDTH2] = (u8)(props->digital.img_width & 0xff);
	dst[DIG_CLK_HEIGHT1] = (u8)(props->digital.img_height >> 8);
	dst[DIG_CLK_HEIGHT2] = (u8)(props->digital.img_height & 0xff);
}

void s6e3had_copy_time(struct maptbl *tbl, u8 *dst)
{
	struct aod_dev_info *aod = tbl->pdata;
	struct aod_ioctl_props *props = &aod->props;

	if (aod == NULL) {
		panel_err("aod is null\n");
		return;
	}

	dst[TIME_HH_REG] = props->cur_time.cur_h;
	dst[TIME_MM_REG] = props->cur_time.cur_m;
	dst[TIME_SS_REG] = props->cur_time.cur_s;
	dst[TIME_MSS_REG] = props->cur_time.cur_ms;

	panel_info("%x %x %x\n", dst[0], dst[1], dst[2]);
}


void s6e3had_copy_timer_rate(struct maptbl *tbl, u8 *dst)
{
	u8 value = 0;
	struct aod_dev_info *aod = tbl->pdata;
	struct aod_ioctl_props *props = &aod->props;

	if ((aod == NULL) || (props == NULL)) {
		panel_err("aod/props is null\n");
		return;
	}

	dst[2] &= ~0x10;

	if (props->analog.en) {
		value = SELF_IP_HOP_SS_EN | SELF_IP_HOP_MSS_EN;

		if (props->first_clk_update == 0)
			dst[2] |= 0x10;
	}

	switch (props->cur_time.interval) {
	case ALG_INTERVAL_100m:
		value |= 0x3;
		dst[2] = (dst[2] & ~0x03);
		break;
	case ALG_INTERVAL_200m:
		value |= 0x06;
		dst[2] = (dst[2] & ~0x03) | 0x01;
		break;
	case ALG_INTERVAL_500m:
		value |= 0x0f;
		dst[2] = (dst[2] & ~0x03) | 0x02;
		break;
	case ALG_INTERVAL_1000:
		value |= 0x1e;
		dst[2] = (dst[2] & ~0x03) | 0x03;
		break;
	case INTERVAL_DEBUG:
		value |= 0x01;
		dst[2] = (dst[2] & ~0x03) | 0x03;
		break;
	default:
		panel_info("invalid interval:%d\n",
				props->cur_time.interval);
		break;
	}

	dst[1] = value;
	panel_info("dst[1]:%x, dst[2]:%x\n",
			dst[1], dst[2]);
}


void s6e3had_copy_self_move_on_ctrl(struct maptbl *tbl, u8 *dst)
{
	char enable = 0;
	struct aod_dev_info *aod = tbl->pdata;
	struct aod_ioctl_props *props = &aod->props;

	if ((aod == NULL) || (props == NULL)) {
		panel_err("aod/props is null\n");
		return;
	}

	if (props->self_move_en)
		enable |= FB_SELF_MOVE_EN;

	dst[SM_ENABLE_REG] = enable;

	panel_info("%x\n", dst[SM_ENABLE_REG]);
}

void s6e3had_copy_analog_pos(struct maptbl *tbl, u8 *dst)
{
	struct aod_dev_info *aod = tbl->pdata;
	struct aod_ioctl_props *props = &aod->props;

	if ((aod == NULL) || (props == NULL)) {
		panel_err("aod/props is null\n");
		return;
	}

	switch (props->analog.rotate) {

	case ALG_ROTATE_0:
		dst[ANALOG_ROT_REG] = ALG_ROTATE_0;
		break;
	case ALG_ROTATE_90:
		dst[ANALOG_ROT_REG] = ALG_ROTATE_90;
		break;
	case ALG_ROTATE_180:
		dst[ANALOG_ROT_REG] = ALG_ROTATE_180;
		break;
	case ALG_ROTATE_270:
		dst[ANALOG_ROT_REG] = ALG_ROTATE_270;
		break;
	default:
		dst[ANALOG_ROT_REG] = ALG_ROTATE_0;
		panel_err("undefined rotation mode : %d\n",
				props->analog.rotate);
		break;
	}

	dst[ANALOG_POS_X1_REG] = (char)(props->analog.pos_x >> 8);
	dst[ANALOG_POS_X2_REG] = (char)(props->analog.pos_x & 0xff);
	dst[ANALOG_POS_Y1_REG] = (char)(props->analog.pos_y >> 8);
	dst[ANALOG_POS_Y2_REG] = (char)(props->analog.pos_y & 0xff);

	panel_dbg("%x\n", dst[ANALOG_POS_X1_REG]);
}

void s6e3had_copy_analog_en(struct maptbl *tbl, u8 *dst)
{
	char en_reg = 0;
	struct aod_dev_info *aod = tbl->pdata;
	struct aod_ioctl_props *props = &aod->props;

	if ((aod == NULL) || (props == NULL)) {
		panel_err("aod/props is null\n");
		return;
	}

	if (props->analog.en)
		en_reg = SC_A_CLK_EN | SC_DISP_ON;

	dst[ANALOG_EN_REG] = en_reg;

	panel_dbg("%x %x %x\n", dst[0], dst[1], dst[2]);
}

void s6e3had_copy_digital_en(struct maptbl *tbl, u8 *dst)
{
	char en_reg = 0;
	char disp_format = 0;
	struct aod_dev_info *aod = tbl->pdata;
	struct aod_ioctl_props *props = &aod->props;

	if (aod == NULL) {
		panel_err("aod is null\n");
		return;
	}

	if (props->digital.en) {

		if (props->digital.en_hh)
			disp_format |= (props->digital.en_hh & 0x03) << 2;

		if (props->digital.en_mm)
			disp_format |= (props->digital.en_mm & 0x03);

		if (props->cur_time.disp_24h)
			disp_format |= (props->cur_time.disp_24h & 0x03) << 4;

		en_reg = SC_D_CLK_EN | SC_DISP_ON;

		dst[DIGITAL_UN_REG] = props->digital.unicode_attr;
		dst[DIGITAL_FMT_REG] = disp_format;
	}

	dst[DIGITAL_EN_REG] = en_reg;

	panel_info("%x %x %x %x\n", dst[0], dst[1], dst[2], dst[3]);
}

int s6e3had_getidx_self_mode_pos(struct maptbl *tbl)
{
	int row = 0;
	struct aod_dev_info *aod = tbl->pdata;
	struct aod_ioctl_props *props = &aod->props;

	switch (props->cur_time.interval) {
	case ALG_INTERVAL_100m:
		panel_info("interval : 100msec\n");
		row = 0;
		break;
	case ALG_INTERVAL_200m:
		panel_info("interval : 200msec\n");
		row = 1;
		break;
	case ALG_INTERVAL_500m:
		panel_info("interval : 500msec\n");
		row = 2;
		break;
	case ALG_INTERVAL_1000:
		panel_info("interval : 1sec\n");
		row = 3;
		break;
	case INTERVAL_DEBUG:
		panel_info("interval : debug\n");
		row = 4;
		break;
	default:
		panel_info("invalid interval:%d\n",
				props->cur_time.interval);
		row = 0;
		break;
	}
	return maptbl_index(tbl, 0, row, 0);
}


void s6e3had_copy_self_move_reset(struct maptbl *tbl, u8 *dst)
{
	struct aod_dev_info *aod = tbl->pdata;
	struct aod_ioctl_props *props = &aod->props;

	props->self_reset_cnt++;

	dst[REG_MOVE_DSP_X] = (char)props->self_reset_cnt;

	panel_info("%x:%x:%x:%x:%x\n",
			dst[0], dst[1], dst[2], dst[3], dst[4]);
}

void s6e3had_copy_icon_ctrl(struct maptbl *tbl, u8 *dst)
{
	u8 enable = 0;
	struct aod_dev_info *aod = tbl->pdata;
	struct aod_ioctl_props *props = &aod->props;

	if ((aod == NULL) || (props == NULL)) {
		panel_err("aod/props is null\n");
		return;
	}

	panel_info("%d\n", props->icon.en);

	if (props->icon.en) {

		dst[ICON_REG_XPOS0] = (char)(props->icon.pos_x >> 8);
		dst[ICON_REG_XPOS1] = (char)(props->icon.pos_x & 0x00ff);
		dst[ICON_REG_YPOS0] = (char)(props->icon.pos_y >> 8);
		dst[ICON_REG_YPOS1] = (char)(props->icon.pos_y & 0x00ff);

		dst[ICON_REG_WIDTH0] = (char)(props->icon.width >> 8);
		dst[ICON_REG_WIDTH1] = (char)(props->icon.width & 0xff);
		dst[ICON_REG_HEIGHT0] = (char)(props->icon.height >> 8);
		dst[ICON_REG_HEIGHT1] = (char)(props->icon.height & 0x00ff);

		dst[ICON_REG_COLOR0] = (char)(props->icon.color >> 24);
		dst[ICON_REG_COLOR1] = (char)(props->icon.color >> 16);
		dst[ICON_REG_COLOR2] = (char)(props->icon.color >> 8);
		dst[ICON_REG_COLOR3] = (char)(props->icon.color & 0x00ff);

		enable = ICON_ENABLE;
	}

	dst[ICON_REG_EN] = enable;

	panel_info("%x:%x:%x:%x:%x\n",
			dst[0], dst[1], dst[2], dst[3], dst[4]);
}


void s6e3had_copy_digital_color(struct maptbl *tbl, u8 *dst)
{
	struct aod_dev_info *aod = tbl->pdata;
	struct aod_ioctl_props *props = &aod->props;

	if ((aod == NULL) || (props == NULL)) {
		panel_err("aod/props is null\n");
		return;
	}

	if (!props->digital.en)
		return;

	dst[DIG_COLOR_ALPHA_REG] = (char)((props->digital.color >> 24) & 0xff);
	dst[DIG_COLOR_RED_REG] = (char)((props->digital.color >> 16) & 0xff);
	dst[DIG_COLOR_GREEN_REG] = (char)((props->digital.color >> 8) & 0xff);
	dst[DIG_COLOR_BLUE_REG] = (char)(props->digital.color & 0xff);
}

void s6e3had_copy_digital_un_width(struct maptbl *tbl, u8 *dst)
{
	struct aod_dev_info *aod = tbl->pdata;
	struct aod_ioctl_props *props = &aod->props;

	if ((aod == NULL) || (props == NULL)) {
		panel_err("aod/props is null\n");
		return;
	}

	if (!props->digital.en)
		return;

	dst[DIG_UN_WIDTH0] = (char)((props->digital.unicode_width >> 8) & 0xff);
	dst[DIG_UN_WIDTH1] = (char)(props->digital.unicode_width & 0xff);
}


#ifdef SUPPORT_NORMAL_SELF_MOVE
int s6e3had_getidx_self_pattern(struct maptbl *tbl)
{
	int row = 0;
	struct aod_dev_info *aod = tbl->pdata;
	struct aod_ioctl_props *props = &aod->props;

	switch (props->self_move_pattern) {
	case 1:
	case 3:
		panel_info("pattern : %d\n",
				props->self_move_pattern);
		row = 0;
		break;
	case 2:
	case 4:
		panel_info("pattern : %d\n",
				props->self_move_pattern);
		row = 1;
		break;
	default:
		panel_info("invalid pattern:%d\n",
				props->self_move_pattern);
		row = 0;
		break;
	}

	return maptbl_index(tbl, 0, row, 0);
}

void s6e3had_copy_self_move_pattern(struct maptbl *tbl, u8 *dst)
{
	int idx;

	if (!tbl || !dst) {
		panel_err("invalid parameter (tbl %p, dst %p\n",
				tbl, dst);
		return;
	}

	idx = maptbl_getidx(tbl);
	if (idx < 0) {
		panel_err("failed to getidx %d\n", idx);
		return;
	}
	memcpy(dst, &(tbl->arr)[idx], sizeof(u8) * tbl->sz_copy);

#ifdef FAST_TIMER
	dst[7] = 0x22;
#endif

	panel_info("%x:%x:%x:%x:%x:%x:%x:(%x):%x\n",
			dst[0], dst[1], dst[2], dst[3],
		dst[4], dst[5], dst[6], dst[7], dst[8]);
}
#endif
