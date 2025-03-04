/*
 *  max77705_charger.c
 *  Samsung max77705 Charger Driver
 *
 *  Copyright (C) 2012 Samsung Electronics
 *
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */
#define DEBUG

#include <linux/mfd/max77705-private.h>
#include <linux/debugfs.h>
#include <linux/seq_file.h>
#include <linux/power_supply.h>
#include <linux/mfd/max77705.h>
#include <linux/of_gpio.h>
#include "max77705_charger.h"
#include <linux/muic/common/muic.h>
#ifdef CONFIG_USB_HOST_NOTIFY
#include <linux/usb_notify.h>
#endif
#include <linux/sec_debug.h>

#if defined(CONFIG_SEC_KUNIT)
#define __visible_for_testing
#else
#define __visible_for_testing static
#endif

#define ENABLE 1
#define DISABLE 0

#if defined(CONFIG_SEC_FACTORY)
#define WC_CURRENT_WORK_STEP	250
#else
#define WC_CURRENT_WORK_STEP	1000
#endif
#define AICL_WORK_DELAY		100

#if IS_ENABLED(CONFIG_SEC_MPARAM) || (IS_MODULE(CONFIG_SEC_PARAM) && defined(CONFIG_ARCH_EXYNOS))
extern unsigned int lpcharge;
#if defined(CONFIG_SEC_FACTORY)
extern int factory_mode;
#endif
#else
static unsigned int __read_mostly lpcharge;
module_param(lpcharge, uint, 0444);
#if defined(CONFIG_SEC_FACTORY)
static int __read_mostly factory_mode;
module_param(factory_mode, int, 0444);
#endif
#endif

extern void max77705_usbc_icurr(u8 curr);
extern void max77705_set_fw_noautoibus(int enable);
#if defined(CONFIG_SUPPORT_SHIP_MODE)
extern void max77705_set_fw_ship_mode(int enable);
extern int max77705_get_fw_ship_mode(void);
#endif

static enum power_supply_property max77705_charger_props[] = {
	POWER_SUPPLY_PROP_ONLINE,
};

static enum power_supply_property max77705_otg_props[] = {
	POWER_SUPPLY_PROP_ONLINE,
};

static struct device_attribute max77705_charger_attrs[] = {
	MAX77705_CHARGER_ATTR(chip_id),
	MAX77705_CHARGER_ATTR(data),
};

__visible_for_testing void max77705_charger_initialize(struct max77705_charger_data *charger);
__visible_for_testing int max77705_get_vbus_state(struct max77705_charger_data *charger);
__visible_for_testing int max77705_get_charger_state(struct max77705_charger_data *charger);
static void max77705_enable_aicl_irq(struct max77705_charger_data *charger);
__visible_for_testing void max77705_chg_set_mode_state(struct max77705_charger_data *charger,
					unsigned int state);
__visible_for_testing void max77705_set_switching_frequency(struct max77705_charger_data *charger,
					int frequency);

static unsigned int max77705_get_lpmode(void) { return lpcharge; }
#if defined(CONFIG_SEC_FACTORY)
static unsigned int max77705_get_facmode(void) { return factory_mode; }
#endif

static bool max77705_charger_unlock(struct max77705_charger_data *charger)
{
	u8 reg_data, chgprot;
	int retry_cnt = 0;
	bool need_init = false;

	do {
		max77705_read_reg(charger->i2c, MAX77705_CHG_REG_CNFG_06, &reg_data);
		chgprot = ((reg_data & 0x0C) >> 2);
		if (chgprot != 0x03) {
			pr_err("%s: unlock err, chgprot(0x%x), retry(%d)\n",
			       __func__, chgprot, retry_cnt);
			max77705_update_reg(charger->i2c, MAX77705_CHG_REG_CNFG_06,
					    (0x03 << 2), (0x03 << 2));
			need_init = true;
			msleep(20);
		} else {
			break;
		}
	} while ((chgprot != 0x03) && (++retry_cnt < 10));

	return need_init;
}

static void check_charger_unlock_state(struct max77705_charger_data *charger)
{
	pr_debug("%s\n", __func__);

	if (max77705_charger_unlock(charger)) {
		pr_err("%s: charger locked state, reg init\n", __func__);
		max77705_charger_initialize(charger);
	}
}

static void max77705_test_read(struct max77705_charger_data *charger)
{
	u8 data = 0;
	u32 addr = 0;
	char str[1024] = { 0, };

	for (addr = 0xB1; addr <= 0xC3; addr++) {
		max77705_read_reg(charger->i2c, addr, &data);
		sprintf(str + strlen(str), "[0x%02x]0x%02x, ", addr, data);
	}
	pr_info("max77705 : %s\n", str);
}

__visible_for_testing int max77705_get_autoibus(struct max77705_charger_data *charger)
{
	u8 reg_data;

	max77705_read_reg(charger->i2c, MAX77705_CHG_REG_DETAILS_00, &reg_data);
	if (reg_data & 0x80)
		return 1; /* set by charger */

	return 0; /* set by USBC */
}

__visible_for_testing int max77705_get_vbus_state(struct max77705_charger_data *charger)
{
	u8 reg_data;

	max77705_read_reg(charger->i2c, MAX77705_CHG_REG_DETAILS_00, &reg_data);

	if (is_wireless_type(charger->cable_type))
		reg_data = ((reg_data & MAX77705_WCIN_DTLS) >>
			    MAX77705_WCIN_DTLS_SHIFT);
	else
		reg_data = ((reg_data & MAX77705_CHGIN_DTLS) >>
			    MAX77705_CHGIN_DTLS_SHIFT);

	switch (reg_data) {
	case 0x00:
		pr_info("%s: VBUS is invalid. CHGIN < CHGIN_UVLO\n", __func__);
		break;
	case 0x01:
		pr_info("%s: VBUS is invalid. CHGIN < MBAT+CHGIN2SYS and CHGIN > CHGIN_UVLO\n", __func__);
		break;
	case 0x02:
		pr_info("%s: VBUS is invalid. CHGIN > CHGIN_OVLO", __func__);
		break;
	case 0x03:
		pr_info("%s: VBUS is valid. CHGIN < CHGIN_OVLO", __func__);
		break;
	default:
		break;
	}

	return reg_data;
}

__visible_for_testing int max77705_get_charger_state(struct max77705_charger_data *charger)
{
	int status = POWER_SUPPLY_STATUS_UNKNOWN;
	u8 reg_data;

	max77705_read_reg(charger->i2c, MAX77705_CHG_REG_DETAILS_01, &reg_data);
	pr_info("%s : charger status (0x%02x)\n", __func__, reg_data);

	reg_data &= 0x0f;
	switch (reg_data) {
	case 0x00:
	case 0x01:
	case 0x02:
		status = POWER_SUPPLY_STATUS_CHARGING;
		break;
	case 0x03:
	case 0x04:
		status = POWER_SUPPLY_STATUS_FULL;
		break;
	case 0x05:
	case 0x06:
	case 0x07:
		status = POWER_SUPPLY_STATUS_NOT_CHARGING;
		break;
	case 0x08:
	case 0xA:
	case 0xB:
		status = POWER_SUPPLY_STATUS_DISCHARGING;
		break;
	default:
		break;
	}

	return (int)status;
}

__visible_for_testing bool max77705_chg_get_wdtmr_status(struct max77705_charger_data *charger)
{
	u8 reg_data;

	max77705_read_reg(charger->i2c, MAX77705_CHG_REG_DETAILS_01, &reg_data);
	reg_data = ((reg_data & MAX77705_CHG_DTLS) >> MAX77705_CHG_DTLS_SHIFT);

	if (reg_data == 0x0B) {
		dev_info(charger->dev, "WDT expired 0x%x !!\n", reg_data);
		return true;
	}

	return false;
}

__visible_for_testing int max77705_chg_set_wdtmr_en(struct max77705_charger_data *charger,
					bool enable)
{
	pr_info("%s: WDT en = %d\n", __func__, enable);
	max77705_update_reg(charger->i2c, MAX77705_CHG_REG_CNFG_00,
			(enable ? CHG_CNFG_00_WDTEN_MASK : 0), CHG_CNFG_00_WDTEN_MASK);

	return 0;
}

__visible_for_testing int max77705_chg_set_wdtmr_kick(struct max77705_charger_data *charger)
{
	pr_info("%s: WDT Kick\n", __func__);
	max77705_update_reg(charger->i2c, MAX77705_CHG_REG_CNFG_06,
			    (MAX77705_WDTCLR << CHG_CNFG_06_WDTCLR_SHIFT),
			    CHG_CNFG_06_WDTCLR_MASK);

	return 0;
}

__visible_for_testing void max77705_set_float_voltage(struct max77705_charger_data *charger,
					int float_voltage)
{
	u8 reg_data = 0;

#if defined(CONFIG_SEC_FACTORY)
	if (max77705_get_facmode()) {
		float_voltage = charger->pdata->fac_vsys;
		pr_info("%s: Factory Mode Skip set float voltage(%d)\n", __func__, float_voltage);
		// do not return here
	}
#endif
	reg_data =
		(float_voltage == 0) ? 0x13 :
		(float_voltage == 3800) ? 0x38 :
		(float_voltage == 3900) ? 0x39 :
	    (float_voltage >= 4500) ? 0x23 :
	    (float_voltage <= 4200) ? (float_voltage - 4000) / 50 :
	    (((float_voltage - 4200) / 10) + 0x04);

	max77705_update_reg(charger->i2c, MAX77705_CHG_REG_CNFG_04,
			    (reg_data << CHG_CNFG_04_CHG_CV_PRM_SHIFT),
			    CHG_CNFG_04_CHG_CV_PRM_MASK);

	max77705_read_reg(charger->i2c, MAX77705_CHG_REG_CNFG_04, &reg_data);
	pr_info("%s: battery cv voltage 0x%x\n", __func__, reg_data);
}

__visible_for_testing int max77705_get_float_voltage(struct max77705_charger_data *charger)
{
	u8 reg_data = 0;
	int float_voltage;

	max77705_read_reg(charger->i2c, MAX77705_CHG_REG_CNFG_04, &reg_data);
	reg_data &= 0x3F;
	float_voltage =
		(reg_data == 0x39) ? 3900 :
		(reg_data == 0x38) ? 3800 :
		(reg_data == 0x23) ? 4500 :
		(reg_data <= 0x04) ? reg_data * 50 + 4000 :
		(reg_data - 4) * 10 + 4200;
	pr_debug("%s: battery cv reg : 0x%x, float voltage val : %d\n",
		__func__, reg_data, float_voltage);

	return float_voltage;
}

__visible_for_testing int max77705_get_charging_health(struct max77705_charger_data *charger)
{
	union power_supply_propval value = {0,}, val_iin = {0,}, val_vbyp = {0,};
	int state = POWER_SUPPLY_HEALTH_GOOD;
	int vbus_state, retry_cnt;
	u8 chg_dtls, reg_data, chg_cnfg_00;
	bool wdt_status, abnormal_status = false;

	/* watchdog kick */
	max77705_chg_set_wdtmr_kick(charger);

	max77705_read_reg(charger->i2c, MAX77705_CHG_REG_DETAILS_01, &reg_data);
	reg_data = ((reg_data & MAX77705_BAT_DTLS) >> MAX77705_BAT_DTLS_SHIFT);

	if (charger->pdata->enable_noise_wa) {
		psy_do_property("battery", get, POWER_SUPPLY_PROP_CAPACITY, value);
		if ((value.intval >= 80) &&
			(charger->fsw_now != MAX77705_CHG_FSW_3MHz))
			max77705_set_switching_frequency(charger, MAX77705_CHG_FSW_3MHz);
		else if ((value.intval < 80) &&
			(charger->fsw_now != MAX77705_CHG_FSW_1_5MHz))
			max77705_set_switching_frequency(charger, MAX77705_CHG_FSW_1_5MHz);
	}

	pr_info("%s: reg_data(0x%x)\n", __func__, reg_data);
	switch (reg_data) {
	case 0x00:
		pr_info("%s: No battery and the charger is suspended\n", __func__);
		break;
	case 0x01:
		pr_info("%s: battery is okay but its voltage is low(~VPQLB)\n", __func__);
		break;
	case 0x02:
		pr_info("%s: battery dead\n", __func__);
		break;
	case 0x03:
		break;
	case 0x04:
		pr_info("%s: battery is okay but its voltage is low\n", __func__);
		break;
	case 0x05:
		pr_info("%s: battery ovp\n", __func__);
		break;
	default:
		pr_info("%s: battery unknown\n", __func__);
		break;
	}
	if (charger->is_charging) {
		max77705_read_reg(charger->i2c,	MAX77705_CHG_REG_DETAILS_00, &reg_data);
		pr_info("%s: details00(0x%x)\n", __func__, reg_data);
	}

	/* get wdt status */
	wdt_status = max77705_chg_get_wdtmr_status(charger);

	psy_do_property("battery", get, POWER_SUPPLY_EXT_PROP_HEALTH, value);
	/* VBUS OVP state return battery OVP state */
	vbus_state = max77705_get_vbus_state(charger);
	/* read CHG_DTLS and detecting battery terminal error */
	max77705_read_reg(charger->i2c, MAX77705_CHG_REG_DETAILS_01, &chg_dtls);
	chg_dtls = ((chg_dtls & MAX77705_CHG_DTLS) >> MAX77705_CHG_DTLS_SHIFT);
	max77705_read_reg(charger->i2c, MAX77705_CHG_REG_CNFG_00, &chg_cnfg_00);

	/* print the log at the abnormal case */
	if ((charger->is_charging == 1) && !charger->uno_on
		&& ((chg_dtls == 0x08) || (chg_dtls == 0x0B))) {
		max77705_test_read(charger);
		max77705_chg_set_mode_state(charger, SEC_BAT_CHG_MODE_CHARGING_OFF);
		max77705_set_float_voltage(charger, charger->float_voltage);
		max77705_chg_set_mode_state(charger, SEC_BAT_CHG_MODE_CHARGING);
		abnormal_status = true;
	}

	val_iin.intval = SEC_BATTERY_IIN_MA;
	psy_do_property("max77705-fuelgauge", get,
		POWER_SUPPLY_EXT_PROP_MEASURE_INPUT, val_iin);

	val_vbyp.intval = SEC_BATTERY_VBYP;
	psy_do_property("max77705-fuelgauge", get,
		POWER_SUPPLY_EXT_PROP_MEASURE_INPUT, val_vbyp);

	pr_info("%s: vbus_state: 0x%x, chg_dtls: 0x%x, iin: %dmA, vbyp: %dmV, health: %d, abnormal: %s\n",
		__func__, vbus_state, chg_dtls, val_iin.intval,
		val_vbyp.intval, value.intval, (abnormal_status ? "true" : "false"));

	/*  OVP is higher priority */
	if (vbus_state == 0x02) {	/*  CHGIN_OVLO */
		pr_info("%s: vbus ovp\n", __func__);
		if (is_wireless_type(charger->cable_type)) {
			retry_cnt = 0;
			do {
				msleep(50);
				vbus_state = max77705_get_vbus_state(charger);
			} while ((retry_cnt++ < 2) && (vbus_state == 0x02));
			if (vbus_state == 0x02) {
				state = POWER_SUPPLY_HEALTH_OVERVOLTAGE;
				pr_info("%s: wpc and over-voltage\n", __func__);
			} else {
				state = POWER_SUPPLY_HEALTH_GOOD;
			}
		} else {
			state = POWER_SUPPLY_HEALTH_OVERVOLTAGE;
		}
	} else if (((vbus_state == 0x0) || (vbus_state == 0x01)) && (chg_dtls & 0x08)
		   && (chg_cnfg_00 & MAX77705_MODE_5_BUCK_CHG_ON)
		   && (is_not_wireless_type(charger->cable_type))) {
		pr_info("%s: vbus is under\n", __func__);
		state = POWER_SUPPLY_EXT_HEALTH_UNDERVOLTAGE;
	} else if ((value.intval == POWER_SUPPLY_EXT_HEALTH_UNDERVOLTAGE) &&
		   ((vbus_state == 0x0) || (vbus_state == 0x01)) &&
		   (is_not_wireless_type(charger->cable_type))) {
		pr_info("%s: keep under-voltage\n", __func__);
		state = POWER_SUPPLY_EXT_HEALTH_UNDERVOLTAGE;
	} else if (wdt_status) {
		pr_info("%s: wdt expired\n", __func__);
		state = POWER_SUPPLY_HEALTH_WATCHDOG_TIMER_EXPIRE;
	} else if (is_wireless_type(charger->cable_type)) {
		if (abnormal_status || (vbus_state == 0x00) || (vbus_state == 0x01))
			charger->misalign_cnt++;
		else
			charger->misalign_cnt = 0;

		if (charger->misalign_cnt >= 3) {
			pr_info("%s: invalid WCIN, Misalign occurs!\n", __func__);
			value.intval = POWER_SUPPLY_STATUS_NOT_CHARGING;
			psy_do_property(charger->pdata->wireless_charger_name,
				set, POWER_SUPPLY_PROP_STATUS, value);
		}
	}

	return (int)state;
}

