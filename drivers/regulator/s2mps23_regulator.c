/*
 * s2mps23.c
 *
 * Copyright (c) 2020 Samsung Electronics Co., Ltd
 *              http://www.samsung.com
 *
 *  This program is free software; you can redistribute  it and/or modify it
 *  under  the terms of  the GNU General  Public License as published by the
 *  Free Software Foundation;  either version 2 of the  License, or (at your
 *  option) any later version.
 *
 */

#include <linux/bug.h>
#include <linux/delay.h>
#include <linux/err.h>
#include <linux/gpio.h>
#include <linux/of_gpio.h>
#include <../drivers/pinctrl/samsung/pinctrl-samsung.h>
#include <linux/slab.h>
#include <linux/module.h>
#include <linux/regmap.h>
#include <linux/platform_device.h>
#include <linux/regulator/driver.h>
#include <linux/regulator/machine.h>
#include <linux/regulator/of_regulator.h>
#include <linux/mfd/samsung/s2mps23.h>
#include <linux/mfd/samsung/s2mps23-regulator.h>
#include <linux/mfd/samsung/s2mps23-regulator-evt0.h>
#include <linux/reset/exynos-reset.h>
#include <linux/io.h>
#include <linux/mutex.h>
#include <linux/interrupt.h>
#include <soc/samsung/exynos-pmu-if.h>
#include <linux/regulator/pmic_class.h>

#if IS_ENABLED(CONFIG_SEC_PM)
#include <linux/sec_class.h>
#endif /* CONFIG_SEC_PM */
#if IS_ENABLED(CONFIG_SEC_PM_DEBUG)
#include <linux/cpufreq.h>
#include <linux/sec_pm_debug.h>
#endif /* CONFIG_SEC_PM_DEBUG */
#if IS_ENABLED(CONFIG_SEC_PM_BIGDATA_UEVENT)
#include <linux/jiffies.h>
#include <linux/workqueue.h>
#include <linux/sec_hqm_device.h>
#endif /* CONFIG_SEC_PM_BIGDATA_UEVENT */

#if IS_ENABLED(CONFIG_EXYNOS_ACPM)
#include <soc/samsung/acpm_mfd.h>
#define MASTER_CHANNEL	0
static struct device_node *acpm_mfd_node;
#endif

#if IS_ENABLED(CONFIG_SEC_DEBUG_EXTRA_INFO)
#include <linux/sec_debug.h>
#endif

#define I2C_BASE_COMMON	0x00
#define I2C_BASE_PM	0x01
#define I2C_BASE_RTC	0x02
#define I2C_BASE_ADC	0x0A
#define I2C_BASE_GPIO	0x0B
#define I2C_BASE_CLOSE	0x0F

static struct s2mps23_info *s2mps23_static_info;
static int s2mps23_buck_ocp_cnt[S2MPS23_BUCK_MAX]; /* BUCK 1~9 OCP count */
static int s2mps23_temp_cnt[S2MPS23_TEMP_MAX]; /* 0 : 120 degree , 1 : 140 degree */
static int s2mps23_buck_oi_cnt[S2MPS23_BUCK_MAX]; /* BUCK 1~9 OI count */
#if IS_ENABLED(CONFIG_SEC_PM_BIGDATA)
static int hqm_bocp_cnt[S2MPS23_BUCK_MAX];
#endif /* CONFIG_SEC_PM_BIGDATA */

struct s2mps23_info {
	struct regulator_dev *rdev[S2MPS23_REGULATOR_MAX];
	unsigned int opmode[S2MPS23_REGULATOR_MAX];
	int num_regulators;
	struct s2mps23_dev *iodev;
	struct mutex lock;
	struct i2c_client *i2c;
	int buck_ocp_irq[S2MPS23_BUCK_MAX];	/* BUCK OCP IRQ */
	int buck_oi_irq[S2MPS23_BUCK_MAX];	/* BUCK OI IRQ */
	int temp_irq[S2MPS23_TEMP_MAX];	/* 0 : 120 degree, 1 : 140 degree */
#if IS_ENABLED(CONFIG_DRV_SAMSUNG_PMIC)
	u8 base_addr;
	u8 read_addr;
	u8 read_val;
	struct device *dev;
#endif
#if IS_ENABLED(CONFIG_SEC_PM_BIGDATA_UEVENT)
	struct delayed_work hqm_pmtp_work;
	struct delayed_work hqm_bocp_work;
#endif /* CONFIG_SEC_PM_BIGDATA_UEVENT */
};

#if IS_ENABLED(CONFIG_SEC_PM_DEBUG)
static u8 pmic_onsrc;
static u8 pmic_offsrc[2];

int pmic_get_onsrc(u8 *onsrc, size_t size)
{
	if (size > 0) {
		onsrc[0] = pmic_onsrc;
		return 1;
	}

	return 0;
}
EXPORT_SYMBOL_GPL(pmic_get_onsrc);

int pmic_get_offsrc(u8 *offsrc, size_t size)
{
	if (size >= 2) {
		offsrc[0] = pmic_offsrc[0];
		offsrc[1] = pmic_offsrc[1];
		return 2;
	}

	return 0;
}
EXPORT_SYMBOL_GPL(pmic_get_offsrc);
#endif /* CONFIG_SEC_PM_DEBUG */

static unsigned int s2mps23_of_map_mode(unsigned int val) {
	switch (val) {
	case SEC_OPMODE_SUSPEND:	/* ON in Standby Mode */
		return 0x1;
	case SEC_OPMODE_MIF:		/* ON in PWREN_MIF mode */
		return 0x2;
	case SEC_OPMODE_ON:		/* ON in Normal Mode */
		return 0x3;
	default:
		return REGULATOR_MODE_INVALID;
	}
}

/* Some LDOs supports [LPM/Normal]ON mode during suspend state */
static int s2m_set_mode(struct regulator_dev *rdev,
				     unsigned int mode)
{
	struct s2mps23_info *s2mps23 = rdev_get_drvdata(rdev);
	unsigned int val;
	int id = rdev_get_id(rdev);

	val = mode << S2MPS23_ENABLE_SHIFT;

	s2mps23->opmode[id] = val;
	return 0;
}

static int s2m_enable(struct regulator_dev *rdev)
{
	struct s2mps23_info *s2mps23 = rdev_get_drvdata(rdev);

	return s2mps23_update_reg(s2mps23->i2c, rdev->desc->enable_reg,
				  s2mps23->opmode[rdev_get_id(rdev)],
				  rdev->desc->enable_mask);
}

static int s2m_disable_regmap(struct regulator_dev *rdev)
{
	struct s2mps23_info *s2mps23 = rdev_get_drvdata(rdev);
	unsigned int val;

	if (rdev->desc->enable_is_inverted)
		val = rdev->desc->enable_mask;
	else
		val = 0;

	return s2mps23_update_reg(s2mps23->i2c, rdev->desc->enable_reg,
				  val, rdev->desc->enable_mask);
}

static int s2m_is_enabled_regmap(struct regulator_dev *rdev)
{
	struct s2mps23_info *s2mps23 = rdev_get_drvdata(rdev);
	int ret;
	u8 val;

	ret = s2mps23_read_reg(s2mps23->i2c,
				rdev->desc->enable_reg, &val);
	if (ret)
		return ret;

	if (rdev->desc->enable_is_inverted)
		return (val & rdev->desc->enable_mask) == 0;
	else
		return (val & rdev->desc->enable_mask) != 0;
}

static int get_ramp_delay(int ramp_delay)
{
	unsigned char cnt = 0;

	ramp_delay /= 6;

	while (true) {
		ramp_delay = ramp_delay >> 1;
		if (ramp_delay == 0)
			break;
		cnt++;
	}
	return cnt;
}

static int s2m_set_ramp_delay(struct regulator_dev *rdev, int ramp_delay)
{
	struct s2mps23_info *s2mps23 = rdev_get_drvdata(rdev);
	int ramp_shift, reg_id = rdev_get_id(rdev);
	int ramp_addr;
	unsigned int ramp_value = 0;

	ramp_value = get_ramp_delay(ramp_delay / 1000);
	if (ramp_value > 4) {
		pr_warn("%s: ramp_delay: %d not supported\n",
			rdev->desc->name, ramp_delay);
	}
	ramp_value = 0x00;	// 6.25mv/us fixed

	switch (reg_id) {
	case S2MPS23_BUCK4:
	case S2MPS23_BUCK8:
		ramp_shift = 6;
		break;
/*	case S2MPS23_BUCK3:	BUCK3 not use */
	case S2MPS23_BUCK7:
		ramp_shift = 4;
		break;
/*	case S2MPS23_BUCK2:	BUCK2 not use */
	case S2MPS23_BUCK6:
		ramp_shift = 2;
		break;
	case S2MPS23_BUCK1:
	case S2MPS23_BUCK5:
	case S2MPS23_BUCK9:
		ramp_shift = 0;
		break;
	default:
		return -EINVAL;
	}

	switch (reg_id) {
	case S2MPS23_BUCK1:
/*	case S2MPS23_BUCK2:
	case S2MPS23_BUCK3:	BUCK2,3 not use */
	case S2MPS23_BUCK4:
		if (s2mps23->iodev->pmic_rev)
			ramp_addr = S2MPS23_PMIC_REG_BUCK_DVS1;
		else
			ramp_addr = S2MPS23_PMIC_REG_BUCK_DVS1_EVT0;
		break;
	case S2MPS23_BUCK5:
	case S2MPS23_BUCK6:
	case S2MPS23_BUCK7:
	case S2MPS23_BUCK8:
		if (s2mps23->iodev->pmic_rev)
			ramp_addr = S2MPS23_PMIC_REG_BUCK_DVS2;
		else
			ramp_addr = S2MPS23_PMIC_REG_BUCK_DVS2_EVT0;
		break;
	case S2MPS23_BUCK9:
		if (s2mps23->iodev->pmic_rev)
			ramp_addr = S2MPS23_PMIC_REG_BUCK_DVS3;
		else
			ramp_addr = S2MPS23_PMIC_REG_BUCK_DVS3_EVT0;
		break;
	default:
		return -EINVAL;
	}

	return s2mps23_update_reg(s2mps23->i2c, ramp_addr,
		ramp_value << ramp_shift, BUCK_RAMP_MASK << ramp_shift);
}

static int s2m_get_voltage_sel_regmap(struct regulator_dev *rdev)
{
	struct s2mps23_info *s2mps23 = rdev_get_drvdata(rdev);
	int ret;
	u8 val;

	ret = s2mps23_read_reg(s2mps23->i2c, rdev->desc->vsel_reg, &val);
	if (ret)
		return ret;

	val &= rdev->desc->vsel_mask;

	return val;
}

static int s2m_set_voltage_sel_regmap(struct regulator_dev *rdev, unsigned sel)
{
	struct s2mps23_info *s2mps23 = rdev_get_drvdata(rdev);
	int reg_id = rdev_get_id(rdev);
	int ret;
	char name[16];

	/* voltage information logging to snapshot feature */
	snprintf(name, sizeof(name), "LDO%d", (reg_id - S2MPS23_LDO1) + 1);
	ret = s2mps23_update_reg(s2mps23->i2c, rdev->desc->vsel_reg,
				  sel, rdev->desc->vsel_mask);
	if (ret < 0)
		goto out;

	if (rdev->desc->apply_bit)
		ret = s2mps23_update_reg(s2mps23->i2c, rdev->desc->apply_reg,
					 rdev->desc->apply_bit,
					 rdev->desc->apply_bit);
	return ret;
out:
	pr_warn("%s: failed to set voltage_sel_regmap\n", rdev->desc->name);
	return ret;
}

