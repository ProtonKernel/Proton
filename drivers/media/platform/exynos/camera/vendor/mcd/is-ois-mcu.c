// SPDX-License-Identifier: GPL-2.0
/*
 * Samsung Exynos SoC series Pablo driver
 *
 * Exynos Pablo image subsystem functions
 *
 * Copyright (c) 2019 Samsung Electronics Co., Ltd
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/fs.h>
#include <linux/slab.h>
#include <linux/init.h>
#include <linux/list.h>
#include <linux/platform_device.h>
#include <linux/spi/spi.h>
#include <linux/vmalloc.h>
#include <linux/firmware.h>
#include <linux/i2c.h>
#include <linux/gpio.h>
#include <linux/of.h>
#include <linux/of_gpio.h>
#include <linux/io.h>
#include <linux/clk.h>
#include <linux/platform_device.h>
#include <linux/regulator/consumer.h>
#include <linux/delay.h>
#include <linux/file.h>
#include <soc/samsung/exynos-pmu-if.h>
#if defined(CONFIG_USE_OIS_TAMODE_CONTROL)
#include <linux/power_supply.h>
#endif

#include <exynos-is-sensor.h>
#include "is-device-sensor-peri.h"
#include "is-hw-api-common.h"
#include "is-hw-api-ois-mcu.h"
#include "is-ois-mcu.h"
#include "is-device-ois.h"
#include "is-sfr-ois-mcu-v1_1_1.h"
#ifdef CONFIG_AF_HOST_CONTROL
#include "is-device-af.h"
#endif
#if defined(CONFIG_USE_OIS_TAMODE_CONTROL)
#include "is-vender.h"
#endif
#include "is-vender-specific.h"
#include "is-sec-define.h"

int debug_ois_mcu;
module_param(debug_ois_mcu, int, 0644);

static const struct v4l2_subdev_ops subdev_ops;
static bool ois_wide_init;
static bool ois_tele_init;
static bool ois_tele2_init;
static bool ois_hw_check;
static bool ois_fadeupdown;
#if defined(CONFIG_USE_OIS_TAMODE_CONTROL)
static bool ois_tamode_onoff;
static bool ois_tamode_status;
#endif
static u16 ois_center_x;
static u16 ois_center_y;
static struct is_common_mcu_info common_mcu_infos;
#ifndef OIS_DUAL_CAL_DEFAULT_VALUE_TELE
static struct mcu_efs_info efs_info;
#endif
extern struct is_sysfs_actuator sysfs_actuator;
bool mcu_support_oldhw = false;

void is_get_common_mcu_info(struct is_common_mcu_info **mcuinfo)
{
	*mcuinfo = &common_mcu_infos;
}
EXPORT_SYMBOL_GPL(is_get_common_mcu_info);

static int ois_mcu_clk_get(struct ois_mcu_dev *mcu)
{
	mcu->clk = clk_get(mcu->dev, "user_mux");
	mcu->spi_clk = clk_get(mcu->dev, "ipclk_spi");
	if (!IS_ERR(mcu->clk) && !IS_ERR(mcu->spi_clk))
		return 0;
	else
		goto err;

err:
	if (PTR_ERR(mcu->clk) != -ENOENT) {
		dev_err(mcu->dev, "Failed to get 'user_mux' clock: %ld",
			PTR_ERR(mcu->clk));
		return PTR_ERR(mcu->clk);
	}
	dev_info(mcu->dev, "[@] 'user_mux' clock is not present\n");

	if (PTR_ERR(mcu->spi_clk) != -ENOENT) {
		dev_err(mcu->dev, "Failed to get 'spiclk' clock: %ld",
			PTR_ERR(mcu->spi_clk));
		return PTR_ERR(mcu->spi_clk);
	}
	dev_info(mcu->dev, "[@] 'spiclk' clock is not present\n");

	return -EIO;
}

static void ois_mcu_clk_put(struct ois_mcu_dev *mcu)
{
	if (!IS_ERR(mcu->clk))
		clk_put(mcu->clk);

	if (!IS_ERR(mcu->spi_clk))
		clk_put(mcu->spi_clk);
}

static int ois_mcu_clk_enable(struct ois_mcu_dev *mcu)
{
	int ret = 0;

	if (IS_ERR(mcu->clk)) {
		dev_info(mcu->dev, "[@] 'user_mux' clock is not present\n");
		return -EIO;
	}

	ret = clk_prepare_enable(mcu->clk);
	if (ret) {
		dev_err(mcu->dev, "%s: failed to enable clk (err %d)\n",
					__func__, ret);
		return ret;
	}

	if (IS_ERR(mcu->spi_clk)) {
		dev_info(mcu->dev, "[@] 'spi_clk' clock is not present\n");
		return -EIO;
	}

	/* set spi clock to 10Mhz */
	clk_set_rate(mcu->spi_clk, 26000000);
	ret = clk_prepare_enable(mcu->spi_clk);
	if (ret) {
		dev_err(mcu->dev, "%s: failed to enable clk (err %d)\n",
					__func__, ret);
		return ret;
	}

	return ret;
}

static void ois_mcu_clk_disable(struct ois_mcu_dev *mcu)
{
	if (!IS_ERR(mcu->clk))
		clk_disable_unprepare(mcu->clk);

	if (!IS_ERR(mcu->spi_clk))
		clk_disable_unprepare(mcu->spi_clk);
}

static int ois_mcu_runtime_resume(struct device *dev)
{
	struct ois_mcu_dev *mcu = dev_get_drvdata(dev);
	int ret = 0;

	info_mcu("%s E\n", __func__);

	ret = ois_mcu_clk_get(mcu);
	if (ret)
		return ret;

	ret = ois_mcu_clk_enable(mcu);

	__is_mcu_pmu_control(1);
	usleep_range(1000, 1100);

	__is_mcu_hw_enable(mcu->regs[OM_REG_CORE]);
	ret |= __is_mcu_hw_reset_peri(mcu->regs[OM_REG_PERI1], 0); /* clear USI reset reg USI17 */
	usleep_range(2000, 2100);
	ret |= __is_mcu_hw_reset_peri(mcu->regs[OM_REG_PERI2], 0); /* clear USI reset reg USI18 */
	usleep_range(2000, 2100);
	ret |= __is_mcu_hw_set_init_peri(mcu->regs[OM_REG_PERI_SETTING]); /*GPP9 setting */
	ret |= __is_mcu_hw_set_clock_peri(mcu->regs[OM_REG_PERI1]); /* set i2c clock to 1MH */

	clear_bit(OM_HW_SUSPENDED, &mcu->state);

	info_mcu("%s X\n", __func__);

	return ret;
}

static int ois_mcu_runtime_suspend(struct device *dev)
{
	struct ois_mcu_dev *mcu = dev_get_drvdata(dev);
	int ret = 0;
	u8 val = 0;
	int retries = 50;

	info_mcu("%s E\n", __func__);

	is_mcu_set_reg(mcu->regs[OM_REG_CORE], R_OIS_CMD_START, 0x00);
	do {
		val = is_mcu_get_reg(mcu->regs[OM_REG_CORE], R_OIS_CMD_STATUS);
		usleep_range(1000, 1100);
		if (--retries < 0) {
			err_mcu("%s Read status failed!!!!, data = 0x%04x\n", __func__, val);
			break;
		}
	} while (val != 0x01);

	ret = __is_mcu_hw_disable(mcu->regs[OM_REG_CORE]);
	ret |= __is_mcu_hw_set_clear_peri(mcu->regs[OM_REG_PERI_SETTING]); /*GPP9 clear */
	usleep_range(2000, 2100); //TEMP_2020 Need to be checked
	ret |= __is_mcu_hw_reset_peri(mcu->regs[OM_REG_PERI1], 1); /* clear USI reset reg USI17 */
	ret |= __is_mcu_hw_reset_peri(mcu->regs[OM_REG_PERI2], 1); /* clear USI reset reg USI18 */
	ret |= __is_mcu_hw_clear_peri(mcu->regs[OM_REG_PERI1]);
	ret |= __is_mcu_hw_clear_peri(mcu->regs[OM_REG_PERI2]);

	ois_mcu_clk_disable(mcu);
	ois_mcu_clk_put(mcu);

	__is_mcu_pmu_control(0);

	set_bit(OM_HW_SUSPENDED, &mcu->state);

	info_mcu("%s X\n", __func__);

	return ret;
}

#ifdef CONFIG_PM_SLEEP
static int ois_mcu_resume(struct device *dev)
{
	/* TODO: */
	return 0;
}

static int ois_mcu_suspend(struct device *dev)
{
	struct ois_mcu_dev *mcu = dev_get_drvdata(dev);

	/* TODO: */
	if (!test_bit(OM_HW_SUSPENDED, &mcu->state))
		return -EBUSY;

	return 0;
}
#endif /* CONFIG_PM_SLEEP */

static irqreturn_t is_isr_ois_mcu(int irq, void *data)
{
	struct ois_mcu_dev *mcu;
	unsigned int state;

	mcu = (struct ois_mcu_dev *)data;
	state = is_mcu_hw_g_irq_state(mcu->regs[OM_REG_CORE], true);

	/* FIXME: temp log for testing */
	//info_mcu("IRQ: %d\n", state);
	if (is_mcu_hw_g_irq_type(state, MCU_IRQ_WDT)) {
		/* TODO: WDR IRQ handling */
		dbg_mcu(1, "IRQ: MCU_IRQ_WDT");
	}

	if (is_mcu_hw_g_irq_type(state, MCU_IRQ_WDT_RST)) {
		/* TODO: WDR RST handling */
		dbg_mcu(1, "IRQ: MCU_IRQ_WDT_RST");
	}

	if (is_mcu_hw_g_irq_type(state, MCU_IRQ_LOCKUP_RST)) {
		/* TODO: LOCKUP RST handling */
		dbg_mcu(1, "IRQ: MCU_IRQ_LOCKUP_RST");
	}

	if (is_mcu_hw_g_irq_type(state, MCU_IRQ_SYS_RST)) {
		/* TODO: SYS RST handling */
		dbg_mcu(1, "IRQ: MCU_IRQ_SYS_RST");
	}

	return IRQ_HANDLED;
}

/*
 * API functions
 */
int ois_mcu_power_ctrl(struct ois_mcu_dev *mcu, int on)
{
	int ret = 0;
#if defined(CONFIG_PM)
	int rpm_ret;
#endif
	BUG_ON(!mcu);

	info_mcu("%s E\n", __func__);

	if (on) {
		if (!test_bit(OM_HW_SUSPENDED, &mcu->state)) {
			warning_mcu("already power on\n");
			goto p_err;
		}
#if defined(CONFIG_PM)
		rpm_ret = pm_runtime_get_sync(mcu->dev);
		if (rpm_ret < 0)
			err_mcu("pm_runtime_get_sync() err: %d", rpm_ret);
#else
		ret = ois_mcu_runtime_resume(mcu->dev);
#endif
		clear_bit(OM_HW_SUSPENDED, &mcu->state);
	} else {
		if (test_bit(OM_HW_SUSPENDED, &mcu->state)) {
			warning_mcu("already power off\n");
			goto p_err;
		}
#if defined(CONFIG_PM)
		rpm_ret = pm_runtime_put_sync(mcu->dev);
		if (rpm_ret < 0)
			err_mcu("pm_runtime_put_sync() err: %d", rpm_ret);
#else
		ret = ois_mcu_runtime_suspend(mcu->dev);
#endif
		set_bit(OM_HW_SUSPENDED, &mcu->state);
		clear_bit(OM_HW_FW_LOADED, &mcu->state);
		clear_bit(OM_HW_RUN, &mcu->state);
		
		mcu->dev_ctrl_state = false;
	}

	info_mcu("%s: (%d) X\n", __func__, on);

p_err:
	return ret;
}

int ois_mcu_load_binary(struct ois_mcu_dev *mcu)
{
	int ret = 0;
	long size = 0;

	BUG_ON(!mcu);

	if (test_bit(OM_HW_FW_LOADED, &mcu->state)) {
		warning_mcu("already fw was loaded\n");
		return ret;
	}

	size = __is_mcu_load_fw(mcu->regs[OM_REG_CORE], mcu->dev);
	if (size <= 0)
		return -EINVAL;

	set_bit(OM_HW_FW_LOADED, &mcu->state);

	return ret;
}

int ois_mcu_core_ctrl(struct ois_mcu_dev *mcu, int on)
{
	int ret = 0;

	BUG_ON(!mcu);

	info_mcu("%s E\n", __func__);

	if (on) {
		if (test_bit(OM_HW_RUN, &mcu->state)) {
			warning_mcu("already started\n");
			return ret;
		}
		__is_mcu_hw_s_irq_enable(mcu->regs[OM_REG_CORE], 0x0);
		set_bit(OM_HW_RUN, &mcu->state);
	} else {
		if (!test_bit(OM_HW_RUN, &mcu->state)) {
			warning_mcu("already stopped\n");
			return ret;
		}
		clear_bit(OM_HW_RUN, &mcu->state);
	}

	ret = __is_mcu_core_control(mcu->regs[OM_REG_CORE], on);

	info_mcu("%s: %d X\n", __func__, on);

	return ret;
}

int ois_mcu_dump(struct ois_mcu_dev *mcu, int type)
{
	int ret = 0;

	BUG_ON(!mcu);

	if (test_bit(OM_HW_SUSPENDED, &mcu->state))
		return 0;

	switch (type) {
	case OM_REG_CORE:
		__is_mcu_hw_cr_dump(mcu->regs[OM_REG_CORE]);
		__is_mcu_hw_sram_dump(mcu->regs[OM_REG_CORE],
			__is_mcu_get_sram_size());
		break;
	case OM_REG_PERI1:
		__is_mcu_hw_peri1_dump(mcu->regs[OM_REG_PERI1]);
		break;
	case OM_REG_PERI2:
		__is_mcu_hw_peri2_dump(mcu->regs[OM_REG_PERI2]);
		break;
	default:
		err_mcu("undefined type (%d)\n", type);
	}

	return ret;
}

void ois_mcu_parsing_raw_data(uint8_t *buf, long efs_size, long *raw_data_x, long *raw_data_y)
{
	int i = 0, j = 0;
	char efs_data_pre[MAX_GYRO_EFS_DATA_LENGTH + 1];
	char efs_data_post[MAX_GYRO_EFS_DATA_LENGTH+ 1];
	bool detect_point = false;
	int sign = 1;
	long raw_pre = 0, raw_post = 0;

	memset(efs_data_pre, 0x0, sizeof(efs_data_pre));
	memset(efs_data_post, 0x0, sizeof(efs_data_post));
	i = 0;
	j = 0;
	while ((*(buf + i)) != ',') {
		if (((char)*(buf + i)) == '-' ) {
			sign = -1;
			i++;
		}

		if (((char)*(buf + i)) == '.') {
			detect_point = true;
			i++;
			j = 0;
		}

		if (detect_point) {
			memcpy(efs_data_post + j, buf + i, 1);
			j++;
		} else {
			memcpy(efs_data_pre + j, buf + i, 1);
			j++;
		}

		if (++i > MAX_GYRO_EFS_DATA_LENGTH) {
			err_mcu("wrong EFS data.");
			break;
		}
	}
	i++;
	kstrtol(efs_data_pre, 10, &raw_pre);
	kstrtol(efs_data_post, 10, &raw_post);
	*raw_data_x = sign * (raw_pre * 1000 + raw_post);

	detect_point = false;
	j = 0;
	raw_pre = 0;
	raw_post = 0;
	sign = 1;
	memset(efs_data_pre, 0x0, sizeof(efs_data_pre));
	memset(efs_data_post, 0x0, sizeof(efs_data_post));
	while (i < efs_size) {
		if (((char)*(buf + i)) == '-' ) {
			sign = -1;
			i++;
		}

		if (((char)*(buf + i)) == '.') {
			detect_point = true;
			i++;
			j = 0;
		}

		if (detect_point) {
			memcpy(efs_data_post + j, buf + i, 1);
			j++;
		} else {
			memcpy(efs_data_pre + j, buf + i, 1);
			j++;
		}

		if (++i > MAX_GYRO_EFS_DATA_LENGTH) {
			err_mcu("wrong EFS data.");
			break;
		}
	}
	kstrtol(efs_data_pre, 10, &raw_pre);
	kstrtol(efs_data_post, 10, &raw_post);
	*raw_data_y = sign * (raw_pre * 1000 + raw_post);

	info_mcu("%s : X raw_x = %ld, raw_y = %ld\n", __func__, *raw_data_x, *raw_data_y);
}

long ois_mcu_get_efs_data(struct ois_mcu_dev *mcu, long *raw_data_x, long *raw_data_y)
{
	long efs_size = 0;
	struct is_core *core = NULL;
	struct is_vender_specific *specific;

	core = (struct is_core *)dev_get_drvdata(is_dev);
	specific = core->vender.private_data;

	info_mcu("%s : E\n", __func__);

	efs_size = specific->gyro_efs_size;

	if (efs_size == 0) {
		err_mcu("efs read failed.");
		goto p_err;
	}

	ois_mcu_parsing_raw_data(specific->gyro_efs_data, efs_size, raw_data_x, raw_data_y);

p_err:
	return efs_size;
}

