
/*
 * sm5451_direct_charger.c - Direct charging module on the SM ICs
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
#if IS_ENABLED(CONFIG_BATTERY_SAMSUNG)
#include "../../common/sec_charging_common.h"
#include "../../common/sec_direct_charger.h"
#endif
#include "sm5451_direct_charger.h"

/**
 *  Internal support functions for Direct-charging
 *    - used sharing PD3.0 & WPC3.0
 */
static u32 pps_v(u32 vol)
{
	if ((vol%PPS_V_STEP) >= (PPS_V_STEP / 2))
		vol += PPS_V_STEP;

	return (vol / PPS_V_STEP) * PPS_V_STEP;
}

static u32 pps_c(u32 cur)
{
	if ((cur % PPS_C_STEP) >= (PPS_C_STEP / 2))
		cur += PPS_C_STEP;

	return (cur / PPS_C_STEP) * PPS_C_STEP;
}

static u32 wpc_v(u32 vol)
{
	if ((vol%WPC_V_STEP) >= (WPC_V_STEP / 2))
		vol += WPC_V_STEP;

	return (vol / WPC_V_STEP) * WPC_V_STEP;
}

static int report_dc_state(struct sm_dc_info *sm_dc)
{
	union power_supply_propval val = {0,};
	static int prev_val = SEC_DIRECT_CHG_MODE_DIRECT_OFF;

	switch (sm_dc->state) {
	case SM_DC_CHG_OFF:
	case SM_DC_ERR:
		val.intval = SEC_DIRECT_CHG_MODE_DIRECT_OFF;
		break;
	case SM_DC_EOC:
		val.intval = SEC_DIRECT_CHG_MODE_DIRECT_DONE;
		break;
	case SM_DC_CHECK_VBAT:
		val.intval = SEC_DIRECT_CHG_MODE_DIRECT_CHECK_VBAT;
		break;
	case SM_DC_PRESET:
		val.intval = SEC_DIRECT_CHG_MODE_DIRECT_PRESET;
		break;
	case SM_DC_PRE_CC:
	case SM_DC_UPDAT_BAT:
		val.intval = SEC_DIRECT_CHG_MODE_DIRECT_ON_ADJUST;
		break;
	case SM_DC_CC:
	case SM_DC_CV:
		val.intval = SEC_DIRECT_CHG_MODE_DIRECT_ON;
		break;
	default:
		return -EINVAL;
	}

	if (prev_val != val.intval) {
		psy_do_property(sm_dc->config.sec_dc_name, set,
				POWER_SUPPLY_EXT_PROP_DIRECT_CHARGER_MODE, val);
	}

	return 0;
}

static int request_state_work(struct sm_dc_info *sm_dc, u8 state, u32 delay)
{
	int ret = 0;

	mutex_lock(&sm_dc->st_lock);

	switch (state) {
	case SM_DC_CHECK_VBAT:
		queue_delayed_work(sm_dc->dc_wqueue, &sm_dc->check_vbat_work,
				msecs_to_jiffies(delay));
		break;
	case SM_DC_PRESET:
		queue_delayed_work(sm_dc->dc_wqueue, &sm_dc->preset_dc_work,
				msecs_to_jiffies(delay));
		break;
	case SM_DC_PRE_CC:
		queue_delayed_work(sm_dc->dc_wqueue, &sm_dc->pre_cc_work,
				msecs_to_jiffies(delay));
		break;
	case SM_DC_CC:
		queue_delayed_work(sm_dc->dc_wqueue, &sm_dc->cc_work,
				msecs_to_jiffies(delay));
		break;
	case SM_DC_CV:
		queue_delayed_work(sm_dc->dc_wqueue, &sm_dc->cv_work,
				msecs_to_jiffies(delay));
		break;
	case SM_DC_UPDAT_BAT:
		queue_delayed_work(sm_dc->dc_wqueue, &sm_dc->update_bat_work,
				msecs_to_jiffies(delay));
		break;
	case SM_DC_ERR:
		queue_delayed_work(sm_dc->dc_wqueue, &sm_dc->error_work,
				msecs_to_jiffies(delay));
		break;
	default:
		pr_err("%s %s: invalid state(%d)\n", sm_dc->name, __func__, state);
		ret = -EINVAL;
		break;
	}

	mutex_unlock(&sm_dc->st_lock);

	return ret;
}

static int update_work_state(struct sm_dc_info *sm_dc, u8 state)
{
	int ret = 0;

	if (sm_dc->state == SM_DC_CHG_OFF) {
		pr_err("%s %s: detected chg_off, terminate work\n", sm_dc->name, __func__);
		return -EBUSY;
	}

	pr_info("%s %s: sm_dc->state=%d, state=%d, update(%d,%d,%d)\n",
		sm_dc->name, __func__, sm_dc->state, state, sm_dc->req_update_vbat,
		sm_dc->req_update_ibus, sm_dc->req_update_ibat);

	if (sm_dc->state > SM_DC_PRESET && state > SM_DC_PRESET) {		/* going on charging-cycle */
		if (sm_dc->req_update_vbat || sm_dc->req_update_ibus || sm_dc->req_update_ibat) {
			/* Requested update charging-param */
			pr_info("%s %s: changed chg param, request: update_bat\n", sm_dc->name, __func__);
			request_state_work(sm_dc, SM_DC_UPDAT_BAT, DELAY_NONE);
			ret = -EINVAL;
		}
	}

	if (sm_dc->state != state) {
		mutex_lock(&sm_dc->st_lock);
		sm_dc->state = state;
		mutex_unlock(&sm_dc->st_lock);
		report_dc_state(sm_dc);
	}

	return ret;
}

static int send_power_source_msg(struct sm_dc_info *sm_dc)
{
	int ret;

	if (sm_dc->state < SM_DC_CHECK_VBAT)
		return -EINVAL;

	if (sm_dc->ta.v > sm_dc->ta.v_max || sm_dc->ta.c > sm_dc->ta.c_max) {
		pr_err("%s %s: ERROR: out of bounce v=%dmV(max=%dmV) c=%dmA(max=%dmA)\n",
			sm_dc->name, __func__, sm_dc->ta.v, sm_dc->ta.v_max,
			sm_dc->ta.c, sm_dc->ta.c_max);

		sm_dc->err = SM_DC_ERR_SEND_PD_MSG;
		request_state_work(sm_dc, SM_DC_ERR, DELAY_NONE);
		return -EINVAL;
	}


	pr_info("%s %s: [send PWR_MSG] pdo=%d, v=%dmV(max=%dmV) c=%dmA(max=%dmA)\n",
			sm_dc->name, __func__, sm_dc->ta.pdo_pos, sm_dc->ta.v,
			sm_dc->ta.v_max, sm_dc->ta.c, sm_dc->ta.c_max);


	ret = sm_dc->ops->send_power_source_msg(sm_dc->i2c, &sm_dc->ta);
	if (ret < 0) {
		pr_err("%s %s: fail to send msg(ret=%d)\n", sm_dc->name, __func__, ret);
		sm_dc->err = SM_DC_ERR_SEND_PD_MSG;
		request_state_work(sm_dc, SM_DC_ERR, DELAY_NONE);
	}

	return ret;
}

static int setup_direct_charging_work_config(struct sm_dc_info *sm_dc)
{
	sm_dc->wq.pps_cl = 0;
	sm_dc->wq.c_down = 0;
	sm_dc->wq.c_up = 0;
	sm_dc->wq.v_down = 0;
	sm_dc->wq.v_up = 0;
	sm_dc->wq.prev_adc_ibus = 0;
	sm_dc->wq.prev_adc_vbus = 0;
	sm_dc->wq.cc_limit = 0;
	sm_dc->wq.cc_cnt = 0;
	sm_dc->wq.cv_cnt = 0;
	sm_dc->wq.cv_gl = sm_dc->target_vbat;
	sm_dc->wq.ci_gl = MIN(sm_dc->ta.c_max, ((sm_dc->target_ibus * 100) / 100));
	sm_dc->wq.cc_gl = sm_dc->wq.ci_gl * 2;
	sm_dc->wq.retry_cnt = 0;

	pr_info("%s %s: CV_GL=%dmV, CI_GL=%dmA, CC_GL=%dmA\n", sm_dc->name, __func__,
			sm_dc->wq.cv_gl, sm_dc->wq.ci_gl, sm_dc->wq.cc_gl);
	sm_dc->ops->set_charging_config(sm_dc->i2c, sm_dc->wq.cv_gl, sm_dc->wq.ci_gl, sm_dc->wq.cc_gl);

	return 0;
}

