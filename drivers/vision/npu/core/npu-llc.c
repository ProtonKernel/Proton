/*
 * Samsung Exynos SoC series NPU driver
 *
 * Copyright (c) 2019 Samsung Electronics Co., Ltd.
 *              http://www.samsung.com/
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 */

#include <linux/version.h>
#include <linux/highmem.h>
#include <soc/samsung/exynos-sci.h>
#include "npu-scheduler.h"
#include "npu-log.h"
#include "npu-device.h"

/*****************************************************************************
 *****                         wrapper function                          *****
 *****************************************************************************/
#if LINUX_VERSION_CODE >= KERNEL_VERSION(5, 4, 0)
static unsigned int npu_llc_region_alloc(unsigned int region_index,
				unsigned int set, unsigned int ways)
{
	llc_region_alloc(region_index, set, ways);
	return llc_get_region_info(region_index);
}
#else
static unsigned int npu_llc_region_alloc(unsigned int region_index,
		unsigned int set, __attribute__((unused)) unsigned int ways)
{
	llc_region_alloc(region_index, set);
	return llc_get_region_info(region_index);
}
#endif

void npu_set_llc(struct npu_scheduler_info *info)
{
	u32 n0, n1, n2;
	struct npu_system *system;
	volatile struct mailbox_hdr *mbox_hdr;

	system = &(info->device->system);
	mbox_hdr = system->mbox_hdr;

	if (!mbox_hdr)
		return;

	/* get current llc status */
	n0 = llc_get_region_info(LLC_REGION_NPU0);
	n1 = llc_get_region_info(LLC_REGION_NPU1);
	n2 = llc_get_region_info(LLC_REGION_NPU2);

	if (info->mode == NPU_PERF_MODE_NPU_BOOST) {
		if (!info->llc_status) {
			/* If llc disabled, set llc */
			n0 = npu_llc_region_alloc(LLC_REGION_NPU0, 1, 12);
			n1 = npu_llc_region_alloc(LLC_REGION_NPU1, 1, 0);
			n2 = npu_llc_region_alloc(LLC_REGION_NPU2, 1, 4);
			info->llc_status = 1;
		}
	} else if (info->mode == NPU_PERF_MODE_NPU_BOOST_DLV3) {
		if (!info->llc_status) {
			/* If llc disabled, set llc */
			n0 = npu_llc_region_alloc(LLC_REGION_NPU0, 1, 16);
			n1 = npu_llc_region_alloc(LLC_REGION_NPU1, 1, 0);
			n2 = npu_llc_region_alloc(LLC_REGION_NPU2, 1, 0);
			info->llc_status = 1;
		}
	} else {
		/* Non NPU_PERF_MODE_NPU_BOOST or
		 * NPU_PERF_MODE_NPU_SINGLE_BOOST.
		 */
		if (info->llc_status) {
			/* If llc enabled, put llc */
			n0 = npu_llc_region_alloc(LLC_REGION_NPU0, 0, 0);
			n1 = npu_llc_region_alloc(LLC_REGION_NPU1, 0, 0);
			n2 = npu_llc_region_alloc(LLC_REGION_NPU2, 0, 0);
			info->llc_status = 0;
		}
	}
	info->llc_mode = (n0 & 0xff) << 24 | (n1 & 0xff) << 16 |
		(n2 & 0xff) << 8 | (info->mode & 0xff);
	writel(info->llc_mode, &mbox_hdr->llc_mode);
	flush_kernel_vmap_range((void *)system->mbox_hdr, sizeof(*system->mbox_hdr));
	npu_info("npu set llc(mode:%u, status:%u, llc_mode:0x%08x)\n",
			info->mode, info->llc_status, info->llc_mode);
	return;
}
