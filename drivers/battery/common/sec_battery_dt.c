/*
 *  sec_battery_dt.c
 *  Samsung Mobile Battery Driver
 *
 *  Copyright (C) 2018 Samsung Electronics
 *
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */
#include "sec_battery.h"
#include "sec_battery_dt.h"


#ifdef CONFIG_OF
#define PROPERTY_NAME_SIZE 128
int sec_bat_parse_dt_siop(
	struct sec_battery_info *battery, struct device_node *np)
{
	sec_battery_platform_data_t *pdata = battery->pdata;
	int ret = 0, len = 0;
	const u32 *p;

	ret = of_property_read_u32(np, "battery,siop_icl",
			&pdata->siop_icl);
	if (ret)
		pdata->siop_icl = SIOP_INPUT_LIMIT_CURRENT;

	ret = of_property_read_u32(np, "battery,siop_fcc",
			&pdata->siop_fcc);
	if (ret)
		pdata->siop_fcc = SIOP_CHARGING_LIMIT_CURRENT;

	ret = of_property_read_u32(np, "battery,siop_hv_12v_icl",
			&pdata->siop_hv_12v_icl);
	if (ret)
		pdata->siop_hv_12v_icl = SIOP_HV_12V_INPUT_LIMIT_CURRENT;

	ret = of_property_read_u32(np, "battery,siop_hv_12v_fcc",
			&pdata->siop_hv_12v_fcc);
	if (ret)
		pdata->siop_hv_12v_fcc = SIOP_HV_12V_CHARGING_LIMIT_CURRENT;

	ret = of_property_read_u32(np, "battery,siop_hv_icl",
			&pdata->siop_hv_icl);
	if (ret)
		pdata->siop_hv_icl = SIOP_HV_INPUT_LIMIT_CURRENT;

	ret = of_property_read_u32(np, "battery,siop_hv_icl_2nd",
			&pdata->siop_hv_icl_2nd);
	if (ret)
		pdata->siop_hv_icl_2nd = SIOP_HV_INPUT_LIMIT_CURRENT;

	ret = of_property_read_u32(np, "battery,siop_hv_fcc",
			&pdata->siop_hv_fcc);
	if (ret)
		pdata->siop_hv_fcc = SIOP_HV_CHARGING_LIMIT_CURRENT;

	ret = of_property_read_u32(np, "battery,siop_apdo_icl",
			&pdata->siop_apdo_icl);
	if (ret)
		pdata->siop_apdo_icl = SIOP_APDO_INPUT_LIMIT_CURRENT;

	ret = of_property_read_u32(np, "battery,siop_apdo_fcc",
			&pdata->siop_apdo_fcc);
	if (ret)
		pdata->siop_apdo_fcc = SIOP_APDO_CHARGING_LIMIT_CURRENT;

	ret = of_property_read_u32(np, "battery,siop_wpc_icl",
			&pdata->siop_wpc_icl);
	if (ret)
		pdata->siop_wpc_icl = SIOP_WIRELESS_INPUT_LIMIT_CURRENT;

	p = of_get_property(np, "battery,siop_wpc_fcc", &len);
	if (!p) {
		pr_info("%s : battery,siop_wpc_fcc is Empty\n", __func__);
	} else {
		len = len / sizeof(u32);
		pdata->siop_wpc_fcc =
			kzalloc(sizeof(*pdata->siop_wpc_fcc) * len, GFP_KERNEL);
		ret = of_property_read_u32_array(np, "battery,siop_wpc_fcc",
				pdata->siop_wpc_fcc, len);

		pr_info("%s: parse siop_wpc_fcc, ret = %d, len = %d\n", __func__, ret, len);
	}

	ret = of_property_read_u32(np, "battery,siop_hv_wpc_icl",
			&pdata->siop_hv_wpc_icl);
	if (ret)
		pdata->siop_hv_wpc_icl = SIOP_HV_WIRELESS_INPUT_LIMIT_CURRENT;

	p = of_get_property(np, "battery,siop_hv_wpc_fcc", &len);
	if (!p) {
		pr_info("%s : battery,siop_hv_wpc_fcc is Empty\n", __func__);
	} else {
		len = len / sizeof(u32);
		pdata->siop_hv_wpc_fcc =
			kzalloc(sizeof(*pdata->siop_hv_wpc_fcc) * len, GFP_KERNEL);
		ret = of_property_read_u32_array(np, "battery,siop_hv_wpc_fcc",
				pdata->siop_hv_wpc_fcc, len);

		pr_info("%s: parse siop_hv_wpc_fcc, ret = %d, len = %d\n", __func__, ret, len);
	}

	len = of_property_count_u32_elems(np, "battery,siop_scenarios");
	if (len > 0) {
		int siop_scenarios[SIOP_SCENARIO_NUM_MAX];

		pdata->siop_scenarios_num = (len > SIOP_SCENARIO_NUM_MAX) ? SIOP_SCENARIO_NUM_MAX : len;
		ret = of_property_read_u32_array(np, "battery,siop_scenarios",
					(u32 *)siop_scenarios, pdata->siop_scenarios_num);

		ret = of_property_read_u32(np, "battery,siop_curr_type_num",
				&pdata->siop_curr_type_num);
		if (ret) {
			pdata->siop_scenarios_num = 0;
			pdata->siop_curr_type_num = 0;
			goto parse_siop_next;
		}

		pdata->siop_curr_type_num =
			(pdata->siop_curr_type_num > SIOP_CURR_TYPE_MAX) ? SIOP_CURR_TYPE_MAX : pdata->siop_curr_type_num;

		if (pdata->siop_curr_type_num > 0) {
			int i, j, siop_level;
			char prop_name[PROPERTY_NAME_SIZE];

			for (i = 0; i < pdata->siop_scenarios_num; ++i) {
				siop_level = siop_scenarios[i];
				pdata->siop_table[i].level = siop_level;

				snprintf(prop_name, PROPERTY_NAME_SIZE, "battery,siop_icl_%d", siop_level);
				ret = of_property_read_u32_array(np, prop_name,
							(u32 *)pdata->siop_table[i].icl, pdata->siop_curr_type_num);

				snprintf(prop_name, PROPERTY_NAME_SIZE, "battery,siop_fcc_%d", siop_level);
				ret = of_property_read_u32_array(np, prop_name,
							(u32 *)pdata->siop_table[i].fcc, pdata->siop_curr_type_num);

				for (j = 0; j < pdata->siop_curr_type_num; ++j)
					pr_info("%s: level=%d, [%d].siop_icl[%d]=%d, [%d].siop_fcc[%d]=%d\n",
						__func__, pdata->siop_table[i].level,
						i, j, pdata->siop_table[i].icl[j],
						i, j, pdata->siop_table[i].fcc[j]);
			}
		}
	}

parse_siop_next:

	return ret;
}

int sec_bat_parse_dt_lrp(
	struct sec_battery_info *battery, struct device_node *np, int type)
{
	sec_battery_platform_data_t *pdata = battery->pdata;
	int ret = 0, len = 0;
	char prop_name[PROPERTY_NAME_SIZE];
	int lrp_table[LRP_PROPS];

	snprintf(prop_name, PROPERTY_NAME_SIZE,
			"battery,temp_table_%s", LRP_TYPE_STRING[type]);
	len = of_property_count_u32_elems(np, prop_name);
	if (len != LRP_PROPS)
		return -1;

	ret = of_property_read_u32_array(np, prop_name,
		(u32 *)lrp_table, LRP_PROPS);
	if (ret) {
		pr_info("%s: failed to parse %s!!, ret = %d\n",
			__func__, LRP_TYPE_STRING[type], ret);
		return ret;
	}

	pdata->lrp_temp[type].trig[ST2][LCD_OFF] = lrp_table[0];
	pdata->lrp_temp[type].recov[ST2][LCD_OFF] = lrp_table[1];
	pdata->lrp_temp[type].trig[ST1][LCD_OFF] = lrp_table[2];
	pdata->lrp_temp[type].recov[ST1][LCD_OFF] = lrp_table[3];
	pdata->lrp_temp[type].trig[ST2][LCD_ON] = lrp_table[4];
	pdata->lrp_temp[type].recov[ST2][LCD_ON] = lrp_table[5];
	pdata->lrp_temp[type].trig[ST1][LCD_ON] = lrp_table[6];
	pdata->lrp_temp[type].recov[ST1][LCD_ON] = lrp_table[7];
	pdata->lrp_curr[type].st_icl[ST1] = lrp_table[8];
	pdata->lrp_curr[type].st_fcc[ST1] = lrp_table[9];
	pdata->lrp_curr[type].st_icl[ST2] = lrp_table[10];
	pdata->lrp_curr[type].st_fcc[ST2] = lrp_table[11];

	pr_info("%s: pdata->lrp_temp[%s].trig_st1=%d\n",
		__func__, LRP_TYPE_STRING[type], pdata->lrp_temp[type].trig[ST1][LCD_OFF]);
	pr_info("%s: pdata->lrp_temp[%s].trig_st2=%d\n",
		__func__, LRP_TYPE_STRING[type], pdata->lrp_temp[type].trig[ST2][LCD_OFF]);
	pr_info("%s: pdata->lrp_temp[%s].recov_st1=%d\n",
		__func__, LRP_TYPE_STRING[type], pdata->lrp_temp[type].recov[ST1][LCD_OFF]);
	pr_info("%s: pdata->lrp_temp[%s].recov_st2=%d\n",
		__func__, LRP_TYPE_STRING[type], pdata->lrp_temp[type].recov[ST2][LCD_OFF]);
	pr_info("%s: pdata->lrp_temp[%s].trig_st1_lcdon=%d\n",
		__func__, LRP_TYPE_STRING[type], pdata->lrp_temp[type].trig[ST1][LCD_ON]);
	pr_info("%s: pdata->lrp_temp[%s].trig_st2_lcdon=%d\n",
		__func__, LRP_TYPE_STRING[type], pdata->lrp_temp[type].trig[ST2][LCD_ON]);
	pr_info("%s: pdata->lrp_temp[%s].recov_st1_lcdon=%d\n",
		__func__, LRP_TYPE_STRING[type], pdata->lrp_temp[type].recov[ST1][LCD_ON]);
	pr_info("%s: pdata->lrp_temp[%s].recov_st2_lcdon=%d\n",
		__func__, LRP_TYPE_STRING[type], pdata->lrp_temp[type].recov[ST2][LCD_ON]);
	pr_info("%s: pdata->lrp_temp[%s].st1_icl=%d\n",
		__func__, LRP_TYPE_STRING[type], pdata->lrp_curr[type].st_icl[ST1]);
	pr_info("%s: pdata->lrp_temp[%s].st1_fcc=%d\n",
		__func__, LRP_TYPE_STRING[type], pdata->lrp_curr[type].st_fcc[ST1]);
	pr_info("%s: pdata->lrp_temp[%s].st2_icl=%d\n",
		__func__, LRP_TYPE_STRING[type], pdata->lrp_curr[type].st_icl[ST2]);
	pr_info("%s: pdata->lrp_temp[%s].st2_fcc=%d\n",
		__func__, LRP_TYPE_STRING[type], pdata->lrp_curr[type].st_fcc[ST2]);

	return 0;
}

#if IS_ENABLED(CONFIG_DIRECT_CHARGING)
static void sec_bat_parse_dc_thm(struct device_node *np, sec_battery_platform_data_t *pdata)
{
	int ret = 0, len = 0, i = 0;
	const u32 *p;
	int len_step = 4;
	char str[256] = {0, };

	pdata->dctp_by_cgtp = of_property_read_bool(np, "battery,dctp_by_cgtp");

	/* dchg_high_temp */
	p = of_get_property(np, "battery,dchg_high_temp", &len);
	if (!p) {
		pr_info("%s: failed to parse dchg_high_temp!\n", __func__);
		for (i = 0; i < len_step; i++)
			pdata->dchg_high_temp[i] = 690;
		return;
	}
	len = len / sizeof(u32);
	ret = of_property_read_u32_array(np, "battery,dchg_high_temp",
			pdata->dchg_high_temp, len);
	if (len != len_step) {
		pr_err("%s not match size of dchg_high_temp: %d\n", __func__, len);
		for (i = 1; i < len_step; i++)
			pdata->dchg_high_temp[i] = pdata->dchg_high_temp[0];
	}

	/* dchg_high_temp_recovery */
	p = of_get_property(np, "battery,dchg_high_temp_recovery", &len);
	if (!p) {
		pr_info("%s: failed to parse dchg_high_temp_recovery!\n", __func__);
		for (i = 0; i < len_step; i++)
			pdata->dchg_high_temp_recovery[i] = 630;
	}
	len = len / sizeof(u32);
	ret = of_property_read_u32_array(np, "battery,dchg_high_temp_recovery",
			pdata->dchg_high_temp_recovery, len);
	if (len != len_step) {
		pr_err("%s not match size of dchg_high_temp_recovery: %d\n", __func__, len);
		for (i = 1; i < len_step; i++)
			pdata->dchg_high_temp_recovery[i] = pdata->dchg_high_temp_recovery[0];
	}

	/* dchg_high_batt_temp */
	p = of_get_property(np, "battery,dchg_high_batt_temp", &len);
	if (!p) {
		pr_info("%s: failed to parse dchg_high_batt_temp!\n", __func__);
		for (i = 0; i < len_step; i++)
			pdata->dchg_high_batt_temp[i] = 400;
	}
	len = len / sizeof(u32);
	ret = of_property_read_u32_array(np, "battery,dchg_high_batt_temp",
			pdata->dchg_high_batt_temp, len);
	if (len != len_step) {
		pr_err("%s not match size of dchg_high_batt_temp: %d\n", __func__, len);
		for (i = 1; i < len_step; i++)
			pdata->dchg_high_batt_temp[i] = pdata->dchg_high_batt_temp[0];
	}

	/* dchg_high_batt_temp_recovery */
	p = of_get_property(np, "battery,dchg_high_batt_temp_recovery", &len);
	if (!p) {
		pr_info("%s: failed to parse dchg_high_batt_temp_recovery!\n", __func__);
		for (i = 0; i < len_step; i++)
			pdata->dchg_high_batt_temp_recovery[i] = 380;
	}
	len = len / sizeof(u32);
	ret = of_property_read_u32_array(np, "battery,dchg_high_batt_temp_recovery",
			pdata->dchg_high_batt_temp_recovery, len);
	if (len != len_step) {
		pr_err("%s not match size of dchg_high_batt_temp_recovery: %d\n", __func__, len);
		for (i = 1; i < len_step; i++)
			pdata->dchg_high_batt_temp_recovery[i] = pdata->dchg_high_batt_temp_recovery[0];
	}

	sprintf(str, "%s: dchg_htemp: ", __func__);
	for (i = 0; i < len_step; i++)
		sprintf(str + strlen(str), "%d ", pdata->dchg_high_temp[i]);
	sprintf(str + strlen(str), ",dchg_htemp_rec: ");
	for (i = 0; i < len_step; i++)
		sprintf(str + strlen(str), "%d ", pdata->dchg_high_temp_recovery[i]);
	sprintf(str + strlen(str), ",dchg_batt_htemp: ");
	for (i = 0; i < len_step; i++)
		sprintf(str + strlen(str), "%d ", pdata->dchg_high_batt_temp[i]);
	sprintf(str + strlen(str), ",dchg_batt_htemp_rec: ");
	for (i = 0; i < len_step; i++)
		sprintf(str + strlen(str), "%d ", pdata->dchg_high_batt_temp_recovery[i]);
	sprintf(str + strlen(str), "\n");
	pr_info("%s", str);
}
#else
static void sec_bat_parse_dc_thm(struct device_node *np, sec_battery_platform_data_t *pdata)
{
	pr_info("%s: direct charging is not set\n", __func__);
}
#endif

