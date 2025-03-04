/*
 * s2dos05_powermeter.c
 *
 * Copyright (c) 2015 Samsung Electronics Co., Ltd
 *              http://www.samsung.com
 *
 *  This program is free software; you can redistribute  it and/or modify it
 *  under  the terms of  the GNU General  Public License as published by the
 *  Free Software Foundation;  either version 2 of the  License, or (at your
 *  option) any later version.
 *
 */

#include <linux/err.h>
#include <linux/slab.h>
#include <linux/regulator/s2dos05.h>
#include <linux/platform_device.h>
#include <linux/regulator/pmic_class.h>

#define CURRENT_METER		1
#define POWER_METER		2
#define RAWCURRENT_METER	3
#define SYNC_MODE		1
#define ASYNC_MODE		2

#define MICRO			100

struct adc_info {
	struct i2c_client *i2c;
	u8 adc_mode;
	u8 adc_sync_mode;
	unsigned long *adc_val;
	u8 adc_ctrl1;
#if IS_ENABLED(CONFIG_SEC_PM)
	bool is_sm3080;
#endif /* CONFIG_SEC_PM */
};

enum s2dos05_adc_ch {
	ADC_CH0 = 0,
	ADC_CH1,
	ADC_CH2,
	ADC_CH3,
	ADC_CH4,
	ADC_CH5,
	ADC_CH6,
	ADC_CH7,
	ADC_CH8,
};

#if IS_ENABLED(CONFIG_DRV_SAMSUNG_PMIC)
static const unsigned long current_coeffs[S2DOS05_MAX_ADC_CHANNEL] =
	{CURRENT_ELVDD, CURRENT_ELVSS, CURRENT_AVDD,
	 CURRENT_BUCK, CURRENT_L1, CURRENT_L2, CURRENT_L3, CURRENT_L4};

static const unsigned long power_coeffs[S2DOS05_MAX_ADC_CHANNEL] =
	{POWER_ELVDD, POWER_ELVSS, POWER_AVDD,
	 POWER_BUCK, POWER_L1, POWER_L2, POWER_L3, POWER_L4};

static void s2m_adc_read_current_data(struct adc_info *adc_meter, int channel)
{
	u8 data_l = 0;

	s2dos05_update_reg(adc_meter->i2c, S2DOS05_REG_PWRMT_CTRL2,
			   channel, ADC_PTR_MASK);
	s2dos05_read_reg(adc_meter->i2c, S2DOS05_REG_PWRMT_DATA, &data_l);
	adc_meter->adc_val[channel] = data_l;
}

static void s2m_adc_read_power_data(struct adc_info *adc_meter, int channel)
{
	u8 data_l = 0, data_h = 0;

	/* even channel */
	s2dos05_update_reg(adc_meter->i2c, S2DOS05_REG_PWRMT_CTRL2,
			   2*channel, ADC_PTR_MASK);
	s2dos05_read_reg(adc_meter->i2c, S2DOS05_REG_PWRMT_DATA, &data_l);

	/* odd channel */
	s2dos05_update_reg(adc_meter->i2c, S2DOS05_REG_PWRMT_CTRL2,
			   2 * channel + 1, ADC_PTR_MASK);
	s2dos05_read_reg(adc_meter->i2c, S2DOS05_REG_PWRMT_DATA, &data_h);

	adc_meter->adc_val[channel] = ((data_h & 0xff) << 8) | (data_l & 0xff);
}

