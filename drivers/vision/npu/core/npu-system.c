/*
 * Samsung Exynos SoC series NPU driver
 *
 * Copyright (c) 2017 Samsung Electronics Co., Ltd.
 *              http://www.samsung.com/
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 */


#include <linux/version.h>
#include <linux/platform_device.h>
#include <linux/pm_runtime.h>
#include <linux/clk-provider.h>

#include <linux/io.h>
#include <linux/delay.h>
#include <linux/highmem.h>
#include <linux/ion.h>
#include <uapi/linux/ion.h>
#include <linux/scatterlist.h>
#include <linux/dma-direct.h>
#include <linux/dma-buf.h>
#include <linux/iommu.h>
#include <linux/dma-iommu.h>
#include <linux/smc.h>
#include <linux/soc/samsung/exynos-soc.h>
#include <linux/of_reserved_mem.h>

#if LINUX_VERSION_CODE <= KERNEL_VERSION(4, 19, 128)
#include <linux/exynos_iovmm.h>
#include <linux/ion_exynos.h>
#endif

#include "npu-log.h"
#include "npu-device.h"
#include "npu-system.h"
#include "npu-util-regs.h"
#include "npu-stm-soc.h"

#ifdef CONFIG_NPU_HARDWARE
#include "mailbox_ipc.h"
#elif defined(CONFIG_NPU_LOOPBACK)
#include "mailbox_ipc.h"
#endif


#ifdef CONFIG_FIRMWARE_SRAM_DUMP_DEBUGFS
#include "npu-util-memdump.h"
#endif

#ifdef CONFIG_NPU_USE_MBR
#include "dsp-npu.h"
#endif

/*****************************************************************************
 *****                         wrapper function                          *****
 *****************************************************************************/
#if LINUX_VERSION_CODE >= KERNEL_VERSION(5, 4, 0)
static unsigned int __npu_memory_get_query_from_ion(const char *heapname)
{
	struct ion_heap_data data[ION_NUM_MAX_HEAPS];
	int i;
	int cnt = ion_query_heaps_kernel(NULL, 0);

	ion_query_heaps_kernel((struct ion_heap_data *)data, cnt);

	for (i = 0; i < cnt; i++) {
		if (!strncmp(data[i].name, heapname, MAX_HEAP_NAME))
			break;
	}

	if (i == cnt)
		return 0;

	return 1 << data[i].heap_id;
}

static struct dma_buf *npu_memory_ion_alloc(size_t size, unsigned long flag)
{
	unsigned int heapmask = __npu_memory_get_query_from_ion("vendor_system_heap");

	if (!heapmask)
		heapmask = __npu_memory_get_query_from_ion("ion_system_heap");

	return ion_alloc(size, heapmask, flag);
}

static dma_addr_t npu_memory_dma_buf_dva_map(struct npu_memory_buffer *buffer)
{
	return sg_dma_address(buffer->sgt->sgl);
}

static void npu_memory_dma_buf_dva_unmap(
		__attribute__((unused))struct npu_memory_buffer *buffer)
{
	struct iommu_domain *domain = iommu_get_domain_for_dev(buffer->attachment->dev);
	size_t unmapped = iommu_unmap(domain, buffer->daddr, buffer->size);
	if (!unmapped)
		probe_warn("iommu_unmap failed");
	if (unmapped != buffer->size)
		probe_warn("iomuu_unmap unmapped size(%lu) is not buffer size(%lu)",
				unmapped, buffer->size);
	return;
}

static int npu_iommu_dma_enable_best_fit_algo(struct device *dev)
{
	return iommu_dma_enable_best_fit_algo(dev);
}

/*
static void *npu_memory_dma_buf_va_map(struct npu_memory_buffer *buffer)
{
	return dma_buf_kmap(buffer->dma_buf, 0);
}

static void npu_memory_dma_buf_va_unmap(struct npu_memory_buffer *buffer)
{
	dma_buf_kunmap(buffer->dma_buf, 0, buffer->vaddr);
}
*/
#else
static struct dma_buf *npu_memory_ion_alloc(size_t size, unsigned long flag)
{
	const char *heapname = "npu_fw";

	return *ion_alloc_dmabuf(heapname, size, flag);
}

static dma_addr_t npu_memory_dma_buf_dva_map(struct npu_memory_buffer *buffer)
{
	return ion_iovmm_map(buffer->attachment, 0, buffer->size,
						DMA_BIDIRECTIONAL, 0);
}

static void npu_memory_dma_buf_dva_unmap(struct npu_memory_buffer *buffer)
{
	ion_iovmm_unmap(buffer->attachment, buffer->daddr);
}

static int npu_iommu_dma_enable_best_fit_algo(__attribute__((unused))struct device *dev)
{
	return 0;
}

/*
static void *npu_memory_dma_buf_va_map(struct npu_memory_buffer *buffer)
{
	return dma_buf_vmap(buffer->dma_buf);
}

static void npu_memory_dma_buf_va_unmap(struct npu_memory_buffer *buffer)
{
	dma_buf_vunmap(buffer->dma_buf, buffer->vaddr);
}
*/
#endif



struct system_pwr sysPwr;
struct npu_system *g_system_info;

#define OFFSET_END	0xFFFFFFFF

/* Initialzation steps for system_resume */
enum npu_system_resume_steps {
	NPU_SYS_RESUME_SETUP_WAKELOCK,
	NPU_SYS_RESUME_INIT_FWBUF,
	NPU_SYS_RESUME_FW_LOAD,
	NPU_SYS_RESUME_CLK_PREPARE,
	NPU_SYS_RESUME_FW_VERIFY,
	NPU_SYS_RESUME_SOC,
	NPU_SYS_RESUME_OPEN_INTERFACE,
	NPU_SYS_RESUME_SOC_STM_SETUP,
	NPU_SYS_RESUME_COMPLETED
};

/* Initialzation steps for system_soc_resume */
enum npu_system_resume_soc_steps {
	NPU_SYS_RESUME_SOC_CPU_ON,
	NPU_SYS_RESUME_SOC_STM_DISABLE,
	NPU_SYS_RESUME_SOC_COMPLETED
};

static int npu_firmware_load(struct npu_system *system, int mode);

static struct npu_memory_buffer *npu_flush_memory_buffer;
void npu_memory_sync_for_cpu(void)
{
	return;
//	dma_sync_sg_for_cpu(
//		npu_flush_memory_buffer->attachment->dev,
//		npu_flush_memory_buffer->sgt->sgl,
//		npu_flush_memory_buffer->sgt->orig_nents,
//		DMA_FROM_DEVICE);
}

void npu_memory_sync_for_device(void)
{
	return;
//	dma_sync_sg_for_device(
//		npu_flush_memory_buffer->attachment->dev,
//		npu_flush_memory_buffer->sgt->sgl,
//		npu_flush_memory_buffer->sgt->orig_nents,
//		DMA_TO_DEVICE);
}