int sec_bat_parse_dt(struct device *dev,
		struct sec_battery_info *battery)
{
	struct device_node *np;
	sec_battery_platform_data_t *pdata = battery->pdata;
	int ret = 0, len = 0;
	unsigned int i = 0;
	const u32 *p;
	u32 temp = 0;

	np = of_find_node_by_name(NULL, "cable-info");
	if (!np) {
		pr_err ("%s : np NULL\n", __func__);
	} else {
		struct device_node *child;
		u32 input_current = 0, charging_current = 0;

		ret = of_property_read_u32(np, "default_input_current", &input_current);
		ret = of_property_read_u32(np, "default_charging_current", &charging_current);
		ret = of_property_read_u32(np, "full_check_current_1st", &pdata->full_check_current_1st);
		ret = of_property_read_u32(np, "full_check_current_2nd", &pdata->full_check_current_2nd);
		if (!pdata->full_check_current_2nd)
			pdata->full_check_current_2nd = pdata->full_check_current_1st;

		pdata->default_input_current = input_current;
		pdata->default_charging_current = charging_current;
		pdata->charging_current =
			kzalloc(sizeof(sec_charging_current_t) * SEC_BATTERY_CABLE_MAX,
				GFP_KERNEL);

		for (i = 0; i < SEC_BATTERY_CABLE_MAX; i++) {
			pdata->charging_current[i].input_current_limit = (unsigned int)input_current;
			pdata->charging_current[i].fast_charging_current = (unsigned int)charging_current;
		}

		for_each_child_of_node(np, child) {
			ret = of_property_read_u32(child, "input_current", &input_current);
			ret = of_property_read_u32(child, "charging_current", &charging_current);
			p = of_get_property(child, "cable_number", &len);
			if (!p)
				return 1;

			len = len / sizeof(u32);

			for (i = 0; i <= len; i++) {
				ret = of_property_read_u32_index(child, "cable_number", i, &temp);
				pdata->charging_current[temp].input_current_limit = (unsigned int)input_current;
				pdata->charging_current[temp].fast_charging_current = (unsigned int)charging_current;
			}
		}
	}

	for (i = 0; i < SEC_BATTERY_CABLE_MAX; i++) {
		pr_info("%s : CABLE_NUM(%d) INPUT(%d) CHARGING(%d)\n",
			__func__, i,
			pdata->charging_current[i].input_current_limit,
			pdata->charging_current[i].fast_charging_current);
	}

	np = of_find_node_by_name(NULL, "wireless-power-info");
	if (!np) {
		pr_err ("%s : np NULL\n", __func__);
	} else {
		struct device_node *child;
		u32 wireless_power_class = 0, vout = 0, input_current_limit = 0, fast_charging_current = 0, ttf_charge_current = 0, rx_power = 0;

		pdata->wireless_power_info =
			kzalloc(sizeof(sec_wireless_rx_power_info_t) * SEC_WIRELESS_RX_POWER_MAX,
				GFP_KERNEL);
		i = 0;
		for_each_child_of_node(np, child) {
			ret = of_property_read_u32(child, "vout", &vout);
			ret = of_property_read_u32(child, "input_current_limit", &input_current_limit);
			ret = of_property_read_u32(child, "fast_charging_current", &fast_charging_current);
			ret = of_property_read_u32(child, "ttf_charge_current", &ttf_charge_current);
			ret = of_property_read_u32(child, "rx_power", &rx_power);
			ret = of_property_read_u32(child, "wireless_power_class", &wireless_power_class);

			pdata->wireless_power_info[i].wireless_power_class = (unsigned int)wireless_power_class;
			pdata->wireless_power_info[i].vout = (unsigned int)vout;
			pdata->wireless_power_info[i].input_current_limit = (unsigned int)input_current_limit;
			pdata->wireless_power_info[i].fast_charging_current = (unsigned int)fast_charging_current;
			pdata->wireless_power_info[i].ttf_charge_current = (unsigned int)ttf_charge_current;
			pdata->wireless_power_info[i].rx_power = (unsigned int)rx_power;
			i++;
		}
		if (i > 0) {
			int len_wireless_power_info = i;

			while (i < SEC_WIRELESS_RX_POWER_MAX) {
				pdata->wireless_power_info[i].wireless_power_class = pdata->wireless_power_info[len_wireless_power_info-1].wireless_power_class;
				pdata->wireless_power_info[i].vout = pdata->wireless_power_info[len_wireless_power_info-1].vout;
				pdata->wireless_power_info[i].input_current_limit = pdata->wireless_power_info[len_wireless_power_info-1].input_current_limit;
				pdata->wireless_power_info[i].fast_charging_current = pdata->wireless_power_info[len_wireless_power_info-1].fast_charging_current;
				pdata->wireless_power_info[i].ttf_charge_current = pdata->wireless_power_info[len_wireless_power_info-1].ttf_charge_current;
				pdata->wireless_power_info[i].rx_power = pdata->wireless_power_info[len_wireless_power_info-1].rx_power;
				i++;
			}
		}
		for (i = 0; i < SEC_WIRELESS_RX_POWER_MAX; i++) {
			pr_info("%s : POWER_LIST(%d) POWER_CLASS(%d) VOUT(%d) INPUT(%d) CHARGING(%d) TTF(%d) POWER(%d)\n",
				__func__, i,
				pdata->wireless_power_info[i].wireless_power_class,
				pdata->wireless_power_info[i].vout,
				pdata->wireless_power_info[i].input_current_limit,
				pdata->wireless_power_info[i].fast_charging_current,
				pdata->wireless_power_info[i].ttf_charge_current,
				pdata->wireless_power_info[i].rx_power);
		}
	}

	pr_info("%s : TOPOFF_1ST(%d), TOPOFF_2ND(%d)\n",
		__func__, pdata->full_check_current_1st, pdata->full_check_current_2nd);

	pdata->default_usb_input_current = pdata->charging_current[SEC_BATTERY_CABLE_USB].input_current_limit;
	pdata->default_usb_charging_current = pdata->charging_current[SEC_BATTERY_CABLE_USB].fast_charging_current;
	pdata->default_wc20_input_current = pdata->charging_current[SEC_BATTERY_CABLE_HV_WIRELESS_20].input_current_limit;
	pdata->default_wc20_charging_current = pdata->charging_current[SEC_BATTERY_CABLE_HV_WIRELESS_20].fast_charging_current;
#ifdef CONFIG_SEC_FACTORY
	pdata->default_charging_current = 1500;
	pdata->charging_current[SEC_BATTERY_CABLE_TA].fast_charging_current = 1500;
#endif
	np = of_find_node_by_name(NULL, "battery");
	if (!np) {
		pr_info("%s: np NULL\n", __func__);
		return 1;
	}

	ret = of_property_read_u32(np, "battery,battery_full_capacity",
			&pdata->battery_full_capacity);

	if (ret)
		pr_info("%s : battery_full_capacity is Empty\n", __func__);

#ifdef CONFIG_SEC_FACTORY
	ret = of_property_read_u32(np, "battery,factory_chg_limit_max",
		&pdata->store_mode_charging_max);
	if (ret) {
		pr_info("%s :factory_chg_limit_max is Empty\n", __func__);
		pdata->store_mode_charging_max = 80;
	}

	ret = of_property_read_u32(np, "battery,factory_chg_limit_min",
		&pdata->store_mode_charging_min);
	if (ret) {
		pr_info("%s :factory_chg_limit_min is Empty\n", __func__);
		pdata->store_mode_charging_min = 70;
	}
#else
	ret = of_property_read_u32(np, "battery,store_mode_charging_max",
		&pdata->store_mode_charging_max);
	if (ret) {
		pr_info("%s :factory_chg_limit_max is Empty\n", __func__);
		pdata->store_mode_charging_max = 70;
	}

	ret = of_property_read_u32(np, "battery,store_mode_charging_min",
		&pdata->store_mode_charging_min);
	if (ret) {
		pr_info("%s :factory_chg_limit_min is Empty\n", __func__);
		pdata->store_mode_charging_min = 60;
	}
#if !defined(CONFIG_ARCH_EXYNOS)
	/* VZW's prepaid devices has "VPP" as sales_code, not "VZW" */
	if (sales_code_is("VZW") || sales_code_is("VPP")) {
		dev_err(battery->dev, "%s: Sales is VZW or VPP\n", __func__);

		pdata->store_mode_charging_max = 35;
		pdata->store_mode_charging_min = 30;
	}
#endif
#endif /*CONFIG_SEC_FACTORY */

#if defined(CONFIG_BATTERY_CISD)
	else {
		pr_info("%s : battery_full_capacity : %d\n", __func__, pdata->battery_full_capacity);
		pdata->cisd_cap_high_thr = pdata->battery_full_capacity + 1000; /* battery_full_capacity + 1000 */
		pdata->cisd_cap_low_thr = pdata->battery_full_capacity + 500; /* battery_full_capacity + 500 */
		pdata->cisd_cap_limit = (pdata->battery_full_capacity * 11) / 10; /* battery_full_capacity + 10% */
	}

	ret = of_property_read_u32(np, "battery,cisd_max_voltage_thr",
		&pdata->max_voltage_thr);
	if (ret) {
		pr_info("%s : cisd_max_voltage_thr is Empty\n", __func__);
		pdata->max_voltage_thr = 4400;
	}

	ret = of_property_read_u32(np, "battery,cisd_alg_index",
			&pdata->cisd_alg_index);
	if (ret) {
		pr_info("%s : cisd_alg_index is Empty. Defalut set to six\n", __func__);
		pdata->cisd_alg_index = 6;
	} else {
		pr_info("%s : set cisd_alg_index : %d\n", __func__, pdata->cisd_alg_index);
	}
#endif

	ret = of_property_read_u32(np,
				   "battery,expired_time", &temp);
	if (ret) {
		pr_info("expired time is empty\n");
		pdata->expired_time = 3 * 60 * 60;
	} else {
		pdata->expired_time = (unsigned int) temp;
	}
	pdata->expired_time *= 1000;
	battery->expired_time = pdata->expired_time;

	ret = of_property_read_u32(np,
				   "battery,recharging_expired_time", &temp);
	if (ret) {
		pr_info("expired time is empty\n");
		pdata->recharging_expired_time = 90 * 60;
	} else {
		pdata->recharging_expired_time = (unsigned int) temp;
	}
	pdata->recharging_expired_time *= 1000;

	ret = of_property_read_u32(np,
				   "battery,standard_curr", &pdata->standard_curr);
	if (ret) {
		pr_info("standard_curr is empty\n");
		pdata->standard_curr = 2150;
	}

	ret = of_property_read_string(np,
		"battery,vendor", (char const **)&pdata->vendor);
	if (ret)
		pr_info("%s: Vendor is Empty\n", __func__);

	ret = of_property_read_string(np,
		"battery,charger_name", (char const **)&pdata->charger_name);
	if (ret)
		pr_info("%s: Charger name is Empty\n", __func__);

	ret = of_property_read_string(np,
		"battery,otg_name", (char const **)&pdata->otg_name);
	if (ret)
		pr_info("%s: otg_name is Empty\n", __func__);

	ret = of_property_read_string(np,
		"battery,fuelgauge_name", (char const **)&pdata->fuelgauge_name);
	if (ret)
		pr_info("%s: Fuelgauge name is Empty\n", __func__);