__visible_for_testing int max77705_get_charge_current(struct max77705_charger_data *charger)
{
	u8 reg_data;
	int get_current = 0;

	max77705_read_reg(charger->i2c, MAX77705_CHG_REG_CNFG_02, &reg_data);
	reg_data &= MAX77705_CHG_CC;

	get_current = reg_data <= 0x2 ? 100 : reg_data * 50;

	pr_info("%s: reg:(0x%x), charging_current:(%d)\n",
			__func__, reg_data, get_current);

	return get_current;
}

__visible_for_testing int max77705_get_input_current_type(struct max77705_charger_data
					*charger, int cable_type)
{
	u8 reg_data;
	int get_current = 0;

	if (cable_type == SEC_BATTERY_CABLE_WIRELESS) {
		max77705_read_reg(charger->i2c, MAX77705_CHG_REG_CNFG_10, &reg_data);
		/* AND operation for removing the formal 2bit  */
		reg_data &= MAX77705_CHG_WCIN_LIM;

		if (reg_data <= 0x3)
			get_current = 100;
		else if (reg_data >= 0x3F)
			get_current = 1600;
		else
			get_current = (reg_data + 0x01) * 25;
	} else {
		max77705_read_reg(charger->i2c, MAX77705_CHG_REG_CNFG_09, &reg_data);
		/* AND operation for removing the formal 1bit  */
		reg_data &= MAX77705_CHG_CHGIN_LIM;

		if (reg_data <= 0x3)
			get_current = 100;
		else if (reg_data >= 0x7F)
			get_current = 3200;
		else
			get_current = (reg_data + 0x01) * 25;
	}
	pr_info("%s: reg:(0x%x), charging_current:(%d)\n",
			__func__, reg_data, get_current);

	return get_current;
}

__visible_for_testing int max77705_get_input_current(struct max77705_charger_data *charger)
{
	if (is_wireless_type(charger->cable_type))
		return max77705_get_input_current_type(charger, SEC_BATTERY_CABLE_WIRELESS);
	else
		return max77705_get_input_current_type(charger,	SEC_BATTERY_CABLE_TA);
}

__visible_for_testing void reduce_input_current(struct max77705_charger_data *charger, int curr)
{
	u8 set_reg = 0, set_mask = 0, set_value = 0;
	unsigned int curr_step = 25;
	int input_current = 0, max_value = 0;

	input_current = max77705_get_input_current(charger);
	if (input_current <= MINIMUM_INPUT_CURRENT)
		return;

	if (is_wireless_type(charger->cable_type)) {
		set_reg = MAX77705_CHG_REG_CNFG_10;
		set_mask = MAX77705_CHG_WCIN_LIM;
		max_value = 1600;
	} else {
		set_reg = MAX77705_CHG_REG_CNFG_09;
		set_mask = MAX77705_CHG_CHGIN_LIM;
		max_value = 3200;
	}

	input_current -= curr;
	input_current = (input_current > max_value) ? max_value :
		((input_current < MINIMUM_INPUT_CURRENT) ? MINIMUM_INPUT_CURRENT : input_current);

	set_value |= (input_current / curr_step) - 0x01;
	max77705_update_reg(charger->i2c, set_reg, set_value, set_mask);

	pr_info("%s: reg:(0x%x), val(0x%x), input current(%d)\n",
		__func__, set_reg, set_value, input_current);
	charger->input_current = max77705_get_input_current(charger);
	charger->aicl_curr = input_current;
}

__visible_for_testing bool max77705_check_battery(struct max77705_charger_data *charger)
{
	u8 reg_data, reg_data2;

	max77705_read_reg(charger->i2c, MAX77705_CHG_REG_INT_OK, &reg_data);
	max77705_read_reg(charger->i2c, MAX77705_CHG_REG_DETAILS_00, &reg_data2);
	pr_info("%s: CHG_INT_OK(0x%x), CHG_DETAILS00(0x%x)\n",
		__func__, reg_data, reg_data2);

	if ((reg_data & MAX77705_BATP_OK) || !(reg_data2 & MAX77705_BATP_DTLS))
		return true;
	else
		return false;
}

static void max77705_check_cnfg12_reg(struct max77705_charger_data *charger)
{
	static bool is_valid = true;
	u8 valid_cnfg12, reg_data;

	if (is_valid) {
		reg_data = MAX77705_CHG_WCINSEL;
		valid_cnfg12 = (is_wireless_type(charger->cable_type)) ?
			reg_data : (reg_data | (1 << CHG_CNFG_12_CHGINSEL_SHIFT));

		max77705_read_reg(charger->i2c, MAX77705_CHG_REG_CNFG_12, &reg_data);
		pr_info("%s: valid_data = 0x%2x, reg_data = 0x%2x\n",
			__func__, valid_cnfg12, reg_data);
		if (valid_cnfg12 != reg_data) {
			max77705_test_read(charger);
			is_valid = false;
		}
	}
}

__visible_for_testing void max77705_change_charge_path(struct max77705_charger_data *charger,
					int path)
{
	u8 cnfg12;

	if (is_wireless_type(path))
		cnfg12 = (0 << CHG_CNFG_12_CHGINSEL_SHIFT);
	else
		cnfg12 = (1 << CHG_CNFG_12_CHGINSEL_SHIFT);

	max77705_update_reg(charger->i2c, MAX77705_CHG_REG_CNFG_12,
			    cnfg12, CHG_CNFG_12_CHGINSEL_MASK);
	max77705_read_reg(charger->i2c, MAX77705_CHG_REG_CNFG_12, &cnfg12);
	pr_info("%s : CHG_CNFG_12(0x%02x)\n", __func__, cnfg12);

	max77705_check_cnfg12_reg(charger);
}

__visible_for_testing void max77705_set_ship_mode(struct max77705_charger_data *charger,
					int enable)
{
	u8 cnfg07 = ((enable ? 1 : 0) << CHG_CNFG_07_REG_SHIPMODE_SHIFT);

	max77705_update_reg(charger->i2c, MAX77705_CHG_REG_CNFG_07,
			    cnfg07, CHG_CNFG_07_REG_SHIPMODE_MASK);
	max77705_read_reg(charger->i2c, MAX77705_CHG_REG_CNFG_07, &cnfg07);
	pr_info("%s : CHG_CNFG_07(0x%02x)\n", __func__, cnfg07);
}

__visible_for_testing void max77705_set_auto_ship_mode(struct max77705_charger_data *charger,
					int enable)
{
	u8 cnfg03 = ((enable ? 1 : 0) << CHG_CNFG_03_REG_AUTO_SHIPMODE_SHIFT);

	/* auto ship mode should work under 2.6V */
	max77705_update_reg(charger->i2c, MAX77705_CHG_REG_CNFG_03,
			    cnfg03, CHG_CNFG_03_REG_AUTO_SHIPMODE_MASK);
	max77705_read_reg(charger->i2c, MAX77705_CHG_REG_CNFG_03, &cnfg03);
	pr_info("%s : CHG_CNFG_03(0x%02x)\n", __func__, cnfg03);
}

__visible_for_testing void max77705_set_input_current(struct max77705_charger_data *charger,
					int input_current)
{
	int curr_step = 25;
	u8 set_reg, set_mask, reg_data = 0;
#if defined(CONFIG_SEC_FACTORY)
	if (max77705_get_facmode()) {
		pr_info("%s: Factory Mode Skip set input current\n", __func__);
		return;
	}
#endif

	mutex_lock(&charger->charger_mutex);

	if (is_wireless_type(charger->cable_type)) {
		set_reg = MAX77705_CHG_REG_CNFG_10;
		set_mask = MAX77705_CHG_WCIN_LIM;
		input_current = (input_current > 1600) ? 1600 : input_current;
	} else {
		set_reg = MAX77705_CHG_REG_CNFG_09;
		set_mask = MAX77705_CHG_CHGIN_LIM;
		input_current = (input_current > 3200) ? 3200 : input_current;
	}

	if (input_current >= 100)
		reg_data = (input_current / curr_step) - 0x01;

	max77705_update_reg(charger->i2c, set_reg, reg_data, set_mask);

	if (!max77705_get_autoibus(charger))
		max77705_set_fw_noautoibus(MAX77705_AUTOIBUS_AT_OFF);

	mutex_unlock(&charger->charger_mutex);
	pr_info("[%s] REG(0x%02x) DATA(0x%02x), CURRENT(%d)\n",
		__func__, set_reg, reg_data, input_current);
}

__visible_for_testing void max77705_set_charge_current(struct max77705_charger_data *charger,
					int fast_charging_current)
{
	int curr_step = 50;
	u8 reg_data = 0;
#if defined(CONFIG_SEC_FACTORY)
	if (max77705_get_facmode()) {
		pr_info("%s: Factory Mode Skip set charge current\n", __func__);
		return;
	}
#endif

	fast_charging_current =
		(fast_charging_current > 3150) ? 3150 : fast_charging_current;
	if (fast_charging_current >= 100)
		reg_data |= (fast_charging_current / curr_step);

	max77705_update_reg(charger->i2c, MAX77705_CHG_REG_CNFG_02,
		reg_data, MAX77705_CHG_CC);

	pr_info("[%s] REG(0x%02x) DATA(0x%02x), CURRENT(%d)\n", __func__,
		MAX77705_CHG_REG_CNFG_02, reg_data, fast_charging_current);
}

__visible_for_testing void max77705_set_wireless_input_current(
				struct max77705_charger_data *charger, int input_current)
{
	union power_supply_propval value;
	unsigned int work_state;

	cancel_delayed_work(&charger->wc_chg_current_work);
	__pm_relax(charger->wc_chg_current_ws);

	if (is_wireless_type(charger->cable_type)) {
		work_state = work_busy(&charger->wc_current_work.work);
		pr_info("%s: check wc_current_work state(0x%x)\n", __func__, work_state);
		if (!(work_state & (WORK_BUSY_PENDING | WORK_BUSY_RUNNING))) {
			/* Wcurr-A) In cases of wireless input current change,
			 * configure the Vrect adj room to 270mV for safe wireless charging.
			 */
			if (is_hv_wireless_type(charger->cable_type))
				value.intval = WIRELESS_VRECT_ADJ_ROOM_1; /* Vrect Room 277mV */
			else
				value.intval = charger->pdata->nv_wc_headroom;
			psy_do_property(charger->pdata->wireless_charger_name, set,
					POWER_SUPPLY_EXT_PROP_WIRELESS_RX_CONTROL, value);
			msleep(500); /* delay 0.5sec */
		}
		charger->wc_pre_current = max77705_get_input_current(charger);
		charger->wc_current = input_current;
		pr_info("%s: wc_current(%d), wc_pre_current(%d)\n",
				__func__, charger->wc_current, charger->wc_pre_current);
		if (charger->wc_current > charger->wc_pre_current)
			max77705_set_charge_current(charger, charger->charging_current);
	}
	mutex_lock(&charger->icl_mutex);
	__pm_stay_awake(charger->wc_current_ws);
	queue_delayed_work(charger->wqueue, &charger->wc_current_work, 0);
	mutex_unlock(&charger->icl_mutex);
}

__visible_for_testing void max77705_set_topoff_current(struct max77705_charger_data *charger,
					int termination_current)
{
	int curr_base = 150, curr_step = 50;
	u8 reg_data;

	if (termination_current < curr_base)
		termination_current = curr_base;
	else if (termination_current > 500)
		termination_current = 500;

	reg_data = (termination_current - curr_base) / curr_step;
	max77705_update_reg(charger->i2c, MAX77705_CHG_REG_CNFG_03,
			    reg_data, CHG_CNFG_03_TO_ITH_MASK);

	pr_info("%s: reg_data(0x%02x), topoff(%dmA)\n",
		__func__, reg_data, termination_current);
}

__visible_for_testing void max77705_set_topoff_time(struct max77705_charger_data *charger,
					int topoff_time)
{
	u8 reg_data = (topoff_time / 10) << CHG_CNFG_03_TO_TIME_SHIFT;

	max77705_update_reg(charger->i2c, MAX77705_CHG_REG_CNFG_03,
		reg_data, CHG_CNFG_03_TO_TIME_MASK);

	pr_info("%s: reg_data(0x%02x), topoff_time(%dmin)\n",
		__func__, reg_data, topoff_time);
}

__visible_for_testing void max77705_set_switching_frequency(struct max77705_charger_data *charger,
				int frequency)
{
	u8 cnfg_08;

	/* Set Switching Frequency */
	max77705_update_reg(charger->i2c, MAX77705_CHG_REG_CNFG_08,
			(frequency << CHG_CNFG_08_REG_FSW_SHIFT),
			CHG_CNFG_08_REG_FSW_MASK);
	max77705_read_reg(charger->i2c, MAX77705_CHG_REG_CNFG_08, &cnfg_08);

	charger->fsw_now = frequency;
	pr_info("%s : CHG_CNFG_08(0x%02x)\n", __func__, cnfg_08);
}

