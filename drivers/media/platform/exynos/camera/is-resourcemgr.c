/*
 * Samsung Exynos5 SoC series FIMC-IS driver
 *
 * exynos5 fimc-is video functions
 *
 * Copyright (c) 2011 Samsung Electronics Co., Ltd
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#include <linux/memblock.h>
#include <linux/slab.h>
#include <linux/irq.h>
#include <linux/interrupt.h>
#include <linux/delay.h>
#include <linux/version.h>
#include <linux/gpio.h>
#include <linux/clk.h>
#include <linux/regulator/consumer.h>
#include <linux/videodev2.h>
#include <linux/videodev2_exynos_camera.h>
#include <linux/module.h>
#include <linux/moduleparam.h>
#include <linux/platform_device.h>
#include <media/v4l2-ctrls.h>
#include <media/v4l2-device.h>
#include <media/v4l2-subdev.h>
#include <soc/samsung/freq-qos-tracer.h>
#if IS_ENABLED(CONFIG_EXYNOS_THERMAL) || IS_ENABLED(CONFIG_EXYNOS_THERMAL_V2)
#include <soc/samsung/tmu.h>
#if (LINUX_VERSION_CODE >= KERNEL_VERSION(5, 4, 0))
#include <soc/samsung/isp_cooling.h>
#else
#include <linux/isp_cooling.h>
#endif
#endif
#include <linux/cpuidle.h>
#include <linux/soc/samsung/exynos-soc.h>
#include <soc/samsung/bts.h>
#if IS_ENABLED(CONFIG_CMU_EWF)
#include <soc/samsung/cmu_ewf.h>
#endif

#include <linux/of_fdt.h>
#include <linux/of_reserved_mem.h>

#ifdef CONFIG_EXYNOS_ITMON
#include <soc/samsung/exynos-itmon.h>
#endif

#if IS_ENABLED(CONFIG_EXYNOS_SNAPSHOT)
#include <linux/exynos-ss.h>
#elif defined(CONFIG_DEBUG_SNAPSHOT)
#include <linux/debug-snapshot.h>
#endif
#include <soc/samsung/debug-snapshot.h>
#include <soc/samsung/exynos-bcm_dbg.h>
#include <soc/samsung/exynos-sci_dbg.h>
#if IS_ENABLED(CONFIG_EXYNOS_SYSTEM_EVENT)
#include <soc/samsung/sysevent.h>
#endif

#include "is-resourcemgr.h"
#include "is-hw.h"
#include "is-debug.h"
#include "is-core.h"
#include "is-dvfs.h"
#include "is-interface-library.h"
#include "hardware/is-hw-control.h"
#include "votf/camerapp-votf.h"
#include "is-device-camif-dma.h"

#if IS_ENABLED(CONFIG_EXYNOS_MSN)
#include <linux/ems.h>
struct emstune_mode_request emstune_req;
#endif

#define CLUSTER_MIN_MASK			0x0000FFFF
#define CLUSTER_MIN_SHIFT			0
#define CLUSTER_MAX_MASK			0xFFFF0000
#define CLUSTER_MAX_SHIFT			16

#if defined(QOS_INTCAM)
struct is_pm_qos_request exynos_isp_qos_int_cam;
#endif
#if defined(QOS_TNR)
struct is_pm_qos_request exynos_isp_qos_tnr;
#endif
#if defined(QOS_CSIS)
struct is_pm_qos_request exynos_isp_qos_csis;
#endif
#if defined(QOS_ISP)
struct is_pm_qos_request exynos_isp_qos_isp;
#endif
struct is_pm_qos_request exynos_isp_qos_int;
struct is_pm_qos_request exynos_isp_qos_mem;
struct is_pm_qos_request exynos_isp_qos_cam;
struct is_pm_qos_request exynos_isp_qos_disp;
struct is_pm_qos_request exynos_isp_qos_hpg;
struct is_pm_qos_request exynos_isp_qos_cpu_online_min;
#if IS_ENABLED(CONFIG_CMU_EWF)
unsigned int idx_ewf;
#endif

#if (LINUX_VERSION_CODE >= KERNEL_VERSION(5, 4, 0))
struct freq_qos_request exynos_isp_qos_cluster0_min;
struct freq_qos_request exynos_isp_qos_cluster0_max;
struct freq_qos_request exynos_isp_qos_cluster1_min;
struct freq_qos_request exynos_isp_qos_cluster1_max;
struct freq_qos_request exynos_isp_qos_cluster2_min;
struct freq_qos_request exynos_isp_qos_cluster2_max;

#define IS_GET_CPU_QOS(cpu)	\
	cpufreq_cpu_get(cpu) ? &(cpufreq_cpu_get(cpu)->constraints) : NULL

#define C0MIN_QOS_ADD(freq) freq_qos_tracer_add_request(IS_GET_CPU_QOS(CL0), &exynos_isp_qos_cluster0_min, FREQ_QOS_MIN, freq * 1000)
#define C0MIN_QOS_DEL() freq_qos_tracer_remove_request(&exynos_isp_qos_cluster0_min)
#define C0MIN_QOS_UPDATE(freq) freq_qos_update_request(&exynos_isp_qos_cluster0_min, freq * 1000)
#define C0MAX_QOS_ADD(freq) freq_qos_tracer_add_request(IS_GET_CPU_QOS(CL0), &exynos_isp_qos_cluster0_max, FREQ_QOS_MAX, freq * 1000)
#define C0MAX_QOS_DEL() freq_qos_tracer_remove_request(&exynos_isp_qos_cluster0_max)
#define C0MAX_QOS_UPDATE(freq) freq_qos_update_request(&exynos_isp_qos_cluster0_max, freq * 1000)
#define C1MIN_QOS_ADD(freq) freq_qos_tracer_add_request(IS_GET_CPU_QOS(CL1), &exynos_isp_qos_cluster1_min, FREQ_QOS_MIN, freq * 1000)
#define C1MIN_QOS_DEL() freq_qos_tracer_remove_request(&exynos_isp_qos_cluster1_min)
#define C1MIN_QOS_UPDATE(freq) freq_qos_update_request(&exynos_isp_qos_cluster1_min, freq * 1000)
#define C1MAX_QOS_ADD(freq) freq_qos_tracer_add_request(IS_GET_CPU_QOS(CL1), &exynos_isp_qos_cluster1_max, FREQ_QOS_MAX, freq * 1000)
#define C1MAX_QOS_DEL() freq_qos_tracer_remove_request(&exynos_isp_qos_cluster1_max)
#define C1MAX_QOS_UPDATE(freq) freq_qos_update_request(&exynos_isp_qos_cluster1_max, freq * 1000)
#define C2MIN_QOS_ADD(freq) freq_qos_tracer_add_request(IS_GET_CPU_QOS(CL2), &exynos_isp_qos_cluster2_min, FREQ_QOS_MIN, freq * 1000)
#define C2MIN_QOS_DEL() freq_qos_tracer_remove_request(&exynos_isp_qos_cluster2_min)
#define C2MIN_QOS_UPDATE(freq) freq_qos_update_request(&exynos_isp_qos_cluster2_min, freq * 1000)
#define C2MAX_QOS_ADD(freq) freq_qos_tracer_add_request(IS_GET_CPU_QOS(CL2), &exynos_isp_qos_cluster2_max, FREQ_QOS_MAX, freq * 1000)
#define C2MAX_QOS_DEL() freq_qos_tracer_remove_request(&exynos_isp_qos_cluster2_max)
#define C2MAX_QOS_UPDATE(freq) freq_qos_update_request(&exynos_isp_qos_cluster2_max, freq * 1000)
#else
struct is_pm_qos_request exynos_isp_qos_cluster0_min;
struct is_pm_qos_request exynos_isp_qos_cluster0_max;
struct is_pm_qos_request exynos_isp_qos_cluster1_min;
struct is_pm_qos_request exynos_isp_qos_cluster1_max;
struct is_pm_qos_request exynos_isp_qos_cluster2_min;
struct is_pm_qos_request exynos_isp_qos_cluster2_max;

#define C0MIN_QOS_ADD(freq) is_pm_qos_add_request(&exynos_isp_qos_cluster0_min, PM_QOS_CLUSTER0_FREQ_MIN, freq * 1000)
#define C0MIN_QOS_DEL() is_pm_qos_remove_request(&exynos_isp_qos_cluster0_min)
#define C0MIN_QOS_UPDATE(freq) is_pm_qos_update_request(&exynos_isp_qos_cluster0_min, freq * 1000)
#define C0MAX_QOS_ADD(freq) is_pm_qos_add_request(&exynos_isp_qos_cluster0_max, PM_QOS_CLUSTER0_FREQ_MAX, freq * 1000)
#define C0MAX_QOS_DEL() is_pm_qos_remove_request(&exynos_isp_qos_cluster0_max)
#define C0MAX_QOS_UPDATE(freq) is_pm_qos_update_request(&exynos_isp_qos_cluster0_max, freq * 1000)
#define C1MIN_QOS_ADD(freq) is_pm_qos_add_request(&exynos_isp_qos_cluster1_min, PM_QOS_CLUSTER1_FREQ_MIN, freq * 1000)
#define C1MIN_QOS_DEL() is_pm_qos_remove_request(&exynos_isp_qos_cluster1_min)
#define C1MIN_QOS_UPDATE(freq) is_pm_qos_update_request(&exynos_isp_qos_cluster1_min, freq * 1000)
#define C1MAX_QOS_ADD(freq) is_pm_qos_add_request(&exynos_isp_qos_cluster1_max, PM_QOS_CLUSTER1_FREQ_MAX, freq * 1000)
#define C1MAX_QOS_DEL() is_pm_qos_remove_request(&exynos_isp_qos_cluster1_max)
#define C1MAX_QOS_UPDATE(freq) is_pm_qos_update_request(&exynos_isp_qos_cluster1_max, freq * 1000)
#define C2MIN_QOS_ADD(freq) is_pm_qos_add_request(&exynos_isp_qos_cluster2_min, PM_QOS_CLUSTER2_FREQ_MIN, freq * 1000)
#define C2MIN_QOS_DEL() is_pm_qos_remove_request(&exynos_isp_qos_cluster2_min)
#define C2MIN_QOS_UPDATE(freq) is_pm_qos_update_request(&exynos_isp_qos_cluster2_min, freq * 1000)
#define C2MAX_QOS_ADD(freq) is_pm_qos_add_request(&exynos_isp_qos_cluster2_max, PM_QOS_CLUSTER2_FREQ_MAX, freq * 1000)
#define C2MAX_QOS_DEL() is_pm_qos_remove_request(&exynos_isp_qos_cluster2_max)
#define C2MAX_QOS_UPDATE(freq) is_pm_qos_update_request(&exynos_isp_qos_cluster2_max, freq * 1000)
#endif

extern struct is_sysfs_debug sysfs_debug;
extern int is_sensor_runtime_suspend(struct device *dev);
extern int is_sensor_runtime_resume(struct device *dev);
extern void is_vendor_resource_clean(struct is_core *core);
static int is_resourcemgr_allocmem(struct is_resourcemgr *resourcemgr)
{
	struct is_mem *mem = &resourcemgr->mem;
	struct is_minfo *minfo = &resourcemgr->minfo;
#if !defined(ENABLE_DYNAMIC_MEM) && defined(ENABLE_TNR)
	size_t tnr_size = TNR_DMA_SIZE;
#endif
	int i;

	minfo->total_size = 0;
	/* setfile */
	minfo->pb_setfile = CALL_PTR_MEMOP(mem, alloc, mem->default_ctx, SETFILE_SIZE, NULL, 0);
	if (IS_ERR_OR_NULL(minfo->pb_setfile)) {
		err("failed to allocate buffer for SETFILE");
		return -ENOMEM;
	}
	minfo->total_size += minfo->pb_setfile->size;
	info("[RSC]memory_alloc(SETFILE_SIZE): 0x%08lx\n", minfo->pb_setfile->size);

	/* calibration data for each sensor postion */
	for (i = 0; i < SENSOR_POSITION_MAX; i++) {
		minfo->pb_cal[i] = CALL_PTR_MEMOP(mem, alloc, mem->default_ctx, TOTAL_CAL_DATA_SIZE, NULL, 0);
		if (IS_ERR_OR_NULL(minfo->pb_cal[i])) {
			err("failed to allocate buffer for TOTAL_CAL_DATA");
			return -ENOMEM;
		}
		minfo->total_size += minfo->pb_cal[i]->size;
		info("[RSC]memory_alloc(TOTAL_CAL_DATA_SIZE[%d]): 0x%08lx\n",
			i, minfo->pb_cal[i]->size);
	}

	/* library logging */
#if !defined(ENABLE_CLOG_RESERVED_MEM)
	minfo->pb_debug = mem->kmalloc(DEBUG_REGION_SIZE + 0x10, 16);
	if (IS_ERR_OR_NULL(minfo->pb_debug)) {
		/* retry by ION */
		minfo->pb_debug = CALL_PTR_MEMOP(mem, alloc, mem->default_ctx, DEBUG_REGION_SIZE + 0x10, NULL, 0);
		if (IS_ERR_OR_NULL(minfo->pb_debug)) {
			err("failed to allocate buffer for DEBUG_REGION");
			return -ENOMEM;
		}
	}
	minfo->total_size += minfo->pb_debug->size;
	info("[RSC]memory_alloc(DEBUG_REGION_SIZE): 0x%08lx\n", minfo->pb_debug->size);
#endif
	/* library event logging */
	minfo->pb_event = mem->kmalloc(EVENT_REGION_SIZE + 0x10, 16);
	if (IS_ERR_OR_NULL(minfo->pb_event)) {
		/* retry by ION */
		minfo->pb_event = CALL_PTR_MEMOP(mem, alloc, mem->default_ctx, EVENT_REGION_SIZE + 0x10, NULL, 0);
		if (IS_ERR_OR_NULL(minfo->pb_event)) {
			err("failed to allocate buffer for EVENT_REGION");
			return -ENOMEM;
		}
	}
	minfo->total_size += minfo->pb_event->size;
	info("[RSC]memory_alloc(EVENT_REGION_SIZE): 0x%08lx\n", minfo->pb_event->size);

	/* data region */
	minfo->pb_dregion = CALL_PTR_MEMOP(mem, alloc, mem->default_ctx, DATA_REGION_SIZE, NULL, 0);
	if (IS_ERR_OR_NULL(minfo->pb_dregion)) {
		err("failed to allocate buffer for DATA_REGION");
		return -ENOMEM;
	}
	minfo->total_size += minfo->pb_dregion->size;
	info("[RSC]memory_alloc(DATA_REGION_SIZE): 0x%08lx\n", minfo->pb_dregion->size);

	/* parameter region */
	minfo->pb_pregion = CALL_PTR_MEMOP(mem, alloc, mem->default_ctx,
						(IS_STREAM_COUNT * PARAM_REGION_SIZE), NULL, 0);
	if (IS_ERR_OR_NULL(minfo->pb_pregion)) {
		err("failed to allocate buffer for PARAM_REGION");
		return -ENOMEM;
	}
	minfo->total_size += minfo->pb_pregion->size;
	info("[RSC]memory_alloc(PARAM_REGION_SIZE x %d): 0x%08lx\n",
		IS_STREAM_COUNT, minfo->pb_pregion->size);

	/* fshared data region */
	minfo->pb_fshared = CALL_PTR_MEMOP(mem, alloc, mem->default_ctx, FSHARED_REGION_SIZE, NULL, 0);
	if (IS_ERR_OR_NULL(minfo->pb_fshared)) {
		err("failed to allocate buffer for FSHARED_REGION");
		return -ENOMEM;
	}
	minfo->total_size += minfo->pb_fshared->size;
	info("[RSC]memory_alloc(FSHARED_REGION_SIZE): 0x%08lx\n", minfo->pb_fshared->size);

