/*
 *  sec_battery_wc.c
 *  Samsung Mobile Battery Driver
 *
 *  Copyright (C) 2020 Samsung Electronics
 *
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */
#include "sec_battery.h"

#if defined(CONFIG_WIRELESS_FIRMWARE_UPDATE)
bool sec_bat_check_boost_mfc_condition(struct sec_battery_info *battery, int mode)
{
	union power_supply_propval value = {0, };
	int boost_status = 0, wpc_det = 0, mst_pwr_en = 0;

	pr_info("%s\n", __func__);

	if (mode == SEC_WIRELESS_FW_UPDATE_AUTO_MODE) {
		psy_do_property(battery->pdata->wireless_charger_name, get,
			POWER_SUPPLY_EXT_PROP_WIRELESS_INITIAL_WC_CHECK, value);
		wpc_det = value.intval;
	}

	psy_do_property(battery->pdata->wireless_charger_name, get,
		POWER_SUPPLY_EXT_PROP_WIRELESS_MST_PWR_EN, value);
	mst_pwr_en = value.intval;

	psy_do_property(battery->pdata->charger_name, get,
		POWER_SUPPLY_EXT_PROP_CHARGE_BOOST, value);
	boost_status = value.intval;

	pr_info("%s wpc_det(%d), mst_pwr_en(%d), boost_status(%d)\n",
		__func__, wpc_det, mst_pwr_en, boost_status);

	if (!boost_status && !wpc_det && !mst_pwr_en)
		return true;
	return false;
}

void sec_bat_fw_update(struct sec_battery_info *battery, int mode)
{
	union power_supply_propval value = {0, };
	int ret = 0;

	pr_info("%s\n", __func__);

	__pm_wakeup_event(battery->vbus_ws, jiffies_to_msecs(HZ * 10));

	switch (mode) {
	case SEC_WIRELESS_FW_UPDATE_SDCARD_MODE:
	case SEC_WIRELESS_FW_UPDATE_BUILTIN_MODE:
	case SEC_WIRELESS_FW_UPDATE_AUTO_MODE:
	case SEC_WIRELESS_FW_UPDATE_SPU_MODE:
	case SEC_WIRELESS_FW_UPDATE_SPU_VERIFY_MODE:
		battery->mfc_fw_update = true;
		sec_vote(battery->chgen_vote, VOTER_FW, true, SEC_BAT_CHG_MODE_BUCK_OFF);
		msleep(500);
		sec_vote(battery->iv_vote, VOTER_FW, true, SEC_INPUT_VOLTAGE_5V);
		msleep(500);
		value.intval = mode;
		ret = psy_do_property(battery->pdata->wireless_charger_name, set,
				POWER_SUPPLY_EXT_PROP_CHARGE_POWERED_OTG_CONTROL, value);
		if (ret < 0) {
			battery->mfc_fw_update = false;
			sec_vote(battery->chgen_vote, VOTER_FW, false, 0);
			sec_vote(battery->iv_vote, VOTER_FW, false, 0);
		}
		break;
	default:
		break;
	}
}
#endif

void sec_wireless_otg_control(struct sec_battery_info *battery, int enable)
{
	union power_supply_propval value = {0, };

	if (enable) {
		sec_bat_set_current_event(battery, SEC_BAT_CURRENT_EVENT_WPC_VOUT_LOCK,
			SEC_BAT_CURRENT_EVENT_WPC_VOUT_LOCK);
	} else {
		sec_bat_set_current_event(battery, 0,
			SEC_BAT_CURRENT_EVENT_WPC_VOUT_LOCK);
	}

	value.intval = enable;
	psy_do_property(battery->pdata->wireless_charger_name, set,
		POWER_SUPPLY_EXT_PROP_CHARGE_OTG_CONTROL, value);

	if (is_hv_wireless_type(battery->cable_type)) {
		int cnt;

		mutex_lock(&battery->voutlock);
		value.intval = (enable) ? WIRELESS_VOUT_5V :
			battery->wpc_vout_level;
		psy_do_property(battery->pdata->wireless_charger_name, set,
			POWER_SUPPLY_EXT_PROP_INPUT_VOLTAGE_REGULATION, value);
		mutex_unlock(&battery->voutlock);

		for (cnt = 0; cnt < 5; cnt++) {
			msleep(100);
			psy_do_property(battery->pdata->wireless_charger_name, get,
				POWER_SUPPLY_PROP_ENERGY_NOW, value);
			if (value.intval <= 6000) {
				pr_info("%s: wireless vout goes to 5V Vout(%d).\n",
					__func__, value.intval);
				break;
			}
		}
		sec_vote(battery->input_vote, VOTER_AICL, false, 0);
	} else if (is_nv_wireless_type(battery->cable_type)) {
		if (enable) {
			pr_info("%s: wireless 5V with OTG\n", __func__);
			value.intval = WIRELESS_VOUT_5V;
			psy_do_property(battery->pdata->wireless_charger_name, set,
				POWER_SUPPLY_EXT_PROP_INPUT_VOLTAGE_REGULATION, value);
		} else {
			pr_info("%s: wireless 5.5V without OTG\n", __func__);
			value.intval = WIRELESS_VOUT_CC_CV_VOUT;
			psy_do_property(battery->pdata->wireless_charger_name, set,
				POWER_SUPPLY_EXT_PROP_INPUT_VOLTAGE_REGULATION, value);
		}
	} else if (battery->wc_tx_enable && enable) {
		/* TX power should turn off during otg on */
		pr_info("@Tx_Mode %s: OTG is going to work, TX power should off\n", __func__);
		/* set tx event */
		sec_bat_set_tx_event(battery, BATT_TX_EVENT_WIRELESS_TX_OTG_ON, BATT_TX_EVENT_WIRELESS_TX_OTG_ON);
		sec_wireless_set_tx_enable(battery, false);
	}
	queue_delayed_work(battery->monitor_wqueue,
			&battery->otg_work, msecs_to_jiffies(0));
}

void sec_bat_set_wireless20_current(struct sec_battery_info *battery, int rx_power)
{
	battery->wc20_vout = battery->pdata->wireless_power_info[rx_power].vout;

	pr_info("%s: vout=%dmV\n", __func__, battery->wc20_vout);
	if (battery->wc_status == SEC_BATTERY_CABLE_HV_WIRELESS_20) {
		sec_bat_change_default_current(battery, SEC_BATTERY_CABLE_HV_WIRELESS_20,
				battery->pdata->wireless_power_info[rx_power].input_current_limit,
				battery->pdata->wireless_power_info[rx_power].fast_charging_current);
		sec_vote(battery->input_vote, VOTER_CABLE, true,
			battery->pdata->wireless_power_info[rx_power].input_current_limit);
		sec_vote(battery->fcc_vote, VOTER_CABLE, true,
			battery->pdata->wireless_power_info[rx_power].fast_charging_current);

		if (battery->pdata->wireless_power_info[rx_power].rx_power <= 4500)
			battery->wc20_power_class = 0;
		else if (battery->pdata->wireless_power_info[rx_power].rx_power <= 7500)
			battery->wc20_power_class = SEC_WIRELESS_RX_POWER_CLASS_1;
		else if (battery->pdata->wireless_power_info[rx_power].rx_power <= 12000)
			battery->wc20_power_class = SEC_WIRELESS_RX_POWER_CLASS_2;
		else if (battery->pdata->wireless_power_info[rx_power].rx_power <= 20000)
			battery->wc20_power_class = SEC_WIRELESS_RX_POWER_CLASS_3;
		else
			battery->wc20_power_class = SEC_WIRELESS_RX_POWER_CLASS_4;

		if (is_wired_type(battery->cable_type)) {
			int wl_power = battery->pdata->wireless_power_info[rx_power].rx_power;

			pr_info("%s: check power(%d <--> %d)\n",
				__func__, battery->max_charge_power, wl_power);
			if (battery->max_charge_power < wl_power) {
				__pm_stay_awake(battery->cable_ws);
				queue_delayed_work(battery->monitor_wqueue,
					&battery->cable_work, 0);
			}
		} else {
			sec_bat_set_charging_current(battery);
		}
	}
}

void set_wireless_otg_input_current(struct sec_battery_info *battery)
{
	union power_supply_propval value = {0, };

	if (!is_wireless_type(battery->cable_type))
		return;

	psy_do_property(battery->pdata->charger_name, get,
			POWER_SUPPLY_EXT_PROP_CHARGE_OTG_CONTROL, value);
	if (value.intval)
		sec_vote(battery->input_vote, VOTER_OTG, true, battery->pdata->wireless_otg_input_current);
	else
		sec_vote(battery->input_vote, VOTER_OTG, false, 0);
}

void sec_bat_set_decrease_iout(struct sec_battery_info *battery, bool last_delay)
{
	union power_supply_propval value = {0, };
	int i = 0, step = 3, input_current[3] = {500, 300, 100};
	int prev_input_current = battery->input_current;

	for (i = 0 ; i < step ; i++) {
		if (prev_input_current > input_current[i]) {
			pr_info("@DIS_MFC %s: Wireless iout goes to %dmA before switch charging path to cable\n",
				__func__, input_current[i]);
			prev_input_current = input_current[i];
			value.intval = input_current[i];
			psy_do_property(battery->pdata->charger_name, set,
				POWER_SUPPLY_EXT_PROP_PAD_VOLT_CTRL, value);

			if ((i != step - 1) || last_delay)
				msleep(300);
		}
	}
}
EXPORT_SYMBOL_KUNIT(sec_bat_set_decrease_iout);

void sec_bat_set_mfc_off(struct sec_battery_info *battery, bool need_ept)
{
	union power_supply_propval value = {0, };
	char wpc_en_status[2];

	if (need_ept) {
		psy_do_property(battery->pdata->wireless_charger_name, set,
			POWER_SUPPLY_PROP_ONLINE, value);

		msleep(300);
	}

	wpc_en_status[0] = WPC_EN_CHARGING;
	wpc_en_status[1] = false;
	value.strval = wpc_en_status;
	psy_do_property(battery->pdata->wireless_charger_name, set,
		POWER_SUPPLY_EXT_PROP_WPC_EN, value);

	pr_info("@DIS_MFC %s: WC CONTROL: Disable\n", __func__);
}

