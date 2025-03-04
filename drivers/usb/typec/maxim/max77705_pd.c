/*
 * Copyrights (C) 2017 Samsung Electronics, Inc.
 * Copyrights (C) 2017 Maxim Integrated Products, Inc.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */

#include <linux/init.h>
#include <linux/module.h>
#include <linux/slab.h>
#include <linux/i2c.h>
#include <linux/delay.h>
#include <linux/interrupt.h>
#include <linux/mod_devicetable.h>
#include <linux/power_supply.h>
#include <linux/of.h>
#include <linux/irq.h>
#include <linux/gpio.h>
#include <linux/mfd/max77705-private.h>
#include <linux/platform_device.h>
#if IS_ENABLED(CONFIG_USB_NOTIFY_LAYER)
#include <linux/usb_notify.h>
#endif
#include <linux/usb/typec.h>
#include <linux/muic/common/muic.h>
#if IS_ENABLED(CONFIG_PDIC_NOTIFIER)
#include <linux/usb/typec/common/pdic_core.h>
#include <linux/usb/typec/common/pdic_notifier.h>
#endif
#include <linux/usb/typec/maxim/max77705-muic.h>
#include <linux/usb/typec/maxim/max77705_usbc.h>
#include <linux/usb/typec/maxim/max77705_alternate.h>
#include <linux/battery/sec_battery_common.h>
#include <linux/battery/sec_pd.h>

#if IS_ENABLED(CONFIG_SEC_MPARAM) || (IS_MODULE(CONFIG_SEC_PARAM) && defined(CONFIG_ARCH_EXYNOS))
#if defined(CONFIG_SEC_FACTORY)
extern int factory_mode;
#endif
#else
#if defined(CONFIG_SEC_FACTORY)
static int __read_mostly factory_mode;
module_param(factory_mode, int, 0444);
#endif
#endif

extern struct max77705_usbc_platform_data *g_usbc_data;
extern void max77705_set_CCForceError(struct max77705_usbc_platform_data *usbpd_data);
void max77705_set_enable_pps(bool bPPS_on, bool enable, int ppsVol, int ppsCur);

#if defined(CONFIG_SEC_FACTORY)
static int max77705_get_facmode(void) { return factory_mode; }
#endif

static void max77705_process_pd(struct max77705_usbc_platform_data *usbc_data)
{
	struct max77705_pd_data *pd_data = usbc_data->pd_data;

	if (pd_data->cc_sbu_short) {
		pd_data->pd_noti.sink_status.available_pdo_num = 1;
		pd_data->pd_noti.sink_status.power_list[1].max_current =
			pd_data->pd_noti.sink_status.power_list[1].max_current > 1800 ?
			1800 : pd_data->pd_noti.sink_status.power_list[1].max_current;
		pd_data->pd_noti.sink_status.has_apdo = false;
	}

	pr_info("%s : current_pdo_num(%d), available_pdo_num(%d), has_apdo(%d)\n", __func__,
		pd_data->pd_noti.sink_status.current_pdo_num, pd_data->pd_noti.sink_status.available_pdo_num,
		pd_data->pd_noti.sink_status.has_apdo);

	max77705_ccic_event_work(usbc_data, PDIC_NOTIFY_DEV_BATT,
		PDIC_NOTIFY_ID_POWER_STATUS, 1/*attach*/, 0, 0);
}
static void max77705_send_new_src_cap(struct max77705_usbc_platform_data *pusbpd,
	int auth, int d2d_type);
void max77705_vbus_turn_on_ctrl(struct max77705_usbc_platform_data *usbc_data, bool enable, bool swaped);
void max77705_response_req_pdo(struct max77705_usbc_platform_data *usbc_data,
	unsigned char *data)
{
	u8 sel_pdo = 0x00;
	int auth_type = usbc_data->pd_data->auth_type;
	int d2d_type = usbc_data->pd_data->d2d_type;

	if ((d2d_type == D2D_NONE) || (auth_type == AUTH_NONE))
		return;

	sel_pdo = ((data[1] >> 3) & 0x07);

	if (d2d_type == D2D_SRCSNK) {
		if (sel_pdo == 1) {
			/* 2.5w fpdo */
			usbc_data->pd_data->req_pdo_type = PDO_TYPE_FIXED;
		} else if ((sel_pdo >= 2) &&
			(auth_type == AUTH_HIGH_PWR)) {
			/* 15w vpdo */
			usbc_data->pd_data->req_pdo_type = PDO_TYPE_VARIABLE;
		}
		/* TEST */

		//if (auth_type == AUTH_HIGH_PWR)
		//	usbc_data->pd_data->req_pdo_type = PDO_TYPE_VARIABLE;
		//else
		//	usbc_data->pd_data->req_pdo_type = PDO_TYPE_FIXED;

		/* TEST */
	} else
		usbc_data->pd_data->req_pdo_type = PDO_TYPE_FIXED;

	pr_info("%s : snk pdo(%d, %d)\n", __func__, sel_pdo, usbc_data->pd_data->req_pdo_type);
	max77705_vbus_turn_on_ctrl(usbc_data, ON, false);
}

void max77705_check_req_pdo(struct max77705_usbc_platform_data *usbc_data)
{
	usbc_cmd_data value;

	init_usbc_cmd_data(&value);
	value.opcode = OPCODE_SNK_SELECTED_PDO;
	value.write_length = 0x0;
	value.read_length = 31;
	max77705_usbc_opcode_read(usbc_data, &value);

	pr_info("%s : OPCODE(0x%02x) W_LENGTH(%d) R_LENGTH(%d)\n",
		__func__, value.opcode, value.write_length, value.read_length);
}
void max77705_select_pdo(int num)
{
	struct max77705_pd_data *pd_data = g_usbc_data->pd_data;
	usbc_cmd_data value;
	u8 temp;

	if (pd_data->pd_noti.event == PDIC_NOTIFY_EVENT_DETACH) {
		pr_info("%s : PD TA already detached. Doesn't select pdo(%d)\n", __func__, num);
		return;
	}

	max77705_set_enable_pps(pd_data->bPPS_on, false, 0, 0);

	init_usbc_cmd_data(&value);
	pr_info("%s : NUM(%d)\n", __func__, num);

	temp = num;

	pd_data->pd_noti.sink_status.selected_pdo_num = num;

	if (pd_data->pd_noti.event != PDIC_NOTIFY_EVENT_PD_SINK)
		pd_data->pd_noti.event = PDIC_NOTIFY_EVENT_PD_SINK;

	if (pd_data->pd_noti.sink_status.current_pdo_num == pd_data->pd_noti.sink_status.selected_pdo_num) {
		max77705_process_pd(g_usbc_data);
	} else {
		g_usbc_data->pn_flag = false;
		value.opcode = OPCODE_SRCCAP_REQUEST;
		value.write_data[0] = temp;
		value.write_length = 1;
		value.read_length = 1;
		max77705_usbc_opcode_write(g_usbc_data, &value);
	}

	pr_info("%s : OPCODE(0x%02x) W_LENGTH(%d) R_LENGTH(%d) NUM(%d)\n",
		__func__, value.opcode, value.write_length, value.read_length, num);
}

void max77705_response_pdo_request(struct max77705_usbc_platform_data *usbc_data,
		unsigned char *data)
{
	u8 result = data[1];

	pr_info("%s: %s (0x%02X)\n", __func__, result ? "Error," : "Sent,", result);

	switch (result) {
		case 0x00:
			pr_info("%s: Sent PDO Request Message to Port Partner(0x%02X)\n", __func__, result);
			break;
		case 0xFE:
			pr_info("%s: Error, SinkTxNg(0x%02X)\n", __func__, result);
			break;
		case 0xFF:
			pr_info("%s: Error, Not in SNK Ready State(0x%02X)\n", __func__, result);
			break;
		default:
			break;
	}

	/* retry if the state of sink is not stable yet */
	if (result == 0xFE || result == 0xFF) {
		cancel_delayed_work(&usbc_data->pd_data->retry_work);
		queue_delayed_work(usbc_data->pd_data->wqueue, &usbc_data->pd_data->retry_work, 0);
	}
}

void max77705_set_enable_pps(bool bPPS_on, bool enable, int ppsVol, int ppsCur)
{
	usbc_cmd_data value;

	if (bPPS_on == enable)
		return;

	init_usbc_cmd_data(&value);
	value.opcode = OPCODE_SET_PPS;
	if (enable) {
		value.write_data[0] = 0x1; //PPS_ON On
		value.write_data[1] = (ppsVol / 20) & 0xFF; //Default Output Voltage (Low), 20mV
		value.write_data[2] = ((ppsVol / 20) >> 8) & 0xFF; //Default Output Voltage (High), 20mV
		value.write_data[3] = (ppsCur / 50) & 0x7F; //Default Operating Current, 50mA
		value.write_length = 4;
		value.read_length = 1;
		pr_info("%s : PPS_On (Vol:%dmV, Cur:%dmA)\n", __func__, ppsVol, ppsCur);
	} else {
		value.write_data[0] = 0x0; //PPS_ON Off
		value.write_length = 1;
		value.read_length = 1;
		pr_info("%s : PPS_Off\n", __func__);
	}
	max77705_usbc_opcode_write(g_usbc_data, &value);
}

void max77705_response_set_pps(struct max77705_usbc_platform_data *usbc_data,
		unsigned char *data)
{
	u8 result = data[1];

	if (result & 0x01)
		usbc_data->pd_data->bPPS_on = true;
	else
		usbc_data->pd_data->bPPS_on = false;

	pr_info("%s : PPS_%s (0x%02X)\n",
		__func__, usbc_data->pd_data->bPPS_on ? "On" : "Off", result);
}

void max77705_response_apdo_request(struct max77705_usbc_platform_data *usbc_data,
		unsigned char *data)
{
	u8 result = data[1];
	u8 status[5];
	u8 vbvolt;

	pr_info("%s: %s (0x%02X)\n", __func__, result ? "Error," : "Sent,", result);

	switch (result) {
	case 0x00:
		pr_info("%s: Sent APDO Request Message to Port Partner(0x%02X)\n", __func__, result);
		break;
	case 0x01:
		pr_info("%s: Error, Invalid APDO position(0x%02X)\n", __func__, result);
		break;
	case 0x02:
		pr_info("%s: Error, Invalid Output Voltage(0x%02X)\n", __func__, result);
		break;
	case 0x03:
		pr_info("%s: Error, Invalid Operating Current(0x%02X)\n", __func__, result);
		break;
	case 0x04:
		pr_info("%s: Error, PPS Function Off(0x%02X)\n", __func__, result);
		break;
	case 0x05:
		pr_info("%s: Error, Not in SNK Ready State(0x%02X)\n", __func__, result);
		break;
	case 0x06:
		pr_info("%s: Error, PD2.0 Contract(0x%02X)\n", __func__, result);
		break;
	case 0x07:
		pr_info("%s: Error, SinkTxNg(0x%02X)\n", __func__, result);
		break;
	default:
		break;
	}

	max77705_bulk_read(usbc_data->muic, MAX77705_USBC_REG_USBC_STATUS1, 5, status);
	vbvolt = (status[2] & BC_STATUS_VBUSDET_MASK) >> BC_STATUS_VBUSDET_SHIFT;
	if (vbvolt != 0x01)
		pr_info("%s: Error, VBUS isn't above 5V(0x%02X)\n", __func__, vbvolt);

	/* retry if the state of sink is not stable yet */
	if ((result == 0x05 || result == 0x07) && vbvolt == 0x1) {
		cancel_delayed_work(&usbc_data->pd_data->retry_work);
		queue_delayed_work(usbc_data->pd_data->wqueue, &usbc_data->pd_data->retry_work, 0);
	}
}

