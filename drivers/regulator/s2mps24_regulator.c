/*
 * s2mps24.c
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
#include <linux/mfd/samsung/s2mps24.h>
#include <linux/mfd/samsung/s2mps24-regulator.h>
#include <linux/mfd/samsung/s2mps24-regulator-evt0.h>
#include <linux/reset/exynos-reset.h>
#include <linux/io.h>
#include <linux/mutex.h>
#include <linux/interrupt.h>
#include <linux/regulator/pmic_class.h>
#if IS_ENABLED(CONFIG_SEC_PM)
#include <linux/sec_class.h>
#endif /* CONFIG_SEC_PM */
#if IS_ENABLED(CONFIG_EXYNOS_ACPM)
#include <soc/samsung/acpm_mfd.h>
#define SLAVE_CHANNEL	1
static struct device_node *acpm_mfd_node;
#endif

#define I2C_BASE_COMMON	0x00
#define I2C_BASE_PM	0x01
#define I2C_BASE_ADC	0x0A
#define I2C_BASE_GPIO	0x0B
#define I2C_BASE_CLOSE	0x0F

static struct s2mps24_info *s2mps24_static_info;

struct s2mps24_info {
	bool g3d_en;
	int wtsr_en;
	int num_regulators;
	unsigned int opmode[S2MPS24_REG_MAX];
	struct regulator_dev *rdev[S2MPS24_REG_MAX];
	struct s2mps24_dev *iodev;
	struct mutex lock;
	struct i2c_client *i2c;
#if IS_ENABLED(CONFIG_DRV_SAMSUNG_PMIC)
	u8 base_addr;
	u8 read_addr;
	u8 read_val;
	struct device *dev;
#endif
};

static unsigned int s2mps24_of_map_mode(unsigned int val) {
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
	struct s2mps24_info *s2mps24 = rdev_get_drvdata(rdev);
	unsigned int val;
	int id = rdev_get_id(rdev);

	val = mode << S2MPS24_ENABLE_SHIFT;

	s2mps24->opmode[id] = val;
	return 0;
}

static int s2m_enable(struct regulator_dev *rdev)
{
	struct s2mps24_info *s2mps24 = rdev_get_drvdata(rdev);

	return s2mps24_update_reg(s2mps24->i2c, rdev->desc->enable_reg,
				  s2mps24->opmode[rdev_get_id(rdev)],
				  rdev->desc->enable_mask);
}

static int s2m_disable_regmap(struct regulator_dev *rdev)
{
	struct s2mps24_info *s2mps24 = rdev_get_drvdata(rdev);
	unsigned int val;

	if (rdev->desc->enable_is_inverted)
		val = rdev->desc->enable_mask;
	else
		val = 0;

	return s2mps24_update_reg(s2mps24->i2c, rdev->desc->enable_reg,
				  val, rdev->desc->enable_mask);
}