static int s2m_set_voltage_sel_regmap_buck(struct regulator_dev *rdev,
								unsigned sel)
{
	int ret = 0;
	struct s2mps23_info *s2mps23 = rdev_get_drvdata(rdev);

	ret = s2mps23_write_reg(s2mps23->i2c, rdev->desc->vsel_reg, sel);
	if (ret < 0)
		goto i2c_out;

	if (rdev->desc->apply_bit)
		ret = s2mps23_update_reg(s2mps23->i2c, rdev->desc->apply_reg,
					 rdev->desc->apply_bit,
					 rdev->desc->apply_bit);

	return ret;

i2c_out:
	pr_warn("%s: failed to set voltage_sel_regmap\n", rdev->desc->name);
	ret = -EINVAL;
	return ret;
}

static int s2m_set_voltage_time_sel(struct regulator_dev *rdev,
				   unsigned int old_selector,
				   unsigned int new_selector)
{
	unsigned int ramp_delay = 0;
	int old_volt, new_volt;

	if (rdev->constraints->ramp_delay)
		ramp_delay = rdev->constraints->ramp_delay;
	else if (rdev->desc->ramp_delay)
		ramp_delay = rdev->desc->ramp_delay;

	if (ramp_delay == 0) {
		pr_warn("%s: ramp_delay not set\n", rdev->desc->name);
		return -EINVAL;
	}

	/* sanity check */
	if (!rdev->desc->ops->list_voltage)
		return -EINVAL;

	old_volt = rdev->desc->ops->list_voltage(rdev, old_selector);
	new_volt = rdev->desc->ops->list_voltage(rdev, new_selector);

	if (old_selector < new_selector)
		return DIV_ROUND_UP(new_volt - old_volt, ramp_delay);
	else
		return DIV_ROUND_UP(old_volt - new_volt, ramp_delay);

	return 0;
}

static int s2mps23_read_pwron_status(void)
{
	u8 val;
	struct s2mps23_info *s2mps23 = s2mps23_static_info;

	s2mps23_read_reg(s2mps23->i2c, S2MPS23_PMIC_REG_STATUS1, &val);
	pr_info("%s : 0x%02hhx\n", __func__, val);

	return (val & S2MPS23_STATUS1_PWRON);
}

int pmic_read_pwrkey_status(void)
{
	return s2mps23_read_pwron_status();
}
EXPORT_SYMBOL_GPL(pmic_read_pwrkey_status);

static struct regulator_ops s2mps23_ldo_ops = {
	.list_voltage		= regulator_list_voltage_linear,
	.map_voltage		= regulator_map_voltage_linear,
	.is_enabled		= s2m_is_enabled_regmap,
	.enable			= s2m_enable,
	.disable		= s2m_disable_regmap,
	.get_voltage_sel	= s2m_get_voltage_sel_regmap,
	.set_voltage_sel	= s2m_set_voltage_sel_regmap,
	.set_voltage_time_sel	= s2m_set_voltage_time_sel,
	.set_mode		= s2m_set_mode,
};

static struct regulator_ops s2mps23_buck_ops = {
	.list_voltage		= regulator_list_voltage_linear,
	.map_voltage		= regulator_map_voltage_linear,
	.is_enabled		= s2m_is_enabled_regmap,
	.enable			= s2m_enable,
	.disable		= s2m_disable_regmap,
	.get_voltage_sel	= s2m_get_voltage_sel_regmap,
	.set_voltage_sel	= s2m_set_voltage_sel_regmap_buck,
	.set_voltage_time_sel	= s2m_set_voltage_time_sel,
	.set_mode		= s2m_set_mode,
	.set_ramp_delay		= s2m_set_ramp_delay,
};
#if 0
static struct regulator_ops s2mps23_bb_ops = {
	.list_voltage		= regulator_list_voltage_linear,
	.map_voltage		= regulator_map_voltage_linear,
	.is_enabled		= s2m_is_enabled_regmap,
	.enable			= s2m_enable,
	.disable		= s2m_disable_regmap,
	.get_voltage_sel	= s2m_get_voltage_sel_regmap,
	.set_voltage_sel	= s2m_set_voltage_sel_regmap,
	.set_voltage_time_sel	= s2m_set_voltage_time_sel,
	.set_mode		= s2m_set_mode,
};
#endif

#define _BUCK(macro)		S2MPS23_BUCK##macro
#define _buck_ops(num)		s2mps23_buck_ops##num
#define _LDO(macro)		S2MPS23_LDO##macro
#define _ldo_ops(num)		s2mps23_ldo_ops##num
#define _BB(macro)		S2MPS23_BB##macro
#define _bb_ops(num)		s2mps23_bb_ops##num

#define _REG(ctrl)		S2MPS23_PMIC_REG##ctrl
#define _TIME(macro)		S2MPS23_ENABLE_TIME##macro

#define _LDO_MIN(group)		S2MPS23_LDO_MIN##group
#define _LDO_STEP(group)	S2MPS23_LDO_STEP##group
#define _BUCK_MIN(group)	S2MPS23_BUCK_MIN##group
#define _BUCK_STEP(group)	S2MPS23_BUCK_STEP##group
#define _BB_MIN(group)		S2MPS23_BB_MIN##group
#define _BB_STEP(group)		S2MPS23_BB_STEP##group

#define BUCK_DESC(_name, _id, _ops, g, v, e, t)	{		\
	.name		= _name,				\
	.id		= _id,					\
	.ops		= _ops,					\
	.type		= REGULATOR_VOLTAGE,			\
	.owner		= THIS_MODULE,				\
	.min_uV		= _BUCK_MIN(g),				\
	.uV_step	= _BUCK_STEP(g),			\
	.n_voltages	= S2MPS23_BUCK_N_VOLTAGES,		\
	.vsel_reg	= v,					\
	.vsel_mask	= S2MPS23_BUCK_VSEL_MASK,		\
	.enable_reg	= e,					\
	.enable_mask	= S2MPS23_ENABLE_MASK,			\
	.enable_time	= t,					\
	.of_map_mode	= s2mps23_of_map_mode			\
}

#define LDO_DESC(_name, _id, _ops, g, v, e, t)	{		\
	.name		= _name,				\
	.id		= _id,					\
	.ops		= _ops,					\
	.type		= REGULATOR_VOLTAGE,			\
	.owner		= THIS_MODULE,				\
	.min_uV		= _LDO_MIN(g),				\
	.uV_step	= _LDO_STEP(g),				\
	.n_voltages	= S2MPS23_LDO_N_VOLTAGES,		\
	.vsel_reg	= v,					\
	.vsel_mask	= S2MPS23_LDO_VSEL_MASK,		\
	.enable_reg	= e,					\
	.enable_mask	= S2MPS23_ENABLE_MASK,			\
	.enable_time	= t,					\
	.of_map_mode	= s2mps23_of_map_mode			\
}
#if 0
#define BB_DESC(_name, _id, _ops, g, v, e, t)	{		\
	.name		= _name,				\
	.id		= _id,					\
	.ops		= _ops,					\
	.type		= REGULATOR_VOLTAGE,			\
	.owner		= THIS_MODULE,				\
	.min_uV		= _BB_MIN(),				\
	.uV_step	= _BB_STEP(),				\
	.n_voltages	= S2MPS23_BB_N_VOLTAGES,		\
	.vsel_reg	= v,					\
	.vsel_mask	= S2MPS23_BB_VSEL_MASK,			\
	.enable_reg	= e,					\
	.enable_mask	= S2MPS23_ENABLE_MASK,			\
	.enable_time	= t,					\
	.of_map_mode	= s2mps23_of_map_mode			\
}
#endif

/* EVT0 */
#define S2MPS23_REGULATOR_EVT0_MAX	(27)
static struct regulator_desc regulators_evt0[S2MPS23_REGULATOR_EVT0_MAX] = {
	/* name, id, ops, group, vsel_reg, enable_reg, ramp_delay */
	LDO_DESC("LDO1", _LDO(1), &_ldo_ops(), 1_EVT0, _REG(_L1CTRL_EVT0), _REG(_L1CTRL_EVT0), _TIME(_LDO)),
	LDO_DESC("LDO2", _LDO(2), &_ldo_ops(), 4_EVT0, _REG(_L2CTRL_EVT0), _REG(_L2CTRL_EVT0), _TIME(_LDO)),
/*	LDO_DESC("LDO3", _LDO(3), &_ldo_ops(), 2_EVT0, _REG(_L3CTRL_EVT0), _REG(_L3CTRL_EVT0), _TIME(_LDO)),
	LDO_DESC("LDO4", _LDO(4), &_ldo_ops(), 3_EVT0, _REG(_L4CTRL_EVT0), _REG(_L4CTRL_EVT0), _TIME(_LDO)),
	LDO_DESC("LDO5", _LDO(5), &_ldo_ops(), 3_EVT0, _REG(_L5CTRL1_EVT0), _REG(_L5CTRL1_EVT0), _TIME(_LDO)),	*/
	LDO_DESC("LDO6", _LDO(6), &_ldo_ops(), 3_EVT0, _REG(_L6CTRL_EVT0), _REG(_L6CTRL_EVT0), _TIME(_LDO)),
/*	LDO_DESC("LDO7", _LDO(7), &_ldo_ops(), 3_EVT0, _REG(_L7CTRL_EVT0), _REG(_L7CTRL_EVT0), _TIME(_LDO)),	*/
	LDO_DESC("LDO8", _LDO(8), &_ldo_ops(), 1_EVT0, _REG(_L8CTRL_EVT0), _REG(_L8CTRL_EVT0), _TIME(_LDO)),
	LDO_DESC("LDO9", _LDO(9), &_ldo_ops(), 1_EVT0, _REG(_L9CTRL_EVT0), _REG(_L9CTRL_EVT0), _TIME(_LDO)),
	LDO_DESC("LDO10", _LDO(10), &_ldo_ops(), 1_EVT0, _REG(_L10CTRL_EVT0), _REG(_L10CTRL_EVT0), _TIME(_LDO)),
	LDO_DESC("LDO11", _LDO(11), &_ldo_ops(), 4_EVT0, _REG(_L11CTRL_EVT0), _REG(_L11CTRL_EVT0), _TIME(_LDO)),
	LDO_DESC("LDO12", _LDO(12), &_ldo_ops(), 4_EVT0, _REG(_L12CTRL_EVT0), _REG(_L12CTRL_EVT0), _TIME(_LDO)),
	LDO_DESC("LDO13", _LDO(13), &_ldo_ops(), 1_EVT0, _REG(_L13CTRL_EVT0), _REG(_L13CTRL_EVT0), _TIME(_LDO)),
/*	LDO_DESC("LDO14", _LDO(14), &_ldo_ops(), 4_EVT0, _REG(_L14CTRL_EVT0), _REG(_L14CTRL_EVT0), _TIME(_LDO)),
	LDO_DESC("LDO15", _LDO(15), &_ldo_ops(), 4_EVT0, _REG(_L15CTRL_EVT0), _REG(_L15CTRL_EVT0), _TIME(_LDO)),
	LDO_DESC("LDO16", _LDO(16), &_ldo_ops(), 4_EVT0, _REG(_L16CTRL_EVT0), _REG(_L16CTRL_EVT0), _TIME(_LDO)),
	LDO_DESC("LDO17", _LDO(17), &_ldo_ops(), 1_EVT0, _REG(_L17CTRL_EVT0), _REG(_L17CTRL_EVT0), _TIME(_LDO)),
	LDO_DESC("LDO18", _LDO(18), &_ldo_ops(), 3_EVT0, _REG(_L18CTRL1_EVT0), _REG(_L18CTRL1_EVT0), _TIME(_LDO)),
	LDO_DESC("LDO19", _LDO(19), &_ldo_ops(), 4_EVT0, _REG(_L19CTRL_EVT0), _REG(_L19CTRL_EVT0), _TIME(_LDO)),
	LDO_DESC("LDO20", _LDO(20), &_ldo_ops(), 4_EVT0, _REG(_L20CTRL_EVT0), _REG(_L20CTRL_EVT0), _TIME(_LDO)),
	LDO_DESC("LDO21", _LDO(21), &_ldo_ops(), 4_EVT0, _REG(_L21CTRL_EVT0), _REG(_L21CTRL_EVT0), _TIME(_LDO)),	*/
	LDO_DESC("LDO22", _LDO(22), &_ldo_ops(), 4_EVT0, _REG(_L22CTRL_EVT0), _REG(_L22CTRL_EVT0), _TIME(_LDO)),
	LDO_DESC("LDO23", _LDO(23), &_ldo_ops(), 4_EVT0, _REG(_L23CTRL_EVT0), _REG(_L23CTRL_EVT0), _TIME(_LDO)),
	LDO_DESC("LDO24", _LDO(24), &_ldo_ops(), 4_EVT0, _REG(_L24CTRL_EVT0), _REG(_L24CTRL_EVT0), _TIME(_LDO)),
	LDO_DESC("LDO25", _LDO(25), &_ldo_ops(), 4_EVT0, _REG(_L25CTRL_EVT0), _REG(_L25CTRL_EVT0), _TIME(_LDO)),
	LDO_DESC("LDO26", _LDO(26), &_ldo_ops(), 4_EVT0, _REG(_L26CTRL_EVT0), _REG(_L26CTRL_EVT0), _TIME(_LDO)),
	LDO_DESC("LDO27", _LDO(27), &_ldo_ops(), 4_EVT0, _REG(_L27CTRL_EVT0), _REG(_L27CTRL_EVT0), _TIME(_LDO)),
	LDO_DESC("LDO28", _LDO(28), &_ldo_ops(), 4_EVT0, _REG(_L28CTRL_EVT0), _REG(_L28CTRL_EVT0), _TIME(_LDO)),
	LDO_DESC("LDO29", _LDO(29), &_ldo_ops(), 4_EVT0, _REG(_L29CTRL_EVT0), _REG(_L29CTRL_EVT0), _TIME(_LDO)),
	LDO_DESC("LDO30", _LDO(30), &_ldo_ops(), 4_EVT0, _REG(_L30CTRL_EVT0), _REG(_L30CTRL_EVT0), _TIME(_LDO)),
	LDO_DESC("LDO31", _LDO(31), &_ldo_ops(), 4_EVT0, _REG(_L31CTRL_EVT0), _REG(_L31CTRL_EVT0), _TIME(_LDO)),
	LDO_DESC("LDO32", _LDO(32), &_ldo_ops(), 4_EVT0, _REG(_L32CTRL_EVT0), _REG(_L32CTRL_EVT0), _TIME(_LDO)),

