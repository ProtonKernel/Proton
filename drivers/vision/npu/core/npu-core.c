/*
 * Samsung Exynos SoC series NPU driver
 *
 * Copyright (c) 2017 Samsung Electronics Co., Ltd.
 *	http://www.samsung.com/
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 */

#include <linux/module.h>
#include <linux/platform_device.h>
#include <linux/of.h>
#include <linux/slab.h>
#include <linux/delay.h>

#include <linux/pm_runtime.h>

#include "npu-core.h"
#include "npu-log.h"
#include "npu-device.h"

static int g_npu_core_num;
static int g_npu_core_always_num;
static int g_npu_core_inuse_num;
static struct npu_core *g_npu_core_list[NPU_CORE_MAX_NUM];

int npu_core_on(struct npu_system *system)
{
	int ret = 0;
	int i;
	bool active;
	struct npu_core *core;

	BUG_ON(!system);

	for (i = g_npu_core_always_num; i < g_npu_core_inuse_num; i++) {
		core = g_npu_core_list[i];
		active = pm_runtime_active(core->dev);
		npu_info("%s %d %d\n", __func__, i, core->id);
		ret += pm_runtime_get_sync(core->dev);
		if (ret)
			npu_err("fail(%d) in pm_runtime_get_sync\n", ret);

		pm_runtime_set_active(core->dev);

		if (!active && pm_runtime_active(core->dev)) {
			npu_info("%s core\n", __func__);
			//ret = npu_soc_core_on(system, core->id);
			if (ret)
				npu_err("fail(%d) in npu_soc_core_on\n", ret);
		}
	}
	npu_dbg("%s\n", __func__);
	return ret;
}

int npu_core_off(struct npu_system *system)
{
	int ret = 0;
	int i;
	struct npu_core *core;

	BUG_ON(!system);

	for (i = g_npu_core_inuse_num - 1; i >= g_npu_core_always_num; i--) {
		core = g_npu_core_list[i];
		npu_info("%s %d %d\n", __func__, i, core->id);
		//ret = npu_soc_core_off(system, core->id);
		//if (ret)
		//	npu_err("fail(%d) in npu_soc_core_off\n", ret);

		ret += pm_runtime_put_sync(core->dev);
		if (ret)
			npu_err("fail(%d) in pm_runtime_put_sync\n", ret);

		BUG_ON(ret < 0);
	}
	npu_dbg("%s\n", __func__);
	return ret;
}

int npu_core_clock_on(struct npu_system *system)
{
	int ret = 0;
	int i;
	struct npu_core *core;

	BUG_ON(!system);

	for (i = g_npu_core_always_num; i < g_npu_core_inuse_num; i++) {
		core = g_npu_core_list[i];
		ret = npu_clk_prepare_enable(&core->clks);
		if (ret)
			npu_err("fail(%d) in npu_clk_prepare_enable\n", ret);
	}
	npu_dbg("%s\n", __func__);
	return ret;
}

int npu_core_clock_off(struct npu_system *system)
{
	int i;
	struct npu_core *core;

	BUG_ON(!system);

	for (i = g_npu_core_inuse_num - 1; i >= g_npu_core_always_num; i--) {
		core = g_npu_core_list[i];
		npu_clk_disable_unprepare(&core->clks);
	}
	npu_dbg("%s\n", __func__);
	return 0;
}

static int npu_sysevent_ramdump(int enable, const struct sysevent_desc *sysevent)
{
	/*
	 * This function is called in syseventtem_put / restart
	 *
	 * TODO: Ramdump related
	 *
	 */
	npu_info("%s: call-back function\n", __func__);

	return 0;
}

static int npu_sysevent_powerup(const struct sysevent_desc *sysevent)
{
	/*
	 * This function is called in syseventtem_get / restart
	 *
	 * TODO: Power up related
	 *
	 */
	npu_info("%s: call-back function\n", __func__);

	return 0;
}

static int npu_sysevent_shutdown(const struct sysevent_desc *sysevent, bool force_stop)
{
	/*
	 * This function is called in syseventtem_get / restart
	 *
	 * TODO: Shutdown related
	 *
	 */
	npu_info("%s: call-back function\n", __func__);

	return 0;
}

static void npu_sysevent_crash_shutdown(const struct sysevent_desc *sysevent)
{
	/*
	 * This function is called in syseventtem_get / restart
	 *
	 * TODO: Crash Shutdown related
	 *
	 */
	npu_info("%s: call-back function\n", __func__);
}

