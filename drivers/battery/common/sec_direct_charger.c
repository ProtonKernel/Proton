/*
 *  sec_direct_charger.c
 *  Samsung Mobile Charger Driver
 *
 *  Copyright (C) 2020 Samsung Electronics
 *
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */
#define DEBUG

#include "sec_direct_charger.h"

#if IS_ENABLED(CONFIG_SEC_ABC)
#include <linux/sti/abc_common.h>
#endif

char *sec_direct_chg_mode_str[] = {
	"OFF", //SEC_DIRECT_CHG_MODE_DIRECT_OFF
	"CHECK_VBAT", //SEC_DIRECT_CHG_MODE_DIRECT_CHECK_VBAT
	"PRESET", //SEC_DIRECT_CHG_MODE_DIRECT_PRESET
	"ON_ADJUST", // SEC_DIRECT_CHG_MODE_DIRECT_ON_ADJUST
	"ON", //SEC_DIRECT_CHG_MODE_DIRECT_ON
	"DONE", //SEC_DIRECT_CHG_MODE_DIRECT_DONE
};

char *sec_direct_charger_mode_str[] = {
	"Buck-Off",
	"Charging-Off",
	"Charging-On",
	"OTG-On",
	"OTG-Off",
	"UNO-On",
	"UNO-Off",
	"UNO-Only",
	"Not-Set",
	"Max",
};

void sec_direct_chg_monitor(struct sec_direct_charger_info *charger)
{
	if (charger->charging_source == SEC_CHARGING_SOURCE_DIRECT) {
		pr_info("%s: Src(%s), direct(%s), switching(%s), Imax(%dmA), Ichg(%dmA), dc_input(%dmA)\n",
			__func__, charger->charging_source ? "DIRECT" : "SWITCHING",
			sec_direct_charger_mode_str[charger->charger_mode_direct],
			sec_direct_charger_mode_str[charger->charger_mode_main],
			charger->input_current, charger->charging_current, charger->dc_input_current);
	}
}

static bool sec_direct_chg_set_direct_charge(
		struct sec_direct_charger_info *charger, unsigned int charger_mode)
{
	union power_supply_propval value = {0,};

	if (charger->ta_alert_wa) {
		psy_do_property("battery", get,
				POWER_SUPPLY_EXT_PROP_DIRECT_TA_ALERT, value);
		charger->ta_alert_mode =  value.intval;
	}

	if (charger->charger_mode_direct == charger_mode && !(charger->dc_retry_cnt) &&
		(charger->ta_alert_mode == OCP_NONE)) {
		pr_info("%s: charger_mode is same(%s)\n", __func__,
			sec_direct_charger_mode_str[charger->charger_mode_direct]);
		return false;
	}

	pr_info("%s: charger_mode(%s->%s)\n", __func__,
		sec_direct_charger_mode_str[charger->charger_mode_direct],
		sec_direct_charger_mode_str[charger_mode]);
	charger->charger_mode_direct = charger_mode;

	if (charger_mode == SEC_BAT_CHG_MODE_CHARGING)
		value.intval = true;
	else
		value.intval = false;

	psy_do_property(charger->pdata->direct_charger_name, set,
		POWER_SUPPLY_EXT_PROP_CHARGING_ENABLED, value);

	return true;
}

static bool sec_direct_chg_set_switching_charge(
		struct sec_direct_charger_info *charger, unsigned int charger_mode)
{
	union power_supply_propval value = {0,};

	pr_info("%s: charger_mode(%s->%s)\n", __func__,
		sec_direct_charger_mode_str[charger->charger_mode_main],
		sec_direct_charger_mode_str[charger_mode]);

	charger->charger_mode_main = charger_mode;

	value.intval = charger_mode;
	psy_do_property(charger->pdata->main_charger_name, set,
		POWER_SUPPLY_EXT_PROP_CHARGING_ENABLED, value);

	return true;
}

