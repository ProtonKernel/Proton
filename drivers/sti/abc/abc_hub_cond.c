/* abc_hub_cond.c
 *
 * Abnormal Behavior Catcher Hub Driver Sub Module Cond(Detecting Connection)
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

#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/device.h>
#include <linux/platform_device.h>
#include <linux/of.h>
#include <linux/of_device.h>
#include <linux/err.h>
#include <linux/kthread.h>
#include <linux/delay.h>
#include <linux/io.h>
#include <linux/irq.h>
#include <linux/interrupt.h>
#include <linux/slab.h>
#include <linux/suspend.h>
#include <linux/of_gpio.h>
#include <linux/sec_class.h>
#include <linux/sti/abc_hub.h>

#define SEC_CONN_PRINT(format, ...) \
	pr_info("[ABC_COND] " format, ##__VA_ARGS__)
#define ABCEVENT_CONN_MAX_DEV_STRING 120
#define ABC_CONN_DEBOUNCE_TIME_MS 300

/* This value is used for checking gpio irq is enabled or not. */
static int detect_conn_enabled;

/**
 * Parse the device tree and get gpio number, irq type.
 * And then request gpios.
 */
int parse_cond_data(struct device *dev,
		    struct abc_hub_platform_data *pdata,
		    struct device_node *np)
{
	int i;
	int gpio_num = 0;
#if IS_ENABLED(CONFIG_ARCH_QCOM)
	struct pinctrl *conn_pinctrl;
	struct pinctrl *pm_conn_pinctrl;
#endif

	SEC_CONN_PRINT("ABC_HUB_COND DT Parsing start.");

	pdata->cond.gpio_cnt = of_gpio_named_count(np, "sec,det_conn_gpios");
	SEC_CONN_PRINT("gpio cnt = : %d\n", pdata->cond.gpio_cnt);

	/* if support gpio cnt less than 1 set value to 0 */
	if (pdata->cond.gpio_cnt <= 0) {
		SEC_CONN_PRINT("of_gpio_named_count = 0.\n");
		pdata->cond.gpio_cnt = 0;
	}

	pdata->cond.gpio_total_cnt = pdata->cond.gpio_cnt;

#if IS_ENABLED(CONFIG_ARCH_QCOM)
	/* Setting pinctrl state to NO PULL */
	conn_pinctrl = devm_pinctrl_get_select(dev, "det_ap_connect");
	if (IS_ERR_OR_NULL(conn_pinctrl))
		SEC_CONN_PRINT("setting no pull failed.\n");
#endif
	for (i = 0; i < pdata->cond.gpio_cnt; i++) {
		/*Get connector name*/
		of_property_read_string_index(np, "sec,det_conn_name", i,
					      &pdata->cond.name[i]);

		/*Get connector gpio number*/
		gpio_num = of_get_named_gpio(np, "sec,det_conn_gpios", i);
		pdata->cond.irq_gpio[i] = gpio_num;
		SEC_CONN_PRINT("index[%d] : gpio number / name: %d / %s\n",
			       i, gpio_num, pdata->cond.name[i]);

		if (!gpio_is_valid(gpio_num)) {
			dev_err(dev, "%s: Failed to get irq gpio\n", __func__);
			return -EINVAL;
		}
		/*Filling the irq_num from this gpio.*/
		pdata->cond.irq_num[i] = gpio_to_irq(gpio_num);
		pdata->cond.irq_type[i] = IRQ_TYPE_EDGE_BOTH;
		SEC_CONN_PRINT("i = [%d], gpio level [%d] = %d\n",
			       i, gpio_num, gpio_get_value(gpio_num));
		SEC_CONN_PRINT("gpio irq gpio = [%d], irq = [%d]\n",
			       gpio_num, pdata->cond.irq_type[i]);
	}

#if IS_ENABLED(CONFIG_ARCH_QCOM)
	/* Get pm_gpio */
	pdata->cond.gpio_pm_cnt = of_gpio_named_count(np,
						      "sec,det_pm_conn_gpios");
	SEC_CONN_PRINT("pm gpio cnt = : %d\n", pdata->cond.gpio_pm_cnt);

	if (pdata->cond.gpio_pm_cnt <= 0) {
		SEC_CONN_PRINT("pm of_gpio_named_count = 0.\n");
		return 0;
	}

	/* Setting pinctrl state to NO PULL */
	pm_conn_pinctrl = devm_pinctrl_get_select(dev, "det_pm_connect");
	if (IS_ERR_OR_NULL(pm_conn_pinctrl)) {
		SEC_CONN_PRINT("pm setting no pull failed.\n");
		return -ENODEV;
	}