	BUCK_DESC("BUCK1", _BUCK(1), &_buck_ops(), 1_EVT0, _REG(_B1M_OUT1_EVT0), _REG(_B1M_CTRL_EVT0), _TIME(_BUCK)),
/*	BUCK_DESC("BUCK2", _BUCK(2), &_buck_ops(), 1_EVT0, _REG(_B2M_OUT1_EVT0), _REG(_B2M_CTRL_EVT0), _TIME(_BUCK)),
	BUCK_DESC("BUCK3", _BUCK(3), &_buck_ops(), 1_EVT0, _REG(_B3M_OUT1_EVT0), _REG(_B3M_CTRL_EVT0), _TIME(_BUCK)),	*/
	BUCK_DESC("BUCK4", _BUCK(4), &_buck_ops(), 1_EVT0, _REG(_B4M_OUT2_EVT0), _REG(_B4M_CTRL_EVT0), _TIME(_BUCK)),
	BUCK_DESC("BUCK5", _BUCK(5), &_buck_ops(), 1_EVT0, _REG(_B5M_OUT1_EVT0), _REG(_B5M_CTRL_EVT0), _TIME(_BUCK)),
	BUCK_DESC("BUCK6", _BUCK(6), &_buck_ops(), 1_EVT0, _REG(_B6M_OUT1_EVT0), _REG(_B6M_CTRL_EVT0), _TIME(_BUCK)),
	BUCK_DESC("BUCK7", _BUCK(7), &_buck_ops(), 1_EVT0, _REG(_B7M_OUT1_EVT0), _REG(_B7M_CTRL_EVT0), _TIME(_BUCK)),
	BUCK_DESC("BUCK8", _BUCK(8), &_buck_ops(), 1_EVT0, _REG(_B8M_OUT1_EVT0), _REG(_B8M_CTRL_EVT0), _TIME(_BUCK)),
	BUCK_DESC("BUCK9", _BUCK(9), &_buck_ops(), 2_EVT0, _REG(_B9M_OUT1_EVT0), _REG(_B9M_CTRL_EVT0), _TIME(_BUCK)),
};

/* EVT1 */
static struct regulator_desc regulators[S2MPS23_REGULATOR_MAX] = {
	/* name, id, ops, group, vsel_reg, enable_reg, ramp_delay */
	LDO_DESC("LDO1", _LDO(1), &_ldo_ops(), 1, _REG(_L1CTRL), _REG(_L1CTRL), _TIME(_LDO)),
	LDO_DESC("LDO2", _LDO(2), &_ldo_ops(), 4, _REG(_L2CTRL), _REG(_L2CTRL), _TIME(_LDO)),
/*	LDO_DESC("LDO3", _LDO(3), &_ldo_ops(), 2, _REG(_L3CTRL), _REG(_L3CTRL), _TIME(_LDO)),
	LDO_DESC("LDO4", _LDO(4), &_ldo_ops(), 3, _REG(_L4CTRL), _REG(_L4CTRL), _TIME(_LDO)),
	LDO_DESC("LDO5", _LDO(5), &_ldo_ops(), 3, _REG(_L5CTRL1), _REG(_L5CTRL1), _TIME(_LDO)),	*/
	LDO_DESC("LDO6", _LDO(6), &_ldo_ops(), 3, _REG(_L6CTRL), _REG(_L6CTRL), _TIME(_LDO)),
/*	LDO_DESC("LDO7", _LDO(7), &_ldo_ops(), 1, _REG(_L7CTRL), _REG(_L7CTRL), _TIME(_LDO)),	*/
	LDO_DESC("LDO8", _LDO(8), &_ldo_ops(), 1, _REG(_L8CTRL), _REG(_L8CTRL), _TIME(_LDO)),
	LDO_DESC("LDO9", _LDO(9), &_ldo_ops(), 1, _REG(_L9CTRL), _REG(_L9CTRL), _TIME(_LDO)),
	LDO_DESC("LDO10", _LDO(10), &_ldo_ops(), 1, _REG(_L10CTRL), _REG(_L10CTRL), _TIME(_LDO)),
	LDO_DESC("LDO11", _LDO(11), &_ldo_ops(), 4, _REG(_L11CTRL), _REG(_L11CTRL), _TIME(_LDO)),
	LDO_DESC("LDO12", _LDO(12), &_ldo_ops(), 4, _REG(_L12CTRL), _REG(_L12CTRL), _TIME(_LDO)),
	LDO_DESC("LDO13", _LDO(13), &_ldo_ops(), 1, _REG(_L13CTRL), _REG(_L13CTRL), _TIME(_LDO)),
/*	LDO_DESC("LDO14", _LDO(14), &_ldo_ops(), 1, _REG(_L14CTRL), _REG(_L14CTRL), _TIME(_LDO)),
	LDO_DESC("LDO15", _LDO(15), &_ldo_ops(), 4, _REG(_L15CTRL), _REG(_L15CTRL), _TIME(_LDO)),
	LDO_DESC("LDO16", _LDO(16), &_ldo_ops(), 4, _REG(_L16CTRL), _REG(_L16CTRL), _TIME(_LDO)),
	LDO_DESC("LDO17", _LDO(17), &_ldo_ops(), 3, _REG(_L17CTRL), _REG(_L17CTRL), _TIME(_LDO)),
	LDO_DESC("LDO18", _LDO(18), &_ldo_ops(), 3, _REG(_L18CTRL1), _REG(_L18CTRL1), _TIME(_LDO)),	*/
	LDO_DESC("LDO19", _LDO(19), &_ldo_ops(), 4, _REG(_L19CTRL), _REG(_L19CTRL), _TIME(_LDO)),
/*	LDO_DESC("LDO20", _LDO(20), &_ldo_ops(), 4, _REG(_L20CTRL), _REG(_L20CTRL), _TIME(_LDO)),
	LDO_DESC("LDO21", _LDO(21), &_ldo_ops(), 4, _REG(_L21CTRL), _REG(_L21CTRL), _TIME(_LDO)),	*/
	LDO_DESC("LDO22", _LDO(22), &_ldo_ops(), 4, _REG(_L22CTRL), _REG(_L22CTRL), _TIME(_LDO)),
	LDO_DESC("LDO23", _LDO(23), &_ldo_ops(), 4, _REG(_L23CTRL), _REG(_L23CTRL), _TIME(_LDO)),
	LDO_DESC("LDO24", _LDO(24), &_ldo_ops(), 4, _REG(_L24CTRL), _REG(_L24CTRL), _TIME(_LDO)),
	LDO_DESC("LDO25", _LDO(25), &_ldo_ops(), 4, _REG(_L25CTRL), _REG(_L25CTRL), _TIME(_LDO)),
	LDO_DESC("LDO26", _LDO(26), &_ldo_ops(), 4, _REG(_L26CTRL), _REG(_L26CTRL), _TIME(_LDO)),
	LDO_DESC("LDO27", _LDO(27), &_ldo_ops(), 4, _REG(_L27CTRL), _REG(_L27CTRL), _TIME(_LDO)),
	LDO_DESC("LDO28", _LDO(28), &_ldo_ops(), 4, _REG(_L28CTRL), _REG(_L28CTRL), _TIME(_LDO)),
	LDO_DESC("LDO29", _LDO(29), &_ldo_ops(), 4, _REG(_L29CTRL), _REG(_L29CTRL), _TIME(_LDO)),
	LDO_DESC("LDO30", _LDO(30), &_ldo_ops(), 4, _REG(_L30CTRL), _REG(_L30CTRL), _TIME(_LDO)),
	LDO_DESC("LDO31", _LDO(31), &_ldo_ops(), 4, _REG(_L31CTRL), _REG(_L31CTRL), _TIME(_LDO)),
	LDO_DESC("LDO32", _LDO(32), &_ldo_ops(), 4, _REG(_L32CTRL), _REG(_L32CTRL), _TIME(_LDO)),
/*	LDO_DESC("LDO33", _LDO(33), &_ldo_ops(), 3, _REG(_L33CTRL), _REG(_L33CTRL), _TIME(_LDO)),	*/

