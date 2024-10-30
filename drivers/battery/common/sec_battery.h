/*
 * sec_battery.h
 * Samsung Mobile Battery Header
 *
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

#ifndef __SEC_BATTERY_H
#define __SEC_BATTERY_H __FILE__

#include "sec_charging_common.h"
#include <linux/of_gpio.h>
#include <linux/alarmtimer.h>
#include <linux/pm_wakeup.h>
#include <linux/workqueue.h>
#include <linux/proc_fs.h>
#include <linux/jiffies.h>
#include <linux/i2c.h>

#if IS_ENABLED(CONFIG_PDIC_NOTIFIER)
#include <linux/usb/typec/common/pdic_notifier.h>
#endif

#if IS_ENABLED(CONFIG_USB_TYPEC_MANAGER_NOTIFIER)
#include <linux/usb/typec/manager/usb_typec_manager_notifier.h>
#else
#if IS_ENABLED(CONFIG_MUIC_NOTIFIER)
#include <linux/muic/common/muic.h>
#include <linux/muic/common/muic_notifier.h>
#endif
#endif
#include <linux/battery/sec_pd.h>
#if defined(CONFIG_BATTERY_CISD)
#include "sec_cisd.h"
#endif
#if IS_ENABLED(CONFIG_DIRECT_CHARGING)
#include "sec_direct_charger.h"
#endif

#if defined(CONFIG_WIRELESS_AUTH)
#include "sec_battery_misc.h"
#endif

#include "sec_battery_vote.h"
#include "sec_adc.h"
#include "sb_full_soc.h"

extern char *sec_cable_type[];

/* current event */
#define SEC_BAT_CURRENT_EVENT_NONE					0x000000
#define SEC_BAT_CURRENT_EVENT_AFC					0x000001
#define SEC_BAT_CURRENT_EVENT_CHARGE_DISABLE		0x000002
#define SEC_BAT_CURRENT_EVENT_SKIP_HEATING_CONTROL	0x000004
#define SEC_BAT_CURRENT_EVENT_LOW_TEMP_SWELLING_COOL1	0x000008
#define SEC_BAT_CURRENT_EVENT_HIGH_TEMP_SWELLING	0x000020
#define SEC_BAT_CURRENT_EVENT_LOW_TEMP_SWELLING_COOL2	0x000080
#define SEC_BAT_CURRENT_EVENT_LOW_TEMP_SWELLING_COOL3	0x000010
#define SEC_BAT_CURRENT_EVENT_SWELLING_MODE		(SEC_BAT_CURRENT_EVENT_LOW_TEMP_SWELLING_COOL1 | SEC_BAT_CURRENT_EVENT_LOW_TEMP_SWELLING_COOL2 | SEC_BAT_CURRENT_EVENT_HIGH_TEMP_SWELLING | SEC_BAT_CURRENT_EVENT_LOW_TEMP_SWELLING_COOL3)
#define SEC_BAT_CURRENT_EVENT_LOW_TEMP_MODE		(SEC_BAT_CURRENT_EVENT_LOW_TEMP_SWELLING_COOL1 | SEC_BAT_CURRENT_EVENT_LOW_TEMP_SWELLING_COOL2 | SEC_BAT_CURRENT_EVENT_LOW_TEMP_SWELLING_COOL3)
#define SEC_BAT_CURRENT_EVENT_CHG_LIMIT			0x000200
#define SEC_BAT_CURRENT_EVENT_CALL			0x000400
#define SEC_BAT_CURRENT_EVENT_SLATE			0x000800
#define SEC_BAT_CURRENT_EVENT_VBAT_OVP			0x001000
#define SEC_BAT_CURRENT_EVENT_VSYS_OVP			0x002000
#define SEC_BAT_CURRENT_EVENT_WPC_VOUT_LOCK		0x004000
#define SEC_BAT_CURRENT_EVENT_AICL			0x008000
#define SEC_BAT_CURRENT_EVENT_HV_DISABLE		0x010000
#define SEC_BAT_CURRENT_EVENT_SELECT_PDO		0x020000
#define SEC_BAT_CURRENT_EVENT_FG_RESET			0x040000
#define SEC_BAT_CURRENT_EVENT_WDT_EXPIRED		0x080000
#define SEC_BAT_CURRENT_EVENT_SAFETY_TMR		0x100000
#define SEC_BAT_CURRENT_EVENT_ISDB			0x200000
#define SEC_BAT_CURRENT_EVENT_DC_ERR			0x400000
#define SEC_BAT_CURRENT_EVENT_SIOP_LIMIT		0x800000
#define SEC_BAT_CURRENT_EVENT_TEMP_CTRL_TEST		0x1000000
#define SEC_BAT_CURRENT_EVENT_25W_OCP			0x2000000
#define SEC_BAT_CURRENT_EVENT_AFC_DISABLE		0x4000000
#define SEC_BAT_CURRENT_EVENT_SEND_UVDM			0x8000000

#define SEC_BAT_CURRENT_EVENT_USB_SUSPENDED		0x10000000
#define SEC_BAT_CURRENT_EVENT_USB_SUPER			0x20000000
#define SEC_BAT_CURRENT_EVENT_USB_100MA			0x40000000
#define SEC_BAT_CURRENT_EVENT_USB_STATE		(SEC_BAT_CURRENT_EVENT_USB_SUSPENDED |\
						SEC_BAT_CURRENT_EVENT_USB_SUPER |\
						SEC_BAT_CURRENT_EVENT_USB_100MA)
#define SEC_BAT_CURRENT_EVENT_WPC_EN		0x80000000

/* misc_event */
#define BATT_MISC_EVENT_UNDEFINED_RANGE_TYPE		0x00000001
#define BATT_MISC_EVENT_WIRELESS_BACKPACK_TYPE		0x00000002
#define BATT_MISC_EVENT_TIMEOUT_OPEN_TYPE		0x00000004
#define BATT_MISC_EVENT_BATT_RESET_SOC			0x00000008
#define BATT_MISC_EVENT_WATER_HICCUP_TYPE		0x00000020
#define BATT_MISC_EVENT_WIRELESS_DET_LEVEL		0x00000040
#define BATT_MISC_EVENT_WIRELESS_FOD			0x00000100
#define BATT_MISC_EVENT_WIRELESS_AUTH_START		0x00000200
#define BATT_MISC_EVENT_WIRELESS_AUTH_RECVED		0x00000400
#define BATT_MISC_EVENT_WIRELESS_AUTH_FAIL		0x00000800
#define BATT_MISC_EVENT_WIRELESS_AUTH_PASS		0x00001000
#define BATT_MISC_EVENT_TEMP_HICCUP_TYPE		0x00002000
#define BATT_MISC_EVENT_BATTERY_HEALTH			0x000F0000
#define BATT_MISC_EVENT_HEALTH_OVERHEATLIMIT		0x00100000
#define BATT_MISC_EVENT_ABNORMAL_PAD		0x00200000
#define BATT_MISC_EVENT_WIRELESS_MISALIGN		0x00400000
#define BATT_MISC_EVENT_FULL_CAPACITY		0x01000000