int ois_mcu_init(struct v4l2_subdev *subdev)
{
	int ret = 0;
#ifdef USE_OIS_SLEEP_MODE
	u8 read_gyrocalcen = 0;
#endif
	u8 val = 0;
	u8 error_reg[2] = {0, };
	u8 gyro_orientation = 0;
	u8 wx_pole = 0;
	u8 wy_pole = 0;
	u8 tx_pole = 0;
	u8 ty_pole = 0;
#ifdef CAMERA_3RD_OIS
	u8 t2x_pole = 0;
	u8 t2y_pole = 0;
#endif
	int retries = 600;
	int i = 0;
	int scale_factor = OIS_GYRO_SCALE_FACTOR_LSM6DSO;
	long gyro_data_x = 0, gyro_data_y = 0, gyro_data_size = 0;
	u8 gyro_x = 0, gyro_x2 = 0;
	u8 gyro_y = 0, gyro_y2 = 0;
	int tele_cmd_xcoef = 0;
	int tele_cmd_ycoef = 0;
#ifndef OIS_DUAL_CAL_DEFAULT_VALUE_TELE
	struct is_vender_specific *specific;
	u8 tele_xcoef[2];
	u8 tele_ycoef[2];
	long efs_size = 0;
#ifndef OIS_DUAL_CAL_DEFAULT_EEPROM_VALUE_TELE
	int rom_id = 0;
	char *cal_buf;
	struct is_rom_info *finfo = NULL;
	u8 eeprom_xcoef[2];
	u8 eeprom_ycoef[2];
#endif
#endif
	struct is_mcu *is_mcu = NULL;
	struct ois_mcu_dev *mcu = NULL;
	struct is_ois *ois = NULL;
	struct is_module_enum *module = NULL;
	struct is_device_sensor_peri *sensor_peri = NULL;
	struct is_ois_info *ois_pinfo = NULL;
	struct is_core *core = NULL;

	WARN_ON(!subdev);

	info_mcu("%s E\n", __func__);

	mcu = (struct ois_mcu_dev *)v4l2_get_subdevdata(subdev);
	if(!mcu) {
		err_mcu("%s, mcu is NULL", __func__);
		ret = -EINVAL;
		return ret;
	}

	is_mcu = (struct is_mcu *)v4l2_get_subdev_hostdata(subdev);
	if(!is_mcu) {
		err_mcu("%s, is_mcu is NULL", __func__);
		ret = -EINVAL;
		return ret;
	}

	sensor_peri = is_mcu->sensor_peri;
	if (!sensor_peri) {
		err_mcu("%s, sensor_peri is NULL", __func__);
		ret = -EINVAL;
		return ret;
	}

	module = sensor_peri->module;
	if (!module) {
		err_mcu("%s, module is NULL", __func__);
		ret = -EINVAL;
		return ret;
	}
	
	core = (struct is_core *)dev_get_drvdata(is_dev);
	if (!core) {
		err_mcu("%s, core is null", __func__);
		ret = -EINVAL;
		return ret;
	}

	is_sec_get_ois_pinfo(&ois_pinfo);

	ois = is_mcu->ois;
	ois->ois_mode = sensor_peri->mcu->ois->ois_mode;
	ois->pre_ois_mode = OPTICAL_STABILIZATION_MODE_OFF;
	ois->coef = 0;
	ois->pre_coef = 255;
	ois->fadeupdown = false;
	ois->initial_centering_mode = false;
	ois->af_pos_wide = 0;
	ois->af_pos_tele = 0;
#ifdef CAMERA_2ND_OIS
	ois->ois_power_mode = -1;
#endif
	ois_pinfo->reset_check = false;

	if (ois_hw_check) {
		if (module->position == SENSOR_POSITION_REAR)
			ois_wide_init = true;
		else if (module->position == SENSOR_POSITION_REAR2)
			ois_tele_init = true;
		else if (module->position == SENSOR_POSITION_REAR4)
			ois_tele2_init = true;

		info_mcu("%s %d sensor(%d) mcu is already initialized.\n", __func__, __LINE__, module->position);
		ois->ois_shift_available = true;
	}

	if (!ois_hw_check && test_bit(OM_HW_RUN, &mcu->state)) {
		do {
			val = is_mcu_get_reg(mcu->regs[OM_REG_CORE], R_OIS_CMD_STATUS);
			usleep_range(500, 510);
			if (--retries < 0) {
				err_mcu("%s Read status failed!!!!, data = 0x%04x\n", __func__, val);
				break;
			}
		} while (val != 0x01);

		error_reg[0] = (u8)is_mcu_get_reg(mcu->regs[OM_REG_CORE], R_OIS_CMD_GYRO_RESULT);
		error_reg[1] = (u8)is_mcu_get_reg(mcu->regs[OM_REG_CORE], R_OIS_CMD_CHECKSUM);
		info_mcu("%s error reg value = 0x%02x/0x%02x", __func__, error_reg[0], error_reg[1]);

		/* MCU err reg recovery code */
		if (error_reg[1] && (core->mcu->need_reset_mcu || (mcu->recover_tele2 && mcu->is_tele2_on))) {
			/* write AF CTRL standby */
			is_mcu_set_reg(mcu->regs[OM_REG_CORE], R_OIS_CMD_CTRL_AF, MCU_AF_MODE_STANDBY);
			msleep(10);
			/* clear ois err reg */
			is_mcu_set_reg(mcu->regs[OM_REG_CORE], R_OIS_CMD_CHECKSUM, 0x0);
			/* write AF CTRL active */
			is_mcu_set_reg(mcu->regs[OM_REG_CORE], R_OIS_CMD_CTRL_AF, MCU_AF_MODE_ACTIVE);

			if (core->mcu->need_reset_mcu) {
				core->mcu->need_reset_mcu = false;
				info_mcu("[%s] clear ois reset flag.", __func__);
			} else if (mcu->recover_tele2 && mcu->is_tele2_on) {
				mcu->recover_tele2 = false;
				mcu->is_tele2_on = false;
				info_mcu("[%s] recover mcu in case of not using tele2 sensor.", __func__);
			}
		}

		if (val == 0x01) {
			/* loading gyro data */
			gyro_data_size = ois_mcu_get_efs_data(mcu, &gyro_data_x, &gyro_data_y);
			info_mcu("Read Gyro offset data :  0x%04x, 0x%04x", gyro_data_x, gyro_data_y);
			gyro_data_x = gyro_data_x * scale_factor;
			gyro_data_y = gyro_data_y * scale_factor;
			gyro_data_x = gyro_data_x / 1000;
			gyro_data_y = gyro_data_y / 1000;
			if (gyro_data_size > 0) {
				gyro_x = gyro_data_x & 0xFF;
				gyro_x2 = (gyro_data_x >> 8) & 0xFF;
				gyro_y = gyro_data_y & 0xFF;
				gyro_y2 = (gyro_data_y >> 8) & 0xFF;
				is_mcu_set_reg(mcu->regs[OM_REG_CORE], R_OIS_CMD_RAW_DEBUG_X1, gyro_x);
				is_mcu_set_reg(mcu->regs[OM_REG_CORE], R_OIS_CMD_RAW_DEBUG_X2, gyro_x2);
				is_mcu_set_reg(mcu->regs[OM_REG_CORE], R_OIS_CMD_RAW_DEBUG_Y1, gyro_y);
				is_mcu_set_reg(mcu->regs[OM_REG_CORE], R_OIS_CMD_RAW_DEBUG_Y2, gyro_y2);
				info_mcu("Write Gyro offset data :  0x%02x, 0x%02x, 0x%02x, 0x%02x", gyro_x, gyro_x2, gyro_y, gyro_y2);
			}
			/* write wide xgg ygg xcoef ycoef */
			if (ois_pinfo->wide_romdata.cal_mark[0] == 0xBB) {
				for (i = 0; i < 4; i++) {
					is_mcu_set_reg(mcu->regs[OM_REG_CORE], R_OIS_CMD_REAR_XGG1 + i, ois_pinfo->wide_romdata.xgg[i]);
				}
				for (i = 0; i < 4; i++) {
					is_mcu_set_reg(mcu->regs[OM_REG_CORE], R_OIS_CMD_REAR_YGG1 + i, ois_pinfo->wide_romdata.ygg[i]);
				}
				for (i = 0; i < 2; i++) {
#ifdef OIS_DUAL_CAL_DEFAULT_VALUE_WIDE
					is_mcu_set_reg(mcu->regs[OM_REG_CORE], R_OIS_CMD_XCOEF_M1_1 + i, OIS_DUAL_CAL_DEFAULT_VALUE_WIDE);
#else
					is_mcu_set_reg(mcu->regs[OM_REG_CORE], R_OIS_CMD_XCOEF_M1_1 + i, ois_pinfo->wide_romdata.xcoef[i]);
#endif
				}
				for (i = 0; i < 2; i++) {
#ifdef OIS_DUAL_CAL_DEFAULT_VALUE_WIDE
					is_mcu_set_reg(mcu->regs[OM_REG_CORE], R_OIS_CMD_YCOEF_M1_1 + i, OIS_DUAL_CAL_DEFAULT_VALUE_WIDE);
#else
					is_mcu_set_reg(mcu->regs[OM_REG_CORE], R_OIS_CMD_YCOEF_M1_1 + i, ois_pinfo->wide_romdata.ycoef[i]);
#endif
				}
			} else {
				info_mcu("%s Does not loading wide xgg/ygg data from eeprom.", __func__);
			}

			/* write tele xgg ygg xcoef ycoef */
			if (ois_pinfo->tele_tilt_romdata.cal_mark[0] == 0xBB) {
#ifdef OIS_DUAL_CAL_USE_REAR3_DATA
				tele_cmd_xcoef = R_OIS_CMD_XCOEF_M3_1;
				tele_cmd_ycoef = R_OIS_CMD_YCOEF_M3_1;
#else
				tele_cmd_xcoef = R_OIS_CMD_XCOEF_M2_1;
				tele_cmd_ycoef = R_OIS_CMD_YCOEF_M2_1;
#endif

#ifndef OIS_DUAL_CAL_DEFAULT_VALUE_TELE
				specific = core->vender.private_data;
				efs_size = specific->tilt_cal_tele2_efs_size;
				if (efs_size) {
					efs_info.ois_hall_shift_x = *((s16 *)&specific->tilt_cal_tele2_efs_data[MCU_HALL_SHIFT_ADDR_X_M2]);
					efs_info.ois_hall_shift_y = *((s16 *)&specific->tilt_cal_tele2_efs_data[MCU_HALL_SHIFT_ADDR_Y_M2]);
					set_bit(IS_EFS_STATE_READ, &efs_info.efs_state);
				} else {
					clear_bit(IS_EFS_STATE_READ, &efs_info.efs_state);
				}
#endif
				for (i = 0; i < 4; i++) {
					is_mcu_set_reg(mcu->regs[OM_REG_CORE], R_OIS_CMD_REAR2_XGG1 + i, ois_pinfo->tele_romdata.xgg[i]);
#ifdef CAMERA_3RD_OIS
					is_mcu_set_reg(mcu->regs[OM_REG_CORE], R_OIS_CMD_REAR3_XGG1 + i, ois_pinfo->tele2_romdata.xgg[i]);
#endif
				}
				for (i = 0; i < 4; i++) {
					is_mcu_set_reg(mcu->regs[OM_REG_CORE], R_OIS_CMD_REAR2_YGG1 + i, ois_pinfo->tele_romdata.ygg[i]);
#ifdef CAMERA_3RD_OIS
					is_mcu_set_reg(mcu->regs[OM_REG_CORE], R_OIS_CMD_REAR3_YGG1 + i, ois_pinfo->tele2_romdata.ygg[i]);
#endif
				}
#ifdef OIS_DUAL_CAL_DEFAULT_VALUE_TELE
				is_mcu_set_reg(mcu->regs[OM_REG_CORE], tele_cmd_xcoef, OIS_DUAL_CAL_DEFAULT_VALUE_TELE);
				is_mcu_set_reg(mcu->regs[OM_REG_CORE], tele_cmd_xcoef + 1, OIS_DUAL_CAL_DEFAULT_VALUE_TELE);
				is_mcu_set_reg(mcu->regs[OM_REG_CORE], tele_cmd_ycoef, OIS_DUAL_CAL_DEFAULT_VALUE_TELE);
				is_mcu_set_reg(mcu->regs[OM_REG_CORE], tele_cmd_ycoef + 1, OIS_DUAL_CAL_DEFAULT_VALUE_TELE);

				info_mcu("%s tele use default coef value", __func__);
#else
				if (!test_bit(IS_EFS_STATE_READ, &efs_info.efs_state)) {
#ifdef OIS_DUAL_CAL_DEFAULT_EEPROM_VALUE_TELE
					is_mcu_set_reg(mcu->regs[OM_REG_CORE], tele_cmd_xcoef, OIS_DUAL_CAL_DEFAULT_EEPROM_VALUE_TELE);
					is_mcu_set_reg(mcu->regs[OM_REG_CORE], tele_cmd_xcoef + 1, OIS_DUAL_CAL_DEFAULT_EEPROM_VALUE_TELE);
					is_mcu_set_reg(mcu->regs[OM_REG_CORE], tele_cmd_ycoef, OIS_DUAL_CAL_DEFAULT_EEPROM_VALUE_TELE);
					is_mcu_set_reg(mcu->regs[OM_REG_CORE], tele_cmd_ycoef + 1, OIS_DUAL_CAL_DEFAULT_EEPROM_VALUE_TELE);

					info_mcu("%s tele use default eeprom coef value", __func__);
#else
					rom_id = is_vendor_get_rom_id_from_position(SENSOR_POSITION_REAR2);
					is_sec_get_cal_buf(&cal_buf, rom_id);
					is_sec_get_sysfs_finfo(&finfo, rom_id);

					eeprom_xcoef[0] = *((u8 *)&cal_buf[finfo->rom_dualcal_slave1_oisshift_x_addr]);
					eeprom_xcoef[1] = *((u8 *)&cal_buf[finfo->rom_dualcal_slave1_oisshift_x_addr + 1]);
					eeprom_ycoef[0] = *((u8 *)&cal_buf[finfo->rom_dualcal_slave1_oisshift_y_addr]);
					eeprom_ycoef[1] = *((u8 *)&cal_buf[finfo->rom_dualcal_slave1_oisshift_y_addr + 1]);

					is_mcu_set_reg(mcu->regs[OM_REG_CORE], tele_cmd_xcoef, eeprom_xcoef[0]);
					is_mcu_set_reg(mcu->regs[OM_REG_CORE], tele_cmd_xcoef + 1, eeprom_xcoef[1]);
					is_mcu_set_reg(mcu->regs[OM_REG_CORE], tele_cmd_ycoef, eeprom_ycoef[0]);
					is_mcu_set_reg(mcu->regs[OM_REG_CORE], tele_cmd_ycoef + 1, eeprom_ycoef[1]);

					info_mcu("%s tele eeprom xcoef = %d/%d, ycoef = %d/%d", __func__, eeprom_xcoef[0], eeprom_xcoef[1],
						eeprom_ycoef[0], eeprom_ycoef[1]);
#endif
				} else {
					tele_xcoef[0] = efs_info.ois_hall_shift_x & 0xFF;
					tele_xcoef[1] = (efs_info.ois_hall_shift_x >> 8) & 0xFF;
					tele_ycoef[0] = efs_info.ois_hall_shift_y & 0xFF;
					tele_ycoef[1] = (efs_info.ois_hall_shift_y >> 8) & 0xFF;

					is_mcu_set_reg(mcu->regs[OM_REG_CORE], tele_cmd_xcoef, tele_xcoef[0]);
					is_mcu_set_reg(mcu->regs[OM_REG_CORE], tele_cmd_xcoef + 1, tele_xcoef[1]);
					is_mcu_set_reg(mcu->regs[OM_REG_CORE], tele_cmd_ycoef, tele_ycoef[0]);
					is_mcu_set_reg(mcu->regs[OM_REG_CORE], tele_cmd_ycoef + 1, tele_ycoef[1]);

					info_mcu("%s tele efs xcoef = %d, ycoef = %d", __func__, efs_info.ois_hall_shift_x, efs_info.ois_hall_shift_y);
				}
#endif
			} else {
				info_mcu("%s Does not loading tele xgg/ygg data from eeprom.", __func__);
			}

			/* enable dual cal */
			is_mcu_set_reg(mcu->regs[OM_REG_CORE], R_OIS_CMD_ENABLE_DUALCAL, 0x01);

			wx_pole = common_mcu_infos.ois_gyro_direction[0];
			wy_pole = common_mcu_infos.ois_gyro_direction[1];
			gyro_orientation = common_mcu_infos.ois_gyro_direction[2];
			tx_pole = common_mcu_infos.ois_gyro_direction[3];
			ty_pole = common_mcu_infos.ois_gyro_direction[4];
#ifdef CAMERA_3RD_OIS
			t2x_pole = common_mcu_infos.ois_gyro_direction[5];
			t2y_pole = common_mcu_infos.ois_gyro_direction[6];

			info_mcu("%s gyro direction list  %d,%d,%d,%d,%d,%d,%d\n", __func__, wx_pole, wy_pole, gyro_orientation,
				tx_pole, ty_pole, t2x_pole, t2y_pole);
#else
			info_mcu("%s gyro direction list  %d,%d,%d,%d,%d\n", __func__, wx_pole, wy_pole, gyro_orientation,
				tx_pole, ty_pole);
#endif


			is_mcu_set_reg(mcu->regs[OM_REG_CORE], R_OIS_CMD_GYRO_POLA_X, wx_pole);
			is_mcu_set_reg(mcu->regs[OM_REG_CORE], R_OIS_CMD_GYRO_POLA_Y, wy_pole);
			is_mcu_set_reg(mcu->regs[OM_REG_CORE], R_OIS_CMD_GYRO_ORIENT, gyro_orientation);
			is_mcu_set_reg(mcu->regs[OM_REG_CORE], R_OIS_CMD_GYRO_POLA_X_M2, tx_pole);
			is_mcu_set_reg(mcu->regs[OM_REG_CORE], R_OIS_CMD_GYRO_POLA_Y_M2, ty_pole);
#ifdef CAMERA_3RD_OIS
			is_mcu_set_reg(mcu->regs[OM_REG_CORE], R_OIS_CMD_GYRO_POLA_X_M3, t2x_pole);
			is_mcu_set_reg(mcu->regs[OM_REG_CORE], R_OIS_CMD_GYRO_POLA_Y_M3, t2y_pole);
#endif
			info_mcu("%s gyro init data applied.\n", __func__);

#ifdef CAMERA_3RD_OIS
			if (!error_reg[1] || !mcu->recover_tele2)
#endif
				ois_hw_check = true;
			mcu->is_mcu_active = false;

			if (module->position == SENSOR_POSITION_REAR)
				ois_wide_init = true;
			else if (module->position == SENSOR_POSITION_REAR2)
				ois_tele_init = true;
			else if (module->position == SENSOR_POSITION_REAR4)
				ois_tele2_init = true;
		}
	}

	info_mcu("%s sensor(%d) X\n", __func__, module->position);

	return ret;
}

int ois_mcu_init_factory(struct v4l2_subdev *subdev)
{
	int ret = 0;
	u8 val = 0;
	int retries = 600;
	u8 gyro_orientation = 0;
	struct is_mcu *is_mcu = NULL;
	struct ois_mcu_dev *mcu = NULL;
	struct is_ois *ois = NULL;
	struct is_ois_info *ois_pinfo = NULL;

	WARN_ON(!subdev);

	info_mcu("%s E\n", __func__);

	mcu = (struct ois_mcu_dev *)v4l2_get_subdevdata(subdev);
	if(!mcu) {
		err_mcu("%s, mcu is NULL", __func__);
		ret = -EINVAL;
		return ret;
	}

	is_mcu = (struct is_mcu *)v4l2_get_subdev_hostdata(subdev);
	if(!is_mcu) {
		err_mcu("%s, is_mcu is NULL", __func__);
		ret = -EINVAL;
		return ret;
	}

	is_sec_get_ois_pinfo(&ois_pinfo);

	ois = is_mcu->ois;
	ois->ois_mode = OPTICAL_STABILIZATION_MODE_OFF;
	ois->pre_ois_mode = OPTICAL_STABILIZATION_MODE_OFF;
	ois->coef = 0;
	ois->pre_coef = 255;
	ois->fadeupdown = false;
	ois->initial_centering_mode = false;
	ois->af_pos_wide = 0;
	ois->af_pos_tele = 0;
#ifdef CAMERA_2ND_OIS
	ois->ois_power_mode = -1;
#endif
	ois_pinfo->reset_check = false;

	if (test_bit(OM_HW_RUN, &mcu->state)) {
		do {
			val = is_mcu_get_reg(mcu->regs[OM_REG_CORE], R_OIS_CMD_STATUS);
			usleep_range(500, 510);
			if (--retries < 0) {
				err_mcu("%s Read status failed!!!!, data = 0x%04x\n", __func__, val);
				break;
			}
		} while (val != 0x01);
	}

#ifdef CAMERA_3RD_OIS
	is_mcu_set_reg(mcu->regs[OM_REG_CORE], R_OIS_CMD_OIS_SEL, 0x07); /* OIS SEL (wide : 1 , tele : 2, tele2 : 4, triple : 7 ). */
#else
	is_mcu_set_reg(mcu->regs[OM_REG_CORE], R_OIS_CMD_OIS_SEL, 0x03); /* OIS SEL (wide : 1 , tele : 2, both : 3 ). */
#endif

	gyro_orientation = common_mcu_infos.ois_gyro_direction[2];
	is_mcu_set_reg(mcu->regs[OM_REG_CORE], R_OIS_CMD_GYRO_ORIENT, gyro_orientation);

	info_mcu("%s sensor(%d) X\n", __func__, ois->device);
	return ret;
}

void ois_mcu_init_rear2(struct is_core *core)
{
	u8 val = 0;
	int retries = 600;
	struct ois_mcu_dev *mcu = NULL;

	mcu = core->mcu;

	info_mcu("%s : E\n", __func__);

	/* check ois status */
	do {
		val = is_mcu_get_reg(mcu->regs[OM_REG_CORE], R_OIS_CMD_STATUS);
		usleep_range(500, 510);
		if (--retries < 0) {
			err_mcu("%s Read status failed!!!!, data = 0x%04x\n", __func__, val);
			break;
		}
	} while (val != 0x01);

	/* set power mode */
	is_mcu_set_reg(mcu->regs[OM_REG_CORE], R_OIS_CMD_OIS_SEL, 0x04); /* OIS SEL (wide : 1 , tele : 2, tele2 : 4, triple : 7 ). */

	info_mcu("%s : X\n", __func__);

	return;
}

int ois_mcu_deinit(struct v4l2_subdev *subdev)
{
	int ret = 0;
	struct ois_mcu_dev *mcu = NULL;
	struct is_mcu *is_mcu = NULL;
	struct is_device_sensor_peri *sensor_peri = NULL;
	struct is_module_enum *module = NULL;
	struct is_core *core = NULL;

	WARN_ON(!subdev);

	info_mcu("%s E\n", __func__);

	mcu = (struct ois_mcu_dev *)v4l2_get_subdevdata(subdev);
	if(!mcu) {
		err("%s, mcu subdev is NULL", __func__);
		ret = -EINVAL;
		return ret;
	}

	is_mcu = (struct is_mcu *)v4l2_get_subdev_hostdata(subdev);
	if(!is_mcu) {
		err_mcu("%s, is_mcu is NULL", __func__);
		ret = -EINVAL;
		return ret;
	}

	sensor_peri = is_mcu->sensor_peri;
	if (!sensor_peri) {
		err_mcu("%s, sensor_peri is NULL", __func__);
		ret = -EINVAL;
		return ret;
	}

	module = sensor_peri->module;
	if (!module) {
		err_mcu("%s, module is NULL", __func__);
		ret = -EINVAL;
		return ret;
	}

	core = (struct is_core *)dev_get_drvdata(is_dev);
	if (!core) {
		err("core is null");
		ret = -EINVAL;
		return ret;
	}

	if (module->position  == SENSOR_POSITION_REAR)
		ois_wide_init = false;
	else if  (module->position  == SENSOR_POSITION_REAR2)
		ois_tele_init = false;
	else if  (module->position  == SENSOR_POSITION_REAR4)
		ois_tele2_init = false;

	if (ois_hw_check && !(ois_wide_init || ois_tele_init || ois_tele2_init)) {
#ifdef CONFIG_USE_OIS_TAMODE_CONTROL
		if (ois_tamode_onoff && ois_tamode_status) {
			is_mcu_set_reg(mcu->regs[OM_REG_CORE], R_OIS_CMD_TAMODE, 0x00);
			ois_tamode_status = false;
			ois_tamode_onoff = false;
		}
#endif
		is_mcu_set_reg(mcu->regs[OM_REG_CORE], R_OIS_CMD_START, 0x00);
		usleep_range(2000, 2100);

		ois_fadeupdown = false;
		ois_hw_check = false;
		mcu->is_mcu_active = false;
		mcu->recover_tele2 = false;
		mcu->is_tele2_on = false;
		info_mcu("%s ois stop. sensor = (%d)X\n", __func__, module->position);
	}

	if (core->mcu->need_reset_mcu) {
		core->mcu->need_reset_mcu = false;
		info("[%s] clear ois reset flag.", __func__);
	}

	info_mcu("%s sensor = (%d)X\n", __func__, module->position);

	return ret;
}