int max77705_select_pps(int num, int ppsVol, int ppsCur)
{
	struct max77705_pd_data *pd_data = g_usbc_data->pd_data;
	usbc_cmd_data value;

	/* [dchg] TODO: check more below option */
	if (num > pd_data->pd_noti.sink_status.available_pdo_num) {
		pr_info("%s: request pdo num(%d) is higher taht available pdo.\n", __func__, num);
		return -EINVAL;
	}

	if (!(pd_data->pd_noti.sink_status.power_list[num].pdo_type == APDO_TYPE)) {
		pr_info("%s: request pdo num(%d) is not apdo.\n", __func__, num);
		return -EINVAL;
	} else
		pd_data->pd_noti.sink_status.selected_pdo_num = num;

	if (ppsVol > pd_data->pd_noti.sink_status.power_list[num].max_voltage) {
		pr_info("%s: ppsVol is over(%d, max:%d)\n",
			__func__, ppsVol, pd_data->pd_noti.sink_status.power_list[num].max_voltage);
		ppsVol = pd_data->pd_noti.sink_status.power_list[num].max_voltage;
	} else if (ppsVol < pd_data->pd_noti.sink_status.power_list[num].min_voltage) {
		pr_info("%s: ppsVol is under(%d, min:%d)\n",
			__func__, ppsVol, pd_data->pd_noti.sink_status.power_list[num].min_voltage);
		ppsVol = pd_data->pd_noti.sink_status.power_list[num].min_voltage;
	}

	if (ppsCur > pd_data->pd_noti.sink_status.power_list[num].max_current) {
		pr_info("%s: ppsCur is over(%d, max:%d)\n",
			__func__, ppsCur, pd_data->pd_noti.sink_status.power_list[num].max_current);
		ppsCur = pd_data->pd_noti.sink_status.power_list[num].max_current;
	} else if (ppsCur < 0) {
		pr_info("%s: ppsCur is under(%d, 0)\n",
			__func__, ppsCur);
		ppsCur = 0;
	}

	pd_data->pd_noti.sink_status.pps_voltage = ppsVol;
	pd_data->pd_noti.sink_status.pps_current = ppsCur;

	pr_info(" %s : PPS PDO(%d), voltage(%d), current(%d) is selected to change\n",
		__func__, pd_data->pd_noti.sink_status.selected_pdo_num, ppsVol, ppsCur);

	max77705_set_enable_pps(pd_data->bPPS_on, true, 5000, 1000); /* request as default 5V when enable first */

	init_usbc_cmd_data(&value);

	g_usbc_data->pn_flag = false;
	value.opcode = OPCODE_APDO_SRCCAP_REQUEST;
	value.write_data[0] = (num & 0xFF); /* APDO Position */
	value.write_data[1] = (ppsVol / 20) & 0xFF; /* Output Voltage(Low) */
	value.write_data[2] = ((ppsVol / 20) >> 8) & 0xFF; /* Output Voltage(High) */
	value.write_data[3] = (ppsCur / 50) & 0x7F; /* Operating Current */
	value.write_length = 4;
	value.read_length = 1; /* Result */
	max77705_usbc_opcode_write(g_usbc_data, &value);

/* [dchg] TODO: add return value */
	return 0;
}

void max77705_pd_retry_work(struct work_struct *work)
{
	struct max77705_pd_data *pd_data = g_usbc_data->pd_data;
	usbc_cmd_data value;
	u8 num;

	if (pd_data->pd_noti.event == PDIC_NOTIFY_EVENT_DETACH)
		return;

	init_usbc_cmd_data(&value);
	num = pd_data->pd_noti.sink_status.selected_pdo_num;
	pr_info("%s : latest selected_pdo_num(%d)\n", __func__, num);
	g_usbc_data->pn_flag = false;

	if (pd_data->pd_noti.sink_status.power_list[num].pdo_type == APDO_TYPE) {
		value.opcode = OPCODE_APDO_SRCCAP_REQUEST;
		value.write_data[0] = (num & 0xFF); /* APDO Position */
		value.write_data[1] = (pd_data->pd_noti.sink_status.pps_voltage / 20) & 0xFF; /* Output Voltage(Low) */
		value.write_data[2] = ((pd_data->pd_noti.sink_status.pps_voltage / 20) >> 8) & 0xFF; /* Output Voltage(High) */
		value.write_data[3] = (pd_data->pd_noti.sink_status.pps_current / 50) & 0x7F; /* Operating Current */
		value.write_length = 4;
		value.read_length = 1; /* Result */
		max77705_usbc_opcode_write(g_usbc_data, &value);
	} else {
		value.opcode = OPCODE_SRCCAP_REQUEST;
		value.write_data[0] = num;
		value.write_length = 1;
		value.read_length = 1;
		max77705_usbc_opcode_write(g_usbc_data, &value);
	}

	pr_info("%s : OPCODE(0x%02x) W_LENGTH(%d) R_LENGTH(%d) NUM(%d)\n",
			__func__, value.opcode, value.write_length, value.read_length, num);
}

void max77705_usbc_icurr(u8 curr)
{
	usbc_cmd_data value;

	init_usbc_cmd_data(&value);
	value.opcode = OPCODE_CHGIN_ILIM_W;
	value.write_data[0] = curr;
	value.write_length = 1;
	value.read_length = 0;
	max77705_usbc_opcode_write(g_usbc_data, &value);

	pr_info("%s : OPCODE(0x%02x) W_LENGTH(%d) R_LENGTH(%d) USBC_ILIM(0x%x)\n",
		__func__, value.opcode, value.write_length, value.read_length, curr);

}
EXPORT_SYMBOL(max77705_usbc_icurr);

void max77705_set_gpio5_control(int direction, int output)
{
	usbc_cmd_data value;
	u8 op_data1, op_data2;

	if (direction)
		op_data1 = 0xFF; // OUTPUT
	else
		op_data1 = 0x00; // INPUT

	if (output)
		op_data2 = 0xFF; // HIGH
	else
		op_data2 = 0x00; // LOW

	init_usbc_cmd_data(&value);
	value.opcode = OPCODE_SAMSUNG_GPIO5_CONTROL;
	value.write_data[0] = op_data1;
	value.write_data[1] = op_data2;
	value.write_length = 2;
	value.read_length = 0;
	max77705_usbc_opcode_write(g_usbc_data, &value);

	pr_info("%s : OPCODE(0x%02x) W_LENGTH(%d) R_LENGTH(%d) gpio5(0x%x, 0x%x)\n",
		__func__, value.opcode, value.write_length, value.read_length, op_data1, op_data2);
}

void max77705_set_fw_noautoibus(int enable)
{
	usbc_cmd_data value;
	u8 op_data = 0x00;

	switch (enable) {
	case MAX77705_AUTOIBUS_FW_AT_OFF:
		op_data = 0x03; /* usbc fw off & auto off(manual on) */
		break;
	case MAX77705_AUTOIBUS_FW_OFF:
		op_data = 0x02; /* usbc fw off & auto on(manual off) */
		break;
	case MAX77705_AUTOIBUS_AT_OFF:
		op_data = 0x01; /* usbc fw on & auto off(manual on) */
		break;
	case MAX77705_AUTOIBUS_ON:
	default:
		op_data = 0x00; /* usbc fw on & auto on(manual off) */
		break;
	}

#if defined(CONFIG_SEC_FACTORY)
	if (max77705_get_facmode()) {
		pr_info("%s: Factory Mode set AUTOIBUS_FW_AT_OFF\n", __func__);
		op_data = 0x03; /* usbc fw off & auto off(manual on) */
	}
#endif

	init_usbc_cmd_data(&value);
	value.opcode = OPCODE_SAMSUNG_FW_AUTOIBUS;
	value.write_data[0] = op_data;
	value.write_length = 1;
	value.read_length = 0;
	max77705_usbc_opcode_write(g_usbc_data, &value);

	pr_info("%s : OPCODE(0x%02x) W_LENGTH(%d) R_LENGTH(%d) AUTOIBUS(0x%x)\n",
		__func__, value.opcode, value.write_length, value.read_length, op_data);
}
EXPORT_SYMBOL(max77705_set_fw_noautoibus);

#if defined(CONFIG_SUPPORT_SHIP_MODE)
void max77705_set_fw_ship_mode(int enable)
{
	struct max77705_usbc_platform_data *pusbpd = g_usbc_data;
	usbc_cmd_data value;

	if (!enable) {
		pr_info("%s: not support disable. current ship_mode_en(%d)\n",
			__func__, pusbpd->ship_mode_en);
		return;
	}

	init_usbc_cmd_data(&value);

	value.opcode = OPCODE_SAMSUNG_SHIPMODE_EN;
	value.write_data[0] = 0x38;
	value.write_length = 1;
	value.read_length = 2;
	max77705_usbc_opcode_write(pusbpd, &value);

	pr_info("%s : OPCODE(0x%02x) W_LENGTH(%d) R_LENGTH(%d) ship_mone(0x%x)\n",
		__func__, value.opcode, value.write_length, value.read_length, value.write_data[0]);
}
EXPORT_SYMBOL(max77705_set_fw_ship_mode);

int max77705_get_fw_ship_mode(void)
{
	if (!g_usbc_data)
		return 0;

	return g_usbc_data->ship_mode_en;
}
EXPORT_SYMBOL(max77705_get_fw_ship_mode);
#endif