void sec_bat_set_mfc_on(struct sec_battery_info *battery)
{
	union power_supply_propval value = {0, };
	char wpc_en_status[2];

	wpc_en_status[0] = WPC_EN_CHARGING;
	wpc_en_status[1] = true;
	value.strval = wpc_en_status;
	psy_do_property(battery->pdata->wireless_charger_name, set,
		POWER_SUPPLY_EXT_PROP_WPC_EN, value);

	pr_info("%s: WC CONTROL: Enable\n", __func__);
}

int sec_bat_choose_cable_type(struct sec_battery_info *battery)
{
	int current_cable_type = SEC_BATTERY_CABLE_NONE, current_wire_status = battery->wire_status;
	union power_supply_propval val = {0, };
	int wire_power = 0;

	if (is_wired_type(current_wire_status)) {
		int wire_current = 0, wire_vol = 0;

		wire_current = (current_wire_status == SEC_BATTERY_CABLE_PREPARE_TA ?
			battery->pdata->charging_current[SEC_BATTERY_CABLE_TA].input_current_limit :
			battery->pdata->charging_current[current_wire_status].input_current_limit);
		wire_vol = is_hv_wire_type(current_wire_status) ?
			(current_wire_status == SEC_BATTERY_CABLE_12V_TA ? SEC_INPUT_VOLTAGE_12V : SEC_INPUT_VOLTAGE_9V)
			: SEC_INPUT_VOLTAGE_5V;

		wire_power = mW_by_mVmA(wire_vol, wire_current);
		pr_info("%s: wr_power(%d), wire_cable_type(%d)\n", __func__, wire_power, current_wire_status);
	}

	if ((battery->wc_status != SEC_BATTERY_CABLE_NONE) && battery->wc_enable) {
		int temp_current_type;

		current_cable_type = battery->wc_status;

		if (current_cable_type == SEC_BATTERY_CABLE_PREPARE_WIRELESS_HV)
			temp_current_type = SEC_BATTERY_CABLE_HV_WIRELESS;
		else if (current_cable_type == SEC_BATTERY_CABLE_PREPARE_WIRELESS_20)
			temp_current_type = SEC_BATTERY_CABLE_HV_WIRELESS_20;
		else
			temp_current_type = current_cable_type;

		if (!is_nocharge_type(current_wire_status)) {
			int wireless_current = 0, wireless_power = 0;

			wireless_current = battery->pdata->charging_current[temp_current_type].input_current_limit;
			if (is_nv_wireless_type(temp_current_type))
				wireless_power = mW_by_mVmA(SEC_INPUT_VOLTAGE_5_5V, wireless_current);
			else if (temp_current_type == SEC_BATTERY_CABLE_HV_WIRELESS_20)
				wireless_power = mW_by_mVmA(battery->wc20_vout, wireless_current);
			else
				wireless_power = mW_by_mVmA(SEC_INPUT_VOLTAGE_10V, wireless_current);

			if (is_pd_wire_type(current_wire_status)) {
				pr_info("%s: wr_power(%d), pd_max_power(%d), wc_cable_type(%d), wire_cable_type(%d)\n",
					__func__, wireless_power,
					battery->pd_max_charge_power, current_cable_type, current_wire_status);
			} else {
				pr_info("%s: wl_power(%d), wr_power(%d), wc_cable_type(%d), wire_cable_type(%d)\n",
					__func__, wireless_power, wire_power, current_cable_type, current_wire_status);
			}

			if ((is_pd_wire_type(current_wire_status) && wireless_power <= battery->pd_max_charge_power) ||
				(!is_pd_wire_type(current_wire_status) && wireless_power <= wire_power)) {
				current_cable_type = current_wire_status;
				pr_info("%s: switch charging path to cable\n", __func__);

				/* limit charging current before change path between chgin and wcin */
				if ((battery->cable_type == SEC_BATTERY_CABLE_HV_WIRELESS_20) &&
					(temp_current_type == SEC_BATTERY_CABLE_HV_WIRELESS_20)) {
					/* limit charging current before change path between chgin and wcin */
					pr_info("%s: set charging current %dmA for a moment in case of TA OCP\n",
						__func__, battery->pdata->wpc_charging_limit_current);
					sec_vote(battery->fcc_vote, VOTER_CABLE, true, battery->pdata->wpc_charging_limit_current);
					msleep(100);
				}

				battery->wc_need_ldo_on = true;
				val.intval = MFC_LDO_OFF;
				psy_do_property(battery->pdata->wireless_charger_name, set,
					POWER_SUPPLY_PROP_CHARGE_EMPTY, val);
				/* Turn off TX to charge by cable charging having more power */
				if (battery->wc_status == SEC_BATTERY_CABLE_WIRELESS_TX) {
					pr_info("@Tx_Mode %s: RX device with TA, notify TX device of this info\n",
							__func__);
					val.intval = true;
					psy_do_property(battery->pdata->wireless_charger_name, set,
						POWER_SUPPLY_EXT_PROP_WIRELESS_SWITCH, val);
				}
			} else {
				if (!is_wireless_type(battery->cable_type)) {
					/* limit charging current before change path between chgin and wcin */
					pr_info("%s: set charging current %dmA for a moment in case of TA OCP\n",
						__func__, battery->pdata->wpc_charging_limit_current);
					sec_vote(battery->fcc_vote, VOTER_CABLE, true, battery->pdata->wpc_charging_limit_current);
					msleep(100);
				}
				pr_info("%s : switch charging path to wireless\n", __func__);
				battery->wc_need_ldo_on = false;
				val.intval = MFC_LDO_ON;
				psy_do_property(battery->pdata->wireless_charger_name, set,
					POWER_SUPPLY_PROP_CHARGE_EMPTY, val);
			}
		} else {
			/* turn on ldo when ldo was off because of TA, */
			/* ldo is supposed to turn on automatically except force off by sw. */
			/* do not turn on ldo every wireless connection just in case ldo re-toggle by ic */
			if (battery->wc_need_ldo_on) {
				battery->wc_need_ldo_on = false;
				val.intval = MFC_LDO_ON;
				psy_do_property(battery->pdata->wireless_charger_name, set,
					POWER_SUPPLY_PROP_CHARGE_EMPTY, val);
			}
		}
	} else if (battery->pogo_status) {
	} else
		current_cable_type = current_wire_status;

	return current_cable_type;
}

void sec_bat_get_wireless_current(struct sec_battery_info *battery)
{
	int incurr = INT_MAX;
	union power_supply_propval value = {0, };
	int is_otg_on = 0;

	psy_do_property(battery->pdata->charger_name, get,
			POWER_SUPPLY_EXT_PROP_CHARGE_OTG_CONTROL, value);
	is_otg_on = value.intval;

	if (is_otg_on) {
		pr_info("%s: both wireless chg and otg recognized.\n", __func__);
		incurr = battery->pdata->wireless_otg_input_current;
	}

	/* 2. WPC_SLEEP_MODE */
	if (is_hv_wireless_type(battery->cable_type) && battery->sleep_mode) {
		if (incurr > battery->pdata->sleep_mode_limit_current)
			incurr = battery->pdata->sleep_mode_limit_current;
		pr_info("%s: sleep_mode = %d, chg_limit = %d, in_curr = %d\n",
				__func__, battery->sleep_mode, battery->chg_limit, incurr);

		if (!battery->auto_mode) {
			/* send cmd once */
			battery->auto_mode = true;
			value.intval = WIRELESS_SLEEP_MODE_ENABLE;
			psy_do_property(battery->pdata->wireless_charger_name, set,
					POWER_SUPPLY_EXT_PROP_WIRELESS_RX_CONTROL, value);
		}
	}

	/* 3. WPC_TEMP_MODE */
	if (is_wireless_type(battery->cable_type) && battery->chg_limit) {
		if ((battery->siop_level >= 100 && !battery->lcd_status) &&
				(incurr > battery->pdata->wpc_input_limit_current)) {
			if (battery->cable_type == SEC_BATTERY_CABLE_WIRELESS_TX &&
				battery->pdata->wpc_input_limit_by_tx_check)
				incurr = battery->pdata->wpc_input_limit_current_by_tx;
			else
				incurr = battery->pdata->wpc_input_limit_current;
		} else if ((battery->siop_level < 100 || battery->lcd_status) &&
				(incurr > battery->pdata->wpc_lcd_on_input_limit_current))
			incurr = battery->pdata->wpc_lcd_on_input_limit_current;
	}

	/* 5. Full-Additional state */
	if (battery->status == POWER_SUPPLY_STATUS_FULL && battery->charging_mode == SEC_BATTERY_CHARGING_2ND) {
		if (incurr > battery->pdata->siop_hv_wpc_icl)
			incurr = battery->pdata->siop_hv_wpc_icl;
	}

	/* 6. Hero Stand Pad CV */
	if (battery->capacity >= battery->pdata->wc_hero_stand_cc_cv) {
		if (battery->cable_type == SEC_BATTERY_CABLE_WIRELESS_STAND) {
			if (incurr > battery->pdata->wc_hero_stand_cv_current)
				incurr = battery->pdata->wc_hero_stand_cv_current;
		} else if (battery->cable_type == SEC_BATTERY_CABLE_WIRELESS_HV_STAND) {
			if (battery->chg_limit &&
					incurr > battery->pdata->wc_hero_stand_cv_current) {
				incurr = battery->pdata->wc_hero_stand_cv_current;
			} else if (!battery->chg_limit &&
					incurr > battery->pdata->wc_hero_stand_hv_cv_current) {
				incurr = battery->pdata->wc_hero_stand_hv_cv_current;
			}
		}
	}

	/* 7. Full-None state && SIOP_LEVEL 100 */
	if ((battery->siop_level >= 100 && !battery->lcd_status) &&
		battery->status == POWER_SUPPLY_STATUS_FULL && battery->charging_mode == SEC_BATTERY_CHARGING_NONE) {
		incurr = battery->pdata->wc_full_input_limit_current;
	}

	if (incurr != INT_MAX)
		sec_vote(battery->input_vote, VOTER_WPC_CUR, true, incurr);
	else
		sec_vote(battery->input_vote, VOTER_WPC_CUR, false, incurr);
}

