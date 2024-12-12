/*
 *  max77705_fuelgauge.c
 *  Samsung max77705 Fuel Gauge Driver
 *
 *  Copyright (C) 2015 Samsung Electronics
 *
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#define DEBUG
/* #define BATTERY_LOG_MESSAGE */

#include <linux/mfd/max77705-private.h>
#include <linux/of_gpio.h>
#include <linux/debugfs.h>
#include <linux/seq_file.h>
#include "max77705_fuelgauge.h"

#if defined(CONFIG_SEC_KUNIT)
#define __visible_for_testing
#else
#define __visible_for_testing static
#endif

#if IS_ENABLED(CONFIG_SEC_MPARAM) || (IS_MODULE(CONFIG_SEC_PARAM) && defined(CONFIG_ARCH_EXYNOS))
extern unsigned int lpcharge;
#else
static unsigned int __read_mostly lpcharge;
module_param(lpcharge, uint, 0444);
#endif

static enum power_supply_property max77705_fuelgauge_props[] = {
	POWER_SUPPLY_PROP_ONLINE,
};

bool max77705_fg_fuelalert_init(struct max77705_fuelgauge_data *fuelgauge,
				int soc);
static void max77705_fg_periodic_read_power(
				struct max77705_fuelgauge_data *fuelgauge);
static void max77705_reset_bat_id(struct max77705_fuelgauge_data *fuelgauge);

static struct device_attribute max77705_fg_attrs[] = {
	MAX77705_FG_ATTR(fg_data),
};

static unsigned int max77705_get_lpmode(void) { return lpcharge; }
__visible_for_testing int max77705_fg_read_vfsoc(struct max77705_fuelgauge_data *fuelgauge);

#if !defined(CONFIG_SEC_FACTORY)
static void max77705_fg_adaptation_wa(struct max77705_fuelgauge_data *fuelgauge)
{
	u32 rcomp0;
	u32 fullcapnom, fullcaprep, designcap;
	u32 temp;
	u8 data[2];
	int vfsoc;
	struct fg_reset_wa *fg_reset_data = fuelgauge->fg_reset_data;

	if (!fg_reset_data)
		return;

	/* check RCOMP0 */
	rcomp0 = max77705_read_word(fuelgauge->i2c, RCOMP_REG);
	if ((rcomp0 > (fg_reset_data->rcomp0 * 14 / 10)) || (rcomp0 < (fg_reset_data->rcomp0 * 7 / 10))) {
		pr_err("%s: abnormal RCOMP0 (0x%x / 0x%x)\n", __func__, rcomp0, fg_reset_data->rcomp0);
		goto set_default_value;
	}

	/* check TEMPCO */
	if (max77705_bulk_read(fuelgauge->i2c, TEMPCO_REG,
			       2, data) < 0) {
		pr_err("%s: Failed to read TEMPCO\n", __func__);
		return;
	}
	/* tempcohot = data[1]; 	tempcocold = data[0]; */
	temp = (fg_reset_data->tempco & 0xFF00) >> 8;
	if ((data[1] > (temp * 14 / 10)) || (data[1] < (temp * 7 / 10))) {
		pr_err("%s: abnormal TempCoHot (0x%x / 0x%x)\n", __func__, data[1], temp);
		goto set_default_value;
	}

	temp = fg_reset_data->tempco & 0x00FF;
	if ((data[0] > (temp * 14 / 10)) || (data[0] < (temp * 7 / 10))) {
		pr_err("%s: abnormal TempCoCold (0x%x / 0x%x)\n", __func__, data[0], temp);
		goto set_default_value;
	}

	/* get DESIGNCAP */
	designcap = max77705_read_word(fuelgauge->i2c, DESIGNCAP_REG);

	/* check FULLCAPNOM */
	fullcapnom = max77705_read_word(fuelgauge->i2c, FULLCAP_NOM_REG);
	if (fullcapnom > (designcap * 11 / 10)) {
		pr_err("%s: abnormal fullcapnom (0x%x / 0x%x)\n", __func__, fullcapnom, designcap);
		goto re_calculation_fullcap_nom;
	}

	/* check FULLCAPREP */
	fullcaprep = max77705_read_word(fuelgauge->i2c, FULLCAP_REP_REG);
	if (fullcaprep > (designcap * 115 / 100)) {
		pr_err("%s: abnormal fullcaprep (0x%x / 0x%x)\n", __func__, fullcaprep, designcap);
		goto re_calculation_fullcap_rep;
	}

	return;

re_calculation_fullcap_nom:
	pr_err("%s: enter re_calculation fullcapnom\n", __func__);
	max77705_write_word(fuelgauge->i2c, DPACC_REG, fg_reset_data->dPacc);
	max77705_write_word(fuelgauge->i2c, DQACC_REG, fg_reset_data->dQacc);
	max77705_write_word(fuelgauge->i2c, FULLCAP_NOM_REG, fg_reset_data->fullcapnom);
	temp = max77705_read_word(fuelgauge->i2c, LEARN_CFG_REG);
	temp &= 0xFF0F;
	max77705_write_word(fuelgauge->i2c, LEARN_CFG_REG, temp);
	max77705_write_word(fuelgauge->i2c, CYCLES_REG, 0);
re_calculation_fullcap_rep:
	pr_err("%s: enter re_calculation fullcaprep\n", __func__);
	vfsoc = max77705_fg_read_vfsoc(fuelgauge);
	max77705_write_word(fuelgauge->i2c, REMCAP_REP_REG, vfsoc * fg_reset_data->fullcapnom / 1000);
	msleep(200);
	max77705_write_word(fuelgauge->i2c, FULLCAP_REP_REG, fg_reset_data->fullcapnom);
	fuelgauge->err_cnt++;
set_default_value:
	pr_err("%s: enter set_default_value\n", __func__);
	max77705_write_word(fuelgauge->i2c, RCOMP_REG, fg_reset_data->rcomp0);
	max77705_write_word(fuelgauge->i2c, TEMPCO_REG, fg_reset_data->tempco);

	return;
}

static void max77705_fg_periodic_read(struct max77705_fuelgauge_data *fuelgauge)
{
	u8 reg;
	int i, data[0x10];
	char *str = NULL;
	unsigned int v_cnt = 0; /* verify count */
	unsigned int err_cnt = 0;
	struct verify_reg *fg_verify = fuelgauge->verify_selected_reg;

	str = kzalloc(sizeof(char) * 1024, GFP_KERNEL);
	if (!str)
		return;

	for (i = 0; i < 16; i++) {
		if (i == 5)
			i = 11;
		else if (i == 12)
			i = 13;
		for (reg = 0; reg < 0x10; reg++) {
			data[reg] = max77705_read_word(fuelgauge->i2c, reg + i * 0x10);

			if (data[reg] < 0) {
				kfree(str);
				return;
			}

			/* verify fg reg */
			if (!fuelgauge->skip_fg_verify && fg_verify && v_cnt < fuelgauge->verify_selected_reg_length) {
				if (fg_verify[v_cnt].addr == reg + i * 0x10) {
					if (fg_verify[v_cnt].data != data[reg]) {
						pr_err("%s:[%d] addr(0x%x 0x%x) data(0x%x 0x%x) read veryfy error! not matched!\n",
								__func__, v_cnt,
								fg_verify[v_cnt].addr, reg + i * 0x10,
								fg_verify[v_cnt].data, data[reg]);
						err_cnt++;
					}
					v_cnt++;
				}
			}
		}
		sprintf(str + strlen(str),
			"%04xh,%04xh,%04xh,%04xh,%04xh,%04xh,%04xh,%04xh,",
			data[0x00], data[0x01], data[0x02], data[0x03],
			data[0x04], data[0x05], data[0x06], data[0x07]);
		sprintf(str + strlen(str),
			"%04xh,%04xh,%04xh,%04xh,%04xh,%04xh,%04xh,%04xh,",
			data[0x08], data[0x09], data[0x0a], data[0x0b],
			data[0x0c], data[0x0d], data[0x0e], data[0x0f]);

		if (!fuelgauge->initial_update_of_soc)
			msleep(1);
	}

	pr_info("[%d][FG] %s\n", fuelgauge->battery_data->battery_id, str);

	max77705_fg_adaptation_wa(fuelgauge);

	kfree(str);
}
#endif

__visible_for_testing int max77705_fg_read_vcell(struct max77705_fuelgauge_data *fuelgauge)
{
	u8 data[2];
	u32 vcell, temp;
	u16 w_data;

	if (max77705_bulk_read(fuelgauge->i2c, VCELL_REG, 2, data) < 0) {
		pr_err("%s: Failed to read VCELL_REG\n", __func__);
		return -1;
	}

	w_data = (data[1] << 8) | data[0];

	temp = (w_data & 0xFFF) * 78125;
	vcell = temp / 1000000;

	temp = ((w_data & 0xF000) >> 4) * 78125;
	temp /= 1000000;
	vcell += temp << 4;

	if (!(fuelgauge->info.pr_cnt++ % PRINT_COUNT)) {
		fuelgauge->info.pr_cnt = 1;
		pr_info("%s: VCELL(%d)mV, data(0x%04x)\n",
			__func__, vcell, (data[1] << 8) | data[0]);
	}

	if ((fuelgauge->vempty_mode == VEMPTY_MODE_SW_VALERT) &&
	    (vcell >= fuelgauge->battery_data->sw_v_empty_recover_vol)) {
		fuelgauge->vempty_mode = VEMPTY_MODE_SW_RECOVERY;
		max77705_fg_fuelalert_init(fuelgauge, fuelgauge->pdata->fuel_alert_soc);
		pr_info("%s: Recoverd from SW V EMPTY Activation\n", __func__);
#if defined(CONFIG_BATTERY_CISD)
		if (fuelgauge->valert_count_flag) {
			pr_info("%s: Vcell(%d) release CISD VALERT COUNT check\n",
					__func__, vcell);
			fuelgauge->valert_count_flag = false;
		}
#endif
	}

	return vcell;
}

static int max77705_fg_read_vfocv(struct max77705_fuelgauge_data *fuelgauge)
{
	u8 data[2];
	u32 vfocv = 0, temp;
	u16 w_data;

	if (max77705_bulk_read(fuelgauge->i2c, VFOCV_REG, 2, data) < 0) {
		pr_err("%s: Failed to read VFOCV_REG\n", __func__);
		return -1;
	}

	w_data = (data[1] << 8) | data[0];

	temp = (w_data & 0xFFF) * 78125;
	vfocv = temp / 1000000;

	temp = ((w_data & 0xF000) >> 4) * 78125;
	temp /= 1000000;
	vfocv += (temp << 4);

#if !defined(CONFIG_SEC_FACTORY)
	max77705_fg_periodic_read(fuelgauge);
#endif
	max77705_fg_periodic_read_power(fuelgauge);

	return vfocv;
}

__visible_for_testing int max77705_fg_read_avg_vcell(struct max77705_fuelgauge_data *fuelgauge)
{
	u8 data[2];
	u32 avg_vcell = 0, temp;
	u16 w_data;

	if (max77705_bulk_read(fuelgauge->i2c, AVR_VCELL_REG, 2, data) < 0) {
		pr_err("%s: Failed to read AVR_VCELL_REG\n", __func__);
		return -1;
	}

	w_data = (data[1] << 8) | data[0];

	temp = (w_data & 0xFFF) * 78125;
	avg_vcell = temp / 1000000;

	temp = ((w_data & 0xF000) >> 4) * 78125;
	temp /= 1000000;
	avg_vcell += (temp << 4);

	return avg_vcell;
}

__visible_for_testing int max77705_fg_check_battery_present(
					struct max77705_fuelgauge_data *fuelgauge)
{
	u8 status_data[2];
	int ret = 1;

	/* 1. Check Bst bit */
	if (max77705_bulk_read(fuelgauge->i2c, STATUS_REG, 2, status_data) < 0) {
		pr_err("%s: Failed to read STATUS_REG\n", __func__);
		return 0;
	}

	if (status_data[0] & (0x1 << 3)) {
		pr_info("%s: addr(0x00), data(0x%04x)\n", __func__,
			(status_data[1] << 8) | status_data[0]);
		pr_info("%s: battery is absent!!\n", __func__);
		ret = 0;
	}

	return ret;
}

static void max77705_fg_set_vempty(struct max77705_fuelgauge_data *fuelgauge,
				   int vempty_mode)
{
	u16 data = 0;
	u8 valrt_data[2] = { 0, };

	if (!fuelgauge->using_temp_compensation) {
		pr_info("%s: does not use temp compensation, default hw vempty\n",
			__func__);
		vempty_mode = VEMPTY_MODE_HW;
	}

	fuelgauge->vempty_mode = vempty_mode;
	switch (vempty_mode) {
	case VEMPTY_MODE_SW:
		/* HW Vempty Disable */
		max77705_write_word(fuelgauge->i2c, VEMPTY_REG,
				    fuelgauge->battery_data->V_empty_origin);
		/* Reset VALRT Threshold setting (enable) */
		valrt_data[1] = 0xFF;
		valrt_data[0] = fuelgauge->battery_data->sw_v_empty_vol / 20;
		if (max77705_bulk_write(fuelgauge->i2c, VALRT_THRESHOLD_REG,
					2, valrt_data) < 0) {
			pr_info("%s: Failed to write VALRT_THRESHOLD_REG\n", __func__);
			return;
		}
		data = max77705_read_word(fuelgauge->i2c, VALRT_THRESHOLD_REG);
		pr_info("%s: HW V EMPTY Disable, SW V EMPTY Enable with %d mV (%d)\n",
			__func__, fuelgauge->battery_data->sw_v_empty_vol, (data & 0x00ff) * 20);
		break;
	default:
		/* HW Vempty Enable */
		max77705_write_word(fuelgauge->i2c, VEMPTY_REG,
				    fuelgauge->battery_data->V_empty);
		/* Reset VALRT Threshold setting (disable) */
		valrt_data[1] = 0xFF;
		valrt_data[0] = fuelgauge->battery_data->sw_v_empty_vol_cisd / 20;
		if (max77705_bulk_write(fuelgauge->i2c, VALRT_THRESHOLD_REG,
					2, valrt_data) < 0) {
			pr_info("%s: Failed to write VALRT_THRESHOLD_REG\n", __func__);
			return;
		}
		data = max77705_read_word(fuelgauge->i2c, VALRT_THRESHOLD_REG);
		pr_info("%s: HW V EMPTY Enable, SW V EMPTY Disable %d mV (%d)\n",
		     __func__, 0, (data & 0x00ff) * 20);
		break;
	}
}

__visible_for_testing int max77705_fg_write_temp(struct max77705_fuelgauge_data *fuelgauge,
				  int temperature)
{
	u8 data[2];

	data[0] = (temperature % 10) * 1000 / 39;
	data[1] = temperature / 10;
	max77705_bulk_write(fuelgauge->i2c, TEMPERATURE_REG, 2, data);

	pr_debug("%s: temperature to (%d, 0x%02x%02x)\n",
		__func__, temperature, data[1], data[0]);

	fuelgauge->temperature = temperature;
	if (!fuelgauge->vempty_init_flag)
		fuelgauge->vempty_init_flag = true;

	return temperature;
}