	ret = of_property_read_string(np,
		"battery,wireless_charger_name", (char const **)&pdata->wireless_charger_name);
	if (ret)
		pr_info("%s: Wireless charger name is Empty\n", __func__);

	ret = of_property_read_string(np,
		"battery,fgsrc_switch_name", (char const **)&pdata->fgsrc_switch_name);
	if (ret) {
		pdata->support_fgsrc_change = false;
		pr_info("%s: fgsrc_switch_name is Empty\n", __func__);
	}
	else
		pdata->support_fgsrc_change = true;

	pdata->chg_vbus_control_after_fullcharged = of_property_read_bool(np,
								"battery,chg_vbus_control_after_fullcharged");

	pdata->slowcharging_usb_bootcomplete = of_property_read_bool(np,
						     "battery,slowcharging_usb_bootcomplete");

	ret = of_property_read_string(np,
		"battery,chip_vendor", (char const **)&pdata->chip_vendor);
	if (ret)
		pr_info("%s: Chip vendor is Empty\n", __func__);

	ret = of_property_read_u32(np, "battery,technology",
		&pdata->technology);
	if (ret)
		pr_info("%s : technology is Empty\n", __func__);

	ret = of_property_read_u32(np,
		"battery,wireless_cc_cv", &pdata->wireless_cc_cv);

	pdata->fake_capacity = of_property_read_bool(np,
						     "battery,fake_capacity");

	pdata->dis_auto_shipmode_temp_ctrl = of_property_read_bool(np,
						     "battery,dis_auto_shipmode_temp_ctrl");

	battery->ta_alert_wa = of_property_read_bool(np, "battery,ta_alert_wa");

	p = of_get_property(np, "battery,polling_time", &len);
	if (!p)
		return 1;

	len = len / sizeof(u32);
	pdata->polling_time = kzalloc(sizeof(*pdata->polling_time) * len, GFP_KERNEL);
	ret = of_property_read_u32_array(np, "battery,polling_time",
					 pdata->polling_time, len);
	if (ret)
		pr_info("%s : battery,polling_time is Empty\n", __func__);

	/* battery thermistor */
	ret = of_property_read_u32(np, "battery,thermal_source",
		&pdata->thermal_source);
	if (ret)
		pr_info("%s : Thermal source is Empty\n", __func__);

	if (pdata->thermal_source == SEC_BATTERY_THERMAL_SOURCE_ADC) {
		p = of_get_property(np, "battery,temp_table_adc", &len);
		if (!p)
			return 1;

		len = len / sizeof(u32);

		pdata->temp_adc_table_size = len;
		pdata->temp_amb_adc_table_size = len;

		pdata->temp_adc_table =
			kzalloc(sizeof(sec_bat_adc_table_data_t) *
				pdata->temp_adc_table_size, GFP_KERNEL);
		pdata->temp_amb_adc_table =
			kzalloc(sizeof(sec_bat_adc_table_data_t) *
				pdata->temp_adc_table_size, GFP_KERNEL);

		for (i = 0; i < pdata->temp_adc_table_size; i++) {
			ret = of_property_read_u32_index(np,
					 "battery,temp_table_adc", i, &temp);
			pdata->temp_adc_table[i].adc = (int)temp;
			if (ret)
				pr_info("%s : Temp_adc_table(adc) is Empty\n",
					__func__);

			ret = of_property_read_u32_index(np,
							 "battery,temp_table_data", i, &temp);
			pdata->temp_adc_table[i].data = (int)temp;
			if (ret)
				pr_info("%s : Temp_adc_table(data) is Empty\n",
					__func__);

			ret = of_property_read_u32_index(np,
							 "battery,temp_table_adc", i, &temp);
			pdata->temp_amb_adc_table[i].adc = (int)temp;
			if (ret)
				pr_info("%s : Temp_amb_adc_table(adc) is Empty\n",
					__func__);

			ret = of_property_read_u32_index(np,
							 "battery,temp_table_data", i, &temp);
			pdata->temp_amb_adc_table[i].data = (int)temp;
			if (ret)
				pr_info("%s : Temp_amb_adc_table(data) is Empty\n",
					__func__);
		}
	}
	ret = of_property_read_u32(np, "battery,temp_check_type",
		&pdata->temp_check_type);
	if (ret)
		pr_info("%s : Temp check type is Empty\n", __func__);

	ret = of_property_read_u32(np, "battery,temp_check_count",
		&pdata->temp_check_count);
	if (ret)
		pr_info("%s : Temp check count is Empty\n", __func__);

	/* usb thermistor */
	ret = of_property_read_u32(np, "battery,usb_thermal_source",
		&pdata->usb_thermal_source);
	if (ret)
		pr_info("%s : usb_thermal_source is Empty\n", __func__);

	if (pdata->usb_thermal_source == SEC_BATTERY_THERMAL_SOURCE_ADC) {
		p = of_get_property(np, "battery,usb_temp_table_adc", &len);
		if (!p)
			return 1;

		len = len / sizeof(u32);

		pdata->usb_temp_adc_table_size = len;

		pdata->usb_temp_adc_table =
			kzalloc(sizeof(sec_bat_adc_table_data_t) *
				pdata->usb_temp_adc_table_size, GFP_KERNEL);

		for (i = 0; i < pdata->usb_temp_adc_table_size; i++) {
			ret = of_property_read_u32_index(np,
							 "battery,usb_temp_table_adc", i, &temp);
			pdata->usb_temp_adc_table[i].adc = (int)temp;
			if (ret)
				pr_info("%s : Usb_Temp_adc_table(adc) is Empty\n",
					__func__);

			ret = of_property_read_u32_index(np,
							 "battery,usb_temp_table_data", i, &temp);
			pdata->usb_temp_adc_table[i].data = (int)temp;
			if (ret)
				pr_info("%s : Usb_Temp_adc_table(data) is Empty\n",
					__func__);
		}
	}
	ret = of_property_read_u32(np, "battery,usb_temp_check_type",
		&pdata->usb_temp_check_type);
	if (ret)
		pr_info("%s : usb_temp_check_type is Empty\n", __func__);

	/* chg thermistor */
	ret = of_property_read_u32(np, "battery,chg_thermal_source",
		&pdata->chg_thermal_source);
	if (ret)
		pr_info("%s : chg_thermal_source is Empty\n", __func__);

	if (pdata->chg_thermal_source == SEC_BATTERY_THERMAL_SOURCE_ADC) {
		p = of_get_property(np, "battery,chg_temp_table_adc", &len);
		if (!p)
			return 1;

		len = len / sizeof(u32);

		pdata->chg_temp_adc_table_size = len;

		pdata->chg_temp_adc_table =
			kzalloc(sizeof(sec_bat_adc_table_data_t) *
				pdata->chg_temp_adc_table_size, GFP_KERNEL);

		for (i = 0; i < pdata->chg_temp_adc_table_size; i++) {
			ret = of_property_read_u32_index(np,
							 "battery,chg_temp_table_adc", i, &temp);
			pdata->chg_temp_adc_table[i].adc = (int)temp;
			if (ret)
				pr_info("%s : CHG_Temp_adc_table(adc) is Empty\n",
					__func__);

			ret = of_property_read_u32_index(np,
							 "battery,chg_temp_table_data", i, &temp);
			pdata->chg_temp_adc_table[i].data = (int)temp;
			if (ret)
				pr_info("%s : CHG_Temp_adc_table(data) is Empty\n",
					__func__);
		}
	}
	ret = of_property_read_u32(np, "battery,chg_temp_check_type",
		&pdata->chg_temp_check_type);
	if (ret)
		pr_info("%s : chg_temp_check_type is Empty\n", __func__);

#if IS_ENABLED(CONFIG_DIRECT_CHARGING)
	/* direct chg thermistor */
	ret = of_property_read_u32(np, "battery,dchg_thermal_source",
		&pdata->dchg_thermal_source);
	if (ret)
		pr_info("%s : dchg_thermal_source is Empty\n", __func__);

	if (pdata->dchg_thermal_source == SEC_BATTERY_THERMAL_SOURCE_CHG_ADC) {
		p = of_get_property(np, "battery,dchg_temp_table_adc", &len);
		if (!p)
			return 1;

		len = len / sizeof(u32);

		pdata->dchg_temp_adc_table_size = len;

		pdata->dchg_temp_adc_table =
			kzalloc(sizeof(sec_bat_adc_table_data_t) *
				pdata->dchg_temp_adc_table_size, GFP_KERNEL);

		for (i = 0; i < pdata->dchg_temp_adc_table_size; i++) {
			ret = of_property_read_u32_index(np,
							 "battery,dchg_temp_table_adc", i, &temp);
			pdata->dchg_temp_adc_table[i].adc = (int)temp;
			if (ret)
				pr_info("%s : DIRECT CHG_Temp_adc_table(adc) is Empty\n",
					__func__);

			ret = of_property_read_u32_index(np,
							 "battery,dchg_temp_table_data", i, &temp);
			pdata->dchg_temp_adc_table[i].data = (int)temp;
			if (ret)
				pr_info("%s : DIRECT CHG_Temp_adc_table(data) is Empty\n",
					__func__);
		}
	}
	ret = of_property_read_u32(np, "battery,dchg_temp_check_type",
		&pdata->dchg_temp_check_type);
	if (ret)
		pr_info("%s : dchg_temp_check_type is Empty\n", __func__);
#endif
	/* parse dc thm info */
	sec_bat_parse_dc_thm(np, pdata);

	/* wpc thermistor */
	ret = of_property_read_u32(np, "battery,wpc_thermal_source",
		&pdata->wpc_thermal_source);
	if (ret)
		pr_info("%s : wpc_thermal_source is Empty\n", __func__);

	if (pdata->wpc_thermal_source == SEC_BATTERY_THERMAL_SOURCE_ADC) {
		p = of_get_property(np, "battery,wpc_temp_table_adc", &len);
		if (!p) {
			pr_info("%s : wpc_temp_table_adc(adc) is Empty\n",__func__);
		} else {
			len = len / sizeof(u32);

			pdata->wpc_temp_adc_table_size = len;

			pdata->wpc_temp_adc_table =
				kzalloc(sizeof(sec_bat_adc_table_data_t) *
					pdata->wpc_temp_adc_table_size, GFP_KERNEL);

			for (i = 0; i < pdata->wpc_temp_adc_table_size; i++) {
				ret = of_property_read_u32_index(np,
								 "battery,wpc_temp_table_adc", i, &temp);
				pdata->wpc_temp_adc_table[i].adc = (int)temp;
				if (ret)
					pr_info("%s : WPC_Temp_adc_table(adc) is Empty\n",
						__func__);

				ret = of_property_read_u32_index(np,
								 "battery,wpc_temp_table_data", i, &temp);
				pdata->wpc_temp_adc_table[i].data = (int)temp;
				if (ret)
					pr_info("%s : WPC_Temp_adc_table(data) is Empty\n",
						__func__);
			}
		}
	}
	ret = of_property_read_u32(np, "battery,wpc_temp_check_type",
		&pdata->wpc_temp_check_type);
	if (ret)
		pr_info("%s : wpc_temp_check_type is Empty\n", __func__);

	/* sub bat thermistor */
	ret = of_property_read_u32(np, "battery,sub_bat_thermal_source",
		&pdata->sub_bat_thermal_source);
	if (ret)
		pr_info("%s : sub_bat_thermal_source is Empty\n", __func__);

	if (pdata->sub_bat_thermal_source) {
		p = of_get_property(np, "battery,sub_bat_temp_table_adc", &len);
		if (!p) {
			pr_info("%s : sub_bat_temp_table_adc(adc) is Empty\n",__func__);
		} else {
			len = len / sizeof(u32);

			pdata->sub_bat_temp_adc_table_size = len;

			pdata->sub_bat_temp_adc_table =
				kzalloc(sizeof(sec_bat_adc_table_data_t) *
					pdata->sub_bat_temp_adc_table_size, GFP_KERNEL);

			for (i = 0; i < pdata->sub_bat_temp_adc_table_size; i++) {
				ret = of_property_read_u32_index(np,
								 "battery,sub_bat_temp_table_adc", i, &temp);
				pdata->sub_bat_temp_adc_table[i].adc = (int)temp;
				if (ret)
					pr_info("%s : sub_bat_adc_table(adc) is Empty\n",
						__func__);

				ret = of_property_read_u32_index(np,
								 "battery,sub_bat_temp_table_data", i, &temp);
				pdata->sub_bat_temp_adc_table[i].data = (int)temp;
				if (ret)
					pr_info("%s : sub_bat_temp_adc_table(data) is Empty\n",
						__func__);
			}
		}
	}
	ret = of_property_read_u32(np, "battery,sub_bat_temp_check_type",
		&pdata->sub_bat_temp_check_type);
	if (ret)
		pr_info("%s : sub_bat_temp_check_type is Empty\n", __func__);

	/* sub chg thermistor */
	ret = of_property_read_u32(np, "battery,sub_chg_thermal_source",
		&pdata->sub_chg_thermal_source);
	if (ret)
		pr_info("%s : sub_chg_thermal_source is Empty\n", __func__);

