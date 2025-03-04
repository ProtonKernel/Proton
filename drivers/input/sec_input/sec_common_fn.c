/*
 * sec_common_fn.c - samsung common functions
 *
 * Copyright (C) 2020 Samsung Electronics
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 */

#include "sec_input.h"

#if !IS_ENABLED(CONFIG_SEC_KUNIT)
#define __visible_for_testing static
#endif

static char *lcd_id;
module_param(lcd_id, charp, S_IRUGO);

#if IS_ENABLED(CONFIG_TOUCHSCREEN_DUAL_FOLDABLE)
static char *lcd_id1;
module_param(lcd_id1, charp, S_IRUGO);
#endif

struct device *ptsp;
EXPORT_SYMBOL(ptsp);

struct sec_ts_secure_data *psecuretsp = NULL;
EXPORT_SYMBOL(psecuretsp);

static int sec_input_lcd_parse_panel_id(char *panel_id)
{
	char *pt;
	int lcd_id_p = 0;

	if (IS_ERR_OR_NULL(panel_id))
		return lcd_id_p;

	for (pt = panel_id; *pt != 0; pt++)  {
		lcd_id_p <<= 4;
		switch (*pt) {
		case '0' ... '9':
			lcd_id_p += *pt - '0';
			break;
		case 'a' ... 'f':
			lcd_id_p += 10 + *pt - 'a';
			break;
		case 'A' ... 'F':
			lcd_id_p += 10 + *pt - 'A';
			break;
		}
	}
	return lcd_id_p;
}

int sec_input_get_lcd_id(struct device *dev)
{
#if IS_ENABLED(CONFIG_TOUCHSCREEN_DUAL_FOLDABLE)
	struct sec_ts_plat_data *pdata = dev->platform_data;
#endif
#if !IS_ENABLED(CONFIG_SMCDSD_PANEL)
	int lcdtype = 0;
#endif
#if IS_ENABLED(CONFIG_EXYNOS_DPU30) || IS_ENABLED(CONFIG_MCD_PANEL) || IS_ENABLED(CONFIG_USDM_PANEL)
	int connected;
#endif
	int lcd_id_param = 0;

#if IS_ENABLED(CONFIG_DISPLAY_SAMSUNG)
	lcdtype = get_lcd_attached("GET");
	if (lcdtype == 0xFFFFFF) {
		input_err(true, dev, "%s: lcd is not attached(GET)\n", __func__);
		return -ENODEV;
	}
#endif

#if IS_ENABLED(CONFIG_EXYNOS_DPU30) || IS_ENABLED(CONFIG_MCD_PANEL) || IS_ENABLED(CONFIG_USDM_PANEL)
	connected = get_lcd_info("connected");
	if (connected < 0) {
		input_err(true, dev, "%s: Failed to get lcd info(connected)\n", __func__);
		return -EINVAL;
	}

	if (!connected) {
		input_err(true, dev, "%s: lcd is disconnected(connected)\n", __func__);
		return -ENODEV;
	}

	input_info(true, dev, "%s: lcd is connected\n", __func__);

	lcdtype = get_lcd_info("id");
	if (lcdtype < 0) {
		input_err(true, dev, "%s: Failed to get lcd info(id)\n", __func__);
		return -EINVAL;
	}
#endif
#if IS_ENABLED(CONFIG_SMCDSD_PANEL)
	if (!lcdtype) {
		input_err(true, dev, "%s: lcd is disconnected(lcdtype)\n", __func__);
		return -ENODEV;
	}
#endif

#if IS_ENABLED(CONFIG_TOUCHSCREEN_DUAL_FOLDABLE)
	input_info(true, dev, "%s: %d\n", __func__, pdata->support_dual_foldable);
	if (pdata->support_dual_foldable == SUB_TOUCH)
		lcd_id_param = sec_input_lcd_parse_panel_id(lcd_id1);
	else
#endif
		lcd_id_param = sec_input_lcd_parse_panel_id(lcd_id);

	if (lcdtype <= 0 && lcd_id_param != 0) {
		lcdtype = lcd_id_param;
		if (lcdtype == 0xFFFFFF) {
			input_err(true, dev, "%s: lcd is not attached(PARAM)\n", __func__);
			return -ENODEV;
		}
	}

	input_info(true, dev, "%s: lcdtype 0x%08X\n", __func__, lcdtype);
	return lcdtype;
}
EXPORT_SYMBOL(sec_input_get_lcd_id);

static void sec_input_handler_wait_resume_work(struct work_struct *work)
{
	struct sec_ts_plat_data *pdata = container_of(work, struct sec_ts_plat_data, irq_work);
	unsigned int irq = gpio_to_irq(pdata->irq_gpio);
	struct irq_desc *desc = irq_to_desc(irq);
	int ret;

	ret = wait_for_completion_interruptible_timeout(&pdata->resume_done,
			msecs_to_jiffies(SEC_TS_WAKE_LOCK_TIME));
	if (ret == 0) {
		input_err(true, pdata->dev, "%s: LPM: pm resume is not handled\n", __func__);
		goto out;
	}
	if (ret < 0) {
		input_err(true, pdata->dev, "%s: LPM: -ERESTARTSYS if interrupted, %d\n", __func__, ret);
		goto out;
	}

	if (desc && desc->action && desc->action->thread_fn) {
		input_info(true, pdata->dev, "%s: run irq thread\n", __func__);
		desc->action->thread_fn(irq, desc->action->dev_id);
	}
out:
	if (!desc)
		return;

	while (desc->depth > 0) {
		enable_irq(irq);
		input_info(true, pdata->dev, "%s: depth: %d\n", __func__, desc->depth);
	}
}

int sec_input_handler_start(struct device *dev)
{
	struct sec_ts_plat_data *pdata = dev->platform_data;
	unsigned int irq = gpio_to_irq(pdata->irq_gpio);
	struct irq_desc *desc = irq_to_desc(irq);

	if (desc && desc->action) {
		if ((desc->action->flags & IRQF_TRIGGER_LOW) && (gpio_get_value(pdata->irq_gpio) == 1))
			return SEC_ERROR;
	}

	if (pdata->power_state == SEC_INPUT_STATE_LPM) {
		__pm_wakeup_event(pdata->sec_ws, SEC_TS_WAKE_LOCK_TIME);

		if (!pdata->resume_done.done) {
			if (!IS_ERR_OR_NULL(pdata->irq_workqueue)) {
				input_info(true, dev, "%s: disable irq and queue waiting work\n", __func__);
				disable_irq_nosync(gpio_to_irq(pdata->irq_gpio));
				queue_work(pdata->irq_workqueue, &pdata->irq_work);
			} else {
				input_err(true, dev, "%s: irq_workqueue not exist\n", __func__);
			}
			return SEC_ERROR;
		}
	}

	return SEC_SUCCESS;
}
EXPORT_SYMBOL(sec_input_handler_start);

/************************************************************
 *  720  * 1480 : <48 96 60>
 * indicator: 24dp navigator:48dp edge:60px dpi=320
 * 1080  * 2220 :  4096 * 4096 : <133 266 341>  (approximately value)
 ************************************************************/
static void location_detect(struct sec_ts_plat_data *pdata, int t_id)
{
	int x = pdata->coord[t_id].x, y = pdata->coord[t_id].y;

	memset(pdata->location, 0x00, SEC_TS_LOCATION_DETECT_SIZE);

	if (x < pdata->area_edge)
		strlcat(pdata->location, "E.", SEC_TS_LOCATION_DETECT_SIZE);
	else if (x < (pdata->max_x - pdata->area_edge))
		strlcat(pdata->location, "C.", SEC_TS_LOCATION_DETECT_SIZE);
	else
		strlcat(pdata->location, "e.", SEC_TS_LOCATION_DETECT_SIZE);

	if (y < pdata->area_indicator)
		strlcat(pdata->location, "S", SEC_TS_LOCATION_DETECT_SIZE);
	else if (y < (pdata->max_y - pdata->area_navigation))
		strlcat(pdata->location, "C", SEC_TS_LOCATION_DETECT_SIZE);
	else
		strlcat(pdata->location, "N", SEC_TS_LOCATION_DETECT_SIZE);
}

void sec_delay(unsigned int ms)
{
	if (ms < 20)
		usleep_range(ms * 1000, ms * 1000);
	else
		msleep(ms);
}
EXPORT_SYMBOL(sec_delay);

int sec_input_set_temperature(struct device *dev, int state)
{
	struct sec_ts_plat_data *pdata = dev->platform_data;
	int ret = 0;
	u8 temperature_data = 0;
	bool bforced = false;

	if (pdata->set_temperature == NULL) {
		input_dbg(false, dev, "%s: vendor function is not allocated\n", __func__);
		return SEC_ERROR;
	}

	if (state == SEC_INPUT_SET_TEMPERATURE_NORMAL) {
		if (pdata->touch_count) {
			pdata->tsp_temperature_data_skip = true;
			input_err(true, dev, "%s: skip, t_cnt(%d)\n",
					__func__, pdata->touch_count);
			return SEC_SUCCESS;
		}
	} else if (state == SEC_INPUT_SET_TEMPERATURE_IN_IRQ) {
		if (pdata->touch_count != 0 || pdata->tsp_temperature_data_skip == false)
			return SEC_SUCCESS;
	} else if (state == SEC_INPUT_SET_TEMPERATURE_FORCE) {
		bforced = true;
	} else {
		input_err(true, dev, "%s: invalid param %d\n", __func__, state);
		return SEC_ERROR;
	}

	pdata->tsp_temperature_data_skip = false;

	if (!pdata->psy)
		pdata->psy = power_supply_get_by_name("battery");

	if (!pdata->psy) {
		input_err(true, dev, "%s: cannot find power supply\n", __func__);
		return SEC_ERROR;
	}

	ret = power_supply_get_property(pdata->psy, POWER_SUPPLY_PROP_TEMP, &pdata->psy_value);
	if (ret < 0) {
		input_err(true, dev, "%s: couldn't get temperature value, ret:%d\n", __func__, ret);
		return ret;
	}

	temperature_data = (u8)(pdata->psy_value.intval / 10);

	if (bforced || pdata->tsp_temperature_data != temperature_data) {
		ret = pdata->set_temperature(dev, temperature_data);
		if (ret < 0) {
			input_err(true, dev, "%s: failed to write temperature %u, ret=%d\n",
					__func__, temperature_data, ret);
			return ret;
		}

		pdata->tsp_temperature_data = temperature_data;
		input_info(true, dev, "%s set temperature:%u\n", __func__, temperature_data);
	} else {
		input_dbg(true, dev, "%s skip temperature:%u\n", __func__, temperature_data);
	}

	return SEC_SUCCESS;
}
EXPORT_SYMBOL(sec_input_set_temperature);

