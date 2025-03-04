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

#include <linux/version.h>
#include <linux/module.h>
#include <linux/platform_device.h>
#include <linux/of.h>
#include <linux/slab.h>
#include <linux/kfifo.h>
#include <linux/delay.h>
#include <linux/fs.h>
#include <linux/device.h>

#include <linux/iommu.h>
#include <linux/ion.h>
#include <linux/pm_runtime.h>
#include <soc/samsung/sysevent_notif.h>

#if LINUX_VERSION_CODE <= KERNEL_VERSION(4, 19, 128)
#include <linux/exynos_iovmm.h>
#endif
#include "npu-config.h"
#include "vs4l.h"
#include "vision-dev.h"
#include "vision-ioctl.h"
#include "npu-clock.h"
#include "npu-log.h"
#include "npu-device.h"
#include "npu-debug.h"
#include "npu-protodrv.h"
#include "npu-util-memdump.h"

#ifdef CONFIG_EXYNOS_NPU_PUBLISH_NPU_BUILD_VER
#include "npu-ver-info.h"
#endif

#ifdef CONFIG_NPU_LOOPBACK
//#include "interface/loopback/npu-mailbox-mgr-mock.h"
#endif

extern struct class vision_class;
extern struct system_pwr sysPwr;
static struct notifier_block npu_nb;
extern int npu_system_save_result(struct npu_session *session, struct nw_result nw_result);
static int npu_core_sysevent_notifier_cb(struct notifier_block *this, unsigned long code, void *_cmd);


/*****************************************************************************
 *****                         wrapper function                          *****
 *****************************************************************************/
#if LINUX_VERSION_CODE >= KERNEL_VERSION(5, 4, 0)
static int npu_iommu_fault_handler(struct iommu_fault *fault, void *token)
{
	struct npu_device *device = token;

	if (test_bit(NPU_DEVICE_STATE_OPEN, &device->state)) {
		npu_err("%s() is called with mode(%pK), state(%pK))\n",
				__func__, &device->mode, &device->state);
		npu_util_dump_handle_error_k(device);
	}

	return 0;
}
static int npu_iommu_set_fault_handler(struct device *dev,
					struct npu_device *device)
{	int ret = 0;
	ret = iommu_register_device_fault_handler(dev,
				npu_iommu_fault_handler, device);

	return ret;
}
#else
static int npu_iommu_fault_handler(struct iommu_domain *domain,
		struct device *dev, unsigned long fault_addr,
		int fault_flags, void *token)
{
	struct npu_device	*device = token;

	if (test_bit(NPU_DEVICE_STATE_OPEN, &device->state)) {
		dev_err(dev, "%s() is called with "
			"fault addr(0x%lx), mode(%pK), state(%pK))\n",
			__func__, fault_addr, &device->mode, &device->state);
		npu_util_dump_handle_error_k(device);
	}

	return 0;
}

static int npu_iommu_set_fault_handler(struct device *dev,
					struct npu_device *device)
{
	int ret = 0;
	ret = iovmm_activate(dev);
	if (ret < 0) {
		probe_err("iovmm_activate is failed(%d)\n", ret);
		return ret;
	}

	iovmm_set_fault_handler(dev, npu_iommu_fault_handler, device);
	return ret;
}
#endif


static int __npu_device_start(struct npu_device *device)
{
	int ret = 0;

	if (test_bit(NPU_DEVICE_STATE_START, &device->state)) {
		npu_err("already started\n");
		ret = -EINVAL;
		goto ErrorExit;
	}

	ret = npu_system_start(&device->system);
	if (ret) {
		npu_err("fail(%d) in npu_system_start\n", ret);
		goto ErrorExit;
	}

	ret = npu_debug_start(device);
	if (ret) {
		npu_err("fail(%d) in npu_debug_start\n", ret);
		goto ErrorExit;
	}

	ret = npu_log_start(device);
	if (ret) {
		npu_err("fail(%d) in npu_log_start\n", ret);
		goto ErrorExit;
	}

	ret = proto_drv_start(device);
	if (ret) {
		npu_err("fail(%d) in proto_drv_start\n", ret);
		goto ErrorExit;
	}
//[BAE]
#if 0
	ret = mailbox_mgr_mock_start(device);
	if (ret) {
		npu_err("fail(%d) in mailbox_mgr_mock_start\n", ret);
		goto ErrorExit;
	}
#endif

	set_bit(NPU_DEVICE_STATE_START, &device->state);
ErrorExit:
	return ret;
}