int npu_core_probe(struct platform_device *pdev)
{
	int ret = 0;
	struct device *dev;
	struct npu_core *core;

	BUG_ON(!pdev);

	dev = &pdev->dev;

	core = devm_kzalloc(dev, sizeof(*core), GFP_KERNEL);
	if (!core) {
		probe_err("fail in devm_kzalloc\n");
		ret = -ENOMEM;
		goto err_exit;
	}
	core->dev = dev;

	ret = of_property_read_u32(dev->of_node, "samsung,npucore-id", &core->id);
	if (ret) {
		probe_err("fail(%d) in of_property_read_u32\n", ret);
		goto err_exit;
	}

	ret = npu_clk_get(&core->clks, dev);
	if (ret) {
		probe_err("fail(%d) in npu_clk_get\n", ret);
		goto err_exit;
	}

	pm_runtime_enable(dev);
	dev_set_drvdata(dev, core);

	// TODO: do NOT use global variable for core list
	g_npu_core_list[g_npu_core_num++] = core;
	g_npu_core_always_num = 1;
	g_npu_core_inuse_num = 2;
	// TODO

	core->sysevent_dev = NULL;
	core->sysevent_desc.name = pdev->name;
	core->sysevent_desc.owner = THIS_MODULE;
	core->sysevent_desc.ramdump = npu_sysevent_ramdump;
	core->sysevent_desc.powerup = npu_sysevent_powerup;
	core->sysevent_desc.shutdown = npu_sysevent_shutdown;
	core->sysevent_desc.crash_shutdown = npu_sysevent_crash_shutdown;
	core->sysevent_desc.dev = &pdev->dev;
	core->sysevent_dev = sysevent_register(&core->sysevent_desc);
	if (IS_ERR(core->sysevent_dev)) {
		ret = PTR_ERR(core->sysevent_dev);
		probe_err("fail in np_device_event_probe : %d\n", ret);
		goto err_exit;
	}

	probe_info("npu core %d registerd in %s\n", core->id, __func__);

	goto ok_exit;
err_exit:
	probe_err("error on %s ret(%d)\n", __func__, ret);
ok_exit:
	return ret;

}

#ifdef CONFIG_PM_SLEEP
int npu_core_suspend(struct device *dev)
{
	npu_info("%s()\n", __func__);
	return 0;
}

int npu_core_resume(struct device *dev)
{
	npu_info("%s()\n", __func__);
	return 0;
}
#endif

#ifdef CONFIG_PM
int npu_core_runtime_suspend(struct device *dev)
{
	struct npu_core *core;

	BUG_ON(!dev);
	npu_info("%s\n", __func__);

	core = dev_get_drvdata(dev);

	npu_clk_disable_unprepare(&core->clks);

	if (core->sysevent_dev)
		sysevent_put((void *)core->sysevent_dev);

	npu_dbg("%s\n", __func__);
	return 0;
}

int npu_core_runtime_resume(struct device *dev)
{
	int ret = 0;
	struct npu_core *core;
	void *retval = NULL;
	struct sysevent_desc *desc = NULL;

	BUG_ON(!dev);
	npu_info("%s\n", __func__);

	core = dev_get_drvdata(dev);
	desc = &core->sysevent_desc;

	npu_dbg("%s clk\n", __func__);
	ret = npu_clk_prepare_enable(&core->clks);
	if (ret) {
		npu_err("fail(%d) in npu_clk_prepare_enable\n", ret);
		return ret;
	}

	if (core->sysevent_dev) {
		retval = sysevent_get(desc->name);
		if (!retval)
			npu_err("fail in sysevent_get\n");
	}
	npu_dbg("%s:%d\n", __func__, ret);
	return ret;
}
#endif

int npu_core_remove(struct platform_device *pdev)
{
	struct device *dev;
	struct npu_core *core;

	BUG_ON(!pdev);

	dev = &pdev->dev;
	core = dev_get_drvdata(dev);

	pm_runtime_disable(dev);

	npu_clk_put(&core->clks, dev);

	probe_info("completed in %s\n", __func__);
	return 0;
}

//static const struct dev_pm_ops npu_core_pm_ops = {
//	SET_SYSTEM_SLEEP_PM_OPS(npu_core_suspend, npu_core_resume)
//	SET_RUNTIME_PM_OPS(npu_core_runtime_suspend, npu_core_runtime_resume, NULL)
//};
//
//#ifdef CONFIG_OF
//const static struct of_device_id exynos_npu_core_match[] = {
//	{
//		.compatible = "samsung,exynos-npu-core"
//	},
//	{}
//};
//MODULE_DEVICE_TABLE(of, exynos_npu_core_match);
//#endif
//
//static struct platform_driver npu_core_driver = {
//	.probe	= npu_core_probe,
//	.remove = npu_core_remove,
//	.driver = {
//		.name	= "exynos-npu-core",
//		.owner	= THIS_MODULE,
//		.pm	= &npu_core_pm_ops,
//		.of_match_table = of_match_ptr(exynos_npu_core_match),
//	},
//};
//
//static int __init npu_core_init(void)
//{
//	int ret = platform_driver_register(&npu_core_driver);
//
//	if (ret) {
//		probe_err("error(%d) in platform_driver_register\n", ret);
//		goto err_exit;
//	}
//
//	probe_info("success in %s\n", __func__);
//	ret = 0;
//	goto ok_exit;
//
//err_exit:
//	// necessary clean-up
//
//ok_exit:
//	return ret;
//}
//
//static void __exit npu_core_exit(void)
//{
//	platform_driver_unregister(&npu_core_driver);
//	npu_info("success in %s\n", __func__);
//}
//
//
//late_initcall(npu_core_init);
//module_exit(npu_core_exit);
//
//
//MODULE_AUTHOR("Sungsup Lim <sungsup0.lim@samsung.com>");
//MODULE_DESCRIPTION("Exynos NPU core driver");
//MODULE_LICENSE("GPL");