#if !defined(ENABLE_DYNAMIC_MEM)
	/* 3aa/isp internal DMA buffer */
	minfo->pb_taaisp = CALL_PTR_MEMOP(mem, alloc, mem->default_ctx,
				TAAISP_DMA_SIZE, NULL, 0);
	if (IS_ERR_OR_NULL(minfo->pb_taaisp)) {
		err("failed to allocate buffer for TAAISP_DMA");
		return -ENOMEM;
	}
	minfo->total_size += minfo->pb_taaisp->size;
	info("[RSC]memory_alloc(TAAISP_DMA_SIZE): 0x%08lx\n", minfo->pb_taaisp->size);

	/* ME/DRC buffer */
#if (MEDRC_DMA_SIZE > 0)
	minfo->pb_medrc = CALL_PTR_MEMOP(mem, alloc, mem->default_ctx,
				MEDRC_DMA_SIZE, NULL, 0);
	if (IS_ERR_OR_NULL(minfo->pb_medrc)) {
		err("failed to allocate buffer for ME_DRC");
		return -ENOMEM;
	}

	minfo->total_size += minfo->pb_medrc->size;
	info("[RSC]memory_alloc(MEDRC_DMA_SIZE): 0x%08lx\n", minfo->pb_medrc->size);
#endif

#if defined(ENABLE_TNR)
	/* TNR internal DMA buffer */
	minfo->pb_tnr = CALL_PTR_MEMOP(mem, alloc, mem->default_ctx, tnr_size, NULL, 0);
	if (IS_ERR_OR_NULL(minfo->pb_tnr)) {
		err("failed to allocate buffer for TNR DMA");
		return -ENOMEM;
	}
	minfo->total_size += minfo->pb_tnr->size;
	info("[RSC]memory_alloc(TNR_DMA_SIZE): 0x%08lx\n", minfo->pb_tnr->size);
#endif
#if (ORBMCH_DMA_SIZE > 0)
	/* ORBMCH internal DMA buffer */
	minfo->pb_orbmch = CALL_PTR_MEMOP(mem, alloc, mem->default_ctx, ORBMCH_DMA_SIZE, NULL, 0);
	if (IS_ERR_OR_NULL(minfo->pb_orbmch)) {
		err("failed to allocate buffer for ORBMCH DMA");
		return -ENOMEM;
	}
	minfo->total_size += minfo->pb_orbmch->size;
	info("[RSC]memory_alloc(ORBMCH_DMA_SIZE): 0x%08lx\n", minfo->pb_orbmch->size);
#endif
#if (CLAHE_DMA_SIZE > 0)
	/* CLAHE internal DMA buffer */
	minfo->pb_clahe = CALL_PTR_MEMOP(mem, alloc, mem->default_ctx, CLAHE_DMA_SIZE, NULL, 0);
	if (IS_ERR_OR_NULL(minfo->pb_clahe)) {
		err("failed to allocate buffer for CLAHE DMA");
		return -ENOMEM;
	}
	minfo->total_size += minfo->pb_clahe->size;
	info("[RSC]memory_alloc(CLAHE_DMA_SIZE): 0x%08lx\n", minfo->pb_clahe->size);
#endif
#endif

#if defined (ENABLE_VRA)
	minfo->pb_vra = CALL_PTR_MEMOP(mem, alloc, mem->default_ctx, VRA_DMA_SIZE, NULL, 0);
	if (IS_ERR_OR_NULL(minfo->pb_vra)) {
		err("failed to allocate buffer for VRA");
		return -ENOMEM;
	}
	minfo->total_size += minfo->pb_vra->size;
	info("[RSC]memory_alloc(VRA_DMA_SIZE): 0x%08lx\n", minfo->pb_vra->size);
#endif
#if defined (ENABLE_VRA_NETARRAY)
	minfo->pb_vra_netarray = CALL_PTR_MEMOP(mem, alloc, mem->default_ctx, VRA_NET_SIZE, NULL, 0);
	if (IS_ERR_OR_NULL(minfo->pb_vra_netarray)) {
		err("failed to allocate buffer for VRA network array");
		return -ENOMEM;
	}
	minfo->total_size += minfo->pb_vra_netarray->size;
	info("[RSC]memory_alloc(VRA_NET_SIZE): 0x%08lx\n", minfo->pb_vra_netarray->size);
#endif

#if defined (ENABLE_DNR_IN_MCSC)
	minfo->pb_mcsc_dnr = CALL_PTR_MEMOP(mem, alloc, mem->default_ctx, MCSC_DNR_DMA_SIZE, NULL, 0);
	if (IS_ERR_OR_NULL(minfo->pb_mcsc_dnr)) {
		err("failed to allocate buffer for MCSC DNR");
		return -ENOMEM;
	}
	minfo->total_size += minfo->pb_mcsc_dnr->size;
	info("[RSC]memory_alloc(MCSC_DNR_DMA_SIZE): 0x%08lx\n", minfo->pb_mcsc_dnr->size);
#endif

	if (DUMMY_DMA_SIZE) {
		minfo->pb_dummy = CALL_PTR_MEMOP(mem, alloc, mem->default_ctx, DUMMY_DMA_SIZE,
			"camera_heap", 0);
		if (IS_ERR_OR_NULL(minfo->pb_dummy)) {
			err("failed to allocate buffer for dummy");
			return -ENOMEM;
		}
		minfo->total_size += minfo->pb_dummy->size;
		info("[RSC]memory_alloc(DUMMY_DMA_SIZE): 0x%08lx\n", minfo->pb_dummy->size);
	}

	probe_info("[RSC]memory_alloc(Internal total): 0x%08lx\n", minfo->total_size);

	return 0;
}

#if defined(ENABLE_CLOG_RESERVED_MEM)
static struct is_resource_rmem crmem;

#ifndef MODULE
#ifdef CONFIG_RKP
static struct vm_struct is_lib_vm_for_rkp;
#endif
static int __init is_reserved_mem_setup(struct reserved_mem *remem)
{
	crmem.phys_addr = remem->base;
	crmem.size = remem->size;

	info("%s: cam reserved mem: paddr=0x%x, size=0x%x\n", __func__,
		(unsigned int)remem->base, (unsigned int)remem->size);

	return 0;
}
RESERVEDMEM_OF_DECLARE(camera_rmem, "exynos,camera_rmem",
			is_reserved_mem_setup);
#endif
static unsigned long is_request_region(unsigned long addr, unsigned int size)
{
	int i;
	unsigned int num_pages = (size >> PAGE_SHIFT);
	pgprot_t prot = pgprot_writecombine(PAGE_KERNEL);
	struct page **pages = NULL;
	void *v_addr = NULL;

	if (!addr)
		return 0;

	pages = kmalloc_array(num_pages, sizeof(struct page *), GFP_ATOMIC);
	if (!pages)
		return 0;

	for (i = 0; i < num_pages; i++) {
		pages[i] = phys_to_page(addr);
		addr += PAGE_SIZE;
	}

	v_addr = vmap(pages, num_pages, VM_MAP, prot);
	kfree(pages);

	return (unsigned long)v_addr;
}
#endif	/* CONFIG_OF_RESERVED_MEM */

static int is_resourcemgr_initmem(struct is_resourcemgr *resourcemgr)
{
	struct is_minfo *minfo = NULL;
	int ret = 0;
	int i;

	probe_info("is_init_mem - ION\n");

	ret = is_resourcemgr_allocmem(resourcemgr);
	if (ret) {
		err("Couldn't alloc for FIMC-IS\n");
		ret = -ENOMEM;
		goto p_err;
	}

	minfo = &resourcemgr->minfo;
	/* set information */
	resourcemgr->minfo.dvaddr = 0;
	resourcemgr->minfo.kvaddr = 0;
#if defined(ENABLE_CLOG_RESERVED_MEM)
	resourcemgr->minfo.dvaddr_debug = 0x0;
	resourcemgr->minfo.phaddr_debug = crmem.phys_addr;
	resourcemgr->minfo.kvaddr_debug = is_request_region(crmem.phys_addr,
					crmem.size);
	dbg_snapshot_add_bl_item_info(CLOG_DSS_NAME,
					crmem.phys_addr, crmem.size);
	probe_info("[RSC] CLOG RMEM INFO(V/P/S): 0x%pK/0x%08x/0x%08x\n",
		resourcemgr->minfo.kvaddr_debug,
		(unsigned int)resourcemgr->minfo.phaddr_debug,
		(unsigned int)crmem.size);
	memset((void *)resourcemgr->minfo.kvaddr_debug, 0x0, crmem.size - 1);
#else
	resourcemgr->minfo.dvaddr_debug = CALL_BUFOP(minfo->pb_debug, dvaddr,
						minfo->pb_debug);
	resourcemgr->minfo.kvaddr_debug = CALL_BUFOP(minfo->pb_debug, kvaddr,
						minfo->pb_debug);
	resourcemgr->minfo.phaddr_debug = CALL_BUFOP(minfo->pb_debug, phaddr,
						minfo->pb_debug);
#endif
	resourcemgr->minfo.dvaddr_event = CALL_BUFOP(minfo->pb_event, dvaddr, minfo->pb_event);
	resourcemgr->minfo.kvaddr_event = CALL_BUFOP(minfo->pb_event, kvaddr, minfo->pb_event);
	resourcemgr->minfo.phaddr_event = CALL_BUFOP(minfo->pb_event, phaddr, minfo->pb_event);

	resourcemgr->minfo.dvaddr_fshared = CALL_BUFOP(minfo->pb_fshared, dvaddr, minfo->pb_fshared);
	resourcemgr->minfo.kvaddr_fshared = CALL_BUFOP(minfo->pb_fshared, kvaddr, minfo->pb_fshared);

	resourcemgr->minfo.dvaddr_region = CALL_BUFOP(minfo->pb_pregion, dvaddr, minfo->pb_pregion);
	resourcemgr->minfo.kvaddr_region = CALL_BUFOP(minfo->pb_pregion, kvaddr, minfo->pb_pregion);

	resourcemgr->minfo.dvaddr_lhfd = 0;
	resourcemgr->minfo.kvaddr_lhfd = 0;

#if defined(ENABLE_VRA)
	resourcemgr->minfo.dvaddr_vra = CALL_BUFOP(minfo->pb_vra, dvaddr, minfo->pb_vra);
	resourcemgr->minfo.kvaddr_vra = CALL_BUFOP(minfo->pb_vra, kvaddr, minfo->pb_vra);
#else
	resourcemgr->minfo.dvaddr_vra = 0;
	resourcemgr->minfo.kvaddr_vra = 0;
#endif

#if defined(ENABLE_VRA_NETARRAY)
	resourcemgr->minfo.dvaddr_vra_netarray = CALL_BUFOP(minfo->pb_vra_netarray, dvaddr, minfo->pb_vra_netarray);
	resourcemgr->minfo.kvaddr_vra_netarray = CALL_BUFOP(minfo->pb_vra_netarray, kvaddr, minfo->pb_vra_netarray);
#else
	resourcemgr->minfo.dvaddr_vra_netarray = 0;
	resourcemgr->minfo.kvaddr_vra_netarray = 0;
#endif

#if defined(ENABLE_DNR_IN_MCSC)
	resourcemgr->minfo.dvaddr_mcsc_dnr = CALL_BUFOP(minfo->pb_mcsc_dnr, dvaddr, minfo->pb_mcsc_dnr);
	resourcemgr->minfo.kvaddr_mcsc_dnr = CALL_BUFOP(minfo->pb_mcsc_dnr, kvaddr, minfo->pb_mcsc_dnr);
#else
	resourcemgr->minfo.dvaddr_mcsc_dnr = 0;
	resourcemgr->minfo.kvaddr_mcsc_dnr = 0;
#endif

	resourcemgr->minfo.dvaddr_tpu = 0;
	resourcemgr->minfo.kvaddr_tpu = 0;
	resourcemgr->minfo.kvaddr_debug_cnt =  resourcemgr->minfo.kvaddr_debug
						+ DEBUG_REGION_SIZE;
	resourcemgr->minfo.kvaddr_event_cnt =  resourcemgr->minfo.kvaddr_event
						+ EVENT_REGION_SIZE;
	resourcemgr->minfo.kvaddr_setfile = CALL_BUFOP(minfo->pb_setfile, kvaddr, minfo->pb_setfile);

	for (i = 0; i < SENSOR_POSITION_MAX; i++)
		resourcemgr->minfo.kvaddr_cal[i] =
			CALL_BUFOP(minfo->pb_cal[i], kvaddr, minfo->pb_cal[i]);

	if (DUMMY_DMA_SIZE) {
		resourcemgr->minfo.dvaddr_dummy =
			CALL_BUFOP(minfo->pb_dummy, dvaddr, minfo->pb_dummy);
		resourcemgr->minfo.kvaddr_dummy =
			CALL_BUFOP(minfo->pb_dummy, kvaddr, minfo->pb_dummy);
		resourcemgr->minfo.phaddr_dummy =
			CALL_BUFOP(minfo->pb_dummy, phaddr, minfo->pb_dummy);

		probe_info("[RSC] Dummy buffer: dva(0x%pad) kva(0x%pK) pha(0x%pad)\n",
			&resourcemgr->minfo.dvaddr_dummy,
			resourcemgr->minfo.kvaddr_dummy,
			&resourcemgr->minfo.phaddr_dummy);
	}

	probe_info("[RSC] Kernel virtual for library: 0x%pK\n", resourcemgr->minfo.kvaddr);
	probe_info("[RSC] Kernel virtual for debug: 0x%pK\n", resourcemgr->minfo.kvaddr_debug);
	probe_info("[RSC] is_init_mem done\n");
p_err:
	return ret;
}