void sec_input_set_grip_type(struct device *dev, u8 set_type)
{
	struct sec_ts_plat_data *pdata = dev->platform_data;
	u8 mode = G_NONE;

	if (pdata->set_grip_data == NULL) {
		input_dbg(true, dev, "%s: vendor function is not allocated\n", __func__);
		return;
	}

	input_info(true, dev, "%s: re-init grip(%d), edh:%d, edg:%d, lan:%d\n", __func__,
			set_type, pdata->grip_data.edgehandler_direction,
			pdata->grip_data.edge_range, pdata->grip_data.landscape_mode);

	if (pdata->grip_data.edgehandler_direction != 0)
		mode |= G_SET_EDGE_HANDLER;

	if (set_type == GRIP_ALL_DATA) {
		/* edge */
		if (pdata->grip_data.edge_range != 60)
			mode |= G_SET_EDGE_ZONE;

		/* dead zone default 0 mode, 32 */
		if (pdata->grip_data.landscape_mode == 1)
			mode |= G_SET_LANDSCAPE_MODE;
		else
			mode |= G_SET_NORMAL_MODE;
	}

	if (mode)
		pdata->set_grip_data(dev, mode);
}
EXPORT_SYMBOL(sec_input_set_grip_type);

int sec_input_check_cover_type(struct device *dev)
{
	struct sec_ts_plat_data *pdata = dev->platform_data;
	int cover_cmd = 0;

	switch (pdata->cover_type) {
	case SEC_TS_FLIP_COVER:
	case SEC_TS_SVIEW_COVER:
	case SEC_TS_SVIEW_CHARGER_COVER:
	case SEC_TS_S_VIEW_WALLET_COVER:
	case SEC_TS_LED_COVER:
	case SEC_TS_CLEAR_COVER:
	case SEC_TS_KEYBOARD_KOR_COVER:
	case SEC_TS_KEYBOARD_US_COVER:
	case SEC_TS_CLEAR_SIDE_VIEW_COVER:
	case SEC_TS_MINI_SVIEW_WALLET_COVER:
	case SEC_TS_MONTBLANC_COVER:
	case SEC_TS_CLEAR_CAMERA_VIEW_COVER:
		cover_cmd = pdata->cover_type;
		break;
	default:
		input_err(true, dev, "%s: not change touch state, cover_type=%d\n",
				__func__, pdata->cover_type);
		break;
	}

	return cover_cmd;
}
EXPORT_SYMBOL(sec_input_check_cover_type);

void sec_input_set_fod_info(struct device *dev, int vi_x, int vi_y, int vi_size, int vi_event)
{
	struct sec_ts_plat_data *pdata = dev->platform_data;
	int byte_size = vi_x * vi_y / 8;

	if (vi_x * vi_y % 8)
		byte_size++;

	pdata->fod_data.vi_x = vi_x;
	pdata->fod_data.vi_y = vi_y;
	pdata->fod_data.vi_size = vi_size;
	pdata->fod_data.vi_event = vi_event;

	if (byte_size != vi_size)
		input_err(true, dev, "%s: NEED TO CHECK! vi size %d maybe wrong (byte size should be %d)\n",
				__func__, vi_size, byte_size);

	input_info(true, dev, "%s: vi_event:%d, x:%d, y:%d, size:%d\n",
			__func__, pdata->fod_data.vi_event, pdata->fod_data.vi_x, pdata->fod_data.vi_y,
			pdata->fod_data.vi_size);
}
EXPORT_SYMBOL(sec_input_set_fod_info);

ssize_t sec_input_get_fod_info(struct device *dev, char *buf)
{
	struct sec_ts_plat_data *pdata = dev->platform_data;

	if (!pdata->support_fod) {
		input_err(true, dev, "%s: fod is not supported\n", __func__);
		return snprintf(buf, SEC_CMD_BUF_SIZE, "NA");
	}

	if (pdata->x_node_num <= 0 || pdata->y_node_num <= 0) {
		input_err(true, dev, "%s: x/y node num value is wrong\n", __func__);
		return snprintf(buf, SEC_CMD_BUF_SIZE, "NG");
	}

	input_info(true, dev, "%s: x:%d/%d, y:%d/%d, size:%d\n",
			__func__, pdata->fod_data.vi_x, pdata->x_node_num,
			pdata->fod_data.vi_y, pdata->y_node_num, pdata->fod_data.vi_size);

	return snprintf(buf, SEC_CMD_BUF_SIZE, "%d,%d,%d,%d,%d",
			pdata->fod_data.vi_x, pdata->fod_data.vi_y,
			pdata->fod_data.vi_size, pdata->x_node_num, pdata->y_node_num);
}
EXPORT_SYMBOL(sec_input_get_fod_info);

bool sec_input_set_fod_rect(struct device *dev, int *rect_data)
{
	struct sec_ts_plat_data *pdata = dev->platform_data;
	int i;

	pdata->fod_data.set_val = 1;

	if (rect_data[0] <= 0 || rect_data[1] <= 0 || rect_data[2] <= 0 || rect_data[3] <= 0)
		pdata->fod_data.set_val = 0;

	if (pdata->display_x > 0 && pdata->display_y > 0)
		if (rect_data[0] >= pdata->display_x || rect_data[1] >= pdata->display_y
				|| rect_data[2] >= pdata->display_x || rect_data[3] >= pdata->display_y)
			pdata->fod_data.set_val = 0;

	if (pdata->fod_data.set_val)
		for (i = 0; i < 4; i++)
			pdata->fod_data.rect_data[i] = rect_data[i];

	return pdata->fod_data.set_val;
}
EXPORT_SYMBOL(sec_input_set_fod_rect);

int sec_input_check_wirelesscharger_mode(struct device *dev, int mode, int force)
{
	struct sec_ts_plat_data *pdata = dev->platform_data;

	if (mode != TYPE_WIRELESS_CHARGER_NONE
			&& mode != TYPE_WIRELESS_CHARGER
			&& mode != TYPE_WIRELESS_BATTERY_PACK) {
		input_err(true, dev,
				"%s: invalid param %d\n", __func__, mode);
		return SEC_ERROR;
	}

	if (pdata->force_wirelesscharger_mode == true && force == 0) {
		input_err(true, dev,
				"%s: [force enable] skip %d\n", __func__, mode);
		return SEC_SKIP;
	}

	if (force == 1) {
		if (mode == TYPE_WIRELESS_CHARGER_NONE) {
			pdata->force_wirelesscharger_mode = false;
			input_err(true, dev,
					"%s: force enable off\n", __func__);
			return SEC_SKIP;
		}

		pdata->force_wirelesscharger_mode = true;
	}

	pdata->wirelesscharger_mode = mode & 0xFF;

	return SEC_SUCCESS;
}
EXPORT_SYMBOL(sec_input_check_wirelesscharger_mode);

ssize_t sec_input_get_common_hw_param(struct sec_ts_plat_data *pdata, char *buf)
{
	char buff[SEC_INPUT_HW_PARAM_SIZE];
	char tbuff[SEC_CMD_STR_LEN];

	memset(buff, 0x00, sizeof(buff));

	memset(tbuff, 0x00, sizeof(tbuff));
	snprintf(tbuff, sizeof(tbuff), "\"TITO\":\"%02X%02X%02X%02X\",",
			pdata->hw_param.ito_test[0], pdata->hw_param.ito_test[1],
			pdata->hw_param.ito_test[2], pdata->hw_param.ito_test[3]);
	strlcat(buff, tbuff, sizeof(buff));

	memset(tbuff, 0x00, sizeof(tbuff));
	snprintf(tbuff, sizeof(tbuff), "\"TMUL\":\"%d\",", pdata->hw_param.multi_count);
	strlcat(buff, tbuff, sizeof(buff));

	memset(tbuff, 0x00, sizeof(tbuff));
	snprintf(tbuff, sizeof(tbuff), "\"TWET\":\"%d\",", pdata->hw_param.wet_count);
	strlcat(buff, tbuff, sizeof(buff));

	memset(tbuff, 0x00, sizeof(tbuff));
	snprintf(tbuff, sizeof(tbuff), "\"TNOI\":\"%d\",", pdata->hw_param.noise_count);
	strlcat(buff, tbuff, sizeof(buff));

	memset(tbuff, 0x00, sizeof(tbuff));
	snprintf(tbuff, sizeof(tbuff), "\"TCOM\":\"%d\",", pdata->hw_param.comm_err_count);
	strlcat(buff, tbuff, sizeof(buff));

	memset(tbuff, 0x00, sizeof(tbuff));
	snprintf(tbuff, sizeof(tbuff), "\"TCHK\":\"%d\",", pdata->hw_param.checksum_result);
	strlcat(buff, tbuff, sizeof(buff));

	memset(tbuff, 0x00, sizeof(tbuff));
	snprintf(tbuff, sizeof(tbuff), "\"TTCN\":\"%d\",\"TACN\":\"%d\",\"TSCN\":\"%d\",",
			pdata->hw_param.all_finger_count, pdata->hw_param.all_aod_tap_count,
			pdata->hw_param.all_spay_count);
	strlcat(buff, tbuff, sizeof(buff));

	memset(tbuff, 0x00, sizeof(tbuff));
	snprintf(tbuff, sizeof(tbuff), "\"TMCF\":\"%d\",", pdata->hw_param.mode_change_failed_count);
	strlcat(buff, tbuff, sizeof(buff));

	memset(tbuff, 0x00, sizeof(tbuff));
	snprintf(tbuff, sizeof(tbuff), "\"TRIC\":\"%d\"", pdata->hw_param.ic_reset_count);
	strlcat(buff, tbuff, sizeof(buff));

	return snprintf(buf, SEC_INPUT_HW_PARAM_SIZE, "%s", buff);
}
EXPORT_SYMBOL(sec_input_get_common_hw_param);

