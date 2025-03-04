/*
 * s2mps24.c - mfd core driver for the s2mps24
 *
 * Copyright (C) 2020 Samsung Electronics
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 *
 */

#include <linux/module.h>
#include <linux/slab.h>
#include <linux/i2c.h>
#include <linux/irq.h>
#include <linux/interrupt.h>
#include <linux/mutex.h>
#include <linux/mfd/core.h>
#include <linux/mfd/samsung/s2mps24.h>
#include <linux/mfd/samsung/s2mps24-regulator.h>
#include <linux/regulator/machine.h>
#include <linux/rtc.h>
#if IS_ENABLED(CONFIG_EXYNOS_ACPM)
#include <soc/samsung/acpm_mfd.h>
#define SLAVE_CHANNEL	1
#endif
#if IS_ENABLED(CONFIG_OF)
#include <linux/of_device.h>
#include <linux/of_gpio.h>
#endif /* CONFIG_OF */

#define I2C_ADDR_TOP	0x00
#define I2C_ADDR_PMIC	0x01
#define I2C_ADDR_DEBUG	0x0F
#define I2C_ADDR_ADC	0x0A

#if IS_ENABLED(CONFIG_EXYNOS_ACPM)
static struct device_node *acpm_mfd_node;
#endif
static struct mfd_cell s2mps24_devs[] = {
	{ .name = "s2mps24-regulator", },
};

int s2mps24_read_reg(struct i2c_client *i2c, u8 reg, u8 *dest)
{

#if IS_ENABLED(CONFIG_EXYNOS_ACPM)
	struct s2mps24_dev *s2mps24 = i2c_get_clientdata(i2c);
	int ret;

	mutex_lock(&s2mps24->i2c_lock);
	ret = exynos_acpm_read_reg(acpm_mfd_node, SLAVE_CHANNEL, i2c->addr, reg, dest);
	mutex_unlock(&s2mps24->i2c_lock);
	if (ret) {
		pr_err("[%s] acpm ipc fail!\n", __func__);
		return ret;
	}
#endif
	return 0;
}
EXPORT_SYMBOL_GPL(s2mps24_read_reg);

int s2mps24_bulk_read(struct i2c_client *i2c, u8 reg, int count, u8 *buf)
{
#if IS_ENABLED(CONFIG_EXYNOS_ACPM)
	struct s2mps24_dev *s2mps24 = i2c_get_clientdata(i2c);
	int ret;

	mutex_lock(&s2mps24->i2c_lock);
	ret = exynos_acpm_bulk_read(acpm_mfd_node, SLAVE_CHANNEL, i2c->addr, reg, count, buf);
	mutex_unlock(&s2mps24->i2c_lock);
	if (ret) {
		pr_err("[%s] acpm ipc fail!\n", __func__);
		return ret;
	}
#endif
	return 0;
}
EXPORT_SYMBOL_GPL(s2mps24_bulk_read);

int s2mps24_write_reg(struct i2c_client *i2c, u8 reg, u8 value)
{
	int ret = 0;
#if IS_ENABLED(CONFIG_EXYNOS_ACPM)
	struct s2mps24_dev *s2mps24 = i2c_get_clientdata(i2c);

	mutex_lock(&s2mps24->i2c_lock);
	ret = exynos_acpm_write_reg(acpm_mfd_node, SLAVE_CHANNEL, i2c->addr, reg, value);
	mutex_unlock(&s2mps24->i2c_lock);
	if (ret) {
		pr_err("[%s] acpm ipc fail!\n", __func__);
		return ret;
	}
#endif
	return ret;
}
EXPORT_SYMBOL_GPL(s2mps24_write_reg);

int s2mps24_bulk_write(struct i2c_client *i2c, u8 reg, int count, u8 *buf)
{
	int ret = 0;
#if IS_ENABLED(CONFIG_EXYNOS_ACPM)
	struct s2mps24_dev *s2mps24 = i2c_get_clientdata(i2c);

	mutex_lock(&s2mps24->i2c_lock);
	ret = exynos_acpm_bulk_write(acpm_mfd_node, SLAVE_CHANNEL, i2c->addr, reg, count, buf);
	mutex_unlock(&s2mps24->i2c_lock);
	if (ret) {
		pr_err("[%s] acpm ipc fail!\n", __func__);
		return ret;
	}
#endif
	return ret;
}
EXPORT_SYMBOL_GPL(s2mps24_bulk_write);