static int sec_direct_chg_check_charging_source(struct sec_direct_charger_info *charger)
{
	union power_supply_propval value = {0,};
#if defined(CONFIG_DUAL_BATTERY_CELL_SENSING)
	union power_supply_propval value2 = {0,};
#endif

	pr_info("%s: dc_retry_cnt(%d)\n", __func__, charger->dc_retry_cnt);

	if (charger->dc_err) {
		if (charger->ta_alert_wa) {
			psy_do_property("battery", get,
					POWER_SUPPLY_EXT_PROP_DIRECT_TA_ALERT, value);
			charger->ta_alert_mode =  value.intval;
		}

		pr_info("%s: dc_err(%d), ta_alert_mode(%d)\n", __func__, charger->dc_err, charger->ta_alert_mode);
		value.intval = SEC_BAT_CURRENT_EVENT_DC_ERR;
		psy_do_property("battery", set, POWER_SUPPLY_EXT_PROP_CURRENT_EVENT, value);
		if (!charger->ta_alert_wa || (charger->ta_alert_mode == OCP_NONE)) {
			pr_info("%s:  S/C was selected! ta_alert_mode(%d)\n", __func__, charger->ta_alert_mode);
			return SEC_CHARGING_SOURCE_SWITCHING;
		}
	}

	psy_do_property("battery", get, POWER_SUPPLY_PROP_STATUS, value);
	charger->batt_status = value.intval;

	psy_do_property("battery", get, POWER_SUPPLY_PROP_CAPACITY, value);
	charger->capacity = value.intval;

#if defined(CONFIG_WIRELESS_TX_MODE)
	/* check TX enable*/
	psy_do_property("battery", get, POWER_SUPPLY_EXT_PROP_WIRELESS_TX_ENABLE, value);
	charger->wc_tx_enable = value.intval;
	if (charger->wc_tx_enable) {
		pr_info("@TX_Mode %s: Source Switching charger during Tx mode\n", __func__);
		return SEC_CHARGING_SOURCE_SWITCHING;
	}
#endif

	/* check Tbat temperature */
	psy_do_property("battery", get, POWER_SUPPLY_PROP_TEMP, value);
	if (value.intval <= charger->pdata->dchg_temp_low_threshold ||
			value.intval >= charger->pdata->dchg_temp_high_threshold) {
		pr_info("%s:  S/C was selected! Tbat(%d)\n", __func__, value.intval);
		return SEC_CHARGING_SOURCE_SWITCHING;
	}

	/* check mix limit */
	psy_do_property("battery", get, POWER_SUPPLY_EXT_PROP_MIX_LIMIT, value);
	if (value.intval) {
		pr_info("%s:  S/C was selected! mix_limit(%d)\n", __func__, value.intval);
		return SEC_CHARGING_SOURCE_SWITCHING;
	}

#if IS_ENABLED(CONFIG_DUAL_BATTERY)
	/* check Tsub temperature */
	psy_do_property("battery", get, POWER_SUPPLY_EXT_PROP_SUB_TEMP, value);
	if (value.intval <= charger->pdata->dchg_temp_low_threshold ||
			value.intval >= charger->pdata->dchg_temp_high_threshold) {
		pr_info("%s:  S/C was selected! Tsub(%d)\n", __func__, value.intval);
		return SEC_CHARGING_SOURCE_SWITCHING;
	}
#endif

	/* check current event */
	psy_do_property("battery", get, POWER_SUPPLY_EXT_PROP_CURRENT_EVENT, value);
	if (value.intval & SEC_BAT_CURRENT_EVENT_SWELLING_MODE ||
			value.intval & SEC_BAT_CURRENT_EVENT_HV_DISABLE ||
			value.intval & SEC_BAT_CURRENT_EVENT_SIOP_LIMIT ||
			value.intval & SEC_BAT_CURRENT_EVENT_SEND_UVDM ||
			(value.intval & SEC_BAT_CURRENT_EVENT_DC_ERR && charger->ta_alert_mode == OCP_NONE)) {
		pr_info("%s:  S/C was selected! BAT_CURRENT_EVENT(0x%x)\n", __func__, value.intval);
		return SEC_CHARGING_SOURCE_SWITCHING;
	}

	/* check test mode */
	if (charger->test_mode_source == SEC_CHARGING_SOURCE_SWITCHING) {
		pr_info("%s:  S/C was selected! tese_mode_source(%d)\n", __func__, charger->test_mode_source);
		return SEC_CHARGING_SOURCE_SWITCHING;
	}

	/* check apdo */
	psy_do_property("battery", get, POWER_SUPPLY_PROP_ONLINE, value);
	if (!is_pd_apdo_wire_type(charger->cable_type) || !is_pd_apdo_wire_type(value.intval)) {
		pr_info("%s:  S/C was selected! Not APDO(%d, %d)\n",
				__func__, charger->cable_type, value.intval);
		return SEC_CHARGING_SOURCE_SWITCHING;
	}

	/* check battery->status */
	if (charger->batt_status == POWER_SUPPLY_STATUS_FULL ||
		charger->batt_status == POWER_SUPPLY_STATUS_NOT_CHARGING ||
		charger->batt_status == POWER_SUPPLY_STATUS_DISCHARGING) {
		pr_info("%s:  S/C was selected! battery->status(%d)\n",
				__func__, charger->batt_status);
		return SEC_CHARGING_SOURCE_SWITCHING;
	}

	/* check charging status */
	psy_do_property("battery", get, POWER_SUPPLY_EXT_PROP_DIRECT_HAS_APDO, value);
#if defined(CONFIG_DUAL_BATTERY_CELL_SENSING)
	psy_do_property("battery", get,
		POWER_SUPPLY_EXT_PROP_DIRECT_VBAT_CHECK, value2);

	if (charger->direct_chg_done || (charger->capacity >= 95) || !value.intval || value2.intval ||
		charger->store_mode)
#else
	if (charger->direct_chg_done || (charger->capacity >= 95) || !value.intval || charger->store_mode)
#endif
	{
		pr_info("%s:  S/C was selected! dc_done(%s), SoC(%d), has_apdo(%d)\n",
				__func__, charger->direct_chg_done ? "TRUE" : "FALSE",
				charger->capacity, value.intval);
		return SEC_CHARGING_SOURCE_SWITCHING;
	}
	return SEC_CHARGING_SOURCE_DIRECT;
}

