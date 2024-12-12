/*
 * sec-detect-conn.c
 *
 * Copyright (C) 2017 Samsung Electronics
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
 */

#include <linux/sec_detect_conn.h>

static int gsec_detect_conn_enabled;
struct sec_det_conn_info *gpinfo;

#define SEC_CONN_PRINT(format, ...) \
	pr_info("[sec_detect_conn] " format, ##__VA_ARGS__)

#if defined(CONFIG_SEC_FACTORY)
#if defined(CONFIG_OF)
static const struct of_device_id sec_detect_conn_dt_match[] = {
	{ .compatible = "samsung,sec_detect_conn" },
	{ }
};
#endif	//CONFIG_OF

#if defined(CONFIG_PM)
static int sec_detect_conn_pm_suspend(struct device *dev)
{
	return 0;
}

static int sec_detect_conn_pm_resume(struct device *dev)
{
	return 0;
}

static const struct dev_pm_ops sec_detect_conn_pm = {
	.suspend = sec_detect_conn_pm_suspend,
	.resume = sec_detect_conn_pm_resume,
};
#endif	//CONFIG_PM

/*
 * Send uevent from irq handler.
 */
void sec_detect_send_uevent_by_num(int num,
				   struct sec_det_conn_info *pinfo,
				   int level)
{
	char *uevent_conn_str[3] = {"", "", NULL};
	char uevent_dev_str[UEVENT_CONN_MAX_DEV_NAME];
	char uevent_dev_type_str[UEVENT_CONN_MAX_DEV_NAME];

	/* Send Uevent Data */
	snprintf(uevent_dev_str, UEVENT_CONN_MAX_DEV_NAME, "CONNECTOR_NAME=%s",
		 pinfo->pdata->name[num]);
	uevent_conn_str[0] = uevent_dev_str;

	if (level == 1)
		snprintf(uevent_dev_type_str, UEVENT_CONN_MAX_DEV_NAME,
			 "CONNECTOR_TYPE=HIGH_LEVEL");
	else
		snprintf(uevent_dev_type_str, UEVENT_CONN_MAX_DEV_NAME,
			 "CONNECTOR_TYPE=LOW_LEVEL");

	uevent_conn_str[1] = uevent_dev_type_str;

	kobject_uevent_env(&pinfo->dev->kobj, KOBJ_CHANGE, uevent_conn_str);

	SEC_CONN_PRINT("send uevent pin[%d]:CONNECTOR_NAME=%s, TYPE=[%d].\n",
		       num, pinfo->pdata->name[num], level);
}

/*
 * Check gpio and send uevent.
 */
static void check_gpio_and_send_uevent(int i,
				       struct sec_det_conn_info *pinfo)
{
	if (gpio_get_value(pinfo->pdata->irq_gpio[i])) {
		SEC_CONN_PRINT("%s status changed [disconnected]\n",
			       pinfo->pdata->name[i]);
		sec_detect_send_uevent_by_num(i, pinfo, 1);
	} else {
		SEC_CONN_PRINT("%s status changed [connected]\n",
			       pinfo->pdata->name[i]);
		sec_detect_send_uevent_by_num(i, pinfo, 0);
	}
}

/*
 * Check and send uevent from irq handler.
 */
void sec_detect_send_uevent_irq(int irq,
				struct sec_det_conn_info *pinfo,
				int type)
{
	int i;

	for (i = 0; i < pinfo->pdata->gpio_total_cnt; i++) {
		if (irq == pinfo->pdata->irq_number[i]) {
			/* apply s/w debounce time */
			usleep_range(1 * 1000, 1 * 1000);
			check_gpio_and_send_uevent(i, pinfo);
		}
	}
}

/*
 * Called when the connector pin state changes.
 */
static irqreturn_t sec_detect_conn_interrupt_handler(int irq, void *handle)
{
	int type;
	struct sec_det_conn_info *pinfo = handle;

	if (gsec_detect_conn_enabled != 0) {
		SEC_CONN_PRINT("%s\n", __func__);

		type = irq_get_trigger_type(irq);
		sec_detect_send_uevent_irq(irq, pinfo, type);
	}
	return IRQ_HANDLED;
}

/*
 * Enable all gpio pin IRQ which is from Device Tree.
 */
int detect_conn_irq_enable(struct sec_det_conn_info *pinfo, bool enable,
			   int pin)
{
	int ret = 0;
	int i;

	if (!enable) {
		for (i = 0; i < pinfo->pdata->gpio_total_cnt; i++) {
			if (pinfo->irq_enabled[i]) {
				disable_irq(pinfo->pdata->irq_number[i]);
				pinfo->irq_enabled[i] = DET_CONN_GPIO_IRQ_DISABLED;
			}
		}
		return ret;
	}

	if (pin >= pinfo->pdata->gpio_total_cnt)
		return ret;

	if (pinfo->irq_enabled[pin] == DET_CONN_GPIO_IRQ_NOT_INIT) {
		ret = request_threaded_irq(pinfo->pdata->irq_number[pin], NULL,
					   sec_detect_conn_interrupt_handler,
					   pinfo->pdata->irq_type[pin] | IRQF_ONESHOT,
					   pinfo->pdata->name[pin], pinfo);

		if (ret) {
			SEC_CONN_PRINT("%s: Failed to request irq %d.\n", __func__,
				       ret);
			return ret;
		}

		SEC_CONN_PRINT("%s: Succeeded to request threaded irq %d:\n",
			       __func__, ret);
	} else if (pinfo->irq_enabled[pin] == DET_CONN_GPIO_IRQ_DISABLED) {
		enable_irq(pinfo->pdata->irq_number[pin]);
	}
	SEC_CONN_PRINT("irq_num[%d], type[%x],name[%s].\n",
		       pinfo->pdata->irq_number[pin],
		       pinfo->pdata->irq_type[pin], pinfo->pdata->name[pin]);

	pinfo->irq_enabled[pin] = DET_CONN_GPIO_IRQ_ENABLED;
	return ret;
}

/*
 * Check and send an uevent if the pin level is high.
 * And then enable gpio pin interrupt.
 */
static int one_gpio_irq_enable(int i, struct sec_det_conn_p_data *pdata,
			       const char *buf)
{
	int ret = 0;

	SEC_CONN_PRINT("%s driver enabled.\n", buf);
	gsec_detect_conn_enabled |= (1 << i);

	SEC_CONN_PRINT("gpio [%d] level %d\n", pdata->irq_gpio[i],
		       gpio_get_value(pdata->irq_gpio[i]));

	/*get level value of the gpio pin.*/
	/*if there's gpio low pin, send uevent*/
	check_gpio_and_send_uevent(i, pdata->pinfo);

	/*Enable interrupt.*/
	ret = detect_conn_irq_enable(pdata->pinfo, true, i);

	if (ret < 0)
		SEC_CONN_PRINT("%s Interrupt not enabled.\n", buf);

	return ret;
}

/*
 * Triggered when "enabled" node is set.
 * Check and send an uevent if the pin level is high.
 * And then gpio pin interrupt is enabled.
 */
static ssize_t enabled_store(struct device *dev, struct device_attribute *attr,
			     const char *buf, size_t count)
{
	struct sec_det_conn_p_data *pdata;
	struct sec_det_conn_info *pinfo;
	int ret;
	int i;
	int buf_len;
	int pin_name_len;

	if (gpinfo == 0)
		return 0;

	pinfo = gpinfo;
	pdata = pinfo->pdata;

	buf_len = strlen(buf);

	SEC_CONN_PRINT("buf = %s, buf_len = %d\n", buf, buf_len);

	/* disable irq when "enabled" value set to 0*/
	if (!strncmp(buf, "0", 1)) {
		SEC_CONN_PRINT("sec detect connector driver disable.\n");
		gsec_detect_conn_enabled = 0;

		ret = detect_conn_irq_enable(pinfo, false, 0);

		if (ret) {
			SEC_CONN_PRINT("Interrupt not disabled.\n");
			return ret;
		}
		return count;
	}
	for (i = 0; i < pdata->gpio_total_cnt; i++) {
		pin_name_len = strlen(pdata->name[i]);
		SEC_CONN_PRINT("pinName = %s\n", pdata->name[i]);
		SEC_CONN_PRINT("pin_name_len = %d\n", pin_name_len);

		if (pin_name_len == buf_len) {
			if (!strncmp(buf, pdata->name[i], buf_len)) {
				/* buf sting is equal to pdata->name[i] */
				ret = one_gpio_irq_enable(i, pdata, buf);
				if (ret < 0)
					return ret;
			}
		}

		/*
		 * For ALL_CONNECT input,
		 * enable all nodes except already enabled node.
		 */
		if (buf_len >= 11) {
			if (strncmp(buf, "ALL_CONNECT", 11)) {
				/* buf sting is not equal to ALL_CONNECT */
				continue;
			}
			/* buf sting is equal to pdata->name[i] */
			if (!(gsec_detect_conn_enabled & (1 << i))) {
				ret = one_gpio_irq_enable(i, pdata, buf);
				if (ret < 0)
					return ret;
			}
		}
	}
	return count;
}

static ssize_t enabled_show(struct device *dev, struct device_attribute *attr,
			    char *buf)
{
	return snprintf(buf, 12, "%d\n", gsec_detect_conn_enabled);
}
static DEVICE_ATTR_RW(enabled);

static ssize_t available_pins_show(struct device *dev,
				   struct device_attribute *attr,
				   char *buf)
{
	struct sec_det_conn_p_data *pdata;
	struct sec_det_conn_info *pinfo;
	char available_pins_string[1024] = {0, };
	int i;

	if (gpinfo == 0)
		return 0;

	pinfo = gpinfo;
	pdata = pinfo->pdata;

	for (i = 0; i < pdata->gpio_total_cnt; i++) {
		SEC_CONN_PRINT("pinName = %s\n", pdata->name[i]);
		strlcat(available_pins_string, pdata->name[i], 1024);
		strlcat(available_pins_string, "/", 1024);
	}
	available_pins_string[strlen(available_pins_string) - 1] = '\0';

	return snprintf(buf, 1024, "%s\n", available_pins_string);
}
static DEVICE_ATTR_RO(available_pins);

#if defined(CONFIG_OF)
/**
 * Parse the device tree and get gpio number, irq type.
 * Request gpio
 */
static int detect_conn_parse_dt(struct device *dev)
{
	struct sec_det_conn_p_data *pdata = dev->platform_data;
	struct device_node *np = dev->of_node;
#if IS_ENABLED(CONFIG_QCOM_SEC_DETECT)
	struct pinctrl *conn_pinctrl;
	struct pinctrl *pm_conn_pinctrl;
#endif
	int i;

	pdata->gpio_cnt = of_gpio_named_count(np, "sec,det_conn_gpios");

	/* if support gpio cnt less than 1 set value to 0 */
	if (pdata->gpio_cnt < 1) {
		SEC_CONN_PRINT("detect_pinctrl_init failed gpio_cnt < 1 %d.\n",
			       pdata->gpio_cnt);
		pdata->gpio_cnt = 0;
	}

	pdata->gpio_total_cnt = pdata->gpio_cnt;

#if IS_ENABLED(CONFIG_QCOM_SEC_DETECT)
	/* Setting pinctrl state to NO PULL */
	conn_pinctrl = devm_pinctrl_get_select(dev, "det_ap_connect");
	if (IS_ERR_OR_NULL(conn_pinctrl))
		SEC_CONN_PRINT("detect_pinctrl_init failed.\n");
#endif
	for (i = 0; i < pdata->gpio_cnt; i++) {
		/* get connector name*/
		of_property_read_string_index(np, "sec,det_conn_name",
					      i, &pdata->name[i]);

		/* get connector gpio number*/
		pdata->irq_gpio[i] = of_get_named_gpio(np, "sec,det_conn_gpios",
						       i);

		if (!gpio_is_valid(pdata->irq_gpio[i])) {
			dev_err(dev, "%s: Failed to get irq gpio.\n", __func__);
			return -EINVAL;
		}
		/* added gpio init feature to support interrupt configuration */
		/* set default irq type to both edge */
		pdata->irq_type[i] = IRQ_TYPE_EDGE_BOTH;

		/* filling the irq_number from this gpio.*/
		pdata->irq_number[i] = gpio_to_irq(pdata->irq_gpio[i]);

		/* print out current sec detect gpio status */
		SEC_CONN_PRINT("i = [%d] gpio [%d] level %d\n", i,
			       pdata->irq_gpio[i],
			       gpio_get_value(pdata->irq_gpio[i]));
		SEC_CONN_PRINT("i = [%d] gpio [%d] to irq [%d]\n", i,
			       pdata->irq_gpio[i],
			       pdata->irq_number[i]);
	}

#if IS_ENABLED(CONFIG_QCOM_SEC_DETECT)
	/* Setting PM gpio for QC */
	pdata->gpio_pm_cnt = of_gpio_named_count(np, "sec,det_pm_conn_gpios");

	if (pdata->gpio_pm_cnt < 1) {
		SEC_CONN_PRINT("pm detect_pinctrl_init failed pm cnt < 1. %d\n",
			       pdata->gpio_pm_cnt);
		pdata->gpio_pm_cnt = 0;
	}

	pdata->gpio_total_cnt = pdata->gpio_total_cnt + pdata->gpio_pm_cnt;

	/* Setting pinctrl state to NO PULL */
	pm_conn_pinctrl = devm_pinctrl_get_select(dev, "det_pm_connect");
	if (IS_ERR_OR_NULL(pm_conn_pinctrl))
		SEC_CONN_PRINT("pm detect_pinctrl_init failed.\n");

	SEC_CONN_PRINT("gpio_total_count = %d", pdata->gpio_total_cnt);

	for (i = pdata->gpio_cnt; i < pdata->gpio_total_cnt; i++) {
		/*Get connector name*/
		of_property_read_string_index(np, "sec,det_pm_conn_name",
					      i - pdata->gpio_cnt,
					      &pdata->name[i]);

		/*Get connector gpio number*/
		pdata->irq_gpio[i] = of_get_named_gpio(np,
						       "sec,det_pm_conn_gpios",
						       i - pdata->gpio_cnt);
		if (!gpio_is_valid(pdata->irq_gpio[i])) {
			SEC_CONN_PRINT("pm gpio[%d] invalid\n",
				       i - pdata->gpio_cnt);
			return -EINVAL;
		}
		pdata->irq_number[i] = gpio_to_irq(pdata->irq_gpio[i]);
		pdata->irq_type[i] = IRQ_TYPE_EDGE_BOTH;
		SEC_CONN_PRINT("i = [%d] gpio [%d] level %d\n", i,
			       pdata->irq_gpio[i],
			       gpio_get_value(pdata->irq_gpio[i]));
		SEC_CONN_PRINT("i = [%d] gpio [%d] to irq [%d]\n", i,
			       pdata->irq_gpio[i],
			       pdata->irq_number[i]);
	}
#endif
	return 0;
}
#endif	//CONFIG_OF

static int sec_detect_conn_probe(struct platform_device *pdev)
{
	struct sec_det_conn_p_data *pdata;
	struct sec_det_conn_info *pinfo;
	int ret = 0;

	SEC_CONN_PRINT("%s\n", __func__);

	/* First Get the GPIO pins; if it fails, we'll defer the probe. */
	if (pdev->dev.of_node) {
		pdata = devm_kzalloc(&pdev->dev,
				     sizeof(struct sec_det_conn_p_data),
				     GFP_KERNEL);

		if (!pdata) {
			dev_err(&pdev->dev, "Failed to allocate pdata.\n");
			return -ENOMEM;
		}

		pdev->dev.platform_data = pdata;

#if defined(CONFIG_OF)
		ret = detect_conn_parse_dt(&pdev->dev);
#endif
		if (ret) {
			dev_err(&pdev->dev, "Failed to parse dt data.\n");
			return ret;
		}

		pr_info("%s: parse dt done.\n", __func__);
	} else {
		pdata = pdev->dev.platform_data;
	}

	if (!pdata) {
		dev_err(&pdev->dev, "There are no platform data.\n");
		return -EINVAL;
	}

	pinfo = devm_kzalloc(&pdev->dev, sizeof(struct sec_det_conn_info),
			     GFP_KERNEL);

	if (!pinfo) {
		SEC_CONN_PRINT("pinfo : failed to allocate pinfo.\n");
		return -ENOMEM;
	}

	/* Create sys device /sys/class/sec/sec_detect_conn */
	pinfo->dev = sec_device_create(pinfo, "sec_detect_conn");

	if (unlikely(IS_ERR(pinfo->dev))) {
		pr_err("%s Failed to create device(sec_detect_conn).\n",
		       __func__);
		ret = -ENODEV;
		goto out;
	}

	/* Create sys node /sys/class/sec/sec_detect_conn/enabled */
	ret = device_create_file(pinfo->dev, &dev_attr_enabled);

	if (ret) {
		dev_err(&pdev->dev, "%s: Failed to create device file.\n",
			__func__);
		goto err_create_detect_conn_sysfs;
	}

	/* Create sys node /sys/class/sec/sec_detect_conn/available_pins */
	ret = device_create_file(pinfo->dev, &dev_attr_available_pins);

	if (ret) {
		dev_err(&pdev->dev, "%s: Failed to create device file.\n",
			__func__);
		goto err_create_detect_conn_sysfs;
	}

	/*save pinfo data to pdata to interrupt enable*/
	pdata->pinfo = pinfo;

	/*save pdata data to pinfo for enable node*/
	pinfo->pdata = pdata;

	/* save pinfo to gpinfo to enabled node*/
	gpinfo = pinfo;
	return ret;

err_create_detect_conn_sysfs:
	sec_device_destroy(pinfo->dev->devt);
out:
	gpinfo = 0;
	kfree(pinfo);
	kfree(pdata);
	return ret;
}

static int sec_detect_conn_remove(struct platform_device *pdev)
{
	return 0;
}

static struct platform_driver sec_detect_conn_driver = {
	.probe = sec_detect_conn_probe,
	.remove = sec_detect_conn_remove,
	.driver = {
		.name = "sec_detect_conn",
		.owner = THIS_MODULE,
#if defined(CONFIG_PM)
		.pm     = &sec_detect_conn_pm,
#endif
#if defined(CONFIG_OF)
		.of_match_table = of_match_ptr(sec_detect_conn_dt_match),
#endif
	},
};
#endif	//CONFIG_SEC_FACTORY

static int __init sec_detect_conn_init(void)
{
#if defined(CONFIG_SEC_FACTORY)
	SEC_CONN_PRINT("%s gogo\n", __func__);
	return platform_driver_register(&sec_detect_conn_driver);
#endif
}

static void __exit sec_detect_conn_exit(void)
{
#if defined(CONFIG_SEC_FACTORY)
	return platform_driver_unregister(&sec_detect_conn_driver);
#endif
}

module_init(sec_detect_conn_init);
module_exit(sec_detect_conn_exit);

MODULE_DESCRIPTION("Samsung Detecting Connector Driver");
MODULE_AUTHOR("Samsung Electronics");
MODULE_LICENSE("GPL");