int sec_bat_check_wc_available(struct sec_battery_info *battery)
{
	mutex_lock(&battery->wclock);
	if (!battery->wc_enable) {
		pr_info("%s: wc_enable(%d), cnt(%d)\n", __func__, battery->wc_enable, battery->wc_enable_cnt);
		if (battery->wc_enable_cnt > battery->wc_enable_cnt_value) {
			union power_supply_propval val = {0, };
			char wpc_en_status[2];

			battery->wc_enable = true;
			battery->wc_enable_cnt = 0;
			wpc_en_status[0] = WPC_EN_SYSFS;
			wpc_en_status[1] = true;
			val.strval = wpc_en_status;
			psy_do_property(battery->pdata->wireless_charger_name, set,
					POWER_SUPPLY_EXT_PROP_WPC_EN, val);
			pr_info("%s: WC CONTROL: Enable\n", __func__);
		}
		battery->wc_enable_cnt++;
	}
	mutex_unlock(&battery->wclock);

	return 0;
}

/* OTG during HV wireless charging or sleep mode have 4.5W normal wireless charging UI */
bool sec_bat_hv_wc_normal_mode_check(struct sec_battery_info *battery)
{
	union power_supply_propval value = {0, };

	psy_do_property(battery->pdata->charger_name, get,
			POWER_SUPPLY_EXT_PROP_CHARGE_OTG_CONTROL, value);
	if (value.intval || battery->sleep_mode) {
		pr_info("%s: otg(%d), sleep_mode(%d)\n", __func__, value.intval, battery->sleep_mode);
		return true;
	}
	return false;
}
EXPORT_SYMBOL_KUNIT(sec_bat_hv_wc_normal_mode_check);

void sec_bat_ext_event_work_content(struct sec_battery_info *battery)
{
	union power_supply_propval value = {0, };

	if (battery->wc_tx_enable) { /* TX ON state */
		if (battery->ext_event & BATT_EXT_EVENT_CAMERA) {
			pr_info("@Tx_Mode %s: Camera ON, TX OFF\n", __func__);
			sec_bat_set_tx_event(battery,
					BATT_TX_EVENT_WIRELESS_TX_CAMERA_ON, BATT_TX_EVENT_WIRELESS_TX_CAMERA_ON);
			sec_wireless_set_tx_enable(battery, false);
		} else if (battery->ext_event & BATT_EXT_EVENT_DEX) {
			pr_info("@Tx_Mode %s: Dex ON, TX OFF\n", __func__);
			sec_bat_set_tx_event(battery,
					BATT_TX_EVENT_WIRELESS_TX_OTG_ON, BATT_TX_EVENT_WIRELESS_TX_OTG_ON);
			sec_wireless_set_tx_enable(battery, false);
		} else if (battery->ext_event & BATT_EXT_EVENT_CALL) {
			pr_info("@Tx_Mode %s: Call ON, TX OFF\n", __func__);
			battery->tx_retry_case |= SEC_BAT_TX_RETRY_CALL;
			/* clear tx all event */
			sec_bat_set_tx_event(battery, 0, BATT_TX_EVENT_WIRELESS_ALL_MASK);
			sec_wireless_set_tx_enable(battery, false);
		}
	} else { /* TX OFF state, it has only call scenario */
		if (battery->ext_event & BATT_EXT_EVENT_CALL) {
			pr_info("@Tx_Mode %s: Call ON\n", __func__);

			value.intval = BATT_EXT_EVENT_CALL;
			psy_do_property(battery->pdata->wireless_charger_name, set,
							POWER_SUPPLY_EXT_PROP_CALL_EVENT, value);

			if (battery->cable_type == SEC_BATTERY_CABLE_WIRELESS_PACK ||
				battery->cable_type == SEC_BATTERY_CABLE_WIRELESS_HV_PACK ||
				battery->cable_type == SEC_BATTERY_CABLE_WIRELESS_TX) {
				pr_info("%s: Call is on during Wireless Pack or TX\n", __func__);
				battery->wc_rx_phm_mode = true;
			}
			if (battery->tx_retry_case != SEC_BAT_TX_RETRY_NONE) {
				pr_info("@Tx_Mode %s: TX OFF because of other reason(retry:0x%x), save call retry case\n",
					__func__, battery->tx_retry_case);
				battery->tx_retry_case |= SEC_BAT_TX_RETRY_CALL;
			}
		} else if (!(battery->ext_event & BATT_EXT_EVENT_CALL)) {
			pr_info("@Tx_Mode %s: Call OFF\n", __func__);

			value.intval = BATT_EXT_EVENT_NONE;
			psy_do_property(battery->pdata->wireless_charger_name, set,
							POWER_SUPPLY_EXT_PROP_CALL_EVENT, value);

			/* check the diff between current and previous ext_event state */
			if (battery->tx_retry_case & SEC_BAT_TX_RETRY_CALL) {
				battery->tx_retry_case &= ~SEC_BAT_TX_RETRY_CALL;
				if (!battery->tx_retry_case) {
					pr_info("@Tx_Mode %s: Call OFF, TX Retry\n", __func__);
					sec_bat_set_tx_event(battery,
							BATT_TX_EVENT_WIRELESS_TX_RETRY,
							BATT_TX_EVENT_WIRELESS_TX_RETRY);
				}
			} else if (battery->cable_type == SEC_BATTERY_CABLE_WIRELESS_PACK ||
				battery->cable_type == SEC_BATTERY_CABLE_WIRELESS_HV_PACK ||
				battery->cable_type == SEC_BATTERY_CABLE_WIRELESS_TX) {
				pr_info("%s: Call is off during Wireless Pack or TX\n", __func__);
			}

			/* process escape phm */
			if (battery->wc_rx_phm_mode) {
				pr_info("%s: ESCAPE PHM STEP 1\n", __func__);
				sec_bat_set_mfc_on(battery);
				msleep(100);

				pr_info("%s: ESCAPE PHM STEP 2\n", __func__);
				sec_bat_set_mfc_off(battery, false);
				msleep(510);

				pr_info("%s: ESCAPE PHM STEP 3\n", __func__);
				sec_bat_set_mfc_on(battery);
			}
			battery->wc_rx_phm_mode = false;
		}
	}

	__pm_relax(battery->ext_event_ws);
}

void sec_bat_wireless_minduty_cntl(struct sec_battery_info *battery, unsigned int duty_val)
{
	union power_supply_propval value = {0, };

	if (duty_val != battery->tx_minduty) {
		value.intval = duty_val;
		psy_do_property(battery->pdata->wireless_charger_name, set,
				POWER_SUPPLY_EXT_PROP_WIRELESS_MIN_DUTY, value);

		pr_info("@Tx_Mode %s: Min duty chagned (%d -> %d)\n", __func__, battery->tx_minduty, duty_val);
		battery->tx_minduty = duty_val;
	}
}

void sec_bat_wireless_uno_cntl(struct sec_battery_info *battery, bool en)
{
	union power_supply_propval value = {0, };

	value.intval = en;
	battery->uno_en = en;
	pr_info("@Tx_Mode %s: Uno control %d\n", __func__, battery->uno_en);

	psy_do_property(battery->pdata->wireless_charger_name, set,
		POWER_SUPPLY_EXT_PROP_WIRELESS_TX_ENABLE, value);
}

void sec_bat_wireless_iout_cntl(struct sec_battery_info *battery, int uno_iout, int mfc_iout)
{
	union power_supply_propval value = {0, };

	if (battery->tx_uno_iout != uno_iout) {
		pr_info("@Tx_Mode %s: set uno iout(%d) -> (%d)\n", __func__, battery->tx_uno_iout, uno_iout);
		value.intval = battery->tx_uno_iout = uno_iout;
		psy_do_property("otg", set,
				POWER_SUPPLY_EXT_PROP_WIRELESS_TX_IOUT, value);
	} else {
		pr_info("@Tx_Mode %s: Already set Uno Iout(%d == %d)\n", __func__, battery->tx_uno_iout, uno_iout);
	}

#if !defined(CONFIG_SEC_FACTORY)
	if (battery->lcd_status && (mfc_iout == battery->pdata->tx_mfc_iout_phone)) {
		pr_info("@Tx_Mode %s: Reduce Tx MFC Iout. LCD ON\n", __func__);
		mfc_iout = battery->pdata->tx_mfc_iout_lcd_on;
	}
#endif

	if (battery->tx_mfc_iout != mfc_iout) {
		pr_info("@Tx_Mode %s: set mfc iout(%d) -> (%d)\n", __func__, battery->tx_mfc_iout, mfc_iout);
		value.intval = battery->tx_mfc_iout = mfc_iout;
		psy_do_property(battery->pdata->wireless_charger_name, set,
				POWER_SUPPLY_EXT_PROP_WIRELESS_TX_IOUT, value);
	} else {
		pr_info("@Tx_Mode %s: Already set MFC Iout(%d == %d)\n", __func__, battery->tx_mfc_iout, mfc_iout);
	}
}

void sec_bat_wireless_vout_cntl(struct sec_battery_info *battery, int vout_now)
{
	union power_supply_propval value = {0, };

	pr_info("@Tx_Mode %s: set uno & mfc vout (%dmV -> %dmV)\n", __func__, battery->wc_tx_vout, vout_now);

	if (battery->wc_tx_vout >= vout_now) {
		battery->wc_tx_vout = value.intval = vout_now;
		psy_do_property(battery->pdata->wireless_charger_name, set,
				POWER_SUPPLY_EXT_PROP_WIRELESS_TX_VOUT, value);
		psy_do_property("otg", set,
				POWER_SUPPLY_EXT_PROP_WIRELESS_TX_VOUT, value);
	} else if (vout_now > battery->wc_tx_vout) {
		battery->wc_tx_vout = value.intval = vout_now;
		psy_do_property("otg", set,
				POWER_SUPPLY_EXT_PROP_WIRELESS_TX_VOUT, value);
		psy_do_property(battery->pdata->wireless_charger_name, set,
				POWER_SUPPLY_EXT_PROP_WIRELESS_TX_VOUT, value);
	}

}

#if defined(CONFIG_WIRELESS_TX_MODE)
#if !defined(CONFIG_SEC_FACTORY)
static void sec_bat_check_tx_battery_drain(struct sec_battery_info *battery)
{
	if (battery->capacity <= battery->pdata->tx_stop_capacity &&
		is_nocharge_type(battery->cable_type)) {
		pr_info("@Tx_Mode %s: battery level is drained, TX mode should turn off\n", __func__);
		/* set tx event */
		sec_bat_set_tx_event(battery, BATT_TX_EVENT_WIRELESS_TX_SOC_DRAIN, BATT_TX_EVENT_WIRELESS_TX_SOC_DRAIN);
		sec_wireless_set_tx_enable(battery, false);
	}
}

