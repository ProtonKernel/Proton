/*
 * sec_charging_common.h
 * Samsung Mobile Charging Common Header
 *
 * Copyright (C) 2012 Samsung Electronics, Inc.
 *
 *
 * This software is licensed under the terms of the GNU General Public
 * License version 2, as published by the Free Software Foundation, and
 * may be copied, distributed, and modified under those terms.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 */

#ifndef __SEC_CHARGING_COMMON_H
#define __SEC_CHARGING_COMMON_H __FILE__

#include <linux/module.h>
#include <linux/init.h>
#include <linux/platform_device.h>
#include <linux/err.h>
#include <linux/delay.h>
#include <linux/gpio.h>
#include <linux/irq.h>
#include <linux/interrupt.h>
#include <linux/i2c.h>
#include <linux/power_supply.h>
#include <linux/slab.h>
#include <linux/device.h>
#include <linux/pm_wakeup.h>
#include <linux/battery/sec_battery_common.h>
#include <dt-bindings/battery/sec-battery.h>

/* definitions */
#define SEC_BATTERY_CABLE_HV_WIRELESS_ETX	100
#define SEC_BATTERY_CABLE_SILENT_TYPE		99

#define WC_AUTH_MSG		"@WC_AUTH "
#define WC_TX_MSG		"@Tx_Mode "

#define MFC_LDO_ON		1
#define MFC_LDO_OFF		0

#define TX_ID_CHECK_CNT		3

enum battery_thermal_zone {
	BAT_THERMAL_COLD = 0,
	BAT_THERMAL_COOL3,
	BAT_THERMAL_COOL2,
	BAT_THERMAL_COOL1,
	BAT_THERMAL_NORMAL,
	BAT_THERMAL_WARM,
	BAT_THERMAL_OVERHEAT,
	BAT_THERMAL_OVERHEATLIMIT,
};

enum rx_device_type {
	NO_DEV = 0,
	OTHER_DEV,
	SS_GEAR,
	SS_PHONE,
	SS_BUDS,
};

enum power_supply_ext_health {
	POWER_SUPPLY_EXT_HEALTH_MIN = 20,
	POWER_SUPPLY_EXT_HEALTH_UNDERVOLTAGE = POWER_SUPPLY_EXT_HEALTH_MIN,
	POWER_SUPPLY_EXT_HEALTH_OVERHEATLIMIT,
	POWER_SUPPLY_EXT_HEALTH_VSYS_OVP,
	POWER_SUPPLY_EXT_HEALTH_VBAT_OVP,
	POWER_SUPPLY_EXT_HEALTH_DC_ERR,
	POWER_SUPPLY_EXT_HEALTH_MAX,
};

enum sec_battery_current_type {
	/* uA */
	SEC_BATTERY_CURRENT_UA = 0,
	/* mA */
	SEC_BATTERY_CURRENT_MA,
};

enum sec_battery_voltage_type {
	/* uA */
	SEC_BATTERY_VOLTAGE_UV = 0,
	/* mA */
	SEC_BATTERY_VOLTAGE_MV,
};

#if IS_ENABLED(CONFIG_DUAL_BATTERY)
enum sec_battery_dual_mode {
	SEC_DUAL_BATTERY_MAIN = 0,
	SEC_DUAL_BATTERY_SUB,
};
#endif

enum sec_battery_capacity_mode {
	/* designed capacity */
	SEC_BATTERY_CAPACITY_DESIGNED = 0,
	/* absolute capacity by fuel gauge */
	SEC_BATTERY_CAPACITY_ABSOLUTE,
	/* temperary capacity in the time */
	SEC_BATTERY_CAPACITY_TEMPERARY,
	/* current capacity now */
	SEC_BATTERY_CAPACITY_CURRENT,
	/* cell aging information */
	SEC_BATTERY_CAPACITY_AGEDCELL,
	/* charge count */
	SEC_BATTERY_CAPACITY_CYCLE,
	/* full capacity rep */
	SEC_BATTERY_CAPACITY_FULL,
	/* QH capacity */
	SEC_BATTERY_CAPACITY_QH,
	/* vfsoc */
	SEC_BATTERY_CAPACITY_VFSOC,
};