__visible_for_testing void max77705_set_skipmode(struct max77705_charger_data *charger,
				int enable)
{
	u8 reg_data = enable ? MAX77705_AUTO_SKIP : MAX77705_DISABLE_SKIP;

	max77705_update_reg(charger->i2c, MAX77705_CHG_REG_CNFG_12,
			reg_data << CHG_CNFG_12_REG_DISKIP_SHIFT,
			CHG_CNFG_12_REG_DISKIP_MASK);
}

__visible_for_testing void max77705_set_b2sovrc(struct max77705_charger_data *charger,
					u32 ocp_current, u32 ocp_dtc)
{
	u8 reg_data = MAX77705_B2SOVRC_4_6A;

	if (ocp_current == 0)
		reg_data = MAX77705_B2SOVRC_DISABLE;
	else
		reg_data += (ocp_current - 4600) / 200;

	max77705_update_reg(charger->i2c, MAX77705_CHG_REG_CNFG_05,
		(reg_data << CHG_CNFG_05_REG_B2SOVRC_SHIFT),
		CHG_CNFG_05_REG_B2SOVRC_MASK);

	max77705_update_reg(charger->i2c, MAX77705_CHG_REG_CNFG_06,
		((ocp_dtc == 100 ? MAX77705_B2SOVRC_DTC_100MS : 0)
		<< CHG_CNFG_06_B2SOVRC_DTC_SHIFT), CHG_CNFG_06_B2SOVRC_DTC_MASK);

	max77705_read_reg(charger->i2c, MAX77705_CHG_REG_CNFG_05, &reg_data);
	pr_info("%s : CHG_CNFG_05(0x%02x)\n", __func__, reg_data);
	max77705_read_reg(charger->i2c, MAX77705_CHG_REG_CNFG_06, &reg_data);
	pr_info("%s : CHG_CNFG_06(0x%02x)\n", __func__, reg_data);

	return;
}

__visible_for_testing int max77705_check_wcin_before_otg_on(struct max77705_charger_data *charger)
{
	union power_supply_propval value = {0,};
	struct power_supply *psy;
	u8 reg_data;

	psy = get_power_supply_by_name("wireless");
	if (!psy)
		return -ENODEV;
	if ((psy->desc->get_property != NULL) &&
		(psy->desc->get_property(psy, POWER_SUPPLY_PROP_ONLINE, &value) >= 0)) {
		if (value.intval)
			return 0;
	} else {
		return -ENODEV;
	}
	power_supply_put(psy);

#if defined(CONFIG_WIRELESS_TX_MODE)
	/* check TX status */
	max77705_read_reg(charger->i2c, MAX77705_CHG_REG_CNFG_00, &reg_data);
	reg_data &= CHG_CNFG_00_MODE_MASK;
	if (reg_data == MAX77705_MODE_8_BOOST_UNO_ON ||
		reg_data == MAX77705_MODE_C_BUCK_BOOST_UNO_ON ||
		reg_data == MAX77705_MODE_D_BUCK_CHG_BOOST_UNO_ON) {
		value.intval = BATT_TX_EVENT_WIRELESS_TX_OTG_ON;
		psy_do_property("wireless", set,
			POWER_SUPPLY_EXT_PROP_WIRELESS_TX_ERR, value);
		return 0;
	}
#endif

	max77705_read_reg(charger->i2c, MAX77705_CHG_REG_DETAILS_00, &reg_data);
	reg_data = ((reg_data & MAX77705_WCIN_DTLS) >> MAX77705_WCIN_DTLS_SHIFT);
	if ((reg_data != 0x03) || (charger->pdata->wireless_charger_name == NULL))
		return 0;

	psy_do_property(charger->pdata->wireless_charger_name, get,
		POWER_SUPPLY_PROP_ENERGY_NOW, value);
	if (value.intval <= 0)
		return -ENODEV;

	value.intval = WIRELESS_VOUT_5V;
	psy_do_property(charger->pdata->wireless_charger_name, set,
		POWER_SUPPLY_EXT_PROP_INPUT_VOLTAGE_REGULATION, value);

    return 0;
}

__visible_for_testing int max77705_set_otg(struct max77705_charger_data *charger, int enable)
{
	union power_supply_propval value;
	u8 chg_int_state;
	int ret = 0;

	pr_info("%s: CHGIN-OTG %s\n", __func__,	enable > 0 ? "on" : "off");
	if (charger->otg_on == enable || max77705_get_lpmode())
		return 0;

	if (charger->pdata->wireless_charger_name) {
		ret = max77705_check_wcin_before_otg_on(charger);
		pr_info("%s: wc_state = %d\n", __func__, ret);
		if (ret < 0)
			return ret;
	}

	__pm_stay_awake(charger->otg_ws);
	/* CHGIN-OTG */
	value.intval = enable;

	/* otg current limit 900mA */
	max77705_update_reg(charger->i2c, MAX77705_CHG_REG_CNFG_02,
			MAX77705_OTG_ILIM_900 << CHG_CNFG_02_OTG_ILIM_SHIFT,
			CHG_CNFG_02_OTG_ILIM_MASK);

	if (enable) {
		psy_do_property("wireless", set,
			POWER_SUPPLY_EXT_PROP_CHARGE_OTG_CONTROL, value);

		mutex_lock(&charger->charger_mutex);
		charger->otg_on = enable;
		/* OTG on, boost on */
		max77705_chg_set_mode_state(charger, SEC_BAT_CHG_MODE_OTG_ON);
		mutex_unlock(&charger->charger_mutex);
	} else {
		mutex_lock(&charger->charger_mutex);
		charger->otg_on = enable;
		/* OTG off(UNO on), boost off */
		max77705_chg_set_mode_state(charger, SEC_BAT_CHG_MODE_OTG_OFF);
		mutex_unlock(&charger->charger_mutex);
		msleep(50);

		psy_do_property("wireless", set,
			POWER_SUPPLY_EXT_PROP_CHARGE_OTG_CONTROL, value);
	}
	max77705_read_reg(charger->i2c, MAX77705_CHG_REG_INT_MASK, &chg_int_state);

	__pm_relax(charger->otg_ws);
	pr_info("%s: INT_MASK(0x%x)\n", __func__, chg_int_state);
	power_supply_changed(charger->psy_otg);

	return 0;
}

__visible_for_testing void max77705_check_slow_charging(struct max77705_charger_data *charger,
					int input_current)
{
	union power_supply_propval value;

	/* under 400mA considered as slow charging concept for VZW */
	if (input_current <= SLOW_CHARGING_CURRENT_STANDARD &&
	    !is_nocharge_type(charger->cable_type)) {
		charger->slow_charging = true;
		pr_info("%s: slow charging on : input current(%dmA), cable type(%d)\n",
		     __func__, input_current, charger->cable_type);

		psy_do_property("battery", set,	POWER_SUPPLY_PROP_CHARGE_TYPE, value);
	} else {
		charger->slow_charging = false;
	}
}

__visible_for_testing void max77705_charger_initialize(struct max77705_charger_data *charger)
{
	u8 reg_data;
	int jig_gpio;

	pr_info("%s\n", __func__);

	max77705_read_reg(charger->i2c, MAX77705_CHG_REG_CNFG_00, &reg_data);
	charger->cnfg00_mode = (reg_data & CHG_CNFG_00_MODE_MASK);

	/* unlock charger setting protect slowest LX slope
	 */
	reg_data = (0x03 << 2);
	reg_data |= 0x60;
	max77705_update_reg(charger->i2c, MAX77705_CHG_REG_CNFG_06, reg_data, reg_data);

#if !defined(CONFIG_SEC_FACTORY)
	/* If DIS_AICL is enabled(CNFG06[4]: 1) from factory_mode,
	 * clear to 0 to disable DIS_AICL
	 */
	max77705_update_reg(charger->i2c, MAX77705_CHG_REG_CNFG_06,
			MAX77705_DIS_AICL << CHG_CNFG_06_DIS_AICL_SHIFT,
			CHG_CNFG_06_DIS_AICL_MASK);
#endif

	/* fast charge timer disable
	 * restart threshold disable
	 * pre-qual charge disable
	 */
	reg_data = (MAX77705_FCHGTIME_DISABLE << CHG_CNFG_01_FCHGTIME_SHIFT) |
			(MAX77705_CHG_RSTRT_DISABLE << CHG_CNFG_01_CHG_RSTRT_SHIFT) |
			(MAX77705_CHG_PQEN_DISABLE << CHG_CNFG_01_PQEN_SHIFT);
	max77705_update_reg(charger->i2c, MAX77705_CHG_REG_CNFG_01, reg_data,
	(CHG_CNFG_01_FCHGTIME_MASK | CHG_CNFG_01_CHG_RSTRT_MASK | CHG_CNFG_01_PQEN_MASK));

	/* enalbe RECYCLE_EN for ocp */
	max77705_update_reg(charger->i2c, MAX77705_CHG_REG_CNFG_01,
			MAX77705_RECYCLE_EN_ENABLE << CHG_CNFG_01_RECYCLE_EN_SHIFT,
			CHG_CNFG_01_RECYCLE_EN_MASK);

	/* OTG off(UNO on), boost off */
	max77705_update_reg(charger->i2c, MAX77705_CHG_REG_CNFG_00,
			0, CHG_CNFG_00_OTG_CTRL);

	/* otg current limit 900mA */
	max77705_update_reg(charger->i2c, MAX77705_CHG_REG_CNFG_02,
			MAX77705_OTG_ILIM_900 << CHG_CNFG_02_OTG_ILIM_SHIFT,
			CHG_CNFG_02_OTG_ILIM_MASK);

	/* UNO ILIM 1.0A */
	max77705_update_reg(charger->i2c, MAX77705_CHG_REG_CNFG_05,
			MAX77705_UNOILIM_1000 << CHG_CNFG_05_REG_UNOILIM_SHIFT,
			CHG_CNFG_05_REG_UNOILIM_MASK);

	/* BAT to SYS OCP */
	max77705_set_b2sovrc(charger,
		charger->pdata->chg_ocp_current, charger->pdata->chg_ocp_dtc);

	/* top off current 150mA */
	reg_data = (MAX77705_TO_ITH_150MA << CHG_CNFG_03_TO_ITH_SHIFT) |
			(MAX77705_SYS_TRACK_DISABLE << CHG_CNFG_03_SYS_TRACK_DIS_SHIFT);
	max77705_update_reg(charger->i2c, MAX77705_CHG_REG_CNFG_03, reg_data,
	(CHG_CNFG_03_TO_ITH_MASK | CHG_CNFG_03_TO_TIME_MASK | CHG_CNFG_03_SYS_TRACK_DIS_MASK));

	/* topoff_time */
	max77705_set_topoff_time(charger, charger->pdata->topoff_time);

	/* cv voltage 4.2V or 4.35V */
	max77705_set_float_voltage(charger, charger->pdata->chg_float_voltage);

	max77705_read_reg(charger->i2c, MAX77705_CHG_REG_INT_OK, &reg_data);
	/* VCHGIN : REG=4.6V, UVLO=4.8V
	 * to fix CHGIN-UVLO issues including cheapy battery packs
	 */
	if (reg_data & MAX77705_CHGIN_OK)
		max77705_update_reg(charger->i2c, MAX77705_CHG_REG_CNFG_12,
			0x08, CHG_CNFG_12_VCHGIN_REG_MASK);
	/* VCHGIN : REG=4.5V, UVLO=4.7V */
	else
		max77705_update_reg(charger->i2c, MAX77705_CHG_REG_CNFG_12,
			0x00, CHG_CNFG_12_VCHGIN_REG_MASK);

	/* Boost mode possible in FACTORY MODE */
	max77705_update_reg(charger->i2c, MAX77705_CHG_REG_CNFG_07,
			    MAX77705_CHG_FMBST, CHG_CNFG_07_REG_FMBST_MASK);

	if (charger->jig_low_active)
		jig_gpio = !gpio_get_value(charger->jig_gpio);
	else
		jig_gpio = gpio_get_value(charger->jig_gpio);

	pr_info("%s jig_gpio = %d \n", __func__, jig_gpio);

#if defined(CONFIG_SEC_FACTORY)
	if (max77705_get_facmode()) {
		/* fgsrc should depend on factory_mode since 301k and 619k do not triger jig_gpio */
		max77705_update_reg(charger->i2c, MAX77705_CHG_REG_CNFG_07,
				    (1 << CHG_CNFG_07_REG_FGSRC_SHIFT),
				    CHG_CNFG_07_REG_FGSRC_MASK);
		/* Watchdog Disable */
		max77705_chg_set_wdtmr_en(charger, 0);
	} else {
		/* fgsrc should depend on jig_gpio */
		max77705_update_reg(charger->i2c, MAX77705_CHG_REG_CNFG_07,
			    (jig_gpio << CHG_CNFG_07_REG_FGSRC_SHIFT),
			    CHG_CNFG_07_REG_FGSRC_MASK);
		/* Watchdog Enable */
		max77705_chg_set_wdtmr_en(charger, 1);
	}
#else
	max77705_update_reg(charger->i2c, MAX77705_CHG_REG_CNFG_07,
		    (jig_gpio << CHG_CNFG_07_REG_FGSRC_SHIFT),
		    CHG_CNFG_07_REG_FGSRC_MASK);
	/* Watchdog Enable */
	max77705_chg_set_wdtmr_en(charger, 1);
#endif

	/* Active Discharge Enable */
	max77705_update_reg(charger->pmic_i2c, MAX77705_PMIC_REG_MAINCTRL1, 0x01, 0x01);

	max77705_update_reg(charger->i2c, MAX77705_CHG_REG_CNFG_09,
			    MAX77705_CHG_EN, MAX77705_CHG_EN);

	/* VBYPSET=5.0V */
	max77705_update_reg(charger->i2c, MAX77705_CHG_REG_CNFG_11,
			0x00, CHG_CNFG_11_VBYPSET_MASK);

	/* Switching Frequency */
	max77705_set_switching_frequency(charger, charger->pdata->fsw);

	/* Auto skip mode */
	max77705_set_skipmode(charger, 1);

	/* disable shipmode */
	max77705_set_ship_mode(charger, 0);

	/* enable auto shipmode, this should work under 2.6V */
	max77705_set_auto_ship_mode(charger, 0);

	max77705_test_read(charger);
}

__visible_for_testing void max77705_set_sysovlo(struct max77705_charger_data *charger, int enable)
{
	u8 reg_data;

	max77705_read_reg(charger->pmic_i2c,
		MAX77705_PMIC_REG_SYSTEM_INT_MASK, &reg_data);

	reg_data = enable ? reg_data & 0xDF : reg_data | 0x20;
	max77705_write_reg(charger->pmic_i2c,
		MAX77705_PMIC_REG_SYSTEM_INT_MASK, reg_data);

	max77705_read_reg(charger->pmic_i2c,
		MAX77705_PMIC_REG_SYSTEM_INT_MASK, &reg_data);
	pr_info("%s: check topsys irq mask(0x%x), enable(%d)\n",
		__func__, reg_data, enable);
}

