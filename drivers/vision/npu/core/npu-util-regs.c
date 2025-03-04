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

#include <linux/platform_device.h>
#include <linux/delay.h>
#include <soc/samsung/exynos-smc.h>

#include "npu-log.h"
#include "npu-device.h"
#include "npu-system.h"
#include "npu-util-regs.h"

struct reg_cmd_list;

char *npu_cmd_type[] = {
	"read",
	"write",
	"smc_read",
	"smc_write"
};

u32 npu_read_hw_reg(const struct npu_iomem_area *base, u32 offset, u32 mask)
{
	volatile u32 v;
	void __iomem *reg_addr;

	BUG_ON(!base);
	BUG_ON(!base->vaddr);

	if (offset > base->size) {
		npu_err("offset(%u) exceeds iomem region size(%llu), starting at (%u)\n",
				offset, base->size, base->paddr);
		BUG_ON(1);
	}
	reg_addr = base->vaddr + offset;
	v = readl(reg_addr);
	npu_dbg("setting register pa(0x%08x) va(%p) cur(0x%08x(raw 0x%08x)) mask(0x%08x)\n",
		base->paddr + offset, reg_addr,	(v & mask), v, mask);

	return (v & mask);
}

void npu_write_hw_reg(const struct npu_iomem_area *base, u32 offset, u32 val, u32 mask)
{
	volatile u32 v;
	void __iomem *reg_addr;

	BUG_ON(!base);
	BUG_ON(!base->vaddr);

	if (offset > base->size) {
		npu_err("offset(%u) exceeds iomem region size(%llu), starting at (%u)\n",
				offset, base->size, base->paddr);
		BUG_ON(1);
	}
	reg_addr = base->vaddr + offset;
	v = readl(reg_addr);
	npu_dbg("setting register pa(0x%08x) va(%p) cur(0x%08x) val(0x%08x) mask(0x%08x)\n",
		base->paddr + offset, reg_addr,	v, val, mask);

	v = (v & (~mask)) | (val & mask);
	writel(v, (void *)(reg_addr));
	npu_dbg("written (0x%08x) at (%p)\n", v, reg_addr);
}

unsigned long npu_smc_read_hw_reg(const struct npu_iomem_area *base,
						u32 offset, u32 val, u32 mask)
{
	unsigned long v;
	u32 reg_addr;

	if (!base || !base->paddr) {
		npu_err("base or base->paddr is NULL\n");
		return 0;
	}

	if (offset > base->size) {
		npu_err("offset(%u) > size(%llu), starting at (%u)\n",
					offset, base->size, base->paddr);
		return 0;
	}
	reg_addr = base->paddr + offset;
	npu_dbg("calling smc pa(0x%08x) va(%p) val(0x%08x) mask(0x%08x)\n",
		base->paddr + offset, reg_addr,	val, mask);

	if (!exynos_smc_readsfr((unsigned long)reg_addr, &v))
		return (v & mask);

	return 0;
}

void npu_smc_write_hw_reg(const struct npu_iomem_area *base,
						u32 offset, u32 val, u32 mask)
{
	volatile u32 v;
	u32 reg_addr;

	BUG_ON(!base);
	BUG_ON(!base->paddr);

	if (offset > base->size) {
		npu_err("offset(%u) exceeds iomem region size(%llu), starting at (%u)\n",
				offset, base->size, base->paddr);
		BUG_ON(1);
	}
	reg_addr = base->paddr + offset;
	npu_dbg("calling smc pa(0x%08x) va(%p) val(0x%08x) mask(0x%08x)\n",
		base->paddr + offset, reg_addr,	val, mask);

	v = val & mask;
	exynos_smc(SMC_CMD_REG, SMC_REG_ID_SFR_W((int)reg_addr), v, 0x0);

	npu_dbg("smc (0x%08x) at (%p)\n", v, reg_addr);
}

/*
 * Set the set of register value specified in set_map,
 * with the base specified at base.
 * if regset_mdelay != 0, it will be the delay between register set.
 */