#ifdef ENABLE_DYNAMIC_MEM
static int is_resourcemgr_alloc_dynamic_mem(struct is_resourcemgr *resourcemgr)
{
	struct is_mem *mem = &resourcemgr->mem;
	struct is_minfo *minfo = &resourcemgr->minfo;
#if defined (ENABLE_TNR)
	size_t tnr_size = TNR_DMA_SIZE;
#endif

	/* 3aa/isp internal DMA buffer */
	minfo->pb_taaisp = CALL_PTR_MEMOP(mem, alloc, mem->default_ctx,
				TAAISP_DMA_SIZE, NULL, 0);
	if (IS_ERR_OR_NULL(minfo->pb_taaisp)) {
		err("failed to allocate buffer for TAAISP_DMA memory");
		return -ENOMEM;
	}

	info("[RSC]memory_alloc(TAAISP_DMA_SIZE): 0x%08lx\n", minfo->pb_taaisp->size);

	/* ME/DRC buffer */
#if (MEDRC_DMA_SIZE > 0)
	minfo->pb_medrc = CALL_PTR_MEMOP(mem, alloc, mem->default_ctx,
				MEDRC_DMA_SIZE, NULL, 0);
	if (IS_ERR_OR_NULL(minfo->pb_medrc)) {
		CALL_VOID_BUFOP(minfo->pb_taaisp, free, minfo->pb_taaisp);
		err("failed to allocate buffer for ME_DRC");
		return -ENOMEM;
	}

	info("[RSC]memory_alloc(MEDRC_DMA_SIZE): 0x%08lx\n", minfo->pb_medrc->size);
#endif

#if defined(ENABLE_TNR)
	/* TNR internal DMA buffer */
	minfo->pb_tnr = CALL_PTR_MEMOP(mem, alloc, mem->default_ctx, tnr_size, NULL, 0);
	if (IS_ERR_OR_NULL(minfo->pb_tnr)) {
		CALL_VOID_BUFOP(minfo->pb_taaisp, free, minfo->pb_taaisp);
#if (MEDRC_DMA_SIZE > 0)
		CALL_VOID_BUFOP(minfo->pb_medrc, free, minfo->pb_medrc);
#endif
		err("failed to allocate buffer for TNR DMA");
		return -ENOMEM;
	}

	info("[RSC]memory_alloc(TNR_DMA_SIZE): 0x%08lx\n", minfo->pb_tnr->size);
#endif

#if (ORBMCH_DMA_SIZE > 0)
	/* ORBMCH internal DMA buffer */
	minfo->pb_orbmch = CALL_PTR_MEMOP(mem, alloc, mem->default_ctx,
			ORBMCH_DMA_SIZE, NULL, 0);
	if (IS_ERR_OR_NULL(minfo->pb_orbmch)) {
		CALL_VOID_BUFOP(minfo->pb_taaisp, free, minfo->pb_taaisp);
#if (MEDRC_DMA_SIZE > 0)
		CALL_VOID_BUFOP(minfo->pb_medrc, free, minfo->pb_medrc);
#endif
#if defined(ENABLE_TNR)
		CALL_VOID_BUFOP(minfo->pb_tnr, free, minfo->pb_tnr);
#endif
		err("failed to allocate buffer for ORBMCH DMA");
		return -ENOMEM;
	}

	info("[RSC]memory_alloc(ORBMCH_DMA_SIZE): 0x%08lx\n", minfo->pb_orbmch->size);
#endif

#if (CLAHE_DMA_SIZE > 0)
	/* CLAHE internal DMA buffer */
	minfo->pb_clahe = CALL_PTR_MEMOP(mem, alloc, mem->default_ctx,
			CLAHE_DMA_SIZE, NULL, 0);
	if (IS_ERR_OR_NULL(minfo->pb_clahe)) {
		CALL_VOID_BUFOP(minfo->pb_taaisp, free, minfo->pb_taaisp);
#if (MEDRC_DMA_SIZE > 0)
		CALL_VOID_BUFOP(minfo->pb_medrc, free, minfo->pb_medrc);
#endif
#if defined(ENABLE_TNR)
		CALL_VOID_BUFOP(minfo->pb_tnr, free, minfo->pb_tnr);
#endif
#if (ORBMCH_DMA_SIZE > 0)
		CALL_VOID_BUFOP(minfo->pb_orbmch, free, minfo->pb_orbmch);
#endif
		err("failed to allocate buffer for CLAHE DMA");
		return -ENOMEM;
	}

	info("[RSC]memory_alloc(CLAHE_DMA_SIZE): 0x%08lx\n", minfo->pb_clahe->size);
#endif

	return 0;
}

static int is_resourcemgr_init_dynamic_mem(struct is_resourcemgr *resourcemgr)
{
	struct is_minfo *minfo = &resourcemgr->minfo;
	int ret = 0;
	unsigned long kva;
	dma_addr_t dva;

	probe_info("is_init_mem - ION\n");

	ret = is_resourcemgr_alloc_dynamic_mem(resourcemgr);
	if (ret) {
		err("Couldn't alloc for FIMC-IS\n");
		ret = -ENOMEM;
		goto p_err;
	}

	kva = CALL_BUFOP(minfo->pb_taaisp, kvaddr, minfo->pb_taaisp);
	dva = CALL_BUFOP(minfo->pb_taaisp, dvaddr, minfo->pb_taaisp);
	info("[RSC] TAAISP_DMA memory kva:0x%pK, dva: %pad\n", kva, &dva);

#if (MEDRC_DMA_SIZE > 0)
	kva = CALL_BUFOP(minfo->pb_medrc, kvaddr, minfo->pb_medrc);
	dva = CALL_BUFOP(minfo->pb_medrc, dvaddr, minfo->pb_medrc);
	info("[RSC] ME_DRC memory kva:0x%pK, dva: %pad\n", kva, &dva);
#endif

#if defined(ENABLE_TNR)
	kva = CALL_BUFOP(minfo->pb_tnr, kvaddr, minfo->pb_tnr);
	dva = CALL_BUFOP(minfo->pb_tnr, dvaddr, minfo->pb_tnr);
	info("[RSC] TNR_DMA memory kva:0x%pK, dva: %pad\n", kva, &dva);
#endif

#if (ORBMCH_DMA_SIZE > 0)
	kva = CALL_BUFOP(minfo->pb_orbmch, kvaddr, minfo->pb_orbmch);
	dva = CALL_BUFOP(minfo->pb_orbmch, dvaddr, minfo->pb_orbmch);
	info("[RSC] ORBMCH_DMA memory kva:0x%pK, dva: %pad\n", kva, &dva);
#endif

#if (CLAHE_DMA_SIZE > 0)
	kva = CALL_BUFOP(minfo->pb_clahe, kvaddr, minfo->pb_clahe);
	dva = CALL_BUFOP(minfo->pb_clahe, dvaddr, minfo->pb_clahe);
	info("[RSC] CLAHE_DMA memory kva:0x%pK, dva: %pad\n", kva, &dva);
#endif

	info("[RSC] %s done\n", __func__);

p_err:
	return ret;
}

static int is_resourcemgr_deinit_dynamic_mem(struct is_resourcemgr *resourcemgr)
{
	struct is_minfo *minfo = &resourcemgr->minfo;

#if (CLAHE_DMA_SIZE > 0)
	CALL_VOID_BUFOP(minfo->pb_clahe, free, minfo->pb_clahe);
#endif
#if (ORBMCH_DMA_SIZE > 0)
	CALL_VOID_BUFOP(minfo->pb_orbmch, free, minfo->pb_orbmch);
#endif
#if defined(ENABLE_TNR)
	CALL_VOID_BUFOP(minfo->pb_tnr, free, minfo->pb_tnr);
#endif
#if (MEDRC_DMA_SIZE > 0)
		CALL_VOID_BUFOP(minfo->pb_medrc, free, minfo->pb_medrc);
#endif
	CALL_VOID_BUFOP(minfo->pb_taaisp, free, minfo->pb_taaisp);

	return 0;
}
#endif /* #ifdef ENABLE_DYNAMIC_MEM */

#if defined(SECURE_CAMERA_MEM_SHARE)
static int is_resourcemgr_alloc_secure_mem(struct is_resourcemgr *resourcemgr)
{
	struct is_mem *mem = &resourcemgr->mem;
	struct is_minfo *minfo = &resourcemgr->minfo;

#if defined(SECURE_CAMERA_TAAISP)
	/* 3aa/isp internal DMA buffer */
	minfo->pb_taaisp_s = CALL_PTR_MEMOP(mem, alloc, mem->default_ctx,
				TAAISP_DMA_SIZE, "camera_heap",
				ION_FLAG_CACHED | ION_FLAG_PROTECTED);
	if (IS_ERR_OR_NULL(minfo->pb_taaisp_s)) {
		err("failed to allocate buffer for TAAISP_DMA_S");
		return -ENOMEM;
	}

	info("[RSC]memory_alloc(TAAISP_DMA_S): %08lx\n", TAAISP_DMA_SIZE);
#endif
#if defined(SECURE_CAMERA_MEDRC)
	/* ME/DRC buffer */
#if (MEDRC_DMA_SIZE > 0)
#if defined(SECURE_CAMERA_USE_EXT_HEAP)
	minfo->pb_medrc_s = CALL_PTR_MEMOP(mem, alloc, mem->default_ctx,
			MEDRC_DMA_SIZE, "secure_camera_heap_drc",
			ION_FLAG_CACHED | ION_FLAG_PROTECTED);
#else
	minfo->pb_medrc_s = CALL_PTR_MEMOP(mem, alloc, mem->default_ctx,
				MEDRC_DMA_SIZE, "secure_camera_heap",
				ION_FLAG_CACHED | ION_FLAG_PROTECTED);
#endif
	if (IS_ERR_OR_NULL(minfo->pb_medrc_s)) {
		CALL_VOID_BUFOP(minfo->pb_taaisp_s, free, minfo->pb_taaisp_s);
		err("failed to allocate buffer for ME_DRC_S");
		return -ENOMEM;
	}

	info("[RSC]memory_alloc(ME_DRC_S): %08lx\n", MEDRC_DMA_SIZE);
#endif
#endif
#if defined(SECURE_CAMERA_TNR)
	/* TNR secure internal DMA buffer */
	mem->default_ctx->heapmask_s = is_ion_query_heapmask("secure_camera_heap");
	if (!mem->default_ctx->heapmask_s) {
		err("can't find secure_camera_heap in ion");
		return -EINVAL;
	}

	minfo->pb_tnr_s = CALL_PTR_MEMOP(mem, alloc, mem->default_ctx,
				TNR_S_DMA_SIZE, "secure_camera_heap",
				ION_EXYNOS_FLAG_PROTECTED);
	if (IS_ERR_OR_NULL(minfo->pb_tnr_s)) {
		err("failed to allocate buffer for TNR_DMA_S");
		return -ENOMEM;
	}

	info("[RSC]memory_alloc(TNR_DMA_S): %08lx\n", TNR_S_DMA_SIZE);
#endif
	return 0;
}

static int is_resourcemgr_init_secure_mem(struct is_resourcemgr *resourcemgr)
{
	struct is_core *core;
	int ret = 0;

	core = container_of(resourcemgr, struct is_core, resourcemgr);
	FIMC_BUG(!core);

	if (core->scenario != IS_SCENARIO_SECURE)
		return ret;

	info("is_init_secure_mem - ION\n");

	ret = is_resourcemgr_alloc_secure_mem(resourcemgr);
	if (ret) {
		err("Couldn't alloc for FIMC-IS\n");
		ret = -ENOMEM;
		goto p_err;
	}

	info("[RSC] %s done\n", __func__);
p_err:
	return ret;
}

static int is_resourcemgr_deinit_secure_mem(struct is_resourcemgr *resourcemgr)
{
	struct is_minfo *minfo = &resourcemgr->minfo;
	struct is_core *core;
	int ret = 0;

	core = container_of(resourcemgr, struct is_core, resourcemgr);
	FIMC_BUG(!core);

	if (minfo->pb_taaisp_s)
		CALL_VOID_BUFOP(minfo->pb_taaisp_s, free, minfo->pb_taaisp_s);
	if (minfo->pb_medrc_s)
		CALL_VOID_BUFOP(minfo->pb_medrc_s, free, minfo->pb_medrc_s);
	if (minfo->pb_tnr_s)
		CALL_VOID_BUFOP(minfo->pb_tnr_s, free, minfo->pb_tnr_s);

	minfo->pb_taaisp_s = NULL;
	minfo->pb_medrc_s = NULL;
	minfo->pb_tnr_s = NULL;

	info("[RSC] %s done\n", __func__);

	return ret;
}
#endif

static struct vm_struct is_lib_vm;
#if !defined(ENABLE_DYNAMIC_HEAP_FOR_DDK_RTA)
static struct vm_struct is_heap_vm;
static struct vm_struct is_heap_rta_vm;
#endif
#if defined(RESERVED_MEM_IN_DT)
static int is_rmem_device_init(struct reserved_mem *rmem,
				    struct device *dev)
{
	WARN(1, "%s() should never be called!", __func__);
	return 0;
}

static void is_rmem_device_release(struct reserved_mem *rmem,
					struct device *dev)
{
}

static const struct reserved_mem_ops is_rmem_ops = {
	.device_init	= is_rmem_device_init,
	.device_release	= is_rmem_device_release,
};