	BUCK_DESC("BUCK1", _BUCK(1), &_buck_ops(), 1, _REG(_B1M_OUT1), _REG(_B1M_CTRL), _TIME(_BUCK)),
/*	BUCK_DESC("BUCK2", _BUCK(2), &_buck_ops(), 1, _REG(_B2M_OUT1), _REG(_B2M_CTRL), _TIME(_BUCK)),
	BUCK_DESC("BUCK3", _BUCK(3), &_buck_ops(), 1, _REG(_B3M_OUT1), _REG(_B3M_CTRL), _TIME(_BUCK)),	*/
	BUCK_DESC("BUCK4", _BUCK(4), &_buck_ops(), 1, _REG(_B4M_OUT2), _REG(_B4M_CTRL), _TIME(_BUCK)),
	BUCK_DESC("BUCK5", _BUCK(5), &_buck_ops(), 1, _REG(_B5M_OUT1), _REG(_B5M_CTRL), _TIME(_BUCK)),
	BUCK_DESC("BUCK6", _BUCK(6), &_buck_ops(), 1, _REG(_B6M_OUT1), _REG(_B6M_CTRL), _TIME(_BUCK)),
	BUCK_DESC("BUCK7", _BUCK(7), &_buck_ops(), 1, _REG(_B7M_OUT2), _REG(_B7M_CTRL), _TIME(_BUCK)),
	BUCK_DESC("BUCK8", _BUCK(8), &_buck_ops(), 1, _REG(_B8M_OUT1), _REG(_B8M_CTRL), _TIME(_BUCK)),
	BUCK_DESC("BUCK9", _BUCK(9), &_buck_ops(), 2, _REG(_B9M_OUT1), _REG(_B9M_CTRL), _TIME(_BUCK)),
};
#if IS_ENABLED(CONFIG_OF)
static int s2mps23_pmic_dt_parse_pdata(struct s2mps23_dev *iodev,
					struct s2mps23_platform_data *pdata)
{
	struct device_node *pmic_np, *regulators_np, *reg_np;
	struct s2mps23_regulator_data *rdata;
	u32 i;
	int ret, len;
	u32 val;
	const u32 *p;

	pdata->smpl_warn_vth = 0;
	pdata->smpl_warn_hys = 0;

	pmic_np = iodev->dev->of_node;
	if (!pmic_np) {
		dev_err(iodev->dev, "could not find pmic sub-node\n");
		return -ENODEV;
	}
#if IS_ENABLED(CONFIG_EXYNOS_ACPM)
	acpm_mfd_node = pmic_np;
#endif

	/* get 1 gpio values */
	if (of_gpio_count(pmic_np) < 1) {
		dev_err(iodev->dev, "could not find pmic gpios\n");
		return -EINVAL;
	}
	pdata->smpl_warn = of_get_gpio(pmic_np, 0);

	ret = of_property_read_u32(pmic_np, "smpl_warn_en", &val);
	if (ret)
		return -EINVAL;
	pdata->smpl_warn_en = !!val;

	/* TODO : don't parshed, only default setting */
#if 0
	ret = of_property_read_u32(pmic_np, "smpl_warn_vth", &val);
	if (ret)
		return -EINVAL;
	pdata->smpl_warn_vth = val;
	ret = of_property_read_u32(pmic_np, "smpl_warn_hys", &val);
	if (ret)
		return -EINVAL;
	pdata->smpl_warn_hys = val;
#endif
	ret = of_property_read_u32(pmic_np, "ocp_warn1_en", &val);
	if (ret)
		return -EINVAL;
	pdata->ocp_warn1_en = !!val;

	ret = of_property_read_u32(pmic_np, "ocp_warn1_cnt", &val);
	if (ret)
		return -EINVAL;
	pdata->ocp_warn1_cnt = val;

	ret = of_property_read_u32(pmic_np, "ocp_warn1_dvs_mask", &val);
	if (ret)
		return -EINVAL;
	pdata->ocp_warn1_dvs_mask = val;

	ret = of_property_read_u32(pmic_np, "ocp_warn1_lv", &val);
	if (ret)
		return -EINVAL;
	pdata->ocp_warn1_lv = val;

	ret = of_property_read_u32(pmic_np, "adc_mode", &val);
	if (ret)
		return -EINVAL;
	pdata->adc_mode = val;

	if (iodev->pmic_rev) {
		p = of_get_property(pmic_np, "control_sel", &len);
		if (!p) {
			pr_info("%s : (ERROR) control_sel isn't parsing\n", __func__);
			return -EINVAL;
		}

		len = len / sizeof(u32);
		if (len != S2MPS23_CONTROL_SEL_NUM) {
			pr_info("%s : (ERROR) control_sel num isn't not equal\n", __func__);
			return -EINVAL;
		}

		pdata->control_sel = devm_kzalloc(iodev->dev, sizeof(u32) * len, GFP_KERNEL);
		if (!(pdata->control_sel)) {
			dev_err(iodev->dev,
					"(ERROR) could not allocate memory for control_sel data\n");
			return -ENOMEM;
		}

		for (i = 0; i < len; i++) {
			ret = of_property_read_u32_index(pmic_np, "control_sel", i, &pdata->control_sel[i]);
			if (ret) {
				pr_info("%s : (ERROR) control_sel%d is empty\n", __func__, i + 1);
				pdata->control_sel[i] = 0x1FF;
			}
		}
	} else {
		p = of_get_property(pmic_np, "control_sel_evt0", &len);
		if (!p) {
			pr_info("%s : (ERROR) control_sel isn't parsing\n", __func__);
			return -EINVAL;
		}

		len = len / sizeof(u32);
		if (len != S2MPS23_CONTROL_SEL_NUM_EVT0) {
			pr_info("%s : (ERROR) control_sel num isn't not equal\n", __func__);
			return -EINVAL;
		}

		pdata->control_sel = devm_kzalloc(iodev->dev, sizeof(u32) * len, GFP_KERNEL);
		if (!(pdata->control_sel)) {
			dev_err(iodev->dev,
					"(ERROR) could not allocate memory for control_sel data\n");
			return -ENOMEM;
		}

		for (i = 0; i < len; i++) {
			ret = of_property_read_u32_index(pmic_np, "control_sel_evt0", i, &pdata->control_sel[i]);
			if (ret) {
				pr_info("%s : (ERROR) control_sel%d is empty\n", __func__, i + 1);
				pdata->control_sel[i] = 0x1FF;
			}
		}
	}

	regulators_np = of_find_node_by_name(pmic_np, "regulators");
	if (!regulators_np) {
		dev_err(iodev->dev, "could not find regulators sub-node\n");
		return -EINVAL;
	}

	/* count the number of regulators to be supported in pmic */
	pdata->num_regulators = 0;
	for_each_child_of_node(regulators_np, reg_np) {
		pdata->num_regulators++;
	}
	if (!iodev->pmic_rev)
		pdata->num_regulators = S2MPS23_REGULATOR_EVT0_MAX;

	rdata = devm_kzalloc(iodev->dev, sizeof(*rdata) *
				pdata->num_regulators, GFP_KERNEL);
	if (!rdata) {
		dev_err(iodev->dev,
			"could not allocate memory for regulator data\n");
		return -ENOMEM;
	}

	pdata->regulators = rdata;
	pdata->num_rdata = 0;
	for_each_child_of_node(regulators_np, reg_np) {
		if (iodev->pmic_rev) {
			for (i = 0; i < ARRAY_SIZE(regulators); i++)
				if (!of_node_cmp(reg_np->name, regulators[i].name))
					break;

			if (i == ARRAY_SIZE(regulators)) {
				dev_warn(iodev->dev,
						"don't know how to configure regulator %s\n",
						reg_np->name);
				continue;
			}

			rdata->id = i;
			rdata->initdata = of_get_regulator_init_data(iodev->dev, reg_np,
					&regulators[i]);
			rdata->reg_node = reg_np;
			rdata++;
			pdata->num_rdata++;
		} else {
			for (i = 0; i < ARRAY_SIZE(regulators_evt0); i++)
				if (!of_node_cmp(reg_np->name, regulators_evt0[i].name))
					break;

			if (i == ARRAY_SIZE(regulators_evt0)) {
				dev_warn(iodev->dev,
						"don't know how to configure regulator %s\n",
						reg_np->name);
				continue;
			}

			rdata->id = i;
			rdata->initdata = of_get_regulator_init_data(iodev->dev, reg_np,
					&regulators_evt0[i]);
			rdata->reg_node = reg_np;
			rdata++;
			pdata->num_rdata++;
		}
	}

	return 0;
}
#else
static int s2mps23_pmic_dt_parse_pdata(struct s2mps23_pmic_dev *iodev,
					struct s2mps23_platform_data *pdata)
{
	return 0;
}
#endif /* CONFIG_OF */

#if 0
#if IS_ENABLED(CONFIG_EXYNOS_OCP)
void get_s2mps23_i2c(struct i2c_client **i2c)
{
	if(!s2mps23_static_info)
		return;

	*i2c = s2mps23_static_info->i2c;
}
#endif
#endif

#if IS_ENABLED(CONFIG_SEC_PM_BIGDATA_UEVENT)
void send_hqm_pmtp_work(struct work_struct *work)
{
	hqm_device_info hqm_info;
	char feature[HQM_FEATURE_LEN] ="PMTP";
	 
	memcpy(hqm_info.feature, feature, HQM_FEATURE_LEN);
	send_uevent_via_hqm_device(hqm_info);
}

void send_hqm_bocp_work(struct work_struct *work)
{
	hqm_device_info hqm_info;
	char feature[HQM_FEATURE_LEN] ="BOCP";

	memcpy(hqm_info.feature, feature, HQM_FEATURE_LEN);
	send_uevent_via_hqm_device(hqm_info);
}
#endif /* CONFIG_SEC_PM_BIGDATA_UEVENT */

static irqreturn_t s2mps23_buck_ocp_irq(int irq, void *data)
{
	struct s2mps23_info *s2mps23 = data;
	u32 i;

	mutex_lock(&s2mps23->lock);

	for (i = 0; i < S2MPS23_BUCK_MAX; i++) {
		if (s2mps23_static_info->buck_ocp_irq[i] == irq) {
			s2mps23_buck_ocp_cnt[i]++;
#if IS_ENABLED(CONFIG_SEC_PM_BIGDATA)
			hqm_bocp_cnt[i]++;
#endif /* CONFIG_SEC_PM_BIGDATA */
			pr_info("%s : BUCK[%d] OCP IRQ : %d, %d\n",
				__func__, i + 1, s2mps23_buck_ocp_cnt[i], irq);
			break;
		}
	}

	mutex_unlock(&s2mps23->lock);

#if IS_ENABLED(CONFIG_SEC_PM_DEBUG)
	pr_info("BUCK OCP: BIG: %u kHz, MID: %u kHz, LITTLE: %u kHz\n",
			cpufreq_get(7), cpufreq_get(4), cpufreq_get(0));
#endif /* CONFIG_SEC_PM_DEBUG */

#if IS_ENABLED(CONFIG_SEC_DEBUG_EXTRA_INFO)
	secdbg_exin_set_main_ocp(s2mps23_buck_ocp_cnt, s2mps23_buck_oi_cnt);
#endif

#if IS_ENABLED(CONFIG_SEC_PM_BIGDATA_UEVENT)
	cancel_delayed_work(&s2mps23->hqm_bocp_work);
	schedule_delayed_work(&s2mps23->hqm_bocp_work, 5 * HZ);
#endif /* CONFIG_SEC_PM_BIGDATA_UEVENT */

	return IRQ_HANDLED;
}

static irqreturn_t s2mps23_buck_oi_irq(int irq, void *data)
{
	struct s2mps23_info *s2mps23 = data;
	u32 i;

	mutex_lock(&s2mps23->lock);

	for (i = 0; i < S2MPS23_BUCK_MAX; i++) {
		if (s2mps23_static_info->buck_oi_irq[i] == irq) {
			s2mps23_buck_oi_cnt[i]++;
			pr_info("%s : BUCK[%d] OI IRQ : %d, %d\n",
				__func__, i + 1, s2mps23_buck_oi_cnt[i], irq);
			break;
		}
	}

	mutex_unlock(&s2mps23->lock);

#if IS_ENABLED(CONFIG_SEC_PM_DEBUG)
	pr_info("BUCK OI: BIG: %u kHz, MID: %u kHz, LITTLE: %u kHz\n",
			cpufreq_get(7), cpufreq_get(4), cpufreq_get(0));
#endif /* CONFIG_SEC_PM_DEBUG */

#if IS_ENABLED(CONFIG_SEC_DEBUG_EXTRA_INFO)
	secdbg_exin_set_main_ocp(s2mps23_buck_ocp_cnt, s2mps23_buck_oi_cnt);
#endif

	return IRQ_HANDLED;
}