enum sec_wireless_info_mode {
	SEC_WIRELESS_OTP_FIRM_RESULT = 0,
	SEC_WIRELESS_IC_REVISION,
	SEC_WIRELESS_IC_GRADE,
	SEC_WIRELESS_IC_CHIP_ID,
	SEC_WIRELESS_OTP_FIRM_VER_BIN,
	SEC_WIRELESS_OTP_FIRM_VER,
	SEC_WIRELESS_OTP_FIRM_VERIFY,
	SEC_WIRELESS_MST_SWITCH_VERIFY,
};

enum sec_wireless_firmware_update_mode {
	SEC_WIRELESS_FW_UPDATE_SDCARD_MODE = 0,		/* manual update mode , firmware file must be in sdcard */
	SEC_WIRELESS_FW_UPDATE_BUILTIN_MODE,		/* factory line update mode, MSP wirtes only this mode */
	SEC_WIRELESS_FW_UPDATE_AUTO_MODE,			/* auto update mode, it works during kernel on, very similar to BUILTIN MODE */
	SEC_WIRELESS_FW_UPDATE_SPU_MODE,			/* spu update mode */
	SEC_WIRELESS_FW_UPDATE_SPU_VERIFY_MODE,		/* for automation test */
};

enum sec_tx_sharing_mode {
	SEC_TX_OFF = 0,
	SEC_TX_STANDBY,
	SEC_TX_POWER_TRANSFER,
	SEC_TX_ERROR,
};

enum sec_wireless_auth_mode {
	WIRELESS_AUTH_WAIT = 0,
	WIRELESS_AUTH_START,
	WIRELESS_AUTH_SENT,
	WIRELESS_AUTH_RECEIVED,
	WIRELESS_AUTH_FAIL,
	WIRELESS_AUTH_PASS,
};

enum sec_wireless_rx_control_mode {
	WIRELESS_PAD_FAN_OFF = 0,
	WIRELESS_PAD_FAN_ON,
	WIRELESS_PAD_LED_OFF,
	WIRELESS_PAD_LED_ON,
	WIRELESS_PAD_LED_DIMMING,
	WIRELESS_VRECT_ADJ_ON,
	WIRELESS_VRECT_ADJ_OFF,
	WIRELESS_VRECT_ADJ_ROOM_0,
	WIRELESS_VRECT_ADJ_ROOM_1,
	WIRELESS_VRECT_ADJ_ROOM_2,
	WIRELESS_VRECT_ADJ_ROOM_3,
	WIRELESS_VRECT_ADJ_ROOM_4,
	WIRELESS_VRECT_ADJ_ROOM_5,
	WIRELESS_CLAMP_ENABLE,
	WIRELESS_SLEEP_MODE_ENABLE,
	WIRELESS_SLEEP_MODE_DISABLE,
};

enum sec_wireless_tx_vout {
	WC_TX_VOUT_OFF = 0,
	WC_TX_VOUT_5000MV = 5000,
	WC_TX_VOUT_5500MV = 5500,
	WC_TX_VOUT_6000MV = 6000,
	WC_TX_VOUT_6500MV = 6500,
	WC_TX_VOUT_7000MV = 7000,
	WC_TX_VOUT_7500MV = 7500,
	WC_TX_VOUT_8000MV = 8000,
	WC_TX_VOUT_8500MV = 8500,
	WC_TX_VOUT_9000MV = 9000,
	WC_TX_VOUT_MIN = WC_TX_VOUT_5000MV,
	WC_TX_VOUT_MAX = WC_TX_VOUT_9000MV,
	WC_TX_VOUT_STEP_AOV = 500,
};