static int __npu_device_stop(struct npu_device *device)
{
	int ret = 0;

	if (!test_bit(NPU_DEVICE_STATE_START, &device->state))
		goto ErrorExit;
#if 0
//#ifdef CONFIG_NPU_LOOPBACK
	//ret = mailbox_mgr_mock_stop(device);
	//if (ret)
	//	npu_err("fail(%d) in mailbox_mgr_mock_stop\n", ret);
#endif
	ret = proto_drv_stop(device);
	if (ret)
		npu_err("fail(%d) in proto_drv_stop\n", ret);

	ret = npu_log_stop(device);
	if (ret)
		npu_err("fail(%d) in npu_log_stop\n", ret);

	ret = npu_debug_stop(device);
	if (ret)
		npu_err("fail(%d) in npu_debug_stop\n", ret);

	ret = npu_system_stop(&device->system);
	if (ret)
		npu_err("fail(%d) in npu_system_stop\n", ret);

	clear_bit(NPU_DEVICE_STATE_START, &device->state);

ErrorExit:
	return ret;
}

static int __npu_device_power_on(struct npu_device *device)
{
	int ret = 0;

	ret = pm_runtime_get_sync(device->dev);
	if (ret)
		npu_err("fail(%d) in runtime resume\n", ret);

	if (!test_bit(NPU_DEVICE_ERR_STATE_EMERGENCY, &device->err_state)) {
		if (ret != -ETIMEDOUT)
			BUG_ON(ret < 0);
		else
			npu_err("EMERGENCY_RECOVERY during %s.\n", __func__);
	}
	return ret;
}

static int __npu_device_power_off(struct npu_device *device)
{
	int ret = 0;

	if (test_bit(NPU_DEVICE_ERR_STATE_EMERGENCY, &device->err_state)) {
		npu_info("EMERGENCY_RECOVERY - %ums delay insearted before power down.\n",
			configs[POWER_DOWN_DELAY_ON_EMERGENCY]);
		msleep(configs[POWER_DOWN_DELAY_ON_EMERGENCY]);
	}

	ret = pm_runtime_put_sync(device->dev);
	if (ret)
		npu_err("fail(%d) in runtime suspend\n", ret);
	BUG_ON(ret < 0);

	return ret;
}

/*
 * Called after all the open procedure is completed.
 * NPU H/W is fully working at this stage
 */
static int __npu_device_late_open(struct npu_device *device)
{
	int ret = 0;

	npu_trace("starting\n");

	npu_scheduler_late_open(device);
#ifdef CONFIG_NPU_USE_SPROFILER
	ret = npu_profile_open(&device->system);
	if (ret) {
		npu_err("fail(%d) in npu_profile_open\n", ret);
		goto err_exit;
	}

err_exit:
#endif
	if (ret)
		npu_err("error occurred: ret = %d\n", ret);
	else
		npu_trace("completed\n");
	return ret;
}

/*
 * Called before the close procedure is started.
 * NPU H/W is fully working at this stage
 */
static int __npu_device_early_close(struct npu_device *device)
{
	int ret = 0;

	npu_trace("Starting.\n");

	npu_scheduler_early_close(device);
#ifdef CONFIG_NPU_USE_SPROFILER
	ret = npu_profile_close(&device->system);
	if (ret) {
		npu_err("fail(%d) in npu_profile_close\n", ret);
		goto err_exit;
	}

err_exit:
#endif
	if (ret)
		npu_err("error occurred : ret = %d\n", ret);
	else
		npu_trace("completed\n");
	return ret;
}

#ifdef CONFIG_EXYNOS_ITMON
static int npu_itmon_notifier(struct notifier_block *nb, unsigned long action, void *nb_data)
{
	struct npu_device *device;
	struct itmon_notifier *itmon_info = nb_data;

	device = container_of(nb, struct npu_device, itmon_nb);

	if (IS_ERR_OR_NULL(itmon_info))
		return NOTIFY_DONE;

	/* TODO: For now, bus error would be treated as panic without checking status
	 * notification level should be graded as current NPU status
	 * */
	if ((itmon_info->port && !strcmp("NPU", itmon_info->port)) ||
		(itmon_info->master && !strcmp("NPU", itmon_info->master)) ||
		(itmon_info->dest && !strcmp("NPU", itmon_info->dest))) {
		return NOTIFY_BAD;
	}

	return NOTIFY_OK;
}
#endif