static int sec_direct_chg_set_charging_source(struct sec_direct_charger_info *charger,
		unsigned int charger_mode, int charging_source)
{
	union power_supply_propval value = {0,};

	mutex_lock(&charger->charger_mutex);
	if (charging_source == SEC_CHARGING_SOURCE_DIRECT &&
		charger_mode == SEC_BAT_CHG_MODE_CHARGING) {
		sec_direct_chg_set_switching_charge(charger, SEC_BAT_CHG_MODE_BUCK_OFF);
		sec_direct_chg_set_direct_charge(charger, SEC_BAT_CHG_MODE_CHARGING);

		value.intval = SEC_INPUT_VOLTAGE_APDO;
		psy_do_property("battery", set,
				POWER_SUPPLY_EXT_PROP_DIRECT_FIXED_PDO, value);
	} else {
		psy_do_property("battery", get,
					POWER_SUPPLY_EXT_PROP_DIRECT_HV_PDO, value);
		charger->hv_pdo = value.intval;
		if (charger->ta_alert_wa) {
			psy_do_property("battery", get,
					POWER_SUPPLY_EXT_PROP_DIRECT_TA_ALERT, value);
			charger->ta_alert_mode =  value.intval;
		}

		value.intval = SEC_INPUT_VOLTAGE_9V;
		psy_do_property("battery", set,
				POWER_SUPPLY_EXT_PROP_DIRECT_FIXED_PDO, value);

		sec_direct_chg_set_direct_charge(charger, SEC_BAT_CHG_MODE_CHARGING_OFF);
		sec_direct_chg_set_switching_charge(charger, charger_mode);
	}

	charger->charging_source = charging_source;
	mutex_unlock(&charger->charger_mutex);

	return 0;
}

static void sec_direct_chg_set_charge(struct sec_direct_charger_info *charger, unsigned int charger_mode)
{
	int charging_source;

	charger->charger_mode = charger_mode;

	switch (charger->charger_mode) {
		case SEC_BAT_CHG_MODE_BUCK_OFF:
		case SEC_BAT_CHG_MODE_CHARGING_OFF:
			charger->is_charging = false;
			break;
		case SEC_BAT_CHG_MODE_CHARGING:
			charger->is_charging = true;
			break;
	}

	charging_source = sec_direct_chg_check_charging_source(charger);
	sec_direct_chg_set_charging_source(charger, charger_mode, charging_source);
}

static void sec_direct_chg_do_dc_fullcharged(struct sec_direct_charger_info *charger) {
	int charging_source;

	pr_info("%s: called\n", __func__);
	charger->direct_chg_done = true;

	charging_source = sec_direct_chg_check_charging_source(charger);
	sec_direct_chg_set_charging_source(charger, charger->charger_mode, charging_source);
}

static int sec_direct_chg_set_input_current(struct sec_direct_charger_info *charger,
			enum power_supply_property psp, int input_current) {
	union power_supply_propval value = {0,};

	pr_info("%s: called(%dmA)\n", __func__, input_current);

	value.intval = input_current;
	psy_do_property(charger->pdata->main_charger_name, set,
		POWER_SUPPLY_PROP_INPUT_CURRENT_LIMIT, value);

	/* direct charger input current is based on charging current */
	return 0;
}

static int sec_direct_chg_set_charging_current(struct sec_direct_charger_info *charger,
			enum power_supply_property psp, int charging_current) {
	union power_supply_propval value = {0,};
	int charging_source;

	psy_do_property("battery", get,
				POWER_SUPPLY_EXT_PROP_DIRECT_CHARGER_MODE, value);
	charger->now_isApdo = value.intval;

	pr_info("%s: called(%dmA) now_isApdo(%d)\n", __func__, charging_current, charger->now_isApdo);

	/* main charger */
	value.intval = charging_current;
	psy_do_property(charger->pdata->main_charger_name, set,
		POWER_SUPPLY_PROP_CONSTANT_CHARGE_CURRENT, value);

	/* direct charger */
	if (is_pd_apdo_wire_type(charger->cable_type)) {
		charger->dc_charging_current = charging_current;
		charger->dc_input_current = charger->dc_charging_current / 2;

		charging_source = sec_direct_chg_check_charging_source(charger);
		if (charging_source == SEC_CHARGING_SOURCE_DIRECT) {
#if IS_ENABLED(CONFIG_SEC_ABC)
			if (charger->now_isApdo && charger->dc_input_current < 900)
				sec_abc_send_event("MODULE=battery@WARN=dc_current");
#endif
			value.intval = charger->dc_input_current;
			psy_do_property(charger->pdata->direct_charger_name, set,
				POWER_SUPPLY_PROP_INPUT_CURRENT_LIMIT, value);
		}
		sec_direct_chg_set_charging_source(charger, charger->charger_mode, charging_source);
	}

	return 0;
}

