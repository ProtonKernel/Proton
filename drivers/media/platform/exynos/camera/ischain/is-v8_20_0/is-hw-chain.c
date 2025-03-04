/*
 * Samsung Exynos SoC series FIMC-IS driver
 *
 * Exynos Pablo image subsystem functions
 *
 * Copyright (c) 2018 Samsung Electronics Co., Ltd
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#include <linux/module.h>
#include <linux/delay.h>
#include <linux/kernel.h>
#include <linux/errno.h>
#include <linux/platform_device.h>
#include <linux/io.h>

#include <asm/neon.h>

#include "is-config.h"
#include "is-param.h"
#include "is-type.h"
#include "is-core.h"
#include "is-hw-chain.h"
#include "is-hw-settle-8nm-lpp.h"
#include "is-device-sensor.h"
#include "is-device-csi.h"
#include "is-device-ischain.h"

#include "../../interface/is-interface-ischain.h"
#include "../../hardware/is-hw-control.h"
#include "../../hardware/is-hw-mcscaler-v3.h"

#include <linux/smc.h>

/* SYSREG register description */
static struct is_reg sysreg_is_regs[SYSREG_IS_REG_CNT] = {
	{0x0428, "IS_USER_GLUE_CON0"},
	{0x0430, "IS_USER_GLUE_CON2"},
	{0x0438, "IS_USER_GLUE_CON4"},
	{0x0440, "IS_USER_GLUE_CON6"},
	{0x0448, "IS_USER_GLUE_CON8"},
	{0x0450, "IS_USER_ACIC_CON7"},
	{0x0500, "MIPI_PHY_CON0"},
};

static struct is_field sysreg_is_fields[SYSREG_IS_REG_FIELD_CNT] = {
	{"MUX_FOR_OTF0", 0, 2, RW, 0x0},
	{"MUX_FOR_OTF1", 0, 2, RW, 0x1},
	{"MASK_FOR_CSIS0_OTF", 0, 1, RW, 0x0},
	{"MASK_FOR_CSIS1_OTF", 0, 1, RW, 0x0},
	{"MASK_FOR_CSIS2_OTF", 0, 1, RW, 0x0},
	{"AW_AC_TARGET_GDC", 15, 6, RW, 0x3f},
	{"AR_AC_TARGET_GDC", 0, 6, RW, 0x3f},
	{"MIPI_RESETN_DPHY_S2", 2, 1, RW, 0x0},
	{"MIPI_RESETN_DPHY_S4_1", 1, 1, RW, 0x0},
	{"MIPI_RESETN_DPHY_S4_0", 0, 1, RW, 0x0},
};

void __iomem *hwfc_rst;

/*
 * ISR definitions
 */
void is_enter_lib_isr(void)
{
#ifdef ENABLE_FPSIMD_FOR_USER
	kernel_neon_begin();
#endif
}

void is_exit_lib_isr(void)
{
#ifdef ENABLE_FPSIMD_FOR_USER
	kernel_neon_end();
#endif
}

static inline void __nocfi __is_isr_ddk(void *data, int handler_id)
{
	struct is_interface_hwip *itf_hw = NULL;
	struct hwip_intr_handler *intr_hw = NULL;

	itf_hw = (struct is_interface_hwip *)data;
	intr_hw = &itf_hw->handler[handler_id];

	if (intr_hw->valid) {
		is_enter_lib_isr();
		intr_hw->handler(intr_hw->id, intr_hw->ctx);
		is_exit_lib_isr();
	} else {
		err_itfc("[ID:%d](%d)- chain(%d) empty handler!!",
			itf_hw->id, handler_id, intr_hw->chain_id);
	}
}

static inline void __is_isr_host(void *data, int handler_id)
{
	struct is_interface_hwip *itf_hw = NULL;
	struct hwip_intr_handler *intr_hw = NULL;

	itf_hw = (struct is_interface_hwip *)data;
	intr_hw = &itf_hw->handler[handler_id];

	if (intr_hw->valid)
		intr_hw->handler(intr_hw->id, (void *)itf_hw->hw_ip);
	else
		err_itfc("[ID:%d](1) empty handler!!", itf_hw->id);
}

/*
 * Interrupt handler definitions
 */
/* IPP0 */
static irqreturn_t __is_isr1_3aa0(int irq, void *data)
{
	__is_isr_ddk(data, INTR_HWIP1);
	return IRQ_HANDLED;
}

static irqreturn_t __is_isr2_3aa0(int irq, void *data)
{
	__is_isr_ddk(data, INTR_HWIP2);
	return IRQ_HANDLED;
}

/* IPP1 */
static irqreturn_t __is_isr1_3aa1(int irq, void *data)
{
	__is_isr_ddk(data, INTR_HWIP1);
	return IRQ_HANDLED;
}

static irqreturn_t __is_isr2_3aa1(int irq, void *data)
{
	__is_isr_ddk(data, INTR_HWIP2);
	return IRQ_HANDLED;
}

/* ITP0 */
static irqreturn_t __is_isr1_itp0(int irq, void *data)
{
	__is_isr_ddk(data, INTR_HWIP1);
	return IRQ_HANDLED;
}

static irqreturn_t __is_isr2_itp0(int irq, void *data)
{
	__is_isr_ddk(data, INTR_HWIP2);
	return IRQ_HANDLED;
}

/* MCSC */
static irqreturn_t __is_isr1_mcs0(int irq, void *data)
{
	__is_isr_host(data, INTR_HWIP1);
	return IRQ_HANDLED;
}

static irqreturn_t __is_isr1_mcs1(int irq, void *data)
{
	__is_isr_host(data, INTR_HWIP1);
	return IRQ_HANDLED;
}

static irqreturn_t __is_isr1_vra(int irq, void *data)
{
	struct is_interface_hwip *itf_hw = NULL;
	struct hwip_intr_handler *intr_hw = NULL;

	itf_hw = (struct is_interface_hwip *)data;
	intr_hw = &itf_hw->handler[INTR_HWIP1];

	if (intr_hw->valid) {
		is_enter_lib_isr();
		intr_hw->handler(intr_hw->id, itf_hw->hw_ip);
		is_exit_lib_isr();
	} else {
		err_itfc("[ID:%d](2) empty handler!!", itf_hw->id);
	}
	return IRQ_HANDLED;
}