static void max77705_chg_monitor_work(struct max77705_charger_data *charger)
{
	u8 reg_b2sovrc = 0, reg_mode = 0;

	max77705_read_reg(charger->i2c, MAX77705_CHG_REG_CNFG_00, &reg_mode);
	reg_mode = (reg_mode & CHG_CNFG_00_MODE_MASK) >> CHG_CNFG_00_MODE_SHIFT;

	max77705_read_reg(charger->i2c, MAX77705_CHG_REG_CNFG_05, &reg_b2sovrc);
	reg_b2sovrc =
		(reg_b2sovrc & CHG_CNFG_05_REG_B2SOVRC_MASK) >> CHG_CNFG_05_REG_B2SOVRC_SHIFT;

	pr_info("%s: [CHG] MODE(0x%x), B2SOVRC(0x%x), otg_on(%d)\n",
		__func__, reg_mode, reg_b2sovrc, charger->otg_on);
}

static int max77705_chg_create_attrs(struct device *dev)
{
	int i, rc;

	for (i = 0; i < (int)ARRAY_SIZE(max77705_charger_attrs); i++) {
		rc = device_create_file(dev, &max77705_charger_attrs[i]);
		if (rc)
			goto create_attrs_failed;
	}
	return rc;

create_attrs_failed:
	dev_err(dev, "%s: failed (%d)\n", __func__, rc);
	while (i--)
		device_remove_file(dev, &max77705_charger_attrs[i]);
	return rc;
}

ssize_t max77705_chg_show_attrs(struct device *dev,
				struct device_attribute *attr, char *buf)
{
	struct power_supply *psy = dev_get_drvdata(dev);
	struct max77705_charger_data *charger = power_supply_get_drvdata(psy);
	const ptrdiff_t offset = attr - max77705_charger_attrs;
	int i = 0;
	u8 addr, data;

	switch (offset) {
	case CHIP_ID:
		max77705_read_reg(charger->pmic_i2c, MAX77705_PMIC_REG_PMICID1, &data);
		i += scnprintf(buf + i, PAGE_SIZE - i, "%x\n", data);
		break;
	case DATA:
		for (addr = 0xB1; addr <= 0xC3; addr++) {
			max77705_read_reg(charger->i2c, addr, &data);
			i += scnprintf(buf + i, PAGE_SIZE - i,
				       "0x%02x : 0x%02x\n", addr, data);
		}
		break;
	default:
		return -EINVAL;
	}
	return i;
}

ssize_t max77705_chg_store_attrs(struct device *dev,
				struct device_attribute *attr,
				const char *buf, size_t count)
{
	struct power_supply *psy = dev_get_drvdata(dev);
	struct max77705_charger_data *charger = power_supply_get_drvdata(psy);
	const ptrdiff_t offset = attr - max77705_charger_attrs;
	int ret = 0;
	int x, y;

	switch (offset) {
	case CHIP_ID:
		ret = count;
		break;
	case DATA:
		if (sscanf(buf, "0x%8x 0x%8x", &x, &y) == 2) {
			if (x >= 0xB1 && x <= 0xC3) {
				u8 addr = x, data = y;

				if (max77705_write_reg(charger->i2c, addr, data) < 0)
					dev_info(charger->dev, "%s: addr: 0x%x write fail\n",
						__func__, addr);
			} else {
				dev_info(charger->dev, "%s: addr: 0x%x is wrong\n",
					__func__, x);
			}
		}
		ret = count;
		break;
	default:
		ret = -EINVAL;
	}
	return ret;
}

__visible_for_testing void max77705_set_uno(struct max77705_charger_data *charger, int en)
{
	u8 chg_int_state;
	u8 reg;

	if (charger->otg_on) {
		pr_info("%s: OTG ON, then skip UNO Control\n", __func__);
		if (en) {
#if defined(CONFIG_WIRELESS_TX_MODE)
			union power_supply_propval value = {0, };
			psy_do_property("battery", get,
				POWER_SUPPLY_EXT_PROP_WIRELESS_TX_ENABLE, value);
			if (value.intval) {
				value.intval = BATT_TX_EVENT_WIRELESS_TX_ETC;
				psy_do_property("wireless", set, POWER_SUPPLY_EXT_PROP_WIRELESS_TX_ERR, value);
			}
#endif
		}
		return;
	}

	max77705_read_reg(charger->i2c, MAX77705_CHG_REG_INT_OK, &reg);
	if (en && (reg & MAX77705_WCIN_OK)) {
		pr_info("%s: WCIN is already valid by wireless charging, then skip UNO Control\n",
			__func__);
		return;
	}

	if (en == SEC_BAT_CHG_MODE_UNO_ONLY) {
		charger->uno_on = true;
		charger->cnfg00_mode = MAX77705_MODE_8_BOOST_UNO_ON;
		max77705_update_reg(charger->i2c, MAX77705_CHG_REG_CNFG_00,
				charger->cnfg00_mode, CHG_CNFG_00_MODE_MASK);
	} else if (en == SEC_BAT_CHG_MODE_CHARGING_OFF) {
		charger->uno_on = true;
		/* UNO on, boost on */
		max77705_chg_set_mode_state(charger, SEC_BAT_CHG_MODE_UNO_ON);
	} else {
		charger->uno_on = false;
		/* boost off */
		max77705_chg_set_mode_state(charger, SEC_BAT_CHG_MODE_UNO_OFF);
		msleep(50);
	}
	max77705_read_reg(charger->i2c, MAX77705_CHG_REG_INT_MASK, &chg_int_state);
	max77705_read_reg(charger->i2c, MAX77705_CHG_REG_CNFG_00, &reg);
	pr_info("%s: UNO(%d), INT_MASK(0x%x), CHG_CNFG_00(0x%x)\n",
		__func__, charger->uno_on, chg_int_state, reg);
}

__visible_for_testing void max77705_set_uno_iout(struct max77705_charger_data *charger, int iout)
{
	u8 reg = 0;

	if (iout < 300)
		reg = MAX77705_UNOILIM_200;
	else if (iout >= 300 && iout < 400)
		reg = MAX77705_UNOILIM_300;
	else if (iout >= 400 && iout < 600)
		reg = MAX77705_UNOILIM_400;
	else if (iout >= 600 && iout < 800)
		reg = MAX77705_UNOILIM_600;
	else if (iout >= 800 && iout < 1000)
		reg = MAX77705_UNOILIM_800;
	else if (iout >= 1000 && iout < 1500)
		reg = MAX77705_UNOILIM_1000;
	else if (iout >= 1500)
		reg = MAX77705_UNOILIM_1500;

	if (reg)
		max77705_update_reg(charger->i2c, MAX77705_CHG_REG_CNFG_05,
				reg << CHG_CNFG_05_REG_UNOILIM_SHIFT,
				CHG_CNFG_05_REG_UNOILIM_MASK);

	max77705_read_reg(charger->i2c, MAX77705_CHG_REG_CNFG_05, &reg);
	pr_info("@Tx_mode %s: CNFG_05 (0x%x)\n", __func__, reg);
}

__visible_for_testing void max77705_set_uno_vout(struct max77705_charger_data *charger, int vout)
{
	u8 reg = 0;

	if (vout == WC_TX_VOUT_OFF) {
		pr_info("%s: set UNO default\n", __func__);
	} else {
		if (vout < WC_TX_VOUT_MIN) {
			pr_err("%s: vout(%d) is lower than min\n", __func__, vout);
			vout = WC_TX_VOUT_MIN;
		} else if (vout > WC_TX_VOUT_MAX) {
			pr_err("%s: vout(%d) is higher than max\n", __func__, vout);
			vout = WC_TX_VOUT_MAX;
		}
		/* Set TX Vout(VBYPSET) */
		if (vout <= 5000)
			reg = 0;
		else
			reg = (vout - 5000) / 100;
		pr_info("%s: UNO VOUT (0x%x)\n", __func__, reg);
	}
	max77705_update_reg(charger->i2c, MAX77705_CHG_REG_CNFG_11,
				reg, CHG_CNFG_11_VBYPSET_MASK);

	max77705_read_reg(charger->i2c, MAX77705_CHG_REG_CNFG_11, &reg);
	pr_info("@Tx_mode %s: CNFG_11(0x%x)\n", __func__, reg);
}

static int max77705_chg_get_property(struct power_supply *psy,
					enum power_supply_property psp,
					union power_supply_propval *val)
{
	struct max77705_charger_data *charger = power_supply_get_drvdata(psy);
	enum power_supply_ext_property ext_psp = (enum power_supply_ext_property) psp;
	u8 reg_data;

	switch ((int)psp) {
	case POWER_SUPPLY_PROP_ONLINE:
		val->intval = SEC_BATTERY_CABLE_NONE;
		if (!max77705_read_reg(charger->i2c,
				      MAX77705_CHG_REG_INT_OK, &reg_data)) {
			if (reg_data & MAX77705_WCIN_OK) {
				val->intval = SEC_BATTERY_CABLE_WIRELESS;
			} else if (reg_data & MAX77705_CHGIN_OK) {
				val->intval = SEC_BATTERY_CABLE_TA;
			}
		}
		break;
	case POWER_SUPPLY_PROP_PRESENT:
		val->intval = max77705_check_battery(charger);
		break;
	case POWER_SUPPLY_PROP_STATUS:
		val->intval = max77705_get_charger_state(charger);
		break;
	case POWER_SUPPLY_PROP_CHARGE_TYPE:
		if (!charger->is_charging) {
			val->intval = POWER_SUPPLY_CHARGE_TYPE_NONE;
		} else if (charger->slow_charging) {
			val->intval = POWER_SUPPLY_CHARGE_TYPE_TRICKLE;
			pr_info("%s: slow-charging mode\n", __func__);
		} else {
			val->intval = POWER_SUPPLY_CHARGE_TYPE_FAST;
		}
		break;
	case POWER_SUPPLY_PROP_HEALTH:
		val->intval = max77705_get_charging_health(charger);
		max77705_check_cnfg12_reg(charger);
		break;
	case POWER_SUPPLY_PROP_INPUT_CURRENT_LIMIT:
		val->intval = charger->input_current;
		break;
	case POWER_SUPPLY_PROP_CONSTANT_CHARGE_CURRENT:
		val->intval = charger->charging_current;
		break;
	case POWER_SUPPLY_PROP_CONSTANT_CHARGE_VOLTAGE:
		val->intval = max77705_get_float_voltage(charger);
		break;
	case POWER_SUPPLY_PROP_CHARGE_NOW:
		max77705_read_reg(charger->i2c,
				  MAX77705_CHG_REG_DETAILS_01, &reg_data);
		reg_data &= 0x0F;
		switch (reg_data) {
		case 0x01:
			val->strval = "CC Mode";
			break;
		case 0x02:
			val->strval = "CV Mode";
			break;
		case 0x03:
			val->strval = "EOC";
			break;
		case 0x04:
			val->strval = "DONE";
			break;
		default:
			val->strval = "NONE";
			break;
		}
		break;
	case POWER_SUPPLY_EXT_PROP_MIN ... POWER_SUPPLY_EXT_PROP_MAX:
		switch (ext_psp) {
		case POWER_SUPPLY_EXT_PROP_WDT_STATUS:
			if (max77705_chg_get_wdtmr_status(charger)) {
				dev_info(charger->dev, "charger WDT is expired!!\n");
				max77705_test_read(charger);
			}
			break;
		case POWER_SUPPLY_EXT_PROP_CHIP_ID:
			if (!max77705_read_reg(charger->i2c,
					MAX77705_PMIC_REG_PMICREV, &reg_data)) {
				/* pmic_ver should below 0x7 */
				val->intval =
					(charger->pmic_ver >= 0x1 && charger->pmic_ver <= 0x7);
				pr_info("%s : IF PMIC ver.0x%x\n", __func__,
					charger->pmic_ver);
			} else {
				val->intval = 0;
				pr_info("%s : IF PMIC I2C fail.\n", __func__);
			}
			break;
		case POWER_SUPPLY_EXT_PROP_MONITOR_WORK:
			max77705_test_read(charger);
			max77705_chg_monitor_work(charger);
			break;
		case POWER_SUPPLY_EXT_PROP_CHARGE_BOOST:
			max77705_read_reg(charger->i2c, MAX77705_CHG_REG_CNFG_00, &reg_data);
			reg_data &= CHG_CNFG_00_MODE_MASK;
			val->intval = (reg_data & MAX77705_MODE_BOOST) ? 1 : 0;
			break;
#if defined(CONFIG_AFC_CHARGER_MODE)
		case POWER_SUPPLY_EXT_PROP_AFC_CHARGER_MODE:
			return -ENODATA;
#endif
		case POWER_SUPPLY_EXT_PROP_CHARGE_OTG_CONTROL:
			mutex_lock(&charger->charger_mutex);
			val->intval = charger->otg_on;
			mutex_unlock(&charger->charger_mutex);
			break;
		case POWER_SUPPLY_EXT_PROP_CHARGE_COUNTER_SHADOW:
			break;
		case POWER_SUPPLY_EXT_PROP_CHARGING_ENABLED:
			val->intval = charger->charge_mode;
			break;
		case POWER_SUPPLY_EXT_PROP_SHIPMODE_TEST:
#if defined(CONFIG_SUPPORT_SHIP_MODE)
			if (charger->pdata->disable_ship_mode) {
				val->intval = 0;
				pr_info("%s: disable_ship_mode(%d), ship mode is not supported\n",
						__func__, charger->pdata->disable_ship_mode);
			} else {
				val->intval = max77705_get_fw_ship_mode();
				pr_info("%s: ship mode op is %d\n", __func__, val->intval);
			}
#else
			val->intval = 0;
			pr_info("%s: ship mode is not supported\n", __func__);
#endif
			break;
		case POWER_SUPPLY_EXT_PROP_INPUT_CURRENT_LIMIT_WRL:
			val->intval = max77705_get_input_current_type(charger, SEC_BATTERY_CABLE_WIRELESS);
			break;
		default:
			return -EINVAL;
		}
		break;
	default:
		return -EINVAL;
	}
	return 0;
}