static int check_error_state(struct sm_dc_info *sm_dc, u8 retry_state)
{
	int adc_vbat;

	if (sm_dc->state == SM_DC_ERR) {
		pr_err("%s %s: already occurred error (err=0x%x)\n", sm_dc->name, __func__, sm_dc->err);
		return -EINVAL;
	}

	sm_dc->err = sm_dc->ops->get_dc_error_status(sm_dc->i2c);
	if (sm_dc->err == SM_DC_ERR_RETRY) {
		pr_err("%s %s: error status retry, wait 2sec\n", sm_dc->name, __func__);
		request_state_work(sm_dc, retry_state, DELAY_RETRY);
		return -EAGAIN;
	} else if (sm_dc->err == SM_DC_ERR_VBATREG || sm_dc->err == SM_DC_ERR_IBUSREG) {
		return sm_dc->err;
	} else if (sm_dc->err > SM_DC_ERR_NONE) {
		pr_err("%s %s: error status:0x%x\n", sm_dc->name, __func__, sm_dc->err);
		request_state_work(sm_dc, SM_DC_ERR, DELAY_NONE);
		return -EPERM;
	}

	adc_vbat = sm_dc->ops->get_adc_value(sm_dc->i2c, SM_DC_ADC_VBAT);
	if (adc_vbat <= sm_dc->config.dc_min_vbat) {
		pr_err("%s %s: abnormal adc_vbat(%d)\n", sm_dc->name, __func__, adc_vbat);
		sm_dc->err = SM_DC_ERR_INVAL_VBAT;
		request_state_work(sm_dc, SM_DC_ERR, DELAY_NONE);
		return -ERANGE;
	}

	return 0;
}

static int get_adc_values(struct sm_dc_info *sm_dc, const char *str, int *vbus, int *ibus, int *vout,
		int *vbat, int *ibat, int *them, int *dietemp)
{
	int adc_vbus, adc_ibus, adc_vout, adc_vbat, adc_ibat, adc_them, adc_dietemp;

	adc_vbus = sm_dc->ops->get_adc_value(sm_dc->i2c, SM_DC_ADC_VBUS);
	adc_ibus = sm_dc->ops->get_adc_value(sm_dc->i2c, SM_DC_ADC_IBUS);
	adc_vout = sm_dc->ops->get_adc_value(sm_dc->i2c, SM_DC_ADC_VOUT);
	adc_vbat = sm_dc->ops->get_adc_value(sm_dc->i2c, SM_DC_ADC_VBAT);
	adc_ibat = sm_dc->ops->get_adc_value(sm_dc->i2c, SM_DC_ADC_IBAT);
	adc_them = sm_dc->ops->get_adc_value(sm_dc->i2c, SM_DC_ADC_THEM);
	adc_dietemp = sm_dc->ops->get_adc_value(sm_dc->i2c, SM_DC_ADC_DIETEMP);

	pr_info("%s %s:vbus:%d:ibus:%d:vout:%d:vbat:%d:ibat:%d:them:%d:dietemp:%d\n",
		sm_dc->name, str, adc_vbus, adc_ibus, adc_vout,
		adc_vbat, adc_ibat, adc_them, adc_dietemp);

	if (vbus)
		*vbus = adc_vbus;

	if (ibus)
		*ibus = adc_ibus;

	if (vout)
		*vout = adc_vout;

	if (vbat)
		*vbat = adc_vbat;

	if (ibat)
		*ibat = adc_ibat;

	if (them)
		*them = adc_them;

	if (dietemp)
		*dietemp = adc_dietemp;

	return 0;
}

static int terminate_charging_work(struct sm_dc_info *sm_dc)
{
	flush_workqueue(sm_dc->dc_wqueue);

	cancel_delayed_work_sync(&sm_dc->check_vbat_work);
	cancel_delayed_work_sync(&sm_dc->preset_dc_work);
	cancel_delayed_work_sync(&sm_dc->pre_cc_work);
	cancel_delayed_work_sync(&sm_dc->cc_work);
	cancel_delayed_work_sync(&sm_dc->cv_work);
	cancel_delayed_work_sync(&sm_dc->update_bat_work);
	cancel_delayed_work_sync(&sm_dc->error_work);

	sm_dc->ops->set_charging_enable(sm_dc->i2c, 0);

	return 0;
}

/**
 *  PD3.0 PPS Direct-charging work functions
 */
static inline u32 _calc_pps_v_init_offset(struct sm_dc_info *sm_dc)
{
	u32 offset;

	offset = (sm_dc->ta.c * (sm_dc->config.pps_lr + sm_dc->config.rpara +
				(sm_dc->config.rsns * 2) + (sm_dc->config.rpcm * 2))) / 1000000;
	offset += 200;   /* add to extra initial offset */
	pr_info("%s %s: pps_c=%dmA, v_init_offset=%dmV\n", sm_dc->name, __func__, sm_dc->ta.c, offset);

	return offset;
}

static inline int _adjust_pps_v(struct sm_dc_info *sm_dc, int pps_v_original)
{
	int i, adc_vbus, ret;

	adc_vbus = sm_dc->ops->get_adc_value(sm_dc->i2c, SM_DC_ADC_VBUS);
	pr_info("%s %s: adc_vbus=%dmV, pps_v_original=%dmV\n",
		sm_dc->name, __func__, adc_vbus, pps_v_original);

	if (adc_vbus >= pps_v_original - PPS_V_STEP) {
		/* case ADC_VBUS higher than PPS_V (adjustment max offset = 100mV) */
		sm_dc->wq.v_offset = MIN((((adc_vbus - pps_v_original) / PPS_V_STEP) * PPS_V_STEP), 100) * (-1);
		msleep(DELAY_PPS_UPDATE);
	} else {
		/* case ADC_VBUS lower than PPS_V (adjustment max offset = 100mV) */
		for (i = 0; i < 5; ++i) {
			if (adc_vbus >= pps_v_original - PPS_V_STEP)
				break;

			sm_dc->ta.v += PPS_V_STEP;
			ret = send_power_source_msg(sm_dc);
			if (ret < 0)
				return -1;

			msleep(DELAY_PPS_UPDATE);
			adc_vbus = sm_dc->ops->get_adc_value(sm_dc->i2c, SM_DC_ADC_VBUS);
			pr_info("%s %s: adc_vbus=%dmV, pps_v_original=%dmV\n",
				sm_dc->name, __func__, adc_vbus, pps_v_original);
		}
		sm_dc->wq.v_offset = sm_dc->ta.v - pps_v_original;
	}

	pr_info("%s %s: pps_v_offset=%dmV\n", sm_dc->name, __func__, sm_dc->wq.v_offset);
	return 0;
}

static inline int _pd_pre_cc_check_limitation(struct sm_dc_info *sm_dc, int adc_ibus, int adc_vbus)
{
	u32 calc_pps_v, calc_reg_v;
	int ret = 0;

	if (adc_ibus == sm_dc->wq.prev_adc_ibus && adc_vbus == sm_dc->wq.prev_adc_vbus) {
		pr_info("%s %s: adc didn't update yet\n", sm_dc->name, __func__);
		/* if continuos adc didn't update yet */
		return 0;
	}

	if ((adc_ibus > sm_dc->ta.c - PRE_CC_ST_IBUS_OFFSET) &&
			(adc_vbus < sm_dc->wq.prev_adc_vbus + (PPS_V_STEP/2)) &&
			(adc_ibus < sm_dc->wq.prev_adc_ibus + (PPS_C_STEP)) &&
			(sm_dc->wq.c_down == 0 && sm_dc->wq.pps_cl == 0)) {
		ret = 1;
	} else if (sm_dc->wq.ci_gl == sm_dc->config.ta_min_current) {     /* Case: didn't reduce PPS_C than TA_MIN_C */
		ret = 1;
	}

	if (ret) {
		calc_reg_v = (sm_dc->wq.ci_gl * (sm_dc->config.pps_lr + sm_dc->config.rpara +
					sm_dc->config.rsns * 2 + sm_dc->config.rpcm * 2)) / 1000000;
		if (sm_dc->ta.v_max < SM_DC_BYPASS_TA_MAX_VOL) {
			calc_pps_v = (sm_dc->target_vbat) + calc_reg_v + sm_dc->wq.v_offset;
			calc_pps_v = MIN(calc_pps_v, sm_dc->ta.v_max);
		} else {
			calc_pps_v = (sm_dc->target_vbat * 2) + calc_reg_v + sm_dc->wq.v_offset;
		}
		sm_dc->ta.v = pps_v(MIN(calc_pps_v, sm_dc->config.dc_vbus_ovp_th - 500));
		pr_info("%s %s: PPS_LR=%d, RPARA=%d, RSNS=%d, RPCM=%d, calc_reg_v=%dmV, calc_pps_v=%dmV\n",
			sm_dc->name, __func__, sm_dc->config.pps_lr, sm_dc->config.rpara,
			sm_dc->config.rsns, sm_dc->config.rpcm,	calc_reg_v, calc_pps_v);
		sm_dc->ta.c = sm_dc->ta.c;
		sm_dc->wq.pps_cl = 1;
	}

	return ret;
}

static inline int _try_to_adjust_cc_up(struct sm_dc_info *sm_dc)
{
	sm_dc->wq.cc_cnt += 1;

	if (sm_dc->wq.cc_cnt % 2) {
		if (sm_dc->ta.v + (PPS_V_STEP * 2) <= sm_dc->config.dc_vbus_ovp_th - 500) {
			sm_dc->ta.v += PPS_V_STEP * 2;
			if (sm_dc->ta.v > sm_dc->ta.v_max)
				sm_dc->ta.v = sm_dc->ta.v_max;
		} else {
			sm_dc->ta.c += PPS_C_STEP;
			if (sm_dc->ta.c > sm_dc->ta.c_max)
				sm_dc->ta.c = sm_dc->ta.c_max;
		}
	} else {
		if (sm_dc->ta.c + PPS_C_STEP <= sm_dc->target_ibus) {
			sm_dc->ta.c += PPS_C_STEP;
			if (sm_dc->ta.c > sm_dc->ta.c_max)
				sm_dc->ta.c = sm_dc->ta.c_max;
		}
	}

	if ((sm_dc->ta.v * sm_dc->ta.c >= sm_dc->ta.p_max) ||
		(sm_dc->ta.v == sm_dc->ta.v_max) || (sm_dc->ta.c == sm_dc->ta.c_max)) {
		pr_info("%s %s: PPS-TA has been reached limitation(v=%dmV, c=%dmA)\n",
			sm_dc->name, __func__, sm_dc->ta.v, sm_dc->ta.c);
		sm_dc->wq.cc_limit = 1;

		return -EINVAL;
	}

	return 0;
}