/*
 * HW group related functions
 */
void __is_hw_group_init(struct is_group *group)
{
	int i;

	for (i = ENTRY_SENSOR; i < ENTRY_END; i++)
		group->subdev[i] = NULL;

	INIT_LIST_HEAD(&group->subdev_list);
}

int is_hw_group_cfg(void *group_data)
{
	int ret = 0;
	struct is_group *group;
	struct is_device_sensor *sensor;
	struct is_device_ischain *device;

	FIMC_BUG(!group_data);

	group = (struct is_group *)group_data;

#ifdef CONFIG_USE_SENSOR_GROUP
	if (group->slot == GROUP_SLOT_SENSOR) {
		sensor = group->sensor;
		if (!sensor) {
			err("device is NULL");
			BUG();
		}

		__is_hw_group_init(group);
		group->subdev[ENTRY_SENSOR] = &sensor->group_sensor.leader;
		group->subdev[ENTRY_SSVC0] = &sensor->ssvc0;
		group->subdev[ENTRY_SSVC1] = &sensor->ssvc1;
		group->subdev[ENTRY_SSVC2] = &sensor->ssvc2;
		group->subdev[ENTRY_SSVC3] = &sensor->ssvc3;

		list_add_tail(&sensor->group_sensor.leader.list, &group->subdev_list);
		list_add_tail(&sensor->ssvc0.list, &group->subdev_list);
		list_add_tail(&sensor->ssvc1.list, &group->subdev_list);
		list_add_tail(&sensor->ssvc2.list, &group->subdev_list);
		list_add_tail(&sensor->ssvc3.list, &group->subdev_list);
		return ret;
	}
#endif

	device = group->device;
	if (!device) {
		err("device is NULL");
		BUG();
	}

	switch (group->slot) {
	case GROUP_SLOT_PAF:
		break;
	case GROUP_SLOT_3AA:
		__is_hw_group_init(group);
		group->subdev[ENTRY_3AA] = &device->group_3aa.leader;
		group->subdev[ENTRY_3AC] = &device->txc;
		group->subdev[ENTRY_3AP] = &device->txp;

		list_add_tail(&device->group_3aa.leader.list, &group->subdev_list);
		list_add_tail(&device->txc.list, &group->subdev_list);
		list_add_tail(&device->txp.list, &group->subdev_list);

		device->txc.param_dma_ot = PARAM_3AA_VDMA4_OUTPUT;
		device->txp.param_dma_ot = PARAM_3AA_VDMA2_OUTPUT;
		break;
	case GROUP_SLOT_ISP:
		__is_hw_group_init(group);
		group->subdev[ENTRY_ISP] = &device->group_isp.leader;
		group->subdev[ENTRY_IXC] = &device->ixc;
		group->subdev[ENTRY_IXP] = &device->ixp;

		list_add_tail(&device->group_isp.leader.list, &group->subdev_list);
		list_add_tail(&device->ixc.list, &group->subdev_list);
		list_add_tail(&device->ixp.list, &group->subdev_list);
		break;
	case GROUP_SLOT_MCS:
		__is_hw_group_init(group);
		group->subdev[ENTRY_MCS] = &device->group_mcs.leader;
		group->subdev[ENTRY_M0P] = &device->m0p;

		list_add_tail(&device->group_mcs.leader.list, &group->subdev_list);
		list_add_tail(&device->m0p.list, &group->subdev_list);

		device->m0p.param_dma_ot = PARAM_MCS_OUTPUT0;
		break;
	case GROUP_SLOT_VRA:
		__is_hw_group_init(group);
		group->subdev[ENTRY_VRA] = &device->group_vra.leader;

		list_add_tail(&device->group_vra.leader.list, &group->subdev_list);
		break;
	case GROUP_SLOT_CLH:
		break;
	default:
		probe_err("group slot(%d) is invalid", group->slot);
		BUG();
		break;
	}

	/* for hwfc: reset all REGION_IDX registers and outputs */
	hwfc_rst = ioremap(HWFC_INDEX_RESET_ADDR, SZ_4);

	return ret;
}

int is_hw_group_open(void *group_data)
{
	int ret = 0;
	u32 group_id;
	struct is_subdev *leader;
	struct is_group *group;
	struct is_device_ischain *device;

	FIMC_BUG(!group_data);

	group = group_data;
	leader = &group->leader;
	device = group->device;
	group_id = group->id;

	switch (group_id) {
#ifdef CONFIG_USE_SENSOR_GROUP
	case GROUP_ID_SS0:
	case GROUP_ID_SS1:
	case GROUP_ID_SS2:
	case GROUP_ID_SS3:
	case GROUP_ID_SS4:
	case GROUP_ID_SS5:
		leader->constraints_width = GROUP_SENSOR_MAX_WIDTH;
		leader->constraints_height = GROUP_SENSOR_MAX_HEIGHT;
		break;
#endif
	case GROUP_ID_3AA0:
	case GROUP_ID_3AA1:
		leader->constraints_width = GROUP_3AA_MAX_WIDTH;
		leader->constraints_height = GROUP_3AA_MAX_HEIGHT;
		break;
	case GROUP_ID_ISP0:
	case GROUP_ID_MCS0:
		leader->constraints_width = GROUP_ITP_MAX_WIDTH;
		leader->constraints_height = GROUP_ITP_MAX_HEIGHT;
		break;
	case GROUP_ID_VRA0:
		leader->constraints_width = GROUP_VRA_MAX_WIDTH;
		leader->constraints_height = GROUP_VRA_MAX_HEIGHT;
		break;
	case GROUP_ID_MCS1:
		leader->constraints_width = GROUP_MCSC_MFC_MAX_WIDTH;
		leader->constraints_height = GROUP_MCSC_MFC_MAX_HEIGHT;
		break;
	default:
		merr("(%s) is invalid", group, group_id_name[group_id]);
		break;
	}

	return ret;
}