static void max77705_set_snkcap(struct max77705_usbc_platform_data *usbc_data)
{
	struct device_node *np = NULL;
	u8 *snkcap_data;
	int len = 0, ret = 0;
	usbc_cmd_data value;
	int i;
	char *str = NULL;

	np = of_find_compatible_node(NULL, NULL, "maxim,max77705");
	if (!np) {
		pr_info("%s : np is NULL\n", __func__);
		return;
	}

	if (!of_get_property(np, "max77705,snkcap_data", &len)) {
		pr_info("%s : max77705,snkcap_data is Empty !!\n", __func__);
		return;
	}

	len = len / sizeof(u8);
	snkcap_data = kzalloc(sizeof(*snkcap_data) * len, GFP_KERNEL);
	if (!snkcap_data) {
		pr_err("%s : Failed to allocate memory (snkcap_data)\n", __func__);
		return;
	}

	ret = of_property_read_u8_array(np, "max77705,snkcap_data",
		snkcap_data, len);
	if (ret) {
		pr_info("%s : max77705,snkcap_data is Empty\n", __func__);
		goto err_free_snkcap_data;
	}

	init_usbc_cmd_data(&value);

	if (len)
		memcpy(value.write_data, snkcap_data, len);

	str = kzalloc(sizeof(char) * 1024, GFP_KERNEL);
	if (str) {
		for (i = 0; i < len; i++)
			sprintf(str + strlen(str), "0x%02x ", value.write_data[i]);
		pr_info("%s: SNK_CAP : %s\n", __func__, str);
	}

	value.opcode = OPCODE_SET_SNKCAP;
	value.write_length = len;
	value.read_length = 0;
	max77705_usbc_opcode_write(usbc_data, &value);

	kfree(str);
err_free_snkcap_data:
	kfree(snkcap_data);
}

bool max77705_check_boost_enable(int auth_t, int req_pdo, int d2d_t)
{
	if ((auth_t == AUTH_NONE) || (d2d_t != D2D_SRCSNK))
		return false;

	if (req_pdo == PDO_TYPE_VARIABLE)
		return true;

	return false;
}

bool max77705_check_boost_off(int auth_t, int req_pdo, int d2d_t)
{
	if ((auth_t == AUTH_NONE) || (d2d_t != D2D_SRCSNK))
		return false;

	if (req_pdo == PDO_TYPE_FIXED)
		return true;

	return false;
}

bool max77705_check_src_otg_type(bool enable, int auth_t, int req_pdo, int d2d_t)
{
	if ((auth_t == AUTH_NONE) || (d2d_t != D2D_SRCSNK))
		return enable;

	if (req_pdo == PDO_TYPE_VARIABLE)
		return false;

	return enable;
}

void max77705_vbus_turn_on_ctrl(struct max77705_usbc_platform_data *usbc_data, bool enable, bool swaped)
{
	struct power_supply *psy_otg;
	union power_supply_propval val;
	int on = !!enable;
	int ret = 0;
	int count = 5;
	int auth_type = usbc_data->pd_data->auth_type;
	int req_pdo_type = usbc_data->pd_data->req_pdo_type;
	int d2d_type = usbc_data->pd_data->d2d_type;
#if defined(CONFIG_USB_HOST_NOTIFY)
	struct otg_notify *o_notify = get_otg_notify();
	bool must_block_host = 0;
	static int reserve_booster = 0;

#ifdef CONFIG_DISABLE_LOCKSCREEN_USB_RESTRICTION
	if (o_notify)
		must_block_host = is_blocked(o_notify, NOTIFY_BLOCK_TYPE_HOST);
#endif

	pr_info("%s : enable=%d, auto_vbus_en=%d, must_block_host=%d, swaped=%d\n",
		__func__, enable, usbc_data->auto_vbus_en, must_block_host, swaped);
	// turn on
	if (enable) {
		// auto-mode
		if (usbc_data->auto_vbus_en) {
			// mpsm
			if (must_block_host) {
				if (swaped) {
					// turn off vbus because of swaped and blocked host
					enable = false;
					pr_info("%s : turn off vbus because of blocked host\n",
						__func__);
				} else {
					enable = false;
					pr_info("%s : turn off vbus because of blocked host\n",
						__func__);
				}
			} else {
				// don't turn on because of auto-mode
				return;
			}
		// non auto-mode
		} else {
			if (must_block_host) {
				if (swaped) {
					enable = false;
					pr_info("%s : turn off vbus because of blocked host\n",
						__func__);
				} else {
					enable = false;
					pr_info("%s : turn off vbus because of blocked host\n",
						__func__);
				}
			}
		}
	// turn off
	} else {
		// don't turn off because of auto-mode or blocked (already off)
		if (usbc_data->auto_vbus_en || must_block_host)
			return;
	}
#endif

	pr_info("%s : enable=%d\n", __func__, enable);

#if defined(CONFIG_USB_HOST_NOTIFY)
	if (o_notify && o_notify->booting_delay_sec && enable) {
		pr_info("%s %d, is booting_delay_sec. skip to control booster\n",
			__func__, __LINE__);
		reserve_booster = 1;
		send_otg_notify(o_notify, NOTIFY_EVENT_RESERVE_BOOSTER, 1);
		return;
	}
	if (!enable) {
		if (reserve_booster) {
			reserve_booster = 0;
			send_otg_notify(o_notify, NOTIFY_EVENT_RESERVE_BOOSTER, 0);
		}
	}
#endif

	while (count) {
		psy_otg = power_supply_get_by_name("otg");
		if (psy_otg) {
			if (max77705_check_boost_off(auth_type, req_pdo_type, d2d_type)) {
				val.intval = 0;
				 /* disable dc reverse boost before otg on */
				psy_do_property("battery", set,
					POWER_SUPPLY_EXT_PROP_CHARGE_OTG_CONTROL, val);
			}

			val.intval = max77705_check_src_otg_type(enable, auth_type, req_pdo_type, d2d_type);
#if defined(CONFIG_USE_SECOND_MUIC)
			muic_hv_charger_disable(enable);
#endif
			ret = psy_otg->desc->set_property(psy_otg, POWER_SUPPLY_PROP_ONLINE, &val);
			if (ret == -ENODEV) {
				pr_err("%s: fail to set power_suppy ONLINE property %d) retry (%d)\n",__func__, ret, count);
				count--;
			} else {
				if (ret) {
					pr_err("%s: fail to set power_suppy ONLINE property(%d) \n",__func__, ret);
				} else {
					pr_info("otg accessory power = %d\n", on);
				}
				if (max77705_check_boost_enable(auth_type, req_pdo_type, d2d_type)) {
					val.intval = enable; /* set dc reverse boost after otg off */
					psy_do_property("battery", set,
						POWER_SUPPLY_EXT_PROP_CHARGE_OTG_CONTROL, val);
				}
				break;
			}
		} else {
			pr_err("%s: fail to get psy battery\n", __func__);
			count--;
			msleep(200);
		}
	}
}

void max77705_pdo_list(struct max77705_usbc_platform_data *usbc_data, unsigned char *data)
{
	struct max77705_pd_data *pd_data = usbc_data->pd_data;
	u8 temp = 0x00;
	int i;
	bool do_power_nego = false;
	pd_data->pd_noti.event = PDIC_NOTIFY_EVENT_PD_SINK;

	temp = (data[1] >> 5);

	if (temp > MAX_PDO_NUM) {
		pr_info("%s : update available_pdo_num[%d -> %d]",
			__func__, temp, MAX_PDO_NUM);
		temp = MAX_PDO_NUM;
	}

	pd_data->pd_noti.sink_status.available_pdo_num = temp;
	pr_info("%s : Temp[0x%02x] Data[0x%02x] available_pdo_num[%d]\n",
		__func__, temp, data[1], pd_data->pd_noti.sink_status.available_pdo_num);

	for (i = 0; i < temp; i++) {
		u32 pdo_temp;
		int max_current = 0, max_voltage = 0;

		pdo_temp = (data[2 + (i * 4)]
			| (data[3 + (i * 4)] << 8)
			| (data[4 + (i * 4)] << 16)
			| (data[5 + (i * 4)] << 24));

		pr_info("%s : PDO[%d] = 0x%x\n", __func__, i, pdo_temp);

		max_current = (0x3FF & pdo_temp);
		max_voltage = (0x3FF & (pdo_temp >> 10));

		if (!(do_power_nego) &&
			(pd_data->pd_noti.sink_status.power_list[i + 1].max_current != max_current * UNIT_FOR_CURRENT ||
			pd_data->pd_noti.sink_status.power_list[i + 1].max_voltage != max_voltage * UNIT_FOR_VOLTAGE))
			do_power_nego = true;

		pd_data->pd_noti.sink_status.power_list[i + 1].max_current = max_current * UNIT_FOR_CURRENT;
		pd_data->pd_noti.sink_status.power_list[i + 1].max_voltage = max_voltage * UNIT_FOR_VOLTAGE;

		pr_info("%s : PDO_Num[%d] MAX_CURR(%d) MAX_VOLT(%d), AVAILABLE_PDO_Num(%d)\n", __func__,
				i, pd_data->pd_noti.sink_status.power_list[i + 1].max_current,
				pd_data->pd_noti.sink_status.power_list[i + 1].max_voltage,
				pd_data->pd_noti.sink_status.available_pdo_num);
	}

	if (usbc_data->pd_data->pdo_list && do_power_nego) {
		pr_info("%s : PDO list is changed, so power negotiation is need\n",
			__func__);
		pd_data->pd_noti.sink_status.selected_pdo_num = 0;
		pd_data->pd_noti.event = PDIC_NOTIFY_EVENT_PD_SINK_CAP;
	}

	if (pd_data->pd_noti.sink_status.current_pdo_num != pd_data->pd_noti.sink_status.selected_pdo_num) {
		if (pd_data->pd_noti.sink_status.selected_pdo_num == 0)
			pr_info("%s : PDO is not selected yet by default\n", __func__);
	}

	usbc_data->pd_data->pdo_list = true;
	max77705_process_pd(usbc_data);
}

bool is_accept_pdo(POWER_LIST* pPower_list)
{
	int pdo_type = pPower_list->pdo_type;
	int max_volt = pPower_list->max_voltage;
	int min_volt = pPower_list->min_voltage;

	if (max_volt < min_volt)
		return false;

	if ((pdo_type == FPDO_TYPE) || (pdo_type == VPDO_TYPE)) {
		if ((max_volt < DEFAULT_VOLTAGE) || (max_volt > AVAILABLE_VOLTAGE))
			return false;
	}

	return true;
}

void max77705_abnormal_pdo_work(struct work_struct *work)
{
	struct max77705_usbc_platform_data *usbc_data = g_usbc_data;

	pr_info("%s\n", __func__);
	//executes the ErroryRecovery.
	max77705_set_CCForceError(usbc_data);
}

void max77705_send_identity_work(struct work_struct *work)
{
	struct max77705_usbc_platform_data *usbc_data = g_usbc_data;

	pr_info("%s\n", __func__);
	max77705_vdm_process_set_identity_req(usbc_data);
}