__visible_for_testing int max77705_fg_read_temp(struct max77705_fuelgauge_data *fuelgauge)
{
	u8 data[2] = { 0, 0 };
	int temper = 0;

	if (max77705_fg_check_battery_present(fuelgauge)) {
		if (max77705_bulk_read(fuelgauge->i2c, TEMPERATURE_REG, 2, data) < 0) {
			pr_err("%s: Failed to read TEMPERATURE_REG\n", __func__);
			return -1;
		}

		if (data[1] & (0x1 << 7)) {
			temper = ((~(data[1])) & 0xFF) + 1;
			temper *= (-1000);
			temper -= ((~((int)data[0])) + 1) * 39 / 10;
		} else {
			temper = data[1] & 0x7f;
			temper *= 1000;
			temper += data[0] * 39 / 10;
		}
	} else {
		temper = 20000;
	}

	if (!(fuelgauge->info.pr_cnt % PRINT_COUNT))
		pr_info("%s: TEMPERATURE(%d), data(0x%04x)\n",
			__func__, temper, (data[1] << 8) | data[0]);

	return temper / 100;
}

/* soc should be 0.1% unit */
__visible_for_testing int max77705_fg_read_vfsoc(struct max77705_fuelgauge_data *fuelgauge)
{
	u8 data[2];
	int soc;

	if (max77705_bulk_read(fuelgauge->i2c, VFSOC_REG, 2, data) < 0) {
		pr_err("%s: Failed to read VFSOC_REG\n", __func__);
		return -1;
	}
	soc = ((data[1] * 100) + (data[0] * 100 / 256)) / 10;

	return min(soc, 1000);
}

/* soc should be 0.001% unit */
__visible_for_testing int max77705_fg_read_qh_vfsoc(struct max77705_fuelgauge_data *fuelgauge)
{
	u8 data[2];
	int soc;

	if (max77705_bulk_read(fuelgauge->i2c, VFSOC_REG, 2, data) < 0) {
		pr_err("%s: Failed to read VFSOC_REG\n", __func__);
		return -1;
	}
	soc = ((data[1] * 10000) + (data[0] * 10000 / 256)) / 10;

	return soc;
}

__visible_for_testing int max77705_fg_read_qh(struct max77705_fuelgauge_data *fuelgauge)
{
	u8 data[2];
	u32 temp, sign;
	s32 qh;

	if (max77705_bulk_read(fuelgauge->i2c, QH_REG, 2, data) < 0) {
		pr_err("%s: Failed to read QH_REG\n", __func__);
		return -1;
	}

	temp = ((data[1] << 8) | data[0]) & 0xFFFF;
	if (temp & (0x1 << 15)) {
		sign = NEGATIVE;
		temp = (~temp & 0xFFFF) + 1;
	} else {
		sign = POSITIVE;
	}

	qh = temp * 1000 * fuelgauge->fg_resistor / 2;

	if (sign)
		qh *= -1;

	pr_info("%s : QH(%d)\n", __func__, qh);

	return qh;
}

/* soc should be 0.1% unit */
__visible_for_testing int max77705_fg_read_avsoc(struct max77705_fuelgauge_data *fuelgauge)
{
	u8 data[2];
	int soc;

	if (max77705_bulk_read(fuelgauge->i2c, SOCAV_REG, 2, data) < 0) {
		pr_err("%s: Failed to read SOCAV_REG\n", __func__);
		return -1;
	}
	soc = ((data[1] * 100) + (data[0] * 100 / 256)) / 10;

	return min(soc, 1000);
}

/* soc should be 0.1% unit */
__visible_for_testing int max77705_fg_read_soc(struct max77705_fuelgauge_data *fuelgauge)
{
	u8 data[2];
	int soc;

	if (max77705_bulk_read(fuelgauge->i2c, SOCREP_REG, 2, data) < 0) {
		pr_err("%s: Failed to read SOCREP_REG\n", __func__);
		return -1;
	}
	soc = ((data[1] * 100) + (data[0] * 100 / 256)) / 10;

#ifdef BATTERY_LOG_MESSAGE
	pr_debug("%s: raw capacity (%d)\n", __func__, soc);

	if (!(fuelgauge->info.pr_cnt % PRINT_COUNT)) {
		pr_debug("%s: raw capacity (%d), data(0x%04x)\n",
			 __func__, soc, (data[1] << 8) | data[0]);
		pr_debug("%s: REPSOC (%d), VFSOC (%d), data(0x%04x)\n",
			 __func__, soc / 10,
			 max77705_fg_read_vfsoc(fuelgauge) / 10,
			 (data[1] << 8) | data[0]);
	}
#endif

	return min(soc, 1000);
}

/* soc should be 0.01% unit */
__visible_for_testing int max77705_fg_read_rawsoc(struct max77705_fuelgauge_data *fuelgauge)
{
	u8 data[2];
	int soc;

	if (max77705_bulk_read(fuelgauge->i2c, SOCREP_REG, 2, data) < 0) {
		pr_err("%s: Failed to read SOCREP_REG\n", __func__);
		return -1;
	}
	soc = (data[1] * 100) + (data[0] * 100 / 256);

	pr_debug("%s: raw capacity (0.01%%) (%d)\n", __func__, soc);

	if (!(fuelgauge->info.pr_cnt % PRINT_COUNT))
		pr_debug("%s: raw capacity (%d), data(0x%04x)\n",
			 __func__, soc, (data[1] << 8) | data[0]);

	return min(soc, 10000);
}

__visible_for_testing int max77705_fg_read_fullcap(struct max77705_fuelgauge_data *fuelgauge)
{
	u8 data[2];
	int ret;

	if (max77705_bulk_read(fuelgauge->i2c, FULLCAP_REG, 2, data) < 0) {
		pr_err("%s: Failed to read FULLCAP_REG\n", __func__);
		return -1;
	}
	ret = (data[1] << 8) + data[0];

	return ret * fuelgauge->fg_resistor / 2;
}

__visible_for_testing int max77705_fg_read_fullcaprep(struct max77705_fuelgauge_data *fuelgauge)
{
	u8 data[2];
	int ret;

	if (max77705_bulk_read(fuelgauge->i2c, FULLCAP_REP_REG, 2, data) < 0) {
		pr_err("%s: Failed to read FULLCAP_REP_REG\n", __func__);
		return -1;
	}
	ret = (data[1] << 8) + data[0];

	return ret * fuelgauge->fg_resistor / 2;
}

static int max77705_fg_read_fullcapnom(struct max77705_fuelgauge_data *fuelgauge)
{
	u8 data[2];
	int ret;

	if (max77705_bulk_read(fuelgauge->i2c, FULLCAP_NOM_REG, 2, data) < 0) {
		pr_err("%s: Failed to read FULLCAP_NOM_REG\n", __func__);
		return -1;
	}
	ret = (data[1] << 8) + data[0];

	return ret * fuelgauge->fg_resistor / 2;
}

static int max77705_fg_read_mixcap(struct max77705_fuelgauge_data *fuelgauge)
{
	u8 data[2];
	int ret;

	if (max77705_bulk_read(fuelgauge->i2c, REMCAP_MIX_REG, 2, data) < 0) {
		pr_err("%s: Failed to read REMCAP_MIX_REG\n", __func__);
		return -1;
	}
	ret = (data[1] << 8) + data[0];

	return ret * fuelgauge->fg_resistor / 2;
}

__visible_for_testing int max77705_fg_read_avcap(struct max77705_fuelgauge_data *fuelgauge)
{
	u8 data[2];
	int ret;

	if (max77705_bulk_read(fuelgauge->i2c, REMCAP_AV_REG, 2, data) < 0) {
		pr_err("%s: Failed to read REMCAP_AV_REG\n", __func__);
		return -1;
	}
	ret = (data[1] << 8) + data[0];

	return ret * fuelgauge->fg_resistor / 2;
}

__visible_for_testing int max77705_fg_read_repcap(struct max77705_fuelgauge_data *fuelgauge)
{
	u8 data[2];
	int ret;

	if (max77705_bulk_read(fuelgauge->i2c, REMCAP_REP_REG, 2, data) < 0) {
		pr_err("%s: Failed to read REMCAP_REP_REG\n", __func__);
		return -1;
	}
	ret = (data[1] << 8) + data[0];

	return ret * fuelgauge->fg_resistor / 2;
}

__visible_for_testing int max77705_fg_read_current(struct max77705_fuelgauge_data *fuelgauge,
				    int unit)
{
	u8 data1[2];
	u32 temp, sign;
	s32 i_current;

	if (max77705_bulk_read(fuelgauge->i2c, CURRENT_REG, 2, data1) < 0) {
		pr_err("%s: Failed to read CURRENT_REG\n", __func__);
		return -1;
	}

	temp = ((data1[1] << 8) | data1[0]) & 0xFFFF;
	/* Debug log for abnormal current case */
	if (temp & (0x1 << 15)) {
		sign = NEGATIVE;
		temp = (~temp & 0xFFFF) + 1;
	} else {
		sign = POSITIVE;
	}

	/* 1.5625uV/0.01Ohm(Rsense) = 156.25uA */
	/* 1.5625uV/0.005Ohm(Rsense) = 312.5uA */
	switch (unit) {
	case SEC_BATTERY_CURRENT_UA:
		i_current = temp * 15625 * fuelgauge->fg_resistor / 100;
		break;
	case SEC_BATTERY_CURRENT_MA:
	default:
		i_current = temp * 15625 * fuelgauge->fg_resistor / 100000;
		break;
	}

	if (sign)
		i_current *= -1;

	pr_debug("%s: current=%d%s\n", __func__, i_current,
		(unit == SEC_BATTERY_CURRENT_UA)? "uA" : "mA");

	return i_current;
}

__visible_for_testing int max77705_fg_read_avg_current(struct max77705_fuelgauge_data *fuelgauge,
					int unit)
{
	static int cnt;
	u8 data2[2];
	u32 temp, sign;
	s32 avg_current;
	int vcell;

	if (max77705_bulk_read(fuelgauge->i2c, AVG_CURRENT_REG, 2, data2) < 0) {
		pr_err("%s: Failed to read AVG_CURRENT_REG\n", __func__);
		return -1;
	}

	temp = ((data2[1] << 8) | data2[0]) & 0xFFFF;
	if (temp & (0x1 << 15)) {
		sign = NEGATIVE;
		temp = (~temp & 0xFFFF) + 1;
	} else {
		sign = POSITIVE;
	}

	/* 1.5625uV/0.01Ohm(Rsense) = 156.25uA */
	/* 1.5625uV/0.005Ohm(Rsense) = 312.5uA */
	switch (unit) {
	case SEC_BATTERY_CURRENT_UA:
		avg_current = temp * 15625 * fuelgauge->fg_resistor / 100;
		break;
	case SEC_BATTERY_CURRENT_MA:
	default:
		avg_current = temp * 15625 * fuelgauge->fg_resistor / 100000;
		break;
	}

	if (sign)
		avg_current *= -1;

	vcell = max77705_fg_read_vcell(fuelgauge);
	if ((vcell < 3500) && (cnt < 10) && (avg_current < 0) && fuelgauge->is_charging) {
		avg_current = 1;
		cnt++;
	}

	pr_debug("%s: avg_current=%d%s\n", __func__, avg_current,
		(unit == SEC_BATTERY_CURRENT_UA)? "uA" : "mA");

	return avg_current;
}

__visible_for_testing int max77705_fg_read_isys(struct max77705_fuelgauge_data *fuelgauge,
					int unit)
{
	u8 data1[2];
	u32 temp = 0;
	s32 i_current = 0;
	s32 inow = 0, inow_comp = 0;
	u32 unit_type = 0;

	if (max77705_bulk_read(fuelgauge->i2c, ISYS_REG, 2, data1) < 0) {
		pr_err("%s: Failed to read ISYS_REG\n", __func__);
		return -1;
	}
	temp = ((data1[1] << 8) | data1[0]) & 0xFFFF;

	/* standard value is 2 whitch means 5mOhm */
	if (fuelgauge->fg_resistor != 2) {
		inow = max77705_fg_read_current(fuelgauge, unit);
	}

	if (unit == SEC_BATTERY_CURRENT_UA)
		unit_type = 1;
	else
		unit_type = 1000;

	i_current = temp * 3125 / 10 / unit_type;

	if (fuelgauge->fg_resistor != 2 && i_current != 0) {
		/* inow_comp = 60% x inow if 0.002Ohm */
		inow_comp = (int)((fuelgauge->fg_resistor - 2) * 10 / fuelgauge->fg_resistor) * inow / 10;
		/* i_current must have a value of inow compensated */
		i_current = i_current - inow_comp;
	}
	if (!(fuelgauge->info.pr_cnt % PRINT_COUNT))
		pr_info("%s: isys_current=%d%s\n", __func__, i_current,
			(unit == SEC_BATTERY_CURRENT_UA)? "uA" : "mA");

	return i_current;
}

__visible_for_testing int max77705_fg_read_isys_avg(struct max77705_fuelgauge_data *fuelgauge,
					int unit)
{
	u8 data2[2];
	u32 temp = 0;
	s32 avg_current = 0;
	s32 avg_inow = 0, avg_inow_comp = 0;
	u32 unit_type = 0;

	if (max77705_bulk_read(fuelgauge->i2c, AVGISYS_REG, 2, data2) < 0) {
		pr_err("%s: Failed to read AVGISYS_REG\n", __func__);
		return -1;
	}
	temp = ((data2[1] << 8) | data2[0]) & 0xFFFF;

	/* standard value is 2 whitch means 5mOhm */
	if (fuelgauge->fg_resistor != 2) {
		avg_inow = max77705_fg_read_avg_current(fuelgauge, unit);
	}

	if (unit == SEC_BATTERY_CURRENT_UA)
		unit_type = 1;
	else
		unit_type = 1000;

	avg_current = temp * 3125 / 10 / unit_type;

	if (fuelgauge->fg_resistor != 2 && avg_current != 0) {
		/* inow_comp = 60% x inow if 0.002Ohm */
		avg_inow_comp = (int)((fuelgauge->fg_resistor - 2) * 10 / fuelgauge->fg_resistor) * avg_inow / 10;
		/* i_current must have a value of inow compensated */
		avg_current = avg_current - avg_inow_comp;
	}
	if (!(fuelgauge->info.pr_cnt % PRINT_COUNT))
		pr_info("%s: isys_avg_current=%d%s\n", __func__, avg_current,
			(unit == SEC_BATTERY_CURRENT_UA)? "uA" : "mA");

	return avg_current;
}

__visible_for_testing int max77705_fg_read_iin(struct max77705_fuelgauge_data *fuelgauge,
					int unit)
{
	u8 data1[2];
	u32 temp;
	s32 i_current;

	if (max77705_bulk_read(fuelgauge->i2c, IIN_REG, 2, data1) < 0) {
		pr_err("%s: Failed to read IIN_REG\n", __func__);
		return -1;
	}

	temp = ((data1[1] << 8) | data1[0]) & 0xFFFF;

	/* LSB 0.125mA */
	switch (unit) {
	case SEC_BATTERY_CURRENT_UA:
		i_current = temp * 125;
		break;
	case SEC_BATTERY_CURRENT_MA:
	default:
		i_current = temp * 125 / 1000;
		break;
	}

	if (!(fuelgauge->info.pr_cnt % PRINT_COUNT))
		pr_debug("%s: iin_current=%d%s\n", __func__, i_current,
			(unit == SEC_BATTERY_CURRENT_UA)? "uA" : "mA");

	return i_current;
}

__visible_for_testing int max77705_fg_read_vbyp(struct max77705_fuelgauge_data *fuelgauge)
{
	u8 data[2];
	u32 vbyp, temp;
	u16 w_data;

	if (max77705_bulk_read(fuelgauge->i2c, VBYP_REG, 2, data) < 0) {
		pr_err("%s: Failed to read VBYP_REG\n", __func__);
		return -1;
	}

	w_data = (data[1] << 8) | data[0];

	temp = (w_data & 0xFFF) * 427246;
	vbyp = temp / 1000000;

	temp = ((w_data & 0xF000) >> 4) * 427246;
	temp /= 1000000;
	vbyp += (temp << 4);

	if (!(fuelgauge->info.pr_cnt % PRINT_COUNT))
		pr_info("%s: VBYP(%d), data(0x%04x)\n",
			__func__, vbyp, (data[1] << 8) | data[0]);

	return vbyp;
}

