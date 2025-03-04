/* sound/soc/samsung/abox/abox_vss.c
 *
 * ALSA SoC Audio Layer - Samsung Abox VSS driver
 *
 * Copyright (c) 2016 Samsung Electronics Co. Ltd.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */
/* #define DEBUG */
#include <linux/device.h>
#include <linux/debugfs.h>
#include <linux/module.h>
#include <linux/io.h>
#if IS_ENABLED(CONFIG_SHM_IPC)
#include <linux/shm_ipc.h>
#endif

#if IS_ENABLED(CONFIG_REINIT_VSS)
#include <soc/samsung/exynos-modem-ctrl.h>
#endif
#include "abox.h"
#include "abox_util.h"
#include "abox_qos.h"
#include "abox_memlog.h"

static unsigned int MAGIC_OFFSET = 0x500000;
static const int E9810_INT_FREQ = 178000;
static const int E9810_INT_FREQ_SPK = 400000;
static const unsigned int E9810_INT_ID = ABOX_CPU_GEAR_CALL_KERNEL;

static RAW_NOTIFIER_HEAD(abox_call_event_notifier);
static atomic_t abox_call_noti = ATOMIC_INIT(0);

int register_abox_call_event_notifier(struct notifier_block *nb)
{
	if (!nb)
		return -ENOENT;

	return raw_notifier_chain_register(&abox_call_event_notifier, nb);
}
EXPORT_SYMBOL(register_abox_call_event_notifier);

void abox_call_notify_event(enum abox_call_event evt, void *data)
{
	pr_debug("abox event notify (%d) ++\n", evt);
	raw_notifier_call_chain(&abox_call_event_notifier, evt, data);
	pr_debug("abox event notify (%d) --\n", evt);
}

void abox_vss_notify_status(bool start)
{
	enum abox_call_event evt;

#if IS_ENABLED(CONFIG_REINIT_VSS)
	if (!start)
		send_noti_vss_stopped();
#endif

	evt = start ? ABOX_CALL_EVENT_VSS_STARTED : ABOX_CALL_EVENT_VSS_STOPPED;
	abox_call_notify_event(evt, NULL);
}

int abox_vss_notify_call(struct device *dev, struct abox_data *data, int en)
{
	static const char cookie[] = "vss_notify_call";
	int ret = 0;

	abox_dbg(dev, "%s(%d)\n", __func__, en);

	if (en) {
		if (IS_ENABLED(CONFIG_SOC_EXYNOS9810)) {
			if (data->sound_type == SOUND_TYPE_SPEAKER)
				ret = abox_qos_request_int(dev, E9810_INT_ID,
						E9810_INT_FREQ_SPK, cookie);
			else
				ret = abox_qos_request_int(dev, E9810_INT_ID,
						E9810_INT_FREQ, cookie);
		} else if (IS_ENABLED(CONFIG_SOC_EXYNOS9830)) {
			if (atomic_cmpxchg(&abox_call_noti, 0, 1) == 0) {
				abox_info(dev, "%s en(%d)\n", __func__, en);
				abox_call_notify_event(ABOX_CALL_EVENT_ON,
						NULL);
				abox_set_system_state(data, SYSTEM_CALL, true);
			}
		}
	} else {
		if (IS_ENABLED(CONFIG_SOC_EXYNOS9810)) {
			ret = abox_qos_request_int(dev, E9810_INT_ID, 0,
					cookie);
		} else if (IS_ENABLED(CONFIG_SOC_EXYNOS9830)) {
			if (atomic_cmpxchg(&abox_call_noti, 1, 0) == 1) {
				abox_info(dev, "%s en(%d)\n", __func__, en);
				abox_call_notify_event(ABOX_CALL_EVENT_OFF,
						NULL);
				abox_set_system_state(data, SYSTEM_CALL, false);
			}
		}
	}

	return ret;
}

static int samsung_abox_vss_probe(struct platform_device *pdev)
{
	struct device *dev = &pdev->dev;
	struct device_node *np = dev->of_node;
	void __iomem *magic_addr;

	abox_dbg(dev, "%s\n", __func__);

	of_samsung_property_read_u32(dev, np, "magic-offset", &MAGIC_OFFSET);
	abox_info(dev, "magic-offset = 0x%08X\n", MAGIC_OFFSET);
	if (!IS_ERR_OR_NULL(shm_get_vss_region())) {
		magic_addr = shm_get_vss_region() + MAGIC_OFFSET;
		writel(0, magic_addr);
	}

	return 0;
}

static int samsung_abox_vss_remove(struct platform_device *pdev)
{
	struct device *dev = &pdev->dev;

	abox_dbg(dev, "%s\n", __func__);
	return 0;
}

static const struct of_device_id samsung_abox_vss_match[] = {
	{
		.compatible = "samsung,abox-vss",
	},
	{},
};
MODULE_DEVICE_TABLE(of, samsung_abox_vss_match);

struct platform_driver samsung_abox_vss_driver = {
	.probe  = samsung_abox_vss_probe,
	.remove = samsung_abox_vss_remove,
	.driver = {
		.name = "abox-vss",
		.owner = THIS_MODULE,
		.of_match_table = of_match_ptr(samsung_abox_vss_match),
	},
};