void max77705_current_pdo(struct max77705_usbc_platform_data *usbc_data, unsigned char *data)
{
	struct max77705_pd_data *pd_data = usbc_data->pd_data;
	u8 sel_pdo_pos = 0x00, num_of_pdo = 0x00;
	int i, available_pdo_num = 0;
	bool do_power_nego = false, is_abnormal_pdo = true;
	U_SEC_PDO_OBJECT pdo_obj;
	POWER_LIST* pPower_list;
	POWER_LIST prev_power_list;

	if (!pd_data->pd_noti.sink_status.available_pdo_num)
		do_power_nego = true;

	sel_pdo_pos = ((data[1] >> 3) & 0x07);
	pd_data->pd_noti.sink_status.current_pdo_num = sel_pdo_pos;

	num_of_pdo = (data[1] & 0x07);
	if (num_of_pdo > MAX_PDO_NUM) {
		pr_info("%s : update available_pdo_num[%d -> %d]",
			__func__, num_of_pdo, MAX_PDO_NUM);
		num_of_pdo = MAX_PDO_NUM;
	}

	pd_data->pd_noti.sink_status.has_apdo = false;

	for (i = 0; i < num_of_pdo; ++i) {
		pPower_list = &pd_data->pd_noti.sink_status.power_list[available_pdo_num + 1];

		pdo_obj.data = (data[2 + (i * 4)]
			| (data[3 + (i * 4)] << 8)
			| (data[4 + (i * 4)] << 16)
			| (data[5 + (i * 4)] << 24));

		if (!do_power_nego)
			prev_power_list = pd_data->pd_noti.sink_status.power_list[available_pdo_num + 1];

		switch (pdo_obj.BITS_supply.type) {
		case PDO_TYPE_FIXED:
			pPower_list->apdo = false;
			pPower_list->pdo_type = FPDO_TYPE;
			pPower_list->max_voltage = pdo_obj.BITS_pdo_fixed.voltage * UNIT_FOR_VOLTAGE;
			pPower_list->min_voltage = 0;
			pPower_list->max_current = pdo_obj.BITS_pdo_fixed.max_current * UNIT_FOR_CURRENT;
			pPower_list->comm_capable = pdo_obj.BITS_pdo_fixed.usb_communications_capable;
			pPower_list->suspend = pdo_obj.BITS_pdo_fixed.usb_suspend_supported;
			available_pdo_num++;
 			break;
		case PDO_TYPE_APDO:
			pd_data->pd_noti.sink_status.has_apdo = true;
			pPower_list->apdo = true;
			pPower_list->pdo_type = APDO_TYPE;
			pPower_list->max_voltage = pdo_obj.BITS_pdo_programmable.max_voltage * UNIT_FOR_APDO_VOLTAGE;
			pPower_list->min_voltage = pdo_obj.BITS_pdo_programmable.min_voltage * UNIT_FOR_APDO_VOLTAGE;
			pPower_list->max_current = pdo_obj.BITS_pdo_programmable.max_current * UNIT_FOR_APDO_CURRENT;
			available_pdo_num++;
			break;
		case PDO_TYPE_VARIABLE:
			pPower_list->apdo = false;
			pPower_list->pdo_type = VPDO_TYPE;
			pPower_list->max_voltage = pdo_obj.BITS_pdo_variable.max_voltage * UNIT_FOR_VOLTAGE;
			pPower_list->min_voltage = pdo_obj.BITS_pdo_variable.min_voltage * UNIT_FOR_VOLTAGE;
			pPower_list->max_current = pdo_obj.BITS_pdo_variable.max_current * UNIT_FOR_CURRENT;
			available_pdo_num++;
			break;
		default:
			break;
		}

		if (!(do_power_nego) &&
			(pPower_list->max_current != prev_power_list.max_current ||
			pPower_list->max_voltage != prev_power_list.max_voltage ||
			pPower_list->min_voltage != prev_power_list.min_voltage))
			do_power_nego = true;
	}


	if (!do_power_nego && (pd_data->pd_noti.sink_status.available_pdo_num != available_pdo_num))
		do_power_nego = true;

	pd_data->pd_noti.sink_status.available_pdo_num = available_pdo_num;
	pr_info("%s : current_pdo_num(%d), available_pdo_num(%d/%d), comm(%d), suspend(%d)\n", __func__,
		pd_data->pd_noti.sink_status.current_pdo_num,
		pd_data->pd_noti.sink_status.available_pdo_num, num_of_pdo,
		pd_data->pd_noti.sink_status.power_list[sel_pdo_pos].comm_capable,
		pd_data->pd_noti.sink_status.power_list[sel_pdo_pos].suspend);

	pd_data->pd_noti.event = PDIC_NOTIFY_EVENT_PD_SINK;

	if (usbc_data->pd_data->pdo_list && do_power_nego) {
		pr_info("%s : PDO list is changed, so power negotiation is need\n", __func__);
		pd_data->pd_noti.sink_status.selected_pdo_num = 0;
		pd_data->pd_noti.event = PDIC_NOTIFY_EVENT_PD_SINK_CAP;
	}

	if (pd_data->pd_noti.sink_status.current_pdo_num != pd_data->pd_noti.sink_status.selected_pdo_num) {
		if (pd_data->pd_noti.sink_status.selected_pdo_num == 0)
			pr_info("%s : PDO is not selected yet by default\n", __func__);
	}

	if (do_power_nego || pd_data->pd_noti.sink_status.selected_pdo_num == 0) {
		for (i = 0; i < num_of_pdo; ++i) {
			pdo_obj.data = (data[2 + (i * 4)]
				| (data[3 + (i * 4)] << 8)
				| (data[4 + (i * 4)] << 16)
				| (data[5 + (i * 4)] << 24));
			pr_info("%s : PDO[%d] = 0x%08X\n", __func__, i + 1, pdo_obj.data);
		}

		for (i = 0; i < pd_data->pd_noti.sink_status.available_pdo_num; ++i) {
			pPower_list = &pd_data->pd_noti.sink_status.power_list[i + 1];
			pPower_list->accept = is_accept_pdo(pPower_list);

			pr_info("%s : PDO[%d,%s,%s] max_vol(%dmV),min_vol(%dmV),max_cur(%dmA)\n",
				__func__, i + 1,
				pPower_list->pdo_type ? ((pPower_list->pdo_type == APDO_TYPE) ? "APDO" : "VPDO") : "FIXED",
				pPower_list->accept ? "O" : "X",
				pPower_list->max_voltage, pPower_list->min_voltage, pPower_list->max_current);

			if (pPower_list->accept)
				is_abnormal_pdo = false;
		}
	} else {
		is_abnormal_pdo = false;
	}

	usbc_data->pd_data->pdo_list = true;
	if (is_abnormal_pdo) {
		if (!delayed_work_pending(&usbc_data->pd_data->abnormal_pdo_work)) {
			union power_supply_propval val = {0,};

			for (i = 0; i < num_of_pdo; ++i) {
				pdo_obj.data = (data[2 + (i * 4)]
					| (data[3 + (i * 4)] << 8)
					| (data[4 + (i * 4)] << 16)
					| (data[5 + (i * 4)] << 24));
				val.intval = (int)pdo_obj.data;
				psy_do_property("battery", set,
					POWER_SUPPLY_EXT_PROP_ABNORMAL_SRCCAP, val);
			}
			queue_delayed_work(usbc_data->pd_data->wqueue,
				&usbc_data->pd_data->abnormal_pdo_work, 0);
		}
	} else {
		max77705_process_pd(usbc_data);
	}
}

void max77705_detach_pd(struct max77705_usbc_platform_data *usbc_data)
{
	struct max77705_pd_data *pd_data = usbc_data->pd_data;

	pr_info("%s : Detach PD CHARGER\n", __func__);

	if (pd_data->pd_noti.event != PDIC_NOTIFY_EVENT_DETACH) {
		cancel_delayed_work(&usbc_data->pd_data->retry_work);
		cancel_delayed_work(&usbc_data->pd_data->abnormal_pdo_work);
		cancel_delayed_work(&usbc_data->pd_data->send_identity_work);
		if (pd_data->pd_noti.sink_status.available_pdo_num)
			memset(pd_data->pd_noti.sink_status.power_list, 0, (sizeof(POWER_LIST) * (MAX_PDO_NUM + 1)));
		pd_data->pd_noti.sink_status.rp_currentlvl = RP_CURRENT_LEVEL_NONE;
		pd_data->pd_noti.sink_status.selected_pdo_num = 0;
		pd_data->pd_noti.sink_status.available_pdo_num = 0;
		pd_data->pd_noti.sink_status.current_pdo_num = 0;
		pd_data->pd_noti.sink_status.pps_voltage = 0;
		pd_data->pd_noti.sink_status.pps_current = 0;
 		pd_data->pd_noti.sink_status.has_apdo = false;
		max77705_set_enable_pps(pd_data->bPPS_on, false, 0, 0);
		pd_data->pd_noti.event = PDIC_NOTIFY_EVENT_DETACH;
		usbc_data->pd_data->psrdy_received = false;
		usbc_data->pd_data->pdo_list = false;
		usbc_data->pd_data->cc_sbu_short = false;
		pd_data->auth_type = AUTH_NONE;
		max77705_ccic_event_work(usbc_data, PDIC_NOTIFY_DEV_BATT,
			PDIC_NOTIFY_ID_POWER_STATUS, 0/*attach*/, 0, 0);
	}
}

static void max77705_notify_prswap(struct max77705_usbc_platform_data *usbc_data, u8 pd_msg)
{
	struct max77705_pd_data *pd_data = usbc_data->pd_data;

	pr_info("%s : PR SWAP pd_msg [%x]\n", __func__, pd_msg);

	switch(pd_msg) {
	case PRSWAP_SNKTOSWAP:
		pd_data->pd_noti.event = PDIC_NOTIFY_EVENT_PD_PRSWAP_SNKTOSRC;
		pd_data->pd_noti.sink_status.selected_pdo_num = 0;
		pd_data->pd_noti.sink_status.available_pdo_num = 0;
		pd_data->pd_noti.sink_status.current_pdo_num = 0;
		usbc_data->pd_data->psrdy_received = false;
		usbc_data->pd_data->pdo_list = false;
		usbc_data->pd_data->cc_sbu_short = false;
		max77705_ccic_event_work(usbc_data, PDIC_NOTIFY_DEV_BATT,
			PDIC_NOTIFY_ID_POWER_STATUS, 0/*attach*/, 0, 0);
		break;
	case PRSWAP_SRCTOSWAP:
		pd_data->pd_noti.event = PDIC_NOTIFY_EVENT_PD_PRSWAP_SRCTOSNK;
		pd_data->pd_noti.sink_status.selected_pdo_num = 0;
		pd_data->pd_noti.sink_status.available_pdo_num = 0;
		pd_data->pd_noti.sink_status.current_pdo_num = 0;
		usbc_data->pd_data->psrdy_received = false;
		usbc_data->pd_data->pdo_list = false;
		usbc_data->pd_data->cc_sbu_short = false;
		max77705_ccic_event_work(usbc_data, PDIC_NOTIFY_DEV_BATT,
			PDIC_NOTIFY_ID_POWER_STATUS, 0/*attach*/, 0, 0);
		break;
	default:
		break;
	}
}