#define BATTERY_HEALTH_SHIFT	16
enum misc_battery_health {
	BATTERY_HEALTH_UNKNOWN = 0,
	BATTERY_HEALTH_GOOD,
	BATTERY_HEALTH_NORMAL,
	BATTERY_HEALTH_AGED,
	BATTERY_HEALTH_MAX = BATTERY_HEALTH_AGED,

	/* For event */
	BATTERY_HEALTH_BAD = 0xF,
};
#define BATT_MISC_EVENT_MUIC_ABNORMAL	(BATT_MISC_EVENT_UNDEFINED_RANGE_TYPE |\
					BATT_MISC_EVENT_WATER_HICCUP_TYPE |\
					BATT_MISC_EVENT_TEMP_HICCUP_TYPE)

#define ADC_CH_COUNT		10
#define ADC_SAMPLE_COUNT	10

#define DEFAULT_HEALTH_CHECK_COUNT	5
#define TEMP_HIGHLIMIT_DEFAULT	2000

#define SIOP_INPUT_LIMIT_CURRENT                1200
#define SIOP_CHARGING_LIMIT_CURRENT             1800
#define SIOP_WIRELESS_INPUT_LIMIT_CURRENT       600
#define SIOP_HV_WIRELESS_INPUT_LIMIT_CURRENT	700
#define SIOP_STORE_HV_WIRELESS_CHARGING_LIMIT_CURRENT	450
#define SIOP_HV_INPUT_LIMIT_CURRENT			700
#define SIOP_HV_CHARGING_LIMIT_CURRENT			1800
#define SIOP_HV_12V_INPUT_LIMIT_CURRENT			535
#define SIOP_HV_12V_CHARGING_LIMIT_CURRENT		1000
#define SIOP_APDO_INPUT_LIMIT_CURRENT				1000
#define SIOP_APDO_CHARGING_LIMIT_CURRENT			2000

#define FOREACH_VOTER(GENERATE) \
	GENERATE(VOTER_VBUS_CHANGE)   \
	GENERATE(VOTER_USB_100MA)	\
	GENERATE(VOTER_CHG_LIMIT)	\
	GENERATE(VOTER_AICL)	\
	GENERATE(VOTER_SELECT_PDO)	\
	GENERATE(VOTER_CABLE)	\
	GENERATE(VOTER_MIX_LIMIT)	\
	GENERATE(VOTER_CHG_TEMP)	\
	GENERATE(VOTER_LRP_TEMP)	\
	GENERATE(VOTER_STORE_MODE)	\
	GENERATE(VOTER_SIOP)	\
	GENERATE(VOTER_WPC_CUR)	\
	GENERATE(VOTER_SWELLING)	\
	GENERATE(VOTER_OTG)		\
	GENERATE(VOTER_SLEEP_MODE)	\
	GENERATE(VOTER_USER)	\
	GENERATE(VOTER_STEP)	\
	GENERATE(VOTER_AGING_STEP)	\
	GENERATE(VOTER_VBUS_OVP)	\
	GENERATE(VOTER_FULL_CHARGE)	\
	GENERATE(VOTER_TEST_MODE)	\
	GENERATE(VOTER_TIME_EXPIRED)	\
	GENERATE(VOTER_MUIC_ABNORMAL)	\
	GENERATE(VOTER_WC_TX)	\
	GENERATE(VOTER_SLATE)	\
	GENERATE(VOTER_SMART_SLATE)	\
	GENERATE(VOTER_SUSPEND)	\
	GENERATE(VOTER_SYSOVLO)	\
	GENERATE(VOTER_VBAT_OVP)	\
	GENERATE(VOTER_STEP_CHARGE)	\
	GENERATE(VOTER_TOPOFF_CHANGE)	\
	GENERATE(VOTER_HMT)	\
	GENERATE(VOTER_DC_ERR)	\
	GENERATE(VOTER_DC_MODE)	\
	GENERATE(VOTER_FULL_CAPACITY)	\
	GENERATE(VOTER_WDT_EXPIRE)	\
	GENERATE(VOTER_BATTERY)	\
	GENERATE(VOTER_CHANGE_CHGMODE)	\
	GENERATE(VOTER_FW)	\
	GENERATE(VOTER_MAX)

#define GENERATE_ENUM(ENUM) ENUM,
#define GENERATE_STRING(STRING) #STRING,

enum VOTER_ENUM {
		FOREACH_VOTER(GENERATE_ENUM)
};
#define WIRELESS_OTG_INPUT_CURRENT 900

enum {
	SEC_INPUT_VOLTAGE_0V = 0,
	SEC_INPUT_VOLTAGE_NONE = 1000,
	SEC_INPUT_VOLTAGE_APDO = 1234,
	SEC_INPUT_VOLTAGE_5V = 5000,
	SEC_INPUT_VOLTAGE_5_5V = 5500,
	SEC_INPUT_VOLTAGE_9V = 9000,
	SEC_INPUT_VOLTAGE_10V = 10000,
	SEC_INPUT_VOLTAGE_12V = 12000,
	SEC_INPUT_VOLTAGE_12_5V = 12500,
};

#define HV_CHARGER_STATUS_STANDARD1	12000 /* mW */
#define HV_CHARGER_STATUS_STANDARD2	20000 /* mW */
#define HV_CHARGER_STATUS_STANDARD3 24500 /* mW */
#define HV_CHARGER_STATUS_STANDARD4 40000 /* mW */

#define mW_by_mVmA(v, a)	((v) * (a) / 1000)
#define mV_by_mWmA(w, a)	((a) ? (((w) * 1000) / (a)) : (0))
#define mA_by_mWmV(w, v)	((v) ? (((w) * 1000) / (v)) : (0))

enum battery_misc_test {
	MISC_TEST_RESET = 0,
	MISC_TEST_DISPLAY,
	MISC_TEST_MAX,
};

enum {
	NORMAL_TA,
	AFC_9V_OR_15W,
	AFC_12V_OR_20W,
	SFC_25W,
	SFC_45W,
};

struct sec_bat_pdic_info {
	unsigned int pdo_index;
	bool apdo;
	unsigned int max_voltage;
	unsigned int min_voltage;
	unsigned int max_current;
	unsigned int comm_capable;
	unsigned int suspend;
};

struct sec_bat_pdic_list {
	struct sec_bat_pdic_info pd_info[MAX_PDO_NUM + 1]; /* 5V ~ 12V */
	unsigned int max_pd_count;
	bool now_isApdo;
	unsigned int num_fpdo;
	unsigned int num_apdo;
};

enum {
	USB_THM_NORMAL = 0,
	USB_THM_OVERHEATLIMIT,
	USB_THM_GAP_OVER,
};

struct adc_sample_info {
	unsigned int cnt;
	int total_adc;
	int average_adc;
	int adc_arr[ADC_SAMPLE_COUNT];
	int index;
};

typedef struct sec_charging_current {
	unsigned int input_current_limit;
	unsigned int fast_charging_current;
#if IS_ENABLED(CONFIG_DUAL_BATTERY)
	unsigned int main_limiter_current;
	unsigned int sub_limiter_current;
#endif
} sec_charging_current_t;

