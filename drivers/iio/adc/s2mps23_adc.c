/*
 *  s2mps23_adc.c - Support for ADC in S2MPS23 PMIC
 *
 *  13 channels, 12-bit ADC
 *
 *  Copyright (c) 2020 Samsung Electronics Co., Ltd
 *              http://www.samsung.com
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 */

#include <linux/module.h>
#include <linux/platform_device.h>
#include <linux/of.h>
#include <linux/of_platform.h>
#include <linux/input.h>
#include <linux/iio/iio.h>
#include <linux/mfd/samsung/s2mps23.h>
#include <linux/mfd/samsung/s2mps23-regulator.h>
#if IS_ENABLED(CONFIG_EXYNOS_ACPM)
#include <soc/samsung/acpm_mfd.h>
#include <soc/samsung/acpm_ipc_ctrl.h>
#endif

/* GP-ADC registers */
#define ADC_INT1P		(0x00)
#define ADC_INT2P		(0x01)
#define ADC_INT1M		(0x02)
#define ADC_INT2M		(0x03)

#define GPADC_CTRL0		(0x4D)
#define GPADC_CTRL1		(0x4E)
#define GPADC_CTRL2		(0x4F)

#define GPADC_DATA1		(0x50)
#define GPADC_DATA2		(0x51)

#define GPADC_SUM_DATA1		(0x52)
#define GPADC_SUM_DATA2		(0x53)
#define GPADC_SUM_DATA3		(0x54)

#define GPADC_DBG_DATA1		(0x55)
#define GPADC_DBG_DATA2		(0x56)
#define GPADC_DBG_DATA3		(0x57)

/* GP-ADC setting */
#define ADC_MAX_CHANNELS	(13)
#define ADC_DATA_MASK		(0xFFFF)
#define INVALID_ADC_DATA	(0xFFFF)

struct s2mps23_adc {
	struct s2mps23_adc_data	*data;
	struct device		*dev;
	struct i2c_client 	*i2c;
	struct device_node	*gpadc_node;
	u32			value;
};

struct s2mps23_adc_data {
	int num_channels;

	int (*read_data)(struct s2mps23_adc *info, unsigned long chan);
};

static struct s2mps23_adc *static_info;

#if IS_ENABLED(CONFIG_EXYNOS_ACPM)
static int s2mps23_adc_read_data(struct s2mps23_adc *info, unsigned long chan)
{
	unsigned int channel_num, size, gpadc_data = 0;
	unsigned int command[4] = {0,};
	struct ipc_config config;
	struct device_node *acpm_gpadc_node = info->gpadc_node;
	int ret = 0;

	if (!acpm_ipc_request_channel(acpm_gpadc_node, NULL, &channel_num, &size)) {
		config.cmd = command;
		config.cmd[0] = 0;
		config.cmd[1] = chan;
		config.response = true;
		config.indirection = false;

		ret = acpm_ipc_send_data(channel_num, &config);
		if (ret) {
			pr_err("%s: acpm_ipc_send_data fail\n", __func__);
			goto err;
		}
		gpadc_data = (config.cmd[1] & ADC_DATA_MASK);
		if (gpadc_data == INVALID_ADC_DATA) {
			pr_err("%s: Conversion timed out!\n", __func__);
			goto err;
		}

		info->value = gpadc_data;

		acpm_ipc_release_channel(acpm_gpadc_node, channel_num);
	} else {
		pr_err("%s: ipc request_channel fail, id:%u, size:%u\n",
			__func__, channel_num, size);
		ret = -EBUSY;
		goto err;
	}

	return ret;
err:
	info->value = INVALID_ADC_DATA;
	return ret;
}
#else
static int s2mps23_adc_read_data(struct s2mps23_adc *info, unsigned long chan)
{
	pr_info("%s: Need CONFIG_EXYNOS_ACPM configs\n", __func__);

	return -EINVAL;
}
#endif

static const struct s2mps23_adc_data s2mps23_adc_info = {
	.num_channels	= ADC_MAX_CHANNELS,
	.read_data	= s2mps23_adc_read_data,
};

static const struct of_device_id s2mps23_adc_match[] = {
	{
		.compatible = "s2mps23-gpadc",
		.data = &s2mps23_adc_info,
	},
	{},
};
MODULE_DEVICE_TABLE(of, s2mps23_adc_match);

static struct s2mps23_adc_data *s2mps23_adc_get_data(void)
{
	return (struct s2mps23_adc_data *)&s2mps23_adc_info;
}

static int s2mps23_read_raw(struct iio_dev *indio_dev,
			    struct iio_chan_spec const *chan,
			    int *val, int *val2, long mask)
{
	struct s2mps23_adc *info = iio_priv(indio_dev);
	unsigned long channel = chan->address;
	int ret = 0;

	/* critical section start */
	if (mask != IIO_CHAN_INFO_RAW)
		return -EINVAL;

	if (channel >= ADC_MAX_CHANNELS || !info->data->read_data)
		return -EINVAL;

	mutex_lock(&indio_dev->mlock);

	if (info->data->read_data(info, channel))
		pr_err("%s: read_data fail\n", __func__);
	else {
		*val = info->value;
		*val2 = 0;
		ret = IIO_VAL_INT;
	}

	mutex_unlock(&indio_dev->mlock);

	return ret;
}