__visible_for_testing void max77705_chg_set_mode_state(struct max77705_charger_data *charger,
					unsigned int state)
{
	u8 reg;

	if (state == SEC_BAT_CHG_MODE_CHARGING)
		charger->is_charging = true;
	else if (state == SEC_BAT_CHG_MODE_CHARGING_OFF ||
			state == SEC_BAT_CHG_MODE_BUCK_OFF)
		charger->is_charging = false;

#if defined(CONFIG_SEC_FACTORY)
	if (max77705_get_facmode()) {
		if (state == SEC_BAT_CHG_MODE_CHARGING ||
			state == SEC_BAT_CHG_MODE_CHARGING_OFF ||
			state == SEC_BAT_CHG_MODE_BUCK_OFF) {
			pr_info("%s: Factory Mode Skip set charger state\n", __func__);
			return;
		}
	}
#endif

	mutex_lock(&charger->mode_mutex);
	pr_info("%s: current_mode(0x%x), state(%d)\n", __func__, charger->cnfg00_mode, state);
	switch (charger->cnfg00_mode) {
		/* all off */
	case MAX77705_MODE_0_ALL_OFF:
		if (state == SEC_BAT_CHG_MODE_CHARGING_OFF)
			charger->cnfg00_mode = MAX77705_MODE_4_BUCK_ON;
		else if (state == SEC_BAT_CHG_MODE_CHARGING)
			charger->cnfg00_mode = MAX77705_MODE_5_BUCK_CHG_ON;
		else if (state == SEC_BAT_CHG_MODE_OTG_ON)
			charger->cnfg00_mode = MAX77705_MODE_A_BOOST_OTG_ON;
		else if (state == SEC_BAT_CHG_MODE_UNO_ON)
			charger->cnfg00_mode = MAX77705_MODE_8_BOOST_UNO_ON;
		break;
		/* buck only */
	case MAX77705_MODE_4_BUCK_ON:
		if (state == SEC_BAT_CHG_MODE_CHARGING)
			charger->cnfg00_mode = MAX77705_MODE_5_BUCK_CHG_ON;
		else if (state == SEC_BAT_CHG_MODE_BUCK_OFF)
			charger->cnfg00_mode = MAX77705_MODE_0_ALL_OFF;
		else if (state == SEC_BAT_CHG_MODE_OTG_ON)
			charger->cnfg00_mode = MAX77705_MODE_E_BUCK_BOOST_OTG_ON;
		else if (state == SEC_BAT_CHG_MODE_UNO_ON)
			charger->cnfg00_mode = MAX77705_MODE_C_BUCK_BOOST_UNO_ON;
		break;
		/* buck, charger on */
	case MAX77705_MODE_5_BUCK_CHG_ON:
		if (state == SEC_BAT_CHG_MODE_BUCK_OFF)
			charger->cnfg00_mode = MAX77705_MODE_0_ALL_OFF;
		else if (state == SEC_BAT_CHG_MODE_CHARGING_OFF)
			charger->cnfg00_mode = MAX77705_MODE_4_BUCK_ON;
		else if (state == SEC_BAT_CHG_MODE_OTG_ON)
			charger->cnfg00_mode = MAX77705_MODE_F_BUCK_CHG_BOOST_OTG_ON;
		else if (state == SEC_BAT_CHG_MODE_UNO_ON)
			charger->cnfg00_mode = MAX77705_MODE_D_BUCK_CHG_BOOST_UNO_ON;
		break;
	case MAX77705_MODE_8_BOOST_UNO_ON:
		if (state == SEC_BAT_CHG_MODE_CHARGING_OFF)
			charger->cnfg00_mode = MAX77705_MODE_C_BUCK_BOOST_UNO_ON;
		else if (state == SEC_BAT_CHG_MODE_CHARGING)
			charger->cnfg00_mode = MAX77705_MODE_D_BUCK_CHG_BOOST_UNO_ON;
		else if (state == SEC_BAT_CHG_MODE_UNO_OFF)
			charger->cnfg00_mode = MAX77705_MODE_0_ALL_OFF;
		/* UNO -> OTG */
		else if (state == SEC_BAT_CHG_MODE_OTG_ON) {
			max77705_update_reg(charger->i2c, MAX77705_CHG_REG_CNFG_00,
			    MAX77705_MODE_4_BUCK_ON, CHG_CNFG_00_MODE_MASK);
			usleep_range(1000, 2000);
			/* mode 0x4, and 1msec delay, and then otg on */
			charger->cnfg00_mode = MAX77705_MODE_A_BOOST_OTG_ON;
		}
		break;
	case MAX77705_MODE_A_BOOST_OTG_ON:
		if (state == SEC_BAT_CHG_MODE_CHARGING_OFF)
			charger->cnfg00_mode = MAX77705_MODE_E_BUCK_BOOST_OTG_ON;
		else if (state == SEC_BAT_CHG_MODE_CHARGING)
			charger->cnfg00_mode = MAX77705_MODE_F_BUCK_CHG_BOOST_OTG_ON;
		else if (state == SEC_BAT_CHG_MODE_OTG_OFF)
			charger->cnfg00_mode = MAX77705_MODE_0_ALL_OFF;
		/* OTG -> UNO */
		else if (state == SEC_BAT_CHG_MODE_UNO_ON) {
			max77705_update_reg(charger->i2c, MAX77705_CHG_REG_CNFG_00,
			    MAX77705_MODE_4_BUCK_ON, CHG_CNFG_00_MODE_MASK);
			usleep_range(1000, 2000);
			/* mode 0x4, and 1msec delay, and then uno on */
			charger->cnfg00_mode = MAX77705_MODE_8_BOOST_UNO_ON;
		}
		break;
	case MAX77705_MODE_C_BUCK_BOOST_UNO_ON:
		if (state == SEC_BAT_CHG_MODE_BUCK_OFF)
			charger->cnfg00_mode = MAX77705_MODE_8_BOOST_UNO_ON;
		else if (state == SEC_BAT_CHG_MODE_CHARGING)
			charger->cnfg00_mode = MAX77705_MODE_D_BUCK_CHG_BOOST_UNO_ON;
		else if (state == SEC_BAT_CHG_MODE_UNO_OFF)
			charger->cnfg00_mode = MAX77705_MODE_4_BUCK_ON;
		/* UNO -> OTG */
		else if (state == SEC_BAT_CHG_MODE_OTG_ON) {
			max77705_update_reg(charger->i2c, MAX77705_CHG_REG_CNFG_00,
			    MAX77705_MODE_4_BUCK_ON, CHG_CNFG_00_MODE_MASK);
			usleep_range(1000, 2000);
			/* mode 0x4, and 1msec delay, and then otg on */
			charger->cnfg00_mode = MAX77705_MODE_E_BUCK_BOOST_OTG_ON;
		}
		break;
	case MAX77705_MODE_D_BUCK_CHG_BOOST_UNO_ON:
		if (state == SEC_BAT_CHG_MODE_BUCK_OFF)
			charger->cnfg00_mode = MAX77705_MODE_8_BOOST_UNO_ON;
		else if (state == SEC_BAT_CHG_MODE_CHARGING_OFF)
			charger->cnfg00_mode = MAX77705_MODE_C_BUCK_BOOST_UNO_ON;
		else if (state == SEC_BAT_CHG_MODE_UNO_OFF)
			charger->cnfg00_mode = MAX77705_MODE_5_BUCK_CHG_ON;
		/* UNO -> OTG */
		else if (state == SEC_BAT_CHG_MODE_OTG_ON) {
			max77705_update_reg(charger->i2c, MAX77705_CHG_REG_CNFG_00,
			    MAX77705_MODE_4_BUCK_ON, CHG_CNFG_00_MODE_MASK);
			usleep_range(1000, 2000);
			/* mode 0x4, and 1msec delay, and then otg on */
			charger->cnfg00_mode = MAX77705_MODE_E_BUCK_BOOST_OTG_ON;
		}
		break;
	case MAX77705_MODE_E_BUCK_BOOST_OTG_ON:
		if (state == SEC_BAT_CHG_MODE_BUCK_OFF)
			charger->cnfg00_mode = MAX77705_MODE_A_BOOST_OTG_ON;
		else if (state == SEC_BAT_CHG_MODE_CHARGING)
			charger->cnfg00_mode = MAX77705_MODE_F_BUCK_CHG_BOOST_OTG_ON;
		else if (state == SEC_BAT_CHG_MODE_OTG_OFF)
			charger->cnfg00_mode = MAX77705_MODE_4_BUCK_ON;
		/* OTG -> UNO */
		else if (state == SEC_BAT_CHG_MODE_UNO_ON) {
			max77705_update_reg(charger->i2c, MAX77705_CHG_REG_CNFG_00,
			    MAX77705_MODE_4_BUCK_ON, CHG_CNFG_00_MODE_MASK);
			usleep_range(1000, 2000);
			/* mode 0x4, and 1msec delay, and then uno on */
			charger->cnfg00_mode = MAX77705_MODE_C_BUCK_BOOST_UNO_ON;
		}
		break;
	case MAX77705_MODE_F_BUCK_CHG_BOOST_OTG_ON:
		if (state == SEC_BAT_CHG_MODE_CHARGING_OFF)
			charger->cnfg00_mode = MAX77705_MODE_E_BUCK_BOOST_OTG_ON;
		else if (state == SEC_BAT_CHG_MODE_BUCK_OFF)
			charger->cnfg00_mode = MAX77705_MODE_A_BOOST_OTG_ON;
		else if (state == SEC_BAT_CHG_MODE_OTG_OFF)
			charger->cnfg00_mode = MAX77705_MODE_5_BUCK_CHG_ON;
		break;
	}

	if (state == SEC_BAT_CHG_MODE_OTG_ON &&
		charger->cnfg00_mode == MAX77705_MODE_F_BUCK_CHG_BOOST_OTG_ON) {
		/* W/A for shutdown problem when turn on OTG during wireless charging */
		pr_info("%s : disable WCIN_SEL before change mode to 0xF\n", __func__);
		max77705_update_reg(charger->i2c, MAX77705_CHG_REG_CNFG_12,
			(0 << CHG_CNFG_12_WCINSEL_SHIFT), CHG_CNFG_12_WCINSEL_MASK);
	}

	pr_info("%s: current_mode(0x%x)\n", __func__, charger->cnfg00_mode);
	max77705_update_reg(charger->i2c, MAX77705_CHG_REG_CNFG_00,
			charger->cnfg00_mode, CHG_CNFG_00_MODE_MASK);

	if (state == SEC_BAT_CHG_MODE_OTG_ON &&
		charger->cnfg00_mode == MAX77705_MODE_F_BUCK_CHG_BOOST_OTG_ON) {
		/* W/A for shutdown problem when turn on OTG during wireless charging */
		pr_info("%s : enable WCIN_SEL after change mode to 0xF\n", __func__);
		max77705_update_reg(charger->i2c, MAX77705_CHG_REG_CNFG_12,
			MAX77705_CHG_WCINSEL, CHG_CNFG_12_WCINSEL_MASK);
	}

	max77705_read_reg(charger->i2c, MAX77705_CHG_REG_CNFG_00, &reg);
	pr_info("%s: CNFG_00 (0x%x)\n", __func__, reg);
	mutex_unlock(&charger->mode_mutex);
}