static int __init is_reserved_mem_setup(struct reserved_mem *rmem)
{
	const __be32 *prop;
	u64 kbase;
	int len;
#ifdef CONFIG_RKP
	struct rkp_dynamic_load rkp_dyn;
	int ret = 0;
#endif

	prop = of_get_flat_dt_prop(rmem->fdt_node, "kernel_virt", &len);
	if (!prop) {
		pr_err("kernel_virt is not found in '%s' node\n", rmem->name);
		return -EINVAL;
	}

	if (len != dt_root_addr_cells * sizeof(__be32)) {
		pr_err("invalid kernel_virt property in '%s' node.\n",
			rmem->name);
		return -EINVAL;
	}

	kbase = dt_mem_next_cell(dt_root_addr_cells, &prop);

	is_lib_vm.phys_addr = rmem->base;
	is_lib_vm.addr = (void *)kbase;
	is_lib_vm.size = LIB_SIZE + PAGE_SIZE;

	BUG_ON(rmem->size < LIB_SIZE);

#ifdef CONFIG_RKP
	memcpy(&is_lib_vm_for_rkp, &is_lib_vm, sizeof(struct vm_struct));

	is_lib_vm_for_rkp.addr = (void *)rounddown((u64)is_lib_vm_for_rkp.addr, SECTION_SIZE);
	is_lib_vm_for_rkp.size = (u64)roundup(is_lib_vm_for_rkp.size, SECTION_SIZE);

	rkp_dyn.binary_base = is_lib_vm.phys_addr;
	rkp_dyn.binary_size = LIB_SIZE;
	rkp_dyn.type = RKP_DYN_COMMAND_BREAKDOWN;
	uh_call2(UH_APP_RKP, RKP_DYNAMIC_LOAD, RKP_DYN_COMMAND_BREAKDOWN, (u64)&rkp_dyn, (u64)&ret, 0);
	if (ret) {
		err_lib("fail to break-before-init FIMC in EL2");
	}
	vm_area_add_early(&is_lib_vm_for_rkp);
#else
	vm_area_add_early(&is_lib_vm);
#endif

	probe_info("is library memory: 0x%llx\n", kbase);

#if !defined(ENABLE_DYNAMIC_HEAP_FOR_DDK_RTA)
	is_heap_vm.addr = (void *)HEAP_START;
	is_heap_vm.size = HEAP_SIZE + PAGE_SIZE;

	vm_area_add_early(&is_heap_vm);

	probe_info("is heap memory: 0x%lx\n", HEAP_START);
#endif

	rmem->ops = &is_rmem_ops;

	return 0;
}
RESERVEDMEM_OF_DECLARE(is_lib, "exynos,is_lib", is_reserved_mem_setup);
#else
#ifndef MODULE
static int __init is_lib_mem_alloc(char *str)
{
	ulong addr = 0;
#ifdef CONFIG_RKP
	struct rkp_dynamic_load rkp_dyn;
	int ret = 0;
#endif

	if (kstrtoul(str, 0, (ulong *)&addr) || !addr) {
		probe_warn("invalid is library memory address, use default");
		addr = __LIB_START;
	}

	if (addr != __LIB_START)
		probe_warn("use different address [reserve-fimc=0x%lx default:0x%lx]",
				addr, __LIB_START);

	is_lib_vm.phys_addr = is_memblock_alloc(LIB_SIZE, SZ_2M);
	is_lib_vm.addr = (void *)addr;
	is_lib_vm.size = LIB_SIZE + PAGE_SIZE;
#ifdef CONFIG_RKP
	memcpy(&is_lib_vm_for_rkp, &is_lib_vm, sizeof(struct vm_struct));

	is_lib_vm_for_rkp.addr = (void *)rounddown((u64)is_lib_vm_for_rkp.addr, SECTION_SIZE);
	is_lib_vm_for_rkp.size = (u64)roundup(is_lib_vm_for_rkp.size, SECTION_SIZE);

	rkp_dyn.binary_base = is_lib_vm.phys_addr;
	rkp_dyn.binary_size = LIB_SIZE;
	rkp_dyn.type = RKP_DYN_COMMAND_BREAKDOWN;
	uh_call2(UH_APP_RKP, RKP_DYNAMIC_LOAD, RKP_DYN_COMMAND_BREAKDOWN, (u64)&rkp_dyn, (u64)&ret, 0);
	if (ret) {
		err_lib("fail to break-before-init FIMC in EL2");
	}
	vm_area_add_early(&is_lib_vm_for_rkp);
#else
	vm_area_add_early(&is_lib_vm);
#endif

	probe_info("is library memory: 0x%lx\n", addr);

#if !defined(ENABLE_DYNAMIC_HEAP_FOR_DDK_RTA)
	is_heap_vm.addr = (void *)HEAP_START;
	is_heap_vm.size = HEAP_SIZE + PAGE_SIZE;

	vm_area_add_early(&is_heap_vm);

	probe_info("is heap DDK memory: 0x%lx\n", HEAP_START);

	is_heap_rta_vm.addr = (void *)HEAP_RTA_START;
	is_heap_rta_vm.size = HEAP_RTA_SIZE + PAGE_SIZE;

	vm_area_add_early(&is_heap_rta_vm);

	probe_info("is heap RTA memory: 0x%lx\n", HEAP_RTA_START);
#endif
	return 0;
}
__setup("reserve-fimc=", is_lib_mem_alloc);
#endif /* ifndef MODULE */
#endif

static int is_mem_map_vm(struct is_resourcemgr *resourcemgr,
	struct vm_struct *vm, int heap_size, pgprot_t prot)
{
	struct is_mem *mem = &resourcemgr->mem;
	struct is_priv_buf *pb;
	struct scatterlist *sg;
	struct sg_table *table;
	int i, j;
	int npages = vm->size / PAGE_SIZE;
	struct page **pages = vmalloc(sizeof(struct page *) * npages);
	struct page **tmp = pages;

	if (heap_size == 0) {
		warn("heap size is zero");
		vfree(pages);
		return 0;
	} else if (!pages) {
		probe_err("Failed to vmalloc. pages %d", npages);
		return -ENOMEM;
	}

	pb = CALL_PTR_MEMOP(mem, alloc, mem->default_ctx, heap_size, NULL, 0);
	if (IS_ERR_OR_NULL(pb)) {
		err("failed to allocate buffer for HEAP");
		vfree(pages);
		return -ENOMEM;
	}

	table = pb->sgt;

	for_each_sg(table->sgl, sg, table->orig_nents, i) {
		int npages_this_entry = PAGE_ALIGN(sg->length) / PAGE_SIZE;
		struct page *page = sg_page(sg);

		BUG_ON(i >= npages);

		for (j = 0; j < npages_this_entry; j++)
			*(tmp++) = page++;
	}
	if (map_vm_area(vm, prot, pages)) {
		probe_err("failed to mapping between virt and phys for binary");
		vfree(pages);
		CALL_VOID_BUFOP(pb, free, pb);
		return -ENOMEM;
	}

	vfree(pages);

	return 0;
}

static int is_lib_mem_map(struct is_resourcemgr *resourcemgr)
{
	int ret;
	int page_size, i;
	struct page *page;
	struct page **pages;
	pgprot_t prot;

#ifndef CONFIG_DISABLE_CAMERA_MEM_ATTR
	prot = PAGE_KERNEL;
#else
	prot = PAGE_KERNEL_EXEC;
#endif

	if (!is_lib_vm.phys_addr) {
		probe_warn("There is no reserve memory for DDK_LIB. Try to alloc");

		ret = is_mem_map_vm(resourcemgr, &is_lib_vm, LIB_SIZE, prot);
		return ret;
	}

	if (is_lib_vm.size < LIB_SIZE) {
		probe_err("is_lib_vm.size is not enough (0x%lx < 0x%lx)",
			is_lib_vm.size, LIB_SIZE);
		return -ENOMEM;
	}

	page_size = is_lib_vm.size / PAGE_SIZE;
	pages = kzalloc(sizeof(struct page*) * page_size, GFP_KERNEL);
	page = phys_to_page(is_lib_vm.phys_addr);

	for (i = 0; i < page_size; i++)
		pages[i] = page++;

	if (map_vm_area(&is_lib_vm, prot, pages)) {
		probe_err("failed to mapping between virt and phys for binary");
		kfree(pages);
		return -ENOMEM;
	}

	kfree(pages);

	return 0;
}

#if defined(ENABLE_DYNAMIC_HEAP_FOR_DDK_RTA)
int is_heap_mem_alloc_dynamic(struct is_resourcemgr *resourcemgr,
	int type, int heap_size)
{
	struct is_mem *mem = &resourcemgr->mem;
	struct is_minfo *minfo = &resourcemgr->minfo;

	if (heap_size == 0) {
		warn("heap size is zero");
		return 0;
	}

	if (type == IS_BIN_LIB_HINT_DDK) {
		if (minfo->kvaddr_heap_ddk) {
			info_lib("DDK heap is already allocated(addr:0x%pK), use it", minfo->kvaddr_heap_ddk);
			return 0;
		}

		minfo->pb_heap_ddk = CALL_PTR_MEMOP(mem, alloc, mem->default_ctx, heap_size, NULL, 0);
		if (IS_ERR_OR_NULL(minfo->pb_heap_ddk)) {
			err("failed to allocate buffer for DDK HEAP");
			return -ENOMEM;
		}

		minfo->kvaddr_heap_ddk = CALL_BUFOP(minfo->pb_heap_ddk, kvaddr, minfo->pb_heap_ddk);

		info_lib("memory_alloc(DDK heap)(V/S): 0x%pK/0x%x", minfo->kvaddr_heap_ddk, heap_size);
	} else if (type == IS_BIN_LIB_HINT_RTA) {
		if (minfo->kvaddr_heap_rta) {
			info_lib("RTA heap is already allocated(addr:0x%pK), use it", minfo->kvaddr_heap_rta);
			return 0;
		}

		minfo->pb_heap_rta = CALL_PTR_MEMOP(mem, alloc, mem->default_ctx, heap_size, NULL, 0);
		if (IS_ERR_OR_NULL(minfo->pb_heap_rta)) {
			err("failed to allocate buffer for RTA HEAP");
			return -ENOMEM;
		}

		minfo->kvaddr_heap_rta = CALL_BUFOP(minfo->pb_heap_rta, kvaddr, minfo->pb_heap_rta);

		info_lib("memory_alloc(RTA heap)(V/S): 0x%pK/0x%x", minfo->kvaddr_heap_rta, heap_size);
	}

	return 0;
}

int is_heap_mem_free(struct is_resourcemgr *resourcemgr)
{
	struct is_minfo *minfo = &resourcemgr->minfo;
	int ret = 0;

	if (IS_ENABLED(DISABLE_DDK_HEAP_FREE))
		return 0;

	if (minfo->pb_heap_ddk)
		CALL_VOID_BUFOP(minfo->pb_heap_ddk, free, minfo->pb_heap_ddk);
	if (minfo->pb_heap_rta)
		CALL_VOID_BUFOP(minfo->pb_heap_rta, free, minfo->pb_heap_rta);

	minfo->pb_taaisp_s = NULL;
	minfo->pb_medrc_s = NULL;
	minfo->kvaddr_heap_ddk = 0;
	minfo->kvaddr_heap_rta = 0;

	info("[RSC] %s done\n", __func__);

	return ret;
}
#endif

void is_bts_scen(struct is_resourcemgr *resourcemgr,
	unsigned int index, bool enable)
{
#if IS_ENABLED(CONFIG_EXYNOS_BTS)
	int ret = 0;
	unsigned int scen_idx;
	const char *name;

	if (resourcemgr->bts_scen != NULL)
		name = resourcemgr->bts_scen[index].name;
	else {
		err("%s : null bts_scen struct\n", __func__);
		return;
	}

	scen_idx = bts_get_scenindex(name);
	if (scen_idx) {
		if (enable)
			ret = bts_add_scenario(scen_idx);
		else
			ret = bts_del_scenario(scen_idx);

		if (ret) {
			err("call bts_%s_scenario is fail (%d:%s)\n",
				enable ? "add" : "del", scen_idx, name);
		} else {
			info("call bts_%s_scenario (%d:%s)\n",
				enable ? "add" : "del", scen_idx, name);
		}
	}
#endif
}

#if IS_ENABLED(CONFIG_EXYNOS_THERMAL) || IS_ENABLED(CONFIG_EXYNOS_THERMAL_V2)
static int is_tmu_notifier(struct notifier_block *nb,
	unsigned long state, void *data)
{
	int ret = 0, fps = 0;
	struct is_resourcemgr *resourcemgr;
	struct is_core *core = NULL;
	struct is_dvfs_ctrl *dvfs_ctrl;
#if IS_ENABLED(CONFIG_EXYNOS_SNAPSHOT_THERMAL) || IS_ENABLED(CONFIG_DEBUG_SNAPSHOT)
	char *cooling_device_name = "ISP";
#endif
	resourcemgr = container_of(nb, struct is_resourcemgr, tmu_notifier);
	dvfs_ctrl = &(resourcemgr->dvfs_ctrl);

	core = (struct is_core *)dev_get_drvdata(is_dev);
	if (!core)
		return ret;

	if (!atomic_read(&core->rsccount))
		warn("[RSC] Camera is not opened");

	switch (state) {
	case ISP_NORMAL:
		resourcemgr->tmu_state = ISP_NORMAL;
		resourcemgr->limited_fps = 0;
		break;
	case ISP_COLD:
		resourcemgr->tmu_state = ISP_COLD;
		resourcemgr->limited_fps = 0;
		break;
	case ISP_THROTTLING:
		resourcemgr->tmu_state = ISP_THROTTLING;
		fps = isp_cooling_get_fps(0, *(unsigned long *)data);
		mutex_lock(&dvfs_ctrl->lock);
		atomic_set(&dvfs_ctrl->thrott_state, 1);
		mutex_unlock(&dvfs_ctrl->lock);
		/* The FPS can be defined to any specific value. */
		if (fps >= 60) {
			resourcemgr->limited_fps = 0;
			if (IS_ENABLED(THROTTLING_INTCAM_ENABLE)) {
				dvfs_ctrl->thr_int_cam_qos = 0;
				dvfs_ctrl->cur_int_cam_qos = 0;
			}
			if (IS_ENABLED(THROTTLING_INT_ENABLE)) {
				dvfs_ctrl->thr_int_qos = 0;
				dvfs_ctrl->cur_int_qos = 0;
			}
			if (IS_ENABLED(THROTTLING_MIF_ENABLE)) {
				dvfs_ctrl->thr_mif_qos = 0;
				dvfs_ctrl->cur_mif_qos = 0;
			}
			warn("[RSC] THROTTLING : Unlimited FPS");
		} else {
			resourcemgr->limited_fps = fps;
			warn("[RSC] THROTTLING : Limited %d FPS", fps);
		}
		break;
	case ISP_TRIPPING:
		resourcemgr->tmu_state = ISP_TRIPPING;
		resourcemgr->limited_fps = 5;
		warn("[RSC] TRIPPING : Limited 5FPS");
		break;
	default:
		err("[RSC] invalid tmu state(%ld)", state);
		break;
	}

#if IS_ENABLED(CONFIG_EXYNOS_SNAPSHOT_THERMAL)
	exynos_ss_thermal(NULL, 0, cooling_device_name, resourcemgr->limited_fps);
#elif IS_ENABLED(CONFIG_DEBUG_SNAPSHOT)
	dbg_snapshot_thermal(NULL, 0, cooling_device_name, resourcemgr->limited_fps);
#endif

	return ret;
}
#endif

#ifdef CONFIG_EXYNOS_ITMON
static int due_to_is(const char *desc)
{
	if (desc && (strstr((char *)desc, "CAM")
				|| strstr((char *)desc, "ISP")
				|| strstr((char *)desc, "CSIS")
				|| strstr((char *)desc, "IPP")
				|| strstr((char *)desc, "TNR")
				|| strstr((char *)desc, "DNS")
				|| strstr((char *)desc, "MCSC")
				|| strstr((char *)desc, "VRA")))
			return 1;

	return 0;
}