static void s2m_adc_read_data(struct device *dev, int channel)
{
	struct adc_info *adc_meter = dev_get_drvdata(dev);
	size_t i;
	u8 temp;

	/* ASYNCRD bit '1' --> 2ms delay --> read  in case of ADC Async mode */
	if (adc_meter->adc_sync_mode == ASYNC_MODE) {

		s2dos05_read_reg(adc_meter->i2c, S2DOS05_REG_PWRMT_CTRL1, &temp);
		if (!(temp & 0x40)) {
			switch (adc_meter->adc_mode) {
			case CURRENT_METER:
			case POWER_METER:
				if (channel < 0) {
					for (i = 0; i < S2DOS05_MAX_ADC_CHANNEL; i++)
						adc_meter->adc_val[i] = 0;
				} else
					adc_meter->adc_val[channel] = 0;
				break;
			default:
				dev_err(dev, "%s: invalid adc mode(%d)\n",
					__func__, adc_meter->adc_mode);
				return;
			}
		}
		s2dos05_update_reg(adc_meter->i2c, S2DOS05_REG_PWRMT_CTRL1,
				   ADC_ASYNCRD_MASK, ADC_ASYNCRD_MASK);
		usleep_range(2000, 2100);
	}

	switch (adc_meter->adc_mode) {
	case CURRENT_METER:
		if (channel < 0) {
			for (i = 0; i < S2DOS05_MAX_ADC_CHANNEL; i++)
				s2m_adc_read_current_data(adc_meter, i);
		} else
			s2m_adc_read_current_data(adc_meter, channel);
		break;
	case POWER_METER:
		if (channel < 0) {
			for (i = 0; i < S2DOS05_MAX_ADC_CHANNEL; i++)
				s2m_adc_read_power_data(adc_meter, i);
		} else
			s2m_adc_read_power_data(adc_meter, channel);
		break;
	default:
		dev_err(dev, "%s: invalid adc mode(%d)\n",
			__func__, adc_meter->adc_mode);
	}
#if IS_ENABLED(CONFIG_SEC_PM)
	if (adc_meter->is_sm3080) {
		s2dos05_update_reg(adc_meter->i2c, S2DOS05_REG_PWRMT_CTRL2,
				0, ADC_EN_MASK);
		usleep_range(150, 200);
		s2dos05_update_reg(adc_meter->i2c, S2DOS05_REG_PWRMT_CTRL2,
				ADC_EN_MASK, ADC_EN_MASK);
		dev_info(dev, "%s: power meter on/off\n", __func__);
	}
#endif /* CONFIG_SEC_PM */
}

static unsigned long get_coeff(struct device *dev, u8 adc_reg_num)
{
	struct adc_info *adc_meter = dev_get_drvdata(dev);
	unsigned long coeff;

	if (adc_meter->adc_mode == CURRENT_METER) {
		if (adc_reg_num < S2DOS05_MAX_ADC_CHANNEL)
			coeff = current_coeffs[adc_reg_num];
		else {
			dev_err(dev, "%s: invalid adc regulator number(%d)\n",
				__func__, adc_reg_num);
			coeff = 0;
		}
	} else if (adc_meter->adc_mode == POWER_METER) {
		if (adc_reg_num < S2DOS05_MAX_ADC_CHANNEL)
			coeff = power_coeffs[adc_reg_num];
		else {
			dev_err(dev, "%s: invalid adc regulator number(%d)\n",
				__func__, adc_reg_num);
			coeff = 0;
		}
	} else {
		dev_err(dev, "%s: invalid adc mode(%d)\n",
			__func__, adc_meter->adc_mode);
		coeff = 0;
	}
	return coeff;
}

static ssize_t adc_val_all_show(struct device *dev,
				struct device_attribute *attr, char *buf)
{
	struct adc_info *adc_meter = dev_get_drvdata(dev);
	size_t i = 0;
	int cnt = 0, chan = 0;

	s2m_adc_read_data(dev, -1);
	for (i = 0; i < S2DOS05_MAX_ADC_CHANNEL; i++) {
		chan = ADC_CH0 + i;
		if (adc_meter->adc_mode == POWER_METER)
			cnt += snprintf(buf + cnt, PAGE_SIZE, "CH%d:%7lu uW (%7lu)  ",
					chan, (adc_meter->adc_val[chan] * get_coeff(dev, chan)) / MICRO,
					adc_meter->adc_val[chan]);
		else
			cnt += snprintf(buf + cnt, PAGE_SIZE, "CH%d:%7lu uA (%7lu)  ",
					chan, (adc_meter->adc_val[chan] * get_coeff(dev, chan)),
					adc_meter->adc_val[chan]);
		if (i == S2DOS05_MAX_ADC_CHANNEL / 2 - 1)
				cnt += snprintf(buf + cnt, PAGE_SIZE, "\n");
	}

	cnt += snprintf(buf + cnt, PAGE_SIZE, "\n");
	return cnt;
}

static ssize_t adc_en_show(struct device *dev,
			   struct device_attribute *attr, char *buf)
{
	struct adc_info *adc_meter = dev_get_drvdata(dev);
	u8 adc_ctrl3;