static void sec_direct_chg_set_initial_status(struct sec_direct_charger_info *charger)
{
	union power_supply_propval value = {0,};

	if (charger->dc_err) {
		value.intval = SEC_BAT_CURRENT_EVENT_DC_ERR;
		psy_do_property("battery", set,
				POWER_SUPPLY_EXT_PROP_CURRENT_EVENT_CLEAR, value);
	}
	charger->direct_chg_done = false;

	charger->dc_charging_current = charger->pdata->dchg_min_current;
	charger->dc_input_current = charger->dc_charging_current / 2;
	charger->dc_err = false;
	charger->dc_retry_cnt = 0;
	charger->test_mode_source = SEC_CHARGING_SOURCE_DIRECT;
}

static int sec_direct_chg_get_property(struct power_supply *psy,
			    enum power_supply_property psp,
			    union power_supply_propval *val)
{
	struct sec_direct_charger_info *charger = power_supply_get_drvdata(psy);
	enum power_supply_ext_property ext_psp = (enum power_supply_ext_property) psp;
	union power_supply_propval value = {0,};
	int ret = 0;

	value.intval = val->intval;

	switch ((int)psp) {
	case POWER_SUPPLY_PROP_STATUS:
		if (charger->charging_source == SEC_CHARGING_SOURCE_DIRECT) {
			psy_do_property(charger->pdata->direct_charger_name, get, psp, value);
		} else {
			psy_do_property(charger->pdata->main_charger_name, get, psp, value);
		}
		val->intval = value.intval;
		break;
	case POWER_SUPPLY_PROP_HEALTH:
		if (charger->charging_source == SEC_CHARGING_SOURCE_DIRECT) {
			psy_do_property(charger->pdata->direct_charger_name, get, psp, value);
			if (value.intval == POWER_SUPPLY_EXT_HEALTH_DC_ERR) {
				charger->dc_retry_cnt++;
				if (charger->dc_retry_cnt > 2) {
					charger->dc_err = true;
				} else
					charger->dc_err = false;
			} else {
				charger->dc_err = false;
				charger->dc_retry_cnt = 0;
			}
		} else {
			psy_do_property(charger->pdata->main_charger_name, get, psp, value);
			charger->dc_retry_cnt = 0;
		}
		val->intval = value.intval;
		break;
	case POWER_SUPPLY_PROP_INPUT_CURRENT_LIMIT: /* get input current which was set */
		psy_do_property(charger->pdata->main_charger_name, get, psp, value);
		if (is_direct_chg_mode_on(charger->direct_chg_mode)) {
			// NEED to CHECK
			val->intval = charger->input_current;
		} else {
			val->intval = value.intval;
		}
		break;
	case POWER_SUPPLY_PROP_CONSTANT_CHARGE_CURRENT: /* get charge current which was set */
		psy_do_property(charger->pdata->main_charger_name, get, psp, value);
		if (is_direct_chg_mode_on(charger->direct_chg_mode)) {
			// NEED to CHECK
			val->intval = charger->charging_current;
		} else {
			val->intval = value.intval;
		}
		break;
	case POWER_SUPPLY_PROP_TEMP:
		psy_do_property(charger->pdata->direct_charger_name, get, psp, value);
		val->intval = value.intval;
		break;
#if defined(CONFIG_DUAL_BATTERY_CELL_SENSING)
	case POWER_SUPPLY_PROP_VOLTAGE_NOW:
		psy_do_property(charger->pdata->direct_charger_name, get, psp, value);
		val->intval = value.intval;
		break;
#endif
	case POWER_SUPPLY_EXT_PROP_MIN ... POWER_SUPPLY_EXT_PROP_MAX:
		switch (ext_psp) {
		case POWER_SUPPLY_EXT_PROP_MONITOR_WORK:
			psy_do_property(charger->pdata->main_charger_name, get, ext_psp, value);
			if (is_pd_apdo_wire_type(charger->cable_type))
				psy_do_property(charger->pdata->direct_charger_name, get, ext_psp, value);
			sec_direct_chg_monitor(charger);
			break;
		case POWER_SUPPLY_EXT_PROP_DIRECT_CHARGER_MODE:
			val->intval = charger->direct_chg_mode;
			break;
		case POWER_SUPPLY_EXT_PROP_CHARGING_ENABLED_DC:
			psy_do_property(charger->pdata->main_charger_name, get,
				POWER_SUPPLY_EXT_PROP_CHARGING_ENABLED, value);
			if (value.intval == SEC_BAT_CHG_MODE_CHARGING)
				val->intval = true;
			else
				val->intval = false;
			break;
		case POWER_SUPPLY_EXT_PROP_DIRECT_DONE:
			val->intval = charger->direct_chg_done;
			break;
		case POWER_SUPPLY_EXT_PROP_MEASURE_INPUT:
			psy_do_property(charger->pdata->direct_charger_name, get, ext_psp, value);
			val->intval = value.intval;
			break;
		case POWER_SUPPLY_EXT_PROP_DIRECT_CHARGER_CHG_STATUS:
			psy_do_property(charger->pdata->direct_charger_name, get, ext_psp, value);
			val->strval = value.strval;
			break;
		case POWER_SUPPLY_EXT_PROP_CHANGE_CHARGING_SOURCE:
			val->intval = charger->test_mode_source;
			break;
		case POWER_SUPPLY_EXT_PROP_DIRECT_CONSTANT_CHARGE_VOLTAGE:
			psy_do_property(charger->pdata->direct_charger_name, get,
				POWER_SUPPLY_PROP_CONSTANT_CHARGE_VOLTAGE, value);
			val->intval = value.intval;
			break;
		default:
			ret = psy_do_property(charger->pdata->main_charger_name, get, ext_psp, value);
			val->intval = value.intval;
			return ret;
		}
		break;
	default:
		ret = psy_do_property(charger->pdata->main_charger_name, get, psp, value);
		val->intval = value.intval;
		return ret;
	}