static int is_itmon_notifier(struct notifier_block *nb,
	unsigned long state, void *data)
{
	int i;
	struct is_core *core;
	struct is_resourcemgr *resourcemgr;
	struct itmon_notifier *itmon;

	resourcemgr = container_of(nb, struct is_resourcemgr, itmon_notifier);
	core = container_of(resourcemgr, struct is_core, resourcemgr);
	itmon = (struct itmon_notifier *)data;

	if (!itmon)
		return NOTIFY_DONE;

	if (due_to_is(itmon->port)
			|| due_to_is(itmon->dest)
			|| due_to_is(itmon->master)) {
		info("1. NOC info.\n");
		info("%s: init description : %s\n", __func__, itmon->port);
		info("%s: target descrition: %s\n", __func__, itmon->dest);
		info("%s: user description : %s\n", __func__, itmon->master);
		info("%s: Transaction Type : %s\n", __func__, itmon->read ? "READ" : "WRITE");
		info("%s: target address   : %lx\n",__func__, itmon->target_addr);
		info("%s: Power Domain     : %s(%d)\n",__func__, itmon->pd_name, itmon->onoff);

		for (i = 0; i < IS_STREAM_COUNT; ++i) {
			if (!test_bit(IS_ISCHAIN_POWER_ON, &core->ischain[i].state))
				continue;

			info("2. FW log dump\n");
			is_hw_logdump(&core->interface);

			info("3. Clock info.\n");
			schedule_work(&core->wq_data_print_clk);
		}

		return NOTIFY_STOP;
	}

	return NOTIFY_DONE;
}
#endif /* CONFIG_EXYNOS_ITMON */

#ifdef ENABLE_FW_SHARE_DUMP
static int is_fw_share_dump(void)
{
	int ret = 0;
	u8 *buf;
	struct is_core *core = NULL;
	struct is_resourcemgr *resourcemgr;

	core = (struct is_core *)dev_get_drvdata(is_dev);
	if (!core)
		goto p_err;

	resourcemgr = &core->resourcemgr;
	buf = (u8 *)resourcemgr->fw_share_dump_buf;

	/* dump share region in fw area */
	if (IS_ERR_OR_NULL(buf)) {
		err("%s: fail to alloc", __func__);
		ret = -ENOMEM;
		goto p_err;
	}

	/* sync with fw for memory */
	vb2_ion_sync_for_cpu(resourcemgr->minfo.fw_cookie, 0,
			SHARED_OFFSET, DMA_BIDIRECTIONAL);

	memcpy(buf, (u8 *)resourcemgr->minfo.kvaddr_fshared, SHARED_SIZE);

	info("%s: dumped ramdump addr(virt/phys/size): (%p/%p/0x%X)", __func__, buf,
			(void *)virt_to_phys(buf), SHARED_SIZE);
p_err:
	return ret;
}
#endif

#if defined(ENABLE_CLOG_RESERVED_MEM)
int cdump(const char *str, ...)
{
	struct is_core *core = NULL;
	struct is_resourcemgr *rscmgr = NULL;
	int ret = 0;
	int size = 0;
	int cpu = raw_smp_processor_id();
	unsigned long long t;
	unsigned long usec;
	va_list args;
	unsigned long flag;
	char dlog[MAX_LIB_LOG_BUF];
	char string[MAX_LIB_LOG_BUF];

	core = (struct is_core *)dev_get_drvdata(is_dev);
	if (!core)
		return -EINVAL;
	rscmgr = &core->resourcemgr;

	va_start(args, str);
	size += vscnprintf(string + size, sizeof(string) - size, str, args);
	va_end(args);

	t = local_clock();
	usec = do_div(t, NSEC_PER_SEC) / NSEC_PER_USEC;
	size = snprintf(dlog, sizeof(dlog), "[%5lu.%06lu] [%d][#] %s",
		(unsigned long)t, usec, cpu, string);
	if (size > MAX_LIB_LOG_BUF)
		size = MAX_LIB_LOG_BUF;

	spin_lock_irqsave(&rscmgr->slock_cdump, flag);

	if (rscmgr->cdump_ptr == 0)
		rscmgr->cdump_ptr =
			rscmgr->minfo.kvaddr_debug + DEBUG_REGION_SIZE;
	else if ((rscmgr->cdump_ptr + size) >=
		(rscmgr->minfo.kvaddr_debug +
			DEBUG_REGION_SIZE + DEBUG_DUMP_SIZE))
		rscmgr->cdump_ptr =
			rscmgr->minfo.kvaddr_debug + DEBUG_REGION_SIZE;

	memcpy((void *)rscmgr->cdump_ptr, (void *)dlog, size);
	rscmgr->cdump_ptr += size;

	spin_unlock_irqrestore(&rscmgr->slock_cdump, flag);

	return ret;
}

void hex_cdump(const char *level, const char *prefix_str, int prefix_type,
		    int rowsize, int groupsize,
		    const void *buf, size_t len, bool ascii)
{
	const u8 *ptr = buf;
	int i, linelen, remaining = len;
	unsigned char linebuf[32 * 3 + 2 + 32 + 1];

	if (rowsize != 16 && rowsize != 32)
		rowsize = 16;

	for (i = 0; i < (int)len; i += rowsize) {
		linelen = min(remaining, rowsize);
		remaining -= rowsize;

		hex_dump_to_buffer(ptr + i, linelen, rowsize, groupsize,
				   linebuf, sizeof(linebuf), ascii);

		switch (prefix_type) {
		case DUMP_PREFIX_ADDRESS:
			cinfo("%s%p: %s\n", prefix_str, ptr + i, linebuf);
			break;
		case DUMP_PREFIX_OFFSET:
			cinfo("%s%.8x: %s\n", prefix_str, i, linebuf);
			break;
		default:
			cinfo("%s%s\n", prefix_str, linebuf);
			break;
		}
	}
}

int is_resource_cdump(void)
{
	struct is_core *core = NULL;
	struct is_group *group;
	struct is_subdev *subdev;
	struct is_framemgr *framemgr;
	struct is_groupmgr *groupmgr;
	struct is_device_ischain *device = NULL;
	struct is_device_csi *csi;
	int i, j;

	core = (struct is_core *)dev_get_drvdata(is_dev);
	if (!core)
		goto exit;

	if (atomic_read(&core->rsccount) == 0)
		goto exit;

	info("### %s dump start ###\n", __func__);
	cinfo("###########################\n");
	cinfo("###########################\n");
	cinfo("### %s dump start ###\n", __func__);

	groupmgr = &core->groupmgr;

	/* dump all CR by using memlogger */
	is_debug_memlog_dump_cr_all(MEMLOG_LEVEL_EMERG);

	/* dump per core */
	for (i = 0; i < IS_STREAM_COUNT; ++i) {
		device = &core->ischain[i];
		if (!test_bit(IS_ISCHAIN_OPEN, &device->state))
			continue;

		if (test_bit(IS_ISCHAIN_CLOSING, &device->state))
			continue;

		/* clock & gpio dump */
		if (!in_interrupt())
			exynos_is_dump_clk(&core->pdev->dev);
		cinfo("### 2. SFR DUMP ###\n");
		cinfo("Base: 0x%pK\n", core->resourcemgr.minfo.kvaddr_debug);
		core->resourcemgr.sfrdump_ptr =
			core->resourcemgr.minfo.kvaddr_debug
			+ DEBUG_REGION_SIZE + DEBUG_DUMP_SIZE;
		cinfo("Dump base: 0x%pK\n", core->resourcemgr.sfrdump_ptr);
		break;
	}

	/* dump per ischain */
	for (i = 0; i < IS_STREAM_COUNT; ++i) {
		device = &core->ischain[i];
		if (!test_bit(IS_ISCHAIN_OPEN, &device->state))
			continue;

		if (test_bit(IS_ISCHAIN_CLOSING, &device->state))
			continue;

		if (device->sensor && !test_bit(IS_ISCHAIN_REPROCESSING, &device->state)) {
			csi = (struct is_device_csi *)v4l2_get_subdevdata(device->sensor->subdev_csi);
			if (csi)
				csi_hw_cdump_all(csi);
		}

		cinfo("### 3. DUMP frame manager ###\n");

		/* dump all framemgr */
		group = groupmgr->leader[i];
		while (group) {
			if (!test_bit(IS_GROUP_OPEN, &group->state))
				break;

			for (j = 0; j < ENTRY_END; j++) {
				subdev = group->subdev[j];
				if (subdev && test_bit(IS_SUBDEV_START, &subdev->state)) {
					framemgr = GET_SUBDEV_FRAMEMGR(subdev);
					if (framemgr) {
						unsigned long flags;

						cinfo("[%d][%s] framemgr dump\n",
							subdev->instance, subdev->name);
						framemgr_e_barrier_irqs(framemgr, 0, flags);
						frame_manager_dump_queues(framemgr);
						framemgr_x_barrier_irqr(framemgr, 0, flags);
					}
				}
			}

			group = group->next;
		}
	}


	/* dump per core */
	for (i = 0; i < IS_STREAM_COUNT; ++i) {
		device = &core->ischain[i];
		if (!test_bit(IS_ISCHAIN_OPEN, &device->state))
			continue;

		if (test_bit(IS_ISCHAIN_CLOSING, &device->state))
			continue;

		is_hardware_sfr_dump(&core->hardware, DEV_HW_END, false);
		break;
	}
	cinfo("#####################\n");
	cinfo("#####################\n");
	cinfo("### %s dump end ###\n", __func__);
	info("### %s dump end (refer camera dump)###\n", __func__);
exit:
	return 0;
}
#endif

#ifdef ENABLE_KERNEL_LOG_DUMP
int is_kernel_log_dump(bool overwrite)
{
	static int dumped = 0;
	struct is_core *core;
	struct is_resourcemgr *resourcemgr;
	void *log_kernel = NULL;
	unsigned long long when;
	unsigned long usec;

	core = (struct is_core *)dev_get_drvdata(is_dev);
	if (!core)
		return -EINVAL;

	resourcemgr = &core->resourcemgr;

	if (dumped && !overwrite) {
		when = resourcemgr->kernel_log_time;
		usec = do_div(when, NSEC_PER_SEC) / NSEC_PER_USEC;
		info("kernel log was saved already at [%5lu.%06lu]\n",
				(unsigned long)when, usec);

		return -ENOSPC;
	}

#if IS_ENABLED(CONFIG_EXYNOS_SNAPSHOT)
	log_kernel = (void *)exynos_ss_get_item_vaddr("log_kernel");
#elif IS_ENABLED(CONFIG_DEBUG_SNAPSHOT)
	log_kernel = (void *)dbg_snapshot_get_item_vaddr("log_kernel");
#endif
	if (!log_kernel)
		return -EINVAL;

	if (resourcemgr->kernel_log_buf) {
		resourcemgr->kernel_log_time = local_clock();

		info("kernel log saved to %p(%p) from %p\n",
				resourcemgr->kernel_log_buf,
				(void *)virt_to_phys(resourcemgr->kernel_log_buf),
				log_kernel);
#if IS_ENABLED(CONFIG_EXYNOS_SNAPSHOT)
		memcpy(resourcemgr->kernel_log_buf, log_kernel,
				exynos_ss_get_item_size("log_kernel"));
#elif IS_ENABLED(CONFIG_DEBUG_SNAPSHOT)
		memcpy(resourcemgr->kernel_log_buf, log_kernel,
				dbg_snapshot_get_item_size("log_kernel"));
#endif

		dumped = 1;
	}

	return 0;
}
#endif

int is_resource_dump(void)
{
	struct is_core *core = NULL;
	struct is_group *group;
	struct is_subdev *subdev;
	struct is_framemgr *framemgr;
	struct is_groupmgr *groupmgr;
	struct is_device_ischain *device = NULL;
	struct is_device_csi *csi;
	int i, j;

	core = (struct is_core *)dev_get_drvdata(is_dev);
	if (!core)
		goto exit;

	info("### %s dump start ###\n", __func__);
	groupmgr = &core->groupmgr;

	/* dump per core */
	for (i = 0; i < IS_STREAM_COUNT; ++i) {
		device = &core->ischain[i];
		if (!test_bit(IS_ISCHAIN_OPEN, &device->state))
			continue;

		if (test_bit(IS_ISCHAIN_CLOSING, &device->state))
			continue;

		/* clock & gpio dump */
		schedule_work(&core->wq_data_print_clk);
		/* ddk log dump */
		is_lib_logdump();
		is_hardware_sfr_dump(&core->hardware, DEV_HW_END, false);
		break;
	}

	/* dump per ischain */
	for (i = 0; i < IS_STREAM_COUNT; ++i) {
		device = &core->ischain[i];
		if (!test_bit(IS_ISCHAIN_OPEN, &device->state))
			continue;

		if (test_bit(IS_ISCHAIN_CLOSING, &device->state))
			continue;

		if (device->sensor && !test_bit(IS_ISCHAIN_REPROCESSING, &device->state)) {
			csi = (struct is_device_csi *)v4l2_get_subdevdata(device->sensor->subdev_csi);
			if (csi)
				csi_hw_dump_all(csi);
		}

		/* dump all framemgr */
		group = groupmgr->leader[i];
		while (group) {
			if (!test_bit(IS_GROUP_OPEN, &group->state))
				break;

			for (j = 0; j < ENTRY_END; j++) {
				subdev = group->subdev[j];
				if (subdev && test_bit(IS_SUBDEV_START, &subdev->state)) {
					framemgr = GET_SUBDEV_FRAMEMGR(subdev);
					if (framemgr) {
						unsigned long flags;
						mserr(" dump framemgr..", subdev, subdev);
						framemgr_e_barrier_irqs(framemgr, 0, flags);
						frame_manager_print_queues(framemgr);
						framemgr_x_barrier_irqr(framemgr, 0, flags);
					}
				}
			}

			group = group->next;
		}
	}

	info("### %s dump end ###\n", __func__);
exit:
	return 0;
}

#ifdef ENABLE_PANIC_HANDLER
static int is_panic_handler(struct notifier_block *nb, ulong l,
	void *buf)
{
#if defined(ENABLE_CLOG_RESERVED_MEM)
	is_resource_cdump();
#else
	is_resource_dump();
#endif
#ifdef ENABLE_FW_SHARE_DUMP
	/* dump share area in fw region */
	is_fw_share_dump();
#endif
	return 0;
}

static struct notifier_block notify_panic_block = {
	.notifier_call = is_panic_handler,
};
#endif

#if defined(ENABLE_REBOOT_HANDLER)
static int is_reboot_handler(struct notifier_block *nb, ulong l,
	void *buf)
{
	struct is_core *core = NULL;

	info("%s enter\n", __func__);

	core = (struct is_core *)dev_get_drvdata(is_dev);
	if (core)
		is_cleanup(core);

	info("%s exit\n", __func__);

	return 0;
}

static struct notifier_block notify_reboot_block = {
	.notifier_call = is_reboot_handler,
};
#endif

#if defined(DISABLE_CORE_IDLE_STATE)
static void is_resourcemgr_c2_disable_work(struct work_struct *data)
{
	/* CPU idle on/off */
	info("%s: call cpuidle_pause()\n", __func__);
	cpuidle_pause_and_lock();
}
#endif