/**
 * struct sec_bat_adc_table_data - adc to temperature table for sec battery
 * driver
 * @adc: adc value
 * @temperature: temperature(C) * 10
 */
typedef struct sec_bat_adc_table_data {
	int adc;
	int data;
} sec_bat_adc_table_data_t;

typedef struct sec_bat_adc_region {
	int min;
	int max;
} sec_bat_adc_region_t;

/* nv, hv, dc, wpc, wpc_hv */
enum sec_siop_curr_type {
	SIOP_CURR_TYPE_NV = 0,
	SIOP_CURR_TYPE_HV,
	SIOP_CURR_TYPE_FPDO,
	SIOP_CURR_TYPE_APDO,
	SIOP_CURR_TYPE_WPC_NV,
	SIOP_CURR_TYPE_WPC_HV,
	SIOP_CURR_TYPE_MAX,
};

struct sec_siop_table {
	int level;
	int icl[SIOP_CURR_TYPE_MAX];
	int fcc[SIOP_CURR_TYPE_MAX];
};

#define SIOP_SCENARIO_NUM_MAX		10

#if defined(CONFIG_TABLET_MODEL_CONCEPT) && !defined(CONFIG_SEC_FACTORY)
#define SLOW_CHARGING_CURRENT_STANDARD          1000
#else
#define SLOW_CHARGING_CURRENT_STANDARD          400
#endif

struct sec_wireless_rx_power_info {
	unsigned int vout;
	unsigned int input_current_limit;
	unsigned int fast_charging_current;
	unsigned int ttf_charge_current;
	unsigned int wireless_power_class;
	unsigned int rx_power;
};

#define sec_wireless_rx_power_info_t \
	struct sec_wireless_rx_power_info

/* LRP structure */
#define LRP_PROPS 12
#define FOREACH_LRP_TYPE(GEN_LRP_TYPE) \
	GEN_LRP_TYPE(LRP_NORMAL) \
	GEN_LRP_TYPE(LRP_25W) \
	GEN_LRP_TYPE(LRP_45W) \
	GEN_LRP_TYPE(LRP_MAX)

#define GENERATE_LRP_ENUM(ENUM) ENUM,
#define GENERATE_LRP_STRING(STRING) #STRING,

enum LRP_TYPE_ENUM {
	FOREACH_LRP_TYPE(GENERATE_LRP_ENUM)
};

static const char * const LRP_TYPE_STRING[] = {
	FOREACH_LRP_TYPE(GENERATE_LRP_STRING)
};

enum {
	LRP_NONE = 0,
	LRP_STEP1,
	LRP_STEP2,
};

enum {
	ST1 = 0,
	ST2,
};

enum {
	LCD_ON = 0,
	LCD_OFF,
};

struct lrp_temp_t {
	int trig[2][2];
	int recov[2][2];
};

struct lrp_current_t {
	int st_icl[2];
	int st_fcc[2];
};