	pdata->cond.gpio_total_cnt = pdata->cond.gpio_total_cnt +
				     pdata->cond.gpio_pm_cnt;
	SEC_CONN_PRINT("gpio_total_count = %d", pdata->cond.gpio_total_cnt);

	for (i = pdata->cond.gpio_cnt; i < pdata->cond.gpio_total_cnt; i++) {
		/*Get connector name*/
		of_property_read_string_index(np, "sec,det_pm_conn_name",
					      i - pdata->cond.gpio_cnt,
					      &pdata->cond.name[i]);

		/* Get connector gpio number */
		gpio_num = of_get_named_gpio(np, "sec,det_pm_conn_gpios",
					     i - pdata->cond.gpio_cnt);
		pdata->cond.irq_gpio[i] = gpio_num;
		SEC_CONN_PRINT("index[%d] : pm gpio number / name: %d / %s\n",
			       i, gpio_num, pdata->cond.name[i]);
		if (!gpio_is_valid(gpio_num)) {
			SEC_CONN_PRINT("pm gpio[%d] invalid\n",
				       i - pdata->cond.gpio_cnt);
			return -EINVAL;
		}
		/*Filling the irq_num from this gpio.*/
		pdata->cond.irq_num[i] = gpio_to_irq(gpio_num);
		pdata->cond.irq_type[i] = IRQ_TYPE_EDGE_BOTH;
		SEC_CONN_PRINT("i = [%d], gpio level [%d] = %d\n",
			       i, gpio_num, gpio_get_value(gpio_num));
		SEC_CONN_PRINT("gpio irq gpio = [%d], irq = [%d]\n",
			       gpio_num, pdata->cond.irq_type[i]);
	}
#endif
	SEC_CONN_PRINT("ABC_HUB_COND DT Parsing done.");

	return 0;
}

/**
 * Send an ABC_event about given gpio pin number.
 */
void send_ABC_event_by_num(int num, struct abc_hub_info *pinfo, int level)
{
	char ABC_event_dev_str[ABCEVENT_CONN_MAX_DEV_STRING];

	/*Send ABC Event Data*/
	if (level == 1) {
		snprintf(ABC_event_dev_str, ABCEVENT_CONN_MAX_DEV_STRING,
			 "MODULE=cond@ERROR=%s", pinfo->pdata->cond.name[num]);
		abc_hub_send_event(ABC_event_dev_str);

		SEC_CONN_PRINT("send ABC pin[%d]:CONNECTOR_NAME=%s,TYPE=%d\n",
			       num, pinfo->pdata->cond.name[num], level);
	} else if (level == 0)
		SEC_CONN_PRINT("send ABC pin[%d]:CONNECTOR_NAME=%s,TYPE=%d\n",
			       num, pinfo->pdata->cond.name[num], level);
}

/**
 * Check and send ABC_event from irq handler.
 */
void check_and_send_ABC_event_irq(int irq, struct abc_hub_info *pinfo,
				  int type)
{
	int i;

	/*Send ABCEvent Data*/
	for (i = 0; i < pinfo->pdata->cond.gpio_total_cnt; i++) {
		if (irq != pinfo->pdata->cond.irq_num[i])
			continue;
		/* if edge interrupt occurs, check level twice more.*/
		/* and if it still level high send ABC_event.*/
		usleep_range(ABC_CONN_DEBOUNCE_TIME_MS * 1000, ABC_CONN_DEBOUNCE_TIME_MS * 1000);
		if (gpio_get_value(pinfo->pdata->cond.irq_gpio[i]))
			if (gpio_get_value(pinfo->pdata->cond.irq_gpio[i]))
				send_ABC_event_by_num(i, pinfo, 1);

		SEC_CONN_PRINT("%s status changed.\n",
			       pinfo->pdata->cond.name[i]);
	}
}

/**
 * This is IRQ handler function which is
 * called when the connector pin state change.
 */
static irqreturn_t detect_conn_interrupt_handler(int irq, void *handle)
{
	int type;
	struct abc_hub_info *pinfo = handle;

	if (detect_conn_enabled != 0) {
		SEC_CONN_PRINT("%s\n", __func__);

		type = irq_get_trigger_type(irq);
		check_and_send_ABC_event_irq(irq, pinfo, type);
	}

	return IRQ_HANDLED;
}

/**
 * Enable all gpio pin IRQs which is from Device Tree.
 */