int npu_memory_alloc_from_heap(struct platform_device *pdev,
		struct npu_memory_buffer *buffer, dma_addr_t daddr,
		__attribute__((unused))const char *heapname, int prot)
{
	int ret = 0;
	bool complete_suc = false;

	struct dma_buf *dma_buf;
	struct dma_buf_attachment *attachment;
	struct sg_table *sgt;
	phys_addr_t phys_addr;
	void *vaddr;
	int flag;

	size_t size;
	size_t map_size;
	unsigned int iommu_attributes = 0;

	BUG_ON(!buffer);

	buffer->dma_buf = NULL;
	buffer->attachment = NULL;
	buffer->sgt = NULL;
	buffer->daddr = 0;
	buffer->vaddr = NULL;
	INIT_LIST_HEAD(&buffer->list);

	size = buffer->size;
	flag = 0;

	dma_buf = npu_memory_ion_alloc(size, flag);
	if (IS_ERR_OR_NULL(dma_buf)) {
		probe_err("npu_memory_ion_alloc is fail(%p)\n", dma_buf);
		ret = -EINVAL;
		goto p_err;
	}
	buffer->dma_buf = dma_buf;

	attachment = dma_buf_attach(dma_buf, &pdev->dev);
	if (IS_ERR(attachment)) {
		ret = PTR_ERR(attachment);
		probe_err("dma_buf_attach is fail(%d)\n", ret);
		goto p_err;
	}
	buffer->attachment = attachment;

	if (prot) {
		probe_info("prot : %d", prot);
		attachment->dma_map_attrs |= DMA_ATTR_SET_PRIV_DATA(prot);
		iommu_attributes = (prot) << IOMMU_PRIV_SHIFT;
	}

	sgt = dma_buf_map_attachment(attachment, DMA_BIDIRECTIONAL);
	if (IS_ERR(sgt)) {
		ret = PTR_ERR(sgt);
		probe_err("dma_buf_map_attach is fail(%d)\n", ret);
		goto p_err;
	}
	buffer->sgt = sgt;

	/* But, first phys. Not all */
	phys_addr = sg_phys(sgt->sgl);
	buffer->paddr = phys_addr;

	if (!daddr) {
		daddr = npu_memory_dma_buf_dva_map(buffer);
		if (IS_ERR_VALUE(daddr)) {
			probe_err("fail(err %pad) to allocate iova\n", &daddr);
			ret = -ENOMEM;
			goto p_err;
		}
		probe_info("sg_dma_address is success");
	} else {
		struct iommu_domain *domain = iommu_get_domain_for_dev(&pdev->dev);

		probe_info("domain = 0x%p daddr = 0x%llx, phys_addr = 0x%llx, size = 0x%lx", domain, daddr, phys_addr, size);
		map_size = iommu_map_sg(domain, daddr, sgt->sgl, sgt->orig_nents, iommu_attributes);
		if (!map_size) {
			probe_err("fail(err %pad) in iommu_map\n", &daddr);
			ret = -ENOMEM;
			goto p_err;
		}

		if (prot) {
			ret = iommu_dma_reserve_iova(&pdev->dev, daddr, size);
			if (ret) {
				probe_err("failed(err %pad) in iommu_dma_reserve_iova\n", &daddr);
				goto p_err;
			}
			probe_info("success reserve iova(daddr : 0x%llx, size = 0x%lx)\n", daddr, size);
		}

		if (map_size != size) {
			probe_info("iommu_mag_sg return %lu, we want to map %lu",
				map_size, size);
		}
	}

	buffer->daddr = daddr;
	vaddr = dma_buf_vmap(dma_buf);
	if (IS_ERR_OR_NULL(vaddr)) {
		if (vaddr)
			probe_err("fail(err %p) in dma_buf_vmap\n", vaddr);
		else /* !vaddr */
			probe_err("fail(err NULL) in dma_buf_vmap\n");

		ret = -EFAULT;
		goto p_err;
	}
	buffer->vaddr = vaddr;

	complete_suc = true;

	probe_info("buffer[%p], paddr[%llx], vaddr[%p], daddr[%llx], "
		"sgt[%p], nents[%u], attachment[%p], map[%ld]\n",
		buffer, buffer->paddr, buffer->vaddr, buffer->daddr,
		buffer->sgt, buffer->sgt->orig_nents, buffer->attachment, map_size);
p_err:
	if (complete_suc != true) {
		npu_memory_free_from_heap(&pdev->dev, buffer);
	}
	return ret;
}

void npu_memory_free_from_heap(struct device *dev, struct npu_memory_buffer *buffer)
{
	if (!IS_ERR_OR_NULL(buffer->vaddr))
		dma_buf_vunmap(buffer->dma_buf, buffer->vaddr);
	if (buffer->daddr && !IS_ERR_VALUE(buffer->daddr))
		npu_memory_dma_buf_dva_unmap(buffer);
	if (!IS_ERR_OR_NULL(buffer->sgt))
		dma_buf_unmap_attachment(buffer->attachment, buffer->sgt, DMA_BIDIRECTIONAL);
	if (!IS_ERR_OR_NULL(buffer->attachment) && !IS_ERR_OR_NULL(buffer->dma_buf))
		dma_buf_detach(buffer->dma_buf, buffer->attachment);
	if (!IS_ERR_OR_NULL(buffer->dma_buf))
		dma_buf_put(buffer->dma_buf);

	buffer->dma_buf = NULL;
	buffer->attachment = NULL;
	buffer->sgt = NULL;
	buffer->daddr = 0;
	buffer->vaddr = NULL;
}

void npu_soc_status_report(struct npu_system *system)
{
	BUG_ON(!system);

	npu_info("Print CPU PC value\n");
	npu_cmd_map(system, "cpupc");
#if 0
	npu_info("CA5 PC : CPU0 0x%x, CPU1 0x%x\n",
		readl(system->sfrdnc.vaddr + 0x100cc),
		readl(system->sfrdnc.vaddr + 0x100d0));
	npu_info("DSPC DBG STATUS : INTR 0x%x, DINTR 0x%x\n",
		readl(system->sfrdnc.vaddr + 0x110c),
		readl(system->sfrdnc.vaddr + 0x1144));
#endif
}

u32 npu_get_hw_info(void)
{
	union npu_hw_info hw_info;

	memset(&hw_info, 0, sizeof(hw_info));

	hw_info.fields.product_id = (exynos_soc_info.product_id & EXYNOS_SOC_MASK) << 4;

	/* DCache disable before EVT 1.1 */
	if ((exynos_soc_info.main_rev >= 2)
	   || (exynos_soc_info.main_rev >= 1 && exynos_soc_info.sub_rev >= 1))
		hw_info.fields.dcache_en = 1;
	else
		hw_info.fields.dcache_en = 0;

	npu_info("HW Info = %08x\n", hw_info.value);

	return hw_info.value;
}

#ifdef CONFIG_EXYNOS_NPU_DRAM_FW_LOG_BUF
#define DRAM_FW_REPORT_BUF_SIZE		(1024*1024)
#define DRAM_FW_PROFILE_BUF_SIZE	(1024*1024)

static struct npu_memory_v_buf fw_report_buf = {
	.size = DRAM_FW_REPORT_BUF_SIZE,
};
static struct npu_memory_v_buf fw_profile_buf = {
	.size = DRAM_FW_PROFILE_BUF_SIZE,
};

