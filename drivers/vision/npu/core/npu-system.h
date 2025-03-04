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

#ifndef _NPU_SYSTEM_H_
#define _NPU_SYSTEM_H_

#include <linux/platform_device.h>
#include <linux/of_reserved_mem.h>
#include "npu-scheduler.h"
#include "npu-qos.h"
#include "npu-clock.h"
#include "npu-wakeup.h"

#ifdef CONFIG_NPU_HARDWARE
#include "npu-interface.h"
#include "mailbox.h"
#else
#ifdef CONFIG_NPU_LOOPBACK
#include "interface/loopback/npu-interface.h"
#include "interface/loopback/mailbox.h"
#endif

#endif

#ifdef CONFIG_NPU_USE_SPROFILER
#include "npu-profile.h"
#endif

#include "npu-exynos.h"
#if 0
#include "npu-interface.h"
#endif

#include "npu-memory.h"

#include "npu-binary.h"
#if 0
#include "npu-tv.h"
#endif

#include "npu-fw-test-handler.h"

#define NPU_SYSTEM_DEFAULT_CORENUM	1

struct iomem_reg_t {
	u32 vaddr;
	u32 paddr;
	u32 size;
};

struct npu_iomem_init_data {
	const char	*heapname;
	const char	*name;
	void	*area_info;       /* Save iomem result */
};

#define NPU_MAX_IO_DATA		32
struct npu_io_data {
	const char	*heapname;
	const char	*name;
	struct npu_iomem_area	*area_info;
};

#define NPU_MAX_MEM_DATA	32
struct npu_mem_data {
	const char	*heapname;
	const char	*name;
	struct npu_memory_buffer	*area_info;
};

struct npu_rmem_data {
	const char	*heapname;
	const char	*name;
	struct npu_memory_buffer	*area_info;
	struct reserved_mem *rmem;
};

struct reg_cmd_list {
	char			*name;
	struct reg_cmd_map	*list;
	int			count;
};

struct npu_system {
	struct platform_device	*pdev;

	struct npu_io_data	io_area[NPU_MAX_IO_DATA];
	struct npu_mem_data	mem_area[NPU_MAX_MEM_DATA];
	struct npu_rmem_data	rmem_area[NPU_MAX_MEM_DATA];
	struct iommu_domain *domain;

	struct reg_cmd_list	*cmd_list;

	struct npu_fw_test_handler	npu_fw_test_handler;

	int			irq0;
	int			irq1;
	int			irq2;

	struct npu_qos_setting	qos_setting;

	u32			max_npu_core;
	struct npu_clocks	clks;
#ifdef CONFIG_PM_SLEEP
	/* maintain to be awake */
	struct wakeup_source	*ws;
#endif

	struct npu_exynos exynos;
	struct npu_memory memory;
	struct npu_binary binary;

	volatile struct mailbox_hdr	*mbox_hdr;
	volatile struct npu_interface	*interface;

#ifdef CONFIG_NPU_USE_SPROFILER
	struct npu_profile_control	profile_ctl;
#endif
	/* Open status (Bitfield of npu_system_resume_steps) */
	unsigned long			resume_steps;
	unsigned long			resume_soc_steps;
};

static inline struct npu_io_data *npu_get_io_data(struct npu_system *system, const char *name)
{
	int i, t = -1;

	for (i = 0; i < NPU_MAX_IO_DATA && system->io_area[i].name != NULL; i++) {
		if (!strcmp(name, system->io_area[i].name)) {
			t = i;
			break;
		}
	}
	if (t < 0)
		return (struct npu_io_data *)NULL;
	else
		return (system->io_area + t);
}
static inline struct npu_iomem_area *npu_get_io_area(struct npu_system *system, const char *name)
{
	struct npu_io_data *t;
	t = npu_get_io_data(system, name);
	return t ? t->area_info : (struct npu_iomem_area *)NULL;
}

static inline struct npu_mem_data *npu_get_mem_data(struct npu_system *system, const char *name)
{
	int i, t = -1;

	for (i = 0; i < NPU_MAX_MEM_DATA && system->mem_area[i].name != NULL; i++) {
		if (!strcmp(name, system->mem_area[i].name)) {
			t = i;
			break;
		}
	}
	if (t < 0)
		return (struct npu_mem_data *)NULL;
	else
		return (system->mem_area + t);
}

static inline struct npu_rmem_data *npu_get_rmem_data(struct npu_system *system, const char *name)
{
	int i, t = -1;

	for (i = 0; i < NPU_MAX_MEM_DATA && system->rmem_area[i].name != NULL; i++) {
		if (!strcmp(name, system->rmem_area[i].name)) {
			t = i;
			break;
		}
	}
	if (t < 0)
		return (struct npu_rmem_data *)NULL;
	else
		return (system->rmem_area + t);
}

static inline struct npu_memory_buffer *npu_get_mem_area(struct npu_system *system, const char *name)
{
	struct npu_mem_data *t;
	struct npu_rmem_data *tr;
	t = npu_get_mem_data(system, name);
	if (t)
		return t->area_info;
	tr = npu_get_rmem_data(system, name);
	if (tr)
		return tr->area_info;
	return (struct npu_memory_buffer *)NULL;
}

static inline int get_iomem_data_index(const struct npu_iomem_init_data data[], const char *name)
{
	int i;
	for (i = 0; data[i].name != NULL; i++) {
		if (!strcmp(data[i].name, name))
			return i;
	}
	return -1;
}

static inline struct reg_cmd_list *get_npu_cmd_map(struct npu_system *system, const char *cmd_name)

{
	int i;
	for (i = 0; ((system->cmd_list) + i)->name != NULL; i++) {
		if (!strcmp(((system->cmd_list) + i)->name, cmd_name))
			return (system->cmd_list + i);
	}
	return (struct reg_cmd_list *)NULL;
}

int npu_system_probe(struct npu_system *system, struct platform_device *pdev);
int npu_system_release(struct npu_system *system, struct platform_device *pdev);
int npu_system_open(struct npu_system *system);
int npu_system_close(struct npu_system *system);
int npu_system_resume(struct npu_system *system, u32 mode);
int npu_system_suspend(struct npu_system *system);
int npu_system_start(struct npu_system *system);
int npu_system_stop(struct npu_system *system);

void npu_memory_sync_for_cpu(void);
void npu_memory_sync_for_device(void);
int npu_memory_alloc_from_heap(struct platform_device *pdev, struct npu_memory_buffer *buffer,
		dma_addr_t daddr, __attribute__((unused))const char *heapname, int prot);
void npu_memory_free_from_heap(struct device *dev, struct npu_memory_buffer *buffer);
void npu_soc_status_report(struct npu_system *system);
u32 npu_get_hw_info(void);
#endif