	s2dos05_read_reg(adc_meter->i2c, S2DOS05_REG_PWRMT_CTRL2, &adc_ctrl3);
	if ((adc_ctrl3 & ADC_EN_MASK) == ADC_EN_MASK)
		return snprintf(buf, PAGE_SIZE, "ADC enable (0x%02hhx)\n", adc_ctrl3);
	else
		return snprintf(buf, PAGE_SIZE, "ADC disable (0x%02hhx)\n", adc_ctrl3);
}

static ssize_t adc_en_store(struct device *dev, struct device_attribute *attr,
			    const char *buf, size_t count)
{
	struct adc_info *adc_meter = dev_get_drvdata(dev);
	int ret;
	u8 temp, val;

	ret = kstrtou8(buf, 16, &temp);
	if (ret)
		return -EINVAL;

	switch (temp) {
	case 0:
		val = 0x00;
		break;
	case 1:
		val = ADC_EN_MASK;
		break;
	default:
		val = 0x00;
		break;
	}

	s2dos05_update_reg(adc_meter->i2c, S2DOS05_REG_PWRMT_CTRL2,
			   val, ADC_EN_MASK);
	return count;
}

static ssize_t adc_mode_show(struct device *dev,
			     struct device_attribute *attr, char *buf)
{
	struct adc_info *adc_meter = dev_get_drvdata(dev);
	u8 adc_ctrl3;

	s2dos05_read_reg(adc_meter->i2c, S2DOS05_REG_PWRMT_CTRL2, &adc_ctrl3);

	adc_ctrl3 = adc_ctrl3 & ADC_PGEN_MASK;
	switch (adc_ctrl3) {
	case CURRENT_MODE:
		return snprintf(buf, PAGE_SIZE, "CURRENT MODE (%d)\n", CURRENT_METER);
	case POWER_MODE:
		return snprintf(buf, PAGE_SIZE, "POWER MODE (%d)\n", POWER_METER);
	default:
		return snprintf(buf, PAGE_SIZE, "error (0x%02hhx)\n", adc_ctrl3);
	}
}

static ssize_t adc_mode_store(struct device *dev, struct device_attribute *attr,
				const char *buf, size_t count)
{
	struct adc_info *adc_meter = dev_get_drvdata(dev);
	int ret;
	u8 temp, val;

	ret = kstrtou8(buf, 16, &temp);
	if (ret)
		return -EINVAL;

	switch (temp) {
	case CURRENT_METER:
		adc_meter->adc_mode = CURRENT_METER;
		val = CURRENT_MODE;
		break;
	case POWER_METER:
		adc_meter->adc_mode = POWER_METER;
		val = POWER_MODE;
		break;
	default:
		val = CURRENT_MODE;
		break;
	}
	s2dos05_update_reg(adc_meter->i2c, S2DOS05_REG_PWRMT_CTRL2,
			(ADC_EN_MASK | val), (ADC_EN_MASK | ADC_PGEN_MASK));
	return count;

}

static ssize_t adc_sync_mode_show(struct device *dev,
				  struct device_attribute *attr, char *buf)
{
	struct adc_info *adc_meter = dev_get_drvdata(dev);

	switch (adc_meter->adc_sync_mode) {
	case SYNC_MODE:
		return snprintf(buf, PAGE_SIZE,
				"SYNC_MODE (%d)\n", adc_meter->adc_sync_mode);
	case ASYNC_MODE:
		return snprintf(buf, PAGE_SIZE,
				"ASYNC_MODE (%d)\n", adc_meter->adc_sync_mode);
	default:
		return snprintf(buf, PAGE_SIZE,
				"error (%d)\n", adc_meter->adc_sync_mode);
	}
}

static ssize_t adc_sync_mode_store(struct device *dev, struct device_attribute *attr,
				   const char *buf, size_t count)
{
	struct adc_info *adc_meter = dev_get_drvdata(dev);
	int ret;
	u8 temp;

	ret = kstrtou8(buf, 16, &temp);
	if (ret)
		return -EINVAL;

	switch (temp) {
	case SYNC_MODE:
		adc_meter->adc_sync_mode = SYNC_MODE;
		break;
	case ASYNC_MODE:
		adc_meter->adc_sync_mode = ASYNC_MODE;
		break;
	default:
		adc_meter->adc_sync_mode = SYNC_MODE;
		break;
	}

	return count;

}