int ois_mcu_set_ggfadeupdown(struct v4l2_subdev *subdev, int up, int down)
{
	int ret = 0;
	struct is_ois *ois = NULL;
	struct is_mcu *is_mcu = NULL;
	struct ois_mcu_dev *mcu = NULL;
	u8 status = 0;
	int retries = 100;
	u8 data[2];
	//u8 write_data[4] = {0,};
#ifdef USE_OIS_SLEEP_MODE
	u8 read_sensorStart = 0;
#endif

	WARN_ON(!subdev);

	mcu = (struct ois_mcu_dev *)v4l2_get_subdevdata(subdev);
	if(!mcu) {
		err("%s, mcu subdev is NULL", __func__);
		ret = -EINVAL;
		return ret;
	}

	is_mcu = (struct is_mcu *)v4l2_get_subdev_hostdata(subdev);
	if(!is_mcu) {
		err("%s, is_mcu is NULL", __func__);
		ret = -EINVAL;
		return ret;
	}

	ois = is_mcu->ois;

	dbg_ois("%s up:%d down:%d\n", __func__, up, down);

	/* Wide af position value */
	is_mcu_set_reg(mcu->regs[OM_REG_CORE], R_OIS_CMD_REAR_AF, MCU_AF_INIT_POSITION);

#ifdef CAMERA_2ND_OIS
	/* Tele af position value */
	is_mcu_set_reg(mcu->regs[OM_REG_CORE], R_OIS_CMD_REAR2_AF, MCU_AF_INIT_POSITION);
#endif
#ifdef CAMERA_3RD_OIS
	/* Tele2 af position value */
	is_mcu_set_reg(mcu->regs[OM_REG_CORE], R_OIS_CMD_REAR3_AF, MCU_AF_INIT_POSITION);
#endif

	is_mcu_set_reg(mcu->regs[OM_REG_CORE], R_OIS_CMD_CACTRL_WRITE, 0x01);

	/* set fadeup */
	data[0] = up & 0xFF;
	data[1] = (up >> 8) & 0xFF;
	is_mcu_set_reg(mcu->regs[OM_REG_CORE], R_OIS_CMD_FADE_UP1, data[0]);
	is_mcu_set_reg(mcu->regs[OM_REG_CORE], R_OIS_CMD_FADE_UP2, data[1]);

	/* set fadedown */
	data[0] = down & 0xFF;
	data[1] = (down >> 8) & 0xFF;
	is_mcu_set_reg(mcu->regs[OM_REG_CORE], R_OIS_CMD_FADE_DOWN1, data[0]);
	is_mcu_set_reg(mcu->regs[OM_REG_CORE], R_OIS_CMD_FADE_DOWN2, data[1]);

	/* wait idle status
	 * 100msec delay is needed between "ois_power_on" and "ois_mode_s6".
	 */
	do {
		status = is_mcu_get_reg(mcu->regs[OM_REG_CORE], R_OIS_CMD_STATUS);
		if (status == 0x01 || status == 0x13)
			break;
		if (--retries < 0) {
			err("%s : read register fail!. status: 0x%x\n", __func__, status);
			ret = -1;
			break;
		}
		usleep_range(1000, 1100);
	} while (status != 0x01);

	dbg_ois("%s retryCount = %d , status = 0x%x\n", __func__, 100 - retries, status);

	return ret;
}

int ois_mcu_set_mode(struct v4l2_subdev *subdev, int mode)
{
	int ret = 0;
	struct is_ois *ois = NULL;
	struct is_mcu *is_mcu = NULL;
	struct ois_mcu_dev *mcu = NULL;

	WARN_ON(!subdev);

#ifndef CONFIG_SEC_FACTORY
	if (!ois_wide_init && !ois_tele_init && !ois_tele2_init)
		return 0;
#endif

	mcu = (struct ois_mcu_dev *)v4l2_get_subdevdata(subdev);
	if(!mcu) {
		err("%s, mcu subdev is NULL", __func__);
		ret = -EINVAL;
		return ret;
	}

	is_mcu = (struct is_mcu *)v4l2_get_subdev_hostdata(subdev);
	if(!is_mcu) {
		err("%s, is_mcu is NULL", __func__);
		ret = -EINVAL;
		return ret;
	}

	ois = is_mcu->ois;

	if (ois->fadeupdown == false) {
		if (ois_fadeupdown == false) {
			ois_fadeupdown = true;
			ois_mcu_set_ggfadeupdown(subdev, 1000, 1000);
		}
		ois->fadeupdown = true;
	}

	if (mode == ois->pre_ois_mode) {
		return ret;
	}

	ois->pre_ois_mode = mode;
	info_mcu("%s: ois_mode value(%d)\n", __func__, mode);

	switch(mode) {
		case OPTICAL_STABILIZATION_MODE_STILL:
			is_mcu_set_reg(mcu->regs[OM_REG_CORE], R_OIS_CMD_MODE, 0x00);
			is_mcu_set_reg(mcu->regs[OM_REG_CORE], R_OIS_CMD_START, 0x01);
			break;
		case OPTICAL_STABILIZATION_MODE_VIDEO:
			is_mcu_set_reg(mcu->regs[OM_REG_CORE], R_OIS_CMD_MODE, 0x01);
			is_mcu_set_reg(mcu->regs[OM_REG_CORE], R_OIS_CMD_START, 0x01);
			break;
		case OPTICAL_STABILIZATION_MODE_CENTERING:
			is_mcu_set_reg(mcu->regs[OM_REG_CORE], R_OIS_CMD_MODE, 0x05);
			is_mcu_set_reg(mcu->regs[OM_REG_CORE], R_OIS_CMD_START, 0x01);
			break;
		case OPTICAL_STABILIZATION_MODE_HOLD:
			is_mcu_set_reg(mcu->regs[OM_REG_CORE], R_OIS_CMD_MODE, 0x06);
			is_mcu_set_reg(mcu->regs[OM_REG_CORE], R_OIS_CMD_START, 0x01);
			break;
		case OPTICAL_STABILIZATION_MODE_STILL_ZOOM:
			is_mcu_set_reg(mcu->regs[OM_REG_CORE], R_OIS_CMD_MODE, 0x13);
			is_mcu_set_reg(mcu->regs[OM_REG_CORE], R_OIS_CMD_START, 0x01);
			break;
		case OPTICAL_STABILIZATION_MODE_VDIS:
			is_mcu_set_reg(mcu->regs[OM_REG_CORE], R_OIS_CMD_MODE, 0x14);
			is_mcu_set_reg(mcu->regs[OM_REG_CORE], R_OIS_CMD_START, 0x01);
			break;
		case OPTICAL_STABILIZATION_MODE_VDIS_ASR:
			is_mcu_set_reg(mcu->regs[OM_REG_CORE], R_OIS_CMD_MODE, 0x15);
			is_mcu_set_reg(mcu->regs[OM_REG_CORE], R_OIS_CMD_START, 0x01);
			break;
		case OPTICAL_STABILIZATION_MODE_SINE_X:
			is_mcu_set_reg(mcu->regs[OM_REG_CORE], R_OIS_CMD_SINE_1, 0x01);
			is_mcu_set_reg(mcu->regs[OM_REG_CORE], R_OIS_CMD_SINE_2, 0x01);
			is_mcu_set_reg(mcu->regs[OM_REG_CORE], R_OIS_CMD_SINE_3, 0x2D);
			is_mcu_set_reg(mcu->regs[OM_REG_CORE], R_OIS_CMD_MODE, 0x03);
			msleep(20);
			is_mcu_set_reg(mcu->regs[OM_REG_CORE], R_OIS_CMD_START, 0x01);
			break;
		case OPTICAL_STABILIZATION_MODE_SINE_Y:
			is_mcu_set_reg(mcu->regs[OM_REG_CORE], R_OIS_CMD_SINE_1, 0x02);
			is_mcu_set_reg(mcu->regs[OM_REG_CORE], R_OIS_CMD_SINE_2, 0x01);
			is_mcu_set_reg(mcu->regs[OM_REG_CORE], R_OIS_CMD_SINE_3, 0x2D);
			is_mcu_set_reg(mcu->regs[OM_REG_CORE], R_OIS_CMD_MODE, 0x03);
			msleep(20);
			is_mcu_set_reg(mcu->regs[OM_REG_CORE], R_OIS_CMD_START, 0x01);
			break;
		default:
			dbg_ois("%s: ois_mode value(%d)\n", __func__, mode);
			break;
	}

#ifdef USE_OIS_STABILIZATION_DELAY
	if (!mcu->is_mcu_active) {
		usleep_range(USE_OIS_STABILIZATION_DELAY, USE_OIS_STABILIZATION_DELAY + 10);
		mcu->is_mcu_active = true;
		info_mcu("%s : Stabilization delay applied, %d\n", __func__, USE_OIS_STABILIZATION_DELAY);
	}
#endif

	return ret;
}

int ois_mcu_shift_compensation(struct v4l2_subdev *subdev, int position, int resolution)
{
	int ret = 0;
	struct is_ois *ois;
	struct ois_mcu_dev *mcu = NULL;
	struct is_mcu *is_mcu = NULL;
	struct is_module_enum *module = NULL;
	struct is_device_sensor_peri *sensor_peri = NULL;
	int position_changed;

	WARN_ON(!subdev);

	mcu = (struct ois_mcu_dev *)v4l2_get_subdevdata(subdev);
	if(!mcu) {
		err("%s, mcu subdev is NULL", __func__);
		ret = -EINVAL;
		goto p_err;
	}

	is_mcu = (struct is_mcu *)v4l2_get_subdev_hostdata(subdev);
	if(!is_mcu) {
		err("%s, is_mcu is NULL", __func__);
		ret = -EINVAL;
		return ret;
	}

	sensor_peri = is_mcu->sensor_peri;
	if (!sensor_peri) {
		err("%s, sensor_peri is NULL", __func__);
		ret = -EINVAL;
		return ret;
	}

	module = sensor_peri->module;
	if (!module) {
		err("%s, module is NULL", __func__);
		ret = -EINVAL;
		return ret;
	}

	ois = is_mcu->ois;

	position_changed = position >> 4;

	if (module->position == SENSOR_POSITION_REAR && ois->af_pos_wide != position_changed) {
		/* Wide af position value */
		is_mcu_set_reg(mcu->regs[OM_REG_CORE], R_OIS_CMD_REAR_AF, (u8)position_changed);
		ois->af_pos_wide = position_changed;
	}
#if !defined(USE_TELE_OIS_AF_COMMON_INTERFACE) && defined(CAMERA_2ND_OIS)
	else if (module->position == SENSOR_POSITION_REAR2 && ois->af_pos_tele != position_changed) {
		/* Tele af position value */
		is_mcu_set_reg(mcu->regs[OM_REG_CORE], R_OIS_CMD_REAR2_AF, (u8)position_changed);
		ois->af_pos_tele = position_changed;
	}
#elif !defined(USE_TELE2_OIS_AF_COMMON_INTERFACE) && defined(CAMERA_3RD_OIS)
	else if (module->position == SENSOR_POSITION_REAR4 && ois->af_pos_tele2 != position_changed) {
		/* Tele af position value */
		is_mcu_set_reg(mcu->regs[OM_REG_CORE], R_OIS_CMD_REAR3_AF, (u8)position_changed);
		ois->af_pos_tele2 = position_changed;
	}
#endif

p_err:
	return ret;
}

int ois_mcu_self_test(struct is_core *core)
{
	u8 val = 0;
	u8 reg_val = 0, x = 0, y = 0;
	u16 x_gyro_log = 0, y_gyro_log = 0;
	int retries = 30;
	struct ois_mcu_dev *mcu = NULL;

	info_mcu("%s : E\n", __func__);

	mcu = core->mcu;

	is_mcu_set_reg(mcu->regs[OM_REG_CORE], R_OIS_CMD_GYRO_CAL, 0x08);

	do {
		val = is_mcu_get_reg(mcu->regs[OM_REG_CORE], R_OIS_CMD_GYRO_CAL);
		 msleep(50);
		if (--retries < 0) {
			err("Read register failed!!!!, data = 0x%04x\n", val);
			break;
		}
	} while (val);

	val = is_mcu_get_reg(mcu->regs[OM_REG_CORE], R_OIS_CMD_GYRO_RESULT);

	/* Gyro selfTest result */
	reg_val = is_mcu_get_reg(mcu->regs[OM_REG_CORE], R_OIS_CMD_GYRO_VAL_X);
	x = reg_val;
	reg_val = is_mcu_get_reg(mcu->regs[OM_REG_CORE], R_OIS_CMD_GYRO_LOG_X);
	x_gyro_log = (reg_val << 8) | x;

	reg_val = is_mcu_get_reg(mcu->regs[OM_REG_CORE], R_OIS_CMD_GYRO_VAL_Y);
	y = reg_val;
	reg_val = is_mcu_get_reg(mcu->regs[OM_REG_CORE], R_OIS_CMD_GYRO_LOG_Y);
	y_gyro_log = (reg_val << 8) | y;

	info_mcu("%s(GSTLOG0=%d, GSTLOG1=%d)\n", __func__, x_gyro_log, y_gyro_log);

	info_mcu("%s(%d) : X\n", __func__, val);
	return (int)val;
}

int ois_mcu_af_get_position(struct v4l2_subdev *subdev, struct v4l2_control *ctrl)
{
	ctrl->value = ACTUATOR_STATUS_NO_BUSY;

	return 0;
}

int ois_mcu_af_valid_check(void)
{
	int i;

	if (sysfs_actuator.init_step > 0) {
		for (i = 0; i < sysfs_actuator.init_step; i++) {
			if (sysfs_actuator.init_positions[i] < 0) {
				warn("invalid position value, default setting to position");
				return 0;
			} else if (sysfs_actuator.init_delays[i] < 0) {
				warn("invalid delay value, default setting to delay");
				return 0;
			}
		}
	} else
		return 0;

	return sysfs_actuator.init_step;
}

int ois_mcu_af_write_position(struct ois_mcu_dev *mcu, u32 val)
{
	u8 val_high = 0, val_low = 0;

	dbg_mcu(1, "%s : E\n", __func__);

	val_high = (val & 0x0FFF) >> 4;
	val_low = (val & 0x000F) << 4;

#if defined(USE_TELE_OIS_AF_COMMON_INTERFACE)
	is_mcu_set_reg(mcu->regs[OM_REG_CORE], R_OIS_CMD_POS1_REAR2_AF, val_high);
	is_mcu_set_reg(mcu->regs[OM_REG_CORE], R_OIS_CMD_POS2_REAR2_AF, val_low);
#elif defined(USE_TELE2_OIS_AF_COMMON_INTERFACE)
	is_mcu_set_reg(mcu->regs[OM_REG_CORE], R_OIS_CMD_POS1_REAR3_AF, val_high);
	is_mcu_set_reg(mcu->regs[OM_REG_CORE], R_OIS_CMD_POS2_REAR3_AF, val_low);
#endif
	usleep_range(2000, 2100);

	dbg_mcu(1, "%s : X\n", __func__);

	return 0;
}

static int ois_mcu_af_init_position(struct ois_mcu_dev *mcu,
		struct is_actuator *actuator)
{
	int i;
	int ret = 0;
	int init_step = 0;

	init_step = ois_mcu_af_valid_check();

	if (init_step > 0) {
		for (i = 0; i < init_step; i++) {
			ret = ois_mcu_af_write_position(mcu, sysfs_actuator.init_positions[i]);
			if (ret < 0)
				goto p_err;

			mdelay(sysfs_actuator.init_delays[i]);
		}

		actuator->position = sysfs_actuator.init_positions[i];
	} else {
		/* wide, tele camera uses previous position at initial time */
		if (actuator->device == 1 || actuator->position == 0)
			actuator->position = MCU_ACT_DEFAULT_FIRST_POSITION;

		ret = ois_mcu_af_write_position(mcu, actuator->position);
		if (ret < 0)
			goto p_err;
	}

p_err:
	return ret;
}

int ois_mcu_af_init(struct v4l2_subdev *subdev, u32 val)
{
	struct ois_mcu_dev *mcu = NULL;
	struct is_actuator *actuator = NULL;
	struct is_core *core;

	WARN_ON(!subdev);

	core = (struct is_core *)dev_get_drvdata(is_dev);
	if (!core) {
		err("core is null");
		return -EINVAL;
	}

	actuator = (struct is_actuator *)v4l2_get_subdevdata(subdev);
	WARN_ON(!actuator);

	mcu = core->mcu;

	is_mcu_set_reg(mcu->regs[OM_REG_CORE], R_OIS_CMD_CTRL_AF, MCU_AF_MODE_ACTIVE);
	msleep(10);

	actuator->position = val;

	dbg_mcu(1, "%s : X\n", __func__);

	return 0;
}

int ois_mcu_af_set_active(struct v4l2_subdev *subdev, int enable)
{
	int ret = 0;
	struct ois_mcu_dev *mcu = NULL;
	struct is_core *core;
	struct is_mcu *is_mcu = NULL;
	struct is_actuator *actuator = NULL;

	WARN_ON(!subdev);

	is_mcu = (struct is_mcu *)v4l2_get_subdev_hostdata(subdev);
	if(!is_mcu) {
		err_mcu("%s, is_mcu is NULL", __func__);
		ret = -EINVAL;
		return ret;
	}

	core = (struct is_core *)dev_get_drvdata(is_dev);
	if (!core) {
		err("core is null");
		return -EINVAL;
	}

	mcu = core->mcu;
	actuator = is_mcu->actuator;

	info_mcu("%s : E\n", __func__);

	if (enable) {
		is_mcu_set_reg(mcu->regs[OM_REG_CORE], R_OIS_CMD_CTRL_AF, MCU_AF_MODE_ACTIVE);
		msleep(10);
		ois_mcu_af_init_position(mcu, actuator);
	} else {
		is_mcu_set_reg(mcu->regs[OM_REG_CORE], R_OIS_CMD_CTRL_AF, MCU_AF_MODE_STANDBY);
	}

	info_mcu("%s : enable = %d X\n", __func__, enable);

	return 0;
}

int ois_mcu_af_set_position(struct v4l2_subdev *subdev, struct v4l2_control *ctrl)
{
	struct ois_mcu_dev *mcu = NULL;
	struct is_actuator *actuator = NULL;
	struct is_core *core;
	u32 position = 0;

	WARN_ON(!subdev);

	core = (struct is_core *)dev_get_drvdata(is_dev);
	if (!core) {
		err("core is null");
		return -EINVAL;
	}

	actuator = (struct is_actuator *)v4l2_get_subdevdata(subdev);
	WARN_ON(!actuator);

	mcu = core->mcu;
	position = ctrl->value;

	ois_mcu_af_write_position(mcu, position);

	actuator->position = position;

	dbg_mcu(1, "%s : X\n", __func__);

	return 0;
}

long ois_mcu_actuator_ioctl(struct v4l2_subdev *subdev, unsigned int cmd, void *arg)
{
	int ret = 0;
	struct v4l2_control *ctrl;

	ctrl = (struct v4l2_control *)arg;
	switch (cmd) {
	case SENSOR_IOCTL_ACT_S_CTRL:
		ret = ois_mcu_af_set_position(subdev, ctrl);
		if (ret) {
			err("mcu actuator_s_ctrl failed(%d)", ret);
			goto p_err;
		}
		break;
	case SENSOR_IOCTL_ACT_G_CTRL:
		ret = ois_mcu_af_get_position(subdev, ctrl);
		if (ret) {
			err("mcu actuator_g_ctrl failed(%d)", ret);
			goto p_err;
		}
		break;
	default:
		err("Unknown command(%#x)", cmd);
		ret = -EINVAL;
		goto p_err;
	}

p_err:
	return (long)ret;
}

#if defined(USE_TELE_OIS_AF_COMMON_INTERFACE) || defined(USE_TELE2_OIS_AF_COMMON_INTERFACE)
int ois_mcu_af_move_lens(struct is_core *core)
{
	struct ois_mcu_dev *mcu = NULL;

	mcu = core->mcu;

	info_mcu("%s : E\n", __func__);

	is_mcu_set_reg(mcu->regs[OM_REG_CORE], R_OIS_CMD_CTRL_AF, MCU_AF_MODE_ACTIVE);
#if defined(USE_TELE_OIS_AF_COMMON_INTERFACE)
	is_mcu_set_reg(mcu->regs[OM_REG_CORE], R_OIS_CMD_POS1_REAR2_AF, 0x80);
	is_mcu_set_reg(mcu->regs[OM_REG_CORE], R_OIS_CMD_POS2_REAR2_AF, 0x00);
#elif defined(USE_TELE2_OIS_AF_COMMON_INTERFACE)
	is_mcu_set_reg(mcu->regs[OM_REG_CORE], R_OIS_CMD_POS1_REAR3_AF, 0x80);
	is_mcu_set_reg(mcu->regs[OM_REG_CORE], R_OIS_CMD_POS2_REAR3_AF, 0x00);
#endif

	info_mcu("%s : X\n", __func__);

	return 0;
}
#endif

