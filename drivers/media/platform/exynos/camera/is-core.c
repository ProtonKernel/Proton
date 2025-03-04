// SPDX-License-Identifier: GPL-2.0
/*
 * Samsung Exynos5 SoC series FIMC-IS driver
 *
 * exynos5 fimc-is core functions
 *
 * Copyright (c) 2011 Samsung Electronics Co., Ltd
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#include <linux/module.h>
#include <linux/delay.h>
#include <linux/kernel.h>
#include <linux/errno.h>
#include <linux/interrupt.h>
#include <linux/device.h>
#include <linux/platform_device.h>
#include <linux/slab.h>
#include <asm/cacheflush.h>
#include <asm/pgtable.h>
#include <linux/firmware.h>
#include <linux/dma-mapping.h>
#include <linux/scatterlist.h>
#include <linux/videodev2.h>
#include <linux/videodev2_exynos_camera.h>
#include <linux/vmalloc.h>
#include <linux/interrupt.h>
#include <linux/pm_qos.h>
#include <linux/bug.h>
#include <linux/v4l2-mediabus.h>
#include <linux/gpio.h>

#ifdef CONFIG_OF
#include <linux/of.h>
#include <linux/of_gpio.h>
#endif

#include "is-core.h"
#include "is-cmd.h"
#include "is-debug.h"
#include "is-hw.h"
#include "is-err.h"
#include "is-framemgr.h"
#include "is-dt.h"
#include "is-resourcemgr.h"
#include "is-dvfs.h"
#include "is-device-sensor.h"
#include "sensor/module_framework/is-device-sensor-peri.h"

#ifdef ENABLE_FAULT_HANDLER
#ifdef CONFIG_EXYNOS_IOVMM
#include <linux/exynos_iovmm.h>
#else
#include <linux/iommu.h>
#include <linux/dma-iommu.h>
#endif
#endif

struct device *is_dev = NULL;
EXPORT_SYMBOL_GPL(is_dev);

extern struct is_pm_qos_request exynos_isp_qos_int;
extern struct is_pm_qos_request exynos_isp_qos_mem;
extern struct is_pm_qos_request exynos_isp_qos_cam;

/* sysfs global variable for debug */
struct is_sysfs_debug sysfs_debug;

/* sysfs global variable for set position to actuator */
struct is_sysfs_actuator sysfs_actuator;
EXPORT_SYMBOL_GPL(sysfs_actuator);
#ifdef FIXED_SENSOR_DEBUG
struct is_sysfs_sensor sysfs_sensor;
EXPORT_SYMBOL_GPL(sysfs_sensor);
#endif

int debug_clk;
module_param(debug_clk, int, 0644);
int debug_stream;
module_param(debug_stream, int, 0644);
int debug_video;
module_param(debug_video, int, 0644);
int debug_hw;
module_param(debug_hw, int, 0644);
int debug_device;
module_param(debug_device, int, 0644);
int debug_irq;
module_param(debug_irq, int, 0644);
int debug_csi;
module_param(debug_csi, int, 0644);
int debug_sensor;
module_param(debug_sensor, int, 0644);
EXPORT_SYMBOL_GPL(debug_sensor);
int debug_time_launch;
module_param(debug_time_launch, int, 0644);
int debug_time_queue;
module_param(debug_time_queue, int, 0644);
int debug_time_shot;
module_param(debug_time_shot, int, 0644);
int debug_pdp;
module_param(debug_pdp, int, 0644);
int debug_pafstat;
module_param(debug_pafstat, int, 0644);
int debug_s2d;
module_param(debug_s2d, int, 0644);
int debug_dvfs;
module_param(debug_dvfs, int, 0644);
int debug_mem;
module_param(debug_mem, int, 0644);
int debug_lvn;
module_param(debug_lvn, int, 0644);

#ifdef MODULE
extern struct platform_driver is_sensor_driver;
extern struct spi_driver is_spi_driver;
extern struct platform_driver sensor_paf_pdp_platform_driver;
extern struct platform_driver sensor_module_driver;
extern struct i2c_driver sensor_i2c_dummy_driver;
extern struct platform_driver is_camif_dma_driver;
#endif

struct is_device_sensor *is_get_sensor_device(struct is_core *core)
{
	mutex_lock(&core->sensor_lock);

	return &core->sensor[0];
}

int is_put_sensor_device(struct is_core *core)
{
	mutex_unlock(&core->sensor_lock);

	return 0;
}

struct is_device_ischain *is_get_ischain_device(u32 instance)
{
	struct is_core *core;

	if (instance >= IS_STREAM_COUNT)
		return NULL;

	core = (struct is_core *)dev_get_drvdata(is_dev);

	return &core->ischain[instance];
}

#ifdef CONFIG_CPU_THERMAL_IPA
static int is_mif_throttling_notifier(struct notifier_block *nb,
		unsigned long val, void *v)
{
	struct is_core *core = NULL;
	struct is_device_sensor *device = NULL;
	int i;

	core = (struct is_core *)dev_get_drvdata(is_dev);
	if (!core) {
		err("core is null");
		goto exit;
	}

	for (i = 0; i < IS_SENSOR_COUNT; i++) {
		if (test_bit(IS_SENSOR_OPEN, &core->sensor[i].state)) {
			device = &core->sensor[i];
			break;
		}
	}

	if (device && !test_bit(IS_SENSOR_FRONT_DTP_STOP, &device->state))
		/* Set DTP */
		set_bit(IS_MIF_THROTTLING_STOP, &device->force_stop);
	else
		err("any sensor is not opened");

exit:
	err("MIF: cause of mif_throttling, mif_qos is [%lu]!!!\n", val);

	return NOTIFY_OK;
}

static struct notifier_block exynos_is_mif_throttling_nb = {
	.notifier_call = is_mif_throttling_notifier,
};
#endif

static int is_suspend(struct device *dev)
{
	pr_debug("FIMC_IS Suspend\n");
	pinctrl_pm_select_sleep_state(dev);
	return 0;
}

static int is_resume(struct device *dev)
{
	pr_debug("FIMC_IS Resume\n");
	pinctrl_pm_select_default_state(dev);
	return 0;
}

#if defined(SECURE_CAMERA_IRIS)
static int is_secure_iris(struct is_device_sensor *device,
	u32 type, u32 scenario, ulong smc_cmd)
{
	int ret = 0;

	if (scenario != SENSOR_SCENARIO_SECURE)
		return ret;

	switch (smc_cmd) {
	case SMC_SECCAM_PREPARE:
		ret = exynos_smc(SMC_SECCAM_PREPARE, 0, 0, 0);
		if (ret) {
			merr("[SMC] SMC_SECURE_CAMERA_PREPARE fail(%d)\n", device, ret);
		} else {
			minfo("[SMC] Call SMC_SECURE_CAMERA_PREPARE ret(%d) / smc_state(%d->%d)\n",
				device, ret, device->smc_state, IS_SENSOR_SMC_PREPARE);
			device->smc_state = IS_SENSOR_SMC_PREPARE;
		}
		break;
	case SMC_SECCAM_UNPREPARE:
		if (device->smc_state != IS_SENSOR_SMC_PREPARE)
			break;

		ret = exynos_smc(SMC_SECCAM_UNPREPARE, 0, 0, 0);
		if (ret != 0) {
			merr("[SMC] SMC_SECURE_CAMERA_UNPREPARE fail(%d)\n", device, ret);
		} else {
			minfo("[SMC] Call SMC_SECURE_CAMERA_UNPREPARE ret(%d) / smc_state(%d->%d)\n",
				device, ret, device->smc_state, IS_SENSOR_SMC_UNPREPARE);
			device->smc_state = IS_SENSOR_SMC_UNPREPARE;
		}
		break;
	default:
		break;
	}

	return ret;
}
#endif