int s2mps24_update_reg(struct i2c_client *i2c, u8 reg, u8 val, u8 mask)
{
	int ret = 0;
#if IS_ENABLED(CONFIG_EXYNOS_ACPM)
	struct s2mps24_dev *s2mps24 = i2c_get_clientdata(i2c);

	mutex_lock(&s2mps24->i2c_lock);
	ret = exynos_acpm_update_reg(acpm_mfd_node, SLAVE_CHANNEL, i2c->addr, reg, val, mask);
	mutex_unlock(&s2mps24->i2c_lock);
	if (ret) {
		pr_err("[%s] acpm ipc fail!\n", __func__);
		return ret;
	}
#endif
	return ret;
}
EXPORT_SYMBOL_GPL(s2mps24_update_reg);

#if 0
int s2mps24_read_reg(struct i2c_client *i2c, u8 reg, u8 *dest)
{
	struct s2mps24_dev *s2mps24 = i2c_get_clientdata(i2c);
	int ret;

	mutex_lock(&s2mps24->i2c_lock);
	ret = i2c_smbus_read_byte_data(i2c, reg);
	mutex_unlock(&s2mps24->i2c_lock);
	if (ret < 0) {
		pr_info("%s:%s reg(0x%hhx), ret(%d)\n",
			 MFD_DEV_NAME, __func__, reg, ret);
		return ret;
	}

	ret &= 0xff;
	*dest = ret;
	return 0;
}
EXPORT_SYMBOL_GPL(s2mps24_read_reg);

int s2mps24_bulk_read(struct i2c_client *i2c, u8 reg, int count, u8 *buf)
{
	struct s2mps24_dev *s2mps24 = i2c_get_clientdata(i2c);
	int ret;

	mutex_lock(&s2mps24->i2c_lock);
	ret = i2c_smbus_read_i2c_block_data(i2c, reg, count, buf);
	mutex_unlock(&s2mps24->i2c_lock);
	if (ret < 0)
		return ret;

	return 0;
}
EXPORT_SYMBOL_GPL(s2mps24_bulk_read);

int s2mps24_read_word(struct i2c_client *i2c, u8 reg)
{
	struct s2mps24_dev *s2mps24 = i2c_get_clientdata(i2c);
	int ret;

	mutex_lock(&s2mps24->i2c_lock);
	ret = i2c_smbus_read_word_data(i2c, reg);
	mutex_unlock(&s2mps24->i2c_lock);
	if (ret < 0)
		return ret;

	return ret;
}
EXPORT_SYMBOL_GPL(s2mps24_read_word);

int s2mps24_write_reg(struct i2c_client *i2c, u8 reg, u8 value)
{
	struct s2mps24_dev *s2mps24 = i2c_get_clientdata(i2c);
	int ret;

	mutex_lock(&s2mps24->i2c_lock);
	ret = i2c_smbus_write_byte_data(i2c, reg, value);
	mutex_unlock(&s2mps24->i2c_lock);
	if (ret < 0)
		pr_info("%s:%s reg(0x%hhx), ret(%d)\n",
				MFD_DEV_NAME, __func__, reg, ret);

	return ret;
}
EXPORT_SYMBOL_GPL(s2mps24_write_reg);

int s2mps24_bulk_write(struct i2c_client *i2c, u8 reg, int count, u8 *buf)
{
	struct s2mps24_dev *s2mps24 = i2c_get_clientdata(i2c);
	int ret;

	mutex_lock(&s2mps24->i2c_lock);
	ret = i2c_smbus_write_i2c_block_data(i2c, reg, count, buf);
	mutex_unlock(&s2mps24->i2c_lock);
	if (ret < 0)
		return ret;

	return 0;
}
EXPORT_SYMBOL_GPL(s2mps24_bulk_write);

int s2mps24_write_word(struct i2c_client *i2c, u8 reg, u16 value)
{
	struct s2mps24_dev *s2mps24 = i2c_get_clientdata(i2c);
	int ret;

	mutex_lock(&s2mps24->i2c_lock);
	ret = i2c_smbus_write_word_data(i2c, reg, value);
	mutex_unlock(&s2mps24->i2c_lock);
	if (ret < 0)
		return ret;
	return 0;
}
EXPORT_SYMBOL_GPL(s2mps24_write_word);