bool ois_mcu_sine_wavecheck_all(struct is_core *core,
					int threshold, int *sinx, int *siny, int *result,
					int *sinx_2nd, int *siny_2nd, int *sinx_3rd, int *siny_3rd)
{
	u8 buf[2] = {0, }, val = 0;
	int retries = 10;
	int sinx_count = 0, siny_count = 0;
	int sinx_count_2nd = 0, siny_count_2nd = 0;
#ifdef CAMERA_3RD_OIS
	int sinx_count_3rd = 0, siny_count_3rd = 0;
#endif
	u8 u8_sinx_count[2] = {0, }, u8_siny_count[2] = {0, };
	u8 u8_sinx[2] = {0, }, u8_siny[2] = {0, };
	struct ois_mcu_dev *mcu = NULL;

	mcu = core->mcu;

#ifdef CAMERA_3RD_OIS
	is_mcu_set_reg(mcu->regs[OM_REG_CORE], R_OIS_CMD_OIS_SEL, 0x07); /* OIS SEL (wide : 1 , tele : 2, tele2 : 4, w/t : 3, all : 7 ). */
#else
	is_mcu_set_reg(mcu->regs[OM_REG_CORE], R_OIS_CMD_OIS_SEL, 0x03); /* OIS SEL (wide : 1 , tele : 2, tele2 : 4, w/t : 3, all : 7 ). */
#endif
	is_mcu_set_reg(mcu->regs[OM_REG_CORE], R_OIS_CMD_THRESH_ERR_LEV, (u8)threshold); /* error threshold level. */
	is_mcu_set_reg(mcu->regs[OM_REG_CORE], R_OIS_CMD_THRESH_ERR_LEV_M2, (u8)threshold); /* error threshold level. */
#ifdef CAMERA_3RD_OIS
	is_mcu_set_reg(mcu->regs[OM_REG_CORE], R_OIS_CMD_THRESH_ERR_LEV_M3, (u8)threshold); /* error threshold level. */
#endif
	is_mcu_set_reg(mcu->regs[OM_REG_CORE], R_OIS_CMD_ERR_VAL_CNT, 0x00); /* count value for error judgement level. */
	is_mcu_set_reg(mcu->regs[OM_REG_CORE], R_OIS_CMD_FREQ_LEV, 0x05); /* frequency level for measurement. */
	is_mcu_set_reg(mcu->regs[OM_REG_CORE], R_OIS_CMD_AMPLI_LEV, 0x2A); /* amplitude level for measurement. */
	is_mcu_set_reg(mcu->regs[OM_REG_CORE], R_OIS_CMD_DUM_PULSE, 0x03); /* dummy pulse setting. */
	is_mcu_set_reg(mcu->regs[OM_REG_CORE], R_OIS_CMD_VYVLE_LEV, 0x02); /* vyvle level for measurement. */
	is_mcu_set_reg(mcu->regs[OM_REG_CORE], R_OIS_CMD_START_WAVE_CHECK, 0x01); /* start sine wave check operation */

	retries = 22;
	do {
		val = is_mcu_get_reg(mcu->regs[OM_REG_CORE], R_OIS_CMD_START_WAVE_CHECK);
		msleep(100);
		if (--retries < 0) {
			err("sine wave operation fail.\n");
			break;
		}
	} while (val);

	buf[0] = (u8)is_mcu_get_reg(mcu->regs[OM_REG_CORE], R_OIS_CMD_MCERR_W);
	buf[1] = (u8)is_mcu_get_reg(mcu->regs[OM_REG_CORE], R_OIS_CMD_MCERR_W2);

	*result = (buf[1] << 8) | buf[0];

	u8_sinx_count[0] = (u8)is_mcu_get_reg(mcu->regs[OM_REG_CORE], R_OIS_CMD_REAR_SINX_COUNT1);
	u8_sinx_count[1] = (u8)is_mcu_get_reg(mcu->regs[OM_REG_CORE], R_OIS_CMD_REAR_SINX_COUNT2);
	sinx_count = (u8_sinx_count[1] << 8) | u8_sinx_count[0];
	if (sinx_count > 0x7FFF) {
		sinx_count = -((sinx_count ^ 0xFFFF) + 1);
	}
	u8_siny_count[0] = (u8)is_mcu_get_reg(mcu->regs[OM_REG_CORE], R_OIS_CMD_REAR_SINY_COUNT1);
	u8_siny_count[1] = (u8)is_mcu_get_reg(mcu->regs[OM_REG_CORE], R_OIS_CMD_REAR_SINY_COUNT2);
	siny_count = (u8_siny_count[1] << 8) | u8_siny_count[0];
	if (siny_count > 0x7FFF) {
		siny_count = -((siny_count ^ 0xFFFF) + 1);
	}
	u8_sinx[0] = (u8)is_mcu_get_reg(mcu->regs[OM_REG_CORE], R_OIS_CMD_REAR_SINX_DIFF1);
	u8_sinx[1] = (u8)is_mcu_get_reg(mcu->regs[OM_REG_CORE], R_OIS_CMD_REAR_SINX_DIFF2);
	*sinx = (u8_sinx[1] << 8) | u8_sinx[0];
	if (*sinx > 0x7FFF) {
		*sinx = -((*sinx ^ 0xFFFF) + 1);
	}
	u8_siny[0] = (u8)is_mcu_get_reg(mcu->regs[OM_REG_CORE], R_OIS_CMD_REAR_SINY_DIFF1);
	u8_siny[1] = (u8)is_mcu_get_reg(mcu->regs[OM_REG_CORE], R_OIS_CMD_REAR_SINY_DIFF2);
	*siny = (u8_siny[1] << 8) | u8_siny[0];
	if (*siny > 0x7FFF) {
		*siny = -((*siny ^ 0xFFFF) + 1);
	}

	u8_sinx_count[0] = (u8)is_mcu_get_reg(mcu->regs[OM_REG_CORE], R_OIS_CMD_REAR2_SINX_COUNT1);
	u8_sinx_count[1] = (u8)is_mcu_get_reg(mcu->regs[OM_REG_CORE], R_OIS_CMD_REAR2_SINX_COUNT2);
	sinx_count_2nd = (u8_sinx_count[1] << 8) | u8_sinx_count[0];
	if (sinx_count_2nd > 0x7FFF) {
		sinx_count_2nd = -((sinx_count_2nd ^ 0xFFFF) + 1);
	}
	u8_siny_count[0] = (u8)is_mcu_get_reg(mcu->regs[OM_REG_CORE], R_OIS_CMD_REAR2_SINY_COUNT1);
	u8_siny_count[1] = (u8)is_mcu_get_reg(mcu->regs[OM_REG_CORE], R_OIS_CMD_REAR2_SINY_COUNT2);
	siny_count_2nd = (u8_siny_count[1] << 8) | u8_siny_count[0];
	if (siny_count_2nd > 0x7FFF) {
		siny_count_2nd = -((siny_count_2nd ^ 0xFFFF) + 1);
	}
	u8_sinx[0] = (u8)is_mcu_get_reg(mcu->regs[OM_REG_CORE], R_OIS_CMD_REAR2_SINX_DIFF1);
	u8_sinx[1] = (u8)is_mcu_get_reg(mcu->regs[OM_REG_CORE], R_OIS_CMD_REAR2_SINX_DIFF2);
	*sinx_2nd = (u8_sinx[1] << 8) | u8_sinx[0];
	if (*sinx_2nd > 0x7FFF) {
		*sinx_2nd = -((*sinx_2nd ^ 0xFFFF) + 1);
	}
	u8_siny[0] = (u8)is_mcu_get_reg(mcu->regs[OM_REG_CORE], R_OIS_CMD_REAR2_SINY_DIFF1);
	u8_siny[1] = (u8)is_mcu_get_reg(mcu->regs[OM_REG_CORE], R_OIS_CMD_REAR2_SINY_DIFF2);
	*siny_2nd = (u8_siny[1] << 8) | u8_siny[0];
	if (*siny_2nd > 0x7FFF) {
		*siny_2nd = -((*siny_2nd ^ 0xFFFF) + 1);
	}

#ifdef CAMERA_3RD_OIS
	u8_sinx_count[0] = (u8)is_mcu_get_reg(mcu->regs[OM_REG_CORE], R_OIS_CMD_REAR3_SINX_COUNT1);
	u8_sinx_count[1] = (u8)is_mcu_get_reg(mcu->regs[OM_REG_CORE], R_OIS_CMD_REAR3_SINX_COUNT2);
	sinx_count_3rd = (u8_sinx_count[1] << 8) | u8_sinx_count[0];
	if (sinx_count_3rd > 0x7FFF) {
		sinx_count_3rd = -((sinx_count_3rd ^ 0xFFFF) + 1);
	}
	u8_siny_count[0] = (u8)is_mcu_get_reg(mcu->regs[OM_REG_CORE], R_OIS_CMD_REAR3_SINY_COUNT1);
	u8_siny_count[1] = (u8)is_mcu_get_reg(mcu->regs[OM_REG_CORE], R_OIS_CMD_REAR3_SINY_COUNT2);
	siny_count_3rd = (u8_siny_count[1] << 8) | u8_siny_count[0];
	if (siny_count_3rd > 0x7FFF) {
		siny_count_3rd = -((siny_count_3rd ^ 0xFFFF) + 1);
	}
	u8_sinx[0] = (u8)is_mcu_get_reg(mcu->regs[OM_REG_CORE], R_OIS_CMD_REAR3_SINX_DIFF1);
	u8_sinx[1] = (u8)is_mcu_get_reg(mcu->regs[OM_REG_CORE], R_OIS_CMD_REAR3_SINX_DIFF2);
	*sinx_3rd = (u8_sinx[1] << 8) | u8_sinx[0];
	if (*sinx_3rd > 0x7FFF) {
		*sinx_3rd = -((*sinx_3rd ^ 0xFFFF) + 1);
	}
	u8_siny[0] = (u8)is_mcu_get_reg(mcu->regs[OM_REG_CORE], R_OIS_CMD_REAR3_SINY_DIFF1);
	u8_siny[1] = (u8)is_mcu_get_reg(mcu->regs[OM_REG_CORE], R_OIS_CMD_REAR3_SINY_DIFF2);
	*siny_3rd = (u8_siny[1] << 8) | u8_siny[0];
	if (*siny_3rd > 0x7FFF) {
		*siny_3rd = -((*siny_3rd ^ 0xFFFF) + 1);
	}

	info_mcu("%s threshold = %d, sinx_3rd = %d, siny_3rd = %d, sinx_count_3rd = %d, syny_count_3rd = %d\n",
		__func__, threshold, *sinx_3rd, *siny_3rd, sinx_count_3rd, siny_count_3rd);
#endif

	info_mcu("%s threshold = %d, sinx = %d, siny = %d, sinx_count = %d, syny_count = %d\n",
		__func__, threshold, *sinx, *siny, sinx_count, siny_count);

	info_mcu("%s threshold = %d, sinx_2nd = %d, siny_2nd = %d, sinx_count_2nd = %d, syny_count_2nd = %d\n",
		__func__, threshold, *sinx_2nd, *siny_2nd, sinx_count_2nd, siny_count_2nd);

	if (*result == 0x0) {
		return true;
	} else {
		return false;
	}
}

bool ois_mcu_auto_test_all(struct is_core *core,
					int threshold, bool *x_result, bool *y_result, int *sin_x, int *sin_y,
					bool *x_result_2nd, bool *y_result_2nd, int *sin_x_2nd, int *sin_y_2nd,
					bool *x_result_3rd, bool *y_result_3rd, int *sin_x_3rd, int *sin_y_3rd)
{
	int result = 0;
	bool value = false;

#ifdef CONFIG_AF_HOST_CONTROL
#ifdef CAMERA_2ND_OIS
#ifdef USE_TELE_OIS_AF_COMMON_INTERFACE
	ois_mcu_af_move_lens(core);
#else
	is_af_move_lens(core, SENSOR_POSITION_REAR2);
#endif
	msleep(100);
#endif
#ifdef CAMERA_3RD_OIS
#ifdef USE_TELE2_OIS_AF_COMMON_INTERFACE
	ois_mcu_af_move_lens(core);
#else
	is_af_move_lens(core, SENSOR_POSITION_REAR4);
#endif
	msleep(100);
#endif
	is_af_move_lens(core, SENSOR_POSITION_REAR);
	msleep(100);
#endif

	value = ois_mcu_sine_wavecheck_all(core, threshold, sin_x, sin_y, &result,
				sin_x_2nd, sin_y_2nd, sin_x_3rd, sin_y_3rd);

	if (*sin_x == -1 && *sin_y == -1) {
		err("OIS device is not prepared.");
		*x_result = false;
		*y_result = false;

		return false;
	}

	if (*sin_x_2nd == -1 && *sin_y_2nd == -1) {
		err("OIS 2 device is not prepared.");
		*x_result_2nd = false;
		*y_result_2nd = false;

		return false;
	}

#ifdef CAMERA_3RD_OIS
	if (*sin_x_3rd == -1 && *sin_y_3rd == -1) {
		err("OIS 3 device is not prepared.");
		*x_result_3rd = false;
		*y_result_3rd = false;

		return false;
	}
#endif

	if (value == true) {
		*x_result = true;
		*y_result = true;
		*x_result_2nd = true;
		*y_result_2nd = true;
		*x_result_3rd = true;
		*y_result_3rd = true;

		return true;
	} else {
		err("OIS autotest_2nd is failed result (0x004C) = 0x%04x\n", result);
		if ((result & 0x03) == 0x00) {
			*x_result = true;
			*y_result = true;
		} else if ((result & 0x03) == 0x01) {
			*x_result = false;
			*y_result = true;
		} else if ((result & 0x03) == 0x02) {
			*x_result = true;
			*y_result = false;
		} else {
			*x_result = false;
			*y_result = false;
		}

		if ((result & 0x30) == 0x00) {
			*x_result_2nd = true;
			*y_result_2nd = true;
		} else if ((result & 0x30) == 0x10) {
			*x_result_2nd = false;
			*y_result_2nd = true;
		} else if ((result & 0x30) == 0x20) {
			*x_result_2nd = true;
			*y_result_2nd = false;
		} else {
			*x_result_2nd = false;
			*y_result_2nd = false;
		}

		if ((result & 0x300) == 0x00) {
			*x_result_3rd = true;
			*y_result_3rd = true;
		} else if ((result & 0x300) == 0x100) {
			*x_result_3rd = false;
			*y_result_3rd = true;
		} else if ((result & 0x300) == 0x200) {
			*x_result_3rd = true;
			*y_result_3rd = false;
		} else {
			*x_result_3rd = false;
			*y_result_3rd = false;
		}

		return false;
	}
}

#ifdef CAMERA_2ND_OIS
bool ois_mcu_sine_wavecheck_rear2(struct is_core *core,
					int threshold, int *sinx, int *siny, int *result,
					int *sinx_2nd, int *siny_2nd)
{
	u8 buf = 0, val = 0;
	int retries = 10;
	int sinx_count = 0, siny_count = 0;
	int sinx_count_2nd = 0, siny_count_2nd = 0;
	u8 u8_sinx_count[2] = {0, }, u8_siny_count[2] = {0, };
	u8 u8_sinx[2] = {0, }, u8_siny[2] = {0, };
	struct ois_mcu_dev *mcu = NULL;

	mcu = core->mcu;

	is_mcu_set_reg(mcu->regs[OM_REG_CORE], R_OIS_CMD_OIS_SEL, 0x03); /* OIS SEL (wide : 1 , tele : 2, both : 3 ). */
	is_mcu_set_reg(mcu->regs[OM_REG_CORE], R_OIS_CMD_THRESH_ERR_LEV, (u8)threshold); /* error threshold level. */
	is_mcu_set_reg(mcu->regs[OM_REG_CORE], R_OIS_CMD_THRESH_ERR_LEV_M2, (u8)threshold); /* error threshold level. */
	is_mcu_set_reg(mcu->regs[OM_REG_CORE], R_OIS_CMD_ERR_VAL_CNT, 0x00); /* count value for error judgement level. */
	is_mcu_set_reg(mcu->regs[OM_REG_CORE], R_OIS_CMD_FREQ_LEV, 0x05); /* frequency level for measurement. */
	is_mcu_set_reg(mcu->regs[OM_REG_CORE], R_OIS_CMD_AMPLI_LEV, 0x2A); /* amplitude level for measurement. */
	is_mcu_set_reg(mcu->regs[OM_REG_CORE], R_OIS_CMD_DUM_PULSE, 0x03); /* dummy pulse setting. */
	is_mcu_set_reg(mcu->regs[OM_REG_CORE], R_OIS_CMD_VYVLE_LEV, 0x02); /* vyvle level for measurement. */
	is_mcu_set_reg(mcu->regs[OM_REG_CORE], R_OIS_CMD_START_WAVE_CHECK, 0x01); /* start sine wave check operation */

	retries = 22;
	do {
		val = is_mcu_get_reg(mcu->regs[OM_REG_CORE], R_OIS_CMD_START_WAVE_CHECK);
		msleep(100);
		if (--retries < 0) {
			err("sine wave operation fail.\n");
			break;
		}
	} while (val);

	buf = (u8)is_mcu_get_reg(mcu->regs[OM_REG_CORE], R_OIS_CMD_MCERR_W);

	*result = (int)buf;

	u8_sinx_count[0] = (u8)is_mcu_get_reg(mcu->regs[OM_REG_CORE], R_OIS_CMD_REAR_SINX_COUNT1);
	u8_sinx_count[1] = (u8)is_mcu_get_reg(mcu->regs[OM_REG_CORE], R_OIS_CMD_REAR_SINX_COUNT2);
	sinx_count = (u8_sinx_count[1] << 8) | u8_sinx_count[0];
	if (sinx_count > 0x7FFF) {
		sinx_count = -((sinx_count ^ 0xFFFF) + 1);
	}
	u8_siny_count[0] = (u8)is_mcu_get_reg(mcu->regs[OM_REG_CORE], R_OIS_CMD_REAR_SINY_COUNT1);
	u8_siny_count[1] = (u8)is_mcu_get_reg(mcu->regs[OM_REG_CORE], R_OIS_CMD_REAR_SINY_COUNT2);
	siny_count = (u8_siny_count[1] << 8) | u8_siny_count[0];
	if (siny_count > 0x7FFF) {
		siny_count = -((siny_count ^ 0xFFFF) + 1);
	}
	u8_sinx[0] = (u8)is_mcu_get_reg(mcu->regs[OM_REG_CORE], R_OIS_CMD_REAR_SINX_DIFF1);
	u8_sinx[1] = (u8)is_mcu_get_reg(mcu->regs[OM_REG_CORE], R_OIS_CMD_REAR_SINX_DIFF2);
	*sinx = (u8_sinx[1] << 8) | u8_sinx[0];
	if (*sinx > 0x7FFF) {
		*sinx = -((*sinx ^ 0xFFFF) + 1);
	}
	u8_siny[0] = (u8)is_mcu_get_reg(mcu->regs[OM_REG_CORE], R_OIS_CMD_REAR_SINY_DIFF1);
	u8_siny[1] = (u8)is_mcu_get_reg(mcu->regs[OM_REG_CORE], R_OIS_CMD_REAR_SINY_DIFF2);
	*siny = (u8_siny[1] << 8) | u8_siny[0];
	if (*siny > 0x7FFF) {
		*siny = -((*siny ^ 0xFFFF) + 1);
	}

	u8_sinx_count[0] = (u8)is_mcu_get_reg(mcu->regs[OM_REG_CORE], R_OIS_CMD_REAR2_SINX_COUNT1);
	u8_sinx_count[1] = (u8)is_mcu_get_reg(mcu->regs[OM_REG_CORE], R_OIS_CMD_REAR2_SINX_COUNT2);
	sinx_count_2nd = (u8_sinx_count[1] << 8) | u8_sinx_count[0];
	if (sinx_count_2nd > 0x7FFF) {
		sinx_count_2nd = -((sinx_count_2nd ^ 0xFFFF) + 1);
	}
	u8_siny_count[0] = (u8)is_mcu_get_reg(mcu->regs[OM_REG_CORE], R_OIS_CMD_REAR2_SINY_COUNT1);
	u8_siny_count[1] = (u8)is_mcu_get_reg(mcu->regs[OM_REG_CORE], R_OIS_CMD_REAR2_SINY_COUNT2);
	siny_count_2nd = (u8_siny_count[1] << 8) | u8_siny_count[0];
	if (siny_count_2nd > 0x7FFF) {
		siny_count_2nd = -((siny_count_2nd ^ 0xFFFF) + 1);
	}
	u8_sinx[0] = (u8)is_mcu_get_reg(mcu->regs[OM_REG_CORE], R_OIS_CMD_REAR2_SINX_DIFF1);
	u8_sinx[1] = (u8)is_mcu_get_reg(mcu->regs[OM_REG_CORE], R_OIS_CMD_REAR2_SINX_DIFF2);
	*sinx_2nd = (u8_sinx[1] << 8) | u8_sinx[0];
	if (*sinx_2nd > 0x7FFF) {
		*sinx_2nd = -((*sinx_2nd ^ 0xFFFF) + 1);
	}
	u8_siny[0] = (u8)is_mcu_get_reg(mcu->regs[OM_REG_CORE], R_OIS_CMD_REAR2_SINY_DIFF1);
	u8_siny[1] = (u8)is_mcu_get_reg(mcu->regs[OM_REG_CORE], R_OIS_CMD_REAR2_SINY_DIFF2);
	*siny_2nd = (u8_siny[1] << 8) | u8_siny[0];
	if (*siny_2nd > 0x7FFF) {
		*siny_2nd = -((*siny_2nd ^ 0xFFFF) + 1);
	}

	info_mcu("threshold = %d, sinx = %d, siny = %d, sinx_count = %d, syny_count = %d\n",
		threshold, *sinx, *siny, sinx_count, siny_count);

	info_mcu("threshold = %d, sinx_2nd = %d, siny_2nd = %d, sinx_count_2nd = %d, syny_count_2nd = %d\n",
		threshold, *sinx_2nd, *siny_2nd, sinx_count_2nd, siny_count_2nd);

	if (buf == 0x0) {
		return true;
	} else {
		return false;
	}
}