__visible_for_testing int max77705_fg_read_vsys(struct max77705_fuelgauge_data *fuelgauge)
{
	u8 data[2];
	u32 vsys, temp;
	u16 w_data;

	if (max77705_bulk_read(fuelgauge->i2c, VSYS_REG, 2, data) < 0) {
		pr_err("%s: Failed to read VSYS_REG\n", __func__);
		return -1;
	}

	w_data = (data[1] << 8) | data[0];

	temp = (w_data & 0xFFF) * 15625;
	vsys = temp / 100000;

	temp = ((w_data & 0xF000) >> 4) * 15625;
	temp /= 100000;
	vsys += (temp << 4);

	if (!(fuelgauge->info.pr_cnt % PRINT_COUNT))
		pr_info("%s: VSYS(%d), data(0x%04x)\n",
			__func__, vsys, (data[1] << 8) | data[0]);

	return vsys;
}

__visible_for_testing int max77705_fg_read_cycle(struct max77705_fuelgauge_data *fuelgauge)
{
	u8 data[2];
	int ret;

	if (max77705_bulk_read(fuelgauge->i2c, CYCLES_REG, 2, data) < 0) {
		pr_err("%s: Failed to read CYCLES_REG\n", __func__);
		return -1;
	}
	ret = (data[1] << 8) + data[0];

	return ret;
}

__visible_for_testing bool max77705_check_jig_status(struct max77705_fuelgauge_data *fuelgauge)
{
	bool ret = false;

	if (fuelgauge->pdata->jig_gpio) {
		if (fuelgauge->pdata->jig_low_active)
			ret = !gpio_get_value(fuelgauge->pdata->jig_gpio);
		else
			ret = gpio_get_value(fuelgauge->pdata->jig_gpio);
	}
	pr_info("%s: ret(%d)\n", __func__, ret);

	return ret;
}

int max77705_fg_reset_soc(struct max77705_fuelgauge_data *fuelgauge)
{
	u8 data[2];
	int vfocv, fullcap;

	/* delay for current stablization */
	msleep(500);

	pr_info("%s: Before quick-start - VCELL(%d), VFOCV(%d), VfSOC(%d), RepSOC(%d)\n",
		__func__, max77705_fg_read_vcell(fuelgauge),
	     max77705_fg_read_vfocv(fuelgauge),
	     max77705_fg_read_vfsoc(fuelgauge),
	     max77705_fg_read_soc(fuelgauge));
	pr_info("%s: Before quick-start - current(%d), avg current(%d)\n", __func__,
		max77705_fg_read_current(fuelgauge, SEC_BATTERY_CURRENT_MA),
		max77705_fg_read_avg_current(fuelgauge, SEC_BATTERY_CURRENT_MA));

	if (!max77705_check_jig_status(fuelgauge)) {
		pr_info("%s : Return by No JIG_ON signal\n", __func__);
		return 0;
	}

	max77705_write_word(fuelgauge->i2c, CYCLES_REG, 0);

	if (max77705_bulk_read(fuelgauge->i2c, MISCCFG_REG, 2, data) < 0) {
		pr_err("%s: Failed to read MISCCFG_REG\n", __func__);
		return -1;
	}

	data[1] |= (0x1 << 2);
	if (max77705_bulk_write(fuelgauge->i2c, MISCCFG_REG, 2, data) < 0) {
		pr_info("%s: Failed to write MISCCFG_REG\n", __func__);
		return -1;
	}

	msleep(250);
	max77705_write_word(fuelgauge->i2c, FULLCAP_REG,
			    fuelgauge->battery_data->Capacity);
	max77705_write_word(fuelgauge->i2c, FULLCAP_REP_REG,
			    fuelgauge->battery_data->Capacity);
	msleep(500);

	pr_info("%s: After quick-start - VCELL(%d), VFOCV(%d), VfSOC(%d), RepSOC(%d)\n",
		__func__, max77705_fg_read_vcell(fuelgauge),
	     max77705_fg_read_vfocv(fuelgauge),
	     max77705_fg_read_vfsoc(fuelgauge),
	     max77705_fg_read_soc(fuelgauge));
	pr_info("%s: After quick-start - current(%d), avg current(%d)\n", __func__,
		max77705_fg_read_current(fuelgauge, SEC_BATTERY_CURRENT_MA),
		max77705_fg_read_avg_current(fuelgauge, SEC_BATTERY_CURRENT_MA));

	max77705_write_word(fuelgauge->i2c, CYCLES_REG, 0x00a0);

/* P8 is not turned off by Quickstart @3.4V
 * (It's not a problem, depend on mode data)
 * Power off for factory test(File system, etc..)
 */
	vfocv = max77705_fg_read_vfocv(fuelgauge);
	if (vfocv < POWER_OFF_VOLTAGE_LOW_MARGIN) {
		pr_info("%s: Power off condition(%d)\n", __func__, vfocv);
		fullcap = max77705_read_word(fuelgauge->i2c, FULLCAP_REG);

		/* FullCAP * 0.009 */
		max77705_write_word(fuelgauge->i2c, REMCAP_REP_REG,
				    (u16) (fullcap * 9 / 1000));
		msleep(200);
		pr_info("%s: new soc=%d, vfocv=%d\n", __func__,
			max77705_fg_read_soc(fuelgauge), vfocv);
	}

	pr_info("%s: Additional step - VfOCV(%d), VfSOC(%d), RepSOC(%d)\n",
		__func__, max77705_fg_read_vfocv(fuelgauge),
		max77705_fg_read_vfsoc(fuelgauge),
		max77705_fg_read_soc(fuelgauge));

	return 0;
}

int max77705_fg_reset_capacity_by_jig_connection(
			struct max77705_fuelgauge_data *fuelgauge)
{
	union power_supply_propval val;

	val.intval = SEC_BAT_FGSRC_SWITCHING_VSYS;
	psy_do_property("max77705-charger", set,
			POWER_SUPPLY_EXT_PROP_FGSRC_SWITCHING, val);

	val.intval = 1;
	psy_do_property("battery", set, POWER_SUPPLY_PROP_ENERGY_NOW, val);
	pr_info("%s: DesignCap = Capacity - 1 (Jig Connection)\n", __func__);

	return max77705_write_word(fuelgauge->i2c, DESIGNCAP_REG,
				   fuelgauge->battery_data->Capacity - 1);
}

__visible_for_testing int max77705_fg_check_status_reg(struct max77705_fuelgauge_data *fuelgauge)
{
	u8 status_data[2];
	int ret = 0;

	/* 1. Check Smn was generatedread */
	if (max77705_bulk_read(fuelgauge->i2c, STATUS_REG, 2, status_data) < 0) {
		pr_err("%s: Failed to read STATUS_REG\n", __func__);
		return -1;
	}
#ifdef BATTERY_LOG_MESSAGE
	pr_info("%s: addr(0x00), data(0x%04x)\n", __func__,
		(status_data[1] << 8) | status_data[0]);
#endif

	if (status_data[1] & (0x1 << 2))
		ret = 1;

	/* 2. clear Status reg */
	status_data[1] = 0;
	if (max77705_bulk_write(fuelgauge->i2c, STATUS_REG, 2, status_data) < 0) {
		pr_info("%s: Failed to write STATUS_REG\n", __func__);
		return -1;
	}

	return ret;
}

int max77705_get_fuelgauge_value(struct max77705_fuelgauge_data *fuelgauge,
				 int data)
{
	int ret;

	switch (data) {
	case FG_LEVEL:
		ret = max77705_fg_read_soc(fuelgauge);
		break;
	case FG_TEMPERATURE:
		ret = max77705_fg_read_temp(fuelgauge);
		break;
	case FG_VOLTAGE:
		ret = max77705_fg_read_vcell(fuelgauge);
		break;
	case FG_CURRENT:
		ret = max77705_fg_read_current(fuelgauge, SEC_BATTERY_CURRENT_MA);
		break;
	case FG_CURRENT_AVG:
		ret = max77705_fg_read_avg_current(fuelgauge, SEC_BATTERY_CURRENT_MA);
		break;
	case FG_CHECK_STATUS:
		ret = max77705_fg_check_status_reg(fuelgauge);
		break;
	case FG_RAW_SOC:
		ret = max77705_fg_read_rawsoc(fuelgauge);
		break;
	case FG_VF_SOC:
		ret = max77705_fg_read_vfsoc(fuelgauge);
		break;
	case FG_AV_SOC:
		ret = max77705_fg_read_avsoc(fuelgauge);
		break;
	case FG_FULLCAP:
		ret = max77705_fg_read_fullcap(fuelgauge);
		if (ret == -1)
			ret = max77705_fg_read_fullcap(fuelgauge);
		break;
	case FG_FULLCAPNOM:
		ret = max77705_fg_read_fullcapnom(fuelgauge);
		if (ret == -1)
			ret = max77705_fg_read_fullcapnom(fuelgauge);
		break;
	case FG_FULLCAPREP:
		ret = max77705_fg_read_fullcaprep(fuelgauge);
		if (ret == -1)
			ret = max77705_fg_read_fullcaprep(fuelgauge);
		break;
	case FG_MIXCAP:
		ret = max77705_fg_read_mixcap(fuelgauge);
		break;
	case FG_AVCAP:
		ret = max77705_fg_read_avcap(fuelgauge);
		break;
	case FG_REPCAP:
		ret = max77705_fg_read_repcap(fuelgauge);
		break;
	case FG_CYCLE:
		ret = max77705_fg_read_cycle(fuelgauge);
		break;
	case FG_QH:
		ret = max77705_fg_read_qh(fuelgauge);
		break;
	case FG_QH_VF_SOC:
		ret = max77705_fg_read_qh_vfsoc(fuelgauge);
		break;
	case FG_ISYS:
		ret = max77705_fg_read_isys(fuelgauge, SEC_BATTERY_CURRENT_MA);
		break;
	case FG_ISYS_AVG:
		ret = max77705_fg_read_isys_avg(fuelgauge, SEC_BATTERY_CURRENT_MA);
		break;
	case FG_VSYS:
		ret = max77705_fg_read_vsys(fuelgauge);
		break;
	case FG_IIN:
		ret = max77705_fg_read_iin(fuelgauge, SEC_BATTERY_CURRENT_MA);
		break;
	case FG_VBYP:
		ret = max77705_fg_read_vbyp(fuelgauge);
		break;
	default:
		ret = -1;
		break;
	}

	return ret;
}

static void max77705_fg_periodic_read_power(
				struct max77705_fuelgauge_data *fuelgauge)
{
	int isys, isys_avg, vsys, iin, vbyp, qh;

	isys = max77705_get_fuelgauge_value(fuelgauge, FG_ISYS);
	isys_avg = max77705_get_fuelgauge_value(fuelgauge, FG_ISYS_AVG);
	vsys = max77705_get_fuelgauge_value(fuelgauge, FG_VSYS);
	iin = max77705_get_fuelgauge_value(fuelgauge, FG_IIN);
	vbyp = max77705_get_fuelgauge_value(fuelgauge, FG_VBYP);
	qh = max77705_get_fuelgauge_value(fuelgauge, FG_QH);

	pr_info("[FG power] ISYS(%dmA),ISYSAVG(%dmA),VSYS(%dmV),IIN(%dmA),VBYP(%dmV),QH(%d uah),WA(%d)\n",
		isys, isys_avg, vsys, iin, vbyp, qh, fuelgauge->err_cnt);
}

static void max77705_fg_read_power_log(
	struct max77705_fuelgauge_data *fuelgauge)
{
	int vnow, inow;

	vnow = max77705_get_fuelgauge_value(fuelgauge, FG_VOLTAGE);
	inow = max77705_get_fuelgauge_value(fuelgauge, FG_CURRENT);

	pr_info("[FG info] VNOW(%dmV),INOW(%dmA)\n", vnow, inow);
}

int max77705_fg_alert_init(struct max77705_fuelgauge_data *fuelgauge, int soc)
{
	u8 misccgf_data[2], salrt_data[2], config_data[2], talrt_data[2];
	u16 read_data = 0;

	fuelgauge->is_fuel_alerted = false;

	/* Using RepSOC */
	if (max77705_bulk_read(fuelgauge->i2c, MISCCFG_REG, 2, misccgf_data) < 0) {
		pr_err("%s: Failed to read MISCCFG_REG\n", __func__);
		return -1;
	}
	misccgf_data[0] = misccgf_data[0] & ~(0x03);

	if (max77705_bulk_write(fuelgauge->i2c, MISCCFG_REG, 2, misccgf_data) < 0) {
		pr_info("%s: Failed to write MISCCFG_REG\n", __func__);
		return -1;
	}

	/* SALRT Threshold setting */
	salrt_data[1] = 0xff;
	salrt_data[0] = soc;
	if (max77705_bulk_write(fuelgauge->i2c, SALRT_THRESHOLD_REG, 2, salrt_data) < 0) {
		pr_info("%s: Failed to write SALRT_THRESHOLD_REG\n", __func__);
		return -1;
	}

	/* Reset TALRT Threshold setting (disable) */
	talrt_data[1] = 0x7F;
	talrt_data[0] = 0x80;
	if (max77705_bulk_write(fuelgauge->i2c, TALRT_THRESHOLD_REG, 2, talrt_data) < 0) {
		pr_info("%s: Failed to write TALRT_THRESHOLD_REG\n", __func__);
		return -1;
	}

	read_data = max77705_read_word(fuelgauge->i2c, TALRT_THRESHOLD_REG);
	if (read_data != 0x7f80)
		pr_err("%s: TALRT_THRESHOLD_REG is not valid (0x%x)\n",
		       __func__, read_data);

	/* Enable SOC alerts */
	if (max77705_bulk_read(fuelgauge->i2c, CONFIG_REG, 2, config_data) < 0) {
		pr_err("%s: Failed to read CONFIG_REG\n", __func__);
		return -1;
	}
	config_data[0] = config_data[0] | (0x1 << 2);

	if (max77705_bulk_write(fuelgauge->i2c, CONFIG_REG, 2, config_data) < 0) {
		pr_info("%s: Failed to write CONFIG_REG\n", __func__);
		return -1;
	}

	max77705_update_reg(fuelgauge->pmic, MAX77705_PMIC_REG_INTSRC_MASK,
			    ~MAX77705_IRQSRC_FG, MAX77705_IRQSRC_FG);

	pr_info("[%s] SALRT(0x%02x%02x), CONFIG(0x%02x%02x)\n",	__func__,
		salrt_data[1], salrt_data[0], config_data[1], config_data[0]);

	return 1;
}

__visible_for_testing int max77705_get_fg_soc(struct max77705_fuelgauge_data *fuelgauge)
{
	int fg_soc = 0, fg_vcell, avg_current;

	fg_soc = max77705_get_fuelgauge_value(fuelgauge, FG_LEVEL);
	if (fg_soc < 0) {
		pr_info("Can't read soc!!!");
		fg_soc = fuelgauge->info.soc;
	}

	fg_vcell = max77705_get_fuelgauge_value(fuelgauge, FG_VOLTAGE);
	avg_current = max77705_get_fuelgauge_value(fuelgauge, FG_CURRENT_AVG);

	if (fuelgauge->info.is_first_check)
		fuelgauge->info.is_first_check = false;

	fuelgauge->info.soc = fg_soc;
	pr_debug("%s: soc(%d)\n", __func__, fuelgauge->info.soc);

	return fg_soc;
}