	return ret;
}

static int sec_direct_chg_set_property(struct power_supply *psy,
			    enum power_supply_property psp,
			    const union power_supply_propval *val)
{
	struct sec_direct_charger_info *charger = power_supply_get_drvdata(psy);
	enum power_supply_ext_property ext_psp = (enum power_supply_ext_property) psp;
	union power_supply_propval value = {0,};
	int prev_val;
	int ret = 0;

	value.intval = val->intval;

	switch ((int)psp) {
	case POWER_SUPPLY_PROP_STATUS:
		psy_do_property(charger->pdata->main_charger_name, set,
			psp, value);
		charger->batt_status = val->intval;
		pr_info("%s: batt status(%d)\n", __func__, charger->batt_status);
		break;
	case POWER_SUPPLY_PROP_ONLINE:
		prev_val = charger->cable_type;
		charger->cable_type = val->intval;

		if (charger->cable_type == SEC_BATTERY_CABLE_NONE) {
			sec_direct_chg_set_initial_status(charger);
#if defined(CONFIG_DUAL_BATTERY_CELL_SENSING)
			value.intval = 0;
			psy_do_property(charger->pdata->direct_charger_name, set,
				POWER_SUPPLY_EXT_PROP_DIRECT_ADC_CTRL, value);
#endif
		}
#if defined(CONFIG_DUAL_BATTERY_CELL_SENSING)
		else {
			value.intval = 1;
			psy_do_property(charger->pdata->direct_charger_name, set,
				POWER_SUPPLY_EXT_PROP_DIRECT_ADC_CTRL, value);
		}
#endif

		/* main charger */
		value.intval = val->intval;
		psy_do_property(charger->pdata->main_charger_name, set,
			psp, value);

		/* direct charger */
		if (is_pd_apdo_wire_type(charger->cable_type)) {
			charger->direct_chg_mode = SEC_DIRECT_CHG_MODE_DIRECT_CHECK_VBAT;
			value.intval = 1;
			psy_do_property(charger->pdata->direct_charger_name, set,
				psp, value);
		} else {
			value.intval = 0;
			psy_do_property(charger->pdata->direct_charger_name, set,
				psp, value);
		}
		break;
	case POWER_SUPPLY_PROP_INPUT_CURRENT_LIMIT:
		charger->input_current = val->intval;
		sec_direct_chg_set_input_current(charger, psp, charger->input_current);
		break;
	case POWER_SUPPLY_PROP_CONSTANT_CHARGE_CURRENT:
		charger->charging_current = val->intval;
		sec_direct_chg_set_charging_current(charger, psp, charger->charging_current);
		break;
	case POWER_SUPPLY_PROP_CONSTANT_CHARGE_VOLTAGE:
		charger->float_voltage = val->intval;
		psy_do_property(charger->pdata->main_charger_name, set,
			psp, value);
		break;
	case POWER_SUPPLY_EXT_PROP_MIN ... POWER_SUPPLY_EXT_PROP_MAX:
		switch (ext_psp) {
		case POWER_SUPPLY_EXT_PROP_DIRECT_CHARGER_MODE:
			if (val->intval >= SEC_DIRECT_CHG_MODE_MAX) {
				pr_info("%s: abnormal direct_chg_mode(%d)\n", __func__, val->intval);
			} else {
				if (!charger->direct_chg_done) {
					pr_info("%s: direct_chg_mode:%s(%d)->%s(%d)\n", __func__,
						sec_direct_chg_mode_str[charger->direct_chg_mode], charger->direct_chg_mode,
						sec_direct_chg_mode_str[val->intval], val->intval);
					charger->direct_chg_mode = val->intval;
				}
			}
			break;
		case POWER_SUPPLY_EXT_PROP_CHARGING_ENABLED_DC:
#if 0
			if (val->intval)
				sec_direct_chg_check_set_charge(charger, charger->charger_mode,
					SEC_BAT_CHG_MODE_BUCK_OFF, SEC_BAT_CHG_MODE_CHARGING);
			else
				sec_direct_chg_check_set_charge(charger, charger->charger_mode,
					SEC_BAT_CHG_MODE_CHARGING, SEC_BAT_CHG_MODE_CHARGING_OFF);
#endif
			break;
		case POWER_SUPPLY_EXT_PROP_DIRECT_DONE:
			pr_info("%s: POWER_SUPPLY_EXT_PROP_DIRECT_DONE(%d)\n", __func__, val->intval);
			if (val->intval)
				sec_direct_chg_do_dc_fullcharged(charger);
			break;
		case POWER_SUPPLY_EXT_PROP_CURRENT_MEASURE:
			psy_do_property(charger->pdata->main_charger_name, set,
				ext_psp, value);
			break;
		case POWER_SUPPLY_EXT_PROP_DIRECT_WDT_CONTROL:
			psy_do_property(charger->pdata->direct_charger_name, set,
				ext_psp, value);
			break;
		case POWER_SUPPLY_EXT_PROP_DIRECT_CONSTANT_CHARGE_VOLTAGE:
			psy_do_property(charger->pdata->direct_charger_name, set,
				POWER_SUPPLY_PROP_CONSTANT_CHARGE_VOLTAGE, value);
			break;
		case POWER_SUPPLY_EXT_PROP_DIRECT_CURRENT_MAX:
			psy_do_property(charger->pdata->direct_charger_name, set,
				ext_psp, value);
			break;
		case POWER_SUPPLY_EXT_PROP_DIRECT_CONSTANT_CHARGE_VOLTAGE_MAX:
			psy_do_property(charger->pdata->direct_charger_name, set,
				POWER_SUPPLY_PROP_CONSTANT_CHARGE_VOLTAGE_MAX, value);
			break;
		case POWER_SUPPLY_EXT_PROP_DIRECT_ADC_CTRL:
			psy_do_property(charger->pdata->direct_charger_name, set,
				ext_psp, value);
			break;
		case POWER_SUPPLY_EXT_PROP_DIRECT_CLEAR_ERR:
			/* If SRCCAP is changed by Src, clear DC err variables */
			charger->dc_err = false;
			charger->dc_retry_cnt = 0;
			break;
		case POWER_SUPPLY_EXT_PROP_CHANGE_CHARGING_SOURCE:
			pr_info("%s: POWER_SUPPLY_EXT_PROP_CHANGE_CHARGING_SOURCE(%d, %d)\n",
				__func__, val->strval[0], val->strval[1]);
			if (val->strval[0] == SEC_STORE_MODE)
				charger->store_mode = true;
			if (is_pd_apdo_wire_type(charger->cable_type)) {
				charger->test_mode_source = val->strval[1];

				if (charger->test_mode_source == SEC_CHARGING_SOURCE_DIRECT)
					charger->test_mode_source = sec_direct_chg_check_charging_source(charger);

				sec_direct_chg_set_charging_source(charger, charger->charger_mode, charger->test_mode_source);
			} else {
				pr_info("%s: block to set charging_source (cable:%d, mode:%d, test:%d, store:%d)\n",
					__func__, charger->cable_type, charger->charger_mode,
					charger->test_mode_source, charger->store_mode);
			}
			break;
		case POWER_SUPPLY_EXT_PROP_REFRESH_CHARGING_SOURCE:
			if (is_pd_apdo_wire_type(charger->cable_type)) {
				int charging_source;
				
				charging_source = sec_direct_chg_check_charging_source(charger);
				sec_direct_chg_set_charging_source(charger, charger->charger_mode, charging_source);
			}			
			break;
		case POWER_SUPPLY_EXT_PROP_CHARGING_ENABLED:
			sec_direct_chg_set_charge(charger, val->intval);
			break;
		case POWER_SUPPLY_EXT_PROP_DC_INITIALIZE:
			sec_direct_chg_set_initial_status(charger);
			break;
 		default:
			ret = psy_do_property(charger->pdata->main_charger_name, set, ext_psp, value);
			return ret;
		}
		break;
	default:
		ret = psy_do_property(charger->pdata->main_charger_name, set, psp, value);
		return ret;
	}

	return ret;
}