#if defined(CONFIG_UPDATE_BATTERY_DATA)
static int max77705_charger_parse_dt(struct max77705_charger_data *charger);
#endif
static int max77705_chg_set_property(struct power_supply *psy,
					enum power_supply_property psp,
					const union power_supply_propval *val)
{
	struct max77705_charger_data *charger = power_supply_get_drvdata(psy);
	enum power_supply_ext_property ext_psp = (enum power_supply_ext_property) psp;
	u8 reg_data = 0;

	/* check unlock status before does set the register */
	max77705_charger_unlock(charger);
	switch ((int)psp) {
	/* val->intval : type */
	case POWER_SUPPLY_PROP_STATUS:
		charger->status = val->intval;
		break;
	case POWER_SUPPLY_PROP_ONLINE:
		charger->cable_type = val->intval;
		charger->aicl_curr = 0;
		charger->slow_charging = false;
		charger->input_current = max77705_get_input_current(charger);
		max77705_change_charge_path(charger, charger->cable_type);
		if (!max77705_get_autoibus(charger))
			max77705_set_fw_noautoibus(MAX77705_AUTOIBUS_AT_OFF);

		if (is_nocharge_type(charger->cable_type)) {
			charger->wc_pre_current = WC_CURRENT_START;
			/* VCHGIN : REG=4.5V, UVLO=4.7V */
			max77705_update_reg(charger->i2c, MAX77705_CHG_REG_CNFG_12,
			    0x00, CHG_CNFG_12_VCHGIN_REG_MASK);
			if (charger->pdata->enable_sysovlo_irq)
				max77705_set_sysovlo(charger, 1);

			/* Enable AICL IRQ */
			if (charger->irq_aicl_enabled == 0) {
				charger->irq_aicl_enabled = 1;
				enable_irq(charger->irq_aicl);
				max77705_read_reg(charger->i2c,
					MAX77705_CHG_REG_INT_MASK, &reg_data);
				pr_info("%s: enable aicl : 0x%x\n",
					__func__, reg_data);
			}
		} else if (is_wired_type(charger->cable_type)) {
			/* VCHGIN : REG=4.6V, UVLO=4.8V
			 * to fix CHGIN-UVLO issues including cheapy battery packs
			 */
			max77705_update_reg(charger->i2c, MAX77705_CHG_REG_CNFG_12,
				0x08, CHG_CNFG_12_VCHGIN_REG_MASK);
			if (is_hv_wire_type(charger->cable_type) ||
				(charger->cable_type == SEC_BATTERY_CABLE_HV_TA_CHG_LIMIT)) {
				/* Disable AICL IRQ */
				if (charger->irq_aicl_enabled == 1) {
					charger->irq_aicl_enabled = 0;
					disable_irq_nosync(charger->irq_aicl);
					cancel_delayed_work(&charger->aicl_work);
					__pm_relax(charger->aicl_ws);
					max77705_read_reg(charger->i2c,
						MAX77705_CHG_REG_INT_MASK, &reg_data);
					pr_info("%s: disable aicl : 0x%x\n",
						__func__, reg_data);
					charger->aicl_curr = 0;
					charger->slow_charging = false;
				}
			}
		}
		break;
		/* val->intval : input charging current */
	case POWER_SUPPLY_PROP_INPUT_CURRENT_LIMIT:
		{
			int input_current = val->intval;

			if (delayed_work_pending(&charger->aicl_work)) {
				cancel_delayed_work(&charger->aicl_work);
				charger->aicl_curr = 0;
				queue_delayed_work(charger->wqueue, &charger->aicl_work,
					msecs_to_jiffies(AICL_WORK_DELAY));
			}

			if (is_wireless_type(charger->cable_type))
				max77705_set_wireless_input_current(charger, input_current);
			else
				max77705_set_input_current(charger, input_current);

			if (is_nocharge_type(charger->cable_type))
				max77705_set_wireless_input_current(charger, input_current);
			charger->input_current = input_current;
		}
		break;
		/* val->intval : charging current */
	case POWER_SUPPLY_PROP_CONSTANT_CHARGE_CURRENT:
		charger->charging_current = val->intval;
		max77705_set_charge_current(charger, val->intval);
		break;
	case POWER_SUPPLY_PROP_CONSTANT_CHARGE_VOLTAGE:
		charger->float_voltage = val->intval;
		pr_info("%s: float voltage(%d)\n", __func__, val->intval);
		max77705_set_float_voltage(charger, val->intval);
		break;
	case POWER_SUPPLY_PROP_CHARGE_CONTROL_LIMIT_MAX:
		max77705_enable_aicl_irq(charger);
		max77705_read_reg(charger->i2c, MAX77705_CHG_REG_INT_OK, &reg_data);
		if (!(reg_data & MAX77705_AICL_OK))
			queue_delayed_work(charger->wqueue, &charger->aicl_work,
					   msecs_to_jiffies(AICL_WORK_DELAY));
		break;
	case POWER_SUPPLY_PROP_CHARGE_TERM_CURRENT:
		max77705_set_topoff_current(charger, val->intval);
		break;
	case POWER_SUPPLY_EXT_PROP_MIN ... POWER_SUPPLY_EXT_PROP_MAX:
		switch (ext_psp) {
		case POWER_SUPPLY_EXT_PROP_SURGE:
			if (val->intval) {
				pr_info("%s : Charger IC reset by surge. charger re-initialize\n",
					__func__);
				check_charger_unlock_state(charger);
			}
			break;
		case POWER_SUPPLY_EXT_PROP_CHGINSEL:
			max77705_change_charge_path(charger, charger->cable_type);
			break;
		case POWER_SUPPLY_EXT_PROP_PAD_VOLT_CTRL:
			__pm_relax(charger->wc_current_ws);
			cancel_delayed_work(&charger->wc_current_work);
			max77705_set_input_current(charger, val->intval);
			break;
		case POWER_SUPPLY_EXT_PROP_SHIPMODE_TEST:
#if defined(CONFIG_SUPPORT_SHIP_MODE)
			if (charger->pdata->disable_ship_mode) {
				pr_info("%s: disable_ship_mode(%d), ship mode(%d) is not supported\n",
						__func__, charger->pdata->disable_ship_mode,  val->intval);
			} else {
				if (val->intval == SHIP_MODE_EN) {
					pr_info("%s: set ship mode enable\n", __func__);
					max77705_set_ship_mode(charger, 1);
				} else if (val->intval == SHIP_MODE_EN_OP) {
					pr_info("%s: set ship mode op enable\n", __func__);
					max77705_set_fw_ship_mode(1);
				} else {
					pr_info("%s: ship mode disable is not supported\n", __func__);
				}
			}
#else
			pr_info("%s: ship mode(%d) is not supported\n", __func__, val->intval);
#endif
			break;
		case POWER_SUPPLY_EXT_PROP_AUTO_SHIPMODE_CONTROL:
			if (val->intval) {
				pr_info("%s: auto ship mode is enabled\n", __func__);
				max77705_set_auto_ship_mode(charger, 1);
			} else {
				pr_info("%s: auto ship mode is disabled\n", __func__);
				max77705_set_auto_ship_mode(charger, 0);
			}
			break;
		case POWER_SUPPLY_EXT_PROP_FGSRC_SWITCHING:
			{
				u8 reg_data = 0, reg_fgsrc = 0;

				/* if jig attached, change the power source */
				/* from the VBATFG to the internal VSYS */
				if ((val->intval == SEC_BAT_INBAT_FGSRC_SWITCHING_VSYS) ||
					(val->intval == SEC_BAT_FGSRC_SWITCHING_VSYS))
					reg_fgsrc = 1;
				else
					reg_fgsrc = 0;

				max77705_update_reg(charger->i2c,
					MAX77705_CHG_REG_CNFG_07,
					(reg_fgsrc << CHG_CNFG_07_REG_FGSRC_SHIFT),
					CHG_CNFG_07_REG_FGSRC_MASK);
				max77705_read_reg(charger->i2c,
					MAX77705_CHG_REG_CNFG_07, &reg_data);

				pr_info("%s: POWER_SUPPLY_EXT_PROP_FGSRC_SWITCHING(%d): reg(0x%x) val(0x%x)\n",
					__func__, reg_fgsrc, MAX77705_CHG_REG_CNFG_07, reg_data);
			}
			break;
		case POWER_SUPPLY_EXT_PROP_CHARGING_ENABLED:
			charger->charge_mode = val->intval;
			charger->misalign_cnt = 0;
			max77705_chg_set_mode_state(charger, charger->charge_mode);
			break;
#if defined(CONFIG_AFC_CHARGER_MODE)
		case POWER_SUPPLY_EXT_PROP_AFC_CHARGER_MODE:
			muic_hv_charger_init();
			break;
#endif
		case POWER_SUPPLY_EXT_PROP_CHARGE_OTG_CONTROL:
			max77705_set_otg(charger, val->intval);
			break;
#if defined(CONFIG_UPDATE_BATTERY_DATA)
		case POWER_SUPPLY_EXT_PROP_POWER_DESIGN:
			max77705_charger_parse_dt(charger);
			break;
#endif
		case POWER_SUPPLY_EXT_PROP_CONSTANT_CHARGE_CURRENT_WRL:
			charger->charging_current = val->intval;
			__pm_stay_awake(charger->wc_chg_current_ws);
			queue_delayed_work(charger->wqueue, &charger->wc_chg_current_work,
				msecs_to_jiffies(3000));
			break;
		default:
			return -EINVAL;
		}
		break;
	default:
		return -EINVAL;
	}
	return 0;
}

static int max77705_otg_get_property(struct power_supply *psy,
				     enum power_supply_property psp,
				     union power_supply_propval *val)
{
	struct max77705_charger_data *charger = power_supply_get_drvdata(psy);
	enum power_supply_ext_property ext_psp = (enum power_supply_ext_property) psp;
	u8 reg_data;

	switch ((int)psp) {
	case POWER_SUPPLY_PROP_ONLINE:
		mutex_lock(&charger->charger_mutex);
		val->intval = charger->otg_on;
		mutex_unlock(&charger->charger_mutex);
		break;
	case POWER_SUPPLY_PROP_VOLTAGE_MAX:
		break;
	case POWER_SUPPLY_EXT_PROP_MIN ... POWER_SUPPLY_EXT_PROP_MAX:
		switch (ext_psp) {
		case POWER_SUPPLY_EXT_PROP_CHARGE_UNO_CONTROL:
			max77705_read_reg(charger->i2c, MAX77705_CHG_REG_CNFG_00,
					  &reg_data);
			reg_data &= CHG_CNFG_00_MODE_MASK;
			if (reg_data == MAX77705_MODE_8_BOOST_UNO_ON ||
				reg_data == MAX77705_MODE_C_BUCK_BOOST_UNO_ON ||
				reg_data == MAX77705_MODE_D_BUCK_CHG_BOOST_UNO_ON)
				val->intval = 1;
			else
				val->intval = 0;
			break;
		default:
			return -EINVAL;
		}
		break;
	default:
		return -EINVAL;
	}
	return 0;
}

static int max77705_otg_set_property(struct power_supply *psy,
				     enum power_supply_property psp,
				     const union power_supply_propval *val)
{
	struct max77705_charger_data *charger = power_supply_get_drvdata(psy);
	enum power_supply_ext_property ext_psp = (enum power_supply_ext_property) psp;
	u8 reg_data;
	bool mfc_fw_update = false;
	union power_supply_propval value = {0, };

	switch ((int)psp) {
	case POWER_SUPPLY_PROP_ONLINE:
		psy_do_property("battery", get,
			POWER_SUPPLY_EXT_PROP_MFC_FW_UPDATE, value);
		mfc_fw_update = value.intval;
		if (!mfc_fw_update) {
			max77705_set_otg(charger, val->intval);
			if (val->intval && (charger->irq_aicl_enabled == 1)) {
				charger->irq_aicl_enabled = 0;
				disable_irq_nosync(charger->irq_aicl);
				cancel_delayed_work(&charger->aicl_work);
				__pm_relax(charger->aicl_ws);
				max77705_read_reg(charger->i2c,
					MAX77705_CHG_REG_INT_MASK, &reg_data);
				pr_info("%s : disable aicl : 0x%x\n",
					__func__, reg_data);
				charger->aicl_curr = 0;
				charger->slow_charging = false;
			} else if (!val->intval && (charger->irq_aicl_enabled == 0)) {
				charger->irq_aicl_enabled = 1;
				enable_irq(charger->irq_aicl);
				max77705_read_reg(charger->i2c,
					MAX77705_CHG_REG_INT_MASK, &reg_data);
				pr_info("%s : enable aicl : 0x%x\n",
					__func__, reg_data);
			}
		} else {
			pr_info("%s : max77705_set_otg skip, mfc_fw_update(%d)\n",
				__func__, mfc_fw_update);
		}
		break;
	case POWER_SUPPLY_PROP_VOLTAGE_MAX:
		pr_info("POWER_SUPPLY_PROP_VOLTAGE_MAX, set otg current limit %dmA\n", (val->intval) ? 1500 : 900);

		if (val->intval) {
			/* otg current limit 1500mA */
			max77705_update_reg(charger->i2c, MAX77705_CHG_REG_CNFG_02,
					MAX77705_OTG_ILIM_1500 << CHG_CNFG_02_OTG_ILIM_SHIFT,
					CHG_CNFG_02_OTG_ILIM_MASK);
		} else {
			/* otg current limit 900mA */
			max77705_update_reg(charger->i2c, MAX77705_CHG_REG_CNFG_02,
					MAX77705_OTG_ILIM_900 << CHG_CNFG_02_OTG_ILIM_SHIFT,
					CHG_CNFG_02_OTG_ILIM_MASK);
		}
		break;
	case POWER_SUPPLY_EXT_PROP_MIN ... POWER_SUPPLY_EXT_PROP_MAX:
		switch (ext_psp) {
		case POWER_SUPPLY_EXT_PROP_WIRELESS_TX_VOUT:
			max77705_set_uno_vout(charger, val->intval);
			break;
		case POWER_SUPPLY_EXT_PROP_WIRELESS_TX_IOUT:
			max77705_set_uno_iout(charger, val->intval);
			break;
		case POWER_SUPPLY_EXT_PROP_CHARGE_UNO_CONTROL:
			pr_info("%s: WCIN-UNO %d\n", __func__, val->intval);
			max77705_set_uno(charger, val->intval);
			break;
		default:
			return -EINVAL;
		}
		break;
	default:
		return -EINVAL;
	}
	return 0;
}

static int max77705_debugfs_show(struct seq_file *s, void *data)
{
	struct max77705_charger_data *charger = s->private;
	u8 reg, reg_data;

	seq_puts(s, "MAX77705 CHARGER IC :\n");
	seq_puts(s, "===================\n");
	for (reg = 0xB0; reg <= 0xC3; reg++) {
		max77705_read_reg(charger->i2c, reg, &reg_data);
		seq_printf(s, "0x%02x:\t0x%02x\n", reg, reg_data);
	}

	seq_puts(s, "\n");
	return 0;
}

static int max77705_debugfs_open(struct inode *inode, struct file *file)
{
	return single_open(file, max77705_debugfs_show, inode->i_private);
}

static const struct file_operations max77705_debugfs_fops = {
	.open = max77705_debugfs_open,
	.read = seq_read,
	.llseek = seq_lseek,
	.release = single_release,
};

static void max77705_chg_isr_work(struct work_struct *work)
{
	struct max77705_charger_data *charger =
	    container_of(work, struct max77705_charger_data, isr_work.work);

	max77705_get_charger_state(charger);
	max77705_get_charging_health(charger);
}

static irqreturn_t max77705_chg_irq_thread(int irq, void *irq_data)
{
	struct max77705_charger_data *charger = irq_data;

	pr_info("%s: Charger interrupt occurred\n", __func__);

	if ((charger->pdata->full_check_type == SEC_BATTERY_FULLCHARGED_CHGINT)
		|| (charger->pdata->ovp_uvlo_check_type == SEC_BATTERY_OVP_UVLO_CHGINT))
		schedule_delayed_work(&charger->isr_work, 0);

	return IRQ_HANDLED;
}

static irqreturn_t max77705_batp_irq(int irq, void *data)
{
	struct max77705_charger_data *charger = data;
	union power_supply_propval value;
	u8 reg_data;

	pr_info("%s : irq(%d)\n", __func__, irq);

	check_charger_unlock_state(charger);
	max77705_read_reg(charger->i2c, MAX77705_CHG_REG_INT_OK, &reg_data);

	if (!(reg_data & MAX77705_BATP_OK))
		psy_do_property("battery", set, POWER_SUPPLY_PROP_PRESENT, value);

	return IRQ_HANDLED;
}

#if defined(CONFIG_MAX77705_CHECK_B2SOVRC)
#if defined(CONFIG_REGULATOR_S2MPS18)
extern void s2mps18_print_adc_val_power(void);
#endif
static irqreturn_t max77705_bat_irq(int irq, void *data)
{
	struct max77705_charger_data *charger = data;
	union power_supply_propval value;
	u8 reg_int_ok, reg_data;

	max77705_read_reg(charger->i2c, MAX77705_CHG_REG_INT_OK, &reg_int_ok);
	if (!(reg_int_ok & MAX77705_BAT_OK)) {
		max77705_read_reg(charger->i2c, MAX77705_CHG_REG_DETAILS_01, &reg_data);
		reg_data = ((reg_data & MAX77705_BAT_DTLS) >> MAX77705_BAT_DTLS_SHIFT);
		if (reg_data == 0x06) {
			pr_info("OCP(B2SOVRC)\n");

			if (charger->uno_on) {
#if defined(CONFIG_WIRELESS_TX_MODE)
				union power_supply_propval val;
				val.intval = BATT_TX_EVENT_WIRELESS_TX_OCP;
				psy_do_property("wireless", set,
					POWER_SUPPLY_EXT_PROP_WIRELESS_TX_ERR, val);
#endif
			}
#if defined(CONFIG_REGULATOR_S2MPS18)
			s2mps18_print_adc_val_power();
#endif
			/* print vnow, inow */
			psy_do_property("max77705-fuelgauge", get,
				POWER_SUPPLY_EXT_PROP_MONITOR_WORK, value);
		}
	} else {
		max77705_read_reg(charger->i2c, MAX77705_CHG_REG_DETAILS_01, &reg_data);
		reg_data = ((reg_data & MAX77705_BAT_DTLS) >> MAX77705_BAT_DTLS_SHIFT);
		pr_info("%s: reg_data(0x%x)\n", __func__, reg_data);
	}
	check_charger_unlock_state(charger);

	return IRQ_HANDLED;
}
#endif