__visible_for_testing void max77705_offset_leakage(
				struct max77705_fuelgauge_data *fuelgauge)
{
	// offset coffset
	if (fuelgauge->battery_data->coff_origin != fuelgauge->battery_data->coff_charging) {
		pr_info("%s: modify coffset\n", __func__);
		if (fuelgauge->is_charging) {
			max77705_write_word(fuelgauge->i2c, COFFSET_REG,
				fuelgauge->battery_data->coff_charging);
			pr_info("%s: modify coffset 0x%x\n", __func__, fuelgauge->battery_data->coff_charging);
		} else {
			max77705_write_word(fuelgauge->i2c, COFFSET_REG,
				fuelgauge->battery_data->coff_origin);
			pr_info("%s: modify coffset 0x%x\n", __func__, fuelgauge->battery_data->coff_origin);
		}
	}

	// offset cgain
	if (fuelgauge->battery_data->cgain_origin) {
		pr_info("%s: modify cgain\n", __func__);
		if (fuelgauge->is_charging) {
			max77705_write_word(fuelgauge->i2c, CGAIN_REG,
				fuelgauge->battery_data->cgain_charging);
			pr_info("%s: modify cgain 0x%x\n", __func__, fuelgauge->battery_data->cgain_charging);
		} else {
			max77705_write_word(fuelgauge->i2c, CGAIN_REG,
				fuelgauge->battery_data->cgain_origin);
			pr_info("%s: modify cgain 0x%x\n", __func__, fuelgauge->battery_data->cgain_origin);
		}
	}
}

__visible_for_testing void max77705_offset_leakage_default(
				struct max77705_fuelgauge_data *fuelgauge)
{
	// offset coffset
	if (fuelgauge->battery_data->coff_charging) {
		pr_info("%s: modify coffset\n", __func__);
		max77705_write_word(fuelgauge->i2c, COFFSET_REG,
			fuelgauge->battery_data->coff_charging);
		pr_info("%s: modify coffset 0x%x\n", __func__, fuelgauge->battery_data->coff_charging);
	}

	// offset cgain
	if (fuelgauge->battery_data->cgain_origin) {
		pr_info("%s: modify cgain\n", __func__);
		max77705_write_word(fuelgauge->i2c, CGAIN_REG,
			fuelgauge->battery_data->cgain_origin);
		pr_info("%s: modify cgain 0x%x\n", __func__, fuelgauge->battery_data->cgain_origin);
	}
}

static irqreturn_t max77705_jig_irq_thread(int irq, void *irq_data)
{
	struct max77705_fuelgauge_data *fuelgauge = irq_data;

	pr_info("%s\n", __func__);

	if (max77705_check_jig_status(fuelgauge))
		max77705_fg_reset_capacity_by_jig_connection(fuelgauge);
	else
		pr_info("%s: jig removed\n", __func__);

	return IRQ_HANDLED;
}

bool max77705_fg_init(struct max77705_fuelgauge_data *fuelgauge)
{
	ktime_t current_time;
	struct timespec64 ts;
	u8 data[2] = { 0, 0 };

#if defined(ANDROID_ALARM_ACTIVATED)
	current_time = alarm_get_elapsed_realtime();
#else
	current_time = ktime_get_boottime();
#endif
	ts = ktime_to_timespec64(ktime_get_boottime());

	fuelgauge->info.fullcap_check_interval = ts.tv_sec;
	fuelgauge->info.is_first_check = true;

	if (max77705_bulk_read(fuelgauge->i2c, CONFIG2_REG, 2, data) < 0) {
		pr_err("%s: Failed to read CONFIG2_REG\n", __func__);
	} else if ((data[0] & 0x0F) != 0x05) {
		data[0] &= ~0x2F;
		data[0] |= (0x5 & 0xF); /* ISysNCurr: 11.25 */
		max77705_bulk_write(fuelgauge->i2c, CONFIG2_REG, 2, data);
	}

	/* 0xB2 == modeldata_ver reg */
	if (max77705_read_word(fuelgauge->i2c, 0xB2) != fuelgauge->data_ver) {
		pr_err("%s: fg data_ver miss match. skip verify fg reg\n", __func__);
		fuelgauge->skip_fg_verify = true;
	} else {
		pr_err("%s: fg data_ver match!(0x%x)\n", __func__, fuelgauge->data_ver);
		fuelgauge->skip_fg_verify = false;
	}

	/* NOT using FG for temperature */
	if (fuelgauge->pdata->thermal_source != SEC_BATTERY_THERMAL_SOURCE_FG) {
		if (max77705_bulk_read(fuelgauge->i2c, CONFIG_REG, 2, data) < 0) {
			pr_err("%s : Failed to read CONFIG_REG\n", __func__);
			return false;
		}
		data[1] |= 0x1;

		if (max77705_bulk_write(fuelgauge->i2c, CONFIG_REG, 2, data) < 0) {
			pr_info("%s : Failed to write CONFIG_REG\n", __func__);
			return false;
		}
	} else {
		if (max77705_bulk_read(fuelgauge->i2c, CONFIG_REG, 2, data) < 0) {
			pr_err("%s : Failed to read CONFIG_REG\n", __func__);
			return false;
		}
		data[1] &= 0xFE;
		data[0] |= 0x10;

		if (max77705_bulk_write(fuelgauge->i2c, CONFIG_REG, 2, data) < 0) {
			pr_info("%s : Failed to write CONFIG_REG\n", __func__);
			return false;
		}
	}

	max77705_offset_leakage(fuelgauge);

	return true;
}

bool max77705_fg_fuelalert_init(struct max77705_fuelgauge_data *fuelgauge,
				int soc)
{
	/* 1. Set max77705 alert configuration. */
	if (max77705_fg_alert_init(fuelgauge, soc) > 0)
		return true;
	else
		return false;
}

void max77705_fg_fuelalert_set(struct max77705_fuelgauge_data *fuelgauge,
			       int enable)
{
	u8 config_data[2], status_data[2];

	if (max77705_bulk_read(fuelgauge->i2c, CONFIG_REG, 2, config_data) < 0)
		pr_err("%s: Failed to read CONFIG_REG\n", __func__);

	if (enable)
		config_data[0] |= ALERT_EN;
	else
		config_data[0] &= ~ALERT_EN;

	pr_info("%s: CONFIG(0x%02x%02x)\n", __func__, config_data[1], config_data[0]);

	if (max77705_bulk_write(fuelgauge->i2c, CONFIG_REG, 2, config_data) < 0)
		pr_info("%s: Failed to write CONFIG_REG\n", __func__);

	if (max77705_bulk_read(fuelgauge->i2c, STATUS_REG, 2, status_data) < 0)
		pr_err("%s: Failed to read STATUS_REG\n", __func__);

	if ((status_data[1] & 0x01) && !max77705_get_lpmode() && !fuelgauge->is_charging) {
		pr_info("%s: Battery Voltage is Very Low!! V EMPTY(%d)\n",
			__func__, fuelgauge->vempty_mode);

		if (fuelgauge->vempty_mode != VEMPTY_MODE_HW)
			fuelgauge->vempty_mode = VEMPTY_MODE_SW_VALERT;
#if defined(CONFIG_BATTERY_CISD)
		else {
			if (!fuelgauge->valert_count_flag) {
				union power_supply_propval value;

				value.intval = fuelgauge->vempty_mode;
				psy_do_property("battery", set,
						POWER_SUPPLY_PROP_VOLTAGE_MIN, value);
				fuelgauge->valert_count_flag = true;
			}
		}
#endif
	}
}

bool max77705_fg_fuelalert_process(void *irq_data)
{
	struct max77705_fuelgauge_data *fuelgauge =
	    (struct max77705_fuelgauge_data *)irq_data;

	max77705_fg_fuelalert_set(fuelgauge, 0);

	return true;
}

bool max77705_fg_reset(struct max77705_fuelgauge_data *fuelgauge)
{
	if (!max77705_fg_reset_soc(fuelgauge))
		return true;
	else
		return false;
}

static int max77705_fg_check_capacity_max(
	struct max77705_fuelgauge_data *fuelgauge, int capacity_max)
{
	int cap_max, cap_min;

	cap_max = fuelgauge->pdata->capacity_max;
	cap_min =
		(fuelgauge->pdata->capacity_max - fuelgauge->pdata->capacity_max_margin);

	return (capacity_max < cap_min) ? cap_min :
		((capacity_max >= cap_max) ? cap_max : capacity_max);
}

static void max77705_fg_adjust_capacity_max(
	struct max77705_fuelgauge_data *fuelgauge)
{
	struct timespec64 c_ts = {0, };
	static struct timespec64 old_ts = {0, };

	if (fuelgauge->capacity_max_conv) {
		c_ts = ktime_to_timespec64(ktime_get_boottime());
		pr_info("%s: capacit max conv time(%llu)\n",
			__func__, c_ts.tv_sec - old_ts.tv_sec);

		if ((fuelgauge->capacity_max < fuelgauge->g_capacity_max) &&
			((unsigned long)(c_ts.tv_sec - old_ts.tv_sec) >= 60)) {
			fuelgauge->capacity_max++;
			old_ts = c_ts;
		} else if (fuelgauge->capacity_max >= fuelgauge->g_capacity_max) {
			fuelgauge->g_capacity_max = 0;
			fuelgauge->capacity_max_conv = false;
		}
		pr_info("%s: capacity_max_conv(%d) Capacity Max(%d | %d)\n",
			__func__, fuelgauge->capacity_max_conv,
			fuelgauge->capacity_max, fuelgauge->g_capacity_max);
	}
}

__visible_for_testing unsigned int max77705_fg_get_scaled_capacity(
	struct max77705_fuelgauge_data *fuelgauge, unsigned int soc)
{
	int raw_soc = soc;

	soc = (soc < fuelgauge->pdata->capacity_min) ?
	    0 : ((soc - fuelgauge->pdata->capacity_min) * 1000 /
		 (fuelgauge->capacity_max - fuelgauge->pdata->capacity_min));

	pr_info("%s : capacity_max (%d) scaled capacity(%d.%d), raw_soc(%d.%d)\n",
		__func__, fuelgauge->capacity_max, soc / 10, soc % 10,
		raw_soc / 10, raw_soc % 10);

	return soc;
}

/* capacity is integer */
static unsigned int max77705_fg_get_atomic_capacity(
	struct max77705_fuelgauge_data *fuelgauge, unsigned int soc)
{
	pr_info("%s : NOW(%d), OLD(%d)\n",
		__func__, soc, fuelgauge->capacity_old);

	if (fuelgauge->pdata->capacity_calculation_type &
	    SEC_FUELGAUGE_CAPACITY_TYPE_ATOMIC) {
		if (fuelgauge->capacity_old < soc)
			soc = fuelgauge->capacity_old + 1;
		else if (fuelgauge->capacity_old > soc)
			soc = fuelgauge->capacity_old - 1;
	}

	/* keep SOC stable in abnormal status */
	if (fuelgauge->pdata->capacity_calculation_type &
	    SEC_FUELGAUGE_CAPACITY_TYPE_SKIP_ABNORMAL) {
		if (!fuelgauge->is_charging &&
		    fuelgauge->capacity_old < soc) {
			pr_err("%s: capacity (old %d : new %d)\n",
			       __func__, fuelgauge->capacity_old, soc);
			soc = fuelgauge->capacity_old;
		}
	}

	/* updated old capacity */
	fuelgauge->capacity_old = soc;

	return soc;
}

static int max77705_fg_calculate_dynamic_scale(
	struct max77705_fuelgauge_data *fuelgauge, int capacity, bool scale_by_full)
{
	union power_supply_propval raw_soc_val;

	raw_soc_val.intval = max77705_get_fuelgauge_value(fuelgauge, FG_RAW_SOC) / 10;

	if (raw_soc_val.intval <
		fuelgauge->pdata->capacity_max - fuelgauge->pdata->capacity_max_margin) {
		pr_info("%s: raw soc(%d) is very low, skip routine\n",
			__func__, raw_soc_val.intval);
	} else if (fuelgauge->capacity_max_conv) {
		pr_info("%s: skip dynamic scale routine\n", __func__);
	} else {
		fuelgauge->capacity_max = (raw_soc_val.intval * 100 / (capacity + 1));
		fuelgauge->capacity_old = capacity;

		fuelgauge->capacity_max =
			max77705_fg_check_capacity_max(fuelgauge, fuelgauge->capacity_max);

		pr_info("%s: %d is used for capacity_max, capacity(%d)\n",
			__func__, fuelgauge->capacity_max, capacity);
		if ((capacity == 100) && !fuelgauge->capacity_max_conv && scale_by_full) {
			fuelgauge->capacity_max_conv = true;
			fuelgauge->g_capacity_max = raw_soc_val.intval;
			pr_info("%s: Goal capacity max %d\n", __func__, fuelgauge->g_capacity_max);
		}
	}

	return fuelgauge->capacity_max;
}

__visible_for_testing void max77705_lost_soc_reset(struct max77705_fuelgauge_data *fuelgauge)
{
	fuelgauge->lost_soc.ing = false;
	fuelgauge->lost_soc.prev_raw_soc = -1;
	fuelgauge->lost_soc.prev_remcap = 0;
	fuelgauge->lost_soc.prev_qh = 0;
	fuelgauge->lost_soc.lost_cap = 0;
	fuelgauge->lost_soc.weight = 0;
}
EXPORT_SYMBOL_KUNIT(max77705_lost_soc_reset);

__visible_for_testing void max77705_lost_soc_check_trigger_cond(
	struct max77705_fuelgauge_data *fuelgauge, int raw_soc, int d_raw_soc, int d_remcap, int d_qh)
{
	if (fuelgauge->lost_soc.prev_raw_soc >= fuelgauge->lost_soc.trig_soc ||
		d_raw_soc <= 0 || d_qh < 0)
		return;

	/*
	 * raw soc is jumped over gap_soc
	 * and remcap is decreased more than trig_scale of qh
	 */
	if (d_raw_soc >= fuelgauge->lost_soc.trig_d_soc &&
		d_remcap >= (d_qh * fuelgauge->lost_soc.trig_scale)) {
		fuelgauge->lost_soc.ing = true;
		fuelgauge->lost_soc.lost_cap += d_remcap;

		/* calc weight */
		fuelgauge->lost_soc.weight += d_raw_soc * 10 / fuelgauge->lost_soc.guarantee_soc;
		if (fuelgauge->lost_soc.weight < fuelgauge->lost_soc.min_weight)
			fuelgauge->lost_soc.weight = fuelgauge->lost_soc.min_weight;

		pr_info("%s: trigger: raw_soc(%d->%d), d_raw_soc(%d), d_remcap(%d), d_qh(%d), weight(%d.%d)\n",
			__func__, fuelgauge->lost_soc.prev_raw_soc, raw_soc, d_raw_soc, d_remcap,
			d_qh, fuelgauge->lost_soc.weight / 10, fuelgauge->lost_soc.weight % 10);
	}
}
EXPORT_SYMBOL_KUNIT(max77705_lost_soc_check_trigger_cond);

__visible_for_testing int max77705_lost_soc_calc_soc(
	struct max77705_fuelgauge_data *fuelgauge, int request_soc, int d_qh, int d_remcap)
{
	int lost_soc = 0, gap_cap = 0;
	int vavg = 0, fullcaprep = 0, onecap = 0;

	vavg = max77705_fg_read_avg_vcell(fuelgauge);
	fullcaprep = max77705_fg_read_fullcaprep(fuelgauge);
	if (fullcaprep < 0) {
		fullcaprep = fuelgauge->battery_data->Capacity * fuelgauge->fg_resistor / 2;
		pr_info("%s: ing: fullcaprep is replaced\n", __func__);
	}
	onecap = (fullcaprep / 100) + 1;