typedef struct sec_battery_platform_data {
	/* NO NEED TO BE CHANGED */
	/* callback functions */
	void (*initial_check)(void);
	void (*monitor_additional_check)(void);
	bool (*bat_gpio_init)(void);
	bool (*fg_gpio_init)(void);
	bool (*is_lpm)(void);
	bool (*check_jig_status)(void);
	bool (*is_interrupt_cable_check_possible)(int);
	int (*check_cable_callback)(void);
	int (*get_cable_from_extended_cable_type)(int);
	bool (*cable_switch_check)(void);
	bool (*cable_switch_normal)(void);
	bool (*check_cable_result_callback)(int);
	bool (*check_battery_callback)(void);
	bool (*check_battery_result_callback)(void);
	int (*ovp_uvlo_callback)(void);
	bool (*ovp_uvlo_result_callback)(int);
	bool (*fuelalert_process)(bool);
	bool (*get_temperature_callback)(
			enum power_supply_property,
			union power_supply_propval*);

	/* ADC region by power supply type
	 * ADC region should be exclusive
	 */
	sec_bat_adc_region_t *cable_adc_value;
	/* charging current for type (0: not use) */
	sec_charging_current_t *charging_current;
	sec_wireless_rx_power_info_t *wireless_power_info;
	unsigned int *polling_time;
	char *chip_vendor;
	/* NO NEED TO BE CHANGED */
	unsigned int pre_afc_input_current;
	unsigned int pre_wc_afc_input_current;
	unsigned int select_pd_input_current;
	unsigned int store_mode_max_input_power;
	unsigned int prepare_ta_delay;

	char *pmic_name;

	/* battery */
	char *vendor;
	int technology;
	int battery_type;
	void *battery_data;

	int bat_gpio_ta_nconnected;
	/* 1 : active high, 0 : active low */
	int bat_polarity_ta_nconnected;
	int bat_irq;
	int bat_irq_gpio; /* BATT_INT(BAT_ID detecting) */
	unsigned long bat_irq_attr;
	sec_battery_cable_check_t cable_check_type;
	sec_battery_cable_source_t cable_source_type;

	unsigned int swelling_high_rechg_voltage;
	unsigned int swelling_low_rechg_voltage;

#if defined(CONFIG_STEP_CHARGING)
	/* step charging */
	unsigned int **step_chg_cond;
#if IS_ENABLED(CONFIG_DUAL_BATTERY)
	unsigned int **step_chg_cond_vsub;
#endif
	unsigned int *step_chg_cond_curr;
	unsigned int **step_chg_curr;
	unsigned int **step_chg_vfloat;
#if IS_ENABLED(CONFIG_DIRECT_CHARGING)
	unsigned int dc_step_chg_cond_v_margin;
	unsigned int **dc_step_chg_cond_vol;
	unsigned int **dc_step_chg_cond_soc;
	unsigned int *dc_step_chg_cond_iin;
	int dc_step_chg_iin_check_cnt;

	unsigned int **dc_step_chg_val_iout;
	unsigned int **dc_step_chg_val_vfloat;
#endif
#endif

	/* Monitor setting */
	int polling_type;
	/* for initial check */
	unsigned int monitor_initial_count;

	/* Battery check */
	sec_battery_check_t battery_check_type;
	/* how many times do we need to check battery */
	unsigned int check_count;
	/* ADC */
	/* battery check ADC maximum value */
	unsigned int check_adc_max;
	/* battery check ADC minimum value */
	unsigned int check_adc_min;

	/* OVP/UVLO check */
	int ovp_uvlo_check_type;

	int thermal_source;
	int usb_thermal_source; /* To confirm the usb temperature */
	int chg_thermal_source; /* To confirm the charger temperature */
	int wpc_thermal_source; /* To confirm the wpc temperature */
	int sub_chg_thermal_source; /* To confirm the sub chg charger temperature */
	int sub_bat_thermal_source; /* To confirm the sub battery temperature */
#if IS_ENABLED(CONFIG_DIRECT_CHARGING)
	int dchg_thermal_source; /* To confirm the charger temperature */
	bool dctp_by_cgtp;
#endif
	int blkt_thermal_source; /* To confirm the blanket temperature */

	/*
	 * inbat_adc_table
	 * in-battery voltage check for table models:
	 * To read real battery voltage with Jig cable attached,
	 * dedicated hw pin & conversion table of adc-voltage are required
	 */
	sec_bat_adc_table_data_t *temp_adc_table;
	sec_bat_adc_table_data_t *temp_amb_adc_table;
	sec_bat_adc_table_data_t *usb_temp_adc_table;
	sec_bat_adc_table_data_t *chg_temp_adc_table;
	sec_bat_adc_table_data_t *wpc_temp_adc_table;
	sec_bat_adc_table_data_t *sub_chg_temp_adc_table;
	sec_bat_adc_table_data_t *inbat_adc_table;
	sec_bat_adc_table_data_t *sub_bat_temp_adc_table;
#if IS_ENABLED(CONFIG_DIRECT_CHARGING)
	sec_bat_adc_table_data_t *dchg_temp_adc_table;
#endif
	sec_bat_adc_table_data_t *blkt_temp_adc_table;

	unsigned int temp_adc_table_size;
	unsigned int temp_amb_adc_table_size;
	unsigned int usb_temp_adc_table_size;
	unsigned int chg_temp_adc_table_size;
	unsigned int wpc_temp_adc_table_size;
	unsigned int sub_chg_temp_adc_table_size;
	unsigned int inbat_adc_table_size;
	unsigned int sub_bat_temp_adc_table_size;
#if IS_ENABLED(CONFIG_DIRECT_CHARGING)
	unsigned int dchg_temp_adc_table_size;
#endif
	unsigned int blkt_temp_adc_table_size;

	int temp_check_type;
	int lrp_temp_check_type;
	unsigned int temp_check_count;
	int usb_temp_check_type;
	int usb_temp_check_type_backup; /* sec_bat_set_temp_control_test() */
	int chg_temp_check_type;
	int wpc_temp_check_type;
	int sub_chg_temp_check_type;
	unsigned int sub_bat_temp_check_type;
#if IS_ENABLED(CONFIG_DIRECT_CHARGING)
	int dchg_temp_check_type;
#endif
	int blkt_temp_check_type;

	unsigned int inbat_voltage;

	/*
	 * limit can be ADC value or Temperature
	 * depending on temp_check_type
	 * temperature should be temp x 10 (0.1 degree)
	 */
	int wireless_cold_cool3_thresh;
	int wireless_cool3_cool2_thresh;
	int wireless_cool2_cool1_thresh;
	int wireless_cool1_normal_thresh;
	int wireless_normal_warm_thresh;
	int wireless_warm_overheat_thresh;

	int wire_cold_cool3_thresh;
	int wire_cool3_cool2_thresh;
	int wire_cool2_cool1_thresh;
	int wire_cool1_normal_thresh;
	int wire_normal_warm_thresh;
	int wire_warm_overheat_thresh;

	int wire_warm_current;
	int wire_cool1_current;
	int wire_cool2_current;
	int wire_cool3_current;
	int wireless_warm_current;
	int wireless_cool1_current;
	int wireless_cool2_current;
	int wireless_cool3_current;
	int high_temp_float;
	int low_temp_float;

#if IS_ENABLED(CONFIG_DUAL_BATTERY)
	unsigned int limiter_main_warm_current;
	unsigned int limiter_sub_warm_current;
	unsigned int limiter_main_cool1_current;
	unsigned int limiter_sub_cool1_current;
	unsigned int limiter_main_cool2_current;
	unsigned int limiter_sub_cool2_current;
	unsigned int limiter_main_cool3_current;
	unsigned int limiter_sub_cool3_current;
#endif

	int buck_recovery_margin;

	int tx_high_threshold;
	int tx_high_recovery;
	int tx_low_threshold;
	int tx_low_recovery;
	int chg_12v_high_temp;
	int chg_high_temp;
	int chg_high_temp_recovery;
	int dchg_high_temp[4];
	int dchg_high_temp_recovery[4];
	int dchg_high_batt_temp[4];
	int dchg_high_batt_temp_recovery[4];

	struct lrp_temp_t lrp_temp[LRP_MAX];
	struct lrp_current_t lrp_curr[LRP_MAX];

	unsigned int chg_charging_limit_current;
	unsigned int chg_input_limit_current;
	unsigned int dchg_charging_limit_current;
	unsigned int dchg_input_limit_current;
	unsigned int wpc_temp_control_source;
	unsigned int wpc_temp_lcd_on_control_source;
	int wpc_high_temp;
	int wpc_high_temp_recovery;
	unsigned int wpc_input_limit_current;
	unsigned int wpc_charging_limit_current;

	unsigned int wpc_step_limit_size;
	unsigned int *wpc_step_limit_temp;
	unsigned int *wpc_step_limit_fcc;
	unsigned int *wpc_step_limit_fcc_12w;
	unsigned int *wpc_step_limit_fcc_15w;

	int wpc_lcd_on_high_temp;
	int wpc_lcd_on_high_temp_rec;
	unsigned int wpc_lcd_on_input_limit_current;
	unsigned int sleep_mode_limit_current;
	unsigned int wc_full_input_limit_current;
	unsigned int max_charging_current;
	unsigned int max_charging_charge_power;
	unsigned int apdo_max_volt;
	int mix_high_temp;
	int mix_high_chg_temp;
	int mix_high_temp_recovery;
	unsigned int charging_limit_by_tx_check; /* check limited charging current during wireless power sharing with cable charging */
	unsigned int charging_limit_current_by_tx;
	unsigned int charging_limit_current_by_tx_gear;
	unsigned int wpc_input_limit_by_tx_check; /* check limited wpc input current with tx device */
	unsigned int wpc_input_limit_current_by_tx;
	int non_wc20_wpc_high_temp;
	int non_wc20_wpc_high_temp_recovery;

	/* If these is NOT full check type or NONE full check type,
	 * it is skipped
	 */
	/* 1st full check */
	int full_check_type;
	/* 2nd full check */
	int full_check_type_2nd;
	unsigned int full_check_count;
	int chg_gpio_full_check;
	/* 1 : active high, 0 : active low */
	int chg_polarity_full_check;
	bool chg_vbus_control_after_fullcharged;
	sec_battery_full_condition_t full_condition_type;
	unsigned int full_condition_soc;
	unsigned int full_condition_vcell;
	unsigned int full_condition_avgvcell;
	unsigned int full_condition_ocv;

	unsigned int recharge_check_count;
	sec_battery_recharge_condition_t recharge_condition_type;
	unsigned int recharge_condition_soc;
	unsigned int recharge_condition_avgvcell;
	unsigned int recharge_condition_vcell;

	/* for absolute timer (second) */
	unsigned long charging_total_time;
	/* for recharging timer (second) */
	unsigned long recharging_total_time;
	/* reset charging for abnormal malfunction (0: not use) */
	unsigned long charging_reset_time;
	unsigned int hv_charging_total_time;
	unsigned int normal_charging_total_time;
	unsigned int usb_charging_total_time;

	/* fuel gauge */
	char *fuelgauge_name;

	unsigned int store_mode_charging_max;
	unsigned int store_mode_charging_min;
	unsigned int store_mode_buckoff;
	/* charger */
	char *charger_name;
	char *otg_name;
	char *fgsrc_switch_name;
	bool support_fgsrc_change;
	bool slowcharging_usb_bootcomplete;

	/* wireless charger */
	char *wireless_charger_name;
	int wireless_cc_cv;

	/* float voltage (mV) */
	unsigned int chg_float_voltage;
	unsigned int chg_float_voltage_conv;

#if IS_ENABLED(CONFIG_DUAL_BATTERY)
	/* current limiter */
	char *dual_battery_name;
	char *main_limiter_name;
	char *sub_limiter_name;
	bool support_dual_battery;
	int main_bat_enb_gpio;
	int main_bat_enb2_gpio;
	int sub_bat_enb_gpio;
#endif

#if defined(CONFIG_DUAL_BATTERY_CELL_SENSING)
	unsigned int main_cell_margin_cc;
	unsigned int main_cell_margin_cv;
	unsigned int main_cell_sensing;
	unsigned int sub_cell_sensing;
#endif

#if defined(CONFIG_BATTERY_AGE_FORECAST)
	int num_age_step;
	int age_step;
	int age_data_length;
	sec_age_data_t* age_data;
#endif
	battery_health_condition* health_condition;

	int siop_icl;
	int siop_fcc;
	int siop_hv_icl;
	int siop_hv_icl_2nd;
	int siop_hv_fcc;
	int siop_hv_12v_icl;
	int siop_hv_12v_fcc;
	int siop_apdo_icl;
	int siop_apdo_fcc;

	int siop_wpc_icl;
	int *siop_wpc_fcc;
	int siop_hv_wpc_icl;
	int *siop_hv_wpc_fcc;
	int wireless_otg_input_current;
	int wc_hero_stand_cc_cv;
	int wc_hero_stand_cv_current;
	int wc_hero_stand_hv_cv_current;

	int siop_scenarios_num;
	int siop_curr_type_num;
	struct sec_siop_table siop_table[SIOP_SCENARIO_NUM_MAX];

	int default_input_current;
	int default_charging_current;
	int default_usb_input_current;
	int default_usb_charging_current;
	unsigned int default_wc20_input_current;
	unsigned int default_wc20_charging_current;
	int max_input_voltage;
	int max_input_current;
	int pre_afc_work_delay;
	int pre_wc_afc_work_delay;

	unsigned int rp_current_rp1;
	unsigned int rp_current_rp2;
	unsigned int rp_current_rp3;
	unsigned int rp_current_rdu_rp3;
	unsigned int rp_current_abnormal_rp3;

	sec_charger_functions_t chg_functions_setting;

	bool fake_capacity;
	bool dis_auto_shipmode_temp_ctrl;
	bool tx_5v_disable;
	unsigned int phm_vout_ctrl_dev;

	/* tx power sharging */
	unsigned int tx_stop_capacity;

	unsigned int battery_full_capacity;
#if defined(CONFIG_BATTERY_CISD)
	unsigned int cisd_cap_high_thr;
	unsigned int cisd_cap_low_thr;
	unsigned int cisd_cap_limit;
	unsigned int max_voltage_thr;
	unsigned int cisd_alg_index;
	unsigned int *ignore_cisd_index;
	unsigned int *ignore_cisd_index_d;
#endif

#if IS_ENABLED(CONFIG_DUAL_BATTERY)
	/* main + sub value should be over 110% */
	unsigned int main_current_rate;
	unsigned int sub_current_rate;
	/* zone 1 : 0C ~ 0.4C */
	unsigned int main_zone1_current_rate;
	unsigned int sub_zone1_current_rate;
	/* zone 2 : 0.4C ~ 1.1C */
	unsigned int main_zone2_current_rate;
	unsigned int sub_zone2_current_rate;
	/* zone 3 : 1.1C ~ MAX */
	unsigned int main_zone3_current_rate;
	unsigned int sub_zone3_current_rate;
	unsigned int force_recharge_margin;
	unsigned int max_main_limiter_current;
	unsigned int min_main_limiter_current;
	unsigned int max_sub_limiter_current;
	unsigned int min_sub_limiter_current;
#endif

	/* ADC setting */
	unsigned int adc_check_count;

	unsigned int full_check_current_1st;
	unsigned int full_check_current_2nd;

	unsigned int pd_charging_charge_power;
	unsigned int nv_charge_power;

	unsigned int expired_time;
	unsigned int recharging_expired_time;
	int standard_curr;

	unsigned int tx_minduty_5V;
	unsigned int tx_minduty_default;

	unsigned int tx_uno_vout;
	unsigned int tx_uno_iout;
	unsigned int tx_gear_vout;
	unsigned int tx_mfc_iout_gear;
	unsigned int tx_mfc_iout_phone;
	unsigned int tx_mfc_iout_phone_5v;
	unsigned int tx_mfc_iout_lcd_on;

	unsigned int tx_aov_start_vout;
	unsigned int tx_aov_freq_low;
	unsigned int tx_aov_freq_high;
	unsigned int tx_aov_delay;
	unsigned int tx_aov_delay_phm_escape;

	/* Sub_THM LRPST */
	bool lr_sub_enable;
	unsigned int lr_param_bat_thm;
	unsigned int lr_param_sub_bat_thm;
	unsigned int lr_delta;
	unsigned int lr_param_init_bat_thm;
	unsigned int lr_param_init_sub_bat_thm;
	unsigned int lr_round_off;

	bool cs100_jpn;

	/* ADC type for each channel */
	unsigned int adc_type[];
} sec_battery_platform_data_t;