enum sec_wireless_pad_id {
	WC_PAD_ID_UNKNOWN	= 0x00,
	/* 0x01~1F : Single Port */
	WC_PAD_ID_SNGL_NOBLE = 0x10,
	WC_PAD_ID_SNGL_VEHICLE,
	WC_PAD_ID_SNGL_MINI,
	WC_PAD_ID_SNGL_ZERO,
	WC_PAD_ID_SNGL_DREAM,
	/* 0x20~2F : Multi Port */
	/* 0x30~3F : Stand Type */
	WC_PAD_ID_STAND_HERO = 0x30,
	WC_PAD_ID_STAND_DREAM,
	/* 0x40~4F : External Battery Pack */
	WC_PAD_ID_EXT_BATT_PACK = 0x40,
	WC_PAD_ID_EXT_BATT_PACK_TA,
	/* 0x50~6F : Reserved */
	WC_PAD_ID_UNO_TX = 0x72,
	WC_PAD_ID_UNO_TX_B0 = 0x80,
	WC_PAD_ID_UNO_TX_B1,
	WC_PAD_ID_UNO_TX_B2,
	WC_PAD_ID_UNO_TX_MAX = 0x9F,
	WC_PAD_ID_AUTH_PAD = 0xA0,
	WC_PAD_ID_DAVINCI_PAD_V,
	WC_PAD_ID_DAVINCI_PAD_H,
	WC_PAD_ID_AUTH_PAD_ACLASS_END = 0xAF,
	WC_PAD_ID_AUTH_PAD_END = 0xBF,
	/* reserved 0xA1 ~ 0xBF for auth pad */
	WC_PAD_ID_MAX = 0xFF,
};

enum sec_battery_adc_channel {
	SEC_BAT_ADC_CHANNEL_CABLE_CHECK = 0,
	SEC_BAT_ADC_CHANNEL_BAT_CHECK,
	SEC_BAT_ADC_CHANNEL_TEMP,
	SEC_BAT_ADC_CHANNEL_TEMP_AMBIENT,
	SEC_BAT_ADC_CHANNEL_FULL_CHECK,
	SEC_BAT_ADC_CHANNEL_VOLTAGE_NOW,
	SEC_BAT_ADC_CHANNEL_CHG_TEMP,
	SEC_BAT_ADC_CHANNEL_INBAT_VOLTAGE,
	SEC_BAT_ADC_CHANNEL_DISCHARGING_CHECK,
	SEC_BAT_ADC_CHANNEL_DISCHARGING_NTC,
	SEC_BAT_ADC_CHANNEL_WPC_TEMP,
	SEC_BAT_ADC_CHANNEL_SUB_CHG_TEMP,
	SEC_BAT_ADC_CHANNEL_USB_TEMP,
	SEC_BAT_ADC_CHANNEL_SUB_BAT_TEMP,
	SEC_BAT_ADC_CHANNEL_BLKT_TEMP,
	SEC_BAT_ADC_CHANNEL_NUM,
};

enum sec_battery_charge_mode {
	SEC_BAT_CHG_MODE_BUCK_OFF = 0, /* buck, chg off */
	SEC_BAT_CHG_MODE_CHARGING_OFF,
	SEC_BAT_CHG_MODE_CHARGING, /* buck, chg on */
//	SEC_BAT_CHG_MODE_BUCK_ON,
	SEC_BAT_CHG_MODE_OTG_ON,
	SEC_BAT_CHG_MODE_OTG_OFF,
	SEC_BAT_CHG_MODE_UNO_ON,
	SEC_BAT_CHG_MODE_UNO_OFF,
	SEC_BAT_CHG_MODE_UNO_ONLY,
	SEC_BAT_CHG_MODE_NOT_SET,
	SEC_BAT_CHG_MODE_MAX,
};

/* charging mode */
enum sec_battery_charging_mode {
	/* no charging */
	SEC_BATTERY_CHARGING_NONE = 0,
	/* 1st charging */
	SEC_BATTERY_CHARGING_1ST,
	/* 2nd charging */
	SEC_BATTERY_CHARGING_2ND,
	/* recharging */
	SEC_BATTERY_CHARGING_RECHARGING,
};

/* POWER_SUPPLY_EXT_PROP_MEASURE_SYS */
enum sec_battery_measure_sys {
	SEC_BATTERY_ISYS_MA = 0,
	SEC_BATTERY_ISYS_UA,
	SEC_BATTERY_ISYS_AVG_MA,
	SEC_BATTERY_ISYS_AVG_UA,
	SEC_BATTERY_VSYS,
};