int npu_system_alloc_fw_dram_log_buf(struct npu_system *system)
{
	int ret = 0;
	BUG_ON(!system);

	npu_info("start: initialization.\n");

	if (!fw_report_buf.v_buf) {
		ret = npu_memory_v_alloc(&system->memory, &fw_report_buf);
		if (ret) {
			npu_err("fail(%d) in FW report buffer memory allocation\n", ret);
			return ret;
		}
		npu_fw_report_init(fw_report_buf.v_buf, fw_report_buf.size);
	} else {//Case of fw_report is already allocated by ion memory
		npu_dbg("fw_report is already initialized - %pK.\n", fw_report_buf.v_buf);
	}

	if (!fw_profile_buf.v_buf) {
		ret = npu_memory_v_alloc(&system->memory, &fw_profile_buf);
		if (ret) {
			npu_err("fail(%d) in FW profile memory allocation\n", ret);
			return ret;
		}
		npu_fw_profile_init(fw_profile_buf.v_buf, fw_profile_buf.size);
	} else {//Case of fw_profile is already allocated by ion memory
		npu_dbg("fw_profile is already initialized - %pK.\n", fw_profile_buf.v_buf);
	}

	/* Initialize firmware utc handler with dram log buf */
	ret = npu_fw_test_initialize(system);
	if (ret) {
		npu_err("npu_fw_test_probe() failed : ret = %d\n", ret);
		return ret;
	}

	npu_info("complete : initialization.\n");
	return ret;
}

static int npu_system_free_fw_dram_log_buf(void)
{
	/* TODO */
	return 0;
}

#else
#define npu_system_alloc_fw_dram_log_buf(t)	(0)
#define npu_system_free_fw_dram_log_buf(t)	(0)
#endif	/* CONFIG_EXYNOS_NPU_DRAM_FW_LOG_BUF */

struct reg_cmd_list npu_cmd_list[] = {
//	{	.name = "poweron",	.list = NULL,	.count = 0	},
//	{	.name = "poweroff",	.list = NULL,	.count = 0	},
	{	.name = "cpuon",	.list = NULL,	.count = 0	},
	{	.name = "cpuoff",	.list = NULL,	.count = 0	},
	{	.name = "hwacgen",	.list = NULL,	.count = 0	},
	{	.name = "hwacgdis",	.list = NULL,	.count = 0	},
	{	.name = "glafmen",	.list = NULL,	.count = 0	},
	{	.name = "glafmdis",	.list = NULL,	.count = 0	},
	{	.name = "afmitren",	.list = NULL,	.count = 0	},
	{	.name = "afmitrdis",	.list = NULL,	.count = 0	},
	{	.name = "afmthren",	.list = NULL,	.count = 0	},
	{	.name = "afmthrdis",	.list = NULL,	.count = 0	},
	{	.name = "chkafmitr",	.list = NULL,	.count = 0	},
	{	.name = "clrafmitr",	.list = NULL,	.count = 0	},
	{	.name = "clrafmtdc",	.list = NULL,	.count = 0	},
	{	.name = "printafm",	.list = NULL,	.count = 0	},
	{	.name = "cpupc",	.list = NULL,	.count = 0	},
#ifdef CONFIG_NPU_STM
	{	.name = "buspdiv1to1",	.list = NULL,	.count = 0	},
	{	.name = "buspdiv1to4",	.list = NULL,	.count = 0	},
	{	.name = "enablestm",	.list = NULL,	.count = 0	},
	{	.name = "disablestm",	.list = NULL,	.count = 0	},
	{	.name = "allow64stm",	.list = NULL,	.count = 0	},
	{	.name = "syncevent",	.list = NULL,	.count = 0	},
#endif
	{	.name = NULL,	.list = NULL,	.count = 0	}
};

static int npu_init_cmd_list(struct npu_system *system)
{
	int ret = 0;
	int i;

	BUG_ON(!system);

	system->cmd_list = (struct reg_cmd_list *)npu_cmd_list;

	for (i = 0; ((system->cmd_list) + i)->name; i++) {
		if (npu_get_reg_cmd_map(system, (system->cmd_list) + i) == -ENODEV)
			probe_info("No cmd for %s\n", ((system->cmd_list) + i)->name);
		else
			probe_info("register cmd for %s\n", ((system->cmd_list) + i)->name);
	}

	return ret;
}

static inline void set_max_npu_core(struct npu_system *system, s32 num)
{
	BUG_ON(num < 0);
	probe_info("Max number of npu core : %d\n", num);
	system->max_npu_core = num;
}

static int npu_rsvd_map(struct npu_system *system, struct npu_rmem_data *rmt)
{
	int ret = 0;
	unsigned int num_pages;
	struct page **pages, **page;
	phys_addr_t phys;

	/* try to map kvmap */
	num_pages = (unsigned int)DIV_ROUND_UP(rmt->area_info->size, PAGE_SIZE);
	pages = kcalloc(num_pages, sizeof(pages[0]), GFP_KERNEL);
	if (!pages) {
		ret = -ENOMEM;
		probe_err("fail to alloc pages for rmem(%s)\n", rmt->name);
		iommu_unmap(system->domain, rmt->area_info->daddr,
				(size_t) rmt->rmem->size);
		goto p_err;
	}

	phys = rmt->rmem->base;
	for (page = pages; (page - pages < num_pages); page++) {
		*page = phys_to_page(phys);
		phys += PAGE_SIZE;
	}
	rmt->area_info->paddr = rmt->rmem->base;

	rmt->area_info->vaddr = vmap(pages, num_pages,
			VM_MAP, pgprot_writecombine(PAGE_KERNEL));
	kfree(pages);
	if (!rmt->area_info->vaddr) {
		ret = -ENOMEM;
		probe_err("fail to vmap %u pages for rmem(%s)\n",
				num_pages, rmt->name);
		iommu_unmap(system->domain, rmt->area_info->daddr,
				(size_t) rmt->area_info->size);
		goto p_err;
	}

p_err:
	return ret;
}

