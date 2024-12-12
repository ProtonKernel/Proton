/*
 * Copyright (c) 2018 Samsung Electronics Co., Ltd.
 *		http://www.samsung.com
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

/* Trace format
 * Traces are dumped in binary formats, encoded in multiple doublewords (64-bit)
 * dw 0 : magic number {'B', 'D', 'U', 'D', 'U', 'M', 'P', '\0')
 * dw 1 : { block number, port number } (each 32-bit integers
 * dw 2 : base physical address of BDU dump region
 * dw 3 : size (in bytes) of BDU dump region
 * dw 4 : tidemark if timestampes are used (0xffffffff if no timestamp)
 * dw 5~15 : zero - reserved for future use
 */

#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/of_platform.h>
#include <linux/slab.h>
#include <linux/syscalls.h>
#include <linux/vmalloc.h>
#include <linux/sched/clock.h>
#include <linux/of_reserved_mem.h>
#include <soc/samsung/debug-snapshot.h>

#include <linux/dma-mapping.h>

//#include <asm/map.h>

#include <soc/samsung/acpm_ipc_ctrl.h>
#include <soc/samsung/exynos-sci_dbg.h>

static bool exynos_sci_llc_debug_mode = false;
static struct exynos_ppc_dump_addr ppc_reserved;
static struct exynos_sci_dbg_data *sci_dbg_data;

#define HRTIMER_DURATION	1
static u32 buffer_cnt = 0;

static u32 DebugSrc10 = 0x08060807;
static u32 DebugSrc32 = 0x000E0023;
static u32 PPC_DEBUG_SCI_EVENT_SEL = 0x3F2B140D;

static u32 smc_dbg_blk_ctl_value[NUM_OF_SMC_DBG_BLK_CTL] = {
	0x30000000,
	0x30000000,
	0x30000000,
	0x30000000,
	0x30000000,
	0x10000000,
	0x21000007,
	0x30000000,
	0x30000000,
};
static u32 smc_global_dbg_ctl_value = 0x50000006;
static u32 sysreg_mif_ppc_debug_value[NUM_OF_SYSREG_MIF] = {
	0x3A821,
	0x3A821,
	0x3A821,
	0x3A821,
};

static u32 llc_ctrl;
static u32 llc_id[6];
static u32 llc_addr_match;
static u32 llc_addr_mask;

void carveout_reserved_mem(void *r)
{
	struct reserved_mem *rmem = r;
	struct page *first = phys_to_page(rmem->base & PAGE_MASK);
	struct page *last = phys_to_page(PAGE_ALIGN(rmem->base + rmem->size));
	struct page *page;

	for (page = first; page != last; page++)
		free_reserved_page(page);
}

void set_exynos_sci_llc_debug_mode(int enable)
{
	exynos_sci_llc_debug_mode = enable;
}

bool get_exynos_sci_llc_debug_mode(void)
{
	return exynos_sci_llc_debug_mode;
}

#if IS_ENABLED(CONFIG_USE_PPC_LOG)
static void sci_dump_data_save(void)
{
	u32 dump_entry_size = sizeof(struct exynos_sci_dbg_dump_data);
	struct exynos_sci_dbg_dump_data *dump_data = NULL;
	u32 remain_buff_size;
	int i;

	dump_data = (struct exynos_sci_dbg_dump_data *)(sci_dbg_data->dump_addr.v_addr + buffer_cnt);
	dump_data->index = sci_dbg_data->dump_data.index;
	dump_data->time = sci_dbg_data->dump_data.time;
	for (i = 0; i < 5; i++)
		dump_data->count[i] = sci_dbg_data->dump_data.count[i];

	buffer_cnt += dump_entry_size;
	remain_buff_size = sci_dbg_data->dump_addr.buff_size - buffer_cnt;

	if (remain_buff_size < dump_entry_size)
		buffer_cnt = 0;
}

static enum hrtimer_restart count_monitor(struct hrtimer *hrtimer)
{
	enum hrtimer_restart ret = HRTIMER_RESTART;

	sci_dbg_data->dump_data.index++;

	/* PMNC stop  */
	__raw_writel(0x0, sci_dbg_data->dump_addr.cnt_sfr_base + SCI_PPC_PMNC);
	/* Read overflow flag */
	__raw_readl(sci_dbg_data->dump_addr.cnt_sfr_base + SCI_PPC_FLAG);

	sci_dbg_data->dump_data.time = cpu_clock(raw_smp_processor_id());

	/* read counter */
	sci_dbg_data->dump_data.count[0] =
			__raw_readl(sci_dbg_data->dump_addr.cnt_sfr_base + SCI_PPC_PMCNT0_LOW);
	sci_dbg_data->dump_data.count[1] =
			__raw_readl(sci_dbg_data->dump_addr.cnt_sfr_base + SCI_PPC_PMCNT1_LOW);
	sci_dbg_data->dump_data.count[2] =
		    __raw_readl(sci_dbg_data->dump_addr.cnt_sfr_base + SCI_PPC_PMCNT2_LOW);
	sci_dbg_data->dump_data.count[3] =
		    __raw_readl(sci_dbg_data->dump_addr.cnt_sfr_base + SCI_PPC_PMCNT3_LOW);
	sci_dbg_data->dump_data.count[4] =
			__raw_readl(sci_dbg_data->dump_addr.cnt_sfr_base + SCI_PPC_CCNT_LOW);


	__raw_writel(0x6, sci_dbg_data->dump_addr.cnt_sfr_base + SCI_PPC_PMNC);
	__raw_writel(0x1, sci_dbg_data->dump_addr.cnt_sfr_base + SCI_PPC_PMNC);

	sci_dump_data_save();

	hrtimer_forward_now(hrtimer, ms_to_ktime(HRTIMER_DURATION));

	return ret;
}
#endif

static int exynos_sci_dump_enable(struct exynos_sci_dbg_data *data,
					unsigned int enable)
{
	if (enable) {
		data->dump_data.index = 0;
		buffer_cnt = 0;
		memset(data->dump_addr.v_addr, 0, data->dump_addr.buff_size);

		/* PPC_DEBUG_SCI_EVENT_SEL */
		__raw_writel(PPC_DEBUG_SCI_EVENT_SEL, data->dump_addr.trex_core_base + 0x100);
		/*  */
		__raw_writel(DebugSrc10, data->dump_addr.sci_base + DebugSrc10_offset);	// DebugSrc10
		__raw_writel(DebugSrc32, data->dump_addr.sci_base + DebugSrc32_offset);	// DebugSrc32
		__raw_writel(0x00000001, data->dump_addr.sci_base + DebugCtrl_offset);	// DebugCtrl

		/* CNTENS */
		__raw_writel(0x8000000F, data->dump_addr.cnt_sfr_base + SCI_PPC_CNTENS);
		/* INTENS */
		__raw_writel(0x8000000F, data->dump_addr.cnt_sfr_base + SCI_PPC_INTENS);
		/* FLAG */
		__raw_writel(0x8000000F, data->dump_addr.cnt_sfr_base + SCI_PPC_FLAG);
		/* PMNC reset */
		__raw_writel(0x6, data->dump_addr.cnt_sfr_base + SCI_PPC_PMNC);
		/* PMNC start */
		__raw_writel(0x1, data->dump_addr.cnt_sfr_base + SCI_PPC_PMNC);
		hrtimer_start(&data->hrtimer,
				ms_to_ktime(HRTIMER_DURATION), HRTIMER_MODE_REL);
	} else {
		hrtimer_try_to_cancel(&data->hrtimer);
		__raw_writel(0x0, data->dump_addr.cnt_sfr_base + SCI_PPC_PMNC);
	}

	return 0;
}