/* POWER_SUPPLY_EXT_PROP_MEASURE_INPUT */
enum sec_battery_measure_input {
	SEC_BATTERY_IIN_MA = 0,
	SEC_BATTERY_IIN_UA,
	SEC_BATTERY_VBYP,
	SEC_BATTERY_VIN_MA,
	SEC_BATTERY_VIN_UA,
};

enum sec_battery_direct_charging_source_ctrl {
	SEC_TEST_MODE = 0x1,
	SEC_SEND_UVDM = 0x2,
	SEC_STORE_MODE = 0x4,
};

/* tx_event */
#define BATT_TX_EVENT_WIRELESS_TX_STATUS		0x00000001
#define BATT_TX_EVENT_WIRELESS_RX_CONNECT		0x00000002
#define BATT_TX_EVENT_WIRELESS_TX_FOD			0x00000004
#define BATT_TX_EVENT_WIRELESS_TX_HIGH_TEMP		0x00000008
#define BATT_TX_EVENT_WIRELESS_RX_UNSAFE_TEMP	0x00000010
#define BATT_TX_EVENT_WIRELESS_RX_CHG_SWITCH	0x00000020
#define BATT_TX_EVENT_WIRELESS_RX_CS100			0x00000040
#define BATT_TX_EVENT_WIRELESS_TX_OTG_ON		0x00000080
#define BATT_TX_EVENT_WIRELESS_TX_LOW_TEMP		0x00000100
#define BATT_TX_EVENT_WIRELESS_TX_SOC_DRAIN		0x00000200
#define BATT_TX_EVENT_WIRELESS_TX_CRITICAL_EOC	0x00000400
#define BATT_TX_EVENT_WIRELESS_TX_CAMERA_ON		0x00000800
#define BATT_TX_EVENT_WIRELESS_TX_OCP			0x00001000
#define BATT_TX_EVENT_WIRELESS_TX_MISALIGN      0x00002000
#define BATT_TX_EVENT_WIRELESS_TX_ETC			0x00004000
#define BATT_TX_EVENT_WIRELESS_TX_RETRY			0x00008000
#define BATT_TX_EVENT_WIRELESS_TX_5V_TA			0x00010000
#define BATT_TX_EVENT_WIRELESS_TX_AC_MISSING	0x00020000
#define BATT_TX_EVENT_WIRELESS_ALL_MASK			0x0003ffff
#define BATT_TX_EVENT_WIRELESS_TX_ERR			(BATT_TX_EVENT_WIRELESS_TX_FOD | \
	BATT_TX_EVENT_WIRELESS_TX_HIGH_TEMP | BATT_TX_EVENT_WIRELESS_RX_UNSAFE_TEMP | \
	BATT_TX_EVENT_WIRELESS_RX_CHG_SWITCH | BATT_TX_EVENT_WIRELESS_RX_CS100 | \
	BATT_TX_EVENT_WIRELESS_TX_OTG_ON | BATT_TX_EVENT_WIRELESS_TX_LOW_TEMP | \
	BATT_TX_EVENT_WIRELESS_TX_SOC_DRAIN | BATT_TX_EVENT_WIRELESS_TX_CRITICAL_EOC | \
	BATT_TX_EVENT_WIRELESS_TX_CAMERA_ON | BATT_TX_EVENT_WIRELESS_TX_OCP | \
	BATT_TX_EVENT_WIRELESS_TX_MISALIGN | BATT_TX_EVENT_WIRELESS_TX_ETC | \
	BATT_TX_EVENT_WIRELESS_TX_5V_TA | BATT_TX_EVENT_WIRELESS_TX_AC_MISSING)

#define SEC_BAT_TX_RETRY_NONE			0x0000
#define SEC_BAT_TX_RETRY_MISALIGN		0x0001
#define SEC_BAT_TX_RETRY_CAMERA			0x0002
#define SEC_BAT_TX_RETRY_CALL			0x0004
#define SEC_BAT_TX_RETRY_MIX_TEMP		0x0008
#define SEC_BAT_TX_RETRY_HIGH_TEMP		0x0010
#define SEC_BAT_TX_RETRY_LOW_TEMP		0x0020
#define SEC_BAT_TX_RETRY_OCP			0x0040