static int npu_init_iomem_area(struct npu_system *system)
{
	int ret = 0;
	int i, k, si, mi;
	void __iomem *iomem;
	struct device *dev;
	int iomem_count, init_count, phdl_cnt, rmem_cnt;
	struct iomem_reg_t *iomem_data;
	struct iomem_reg_t *iomem_init_data = NULL;
	struct iommu_domain *domain;
	const char *iomem_name;
	const char *heap_name;
	struct npu_iomem_area *id;
	struct npu_memory_buffer *bd;
	struct npu_io_data *it;
	struct npu_mem_data *mt;
	struct npu_rmem_data *rmt;
	struct device_node *mems_node, *mem_node, *phdl_node;
	struct reserved_mem *rsvd_mem;
	char tmpname[32];
	u32 core_num;
	u32 size;

	BUG_ON(!system);
	BUG_ON(!system->pdev);

	probe_info("start in iomem area.\n");

	dev = &(system->pdev->dev);
	iomem_count = of_property_count_strings(
			dev->of_node, "samsung,npumem-names") / 2;
	if (IS_ERR_VALUE((unsigned long)iomem_count)) {
		probe_err("invalid iomem list in %s node", dev->of_node->name);
		ret = -EINVAL;
		goto err_exit;
	}
	probe_info("%d iomem items\n", iomem_count);

	iomem_data = (struct iomem_reg_t *)devm_kzalloc(dev,
			iomem_count * sizeof(struct iomem_reg_t), GFP_KERNEL);
	if (!iomem_data) {
		probe_err("failed to alloc for iomem data");
		ret = -ENOMEM;
		goto err_exit;
	}

	ret = of_property_read_u32_array(dev->of_node, "samsung,npumem-address", (u32 *)iomem_data,
			iomem_count * sizeof(struct iomem_reg_t) / sizeof(u32));
	if (ret) {
		probe_err("failed to get iomem data");
		ret = -EINVAL;
		goto err_data;
	}

	si = 0; mi = 0;
	for (i = 0; i < iomem_count; i++) {
		ret = of_property_read_string_index(dev->of_node,
				"samsung,npumem-names", i * 2, &iomem_name);
		if (ret) {
			probe_err("failed to read iomem name %d from %s node\n",
					i, dev->of_node->name);
			ret = -EINVAL;
			goto err_data;
		}
		ret = of_property_read_string_index(dev->of_node,
				"samsung,npumem-names", i * 2 + 1, &heap_name);
		if (ret) {
			probe_err("failed to read iomem type for %s\n",
					iomem_name);
			ret = -EINVAL;
			goto err_data;
		}
		if (strcmp(heap_name, "SFR")) {
			mt = &((system->mem_area)[mi]);
			mt->heapname = heap_name;
			mt->name = iomem_name;
			probe_info("MEM %s(%d) uses %s\n", mt->name, mi,
				strcmp("", mt->heapname) ? mt->heapname : "System Heap");

			mt->area_info = (struct npu_memory_buffer *)devm_kzalloc(dev, sizeof(struct npu_memory_buffer), GFP_KERNEL);
			if (mt->area_info == NULL) {
				probe_err("error allocating npu buffer\n");
				goto err_data;
			}
			mt->area_info->size = (iomem_data + i)->size;
			probe_info("Flags[%08x], DVA[%08x], SIZE[%08x]",
				(iomem_data + i)->vaddr, (iomem_data + i)->paddr, (iomem_data + i)->size);
			ret = npu_memory_alloc_from_heap(system->pdev, mt->area_info,
					(iomem_data + i)->paddr, mt->heapname, (iomem_data + i)->vaddr);
			if (ret) {
				for (k = 0; k < mi; k++) {
					bd = (system->mem_area)[k].area_info;
					if (bd) {
						if (bd->vaddr)
							npu_memory_free_from_heap(&system->pdev->dev, bd);
						devm_kfree(dev, bd);
					}
				}
				probe_err("buffer allocation for %s heap failed w/ err: %d\n",
						mt->name, ret);
				ret = -EFAULT;
				goto err_data;
			}
			probe_info("%s : Paddr[%08x], [%08x] alloc memory\n",
					iomem_name, (iomem_data + i)->paddr, (iomem_data + i)->size);
			mi++;
		}	else {
			it = &((system->io_area)[si]);
			it->heapname = heap_name;
			it->name = iomem_name;
			probe_info("SFR %s(%d)\n", it->name, si);

			it->area_info = (struct npu_iomem_area *)devm_kzalloc(dev, sizeof(struct npu_iomem_area), GFP_KERNEL);
			if (it->area_info == NULL) {
				probe_err("error allocating npu iomem\n");
				goto err_data;
			}
			id = (struct npu_iomem_area *)it->area_info;
			id->paddr = (iomem_data + i)->paddr;
			id->size = (iomem_data + i)->size;
#ifdef CONFIG_NPU_LOOPBACK
			id->vaddr = kmalloc(id->size, GFP_KERNEL);
#else
			iomem = devm_ioremap_nocache(&(system->pdev->dev),
				(iomem_data + i)->paddr, (iomem_data + i)->size);
			if (IS_ERR_OR_NULL(iomem)) {
				probe_err("fail(%pK) in devm_ioremap_nocache(0x%08x, %u)\n",
					iomem, id->paddr, (u32)id->size);
				ret = -EFAULT;
				goto err_data;
			}
			id->vaddr = iomem;
#endif
			probe_info("%s : Paddr[%08x], [%08x] => Mapped @[%pK], Length = %llu\n",
					iomem_name, (iomem_data + i)->paddr, (iomem_data + i)->size,
					id->vaddr, id->size);

			/* initial settings for this area
			 * use dt "samsung,npumem-xxx"
			 */
			tmpname[0] = '\0';
			strcpy(tmpname, "samsung,npumem-");
			strcat(tmpname, iomem_name);
			init_count = of_property_read_variable_u32_array(dev->of_node,
					tmpname, (u32 *)iomem_init_data,
					sizeof(struct reg_set_map) / sizeof(u32), 0);
			if (init_count > 0) {
				probe_trace("start in init settings for %s\n", iomem_name);
				ret = npu_set_hw_reg(id,
						(struct reg_set_map *)iomem_init_data,
						init_count / sizeof(struct reg_set_map), 0);
				if (ret) {
					npu_err("fail(%d) in npu_set_hw_reg\n", ret);
					goto err_data;
				}
				probe_info("complete in %d init settings for %s\n",
						(int)(init_count / sizeof(struct reg_set_map)),
						iomem_name);
			}
			si++;
		}
	}

	/* reserved memory */
	rmem_cnt = 0;

	domain = iommu_get_domain_for_dev(dev);
	if (!domain) {
		probe_err("fail to get domain for dnc\n");
		goto err_data;
	}
	system->domain = domain;

	mems_node = of_get_child_by_name(dev->of_node, "samsung,npurmem-address");
	if (!mems_node) {
		ret = 0;	/* not an error */
		probe_err("null npurmem-address node\n");
		goto err_data;
	}

	for_each_child_of_node(mems_node, mem_node) {
		rmt = &((system->rmem_area)[rmem_cnt]);
		rmt->area_info = (struct npu_memory_buffer *)devm_kzalloc(dev, sizeof(struct npu_memory_buffer), GFP_KERNEL);

		rmt->name = kbasename(mem_node->full_name);
		ret = of_property_read_u32(mem_node,
				"iova", (u32 *) &rmt->area_info->daddr);
		if (ret) {
			probe_err("'iova' is mandatory but not defined\n");
			goto err_data;
		}

		phdl_cnt = of_count_phandle_with_args(mem_node,
				"memory-region", NULL);
		if (phdl_cnt > 1) {
			probe_err("only one phandle required. "
					"phdl_cnt(%d)\n", phdl_cnt);
			ret = -EINVAL;
			goto err_data;
		}

		if (phdl_cnt == 1) {	/* reserved mem case */
			phdl_node = of_parse_phandle(mem_node,
					"memory-region", 0);
			if (!phdl_node) {
				ret = -EINVAL;
				probe_err("fail to get memory-region in name(%s)\n", rmt->name);
				goto err_data;
			}

			rsvd_mem = of_reserved_mem_lookup(phdl_node);
			if (!rsvd_mem) {
				ret = -EINVAL;
				probe_err("fail to look-up rsvd mem(%s)\n", rmt->name);
				goto err_data;
			}
			rmt->rmem = rsvd_mem;
		}

		ret = of_property_read_u32(mem_node,
				"size", &size);
		if (ret) {
			probe_err("'size' is mandatory but not defined\n");
			goto err_data;
		} else {
			if (size > rmt->rmem->size) {
				ret = - EINVAL;
				probe_err("rmt->size(%x) > rsvd_size(%llx)\n", size, rmt->rmem->size);
				goto err_data;
			}
		}
		rmt->area_info->size = size;

		/* iommu map */
		ret = iommu_map(system->domain, rmt->area_info->daddr, rmt->rmem->base,
				rmt->area_info->size, 0);
		if (ret) {
			probe_err("fail to map iova for rmem(%s) ret(%d)\n",
					rmt->name, ret);
			goto err_data;
		}

		ret = npu_rsvd_map(system, rmt);
		if (ret) {
			probe_err("fail to map kvmap, rmem(%s)", rmt->name);
			goto err_data;
		}

		rmem_cnt++;
	}

	/* set NULL for last */
	(system->mem_area)[mi].heapname = NULL;
	(system->mem_area)[mi].name = NULL;
	(system->rmem_area)[rmem_cnt].heapname = NULL;
	(system->rmem_area)[rmem_cnt].name = NULL;
	(system->io_area)[si].heapname = NULL;
	(system->io_area)[si].name = NULL;

	ret = of_property_read_u32(dev->of_node,
			"samsung,npusys-corenum", &core_num);
	if (ret)
		core_num = NPU_SYSTEM_DEFAULT_CORENUM;

	set_max_npu_core(system, core_num);

	npu_flush_memory_buffer = npu_get_mem_area(system, "fwmemory");

	probe_info("complete in init_iomem_area\n");
err_data:
	devm_kfree(dev, iomem_data);
err_exit:
	return ret;
}


