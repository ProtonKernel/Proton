/*
 * s2mps23-powermeter.c
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

#include <linux/err.h>
#include <linux/slab.h>
#include <linux/mfd/samsung/s2mps23.h>
#include <linux/mfd/samsung/s2mps23-regulator.h>
#include <linux/platform_device.h>
#include <linux/regulator/pmic_class.h>

/* Power-meter registers */
#define ADC_CTRL1		(0x04)
#define ADC_CTRL2		(0x05)
#define ADC_CTRL3		(0x06)
#define MUX0SEL			(0x08)
#define MUX1SEL			(0x09)
#define MUX2SEL			(0x0A)
#define MUX3SEL			(0x0B)
#define MUX4SEL			(0x0C)
#define MUX5SEL			(0x0D)
#define MUX6SEL			(0x0E)
#define MUX7SEL			(0x0F)
#define ADC0_ACC0		(0x23)
#define ADC1_ACC0		(0x27)
#define ADC2_ACC0		(0x2B)
#define ADC3_ACC0		(0x2F)
#define ADC4_ACC0		(0x33)
#define ADC5_ACC0		(0x37)
#define ADC6_ACC0		(0x3B)
#define ADC7_ACC0		(0x3F)
#define ACC_COUNT_L		(0x43)
#define ACC_COUNT_H		(0x44)

/* Power-meter setting */
#define PICO_MICRO		(1000000)
#define MAX_ADC_OUTPUT		(4)
#define MAX_ADC_CHANNEL		(8)
#define BUCK_START		(0x01)
#define BUCK_END		(0x06)
#define BUCK_CNT		(6)
#define ADC_ENABLE		(0x01)
#define ADC_DISABLE		(0x00)
#define ADCEN_MASK		(0x01)
#define ASYNC_RD		(0x02)
#define DIV_RATIO		(0x60)
#define CH_SEL			(0x07)

#define ADC_B1_SUM		(0x01)
#define ADC_B2			(0x02)
#define ADC_B3_SUM		(0x03)
#define ADC_B4			(0x04)
#define ADC_B5			(0x05)
#define ADC_B6			(0x06)

#define POWER_B1		(292969000) /* unit: pW */
#define POWER_B2		(97656300)
#define POWER_B3		(292969000)
#define POWER_B4		(97656300)
#define POWER_B5		(97656300)
#define POWER_B6		(97656300)

struct adc_info {
	struct i2c_client *i2c;
	struct mutex adc_lock;
	unsigned long *power_val;
	u8 *adc_reg;
	u8 pmic_rev;
};

/* only use BUCK1~6 */
static const unsigned long power_coeffs[BUCK_CNT] =
	{POWER_B1, POWER_B2, POWER_B3, POWER_B4, POWER_B5, POWER_B6};

enum s2mps23_adc_ch {
	ADC_CH0 = 0,
	ADC_CH1,
	ADC_CH2,
	ADC_CH3,
	ADC_CH4,
	ADC_CH5,
	ADC_CH6,
	ADC_CH7,
};

static const u8 adc_channel_reg[MAX_ADC_CHANNEL] = {
	[ADC_CH0] = ADC0_ACC0,
	[ADC_CH1] = ADC1_ACC0,
	[ADC_CH2] = ADC2_ACC0,
	[ADC_CH3] = ADC3_ACC0,
	[ADC_CH4] = ADC4_ACC0,
	[ADC_CH5] = ADC5_ACC0,
	[ADC_CH6] = ADC6_ACC0,
	[ADC_CH7] = ADC7_ACC0,
};

#if IS_ENABLED(CONFIG_DRV_SAMSUNG_PMIC)
static int adc_assign_channel(struct adc_info *adc_meter);

static unsigned long adc_get_count(struct adc_info *adc_meter)
{
	struct i2c_client *i2c = adc_meter->i2c;
	u8 acc_count_l = 0, acc_count_h = 0;
	unsigned long count = 0;
	int ret = 0;

	ret = s2mps23_read_reg(i2c, ACC_COUNT_L, &acc_count_l);
	if (ret)
		pr_err("%s: failed to register\n", __func__);
	ret = s2mps23_read_reg(i2c, ACC_COUNT_H, &acc_count_h);
	if (ret)
		pr_err("%s: failed to register\n", __func__);

	count = ((acc_count_h & 0x3F) << 8) | (acc_count_l);

	return count;
}