void max77705_check_pdo(struct max77705_usbc_platform_data *usbc_data)
{
	usbc_cmd_data value;

	init_usbc_cmd_data(&value);
	value.opcode = OPCODE_CURRENT_SRCCAP;
	value.write_length = 0x0;
	value.read_length = 31;
	max77705_usbc_opcode_read(usbc_data, &value);

	pr_info("%s : OPCODE(0x%02x) W_LENGTH(%d) R_LENGTH(%d)\n",
		__func__, value.opcode, value.write_length, value.read_length);
}

void max77705_notify_rp_current_level(struct max77705_usbc_platform_data *usbc_data)
{
	struct max77705_pd_data *pd_data = usbc_data->pd_data;
	unsigned int rp_currentlvl;

	switch (usbc_data->cc_data->ccistat) {
	case CCI_500mA:
		rp_currentlvl = RP_CURRENT_LEVEL_DEFAULT;
		break;
	case CCI_1_5A:
		rp_currentlvl = RP_CURRENT_LEVEL2;
		break;
	case CCI_3_0A:
		rp_currentlvl = RP_CURRENT_LEVEL3;
		break;
	case CCI_SHORT:
		rp_currentlvl = RP_CURRENT_ABNORMAL;
		break;
	default:
		rp_currentlvl = RP_CURRENT_LEVEL_NONE;
		break;
	}

	if (usbc_data->plug_attach_done && !usbc_data->pd_data->psrdy_received &&
		usbc_data->cc_data->current_pr == SNK &&
		usbc_data->pd_state == max77705_State_PE_SNK_Wait_for_Capabilities &&
		pd_data->pdsmg != SRC_CAP_RECEIVED && // fw changes for advertise Rp22k for CtoC
		rp_currentlvl != pd_data->pd_noti.sink_status.rp_currentlvl &&
		rp_currentlvl >= RP_CURRENT_LEVEL_DEFAULT) {
		pd_data->pd_noti.sink_status.rp_currentlvl = rp_currentlvl;
		pd_data->pd_noti.event = PDIC_NOTIFY_EVENT_PDIC_ATTACH;
		pr_info("%s : rp_currentlvl(%d)\n", __func__, pd_data->pd_noti.sink_status.rp_currentlvl);
		max77705_ccic_event_work(usbc_data, PDIC_NOTIFY_DEV_BATT,
			PDIC_NOTIFY_ID_POWER_STATUS, 0/*attach*/, 0, 0);
	}
}

static int max77705_get_chg_info(struct max77705_usbc_platform_data *usbc_data)
{
	usbc_cmd_data value;
	POWER_LIST *pPower_list;

	pPower_list = &usbc_data->pd_data->pd_noti.sink_status.power_list[1];

	if ((usbc_data->pd_data->sent_chg_info) ||
		(pPower_list->max_current < 2000))
		return 0;

	if (usbc_data->pd_data->sent_chg_info)
		return 0;

	init_usbc_cmd_data(&value);
	value.opcode = OPCODE_SEND_GET_REQUEST;
	value.write_data[0] = OPCODE_GET_SRC_CAP_EXT;
	value.write_data[1] = 0; /*  */
	value.write_data[2] = 0; /*  */
	value.write_length = 3;
	value.read_length = 1; /* Result */
	max77705_usbc_opcode_write(g_usbc_data, &value);

	pr_info("%s : OPCODE(0x%02x) W_LENGTH(%d) R_LENGTH(%d)\n",
		__func__, value.opcode, value.write_length, value.read_length);

	usbc_data->pd_data->sent_chg_info = true;
	return 0;
}

static void clear_chg_info(struct max77705_usbc_platform_data *usbc_data)
{
	SEC_PD_SINK_STATUS *snk_sts = &usbc_data->pd_data->pd_noti.sink_status;

	usbc_data->pd_data->sent_chg_info = false;
	snk_sts->pid = 0;
	snk_sts->vid = 0;
	snk_sts->xid = 0;
}

static void max77705_pd_check_pdmsg(struct max77705_usbc_platform_data *usbc_data, u8 pd_msg)
{
	struct power_supply *psy_charger;
	union power_supply_propval val;
	usbc_cmd_data value;
	/*int dr_swap, pr_swap, vcon_swap = 0; u8 value[2], rc = 0;*/
	MAX77705_VDM_MSG_IRQ_STATUS_Type VDM_MSG_IRQ_State;
#ifdef CONFIG_USB_NOTIFY_PROC_LOG
	int event;
#endif
#if IS_ENABLED(CONFIG_USB_NOTIFY_LAYER)
	struct otg_notify *o_notify = get_otg_notify();
#endif
#if IS_ENABLED(CONFIG_BATTERY_SAMSUNG)
	struct power_supply *psy;
#endif

	VDM_MSG_IRQ_State.DATA = 0x0;
	init_usbc_cmd_data(&value);
	msg_maxim(" pd_msg [%x]", pd_msg);

	switch (pd_msg) {
	case Nothing_happened:
		usbc_data->pd_data->src_cap_done = CC_SNK;
		usbc_data->pd_data->req_pdo_type = PDO_TYPE_FIXED;
		usbc_data->pd_data->psrdy_sent = false;
		clear_chg_info(usbc_data);
		break;
	case Sink_PD_PSRdy_received:
		max77705_get_chg_info(usbc_data);
		/* currently, do nothing
		 * calling max77705_check_pdo() has been moved to max77705_psrdy_irq()
		 * for specific PD charger issue
		 */
		break;
	case Sink_PD_Error_Recovery:
		break;
	case Sink_PD_SenderResponseTimer_Timeout:
		msg_maxim("Sink_PD_SenderResponseTimer_Timeout received.");
	/*	queue_work(usbc_data->op_send_queue, &usbc_data->op_send_work); */
		break;
	case Source_PD_PSRdy_Sent:
		if (usbc_data->mpsm_mode && (usbc_data->pd_pr_swap == cc_SOURCE)) {
			max77705_usbc_disable_auto_vbus(usbc_data);
			max77705_vbus_turn_on_ctrl(usbc_data, OFF, false);
		}
		if ((usbc_data->pd_data->src_cap_done == CC_SRC) &&
			(usbc_data->pd_data->d2d_type != D2D_NONE))
			max77705_check_req_pdo(usbc_data);
		usbc_data->pd_data->psrdy_sent = true;
		break;
	case Source_PD_Error_Recovery:
		break;
	case Source_PD_SenderResponseTimer_Timeout:
		max77705_vbus_turn_on_ctrl(usbc_data, OFF, false);
		schedule_delayed_work(&usbc_data->vbus_hard_reset_work, msecs_to_jiffies(800));
		break;
	case PD_DR_Swap_Request_Received:
		msg_maxim("DR_SWAP received.");
#if IS_ENABLED(CONFIG_USB_HOST_NOTIFY)
		send_otg_notify(o_notify, NOTIFY_EVENT_DR_SWAP, 1);
#endif
		/* currently, do nothing
		 * calling max77705_check_pdo() has been moved to max77705_psrdy_irq()
		 * for specific PD charger issue
		 */
		break;
	case PD_PR_Swap_Request_Received:
		msg_maxim("PR_SWAP received.");
		break;
	case PD_VCONN_Swap_Request_Received:
		msg_maxim("VCONN_SWAP received.");
		break;
	case Received_PD_Message_in_illegal_state:
		break;
	case Samsung_Accessory_is_attached:
		break;
	case VDM_Attention_message_Received:
		break;
	case Sink_PD_Disabled:
#if 0
		/* to do */
		/* AFC HV */
		value[0] = 0x20;
		rc = max77705_ccpd_write_command(chip, value, 1);
		if (rc > 0)
			pr_err("failed to send command\n");
#endif
		break;
	case Source_PD_Disabled:
		break;
	case Prswap_Snktosrc_Sent:
		usbc_data->pd_pr_swap = cc_SOURCE;
		break;
	case Prswap_Srctosnk_Sent:
		usbc_data->pd_pr_swap = cc_SINK;
		break;
	case HARDRESET_RECEIVED:
		max77705_ccic_event_work(usbc_data,
			PDIC_NOTIFY_DEV_ALL, PDIC_NOTIFY_ID_CLEAR_INFO,
			PDIC_NOTIFY_ID_DEVICE_INFO, 0, 0);
		max77705_ccic_event_work(usbc_data,
			PDIC_NOTIFY_DEV_ALL, PDIC_NOTIFY_ID_CLEAR_INFO,
			PDIC_NOTIFY_ID_SVID_INFO, 0, 0);
		usbc_data->send_enter_mode_req = 0;
		/*turn off the vbus both Source and Sink*/
		if (usbc_data->cc_data->current_pr == SRC) {
			max77705_vbus_turn_on_ctrl(usbc_data, OFF, false);
			schedule_delayed_work(&usbc_data->vbus_hard_reset_work, msecs_to_jiffies(760));
		} else if (usbc_data->cc_data->current_pr == SNK) {
			usbc_data->detach_done_wait = 1;
#if IS_ENABLED(CONFIG_BATTERY_SAMSUNG)
			psy = power_supply_get_by_name("battery");
			if (psy) {
				val.intval = 0;
				psy_do_property("battery", set,
					POWER_SUPPLY_EXT_PROP_HARDRESET_OCCUR, val);
			} else {
				pr_err("%s: Fail to get psy battery\n", __func__);
			}
#endif
		}
#ifdef CONFIG_USB_NOTIFY_PROC_LOG
		event = NOTIFY_EXTRA_HARDRESET_RECEIVED;
		store_usblog_notify(NOTIFY_EXTRA, (void *)&event, NULL);
#endif
		cancel_delayed_work(&usbc_data->pd_data->send_identity_work);
		break;
	case HARDRESET_SENT:
		max77705_ccic_event_work(usbc_data,
			PDIC_NOTIFY_DEV_ALL, PDIC_NOTIFY_ID_CLEAR_INFO,
			PDIC_NOTIFY_ID_DEVICE_INFO, 0, 0);
		max77705_ccic_event_work(usbc_data,
			PDIC_NOTIFY_DEV_ALL, PDIC_NOTIFY_ID_CLEAR_INFO,
			PDIC_NOTIFY_ID_SVID_INFO, 0, 0);
		usbc_data->send_enter_mode_req = 0;
		/*turn off the vbus both Source and Sink*/
		if (usbc_data->cc_data->current_pr == SRC) {
			max77705_vbus_turn_on_ctrl(usbc_data, OFF, false);
			schedule_delayed_work(&usbc_data->vbus_hard_reset_work, msecs_to_jiffies(760));
		} else if (usbc_data->cc_data->current_pr == SNK) {
			usbc_data->detach_done_wait = 1;
#if IS_ENABLED(CONFIG_BATTERY_SAMSUNG)
			psy = power_supply_get_by_name("battery");
			if (psy) {
				val.intval = 1;
				psy_do_property("battery", set,
					POWER_SUPPLY_EXT_PROP_HARDRESET_OCCUR, val);
			} else {
				pr_err("%s: Fail to get psy battery\n", __func__);
			}
#endif
		}
#ifdef CONFIG_USB_NOTIFY_PROC_LOG
		event = NOTIFY_EXTRA_HARDRESET_SENT;
		store_usblog_notify(NOTIFY_EXTRA, (void *)&event, NULL);
#endif
		cancel_delayed_work(&usbc_data->pd_data->send_identity_work);
		break;
	case Get_Vbus_turn_on:
		break;
	case Get_Vbus_turn_off:
		max77705_vbus_turn_on_ctrl(usbc_data, OFF, false);
		break;
	case PRSWAP_SRCTOSWAP:
		usbc_data->pd_data->req_pdo_type = PDO_TYPE_FIXED;
		max77705_notify_prswap(usbc_data, PRSWAP_SRCTOSWAP);
		max77705_vbus_turn_on_ctrl(usbc_data, OFF, false);
		msg_maxim("PRSWAP_SRCTOSWAP : [%x]", pd_msg);
		break;
	case PRSWAP_SWAPTOSNK:
		max77705_vbus_turn_on_ctrl(usbc_data, OFF, false);
		msg_maxim("PRSWAP_SWAPTOSNK : [%x]", pd_msg);
		break;
	case PRSWAP_SNKTOSWAP:
		msg_maxim("PRSWAP_SNKTOSWAP : [%x]", pd_msg);
		max77705_notify_prswap(usbc_data, PRSWAP_SNKTOSWAP);
		/* CHGINSEL disable */
		psy_charger = power_supply_get_by_name("max77705-charger");
		if (psy_charger) {
			val.intval = 0;
			psy_do_property("max77705-charger", set, POWER_SUPPLY_EXT_PROP_CHGINSEL, val);
		} else {
			pr_err("%s: Fail to get psy charger\n", __func__);
		}
		break;
	case PRSWAP_SWAPTOSRC:
		if (usbc_data->pd_data->d2d_type == D2D_SRCSNK)
			max77705_send_new_src_cap(g_usbc_data,
				usbc_data->pd_data->auth_type, usbc_data->pd_data->d2d_type);
		max77705_vbus_turn_on_ctrl(usbc_data, ON, false);
		msg_maxim("PRSWAP_SNKTOSRC : [%x]", pd_msg);
		break;
	case Current_Cable_Connected:
		max77705_manual_jig_on(usbc_data, 1);
		usbc_data->manual_lpm_mode = 1;
		msg_maxim("Current_Cable_Connected : [%x]", pd_msg);
		break;
	case SRC_CAP_RECEIVED:
		break;
	case Status_Received:
		value.opcode = OPCODE_SAMSUNG_READ_MESSAGE;
		value.write_data[0] = 0x02;
		value.write_length = 1;
		value.read_length = 32;
		max77705_usbc_opcode_write(usbc_data, &value);
		msg_maxim("@TA_ALERT: Status Receviced : [%x]", pd_msg);
		break;
	case Alert_Message:
		value.opcode = OPCODE_SAMSUNG_READ_MESSAGE;
		value.write_data[0] = 0x01;
		value.write_length = 1;
		value.read_length = 32;
		max77705_usbc_opcode_write(usbc_data, &value);
		msg_maxim("@TA_ALERT: Alert Message : [%x]", pd_msg);
		break;
	case PDMSG_DP_ENTER_MODE:
		/* To check SVID of enter mode */
		value.opcode = OPCODE_SAMSUNG_READ_MESSAGE;
		value.write_data[0] = 0x03;
		value.write_length = 1;
		value.read_length = 32;
		max77705_usbc_opcode_write(usbc_data, &value);
		msg_maxim("Enter mode Receviced : [%x]", pd_msg);
		break;
	case PDMSG_SRC_ACCEPT:
		if ((usbc_data->pd_data->src_cap_done == CC_SRC) &&
			(usbc_data->pd_data->d2d_type != D2D_NONE))
			max77705_check_req_pdo(usbc_data);
		msg_maxim("SRC ACCEPT : [%x]", pd_msg);
		break;
	default:
		break;
	}
}