void sec_input_clear_common_hw_param(struct sec_ts_plat_data *pdata)
{
	pdata->hw_param.multi_count = 0;
	pdata->hw_param.wet_count = 0;
	pdata->hw_param.noise_count = 0;
	pdata->hw_param.comm_err_count = 0;
	pdata->hw_param.checksum_result = 0;
	pdata->hw_param.all_finger_count = 0;
	pdata->hw_param.all_aod_tap_count = 0;
	pdata->hw_param.all_spay_count = 0;
	pdata->hw_param.mode_change_failed_count = 0;
	pdata->hw_param.ic_reset_count = 0;
}
EXPORT_SYMBOL(sec_input_clear_common_hw_param);

void sec_input_print_info(struct device *dev, struct sec_tclm_data *tdata)
{
	struct sec_ts_plat_data *pdata = dev->platform_data;
	unsigned int irq = gpio_to_irq(pdata->irq_gpio);
	struct irq_desc *desc = irq_to_desc(irq);
	char tclm_buff[INPUT_TCLM_LOG_BUF_SIZE] = { 0 };

	pdata->print_info_cnt_open++;

	if (pdata->print_info_cnt_open > 0xfff0)
		pdata->print_info_cnt_open = 0;

	if (pdata->touch_count == 0)
		pdata->print_info_cnt_release++;

#if IS_ENABLED(CONFIG_INPUT_TOUCHSCREEN_TCLMV2)
	if (tdata && (tdata->tclm_level == TCLM_LEVEL_NOT_SUPPORT))
		snprintf(tclm_buff, sizeof(tclm_buff), "");
	else if (tdata && tdata->tclm_string)
		snprintf(tclm_buff, sizeof(tclm_buff), "C%02XT%04X.%4s%s Cal_flag:%d fail_cnt:%d",
			tdata->nvdata.cal_count, tdata->nvdata.tune_fix_ver,
			tdata->tclm_string[tdata->nvdata.cal_position].f_name,
			(tdata->tclm_level == TCLM_LEVEL_LOCKDOWN) ? ".L" : " ",
			tdata->nvdata.cal_fail_falg, tdata->nvdata.cal_fail_cnt);
	else
		snprintf(tclm_buff, sizeof(tclm_buff), "TCLM data is empty");
#else
	snprintf(tclm_buff, sizeof(tclm_buff), "");
#endif

	input_info(true, dev,
			"mode:%04X tc:%d noise:%d/%d ext_n:%d wet:%d wc:%d(f:%d) lp:%x fn:%04X/%04X irqd:%d ED:%d PK:%d LS:%d// v:%02X%02X %s // id:%d,%d tmp:%d // #%d %d\n",
			pdata->print_info_currnet_mode, pdata->touch_count,
			pdata->touch_noise_status, pdata->touch_pre_noise_status,
			pdata->external_noise_mode, pdata->wet_mode,
			pdata->wirelesscharger_mode, pdata->force_wirelesscharger_mode,
			pdata->lowpower_mode, pdata->touch_functions, pdata->ic_status, desc->depth,
			pdata->ed_enable, pdata->pocket_mode, pdata->low_sensitivity_mode,
			pdata->img_version_of_ic[2], pdata->img_version_of_ic[3],
			tclm_buff, pdata->tspid_val, pdata->tspicid_val,
			pdata->tsp_temperature_data,
			pdata->print_info_cnt_open, pdata->print_info_cnt_release);
}
EXPORT_SYMBOL(sec_input_print_info);

void sec_input_proximity_report(struct device *dev, int data)
{
	struct sec_ts_plat_data *pdata = dev->platform_data;

	if (!pdata->support_ear_detect || !pdata->input_dev_proximity)
		return;

	if (!(pdata->ed_enable || pdata->pocket_mode))
		return;

	input_info(true, dev, "%s: PROX(%d)\n", __func__, data);
	input_report_abs(pdata->input_dev_proximity, ABS_MT_CUSTOM, data);
	input_sync(pdata->input_dev_proximity);
}
EXPORT_SYMBOL(sec_input_proximity_report);

void sec_input_gesture_report(struct device *dev, int id, int x, int y)
{
	struct sec_ts_plat_data *pdata = dev->platform_data;
	char buff[SEC_TS_GESTURE_REPORT_BUFF_SIZE] = { 0 };

	pdata->gesture_id = id;
	pdata->gesture_x = x;
	pdata->gesture_y = y;

	input_report_key(pdata->input_dev, KEY_BLACK_UI_GESTURE, 1);
	input_sync(pdata->input_dev);
	input_report_key(pdata->input_dev, KEY_BLACK_UI_GESTURE, 0);
	input_sync(pdata->input_dev);

	if (id == SPONGE_EVENT_TYPE_SPAY) {
		snprintf(buff, sizeof(buff), "SPAY");
		pdata->hw_param.all_spay_count++;
	} else if (id == SPONGE_EVENT_TYPE_SINGLE_TAP) {
		snprintf(buff, sizeof(buff), "SINGLE TAP");
	} else if (id == SPONGE_EVENT_TYPE_AOD_DOUBLETAB) {
		snprintf(buff, sizeof(buff), "AOD");
		pdata->hw_param.all_aod_tap_count++;
	} else if (id == SPONGE_EVENT_TYPE_FOD_PRESS) {
		snprintf(buff, sizeof(buff), "FOD PRESS");
	} else if (id == SPONGE_EVENT_TYPE_FOD_RELEASE) {
		snprintf(buff, sizeof(buff), "FOD RELEASE");
	} else if (id == SPONGE_EVENT_TYPE_FOD_OUT) {
		snprintf(buff, sizeof(buff), "FOD OUT");
	} else if (id == SPONGE_EVENT_TYPE_TSP_SCAN_UNBLOCK) {
		snprintf(buff, sizeof(buff), "SCAN UNBLOCK");
	} else if (id == SPONGE_EVENT_TYPE_TSP_SCAN_BLOCK) {
		snprintf(buff, sizeof(buff), "SCAN BLOCK");
	} else if (id == SPONGE_EVENT_TYPE_LONG_PRESS) {
		snprintf(buff, sizeof(buff), "LONG PRESS");
	} else {
		snprintf(buff, sizeof(buff), "");
	}

#if IS_ENABLED(CONFIG_SAMSUNG_PRODUCT_SHIP)
	input_info(true, dev, "%s: %s: %d\n", __func__, buff, pdata->gesture_id);
#else
	input_info(true, dev, "%s: %s: %d, %d, %d\n",
			__func__, buff, pdata->gesture_id, pdata->gesture_x, pdata->gesture_y);
#endif
}
EXPORT_SYMBOL(sec_input_gesture_report);

static void sec_input_coord_report(struct device *dev, u8 t_id)
{
	struct sec_ts_plat_data *pdata = dev->platform_data;
	int action = pdata->coord[t_id].action;

	if (action == SEC_TS_COORDINATE_ACTION_RELEASE) {
		input_mt_slot(pdata->input_dev, t_id);
		if (pdata->support_mt_pressure)
			input_report_abs(pdata->input_dev, ABS_MT_PRESSURE, 0);
		input_mt_report_slot_state(pdata->input_dev, MT_TOOL_FINGER, 0);

		pdata->palm_flag &= ~(1 << t_id);

		if (pdata->touch_count > 0)
			pdata->touch_count--;
		if (pdata->touch_count == 0) {
			input_report_key(pdata->input_dev, BTN_TOUCH, 0);
			input_report_key(pdata->input_dev, BTN_TOOL_FINGER, 0);
			pdata->hw_param.check_multi = 0;
			pdata->print_info_cnt_release = 0;
			pdata->palm_flag = 0;
		}
		input_report_key(pdata->input_dev, BTN_PALM, pdata->palm_flag);
	} else if (action == SEC_TS_COORDINATE_ACTION_PRESS || action == SEC_TS_COORDINATE_ACTION_MOVE) {
		if (action == SEC_TS_COORDINATE_ACTION_PRESS) {
			pdata->touch_count++;
			pdata->coord[t_id].p_x = pdata->coord[t_id].x;
			pdata->coord[t_id].p_y = pdata->coord[t_id].y;

			pdata->hw_param.all_finger_count++;
			if ((pdata->touch_count > 4) && (pdata->hw_param.check_multi == 0)) {
				pdata->hw_param.check_multi = 1;
				pdata->hw_param.multi_count++;
			}
		} else {
			/* action == SEC_TS_COORDINATE_ACTION_MOVE */
			pdata->coord[t_id].mcount++;
		}

		input_mt_slot(pdata->input_dev, t_id);
		input_mt_report_slot_state(pdata->input_dev, MT_TOOL_FINGER, 1);
		input_report_key(pdata->input_dev, BTN_TOUCH, 1);
		input_report_key(pdata->input_dev, BTN_TOOL_FINGER, 1);
		input_report_key(pdata->input_dev, BTN_PALM, pdata->palm_flag);

		input_report_abs(pdata->input_dev, ABS_MT_POSITION_X, pdata->coord[t_id].x);
		input_report_abs(pdata->input_dev, ABS_MT_POSITION_Y, pdata->coord[t_id].y);
		input_report_abs(pdata->input_dev, ABS_MT_TOUCH_MAJOR, pdata->coord[t_id].major);
		input_report_abs(pdata->input_dev, ABS_MT_TOUCH_MINOR, pdata->coord[t_id].minor);

		if (pdata->support_mt_pressure)
			input_report_abs(pdata->input_dev, ABS_MT_PRESSURE, pdata->coord[t_id].z);
	}
}