#if IS_ENABLED(CONFIG_USE_PPC_LOG)
static void smc_dump_data_save(void)
{
	u32 dump_entry_size = sizeof(struct exynos_smc_dump_data) * 4;
	struct exynos_smc_dump_data *dump_data = NULL;
	u32 remain_buff_size;
	int i, j;

	for (i = 0; i < 4; i++) {
		dump_data = (struct exynos_smc_dump_data *)(sci_dbg_data->dump_addr.v_addr + buffer_cnt);
		dump_data->index = sci_dbg_data->smc_dump_data[i].index;
		dump_data->smc_ch = sci_dbg_data->smc_dump_data[i].smc_ch;
		dump_data->time = sci_dbg_data->smc_dump_data[i].time;
		for (j = 0; j < 6; j++)
			dump_data->count[j] = sci_dbg_data->smc_dump_data[i].count[j];

		buffer_cnt += (dump_entry_size / 4);
	}

	remain_buff_size = sci_dbg_data->dump_addr.buff_size - buffer_cnt;

	if (remain_buff_size < dump_entry_size)
		buffer_cnt = 0;
}

static enum hrtimer_restart smc_count_monitor(struct hrtimer *hrtimer)
{
	enum hrtimer_restart ret = HRTIMER_RESTART;
	int i;

	for (i = 0; i < 4; i++) {
		sci_dbg_data->smc_dump_data[i].index++;
		sci_dbg_data->smc_dump_data[i].smc_ch = i;
		__raw_writel(0x0, sci_dbg_data->dump_addr.ppc_dbg_base[i] + SMC_PPC_PMNC);
	}


	for (i = 0; i < 4; i++) {
		sci_dbg_data->smc_dump_data[i].time = cpu_clock(raw_smp_processor_id());
		sci_dbg_data->smc_dump_data[i].count[0] =
			__raw_readl(sci_dbg_data->dump_addr.ppc_dbg_base[i] + SMC_PPC_CCNT);
		sci_dbg_data->smc_dump_data[i].count[1] =
			__raw_readl(sci_dbg_data->dump_addr.ppc_dbg_base[i] + SMC_PPC_PMCNT0);
		sci_dbg_data->smc_dump_data[i].count[2] =
			__raw_readl(sci_dbg_data->dump_addr.ppc_dbg_base[i] + SMC_PPC_PMCNT1);
		sci_dbg_data->smc_dump_data[i].count[3] =
			__raw_readl(sci_dbg_data->dump_addr.ppc_dbg_base[i] + SMC_PPC_PMCNT2);
		sci_dbg_data->smc_dump_data[i].count[4] =
			__raw_readl(sci_dbg_data->dump_addr.ppc_dbg_base[i] + SMC_PPC_PMCNT3_HIGH);
		sci_dbg_data->smc_dump_data[i].count[5] =
			__raw_readl(sci_dbg_data->dump_addr.ppc_dbg_base[i] + SMC_PPC_PMCNT3_LOW);

		/* PPC_DEBUG: reset CCNT */
		__raw_writel(0x0, sci_dbg_data->dump_addr.ppc_dbg_base[i] + SMC_PPC_CCNT);
		/* PPC_DEBUG: PPC_Counter Reset  */
		__raw_writel(0x2, sci_dbg_data->dump_addr.ppc_dbg_base[i] + SMC_PPC_PMNC);
		__raw_writel(0x1, sci_dbg_data->dump_addr.ppc_dbg_base[i] + SMC_PPC_PMNC);
	}

	smc_dump_data_save();

	hrtimer_forward_now(hrtimer, ms_to_ktime(HRTIMER_DURATION));

	return ret;
}
#endif
static int exynos_smc_dump_enable(struct exynos_sci_dbg_data *data,
					unsigned int enable)
{
	int i;

	if (enable) {
		for (i = 0; i < 4; i++)
			data->smc_dump_data[i].index = 0;
		buffer_cnt = 0;
		memset(data->dump_addr.v_addr, 0, data->dump_addr.buff_size);

		/* Disable all debug block */
		__raw_writel(0x30000000, data->dump_addr.smc_base + SMC_DBG_BLK_CTL0);
		__raw_writel(0x30000000, data->dump_addr.smc_base + SMC_DBG_BLK_CTL1);
		__raw_writel(0x30000000, data->dump_addr.smc_base + SMC_DBG_BLK_CTL2);
		__raw_writel(0x30000000, data->dump_addr.smc_base + SMC_DBG_BLK_CTL3);
		__raw_writel(0x30000000, data->dump_addr.smc_base + SMC_DBG_BLK_CTL4);
		__raw_writel(0x30000000, data->dump_addr.smc_base + SMC_DBG_BLK_CTL5);
		__raw_writel(0x30000000, data->dump_addr.smc_base + SMC_DBG_BLK_CTL6);
		__raw_writel(0x30000000, data->dump_addr.smc_base + SMC_DBG_BLK_CTL7);
		__raw_writel(0x30000000, data->dump_addr.smc_base + SMC_DBG_BLK_CTL8);
		/* GlobalDbgCtl, Enable, Performance mode */
		__raw_writel(smc_global_dbg_ctl_value, data->dump_addr.smc_base + SMC_GLOBAL_DBG_CTL);

		__raw_writel(smc_dbg_blk_ctl_value[0], data->dump_addr.smc_base + SMC_DBG_BLK_CTL0);
		__raw_writel(smc_dbg_blk_ctl_value[1], data->dump_addr.smc_base + SMC_DBG_BLK_CTL1);
		__raw_writel(smc_dbg_blk_ctl_value[2], data->dump_addr.smc_base + SMC_DBG_BLK_CTL2);
		__raw_writel(smc_dbg_blk_ctl_value[3], data->dump_addr.smc_base + SMC_DBG_BLK_CTL3);
		__raw_writel(smc_dbg_blk_ctl_value[4], data->dump_addr.smc_base + SMC_DBG_BLK_CTL4);
		__raw_writel(smc_dbg_blk_ctl_value[5], data->dump_addr.smc_base + SMC_DBG_BLK_CTL5);
		__raw_writel(smc_dbg_blk_ctl_value[6], data->dump_addr.smc_base + SMC_DBG_BLK_CTL6);
		__raw_writel(smc_dbg_blk_ctl_value[7], data->dump_addr.smc_base + SMC_DBG_BLK_CTL7);
		__raw_writel(smc_dbg_blk_ctl_value[8], data->dump_addr.smc_base + SMC_DBG_BLK_CTL8);

		__raw_writel(sysreg_mif_ppc_debug_value[0], data->dump_addr.sysreg_mif_base[0] + 0x1000);
		__raw_writel(sysreg_mif_ppc_debug_value[1], data->dump_addr.sysreg_mif_base[1] + 0x1000);
		__raw_writel(sysreg_mif_ppc_debug_value[2], data->dump_addr.sysreg_mif_base[2] + 0x1000);
		__raw_writel(sysreg_mif_ppc_debug_value[3], data->dump_addr.sysreg_mif_base[3] + 0x1000);

		__raw_writel(0x10000000, data->dump_addr.smc_mif_base[0] + 0x2A8);
		__raw_writel(0x10000000, data->dump_addr.smc_mif_base[1] + 0x2A8);
		__raw_writel(0x10000000, data->dump_addr.smc_mif_base[2] + 0x2A8);
		__raw_writel(0x10000000, data->dump_addr.smc_mif_base[3] + 0x2A8);

		for (i = 0; i < 4; i++) {
			/* PPC_DEBUG: reset CCNT */
			__raw_writel(0x0, data->dump_addr.ppc_dbg_base[i] + SMC_PPC_CCNT);
			/* PPC_DEBUG: PPC_Counter Reset  */
			__raw_writel(0x2, data->dump_addr.ppc_dbg_base[i] + SMC_PPC_PMNC);
			/* CCNT, PMCNT0~4 Enable */
			__raw_writel(0x8000000F, data->dump_addr.ppc_dbg_base[i] + SMC_PPC_CNTENS);
			/* PPC enable */
			__raw_writel(0x1, data->dump_addr.ppc_dbg_base[i] + SMC_PPC_PMNC);
		}
		hrtimer_start(&data->smc_hrtimer,
				ms_to_ktime(HRTIMER_DURATION), HRTIMER_MODE_REL);
	} else {
		hrtimer_try_to_cancel(&data->smc_hrtimer);
		for (i = 0; i < 4; i++)
			__raw_writel(0x0, data->dump_addr.ppc_dbg_base[i] + SMC_PPC_PMNC);
	}

	return 0;
}