static inline void _try_to_adjust_cc_down(struct sm_dc_info *sm_dc)
{
	if (sm_dc->ta.v > sm_dc->config.ta_min_voltage)
		sm_dc->ta.v -= PPS_V_STEP;

	return;
}

static void pd_check_vbat_work(struct work_struct *work)
{
	struct sm_dc_info *sm_dc = container_of(work, struct sm_dc_info, check_vbat_work.work);
	struct sm_dc_power_source_info ta;
	union power_supply_propval val;
	int adc_vbat;
	int ret;

	ret = update_work_state(sm_dc, SM_DC_CHECK_VBAT);
	if (ret < 0)
		return;

	ret = psy_do_property(sm_dc->config.sec_dc_name, get,
			POWER_SUPPLY_EXT_PROP_CHARGING_ENABLED_DC, val);
	if (ret < 0) {
		pr_err("%s %s: is not ready to work (wait 1sec)\n", sm_dc->name, __func__);
		request_state_work(sm_dc, SM_DC_CHECK_VBAT, DELAY_ADC_UPDATE);
		return;
	}

	ret = sm_dc->ops->get_apdo_max_power(sm_dc->i2c, &ta);
	if (ret < 0) {
		if (sm_dc->ta.retry_cnt < 3) {
			pr_err("%s %s: get_apdo_max_power, RETRY=%d\n",
				sm_dc->name, __func__, sm_dc->ta.retry_cnt);
			sm_dc->ta.retry_cnt++;
			request_state_work(sm_dc, SM_DC_CHECK_VBAT, DELAY_PPS_UPDATE);
			return;
		} else {
			pr_err("%s %s: fail to get APDO(ret=%d)\n", sm_dc->name, __func__, ret);
			sm_dc->err = SM_DC_ERR_SEND_PD_MSG;
			request_state_work(sm_dc, SM_DC_ERR, DELAY_NONE);
			return;
		}
	}

	val.intval = 0;

	if (val.intval == 0) {  /* already disabled switching charger */
		pr_info("%s %s: [request] check_vbat -> preset\n", sm_dc->name, __func__);
		request_state_work(sm_dc, SM_DC_PRESET, DELAY_NONE);

		psy_do_property(sm_dc->config.sec_dc_name, set,
				POWER_SUPPLY_EXT_PROP_CHARGING_ENABLED_DC, val);
	} else {
		adc_vbat = sm_dc->ops->get_adc_value(sm_dc->i2c, SM_DC_ADC_VBAT);
		pr_info("%s %s: adc:vbat=%dmV\n", sm_dc->name, __func__, adc_vbat);

		if (adc_vbat > sm_dc->config.dc_min_vbat) {
			pr_info("%s %s: set_prop - disable sw_chg\n", sm_dc->name, __func__);
			val.intval = 0;
			psy_do_property(sm_dc->config.sec_dc_name, set,
					POWER_SUPPLY_EXT_PROP_CHARGING_ENABLED_DC, val);
		}
		pr_err("%s %s: sw_chg not disabled yet (wait 1sec)\n", sm_dc->name, __func__);
		request_state_work(sm_dc, SM_DC_CHECK_VBAT, DELAY_ADC_UPDATE);
	}
}

static void pd_preset_dc_work(struct work_struct *work)
{
	struct sm_dc_info *sm_dc = container_of(work, struct sm_dc_info, preset_dc_work.work);
	int adc_vbat, ret, delay = DELAY_PPS_UPDATE;

	ret = update_work_state(sm_dc, SM_DC_PRESET);
	if (ret < 0)
		return;

	adc_vbat = sm_dc->ops->get_adc_value(sm_dc->i2c, SM_DC_ADC_VBAT);
	if (adc_vbat > sm_dc->target_vbat + PPS_V_STEP) {     /* Debounce ADC accuracy */
		pr_err("%s %s: adc_vbat(%dmV) > target_v, can't start dc\n", sm_dc->name, __func__, adc_vbat);
		sm_dc->err = SM_DC_ERR_INVAL_VBAT;
		request_state_work(sm_dc, SM_DC_ERR, DELAY_NONE);
		return;
	}
	pr_info("%s %s: adc_vbat=%dmV, ta_min_v=%dmV, v_max=%dmA, c_max=%dmA, target_ibus=%dmA\n",
			sm_dc->name, __func__, adc_vbat, sm_dc->config.ta_min_voltage, sm_dc->ta.v_max,
			sm_dc->ta.c_max, sm_dc->target_ibus);

	sm_dc->ta.c = MIN(sm_dc->ta.c_max, ((sm_dc->target_ibus * 50) / 100));
	sm_dc->ta.c = pps_c(MAX(sm_dc->ta.c, sm_dc->config.ta_min_current));
	if (sm_dc->ta.v_max < SM_DC_BYPASS_TA_MAX_VOL) {
		sm_dc->ta.v = adc_vbat + _calc_pps_v_init_offset(sm_dc);
		sm_dc->ta.v = pps_v(MAX(sm_dc->ta.v, sm_dc->config.ta_min_voltage / 2));
	} else {
		sm_dc->ta.v = (2 * adc_vbat) + _calc_pps_v_init_offset(sm_dc);
		sm_dc->ta.v = pps_v(MAX(sm_dc->ta.v, sm_dc->config.ta_min_voltage));
	}
	ret = send_power_source_msg(sm_dc);
	if (ret < 0)
		return;

	msleep(DELAY_PPS_UPDATE);

	if (sm_dc->ops->get_charging_enable(sm_dc->i2c) == 0x0) {
		ret = _adjust_pps_v(sm_dc, sm_dc->ta.v);
		if (ret < 0)
			return;
	}

	if (sm_dc->target_ibus < sm_dc->wq.ci_gl)
		delay = DELAY_ADC_UPDATE;

	setup_direct_charging_work_config(sm_dc);
	sm_dc->ops->set_charging_enable(sm_dc->i2c, 1);
	pr_info("%s %s: enable Direct-charging\n", sm_dc->name, __func__);
	/* Pre-update PRE_CC state. for check to charging initial error case */
	ret = update_work_state(sm_dc, SM_DC_PRE_CC);
	if (ret < 0)
		return;

	pr_info("%s %s: [request] preset -> pre_cc\n", sm_dc->name, __func__);
	request_state_work(sm_dc, SM_DC_PRE_CC, delay);
}