extern int npu_pwr_on(struct npu_system *system);
extern int npu_clk_init(struct npu_system *system);

static int npu_device_probe(struct platform_device *pdev)
{
	int ret = 0;
	struct device *dev;
	struct npu_device *device;

	BUG_ON(!pdev);

	dev = &pdev->dev;
#ifdef NPU_EXYNOS_PRINTK
	/* activate exynos printk */
	//dev_set_socdata(dev, "Exynos", "NPU");
	//npu_log.dev = dev;
#endif

#ifdef CONFIG_EXYNOS_NPU_PUBLISH_NPU_BUILD_VER
	probe_info("NPU Device driver build info :\n%s\n%s\n",
		npu_build_info, npu_git_log_str);
#endif
	device = devm_kzalloc(dev, sizeof(*device), GFP_KERNEL);
	if (!device) {
		probe_err("fail in devm_kzalloc\n");
		ret = -ENOMEM;
		goto err_exit;
	}
	device->dev = dev;

	mutex_init(&device->start_stop_lock);
	probe_info("NPU Device - init start_stop lock\n");

	ret = npu_create_config(dev);
	if (ret) {
		probe_err("failed npu_create_config\n");
		goto err_exit;
	}

	ret = npu_system_probe(&device->system, pdev);
	if (ret) {
		probe_err("fail(%d) in npu_system_probe\n", ret);
		ret = -EINVAL;
		goto err_exit;
	}

	/* Probe sub modules */
	ret = npu_debug_probe(device);
	if (ret) {
	    probe_err("fail(%d) in npu_debug_probe\n", ret);
		goto err_exit;
	}

	ret = npu_log_probe(device);
	if (ret) {
		probe_err("fail(%d) in npu_log_probe\n", ret);
		goto err_exit;
	}

	ret = npu_vertex_probe(&device->vertex, dev);
	if (ret) {
		probe_err("fail(%d) in npu_vertex_probe\n", ret);
		goto err_exit;
	}

	ret = proto_drv_probe(device);
	if (ret) {
		probe_err("fail(%d) in proto_drv_probe\n", ret);
		goto err_exit;
	}

	ret = npu_sessionmgr_probe(&device->sessionmgr);

#ifdef CONFIG_NPU_GOLDEN_MATCH
	ret = register_golden_matcher(dev);
	if (ret) {
		probe_err("fail(%d) in register_golden_matcher\n", ret);
		goto err_exit;
	}
#endif

#ifdef CONFIG_NPU_LOOPBACK
	ret = mailbox_mgr_mock_probe(device);
	if (ret) {
		probe_err("fail(%d) in mailbox_mgr_mock_probe\n", ret);
		goto err_exit;
	}
#endif
#ifdef CONFIG_NPU_USE_SPROFILER
	ret = npu_profile_probe(&device->system);
	if (ret) {
		probe_err("fail(%d) in npu_profile_probe\n", ret);
		goto err_exit;
	}
#endif
	ret = npu_iommu_set_fault_handler(dev, device);
	if (ret) {
		probe_err("fail(%d) in "
			"iommu_register_device_fault_handler\n", ret);
		goto err_exit;
	}

#ifdef CONFIG_EXYNOS_ITMON
	device->itmon_nb.notifier_call = npu_itmon_notifier;
#if 0
	itmon_notifier_chain_register(&device->itmon_nb);
#endif
#endif

	dev_set_drvdata(dev, device);

	ret = 0;
	probe_info("[EVT1] complete in %s\n", __func__);

	goto ok_exit;

err_exit:
	probe_err("error on %s ret(%d)\n", __func__, ret);
ok_exit:
	return ret;

}