#ifdef CONFIG_NPU_USE_VOTF
static int __npu_system_sharedMem_mapped(struct npu_device *device)
{
	int ret = 0;
	struct device_node *np;
	struct reserved_mem *rmem;
	struct iommu_domain *domain;

	np = of_parse_phandle(device->dev->of_node, "memory-region", 0);
	if (!np) {
		ret = -EFAULT;
		probe_err("Failed to get memory-region from DT\n");
		goto p_err;
	}

	rmem = of_reserved_mem_lookup(np);
	if (!rmem) {
		ret = -EFAULT;
		probe_err("Failed to get reserved_mem\n");
		goto p_err;
	}

	domain = iommu_get_domain_for_dev(device->dev);

	if (configs[NPU_SHARED_MEM_PAYLOAD] != (-1)) {
		ret = iommu_map(domain, configs[NPU_SHARED_MEM_PAYLOAD], rmem->base, rmem->size, 0);
		probe_info("%s rmem->base = %llx, rmem->size = %llx DVA = %x\n", __func__, rmem->base, rmem->size, configs[NPU_SHARED_MEM_PAYLOAD]);
		if (ret) {
			ret = -EFAULT;
			probe_err("Failed iommu_map %d\n", ret);
			goto p_err;
		}
	}

	if (configs[NPU_C2AGENT_0] != (-1)) {
		ret = iommu_map(domain, configs[NPU_C2AGENT_0], configs[NPU_C2AGENT_0], configs[NPU_VOTF_SIZE], 0);
		probe_info("%s base = %x, size = %x. \n", __func__, configs[NPU_C2AGENT_0], configs[NPU_VOTF_SIZE]);
		if (ret) {
			ret = -EFAULT;
			probe_err("Failed iommu_map %d\n", ret);
			goto p_err;
		}
	}


	if (configs[NPU_C2AGENT_1] != (-1)) {
		ret = iommu_map(domain, configs[NPU_C2AGENT_1], configs[NPU_C2AGENT_1], configs[NPU_VOTF_SIZE], 0);
		probe_info("%s base = %x, size = %x. \n", __func__, configs[NPU_C2AGENT_1], configs[NPU_VOTF_SIZE]);
		if (ret) {
			ret = -EFAULT;
			probe_err("Failed iommu_map %d\n", ret);
			goto p_err;
		}
	}

	if (configs[NPU_VOTF] != (-1)) {
		ret = iommu_map(domain, configs[NPU_VOTF], configs[NPU_VOTF], configs[NPU_VOTF_SIZE], 0);
		probe_info("%s base = %x, size = %x. \n", __func__, configs[NPU_VOTF], configs[NPU_VOTF_SIZE]);
		if (ret) {
			ret = -EFAULT;
			probe_err("Failed iommu_map %d\n", ret);
			goto p_err;
		}
	}

	return 0;

p_err:
	return ret;
}
#endif

static int npu_clk_init(struct npu_system *system)
{
	/* TODO : need core driver */
	return 0;
}

static int npu_cpu_on(struct npu_system *system)
{
	int ret = 0;
	struct npu_iomem_area *tcusram;
	struct npu_memory_buffer *fwmem;

	BUG_ON(!system);

	npu_info("start in npu_cpu_on\n");

	tcusram = npu_get_io_area(system, "tcusram");
	fwmem = npu_get_mem_area(system, "fwmemory");

#ifdef CONFIG_NPU_USE_MBR
	npu_info("use Master Boot Record\n");
	npu_info("ask to jump to  0x%x\n", (u32)fwmem->daddr);
	ret = dsp_npu_release(true, fwmem->daddr);
	if (ret) {
		npu_err("Failed to release CPU_SS : %d\n", ret);
		goto err_exit;
	}
#else
	if (tcusram) {
		memset_io(tcusram->vaddr, 0x0, tcusram->size);
#ifdef CONFIG_NPU_USE_JUMPCODE
		/* load SRAM binary */
		memcpy_toio(tcusram->vaddr, npu_jump_code, npu_jump_code_len);
#endif
	}

	ret = npu_cmd_map(system, "cpuon");
	if (ret) {
		npu_err("fail(%d) in npu_cmd_map for cpu_on\n", ret);
		goto err_exit;
	}
	npu_info("release CA32\n");
#endif

	npu_info("complete in npu_cpu_on\n");
	return 0;
err_exit:
	npu_info("error(%d) in npu_cpu_on\n", ret);
	return ret;
}

static int npu_cpu_off(struct npu_system *system)
{
	int ret = 0;

#ifdef CONFIG_NPU_USE_MBR
	ret = dsp_npu_release(false, 0);
	if (ret) {
		npu_err("Failed to release CPU_SS : %d\n", ret);
		goto err_exit;
	}
#else
	BUG_ON(!system);

	npu_info("start in npu_cpu_off\n");
	ret = npu_cmd_map(system, "cpuoff");
	if (ret) {
		npu_err("fail(%d) in npu_cmd_map for cpu_off\n", ret);
		goto err_exit;
	}
#endif
	npu_info("complete in npu_cpu_off\n");
	return 0;
err_exit:
	npu_info("error(%d) in npu_cpu_off\n", ret);
	return ret;
}

int npu_pwr_on(struct npu_system *system)
{
	int ret = 0;

	BUG_ON(!system);

	npu_info("start in npu_pwr_on\n");

	ret = npu_cmd_map(system, "pwron");
	if (ret) {
		npu_err("fail(%d) in npu_cmd_map for pwr_on\n", ret);
		goto err_exit;
	}

	npu_info("complete in npu_pwr_on\n");
	return 0;
err_exit:
	npu_info("error(%d) in npu_pwr_on\n", ret);
	return ret;

}