bool ois_mcu_auto_test_rear2(struct is_core *core,
					int threshold, bool *x_result, bool *y_result, int *sin_x, int *sin_y,
					bool *x_result_2nd, bool *y_result_2nd, int *sin_x_2nd, int *sin_y_2nd)
{
	int result = 0;
	bool value = false;

#ifdef CONFIG_AF_HOST_CONTROL
#ifdef CAMERA_2ND_OIS
#ifdef USE_TELE_OIS_AF_COMMON_INTERFACE
	ois_mcu_af_move_lens(core);
#else
	is_af_move_lens(core, SENSOR_POSITION_REAR2);
#endif
	msleep(100);
#endif
#ifdef CAMERA_3RD_OIS
#ifdef USE_TELE2_OIS_AF_COMMON_INTERFACE
	ois_mcu_af_move_lens(core);
#else
	is_af_move_lens(core, SENSOR_POSITION_REAR4);
#endif
	msleep(100);
#endif
	is_af_move_lens(core, SENSOR_POSITION_REAR);
	msleep(100);
#endif

	value = ois_mcu_sine_wavecheck_rear2(core, threshold, sin_x, sin_y, &result,
				sin_x_2nd, sin_y_2nd);

	if (*sin_x == -1 && *sin_y == -1) {
		err("OIS device is not prepared.");
		*x_result = false;
		*y_result = false;

		return false;
	}

	if (*sin_x_2nd == -1 && *sin_y_2nd == -1) {
		err("OIS 2 device is not prepared.");
		*x_result_2nd = false;
		*y_result_2nd = false;

		return false;
	}

	if (value == true) {
		*x_result = true;
		*y_result = true;
		*x_result_2nd = true;
		*y_result_2nd = true;

		return true;
	} else {
		err("OIS autotest_2nd is failed result (0x0051) = 0x%x\n", result);
		if ((result & 0x03) == 0x00) {
			*x_result = true;
			*y_result = true;
		} else if ((result & 0x03) == 0x01) {
			*x_result = false;
			*y_result = true;
		} else if ((result & 0x03) == 0x02) {
			*x_result = true;
			*y_result = false;
		} else {
			*x_result = false;
			*y_result = false;
		}

		if ((result & 0x30) == 0x00) {
			*x_result_2nd = true;
			*y_result_2nd = true;
		} else if ((result & 0x30) == 0x10) {
			*x_result_2nd = false;
			*y_result_2nd = true;
		} else if ((result & 0x30) == 0x20) {
			*x_result_2nd = true;
			*y_result_2nd = false;
		} else {
			*x_result_2nd = false;
			*y_result_2nd = false;
		}

		return false;
	}
}

void ois_mcu_device_ctrl(struct ois_mcu_dev *mcu)
{
	is_mcu_set_reg(mcu->regs[OM_REG_CORE], R_OIS_CMD_DEVCTRL, 0x01);
}

int ois_mcu_set_power_mode(struct v4l2_subdev *subdev)
{
	struct is_ois *ois = NULL;
	struct ois_mcu_dev *mcu = NULL;
	struct is_mcu *is_mcu = NULL;
	u8 val = 0;
	int retry = 200;
	bool camera_running;
	bool camera_running2;
#ifdef CAMERA_3RD_OIS
	bool camera_running4;
#endif

	mcu = (struct ois_mcu_dev*)v4l2_get_subdevdata(subdev);
	if(!mcu) {
		err("%s, mcu subdev is NULL", __func__);
		return -EINVAL;
	}

	is_mcu = (struct is_mcu *)v4l2_get_subdev_hostdata(subdev);
	if(!is_mcu) {
		err("%s, is_mcu is NULL", __func__);
		return -EINVAL;
	}

	ois = is_mcu->ois;
	if(!ois) {
		err("%s, ois subdev is NULL", __func__);
		return -EINVAL;
	}

	info_mcu("%s : E\n", __func__);

#if defined(CONFIG_SEC_FACTORY) //Factory timing issue.
	retry = 600;
#endif

	if (!(ois_wide_init || ois_tele_init || ois_tele2_init) && (mcu->dev_ctrl_state == false)) {
		ois_mcu_device_ctrl(mcu);
		do {
			usleep_range(500, 510);
			val = is_mcu_get_reg(mcu->regs[OM_REG_CORE], R_OIS_CMD_DEVCTRL);
			if (--retry < 0) {
				err_mcu("%s Read status failed!!!!, data = 0x%04x\n", __func__, val);
				break;
			}
		} while (val != 0x00);

		if (val == 0x00) {
			mcu->dev_ctrl_state = true;
			info_mcu("%s dev ctrl done.", __func__);
		}
	}

	camera_running = is_vendor_check_camera_running(SENSOR_POSITION_REAR);
	camera_running2 = is_vendor_check_camera_running(SENSOR_POSITION_REAR2);

#ifdef CAMERA_3RD_OIS
	camera_running4 = is_vendor_check_camera_running(SENSOR_POSITION_REAR4);
	mcu->recover_tele2 = false;

	/* OIS SEL (wide : 1 , tele : 2, tele2 : 4, triple : 7 ). */
	if (camera_running && !camera_running2 && !camera_running4) { //TEMP_OLYMPUS ==> need to be changed based on camera scenario
		is_mcu_set_reg(mcu->regs[OM_REG_CORE], R_OIS_CMD_OIS_SEL, 0x01);
		ois->ois_power_mode = OIS_POWER_MODE_SINGLE_WIDE;
	} else if (!camera_running && camera_running2 && !camera_running4) {
		is_mcu_set_reg(mcu->regs[OM_REG_CORE], R_OIS_CMD_OIS_SEL, 0x02);
		ois->ois_power_mode = OIS_POWER_MODE_SINGLE_TELE;
	} else if (camera_running && camera_running2 && !camera_running4) {
		is_mcu_set_reg(mcu->regs[OM_REG_CORE], R_OIS_CMD_OIS_SEL, 0x03);
		ois->ois_power_mode = OIS_POWER_MODE_DUAL;
	} else if (!camera_running && !camera_running2 && camera_running4) {
		is_mcu_set_reg(mcu->regs[OM_REG_CORE], R_OIS_CMD_OIS_SEL, 0x04);
		ois->ois_power_mode = OIS_POWER_MODE_SINGLE_TELE2;
		mcu->recover_tele2 = true;
	} else {
		is_mcu_set_reg(mcu->regs[OM_REG_CORE], R_OIS_CMD_OIS_SEL, 0x07);
		ois->ois_power_mode = OIS_POWER_MODE_TRIPLE;
		mcu->recover_tele2 = true;
	}
#else
	/* OIS SEL (wide : 1 , tele : 2, both : 3 ). */
	if (camera_running && !camera_running2) {
		is_mcu_set_reg(mcu->regs[OM_REG_CORE], R_OIS_CMD_OIS_SEL, 0x01);
		ois->ois_power_mode = OIS_POWER_MODE_SINGLE_WIDE;
	} else if (!camera_running && camera_running2) {
		is_mcu_set_reg(mcu->regs[OM_REG_CORE], R_OIS_CMD_OIS_SEL, 0x02);
		ois->ois_power_mode = OIS_POWER_MODE_SINGLE_TELE;
	} else {
		is_mcu_set_reg(mcu->regs[OM_REG_CORE], R_OIS_CMD_OIS_SEL, 0x03);
		ois->ois_power_mode = OIS_POWER_MODE_DUAL;
	}
#endif

	info_mcu("%s ois power setting is %d X\n", __func__, ois->ois_power_mode);

	return 0;
}
#endif

void ois_mcu_enable(struct is_core *core)
{
	struct ois_mcu_dev *mcu = NULL;

	mcu = core->mcu;

	info_mcu("%s : E\n", __func__);

	is_mcu_set_reg(mcu->regs[OM_REG_CORE], R_OIS_CMD_MODE, 0x00);

	is_mcu_set_reg(mcu->regs[OM_REG_CORE], R_OIS_CMD_START, 0x01);

	info_mcu("%s : X\n", __func__);
}

int ois_mcu_disable(struct v4l2_subdev *subdev)
{
	struct ois_mcu_dev *mcu = NULL;

	info_mcu("%s : E\n", __func__);

	mcu = (struct ois_mcu_dev*)v4l2_get_subdevdata(subdev);
	if(!mcu) {
		err("%s, mcu subdev is NULL", __func__);
		return -EINVAL;
	}

	if (ois_hw_check) {
#ifdef CONFIG_USE_OIS_TAMODE_CONTROL
		if (ois_tamode_onoff && ois_tamode_status) {
			is_mcu_set_reg(mcu->regs[OM_REG_CORE], R_OIS_CMD_TAMODE, 0x00);
			ois_tamode_status = false;
			ois_tamode_onoff = false;
		}
#endif
		is_mcu_set_reg(mcu->regs[OM_REG_CORE], R_OIS_CMD_START, 0x00);
		usleep_range(2000, 2100);

		ois_fadeupdown = false;
		ois_hw_check = false;

		/* off all ois */
		ois_wide_init = false;
		ois_tele_init = false;
		ois_tele2_init = false;

		info_mcu("%s ois stop.X\n", __func__);
	}

	info_mcu("%s : X\n", __func__);

	return 0;
}

void ois_mcu_get_hall_position(struct is_core *core, u16 *targetPos, u16 *hallPos)
{
	struct ois_mcu_dev *mcu = NULL;
	u8 pos_temp[2] = {0, };
	u16 pos = 0;

	mcu = core->mcu;

	info_mcu("%s : E\n", __func__);

	/* set centering mode */
	is_mcu_set_reg(mcu->regs[OM_REG_CORE], R_OIS_CMD_MODE, 0x05);

	/* enable position data read */
	is_mcu_set_reg(mcu->regs[OM_REG_CORE], R_OIS_CMD_FWINFO_CTRL, 0x01);

	msleep(150);

	pos_temp[0] = is_mcu_get_reg(mcu->regs[OM_REG_CORE], R_OIS_CMD_TARGET_POS_REAR_X);
	pos_temp[1] = is_mcu_get_reg(mcu->regs[OM_REG_CORE], R_OIS_CMD_TARGET_POS_REAR_X2);
	pos = (pos_temp[1] << 8) | pos_temp[0];
	targetPos[0] = pos;

	pos_temp[0] = is_mcu_get_reg(mcu->regs[OM_REG_CORE], R_OIS_CMD_TARGET_POS_REAR_Y);
	pos_temp[1] = is_mcu_get_reg(mcu->regs[OM_REG_CORE], R_OIS_CMD_TARGET_POS_REAR_Y2);
	pos = (pos_temp[1] << 8) | pos_temp[0];
	targetPos[1] = pos;

	pos_temp[0] = is_mcu_get_reg(mcu->regs[OM_REG_CORE], R_OIS_CMD_HALL_POS_REAR_X);
	pos_temp[1] = is_mcu_get_reg(mcu->regs[OM_REG_CORE], R_OIS_CMD_HALL_POS_REAR_X2);
	pos = (pos_temp[1] << 8) | pos_temp[0];
	hallPos[0] = pos;

	pos_temp[0] = is_mcu_get_reg(mcu->regs[OM_REG_CORE], R_OIS_CMD_HALL_POS_REAR_Y);
	pos_temp[1] = is_mcu_get_reg(mcu->regs[OM_REG_CORE], R_OIS_CMD_HALL_POS_REAR_Y2);
	pos = (pos_temp[1] << 8) | pos_temp[0];
	hallPos[1] = pos;

	info_mcu("%s : wide pos = 0x%04x, 0x%04x, 0x%04x, 0x%04x\n", __func__, targetPos[0], targetPos[1], hallPos[0], hallPos[1]);

	pos_temp[0] = is_mcu_get_reg(mcu->regs[OM_REG_CORE], R_OIS_CMD_TARGET_POS_REAR2_X);
	pos_temp[1] = is_mcu_get_reg(mcu->regs[OM_REG_CORE], R_OIS_CMD_TARGET_POS_REAR2_X2);
	pos = (pos_temp[1] << 8) | pos_temp[0];
	targetPos[2] = pos;

	pos_temp[0] = is_mcu_get_reg(mcu->regs[OM_REG_CORE], R_OIS_CMD_TARGET_POS_REAR2_Y);
	pos_temp[1] = is_mcu_get_reg(mcu->regs[OM_REG_CORE], R_OIS_CMD_TARGET_POS_REAR2_Y2);
	pos = (pos_temp[1] << 8) | pos_temp[0];
	targetPos[3] = pos;

	pos_temp[0] = is_mcu_get_reg(mcu->regs[OM_REG_CORE], R_OIS_CMD_HALL_POS_REAR2_X);
	pos_temp[1] = is_mcu_get_reg(mcu->regs[OM_REG_CORE], R_OIS_CMD_HALL_POS_REAR2_X2);
	pos = (pos_temp[1] << 8) | pos_temp[0];
	hallPos[2] = pos;

	pos_temp[0] = is_mcu_get_reg(mcu->regs[OM_REG_CORE], R_OIS_CMD_HALL_POS_REAR2_Y);
	pos_temp[1] = is_mcu_get_reg(mcu->regs[OM_REG_CORE], R_OIS_CMD_HALL_POS_REAR2_Y2);
	pos = (pos_temp[1] << 8) | pos_temp[0];
	hallPos[3] = pos;

	info_mcu("%s : tele pos = 0x%04x, 0x%04x, 0x%04x, 0x%04x\n", __func__, targetPos[2], targetPos[3], hallPos[2], hallPos[3]);

#ifdef CAMERA_3RD_OIS
	pos_temp[0] = is_mcu_get_reg(mcu->regs[OM_REG_CORE], R_OIS_CMD_TARGET_POS_REAR3_X);
	pos_temp[1] = is_mcu_get_reg(mcu->regs[OM_REG_CORE], R_OIS_CMD_TARGET_POS_REAR3_X2);
	pos = (pos_temp[1] << 8) | pos_temp[0];
	targetPos[4] = pos;

	pos_temp[0] = is_mcu_get_reg(mcu->regs[OM_REG_CORE], R_OIS_CMD_TARGET_POS_REAR3_Y);
	pos_temp[1] = is_mcu_get_reg(mcu->regs[OM_REG_CORE], R_OIS_CMD_TARGET_POS_REAR3_Y2);
	pos = (pos_temp[1] << 8) | pos_temp[0];
	targetPos[5] = pos;

	pos_temp[0] = is_mcu_get_reg(mcu->regs[OM_REG_CORE], R_OIS_CMD_HALL_POS_REAR3_X);
	pos_temp[1] = is_mcu_get_reg(mcu->regs[OM_REG_CORE], R_OIS_CMD_HALL_POS_REAR3_X2);
	pos = (pos_temp[1] << 8) | pos_temp[0];
	hallPos[4] = pos;

	pos_temp[0] = is_mcu_get_reg(mcu->regs[OM_REG_CORE], R_OIS_CMD_HALL_POS_REAR3_Y);
	pos_temp[1] = is_mcu_get_reg(mcu->regs[OM_REG_CORE], R_OIS_CMD_HALL_POS_REAR3_Y2);
	pos = (pos_temp[1] << 8) | pos_temp[0];
	hallPos[5] = pos;

	info_mcu("%s : tele2 pos = 0x%04x, 0x%04x, 0x%04x, 0x%04x\n", __func__, targetPos[4], targetPos[5], hallPos[4], hallPos[5]);
#endif

	/* disable position data read */
	is_mcu_set_reg(mcu->regs[OM_REG_CORE], R_OIS_CMD_FWINFO_CTRL, 0x00);

	info_mcu("%s : X\n", __func__);
}

bool ois_mcu_offset_test(struct is_core *core, long *raw_data_x, long *raw_data_y)
{
	int i = 0;
	u8 val = 0, x = 0, y = 0;
	int x_sum = 0, y_sum = 0, sum = 0;
	int retries = 0, avg_count = 30;
	bool result = false;
	int scale_factor = OIS_GYRO_SCALE_FACTOR_LSM6DSO;
	struct ois_mcu_dev *mcu = NULL;

	mcu = core->mcu;

	info_mcu("%s : E\n", __func__);

	is_mcu_set_reg(mcu->regs[OM_REG_CORE], R_OIS_CMD_GYRO_CAL, 0x01);

	retries = avg_count;
	do {
		val = (u8)is_mcu_get_reg(mcu->regs[OM_REG_CORE], R_OIS_CMD_GYRO_CAL);
		 msleep(50);
		if (--retries < 0) {
			err("Read register failed!!!!, data = 0x%04x\n", val);
			break;
		}
	} while (val);

	/* Gyro result check */
	val = (u8)is_mcu_get_reg(mcu->regs[OM_REG_CORE], R_OIS_CMD_GYRO_RESULT);

	if ((val & 0x23) == 0x0) {
		info_mcu("[%s] Gyro result check success. Result is OK. gyro value = 0x%02x", __func__, val);
		result = true;
	} else {
		info_mcu("[%s] Gyro result check fail. Result is NG. gyro value = 0x%02x", __func__, val);
		result = false;
	}

	sum = 0;
	retries = avg_count;
	for (i = 0; i < retries; retries--) {
		val = (u8)is_mcu_get_reg(mcu->regs[OM_REG_CORE], R_OIS_CMD_RAW_DEBUG_X1);
		x = val;
		val = (u8)is_mcu_get_reg(mcu->regs[OM_REG_CORE], R_OIS_CMD_RAW_DEBUG_X2);
		x_sum = (val << 8) | x;
		if (x_sum > 0x7FFF) {
			x_sum = -((x_sum ^ 0xFFFF) + 1);
		}
		sum += x_sum;
	}
	sum = sum * 10 / avg_count;
	*raw_data_x = sum * 1000 / scale_factor / 10;

	sum = 0;
	retries = avg_count;
	for (i = 0; i < retries; retries--) {
		val = (u8)is_mcu_get_reg(mcu->regs[OM_REG_CORE], R_OIS_CMD_RAW_DEBUG_Y1);
		y = val;
		val = (u8)is_mcu_get_reg(mcu->regs[OM_REG_CORE], R_OIS_CMD_RAW_DEBUG_Y2);
		y_sum = (val << 8) | y;
		if (y_sum > 0x7FFF) {
			y_sum = -((y_sum ^ 0xFFFF) + 1);
		}
		sum += y_sum;
	}
	sum = sum * 10 / avg_count;
	*raw_data_y = sum * 1000 / scale_factor / 10;

	//is_mcu_fw_version(core); // TEMP_2020
	info_mcu("%s : X raw_x = %ld, raw_y = %ld\n", __func__, *raw_data_x, *raw_data_y);

	return result;
}

void ois_mcu_get_offset_data(struct is_core *core, long *raw_data_x, long *raw_data_y)
{
	u8 val = 0;
	int retries = 0, avg_count = 40;
	struct ois_mcu_dev *mcu = NULL;

	mcu = core->mcu;

	info_mcu("%s : E\n", __func__);

	/* check ois status */
	retries = avg_count;
	do {
		val = (u8)is_mcu_get_reg(mcu->regs[OM_REG_CORE], R_OIS_CMD_STATUS);
		msleep(50);
		if (--retries < 0) {
			err_mcu("%s Read status failed!!!!, data = 0x%04x\n", __func__, val);
			break;
		}
	} while (val != 0x01);

	//is_mcu_fw_version(core); // TEMP_2020

	ois_mcu_get_efs_data(mcu, raw_data_x, raw_data_y);

	return;
}

