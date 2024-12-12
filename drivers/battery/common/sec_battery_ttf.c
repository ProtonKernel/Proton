/*
 * sec_battery_ttf.c
 * Samsung Mobile Battery Driver
 *
 * Copyright (C) 2019 Samsung Electronics
 *
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */
#include "sec_battery.h"
#include "sec_battery_ttf.h"

#define is_ttf_thermal_zone(thermal_zone) ( \
	thermal_zone == BAT_THERMAL_NORMAL || \
	thermal_zone == BAT_THERMAL_COOL1 || \
	thermal_zone == BAT_THERMAL_COOL2)

int sec_calc_ttf(struct sec_battery_info * battery, unsigned int ttf_curr)
{
	struct sec_cv_slope *cv_data = battery->ttf_d->cv_data;
	int i, cc_time = 0, cv_time = 0;
	int soc = battery->capacity;
	int charge_current = ttf_curr;
	int design_cap = battery->ttf_d->ttf_capacity;
	union power_supply_propval value = {0, };

	value.intval = SEC_FUELGAUGE_CAPACITY_TYPE_DYNAMIC_SCALE;
	psy_do_property(battery->pdata->fuelgauge_name, get,
			POWER_SUPPLY_PROP_CAPACITY, value);
	soc = value.intval;

	if (!cv_data || (ttf_curr <= 0)) {
		pr_info("%s: no cv_data or val: %d\n", __func__, ttf_curr);
		return -1;
	}
	for (i = 0; i < battery->ttf_d->cv_data_length; i++) {
		if (charge_current >= cv_data[i].fg_current)
			break;
	}
	i = i >= battery->ttf_d->cv_data_length ? battery->ttf_d->cv_data_length - 1 : i;
	if (cv_data[i].soc < soc) {
		for (i = 0; i < battery->ttf_d->cv_data_length; i++) {
			if (soc <= cv_data[i].soc)
				break;
		}
		cv_time =
		    ((cv_data[i - 1].time - cv_data[i].time) * (cv_data[i].soc - soc)
		     / (cv_data[i].soc - cv_data[i - 1].soc)) + cv_data[i].time;
	} else {		/* CC mode || NONE */
		cv_time = cv_data[i].time;
		cc_time =
			design_cap * (cv_data[i].soc - soc) / ttf_curr * 3600 / 1000;
		pr_debug("%s: cc_time: %d\n", __func__, cc_time);
		if (cc_time < 0)
			cc_time = 0;
	}

	pr_info("%s: cap: %d, soc: %4d, T: %6d, avg: %4d, cv soc: %4d, i: %4d, val: %d\n",
	     __func__, design_cap, soc, cv_time + cc_time,
	     battery->current_avg, cv_data[i].soc, i, ttf_curr);

	if (cv_time + cc_time >= 0)
		return cv_time + cc_time + 60;
	else
		return 60;	/* minimum 1minutes */
}

int sec_calc_ttf_to_full_capacity(struct sec_battery_info *battery, unsigned int ttf_curr)
{
	struct sec_cv_slope *cv_data = battery->ttf_d->cv_data;
	int i, cc_time = 0, cv_time = 0;
	int soc = get_full_capacity(battery->fs) * 10;
	int charge_current = ttf_curr;
	int design_cap = battery->ttf_d->ttf_capacity;

	if (!cv_data || (ttf_curr <= 0)) {
		pr_info("%s: no cv_data or val: %d\n", __func__, ttf_curr);
		return -1;
	}
	for (i = 0; i < battery->ttf_d->cv_data_length; i++) {
		if (charge_current >= cv_data[i].fg_current)
			break;
	}
	i = i >= battery->ttf_d->cv_data_length ? battery->ttf_d->cv_data_length - 1 : i;
	if (cv_data[i].soc < soc) {
		for (i = 0; i < battery->ttf_d->cv_data_length; i++) {
			if (soc <= cv_data[i].soc)
				break;
		}
		cv_time =
		    ((cv_data[i - 1].time - cv_data[i].time) * (cv_data[i].soc - soc)
		     / (cv_data[i].soc - cv_data[i - 1].soc)) + cv_data[i].time;
	} else {		/* CC mode || NONE */
		cv_time = cv_data[i].time;
		cc_time =
			design_cap * (cv_data[i].soc - soc) / ttf_curr * 3600 / 1000;
		pr_debug("%s: cc_time: %d\n", __func__, cc_time);
		if (cc_time < 0)
			cc_time = 0;
	}

	pr_info("%s: cap: %d, soc: %4d, T: %6d, avg: %4d, cv soc: %4d, i: %4d, val: %d\n",
	     __func__, design_cap, soc, cv_time + cc_time,
	     battery->current_avg, cv_data[i].soc, i, ttf_curr);

	if (cv_time + cc_time >= 0)
		return cv_time + cc_time;
	else
		return 0;
}