	if (d_qh < 0) {
		/* charging status, recover capacity is delta of remcap */
		if (d_remcap < 0)
			gap_cap = d_remcap * (-1);
		else
			gap_cap = d_remcap;
	} else if (d_qh == 0) {
		gap_cap = 1;
	} else {
		gap_cap = (d_qh * fuelgauge->lost_soc.weight / 10);
	}

	if ((vavg < fuelgauge->lost_soc.min_vol) && (vavg > 0) && (gap_cap < onecap)) {
		gap_cap = onecap; /* reduce 1% */
		pr_info("%s: ing: vavg(%d) is under min_vol(%d), reduce cap more(%d)\n",
			__func__, vavg, fuelgauge->lost_soc.min_vol, (fullcaprep / 100));
	}

	fuelgauge->lost_soc.lost_cap -= gap_cap;

	if (fuelgauge->lost_soc.lost_cap > 0) {
		lost_soc = (fuelgauge->lost_soc.lost_cap * 1000) / fullcaprep;
		pr_info("%s: ing: calc_soc(%d), lost_soc(%d), lost_cap(%d), d_qh(%d), d_remcap(%d), weight(%d.%d)\n",
			__func__, request_soc + lost_soc, lost_soc, fuelgauge->lost_soc.lost_cap,
			d_qh, d_remcap, fuelgauge->lost_soc.weight / 10, fuelgauge->lost_soc.weight % 10);
	} else {
		lost_soc = 0;
		max77705_lost_soc_reset(fuelgauge);
		pr_info("%s: done: request_soc(%d), lost_soc(%d), lost_cap(%d)\n",
			__func__, request_soc, lost_soc, fuelgauge->lost_soc.lost_cap);
	}

	return lost_soc;
}
EXPORT_SYMBOL_KUNIT(max77705_lost_soc_calc_soc);

__visible_for_testing int max77705_lost_soc_get(struct max77705_fuelgauge_data *fuelgauge, int request_soc)
{
	int raw_soc, remcap, qh; /* now values */
	int d_raw_soc, d_remcap, d_qh; /* delta between prev values */
	int report_soc;

	/* get current values */
	raw_soc = max77705_get_fuelgauge_value(fuelgauge, FG_RAW_SOC) / 10;
	remcap = max77705_get_fuelgauge_value(fuelgauge, FG_REPCAP);
	qh = max77705_get_fuelgauge_value(fuelgauge, FG_QH) / 1000;

	if (fuelgauge->lost_soc.prev_raw_soc < 0) {
		fuelgauge->lost_soc.prev_raw_soc = raw_soc;
		fuelgauge->lost_soc.prev_remcap = remcap;
		fuelgauge->lost_soc.prev_qh = qh;
		fuelgauge->lost_soc.lost_cap = 0;
		pr_info("%s: init: raw_soc(%d), remcap(%d), qh(%d)\n",
			__func__, raw_soc, remcap, qh);

		return request_soc;
	}

	/* get diff values with prev */
	d_raw_soc = fuelgauge->lost_soc.prev_raw_soc - raw_soc;
	d_remcap = fuelgauge->lost_soc.prev_remcap - remcap;
	d_qh = fuelgauge->lost_soc.prev_qh - qh;

	max77705_lost_soc_check_trigger_cond(fuelgauge, raw_soc, d_raw_soc, d_remcap, d_qh);

	/* backup prev values */
	fuelgauge->lost_soc.prev_raw_soc = raw_soc;
	fuelgauge->lost_soc.prev_remcap = remcap;
	fuelgauge->lost_soc.prev_qh = qh;

	if (!fuelgauge->lost_soc.ing)
		return request_soc;

	report_soc = request_soc + max77705_lost_soc_calc_soc(fuelgauge, request_soc, d_qh, d_remcap);

	if (report_soc > 1000)
		report_soc = 1000;
	if (report_soc < 0)
		report_soc = 0;

	return report_soc;
}

__visible_for_testing bool max77705_fg_check_vempty_recover_time(struct max77705_fuelgauge_data *fuelgauge)
{
	struct timespec64 c_ts = {0, };
	unsigned long vempty_time = 0;

	c_ts = ktime_to_timespec64(ktime_get_boottime());
	if (!fuelgauge->vempty_time)
		fuelgauge->vempty_time = (c_ts.tv_sec) ? c_ts.tv_sec : 1;
	else if (c_ts.tv_sec >= fuelgauge->vempty_time)
		vempty_time = c_ts.tv_sec - fuelgauge->vempty_time;
	else
		vempty_time = 0xFFFFFFFF - fuelgauge->vempty_time + c_ts.tv_sec;

	pr_info("%s: check vempty time(%ld, %ld)\n",
		__func__, fuelgauge->vempty_time, vempty_time);
	return (vempty_time >= fuelgauge->vempty_recover_time);
}

static unsigned int max77705_check_vempty_status(struct max77705_fuelgauge_data *fuelgauge, unsigned int soc)
{
	/* SW/HW V Empty setting */
	if (fuelgauge->using_hw_vempty && fuelgauge->vempty_init_flag) {
		if (fuelgauge->temperature <= (int)fuelgauge->low_temp_limit) { /* at low temperature (under 10'C) */
			if (fuelgauge->raw_capacity <= 50 &&
				(fuelgauge->vempty_mode != VEMPTY_MODE_HW) &&
				max77705_fg_check_vempty_recover_time(fuelgauge)) {
				max77705_fg_set_vempty(fuelgauge, VEMPTY_MODE_HW);
				fuelgauge->vempty_time = 0;
			} else if (fuelgauge->raw_capacity > 50 &&
				fuelgauge->vempty_mode == VEMPTY_MODE_HW) {
				max77705_fg_set_vempty(fuelgauge, VEMPTY_MODE_SW);
				fuelgauge->vempty_time = 0;
			}
		} else { /* at room temperature (over 10'C) */
			if (fuelgauge->using_room_temperature_vempty) {
				if (fuelgauge->raw_capacity <= 500 &&
					(fuelgauge->vempty_mode != VEMPTY_MODE_HW) &&
					max77705_fg_check_vempty_recover_time(fuelgauge)) {
					max77705_fg_set_vempty(fuelgauge, VEMPTY_MODE_HW);
					fuelgauge->vempty_time = 0;
				} else if (fuelgauge->raw_capacity > 500 &&
					fuelgauge->vempty_mode == VEMPTY_MODE_HW) {
					max77705_fg_set_vempty(fuelgauge, VEMPTY_MODE_SW);
					fuelgauge->vempty_time = 0;
				}
			} else {
				if (fuelgauge->vempty_mode != VEMPTY_MODE_HW &&
					max77705_fg_check_vempty_recover_time(fuelgauge)) {
					max77705_fg_set_vempty(fuelgauge, VEMPTY_MODE_HW);
					fuelgauge->vempty_time = 0;
				}
			}
		}
	} else {
		fuelgauge->vempty_time = 0;
	}

	if (!fuelgauge->is_charging && !max77705_get_lpmode() &&
		fuelgauge->vempty_mode == VEMPTY_MODE_SW_VALERT) {
		if (!fuelgauge->vempty_time) {
			pr_info("%s : SW V EMPTY. Decrease SOC\n", __func__);
			if (fuelgauge->capacity_old > 0)
				soc = fuelgauge->capacity_old - 1;
			else
				soc =  0;
		}
	} else if ((fuelgauge->vempty_mode == VEMPTY_MODE_SW_RECOVERY)
		&& (soc == fuelgauge->capacity_old)) {
		fuelgauge->vempty_mode = VEMPTY_MODE_SW;
	}

	return soc;
}

static int max77705_get_fg_ui_soc(struct max77705_fuelgauge_data *fuelgauge, union power_supply_propval *val)
{
	if (val->intval == SEC_FUELGAUGE_CAPACITY_TYPE_RAW) {
		val->intval = max77705_get_fuelgauge_value(fuelgauge, FG_RAW_SOC);
	} else if (val->intval == SEC_FUELGAUGE_CAPACITY_TYPE_CAPACITY_POINT) {
		val->intval = fuelgauge->raw_capacity % 10;
	} else if (val->intval == SEC_FUELGAUGE_CAPACITY_TYPE_DYNAMIC_SCALE) {
		val->intval = fuelgauge->raw_capacity;
	} else {
		val->intval = max77705_get_fg_soc(fuelgauge);

		if (fuelgauge->pdata->capacity_calculation_type &
			(SEC_FUELGAUGE_CAPACITY_TYPE_SCALE |
			 SEC_FUELGAUGE_CAPACITY_TYPE_DYNAMIC_SCALE)) {

			max77705_fg_adjust_capacity_max(fuelgauge);
			val->intval = max77705_fg_get_scaled_capacity(fuelgauge, val->intval);

			if (val->intval > 1010) {
				pr_info("%s: scaled capacity (%d)\n", __func__, val->intval);
				max77705_fg_calculate_dynamic_scale(fuelgauge, 100, false);
			}
		}

		/* capacity should be between 0% and 100%
		 * (0.1% degree)
		 */
		if (val->intval > 1000)
			val->intval = 1000;
		if (val->intval < 0)
			val->intval = 0;

		fuelgauge->raw_capacity = val->intval;

		if (fuelgauge->pdata->capacity_calculation_type &
			SEC_FUELGAUGE_CAPACITY_TYPE_LOST_SOC) {
			val->intval = max77705_lost_soc_get(fuelgauge, fuelgauge->raw_capacity);
		}

		/* get only integer part */
		val->intval /= 10;

		/* check out vempty status and get new soc */
		val->intval = max77705_check_vempty_status(fuelgauge, val->intval);

		/* check whether doing the __pm_relax */
		if ((val->intval > fuelgauge->pdata->fuel_alert_soc) &&
			fuelgauge->is_fuel_alerted) {
			max77705_fg_fuelalert_init(fuelgauge,
				fuelgauge->pdata->fuel_alert_soc);
		}

		/* (Only for atomic capacity)
		 * In initial time, capacity_old is 0.
		 * and in resume from sleep,
		 * capacity_old is too different from actual soc.
		 * should update capacity_old
		 * by val->intval in booting or resume.
		 */
		if (fuelgauge->initial_update_of_soc) {
			fuelgauge->initial_update_of_soc = false;
			if (fuelgauge->vempty_mode != VEMPTY_MODE_SW_VALERT) {
				/* updated old capacity */
				fuelgauge->capacity_old = val->intval;

				return val->intval;
			}
		}

		if (fuelgauge->sleep_initial_update_of_soc) {
			/* updated old capacity in case of resume */
			if (fuelgauge->is_charging ||
				((!fuelgauge->is_charging) && (fuelgauge->capacity_old >= val->intval))) {
				fuelgauge->capacity_old = val->intval;
				fuelgauge->sleep_initial_update_of_soc = false;

				return val->intval;
			}
		}

		if (fuelgauge->pdata->capacity_calculation_type &
			(SEC_FUELGAUGE_CAPACITY_TYPE_ATOMIC |
			 SEC_FUELGAUGE_CAPACITY_TYPE_SKIP_ABNORMAL)) {
			val->intval = max77705_fg_get_atomic_capacity(fuelgauge, val->intval);
		}
	}

	return val->intval;
}

#if defined(CONFIG_EN_OOPS)
static void max77705_set_full_value(struct max77705_fuelgauge_data *fuelgauge,
				    int cable_type)
{
	u16 ichgterm, misccfg, fullsocthr;

	if (is_hv_wireless_type(cable_type) || is_hv_wire_type(cable_type)) {
		ichgterm = fuelgauge->battery_data->ichgterm_2nd;
		misccfg = fuelgauge->battery_data->misccfg_2nd;
		fullsocthr = fuelgauge->battery_data->fullsocthr_2nd;
	} else {
		ichgterm = fuelgauge->battery_data->ichgterm;
		misccfg = fuelgauge->battery_data->misccfg;
		fullsocthr = fuelgauge->battery_data->fullsocthr;
	}

	max77705_write_word(fuelgauge->i2c, ICHGTERM_REG, ichgterm);
	max77705_write_word(fuelgauge->i2c, MISCCFG_REG, misccfg);
	max77705_write_word(fuelgauge->i2c, FULLSOCTHR_REG, fullsocthr);

	pr_info("%s : ICHGTERM(0x%04x) FULLSOCTHR(0x%04x), MISCCFG(0x%04x)\n",
		__func__, ichgterm, misccfg, fullsocthr);
}
#endif

static int max77705_fg_check_initialization_result(
				struct max77705_fuelgauge_data *fuelgauge)
{
	u8 data[2];

	if (max77705_bulk_read(fuelgauge->i2c, FG_INIT_RESULT_REG, 2, data) < 0) {
		pr_err("%s: Failed to read FG_INIT_RESULT_REG\n", __func__);
		return SEC_BAT_ERROR_CAUSE_I2C_FAIL;
	}

	if (data[1] == 0xFF) {
		pr_info("%s : initialization is failed.(0x%02X:0x%04X)\n",
			__func__, FG_INIT_RESULT_REG, data[1] << 8 | data[0]);
		return SEC_BAT_ERROR_CAUSE_FG_INIT_FAIL;
	} else {
		pr_info("%s : initialization is succeed.(0x%02X:0x%04X)\n",
			__func__, FG_INIT_RESULT_REG, data[1] << 8 | data[0]);
	}

	return SEC_BAT_ERROR_CAUSE_NONE;
}

static int max77705_fg_create_attrs(struct device *dev)
{
	int i, rc;

	for (i = 0; i < (int)ARRAY_SIZE(max77705_fg_attrs); i++) {
		rc = device_create_file(dev, &max77705_fg_attrs[i]);
		if (rc)
			goto create_attrs_failed;
	}
	return rc;

create_attrs_failed:
	dev_err(dev, "%s: failed (%d)\n", __func__, rc);
	while (i--)
		device_remove_file(dev, &max77705_fg_attrs[i]);
	return rc;
}

ssize_t max77705_fg_show_attrs(struct device *dev,
				struct device_attribute *attr, char *buf)
{
	struct power_supply *psy = dev_get_drvdata(dev);
	struct max77705_fuelgauge_data *fuelgauge = power_supply_get_drvdata(psy);
	const ptrdiff_t offset = attr - max77705_fg_attrs;
	int i = 0, j = 0;
	u8 addr = 0;
	u16 data = 0;

	dev_info(fuelgauge->dev, "%s: (%ld)\n", __func__, offset);

	switch (offset) {
	case FG_DATA:
		for (j = 0; j <= 0x0F; j++) {
			for (addr = 0; addr < 0x10; addr++) {
				data = max77705_read_word(fuelgauge->i2c, addr + j * 0x10);
				i += scnprintf(buf + i, PAGE_SIZE - i,
						"0x%02x:\t0x%04x\n", addr + j * 0x10, data);
			}
			if (j == 5)
				j = 0x0A;
		}
		break;
	default:
		return -EINVAL;
	}
	return i;
}

