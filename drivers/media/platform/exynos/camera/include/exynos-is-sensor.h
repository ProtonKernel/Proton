/*
 * /include/media/exynos_is_sensor.h
 *
 * Copyright (C) 2012 Samsung Electronics, Co. Ltd
 *
 * Exynos series exynos_is_sensor device support
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#ifndef MEDIA_EXYNOS_SENSOR_H
#define MEDIA_EXYNOS_SENSOR_H

#include <dt-bindings/camera/exynos_is_dt.h>
#include <linux/platform_device.h>

#define IS_SENSOR_DEV_NAME "exynos-is-sensor"
#define IS_PINNAME_LEN 32
#define IS_MAX_NAME_LEN 32

enum exynos_csi_id {
	CSI_ID_A = 0,
	CSI_ID_B = 1,
	CSI_ID_C = 2,
	CSI_ID_D = 3,
	CSI_ID_E = 4,
	CSI_ID_F = 5,
	CSI_ID_MAX
};

enum exynos_sensor_channel {
	SENSOR_CONTROL_I2C0	 = 0,
	SENSOR_CONTROL_I2C1	 = 1,
	SENSOR_CONTROL_I2C2	 = 2,
	SENSOR_CONTROL_I2C3	 = 3,
	SENSOR_CONTROL_I2C4	 = 4,
	SENSOR_CONTROL_I2C5	 = 5,
	SENSOR_CONTROL_I2C6	 = 6,
	SENSOR_CONTROL_I2C7	 = 7,
	SENSOR_CONTROL_I2C8	 = 8,
	SENSOR_CONTROL_I2C9	 = 9,
	SENSOR_CONTROL_I2C10	 = 10,
	SENSOR_CONTROL_I2C11	 = 11,
	SENSOR_CONTROL_I2C12	 = 12,
	SENSOR_CONTROL_I2C13	 = 13,
	SENSOR_CONTROL_I2C14	 = 14,
	SENSOR_CONTROL_I2C_MAX
};

enum exynos_sensor_position {
	/* for the position of real sensors */
	SENSOR_POSITION_REAR		= SP_REAR,
	SENSOR_POSITION_FRONT		= SP_FRONT,
	SENSOR_POSITION_REAR2		= SP_REAR2,
	SENSOR_POSITION_FRONT2		= SP_FRONT2,
	SENSOR_POSITION_REAR3		= SP_REAR3,
	SENSOR_POSITION_FRONT3		= SP_FRONT3,
	SENSOR_POSITION_REAR4		= SP_REAR4,
	SENSOR_POSITION_FRONT4		= SP_FRONT4,
	SENSOR_POSITION_REAR_TOF	= SP_REAR_TOF,
	SENSOR_POSITION_FRONT_TOF	= SP_FRONT_TOF,
	SENSOR_POSITION_MAX,

	/* to characterize the sensor */
	SENSOR_POSITION_SECURE		= SP_SECURE,
	SENSOR_POSITION_VIRTUAL		= SP_VIRTUAL,
};

enum actuator_name {
	ACTUATOR_NAME_AD5823	= 1,
	ACTUATOR_NAME_DWXXXX	= 2,
	ACTUATOR_NAME_AK7343	= 3,
	ACTUATOR_NAME_HYBRIDVCA	= 4,
	ACTUATOR_NAME_LC898212	= 5,
	ACTUATOR_NAME_WV560     = 6,
	ACTUATOR_NAME_AK7345	= 7,
	ACTUATOR_NAME_DW9804	= 8,
	ACTUATOR_NAME_AK7348	= 9,
	ACTUATOR_NAME_SHAF3301	= 10,
	ACTUATOR_NAME_BU64241GWZ = 11,
	ACTUATOR_NAME_AK7371	= 12,
	ACTUATOR_NAME_DW9807	= 13,
	ACTUATOR_NAME_ZC533     = 14,
	ACTUATOR_NAME_BU63165	= 15,
	ACTUATOR_NAME_AK7372    = 16,
	ACTUATOR_NAME_AK7371_DUAL = 17,
	ACTUATOR_NAME_AK737X    = 18,
	ACTUATOR_NAME_DW9780	= 19,
	ACTUATOR_NAME_LC898217	= 20,
	ACTUATOR_NAME_ZC569     = 21,
	ACTUATOR_NAME_DW9823	= 22,
	ACTUATOR_NAME_DW9839	= 23,
	ACTUATOR_NAME_DW9808	= 24,
	ACTUATOR_NAME_ZC535	= 25,
	ACTUATOR_NAME_DW9817	= 26,
	ACTUATOR_NAME_FP5529	= 27,
	ACTUATOR_NAME_DW9800	= 28,
	ACTUATOR_NAME_LC898219	= 29,
	ACTUATOR_NAME_DW9714	= 30,
	ACTUATOR_NAME_BU64981	= 31,
	ACTUATOR_NAME_SEM1215S	= 32,
	ACTUATOR_NAME_END,
	ACTUATOR_NAME_NOTHING	= 100,
};