static ssize_t show_sci_dump_enable(struct device *dev,
				struct device_attribute *attr, char *buf)
{
	struct platform_device *pdev = container_of(dev,
					struct platform_device, dev);
	struct exynos_sci_dbg_data *data = platform_get_drvdata(pdev);
	ssize_t count = 0;
	unsigned int enable = data->dump_enable;

	count += snprintf(buf + count, PAGE_SIZE, "SCI dump enable: %s\n",
			enable ? "enable" : "disable");

	return count;
}

void smc_ppc_enable(unsigned int enable)
{
	int ret;

	ret = exynos_smc_dump_enable(sci_dbg_data, enable);
	sci_dbg_data->smc_dump_enable = enable;

	return;
}
EXPORT_SYMBOL(smc_ppc_enable);

void sci_ppc_enable(unsigned int enable)
{
	int ret;

	ret = exynos_sci_dump_enable(sci_dbg_data, enable);
	sci_dbg_data->dump_enable = enable;

	return;
}
EXPORT_SYMBOL(sci_ppc_enable);

static ssize_t store_sci_dump_enable(struct device *dev,
				struct device_attribute *attr,
				const char *buf, size_t count)
{
	struct platform_device *pdev = container_of(dev,
					struct platform_device, dev);
	struct exynos_sci_dbg_data *data = platform_get_drvdata(pdev);
	unsigned int enable;
	int ret;

	ret = sscanf(buf, "%u",	&enable);
	if (ret != 1)
		return -EINVAL;

	ret = exynos_sci_dump_enable(data, enable);
	if (ret) {
		SCI_DBG_ERR("%s: Failed sci dump enable control\n", __func__);
		return ret;
	}

	data->dump_enable = enable;

	return count;
}

static ssize_t show_debug_mode(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	ssize_t count = 0;

	count += snprintf(buf + count, PAGE_SIZE, "exynos_sci_llc_debug_mode: %s\n",
			exynos_sci_llc_debug_mode? "Enabled":"Disabled");

	return count;
}

static ssize_t store_debug_mode(struct device *dev,
		struct device_attribute *attr,
		const char *buf, size_t count)
{
	int ret;

	ret = sscanf(buf, "%d",	&exynos_sci_llc_debug_mode);
	if (ret != 1)
		return -EINVAL;

	return count;
}

static ssize_t show_DebugSrc10(struct device *dev,
				struct device_attribute *attr, char *buf)
{
	ssize_t count = 0;

	count += snprintf(buf + count, PAGE_SIZE, "DebugSrc10: 0x%08X\n", DebugSrc10);

	return count;
}

static ssize_t store_DebugSrc10(struct device *dev,
				struct device_attribute *attr,
				const char *buf, size_t count)
{
	int ret;

	ret = sscanf(buf, "%x",	&DebugSrc10);
	if (ret != 1)
		return -EINVAL;

	return count;
}

static ssize_t show_DebugSrc32(struct device *dev,
				struct device_attribute *attr, char *buf)
{
	ssize_t count = 0;

	count += snprintf(buf + count, PAGE_SIZE, "DebugSrc32: 0x%08X\n", DebugSrc32);

	return count;
}

static ssize_t store_DebugSrc32(struct device *dev,
				struct device_attribute *attr,
				const char *buf, size_t count)
{
	int ret;

	ret = sscanf(buf, "%x",	&DebugSrc32);
	if (ret != 1)
		return -EINVAL;

	return count;
}

static ssize_t show_SCI_EVENT_SEL(struct device *dev,
				struct device_attribute *attr, char *buf)
{
	ssize_t count = 0;

	count += snprintf(buf + count, PAGE_SIZE, "PPC_DEBUG_SCI_EVENT_SEL: 0x%08X\n", PPC_DEBUG_SCI_EVENT_SEL);

	return count;
}

static ssize_t store_SCI_EVENT_SEL(struct device *dev,
				struct device_attribute *attr,
				const char *buf, size_t count)
{
	int ret;

	ret = sscanf(buf, "%x",	&PPC_DEBUG_SCI_EVENT_SEL);
	if (ret != 1)
		return -EINVAL;

	return count;
}

static ssize_t show_smc_dbg_blk_ctl(struct device *dev,
				struct device_attribute *attr, char *buf)
{
	unsigned int i;
	ssize_t count = 0;

	for (i = 0; i < NUM_OF_SMC_DBG_BLK_CTL; i++) {
		count += snprintf(buf + count, PAGE_SIZE, "SMC_DBG_BLK_CTL%u: 0x%08X\n",
				i, smc_dbg_blk_ctl_value[i]);
	}

	return count;
}