int ois_mcu_bypass_read(struct ois_mcu_dev *mcu, u16 id, u16 reg, u8 reg_size, u8 *buf, u8 data_size)
{
	u8 mode = 0;
	u8 rcvdata = 0;
	u8 dev_id[2] = {0, };
	u8 reg_add[2] = {0, };
	int retries = 1000;
	int i = 0;

	info_mcu("%s E\n", __func__);

	/* device id */
	dev_id[0] = id & 0xFF;
	dev_id[1] = (id >> 8) & 0xFF;
	is_mcu_set_reg(mcu->regs[OM_REG_CORE], R_OIS_CMD_BYPASS_DEVICE_ID1, dev_id[0]);
	is_mcu_set_reg(mcu->regs[OM_REG_CORE], R_OIS_CMD_BYPASS_DEVICE_ID2, dev_id[1]);

	/* register address */
	reg_add[0] = reg & 0xFF;
	reg_add[1] = (reg >> 8) & 0xFF;
	is_mcu_set_reg(mcu->regs[OM_REG_CORE], R_OIS_CMD_BYPASS_REG_ADD1, reg_add[0]);
	is_mcu_set_reg(mcu->regs[OM_REG_CORE], R_OIS_CMD_BYPASS_REG_ADD2, reg_add[1]);

	/* reg size */
	is_mcu_set_reg(mcu->regs[OM_REG_CORE], R_OIS_CMD_BYPASS_REG_SIZE, reg_size);

	/* data size */
	is_mcu_set_reg(mcu->regs[OM_REG_CORE], R_OIS_CMD_BYPASS_DATA_SIZE, data_size);

	/* run bypass mode */
	mode = 0x02;
	is_mcu_set_reg(mcu->regs[OM_REG_CORE], R_OIS_CMD_BYPASS_CTRL, mode);

	do {
		rcvdata = is_mcu_get_reg(mcu->regs[OM_REG_CORE], R_OIS_CMD_BYPASS_CTRL);
		usleep_range(1000, 1100);
		if (--retries < 0) {
			err_mcu("%s read status failed!!!!, data = 0x%04x\n", __func__, rcvdata);
			break;
		}
	} while (rcvdata != 0x00);

	/* get data */
	for (i = 0; i < data_size; i++) {
		rcvdata = is_mcu_get_reg(mcu->regs[OM_REG_CORE], R_OIS_CMD_BYPASS_DATA_TRANSFER + i);
		*(buf + i) = rcvdata & 0xFF;
	}

	info_mcu("%s X\n", __func__);

	return 0;
}

int ois_mcu_bypass_write(struct ois_mcu_dev *mcu, u16 id, u16 reg, u8 reg_size, u8 *buf, u8 data_size)
{
	u8 mode = 0;
	u8 rcvdata = 0;
	u8 dev_id[2] = {0, };
	u8 reg_add[2] = {0, };
	int retries = 1000;
	int i = 0;

	info_mcu("%s E\n", __func__);

	/* device id */
	dev_id[0] = id & 0xFF;
	dev_id[1] = (id >> 8) & 0xFF;
	is_mcu_set_reg(mcu->regs[OM_REG_CORE], R_OIS_CMD_BYPASS_DEVICE_ID1, dev_id[0]);
	is_mcu_set_reg(mcu->regs[OM_REG_CORE], R_OIS_CMD_BYPASS_DEVICE_ID2, dev_id[1]);

	/* register address */
	reg_add[0] = reg& 0xFF;
	reg_add[1] = (reg >> 8) & 0xFF;
	is_mcu_set_reg(mcu->regs[OM_REG_CORE], R_OIS_CMD_BYPASS_REG_ADD1, reg_add[0]);
	is_mcu_set_reg(mcu->regs[OM_REG_CORE], R_OIS_CMD_BYPASS_REG_ADD2, reg_add[1]);

	/* reg size */
	is_mcu_set_reg(mcu->regs[OM_REG_CORE], R_OIS_CMD_BYPASS_REG_SIZE, reg_size);

	/* data size */
	is_mcu_set_reg(mcu->regs[OM_REG_CORE], R_OIS_CMD_BYPASS_DATA_SIZE, data_size);

	/* send data */
	for (i = 0; i < data_size; i++) {
		is_mcu_set_reg(mcu->regs[OM_REG_CORE], R_OIS_CMD_BYPASS_DATA_TRANSFER + i, *(buf + i) & 0xFF);
	}

	/* run bypass mode */
	mode = 0x02;
	is_mcu_set_reg(mcu->regs[OM_REG_CORE], R_OIS_CMD_BYPASS_CTRL, mode);

	do {
		rcvdata = is_mcu_get_reg(mcu->regs[OM_REG_CORE], R_OIS_CMD_BYPASS_CTRL);
		usleep_range(1000, 1100);
		if (--retries < 0) {
			err_mcu("%s read status failed!!!!, data = 0x%04x\n", __func__, rcvdata);
			break;
		}
		i++;
	} while (rcvdata != 0x00);

	info_mcu("%s X\n", __func__);

	return 0;
}

int ois_mcu_check_cross_talk(struct v4l2_subdev *subdev, u16 *hall_data)
{
	int ret = 0;
	u8 val = 0;
	u16 x_target = 0;
	int retries = 600;
	u8 addr_size = 0x02;
	u8 data[2] = {0, };
	u8 hall_value[2] = {0, };
	int i = 0;
	struct ois_mcu_dev *mcu = NULL;

	WARN_ON(!subdev);

	info_mcu("%s E\n", __func__);

	mcu = (struct ois_mcu_dev *)v4l2_get_subdevdata(subdev);
	if(!mcu) {
		err_mcu("%s, mcu is NULL", __func__);
		ret = -EINVAL;
		return ret;
	}

	do {
		val = is_mcu_get_reg(mcu->regs[OM_REG_CORE], R_OIS_CMD_STATUS);
		usleep_range(500, 510);
		if (--retries < 0) {
			err_mcu("%s Read status failed!!!!, data = 0x%04x\n", __func__, val);
			break;
		}
	} while (val != 0x01);

	data[0] = 0x08;
	ois_mcu_bypass_write(mcu, MCU_BYPASS_MODE_WRITE_ID, 0x0002, addr_size, data, 0x01);
	data[0] = 0x01;
	ois_mcu_bypass_write(mcu, MCU_BYPASS_MODE_WRITE_ID, 0x0080, addr_size, data, 0x01);
	data[0] = 0x01;
	ois_mcu_bypass_write(mcu, MCU_BYPASS_MODE_WRITE_ID, 0x0000, addr_size, data, 0x01);

	data[0] = 0x20;
	data[1] = 0x03;
	ois_mcu_bypass_write(mcu, MCU_BYPASS_MODE_WRITE_ID, 0x0022, addr_size, data, 0x02);
	data[0] = 0x00;
	data[1] = 0x08;
	ois_mcu_bypass_write(mcu, MCU_BYPASS_MODE_WRITE_ID, 0x0024, addr_size, data, 0x02);

	x_target = 800;
	for (i = 0; i < 10; i++) {
		data[0] = x_target & 0xFF;
		data[1] = (x_target >> 8) & 0xFF;
		ois_mcu_bypass_write(mcu, MCU_BYPASS_MODE_WRITE_ID, 0x0022, addr_size, data, 0x02);
		msleep(45);

		ois_mcu_bypass_read(mcu, MCU_BYPASS_MODE_READ_ID, 0x0090, addr_size, hall_value, 0x02);
		*(hall_data + i) = (hall_value[1] << 8) | hall_value[0];
		info_mcu("%s hall_data[0] = 0x%02x, hall_value[1] = 0x%02x", __func__, hall_value[0], hall_value[1]);
		x_target += 300;
	}

	info_mcu("%s  X\n", __func__);

	return ret;
}

int ois_mcu_bypass_read_mode1(struct ois_mcu_dev *mcu, u8 id, u8 reg, u8 *buf, u8 data_size)
{
	u8 mode = 0;
	u8 rcvdata = 0;
	int retries = 1000;
	int i = 0;

	info_mcu("%s E\n", __func__);

	/* device id */
	is_mcu_set_reg(mcu->regs[OM_REG_CORE], R_OIS_CMD_BYPASS_DEVICE_ID1, id);

	/* register address */
	is_mcu_set_reg(mcu->regs[OM_REG_CORE], R_OIS_CMD_BYPASS_DEVICE_ID2, reg);

	/* data size */
	is_mcu_set_reg(mcu->regs[OM_REG_CORE], R_OIS_CMD_BYPASS_REG_ADD1, data_size);

	/* run bypass mode */
	mode = 0x01;
	is_mcu_set_reg(mcu->regs[OM_REG_CORE], R_OIS_CMD_BYPASS_CTRL, mode);

	do {
		rcvdata = is_mcu_get_reg(mcu->regs[OM_REG_CORE], R_OIS_CMD_BYPASS_CTRL);
		usleep_range(1000, 1100);
		if (--retries < 0) {
			err_mcu("%s read status failed!!!!, data = 0x%04x\n", __func__, rcvdata);
			break;
		}
	} while (rcvdata != 0x00);

	/* get data */
	for (i = 0; i < data_size; i++) {
		rcvdata = is_mcu_get_reg(mcu->regs[OM_REG_CORE], R_OIS_CMD_BYPASS_REG_ADD2 + i);
		*(buf + i) = rcvdata & 0xFF;
	}

	info_mcu("%s X\n", __func__);

	return 0;
}

int ois_mcu_bypass_write_mode1(struct ois_mcu_dev *mcu, u8 id, u8 reg, u8 *buf, u8 data_size)
{
	u8 mode = 0;
	u8 rcvdata = 0;
	int retries = 1000;
	int i = 0;

	info_mcu("%s E\n", __func__);

	/* device id */
	is_mcu_set_reg(mcu->regs[OM_REG_CORE], R_OIS_CMD_BYPASS_DEVICE_ID1, id);

	/* register address */
	is_mcu_set_reg(mcu->regs[OM_REG_CORE], R_OIS_CMD_BYPASS_DEVICE_ID2, reg);

	/* data size */
	is_mcu_set_reg(mcu->regs[OM_REG_CORE], R_OIS_CMD_BYPASS_REG_ADD1, data_size);

	/* send data */
	for (i = 0; i < data_size; i++) {
		is_mcu_set_reg(mcu->regs[OM_REG_CORE], R_OIS_CMD_BYPASS_REG_ADD2 + i, *(buf + i) & 0xFF);
	}

	/* run bypass mode */
	mode = 0x01;
	is_mcu_set_reg(mcu->regs[OM_REG_CORE], R_OIS_CMD_BYPASS_CTRL, mode);

	do {
		rcvdata = is_mcu_get_reg(mcu->regs[OM_REG_CORE], R_OIS_CMD_BYPASS_CTRL);
		usleep_range(1000, 1100);
		if (--retries < 0) {
			err_mcu("%s read status failed!!!!, data = 0x%04x\n", __func__, rcvdata);
			break;
		}
		i++;
	} while (rcvdata != 0x00);

	info_mcu("%s X\n", __func__);

	return 0;
}

int ois_mcu_check_hall_cal(struct v4l2_subdev *subdev, u16 *hall_cal_data)
{
	int ret = 0;
	u8 val = 0;
	int retries = 600;
	u8 rxbuf[32] = {0, };
	u8 txbuf[32] = {0, };
	u16 af_best_pos = 0;
	u16 temp = 0;
	int pre_pcal[2] = {0, };
	int pre_ncal[2] = {0, };
	int cur_pcal[2] = {0, };
	int cur_ncal[2] = {0, };
	struct ois_mcu_dev *mcu = NULL;
	struct is_core *core = NULL;

	WARN_ON(!subdev);

	info_mcu("%s E\n", __func__);

	mcu = (struct ois_mcu_dev *)v4l2_get_subdevdata(subdev);
	if(!mcu) {
		err_mcu("%s, mcu is NULL", __func__);
		ret = -EINVAL;
		return ret;
	}

	core = (struct is_core *)dev_get_drvdata(is_dev);
	if (!core) {
		err("core is null");
		ret = -EINVAL;
		return ret;
	}

	do {
		val = is_mcu_get_reg(mcu->regs[OM_REG_CORE], R_OIS_CMD_STATUS);
		usleep_range(500, 510);
		if (--retries < 0) {
			err_mcu("%s Read status failed!!!!, data = 0x%04x\n", __func__, val);
			break;
		}
	} while (val != 0x01);

	/* Read stored calibration mark */
	ois_mcu_bypass_read_mode1(mcu, 0xE9, 0xE4, rxbuf, 0x01);
	info_mcu("read reg(0xE4) = 0x%02x", rxbuf[0]);

	if (rxbuf[0] != 0x01) {
		info_mcu("calibration data empty(0x%02x).", rxbuf[0]);
		return ret;
	}
	
	/* Read stored AF best position*/
	ois_mcu_bypass_read_mode1(mcu, 0xE9, 0xE5, rxbuf, 0x01);
	af_best_pos = (u16)rxbuf[0] << 4;
	info_mcu("read reg(0xE5) = 0x%04x", af_best_pos);

	
	/* Read stored PCAL and NCAL of X axis */
	ois_mcu_bypass_read_mode1(mcu, 0xE9, 0x04, rxbuf, 0x04);
	temp = ((u16)rxbuf[0] << 8) & 0x8000;
	temp |= ((u16)rxbuf[0] << 1) & 0x00FE;
	temp |= ((u16)rxbuf[1] >> 7) & 0x0001;
	pre_pcal[0] = (int)temp;
	info_mcu("read reg(0x04) = 0x%04x", pre_pcal[0]);

	temp = 0x0;
	temp = ((u16)rxbuf[2] << 8) & 0x8000;
	temp |= ((u16)rxbuf[2] << 1) & 0x00FE;
	temp |= ((u16)rxbuf[3] >> 7) & 0x0001;
	pre_ncal[0] = (int)temp;
	info_mcu("read reg(0x06) = 0x%04x", pre_ncal[0]);

	
	/* Read stored PCAL and NCAL for Y axis */
	memset(rxbuf, 0x0, sizeof(rxbuf));
	ois_mcu_bypass_read_mode1(mcu, 0x69, 0x04, rxbuf, 0x04);
	temp = ((u16)rxbuf[0] << 8) & 0x8000;
	temp |= ((u16)rxbuf[0] << 1) & 0x00FE;
	temp |= ((u16)rxbuf[1] >> 7) & 0x0001;
	pre_pcal[1] = (int)temp;
	info_mcu("read reg(0x04) = 0x%04x", pre_pcal[1]);

	temp = 0x0;
	temp = ((u16)rxbuf[2] << 8) & 0x8000;
	temp |= ((u16)rxbuf[2] << 1) & 0x00FE;
	temp |= ((u16)rxbuf[3] >> 7) & 0x0001;
	pre_ncal[1] = (int)temp;
	info_mcu("read reg(0x06) = 0x%04x", pre_ncal[1]);

	/* Move AF to best position which read from EEPROM */
	is_af_move_lens_pos(core, SENSOR_POSITION_REAR2, af_best_pos);
	msleep(50);
	
	/* Change setting  Mode for Hall cal */
	txbuf[0] = 0x3B;
	ois_mcu_bypass_write_mode1(mcu, 0xE8, 0xAE, txbuf, 0x01);
	ois_mcu_bypass_write_mode1(mcu, 0x68, 0xAE, txbuf, 0x01);
	info_mcu("write reg(0xAE) = 0x%02x", txbuf[0]);
	
	/* Start hall calibration for X axis */
	txbuf[0] = 0x01;
	ois_mcu_bypass_write_mode1(mcu, 0xE8, 0x02, txbuf, 0x01);
	msleep(150);
	
	/* Start hall calibration for Y axis */
	ois_mcu_bypass_write_mode1(mcu, 0x68, 0x02, txbuf, 0x01);
	msleep(150);

	/*Clear setting  Mode */
	txbuf[0] = 0x00;
	ois_mcu_bypass_write_mode1(mcu, 0xE8, 0xAE, txbuf, 0x01);
	ois_mcu_bypass_write_mode1(mcu, 0x68, 0xAE, txbuf, 0x01);
	info_mcu("write reg(0xAE) = 0x%02x", txbuf[0]);
	
	/*Read new PCAL and NCAL for X axis*/
	memset(rxbuf, 0x0, sizeof(rxbuf));
	ois_mcu_bypass_read_mode1(mcu, 0xE9, 0x04, rxbuf, 0x04);
	temp = ((u16)rxbuf[0] << 8) & 0x8000;
	temp |= ((u16)rxbuf[0] << 1) & 0x00FE;
	temp |= ((u16)rxbuf[1] >> 7) & 0x0001;
	cur_pcal[0] = (int)temp;
	info_mcu("read reg(0x04) = 0x%04x", cur_pcal[0]);

	temp = 0x0;
	temp = ((u16)rxbuf[2] << 8) & 0x8000;
	temp |= ((u16)rxbuf[2] << 1) & 0x00FE;
	temp |= ((u16)rxbuf[3] >> 7) & 0x0001;
	cur_ncal[0] = (int)temp;
	info_mcu("read reg(0x06) = 0x%04x", cur_ncal[0]);
	
	/*Read new PCAL and NCAL for Y axis*/
	memset(rxbuf, 0x0, sizeof(rxbuf));
	ois_mcu_bypass_read_mode1(mcu, 0x69, 0x04, rxbuf, 0x04);
	temp = ((u16)rxbuf[0] << 8) & 0x8000;
	temp |= ((u16)rxbuf[0] << 1) & 0x00FE;
	temp |= ((u16)rxbuf[1] >> 7) & 0x0001;
	cur_pcal[1] = (int)temp;
	info_mcu("read reg(0x04) = 0x%04x", cur_pcal[1]);

	temp = 0x0;
	temp = ((u16)rxbuf[2] << 8) & 0x8000;
	temp |= ((u16)rxbuf[2] << 1) & 0x00FE;
	temp |= ((u16)rxbuf[3] >> 7) & 0x0001;
	cur_ncal[1] = (int)temp;
	info_mcu("read reg(0x06) = 0x%04x", cur_ncal[1]);

	hall_cal_data[0] = pre_pcal[0];
	hall_cal_data[1] = pre_ncal[0];
	hall_cal_data[2] = pre_pcal[1];
	hall_cal_data[3] = pre_ncal[1];
	hall_cal_data[4] = cur_pcal[0];
	hall_cal_data[5] = cur_ncal[0];
	hall_cal_data[6] = cur_pcal[1];
	hall_cal_data[7] = cur_ncal[1];

	info_mcu("%s  X\n", __func__);

	return ret;
}

int ois_mcu_read_ext_clock(struct v4l2_subdev *subdev, u32 *clock)
{
	int ret = 0;
	u8 val = 0;
	int retries = 600;
	u8 addr_size = 0x02;
	u8 data[4] = {0, };

	struct ois_mcu_dev *mcu = NULL;

	WARN_ON(!subdev);

	info_mcu("%s E\n", __func__);

	mcu = (struct ois_mcu_dev *)v4l2_get_subdevdata(subdev);
	if(!mcu) {
		err_mcu("%s, mcu is NULL", __func__);
		ret = -EINVAL;
		return ret;
	}

	do {
		val = is_mcu_get_reg(mcu->regs[OM_REG_CORE], R_OIS_CMD_STATUS);
		usleep_range(500, 510);
		if (--retries < 0) {
			err_mcu("%s Read status failed!!!!, data = 0x%04x\n", __func__, val);
			break;
		}
	} while (val != 0x01);

	ois_mcu_bypass_read(mcu, MCU_BYPASS_MODE_READ_ID, 0x03F0, addr_size, data, 0x02);
	ois_mcu_bypass_read(mcu, MCU_BYPASS_MODE_READ_ID, 0x03F2, addr_size, &data[2], 0x02);
	*clock = (data[3] << 24) | (data[2] << 16) | (data[1] << 8) | data[0];

	info_mcu("%s  X\n", __func__);

	return ret;
}

void ois_mcu_gyro_sleep(struct is_core *core)
{
	u8 val = 0;
	int retries = 20;
	struct ois_mcu_dev *mcu = NULL;

	mcu = core->mcu;

	is_mcu_set_reg(mcu->regs[OM_REG_CORE], R_OIS_CMD_START, 0x00);

	do {
		val = (u8)is_mcu_get_reg(mcu->regs[OM_REG_CORE], R_OIS_CMD_STATUS);

		if (val == 0x01 || val == 0x13)
			break;

		usleep_range(1000, 1100);
	} while (--retries > 0);

	if (retries <= 0) {
		err("Read register failed!!!!, data = 0x%04x\n", val);
	}

	is_mcu_set_reg(mcu->regs[OM_REG_CORE], R_OIS_CMD_GYRO_SLEEP, 0x03);
	usleep_range(1000, 1100);

	return;
}