int npu_device_open(struct npu_device *device)
{
	int ret = 0, ret2 = 0;

	BUG_ON(!device);

	ret = npu_debug_open(device);
	if (ret) {
		npu_err("fail(%d) in npu_debug_open\n", ret);
		goto ErrorExit;
	}

	ret = npu_log_open(device);
	if (ret) {
		npu_err("fail(%d) in npu_log_open\n", ret);
		goto err_log_open;
	}

	ret = npu_system_open(&device->system);
	if (ret) {
		npu_err("fail(%d) in npu_system_open\n", ret);
		goto err_sys_open;
	}

	ret = __npu_device_power_on(device);
	if (ret) {
		npu_err("fail(%d) in __npu_device_power_on\n", ret);
		goto err_pwr_on;
	}
	if (test_bit(NPU_DEVICE_ERR_STATE_EMERGENCY, &device->err_state)) {
		ret = -ELIBACC;
		goto err_proto_open;
	}

	ret = proto_drv_open(device);
	if (ret) {
		npu_err("fail(%d) in proto_drv_open\n", ret);
		goto err_proto_open;
	}

	ret = __npu_device_late_open(device);
	if (ret) {
		npu_err("fail(%d) in __npu_device_late_open\n", ret);
		goto err_dev_lopen;
	}
	/* clear npu_devcice_err_state to no_error for emergency recover */
	clear_bit(NPU_DEVICE_ERR_STATE_EMERGENCY, &device->err_state);

	set_bit(NPU_DEVICE_STATE_OPEN, &device->state);

	npu_info("%s():%d\n", __func__, ret);
	return ret;
err_dev_lopen:
err_proto_open:
	ret2 = proto_drv_close(device);
	if (ret2)
		npu_err("fail(%d) in proto_drv_close\n", ret2);

	npu_clk_disable_unprepare(&device->system.clks);
	ret2 = __npu_device_power_off(device);
	if (ret2)
		npu_err("fail(%d) in __npu_device_power_off\n", ret2);
err_pwr_on:
	ret2 = npu_system_close(&device->system);
	if (ret2)
		npu_err("fail(%d) in npu_system_close\n", ret2);
err_sys_open:
	ret2 = npu_log_close(device);
	if (ret2)
		npu_err("fail(%d) in npu_log_close)\n", ret2);
err_log_open:
	ret2 = npu_debug_close(device);
	if (ret2)
		npu_err("fail(%d) in npu_debug_close\n", ret2);
ErrorExit:
	npu_err("%s():ret(%d) ret2(%d)\n", __func__, ret, ret2);
	return ret;
}

int npu_system_put_req_nw(nw_cmd_e nw_cmd)
{
	int ret = 0;
	struct npu_nw req = {};

	//req.uid = session->uid,
	req.npu_req_id = 0,
	req.result_code = 0,
	req.result_value = 0;
	//req.session = session,
	req.cmd = nw_cmd,
	//req.ncp_addr = session->ncp_info.ncp_addr,
	//req.magic_tail = NPU_NW_MAGIC_TAIL,
	req.notify_func = npu_system_save_result;

	ret = npu_ncp_mgmt_put(&req);
	if (!ret) {
		npu_err("fail in npu_ncp_mgmt_put");
		return ret;
	}
	return ret;
}

int npu_system_NW_CMD_POWER_DOWN(void)
{
	int ret = 0;
	nw_cmd_e nw_cmd;

	npu_info("start power_down\n");
	nw_cmd = NPU_NW_CMD_POWER_DOWN;

	sysPwr.system_result.result_code = NPU_SYSTEM_JUST_STARTED;
	npu_system_put_req_nw(nw_cmd);
	wait_event(sysPwr.wq, sysPwr.system_result.result_code != NPU_SYSTEM_JUST_STARTED);
	if (sysPwr.system_result.result_code != NPU_ERR_NO_ERROR) {
		ret = -EINVAL;
		goto ErrorExit;
	}
	npu_info("complete in powerdown(%d)\n", ret);
ErrorExit:
	return ret;
}