inline int is_hw_slot_id(int hw_id)
{
	int slot_id = -1;

	switch (hw_id) {
	case DEV_HW_3AA0:
		slot_id = 0;
		break;
	case DEV_HW_3AA1:
		slot_id = 1;
		break;
	case DEV_HW_ISP0:
		slot_id = 2;
		break;
	case DEV_HW_MCSC0:
		slot_id = 3;
		break;
	case DEV_HW_VRA:
		slot_id = 4;
		break;
	case DEV_HW_MCSC1:
		slot_id = 5;
		break;
	case DEV_HW_3AA2:
	case DEV_HW_ISP1:
		break;
	default:
		err("Invalid hw id(%d)", hw_id);
		break;
	}

	return slot_id;
}

int is_get_hw_list(int group_id, int *hw_list)
{
	int i;
	int hw_index = 0;

	/* initialization */
	for (i = 0; i < GROUP_HW_MAX; i++)
		hw_list[i] = -1;

	switch (group_id) {
	case GROUP_ID_3AA0:
		hw_list[hw_index] = DEV_HW_3AA0; hw_index++;
		break;
	case GROUP_ID_3AA1:
		hw_list[hw_index] = DEV_HW_3AA1; hw_index++;
		break;
	case GROUP_ID_ISP0:
		hw_list[hw_index] = DEV_HW_ISP0; hw_index++;
		break;
	case GROUP_ID_MCS0:
		hw_list[hw_index] = DEV_HW_MCSC0; hw_index++;
		break;
	case GROUP_ID_VRA0:
		hw_list[hw_index] = DEV_HW_VRA; hw_index++;
		break;
	case GROUP_ID_MCS1:
		hw_list[hw_index] = DEV_HW_MCSC1; hw_index++;
		break;
	case GROUP_ID_MAX:
		break;
	default:
		err("Invalid group%d(%s)", group_id, group_id_name[group_id]);
		break;
	}

	return hw_index;
}
/*
 * System registers configurations
 */
static int is_hw_get_clk_gate(struct is_hw_ip *hw_ip, int hw_id)
{
	if (!hw_ip) {
		probe_err("hw_id(%d) hw_ip(NULL)", hw_id);
		return -EINVAL;
	}

	hw_ip->clk_gate_idx = 0;
	hw_ip->clk_gate = NULL;

	return 0;
}

int is_hw_get_address(void *itfc_data, void *pdev_data, int hw_id)
{
	int ret = 0;
	struct resource *mem_res = NULL;
	struct platform_device *pdev = NULL;
	struct is_interface_hwip *itf_hwip = NULL;
	int idx;

	FIMC_BUG(!itfc_data);
	FIMC_BUG(!pdev_data);

	itf_hwip = (struct is_interface_hwip *)itfc_data;
	pdev = (struct platform_device *)pdev_data;

	switch (hw_id) {
	case DEV_HW_3AA0:
		mem_res = platform_get_resource(pdev, IORESOURCE_MEM, IORESOURCE_3AA0);
		if (!mem_res) {
			dev_err(&pdev->dev, "Failed to get io memory region\n");
			return -EINVAL;
		}
		itf_hwip->hw_ip->regs_start[REG_SETA] = mem_res->start;
		itf_hwip->hw_ip->regs_end[REG_SETA] = mem_res->end;
		itf_hwip->hw_ip->regs[REG_SETA] = ioremap_nocache(mem_res->start, resource_size(mem_res));
		if (!itf_hwip->hw_ip->regs[REG_SETA]) {
			dev_err(&pdev->dev, "Failed to remap io region\n");
			return -EINVAL;
		}

		idx = 0;
		itf_hwip->hw_ip->dump_region[REG_SETA][idx].start = 0x0;
		itf_hwip->hw_ip->dump_region[REG_SETA][idx++].end = 0x1FB3;
		itf_hwip->hw_ip->dump_region[REG_SETA][idx].start = 0x1FB8;
		itf_hwip->hw_ip->dump_region[REG_SETA][idx++].end = 0x9FB3;
		itf_hwip->hw_ip->dump_region[REG_SETA][idx].start = 0x9FB8;
		itf_hwip->hw_ip->dump_region[REG_SETA][idx++].end = 0xFFFF;

		info_itfc("[ID:%2d] 3AA VA(0x%lx)\n", hw_id, (ulong)itf_hwip->hw_ip->regs[REG_SETA]);
		break;
	case DEV_HW_3AA1:
		mem_res = platform_get_resource(pdev, IORESOURCE_MEM, IORESOURCE_3AA1);
		if (!mem_res) {
			dev_err(&pdev->dev, "Failed to get io memory region\n");
			return -EINVAL;
		}
		itf_hwip->hw_ip->regs_start[REG_SETA] = mem_res->start + LIC_CHAIN_OFFSET;
		itf_hwip->hw_ip->regs_end[REG_SETA] = mem_res->end;
		itf_hwip->hw_ip->regs[REG_SETA] = ioremap_nocache(mem_res->start, resource_size(mem_res));
		if (!itf_hwip->hw_ip->regs[REG_SETA]) {
			dev_err(&pdev->dev, "Failed to remap io region\n");
			return -EINVAL;
		}
		itf_hwip->hw_ip->regs[REG_SETA] += LIC_CHAIN_OFFSET;

		idx = 0;
		itf_hwip->hw_ip->dump_region[REG_SETA][idx].start = 0x0;
		itf_hwip->hw_ip->dump_region[REG_SETA][idx++].end = 0x1FB3;
		itf_hwip->hw_ip->dump_region[REG_SETA][idx].start = 0x1FB8;
		itf_hwip->hw_ip->dump_region[REG_SETA][idx++].end = 0x9FB3;
		itf_hwip->hw_ip->dump_region[REG_SETA][idx].start = 0x9FB8;
		itf_hwip->hw_ip->dump_region[REG_SETA][idx++].end = 0xFFFF;

		info_itfc("[ID:%2d] 3AA VA(0x%lx)\n", hw_id, (ulong)itf_hwip->hw_ip->regs[REG_SETA]);
		break;
	case DEV_HW_ISP0:
		mem_res = platform_get_resource(pdev, IORESOURCE_MEM, IORESOURCE_ITP0);
		if (!mem_res) {
			dev_err(&pdev->dev, "Failed to get io memory region\n");
			return -EINVAL;
		}

		itf_hwip->hw_ip->regs_start[REG_SETA] = mem_res->start;
		itf_hwip->hw_ip->regs_end[REG_SETA] = mem_res->end;
		itf_hwip->hw_ip->regs[REG_SETA] = ioremap_nocache(mem_res->start, resource_size(mem_res));
		if (!itf_hwip->hw_ip->regs[REG_SETA]) {
			dev_err(&pdev->dev, "Failed to remap io region\n");
			return -EINVAL;
		}

		info_itfc("[ID:%2d] ITP0 VA(0x%lx)\n", hw_id, (ulong)itf_hwip->hw_ip->regs[REG_SETA]);
		break;
	case DEV_HW_MCSC0:
		mem_res = platform_get_resource(pdev, IORESOURCE_MEM, IORESOURCE_MCSC);
		if (!mem_res) {
			dev_err(&pdev->dev, "Failed to get io memory region\n");
			return -EINVAL;
		}

		itf_hwip->hw_ip->regs_start[REG_SETA] = mem_res->start;
		itf_hwip->hw_ip->regs_end[REG_SETA] = mem_res->end;
		itf_hwip->hw_ip->regs[REG_SETA] = ioremap_nocache(mem_res->start, resource_size(mem_res));
		if (!itf_hwip->hw_ip->regs[REG_SETA]) {
			dev_err(&pdev->dev, "Failed to remap io region\n");
			return -EINVAL;
		}

		info_itfc("[ID:%2d] MCSC0 VA(0x%lx)\n", hw_id, (ulong)itf_hwip->hw_ip->regs[REG_SETA]);
		break;
	case DEV_HW_VRA:
		mem_res = platform_get_resource(pdev, IORESOURCE_MEM, IORESOURCE_VRA0);
		if (!mem_res) {
			dev_err(&pdev->dev, "Failed to get io memory region\n");
			return -EINVAL;
		}

		itf_hwip->hw_ip->regs_start[REG_SETA] = mem_res->start;
		itf_hwip->hw_ip->regs_end[REG_SETA] = mem_res->end;
		itf_hwip->hw_ip->regs[REG_SETA] = ioremap_nocache(mem_res->start, resource_size(mem_res));
		if (!itf_hwip->hw_ip->regs[REG_SETA]) {
			dev_err(&pdev->dev, "Failed to remap io region\n");
			return -EINVAL;
		}

		info_itfc("[ID:%2d] VRA0 VA(0x%lx)\n", hw_id, (ulong)itf_hwip->hw_ip->regs[REG_SETA]);
		break;
	case DEV_HW_MCSC1:
		mem_res = platform_get_resource(pdev, IORESOURCE_MEM, IORESOURCE_MCSC_MFC);
		if (!mem_res) {
			dev_err(&pdev->dev, "Failed to get io memory region\n");
			return -EINVAL;
		}

		itf_hwip->hw_ip->regs_start[REG_SETA] = mem_res->start;
		itf_hwip->hw_ip->regs_end[REG_SETA] = mem_res->end;
		itf_hwip->hw_ip->regs[REG_SETA] = ioremap_nocache(mem_res->start, resource_size(mem_res));
		if (!itf_hwip->hw_ip->regs[REG_SETA]) {
			dev_err(&pdev->dev, "Failed to remap io region\n");
			return -EINVAL;
		}

		info_itfc("[ID:%2d] MCSC1 VA(0x%lx)\n", hw_id, (ulong)itf_hwip->hw_ip->regs[REG_SETA]);
		break;
	default:
		probe_err("hw_id(%d) is invalid", hw_id);
		return -EINVAL;
	}

	ret = is_hw_get_clk_gate(itf_hwip->hw_ip, hw_id);
	if (ret)
		dev_err(&pdev->dev, "is_hw_get_clk_gate is fail\n");

	return ret;
}