static int convert_adc_val(struct device *dev, char *buf, int channel)
{
	struct adc_info *adc_meter = dev_get_drvdata(dev);

	s2m_adc_read_data(dev, channel);

	if (adc_meter->adc_mode == POWER_METER)
		return snprintf(buf, PAGE_SIZE, "%7lu uW\n",
				(adc_meter->adc_val[channel] * get_coeff(dev, channel)) / MICRO);
	else
		return snprintf(buf, PAGE_SIZE, "%7lu uA\n",
				adc_meter->adc_val[channel] * get_coeff(dev, channel));

}

static ssize_t adc_val_0_show(struct device *dev,
			      struct device_attribute *attr, char *buf)
{
	return convert_adc_val(dev, buf, ADC_CH0);
}

static ssize_t adc_val_1_show(struct device *dev,
			      struct device_attribute *attr, char *buf)
{
	return convert_adc_val(dev, buf, ADC_CH1);
}

static ssize_t adc_val_2_show(struct device *dev,
			      struct device_attribute *attr, char *buf)
{
	return convert_adc_val(dev, buf, ADC_CH2);
}

static ssize_t adc_val_3_show(struct device *dev,
			      struct device_attribute *attr, char *buf)
{
	return convert_adc_val(dev, buf, ADC_CH3);
}

static ssize_t adc_val_4_show(struct device *dev,
			      struct device_attribute *attr, char *buf)
{
	return convert_adc_val(dev, buf, ADC_CH4);
}

static ssize_t adc_val_5_show(struct device *dev,
			      struct device_attribute *attr, char *buf)
{
	return convert_adc_val(dev, buf, ADC_CH5);
}

static ssize_t adc_val_6_show(struct device *dev,
			      struct device_attribute *attr, char *buf)
{
	return convert_adc_val(dev, buf, ADC_CH6);
}

static ssize_t adc_val_7_show(struct device *dev,
			      struct device_attribute *attr, char *buf)
{
	return convert_adc_val(dev, buf, ADC_CH7);
}

static void adc_ctrl1_update(struct device *dev)
{
	struct adc_info *adc_meter = dev_get_drvdata(dev);
	/* ADC temporarily off */
	s2dos05_update_reg(adc_meter->i2c,
			   S2DOS05_REG_PWRMT_CTRL2, 0x00, ADC_EN_MASK);

	/* update ADC_CTRL1 register */
	s2dos05_write_reg(adc_meter->i2c,
			  S2DOS05_REG_PWRMT_CTRL1, adc_meter->adc_ctrl1);

	/* ADC Continuous ON */
	s2dos05_update_reg(adc_meter->i2c,
			   S2DOS05_REG_PWRMT_CTRL2, ADC_EN_MASK, ADC_EN_MASK);
}

static ssize_t adc_ctrl1_show(struct device *dev,
			      struct device_attribute *attr, char *buf)
{
	struct adc_info *adc_meter = dev_get_drvdata(dev);
	return snprintf(buf, PAGE_SIZE, "0x%02hhx\n", adc_meter->adc_ctrl1);
}

static ssize_t adc_ctrl1_store(struct device *dev, struct device_attribute *attr,
				const char *buf, size_t count)
{
	struct adc_info *adc_meter = dev_get_drvdata(dev);
	int ret;
	u8 temp;

	ret = kstrtou8(buf, 16, &temp);
	if (ret)
		return -EINVAL;

	temp &= SMPNUM_MASK;
	adc_meter->adc_ctrl1 &= ~SMPNUM_MASK;
	adc_meter->adc_ctrl1 |= temp;
	adc_ctrl1_update(dev);
	return count;
}

#if IS_ENABLED(CONFIG_SEC_PM)
static ssize_t adc_validity_show(struct device *dev, struct device_attribute *attr, char *buf)
{
	struct adc_info *adc_meter = dev_get_drvdata(dev);
	u8 adc_validity;

	s2dos05_read_reg(adc_meter->i2c, S2DOS05_REG_OCL, &adc_validity);
	return snprintf(buf, PAGE_SIZE, "%d\n", (adc_validity >> 7));
}
#endif /* CONFIG_SEC_PM */

