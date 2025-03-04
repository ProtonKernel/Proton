/*
 * sm5451_charger.c - SM5451 Charger device driver for SAMSUNG platform
 *
 * Copyright (C) 2020 SiliconMitus Co.Ltd
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#include <linux/slab.h>
#include <linux/debugfs.h>
#include <linux/seq_file.h>
#include <linux/power_supply.h>
#if defined (CONFIG_OF)
#include <linux/of_device.h>
#include <linux/of_gpio.h>
#include <linux/pinctrl/consumer.h>
#include <linux/pm_runtime.h>
#endif /* CONFIG_OF */
#if IS_ENABLED(CONFIG_BATTERY_SAMSUNG)
#include "../../common/sec_charging_common.h"
#include "../../common/sec_direct_charger.h"
#endif
#include "sm5451_charger.h"
#if IS_ENABLED(CONFIG_SEC_ABC)
#include <linux/sti/abc_common.h>
#endif

#define SM5451_DC_VERSION  "UG3"

static int sm5451_read_reg(struct sm5451_charger *sm5451, u8 reg, u8 *dest)
{
	int cnt, ret;

	for (cnt = 0; cnt < 3; ++cnt) {
		ret = i2c_smbus_read_byte_data(sm5451->i2c, reg);
		if (ret < 0) {
			dev_err(sm5451->dev, "%s: fail to i2c_read(ret=%d)\n", __func__, ret);
		} else {
			break;
		}
		if (cnt == 0)
			msleep(30);
	}

	if (ret < 0) {
#if IS_ENABLED(CONFIG_SEC_ABC) && !defined(CONFIG_SEC_FACTORY)
		sec_abc_send_event("MODULE=battery@WARN=dc_i2c_fail");
#endif
		return ret;
	} else {
		*dest = (ret & 0xff);
	}

	return 0;
}

int sm5451_bulk_read(struct sm5451_charger *sm5451, u8 reg, int count, u8 *buf)
{
	int cnt, ret;

	for (cnt = 0; cnt < 3; ++cnt) {
		ret = i2c_smbus_read_i2c_block_data(sm5451->i2c, reg, count, buf);
		if (ret < 0) {
			dev_err(sm5451->dev, "%s: fail to i2c_bulk_read(ret=%d)\n", __func__, ret);
		} else {
			break;
		}
		if (cnt == 0)
			msleep(30);
	}

#if IS_ENABLED(CONFIG_SEC_ABC) && !defined(CONFIG_SEC_FACTORY)
	if (ret < 0)
		sec_abc_send_event("MODULE=battery@WARN=dc_i2c_fail");
#endif

	return ret;
}

static int sm5451_write_reg(struct sm5451_charger *sm5451, u8 reg, u8 value)
{
	int cnt, ret;

	for (cnt = 0; cnt < 3; ++cnt) {
		ret = i2c_smbus_write_byte_data(sm5451->i2c, reg, value);
		if (ret < 0) {
			dev_err(sm5451->dev, "%s: fail to i2c_write(ret=%d)\n", __func__, ret);
		} else {
			break;
		}
		if (cnt == 0)
			msleep(30);
	}

#if IS_ENABLED(CONFIG_SEC_ABC) && !defined(CONFIG_SEC_FACTORY)
	if (ret < 0)
		sec_abc_send_event("MODULE=battery@WARN=dc_i2c_fail");
#endif

	return ret;
}

static int sm5451_update_reg(struct sm5451_charger *sm5451, u8 reg, 
                                u8 val, u8 mask, u8 pos)
{
	int ret;
	u8 old_val;

	mutex_lock(&sm5451->i2c_lock);

	ret = sm5451_read_reg(sm5451, reg, &old_val);
	if (ret == 0) {
		u8 new_val = (val & mask) << pos | (old_val & ~(mask << pos));
		ret = sm5451_write_reg(sm5451, reg, new_val);
	}

	mutex_unlock(&sm5451->i2c_lock);

	return ret;
}

static u8 sm5451_get_flag_status(struct sm5451_charger *sm5451, u8 flag_addr)
{
	u8 reg;

	/* read twice to get correct value */
	mutex_lock(&sm5451->i2c_lock);
	sm5451_read_reg(sm5451, flag_addr, &reg);
	sm5451_read_reg(sm5451, flag_addr, &reg);
	mutex_unlock(&sm5451->i2c_lock);

	return reg;
}

static u32 sm5451_get_vbatreg(struct sm5451_charger *sm5451)
{
	u8 reg;
	u32 vbatovp, offset;

	sm5451_read_reg(sm5451, SM5451_REG_VBAT_OVP, &reg);
	vbatovp = 4000 + ((reg & 0x7F) * 10);
	sm5451_read_reg(sm5451, SM5451_REG_REG1, &reg);
	offset = ((reg & 0x3) * 50) + 50;

	dev_info(sm5451->dev, "%s: vbatovp=%d, offset=%d\n",
		__func__, vbatovp, offset);

	return (vbatovp - offset);
}

static int sm5451_set_vbatreg(struct sm5451_charger *sm5451, u32 vbatreg)
{
	u8 reg;
	u32 vbatovp, offset;

	sm5451_read_reg(sm5451, SM5451_REG_REG1, &reg);
	offset = ((reg & 0x3) * 50) + 50;
	vbatovp = vbatreg + offset;
	reg = (vbatovp - 4000) / 10;

	dev_info(sm5451->dev, "%s: vbatovp=%d, offset=%d reg=0x%x\n",
		__func__, vbatovp, offset, reg);

	return sm5451_update_reg(sm5451, SM5451_REG_VBAT_OVP, reg, 0x7F, 0);
}

static u32 sm5451_get_ibuslim(struct sm5451_charger *sm5451)
{
	u8 reg;
	u32 ibusocp, offset;

	sm5451_read_reg(sm5451, SM5451_REG_IBUS_PROT, &reg);
	ibusocp = 500 + ((reg & 0x3F) * 100);
	sm5451_read_reg(sm5451, SM5451_REG_CNTL5, &reg);
	offset = ((reg & 0x3) * 100) + 100;

	dev_info(sm5451->dev, "%s: ibusocp=%d, offset=%d\n",
		__func__, ibusocp, offset);

	return ibusocp - offset;
}

static int sm5451_set_ibuslim(struct sm5451_charger *sm5451, u32 ibuslim)
{
	u8 reg;
	u32 ibusocp, offset;

	sm5451_read_reg(sm5451, SM5451_REG_CNTL5, &reg);
	offset = ((reg & 0x3) * 100) + 100;
	ibusocp = ibuslim + offset;
	reg = (ibusocp - 500) / 100;

	dev_info(sm5451->dev, "%s: ibusocp=%d, offset=%d reg=0x%x\n",
		__func__, ibusocp, offset, reg);

	return sm5451_update_reg(sm5451, SM5451_REG_IBUS_PROT, reg, 0x3F, 0);
}

static int sm5451_set_freq(struct sm5451_charger *sm5451, u8 freq)
{
	return sm5451_update_reg(sm5451, SM5451_REG_CNTL2, freq, 0x7, 5);
}

static int sm5451_set_op_mode(struct sm5451_charger *sm5451, u8 op_mode)
{
	return sm5451_update_reg(sm5451, SM5451_REG_CNTL1, op_mode, 0x7, 4);
}

static u8 sm5451_get_op_mode(struct sm5451_charger *sm5451)
{
	u8 reg;

	sm5451_read_reg(sm5451, SM5451_REG_CNTL1, &reg);

	return (reg >> 4) & 0x7;
}

static int sm5451_set_wdt_timer(struct sm5451_charger *sm5451, u8 tmr)
{
	return sm5451_update_reg(sm5451, SM5451_REG_CNTL1, tmr, 0x7, 0);
}

static u8 sm5451_get_wdt_timer(struct sm5451_charger *sm5451)
{
	u8 reg = 0x0;

	sm5451_read_reg(sm5451, SM5451_REG_CNTL1, &reg);

	return reg & 0x7;
}

static int sm5451_kick_wdt(struct sm5451_charger *sm5451)
{
	return sm5451_update_reg(sm5451, SM5451_REG_CNTL2, 0x1, 0x1, 0);
}

static int sm5451_set_adcmode(struct sm5451_charger *sm5451, u8 mode)
{
	return sm5451_update_reg(sm5451, SM5451_REG_ADC_CNTL, mode, 0x1, 6);
}

static int sm5451_enable_adc(struct sm5451_charger *sm5451, bool enable)
{
	return sm5451_update_reg(sm5451, SM5451_REG_ADC_CNTL, enable, 0x1, 7);
}

static int sm5451_enable_adc_oneshot(struct sm5451_charger *sm5451, u8 enable)
{
	int ret, cnt;
	u8 reg;

	if(enable) {
		for (cnt = 0; cnt < 3; ++cnt) {
			ret = sm5451_write_reg(sm5451, SM5451_REG_ADC_CNTL, 0xC0);
			/* ADC update time */
			msleep(25);
			sm5451_read_reg(sm5451, SM5451_REG_FLAG3, &reg);
			if ((reg & SM5451_FLAG3_ADCDONE) == 0)
				dev_info(sm5451->dev, "%s: fail to adc done(FLAG3=0x%02X)\n", __func__, reg);
			else
				break;
		}
	} else {
		msleep(30);
		ret = sm5451_write_reg(sm5451, SM5451_REG_ADC_CNTL, 0x0);
	}

	return ret;
}