static unsigned long adc_get_acc(struct adc_info *adc_meter, int chan)
{
	struct i2c_client *i2c = adc_meter->i2c;
	u8 adc_acc[MAX_ADC_OUTPUT] = {0};
	unsigned long acc = 0;
	int ret = 0;
	size_t i = 0;

	for (i = 0; i < MAX_ADC_OUTPUT; i++) {
		ret = s2mps23_read_reg(i2c, adc_channel_reg[chan] + i, adc_acc + i);
		if (ret) {
			pr_err("%s: failed to register\n", __func__);
			break;
		}
	}

	acc = ((adc_acc[3] & 0x7F) << 24) | (adc_acc[2] << 16) | (adc_acc[1] << 8) | (adc_acc[0]);

	return acc;
}

static unsigned long adc_get_pout(struct adc_info *adc_meter, int chan)
{
	unsigned long pout = 0, acc = 0, count = 0;

	acc = adc_get_acc(adc_meter, chan);
	count = adc_get_count(adc_meter);

	/* Calculate power output */
	pout = 2 * acc / count;

	return pout;
}

static int adc_check_async(struct adc_info *adc_meter)
{
	struct i2c_client *i2c = adc_meter->i2c;
	int ret = 0;
	u8 val = 0;

	ret = s2mps23_update_reg(i2c, ADC_CTRL1, ASYNC_RD, ASYNC_RD);
	if (ret)
		goto err;

	usleep_range(2000, 2100);
	ret = s2mps23_read_reg(i2c, ADC_CTRL1, &val);
	if (ret)
		goto err;

	pr_info("%s: check async clear(0x%02hhx)\n", __func__, val);

	if (val & ASYNC_RD)
		goto err;

	return 0;
err:
	return -1;
}

static int adc_read_data(struct adc_info *adc_meter, int channel)
{
	size_t i = 0;

	mutex_lock(&adc_meter->adc_lock);

	if (adc_check_async(adc_meter) < 0) {
		pr_err("%s: adc_check_async fail\n", __func__);
		mutex_unlock(&adc_meter->adc_lock);
		goto err;
	}

	if (channel < 0)
		for (i = 0; i < MAX_ADC_CHANNEL; i++)
			adc_meter->power_val[i] = adc_get_pout(adc_meter, i);
	else
		adc_meter->power_val[channel] = adc_get_pout(adc_meter, channel);

	mutex_unlock(&adc_meter->adc_lock);

	return 0;
err:
	return -1;
}

static unsigned long get_coeff_p(u8 adc_reg_num)
{
	unsigned long coeff = 0;

	coeff = power_coeffs[adc_reg_num - BUCK_START];

	return coeff;
}

static ssize_t adc_val_power_show(struct device *dev,
				  struct device_attribute *attr, char *buf)
{
	struct adc_info *adc_meter = dev_get_drvdata(dev);
	size_t i = 0;
	int cnt = 0, chan = 0;

	for(i = 0; i < MAX_ADC_CHANNEL; i++) {
		if ((adc_meter->adc_reg[i] < BUCK_START) &&
		    (adc_meter->adc_reg[i] > BUCK_END))
			return snprintf(buf, PAGE_SIZE,
					"Power-Meter supports only BUCK%d~%d\n",
					BUCK_START, BUCK_END);
	}

	if (adc_read_data(adc_meter, -1) < 0)
		goto err;

	for (i = 0; i < MAX_ADC_CHANNEL; i++) {
		chan = ADC_CH0 + i;
		cnt += snprintf(buf + cnt, PAGE_SIZE, "CH%d[0x%02hhx]:%7lu uW (%7lu)  ",
				chan,
				adc_meter->adc_reg[chan],
				(adc_meter->power_val[chan] * get_coeff_p(adc_meter->adc_reg[chan])) / PICO_MICRO,
				adc_meter->power_val[chan]);

		if (i == MAX_ADC_CHANNEL / 2 - 1)
			cnt += snprintf(buf + cnt, PAGE_SIZE, "\n");
	}
	cnt += snprintf(buf + cnt, PAGE_SIZE, "\n");