static ssize_t store_smc_dbg_blk_ctl(struct device *dev,
				struct device_attribute *attr,
				const char *buf, size_t count)
{
	int ret;
	u32 index, value;

	ret = sscanf(buf, "%u %x", &index, &value);
	if (ret != 2)
		return -EINVAL;

	if (index >= NUM_OF_SMC_DBG_BLK_CTL)
		return -EINVAL;

	smc_dbg_blk_ctl_value[index] = value;

	return count;
}

static ssize_t show_smc_global_dbg_ctl(struct device *dev,
				struct device_attribute *attr, char *buf)
{
	ssize_t count = 0;

	count += snprintf(buf + count, PAGE_SIZE, "GlobalDbgCtl: 0x%08X\n",
			smc_global_dbg_ctl_value);

	return count;
}

static ssize_t store_smc_global_dbg_ctl(struct device *dev,
				struct device_attribute *attr,
				const char *buf, size_t count)
{
	int ret;

	ret = sscanf(buf, "%x", &smc_global_dbg_ctl_value);
	if (ret != 1)
		return -EINVAL;

	return count;
}

static ssize_t show_sysreg_mif_ppc_debug(struct device *dev,
				struct device_attribute *attr, char *buf)
{
	unsigned int i;
	ssize_t count = 0;

	for (i = 0; i < NUM_OF_SYSREG_MIF; i++) {
		count += snprintf(buf + count, PAGE_SIZE, "MIF_PPC_DEBUG%u: 0x%08X\n",
				i, sysreg_mif_ppc_debug_value[i]);
	}

	return count;
}

static ssize_t store_sysreg_mif_ppc_debug(struct device *dev,
				struct device_attribute *attr,
				const char *buf, size_t count)
{
	int ret;
	u32 index, value;

	ret = sscanf(buf, "%u %x", &index, &value);
	if (ret != 2)
		return -EINVAL;

	if (index >= NUM_OF_SYSREG_MIF)
		return -EINVAL;

	sysreg_mif_ppc_debug_value[index] = value;

	return count;
}

static ssize_t show_smc_dump_enable(struct device *dev,
				struct device_attribute *attr, char *buf)
{
	struct platform_device *pdev = container_of(dev,
					struct platform_device, dev);
	struct exynos_sci_dbg_data *data = platform_get_drvdata(pdev);
	ssize_t count = 0;
	unsigned int enable = data->smc_dump_enable;

	count += snprintf(buf + count, PAGE_SIZE, "SMC dump enable: %s\n",
			enable ? "enable" : "disable");

	return count;
}

static ssize_t store_smc_dump_enable(struct device *dev,
				struct device_attribute *attr,
				const char *buf, size_t count)
{
	struct platform_device *pdev = container_of(dev,
					struct platform_device, dev);
	struct exynos_sci_dbg_data *data = platform_get_drvdata(pdev);
	unsigned int enable;
	int ret;

	ret = sscanf(buf, "%u",	&enable);
	if (ret != 1)
		return -EINVAL;

	ret = exynos_smc_dump_enable(data, enable);
	if (ret) {
		SCI_DBG_ERR("%s: Failed sci dump enable control\n", __func__);
		return ret;
	}

	data->smc_dump_enable = enable;

	return count;
}

static ssize_t show_llc_ctrl(struct device *dev,
				struct device_attribute *attr, char *buf)
{
	struct platform_device *pdev = container_of(dev,
					struct platform_device, dev);
	struct exynos_sci_dbg_data *data = platform_get_drvdata(pdev);
	ssize_t count = 0;

	llc_ctrl = __raw_readl(data->sci_base + LLCControl);	// LLCControl

	count += snprintf(buf + count, PAGE_SIZE, "LLCControl: 0x%08X\n", llc_ctrl);

	return count;
}

static ssize_t store_llc_ctrl(struct device *dev,
				struct device_attribute *attr,
				const char *buf, size_t count)
{
	struct platform_device *pdev = container_of(dev,
					struct platform_device, dev);
	struct exynos_sci_dbg_data *data = platform_get_drvdata(pdev);
	int ret;

	ret = sscanf(buf, "%x",	&llc_ctrl);
	if (ret != 1)
		return -EINVAL;

	__raw_writel(llc_ctrl, data->sci_base + LLCControl);	// LLCControl

	return count;
}

static ssize_t show_llc_id(struct device *dev,
				struct device_attribute *attr, char *buf)
{
	struct platform_device *pdev = container_of(dev,
					struct platform_device, dev);
	struct exynos_sci_dbg_data *data = platform_get_drvdata(pdev);
	ssize_t count = 0;
	int i;
	u32 llc_alloc;

	for (i = 0; i < 6; i++) {
		llc_id[i] = __raw_readl(data->sci_base + LLCId_0 + (i * 0x8));	// LLCId_x
		llc_alloc = __raw_readl(data->sci_base + LLCIdAllocLkup_0 + (i * 0x8));
		count += snprintf(buf + count, PAGE_SIZE, "LLCId_%d: 0x%08X / ", i, llc_id[i]);
		count += snprintf(buf + count, PAGE_SIZE, "LLCIdAllocLkup_%d: 0x%08X\n", i, llc_alloc);
	}

	return count;
}

static ssize_t store_llc_id(struct device *dev,
				struct device_attribute *attr,
				const char *buf, size_t count)
{
	struct platform_device *pdev = container_of(dev,
					struct platform_device, dev);
	struct exynos_sci_dbg_data *data = platform_get_drvdata(pdev);
	int ret;
	u32 id, llc_val, llc_alloc;

	ret = sscanf(buf, "%d %x %x", &id, &llc_val, &llc_alloc);
	if (ret != 3)
		return -EINVAL;

	if (id >= 6)
		return -EINVAL;

	llc_id[id] = llc_val;

	__raw_writel(llc_id[id], data->sci_base + LLCId_0 + (id * 0x8));// LLCId_x
	__raw_writel(llc_alloc, data->sci_base + LLCIdAllocLkup_0 + (id * 0x8));	// LLCIdAllocLkup_x

	return count;
}

static ssize_t show_llc_addr_match(struct device *dev,
				struct device_attribute *attr, char *buf)
{
	struct platform_device *pdev = container_of(dev,
					struct platform_device, dev);
	struct exynos_sci_dbg_data *data = platform_get_drvdata(pdev);
	ssize_t count = 0;

	llc_addr_match = __raw_readl(data->sci_base + LLCAddrMatch);	// LLCAddrMatch

	count += snprintf(buf + count, PAGE_SIZE, "LLCAddrMatch: 0x%08X\n", llc_addr_match);

	return count;
}

static ssize_t store_llc_addr_match(struct device *dev,
				struct device_attribute *attr,
				const char *buf, size_t count)
{
	struct platform_device *pdev = container_of(dev,
					struct platform_device, dev);
	struct exynos_sci_dbg_data *data = platform_get_drvdata(pdev);
	int ret;

	ret = sscanf(buf, "%x",	&llc_addr_match);
	if (ret != 1)
		return -EINVAL;

	__raw_writel(llc_addr_match, data->sci_base + LLCAddrMatch);	// LLCAddrMatch

	return count;
}