static const struct iio_info s2mps23_adc_iio_info = {
	.read_raw = &s2mps23_read_raw,
};

#define ADC_CHANNEL(_index, _id) {			\
	.type = IIO_VOLTAGE,				\
	.indexed = 1,					\
	.channel = _index,				\
	.address = _index,				\
	.info_mask_separate = BIT(IIO_CHAN_INFO_RAW),	\
	.datasheet_name = _id,				\
}

static const struct iio_chan_spec s2mps23_adc_iio_channels[] = {
	ADC_CHANNEL(0, "adc0"),
	ADC_CHANNEL(1, "adc1"),
	ADC_CHANNEL(2, "adc2"),
	ADC_CHANNEL(3, "adc3"),
	ADC_CHANNEL(4, "adc4"),
	ADC_CHANNEL(5, "adc5"),
	ADC_CHANNEL(6, "adc6"),
	ADC_CHANNEL(7, "adc7"),
	ADC_CHANNEL(8, "adc8"),
	ADC_CHANNEL(9, "adc9"),
	ADC_CHANNEL(10, "adc10"),
	ADC_CHANNEL(11, "adc11"),
	ADC_CHANNEL(12, "adc12"),
};

static int s2mps23_adc_remove_devices(struct device *dev, void *c)
{
	struct platform_device *pdev = to_platform_device(dev);

	platform_device_unregister(pdev);

	return 0;
}

static int s2mps23_adc_parse_dt(struct s2mps23_dev *iodev, struct s2mps23_adc *info)
{
	struct device_node *mfd_np, *gpadc_np;

	if (!iodev->dev->of_node) {
		pr_err("%s: error\n", __func__);
		goto err;
	}

	mfd_np = iodev->dev->of_node;
	if (!mfd_np) {
		pr_err("%s: could not find parent_node\n", __func__);
		goto err;
	}

	gpadc_np = of_find_node_by_name(mfd_np, "s2mps23-gpadc");
	if (!gpadc_np) {
		pr_err("%s: could not find current_node\n", __func__);
		goto err;
	}

	info->gpadc_node = gpadc_np;

	return 0;
err:
	return -1;
}

static int s2mps23_adc_probe(struct platform_device *pdev)
{
	struct s2mps23_dev *iodev = dev_get_drvdata(pdev->dev.parent);
	struct s2mps23_adc *info = NULL;
	struct iio_dev *indio_dev = NULL;
	int ret = -ENODEV;

	pr_info("%s: Start\n", __func__);

	indio_dev = devm_iio_device_alloc(&pdev->dev, sizeof(struct s2mps23_adc));
	if (!indio_dev) {
		dev_err(&pdev->dev, "failed allocating iio device\n");
		return -ENOMEM;
	}

	info = iio_priv(indio_dev);
	info->i2c = iodev->adc_i2c;
	info->dev = &pdev->dev;
	info->data = s2mps23_adc_get_data();

	ret = s2mps23_adc_parse_dt(iodev, info);
	if (ret < 0) {
		pr_err("%s: s2mps23_adc_parse_dt fail\n", __func__);
		return -ENODEV;
	}

	static_info = info;
	platform_set_drvdata(pdev, indio_dev);

	indio_dev->name = dev_name(&pdev->dev);
	indio_dev->dev.parent = &pdev->dev;
	indio_dev->dev.of_node = info->gpadc_node;
	indio_dev->info = &s2mps23_adc_iio_info;
	indio_dev->modes = INDIO_DIRECT_MODE;
	indio_dev->channels = s2mps23_adc_iio_channels;
	indio_dev->num_channels = info->data->num_channels;

	ret = iio_device_register(indio_dev);
	if (ret)
		return -ENODEV;

	ret = of_platform_populate(info->gpadc_node, s2mps23_adc_match, NULL, &indio_dev->dev);
	if (ret < 0) {
		dev_err(&pdev->dev, "%s: failed adding child nodes\n", __func__);
		goto err_of_populate;
	}

	pr_info("%s: End\n", __func__);

	return 0;

err_of_populate:
	device_for_each_child(&indio_dev->dev, NULL, s2mps23_adc_remove_devices);

	return ret;
}

static int s2mps23_adc_remove(struct platform_device *pdev)
{
	struct iio_dev *indio_dev = platform_get_drvdata(pdev);

	device_for_each_child(&indio_dev->dev, NULL, s2mps23_adc_remove_devices);
	iio_device_unregister(indio_dev);

	return 0;
}

#if IS_ENABLED(CONFIG_PM_SLEEP)
static int s2mps23_adc_suspend(struct device *dev)
{
	return 0;
}

static int s2mps23_adc_resume(struct device *dev)
{
	return 0;
}
#endif

static SIMPLE_DEV_PM_OPS(s2mps23_adc_pm_ops,
			 s2mps23_adc_suspend, s2mps23_adc_resume);

static struct platform_driver s2mps23_adc_driver = {
	.probe		= s2mps23_adc_probe,
	.remove		= s2mps23_adc_remove,
	.driver		= {
		.name	= "s2mps23-gpadc",
		.of_match_table = s2mps23_adc_match,
	},
};

module_platform_driver(s2mps23_adc_driver);

MODULE_AUTHOR("Samsung Electronics");
MODULE_DESCRIPTION("GP-ADC driver for s2mps23");
MODULE_LICENSE("GPL v2");