static void pd_pre_cc_work(struct work_struct *work)
{
	struct sm_dc_info *sm_dc = container_of(work, struct sm_dc_info, pre_cc_work.work);
	int ret, adc_ibus, adc_vbus, adc_vbat;
	int delay_time = DELAY_PPS_UPDATE;
	u8 loop_status = LOOP_INACTIVE;

	pr_info("%s %s: (CI_GL=%dmA)\n", sm_dc->name, __func__, sm_dc->wq.ci_gl);
	ret = check_error_state(sm_dc, SM_DC_PRE_CC);
	if (ret < 0)
		return;
	if (ret == SM_DC_ERR_VBATREG)
		loop_status = LOOP_VBATREG;
	else if (ret == SM_DC_ERR_IBUSREG)
		loop_status = LOOP_IBUSREG;

	ret = update_work_state(sm_dc, SM_DC_PRE_CC);
	if (ret < 0)
		return;

	get_adc_values(sm_dc, "[adc-values]:pre_cc_work", &adc_vbus, &adc_ibus, NULL,
			&adc_vbat, NULL, NULL, NULL);

	switch (loop_status) {
	case LOOP_VBATREG:
		sm_dc->wq.cv_cnt = 0;
		pr_info("%s %s: [request] pre-cc -> cv\n", sm_dc->name, __func__);
		sm_dc->ops->set_adc_mode(sm_dc->i2c, SM_DC_ADC_MODE_CONTINUOUS);
		request_state_work(sm_dc, SM_DC_CV, DELAY_ADC_UPDATE);
		return;
	case LOOP_IBUSREG:
		if (sm_dc->config.need_to_sw_ocp && sm_dc->wq.c_down == 1) {
			pr_info("%s %s: call check_sw_ocp\n", sm_dc->name, __func__);
			ret = sm_dc->ops->check_sw_ocp(sm_dc->i2c);
			if (ret < 0)
				return;
		}
		sm_dc->ta.c -= PPS_C_STEP;
		if (sm_dc->ta.c < sm_dc->config.ta_min_current) {
			sm_dc->ta.c = sm_dc->config.ta_min_current;
			pr_info("%s %s: can't used less then ta_min_current\n",
				sm_dc->name, __func__);
			sm_dc->ta.v -= PPS_V_STEP;
			if (sm_dc->ta.v < sm_dc->config.ta_min_voltage) {
				sm_dc->wq.c_offset = 0;
				sm_dc->wq.cc_limit = 0;
				sm_dc->wq.cc_cnt = 0;
				pr_info("%s %s: [request] pre_cc -> cc\n", sm_dc->name, __func__);
				sm_dc->ops->set_adc_mode(sm_dc->i2c, SM_DC_ADC_MODE_CONTINUOUS);
				request_state_work(sm_dc, SM_DC_CC, DELAY_ADC_UPDATE);
				return;
			}
		}
		ret = send_power_source_msg(sm_dc);
		if (ret < 0)
			return;
		sm_dc->wq.c_down = 1;
		sm_dc->wq.c_up = 0;
		request_state_work(sm_dc, SM_DC_PRE_CC, DELAY_PPS_UPDATE);
		return;
	}

	if (sm_dc->wq.c_down) {
		pr_info("%s %s: decrease CI_GL(%dmA to %dmA)\n", sm_dc->name, __func__,
				sm_dc->wq.ci_gl, sm_dc->wq.ci_gl - PPS_C_STEP);
		sm_dc->wq.ci_gl -= PPS_C_STEP;
		sm_dc->wq.c_down = 0;
		request_state_work(sm_dc, SM_DC_PRE_CC, DELAY_PPS_UPDATE);
		return;
	}

	if (_pd_pre_cc_check_limitation(sm_dc, adc_ibus, adc_vbus)) {
		ret = send_power_source_msg(sm_dc);
		if (ret < 0)
			return;
	}

	if (adc_ibus > sm_dc->wq.ci_gl - (PPS_C_STEP * 80 / 100)) {
		sm_dc->wq.cc_limit = 0;
		sm_dc->wq.cc_cnt = 0;
		if (sm_dc->wq.ci_gl > sm_dc->ta.c)
			sm_dc->wq.c_offset = sm_dc->wq.ci_gl - sm_dc->ta.c;
		else
			sm_dc->wq.c_offset = 0;

		pr_info("%s %s: [request] pre_cc -> cc (c_offset=%d, pps_cl=%d)\n", sm_dc->name,
				__func__, sm_dc->wq.c_offset, sm_dc->wq.pps_cl);
		sm_dc->ops->set_adc_mode(sm_dc->i2c, SM_DC_ADC_MODE_CONTINUOUS);
		request_state_work(sm_dc, SM_DC_CC, DELAY_ADC_UPDATE);
		return;
	}

	if ((sm_dc->wq.pps_cl) &&
		((sm_dc->ta.v * (sm_dc->ta.c + PPS_C_STEP) >= sm_dc->ta.p_max) ||
		((sm_dc->ta.c + PPS_C_STEP) >= sm_dc->ta.c_max))) {
		sm_dc->wq.c_offset = 0;
		sm_dc->wq.cc_limit = 0;
		sm_dc->wq.cc_cnt = 0;
		pr_info("%s %s: [request] pre_cc -> cc\n", sm_dc->name, __func__);
		sm_dc->ops->set_adc_mode(sm_dc->i2c, SM_DC_ADC_MODE_CONTINUOUS);
		request_state_work(sm_dc, SM_DC_CC, DELAY_ADC_UPDATE);
		return;
	}

	if (sm_dc->wq.pps_cl) {
		if ((adc_ibus < sm_dc->wq.ci_gl - (PPS_C_STEP * 6)) &&
				(sm_dc->ta.c < ((sm_dc->wq.ci_gl * 90)/100)))
			sm_dc->ta.c += (PPS_C_STEP * 3);
		else
			sm_dc->ta.c += PPS_C_STEP;

		ret = send_power_source_msg(sm_dc);
		if (ret < 0)
			return;

		sm_dc->wq.c_up = 1;
		sm_dc->wq.c_down = 0;
	} else {
		sm_dc->ta.v += PPS_V_STEP;
		ret = send_power_source_msg(sm_dc);
		if (ret < 0)
			return;

		sm_dc->wq.v_up = 1;
		sm_dc->wq.v_down = 0;
	}

	sm_dc->wq.prev_adc_vbus = adc_vbus;
	sm_dc->wq.prev_adc_ibus = adc_ibus;
	request_state_work(sm_dc, SM_DC_PRE_CC, delay_time);
}

static void pd_cc_work(struct work_struct *work)
{
	struct sm_dc_info *sm_dc = container_of(work, struct sm_dc_info, cc_work.work);
	int ret, adc_ibus, adc_vbus, adc_vbat;
	u8 loop_status = LOOP_INACTIVE;
#if defined(CONFIG_DUAL_BATTERY_CELL_SENSING)
	union power_supply_propval value = {0,};
#endif

	pr_info("%s %s\n", sm_dc->name, __func__);

	ret = check_error_state(sm_dc, SM_DC_CC);
	if (ret < 0)
		return;
	if (ret == SM_DC_ERR_VBATREG)
		loop_status = LOOP_VBATREG;
	else if (ret == SM_DC_ERR_IBUSREG)
		loop_status = LOOP_IBUSREG;

	ret = update_work_state(sm_dc, SM_DC_CC);
	if (ret < 0)
		return;

	get_adc_values(sm_dc, "[adc-values]:cc_work", &adc_vbus, &adc_ibus, NULL,
			&adc_vbat, NULL, NULL, NULL);

#if defined(CONFIG_DUAL_BATTERY_CELL_SENSING)
	psy_do_property("battery", get,
		POWER_SUPPLY_EXT_PROP_DIRECT_VBAT_CHECK, value);
	if (value.intval) {
		pr_info("%s: CC MODE will be done by vcell\n", __func__);
		loop_status = LOOP_VBATREG;
	}
#endif
	switch (loop_status) {
	case LOOP_VBATREG:
		sm_dc->wq.cv_cnt = 0;
		pr_info("%s %s: [request] cc -> cv\n", sm_dc->name, __func__);
		request_state_work(sm_dc, SM_DC_CV, DELAY_ADC_UPDATE);
		return;
	case LOOP_IBUSREG:
		if (sm_dc->config.need_to_sw_ocp) {
			pr_info("%s %s: call check_sw_ocp\n", sm_dc->name, __func__);
			ret = sm_dc->ops->check_sw_ocp(sm_dc->i2c);
			if (ret < 0)
				return;
		}
		break;
	}

	/* CC_STEP_DOWN */
	if (sm_dc->wq.ci_gl + CC_ST_IBUS_OFFSET < adc_ibus && (sm_dc->wq.cc_limit == 0)) {
		_try_to_adjust_cc_down(sm_dc);
		if (sm_dc->config.support_pd_remain) {
			ret = send_power_source_msg(sm_dc);
			if (ret < 0)
				return;
		}
		request_state_work(sm_dc, SM_DC_CC, DELAY_ADC_UPDATE);
		return;
	}

	if (adc_ibus >= sm_dc->wq.ci_gl - CC_ST_IBUS_OFFSET || sm_dc->wq.cc_limit) {
		if (sm_dc->config.support_pd_remain) {
			ret = send_power_source_msg(sm_dc);
			if (ret < 0)
				return;
		}
		request_state_work(sm_dc, SM_DC_CC, DELAY_CHG_LOOP);
		return;
	}

	/* CC_STEP_UP */
	ret = _try_to_adjust_cc_up(sm_dc);
	if (ret < 0) {
		if (sm_dc->config.support_pd_remain) {
			ret = send_power_source_msg(sm_dc);
			if (ret < 0)
				return;
		}
		request_state_work(sm_dc, SM_DC_CC, DELAY_CHG_LOOP);
		return;
	} else {
		pr_info("%s %s: Changed PPS Msg\n", sm_dc->name, __func__);
		ret = send_power_source_msg(sm_dc);
		if (ret < 0)
			return;

		request_state_work(sm_dc, SM_DC_CC, DELAY_ADC_UPDATE);
	}
}