	if (pdata->sub_chg_thermal_source == SEC_BATTERY_THERMAL_SOURCE_ADC) {
		p = of_get_property(np, "battery,sub_chg_temp_table_adc", &len);
		if (!p)
			return 1;

		len = len / sizeof(u32);

		pdata->sub_chg_temp_adc_table_size = len;

		pdata->sub_chg_temp_adc_table =
			kzalloc(sizeof(sec_bat_adc_table_data_t) *
				pdata->sub_chg_temp_adc_table_size, GFP_KERNEL);

		for (i = 0; i < pdata->sub_chg_temp_adc_table_size; i++) {
			ret = of_property_read_u32_index(np,
							 "battery,sub_chg_temp_table_adc", i, &temp);
			pdata->sub_chg_temp_adc_table[i].adc = (int)temp;
			if (ret)
				pr_info("%s : sub_chg_temp_adc_table(adc) is Empty\n",
					__func__);

			ret = of_property_read_u32_index(np,
							 "battery,sub_chg_temp_table_data", i, &temp);
			pdata->sub_chg_temp_adc_table[i].data = (int)temp;
			if (ret)
				pr_info("%s : sub_chg_temp_adc_table(data) is Empty\n",
					__func__);
		}
	}
	ret = of_property_read_u32(np, "battery,sub_chg_temp_check_type",
		&pdata->sub_chg_temp_check_type);
	if (ret)
		pr_info("%s : sub_chg_temp_check_type is Empty\n", __func__);

	/* blkt thermistor */
	ret = of_property_read_u32(np, "battery,blkt_thermal_source",
		&pdata->blkt_thermal_source);
	if (ret)
		pr_info("%s : blkt_thermal_source is Empty\n", __func__);

	if (pdata->blkt_thermal_source == SEC_BATTERY_THERMAL_SOURCE_ADC) {
		p = of_get_property(np, "battery,blkt_temp_table_adc", &len);
		if (!p)
			return 1;

		len = len / sizeof(u32);

		pdata->blkt_temp_adc_table_size = len;

		pdata->blkt_temp_adc_table =
			kzalloc(sizeof(sec_bat_adc_table_data_t) *
				pdata->blkt_temp_adc_table_size, GFP_KERNEL);

		for (i = 0; i < pdata->blkt_temp_adc_table_size; i++) {
			ret = of_property_read_u32_index(np,
							 "battery,blkt_temp_table_adc", i, &temp);
			pdata->blkt_temp_adc_table[i].adc = (int)temp;
			if (ret)
				pr_info("%s : blkt_temp_adc_table(adc) is Empty\n",
					__func__);

			ret = of_property_read_u32_index(np,
							 "battery,blkt_temp_table_data", i, &temp);
			pdata->blkt_temp_adc_table[i].data = (int)temp;
			if (ret)
				pr_info("%s : blkt_temp_adc_table(data) is Empty\n",
					__func__);
		}
	}
	ret = of_property_read_u32(np, "battery,blkt_temp_check_type",
		&pdata->blkt_temp_check_type);
	if (ret)
		pr_info("%s : blkt_temp_check_type is Empty\n", __func__);

	if (pdata->chg_temp_check_type) {
		ret = of_property_read_u32(np, "battery,chg_12v_high_temp",
					   &temp);
		pdata->chg_12v_high_temp = (int)temp;
		if (ret)
			pr_info("%s : chg_12v_high_temp is Empty\n", __func__);

		ret = of_property_read_u32(np, "battery,chg_high_temp",
					   &temp);
		pdata->chg_high_temp = (int)temp;
		if (ret)
			pr_info("%s : chg_high_temp is Empty\n", __func__);

		ret = of_property_read_u32(np, "battery,chg_high_temp_recovery",
					   &temp);
		pdata->chg_high_temp_recovery = (int)temp;
		if (ret)
			pr_info("%s : chg_temp_recovery is Empty\n", __func__);

		ret = of_property_read_u32(np, "battery,chg_charging_limit_current",
					   &pdata->chg_charging_limit_current);
		if (ret)
			pr_info("%s : chg_charging_limit_current is Empty\n", __func__);

		ret = of_property_read_u32(np, "battery,chg_input_limit_current",
					   &pdata->chg_input_limit_current);
		if (ret)
			pr_info("%s : chg_input_limit_current is Empty\n", __func__);

		ret = of_property_read_u32(np, "battery,dchg_charging_limit_current",
					   &pdata->dchg_charging_limit_current);
		if (ret) {
			pr_info("%s : dchg_charging_limit_current is Empty\n", __func__);
			pdata->dchg_charging_limit_current = pdata->chg_charging_limit_current;
		}

		ret = of_property_read_u32(np, "battery,dchg_input_limit_current",
					   &pdata->dchg_input_limit_current);
		if (ret) {
			pr_info("%s : dchg_input_limit_current is Empty\n", __func__);
			pdata->dchg_input_limit_current = pdata->chg_input_limit_current;
		}

		ret = of_property_read_u32(np, "battery,mix_high_temp",
					   &temp);
		pdata->mix_high_temp = (int)temp;
		if (ret)
			pr_info("%s : mix_high_temp is Empty\n", __func__);

		ret = of_property_read_u32(np, "battery,mix_high_chg_temp",
					   &temp);
		pdata->mix_high_chg_temp = (int)temp;
		if (ret)
			pr_info("%s : mix_high_chg_temp is Empty\n", __func__);

		ret = of_property_read_u32(np, "battery,mix_high_temp_recovery",
					   &temp);
		pdata->mix_high_temp_recovery = (int)temp;
		if (ret)
			pr_info("%s : mix_high_temp_recovery is Empty\n", __func__);
	}

	ret = of_property_read_u32(np, "battery,lrp_temp_check_type",
			&pdata->lrp_temp_check_type);
	if (ret)
		pr_info("%s : lrp_temp_check_type is Empty\n", __func__);

	if (pdata->lrp_temp_check_type) {
		for (i = 0; i < LRP_MAX; i++) {
			if (sec_bat_parse_dt_lrp(battery, np, i) < 0) {
				pdata->lrp_temp[i].trig[ST1][LCD_OFF] = 375;
				pdata->lrp_temp[i].trig[ST2][LCD_OFF] = 375;
				pdata->lrp_temp[i].recov[ST1][LCD_OFF] = 365;
				pdata->lrp_temp[i].recov[ST2][LCD_OFF] = 365;
				pdata->lrp_temp[i].trig[ST1][LCD_ON] = 375;
				pdata->lrp_temp[i].trig[ST2][LCD_ON] = 375;
				pdata->lrp_temp[i].recov[ST1][LCD_ON] = 365;
				pdata->lrp_temp[i].recov[ST2][LCD_ON] = 365;
				pdata->lrp_curr[i].st_icl[0] = pdata->default_input_current;
				pdata->lrp_curr[i].st_fcc[0] = pdata->default_charging_current;
				pdata->lrp_curr[i].st_icl[1] = pdata->default_input_current;
				pdata->lrp_curr[i].st_fcc[1] = pdata->default_charging_current;
			}
		}
	}

	if (pdata->wpc_temp_check_type) {
		ret = of_property_read_u32(np, "battery,wpc_temp_control_source",
				&pdata->wpc_temp_control_source);
		if (ret) {
			pr_info("%s : wpc_temp_control_source is Empty\n", __func__);
			pdata->wpc_temp_control_source = TEMP_CONTROL_SOURCE_CHG_THM;
		}

		ret = of_property_read_u32(np, "battery,wpc_temp_lcd_on_control_source",
				&pdata->wpc_temp_lcd_on_control_source);
		if (ret) {
			pr_info("%s : wpc_temp_lcd_on_control_source is Empty\n", __func__);
			pdata->wpc_temp_lcd_on_control_source = TEMP_CONTROL_SOURCE_CHG_THM;
		}

		ret = of_property_read_u32(np, "battery,wpc_high_temp",
				&pdata->wpc_high_temp);
		if (ret)
			pr_info("%s : wpc_high_temp is Empty\n", __func__);

		ret = of_property_read_u32(np, "battery,wpc_high_temp_recovery",
				&pdata->wpc_high_temp_recovery);
		if (ret)
			pr_info("%s : wpc_high_temp_recovery is Empty\n", __func__);

		ret = of_property_read_u32(np, "battery,wpc_input_limit_current",
				&pdata->wpc_input_limit_current);
		if (ret)
			pr_info("%s : wpc_input_limit_current is Empty\n", __func__);

		ret = of_property_read_u32(np, "battery,wpc_charging_limit_current",
				&pdata->wpc_charging_limit_current);
		if (ret)
			pr_info("%s : wpc_charging_limit_current is Empty\n", __func__);

		ret = of_property_read_u32(np, "battery,wpc_lcd_on_high_temp",
				&pdata->wpc_lcd_on_high_temp);
		if (ret)
			pr_info("%s : wpc_lcd_on_high_temp is Empty\n", __func__);

		ret = of_property_read_u32(np, "battery,wpc_lcd_on_high_temp_rec",
				&pdata->wpc_lcd_on_high_temp_rec);
		if (ret)
			pr_info("%s : wpc_lcd_on_high_temp_rec is Empty\n", __func__);

		ret = of_property_read_u32(np, "battery,wpc_lcd_on_input_limit_current",
				&pdata->wpc_lcd_on_input_limit_current);
		if (ret) {
			pr_info("%s : wpc_lcd_on_input_limit_current is Empty\n", __func__);
			pdata->wpc_lcd_on_input_limit_current =
				pdata->wpc_input_limit_current;
		}

		battery->wpc_vout_ctrl_lcd_on = of_property_read_bool(np,
				"battery,wpc_vout_ctrl_lcd_on");

		len = of_property_count_u32_elems(np, "battery,wpc_step_limit_temp");
		if (len > 0) {
			pdata->wpc_step_limit_size = len;
			len = of_property_count_u32_elems(np, "battery,wpc_step_limit_fcc");
			if (pdata->wpc_step_limit_size != len) {
				pr_err("%s: not matched, wpc_step_limit_temp is %d, wpc_step_limit_fcc is %d\n",
						 __func__, pdata->wpc_step_limit_size, len);
				pdata->wpc_step_limit_size = 0;
			} else {
				pdata->wpc_step_limit_temp =
					kcalloc(pdata->wpc_step_limit_size, sizeof(unsigned int), GFP_KERNEL);
				ret = of_property_read_u32_array(np, "battery,wpc_step_limit_temp",
							(u32 *)pdata->wpc_step_limit_temp, pdata->wpc_step_limit_size);
				if (ret < 0) {
					pr_err("%s failed to read battery,wpc_step_limit_temp: %d\n",
							 __func__, ret);

					kfree(pdata->wpc_step_limit_temp);
					pdata->wpc_step_limit_temp = NULL;
				}

				pdata->wpc_step_limit_fcc =
					kcalloc(pdata->wpc_step_limit_size, sizeof(unsigned int), GFP_KERNEL);
				ret = of_property_read_u32_array(np, "battery,wpc_step_limit_fcc",
							(u32 *)pdata->wpc_step_limit_fcc, pdata->wpc_step_limit_size);
				if (ret < 0) {
					pr_err("%s failed to read battery,wpc_step_limit_fcc: %d\n",
							 __func__, ret);

					kfree(pdata->wpc_step_limit_fcc);
					pdata->wpc_step_limit_fcc = NULL;
				}

				if (!pdata->wpc_step_limit_temp || !pdata->wpc_step_limit_fcc) {
					pdata->wpc_step_limit_size = 0;
				} else {
					for (i = 0; i < pdata->wpc_step_limit_size; ++i) {
						pr_info("%s: wpc_step temp:%d, fcc:%d\n", __func__,
							pdata->wpc_step_limit_temp[i], pdata->wpc_step_limit_fcc[i]);
					}
				}
			}

			len = of_property_count_u32_elems(np, "battery,wpc_step_limit_fcc_12w");
			if (pdata->wpc_step_limit_size != len) {
				pr_err("%s: not matched, wpc_step_limit_temp is %d, wpc_step_limit_fcc_12w is %d\n",
						 __func__, pdata->wpc_step_limit_size, len);
				pdata->wpc_step_limit_fcc_12w =
					kcalloc(pdata->wpc_step_limit_size, sizeof(unsigned int), GFP_KERNEL);
				for (i = 0; i < pdata->wpc_step_limit_size; ++i) {
					pdata->wpc_step_limit_fcc_12w[i] = pdata->wpc_step_limit_fcc[i];
					pr_info("%s: wpc_step temp:%d, fcc_12w:%d\n", __func__,
						pdata->wpc_step_limit_temp[i], pdata->wpc_step_limit_fcc_12w[i]);
				}
			} else {
				pdata->wpc_step_limit_fcc_12w =
					kcalloc(pdata->wpc_step_limit_size, sizeof(unsigned int), GFP_KERNEL);
				ret = of_property_read_u32_array(np, "battery,wpc_step_limit_fcc_12w",
							(u32 *)pdata->wpc_step_limit_fcc_12w,
							pdata->wpc_step_limit_size);
				if (ret < 0) {
					pr_err("%s failed to read battery,wpc_step_limit_fcc_12w: %d\n",
							 __func__, ret);

					for (i = 0; i < pdata->wpc_step_limit_size; ++i) {
						pdata->wpc_step_limit_fcc_12w[i] = pdata->wpc_step_limit_fcc[i];
						pr_info("%s: wpc_step temp:%d, fcc_12w:%d\n", __func__,
							pdata->wpc_step_limit_temp[i],
							pdata->wpc_step_limit_fcc_12w[i]);
					}
				} else {
					for (i = 0; i < pdata->wpc_step_limit_size; ++i) {
						pr_info("%s: wpc_step temp:%d, fcc_12w:%d\n", __func__,
							pdata->wpc_step_limit_temp[i],
							pdata->wpc_step_limit_fcc_12w[i]);
					}
				}
			}

			len = of_property_count_u32_elems(np, "battery,wpc_step_limit_fcc_15w");
			if (pdata->wpc_step_limit_size != len) {
				pr_err("%s: not matched, wpc_step_limit_temp is %d, wpc_step_limit_fcc_15w is %d\n",
						 __func__, pdata->wpc_step_limit_size, len);
				pdata->wpc_step_limit_fcc_15w =
					kcalloc(pdata->wpc_step_limit_size, sizeof(unsigned int), GFP_KERNEL);
				for (i = 0; i < pdata->wpc_step_limit_size; ++i) {
					pdata->wpc_step_limit_fcc_15w[i] = pdata->wpc_step_limit_fcc[i];
					pr_info("%s: wpc_step temp:%d, fcc_15w:%d\n", __func__,
						pdata->wpc_step_limit_temp[i], pdata->wpc_step_limit_fcc_15w[i]);
				}
			} else {
				pdata->wpc_step_limit_fcc_15w =
					kcalloc(pdata->wpc_step_limit_size, sizeof(unsigned int), GFP_KERNEL);
				ret = of_property_read_u32_array(np, "battery,wpc_step_limit_fcc_15w",
							(u32 *)pdata->wpc_step_limit_fcc_15w,
							pdata->wpc_step_limit_size);
				if (ret < 0) {
					pr_err("%s failed to read battery,wpc_step_limit_fcc_15w: %d\n",
							 __func__, ret);

					for (i = 0; i < pdata->wpc_step_limit_size; ++i) {
						pdata->wpc_step_limit_fcc_15w[i] = pdata->wpc_step_limit_fcc[i];
						pr_info("%s: wpc_step temp:%d, fcc_15w:%d\n", __func__,
							pdata->wpc_step_limit_temp[i],
							pdata->wpc_step_limit_fcc_15w[i]);
					}
				} else {
					for (i = 0; i < pdata->wpc_step_limit_size; ++i) {
						pr_info("%s: wpc_step temp:%d, fcc_15w:%d\n", __func__,
							pdata->wpc_step_limit_temp[i],
							pdata->wpc_step_limit_fcc_15w[i]);
					}
				}
			}
		} else {
			pdata->wpc_step_limit_size = 0;
			pr_info("%s : wpc_step_limit_temp is Empty. len(%d), wpc_step_limit_size(%d)\n",
				__func__, len, pdata->wpc_step_limit_size);
		}

	}