int npu_device_recovery_close(struct npu_device *device)
{

	int ret = 0;

	ret += npu_system_close(&device->system);
	ret += __npu_device_power_off(device);
	ret += npu_log_close(device);
	ret += npu_debug_close(device);
	if (ret)
		goto err_coma;
	clear_bit(NPU_DEVICE_STATE_OPEN, &device->state);
	clear_bit(NPU_DEVICE_ERR_STATE_EMERGENCY, &device->err_state);

	npu_info("%s():%d\n", __func__, ret);
	return ret;
err_coma:
	BUG_ON(1);
}
int npu_device_close(struct npu_device *device)
{
	int ret = 0;

	BUG_ON(!device);

	if (!test_bit(NPU_DEVICE_STATE_OPEN, &device->state)) {
		npu_err("already closed of device\n");
		ret = -EINVAL;
		goto ErrorExit;
	}

	ret = __npu_device_early_close(device);
	if (ret)
		npu_err("fail(%d) in __npu_device_early_close\n", ret);

	if (!npu_device_is_emergency_err(device)) {
		ret = npu_system_NW_CMD_POWER_DOWN();
		if (ret) {
			npu_err("fail(%d) in __npu_powerdown\n", ret);
		}
	} else {
		npu_warn("NPU DEVICE IS EMERGENCY ERROR!\n");
	}
#if 0 // 0521_CLEAN_CODE
	ret = __npu_device_stop(device);
	if (ret)
		npu_err("fail(%d) in __npu_device_stop\n", ret);
#endif

#if 0
//#ifdef CONFIG_NPU_LOOPBACK
	ret = mailbox_mgr_mock_close(device);
	if (ret)
		npu_err("fail(%d) in mailbox_mgr_mock_close\n", ret);
#endif
	ret = proto_drv_close(device);
	if (ret)
		npu_err("fail(%d) in proto_drv_close\n", ret);

	ret = npu_system_close(&device->system);
	if (ret)
		npu_err("fail(%d) in npu_system_close\n", ret);

	ret = __npu_device_power_off(device);
	if (ret)
		npu_err("fail(%d) in __npu_device_power_off\n", ret);

	ret = npu_log_close(device);
	if (ret)
		npu_err("fail(%d) in npu_log_close)\n", ret);

	ret = npu_debug_close(device);
	if (ret)
		npu_err("fail(%d) in npu_debug_close\n", ret);

	clear_bit(NPU_DEVICE_STATE_OPEN, &device->state);

	if (test_bit(NPU_DEVICE_ERR_STATE_EMERGENCY, &device->err_state)) {
		npu_dbg("%s() : NPU_DEVICE_ERR_STATE_EMERGENCY bit was set before\n", __func__);
		clear_bit(NPU_DEVICE_ERR_STATE_EMERGENCY, &device->err_state);
		npu_dbg("%s() : NPU_DEVICE_ERR_STATE_EMERGENCY bit is clear now", __func__);
		npu_dbg(" (%d) \n", test_bit(NPU_DEVICE_ERR_STATE_EMERGENCY, &device->err_state));
	}

ErrorExit:
	npu_info("%s():%d\n", __func__, ret);
	return ret;
}

int npu_device_start(struct npu_device *device)
{
	int ret = 0;

	BUG_ON(!device);

	mutex_lock(&device->start_stop_lock);
	ret = __npu_device_start(device);
	if (ret)
		npu_err("fail(%d) in __npu_device_start\n", ret);

	mutex_unlock(&device->start_stop_lock);
	npu_info("%s():%d\n", __func__, ret);
	return ret;
}

int npu_device_stop(struct npu_device *device)
{
	int ret = 0;

	BUG_ON(!device);

	mutex_lock(&device->start_stop_lock);
	ret = __npu_device_stop(device);
	if (ret)
		npu_err("fail(%d) in __npu_device_stop\n", ret);

	mutex_unlock(&device->start_stop_lock);
	npu_info("%s():%d\n", __func__, ret);

	return ret;
}

#ifdef CONFIG_PM_SLEEP
static int npu_device_suspend(struct device *dev)
{
	npu_info("%s()\n", __func__);
	return 0;
}

static int npu_device_resume(struct device *dev)
{
	npu_info("%s()\n", __func__);
	return 0;
}
#endif

#ifdef CONFIG_PM
static int npu_device_runtime_suspend(struct device *dev)
{
	int ret = 0;
	struct npu_device *device;

	device = dev_get_drvdata(dev);

	/* clock OSC switch */
	ret = npu_core_clock_on(&device->system);
	if (ret)
		npu_err("fail(%d) in npu_core_clock_on\n", ret);

	ret = npu_system_suspend(&device->system);
	if (ret)
		npu_err("fail(%d) in npu_system_suspend\n", ret);

	npu_info("%s():%d\n", __func__, ret);
	return ret;
}