static void pd_cv_work(struct work_struct *work)
{
	struct sm_dc_info *sm_dc = container_of(work, struct sm_dc_info, cv_work.work);
	int ret, adc_ibus, adc_vbus, adc_vbat, delay = DELAY_CHG_LOOP;
	u8 loop_status = LOOP_INACTIVE;
#if defined(CONFIG_DUAL_BATTERY_CELL_SENSING)
	union power_supply_propval value = {0,};
#endif

	pr_info("%s %s\n", sm_dc->name, __func__);

	ret = check_error_state(sm_dc, SM_DC_CV);
	if (ret < 0)
		return;
	if (ret == SM_DC_ERR_VBATREG)
		loop_status = LOOP_VBATREG;
	else if (ret == SM_DC_ERR_IBUSREG)
		loop_status = LOOP_IBUSREG;

	ret = update_work_state(sm_dc, SM_DC_CV);
	if (ret < 0)
		return;

	get_adc_values(sm_dc, "[adc-values]:cv_work", &adc_vbus, &adc_ibus, NULL,
			&adc_vbat, NULL, NULL, NULL);

	if ((sm_dc->wq.cv_cnt == 0) && (loop_status & (LOOP_VBATREG | LOOP_IBUSREG)))
		sm_dc->wq.cv_cnt = 1;
	else if ((sm_dc->wq.cv_cnt == 1) && (loop_status == LOOP_INACTIVE))
		sm_dc->wq.cv_cnt = 2;

	switch (loop_status) {
	case LOOP_VBATREG:
	case LOOP_IBUSREG:
		if (sm_dc->wq.cv_cnt == 1)
			/* fast decrease PPS_V during on the first vbatreg loop */
			sm_dc->ta.v -= PPS_V_STEP * 2;
		else
			sm_dc->ta.v -= PPS_V_STEP;

		ret = send_power_source_msg(sm_dc);
		if (ret < 0)
			return;

		delay = DELAY_PPS_UPDATE;
		break;
	case LOOP_THEMREG:
		if (sm_dc->config.support_pd_remain) {
			ret = send_power_source_msg(sm_dc);
			if (ret < 0)
				return;
		}
		delay = DELAY_PPS_UPDATE;
		break;
	case LOOP_IBATREG:
		/* un-used IBATREG*/
		loop_status = LOOP_INACTIVE;
		break;
	}

	/* occurred abnormal CV status */
	if (adc_vbat < sm_dc->target_vbat - 100) {
		pr_info("%s %s: adnormal cv, [request] cv -> pre_cc\n", sm_dc->name, __func__);
		sm_dc->ops->set_adc_mode(sm_dc->i2c, SM_DC_ADC_MODE_ONESHOT);
		request_state_work(sm_dc, SM_DC_PRE_CC, DELAY_ADC_UPDATE);
		return;
	}

#if defined(CONFIG_DUAL_BATTERY_CELL_SENSING)
	psy_do_property("battery", get,
		POWER_SUPPLY_EXT_PROP_DIRECT_VBAT_CHECK, value);
	if (value.intval) {
		pr_info("%s: CV MODE will be done by vcell\n", __func__);
		schedule_delayed_work(&sm_dc->done_event_work, msecs_to_jiffies(50));
	}
#endif

	/* Support to "POWER_SUPPLY_EXT_PROP_DIRECT_DONE" used ADC_IBUS */
	if (sm_dc->config.topoff_current > 0) {
		if ((sm_dc->target_vbat == sm_dc->config.chg_float_voltage) &&
			(adc_ibus < sm_dc->config.topoff_current)) {
			pr_info("%s : dc done!!\n", __func__);
			schedule_delayed_work(&sm_dc->done_event_work, msecs_to_jiffies(50));
		}
	}

	if (loop_status == LOOP_INACTIVE && sm_dc->config.support_pd_remain) {
		ret = send_power_source_msg(sm_dc);
		if (ret < 0)
			return;
	}
	request_state_work(sm_dc, SM_DC_CV, delay);
}

static void pd_update_bat_work(struct work_struct *work)
{
	struct sm_dc_info *sm_dc = container_of(work, struct sm_dc_info, update_bat_work.work);
	int index, ret, cnt;
	bool need_to_preset = 1;

	pr_info("%s %s\n", sm_dc->name, __func__);

	ret = check_error_state(sm_dc, SM_DC_UPDAT_BAT);
	if (ret < 0)
		return;

	/* waiting for step change event */
	for (cnt = 0; cnt < 1; ++cnt) {
		if (sm_dc->req_update_vbat && sm_dc->req_update_ibus)
			break;

		pr_info("%s %s: wait 1sec for step changed\n", sm_dc->name, __func__);
		msleep(1000);
	}

	mutex_lock(&sm_dc->st_lock);
	index = (sm_dc->req_update_vbat << 2) | (sm_dc->req_update_ibus << 1) | sm_dc->req_update_ibat;
	sm_dc->req_update_vbat = 0;
	sm_dc->req_update_ibus = 0;
	sm_dc->req_update_ibat = 0;
	mutex_unlock(&sm_dc->st_lock);

	ret = update_work_state(sm_dc, SM_DC_UPDAT_BAT);
	if (ret < 0)
		return;

	if (index & (0x1 << 2))
		pr_info("%s %s: vbat changed (%dmV)\n", sm_dc->name, __func__, sm_dc->target_vbat);

	if (index & (0x1 << 1))
		pr_info("%s %s: ibus changed (%dmA)\n", sm_dc->name, __func__, sm_dc->target_ibus);

	if (index & 0x1)
		pr_info("%s %s: ibat changed (%dmA)\n", sm_dc->name, __func__, sm_dc->target_ibat);

	/* check step change event */
	if ((index & (0x1 << 2)) && (index & (0x1 << 1))) {
		if ((sm_dc->target_vbat > sm_dc->wq.cv_gl) && (sm_dc->target_ibus < sm_dc->wq.ci_gl))
			need_to_preset = 0;
	}

	sm_dc->ops->set_adc_mode(sm_dc->i2c, SM_DC_ADC_MODE_ONESHOT);

	if (need_to_preset) {
		pr_info("%s %s: [request] update_bat -> preset\n", sm_dc->name, __func__);
		request_state_work(sm_dc, SM_DC_PRESET, DELAY_NONE);
	} else {
		setup_direct_charging_work_config(sm_dc);
		sm_dc->ta.c = sm_dc->wq.ci_gl - 200 - sm_dc->wq.c_offset;
		ret = send_power_source_msg(sm_dc);
		if (ret < 0)
			return;

		pr_info("%s %s: [request] update_bat -> pre_cc\n", sm_dc->name,  __func__);
		request_state_work(sm_dc, SM_DC_PRE_CC, DELAY_ADC_UPDATE);
	}
}

static void pd_error_work(struct work_struct *work)
{
	struct sm_dc_info *sm_dc = container_of(work, struct sm_dc_info, error_work.work);
	int ret;

	pr_info("%s %s: err=0x%x\n", sm_dc->name, __func__, sm_dc->err);

	ret = update_work_state(sm_dc, SM_DC_ERR);
	if (ret < 0)
		return;

	sm_dc->ops->set_adc_mode(sm_dc->i2c, SM_DC_ADC_MODE_OFF);
	sm_dc->ops->set_charging_enable(sm_dc->i2c, 0);
}

static void sec_done_event_work(struct work_struct *work)
{
	struct sm_dc_info *sm_dc = container_of(work, struct sm_dc_info, done_event_work.work);
	union power_supply_propval value = {0, };

	pr_info("%s called\n", __func__);

	value.intval = 1;
	psy_do_property(sm_dc->config.sec_dc_name, set, POWER_SUPPLY_EXT_PROP_DIRECT_DONE, value);
}

/**
 *  WPC3.0 wireless Direct-charging work functions
 */
static inline u32 _calc_wpc_v_init_offset(struct sm_dc_info *sm_dc)
{
	u32 offset;

	offset = (((sm_dc->target_ibus * 50) / 100) * (sm_dc->config.pps_lr + sm_dc->config.rpara +
				(sm_dc->config.rsns * 2) + (sm_dc->config.rpcm * 2))) / 1000000;
	pr_info("%s %s: target_ibus=%dmA, v_init_offset=%d\n", sm_dc->name, __func__, sm_dc->target_ibus, offset);

	return offset;
}

static inline u32 _calc_entry_wpc_v(struct sm_dc_info *sm_dc)
{
	int adc_vbat;
	u32 reg_v, entry_v;

	adc_vbat = sm_dc->ops->get_adc_value(sm_dc->i2c, SM_DC_ADC_VBAT);
	reg_v = (sm_dc->wq.ci_gl * (sm_dc->config.pps_lr + sm_dc->config.rpara + (sm_dc->config.rsns * 2) +
				(sm_dc->config.rpcm * 2))) / 1000000;
	entry_v = wpc_v((adc_vbat * 2) + reg_v + sm_dc->wq.v_offset);

	pr_info("%s %s: CI_GL=%d, PPS_LR=%d, RPARA=%d, RSNS=%d, RPCM=%d, reg_v=%dmV\n", sm_dc->name,
			__func__, sm_dc->wq.ci_gl, sm_dc->config.pps_lr, sm_dc->config.rpara,
			sm_dc->config.rsns, sm_dc->config.rpcm, reg_v);
	pr_info("%s %s: adc_vbat=%d, wpc_v_offset=%d, wpc_v=%d\n", sm_dc->name, __func__, adc_vbat,
			sm_dc->wq.v_offset, entry_v);

	return entry_v;
}

static inline int _adjust_wpc_v(struct sm_dc_info *sm_dc, int wpc_v_original)
{
	int i, adc_vbus, ret;

	adc_vbus = sm_dc->ops->get_adc_value(sm_dc->i2c, SM_DC_ADC_VBUS);
	pr_info("%s %s: adc_vbus=%dmV, wpc_v_original=%dmV\n", sm_dc->name, __func__, adc_vbus, wpc_v_original);

	if (adc_vbus >= wpc_v_original - WPC_V_STEP) {
		/* case ADC_VBUS higher than PPS_V (adjustment max offset = 300mV) */
		sm_dc->wq.v_offset = MIN((((adc_vbus - wpc_v_original) / WPC_V_STEP) * WPC_V_STEP), 300) * (-1);
	} else {
		/* case ADC_VBUS lower than PPS_V (adjustment max offset = 300mV) */
		for (i = 0; i < 3; ++i) {
			if (adc_vbus >= wpc_v_original - WPC_V_STEP)
				break;

			sm_dc->ta.v += WPC_V_STEP;
			ret = send_power_source_msg(sm_dc);
			if (ret < 0)
				return -1;

			msleep(DELAY_WPC_UPDATE);
			adc_vbus = sm_dc->ops->get_adc_value(sm_dc->i2c, SM_DC_ADC_VBUS);
			pr_info("%s %s: adc_vbus=%dmV, wpc_v_original=%dmV\n",
				sm_dc->name, __func__, adc_vbus, wpc_v_original);
		}
		sm_dc->wq.v_offset = sm_dc->ta.v - wpc_v_original;
	}

	pr_info("%s %s: wpc_v_offset=%dmV\n", sm_dc->name, __func__, sm_dc->wq.v_offset);
	return 0;
}