	ret = of_property_read_u32(np, "battery,wc_full_input_limit_current",
		&pdata->wc_full_input_limit_current);
	if (ret)
		pr_info("%s : wc_full_input_limit_current is Empty\n", __func__);

	ret = of_property_read_u32(np, "battery,wc_hero_stand_cc_cv",
		&pdata->wc_hero_stand_cc_cv);
	if (ret) {
		pr_info("%s : wc_hero_stand_cc_cv is Empty\n", __func__);
		pdata->wc_hero_stand_cc_cv = 70;
	}
	ret = of_property_read_u32(np, "battery,wc_hero_stand_cv_current",
		&pdata->wc_hero_stand_cv_current);
	if (ret) {
		pr_info("%s : wc_hero_stand_cv_current is Empty\n", __func__);
		pdata->wc_hero_stand_cv_current = 600;
	}
	ret = of_property_read_u32(np, "battery,wc_hero_stand_hv_cv_current",
		&pdata->wc_hero_stand_hv_cv_current);
	if (ret) {
		pr_info("%s : wc_hero_stand_hv_cv_current is Empty\n", __func__);
		pdata->wc_hero_stand_hv_cv_current = 450;
	}

	ret = of_property_read_u32(np, "battery,sleep_mode_limit_current",
			&pdata->sleep_mode_limit_current);
	if (ret)
		pr_info("%s : sleep_mode_limit_current is Empty\n", __func__);

	ret = of_property_read_u32(np, "battery,inbat_voltage",
			&pdata->inbat_voltage);
	if (ret)
		pr_info("%s : inbat_voltage is Empty\n", __func__);

	if (pdata->inbat_voltage) {
		p = of_get_property(np, "battery,inbat_voltage_table_adc", &len);
		if (!p)
			return 1;

		len = len / sizeof(u32);

		pdata->inbat_adc_table_size = len;

		pdata->inbat_adc_table =
			kzalloc(sizeof(sec_bat_adc_table_data_t) *
					pdata->inbat_adc_table_size, GFP_KERNEL);

		for (i = 0; i < pdata->inbat_adc_table_size; i++) {
			ret = of_property_read_u32_index(np,
							 "battery,inbat_voltage_table_adc", i, &temp);
			pdata->inbat_adc_table[i].adc = (int)temp;
			if (ret)
				pr_info("%s : inbat_adc_table(adc) is Empty\n",
						__func__);

			ret = of_property_read_u32_index(np,
							 "battery,inbat_voltage_table_data", i, &temp);
			pdata->inbat_adc_table[i].data = (int)temp;
			if (ret)
				pr_info("%s : inbat_adc_table(data) is Empty\n",
						__func__);
		}
	}

	ret = of_property_read_u32(np, "battery,pre_afc_input_current",
		&pdata->pre_afc_input_current);
	if (ret) {
		pr_info("%s : pre_afc_input_current is Empty\n", __func__);
		pdata->pre_afc_input_current = 1000;
	}

	ret = of_property_read_u32(np, "battery,select_pd_input_current",
		&pdata->select_pd_input_current);
	if (ret) {
		pr_info("%s : select_pd_input_current is Empty\n", __func__);
		pdata->select_pd_input_current = 1000;
	}

	ret = of_property_read_u32(np, "battery,pre_afc_work_delay",
			&pdata->pre_afc_work_delay);
	if (ret) {
		pr_info("%s : pre_afc_work_delay is Empty\n", __func__);
		pdata->pre_afc_work_delay = 2000;
	}

	ret = of_property_read_u32(np, "battery,pre_wc_afc_input_current",
		&pdata->pre_wc_afc_input_current);
	if (ret) {
		pr_info("%s : pre_wc_afc_input_current is Empty\n", __func__);
		pdata->pre_wc_afc_input_current = 500; /* wc input default */
	}

	ret = of_property_read_u32(np, "battery,pre_wc_afc_work_delay",
			&pdata->pre_wc_afc_work_delay);
	if (ret) {
		pr_info("%s : pre_wc_afc_work_delay is Empty\n", __func__);
		pdata->pre_wc_afc_work_delay = 4000;
	}

	ret = of_property_read_u32(np, "battery,prepare_ta_delay",
			&pdata->prepare_ta_delay);
	if (ret) {
		pr_info("%s : prepare_ta_delay is Empty\n", __func__);
		pdata->prepare_ta_delay = 500;
	}

	ret = of_property_read_u32(np, "battery,select_pd_input_current",
		&pdata->select_pd_input_current);
	if (ret) {
		pr_info("%s : select_pd_input_current is Empty\n", __func__);
		pdata->select_pd_input_current = 1000;
	}

	ret = of_property_read_u32(np, "battery,tx_stop_capacity",
		&pdata->tx_stop_capacity);
	if (ret) {
		pr_info("%s : tx_stop_capacity is Empty\n", __func__);
		pdata->tx_stop_capacity = 30;
	}

	ret = of_property_read_u32(np, "battery,adc_check_count",
		&pdata->adc_check_count);
	if (ret)
		pr_info("%s : Adc check count is Empty\n", __func__);

	ret = of_property_read_u32(np, "battery,cable_check_type",
		&pdata->cable_check_type);
	if (ret)
		pr_info("%s : Cable check type is Empty\n", __func__);

	ret = of_property_read_u32(np, "battery,cable_source_type",
		&pdata->cable_source_type);
	if (ret)
		pr_info("%s: Cable_source_type is Empty\n", __func__);
#if defined(CONFIG_CHARGING_VZWCONCEPT)
	pdata->cable_check_type &= ~SEC_BATTERY_CABLE_CHECK_NOUSBCHARGE;
	pdata->cable_check_type |= SEC_BATTERY_CABLE_CHECK_NOINCOMPATIBLECHARGE;
#endif
	ret = of_property_read_u32(np, "battery,polling_type",
		&pdata->polling_type);
	if (ret)
		pr_info("%s : Polling type is Empty\n", __func__);

	ret = of_property_read_u32(np, "battery,monitor_initial_count",
		&pdata->monitor_initial_count);
	if (ret)
		pr_info("%s : Monitor initial count is Empty\n", __func__);

	ret = of_property_read_u32(np, "battery,battery_check_type",
		&pdata->battery_check_type);
	if (ret)
		pr_info("%s : Battery check type is Empty\n", __func__);

	ret = of_property_read_u32(np, "battery,check_count",
		&pdata->check_count);
	if (ret)
		pr_info("%s : Check count is Empty\n", __func__);

	ret = of_property_read_u32(np, "battery,check_adc_max",
		&pdata->check_adc_max);
	if (ret)
		pr_info("%s : Check adc max is Empty\n", __func__);

	ret = of_property_read_u32(np, "battery,check_adc_min",
		&pdata->check_adc_min);
	if (ret)
		pr_info("%s : Check adc min is Empty\n", __func__);

	ret = of_property_read_u32(np, "battery,ovp_uvlo_check_type",
		&pdata->ovp_uvlo_check_type);
	if (ret)
		pr_info("%s : Ovp Uvlo check type is Empty\n", __func__);

	ret = of_property_read_u32(np, "battery,overheatlimit_threshold", &temp);
	battery->overheatlimit_threshold = (int)temp;
	if (ret) {
		pr_info("%s : overheatlimit_threshold is Empty\n", __func__);
		battery->overheatlimit_threshold = 700;
	}

	ret = of_property_read_u32(np, "battery,overheatlimit_recovery", &temp);
	battery->overheatlimit_recovery = (int)temp;
	if (ret) {
		pr_info("%s : overheatlimit_recovery is Empty\n", __func__);
		battery->overheatlimit_recovery = 680;
	}

#if defined(CONFIG_PREVENT_USB_CONN_OVERHEAT)
	ret = of_property_read_u32(np, "battery,usb_protection_temp", &temp);
	battery->usb_protection_temp = (int)temp;
	if (ret) {
		pr_info("%s : usb protection temp value is Empty\n", __func__);
		battery->usb_protection_temp = 610;
	}

	ret = of_property_read_u32(np, "battery,temp_gap_bat_usb", &temp);
	battery->temp_gap_bat_usb = (int)temp;
	if (ret) {
		pr_info("%s : temp gap value is Empty\n", __func__);
		battery->temp_gap_bat_usb = 200;
	}
#endif

	ret = of_property_read_u32(np, "battery,wire_warm_overheat_thresh", &temp);
	pdata->wire_warm_overheat_thresh = (int)temp;
	if (ret) {
		pr_info("%s : wire_warm_overheat_thresh is Empty\n", __func__);
		pdata->wire_warm_overheat_thresh = 500;
	}

	ret = of_property_read_u32(np, "battery,wire_normal_warm_thresh", &temp);
	pdata->wire_normal_warm_thresh = (int)temp;
	if (ret) {
		pr_info("%s : wire_normal_warm_thresh is Empty\n", __func__);
		pdata->wire_normal_warm_thresh = 410;
	}

	ret = of_property_read_u32(np, "battery,wire_cool1_normal_thresh", &temp);
	pdata->wire_cool1_normal_thresh = (int)temp;
	if (ret) {
		pr_info("%s : wire_cool1_normal_thresh is Empty\n", __func__);
		pdata->wire_cool1_normal_thresh = 180;
	}

	ret = of_property_read_u32(np, "battery,wire_cool2_cool1_thresh", &temp);
	pdata->wire_cool2_cool1_thresh = (int)temp;
	if (ret) {
		pr_info("%s : wire_cool2_cool1_thresh is Empty\n", __func__);
		pdata->wire_cool2_cool1_thresh = 150;
	}

	ret = of_property_read_u32(np, "battery,wire_cool3_cool2_thresh", &temp);
	pdata->wire_cool3_cool2_thresh = (int)temp;
	if (ret) {
		pr_info("%s : wire_cool3_cool2_thresh is Empty\n", __func__);
		pdata->wire_cool3_cool2_thresh = 50;
	}

	ret = of_property_read_u32(np, "battery,wire_cold_cool3_thresh", &temp);
	pdata->wire_cold_cool3_thresh = (int)temp;
	if (ret) {
		pr_info("%s : wire_cold_cool3_thresh is Empty\n", __func__);
		pdata->wire_cold_cool3_thresh = 0;
	}

	ret = of_property_read_u32(np, "battery,wireless_warm_overheat_thresh", &temp);
	pdata->wireless_warm_overheat_thresh = (int)temp;
	if (ret) {
		pr_info("%s : wireless_warm_overheat_thresh is Empty\n", __func__);
		pdata->wireless_warm_overheat_thresh = 450;
	}

	ret = of_property_read_u32(np, "battery,wireless_normal_warm_thresh", &temp);
	pdata->wireless_normal_warm_thresh = (int)temp;
	if (ret) {
		pr_info("%s : wireless_normal_warm_thresh is Empty\n", __func__);
		pdata->wireless_normal_warm_thresh = 410;
	}

	ret = of_property_read_u32(np, "battery,wireless_cool1_normal_thresh", &temp);
	pdata->wireless_cool1_normal_thresh = (int)temp;
	if (ret) {
		pr_info("%s : wireless_cool1_normal_thresh is Empty\n", __func__);
		pdata->wireless_cool1_normal_thresh = 180;
	}