void ois_mcu_exif_data(struct is_core *core)
{
	u8 error_reg[2], status_reg;
	u16 error_sum;
	struct ois_mcu_dev *mcu = NULL;
	 struct is_ois_exif *ois_exif_data = NULL;

	mcu = core->mcu;

	is_ois_get_exif_data(&ois_exif_data);

	error_reg[0] = (u8)is_mcu_get_reg(mcu->regs[OM_REG_CORE], R_OIS_CMD_GYRO_RESULT);
	error_reg[1] = (u8)is_mcu_get_reg(mcu->regs[OM_REG_CORE], R_OIS_CMD_CHECKSUM);

	error_sum = (error_reg[1] << 8) | error_reg[0];

	status_reg = (u8)is_mcu_get_reg(mcu->regs[OM_REG_CORE], R_OIS_CMD_STATUS);

	ois_exif_data->error_data = error_sum;
	ois_exif_data->status_data = status_reg;

	return;
}

u8 ois_mcu_read_status(struct is_core *core)
{
	u8 status = 0;
	struct ois_mcu_dev *mcu = NULL;

	mcu = core->mcu;

	status = (u8)is_mcu_get_reg(mcu->regs[OM_REG_CORE], R_OIS_CMD_READ_STATUS);

	return status;
}

u8 ois_mcu_read_cal_checksum(struct is_core *core)
{
	u8 status = 0;
	struct ois_mcu_dev *mcu = NULL;

	mcu = core->mcu;

	status = (u8)is_mcu_get_reg(mcu->regs[OM_REG_CORE], R_OIS_CMD_CHECKSUM);

	return status;
}

int ois_mcu_set_coef(struct v4l2_subdev *subdev, u8 coef)
{
	int ret = 0;
	struct is_ois *ois = NULL;
	struct ois_mcu_dev *mcu = NULL;
	struct is_mcu *is_mcu = NULL;

	WARN_ON(!subdev);

	if (!ois_wide_init && !ois_tele_init && !ois_tele2_init)
		return 0;

	mcu = (struct ois_mcu_dev *)v4l2_get_subdevdata(subdev);
	if(!mcu) {
		err("%s, mcu is NULL", __func__);
		ret = -EINVAL;
		return ret;
	}

	is_mcu = (struct is_mcu *)v4l2_get_subdev_hostdata(subdev);
	if(!is_mcu) {
		err("%s, is_mcu is NULL", __func__);
		ret = -EINVAL;
		return ret;
	}

	ois = is_mcu->ois;

	if (ois->pre_coef == coef)
		return ret;

	dbg_ois("%s %d\n", __func__, coef);

	is_mcu_set_reg(mcu->regs[OM_REG_CORE], R_OIS_CMD_SET_COEF, coef);

	ois->pre_coef = coef;

	return ret;
}

int ois_mcu_shift(struct v4l2_subdev *subdev)
{
	struct is_ois *ois = NULL;
	struct ois_mcu_dev *mcu = NULL;
	struct is_mcu *is_mcu = NULL;
	u8 data[2];
	int ret = 0;

	mcu = (struct ois_mcu_dev *)v4l2_get_subdevdata(subdev);
	if(!mcu) {
		err("%s, mcu is NULL", __func__);
		ret = -EINVAL;
		return ret;
	}

	is_mcu = (struct is_mcu *)v4l2_get_subdev_hostdata(subdev);
	if(!is_mcu) {
		err("%s, is_mcu is NULL", __func__);
		ret = -EINVAL;
		return ret;
	}

	ois = is_mcu->ois;

	data[0] = (ois_center_x & 0xFF);
	data[1] = (ois_center_x & 0xFF00) >> 8;
	is_mcu_set_reg(mcu->regs[OM_REG_CORE], R_OIS_CMD_CENTER_X1, data[0]);
	is_mcu_set_reg(mcu->regs[OM_REG_CORE], R_OIS_CMD_CENTER_X2, data[1]);

	data[0] = (ois_center_y & 0xFF);
	data[1] = (ois_center_y & 0xFF00) >> 8;
	is_mcu_set_reg(mcu->regs[OM_REG_CORE], R_OIS_CMD_CENTER_Y1, data[0]);
	is_mcu_set_reg(mcu->regs[OM_REG_CORE], R_OIS_CMD_CENTER_Y2, data[1]);

	return ret;
}

int ois_mcu_set_centering(struct v4l2_subdev *subdev)
{
	int ret = 0;
	struct is_ois *ois = NULL;
	struct ois_mcu_dev *mcu = NULL;
	struct is_mcu *is_mcu = NULL;

	WARN_ON(!subdev);

	mcu = (struct ois_mcu_dev *)v4l2_get_subdevdata(subdev);
	if(!mcu) {
		err("%s, mcu is NULL", __func__);
		ret = -EINVAL;
		return ret;
	}

	is_mcu = (struct is_mcu *)v4l2_get_subdev_hostdata(subdev);
	if(!is_mcu) {
		err("%s, is_mcu is NULL", __func__);
		ret = -EINVAL;
		return ret;
	}

	ois = is_mcu->ois;

	is_mcu_set_reg(mcu->regs[OM_REG_CORE], R_OIS_CMD_MODE, 0x05);

	ois->pre_ois_mode = OPTICAL_STABILIZATION_MODE_CENTERING;

	return ret;
}

u8 ois_mcu_read_mode(struct v4l2_subdev *subdev)
{
	int ret = 0;
	u8 mode = OPTICAL_STABILIZATION_MODE_OFF;
	struct ois_mcu_dev *mcu = NULL;
	struct is_mcu *is_mcu = NULL;

	mcu = (struct ois_mcu_dev *)v4l2_get_subdevdata(subdev);
	if(!mcu) {
		err("%s, mcu is NULL", __func__);
		ret = -EINVAL;
		return ret;
	}

	is_mcu = (struct is_mcu *)v4l2_get_subdev_hostdata(subdev);
	if(!is_mcu) {
		err("%s, is_mcu is NULL", __func__);
		ret = -EINVAL;
		return ret;
	}

	mode = (u8)is_mcu_get_reg(mcu->regs[OM_REG_CORE], R_OIS_CMD_MODE);

	switch(mode) {
		case 0x00:
			mode = OPTICAL_STABILIZATION_MODE_STILL;
			break;
		case 0x01:
			mode = OPTICAL_STABILIZATION_MODE_VIDEO;
			break;
		case 0x05:
			mode = OPTICAL_STABILIZATION_MODE_CENTERING;
			break;
		case 0x13:
			mode = OPTICAL_STABILIZATION_MODE_STILL_ZOOM;
			break;
		case 0x14:
			mode = OPTICAL_STABILIZATION_MODE_VDIS;
			break;
		default:
			dbg_ois("%s: ois_mode value(%d)\n", __func__, mode);
			break;
	}

	return mode;
}

bool ois_mcu_gyro_cal(struct is_core *core, long *x_value, long *y_value)
{
	u8 val = 0, x = 0, y = 0;
	int retries = 30;
	int scale_factor = OIS_GYRO_SCALE_FACTOR_LSM6DSO;
	int x_sum = 0, y_sum = 0;
	bool result = false;
	struct ois_mcu_dev *mcu = NULL;

	mcu = core->mcu;

	info_mcu("%s : E\n", __func__);

	/* check ois status */
	do {
		val = (u8)is_mcu_get_reg(mcu->regs[OM_REG_CORE], R_OIS_CMD_STATUS);
		 msleep(20);
		if (--retries < 0) {
			err_mcu("%s Read status failed!!!!, data = 0x%04x\n", __func__, val);
			break;
		}
	} while (val != 0x01);

	retries = 30;

	is_mcu_set_reg(mcu->regs[OM_REG_CORE], R_OIS_CMD_GYRO_CAL, 0x01);

	do {
		val = (u8)is_mcu_get_reg(mcu->regs[OM_REG_CORE], R_OIS_CMD_GYRO_CAL);
		msleep(15);
		if (--retries < 0) {
			err("Read register failed!!!!, data = 0x%04x\n", val);
			break;
		}
	} while (val);

	/* Gyro result check */
	val = (u8)is_mcu_get_reg(mcu->regs[OM_REG_CORE], R_OIS_CMD_GYRO_RESULT);

	if ((val & 0x23) == 0x0) {
		info_mcu("[%s] Written cal is OK. val = 0x%02x.", __func__, val);
		result = true;
	} else {
		info_mcu("[%s] Written cal is NG. val = 0x%02x.", __func__, val);
		result = false;
	}

	val = (u8)is_mcu_get_reg(mcu->regs[OM_REG_CORE], R_OIS_CMD_RAW_DEBUG_X1);
	x = val;
	val = (u8)is_mcu_get_reg(mcu->regs[OM_REG_CORE], R_OIS_CMD_RAW_DEBUG_X2);
	x_sum = (val << 8) | x;
	if (x_sum > 0x7FFF) {
		x_sum = -((x_sum ^ 0xFFFF) + 1);
	}

	val = (u8)is_mcu_get_reg(mcu->regs[OM_REG_CORE], R_OIS_CMD_RAW_DEBUG_Y1);
	y = val;
	val = (u8)is_mcu_get_reg(mcu->regs[OM_REG_CORE], R_OIS_CMD_RAW_DEBUG_Y2);
	y_sum = (val << 8) | y;
	if (y_sum > 0x7FFF) {
		y_sum = -((y_sum ^ 0xFFFF) + 1);
	}

	*x_value = x_sum * 1000 / scale_factor;
	*y_value = y_sum * 1000 / scale_factor;

	info_mcu("%s X (x = %ld/y = %ld) : result = %d\n", __func__, *x_value, *y_value, result);

	return result;
}
bool ois_mcu_read_gyro_noise(struct is_core *core, long *x_value, long *y_value)
{
	u8 val = 0, x = 0, y = 0;
	int retries = 30;
	int scale_factor = OIS_GYRO_SCALE_FACTOR_LSM6DSO;
	int x_sum = 0, y_sum = 0;
	bool result = true;
	struct ois_mcu_dev *mcu = NULL;

	mcu = core->mcu;

	info_mcu("%s : E\n", __func__);

	msleep(500);

	is_mcu_set_reg(mcu->regs[OM_REG_CORE], R_OIS_CMD_START, 0x00);
	usleep_range(1000, 1100);

	/* check ois status */
	do {
		val = (u8)is_mcu_get_reg(mcu->regs[OM_REG_CORE], R_OIS_CMD_STATUS);
		msleep(20);
		if (--retries < 0) {
			err_mcu("%s Read status failed!!!!, data = 0x%04x\n", __func__, val);
			result = false;
			break;
		}
	} while (val != 0x01);

	is_mcu_set_reg(mcu->regs[OM_REG_CORE], R_OIS_CMD_SET_GYRO_NOISE, 0x01);

	msleep(1000);

	val = (u8)is_mcu_get_reg(mcu->regs[OM_REG_CORE], R_OIS_CMD_READ_GYRO_NOISE_X1);
	x = val;
	val = (u8)is_mcu_get_reg(mcu->regs[OM_REG_CORE], R_OIS_CMD_READ_GYRO_NOISE_X2);
	x_sum = (val << 8) | x;
	if (x_sum > 0x7FFF) {
		x_sum = -((x_sum ^ 0xFFFF) + 1);
	}

	val = (u8)is_mcu_get_reg(mcu->regs[OM_REG_CORE], R_OIS_CMD_READ_GYRO_NOISE_Y1);
	y = val;
	val = (u8)is_mcu_get_reg(mcu->regs[OM_REG_CORE], R_OIS_CMD_READ_GYRO_NOISE_Y2);
	y_sum = (val << 8) | y;
	if (y_sum > 0x7FFF) {
		y_sum = -((y_sum ^ 0xFFFF) + 1);
	}

	*x_value = x_sum * 1000 / scale_factor;
	*y_value = y_sum * 1000 / scale_factor;

	info_mcu("%s X (x = %ld/y = %ld) : result = %d\n", __func__, *x_value, *y_value, result);

	return result;
}

long ois_mcu_open_fw(struct is_core *core)
{
	int ret = 0;
	struct is_binary mcu_bin;
	struct is_mcu *is_mcu = NULL;
	struct is_device_sensor *device = NULL;
	struct ois_mcu_dev *mcu = NULL;
	struct is_ois_info *ois_minfo = NULL;
	struct is_ois_info *ois_pinfo = NULL;

	info_mcu("%s started", __func__);

	mcu = core->mcu;

	device = &core->sensor[0];
	is_mcu = device->mcu;

	is_ois_get_module_version(&ois_minfo);
	is_ois_get_phone_version(&ois_pinfo);

	setup_binary_loader(&mcu_bin, 3, -EAGAIN, NULL, NULL);
	ret = request_binary(&mcu_bin, IS_MCU_PATH, IS_MCU_FW_NAME, mcu->dev);
	if (ret) {
		err_mcu("request_firmware was failed(%ld)\n", ret);
		ret = 0;
		goto request_err;
	}

	memcpy(&is_mcu->vdrinfo_bin[0], mcu_bin.data + MCU_HW_VERSION_OFFSET, sizeof(is_mcu->vdrinfo_bin));
	is_mcu->hw_bin[0] = *((u8 *)mcu_bin.data + MCU_BIN_VERSION_OFFSET + 3);
	is_mcu->hw_bin[1] = *((u8 *)mcu_bin.data + MCU_BIN_VERSION_OFFSET + 2);
	is_mcu->hw_bin[2] = *((u8 *)mcu_bin.data + MCU_BIN_VERSION_OFFSET + 1);
	is_mcu->hw_bin[3] = *((u8 *)mcu_bin.data + MCU_BIN_VERSION_OFFSET);
	memcpy(ois_pinfo->header_ver, is_mcu->hw_bin, 4);
	memcpy(&ois_pinfo->header_ver[4], mcu_bin.data + MCU_HW_VERSION_OFFSET, 4);
	memcpy(ois_minfo->header_ver, ois_pinfo->header_ver, sizeof(ois_pinfo->header_ver));

	info_mcu("Request FW was done (%s%s, %ld)\n",
		IS_MCU_PATH, IS_MCU_FW_NAME, mcu_bin.size);

	ret = mcu_bin.size;

request_err:
	release_binary(&mcu_bin);

	info_mcu("%s %d end", __func__, __LINE__);

	return ret;
}

#ifdef USE_OIS_HALL_DATA_FOR_VDIS
int ois_mcu_get_hall_data(struct v4l2_subdev *subdev, struct is_ois_hall_data *halldata)
{
	int ret = 0;
	struct ois_mcu_dev *mcu = NULL;
	u8 val1 = 0, val2 = 0, val3 = 0, val4 = 0;
	u32 counter = 0;
	int val_sum = 0;

	WARN_ON(!subdev);

	mcu = (struct ois_mcu_dev *)v4l2_get_subdevdata(subdev);
	if(!mcu) {
		err("%s, mcu is NULL", __func__);
		ret = -EINVAL;
		return ret;
	}

	val1 = is_mcu_get_reg(mcu->regs[OM_REG_CORE], R_OIS_CMD_VDIS_TIME_STAMP_1);
	val2 = is_mcu_get_reg(mcu->regs[OM_REG_CORE], R_OIS_CMD_VDIS_TIME_STAMP_2);
	val3 = is_mcu_get_reg(mcu->regs[OM_REG_CORE], R_OIS_CMD_VDIS_TIME_STAMP_3);
	val4 = is_mcu_get_reg(mcu->regs[OM_REG_CORE], R_OIS_CMD_VDIS_TIME_STAMP_4);
	counter =  (val4 << 24) | (val3 << 16) | (val2 << 8) | val1;
	halldata->counter = counter;

	val1 = is_mcu_get_reg(mcu->regs[OM_REG_CORE], R_OIS_CMD_VDIS_X1_ANGVEL_1);
	val2 = is_mcu_get_reg(mcu->regs[OM_REG_CORE], R_OIS_CMD_VDIS_X1_ANGVEL_2);
	val_sum = (val2 << 8) | val1;
	halldata->X_AngVel[0] = val_sum;

	val1 = is_mcu_get_reg(mcu->regs[OM_REG_CORE], R_OIS_CMD_VDIS_Y1_ANGVEL_1);
	val2 = is_mcu_get_reg(mcu->regs[OM_REG_CORE], R_OIS_CMD_VDIS_Y1_ANGVEL_2);
	val_sum = (val2 << 8) | val1;
	halldata->Y_AngVel[0] = val_sum;

	val1 = is_mcu_get_reg(mcu->regs[OM_REG_CORE], R_OIS_CMD_VDIS_Z1_ANGVEL_1);
	val2 = is_mcu_get_reg(mcu->regs[OM_REG_CORE], R_OIS_CMD_VDIS_Z1_ANGVEL_2);
	val_sum = (val2 << 8) | val1;
	halldata->Z_AngVel[0] = val_sum;

	val1 = is_mcu_get_reg(mcu->regs[OM_REG_CORE], R_OIS_CMD_VDIS_X2_ANGVEL_1);
	val2 = is_mcu_get_reg(mcu->regs[OM_REG_CORE], R_OIS_CMD_VDIS_X2_ANGVEL_2);
	val_sum = (val2 << 8) | val1;
	halldata->X_AngVel[1] = val_sum;

	val1 = is_mcu_get_reg(mcu->regs[OM_REG_CORE], R_OIS_CMD_VDIS_Y2_ANGVEL_1);
	val2 = is_mcu_get_reg(mcu->regs[OM_REG_CORE], R_OIS_CMD_VDIS_Y2_ANGVEL_2);
	val_sum = (val2 << 8) | val1;
	halldata->Y_AngVel[1] = val_sum;

	val1 = is_mcu_get_reg(mcu->regs[OM_REG_CORE], R_OIS_CMD_VDIS_Z2_ANGVEL_1);
	val2 = is_mcu_get_reg(mcu->regs[OM_REG_CORE], R_OIS_CMD_VDIS_Z2_ANGVEL_2);
	val_sum = (val2 << 8) | val1;
	halldata->Z_AngVel[1] = val_sum;

	val1 = is_mcu_get_reg(mcu->regs[OM_REG_CORE], R_OIS_CMD_VDIS_X3_ANGVEL_1);
	val2 = is_mcu_get_reg(mcu->regs[OM_REG_CORE], R_OIS_CMD_VDIS_X3_ANGVEL_2);
	val_sum = (val2 << 8) | val1;
	halldata->X_AngVel[2] = val_sum;

	val1 = is_mcu_get_reg(mcu->regs[OM_REG_CORE], R_OIS_CMD_VDIS_Y3_ANGVEL_1);
	val2 = is_mcu_get_reg(mcu->regs[OM_REG_CORE], R_OIS_CMD_VDIS_Y3_ANGVEL_2);
	val_sum = (val2 << 8) | val1;
	halldata->Y_AngVel[2] = val_sum;

	val1 = is_mcu_get_reg(mcu->regs[OM_REG_CORE], R_OIS_CMD_VDIS_Z3_ANGVEL_1);
	val2 = is_mcu_get_reg(mcu->regs[OM_REG_CORE], R_OIS_CMD_VDIS_Z3_ANGVEL_2);
	val_sum = (val2 << 8) | val1;
	halldata->Z_AngVel[2] = val_sum;

	val1 = is_mcu_get_reg(mcu->regs[OM_REG_CORE], R_OIS_CMD_VDIS_X4_ANGVEL_1);
	val2 = is_mcu_get_reg(mcu->regs[OM_REG_CORE], R_OIS_CMD_VDIS_X4_ANGVEL_2);
	val_sum = (val2 << 8) | val1;
	halldata->X_AngVel[3] = val_sum;

	val1 = is_mcu_get_reg(mcu->regs[OM_REG_CORE], R_OIS_CMD_VDIS_Y4_ANGVEL_1);
	val2 = is_mcu_get_reg(mcu->regs[OM_REG_CORE], R_OIS_CMD_VDIS_Y4_ANGVEL_2);
	val_sum = (val2 << 8) | val1;
	halldata->Y_AngVel[3] = val_sum;

	val1 = is_mcu_get_reg(mcu->regs[OM_REG_CORE], R_OIS_CMD_VDIS_Z4_ANGVEL_1);
	val2 = is_mcu_get_reg(mcu->regs[OM_REG_CORE], R_OIS_CMD_VDIS_Z4_ANGVEL_2);
	val_sum = (val2 << 8) | val1;
	halldata->Z_AngVel[3] = val_sum;

	return ret;
}
#endif

bool ois_mcu_check_fw(struct is_core *core)
{
	long ret = 0;
	struct is_vender_specific *specific;

	info_mcu("%s", __func__);

	ret = ois_mcu_open_fw(core);
	if (ret == 0) {
		err("mcu fw open failed");
		return false;
	}

	specific = core->vender.private_data;
	specific->ois_ver_read = true;

	return true;
}

void ois_mcu_check_valid(struct v4l2_subdev *subdev, u8 *value)
{
	struct ois_mcu_dev *mcu = NULL;
	u8 error_reg[2] = {0, };

	mcu = (struct ois_mcu_dev *)v4l2_get_subdevdata(subdev);
	if(!mcu) {
		err("%s, mcu is NULL", __func__);
		return;
	}

	ois_mcu_init_factory(subdev);

	error_reg[0] = (u8)is_mcu_get_reg(mcu->regs[OM_REG_CORE], R_OIS_CMD_GYRO_RESULT);
	error_reg[1] = (u8)is_mcu_get_reg(mcu->regs[OM_REG_CORE], R_OIS_CMD_CHECKSUM);
	info_mcu("%s error reg value = 0x%02x/0x%02x", __func__, error_reg[0], error_reg[1]);

	*value = error_reg[1];

	return;
}