int is_resourcemgr_probe(struct is_resourcemgr *resourcemgr,
	void *private_data, struct platform_device *pdev)
{
	int ret = 0;
	struct device_node *np;

	FIMC_BUG(!resourcemgr);
	FIMC_BUG(!private_data);

	np = pdev->dev.of_node;
	resourcemgr->private_data = private_data;

	clear_bit(IS_RM_COM_POWER_ON, &resourcemgr->state);
	clear_bit(IS_RM_SS0_POWER_ON, &resourcemgr->state);
	clear_bit(IS_RM_SS1_POWER_ON, &resourcemgr->state);
	clear_bit(IS_RM_SS2_POWER_ON, &resourcemgr->state);
	clear_bit(IS_RM_SS3_POWER_ON, &resourcemgr->state);
	clear_bit(IS_RM_SS4_POWER_ON, &resourcemgr->state);
	clear_bit(IS_RM_SS5_POWER_ON, &resourcemgr->state);
	clear_bit(IS_RM_ISC_POWER_ON, &resourcemgr->state);
	clear_bit(IS_RM_POWER_ON, &resourcemgr->state);
	atomic_set(&resourcemgr->rsccount, 0);
	atomic_set(&resourcemgr->qos_refcount, 0);
	atomic_set(&resourcemgr->resource_sensor0.rsccount, 0);
	atomic_set(&resourcemgr->resource_sensor1.rsccount, 0);
	atomic_set(&resourcemgr->resource_sensor2.rsccount, 0);
	atomic_set(&resourcemgr->resource_sensor3.rsccount, 0);
	atomic_set(&resourcemgr->resource_ischain.rsccount, 0);
	atomic_set(&resourcemgr->resource_preproc.rsccount, 0);

	resourcemgr->cluster0 = 0;
	resourcemgr->cluster1 = 0;
	resourcemgr->cluster2 = 0;
	resourcemgr->hal_version = IS_HAL_VER_1_0;

	/* rsc mutex init */
	mutex_init(&resourcemgr->rsc_lock);
	mutex_init(&resourcemgr->sysreg_lock);
	mutex_init(&resourcemgr->qos_lock);

	/* bus monitor unit */

#ifdef CONFIG_EXYNOS_ITMON
	resourcemgr->itmon_notifier.notifier_call = is_itmon_notifier;
	resourcemgr->itmon_notifier.priority = 0;

	itmon_notifier_chain_register(&resourcemgr->itmon_notifier);
#endif

#if IS_ENABLED(CONFIG_EXYNOS_THERMAL) || IS_ENABLED(CONFIG_EXYNOS_THERMAL_V2)
	/* temperature monitor unit */
	resourcemgr->tmu_notifier.notifier_call = is_tmu_notifier;
	resourcemgr->tmu_notifier.priority = 0;
	resourcemgr->tmu_state = ISP_NORMAL;
	resourcemgr->limited_fps = 0;
	resourcemgr->streaming_cnt = 0;

	ret = exynos_tmu_isp_add_notifier(&resourcemgr->tmu_notifier);
	if (ret) {
		probe_err("exynos_tmu_isp_add_notifier is fail(%d)", ret);
		goto p_err;
	}
#endif

#if defined(MODULE)
	/* get reserved memory from dt */
	{
		struct device_node *rmem_np;
		struct reserved_mem *crmem_dt;
		struct vm_struct *vm;

#if defined(ENABLE_CLOG_RESERVED_MEM)
		rmem_np = of_parse_phandle(np, "memory-region", 0);
		if (rmem_np) {
			crmem_dt = of_reserved_mem_lookup(rmem_np);
			if (!crmem_dt) {
				probe_err("failed to acquire memory region for crmem\n");
				return -ENOMEM;
			}

			crmem.phys_addr = crmem_dt->base;
			crmem.size = crmem_dt->size;

			info("%s: memory_alloc(reserved) for CLOG(P/S): 0x%lx/0x%lx\n",
					__func__, crmem.phys_addr, crmem.size);
		}
#endif /* if defined(ENABLE_CLOG_RESERVED_MEM) */

		rmem_np = of_parse_phandle(np, "memory-region", 1);
		if (rmem_np) {
			crmem_dt = of_reserved_mem_lookup(rmem_np);
			if (!crmem_dt) {
				probe_err("failed to acquire memory region for crmem\n");
				return -ENOMEM;
			}

			vm = __get_vm_area(crmem_dt->size + PAGE_SIZE, 0, LIB_START, VMALLOC_END);

			is_lib_vm.phys_addr = crmem_dt->base;
			is_lib_vm.addr = vm->addr;
			is_lib_vm.size = vm->size;
		} else {
			is_lib_vm.phys_addr = 0;
			is_lib_vm.addr = (void *)__LIB_START;
			is_lib_vm.size = LIB_SIZE + PAGE_SIZE;
		}

		info("%s: memory_alloc(reserved) for DDK_LIB(V/P/S): 0x%lx/0x%llx/0x%lx\n",
			__func__, is_lib_vm.addr, is_lib_vm.phys_addr, is_lib_vm.size);
	}

#if !defined(ENABLE_DYNAMIC_HEAP_FOR_DDK_RTA)
	is_heap_vm.addr = (void *)HEAP_START;
	is_heap_vm.size = HEAP_SIZE + PAGE_SIZE;
	is_heap_rta_vm.addr = (void *)HEAP_RTA_START;
	is_heap_rta_vm.size = HEAP_RTA_SIZE + PAGE_SIZE;
#endif /* if !defined(ENABLE_DYNAMIC_HEAP_FOR_DDK_RTA) */
#endif /* if defined(MODULE) */

#ifdef ENABLE_RESERVED_MEM
	ret = is_resourcemgr_initmem(resourcemgr);
	if (ret) {
		probe_err("is_resourcemgr_initmem is fail(%d)", ret);
		goto p_err;
	}
#endif

	ret = is_lib_mem_map(resourcemgr);
	if (ret) {
		probe_err("is_lib_mem_map is fail(%d)", ret);
		goto p_err;
	}

#if !defined(ENABLE_DYNAMIC_HEAP_FOR_DDK_RTA)
	ret = is_mem_map_vm(resourcemgr, &is_heap_vm, HEAP_SIZE, PAGE_KERNEL);
	if (ret) {
		probe_err("is_mem_map_vm for HEAP_DDK is fail(%d)", ret);
		goto p_err;
	}

	ret = is_mem_map_vm(resourcemgr, &is_heap_rta_vm, HEAP_RTA_SIZE, PAGE_KERNEL);
	if (ret) {
		probe_err("is_mem_map_vm for HEAP_RTA is fail(%d)", ret);
		goto p_err;
	}
#endif
#ifdef ENABLE_DVFS
	/* dvfs controller init */
	ret = is_dvfs_init(resourcemgr);
	if (ret) {
		probe_err("%s: is_dvfs_init failed!\n", __func__);
		goto p_err;
	}
#endif

#ifdef ENABLE_PANIC_HANDLER
	atomic_notifier_chain_register(&panic_notifier_list, &notify_panic_block);
#endif
#if defined(ENABLE_REBOOT_HANDLER)
	register_reboot_notifier(&notify_reboot_block);
#endif
#ifdef ENABLE_SHARED_METADATA
	spin_lock_init(&resourcemgr->shared_meta_lock);
#endif
#ifdef ENABLE_FW_SHARE_DUMP
	/* to dump share region in fw area */
	resourcemgr->fw_share_dump_buf = (ulong)kzalloc(SHARED_SIZE, GFP_KERNEL);
#endif
#if IS_ENABLED(CONFIG_CMU_EWF)
	get_cmuewf_index(np, &idx_ewf);
#endif

#ifdef ENABLE_KERNEL_LOG_DUMP
#if IS_ENABLED(CONFIG_EXYNOS_SNAPSHOT)
	resourcemgr->kernel_log_buf = kzalloc(exynos_ss_get_item_size("log_kernel"),
						GFP_KERNEL);
#elif IS_ENABLED(CONFIG_DEBUG_SNAPSHOT)
	resourcemgr->kernel_log_buf = kzalloc(dbg_snapshot_get_item_size("log_kernel"),
						GFP_KERNEL);
#endif
#endif

	mutex_init(&resourcemgr->global_param.lock);
	resourcemgr->global_param.video_mode = 0;
	resourcemgr->global_param.state = 0;

#if defined(DISABLE_CORE_IDLE_STATE)
	INIT_WORK(&resourcemgr->c2_disable_work, is_resourcemgr_c2_disable_work);
#endif

	INIT_LIST_HEAD(&resourcemgr->regulator_list);

p_err:
	probe_info("[RSC] %s(%d)\n", __func__, ret);
	return ret;
}

int is_resource_open(struct is_resourcemgr *resourcemgr, u32 rsc_type, void **device)
{
	int ret = 0;
	u32 stream;
	void *result;
	struct is_resource *resource;
	struct is_core *core;
	struct is_device_ischain *ischain;

	FIMC_BUG(!resourcemgr);
	FIMC_BUG(!resourcemgr->private_data);
	FIMC_BUG(rsc_type >= RESOURCE_TYPE_MAX);

	result = NULL;
	core = (struct is_core *)resourcemgr->private_data;
	resource = GET_RESOURCE(resourcemgr, rsc_type);
	if (!resource) {
		err("[RSC] resource is NULL");
		ret = -EINVAL;
		goto p_err;
	}

	switch (rsc_type) {
	case RESOURCE_TYPE_SENSOR0:
		result = &core->sensor[RESOURCE_TYPE_SENSOR0];
		resource->pdev = core->sensor[RESOURCE_TYPE_SENSOR0].pdev;
		break;
	case RESOURCE_TYPE_SENSOR1:
		result = &core->sensor[RESOURCE_TYPE_SENSOR1];
		resource->pdev = core->sensor[RESOURCE_TYPE_SENSOR1].pdev;
		break;
	case RESOURCE_TYPE_SENSOR2:
		result = &core->sensor[RESOURCE_TYPE_SENSOR2];
		resource->pdev = core->sensor[RESOURCE_TYPE_SENSOR2].pdev;
		break;
	case RESOURCE_TYPE_SENSOR3:
		result = &core->sensor[RESOURCE_TYPE_SENSOR3];
		resource->pdev = core->sensor[RESOURCE_TYPE_SENSOR3].pdev;
		break;
#if (IS_SENSOR_COUNT > 4)
	case RESOURCE_TYPE_SENSOR4:
		result = &core->sensor[RESOURCE_TYPE_SENSOR4];
		resource->pdev = core->sensor[RESOURCE_TYPE_SENSOR4].pdev;
		break;
#if (IS_SENSOR_COUNT > 5)
	case RESOURCE_TYPE_SENSOR5:
		result = &core->sensor[RESOURCE_TYPE_SENSOR5];
		resource->pdev = core->sensor[RESOURCE_TYPE_SENSOR5].pdev;
		break;
#endif
#endif
	case RESOURCE_TYPE_ISCHAIN:
		for (stream = 0; stream < IS_STREAM_COUNT; ++stream) {
			ischain = &core->ischain[stream];
			if (!test_bit(IS_ISCHAIN_OPEN, &ischain->state)) {
				result = ischain;
				resource->pdev = ischain->pdev;
				break;
			}
		}
		break;
	}

	if (device)
		*device = result;

p_err:
	dbgd_resource("%s\n", __func__);
	return ret;
}

static void is_resource_reset(struct is_resourcemgr *resourcemgr)
{
	struct is_lic_sram *lic_sram = &resourcemgr->lic_sram;

	memset((void *)lic_sram, 0x0, sizeof(struct is_lic_sram));

	resourcemgr->cluster0 = 0;
	resourcemgr->cluster1 = 0;
	resourcemgr->cluster2 = 0;

	resourcemgr->global_param.state = 0;
	resourcemgr->shot_timeout = SHOT_TIMEOUT;
	resourcemgr->shot_timeout_tick = 0;

#if defined(ENABLE_HMP_BOOST)
#if IS_ENABLED(CONFIG_EXYNOS_MSN)
	emstune_add_request(&emstune_req);
#endif
#endif

#if IS_ENABLED(CONFIG_CMU_EWF)
	set_cmuewf(idx_ewf, 1);
#endif
	exynos_bcm_dbg_start();

#if IS_ENABLED(CONFIG_EXYNOS_SCI_DBG_AUTO)
	smc_ppc_enable(1);
#endif

#ifdef DISABLE_BTS_CALC
	bts_calc_disable(1);
#endif

#if defined(ENABLE_CLOG_RESERVED_MEM)
	spin_lock_init(&resourcemgr->slock_cdump);
	resourcemgr->cdump_ptr = 0;
	resourcemgr->sfrdump_ptr = 0;
#endif
	votfitf_init();
}

static void is_resource_clear(struct is_resourcemgr *resourcemgr)
{
	u32 current_min, current_max;

	current_min = (resourcemgr->cluster0 & CLUSTER_MIN_MASK) >> CLUSTER_MIN_SHIFT;
	current_max = (resourcemgr->cluster0 & CLUSTER_MAX_MASK) >> CLUSTER_MAX_SHIFT;
	if (current_min) {
		C0MIN_QOS_DEL();
		warn("[RSC] cluster0 minfreq is not removed(%dMhz)\n", current_min);
	}

	if (current_max) {
		C0MAX_QOS_DEL();
		warn("[RSC] cluster0 maxfreq is not removed(%dMhz)\n", current_max);
	}

	current_min = (resourcemgr->cluster1 & CLUSTER_MIN_MASK) >> CLUSTER_MIN_SHIFT;
	current_max = (resourcemgr->cluster1 & CLUSTER_MAX_MASK) >> CLUSTER_MAX_SHIFT;
	if (current_min) {
		C1MIN_QOS_DEL();
		warn("[RSC] cluster1 minfreq is not removed(%dMhz)\n", current_min);
	}

	if (current_max) {
		C1MAX_QOS_DEL();
		warn("[RSC] cluster1 maxfreq is not removed(%dMhz)\n", current_max);
	}

	current_min = (resourcemgr->cluster2 & CLUSTER_MIN_MASK) >> CLUSTER_MIN_SHIFT;
	current_max = (resourcemgr->cluster2 & CLUSTER_MAX_MASK) >> CLUSTER_MAX_SHIFT;
	if (current_min) {
		C2MIN_QOS_DEL();
		warn("[RSC] cluster2 minfreq is not removed(%dMhz)\n", current_min);
	}

	if (current_max) {
		C2MAX_QOS_DEL();
		warn("[RSC] cluster2 maxfreq is not removed(%dMhz)\n", current_max);
	}

	resourcemgr->cluster0 = 0;
	resourcemgr->cluster1 = 0;
	resourcemgr->cluster2 = 0;

	is_mem_check_stats(&resourcemgr->mem);

	/* clear hal version, default 1.0 */
	resourcemgr->hal_version = IS_HAL_VER_1_0;

#if defined(ENABLE_HMP_BOOST)
#if IS_ENABLED(CONFIG_EXYNOS_MSN)
	if (resourcemgr->dvfs_ctrl.cur_hmp_bst)
		emstune_boost(&emstune_req, 0);

	emstune_remove_request(&emstune_req);
#endif
#endif

#if IS_ENABLED(CONFIG_CMU_EWF)
	set_cmuewf(idx_ewf, 0);
#endif
	exynos_bcm_dbg_stop(CAMERA_DRIVER);

#if IS_ENABLED(CONFIG_EXYNOS_SCI_DBG_AUTO)
	smc_ppc_enable(0);
#endif

#ifdef DISABLE_BTS_CALC
	bts_calc_disable(0);
#endif
}