	ret = of_property_read_u32(np, "battery,wireless_cool2_cool1_thresh", &temp);
	pdata->wireless_cool2_cool1_thresh = (int)temp;
	if (ret) {
		pr_info("%s : wireless_cool2_cool1_thresh is Empty\n", __func__);
		pdata->wireless_cool2_cool1_thresh = 150;
	}

	ret = of_property_read_u32(np, "battery,wireless_cool3_cool2_thresh", &temp);
	pdata->wireless_cool3_cool2_thresh = (int)temp;
	if (ret) {
		pr_info("%s : wireless_cool3_cool2_thresh is Empty\n", __func__);
		pdata->wireless_cool3_cool2_thresh = 50;
	}

	ret = of_property_read_u32(np, "battery,wireless_cold_cool3_thresh", &temp);
	pdata->wireless_cold_cool3_thresh = (int)temp;
	if (ret) {
		pr_info("%s : wireless_cold_cool3_thresh is Empty\n", __func__);
		pdata->wireless_cold_cool3_thresh = 0;
	}

	ret = of_property_read_u32(np, "battery,wire_warm_current", &temp);
	pdata->wire_warm_current = (int)temp;
	if (ret) {
		pr_info("%s : wire_warm_current is Empty\n", __func__);
		pdata->wire_warm_current = 500;
	}

	ret = of_property_read_u32(np, "battery,wire_cool1_current", &temp);
	pdata->wire_cool1_current = (int)temp;
	if (ret) {
		pr_info("%s : wire_cool1_current is Empty\n", __func__);
		pdata->wire_cool1_current = 500;
	}

	ret = of_property_read_u32(np, "battery,wire_cool2_current", &temp);
	pdata->wire_cool2_current = (int)temp;
	if (ret) {
		pr_info("%s : wire_cool2_current is Empty\n", __func__);
		pdata->wire_cool2_current = 500;
	}

	ret = of_property_read_u32(np, "battery,wire_cool3_current", &temp);
	pdata->wire_cool3_current = (int)temp;
	if (ret) {
		pr_info("%s : wire_cool3_current is Empty\n", __func__);
		pdata->wire_cool3_current = 500;
	}

	ret = of_property_read_u32(np, "battery,wireless_warm_current", &temp);
	pdata->wireless_warm_current = (int)temp;
	if (ret) {
		pr_info("%s : wireless_warm_current is Empty\n", __func__);
		pdata->wireless_warm_current = 500;
	}

	ret = of_property_read_u32(np, "battery,wireless_cool1_current", &temp);
	pdata->wireless_cool1_current = (int)temp;
	if (ret) {
		pr_info("%s : wireless_cool1_current is Empty\n", __func__);
		pdata->wireless_cool1_current = 500;
	}

	ret = of_property_read_u32(np, "battery,wireless_cool2_current", &temp);
	pdata->wireless_cool2_current = (int)temp;
	if (ret) {
		pr_info("%s : wireless_cool2_current is Empty\n", __func__);
		pdata->wireless_cool2_current = 500;
	}

	ret = of_property_read_u32(np, "battery,wireless_cool3_current", &temp);
	pdata->wireless_cool3_current = (int)temp;
	if (ret) {
		pr_info("%s : wireless_cool3_current is Empty\n", __func__);
		pdata->wireless_cool3_current = 500;
	}

#if IS_ENABLED(CONFIG_DUAL_BATTERY)
	ret = of_property_read_u32(np, "battery,limiter_main_warm_current",
					&pdata->limiter_main_warm_current);
	if (ret)
		pr_info("%s: limiter_main_warm_current is Empty\n", __func__);

	ret = of_property_read_u32(np, "battery,limiter_sub_warm_current",
					&pdata->limiter_sub_warm_current);
	if (ret)
		pr_info("%s: limiter_sub_warm_current is Empty\n", __func__);

	ret = of_property_read_u32(np, "battery,limiter_main_cool1_current",
					&pdata->limiter_main_cool1_current);
	if (ret)
		pr_info("%s: limiter_main_cool1_current is Empty\n", __func__);

	ret = of_property_read_u32(np, "battery,limiter_sub_cool1_current",
					&pdata->limiter_sub_cool1_current);
	if (ret)
		pr_info("%s: limiter_sub_cool1_current is Empty\n", __func__);

	ret = of_property_read_u32(np, "battery,limiter_main_cool2_current",
					&pdata->limiter_main_cool2_current);
	if (ret)
		pr_info("%s: limiter_main_cool2_current is Empty\n", __func__);

	ret = of_property_read_u32(np, "battery,limiter_sub_cool2_current",
					&pdata->limiter_sub_cool2_current);
	if (ret)
		pr_info("%s: limiter_sub_cool2_current is Empty\n", __func__);

	ret = of_property_read_u32(np, "battery,limiter_main_cool3_current",
					&pdata->limiter_main_cool3_current);
	if (ret)
		pr_info("%s: limiter_main_cool3_current is Empty\n", __func__);

	ret = of_property_read_u32(np, "battery,limiter_sub_cool3_current",
					&pdata->limiter_sub_cool3_current);
	if (ret)
		pr_info("%s: limiter_sub_cool3_current is Empty\n", __func__);
#endif

	ret = of_property_read_u32(np, "battery,high_temp_float", &temp);
	pdata->high_temp_float = (int)temp;
	if (ret) {
		pr_info("%s : high_temp_float is Empty\n", __func__);
		pdata->high_temp_float = 4150;
	}

	ret = of_property_read_u32(np, "battery,low_temp_float", &temp);
	pdata->low_temp_float = (int)temp;
	if (ret) {
		pr_info("%s : low_temp_float is Empty\n", __func__);
		pdata->low_temp_float = 4350;
	}

	ret = of_property_read_u32(np, "battery,buck_recovery_margin", &temp);
	pdata->buck_recovery_margin = (int)temp;
	if (ret) {
		pr_info("%s : buck_recovery_margin is Empty\n", __func__);
		pdata->buck_recovery_margin = 50;
	}

	ret = of_property_read_u32(np, "battery,recharge_condition_vcell", &pdata->recharge_condition_vcell);
	if (ret) {
		pr_info("%s : recharge_condition_vcell is Empty\n", __func__);
		pdata->recharge_condition_vcell = 4270;
	}

	ret = of_property_read_u32(np, "battery,swelling_high_rechg_voltage", &temp);
	pdata->swelling_high_rechg_voltage = (int)temp;
	if (ret) {
		pr_info("%s : swelling_high_rechg_voltage is Empty\n", __func__);
		pdata->swelling_high_rechg_voltage = 4000;
	}

	ret = of_property_read_u32(np, "battery,swelling_low_rechg_voltage", &temp);
	pdata->swelling_low_rechg_voltage = (int)temp;
	if (ret) {
		pr_info("%s : swelling_low_rechg_voltage is Empty\n", __func__);
		pdata->swelling_low_rechg_voltage = 4200;
	}

	ret = of_property_read_u32(np, "battery,tx_high_threshold",
				   &temp);
	pdata->tx_high_threshold = (int)temp;
	if (ret) {
		pr_info("%s : tx_high_threshold is Empty\n", __func__);
		pdata->tx_high_threshold = 450;
	}

	ret = of_property_read_u32(np, "battery,tx_high_recovery",
				   &temp);
	pdata->tx_high_recovery = (int)temp;
	if (ret) {
		pr_info("%s : tx_high_recovery is Empty\n", __func__);
		pdata->tx_high_recovery = 400;
	}

	ret = of_property_read_u32(np, "battery,tx_low_threshold",
				   &temp);
	pdata->tx_low_threshold = (int)temp;
	if (ret) {
		pr_info("%s : tx_low_threshold is Empty\n", __func__);
		pdata->tx_low_recovery = 0;
	}
	ret = of_property_read_u32(np, "battery,tx_low_recovery",
				   &temp);
	pdata->tx_low_recovery = (int)temp;
	if (ret) {
		pr_info("%s : tx_low_recovery is Empty\n", __func__);
		pdata->tx_low_recovery = 50;
	}

	ret = of_property_read_u32(np, "battery,tx_gear_vout",
				   &temp);
	pdata->tx_gear_vout = (int)temp;
	if (ret) {
		pr_info("%s : tx_gear_vout is Empty\n", __func__);
		pdata->tx_gear_vout = WC_TX_VOUT_5000MV;
	}

	ret = of_property_read_u32(np, "battery,charging_limit_by_tx_check",
				   &pdata->charging_limit_by_tx_check);
	if (ret)
		pr_info("%s : charging_limit_by_tx_check is Empty\n", __func__);

	if (pdata->charging_limit_by_tx_check) {
		ret = of_property_read_u32(np, "battery,charging_limit_current_by_tx",
					   &pdata->charging_limit_current_by_tx);
		if (ret) {
			pr_info("%s : charging_limit_current_by_tx is Empty\n", __func__);
			pdata->charging_limit_current_by_tx = 1000;
		}

		ret = of_property_read_u32(np, "battery,charging_limit_current_by_tx_gear",
					   &pdata->charging_limit_current_by_tx_gear);
		if (ret) {
			pr_info("%s : charging_limit_current_by_tx_gear is Empty\n", __func__);
			pdata->charging_limit_current_by_tx_gear = pdata->charging_limit_current_by_tx;
		}
	}

	ret = of_property_read_u32(np, "battery,wpc_input_limit_by_tx_check",
				   &pdata->wpc_input_limit_by_tx_check);
	if (ret)
		pr_info("%s : wpc_input_limit_by_tx_check is Empty\n", __func__);

	if (pdata->wpc_input_limit_by_tx_check) {
		ret = of_property_read_u32(np, "battery,wpc_input_limit_current_by_tx",
				&pdata->wpc_input_limit_current_by_tx);
		if (ret) {
			pr_info("%s : wpc_input_limit_current_by_tx is Empty\n", __func__);
			pdata->wpc_input_limit_current_by_tx =
				pdata->wpc_input_limit_current;
		}
	}

	ret = of_property_read_u32(np, "battery,non_wc20_wpc_high_temp",
		&pdata->non_wc20_wpc_high_temp);
	if (ret) {
		pr_info("%s : non_wc20_wpc_high_temp is Empty\n", __func__);
		pdata->non_wc20_wpc_high_temp = pdata->wpc_high_temp;
	}
	ret = of_property_read_u32(np, "battery,non_wc20_wpc_high_temp_recovery",
		&pdata->non_wc20_wpc_high_temp_recovery);
	if (ret) {
		pr_info("%s : non_wc20_wpc_high_temp_recovery is Empty\n", __func__);
		pdata->non_wc20_wpc_high_temp_recovery = pdata->wpc_high_temp_recovery;
	}

	ret = of_property_read_u32(np, "battery,full_check_type",
		&pdata->full_check_type);
	if (ret)
		pr_info("%s : Full check type is Empty\n", __func__);

	ret = of_property_read_u32(np, "battery,full_check_type_2nd",
		&pdata->full_check_type_2nd);
	if (ret)
		pr_info("%s : Full check type 2nd is Empty\n", __func__);

	ret = of_property_read_u32(np, "battery,full_check_count",
		&pdata->full_check_count);
	if (ret)
		pr_info("%s : Full check count is Empty\n", __func__);

        ret = of_property_read_u32(np, "battery,chg_gpio_full_check",
                &pdata->chg_gpio_full_check);
	if (ret)
		pr_info("%s : Chg gpio full check is Empty\n", __func__);

	ret = of_property_read_u32(np, "battery,chg_polarity_full_check",
		&pdata->chg_polarity_full_check);
	if (ret)
		pr_info("%s : Chg polarity full check is Empty\n", __func__);

	ret = of_property_read_u32(np, "battery,full_condition_type",
		&pdata->full_condition_type);
	if (ret)
		pr_info("%s : Full condition type is Empty\n", __func__);

	ret = of_property_read_u32(np, "battery,full_condition_soc",
		&pdata->full_condition_soc);
	if (ret)
		pr_info("%s : Full condition soc is Empty\n", __func__);

	ret = of_property_read_u32(np, "battery,full_condition_vcell",
		&pdata->full_condition_vcell);
	if (ret)
		pr_info("%s : Full condition vcell is Empty\n", __func__);

	ret = of_property_read_u32(np, "battery,recharge_check_count",
		&pdata->recharge_check_count);
	if (ret)
		pr_info("%s : Recharge check count is Empty\n", __func__);

	ret = of_property_read_u32(np, "battery,recharge_condition_type",
		&pdata->recharge_condition_type);
	if (ret)
		pr_info("%s : Recharge condition type is Empty\n", __func__);

	ret = of_property_read_u32(np, "battery,recharge_condition_soc",
		&pdata->recharge_condition_soc);
	if (ret)
		pr_info("%s : Recharge condition soc is Empty\n", __func__);

	ret = of_property_read_u32(np, "battery,charging_total_time",
		(unsigned int *)&pdata->charging_total_time);
	if (ret)
		pr_info("%s : Charging total time is Empty\n", __func__);

	ret = of_property_read_u32(np, "battery,hv_charging_total_time",
				   &pdata->hv_charging_total_time);
	if (ret) {
		pdata->hv_charging_total_time = 3 * 60 * 60;
		pr_info("%s : HV Charging total time is %d\n",
			__func__, pdata->hv_charging_total_time);
	}

	ret = of_property_read_u32(np, "battery,normal_charging_total_time",
				   &pdata->normal_charging_total_time);
	if (ret) {
		pdata->normal_charging_total_time = 5 * 60 * 60;
		pr_info("%s : Normal(WC) Charging total time is %d\n",
			__func__, pdata->normal_charging_total_time);
	}