int s2mps24_update_reg(struct i2c_client *i2c, u8 reg, u8 val, u8 mask)
{
	struct s2mps24_dev *s2mps24 = i2c_get_clientdata(i2c);
	int ret;
	u8 old_val, new_val;

	mutex_lock(&s2mps24->i2c_lock);
	ret = i2c_smbus_read_byte_data(i2c, reg);
	if (ret >= 0) {
		old_val = ret & 0xff;
		new_val = (val & mask) | (old_val & (~mask));
		ret = i2c_smbus_write_byte_data(i2c, reg, new_val);
	}
	mutex_unlock(&s2mps24->i2c_lock);
	return ret;
}
EXPORT_SYMBOL_GPL(s2mps24_update_reg);

#endif

#if IS_ENABLED(CONFIG_OF)
static int of_s2mps24_dt(struct device *dev,
			 struct s2mps24_platform_data *pdata,
			 struct s2mps24_dev *s2mps24)
{
	struct device_node *np = dev->of_node;
	int strlen;
	const char *status;

	if (!np)
		return -EINVAL;
#if IS_ENABLED(CONFIG_EXYNOS_ACPM)
	acpm_mfd_node = np;
#endif
	status = of_get_property(np, "s2mps24,wakeup", &strlen);
	if (status == NULL)
		return -EINVAL;
	if (strlen > 0) {
		if (!strcmp(status, "enabled") || !strcmp(status, "okay"))
			pdata->wakeup = true;
		else
			pdata->wakeup = false;
	}

	return 0;
}
#else
static int of_s2mps24_dt(struct device *dev,
			 struct s2mps24_platform_data *pdata)
{
	return 0;
}
#endif /* CONFIG_OF */

static int s2mps24_i2c_probe(struct i2c_client *i2c,
			     const struct i2c_device_id *dev_id)
{
	struct s2mps24_dev *s2mps24;
	struct s2mps24_platform_data *pdata = i2c->dev.platform_data;
	u8 reg_data;
	int ret = 0;

	pr_info("%s:%s start\n", MFD_DEV_NAME, __func__);

	s2mps24 = devm_kzalloc(&i2c->dev, sizeof(struct s2mps24_dev), GFP_KERNEL);
	if (!s2mps24) {
		dev_err(&i2c->dev, "%s: Failed to alloc mem for s2mps24\n",
								 __func__);
		return -ENOMEM;
	}

	if (i2c->dev.of_node) {
		pdata = devm_kzalloc(&i2c->dev,
				     sizeof(struct s2mps24_platform_data),
				     GFP_KERNEL);
		if (!pdata) {
			dev_err(&i2c->dev, "Failed to allocate memory\n");
			ret = -ENOMEM;
			goto err;
		}

		ret = of_s2mps24_dt(&i2c->dev, pdata, s2mps24);
		if (ret < 0) {
			dev_err(&i2c->dev, "Failed to get device of_node\n");
			goto err;
		}

		i2c->dev.platform_data = pdata;
	} else
		pdata = i2c->dev.platform_data;

	s2mps24->dev = &i2c->dev;
	i2c->addr = I2C_ADDR_TOP;	/* forced COMMON address For GKI-R */
	s2mps24->i2c = i2c;
	s2mps24->device_type = S2MPS24X;

	if (pdata) {
		s2mps24->pdata = pdata;
		s2mps24->wakeup = pdata->wakeup;
	} else {
		ret = -EINVAL;
		goto err;
	}
	mutex_init(&s2mps24->i2c_lock);

	i2c_set_clientdata(i2c, s2mps24);

	ret = s2mps24_read_reg(i2c, S2MPS24_PMIC_REG_PMICID, &reg_data);
	if (ret < 0) {
		dev_err(s2mps24->dev,
			"device not found on this channel (this is not an error)\n");
		ret = -ENODEV;
		goto err_w_lock;
	} else {
		/* print rev */
		s2mps24->pmic_rev = reg_data & CHIP_ID_HW_MASK;
	}

	s2mps24->pmic = i2c_new_dummy(i2c->adapter, I2C_ADDR_PMIC);
	s2mps24->debug_i2c = i2c_new_dummy(i2c->adapter, I2C_ADDR_DEBUG);
	s2mps24->adc_i2c = i2c_new_dummy(i2c->adapter, I2C_ADDR_ADC);

	i2c_set_clientdata(s2mps24->pmic, s2mps24);
	i2c_set_clientdata(s2mps24->debug_i2c, s2mps24);
	i2c_set_clientdata(s2mps24->adc_i2c, s2mps24);

	pr_info("%s device found: rev.0x%02hhx\n", __func__, s2mps24->pmic_rev);

	/* init slave PMIC notifier */
	ret = s2mps24_notifier_init(s2mps24);
	if (ret < 0)
		pr_err("%s: s2mps24_notifier_init fail\n", __func__);

	ret = mfd_add_devices(s2mps24->dev, -1, s2mps24_devs,
			      ARRAY_SIZE(s2mps24_devs), NULL, 0, NULL);
	if (ret < 0)
		goto err_mfd;

	ret = device_init_wakeup(s2mps24->dev, pdata->wakeup);
	if (ret < 0) {
		pr_err("%s: device_init_wakeup fail(%d)\n", __func__, ret);
		goto err_mfd;
	}

	return ret;

err_mfd:
	mfd_remove_devices(s2mps24->dev);
err_w_lock:
	mutex_destroy(&s2mps24->i2c_lock);
err:
	return ret;
}