void sec_bat_calc_time_to_full(struct sec_battery_info * battery)
{
	if (delayed_work_pending(&battery->ttf_d->timetofull_work)) {
		pr_info("%s: keep time_to_full(%5d sec)\n", __func__, battery->ttf_d->timetofull);
	} else if ((battery->status == POWER_SUPPLY_STATUS_CHARGING ||
		(battery->status == POWER_SUPPLY_STATUS_FULL && battery->capacity != 100)) && !battery->wc_tx_enable) {
		int charge = 0;

		if (is_hv_wire_12v_type(battery->cable_type)) {
			charge = battery->ttf_d->ttf_hv_12v_charge_current;
#if IS_ENABLED(CONFIG_WIRELESS_CHARGING)
		} else if (is_hv_wireless_type(battery->cable_type) ||
			battery->cable_type == SEC_BATTERY_CABLE_PREPARE_WIRELESS_HV ||
			battery->cable_type == SEC_BATTERY_CABLE_PREPARE_WIRELESS_20) {
			if (sec_bat_hv_wc_normal_mode_check(battery))
				charge = battery->ttf_d->ttf_wireless_charge_current;
			else if ((battery->cable_type == SEC_BATTERY_CABLE_PREPARE_WIRELESS_20 && !sec_bat_get_lpmode()) ||
				battery->cable_type == SEC_BATTERY_CABLE_HV_WIRELESS_20)
				charge = battery->ttf_d->ttf_predict_wc20_charge_current;
			else
				charge = battery->ttf_d->ttf_hv_wireless_charge_current;
		} else if (is_nv_wireless_type(battery->cable_type)) {
			charge = battery->ttf_d->ttf_wireless_charge_current;
#endif
		} else if (is_hv_wire_type(battery->cable_type)) {
			charge = battery->ttf_d->ttf_hv_charge_current;
		} else if (is_pd_apdo_wire_type(battery->cable_type) ||
			(is_pd_fpdo_wire_type(battery->cable_type) && battery->hv_pdo)) {
			if (battery->pd_max_charge_power > HV_CHARGER_STATUS_STANDARD4) {
				charge = battery->ttf_d->ttf_dc45_charge_current;
			} else if (battery->pd_max_charge_power > HV_CHARGER_STATUS_STANDARD3) {
				charge = battery->ttf_d->ttf_dc25_charge_current;
			} else if (battery->pd_max_charge_power <= battery->pdata->pd_charging_charge_power &&
				battery->pdata->charging_current[battery->cable_type].fast_charging_current >= \
				battery->pdata->max_charging_current) { /* same PD power with AFC */
				charge = battery->ttf_d->ttf_hv_charge_current;
			} else { /* other PD charging */
				charge = (battery->pd_max_charge_power / 5) > battery->pdata->charging_current[battery->cable_type].fast_charging_current ?
					battery->pdata->charging_current[battery->cable_type].fast_charging_current : (battery->pd_max_charge_power / 5);
			}
		} else {
			charge = (battery->max_charge_power / 5) > battery->pdata->charging_current[battery->cable_type].fast_charging_current ?
					battery->pdata->charging_current[battery->cable_type].fast_charging_current : (battery->max_charge_power / 5);
		}
		if (is_full_capacity(battery->fs)) {
			pr_info("%s: time to %d percent\n", __func__, battery->batt_full_capacity);
			battery->ttf_d->timetofull =
				sec_calc_ttf(battery, charge) - sec_calc_ttf_to_full_capacity(battery, charge);
		} else
			battery->ttf_d->timetofull = sec_calc_ttf(battery, charge);
		dev_info(battery->dev, "%s: T: %5d sec, passed time: %5ld, current: %d\n",
				__func__, battery->ttf_d->timetofull, battery->charging_passed_time, charge);
	} else {
		battery->ttf_d->timetofull = -1;
	}
}