static irqreturn_t max77705_bypass_irq(int irq, void *data)
{
	struct max77705_charger_data *charger = data;
#ifdef CONFIG_USB_HOST_NOTIFY
	struct otg_notify *o_notify;
#endif
	union power_supply_propval val;
	u8 dtls_02, byp_dtls;

	pr_info("%s: irq(%d)\n", __func__, irq);

	/* check and unlock */
	check_charger_unlock_state(charger);
	max77705_read_reg(charger->i2c, MAX77705_CHG_REG_DETAILS_02, &dtls_02);

	byp_dtls = ((dtls_02 & MAX77705_BYP_DTLS) >> MAX77705_BYP_DTLS_SHIFT);
	pr_info("%s: BYP_DTLS(0x%02x)\n", __func__, byp_dtls);

	if (byp_dtls & 0x1) {
		pr_info("%s: bypass overcurrent limit\n", __func__);
		/* disable the register values just related to OTG and
		 * keep the values about the charging
		 */
		if (charger->otg_on) {
#ifdef CONFIG_USB_HOST_NOTIFY
			o_notify = get_otg_notify();
			if (o_notify)
				send_otg_notify(o_notify, NOTIFY_EVENT_OVERCURRENT, 0);
#endif
			val.intval = 0;
			psy_do_property("otg", set, POWER_SUPPLY_PROP_ONLINE, val);
		} else if (charger->uno_on) {
#if defined(CONFIG_WIRELESS_TX_MODE)
			val.intval = BATT_TX_EVENT_WIRELESS_TX_OCP;
			psy_do_property("wireless", set, POWER_SUPPLY_EXT_PROP_WIRELESS_TX_ERR, val);
#endif
		}
	}
	return IRQ_HANDLED;
}

__visible_for_testing void max77705_aicl_isr_work(struct work_struct *work)
{
	struct max77705_charger_data *charger =
		container_of(work, struct max77705_charger_data, aicl_work.work);
	union power_supply_propval value;
	bool aicl_mode = false;
	u8 aicl_state = 0, reg_data;
	int aicl_current = 0;

	if (!charger->irq_aicl_enabled || is_nocharge_type(charger->cable_type)) {
		pr_info("%s: skip\n", __func__);
		charger->aicl_curr = 0;
		__pm_relax(charger->aicl_ws);
		return;
	}

	mutex_lock(&charger->icl_mutex);
	cancel_delayed_work(&charger->wc_current_work);
	__pm_relax(charger->wc_current_ws);
	mutex_unlock(&charger->icl_mutex);

	mutex_lock(&charger->charger_mutex);
	/* check and unlock */
	check_charger_unlock_state(charger);
	max77705_read_reg(charger->i2c, MAX77705_CHG_REG_INT_OK, &aicl_state);

	if (!(aicl_state & MAX77705_AICL_OK)) {
		/* AICL mode */
		pr_info("%s : AICL Mode : CHG_INT_OK(0x%02x)\n",
			__func__, aicl_state);

		reduce_input_current(charger, REDUCE_CURRENT_STEP);

		if (is_not_wireless_type(charger->cable_type))
			max77705_check_slow_charging(charger, charger->input_current);

		if ((charger->irq_aicl_enabled == 1) && (charger->slow_charging) &&
			(charger->input_current <= MINIMUM_INPUT_CURRENT)) {
			/* Disable AICL IRQ, no more reduce current */
			charger->irq_aicl_enabled = 0;
			disable_irq_nosync(charger->irq_aicl);
			max77705_read_reg(charger->i2c,
					MAX77705_CHG_REG_INT_MASK, &reg_data);
			pr_info("%s : disable aicl : 0x%x\n", __func__, reg_data);

			/* notify aicl current, no more aicl check */
			aicl_current = MINIMUM_INPUT_CURRENT;
		} else {
			aicl_mode = true;
			queue_delayed_work(charger->wqueue, &charger->aicl_work,
				msecs_to_jiffies(AICL_WORK_DELAY));
		}
	} else {
		/* Not in AICL mode */
		pr_info("%s : Not in AICL Mode : CHG_INT_OK(0x%02x), aicl_curr(%d)\n",
			__func__, aicl_state, charger->aicl_curr);
		if (charger->aicl_curr) {
			/* notify aicl current, if aicl is on and aicl state is cleard */
			aicl_current = charger->aicl_curr;
		}
	}

	mutex_unlock(&charger->charger_mutex);

	if (aicl_current) {
		value.intval = aicl_current;
		psy_do_property("battery", set,
			POWER_SUPPLY_EXT_PROP_AICL_CURRENT, value);
	}

	/* keep wakeup_source if this is not last work to prevent to enter suspend */
	if (!aicl_mode && !delayed_work_pending(&charger->aicl_work))
		__pm_relax(charger->aicl_ws);
}

static irqreturn_t max77705_aicl_irq(int irq, void *data)
{
	struct max77705_charger_data *charger = data;

	__pm_stay_awake(charger->aicl_ws);
	queue_delayed_work(charger->wqueue, &charger->aicl_work,
		msecs_to_jiffies(AICL_WORK_DELAY));

	pr_info("%s: irq(%d)\n", __func__, irq);

	return IRQ_HANDLED;
}

static void max77705_enable_aicl_irq(struct max77705_charger_data *charger)
{
	int ret;

	ret = request_threaded_irq(charger->irq_aicl, NULL,
					max77705_aicl_irq, 0,
					"aicl-irq", charger);
	if (ret < 0) {
		pr_err("%s: fail to request aicl IRQ: %d: %d\n",
		       __func__, charger->irq_aicl, ret);
		charger->irq_aicl_enabled = -1;
	} else {
		charger->irq_aicl_enabled = 1;
	}
	pr_info("%s: enabled(%d)\n", __func__, charger->irq_aicl_enabled);
}

__visible_for_testing void max77705_wc_current_work(struct work_struct *work)
{
	struct max77705_charger_data *charger =
		container_of(work, struct max77705_charger_data, wc_current_work.work);
	union power_supply_propval value = {0, };
	int diff_current = 0;

	if (is_not_wireless_type(charger->cable_type)) {
		charger->wc_pre_current = WC_CURRENT_START;
		max77705_write_reg(charger->i2c, MAX77705_CHG_REG_CNFG_10, 0x10);
		__pm_relax(charger->wc_current_ws);
		return;
	}

	if (charger->wc_pre_current == charger->wc_current) {
		max77705_set_charge_current(charger, charger->charging_current);
#if IS_ENABLED(CONFIG_DUAL_BATTERY)
		psy_do_property("battery", set,
			POWER_SUPPLY_EXT_PROP_FASTCHG_LIMIT_CURRENT, value);
#endif
		/* Wcurr-B) Restore Vrect adj room to previous value
		 *  after finishing wireless input current setting.
		 * Refer to Wcurr-A) step
		 */
		msleep(500);
		value.intval = WIRELESS_VRECT_ADJ_OFF; /* Vrect Room 0mV */
		psy_do_property(charger->pdata->wireless_charger_name, set,
				POWER_SUPPLY_EXT_PROP_WIRELESS_RX_CONTROL, value);
		/* keep wakeup_source if this is not last work to prevent to enter suspend */
		if (!delayed_work_pending(&charger->wc_current_work))
			__pm_relax(charger->wc_current_ws);
	} else {
		diff_current = charger->wc_pre_current - charger->wc_current;
		diff_current = (diff_current > charger->pdata->wc_current_step) ? charger->pdata->wc_current_step :
			((diff_current < -charger->pdata->wc_current_step) ? -charger->pdata->wc_current_step : diff_current);

		charger->wc_pre_current -= diff_current;
		max77705_set_input_current(charger, charger->wc_pre_current);
		__pm_stay_awake(charger->wc_current_ws);
		queue_delayed_work(charger->wqueue, &charger->wc_current_work,
				   msecs_to_jiffies(WC_CURRENT_WORK_STEP));
	}
	pr_info("%s: wc_current(%d), wc_pre_current(%d), diff(%d)\n", __func__,
		charger->wc_current, charger->wc_pre_current, diff_current);
}

__visible_for_testing void max77705_wc_chg_current_work(struct work_struct *work)
{
	struct max77705_charger_data *charger =
		container_of(work, struct max77705_charger_data, wc_chg_current_work.work);
#if IS_ENABLED(CONFIG_DUAL_BATTERY)
	union power_supply_propval value = {0, };
#endif

	max77705_set_charge_current(charger, charger->charging_current);
#if IS_ENABLED(CONFIG_DUAL_BATTERY)
	psy_do_property("battery", set,
		POWER_SUPPLY_EXT_PROP_FASTCHG_LIMIT_CURRENT, value);
#endif
	__pm_relax(charger->wc_chg_current_ws);
}

static irqreturn_t max77705_sysovlo_irq(int irq, void *data)
{
	struct max77705_charger_data *charger = data;
	union power_supply_propval value;

	pr_info("%s\n", __func__);
	__pm_wakeup_event(charger->sysovlo_ws, jiffies_to_msecs(HZ * 5));

	psy_do_property("battery", set, POWER_SUPPLY_EXT_PROP_SYSOVLO, value);

	max77705_set_sysovlo(charger, 0);
	return IRQ_HANDLED;
}

#ifdef CONFIG_OF
static int max77705_charger_parse_dt(struct max77705_charger_data *charger)
{
	struct device_node *np;
	max77705_charger_platform_data_t *pdata = charger->pdata;
	int ret = 0;

	np = of_find_node_by_name(NULL, "battery");
	if (!np) {
		pr_err("%s: np(battery) NULL\n", __func__);
	} else {
		ret = of_property_read_u32(np, "battery,chg_float_voltage",
					   &pdata->chg_float_voltage);
		if (ret) {
			pr_info("%s: battery,chg_float_voltage is Empty\n", __func__);
			pdata->chg_float_voltage = 4200;
		}
		charger->float_voltage = pdata->chg_float_voltage;

		ret = of_property_read_u32(np, "battery,chg_ocp_current",
					   &pdata->chg_ocp_current);
		if (ret) {
			pr_info("%s: battery,chg_ocp_current is Empty\n", __func__);
			pdata->chg_ocp_current = 5600; /* mA */
		}

		ret = of_property_read_u32(np, "battery,chg_ocp_dtc",
					   &pdata->chg_ocp_dtc);
		if (ret) {
			pr_info("%s: battery,chg_ocp_dtc is Empty\n", __func__);
			pdata->chg_ocp_dtc = 6; /* ms */
		}

		ret = of_property_read_u32(np, "battery,topoff_time",
					   &pdata->topoff_time);
		if (ret) {
			pr_info("%s: battery,topoff_time is Empty\n", __func__);
			pdata->topoff_time = 30; /* min */
		}

		ret = of_property_read_string(np, "battery,wireless_charger_name",
					(char const **)&pdata->wireless_charger_name);
		if (ret)
			pr_info("%s: Wireless charger name is Empty\n", __func__);

		ret = of_property_read_u32(np, "battery,full_check_type_2nd",
					   &pdata->full_check_type_2nd);
		if (ret)
			pr_info("%s : Full check type 2nd is Empty\n", __func__);

		ret = of_property_read_u32(np, "battery,wireless_cc_cv",
						&pdata->wireless_cc_cv);
		if (ret)
			pr_info("%s : wireless_cc_cv is Empty\n", __func__);

		ret = of_property_read_u32(np, "battery,nv_wc_headroom",
								&pdata->nv_wc_headroom);
		if (ret) {
			pr_info("%s : nv_wc_headroom is Empty\n", __func__);
			pdata->nv_wc_headroom = WIRELESS_VRECT_ADJ_ROOM_1; /* 277mV */
		}

		pr_info("%s: fv : %d, ocp_curr : %d, ocp_dtc : %d, topoff_time : %d\n",
				__func__, charger->float_voltage, pdata->chg_ocp_current,
				pdata->chg_ocp_dtc, pdata->topoff_time);
	}

	np = of_find_node_by_name(NULL, "max77705-charger");
	if (!np) {
		pr_err("%s: np(max77705-charger) NULL\n", __func__);
	} else {
		ret = of_property_read_u32(np, "charger,fac_vsys", &pdata->fac_vsys);
		if (ret) {
			pr_info("%s : fac_vsys is Empty\n", __func__);
			pdata->fac_vsys = 3800; /* mV */
		}

		pdata->enable_sysovlo_irq =
		    of_property_read_bool(np, "charger,enable_sysovlo_irq");

#if defined(CONFIG_SUPPORT_SHIP_MODE)
		/* there is ship_mode config, but no use ship mode */
		if (of_property_read_u32(np, "charger,disable_ship_mode", &pdata->disable_ship_mode)) {
			pr_info("%s : disable_ship_mode is Empty\n", __func__);
			pdata->disable_ship_mode = 0;
		}
#endif

		pdata->enable_noise_wa =
		    of_property_read_bool(np, "charger,enable_noise_wa");

		if (of_property_read_u32(np, "charger,fsw", &pdata->fsw)) {
			pr_info("%s : fsw is Empty\n", __func__);
			pdata->fsw = MAX77705_CHG_FSW_1_5MHz;
		}
		charger->fsw_now = pdata->fsw;

		ret = of_property_read_u32(np, "charger,wc_current_step",
					   &pdata->wc_current_step);
		if (ret) {
			pr_info("%s: battery,wc_current_step is Empty\n", __func__);
			pdata->wc_current_step = WC_CURRENT_STEP; /* default 100mA */
		}

		pr_info("%s: fac_vsys:%d, fsw:%d, wc_current_step:%d\n", __func__,
			pdata->fac_vsys, pdata->fsw, pdata->wc_current_step);
	}

	np = of_find_node_by_name(NULL, "max77705-fuelgauge");
	if (!np) {
		pr_err("%s: np(max77705-fuelgauge) NULL\n", __func__);
	} else {
		charger->jig_low_active = of_property_read_bool(np,
						"fuelgauge,jig_low_active");
		charger->jig_gpio = of_get_named_gpio(np, "fuelgauge,jig_gpio", 0);
		if (charger->jig_gpio < 0) {
			pr_err("%s: error reading jig_gpio = %d\n",
				__func__, charger->jig_gpio);
			charger->jig_gpio = 0;
		}
	}

	return ret;
}
#endif

static const struct power_supply_desc max77705_charger_power_supply_desc = {
	.name = "max77705-charger",
	.type = POWER_SUPPLY_TYPE_UNKNOWN,
	.properties = max77705_charger_props,
	.num_properties = ARRAY_SIZE(max77705_charger_props),
	.get_property = max77705_chg_get_property,
	.set_property = max77705_chg_set_property,
	.no_thermal = true,
};

static char *max77705_otg_supply_list[] = {
	"otg",
};

static const struct power_supply_desc max77705_otg_power_supply_desc = {
	.name = "max77705-otg",
	.type = POWER_SUPPLY_TYPE_UNKNOWN,
	.properties = max77705_otg_props,
	.num_properties = ARRAY_SIZE(max77705_otg_props),
	.get_property = max77705_otg_get_property,
	.set_property = max77705_otg_set_property,
};