int is_hw_get_irq(void *itfc_data, void *pdev_data, int hw_id)
{
	struct is_interface_hwip *itf_hwip = NULL;
	struct platform_device *pdev = NULL;
	int ret = 0;

	FIMC_BUG(!itfc_data);

	itf_hwip = (struct is_interface_hwip *)itfc_data;
	pdev = (struct platform_device *)pdev_data;

	switch (hw_id) {
	case DEV_HW_3AA0:
		itf_hwip->irq[INTR_HWIP1] = platform_get_irq(pdev, 0);
		if (itf_hwip->irq[INTR_HWIP1] < 0) {
			err("Failed to get irq 3aa0-1\n");
			return -EINVAL;
		}

		itf_hwip->irq[INTR_HWIP2] = platform_get_irq(pdev, 1);
		if (itf_hwip->irq[INTR_HWIP2] < 0) {
			err("Failed to get irq 3aa0-2\n");
			return -EINVAL;
		}
		break;
	case DEV_HW_3AA1:
		itf_hwip->irq[INTR_HWIP1] = platform_get_irq(pdev, 2);
		if (itf_hwip->irq[INTR_HWIP1] < 0) {
			err("Failed to get irq 3aa1-1\n");
			return -EINVAL;
		}

		itf_hwip->irq[INTR_HWIP2] = platform_get_irq(pdev, 3);
		if (itf_hwip->irq[INTR_HWIP2] < 0) {
			err("Failed to get irq 3aa1-2\n");
			return -EINVAL;
		}
		break;
	case DEV_HW_ISP0:
		itf_hwip->irq[INTR_HWIP1] = platform_get_irq(pdev, 4);
		if (itf_hwip->irq[INTR_HWIP1] < 0) {
			err("Failed to get irq isp0-1\n");
			return -EINVAL;
		}

		itf_hwip->irq[INTR_HWIP2] = platform_get_irq(pdev, 5);
		if (itf_hwip->irq[INTR_HWIP2] < 0) {
			err("Failed to get irq isp0-2\n");
			return -EINVAL;
		}
		break;
	case DEV_HW_MCSC0:
		itf_hwip->irq[INTR_HWIP1] = platform_get_irq(pdev, 6);
		if (itf_hwip->irq[INTR_HWIP1] < 0) {
			err("Failed to get irq mcsc0\n");
			return -EINVAL;
		}
		break;
	case DEV_HW_VRA:
		itf_hwip->irq[INTR_HWIP1] = platform_get_irq(pdev, 7);
		if (itf_hwip->irq[INTR_HWIP1] < 0) {
			err("Failed to get irq clh0\n");
			return -EINVAL;
		}
		break;
	case DEV_HW_MCSC1:
		itf_hwip->irq[INTR_HWIP1] = platform_get_irq(pdev, 8);
		if (itf_hwip->irq[INTR_HWIP1] < 0) {
			err("Failed to get irq mcsc1\n");
			return -EINVAL;
		}
		break;
	default:
		probe_err("hw_id(%d) is invalid", hw_id);
		return -EINVAL;
	}

	return ret;
}