static struct pmic_device_attribute powermeter_attr[] = {
	PMIC_ATTR(adc_val_all, S_IRUGO, adc_val_all_show, NULL),
	PMIC_ATTR(adc_en, S_IRUGO | S_IWUSR, adc_en_show, adc_en_store),
	PMIC_ATTR(adc_mode, S_IRUGO | S_IWUSR, adc_mode_show, adc_mode_store),
	PMIC_ATTR(adc_sync_mode, S_IRUGO | S_IWUSR, adc_sync_mode_show, adc_sync_mode_store),
	PMIC_ATTR(adc_val_0, S_IRUGO, adc_val_0_show, NULL),
	PMIC_ATTR(adc_val_1, S_IRUGO, adc_val_1_show, NULL),
	PMIC_ATTR(adc_val_2, S_IRUGO, adc_val_2_show, NULL),
	PMIC_ATTR(adc_val_3, S_IRUGO, adc_val_3_show, NULL),
	PMIC_ATTR(adc_val_4, S_IRUGO, adc_val_4_show, NULL),
	PMIC_ATTR(adc_val_5, S_IRUGO, adc_val_5_show, NULL),
	PMIC_ATTR(adc_val_6, S_IRUGO, adc_val_6_show, NULL),
	PMIC_ATTR(adc_val_7, S_IRUGO, adc_val_7_show, NULL),
	PMIC_ATTR(adc_ctrl1, S_IRUGO | S_IWUSR, adc_ctrl1_show, adc_ctrl1_store),
#if IS_ENABLED(CONFIG_SEC_PM)
	PMIC_ATTR(adc_validity, S_IRUGO, adc_validity_show, NULL),
#endif /* CONFIG_SEC_PM */
};

static int s2dos05_create_powermeter_sysfs(struct s2dos05_dev *s2dos05)
{
	struct device *s2dos05_adc_dev;
	struct device *dev = s2dos05->dev;
	char device_name[50] = {0, };
	int err = -ENODEV, i = 0;

	pr_info("%s()\n", __func__);

	/* Dynamic allocation for device name */
	snprintf(device_name, sizeof(device_name) - 1, "%s-powermeter@%s",
		 dev_driver_string(dev), dev_name(dev));

	s2dos05_adc_dev = pmic_device_create(s2dos05->adc_meter, device_name);
	s2dos05->powermeter_dev = s2dos05_adc_dev;

	/* Create sysfs entries */
	for (i = 0; i < ARRAY_SIZE(powermeter_attr); i++) {
		err = device_create_file(s2dos05_adc_dev, &powermeter_attr[i].dev_attr);
		if (err)
			goto remove_pmic_device;
	}

#if IS_ENABLED(CONFIG_SEC_PM)
	if (!IS_ERR_OR_NULL(s2dos05->sec_disp_pmic_dev)) {
		err = sysfs_create_link(&s2dos05->sec_disp_pmic_dev->kobj,
				&s2dos05_adc_dev->kobj, "power_meter");
		if (err) {
			pr_err("%s: fail to create link for power_meter\n",
					__func__);
			goto remove_pmic_device;
		}
	}
#endif /* CONFIG_SEC_PM */

	return 0;

remove_pmic_device:
	for (i--; i >= 0; i--)
		device_remove_file(s2dos05_adc_dev, &powermeter_attr[i].dev_attr);
	pmic_device_destroy(s2dos05_adc_dev->devt);

	return -1;
}
#endif

static void s2dos05_set_smp_num(struct adc_info *adc_meter)
{
	switch (adc_meter->adc_mode) {
	case CURRENT_METER:
		adc_meter->adc_ctrl1 = 0x0C;
		break;
	case POWER_METER:
		adc_meter->adc_ctrl1 = 0x0C;
		break;
	default:
		adc_meter->adc_ctrl1 = 0x0C;
		break;
	}

}