/* ext_event */
#define BATT_EXT_EVENT_NONE			0x00000000
#define BATT_EXT_EVENT_CAMERA		0x00000001
#define BATT_EXT_EVENT_DEX			0x00000002
#define BATT_EXT_EVENT_CALL			0x00000004

#define SEC_BAT_ERROR_CAUSE_NONE		0x0000
#define SEC_BAT_ERROR_CAUSE_FG_INIT_FAIL	0x0001
#define SEC_BAT_ERROR_CAUSE_I2C_FAIL		0xFFFFFFFF

/* monitor activation */
enum sec_battery_polling_time_type {
	/* same order with power supply status */
	SEC_BATTERY_POLLING_TIME_BASIC = 0,
	SEC_BATTERY_POLLING_TIME_CHARGING,
	SEC_BATTERY_POLLING_TIME_DISCHARGING,
	SEC_BATTERY_POLLING_TIME_NOT_CHARGING,
	SEC_BATTERY_POLLING_TIME_SLEEP,
};

/* BATT_INBAT_VOLTAGE */
enum sec_battery_fgsrc_switching {
	SEC_BAT_INBAT_FGSRC_SWITCHING_VBAT = 0,
	SEC_BAT_INBAT_FGSRC_SWITCHING_VSYS,
	SEC_BAT_FGSRC_SWITCHING_VBAT,
 	SEC_BAT_FGSRC_SWITCHING_VSYS
};

enum ta_alert_mode {
	OCP_NONE = 0,
	OCP_DETECT,
	OCP_WA_ACTIVE,
};

enum tx_switch_mode_state {
	TX_SWITCH_MODE_OFF = 0,
	TX_SWITCH_CHG_ONLY,
	TX_SWITCH_UNO_ONLY,
	TX_SWITCH_UNO_FOR_GEAR,
};

/* full check condition type (can be used overlapped) */
#define sec_battery_full_condition_t unsigned int

/* battery check : POWER_SUPPLY_PROP_PRESENT */
enum sec_battery_check {
	/* No Check for internal battery */
	SEC_BATTERY_CHECK_NONE,
	/* by ADC */
	SEC_BATTERY_CHECK_ADC,
	/* by callback function (battery certification by 1 wired)*/
	SEC_BATTERY_CHECK_CALLBACK,
	/* by PMIC */
	SEC_BATTERY_CHECK_PMIC,
	/* by fuel gauge */
	SEC_BATTERY_CHECK_FUELGAUGE,
	/* by charger */
	SEC_BATTERY_CHECK_CHARGER,
	/* by interrupt (use check_battery_callback() to check battery) */
	SEC_BATTERY_CHECK_INT,
#if IS_ENABLED(CONFIG_DUAL_BATTERY)
	/* by dual battery */
	SEC_BATTERY_CHECK_DUAL_BAT_GPIO,
#endif
};
#define sec_battery_check_t \
	enum sec_battery_check

/* cable check (can be used overlapped) */
#define sec_battery_cable_check_t unsigned int

/* check cable source (can be used overlapped) */
#define sec_battery_cable_source_t unsigned int

/* capacity calculation type (can be used overlapped) */
#define sec_fuelgauge_capacity_type_t int

/* SEC_FUELGAUGE_CAPACITY_TYPE_RESET
  * use capacity information to reset fuel gauge
  * (only for driver algorithm, can NOT be set by user)
  */
#define SEC_FUELGAUGE_CAPACITY_TYPE_RESET	(-1)
/* SEC_FUELGAUGE_CAPACITY_TYPE_RAW
  * use capacity information from fuel gauge directly
  */
#define SEC_FUELGAUGE_CAPACITY_TYPE_RAW		0x1
/* SEC_FUELGAUGE_CAPACITY_TYPE_SCALE
  * rescale capacity by scaling, need min and max value for scaling
  */