static int is_resource_phy_power(struct is_core *core, int id, int on)
{
	int ret = 0;
	struct is_device_sensor *sensor;
	struct v4l2_subdev *subdev;
	struct is_device_csi *csi;

	sensor = &core->sensor[id];
	subdev = sensor->subdev_csi;
	if (!subdev) {
		dbgd_resource("%s subdev is null (ID:%d)\n", __func__, id);
		return ret;
	}

	csi = (struct is_device_csi *)v4l2_get_subdevdata(subdev);
	if (!csi) {
		dbgd_resource("%s csi is null (ID:%d)\n", __func__, id);
		return ret;
	}

	if (on)
		ret = phy_power_on(csi->phy);
	else
		ret = phy_power_off(csi->phy);

	if (ret)
		err("fail to phy power on/off(%d)", on);

	mdbgd_front("%s(csi %d, %d, %d)\n", csi, __func__, csi->ch, on, ret);
	return ret;
}

int is_resource_get(struct is_resourcemgr *resourcemgr, u32 rsc_type)
{
	int ret = 0;
	u32 rsccount;
	struct is_resource *resource;
	struct is_core *core;
	int i, id;

	FIMC_BUG(!resourcemgr);
	FIMC_BUG(!resourcemgr->private_data);
	FIMC_BUG(rsc_type >= RESOURCE_TYPE_MAX);

	core = (struct is_core *)resourcemgr->private_data;

	mutex_lock(&resourcemgr->rsc_lock);

	rsccount = atomic_read(&core->rsccount);
	resource = GET_RESOURCE(resourcemgr, rsc_type);
	if (!resource) {
		err("[RSC] resource is NULL");
		ret = -EINVAL;
		goto p_err;
	}

	if (!core->pdev) {
		err("[RSC] pdev is NULL");
		ret = -EMFILE;
		goto p_err;
	}

	if (rsccount >= (IS_STREAM_COUNT + IS_VIDEO_SS5_NUM)) {
		err("[RSC] Invalid rsccount(%d)", rsccount);
		ret = -EMFILE;
		goto p_err;
	}

#ifdef ENABLE_KERNEL_LOG_DUMP
	/* to secure kernel log when there was an instance that remain open */
	{
		struct is_resource *resource_ischain;
		resource_ischain = GET_RESOURCE(resourcemgr, RESOURCE_TYPE_ISCHAIN);
		if ((rsc_type != RESOURCE_TYPE_ISCHAIN)	&& rsccount == 1) {
			if (atomic_read(&resource_ischain->rsccount) == 1)
				is_kernel_log_dump(false);
		}
	}
#endif

	if (rsccount == 0) {
		TIME_LAUNCH_STR(LAUNCH_TOTAL);
		pm_stay_awake(&core->pdev->dev);

		is_resource_reset(resourcemgr);

#ifdef ENABLE_DVFS
		/* dvfs controller init */
		ret = is_dvfs_init(resourcemgr);
		if (ret) {
			err("%s: is_dvfs_init failed!\n", __func__);
			goto p_err;
		}
#endif
#if defined(SECURE_CAMERA_FACE)
		mutex_init(&core->secure_state_lock);
		core->secure_state = IS_STATE_UNSECURE;
		core->scenario = 0;

		info("%s: is secure state has reset\n", __func__);
#endif
		core->dual_info.mode = IS_DUAL_MODE_NOTHING;
		core->dual_info.pre_mode = IS_DUAL_MODE_NOTHING;
		core->dual_info.tick_count = 0;
#if defined(CONFIG_CAMERA_VENDER_MCD) || defined(CONFIG_CAMERA_VENDER_MCD_V2)
		core->vender.opening_hint = IS_OPENING_HINT_NONE;
#endif
		core->vender.isp_max_width = 0;
		core->vender.isp_max_height = 0;

		for (i = 0; i < MAX_SENSOR_SHARED_RSC; i++) {
			spin_lock_init(&core->shared_rsc_slock[i]);
#ifdef USE_FAKE_RETENTION
			if (core->vender.statndby_sensorid == -1)
#endif
				atomic_set(&core->shared_rsc_count[i], 0);
		}
#ifdef CONFIG_CAMERA_VENDER_MCD
		core->vender.statndby_sensorid = -1;
#endif

		for (i = 0; i < SENSOR_CONTROL_I2C_MAX; i++)
			atomic_set(&core->i2c_rsccount[i], 0);

#ifdef ENABLE_DYNAMIC_MEM
		ret = is_resourcemgr_init_dynamic_mem(resourcemgr);
		if (ret) {
			err("is_resourcemgr_init_dynamic_mem is fail(%d)\n", ret);
			goto p_err;
		}
#endif

#if IS_ENABLED(CONFIG_EXYNOS_SYSTEM_EVENT)
		if (core->sysevent_dev) {
			void *sysevent_ret = sysevent_get(core->sysevent_desc.name);
			if (IS_ERR(sysevent_ret))
				err("sysevent_get is fail for IS");
		}
#endif
		if (IS_ENABLED(AP_PHY_LDO_ALL_SAME))
			is_resource_phy_power(core, 0, 1);
		else {
			/* If the LDO connected to each PHY is different */
			for(i = 0; i <= IS_SENSOR_COUNT; i++)
				is_resource_phy_power(core, i, 1);
		}
	}

	if (atomic_read(&resource->rsccount) == 0) {
		switch (rsc_type) {
		case RESOURCE_TYPE_SENSOR0:
		case RESOURCE_TYPE_SENSOR1:
		case RESOURCE_TYPE_SENSOR2:
		case RESOURCE_TYPE_SENSOR3:
		case RESOURCE_TYPE_SENSOR4:
		case RESOURCE_TYPE_SENSOR5:
			id = rsc_type - RESOURCE_TYPE_SENSOR0;
#ifdef CONFIG_PM
			pm_runtime_get_sync(&resource->pdev->dev);
#else
			is_sensor_runtime_resume(&resource->pdev->dev);
#endif
			set_bit(IS_RM_SS0_POWER_ON + id, &resourcemgr->state);
			break;
		case RESOURCE_TYPE_ISCHAIN:
			if (test_bit(IS_RM_POWER_ON, &resourcemgr->state)) {
				err("all resource is not power off(%lX)", resourcemgr->state);
				ret = -EINVAL;
				goto p_err;
			}

			ret = is_debug_open(&resourcemgr->minfo);
			if (ret) {
				err("is_debug_open is fail(%d)", ret);
				goto p_err;
			}

			ret = is_interface_open(&core->interface);
			if (ret) {
				err("is_interface_open is fail(%d)", ret);
				goto p_err;
			}

#if defined(SECURE_CAMERA_MEM_SHARE)
			ret = is_resourcemgr_init_secure_mem(resourcemgr);
			if (ret) {
				err("is_resourcemgr_init_secure_mem is fail(%d)\n", ret);
				goto p_err;
			}
#endif

			ret = is_ischain_power(&core->ischain[0], 1);
			if (ret) {
				err("is_ischain_power is fail(%d)", ret);
				is_ischain_power(&core->ischain[0], 0);
				goto p_err;
			}

			/* W/A for a lower version MCUCTL */
			is_interface_reset(&core->interface);

			set_bit(IS_RM_ISC_POWER_ON, &resourcemgr->state);
			set_bit(IS_RM_POWER_ON, &resourcemgr->state);

#if defined(DISABLE_CORE_IDLE_STATE)
			schedule_work(&resourcemgr->c2_disable_work);
#endif

			is_bts_scen(resourcemgr, 0, true);
			resourcemgr->cur_bts_scen_idx = 0;

			is_hw_ischain_qe_cfg();

			break;
		default:
			err("[RSC] resource type(%d) is invalid", rsc_type);
			BUG();
			break;
		}

#if !defined(DISABLE_LIB)
		if ((rsc_type == RESOURCE_TYPE_ISCHAIN)
			&& (!test_and_set_bit(IS_BINARY_LOADED, &resourcemgr->binary_state))) {
			TIME_LAUNCH_STR(LAUNCH_DDK_LOAD);
			ret = is_load_bin();
			if (ret < 0) {
				err("is_load_bin() is fail(%d)", ret);
				clear_bit(IS_BINARY_LOADED, &resourcemgr->binary_state);
				goto p_err;
			}
			TIME_LAUNCH_END(LAUNCH_DDK_LOAD);
		}
#endif
		is_vender_resource_get(&core->vender, rsc_type);
	}

	if (rsccount == 0) {
#if defined(USE_OFFLINE_PROCESSING)
		ret = is_runtime_resume_post(&resource->pdev->dev);
		if (ret)
			err("is_runtime_resume_post is fail(%d)", ret);
#endif
#ifdef ENABLE_HWACG_CONTROL
		/* for CSIS HWACG */
		is_hw_csi_qchannel_enable_all(true);
#endif
		/* when the first sensor device be opened */
		if (rsc_type < RESOURCE_TYPE_ISCHAIN)
			is_hw_camif_init();

		/* It must be done after power on, because of accessing mux register */
		is_camif_wdma_init();
	}

	atomic_inc(&resource->rsccount);
	atomic_inc(&core->rsccount);

	info("[RSC] rsctype: %d, rsccount: device[%d], core[%d]\n", rsc_type,
			atomic_read(&resource->rsccount), rsccount + 1);
p_err:
	mutex_unlock(&resourcemgr->rsc_lock);

	return ret;
}

int is_resource_put(struct is_resourcemgr *resourcemgr, u32 rsc_type)
{
	int i, id, ret = 0;
	u32 rsccount;
	struct is_resource *resource;
	struct is_core *core;

	FIMC_BUG(!resourcemgr);
	FIMC_BUG(!resourcemgr->private_data);
	FIMC_BUG(rsc_type >= RESOURCE_TYPE_MAX);

	core = (struct is_core *)resourcemgr->private_data;

	mutex_lock(&resourcemgr->rsc_lock);

	rsccount = atomic_read(&core->rsccount);
	resource = GET_RESOURCE(resourcemgr, rsc_type);
	if (!resource) {
		err("[RSC] resource is NULL");
		ret = -EINVAL;
		goto p_err;
	}

	if (!core->pdev) {
		err("[RSC] pdev is NULL");
		ret = -EMFILE;
		goto p_err;
	}

	if (rsccount == 0) {
		err("[RSC] Invalid rsccount(%d)\n", rsccount);
		ret = -EMFILE;
		goto p_err;
	}

#if defined(USE_OFFLINE_PROCESSING)
	if (rsccount == 1) {
		ret = is_runtime_suspend_pre(&resource->pdev->dev);
		if (ret)
			err("is_runtime_suspend_free is fail(%d)", ret);
	}
#endif

	/* local update */
	if (atomic_read(&resource->rsccount) == 1) {
		switch (rsc_type) {
		case RESOURCE_TYPE_SENSOR0:
		case RESOURCE_TYPE_SENSOR1:
		case RESOURCE_TYPE_SENSOR2:
		case RESOURCE_TYPE_SENSOR3:
		case RESOURCE_TYPE_SENSOR4:
		case RESOURCE_TYPE_SENSOR5:
			id = rsc_type - RESOURCE_TYPE_SENSOR0;
#if defined(CONFIG_PM)
			pm_runtime_put_sync(&resource->pdev->dev);
#else
			is_sensor_runtime_suspend(&resource->pdev->dev);
#endif
			clear_bit(IS_RM_SS0_POWER_ON + id, &resourcemgr->state);
			break;
		case RESOURCE_TYPE_ISCHAIN:
#if !defined(DISABLE_LIB)
			if (test_and_clear_bit(IS_BINARY_LOADED,
						&resourcemgr->binary_state)) {
				is_load_clear();
				info("is_load_clear() done\n");
			}
#endif
#if defined(SECURE_CAMERA_FACE)
			if (is_secure_func(core, NULL,
						IS_SECURE_CAMERA_FACE,
						core->scenario,
						SMC_SECCAM_UNPREPARE))
				err("Failed to is_secure_func(FACE, UNPREPARE)");
#endif
			ret = is_itf_power_down(&core->interface);
			if (ret)
				err("power down cmd is fail(%d)", ret);

			ret = is_ischain_power(&core->ischain[0], 0);
			if (ret)
				err("is_ischain_power is fail(%d)", ret);

			ret = is_interface_close(&core->interface);
			if (ret)
				err("is_interface_close is fail(%d)", ret);

#if defined(SECURE_CAMERA_MEM_SHARE)
			ret = is_resourcemgr_deinit_secure_mem(resourcemgr);
			if (ret)
				err("is_resourcemgr_deinit_secure_mem is fail(%d)", ret);
#endif

			ret = is_debug_close();
			if (ret)
				err("is_debug_close is fail(%d)", ret);

			clear_bit(IS_RM_ISC_POWER_ON, &resourcemgr->state);

#if defined(DISABLE_CORE_IDLE_STATE)
			/* CPU idle on/off */
			info("%s: call cpuidle_resume()\n", __func__);
			flush_work(&resourcemgr->c2_disable_work);

			cpuidle_resume_and_unlock();
#endif

			if (resourcemgr->cur_bts_scen_idx) {
				is_bts_scen(resourcemgr, resourcemgr->cur_bts_scen_idx, false);
				resourcemgr->cur_bts_scen_idx = 0;
			}
			is_bts_scen(resourcemgr, 0, false);

			break;
		}

		is_vender_resource_put(&core->vender, rsc_type);
	}

	/* global update */
	if (atomic_read(&core->rsccount) == 1) {
		if (IS_ENABLED(AP_PHY_LDO_ALL_SAME))
			is_resource_phy_power(core, 0, 0);
		else {
			for(i = 0; i <= IS_SENSOR_COUNT; i++)
				is_resource_phy_power(core, i, 0);
		}

		is_resource_clear(resourcemgr);

#ifdef ENABLE_DYNAMIC_MEM
		ret = is_resourcemgr_deinit_dynamic_mem(resourcemgr);
		if (ret)
			err("is_resourcemgr_deinit_dynamic_mem is fail(%d)", ret);
#endif

#if defined(CONFIG_CAMERA_VENDER_MCD) || defined(CONFIG_CAMERA_VENDER_MCD_V2)
		is_vendor_resource_clean(core);

		core->vender.closing_hint = IS_CLOSING_HINT_NONE;
#endif

		ret = is_runtime_suspend_post(&resource->pdev->dev);
		if (ret)
			err("is_runtime_suspend_post is fail(%d)", ret);

		pm_relax(&core->pdev->dev);

#if IS_ENABLED(CONFIG_EXYNOS_SYSTEM_EVENT)
		if (core->sysevent_dev)
			sysevent_put((void *)core->sysevent_dev);
#endif
		clear_bit(IS_RM_POWER_ON, &resourcemgr->state);
	}

	atomic_dec(&resource->rsccount);
	atomic_dec(&core->rsccount);
	info("[RSC] rsctype: %d, rsccount: device[%d], core[%d]\n", rsc_type,
			atomic_read(&resource->rsccount), rsccount - 1);
p_err:
	mutex_unlock(&resourcemgr->rsc_lock);

	return ret;
}