static void s2dos05_adc_set_enable(struct adc_info *adc_meter, struct s2dos05_dev *s2dos05)
{
	switch (adc_meter->adc_mode) {
	case CURRENT_METER:
		s2dos05_update_reg(s2dos05->i2c, S2DOS05_REG_PWRMT_CTRL2,
				   (ADC_EN_MASK | CURRENT_MODE),
				   (ADC_EN_MASK | ADC_PGEN_MASK));
		pr_info("%s: current mode enable (0x%2x)\n",
			__func__, (ADC_EN_MASK | CURRENT_MODE));
		break;
	case POWER_METER:
		s2dos05_update_reg(s2dos05->i2c, S2DOS05_REG_PWRMT_CTRL2,
				   (ADC_EN_MASK | POWER_MODE),
				   (ADC_EN_MASK | ADC_PGEN_MASK));
		pr_info("%s: power mode enable (0x%2x)\n",
			__func__, (ADC_EN_MASK | POWER_MODE));
		break;
	default:
		s2dos05_update_reg(s2dos05->i2c, S2DOS05_REG_PWRMT_CTRL2,
				   (0x00 | CURRENT_MODE),
				   (ADC_EN_MASK | ADC_PGEN_MASK));
		pr_info("%s: current/power meter disable (0x%2x)\n",
			__func__, (ADC_EN_MASK | CURRENT_MODE));

	}
}

void s2dos05_powermeter_init(struct s2dos05_dev *s2dos05)
{
	struct adc_info *adc_meter;

	adc_meter = devm_kzalloc(s2dos05->dev,
				 sizeof(struct adc_info), GFP_KERNEL);
	if (!adc_meter) {
		pr_err("%s: adc_meter alloc fail.\n", __func__);
		return;
	}

	adc_meter->adc_val = devm_kzalloc(s2dos05->dev,
					  sizeof(unsigned long) * S2DOS05_MAX_ADC_CHANNEL,
					  GFP_KERNEL);

	pr_info("%s: s2dos05 power meter init start\n", __func__);

	adc_meter->adc_mode = s2dos05->adc_mode;
	adc_meter->adc_sync_mode = s2dos05->adc_sync_mode;

	/* POWER_METER mode needs bigger SMP_NUM to get stable value */
	s2dos05_set_smp_num(adc_meter);

	/*  SMP_NUM = 1100(16384) ~16s in case of aync mode */
	if (adc_meter->adc_sync_mode == ASYNC_MODE)
		adc_meter->adc_ctrl1 = 0x0C;

#if IS_ENABLED(CONFIG_SEC_PM)
	adc_meter->is_sm3080 = s2dos05->is_sm3080;
	if (adc_meter->is_sm3080)
		adc_meter->adc_ctrl1 = 0x0B; /* 10 seconds */
#endif /* CONFIG_SEC_PM */

	s2dos05_write_reg(s2dos05->i2c,
			  S2DOS05_REG_PWRMT_CTRL1, adc_meter->adc_ctrl1);

	/* ADC EN */
	s2dos05_adc_set_enable(adc_meter, s2dos05);

	adc_meter->i2c = s2dos05->i2c;
	s2dos05->adc_meter = adc_meter;

#if IS_ENABLED(CONFIG_DRV_SAMSUNG_PMIC)
	if (s2dos05_create_powermeter_sysfs(s2dos05) < 0) {
		pr_info("%s: failed to create sysfs\n", __func__);
		return;
	}
#endif
	pr_info("%s: s2dos05 power meter init end\n", __func__);
}
EXPORT_SYMBOL_GPL(s2dos05_powermeter_init);

void s2dos05_powermeter_deinit(struct s2dos05_dev *s2dos05)
{
#if IS_ENABLED(CONFIG_DRV_SAMSUNG_PMIC)
	struct device *s2dos05_adc_dev = s2dos05->powermeter_dev;
	int i = 0;

	/* Remove sysfs entries */
	for (i = 0; i < ARRAY_SIZE(powermeter_attr); i++)
		device_remove_file(s2dos05_adc_dev, &powermeter_attr[i].dev_attr);
	pmic_device_destroy(s2dos05_adc_dev->devt);
#endif

	/* ADC turned off */
	s2dos05_write_reg(s2dos05->i2c, S2DOS05_REG_PWRMT_CTRL2, 0);
	pr_info("%s: s2dos05 power meter deinit\n", __func__);
}
EXPORT_SYMBOL_GPL(s2dos05_powermeter_deinit);
