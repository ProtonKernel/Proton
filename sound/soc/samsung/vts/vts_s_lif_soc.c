/* sound/soc/samsung/vts/vts_s_lif.c
 *
 * ALSA SoC - Samsung VTS Serial Local Interface driver
 *
 * Copyright (c) 2019 Samsung Electronics Co. Ltd.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

/* #define DEBUG */
#include <linux/clk.h>
#include <linux/io.h>
#include <linux/module.h>
#include <linux/of.h>
#include <linux/of_platform.h>
#include <linux/of_reserved_mem.h>
#include <linux/pm_runtime.h>
#include <linux/firmware.h>
#include <linux/dma-mapping.h>
#include <linux/proc_fs.h>
#include <linux/delay.h>
#include <linux/regmap.h>
#include <linux/sched/clock.h>
#include <linux/miscdevice.h>

#include <asm-generic/delay.h>

#include <sound/pcm_params.h>
#include <sound/tlv.h>
#include <sound/vts.h>

#include <sound/samsung/mailbox.h>
#include <sound/samsung/vts.h>
#include <soc/samsung/exynos-pmu-if.h>
#include <soc/samsung/exynos-el3_mon.h>

#include "vts.h"
#include "vts_res.h"
#include "vts_s_lif.h"
#include "vts_s_lif_soc.h"
#include "vts_s_lif_nm.h"
#include "vts_s_lif_clk_table.h"
#include "vts_s_lif_memlog.h"

#define vts_set_mask_value(id, mask, value) \
		{id = (typeof(id))((id & ~mask) | (value & mask)); }