static int max77705_charger_probe(struct platform_device *pdev)
{
	struct max77705_dev *max77705 = dev_get_drvdata(pdev->dev.parent);
	struct max77705_platform_data *pdata = dev_get_platdata(max77705->dev);
	max77705_charger_platform_data_t *charger_data;
	struct max77705_charger_data *charger;
	struct power_supply_config charger_cfg = { };
	int ret = 0;
	u8 reg_data;

	pr_info("%s: max77705 Charger Driver Loading\n", __func__);

	charger = kzalloc(sizeof(*charger), GFP_KERNEL);
	if (!charger)
		return -ENOMEM;

	charger_data = kzalloc(sizeof(max77705_charger_platform_data_t), GFP_KERNEL);
	if (!charger_data) {
		ret = -ENOMEM;
		goto err_free;
	}

	mutex_init(&charger->charger_mutex);
	mutex_init(&charger->mode_mutex);
	mutex_init(&charger->icl_mutex);

	charger->dev = &pdev->dev;
	charger->i2c = max77705->charger;
	charger->pmic_i2c = max77705->i2c;
	charger->pdata = charger_data;
	charger->aicl_curr = 0;
	charger->slow_charging = false;
	charger->otg_on = false;
	charger->uno_on = false;
	charger->max77705_pdata = pdata;
	charger->wc_pre_current = WC_CURRENT_START;
	charger->cable_type = SEC_BATTERY_CABLE_NONE;

#if defined(CONFIG_OF)
	ret = max77705_charger_parse_dt(charger);
	if (ret < 0)
		pr_err("%s not found charger dt! ret[%d]\n", __func__, ret);
#endif
	platform_set_drvdata(pdev, charger);

	max77705_charger_initialize(charger);

	max77705_read_reg(charger->i2c, MAX77705_CHG_REG_INT_OK, &reg_data);
	if (reg_data & MAX77705_WCIN_OK)
		charger->cable_type = SEC_BATTERY_CABLE_WIRELESS;
	charger->input_current = max77705_get_input_current(charger);
	charger->charging_current = max77705_get_charge_current(charger);

	if (max77705_read_reg(max77705->i2c, MAX77705_PMIC_REG_PMICREV, &reg_data) < 0) {
		pr_err("device not found on this channel (this is not an error)\n");
		ret = -ENOMEM;
		goto err_pdata_free;
	} else {
		charger->pmic_ver = (reg_data & 0x7);
		pr_info("%s : device found : ver.0x%x\n", __func__, charger->pmic_ver);
	}

	(void)debugfs_create_file("max77705-regs",
				S_IRUGO, NULL, (void *)charger,
				  &max77705_debugfs_fops);

	charger->wqueue = create_singlethread_workqueue(dev_name(&pdev->dev));
	if (!charger->wqueue) {
		pr_err("%s: Fail to Create Workqueue\n", __func__);
		goto err_pdata_free;
	}
	charger->aicl_ws = wakeup_source_register(&pdev->dev, "charger-aicl");
	charger->wc_current_ws = wakeup_source_register(&pdev->dev, "charger->wc-current");
	charger->otg_ws = wakeup_source_register(&pdev->dev, "charger->otg");
	charger->wc_chg_current_ws = wakeup_source_register(&pdev->dev, "charger->wc-chg-current");

	INIT_DELAYED_WORK(&charger->aicl_work, max77705_aicl_isr_work);
	INIT_DELAYED_WORK(&charger->wc_current_work, max77705_wc_current_work);
	INIT_DELAYED_WORK(&charger->wc_chg_current_work, max77705_wc_chg_current_work);

	charger_cfg.drv_data = charger;
	charger->psy_chg = power_supply_register(&pdev->dev,
				&max77705_charger_power_supply_desc, &charger_cfg);
	if (IS_ERR(charger->psy_chg)) {
		ret = PTR_ERR(charger->psy_chg);
		pr_err("%s: Failed to Register psy_chg(%d)\n", __func__, ret);
		goto err_power_supply_register;
	}

	charger->psy_otg = power_supply_register(&pdev->dev,
				  &max77705_otg_power_supply_desc, &charger_cfg);
	if (IS_ERR(charger->psy_otg)) {
		ret = PTR_ERR(charger->psy_otg);
		pr_err("%s: Failed to Register otg_chg(%d)\n", __func__, ret);
		goto err_power_supply_register_otg;
	}
	charger->psy_otg->supplied_to = max77705_otg_supply_list;
	charger->psy_otg->num_supplicants = ARRAY_SIZE(max77705_otg_supply_list);

	if (charger->pdata->chg_irq) {
		INIT_DELAYED_WORK(&charger->isr_work, max77705_chg_isr_work);

		ret = request_threaded_irq(charger->pdata->chg_irq, NULL,
				max77705_chg_irq_thread, 0,
				"charger-irq", charger);
		if (ret) {
			pr_err("%s: Failed to Request IRQ\n", __func__);
			goto err_irq;
		}

		ret = enable_irq_wake(charger->pdata->chg_irq);
		if (ret < 0)
			pr_err("%s: Failed to Enable Wakeup Source(%d)\n", __func__, ret);
	}

	max77705_read_reg(charger->i2c, MAX77705_CHG_REG_INT_OK, &reg_data);

	charger->irq_bypass = pdata->irq_base + MAX77705_CHG_IRQ_BYP_I;
	ret = request_threaded_irq(charger->irq_bypass, NULL,
					max77705_bypass_irq, 0,
					"bypass-irq", charger);
	if (ret < 0)
		pr_err("%s: fail to request bypass IRQ: %d: %d\n",
		       __func__, charger->irq_bypass, ret);

	charger->irq_aicl_enabled = -1;
	charger->irq_aicl = pdata->irq_base + MAX77705_CHG_IRQ_AICL_I;
	charger->irq_batp = pdata->irq_base + MAX77705_CHG_IRQ_BATP_I;
	ret = request_threaded_irq(charger->irq_batp, NULL,
					max77705_batp_irq, 0,
					"batp-irq", charger);
	if (ret < 0)
		pr_err("%s: fail to request Battery Presense IRQ: %d: %d\n",
		       __func__, charger->irq_batp, ret);

#if defined(CONFIG_MAX77705_CHECK_B2SOVRC)
	if ((sec_debug_get_debug_level() & 0x1) == 0x1) {
		/* only work for debug level is mid */
		charger->irq_bat = pdata->irq_base + MAX77705_CHG_IRQ_BAT_I;
		ret = request_threaded_irq(charger->irq_bat, NULL,
					   max77705_bat_irq, 0,
					   "bat-irq", charger);
		if (ret < 0)
			pr_err("%s: fail to request Battery IRQ: %d: %d\n",
				   __func__, charger->irq_bat, ret);
	}
#endif

	if (charger->pdata->enable_sysovlo_irq) {
		charger->sysovlo_ws = wakeup_source_register(&pdev->dev, "max77705-sysovlo");
		/* Enable BIAS */
		max77705_update_reg(max77705->i2c, MAX77705_PMIC_REG_MAINCTRL1,
				    0x80, 0x80);
		/* set IRQ thread */
		charger->irq_sysovlo =
		    pdata->irq_base + MAX77705_SYSTEM_IRQ_SYSOVLO_INT;
		ret = request_threaded_irq(charger->irq_sysovlo, NULL,
						max77705_sysovlo_irq, 0,
						"sysovlo-irq", charger);
		if (ret < 0)
			pr_err("%s: fail to request sysovlo IRQ: %d: %d\n",
			       __func__, charger->irq_sysovlo, ret);
		enable_irq_wake(charger->irq_sysovlo);
	}

	ret = max77705_chg_create_attrs(&charger->psy_chg->dev);
	if (ret) {
		dev_err(charger->dev, "%s : Failed to max77705_chg_create_attrs\n", __func__);
		goto err_atts;
	}

	/* watchdog kick */
	max77705_chg_set_wdtmr_kick(charger);
	pr_info("%s: MAX77705 Charger Driver Loaded\n", __func__);

	return 0;

err_atts:
	free_irq(charger->pdata->chg_irq, charger);
err_irq:
	power_supply_unregister(charger->psy_otg);
err_power_supply_register_otg:
	power_supply_unregister(charger->psy_chg);
err_power_supply_register:
	destroy_workqueue(charger->wqueue);
	wakeup_source_unregister(charger->sysovlo_ws);
	wakeup_source_unregister(charger->otg_ws);
	wakeup_source_unregister(charger->wc_current_ws);
	wakeup_source_unregister(charger->aicl_ws);
	wakeup_source_unregister(charger->wc_chg_current_ws);
err_pdata_free:
	kfree(charger_data);
err_free:
	kfree(charger);

	return ret;
}

static int max77705_charger_remove(struct platform_device *pdev)
{
	struct max77705_charger_data *charger = platform_get_drvdata(pdev);

	pr_info("%s: ++\n", __func__);

	destroy_workqueue(charger->wqueue);

	if (charger->i2c) {
		u8 reg_data;

		reg_data = MAX77705_MODE_4_BUCK_ON;
		max77705_write_reg(charger->i2c, MAX77705_CHG_REG_CNFG_00, reg_data);
		reg_data = 0x0F;
		max77705_write_reg(charger->i2c, MAX77705_CHG_REG_CNFG_09, reg_data);
		reg_data = 0x10;
		max77705_write_reg(charger->i2c, MAX77705_CHG_REG_CNFG_10, reg_data);
		reg_data = 0x60;
		max77705_write_reg(charger->i2c, MAX77705_CHG_REG_CNFG_12, reg_data);
	} else {
		pr_err("%s: no max77705 i2c client\n", __func__);
	}

	if (charger->irq_sysovlo)
		free_irq(charger->irq_sysovlo, charger);
	if (charger->pdata->chg_irq)
		free_irq(charger->pdata->chg_irq, charger);
	if (charger->psy_chg)
		power_supply_unregister(charger->psy_chg);
	if (charger->psy_otg)
		power_supply_unregister(charger->psy_otg);

	wakeup_source_unregister(charger->sysovlo_ws);
	wakeup_source_unregister(charger->otg_ws);
	wakeup_source_unregister(charger->wc_current_ws);
	wakeup_source_unregister(charger->aicl_ws);
	wakeup_source_unregister(charger->wc_chg_current_ws);

	kfree(charger);

	pr_info("%s: --\n", __func__);

	return 0;
}

#if defined CONFIG_PM
static int max77705_charger_prepare(struct device *dev)
{
	struct max77705_charger_data *charger = dev_get_drvdata(dev);

	pr_info("%s\n", __func__);

	if ((charger->cable_type == SEC_BATTERY_CABLE_USB ||
		charger->cable_type == SEC_BATTERY_CABLE_TA)
		&& charger->input_current >= 500) {
		u8 reg_data;

		max77705_read_reg(charger->i2c, MAX77705_CHG_REG_CNFG_09, &reg_data);
		reg_data &= MAX77705_CHG_CHGIN_LIM;
		max77705_usbc_icurr(reg_data);
		max77705_set_fw_noautoibus(MAX77705_AUTOIBUS_ON);
	}

	return 0;
}

static int max77705_charger_suspend(struct device *dev)
{
	return 0;
}

static int max77705_charger_resume(struct device *dev)
{
	return 0;
}

static void max77705_charger_complete(struct device *dev)
{
	struct max77705_charger_data *charger = dev_get_drvdata(dev);

	pr_info("%s\n", __func__);

	if (!max77705_get_autoibus(charger))
		max77705_set_fw_noautoibus(MAX77705_AUTOIBUS_AT_OFF);
}
#else
#define max77705_charger_prepare NULL
#define max77705_charger_suspend NULL
#define max77705_charger_resume NULL
#define max77705_charger_complete NULL
#endif

static void max77705_charger_shutdown(struct platform_device *pdev)
{
	struct max77705_charger_data *charger = platform_get_drvdata(pdev);

	pr_info("%s: ++\n", __func__);

#if defined(CONFIG_SEC_FACTORY)
	if (max77705_get_facmode())
		goto free_chg;	/* prevent SMPL during SMD ARRAY shutdown */
#endif
	if (charger->i2c) {
		u8 reg_data;

		reg_data = MAX77705_MODE_4_BUCK_ON;	/* Buck on, Charge off */
		max77705_write_reg(charger->i2c, MAX77705_CHG_REG_CNFG_00, reg_data);
#if !defined(CONFIG_SEC_FACTORY)
		if ((is_wired_type(charger->cable_type))
			&& (charger->cable_type != SEC_BATTERY_CABLE_USB))
			reg_data = 0x3B;	/* CHGIN_ILIM 1500mA */
		else
#endif
			reg_data = 0x13;	/* CHGIN_ILIM 500mA */
		max77705_write_reg(charger->i2c, MAX77705_CHG_REG_CNFG_09, reg_data);
		reg_data = 0x13;	/* WCIN_ILIM 500mA */
		max77705_write_reg(charger->i2c, MAX77705_CHG_REG_CNFG_10, reg_data);
		reg_data = 0x60;	/* CHGINSEL/WCINSEL enable */
		max77705_write_reg(charger->i2c, MAX77705_CHG_REG_CNFG_12, reg_data);

		/* enable auto shipmode, this should work under 2.6V */
		max77705_set_auto_ship_mode(charger, 1);
	} else {
		pr_err("%s: no max77705 i2c client\n", __func__);
	}

#if defined(CONFIG_SEC_FACTORY)
free_chg:
#endif
	free_irq(charger->irq_aicl, charger);
	free_irq(charger->irq_chgin, charger);
	free_irq(charger->irq_bypass, charger);
	free_irq(charger->irq_batp, charger);
#if defined(CONFIG_MAX77705_CHECK_B2SOVRC)
	if (charger->irq_bat)
		free_irq(charger->irq_bat, charger);
#endif
	if (charger->irq_sysovlo)
		free_irq(charger->irq_sysovlo, charger);
	if (charger->pdata->chg_irq)
		free_irq(charger->pdata->chg_irq, charger);

	cancel_delayed_work(&charger->aicl_work);
	cancel_delayed_work(&charger->isr_work);
	cancel_delayed_work(&charger->wc_current_work);

	pr_info("%s: --\n", __func__);
}

static const struct dev_pm_ops max77705_charger_pm_ops = {
	.prepare = max77705_charger_prepare,
	.suspend = max77705_charger_suspend,
	.resume = max77705_charger_resume,
	.complete = max77705_charger_complete,
};

static struct platform_driver max77705_charger_driver = {
	.driver = {
		   .name = "max77705-charger",
		   .owner = THIS_MODULE,
#ifdef CONFIG_PM
		   .pm = &max77705_charger_pm_ops,
#endif
	},
	.probe = max77705_charger_probe,
	.remove = max77705_charger_remove,
	.shutdown = max77705_charger_shutdown,
};

static int __init max77705_charger_init(void)
{
	pr_info("%s:\n", __func__);
	return platform_driver_register(&max77705_charger_driver);
}

static void __exit max77705_charger_exit(void)
{
	platform_driver_unregister(&max77705_charger_driver);
}

module_init(max77705_charger_init);
module_exit(max77705_charger_exit);

MODULE_DESCRIPTION("Samsung MAX77705 Charger Driver");
MODULE_AUTHOR("Samsung Electronics");
MODULE_LICENSE("GPL");