int npu_set_hw_reg(const struct npu_iomem_area *base, const struct reg_set_map *set_map,
		      size_t map_len, int regset_mdelay)
{
	size_t i;

	BUG_ON(!base);
	BUG_ON(!set_map);
	BUG_ON(!base->vaddr);

	for (i = 0; i < map_len; ++i) {
		npu_write_hw_reg(base, set_map[i].offset, set_map[i].val, set_map[i].mask);
		/* Insert delay between register setting */
		if (regset_mdelay > 0)
			mdelay(regset_mdelay);
	}
	return 0;
}

int npu_set_hw_reg_2(const struct reg_set_map_2 *set_map, size_t map_len, int regset_mdelay)
{
	size_t i;

	BUG_ON(!set_map);

	for (i = 0; i < map_len; ++i) {
		npu_write_hw_reg(set_map[i].iomem_area, set_map[i].offset, set_map[i].val, set_map[i].mask);

		/* Insert delay between register setting */
		if (regset_mdelay > 0)
			mdelay(regset_mdelay);
	}
	return 0;
}

/* dt_name has prefix "samsung,npucmd-" */
#define NPU_REG_CMD_PREFIX	"samsung,npucmd-"
int npu_get_reg_cmd_map(struct npu_system *system, struct reg_cmd_list *cmd_list)
{
	int i, ret = 0;
	u32 *cmd_data = NULL;
	char sfr_name[32], data_name[32];
	char **cmd_sfr;
	struct device *dev;

	sfr_name[0] = '\0';
	data_name[0] = '\0';
	strcpy(sfr_name, NPU_REG_CMD_PREFIX);
	strcpy(data_name, NPU_REG_CMD_PREFIX);
	strcat(strcat(sfr_name, cmd_list->name), "-sfr");
	strcat(strcat(data_name, cmd_list->name), "-data");

	dev = &(system->pdev->dev);
	cmd_list->count = of_property_count_strings(
			dev->of_node, sfr_name);
	if (cmd_list->count <= 0) {
		probe_err("invalid reg_cmd list by %s\n", sfr_name);
		cmd_list->list = NULL;
		cmd_list->count = 0;
		ret = -ENODEV;
		goto err_exit;
	}
	probe_info("%s register %d commands\n", sfr_name, cmd_list->count);

	cmd_list->list = (struct reg_cmd_map *)devm_kmalloc(dev,
				(cmd_list->count + 1) * sizeof(struct reg_cmd_map),
				GFP_KERNEL);
	if (!cmd_list->list) {
		probe_err("failed to alloc for cmd map\n");
		ret = -ENOMEM;
		goto err_exit;
	}
	(cmd_list->list)[cmd_list->count].name = NULL;

	cmd_sfr = (char **)devm_kmalloc(dev,
			cmd_list->count * sizeof(char *), GFP_KERNEL);
	if (!cmd_sfr) {
		probe_err("failed to alloc for reg_cmd sfr for %s\n", sfr_name);
		ret = -ENOMEM;
		goto err_exit;
	}
	ret = of_property_read_string_array(dev->of_node, sfr_name, (const char **)cmd_sfr, cmd_list->count);
	if (ret < 0) {
		probe_err("failed to get reg_cmd for %s (%d)\n", sfr_name, ret);
		ret = -EINVAL;
		goto err_sfr;
	}

	cmd_data = (u32 *)devm_kmalloc(dev,
			cmd_list->count * NPU_REG_CMD_LEN * sizeof(u32), GFP_KERNEL);
	if (!cmd_data) {
		probe_err("failed to alloc for reg_cmd data for %s\n", data_name);
		ret = -ENOMEM;
		goto err_sfr;
	}
	ret = of_property_read_u32_array(dev->of_node, data_name, (u32 *)cmd_data,
			cmd_list->count * NPU_REG_CMD_LEN);
	if (ret) {
		probe_err("failed to get reg_cmd for %s (%d)\n", data_name, ret);
		ret = -EINVAL;
		goto err_data;
	}

	for (i = 0; i < cmd_list->count; i++) {
		(*(cmd_list->list + i)).name = *(cmd_sfr + i);
		memcpy((void *)(&(*(cmd_list->list + i)).cmd),
				(void *)(cmd_data + (i * NPU_REG_CMD_LEN)),
				sizeof(u32) * NPU_REG_CMD_LEN);
		probe_info("copy %s cmd (%lu)\n", *(cmd_sfr + i), sizeof(u32) * NPU_REG_CMD_LEN);
	}
err_data:
	devm_kfree(dev, cmd_data);
err_sfr:
	devm_kfree(dev, cmd_sfr);
err_exit:
	return ret;
}