enum flash_drv_name {
	FLADRV_NAME_KTD267	= 1,	/* Gpio type(Flash mode, Movie/torch mode) */
	FLADRV_NAME_AAT1290A	= 2,
	FLADRV_NAME_MAX77693	= 3,
	FLADRV_NAME_AS3643	= 4,
	FLADRV_NAME_KTD2692	= 5,
	FLADRV_NAME_LM3560	= 6,
	FLADRV_NAME_SKY81296	= 7,
	FLADRV_NAME_RT5033	= 8,
	FLADRV_NAME_AS3647	= 9,
	FLADRV_NAME_LM3646	= 10,
	FLADRV_NAME_DRV_FLASH_GPIO = 11, /* Common Gpio type(Flash mode, Movie/torch mode) */
	FLADRV_NAME_LM3644	= 12,
	FLADRV_NAME_DRV_FLASH_I2C = 13, /* Common I2C type */
	FLADRV_NAME_S2MU106	= 14,
	FLADRV_NAME_OCP8132A	= 15,
	FLADRV_NAME_RT8547	= 16,
	FLADRV_NAME_S2MU107	= 17,
	FLADRV_NAME_END,
	FLADRV_NAME_NOTHING	= 100,
};

enum from_name {
	FROMDRV_NAME_W25Q80BW	= 1,	/* Spi type */
	FROMDRV_NAME_FM25M16A	= 2,	/* Spi type */
	FROMDRV_NAME_FM25M32A	= 3,
	FROMDRV_NAME_NOTHING	= 100,
};

enum preprocessor_name {
	PREPROCESSOR_NAME_73C1 = 1,	/* SPI, I2C, FIMC Lite */
	PREPROCESSOR_NAME_73C2 = 2,
	PREPROCESSOR_NAME_73C3 = 3,
	PREPROCESSOR_NAME_END,
	PREPROCESSOR_NAME_NOTHING = 100,
};

enum ois_name {
	OIS_NAME_RUMBA_S4	= 1,
	OIS_NAME_RUMBA_S6	= 2,
	OIS_NAME_ROHM_BU24218GWL = 3,
	OIS_NAME_SEM1215S	= 4,
	OIS_NAME_END,
	OIS_NAME_NOTHING	= 100,
};

enum mcu_name {
	MCU_NAME_STM32	= 1,
	MCU_NAME_INTERNAL	= 2,
	MCU_NAME_END,
	MCU_NAME_NOTHING	= 100,
};

enum aperture_name {
	APERTURE_NAME_AK7372	= 1,
	APERTURE_NAME_END,
	APERTURE_NAME_NOTHING	= 100,
};

enum eeprom_name {
	EEPROM_NAME_GM1		= 1,
	EEPROM_NAME_5E9		= 2,
	EEPROM_NAME_GW1		= 3,
	EEPROM_NAME_GD1		= 4,
	EEPROM_NAME_HI846	= 5,
	EEPROM_NAME_3M5_TELE	= 6,
	EEPROM_NAME_3M5_FOLD	= 7,
	EEPROM_NAME_GD1_TELE	= 8,
	EEPROM_NAME_GC5035	= 9,
	EEPROM_NAME_OV02A10	= 10,
	EEPROM_NAME_END,
	EEPROM_NAME_NOTHING	= 100,
};