#define vts_set_value_by_name(id, name, value) \
		vts_set_mask_value(id, name##_MASK, value << name##_L)

#define VTS_S_LIF_USE_AUD0
/* #define VTS_S_LIF_REG_LOW_DUMP */

void vts_s_lif_debug_pad_en(int en)
{
	volatile unsigned long gpio_peric1;

	gpio_peric1 =
		(volatile unsigned long)ioremap_nocache(0x10730100, 0x100);

	if (en) {
		writel(0x04440000, (volatile void *)(gpio_peric1 + 0x20));
		pr_info("gpio_peric1(0x120) 0x%08x\n",
				readl((volatile void *)(gpio_peric1 + 0x20)));
	} else {
		writel(0x00000000, (volatile void *)(gpio_peric1 + 0x20));
		pr_info("gpio_peric1(0x120) 0x%08x\n",
				readl((volatile void *)(gpio_peric1 + 0x20)));
	}
	iounmap((volatile void __iomem *)gpio_peric1);
}

#ifdef VTS_S_LIF_REG_LOW_DUMP
static void vts_s_lif_check_reg(int write_enable)
{
	volatile unsigned long rco_reg;
	volatile unsigned long gpv0_con;
	volatile unsigned long sysreg_vts;
	volatile unsigned long dmic_aud0;
	volatile unsigned long dmic_aud1;
	volatile unsigned long dmic_aud2;
	volatile unsigned long serial_lif;
	volatile unsigned long div_ratio;

	printk("%s : %d\n", __func__, __LINE__);
	/* check rco */
	rco_reg =
		(volatile unsigned long)ioremap_nocache(0x15860000, 0x1000);
	pr_info("rco_reg : 0x%8x\n",
			readl((volatile void *)(rco_reg + 0xb00)));
	iounmap((volatile void __iomem *)rco_reg);

	/* check gpv0 */
	gpv0_con =
		(volatile unsigned long)ioremap_nocache(0x15580000, 0x1000);
	pr_info("gpv0_con(0x00) 0x%08x 0x%08x 0x%08x\n",
			readl((volatile void *)(gpv0_con + 0x0)),
			readl((volatile void *)(gpv0_con + 0x4)),
			readl((volatile void *)(gpv0_con + 0x8)));
	pr_info("gpv0_con(0x10) 0x%08x 0x%08x 0x%08x\n",
			readl((volatile void *)(gpv0_con + 0x10)),
			readl((volatile void *)(gpv0_con + 0x14)),
			readl((volatile void *)(gpv0_con + 0x18)));
	pr_info("gpv0_con(0x20) 0x%08x 0x%08x 0x%08x\n",
			readl((volatile void *)(gpv0_con + 0x20)),
			readl((volatile void *)(gpv0_con + 0x24)),
			readl((volatile void *)(gpv0_con + 0x28)));
	iounmap((volatile void __iomem *)gpv0_con);

	/* check sysreg_vts */
	sysreg_vts =
		(volatile unsigned long)ioremap_nocache(0x15510000, 0x2000);
#if 0
	if (write_enable) {
		writel(0x1c0, (volatile void *)(sysreg_vts + 0x1000));
	}
#endif
	pr_info("sysreg_vts(0x1000) 0x%08x 0x%08x 0x%08x 0x%08x 0x%08x 0x%08x\n",
			readl((volatile void *)(sysreg_vts + 0x1000)),
			readl((volatile void *)(sysreg_vts + 0x1004)),
			readl((volatile void *)(sysreg_vts + 0x1008)),
			readl((volatile void *)(sysreg_vts + 0x100C)),
			readl((volatile void *)(sysreg_vts + 0x1010)),
			readl((volatile void *)(sysreg_vts + 0x1014)));
	pr_info("sysreg_vts(0x0108) 0x%08x\n",
			readl((volatile void *)(sysreg_vts + 0x108)));
	iounmap((volatile void __iomem *)sysreg_vts);

	/* check dmic_aud0 *//* undescribed register */
	dmic_aud0 =
		(volatile unsigned long)ioremap_nocache(0x15350000, 0x10);
	pr_info("vts_dmic_aud0(0x0) 0x%08x 0x%08x 0x%08x 0x%08x 0x%08x",
			readl((volatile void *)(dmic_aud0 + 0x0)),
			readl((volatile void *)(dmic_aud0 + 0x4)),
			readl((volatile void *)(dmic_aud0 + 0x8)),
			readl((volatile void *)(dmic_aud0 + 0xC)),
			readl((volatile void *)(dmic_aud0 + 0x10)));
	iounmap((volatile void __iomem *)dmic_aud0);
	/* check dmic_aud1 */
	dmic_aud1 =
		(volatile unsigned long)ioremap_nocache(0x15360000, 0x10);
	pr_info("vts_dmic_aud1(0x1) 0x%08x 0x%08x 0x%08x 0x%08x 0x%08x",
			readl((volatile void *)(dmic_aud1 + 0x0)),
			readl((volatile void *)(dmic_aud1 + 0x4)),
			readl((volatile void *)(dmic_aud1 + 0x8)),
			readl((volatile void *)(dmic_aud1 + 0xC)),
			readl((volatile void *)(dmic_aud1 + 0x10)));
	iounmap((volatile void __iomem *)dmic_aud1);
	/* check dmic_aud2 */
	dmic_aud2 =
		(volatile unsigned long)ioremap_nocache(0x15370000, 0x10);
	pr_info("vts_dmic_aud2(0x2) 0x%08x 0x%08x 0x%08x 0x%08x 0x%08x",
			readl((volatile void *)(dmic_aud2 + 0x0)),
			readl((volatile void *)(dmic_aud2 + 0x4)),
			readl((volatile void *)(dmic_aud2 + 0x8)),
			readl((volatile void *)(dmic_aud2 + 0xC)),
			readl((volatile void *)(dmic_aud2 + 0x10)));
	iounmap((volatile void __iomem *)dmic_aud2);

	/* check serial lif */
	serial_lif =
		(volatile unsigned long)ioremap_nocache(0x15340100, 0x1000);
	pr_info("vts_s_lif(0x000) 0x%08x 0x%08x 0x%08x 0x%08x\n",
			readl((volatile void *)(serial_lif + 0x0)),
			readl((volatile void *)(serial_lif + 0x4)),
			readl((volatile void *)(serial_lif + 0x8)),
			readl((volatile void *)(serial_lif + 0xC)));
	pr_info("vts_s_lif(0x010) 0x%08x 0x%08x 0x%08x 0x%08x\n",
			readl((volatile void *)(serial_lif + 0x10)),
			readl((volatile void *)(serial_lif + 0x14)),
			readl((volatile void *)(serial_lif + 0x18)),
			readl((volatile void *)(serial_lif + 0x1C)));
	pr_info("vts_s_lif(0x020) 0x%08x 0x%08x 0x%08x 0x%08x\n",
			readl((volatile void *)(serial_lif + 0x20)),
			readl((volatile void *)(serial_lif + 0x24)),
			readl((volatile void *)(serial_lif + 0x28)),
			readl((volatile void *)(serial_lif + 0x2C)));
	pr_info("vts_s_lif(0x030) 0x%08x 0x%08x 0x%08x\n",
			readl((volatile void *)(serial_lif + 0x30)),
			readl((volatile void *)(serial_lif + 0x34)),
			readl((volatile void *)(serial_lif + 0x38)));
	pr_info("vts_s_lif(0x050) 0x%08x \n",
			readl((volatile void *)(serial_lif + 0x50)));
	pr_info("vts_s_lif(0x300) 0x%08x\n",
			readl((volatile void *)(serial_lif + 0x300)));
	iounmap((volatile void __iomem *)serial_lif);

	/* check dividr */
	div_ratio =
		(volatile unsigned long)ioremap_nocache(0x15500000, 0x2000);
	pr_info("vts_div_ratio(0x1804) 0x%08x 0x%08x 0x%08x 0x%08x\n",
			readl((volatile void *)(div_ratio + 0x1804)),
			readl((volatile void *)(div_ratio + 0x1808)),
			readl((volatile void *)(div_ratio + 0x1818)),
			readl((volatile void *)(div_ratio + 0x181c)));
#if 0
#ifdef VTS_S_LIF_USE_AUD0
/* PLL_AUD0 */
{
	unsigned int val = 0;
	/* CLK_CON_DIV_DIV_CLK_VTS_SERIAL_LIF */
	val = readl(div_ratio + 0x181c);
	val &= ~(0x1FF << 0);
	val |= (47 << 0);
	writel(val, (volatile void *)(div_ratio + 0x181c));

	/* CLK_CON_DIV_DIV_CLK_VTS_DMIC_AUD_DIV2 */
	val = readl(div_ratio + 0x1808);
	val &= ~(0x7 << 0);
	val |= (3 << 0);
	writel(val, (volatile void *)(div_ratio + 0x1808));
}
#else
/* OSCCLK_RCO */
{
	unsigned int val = 0;
	/* CLK_CON_DIV_DIV_CLK_VTS_SERIAL_LIF */
	val = readl(div_ratio + 0x181c);
	val &= ~(0x1FF << 0);
	val |= (3 << 0);
	writel(val, (volatile void *)(div_ratio + 0x181c));

	/* CLK_CON_DIV_DIV_CLK_VTS_DMIC_AUD_DIV2 */
	val = readl(div_ratio + 0x1808);
	val &= ~(0x7 << 0);
	val |= (3 << 0);
	writel(val, (volatile void *)(div_ratio + 0x1808));
}
#endif
#endif
	pr_info("vts_div_ratio(0x181c) 0x%08x 0x%08x 0x%08x 0x%08x\n",
			readl((volatile void *)(div_ratio + 0x1804)),
			readl((volatile void *)(div_ratio + 0x1808)),
			readl((volatile void *)(div_ratio + 0x1818)),
			readl((volatile void *)(div_ratio + 0x181c)));
	iounmap((volatile void __iomem *)div_ratio);
}
#endif

static u32 vts_s_lif_direct_readl(const volatile void __iomem *addr)
{
	u32 ret = readl(addr);

	return ret;
}

static void vts_s_lif_direct_writel(u32 b, volatile void __iomem *addr)
{
	writel(b, addr);
}

/* private functions */
static void vts_s_lif_soc_reset_status(struct vts_s_lif_data *data)
{
	/* set_bit(0, &data->mode); */
	data->enabled = 0;
	data->running = 0;
	/* data->state = 0; */
	/* data->fmt = -1 */;
}

static void vts_s_lif_soc_set_default_gain(struct vts_s_lif_data *data)
{
	data->gain_mode[0] =
		data->gain_mode[1] =
		data->gain_mode[2] = VTS_S_DEFAULT_GAIN_MODE;

	data->max_scale_gain[0] =
		data->max_scale_gain[1] =
		data->max_scale_gain[2] = VTS_S_DEFAULT_MAX_SCALE_GAIN;

	data->control_dmic_aud[0] =
		data->control_dmic_aud[1] =
		data->control_dmic_aud[2] = VTS_S_DEFAULT_CONTROL_DMIC_AUD;

	vts_s_lif_soc_dmic_aud_gain_mode_write(data, 0);
	vts_s_lif_soc_dmic_aud_gain_mode_write(data, 1);
	vts_s_lif_soc_dmic_aud_gain_mode_write(data, 2);
	vts_s_lif_soc_dmic_aud_max_scale_gain_write(data, 0);
	vts_s_lif_soc_dmic_aud_max_scale_gain_write(data, 1);
	vts_s_lif_soc_dmic_aud_max_scale_gain_write(data, 2);
	vts_s_lif_soc_dmic_aud_control_gain_write(data, 0);
	vts_s_lif_soc_dmic_aud_control_gain_write(data, 1);
	vts_s_lif_soc_dmic_aud_control_gain_write(data, 2);
}

static void vts_s_lif_soc_set_dmic_aud(struct vts_s_lif_data *data, int enable)
{
	if (enable) {
#if (VTS_S_SOC_VERSION(1, 0, 0) == CONFIG_SND_SOC_SAMSUNG_VTS_S_LIF_VERSION)
		vts_s_lif_direct_writel(0x80030000, data->dmic_aud[0] + 0x0);
		vts_s_lif_direct_writel(0x80030000, data->dmic_aud[1] + 0x0);
		vts_s_lif_direct_writel(0x80030000, data->dmic_aud[2] + 0x0);
#elif (VTS_S_SOC_VERSION(1, 1, 1) == CONFIG_SND_SOC_SAMSUNG_VTS_S_LIF_VERSION)
		vts_s_lif_direct_writel(0xA0030000, data->dmic_aud[0] + 0x0);
		vts_s_lif_direct_writel(0xA0030000, data->dmic_aud[1] + 0x0);
		vts_s_lif_direct_writel(0xA0030000, data->dmic_aud[2] + 0x0);
#elif (VTS_S_SOC_VERSION(1, 1, 2) >= CONFIG_SND_SOC_SAMSUNG_VTS_S_LIF_VERSION)
		vts_s_lif_direct_writel(0xA0030000, data->dmic_aud[0] + 0x0);
		vts_s_lif_direct_writel(0xA0030000, data->dmic_aud[1] + 0x0);
		vts_s_lif_direct_writel(0xA0030000, data->dmic_aud[2] + 0x0);
#else
#error "VTS_S_SOC_VERSION is not defined"
#endif
	} else {
	}
}

static void vts_s_lif_soc_set_sel_pad(struct vts_s_lif_data *data, int enable)
{
	struct device *dev = data->dev;
	unsigned int ctrl;

	if (enable) {
		vts_s_lif_direct_writel(0x7, data->sfr_sys_base +
			VTS_S_SEL_PAD_AUD_BASE);
		ctrl = vts_s_lif_direct_readl(data->sfr_sys_base +
				VTS_S_SEL_PAD_AUD_BASE);
		slif_info(dev, "SEL_PAD_AUD(0x%08x)\n", ctrl);

		vts_s_lif_direct_writel(0x38, data->sfr_sys_base +
			VTS_S_SEL_DIV2_CLK_BASE);
		ctrl = vts_s_lif_direct_readl(data->sfr_sys_base +
				VTS_S_SEL_DIV2_CLK_BASE);
		slif_info(dev, "SEL_DIV2_CLK(0x%08x)\n", ctrl);
	} else {
		vts_s_lif_direct_writel(0, data->sfr_sys_base +
			VTS_S_SEL_PAD_AUD_BASE);
		ctrl = vts_s_lif_direct_readl(data->sfr_sys_base +
				VTS_S_SEL_PAD_AUD_BASE);
		slif_info(dev, "SEL_PAD_AUD(0x%08x)\n", ctrl);

		vts_s_lif_direct_writel(0, data->sfr_sys_base +
			VTS_S_SEL_DIV2_CLK_BASE);
		ctrl = vts_s_lif_direct_readl(data->sfr_sys_base +
				VTS_S_SEL_DIV2_CLK_BASE);
		slif_info(dev, "SEL_DIV2_CLK(0x%08x)\n", ctrl);
	}
}

/* public functions */
int vts_s_lif_soc_dmic_aud_gain_mode_write(struct vts_s_lif_data *data,
		unsigned int id)
{
	struct device *dev = data->dev;
	struct regmap *regmap = data->regmap_dmic_aud[id];
	int ret = 0;

	ret = regmap_write(regmap,
			VTS_S_SFR_GAIN_MODE_BASE,
			data->gain_mode[id]);
	if (ret < 0)
		slif_err(dev, "Failed to write GAIN_MODE(%d): %d\n",
			id, data->gain_mode[id]);

	return ret;
}

int vts_s_lif_soc_dmic_aud_gain_mode_get(struct vts_s_lif_data *data,
		unsigned int id, unsigned int *val)
{
	struct device *dev = data->dev;
	struct regmap *regmap = data->regmap_dmic_aud[id];
	int ret = 0;

	ret = regmap_read(regmap,
			VTS_S_SFR_GAIN_MODE_BASE,
			&data->gain_mode[id]);

	if (ret < 0)
		slif_err(dev, "Failed to get GAIN_MODE(%d): %d\n",
			id, data->gain_mode[id]);

	*val = data->gain_mode[id];

	return ret;
}

int vts_s_lif_soc_dmic_aud_gain_mode_put(struct vts_s_lif_data *data,
		unsigned int id, unsigned int val)
{
	int ret = 0;

	data->gain_mode[id] = val;
	ret = vts_s_lif_soc_dmic_aud_gain_mode_write(data, id);

	return ret;
}


int vts_s_lif_soc_dmic_aud_max_scale_gain_write(struct vts_s_lif_data *data,
		unsigned int id)
{
	struct device *dev = data->dev;
	struct regmap *regmap = data->regmap_dmic_aud[id];
	int ret = 0;

	ret = regmap_write(regmap,
			VTS_S_SFR_MAX_SCALE_GAIN_BASE,
			data->max_scale_gain[id]);
	if (ret < 0)
		slif_err(dev, "Failed to write MAX_SCALE_GAIN(%d): %d\n",
				id, data->max_scale_gain[id]);

	return ret;
}

int vts_s_lif_soc_dmic_aud_max_scale_gain_get(struct vts_s_lif_data *data,
		unsigned int id, unsigned int *val)
{
	struct device *dev = data->dev;
	struct regmap *regmap = data->regmap_dmic_aud[id];
	int ret = 0;

	ret = regmap_read(regmap,
			VTS_S_SFR_MAX_SCALE_GAIN_BASE,
			&data->max_scale_gain[id]);

	if (ret < 0)
		slif_err(dev, "Failed to get MAX_SCALE_GAIN(%d): %d\n",
				id, data->max_scale_gain[id]);

	*val = data->max_scale_gain[id];

	return ret;
}

int vts_s_lif_soc_dmic_aud_max_scale_gain_put(struct vts_s_lif_data *data,
		unsigned int id, unsigned int val)
{
	int ret = 0;

	data->max_scale_gain[id] = val;
	ret = vts_s_lif_soc_dmic_aud_max_scale_gain_write(data, id);

	return ret;
}

int vts_s_lif_soc_dmic_aud_control_gain_write(struct vts_s_lif_data *data,
		unsigned int id)
{
	struct device *dev = data->dev;
	struct regmap *regmap = data->regmap_dmic_aud[id];
	int ret = 0;

	ret = regmap_write(regmap,
			VTS_S_SFR_CONTROL_DMIC_AUD_BASE,
			data->control_dmic_aud[id]);

	if (ret < 0)
		slif_err(dev, "Failed to write CONTROL_DMIC_AUD(%d): %d\n",
				id, data->control_dmic_aud[id]);

	return ret;
}

int vts_s_lif_soc_dmic_aud_control_gain_get(struct vts_s_lif_data *data,
		unsigned int id, unsigned int *val)
{
	struct device *dev = data->dev;
	struct regmap *regmap = data->regmap_dmic_aud[id];
	unsigned int mask = 0;
	int ret = 0;

	ret = regmap_read(regmap,
			VTS_S_SFR_CONTROL_DMIC_AUD_BASE,
			&data->control_dmic_aud[id]);

	if (ret < 0)
		slif_err(dev, "Failed to get CONTROL_DMIC_AUD(%d): %d\n",
				id, data->control_dmic_aud[id]);

	mask = VTS_S_SFR_CONTROL_DMIC_AUD_GAIN_MASK;
	*val = ((data->control_dmic_aud[id] & mask) >>
			VTS_S_SFR_CONTROL_DMIC_AUD_GAIN_L);

	slif_dbg(dev, "mask 0x%x\n", mask);
	slif_dbg(dev, "dmic aud[%d] 0x%x \n",
			id, data->control_dmic_aud[id]);
	slif_dbg(dev, "val 0x%x \n", *val);

	return ret;
}

int vts_s_lif_soc_dmic_aud_control_gain_put(struct vts_s_lif_data *data,
		unsigned int id, unsigned int val)
{
	struct device *dev = data->dev;
	unsigned int mask = 0;
	unsigned int value = 0;
	int ret = 0;

	mask = VTS_S_SFR_CONTROL_DMIC_AUD_GAIN_MASK;
	value = (val << VTS_S_SFR_CONTROL_DMIC_AUD_GAIN_L) & mask;
	data->control_dmic_aud[id] &= ~mask;
	data->control_dmic_aud[id] |= value;

	slif_dbg(dev, "mask 0x%x val 0x%x value 0x%x\n",
			mask, val, value);
	slif_dbg(dev, "dmic aud[%d] 0x%x \n",
			id, data->control_dmic_aud[id]);

	ret = vts_s_lif_soc_dmic_aud_control_gain_write(data, id);

	return ret;
}

int vts_s_lif_soc_vol_set_get(struct vts_s_lif_data *data,
		unsigned int id, unsigned int *val)
{
	struct device *dev = data->dev;
	struct snd_soc_component *cmpnt = data->cmpnt;
	unsigned int ctrl;
	unsigned int volumes;
	int ret = 0;

	ret = snd_soc_component_read(cmpnt, VTS_S_VOL_SET(id), &ctrl);
	if (ret < 0)
		slif_err(dev, "failed:%d\n", ret);

	volumes = (ctrl & VTS_S_VOL_SET_MASK);

	slif_dbg(dev, "(0x%08x, %u)\n", id, volumes);

	*val = volumes;

	return ret;
}

int vts_s_lif_soc_vol_set_put(struct vts_s_lif_data *data,
		unsigned int id, unsigned int val)
{
	struct device *dev = data->dev;
	struct snd_soc_component *cmpnt = data->cmpnt;
	int ret = 0;

	ret = snd_soc_component_update_bits(cmpnt,
			VTS_S_VOL_SET(id),
			VTS_S_VOL_SET_MASK,
			val << VTS_S_VOL_SET_L);
	if (ret < 0)
		slif_err(dev, "failed:%d\n", ret);

	slif_dbg(dev, "(0x%08x, %u)\n", id, val);

	return ret;
}

int vts_s_lif_soc_vol_change_get(struct vts_s_lif_data *data,
		unsigned int id, unsigned int *val)
{
	struct device *dev = data->dev;
	struct snd_soc_component *cmpnt = data->cmpnt;
	unsigned int ctrl;
	unsigned int volumes;
	int ret = 0;

	ret = snd_soc_component_read(cmpnt, VTS_S_VOL_CHANGE(id), &ctrl);
	if (ret < 0)
		slif_err(dev, "failed:%d\n", ret);

	volumes = (ctrl & VTS_S_VOL_CHANGE_MASK);

	slif_dbg(dev, "(0x%08x, %u)\n", id, volumes);

	*val = volumes;

	return ret;
}

int vts_s_lif_soc_vol_change_put(struct vts_s_lif_data *data,
		unsigned int id, unsigned int val)
{
	struct device *dev = data->dev;
	struct snd_soc_component *cmpnt = data->cmpnt;
	int ret = 0;

	ret = snd_soc_component_update_bits(cmpnt,
			VTS_S_VOL_CHANGE(id),
			VTS_S_VOL_CHANGE_MASK,
			val << VTS_S_VOL_CHANGE_L);
	if (ret < 0)
		slif_err(dev, "failed:%d\n", ret);

	slif_dbg(dev, "(0x%08x, %u)\n", id, val);

	return ret;
}

int vts_s_lif_soc_channel_map_get(struct vts_s_lif_data *data,
		unsigned int id, unsigned int *val)
{
#if (VTS_S_SOC_VERSION(1, 0, 0) == CONFIG_SND_SOC_SAMSUNG_VTS_S_LIF_VERSION)
	return 0;

#elif (VTS_S_SOC_VERSION(1, 1, 1) == CONFIG_SND_SOC_SAMSUNG_VTS_S_LIF_VERSION)
	struct device *dev = data->dev;
	struct snd_soc_component *cmpnt = data->cmpnt;
	unsigned int ctrl;
	unsigned int channel_map;
	unsigned int channel_map_mask;
	int ret = 0;

	if (id > 7) {
		slif_err(dev, "id(%d) is not valid\n", id);
		return -EINVAL;
	}

	ret = snd_soc_component_read(cmpnt, VTS_S_CHANNEL_MAP_BASE, &ctrl);
	if (ret < 0)
		slif_err(dev, "failed:%d\n", ret);

	channel_map_mask = VTS_S_CHANNEL_MAP_MASK(id);
	channel_map = ((ctrl & channel_map_mask) >>
			(VTS_S_CHANNEL_MAP_ITV * id));

	slif_dbg(dev, "(0x%08x 0x%08x)\n", ctrl, channel_map_mask);
	slif_dbg(dev, "(%d, 0x%08x)\n", id, channel_map);

	*val = channel_map;
	data->channel_map = ctrl;

	return ret;
#elif (VTS_S_SOC_VERSION(1, 1, 2) >= CONFIG_SND_SOC_SAMSUNG_VTS_S_LIF_VERSION)
	struct device *dev = data->dev;
	struct snd_soc_component *cmpnt = data->cmpnt;
	unsigned int ctrl;
	unsigned int channel_map;
	unsigned int channel_map_mask;
	int ret = 0;

	if (id > 7) {
		slif_err(dev, "id(%d) is not valid\n", id);
		return -EINVAL;
	}

	ret = snd_soc_component_read(cmpnt, VTS_S_CHANNEL_MAP_BASE, &ctrl);
	if (ret < 0)
		slif_err(dev, "failed:%d\n", ret);

	channel_map_mask = VTS_S_CHANNEL_MAP_MASK(id);
	channel_map = ((ctrl & channel_map_mask) >>
			(VTS_S_CHANNEL_MAP_ITV * id));

	slif_dbg(dev, "(0x%08x 0x%08x)\n", ctrl, channel_map_mask);
	slif_dbg(dev, "(%d, 0x%08x)\n", id, channel_map);

	*val = channel_map;
	data->channel_map = ctrl;

	return ret;
#endif
}

int vts_s_lif_soc_channel_map_put(struct vts_s_lif_data *data,
		unsigned int id, unsigned int val)
{
#if (VTS_S_SOC_VERSION(1, 0, 0) == CONFIG_SND_SOC_SAMSUNG_VTS_S_LIF_VERSION)
	return 0;

#elif (VTS_S_SOC_VERSION(1, 1, 1) == CONFIG_SND_SOC_SAMSUNG_VTS_S_LIF_VERSION)
	struct device *dev = data->dev;
	struct snd_soc_component *cmpnt = data->cmpnt;
	int ret = 0;

	if (id > 7) {
		slif_err(dev, "id(%d) is not valid\n", id);
		return -EINVAL;
	}

	ret = snd_soc_component_update_bits(cmpnt,
			VTS_S_CHANNEL_MAP_BASE,
			VTS_S_CHANNEL_MAP_MASK(id),
			val << (VTS_S_CHANNEL_MAP_ITV * id));
	if (ret < 0)
		slif_err(dev, "failed: %d\n", ret);

	snd_soc_component_read(cmpnt, VTS_S_CHANNEL_MAP_BASE, &data->channel_map);
	slif_info(dev, "(0x%08x, 0x%x)\n", id, data->channel_map);

	return ret;
#elif (VTS_S_SOC_VERSION(1, 1, 2) >= CONFIG_SND_SOC_SAMSUNG_VTS_S_LIF_VERSION)
	struct device *dev = data->dev;
	struct snd_soc_component *cmpnt = data->cmpnt;
	int ret = 0;

	if (id > 7) {
		slif_err(dev, "id(%d) is not valid\n", id);
		return -EINVAL;
	}

	ret = snd_soc_component_update_bits(cmpnt,
			VTS_S_CHANNEL_MAP_BASE,
			VTS_S_CHANNEL_MAP_MASK(id),
			val << (VTS_S_CHANNEL_MAP_ITV * id));
	if (ret < 0)
		slif_err(dev, "failed:%d\n", ret);

	snd_soc_component_read(cmpnt, VTS_S_CHANNEL_MAP_BASE, &data->channel_map);
	slif_info(dev, "(0x%08x, 0x%x)\n", id, data->channel_map);

	return ret;
#endif
}

int vts_s_lif_soc_dmic_aud_control_hpf_sel_get(struct vts_s_lif_data *data,
		unsigned int id, unsigned int *val)
{
	struct device *dev = data->dev;
	struct regmap *regmap = data->regmap_dmic_aud[id];
	unsigned int mask = 0;
	int ret = 0;

	ret = regmap_read(regmap,
			VTS_S_SFR_CONTROL_DMIC_AUD_BASE,
			&data->control_dmic_aud[id]);

	if (ret < 0)
		slif_err(dev, "Failed to get HPF_SEL(%d): %d\n",
				id, data->control_dmic_aud[id]);

	mask = VTS_S_SFR_CONTROL_DMIC_AUD_HPF_SEL_MASK;
	*val = ((data->control_dmic_aud[id] & mask) >>
			VTS_S_SFR_CONTROL_DMIC_AUD_HPF_SEL_L);

	slif_dbg(dev, "mask 0x%x\n", mask);
	slif_dbg(dev, "dmic aud[%d] 0x%x \n",
			id, data->control_dmic_aud[id]);
	slif_dbg(dev, "val 0x%x \n", *val);

	return ret;
}

int vts_s_lif_soc_dmic_aud_control_hpf_sel_put(struct vts_s_lif_data *data,
		unsigned int id, unsigned int val)
{
	struct device *dev = data->dev;
	unsigned int mask = 0;
	unsigned int value = 0;
	int ret = 0;

	mask = VTS_S_SFR_CONTROL_DMIC_AUD_HPF_SEL_MASK;
	value = (val << VTS_S_SFR_CONTROL_DMIC_AUD_HPF_SEL_L) & mask;
	data->control_dmic_aud[id] &= ~mask;
	data->control_dmic_aud[id] |= value;

	slif_dbg(dev, "mask 0x%x val 0x%x value 0x%x\n",
			mask, val, value);
	slif_dbg(dev, "dmic aud[%d] 0x%x \n",
			id, data->control_dmic_aud[id]);

	ret = vts_s_lif_soc_dmic_aud_control_gain_write(data, id);

	return ret;
}

int vts_s_lif_soc_dmic_aud_control_hpf_en_get(struct vts_s_lif_data *data,
		unsigned int id, unsigned int *val)
{
	struct device *dev = data->dev;
	struct regmap *regmap = data->regmap_dmic_aud[id];
	unsigned int mask = 0;
	int ret = 0;

	ret = regmap_read(regmap,
			VTS_S_SFR_CONTROL_DMIC_AUD_BASE,
			&data->control_dmic_aud[id]);

	if (ret < 0)
		slif_err(dev, "Failed to get HPF_EN(%d): %d\n",
				id, data->control_dmic_aud[id]);

	mask = VTS_S_SFR_CONTROL_DMIC_AUD_HPF_EN_MASK;
	*val = ((data->control_dmic_aud[id] & mask) >>
			VTS_S_SFR_CONTROL_DMIC_AUD_HPF_EN_L);
	slif_dbg(dev, "mask 0x%x\n", mask);
	slif_dbg(dev, "dmic aud[%d] 0x%x \n",
			id, data->control_dmic_aud[id]);
	slif_dbg(dev, "val 0x%x \n", *val);

	return ret;
}

int vts_s_lif_soc_dmic_aud_control_hpf_en_put(struct vts_s_lif_data *data,
		unsigned int id, unsigned int val)
{
	struct device *dev = data->dev;
	unsigned int mask = 0;
	unsigned int value = 0;
	int ret = 0;

	mask = VTS_S_SFR_CONTROL_DMIC_AUD_HPF_EN_MASK;
	value = (val << VTS_S_SFR_CONTROL_DMIC_AUD_HPF_EN_L) & mask;
	data->control_dmic_aud[id] &= ~mask;
	data->control_dmic_aud[id] |= value;

	slif_dbg(dev, "mask 0x%x val 0x%x value 0x%x\n",
			mask, val, value);
	slif_dbg(dev, "dmic aud[%d] 0x%x \n",
			id, data->control_dmic_aud[id]);

	ret = vts_s_lif_soc_dmic_aud_control_gain_write(data, id);

	return ret;
}

int vts_s_lif_soc_dmic_en_get(struct vts_s_lif_data *data,
		unsigned int id, unsigned int *val)
{
	int ret = 0;

	*val = data->dmic_en[id];

	return ret;
}

int vts_s_lif_soc_dmic_en_put(struct vts_s_lif_data *data,
		unsigned int id, unsigned int val, bool update)
{
	struct device *dev = data->dev;
	const char *pin_name;

	if (update)
		data->dmic_en[id] = val;

	switch (id) {
	case 0:
		if (!!val)
			pin_name = "dmic_default";
		else
			pin_name = "s_lif_0_idle";
		break;
	case 1:
		if (!!val)
			pin_name = "s_lif_1_default";
		else
			pin_name = "s_lif_1_idle";
		break;
	case 2:
		if (!!val)
			pin_name = "s_lif_2_default";
		else
			pin_name = "s_lif_2_idle";
		break;
	default:
		slif_err(dev, "failed:%d\n", -EINVAL);
		return -EINVAL;
	}

	if (!test_bit(VTS_S_STATE_OPENED, &data->state)) {
		slif_info(dev, "not powered\n");
		return 0;
	} else {
		return vts_cfg_gpio(data->dev_vts, pin_name);
	}
}

int vts_s_lif_soc_dmic_aud_control_sys_sel_put(struct vts_s_lif_data *data,
		unsigned int id, unsigned int val)
{
	struct device *dev = data->dev;
	unsigned int mask = 0;
	unsigned int value = 0;
	int ret = 0;

	mask = VTS_S_SFR_CONTROL_DMIC_AUD_SYS_SEL_MASK;
	value = (val << VTS_S_SFR_CONTROL_DMIC_AUD_SYS_SEL_L) & mask;
	data->control_dmic_aud[id] &= ~mask;
	data->control_dmic_aud[id] |= value;

	slif_dbg(dev, "mask 0x%x val 0x%x value 0x%x\n",
			mask, val, value);
	slif_dbg(dev, "dmic aud[%d] 0x%x \n",
			id, data->control_dmic_aud[id]);

	ret = vts_s_lif_soc_dmic_aud_control_gain_write(data, id);

	return ret;
}

static void vts_s_lif_mark_dirty_register(struct vts_s_lif_data *data)
{
	regcache_mark_dirty(data->regmap_dmic_aud[0]);
	regcache_mark_dirty(data->regmap_dmic_aud[1]);
	regcache_mark_dirty(data->regmap_dmic_aud[2]);
	regcache_mark_dirty(data->regmap_sfr);
}

static void vts_s_lif_save_register(struct vts_s_lif_data *data)
{
	regcache_cache_only(data->regmap_dmic_aud[0], true);
	regcache_mark_dirty(data->regmap_dmic_aud[0]);
	regcache_cache_only(data->regmap_dmic_aud[1], true);
	regcache_mark_dirty(data->regmap_dmic_aud[1]);
	regcache_cache_only(data->regmap_dmic_aud[2], true);
	regcache_mark_dirty(data->regmap_dmic_aud[2]);
	regcache_cache_only(data->regmap_sfr, true);
	regcache_mark_dirty(data->regmap_sfr);
}

static void vts_s_lif_restore_register(struct vts_s_lif_data *data)
{
	regcache_cache_only(data->regmap_dmic_aud[0], false);
	regcache_sync(data->regmap_dmic_aud[0]);
	regcache_cache_only(data->regmap_dmic_aud[1], false);
	regcache_sync(data->regmap_dmic_aud[1]);
	regcache_cache_only(data->regmap_dmic_aud[2], false);
	regcache_sync(data->regmap_dmic_aud[2]);
	regcache_cache_only(data->regmap_sfr, false);
	regcache_sync(data->regmap_sfr);
}

static int vts_s_lif_soc_set_fmt_master(struct vts_s_lif_data *data,
		unsigned int fmt)
{
	struct device *dev = data->dev;
	struct snd_soc_component *cmpnt = data->cmpnt;
	unsigned int ctrl;
	int ret = 0;

	slif_info(dev, "(0x%08x)\n", fmt);

	if (fmt < 0)
		return -EINVAL;

	ret = snd_soc_component_read(cmpnt, VTS_S_CONFIG_MASTER_BASE, &ctrl);
	if (ret < 0)
		slif_err(dev, "failed:%d\n", ret);

#if (VTS_S_SOC_VERSION(1, 0, 0) == CONFIG_SND_SOC_SAMSUNG_VTS_S_LIF_VERSION)
#elif (VTS_S_SOC_VERSION(1, 1, 1) == CONFIG_SND_SOC_SAMSUNG_VTS_S_LIF_VERSION)
	switch (fmt & SND_SOC_DAIFMT_INV_MASK) {
	case SND_SOC_DAIFMT_NB_NF:
		vts_set_value_by_name(ctrl, VTS_S_CONFIG_MASTER_WS_POLAR, 0);
		break;
	case SND_SOC_DAIFMT_NB_IF:
		vts_set_value_by_name(ctrl, VTS_S_CONFIG_MASTER_WS_POLAR, 1);
		break;
	case SND_SOC_DAIFMT_IB_NF:
		vts_set_value_by_name(ctrl, VTS_S_CONFIG_MASTER_WS_POLAR, 0);
		break;
	case SND_SOC_DAIFMT_IB_IF:
		vts_set_value_by_name(ctrl, VTS_S_CONFIG_MASTER_WS_POLAR, 1);
		break;
	default:
		ret = -EINVAL;
	}
#elif (VTS_S_SOC_VERSION(1, 1, 2) >= CONFIG_SND_SOC_SAMSUNG_VTS_S_LIF_VERSION)
	switch (fmt & SND_SOC_DAIFMT_INV_MASK) {
	case SND_SOC_DAIFMT_NB_NF:
		vts_set_value_by_name(ctrl, VTS_S_CONFIG_MASTER_WS_POLAR, 0);
		vts_set_value_by_name(ctrl, VTS_S_CONFIG_MASTER_BCLK_POLAR, 0);
		break;
	case SND_SOC_DAIFMT_NB_IF:
		vts_set_value_by_name(ctrl, VTS_S_CONFIG_MASTER_WS_POLAR, 1);
		vts_set_value_by_name(ctrl, VTS_S_CONFIG_MASTER_BCLK_POLAR, 1);
		break;
	case SND_SOC_DAIFMT_IB_NF:
		vts_set_value_by_name(ctrl, VTS_S_CONFIG_MASTER_WS_POLAR, 0);
		vts_set_value_by_name(ctrl, VTS_S_CONFIG_MASTER_BCLK_POLAR, 0);
		break;
	case SND_SOC_DAIFMT_IB_IF:
		vts_set_value_by_name(ctrl, VTS_S_CONFIG_MASTER_WS_POLAR, 1);
		vts_set_value_by_name(ctrl, VTS_S_CONFIG_MASTER_BCLK_POLAR, 1);
		break;
	default:
		ret = -EINVAL;
	}
#else
#error "VTS_S_SOC_VERSION is not defined"
#endif

	slif_info(dev, "ctrl(0x%08x)\n", ctrl);
	snd_soc_component_write(cmpnt, VTS_S_CONFIG_MASTER_BASE, ctrl);

	return ret;
}

static int vts_s_lif_soc_set_fmt_slave(struct vts_s_lif_data *data,
		unsigned int fmt)
{
	struct device *dev = data->dev;
	struct snd_soc_component *cmpnt = data->cmpnt;
	unsigned int ctrl;
	int ret = 0;

	slif_info(dev, "(0x%08x)\n", fmt);

	if (fmt < 0)
		return -EINVAL;

	ret = snd_soc_component_read(cmpnt, VTS_S_CONFIG_SLAVE_BASE, &ctrl);
	if (ret < 0)
		slif_err(dev, "failed:%d\n", ret);

#if (VTS_S_SOC_VERSION(1, 0, 0) == CONFIG_SND_SOC_SAMSUNG_VTS_S_LIF_VERSION)
#elif (VTS_S_SOC_VERSION(1, 1, 1) == CONFIG_SND_SOC_SAMSUNG_VTS_S_LIF_VERSION)
	switch (fmt & SND_SOC_DAIFMT_INV_MASK) {
	case SND_SOC_DAIFMT_NB_NF:
		vts_set_value_by_name(ctrl, VTS_S_CONFIG_SLAVE_WS_POLAR, 0);
		break;
	case SND_SOC_DAIFMT_NB_IF:
		vts_set_value_by_name(ctrl, VTS_S_CONFIG_SLAVE_WS_POLAR, 1);
		break;
	case SND_SOC_DAIFMT_IB_NF:
		vts_set_value_by_name(ctrl, VTS_S_CONFIG_SLAVE_WS_POLAR, 0);
		break;
	case SND_SOC_DAIFMT_IB_IF:
		vts_set_value_by_name(ctrl, VTS_S_CONFIG_SLAVE_WS_POLAR, 1);
		break;
	default:
		ret = -EINVAL;
	}
#elif (VTS_S_SOC_VERSION(1, 1, 2) >= CONFIG_SND_SOC_SAMSUNG_VTS_S_LIF_VERSION)
	switch (fmt & SND_SOC_DAIFMT_INV_MASK) {
	case SND_SOC_DAIFMT_NB_NF:
		vts_set_value_by_name(ctrl, VTS_S_CONFIG_SLAVE_WS_POLAR, 0);
		vts_set_value_by_name(ctrl, VTS_S_CONFIG_SLAVE_BCLK_POLAR, 0);
		break;
	case SND_SOC_DAIFMT_NB_IF:
		vts_set_value_by_name(ctrl, VTS_S_CONFIG_SLAVE_WS_POLAR, 1);
		vts_set_value_by_name(ctrl, VTS_S_CONFIG_SLAVE_BCLK_POLAR, 1);
		break;
	case SND_SOC_DAIFMT_IB_NF:
		vts_set_value_by_name(ctrl, VTS_S_CONFIG_SLAVE_WS_POLAR, 0);
		vts_set_value_by_name(ctrl, VTS_S_CONFIG_SLAVE_BCLK_POLAR, 0);
		break;
	case SND_SOC_DAIFMT_IB_IF:
		vts_set_value_by_name(ctrl, VTS_S_CONFIG_SLAVE_WS_POLAR, 1);
		vts_set_value_by_name(ctrl, VTS_S_CONFIG_SLAVE_BCLK_POLAR, 1);
		break;
	default:
		ret = -EINVAL;
	}
#else
#error "VTS_S_SOC_VERSION is not defined"
#endif

	slif_info(dev, "ctrl(0x%08x)\n", ctrl);
	snd_soc_component_write(cmpnt, VTS_S_CONFIG_SLAVE_BASE, ctrl);

	return ret;
}

int vts_s_lif_soc_set_fmt(struct vts_s_lif_data *data, unsigned int fmt)
{
	struct device *dev = data->dev;
	int ret = 0;

	slif_info(dev, "(0x%08x)\n", fmt);

	switch (fmt & SND_SOC_DAIFMT_MASTER_MASK) {
	case SND_SOC_DAIFMT_CBM_CFM:
	case SND_SOC_DAIFMT_CBM_CFS:
		set_bit(VTS_S_MODE_MASTER, &data->mode);
		break;
	case SND_SOC_DAIFMT_CBS_CFM:
	case SND_SOC_DAIFMT_CBS_CFS:
		set_bit(VTS_S_MODE_SLAVE, &data->mode);
		break;
	default:
		ret = -EINVAL;
	}

	data->fmt = fmt;

	return ret;
}

int vts_s_lif_soc_hw_params(struct snd_pcm_substream *substream,
		struct snd_pcm_hw_params *hw_params,
		struct vts_s_lif_data *data)
{
	struct device *dev = data->dev;
	struct snd_soc_component *cmpnt = data->cmpnt;
	unsigned int ctrl0, ctrl1;
	int clk_val = 0;
	int val = 0;
	int ret = 0;
	unsigned int i = 0;

	slif_info(dev, "[%c]\n",
			(substream->stream == SNDRV_PCM_STREAM_CAPTURE) ?
			'C' : 'P');

	data->channels = params_channels(hw_params);
	data->rate = params_rate(hw_params);
	data->width = params_width(hw_params);
	data->clk_table_id = vts_s_lif_clk_table_id_search(data->rate,
			data->width);

	slif_info(dev, "rate=%u, width=%d, channel=%u id=%d\n",
			data->rate, data->width,
			data->channels, data->clk_table_id);

	if (data->channels > 8) {
		slif_err(dev, "(%d) is not support channels\n", data->channels);
		return -EINVAL;
	}

	ret = snd_soc_component_read(cmpnt, VTS_S_CONFIG_MASTER_BASE, &ctrl0);
	if (ret < 0)
		slif_err(dev, "Failed to access CONFIG_MASTER sfr:%d\n",
				ret);
	ret = snd_soc_component_read(cmpnt, VTS_S_CONFIG_SLAVE_BASE, &ctrl1);
	if (ret < 0)
		slif_err(dev, "Failed to access CONFIG_SLAVE sfr:%d\n",
				ret);

#if (VTS_S_SOC_VERSION(1, 0, 0) == CONFIG_SND_SOC_SAMSUNG_VTS_S_LIF_VERSION)
	switch (params_format(hw_params)) {
	case SNDRV_PCM_FORMAT_S16:
		vts_set_value_by_name(ctrl0, VTS_S_CONFIG_MASTER_OPMODE, 1);
		vts_set_value_by_name(ctrl1, VTS_S_CONFIG_SLAVE_OPMODE, 1);

		break;
	case SNDRV_PCM_FORMAT_S24:
		vts_set_value_by_name(ctrl0, VTS_S_CONFIG_MASTER_OPMODE, 3);
		vts_set_value_by_name(ctrl1, VTS_S_CONFIG_SLAVE_OPMODE, 3);

		break;
	default:
		return -EINVAL;
	}
#elif (VTS_S_SOC_VERSION(1, 1, 1) == CONFIG_SND_SOC_SAMSUNG_VTS_S_LIF_VERSION)
	switch (params_format(hw_params)) {
	case SNDRV_PCM_FORMAT_S16:
		vts_set_value_by_name(ctrl0, VTS_S_CONFIG_MASTER_OP_D, 1);
		vts_set_value_by_name(ctrl1, VTS_S_CONFIG_SLAVE_OP_D, 1);

		break;
	case SNDRV_PCM_FORMAT_S24:
		vts_set_value_by_name(ctrl0, VTS_S_CONFIG_MASTER_OP_D, 0);
		vts_set_value_by_name(ctrl1, VTS_S_CONFIG_SLAVE_OP_D, 0);

		break;
	default:
		return -EINVAL;
	}

	vts_set_value_by_name(ctrl0, VTS_S_CONFIG_MASTER_OP_C,
			(data->channels - 1));
	vts_set_value_by_name(ctrl1, VTS_S_CONFIG_SLAVE_OP_C,
			(data->channels - 1));
#elif (VTS_S_SOC_VERSION(1, 1, 2) >= CONFIG_SND_SOC_SAMSUNG_VTS_S_LIF_VERSION)
	switch (params_format(hw_params)) {
	case SNDRV_PCM_FORMAT_S16:
		vts_set_value_by_name(ctrl0, VTS_S_CONFIG_MASTER_OP_D, 1);
		vts_set_value_by_name(ctrl1, VTS_S_CONFIG_SLAVE_OP_D, 1);

		break;
	case SNDRV_PCM_FORMAT_S24:
		vts_set_value_by_name(ctrl0, VTS_S_CONFIG_MASTER_OP_D, 0);
		vts_set_value_by_name(ctrl1, VTS_S_CONFIG_SLAVE_OP_D, 0);

		break;
	default:
		return -EINVAL;
	}

	vts_set_value_by_name(ctrl0, VTS_S_CONFIG_MASTER_OP_C,
			(data->channels - 1));
	vts_set_value_by_name(ctrl1, VTS_S_CONFIG_SLAVE_OP_C,
			(data->channels - 1));
#else
#error "VTS_S_SOC_VERSION is not defined"
#endif

	/* SYS_SEL */
	for (i = 0; i < VTS_S_LIF_DMIC_AUD_NUM; i++) {
		val = vts_s_lif_clk_table_get(data->clk_table_id,
				CLK_TABLE_SYS_SEL);

		ret = vts_s_lif_soc_dmic_aud_control_sys_sel_put(data, i, val);
		if (ret < 0)
			slif_err(dev, "failed SYS_SEL[%d] ctrl %d\n",
				i, ret);
	}

	clk_val = vts_s_lif_clk_table_get(data->clk_table_id,
			CLK_TABLE_DMIC_AUD);
	if (clk_val < 0)
		slif_err(dev, "Failed to find clk table : %s\n", "clk_dmic_aud");
	slif_info(dev, "clk_dmic_aud bef:%d\n",
			clk_get_rate(data->clk_dmic_aud));
	slif_dbg(dev, "find clk table : %s: (%d)\n", "clk_dmic_aud", clk_val);
	ret = clk_set_rate(data->clk_dmic_aud, clk_val);
	if (ret < 0) {
		slif_err(dev, "Failed to set rate : %s\n", "dmic_aud");
		return ret;
	}
	slif_info(dev, "clk_dmic_aud aft:%d\n",
			clk_get_rate(data->clk_dmic_aud));

#if (VTS_S_SOC_VERSION(1, 1, 1) >= CONFIG_SND_SOC_SAMSUNG_VTS_S_LIF_VERSION)
	clk_val = vts_s_lif_clk_table_get(data->clk_table_id,
			CLK_TABLE_DMIC_AUD_PAD);
	if (clk_val < 0)
		slif_err(dev, "Failed to find clk table : %s\n", "clk_dmic_pad");
	slif_info(dev, "find clk table : %s: (%d)\n", "clk_dmic_pad", clk_val);
	ret = clk_set_rate(data->clk_dmic_aud_pad, clk_val);
	if (ret < 0) {
		slif_err(dev, "Failed to set rate : %s\n", "dmic_aud_pad");
		return ret;
	}
#endif
	clk_val = vts_s_lif_clk_table_get(data->clk_table_id,
			CLK_TABLE_DMIC_AUD_DIV2);
	if (clk_val < 0)
		slif_err(dev, "Failed to find clk : %s\n", "clk_dmic_div2");
	slif_dbg(dev, "find clk table : %s: (%d)\n", "clk_dmic_div2", clk_val);
	slif_info(dev, "clk_dmic_aud_div2 bef:%d\n",
			clk_get_rate(data->clk_dmic_aud_div2));
	ret = clk_set_rate(data->clk_dmic_aud_div2, clk_val);
	if (ret < 0) {
		slif_err(dev, "Failed to set rate : %s\n", "dmic_aud_div2");
		return ret;
	}
	slif_info(dev, "clk_dmic_aud_div2 aft:%d\n",
			clk_get_rate(data->clk_dmic_aud_div2));

	clk_val = vts_s_lif_clk_table_get(data->clk_table_id,
			CLK_TABLE_SERIAL_LIF);
	if (clk_val < 0)
		slif_err(dev, "Failed to find clk table : %s\n",
				"clk_serial_lif");
	slif_info(dev, "find clk table : %s: (%d)\n",
			"clk_serial_lif", clk_val);
	/* change blck to supprot channel */
	clk_val = (clk_val * data->channels) / VTS_S_LIF_MAX_CHANNEL;
	slif_info(dev, "clk_s_lif bef:%d:%d\n",
			clk_val, clk_get_rate(data->clk_serial_lif));
	ret = clk_set_rate(data->clk_serial_lif, clk_val);
	if (ret < 0)
		slif_err(dev, "Failed to set rate : %s\n", "clk_s_lif");

	slif_info(dev, "clk_s_lif aft:%d\n",
			clk_get_rate(data->clk_serial_lif));
#if 0
	clk_val = 49152000;
	slif_info(dev, "clk_mux_dmic_aud bef:%d :%d\n",
			clk_val, clk_get_rate(data->clk_mux_dmic_aud));
	ret = clk_set_rate(data->clk_mux_dmic_aud, clk_val);
	if (ret < 0)
		slif_err(dev, "Failed to set rate : %s\n", "clk_mux_dmic_aud");

	slif_info(dev, "clk_mux_dmic_aud aft:%d\n",
			clk_get_rate(data->clk_mux_dmic_aud));
#endif
#ifdef VTS_S_LIF_USE_AUD0
	ret = clk_enable(data->clk_mux_dmic_aud);
	if (ret < 0) {
		slif_err(dev, "Failed to enable \
				clk_mux_dmic_aud: %d\n", ret);
		return ret;
	}
	ret = clk_enable(data->clk_mux_serial_lif);
	if (ret < 0) {
		slif_err(dev, "Failed to enable \
				clk_mux_serial_lif: %d\n", ret);
		return ret;
	}
#endif
#if (VTS_S_SOC_VERSION(1, 1, 1) >= CONFIG_SND_SOC_SAMSUNG_VTS_S_LIF_VERSION)
	ret = clk_enable(data->clk_dmic_aud_pad);
	if (ret < 0) {
		slif_err(dev, "Failed to enable clk_dmic_aud_pad: %d\n", ret);
		return ret;
	}
#endif
	ret = clk_enable(data->clk_dmic_aud_div2);
	if (ret < 0) {
		slif_err(dev, "Failed to enable clk_dmic_aud_div2: %d\n", ret);
		return ret;
	}
	ret = clk_enable(data->clk_dmic_aud);
	if (ret < 0) {
		slif_err(dev, "Failed to enable clk_dmic_aud: %d\n", ret);
		return ret;
	}
	ret = clk_enable(data->clk_serial_lif);
	if (ret < 0) {
		slif_err(dev, "Failed to enable clk_s_lif: %d\n", ret);
		return ret;
	}

	ret = snd_soc_component_write(cmpnt, VTS_S_CONFIG_MASTER_BASE, ctrl0);
	if (ret < 0)
		slif_err(dev, "Failed to access CONFIG_MASTER sfr:%d\n",
				ret);
	ret = snd_soc_component_write(cmpnt, VTS_S_CONFIG_SLAVE_BASE, ctrl1);
	if (ret < 0)
		slif_err(dev, "Failed to access CONFIG_SLAVE sfr:%d\n",
				ret);

	set_bit(VTS_S_STATE_SET_PARAM, &data->state);

	return 0;
}

int vts_s_lif_soc_startup(struct snd_pcm_substream *substream,
		struct vts_s_lif_data *data)
{
	struct device *dev = data->dev;
	int clk_val = 0;
	int ret = 0;

	slif_info(dev, "[%c]\n",
			(substream->stream == SNDRV_PCM_STREAM_CAPTURE) ?
			'C' : 'P');

	pm_runtime_get_sync(dev);

#ifdef VTS_S_LIF_USE_AUD0
	slif_info(dev, "clk_src is PLL_AUD:%d :%d\n",
			clk_val, clk_get_rate(data->clk_src));
#else
	clk_val = 49152000;
	slif_info(dev, "clk_src bef:%d :%d\n",
			clk_val, clk_get_rate(data->clk_src));
	ret = clk_set_rate(data->clk_src, clk_val);
	if (ret < 0)
		slif_err(dev, "Failed to set rate : %s\n", "clk_src");

	slif_info(dev, "clk_src aft:%d\n", clk_get_rate(data->clk_src));
#endif
	ret = clk_enable(data->clk_src);
	if (ret < 0) {
		slif_err(dev, "Failed to enable clk_src: %d\n", ret);
		goto err;
	}

#if 1
	ret = clk_set_rate(data->clk_mux_dmic_aud,
			clk_get_rate(data->clk_src));
	if (ret < 0) {
		slif_err(dev, "Failed to set rate : %s\n", "clk_mux_dmic_aud");
		return ret;
	}

	ret = clk_set_rate(data->clk_mux_serial_lif,
			clk_get_rate(data->clk_src));
	if (ret < 0) {
		slif_err(dev, "Failed to set rate : %s\n", "clk_mux_serial_lif");
		return ret;
	}
#endif

	vts_s_lif_restore_register(data);
	set_bit(VTS_S_STATE_OPENED, &data->state);

	vts_s_lif_soc_dmic_en_put(data, 0, data->dmic_en[0], true);
	vts_s_lif_soc_dmic_en_put(data, 1, data->dmic_en[1], true);
	vts_s_lif_soc_dmic_en_put(data, 2, data->dmic_en[2], true);

	return 0;

err:
	pm_runtime_put_sync(dev);
	return ret;
}

void vts_s_lif_soc_shutdown(struct snd_pcm_substream *substream,
		struct vts_s_lif_data *data)
{
	struct device *dev = data->dev;
	int ret_chk = 0;

	slif_dbg(dev, "[%c]\n",
			(substream->stream == SNDRV_PCM_STREAM_CAPTURE) ?
			'C' : 'P');

	/* make default pin state as idle to prevent conflict with vts */
	vts_s_lif_soc_dmic_en_put(data, 0, 0, false);
	vts_s_lif_soc_dmic_en_put(data, 1, 0, false);
	vts_s_lif_soc_dmic_en_put(data, 2, 0, false);

	vts_s_lif_save_register(data);

	/* reset status */
	vts_s_lif_soc_reset_status(data);

	slif_info(dev, " - set VTS clk\n");
	vts_set_clk_src(data->dev_vts, VTS_CLK_SRC_RCO);
	ret_chk = vts_chk_dmic_clk_mode(data->dev_vts);
	if (ret_chk < 0) {
		slif_info(dev, "ret_chk failed(%d)\n", ret_chk);
	}

	if (test_bit(VTS_S_STATE_SET_PARAM, &data->state)) {
		clk_disable(data->clk_serial_lif);
		clk_disable(data->clk_mux_serial_lif);
#if (VTS_S_SOC_VERSION(1, 1, 1) >= CONFIG_SND_SOC_SAMSUNG_VTS_S_LIF_VERSION)
		clk_disable(data->clk_dmic_aud_pad);
#endif
		clk_disable(data->clk_dmic_aud_div2);
		clk_disable(data->clk_dmic_aud);
		clk_disable(data->clk_mux_dmic_aud);
		clear_bit(VTS_S_STATE_SET_PARAM, &data->state);
	}
	clk_disable(data->clk_src);

	clear_bit(VTS_S_STATE_OPENED, &data->state);
	pm_runtime_mark_last_busy(dev);
	pm_runtime_put_sync(dev);
}

int vts_s_lif_soc_hw_free(struct snd_pcm_substream *substream,
		struct vts_s_lif_data *data)
{
	struct device *dev = data->dev;

	slif_dbg(dev, "[%c]\n",
			(substream->stream == SNDRV_PCM_STREAM_CAPTURE) ?
			'C' : 'P');

	return 0;
}

int vts_s_lif_soc_dma_en(int enable,
		struct vts_s_lif_data *data)
{
	struct device *dev = data->dev;
	struct snd_soc_component *cmpnt = data->cmpnt;
	unsigned int ctrl;
	int ret = 0;
	int ret_chk = 0;

	slif_info(dev, "enable(%d)\n", enable);

	if (unlikely(data->slif_dump_enabled)) {
		ret = vts_s_lif_soc_set_fmt_slave(data, data->fmt);
		ret |= vts_s_lif_soc_set_fmt_master(data, data->fmt);
		if (ret < 0) {
			slif_err(dev, "Failed to access CTRL sfr:%d\n",
					ret);
			return ret;
		}
	} else {
		if (test_bit(VTS_S_MODE_SLAVE, &data->mode))
			ret = vts_s_lif_soc_set_fmt_slave(data, data->fmt);
		if (test_bit(VTS_S_MODE_MASTER, &data->mode))
			ret = vts_s_lif_soc_set_fmt_master(data, data->fmt);

		if (ret < 0) {
			slif_err(dev, "Failed to access CTRL sfr:%d\n",
					ret);
			return ret;
		}
	}

	if (unlikely(data->slif_dump_enabled)) {
		ret = snd_soc_component_update_bits(cmpnt,
				VTS_S_CONFIG_DONE_BASE,
				VTS_S_CONFIG_DONE_MASTER_CONFIG_MASK |
				VTS_S_CONFIG_DONE_SLAVE_CONFIG_MASK |
				VTS_S_CONFIG_DONE_ALL_CONFIG_MASK,
				(enable << VTS_S_CONFIG_DONE_MASTER_CONFIG_L) |
				(enable << VTS_S_CONFIG_DONE_SLAVE_CONFIG_L) |
				(enable << VTS_S_CONFIG_DONE_ALL_CONFIG_L));
		if (ret < 0)
			slif_err(dev, "Failed to access CTRL sfr:%d\n",
				ret);
	} else {
		if (test_bit(VTS_S_MODE_SLAVE, &data->mode)) {
			ret = snd_soc_component_update_bits(cmpnt,
					VTS_S_CONFIG_DONE_BASE,
					VTS_S_CONFIG_DONE_SLAVE_CONFIG_MASK |
					VTS_S_CONFIG_DONE_ALL_CONFIG_MASK,
					(enable << VTS_S_CONFIG_DONE_SLAVE_CONFIG_L) |
					(enable << VTS_S_CONFIG_DONE_ALL_CONFIG_L));
			if (ret < 0)
				slif_err(dev, "Failed to access CTRL sfr:%d\n",
					ret);
		}
		if (test_bit(VTS_S_MODE_MASTER, &data->mode)) {
			ret = snd_soc_component_update_bits(cmpnt,
					VTS_S_CONFIG_DONE_BASE,
					VTS_S_CONFIG_DONE_MASTER_CONFIG_MASK |
					VTS_S_CONFIG_DONE_ALL_CONFIG_MASK,
					(enable << VTS_S_CONFIG_DONE_MASTER_CONFIG_L) |
					(enable << VTS_S_CONFIG_DONE_ALL_CONFIG_L));
			if (ret < 0)
				slif_err(dev, "Failed to access CTRL sfr:%d\n",
					ret);
		}
	}

	ret = snd_soc_component_read(cmpnt, VTS_S_CONFIG_DONE_BASE, &ctrl);
	if (ret < 0)
		slif_err(dev, "failed:%d\n", ret);
	slif_info(dev, "ctrl(0x%08x)\n", ctrl);

	/* PAD configuration */
	vts_s_lif_soc_set_sel_pad(data, enable);
	vts_s_lif_dmic_aud_en(data->dev_vts, enable);
	vts_s_lif_dmic_if_en(data->dev_vts, enable);

	/* HACK : MOVE to resume */
	if (enable)
		vts_pad_retention(false);

	/* DMIC_IF configuration */
	vts_s_lif_soc_set_dmic_aud(data, enable);

	data->mute_enable = enable;
	slif_info(dev, "en(%d) ms(%d)\n", enable, data->mute_ms);
	if (enable && (data->mute_ms != 0)) {
		/* queue delayed work at starting */
		schedule_delayed_work(&data->mute_work, msecs_to_jiffies(data->mute_ms));
	} else {
		/* check dmic port and enable EN bit */
		ret = snd_soc_component_update_bits(cmpnt,
				VTS_S_INPUT_EN_BASE,
				VTS_S_INPUT_EN_EN0_MASK |
				VTS_S_INPUT_EN_EN1_MASK |
				VTS_S_INPUT_EN_EN2_MASK,
				(enable << VTS_S_INPUT_EN_EN0_L) |
				(enable << VTS_S_INPUT_EN_EN1_L) |
				(enable << VTS_S_INPUT_EN_EN2_L));
		if (ret < 0)
			slif_err(dev, "Failed to access INPUT_EN sfr:%d\n",
				ret);
	}

	if (unlikely(data->slif_dump_enabled)) {
		ret = snd_soc_component_update_bits(cmpnt,
				VTS_S_CTRL_BASE,
				VTS_S_CTRL_SLAVE_IF_EN_MASK |
				VTS_S_CTRL_MASTER_IF_EN_MASK |
				VTS_S_CTRL_LOOPBACK_EN_MASK |
				VTS_S_CTRL_SPU_EN_MASK,
				(enable << VTS_S_CTRL_SLAVE_IF_EN_L) |
				(enable << VTS_S_CTRL_MASTER_IF_EN_L) |
				(enable << VTS_S_CTRL_LOOPBACK_EN_L) |
				(enable << VTS_S_CTRL_SPU_EN_L));
		if (ret < 0)
			slif_err(dev, "Failed to access CTRL sfr:%d\n",
				ret);
	} else {
		if (test_bit(VTS_S_MODE_SLAVE, &data->mode)) {
			ret = snd_soc_component_update_bits(cmpnt,
					VTS_S_CTRL_BASE,
					VTS_S_CTRL_SLAVE_IF_EN_MASK |
					VTS_S_CTRL_SPU_EN_MASK,
					(enable << VTS_S_CTRL_SLAVE_IF_EN_L) |
					(enable << VTS_S_CTRL_SPU_EN_L));
			if (ret < 0)
				slif_err(dev, "Failed to access CTRL sfr:%d\n",
					ret);
		}
		if (test_bit(VTS_S_MODE_MASTER, &data->mode)) {
			ret = snd_soc_component_update_bits(cmpnt,
					VTS_S_CTRL_BASE,
					VTS_S_CTRL_MASTER_IF_EN_MASK |
					VTS_S_CTRL_SPU_EN_MASK,
					(enable << VTS_S_CTRL_MASTER_IF_EN_L) |
					(enable << VTS_S_CTRL_SPU_EN_L));
			if (ret < 0)
				slif_err(dev, "Failed to access CTRL sfr:%d\n",
					ret);
		}
	}

	ret = snd_soc_component_read(cmpnt, VTS_S_CTRL_BASE, &ctrl);
	if (ret < 0)
		slif_err(dev, "failed:%d\n", ret);
	slif_info(dev, "ctrl(0x%08x)\n", ctrl);
#ifdef VTS_S_LIF_REG_LOW_DUMP
	vts_s_lif_check_reg(0);
#endif

	if (enable) {
		slif_info(dev, " - set VTS clk\n");
		vts_set_clk_src(data->dev_vts, VTS_CLK_SRC_AUD0);
		ret_chk = vts_chk_dmic_clk_mode(data->dev_vts);
		if (ret_chk < 0) {
			slif_info(dev, "ret_chk failed(%d)\n", ret_chk);
		}
	}
	return ret;
}

static struct clk *devm_clk_get_and_prepare(struct device *dev, const char *name)
{
	struct clk *clk;
	int result;

	clk = devm_clk_get(dev, name);
	if (IS_ERR(clk)) {
		slif_err(dev, "Failed to get clock %s\n", name);
		goto error;
	}

	result = clk_prepare(clk);
	if (result < 0) {
		slif_err(dev, "Failed to prepare clock %s\n", name);
		goto error;
	}

error:
	return clk;
}

static void vts_s_lif_soc_mute_func(struct work_struct *work)
{
	struct delayed_work *dwork = to_delayed_work(work);
	struct vts_s_lif_data *data = container_of(dwork, struct vts_s_lif_data,
			mute_work);
	struct device *dev = data->dev;
	struct snd_soc_component *cmpnt = data->cmpnt;
	int ret = 0;

	slif_info(dev, ":(en:%d)\n", data->mute_enable);

	/* check dmic port and enable EN bit */
	ret = snd_soc_component_update_bits(cmpnt,
			VTS_S_INPUT_EN_BASE,
			VTS_S_INPUT_EN_EN0_MASK |
			VTS_S_INPUT_EN_EN1_MASK |
			VTS_S_INPUT_EN_EN2_MASK,
			(data->mute_enable << VTS_S_INPUT_EN_EN0_L) |
			(data->mute_enable << VTS_S_INPUT_EN_EN1_L) |
			(data->mute_enable << VTS_S_INPUT_EN_EN2_L));
	if (ret < 0)
		slif_err(dev, "Failed to access INPUT_EN sfr:%d\n",
			ret);

}

static DECLARE_DELAYED_WORK(vts_s_lif_soc_mute, vts_s_lif_soc_mute_func);

int vts_s_lif_soc_probe(struct vts_s_lif_data *data)
{
	struct device *dev = data->dev;
	int ret = 0;

#ifdef VTS_S_LIF_USE_AUD0
	data->clk_src = devm_clk_get_and_prepare(dev, "aud0");
	if (IS_ERR(data->clk_src)) {
		ret = PTR_ERR(data->clk_src);
		goto err;
	}
#else
	data->clk_src = devm_clk_get_and_prepare(dev, "rco");
	if (IS_ERR(data->clk_src)) {
		ret = PTR_ERR(data->clk_src);
		goto err;
	}
#endif

	data->clk_mux_dmic_aud = devm_clk_get_and_prepare(dev, "mux_dmic_aud");
	if (IS_ERR(data->clk_mux_dmic_aud)) {
		ret = PTR_ERR(data->clk_mux_dmic_aud);
		goto err;
	}

#ifdef VTS_S_LIF_USE_AUD0
	/* HACK: only use in probe to disable VTS_AUD1_USER_MUX */
	data->clk_src1 = devm_clk_get_and_prepare(dev, "aud1");
	if (IS_ERR(data->clk_src1)) {
		ret = PTR_ERR(data->clk_src1);
		goto err;
	}
#endif

	data->clk_mux_serial_lif = devm_clk_get_and_prepare(dev, "mux_serial_lif");
	if (IS_ERR(data->clk_mux_serial_lif)) {
		ret = PTR_ERR(data->clk_mux_serial_lif);
		goto err;
	}

#if (VTS_S_SOC_VERSION(1, 1, 0) >= CONFIG_SND_SOC_SAMSUNG_VTS_S_LIF_VERSION)
	data->clk_dmic_aud_pad = devm_clk_get_and_prepare(dev, "dmic_pad");
	if (IS_ERR(data->clk_dmic_aud_pad)) {
		ret = PTR_ERR(data->clk_dmic_aud_pad);
		goto err;
	}
#endif

	data->clk_dmic_aud_div2 = devm_clk_get_and_prepare(dev, "dmic_aud_div2");
	if (IS_ERR(data->clk_dmic_aud_div2)) {
		ret = PTR_ERR(data->clk_dmic_aud_div2);
		goto err;
	}

	data->clk_dmic_aud = devm_clk_get_and_prepare(dev, "dmic_aud");
	if (IS_ERR(data->clk_dmic_aud)) {
		ret = PTR_ERR(data->clk_dmic_aud);
		goto err;
	}

	data->clk_serial_lif = devm_clk_get_and_prepare(dev, "serial_lif");
	if (IS_ERR(data->clk_serial_lif)) {
		ret = PTR_ERR(data->clk_serial_lif);
		goto err;
	}

#if 0
/* HACK: dummy code : disable input clk of mux_dmic_aud */
#ifdef VTS_S_LIF_USE_AUD0
	ret = clk_set_rate(data->clk_src, 0);
	if (ret < 0) {
		slif_err(dev, "Failed to set rate : %s\n", "clk_src");
		return ret;
	}
	ret = clk_enable(data->clk_src);
	if (ret < 0) {
		slif_err(dev, "Failed to enable clk_src: %d\n", ret);
		goto err;
	}

	ret = clk_set_rate(data->clk_src1, 0);
	if (ret < 0) {
		slif_err(dev, "Failed to set rate : %s\n", "clk_src1");
		return ret;
	}
	ret = clk_enable(data->clk_src1);
	if (ret < 0) {
		slif_err(dev, "Failed to enable clk_src1: %d\n", ret);
		goto err;
	}

	clk_disable(data->clk_src);
	clk_disable(data->clk_src1);
#endif
#endif

	data->mute_enable = false;
	data->mute_ms = 0;
	data->clk_input_path = VTS_S_CLK_PLL_AUD0;
	vts_s_lif_mark_dirty_register(data);
	vts_s_lif_save_register(data);

	vts_s_lif_soc_set_default_gain(data);

	pm_runtime_no_callbacks(dev);
	pm_runtime_enable(dev);

	INIT_DELAYED_WORK(&data->mute_work, vts_s_lif_soc_mute_func);

	return 0;

err:
	return ret;
}