static irqreturn_t s2mps23_temp_irq(int irq, void *data)
{
	struct s2mps23_info *s2mps23 = data;

	mutex_lock(&s2mps23->lock);

	if (s2mps23_static_info->temp_irq[0] == irq) {
		s2mps23_temp_cnt[0]++;
		pr_info("%s: PMIC thermal 120C IRQ : %d, %d\n",
			__func__, s2mps23_temp_cnt[0], irq);
	} else if (s2mps23_static_info->temp_irq[1] == irq) {
		s2mps23_temp_cnt[1]++;
		pr_info("%s: PMIC thermal 140C IRQ : %d, %d\n",
			__func__, s2mps23_temp_cnt[1], irq);
	}

	mutex_unlock(&s2mps23->lock);

#if IS_ENABLED(CONFIG_SEC_PM_BIGDATA_UEVENT)
	cancel_delayed_work(&s2mps23->hqm_pmtp_work);
	schedule_delayed_work(&s2mps23->hqm_pmtp_work, 5 * HZ);
#endif /* CONFIG_SEC_PM_BIGDATA_UEVENT */

	return IRQ_HANDLED;
}

#if IS_ENABLED(CONFIG_SEC_PM)
static ssize_t show_ap_pmic_th120C_count(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	int cnt = s2mps23_temp_cnt[0];

	pr_info("%s: PMIC thermal 120C count: %d\n", __func__, s2mps23_temp_cnt[0]);
	s2mps23_temp_cnt[0] = 0;

	return sprintf(buf, "%d\n", cnt);
}

static ssize_t store_ap_pmic_th120C_count(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t count)
{
	int ret, val;

	ret = kstrtoint(buf, 0, &val);
	if (ret < 0)
		return ret;

	s2mps23_temp_cnt[0] = val;

	return count;
}

static ssize_t show_ap_pmic_th140C_count(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	int cnt = s2mps23_temp_cnt[1];

	pr_info("%s: PMIC thermal 140C count: %d\n", __func__, s2mps23_temp_cnt[1]);
	s2mps23_temp_cnt[1] = 0;

	return sprintf(buf, "%d\n", cnt);
}

static ssize_t store_ap_pmic_th140C_count(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t count)
{
	int ret, val;

	ret = kstrtoint(buf, 0, &val);
	if (ret < 0)
		return ret;

	s2mps23_temp_cnt[1] = val;

	return count;
}

static ssize_t show_ap_pmic_buck_ocp_count(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	int i, buf_offset = 0;

	for (i = 0; i < S2MPS23_BUCK_MAX; i++)
		if (s2mps23_static_info->buck_ocp_irq[i])
			buf_offset += sprintf(buf + buf_offset, "B%d : %d\n",
					i+1, s2mps23_buck_ocp_cnt[i]);

	return buf_offset;
}

#if IS_ENABLED(CONFIG_SEC_PM_BIGDATA)
static ssize_t hqm_bocp_count_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	int i, buf_offset = 0;

	for (i = 0; i < S2MPS23_BUCK_MAX; i++) {
		if (s2mps23_static_info->buck_ocp_irq[i]) {
			buf_offset += sprintf(buf + buf_offset, "\"B%d\":\"%d\",",
					i+1, hqm_bocp_cnt[i]);
			hqm_bocp_cnt[i] = 0;
		}
	}
	if(buf_offset > 0)
		buf[--buf_offset] = '\0';

	return buf_offset;
}
#endif /* CONFIG_SEC_PM_BIGDATA */

static ssize_t pmic_id_show(struct device *dev, struct device_attribute *attr,
		char *buf)
{
	int pmic_id = s2mps23_static_info->iodev->pmic_rev;

	return sprintf(buf, "0x%02X\n", pmic_id);
}

static ssize_t chg_det_show(struct device *dev, struct device_attribute *attr,
		char *buf)
{
	int ret, chg_det;
	u8 val;

	ret = s2mps23_read_reg(s2mps23_static_info->i2c, S2MPS23_PMIC_REG_STATUS1, &val);
	if(ret)
		chg_det = -1;
	else
		chg_det = !(val & (1 << 2));

	pr_info("%s: ap pmic chg det: %d\n", __func__, chg_det);

	return sprintf(buf, "%d\n", chg_det);
}

static ssize_t show_manual_reset(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	int ret;
	bool enabled;
	u8 val;

	ret = s2mps23_read_reg(s2mps23_static_info->i2c, S2MPS23_PMIC_REG_CTRL1, &val);
	if (ret)
		return ret;

	enabled = !!(val & (1 << 4));

	pr_info("%s: %s[0x%02X]\n", __func__, enabled ? "enabled" :  "disabled",
			val);

	return sprintf(buf, "%s\n", enabled ? "enabled" :  "disabled");
}

static ssize_t store_manual_reset(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t count)
{
	int ret;
	bool enable;
	u8 val;

	ret = strtobool(buf, &enable);
	if (ret)
		return ret;

	ret = s2mps23_read_reg(s2mps23_static_info->i2c, S2MPS23_PMIC_REG_CTRL1, &val);
	if (ret)
		return ret;

	val &= ~(1 << 4);
	val |= enable << 4;

	ret = s2mps23_write_reg(s2mps23_static_info->i2c, S2MPS23_PMIC_REG_CTRL1, val);
	if (ret)
		return ret;

	pr_info("%s: %d [0x%02X]\n", __func__, enable, val);

	return count;
}

static DEVICE_ATTR(th120C_count, 0664, show_ap_pmic_th120C_count,
		store_ap_pmic_th120C_count);
static DEVICE_ATTR(th140C_count, 0664, show_ap_pmic_th140C_count,
		store_ap_pmic_th140C_count);
static DEVICE_ATTR(buck_ocp_count, 0444, show_ap_pmic_buck_ocp_count, NULL);
#if IS_ENABLED(CONFIG_SEC_PM_BIGDATA)
static DEVICE_ATTR_RO(hqm_bocp_count);
#endif /* CONFIG_SEC_PM_BIGDATA */
static DEVICE_ATTR_RO(pmic_id);
static DEVICE_ATTR_RO(chg_det);
static DEVICE_ATTR(manual_reset, 0664, show_manual_reset, store_manual_reset);

static struct attribute *ap_pmic_attributes[] = {
	&dev_attr_th120C_count.attr,
	&dev_attr_th140C_count.attr,
	&dev_attr_buck_ocp_count.attr,
#if IS_ENABLED(CONFIG_SEC_PM_BIGDATA)
	&dev_attr_hqm_bocp_count.attr,
#endif /* CONFIG_SEC_PM_BIGDATA */
	&dev_attr_pmic_id.attr,
	&dev_attr_chg_det.attr,
	&dev_attr_manual_reset.attr,
	NULL
};

static const struct attribute_group ap_pmic_attr_group = {
	.attrs = ap_pmic_attributes,
};
#endif /* CONFIG_SEC_PM */


struct s2mps23_oi_data {
	u8 reg;
	u8 val;
};

#define OI_NUM		(9)
#define OI_NUM_EVT0	(9)
#define DECLARE_OI(_reg, _val)	{ .reg = (_reg), .val = (_val) }
static const struct s2mps23_oi_data s2mps23_oi[OI_NUM] = {
	/* BUCK 1~9 OI function enable */
	DECLARE_OI(S2MPS23_PMIC_REG_BUCK_OI_EN1, 0xFF),
	DECLARE_OI(S2MPS23_PMIC_REG_BUCK_OI_EN2, 0x01),
	/* BUCK 1~9 OI power down disable */
	DECLARE_OI(S2MPS23_PMIC_REG_BUCK_OI_PD_EN1, 0x00),
	DECLARE_OI(S2MPS23_PMIC_REG_BUCK_OI_PD_EN2, 0x00),
	/* OI detection time window : 300us, OI comp. output count : 50 times */
	DECLARE_OI(S2MPS23_PMIC_REG_BUCK_OI_CTRL1, 0xCC),
	DECLARE_OI(S2MPS23_PMIC_REG_BUCK_OI_CTRL2, 0xCC),
	DECLARE_OI(S2MPS23_PMIC_REG_BUCK_OI_CTRL3, 0xCC),
	DECLARE_OI(S2MPS23_PMIC_REG_BUCK_OI_CTRL4, 0xCC),
	DECLARE_OI(S2MPS23_PMIC_REG_BUCK_OI_CTRL5, 0x0C),
};

static const struct s2mps23_oi_data s2mps23_oi_evt0[OI_NUM_EVT0] = {
	/* BUCK 1~9 OI function enable */
	DECLARE_OI(S2MPS23_PMIC_REG_BUCK_OI_EN1_EVT0, 0xFF),
	DECLARE_OI(S2MPS23_PMIC_REG_BUCK_OI_EN2_EVT0, 0x01),
	/* BUCK 1~9 OI power down disable */
	DECLARE_OI(S2MPS23_PMIC_REG_BUCK_OI_PD_EN1_EVT0, 0x00),
	DECLARE_OI(S2MPS23_PMIC_REG_BUCK_OI_PD_EN2_EVT0, 0x00),
	/* OI detection time window : 300us, OI comp. output count : 50 times */
	DECLARE_OI(S2MPS23_PMIC_REG_BUCK_OI_CTRL1_EVT0, 0xCC),
	DECLARE_OI(S2MPS23_PMIC_REG_BUCK_OI_CTRL2_EVT0, 0xCC),
	DECLARE_OI(S2MPS23_PMIC_REG_BUCK_OI_CTRL3_EVT0, 0xCC),
	DECLARE_OI(S2MPS23_PMIC_REG_BUCK_OI_CTRL4_EVT0, 0xCC),
	DECLARE_OI(S2MPS23_PMIC_REG_BUCK_OI_CTRL5_EVT0, 0x0C),
};

static int s2mps23_oi_function(struct s2mps23_dev *iodev)
{
	struct i2c_client *i2c = iodev->pmic;
	u32 i;
	u8 val, s, e;
	int ret, cnt = 0;
	char buf[1024] = {0, };

	if (iodev->pmic_rev) {
		for (i = 0; i < OI_NUM; i++) {
			ret = s2mps23_write_reg(i2c, s2mps23_oi[i].reg, s2mps23_oi[i].val);
			if (ret) {
				pr_err("%s: failed to write register\n", __func__);
				goto err;
			}
		}
		s = S2MPS23_PMIC_REG_BUCK_OI_EN1;
		e = S2MPS23_PMIC_REG_BUCK_OI_CTRL5;
	} else {
		for (i = 0; i < OI_NUM_EVT0; i++) {
			ret = s2mps23_write_reg(i2c, s2mps23_oi_evt0[i].reg, s2mps23_oi_evt0[i].val);
			if (ret) {
				pr_err("%s: failed to write register\n", __func__);
				goto err;
			}
		}
		s = S2MPS23_PMIC_REG_BUCK_OI_EN1_EVT0;
		e = S2MPS23_PMIC_REG_BUCK_OI_CTRL5_EVT0;
	}

	for (i = s; i <= e; i++) {
		ret = s2mps23_read_reg(i2c, i, &val);
		if (ret)
			goto err;

		cnt += snprintf(buf + cnt, sizeof(buf) - 1, "0x%x[0x%02hhx], ", i, val);
	}
	pr_info("%s: %s\n", __func__, buf);

	return 0;
err:
	return -1;
}

