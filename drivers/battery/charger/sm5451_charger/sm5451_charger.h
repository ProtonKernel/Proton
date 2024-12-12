/*
 * sm5451_charger.h - SM5451 Charger device driver for SAMSUNG platform
 *
 * Copyright (C) 2020 SiliconMitus Co.Ltd
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#include "sm5451_direct_charger.h"

#ifndef __SM5451_CHARGER_H__
#define __SM5451_CHARGER_H__

#define SM5451_TA_MIN_CURRENT   1000
#define SM5451_CV_OFFSET        0
#define SM5451_CI_OFFSET        300

enum SM5451_flag1_desc {
    SM5451_FLAG1_VBUSPD         = 1 << 5,
    SM5451_FLAG1_VDSQRB         = 1 << 4,
    SM5451_FLAG1_VBUSOVP        = 1 << 3,
    SM5451_FLAG1_IBUSOCP        = 1 << 2,
    SM5451_FLAG1_CHGON          = 1 << 1,
    SM5451_FLAG1_IBUSUCP        = 1 << 0,
};

enum SM5451_flag2_desc {
    SM5451_FLAG2_VBATOVP        = 1 << 7,
    SM5451_FLAG2_IBATOCP        = 1 << 6,
    SM5451_FLAG2_VBATREG        = 1 << 5,
    SM5451_FLAG2_IBATREG        = 1 << 4,
    SM5451_FLAG2_TSD            = 1 << 3,
    SM5451_FLAG2_RLTVUVP        = 1 << 2,
    SM5451_FLAG2_RLTVOVP        = 1 << 1,
    SM5451_FLAG2_CNSHTP         = 1 << 0,
};

enum SM5451_flag3_desc {
    SM5451_FLAG3_VBUSPOK        = 1 << 7,
    SM5451_FLAG3_VOUTPOK        = 1 << 6,
    SM5451_FLAG3_WTDTMR         = 1 << 5,
    SM5451_FLAG3_VBUSUVLO       = 1 << 3,
    SM5451_FLAG3_CHGONTMR       = 1 << 2,
    SM5451_FLAG3_ADCDONE        = 1 << 1,
    SM5451_FLAG3_CFLYSHTP       = 1 << 0,
};

enum SM5451_flag4_desc {
    SM5451_FLAG4_IBUSOCP_RVS    = 1 << 5,
    SM5451_FLAG4_RVSRDY         = 1 << 4,
    SM5451_FLAG4_THEMP          = 1 << 3,
    SM5451_FLAG4_IBUSREG        = 1 << 2,
    SM5451_FLAG4_TOPOFF         = 1 << 1,
    SM5451_FLAG4_DONE           = 1 << 0,
};

enum SM5451_reg_addr {
    SM5451_REG_CNTL1            = 0x00,
    SM5451_REG_CNTL2            = 0x01,
    SM5451_REG_CNTL3            = 0x02,
    SM5451_REG_DEVICE_ID        = 0x03,
    SM5451_REG_CNTL4            = 0x05,
    SM5451_REG_VBUSOVP          = 0x06,
    SM5451_REG_IBUS_PROT        = 0x07,
    SM5451_REG_VBAT_OVP         = 0x08,
    SM5451_REG_IBAT_OCP         = 0x09,
    SM5451_REG_REG1             = 0x0A,
    SM5451_REG_FLAG1            = 0x0B,
    SM5451_REG_FLAGMSK1         = 0x0C,
    SM5451_REG_FLAG2            = 0x0D,
    SM5451_REG_FLAGMSK2         = 0x0E,
    SM5451_REG_FLAG3            = 0x0F,
    SM5451_REG_FLAGMSK3         = 0x10,
    SM5451_REG_ADC_CNTL         = 0x11,
    SM5451_REG_VBUS_ADC_H       = 0x12,
    SM5451_REG_VBUS_ADC_L       = 0x13,
    SM5451_REG_IBUS_ADC_H       = 0x14,
    SM5451_REG_IBUS_ADC_L       = 0x15,
    SM5451_REG_VBAT_ADC_H       = 0x16,
    SM5451_REG_VBAT_ADC_L       = 0x17,
    SM5451_REG_IBAT_ADC_H       = 0x18,
    SM5451_REG_IBAT_ADC_L       = 0x19,
    SM5451_REG_TDIE_ADC         = 0x1A,
    SM5451_REG_THEM             = 0x60,
    SM5451_REG_CNTL5            = 0x61,
    SM5451_REG_TOPOFF           = 0x62,
    SM5451_REG_FLAG4            = 0x63,
    SM5451_REG_FLAGMSK4         = 0x64,
    SM5451_REG_THEM_ADC_H       = 0x65,
    SM5451_REG_THEM_ADC_L       = 0x66,
    SM5451_REG_IBATOCP_DG       = 0x92,
    SM5451_REG_VDSQRB_DG		= 0x95,
    SM5451_REG_PRECHG_MODE      = 0xA0,
	SM5451_REG_CTRL_STM_0       = 0xBA,
	SM5451_REG_CTRL_STM_1       = 0xBB,
	SM5451_REG_CTRL_STM_2       = 0xBC,
	SM5451_REG_CTRL_STM_3       = 0xBD,
	SM5451_REG_CTRL_STM_5       = 0xBF,
};

enum SM5451_vbatovp_offset {
    SM5451_VBATOVP_50   = 0x0,
    SM5451_VBATOVP_100  = 0x1,
    SM5451_VBATOVP_150  = 0x2,
    SM5451_VBATOVP_200  = 0x3,
};

enum SM5451_ibatocp_offset {
    SM5451_IBATOCP_200  = 0x0,
    SM5451_IBATOCP_300  = 0x1,
    SM5451_IBATOCP_400  = 0x2,
    SM5451_IBATOCP_500  = 0x3,
};

enum SM5451_ibusocp_offset {
    SM5451_IBUSOCP_100  = 0x0,
    SM5451_IBUSOCP_200  = 0x1,
    SM5451_IBUSOCP_300  = 0x2,
    SM5451_IBUSOCP_400  = 0x3,
};

enum SM5451_adc_channel {
    SM5451_ADC_THEM     = 0x0,
    SM5451_ADC_TDIE,
    SM5451_ADC_VBUS,
    SM5451_ADC_IBUS,
    SM5451_ADC_VBAT,
    SM5451_ADC_IBAT,
};

enum SM5451_wdt_tmr {
    WDT_TIMER_S_0P5     = 0x0,
    WDT_TIMER_S_1       = 0x1,
    WDT_TIMER_S_2       = 0x2,
    WDT_TIMER_S_5       = 0x3,
    WDT_TIMER_S_10      = 0x4,
    WDT_TIMER_S_20      = 0x5,
    WDT_TIMER_S_40      = 0x6,
    WDT_TIMER_S_80      = 0x7,
};

enum SM5451_op_mode {
    OP_MODE_INIT        = 0x0,
    OP_MODE_FW_BYPASS   = 0x1,
    OP_MODE_FW_BOOST    = 0x2,
    OP_MODE_REV_BYPASS  = 0x3,
    OP_MODE_REV_BOOST   = 0x4,
};

struct sm5451_platform_data {
    u8 rev_id;
    int irq_gpio;
    u32 pps_lr;
    u32 rpcm;

    struct {
        u32 chg_float_voltage;
        char *sec_dc_name;
    }battery;
};

struct sm5451_charger {
    struct device *dev;
    struct i2c_client *i2c;
    struct sm5451_platform_data *pdata;
    struct power_supply	*psy_chg;
    struct sm_dc_info *pps_dc;
    int ps_type;

    struct mutex i2c_lock;
    struct mutex pd_lock;
	struct wakeup_source *chg_ws;

	int irq;
	int cable_online;
	bool vbus_in;
	bool rev_boost;
	u32 max_vbat;
	u32 target_vbat;
	u32 target_ibus;
	u32 target_ibat;

	bool wdt_disable;

    /* debug */
    struct dentry *debug_root;
    u32 debug_address;
    int addr;
    int size;
};

#endif  /* __SM5451_CHARGER_H__ */