struct sec_ttf_data;

struct sec_battery_info {
	struct device *dev;
	sec_battery_platform_data_t *pdata;
	struct sec_ttf_data *ttf_d;

	/* power supply used in Android */
	struct power_supply *psy_bat;
	struct power_supply *psy_usb;
	struct power_supply *psy_ac;
	struct power_supply *psy_wireless;
	struct power_supply *psy_pogo;
	struct power_supply *psy_otg;
	unsigned int irq;

	int pd_usb_attached;
#if IS_ENABLED(CONFIG_USB_TYPEC_MANAGER_NOTIFIER)
	struct notifier_block usb_typec_nb;
#else
#if IS_ENABLED(CONFIG_MUIC_NOTIFIER)
	struct notifier_block batt_nb;
#endif
#endif

	bool pdic_attach;
	bool pdic_ps_rdy;
	bool init_src_cap;
	SEC_PD_SINK_STATUS sink_status;
	SEC_PD_SINK_STATUS *psink_status;
	struct sec_bat_pdic_list pd_list;

	bool hv_pdo;
	bool update_pd_list;
	unsigned int now_pd_index;
	unsigned int target_pd_index;

	bool is_sysovlo;
	bool is_vbatovlo;
	bool is_abnormal_temp;

	bool safety_timer_set;
	bool lcd_status;
	bool skip_swelling;
	bool wc_auth_retried;

	int status;
	int health;
	bool present;
	unsigned int charger_mode;