static inline u32 _wpc_calc_voltage(struct sm_dc_info *sm_dc, u32 cur_v, bool direction)
{
	u32 calc_v;

	if (direction)
		calc_v = cur_v + WPC_V_STEP;
	else
		calc_v = cur_v - WPC_V_STEP;

	return calc_v;
}

static void wpc_check_vbat_work(struct work_struct *work)
{
	struct sm_dc_info *sm_dc = container_of(work, struct sm_dc_info, check_vbat_work.work);
	union power_supply_propval val;
	int adc_vbat, cable_online;
	int ret;

	ret = update_work_state(sm_dc, SM_DC_CHECK_VBAT);
	if (ret < 0)
		return;

	psy_do_property("sm5451-charger", get, POWER_SUPPLY_PROP_ONLINE, val);
	cable_online = val.intval;

	ret = psy_do_property(sm_dc->config.sec_dc_name, get,
			POWER_SUPPLY_EXT_PROP_CHARGING_ENABLED_DC, val);
	if (ret < 0) {
		pr_err("%s %s: is not ready to work (wait 1sec)\n", sm_dc->name, __func__);
		request_state_work(sm_dc, SM_DC_CHECK_VBAT, DELAY_ADC_UPDATE);
		return;
	}

	if (is_hv_wireless_type(cable_online))
		val.intval = 0;

	if (val.intval == 0) {  /* already disabled switching charger */
		pr_info("%s %s: [request] check_vbat -> preset\n", sm_dc->name, __func__);
		request_state_work(sm_dc, SM_DC_PRESET, DELAY_NONE);
	} else {
		adc_vbat = sm_dc->ops->get_adc_value(sm_dc->i2c, SM_DC_ADC_VBAT);
		pr_info("%s %s: adc:vbat=%dmV\n", sm_dc->name, __func__, adc_vbat);

		if (adc_vbat > sm_dc->config.dc_min_vbat) {
			pr_info("%s %s: set_prop - disable sw_chg\n", sm_dc->name, __func__);
			val.intval = 0;
			psy_do_property(sm_dc->config.sec_dc_name, set,
					POWER_SUPPLY_EXT_PROP_CHARGING_ENABLED_DC, val);
		}
		pr_err("%s %s: sw_chg not disabled yet (wait 1sec)\n", sm_dc->name, __func__);
		request_state_work(sm_dc, SM_DC_CHECK_VBAT, DELAY_ADC_UPDATE);
	}
}

static void wpc_preset_dc_work(struct work_struct *work)
{
	struct sm_dc_info *sm_dc = container_of(work, struct sm_dc_info, preset_dc_work.work);
	int adc_vbat, ret, delay = DELAY_WPC_UPDATE;

	ret = update_work_state(sm_dc, SM_DC_PRESET);
	if (ret < 0)
		return;

	adc_vbat = sm_dc->ops->get_adc_value(sm_dc->i2c, SM_DC_ADC_VBAT);
	if (adc_vbat > sm_dc->target_vbat + WPC_V_STEP) {     /* Debounce ADC accuracy */
		pr_err("%s %s: adc_vbat(%dmV) > target_v, can't start dc\n", sm_dc->name, __func__, adc_vbat);
		sm_dc->err = SM_DC_ERR_INVAL_VBAT;
		request_state_work(sm_dc, SM_DC_ERR, DELAY_NONE);
		return;
	}
	pr_info("%s %s: adc_vbat=%dmV, ta_min_v=%dmV, v_max=%dmA, c_max=%dmA, target_ibus=%dmA\n",
			sm_dc->name, __func__, adc_vbat, sm_dc->config.ta_min_voltage, sm_dc->ta.v_max,
			sm_dc->ta.c_max, sm_dc->target_ibus);

	sm_dc->ta.c = sm_dc->ta.c_max;      /* wpc can't control current limit */
	sm_dc->ta.v = (2 * adc_vbat) + _calc_wpc_v_init_offset(sm_dc);
	sm_dc->ta.v = wpc_v(MAX(sm_dc->ta.v, sm_dc->config.ta_min_voltage));
	ret = send_power_source_msg(sm_dc);
	if (ret < 0)
		return;

	msleep(DELAY_WPC_UPDATE);

	if (sm_dc->ops->get_charging_enable(sm_dc->i2c) == 0x0) {
		ret = _adjust_wpc_v(sm_dc, sm_dc->ta.v);
		if (ret < 0)
			return;
	}

	if (sm_dc->target_ibus < sm_dc->wq.ci_gl)
		delay = DELAY_ADC_UPDATE;

	setup_direct_charging_work_config(sm_dc);
	sm_dc->ops->set_charging_enable(sm_dc->i2c, 1);
	pr_info("%s %s: enable Direct-charging\n", sm_dc->name, __func__);

	/* waiting soft-start charging */
	msleep(DELAY_WPC_UPDATE);

	sm_dc->ta.v = _calc_entry_wpc_v(sm_dc);
	ret = send_power_source_msg(sm_dc);
	if (ret < 0)
		return;

	/* waiting entry wpc_v setting */
	msleep(DELAY_WPC_UPDATE);

	/* Pre-update PRE_CC state. for check to charging initial error case */
	ret = update_work_state(sm_dc, SM_DC_PRE_CC);
	if (ret < 0)
		return;

	pr_info("%s %s: [request] preset -> pre_cc\n", sm_dc->name, __func__);
	request_state_work(sm_dc, SM_DC_PRE_CC, delay);
}

static void wpc_pre_cc_work(struct work_struct *work)
{
	struct sm_dc_info *sm_dc = container_of(work, struct sm_dc_info, pre_cc_work.work);
	int ret, adc_ibus, adc_vbus, adc_vbat;
	int delay_time = DELAY_WPC_UPDATE;
	u8 loop_status = LOOP_INACTIVE;
	bool dir;

	pr_info("%s %s: (CI_GL=%dmA)\n", sm_dc->name, __func__, sm_dc->wq.ci_gl);
	ret = check_error_state(sm_dc, SM_DC_PRE_CC);
	if (ret < 0)
		return;
	if (ret == SM_DC_ERR_VBATREG)
		loop_status = LOOP_VBATREG;
	else if (ret == SM_DC_ERR_IBUSREG)
		loop_status = LOOP_IBUSREG;

	ret = update_work_state(sm_dc, SM_DC_PRE_CC);
	if (ret < 0)
		return;

	get_adc_values(sm_dc, "[adc-values]:pre_cc_work", &adc_vbus, &adc_ibus, NULL,
			&adc_vbat, NULL, NULL, NULL);

	switch (loop_status) {
	case LOOP_VBATREG:
		sm_dc->wq.cv_cnt = 0;
		pr_info("%s %s: [request] pre-cc -> cv\n", sm_dc->name, __func__);
		sm_dc->ops->set_adc_mode(sm_dc->i2c, SM_DC_ADC_MODE_CONTINUOUS);
		request_state_work(sm_dc, SM_DC_CV, DELAY_ADC_UPDATE);
		return;
	case LOOP_IBUSREG:
		pr_info("%s %s: call check_sw_ocp\n", sm_dc->name, __func__);
		if (sm_dc->config.need_to_sw_ocp && sm_dc->wq.c_down == 1) {
			ret = sm_dc->ops->check_sw_ocp(sm_dc->i2c);
			if (ret < 0)
				return;
		}
		sm_dc->ta.v -= WPC_V_STEP;
		ret = send_power_source_msg(sm_dc);
		if (ret < 0)
			return;

		sm_dc->wq.c_down = 1;
		sm_dc->wq.c_up = 0;
		request_state_work(sm_dc, SM_DC_PRE_CC, DELAY_WPC_UPDATE);
		return;
	}

	if ((sm_dc->ta.v + WPC_V_STEP) >= sm_dc->ta.v_max) {
		pr_info("%s %s: can't increase voltage(v:%d v_max:%d)\n", sm_dc->name,
				__func__, sm_dc->ta.v, sm_dc->ta.v_max);
		pr_info("%s %s: [request] pre_cc -> cc\n", sm_dc->name, __func__);
		sm_dc->ops->set_adc_mode(sm_dc->i2c, SM_DC_ADC_MODE_CONTINUOUS);
		request_state_work(sm_dc, SM_DC_CC, DELAY_ADC_UPDATE);
		return;
	}

	if (adc_ibus > sm_dc->wq.ci_gl - WPC_C_STEP) {
		if (adc_ibus < sm_dc->wq.ci_gl + WPC_C_STEP) {
			pr_info("%s %s: [request] pre_cc -> cc (adc_ibus=%d)\n", sm_dc->name,
					__func__, adc_ibus);
			sm_dc->ops->set_adc_mode(sm_dc->i2c, SM_DC_ADC_MODE_CONTINUOUS);
			request_state_work(sm_dc, SM_DC_CC, DELAY_ADC_UPDATE);
			return;
		}
		dir = 0;
	} else {
		dir = 1;
	}
	sm_dc->ta.v = _wpc_calc_voltage(sm_dc, sm_dc->ta.v, dir);
	ret = send_power_source_msg(sm_dc);
	if (ret < 0)
		return;

	sm_dc->wq.prev_adc_vbus = adc_vbus;
	sm_dc->wq.prev_adc_ibus = adc_ibus;
	request_state_work(sm_dc, SM_DC_PRE_CC, delay_time);
}