static void sec_input_coord_log(struct device *dev, u8 t_id, int action)
{
	struct sec_ts_plat_data *pdata = dev->platform_data;

	location_detect(pdata, t_id);

	if (action == SEC_TS_COORDINATE_ACTION_PRESS) {
#if !IS_ENABLED(CONFIG_SAMSUNG_PRODUCT_SHIP)
		input_info(true, dev,
				"[P] tID:%d.%d x:%d y:%d z:%d major:%d minor:%d loc:%s tc:%d type:%X noise:(%x,%d%d), nlvl:%d, maxS:%d, hid:%d, fid:%d\n",
				t_id, (pdata->input_dev->mt->trkid - 1) & TRKID_MAX,
				pdata->coord[t_id].x, pdata->coord[t_id].y, pdata->coord[t_id].z,
				pdata->coord[t_id].major, pdata->coord[t_id].minor,
				pdata->location, pdata->touch_count,
				pdata->coord[t_id].ttype,
				pdata->coord[t_id].noise_status, pdata->touch_noise_status,
				pdata->touch_pre_noise_status, pdata->coord[t_id].noise_level,
				pdata->coord[t_id].max_strength, pdata->coord[t_id].hover_id_num,
				pdata->coord[t_id].freq_id);
#else
		input_info(true, dev,
				"[P] tID:%d.%d z:%d major:%d minor:%d loc:%s tc:%d type:%X noise:(%x,%d%d), nlvl:%d, maxS:%d, hid:%d, fid:%d\n",
				t_id, (pdata->input_dev->mt->trkid - 1) & TRKID_MAX,
				pdata->coord[t_id].z, pdata->coord[t_id].major,
				pdata->coord[t_id].minor, pdata->location, pdata->touch_count,
				pdata->coord[t_id].ttype,
				pdata->coord[t_id].noise_status, pdata->touch_noise_status,
				pdata->touch_pre_noise_status, pdata->coord[t_id].noise_level,
				pdata->coord[t_id].max_strength, pdata->coord[t_id].hover_id_num,
				pdata->coord[t_id].freq_id);
#endif

	} else if (action == SEC_TS_COORDINATE_ACTION_MOVE) {
#if !IS_ENABLED(CONFIG_SAMSUNG_PRODUCT_SHIP)
		input_info(true, dev,
				"[M] tID:%d.%d x:%d y:%d z:%d major:%d minor:%d loc:%s tc:%d type:%X noise:(%x,%d%d), nlvl:%d, maxS:%d, hid:%d, fid:%d\n",
				t_id, pdata->input_dev->mt->trkid & TRKID_MAX,
				pdata->coord[t_id].x, pdata->coord[t_id].y, pdata->coord[t_id].z,
				pdata->coord[t_id].major, pdata->coord[t_id].minor,
				pdata->location, pdata->touch_count,
				pdata->coord[t_id].ttype, pdata->coord[t_id].noise_status,
				pdata->touch_noise_status, pdata->touch_pre_noise_status,
				pdata->coord[t_id].noise_level, pdata->coord[t_id].max_strength,
				pdata->coord[t_id].hover_id_num, pdata->coord[t_id].freq_id);
#else
		input_info(true, dev,
				"[M] tID:%d.%d z:%d major:%d minor:%d loc:%s tc:%d type:%X noise:(%x,%d%d), nlvl:%d, maxS:%d, hid:%d, fid:%d\n",
				t_id, pdata->input_dev->mt->trkid & TRKID_MAX, pdata->coord[t_id].z,
				pdata->coord[t_id].major, pdata->coord[t_id].minor,
				pdata->location, pdata->touch_count,
				pdata->coord[t_id].ttype, pdata->coord[t_id].noise_status,
				pdata->touch_noise_status, pdata->touch_pre_noise_status,
				pdata->coord[t_id].noise_level, pdata->coord[t_id].max_strength,
				pdata->coord[t_id].hover_id_num, pdata->coord[t_id].freq_id);
#endif
	} else if (action == SEC_TS_COORDINATE_ACTION_RELEASE || action == SEC_TS_COORDINATE_ACTION_FORCE_RELEASE) {
#if !IS_ENABLED(CONFIG_SAMSUNG_PRODUCT_SHIP)
		input_info(true, dev,
				"[R%s] tID:%d loc:%s dd:%d,%d mc:%d tc:%d lx:%d ly:%d p:%d noise:(%x,%d%d) nlvl:%d, maxS:%d, hid:%d, fid:%d\n",
				action == SEC_TS_COORDINATE_ACTION_FORCE_RELEASE ? "A" : "",
				t_id, pdata->location,
				pdata->coord[t_id].x - pdata->coord[t_id].p_x,
				pdata->coord[t_id].y - pdata->coord[t_id].p_y,
				pdata->coord[t_id].mcount, pdata->touch_count,
				pdata->coord[t_id].x, pdata->coord[t_id].y,
				pdata->coord[t_id].palm_count,
				pdata->coord[t_id].noise_status, pdata->touch_noise_status,
				pdata->touch_pre_noise_status, pdata->coord[t_id].noise_level,
				pdata->coord[t_id].max_strength, pdata->coord[t_id].hover_id_num,
				pdata->coord[t_id].freq_id);
#else
		input_info(true, dev,
				"[R%s] tID:%d loc:%s dd:%d,%d mc:%d tc:%d p:%d noise:(%x,%d%d) nlvl:%d, maxS:%d, hid:%d, fid:%d\n",
				action == SEC_TS_COORDINATE_ACTION_FORCE_RELEASE ? "A" : "",
				t_id, pdata->location,
				pdata->coord[t_id].x - pdata->coord[t_id].p_x,
				pdata->coord[t_id].y - pdata->coord[t_id].p_y,
				pdata->coord[t_id].mcount, pdata->touch_count,
				pdata->coord[t_id].palm_count,
				pdata->coord[t_id].noise_status, pdata->touch_noise_status,
				pdata->touch_pre_noise_status, pdata->coord[t_id].noise_level,
				pdata->coord[t_id].max_strength, pdata->coord[t_id].hover_id_num,
				pdata->coord[t_id].freq_id);
#endif
	}
}

void sec_input_coord_event_fill_slot(struct device *dev, int t_id)
{
	struct sec_ts_plat_data *pdata = dev->platform_data;

	if (pdata->coord[t_id].action == SEC_TS_COORDINATE_ACTION_RELEASE) {
		if (pdata->prev_coord[t_id].action == SEC_TS_COORDINATE_ACTION_NONE
				|| pdata->prev_coord[t_id].action == SEC_TS_COORDINATE_ACTION_RELEASE) {
			input_err(true, dev,
					"%s: tID %d released without press\n", __func__, t_id);
			return;
		}

		sec_input_coord_report(dev, t_id);
		if ((pdata->touch_count == 0) && !IS_ERR_OR_NULL(&pdata->interrupt_notify_work.work)) {
			if (pdata->interrupt_notify_work.work.func && list_empty(&pdata->interrupt_notify_work.work.entry))
				schedule_work(&pdata->interrupt_notify_work.work);
		}
		sec_input_coord_log(dev, t_id, SEC_TS_COORDINATE_ACTION_RELEASE);

		pdata->coord[t_id].action = SEC_TS_COORDINATE_ACTION_NONE;
		pdata->coord[t_id].mcount = 0;
		pdata->coord[t_id].palm_count = 0;
		pdata->coord[t_id].noise_level = 0;
		pdata->coord[t_id].max_strength = 0;
		pdata->coord[t_id].hover_id_num = 0;
	} else if (pdata->coord[t_id].action == SEC_TS_COORDINATE_ACTION_PRESS) {
		sec_input_coord_report(dev, t_id);
		if ((pdata->touch_count == 1) && !IS_ERR_OR_NULL(&pdata->interrupt_notify_work.work)) {
			if (pdata->interrupt_notify_work.work.func && list_empty(&pdata->interrupt_notify_work.work.entry))
				schedule_work(&pdata->interrupt_notify_work.work);
		}
		sec_input_coord_log(dev, t_id, SEC_TS_COORDINATE_ACTION_PRESS);
	} else if (pdata->coord[t_id].action == SEC_TS_COORDINATE_ACTION_MOVE) {
		if (pdata->prev_coord[t_id].action == SEC_TS_COORDINATE_ACTION_NONE
				|| pdata->prev_coord[t_id].action == SEC_TS_COORDINATE_ACTION_RELEASE) {
			pdata->coord[t_id].action = SEC_TS_COORDINATE_ACTION_PRESS;
			sec_input_coord_report(dev, t_id);
			sec_input_coord_log(dev, t_id, SEC_TS_COORDINATE_ACTION_MOVE);
		} else {
			sec_input_coord_report(dev, t_id);
		}
	} else {
		input_dbg(true, dev,
				"%s: do not support coordinate action(%d)\n",
				__func__, pdata->coord[t_id].action);
	}

	if ((pdata->coord[t_id].action == SEC_TS_COORDINATE_ACTION_PRESS)
			|| (pdata->coord[t_id].action == SEC_TS_COORDINATE_ACTION_MOVE)) {
		if (pdata->coord[t_id].ttype != pdata->prev_coord[t_id].ttype) {
			input_info(true, dev, "%s : tID:%d ttype(%x->%x)\n",
					__func__, pdata->coord[t_id].id,
					pdata->prev_coord[t_id].ttype, pdata->coord[t_id].ttype);
		}
	}

	pdata->fill_slot = true;

}
EXPORT_SYMBOL(sec_input_coord_event_fill_slot);

void sec_input_coord_event_sync_slot(struct device *dev)
{
	struct sec_ts_plat_data *pdata = dev->platform_data;

	if (pdata->fill_slot)
		input_sync(pdata->input_dev);
	pdata->fill_slot = false;
}
EXPORT_SYMBOL(sec_input_coord_event_sync_slot);

void sec_input_release_all_finger(struct device *dev)
{
	struct sec_ts_plat_data *pdata = dev->platform_data;
	int i;

	if (!pdata->input_dev)
		return;

	if (pdata->prox_power_off) {
		input_report_key(pdata->input_dev, KEY_INT_CANCEL, 1);
		input_sync(pdata->input_dev);
		input_report_key(pdata->input_dev, KEY_INT_CANCEL, 0);
		input_sync(pdata->input_dev);
	}

	if (pdata->input_dev_proximity) {
		input_report_abs(pdata->input_dev_proximity, ABS_MT_CUSTOM, 0xff);
		input_sync(pdata->input_dev_proximity);
	}

	for (i = 0; i < SEC_TS_SUPPORT_TOUCH_COUNT; i++) {
		input_mt_slot(pdata->input_dev, i);
		input_mt_report_slot_state(pdata->input_dev, MT_TOOL_FINGER, false);

		if (pdata->coord[i].action == SEC_TS_COORDINATE_ACTION_PRESS
				|| pdata->coord[i].action == SEC_TS_COORDINATE_ACTION_MOVE) {
			sec_input_coord_log(dev, i, SEC_TS_COORDINATE_ACTION_FORCE_RELEASE);
			pdata->coord[i].action = SEC_TS_COORDINATE_ACTION_RELEASE;
		}

		pdata->coord[i].mcount = 0;
		pdata->coord[i].palm_count = 0;
		pdata->coord[i].noise_level = 0;
		pdata->coord[i].max_strength = 0;
		pdata->coord[i].hover_id_num = 0;
	}

	input_mt_slot(pdata->input_dev, 0);

	input_report_key(pdata->input_dev, BTN_PALM, false);
	input_report_key(pdata->input_dev, BTN_LARGE_PALM, false);
	input_report_key(pdata->input_dev, BTN_TOUCH, false);
	input_report_key(pdata->input_dev, BTN_TOOL_FINGER, false);
	pdata->palm_flag = 0;
	pdata->touch_count = 0;
	pdata->hw_param.check_multi = 0;

	input_sync(pdata->input_dev);
}
EXPORT_SYMBOL(sec_input_release_all_finger);