	int voltage_now;		/* cell voltage (mV) */
	int voltage_avg;		/* average voltage (mV) */
	int voltage_ocv;		/* open circuit voltage (mV) */
	int current_now;		/* current (mA) */
	int inbat_adc;                  /* inbat adc */
	int current_avg;		/* average current (mA) */
	int current_max;		/* input current limit (mA) */
	int current_sys;		/* system current (mA) */
	int current_sys_avg;		/* average system current (mA) */
	int charge_counter;		/* remaining capacity (uAh) */
	int current_adc;

#if IS_ENABLED(CONFIG_DUAL_BATTERY)
	int voltage_pack_main;		/* pack voltage main battery (mV) */
	int voltage_pack_sub;		/* pack voltage sub battery (mV) */
	int current_now_main;		/* current from main battery (mA) */
	int current_now_sub;		/* current from sub battery (mA) */

	unsigned int limiter_check;
#if defined(CONFIG_DUAL_BATTERY_CELL_SENSING)
	int voltage_cell_main;		/* cell voltage main battery (mV) */
	int voltage_cell_sub;		/* cell voltage sub battery (mV) */
#endif
#endif

	unsigned int capacity;			/* SOC (%) */
	unsigned int input_voltage;		/* CHGIN/WCIN input voltage (V) */
	unsigned int charge_power;		/* charge power (mW) */
	unsigned int max_charge_power;		/* max charge power (mW) */
	unsigned int pd_max_charge_power;		/* max charge power for pd (mW) */

	struct mutex adclock;
	struct adc_sample_info	adc_sample[ADC_CH_COUNT];

	/* keep awake until monitor is done */
	struct wakeup_source *monitor_ws;
	struct workqueue_struct *monitor_wqueue;
	struct delayed_work monitor_work;
	unsigned int polling_count;
	unsigned int polling_time;
	bool polling_in_sleep;
	bool polling_short;

	struct delayed_work polling_work;
	struct alarm polling_alarm;
	ktime_t last_poll_time;

#if defined(CONFIG_BATTERY_CISD)
	struct cisd cisd;
	bool skip_cisd;
	bool usb_overheat_check;
	int prev_volt;
	int prev_temp;
	int prev_jig_on;
	int enable_update_data;
	int prev_chg_on;
#endif

#if defined(CONFIG_WIRELESS_AUTH)
	sec_bat_misc_dev_t *misc_dev;
#endif

	/* battery check */
	unsigned int check_count;
	/* ADC check */
	unsigned int check_adc_count;
	unsigned int check_adc_value;

	/* health change check*/
	bool health_change;
	/* ovp-uvlo health check */
	int health_check_count;

	/* time check */
	unsigned long charging_start_time;
	unsigned long charging_passed_time;
	unsigned long charging_next_time;
	unsigned long charging_fullcharged_time;

	unsigned long wc_heating_start_time;
	unsigned long wc_heating_passed_time;
	unsigned int wc_heat_limit;

	/* chg temperature check */
	unsigned int chg_limit;
	unsigned int chg_limit_recovery_cable;
	unsigned int mix_limit;

	/* lrp temperature check */
	unsigned int lrp_limit;
	unsigned int lrp_step;

	/* temperature check */
	int temperature;	/* battery temperature */
#if defined(CONFIG_ENG_BATTERY_CONCEPT)
	bool test_max_current;
	bool test_charge_current;
#if defined(CONFIG_STEP_CHARGING)
	int test_step_condition;
#endif
#endif
	int temperature_test_battery;
	int temperature_test_usb;
	int temperature_test_wpc;
	int temperature_test_chg;
	int temperature_test_dchg;
	int temperature_test_blkt;
	int temperature_test_sub;

	int temper_amb;		/* target temperature */
	int usb_temp;
	int chg_temp;		/* charger temperature */
	int wpc_temp;
	int coil_temp;
	int sub_chg_temp;
	int sub_bat_temp;
	int usb_thm_status;
#if defined(CONFIG_PREVENT_USB_CONN_OVERHEAT)
	int usb_protection_temp;
	int temp_gap_bat_usb;
#endif
	int dchg_temp;
	int blkt_temp;		/* blanket temperature(instead of batt temp in mix_temp func for tablet model) */

	int lrp;
	int lrp_test;

	int temp_adc;
	int temp_ambient_adc;
	int usb_temp_adc;
	int chg_temp_adc;
	int wpc_temp_adc;
	int coil_temp_adc;
	int sub_chg_temp_adc;
	int sub_bat_temp_adc;
#if IS_ENABLED(CONFIG_DIRECT_CHARGING)
	int dchg_temp_adc;
#endif
	int blkt_temp_adc;

	int overheatlimit_threshold_backup; /* sec_bat_set_temp_control_test() */
	int overheatlimit_recovery_backup; /* sec_bat_set_temp_control_test() */
	int overheatlimit_threshold;
	int overheatlimit_recovery;
	int cold_cool3_thresh;
	int cool3_cool2_thresh;
	int cool2_cool1_thresh;
	int cool1_normal_thresh;
	int normal_warm_thresh;
	int warm_overheat_thresh;
	int thermal_zone;
	int bat_thm_count;
	int adc_init_count;

	/* charging */
	unsigned int charging_mode;
	bool is_recharging;
	int wdt_kick_disable;

	bool is_jig_on;
	int cable_type;
	int muic_cable_type;
	int extended_cable_type;

	bool auto_mode;

	struct wakeup_source *cable_ws;
	struct delayed_work cable_work;
	struct wakeup_source *vbus_ws;
	struct delayed_work siop_work;
	struct wakeup_source *input_ws;
	struct delayed_work input_check_work;
#if defined(CONFIG_WIRELESS_FIRMWARE_UPDATE)
	struct delayed_work update_work;
	struct delayed_work fw_init_work;
#endif
	struct delayed_work siop_level_work;
	struct wakeup_source *siop_level_ws;
	struct wakeup_source *wpc_tx_ws;
	struct delayed_work wpc_tx_work;
	struct wakeup_source *wpc_tx_en_ws;
	struct delayed_work wpc_tx_en_work;
#if defined(CONFIG_UPDATE_BATTERY_DATA)
	struct delayed_work batt_data_work;
	struct wakeup_source *batt_data_ws;
	char *data_path;
#endif
#ifdef CONFIG_OF
	struct delayed_work parse_mode_dt_work;
	struct wakeup_source *parse_mode_dt_ws;
#endif
	struct delayed_work otg_work;

	char batt_type[48];
	unsigned int full_check_cnt;
	unsigned int recharge_check_cnt;

	unsigned int input_check_cnt;

	struct mutex iolock;
	int input_current;
	int charging_current;
#if IS_ENABLED(CONFIG_DUAL_BATTERY)
	unsigned int main_current;
	unsigned int sub_current;
#endif
	int topoff_condition;
	int wpc_vout_level;
	int wpc_max_vout_level;
	unsigned int current_event;