static void sec_bat_check_tx_current(struct sec_battery_info *battery)
{
	if (battery->lcd_status && (battery->tx_mfc_iout > battery->pdata->tx_mfc_iout_lcd_on)) {
		sec_bat_wireless_iout_cntl(battery, battery->pdata->tx_uno_iout, battery->pdata->tx_mfc_iout_lcd_on);
		pr_info("@Tx_Mode %s: Reduce Tx MFC Iout. LCD ON\n", __func__);
	} else if (!battery->lcd_status && (battery->tx_mfc_iout == battery->pdata->tx_mfc_iout_lcd_on)) {
		union power_supply_propval value = {0, };

		sec_bat_wireless_iout_cntl(battery, battery->pdata->tx_uno_iout, battery->pdata->tx_mfc_iout_phone);
		pr_info("@Tx_Mode %s: Recovery Tx MFC Iout. LCD OFF\n", __func__);

		value.intval = true;
		psy_do_property(battery->pdata->wireless_charger_name, set,
			POWER_SUPPLY_EXT_PROP_WIRELESS_SEND_FSK, value);
	}
}
#endif

static void sec_bat_check_tx_switch_mode(struct sec_battery_info *battery)
{
	union power_supply_propval value = {0, };

#if defined(CONFIG_TX_GEAR_AOV)
	if (battery->tx_switch_mode == TX_SWITCH_UNO_FOR_GEAR) {
		pr_info("@Tx_mode %s: Tx mode(%d). skip!\n",
			__func__, battery->tx_switch_mode);
		return;
	}
#endif

	if (battery->current_event & SEC_BAT_CURRENT_EVENT_AFC)	{
		pr_info("@Tx_mode %s: Do not switch switch mode! AFC Event set\n", __func__);
		return;
	}

	value.intval = SEC_FUELGAUGE_CAPACITY_TYPE_CAPACITY_POINT;
	psy_do_property(battery->pdata->fuelgauge_name, get,
			POWER_SUPPLY_PROP_CAPACITY, value);

	if ((battery->tx_switch_mode == TX_SWITCH_UNO_ONLY) && (!battery->buck_cntl_by_tx)) {
		battery->buck_cntl_by_tx = true;
		sec_vote(battery->chgen_vote, VOTER_WC_TX, true, SEC_BAT_CHG_MODE_BUCK_OFF);

		sec_bat_wireless_iout_cntl(battery, battery->pdata->tx_uno_iout, battery->pdata->tx_mfc_iout_phone);
		sec_bat_wireless_vout_cntl(battery, battery->pdata->tx_uno_vout);
#if defined(CONFIG_TX_GEAR_AOV)
		if (battery->wc_rx_type != SS_GEAR)
#endif
			sec_bat_wireless_minduty_cntl(battery, battery->pdata->tx_minduty_default);
	} else if ((battery->tx_switch_mode == TX_SWITCH_CHG_ONLY) && (battery->buck_cntl_by_tx)) {
		sec_bat_wireless_iout_cntl(battery, battery->pdata->tx_uno_iout, battery->pdata->tx_mfc_iout_phone_5v);
		sec_bat_wireless_vout_cntl(battery, WC_TX_VOUT_5000MV);
#if defined(CONFIG_TX_GEAR_AOV)
		if (battery->wc_rx_type != SS_GEAR)
#endif
			sec_bat_wireless_minduty_cntl(battery, battery->pdata->tx_minduty_5V);

		battery->buck_cntl_by_tx = false;
		sec_vote(battery->chgen_vote, VOTER_WC_TX, false, 0);
	}

	if (battery->status == POWER_SUPPLY_STATUS_FULL) {
		if (battery->charging_mode == SEC_BATTERY_CHARGING_NONE) {
			if (battery->tx_switch_mode == TX_SWITCH_CHG_ONLY)
				battery->tx_switch_mode_change = true;
		} else {
			if (battery->tx_switch_mode == TX_SWITCH_UNO_ONLY) {
				if (battery->tx_switch_start_soc >= 100) {
					if (battery->capacity < 99 || (battery->capacity == 99 && value.intval <= 1))
						battery->tx_switch_mode_change = true;
				} else {
					if ((battery->capacity == battery->tx_switch_start_soc && value.intval <= 1) ||
					(battery->capacity < battery->tx_switch_start_soc))
						battery->tx_switch_mode_change = true;
				}
			} else if (battery->tx_switch_mode == TX_SWITCH_CHG_ONLY) {
				if (battery->capacity >= 100)
					battery->tx_switch_mode_change = true;
			}
		}
	} else {
		if (battery->tx_switch_mode == TX_SWITCH_UNO_ONLY) {
			if (((battery->capacity == battery->tx_switch_start_soc) && (value.intval <= 1)) ||
				(battery->capacity < battery->tx_switch_start_soc))
				battery->tx_switch_mode_change = true;

		} else if (battery->tx_switch_mode == TX_SWITCH_CHG_ONLY) {
			if (((battery->capacity == (battery->tx_switch_start_soc + 1)) && (value.intval >= 8)) ||
				(battery->capacity > (battery->tx_switch_start_soc + 1)))
				battery->tx_switch_mode_change = true;
		}
	}
	pr_info("@Tx_mode Tx mode(%d) tx_switch_mode_chage(%d) start soc(%d) now soc(%d.%d)\n",
		battery->tx_switch_mode, battery->tx_switch_mode_change,
		battery->tx_switch_start_soc, battery->capacity, value.intval);
}
#endif

void sec_bat_txpower_calc(struct sec_battery_info *battery)
{
	if (delayed_work_pending(&battery->wpc_txpower_calc_work)) {
		pr_info("%s: keep average tx power(%5d mA)\n", __func__, battery->tx_avg_curr);
	} else if (battery->wc_tx_enable) {
		int tx_vout = 0, tx_iout = 0, vbatt = 0;
		union power_supply_propval value = {0, };

		if (battery->tx_clear) {
			battery->tx_time_cnt = 0;
			battery->tx_avg_curr = 0;
			battery->tx_total_power = 0;
			battery->tx_clear = false;
		}

		if (battery->tx_clear_cisd) {
			battery->tx_total_power_cisd = 0;
			battery->tx_clear_cisd = false;
		}

		psy_do_property(battery->pdata->wireless_charger_name, get,
			POWER_SUPPLY_EXT_PROP_WIRELESS_TX_UNO_VIN, value);
		tx_vout = value.intval;

		psy_do_property(battery->pdata->wireless_charger_name, get,
			POWER_SUPPLY_EXT_PROP_WIRELESS_TX_UNO_IIN, value);
		tx_iout = value.intval;

		value.intval = SEC_BATTERY_VOLTAGE_MV;
		psy_do_property(battery->pdata->fuelgauge_name, get,
			POWER_SUPPLY_PROP_VOLTAGE_NOW, value);
		vbatt = value.intval;

		battery->tx_time_cnt++;

		/* AVG curr will be calculated only when the battery is discharged */
		if (battery->current_avg <= 0 && vbatt > 0)
			tx_iout = (tx_vout / vbatt) * tx_iout;
		else
			tx_iout = 0;

		/* monitor work will be scheduled every 10s when wc_tx_enable is true */
		battery->tx_avg_curr =
			((battery->tx_avg_curr * battery->tx_time_cnt) + tx_iout) / (battery->tx_time_cnt + 1);
		battery->tx_total_power =
			(battery->tx_avg_curr * battery->tx_time_cnt) / (60 * 60 / 10);

		/* daily accumulated power consumption by Tx, will be cleared when cisd data is sent */
		battery->tx_total_power_cisd = battery->tx_total_power_cisd + battery->tx_total_power;

		pr_info("%s:tx_time_cnt(%ds), UNO_Vin(%dV), UNO_Iin(%dmA), tx_avg_curr(%dmA)\n",
				__func__, battery->tx_time_cnt * 10, tx_vout, tx_iout, battery->tx_avg_curr);
		pr_info("%s:tx_total_power(%dmAh), tx_total_power_cisd(%dmAh))\n",
				__func__, battery->tx_total_power, battery->tx_total_power_cisd);
	}
}

void sec_bat_handle_tx_misalign(struct sec_battery_info *battery, bool trigger_misalign)
{
	struct timespec64 ts = {0, };

	if (trigger_misalign) {
		if (battery->tx_misalign_start_time == 0) {
			ts = ktime_to_timespec64(ktime_get_boottime());
			battery->tx_misalign_start_time = ts.tv_sec;
		}
		pr_info("@Tx_Mode %s: misalign is triggered!!(%d)\n", __func__, ++battery->tx_misalign_cnt);
		/* Attention!! in this case, 0x00(TX_OFF)  is sent first */
		/* and then 0x8000(RETRY) is sent */
		if (battery->tx_misalign_cnt < 3) {
			battery->tx_retry_case |= SEC_BAT_TX_RETRY_MISALIGN;
			sec_wireless_set_tx_enable(battery, false);
			/* clear tx all event */
			sec_bat_set_tx_event(battery, 0, BATT_TX_EVENT_WIRELESS_ALL_MASK);
			sec_bat_set_tx_event(battery,
					BATT_TX_EVENT_WIRELESS_TX_RETRY, BATT_TX_EVENT_WIRELESS_TX_RETRY);
		} else {
			battery->tx_retry_case &= ~SEC_BAT_TX_RETRY_MISALIGN;
			battery->tx_misalign_start_time = 0;
			battery->tx_misalign_cnt = 0;
			pr_info("@Tx_Mode %s: Misalign over 3 times, TX OFF (cancel misalign)\n", __func__);
			sec_bat_set_tx_event(battery,
					BATT_TX_EVENT_WIRELESS_TX_MISALIGN, BATT_TX_EVENT_WIRELESS_TX_MISALIGN);
			sec_wireless_set_tx_enable(battery, false);
		}
	} else if (battery->tx_retry_case & SEC_BAT_TX_RETRY_MISALIGN) {
		ts = ktime_to_timespec64(ktime_get_boottime());
		if (ts.tv_sec >= battery->tx_misalign_start_time) {
			battery->tx_misalign_passed_time = ts.tv_sec - battery->tx_misalign_start_time;
		} else {
			battery->tx_misalign_passed_time = 0xFFFFFFFF - battery->tx_misalign_start_time
				+ ts.tv_sec;
		}
		pr_info("@Tx_Mode %s: already misaligned, passed time(%ld)\n",
				__func__, battery->tx_misalign_passed_time);

		if (battery->tx_misalign_passed_time >= 60) {
			pr_info("@Tx_Mode %s: after 1min\n", __func__);
			if (battery->wc_tx_enable) {
				if (battery->wc_rx_connected) {
					pr_info("@Tx_Mode %s: RX Dev, Keep TX ON status (cancel misalign)\n", __func__);
				} else {
					pr_info("@Tx_Mode %s: NO RX Dev, TX OFF (cancel misalign)\n", __func__);
					sec_bat_set_tx_event(battery,
							BATT_TX_EVENT_WIRELESS_TX_MISALIGN,
							BATT_TX_EVENT_WIRELESS_TX_MISALIGN);
					sec_wireless_set_tx_enable(battery, false);
				}
			} else {
				pr_info("@Tx_Mode %s: Keep TX OFF status (cancel misalign)\n", __func__);
//				sec_bat_set_tx_event(battery,
//						BATT_TX_EVENT_WIRELESS_TX_ETC, BATT_TX_EVENT_WIRELESS_TX_ETC);
			}
			battery->tx_retry_case &= ~SEC_BAT_TX_RETRY_MISALIGN;
			battery->tx_misalign_start_time = 0;
			battery->tx_misalign_cnt = 0;
		}
	}
}
EXPORT_SYMBOL_KUNIT(sec_bat_handle_tx_misalign);