void max77705_pd_check_pdmsg_callback(void *data, u8 pdmsg)
{
	struct max77705_usbc_platform_data *usbc_data = data;
	union power_supply_propval val;

	if (!usbc_data) {
		msg_maxim("usbc_data is null");
		return;
	}

	if (!usbc_data->pd_data->psrdy_received &&
		(pdmsg == Sink_PD_PSRdy_received || pdmsg == SRC_CAP_RECEIVED)) {
		msg_maxim("pdmsg=%x", pdmsg);
		val.intval = 1;
		psy_do_property("battery", set, POWER_SUPPLY_EXT_PROP_SRCCAP, val);
	} else if (usbc_data->pd_data->psrdy_received && (pdmsg == SRC_CAP_RECEIVED)) {
		msg_maxim("pdmsg=%x", pdmsg);
		val.intval = 0;
		psy_do_property("battery", set, POWER_SUPPLY_EXT_PROP_SRCCAP, val);
	}
}

static void max77705_pd_rid(struct max77705_usbc_platform_data *usbc_data, u8 fct_id)
{
	struct max77705_pd_data *pd_data = usbc_data->pd_data;

	u8 prev_rid = pd_data->device;
#if IS_ENABLED(CONFIG_PDIC_NOTIFIER)
	static int rid = RID_OPEN;
#endif

	switch (fct_id) {
	case FCT_GND:
		msg_maxim(" RID_000K");
		pd_data->device = DEV_FCT_0;
#if IS_ENABLED(CONFIG_PDIC_NOTIFIER)
		rid = RID_000K;
#endif
		break;
	case FCT_1Kohm:
		msg_maxim(" RID_001K");
		pd_data->device = DEV_FCT_1K;
#if IS_ENABLED(CONFIG_PDIC_NOTIFIER)
		rid = RID_001K;
#endif
		break;
	case FCT_255Kohm:
		msg_maxim(" RID_255K");
		pd_data->device = DEV_FCT_255K;
		usbc_data->rid_check = true;
#if IS_ENABLED(CONFIG_PDIC_NOTIFIER)
		rid = RID_255K;
#endif
		break;
	case FCT_301Kohm:
		msg_maxim(" RID_301K");
		pd_data->device = DEV_FCT_301K;
		usbc_data->rid_check = true;
#if IS_ENABLED(CONFIG_PDIC_NOTIFIER)
		rid = RID_301K;
#endif
		break;
	case FCT_523Kohm:
		msg_maxim(" RID_523K");
		pd_data->device = DEV_FCT_523K;
		usbc_data->rid_check = true;
#if IS_ENABLED(CONFIG_PDIC_NOTIFIER)
		rid = RID_523K;
#endif
		break;
	case FCT_619Kohm:
		msg_maxim(" RID_619K");
		pd_data->device = DEV_FCT_619K;
		usbc_data->rid_check = true;
#if IS_ENABLED(CONFIG_PDIC_NOTIFIER)
		rid = RID_619K;
#endif
		break;
	case FCT_OPEN:
		msg_maxim(" RID_OPEN");
		pd_data->device = DEV_FCT_OPEN;
#if IS_ENABLED(CONFIG_PDIC_NOTIFIER)
		rid = RID_OPEN;
#endif
		break;
	default:
		msg_maxim(" RID_UNDEFINED");
		pd_data->device = DEV_UNKNOWN;
#if IS_ENABLED(CONFIG_PDIC_NOTIFIER)
		rid = RID_UNDEFINED;
#endif
		break;
	}

	if (prev_rid != pd_data->device) {
#if IS_ENABLED(CONFIG_PDIC_NOTIFIER)
		/* RID */
		max77705_ccic_event_work(usbc_data,
			PDIC_NOTIFY_DEV_MUIC, PDIC_NOTIFY_ID_RID,
			rid, 0, 0);
		usbc_data->cur_rid = rid;
		/* turn off USB */
		if (pd_data->device == DEV_FCT_OPEN || pd_data->device == DEV_UNKNOWN
			|| pd_data->device == DEV_FCT_523K || pd_data->device == DEV_FCT_619K) {

			usbc_data->typec_power_role = TYPEC_SINK;

			/* usb or otg */
			max77705_ccic_event_work(usbc_data,
				PDIC_NOTIFY_DEV_USB, PDIC_NOTIFY_ID_USB,
				0/*attach*/, USB_STATUS_NOTIFY_DETACH/*drp*/, 0);
		}
#endif
	}
}

static irqreturn_t max77705_pdmsg_irq(int irq, void *data)
{
	struct max77705_usbc_platform_data *usbc_data = data;
	struct max77705_pd_data *pd_data = usbc_data->pd_data;
	u8 pdmsg = 0;

	max77705_read_reg(usbc_data->muic, REG_PD_STATUS0, &pd_data->pd_status0);
	pdmsg = pd_data->pd_status0;
	msg_maxim("IRQ(%d)_IN pdmsg: %02x", irq, pdmsg);
	max77705_pd_check_pdmsg(usbc_data, pdmsg);
	pd_data->pdsmg = pdmsg;
	msg_maxim("IRQ(%d)_OUT", irq);

	return IRQ_HANDLED;
}