static inline int __is_hw_request_irq(struct is_interface_hwip *itf_hwip,
	const char *name, int isr_num,
	unsigned int added_irq_flags,
	irqreturn_t (*func)(int, void *))
{
	size_t name_len = 0;
	int ret = 0;

	name_len = sizeof(itf_hwip->irq_name[isr_num]);
	snprintf(itf_hwip->irq_name[isr_num], name_len, "%s-%d", name, isr_num);
	ret = request_irq(itf_hwip->irq[isr_num], func,
		IS_HW_IRQ_FLAG | added_irq_flags,
		itf_hwip->irq_name[isr_num],
		itf_hwip);
	if (ret) {
		err_itfc("[HW:%s] request_irq [%d] fail", name, isr_num);
		return -EINVAL;
	}
	itf_hwip->handler[isr_num].id = isr_num;
	itf_hwip->handler[isr_num].valid = true;

	return ret;
}

int is_hw_request_irq(void *itfc_data, int hw_id)
{
	struct is_interface_hwip *itf_hwip = NULL;
	int ret = 0;

	FIMC_BUG(!itfc_data);


	itf_hwip = (struct is_interface_hwip *)itfc_data;

	switch (hw_id) {
	case DEV_HW_3AA0:
		ret = __is_hw_request_irq(itf_hwip, "3a0-0", INTR_HWIP1, IRQF_TRIGGER_NONE, __is_isr1_3aa0);
		ret = __is_hw_request_irq(itf_hwip, "3a0-1", INTR_HWIP2, IRQF_TRIGGER_NONE, __is_isr2_3aa0);
		break;
	case DEV_HW_3AA1:
		ret = __is_hw_request_irq(itf_hwip, "3a1-0", INTR_HWIP1, IRQF_TRIGGER_NONE, __is_isr1_3aa1);
		ret = __is_hw_request_irq(itf_hwip, "3a1-1", INTR_HWIP2, IRQF_TRIGGER_NONE, __is_isr2_3aa1);
		break;
	case DEV_HW_ISP0:
		ret = __is_hw_request_irq(itf_hwip, "itp0-0", INTR_HWIP1, IRQF_TRIGGER_NONE, __is_isr1_itp0);
		ret = __is_hw_request_irq(itf_hwip, "itp0-1", INTR_HWIP2, IRQF_TRIGGER_NONE, __is_isr2_itp0);
		break;
	case DEV_HW_MCSC0:
		ret = __is_hw_request_irq(itf_hwip, "mcs0", INTR_HWIP1, IRQF_TRIGGER_NONE, __is_isr1_mcs0);
		break;
	case DEV_HW_VRA:
		ret = __is_hw_request_irq(itf_hwip, "VRA0", INTR_HWIP1, IRQF_TRIGGER_NONE, __is_isr1_vra);
		break;
	case DEV_HW_MCSC1:
		ret = __is_hw_request_irq(itf_hwip, "mcs1", INTR_HWIP1, IRQF_TRIGGER_NONE, __is_isr1_mcs1);
		break;
	default:
		probe_err("hw_id(%d) is invalid", hw_id);
		return -EINVAL;
	}

	return ret;
}

int is_hw_s_ctrl(void *itfc_data, int hw_id, enum hw_s_ctrl_id id, void *val)
{
	int ret = 0;

	switch (id) {
	case HW_S_CTRL_FULL_BYPASS:
		break;
	case HW_S_CTRL_CHAIN_IRQ:
		break;
	case HW_S_CTRL_HWFC_IDX_RESET:
		if (hw_id == IS_VIDEO_M0P_NUM) {
			struct is_video_ctx *vctx = (struct is_video_ctx *)itfc_data;
			struct is_device_ischain *device;
			unsigned long data = (unsigned long)val;

			FIMC_BUG(!vctx);
			FIMC_BUG(!GET_DEVICE(vctx));

			device = GET_DEVICE(vctx);

			/* reset if this instance is reprocessing */
			if (test_bit(IS_ISCHAIN_REPROCESSING, &device->state))
				writel(data, hwfc_rst);
		}
		break;
	case HW_S_CTRL_MCSC_SET_INPUT:
		{
			unsigned long mode = (unsigned long)val;

			info_itfc("%s: mode(%lu)\n", __func__, mode);
		}
		break;
	default:
		break;
	}

	return ret;
}

int is_hw_g_ctrl(void *itfc_data, int hw_id, enum hw_g_ctrl_id id, void *val)
{
	int ret = 0;

	switch (id) {
	case HW_G_CTRL_FRM_DONE_WITH_DMA:
		*(bool *)val = true;
		break;
	case HW_G_CTRL_HAS_MCSC:
		*(bool *)val = true;
		break;
	case HW_G_CTRL_HAS_VRA_CH1_ONLY:
		*(bool *)val = true;
		break;
	}

	return ret;
}