enum laser_af_name {
	LASER_AF_NAME_VL53L5	= 1,
	LASER_AF_NAME_TOF8801	= 2,
	LASER_AF_NAME_VL53L1,
	LASER_AF_NAME_END,
	LASER_AF_NAME_NOTHING	= 100,
};

enum sensor_peri_type {
	SE_NULL		= 0,
	SE_I2C		= 1,
	SE_SPI		= 2,
	SE_GPIO		= 3,
	SE_MPWM		= 4,
	SE_ADC		= 5,
	SE_DMA		= 6,
};

enum sensor_dma_channel_type {
	DMA_CH_NOT_DEFINED	= 100,
};

struct i2c_type {
	u32 channel;
	u32 slave_address;
	u32 speed;
};

struct spi_type {
	u32 channel;
};

struct gpio_type {
	u32 first_gpio_port_no;
	u32 second_gpio_port_no;
};

struct dma_type {
	/*
	 * This value has FIMC_LITE CH or CSIS virtual CH.
	 * If it's not defined in some SOC, it should be DMA_CH_NOT_DEFINED
	 */
	u32 channel;
};

union sensor_peri_format {
	struct i2c_type i2c;
	struct spi_type spi;
	struct gpio_type gpio;
	struct dma_type dma;
};

struct sensor_protocol1 {
	u32 product_name;
	enum sensor_peri_type peri_type;
	union sensor_peri_format peri_setting;
	u32 csi_ch;
	u32 cal_address;
	u32 reserved[2];
};

struct sensor_peri_info {
	bool valid;
	enum sensor_peri_type peri_type;
	union sensor_peri_format peri_setting;
};

struct sensor_protocol2 {
	u32 product_name; /* enum preprocessor_name */
	struct sensor_peri_info peri_info0;
	struct sensor_peri_info peri_info1;
	struct sensor_peri_info peri_info2;
	struct sensor_peri_info reserved[2];
};

struct sensor_open_extended {
	struct sensor_protocol1 sensor_con;
	struct sensor_protocol1 actuator_con;
	struct sensor_protocol1 flash_con;
	struct sensor_protocol1 from_con;
	struct sensor_protocol1 ois_con;
	struct sensor_protocol1 aperture_con;
	struct sensor_protocol1 mcu_con;
	struct sensor_protocol1 eeprom_con;
	struct sensor_protocol1 laser_af_con;
	u32 mclk;
	u32 mipi_lane_num;
	u32 mipi_speed;
	/* Use sensor retention mode */
	u32 use_retention_mode;
	struct sensor_protocol1 reserved[2];
};

struct exynos_platform_is_sensor {
	int (*iclk_get)(struct device *dev, u32 scenario, u32 channel);
	int (*iclk_cfg)(struct device *dev, u32 scenario, u32 channel);
	int (*iclk_on)(struct device *dev,u32 scenario, u32 channel);
	int (*iclk_off)(struct device *dev, u32 scenario, u32 channel);
	int (*mclk_on)(struct device *dev, u32 scenario, u32 channel);
	int (*mclk_off)(struct device *dev, u32 scenario, u32 channel);
	int (*mclk_force_off)(struct device *dev, u32 channel);
	u32 id;
	u32 scenario;
	u32 csi_ch;
	unsigned int wdma_ch_hint;
	unsigned long internal_state;
	u32 csi_mux;
	u32 multi_ch;
	u32 use_cphy;
	u32 use_module_sel;
	u32 module_sel_cnt;
	u32 module_sel_idx;
	int *module_sel_sensor_id;
	int *module_sel_val;
	u32 scramble;
	u32 i2c_dummy_enable;
};

int exynos_is_sensor_iclk_cfg(struct device *dev,
	u32 scenario,
	u32 channel);
int exynos_is_sensor_iclk_on(struct device *dev,
	u32 scenario,
	u32 channel);
int exynos_is_sensor_iclk_off(struct device *dev,
	u32 scenario,
	u32 channel);
int exynos_is_sensor_mclk_on(struct device *dev,
	u32 scenario,
	u32 channel);
int exynos_is_sensor_mclk_off(struct device *dev,
	u32 scenario,
	u32 channel);
int is_sensor_mclk_force_off(struct device *dev, u32 channel);
#endif /* MEDIA_EXYNOS_SENSOR_H */