static int npu_system_soc_probe(struct npu_system *system, struct platform_device *pdev)
{
	int ret = 0;
#ifdef CONFIG_NPU_USE_VOTF
	struct npu_device *device;
#endif

	BUG_ON(!system);
	probe_info("system soc probe: ioremap areas\n");

#ifdef CONFIG_NPU_USE_VOTF
	device = container_of(system, struct npu_device, system);
	probe_info("system soc probe: __npu_system_sharedMem_mapped\n");
	ret = __npu_system_sharedMem_mapped(device);
	if (ret) {
		probe_err("fail(%d) in  __npu_system_sharedMem_mapped \n", ret);
		goto p_err;
	}
#endif

	ret = npu_init_iomem_area(system);
	if (ret) {
		probe_err("fail(%d) in init iomem area\n", ret);
		goto p_err;
	}

	ret = npu_init_cmd_list(system);
	if (ret) {
		probe_err("fail(%d) in cmd list register\n", ret);
		goto p_err;
	}

	ret = npu_stm_probe(system);
	if (ret)
		probe_err("npu_stm_probe error : %d\n", ret);

p_err:
	return ret;
}

int npu_system_probe(struct npu_system *system, struct platform_device *pdev)
{
	int ret = 0;
	struct device *dev;
	void *addr1, *addr2;
	int irq;
	struct npu_device *device;

	BUG_ON(!system);
	BUG_ON(!pdev);

	dev = &pdev->dev;
	device = container_of(system, struct npu_device, system);
	system->pdev = pdev;	/* TODO: Reference counting ? */

	irq = platform_get_irq(pdev, 0);
	if (irq < 0) {
		probe_err("fail(%d) in platform_get_irq(0)\n", irq);
		ret = -EINVAL;
		goto p_err;
	}
	system->irq0 = irq;

	irq = platform_get_irq(pdev, 1);
	if (irq < 0) {
		probe_err("fail(%d) in platform_get_irq(1)\n", irq);
		ret = -EINVAL;
		goto p_err;
	}
	system->irq1 = irq;

	irq = platform_get_irq(pdev, 2);
	if (irq < 0) {
		probe_err("fail(%d) in platform_get_irq(2)\n", irq);
		ret = -EINVAL;
		goto p_err;
	}
	system->irq2 = irq;

	ret = npu_memory_probe(&system->memory, dev);
	if (ret) {
		probe_err("fail(%d) in npu_memory_probe\n", ret);
		goto p_err;
	}

	/* Invoke platform specific probe routine */
	ret = npu_system_soc_probe(system, pdev);
	if (ret) {
		probe_err("fail(%d) in npu_system_soc_probe\n", ret);
		goto p_err;
	}

	addr1 = (void *)npu_get_io_area(system, "sfrmbox0")->vaddr;
	addr2 = (void *)npu_get_io_area(system, "sfrmbox1")->vaddr;
	ret = npu_interface_probe(dev, addr1, addr2);
	if (ret) {
		probe_err("fail(%d) in npu_interface_probe\n", ret);
		goto p_err;
	}

	ret = npu_binary_init(&system->binary,
		dev,
		NPU_FW_PATH1,
		NPU_FW_PATH2,
		NPU_FW_NAME);
	if (ret) {
		probe_err("fail(%d) in npu_binary_init\n", ret);
		goto p_err;
	}
	ret = npu_util_memdump_probe(system);
	if (ret) {
		probe_err("fail(%d) in npu_util_memdump_probe\n", ret);
		goto p_err;
	}

	ret = npu_iommu_dma_enable_best_fit_algo(dev);
	if (ret) {
		probe_err("fail(%d) npu_iommu_dma_enable_best_fit_algo\n", ret);
		goto p_err;
	}

	/* enable runtime pm */
	pm_runtime_enable(dev);
#if 0
	/* get npu clock, the order in list matters */
	ret = npu_clk_get(&system->clks, dev);
	if (ret) {
		probe_err("fail(%d) npu_clk_get\n", ret);
		goto p_err;
	}
#endif

	ret = npu_scheduler_probe(device);
	if (ret) {
		probe_err("npu_scheduler_probe is fail(%d)\n", ret);
		goto p_qos_err;
	}

	ret = npu_qos_probe(system);
	if (ret) {
		probe_err("npu_qos_probe is fail(%d)\n", ret);
		goto p_qos_err;
	}

	ret = npu_imgloader_probe(&system->binary, dev);
	if (ret) {
		probe_err("npu_imgloader_probe is fail(%d)\n", ret);
		goto p_qos_err;
	}

#ifdef CONFIG_PM_SLEEP
	/* initialize the npu wake lock */
	npu_wake_lock_init(dev, &system->ws,
				NPU_WAKE_LOCK_SUSPEND, "npu_run_wlock");
#endif
	init_waitqueue_head(&sysPwr.wq);
	sysPwr.system_result.result_code = NPU_SYSTEM_JUST_STARTED;

	goto p_exit;
p_qos_err:
	pm_runtime_disable(dev);
	//npu_clk_put(&system->clks, dev);
p_err:
p_exit:
	return ret;
}

static int npu_system_soc_release(struct npu_system *system, struct platform_device *pdev)
{
	int ret;

	ret = npu_stm_release(system);
	if (ret)
		npu_err("npu_stm_release error : %d\n", ret);
	return 0;
}

/* TODO: Implement throughly */
int npu_system_release(struct npu_system *system, struct platform_device *pdev)
{
	int ret = 0;
	struct device *dev;
	struct npu_device *device;

	BUG_ON(!system);
	BUG_ON(!pdev);

	dev = &pdev->dev;
	device = container_of(system, struct npu_device, system);

#ifdef CONFIG_PM_SLEEP
	npu_wake_lock_destroy(system->ws);
#endif

	pm_runtime_disable(dev);

#if 0
	/* put npu clock, reverse order in list */
	npu_clk_put(&system->clks, dev);
#endif
	ret = npu_scheduler_release(device);
	if (ret)
		probe_err("fail(%d) in npu_scheduler_release\n", ret);
	/* Invoke platform specific release routine */
	ret = npu_system_soc_release(system, pdev);
	if (ret)
		probe_err("fail(%d) in npu_system_soc_release\n", ret);

	imgloader_desc_release(&system->binary.imgloader);

	return ret;
}

int npu_system_open(struct npu_system *system)
{
	int ret = 0;
#ifdef CONFIG_NPU_HARDWARE
	struct device *dev;
	struct npu_device *device;

	BUG_ON(!system);
	BUG_ON(!system->pdev);
	dev = &system->pdev->dev;
	device = container_of(system, struct npu_device, system);

	ret = npu_memory_open(&system->memory);
	if (ret) {
		npu_err("fail(%d) in npu_memory_open\n", ret);
		goto p_err;
	}

	ret = npu_util_memdump_open(system);
	if (ret) {
		npu_err("fail(%d) in npu_util_memdump_open\n", ret);
		goto p_err;
	}
	ret = npu_scheduler_open(device);
	if (ret) {
		npu_err("fail(%d) in npu_scheduler_open\n", ret);
		goto p_err;
	}
	// to check available max freq for current NPU dvfs governor, need to finish scheduler open
	// then we can set boost as available
	npu_scheduler_boost_on(device->sched);

	ret = npu_qos_open(system);
	if (ret) {
		npu_err("fail(%d) in npu_qos_open\n", ret);
		goto p_err;
	}

	/* Clear resume steps */
	system->resume_steps = 0;
p_err:
#endif
	return ret;
}