int is_hw_query_cap(void *cap_data, int hw_id)
{
	int ret = 0;

	FIMC_BUG(!cap_data);

	switch (hw_id) {
	case DEV_HW_MCSC0:
		{
			struct is_hw_mcsc_cap *cap = (struct is_hw_mcsc_cap *)cap_data;

			cap->hw_ver = HW_SET_VERSION(8, 0x20, 0, 0);
			cap->max_output = 1;
			cap->max_djag = 0;
			cap->max_cac = 0;
			cap->max_uvsp = 0;
			cap->hwfc = MCSC_CAP_NOT_SUPPORT;
			cap->in_otf = MCSC_CAP_SUPPORT;
			cap->in_dma = MCSC_CAP_NOT_SUPPORT;
			cap->out_dma[0] = MCSC_CAP_SUPPORT;
			cap->out_dma[1] = MCSC_CAP_NOT_SUPPORT;
			cap->out_dma[2] = MCSC_CAP_NOT_SUPPORT;
			cap->out_dma[3] = MCSC_CAP_NOT_SUPPORT;
			cap->out_dma[4] = MCSC_CAP_NOT_SUPPORT;
			cap->out_dma[5] = MCSC_CAP_NOT_SUPPORT;
			cap->out_otf[0] = MCSC_CAP_NOT_SUPPORT;
			cap->out_otf[1] = MCSC_CAP_NOT_SUPPORT;
			cap->out_otf[2] = MCSC_CAP_NOT_SUPPORT;
			cap->out_otf[3] = MCSC_CAP_NOT_SUPPORT;
			cap->out_otf[4] = MCSC_CAP_NOT_SUPPORT;
			cap->out_otf[5] = MCSC_CAP_NOT_SUPPORT;
			cap->out_hwfc[0] = MCSC_CAP_NOT_SUPPORT;
			cap->out_hwfc[1] = MCSC_CAP_NOT_SUPPORT;
			cap->out_hwfc[2] = MCSC_CAP_NOT_SUPPORT;
			cap->out_hwfc[3] = MCSC_CAP_NOT_SUPPORT;
			cap->out_hwfc[4] = MCSC_CAP_NOT_SUPPORT;
			cap->out_hwfc[5] = MCSC_CAP_NOT_SUPPORT;
			cap->out_post[0] = MCSC_CAP_SUPPORT;
			cap->out_post[1] = MCSC_CAP_NOT_SUPPORT;
			cap->out_post[2] = MCSC_CAP_NOT_SUPPORT;
			cap->out_post[3] = MCSC_CAP_NOT_SUPPORT;
			cap->out_post[4] = MCSC_CAP_NOT_SUPPORT;
			cap->out_post[5] = MCSC_CAP_NOT_SUPPORT;
			cap->enable_shared_output = false;
			cap->tdnr = MCSC_CAP_NOT_SUPPORT;
			cap->djag = MCSC_CAP_NOT_SUPPORT;
			cap->cac = MCSC_CAP_NOT_SUPPORT;
			cap->uvsp = MCSC_CAP_NOT_SUPPORT;
			cap->ysum = MCSC_CAP_NOT_SUPPORT;
			cap->ds_vra = MCSC_CAP_NOT_SUPPORT;
		}
		break;
	case DEV_HW_MCSC1:
		{
			struct is_hw_mcsc_cap *cap = (struct is_hw_mcsc_cap *)cap_data;

			cap->hw_ver = HW_SET_VERSION(8, 0x21, 0, 0);
			cap->max_output = 1;
			cap->max_djag = 0;
			cap->max_cac = 0;
			cap->max_uvsp = 0;
			cap->hwfc = MCSC_CAP_NOT_SUPPORT;
			cap->in_otf = MCSC_CAP_NOT_SUPPORT;
			cap->in_dma = MCSC_CAP_SUPPORT;
			cap->out_dma[0] = MCSC_CAP_SUPPORT;
			cap->out_dma[1] = MCSC_CAP_NOT_SUPPORT;
			cap->out_dma[2] = MCSC_CAP_NOT_SUPPORT;
			cap->out_dma[3] = MCSC_CAP_NOT_SUPPORT;
			cap->out_dma[4] = MCSC_CAP_NOT_SUPPORT;
			cap->out_dma[5] = MCSC_CAP_NOT_SUPPORT;
			cap->out_otf[0] = MCSC_CAP_NOT_SUPPORT;
			cap->out_otf[1] = MCSC_CAP_NOT_SUPPORT;
			cap->out_otf[2] = MCSC_CAP_NOT_SUPPORT;
			cap->out_otf[3] = MCSC_CAP_NOT_SUPPORT;
			cap->out_otf[4] = MCSC_CAP_NOT_SUPPORT;
			cap->out_otf[5] = MCSC_CAP_NOT_SUPPORT;
			cap->out_hwfc[0] = MCSC_CAP_NOT_SUPPORT;
			cap->out_hwfc[1] = MCSC_CAP_NOT_SUPPORT;
			cap->out_hwfc[2] = MCSC_CAP_NOT_SUPPORT;
			cap->out_hwfc[3] = MCSC_CAP_NOT_SUPPORT;
			cap->out_hwfc[4] = MCSC_CAP_NOT_SUPPORT;
			cap->out_hwfc[5] = MCSC_CAP_NOT_SUPPORT;
			cap->out_post[0] = MCSC_CAP_SUPPORT;
			cap->out_post[1] = MCSC_CAP_NOT_SUPPORT;
			cap->out_post[2] = MCSC_CAP_NOT_SUPPORT;
			cap->out_post[3] = MCSC_CAP_NOT_SUPPORT;
			cap->out_post[4] = MCSC_CAP_NOT_SUPPORT;
			cap->out_post[5] = MCSC_CAP_NOT_SUPPORT;
			cap->enable_shared_output = false;
			cap->tdnr = MCSC_CAP_NOT_SUPPORT;
			cap->djag = MCSC_CAP_NOT_SUPPORT;
			cap->cac = MCSC_CAP_NOT_SUPPORT;
			cap->uvsp = MCSC_CAP_NOT_SUPPORT;
			cap->ysum = MCSC_CAP_NOT_SUPPORT;
			cap->ds_vra = MCSC_CAP_NOT_SUPPORT;
		}
		break;
	default:
		break;
	}

	return ret;
}

void __iomem *is_hw_get_sysreg(ulong core_regs)
{
	if (core_regs)
		err_itfc("%s: core_regs(%p)\n", __func__, (void *)core_regs);

	/* deprecated */

	return NULL;
}

u32 is_hw_find_settle(u32 mipi_speed, u32 use_cphy)
{
	u32 align_mipi_speed;
	u32 find_mipi_speed;
	const u32 *settle_table;
	size_t max;
	int s, e, m;

	if (use_cphy) {
		settle_table = is_csi_settle_table_cphy;
		max = sizeof(is_csi_settle_table_cphy) / sizeof(u32);
	} else {
		settle_table = is_csi_settle_table;
		max = sizeof(is_csi_settle_table) / sizeof(u32);
	}
	align_mipi_speed = ALIGN(mipi_speed, 10);

	s = 0;
	e = max - 2;

	if (settle_table[s] < align_mipi_speed)
		return settle_table[s + 1];

	if (settle_table[e] > align_mipi_speed)
		return settle_table[e + 1];

	/* Binary search */
	while (s <= e) {
		m = ALIGN((s + e) / 2, 2);
		find_mipi_speed = settle_table[m];

		if (find_mipi_speed == align_mipi_speed)
			break;
		else if (find_mipi_speed > align_mipi_speed)
			s = m + 2;
		else
			e = m - 2;
	}

	return settle_table[m + 1];
}