static ssize_t show_llc_addr_mask(struct device *dev,
				struct device_attribute *attr, char *buf)
{
	struct platform_device *pdev = container_of(dev,
					struct platform_device, dev);
	struct exynos_sci_dbg_data *data = platform_get_drvdata(pdev);
	ssize_t count = 0;

	llc_addr_mask = __raw_readl(data->sci_base + LLCAddrMask);	// LLCAddrMask

	count += snprintf(buf + count, PAGE_SIZE, "LLCAddrMask: 0x%08X\n", llc_addr_mask);

	return count;
}

static ssize_t store_llc_addr_mask(struct device *dev,
				struct device_attribute *attr,
				const char *buf, size_t count)
{
	struct platform_device *pdev = container_of(dev,
					struct platform_device, dev);
	struct exynos_sci_dbg_data *data = platform_get_drvdata(pdev);
	int ret;

	ret = sscanf(buf, "%x",	&llc_addr_mask);
	if (ret != 1)
		return -EINVAL;

	__raw_writel(llc_addr_mask, data->sci_base + LLCAddrMask);	// LLCAddrMask

	return count;
}

static ssize_t show_irps_llc_user_config_default(struct device *dev,
				struct device_attribute *attr, char *buf)
{
	struct platform_device *pdev = container_of(dev,
					struct platform_device, dev);
	struct exynos_sci_dbg_data *data = platform_get_drvdata(pdev);
	ssize_t count = 0;
	u32 config_default;
	int i;

	for (i = 0; i < 4; i++) {
		config_default = __raw_readl(data->dump_addr.trex_irps_base[i] + 0x4);
		count += snprintf(buf + count, PAGE_SIZE,
			"IRPS%d_LLC_USER_CONFIG_DEFAULT: 0x%08X\n", i, config_default);
	}

	return count;
}

static ssize_t store_irps_llc_user_config_default(struct device *dev,
				struct device_attribute *attr,
				const char *buf, size_t count)
{
	struct platform_device *pdev = container_of(dev,
					struct platform_device, dev);
	struct exynos_sci_dbg_data *data = platform_get_drvdata(pdev);
	int ret;
	u32 config_default;
	int i;

	ret = sscanf(buf, "%x",	&config_default);
	if (ret != 1)
		return -EINVAL;

	for (i = 0; i < 4; i++)
		__raw_writel(config_default, data->dump_addr.trex_irps_base[i] + 0x4);

	return count;
}

static ssize_t show_irps_llc_user_config_match(struct device *dev,
				struct device_attribute *attr, char *buf)
{
	struct platform_device *pdev = container_of(dev,
					struct platform_device, dev);
	struct exynos_sci_dbg_data *data = platform_get_drvdata(pdev);
	ssize_t count = 0;
	int i, j;
	u32 config_match;

	for (i = 0; i < 4; i++) {
		for (j = 0; j < 16; j++) {
			config_match =
				__raw_readl(data->dump_addr.trex_irps_base[i]
						+ LLC_USER_CONFIG_MATCH + (j * 0x8));
			count += snprintf(buf + count, PAGE_SIZE,
				"IRPS%d_LLC_USER_CONFIG_MATCH_%d: 0x%08X\n", i, j, config_match);
		}
	}

	return count;
}

static ssize_t store_irps_llc_user_config_match(struct device *dev,
				struct device_attribute *attr,
				const char *buf, size_t count)
{
	struct platform_device *pdev = container_of(dev,
					struct platform_device, dev);
	struct exynos_sci_dbg_data *data = platform_get_drvdata(pdev);
	int ret;
	u32 id, val;
	int i;

	ret = sscanf(buf, "%d %x", &id, &val);
	if (ret != 2)
		return -EINVAL;

	if (id >= 16)
		return -EINVAL;

	for (i = 0; i < 4; i++)
		__raw_writel(val, data->dump_addr.trex_irps_base[i] +
				LLC_USER_CONFIG_MATCH + (id * 0x8));

	return count;
}

static ssize_t show_irps_llc_user_config_user(struct device *dev,
				struct device_attribute *attr, char *buf)
{
	struct platform_device *pdev = container_of(dev,
					struct platform_device, dev);
	struct exynos_sci_dbg_data *data = platform_get_drvdata(pdev);
	ssize_t count = 0;
	int i, j;
	u32 config_user;

	for (i = 0; i < 4; i++) {
		for (j = 0; j < 16; j++) {
			config_user =
				__raw_readl(data->dump_addr.trex_irps_base[i]
						+ LLC_USER_CONFIG_USER + (j * 0x8));
			count += snprintf(buf + count, PAGE_SIZE,
				"IRPS%d_LLC_USER_CONFIG_USER_%d: 0x%08X\n", i, j, config_user);
		}
	}

	return count;
}

static ssize_t store_irps_llc_user_config_user(struct device *dev,
				struct device_attribute *attr,
				const char *buf, size_t count)
{
	struct platform_device *pdev = container_of(dev,
					struct platform_device, dev);
	struct exynos_sci_dbg_data *data = platform_get_drvdata(pdev);
	int ret;
	u32 id, val;
	int i;

	ret = sscanf(buf, "%d %x", &id, &val);
	if (ret != 2)
		return -EINVAL;

	if (id >= 16)
		return -EINVAL;

	for (i = 0; i < 4; i++)
		__raw_writel(val, data->dump_addr.trex_irps_base[i] +
				LLC_USER_CONFIG_USER + (id * 0x8));

	return count;
}

static ssize_t show_cacheaid_user(struct device *dev,
				struct device_attribute *attr, char *buf)
{
	struct platform_device *pdev = container_of(dev,
					struct platform_device, dev);
	struct exynos_sci_dbg_data *data = platform_get_drvdata(pdev);
	ssize_t count = 0;
	int i;
	u32 cacheaid_user;

	for (i = 0; i < 32; i++) {
		cacheaid_user = __raw_readl(data->cacheaid_base
					+ CACHEAID_USER + (i * 0x10));
		count += snprintf(buf + count, PAGE_SIZE,
				"CACHEAID_USER_%d: 0x%08X\n", i, cacheaid_user);
	}

	return count;
}

static ssize_t store_cacheaid_user(struct device *dev,
				struct device_attribute *attr,
				const char *buf, size_t count)
{
	struct platform_device *pdev = container_of(dev,
					struct platform_device, dev);
	struct exynos_sci_dbg_data *data = platform_get_drvdata(pdev);
	int ret;
	u32 index, val;

	ret = sscanf(buf, "%d %x", &index, &val);
	if (ret != 2)
		return -EINVAL;

	if (index >= 32)
		return -EINVAL;

	__raw_writel(val, data->cacheaid_base +
			CACHEAID_USER + (index * 0x10));

	return count;
}