#define SEC_FUELGAUGE_CAPACITY_TYPE_SCALE	0x2
/* SEC_FUELGAUGE_CAPACITY_TYPE_DYNAMIC_SCALE
  * change only maximum capacity dynamically
  * to keep time for every SOC unit
  */
#define SEC_FUELGAUGE_CAPACITY_TYPE_DYNAMIC_SCALE	0x4
/* SEC_FUELGAUGE_CAPACITY_TYPE_ATOMIC
  * change capacity value by only -1 or +1
  * no sudden change of capacity
  */
#define SEC_FUELGAUGE_CAPACITY_TYPE_ATOMIC	0x8
/* SEC_FUELGAUGE_CAPACITY_TYPE_SKIP_ABNORMAL
  * skip current capacity value
  * if it is abnormal value
  */
#define SEC_FUELGAUGE_CAPACITY_TYPE_SKIP_ABNORMAL	0x10

#define SEC_FUELGAUGE_CAPACITY_TYPE_CAPACITY_POINT	0x20

#define SEC_FUELGAUGE_CAPACITY_TYPE_LOST_SOC	0x40

/* charger function settings (can be used overlapped) */
#define sec_charger_functions_t unsigned int
/* SEC_CHARGER_NO_GRADUAL_CHARGING_CURRENT
 * disable gradual charging current setting
 * SUMMIT:AICL, MAXIM:regulation loop
 */
#define SEC_CHARGER_NO_GRADUAL_CHARGING_CURRENT		1

/* SEC_CHARGER_MINIMUM_SIOP_CHARGING_CURRENT
 * charging current should be over than USB charging current
 */
#define SEC_CHARGER_MINIMUM_SIOP_CHARGING_CURRENT	2

#if defined(CONFIG_BATTERY_AGE_FORECAST)
typedef struct sec_age_data {
	unsigned int cycle;
	unsigned int float_voltage;
	unsigned int recharge_condition_vcell;
	unsigned int full_condition_vcell;
	unsigned int full_condition_soc;
} sec_age_data_t;
#endif

typedef struct {
	unsigned int cycle;
	unsigned int asoc;
} battery_health_condition;

#define is_hv_wireless_type(cable_type) ( \
	cable_type == SEC_BATTERY_CABLE_HV_WIRELESS || \
	cable_type == SEC_BATTERY_CABLE_WIRELESS_HV_STAND || \
	cable_type == SEC_BATTERY_CABLE_HV_WIRELESS_20 || \
	cable_type == SEC_BATTERY_CABLE_HV_WIRELESS_20_LIMIT || \
	cable_type == SEC_BATTERY_CABLE_WIRELESS_HV_VEHICLE || \
	cable_type == SEC_BATTERY_CABLE_WIRELESS_HV_PACK)

#define is_nv_wireless_type(cable_type)	( \
	cable_type == SEC_BATTERY_CABLE_WIRELESS || \
	cable_type == SEC_BATTERY_CABLE_PMA_WIRELESS || \
	cable_type == SEC_BATTERY_CABLE_WIRELESS_PACK || \
	cable_type == SEC_BATTERY_CABLE_WIRELESS_STAND || \
	cable_type == SEC_BATTERY_CABLE_WIRELESS_VEHICLE || \
	cable_type == SEC_BATTERY_CABLE_PREPARE_WIRELESS_HV || \
	cable_type == SEC_BATTERY_CABLE_PREPARE_WIRELESS_20 || \
	cable_type == SEC_BATTERY_CABLE_WIRELESS_TX)

#define is_wireless_type(cable_type) \
	(is_hv_wireless_type(cable_type) || is_nv_wireless_type(cable_type))

#define is_wireless_fake_type(cable_type) \
	(is_wireless_type(cable_type) || (cable_type == SEC_BATTERY_CABLE_WIRELESS_FAKE))