unsigned int get_dma(struct is_device_sensor *device, u32 *dma_ch)
{
	*dma_ch = 0;

	return 0;
}

void is_hw_camif_init(void)
{
	/* TODO */
}

#ifdef USE_CAMIF_FIX_UP
int is_hw_camif_fix_up(struct is_device_sensor *sensor)
{
	int ret = 0;

	/* TODO */

	return ret;
}
#endif

int is_hw_camif_cfg(void *sensor_data)
{
	int ret = 0;
	void __iomem *is_sys_regs;
	struct is_device_sensor *sensor;
	struct is_device_csi *csi;
	struct exynos_platform_is_sensor *pdata;
	struct is_core *core;
	struct is_device_ischain *ischain;
	u32 sel_ipp0_input = CSI_ID_A;
	u32 sel_ipp1_input = CSI_ID_B;
	int i = 0, sensor_open_cnt = 0;

	FIMC_BUG(!sensor_data);

	sensor = (struct is_device_sensor *)sensor_data;

	csi = (struct is_device_csi *)v4l2_get_subdevdata(sensor->subdev_csi);
	if (!csi) {
		merr("csi is null\n", sensor);
		ret = -ENODEV;
		return ret;
	}

	pdata = sensor->pdata;
	if (!pdata) {
		merr("pdata is null\n", sensor);
		ret = -ENODEV;
		return ret;
	}

	core = sensor->private_data;
	if (!core) {
		merr("core is null\n", sensor);
		ret = -ENODEV;
		return ret;
	}

	ischain = sensor->ischain;
	if (!ischain) {
		merr("ischain is null\n", sensor);
		ret = -ENODEV;
		return ret;
	}

	/* IPP input path set by DT */
	if (pdata->csi_mux) {
		sel_ipp0_input = pdata->csi_mux & 0x000F;
		sel_ipp1_input = (pdata->csi_mux & 0x00F0) >> 4;
	} else {
		merr("csi-ipp mux dt value is null\n", sensor);
	}

	for (i = 0; i < IS_SENSOR_COUNT; i++) {
		if (test_bit(IS_SENSOR_OPEN, &(core->sensor[i].state)))
			sensor_open_cnt++;
	}
	if (sensor_open_cnt == 1) {
		if (ischain->group_3aa.id == GROUP_ID_3AA0)
			sel_ipp0_input = csi->ch;
		else if (ischain->group_3aa.id == GROUP_ID_3AA1)
			sel_ipp1_input = csi->ch;
		info("USE path CSIS(%d) - IPP(%d)\n", csi->ch, ischain->group_3aa.id);
	}

#if defined(SECURE_CAMERA_FACE)
	mutex_lock(&core->secure_state_lock);
	if (core->secure_state == IS_STATE_UNSECURE
			&& core->scenario == IS_SCENARIO_SECURE
			&& sensor_open_cnt != 1) {
		err("sensor open cnt: (%d != 1)", sensor_open_cnt);
		mutex_unlock(&core->secure_state_lock);
		return -EINVAL;
	}
	mutex_unlock(&core->secure_state_lock);
#endif

	is_sys_regs = ioremap_nocache(SYSREG_IS_BASE_ADDR, 0x1000);

	mutex_lock(&ischain->resourcemgr->sysreg_lock);

	switch (csi->ch) {
	case CSI_ID_A:
		is_hw_set_field(is_sys_regs,
			&sysreg_is_regs[SYSREG_R_IS_USER_GLUE_CON4],
			&sysreg_is_fields[SYSREG_F_MASK_FOR_CSIS0_OTF], 0x1);
		break;
	case CSI_ID_B:
		is_hw_set_field(is_sys_regs,
			&sysreg_is_regs[SYSREG_R_IS_USER_GLUE_CON6],
			&sysreg_is_fields[SYSREG_F_MASK_FOR_CSIS1_OTF], 0x1);
		break;
	case CSI_ID_C:
		is_hw_set_field(is_sys_regs,
			&sysreg_is_regs[SYSREG_R_IS_USER_GLUE_CON8],
			&sysreg_is_fields[SYSREG_F_MASK_FOR_CSIS2_OTF], 0x1);
		break;
	default:
		break;
	}

	is_hw_set_field(is_sys_regs,
		&sysreg_is_regs[SYSREG_R_IS_USER_GLUE_CON0],
		&sysreg_is_fields[SYSREG_F_MUX_FOR_OTF0], sel_ipp0_input);
	is_hw_set_field(is_sys_regs,
		&sysreg_is_regs[SYSREG_R_IS_USER_GLUE_CON2],
		&sysreg_is_fields[SYSREG_F_MUX_FOR_OTF1], sel_ipp1_input);

#if defined(SECURE_CAMERA_FACE) && defined(PROTECT_SYSREG_IS)
	if (sensor->ex_scenario == IS_SCENARIO_SECURE) {
		struct is_sensor_cfg *sensor_cfg;
		u32 settle;

		sensor_cfg = csi->sensor_cfg;
		if (!sensor_cfg) {
			merr("[CSI%d] sensor cfg is null", csi, csi->ch);
			ret = -EINVAL;
			goto p_err;
		}

		if (sensor_cfg->mipi_speed) {
			settle = is_hw_find_settle(sensor_cfg->mipi_speed, csi->use_cphy);
		} else {
			merr("[CSI%d] mipi_speed is invalid", csi, csi->ch);
			ret = -EINVAL;
			goto p_err;
		}

		ret = csi_hw_s_phy_set(csi->phy, sensor_cfg->lanes, sensor_cfg->mipi_speed, settle, csi->ch,
				csi->use_cphy);
	}
p_err:
#endif
	mutex_unlock(&ischain->resourcemgr->sysreg_lock);

	info("SYSREG_GLUE_CON: 0[%x], 2[%x], 4[%x], 6[%x], 8[%x]\n",
		is_hw_get_reg(is_sys_regs, &sysreg_is_regs[SYSREG_R_IS_USER_GLUE_CON0]),
		is_hw_get_reg(is_sys_regs, &sysreg_is_regs[SYSREG_R_IS_USER_GLUE_CON2]),
		is_hw_get_reg(is_sys_regs, &sysreg_is_regs[SYSREG_R_IS_USER_GLUE_CON4]),
		is_hw_get_reg(is_sys_regs, &sysreg_is_regs[SYSREG_R_IS_USER_GLUE_CON6]),
		is_hw_get_reg(is_sys_regs, &sysreg_is_regs[SYSREG_R_IS_USER_GLUE_CON8]));

	iounmap(is_sys_regs);
	return ret;
}