int abc_detect_conn_irq_enable(struct abc_hub_info *pinfo, bool enable,
			       int pin)
{
	int ret = 0;
	int i;

	if (!enable) {
		for (i = 0; i < pinfo->pdata->cond.gpio_total_cnt; i++) {
			if (pinfo->pdata->cond.irq_enabled[i]) {
				disable_irq(pinfo->pdata->cond.irq_num[i]);
				pinfo->pdata->cond.irq_enabled[i] = DET_CONN_GPIO_IRQ_DISABLED;
			}
		}
		detect_conn_enabled = 0;
		return ret;
	}

	if (pin >= pinfo->pdata->cond.gpio_total_cnt)
		return ret;

	if (pinfo->pdata->cond.irq_enabled[pin] == DET_CONN_GPIO_IRQ_NOT_INIT) {
	 	ret = request_threaded_irq(pinfo->pdata->cond.irq_num[pin], NULL,
	 				   detect_conn_interrupt_handler,
	 				   pinfo->pdata->cond.irq_type[pin] |
	 				   IRQF_ONESHOT,
	 				   pinfo->pdata->cond.name[pin], pinfo);
	 
	 	if (ret) {
	 		SEC_CONN_PRINT("%s: Failed to request threaded irq %d.\n",
	 			       __func__, ret);
	 
	 		return ret;
	 	}
	 
	 	SEC_CONN_PRINT("%s: Succeeded to request threaded irq %d:\n",
	 		       __func__, ret);
	}
	else if (pinfo->pdata->cond.irq_enabled[pin] == DET_CONN_GPIO_IRQ_DISABLED) {
		enable_irq(pinfo->pdata->cond.irq_num[pin]);
	}

	SEC_CONN_PRINT("irq_num[%d], type[%x],name[%s].\n",
		       pinfo->pdata->cond.irq_num[pin],
		       pinfo->pdata->cond.irq_type[pin],
		       pinfo->pdata->cond.name[pin]);

	pinfo->pdata->cond.irq_enabled[pin] = DET_CONN_GPIO_IRQ_ENABLED;

	return ret;
}

/**
 * Enable abc hub cond driver.
 * Firstly, check gpio level for all pins and send abc event if value is true.
 * Secondly, enable edge irq so that it can send abc event while drivers is on.
 */
void abc_hub_cond_enable(struct device *dev, int enable)
{
	/* common sequence */
	struct abc_hub_info *pinfo = dev_get_drvdata(dev);
	struct abc_hub_platform_data *pdata = pinfo->pdata;
	int ret;
	int i;

	pdata->cond.enabled = enable;

	/* implement enable sequence */
	if (enable != ABC_HUB_ENABLED) {
		SEC_CONN_PRINT("ABC_HUB_COND_DISANABLED.");
		/*Disable interrupt.*/
		ret = abc_detect_conn_irq_enable(pinfo, false, 0);
		return;
	}
	SEC_CONN_PRINT("ABC_HUB_COND_ENABLED.");
	for (i = 0; i < pdata->cond.gpio_total_cnt; i++) {
		if ((detect_conn_enabled & (1 << i)))
			continue;
		detect_conn_enabled |= (1 << i);

		SEC_CONN_PRINT("gpio level [%d] = %d\n",
			       pdata->cond.irq_gpio[i],
			       gpio_get_value(pdata->cond.irq_gpio[i]));
		/*get level value of the gpio pin.*/
		/*if there's gpio low pin, send ABC_event*/
		if (gpio_get_value(pdata->cond.irq_gpio[i]))
			send_ABC_event_by_num(i, pinfo, 1);
		else
			send_ABC_event_by_num(i, pinfo, 0);

		/*Enable interrupt.*/
		ret = abc_detect_conn_irq_enable(pinfo, true, i);

		if (ret < 0) {
			SEC_CONN_PRINT("gpio[%d] Interrupt not enabled.\n",
				       pdata->cond.irq_gpio[i]);
			return;
		}
	}
}

int abc_hub_cond_init(struct device *dev)
{
	/* implement init sequence : return 0 if success, return -1 if fail */
	// struct abc_hub_info *pinfo = dev_get_drvdata(dev);

	return 0;
}

int abc_hub_cond_suspend(struct device *dev)
{
	/* implement suspend sequence */

	return 0;
}

int abc_hub_cond_resume(struct device *dev)
{
	/* implement resume sequence */

	return 0;
}

MODULE_DESCRIPTION("Samsung ABC Hub Sub Module1(cond) Driver");
MODULE_AUTHOR("Samsung Electronics");
MODULE_LICENSE("GPL");