#if IS_ENABLED(CONFIG_WIRELESS_CHARGING)
void sec_bat_predict_wireless20_time_to_full_current(struct sec_battery_info *battery, int rx_power)
{
	if (rx_power < 0)
		battery->ttf_d->ttf_predict_wc20_charge_current = 0;
	else
		battery->ttf_d->ttf_predict_wc20_charge_current =
			battery->pdata->wireless_power_info[rx_power].ttf_charge_current;

	pr_info("%s: %dmA \n", __func__, battery->ttf_d->ttf_predict_wc20_charge_current);
}
EXPORT_SYMBOL_KUNIT(sec_bat_predict_wireless20_time_to_full_current);
#endif

int sec_ttf_parse_dt(struct sec_battery_info *battery)
{
	struct device_node *np;
	struct sec_ttf_data *pdata = battery->ttf_d;
	sec_battery_platform_data_t *bpdata = battery->pdata;
	int ret = 0, len = 0;
	const u32 *p;

	pdata->pdev = battery;
	np = of_find_node_by_name(NULL, "battery");
		if (!np) {
			pr_info("%s: np NULL\n", __func__);
			return 1;
	}

	ret = of_property_read_u32(np, "battery,ttf_hv_12v_charge_current",
					&pdata->ttf_hv_12v_charge_current);
	if (ret) {
		pdata->ttf_hv_12v_charge_current =
			bpdata->charging_current[SEC_BATTERY_CABLE_12V_TA].fast_charging_current;
		pr_info("%s: ttf_hv_12v_charge_current is Empty, Default value %d\n",
			__func__, pdata->ttf_hv_12v_charge_current);
	}
	ret = of_property_read_u32(np, "battery,ttf_hv_charge_current",
					&pdata->ttf_hv_charge_current);
	if (ret) {
		pdata->ttf_hv_charge_current =
			bpdata->charging_current[SEC_BATTERY_CABLE_9V_TA].fast_charging_current;
		pr_info("%s: ttf_hv_charge_current is Empty, Default value %d\n",
			__func__, pdata->ttf_hv_charge_current);
	}

	ret = of_property_read_u32(np, "battery,ttf_hv_wireless_charge_current",
					&pdata->ttf_hv_wireless_charge_current);
	if (ret) {
		pdata->ttf_hv_wireless_charge_current =
			bpdata->charging_current[SEC_BATTERY_CABLE_HV_WIRELESS].fast_charging_current - 300;
		pr_info("%s: ttf_hv_wireless_charge_current is Empty, Default value %d\n",
			__func__, pdata->ttf_hv_wireless_charge_current);
	}

	ret = of_property_read_u32(np, "battery,ttf_hv_12v_wireless_charge_current",
					&pdata->ttf_hv_12v_wireless_charge_current);
	if (ret) {
		pdata->ttf_hv_12v_wireless_charge_current =
			bpdata->charging_current[SEC_BATTERY_CABLE_HV_WIRELESS_20].fast_charging_current - 300;
		pr_info("%s: ttf_hv_12v_wireless_charge_current is Empty, Default value %d\n",
			__func__, pdata->ttf_hv_12v_wireless_charge_current);
	}

	ret = of_property_read_u32(np, "battery,ttf_wireless_charge_current",
					&pdata->ttf_wireless_charge_current);
	if (ret) {
		pdata->ttf_wireless_charge_current =
			bpdata->charging_current[SEC_BATTERY_CABLE_WIRELESS].input_current_limit;
		pr_info("%s: ttf_wireless_charge_current is Empty, Default value %d\n",
			__func__, pdata->ttf_wireless_charge_current);
	}

	ret = of_property_read_u32(np, "battery,ttf_dc25_charge_current",
					&pdata->ttf_dc25_charge_current);
	if (ret) {
		pdata->ttf_dc25_charge_current =
			bpdata->charging_current[SEC_BATTERY_CABLE_9V_TA].fast_charging_current;
		pr_info("%s: ttf_dc25_charge_current is Empty, Default value %d\n",
			__func__, pdata->ttf_dc25_charge_current);
	}

	ret = of_property_read_u32(np, "battery,ttf_dc45_charge_current",
					&pdata->ttf_dc45_charge_current);
	if (ret) {
		pdata->ttf_dc45_charge_current = pdata->ttf_dc25_charge_current;
		pr_info("%s: ttf_dc45_charge_current is Empty, Default value %d \n",
			__func__, pdata->ttf_dc45_charge_current);
	}

	ret = of_property_read_u32(np, "battery,ttf_capacity",
					   &pdata->ttf_capacity);
	if (ret < 0) {
		pr_err("%s error reading capacity_calculation_type %d\n", __func__, ret);
		pdata->ttf_capacity = bpdata->battery_full_capacity;
	}

	p = of_get_property(np, "battery,cv_data", &len);
	if (p) {
		pdata->cv_data = kzalloc(len, GFP_KERNEL);
		pdata->cv_data_length = len / sizeof(struct sec_cv_slope);
		pr_err("%s: len= %ld, length= %d, %d\n", __func__,
		       sizeof(int) * len, len, pdata->cv_data_length);
		ret = of_property_read_u32_array(np, "battery,cv_data",
				(u32 *)pdata->cv_data, len / sizeof(u32));
		if (ret) {
			pr_err("%s: failed to read battery->cv_data: %d\n",
				__func__, ret);
			kfree(pdata->cv_data);
			pdata->cv_data = NULL;
		}
	} else {
		pr_err("%s: there is not cv_data\n", __func__);
	}
	return 0;
}