static ssize_t show_cacheaid_ctrl(struct device *dev,
				struct device_attribute *attr, char *buf)
{
	struct platform_device *pdev = container_of(dev,
					struct platform_device, dev);
	struct exynos_sci_dbg_data *data = platform_get_drvdata(pdev);
	ssize_t count = 0;
	int i;
	u32 cacheaid_ctrl;

	for (i = 0; i < 32; i++) {
		cacheaid_ctrl = __raw_readl(data->cacheaid_base
					+ CACHEAID_CTRL + (i * 0x10));
		count += snprintf(buf + count, PAGE_SIZE,
				"CACHEAID_CTRL_%d: 0x%08X\n", i, cacheaid_ctrl);
	}

	return count;
}

static ssize_t store_cacheaid_ctrl(struct device *dev,
				struct device_attribute *attr,
				const char *buf, size_t count)
{
	struct platform_device *pdev = container_of(dev,
					struct platform_device, dev);
	struct exynos_sci_dbg_data *data = platform_get_drvdata(pdev);
	int ret;
	u32 index, val;

	ret = sscanf(buf, "%d %x", &index, &val);
	if (ret != 2)
		return -EINVAL;

	if (index >= 32)
		return -EINVAL;

	__raw_writel(val, data->cacheaid_base +
			CACHEAID_CTRL + (index * 0x10));

	return count;
}

static ssize_t show_cacheaid_global_ctrl(struct device *dev,
				struct device_attribute *attr, char *buf)
{
	struct platform_device *pdev = container_of(dev,
					struct platform_device, dev);
	struct exynos_sci_dbg_data *data = platform_get_drvdata(pdev);
	ssize_t count = 0;
	u32 val;

	val = __raw_readl(data->cacheaid_base
			+ CACHEAID_GLOBAL_CTRL);
	count += snprintf(buf + count, PAGE_SIZE,
			"CACHEAID_GLOBAL_CTRL: 0x%08X\n", val);

	return count;
}

static ssize_t store_cacheaid_global_ctrl(struct device *dev,
				struct device_attribute *attr,
				const char *buf, size_t count)
{
	struct platform_device *pdev = container_of(dev,
					struct platform_device, dev);
	struct exynos_sci_dbg_data *data = platform_get_drvdata(pdev);
	int ret;
	u32 val;

	ret = sscanf(buf, "%x", &val);
	if (ret != 1)
		return -EINVAL;

	__raw_writel(val, data->cacheaid_base +
			CACHEAID_GLOBAL_CTRL);

	return count;
}

static ssize_t show_cacheaid_default_ctrl(struct device *dev,
				struct device_attribute *attr, char *buf)
{
	struct platform_device *pdev = container_of(dev,
					struct platform_device, dev);
	struct exynos_sci_dbg_data *data = platform_get_drvdata(pdev);
	ssize_t count = 0;
	u32 val;

	val = __raw_readl(data->cacheaid_base
			+ CACHEAID_DEFAULT_CTRL);
	count += snprintf(buf + count, PAGE_SIZE,
			"CACHEAID_DEFAULT_CTRL: 0x%08X\n", val);

	return count;
}

static ssize_t store_cacheaid_default_ctrl(struct device *dev,
				struct device_attribute *attr,
				const char *buf, size_t count)
{
	struct platform_device *pdev = container_of(dev,
					struct platform_device, dev);
	struct exynos_sci_dbg_data *data = platform_get_drvdata(pdev);
	int ret;
	u32 val;

	ret = sscanf(buf, "%x", &val);
	if (ret != 1)
		return -EINVAL;

	__raw_writel(val, data->cacheaid_base +
			CACHEAID_DEFAULT_CTRL);

	return count;
}

static ssize_t show_cacheaid_pmu_access_ctrl(struct device *dev,
				struct device_attribute *attr, char *buf)
{
	struct platform_device *pdev = container_of(dev,
					struct platform_device, dev);
	struct exynos_sci_dbg_data *data = platform_get_drvdata(pdev);
	ssize_t count = 0;
	u32 val;

	val = __raw_readl(data->cacheaid_base
			+ CACHEAID_PMU_ACCESS_CTRL);
	count += snprintf(buf + count, PAGE_SIZE,
			"CACHEAID_PMU_ACCESS_CTRL: 0x%08X\n", val);

	return count;
}

static ssize_t store_cacheaid_pmu_access_ctrl(struct device *dev,
				struct device_attribute *attr,
				const char *buf, size_t count)
{
	struct platform_device *pdev = container_of(dev,
					struct platform_device, dev);
	struct exynos_sci_dbg_data *data = platform_get_drvdata(pdev);
	int ret;
	u32 val;

	ret = sscanf(buf, "%x", &val);
	if (ret != 1)
		return -EINVAL;

	__raw_writel(val, data->cacheaid_base +
			CACHEAID_PMU_ACCESS_CTRL);

	return count;
}

static ssize_t show_cacheaid_pmu_access_info(struct device *dev,
				struct device_attribute *attr, char *buf)
{
	struct platform_device *pdev = container_of(dev,
					struct platform_device, dev);
	struct exynos_sci_dbg_data *data = platform_get_drvdata(pdev);
	ssize_t count = 0;
	u32 val;

	val = __raw_readl(data->cacheaid_base
			+ CACHEAID_PMU_ACCESS_INFO);
	count += snprintf(buf + count, PAGE_SIZE,
			"CACHEAID_PMU_ACCESS_INFO: 0x%08X\n", val);

	return count;
}

static ssize_t store_cacheaid_pmu_access_info(struct device *dev,
				struct device_attribute *attr,
				const char *buf, size_t count)
{
	struct platform_device *pdev = container_of(dev,
					struct platform_device, dev);
	struct exynos_sci_dbg_data *data = platform_get_drvdata(pdev);
	int ret;
	u32 val;

	ret = sscanf(buf, "%x", &val);
	if (ret != 1)
		return -EINVAL;

	__raw_writel(val, data->cacheaid_base +
			CACHEAID_PMU_ACCESS_INFO);

	return count;
}