void sec_bat_handle_tx_ocp(struct sec_battery_info *battery, bool trigger_ocp)
{
	struct timespec64 ts = {0, };

	if (trigger_ocp) {
		if (battery->tx_ocp_start_time == 0) {
			ts = ktime_to_timespec64(ktime_get_boottime());
			battery->tx_ocp_start_time = ts.tv_sec;
		}
		pr_info("@Tx_Mode %s: ocp is triggered!!(%d)\n", __func__, ++battery->tx_ocp_cnt);
		/* Attention!! in this case, 0x00(TX_OFF)  is sent first */
		/* and then 0x8000(RETRY) is sent */
		if (battery->tx_ocp_cnt < 3) {
			battery->tx_retry_case |= SEC_BAT_TX_RETRY_OCP;
			sec_wireless_set_tx_enable(battery, false);
			/* clear tx all event */
			sec_bat_set_tx_event(battery, 0, BATT_TX_EVENT_WIRELESS_ALL_MASK);
			sec_bat_set_tx_event(battery,
					BATT_TX_EVENT_WIRELESS_TX_RETRY, BATT_TX_EVENT_WIRELESS_TX_RETRY);
		} else {
			battery->tx_retry_case &= ~SEC_BAT_TX_RETRY_OCP;
			battery->tx_ocp_start_time = 0;
			battery->tx_ocp_cnt = 0;
			pr_info("@Tx_Mode %s: ocp over 3 times, TX OFF (cancel ocp)\n", __func__);
			sec_bat_set_tx_event(battery,
				BATT_TX_EVENT_WIRELESS_TX_OCP, BATT_TX_EVENT_WIRELESS_TX_OCP);
			sec_wireless_set_tx_enable(battery, false);
		}
	} else if (battery->tx_retry_case & SEC_BAT_TX_RETRY_OCP) {
		ts = ktime_to_timespec64(ktime_get_boottime());
		if (ts.tv_sec >= battery->tx_ocp_start_time) {
			battery->tx_ocp_passed_time = ts.tv_sec - battery->tx_ocp_start_time;
		} else {
			battery->tx_ocp_passed_time = 0xFFFFFFFF - battery->tx_ocp_start_time
				+ ts.tv_sec;
		}
		pr_info("@Tx_Mode %s: already ocp, passed time(%ld)\n",
				__func__, battery->tx_ocp_passed_time);

		if (battery->tx_ocp_passed_time >= 60) {
			pr_info("@Tx_Mode %s: after 1min\n", __func__);
			if (battery->wc_tx_enable) {
				if (battery->wc_rx_connected) {
					pr_info("@Tx_Mode %s: RX Dev, Keep TX ON status (cancel ocp)\n", __func__);
				} else {
					pr_info("@Tx_Mode %s: NO RX Dev, TX OFF (cancel ocp)\n", __func__);
					sec_bat_set_tx_event(battery,
							BATT_TX_EVENT_WIRELESS_TX_OCP, BATT_TX_EVENT_WIRELESS_TX_OCP);
					sec_wireless_set_tx_enable(battery, false);
				}
			} else {
				pr_info("@Tx_Mode %s: Keep TX OFF status (cancel ocp)\n", __func__);
			}
			battery->tx_retry_case &= ~SEC_BAT_TX_RETRY_OCP;
			battery->tx_ocp_start_time = 0;
			battery->tx_ocp_cnt = 0;
		}
	}
}

void sec_bat_check_wc_re_auth(struct sec_battery_info *battery)
{
	union power_supply_propval value = {0, };

	psy_do_property(battery->pdata->wireless_charger_name, get,
		POWER_SUPPLY_EXT_PROP_WIRELESS_TX_ID, value);

	pr_info("%s: tx_id(0x%x), cable(%d), soc(%d)\n", __func__,
		value.intval, battery->cable_type, battery->capacity);

	if ((value.intval >= WC_PAD_ID_AUTH_PAD) && (value.intval <= WC_PAD_ID_AUTH_PAD_END)
		&& (battery->cable_type == SEC_BATTERY_CABLE_HV_WIRELESS)
		&& (battery->capacity >= 5)) {
		pr_info("%s: EPT Unknown for re-auth\n", __func__);

		value.intval = 1;
		psy_do_property(battery->pdata->wireless_charger_name, set,
			POWER_SUPPLY_EXT_PROP_WC_EPT_UNKNOWN, value);

		battery->wc_auth_retried = true;
	} else if ((value.intval >= WC_PAD_ID_AUTH_PAD) && (value.intval <= WC_PAD_ID_AUTH_PAD_END)
		&& (battery->cable_type == SEC_BATTERY_CABLE_HV_WIRELESS_20)) {
		pr_info("%s: auth success\n", __func__);
		battery->wc_auth_retried = true;
	} else if ((value.intval < WC_PAD_ID_AUTH_PAD) || (value.intval > WC_PAD_ID_AUTH_PAD_END)) {
		pr_info("%s: re-auth is unnecessary\n", __func__);
		battery->wc_auth_retried = true;
	}
}

#if defined(CONFIG_WIRELESS_TX_MODE)
void sec_bat_check_tx_mode(struct sec_battery_info *battery)
{

	if (battery->wc_tx_enable) {
		pr_info("@Tx_Mode %s: tx_retry(0x%x), tx_switch(0x%x)",
			__func__, battery->tx_retry_case, battery->tx_switch_mode);
#if !defined(CONFIG_SEC_FACTORY)
		sec_bat_check_tx_battery_drain(battery);
		sec_bat_check_tx_temperature(battery);

		if ((battery->wc_rx_type == SS_PHONE) ||
				(battery->wc_rx_type == OTHER_DEV) ||
				(battery->wc_rx_type == SS_BUDS))
			sec_bat_check_tx_current(battery);
#endif
		sec_bat_txpower_calc(battery);
		sec_bat_handle_tx_misalign(battery, false);
		sec_bat_handle_tx_ocp(battery, false);
		battery->tx_retry_case &= ~BATT_TX_EVENT_WIRELESS_TX_AC_MISSING;

		if (battery->tx_switch_mode != TX_SWITCH_MODE_OFF && battery->tx_switch_start_soc != 0)
			sec_bat_check_tx_switch_mode(battery);

	} else if (battery->tx_retry_case != SEC_BAT_TX_RETRY_NONE) {
		pr_info("@Tx_Mode %s: tx_retry(0x%x)", __func__, battery->tx_retry_case);
#if !defined(CONFIG_SEC_FACTORY)
		sec_bat_check_tx_temperature(battery);
#endif
		sec_bat_handle_tx_misalign(battery, false);
		sec_bat_handle_tx_ocp(battery, false);
		battery->tx_retry_case &= ~BATT_TX_EVENT_WIRELESS_TX_AC_MISSING;
	}
}
#endif

void sec_bat_wc_cv_mode_check(struct sec_battery_info *battery)
{
	union power_supply_propval value = {0, };
	int is_otg_on = 0;

	psy_do_property(battery->pdata->charger_name, get,
			POWER_SUPPLY_EXT_PROP_CHARGE_OTG_CONTROL, value);
	is_otg_on = value.intval;

	pr_info("%s: battery->wc_cv_mode = %d, otg(%d), cable_type(%d)\n", __func__,
		battery->wc_cv_mode, is_otg_on, battery->cable_type);

	if (battery->capacity >= battery->pdata->wireless_cc_cv && !is_otg_on) {
		battery->wc_cv_mode = true;
		if (is_nv_wireless_type(battery->cable_type)) {
			if ((battery->cable_type == SEC_BATTERY_CABLE_WIRELESS ||
				battery->cable_type == SEC_BATTERY_CABLE_WIRELESS_STAND ||
				battery->cable_type == SEC_BATTERY_CABLE_WIRELESS_TX)) {
				value.intval = WIRELESS_CLAMP_ENABLE;
				psy_do_property(battery->pdata->wireless_charger_name, set,
					POWER_SUPPLY_EXT_PROP_WIRELESS_RX_CONTROL, value);
			}
		}
		/* Change FOD values for CV mode */
		value.intval = POWER_SUPPLY_PROP_CONSTANT_CHARGE_VOLTAGE;
		psy_do_property(battery->pdata->wireless_charger_name, set,
			POWER_SUPPLY_PROP_STATUS, value);
	}
}
EXPORT_SYMBOL_KUNIT(sec_bat_wc_cv_mode_check);

static int is_5v_charger(struct sec_battery_info *battery)
{
	if ((is_pd_wire_type(battery->wire_status) && battery->pd_list.max_pd_count > 1)
			|| is_hv_wire_12v_type(battery->wire_status)
			|| is_hv_wire_type(battery->wire_status)
			|| (battery->wire_status == SEC_BATTERY_CABLE_HV_TA_CHG_LIMIT)
			|| (battery->wire_status == SEC_BATTERY_CABLE_PREPARE_TA)
			|| (battery->current_event & SEC_BAT_CURRENT_EVENT_AFC)) {
		return false;
	} else if (is_wired_type(battery->wire_status)) {
		return true;
	}
	return false;
}