static int npu_device_runtime_resume(struct device *dev)
{
	int ret = 0;
	struct npu_device *device;

	device = dev_get_drvdata(dev);
	ret = npu_system_resume(&device->system, device->mode);
	if (ret) {
		npu_err("fail(%d) in npu_system_resume\n", ret);
		goto ErrorExit;
	}

	/* clock OSC switch */
	ret = npu_core_clock_off(&device->system);
	if (ret)
		npu_err("fail(%d) in npu_core_clock_off\n", ret);

ErrorExit:
	npu_info("%s():%d\n", __func__, ret);
	return ret;
}
#endif

static int npu_device_remove(struct platform_device *pdev)
{
	int ret = 0;
	struct device *dev;
	struct npu_device *device;

	BUG_ON(!pdev);

	dev = &pdev->dev;
	device = dev_get_drvdata(dev);

#ifdef CONFIG_NPU_USE_SPROFILER
	/* Error will not be propagated to upper layer, but just logging */
	ret = npu_profile_release();
	if (ret)
		probe_err("fail(%d) in npu_profile_release\n", ret);
#endif

#if 0
//#ifdef CONFIG_NPU_LOOPBACK
	ret = mailbox_mgr_mock_release();
	if (ret)
		npu_err("fail(%d) in mailbox_mgr_mock_release\n", ret);
#endif

	ret = proto_drv_release();
	if (ret) {
		probe_err("fail(%d) in proto_drv_release\n", ret);
	}

	ret = npu_debug_release();
	if (ret) {
		probe_err("fail(%d) in npu_debug_release\n", ret);
	}

	ret = npu_log_release(device);
	if (ret) {
		probe_err("fail(%d) in npu_log_release\n", ret);
	}

	ret = npu_system_release(&device->system, pdev);
	if (ret) {
		probe_err("fail(%d) in npu_system_release\n", ret);
	}

	probe_info("completed in %s\n", __func__);
	return 0;
}

static int npu_core_sysevent_notifier_cb(struct notifier_block *this, unsigned long code, void *_cmd)
{
	int ret = 0;
	struct notif_data *notifdata = NULL;
	notifdata = (struct notif_data *)_cmd;

	switch (code) {
	case SYSTEM_EVENT_BEFORE_SHUTDOWN:
		npu_info("%s: %s: %s\n", __func__, notifdata->pdev->name, __stringify(SYSTEM_EVENT_BEFORE_SHUTDOWN));
		break;
	case SYSTEM_EVENT_AFTER_SHUTDOWN:
		npu_info("%s: %s: %s\n", __func__, notifdata->pdev->name, __stringify(SYSTEM_EVENT_AFTER_SHUTDOWN));
		break;
	case SYSTEM_EVENT_RAMDUMP_NOTIFICATION:
		npu_info("%s: %s: %s\n", __func__, notifdata->pdev->name, __stringify(SYSTEM_EVENT_RAMDUMP_NOTIFICATION));
		break;
	case SYSTEM_EVENT_BEFORE_POWERUP:
		if (_cmd) {
			notifdata = (struct notif_data *) _cmd;
			pr_info("%s: %s: %s, crash_status:%d, enable_ramdump:%d\n", __func__, notifdata->pdev->name,
								__stringify(SYSTEM_EVENT_BEFORE_POWERUP), notifdata->crashed, notifdata->enable_ramdump);
		} else {
			npu_info("%s: %s: %s\n", __func__, notifdata->pdev->name, __stringify(SYSTEM_EVENT_BEFORE_POWERUP));
		}
		break;
	case SYSTEM_EVENT_AFTER_POWERUP:
		npu_info("%s: %s: %s\n", __func__, notifdata->pdev->name, __stringify(SYSTEM_EVENT_AFTER_POWERUP));
		break;
	default:
		break;
	}

	return ret;
}

static struct notifier_block npu_nb = {
	.notifier_call = npu_core_sysevent_notifier_cb
};