int is_resource_ioctl(struct is_resourcemgr *resourcemgr, struct v4l2_control *ctrl)
{
	int ret = 0;

	FIMC_BUG(!resourcemgr);
	FIMC_BUG(!ctrl);

	mutex_lock(&resourcemgr->qos_lock);
	switch (ctrl->id) {
	/* APOLLO CPU0~3 */
	case V4L2_CID_IS_DVFS_CLUSTER0:
		{
			u32 current_min, current_max;
			u32 request_min, request_max;

			current_min = (resourcemgr->cluster0 & CLUSTER_MIN_MASK) >> CLUSTER_MIN_SHIFT;
			current_max = (resourcemgr->cluster0 & CLUSTER_MAX_MASK) >> CLUSTER_MAX_SHIFT;
			request_min = (ctrl->value & CLUSTER_MIN_MASK) >> CLUSTER_MIN_SHIFT;
			request_max = (ctrl->value & CLUSTER_MAX_MASK) >> CLUSTER_MAX_SHIFT;

			if (current_min) {
				if (request_min)
					C0MIN_QOS_UPDATE(request_min);
				else
					C0MIN_QOS_DEL();
			} else {
				if (request_min)
					C0MIN_QOS_ADD(request_min);
			}

			if (current_max) {
				if (request_max)
					C0MAX_QOS_UPDATE(request_max);
				else
					C0MAX_QOS_DEL();
			} else {
				if (request_max)
					C0MAX_QOS_ADD(request_max);
			}

			info("[RSC] cluster0 minfreq : %dMhz\n", request_min);
			info("[RSC] cluster0 maxfreq : %dMhz\n", request_max);
			resourcemgr->cluster0 = (request_max << CLUSTER_MAX_SHIFT) | request_min;
		}
		break;
	/* ATLAS CPU4~5 */
	case V4L2_CID_IS_DVFS_CLUSTER1:
		{
			u32 current_min, current_max;
			u32 request_min, request_max;

			current_min = (resourcemgr->cluster1 & CLUSTER_MIN_MASK) >> CLUSTER_MIN_SHIFT;
			current_max = (resourcemgr->cluster1 & CLUSTER_MAX_MASK) >> CLUSTER_MAX_SHIFT;
			request_min = (ctrl->value & CLUSTER_MIN_MASK) >> CLUSTER_MIN_SHIFT;
			request_max = (ctrl->value & CLUSTER_MAX_MASK) >> CLUSTER_MAX_SHIFT;

			if (current_min) {
				if (request_min)
					C1MIN_QOS_UPDATE(request_min);
				else
					C1MIN_QOS_DEL();
			} else {
				if (request_min)
					C1MIN_QOS_ADD(request_min);
			}

			if (current_max) {
				if (request_max)
					C1MAX_QOS_UPDATE(request_max);
				else
					C1MAX_QOS_DEL();
			} else {
				if (request_max)
					C1MAX_QOS_ADD(request_max);
			}

			info("[RSC] cluster1 minfreq : %dMhz\n", request_min);
			info("[RSC] cluster1 maxfreq : %dMhz\n", request_max);
			resourcemgr->cluster1 = (request_max << CLUSTER_MAX_SHIFT) | request_min;
		}
		break;
	/* ATLAS CPU6~7 */
	case V4L2_CID_IS_DVFS_CLUSTER2:
		{
#if defined(PM_QOS_CLUSTER2_FREQ_MAX_DEFAULT_VALUE)
			u32 current_min, current_max;
			u32 request_min, request_max;

			current_min = (resourcemgr->cluster2 & CLUSTER_MIN_MASK) >> CLUSTER_MIN_SHIFT;
			current_max = (resourcemgr->cluster2 & CLUSTER_MAX_MASK) >> CLUSTER_MAX_SHIFT;
			request_min = (ctrl->value & CLUSTER_MIN_MASK) >> CLUSTER_MIN_SHIFT;
			request_max = (ctrl->value & CLUSTER_MAX_MASK) >> CLUSTER_MAX_SHIFT;

			if (current_min) {
				if (request_min)
					C2MIN_QOS_UPDATE(request_min);
				else
					C2MIN_QOS_DEL();
			} else {
				if (request_min)
					C2MIN_QOS_ADD(request_min);
			}

			if (current_max) {
				if (request_max)
					C2MAX_QOS_UPDATE(request_max);
				else
					C2MAX_QOS_DEL();
			} else {
				if (request_max)
					C2MAX_QOS_ADD(request_max);
			}

			info("[RSC] cluster2 minfreq : %dMhz\n", request_min);
			info("[RSC] cluster2 maxfreq : %dMhz\n", request_max);
			resourcemgr->cluster2 = (request_max << CLUSTER_MAX_SHIFT) | request_min;
#endif
		}
		break;
	}
	mutex_unlock(&resourcemgr->qos_lock);

	return ret;
}

void is_resource_set_global_param(struct is_resourcemgr *resourcemgr, void *device)
{
	bool video_mode;
	struct is_device_ischain *ischain = device;
	struct is_global_param *global_param = &resourcemgr->global_param;

	mutex_lock(&global_param->lock);

	if (!global_param->state) {
		video_mode = IS_VIDEO_SCENARIO(ischain->setfile & IS_SETFILE_MASK);
		global_param->video_mode = video_mode;
		ischain->hardware->video_mode = video_mode;
		minfo("video mode %d\n", ischain, video_mode);

#ifdef ENABLE_DVFS
		global_param->llc_state = 0;
		if (ischain->llc_mode == false)
			set_bit(LLC_DISABLE, &global_param->llc_state);

		is_hw_configure_llc(true, ischain, &global_param->llc_state);
#endif
	}

	set_bit(ischain->instance, &global_param->state);

	mutex_unlock(&global_param->lock);
}

void is_resource_clear_global_param(struct is_resourcemgr *resourcemgr, void *device)
{
	struct is_device_ischain *ischain = device;
	struct is_global_param *global_param = &resourcemgr->global_param;

	mutex_lock(&global_param->lock);

	clear_bit(ischain->instance, &global_param->state);

	if (!global_param->state) {
		global_param->video_mode = false;
		ischain->hardware->video_mode = false;

#ifdef ENABLE_DVFS
		is_hw_configure_llc(false, ischain, &global_param->llc_state);
#endif
	}

	mutex_unlock(&global_param->lock);
}

int is_resource_update_lic_sram(struct is_resourcemgr *resourcemgr, void *device, bool on)
{
	struct is_device_ischain *ischain = device;
	struct is_device_sensor *sensor;
	struct is_lic_sram *lic_sram;
	u32 taa_id;
	int taa_sram_sum;

	FIMC_BUG(!resourcemgr);
	FIMC_BUG(!device);

	/* TODO: DT flag for LIC use */

	sensor = ischain->sensor;
	FIMC_BUG(!sensor);

	lic_sram = &resourcemgr->lic_sram;
	taa_id = ischain->group_3aa.id - GROUP_ID_3AA0;

	/* In case of remosic capture that is not use PDP & 3AA, it is returned. */
	if (sensor && !test_bit(IS_SENSOR_OTF_OUTPUT, &sensor->state))
		goto p_skip_update_sram;

	if (on) {
		lic_sram->taa_sram[taa_id] = sensor->sensor_width;
		atomic_add(sensor->sensor_width, &lic_sram->taa_sram_sum);
	} else {
		lic_sram->taa_sram[taa_id] = 0;
		taa_sram_sum = atomic_sub_return(sensor->sensor_width, &lic_sram->taa_sram_sum);
		if (taa_sram_sum < 0) {
			mwarn("[RSC] Invalid taa_sram_sum %d\n", ischain, taa_sram_sum);
			atomic_set(&lic_sram->taa_sram_sum, 0);
		}
	}

p_skip_update_sram:
	minfo("[RSC] LIC taa_sram([0]%d, [1]%d, [2]%d, [3]%d, [sum]%d)\n", ischain,
		lic_sram->taa_sram[0],
		lic_sram->taa_sram[1],
		lic_sram->taa_sram[2],
		lic_sram->taa_sram[3],
		atomic_read(&lic_sram->taa_sram_sum));
	return 0;
}

int is_logsync(struct is_interface *itf, u32 sync_id, u32 msg_test_id)
{
	int ret = 0;

	/* print kernel sync log */
	log_sync(sync_id);

#ifdef ENABLE_FW_SYNC_LOG
	ret = is_hw_msg_test(itf, sync_id, msg_test_id);
	if (ret)
	err("is_hw_msg_test(%d)", ret);
#endif
	return ret;
}

struct is_dbuf_q *is_init_dbuf_q(void)
{
	void *ret;
	int i_id, i_list;
	int num_list = MAX_DBUF_LIST;
	struct is_dbuf_q *dbuf_q;

	dbuf_q = vzalloc(sizeof(struct is_dbuf_q));
	if (!dbuf_q) {
		err("failed to allocate dbuf_q");
		return ERR_PTR(-ENOMEM);
	}

	for (i_id = 0; i_id < ID_DBUF_MAX; i_id++) {
		dbuf_q->dbuf_list[i_id] = vzalloc(sizeof(struct is_dbuf_list) * num_list);
		if (!dbuf_q->dbuf_list[i_id]) {
			err("failed to allocate dbuf_list");
			ret = ERR_PTR(-ENOMEM);
			goto err_alloc_list;
		}

		mutex_init(&dbuf_q->lock[i_id]);

		dbuf_q->queu_count[i_id] = 0;
		INIT_LIST_HEAD(&dbuf_q->queu_list[i_id]);

		dbuf_q->free_count[i_id] = 0;
		INIT_LIST_HEAD(&dbuf_q->free_list[i_id]);
	}

	/* set free list */
	for (i_id = 0; i_id < ID_DBUF_MAX; i_id++) {
		for (i_list = 0; i_list < num_list; i_list++) {
			list_add_tail(&dbuf_q->dbuf_list[i_id][i_list].list,
					&dbuf_q->free_list[i_id]);
			dbuf_q->free_count[i_id]++;
		}
	}

	return dbuf_q;

err_alloc_list:
	while (i_id-- > 0) {
		if (dbuf_q->dbuf_list[i_id])
			vfree(dbuf_q->dbuf_list[i_id]);
	}

	vfree(dbuf_q);

	return ret;
}

/* cache maintenance for user buffer */
void is_deinit_dbuf_q(struct is_dbuf_q *dbuf_q)
{
	int i_id;

	for (i_id = 0; i_id < ID_DBUF_MAX; i_id++)
		vfree(dbuf_q->dbuf_list[i_id]);

	vfree(dbuf_q);
}

static void is_flush_dma_buf(struct is_dbuf_q *dbuf_q, u32 dma_id, u32 qcnt)
{
	struct is_dbuf_list *dbuf_list;

	while (dbuf_q->queu_count[dma_id] > qcnt) {
		/* get queue list */
		dbuf_list = list_first_entry(&dbuf_q->queu_list[dma_id],
					struct is_dbuf_list, list);
		list_del(&dbuf_list->list);
		dbuf_q->queu_count[dma_id]--;

		/* put free list */
		list_add_tail(&dbuf_list->list, &dbuf_q->free_list[dma_id]);
		dbuf_q->free_count[dma_id]++;
	}
}

void is_q_dbuf_q(struct is_dbuf_q *dbuf_q, struct is_sub_dma_buf *sdbuf, u32 qcnt)
{
	int dma_id, num_planes, p;
	struct is_dbuf_list *dbuf_list;

	dma_id = is_get_dma_id(sdbuf->vid);
	if (dma_id < 0)
		return;

	mutex_lock(&dbuf_q->lock[dma_id]);

	if (dbuf_q->queu_count[dma_id] > qcnt) {
		info("[%s] dma_id(%d) dbuf qcnt(%d) > vb2 qcnt(%d)", __func__, dma_id,
			dbuf_q->queu_count[dma_id], qcnt);

		is_flush_dma_buf(dbuf_q, dma_id, qcnt);
	}

	if (!dbuf_q->free_count[dma_id] || list_empty(&dbuf_q->free_list[dma_id])) {
		warn("dma_id(%d) free list is NULL[f(%d)/q(%d)]", dma_id,
			dbuf_q->free_count[dma_id], dbuf_q->queu_count[dma_id]);

		mutex_unlock(&dbuf_q->lock[dma_id]);
		return;
	}

	/* get free list */
	dbuf_list = list_first_entry(&dbuf_q->free_list[dma_id],
				struct is_dbuf_list, list);
	list_del(&dbuf_list->list);
	dbuf_q->free_count[dma_id]--;

	/* copy */
	num_planes = sdbuf->num_plane * sdbuf->num_buffer;
	for (p = 0; p < num_planes; p++)
		dbuf_list->dbuf[p] = sdbuf->dbuf[p];

	/* put queue list */
	list_add_tail(&dbuf_list->list, &dbuf_q->queu_list[dma_id]);
	dbuf_q->queu_count[dma_id]++;

	mutex_unlock(&dbuf_q->lock[dma_id]);
}

void is_dq_dbuf_q(struct is_dbuf_q *dbuf_q, u32 dma_id, enum dma_data_direction dir)
{
	u32 p;
	struct is_dbuf_list *dbuf_list;

	mutex_lock(&dbuf_q->lock[dma_id]);

	if (!dbuf_q->queu_count[dma_id] || list_empty(&dbuf_q->queu_list[dma_id])) {
		warn("dma_id(%d) queue list is NULL[f(%d)/q(%d)]", dma_id,
			dbuf_q->free_count[dma_id], dbuf_q->queu_count[dma_id]);
		return;
	}

	/* get queue list */
	dbuf_list = list_first_entry(&dbuf_q->queu_list[dma_id],
				struct is_dbuf_list, list);
	list_del(&dbuf_list->list);
	dbuf_q->queu_count[dma_id]--;

	/* put free list */
	list_add_tail(&dbuf_list->list, &dbuf_q->free_list[dma_id]);
	dbuf_q->free_count[dma_id]++;

	mutex_unlock(&dbuf_q->lock[dma_id]);

	/* cache maintenance */
	for (p = 0; p < IS_MAX_PLANES && dbuf_list->dbuf[p]; p++) {
		if (dir == DMA_FROM_DEVICE)	/* cache inv */
			dma_buf_begin_cpu_access(dbuf_list->dbuf[p], DMA_FROM_DEVICE);
		else if (dir == DMA_TO_DEVICE)	/* cache clean */
			dma_buf_end_cpu_access(dbuf_list->dbuf[p], DMA_TO_DEVICE);
		else
			warn("invalid direction(%d), type(%d)", dir, dma_id);
	}

	if (!p)
		warn("dbuf is NULL. dma_id(%d)", dma_id);
}