static irqreturn_t max77705_psrdy_irq(int irq, void *data)
{
	struct max77705_usbc_platform_data *usbc_data = data;
	u8 psrdy_received = 0;
	enum typec_pwr_opmode mode = TYPEC_PWR_MODE_USB;
#if IS_ENABLED(CONFIG_USB_NOTIFY_LAYER)
	struct otg_notify *o_notify = get_otg_notify();
#endif

	msg_maxim("IN");
	max77705_read_reg(usbc_data->muic, REG_PD_STATUS1, &usbc_data->pd_status1);
	psrdy_received = (usbc_data->pd_status1 & BIT_PD_PSRDY)
			>> FFS(BIT_PD_PSRDY);

	if (psrdy_received && !usbc_data->pd_support
			&& usbc_data->pd_data->cc_status != CC_NO_CONN)
		usbc_data->pd_support = true;

	if (usbc_data->typec_try_state_change == TRY_ROLE_SWAP_PR &&
		usbc_data->pd_support) {
		msg_maxim("typec_reverse_completion");
		usbc_data->typec_try_state_change = TRY_ROLE_SWAP_NONE;
		complete(&usbc_data->typec_reverse_completion);
	}
	msg_maxim("psrdy_received=%d, usbc_data->pd_support=%d, cc_status=%d, src_cp_dn=%d",
		psrdy_received, usbc_data->pd_support, usbc_data->pd_data->cc_status,
		usbc_data->pd_data->src_cap_done);

	mode = max77705_get_pd_support(usbc_data);
	typec_set_pwr_opmode(usbc_data->port, mode);
#if IS_ENABLED(CONFIG_USB_NOTIFY_LAYER)
	if (mode == TYPEC_PWR_MODE_PD)
		send_otg_notify(o_notify, NOTIFY_EVENT_PD_CONTRACT, 1);
	else
		send_otg_notify(o_notify, NOTIFY_EVENT_PD_CONTRACT, 0);
#endif

	if (usbc_data->pd_data->cc_status == CC_SNK && psrdy_received) {
		max77705_check_pdo(usbc_data);
		usbc_data->pd_data->psrdy_received = true;
		usbc_data->pd_data->src_cap_done = CC_SNK;
	}

	if (psrdy_received && usbc_data->pd_data->cc_status != CC_NO_CONN) {
		if (usbc_data->pd_data->cc_status == CC_SRC) {
			if (usbc_data->pd_data->src_cap_done != CC_SRC) {
				cancel_delayed_work(&usbc_data->pd_data->d2d_work);
				/* send the PD message after 1000ms. */
				queue_delayed_work(usbc_data->pd_data->wqueue,
					&usbc_data->pd_data->d2d_work, msecs_to_jiffies(1000));
			}
		}
		usbc_data->pn_flag = true;
		complete(&usbc_data->psrdy_wait);
	}

	msg_maxim("OUT");
	return IRQ_HANDLED;
}

bool max77705_sec_pps_control(int en)
{
	struct max77705_usbc_platform_data *pusbpd = g_usbc_data;
	union power_supply_propval val = {0,};

	msg_maxim(": %d", en);

	val.intval = en; /* 0: stop pps, 1: start pps */
	psy_do_property("battery", set,
		POWER_SUPPLY_EXT_PROP_DIRECT_SEND_UVDM, val);
	if (!en && !pusbpd->pn_flag) {
		reinit_completion(&pusbpd->psrdy_wait);
		if (!wait_for_completion_timeout(&pusbpd->psrdy_wait, msecs_to_jiffies(1000))) {
			msg_maxim("PSRDY COMPLETION TIMEOUT");
			return false;
		}
	}
	return true;
}

static void max77705_datarole_irq_handler(void *data, int irq)
{
	struct max77705_usbc_platform_data *usbc_data = data;
	struct max77705_pd_data *pd_data = usbc_data->pd_data;
	u8 datarole = 0;

	max77705_read_reg(usbc_data->muic, REG_PD_STATUS1, &pd_data->pd_status1);
	datarole = (pd_data->pd_status1 & BIT_PD_DataRole)
			>> FFS(BIT_PD_DataRole);
	/* abnormal data role without setting power role */
	if (usbc_data->cc_data->current_pr == 0xFF) {
		msg_maxim("INVALID IRQ IRQ(%d)_OUT", irq);
		return;
	}

	if (irq == CCIC_IRQ_INIT_DETECT) {
		if (usbc_data->pd_data->cc_status == CC_SNK)
			msg_maxim("initial time : SNK");
		else
			return;
	}

	switch (datarole) {
	case UFP:
			if (pd_data->current_dr != UFP) {
				pd_data->previous_dr = pd_data->current_dr;
				pd_data->current_dr = UFP;
				if (pd_data->previous_dr != 0xFF)
					msg_maxim("%s detach previous usb connection\n", __func__);
				max77705_notify_dr_status(usbc_data, 1);
				if (usbc_data->typec_try_state_change == TRY_ROLE_SWAP_DR ||
					usbc_data->typec_try_state_change == TRY_ROLE_SWAP_TYPE) {
					msg_maxim("typec_reverse_completion");
					usbc_data->typec_try_state_change = TRY_ROLE_SWAP_NONE;
				complete(&usbc_data->typec_reverse_completion);
				}
			}
			msg_maxim(" UFP");
			break;

	case DFP:
			if (pd_data->current_dr != DFP) {
				pd_data->previous_dr = pd_data->current_dr;
				pd_data->current_dr = DFP;
				if (pd_data->previous_dr != 0xFF)
					msg_maxim("%s detach previous usb connection\n", __func__);

				max77705_notify_dr_status(usbc_data, 1);
				if (usbc_data->typec_try_state_change == TRY_ROLE_SWAP_DR ||
					usbc_data->typec_try_state_change == TRY_ROLE_SWAP_TYPE) {
					msg_maxim("typec_reverse_completion");
					usbc_data->typec_try_state_change = TRY_ROLE_SWAP_NONE;
				complete(&usbc_data->typec_reverse_completion);
				}

				if (usbc_data->cc_data->current_pr == SNK && !(usbc_data->is_first_booting)) {
					cancel_delayed_work(&pd_data->send_identity_work);
					queue_delayed_work(pd_data->wqueue,
							&pd_data->send_identity_work,
							msecs_to_jiffies(300));
					msg_maxim("SEND THE IDENTITY REQUEST FROM DFP HANDLER after 300ms");
				}
			}
			msg_maxim(" DFP");
			break;

	default:
			msg_maxim(" DATAROLE(Never Call this routine)");
			break;
	}
}

static irqreturn_t max77705_datarole_irq(int irq, void *data)
{
	pr_debug("%s: IRQ(%d)_IN\n", __func__, irq);
	max77705_datarole_irq_handler(data, irq);
	pr_debug("%s: IRQ(%d)_OUT\n", __func__, irq);
	return IRQ_HANDLED;
}

static irqreturn_t max77705_ssacc_irq(int irq, void *data)
{
	struct max77705_usbc_platform_data *usbc_data = data;
	struct max77705_pd_data *pd_data = usbc_data->pd_data;

	pr_debug("%s: IRQ(%d)_IN\n", __func__, irq);
	msg_maxim(" SSAcc command received");
	/* Read through Opcode command 0x50 */
	pd_data->ssacc = 1;
	pr_debug("%s: IRQ(%d)_OUT\n", __func__, irq);
	return IRQ_HANDLED;
}

static void max77705_check_cc_sbu_short(void *data)
{
	u8 cc_status1 = 0;

	struct max77705_usbc_platform_data *usbc_data = data;
	struct max77705_pd_data *pd_data = usbc_data->pd_data;

	max77705_read_reg(usbc_data->muic, REG_CC_STATUS1, &cc_status1);
	/* 0b01: CC-5V, 0b10: SBU-5V, 0b11: SBU-GND Short */
	cc_status1 = (cc_status1 & BIT_CCSBUSHORT) >> FFS(BIT_CCSBUSHORT);
	if (cc_status1)
		pd_data->cc_sbu_short = true;

	msg_maxim("%s cc_status1 : %x, cc_sbu_short : %d\n", __func__, cc_status1, pd_data->cc_sbu_short);
}

static u8 max77705_check_rid(void *data)
{
	u8 fct_id = 0;
	struct max77705_usbc_platform_data *usbc_data = data;
	struct max77705_pd_data *pd_data = usbc_data->pd_data;

	max77705_read_reg(usbc_data->muic, REG_PD_STATUS1, &pd_data->pd_status1);
	fct_id = (pd_data->pd_status1 & BIT_FCT_ID) >> FFS(BIT_FCT_ID);
#if defined(CONFIG_SEC_FACTORY)
	factory_execute_monitor(FAC_ABNORMAL_REPEAT_RID);
#endif
	max77705_pd_rid(usbc_data, fct_id);
	pd_data->fct_id = fct_id;
	msg_maxim("%s rid : %d, fct_id : %d\n", __func__, usbc_data->cur_rid, fct_id);
	return fct_id;
}

static irqreturn_t max77705_fctid_irq(int irq, void *data)
{
	pr_debug("%s: IRQ(%d)_IN\n", __func__, irq);
	max77705_check_rid(data);
	pr_debug("%s: IRQ(%d)_OUT\n", __func__, irq);
	return IRQ_HANDLED;
}

void set_src_pdo_data(usbc_cmd_data *value,
	int pdo_n, int p_type, int curr)
{
	value->opcode = OPCODE_SET_SRCCAP;
	value->write_data[1 + (pdo_n * 4)] = (u8)(curr / 10);

	if (p_type == VPDO_TYPE) {
		/* 7~9v vpdo */
		value->write_data[2 + (pdo_n * 4)] = 0x30;
		value->write_data[3 + (pdo_n * 4)] = 0x42;
		value->write_data[4 + (pdo_n * 4)] = 0x8B;
	} else {
		/* 5v fpdo */
		value->write_data[2 + (pdo_n * 4)] = 0x90;
		value->write_data[3 + (pdo_n * 4)] = 0x01;
		value->write_data[4 + (pdo_n * 4)] = 0x36;
	}
}

void set_varible_pdo_data(usbc_cmd_data *value, int auth_t, int d2d_t)
{
	value->opcode = OPCODE_SET_SRCCAP;
	if ((d2d_t == D2D_SRCSNK) &&
		(auth_t == AUTH_HIGH_PWR)) {
		value->write_data[0] = 0x2;
		//        0x36019032, //5V, 500mA
		//        0x36019064, //5V, 1 A
		//		  0x3602D0C8, //9V, 2 A
		set_src_pdo_data(value, 0, FPDO_TYPE, 500);
		//        0x8B4230AA, //Variable :7V~9V Max15W
		set_src_pdo_data(value, 1, VPDO_TYPE, 1650);
		value->write_length = 12;
		value->read_length = 1;
	} else if ((d2d_t == D2D_SNKONLY) &&
		(auth_t == AUTH_HIGH_PWR)) {
		value->write_data[0] = 0x1;
		// 0x36019096, //5V, 1.5A
		set_src_pdo_data(value, 0, FPDO_TYPE, 1500);
		value->write_length = 7;
		value->read_length = 1;
	} else {
		value->write_data[0] = 0x1;
		//        0x36019032, //5V, 500mA
		set_src_pdo_data(value, 0, FPDO_TYPE, 500);
		value->write_length = 7;
		value->read_length = 1;
	}
}