void sec_bat_run_wpc_tx_work(struct sec_battery_info *battery, int work_delay)
{
	cancel_delayed_work(&battery->wpc_tx_work);
	__pm_stay_awake(battery->wpc_tx_ws);
	queue_delayed_work(battery->monitor_wqueue,
			&battery->wpc_tx_work, msecs_to_jiffies(work_delay));
}

void sec_bat_set_forced_tx_switch_mode(struct sec_battery_info *battery, int mode)
{
	union power_supply_propval value = {0, };

	pr_info("@Tx_Mode %s : change tx switch mode (%d -> %d)\n",
		__func__, battery->tx_switch_mode, mode);

	switch (mode) {
	case TX_SWITCH_MODE_OFF:
	case TX_SWITCH_CHG_ONLY:
	case TX_SWITCH_UNO_ONLY:
		pr_info("@Tx_Mode %s : tt was ignored now. need to implement\n", __func__);
		break;
	case TX_SWITCH_UNO_FOR_GEAR:
		battery->tx_switch_mode = mode;

		value.intval = 0;
		psy_do_property(battery->pdata->wireless_charger_name, set,
			POWER_SUPPLY_EXT_PROP_FORCED_PHM_CTRL, value);

		if (!battery->buck_cntl_by_tx) {
			battery->buck_cntl_by_tx = true;
			sec_vote(battery->chgen_vote, VOTER_WC_TX,
				true, SEC_BAT_CHG_MODE_BUCK_OFF);
		}

		battery->tx_switch_mode_change = false;
		sec_bat_run_wpc_tx_work(battery,
			battery->pdata->tx_aov_delay_phm_escape);
		break;
	default:
		break;
	}
}

static void sec_bat_wpc_tx_iv(struct sec_vote *target_vote, int target_voltage, bool need_refresh)
{
	int voter_val = get_sec_vote_result(target_vote);

	pr_info("@Tx_mode %s : present iv = %d. target iv = %d\n", __func__, voter_val, target_voltage);

	sec_vote(target_vote, VOTER_WC_TX, true, target_voltage);

	if (need_refresh && (voter_val == target_voltage))
		sec_vote_refresh(target_vote);
}