static void wpc_cc_work(struct work_struct *work)
{
	struct sm_dc_info *sm_dc = container_of(work, struct sm_dc_info, cc_work.work);
	int ret, adc_ibus, adc_vbus, adc_vbat;
	u8 loop_status = LOOP_INACTIVE;
	bool dir;

	pr_info("%s %s\n", sm_dc->name, __func__);

	ret = check_error_state(sm_dc, SM_DC_CC);
	if (ret < 0)
		return;
	if (ret == SM_DC_ERR_VBATREG)
		loop_status = LOOP_VBATREG;
	else if (ret == SM_DC_ERR_IBUSREG)
		loop_status = LOOP_IBUSREG;

	ret = update_work_state(sm_dc, SM_DC_CC);
	if (ret < 0)
		return;

	get_adc_values(sm_dc, "[adc-values]:cc_work", &adc_vbus, &adc_ibus, NULL,
			&adc_vbat, NULL, NULL, NULL);

	switch (loop_status) {
	case LOOP_VBATREG:
		sm_dc->wq.cv_cnt = 0;
		pr_info("%s %s: [request] cc -> cv\n", sm_dc->name, __func__);
		request_state_work(sm_dc, SM_DC_CV, DELAY_ADC_UPDATE);
		return;
	case LOOP_IBUSREG:
		pr_info("%s %s: call check_sw_ocp\n", sm_dc->name, __func__);
		if (sm_dc->config.need_to_sw_ocp) {
			ret = sm_dc->ops->check_sw_ocp(sm_dc->i2c);
			if (ret < 0)
				return;
		}
		break;
	}

	if (adc_ibus >= sm_dc->wq.ci_gl - WPC_C_STEP) {
		if (adc_ibus < sm_dc->wq.ci_gl + WPC_C_STEP) {
			if (sm_dc->config.support_pd_remain) {
				ret = send_power_source_msg(sm_dc);
				if (ret < 0)
					return;
			}
			request_state_work(sm_dc, SM_DC_CC, DELAY_CHG_LOOP);
			return;
		}
		dir = 0;
	} else {
		dir = 1;
	}
	pr_info("%s %s: Changed WPC Msg\n", sm_dc->name, __func__);
	sm_dc->ta.v = _wpc_calc_voltage(sm_dc, sm_dc->ta.v, dir);
	if (sm_dc->ta.v >= sm_dc->ta.v_max) {
		pr_info("%s %s: can't increase wpc_v(v:%d, v_max:%d)\n",
			sm_dc->name, __func__, sm_dc->ta.v, sm_dc->ta.v_max);
		sm_dc->ta.v = sm_dc->ta.v_max;
		ret = send_power_source_msg(sm_dc);
		if (ret < 0)
			return;

		request_state_work(sm_dc, SM_DC_CC, DELAY_CHG_LOOP);
	} else {
		ret = send_power_source_msg(sm_dc);
		if (ret < 0)
			return;

		request_state_work(sm_dc, SM_DC_CC, DELAY_ADC_UPDATE);
	}
	request_state_work(sm_dc, SM_DC_CC, DELAY_ADC_UPDATE);
}

static void wpc_cv_work(struct work_struct *work)
{
	struct sm_dc_info *sm_dc = container_of(work, struct sm_dc_info, cv_work.work);
	int ret, adc_ibus, adc_vbus, adc_vbat, delay = DELAY_CHG_LOOP;
	u8 loop_status = LOOP_INACTIVE;

	pr_info("%s %s\n", sm_dc->name, __func__);

	ret = check_error_state(sm_dc, SM_DC_CV);
	if (ret < 0)
		return;
	if (ret == SM_DC_ERR_VBATREG)
		loop_status = LOOP_VBATREG;
	else if (ret == SM_DC_ERR_IBUSREG)
		loop_status = LOOP_IBUSREG;

	ret = update_work_state(sm_dc, SM_DC_CV);
	if (ret < 0)
		return;

	get_adc_values(sm_dc, "[adc-values]:cv_work", &adc_vbus, &adc_ibus, NULL,
			&adc_vbat, NULL, NULL, NULL);

	if (loop_status & LOOP_VBATREG) {
		sm_dc->ta.v -= PPS_V_STEP;
		ret = send_power_source_msg(sm_dc);
		if (ret < 0)
			return;

		delay = DELAY_WPC_UPDATE;
	}

	/* occurred abnormal CV status */
	if ((adc_vbat < sm_dc->target_vbat - 100) || (loop_status & LOOP_IBUSREG)) {
		pr_info("%s %s: adnormal cv, [request] cv -> pre_cc\n", sm_dc->name, __func__);
		sm_dc->ops->set_adc_mode(sm_dc->i2c, SM_DC_ADC_MODE_ONESHOT);
		request_state_work(sm_dc, SM_DC_PRE_CC, DELAY_ADC_UPDATE);
		return;
	}

	/* Support to "POWER_SUPPLY_EXT_PROP_DIRECT_DONE" used ADC_IBUS */
	if (sm_dc->config.topoff_current > 0) {
		if ((sm_dc->target_vbat == sm_dc->config.chg_float_voltage) &&
			(adc_ibus < sm_dc->config.topoff_current)) {
			pr_info("%s : dc done!!\n", __func__);
			schedule_delayed_work(&sm_dc->done_event_work, msecs_to_jiffies(50));
		}
	}

	if (loop_status == LOOP_INACTIVE && sm_dc->config.support_pd_remain) {
		ret = send_power_source_msg(sm_dc);
		if (ret < 0)
			return;

	}
	request_state_work(sm_dc, SM_DC_CV, delay);
}

static void wpc_update_bat_work(struct work_struct *work)
{
	struct sm_dc_info *sm_dc = container_of(work, struct sm_dc_info, update_bat_work.work);
	int index, ret, cnt;
	bool need_to_preset = 1;

	pr_info("%s %s\n", sm_dc->name, __func__);

	ret = check_error_state(sm_dc, SM_DC_UPDAT_BAT);
	if (ret < 0)
		return;

	/* waiting for step change event */
	for (cnt = 0; cnt < 1; ++cnt) {
		if (sm_dc->req_update_vbat && sm_dc->req_update_ibus)
			break;

		pr_info("%s %s: wait 1sec for step changed\n", sm_dc->name, __func__);
		msleep(1000);
	}

	mutex_lock(&sm_dc->st_lock);
	index = (sm_dc->req_update_vbat << 2) | (sm_dc->req_update_ibus << 1) | sm_dc->req_update_ibat;
	sm_dc->req_update_vbat = 0;
	sm_dc->req_update_ibus = 0;
	sm_dc->req_update_ibat = 0;
	mutex_unlock(&sm_dc->st_lock);

	ret = update_work_state(sm_dc, SM_DC_UPDAT_BAT);
	if (ret < 0)
		return;

	if (index & (0x1 << 2))
		pr_info("%s %s: vbat changed (%dmV)\n", sm_dc->name, __func__, sm_dc->target_vbat);

	if (index & (0x1 << 1))
		pr_info("%s %s: ibus changed (%dmA)\n", sm_dc->name, __func__, sm_dc->target_ibus);

	if (index & 0x1)
		pr_info("%s %s: ibat changed (%dmA)\n", sm_dc->name, __func__, sm_dc->target_ibat);

	/* check step change event */
	if ((index & (0x1 << 2)) && (index & (0x1 << 1))) {
		if ((sm_dc->target_vbat > sm_dc->wq.cv_gl) && (sm_dc->target_ibus < sm_dc->wq.ci_gl))
			need_to_preset = 0;
	}

	sm_dc->ops->set_adc_mode(sm_dc->i2c, SM_DC_ADC_MODE_ONESHOT);

	if (need_to_preset) {
		pr_info("%s %s: [request] update_bat -> preset\n", sm_dc->name, __func__);
		request_state_work(sm_dc, SM_DC_PRESET, DELAY_NONE);
	} else {
		setup_direct_charging_work_config(sm_dc);
		sm_dc->ta.v = _calc_entry_wpc_v(sm_dc);
		ret = send_power_source_msg(sm_dc);
		if (ret < 0)
			return;

		pr_info("%s %s: [request] update_bat -> pre_cc\n", sm_dc->name, __func__);
		request_state_work(sm_dc, SM_DC_PRE_CC, DELAY_ADC_UPDATE);
	}
}

static void wpc_error_work(struct work_struct *work)
{
	struct sm_dc_info *sm_dc = container_of(work, struct sm_dc_info, error_work.work);
	int ret;

	pr_info("%s %s: err=0x%x\n", sm_dc->name, __func__, sm_dc->err);

	ret = update_work_state(sm_dc, SM_DC_ERR);
	if (ret < 0)
		return;

	sm_dc->ops->set_adc_mode(sm_dc->i2c, SM_DC_ADC_MODE_OFF);
	sm_dc->ops->set_charging_enable(sm_dc->i2c, 0);
}

/**
 * SM Direct-charging module management APIs
 */
struct sm_dc_info *sm_dc_create_pd_instance(const char *name, struct i2c_client *i2c)
{
	struct sm_dc_info *sm_dc;
	int ret;