#define is_not_wireless_type(cable_type) ( \
	cable_type != SEC_BATTERY_CABLE_WIRELESS && \
	cable_type != SEC_BATTERY_CABLE_PMA_WIRELESS && \
	cable_type != SEC_BATTERY_CABLE_WIRELESS_PACK && \
	cable_type != SEC_BATTERY_CABLE_WIRELESS_STAND && \
	cable_type != SEC_BATTERY_CABLE_HV_WIRELESS && \
	cable_type != SEC_BATTERY_CABLE_PREPARE_WIRELESS_HV && \
	cable_type != SEC_BATTERY_CABLE_WIRELESS_HV_STAND && \
	cable_type != SEC_BATTERY_CABLE_WIRELESS_VEHICLE && \
	cable_type != SEC_BATTERY_CABLE_WIRELESS_HV_VEHICLE && \
	cable_type != SEC_BATTERY_CABLE_WIRELESS_TX && \
	cable_type != SEC_BATTERY_CABLE_PREPARE_WIRELESS_20 && \
	cable_type != SEC_BATTERY_CABLE_HV_WIRELESS_20 && \
	cable_type != SEC_BATTERY_CABLE_HV_WIRELESS_20_LIMIT && \
	cable_type != SEC_BATTERY_CABLE_WIRELESS_HV_PACK)

#define is_wired_type(cable_type) \
	(is_not_wireless_type(cable_type) && (cable_type != SEC_BATTERY_CABLE_NONE) && \
	(cable_type != SEC_BATTERY_CABLE_OTG))

#define is_hv_qc_wire_type(cable_type) ( \
	cable_type == SEC_BATTERY_CABLE_QC20 || \
	cable_type == SEC_BATTERY_CABLE_QC30)

#define is_hv_afc_wire_type(cable_type) ( \
	cable_type == SEC_BATTERY_CABLE_9V_ERR || \
	cable_type == SEC_BATTERY_CABLE_9V_TA || \
	cable_type == SEC_BATTERY_CABLE_9V_UNKNOWN || \
	cable_type == SEC_BATTERY_CABLE_12V_TA)

#define is_hv_wire_9v_type(cable_type) ( \
	cable_type == SEC_BATTERY_CABLE_9V_ERR || \
	cable_type == SEC_BATTERY_CABLE_9V_TA || \
	cable_type == SEC_BATTERY_CABLE_9V_UNKNOWN || \
	cable_type == SEC_BATTERY_CABLE_QC20)

#define is_hv_wire_12v_type(cable_type) ( \
	cable_type == SEC_BATTERY_CABLE_12V_TA || \
	cable_type == SEC_BATTERY_CABLE_QC30)

#define is_hv_wire_type(cable_type) ( \
	is_hv_afc_wire_type(cable_type) || is_hv_qc_wire_type(cable_type))

#define is_nocharge_type(cable_type) ( \
	cable_type == SEC_BATTERY_CABLE_NONE || \
	cable_type == SEC_BATTERY_CABLE_OTG || \
	cable_type == SEC_BATTERY_CABLE_POWER_SHARING)

#define is_slate_mode(battery) ((battery->current_event & SEC_BAT_CURRENT_EVENT_SLATE) \
		== SEC_BAT_CURRENT_EVENT_SLATE)

#define can_usb_suspend_type(cable_type) ( \
	cable_type == SEC_BATTERY_CABLE_PDIC || \
	cable_type == SEC_BATTERY_CABLE_PDIC_APDO || \
	cable_type == SEC_BATTERY_CABLE_USB || \
	cable_type == SEC_BATTERY_CABLE_USB_CDP)

#define is_pd_wire_type(cable_type) ( \
	cable_type == SEC_BATTERY_CABLE_PDIC || \
	cable_type == SEC_BATTERY_CABLE_PDIC_APDO)

#define is_pd_apdo_wire_type(cable_type) ( \
	cable_type == SEC_BATTERY_CABLE_PDIC_APDO)

#define is_pd_fpdo_wire_type(cable_type) ( \
	cable_type == SEC_BATTERY_CABLE_PDIC)

#define is_hv_pdo_wire_type(cable_type, hv_pdo) ( \
	(cable_type == SEC_BATTERY_CABLE_PDIC || \
	cable_type == SEC_BATTERY_CABLE_PDIC_APDO) && \
	hv_pdo)
#endif /* __SEC_CHARGING_COMMON_H */