static DEVICE_ATTR(debug_mode, 0640, show_debug_mode, store_debug_mode);
static DEVICE_ATTR(DebugSrc10, 0640, show_DebugSrc10, store_DebugSrc10);
static DEVICE_ATTR(DebugSrc32, 0640, show_DebugSrc32, store_DebugSrc32);
static DEVICE_ATTR(SCI_EVENT_SEL, 0640, show_SCI_EVENT_SEL, store_SCI_EVENT_SEL);
static DEVICE_ATTR(smc_dbg_blk_ctl, 0640, show_smc_dbg_blk_ctl, store_smc_dbg_blk_ctl);
static DEVICE_ATTR(smc_global_dbg_ctl, 0640, show_smc_global_dbg_ctl, store_smc_global_dbg_ctl);
static DEVICE_ATTR(sysreg_mif_ppc_debug, 0640, show_sysreg_mif_ppc_debug, store_sysreg_mif_ppc_debug);
static DEVICE_ATTR(smc_dump_enable, 0640, show_smc_dump_enable, store_smc_dump_enable);
static DEVICE_ATTR(sci_dump_enable, 0640, show_sci_dump_enable, store_sci_dump_enable);
static DEVICE_ATTR(llc_ctrl, 0640, show_llc_ctrl, store_llc_ctrl);
static DEVICE_ATTR(llc_id, 0640, show_llc_id, store_llc_id);
static DEVICE_ATTR(llc_addr_match, 0640, show_llc_addr_match, store_llc_addr_match);
static DEVICE_ATTR(llc_addr_mask, 0640, show_llc_addr_mask, store_llc_addr_mask);
static DEVICE_ATTR(irps_llc_user_config_default, 0640, show_irps_llc_user_config_default, store_irps_llc_user_config_default);
static DEVICE_ATTR(irps_llc_user_config_match, 0640, show_irps_llc_user_config_match, store_irps_llc_user_config_match);
static DEVICE_ATTR(irps_llc_user_config_user, 0640, show_irps_llc_user_config_user, store_irps_llc_user_config_user);
static DEVICE_ATTR(cacheaid_user, 0640, show_cacheaid_user, store_cacheaid_user);
static DEVICE_ATTR(cacheaid_ctrl, 0640, show_cacheaid_ctrl, store_cacheaid_ctrl);
static DEVICE_ATTR(cacheaid_global_ctrl, 0640, show_cacheaid_global_ctrl, store_cacheaid_global_ctrl);
static DEVICE_ATTR(cacheaid_default_ctrl, 0640, show_cacheaid_default_ctrl, store_cacheaid_default_ctrl);
static DEVICE_ATTR(cacheaid_pmu_access_ctrl, 0640, show_cacheaid_pmu_access_ctrl, store_cacheaid_pmu_access_ctrl);
static DEVICE_ATTR(cacheaid_pmu_access_info, 0640, show_cacheaid_pmu_access_info, store_cacheaid_pmu_access_info);

static struct attribute *exynos_sci_dbg_sysfs_entries[] = {
	&dev_attr_debug_mode.attr,
	&dev_attr_DebugSrc10.attr,
	&dev_attr_DebugSrc32.attr,
	&dev_attr_SCI_EVENT_SEL.attr,
	&dev_attr_smc_dbg_blk_ctl.attr,
	&dev_attr_smc_global_dbg_ctl.attr,
	&dev_attr_sysreg_mif_ppc_debug.attr,
	&dev_attr_sci_dump_enable.attr,
	&dev_attr_smc_dump_enable.attr,
	&dev_attr_llc_ctrl.attr,
	&dev_attr_llc_id.attr,
	&dev_attr_llc_addr_match.attr,
	&dev_attr_llc_addr_mask.attr,
	&dev_attr_irps_llc_user_config_default.attr,
	&dev_attr_irps_llc_user_config_match.attr,
	&dev_attr_irps_llc_user_config_user.attr,
	&dev_attr_cacheaid_user.attr,
	&dev_attr_cacheaid_ctrl.attr,
	&dev_attr_cacheaid_global_ctrl.attr,
	&dev_attr_cacheaid_default_ctrl.attr,
	&dev_attr_cacheaid_pmu_access_ctrl.attr,
	&dev_attr_cacheaid_pmu_access_info.attr,
	NULL,
};

static struct attribute_group exynos_sci_dbg_attr_group = {
	.name	= "sci_dbg_attr",
	.attrs	= exynos_sci_dbg_sysfs_entries,
};