	ret = of_property_read_u32(np, "battery,usb_charging_total_time",
				   &pdata->usb_charging_total_time);
	if (ret) {
		pdata->usb_charging_total_time = 10 * 60 * 60;
		pr_info("%s : USB Charging total time is %d\n",
			__func__, pdata->usb_charging_total_time);
	}

	ret = of_property_read_u32(np, "battery,recharging_total_time",
		(unsigned int *)&pdata->recharging_total_time);
	if (ret)
		pr_info("%s : Recharging total time is Empty\n", __func__);

	ret = of_property_read_u32(np, "battery,charging_reset_time",
		(unsigned int *)&pdata->charging_reset_time);
	if (ret)
		pr_info("%s : Charging reset time is Empty\n", __func__);

	ret = of_property_read_u32(np, "battery,chg_float_voltage",
		(unsigned int *)&pdata->chg_float_voltage);
	if (ret) {
		pr_info("%s: chg_float_voltage is Empty\n", __func__);
		pdata->chg_float_voltage = 43500;
	}

	ret = of_property_read_u32(np, "battery,chg_float_voltage_conv",
				   &pdata->chg_float_voltage_conv);
	if (ret) {
		pr_info("%s: chg_float_voltage_conv is Empty\n", __func__);
		pdata->chg_float_voltage_conv = 1;
	}

#if defined(CONFIG_BATTERY_AGE_FORECAST)
	p = of_get_property(np, "battery,age_data", &len);
	if (p) {
		battery->pdata->num_age_step = len / sizeof(sec_age_data_t);
		battery->pdata->age_data = kzalloc(len, GFP_KERNEL);
		ret = of_property_read_u32_array(np, "battery,age_data",
				 (u32 *)battery->pdata->age_data, len/sizeof(u32));
		if (ret) {
			pr_err("%s failed to read battery->pdata->age_data: %d\n",
					__func__, ret);
			kfree(battery->pdata->age_data);
			battery->pdata->age_data = NULL;
			battery->pdata->num_age_step = 0;
		}
		pr_err("%s num_age_step : %d\n", __func__, battery->pdata->num_age_step);
		for (len = 0; len < battery->pdata->num_age_step; ++len) {
			pr_err("[%d/%d]cycle:%d, float:%d, full_v:%d, recharge_v:%d, soc:%d\n",
				len, battery->pdata->num_age_step-1,
				battery->pdata->age_data[len].cycle,
				battery->pdata->age_data[len].float_voltage,
				battery->pdata->age_data[len].full_condition_vcell,
				battery->pdata->age_data[len].recharge_condition_vcell,
				battery->pdata->age_data[len].full_condition_soc);
		}
	} else {
		battery->pdata->num_age_step = 0;
		pr_err("%s there is not age_data\n", __func__);
	}
#endif
	p = of_get_property(np, "battery,health_condition", &len);
	if (p || (len / sizeof(battery_health_condition)) == BATTERY_HEALTH_MAX) {
		battery->pdata->health_condition = kzalloc(len, GFP_KERNEL);
		ret = of_property_read_u32_array(np, "battery,health_condition",
				(u32 *)battery->pdata->health_condition, len/sizeof(u32));
		if (ret) {
			pr_err("%s failed to read battery->pdata->health_condition: %d\n",
					__func__, ret);
			kfree(battery->pdata->health_condition);
			battery->pdata->health_condition = NULL;
		} else {
			for (i = 0; i < BATTERY_HEALTH_MAX; i++) {
				pr_err("%s: [BATTERY_HEALTH] %d: Cycle(~ %d), ASoC(~ %d)\n",
						__func__, i,
						battery->pdata->health_condition[i].cycle,
						battery->pdata->health_condition[i].asoc);
			}
		}
	} else {
		battery->pdata->health_condition = NULL;
		pr_err("%s there is not health_condition, len(%d)\n", __func__, len);
	}

	sec_bat_parse_dt_siop(battery, np);

	ret = of_property_read_u32(np, "battery,wireless_otg_input_current",
			&pdata->wireless_otg_input_current);
	if (ret)
		pdata->wireless_otg_input_current = WIRELESS_OTG_INPUT_CURRENT;

	ret = of_property_read_u32(np, "battery,max_input_voltage",
			&pdata->max_input_voltage);
	if (ret)
		pdata->max_input_voltage = 12000;

	ret = of_property_read_u32(np, "battery,max_input_current",
			&pdata->max_input_current);
	if (ret)
		pdata->max_input_current = 3000;

	ret = of_property_read_u32(np, "battery,pd_charging_charge_power",
			&pdata->pd_charging_charge_power);
	if (ret) {
		pr_err("%s: pd_charging_charge_power is Empty\n", __func__);
		pdata->pd_charging_charge_power = 15000;
	}

	ret = of_property_read_u32(np, "battery,rp_current_rp1",
			&pdata->rp_current_rp1);
	if (ret) {
		pr_err("%s: rp_current_rp1 is Empty\n", __func__);
		pdata->rp_current_rp1 = 500;
	}

	ret = of_property_read_u32(np, "battery,rp_current_rp2",
			&pdata->rp_current_rp2);
	if (ret) {
		pr_err("%s: rp_current_rp2 is Empty\n", __func__);
		pdata->rp_current_rp2 = 1500;
	}

	ret = of_property_read_u32(np, "battery,rp_current_rp3",
			&pdata->rp_current_rp3);
	if (ret) {
		pr_err("%s: rp_current_rp3 is Empty\n", __func__);
		pdata->rp_current_rp3 = 3000;
	}

	ret = of_property_read_u32(np, "battery,rp_current_rdu_rp3",
			&pdata->rp_current_rdu_rp3);
	if (ret) {
		pr_err("%s: rp_current_rdu_rp3 is Empty\n", __func__);
		pdata->rp_current_rdu_rp3 = 2100;
	}

	ret = of_property_read_u32(np, "battery,rp_current_abnormal_rp3",
			&pdata->rp_current_abnormal_rp3);
	if (ret) {
		pr_err("%s: rp_current_abnormal_rp3 is Empty\n", __func__);
		pdata->rp_current_rdu_rp3 = 1800;
	}

	ret = of_property_read_u32(np, "battery,nv_charge_power",
			&pdata->nv_charge_power);
	if (ret) {
		pr_err("%s: nv_charge_power is Empty\n", __func__);
		pdata->nv_charge_power = mW_by_mVmA(SEC_INPUT_VOLTAGE_5V, pdata->default_input_current);
	}

	ret = of_property_read_u32(np, "battery,tx_minduty_default",
			&pdata->tx_minduty_default);
	if (ret) {
		pdata->tx_minduty_default = 20;
		pr_err("%s: tx minduty is Empty. set %d\n", __func__, pdata->tx_minduty_default);
	}

	ret = of_property_read_u32(np, "battery,tx_minduty_5V",
			&pdata->tx_minduty_5V);
	if (ret) {
		pdata->tx_minduty_5V = 50;
		pr_err("%s: tx minduty 5V is Empty. set %d\n", __func__, pdata->tx_minduty_5V);
	}

	ret = of_property_read_u32(np, "battery,tx_uno_vout",
			&pdata->tx_uno_vout);
	if (ret) {
		pdata->tx_uno_vout = WC_TX_VOUT_7500MV;
		pr_err("%s: tx uno vout is Empty. set %d\n", __func__, pdata->tx_uno_vout);
	}

	ret = of_property_read_u32(np, "battery,tx_uno_iout",
			&pdata->tx_uno_iout);
	if (ret) {
		pdata->tx_uno_iout = 1500;
		pr_err("%s: tx uno iout is Empty. set %d\n", __func__, pdata->tx_uno_iout);
	}

	ret = of_property_read_u32(np, "battery,tx_mfc_iout_gear",
			&pdata->tx_mfc_iout_gear);
	if (ret) {
		pdata->tx_mfc_iout_gear = 1500;
		pr_err("%s: tx mfc iout gear is Empty. set %d\n", __func__, pdata->tx_mfc_iout_gear);
	}

	ret = of_property_read_u32(np, "battery,tx_mfc_iout_phone",
			&pdata->tx_mfc_iout_phone);
	if (ret) {
		pdata->tx_mfc_iout_phone = 1100;
		pr_err("%s: tx mfc iout phone is Empty. set %d\n", __func__, pdata->tx_mfc_iout_phone);
	}

	ret = of_property_read_u32(np, "battery,tx_mfc_iout_phone_5v",
			&pdata->tx_mfc_iout_phone_5v);
	if (ret) {
		pdata->tx_mfc_iout_phone_5v = 300;
		pr_err("%s: tx mfc iout phone 5v is Empty. set %d\n", __func__, pdata->tx_mfc_iout_phone_5v);
	}

	ret = of_property_read_u32(np, "battery,tx_mfc_iout_lcd_on",
			&pdata->tx_mfc_iout_lcd_on);
	if (ret) {
		pdata->tx_mfc_iout_lcd_on = 900;
		pr_err("%s: tx mfc iout lcd on is Empty. set %d\n", __func__, pdata->tx_mfc_iout_lcd_on);
	}

	pdata->tx_5v_disable = of_property_read_bool(np, "battery,tx_5v_disable");
	pr_info("%s: 5V TA power sharing is %s.\n", __func__,
		pdata->tx_5v_disable ? "Disabled" : "Enabled");

	ret = of_property_read_u32(np, "battery,phm_vout_ctrl_dev",
					&pdata->phm_vout_ctrl_dev);
	if (ret < 0) {
		pr_info("%s: fail to read phm_vout_ctrl_dev\n", __func__);
		pdata->phm_vout_ctrl_dev = 0;
	}
	pr_info("%s: phm_vout_ctrl_dev = %d\n", __func__, pdata->phm_vout_ctrl_dev);

	ret = of_property_read_u32(np, "battery,tx_aov_start_vout",
			&pdata->tx_aov_start_vout);
	if (ret) {
		pdata->tx_aov_start_vout = WC_TX_VOUT_6000MV;
		pr_err("%s: tx aov start vout is Empty. set %d\n", __func__, pdata->tx_aov_start_vout);
	}

	ret = of_property_read_u32(np, "battery,tx_aov_freq_low",
			&pdata->tx_aov_freq_low);
	if (ret) {
		pdata->tx_aov_freq_low = 125;
		pr_err("%s: tx aov freq low is Empty. set %d\n", __func__, pdata->tx_aov_freq_low);
	}

	ret = of_property_read_u32(np, "battery,tx_aov_freq_high",
			&pdata->tx_aov_freq_high);
	if (ret) {
		pdata->tx_aov_freq_high = 147;
		pr_err("%s: tx aov freq high is Empty. set %d\n", __func__, pdata->tx_aov_freq_high);
	}

	ret = of_property_read_u32(np, "battery,tx_aov_delay",
			&pdata->tx_aov_delay);
	if (ret) {
		pdata->tx_aov_delay = 3000;
		pr_err("%s: tx aov dealy is Empty. set %d\n", __func__, pdata->tx_aov_delay);
	}

	ret = of_property_read_u32(np, "battery,tx_aov_delay_phm_escape",
			&pdata->tx_aov_delay_phm_escape);
	if (ret) {
		pdata->tx_aov_delay_phm_escape = 4000;
		pr_err("%s: tx aov dealy phm escape is Empty. set %d\n", __func__, pdata->tx_aov_delay_phm_escape);
	}

	pdata->lr_sub_enable = of_property_read_bool(np, "battery,lr_sub_enable");
	if (pdata->lr_sub_enable) {
		ret = of_property_read_u32(np, "battery,lr_param_bat_thm",
				&pdata->lr_param_bat_thm);
		if (ret)
			pdata->lr_param_bat_thm = 420;

		ret = of_property_read_u32(np, "battery,lr_param_sub_bat_thm",
				&pdata->lr_param_sub_bat_thm);
		if (ret)
			pdata->lr_param_sub_bat_thm = 580;

		ret = of_property_read_u32(np, "battery,lr_delta",
				&pdata->lr_delta);
		if (ret)
			pdata->lr_delta = 16;

		ret = of_property_read_u32(np, "battery,lr_param_init_bat_thm",
				&pdata->lr_param_init_bat_thm);
		if (ret)
			pdata->lr_param_init_bat_thm = 70;

		ret = of_property_read_u32(np, "battery,lr_param_init_sub_bat_thm",
				&pdata->lr_param_init_sub_bat_thm);
		if (ret)
			pdata->lr_param_init_sub_bat_thm = 30;

		ret = of_property_read_u32(np, "battery,lr_round_off",
				&pdata->lr_round_off);
		if (ret)
			pdata->lr_round_off = 500;
	}

	pdata->cs100_jpn = of_property_read_bool(np, "battery,cs100_jpn");

	pr_info("%s: vendor : %s, technology : %d, cable_check_type : %d\n"
		"cable_source_type : %d, polling_type: %d\n"
		"initial_count : %d, check_count : %d\n"
		"check_adc_max : %d, check_adc_min : %d\n"
		"ovp_uvlo_check_type : %d, thermal_source : %d\n"
		"temp_check_type : %d, temp_check_count : %d, nv_charge_power : %d\n"
		"full_condition_type : %d, recharge_condition_type : %d, full_check_type : %d\n",
		__func__,
		pdata->vendor, pdata->technology,pdata->cable_check_type,
		pdata->cable_source_type, pdata->polling_type,
		pdata->monitor_initial_count, pdata->check_count,
		pdata->check_adc_max, pdata->check_adc_min,
		pdata->ovp_uvlo_check_type, pdata->thermal_source,
		pdata->temp_check_type, pdata->temp_check_count, pdata->nv_charge_power,
		pdata->full_condition_type, pdata->recharge_condition_type, pdata->full_check_type
		);

#if defined(CONFIG_STEP_CHARGING)
	sec_step_charging_init(battery, dev);
#endif
	ret = of_property_read_u32(np, "battery,max_charging_current",
			&pdata->max_charging_current);
	if (ret) {
		pr_err("%s: max_charging_current is Empty\n", __func__);
		pdata->max_charging_current = 3000;
	}