#if defined(SECURE_CAMERA_FACE)
static int is_secure_face(struct is_core *core,
	u32 type, u32 scenario, ulong smc_cmd)
{
	int ret = 0;

	if (scenario != IS_SCENARIO_SECURE)
		return ret;

	mutex_lock(&core->secure_state_lock);
	switch (smc_cmd) {
	case SMC_SECCAM_PREPARE:
		if (core->secure_state == IS_STATE_UNSECURE) {
#if defined(SECURE_CAMERA_FACE_SEQ_CHK)
			ret = 0;
#else
			ret = exynos_smc(SMC_SECCAM_PREPARE, 0, 0, 0);
#endif
			if (ret != 0) {
				err("[SMC] SMC_SECCAM_PREPARE fail(%d)", ret);
			} else {
				info("[SMC] Call SMC_SECCAM_PREPARE ret(%d) / state(%d->%d)\n",
					ret, core->secure_state, IS_STATE_SECURED);
				core->secure_state = IS_STATE_SECURED;
			}
		}
		break;
	case SMC_SECCAM_UNPREPARE:
		if (core->secure_state == IS_STATE_SECURED) {
#if defined(SECURE_CAMERA_FACE_SEQ_CHK)
			ret = 0;
#else
			ret = exynos_smc(SMC_SECCAM_UNPREPARE, 0, 0, 0);
#endif
			if (ret != 0) {
				err("[SMC] SMC_SECCAM_UNPREPARE fail(%d)\n", ret);
			} else {
				info("[SMC] Call SMC_SECCAM_UNPREPARE ret(%d) / smc_state(%d->%d)\n",
					ret, core->secure_state, IS_STATE_UNSECURE);
				core->secure_state = IS_STATE_UNSECURE;
			}
		}
		break;
	default:
		break;
	}
	mutex_unlock(&core->secure_state_lock);

	return ret;
}
#endif

int is_secure_func(struct is_core *core,
	struct is_device_sensor *device, u32 type, u32 scenario, ulong smc_cmd)
{
	int ret = 0;

	switch (type) {
	case IS_SECURE_CAMERA_IRIS:
#if defined(SECURE_CAMERA_IRIS)
		ret = is_secure_iris(device, type, scenario, smc_cmd);
#endif
		break;
	case IS_SECURE_CAMERA_FACE:
#if defined(SECURE_CAMERA_FACE)
		ret = is_secure_face(core, type, scenario, smc_cmd);
#endif
		break;
	default:
		break;
	}