static int s2mps24_i2c_remove(struct i2c_client *i2c)
{
	struct s2mps24_dev *s2mps24 = i2c_get_clientdata(i2c);

	if(s2mps24->pdata->wakeup)
		device_init_wakeup(s2mps24->dev, false);
	s2mps24_notifier_deinit(s2mps24);
	mfd_remove_devices(s2mps24->dev);
	i2c_unregister_device(s2mps24->i2c);
	mutex_destroy(&s2mps24->i2c_lock);

	return 0;
}

static const struct i2c_device_id s2mps24_i2c_id[] = {
	{ MFD_DEV_NAME, TYPE_S2MPS24 },
	{ }
};
MODULE_DEVICE_TABLE(i2c, s2mps24_i2c_id);

#if IS_ENABLED(CONFIG_OF)
static struct of_device_id s2mps24_i2c_dt_ids[] = {
	{ .compatible = "samsung,s2mps24mfd" },
	{ },
};
#endif /* CONFIG_OF */

#if IS_ENABLED(CONFIG_PM)
static int s2mps24_suspend(struct device *dev)
{
	return 0;
}

static int s2mps24_resume(struct device *dev)
{
	return 0;
}
#else
#define s2mps24_suspend	NULL
#define s2mps24_resume NULL
#endif /* CONFIG_PM */

static const struct dev_pm_ops s2mps24_pm = {
	.suspend_late = s2mps24_suspend,
	.resume_early = s2mps24_resume,
};

static struct i2c_driver s2mps24_i2c_driver = {
	.driver		= {
		.name	= MFD_DEV_NAME,
		.owner	= THIS_MODULE,
#if IS_ENABLED(CONFIG_PM)
		.pm	= &s2mps24_pm,
#endif /* CONFIG_PM */
#if IS_ENABLED(CONFIG_OF)
		.of_match_table	= s2mps24_i2c_dt_ids,
#endif /* CONFIG_OF */
		.suppress_bind_attrs = true,
	},
	.probe		= s2mps24_i2c_probe,
	.remove		= s2mps24_i2c_remove,
	.id_table	= s2mps24_i2c_id,
};

static int __init s2mps24_i2c_init(void)
{
	pr_info("%s:%s\n", MFD_DEV_NAME, __func__);
	return i2c_add_driver(&s2mps24_i2c_driver);
}
/* init early so consumer devices can complete system boot */
subsys_initcall(s2mps24_i2c_init);

static void __exit s2mps24_i2c_exit(void)
{
	i2c_del_driver(&s2mps24_i2c_driver);
}
module_exit(s2mps24_i2c_exit);

MODULE_DESCRIPTION("s2mps24 multi-function core driver");
MODULE_AUTHOR("Samsung Electronics");
MODULE_LICENSE("GPL");