static void sec_input_set_prop(struct device *dev, struct input_dev *input_dev, u8 propbit, void *data)
{
	struct sec_ts_plat_data *pdata = dev->platform_data;
	static char sec_input_phys[64] = { 0 };

	snprintf(sec_input_phys, sizeof(sec_input_phys), "%s", input_dev->name);
	input_dev->phys = sec_input_phys;
	input_dev->id.bustype = BUS_I2C;
	input_dev->dev.parent = dev;

	set_bit(EV_SYN, input_dev->evbit);
	set_bit(EV_KEY, input_dev->evbit);
	set_bit(EV_ABS, input_dev->evbit);
	set_bit(EV_SW, input_dev->evbit);
	set_bit(BTN_TOUCH, input_dev->keybit);
	set_bit(BTN_TOOL_FINGER, input_dev->keybit);
	set_bit(BTN_PALM, input_dev->keybit);
	set_bit(BTN_LARGE_PALM, input_dev->keybit);
	set_bit(KEY_BLACK_UI_GESTURE, input_dev->keybit);
	set_bit(KEY_INT_CANCEL, input_dev->keybit);

	set_bit(propbit, input_dev->propbit);
	set_bit(KEY_WAKEUP, input_dev->keybit);
	set_bit(KEY_WATCH, input_dev->keybit);

	input_set_abs_params(input_dev, ABS_MT_POSITION_X, 0, pdata->max_x, 0, 0);
	input_set_abs_params(input_dev, ABS_MT_POSITION_Y, 0, pdata->max_y, 0, 0);
	input_set_abs_params(input_dev, ABS_MT_TOUCH_MAJOR, 0, 255, 0, 0);
	input_set_abs_params(input_dev, ABS_MT_TOUCH_MINOR, 0, 255, 0, 0);
	if (pdata->support_mt_pressure)
		input_set_abs_params(input_dev, ABS_MT_PRESSURE, 0, 255, 0, 0);

	if (propbit == INPUT_PROP_POINTER)
		input_mt_init_slots(input_dev, SEC_TS_SUPPORT_TOUCH_COUNT, INPUT_MT_POINTER);
	else
		input_mt_init_slots(input_dev, SEC_TS_SUPPORT_TOUCH_COUNT, INPUT_MT_DIRECT);

	input_set_drvdata(input_dev, data);
}

#if IS_ENABLED(CONFIG_TOUCHSCREEN_DUAL_FOLDABLE)
static void sec_input_set_prop2(struct device *dev, struct input_dev *input_dev, u8 propbit, void *data)
{
	struct sec_ts_plat_data *pdata = dev->platform_data;
	static char sec_input_phys[64] = { 0 };

	snprintf(sec_input_phys, sizeof(sec_input_phys), "%s", input_dev->name);
	input_dev->phys = sec_input_phys;
	input_dev->id.bustype = BUS_I2C;
	input_dev->dev.parent = dev;

	set_bit(EV_SYN, input_dev->evbit);
	set_bit(EV_KEY, input_dev->evbit);
	set_bit(EV_ABS, input_dev->evbit);
	set_bit(EV_SW, input_dev->evbit);
	set_bit(BTN_TOUCH, input_dev->keybit);
	set_bit(BTN_TOOL_FINGER, input_dev->keybit);
	set_bit(BTN_PALM, input_dev->keybit);
	set_bit(BTN_LARGE_PALM, input_dev->keybit);
	set_bit(KEY_BLACK_UI_GESTURE, input_dev->keybit);
	set_bit(KEY_INT_CANCEL, input_dev->keybit);

	set_bit(propbit, input_dev->propbit);
	set_bit(KEY_WAKEUP, input_dev->keybit);
	set_bit(KEY_WATCH, input_dev->keybit);

	input_set_abs_params(input_dev, ABS_MT_POSITION_X, 0, pdata->max_x, 0, 0);
	input_set_abs_params(input_dev, ABS_MT_POSITION_Y, 0, pdata->max_y, 0, 0);
	input_set_abs_params(input_dev, ABS_MT_TOUCH_MAJOR, 0, 255, 0, 0);
	input_set_abs_params(input_dev, ABS_MT_TOUCH_MINOR, 0, 255, 0, 0);
	if (pdata->support_mt_pressure)
		input_set_abs_params(input_dev, ABS_MT_PRESSURE, 0, 255, 0, 0);

	if (propbit == INPUT_PROP_POINTER)
		input_mt_init_slots(input_dev, SEC_TS_SUPPORT_TOUCH_COUNT, INPUT_MT_POINTER);
	else
		input_mt_init_slots(input_dev, SEC_TS_SUPPORT_TOUCH_COUNT, INPUT_MT_DIRECT);

	input_set_drvdata(input_dev, data);
}
#endif

static void sec_input_set_prop_pad(struct device *dev, struct input_dev *input_dev, u8 propbit, void *data)
{
	struct sec_ts_plat_data *pdata = dev->platform_data;
	static char sec_input_phys[64] = { 0 };

	snprintf(sec_input_phys, sizeof(sec_input_phys), "%s", input_dev->name);
	input_dev->phys = sec_input_phys;
	input_dev->id.bustype = BUS_I2C;
	input_dev->dev.parent = dev;

	set_bit(EV_SYN, input_dev->evbit);
	set_bit(EV_KEY, input_dev->evbit);
	set_bit(EV_ABS, input_dev->evbit);
	set_bit(EV_SW, input_dev->evbit);
	set_bit(BTN_TOUCH, input_dev->keybit);
	set_bit(BTN_TOOL_FINGER, input_dev->keybit);
	set_bit(BTN_PALM, input_dev->keybit);
	set_bit(BTN_LARGE_PALM, input_dev->keybit);
	set_bit(KEY_BLACK_UI_GESTURE, input_dev->keybit);
	set_bit(KEY_INT_CANCEL, input_dev->keybit);

	set_bit(propbit, input_dev->propbit);
	set_bit(KEY_WAKEUP, input_dev->keybit);
	set_bit(KEY_WATCH, input_dev->keybit);

	input_set_abs_params(input_dev, ABS_MT_POSITION_X, 0, pdata->max_x, 0, 0);
	input_set_abs_params(input_dev, ABS_MT_POSITION_Y, 0, pdata->max_y, 0, 0);
	input_set_abs_params(input_dev, ABS_MT_TOUCH_MAJOR, 0, 255, 0, 0);
	input_set_abs_params(input_dev, ABS_MT_TOUCH_MINOR, 0, 255, 0, 0);
	if (pdata->support_mt_pressure)
		input_set_abs_params(input_dev, ABS_MT_PRESSURE, 0, 255, 0, 0);

	if (propbit == INPUT_PROP_POINTER)
		input_mt_init_slots(input_dev, SEC_TS_SUPPORT_TOUCH_COUNT, INPUT_MT_POINTER);
	else
		input_mt_init_slots(input_dev, SEC_TS_SUPPORT_TOUCH_COUNT, INPUT_MT_DIRECT);

	input_set_drvdata(input_dev, data);
}

static void sec_input_set_prop_proximity(struct device *dev, struct input_dev *input_dev, void *data)
{
	static char sec_input_phys[64] = { 0 };

	snprintf(sec_input_phys, sizeof(sec_input_phys), "%s/input1", input_dev->name);
	input_dev->phys = sec_input_phys;
	input_dev->id.bustype = BUS_I2C;
	input_dev->dev.parent = dev;

	set_bit(EV_SYN, input_dev->evbit);
	set_bit(EV_SW, input_dev->evbit);

	set_bit(INPUT_PROP_DIRECT, input_dev->propbit);

	input_set_abs_params(input_dev, ABS_MT_CUSTOM, 0, 0xFFFFFFFF, 0, 0);
	input_set_drvdata(input_dev, data);
}

int sec_input_device_register(struct device *dev, void *data)
{
	struct sec_ts_plat_data *pdata = dev->platform_data;
	int ret = 0;

	/* register input_dev */
	pdata->input_dev = devm_input_allocate_device(dev);
	if (!pdata->input_dev) {
		input_err(true, dev, "%s: allocate input_dev err!\n", __func__);
		return -ENOMEM;
	}

#if IS_ENABLED(CONFIG_TOUCHSCREEN_DUAL_FOLDABLE)
	if (pdata->support_dual_foldable == SUB_TOUCH) {
		pdata->input_dev->name = "sec_touchscreen2";
		sec_input_set_prop2(dev, pdata->input_dev, INPUT_PROP_DIRECT, data);
	} else {
		pdata->input_dev->name = "sec_touchscreen";
		sec_input_set_prop(dev, pdata->input_dev, INPUT_PROP_DIRECT, data);
	}
#else
	pdata->input_dev->name = "sec_touchscreen";
	sec_input_set_prop(dev, pdata->input_dev, INPUT_PROP_DIRECT, data);
#endif
	ret = input_register_device(pdata->input_dev);
	if (ret) {
		input_err(true, dev, "%s: Unable to register %s input device\n",
				__func__, pdata->input_dev->name);
		return ret;
	}

	if (pdata->support_dex) {
		/* register input_dev_pad */
		pdata->input_dev_pad = devm_input_allocate_device(dev);
		if (!pdata->input_dev_pad) {
			input_err(true, dev, "%s: allocate input_dev_pad err!\n", __func__);
			return -ENOMEM;
		}

		pdata->input_dev_pad->name = "sec_touchpad";
		sec_input_set_prop_pad(dev, pdata->input_dev_pad, INPUT_PROP_POINTER, data);
		ret = input_register_device(pdata->input_dev_pad);
		if (ret) {
			input_err(true, dev, "%s: Unable to register %s input device\n",
					__func__, pdata->input_dev_pad->name);
			return ret;
		}
	}

	if (pdata->support_ear_detect || pdata->support_lightsensor_detect) {
		/* register input_dev_proximity */
		pdata->input_dev_proximity = devm_input_allocate_device(dev);
		if (!pdata->input_dev_proximity) {
			input_err(true, dev, "%s: allocate input_dev_proximity err!\n", __func__);
			return -ENOMEM;
		}

#if IS_ENABLED(CONFIG_TOUCHSCREEN_DUAL_FOLDABLE)
		if (pdata->support_dual_foldable == SUB_TOUCH)
			pdata->input_dev_proximity->name = "sec_touchproximity2";
		else 
#endif
		pdata->input_dev_proximity->name = "sec_touchproximity";
		sec_input_set_prop_proximity(dev, pdata->input_dev_proximity, data);
		ret = input_register_device(pdata->input_dev_proximity);
		if (ret) {
			input_err(true, dev, "%s: Unable to register %s input device\n",
					__func__, pdata->input_dev_proximity->name);
			return ret;
		}
	}

	return 0;
}
EXPORT_SYMBOL(sec_input_device_register);