static int s2mps23_set_interrupt(struct platform_device *pdev,
				 struct s2mps23_info *s2mps23, int irq_base)
{
	int i, ret;

	/* BUCK 1~9 OCP interrupt */
	for (i = 0; i < S2MPS23_BUCK_MAX; i++) {
		if (i == S2MPS23_BUCK6M_OCP_IDX)
			continue;
		s2mps23->buck_ocp_irq[i] = irq_base +
					   S2MPS23_PMIC_IRQ_OCP_B1M_INT4 + i;

		ret = devm_request_threaded_irq(&pdev->dev,
						s2mps23->buck_ocp_irq[i], NULL,
						s2mps23_buck_ocp_irq, 0,
						"BUCK_OCP_IRQ", s2mps23);
		if (ret < 0) {
			dev_err(&pdev->dev,
				"Failed to request BUCK[%d] OCP IRQ: %d: %d\n",
				i + 1, s2mps23->buck_ocp_irq[i], ret);
			goto err;
		}
	}

	/* BUCK 1~9 OI interrupt */
	for (i = 0; i < S2MPS23_BUCK_MAX; i++) {
		s2mps23->buck_oi_irq[i] = irq_base +
					   S2MPS23_PMIC_IRQ_OI_B1M_INT6 + i;

		ret = devm_request_threaded_irq(&pdev->dev,
						s2mps23->buck_oi_irq[i], NULL,
						s2mps23_buck_oi_irq, 0,
						"BUCK_OI_IRQ", s2mps23);
		if (ret < 0) {
			dev_err(&pdev->dev,
				"Failed to request BUCK[%d] OCP IRQ: %d: %d\n",
				i + 1, s2mps23->buck_oi_irq[i], ret);
			goto err;
		}
	}

	/* Thermal interrupt */
	for (i = 0; i < S2MPS23_TEMP_MAX; i++) {
		s2mps23->temp_irq[i] = irq_base + S2MPS23_PMIC_IRQ_120C_INT3 + i;

		ret = devm_request_threaded_irq(&pdev->dev,
						s2mps23->temp_irq[i], NULL,
						s2mps23_temp_irq, 0,
						"TEMP_IRQ", s2mps23);
		if (ret < 0) {
			dev_err(&pdev->dev,
				"Failed to request over temperature[%d] IRQ: %d: %d\n",
				i, s2mps23->temp_irq[i], ret);
			goto err;
		}
	}

	return 0;
err:
	return -1;
}

#if IS_ENABLED(CONFIG_DRV_SAMSUNG_PMIC)
static ssize_t s2mps23_read_store(struct device *dev,
				  struct device_attribute *attr,
				  const char *buf, size_t size)
{
	struct s2mps23_info *s2mps23 = dev_get_drvdata(dev);
	int ret;
	u8 base_addr = 0, reg_addr = 0, val = 0;
#if IS_ENABLED(CONFIG_EXYNOS_ACPM)
	struct s2mps23_dev *iodev = s2mps23->iodev;
#endif

	if (buf == NULL) {
		pr_info("%s: empty buffer\n", __func__);
		return -1;
	}

	ret = sscanf(buf, "0x%02hhx%02hhx", &base_addr, &reg_addr);
	if (ret != 2) {
		pr_err("%s: input error\n", __func__);
		return size;
	}

#if IS_ENABLED(CONFIG_EXYNOS_ACPM)
	mutex_lock(&iodev->i2c_lock);
	ret = exynos_acpm_read_reg(acpm_mfd_node, MASTER_CHANNEL, base_addr, reg_addr, &val);
	mutex_unlock(&iodev->i2c_lock);
	if (ret)
		pr_info("%s: fail to read i2c addr/data\n", __func__);
#endif

	pr_info("%s: reg(0x%02hhx%02hhx) data(0x%02hhx)\n", __func__,
		base_addr, reg_addr, val);
	s2mps23->base_addr = base_addr;
	s2mps23->read_addr = reg_addr;
	s2mps23->read_val = val;

	return size;
}

static ssize_t s2mps23_read_show(struct device *dev,
				 struct device_attribute *attr,
				 char *buf)
{
	struct s2mps23_info *s2mps23 = dev_get_drvdata(dev);

	return sprintf(buf, "0x%02hhx%02hhx: 0x%02hhx\n",
			s2mps23->base_addr, s2mps23->read_addr, s2mps23->read_val);
}

static ssize_t s2mps23_write_store(struct device *dev,
				   struct device_attribute *attr,
				   const char *buf, size_t size)
{
	struct s2mps23_info *s2mps23 = dev_get_drvdata(dev);
	int ret;
	u8 base_addr = 0, reg_addr = 0, data = 0;
#if IS_ENABLED(CONFIG_EXYNOS_ACPM)
	struct s2mps23_dev *iodev = s2mps23->iodev;
#endif

	if (buf == NULL) {
		pr_info("%s: empty buffer\n", __func__);
		return size;
	}

	ret = sscanf(buf, "0x%02hhx%02hhx 0x%02hhx", &base_addr, &reg_addr, &data);
	if (ret != 3) {
		pr_err("%s: input error\n", __func__);
		return size;
	}

	switch (base_addr) {
	case I2C_BASE_COMMON:
	case I2C_BASE_PM:
	case I2C_BASE_RTC:
	case I2C_BASE_ADC:
	case I2C_BASE_GPIO:
	case I2C_BASE_CLOSE:
		break;
	default:
		pr_err("%s: base address error(0x%02hhx)\n", __func__, base_addr);
		return size;
	}

	pr_info("%s: reg(0x%02hhx%02hhx) data(0x%02hhx)\n", __func__,
		base_addr, reg_addr, data);

#if IS_ENABLED(CONFIG_EXYNOS_ACPM)
	mutex_lock(&iodev->i2c_lock);
	ret = exynos_acpm_write_reg(acpm_mfd_node, MASTER_CHANNEL, base_addr, reg_addr, data);
	mutex_unlock(&iodev->i2c_lock);
	if (ret)
		pr_info("%s: fail to write i2c addr/data\n", __func__);
#endif

	return size;
}

static ssize_t s2mps23_write_show(struct device *dev,
				  struct device_attribute *attr,
				  char *buf)
{
	return sprintf(buf, "echo (register addr.) (data) > s2mps23_write\n");
}

#define ATTR_REGULATOR	(2)
static struct pmic_device_attribute regulator_attr[] = {
	PMIC_ATTR(s2mps23_write, S_IRUGO | S_IWUSR, s2mps23_write_show, s2mps23_write_store),
	PMIC_ATTR(s2mps23_read, S_IRUGO | S_IWUSR, s2mps23_read_show, s2mps23_read_store),
};

static int s2mps23_create_sysfs(struct s2mps23_info *s2mps23)
{
	struct device *s2mps23_pmic = s2mps23->dev;
	struct device *dev = s2mps23->iodev->dev;
	char device_name[32] = {0, };
	int err = -ENODEV, i = 0;

	pr_info("%s()\n", __func__);
	s2mps23->base_addr = 0;
	s2mps23->read_addr = 0;
	s2mps23->read_val = 0;

	/* Dynamic allocation for device name */
	snprintf(device_name, sizeof(device_name) - 1, "%s@%s",
		 dev_driver_string(dev), dev_name(dev));

	s2mps23_pmic = pmic_device_create(s2mps23, device_name);
	s2mps23->dev = s2mps23_pmic;

	/* Create sysfs entries */
	for (i = 0; i < ATTR_REGULATOR; i++) {
		err = device_create_file(s2mps23_pmic, &regulator_attr[i].dev_attr);
		if (err)
			goto remove_pmic_device;
	}

	return 0;

remove_pmic_device:
	for (i--; i >= 0; i--)
		device_remove_file(s2mps23_pmic, &regulator_attr[i].dev_attr);
	pmic_device_destroy(s2mps23_pmic->devt);

	return -1;
}
#endif

static int s2mps23_set_powermeter(struct s2mps23_dev *iodev,
				  struct s2mps23_platform_data *pdata)
{
	if (pdata->adc_mode > 0) {
		iodev->adc_mode = pdata->adc_mode;
		s2mps23_powermeter_init(iodev);

		return 0;
	}

	return -1;
}

static int s2mps23_mapping_consel(struct s2mps23_info *s2mps23, u8 addr, size_t i)
{
	int addr_offset, reg_offset, ret;
	u8 addr_consel, con_val;

	addr_offset = i / 2;
	reg_offset = i % 2;
	addr_consel = S2MPS23_PMIC_REG_CONTROL_SEL1_EVT0 + addr_offset;
	ret = s2mps23_read_reg(s2mps23->i2c, addr_consel, &con_val);
	if (ret) {
		pr_err("%s: fail to read register\n", __func__);
		return -1;
	}

	if (reg_offset == 0) {
		if ((con_val & 0x0F) == S2MPS23_PWREN_MIF_MASK_EVT0) {
			ret = s2mps23_update_reg(s2mps23->i2c, addr, S2MPS23_ENABLE_MASK, S2MPS23_ENABLE_MASK);
			if (ret) {
				pr_err("%s: fail to update register\n", __func__);
				return -1;
			}
			pr_info("%s: changed to always-on addr: 0x%02hhx\n", __func__, addr);
		}
	} else {
		if ((con_val & 0xF0) == S2MPS23_PWREN_MIF_MASK_EVT0) {
			ret = s2mps23_update_reg(s2mps23->i2c, addr, S2MPS23_ENABLE_MASK, S2MPS23_ENABLE_MASK);
			if (ret) {
				pr_err("%s: fail to update register\n", __func__);
				return -1;
			}
			pr_info("%s: changed to always-on addr: 0x%02hhx\n", __func__, addr);
		}
	}

	return 0;
}