#ifdef CONFIG_USE_OIS_TAMODE_CONTROL
void ois_mcu_set_tamode(void *ois_core, bool onoff)
{
	struct is_device_sensor *device = NULL;
	struct is_core *core = (struct is_core *)ois_core;
	struct is_mcu *is_mcu = NULL;
	struct ois_mcu_dev *mcu = NULL;
	bool camera_running_rear = false;
	bool camera_running_rear2 = false;
	bool camera_running_rear4 = false;

	device = &core->sensor[0];
	is_mcu = device->mcu;
	mcu = core->mcu;

	camera_running_rear = is_vendor_check_camera_running(SENSOR_POSITION_REAR);
	camera_running_rear2 = is_vendor_check_camera_running(SENSOR_POSITION_REAR2);
	camera_running_rear4 = is_vendor_check_camera_running(SENSOR_POSITION_REAR4);

	if (onoff) {
		ois_tamode_onoff = true;
		if ((camera_running_rear || camera_running_rear2 || camera_running_rear4) && !ois_tamode_status) {
			is_mcu_set_reg(mcu->regs[OM_REG_CORE], R_OIS_CMD_TAMODE, 0x01);
			ois_tamode_status = true;
			info_mcu("ois ta mode on.");
		}
	} else {
		if (ois_tamode_onoff) {
			if ((camera_running_rear || camera_running_rear2 || camera_running_rear4) && ois_tamode_status) {
				is_mcu_set_reg(mcu->regs[OM_REG_CORE], R_OIS_CMD_TAMODE, 0x00);
				ois_tamode_status = false;
				info_mcu("ois ta mode off.");
			}
		}
		ois_tamode_onoff = false;
	}


}
#endif

bool ois_mcu_get_active(void)
{
	return ois_hw_check;
}
static const struct v4l2_subdev_core_ops core_ops = {
	.init = ois_mcu_af_init,
	.ioctl = ois_mcu_actuator_ioctl,
};

static const struct v4l2_subdev_ops subdev_ops = {
	.core = &core_ops,
};

static struct is_ois_ops ois_ops_mcu = {
	.ois_init = ois_mcu_init,
	.ois_init_fac = ois_mcu_init_factory,
	.ois_init_rear2 = ois_mcu_init_rear2,
	.ois_deinit = ois_mcu_deinit,
	.ois_set_mode = ois_mcu_set_mode,
	.ois_shift_compensation = ois_mcu_shift_compensation,
	.ois_self_test = ois_mcu_self_test,
	.ois_auto_test = ois_mcu_auto_test_all,
#ifdef CAMERA_2ND_OIS
	.ois_auto_test_rear2 = ois_mcu_auto_test_rear2,
	.ois_set_power_mode = ois_mcu_set_power_mode,
#endif
	.ois_check_fw = ois_mcu_check_fw,
	.ois_enable = ois_mcu_enable,
	.ois_disable = ois_mcu_disable,
	.ois_offset_test = ois_mcu_offset_test,
	.ois_get_offset_data = ois_mcu_get_offset_data,
	.ois_gyro_sleep = ois_mcu_gyro_sleep,
	.ois_exif_data = ois_mcu_exif_data,
	.ois_read_status = ois_mcu_read_status,
	.ois_read_cal_checksum = ois_mcu_read_cal_checksum,
	.ois_set_coef = ois_mcu_set_coef,
	//.ois_read_fw_ver = is_mcu_read_fw_ver, //TEMP_2020
	.ois_center_shift = ois_mcu_shift,
	.ois_set_center = ois_mcu_set_centering,
	.ois_read_mode = ois_mcu_read_mode,
	.ois_calibration_test = ois_mcu_gyro_cal,
	.ois_read_gyro_noise = ois_mcu_read_gyro_noise,
	.ois_set_af_active = ois_mcu_af_set_active,
	.ois_get_hall_pos = ois_mcu_get_hall_position,
	.ois_check_cross_talk = ois_mcu_check_cross_talk,
	.ois_check_hall_cal = ois_mcu_check_hall_cal,
	.ois_check_valid = ois_mcu_check_valid,
#ifdef USE_OIS_HALL_DATA_FOR_VDIS
	.ois_get_hall_data = ois_mcu_get_hall_data,
#endif
	.ois_get_active = ois_mcu_get_active,
	.ois_read_ext_clock = ois_mcu_read_ext_clock,
	.ois_parsing_raw_data = ois_mcu_parsing_raw_data,
};

#ifdef CONFIG_USE_OIS_TAMODE_CONTROL
struct ois_tamode_interface {
	void *core;
	void (*ois_func)(void *, bool);
	struct notifier_block nb;
	struct power_supply *psy_bat;
	struct power_supply *psy_ac;
};

static struct ois_tamode_interface set_ois_tamode;

extern int ois_tamode_register(struct ois_tamode_interface *ois);

static int ps_notifier_cb(struct notifier_block *nb, unsigned long event, void *data)
{
	struct ois_tamode_interface *ois =
		container_of(nb, struct ois_tamode_interface, nb);
	struct power_supply *psy = data;

	if (event != PSY_EVENT_PROP_CHANGED)
		return NOTIFY_OK;

	if (ois->psy_bat == NULL || ois->psy_ac == NULL)
		return NOTIFY_OK;

	info("%s\n", __func__);
	if (psy == ois->psy_bat) {
		union power_supply_propval status_val, ac_val;

		status_val.intval = ac_val.intval = 0;
		power_supply_get_property(ois->psy_bat, POWER_SUPPLY_PROP_STATUS, &status_val);
		power_supply_get_property(ois->psy_ac, POWER_SUPPLY_PROP_ONLINE, &ac_val);
		info("%s: status = %d, ac = %d\n", __func__, status_val.intval, ac_val.intval);
		ois->ois_func(ois->core,
			 (status_val.intval == POWER_SUPPLY_STATUS_FULL && ac_val.intval));
	}
	return NOTIFY_OK;
}
#endif

static int ois_mcu_probe(struct platform_device *pdev)
{
	struct is_core *core;
	struct ois_mcu_dev *mcu = NULL;
	struct resource *res;
	int ret = 0;
	struct device_node *dnode;
	struct is_mcu *is_mcu = NULL;
	struct is_device_sensor *device;
	struct v4l2_subdev *subdev_mcu = NULL;
	struct v4l2_subdev *subdev_ois = NULL;
	struct is_device_ois *ois_device = NULL;
	struct is_ois *ois = NULL;
	struct is_actuator *actuator = NULL;
	struct v4l2_subdev *subdev_actuator = NULL;
	const u32 *sensor_id_spec;
	const u32 *mcu_actuator_spec;
	u32 sensor_id_len;
	u32 sensor_id[IS_SENSOR_COUNT] = {0, };
	u32 mcu_actuator_list[IS_SENSOR_COUNT] = {0, };
	int i;
	u32 mcu_actuator_len;
	struct is_vender_specific *specific;
	bool support_photo_fastae = false;
	bool skip_video_fastae = false;
	bool off_during_uwonly_mode = false;

	core = (struct is_core *)dev_get_drvdata(is_dev);

	dnode = pdev->dev.of_node;

	sensor_id_spec = of_get_property(dnode, "id", &sensor_id_len);
	if (!sensor_id_spec) {
		err("sensor_id num read is fail(%d)", ret);
		goto p_err;
	}

	sensor_id_len /= (unsigned int)sizeof(*sensor_id_spec);

	ret = of_property_read_u32_array(dnode, "id", sensor_id, sensor_id_len);
	if (ret) {
		err("sensor_id read is fail(%d)", ret);
		goto p_err;
	}

	mcu_actuator_spec = of_get_property(dnode, "mcu_ctrl_actuator", &mcu_actuator_len);
	if (mcu_actuator_spec) {
		mcu_actuator_len /= (unsigned int)sizeof(*mcu_actuator_spec);
		ret = of_property_read_u32_array(dnode, "mcu_ctrl_actuator",
		        mcu_actuator_list, mcu_actuator_len);
		if (ret)
		        info_mcu("mcu_ctrl_actuator read is fail(%d)", ret);
	}

	mcu_support_oldhw = of_property_read_bool(dnode, "support_oldhw");
	if (!mcu_support_oldhw) {
		info_mcu("support_oldhw not use");
	}

	support_photo_fastae = of_property_read_bool(dnode, "mcu_support_photo_fastae");
	if (!support_photo_fastae) {
		info_mcu("support_photo_fastae not use");
	}

	skip_video_fastae = of_property_read_bool(dnode, "mcu_skip_video_fastae");
	if (!skip_video_fastae) {
		info_mcu("skip_video_fastae not use");
	}

	off_during_uwonly_mode = of_property_read_bool(dnode, "mcu_off_during_uwonly_mode");
	if (!off_during_uwonly_mode) {
		info_mcu("off_during_uwonly_mode not use");
	}

	mcu = devm_kzalloc(&pdev->dev, sizeof(struct ois_mcu_dev), GFP_KERNEL);
	if (!mcu)
		return -ENOMEM;

	is_mcu = devm_kzalloc(&pdev->dev, sizeof(struct is_mcu) * sensor_id_len, GFP_KERNEL);
	if (!mcu) {
		err("fimc_is_mcu is NULL");
		ret = -ENOMEM;
		goto p_err;
	}

	subdev_mcu = devm_kzalloc(&pdev->dev, sizeof(struct v4l2_subdev) * sensor_id_len, GFP_KERNEL);
	if (!subdev_mcu) {
		err("subdev_mcu is NULL");
		ret = -ENOMEM;
		goto p_err;
	}

	ois = devm_kzalloc(&pdev->dev, sizeof(struct is_ois) * sensor_id_len, GFP_KERNEL);
	if (!ois) {
		err("fimc_is_ois is NULL");
		ret = -ENOMEM;
		goto p_err;
	}

	subdev_ois = devm_kzalloc(&pdev->dev, sizeof(struct v4l2_subdev) * sensor_id_len, GFP_KERNEL);
	if (!subdev_ois) {
		err("subdev_ois is NULL");
		ret = -ENOMEM;
		goto p_err;
	}

	ois_device = devm_kzalloc(&pdev->dev, sizeof(struct is_device_ois), GFP_KERNEL);
	if (!ois_device) {
		err("fimc_is_device_ois is NULL");
		ret = -ENOMEM;
		goto p_err;
	}

	actuator = devm_kzalloc(&pdev->dev, sizeof(struct is_actuator), GFP_KERNEL);
	if (!actuator) {
		err("actuator is NULL");
		ret = -ENOMEM;
		goto p_err;
	}

	subdev_actuator = devm_kzalloc(&pdev->dev, sizeof(struct v4l2_subdev), GFP_KERNEL);
	if (!subdev_actuator) {
		err("subdev_actuator is NULL");
		ret = -ENOMEM;
		goto p_err;
	}

	mcu->dev = &pdev->dev;
	res = platform_get_resource(pdev, IORESOURCE_MEM, 0);
	if (!res) {
		dev_err(mcu->dev, "[@] can't get memory resource\n");
		return -ENODEV;
	}

	mcu->regs[OM_REG_CORE] = devm_ioremap_nocache(mcu->dev,
				res->start, resource_size(res));
	if (!mcu->regs[OM_REG_CORE]) {
		dev_err(&pdev->dev, "[@] ioremap failed\n");
		ret = -ENOMEM;
		goto err_ioremap;
	}
	mcu->regs_start[OM_REG_CORE] = res->start;
	mcu->regs_end[OM_REG_CORE] = res->end;

	res = platform_get_resource(pdev, IORESOURCE_MEM, 1);
	if (!res) {
		dev_err(mcu->dev, "[@] can't get memory resource\n");
		return -ENODEV;
	}

	mcu->regs[OM_REG_PERI1] = devm_ioremap_nocache(mcu->dev,
				res->start, resource_size(res));
	if (!mcu->regs[OM_REG_PERI1]) {
		dev_err(&pdev->dev, "[@] ioremap failed\n");
		ret = -ENOMEM;
		goto err_ioremap;
	}
	mcu->regs_start[OM_REG_PERI1] = res->start;
	mcu->regs_end[OM_REG_PERI1] = res->end;

	res = platform_get_resource(pdev, IORESOURCE_MEM, 2);
	if (!res) {
		dev_err(mcu->dev, "[@] can't get memory resource\n");
		return -ENODEV;
	}

	mcu->regs[OM_REG_PERI2] = devm_ioremap_nocache(mcu->dev,
				res->start, resource_size(res));
	if (!mcu->regs[OM_REG_PERI2]) {
		dev_err(mcu->dev, "[@] ioremap failed\n");
		ret = -ENOMEM;
		goto err_ioremap;
	}
	mcu->regs_start[OM_REG_PERI2] = res->start;
	mcu->regs_end[OM_REG_PERI2] = res->end;

	res = platform_get_resource(pdev, IORESOURCE_MEM, 3);
	if (!res) {
		dev_err(mcu->dev, "[@] can't get memory resource\n");
		return -ENODEV;
	}

	mcu->regs[OM_REG_PERI_SETTING] = devm_ioremap_nocache(mcu->dev,
				res->start, resource_size(res));
	if (!mcu->regs[OM_REG_PERI_SETTING]) {
		dev_err(&pdev->dev, "[@] ioremap failed\n");
		ret = -ENOMEM;
		goto err_ioremap;
	}
	mcu->regs_start[OM_REG_PERI_SETTING] = res->start;
	mcu->regs_end[OM_REG_PERI_SETTING] = res->end;

	mcu->irq = platform_get_irq(pdev, 0);
	if (mcu->irq < 0) {
		dev_err(mcu->dev, "[@] failed to get IRQ resource: %d\n",
							mcu->irq);
		ret = mcu->irq;
		goto err_get_irq;
	}
	ret = devm_request_irq(mcu->dev, mcu->irq, is_isr_ois_mcu,
			0,
			dev_name(mcu->dev), mcu);
	if (ret) {
		dev_err(mcu->dev, "[@] failed to request IRQ(%d): %d\n",
							mcu->irq, ret);
		goto err_req_irq;
	}

	platform_set_drvdata(pdev, mcu);
	core->mcu = mcu;
	atomic_set(&mcu->shared_rsc_count, 0);
	mutex_init(&mcu->power_mutex);

	specific = core->vender.private_data;
	specific->ois_ver_read = false;

	ois_device->ois_ops = &ois_ops_mcu;

	for (i = 0; i < sensor_id_len; i++) {
		probe_info("%s sensor_id %d\n", __func__, sensor_id[i]);

		probe_info("%s mcu_actuator_list %d\n", __func__, mcu_actuator_list[i]);

		device = &core->sensor[sensor_id[i]];

		is_mcu[i].name = MCU_NAME_INTERNAL;
		is_mcu[i].subdev = &subdev_mcu[i];
		is_mcu[i].device = sensor_id[i];
		is_mcu[i].private_data = core;
		is_mcu[i].support_photo_fastae = support_photo_fastae;
		is_mcu[i].skip_video_fastae = skip_video_fastae;
		is_mcu[i].off_during_uwonly_mode = off_during_uwonly_mode;

		ois[i].subdev = &subdev_ois[i];
		ois[i].device = sensor_id[i];
		ois[i].ois_mode = OPTICAL_STABILIZATION_MODE_OFF;
		ois[i].pre_ois_mode = OPTICAL_STABILIZATION_MODE_OFF;
		ois[i].ois_shift_available = false;
		ois[i].i2c_lock = NULL;
		ois[i].ois_ops = &ois_ops_mcu;

#if defined(USE_TELE_OIS_AF_COMMON_INTERFACE) || defined(USE_TELE2_OIS_AF_COMMON_INTERFACE)
		if (mcu_actuator_list[i] == 1) {
			actuator->id = ACTUATOR_NAME_AK737X;
			actuator->subdev = subdev_actuator;
			actuator->device = sensor_id[i];
			actuator->position = 0;
			actuator->need_softlanding = 0;
			actuator->max_position = MCU_ACT_POS_MAX_SIZE;
			actuator->pos_size_bit = MCU_ACT_POS_SIZE_BIT;
			actuator->pos_direction = MCU_ACT_POS_DIRECTION;

			is_mcu[i].subdev_actuator = subdev_actuator;
			is_mcu[i].actuator = actuator;

			device->subdev_actuator[sensor_id[i]] = subdev_actuator;
			device->actuator[sensor_id[i]] = actuator;

			v4l2_subdev_init(subdev_actuator, &subdev_ops);
			v4l2_set_subdevdata(subdev_actuator, actuator);
			v4l2_set_subdev_hostdata(subdev_actuator, device);
		}
#endif

		is_mcu[i].mcu_ctrl_actuator = mcu_actuator_list[i];
		is_mcu[i].subdev_ois = &subdev_ois[i];
		is_mcu[i].ois = &ois[i];
		is_mcu[i].ois_device = ois_device;

		device->subdev_mcu = &subdev_mcu[i];
		device->mcu = &is_mcu[i];

		v4l2_set_subdevdata(&subdev_mcu[i], mcu);
		v4l2_set_subdev_hostdata(&subdev_mcu[i], &is_mcu[i]);

		probe_info("%s done\n", __func__);
	}

#if defined(CONFIG_PM)
	pm_runtime_enable(mcu->dev);
	set_bit(OM_HW_SUSPENDED, &mcu->state);
#endif
	set_bit(OM_HW_NONE, &mcu->state);

#ifdef CONFIG_USE_OIS_TAMODE_CONTROL
	set_ois_tamode.core = core;
	set_ois_tamode.ois_func = &ois_mcu_set_tamode;
	set_ois_tamode.nb.notifier_call = ps_notifier_cb;
	ret = power_supply_reg_notifier(&set_ois_tamode.nb);
	if (ret)
		err("ois ps_reg_notifier failed: %d\n", ret);
	else {
		set_ois_tamode.psy_bat = power_supply_get_by_name("battery");
		set_ois_tamode.psy_ac = power_supply_get_by_name("ac");

		if (set_ois_tamode.psy_bat == NULL ||
			set_ois_tamode.psy_ac == NULL) {
			err("failed to get psy\n");
		} else {
			power_supply_put(set_ois_tamode.psy_bat);
			power_supply_put(set_ois_tamode.psy_ac);
		}
	}
	ois_tamode_status = false;
	ois_tamode_onoff = false;
#endif

	probe_info("[@] %s device probe success\n", dev_name(mcu->dev));

	return 0;

err_req_irq:
err_get_irq:
	devm_iounmap(mcu->dev, mcu->regs[OM_REG_CORE]);
	devm_iounmap(mcu->dev, mcu->regs[OM_REG_PERI1]);
	devm_iounmap(mcu->dev, mcu->regs[OM_REG_PERI2]);
	devm_iounmap(mcu->dev, mcu->regs[OM_REG_PERI_SETTING]);
err_ioremap:
	devm_release_mem_region(mcu->dev, res->start, resource_size(res));
p_err:
	if (mcu)
		kfree(mcu);

	if (is_mcu)
		kfree(is_mcu);

	if (subdev_mcu)
		kfree(subdev_mcu);

	if (ois)
		kfree(ois);

	if (subdev_ois)
		kfree(subdev_ois);

	if (ois_device)
		kfree(ois_device);

	if (actuator)
		kfree(actuator);

	if (subdev_actuator)
		kfree(subdev_actuator);

	return ret;
}

static const struct dev_pm_ops ois_mcu_pm_ops = {
	SET_SYSTEM_SLEEP_PM_OPS(ois_mcu_suspend, ois_mcu_resume)
	SET_RUNTIME_PM_OPS(ois_mcu_runtime_suspend, ois_mcu_runtime_resume,
			   NULL)
};

static const struct of_device_id sensor_ois_mcu_match[] = {
	{
		.compatible = "samsung,sensor-ois-mcu",
	},
	{},
};

struct platform_driver sensor_ois_mcu_platform_driver = {
	.probe = ois_mcu_probe,
	.driver = {
		.name   = "Sensor-OIS-MCU",
		.owner  = THIS_MODULE,
		.pm	= &ois_mcu_pm_ops,
		.of_match_table = sensor_ois_mcu_match,
	}
};

#ifndef MODULE
static int __init sensor_ois_mcu_init(void)
{
	int ret;

	ret = platform_driver_probe(&sensor_ois_mcu_platform_driver,
							ois_mcu_probe);
	if (ret)
		err("failed to probe %s driver: %d\n",
			sensor_ois_mcu_platform_driver.driver.name, ret);

	return ret;
}
late_initcall_sync(sensor_ois_mcu_init);
#endif

MODULE_DESCRIPTION("Exynos Pablo OIS-MCU driver");
MODULE_AUTHOR("Younghwan Joo <yhwan.joo@samsung.com>");
MODULE_LICENSE("GPL v2");