	return ret;
}
#ifdef ENABLE_FAULT_HANDLER
#define IS_PRINT_TARGET_DVA(TARGET) \
	if (leader_frame->TARGET[i]) \
		pr_err("[@][%d] %s[%d]: 0x%08X\n", \
			instance, #TARGET, i, leader_frame->TARGET[i]);

static void is_print_target_dva(struct is_frame *leader_frame, u32 instance)
{
	u32 i;

	for (i = 0; i < IS_MAX_PLANES; i++) {
		IS_PRINT_TARGET_DVA(txcTargetAddress);
		IS_PRINT_TARGET_DVA(txpTargetAddress);
		IS_PRINT_TARGET_DVA(mrgTargetAddress);
		IS_PRINT_TARGET_DVA(efdTargetAddress);
#if defined(ENABLE_LOGICAL_VIDEO_NODE)
		IS_PRINT_TARGET_DVA(txdgrTargetAddress);
#endif
#if defined(ENABLE_ORBDS)
		IS_PRINT_TARGET_DVA(txodsTargetAddress);
#endif
#if defined(ENABLE_LMEDS)
		IS_PRINT_TARGET_DVA(txldsTargetAddress);
#endif
#if defined(ENABLE_HF)
		IS_PRINT_TARGET_DVA(txhfTargetAddress);
#endif
#if defined(SOC_LME0)
		IS_PRINT_TARGET_DVA(lmesTargetAddress);
		IS_PRINT_TARGET_DVA(lmecTargetAddress);
#endif
		IS_PRINT_TARGET_DVA(ixcTargetAddress);
		IS_PRINT_TARGET_DVA(ixpTargetAddress);
		IS_PRINT_TARGET_DVA(ixtTargetAddress);
		IS_PRINT_TARGET_DVA(ixgTargetAddress);
		IS_PRINT_TARGET_DVA(ixvTargetAddress);
		IS_PRINT_TARGET_DVA(ixwTargetAddress);
		IS_PRINT_TARGET_DVA(mexcTargetAddress);
		IS_PRINT_TARGET_DVA(orbxcTargetAddress);
#if defined(SOC_MCFP)
		IS_PRINT_TARGET_DVA(ixmTargetAddress);
#endif
#if defined(SOC_YPP)
		IS_PRINT_TARGET_DVA(ixdgrTargetAddress);
		IS_PRINT_TARGET_DVA(ixrrgbTargetAddress);
		IS_PRINT_TARGET_DVA(ixnoirTargetAddress);
		IS_PRINT_TARGET_DVA(ixscmapTargetAddress);
		IS_PRINT_TARGET_DVA(ixnrdsTargetAddress);
		IS_PRINT_TARGET_DVA(ixnoiTargetAddress);
		IS_PRINT_TARGET_DVA(ixdgaTargetAddress);
		IS_PRINT_TARGET_DVA(ixsvhistTargetAddress);
		IS_PRINT_TARGET_DVA(ixhfTargetAddress);
		IS_PRINT_TARGET_DVA(ixwrgbTargetAddress);
		IS_PRINT_TARGET_DVA(ixnoirwTargetAddress);
		IS_PRINT_TARGET_DVA(ixbnrTargetAddress);
		IS_PRINT_TARGET_DVA(ypnrdsTargetAddress);
		IS_PRINT_TARGET_DVA(ypnoiTargetAddress);
		IS_PRINT_TARGET_DVA(ypdgaTargetAddress);
		IS_PRINT_TARGET_DVA(ypsvhistTargetAddress);
#endif
		IS_PRINT_TARGET_DVA(sc0TargetAddress);
		IS_PRINT_TARGET_DVA(sc1TargetAddress);
		IS_PRINT_TARGET_DVA(sc2TargetAddress);
		IS_PRINT_TARGET_DVA(sc3TargetAddress);
		IS_PRINT_TARGET_DVA(sc4TargetAddress);
		IS_PRINT_TARGET_DVA(sc5TargetAddress);
		IS_PRINT_TARGET_DVA(clxsTargetAddress);
		IS_PRINT_TARGET_DVA(clxcTargetAddress);
	}
}

void is_print_frame_dva(struct is_subdev *subdev)
{
	u32 j, k;
	struct is_framemgr *framemgr;
	struct camera2_shot *shot;

	framemgr = GET_SUBDEV_FRAMEMGR(subdev);
	if (test_bit(IS_SUBDEV_START, &subdev->state) && framemgr) {
		pr_err("[@][%d] subdev %s info\n", subdev->instance, subdev->name);
		for (j = 0; j < framemgr->num_frames; ++j) {
			for (k = 0; k < framemgr->frames[j].planes; k++) {
				msinfo(" BUF[%d][%d]: %pad, (0x%lX)\n",
					subdev, subdev, j, k,
					&framemgr->frames[j].dvaddr_buffer[k],
					framemgr->frames[j].mem_state);

				shot = framemgr->frames[j].shot;
				if (shot)
					is_print_target_dva(&framemgr->frames[j], subdev->instance);
			}
		}
	}
}

void __iomem *addr_d3_csis;
static void __is_fault_handler(struct device *dev)
{
	u32 i, sd_index;
	struct is_core *core;
	struct is_device_sensor *sensor;
	struct is_device_ischain *ischain;
	struct is_subdev *subdev;
	struct is_framemgr *framemgr;
	struct is_resourcemgr *resourcemgr;
	struct is_device_csi *csi;

	core = dev_get_drvdata(dev);
	if (!core) {
		pr_err("failed to get core\n");
		return;
	}

	resourcemgr = &core->resourcemgr;

	pr_err("[@] <Buffer information>\n");
	pr_err("[@] 1. Sensor\n");
	for (i = 0; i < IS_SENSOR_COUNT; i++) {
		sensor = &core->sensor[i];
		framemgr = GET_FRAMEMGR(sensor->vctx);
		if (test_bit(IS_SENSOR_OPEN, &sensor->state) && framemgr) {
			/* sensor */
			subdev = &sensor->group_sensor.leader;
			is_print_frame_dva(subdev);

			/* vc */
			subdev = &sensor->ssvc0;
			for (sd_index = 0; sd_index < CSI_VIRTUAL_CH_MAX; sd_index++) {
				is_print_frame_dva(subdev);
				subdev++;
			}
		}
	}

	pr_err("[@] 2. IS chain\n");
	for (i = 0; i < IS_STREAM_COUNT; i++) {
		ischain = &core->ischain[i];
		if (!test_bit(IS_ISCHAIN_OPEN, &ischain->state))
			break;
		pr_err("[@][%d] stream %d info\n", ischain->instance, i);
		/* PDP */
		subdev = &ischain->group_paf.leader;
		is_print_frame_dva(subdev);

		/* 3AA */
		subdev = &ischain->group_3aa.leader;
		is_print_frame_dva(subdev);

		/* 3AAC ~ 3AAP */
		subdev = &ischain->txc;
		for (sd_index = 0; sd_index < NUM_OF_3AA_SUBDEV; sd_index++) {
			is_print_frame_dva(subdev);
			subdev++;
		}

		/* ISP */
		subdev = &ischain->group_isp.leader;
		is_print_frame_dva(subdev);

		/* for ME */
		subdev = &ischain->mexc;
		is_print_frame_dva(subdev);

		/* for ORB */
		subdev = &ischain->orbxc;
		is_print_frame_dva(subdev);

		/* MCS */
		subdev = &ischain->group_mcs.leader;
		is_print_frame_dva(subdev);

		/* M0P ~ M5P */
		subdev = &ischain->m0p;
		for (sd_index = 0; sd_index < NUM_OF_MCS_SUBDEV; sd_index++) {
			is_print_frame_dva(subdev);
			subdev++;
		}
	}

	/* SENSOR */
	for (i = 0; i < IS_SENSOR_COUNT; i++) {
		sensor = &core->sensor[i];
		framemgr = GET_FRAMEMGR(sensor->vctx);
		if (test_bit(IS_SENSOR_OPEN, &sensor->state) && framemgr) {
			csi = (struct is_device_csi *)v4l2_get_subdevdata(sensor->subdev_csi);
			if (csi)
				csi_hw_dump_all(csi);
		}
	}

	/* ETC */
	print_all_hw_frame_count(&core->hardware);
	is_hw_fault(&core->interface);
	/* dump FW page table 1nd(~16KB), 2nd(16KB~32KB) */
	is_hw_memdump(&core->interface,
		resourcemgr->minfo.kvaddr + TTB_OFFSET, /* TTB_BASE ~ 32KB */
		resourcemgr->minfo.kvaddr + TTB_OFFSET + TTB_SIZE);
	is_hw_logdump(&core->interface);

	is_debug_s2d(true, "SYSMMU PAGE FAULT");
}

static void wq_func_print_clk(struct work_struct *data)
{
	struct is_core *core;

	core = container_of(data, struct is_core, wq_data_print_clk);

	CALL_POPS(core, print_clk);
}

#if defined(CONFIG_EXYNOS_IOVMM)
static int __attribute__((unused)) is_fault_handler(struct iommu_domain *domain,
	struct device *dev,
	unsigned long fault_addr,
	int fault_flag,
	void *token)
{
	pr_err("[@] <Pablo IS FAULT HANDLER> ++\n");
	pr_err("[@] Device virtual(0x%08X) is invalid access\n", (u32)fault_addr);

	__is_fault_handler(dev);

	pr_err("[@] <Pablo IS FAULT HANDLER> --\n");

	return -EINVAL;
}
#else
static int __attribute__((unused)) is_fault_handler(struct iommu_fault *fault,
	void *data)
{
	pr_err("[@] <Pablo IS FAULT HANDLER> ++\n");

	__is_fault_handler(is_dev);

	pr_err("[@] <Pablo IS FAULT HANDLER> --\n");

	return -EINVAL;
}
#endif
#endif /* ENABLE_FAULT_HANDLER */

static ssize_t show_en_dvfs(struct device *dev, struct device_attribute *attr,
				  char *buf)
{
	return snprintf(buf, PAGE_SIZE, "%d\n", sysfs_debug.en_dvfs);
}

static ssize_t store_en_dvfs(struct device *dev,
				 struct device_attribute *attr,
				 const char *buf, size_t count)
{
#ifdef ENABLE_DVFS
	struct is_core *core =
		(struct is_core *)dev_get_drvdata(dev);
	struct is_resourcemgr *resourcemgr;
	int i;

	FIMC_BUG(!core);

	resourcemgr = &core->resourcemgr;

	switch (buf[0]) {
	case '0':
		sysfs_debug.en_dvfs = false;
		/* update dvfs lever to max */
		mutex_lock(&resourcemgr->dvfs_ctrl.lock);
		for (i = 0; i < IS_STREAM_COUNT; i++) {
			if (test_bit(IS_ISCHAIN_OPEN, &((core->ischain[i]).state)))
				is_set_dvfs(core, &(core->ischain[i]), IS_SN_MAX);
		}
		is_dvfs_init(resourcemgr);
		resourcemgr->dvfs_ctrl.static_ctrl->cur_scenario_id = IS_SN_MAX;
		mutex_unlock(&resourcemgr->dvfs_ctrl.lock);
		break;
	case '1':
		/* It can not re-define static scenario */
		sysfs_debug.en_dvfs = true;
		break;
	default:
		pr_debug("%s: %c\n", __func__, buf[0]);
		break;
	}
#endif
	return count;
}

static ssize_t show_pattern_en(struct device *dev, struct device_attribute *attr,
				  char *buf)
{
	return snprintf(buf, PAGE_SIZE, "%u\n", sysfs_debug.pattern_en);
}

static ssize_t store_pattern_en(struct device *dev,
				 struct device_attribute *attr,
				 const char *buf, size_t count)
{
	int ret = 0;
	unsigned int cmd;
	struct is_core *core =
		(struct is_core *)platform_get_drvdata(to_platform_device(dev));

	ret = kstrtouint(buf, 0, &cmd);
	if (ret)
		return ret;

	switch (cmd) {
	case 0:
	case 1:
		if (atomic_read(&core->rsccount))
			pr_warn("%s: patter generator cannot be enabled while camera is running.\n", __func__);
		else
			sysfs_debug.pattern_en = cmd;
		break;
	default:
		pr_warn("%s: invalid parameter (%d)\n", __func__, cmd);
		break;
	}

	return count;
}

static ssize_t show_pattern_fps(struct device *dev, struct device_attribute *attr,
				  char *buf)
{
	return snprintf(buf, PAGE_SIZE, "%u\n", sysfs_debug.pattern_fps);
}

static ssize_t store_pattern_fps(struct device *dev,
				 struct device_attribute *attr,
				 const char *buf, size_t count)
{
	int ret = 0;
	unsigned int cmd;

	ret = kstrtouint(buf, 0, &cmd);
	if (ret)
		return ret;

	sysfs_debug.pattern_fps = cmd;

	return count;
}

static ssize_t show_hal_debug_mode(struct device *dev, struct device_attribute *attr,
				  char *buf)
{
	return snprintf(buf, PAGE_SIZE, "0x%lx\n", sysfs_debug.hal_debug_mode);
}

static ssize_t store_hal_debug_mode(struct device *dev,
				 struct device_attribute *attr,
				 const char *buf, size_t count)
{
	int ret;
	unsigned long long debug_mode = 0;

	ret = kstrtoull(buf, 16 /* hexa */, &debug_mode);
	if (ret < 0) {
		pr_err("%s, %s, failed for debug_mode:%llu, ret:%d", __func__, buf, debug_mode, ret);
		return 0;
	}

	sysfs_debug.hal_debug_mode = (unsigned long)debug_mode;

	return count;
}

static ssize_t show_hal_debug_delay(struct device *dev, struct device_attribute *attr,
				  char *buf)
{
	return snprintf(buf, PAGE_SIZE, "%u ms\n", sysfs_debug.hal_debug_delay);
}

static ssize_t store_hal_debug_delay(struct device *dev,
				 struct device_attribute *attr,
				 const char *buf, size_t count)
{
	int ret;

	ret = kstrtouint(buf, 10, &sysfs_debug.hal_debug_delay);
	if (ret < 0) {
		pr_err("%s, %s, failed for debug_delay:%u, ret:%d", __func__, buf, sysfs_debug.hal_debug_delay, ret);
		return 0;
	}

	return count;
}

#ifdef ENABLE_DBG_STATE
static ssize_t show_debug_state(struct device *dev, struct device_attribute *attr,
				  char *buf)
{
	struct is_core *core =
		(struct is_core *)dev_get_drvdata(dev);
	struct is_resourcemgr *resourcemgr;

	FIMC_BUG(!core);

	resourcemgr = &core->resourcemgr;

	return snprintf(buf, PAGE_SIZE, "%d\n", resourcemgr->hal_version);
}

static ssize_t store_debug_state(struct device *dev,
				 struct device_attribute *attr,
				 const char *buf, size_t count)
{
	struct is_core *core =
		(struct is_core *)dev_get_drvdata(dev);
	struct is_resourcemgr *resourcemgr;

	FIMC_BUG(!core);

	resourcemgr = &core->resourcemgr;

	switch (buf[0]) {
	case '0':
		break;
	case '1':
		break;
	case '7':
		break;
	default:
		pr_debug("%s: %c\n", __func__, buf[0]);
		break;
	}

	return count;
}
#endif

static ssize_t store_actuator_init_step(struct device *dev,
		struct device_attribute *attr,
		const char *buf, size_t count)
{
	int ret_count;
	unsigned int init_step;

	ret_count = sscanf(buf, "%u", &init_step);
	if (ret_count != 1)
		return -EINVAL;

	switch (init_step) {
		/* case number is step of set position */
		case 1:
		case 2:
		case 3:
		case 4:
		case 5:
			sysfs_actuator.init_step = init_step;
			break;
		/* default actuator setting (2step default) */
		default:
			sysfs_actuator.init_step = 0;
			break;
	}

	return count;
}

static ssize_t store_actuator_init_positions(struct device *dev,
		struct device_attribute *attr,
		const char *buf, size_t count)
{
	int i;
	int ret_count;
	int init_positions[INIT_MAX_SETTING];

	ret_count = sscanf(buf, "%d %d %d %d %d", &init_positions[0], &init_positions[1],
						&init_positions[2], &init_positions[3], &init_positions[4]);
	if (ret_count > INIT_MAX_SETTING)
		return -EINVAL;

	for (i = 0; i < ret_count; i++) {
		if (init_positions[i] >= 0 && init_positions[i] < 1024)
			sysfs_actuator.init_positions[i] = init_positions[i];
		else
			sysfs_actuator.init_positions[i] = 0;
	}

	return count;
}

static ssize_t store_actuator_init_delays(struct device *dev,
		struct device_attribute *attr,
		const char *buf, size_t count)
{
	int ret_count;
	int i;
	int init_delays[INIT_MAX_SETTING];

	ret_count = sscanf(buf, "%d %d %d %d %d", &init_delays[0], &init_delays[1],
							&init_delays[2], &init_delays[3], &init_delays[4]);
	if (ret_count > INIT_MAX_SETTING)
		return -EINVAL;

	for (i = 0; i < ret_count; i++) {
		if (init_delays[i] >= 0)
			sysfs_actuator.init_delays[i] = init_delays[i];
		else
			sysfs_actuator.init_delays[i] = 0;
	}

	return count;
}

static ssize_t store_actuator_enable_fixed(struct device *dev,
		struct device_attribute *attr,
		const char *buf, size_t count)
{
	if (buf[0] == '1')
		sysfs_actuator.enable_fixed = true;
	else
		sysfs_actuator.enable_fixed = false;

	return count;
}

static ssize_t store_actuator_fixed_position(struct device *dev,
		struct device_attribute *attr,
		const char *buf, size_t count)
{
	int ret_count;
	int fixed_position;

	ret_count = sscanf(buf, "%d", &fixed_position);

	if (ret_count != 1)
		return -EINVAL;

	if (fixed_position < 0 || fixed_position > 1024)
		return -EINVAL;

	sysfs_actuator.fixed_position = fixed_position;

	return count;
}

static ssize_t show_actuator_init_step(struct device *dev, struct device_attribute *attr,
		char *buf)
{
	return snprintf(buf, PAGE_SIZE, "%d\n", sysfs_actuator.init_step);
}

static ssize_t show_actuator_init_positions(struct device *dev, struct device_attribute *attr,
		char *buf)
{
	return snprintf(buf, PAGE_SIZE, "%d %d %d %d %d\n", sysfs_actuator.init_positions[0],
						sysfs_actuator.init_positions[1], sysfs_actuator.init_positions[2],
						sysfs_actuator.init_positions[3], sysfs_actuator.init_positions[4]);
}

static ssize_t show_actuator_init_delays(struct device *dev, struct device_attribute *attr,
		char *buf)
{
	return snprintf(buf, PAGE_SIZE, "%d %d %d %d %d\n", sysfs_actuator.init_delays[0],
							sysfs_actuator.init_delays[1], sysfs_actuator.init_delays[2],
							sysfs_actuator.init_delays[3], sysfs_actuator.init_delays[4]);
}

static ssize_t show_actuator_enable_fixed(struct device *dev, struct device_attribute *attr,
		char *buf)
{
	return snprintf(buf, PAGE_SIZE, "%d\n", sysfs_actuator.enable_fixed);
}

static ssize_t show_actuator_fixed_position(struct device *dev, struct device_attribute *attr,
		char *buf)
{
	return snprintf(buf, PAGE_SIZE, "%d\n", sysfs_actuator.fixed_position);
}

#ifdef FIXED_SENSOR_DEBUG
static ssize_t show_fixed_sensor_val(struct device *dev, struct device_attribute *attr,
				  char *buf)
{
	return snprintf(buf, PAGE_SIZE, "f_duration(%d) ex(%d %d) a_gain(%d %d) d_gain(%d %d)\n",
			sysfs_sensor.frame_duration,
			sysfs_sensor.long_exposure_time,
			sysfs_sensor.short_exposure_time,
			sysfs_sensor.long_analog_gain,
			sysfs_sensor.short_analog_gain,
			sysfs_sensor.long_digital_gain,
			sysfs_sensor.short_digital_gain);
}

static ssize_t store_fixed_sensor_val(struct device *dev,
				 struct device_attribute *attr,
				 const char *buf, size_t count)
{
	int ret_count;
	int input_val[7];

	ret_count = sscanf(buf, "%d %d %d %d %d %d %d", &input_val[0], &input_val[1],
							&input_val[2], &input_val[3],
							&input_val[4], &input_val[5], &input_val[6]);
	if (ret_count != 7) {
		probe_err("%s: count should be 7 but %d \n", __func__, ret_count);
		return -EINVAL;
	}

	sysfs_sensor.frame_duration = input_val[0];
	sysfs_sensor.long_exposure_time = input_val[1];
	sysfs_sensor.short_exposure_time = input_val[2];
	sysfs_sensor.long_analog_gain = input_val[3];
	sysfs_sensor.short_analog_gain = input_val[4];
	sysfs_sensor.long_digital_gain = input_val[5];
	sysfs_sensor.short_digital_gain = input_val[6];

	return count;



}

static ssize_t show_fixed_sensor_fps(struct device *dev, struct device_attribute *attr,
				  char *buf)
{
	return snprintf(buf, PAGE_SIZE, "current_fps(%d), max_fps(%d)\n",
			sysfs_sensor.set_fps,
			sysfs_sensor.max_fps);
}

static ssize_t store_fixed_sensor_fps(struct device *dev,
				 struct device_attribute *attr,
				 const char *buf, size_t count)
{
	int ret;
	int input_val;

	ret = kstrtoint(buf, 0, &input_val);
	if (ret) {
		err("Fail to conversion on success(%d)\n", ret);
		return ret;
	}

	if (sysfs_sensor.is_fps_en) {
		if (input_val > sysfs_sensor.max_fps) {
			warn("Over max fps(%d), setting current fps to max(%d -> %d)\n",
					FIXED_MAX_FPS_VALUE, input_val, FIXED_MAX_FPS_VALUE);

			input_val = sysfs_sensor.max_fps;
		} else if (input_val < FIXED_MIN_FPS_VALUE) {
			warn("Lower than enable to sensor fps setting, setting to (%d -> %d)\n",
					input_val, FIXED_MIN_FPS_VALUE);

			input_val = FIXED_MIN_FPS_VALUE;
		}

		sysfs_sensor.set_fps = input_val;
		sysfs_sensor.frame_duration = FPS_TO_DURATION_US(input_val);
	} else {
		warn("Not enable a is_fps_en, has to first setting is_fps_en\n");
	}

	return count;
}

static ssize_t show_en_fixed_sensor_fps(struct device *dev, struct device_attribute *attr,
				  char *buf)
{
	if (sysfs_sensor.is_fps_en)
		return snprintf(buf, PAGE_SIZE, "%s\n", "enabled");
	else
		return snprintf(buf, PAGE_SIZE, "%s\n", "disabled");
}

static ssize_t store_en_fixed_sensor_fps(struct device *dev,
				 struct device_attribute *attr,
				 const char *buf, size_t count)
{
	if (buf[0] == '1') {
		sysfs_sensor.is_fps_en = true;
	} else {
		sysfs_sensor.is_fps_en = false;
		sysfs_sensor.frame_duration = FIXED_MAX_FPS_VALUE;
	}

	return count;
}

static ssize_t show_en_fixed_sensor(struct device *dev, struct device_attribute *attr,
				  char *buf)
{
	if (sysfs_sensor.is_en)
		return snprintf(buf, PAGE_SIZE, "%s\n", "enabled");
	else
		return snprintf(buf, PAGE_SIZE, "%s\n", "disabled");
}

static ssize_t store_en_fixed_sensor(struct device *dev,
				 struct device_attribute *attr,
				 const char *buf, size_t count)
{
	if (buf[0] == '1')
		sysfs_sensor.is_en = true;
	else
		sysfs_sensor.is_en = false;

	return count;
}
#endif

static DEVICE_ATTR(en_dvfs, 0644, show_en_dvfs, store_en_dvfs);
static DEVICE_ATTR(pattern_en, 0644, show_pattern_en, store_pattern_en);
static DEVICE_ATTR(pattern_fps, 0644, show_pattern_fps, store_pattern_fps);
static DEVICE_ATTR(hal_debug_mode, 0644, show_hal_debug_mode, store_hal_debug_mode);
static DEVICE_ATTR(hal_debug_delay, 0644, show_hal_debug_delay, store_hal_debug_delay);

#ifdef ENABLE_DBG_STATE
static DEVICE_ATTR(en_debug_state, 0644, show_debug_state, store_debug_state);
#endif

static DEVICE_ATTR(init_step, 0644, show_actuator_init_step, store_actuator_init_step);
static DEVICE_ATTR(init_positions, 0644, show_actuator_init_positions, store_actuator_init_positions);
static DEVICE_ATTR(init_delays, 0644, show_actuator_init_delays, store_actuator_init_delays);
static DEVICE_ATTR(enable_fixed, 0644, show_actuator_enable_fixed, store_actuator_enable_fixed);
static DEVICE_ATTR(fixed_position, 0644, show_actuator_fixed_position, store_actuator_fixed_position);

#ifdef FIXED_SENSOR_DEBUG
static DEVICE_ATTR(fixed_sensor_val, 0644, show_fixed_sensor_val, store_fixed_sensor_val);
static DEVICE_ATTR(fixed_sensor_fps, 0644, show_fixed_sensor_fps, store_fixed_sensor_fps);
static DEVICE_ATTR(en_fixed_sensor_fps, 0644, show_en_fixed_sensor_fps, store_en_fixed_sensor_fps);
static DEVICE_ATTR(en_fixed_sensor, 0644, show_en_fixed_sensor, store_en_fixed_sensor);
#endif

static struct attribute *is_debug_entries[] = {
	&dev_attr_en_dvfs.attr,
	&dev_attr_pattern_en.attr,
	&dev_attr_pattern_fps.attr,
	&dev_attr_hal_debug_mode.attr,
	&dev_attr_hal_debug_delay.attr,
#ifdef ENABLE_DBG_STATE
	&dev_attr_en_debug_state.attr,
#endif
	&dev_attr_init_step.attr,
	&dev_attr_init_positions.attr,
	&dev_attr_init_delays.attr,
	&dev_attr_enable_fixed.attr,
	&dev_attr_fixed_position.attr,
#ifdef FIXED_SENSOR_DEBUG
	&dev_attr_fixed_sensor_val.attr,
	&dev_attr_fixed_sensor_fps.attr,
	&dev_attr_en_fixed_sensor_fps.attr,
	&dev_attr_en_fixed_sensor.attr,
#endif
	NULL,
};
static struct attribute_group is_debug_attr_group = {
	.name	= "debug",
	.attrs	= is_debug_entries,
};

#if IS_ENABLED(CONFIG_EXYNOS_SYSTEM_EVENT)
extern struct notifier_block is_nb;

static int is_sysevent_powerup(const struct sysevent_desc *desc)
{
	info("sysevent powerup callback\n");

	return 0;
}

static int is_sysevent_shutdown(const struct sysevent_desc *desc, bool force_stop)
{
	info("sysevent shutdown callback\n");

	return 0;
}

static void is_sysevent_crash_shutdown(const struct sysevent_desc *desc)
{
	info("sysevent crash callback\n");
}

static int __is_sysevent_desc_init(struct platform_device *pdev, struct is_core *core)
{
	int ret = 0;

	core->sysevent_desc.name = "CAM";
	core->sysevent_desc.owner = THIS_MODULE;
	core->sysevent_desc.shutdown = is_sysevent_shutdown;
	core->sysevent_desc.powerup = is_sysevent_powerup;
	core->sysevent_desc.crash_shutdown = is_sysevent_crash_shutdown;
	core->sysevent_desc.dev = &pdev->dev;

	core->sysevent_dev = sysevent_register(&core->sysevent_desc);
	if (IS_ERR(core->sysevent_dev)) {
		ret = PTR_ERR(core->sysevent_dev);
		err("%s: sysevent_register failed :%d\n", pdev->name, ret);
	} else {
		info("%s: sysevent_register success\n", pdev->name);
	}

	return ret;
}
#endif

static int is_probe(struct platform_device *pdev)
{
	struct exynos_platform_is *pdata;
	struct is_core *core;
	int ret = -ENODEV;
	int i;
	u32 stream;
	struct pinctrl_state *s;
#if defined(SECURE_CAMERA_IRIS) || defined(SECURE_CAMERA_FACE)
	ulong mem_info_addr, mem_info_size;
#endif

	probe_info("%s:start(%ld, %ld)\n", __func__,
		sizeof(struct is_core), sizeof(struct is_video_ctx));

	core = kzalloc(sizeof(struct is_core), GFP_KERNEL);
	if (!core) {
		probe_err("core is NULL");
		return -ENOMEM;
	}

	is_dev = &pdev->dev;
	dev_set_drvdata(is_dev, core);

	pdata = dev_get_platdata(&pdev->dev);
	if (!pdata) {
#ifdef CONFIG_OF
		ret = is_parse_dt(pdev);
		if (ret) {
			err("is_parse_dt is fail(%d)", ret);
			return ret;
		}

		pdata = dev_get_platdata(&pdev->dev);
#else
		BUG();
#endif
	}

	core->pdev = pdev;
	core->pdata = pdata;
	core->current_position = SENSOR_POSITION_REAR;
	device_init_wakeup(&pdev->dev, true);

	INIT_WORK(&core->wq_data_print_clk, wq_func_print_clk);

	/* for mideaserver force down */
	atomic_set(&core->rsccount, 0);

	core->regs = NULL;

	if (pdata) {
		ret = pdata->clk_get(&pdev->dev);
		if (ret) {
			probe_err("clk_get is fail(%d)", ret);
			goto p_err3;
		}
	}

	dma_set_mask(&pdev->dev, DMA_BIT_MASK(32));

	ret = is_debug_probe();
	if (ret) {
		probe_err("is_debug_probe is fail(%d)", ret);
		goto p_err3;
	}

	ret = is_mem_init(&core->resourcemgr.mem, core->pdev);
	if (ret) {
		probe_err("is_mem_probe is fail(%d)", ret);
		goto p_err3;
	}

	ret = is_resourcemgr_probe(&core->resourcemgr, core, core->pdev);
	if (ret) {
		probe_err("is_resourcemgr_probe is fail(%d)", ret);
		goto p_err3;
	}

	ret = is_interface_probe(&core->interface,
		&core->resourcemgr.minfo,
		(ulong)core->regs,
		core->irq,
		core);
	if (ret) {
		probe_err("is_interface_probe is fail(%d)", ret);
		goto p_err3;
	}

	ret = is_vender_probe(&core->vender);
	if (ret) {
		probe_err("is_vender_probe is fail(%d)", ret);
		goto p_err3;
	}

	/* group initialization */
	ret = is_groupmgr_probe(pdev, &core->groupmgr);
	if (ret) {
		probe_err("is_groupmgr_probe is fail(%d)", ret);
		goto p_err3;
	}

	ret = is_devicemgr_probe(&core->devicemgr);
	if (ret) {
		probe_err("is_devicemgr_probe is fail(%d)", ret);
		goto p_err3;
	}

	for (stream = 0; stream < IS_STREAM_COUNT; ++stream) {
		ret = is_ischain_probe(&core->ischain[stream],
			&core->interface,
			&core->resourcemgr,
			&core->groupmgr,
			&core->devicemgr,
			&core->resourcemgr.mem,
			core->pdev,
			stream);
		if (ret) {
			probe_err("is_ischain_probe(%d) is fail(%d)", stream, ret);
			goto p_err3;
		}

		core->ischain[stream].hardware = &core->hardware;
	}

	ret = v4l2_device_register(&pdev->dev, &core->v4l2_dev);
	if (ret) {
		dev_err(&pdev->dev, "failed to register is v4l2 device\n");
		goto p_err3;
	}

#ifdef SOC_30S
	/* video entity - 3a0 */
	is_30s_video_probe(core);
#endif

#ifdef SOC_30C
	/* video entity - 3a0 capture */
	is_30c_video_probe(core);
#endif

#ifdef SOC_30P
	/* video entity - 3a0 preview */
	is_30p_video_probe(core);
#endif

#ifdef SOC_30F
	/* video entity - 3a0 early-FD */
	is_30f_video_probe(core);
#endif

#ifdef SOC_30G
	/* video entity - 3a0 MRG_OUT */
	is_30g_video_probe(core);
#endif

#ifdef SOC_30O
	/* video entity - 3a0 orbds */
	is_30o_video_probe(core);
#endif

#ifdef SOC_30L
	/* video entity - 3a0 lmeds */
	is_30l_video_probe(core);
#endif

#ifdef SOC_31S
	/* video entity - 3a1 */
	is_31s_video_probe(core);
#endif

#ifdef SOC_31C
	/* video entity - 3a1 capture */
	is_31c_video_probe(core);
#endif

#ifdef SOC_31P
	/* video entity - 3a1 preview */
	is_31p_video_probe(core);
#endif

#ifdef SOC_31F
	/* video entity - 3a1 early-FD */
	is_31f_video_probe(core);
#endif

#ifdef SOC_31G
	/* video entity - 3a1 MRG_OUT */
	is_31g_video_probe(core);
#endif

#ifdef SOC_31O
	/* video entity - 3a0 orbds */
	is_31o_video_probe(core);
#endif

#ifdef SOC_31L
	/* video entity - 3a0 lmeds */
	is_31l_video_probe(core);
#endif

#ifdef SOC_32S
	/* video entity - 3a2 */
	is_32s_video_probe(core);
#endif

#ifdef SOC_32C
	/* video entity - 3a2 capture*/
	is_32c_video_probe(core);
#endif

#ifdef SOC_32P
	/* video entity - 3a2 preview */
	is_32p_video_probe(core);
#endif

#ifdef SOC_32F
	/* video entity - 3a2 early-FD */
	is_32f_video_probe(core);
#endif

#ifdef SOC_32G
	/* video entity - 3a2 MRG_OUT */
	is_32g_video_probe(core);
#endif

#ifdef SOC_32O
	/* video entity - 3a0 orbds */
	is_32o_video_probe(core);
#endif

#ifdef SOC_32L
	/* video entity - 3a0 lmeds */
	is_32l_video_probe(core);
#endif

#ifdef SOC_33S
	/* video entity - 3a3 */
	is_33s_video_probe(core);
#endif

#ifdef SOC_33C
	/* video entity - 3a3 capture*/
	is_33c_video_probe(core);
#endif

#ifdef SOC_33P
	/* video entity - 3a3 preview */
	is_33p_video_probe(core);
#endif

#ifdef SOC_33F
	/* video entity - 3a3 early-FD */
	is_33f_video_probe(core);
#endif

#ifdef SOC_33G
	/* video entity - 3a3 MRG_OUT */
	is_33g_video_probe(core);
#endif

#ifdef SOC_33O
	/* video entity - 3a0 orbds */
	is_33o_video_probe(core);
#endif

#ifdef SOC_33L
	/* video entity - 3a0 lmeds */
	is_33l_video_probe(core);
#endif

#ifdef SOC_LME0
	/* video entity - LME0 input */
	is_lme0_video_probe(core);
#endif

#ifdef SOC_LME0S
	/* video entity - LME0 MVF output */
	is_lme0s_video_probe(core);
#endif

#ifdef SOC_LME0C
	/* video entity - LME0 cost output */
	is_lme0c_video_probe(core);
#endif

#ifdef SOC_LME1
	/* video entity - LME1 input */
	is_lme1_video_probe(core);
#endif

#ifdef SOC_LME1S
	/* video entity - LME1 MVF output */
	is_lme1s_video_probe(core);
#endif

#ifdef SOC_LME1C
	/* video entity - LME1 cost output */
	is_lme1c_video_probe(core);
#endif

#ifdef SOC_I0S
	/* video entity - isp0 */
	is_i0s_video_probe(core);
#endif

#ifdef SOC_I0C
	/* video entity - isp0 capture */
	is_i0c_video_probe(core);
#endif

#ifdef SOC_I0P
	/* video entity - isp0 preview */
	is_i0p_video_probe(core);
#endif

#ifdef SOC_I1S
	/* video entity - isp1 */
	is_i1s_video_probe(core);
#endif

#ifdef SOC_I1C
	/* video entity - isp1 capture */
	is_i1c_video_probe(core);
#endif

#ifdef SOC_I1P
	/* video entity - isp1 preview */
	is_i1p_video_probe(core);
#endif

#ifdef SOC_ME0C
	/* video entity - me out */
	is_me0c_video_probe(core);
#endif

#ifdef SOC_ME1C
	/* video entity - me out */
	is_me1c_video_probe(core);
#endif

#ifdef SOC_ORBMCH
	/* video entity - orb0 out */
	is_orb0c_video_probe(core);
	/* video entity - orb1 out */
	is_orb1c_video_probe(core);
#endif

#ifdef SOC_MCS
	/* video entity - scp */
	is_m0s_video_probe(core);
	is_m1s_video_probe(core);
	is_m0p_video_probe(core);
	is_m1p_video_probe(core);
	is_m2p_video_probe(core);
	is_m3p_video_probe(core);
	is_m4p_video_probe(core);
	is_m5p_video_probe(core);
#endif

#ifdef SOC_VRA
	/* video entity - vra */
	is_vra_video_probe(core);
#endif

#ifdef SOC_TNR
	/* video entity - TNR slave input (image) */
	is_i0t_video_probe(core);
#endif

#ifdef SOC_TNR_MERGER
	/* video entity - TNR slave input (weight) */
	is_i0g_video_probe(core);
	/* video entity - TNR prev output (image) */
	is_i0v_video_probe(core);
	/* video entity - TNR prev output (weight) */
	is_i0w_video_probe(core);
#endif

#ifdef SOC_CLH
	/* video entity - clahe */
	is_cl0s_video_probe(core);
	is_cl0c_video_probe(core);
#endif

#ifdef SOC_YPP
	/* video entity - yuvpp */
	is_ypp_video_probe(core);
#endif

	platform_set_drvdata(pdev, core);

	mutex_init(&core->sensor_lock);

	ret = is_interface_ischain_probe(&core->interface_ischain,
		&core->hardware,
		&core->resourcemgr,
		core->pdev,
		(ulong)core->regs);
	if (ret) {
		dev_err(&pdev->dev, "interface_ischain_probe fail\n");
		goto p_err1;
	}

	ret = is_hardware_probe(&core->hardware, &core->interface, &core->interface_ischain,
		core->pdev);
	if (ret) {
		dev_err(&pdev->dev, "hardware_probe fail\n");
		goto p_err1;
	}

	/* set sysfs for set position to actuator */
	sysfs_actuator.init_step = 0;
	for (i = 0; i < INIT_MAX_SETTING; i++) {
		sysfs_actuator.init_positions[i] = -1;
		sysfs_actuator.init_delays[i] = -1;
	}
#ifdef FIXED_SENSOR_DEBUG
	sysfs_sensor.is_en = false;
	sysfs_sensor.is_fps_en = false;
	sysfs_sensor.frame_duration = FIXED_MAX_FPS_VALUE;
	sysfs_sensor.max_fps = FIXED_MAX_FPS_VALUE;
	sysfs_sensor.set_fps = FIXED_MAX_FPS_VALUE;
	sysfs_sensor.long_exposure_time = FIXED_EXPOSURE_VALUE;
	sysfs_sensor.short_exposure_time = FIXED_EXPOSURE_VALUE;
	sysfs_sensor.long_analog_gain = FIXED_AGAIN_VALUE;
	sysfs_sensor.short_analog_gain = FIXED_AGAIN_VALUE;
	sysfs_sensor.long_digital_gain = FIXED_DGAIN_VALUE;
	sysfs_sensor.short_digital_gain = FIXED_DGAIN_VALUE;
#endif

	EXYNOS_MIF_ADD_NOTIFIER(&exynos_is_mif_throttling_nb);

#if defined(SECURE_CAMERA_IRIS) || defined(SECURE_CAMERA_FACE)
	probe_info("%s: call SMC_SECCAM_SETENV, SECURE_CAMERA_CH(%#x), SECURE_CAMERA_HEAP_ID(%d)\n",
		__func__, SECURE_CAMERA_CH, SECURE_CAMERA_HEAP_ID);

#if defined(SECURE_CAMERA_FACE_SEQ_CHK)
	ret = 0;
#else
	ret = exynos_smc(SMC_SECCAM_SETENV, SECURE_CAMERA_CH, SECURE_CAMERA_HEAP_ID, 0);
#endif
	if (ret) {
		dev_err(is_dev, "[SMC] SMC_SECCAM_SETENV fail(%d)\n", ret);
		goto p_err3;
	}

	mem_info_addr = core->secure_mem_info[0] ? core->secure_mem_info[0] : SECURE_CAMERA_MEM_ADDR;
	mem_info_size = core->secure_mem_info[1] ? core->secure_mem_info[1] : SECURE_CAMERA_MEM_SIZE;

	probe_info("%s: call SMC_SECCAM_INIT, mem_info(%#08lx, %#08lx)\n",
		__func__, mem_info_addr, mem_info_size);
#if defined(SECURE_CAMERA_FACE_SEQ_CHK)
	ret = 0;
#else
	ret = exynos_smc(SMC_SECCAM_INIT, mem_info_addr, mem_info_size, 0);
#endif
	if (ret) {
		dev_err(is_dev, "[SMC] SMC_SECCAM_INIT fail(%d)\n", ret);
		goto p_err3;
	}
	mem_info_addr = core->non_secure_mem_info[0] ? core->non_secure_mem_info[0] : NON_SECURE_CAMERA_MEM_ADDR;
	mem_info_size = core->non_secure_mem_info[1] ? core->non_secure_mem_info[1] : NON_SECURE_CAMERA_MEM_SIZE;

	probe_info("%s: call SMC_SECCAM_INIT_NSBUF, mem_info(%#08lx, %#08lx)\n",
		__func__, mem_info_addr, mem_info_size);
#if defined(SECURE_CAMERA_FACE_SEQ_CHK)
	ret = 0;
#else
	ret = exynos_smc(SMC_SECCAM_INIT_NSBUF, mem_info_addr, mem_info_size, 0);
#endif
	if (ret) {
		dev_err(is_dev, "[SMC] SMC_SECCAM_INIT_NSBUF fail(%d)\n", ret);
		goto p_err3;
	}
#endif

#if defined(CONFIG_PM)
	pm_runtime_enable(&pdev->dev);
#endif

#ifdef ENABLE_FAULT_HANDLER
#ifdef CONFIG_EXYNOS_IOVMM
	iovmm_set_fault_handler(is_dev, is_fault_handler, NULL);
#else
	iommu_register_device_fault_handler(is_dev, is_fault_handler, NULL);
	addr_d3_csis = ioremap_nocache(0x171a1004, SZ_32); /* HACK */
#endif
#endif
#ifdef USE_CAMERA_IOVM_BEST_FIT
	iommu_dma_enable_best_fit_algo(is_dev);
#endif

	for (i = 0; i < MAX_SENSOR_SHARED_RSC; i++) {
		spin_lock_init(&core->shared_rsc_slock[i]);
		atomic_set(&core->shared_rsc_count[i], 0);
	}

#if defined(USE_I2C_LOCK)
	for (i = 0; i < SENSOR_CONTROL_I2C_MAX; i++) {
		mutex_init(&core->i2c_lock[i]);
		atomic_set(&core->i2c_rsccount[i], 0);
	}
#endif

	mutex_init(&core->ois_mode_lock);
	mutex_init(&core->laser_lock);
	atomic_set(&core->laser_refcount, 0);

	/* set sysfs for debuging */
	sysfs_debug.en_dvfs = 1;
	sysfs_debug.hal_debug_mode = 0;
	sysfs_debug.hal_debug_delay = DBG_HAL_DEAD_PANIC_DELAY;
	sysfs_debug.pattern_en = 0;
	sysfs_debug.pattern_fps = 0;

	ret = sysfs_create_group(&core->pdev->dev.kobj, &is_debug_attr_group);

	if (pdata) {
		s = pinctrl_lookup_state(pdata->pinctrl, "release");

		if (!IS_ERR(s)) {
			if (pinctrl_select_state(pdata->pinctrl, s) < 0) {
				probe_err("pinctrl_select_state is fail\n");
				goto p_err3;
			}
		} else {
			probe_info("[%s]pinctrl_lookup no release state\n", __func__);
		}
	}

	core->shutdown = false;

#if IS_ENABLED(CONFIG_EXYNOS_SYSTEM_EVENT)
	ret = __is_sysevent_desc_init(pdev, core);
	if (ret)
		goto p_err3;
#endif

#if defined(ENABLE_FPSIMD_FOR_USER) && defined(VH_FPSIMD_API)
	ret = is_fpsimd_probe();
	if (ret)
		goto p_err3;
#endif

	probe_info("%s:end\n", __func__);
	return 0;

p_err3:
	iounmap(core->regs);
p_err1:
	kfree(core);
	return ret;
}

void is_cleanup(struct is_core *core)
{
	struct is_device_sensor *device;
	int ret;
	u32 i;

	info("%s enter: shutdown(%d)\n", __func__, core->shutdown);

	for (i = 0; i < IS_SENSOR_COUNT; i++) {
		device = &core->sensor[i];
		if (device) {
			mutex_lock(&device->mutex_reboot);
			device->reboot = true;
			if (test_bit(IS_SENSOR_FRONT_START, &device->state)) {
				minfo("try to stop sensor\n", device);

				ret = is_sensor_front_stop(device, true);
				if (ret)
					mwarn("failed to stop sensor: %d", device, ret);
			}
			mutex_unlock(&device->mutex_reboot);
		}
	}

	info("%s exit\n", __func__);
}

static void is_shutdown(struct platform_device *pdev)
{
	struct is_core *core = (struct is_core *)platform_get_drvdata(pdev);
	struct is_device_sensor *device;
	struct v4l2_subdev *subdev;
	struct is_module_enum *module;
	struct is_device_sensor_peri *sensor_peri;
	u32 i;

	if (!core) {
		err("%s: core(NULL)", __func__);
		return;
	}

	core->shutdown = true;
	info("%s++: shutdown(%d)\n", __func__, core->shutdown);

	is_cleanup(core);

	for (i = 0; i < IS_SENSOR_COUNT; i++) {
		device = &core->sensor[i];
		if (!device) {
			warn("%s: device(NULL)", __func__);
			continue;
		}

		if (test_bit(IS_SENSOR_OPEN, &device->state)) {
			subdev = device->subdev_module;
			if (!subdev) {
				warn("%s: subdev(NULL)", __func__);
				continue;
			}

			module = (struct is_module_enum *)v4l2_get_subdevdata(subdev);
			if (!module) {
				warn("%s: module(NULL)", __func__);
				continue;
			}

			sensor_peri = (struct is_device_sensor_peri *)module->private_data;
			if (!sensor_peri) {
				warn("%s: sensor_peri(NULL)", __func__);
				continue;
			}

			is_sensor_deinit_sensor_thread(sensor_peri);
			cancel_work_sync(&sensor_peri->cis.global_setting_work);
			cancel_work_sync(&sensor_peri->cis.mode_setting_work);
		}
	}
	info("%s:--\n", __func__);
}

static const struct dev_pm_ops is_pm_ops = {
	.suspend		= is_suspend,
	.resume			= is_resume,
	.runtime_suspend	= is_ischain_runtime_suspend,
	.runtime_resume		= is_ischain_runtime_resume,
};

#ifdef CONFIG_OF
static const struct of_device_id exynos_is_match[] = {
	{
		.compatible = "samsung,exynos-is",
	},
	{},
};
MODULE_DEVICE_TABLE(of, exynos_is_match);

static struct platform_driver is_driver = {
	.probe		= is_probe,
	.shutdown	= is_shutdown,
	.driver = {
		.name	= IS_DRV_NAME,
		.owner	= THIS_MODULE,
		.pm	= &is_pm_ops,
		.of_match_table = exynos_is_match,
	}
};
#else
static struct platform_driver is_driver = {
	.driver = {
		.name	= IS_DRV_NAME,
		.owner	= THIS_MODULE,
		.pm	= &is_pm_ops,
	}
};
#endif

#ifdef MODULE
static int is_driver_init(void)
{
	int ret = 0;

	ret = platform_driver_register(&is_driver);
	if (ret) {
		err("is_driver register failed(%d)", ret);
		return ret;
	}

	ret = platform_driver_register(&is_sensor_driver);
	if (ret) {
		err("is_sensor_driver register failed(%d)", ret);
		return ret;
	}

	ret = spi_register_driver(&is_spi_driver);
	if (ret) {
		err("is_spi_driver register failed(%d)", ret);
		return ret;
	}

	ret = platform_driver_register(&sensor_paf_pdp_platform_driver);
	if (ret) {
		err("sensor_paf_pdp_driver register failed(%d)", ret);
		return ret;
	}

	ret = platform_driver_register(&sensor_module_driver);
	if (ret) {
		err("sensor_module_driver register failed(%d)", ret);
		return ret;
	}

	ret = i2c_add_driver(&sensor_i2c_dummy_driver);
	if (ret) {
		err("sensor_i2c_dummy_driver register failed(%d)", ret);
		return ret;
	}

	ret = platform_driver_register(&is_camif_dma_driver);
	if (ret) {
		err("is_camif_dma_driver register failed(%d)", ret);
		return ret;
	}

	ret = is_vender_driver_init();
	if (ret) {
		err("is_vender_driver_init failed(%d)", ret);
		return ret;
	}

	return ret;
}

void is_driver_exit(void)
{
	i2c_del_driver(&sensor_i2c_dummy_driver);
	platform_driver_unregister(&sensor_module_driver);
	platform_driver_unregister(&sensor_paf_pdp_platform_driver);
	spi_unregister_driver(&is_spi_driver);
	platform_driver_unregister(&is_sensor_driver);
	platform_driver_unregister(&is_driver);

	is_vender_driver_exit();
}
module_init(is_driver_init);
module_exit(is_driver_exit);
MODULE_SOFTDEP("pre: cmupmucal clk_exynos exynos-pmu-if pinctrl-samsung-core phy-exynos-mipi samsung-iommu samsung-iommu-group");
#else
builtin_platform_driver_probe(is_driver, is_probe);
#endif


MODULE_AUTHOR("Gilyeon im<kilyeon.im@samsung.com>");
MODULE_DESCRIPTION("Exynos FIMC_IS2 driver");
MODULE_LICENSE("GPL");