	/* wireless charging enable */
	struct mutex wclock;
	bool wc_enable;
	int wc_enable_cnt;
	int wc_enable_cnt_value;
	int led_cover;
	int wc_status;
	int wc_tx_freq;
	bool wc_cv_mode;
	bool wc_pack_max_curr;
	bool wc_rx_phm_mode;
	bool wc_tx_phm_mode;
	bool prev_tx_phm_mode;
	bool wc_tx_adaptive_vout;
	bool wc_found_gear_freq;
	bool wc_need_ldo_on;

	int wire_status;

	/* wireless tx */
	bool wc_tx_enable;
	bool wc_rx_connected;
	bool wc_tx_chg_limit;
	bool afc_disable;
	bool pd_disable;
	bool buck_cntl_by_tx;
	bool tx_switch_mode_change;
	int wc_tx_vout;
	bool uno_en;
	unsigned int wc_rx_type;
	unsigned int tx_minduty;
	unsigned int tx_switch_mode;
	unsigned int tx_switch_start_soc;

	unsigned int tx_mfc_iout;
	unsigned int tx_uno_iout;

	int pogo_status;

	/* test mode */
	int test_mode;
	bool factory_mode;
	bool display_test;
	bool store_mode;

	/* usb suspend */
	int prev_usb_conf;

	/* MTBF test for CMCC */
	bool is_hc_usb;

	int siop_level;
	int siop_prev_event;
	int stability_test;
	int eng_not_full_status;

	bool skip_chg_temp_check;
	bool skip_wpc_temp_check;
	bool wpc_temp_mode;
	bool wpc_vout_ctrl_lcd_on;
	char *hv_chg_name;
#if IS_ENABLED(CONFIG_WIRELESS_CHARGING)
	int tx_avg_curr;
	int tx_time_cnt;
	int tx_total_power;
	int tx_total_power_cisd;
	bool tx_clear;
	bool tx_clear_cisd;
	struct delayed_work wpc_txpower_calc_work;

	unsigned int wc20_vout;
	unsigned int wc20_power_class;
	unsigned int wc20_rx_power;
	struct delayed_work wc20_current_work;
	struct wakeup_source *wc20_current_ws;
#endif
	struct delayed_work slowcharging_work;
#if defined(CONFIG_BATTERY_AGE_FORECAST)
	int batt_cycle;
#endif
	int batt_asoc;
#if defined(CONFIG_STEP_CHARGING)
	bool step_charging_skip_lcd_on;
	bool step_chg_en_in_factory;
	unsigned int step_chg_type;
	unsigned int step_chg_charge_power;
	int step_chg_status;
	int step_chg_step;
#if IS_ENABLED(CONFIG_DIRECT_CHARGING)
	int dc_step_chg_step;
	unsigned int dc_step_chg_type;
	unsigned int dc_step_chg_charge_power;

	bool dc_float_voltage_set;
	unsigned int dc_step_chg_iin_cnt;
#endif
#endif
	struct mutex misclock;
	struct mutex txeventlock;
	unsigned int misc_event;
	unsigned int tx_event;
	unsigned int ext_event;
	unsigned int prev_misc_event;
	unsigned int tx_retry_case;
	unsigned int tx_misalign_cnt;
	unsigned int tx_ocp_cnt;
	struct delayed_work ext_event_work;
	struct delayed_work misc_event_work;
	struct wakeup_source *ext_event_ws;
	struct wakeup_source *misc_event_ws;
	struct wakeup_source *tx_event_ws;
	struct mutex batt_handlelock;
	struct mutex current_eventlock;
	struct mutex typec_notylock;
	struct mutex voutlock;
	unsigned long tx_misalign_start_time;
	unsigned long tx_misalign_passed_time;
	unsigned long tx_ocp_start_time;
	unsigned long tx_ocp_passed_time;

	unsigned int hiccup_status;
	bool hiccup_clear;

	bool stop_timer;
	unsigned long prev_safety_time;
	unsigned long expired_time;
	unsigned long cal_safety_time;
	int fg_reset;

	struct sec_vote * fcc_vote;
	struct sec_vote * input_vote;
	struct sec_vote * fv_vote;
	struct sec_vote * chgen_vote;
	struct sec_vote * topoff_vote;
	struct sec_vote *iv_vote;

	struct sb_full_soc *fs;

	/* 25w ta alert */
	bool ta_alert_wa;
	int ta_alert_mode;

	bool sleep_mode;
	bool mfc_fw_update;

	int charging_night_mode;
	int batt_full_capacity;

	/* Sub_THM LRPST */
	unsigned long lr_start_time;
	unsigned long lr_time_span;
	int lrp_sub;
	int lr_sub_bat_t_1;
	bool usb_slow_chg;
	bool usb_bootcomplete;
};

/* event check */
#define EVENT_NONE				(0)
#define EVENT_2G_CALL			(0x1 << 0)
#define EVENT_3G_CALL			(0x1 << 1)
#define EVENT_MUSIC				(0x1 << 2)
#define EVENT_VIDEO				(0x1 << 3)
#define EVENT_BROWSER			(0x1 << 4)
#define EVENT_HOTSPOT			(0x1 << 5)
#define EVENT_CAMERA			(0x1 << 6)
#define EVENT_CAMCORDER			(0x1 << 7)
#define EVENT_DATA_CALL			(0x1 << 8)
#define EVENT_WIFI				(0x1 << 9)
#define EVENT_WIBRO				(0x1 << 10)
#define EVENT_LTE				(0x1 << 11)
#define EVENT_LCD			(0x1 << 12)
#define EVENT_GPS			(0x1 << 13)

enum {
	EXT_DEV_NONE = 0,
	EXT_DEV_GAMEPAD_CHG,
	EXT_DEV_GAMEPAD_OTG,
};

extern unsigned int sec_bat_get_lpmode(void);
extern void sec_bat_set_lpmode(unsigned int value);
extern int sec_bat_get_fgreset(void);
extern int sec_bat_get_facmode(void);
extern unsigned int sec_bat_get_chgmode(void);
extern void sec_bat_set_chgmode(unsigned int value);
extern unsigned int sec_bat_get_dispd(void);
extern void sec_bat_set_dispd(unsigned int value);

extern int adc_read(struct sec_battery_info *battery, int channel);
extern void adc_init(struct platform_device *pdev, struct sec_battery_info *battery);
extern void adc_exit(struct sec_battery_info *battery);
extern void sec_cable_init(struct platform_device *pdev, struct sec_battery_info *battery);
extern int sec_bat_get_adc_data(struct sec_battery_info *battery, int adc_ch, int count);
extern int sec_bat_get_charger_type_adc(struct sec_battery_info *battery);
extern bool sec_bat_get_value_by_adc(struct sec_battery_info *battery,
	enum sec_battery_adc_channel channel, union power_supply_propval *value, int check_type);
extern int sec_bat_get_adc_value(struct sec_battery_info *battery, int channel);
extern int sec_bat_get_inbat_vol_by_adc(struct sec_battery_info *battery);
extern bool sec_bat_check_vf_adc(struct sec_battery_info *battery);
#if IS_ENABLED(CONFIG_DIRECT_CHARGING)
extern int sec_bat_get_direct_chg_temp_adc(struct sec_battery_info *battery, int adc_data, int count);
#endif
extern void sec_bat_set_misc_event(struct sec_battery_info *battery, unsigned int misc_event_val, unsigned int misc_event_mask);
extern void sec_bat_set_tx_event(struct sec_battery_info *battery,
	unsigned int tx_event_val, unsigned int tx_event_mask);