ssize_t max77705_fg_store_attrs(struct device *dev,
				struct device_attribute *attr,
				const char *buf, size_t count)
{
	struct power_supply *psy = dev_get_drvdata(dev);
	struct max77705_fuelgauge_data *fuelgauge = power_supply_get_drvdata(psy);
	const ptrdiff_t offset = attr - max77705_fg_attrs;
	int ret = 0, x, y;

	dev_info(fuelgauge->dev, "%s: (%ld)\n", __func__, offset);

	switch (offset) {
	case FG_DATA:
		if (sscanf(buf, "0x%8x 0x%8x", &x, &y) == 2) {
			if (x >= VALRT_THRESHOLD_REG && x <= VFSOC_REG) {
				u8 addr = x;
				u16 data = y;

				if (max77705_write_word(fuelgauge->i2c, addr, data) < 0)
					dev_info(fuelgauge->dev,"%s: addr: 0x%x write fail\n",
						__func__, addr);
			} else {
				dev_info(fuelgauge->dev,"%s: addr: 0x%x is wrong\n",
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

static int max77705_fg_get_property(struct power_supply *psy,
				    enum power_supply_property psp,
				    union power_supply_propval *val)
{
	struct max77705_fuelgauge_data *fuelgauge = power_supply_get_drvdata(psy);
	enum power_supply_ext_property ext_psp = (enum power_supply_ext_property) psp;
	u8 data[2] = { 0, 0 };

	switch ((int)psp) {
		/* Cell voltage (VCELL, mV) */
	case POWER_SUPPLY_PROP_VOLTAGE_NOW:
		val->intval = max77705_get_fuelgauge_value(fuelgauge, FG_VOLTAGE);
		break;
		/* Additional Voltage Information (mV) */
	case POWER_SUPPLY_PROP_VOLTAGE_AVG:
		val->intval = max77705_fg_read_avg_vcell(fuelgauge);
		break;
	case POWER_SUPPLY_PROP_VOLTAGE_OCV:
		val->intval = max77705_fg_read_vfocv(fuelgauge);
		break;
		/* Current */
	case POWER_SUPPLY_PROP_CURRENT_NOW:
		val->intval = max77705_fg_read_current(fuelgauge, val->intval);
		break;
		/* Average Current */
	case POWER_SUPPLY_PROP_CURRENT_AVG:
		switch (val->intval) {
		case SEC_BATTERY_CURRENT_UA:
			val->intval = max77705_fg_read_avg_current(fuelgauge,
							 SEC_BATTERY_CURRENT_UA);
			break;
		case SEC_BATTERY_CURRENT_MA:
		default:
			fuelgauge->current_avg = val->intval =
			    max77705_fg_read_avg_current(fuelgauge,
						SEC_BATTERY_CURRENT_MA);
			break;
		}
		break;
		/* Full Capacity */
	case POWER_SUPPLY_PROP_ENERGY_NOW:
		switch (val->intval) {
		case SEC_BATTERY_CAPACITY_DESIGNED:
			val->intval = max77705_get_fuelgauge_value(fuelgauge, FG_FULLCAP);
			break;
		case SEC_BATTERY_CAPACITY_ABSOLUTE:
			val->intval = max77705_get_fuelgauge_value(fuelgauge, FG_MIXCAP);
			break;
		case SEC_BATTERY_CAPACITY_TEMPERARY:
			val->intval = max77705_get_fuelgauge_value(fuelgauge, FG_AVCAP);
			break;
		case SEC_BATTERY_CAPACITY_CURRENT:
			val->intval = max77705_get_fuelgauge_value(fuelgauge, FG_REPCAP);
			break;
		case SEC_BATTERY_CAPACITY_AGEDCELL:
			val->intval = max77705_get_fuelgauge_value(fuelgauge, FG_FULLCAPNOM);
			break;
		case SEC_BATTERY_CAPACITY_CYCLE:
			val->intval = max77705_get_fuelgauge_value(fuelgauge, FG_CYCLE);
			break;
		case SEC_BATTERY_CAPACITY_FULL:
			val->intval = max77705_get_fuelgauge_value(fuelgauge, FG_FULLCAPREP);
			break;
		case SEC_BATTERY_CAPACITY_QH:
			val->intval = max77705_get_fuelgauge_value(fuelgauge, FG_QH);
			break;
		case SEC_BATTERY_CAPACITY_VFSOC:
			val->intval = max77705_get_fuelgauge_value(fuelgauge, FG_QH_VF_SOC);
			break;
		}
		break;
		/* SOC (%) */
	case POWER_SUPPLY_PROP_CAPACITY:
			val->intval = max77705_get_fg_ui_soc(fuelgauge, val);
		break;
		/* Battery Temperature */
	case POWER_SUPPLY_PROP_TEMP:
		/* Target Temperature */
	case POWER_SUPPLY_PROP_TEMP_AMBIENT:
		val->intval = max77705_get_fuelgauge_value(fuelgauge, FG_TEMPERATURE);
		break;
#if defined(CONFIG_EN_OOPS)
	case POWER_SUPPLY_PROP_CHARGE_FULL_DESIGN:
		return -ENODATA;
#endif
	case POWER_SUPPLY_PROP_ENERGY_FULL:
		{
			int fullcap = max77705_get_fuelgauge_value(fuelgauge, FG_FULLCAPNOM);

			val->intval = fullcap * 100 /
				(fuelgauge->battery_data->Capacity * fuelgauge->fg_resistor / 2);
			pr_info("%s: asoc(%d), fullcap(%d)\n", __func__,
				val->intval, fullcap);
#if !defined(CONFIG_SEC_FACTORY)
			max77705_fg_periodic_read(fuelgauge);
#endif
		}
		break;
	case POWER_SUPPLY_PROP_ENERGY_FULL_DESIGN:
		val->intval = fuelgauge->capacity_max;
		break;
#if defined(CONFIG_BATTERY_AGE_FORECAST)
	case POWER_SUPPLY_PROP_CAPACITY_LEVEL:
		return -ENODATA;
#endif
	case POWER_SUPPLY_PROP_CHARGE_COUNTER:
		val->intval = fuelgauge->raw_capacity *
			(fuelgauge->battery_data->Capacity * fuelgauge->fg_resistor / 2);
		break;
	case POWER_SUPPLY_EXT_PROP_MIN ... POWER_SUPPLY_EXT_PROP_MAX:
		switch (ext_psp) {
		case POWER_SUPPLY_EXT_PROP_MONITOR_WORK:
			max77705_fg_read_power_log(fuelgauge);
			break;
		case POWER_SUPPLY_EXT_PROP_ERROR_CAUSE:
			val->intval = max77705_fg_check_initialization_result(fuelgauge);
			break;
		case POWER_SUPPLY_EXT_PROP_MEASURE_SYS:
			switch (val->intval) {
			case SEC_BATTERY_VSYS:
				val->intval = max77705_fg_read_vsys(fuelgauge);
				break;
			case SEC_BATTERY_ISYS_AVG_UA:
				val->intval = max77705_fg_read_isys_avg(fuelgauge,
								SEC_BATTERY_CURRENT_UA);
				break;
			case SEC_BATTERY_ISYS_AVG_MA:
				val->intval = max77705_fg_read_isys_avg(fuelgauge,
								SEC_BATTERY_CURRENT_MA);
				break;
			case SEC_BATTERY_ISYS_UA:
				val->intval = max77705_fg_read_isys(fuelgauge,
								SEC_BATTERY_CURRENT_UA);
				break;
			case SEC_BATTERY_ISYS_MA:
			default:
				val->intval = max77705_fg_read_isys(fuelgauge,
								SEC_BATTERY_CURRENT_MA);
				break;
			}
			break;
		case POWER_SUPPLY_EXT_PROP_MEASURE_INPUT:
			switch (val->intval) {
			case SEC_BATTERY_VBYP:
				val->intval = max77705_fg_read_vbyp(fuelgauge);
				break;
			case SEC_BATTERY_IIN_UA:
				val->intval = max77705_fg_read_iin(fuelgauge,
								SEC_BATTERY_CURRENT_UA);
				break;
			case SEC_BATTERY_IIN_MA:
			default:
				val->intval = max77705_fg_read_iin(fuelgauge,
								SEC_BATTERY_CURRENT_MA);
				break;
			}
			break;
		case POWER_SUPPLY_EXT_PROP_JIG_GPIO:
			if (fuelgauge->pdata->jig_gpio)
				val->intval = gpio_get_value(fuelgauge->pdata->jig_gpio);
			else
				val->intval = -1;
			pr_info("%s: jig gpio = %d \n", __func__, val->intval);
			break;
		case POWER_SUPPLY_EXT_PROP_FILTER_CFG:
			max77705_bulk_read(fuelgauge->i2c, FILTER_CFG_REG, 2, data);
			val->intval = data[1] << 8 | data[0];
			pr_debug("%s: FilterCFG=0x%04X\n", __func__, data[1] << 8 | data[0]);
			break;
		case POWER_SUPPLY_EXT_PROP_CHARGING_ENABLED:
			val->intval = fuelgauge->is_charging;
			break;
		case POWER_SUPPLY_EXT_PROP_BATTERY_ID:
			if (!val->intval) {
				if (fuelgauge->pdata->bat_gpio_cnt > 0)
					max77705_reset_bat_id(fuelgauge);
				val->intval = fuelgauge->battery_data->battery_id;
				pr_info("%s: bat_id_gpio = %d \n", __func__, val->intval);
			}
#if IS_ENABLED(CONFIG_DUAL_BATTERY)
			else if (val->intval == SEC_DUAL_BATTERY_SUB) {
				if (fuelgauge->pdata->sub_bat_id_gpio >= 0)
					val->intval = gpio_get_value(fuelgauge->pdata->sub_bat_id_gpio);
				else
					val->intval = fuelgauge->battery_data->sub_battery_id;
				pr_info("%s: sub_bat_id_gpio = %d \n", __func__, val->intval);
			}
#endif
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

#if defined(CONFIG_UPDATE_BATTERY_DATA)
static int max77705_fuelgauge_parse_dt(struct max77705_fuelgauge_data *fuelgauge);
#endif
static int max77705_fg_set_property(struct power_supply *psy,
				    enum power_supply_property psp,
				    const union power_supply_propval *val)
{
	struct max77705_fuelgauge_data *fuelgauge = power_supply_get_drvdata(psy);
	enum power_supply_ext_property ext_psp = (enum power_supply_ext_property) psp;
	static bool low_temp_wa;
	u8 data[2] = { 0, 0 };
	u16 reg_data;

	switch ((int)psp) {
	case POWER_SUPPLY_PROP_STATUS:
		break;
	case POWER_SUPPLY_PROP_CHARGE_FULL:
		if (fuelgauge->pdata->capacity_calculation_type &
		    SEC_FUELGAUGE_CAPACITY_TYPE_DYNAMIC_SCALE)
			max77705_fg_calculate_dynamic_scale(fuelgauge, val->intval, true);
		break;
#if defined(CONFIG_EN_OOPS)
	case POWER_SUPPLY_PROP_CHARGE_FULL_DESIGN:
		max77705_set_full_value(fuelgauge, val->intval);
		break;
#endif
	case POWER_SUPPLY_PROP_ONLINE:
		fuelgauge->cable_type = val->intval;
		if (!is_nocharge_type(fuelgauge->cable_type)) {
			/* enable alert */
			if (fuelgauge->vempty_mode >= VEMPTY_MODE_SW_VALERT) {
				max77705_fg_set_vempty(fuelgauge, VEMPTY_MODE_HW);
				fuelgauge->initial_update_of_soc = true;
				max77705_fg_fuelalert_init(fuelgauge,
					fuelgauge->pdata->fuel_alert_soc);
			}
		}
		break;
		/* Battery Temperature */
	case POWER_SUPPLY_PROP_CAPACITY:
		if (val->intval == SEC_FUELGAUGE_CAPACITY_TYPE_RESET) {
			if (!max77705_fg_reset(fuelgauge))
				return -EINVAL;
			fuelgauge->initial_update_of_soc = true;
			if (fuelgauge->pdata->capacity_calculation_type &
				SEC_FUELGAUGE_CAPACITY_TYPE_LOST_SOC)
				max77705_lost_soc_reset(fuelgauge);
		}
		break;
	case POWER_SUPPLY_PROP_TEMP:
		if (val->intval < 0) {
			reg_data = max77705_read_word(fuelgauge->i2c, DESIGNCAP_REG);
			if (reg_data == fuelgauge->battery_data->Capacity) {
				max77705_write_word(fuelgauge->i2c, DESIGNCAP_REG,
						fuelgauge->battery_data->Capacity + 3);
				pr_info("%s: set the low temp reset! temp : %d, capacity : 0x%x, original capacity : 0x%x\n",
					__func__, val->intval, reg_data,
					fuelgauge->battery_data->Capacity);
			}
		}

		if (val->intval < 0 && !low_temp_wa) {
			low_temp_wa = true;
			max77705_write_word(fuelgauge->i2c, 0x29, 0xCEA7);
			pr_info("%s: FilterCFG(0x%0x)\n", __func__,
				max77705_read_word(fuelgauge->i2c, 0x29));
		} else if (val->intval > 30 && low_temp_wa) {
			low_temp_wa = false;
			max77705_write_word(fuelgauge->i2c, 0x29, 0xCEA4);
			pr_info("%s: FilterCFG(0x%0x)\n", __func__,
				max77705_read_word(fuelgauge->i2c, 0x29));
		}
		max77705_fg_write_temp(fuelgauge, val->intval);
		break;
	case POWER_SUPPLY_PROP_TEMP_AMBIENT:
		break;
	case POWER_SUPPLY_PROP_ENERGY_NOW:
		pr_info("%s: POWER_SUPPLY_PROP_ENERGY_NOW\n", __func__);
		max77705_fg_reset_capacity_by_jig_connection(fuelgauge);
		break;
	case POWER_SUPPLY_PROP_ENERGY_FULL_DESIGN:
		pr_info("%s: capacity_max changed, %d -> %d\n",
			__func__, fuelgauge->capacity_max, val->intval);
		fuelgauge->capacity_max =
			max77705_fg_check_capacity_max(fuelgauge, val->intval);
		fuelgauge->initial_update_of_soc = true;
		break;
#if defined(CONFIG_BATTERY_AGE_FORECAST)
	case POWER_SUPPLY_PROP_CAPACITY_LEVEL:
	{
		u16 reg_fullsocthr;
		int val_soc = val->intval;

		if (val->intval > fuelgauge->pdata->full_condition_soc
		    || val->intval <= (fuelgauge->pdata->full_condition_soc - 10)) {
			pr_info("%s: abnormal value(%d). so thr is changed to default(%d)\n",
			     __func__, val->intval, fuelgauge->pdata->full_condition_soc);
			val_soc = fuelgauge->pdata->full_condition_soc;
		}

		reg_fullsocthr = val_soc << 8;
		if (max77705_write_word(fuelgauge->i2c, FULLSOCTHR_REG,
				reg_fullsocthr) < 0) {
			pr_info("%s: Failed to write FULLSOCTHR_REG\n", __func__);
		}
		else {
			reg_fullsocthr =
				max77705_read_word(fuelgauge->i2c, FULLSOCTHR_REG);
			pr_info("%s: FullSOCThr %d%%(0x%04X)\n",
				__func__, val_soc, reg_fullsocthr);
		}
	}
		break;
#endif
	case POWER_SUPPLY_EXT_PROP_MIN ... POWER_SUPPLY_EXT_PROP_MAX:
		switch (ext_psp) {
		case POWER_SUPPLY_EXT_PROP_FILTER_CFG:
			/* Set FilterCFG */
			max77705_bulk_read(fuelgauge->i2c, FILTER_CFG_REG, 2, data);
			pr_debug("%s: FilterCFG=0x%04X\n", __func__, data[1] << 8 | data[0]);
			data[0] &= ~0xF;
			data[0] |= (val->intval & 0xF);
			max77705_bulk_write(fuelgauge->i2c, FILTER_CFG_REG, 2, data);

			max77705_bulk_read(fuelgauge->i2c, FILTER_CFG_REG, 2, data);
			pr_debug("%s: FilterCFG=0x%04X\n", __func__, data[1] << 8 | data[0]);
			break;
		case POWER_SUPPLY_EXT_PROP_CHARGING_ENABLED:
			switch (val->intval) {
			case SEC_BAT_CHG_MODE_BUCK_OFF:
			case SEC_BAT_CHG_MODE_CHARGING_OFF:
				fuelgauge->is_charging = false;
				max77705_offset_leakage(fuelgauge);
				break;
			case SEC_BAT_CHG_MODE_CHARGING:
				fuelgauge->is_charging = true;
				max77705_offset_leakage(fuelgauge);
				break;
			};
			break;
#if defined(CONFIG_UPDATE_BATTERY_DATA)
		case POWER_SUPPLY_EXT_PROP_POWER_DESIGN:
			max77705_fuelgauge_parse_dt(fuelgauge);
			break;
#endif
		default:
			return -EINVAL;
		}
		break;
	default:
		return -EINVAL;
	}
	return 0;
}

static void max77705_fg_isr_work(struct work_struct *work)
{
	struct max77705_fuelgauge_data *fuelgauge =
	    container_of(work, struct max77705_fuelgauge_data, isr_work.work);

	/* process for fuel gauge chip */
	max77705_fg_fuelalert_process(fuelgauge);

	__pm_relax(fuelgauge->fuel_alert_ws);
}

static irqreturn_t max77705_fg_irq_thread(int irq, void *irq_data)
{
	struct max77705_fuelgauge_data *fuelgauge = irq_data;

	pr_info("%s\n", __func__);

	max77705_update_reg(fuelgauge->pmic, MAX77705_PMIC_REG_INTSRC_MASK,
			    MAX77705_IRQSRC_FG, MAX77705_IRQSRC_FG);
	if (fuelgauge->is_fuel_alerted)
		return IRQ_HANDLED;

	__pm_stay_awake(fuelgauge->fuel_alert_ws);
	fuelgauge->is_fuel_alerted = true;
	schedule_delayed_work(&fuelgauge->isr_work, 0);

	return IRQ_HANDLED;
}

#ifdef CONFIG_OF
#define PROPERTY_NAME_SIZE 128
static void max77705_fuelgauge_parse_dt_lost_soc(
	struct max77705_fuelgauge_data *fuelgauge, struct device_node *np)
{
	int ret;

	ret = of_property_read_u32(np, "fuelgauge,lost_soc_trig_soc",
				 &fuelgauge->lost_soc.trig_soc);
	if (ret < 0)
		fuelgauge->lost_soc.trig_soc = 1000; /* 100% */

	ret = of_property_read_u32(np, "fuelgauge,lost_soc_trig_d_soc",
				 &fuelgauge->lost_soc.trig_d_soc);
	if (ret < 0)
		fuelgauge->lost_soc.trig_d_soc = 20; /* 2% */

	ret = of_property_read_u32(np, "fuelgauge,lost_soc_trig_scale",
				 &fuelgauge->lost_soc.trig_scale);
	if (ret < 0)
		fuelgauge->lost_soc.trig_scale = 2; /* 2x */

	ret = of_property_read_u32(np, "fuelgauge,lost_soc_guarantee_soc",
				 &fuelgauge->lost_soc.guarantee_soc);
	if (ret < 0)
		fuelgauge->lost_soc.guarantee_soc = 20; /* 2% */

	ret = of_property_read_u32(np, "fuelgauge,lost_soc_min_vol",
				 &fuelgauge->lost_soc.min_vol);
	if (ret < 0)
		fuelgauge->lost_soc.min_vol = 3200; /* 3.2V */

	ret = of_property_read_u32(np, "fuelgauge,lost_soc_min_weight",
				 &fuelgauge->lost_soc.min_weight);
	if (ret < 0 || fuelgauge->lost_soc.min_weight <= 10)
		fuelgauge->lost_soc.min_weight = 20; /* 2.0 */

	pr_info("%s: trigger soc(%d), d_soc(%d), scale(%d), guarantee_soc(%d), min_vol(%d), min_weight(%d)\n",
		__func__, fuelgauge->lost_soc.trig_soc, fuelgauge->lost_soc.trig_d_soc,
		fuelgauge->lost_soc.trig_scale, fuelgauge->lost_soc.guarantee_soc,
		fuelgauge->lost_soc.min_vol, fuelgauge->lost_soc.min_weight);
}

__visible_for_testing int max77705_get_bat_id(int bat_id[], int bat_gpio_cnt)
{
	int battery_id = 0;
	int i = 0;

	for (i = (bat_gpio_cnt - 1); i >= 0; i--)
		battery_id += bat_id[i] << i;

	return battery_id;
}
EXPORT_SYMBOL_KUNIT(max77705_get_bat_id);

static void max77705_reset_bat_id(struct max77705_fuelgauge_data *fuelgauge)
{
	int bat_id[BAT_GPIO_NO] = {0, };
	int i = 0;

	for (i = 0; i < fuelgauge->pdata->bat_gpio_cnt; i++)
		bat_id[i] = gpio_get_value(fuelgauge->pdata->bat_id_gpio[i]);

	fuelgauge->battery_data->battery_id =
			max77705_get_bat_id(bat_id, fuelgauge->pdata->bat_gpio_cnt);
}

static int max77705_fuelgauge_parse_dt(struct max77705_fuelgauge_data *fuelgauge)
{
	struct device_node *np = of_find_node_by_name(NULL, "max77705-fuelgauge");
	max77705_fuelgauge_platform_data_t *pdata = fuelgauge->pdata;
	int ret, len;
	u8 battery_id = 0;
	int i = 0;
	int bat_id[BAT_GPIO_NO] = {0, };
	const u32 *p;

	/* reset, irq gpio info */
	if (np == NULL) {
		pr_err("%s: np NULL\n", __func__);
	} else {
		ret = of_property_read_u32(np, "fuelgauge,capacity_max",
					   &pdata->capacity_max);
		if (ret < 0)
			pr_err("%s: error reading capacity_max %d\n", __func__, ret);

		ret = of_property_read_u32(np, "fuelgauge,capacity_max_margin",
				&pdata->capacity_max_margin);
		if (ret < 0) {
			pr_err("%s: error reading capacity_max_margin %d\n",
				__func__, ret);
			pdata->capacity_max_margin = 300;
		}

		ret = of_property_read_u32(np, "fuelgauge,capacity_min",
					   &pdata->capacity_min);
		if (ret < 0)
			pr_err("%s: error reading capacity_min %d\n", __func__, ret);

		ret = of_property_read_u32(np, "fuelgauge,capacity_calculation_type",
					&pdata->capacity_calculation_type);
		if (ret < 0)
			pr_err("%s: error reading capacity_calculation_type %d\n",
				__func__, ret);

		ret = of_property_read_u32(np, "fuelgauge,fuel_alert_soc",
				&pdata->fuel_alert_soc);
		if (ret < 0)
			pr_err("%s: error reading pdata->fuel_alert_soc %d\n",
				__func__, ret);

		pdata->repeated_fuelalert = of_property_read_bool(np,
					"fuelgauge,repeated_fuelalert");

		fuelgauge->using_temp_compensation = of_property_read_bool(np,
					"fuelgauge,using_temp_compensation");
		if (fuelgauge->using_temp_compensation) {
			ret = of_property_read_u32(np, "fuelgauge,low_temp_limit",
						 &fuelgauge->low_temp_limit);
			if (ret < 0) {
				pr_err("%s: error reading low temp limit %d\n",
					__func__, ret);
				fuelgauge->low_temp_limit = 0; /* Default: 0'C */
			}

			ret = of_property_read_u32(np,
				"fuelgauge,vempty_recover_time", &fuelgauge->vempty_recover_time);
			if (ret < 0) {
				pr_err("%s: error reading low temp limit %d\n",
					__func__, ret);
				fuelgauge->vempty_recover_time = 0; /* Default: 0 */
			}
		}

		fuelgauge->using_hw_vempty = of_property_read_bool(np,
			"fuelgauge,using_hw_vempty");
		if (fuelgauge->using_hw_vempty) {
			ret = of_property_read_u32(np, "fuelgauge,sw_v_empty_voltage",
				&fuelgauge->battery_data->sw_v_empty_vol);
			if (ret < 0)
				pr_err("%s: error reading sw_v_empty_default_vol %d\n",
					__func__, ret);

			ret = of_property_read_u32(np, "fuelgauge,sw_v_empty_voltage_cisd",
				&fuelgauge->battery_data->sw_v_empty_vol_cisd);
			if (ret < 0) {
				pr_err("%s: error reading sw_v_empty_default_vol_cise %d\n",
					__func__, ret);
				fuelgauge->battery_data->sw_v_empty_vol_cisd = 3100;
			}

			ret = of_property_read_u32(np, "fuelgauge,sw_v_empty_recover_voltage",
					&fuelgauge->battery_data->sw_v_empty_recover_vol);
			if (ret < 0)
				pr_err("%s: error reading sw_v_empty_recover_vol %d\n",
					__func__, ret);

			fuelgauge->using_room_temperature_vempty = of_property_read_bool(np,
				"fuelgauge,using_room_temperature_vempty");
			pr_info("%s : SW V Empty (%d)mV,  SW V Empty recover (%d)mV, room(%d)\n",
				__func__, fuelgauge->battery_data->sw_v_empty_vol,
				fuelgauge->battery_data->sw_v_empty_recover_vol, fuelgauge->using_room_temperature_vempty);
		}

		pdata->jig_gpio = of_get_named_gpio(np, "fuelgauge,jig_gpio", 0);
		if (pdata->jig_gpio >= 0) {
			pdata->jig_irq = gpio_to_irq(pdata->jig_gpio);
			pdata->jig_low_active = of_property_read_bool(np,
							"fuelgauge,jig_low_active");
		} else {
			pr_err("%s: error reading jig_gpio = %d\n",
				__func__, pdata->jig_gpio);
			pdata->jig_gpio = 0;
		}

		ret = of_property_read_u32(np, "fuelgauge,fg_resistor",
					   &fuelgauge->fg_resistor);
		if (ret < 0) {
			pr_err("%s: error reading fg_resistor %d\n", __func__, ret);
			fuelgauge->fg_resistor = 1;
		}
#if defined(CONFIG_EN_OOPS)
		ret = of_property_read_u32(np, "fuelgauge,ichgterm",
					   &fuelgauge->battery_data->ichgterm);
		if (ret < 0)
			pr_err("%s: error reading ichgterm %d\n", __func__, ret);

		ret = of_property_read_u32(np, "fuelgauge,ichgterm_2nd",
					   &fuelgauge->battery_data->ichgterm_2nd);
		if (ret < 0)
			pr_err("%s: error reading ichgterm_2nd %d\n", __func__, ret);

		ret = of_property_read_u32(np, "fuelgauge,misccfg",
					   &fuelgauge->battery_data->misccfg);
		if (ret < 0)
			pr_err("%s: error reading misccfg %d\n", __func__, ret);

		ret = of_property_read_u32(np, "fuelgauge,misccfg_2nd",
					   &fuelgauge->battery_data->misccfg_2nd);
		if (ret < 0)
			pr_err("%s: error reading misccfg_2nd %d\n", __func__, ret);

		ret = of_property_read_u32(np, "fuelgauge,fullsocthr",
					   &fuelgauge->battery_data->fullsocthr);
		if (ret < 0)
			pr_err("%s: error reading fullsocthr %d\n", __func__, ret);

		ret = of_property_read_u32(np, "fuelgauge,fullsocthr_2nd",
					   &fuelgauge->battery_data->fullsocthr_2nd);
		if (ret < 0)
			pr_err("%s: error reading fullsocthr_2nd %d\n", __func__, ret);
#endif

		pdata->bat_gpio_cnt = of_gpio_named_count(np, "fuelgauge,bat_id_gpio");
		/* not run if gpio gpio cnt is less than 1 */
		if (pdata->bat_gpio_cnt > 0) {
			pr_info("%s: Has %d bat_id_gpios\n", __func__, pdata->bat_gpio_cnt);
			if (pdata->bat_gpio_cnt > BAT_GPIO_NO) {
				pr_err("%s: bat_id_gpio, catch out-of bounds array read\n",
						__func__);
				pdata->bat_gpio_cnt = BAT_GPIO_NO;
			}
			for (i = 0; i < pdata->bat_gpio_cnt; i++) {
				pdata->bat_id_gpio[i] = of_get_named_gpio(np, "fuelgauge,bat_id_gpio", i);
				if (pdata->bat_id_gpio[i] >= 0) {
					bat_id[i] = gpio_get_value(pdata->bat_id_gpio[i]);
				} else {
					pr_err("%s: error reading bat_id_gpio = %d\n",
						__func__, pdata->bat_id_gpio[i]);
					bat_id[i] = 0;
				}
			}
			fuelgauge->battery_data->battery_id =
					max77705_get_bat_id(bat_id, pdata->bat_gpio_cnt);
		} else
			fuelgauge->battery_data->battery_id = 0;

		battery_id = fuelgauge->battery_data->battery_id;
		pr_info("%s: battery_id(batt_id:%d) = %d\n", __func__, fuelgauge->battery_data->battery_id, battery_id);

#if IS_ENABLED(CONFIG_DUAL_BATTERY)
		pdata->sub_bat_id_gpio = of_get_named_gpio(np, "fuelgauge,sub_bat_id_gpio", 0);
		if (pdata->sub_bat_id_gpio >= 0) {
			/* 0: BAT_ID LOW, 1: BAT_ID OPEN */
			battery_id = gpio_get_value(pdata->sub_bat_id_gpio);
		} else {
			pr_err("%s: error reading sub_bat_id_gpio = %d\n",
				__func__, pdata->sub_bat_id_gpio);
			battery_id = 0;
		}

		fuelgauge->battery_data->sub_battery_id = battery_id;
		pr_info("%s: sub_battery_id = %d\n", __func__, fuelgauge->battery_data->sub_battery_id);
#endif
		if (fuelgauge->pdata->capacity_calculation_type &
			SEC_FUELGAUGE_CAPACITY_TYPE_LOST_SOC)
			max77705_fuelgauge_parse_dt_lost_soc(fuelgauge, np);

		/* get battery_params node */
		np = of_find_node_by_name(of_node_get(np), "battery_params");
		if (np == NULL) {
			pr_err("%s: Cannot find child node \"battery_params\"\n", __func__);
			return -EINVAL;
		} else {
			char prop_name[PROPERTY_NAME_SIZE];

			snprintf(prop_name, PROPERTY_NAME_SIZE, "battery%d,%s",
				battery_id, "v_empty");
			ret = of_property_read_u32(np, prop_name,
				&fuelgauge->battery_data->V_empty);
			if (ret < 0) {
				pr_err("%s: error reading v_empty %d\n", __func__, ret);
				pr_err("%s: battery data: %d does not exist\n", __func__, battery_id);
				battery_id =  0;
				snprintf(prop_name, PROPERTY_NAME_SIZE, "battery%d,%s",
					battery_id, "v_empty");
				ret = of_property_read_u32(np, prop_name,
					&fuelgauge->battery_data->V_empty);
				if (ret < 0)
					pr_err("%s: error reading v_empty of default battery data %d\n",
						__func__, ret);
			}

			snprintf(prop_name, PROPERTY_NAME_SIZE, "battery%d,%s",
				battery_id, "v_empty_origin");
			ret = of_property_read_u32(np, prop_name,
				&fuelgauge->battery_data->V_empty_origin);
			if (ret < 0)
				pr_err("%s: error reading v_empty_origin %d\n", __func__, ret);

			snprintf(prop_name, PROPERTY_NAME_SIZE, "battery%d,%s",
				battery_id, "capacity");
			ret = of_property_read_u32(np, prop_name,
				&fuelgauge->battery_data->Capacity);
			if (ret < 0)
				pr_err("%s: error reading capacity %d\n", __func__, ret);

			snprintf(prop_name, PROPERTY_NAME_SIZE, "battery%d,%s",
				battery_id, "fg_reset_wa_data");
			len = of_property_count_u32_elems(np, prop_name);
			if (len != FG_RESET_DATA_COUNT) {
				pr_err("%s fg_reset_wa_data is %d < %d, need more data\n",
						 __func__, len, FG_RESET_DATA_COUNT);
				fuelgauge->fg_reset_data = NULL;
			} else {
				fuelgauge->fg_reset_data = kzalloc(sizeof(struct fg_reset_wa), GFP_KERNEL);
				ret = of_property_read_u32_array(np, prop_name,
							(u32 *) fuelgauge->fg_reset_data, FG_RESET_DATA_COUNT);
				if (ret < 0) {
					pr_err("%s failed to read fuelgauge->fg_reset_wa_data: %d\n",
							 __func__, ret);

					kfree(fuelgauge->fg_reset_data);
					fuelgauge->fg_reset_data = NULL;
				}
			}

			snprintf(prop_name, PROPERTY_NAME_SIZE, "battery%d,%s",
				battery_id, "data_ver");
			ret = of_property_read_u32(np, prop_name,
				&fuelgauge->data_ver);
			if (ret < 0) {
				pr_err("%s: error reading data_ver %d\n", __func__, ret);
				fuelgauge->data_ver = 0xff;
			}
			pr_info("%s: fg data_ver (%x)\n", __func__, fuelgauge->data_ver);

			snprintf(prop_name, PROPERTY_NAME_SIZE, "battery%d,%s",
					battery_id, "selected_reg");

			p = of_get_property(np, prop_name, &len);
			if (p) {
				fuelgauge->verify_selected_reg = kzalloc(len, GFP_KERNEL);
				fuelgauge->verify_selected_reg_length = len / (int)sizeof(struct verify_reg);
				pr_err("%s: len= %ld, length= %d, %d\n", __func__,
						sizeof(int) * len, len, fuelgauge->verify_selected_reg_length);
				ret = of_property_read_u32_array(np, prop_name,
						(u32 *)fuelgauge->verify_selected_reg, len / sizeof(u32));
				if (ret) {
					pr_err("%s: failed to read fuelgauge->verify_selected_reg: %d\n",
							__func__, ret);
					kfree(fuelgauge->verify_selected_reg);
					fuelgauge->verify_selected_reg = NULL;
				}
			} else {
				pr_err("%s: there is not selected_reg\n", __func__);
				fuelgauge->verify_selected_reg = NULL;
			}

			snprintf(prop_name, PROPERTY_NAME_SIZE, "battery%d,%s",
					battery_id, "coff_origin");
			ret = of_property_read_u32(np, prop_name,
							&fuelgauge->battery_data->coff_origin);
			if (ret < 0)
				pr_err("%s: error reading coff_origin %d\n", __func__, ret);

			snprintf(prop_name, PROPERTY_NAME_SIZE, "battery%d,%s",
					battery_id, "coff_charging");
			ret = of_property_read_u32(np, prop_name,
							&fuelgauge->battery_data->coff_charging);
			if (ret < 0)
				pr_err("%s: error reading coff_charging %d\n", __func__, ret);

			snprintf(prop_name, PROPERTY_NAME_SIZE, "battery%d,%s",
					battery_id, "cgain_origin");
			ret = of_property_read_u32(np, prop_name,
							&fuelgauge->battery_data->cgain_origin);
			if (ret < 0)
				pr_err("%s: error reading cgain_origin %d\n", __func__, ret);

			snprintf(prop_name, PROPERTY_NAME_SIZE, "battery%d,%s",
					battery_id, "cgain_charging");
			ret = of_property_read_u32(np, prop_name,
							&fuelgauge->battery_data->cgain_charging);
			if (ret < 0)
				pr_err("%s: error reading cgain_charging %d\n", __func__, ret);

			pr_info("%s: V_empty(0x%04x), v_empty_origin(0x%04x), capacity(0x%04x),"
					"coff_origin(0x%04x), coff_charging(0x%04x),"
					"cgain_origin(0x%04x), cgain_charging(0x%04x)\n",
				__func__, fuelgauge->battery_data->V_empty, fuelgauge->battery_data->V_empty_origin, fuelgauge->battery_data->Capacity,
				fuelgauge->battery_data->coff_origin, fuelgauge->battery_data->coff_charging,
				fuelgauge->battery_data->cgain_origin, fuelgauge->battery_data->cgain_charging);
		}

		np = of_find_node_by_name(NULL, "battery");
		ret = of_property_read_u32(np, "battery,thermal_source",
					   &pdata->thermal_source);
		if (ret < 0)
			pr_err("%s: error reading pdata->thermal_source %d\n",
				__func__, ret);

#if defined(CONFIG_BATTERY_AGE_FORECAST)
		ret = of_property_read_u32(np, "battery,full_condition_soc",
					   &pdata->full_condition_soc);
		if (ret) {
			pdata->full_condition_soc = 93;
			pr_info("%s: Full condition soc is Empty\n", __func__);
		}
#endif

		pr_info("%s: thermal: %d, jig_gpio: %d, capacity_max: %d\n"
			"capacity_max_margin: %d, capacity_min: %d\n"
			"calculation_type: 0x%x, fuel_alert_soc: %d,\n"
			"repeated_fuelalert: %d, fg_resistor : %d\n",
			__func__, pdata->thermal_source, pdata->jig_gpio, pdata->capacity_max,
			pdata->capacity_max_margin, pdata->capacity_min,
			pdata->capacity_calculation_type, pdata->fuel_alert_soc,
			pdata->repeated_fuelalert, fuelgauge->fg_resistor);
	}
	pr_info("%s: (%d)\n", __func__, fuelgauge->battery_data->Capacity);

	return 0;
}
#endif

static const struct power_supply_desc max77705_fuelgauge_power_supply_desc = {
	.name = "max77705-fuelgauge",
	.type = POWER_SUPPLY_TYPE_UNKNOWN,
	.properties = max77705_fuelgauge_props,
	.num_properties = ARRAY_SIZE(max77705_fuelgauge_props),
	.get_property = max77705_fg_get_property,
	.set_property = max77705_fg_set_property,
	.no_thermal = true,
};

static int max77705_fuelgauge_probe(struct platform_device *pdev)
{
	struct max77705_dev *max77705 = dev_get_drvdata(pdev->dev.parent);
	struct max77705_platform_data *pdata = dev_get_platdata(max77705->dev);
	max77705_fuelgauge_platform_data_t *fuelgauge_data;
	struct max77705_fuelgauge_data *fuelgauge;
	struct power_supply_config fuelgauge_cfg = { };
	union power_supply_propval raw_soc_val;
	int ret = 0;

	pr_info("%s: max77705 Fuelgauge Driver Loading\n", __func__);

	fuelgauge = kzalloc(sizeof(*fuelgauge), GFP_KERNEL);
	if (!fuelgauge)
		return -ENOMEM;

	fuelgauge_data = kzalloc(sizeof(max77705_fuelgauge_platform_data_t), GFP_KERNEL);
	if (!fuelgauge_data) {
		ret = -ENOMEM;
		goto err_free;
	}

	mutex_init(&fuelgauge->fg_lock);

	fuelgauge->dev = &pdev->dev;
	fuelgauge->pdata = fuelgauge_data;
	fuelgauge->i2c = max77705->fuelgauge;
	fuelgauge->pmic = max77705->i2c;
	fuelgauge->max77705_pdata = pdata;
#if defined(CONFIG_OF)
	fuelgauge->battery_data = kzalloc(sizeof(struct battery_data_t), GFP_KERNEL);
	if (!fuelgauge->battery_data) {
		pr_err("Failed to allocate memory\n");
		ret = -ENOMEM;
		goto err_pdata_free;
	}
	ret = max77705_fuelgauge_parse_dt(fuelgauge);
	if (ret < 0)
		pr_err("%s not found fg dt! ret[%d]\n", __func__, ret);
#endif

	fuelgauge->capacity_max = fuelgauge->pdata->capacity_max;
	fuelgauge->g_capacity_max = 0;
	fuelgauge->capacity_max_conv = false;
	max77705_lost_soc_reset(fuelgauge);

	raw_soc_val.intval = max77705_get_fuelgauge_value(fuelgauge, FG_RAW_SOC) / 10;

	if (raw_soc_val.intval > fuelgauge->capacity_max)
		max77705_fg_calculate_dynamic_scale(fuelgauge, 100, false);

	if (!max77705_fg_init(fuelgauge)) {
		pr_err("%s: Failed to Initialize Fuelgauge\n", __func__);
		ret = -ENODEV;
		goto err_data_free;
	}

	/* SW/HW init code. SW/HW V Empty mode must be opposite ! */
	fuelgauge->vempty_init_flag = false;	/* default value */
	pr_info("%s: SW/HW V empty init\n", __func__);
	max77705_fg_set_vempty(fuelgauge, VEMPTY_MODE_SW);

	fuelgauge_cfg.drv_data = fuelgauge;

	fuelgauge->psy_fg = power_supply_register(&pdev->dev,
				  &max77705_fuelgauge_power_supply_desc,
				  &fuelgauge_cfg);
	if (IS_ERR(fuelgauge->psy_fg)) {
		ret = PTR_ERR(fuelgauge->psy_fg);
		pr_err("%s: Failed to Register psy_fg(%d)\n", __func__, ret);
		goto err_data_free;
	}

	fuelgauge->fg_irq = pdata->irq_base + MAX77705_FG_IRQ_ALERT;
	pr_info("[%s]IRQ_BASE(%d) FG_IRQ(%d)\n",
		__func__, pdata->irq_base, fuelgauge->fg_irq);

	fuelgauge->is_fuel_alerted = false;
	if (fuelgauge->pdata->fuel_alert_soc >= 0) {
		if (max77705_fg_fuelalert_init(fuelgauge,
				fuelgauge->pdata->fuel_alert_soc)) {
			fuelgauge->fuel_alert_ws = wakeup_source_register(&pdev->dev, "fuel_alerted");
			if (fuelgauge->fg_irq) {
				INIT_DELAYED_WORK(&fuelgauge->isr_work,
					max77705_fg_isr_work);

				ret = request_threaded_irq(fuelgauge->fg_irq, NULL,
						max77705_fg_irq_thread,
						IRQF_TRIGGER_FALLING | IRQF_ONESHOT,
						"fuelgauge-irq", fuelgauge);
				if (ret) {
					pr_err("%s: Failed to Request IRQ (fg_irq)\n", __func__);
					goto err_supply_unreg;
				}
			}
		} else {
			pr_err("%s: Failed to Initialize Fuel-alert\n", __func__);
			goto err_supply_unreg;
		}
	}

	if (fuelgauge->pdata->jig_gpio) {
		ret = request_threaded_irq(fuelgauge->pdata->jig_irq, NULL,
					max77705_jig_irq_thread,
					(fuelgauge->pdata->jig_low_active ?
					IRQF_TRIGGER_FALLING : IRQF_TRIGGER_RISING)
					| IRQF_ONESHOT,
					"jig-irq", fuelgauge);
		if (ret) {
			pr_info("%s: Failed to Request IRQ (jig_gpio)\n", __func__);
			goto err_supply_unreg;
		}

		/* initial check for the JIG */
		if (max77705_check_jig_status(fuelgauge))
			max77705_fg_reset_capacity_by_jig_connection(fuelgauge);
	}

	ret = max77705_fg_create_attrs(&fuelgauge->psy_fg->dev);
	if (ret) {
		dev_err(fuelgauge->dev,"%s : Failed to create_attrs\n", __func__);
		goto err_irq;
	}

	fuelgauge->err_cnt = 0;
	fuelgauge->sleep_initial_update_of_soc = false;
	fuelgauge->initial_update_of_soc = true;
#if defined(CONFIG_BATTERY_CISD)
	fuelgauge->valert_count_flag = false;
#endif
	platform_set_drvdata(pdev, fuelgauge);

	pr_info("%s: max77705 Fuelgauge Driver Loaded\n", __func__);
	return 0;

err_irq:
	free_irq(fuelgauge->fg_irq, fuelgauge);
	free_irq(fuelgauge->pdata->jig_irq, fuelgauge);
err_supply_unreg:
	power_supply_unregister(fuelgauge->psy_fg);
	wakeup_source_unregister(fuelgauge->fuel_alert_ws);
err_data_free:
#if defined(CONFIG_OF)
	kfree(fuelgauge->battery_data);
#endif
err_pdata_free:
	kfree(fuelgauge_data);
	mutex_destroy(&fuelgauge->fg_lock);
err_free:
	kfree(fuelgauge);

	return ret;
}

static int max77705_fuelgauge_remove(struct platform_device *pdev)
{
	struct max77705_fuelgauge_data *fuelgauge = platform_get_drvdata(pdev);

	pr_info("%s: ++\n", __func__);

	if (fuelgauge) {
		if (fuelgauge->psy_fg)
			power_supply_unregister(fuelgauge->psy_fg);

		free_irq(fuelgauge->fg_irq, fuelgauge);
		free_irq(fuelgauge->pdata->jig_irq, fuelgauge);
		wakeup_source_unregister(fuelgauge->fuel_alert_ws);
#if defined(CONFIG_OF)
		kfree(fuelgauge->battery_data);
#endif
		kfree(fuelgauge->pdata);
		mutex_destroy(&fuelgauge->fg_lock);
		kfree(fuelgauge);
	}

	pr_info("%s: --\n", __func__);

	return 0;
}

#if defined CONFIG_PM
static int max77705_fuelgauge_suspend(struct device *dev)
{
	return 0;
}

static int max77705_fuelgauge_resume(struct device *dev)
{
	struct max77705_fuelgauge_data *fuelgauge = dev_get_drvdata(dev);

	fuelgauge->sleep_initial_update_of_soc = true;

	return 0;
}
#else

#define max77705_fuelgauge_suspend NULL
#define max77705_fuelgauge_resume NULL

#endif

static void max77705_fuelgauge_shutdown(struct platform_device *pdev)
{
	struct max77705_fuelgauge_data *fuelgauge = platform_get_drvdata(pdev);

	pr_info("%s: ++\n", __func__);

	if (fuelgauge) {
		if (fuelgauge->i2c) {
			max77705_offset_leakage_default(fuelgauge);
		}
		if (fuelgauge->fg_irq)
			free_irq(fuelgauge->fg_irq, fuelgauge);
		if (fuelgauge->pdata->jig_gpio)
			free_irq(fuelgauge->pdata->jig_irq, fuelgauge);

		cancel_delayed_work(&fuelgauge->isr_work);
	}

	pr_info("%s: --\n", __func__);
}

static SIMPLE_DEV_PM_OPS(max77705_fuelgauge_pm_ops, max77705_fuelgauge_suspend,
			 max77705_fuelgauge_resume);

static struct platform_driver max77705_fuelgauge_driver = {
	.driver = {
		   .name = "max77705-fuelgauge",
		   .owner = THIS_MODULE,
#ifdef CONFIG_PM
		   .pm = &max77705_fuelgauge_pm_ops,
#endif
	},
	.probe = max77705_fuelgauge_probe,
	.remove = max77705_fuelgauge_remove,
	.shutdown = max77705_fuelgauge_shutdown,
};

static int __init max77705_fuelgauge_init(void)
{
	pr_info("%s:\n", __func__);
	return platform_driver_register(&max77705_fuelgauge_driver);
}

static void __exit max77705_fuelgauge_exit(void)
{
	platform_driver_unregister(&max77705_fuelgauge_driver);
}

module_init(max77705_fuelgauge_init);
module_exit(max77705_fuelgauge_exit);

MODULE_DESCRIPTION("Samsung max77705 Fuel Gauge Driver");
MODULE_AUTHOR("Samsung Electronics");
MODULE_LICENSE("GPL");