static int sm5451_get_enadc(struct sm5451_charger *sm5451)
{
	u8 reg = 0x0;

	sm5451_read_reg(sm5451, SM5451_REG_ADC_CNTL, &reg);

	return (reg >> 7) & 0x1;
}

static int sm5451_sw_reset(struct sm5451_charger *sm5451)
{
	u8 i, reg;

	sm5451_update_reg(sm5451, SM5451_REG_CNTL1, 1, 0x1, 7);     /* Do SW Reset */

	for (i = 0; i < 0xff; ++i) {
		usleep_range(1000, 2000);
		sm5451_read_reg(sm5451, SM5451_REG_CNTL1, &reg);
		if (!((reg >> 7) & 0x1)) {
			break;
		}
	}

	if (i == 0xff) {
		dev_err(sm5451->dev, "%s: didn't clear reset bit\n", __func__);
		return -EBUSY;
	}
	return 0;
}

static void sm5451_init_reg_param(struct sm5451_charger *sm5451)
{
	sm5451_set_wdt_timer(sm5451, WDT_TIMER_S_40);
	sm5451_set_freq(sm5451, 0x3);                                   /* FREQ : 750kHz */
	sm5451_write_reg(sm5451, SM5451_REG_VBUSOVP, 0xC6);             /* VBUS_OVP=11V */
	sm5451_write_reg(sm5451, SM5451_REG_IBAT_OCP, 0x7C);            /* IBATOCP = disabled */
	sm5451_write_reg(sm5451, SM5451_REG_REG1, 0x07);                /* IBATREG = disabled, VBATREG = 200mV below VBATOVP */
	sm5451_write_reg(sm5451, SM5451_REG_CNTL3, 0xA3);               /* RLTVOVP = disabled, RLTVUVP = 1.01x */
	sm5451_write_reg(sm5451, SM5451_REG_CNTL4, 0x00);               /* VDSQRBP = disabled */
	sm5451_write_reg(sm5451, SM5451_REG_IBATOCP_DG, 0x88);          /* Deglitch for IBATOCP & IBUSOCP = 50us to 8ms */
	sm5451_write_reg(sm5451, SM5451_REG_VDSQRB_DG, 0x05);			/* Deglitch for VDSQRB = 5ms to 200ms */
	sm5451_write_reg(sm5451, SM5451_REG_CNTL5, 0x3F);         		/* IBUSOCP_RVS = enable, threshold = 4.5A */
}

static struct sm_dc_info *select_sm_dc_info(struct sm5451_charger *sm5451)
{
	return sm5451->pps_dc;
}

static int sm5451_convert_adc(struct sm5451_charger *sm5451, u8 index)
{
	u8 regs[4] = {0x0, };
	u8 ret = 0x0;
	int adc, cnt;

#if !defined(CONFIG_SEC_FACTORY) && !defined(CONFIG_DUAL_BATTERY_CELL_SENSING)
	struct sm_dc_info *sm_dc = select_sm_dc_info(sm5451);

	if (sm_dc_get_current_state(sm_dc) < SM_DC_CHECK_VBAT) {
		/* Didn't worked ADC block during on CHG-OFF status */
		return 0;
	}
#endif

	for (cnt = 0; cnt < 2; ++cnt) {
		if (sm5451_get_enadc(sm5451) == 0)
			sm5451_enable_adc_oneshot(sm5451, 1);

		switch (index) {
		case SM5451_ADC_THEM:   /* unit - mV */
			sm5451_bulk_read(sm5451, SM5451_REG_THEM_ADC_H, 2, regs);
			adc = (((int)(regs[0] & 0x0F) << 8) + regs[1] + 1) >> 1;
			break;
		case SM5451_ADC_TDIE:   /* unit - C */
			sm5451_read_reg(sm5451, SM5451_REG_TDIE_ADC, regs);
			adc = -40 + regs[0];
			break;
		case SM5451_ADC_VBUS:
			sm5451_bulk_read(sm5451, SM5451_REG_VBUS_ADC_H, 2, regs);
			adc = (((int)(regs[0] & 0x0F) << 8) + regs[1]) * 4;
			break;
		case SM5451_ADC_IBUS:
			sm5451_bulk_read(sm5451, SM5451_REG_IBUS_ADC_H, 2, regs);
			adc = (((int)(regs[0] & 0x0F) << 8) + regs[1]) * 2;
			break;
		case SM5451_ADC_VBAT:
			sm5451_bulk_read(sm5451, SM5451_REG_VBAT_ADC_H, 2, regs);
			adc = (((int)(regs[0] & 0x0F) << 8) + regs[1]) * 2;
			break;
		case SM5451_ADC_IBAT:
			sm5451_bulk_read(sm5451, SM5451_REG_IBAT_ADC_H, 2, regs);
			adc = (((int)(regs[0] & 0x0F) << 8) + regs[1]) * 2;
			break;
		default:
			adc = 0;
		}

		/* prevent for reset of register */
		ret = sm5451_get_wdt_timer(sm5451);
		if(ret != WDT_TIMER_S_40) {
			dev_err(sm5451->dev, "%s: detected REG Reset condition(ret=0x%x)\n", __func__, ret);
			sm5451_init_reg_param(sm5451);
		} else {
			break;
		}
	}
	dev_info(sm5451->dev, "%s: index(%d)=[0x%02X,0x%02X], adc=%d\n",
		__func__, index, regs[0], regs[1], adc);

	return adc;
}

static void sm5451_print_regmap(struct sm5451_charger *sm5451)
{
	u8 print_reg_num, regs[64] = {0x0, };
	char temp_buf[128] = {0,};
	char reg_addr[16] = {0x0C, 0x0E, 0x10, 0x11, 0x60, 0x61, 0x62, 0x64, };
	u8 reg_data;
	int i;

	print_reg_num = SM5451_REG_REG1 - SM5451_REG_CNTL1;
	sm5451_bulk_read(sm5451, SM5451_REG_CNTL1, print_reg_num, regs);
	for (i = 0; i < print_reg_num; ++i) {
		sprintf(temp_buf+strlen(temp_buf), "0x%02X[0x%02X],", SM5451_REG_CNTL1 + i, regs[i]);
	}
	pr_info("sm5451-charger: regmap: %s\n", temp_buf);
	memset(temp_buf, 0x0, sizeof(temp_buf));

	print_reg_num = 8;
	for (i = 0; i < print_reg_num; ++i) {
		sm5451_read_reg(sm5451, reg_addr[i], &reg_data);
		sprintf(temp_buf+strlen(temp_buf), "0x%02X[0x%02X],", reg_addr[i], reg_data);
	}
	pr_info("sm5451-charger: regmap: %s\n", temp_buf);
	memset(temp_buf, 0x0, sizeof(temp_buf));
}

static int sm5451_reverse_boost_enable(struct sm5451_charger *sm5451, bool enable)
{
	u8 i, reg;

	if (enable) {
		sm5451_set_op_mode(sm5451, OP_MODE_REV_BOOST);
		for (i = 0; i < 12; ++i) {
			usleep_range(10000, 11000);
			sm5451_read_reg(sm5451, SM5451_REG_FLAG4, &reg);
			if (reg & (0x1 << 4))
				break;
		}
		if (i == 12) {
			dev_err(sm5451->dev, "%s: fail to reverse boost enable\n", __func__);
			sm5451_set_op_mode(sm5451, OP_MODE_INIT);
			sm5451->rev_boost = false;
			return -EINVAL;
		}
		dev_info(sm5451->dev, "%s: ON\n", __func__);
	} else {
		sm5451_set_op_mode(sm5451, OP_MODE_INIT);
		dev_info(sm5451->dev, "%s: OFF\n", __func__);
	}
	sm5451->rev_boost = enable;

	return 0;
}

static bool sm5451_check_charging_enable(struct sm5451_charger *sm5451)
{
	u8 flag;

	flag = sm5451_get_flag_status(sm5451, SM5451_REG_FLAG1);
	if ((flag >> 1) & 0x1) {
		return true;
	} else {
		return false;
	}
}