int npu_system_close(struct npu_system *system)
{
	int ret = 0;
	struct npu_device *device;

	BUG_ON(!system);
	device = container_of(system, struct npu_device, system);

#ifdef CONFIG_FIRMWARE_SRAM_DUMP_DEBUGFS
	ret = npu_util_memdump_close(system);
	if (ret)
		npu_err("fail(%d) in npu_util_memdump_close\n", ret);
#endif
	ret = npu_memory_close(&system->memory);
	if (ret)
		npu_err("fail(%d) in npu_memory_close\n", ret);

	ret = npu_scheduler_close(device);
	if (ret)
		npu_err("fail(%d) in npu_scheduler_close\n", ret);

	ret = npu_qos_close(system);
	if (ret)
		npu_err("fail(%d) in npu_qos_close\n", ret);

	imgloader_shutdown(&system->binary.imgloader);

	return ret;
}

static inline void print_iomem_area(const char *pr_name, const struct npu_iomem_area *mem_area)
{
	if (mem_area->vaddr)
		npu_info(KERN_CONT "(%8s) Phy(0x%08x)-(0x%08llx) Virt(%pK) Size(%llu)\n",
			pr_name, mem_area->paddr, mem_area->paddr + mem_area->size,
			mem_area->vaddr, mem_area->size);
}

static void print_all_iomem_area(const struct npu_system *system)
{
	int i;

	npu_dbg("start in IOMEM mapping\n");
	for (i = 0; i < NPU_MAX_IO_DATA && system->io_area[i].name != NULL; i++) {
		if (system->io_area[i].area_info)
			print_iomem_area(system->io_area[i].name, system->io_area[i].area_info);
	}
	npu_dbg("end in IOMEM mapping\n");
}

static int npu_system_soc_resume(struct npu_system *system, u32 mode)
{
	int ret = 0;
#ifdef CONFIG_NPU_LOOPBACK
	return ret;
#endif
	BUG_ON(!system);

	/* Clear resume steps */
	system->resume_soc_steps = 0;

	print_all_iomem_area(system);

	ret = npu_pwr_on(system);
	if (ret) {
		npu_err("fail(%d) in npu_pwr_on\n", ret);
		goto p_err;
	}

	npu_clk_init(system);

	ret = npu_cpu_on(system);
	if (ret) {
		npu_err("fail(%d) in npu_cpu_on\n", ret);
		goto p_err;
	}
	set_bit(NPU_SYS_RESUME_SOC_CPU_ON, &system->resume_soc_steps);

	set_bit(NPU_SYS_RESUME_SOC_COMPLETED, &system->resume_soc_steps);

	return ret;
p_err:
	npu_err("Failure detected[%d].\n", ret);
	return ret;
}

int npu_system_soc_suspend(struct npu_system *system)
{
	int ret = 0;
#ifdef CONFIG_NPU_LOOPBACK
	return ret;
#endif
	BUG_ON(!system);

	BIT_CHECK_AND_EXECUTE(NPU_SYS_RESUME_SOC_COMPLETED, &system->resume_soc_steps, NULL, ;);

	BIT_CHECK_AND_EXECUTE(NPU_SYS_RESUME_SOC_STM_DISABLE, &system->resume_soc_steps, NULL, {
		ret = npu_stm_disable(system);
		if (ret)
			npu_err("fail(%d) in npu_stm_disable\n", ret);
	});

	BIT_CHECK_AND_EXECUTE(NPU_SYS_RESUME_SOC_CPU_ON, &system->resume_soc_steps, "Turn NPU cpu off", {
		ret = npu_cpu_off(system);
		if (ret)
			npu_err("fail(%d) in npu_cpu_off\n", ret);
	});

	if (system->resume_soc_steps != 0)
		npu_warn("Missing clean-up steps [%lu] found.\n", system->resume_soc_steps);

	/* Function itself never be failed, even thought there was some error */
	return ret;
}

int npu_system_resume(struct npu_system *system, u32 mode)
{
	int ret = 0;
	void *addr;
	struct device *dev;
	struct npu_device *device;
	struct npu_memory_buffer *fwmem;

	BUG_ON(!system);
	BUG_ON(!system->pdev);

	dev = &system->pdev->dev;

	device = container_of(system, struct npu_device, system);
	fwmem = npu_get_mem_area(system, "fwmemory");

	/* Clear resume steps */
	system->resume_steps = 0;

#ifdef CONFIG_PM_SLEEP
	/* prevent the system to suspend */
	if (!npu_wake_lock_active(system->ws)) {
		npu_wake_lock(system->ws);
		npu_info("wake_lock, now(%d)\n", npu_wake_lock_active(system->ws));
	}
	set_bit(NPU_SYS_RESUME_SETUP_WAKELOCK, &system->resume_steps);
#endif

	ret = npu_system_alloc_fw_dram_log_buf(system);
	if (ret) {
		npu_err("fail(%d) in npu_system_alloc_fw_dram_log_buf\n", ret);
		goto p_err;
	}
	set_bit(NPU_SYS_RESUME_INIT_FWBUF, &system->resume_steps);

	npu_info("reset FW working memory : paddr 0x%08x, vaddr 0x%08x, daddr 0x%08x, size %lu\n",
		fwmem->paddr, fwmem->vaddr, fwmem->daddr, fwmem->size);
	memset(fwmem->vaddr, 0, fwmem->size);

	ret = npu_firmware_load(system, 0);
	if (ret) {
		npu_err("fail(%d) in npu_firmware_load\n", ret);
		goto p_err;
	}
	set_bit(NPU_SYS_RESUME_FW_LOAD, &system->resume_steps);
	set_bit(NPU_SYS_RESUME_FW_VERIFY, &system->resume_steps);

	ret = npu_clk_prepare_enable(&system->clks);
	if (ret) {
		npu_err("fail to prepare and enable npu_clk(%d)\n", ret);
		goto p_err;
	}
	set_bit(NPU_SYS_RESUME_CLK_PREPARE, &system->resume_steps);

	/* Clear mailbox area and setup some initialization variables */
	addr = (void *)fwmem->vaddr;
	system->mbox_hdr = (volatile struct mailbox_hdr *)(addr + configs[NPU_MAILBOX_BASE] - sizeof(struct mailbox_hdr));
	memset((void *)system->mbox_hdr, 0, sizeof(*system->mbox_hdr));
#ifdef CONFIG_NPU_LOOPBACK
	system->mbox_hdr->signature1 = MAILBOX_SIGNATURE1;
#endif
#if (MAILBOX_VERSION >= 7)
	/* Save hardware info */
	system->mbox_hdr->hw_info = npu_get_hw_info();
#endif
	flush_kernel_vmap_range((void *)system->mbox_hdr, sizeof(*system->mbox_hdr));

	/* Invoke platform specific resume routine */
	ret = npu_system_soc_resume(system, mode);
	if (ret) {
		npu_err("fail(%d) in npu_system_soc_resume\n", ret);
		goto p_err;
	}
	set_bit(NPU_SYS_RESUME_SOC, &system->resume_steps);

	ret = npu_interface_open(system);
	if (ret) {
		npu_err("fail(%d) in npu_interface_open\n", ret);
		goto p_err;
	}
	set_bit(NPU_SYS_RESUME_OPEN_INTERFACE, &system->resume_steps);

	ret = npu_stm_enable(system);
	if (ret) {
		npu_err("fail(%d) in npu_stm_enable\n", ret);
		goto p_err;
	}
	set_bit(NPU_SYS_RESUME_SOC_STM_SETUP, &system->resume_soc_steps);

	set_bit(NPU_SYS_RESUME_COMPLETED, &system->resume_steps);

	return ret;
p_err:
	npu_err("Failure detected[%d]. Set emergency recovery flag.\n", ret);
	set_bit(NPU_DEVICE_ERR_STATE_EMERGENCY, &device->err_state);
	ret = 0;//emergency case will be cared by suspend func
	return ret;
}