void sec_bat_time_to_full_work(struct work_struct *work)
{
	struct sec_ttf_data *dev = container_of(work,
				struct sec_ttf_data, timetofull_work.work);
	struct sec_battery_info *battery = dev->pdev;
	union power_supply_propval value = {0, };

	psy_do_property(battery->pdata->charger_name, get,
		POWER_SUPPLY_PROP_INPUT_CURRENT_LIMIT, value);
	battery->current_max = value.intval;

	value.intval = SEC_BATTERY_CURRENT_MA;
	psy_do_property(battery->pdata->fuelgauge_name, get,
		POWER_SUPPLY_PROP_CURRENT_NOW, value);
	battery->current_now = value.intval;

	value.intval = SEC_BATTERY_CURRENT_MA;
	psy_do_property(battery->pdata->fuelgauge_name, get,
		POWER_SUPPLY_PROP_CURRENT_AVG, value);
	battery->current_avg = value.intval;

	sec_bat_calc_time_to_full(battery);
	dev_info(battery->dev, "%s:\n",__func__);
	if (battery->voltage_now > 0)
		battery->voltage_now--;

	power_supply_changed(battery->psy_bat);
}

void ttf_work_start(struct sec_battery_info *battery)
{
	if (sec_bat_get_lpmode()) {
		cancel_delayed_work(&battery->ttf_d->timetofull_work);
		if (battery->current_event & SEC_BAT_CURRENT_EVENT_AFC) {
			int work_delay = 0;

			if (!is_wireless_type(battery->cable_type)) {
				work_delay = battery->pdata->pre_afc_work_delay;
			} else {
				work_delay = battery->pdata->pre_wc_afc_work_delay;
			}
			queue_delayed_work(battery->monitor_wqueue,
				&battery->ttf_d->timetofull_work, msecs_to_jiffies(work_delay));
		}
	}
}

int ttf_display(unsigned int capacity, int bat_sts, int thermal_zone, int time)
{
	if (capacity == 100)
			return 0;

	if (((bat_sts == POWER_SUPPLY_STATUS_CHARGING) ||
		(bat_sts == POWER_SUPPLY_STATUS_FULL && capacity != 100)) &&
		is_ttf_thermal_zone(thermal_zone))
		return time;
	else
		return 0;
}
EXPORT_SYMBOL_KUNIT(ttf_display);

void ttf_init(struct sec_battery_info *battery)
{
	battery->ttf_d = kzalloc(sizeof(struct sec_ttf_data),
		GFP_KERNEL);
	if (!battery->ttf_d) {
		pr_err("Failed to allocate memory\n");
	}
	sec_ttf_parse_dt(battery);
	battery->ttf_d->timetofull = -1;

	INIT_DELAYED_WORK(&battery->ttf_d->timetofull_work, sec_bat_time_to_full_work);
}
EXPORT_SYMBOL_KUNIT(ttf_init);