int s2mps23_power_off_wa(void)
{
	struct s2mps23_info *s2mps23 = s2mps23_static_info;
	size_t i;
	u8 addr, val;
	u32 iomem_val;
	int ret;
	void __iomem *mif_out;

	if (s2mps23->iodev->pmic_rev) {
		pr_info("%s : pmic_rev not EVT0\n",__func__);
		return 0;
	}

	pr_info("%s: start\n",__func__);

	/* EVT0 Manual reset w/a */
	ret = s2mps23_update_reg(s2mps23->i2c, S2MPS23_PMIC_REG_OFF_CTRL1_EVT0, 0xC0, 0xC0);
	if (ret < 0) {
			pr_info("%s: error update reg\n", __func__);
			return -1;
	}

	/* BUCK1~4M */
	for (i = 0; i <= 3; i++) {
		addr = S2MPS23_PMIC_REG_B1M_CTRL_EVT0 + i * 2;
		ret = s2mps23_read_reg(s2mps23->i2c, addr, &val);
		if (ret) {
			pr_err("%s: fail to read register\n", __func__);
			return -1;
		}

		if((val & S2MPS23_ENABLE_MASK) == CONTROL_SEL_ON) {
			if (s2mps23_mapping_consel(s2mps23, addr, i) < 0) {
				pr_err("%s : fail to mapping_consel\n", __func__);
				return -1;
			}
		}
	}

	/* BUCK5~9M */
	for (i = 4; i <= 8; i++) {
		addr = S2MPS23_PMIC_REG_B5M_CTRL_EVT0 + (i - 4) * 2;
		ret = s2mps23_read_reg(s2mps23->i2c, addr, &val);
		if (ret) {
			pr_err("%s: fail to read register\n", __func__);
			return -1;
		}

		if((val & S2MPS23_ENABLE_MASK) == CONTROL_SEL_ON) {
			if (s2mps23_mapping_consel(s2mps23, addr, i) < 0) {
				pr_err("%s : fail to mapping_consel\n", __func__);
				return -1;
			}
		}
	}

	/* LDO1~5M */
	for (i = 9; i <= 13; i++) {
		addr = S2MPS23_PMIC_REG_L1CTRL_EVT0 + i - 9;
		ret = s2mps23_read_reg(s2mps23->i2c, addr, &val);
		if (ret) {
			pr_err("%s: fail to read register\n", __func__);
			return -1;
		}

		if((val & S2MPS23_ENABLE_MASK) == CONTROL_SEL_ON || (val & S2MPS23_ENABLE_MASK) == CONTROL_SEL_NORMAL) {
			if (s2mps23_mapping_consel(s2mps23, addr, i) < 0) {
				pr_err("%s : fail to mapping_consel\n", __func__);
				return -1;
			}
		}
	}

	/* LDO6~18M */
	for (i = 14; i <= 26; i++) {
		addr = S2MPS23_PMIC_REG_L6CTRL_EVT0 + i - 14;
		ret = s2mps23_read_reg(s2mps23->i2c, addr, &val);
		if (ret) {
			pr_err("%s: fail to read register\n", __func__);
			return -1;
		}

		if((val & S2MPS23_ENABLE_MASK) == CONTROL_SEL_ON || (val & S2MPS23_ENABLE_MASK) == CONTROL_SEL_NORMAL) {
			if (s2mps23_mapping_consel(s2mps23, addr, i) < 0) {
				pr_err("%s : fail to mapping_consel\n", __func__);
				return -1;
			}
		}
	}

	/* LDO19~32M */
	for (i = 27; i <= 40; i++) {
		addr = S2MPS23_PMIC_REG_L19CTRL_EVT0 + i - 27;
		ret = s2mps23_read_reg(s2mps23->i2c, addr, &val);
		if (ret) {
			pr_err("%s: fail to read register\n", __func__);
			return -1;
		}

		if((val & S2MPS23_ENABLE_MASK) == CONTROL_SEL_ON || (val & S2MPS23_ENABLE_MASK) == CONTROL_SEL_NORMAL) {
			if (s2mps23_mapping_consel(s2mps23, addr, i) < 0) {
				pr_err("%s : fail to mapping_consel\n", __func__);
				return -1;
			}
		}
	}
	/* Set PWREN_MIF to 0 */
	mif_out = ioremap(MIF_OUT, SZ_32);
	if(mif_out == NULL) {
		pr_info("%s: fail to allocate memory\n",__func__);
		return -1;
	}
	iomem_val = readl(mif_out);
	iomem_val &= ~0x01;
	writel(iomem_val, mif_out);
	iounmap(mif_out);

	return 0;
}

int  s2mps23_power_off_seq_wa(void)
{
	struct s2mps23_info *s2mps23 = s2mps23_static_info;
	int ret;
	uint8_t val;

	if (!s2mps23->iodev->pmic_rev) {
		pr_info("%s : pmic_rev not EVT1\n",__func__);
		return 0;
	}

	/* PMIC stuck bug fix */

	ret = s2mps23_read_reg(s2mps23->i2c, S2MPS23_PMIC_REG_CTRL3, &val);
	if (ret) {
		pr_err("%s: s2mps23_read_reg fail\n", __func__);
		return -1;
	}

	if ((val & 0x10) == 0x00) {
		ret = s2mps23_update_reg(s2mps23->i2c, S2MPS23_PMIC_REG_CTRL1, 0x10, 0x1F);
		if (ret) {
			pr_err("%s: s2mps23_update_reg fail\n", __func__);
			return -1;
		}

		ret = s2mps23_update_reg(s2mps23->i2c, S2MPS23_PMIC_REG_CTRL3, 0x20, 0x20);
		if (ret) {
			pr_err("%s: s2mps23_update_reg fail\n", __func__);
			return -1;
		}

		ret = s2mps23_update_reg(s2mps23->i2c, S2MPS23_PMIC_REG_CFG_PM3, 0x04, 0x04);
		if (ret) {
			pr_err("%s: s2mps23_update_reg fail\n", __func__);
			return -1;
		}
	}

	ret = s2mps23_write_reg(s2mps23->i2c, S2MPS23_PMIC_REG_OFF_CTRL1, 0x81);
	if (ret) {
		pr_err("%s: fail to write register\n", __func__);
		return -1;
	}

	ret = s2mps23_write_reg(s2mps23->i2c, S2MPS23_PMIC_REG_OFF_CTRL2, 0x00);
	if (ret) {
		pr_err("%s: fail to write register\n", __func__);
		return -1;
	}

	ret = s2mps23_write_reg(s2mps23->i2c, S2MPS23_PMIC_REG_OFF_CTRL3, 0xA0);
	if (ret) {
		pr_err("%s: fail to write register\n", __func__);
		return -1;
	}

	ret = s2mps23_write_reg(s2mps23->i2c, S2MPS23_PMIC_REG_OFF_CTRL4, 0x50);
	if (ret) {
		pr_err("%s: fail to write register\n", __func__);
		return -1;
	}

	mdelay(50);

	return 0;
}

#if 0
static int s2mps23_smpl_warn_enable(struct s2mps23_info *s2mps23,
				    struct s2mps23_platform_data *pdata)
{
	int ret;

	ret = s2mps23_update_reg(s2mps23->i2c, S2MPS23_PMIC_REG_CTRL2,
			pdata->smpl_warn_vth, 0xe0);
	if (ret) {
		pr_err("%s: set smpl_warn_vth configuration i2c write error\n", __func__);
		goto err;
	}
	pr_info("%s: smpl_warn vth is 0x%02hhx\n", __func__,
			pdata->smpl_warn_vth);

	ret = s2mps23_update_reg(s2mps23->i2c, S2MPS23_PMIC_REG_CTRL2,
			pdata->smpl_warn_hys, 0x18);
	if (ret) {
		pr_err("%s: set smpl_warn_hys configuration i2c write error\n", __func__);
		goto err;
	}
	pr_info("%s: smpl_warn hysteresis is 0x%02hhx\n",
		__func__, pdata->smpl_warn_hys);

	return 0;
err:
	return -1;
}
#endif
static int s2mps23_set_control_sel(struct s2mps23_info *s2mps23,
				   struct s2mps23_platform_data *pdata)
{
	int ret, i, cnt = 0;
	u8 reg, val;
	char buf[1024] = {0, };

	if (s2mps23->iodev->pmic_rev) {
		for (i = 0; i < S2MPS23_CONTROL_SEL_NUM; i++) {
			reg = S2MPS23_PMIC_REG_CONTROL_SEL1 + i;
			val = pdata->control_sel[i];

			if (val <= S2MPS23_CONTROL_SEL_MAX_VAL) {
				ret = s2mps23_write_reg(s2mps23->i2c, reg, val);
				if (ret) {
					pr_err("%s: control_sel%d write error\n", __func__, i + 1, __func__);
					goto err;
				}

				cnt += snprintf(buf + cnt, sizeof(buf) - 1,
						"0x%02hhx[0x%02hhx], ", reg, val);
			} else {
				pr_err("%s: control_sel%d exceed the value\n", __func__, i + 1);
				goto err;
			}
		}
	} else {
		for (i = 0; i < S2MPS23_CONTROL_SEL_NUM_EVT0; i++) {
			reg = S2MPS23_PMIC_REG_CONTROL_SEL1_EVT0 + i;
			val = pdata->control_sel[i];

			if (val <= S2MPS23_CONTROL_SEL_MAX_VAL_EVT0) {
				ret = s2mps23_write_reg(s2mps23->i2c, reg, val);
				if (ret) {
					pr_err("%s: control_sel%d write error\n", __func__, i + 1, __func__);
					goto err;
				}

				cnt += snprintf(buf + cnt, sizeof(buf) - 1,
						"0x%02hhx[0x%02hhx], ", reg, val);
			} else {
				pr_err("%s: control_sel%d exceed the value\n", __func__, i + 1);
				goto err;
			}
		}
	}

	pr_info("%s: %s\n", __func__, buf);

	return 0;
err:
	return -1;
}

static int s2mps23_set_afm_warn(struct s2mps23_info *s2mps23,
				struct s2mps23_platform_data *pdata)
{
	u8 reg = 0, val = 0;
	int ret = 0;

	if (!s2mps23->iodev->pmic_rev) {
		pr_info("%s: Not suppport EVT0\n", __func__);
		return 0;
	}
	if (!pdata->ocp_warn1_en) {
		pr_info("%s: afm(ocp) warn1 Disable\n", __func__);
		return 0;
	}

	reg = S2MPS23_PMIC_REG_AFM_WARN1;
	val = (pdata->ocp_warn1_en << S2MPS23_AFM_WARN_EN_SHIFT) |
		(pdata->ocp_warn1_cnt << S2MPS23_AFM_WARN_CNT_SHIFT) |
		(pdata->ocp_warn1_dvs_mask << S2MPS23_AFM_WARN_DVS_MASK_SHIFT) |
		((pdata->ocp_warn1_lv & 0x1F) << S2MPS23_AFM_WARN_LV_SHIFT);

	ret = s2mps23_write_reg(s2mps23->i2c, reg, val);

	if (ret) {
		pr_err("%s: i2c write for afm(ocp) warn1 configuration caused error\n",
				__func__);
		goto err;
	}

	pr_info("%s: value for afm(ocp) warn1 register is 0x%02hhx\n", __func__, val);

	return 0;
err:
	return -1;
}

void s2mps23_evt1_1_ldo_wa(void)
{
	struct s2mps23_info *s2mps23 = s2mps23_static_info;

	/* changed EVT1.1 (HW_REV = 0x09) LDO1M, LDO8M min, step */
	if (s2mps23->iodev->pmic_rev >= 0x09) {
		regulators[S2MPS23_LDO1].min_uV = S2MPS23_LDO_MIN2_EVT1_1;
		regulators[S2MPS23_LDO1].uV_step = S2MPS23_LDO_STEP2_EVT1_1;
		regulators[S2MPS23_LDO8].min_uV = S2MPS23_LDO_MIN2_EVT1_1;
		regulators[S2MPS23_LDO8].uV_step = S2MPS23_LDO_STEP2_EVT1_1;
	}
}