int is_hw_camif_open(void *sensor_data)
{
	struct is_device_sensor *sensor;
	struct is_device_csi *csi;

	FIMC_BUG(!sensor_data);

	sensor = sensor_data;
	csi = (struct is_device_csi *)v4l2_get_subdevdata(sensor->subdev_csi);

	if (csi->ch >= CSI_ID_MAX) {
		merr("CSI channel is invalid(%d)\n", sensor, csi->ch);
		return -EINVAL;
	}

	set_bit(CSIS_DMA_ENABLE, &csi->state);

#ifdef SOC_SSVC0
	csi->dma_subdev[CSI_VIRTUAL_CH_0] = &sensor->ssvc0;
#else
	csi->dma_subdev[CSI_VIRTUAL_CH_0] = NULL;
#endif
#ifdef SOC_SSVC1
	csi->dma_subdev[CSI_VIRTUAL_CH_1] = &sensor->ssvc1;
#else
	csi->dma_subdev[CSI_VIRTUAL_CH_1] = NULL;
#endif
#ifdef SOC_SSVC2
	csi->dma_subdev[CSI_VIRTUAL_CH_2] = &sensor->ssvc2;
#else
	csi->dma_subdev[CSI_VIRTUAL_CH_2] = NULL;
#endif
#ifdef SOC_SSVC3
	csi->dma_subdev[CSI_VIRTUAL_CH_3] = &sensor->ssvc3;
#else
	csi->dma_subdev[CSI_VIRTUAL_CH_3] = NULL;
#endif

	return 0;
}

void is_hw_ischain_qe_cfg(void)
{
	dbg_hw(2, "%s()\n", __func__);
}

int is_hw_ischain_cfg(void *ischain_data)
{
	int ret = 0;

	/* If necessary, it will be implemented later. */
	return ret;
}

int is_hw_ischain_enable(struct is_device_ischain *device)
{
	int ret = 0;

	FIMC_BUG(!device);

	if (test_bit(IS_ISCHAIN_REPROCESSING, &device->state))
		return ret;

	/* If necessary, it will be implemented later. */
	return ret;
}

int is_hw_ischain_disable(struct is_device_ischain *device)
{
	int ret = 0;

	FIMC_BUG(!device);

	if (test_bit(IS_ISCHAIN_REPROCESSING, &device->state))
		return ret;

	/* If necessary, it will be implemented later. */
	return ret;
}

/* TODO: remove this, compile check only */
#ifdef ENABLE_HWACG_CONTROL
void is_hw_csi_qchannel_enable_all(bool enable)
{
	void __iomem *csi0_regs;
	void __iomem *csi1_regs;
	void __iomem *csi2_regs;

	u32 reg_val;

	csi0_regs = ioremap_nocache(CSIS0_QCH_EN_ADDR, SZ_4);
	csi1_regs = ioremap_nocache(CSIS1_QCH_EN_ADDR, SZ_4);
	csi2_regs = ioremap_nocache(CSIS2_QCH_EN_ADDR, SZ_4);

	reg_val = readl(csi0_regs);
	reg_val &= ~(1 << 20);
	writel(enable << 20 | reg_val, csi0_regs);

	reg_val = readl(csi1_regs);
	reg_val &= ~(1 << 20);
	writel(enable << 20 | reg_val, csi1_regs);

	reg_val = readl(csi2_regs);
	reg_val &= ~(1 << 20);
	writel(enable << 20 | reg_val, csi2_regs);

	iounmap(csi0_regs);
	iounmap(csi1_regs);
	iounmap(csi2_regs);
}
#endif

void is_hw_djag_adjust_out_size(struct is_device_ischain *ischain,
					u32 in_width, u32 in_height,
					u32 *out_width, u32 *out_height)
{
	/* NOT TO SUPPORT */
}

void is_hw_interrupt_relay(struct is_group *group, void *hw_ip_data)
{
	struct is_group *child;
	struct is_hw_ip *hw_ip = (struct is_hw_ip *)hw_ip_data;

	child = group->child;
	if (child) {
		int hw_list[GROUP_HW_MAX], hw_slot;
		enum is_hardware_id hw_id;

		is_get_hw_list(child->id, hw_list);
		hw_id = hw_list[0];
		hw_slot = is_hw_slot_id(hw_id);
		if (!valid_hw_slot_id(hw_slot)) {
			serr_hw("invalid slot (%d,%d)", hw_ip, hw_id, hw_slot);
		} else {
			struct is_hardware *hardware;
			struct is_hw_ip *hw_ip_child;
			struct hwip_intr_handler *intr_handler;

			hardware = hw_ip->hardware;
			if (!hardware) {
				serr_hw("hardware is NILL", hw_ip);
			} else {
				hw_ip_child = &hardware->hw_ip[hw_slot];
				intr_handler = hw_ip_child->intr_handler[INTR_HWIP3];
				if (intr_handler && intr_handler->handler) {
					is_enter_lib_isr();
					intr_handler->handler(intr_handler->id, intr_handler->ctx);
					is_exit_lib_isr();
				}
			}
		}
	}
}

int is_hw_check_gframe_skip(void *group_data)
{
	int ret = 0;
	struct is_group *group;

	FIMC_BUG(!group_data);

	group = (struct is_group *)group_data;

	switch (group->id) {
	case GROUP_ID_VRA0:
		ret = true;
		break;
	default:
		ret = false;
		break;
	}

	return ret;
}

is_hw_configure_llc(bool on, struct is_device_ischain *device, ulong *llc_state)
{
	dbg("not supported");
}

void is_hw_configure_bts_scen(struct is_resourcemgr *resourcemgr, int scenario_id)
{
	dbg("not supported");
}