/*
int npu_device_emergency_recover(struct npu_device *device){

	npu_dbg("npu_device_emergency_recover!!\n");
	return 0;
}
*/
void npu_device_set_emergency_err(struct npu_device *device)
{
	npu_warn("Emergency flag set.\n");
	set_bit(NPU_DEVICE_ERR_STATE_EMERGENCY, &device->err_state);
}

int npu_device_is_emergency_err(struct npu_device *device)
{

	return test_bit(NPU_DEVICE_ERR_STATE_EMERGENCY, &device->err_state);
}

static const struct dev_pm_ops npu_pm_ops = {
	SET_SYSTEM_SLEEP_PM_OPS(npu_device_suspend, npu_device_resume)
	SET_RUNTIME_PM_OPS(npu_device_runtime_suspend, npu_device_runtime_resume, NULL)
};

#ifdef CONFIG_OF
static const struct of_device_id exynos_npu_match[] = {
	{
		.compatible = "samsung,exynos-npu"
	},
	{}
};
MODULE_DEVICE_TABLE(of, exynos_npu_match);
#endif

static struct platform_driver npu_driver = {
	.probe	= npu_device_probe,
	.remove = npu_device_remove,
	.driver = {
		.name	= "exynos-npu",
		.owner	= THIS_MODULE,
		.pm	= &npu_pm_ops,
		.of_match_table = of_match_ptr(exynos_npu_match),
	},
}
;
static const struct dev_pm_ops npu_core_pm_ops = {
	SET_SYSTEM_SLEEP_PM_OPS(npu_core_suspend, npu_core_resume)
	SET_RUNTIME_PM_OPS(npu_core_runtime_suspend, npu_core_runtime_resume, NULL)
};

#ifdef CONFIG_OF
const static struct of_device_id exynos_npu_core_match[] = {
	{
		.compatible = "samsung,exynos-npu-core"
	},
	{}
};
MODULE_DEVICE_TABLE(of, exynos_npu_core_match);
#endif

static struct platform_driver npu_core_driver = {
	.probe	= npu_core_probe,
	.remove = npu_core_remove,
	.driver = {
		.name	= "exynos-npu-core",
		.owner	= THIS_MODULE,
		.pm	= &npu_core_pm_ops,
		.of_match_table = of_match_ptr(exynos_npu_core_match),
	},
};

static int __init npu_device_init(void)
{
	int ret = 0;
	dev_t dev = MKDEV(VISION_MAJOR, 0);

	probe_info("Linux vision interface: v1.00\n");
	ret = register_chrdev_region(dev, VISION_NUM_DEVICES, VISION_NAME);
	if (ret < 0) {
		probe_err("videodev: unable to get major %d\n", VISION_MAJOR);
		return ret;
	}

	ret = class_register(&vision_class);
	if (ret < 0) {
		unregister_chrdev_region(dev, VISION_NUM_DEVICES);
		probe_err("video_dev: class_register failed\n");
		return -EIO;
	}

	ret = platform_driver_register(&npu_driver);
	if (ret) {
		probe_err("error(%d) in platform_driver_register\n", ret);
		goto err_exit;
	}

	ret = platform_driver_register(&npu_core_driver);
	if (ret) {
		probe_err("error(%d) in platform_driver_register\n", ret);
		goto err_exit;
	}

	sysevent_notif_register_notifier("npu_core0", &npu_nb);
	sysevent_notif_register_notifier("npu_core1", &npu_nb);
	sysevent_notif_register_notifier("npu_core2", &npu_nb);

	probe_info("success in %s\n", __func__);
	ret = 0;
	goto ok_exit;

err_exit:
	// necessary clean-up

ok_exit:
	return ret;
}

static void __exit npu_device_exit(void)
{
	dev_t dev = MKDEV(VISION_MAJOR, 0);

	platform_driver_unregister(&npu_driver);
	platform_driver_unregister(&npu_core_driver);

	class_unregister(&vision_class);
	unregister_chrdev_region(dev, VISION_NUM_DEVICES);

	probe_info("success in %s\n", __func__);
}

module_init(npu_device_init);
module_exit(npu_device_exit);

MODULE_AUTHOR("Eungjoo Kim<ej7.kim@samsung.com>");
MODULE_DESCRIPTION("Exynos NPU driver");
MODULE_LICENSE("GPL");