	sm_dc = kzalloc(sizeof(struct sm_dc_info), GFP_KERNEL);
	if (!sm_dc)
		return ERR_PTR(-ENOMEM);

	sm_dc->name = name;
	sm_dc->i2c = i2c;
	mutex_init(&sm_dc->st_lock);

	/* create work queue */
	sm_dc->state = SM_DC_CHG_OFF;
	sm_dc->dc_wqueue = create_singlethread_workqueue(name);
	if (!sm_dc->dc_wqueue) {
		pr_err("%s %s: fail to crearte workqueue\n", name, __func__);
		ret = -ENOMEM;
		goto err_kmem;
	}
	INIT_DELAYED_WORK(&sm_dc->check_vbat_work,  pd_check_vbat_work);
	INIT_DELAYED_WORK(&sm_dc->preset_dc_work,   pd_preset_dc_work);
	INIT_DELAYED_WORK(&sm_dc->pre_cc_work,      pd_pre_cc_work);
	INIT_DELAYED_WORK(&sm_dc->cc_work,          pd_cc_work);
	INIT_DELAYED_WORK(&sm_dc->cv_work,          pd_cv_work);
	INIT_DELAYED_WORK(&sm_dc->update_bat_work,  pd_update_bat_work);
	INIT_DELAYED_WORK(&sm_dc->error_work,       pd_error_work);
	/* for SEC_BATTERY done event process */
	INIT_DELAYED_WORK(&sm_dc->done_event_work,  sec_done_event_work);
	pr_info("%s %s: done.\n", name, __func__);

	return sm_dc;

err_kmem:
	mutex_destroy(&sm_dc->st_lock);
	kfree(sm_dc);

	return ERR_PTR(ret);
}
EXPORT_SYMBOL(sm_dc_create_pd_instance);

struct sm_dc_info *sm_dc_create_wpc_instance(const char *name, struct i2c_client *i2c)
{
	struct sm_dc_info *sm_dc;
	int ret;

	sm_dc = kzalloc(sizeof(struct sm_dc_info), GFP_KERNEL);
	if (!sm_dc)
		return ERR_PTR(-ENOMEM);

	sm_dc->name = name;
	sm_dc->i2c = i2c;
	mutex_init(&sm_dc->st_lock);

	/* create work queue */
	sm_dc->state = SM_DC_CHG_OFF;
	sm_dc->dc_wqueue = create_singlethread_workqueue(name);
	if (!sm_dc->dc_wqueue) {
		pr_err("%s %s: fail to crearte workqueue\n", name, __func__);
		ret = -ENOMEM;
		goto err_kmem;
	}
	INIT_DELAYED_WORK(&sm_dc->check_vbat_work,  wpc_check_vbat_work);
	INIT_DELAYED_WORK(&sm_dc->preset_dc_work,   wpc_preset_dc_work);
	INIT_DELAYED_WORK(&sm_dc->pre_cc_work,      wpc_pre_cc_work);
	INIT_DELAYED_WORK(&sm_dc->cc_work,          wpc_cc_work);
	INIT_DELAYED_WORK(&sm_dc->cv_work,          wpc_cv_work);
	INIT_DELAYED_WORK(&sm_dc->update_bat_work,  wpc_update_bat_work);
	INIT_DELAYED_WORK(&sm_dc->error_work,       wpc_error_work);

	pr_info("%s %s: done.\n", name, __func__);

	return sm_dc;

err_kmem:
	mutex_destroy(&sm_dc->st_lock);
	kfree(sm_dc);

	return ERR_PTR(ret);
}
EXPORT_SYMBOL(sm_dc_create_wpc_instance);

int sm_dc_verify_configuration(struct sm_dc_info *sm_dc)
{
	if (sm_dc == NULL)
		return -EINVAL;

	if (sm_dc->ops == NULL)
		return -EINVAL;

	return 0;
}
EXPORT_SYMBOL(sm_dc_verify_configuration);

void sm_dc_destroy_instance(struct sm_dc_info *sm_dc)
{
	if (sm_dc != NULL) {
		destroy_workqueue(sm_dc->dc_wqueue);
		mutex_destroy(&sm_dc->st_lock);
		kfree(sm_dc);
	}
}
EXPORT_SYMBOL(sm_dc_destroy_instance);

int sm_dc_report_error_status(struct sm_dc_info *sm_dc, u32 err)
{
	terminate_charging_work(sm_dc);
	sm_dc->state = SM_DC_ERR;
	sm_dc->err = err;
	report_dc_state(sm_dc);

	return 0;
}
EXPORT_SYMBOL(sm_dc_report_error_status);

int sm_dc_report_interrupt_event(struct sm_dc_info *sm_dc, u32 interrupt)
{
	if ((sm_dc->state == SM_DC_CC) && (interrupt == SM_DC_INT_VBATREG)) {
		if (delayed_work_pending(&sm_dc->cc_work)) {
			cancel_delayed_work(&sm_dc->cc_work);
			pr_info("%s %s: cancel CC_work, direct request work\n", sm_dc->name, __func__);
			request_state_work(sm_dc, SM_DC_CC, DELAY_NONE);
		}
	}

	return 0;
}
EXPORT_SYMBOL(sm_dc_report_interrupt_event);

int sm_dc_get_current_state(struct sm_dc_info *sm_dc)
{
	return sm_dc->state;
}
EXPORT_SYMBOL(sm_dc_get_current_state);

int sm_dc_start_charging(struct sm_dc_info *sm_dc)
{
	if (sm_dc->state >= SM_DC_CHECK_VBAT) {
		pr_err("%s %s: already work on dc (state=%d)\n", sm_dc->name, __func__, sm_dc->state);
		return -EBUSY;
	}

	sm_dc->ta.pdo_pos = 0;        /* set '0' else return error */
	sm_dc->ta.v_max = 10000;      /* request voltage level */
	sm_dc->ta.c_max = 0;
	sm_dc->ta.p_max = 0;
	sm_dc->ta.retry_cnt = 0;

	sm_dc->ops->set_adc_mode(sm_dc->i2c, SM_DC_ADC_MODE_ONESHOT);
	mutex_lock(&sm_dc->st_lock);
	sm_dc->state = SM_DC_CHECK_VBAT;   /* Pre-update chg.state */
	mutex_unlock(&sm_dc->st_lock);
	request_state_work(sm_dc, SM_DC_CHECK_VBAT, DELAY_PPS_UPDATE);

	pr_info("%s %s: done\n", sm_dc->name, __func__);

	return 0;
}
EXPORT_SYMBOL(sm_dc_start_charging);

int sm_dc_stop_charging(struct sm_dc_info *sm_dc)
{
	mutex_lock(&sm_dc->st_lock);
	sm_dc->state = SM_DC_CHG_OFF;
	mutex_unlock(&sm_dc->st_lock);
	terminate_charging_work(sm_dc);
	sm_dc->ops->set_adc_mode(sm_dc->i2c, SM_DC_ADC_MODE_OFF);

	report_dc_state(sm_dc);

	return 0;
}
EXPORT_SYMBOL(sm_dc_stop_charging);

int sm_dc_set_target_vbat(struct sm_dc_info *sm_dc, u32 target_vbat)
{
	int ret = 0;
	int adc_vbat;

	pr_info("%s %s: [%dmV] to [%dmV]\n", sm_dc->name, __func__, sm_dc->target_vbat, target_vbat);

	sm_dc->target_vbat = target_vbat;
	if (sm_dc->state > SM_DC_CHECK_VBAT) {
		adc_vbat = sm_dc->ops->get_adc_value(sm_dc->i2c, SM_DC_ADC_VBAT);
		if (sm_dc->target_vbat > adc_vbat) {
			mutex_lock(&sm_dc->st_lock);
			sm_dc->req_update_vbat = 1;
			mutex_unlock(&sm_dc->st_lock);
			pr_info("%s %s: request VBAT update on DC work\n", sm_dc->name, __func__);
		} else {
			pr_err("%s %s: target_vbat(%dmV) less then adc_vbat(%dmV)\n", sm_dc->name, __func__,
					sm_dc->target_vbat, adc_vbat);
			ret = -EINVAL;
		}
	}

	return ret;
}
EXPORT_SYMBOL(sm_dc_set_target_vbat);

int sm_dc_set_target_ibus(struct sm_dc_info *sm_dc, u32 target_ibus)
{
	pr_info("%s %s: [%dmA] to [%dmA]\n", sm_dc->name, __func__, sm_dc->target_ibus, target_ibus);

	sm_dc->target_ibus = target_ibus;
	if (sm_dc->state > SM_DC_CHECK_VBAT) {
		mutex_lock(&sm_dc->st_lock);
		sm_dc->req_update_ibus = 1;
		mutex_unlock(&sm_dc->st_lock);
		pr_info("%s %s: request IBUS update on DC work\n", sm_dc->name, __func__);
	}

	return 0;

}
EXPORT_SYMBOL(sm_dc_set_target_ibus);

int sm_dc_set_target_ibat(struct sm_dc_info *sm_dc, u32 target_ibat)
{
	/* if need to it, we need to improve direct-charging module for cc_loop */

	return 0;
}
EXPORT_SYMBOL(sm_dc_set_target_ibat);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("SiliconMitus <sungdae.choi@siliconmitus.com>");
MODULE_DESCRIPTION("Direct-charger module for SM ICs");