#ifdef CONFIG_OF
static int sec_direct_charger_parse_dt(struct device *dev,
		struct sec_direct_charger_info *charger)
{
	struct device_node *np = dev->of_node;
	int ret = 0;

	if (!np) {
		pr_err("%s: np NULL\n", __func__);
		return 1;
	} else {
		ret = of_property_read_string(np, "charger,battery_name",
				(char const **)&charger->pdata->battery_name);
		if (ret)
			pr_err("%s: battery_name is Empty\n", __func__);

		ret = of_property_read_string(np, "charger,main_charger",
				(char const **)&charger->pdata->main_charger_name);
		if (ret)
			pr_err("%s: main_charger is Empty\n", __func__);

		ret = of_property_read_string(np, "charger,direct_charger",
				(char const **)&charger->pdata->direct_charger_name);
		if (ret)
			pr_err("%s: direct_charger is Empty\n", __func__);

		ret = of_property_read_u32(np, "charger,dchg_min_current",
			&charger->pdata->dchg_min_current);
		if (ret) {
			pr_err("%s : charger,dchg_min_current is Empty\n", __func__);
			charger->pdata->dchg_min_current = SEC_DIRECT_CHG_MIN_IOUT;
		}
		pr_info("%s: charger,dchg_min_current is %d\n", __func__, charger->pdata->dchg_min_current);

		charger->ta_alert_wa = of_property_read_bool(np, "charger,ta_alert_wa");
	}

