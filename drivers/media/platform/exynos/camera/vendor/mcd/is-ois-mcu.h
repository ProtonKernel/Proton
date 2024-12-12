/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Samsung Exynos SoC series Pablo driver
 *
 * Copyright (c) 2019 Samsung Electronics Co., Ltd
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#ifndef IS_OIS_MCU_H
#define IS_OIS_MCU_H

#define	IS_MCU_FW_NAME		"is_mcu_fw.bin"
#define	IS_MCU_PATH		"/system/vendor/firmware/"
#ifdef CONFIG_SAMSUNG_PRODUCT_SHIP
#define IS_MCU_SDCARD_PATH	NULL
#else
#define IS_MCU_SDCARD_PATH	"/vendor/firmware/"
#endif
#define	GYRO_CAL_VALUE_FROM_EFS	"/efs/FactoryApp/camera_ois_gyro_cal"
#define	MAX_GYRO_EFS_DATA_LENGTH	30
#define	MAX_HALL_SHIFT_DATA_LENGTH	800
#define	OIS_GYRO_SCALE_FACTOR_LSM6DSO	114
#define	FW_DRIVER_IC			0
#define	FW_GYRO_SENSOR			1
#define	FW_MODULE_TYPE		2
#define	FW_PROJECT				3
#define	FW_CORE_VERSION		4
#define	OIS_BIN_LEN				45056
#define	MCU_AF_MODE_STANDBY			0x40
#define	MCU_AF_MODE_ACTIVE			0x00
#define	MCU_AF_INIT_POSITION		0x7F
#define	MCU_ACT_POS_SIZE_BIT		ACTUATOR_POS_SIZE_12BIT
#define	MCU_ACT_POS_MAX_SIZE		((1 <<MCU_ACT_POS_SIZE_BIT) - 1)
#define	MCU_ACT_POS_DIRECTION		ACTUATOR_RANGE_INF_TO_MAC
#define	MCU_ACT_DEFAULT_FIRST_POSITION		2048
#define	MCU_ACT_DEFAULT_FIRST_DELAY		2000
#define	MCU_SHARED_SRC_ON_COUNT		1
#define	MCU_SHARED_SRC_OFF_COUNT		0
#define	MCU_HALL_SHIFT_ADDR_X_M2	0x02AA
#define 	MCU_HALL_SHIFT_ADDR_Y_M2	0x02AC
#define 	MCU_HALL_SHIFT_VALUE_FROM_EFS	"/efs/FactoryApp/camera_tilt_calibration_info_3_1"
#define	MCU_BYPASS_MODE_WRITE_ID	0x48
#define	MCU_BYPASS_MODE_READ_ID	0x49
#define	MCU_HW_VERSION_OFFSET		0x987C
#define	MCU_BIN_VERSION_OFFSET		0x98F8

enum is_ois_power_mode {
	OIS_POWER_MODE_SINGLE_WIDE = 0,
	OIS_POWER_MODE_SINGLE_TELE,
	OIS_POWER_MODE_SINGLE_TELE2,
	OIS_POWER_MODE_DUAL,
	OIS_POWER_MODE_TRIPLE,
};

struct mcu_default_data {
	u32 ois_gyro_list[5];
	u32 ois_gyro_list_len;
	u32 aperture_delay_list[2];
	u32 aperture_delay_list_len;
};

enum ois_mcu_state {
	OM_HW_NONE,
	OM_HW_FW_LOADED,
	OM_HW_RUN,
	OM_HW_SUSPENDED,
	OM_HW_END
};

enum ois_mcu_base_reg_index {
	OM_REG_CORE = 0,
	OM_REG_PERI1 = 1,
	OM_REG_PERI2 = 2,
	OM_REG_PERI_SETTING = 3,
	OM_REG_MAX
};

struct is_common_mcu_info {
	int ois_gyro_direction[8];
};

struct ois_mcu_dev {
	struct platform_device	*pdev;
	struct device		*dev;
	struct clk		*clk;
	struct clk		*spi_clk;
	struct mutex 	power_mutex;
	int			irq;
	void __iomem		*regs[OM_REG_MAX];
	resource_size_t		regs_start[OM_REG_MAX];
	resource_size_t		regs_end[OM_REG_MAX];

	unsigned long		state;
	atomic_t 		shared_rsc_count;
	int			current_rsc_count;
	bool			dev_ctrl_state;
	bool			need_reset_mcu;
	bool			is_mcu_active;
	bool			recover_tele2;
	bool			is_tele2_on;
};

enum is_efs_state {
	IS_EFS_STATE_READ,
};

struct mcu_efs_info {
	unsigned long	efs_state;
	s16 ois_hall_shift_x;
	s16 ois_hall_shift_y;
};

/*
 * APIs
 */
int ois_mcu_power_ctrl(struct ois_mcu_dev *mcu, int on);
int ois_mcu_load_binary(struct ois_mcu_dev *mcu);
int ois_mcu_core_ctrl(struct ois_mcu_dev *mcu, int on);
int ois_mcu_dump(struct ois_mcu_dev *mcu, int type);
void ois_mcu_device_ctrl(struct ois_mcu_dev *mcu);
void is_get_common_mcu_info(struct is_common_mcu_info **mcuinfo);

/*
 * log
 */
extern int debug_ois_mcu;

#define __mcu_err(fmt, ...)	pr_err(fmt, ##__VA_ARGS__)
#define __mcu_warning(fmt, ...)	pr_warn(fmt, ##__VA_ARGS__)
#define __mcu_log(fmt, ...)	pr_info(fmt, ##__VA_ARGS__)

#define err_mcu(fmt, args...) \
	__mcu_err("[@][OIS_MCU]" fmt, ##args)

#define warning_mcu(fmt, args...) \
	__mcu_warning("[@][OIS_MCU]" fmt, ##args)

#define info_mcu(fmt, args...) \
	__mcu_log("[@][OIS_MCU]" fmt, ##args)

#define dbg_mcu(level, fmt, args...)			\
	do {						\
		if (unlikely(debug_ois_mcu >= level))	\
			__mcu_log("[@][OIS_MCU]" fmt, ##args);	\
	} while (0)

#endif