void sec_bat_wpc_tx_work_content(struct sec_battery_info *battery)
{
	union power_supply_propval value = {0, };

	dev_info(battery->dev, "@Tx_Mode %s: Start\n", __func__);
	if (!battery->wc_tx_enable) {
		pr_info("@Tx_Mode %s : exit wpc_tx_work. Because Tx is already off\n", __func__);
		goto end_of_tx_work;
	}
	if (battery->pdata->tx_5v_disable && is_5v_charger(battery)) {
		pr_info("@Tx_Mode %s : 5V charger(%d) connected, disable TX\n", __func__, battery->cable_type);
		sec_bat_set_tx_event(battery, BATT_TX_EVENT_WIRELESS_TX_5V_TA, BATT_TX_EVENT_WIRELESS_TX_5V_TA);
		sec_wireless_set_tx_enable(battery, false);
		goto end_of_tx_work;
	}

	switch (battery->wc_rx_type) {
	case NO_DEV:
		if (battery->pdata->charging_limit_by_tx_check)
			sec_vote(battery->fcc_vote, VOTER_WC_TX, true,
				battery->pdata->charging_limit_current_by_tx);

		if (is_pd_apdo_wire_type(battery->wire_status) && battery->pd_list.now_isApdo) {
			pr_info("@Tx_Mode %s: PD30 source charnge (APDO -> Fixed). Because Tx Start.\n", __func__);
			if (battery->wc_tx_enable && battery->buck_cntl_by_tx)
				sec_vote(battery->chgen_vote, VOTER_WC_TX, true, SEC_BAT_CHG_MODE_BUCK_OFF);
			break;
		} else if (is_hv_wire_type(battery->wire_status) ||
				(is_pd_wire_type(battery->wire_status) && battery->hv_pdo)) {
			pr_info("@Tx_Mode %s: change iv(9V -> 5V). Because Tx Start.\n", __func__);

			sec_bat_wpc_tx_iv(battery->iv_vote, SEC_INPUT_VOLTAGE_5V, true);

			break;
		}

		if (!battery->buck_cntl_by_tx) {
			battery->buck_cntl_by_tx = true;
			sec_vote(battery->chgen_vote, VOTER_WC_TX, true, SEC_BAT_CHG_MODE_BUCK_OFF);
		}

		if (!battery->uno_en) {
			battery->buck_cntl_by_tx = true;
			sec_vote(battery->chgen_vote, VOTER_WC_TX, true, SEC_BAT_CHG_MODE_BUCK_OFF);
			sec_bat_wireless_uno_cntl(battery, true);
		}

		sec_bat_wireless_vout_cntl(battery, WC_TX_VOUT_5000MV);
		sec_bat_wireless_iout_cntl(battery, battery->pdata->tx_uno_iout, battery->pdata->tx_mfc_iout_gear);

		break;
	case SS_GEAR:
		{
			if (battery->pdata->charging_limit_by_tx_check)
				sec_vote(battery->fcc_vote, VOTER_WC_TX, true,
					battery->pdata->charging_limit_current_by_tx_gear);

#if defined(CONFIG_TX_GEAR_PHM_VOUT_CTRL)
			if (battery->pdata->phm_vout_ctrl_dev & SEC_WIRELESS_PHM_VOUT_CTRL_GEAR && !battery->wc_tx_phm_mode) {
				if ((battery->wire_status == SEC_BATTERY_CABLE_HV_TA_CHG_LIMIT) ||
					(is_pd_wire_type(battery->wire_status) && !battery->hv_pdo)) {
					pr_info("@Tx_Mode %s : charging voltage change(5V -> 9V)\n", __func__);
					/* prevent ocp */
					if (!battery->buck_cntl_by_tx) {
						sec_bat_wireless_vout_cntl(battery, WC_TX_VOUT_5000MV);
						sec_bat_wireless_iout_cntl(battery, battery->pdata->tx_uno_iout, 1000);
						battery->buck_cntl_by_tx = true;
						sec_vote(battery->chgen_vote, VOTER_WC_TX,
							true, SEC_BAT_CHG_MODE_BUCK_OFF);
						sec_bat_run_wpc_tx_work(battery, 500);
						return;
					} else if (battery->wc_tx_vout < WC_TX_VOUT_8500MV) {
						sec_bat_wireless_vout_cntl(battery,
							battery->wc_tx_vout + WC_TX_VOUT_STEP_AOV);
						sec_bat_run_wpc_tx_work(battery, 500);
						return;
					}
					sec_bat_wpc_tx_iv(battery->iv_vote, SEC_INPUT_VOLTAGE_9V, true);
					break;
				}
			} else {
#endif
				if (is_hv_wire_type(battery->wire_status) ||
					(is_pd_wire_type(battery->wire_status) && battery->hv_pdo)) {
					pr_info("@Tx_Mode %s : change iv(9V -> 5V).\n", __func__);
					sec_bat_wpc_tx_iv(battery->iv_vote, SEC_INPUT_VOLTAGE_5V, true);
					break;
				}
#if defined(CONFIG_TX_GEAR_PHM_VOUT_CTRL)
			}
#endif

#if defined(CONFIG_TX_GEAR_AOV)
			if (is_wired_type(battery->wire_status)) {
				if (battery->wc_found_gear_freq && (battery->wc_tx_vout == WC_TX_VOUT_5000MV)) {
					pr_info("@Tx_mode: Switch Mode Stop. vout 5V charging\n");
					battery->tx_switch_mode = TX_SWITCH_MODE_OFF;
					battery->tx_switch_mode_change = false;
					sec_bat_wireless_iout_cntl(battery,
						battery->pdata->tx_uno_iout, battery->pdata->tx_mfc_iout_gear);
					sec_bat_wireless_vout_cntl(battery, WC_TX_VOUT_5000MV);

					if (battery->buck_cntl_by_tx) {
						battery->buck_cntl_by_tx = false;
						sec_vote(battery->chgen_vote, VOTER_WC_TX, false, 0);
					}
				} else if (battery->tx_switch_mode == TX_SWITCH_MODE_OFF) {
					pr_info("@Tx_mode: Switch Mode Start\n");
					battery->tx_switch_mode = TX_SWITCH_UNO_ONLY;
					battery->tx_switch_start_soc = battery->capacity;

					if (!battery->buck_cntl_by_tx) {
						battery->buck_cntl_by_tx = true;
						sec_vote(battery->chgen_vote, VOTER_WC_TX,
							true, SEC_BAT_CHG_MODE_BUCK_OFF);
					}

					value.intval = 0;
					psy_do_property(battery->pdata->wireless_charger_name, set,
						POWER_SUPPLY_EXT_PROP_FORCED_PHM_CTRL, value);
				} else if (battery->tx_switch_mode_change == true) {
					if (battery->tx_switch_mode != TX_SWITCH_UNO_FOR_GEAR)
						battery->tx_switch_start_soc = battery->capacity;

					pr_info("@Tx_mode: Switch Mode Change(%d -> %d), buck_cntl_by_tx(%d)\n",
						battery->tx_switch_mode,
						battery->tx_switch_mode == TX_SWITCH_UNO_ONLY ?
						TX_SWITCH_CHG_ONLY : (battery->tx_switch_mode == TX_SWITCH_UNO_FOR_GEAR ?
						TX_SWITCH_CHG_ONLY : TX_SWITCH_UNO_ONLY), battery->buck_cntl_by_tx);

					if ((battery->tx_switch_mode == TX_SWITCH_UNO_ONLY)
						|| (battery->tx_switch_mode == TX_SWITCH_UNO_FOR_GEAR)) {
						battery->tx_switch_mode = TX_SWITCH_CHG_ONLY;
						battery->wc_tx_freq = 0;
						battery->wc_tx_adaptive_vout = false;
						battery->wc_found_gear_freq = false;

						value.intval = 1;
						psy_do_property(battery->pdata->wireless_charger_name, set,
							POWER_SUPPLY_EXT_PROP_FORCED_PHM_CTRL, value);

						sec_bat_wireless_iout_cntl(battery,
							battery->pdata->tx_uno_iout, battery->pdata->tx_mfc_iout_gear);
						sec_bat_wireless_vout_cntl(battery, WC_TX_VOUT_5000MV);

						if (battery->buck_cntl_by_tx) {
							battery->buck_cntl_by_tx = false;
							sec_vote(battery->chgen_vote, VOTER_WC_TX, false, 0);
						}

						battery->tx_switch_mode_change = false;
					} else if (battery->tx_switch_mode == TX_SWITCH_CHG_ONLY) {
						battery->tx_switch_mode = TX_SWITCH_UNO_ONLY;

						value.intval = 0;
						psy_do_property(battery->pdata->wireless_charger_name, set,
							POWER_SUPPLY_EXT_PROP_FORCED_PHM_CTRL, value);

						if (!battery->buck_cntl_by_tx) {
							battery->buck_cntl_by_tx = true;
							sec_vote(battery->chgen_vote, VOTER_WC_TX,
								true, SEC_BAT_CHG_MODE_BUCK_OFF);
						}

						battery->tx_switch_mode_change = false;
						sec_bat_run_wpc_tx_work(battery,
							battery->pdata->tx_aov_delay_phm_escape);
						return;
					}
				} else if (battery->wc_found_gear_freq
					&& battery->tx_switch_mode == TX_SWITCH_UNO_FOR_GEAR) {
					value.intval = true;
					psy_do_property(battery->pdata->wireless_charger_name, set,
						POWER_SUPPLY_EXT_PROP_WIRELESS_SEND_FSK, value);
					pr_info("@Tx_Mode %s: send FSK. need to switch to CHG_ONLY mode\n", __func__);
					battery->tx_switch_mode_change = true;
					sec_bat_run_wpc_tx_work(battery, 2000);
					return;
				}
			} else if (!battery->buck_cntl_by_tx) {
				battery->buck_cntl_by_tx = true;
				sec_vote(battery->chgen_vote, VOTER_WC_TX, true, SEC_BAT_CHG_MODE_BUCK_OFF);
			}

			if (battery->tx_switch_mode != TX_SWITCH_CHG_ONLY) {	// Uno Only or Standalone
				if (battery->wc_tx_phm_mode) {
					pr_info("@Tx_Mode %s: gear phm. vout 5V\n", __func__);
					sec_bat_wireless_vout_cntl(battery, WC_TX_VOUT_5000MV);
					battery->wc_tx_adaptive_vout = false;
					battery->wc_found_gear_freq = false;
				} else if (!battery->wc_found_gear_freq) {
					pr_info("@Tx_Mode %s: wc_tx_vout(%d) wc_tx_adaptive_vout(%d)\n",
						__func__, battery->wc_tx_vout, battery->wc_tx_adaptive_vout);
					if (battery->wc_tx_vout < battery->pdata->tx_aov_start_vout
						&& !battery->wc_tx_adaptive_vout) {
						sec_bat_wireless_vout_cntl(battery,
							battery->wc_tx_vout + WC_TX_VOUT_STEP_AOV);

						if (battery->wc_tx_vout == battery->pdata->tx_aov_start_vout)
							sec_bat_run_wpc_tx_work(battery, battery->pdata->tx_aov_delay);
						else
							sec_bat_run_wpc_tx_work(battery, 500);

						return;
					}
					battery->wc_tx_adaptive_vout = true;

					psy_do_property(battery->pdata->wireless_charger_name, get,
						POWER_SUPPLY_EXT_PROP_WIRELESS_OP_FREQ, value);
					battery->wc_tx_freq = value.intval;
					pr_info("@Tx_Mode %s: wc_tx_vout(%d) freq(%d)\n",
						__func__, battery->wc_tx_vout, battery->wc_tx_freq);

					if ((battery->wc_tx_freq < battery->pdata->tx_aov_freq_low)
						&& (battery->wc_tx_vout < WC_TX_VOUT_7500MV)) {
						sec_bat_wireless_vout_cntl(battery,
							battery->wc_tx_vout + WC_TX_VOUT_STEP_AOV);
						sec_bat_run_wpc_tx_work(battery, battery->pdata->tx_aov_delay);
						return;
					} else if ((battery->wc_tx_freq > battery->pdata->tx_aov_freq_high)
								&& (battery->wc_tx_vout > WC_TX_VOUT_5000MV)) {
						sec_bat_wireless_vout_cntl(battery,
							battery->wc_tx_vout - WC_TX_VOUT_STEP_AOV);
						sec_bat_run_wpc_tx_work(battery, battery->pdata->tx_aov_delay);
						return;
					}
					pr_info("@Tx_Mode %s: found freq(%d)\n", __func__, battery->wc_tx_vout);
					battery->wc_found_gear_freq = true;
					if (battery->tx_switch_mode == TX_SWITCH_UNO_FOR_GEAR) {
						pr_info("@Tx_Mode %s: need FSK sending after 5s\n", __func__);
						sec_bat_run_wpc_tx_work(battery, 5000);
						return;
					}
				}
				sec_bat_wireless_iout_cntl(battery, battery->pdata->tx_uno_iout,
					battery->pdata->tx_mfc_iout_gear);
			}
#else
			if (is_wired_type(battery->wire_status) && battery->buck_cntl_by_tx) {
				battery->buck_cntl_by_tx = false;
				sec_vote(battery->chgen_vote, VOTER_WC_TX, false, 0);
			} else if ((battery->wire_status == SEC_BATTERY_CABLE_NONE) && (!battery->buck_cntl_by_tx)) {
				battery->buck_cntl_by_tx = true;
				sec_vote(battery->chgen_vote, VOTER_WC_TX, true, SEC_BAT_CHG_MODE_BUCK_OFF);
			}

			sec_bat_wireless_vout_cntl(battery, battery->pdata->tx_gear_vout);
			sec_bat_wireless_iout_cntl(battery, battery->pdata->tx_uno_iout,
				battery->pdata->tx_mfc_iout_gear);
#endif
		}
		break;
	default: /* SS_BUDS, SS_PHONE, OTHER_DEV */
#if defined(CONFIG_TX_GEAR_PHM_VOUT_CTRL)
		if (battery->pdata->phm_vout_ctrl_dev & SEC_WIRELESS_PHM_VOUT_CTRL_BUDS && battery->wc_tx_phm_mode) {
			pr_info("@Tx_Mode %s : phm on status.\n", __func__);

			if (is_hv_wire_type(battery->wire_status) ||
				(is_pd_wire_type(battery->wire_status) && battery->hv_pdo)) {
				pr_info("@Tx_Mode %s : change iv(9V -> 5V).\n", __func__);
				sec_bat_wpc_tx_iv(battery->iv_vote, SEC_INPUT_VOLTAGE_5V, true);
				battery->prev_tx_phm_mode = battery->wc_tx_phm_mode;
			}
			break;
		} else if (battery->pdata->phm_vout_ctrl_dev & SEC_WIRELESS_PHM_VOUT_CTRL_BUDS && !battery->wc_tx_phm_mode && battery->prev_tx_phm_mode) {
			if ((battery->wire_status == SEC_BATTERY_CABLE_HV_TA_CHG_LIMIT) ||
				(is_pd_wire_type(battery->wire_status) && !battery->hv_pdo)) {
				pr_info("@Tx_Mode %s : keep phm off status concept before tx off\n", __func__);
				break;
			}
		}
#endif

		if ((battery->wire_status == SEC_BATTERY_CABLE_HV_TA_CHG_LIMIT) ||
			(is_pd_wire_type(battery->wire_status) && !battery->hv_pdo)) {
			pr_info("@Tx_Mode %s : change iv(5V -> 9V)\n", __func__);
			sec_bat_wpc_tx_iv(battery->iv_vote, SEC_INPUT_VOLTAGE_9V, true);
			break;
		}

		if (battery->wire_status == SEC_BATTERY_CABLE_NONE) {

			battery->tx_switch_mode = TX_SWITCH_MODE_OFF;
			battery->tx_switch_start_soc = 0;
			battery->tx_switch_mode_change = false;

			if (!battery->buck_cntl_by_tx) {
				battery->buck_cntl_by_tx = true;
				sec_vote(battery->chgen_vote, VOTER_WC_TX, true, SEC_BAT_CHG_MODE_BUCK_OFF);
			}

			sec_bat_wireless_vout_cntl(battery, battery->pdata->tx_uno_vout);
			sec_bat_wireless_iout_cntl(battery,
					battery->pdata->tx_uno_iout, battery->pdata->tx_mfc_iout_phone);
			sec_bat_wireless_minduty_cntl(battery, battery->pdata->tx_minduty_default);
		} else if (is_hv_wire_type(battery->wire_status) ||
				(is_pd_wire_type(battery->wire_status) && battery->hv_pdo)) {

			battery->tx_switch_mode = TX_SWITCH_MODE_OFF;
			battery->tx_switch_start_soc = 0;
			battery->tx_switch_mode_change = false;

			if (battery->buck_cntl_by_tx) {
				battery->buck_cntl_by_tx = false;
				sec_vote(battery->chgen_vote, VOTER_WC_TX, false, 0);
			}

			sec_bat_wireless_iout_cntl(battery,
					battery->pdata->tx_uno_iout, battery->pdata->tx_mfc_iout_phone);
			sec_bat_wireless_minduty_cntl(battery, battery->pdata->tx_minduty_default);
		} else if (is_pd_wire_type(battery->wire_status) && battery->hv_pdo) {

			pr_info("@Tx_Mode %s: PD cable attached. HV PDO(%d)\n", __func__, battery->hv_pdo);

			battery->tx_switch_mode = TX_SWITCH_MODE_OFF;
			battery->tx_switch_start_soc = 0;
			battery->tx_switch_mode_change = false;

			if (battery->buck_cntl_by_tx) {
				battery->buck_cntl_by_tx = false;
				sec_vote(battery->chgen_vote, VOTER_WC_TX, false, 0);
			}

			sec_bat_wireless_iout_cntl(battery,
					battery->pdata->tx_uno_iout, battery->pdata->tx_mfc_iout_phone);

			sec_bat_wireless_minduty_cntl(battery, battery->pdata->tx_minduty_default);
		} else if (is_wired_type(battery->wire_status) && !is_hv_wire_type(battery->wire_status) &&
				(battery->wire_status != SEC_BATTERY_CABLE_HV_TA_CHG_LIMIT)) {
			if (battery->current_event & SEC_BAT_CURRENT_EVENT_AFC)	{
				if (!battery->buck_cntl_by_tx) {
					battery->buck_cntl_by_tx = true;
					sec_vote(battery->chgen_vote, VOTER_WC_TX, true, SEC_BAT_CHG_MODE_BUCK_OFF);
				}

				battery->tx_switch_mode = TX_SWITCH_MODE_OFF;
				battery->tx_switch_start_soc = 0;
				battery->tx_switch_mode_change = false;

				sec_bat_wireless_iout_cntl(battery,
						battery->pdata->tx_uno_iout, battery->pdata->tx_mfc_iout_phone);
				sec_bat_wireless_vout_cntl(battery, battery->pdata->tx_uno_vout);
				sec_bat_wireless_minduty_cntl(battery, battery->pdata->tx_minduty_default);
			} else if (battery->tx_switch_mode == TX_SWITCH_MODE_OFF) {
				battery->tx_switch_mode = TX_SWITCH_UNO_ONLY;
				battery->tx_switch_start_soc = battery->capacity;
				if (!battery->buck_cntl_by_tx) {
					battery->buck_cntl_by_tx = true;
					sec_vote(battery->chgen_vote, VOTER_WC_TX, true, SEC_BAT_CHG_MODE_BUCK_OFF);
				}

				sec_bat_wireless_iout_cntl(battery,
						battery->pdata->tx_uno_iout, battery->pdata->tx_mfc_iout_phone);
				sec_bat_wireless_vout_cntl(battery, battery->pdata->tx_uno_vout);
				sec_bat_wireless_minduty_cntl(battery, battery->pdata->tx_minduty_default);

			} else if (battery->tx_switch_mode_change == true) {
				battery->tx_switch_start_soc = battery->capacity;

				pr_info("@Tx_mode: Switch Mode Change(%d -> %d)\n",
						battery->tx_switch_mode,
						battery->tx_switch_mode == TX_SWITCH_UNO_ONLY ?
						TX_SWITCH_CHG_ONLY : TX_SWITCH_UNO_ONLY);

				if (battery->tx_switch_mode == TX_SWITCH_UNO_ONLY) {
					battery->tx_switch_mode = TX_SWITCH_CHG_ONLY;

					sec_bat_wireless_iout_cntl(battery,
							battery->pdata->tx_uno_iout,
							battery->pdata->tx_mfc_iout_phone_5v);
					sec_bat_wireless_vout_cntl(battery, WC_TX_VOUT_5000MV);
					sec_bat_wireless_minduty_cntl(battery, battery->pdata->tx_minduty_5V);

					if (battery->buck_cntl_by_tx) {
						battery->buck_cntl_by_tx = false;
						sec_vote(battery->chgen_vote, VOTER_WC_TX, false, 0);
					}

				} else if (battery->tx_switch_mode == TX_SWITCH_CHG_ONLY) {
					battery->tx_switch_mode = TX_SWITCH_UNO_ONLY;

					if (!battery->buck_cntl_by_tx) {
						battery->buck_cntl_by_tx = true;
						sec_vote(battery->chgen_vote,
								VOTER_WC_TX, true, SEC_BAT_CHG_MODE_BUCK_OFF);
					}

					sec_bat_wireless_iout_cntl(battery,
							battery->pdata->tx_uno_iout,
							battery->pdata->tx_mfc_iout_phone);
					sec_bat_wireless_vout_cntl(battery, battery->pdata->tx_uno_vout);
					sec_bat_wireless_minduty_cntl(battery, battery->pdata->tx_minduty_default);

					value.intval = true;
					psy_do_property(battery->pdata->wireless_charger_name, set,
							POWER_SUPPLY_EXT_PROP_WIRELESS_SEND_FSK, value);

				}
				battery->tx_switch_mode_change = false;
			}

		}
		break;
	}
end_of_tx_work:
	dev_info(battery->dev, "@Tx_Mode %s End\n", __func__);
	__pm_relax(battery->wpc_tx_ws);
}