int npu_cmd_sfr(struct npu_system *system, const struct reg_cmd_list *cmd_list)
{
	int ret = 0;
	size_t i;
	struct reg_cmd_map *t;
	struct npu_iomem_area *sfrmem;

	BUG_ON(!system);

	if (!cmd_list) {
		npu_dbg("No cmd for sfr\n");
		return 0;
	}

	for (i = 0; i < cmd_list->count; ++i) {
		t = (cmd_list->list + i);
		sfrmem = npu_get_io_area(system, t->name);
		if (t->name) {
			switch (t->cmd) {
			case NPU_CMD_SFR_READ:
				ret = (int)npu_read_hw_reg(sfrmem,
							t->offset, t->mask);
				npu_info("%s+0x%08x : 0x%08x\n",
						t->name, t->offset, ret);
				break;
			case NPU_CMD_SFR_WRITE:
				npu_write_hw_reg(sfrmem, t->offset, t->val, t->mask);
				break;
			case NPU_CMD_SFR_SMC_READ:
				ret = (int)npu_smc_read_hw_reg(sfrmem,
						t->offset, t->val, t->mask);
				npu_info("%s+0x%08x : 0x%08x\n",
						t->name, t->offset, ret);
				break;
			case NPU_CMD_SFR_SMC_WRITE:
				npu_smc_write_hw_reg(sfrmem,
						t->offset, t->val, t->mask);
				break;
			default:
				break;
			}
		} else
				break;
		/* Insert delay between register setting */
		if (t->mdelay) {
			npu_dbg("%s %d mdelay\n", t->name, t->mdelay);
			mdelay(t->mdelay);
		}
	}
	return ret;
}

int npu_cmd_map(struct npu_system *system, const char *cmd_name)
{
	BUG_ON(!system);
	BUG_ON(!cmd_name);

	return npu_cmd_sfr(system, (const struct reg_cmd_list *)get_npu_cmd_map(system, cmd_name));
}

int npu_set_sfr(const u32 sfr_addr, const u32 value, const u32 mask)
{
	int			ret;
	void __iomem		*iomem = NULL;
	struct npu_iomem_area	area_info;	/* Save iomem result */

	iomem = ioremap_nocache(sfr_addr, sizeof(u32));
	if (IS_ERR_OR_NULL(iomem)) {
		probe_err("fail(%p) in ioremap_nocache(0x%08x)\n",
			  iomem, sfr_addr);
		ret = -EFAULT;
		goto err_exit;
	}
	area_info.vaddr = iomem;
	area_info.paddr = sfr_addr;
	area_info.size = sizeof(u32);

	npu_write_hw_reg(&area_info, 0, value, mask);

	ret = 0;
err_exit:
	if (iomem)
		iounmap(iomem);

	return ret;
}

int npu_get_sfr(const u32 sfr_addr)
{
	int			ret;
	void __iomem		*iomem = NULL;
	struct npu_iomem_area	area_info;	/* Save iomem result */
	volatile u32 v;
	void __iomem *reg_addr;

	iomem = ioremap_nocache(sfr_addr, sizeof(u32));
	if (IS_ERR_OR_NULL(iomem)) {
		probe_err("fail(%p) in ioremap_nocache(0x%08x)\n",
			  iomem, sfr_addr);
		ret = -EFAULT;
		goto err_exit;
	}
	area_info.vaddr = iomem;
	area_info.paddr = sfr_addr;
	area_info.size = sizeof(u32);

	reg_addr = area_info.vaddr;

	v = readl(reg_addr);
	npu_trace("get_sfr, vaddr(0x%p), paddr(0x%08x), val(0x%x)\n",
		area_info.vaddr, area_info.paddr, v);

	ret = 0;
err_exit:
	if (iomem)
		iounmap(iomem);

	return ret;
}