	np = of_find_node_by_name(NULL, "battery");
	if (!np) {
		pr_info("%s: np NULL\n", __func__);
		return 1;
	} else {
		ret = of_property_read_u32(np, "battery,wire_normal_warm_thresh",
				&charger->pdata->dchg_temp_high_threshold);
		if (ret) {
			pr_info("%s : dchg_temp_high_threshold is Empty\n", __func__);
			charger->pdata->dchg_temp_high_threshold = 420;
		}

		ret = of_property_read_u32(np, "battery,wire_cool1_normal_thresh",
				&charger->pdata->dchg_temp_low_threshold);
		if (ret) {
			pr_info("%s : dchg_temp_low_threshold is Empty\n", __func__);
			charger->pdata->dchg_temp_low_threshold = 180;
		}
	}

	return 0;
}
#else
static int sec_direct_charger_parse_dt(struct device *dev,
		struct sec_direct_charger_info *charger)
{
	return 0;
}
#endif /* CONFIG_OF */

static int sec_direct_charger_check_devs_registered(struct device *dev)
{
	struct device_node *np = dev->of_node;
	const char *dev_name;
	struct power_supply *psy_dev = NULL;

	if (!np) {
		pr_err("%s: np NULL\n", __func__);
		return 0;
	}

	if (!of_property_read_string(np, "charger,direct_charger", &dev_name)) {
		psy_dev = power_supply_get_by_name(dev_name);
		if (psy_dev) {
			dev_info(dev, "%s: %s is registered\n", __func__, dev_name);
			power_supply_put(psy_dev);
		} else {
			dev_err(dev, "%s: %s is not registered yet\n", __func__, dev_name);
		}
	}

	if (!of_property_read_string(np, "charger,main_charger", &dev_name)) {
		psy_dev = power_supply_get_by_name(dev_name);
		if (psy_dev) {
			dev_info(dev, "%s: %s is registered\n", __func__, dev_name);
			power_supply_put(psy_dev);
		} else {
			dev_err(dev, "%s: %s is not registered yet\n", __func__, dev_name);
#if defined(CONFIG_ARCH_QCOM) && !defined(CONFIG_ARCH_EXYNOS) \
	&& !defined(CONFIG_QGKI)	/* for QC GKI Build */
			return -EPROBE_DEFER;
#endif
		}
	}

	return 0;
}

static enum power_supply_property sec_direct_charger_props[] = {
	POWER_SUPPLY_PROP_ONLINE,
};

static const struct power_supply_desc sec_direct_charger_power_supply_desc = {
	.name = "sec-direct-charger",
	.type = POWER_SUPPLY_TYPE_UNKNOWN,
	.properties = sec_direct_charger_props,
	.num_properties = ARRAY_SIZE(sec_direct_charger_props),
	.get_property = sec_direct_chg_get_property,
	.set_property = sec_direct_chg_set_property,
};