int sec_input_pinctrl_configure(struct device *dev, bool on)
{
	struct sec_ts_plat_data *pdata = dev->platform_data;
	struct pinctrl_state *state;

	input_info(true, dev, "%s: %s\n", __func__, on ? "ACTIVE" : "SUSPEND");

	if (on) {
		state = pinctrl_lookup_state(pdata->pinctrl, "on_state");
		if (IS_ERR(pdata->pinctrl))
			input_err(true, dev, "%s: could not get active pinstate\n", __func__);
	} else {
		state = pinctrl_lookup_state(pdata->pinctrl, "off_state");
		if (IS_ERR(pdata->pinctrl))
			input_err(true, dev, "%s: could not get suspend pinstate\n", __func__);
	}

	if (!IS_ERR_OR_NULL(state))
		return pinctrl_select_state(pdata->pinctrl, state);

	return 0;
}
EXPORT_SYMBOL(sec_input_pinctrl_configure);

int sec_input_power(struct device *dev, bool on)
{
	struct sec_ts_plat_data *pdata = dev->platform_data;
	int ret = 0;

	if (pdata->power_enabled == on) {
		input_info(true, dev, "%s: power_enabled %d\n", __func__, pdata->power_enabled);
		return ret;
	}

	if (on) {
		if (!pdata->not_support_io_ldo) {
			ret = regulator_enable(pdata->dvdd);
			if (ret) {
				input_err(true, dev, "%s: Failed to enable dvdd: %d\n", __func__, ret);
				goto out;
			}

			sec_delay(1);
		}
		ret = regulator_enable(pdata->avdd);
		if (ret) {
			input_err(true, dev, "%s: Failed to enable avdd: %d\n", __func__, ret);
			goto out;
		}
	} else {
		regulator_disable(pdata->avdd);
		if (!pdata->not_support_io_ldo) {
			sec_delay(4);
			regulator_disable(pdata->dvdd);
		}
	}

	pdata->power_enabled = on;

out:
	if (!pdata->not_support_io_ldo) {
		input_info(true, dev, "%s: %s: avdd:%s, dvdd:%s\n", __func__, on ? "on" : "off",
				regulator_is_enabled(pdata->avdd) ? "on" : "off",
				regulator_is_enabled(pdata->dvdd) ? "on" : "off");
	} else {
		input_info(true, dev, "%s: %s: avdd:%s\n", __func__, on ? "on" : "off",
				regulator_is_enabled(pdata->avdd) ? "on" : "off");
	}

	return ret;
}
EXPORT_SYMBOL(sec_input_power);

#if IS_ENABLED(CONFIG_VBUS_NOTIFIER)
#if IS_ENABLED(CONFIG_USB_TYPEC_MANAGER_NOTIFIER)
static int sec_input_ccic_notification(struct notifier_block *nb,
	   unsigned long action, void *data)
{
	struct sec_ts_plat_data *pdata = container_of(nb, struct sec_ts_plat_data, ccic_nb);
	PD_NOTI_USB_STATUS_TYPEDEF usb_status = *(PD_NOTI_USB_STATUS_TYPEDEF *)data;

	if (pdata->dev == NULL) {
		pr_err("%s %s: dev is null\n", SECLOG, __func__);
		return 0;
	}

	if (usb_status.dest != PDIC_NOTIFY_DEV_USB)
		return 0;

	switch (usb_status.drp) {
	case USB_STATUS_NOTIFY_ATTACH_DFP:
		pdata->otg_flag = true;
		input_info(true, pdata->dev, "%s: otg_flag %d\n", __func__, pdata->otg_flag);
		break;
	case USB_STATUS_NOTIFY_DETACH:
		pdata->otg_flag = false;
		input_info(true, pdata->dev, "%s: otg_flag %d\n", __func__, pdata->otg_flag);
		break;
	default:
		break;
	}
	return 0;
}
#endif
static int sec_input_vbus_notification(struct notifier_block *nb,
		unsigned long cmd, void *data)
{
	struct sec_ts_plat_data *pdata = container_of(nb, struct sec_ts_plat_data, vbus_nb);
	vbus_status_t vbus_type = *(vbus_status_t *) data;

	if (pdata->dev == NULL) {
		pr_err("%s %s: dev is null\n", SECLOG, __func__);
		return 0;
	}

	input_info(true, pdata->dev, "%s: cmd=%lu, vbus_type=%d, otg_flag:%d\n",
			__func__, cmd, vbus_type, pdata->otg_flag);

	if (pdata->shutdown_called)
		return 0;

	switch (vbus_type) {
	case STATUS_VBUS_HIGH:
		if (!pdata->otg_flag)
			pdata->charger_flag = true;
		else
			return 0;
		break;
	case STATUS_VBUS_LOW:
		pdata->charger_flag = false;
		break;
	default:
		return 0;
	}

	queue_work(pdata->vbus_notifier_workqueue, &pdata->vbus_notifier_work);

	return 0;
}

static void sec_input_vbus_notification_work(struct work_struct *work)
{
	struct sec_ts_plat_data *pdata = container_of(work, struct sec_ts_plat_data, vbus_notifier_work);
	int ret = 0;

	if (pdata->dev == NULL) {
		pr_err("%s %s: dev is null\n", SECLOG, __func__);
		return;
	}

	if (pdata->set_charger_mode == NULL) {
		input_err(true, pdata->dev, "%s: set_charger_mode function is not allocated\n", __func__);
		return;
	}

	if (pdata->shutdown_called)
		return;

	input_info(true, pdata->dev, "%s: charger_flag:%d\n", __func__, pdata->charger_flag);

	ret = pdata->set_charger_mode(pdata->dev, pdata->charger_flag);
	if (ret < 0) {
		input_info(true, pdata->dev, "%s: failed to set charger_flag\n", __func__);
		return;
	}
}
#endif

void sec_input_register_vbus_notifier(struct device *dev)
{
	struct sec_ts_plat_data *pdata = dev->platform_data;

	if (!pdata->support_vbus_notifier)
		return;

	input_info(true, dev, "%s\n", __func__);

	pdata->otg_flag = false;
	pdata->charger_flag = false;

#if IS_ENABLED(CONFIG_VBUS_NOTIFIER)
#if IS_ENABLED(CONFIG_USB_TYPEC_MANAGER_NOTIFIER)
	manager_notifier_register(&pdata->ccic_nb, sec_input_ccic_notification,
		MANAGER_NOTIFY_PDIC_INITIAL);
	input_info(true, dev, "%s: register ccic notification\n", __func__);
#endif
	pdata->vbus_notifier_workqueue = create_singlethread_workqueue("sec_input_vbus_noti");
	INIT_WORK(&pdata->vbus_notifier_work, sec_input_vbus_notification_work);
	vbus_notifier_register(&pdata->vbus_nb, sec_input_vbus_notification, VBUS_NOTIFY_DEV_CHARGER);
	input_info(true, dev, "%s: register vbus notification\n", __func__);
#endif

}
EXPORT_SYMBOL(sec_input_register_vbus_notifier);

void sec_input_unregister_vbus_notifier(struct device *dev)
{
	struct sec_ts_plat_data *pdata = dev->platform_data;

	if (!pdata->support_vbus_notifier)
		return;

	input_info(true, dev, "%s\n", __func__);

#if IS_ENABLED(CONFIG_VBUS_NOTIFIER)
#if IS_ENABLED(CONFIG_USB_TYPEC_MANAGER_NOTIFIER)
	manager_notifier_unregister(&pdata->ccic_nb);
#endif
	vbus_notifier_unregister(&pdata->vbus_nb);
	cancel_work_sync(&pdata->vbus_notifier_work);
	flush_workqueue(pdata->vbus_notifier_workqueue);
	destroy_workqueue(pdata->vbus_notifier_workqueue);
#endif
}
EXPORT_SYMBOL(sec_input_unregister_vbus_notifier);