static int sm5451_prechg_enable(struct sm5451_charger *sm5451, bool enable)
{
	struct sm_dc_info *sm_dc = select_sm_dc_info(sm5451);
	int state = sm_dc_get_current_state(sm_dc);
	u8 reg, i;

	if (enable) {
		if (state > SM_DC_EOC || sm5451_check_charging_enable(sm5451)) {
			dev_info(sm5451->dev, "%s: charging state (state=%d)\n", __func__, state);
		} else {
			dev_info(sm5451->dev, "%s: ON\n", __func__);
			for (i = 0; i < 2; ++i) {
				sm5451_write_reg(sm5451, SM5451_REG_PRECHG_MODE, 0xEA);
				sm5451_write_reg(sm5451, SM5451_REG_PRECHG_MODE, 0xAE);
				sm5451_write_reg(sm5451, SM5451_REG_CTRL_STM_0, 0xB0);
				sm5451_write_reg(sm5451, SM5451_REG_CTRL_STM_3, 0x80);
				sm5451_write_reg(sm5451, SM5451_REG_CTRL_STM_5, 0x08);
				sm5451_write_reg(sm5451, SM5451_REG_CTRL_STM_2, 0x08);
				sm5451_read_reg(sm5451, SM5451_REG_CTRL_STM_0, &reg);

				if (reg != 0xB0)
					sm5451_write_reg(sm5451, SM5451_REG_PRECHG_MODE, 0x00);
				else
					break;
				dev_info(sm5451->dev, "%s: fail to pre-charging\n", __func__);
			}
			usleep_range(10000, 11000);
		}
	} else {
		dev_info(sm5451->dev, "%s: OFF\n", __func__);
		sm5451_write_reg(sm5451, SM5451_REG_PRECHG_MODE, 0x00);
	}
	return 0;
}

static int sm5451_start_charging(struct sm5451_charger *sm5451)
{
	struct sm_dc_info *sm_dc = select_sm_dc_info(sm5451);
	int state = sm_dc_get_current_state(sm_dc);
	int ret;

	if (sm5451->cable_online < 1) {
		dev_err(sm5451->dev, "%s: can't detect valid cable connection(online=%d)\n",
			__func__, sm5451->cable_online);
		return -ENODEV;
	}
	
	if (state < SM_DC_CHECK_VBAT) {
		dev_info(sm5451->dev, "%s: charging off state (state=%d)\n", __func__, state);
		ret = sm5451_sw_reset(sm5451);
		if (ret < 0) {
			dev_err(sm5451->dev, "%s: fail to sw reset(ret=%d)\n", __func__, ret);
			return ret;
		}
		sm5451_init_reg_param(sm5451);
		sm5451_prechg_enable(sm5451, 1);
	}

	ret = sm_dc_start_charging(sm_dc);
	if (ret < 0) {
		dev_err(sm5451->dev, "%s: fail to start direct-charging\n", __func__);
		return ret;
	}
	__pm_stay_awake(sm5451->chg_ws);

	return 0;
}

static int sm5451_stop_charging(struct sm5451_charger *sm5451)
{
	struct sm_dc_info *sm_dc = select_sm_dc_info(sm5451);

	msleep(30);
	sm_dc_stop_charging(sm_dc);

	__pm_relax(sm5451->chg_ws);

	return 0;
}

static int psy_chg_get_online(struct sm5451_charger *sm5451)
{
	u8 flag, vbus_pok, cable_online;

	cable_online = sm5451->cable_online < 1 ? 0 : 1;
	flag = sm5451_get_flag_status(sm5451, SM5451_REG_FLAG3);
	dev_info(sm5451->dev, "%s: FLAG3=0x%x\n", __func__, flag);
	vbus_pok = (flag >> 7) & 0x1;

	if (vbus_pok != cable_online) {
		dev_err(sm5451->dev, "%s: mismatched vbus state(vbus_pok:%d cable_online:%d)\n",
		__func__, vbus_pok, sm5451->cable_online);
	}

	return sm5451->cable_online;
}

static int psy_chg_get_status(struct sm5451_charger *sm5451)
{
	int status = POWER_SUPPLY_STATUS_UNKNOWN;
	u8 flag1, flag2, flag3, flag4;

	flag1 = sm5451_get_flag_status(sm5451, SM5451_REG_FLAG1);
	flag2 = sm5451_get_flag_status(sm5451, SM5451_REG_FLAG2);
	flag3 = sm5451_get_flag_status(sm5451, SM5451_REG_FLAG3);
	flag4 = sm5451_get_flag_status(sm5451, SM5451_REG_FLAG4);
	dev_info(sm5451->dev, "%s: FLAG:0x%x:0x%x:0x%x:0x%x\n",
		__func__, flag1, flag2, flag3, flag4);

	if (sm5451_check_charging_enable(sm5451)) {
		status = POWER_SUPPLY_STATUS_CHARGING;
	} else {
		if ((flag3 >> 7) & 0x1) { /* check vbus-pok */
			status = POWER_SUPPLY_STATUS_NOT_CHARGING;
		} else {
			status = POWER_SUPPLY_STATUS_DISCHARGING;
		}
	}

	return status;
}

static int psy_chg_get_health(struct sm5451_charger *sm5451)
{
	struct sm_dc_info *sm_dc = select_sm_dc_info(sm5451);
	int state = sm_dc_get_current_state(sm_dc);
	int health = POWER_SUPPLY_HEALTH_GOOD;
	u8 flag, chg_on;

	if (state == SM_DC_ERR) {
		health = POWER_SUPPLY_EXT_HEALTH_DC_ERR;
		dev_info(sm5451->dev, "%s: chg_state=%d, health=%d\n",
			__func__, state, health);
	} else if (state >= SM_DC_PRE_CC && state <= SM_DC_CV) {
		chg_on = sm5451_check_charging_enable(sm5451);
		flag = sm5451_get_flag_status(sm5451, SM5451_REG_FLAG3);
		if (chg_on == false) {
			health = POWER_SUPPLY_EXT_HEALTH_DC_ERR;
		} else if (((flag >> 7) & 0x1) == 0x0) {    /* VBUS_POK status is disabled */
			health = POWER_SUPPLY_EXT_HEALTH_DC_ERR;
		}

		dev_info(sm5451->dev, "%s: chg_on=%d, flag3=0x%x, health=%d\n",
			__func__, chg_on, flag, health);
	}

	return health;
}

static int psy_chg_get_chg_vol(struct sm5451_charger *sm5451)
{
	u32 chg_vol = sm5451_get_vbatreg(sm5451);

	dev_info(sm5451->dev, "%s: VBAT_REG=%dmV\n", __func__, chg_vol);

	return chg_vol;
}

static int psy_chg_get_input_curr(struct sm5451_charger *sm5451)
{
	u32 input_curr = sm5451_get_ibuslim(sm5451);

	dev_info(sm5451->dev, "%s: IBUSLIM=%dmA\n", __func__, input_curr);

	return input_curr;
}

static int psy_chg_get_ext_monitor_work(struct sm5451_charger *sm5451)
{
	struct sm_dc_info *sm_dc = select_sm_dc_info(sm5451);
	int state = sm_dc_get_current_state(sm_dc);
	int adc_vbus, adc_ibus, adc_vbat, adc_ibat, adc_them, adc_tdie;

	if (state < SM_DC_CHECK_VBAT) {
		dev_info(sm5451->dev, "%s: charging off state (state=%d)\n", __func__, state);
		return -EINVAL;
	}

	adc_vbus = sm5451_convert_adc(sm5451, SM5451_ADC_VBUS);
	adc_ibus = sm5451_convert_adc(sm5451, SM5451_ADC_IBUS);
	adc_vbat = sm5451_convert_adc(sm5451, SM5451_ADC_VBAT);
	adc_ibat = sm5451_convert_adc(sm5451, SM5451_ADC_IBAT);
	adc_them = sm5451_convert_adc(sm5451, SM5451_ADC_THEM);
	adc_tdie = sm5451_convert_adc(sm5451, SM5451_ADC_TDIE);

	pr_info("sm5451-charger: adc_monitor: vbus:%d:ibus:%d:vbat:%d:ibat:%d:them:%d:tdie:%d\n",
		adc_vbus, adc_ibus, adc_vbat, adc_ibat, adc_them, adc_tdie);
	sm5451_print_regmap(sm5451);

	return 0;
}

static int psy_chg_get_ext_measure_input(struct sm5451_charger *sm5451, int index)
{
	int adc;

	switch (index) {
	case SEC_BATTERY_IIN_MA:
		adc = sm5451_convert_adc(sm5451, SM5451_ADC_IBUS);
		break;
	case SEC_BATTERY_IIN_UA:
		adc = sm5451_convert_adc(sm5451, SM5451_ADC_IBUS) * 1000;
		break;
	case SEC_BATTERY_VIN_MA:
		adc = sm5451_convert_adc(sm5451, SM5451_ADC_VBUS);
		break;
	case SEC_BATTERY_VIN_UA:
		adc = sm5451_convert_adc(sm5451, SM5451_ADC_VBUS) * 1000;
		break;
	default:
		adc = 0;
		break;
	}

	dev_info(sm5451->dev, "%s: index=%d, adc=%d\n", __func__, index, adc);

	return adc;
}

static const char *sm5451_dc_state_str[] = {
	"NO_CHARGING", "DC_ERR", "CHARGING_DONE", "CHECK_VBAT", "PRESET_DC",
	"ADJUST_CC", "UPDATE_BAT", "CC_MODE", "CV_MODE"
};
static int psy_chg_get_ext_direct_charger_chg_status(struct sm5451_charger *sm5451, 
                                                     union power_supply_propval *val)
{
	struct sm_dc_info *sm_dc = select_sm_dc_info(sm5451);
	int state = sm_dc_get_current_state(sm_dc);