int npu_system_suspend(struct npu_system *system)
{
	int ret = 0;
	struct device *dev;
	struct npu_device *device;

	BUG_ON(!system);
	BUG_ON(!system->pdev);

	dev = &system->pdev->dev;
	device = container_of(system, struct npu_device, system);

	BIT_CHECK_AND_EXECUTE(NPU_SYS_RESUME_COMPLETED, &system->resume_steps, NULL, ;);

	BIT_CHECK_AND_EXECUTE(NPU_SYS_RESUME_OPEN_INTERFACE, &system->resume_steps, "Close interface", {
		ret = npu_interface_close(system);
		if (ret)
			npu_err("fail(%d) in npu_interface_close\n", ret);
	});

	/* Invoke platform specific suspend routine */
	BIT_CHECK_AND_EXECUTE(NPU_SYS_RESUME_SOC, &system->resume_steps, "SoC suspend", {
		ret = npu_system_soc_suspend(system);
		if (ret)
			npu_err("fail(%d) in npu_system_soc_suspend\n", ret);
	});

	BIT_CHECK_AND_EXECUTE(NPU_SYS_RESUME_FW_VERIFY, &system->resume_steps, "FW VERIFY suspend", {
		/* Do not need anything */
	});

#ifdef CONFIG_PM_SLEEP
	BIT_CHECK_AND_EXECUTE(NPU_SYS_RESUME_SETUP_WAKELOCK, &system->resume_steps, "Unlock wake lock", {
		if (npu_wake_lock_active(system->ws)) {
			npu_wake_unlock(system->ws);
			npu_info("wake_unlock, now(%d)\n", npu_wake_lock_active(system->ws));
		}
	});
#endif

	BIT_CHECK_AND_EXECUTE(NPU_SYS_RESUME_CLK_PREPARE, &system->resume_steps, "Unprepare clk", {
		npu_clk_disable_unprepare(&system->clks);
	});

	BIT_CHECK_AND_EXECUTE(NPU_SYS_RESUME_FW_LOAD, &system->resume_steps, NULL, ;);
	BIT_CHECK_AND_EXECUTE(NPU_SYS_RESUME_INIT_FWBUF, &system->resume_steps, "Free DRAM fw log buf", {
		ret = npu_system_free_fw_dram_log_buf();
		if (ret)
			npu_err("fail(%d) in npu_cpu_off\n", ret);
	});

	if (system->resume_steps != 0)
		npu_warn("Missing clean-up steps [%lu] found.\n", system->resume_steps);

	/* Function itself never be failed, even thought there was some error */
	return 0;
}

int npu_system_start(struct npu_system *system)
{
	int ret = 0;
	struct device *dev;
	struct npu_device *device;

	BUG_ON(!system);
	BUG_ON(!system->pdev);

	dev = &system->pdev->dev;
	device = container_of(system, struct npu_device, system);

#ifdef CONFIG_FIRMWARE_SRAM_DUMP_DEBUGFS
	ret = npu_util_memdump_start(system);
	if (ret) {
		npu_err("fail(%d) in npu_util_memdump_start\n", ret);
		goto p_err;
	}
#endif

	ret = npu_scheduler_start(device);
	if (ret) {
		npu_err("fail(%d) in npu_scheduler_start\n", ret);
		goto p_err;
	}

p_err:

	return ret;
}

int npu_system_stop(struct npu_system *system)
{
	int ret = 0;
	struct device *dev;
	struct npu_device *device;

	BUG_ON(!system);
	BUG_ON(!system->pdev);

	dev = &system->pdev->dev;
	device = container_of(system, struct npu_device, system);

#ifdef CONFIG_FIRMWARE_SRAM_DUMP_DEBUGFS
	ret = npu_util_memdump_stop(system);
	if (ret) {
		npu_err("fail(%d) in npu_util_memdump_stop\n", ret);
		goto p_err;
	}
#endif

	ret = npu_scheduler_stop(device);
	if (ret) {
		npu_err("fail(%d) in npu_scheduler_stop\n", ret);
		goto p_err;
	}

p_err:

	return ret;
}

int npu_system_save_result(struct npu_session *session, struct nw_result nw_result)
{
	int ret = 0;
	sysPwr.system_result.result_code = nw_result.result_code;
	wake_up(&sysPwr.wq);
	return ret;
}

static int npu_firmware_load(struct npu_system *system, int mode)
{
	int ret = 0;
	u32 v;
	struct npu_memory_buffer *fwmem;

	BUG_ON(!system);

	fwmem = npu_get_mem_area(system, "fwmemory");

#ifdef CONFIG_NPU_HARDWARE
#ifdef CLEAR_SRAM_ON_FIRMWARE_LOADING
#ifdef CLEAR_ON_SECOND_LOAD_ONLY

	BUG_ON(!system->fwmemory)

	npu_info("Firmware load : Start\n");

	if (fwmem->vaddr) {
		v = readl(fwmem->vaddr + fwmem->size - sizeof(u32));
		npu_dbg("firmware load: Check current signature value : 0x%08x (%s)\n",
			v, (v == 0)?"First load":"Second load");
	}
#else
	v = 1;
#endif
	if (v != 0) {
		if (fwmem->vaddr) {
			npu_dbg("firmware load : clear working memory at %pK, Len(%llu)\n",
				fwmem->vaddr, (long long unsigned int)fwmem->size);
			/* Using memset here causes unaligned access fault.
			Refer: https://patchwork.kernel.org/patch/6362401/ */
			memset_io(fwmem->vaddr, 0, fwmem->size);
		}
	}
#else
	if (fwmem->vaddr) {
		npu_dbg("firmware load: clear firmware signature at %pK(u64)\n",
			fwmem->vaddr + fwmem->size - sizeof(u64));
		writel(0, fwmem->vaddr + fwmem->size - sizeof(u64));
	}
#endif
	if (fwmem->vaddr) {
		npu_dbg("firmware load: read and locate firmware to %pK\n", fwmem->vaddr);
		ret = npu_firmware_file_read_signature(&system->binary);
		if (ret) {
			npu_err("error(%d) in npu_binary_read\n", ret);
			goto err_exit;
		}
		npu_dbg("checking firmware head MAGIC(0x%08x)\n", *(u32 *)fwmem->vaddr);
	}

	//dma_sync_sg_for_device(fwmem->attachment->dev, fwmem->sgt->sgl,
	//		fwmem->sgt->orig_nents, DMA_TO_DEVICE);

	npu_info("complete in npu_firmware_load\n");
	return ret;
err_exit:

	npu_info("error(%d) in npu_firmware_load\n", ret);
#endif
	return ret;
}