void max77705_set_fpdo_srccap(usbc_cmd_data *value, int max_cur)
{
	value->write_data[0] = 0x1;
	set_src_pdo_data(value, 0, FPDO_TYPE, max_cur);
	value->write_length = 7;
	value->read_length = 1;
}

void max77705_forced_change_srccap(int max_cur)
{
	usbc_cmd_data value;

	init_usbc_cmd_data(&value);

	max77705_set_fpdo_srccap(&value, max_cur);
	max77705_usbc_opcode_write(g_usbc_data, &value);

	pr_info("%s : write => OPCODE(0x%02x) W_LENGTH(%d) R_LENGTH(%d)\n",
		__func__, value.opcode, value.write_length, value.read_length);
}

static void max77705_send_new_src_cap(struct max77705_usbc_platform_data *pusbpd,
	int auth, int d2d_type)
{
	usbc_cmd_data value;
	init_usbc_cmd_data(&value);
	set_varible_pdo_data(&value, auth, d2d_type);
	max77705_usbc_opcode_write(pusbpd, &value);

	pr_info("%s : write => OPCODE(0x%02x) W_LENGTH(%d) R_LENGTH(%d)\n",
		__func__, value.opcode, value.write_length, value.read_length);
}

void max77705_send_new_src_cap_push(struct max77705_usbc_platform_data *pusbpd,
	int auth, int d2d_type)
{
	usbc_cmd_data value;
	init_usbc_cmd_data(&value);
	set_varible_pdo_data(&value, auth, d2d_type);
	max77705_usbc_opcode_push(pusbpd, &value);

	pr_info("%s : push => OPCODE(0x%02x) W_LENGTH(%d) R_LENGTH(%d)\n",
		__func__, value.opcode, value.write_length, value.read_length);
}

static void max77705_send_srcap_work(struct work_struct *work)
{
	struct max77705_pd_data *pd_data = g_usbc_data->pd_data;
	int auth = pd_data->auth_type;
	int d2d_type = pd_data->d2d_type;

	if ((pd_data->src_cap_done != CC_SRC) &&
		(auth == AUTH_HIGH_PWR && d2d_type != D2D_NONE)) {
		max77705_send_new_src_cap(g_usbc_data, auth, d2d_type);
		pd_data->src_cap_done = CC_SRC;
		pr_info("%s\n", __func__);
    } else {
        pr_info("%s Donot Send the new SRC_CAP\n", __func__);
    }
}

void max77705_vpdo_auth(int auth, int d2d_type)
{
	struct max77705_pd_data *pd_data = g_usbc_data->pd_data;

	if (d2d_type == D2D_NONE)
		return;

	if (pd_data->cc_status == CC_SRC) {
		if (((pd_data->auth_type == AUTH_HIGH_PWR) && (auth == AUTH_LOW_PWR)) ||
				((pd_data->auth_type == AUTH_LOW_PWR) && (auth == AUTH_HIGH_PWR))) {
			max77705_send_new_src_cap(g_usbc_data, auth, d2d_type);
			pd_data->src_cap_done = CC_SRC;
			pr_info("%s: change src %s -> %s\n", __func__,
				(auth == AUTH_LOW_PWR) ? "HIGH PWR" : "LOW PWR",
				(auth == AUTH_LOW_PWR) ? "LOW PWR" : "HIGH PWR");
		}
	} else if ((pd_data->cc_status == CC_SNK) &&
		(auth == AUTH_HIGH_PWR)) {
		pr_info("%s: preset vpdo auth for prswap snk to src\n", __func__);
	}

	/* set default src cap for detach or hard reset case */
	if (pd_data->cc_status != CC_SNK) {
		if ((pd_data->auth_type == AUTH_HIGH_PWR) && (auth == AUTH_NONE)) {
			max77705_send_new_src_cap(g_usbc_data, auth, d2d_type);
			pr_info("%s: set to default src cap\n", __func__);
		}
	}

	pr_info("%s: vpdo auth set (%d, %d)\n", __func__, auth, d2d_type);
	pd_data->auth_type = auth;
	pd_data->d2d_type = d2d_type;
}

static void max77705_check_enter_mode(void *data)
{
	u8 pd_status1 = 0, enter_mode = 0;
	int ret;
	usbc_cmd_data value;
	struct max77705_usbc_platform_data *usbc_data = data;

	init_usbc_cmd_data(&value);

	ret = max77705_read_reg(usbc_data->muic, REG_PD_STATUS1, &pd_status1);

	if (ret) {
		pr_err("%s fail to read REG_PD_STATUS1 reg\n", __func__);
		return;
	}

	/* 0b01: Enter Mode : 0b00 : Not Enter mode */
	enter_mode = (pd_status1 & BIT_PD_ENTER_MODE) >> FFS(BIT_PD_ENTER_MODE);
	if (enter_mode) {
		value.opcode = OPCODE_SAMSUNG_READ_MESSAGE;
		value.write_data[0] = 0x03;
		value.write_length = 1;
		value.read_length = 32;
		max77705_usbc_opcode_write(usbc_data, &value);
	}
	msg_maxim("%s pd_status1 : %x, enter_mode : %d\n", __func__, pd_status1, enter_mode);
}

int max77705_pd_init(struct max77705_usbc_platform_data *usbc_data)
{
	struct max77705_pd_data *pd_data = usbc_data->pd_data;
	int ret = 0;

	msg_maxim(" IN(%d)", pd_data->pd_noti.sink_status.rp_currentlvl);

	/* skip below codes for detecting incomplete connection cable. */
	/* pd_data->pd_noti.sink_status.rp_currentlvl = RP_CURRENT_LEVEL_NONE; */
	pd_data->pd_noti.sink_status.available_pdo_num = 0;
	pd_data->pd_noti.sink_status.selected_pdo_num = 0;
	pd_data->pd_noti.sink_status.current_pdo_num = 0;
	pd_data->pd_noti.sink_status.pps_voltage = 0;
	pd_data->pd_noti.sink_status.pps_current = 0;
	pd_data->pd_noti.sink_status.has_apdo = false;
	pd_data->pd_noti.sink_status.fp_sec_pd_select_pdo = max77705_select_pdo;
	pd_data->pd_noti.sink_status.fp_sec_pd_select_pps = max77705_select_pps;
	pd_data->pd_noti.sink_status.fp_sec_pd_vpdo_auth = max77705_vpdo_auth;
	pd_data->pd_noti.sink_status.fp_sec_pd_manual_ccopen_req = pdic_manual_ccopen_request;
	pd_data->pd_noti.sink_status.fp_sec_pd_change_src = max77705_forced_change_srccap;

	/* skip below codes for detecting incomplete connection cable. */
	/* pd_data->pd_noti.event = PDIC_NOTIFY_EVENT_DETACH; */
	pd_data->pdo_list = false;
	pd_data->psrdy_received = false;
	pd_data->cc_sbu_short = false;

	pd_data->wqueue = create_singlethread_workqueue("max77705_pd");
	if (!pd_data->wqueue) {
		pr_err("%s: Fail to Create Workqueue\n", __func__);
		goto err_irq;
	}

	INIT_DELAYED_WORK(&pd_data->d2d_work, max77705_send_srcap_work);
	INIT_DELAYED_WORK(&pd_data->retry_work, max77705_pd_retry_work);
	INIT_DELAYED_WORK(&pd_data->abnormal_pdo_work, max77705_abnormal_pdo_work);
	INIT_DELAYED_WORK(&pd_data->send_identity_work, max77705_send_identity_work);

	pd_data->irq_pdmsg = usbc_data->irq_base + MAX77705_PD_IRQ_PDMSG_INT;
	if (pd_data->irq_pdmsg) {
		ret = request_threaded_irq(pd_data->irq_pdmsg,
			   NULL, max77705_pdmsg_irq,
			   0,
			   "pd-pdmsg-irq", usbc_data);
		if (ret) {
			pr_err("%s: Failed to Request IRQ (%d)\n", __func__, ret);
			goto err_irq;
		}
	}

	pd_data->irq_psrdy = usbc_data->irq_base + MAX77705_PD_IRQ_PS_RDY_INT;
	if (pd_data->irq_psrdy) {
		ret = request_threaded_irq(pd_data->irq_psrdy,
			   NULL, max77705_psrdy_irq,
			   0,
			   "pd-psrdy-irq", usbc_data);
		if (ret) {
			pr_err("%s: Failed to Request IRQ (%d)\n", __func__, ret);
			goto err_irq;
		}
	}

	pd_data->irq_datarole = usbc_data->irq_base + MAX77705_PD_IRQ_DATAROLE_INT;
	if (pd_data->irq_datarole) {
		ret = request_threaded_irq(pd_data->irq_datarole,
			   NULL, max77705_datarole_irq,
			   0,
			   "pd-datarole-irq", usbc_data);
		if (ret) {
			pr_err("%s: Failed to Request IRQ (%d)\n", __func__, ret);
			goto err_irq;
		}
	}

	pd_data->irq_ssacc = usbc_data->irq_base + MAX77705_PD_IRQ_SSACCI_INT;
	if (pd_data->irq_ssacc) {
		ret = request_threaded_irq(pd_data->irq_ssacc,
			   NULL, max77705_ssacc_irq,
			   0,
			   "pd-ssacci-irq", usbc_data);
		if (ret) {
			pr_err("%s: Failed to Request IRQ (%d)\n", __func__, ret);
			goto err_irq;
		}
	}
	pd_data->irq_fct_id = usbc_data->irq_base + MAX77705_PD_IRQ_FCTIDI_INT;
	if (pd_data->irq_fct_id) {
		ret = request_threaded_irq(pd_data->irq_fct_id,
			   NULL, max77705_fctid_irq,
			   0,
			   "pd-fctid-irq", usbc_data);
		if (ret) {
			pr_err("%s: Failed to Request IRQ (%d)\n", __func__, ret);
			goto err_irq;
		}
	}
	/* check RID value for booting time */
	max77705_check_rid(usbc_data);
	max77705_set_fw_noautoibus(MAX77705_AUTOIBUS_AT_OFF);
	max77705_set_snkcap(usbc_data);
	/* check CC Pin state for cable attach booting scenario */
	max77705_datarole_irq_handler(usbc_data, CCIC_IRQ_INIT_DETECT);
	max77705_check_cc_sbu_short(usbc_data);

	max77705_check_enter_mode(usbc_data);
	max77705_register_pdmsg_func(usbc_data->max77705,
		max77705_pd_check_pdmsg_callback, (void *)usbc_data);

	msg_maxim(" OUT(%d)", pd_data->pd_noti.sink_status.rp_currentlvl);
	return 0;

err_irq:
	kfree(pd_data);
	return ret;
}