	return cnt;
err:
	return snprintf(buf, PAGE_SIZE, "Not clear ASYNC_RD\n");
}

static ssize_t adc_en_show(struct device *dev,
			   struct device_attribute *attr, char *buf)
{
	struct adc_info *adc_meter = dev_get_drvdata(dev);
	int ret = 0;
	u8 val = 0;

	ret = s2mps23_read_reg(adc_meter->i2c, ADC_CTRL1, &val);
	if (ret)
		pr_err("%s: failed to read register\n", __func__);

	if (val & ADCEN_MASK)
		return snprintf(buf, PAGE_SIZE, "ADC enable(0x%02hhx)\n", val);
	else
		return snprintf(buf, PAGE_SIZE, "ADC disable(0x%02hhx)\n", val);
}

static ssize_t adc_en_store(struct device *dev, struct device_attribute *attr,
			    const char *buf, size_t count)
{
	struct adc_info *adc_meter = dev_get_drvdata(dev);
	int ret = 0;
	u8 temp = 0, val = 0;

	ret = kstrtou8(buf, 16, &temp);
	if (ret)
		return -EINVAL;

	if (temp == ADCEN_MASK)
		val = temp;

	ret = s2mps23_update_reg(adc_meter->i2c, ADC_CTRL1, val, ADCEN_MASK);
	if (ret)
		pr_err("%s: failed to update register\n", __func__);

	return count;
}