#if IS_ENABLED(CONFIG_USE_PPC_LOG)
static void __iomem *exynos_sci_dbg_remap(unsigned long addr, unsigned int size)
{
	int i;
	unsigned int num_pages = (size >> PAGE_SHIFT);
	pgprot_t prot = pgprot_writecombine(PAGE_KERNEL);
	struct page **pages = NULL;
	void __iomem *v_addr = NULL;

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

	return v_addr;
}
#endif
static int exynos_sci_dbg_probe(struct platform_device *pdev)
{
	int ret = 0;
	struct reserved_mem *rmem;
	struct device_node *rmem_np;
	struct exynos_sci_dbg_data *data;

	rmem_np = of_parse_phandle(pdev->dev.of_node, "memory-region", 0);
	rmem = of_reserved_mem_lookup(rmem_np);
	if (!rmem) {
		dev_err(&pdev->dev, "failed to acquire memory region\n");
		return 0;
	}

	ppc_reserved.p_addr = rmem->base;
	ppc_reserved.p_size = rmem->size;

	data = kzalloc(sizeof(struct exynos_sci_dbg_data), GFP_KERNEL);
	if (data == NULL) {
		SCI_DBG_ERR("%s: failed to allocate SCI_DBG device\n", __func__);
		ret = -ENOMEM;
		goto err_data;
	}

	sci_dbg_data = data;
	data->dev = &pdev->dev;

	spin_lock_init(&data->lock);

	data->sci_base = ioremap(SCI_BASE, SZ_4K);
	data->cacheaid_base = ioremap(CACHEAID_BASE, SZ_4K);
	if (IS_ERR(data->sci_base) || IS_ERR(data->cacheaid_base)) {
		SCI_DBG_ERR("%s: Failed SCI base remap\n", __func__);
		goto err_ioremap;
	}

#if IS_ENABLED(CONFIG_USE_PPC_LOG)
	data->dump_addr.p_addr = ppc_reserved.p_addr;
	data->dump_addr.p_size = ppc_reserved.p_size;
	data->dump_addr.v_addr = exynos_sci_dbg_remap(data->dump_addr.p_addr,
			data->dump_addr.p_size);
	SCI_DBG_INFO("%s: p_addr = 0x%08X\n", __func__, data->dump_addr.p_addr);
	SCI_DBG_INFO("%s: p_size = 0x%X\n", __func__, data->dump_addr.p_size);
	SCI_DBG_INFO("%s: v_addr = 0x%08X\n", __func__, data->dump_addr.v_addr);

	data->dump_addr.buff_size = data->dump_addr.p_size;

	dbg_snapshot_add_bl_item_info("log_ppc",
			data->dump_addr.p_addr, data->dump_addr.p_size);

	data->dump_addr.cnt_sfr_base = ioremap(PPC_DEBUG_CCI, SZ_4K);/* PPC_DEBUG_CCI */
	data->dump_addr.trex_core_base = ioremap(SYSREG_CORE_PPC_BASE, SZ_4K);
	data->dump_addr.sci_base = ioremap(SCI_BASE, SZ_4K);

	data->dump_addr.smc_base = ioremap(SMC_ALL_BASE, SZ_4K);
	data->dump_addr.sysreg_mif_base[0] = ioremap(SYSREG_MIF0_BASE, SZ_8K);
	data->dump_addr.sysreg_mif_base[1] = ioremap(SYSREG_MIF1_BASE, SZ_8K);
	data->dump_addr.sysreg_mif_base[2] = ioremap(SYSREG_MIF2_BASE, SZ_8K);
	data->dump_addr.sysreg_mif_base[3] = ioremap(SYSREG_MIF3_BASE, SZ_8K);
	data->dump_addr.smc_mif_base[0] = ioremap(SMC_MIF0_BASE, SZ_8K);
	data->dump_addr.smc_mif_base[1] = ioremap(SMC_MIF1_BASE, SZ_8K);
	data->dump_addr.smc_mif_base[2] = ioremap(SMC_MIF2_BASE, SZ_8K);
	data->dump_addr.smc_mif_base[3] = ioremap(SMC_MIF3_BASE, SZ_8K);
	data->dump_addr.ppc_dbg_base[0] = ioremap(PPC_DEBUG0_BASE, SZ_4K);
	data->dump_addr.ppc_dbg_base[1] = ioremap(PPC_DEBUG1_BASE, SZ_4K);
	data->dump_addr.ppc_dbg_base[2] = ioremap(PPC_DEBUG2_BASE, SZ_4K);
	data->dump_addr.ppc_dbg_base[3] = ioremap(PPC_DEBUG3_BASE, SZ_4K);

	if (IS_ERR(data->dump_addr.cnt_sfr_base) ||
			IS_ERR(data->dump_addr.trex_core_base) ||
			IS_ERR(data->dump_addr.sci_base) ||
			IS_ERR(data->dump_addr.smc_base) ||
			IS_ERR(data->dump_addr.sysreg_mif_base[0]) ||
			IS_ERR(data->dump_addr.sysreg_mif_base[1]) ||
			IS_ERR(data->dump_addr.sysreg_mif_base[2]) ||
			IS_ERR(data->dump_addr.sysreg_mif_base[3]) ||
			IS_ERR(data->dump_addr.smc_mif_base[0]) ||
			IS_ERR(data->dump_addr.smc_mif_base[1]) ||
			IS_ERR(data->dump_addr.smc_mif_base[2]) ||
			IS_ERR(data->dump_addr.smc_mif_base[3]) ||
			IS_ERR(data->dump_addr.ppc_dbg_base[0]) ||
			IS_ERR(data->dump_addr.ppc_dbg_base[1]) ||
			IS_ERR(data->dump_addr.ppc_dbg_base[2]) ||
			IS_ERR(data->dump_addr.ppc_dbg_base[3]))
		goto err_ioremap_all;

	hrtimer_init(&data->hrtimer, CLOCK_MONOTONIC, HRTIMER_MODE_REL);
	data->hrtimer.function = count_monitor;

	hrtimer_init(&data->smc_hrtimer, CLOCK_MONOTONIC, HRTIMER_MODE_REL);
	data->smc_hrtimer.function = smc_count_monitor;
#else
	carveout_reserved_mem(rmem);
	SCI_DBG_INFO("%s: No use ppc log mem\n", __func__);
#endif
	data->dump_addr.trex_irps_base[0] = ioremap(TREX_IRPS0_BASE, SZ_4K);
	data->dump_addr.trex_irps_base[1] = ioremap(TREX_IRPS1_BASE, SZ_4K);
	data->dump_addr.trex_irps_base[2] = ioremap(TREX_IRPS2_BASE, SZ_4K);
	data->dump_addr.trex_irps_base[3] = ioremap(TREX_IRPS3_BASE, SZ_4K);

	if (IS_ERR(data->dump_addr.trex_irps_base[0]) ||
			IS_ERR(data->dump_addr.trex_irps_base[1]) ||
			IS_ERR(data->dump_addr.trex_irps_base[2]) ||
			IS_ERR(data->dump_addr.trex_irps_base[3]))
		goto err_ioremap_all2;

	platform_set_drvdata(pdev, data);

	ret = sysfs_create_group(&data->dev->kobj, &exynos_sci_dbg_attr_group);
	if (ret)
		SCI_DBG_ERR("%s: failed creat sysfs for Exynos SCI_DBG\n", __func__);

	SCI_DBG_INFO("%s: exynos SCI_DBG driver is initialized!!\n", __func__);

	return 0;

err_ioremap_all2:
	iounmap(data->dump_addr.trex_irps_base[0]);
	iounmap(data->dump_addr.trex_irps_base[1]);
	iounmap(data->dump_addr.trex_irps_base[2]);
	iounmap(data->dump_addr.trex_irps_base[3]);

#if IS_ENABLED(CONFIG_USE_PPC_LOG)
err_ioremap_all:
	iounmap(data->dump_addr.cnt_sfr_base);
	iounmap(data->dump_addr.trex_core_base);
	iounmap(data->dump_addr.sci_base);
	iounmap(data->dump_addr.smc_base);
	iounmap(data->dump_addr.sysreg_mif_base[0]);
	iounmap(data->dump_addr.sysreg_mif_base[1]);
	iounmap(data->dump_addr.sysreg_mif_base[2]);
	iounmap(data->dump_addr.sysreg_mif_base[3]);
	iounmap(data->dump_addr.smc_mif_base[0]);
	iounmap(data->dump_addr.smc_mif_base[1]);
	iounmap(data->dump_addr.smc_mif_base[2]);
	iounmap(data->dump_addr.smc_mif_base[3]);
	iounmap(data->dump_addr.ppc_dbg_base[0]);
	iounmap(data->dump_addr.ppc_dbg_base[1]);
	iounmap(data->dump_addr.ppc_dbg_base[2]);
	iounmap(data->dump_addr.ppc_dbg_base[3]);
#endif

err_ioremap:
	kfree(data);

err_data:
	return ret;
}

static int exynos_sci_dbg_remove(struct platform_device *pdev)
{
	struct exynos_sci_dbg_data *data = platform_get_drvdata(pdev);

	sysfs_remove_group(&data->dev->kobj, &exynos_sci_dbg_attr_group);
	platform_set_drvdata(pdev, NULL);
	iounmap(data->sci_base);
	iounmap(data->dump_addr.cnt_sfr_base);
	kfree(data);

	SCI_DBG_INFO("%s: exynos SCI_DBG driver is removed!!\n", __func__);

	return 0;
}

static struct platform_device_id exynos_sci_dbg_driver_ids[] = {
	{ .name = EXYNOS_SCI_DBG_MODULE_NAME, },
	{},
};
MODULE_DEVICE_TABLE(platform, exynos_sci_dbg_driver_ids);

static const struct of_device_id exynos_sci_dbg_match[] = {
	{ .compatible = "samsung,exynos-sci_dbg", },
	{},
};

struct platform_driver exynos_sci_dbg_driver = {
	.remove = exynos_sci_dbg_remove,
	.id_table = exynos_sci_dbg_driver_ids,
	.driver = {
		.name = EXYNOS_SCI_DBG_MODULE_NAME,
		.owner = THIS_MODULE,
		.of_match_table = exynos_sci_dbg_match,
	},
	.probe = exynos_sci_dbg_probe,
};

MODULE_AUTHOR("Junhee Yoo <jh22.yoo@samsung.com>, Taekki Kim <taekki.kim@samsung.com>");
MODULE_DESCRIPTION("Samsung SCI_DBG dump-to-DRAM driver");
MODULE_LICENSE("GPL");