int sec_input_parse_dt(struct device *dev)
{
	struct sec_ts_plat_data *pdata = dev->platform_data;
	struct device_node *np = dev->of_node;
	u32 coords[2];
	int ret = 0;
	int count = 0, i;
	u32 ic_match_value;
	u32 px_zone[3] = { 0 };
	int lcd_type = 0;
	u32 bitmask[2] = { 0 };

	pdata->dev = dev;

	if (of_property_read_u32(np, "sec,support_dual_foldable", &pdata->support_dual_foldable) < 0)
		pdata->support_dual_foldable = 0;

	pdata->chip_on_board = of_property_read_bool(np, "chip_on_board");

	lcd_type = sec_input_get_lcd_id(dev);
#if !defined(DUAL_FOLDABLE_GKI)
	if (lcd_type < 0) {
		input_err(true, dev, "%s: lcd is not attached\n", __func__);
		if (!pdata->chip_on_board)
			return -ENODEV;
	}
#endif
	input_info(true, dev, "%s: lcdtype 0x%08X\n", __func__, lcd_type);

	if (!of_property_read_u32_array(np, "sec,bitmask_unload", bitmask, 2)) {
		if ((lcd_type != 0) && ((lcd_type >> bitmask[0]) == bitmask[1])) {
			input_err(true, dev, "%s: do not load lcdtype:0x%08X bitmask:0x%08X\n", __func__,
						lcd_type >> bitmask[0], bitmask[1]);
			return -ENODEV;
		}
	}

	pdata->tsp_icid = of_get_named_gpio(np, "sec,tsp-icid_gpio", 0);
	if (gpio_is_valid(pdata->tsp_icid)) {
		input_info(true, dev, "%s: TSP_ICID : %d\n", __func__, gpio_get_value(pdata->tsp_icid));
		if (of_property_read_u32(np, "sec,icid_match_value", &ic_match_value)) {
			input_err(true, dev, "%s: not use icid match value\n", __func__);
			return -EINVAL;
		}

		pdata->tspicid_val = gpio_get_value(pdata->tsp_icid);
		if (pdata->tspicid_val != ic_match_value) {
			input_err(true, dev, "%s: Do not match TSP_ICID\n", __func__);
			return -EINVAL;
		}
	} else {
		input_err(true, dev, "%s: not use tsp-icid gpio\n", __func__);
	}

	pdata->irq_gpio = of_get_named_gpio(np, "sec,irq_gpio", 0);
	if (gpio_is_valid(pdata->irq_gpio)) {
		ret = devm_gpio_request_one(dev, pdata->irq_gpio, GPIOF_DIR_IN, "sec,tsp_int");
		if (ret) {
			input_err(true, dev, "%s: Unable to request tsp_int [%d]\n", __func__, pdata->irq_gpio);
			return -EINVAL;
		}
	} else {
		input_err(true, dev, "%s: Failed to get irq gpio\n", __func__);
		return -EINVAL;
	}

	if (of_property_read_u32(np, "sec,irq_flag", &pdata->irq_flag))
		pdata->irq_flag = IRQF_TRIGGER_LOW | IRQF_ONESHOT;

	input_dbg(true, dev, "%s: irq_flag property 0x%X\n", __func__, pdata->irq_flag);

	pdata->gpio_spi_cs = of_get_named_gpio(np, "sec,gpio_spi_cs", 0);
	if (gpio_is_valid(pdata->gpio_spi_cs)) {
		ret = gpio_request(pdata->gpio_spi_cs, "tsp,gpio_spi_cs");
		input_info(true, dev, "%s: gpio_spi_cs: %d, ret: %d\n", __func__, pdata->gpio_spi_cs, ret);
		ret = 0;
	}

	if (of_property_read_u32(np, "sec,i2c-burstmax", &pdata->i2c_burstmax)) {
		input_dbg(false, dev, "%s: Failed to get i2c_burstmax property\n", __func__);
		pdata->i2c_burstmax = 0xffff;
	}

	if (of_property_read_u32_array(np, "sec,max_coords", coords, 2)) {
		input_err(true, dev, "%s: Failed to get max_coords property\n", __func__);
		return -EINVAL;
	}
	pdata->max_x = coords[0] - 1;
	pdata->max_y = coords[1] - 1;

	if (of_property_read_u32(np, "sec,bringup", &pdata->bringup) < 0)
		pdata->bringup = 0;

	pdata->tsp_id = of_get_named_gpio(np, "sec,tsp-id_gpio", 0);
	if (gpio_is_valid(pdata->tsp_id)) {
		pdata->tspid_val = gpio_get_value(pdata->tsp_id);
		input_info(true, dev, "%s: TSP_ID : %d\n", __func__, pdata->tspid_val);
	} else {
		input_err(true, dev, "%s: not use tsp-id gpio\n", __func__);
	}

	count = of_property_count_strings(np, "sec,firmware_name");
	if (count <= 0) {
		pdata->firmware_name = NULL;
	} else {
		if (gpio_is_valid(pdata->tsp_id) && pdata->tspid_val) {
			of_property_read_string_index(np, "sec,firmware_name",
							pdata->tspid_val, &pdata->firmware_name);
			if (pdata->bringup == 4)
				pdata->bringup = 2;
		} else {
			u8 lcd_id_num = of_property_count_u32_elems(np, "sec,select_lcdid");

			if ((lcd_id_num != count) || (lcd_id_num <= 0)) {
				of_property_read_string_index(np, "sec,firmware_name", 0, &pdata->firmware_name);
			} else {
				u32 *lcd_id_t;
				u32 lcd_id_mask;

				lcd_id_t = kcalloc(lcd_id_num, sizeof(u32), GFP_KERNEL);
				if  (!lcd_id_t)
					return -ENOMEM;

				of_property_read_u32_array(np, "sec,select_lcdid", lcd_id_t, lcd_id_num);
				if (of_property_read_u32(np, "sec,lcdid_mask", &lcd_id_mask) < 0)
					lcd_id_mask = 0xFFFFFF;
				else
					input_info(true, dev, "%s: lcd_id_mask: 0x%06X\n", __func__, lcd_id_mask);

				for (i = 0; i < lcd_id_num; i++) {
					if (((lcd_id_t[i] & lcd_id_mask) == (lcd_type & lcd_id_mask)) || (i == (lcd_id_num - 1))) {
						of_property_read_string_index(np, "sec,firmware_name", i, &pdata->firmware_name);
						break;
					}
				}
				if (!pdata->firmware_name)
					pdata->bringup = 1;
				else if (strlen(pdata->firmware_name) == 0)
					pdata->bringup = 1;

				input_info(true, dev, "%s: count: %d, index:%d, lcd_id: 0x%X, firmware: %s\n",
							__func__, count, i, lcd_id_t[i], pdata->firmware_name);
				kfree(lcd_id_t);
			}

			if (pdata->bringup == 4)
				pdata->bringup = 3;
		}
	}

	pdata->not_support_vdd = of_property_read_bool(np, "not_support_vdd");
	if (!pdata->not_support_vdd) {
		pdata->not_support_io_ldo = of_property_read_bool(np, "not_support_io_ldo");
		if (!pdata->not_support_io_ldo) {
			pdata->dvdd = regulator_get(dev, "tsp_io_ldo");
			if (IS_ERR_OR_NULL(pdata->dvdd)) {
				input_err(true, dev, "%s: Failed to get %s regulator.\n",
						__func__, "tsp_io_ldo");
				ret = PTR_ERR(pdata->dvdd);
#if !IS_ENABLED(CONFIG_QGKI)
				if (gpio_is_valid(pdata->gpio_spi_cs))
					gpio_free(pdata->gpio_spi_cs);
#endif
				return -EINVAL;
			}
		}
 
		pdata->avdd = regulator_get(dev, "tsp_avdd_ldo");
		if (IS_ERR_OR_NULL(pdata->avdd)) {
			input_err(true, dev, "%s: Failed to get %s regulator.\n",
					__func__, "tsp_avdd_ldo");
			ret = PTR_ERR(pdata->avdd);
#if !IS_ENABLED(CONFIG_QGKI)
			if (gpio_is_valid(pdata->gpio_spi_cs))
				gpio_free(pdata->gpio_spi_cs);
#endif
			return -EINVAL;
		}
	}

	pdata->regulator_boot_on = of_property_read_bool(np, "sec,regulator_boot_on");
	pdata->support_dex = of_property_read_bool(np, "support_dex_mode");
	pdata->support_fod = of_property_read_bool(np, "support_fod");
	pdata->support_fod_lp_mode = of_property_read_bool(np, "support_fod_lp_mode");
	pdata->enable_settings_aot = of_property_read_bool(np, "enable_settings_aot");
	pdata->sync_reportrate_120 = of_property_read_bool(np, "sync-reportrate-120");
	pdata->support_refresh_rate_mode = of_property_read_bool(np, "support_refresh_rate_mode");
	pdata->support_vrr = of_property_read_bool(np, "support_vrr");
	pdata->support_ear_detect = of_property_read_bool(np, "support_ear_detect_mode");
	pdata->support_open_short_test = of_property_read_bool(np, "support_open_short_test");
	pdata->support_mis_calibration_test = of_property_read_bool(np, "support_mis_calibration_test");
	pdata->support_wireless_tx = of_property_read_bool(np, "support_wireless_tx");
	pdata->support_input_monitor = of_property_read_bool(np, "support_input_monitor");
	pdata->disable_vsync_scan = of_property_read_bool(np, "disable_vsync_scan");
	pdata->unuse_dvdd_power = of_property_read_bool(np, "sec,unuse_dvdd_power");
	pdata->sense_off_when_cover_closed = of_property_read_bool(np, "sense_off_when_cover_closed");
	pdata->not_support_temp_noti = of_property_read_bool(np, "not_support_temp_noti");
	pdata->support_vbus_notifier = of_property_read_bool(np, "support_vbus_notifier");

	of_property_read_u32(np, "support_rawdata_map_num", &pdata->support_rawdata_map_num);

	if (of_property_read_u32(np, "sec,support_sensor_hall", &pdata->support_sensor_hall) < 0)
		pdata->support_sensor_hall = 0;
	if (of_property_read_u32(np, "sec,dump_ic_ver", &pdata->dump_ic_ver) < 0)
		pdata->dump_ic_ver = 0;
	pdata->support_flex_mode = of_property_read_bool(np, "support_flex_mode");
	pdata->support_lightsensor_detect = of_property_read_bool(np, "support_lightsensor_detect");

	pdata->enable_sysinput_enabled = of_property_read_bool(np, "sec,enable_sysinput_enabled");
	input_info(true, dev, "%s: Sysinput enabled %s\n",
				__func__, pdata->enable_sysinput_enabled ? "ON" : "OFF");

	pdata->support_rawdata_motion_aivf = of_property_read_bool(np, "sec,support_rawdata_motion_aivf");
	input_info(true, dev, "%s: motion aivf %s\n",
				__func__, pdata->support_rawdata_motion_aivf ? "ON" : "OFF");
	pdata->support_rawdata_motion_palm = of_property_read_bool(np, "sec,support_rawdata_motion_palm");
	input_info(true, dev, "%s: motion palm %s\n",
				__func__, pdata->support_rawdata_motion_palm ? "ON" : "OFF");

	if (of_property_read_u32_array(np, "sec,area-size", px_zone, 3)) {
		input_info(true, dev, "Failed to get zone's size\n");
		pdata->area_indicator = 48;
		pdata->area_navigation = 96;
		pdata->area_edge = 60;
	} else {
		pdata->area_indicator = px_zone[0];
		pdata->area_navigation = px_zone[1];
		pdata->area_edge = px_zone[2];
	}
	input_info(true, dev, "%s : zone's size - indicator:%d, navigation:%d, edge:%d\n",
		__func__, pdata->area_indicator, pdata->area_navigation ,pdata->area_edge);

#if IS_ENABLED(CONFIG_INPUT_SEC_SECURE_TOUCH)
	of_property_read_u32(np, "sec,ss_touch_num", &pdata->ss_touch_num);
	input_err(true, dev, "%s: ss_touch_num:%d\n", __func__, pdata->ss_touch_num);
#endif
#if IS_ENABLED(CONFIG_SEC_FACTORY)
	pdata->support_mt_pressure = true;
#endif
	input_info(true, dev, "%s: i2c buffer limit: %d, lcd_id:%06X, bringup:%d,"
			" id:%d,%d, dex:%d, max(%d/%d), FOD:%d, AOT:%d, ED:%d, FLM:%d,"
			" COB:%d, disable_vsync_scan:%d, unuse_dvdd_power:%d,"
			" not_support_temp_noti:%d, support_vbus_notifier:%d\n",
			__func__, pdata->i2c_burstmax, lcd_type, pdata->bringup,
			pdata->tsp_id, pdata->tsp_icid,
			pdata->support_dex, pdata->max_x, pdata->max_y,
			pdata->support_fod, pdata->enable_settings_aot,
			pdata->support_ear_detect, pdata->support_fod_lp_mode,
			pdata->chip_on_board, pdata->disable_vsync_scan, pdata->unuse_dvdd_power,
			pdata->not_support_temp_noti, pdata->support_vbus_notifier);

	pdata->irq_workqueue = create_singlethread_workqueue("sec_input_irq_wq");
	if (!IS_ERR_OR_NULL(pdata->irq_workqueue)) {
		INIT_WORK(&pdata->irq_work, sec_input_handler_wait_resume_work);
		input_info(true, dev, "%s: set sec_input_handler_wait_resume_work\n", __func__);
	} else {
		input_err(true, dev, "%s: failed to create irq_workqueue, err: %ld\n",
				__func__, PTR_ERR(pdata->irq_workqueue));
	}

	return ret;
}
EXPORT_SYMBOL(sec_input_parse_dt);