static int convert_adc_val(struct device *dev, char *buf, int channel)
{
	struct adc_info *adc_meter = dev_get_drvdata(dev);
	unsigned long coeff_p = get_coeff_p(adc_meter->adc_reg[channel]);

	if (adc_read_data(adc_meter, channel) < 0)
		return snprintf(buf, PAGE_SIZE, "Not clear ASYNC_RD\n");

	return snprintf(buf, PAGE_SIZE, "CH%d[0x%02hhx]:%7lu uW (%7lu)\n",
			channel, adc_meter->adc_reg[channel],
			(adc_meter->power_val[channel] * coeff_p) / PICO_MICRO,
			adc_meter->power_val[channel]);
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

static ssize_t adc_reg_0_show(struct device *dev,
			      struct device_attribute *attr, char *buf)
{
	struct adc_info *adc_meter = dev_get_drvdata(dev);

	return snprintf(buf, PAGE_SIZE, "0x%02hhx\n", adc_meter->adc_reg[ADC_CH0]);
}

static ssize_t adc_reg_1_show(struct device *dev,
			      struct device_attribute *attr, char *buf)
{
	struct adc_info *adc_meter = dev_get_drvdata(dev);

	return snprintf(buf, PAGE_SIZE, "0x%02hhx\n", adc_meter->adc_reg[ADC_CH1]);
}

static ssize_t adc_reg_2_show(struct device *dev,
			      struct device_attribute *attr, char *buf)
{
	struct adc_info *adc_meter = dev_get_drvdata(dev);

	return snprintf(buf, PAGE_SIZE, "0x%02hhx\n", adc_meter->adc_reg[ADC_CH2]);
}

static ssize_t adc_reg_3_show(struct device *dev,
			      struct device_attribute *attr, char *buf)
{
	struct adc_info *adc_meter = dev_get_drvdata(dev);

	return snprintf(buf, PAGE_SIZE, "0x%02hhx\n", adc_meter->adc_reg[ADC_CH3]);
}

static ssize_t adc_reg_4_show(struct device *dev,
			      struct device_attribute *attr, char *buf)
{
	struct adc_info *adc_meter = dev_get_drvdata(dev);

	return snprintf(buf, PAGE_SIZE, "0x%02hhx\n", adc_meter->adc_reg[ADC_CH4]);
}

static ssize_t adc_reg_5_show(struct device *dev,
			      struct device_attribute *attr, char *buf)
{
	struct adc_info *adc_meter = dev_get_drvdata(dev);

	return snprintf(buf, PAGE_SIZE, "0x%02hhx\n", adc_meter->adc_reg[ADC_CH5]);
}

static ssize_t adc_reg_6_show(struct device *dev,
			      struct device_attribute *attr, char *buf)
{
	struct adc_info *adc_meter = dev_get_drvdata(dev);

	return snprintf(buf, PAGE_SIZE, "0x%02hhx\n", adc_meter->adc_reg[ADC_CH6]);
}

static ssize_t adc_reg_7_show(struct device *dev,
			      struct device_attribute *attr, char *buf)
{
	struct adc_info *adc_meter = dev_get_drvdata(dev);

	return snprintf(buf, PAGE_SIZE, "0x%02hhx\n", adc_meter->adc_reg[ADC_CH7]);
}

static void adc_reg_update(struct adc_info *adc_meter)
{
	if (s2mps23_adc_set_enable(adc_meter, ADC_DISABLE) < 0)
		pr_err("%s: s2mps23_adc_set_enable fail\n", __func__);

	if (adc_assign_channel(adc_meter) < 0)
		pr_err("%s: adc_assign_channel fail\n", __func__);

	if (s2mps23_adc_set_enable(adc_meter, ADC_ENABLE) < 0)
		pr_err("%s: s2mps23_adc_set_enable fail\n", __func__);
}

static u8 buf_to_adc_reg(const char *buf)
{
	u8 adc_reg_num = 0;

	if (kstrtou8(buf, 16, &adc_reg_num))
		return 0;

	if (adc_reg_num >= BUCK_START && adc_reg_num <= BUCK_END)
		return adc_reg_num;
	else
		return 0;
}

static ssize_t assign_adc_reg(struct device *dev, const char *buf,
			      size_t count, int channel)
{
	struct adc_info *adc_meter = dev_get_drvdata(dev);

	u8 adc_reg_num = buf_to_adc_reg(buf);
	if (!adc_reg_num)
		return -EINVAL;
	else {
		adc_meter->adc_reg[channel] = adc_reg_num;
		adc_reg_update(adc_meter);
		return count;
	}
}

static ssize_t adc_reg_0_store(struct device *dev, struct device_attribute *attr,
				const char *buf, size_t count)
{
	return assign_adc_reg(dev, buf, count, ADC_CH0);
}

static ssize_t adc_reg_1_store(struct device *dev, struct device_attribute *attr,
				const char *buf, size_t count)
{
	return assign_adc_reg(dev, buf, count, ADC_CH1);
}

static ssize_t adc_reg_2_store(struct device *dev, struct device_attribute *attr,
				const char *buf, size_t count)
{
	return assign_adc_reg(dev, buf, count, ADC_CH2);
}

static ssize_t adc_reg_3_store(struct device *dev, struct device_attribute *attr,
				const char *buf, size_t count)
{
	return assign_adc_reg(dev, buf, count, ADC_CH3);
}

static ssize_t adc_reg_4_store(struct device *dev, struct device_attribute *attr,
				const char *buf, size_t count)
{
	return assign_adc_reg(dev, buf, count, ADC_CH4);
}
static ssize_t adc_reg_5_store(struct device *dev, struct device_attribute *attr,
				const char *buf, size_t count)
{
	return assign_adc_reg(dev, buf, count, ADC_CH5);
}
static ssize_t adc_reg_6_store(struct device *dev, struct device_attribute *attr,
				const char *buf, size_t count)
{
	return assign_adc_reg(dev, buf, count, ADC_CH6);
}
static ssize_t adc_reg_7_store(struct device *dev, struct device_attribute *attr,
				const char *buf, size_t count)
{
	return assign_adc_reg(dev, buf, count, ADC_CH7);
}

#define ATTR_POWERMETER	(18)
static struct pmic_device_attribute powermeter_attr[] = {
	PMIC_ATTR(power_val_all, S_IRUGO, adc_val_power_show, NULL),
	PMIC_ATTR(adc_en, S_IRUGO | S_IWUSR, adc_en_show, adc_en_store),
	PMIC_ATTR(adc_val_0, S_IRUGO, adc_val_0_show, NULL),
	PMIC_ATTR(adc_val_1, S_IRUGO, adc_val_1_show, NULL),
	PMIC_ATTR(adc_val_2, S_IRUGO, adc_val_2_show, NULL),
	PMIC_ATTR(adc_val_3, S_IRUGO, adc_val_3_show, NULL),
	PMIC_ATTR(adc_val_4, S_IRUGO, adc_val_4_show, NULL),
	PMIC_ATTR(adc_val_5, S_IRUGO, adc_val_5_show, NULL),
	PMIC_ATTR(adc_val_6, S_IRUGO, adc_val_6_show, NULL),
	PMIC_ATTR(adc_val_7, S_IRUGO, adc_val_7_show, NULL),
	PMIC_ATTR(adc_reg_0, S_IRUGO | S_IWUSR, adc_reg_0_show, adc_reg_0_store),
	PMIC_ATTR(adc_reg_1, S_IRUGO | S_IWUSR, adc_reg_1_show, adc_reg_1_store),
	PMIC_ATTR(adc_reg_2, S_IRUGO | S_IWUSR, adc_reg_2_show, adc_reg_2_store),
	PMIC_ATTR(adc_reg_3, S_IRUGO | S_IWUSR, adc_reg_3_show, adc_reg_3_store),
	PMIC_ATTR(adc_reg_4, S_IRUGO | S_IWUSR, adc_reg_4_show, adc_reg_4_store),
	PMIC_ATTR(adc_reg_5, S_IRUGO | S_IWUSR, adc_reg_5_show, adc_reg_5_store),
	PMIC_ATTR(adc_reg_6, S_IRUGO | S_IWUSR, adc_reg_6_show, adc_reg_6_store),
	PMIC_ATTR(adc_reg_7, S_IRUGO | S_IWUSR, adc_reg_7_show, adc_reg_7_store),
};

static int s2mps23_create_powermeter_sysfs(struct s2mps23_dev *s2mps23)
{
	struct device *s2mps23_adc_dev;
	struct device *dev = s2mps23->dev;
	char device_name[32] = {0, };
	int err = -ENODEV, i = 0;

	pr_info("%s()\n", __func__);

	/* Dynamic allocation for device name */
	snprintf(device_name, sizeof(device_name) - 1, "%s-powermeter@%s",
		 dev_driver_string(dev), dev_name(dev));

	s2mps23_adc_dev = pmic_device_create(s2mps23->adc_meter, device_name);
	s2mps23->powermeter_dev = s2mps23_adc_dev;

	/* Create sysfs entries */
	for (i = 0; i < ATTR_POWERMETER; i++) {
		err = device_create_file(s2mps23_adc_dev, &powermeter_attr[i].dev_attr);
		if (err)
			goto remove_pmic_device;
	}

#if IS_ENABLED(CONFIG_SEC_PM)
	if (!IS_ERR_OR_NULL(s2mps23->ap_pmic_dev)) {
		err = sysfs_create_link(&s2mps23->ap_pmic_dev->kobj,
				&s2mps23_adc_dev->kobj, "power_meter");
		if (err)
			pr_err("%s: fail to create link for power_meter(%d)\n",
					__func__, err);
	}
#endif /* CONFIG_SEC_PM */

	return 0;

remove_pmic_device:
	for (i--; i >= 0; i--)
		device_remove_file(s2mps23_adc_dev, &powermeter_attr[i].dev_attr);
	pmic_device_destroy(s2mps23_adc_dev->devt);

	return -1;
}
#endif

int s2mps23_adc_set_enable(struct adc_info *adc_meter, int en)
{
	int ret = 0;
	u8 val = 0;

	if (en)
		val = ADC_ENABLE;
	else
		val = ADC_DISABLE;

	ret = s2mps23_update_reg(adc_meter->i2c, ADC_CTRL1, val, ADCEN_MASK);
	if (ret)
		return -1;

	return 0;
}
EXPORT_SYMBOL_GPL(s2mps23_adc_set_enable);

static int adc_assign_channel(struct adc_info *adc_meter)
{
	int ret = 0, count = 0;
	ssize_t i = 0;

	for (i = MUX0SEL; i <= MUX7SEL; i++) {
		ret = s2mps23_write_reg(adc_meter->i2c, i,
					adc_meter->adc_reg[count++]);
		if (ret) {
			pr_err("%s: failed to write register\n", __func__);
			return -1;
		}
	}

	pr_info("%s: Done\n", __func__);

	return 0;
}

static int adc_set_channel(struct adc_info *adc_meter)
{
	/* Assign BUCK 1~6 for MUX channel */
	adc_meter->adc_reg[ADC_CH0] = ADC_B1_SUM;
	adc_meter->adc_reg[ADC_CH1] = ADC_B2;
	adc_meter->adc_reg[ADC_CH2] = ADC_B3_SUM;
	adc_meter->adc_reg[ADC_CH3] = ADC_B4;
	adc_meter->adc_reg[ADC_CH4] = ADC_B5;
	adc_meter->adc_reg[ADC_CH5] = ADC_B6;
	adc_meter->adc_reg[ADC_CH6] = ADC_B6;
	adc_meter->adc_reg[ADC_CH7] = ADC_B6;

	if (adc_assign_channel(adc_meter) < 0)
		return -1;

	return 0;
}

static int adc_init_hw(struct adc_info *adc_meter)
{
	int ret = 0;

	/* Set DIV_RATIO(125khz) */
	ret = s2mps23_update_reg(adc_meter->i2c, ADC_CTRL1, 0x40, DIV_RATIO);
	if (ret)
		goto err;

	/* Set CH_SEL to use all the channels */
	ret = s2mps23_update_reg(adc_meter->i2c, ADC_CTRL3, 0x07, CH_SEL);
	if (ret)
		goto err;

	/* Enable ADC power mode */
	ret = s2mps23_write_reg(adc_meter->i2c, ADC_CTRL2, 0x00);
	if (ret)
		goto err;

	return 0;
err:
	return -1;
}

void s2mps23_powermeter_init(struct s2mps23_dev *s2mps23)
{
	struct adc_info *adc_meter;

	pr_info("%s: Start\n", __func__);

	adc_meter = devm_kzalloc(s2mps23->dev,
				 sizeof(struct adc_info), GFP_KERNEL);
	if (!adc_meter) {
		pr_err("%s: adc_meter alloc fail.\n", __func__);
		return;
	}

	adc_meter->power_val = devm_kzalloc(s2mps23->dev,
					    sizeof(unsigned long) * MAX_ADC_CHANNEL,
					    GFP_KERNEL);
	adc_meter->adc_reg = devm_kzalloc(s2mps23->dev,
					  sizeof(u8) * MAX_ADC_CHANNEL,
					  GFP_KERNEL);
	adc_meter->pmic_rev = s2mps23->pmic_rev;
	adc_meter->i2c = s2mps23->adc_i2c;
	mutex_init(&adc_meter->adc_lock);
	s2mps23->adc_meter = adc_meter;

	if (adc_init_hw(adc_meter) < 0) {
		pr_err("%s: adc_init_hw fail\n", __func__);
		return;
	}

	if (adc_set_channel(adc_meter) < 0) {
		pr_err("%s: adc_set_channel fail\n", __func__);
		return;
	}

	if (s2mps23_adc_set_enable(adc_meter, ADC_ENABLE) < 0) {
		pr_err("%s: s2mps23_adc_set_enable fail\n", __func__);
		return;
	}

#if IS_ENABLED(CONFIG_DRV_SAMSUNG_PMIC)
	if (s2mps23_create_powermeter_sysfs(s2mps23) < 0) {
		pr_info("%s: failed to create sysfs\n", __func__);
		return;
	}
#endif
	pr_info("%s: Done\n", __func__);
}
EXPORT_SYMBOL_GPL(s2mps23_powermeter_init);

void s2mps23_powermeter_deinit(struct s2mps23_dev *s2mps23)
{
#if IS_ENABLED(CONFIG_DRV_SAMSUNG_PMIC)
	struct device *s2mps23_adc_dev = s2mps23->powermeter_dev;
	int i = 0;

	/* Remove sysfs entries */
	for (i = 0; i < ATTR_POWERMETER; i++)
		device_remove_file(s2mps23_adc_dev, &powermeter_attr[i].dev_attr);
	pmic_device_destroy(s2mps23_adc_dev->devt);
#endif
	/* ADC turned off */
	s2mps23_adc_set_enable(s2mps23->adc_meter, ADC_DISABLE);
}
EXPORT_SYMBOL(s2mps23_powermeter_deinit);