	val->strval = sm5451_dc_state_str[state];
	pr_info("%s: CHARGER_STATUS(%s)\n", __func__, val->strval);

	return 0;
}

static int sm5451_chg_get_property(struct power_supply *psy,
	enum power_supply_property psp, union power_supply_propval *val)
{
	struct sm5451_charger *sm5451 = power_supply_get_drvdata(psy);
	enum power_supply_ext_property ext_psp =
		(enum power_supply_ext_property)psp;
	int adc_them;

	dev_info(sm5451->dev, "%s: psp=%d\n", __func__, psp);

	switch ((int)psp) {
	case POWER_SUPPLY_PROP_ONLINE:
		val->intval = psy_chg_get_online(sm5451);
		break;
	case POWER_SUPPLY_PROP_STATUS:
		val->intval = psy_chg_get_status(sm5451);
		break;
	case POWER_SUPPLY_PROP_HEALTH:
		val->intval = psy_chg_get_health(sm5451);
		break;
	case POWER_SUPPLY_PROP_CONSTANT_CHARGE_VOLTAGE:
		val->intval = psy_chg_get_chg_vol(sm5451);
		break;
	case POWER_SUPPLY_PROP_INPUT_CURRENT_LIMIT:
		val->intval = psy_chg_get_input_curr(sm5451);
		break;
	case POWER_SUPPLY_PROP_CONSTANT_CHARGE_CURRENT:
		val->intval = sm5451->target_ibat;
		break;
	case POWER_SUPPLY_PROP_TEMP:
		adc_them = sm5451_convert_adc(sm5451, SM5451_ADC_THEM);
		val->intval = adc_them;
		break;
#if defined(CONFIG_DUAL_BATTERY_CELL_SENSING)
	case POWER_SUPPLY_PROP_VOLTAGE_NOW:
		val->intval = sm5451_convert_adc(sm5451, SM5451_ADC_VBAT);
		break;
#endif
	case POWER_SUPPLY_EXT_PROP_MIN ... POWER_SUPPLY_EXT_PROP_MAX:
		switch (ext_psp) {
		case POWER_SUPPLY_EXT_PROP_CHARGING_ENABLED:
			val->intval = sm5451_check_charging_enable(sm5451);
			break;
		case POWER_SUPPLY_EXT_PROP_MONITOR_WORK:
			psy_chg_get_ext_monitor_work(sm5451);
			break;
		case POWER_SUPPLY_EXT_PROP_MEASURE_INPUT:
			val->intval = psy_chg_get_ext_measure_input(sm5451, val->intval);
			break;
		case POWER_SUPPLY_EXT_PROP_MEASURE_SYS:
			dev_err(sm5451->dev, "%s: need to works\n", __func__);
			/*
			 *  Need to check operation details.. by SEC.
			 */
			val->intval = -EINVAL;
			break;
		case POWER_SUPPLY_EXT_PROP_DIRECT_CHARGER_CHG_STATUS:
			psy_chg_get_ext_direct_charger_chg_status(sm5451, val);
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

static int psy_chg_set_online(struct sm5451_charger *sm5451, int online)
{
	struct sm_dc_info *sm_dc = select_sm_dc_info(sm5451);
	int ret = 0;

	dev_info(sm5451->dev, "%s: online=%d\n", __func__, online);

	if (online < 2) {
		if (sm_dc_get_current_state(sm_dc) > SM_DC_EOC) {
			sm5451_stop_charging(sm5451);
		}
	}
	sm5451->cable_online = online;

	sm5451->vbus_in = sm5451->cable_online > 2 ? true : false;

	return ret;
}

static int psy_chg_set_const_chg_voltage(struct sm5451_charger *sm5451, int vbat)
{
	struct sm_dc_info *sm_dc = select_sm_dc_info(sm5451);
	int ret = 0;

	dev_info(sm5451->dev, "%s: [%dmV] to [%dmV]\n", __func__, sm5451->target_vbat, vbat);

	if (sm5451->target_vbat != vbat) {
		sm5451->target_vbat = vbat;
		ret = sm_dc_set_target_vbat(sm_dc, sm5451->target_vbat);
	}

	return ret;
}

static int psy_chg_set_chg_curr(struct sm5451_charger *sm5451, int ibat)
{
	int ret = 0;

	dev_info(sm5451->dev, "%s: dldn't support cc_loop\n", __func__);
	sm5451->target_ibat = ibat;

	return ret;
}

static int psy_chg_set_input_curr(struct sm5451_charger *sm5451, int ibus)
{
	struct sm_dc_info *sm_dc = select_sm_dc_info(sm5451);
	int ret = 0;

	dev_info(sm5451->dev, "%s: ibus [%dmA] to [%dmA]\n", __func__, sm5451->target_ibus, ibus);

	if (sm5451->target_ibus != ibus) {
		sm5451->target_ibus = ibus;
		if (sm5451->target_ibus < SM5451_TA_MIN_CURRENT) {
			dev_info(sm5451->dev, "%s: can't used less then ta_min_current(%dmA)\n",
				__func__, SM5451_TA_MIN_CURRENT);
			sm5451->target_ibus = SM5451_TA_MIN_CURRENT;
		}
		ret = sm_dc_set_target_ibus(sm_dc, sm5451->target_ibus);
	}

	return ret;
}

static int psy_chg_set_const_chg_voltage_max(struct sm5451_charger *sm5451, int max_vbat)
{
	struct sm_dc_info *sm_dc = select_sm_dc_info(sm5451);

	dev_info(sm5451->dev, "%s: max_vbat [%dmA] to [%dmA]\n",
		__func__, sm5451->max_vbat, max_vbat);

	sm5451->max_vbat = max_vbat;
	sm_dc->config.chg_float_voltage = max_vbat;

	return 0;

}

static int psy_chg_ext_wdt_control(struct sm5451_charger *sm5451, int wdt_control)
{
	if (wdt_control) {
		sm5451->wdt_disable = 1;
	} else {
		sm5451->wdt_disable = 0;
	}
	dev_info(sm5451->dev, "%s: wdt_disable=%d\n", __func__, sm5451->wdt_disable);

	return 0;
}

static int sm5451_chg_set_property(struct power_supply *psy,
	enum power_supply_property psp, const union power_supply_propval *val)
{
	struct sm5451_charger *sm5451 = power_supply_get_drvdata(psy);
	enum power_supply_ext_property ext_psp = 
		(enum power_supply_ext_property) psp;

	int ret = 0;

	dev_info(sm5451->dev, "%s: psp=%d, val-intval=%d\n", __func__, psp, val->intval);

	switch ((int)psp) {
	case POWER_SUPPLY_PROP_ONLINE:
		psy_chg_set_online(sm5451, val->intval);
		break;
	case POWER_SUPPLY_PROP_CONSTANT_CHARGE_VOLTAGE:
		ret = psy_chg_set_const_chg_voltage(sm5451, val->intval);
		break;
	case POWER_SUPPLY_PROP_CONSTANT_CHARGE_VOLTAGE_MAX:
		ret = psy_chg_set_const_chg_voltage_max(sm5451, val->intval);
		break;
	case POWER_SUPPLY_PROP_CONSTANT_CHARGE_CURRENT:
		ret = psy_chg_set_chg_curr(sm5451, val->intval);
		break;
	case POWER_SUPPLY_PROP_INPUT_CURRENT_LIMIT:
		ret = psy_chg_set_input_curr(sm5451, val->intval);
		break;
	case POWER_SUPPLY_EXT_PROP_MIN ... POWER_SUPPLY_EXT_PROP_MAX:
		switch (ext_psp) {
		case POWER_SUPPLY_EXT_PROP_DIRECT_WDT_CONTROL:
			ret = psy_chg_ext_wdt_control(sm5451, val->intval);
			break;
		case POWER_SUPPLY_EXT_PROP_CHARGING_ENABLED:
			if (val->intval) {
			ret = sm5451_start_charging(sm5451);
			} else {
			ret = sm5451_stop_charging(sm5451);
			}
			sm5451_print_regmap(sm5451);
			break;
		case POWER_SUPPLY_EXT_PROP_DIRECT_CURRENT_MAX:
			ret = psy_chg_set_input_curr(sm5451, val->intval);
			break;
		case POWER_SUPPLY_EXT_PROP_DIRECT_ADC_CTRL:
			if (val->intval)
				sm5451_enable_adc_oneshot(sm5451, 1);
			else
				sm5451_enable_adc_oneshot(sm5451, 0);
			pr_info("%s: ADC_CTRL : %d\n", __func__, val->intval);
			break;
		default:
			return -EINVAL;
		}
		break;
	default:
		return -EINVAL;
	}

	if (ret < 0) {
		return ret;
	}

	return 0;
}

static char *sm5451_supplied_to[] = {
	"sm5451-charger",
};

static enum power_supply_property sm5451_charger_props[] = {
	POWER_SUPPLY_PROP_ONLINE,
};

static const struct power_supply_desc sm5451_charger_power_supply_desc = {
	.name		= "sm5451-charger",
	.type		= POWER_SUPPLY_TYPE_UNKNOWN,
	.get_property	= sm5451_chg_get_property,
	.set_property 	= sm5451_chg_set_property,
	.properties	= sm5451_charger_props,
	.num_properties	= ARRAY_SIZE(sm5451_charger_props),
};

static int sm5451_get_adc_value(struct i2c_client *i2c, u8 adc_ch)
{
	struct sm5451_charger *sm5451 = i2c_get_clientdata(i2c);
	int adc;

	switch (adc_ch) {
	case SM_DC_ADC_THEM:
		adc = sm5451_convert_adc(sm5451, SM5451_ADC_THEM);
		break;
	case SM_DC_ADC_DIETEMP:
		adc = sm5451_convert_adc(sm5451, SM5451_ADC_TDIE);
		break;
	case SM_DC_ADC_VBAT:
		adc = sm5451_convert_adc(sm5451, SM5451_ADC_VBAT);
		break;
	case SM_DC_ADC_IBAT:
		adc = sm5451_convert_adc(sm5451, SM5451_ADC_IBAT);
		break;
	case SM_DC_ADC_VBUS:
		adc = sm5451_convert_adc(sm5451, SM5451_ADC_VBUS);
		break;
	case SM_DC_ADC_IBUS:
		adc = sm5451_convert_adc(sm5451, SM5451_ADC_IBUS);
		break;
	case SM_DC_ADC_VOUT:
	default:
		adc = 0;
		break;
	}

	return adc;
}

static int sm5451_set_adc_mode(struct i2c_client *i2c, u8 mode)
{
	struct sm5451_charger *sm5451 = i2c_get_clientdata(i2c);

	switch (mode) {
	case SM_DC_ADC_MODE_ONESHOT:
		/* covered by continuous mode */
	case SM_DC_ADC_MODE_CONTINUOUS:
		/* SM5451 continuous mode reflash time : 200ms */
		sm5451_set_adcmode(sm5451, 0);
		sm5451_enable_adc(sm5451, 1);
		break;
	case SM_DC_ADC_MODE_OFF:
	default:
		sm5451_set_adcmode(sm5451, 0);
		sm5451_enable_adc(sm5451, 0);
		break;
	}

	return 0;
}

static int sm5451_get_charging_enable(struct i2c_client *i2c)
{
	struct sm5451_charger *sm5451 = i2c_get_clientdata(i2c);

	return sm5451_check_charging_enable(sm5451);
}

static int sm5451_set_charging_enable(struct i2c_client *i2c, bool enable)
{
	struct sm5451_charger *sm5451 = i2c_get_clientdata(i2c);
	struct sm_dc_info *sm_dc = select_sm_dc_info(sm5451);

	sm5451_prechg_enable(sm5451, 0);
	if (enable) {
		if (sm_dc->ta.v_max < SM_DC_BYPASS_TA_MAX_VOL)
			sm5451_set_op_mode(sm5451, OP_MODE_FW_BYPASS);
		else
			sm5451_set_op_mode(sm5451, OP_MODE_FW_BOOST);
	} else {
		sm5451_set_op_mode(sm5451, OP_MODE_INIT);
	}

	sm5451_print_regmap(sm5451);

	return 0;
}

static int sm5451_dc_set_charging_config(struct i2c_client *i2c, u32 cv_gl, u32 ci_gl, u32 cc_gl)
{
	struct sm5451_charger *sm5451 = i2c_get_clientdata(i2c);
	struct sm_dc_info *sm_dc = select_sm_dc_info(sm5451);
	u32 vbatreg, ibuslim;

	vbatreg = cv_gl + SM5451_CV_OFFSET;
	if (ci_gl <= SM5451_TA_MIN_CURRENT) {
		ibuslim = ci_gl + SM5451_CI_OFFSET * 2;
	} else {
		ibuslim = ci_gl + SM5451_CI_OFFSET;
	}
	sm5451_set_ibuslim(sm5451, ibuslim);
	sm5451_set_vbatreg(sm5451, vbatreg);

	pr_info("%s %s: vbat_reg=%dmV, ibus_lim=%dmA\n", sm_dc->name, __func__, vbatreg, ibuslim);

	return 0;
}

static u32 sm5451_get_dc_error_status(struct i2c_client *i2c)
{
	struct sm5451_charger *sm5451 = i2c_get_clientdata(i2c);
	struct sm_dc_info *sm_dc = select_sm_dc_info(sm5451);
	u8 flag1, flag2, flag3, flag4, op_mode;
	u8 flag1_s, flag2_s, flag3_s, flag4_s;
	u32 err = SM_DC_ERR_NONE;

	mutex_lock(&sm5451->i2c_lock);
	sm5451_read_reg(sm5451, SM5451_REG_FLAG1, &flag1);
	sm5451_read_reg(sm5451, SM5451_REG_FLAG2, &flag2);
	sm5451_read_reg(sm5451, SM5451_REG_FLAG3, &flag3);
	sm5451_read_reg(sm5451, SM5451_REG_FLAG4, &flag4);
	pr_info("%s %s: FLAG:0x%x:0x%x:0x%x:0x%x\n", sm_dc->name,
		__func__, flag1, flag2, flag3, flag4);

	sm5451_read_reg(sm5451, SM5451_REG_FLAG1, &flag1_s);
	sm5451_read_reg(sm5451, SM5451_REG_FLAG2, &flag2_s);
	sm5451_read_reg(sm5451, SM5451_REG_FLAG3, &flag3_s);
	sm5451_read_reg(sm5451, SM5451_REG_FLAG4, &flag4_s);
	op_mode = sm5451_get_op_mode(sm5451);
	pr_info("%s %s: FLAG:0x%x:0x%x:0x%x:0x%x op_mode=0x%x\n", sm_dc->name,
		__func__, flag1_s, flag2_s, flag3_s, flag4_s, op_mode);
	mutex_unlock(&sm5451->i2c_lock);

	if ((op_mode == OP_MODE_INIT) && (sm_dc->wq.retry_cnt < 3)) {
		pr_info("%s %s: try to retry, cnt:%d\n", sm_dc->name, __func__, sm_dc->wq.retry_cnt);
		sm_dc->wq.retry_cnt++;
		err = SM_DC_ERR_RETRY;
	} else if (op_mode == OP_MODE_INIT) {
		if ((flag1 & SM5451_FLAG1_IBUSUCP) || (flag1_s & SM5451_FLAG1_IBUSUCP))
			err += SM_DC_ERR_IBUSUCP;
		if ((flag3 & SM5451_FLAG3_VBUSUVLO) || (flag3_s & SM5451_FLAG3_VBUSUVLO))
			err += SM_DC_ERR_VBUSUVLO;
		if ((flag1 & SM5451_FLAG1_VBUSOVP) || (flag1_s & SM5451_FLAG1_VBUSOVP))
			err += SM_DC_ERR_VBUSOVP;
		if ((flag1 & SM5451_FLAG1_IBUSOCP) || (flag1_s & SM5451_FLAG1_IBUSOCP))
			err += SM_DC_ERR_IBUSOCP;
		if ((flag2 & SM5451_FLAG2_CNSHTP) || (flag2_s & SM5451_FLAG2_CNSHTP))
			err += SM_DC_ERR_CN_SHORT;
		if ((flag3 & SM5451_FLAG3_CFLYSHTP) || (flag3_s & SM5451_FLAG3_CFLYSHTP))
			err += SM_DC_ERR_CFLY_SHORT;

		if (err == SM_DC_ERR_NONE)
			err += SM_DC_ERR_UNKNOWN;
		pr_info("%s %s: SM_DC_ERR(err=0x%x)\n", sm_dc->name, __func__, err);
	} else {
		if (flag3_s & SM5451_FLAG3_VBUSUVLO) {
			pr_info("%s %s: vbus uvlo detected, try to retry\n", sm_dc->name, __func__);
			err = SM_DC_ERR_RETRY;
		} else if ((flag2 & SM5451_FLAG2_VBATREG) || (flag2_s & SM5451_FLAG2_VBATREG)) {
			pr_info("%s %s: VBATREG detected\n", sm_dc->name, __func__);
			err = SM_DC_ERR_VBATREG;
		} else if (flag4_s & SM5451_FLAG4_IBUSREG) {
			pr_info("%s %s: IBUSREG detected\n", sm_dc->name, __func__);
			err = SM_DC_ERR_IBUSREG;
		}
	}

	if (!sm5451->wdt_disable) {
		sm5451_kick_wdt(sm5451);
	}

	return err;
}

static int sm5451_send_pd_msg(struct i2c_client *i2c, struct sm_dc_power_source_info *ta)
{
	struct sm5451_charger *sm5451 = i2c_get_clientdata(i2c);
	struct sm_dc_info *sm_dc = select_sm_dc_info(sm5451);
	int ret;

	mutex_lock(&sm5451->pd_lock);

	ret = sec_pd_select_pps(ta->pdo_pos, ta->v, ta->c);
	if (ret == -EBUSY) {
		pr_info("%s %s: request again\n", sm_dc->name, __func__);
		msleep(100);
		ret = sec_pd_select_pps(ta->pdo_pos, ta->v, ta->c);
	}

	mutex_unlock(&sm5451->pd_lock);

	return ret;
	}

static int sm5451_get_apdo_max_power(struct i2c_client *i2c, struct sm_dc_power_source_info *ta)
{
	struct sm5451_charger *sm5451 = i2c_get_clientdata(i2c);
	struct sm_dc_info *sm_dc = select_sm_dc_info(sm5451);
	int ret;

	ta->pdo_pos = 0;        /* set '0' else return error */
	ta->v_max = 10000;      /* request voltage level */
	ta->c_max = 0;
	ta->p_max = 0;

	ret = sec_pd_get_apdo_max_power(&ta->pdo_pos, &ta->v_max, &ta->c_max, &ta->p_max);
	if (ret < 0) {
		dev_err(sm5451->dev, "%s: error:sec_pd_get_apdo_max_power\n", __func__);
	} else {
		sm_dc->ta.pdo_pos = ta->pdo_pos;
		sm_dc->ta.v_max = ta->v_max;
		sm_dc->ta.c_max = ta->c_max;
		sm_dc->ta.p_max = ta->p_max;
	}

	dev_info(sm5451->dev, "%s: pdo_pos:%d, max_vol:%dmV, max_cur:%dmA, max_pwr:%dmW\n",
			__func__, ta->pdo_pos, ta->v_max, ta->c_max, ta->p_max);
	return ret;
}

static const struct sm_dc_ops sm5451_dc_pps_ops = {
	.get_adc_value          = sm5451_get_adc_value,
	.set_adc_mode           = sm5451_set_adc_mode,
	.get_charging_enable    = sm5451_get_charging_enable,
	.get_dc_error_status    = sm5451_get_dc_error_status,
	.set_charging_enable    = sm5451_set_charging_enable,
	.set_charging_config    = sm5451_dc_set_charging_config,
	.send_power_source_msg  = sm5451_send_pd_msg,
	.get_apdo_max_power     = sm5451_get_apdo_max_power,
};


static irqreturn_t sm5451_irq_thread(int irq, void *data)
{
	struct sm5451_charger *sm5451 = (struct sm5451_charger *)data;
	struct sm_dc_info *sm_dc = select_sm_dc_info(sm5451);
	u8 op_mode = sm5451_get_op_mode(sm5451);
	u8 flag1, flag2, flag3, flag4;
	u32 err = 0x0;

	sm5451_read_reg(sm5451, SM5451_REG_FLAG1, &flag1);
	sm5451_read_reg(sm5451, SM5451_REG_FLAG2, &flag2);
	sm5451_read_reg(sm5451, SM5451_REG_FLAG3, &flag3);
	sm5451_read_reg(sm5451, SM5451_REG_FLAG4, &flag4);
	dev_info(sm5451->dev, "%s: FLAG:0x%x:0x%x:0x%x:0x%x\n",
		__func__, flag1, flag2, flag3, flag4);

	/* check forced CUT-OFF status */
	if (sm_dc_get_current_state(sm_dc) > SM_DC_PRESET &&
		op_mode == OP_MODE_INIT) {
		if (flag1 & SM5451_FLAG1_VBUSOVP) {
			err += SM_DC_ERR_VBUSOVP;
		}
		if (flag1 & SM5451_FLAG1_IBUSOCP) {
			err += SM_DC_ERR_IBUSOCP;
		}
		if (flag1 & SM5451_FLAG1_IBUSUCP) {
			err += SM_DC_ERR_IBUSUCP;
		}
		if (flag2 & SM5451_FLAG2_VBATOVP) {
			err += SM_DC_ERR_VBATOVP;
		}
#if 0   /* un-used IBATOCP protection */
		if (flag2 & SM5451_FLAG2_IBATOCP) {
			err += SM_DC_ERR_IBATOCP;
		}
#endif
		if (flag2 & SM5451_FLAG2_TSD) {
			err += SM_DC_ERR_TSD;
		}
		if (flag2 & SM5451_FLAG2_RLTVOVP) {
			err += SM_DC_ERR_STUP_FAIL;
		}
		if (flag2 & SM5451_FLAG2_RLTVUVP) {
			err += SM_DC_ERR_STUP_FAIL;
		}
		if (flag2 & SM5451_FLAG2_CNSHTP) {
			err += SM_DC_ERR_CN_SHORT;
		}
		if (flag3 & SM5451_FLAG3_VBUSUVLO) {
			err += SM_DC_ERR_VBUSUVLO;
		}
		if (flag3 & SM5451_FLAG3_CFLYSHTP) {
			err += SM_DC_ERR_CFLY_SHORT;
		}
		dev_err(sm5451->dev, "%s: forced charge cut-off(err=0x%x)\n", __func__, err);
		sm_dc_report_error_status(sm_dc, err);
	}

	if (flag2 & SM5451_FLAG2_VBATREG) {
		dev_info(sm5451->dev, "%s: VBATREG detected\n", __func__);
		sm_dc_report_interrupt_event(sm_dc, SM_DC_INT_VBATREG);
	}
	if (flag2 & SM5451_FLAG2_IBATREG) {
		dev_info(sm5451->dev, "%s: IBATREG detected\n", __func__);
	}
	if (flag4 & SM5451_FLAG4_IBUSREG) {
		dev_info(sm5451->dev, "%s: IBUSREG detected\n", __func__);
	}

	if (flag3 & SM5451_FLAG3_VBUSPOK) {
		dev_info(sm5451->dev, "%s: VBUSPOK detected\n", __func__);
	}
	if (flag3 & SM5451_FLAG3_VBUSUVLO) {
		dev_info(sm5451->dev, "%s: VBUSUVLO detected\n", __func__);
	}
	if (flag3 & SM5451_FLAG3_WTDTMR) {
		dev_info(sm5451->dev, "%s: Watchdog Timer expired\n", __func__);
		sm_dc_report_interrupt_event(sm_dc, SM_DC_INT_WDTOFF);
	}

	dev_info(sm5451->dev, "closed %s\n", __func__);

	return IRQ_HANDLED;
}

static int sm5451_irq_init(struct sm5451_charger *sm5451)
{
	int ret;
	u8 reg;

	sm5451->irq = gpio_to_irq(sm5451->pdata->irq_gpio);
	dev_info(sm5451->dev, "%s: irq_gpio=%d, irq=%d\n", __func__,
	sm5451->pdata->irq_gpio, sm5451->irq);

	ret = gpio_request(sm5451->pdata->irq_gpio, "sm5540_irq");
	if (ret) {
		dev_err(sm5451->dev, "%s: fail to request gpio(ret=%d)\n",
			__func__, ret);
		return ret;
	}
	gpio_direction_input(sm5451->pdata->irq_gpio);
	gpio_free(sm5451->pdata->irq_gpio);

	sm5451_write_reg(sm5451, SM5451_REG_FLAGMSK1, 0xE2);
	sm5451_write_reg(sm5451, SM5451_REG_FLAGMSK2, 0x00);
	sm5451_write_reg(sm5451, SM5451_REG_FLAGMSK3, 0xD6);
	sm5451_write_reg(sm5451, SM5451_REG_FLAGMSK4, 0xDA);
	sm5451_read_reg(sm5451, SM5451_REG_FLAG1, &reg);
	sm5451_read_reg(sm5451, SM5451_REG_FLAG2, &reg);
	sm5451_read_reg(sm5451, SM5451_REG_FLAG3, &reg);
	sm5451_read_reg(sm5451, SM5451_REG_FLAG4, &reg);

	ret = request_threaded_irq(sm5451->irq, NULL, sm5451_irq_thread,
		IRQF_TRIGGER_LOW | IRQF_ONESHOT,
		"sm5451-irq", sm5451);
	if (ret) {
		dev_err(sm5451->dev, "%s: fail to request irq(ret=%d)\n",
			__func__, ret);
		return ret;
	}

	return 0;
}

static void sm5451_hw_init_config(struct sm5451_charger *sm5451)
{
	int ret;
	u8 reg;

	/* check to valid I2C transfer & register control */
	ret = sm5451_read_reg(sm5451, SM5451_REG_DEVICE_ID, &reg);
	if (ret < 0 || (reg & 0xF) != 0x1) {
		dev_err(sm5451->dev, "%s: device not found on this channel (reg=0x%x)\n",
			__func__, reg);
	}
	sm5451->pdata->rev_id = (reg >> 4) & 0xf;

	sm5451_init_reg_param(sm5451);

#if IS_ENABLED(CONFIG_BATTERY_SAMSUNG)
	psy_chg_set_const_chg_voltage(sm5451, sm5451->pdata->battery.chg_float_voltage);
#endif
}

#if defined(CONFIG_OF)
static int sm5451_charger_parse_dt(struct device *dev,
                                   struct sm5451_platform_data *pdata)
{
	struct device_node *np_sm5451 = dev->of_node;
	struct device_node *np_battery;
	int ret;

	/* Parse: sm5451 node */
	if(!np_sm5451) {
		dev_err(dev, "%s: empty of_node for sm5451_dev\n", __func__);
		return -EINVAL;
	}
	pdata->irq_gpio = of_get_named_gpio(np_sm5451, "sm5451,irq-gpio", 0);
	dev_info(dev, "parse_dt: irq_gpio=%d\n", pdata->irq_gpio);

	ret = of_property_read_u32(np_sm5451, "sm5451,pps_lr", &pdata->pps_lr);
	if (ret) {
		dev_info(dev, "%s: sm5451,pps_lr is Empty\n", __func__);
		pdata->pps_lr = 340000;
	}
	dev_info(dev, "parse_dt: pps_lr=%d\n", pdata->pps_lr);

	ret = of_property_read_u32(np_sm5451, "sm5451,rpcm", &pdata->rpcm);
	if (ret) {
		dev_info(dev, "%s: sm5451,rpcm is Empty\n", __func__);
		pdata->rpcm = 55000;
	}
	dev_info(dev, "parse_dt: rpcm=%d\n", pdata->rpcm);

	/* Parse: battery node */
	np_battery = of_find_node_by_name(NULL, "battery");
	if(!np_battery) {
		dev_err(dev, "%s: empty of_node for battery\n", __func__);
		return -EINVAL;
	}
	ret = of_property_read_u32(np_battery, "battery,chg_float_voltage",
		&pdata->battery.chg_float_voltage);
	if (ret) {
		dev_info(dev, "%s: battery,chg_float_voltage is Empty\n", __func__);
		pdata->battery.chg_float_voltage = 4200;
	}
	ret = of_property_read_string(np_battery, "battery,charger_name",
		(char const **)&pdata->battery.sec_dc_name);
	if (ret) {
		dev_info(dev, "%s: battery,charger_name is Empty\n", __func__);
		pdata->battery.sec_dc_name = "sec-direct-charger";
	}

	dev_info(dev, "parse_dt: float_v=%d, sec_dc_name=%s\n",
	pdata->battery.chg_float_voltage,
	pdata->battery.sec_dc_name);

	return 0;
}
#endif  /* CONFIG_OF */

enum {
	ADDR = 0,
	SIZE,
	DATA,
	UPDATE,
	TA_MIN,
};
static ssize_t chg_show_attrs(struct device *dev,
	struct device_attribute *attr, char *buf);
static ssize_t chg_store_attrs(struct device *dev,
	struct device_attribute *attr,
const char *buf, size_t count);
#define CHARGER_ATTR(_name)				\
{							\
	.attr = {.name = #_name, .mode = 0660},	\
	.show = chg_show_attrs,			\
	.store = chg_store_attrs,			\
}
static struct device_attribute charger_attrs[] = {
	CHARGER_ATTR(addr),
	CHARGER_ATTR(size),
	CHARGER_ATTR(data),
	CHARGER_ATTR(update),
	CHARGER_ATTR(ta_min_v),
};
static int chg_create_attrs(struct device *dev)
{
	int i, rc;

	for (i = 0; i < (int)ARRAY_SIZE(charger_attrs); i++) {
		rc = device_create_file(dev, &charger_attrs[i]);
		if (rc)
			goto create_attrs_failed;
	}
	return rc;

create_attrs_failed:
	dev_err(dev, "%s: failed (%d)\n", __func__, rc);
	while (i--)
		device_remove_file(dev, &charger_attrs[i]);
	return rc;
}

static ssize_t chg_show_attrs(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct sm5451_charger *sm5451 = dev_get_drvdata(dev->parent);
	struct sm_dc_info *sm_dc = select_sm_dc_info(sm5451);
	const ptrdiff_t offset = attr - charger_attrs;
	int i = 0;
	u8 addr;
	u8 val;

	switch (offset) {
	case ADDR:
		i += sprintf(buf, "0x%x\n", sm5451->addr);
		break;
	case SIZE:
		i += sprintf(buf, "0x%x\n", sm5451->size);
		break;
	case DATA:
		for (addr = sm5451->addr; addr < (sm5451->addr+sm5451->size); addr++) {
			sm5451_read_reg(sm5451, addr, &val);
			i += scnprintf(buf + i, PAGE_SIZE - i,
				"0x%04x : 0x%02x\n", addr, val);
		}
		break;
	case TA_MIN:
		i += sprintf(buf, "%d\n", sm_dc->config.ta_min_voltage);
		break;
	default:
		return -EINVAL;
	}
	return i;
}

static ssize_t chg_store_attrs(struct device *dev,
		struct device_attribute *attr,
		const char *buf, size_t count)
{
	struct sm5451_charger *sm5451 = dev_get_drvdata(dev->parent);
	struct sm_dc_info *sm_dc = select_sm_dc_info(sm5451);
	const ptrdiff_t offset = attr - charger_attrs;
	int ret = 0;
	int x, y, z, k;

	switch (offset) {
	case ADDR:
		if (sscanf(buf, "0x%4x\n", &x) == 1) {
			sm5451->addr = x;
		}
		ret = count;
		break;
	case SIZE:
		if (sscanf(buf, "%5d\n", &x) == 1)
			sm5451->size = x;
		ret = count;
		break;
	case DATA:
		if (sscanf(buf, "0x%8x 0x%8x", &x, &y) == 2) {
			if ((x >= SM5451_REG_CNTL1 && x <= SM5451_REG_REG1) ||
				(x >= SM5451_REG_ADC_CNTL && x <= SM5451_REG_TOPOFF)) {
				u8 addr = x;
				u8 data = y;
				if (sm5451_write_reg(sm5451, addr, data) < 0) {
					dev_info(sm5451->dev,
					"%s: addr: 0x%x write fail\n", __func__, addr);
				}
			} else {
				dev_info(sm5451->dev,
				"%s: addr: 0x%x is wrong\n", __func__, x);
			}
		}
		ret = count;
		break;
	case UPDATE:
		if (sscanf(buf, "0x%8x 0x%8x 0x%8x %d", &x, &y, &z, &k) == 4) {
			if ((x >= SM5451_REG_CNTL1 && x <= SM5451_REG_REG1) ||
				(x >= SM5451_REG_ADC_CNTL && x <= SM5451_REG_TOPOFF)) {
				u8 addr = x, data = y, val = z, pos = k;
				if (sm5451_update_reg(sm5451, addr, data, val, pos)) {
					dev_info(sm5451->dev,
					"%s: addr: 0x%x write fail\n", __func__, addr);
				}
			} else {
				dev_info(sm5451->dev,
				"%s: addr: 0x%x is wrong\n", __func__, x);
			}
		}
		ret = count;
		break;
	case TA_MIN:
		if (sscanf(buf, "%5d\n", &x) == 1)
			sm_dc->config.ta_min_voltage = x;
		ret = count;
		break;
	default:
		ret = -EINVAL;
	}
	return ret;
}

static int sm5451_dbg_read_reg(void *data, u64 *val)
{
	struct sm5451_charger *sm5451 = data;
	int ret;
	u8 reg;

	ret = sm5451_read_reg(sm5451, sm5451->debug_address, &reg);
	if (ret < 0) {
		dev_err(sm5451->dev, "%s: failed read 0x%02x\n",
			__func__, sm5451->debug_address);
		return ret;
	}
	*val = reg;

	return 0;
}

static int sm5451_dbg_write_reg(void *data, u64 val)
{
	struct sm5451_charger *sm5451 = data;
	int ret;

	ret = sm5451_write_reg(sm5451, sm5451->debug_address, (u8)val);
	if (ret < 0) {
		dev_err(sm5451->dev, "%s: failed write 0x%02x to 0x%02x\n",
			__func__, (u8)val, sm5451->debug_address);
		return ret;
	}

	return 0;
}
DEFINE_SIMPLE_ATTRIBUTE(register_debug_ops, sm5451_dbg_read_reg, 
	sm5451_dbg_write_reg, "0x%02llx\n");

static int sm5451_create_debugfs_entries(struct sm5451_charger *sm5451)
{
	struct dentry *ent;

	sm5451->debug_root = debugfs_create_dir("charger-sm5451", NULL);
	if (!sm5451->debug_root) {
		dev_err(sm5451->dev, "%s: can't create dir\n", __func__);
		return -ENOENT;
	}

	ent = debugfs_create_x32("address", S_IFREG | S_IWUSR | S_IRUGO,
	sm5451->debug_root, &(sm5451->debug_address));
	if (!ent) {
		dev_err(sm5451->dev, "%s: can't create address\n", __func__);
		return -ENOENT;
	}

	ent = debugfs_create_file("data", S_IFREG | S_IWUSR | S_IRUGO,
	sm5451->debug_root, sm5451, &register_debug_ops);
	if (!ent) {
		dev_err(sm5451->dev, "%s: can't create data\n", __func__);
		return -ENOENT;
	}

	return 0;
}

static int sm5451_charger_probe(struct i2c_client *i2c,
			const struct i2c_device_id *id)
{
	struct sm5451_charger *sm5451;
	struct sm_dc_info *pps_dc;
	struct sm5451_platform_data *pdata;
	struct power_supply_config psy_cfg = {};
	int ret;

	dev_info(&i2c->dev, "%s: probe start\n", __func__);

	sm5451 = devm_kzalloc(&i2c->dev, sizeof(struct sm5451_charger), GFP_KERNEL);
	if (!sm5451) {
		dev_err(&i2c->dev, "%s: can't allocate devmem for sm5540\n", __func__);
		return -ENOMEM;
	}

#if defined(CONFIG_OF)
	pdata = devm_kzalloc(&i2c->dev, sizeof(struct sm5451_platform_data), GFP_KERNEL);
	if (!pdata) {
		dev_err(&i2c->dev, "%s: can't allocate devmem for platform_data\n", __func__);
		return -ENOMEM;
	}

	ret = sm5451_charger_parse_dt(&i2c->dev, pdata);
	if (ret < 0) {
		dev_err(&i2c->dev, "%s: fail to parse_dt\n", __func__);
		pdata = NULL;
	}
#else   /* CONFIG_OF */
	pdata = NULL;
#endif  /* CONFIG_OF */
	if (!pdata) {
		dev_err(&i2c->dev, "%s: we didn't support fixed platform_data yet\n", __func__);
		return -EINVAL;
	}

	/* create sm direct-charging instance for PD3.0(PPS) */
	pps_dc = sm_dc_create_pd_instance("sm5451-PD-DC", i2c);
	if (IS_ERR(pps_dc)) {
		dev_err(&i2c->dev, "%s: fail to create PD-DC module\n", __func__);
		return -ENOMEM;
	}
	pps_dc->config.ta_min_current = SM5451_TA_MIN_CURRENT;
	pps_dc->config.ta_min_voltage = 8200;
	pps_dc->config.dc_min_vbat = 3100;
	pps_dc->config.dc_vbus_ovp_th = 11000;
	pps_dc->config.pps_lr = pdata->pps_lr;
	pps_dc->config.rpara = 150000;
	pps_dc->config.rsns = 0;
	pps_dc->config.rpcm = pdata->rpcm;;
	pps_dc->config.topoff_current = 900;
	pps_dc->config.need_to_sw_ocp = 0;
	pps_dc->config.support_pd_remain = 1;        /* if pdic can't support PPS remaining, plz activate it. */
	pps_dc->config.chg_float_voltage = pdata->battery.chg_float_voltage;
	pps_dc->config.sec_dc_name = pdata->battery.sec_dc_name;
	pps_dc->ops = &sm5451_dc_pps_ops;
	ret = sm_dc_verify_configuration(pps_dc);
	if (ret < 0) {
		dev_err(&i2c->dev, "%s: fail to verify sm_dc(ret=%d)\n", __func__, ret);
		goto err_devmem;
	}
	sm5451->pps_dc = pps_dc;

	sm5451->dev = &i2c->dev;
	sm5451->i2c = i2c;
	sm5451->pdata = pdata;
	mutex_init(&sm5451->i2c_lock);
	mutex_init(&sm5451->pd_lock);
	sm5451->chg_ws = wakeup_source_register(&i2c->dev, "sm5451-charger");
	i2c_set_clientdata(i2c, sm5451);

	sm5451_hw_init_config(sm5451);

	psy_cfg.drv_data = sm5451;
	psy_cfg.supplied_to = sm5451_supplied_to;
	psy_cfg.num_supplicants = ARRAY_SIZE(sm5451_supplied_to),
	sm5451->psy_chg = power_supply_register(sm5451->dev, 
		&sm5451_charger_power_supply_desc, &psy_cfg);
	if (IS_ERR(sm5451->psy_chg)) {
		dev_err(sm5451->dev, "%s: fail to register psy_chg\n", __func__);
		ret = PTR_ERR(sm5451->psy_chg);
		goto err_devmem;
	}

	if (sm5451->pdata->irq_gpio >= 0) {
		ret = sm5451_irq_init(sm5451);
		if (ret < 0) {
			dev_err(sm5451->dev, "%s: fail to init irq(ret=%d)\n", __func__, ret);
			goto err_psy_chg;
		}
	} else {
		dev_warn(sm5451->dev, "%s: didn't assigned irq_gpio\n", __func__);
	}

	ret = chg_create_attrs(&sm5451->psy_chg->dev);
	if (ret) {
		dev_err(sm5451->dev, "%s : Failed to create_attrs\n", __func__);
	}

	ret = sm5451_create_debugfs_entries(sm5451);
	if (ret < 0) {
		dev_err(sm5451->dev, "%s: fail to create debugfs(ret=%d)\n", __func__, ret);
		goto err_psy_chg;
	}

	dev_info(sm5451->dev, "%s: done. (rev_id=0x%x)[%s]\n", __func__,
		sm5451->pdata->rev_id, SM5451_DC_VERSION);

	return 0;

	err_psy_chg:
	power_supply_unregister(sm5451->psy_chg);

	err_devmem:
	mutex_destroy(&sm5451->i2c_lock);
	mutex_destroy(&sm5451->pd_lock);
	wakeup_source_unregister(sm5451->chg_ws);
	sm_dc_destroy_instance(sm5451->pps_dc);

	return ret;
}

static int sm5451_charger_remove(struct i2c_client *i2c)
{
	struct sm5451_charger *sm5451 = i2c_get_clientdata(i2c);

	sm5451_stop_charging(sm5451);
	sm5451_sw_reset(sm5451);

	power_supply_unregister(sm5451->psy_chg);
	mutex_destroy(&sm5451->i2c_lock);
	mutex_destroy(&sm5451->pd_lock);
	wakeup_source_unregister(sm5451->chg_ws);
	sm_dc_destroy_instance(sm5451->pps_dc);

	return 0;
}

static void sm5451_charger_shutdown(struct i2c_client *i2c)
{
	struct sm5451_charger *sm5451 = i2c_get_clientdata(i2c);

	sm5451_stop_charging(sm5451);
	sm5451_reverse_boost_enable(sm5451, false);
}

static const struct i2c_device_id sm5451_charger_id_table[] = {
	{ "sm5451-charger", 0 },
	{ }
};
MODULE_DEVICE_TABLE(i2c, sm5451_charger_id_table);

#if defined(CONFIG_OF)
static struct of_device_id sm5451_of_match_table[] = {
	{ .compatible = "siliconmitus,sm5451" },
	{ },
};
MODULE_DEVICE_TABLE(of, sm5451_of_match_table);
#endif /* CONFIG_OF */

#if defined(CONFIG_PM)
static int sm5451_charger_suspend(struct device *dev)
{
	struct i2c_client *i2c = container_of(dev, struct i2c_client, dev);
	struct sm5451_charger *sm5451 = i2c_get_clientdata(i2c);

	if (device_may_wakeup(dev))
		enable_irq_wake(sm5451->irq);

	disable_irq(sm5451->irq);

	return 0;
}

static int sm5451_charger_resume(struct device *dev)
{
	struct i2c_client *i2c = container_of(dev, struct i2c_client, dev);
	struct sm5451_charger *sm5451 = i2c_get_clientdata(i2c);

	if (device_may_wakeup(dev))
		disable_irq_wake(sm5451->irq);

	enable_irq(sm5451->irq);

	return 0;
}
#else   /* CONFIG_PM */
#define sm5451_charger_suspend		NULL
#define sm5451_charger_resume		NULL
#endif  /* CONFIG_PM */

const struct dev_pm_ops sm5451_pm_ops = {
	.suspend = sm5451_charger_suspend,
	.resume = sm5451_charger_resume,
};

static struct i2c_driver sm5451_charger_driver = {
	.driver = {
		.name = "sm5451-charger",
		.owner	= THIS_MODULE,
#if defined(CONFIG_OF)
		.of_match_table = sm5451_of_match_table,
#endif  /* CONFIG_OF */
#if defined(CONFIG_PM)
		.pm = &sm5451_pm_ops,
#endif  /* CONFIG_PM */  
	},
	.probe        = sm5451_charger_probe,
	.remove       = sm5451_charger_remove,
	.shutdown     = sm5451_charger_shutdown,
	.id_table     = sm5451_charger_id_table,
};

static int __init sm5451_i2c_init(void)
{
	pr_info("sm5451-charger: %s\n", __func__);
	return i2c_add_driver(&sm5451_charger_driver);
}
module_init(sm5451_i2c_init);

static void __exit sm5451_i2c_exit(void)
{
	i2c_del_driver(&sm5451_charger_driver);
}
module_exit(sm5451_i2c_exit);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("SiliconMitus <sungdae.choi@siliconmitus.com>");
MODULE_DESCRIPTION("Charger driver for SM5451");
MODULE_VERSION(SM5451_DC_VERSION);