#define PD_RETRY_DELAY 50
#define PD_RETRY_CNT 13
#define AFC_RETRY_DELAY 100
#define AFC_RETRY_CNT 10
void sec_bat_wpc_tx_en_work_content(struct sec_battery_info *battery)
{
	int selected_pdo;
	static unsigned int pd_cnt, afc_cnt;

	pr_info("@Tx_Mode %s: tx %s\n", __func__,
		battery->wc_tx_enable ? "on" : "off");

	battery->tx_minduty = battery->pdata->tx_minduty_default;
	battery->tx_switch_mode = TX_SWITCH_MODE_OFF;
	battery->tx_switch_start_soc = 0;
	battery->tx_switch_mode_change = false;
#if defined(CONFIG_TX_GEAR_AOV)
	battery->wc_tx_freq = 0;
	battery->wc_tx_adaptive_vout = false;
	battery->wc_found_gear_freq = false;
#endif

	if (battery->wc_tx_enable) {
		/* set tx event */
		sec_bat_set_tx_event(battery, BATT_TX_EVENT_WIRELESS_TX_STATUS,
			(BATT_TX_EVENT_WIRELESS_TX_STATUS | BATT_TX_EVENT_WIRELESS_TX_RETRY));

		if (is_pd_wire_type(battery->wire_status)) {
			sec_pd_get_selected_pdo(&selected_pdo);
			if (selected_pdo == battery->now_pd_index) {
				if (battery->pd_list.pd_info[battery->now_pd_index].max_voltage
					== SEC_INPUT_VOLTAGE_5V) {
					pr_info("@Tx_Mode %s: 5V PDO. Tx Start\n", __func__);
					sec_bat_wpc_tx_iv(battery->iv_vote, SEC_INPUT_VOLTAGE_5V, false);
					sec_bat_run_wpc_tx_work(battery, 0);
				} else {
					pr_info("@Tx_Mode %s: HV PDO. Restart work after %dms\n",
						__func__, PD_RETRY_DELAY);
					sec_bat_wpc_tx_iv(battery->iv_vote, SEC_INPUT_VOLTAGE_5V, false);
					__pm_stay_awake(battery->wpc_tx_en_ws);
					queue_delayed_work(battery->monitor_wqueue,
						&battery->wpc_tx_en_work, msecs_to_jiffies(PD_RETRY_DELAY));
					return;
				}
			} else {
				if (pd_cnt >= PD_RETRY_CNT) {
					pr_info("@Tx_Mode %s: Already request 5V, but no response\n", __func__);
					sec_bat_wpc_tx_iv(battery->iv_vote, SEC_INPUT_VOLTAGE_5V, true);
					sec_bat_run_wpc_tx_work(battery, PD_RETRY_DELAY);
					pd_cnt = 0;
				} else {
					pr_info("@Tx_Mode %s: Maybe PDO will change. Restart work after %dms\n",
						__func__, PD_RETRY_DELAY);
					__pm_stay_awake(battery->wpc_tx_en_ws);
					queue_delayed_work(battery->monitor_wqueue,
						&battery->wpc_tx_en_work, msecs_to_jiffies(PD_RETRY_DELAY));
					pd_cnt++;
				}
				return;
			}
		} else if (is_hv_wire_type(battery->wire_status)) {
			if (afc_cnt >= AFC_RETRY_CNT) {
				pr_info("@Tx_Mode %s: Already request 5V, but still 9V\n", __func__);
				sec_bat_wpc_tx_iv(battery->iv_vote, SEC_INPUT_VOLTAGE_5V, true);
				sec_bat_run_wpc_tx_work(battery, AFC_RETRY_DELAY);
				afc_cnt = 0;
			} else {
				pr_info("@Tx_Mode %s: AFC 9V. Restart work after %dms\n", __func__, AFC_RETRY_DELAY);
				sec_bat_wpc_tx_iv(battery->iv_vote, SEC_INPUT_VOLTAGE_5V, false);
				__pm_stay_awake(battery->wpc_tx_en_ws);
				queue_delayed_work(battery->monitor_wqueue,
					&battery->wpc_tx_en_work, msecs_to_jiffies(AFC_RETRY_DELAY));
				afc_cnt++;
				return;
			}
		} else {
			pr_info("@Tx_Mode %s: 5V or NONE. Tx Start\n", __func__);
			sec_bat_wpc_tx_iv(battery->iv_vote, SEC_INPUT_VOLTAGE_5V, false);
			sec_bat_run_wpc_tx_work(battery, 0);
		}

		pr_info("@Tx_Mode %s: TX Power Calculation start.\n", __func__);
		queue_delayed_work(battery->monitor_wqueue,
				&battery->wpc_txpower_calc_work, 0);
	} else {
		cancel_delayed_work(&battery->wpc_tx_work);
		cancel_delayed_work(&battery->wpc_txpower_calc_work);
		__pm_relax(battery->wpc_tx_ws);

		sec_bat_wireless_minduty_cntl(battery, battery->pdata->tx_minduty_default);
		battery->wc_rx_type = NO_DEV;
		battery->wc_rx_connected = false;
		battery->tx_uno_iout = 0;
		battery->tx_mfc_iout = 0;
		battery->buck_cntl_by_tx = false;
		sec_bat_wireless_uno_cntl(battery, false);
		sec_bat_wireless_vout_cntl(battery, WC_TX_VOUT_5000MV);
		sec_vote(battery->chgen_vote, VOTER_WC_TX, false, 0);
		sec_vote(battery->fcc_vote, VOTER_WC_TX, false, 0);
		/* for 1) not supporting DC and charging bia PD20/DC on Tx */
		/*     2) supporting DC and charging bia PD20 on Tx */
		sec_vote(battery->iv_vote, VOTER_WC_TX, false, 0);
		pd_cnt = 0;
		afc_cnt = 0;
	}

	pr_info("@Tx_Mode %s Done\n", __func__);
	__pm_relax(battery->wpc_tx_en_ws);
}

void sec_wireless_set_tx_enable(struct sec_battery_info *battery, bool wc_tx_enable)
{
	pr_info("@Tx_Mode %s: TX Power enable ? (%d)\n", __func__, wc_tx_enable);

	if (battery->pdata->tx_5v_disable && wc_tx_enable && is_5v_charger(battery)) {
		pr_info("@Tx_Mode %s : 5V charger(%d) connected, do not turn on TX\n", __func__, battery->cable_type);
		sec_bat_set_tx_event(battery, BATT_TX_EVENT_WIRELESS_TX_5V_TA, BATT_TX_EVENT_WIRELESS_TX_5V_TA);
		return;
	}

	battery->wc_tx_enable = wc_tx_enable;
	battery->wc_tx_phm_mode = false;
	battery->prev_tx_phm_mode = false;

	cancel_delayed_work(&battery->wpc_tx_en_work);
	__pm_stay_awake(battery->wpc_tx_en_ws);
	queue_delayed_work(battery->monitor_wqueue,
		&battery->wpc_tx_en_work, 0);
}
EXPORT_SYMBOL(sec_wireless_set_tx_enable);