static int s2m_is_enabled_regmap(struct regulator_dev *rdev)
{
	struct s2mps24_info *s2mps24 = rdev_get_drvdata(rdev);
	int ret;
	u8 val;

	ret = s2mps24_read_reg(s2mps24->i2c,
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
	struct s2mps24_info *s2mps24 = rdev_get_drvdata(rdev);
	int ramp_shift, reg_id = rdev_get_id(rdev);
	unsigned int ramp_value = 0;
	u8 ramp_addr = 0;

	ramp_value = get_ramp_delay(ramp_delay / 1000);
	if (ramp_value > 4) {
		pr_warn("%s: ramp_delay: %d not supported\n",
			rdev->desc->name, ramp_delay);
	}
	ramp_value = 0x00; 	// 6.25mv/us fixed

	switch (reg_id) {
	case S2MPS24_BUCK1:
	case S2MPS24_BUCK5:
		ramp_shift = 0;
		break;
	case S2MPS24_BUCK2:
	case S2MPS24_BUCK6:
		ramp_shift = 2;
		break;
	case S2MPS24_BUCK3:
	case S2MPS24_BUCK7:
		ramp_shift = 4;
		break;
	case S2MPS24_BUCK4:
	case S2MPS24_BUCK8:
		ramp_shift = 6;
		break;
	default:
		return -EINVAL;
	}

	switch(reg_id) {
		case S2MPS24_BUCK1:
		case S2MPS24_BUCK2:
		case S2MPS24_BUCK3:
		case S2MPS24_BUCK4:
			if (s2mps24->iodev->pmic_rev)
				ramp_addr = S2MPS24_REG_BUCK_DVS1;
			else
				ramp_addr = S2MPS24_REG_BUCK_DVS1_EVT0;
			break;
		case S2MPS24_BUCK5:
		case S2MPS24_BUCK6:
		case S2MPS24_BUCK7:
		case S2MPS24_BUCK8:
			if (s2mps24->iodev->pmic_rev)
				ramp_addr = S2MPS24_REG_BUCK_DVS2;
			else
				ramp_addr = S2MPS24_REG_BUCK_DVS2_EVT0;
			break;
		default:
			return -EINVAL;
	}

	return s2mps24_update_reg(s2mps24->i2c, ramp_addr,
				  ramp_value << ramp_shift,
				  BUCK_RAMP_MASK << ramp_shift);
}

static int s2m_get_voltage_sel_regmap(struct regulator_dev *rdev)
{
	struct s2mps24_info *s2mps24 = rdev_get_drvdata(rdev);
	int ret;
	u8 val;

	ret = s2mps24_read_reg(s2mps24->i2c, rdev->desc->vsel_reg, &val);
	if (ret)
		return ret;

	val &= rdev->desc->vsel_mask;

	return val;
}

static int s2m_set_voltage_sel_regmap(struct regulator_dev *rdev, unsigned sel)
{
	struct s2mps24_info *s2mps24 = rdev_get_drvdata(rdev);
	int reg_id = rdev_get_id(rdev), ret;
	char name[16];

	/* voltage information logging to snapshot feature */
	snprintf(name, sizeof(name), "LDO%d", (reg_id - S2MPS24_LDO1) + 1);
	ret = s2mps24_update_reg(s2mps24->i2c, rdev->desc->vsel_reg,
				 sel, rdev->desc->vsel_mask);
	if (ret < 0) {
		pr_warn("%s: failed to set voltage_sel_regmap\n", rdev->desc->name);
		return ret;
	}

	if (rdev->desc->apply_bit)
		ret = s2mps24_update_reg(s2mps24->i2c, rdev->desc->apply_reg,
					 rdev->desc->apply_bit,
					 rdev->desc->apply_bit);
	return ret;
}

static int s2m_set_voltage_sel_regmap_buck(struct regulator_dev *rdev,
					   unsigned sel)
{
	struct s2mps24_info *s2mps24 = rdev_get_drvdata(rdev);
	int ret = 0;

	ret = s2mps24_write_reg(s2mps24->i2c, rdev->desc->vsel_reg, sel);
	if (ret < 0) {
		pr_warn("%s: failed to set voltage_sel_regmap\n", rdev->desc->name);
		return -EINVAL;
	}

	if (rdev->desc->apply_bit)
		ret = s2mps24_update_reg(s2mps24->i2c, rdev->desc->apply_reg,
					 rdev->desc->apply_bit,
					 rdev->desc->apply_bit);

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

static struct regulator_ops s2mps24_ldo_ops = {
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

static struct regulator_ops s2mps24_buck_ops = {
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

static struct regulator_ops s2mps24_bb_ops = {
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

#define _BUCK(macro)		S2MPS24_BUCK##macro
#define _buck_ops(num)		s2mps24_buck_ops##num
#define _LDO(macro)		S2MPS24_LDO##macro
#define _ldo_ops(num)		s2mps24_ldo_ops##num
#define _BB(macro)              S2MPS24_BB##macro
#define _bb_ops(num)  		s2mps24_bb_ops##num

#define _REG(ctrl)		S2MPS24_REG##ctrl
#define _TIME(macro)		S2MPS24_ENABLE_TIME##macro

#define _LDO_MIN(group)		S2MPS24_LDO_MIN##group
#define _LDO_STEP(group)	S2MPS24_LDO_STEP##group
#define _BUCK_MIN(group)	S2MPS24_BUCK_MIN##group
#define _BUCK_STEP(group)	S2MPS24_BUCK_STEP##group
#define _BB_MIN(group)		S2MPS24_BB_MIN##group
#define _BB_STEP(group)		S2MPS24_BB_STEP##group

#define BUCK_DESC(_name, _id, _ops, g, v, e, t)	{		\
	.name		= _name,				\
	.id		= _id,					\
	.ops		= _ops,					\
	.type		= REGULATOR_VOLTAGE,			\
	.owner		= THIS_MODULE,				\
	.min_uV		= _BUCK_MIN(g),				\
	.uV_step	= _BUCK_STEP(g),			\
	.n_voltages	= S2MPS24_BUCK_N_VOLTAGES,		\
	.vsel_reg	= v,					\
	.vsel_mask	= S2MPS24_BUCK_VSEL_MASK,		\
	.enable_reg	= e,					\
	.enable_mask	= S2MPS24_ENABLE_MASK,			\
	.enable_time	= t,					\
	.of_map_mode	= s2mps24_of_map_mode			\
}

#define LDO_DESC(_name, _id, _ops, g, v, e, t)	{		\
	.name		= _name,				\
	.id		= _id,					\
	.ops		= _ops,					\
	.type		= REGULATOR_VOLTAGE,			\
	.owner		= THIS_MODULE,				\
	.min_uV		= _LDO_MIN(g),				\
	.uV_step	= _LDO_STEP(g),				\
	.n_voltages	= S2MPS24_LDO_N_VOLTAGES,		\
	.vsel_reg	= v,					\
	.vsel_mask	= S2MPS24_LDO_VSEL_MASK,		\
	.enable_reg	= e,					\
	.enable_mask	= S2MPS24_ENABLE_MASK,			\
	.enable_time	= t,					\
	.of_map_mode	= s2mps24_of_map_mode			\
}

#define BB_DESC(_name, _id, _ops, g, v, e, t)	{	\
	.name		= _name,				\
	.id			= _id,					\
	.ops		= _ops,					\
	.type		= REGULATOR_VOLTAGE,			\
	.owner		= THIS_MODULE,				\
	.min_uV		= _BB_MIN(g),				\
	.uV_step	= _BB_STEP(g),				\
	.n_voltages	= S2MPS24_BB_N_VOLTAGES,		\
	.vsel_reg	= v,					\
	.vsel_mask	= S2MPS24_BB_VSEL_MASK,		\
	.enable_reg	= e,					\
	.enable_mask	= S2MPS24_ENABLE_MASK,			\
	.enable_time	= t,					\
	.of_map_mode	= s2mps24_of_map_mode			\
}
#define S2MPS24_REG_MAX_EVT0 (22)
static struct regulator_desc regulators_evt0[S2MPS24_REG_MAX_EVT0] = {
	/* name, id, ops, group, vsel_reg, enable_reg, ramp_delay */
	/* LDO 1~22 */
	LDO_DESC("LDO1", _LDO(1), &_ldo_ops(), 1_EVT0, _REG(_LDO1S_CTRL_EVT0), _REG(_LDO1S_CTRL_EVT0), _TIME(_LDO)),
	LDO_DESC("LDO2", _LDO(2), &_ldo_ops(), 1_EVT0, _REG(_LDO2S_CTRL_EVT0), _REG(_LDO2S_CTRL_EVT0), _TIME(_LDO)),
	LDO_DESC("LDO3", _LDO(3), &_ldo_ops(), 1_EVT0, _REG(_LDO3S_CTRL_EVT0), _REG(_LDO3S_CTRL_EVT0), _TIME(_LDO)),
	LDO_DESC("LDO4", _LDO(4), &_ldo_ops(), 2_EVT0, _REG(_LDO4S_CTRL_EVT0), _REG(_LDO4S_CTRL_EVT0), _TIME(_LDO)),
/*	LDO_DESC("LDO5", _LDO(5), &_ldo_ops(), 2_EVT0, _REG(_LDO5S_CTRL_EVT0), _REG(_LDO5S_CTRL_EVT0), _TIME(_LDO)), */
	LDO_DESC("LDO6", _LDO(6), &_ldo_ops(), 3_EVT0, _REG(_LDO6S_CTRL_EVT0), _REG(_LDO6S_CTRL_EVT0), _TIME(_LDO)),
/*	LDO_DESC("LDO7", _LDO(7), &_ldo_ops(), 3_EVT0, _REG(_LDO7S_CTRL_EVT0), _REG(_LDO7S_CTRL_EVT0), _TIME(_LDO)),
	LDO_DESC("LDO8", _LDO(8), &_ldo_ops(), 3_EVT0, _REG(_LDO8S_CTRL_EVT0), _REG(_LDO8S_CTRL_EVT0), _TIME(_LDO)), */
	LDO_DESC("LDO9", _LDO(9), &_ldo_ops(), 3_EVT0, _REG(_LDO9S_CTRL_EVT0), _REG(_LDO9S_CTRL_EVT0), _TIME(_LDO)),
/*	LDO_DESC("LDO10", _LDO(10), &_ldo_ops(), 2_EVT0, _REG(_LDO10S_CTRL_EVT0), _REG(_LDO10S_CTRL_EVT0), _TIME(_LDO)),
	LDO_DESC("LDO11", _LDO(11), &_ldo_ops(), 2_EVT0, _REG(_LDO11S_CTRL_EVT0), _REG(_LDO11S_CTRL_EVT0), _TIME(_LDO)),
	LDO_DESC("LDO12", _LDO(12), &_ldo_ops(), 3_EVT0, _REG(_LDO12S_CTRL_EVT0), _REG(_LDO12S_CTRL_EVT0), _TIME(_LDO)),
	LDO_DESC("LDO13", _LDO(13), &_ldo_ops(), 2_EVT0, _REG(_LDO13S_CTRL_EVT0), _REG(_LDO13S_CTRL_EVT0), _TIME(_LDO)),
	LDO_DESC("LDO14", _LDO(14), &_ldo_ops(), 2_EVT0, _REG(_LDO14S_CTRL_EVT0), _REG(_LDO14S_CTRL_EVT0), _TIME(_LDO)),
	LDO_DESC("LDO15", _LDO(15), &_ldo_ops(), 2_EVT0, _REG(_LDO15S_CTRL_EVT0), _REG(_LDO15S_CTRL_EVT0), _TIME(_LDO)), */
	LDO_DESC("LDO16", _LDO(16), &_ldo_ops(), 2_EVT0, _REG(_LDO16S_CTRL_EVT0), _REG(_LDO16S_CTRL_EVT0), _TIME(_LDO)),
	LDO_DESC("LDO17", _LDO(17), &_ldo_ops(), 3_EVT0, _REG(_LDO17S_CTRL_EVT0), _REG(_LDO17S_CTRL_EVT0), _TIME(_LDO)),
	LDO_DESC("LDO18", _LDO(18), &_ldo_ops(), 3_EVT0, _REG(_LDO18S_CTRL_EVT0), _REG(_LDO18S_CTRL_EVT0), _TIME(_LDO)),
	LDO_DESC("LDO19", _LDO(19), &_ldo_ops(), 3_EVT0, _REG(_LDO19S_CTRL_EVT0), _REG(_LDO19S_CTRL_EVT0), _TIME(_LDO)),
	LDO_DESC("LDO20", _LDO(20), &_ldo_ops(), 3_EVT0, _REG(_LDO20S_CTRL_EVT0), _REG(_LDO20S_CTRL_EVT0), _TIME(_LDO)),
	LDO_DESC("LDO21", _LDO(21), &_ldo_ops(), 3_EVT0, _REG(_LDO21S_CTRL_EVT0), _REG(_LDO21S_CTRL_EVT0), _TIME(_LDO)),
	LDO_DESC("LDO22", _LDO(22), &_ldo_ops(), 1_EVT0, _REG(_LDO22S_CTRL_EVT0), _REG(_LDO22S_CTRL_EVT0), _TIME(_LDO)),

	/* BUCK 1~8, BB */
	BUCK_DESC("BUCK1", _BUCK(1), &_buck_ops(), 1_EVT0, _REG(_BUCK1S_OUT2_EVT0), _REG(_BUCK1S_CTRL_EVT0), _TIME(_BUCK)),
	BUCK_DESC("BUCK2", _BUCK(2), &_buck_ops(), 1_EVT0, _REG(_BUCK2S_OUT2_EVT0), _REG(_BUCK2S_CTRL_EVT0), _TIME(_BUCK)),
	BUCK_DESC("BUCK3", _BUCK(3), &_buck_ops(), 1_EVT0, _REG(_BUCK3S_OUT2_EVT0), _REG(_BUCK3S_CTRL_EVT0), _TIME(_BUCK)),
	BUCK_DESC("BUCK4", _BUCK(4), &_buck_ops(), 1_EVT0, _REG(_BUCK4S_OUT2_EVT0), _REG(_BUCK4S_CTRL_EVT0), _TIME(_BUCK)),
	BUCK_DESC("BUCK5", _BUCK(5), &_buck_ops(), 1_EVT0, _REG(_BUCK5S_OUT2_EVT0), _REG(_BUCK5S_CTRL_EVT0), _TIME(_BUCK)),
	BUCK_DESC("BUCK6", _BUCK(6), &_buck_ops(), 1_EVT0, _REG(_BUCK6S_OUT1_EVT0), _REG(_BUCK6S_CTRL_EVT0), _TIME(_BUCK)),
	BUCK_DESC("BUCK7", _BUCK(7), &_buck_ops(), 1_EVT0, _REG(_BUCK7S_OUT1_EVT0), _REG(_BUCK7S_CTRL_EVT0), _TIME(_BUCK)),
	BUCK_DESC("BUCK8", _BUCK(8), &_buck_ops(), 1_EVT0, _REG(_BUCK8S_OUT1_EVT0), _REG(_BUCK8S_CTRL_EVT0), _TIME(_BUCK)),
	BB_DESC("BUCKB", _BB(), &_bb_ops(), 1_EVT0, _REG(_BB_OUT_EVT0), _REG(_BB_CTRL_EVT0), _TIME(_BB))
};

static struct regulator_desc regulators[S2MPS24_REG_MAX] = {
	/* name, id, ops, group, vsel_reg, enable_reg, ramp_delay */
	/* LDO 1~23 */
	LDO_DESC("LDO1", _LDO(1), &_ldo_ops(), 1, _REG(_LDO1S_CTRL), _REG(_LDO1S_CTRL), _TIME(_LDO)),
	LDO_DESC("LDO2", _LDO(2), &_ldo_ops(), 1, _REG(_LDO2S_CTRL), _REG(_LDO2S_CTRL), _TIME(_LDO)),
	LDO_DESC("LDO3", _LDO(3), &_ldo_ops(), 1, _REG(_LDO3S_CTRL), _REG(_LDO3S_CTRL), _TIME(_LDO)),
	LDO_DESC("LDO4", _LDO(4), &_ldo_ops(), 2, _REG(_LDO4S_CTRL), _REG(_LDO4S_CTRL), _TIME(_LDO)),
/*	LDO_DESC("LDO5", _LDO(5), &_ldo_ops(), 2, _REG(_LDO5S_CTRL), _REG(_LDO5S_CTRL), _TIME(_LDO)), */
	LDO_DESC("LDO6", _LDO(6), &_ldo_ops(), 3, _REG(_LDO6S_CTRL), _REG(_LDO6S_CTRL), _TIME(_LDO)),
/*	LDO_DESC("LDO7", _LDO(7), &_ldo_ops(), 3, _REG(_LDO7S_CTRL), _REG(_LDO7S_CTRL), _TIME(_LDO)),
	LDO_DESC("LDO8", _LDO(8), &_ldo_ops(), 3, _REG(_LDO8S_CTRL), _REG(_LDO8S_CTRL), _TIME(_LDO)), */
	LDO_DESC("LDO9", _LDO(9), &_ldo_ops(), 3, _REG(_LDO9S_CTRL), _REG(_LDO9S_CTRL), _TIME(_LDO)),
/*	LDO_DESC("LDO10", _LDO(10), &_ldo_ops(), 2, _REG(_LDO10S_CTRL), _REG(_LDO10S_CTRL), _TIME(_LDO)),
	LDO_DESC("LDO11", _LDO(11), &_ldo_ops(), 2, _REG(_LDO11S_CTRL), _REG(_LDO11S_CTRL), _TIME(_LDO)),
	LDO_DESC("LDO12", _LDO(12), &_ldo_ops(), 3, _REG(_LDO12S_CTRL), _REG(_LDO12S_CTRL), _TIME(_LDO)),
	LDO_DESC("LDO13", _LDO(13), &_ldo_ops(), 2, _REG(_LDO13S_CTRL), _REG(_LDO13S_CTRL), _TIME(_LDO)), */
	LDO_DESC("LDO14", _LDO(14), &_ldo_ops(), 2, _REG(_LDO14S_CTRL), _REG(_LDO14S_CTRL), _TIME(_LDO)),
/*	LDO_DESC("LDO15", _LDO(15), &_ldo_ops(), 2, _REG(_LDO15S_CTRL), _REG(_LDO15S_CTRL), _TIME(_LDO)), */
	LDO_DESC("LDO16", _LDO(16), &_ldo_ops(), 2, _REG(_LDO16S_CTRL), _REG(_LDO16S_CTRL), _TIME(_LDO)),
	LDO_DESC("LDO17", _LDO(17), &_ldo_ops(), 3, _REG(_LDO17S_CTRL), _REG(_LDO17S_CTRL), _TIME(_LDO)),
	LDO_DESC("LDO18", _LDO(18), &_ldo_ops(), 3, _REG(_LDO18S_CTRL), _REG(_LDO18S_CTRL), _TIME(_LDO)),
	LDO_DESC("LDO19", _LDO(19), &_ldo_ops(), 3, _REG(_LDO19S_CTRL), _REG(_LDO19S_CTRL), _TIME(_LDO)),
	LDO_DESC("LDO20", _LDO(20), &_ldo_ops(), 3, _REG(_LDO20S_CTRL), _REG(_LDO20S_CTRL), _TIME(_LDO)),
	LDO_DESC("LDO21", _LDO(21), &_ldo_ops(), 3, _REG(_LDO21S_CTRL), _REG(_LDO21S_CTRL), _TIME(_LDO)),
	LDO_DESC("LDO22", _LDO(22), &_ldo_ops(), 1, _REG(_LDO22S_CTRL), _REG(_LDO22S_CTRL), _TIME(_LDO)),
	LDO_DESC("LDO23", _LDO(23), &_ldo_ops(), 3, _REG(_LDO23S_CTRL), _REG(_LDO23S_CTRL), _TIME(_LDO)),

	/* BUCK 1~8, BB */
	BUCK_DESC("BUCK1", _BUCK(1), &_buck_ops(), 1, _REG(_BUCK1S_OUT2), _REG(_BUCK1S_CTRL), _TIME(_BUCK)),
	BUCK_DESC("BUCK2", _BUCK(2), &_buck_ops(), 1, _REG(_BUCK2S_OUT2), _REG(_BUCK2S_CTRL), _TIME(_BUCK)),
	BUCK_DESC("BUCK3", _BUCK(3), &_buck_ops(), 1, _REG(_BUCK3S_OUT2), _REG(_BUCK3S_CTRL), _TIME(_BUCK)),
	BUCK_DESC("BUCK4", _BUCK(4), &_buck_ops(), 1, _REG(_BUCK4S_OUT2), _REG(_BUCK4S_CTRL), _TIME(_BUCK)),
	BUCK_DESC("BUCK5", _BUCK(5), &_buck_ops(), 1, _REG(_BUCK5S_OUT2), _REG(_BUCK5S_CTRL), _TIME(_BUCK)),
	BUCK_DESC("BUCK6", _BUCK(6), &_buck_ops(), 1, _REG(_BUCK6S_OUT1), _REG(_BUCK6S_CTRL), _TIME(_BUCK)),
	BUCK_DESC("BUCK7", _BUCK(7), &_buck_ops(), 1, _REG(_BUCK7S_OUT1), _REG(_BUCK7S_CTRL), _TIME(_BUCK)),
	BUCK_DESC("BUCK8", _BUCK(8), &_buck_ops(), 1, _REG(_BUCK8S_OUT1), _REG(_BUCK8S_CTRL), _TIME(_BUCK)),
	BB_DESC("BUCKB", _BB(), &_bb_ops(), 1, _REG(_BB_OUT), _REG(_BB_CTRL), _TIME(_BB))
};

#if IS_ENABLED(CONFIG_OF)
static int s2mps24_pmic_dt_parse_pdata(struct s2mps24_dev *iodev,
				       struct s2mps24_platform_data *pdata)
{
	struct device_node *pmic_np, *regulators_np, *reg_np;
	struct s2mps24_regulator_data *rdata;
	u32 i;
	int ret, len;
	u32 val;
	const u32 *p;

	pmic_np = iodev->dev->of_node;
	if (!pmic_np) {
		dev_err(iodev->dev, "could not find pmic sub-node\n");
		return -ENODEV;
	}
#if IS_ENABLED(CONFIG_EXYNOS_ACPM)
	acpm_mfd_node = pmic_np;
#endif
	/* adc_mode */
	pdata->adc_mode = 0;
	ret = of_property_read_u32(pmic_np, "adc_mode", &val);
	pdata->adc_mode = val;

	/* wtsr_en */
	pdata->wtsr_en = 0;
	ret = of_property_read_u32(pmic_np, "wtsr_en", &val);
	if (ret < 0)
		pr_info("%s: fail to read wtsr_en\n", __func__);
	pdata->wtsr_en = val;

	/* AFM(ocp) warn */
	ret = of_property_read_u32(pmic_np, "ocp_warn2_en", &val);
	if (ret)
		return -EINVAL;
	pdata->ocp_warn2_en = !!val;

	ret = of_property_read_u32(pmic_np, "ocp_warn2_cnt", &val);
	if (ret)
		return -EINVAL;
	pdata->ocp_warn2_cnt = val;

	ret = of_property_read_u32(pmic_np, "ocp_warn2_dvs_mask", &val);
	if (ret)
		return -EINVAL;
	pdata->ocp_warn2_dvs_mask = val;

	ret = of_property_read_u32(pmic_np, "ocp_warn2_lv", &val);
	if (ret)
		return -EINVAL;
	pdata->ocp_warn2_lv = val;

	ret = of_property_read_u32(pmic_np, "ocp_warn3_en", &val);
	if (ret)
		return -EINVAL;
	pdata->ocp_warn3_en = !!val;

	ret = of_property_read_u32(pmic_np, "ocp_warn3_cnt", &val);
	if (ret)
		return -EINVAL;
	pdata->ocp_warn3_cnt = val;

	ret = of_property_read_u32(pmic_np, "ocp_warn3_dvs_mask", &val);
	if (ret)
		return -EINVAL;
	pdata->ocp_warn3_dvs_mask = val;

	ret = of_property_read_u32(pmic_np, "ocp_warn3_lv", &val);
	if (ret)
		return -EINVAL;
	pdata->ocp_warn3_lv = val;

	ret = of_property_read_u32(pmic_np, "ocp_warn4_en", &val);
	if (ret)
		return -EINVAL;
	pdata->ocp_warn4_en = !!val;

	ret = of_property_read_u32(pmic_np, "ocp_warn4_cnt", &val);
	if (ret)
		return -EINVAL;
	pdata->ocp_warn4_cnt = val;

	ret = of_property_read_u32(pmic_np, "ocp_warn4_dvs_mask", &val);
	if (ret)
		return -EINVAL;
	pdata->ocp_warn4_dvs_mask = val;

	ret = of_property_read_u32(pmic_np, "ocp_warn4_lv", &val);
	if (ret)
		return -EINVAL;
	pdata->ocp_warn4_lv = val;

	if (iodev->pmic_rev) {
		p = of_get_property(pmic_np, "control_sel", &len);
		if (!p) {
			pr_info("%s : (ERROR) control_sel isn't parsing\n", __func__);
			return -EINVAL;
		}

		len = len / sizeof(u32);
		if (len != S2MPS24_CONTROL_SEL_NUM) {
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
		if (len != S2MPS24_CONTROL_SEL_NUM) {
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
		pdata->num_regulators = S2MPS24_REG_MAX_EVT0;

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
static int s2mps24_pmic_dt_parse_pdata(struct s2mps24_pmic_dev *iodev,
				       struct s2mps24_platform_data *pdata)
{
	return 0;
}
#endif /* CONFIG_OF */

#if IS_ENABLED(CONFIG_EXYNOS_AFM)
void get_s2mps24_i2c(struct i2c_client **i2c)
{
	if (!s2mps24_static_info)
		return;

	*i2c = s2mps24_static_info->i2c;
}
EXPORT_SYMBOL_GPL(get_s2mps24_i2c);
#endif

struct s2mps24_oi_data {
	u8 reg;
	u8 val;
};
#define OI_NUM 		(9)
#define OI_NUM_EVT0	(9)
#define DECLARE_OI(_reg, _val) { .reg = (_reg), .val = (_val) }
static const struct s2mps24_oi_data s2mps24_oi[OI_NUM] = {
	/* BUCK 1~8 & BUCK-BOOST OI enable */
	DECLARE_OI(S2MPS24_REG_BUCK_OI_EN1, 0xFF),
	DECLARE_OI(S2MPS24_REG_BUCK_OI_EN2, 0x01),
	/* BUCK 1~8 & BUCK-BOOST OI power down disable */
	DECLARE_OI(S2MPS24_REG_BUCK_OI_PD_EN1, 0x00),
	DECLARE_OI(S2MPS24_REG_BUCK_OI_PD_EN2, 0x00),
	/* OI detection time window : 300us, OI comp. output count : 50 times */
	DECLARE_OI(S2MPS24_REG_BUCK_OI_CTRL1, 0xCC),
	DECLARE_OI(S2MPS24_REG_BUCK_OI_CTRL2, 0xCC),
	DECLARE_OI(S2MPS24_REG_BUCK_OI_CTRL3, 0xCC),
	DECLARE_OI(S2MPS24_REG_BUCK_OI_CTRL4, 0xCC),
	DECLARE_OI(S2MPS24_REG_BUCK_OI_CTRL5, 0x0C),
};

static const struct s2mps24_oi_data s2mps24_oi_evt0[OI_NUM_EVT0] = {
	/* BUCK 1~8 & BUCK-BOOST OI enable */
	DECLARE_OI(S2MPS24_REG_BUCK_OI_EN1_EVT0, 0xFF),
	DECLARE_OI(S2MPS24_REG_BUCK_OI_EN2_EVT0, 0x01),
	/* BUCK 1~8 & BUCK-BOOST OI power down disable */
	DECLARE_OI(S2MPS24_REG_BUCK_OI_PD_EN1_EVT0, 0x00),
	DECLARE_OI(S2MPS24_REG_BUCK_OI_PD_EN2_EVT0, 0x00),
	/* OI detection time window : 300us, OI comp. output count : 50 times */
	DECLARE_OI(S2MPS24_REG_BUCK_OI_CTRL1_EVT0, 0xCC),
	DECLARE_OI(S2MPS24_REG_BUCK_OI_CTRL2_EVT0, 0xCC),
	DECLARE_OI(S2MPS24_REG_BUCK_OI_CTRL3_EVT0, 0xCC),
	DECLARE_OI(S2MPS24_REG_BUCK_OI_CTRL4_EVT0, 0xCC),
	DECLARE_OI(S2MPS24_REG_BUCK_OI_CTRL5_EVT0, 0x0C),
};

static int s2mps24_oi_function(struct s2mps24_dev *iodev)
{
	struct i2c_client *i2c = iodev->pmic;
	u32 i;
	u8 val, s, e;
	int ret, cnt = 0;
	char buf[1024] = {0, };

	if (iodev->pmic_rev) {
		for (i = 0; i < OI_NUM; i++) {
			ret = s2mps24_write_reg(i2c, s2mps24_oi[i].reg, s2mps24_oi[i].val);
			if (ret) {
				pr_err("%s: failed to write register\n", __func__);
				goto err;
			}
		}
		s = S2MPS24_REG_BUCK_OI_EN1;
		e = S2MPS24_REG_BUCK_OI_CTRL5;
	} else {
		for (i = 0; i < OI_NUM_EVT0; i++) {
			ret = s2mps24_write_reg(i2c, s2mps24_oi_evt0[i].reg, s2mps24_oi_evt0[i].val);
			if (ret) {
				pr_err("%s: failed to write register\n", __func__);
				goto err;
			}
		}
		s = S2MPS24_REG_BUCK_OI_EN1_EVT0;
		e = S2MPS24_REG_BUCK_OI_CTRL5_EVT0;
	}

	for (i = s; i <= e; i++) {
		ret = s2mps24_read_reg(i2c, i, &val);
		if (ret)
			goto err;

		cnt += snprintf(buf + cnt, sizeof(buf) - 1, "0x%x[0x%02hhx], ", i, val);
	}
	pr_info("%s: %s\n", __func__, buf);

	return 0;
err:
	return -1;
}

static void s2mps24_wtsr_enable(struct s2mps24_info *s2mps24,
				struct s2mps24_platform_data *pdata)
{
	int ret;

	pr_info("%s: WTSR (%s)\n", __func__,
		pdata->wtsr_en ? "enable" : "disable");

	if (s2mps24->iodev->pmic_rev) {
		ret = s2mps24_update_reg(s2mps24->i2c, S2MPS24_REG_CFG1,
				 	S2MPS24_WTSREN_MASK, S2MPS24_WTSREN_MASK);
	} else {
		ret = s2mps24_update_reg(s2mps24->i2c, S2MPS24_REG_CFG1_EVT0,
				 	S2MPS24_WTSREN_MASK, S2MPS24_WTSREN_MASK);
	}

	if (ret < 0)
		pr_info("%s: fail to update WTSR reg(%d)\n", __func__, ret);

	s2mps24->wtsr_en = pdata->wtsr_en;
}

static void s2mps24_wtsr_disable(struct s2mps24_info *s2mps24)
{
	int ret;

	pr_info("%s: disable WTSR\n", __func__);

	if (s2mps24->iodev->pmic_rev) {
		ret = s2mps24_update_reg(s2mps24->i2c, S2MPS24_REG_CFG1,
				 	 0x00, S2MPS24_WTSREN_MASK);
	} else {
		ret = s2mps24_update_reg(s2mps24->i2c, S2MPS24_REG_CFG1_EVT0,
				 	 0x00, S2MPS24_WTSREN_MASK);
	}

	if (ret < 0)
		pr_info("%s: fail to update WTSR reg(%d)\n", __func__, ret);
}

#if IS_ENABLED(CONFIG_DRV_SAMSUNG_PMIC)
static ssize_t s2mps24_read_store(struct device *dev,
				  struct device_attribute *attr,
				  const char *buf, size_t size)
{
	struct s2mps24_info *s2mps24 = dev_get_drvdata(dev);
	int ret;
	u8 base_addr = 0, reg_addr = 0, val = 0;
#if IS_ENABLED(CONFIG_EXYNOS_ACPM)
	struct s2mps24_dev *iodev = s2mps24->iodev;
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
	ret = exynos_acpm_read_reg(acpm_mfd_node, SLAVE_CHANNEL, base_addr, reg_addr, &val);
	mutex_unlock(&iodev->i2c_lock);
	if (ret)
		pr_info("%s: fail to read i2c addr/data\n", __func__);
#endif

	pr_info("%s: reg(0x%02hhx%02hhx) data(0x%02hhx)\n", __func__,
		base_addr, reg_addr, val);
	s2mps24->base_addr = base_addr;
	s2mps24->read_addr = reg_addr;
	s2mps24->read_val = val;

	return size;
}

static ssize_t s2mps24_read_show(struct device *dev,
				 struct device_attribute *attr,
				 char *buf)
{
	struct s2mps24_info *s2mps24 = dev_get_drvdata(dev);

	return sprintf(buf, "0x%02hhx%02hhx: 0x%02hhx\n",
			s2mps24->base_addr, s2mps24->read_addr, s2mps24->read_val);
}

static ssize_t s2mps24_write_store(struct device *dev,
				   struct device_attribute *attr,
				   const char *buf, size_t size)
{
	struct s2mps24_info *s2mps24 = dev_get_drvdata(dev);
	int ret;
	u8 base_addr = 0, reg_addr = 0, data = 0;
#if IS_ENABLED(CONFIG_EXYNOS_ACPM)
	struct s2mps24_dev *iodev = s2mps24->iodev;
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
	ret = exynos_acpm_write_reg(acpm_mfd_node, SLAVE_CHANNEL, base_addr, reg_addr, data);
	mutex_unlock(&iodev->i2c_lock);
	if (ret)
		pr_info("%s: fail to write i2c addr/data\n", __func__);
#endif

	return size;
}

static ssize_t s2mps24_write_show(struct device *dev,
				  struct device_attribute *attr,
				  char *buf)
{
	return sprintf(buf, "echo (register addr.) (data) > s2mps24_write\n");
}

#define ATTR_REGULATOR	(2)
static struct pmic_device_attribute regulator_attr[] = {
	PMIC_ATTR(s2mps24_write, S_IRUGO | S_IWUSR, s2mps24_write_show, s2mps24_write_store),
	PMIC_ATTR(s2mps24_read, S_IRUGO | S_IWUSR, s2mps24_read_show, s2mps24_read_store),
};

static int s2mps24_create_sysfs(struct s2mps24_info *s2mps24)
{
	struct device *s2mps24_pmic = s2mps24->dev;
	struct device *dev = s2mps24->iodev->dev;
	char device_name[32] = {0,};
	int err = -ENODEV, i = 0;

	pr_info("%s()\n", __func__);
	s2mps24->base_addr = 0;
	s2mps24->read_addr = 0;
	s2mps24->read_val = 0;

	/* Dynamic allocation for device name */
	snprintf(device_name, sizeof(device_name) - 1, "%s@%s",
		 dev_driver_string(dev), dev_name(dev));

	s2mps24_pmic = pmic_device_create(s2mps24, device_name);
	s2mps24->dev = s2mps24_pmic;

	/* Create sysfs entries */
	for (i = 0; i < ATTR_REGULATOR; i++) {
		err = device_create_file(s2mps24_pmic, &regulator_attr[i].dev_attr);
		if (err)
			goto remove_pmic_device;
	}

	return 0;

remove_pmic_device:
	for (i--; i >= 0; i--)
		device_remove_file(s2mps24_pmic, &regulator_attr[i].dev_attr);
	pmic_device_destroy(s2mps24_pmic->devt);

	return -1;
}
#endif

static int s2mps24_set_powermeter(struct s2mps24_dev *iodev,
				  struct s2mps24_platform_data *pdata)
{
	if (pdata->adc_mode > 0) {
		iodev->adc_mode = pdata->adc_mode;
		s2mps24_powermeter_init(iodev);

		return 0;
	}

	return -1;
}

static int s2mps24_mapping_consel(struct s2mps24_info *s2mps24, u8 addr, size_t i)
{
	int addr_offset, reg_offset, ret;
	u8 addr_consel, con_val;

	addr_offset = i / 2;
	reg_offset = i % 2;
	addr_consel = S2MPS24_REG_CONTROL_SEL1_EVT0 + addr_offset;
	ret = s2mps24_read_reg(s2mps24->i2c, addr_consel, &con_val);
	if (ret) {
		pr_err("%s: fail to read register\n", __func__);
		return -1;
	}

	if (reg_offset == 0) {
		if ((con_val & 0x0F) == S2MPS24_PWREN_MIF_MASK_EVT0) {
			pr_info("%s: changed to always-on addr: 0x%02hhx\n", __func__, addr);

			ret = s2mps24_update_reg(s2mps24->i2c, addr, S2MPS24_ENABLE_MASK, S2MPS24_ENABLE_MASK);
			if (ret) {
				pr_err("%s: fail to update register\n", __func__);
				return -1;
			}
		}
	} else {
		if ((con_val & 0xF0) == S2MPS24_PWREN_MIF_MASK_EVT0) {
			pr_info("%s: changed to always-on addr: 0x%02hhx\n", __func__, addr);

			ret = s2mps24_update_reg(s2mps24->i2c, addr, S2MPS24_ENABLE_MASK, S2MPS24_ENABLE_MASK);
			if (ret) {
				pr_err("%s: fail to update register\n", __func__);
				return -1;
			}
		}
	}

	return 0;
}

int s2mps24_power_off_wa(void)
{
	struct s2mps24_info *s2mps24 = s2mps24_static_info;
	size_t i;
	u8 addr, val;
	int ret;

	if (s2mps24->iodev->pmic_rev) {
		pr_info("%s : pmic_rev not EVT0\n",__func__);
		return 0;
	}

	pr_info("%s: start\n",__func__);
	/* BUCK1~6S */
	for (i = 0; i <= 5; i++) {
		addr = S2MPS24_REG_BUCK1S_CTRL_EVT0 + i * 3;
		ret = s2mps24_read_reg(s2mps24->i2c, addr, &val);
		if (ret) {
			pr_err("%s: fail to read register\n", __func__);
			return -1;
		}

		if((val & S2MPS24_ENABLE_MASK) == CONTROL_SEL_ON) {
			if (s2mps24_mapping_consel(s2mps24, addr, i) < 0) {
				pr_err("%s : fail to mapping_consel\n", __func__);
				return -1;
			}
		}
	}

	/* BUCK7~8S,BBS */
	for (i = 6; i <= 8; i++) {
		addr = S2MPS24_REG_BUCK7S_CTRL_EVT0 + (i - 6) * 2;
		ret = s2mps24_read_reg(s2mps24->i2c, addr, &val);
		if (ret) {
			pr_err("%s: fail to read register\n", __func__);
			return -1;
		}

		if((val & S2MPS24_ENABLE_MASK) == CONTROL_SEL_ON) {
			if (s2mps24_mapping_consel(s2mps24, addr, i) < 0) {
				pr_err("%s : fail to mapping_consel\n", __func__);
				return -1;
			}
		}
	}

	/* LDO1~22S */
	for (i = 9; i <= 30; i++) {
		addr = S2MPS24_REG_LDO1S_CTRL_EVT0 + i - 9;
		ret = s2mps24_read_reg(s2mps24->i2c, addr, &val);
		if (ret) {
			pr_err("%s: fail to read register\n", __func__);
			return -1;
		}

		if((val & S2MPS24_ENABLE_MASK) == CONTROL_SEL_ON || (val & S2MPS24_ENABLE_MASK) == CONTROL_SEL_NORMAL) {
			if (s2mps24_mapping_consel(s2mps24, addr, i) < 0) {
				pr_err("%s : fail to mapping_consel\n", __func__);
				return -1;
			}
		}
	}

	return 0;
}

static int s2mps24_set_control_sel(struct s2mps24_info *s2mps24,
				struct s2mps24_platform_data *pdata)
{
	int ret, i, cnt = 0;
	u8 reg, val;
	char buf[1024] = {0, };

	if (s2mps24->iodev->pmic_rev) {
		for (i = 0; i < S2MPS24_CONTROL_SEL_NUM; i++) {
			reg = S2MPS24_REG_CONTROL_SEL1 + i;
			val = pdata->control_sel[i];

			if (val <= S2MPS24_CONTROL_SEL_MAX_VAL) {
				ret = s2mps24_write_reg(s2mps24->i2c, reg, val);
				if (ret) {
					pr_err("%s: control_sel%d write error\n", __func__, i + 1);
					goto err;
				}

				cnt += snprintf(buf + cnt, sizeof(buf) - 1,
						"0x%02hhx[0x%02hhx], ", reg, val);
			} else {
				pr_err("%s : control_sel%d exceed the value\n", __func__, i + 1);
				goto err;
			}
		}
	} else {
		for (i = 0; i < S2MPS24_CONTROL_SEL_NUM_EVT0; i++) {
			reg = S2MPS24_REG_CONTROL_SEL1_EVT0 + i;
			val = pdata->control_sel[i];

			if (val <= S2MPS24_CONTROL_SEL_MAX_VAL_EVT0) {
				ret = s2mps24_write_reg(s2mps24->i2c, reg, val);
				if (ret) {
					pr_err("%s: control_sel%d write error\n", __func__, i + 1);
					goto err;
				}

				cnt += snprintf(buf + cnt, sizeof(buf) - 1,
						"0x%02hhx[0x%02hhx], ", reg, val);
			} else {
				pr_err("%s : control_sel%d exceed the value\n", __func__, i + 1);
				goto err;
			}
		}
	}

	pr_info("%s: %s\n", __func__, buf);

	return 0;
err:
	return -1;
}

static int s2mps24_set_afm_code(struct s2mps24_info *s2mps24)
{
	u8 pmic_rev = 0, afm_warn2_lvl = 0, afm_warn3_lvl = 0;
	int ret = 0;

	ret = s2mps24_read_reg(s2mps24->iodev->i2c, S2MPS24_PMIC_REG_PMICID, &pmic_rev);
	if (ret < 0) {
		pr_err("%s: s2mps24_read_reg failed\n", __func__);
		return -1;
	}
	pr_info("%s: pmic_rev: 0x%02hhx\n", __func__, pmic_rev);

	if (pmic_rev == 0x18) {
		afm_warn2_lvl = 0x00;
		afm_warn3_lvl = 0x00;
	} else if (pmic_rev == 0x19) {
		afm_warn2_lvl = 0x00;
		afm_warn3_lvl = 0x0A;
	} else if (pmic_rev == 0x29) {
		afm_warn3_lvl = 0x05;
		goto warn3;
	} else
		return 0;

	ret = s2mps24_update_reg(s2mps24->i2c, S2MPS24_REG_AFM_WARN2, afm_warn2_lvl, 0x1F);
	if (ret < 0) {
		pr_err("%s: s2mps24_update_reg failed\n", __func__);
		return -1;
	}
warn3:
	ret = s2mps24_update_reg(s2mps24->i2c, S2MPS24_REG_AFM_WARN3, afm_warn3_lvl, 0x1F);
	if (ret < 0) {
		pr_err("%s: s2mps24_update_reg failed\n", __func__);
		return -1;
	}

	return 0;
}

static int s2mps24_set_afm_warn(struct s2mps24_info *s2mps24,
				struct s2mps24_platform_data *pdata)
{
	u8 reg = 0, val = 0;
	int ret = 0;

	if (!s2mps24->iodev->pmic_rev) {
		pr_info("%s: Not support EVT0\n", __func__);
		return 0;
	}
	if (!pdata->ocp_warn2_en) {
		pr_info("%s: afm(ocp) warn2 Disable\n", __func__);
		goto ocp_warn3;
	}

	reg = S2MPS24_REG_AFM_WARN2;

	/* EVT1(HW_REV = 0x08) AFM WARN error W/A */
	if (s2mps24->iodev->pmic_rev == 0x08) {
		val = (0 << S2MPS24_AFM_WARN_EN_SHIFT) |
			(pdata->ocp_warn2_cnt << S2MPS24_AFM_WARN_CNT_SHIFT) |
			(pdata->ocp_warn2_dvs_mask << S2MPS24_AFM_WARN_DVS_MASK_SHIFT) |
			((pdata->ocp_warn2_lv & 0x1F) << S2MPS24_AFM_WARN_LV_SHIFT);

		ret = s2mps24_write_reg(s2mps24->i2c, reg, val);

		if (ret) {
			pr_err("%s: i2c write for afm(ocp) warn2 configuration caused error\n",
					__func__);
			goto err;
		}
	}

	val = (pdata->ocp_warn2_en << S2MPS24_AFM_WARN_EN_SHIFT) |
		(pdata->ocp_warn2_cnt << S2MPS24_AFM_WARN_CNT_SHIFT) |
		(pdata->ocp_warn2_dvs_mask << S2MPS24_AFM_WARN_DVS_MASK_SHIFT) |
		((pdata->ocp_warn2_lv & 0x1F) << S2MPS24_AFM_WARN_LV_SHIFT);

	ret = s2mps24_write_reg(s2mps24->i2c, reg, val);

	if (ret) {
		pr_err("%s: i2c write for afm(ocp) warn2 configuration caused error\n",
				__func__);
		goto err;
	}

	pr_info("%s: value for afm(ocp) warn2 register is 0x%02hhx\n", __func__, val);

ocp_warn3:
	if (!pdata->ocp_warn3_en) {
		pr_info("%s: afm(ocp) warn3 Disable\n", __func__);
		goto ocp_warn4;
	}

	reg = S2MPS24_REG_AFM_WARN3;

	/* EVT1(HW_REV = 0x08) AFM WARN error W/A */
	if (s2mps24->iodev->pmic_rev == 0x08) {
		val = (0 << S2MPS24_AFM_WARN_EN_SHIFT) |
			(pdata->ocp_warn3_cnt << S2MPS24_AFM_WARN_CNT_SHIFT) |
			(pdata->ocp_warn3_dvs_mask << S2MPS24_AFM_WARN_DVS_MASK_SHIFT) |
			((pdata->ocp_warn3_lv & 0x1F) << S2MPS24_AFM_WARN_LV_SHIFT);

		ret = s2mps24_write_reg(s2mps24->i2c, reg, val);

		if (ret) {
			pr_err("%s: i2c write for afm(ocp) warn3 configuration caused error\n",
					__func__);
			goto err;
		}
	}

	val = (pdata->ocp_warn3_en << S2MPS24_AFM_WARN_EN_SHIFT) |
		(pdata->ocp_warn3_cnt << S2MPS24_AFM_WARN_CNT_SHIFT) |
		(pdata->ocp_warn3_dvs_mask << S2MPS24_AFM_WARN_DVS_MASK_SHIFT) |
		((pdata->ocp_warn3_lv & 0x1F) << S2MPS24_AFM_WARN_LV_SHIFT);

	ret = s2mps24_write_reg(s2mps24->i2c, reg, val);

	if (ret) {
		pr_err("%s: i2c write for afm(ocp) warn3 configuration caused error\n",
				__func__);
		goto err;
	}

	pr_info("%s: value for afm(ocp) warn3 register is 0x%02hhx\n", __func__, val);

ocp_warn4:
	if (!pdata->ocp_warn4_en) {
		pr_info("%s: afm(ocp) warn4 Disable\n", __func__);
		return 0;
	}

	if (s2mps24->iodev->pmic_rev < 0x09) {
		pr_info("%s: afm(ocp) HW_REV_ID: 0x%02hhx\n", __func__, s2mps24->iodev->pmic_rev);
		return 0;
	}

	reg = S2MPS24_REG_AFM_WARN4;
	val = (pdata->ocp_warn4_en << S2MPS24_AFM_WARN_EN_SHIFT) |
		(pdata->ocp_warn4_cnt << S2MPS24_AFM_WARN_CNT_SHIFT) |
		(pdata->ocp_warn4_dvs_mask << S2MPS24_AFM_WARN_DVS_MASK_SHIFT) |
		((pdata->ocp_warn4_lv & 0x1F) << S2MPS24_AFM_WARN_LV_SHIFT);

	ret = s2mps24_write_reg(s2mps24->i2c, reg, val);

	if (ret) {
		pr_err("%s: i2c write for afm(ocp) warn4 configuration caused error\n",
				__func__);
		goto err;
	}

	pr_info("%s: value for afm(ocp) warn4 register is 0x%02hhx\n", __func__, val);

	return 0;
err:
	return -1;
}

static int s2mps24_pmic_probe(struct platform_device *pdev)
{
	struct s2mps24_dev *iodev = dev_get_drvdata(pdev->dev.parent);
	struct s2mps24_platform_data *pdata = iodev->pdata;
	struct regulator_config config = { };
	struct s2mps24_info *s2mps24;
	int ret;
	u32 i;

	if (iodev->dev->of_node) {
		ret = s2mps24_pmic_dt_parse_pdata(iodev, pdata);
		if (ret)
			goto err_pdata;
	}

	if (!pdata) {
		dev_err(pdev->dev.parent, "Platform data not supplied\n");
		return -ENODEV;
	}

	s2mps24 = devm_kzalloc(&pdev->dev, sizeof(struct s2mps24_info),
			       GFP_KERNEL);
	if (!s2mps24)
		return -ENOMEM;

	s2mps24->iodev = iodev;
	s2mps24->i2c = iodev->pmic;

	mutex_init(&s2mps24->lock);

	s2mps24->g3d_en = pdata->g3d_en;
	s2mps24_static_info = s2mps24;

	platform_set_drvdata(pdev, s2mps24);

	for (i = 0; i < pdata->num_rdata; i++) {
		int id = pdata->regulators[i].id;
		config.dev = &pdev->dev;
		config.init_data = pdata->regulators[i].initdata;
		config.driver_data = s2mps24;
		config.of_node = pdata->regulators[i].reg_node;
		if (iodev->pmic_rev) {
			s2mps24->opmode[id] = regulators[id].enable_mask;
			s2mps24->rdev[i] = devm_regulator_register(&pdev->dev,
							   &regulators[id], &config);
		} else {
			s2mps24->opmode[id] = regulators_evt0[id].enable_mask;
			s2mps24->rdev[i] = devm_regulator_register(&pdev->dev,
							   &regulators_evt0[id], &config);
		}
		if (IS_ERR(s2mps24->rdev[i])) {
			ret = PTR_ERR(s2mps24->rdev[i]);
			dev_err(&pdev->dev, "regulator init failed for %d\n", i);
			s2mps24->rdev[i] = NULL;
			goto err_s2mps24_data;
		}
	}

	s2mps24->num_regulators = pdata->num_rdata;

	ret = s2mps24_set_afm_warn(s2mps24, pdata);
	if (ret < 0) {
		pr_err("%s: s2mps24_set_ocp_warn fail\n", __func__);
		goto err_s2mps24_data;
	}

	ret = s2mps24_set_afm_code(s2mps24);
	if (ret < 0) {
		pr_err("%s: s2mps24_set_afm_code failed\n", __func__);
		return -1;
	}

	if (pdata->wtsr_en)
		s2mps24_wtsr_enable(s2mps24, pdata);

	ret = s2mps24_set_control_sel(s2mps24, pdata);
	if (ret < 0) {
		pr_err("%s: s2mps24_set_control_sel fail\n", __func__);
		goto err_s2mps24_data;
	}

	ret = s2mps24_oi_function(iodev);
	if (ret < 0)
		pr_err("%s: s2mps24_oi_function fail\n", __func__);

#if IS_ENABLED(CONFIG_SEC_PM)
	/* ap_sub_pmic_dev should be intialized before power meter initialization */
	iodev->ap_sub_pmic_dev = sec_device_create(NULL, "ap_sub_pmic");
#endif /* CONFIG_SEC_PM */

	ret = s2mps24_set_powermeter(iodev, pdata);
	if (ret < 0)
		pr_err("%s: Powermeter disable\n", __func__);

	/* SICD DVS voltage setting - BUCK/4M, 1S, 2S, 3S, 4S, 5S, : 0.5V */
	if (iodev->pmic_rev) {
		s2mps24_write_reg(s2mps24->i2c, S2MPS24_REG_BUCK1S_OUT1, 0x20);
		s2mps24_write_reg(s2mps24->i2c, S2MPS24_REG_BUCK2S_OUT1, 0x20);
		s2mps24_write_reg(s2mps24->i2c, S2MPS24_REG_BUCK3S_OUT1, 0x20);
		s2mps24_write_reg(s2mps24->i2c, S2MPS24_REG_BUCK4S_OUT1, 0x20);
		s2mps24_write_reg(s2mps24->i2c, S2MPS24_REG_BUCK5S_OUT1, 0x20);
	} else {
		s2mps24_write_reg(s2mps24->i2c, S2MPS24_REG_BUCK1S_OUT1_EVT0, 0x20);
		s2mps24_write_reg(s2mps24->i2c, S2MPS24_REG_BUCK2S_OUT1_EVT0, 0x20);
		s2mps24_write_reg(s2mps24->i2c, S2MPS24_REG_BUCK3S_OUT1_EVT0, 0x20);
		s2mps24_write_reg(s2mps24->i2c, S2MPS24_REG_BUCK4S_OUT1_EVT0, 0x20);
		s2mps24_write_reg(s2mps24->i2c, S2MPS24_REG_BUCK5S_OUT1_EVT0, 0x20);
	}

	s2mps24_write_reg(s2mps24->i2c, S2MPS24_REG_CONTROL_SEL12, 0x91);

	exynos_reboot_register_pmic_ops(NULL, s2mps24_power_off_wa, NULL, NULL);

#if IS_ENABLED(CONFIG_DRV_SAMSUNG_PMIC)
	ret = s2mps24_create_sysfs(s2mps24);
	if (ret < 0) {
		pr_err("%s: s2mps24_create_sysfs fail\n", __func__);
		goto err_s2mps24_data;
	}
#endif
	return 0;

err_s2mps24_data:
	mutex_destroy(&s2mps24->lock);
err_pdata:
	return ret;
}

static int s2mps24_pmic_remove(struct platform_device *pdev)
{
	struct s2mps24_info *s2mps24 = platform_get_drvdata(pdev);
#if IS_ENABLED(CONFIG_DRV_SAMSUNG_PMIC)
	struct device *s2mps24_pmic = s2mps24->dev;
	int i = 0;

	/* Remove sysfs entries */
	for (i = 0; i < ATTR_REGULATOR; i++)
		device_remove_file(s2mps24_pmic, &regulator_attr[i].dev_attr);
	pmic_device_destroy(s2mps24_pmic->devt);
#endif
	if (s2mps24->iodev->adc_mode > 0)
		s2mps24_powermeter_deinit(s2mps24->iodev);
	mutex_destroy(&s2mps24->lock);

#if IS_ENABLED(CONFIG_SEC_PM)
	if (!IS_ERR_OR_NULL(s2mps24->iodev->ap_sub_pmic_dev))
		sec_device_destroy(s2mps24->iodev->ap_sub_pmic_dev->devt);
#endif /* CONFIG_SEC_PM */

	return 0;
}

static void s2mps24_pmic_shutdown(struct platform_device *pdev)
{
	struct s2mps24_info *s2mps24 = platform_get_drvdata(pdev);

	/* Power-meter off */
	if (s2mps24->iodev->adc_mode > 0)
		if (s2mps24_adc_set_enable(s2mps24->iodev->adc_meter, 0) < 0)
			pr_err("%s: s2mps24_adc_set_enable fail\n", __func__);

	pr_info("%s: Power-meter off\n", __func__);

	/* disable WTSR */
	if (s2mps24->wtsr_en)
		s2mps24_wtsr_disable(s2mps24);
}

#if IS_ENABLED(CONFIG_PM)
static int s2mps24_pmic_suspend(struct device *dev)
{
	struct platform_device *pdev = to_platform_device(dev);
	struct s2mps24_info *s2mps24 = platform_get_drvdata(pdev);

	pr_info("%s: adc_mode %s\n", __func__,
		s2mps24->iodev->adc_mode == 1 ? "enable" : "disable");

	/* Power-meter off */
	if (s2mps24->iodev->adc_mode > 0)
		if (s2mps24_adc_set_enable(s2mps24->iodev->adc_meter, 0) < 0)
			pr_err("%s: s2mps24_adc_set_enable fail\n", __func__);

	return 0;
}

static int s2mps24_pmic_resume(struct device *dev)
{
	struct platform_device *pdev = to_platform_device(dev);
	struct s2mps24_info *s2mps24 = platform_get_drvdata(pdev);

	pr_info("%s: adc_mode %s\n", __func__,
		s2mps24->iodev->adc_mode == 1 ? "enable" : "disable");

	/* Power-meter on */
	if (s2mps24->iodev->adc_mode > 0)
		if (s2mps24_adc_set_enable(s2mps24->iodev->adc_meter, 1) < 0)
			pr_err("%s: s2mps24_adc_set_enable fail\n", __func__);

	return 0;
}
#else
#define s2mps24_pmic_suspend	NULL
#define s2mps24_pmic_resume	NULL
#endif /* CONFIG_PM */

const struct dev_pm_ops s2mps24_pmic_pm = {
	.suspend = s2mps24_pmic_suspend,
	.resume = s2mps24_pmic_resume,
};

static const struct platform_device_id s2mps24_pmic_id[] = {
	{ "s2mps24-regulator", 0},
	{ },
};
MODULE_DEVICE_TABLE(platform, s2mps24_pmic_id);

static struct platform_driver s2mps24_pmic_driver = {
	.driver = {
		.name = "s2mps24-regulator",
		.owner = THIS_MODULE,
#if IS_ENABLED(CONFIG_PM)
		.pm = &s2mps24_pmic_pm,
#endif
		.suppress_bind_attrs = true,
	},
	.probe = s2mps24_pmic_probe,
	.remove = s2mps24_pmic_remove,
	.shutdown = s2mps24_pmic_shutdown,
	.id_table = s2mps24_pmic_id,
};

static int __init s2mps24_pmic_init(void)
{
	return platform_driver_register(&s2mps24_pmic_driver);
}
subsys_initcall(s2mps24_pmic_init);

static void __exit s2mps24_pmic_exit(void)
{
	platform_driver_unregister(&s2mps24_pmic_driver);
}
module_exit(s2mps24_pmic_exit);

/* Module information */
MODULE_AUTHOR("Sangbeom Kim <sbkim73@samsung.com>");
MODULE_DESCRIPTION("SAMSUNG S2MPS24 Regulator Driver");
MODULE_LICENSE("GPL");