extern void sec_bat_set_current_event(struct sec_battery_info *battery, unsigned int current_event_val, unsigned int current_event_mask);
extern void sec_bat_set_temp_control_test(struct sec_battery_info *battery, bool temp_enable);
extern void sec_bat_get_battery_info(struct sec_battery_info *battery);
extern int sec_bat_set_charging_current(struct sec_battery_info *battery);
extern void sec_bat_aging_check(struct sec_battery_info *battery);

extern void sec_bat_set_threshold(struct sec_battery_info *battery, int cable_type);
extern void sec_bat_thermal_check(struct sec_battery_info *battery);
extern void sec_bat_set_charging_status(struct sec_battery_info *battery, int status);
void sec_bat_set_health(struct sec_battery_info *battery, int status);
extern bool sec_bat_check_full(struct sec_battery_info *battery, int full_check_type);
extern bool sec_bat_check_fullcharged(struct sec_battery_info *battery);
extern void sec_bat_check_wpc_temp(struct sec_battery_info *battery);
extern void sec_bat_check_mix_temp(struct sec_battery_info *battery);
extern void sec_bat_check_afc_temp(struct sec_battery_info *battery);
extern void sec_bat_check_pdic_temp(struct sec_battery_info *battery);
extern void sec_bat_check_direct_chg_temp(struct sec_battery_info *battery);
extern void sec_bat_check_lrp_temp(struct sec_battery_info *battery, int ct, int ws, int siop_level, bool lcd_sts);
extern void sec_bat_check_tx_temperature(struct sec_battery_info *battery);
extern void sec_bat_change_default_current(struct sec_battery_info *battery, int cable_type, int input, int output);
extern int sec_bat_set_charge(void *data, int chg_mode);
extern int adjust_sub_bat_temp(struct sec_battery_info *battery, int sub_bat_temp);
extern int get_chg_power_type(int ct, int ws, int pd_max_pw, int max_pw);
#if IS_ENABLED(CONFIG_WIRELESS_CHARGING)
extern void sec_bat_get_wireless_current(struct sec_battery_info *battery);
extern void sec_bat_mfc_work(struct work_struct *work);
extern int sec_bat_check_wc_available(struct sec_battery_info *battery);
extern bool sec_bat_hv_wc_normal_mode_check(struct sec_battery_info *battery);
extern void sec_bat_ext_event_work_content(struct sec_battery_info *battery);
extern void sec_bat_wpc_tx_work_content(struct sec_battery_info *battery);
extern void sec_bat_wpc_tx_en_work_content(struct sec_battery_info *battery);
extern void sec_bat_set_wireless20_current(struct sec_battery_info *battery, int rx_power);
extern void sec_wireless_otg_control(struct sec_battery_info *battery, int enable);
extern void set_wireless_otg_input_current(struct sec_battery_info *battery);
extern void sec_bat_set_decrease_iout(struct sec_battery_info *battery, bool last_delay);
extern void sec_bat_set_mfc_off(struct sec_battery_info *battery, bool need_ept);
extern void sec_bat_set_mfc_on(struct sec_battery_info *battery);
extern int sec_bat_choose_cable_type(struct sec_battery_info *battery);
extern void sec_bat_handle_tx_misalign(struct sec_battery_info *battery, bool trigger_misalign);
extern void sec_bat_handle_tx_ocp(struct sec_battery_info *battery, bool trigger_ocp);
extern void sec_bat_wireless_minduty_cntl(struct sec_battery_info *battery, unsigned int duty_val);
extern void sec_bat_wireless_uno_cntl(struct sec_battery_info *battery, bool en);
extern void sec_bat_wireless_iout_cntl(struct sec_battery_info *battery, int uno_iout, int mfc_iout);
extern void sec_bat_wireless_vout_cntl(struct sec_battery_info *battery, int vout_now);
extern void sec_bat_check_tx_mode(struct sec_battery_info *battery);
extern void sec_bat_wc_cv_mode_check(struct sec_battery_info *battery);
extern void sec_bat_run_wpc_tx_work(struct sec_battery_info *battery, int work_delay);
extern void sec_bat_txpower_calc(struct sec_battery_info *battery);
extern void sec_wireless_set_tx_enable(struct sec_battery_info *battery, bool wc_tx_enable);
extern void sec_bat_set_forced_tx_switch_mode(struct sec_battery_info *battery, int mode);
extern void sec_bat_check_wc_re_auth(struct sec_battery_info *battery);
#endif

#if defined(CONFIG_WIRELESS_FIRMWARE_UPDATE)
extern void sec_bat_fw_update(struct sec_battery_info *battery, int mode);
extern bool sec_bat_check_boost_mfc_condition(struct sec_battery_info *battery, int mode);
#endif

#if defined(CONFIG_STEP_CHARGING)
extern void sec_bat_reset_step_charging(struct sec_battery_info *battery);
extern void sec_step_charging_init(struct sec_battery_info *battery, struct device *dev);
extern bool sec_bat_check_step_charging(struct sec_battery_info *battery);
#if IS_ENABLED(CONFIG_DIRECT_CHARGING)
extern bool sec_bat_check_dc_step_charging(struct sec_battery_info *battery);
#endif
#if defined(CONFIG_BATTERY_AGE_FORECAST)
void sec_bat_set_aging_info_step_charging(struct sec_battery_info *battery);
#endif
#endif

#if !defined(CONFIG_SEC_FACTORY)
#if !defined(CONFIG_ARCH_EXYNOS)
bool sales_code_is(char *str);
#endif
#endif

#if defined(CONFIG_UPDATE_BATTERY_DATA)
extern int sec_battery_update_data(const char* file_path);
#endif
#if defined(CONFIG_BATTERY_CISD)
extern bool sec_bat_cisd_check(struct sec_battery_info *battery);
extern void sec_battery_cisd_init(struct sec_battery_info *battery);
extern void set_cisd_pad_data(struct sec_battery_info *battery, const char* buf);
extern void set_cisd_power_data(struct sec_battery_info *battery, const char* buf);
extern void set_cisd_pd_data(struct sec_battery_info *battery, const char *buf);
#endif

#if defined(CONFIG_WIRELESS_AUTH)
extern int sec_bat_misc_init(struct sec_battery_info *battery);
#endif

int sec_bat_parse_dt(struct device *dev, struct sec_battery_info *battery);
void sec_bat_parse_mode_dt(struct sec_battery_info *battery);
void sec_bat_parse_mode_dt_work(struct work_struct *work);
#if defined(CONFIG_BATTERY_AGE_FORECAST)
void sec_bat_check_battery_health(struct sec_battery_info *battery);
#endif
bool sec_bat_hv_wc_normal_mode_check(struct sec_battery_info *battery);
int sec_bat_get_dctp_info(struct sec_battery_info *battery);

#endif /* __SEC_BATTERY_H */