static int sec_direct_charger_probe(struct platform_device *pdev)
{
	struct sec_direct_charger_info *charger;
	struct sec_direct_charger_platform_data *pdata = NULL;
	struct power_supply_config direct_charger_cfg = {};
	int ret = 0;

	pr_info("%s: SEC Direct-Charger Driver Loading\n", __func__);

	if (pdev->dev.of_node) {
		if (sec_direct_charger_check_devs_registered(&pdev->dev))
			return -EPROBE_DEFER;
	}

	charger = kzalloc(sizeof(*charger), GFP_KERNEL);
	if (!charger)
		return -ENOMEM;

	if (pdev->dev.of_node) {
		pdata = devm_kzalloc(&pdev->dev,
				sizeof(struct sec_direct_charger_platform_data),
				GFP_KERNEL);
		if (!pdata) {
			dev_err(&pdev->dev, "Failed to allocate memory\n");
			ret = -ENOMEM;
			goto err_charger_free;
		}

		charger->pdata = pdata;
		if (sec_direct_charger_parse_dt(&pdev->dev, charger)) {
			dev_err(&pdev->dev,
				"%s: Failed to get sec-direct-charger dt\n", __func__);
			ret = -EINVAL;
			goto err_pdata_free;
		}
	} else {
		pdata = dev_get_platdata(&pdev->dev);
		charger->pdata = pdata;
	}

	/* init direct charger variables */
	charger->direct_chg_done = false;
	charger->direct_chg_mode = SEC_DIRECT_CHG_MODE_DIRECT_OFF;
	charger->cable_type = SEC_BATTERY_CABLE_NONE;

	charger->charger_mode = SEC_BAT_CHG_MODE_CHARGING_OFF;
	charger->charger_mode_direct = SEC_BAT_CHG_MODE_MAX;
	charger->charger_mode_main = SEC_BAT_CHG_MODE_MAX;
	charger->test_mode_source = SEC_CHARGING_SOURCE_DIRECT;

	charger->wc_tx_enable = false;
	charger->now_isApdo = false;
	charger->store_mode = false;

	platform_set_drvdata(pdev, charger);
	charger->dev = &pdev->dev;
	direct_charger_cfg.drv_data = charger;
	charger->ta_alert_mode = OCP_NONE;

	mutex_init(&charger->charger_mutex);

	charger->psy_chg = power_supply_register(&pdev->dev,
			&sec_direct_charger_power_supply_desc, &direct_charger_cfg);
	if (IS_ERR(charger->psy_chg)) {
		ret = PTR_ERR(charger->psy_chg);
		dev_err(charger->dev,
			"%s: Failed to Register psy_chg(%d)\n", __func__, ret);
		goto err_power_supply_register;
	}

	pr_info("%s: SEC Direct-Charger Driver Loaded(%s, %s)\n",
		__func__, charger->pdata->main_charger_name, charger->pdata->direct_charger_name);
	return 0;

err_power_supply_register:
	mutex_destroy(&charger->charger_mutex);
err_pdata_free:
	kfree(pdata);
err_charger_free:
	kfree(charger);

	return ret;
}

static int sec_direct_charger_remove(struct platform_device *pdev)
{
	struct sec_direct_charger_info *charger = platform_get_drvdata(pdev);

	pr_info("%s: ++\n", __func__);

	power_supply_unregister(charger->psy_chg);
	mutex_destroy(&charger->charger_mutex);

	dev_dbg(charger->dev, "%s: End\n", __func__);

	kfree(charger->pdata);
	kfree(charger);

	pr_info("%s: --\n", __func__);

	return 0;
}

static int sec_direct_charger_suspend(struct device *dev)
{
	return 0;
}

static int sec_direct_charger_resume(struct device *dev)
{
	return 0;
}

static void sec_direct_charger_shutdown(struct platform_device *pdev)
{
	struct sec_direct_charger_info *charger = platform_get_drvdata(pdev);
	union power_supply_propval value = {0,};

	pr_info("%s: ++\n", __func__);

	value.intval = false;
	psy_do_property(charger->pdata->direct_charger_name, set,
		POWER_SUPPLY_EXT_PROP_CHARGING_ENABLED, value);

	value.intval = SEC_INPUT_VOLTAGE_5V;
	psy_do_property("battery", set,
		POWER_SUPPLY_EXT_PROP_DIRECT_FIXED_PDO, value);

	pr_info("%s: --\n", __func__);
}

#ifdef CONFIG_OF
static struct of_device_id sec_direct_charger_dt_ids[] = {
	{ .compatible = "samsung,sec-direct-charger" },
	{ }
};
MODULE_DEVICE_TABLE(of, sec_direct_charger_dt_ids);
#endif /* CONFIG_OF */

static const struct dev_pm_ops sec_direct_charger_pm_ops = {
	.suspend = sec_direct_charger_suspend,
	.resume = sec_direct_charger_resume,
};

static struct platform_driver sec_direct_charger_driver = {
	.driver = {
		.name = "sec-direct-charger",
		.owner = THIS_MODULE,
		.pm = &sec_direct_charger_pm_ops,
#ifdef CONFIG_OF
		.of_match_table = sec_direct_charger_dt_ids,
#endif
	},
	.probe = sec_direct_charger_probe,
	.remove = sec_direct_charger_remove,
	.shutdown = sec_direct_charger_shutdown,
};

static int __init sec_direct_charger_init(void)
{
	pr_info("%s: \n", __func__);
	return platform_driver_register(&sec_direct_charger_driver);
}

static void __exit sec_direct_charger_exit(void)
{
	platform_driver_unregister(&sec_direct_charger_driver);
}

device_initcall_sync(sec_direct_charger_init);
module_exit(sec_direct_charger_exit);

MODULE_DESCRIPTION("Samsung Direct Charger Driver");
MODULE_AUTHOR("Samsung Electronics");
MODULE_LICENSE("GPL");