static int s2mps23_pmic_probe(struct platform_device *pdev)
{
	struct s2mps23_dev *iodev = dev_get_drvdata(pdev->dev.parent);
	struct s2mps23_platform_data *pdata = iodev->pdata;
	struct regulator_config config = { };
	struct s2mps23_info *s2mps23;
	int irq_base, ret;
	u32 i;
#if IS_ENABLED(CONFIG_SEC_PM_DEBUG)
	u8 offsrc_val[2] = {0,};
#endif /* CONFIG_SEC_PM_DEBUG */

	if (iodev->dev->of_node) {
		ret = s2mps23_pmic_dt_parse_pdata(iodev, pdata);
		if (ret)
			goto err_pdata;
	}

	if (!pdata) {
		dev_err(pdev->dev.parent, "Platform data not supplied\n");
		return -ENODEV;
	}

	s2mps23 = devm_kzalloc(&pdev->dev, sizeof(struct s2mps23_info),
				GFP_KERNEL);
	if (!s2mps23)
		return -ENOMEM;

	irq_base = pdata->irq_base;
	if (!irq_base) {
		dev_err(&pdev->dev, "Failed to get irq base %d\n", irq_base);
		return -ENODEV;
	}

	s2mps23->iodev = iodev;
	s2mps23->i2c = iodev->pmic;

	mutex_init(&s2mps23->lock);

	s2mps23_static_info = s2mps23;

	platform_set_drvdata(pdev, s2mps23);

#if IS_ENABLED(CONFIG_SEC_PM_DEBUG)
	ret = s2mps23_read_reg(s2mps23->i2c, S2MPS23_PMIC_REG_PWRONSRC,
			&pmic_onsrc);
	if (ret)
		dev_err(&pdev->dev, "failed to read PWRONSRC\n");

	ret = s2mps23_bulk_read(s2mps23->i2c, S2MPS23_PMIC_REG_OFFSRC1, 2,
			pmic_offsrc);
	if (ret)
		dev_err(&pdev->dev, "failed to read OFFSRC\n");

	/* Clear OFFSRC1, OFFSRC2 register */
	ret = s2mps23_bulk_write(s2mps23->i2c, S2MPS23_PMIC_REG_OFFSRC1, 2,
			offsrc_val);
	if (ret)
		dev_err(&pdev->dev, "failed to write OFFSRC\n");
#endif /* CONFIG_SEC_PM_DEBUG */

	/* changed EVT1.1 LDO1M, LDO8 min step */
	s2mps23_evt1_1_ldo_wa();

	for (i = 0; i < pdata->num_rdata; i++) {
		int id = pdata->regulators[i].id;
		config.dev = &pdev->dev;
		config.init_data = pdata->regulators[i].initdata;
		config.driver_data = s2mps23;
		config.of_node = pdata->regulators[i].reg_node;
		if (iodev->pmic_rev) {
			s2mps23->opmode[id] = regulators[id].enable_mask;
			s2mps23->rdev[i] = devm_regulator_register(&pdev->dev,
								   &regulators[id], &config);
		} else {
			s2mps23->opmode[id] = regulators_evt0[id].enable_mask;
			s2mps23->rdev[i] = devm_regulator_register(&pdev->dev,
								   &regulators_evt0[id], &config);
		}
		if (IS_ERR(s2mps23->rdev[i])) {
			ret = PTR_ERR(s2mps23->rdev[i]);
			dev_err(&pdev->dev, "regulator init failed for %d\n", i);
			s2mps23->rdev[i] = NULL;
			goto err_s2mps23_data;
		}
	}

	s2mps23->num_regulators = pdata->num_rdata;

#if IS_ENABLED(CONFIG_SEC_PM_BIGDATA_UEVENT)
	INIT_DELAYED_WORK(&s2mps23->hqm_pmtp_work, send_hqm_pmtp_work);
	INIT_DELAYED_WORK(&s2mps23->hqm_bocp_work, send_hqm_bocp_work);
#endif /* CONFIG_SEC_PM_BIGDATA_UEVENT */

	/* TODO : don't parshed, only default setting */
#if 0
	if (pdata->smpl_warn_en) {
		ret = s2mps23_smpl_warn_enable(s2mps23, pdata);
		if (ret < 0)
			goto err_s2mps23_data;
	}
#endif
	ret = s2mps23_set_afm_warn(s2mps23, pdata);
	if (ret < 0) {
		pr_err("%s: s2mps23_set_ocp_warn fail\n", __func__);
		goto err_s2mps23_data;
	}

	ret = s2mps23_set_control_sel(s2mps23, pdata);
	if (ret < 0) {
		pr_err("%s: s2mps23_set_control_sel fail\n", __func__);
		goto err_s2mps23_data;
	}

	ret = s2mps23_set_interrupt(pdev, s2mps23, irq_base);
	if (ret < 0)
		pr_err("%s: s2mps23_set_interrupt fail\n", __func__);

	ret = s2mps23_oi_function(iodev);
	if (ret < 0)
		pr_err("%s: s2mps23_oi_function fail\n", __func__);

#if IS_ENABLED(CONFIG_SEC_PM)
	/* ap_pmic_dev should be intialized before power meter initialization */
	iodev->ap_pmic_dev = sec_device_create(NULL, "ap_pmic");

	ret = sysfs_create_group(&iodev->ap_pmic_dev->kobj, &ap_pmic_attr_group);
	if (ret)
		dev_err(&pdev->dev, "failed to create ap_pmic sysfs group\n");
#endif /* CONFIG_SEC_PM */

	ret = s2mps23_set_powermeter(iodev, pdata);
	if (ret < 0)
		pr_err("%s: Powermeter disable\n", __func__);

	/* SICD DVS voltage setting - BUCK/4M, 1S, 2S, 3S, 4S, 5S, : 0.5V */
	if (iodev->pmic_rev)
		s2mps23_write_reg(s2mps23->i2c, S2MPS23_PMIC_REG_B4M_OUT1, 0x20);
	else
		s2mps23_write_reg(s2mps23->i2c, S2MPS23_PMIC_REG_B4M_OUT1_EVT0, 0x20);

	s2mps23_write_reg(s2mps23->i2c, S2MPS23_PMIC_REG_CONTROL_SEL9, 0x10);
	s2mps23_write_reg(s2mps23->i2c, S2MPS23_PMIC_REG_CONTROL_SEL10, 0x10);

	exynos_reboot_register_pmic_ops(s2mps23_power_off_wa,
					NULL,
					s2mps23_power_off_seq_wa,
					s2mps23_read_pwron_status);

#if IS_ENABLED(CONFIG_DRV_SAMSUNG_PMIC)
	ret = s2mps23_create_sysfs(s2mps23);
	if (ret < 0) {
		pr_err("%s: s2mps23_create_sysfs fail\n", __func__);
		goto err_s2mps23_data;
	}
#endif
	return 0;

err_s2mps23_data:
	mutex_destroy(&s2mps23->lock);
err_pdata:
	return ret;
}

static int s2mps23_pmic_remove(struct platform_device *pdev)
{
	struct s2mps23_info *s2mps23 = platform_get_drvdata(pdev);
#if IS_ENABLED(CONFIG_DRV_SAMSUNG_PMIC)
	struct device *s2mps23_pmic = s2mps23->dev;
	int i = 0;

	/* Remove sysfs entries */
	for (i = 0; i < ATTR_REGULATOR; i++)
		device_remove_file(s2mps23_pmic, &regulator_attr[i].dev_attr);
	pmic_device_destroy(s2mps23_pmic->devt);
#endif
	if (s2mps23->iodev->adc_mode > 0)
		s2mps23_powermeter_deinit(s2mps23->iodev);
	mutex_destroy(&s2mps23->lock);

#if IS_ENABLED(CONFIG_SEC_PM)
	if (!IS_ERR_OR_NULL(s2mps23->iodev->ap_pmic_dev))
		sec_device_destroy(s2mps23->iodev->ap_pmic_dev->devt);
#endif /* CONFIG_SEC_PM */
#if IS_ENABLED(CONFIG_SEC_PM_BIGDATA_UEVENT)
	cancel_delayed_work(&s2mps23->hqm_pmtp_work);
	cancel_delayed_work(&s2mps23->hqm_bocp_work);
#endif /* CONFIG_SEC_PM_BIGDATA */

	return 0;
}

static void s2mps23_pmic_shutdown(struct platform_device *pdev)
{
	struct s2mps23_info *s2mps23 = platform_get_drvdata(pdev);

	/* Power-meter off */
	if (s2mps23->iodev->adc_mode > 0)
		if (s2mps23_adc_set_enable(s2mps23->iodev->adc_meter, 0) < 0)
			pr_err("%s: s2mps23_adc_set_enable fail\n", __func__);

	pr_info("%s: Power-meter off\n", __func__);

	if (s2mps23->iodev->pmic_rev) {
		pr_info("%s: Set INST_ACOK_EN bit\n", __func__);
		s2mps23_update_reg(s2mps23->i2c, S2MPS23_PMIC_REG_CFG1, (1 << 2),
				(1 << 2));
	}
}

#if IS_ENABLED(CONFIG_PM)
static int s2mps23_pmic_suspend(struct device *dev)
{
	struct platform_device *pdev = to_platform_device(dev);
	struct s2mps23_info *s2mps23 = platform_get_drvdata(pdev);
	int ret;

	/* EVT0 Manual reset w/a */
	if (!(s2mps23->iodev->pmic_rev)) {
		pr_info("%s: pmic_rev is EVT%d\n", __func__, s2mps23->iodev->pmic_rev);
		ret = s2mps23_update_reg(s2mps23->i2c, S2MPS23_PMIC_REG_OFF_CTRL1_EVT0, 0xC0, 0xC0);
		if (ret < 0) {
			pr_info("%s: error update reg\n", __func__);
			return -1;
		}
	}

	pr_info("%s: adc_mode %s\n", __func__,
		s2mps23->iodev->adc_mode == 1 ? "enable" : "disable");

	/* Power-meter off */
	if (s2mps23->iodev->adc_mode > 0)
		if (s2mps23_adc_set_enable(s2mps23->iodev->adc_meter, 0) < 0)
			pr_err("%s: s2mps23_adc_set_enable fail\n", __func__);

	return 0;
}

static int s2mps23_pmic_resume(struct device *dev)
{
	struct platform_device *pdev = to_platform_device(dev);
	struct s2mps23_info *s2mps23 = platform_get_drvdata(pdev);
	int ret;

	/* EVT0 Manual reset w/a */
	if (!(s2mps23->iodev->pmic_rev)) {
		pr_info("%s: pmic_rev is EVT%d\n", __func__, s2mps23->iodev->pmic_rev);
		ret = s2mps23_update_reg(s2mps23->i2c, S2MPS23_PMIC_REG_OFF_CTRL1_EVT0, 0x00, 0xC0);
		if (ret < 0) {
			pr_info("%s: error update reg\n", __func__);
			return -1;
		}
	}

	pr_info("%s: adc_mode %s\n", __func__,
		s2mps23->iodev->adc_mode == 1 ? "enable" : "disable");

	/* Power-meter on */
	if (s2mps23->iodev->adc_mode > 0)
		if (s2mps23_adc_set_enable(s2mps23->iodev->adc_meter, 1) < 0)
			pr_err("%s: s2mps23_adc_set_enable fail\n", __func__);

	return 0;
}
#else
#define s2mps23_pmic_suspend	NULL
#define s2mps23_pmic_resume	NULL
#endif /* CONFIG_PM */

const struct dev_pm_ops s2mps23_pmic_pm = {
	.suspend = s2mps23_pmic_suspend,
	.resume = s2mps23_pmic_resume,
};

static const struct platform_device_id s2mps23_pmic_id[] = {
	{ "s2mps23-regulator", 0},
	{ },
};
MODULE_DEVICE_TABLE(platform, s2mps23_pmic_id);

static struct platform_driver s2mps23_pmic_driver = {
	.driver = {
		.name = "s2mps23-regulator",
		.owner = THIS_MODULE,
#if IS_ENABLED(CONFIG_PM)
		.pm = &s2mps23_pmic_pm,
#endif
		.suppress_bind_attrs = true,
	},
	.probe = s2mps23_pmic_probe,
	.remove = s2mps23_pmic_remove,
	.shutdown = s2mps23_pmic_shutdown,
	.id_table = s2mps23_pmic_id,
};

static int __init s2mps23_pmic_init(void)
{
	return platform_driver_register(&s2mps23_pmic_driver);
}
subsys_initcall(s2mps23_pmic_init);

static void __exit s2mps23_pmic_exit(void)
{
	platform_driver_unregister(&s2mps23_pmic_driver);
}
module_exit(s2mps23_pmic_exit);

/* Module information */
MODULE_AUTHOR("Sangbeom Kim <sbkim73@samsung.com>");
MODULE_DESCRIPTION("SAMSUNG S2MPS23 Regulator Driver");
MODULE_LICENSE("GPL");