void sec_tclm_parse_dt(struct device *dev, struct sec_tclm_data *tdata)
{
	struct device_node *np = dev->of_node;

	if (of_property_read_u32(np, "sec,tclm_level", &tdata->tclm_level) < 0) {
		tdata->tclm_level = 0;
		input_err(true, dev, "%s: Failed to get tclm_level property\n", __func__);
	}

	if (of_property_read_u32(np, "sec,afe_base", &tdata->afe_base) < 0) {
		tdata->afe_base = 0;
		input_err(true, dev, "%s: Failed to get afe_base property\n", __func__);
	}

	tdata->support_tclm_test = of_property_read_bool(np, "support_tclm_test");

	input_err(true, dev, "%s: tclm_level %d, sec_afe_base %04X\n", __func__, tdata->tclm_level, tdata->afe_base);
}
EXPORT_SYMBOL(sec_tclm_parse_dt);

void sec_tclm_parse_dt_dev(struct device *dev, struct sec_tclm_data *tdata)
{
	struct device_node *np = dev->of_node;

	if (of_property_read_u32(np, "sec,tclm_level", &tdata->tclm_level) < 0) {
		tdata->tclm_level = 0;
		input_err(true, dev, "%s: Failed to get tclm_level property\n", __func__);
	}

	if (of_property_read_u32(np, "sec,afe_base", &tdata->afe_base) < 0) {
		tdata->afe_base = 0;
		input_err(true, dev, "%s: Failed to get afe_base property\n", __func__);
	}

	tdata->support_tclm_test = of_property_read_bool(np, "support_tclm_test");

	input_err(true, dev, "%s: tclm_level %d, sec_afe_base %04X\n", __func__, tdata->tclm_level, tdata->afe_base);
}
EXPORT_SYMBOL(sec_tclm_parse_dt_dev);

void stui_tsp_init(int (*stui_tsp_enter)(void), int (*stui_tsp_exit)(void), int (*stui_tsp_type)(void))
{
	pr_info("%s %s: called\n", SECLOG, __func__);

	psecuretsp = kzalloc(sizeof(struct sec_ts_secure_data), GFP_KERNEL);

	psecuretsp->stui_tsp_enter = stui_tsp_enter;
	psecuretsp->stui_tsp_exit = stui_tsp_exit;
	psecuretsp->stui_tsp_type = stui_tsp_type;
}
EXPORT_SYMBOL(stui_tsp_init);


int stui_tsp_enter(void)
{
	struct sec_ts_plat_data *pdata = NULL;

	if (psecuretsp != NULL) {
		pr_info("%s %s: psecuretsp->stui_tsp_enter called!\n", SECLOG, __func__);
		return psecuretsp->stui_tsp_enter();
	}

	if (ptsp == NULL) {
		pr_info("%s: ptsp is null\n", __func__);
		return -EINVAL;
	}

	pdata = ptsp->platform_data;
	if (pdata == NULL) {
		pr_info("%s: pdata is null\n", __func__);
		return  -EINVAL;
	}

	pr_info("%s %s: pdata->stui_tsp_enter called!\n", SECLOG, __func__);
	return pdata->stui_tsp_enter();
}
EXPORT_SYMBOL(stui_tsp_enter);

int stui_tsp_exit(void)
{
	struct sec_ts_plat_data *pdata = NULL;

	if (psecuretsp != NULL) {
		pr_info("%s %s: psecuretsp->stui_tsp_exit called!\n", SECLOG, __func__);
		return psecuretsp->stui_tsp_exit();
	}

	if (ptsp == NULL)
		return -EINVAL;

	pdata = ptsp->platform_data;
	if (pdata == NULL)
		return  -EINVAL;

	pr_info("%s %s: pdata->stui_tsp_exit called!\n", SECLOG, __func__);
	return pdata->stui_tsp_exit();
}
EXPORT_SYMBOL(stui_tsp_exit);

int stui_tsp_type(void)
{
	struct sec_ts_plat_data *pdata = NULL;

	if (psecuretsp != NULL) {
		pr_info("%s %s: psecuretsp->stui_tsp_type called!\n", SECLOG, __func__);
		return psecuretsp->stui_tsp_type();
	}

	if (ptsp == NULL)
		return -EINVAL;

	pdata = ptsp->platform_data;
	if (pdata == NULL)
		return  -EINVAL;

	pr_info("%s %s: pdata->stui_tsp_type called!\n", SECLOG, __func__);
	return pdata->stui_tsp_type();
}
EXPORT_SYMBOL(stui_tsp_type);

int sec_input_enable_device(struct input_dev *dev)
{
	int retval;

	retval = mutex_lock_interruptible(&dev->mutex);
	if (retval)
		return retval;

	if (dev->users && dev->open)
		retval = dev->open(dev);

	mutex_unlock(&dev->mutex);

	return retval;
}
EXPORT_SYMBOL(sec_input_enable_device);

int sec_input_disable_device(struct input_dev *dev)
{
	int retval;

	retval = mutex_lock_interruptible(&dev->mutex);
	if (retval)
		return retval;

	if (dev->users && dev->close)
		dev->close(dev);

	mutex_unlock(&dev->mutex);
	return 0;
}
EXPORT_SYMBOL(sec_input_disable_device);

__visible_for_testing ssize_t sec_input_enabled_show(struct device *dev,
					 struct device_attribute *attr,
					 char *buf)
{
	struct input_dev *input_dev = to_input_dev(dev);
	struct sec_ts_plat_data *pdata = input_dev->dev.parent->platform_data;

	return scnprintf(buf, PAGE_SIZE, "%d\n", pdata->enabled);
}
#if IS_ENABLED(CONFIG_SEC_KUNIT)
EXPORT_SYMBOL_KUNIT(sec_input_enabled_show);
#endif

__visible_for_testing ssize_t sec_input_enabled_store(struct device *dev,
					struct device_attribute *attr,
					const char *buf, size_t size)
{
	int ret;
	bool enable;
	struct input_dev *input_dev = to_input_dev(dev);
	struct sec_ts_plat_data *pdata = input_dev->dev.parent->platform_data;

	ret = strtobool(buf, &enable);
	if (ret)
		return ret;

	if (pdata->enabled == enable) {
		pr_info("%s %s: device already %s\n", SECLOG, __func__,
				enable ? "enabled" : "disabled");
		goto out;
	}

	if (enable)
		ret = sec_input_enable_device(input_dev);
	else
		ret = sec_input_disable_device(input_dev);

	if (ret)
		return ret;

out:
	return size;
}
#if IS_ENABLED(CONFIG_SEC_KUNIT)
EXPORT_SYMBOL_KUNIT(sec_input_enabled_store);
#endif

static DEVICE_ATTR(enabled, 0664, sec_input_enabled_show, sec_input_enabled_store);

static struct attribute *sec_input_attrs[] = {
	&dev_attr_enabled.attr,
	NULL
};

static const struct attribute_group sec_input_attr_group = {
	.attrs	= sec_input_attrs,
};

int sec_input_sysfs_create(struct kobject *kobj)
{
	struct kernfs_node *enabled_sd = NULL;
	int retval = 0;

	enabled_sd = sysfs_get_dirent(kobj->sd, "enabled");
	if (IS_ERR_OR_NULL(enabled_sd)) {
		pr_info("%s %s: make 'enabled' node\n", SECLOG, __func__);
		retval = sysfs_create_group(kobj, &sec_input_attr_group);
		if (retval < 0) {
			pr_err("%s %s: Failed to create sysfs attributes %d\n", SECLOG, __func__, retval);
		}
	} else {
		pr_info("%s %s: 'enabled' is already exist\n", SECLOG, __func__);
	}

	return retval;
}
EXPORT_SYMBOL(sec_input_sysfs_create);

void sec_input_sysfs_remove(struct kobject *kobj)
{
	sysfs_remove_group(kobj, &sec_input_attr_group);
}
EXPORT_SYMBOL(sec_input_sysfs_remove);

MODULE_DESCRIPTION("Samsung common functions");
MODULE_LICENSE("GPL");