	ret = of_property_read_u32(np, "battery,max_charging_charge_power",
			&pdata->max_charging_charge_power);
	if (ret) {
		pr_err("%s: max_charging_charge_power is Empty\n", __func__);
		pdata->max_charging_charge_power = 25000;
	}

	ret = of_property_read_u32(np, "battery,apdo_max_volt",
			&pdata->apdo_max_volt);
	if (ret) {
		pr_err("%s: apdo_max_volt is Empty\n", __func__);
		pdata->apdo_max_volt = 11000; /* 11v */
	}

#if IS_ENABLED(CONFIG_DUAL_BATTERY)
#if defined(CONFIG_DUAL_BATTERY_CELL_SENSING)
	ret = of_property_read_u32(np, "battery,main_cell_margin_cv",
			&pdata->main_cell_margin_cv);
	if (ret) {
		pr_err("%s: main_cell_margin_cv is Empty\n", __func__);
		pdata->main_cell_margin_cv = 50;
	}

	ret = of_property_read_u32(np, "battery,main_cell_sensing",
			&pdata->main_cell_sensing);
	if (ret) {
		pr_err("%s: main_cell_sensing is Empty\n", __func__);
		pdata->main_cell_sensing = 0; // need to change 1 for merge
	}

	ret = of_property_read_u32(np, "battery,sub_cell_sensing",
			&pdata->sub_cell_sensing);
	if (ret) {
		pr_err("%s: sub_cell_sensing is Empty\n", __func__);
		pdata->sub_cell_sensing = 1;
	}
#endif

	ret = of_property_read_string(np,
		"battery,dual_battery_name", (char const **)&pdata->dual_battery_name);
	if (ret)
		pr_info("%s: Dual battery name is Empty\n", __func__);

	np = of_find_node_by_name(NULL, "sec-dual-battery");
	if (!np) {
		pr_info("%s: np NULL\n", __func__);
	} else {
		ret = of_property_read_u32(np, "battery,main_current_rate",
				&pdata->main_current_rate);
		if (ret) {
			pr_err("%s: main_current_rate is Empty\n", __func__);
			pdata->main_current_rate = 60;
		}
		ret = of_property_read_u32(np, "battery,sub_current_rate",
				&pdata->sub_current_rate);
		if (ret) {
			pr_err("%s: sub_current_rate is Empty\n", __func__);
			pdata->sub_current_rate = 50;
		}
		/* zone1 current ratio, 0C ~ 0.4C */
		ret = of_property_read_u32(np, "battery,main_zone1_current_rate",
				&pdata->main_zone1_current_rate);
		if (ret) {
			pr_err("%s: main_zone1_current_rate is Empty\n", __func__);
			pdata->main_zone1_current_rate = 50;
		}
		ret = of_property_read_u32(np, "battery,sub_zone1_current_rate",
				&pdata->sub_zone1_current_rate);
		if (ret) {
			pr_err("%s: sub_zone1_current_rate is Empty\n", __func__);
			pdata->sub_zone1_current_rate = 60;
		}
		/* zone2 current ratio, 0.4C ~ 1.1C */
		ret = of_property_read_u32(np, "battery,main_zone2_current_rate",
				&pdata->main_zone2_current_rate);
		if (ret) {
			pr_err("%s: main_zone2_current_rate is Empty\n", __func__);
			pdata->main_zone2_current_rate = 50;
		}
		ret = of_property_read_u32(np, "battery,sub_zone2_current_rate",
				&pdata->sub_zone2_current_rate);
		if (ret) {
			pr_err("%s: sub_zone2_current_rate is Empty\n", __func__);
			pdata->sub_zone2_current_rate = 60;
		}
		/* zone3 current ratio, 1.1C ~ MAX */
		ret = of_property_read_u32(np, "battery,main_zone3_current_rate",
				&pdata->main_zone3_current_rate);
		if (ret) {
			pr_err("%s: main_zone3_current_rate is Empty\n", __func__);
			pdata->main_zone3_current_rate = pdata->main_zone2_current_rate;
		}
		ret = of_property_read_u32(np, "battery,sub_zone3_current_rate",
				&pdata->sub_zone3_current_rate);
		if (ret) {
			pr_err("%s: sub_zone3_current_rate is Empty\n", __func__);
			pdata->sub_zone3_current_rate = pdata->sub_zone2_current_rate;
		}
		ret = of_property_read_u32(np, "battery,force_recharge_margin",
				&pdata->force_recharge_margin);
		if (ret) {
			pr_err("%s: force_recharge_margin is Empty\n", __func__);
			pdata->force_recharge_margin = 150;
		}
		ret = of_property_read_u32(np, "battery,max_main_limiter_current",
				&pdata->max_main_limiter_current);
		if (ret) {
			pr_err("%s: max_main_limiter_current is Empty\n", __func__);
			pdata->max_main_limiter_current = 1550;
		}
		ret = of_property_read_u32(np, "battery,min_main_limiter_current",
				&pdata->min_main_limiter_current);
		if (ret) {
			pr_err("%s: min_main_limiter_current is Empty\n", __func__);
			pdata->min_main_limiter_current = 450;
		}
		ret = of_property_read_u32(np, "battery,max_sub_limiter_current",
				&pdata->max_sub_limiter_current);
		if (ret) {
			pr_err("%s: max_sub_limiter_current is Empty\n", __func__);
			pdata->max_sub_limiter_current = 1300;
		}
		ret = of_property_read_u32(np, "battery,min_sub_limiter_current",
				&pdata->min_sub_limiter_current);
		if (ret) {
			pr_err("%s: min_sub_limiter_current is Empty\n", __func__);
			pdata->min_sub_limiter_current = 450;
		}
		pr_info("%s : main ratio:%d(zn1) %d(zn2) %d(zn3), sub ratio:%d(zn1) %d(zn2) %d(zn3), recharge marging:%d, "
				"max main curr:%d, min main curr:%d, max sub curr:%d, min sub curr:%d \n",
				__func__, pdata->main_zone1_current_rate, pdata->main_zone2_current_rate, pdata->main_zone3_current_rate,
				pdata->sub_zone1_current_rate, pdata->sub_zone2_current_rate, pdata->sub_zone3_current_rate,
				pdata->force_recharge_margin, pdata->max_main_limiter_current, pdata->min_main_limiter_current,
				pdata->max_sub_limiter_current, pdata->min_sub_limiter_current);

		ret = of_property_read_string(np, "battery,main_current_limiter",
				(char const **)&battery->pdata->main_limiter_name);
		if (ret)
			pr_err("%s: main_current_limiter is Empty\n", __func__);
		else {
			np = of_find_node_by_name(NULL, battery->pdata->main_limiter_name);
			if (!np) {
				pr_info("%s: main_limiter_name is Empty\n", __func__);
			} else {
				/* MAIN_BATTERY_SW_EN */
				ret = pdata->main_bat_enb_gpio = of_get_named_gpio(np, "limiter,main_bat_enb_gpio", 0);
				if (ret < 0)
					pr_info("%s : can't get main_bat_enb_gpio\n", __func__);

				/* MAIN_BATTERY_SW_EN2 */
				ret = pdata->main_bat_enb2_gpio = of_get_named_gpio(np, "limiter,main_bat_enb2_gpio", 0);
				if (ret < 0)
					pr_info("%s : can't get main_bat_enb2_gpio\n", __func__);
			}
		}
		np = of_find_node_by_name(NULL, "sec-dual-battery");

		ret = of_property_read_string(np, "battery,sub_current_limiter",
				(char const **)&battery->pdata->sub_limiter_name);
		if (ret)
			pr_err("%s: sub_current_limiter is Empty\n", __func__);
		else {
			np = of_find_node_by_name(NULL, battery->pdata->sub_limiter_name);
			if (!np) {
				pr_info("%s: sub_limiter_name is Empty\n", __func__);
			} else {
				/* SUB_BATTERY_SW_EN */
				ret = pdata->sub_bat_enb_gpio = of_get_named_gpio(np, "limiter,sub_bat_enb_gpio", 0);
				if (ret < 0)
					pr_info("%s : can't get sub_bat_enb_gpio\n", __func__);
			}
		}
	}
	np = of_find_node_by_name(NULL, "battery");
#endif

#if defined(CONFIG_BATTERY_CISD)
	p = of_get_property(np, "battery,ignore_cisd_index", &len);
	pdata->ignore_cisd_index = kzalloc(sizeof(*pdata->ignore_cisd_index) * 2, GFP_KERNEL);
	if (p) {
		len = len / sizeof(u32);
		ret = of_property_read_u32_array(np, "battery,ignore_cisd_index",
	                     pdata->ignore_cisd_index, len);
		if (ret)
			pr_err("%s failed to read ignore_cisd_index: %d\n",
					__func__, ret);
	} else {
		pr_info("%s : battery,ignore_cisd_index is Empty\n", __func__);
	}

	p = of_get_property(np, "battery,ignore_cisd_index_d", &len);
	pdata->ignore_cisd_index_d = kzalloc(sizeof(*pdata->ignore_cisd_index_d) * 2, GFP_KERNEL);
	if (p) {
		len = len / sizeof(u32);
		ret = of_property_read_u32_array(np, "battery,ignore_cisd_index_d",
	                     pdata->ignore_cisd_index_d, len);
		if (ret)
			pr_err("%s failed to read ignore_cisd_index_d: %d\n",
					__func__, ret);
	} else {
		pr_info("%s : battery,ignore_cisd_index_d is Empty\n", __func__);
	}
#endif

	return 0;
}
EXPORT_SYMBOL(sec_bat_parse_dt);

void sec_bat_parse_mode_dt(struct sec_battery_info *battery)
{
	struct device_node *np;
	sec_battery_platform_data_t *pdata = battery->pdata;
	int ret = 0;
	u32 temp = 0;

	np = of_find_node_by_name(NULL, "battery");
	if (!np) {
		pr_err("%s np NULL\n", __func__);
		return;
	}

	if (battery->store_mode) {
		ret = of_property_read_u32(np, "battery,store_mode_max_input_power",
			&pdata->store_mode_max_input_power);
		if (ret) {
			pr_info("%s : store_mode_max_input_power is Empty\n", __func__);
			pdata->store_mode_max_input_power = 4000;
		}

		if (pdata->wpc_temp_check_type) {
			ret = of_property_read_u32(np, "battery,wpc_store_high_temp",
			   &temp);
			if (!ret)
				pdata->wpc_high_temp = temp;

			ret = of_property_read_u32(np, "battery,wpc_store_high_temp_recovery",
			   &temp);
			if (!ret)
				pdata->wpc_high_temp_recovery = temp;

			ret = of_property_read_u32(np, "battery,wpc_store_charging_limit_current",
			   &temp);
			if (!ret)
				pdata->wpc_input_limit_current = temp;

			ret = of_property_read_u32(np, "battery,wpc_store_lcd_on_high_temp",
			   &temp);
			if (!ret)
				pdata->wpc_lcd_on_high_temp = (int)temp;

			ret = of_property_read_u32(np, "battery,wpc_store_lcd_on_high_temp_rec",
			   &temp);
			if (!ret)
				pdata->wpc_lcd_on_high_temp_rec = (int)temp;

			ret = of_property_read_u32(np, "battery,wpc_store_lcd_on_charging_limit_current",
				&temp);
			if (!ret)
				pdata->wpc_lcd_on_input_limit_current = (int)temp;

			pr_info("%s: update store_mode - wpc high_temp(t:%d, r:%d), lcd_on_high_temp(t:%d, r:%d), curr(%d, %d)\n",
				__func__,
				pdata->wpc_high_temp, pdata->wpc_high_temp_recovery,
				pdata->wpc_lcd_on_high_temp, pdata->wpc_lcd_on_high_temp_rec,
				pdata->wpc_input_limit_current,
				pdata->wpc_lcd_on_input_limit_current);
		}

		ret = of_property_read_u32(np, "battery,siop_store_hv_wpc_icl",
			&temp);
		if (!ret)
			pdata->siop_hv_wpc_icl = temp;
		else
			pdata->siop_hv_wpc_icl = SIOP_STORE_HV_WIRELESS_CHARGING_LIMIT_CURRENT;
		pr_info("%s: update siop_hv_wpc_icl(%d)\n",
			__func__, pdata->siop_hv_wpc_icl);
		pdata->store_mode_buckoff = of_property_read_bool(np, "battery,store_mode_buckoff");
		pr_info("%s : battery,store_mode_buckoff: %d\n", __func__, pdata->store_mode_buckoff);
	}
}
EXPORT_SYMBOL_KUNIT(sec_bat_parse_mode_dt);

void sec_bat_parse_mode_dt_work(struct work_struct *work)
{
	struct sec_battery_info *battery = container_of(work,
		struct sec_battery_info, parse_mode_dt_work.work);

	sec_bat_parse_mode_dt(battery);

	if (is_hv_wire_type(battery->cable_type) ||
		is_hv_wireless_type(battery->cable_type)) {
		sec_bat_set_charging_current(battery);
	}

	__pm_relax(battery->parse_mode_dt_ws);
}
EXPORT_SYMBOL(sec_bat_parse_mode_dt_work);
#endif
