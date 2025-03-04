/*
 * Samsung EXYNOS SoC series MIPI CSI/DSI D/C-PHY driver
 *
 * Copyright (C) 2018 Samsung Electronics Co., Ltd.
 *		http://www.samsung.com
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#include <linux/err.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/mfd/syscon.h>
#include <linux/of.h>
#include <linux/of_platform.h>
#include <linux/phy/phy.h>
#include <linux/platform_device.h>
#include <linux/regmap.h>
#include <linux/spinlock.h>
#include <linux/io.h>
#include <linux/regulator/driver.h>
#include <linux/regulator/consumer.h>
#if IS_ENABLED(CONFIG_EXYNOS_PMU_IF)
#include <soc/samsung/exynos-pmu-if.h>
#else
#include <soc/samsung/exynos-pmu.h>
#endif

/* the maximum number of PHY for each module */
#define EXYNOS_MIPI_PHYS_NUM	4

#define EXYNOS_MIPI_PHY_MAX_REGULATORS 2
#define EXYNOS_MIPI_PHY_ISO_BYPASS  (1 << 0)

#define MIPI_PHY_MxSx_UNIQUE	(0 << 1)
#define MIPI_PHY_MxSx_SHARED	(1 << 1)
#define MIPI_PHY_MxSx_INIT_DONE	(2 << 1)

#define HIWORD(d) ((u16)(((u32)(d)) >> 16))
#define LOWORD(d) ((u16)(u32)(d))

enum exynos_mipi_phy_owner {
	EXYNOS_MIPI_PHY_OWNER_DSIM = 0,
	EXYNOS_MIPI_PHY_OWNER_CSIS = 1,
};

/* per MIPI-PHY module */
struct exynos_mipi_phy_data {
	u8 flags;
	int active_count;
	spinlock_t slock;
};

#define MKVER(ma, mi)	(((ma) << 16) | (mi))
enum phy_infos {
	VERSION,
	TYPE,
	LANES,
	SPEED,
	SETTLE,
};

struct exynos_mipi_phy_cfg {
	u16 major;
	u16 minor;
	u16 mode;
	/* u32 max_speed */
	int (*set)(void __iomem *regs, int option, u32 *info);
};

/* per DT MIPI-PHY node, can have multiple elements */
struct exynos_mipi_phy {
	struct device *dev;
	spinlock_t slock;
	struct regmap *reg_pmu;
	struct regmap *reg_reset;
	enum exynos_mipi_phy_owner owner;
	struct regulator *regulators[EXYNOS_MIPI_PHY_MAX_REGULATORS];
	struct mipi_phy_desc {
		struct phy *phy;
		struct exynos_mipi_phy_data *data;
		unsigned int index;
		unsigned int iso_offset;
		unsigned int rst_bit;
		void __iomem *regs;
	} phys[EXYNOS_MIPI_PHYS_NUM];
};

/* 1: Isolation bypass, 0: Isolation enable */
static int __set_phy_isolation(struct regmap *reg_pmu,
		unsigned int offset, unsigned int on)
{
	unsigned int val;
	int ret;

	val = on ? EXYNOS_MIPI_PHY_ISO_BYPASS : 0;

	if (reg_pmu)
		ret = regmap_update_bits(reg_pmu, offset,
			EXYNOS_MIPI_PHY_ISO_BYPASS, val);
	else
		ret = exynos_pmu_update(offset,
			EXYNOS_MIPI_PHY_ISO_BYPASS, val);

	if (ret)
		pr_err("%s failed to %s PHY isolation 0x%x\n",
				__func__, on ? "set" : "clear", offset);

	pr_debug("%s off=0x%x, val=0x%x\n", __func__, offset, val);

	return ret;
}

/* 1: Enable reset -> release reset, 0: Enable reset */
static int __set_phy_reset(struct regmap *reg_reset,
		unsigned int bit, unsigned int on)
{
	unsigned int cfg;
	int ret = 0;

	if (!reg_reset)
		return 0;

	if (on) {
		/* reset release */
		ret = regmap_update_bits(reg_reset, 0, BIT(bit), BIT(bit));
		if (ret)
			pr_err("%s failed to release reset PHY(%d)\n",
					__func__, bit);
	} else {
		/* reset assertion */
		ret = regmap_update_bits(reg_reset, 0, BIT(bit), 0);
		if (ret)
			pr_err("%s failed to reset PHY(%d)\n", __func__, bit);
	}

	regmap_read(reg_reset, 0, &cfg);
	pr_debug("%s bit=%d, cfg=0x%x\n", __func__, bit, cfg);

	return ret;
}

static int __set_phy_init(struct exynos_mipi_phy *state,
		struct mipi_phy_desc *phy_desc, unsigned int on)
{
	unsigned int cfg;
	int ret = 0;

	if (state->reg_pmu)
		ret = regmap_read(state->reg_pmu,
			phy_desc->iso_offset, &cfg);
	else
		ret = exynos_pmu_read(phy_desc->iso_offset, &cfg);

	if (ret) {
		dev_err(state->dev, "%s Can't read 0x%x\n",
				__func__, phy_desc->iso_offset);
		ret = -EINVAL;
		goto phy_exit;
	}

	/* Add INIT_DONE flag when ISO is already bypass(LCD_ON_UBOOT) */
	if (cfg && EXYNOS_MIPI_PHY_ISO_BYPASS)
		phy_desc->data->flags |= MIPI_PHY_MxSx_INIT_DONE;

phy_exit:
	return ret;
}

static int __set_phy_ldo(struct exynos_mipi_phy *state, bool on)
{
	int ret = 0;
	if (on) {
		/* [on sequence]
		*  (general) 0.85V on  -> 1.8V on  -> 1.2V on
		*  (current) <1.2V on> -> 0.85V on -> 1.8V on
		*  >> prohibition : 0.85 off -> 1.2 on state
		*/
		if (state->regulators[0]) {
			ret = regulator_enable(state->regulators[0]);
			if (ret) {
				dev_err(state->dev, "phy regulator 0.85V enable failed\n");
				return ret;
			}
		}
		usleep_range(100, 101);

		if (state->regulators[1]) {
			ret = regulator_enable(state->regulators[1]);
			if (ret) {
				dev_err(state->dev, "phy regulator 1.80V enable failed\n");
				return ret;
			}
		}
		usleep_range(100, 101);
	} else {
		/* [off sequence]
		* (current) <1.2V on> -> 1.8V off -> 0.85V off
		*/
		if (state->regulators[1]) {
			ret = regulator_disable(state->regulators[1]);
			if (ret) {
				dev_err(state->dev, "phy regulator 1.80V disable failed\n");
				return ret;
			}
		}
		usleep_range(1000, 1001);

		if (state->regulators[0]) {
			ret = regulator_disable(state->regulators[0]);
			if (ret) {
				dev_err(state->dev, "phy regulator 0.85V disable failed\n");
				return ret;
			}
		}
	}
	return ret;
}

static int __set_phy_alone(struct exynos_mipi_phy *state,
		struct mipi_phy_desc *phy_desc, unsigned int on)
{
	int ret = 0;
	unsigned long flags;

	spin_lock_irqsave(&state->slock, flags);

	if (on) {
		ret = __set_phy_isolation(state->reg_pmu,
				phy_desc->iso_offset, on);
		if (ret)
			goto phy_exit;
	} else {
		ret = __set_phy_isolation(state->reg_pmu,
				phy_desc->iso_offset, on);
	}

phy_exit:
	pr_debug("%s: isolation 0x%x, reset 0x%x\n", __func__,
			phy_desc->iso_offset, phy_desc->rst_bit);

	spin_unlock_irqrestore(&state->slock, flags);

	return ret;
}

static int __set_phy_share(struct exynos_mipi_phy *state,
		struct mipi_phy_desc *phy_desc, unsigned int on)
{
	int ret = 0;
	unsigned long flags;

	spin_lock_irqsave(&phy_desc->data->slock, flags);

	on ? ++(phy_desc->data->active_count) : --(phy_desc->data->active_count);

	/* If phy is already initialization(power_on) */
	if (state->owner == EXYNOS_MIPI_PHY_OWNER_DSIM &&
			phy_desc->data->flags & MIPI_PHY_MxSx_INIT_DONE) {
		phy_desc->data->flags &= (~MIPI_PHY_MxSx_INIT_DONE);
		spin_unlock_irqrestore(&phy_desc->data->slock, flags);
		return ret;
	}

	if (on) {
		/* Isolation bypass when reference count is 1 */
		if (phy_desc->data->active_count) {
			ret = __set_phy_isolation(state->reg_pmu,
					phy_desc->iso_offset, on);
			if (ret)
				goto phy_exit;
		}
	} else {
		/* Isolation enabled when reference count is zero */
		if (phy_desc->data->active_count == 0)
			ret = __set_phy_isolation(state->reg_pmu,
					phy_desc->iso_offset, on);
	}

phy_exit:
	pr_debug("%s: isolation 0x%x, reset 0x%x\n", __func__,
			phy_desc->iso_offset, phy_desc->rst_bit);

	spin_unlock_irqrestore(&phy_desc->data->slock, flags);

	return ret;
}

static int __set_phy_state(struct exynos_mipi_phy *state,
		struct mipi_phy_desc *phy_desc, unsigned int on)
{
	int ret = 0;

	if (phy_desc->data->flags & MIPI_PHY_MxSx_SHARED)
		ret = __set_phy_share(state, phy_desc, on);
	else
		ret = __set_phy_alone(state, phy_desc, on);

	return ret;
}

static void update_bits(void __iomem *addr, unsigned int start,
			unsigned int width, unsigned int val)
{
	unsigned int cfg;
	unsigned int mask = (width >= 32) ? 0xffffffff : ((1U << width) - 1);

	cfg = readl(addr);
	cfg &= ~(mask << start);
	cfg |= ((val & mask) << start);
	writel(cfg, addr);
}

#define PHY_REF_SPEED	(1500)
#define CPHY_REF_SPEED	(500)
static int __set_phy_cfg_0501_0000_dphy(void __iomem *regs, int option, u32 *cfg)
{

	int i;
	u32 skew_cal_en = 0;
	u32 skew_delay_sel = 0;
	u32 hs_mode_sel = 1;

	if (cfg[SPEED] >= PHY_REF_SPEED) {
		skew_cal_en = 1;

		if (cfg[SPEED] >= 3000)
			skew_delay_sel = 1;
		else if (cfg[SPEED] >= 2000)
			skew_delay_sel = 2;
		else
			skew_delay_sel = 3;

		hs_mode_sel = 0;
	}

	writel(0x2, regs + 0x0018);
	update_bits(regs + 0x0084, 0, 8, 0x1);
	for (i = 0; i <= cfg[LANES]; i++) {
		update_bits(regs + 0x04e0 + (i * 0x400), 0, 1, skew_cal_en);
		update_bits(regs + 0x043c + (i * 0x400), 5, 2, skew_delay_sel);
		update_bits(regs + 0x04ac + (i * 0x400), 2, 1, hs_mode_sel);
		update_bits(regs + 0x04b0 + (i * 0x400), 0, 8, cfg[SETTLE]);
	}

	return 0;
}

static int __set_phy_cfg_0502_0000_dphy(void __iomem *regs, int option, u32 *cfg)
{

	int i;
	u32 settle_clk_sel = 1;
	u32 skew_delay_sel = 0;
	u32 type = cfg[TYPE] & 0xffff;
	u32 skew_cal_en = 0;

	if (cfg[SPEED] >= PHY_REF_SPEED) {
		settle_clk_sel = 0;
		skew_cal_en = 1;
	}

	if (cfg[SPEED] >= PHY_REF_SPEED && cfg[SPEED] < 4000) {
		if (cfg[SPEED] >= 3000)
			skew_delay_sel = 1;
		else if (cfg[SPEED] >= 2000)
			skew_delay_sel = 2;
		else
			skew_delay_sel = 3;
	}

	writel(0x00000001, regs + 0x0000); /* SC_GNR_CON0 */
	writel(0x00001450, regs + 0x0004); /* SC_GNR_CON1 */
	writel(0x00000004, regs + 0x0008); /* SC_ANA_CON0 */
	writel(0x00009000, regs + 0x000c); /* SC_ANA_CON1 */
	writel(0x00000005, regs + 0x0010); /* SC_ANA_CON2 */
	writel(0x00000600, regs + 0x0014); /* SC_ANA_CON3 */
	writel(0x00000301, regs + 0x0030); /* SC_TIME_CON0 */

	for (i = 0; i <= cfg[LANES]; i++) {
		writel(0x00000001, regs + 0x0100 + (i * 0x100)); /* SD_GNR_CON0 */
		writel(0x00001450, regs + 0x0104 + (i * 0x100)); /* SD_GNR_CON1 */
		writel(0x00000004, regs + 0x0108 + (i * 0x100)); /* SD_ANA_CON0 */
		writel(0x00009000, regs + 0x010c + (i * 0x100)); /* SD_ANA_CON1 */
		writel(0x00000005, regs + 0x0110 + (i * 0x100)); /* SD_ANA_CON2 */
		update_bits(regs + 0x0110 + (i * 0x100), 8, 2, skew_delay_sel); /* SD_ANA_CON2 */
		writel(0x00000600, regs + 0x0114 + (i * 0x100)); /* SD_ANA_CON3 */
		/* DC Combo lane has below SFR (0/1/2) */
		if ((type == 0xDC) && (i < 3))
			writel(0x00000040, regs + 0x0124 + (i * 0x100)); /* SD_ANA_CON7 */
		update_bits(regs + 0x0130 + (i * 0x100), 0, 8, cfg[SETTLE]); /* SD_TIME_CON0 */
		update_bits(regs + 0x0130 + (i * 0x100), 8, 1, settle_clk_sel); /* SD_TIME_CON0 */
		writel(0x00000003, regs + 0x0134 + (i * 0x100)); /* SD_TIME_CON1 */
		update_bits(regs + 0x0140 + (i * 0x100), 0, 1, skew_cal_en); /* SD_DESKEW_CON0 */
		writel(0x0000081a, regs + 0x0150 + (i * 0x100)); /* SD_DESKEW_CON4 */
	}

	return 0;
}

static int __set_phy_cfg_0502_0001_dphy(void __iomem *regs, int option, u32 *cfg)
{

	int i;
	u32 settle_clk_sel = 1;
	u32 skew_delay_sel = 0;
	u32 skew_cal_en = 0;

	if (cfg[SPEED] >= PHY_REF_SPEED) {
		settle_clk_sel = 0;
		skew_cal_en = 1;
	}

	if (cfg[SPEED] >= PHY_REF_SPEED && cfg[SPEED] < 4000) {
		if (cfg[SPEED] >= 3000)
			skew_delay_sel = 1;
		else if (cfg[SPEED] >= 2000)
			skew_delay_sel = 2;
		else
			skew_delay_sel = 3;
	}

	writel(0x00000001, regs + 0x0500); /* SC_GNR_CON0 */
	writel(0x00001450, regs + 0x0504); /* SC_GNR_CON1 */
	writel(0x00000004, regs + 0x0508); /* SC_ANA_CON0 */
	writel(0x00009000, regs + 0x050c); /* SC_ANA_CON1 */
	writel(0x00000005, regs + 0x0510); /* SC_ANA_CON2 */
	writel(0x00000600, regs + 0x0514); /* SC_ANA_CON3 */
	writel(0x00000301, regs + 0x0530); /* SC_TIME_CON0 */

	for (i = 0; i <= cfg[LANES]; i++) {
		writel(0x00000001, regs + 0x0000 + (i * 0x100)); /* SD_GNR_CON0 */
		writel(0x00001450, regs + 0x0004 + (i * 0x100)); /* SD_GNR_CON1 */
		writel(0x00000004, regs + 0x0008 + (i * 0x100)); /* SD_ANA_CON0 */
		writel(0x00009000, regs + 0x000c + (i * 0x100)); /* SD_ANA_CON1 */
		writel(0x00000005, regs + 0x0010 + (i * 0x100)); /* SD_ANA_CON2 */
		update_bits(regs + 0x0010 + (i * 0x100), 8, 2, skew_delay_sel); /* SD_ANA_CON2 */
		update_bits(regs + 0x0010 + (i * 0x100), 15, 1, 1); /* RESETN_CFG_SEL */
		update_bits(regs + 0x0010 + (i * 0x100), 7, 1, 1); /* RXDDRCLKHS_SEL */
		writel(0x00000600, regs + 0x0014 + (i * 0x100)); /* SD_ANA_CON3 */
		update_bits(regs + 0x0030 + (i * 0x100), 0, 8, cfg[SETTLE]); /* SD_TIME_CON0 */
		update_bits(regs + 0x0030 + (i * 0x100), 8, 1, settle_clk_sel); /* SD_TIME_CON0 */
		writel(0x00000003, regs + 0x0034 + (i * 0x100)); /* SD_TIME_CON1 */
		update_bits(regs + 0x0040 + (i * 0x100), 0, 1, skew_cal_en); /* SD_DESKEW_CON0 */
		writel(0x0000081a, regs + 0x0050 + (i * 0x100)); /* SD_DESKEW_CON4 */
	}

	return 0;
}

static int __set_phy_cfg_0502_0002_dphy(void __iomem *regs, int option, u32 *cfg)
{
	int i;
	u32 settle_clk_sel = 1;
	u32 skew_delay_sel = 0;
	u32 type = cfg[TYPE] & 0xffff;
	u32 t_clk_miss = 3;
	u32 freq_s_xi_c = 26; /* MHz */
	u32 clk_div1234_mc;
	u32 skew_cal_en = 0;

	if (cfg[SPEED] >= PHY_REF_SPEED) {
		settle_clk_sel = 0;
		skew_cal_en = 1;
	}

	if (cfg[SPEED] >= PHY_REF_SPEED && cfg[SPEED] < 4000) {
		if (cfg[SPEED] >= 3000)
			skew_delay_sel = 1;
		else if (cfg[SPEED] >= 2000)
			skew_delay_sel = 2;
		else
			skew_delay_sel = 3;
	}

	writel(0x00000001, regs + 0x0000); /* SC_GNR_CON0 */
	writel(0x00001450, regs + 0x0004); /* SC_GNR_CON1 */
	if (cfg[SPEED] > 4500)
		writel(0x00000000, regs + 0x0008); /* SC_ANA_CON0 */
	else
		writel(0x00000004, regs + 0x0008); /* SC_ANA_CON0 */
	if (cfg[SPEED] > 4500)
		writel(0x00008000, regs + 0x000c); /* SC_ANA_CON1 */
	else
		writel(0x00009000, regs + 0x000c); /* SC_ANA_CON1 */
	writel(0x00000005, regs + 0x0010); /* SC_ANA_CON2 */
	clk_div1234_mc = max(0, (int)(5 - DIV_ROUND_UP(((t_clk_miss - 1) * cfg[SPEED]) >> 2, freq_s_xi_c)));
	update_bits(regs + 0x0010, 8, 2, clk_div1234_mc); /* SC_ANA_CON2 */
	writel(0x00000600, regs + 0x0014); /* SC_ANA_CON3 */
	writel(0x00000301, regs + 0x0030); /* SC_TIME_CON0 */

	for (i = 0; i <= cfg[LANES]; i++) {
		writel(0x00000001, regs + 0x0100 + (i * 0x100)); /* SD_GNR_CON0 */
		writel(0x00001450, regs + 0x0104 + (i * 0x100)); /* SD_GNR_CON1 */
		if (cfg[SPEED] > 4500)
			writel(0x00000000, regs + 0x0108 + (i * 0x100)); /* SD_ANA_CON0 */
		else
			writel(0x00000004, regs + 0x0108 + (i * 0x100)); /* SD_ANA_CON0 */
		if (cfg[SPEED] > 4500)
			writel(0x00008260, regs + 0x010c + (i * 0x100)); /* SD_ANA_CON1 */
		else if (cfg[SPEED] == 4500)
			writel(0x00009260, regs + 0x010c + (i * 0x100)); /* SD_ANA_CON1 */
		else
			writel(0x00009000, regs + 0x010c + (i * 0x100)); /* SD_ANA_CON1 */
		writel(0x00000005, regs + 0x0110 + (i * 0x100)); /* SD_ANA_CON2 */
		update_bits(regs + 0x0110 + (i * 0x100), 8, 2, skew_delay_sel); /* SD_ANA_CON2 */
		writel(0x00000600, regs + 0x0114 + (i * 0x100)); /* SD_ANA_CON3 */
		/* DC Combo lane has below SFR (0/1/2) */
		if ((type == 0xDC) && (i < 3))
			writel(0x00000040, regs + 0x0124 + (i * 0x100)); /* SD_ANA_CON7 */
		update_bits(regs + 0x0130 + (i * 0x100), 0, 8, cfg[SETTLE]); /* SD_TIME_CON0 */
		update_bits(regs + 0x0130 + (i * 0x100), 8, 1, settle_clk_sel); /* SD_TIME_CON0 */
		writel(0x00000003, regs + 0x0134 + (i * 0x100)); /* SD_TIME_CON1 */
		update_bits(regs + 0x0140 + (i * 0x100), 0, 1, skew_cal_en); /* SD_DESKEW_CON0 */
		writel(0x0000081a, regs + 0x0150 + (i * 0x100)); /* SD_DESKEW_CON4 */
	}

	return 0;
}

static int __set_phy_cfg_0502_0003_dphy(void __iomem *regs, int option, u32 *cfg)
{
	int i;
	u32 settle_clk_sel = 1;
	u32 skew_delay_sel = 0;
	u32 t_clk_miss = 3;
	u32 freq_s_xi_c = 26; /* MHz */
	u32 clk_div1234_mc;
	u32 skew_cal_en = 0;

	if (cfg[SPEED] >= PHY_REF_SPEED) {
		settle_clk_sel = 0;
		skew_cal_en = 1;
	}

	if (cfg[SPEED] >= PHY_REF_SPEED && cfg[SPEED] < 4000) {
		if (cfg[SPEED] >= 3000)
			skew_delay_sel = 1;
		else if (cfg[SPEED] >= 2000)
			skew_delay_sel = 2;
		else
			skew_delay_sel = 3;
	}

	writel(0x00000001, regs + 0x0500); /* SC_GNR_CON0 */
	writel(0x00001450, regs + 0x0504); /* SC_GNR_CON1 */
	if (cfg[SPEED] > 4500)
		writel(0x00000000, regs + 0x0508); /* SC_ANA_CON0 */
	else
		writel(0x00000004, regs + 0x0508); /* SC_ANA_CON0 */
	if (cfg[SPEED] > 4500)
		writel(0x00008000, regs + 0x050c); /* SC_ANA_CON1 */
	else
		writel(0x00009000, regs + 0x050c); /* SC_ANA_CON1 */
	writel(0x00000005, regs + 0x0510); /* SC_ANA_CON2 */
	clk_div1234_mc = max(0, (int)(5 - DIV_ROUND_UP(((t_clk_miss - 1) * cfg[SPEED]) >> 2, freq_s_xi_c)));
	update_bits(regs + 0x0510, 8, 2, clk_div1234_mc); /* SC_ANA_CON2 */
	writel(0x00000600, regs + 0x0514); /* SC_ANA_CON3 */
	writel(0x00000301, regs + 0x0530); /* SC_TIME_CON0 */

	for (i = 0; i <= cfg[LANES]; i++) {
		writel(0x00000001, regs + 0x0000 + (i * 0x100)); /* SD_GNR_CON0 */
		writel(0x00001450, regs + 0x0004 + (i * 0x100)); /* SD_GNR_CON1 */
		if (cfg[SPEED] > 4500)
			writel(0x00000000, regs + 0x0008 + (i * 0x100)); /* SD_ANA_CON0 */
		else
			writel(0x00000004, regs + 0x0008 + (i * 0x100)); /* SD_ANA_CON0 */
		if (cfg[SPEED] > 4500)
			writel(0x00008260, regs + 0x000c + (i * 0x100)); /* SD_ANA_CON1 */
		else if (cfg[SPEED] == 4500)
			writel(0x00009260, regs + 0x000c + (i * 0x100)); /* SD_ANA_CON1 */
		else
			writel(0x00009000, regs + 0x000c + (i * 0x100)); /* SD_ANA_CON1 */
		writel(0x00000005, regs + 0x0010 + (i * 0x100)); /* SD_ANA_CON2 */
		update_bits(regs + 0x0010 + (i * 0x100), 8, 2, skew_delay_sel); /* SD_ANA_CON2 */
		update_bits(regs + 0x0010 + (i * 0x100), 15, 1, 1); /* RESETN_CFG_SEL */
		update_bits(regs + 0x0010 + (i * 0x100), 7, 1, 1); /* RXDDRCLKHS_SEL */
		writel(0x00000600, regs + 0x0014 + (i * 0x100)); /* SD_ANA_CON3 */
		update_bits(regs + 0x0030 + (i * 0x100), 0, 8, cfg[SETTLE]); /* SD_TIME_CON0 */
		update_bits(regs + 0x0030 + (i * 0x100), 8, 1, settle_clk_sel); /* SD_TIME_CON0 */
		writel(0x00000003, regs + 0x0034 + (i * 0x100)); /* SD_TIME_CON1 */
		update_bits(regs + 0x0040 + (i * 0x100), 0, 1, skew_cal_en); /* SD_DESKEW_CON0 */
		writel(0x0000081a, regs + 0x0050 + (i * 0x100)); /* SD_DESKEW_CON4 */
	}

	return 0;
}

static int __set_phy_cfg_0503_0000_dcphy(void __iomem *regs, int option, u32 *cfg)
{
	int i;
	u32 type = cfg[TYPE] & 0xffff;
	u32 mode = cfg[TYPE] >> 16;
	u32 settle_clk_sel = 1;
	u32 skew_delay_sel = 0;
	u32 skew_cal_en = 0;

	void __iomem *bias;

	/* phy disable for analog logic reset */
	if (mode == 0x000D) {
		writel(0x00000000, regs + 0x0000); /* SC_GNR_CON0, Phy clock enable */

		for (i = 0; i <= cfg[LANES]; i++)
			writel(0x00000000, regs + 0x0100 + (i * 0x100)); /* SD_GNR_CON0 , Phy data enable */
	} else {
		for (i = 0; i <= cfg[LANES]; i++)
			writel(0x00000000, regs + 0x0100 + (i * 0x100)); /* SD_GNR_CON0 , Phy data enable */
	}

	usleep_range(200, 201);

	bias = ioremap(0x170D1000, 0x1000);

	/* BIAS_SET */
	writel(0x00000010, bias + 0x0000); /* M_BIAS_CON0 */
	writel(0x00000110, bias + 0x0004); /* M_BIAS_CON1 */
	writel(0x00003223, bias + 0x0008); /* M_BIAS_CON2 */
	if (mode == 0x000C)
		writel(0x00000040, bias + 0x0010); /* M1_BIAS_CON4 */

	iounmap(bias);

	/* Dphy */
	if (mode == 0x000D) {
		if (cfg[SPEED] >= PHY_REF_SPEED) {
			settle_clk_sel = 0;
			skew_cal_en = 1;

			if (cfg[SPEED] >= 4000 && cfg[SPEED] <= 6500)
				skew_delay_sel = 0;
			else if (cfg[SPEED] >= 3000 && cfg[SPEED] < 4000)
				skew_delay_sel = 1;
			else if (cfg[SPEED] >= 2000 && cfg[SPEED] < 3000)
				skew_delay_sel = 2;
			else if (cfg[SPEED] >= 1500 && cfg[SPEED] < 2000)
				skew_delay_sel = 3;
			else
				skew_delay_sel = 0;
		}

		/* clock */
		writel(0x00001450, regs + 0x0004); /* SC_GNR_CON1 */
		writel(0x00000008, regs + 0x0008); /* SC_ANA_CON0 */
		writel(0x00000002, regs + 0x0010); /* SC_ANA_CON2 */
		writel(0x00008680, regs + 0x0014); /* SC_ANA_CON3 */
		writel(0x00004000, regs + 0x0018); /* SC_ANA_CON4 */
		writel(0x00000301, regs + 0x0030); /* SC_TIME_CON0 */
		/* Enable should be set at last. */
		writel(0x00000001, regs + 0x0000); /* SC_GNR_CON0, Phy clock enable */


		for (i = 0; i <= cfg[LANES]; i++) {
			writel(0x00001450, regs + 0x0104 + (i * 0x100)); /* SD_GNR_CON1 */
			if ((type == 0xDC) && (i < 3))
				writel(0x00000008, regs + 0x0108 + (i * 0x100)); /* SD_ANA_CON0 */
			writel(0x00000002, regs + 0x0110 + (i * 0x100)); /* SD_ANA_CON2 */
			update_bits(regs + 0x0010 + (i * 0x100), 8, 2, skew_delay_sel); /* SD_ANA_CON2 */
			writel(0x00008680, regs + 0x0114 + (i * 0x100)); /* SD_ANA_CON3 */
			writel(0x00004000, regs + 0x0118 + (i * 0x100)); /* SD_ANA_CON4 */
			if ((type == 0xDC) && (i < 3))
				writel(0x00000040, regs + 0x0124 + (i * 0x100)); /* SD_ANA_CON7 */
			update_bits(regs + 0x0130 + (i * 0x100), 0, 8, cfg[SETTLE]);    /* SD_TIME_CON0 */
			update_bits(regs + 0x0130 + (i * 0x100), 8, 1, settle_clk_sel); /* SD_TIME_CON0 */
			writel(0x00000003, regs + 0x0134 + (i * 0x100)); /* SD_TIME_CON1 */
			update_bits(regs + 0x0140 + (i * 0x100), 0, 1, skew_cal_en); /* SD_DESKEW_CON0 */
			writel(0x0000081A, regs + 0x0150 + (i * 0x100)); /* SD_DESKEW_CON4 */
			/* Enable should be set at last. */
			writel(0x00000001, regs + 0x0100 + (i * 0x100)); /* SD_GNR_CON0 , Phy data enable */
		}
	} else { /* Cphy */
		if (cfg[SPEED] >= CPHY_REF_SPEED)
			settle_clk_sel = 0;

		for (i = 0; i <= cfg[LANES]; i++) {
			writel(0x00001450, regs + 0x0104 + (i * 0x100)); /* SD_GNR_CON1 */
			writel(0x00000009, regs + 0x0108 + (i * 0x100)); /* SD_ANA_CON0 */
			writel(0x000082C8, regs + 0x010C + (i * 0x100)); /* SD_ANA_CON1 */
			writel(0x00000001, regs + 0x0110 + (i * 0x100)); /* SD_ANA_CON2 */
			writel(0x00008680, regs + 0x0114 + (i * 0x100)); /* SD_ANA_CON3 */
			writel(0x00004000, regs + 0x0118 + (i * 0x100)); /* SD_ANA_CON4 */
			writel(0x00000200, regs + 0x011c + (i * 0x100)); /* SD_ANA_CON5 */
			writel(0x00000638, regs + 0x0120 + (i * 0x100)); /* SD_ANA_CON6 */
			writel(0x00000040, regs + 0x0124 + (i * 0x100)); /* SD_ANA_CON7 */
			update_bits(regs + 0x0130 + (i * 0x100), 0, 8, cfg[SETTLE]);    /* SD_TIME_CON0 */
			update_bits(regs + 0x0130 + (i * 0x100), 8, 1, settle_clk_sel); /* SD_TIME_CON0 */
			writel(0x00000032, regs + 0x0134 + (i * 0x100)); /* SD_TIME_CON1 */
			update_bits(regs + 0x0140 + (i * 0x100), 0, 1, skew_cal_en); /* SD_DESKEW_CON0 */
			writel(0x00001503, regs + 0x0164 + (i * 0x100)); /* SD_CRC_CON1 */
			writel(0x00000033, regs + 0x0168 + (i * 0x100)); /* SD_CRC_CON2 */
			/* Enable should be set at last. */
			writel(0x00000001, regs + 0x0100 + (i * 0x100)); /* SD_GNR_CON0 , Phy data enable */
		}
	}

	usleep_range(200, 201);

	return 0;
}

static int __set_phy_cfg_0503_0004_dcphy(void __iomem *regs, int option, u32 *cfg)
{
	int i;
	u32 type = cfg[TYPE] & 0xffff;
	u32 mode = cfg[TYPE] >> 16;
	u32 settle_clk_sel = 1;
	u32 skew_delay_sel = 0;
	u32 skew_cal_en = 0;

	u32 t_clk_miss = 3;
	u32 freq_s_xi_c = 26; /* MHz */
	u32 clk_div1234_mc;

	void __iomem *bias;

	/* phy disable for analog logic reset */
	if (mode == 0x000D) {
		writel(0x00000000, regs + 0x0000); /* SC_GNR_CON0, Phy clock enable */

		for (i = 0; i <= cfg[LANES]; i++)
			writel(0x00000000, regs + 0x0100 + (i * 0x100)); /* SD_GNR_CON0 , Phy data enable */
	} else {
		for (i = 0; i <= cfg[LANES]; i++)
			writel(0x00000000, regs + 0x0100 + (i * 0x100)); /* SD_GNR_CON0 , Phy data enable */
	}

	usleep_range(200, 201);

	bias = ioremap(0x150D1000, 0x1000);

	/* BIAS_SET */
	writel(0x00000010, bias + 0x0000); /* M_BIAS_CON0 */
	writel(0x00000110, bias + 0x0004); /* M_BIAS_CON1 */
	if (mode == 0x000D)
		writel(0x00003223, bias + 0x0008); /* M_BIAS_CON2 */
	else {
		writel(0x00003213, bias + 0x0008); /* M_BIAS_CON2 */
		writel(0x00000040, bias + 0x0010); /* M1_BIAS_CON4 */
	}

	iounmap(bias);

	/* Dphy */
	if (mode == 0x000D) {
		if (cfg[SPEED] >= PHY_REF_SPEED) {
			settle_clk_sel = 0;
			skew_cal_en = 1;

			if (cfg[SPEED] >= 4000 && cfg[SPEED] <= 6500)
				skew_delay_sel = 0;
			else if (cfg[SPEED] >= 3000 && cfg[SPEED] < 4000)
				skew_delay_sel = 1;
			else if (cfg[SPEED] >= 2000 && cfg[SPEED] < 3000)
				skew_delay_sel = 2;
			else if (cfg[SPEED] >= 1500 && cfg[SPEED] < 2000)
				skew_delay_sel = 3;
			else
				skew_delay_sel = 0;
		}

		/* clock */
		writel(0x00001450, regs + 0x0004); /* SC_GNR_CON1 */
		if (cfg[SPEED] > 4500)
			writel(0x00000000, regs + 0x0008); /* SC_ANA_CON0 */
		else
			writel(0x00000004, regs + 0x0008); /* SC_ANA_CON0 */
		if (cfg[SPEED] > 4500)
			writel(0x00008000, regs + 0x000c); /* SC_ANA_CON1 */
		else
			writel(0x00009000, regs + 0x000c); /* SC_ANA_CON1 */
		writel(0x00000005, regs + 0x0010); /* SC_ANA_CON2 */
		clk_div1234_mc = max(0, (int)(5 - DIV_ROUND_UP(((t_clk_miss - 1) * cfg[SPEED]) >> 2, freq_s_xi_c)));
		update_bits(regs + 0x0010, 8, 2, clk_div1234_mc); /* SC_ANA_CON2 */
		writel(0x00000600, regs + 0x0014); /* SC_ANA_CON3 */
		writel(0x00000301, regs + 0x0030); /* SC_TIME_CON0 */
		/* Enable should be set at last. */
		writel(0x00000001, regs + 0x0000); /* SC_GNR_CON0, Phy clock enable */

		for (i = 0; i <= cfg[LANES]; i++) {
			writel(0x00001450, regs + 0x0104 + (i * 0x100)); /* SD_GNR_CON1 */
			if (cfg[SPEED] > 4500) {
				writel(0x00000000, regs + 0x0108 + (i * 0x100)); /* SD_ANA_CON0 */
				writel(0x00008000, regs + 0x010c + (i * 0x100)); /* SD_ANA_CON1 */
			} else {
				writel(0x00000004, regs + 0x0108 + (i * 0x100)); /* SD_ANA_CON0 */
				writel(0x00009000, regs + 0x010c + (i * 0x100)); /* SD_ANA_CON1 */
			}
			writel(0x00000005, regs + 0x0110 + (i * 0x100)); /* SD_ANA_CON2 */
			update_bits(regs + 0x0110 + (i * 0x100), 8, 2, skew_delay_sel); /* SD_ANA_CON2 */
			writel(0x00000600, regs + 0x0114 + (i * 0x100)); /* SD_ANA_CON3 */
			if ((type == 0xDC) && (i < 3))
				writel(0x00000040, regs + 0x0124 + (i * 0x100)); /* SD_ANA_CON7 */
			update_bits(regs + 0x0130 + (i * 0x100), 0, 8, cfg[SETTLE]); /* SD_TIME_CON0 */
			update_bits(regs + 0x0130 + (i * 0x100), 8, 1, settle_clk_sel); /* SD_TIME_CON0 */
			writel(0x00000003, regs + 0x0134 + (i * 0x100)); /* SD_TIME_CON1 */
			update_bits(regs + 0x0140 + (i * 0x100), 0, 1, skew_cal_en); /* SD_DESKEW_CON0 */
			writel(0x0000081a, regs + 0x0150 + (i * 0x100)); /* SD_DESKEW_CON4 */
			/* Enable should be set at last. */
			writel(0x00000001, regs + 0x0100 + (i * 0x100)); /* SD_GNR_CON0 , Phy data enable */
		}
	} else { /* Cphy */
		if (cfg[SPEED] >= CPHY_REF_SPEED)
			settle_clk_sel = 0;

		for (i = 0; i <= cfg[LANES]; i++) {
			writel(0x00001450, regs + 0x0104 + (i * 0x100)); /* SD_GNR_CON1 */
			writel(0x00000005, regs + 0x0108 + (i * 0x100)); /* SD_ANA_CON0 */
			writel(0x00008b48, regs + 0x010c + (i * 0x100)); /* SD_ANA_CON1 */
			writel(0x00000005, regs + 0x0110 + (i * 0x100)); /* SD_ANA_CON2 */
			writel(0x00008600, regs + 0x0114 + (i * 0x100)); /* SD_ANA_CON3 */
			writel(0x00000660, regs + 0x0120 + (i * 0x100)); /* SD_ANA_CON6 */
			writel(0x00000040, regs + 0x0124 + (i * 0x100)); /* SD_ANA_CON7 */
			update_bits(regs + 0x0130 + (i * 0x100), 0, 8, cfg[SETTLE]);    /* SD_TIME_CON0 */
			update_bits(regs + 0x0130 + (i * 0x100), 8, 1, settle_clk_sel); /* SD_TIME_CON0 */
			writel(0x00000032, regs + 0x0134 + (i * 0x100)); /* SD_TIME_CON1 */
			writel(0x00001583, regs + 0x0164 + (i * 0x100)); /* SD_CRC_CON1 */
			writel(0x00000010, regs + 0x0168 + (i * 0x100)); /* SD_CRC_CON2 */
			/* Enable should be set at last. */
			writel(0x00000001, regs + 0x0100 + (i * 0x100)); /* SD_GNR_CON0 , Phy data enable */
		}
	}

	usleep_range(200, 201);

	return 0;
}

static int __set_phy_cfg_0503_0005_dphy(void __iomem *regs, int option, u32 *cfg)
{
	int i;
	u32 mode = cfg[TYPE] >> 16;
	u32 settle_clk_sel = 1;
	u32 skew_delay_sel = 0;
	u32 skew_cal_en = 0;

	u32 t_clk_miss = 3;
	u32 freq_s_xi_c = 26; /* MHz */
	u32 clk_div1234_mc;

	void __iomem *bias;
	void __iomem *lane_regs;

	/* phy disable for analog logic reset */
	writel(0x00000000, regs + 0x0000); /* SC_GNR_CON0, Phy clock enable */

	for (i = 0; i <= cfg[LANES]; i++)
		writel(0x00000000, regs + 0x0100 + (i * 0x100)); /* SD_GNR_CON0 , Phy data enable */

	usleep_range(200, 201);

	bias = ioremap(0x150D1000, 0x1000);

	/* BIAS_SET */
	writel(0x00000010, bias + 0x0000); /* M_BIAS_CON0 */
	writel(0x00000110, bias + 0x0004); /* M_BIAS_CON1 */
	if (mode == 0x000D)
		writel(0x00003223, bias + 0x0008); /* M_BIAS_CON2 */
	else {
		writel(0x00003213, bias + 0x0008); /* M_BIAS_CON2 */
		writel(0x00000040, bias + 0x0010); /* M1_BIAS_CON4 */
	}

	iounmap(bias);

	if (cfg[SPEED] >= PHY_REF_SPEED) {
		settle_clk_sel = 0;
		skew_cal_en = 1;
		if (cfg[SPEED] >= 4000 && cfg[SPEED] <= 6500)
			skew_delay_sel = 0;
		else if (cfg[SPEED] >= 3000 && cfg[SPEED] < 4000)
			skew_delay_sel = 1;
		else if (cfg[SPEED] >= 2000 && cfg[SPEED] < 3000)
			skew_delay_sel = 2;
		else if (cfg[SPEED] >= 1500 && cfg[SPEED] < 2000)
			skew_delay_sel = 3;
		else
			skew_delay_sel = 0;
	}

	/* clock */
	writel(0x00000001, regs + 0x0000); /* SC_GNR_CON0, Phy clock enable */
	writel(0x00001450, regs + 0x0004); /* SC_GNR_CON1 */
	if (cfg[SPEED] > 4500)
		writel(0x00000000, regs + 0x0008); /* SC_ANA_CON0 */
	else
		writel(0x00000004, regs + 0x0008); /* SC_ANA_CON0 */
	if (cfg[SPEED] > 4500)
		writel(0x00008000, regs + 0x000c); /* SC_ANA_CON1 */
	else
		writel(0x00009000, regs + 0x000c); /* SC_ANA_CON1 */
	writel(0x00000005, regs + 0x0010); /* SC_ANA_CON2 */
	clk_div1234_mc = max(0, (int)(5 - DIV_ROUND_UP(((t_clk_miss - 1) * cfg[SPEED]) >> 2, freq_s_xi_c)));
	update_bits(regs + 0x0010, 8, 2, clk_div1234_mc); /* SC_ANA_CON2 */
	writel(0x00000600, regs + 0x0014); /* SC_ANA_CON3 */
	writel(0x00000301, regs + 0x0030); /* SC_TIME_CON0 */
	/* Enable should be set at last. */
	writel(0x00000001, regs + 0x0000); /* SC_GNR_CON0, Phy clock enable */

	lane_regs = ioremap(0x150D2E00, 0x200);

	for (i = 0; i <= cfg[LANES]; i++) {
		writel(0x00000001, lane_regs + 0x0000 + (i * 0x100)); /* SD_GNR_CON0 , Phy data enable */
		writel(0x00001450, lane_regs + 0x0004 + (i * 0x100)); /* SD_GNR_CON1 */
		if (cfg[SPEED] > 4500) {
			writel(0x00000000, lane_regs + 0x0008 + (i * 0x100)); /* SD_ANA_CON0 */
			writel(0x00008000, lane_regs + 0x000C + (i * 0x100)); /* SD_ANA_CON1 */
		} else {
			writel(0x00000004, lane_regs + 0x0008 + (i * 0x100)); /* SD_ANA_CON0 */
			writel(0x00009000, lane_regs + 0x000c + (i * 0x100)); /* SD_ANA_CON1 */
		}
		writel(0x00000005, lane_regs + 0x0010 + (i * 0x100)); /* SD_ANA_CON2 */
		update_bits(lane_regs + 0x0010 + (i * 0x100), 8, 2, skew_delay_sel); /* SD_ANA_CON2 */
		update_bits(lane_regs + 0x0010 + (i * 0x100), 15, 1, 1); /* RESETN_CFG_SEL */
		update_bits(lane_regs + 0x0010 + (i * 0x100), 7, 1, 1); /* RXDDRCLKHS_SEL */
		writel(0x00000600, lane_regs + 0x0014 + (i * 0x100)); /* SD_ANA_CON3 */
		update_bits(lane_regs + 0x0030 + (i * 0x100), 0, 8, cfg[SETTLE]); /* SD_TIME_CON0 */
		update_bits(lane_regs + 0x0030 + (i * 0x100), 8, 1, settle_clk_sel); /* SD_TIME_CON0 */
		writel(0x00000003, lane_regs + 0x0034 + (i * 0x100)); /* SD_TIME_CON1 */
		update_bits(lane_regs + 0x0040 + (i * 0x100), 0, 1, skew_cal_en); /* SD_DESKEW_CON0 */
		writel(0x0000081a, lane_regs + 0x0050 + (i * 0x100)); /* SD_DESKEW_CON4 */
		/* Enable should be set at last. */
		writel(0x00000001, lane_regs + 0x0000 + (i * 0x100)); /* SD_GNR_CON0 , Phy data enable */
	}
	iounmap(lane_regs);

	return 0;
}

static int __set_phy_cfg_0503_0008_dphy(void __iomem *regs, int option, u32 *cfg)
{
	int i;
	u32 settle_clk_sel = 1;
	u32 skew_delay_sel = 0;
	u32 skew_cal_en = 0;
	u32 hs_mode_sel = 1;

	if (cfg[SPEED] >= PHY_REF_SPEED) {
		settle_clk_sel = 0;
		skew_cal_en = 1;
		hs_mode_sel = 0;

		if (cfg[SPEED] >= 3000 && cfg[SPEED] <= 4500)
			skew_delay_sel = 1;
		else if (cfg[SPEED] >= 2000 && cfg[SPEED] < 3000)
			skew_delay_sel = 2;
		else if (cfg[SPEED] >= 1500 && cfg[SPEED] < 2000)
			skew_delay_sel = 3;
		else
			skew_delay_sel = 0;
	}

	/* clock */
	writel(0x00000002, regs + 0x1018); /* DPHY_ACTRL_SC_06 */

	/* data */
	for (i = 0; i <= cfg[LANES]; i++) {
		update_bits(regs + 0x143C + (i * 0x400), 5, 2, skew_delay_sel); /* DPHY_ACTRL_SD_08 */
		update_bits(regs + 0x14E0 + (i * 0x400), 0, 1, skew_cal_en); /* DPHY_DCTRL_SD_13 */

		update_bits(regs + 0x14AC + (i * 0x400), 2, 1, hs_mode_sel); /* DPHY_DCTRL_SD_05 */
		update_bits(regs + 0x14B0 + (i * 0x400), 0, 8, cfg[SETTLE]); /* DPHY_DCTRL_SD_06 */
	}

	return 0;
}

static int __set_phy_cfg_0504_0000_dcphy(void __iomem *regs, int option, u32 *cfg)
{
	int i;
	u32 type = cfg[TYPE] & 0xffff;
	u32 mode = cfg[TYPE] >> 16;
	u32 settle_clk_sel = 1;
	u32 skew_delay_sel = 0;
	u32 skew_cal_en = 0;

	void __iomem *bias;

	/* phy disable for analog logic reset */
	if (mode == 0x000D) {
		writel(0x00000000, regs + 0x0000); /* SC_GNR_CON0, Phy clock enable */

		for (i = 0; i <= cfg[LANES]; i++)
			writel(0x00000000, regs + 0x0100 + (i * 0x100)); /* SD_GNR_CON0 , Phy data enable */
	} else {
		for (i = 0; i <= cfg[LANES]; i++)
			writel(0x00000000, regs + 0x0100 + (i * 0x100)); /* SD_GNR_CON0 , Phy data enable */
	}

	usleep_range(200, 201);

	bias = ioremap(0x170A1000, 0x1000);

	/* BIAS_SET */
	writel(0x00000010, bias + 0x0000); /* M_BIAS_CON0 */
	writel(0x00000110, bias + 0x0004); /* M_BIAS_CON1 */
	writel(0x00003223, bias + 0x0008); /* M_BIAS_CON2 */

	if (mode == 0x000C)
		writel(0x00000040, bias + 0x0010); /* M1_BIAS_CON4 */

	iounmap(bias);

	/* Dphy */
	if (mode == 0x000D) {
		skew_cal_en = 1; /* In DPHY, skew cal enable regardless of mipi speed */
		if (cfg[SPEED] >= PHY_REF_SPEED) {
			settle_clk_sel = 0;

			if (cfg[SPEED] >= 4000 && cfg[SPEED] <= 6500)
				skew_delay_sel = 0;
			else if (cfg[SPEED] >= 3000 && cfg[SPEED] < 4000)
				skew_delay_sel = 1;
			else if (cfg[SPEED] >= 2000 && cfg[SPEED] < 3000)
				skew_delay_sel = 2;
			else if (cfg[SPEED] >= 1500 && cfg[SPEED] < 2000)
				skew_delay_sel = 3;
			else
				skew_delay_sel = 0;
		}

		/* clock */
		writel(0x00001450, regs + 0x0004); /* SC_GNR_CON1 */
		writel(0x00000001, regs + 0x0008); /* SC_ANA_CON0 */
		writel(0x0000EA40, regs + 0x000C); /* SC_ANA_CON1 */
		writel(0x00000002, regs + 0x0010); /* SC_ANA_CON2 */
		writel(0x00008600, regs + 0x0014); /* SC_ANA_CON3 */
		writel(0x00004000, regs + 0x0018); /* SC_ANA_CON4 */
		writel(0x00000301, regs + 0x0030); /* SC_TIME_CON0 */
		/* Enable should be set at last. */
		writel(0x00000001, regs + 0x0000); /* SC_GNR_CON0, Phy clock enable */

		for (i = 0; i <= cfg[LANES]; i++) {
			writel(0x00001450, regs + 0x0104 + (i * 0x100)); /* SD_GNR_CON1 */
			writel(0x00000001, regs + 0x0108 + (i * 0x100)); /* SD_ANA_CON0 */
			writel(0x0000EA40, regs + 0x010C + (i * 0x100)); /* SD_ANA_CON1 */

			writel(0x00000004, regs + 0x0110 + (i * 0x100)); /* SD_ANA_CON2 */
			update_bits(regs + 0x0110 + (i * 0x100), 8, 2, skew_delay_sel); /* SD_ANA_CON2 */

			writel(0x00008600, regs + 0x0114 + (i * 0x100)); /* SD_ANA_CON3 */
			writel(0x00004000, regs + 0x0118 + (i * 0x100)); /* SD_ANA_CON4 */

			if ((type == 0xDC) && (i < 3))
				writel(0x00000040, regs + 0x0124 + (i * 0x100)); /* SD_ANA_CON7 */

			update_bits(regs + 0x0130 + (i * 0x100), 0, 8, cfg[SETTLE]);    /* SD_TIME_CON0 */
			update_bits(regs + 0x0130 + (i * 0x100), 8, 1, settle_clk_sel); /* SD_TIME_CON0 */

			writel(0x00000003, regs + 0x0134 + (i * 0x100)); /* SD_TIME_CON1 */

			update_bits(regs + 0x0140 + (i * 0x100), 0, 1, skew_cal_en); /* SD_DESKEW_CON0 */

			writel(0x0000081A, regs + 0x0150 + (i * 0x100)); /* SD_DESKEW_CON4 */
			/* Enable should be set at last. */
			writel(0x00000001, regs + 0x0100 + (i * 0x100)); /* SD_GNR_CON0 , Phy data enable */
		}
	} else { /* Cphy */
		if (cfg[SPEED] >= CPHY_REF_SPEED)
			settle_clk_sel = 0;

		for (i = 0; i <= cfg[LANES]; i++) {
			writel(0x00001450, regs + 0x0104 + (i * 0x100)); /* SD_GNR_CON1 */
			writel(0x000000FB, regs + 0x0108 + (i * 0x100)); /* SD_ANA_CON0 */
			writel(0x0000B7F9, regs + 0x010C + (i * 0x100)); /* SD_ANA_CON1 */
			writel(0x00000005, regs + 0x0110 + (i * 0x100)); /* SD_ANA_CON2 */
			writel(0x00008600, regs + 0x0114 + (i * 0x100)); /* SD_ANA_CON3 */
			writel(0x00004000, regs + 0x0118 + (i * 0x100)); /* SD_ANA_CON4 */
			writel(0x00000200, regs + 0x011c + (i * 0x100)); /* SD_ANA_CON5 */
			writel(0x00000608, regs + 0x0120 + (i * 0x100)); /* SD_ANA_CON6 */
			writel(0x00000040, regs + 0x0124 + (i * 0x100)); /* SD_ANA_CON7 */

			update_bits(regs + 0x0130 + (i * 0x100), 0, 8, cfg[SETTLE]);    /* SD_TIME_CON0 */
			update_bits(regs + 0x0130 + (i * 0x100), 8, 1, settle_clk_sel); /* SD_TIME_CON0 */

			writel(0x00000034, regs + 0x0134 + (i * 0x100)); /* SD_TIME_CON1 */

			update_bits(regs + 0x0140 + (i * 0x100), 0, 1, skew_cal_en); /* SD_DESKEW_CON0 */

			writel(0x00001501, regs + 0x0164 + (i * 0x100)); /* SD_CRC_CON1 */
			writel(0x00000032, regs + 0x0168 + (i * 0x100)); /* SD_CRC_CON2 */
			/* Enable should be set at last. */
			writel(0x00000001, regs + 0x0100 + (i * 0x100)); /* SD_GNR_CON0 , Phy data enable */
		}
	}

	usleep_range(200, 201);

	return 0;
}

static int __set_phy_cfg_0504_0002_dcphy(void __iomem *regs, int option, u32 *cfg)
{
	int i;
	u32 type = cfg[TYPE] & 0xffff;
	u32 mode = cfg[TYPE] >> 16;
	u32 settle_clk_sel = 1;
	u32 skew_delay_sel = 0;
	u32 skew_cal_en = 0;

	void __iomem *bias;

	/* phy disable for analog logic reset */
	if (mode == 0x000D) {
		writel(0x00000000, regs + 0x0000); /* SC_GNR_CON0, Phy clock enable */

		for (i = 0; i <= cfg[LANES]; i++)
			writel(0x00000000, regs + 0x0100 + (i * 0x100)); /* SD_GNR_CON0 , Phy data enable */
	} else {
		for (i = 0; i <= cfg[LANES]; i++)
			writel(0x00000000, regs + 0x0100 + (i * 0x100)); /* SD_GNR_CON0 , Phy data enable */
	}

	usleep_range(200, 201);

	bias = ioremap(0x170A1000, 0x1000);

	/* BIAS_SET */
	writel(0x00000010, bias + 0x0000); /* M_BIAS_CON0 */
	writel(0x00000110, bias + 0x0004); /* M_BIAS_CON1 */
	writel(0x00003223, bias + 0x0008); /* M_BIAS_CON2 */

	if (mode == 0x000C)
		writel(0x00000040, bias + 0x0010); /* M1_BIAS_CON4 */

	iounmap(bias);

	/* Dphy */
	if (mode == 0x000D) {
		skew_cal_en = 1; /* In DPHY, skew cal enable regardless of mipi speed */
		if (cfg[SPEED] >= PHY_REF_SPEED) {
			settle_clk_sel = 0;

			if (cfg[SPEED] >= 4000 && cfg[SPEED] <= 6500)
				skew_delay_sel = 0;
			else if (cfg[SPEED] >= 3000 && cfg[SPEED] < 4000)
				skew_delay_sel = 1;
			else if (cfg[SPEED] >= 2000 && cfg[SPEED] < 3000)
				skew_delay_sel = 2;
			else if (cfg[SPEED] >= 1500 && cfg[SPEED] < 2000)
				skew_delay_sel = 3;
			else
				skew_delay_sel = 0;
		}

		/* clock */
		writel(0x00001450, regs + 0x0004); /* SC_GNR_CON1 */
		writel(0x00000001, regs + 0x0008); /* SC_ANA_CON0 */
		writel(0x0000E800, regs + 0x000C); /* SC_ANA_CON1 */
		writel(0x00000004, regs + 0x0010); /* SC_ANA_CON2 */
		writel(0x00008600, regs + 0x0014); /* SC_ANA_CON3 */
		writel(0x00004000, regs + 0x0018); /* SC_ANA_CON4 */
		writel(0x00000301, regs + 0x0030); /* SC_TIME_CON0 */
		/* Enable should be set at last. */
		writel(0x00000001, regs + 0x0000); /* SC_GNR_CON0, Phy clock enable */

		for (i = 0; i <= cfg[LANES]; i++) {
			writel(0x00001450, regs + 0x0104 + (i * 0x100)); /* SD_GNR_CON1 */
			writel(0x00000001, regs + 0x0108 + (i * 0x100)); /* SD_ANA_CON0 */
			writel(0x0000EA40, regs + 0x010C + (i * 0x100)); /* SD_ANA_CON1 */

			writel(0x00000004, regs + 0x0110 + (i * 0x100)); /* SD_ANA_CON2 */
			update_bits(regs + 0x0110 + (i * 0x100), 8, 2, skew_delay_sel); /* SD_ANA_CON2 */

			writel(0x00008600, regs + 0x0114 + (i * 0x100)); /* SD_ANA_CON3 */
			writel(0x00004000, regs + 0x0118 + (i * 0x100)); /* SD_ANA_CON4 */

			if ((type == 0xDC) && (i < 3))
				writel(0x00000040, regs + 0x0124 + (i * 0x100)); /* SD_ANA_CON7 */

			update_bits(regs + 0x0130 + (i * 0x100), 0, 8, cfg[SETTLE]);    /* SD_TIME_CON0 */
			update_bits(regs + 0x0130 + (i * 0x100), 8, 1, settle_clk_sel); /* SD_TIME_CON0 */

			writel(0x00000003, regs + 0x0134 + (i * 0x100)); /* SD_TIME_CON1 */

			update_bits(regs + 0x0140 + (i * 0x100), 0, 1, skew_cal_en); /* SD_DESKEW_CON0 */

			writel(0x0000081A, regs + 0x0150 + (i * 0x100)); /* SD_DESKEW_CON4 */
			/* Enable should be set at last. */
			writel(0x00000001, regs + 0x0100 + (i * 0x100)); /* SD_GNR_CON0 , Phy data enable */
		}
	} else { /* Cphy */
		if (cfg[SPEED] >= CPHY_REF_SPEED)
			settle_clk_sel = 0;

		for (i = 0; i <= cfg[LANES]; i++) {
			writel(0x00001450, regs + 0x0104 + (i * 0x100)); /* SD_GNR_CON1 */
			writel(0x000000FB, regs + 0x0108 + (i * 0x100)); /* SD_ANA_CON0 */
			writel(0x0000B640, regs + 0x010C + (i * 0x100)); /* SD_ANA_CON1 */
			writel(0x00000005, regs + 0x0110 + (i * 0x100)); /* SD_ANA_CON2 */
			writel(0x00008600, regs + 0x0114 + (i * 0x100)); /* SD_ANA_CON3 */
			writel(0x00004000, regs + 0x0118 + (i * 0x100)); /* SD_ANA_CON4 */
			writel(0x00000200, regs + 0x011c + (i * 0x100)); /* SD_ANA_CON5 */
			writel(0x00000638, regs + 0x0120 + (i * 0x100)); /* SD_ANA_CON6 */
			writel(0x00000040, regs + 0x0124 + (i * 0x100)); /* SD_ANA_CON7 */

			update_bits(regs + 0x0130 + (i * 0x100), 0, 8, cfg[SETTLE]);    /* SD_TIME_CON0 */
			update_bits(regs + 0x0130 + (i * 0x100), 8, 1, settle_clk_sel); /* SD_TIME_CON0 */

			writel(0x00000034, regs + 0x0134 + (i * 0x100)); /* SD_TIME_CON1 */

			update_bits(regs + 0x0140 + (i * 0x100), 0, 1, skew_cal_en); /* SD_DESKEW_CON0 */

			writel(0x00001501, regs + 0x0164 + (i * 0x100)); /* SD_CRC_CON1 */
			writel(0x00000033, regs + 0x0168 + (i * 0x100)); /* SD_CRC_CON2 */
			/* Enable should be set at last. */
			writel(0x00000001, regs + 0x0100 + (i * 0x100)); /* SD_GNR_CON0 , Phy data enable */
		}
	}

	usleep_range(200, 201);

	return 0;
}

/*
 * __set_phy_cfg_0504_0003_dphy is called when 2+2 lane is used
 */
static int __set_phy_cfg_0504_0003_dphy(void __iomem *regs, int option, u32 *cfg)
{
	int i;
	u32 settle_clk_sel = 1;
	u32 skew_delay_sel = 0;
	u32 skew_cal_en = 0;

	void __iomem *bias;
	void __iomem *data_lane;

	/* phy disable for analog logic reset */
	writel(0x00000000, regs + 0x0000); /* SC_GNR_CON0, Phy clock enable */

	for (i = 0; i <= cfg[LANES]; i++)
		writel(0x00000000, regs + 0x0100 + (i * 0x100)); /* SD_GNR_CON0 , Phy data enable */

	usleep_range(200, 201);

	bias = ioremap(0x170A1000, 0x1000);

	/* BIAS_SET */
	writel(0x00000010, bias + 0x0000); /* M_BIAS_CON0 */
	writel(0x00000110, bias + 0x0004); /* M_BIAS_CON1 */
	writel(0x00003223, bias + 0x0008); /* M_BIAS_CON2 */

	iounmap(bias);

	skew_cal_en = 1; /* In DPHY, skew cal enable regardless of mipi speed */
	if (cfg[SPEED] >= PHY_REF_SPEED) {
		settle_clk_sel = 0;

		if (cfg[SPEED] >= 4000 && cfg[SPEED] <= 6500)
			skew_delay_sel = 0;
		else if (cfg[SPEED] >= 3000 && cfg[SPEED] < 4000)
			skew_delay_sel = 1;
		else if (cfg[SPEED] >= 2000 && cfg[SPEED] < 3000)
			skew_delay_sel = 2;
		else if (cfg[SPEED] >= 1500 && cfg[SPEED] < 2000)
			skew_delay_sel = 3;
		else
			skew_delay_sel = 0;
	}

	/* clock */
	writel(0x00001450, regs + 0x0004); /* SC_GNR_CON1 */
	writel(0x00000001, regs + 0x0008); /* SC_ANA_CON0 */
	writel(0x0000E800, regs + 0x000C); /* SC_ANA_CON1 */
	writel(0x00000004, regs + 0x0010); /* SC_ANA_CON2 */
	writel(0x00008600, regs + 0x0014); /* SC_ANA_CON3 */
	writel(0x00004000, regs + 0x0018); /* SC_ANA_CON4 */
	writel(0x00000301, regs + 0x0030); /* SC_TIME_CON0 */
	/* Enable should be set at last. */
	writel(0x00000001, regs + 0x0000); /* SC_GNR_CON0, Phy clock enable */

	data_lane = ioremap(0x170A3500, 0x300);

	for (i = 0; i <= cfg[LANES]; i++) {
		writel(0x00001450, data_lane + 0x0104 + (i * 0x100)); /* SD_GNR_CON1 */
		writel(0x00000001, data_lane + 0x0108 + (i * 0x100)); /* SD_ANA_CON0 */
		writel(0x0000EA40, data_lane + 0x010C + (i * 0x100)); /* SD_ANA_CON1 */

		writel(0x00008084, data_lane + 0x0110 + (i * 0x100)); /* SD_ANA_CON2 */
		update_bits(data_lane + 0x0110 + (i * 0x100), 8, 2, skew_delay_sel); /* SD_ANA_CON2 */

		writel(0x00008600, data_lane + 0x0114 + (i * 0x100)); /* SD_ANA_CON3 */
		writel(0x00004000, data_lane + 0x0118 + (i * 0x100)); /* SD_ANA_CON4 */

		update_bits(data_lane + 0x0130 + (i * 0x100), 0, 8, cfg[SETTLE]);    /* SD_TIME_CON0 */
		update_bits(data_lane + 0x0130 + (i * 0x100), 8, 1, settle_clk_sel); /* SD_TIME_CON0 */

		writel(0x00000003, data_lane + 0x0134 + (i * 0x100)); /* SD_TIME_CON1 */

		update_bits(data_lane + 0x0140 + (i * 0x100), 0, 1, skew_cal_en); /* SD_DESKEW_CON0 */

		writel(0x0000081A, data_lane + 0x0150 + (i * 0x100)); /* SD_DESKEW_CON4 */
		/* Enable should be set at last. */
		writel(0x00000001, data_lane + 0x0100 + (i * 0x100)); /* SD_GNR_CON0 , Phy data enable */
	}

	iounmap(data_lane);

	usleep_range(200, 201);

	return 0;
}

static int __set_phy_cfg_0504_8xxx_cphy(void __iomem *regs, int option, u32 *cfg)
{
	int i;
	u32 mode = HIWORD(cfg[TYPE]);
	u32 settle_clk_sel = 1;
/*
 * cfg[VERSION]
 * [0]: Front, [1]: Wide, [2]: Ultra Wide, [3]: Tele1, [4]: Tele2 => sensor
 * [5]~[7] => model 0x1:O1s, 0x2:T2s, 0x4:P3s, 0x3:R9s
 */
	u8 sensor = cfg[VERSION] & 0x1F;
	u8 model = (cfg[VERSION] & 0xE0) >> 5;
	u32 ana_con0 = 0x000000FB, ana_con1 = 0x0000B7F9, ana_con2 = 0x00000005,
		crc_con1 = 0x00001501, crc_con2 = 0x00000032;

	void __iomem *bias;

	/* phy disable for analog logic reset */
	for (i = 0; i <= cfg[LANES]; i++)
		writel(0x00000000, regs + 0x0100 + (i * 0x100)); /* SD_GNR_CON0 , Phy data enable */

	usleep_range(200, 201);

	bias = ioremap(0x170A1000, 0x1000);

	/* BIAS_SET */
	writel(0x00000010, bias + 0x0000); /* M_BIAS_CON0 */
	writel(0x00000110, bias + 0x0004); /* M_BIAS_CON1 */
	writel(0x00003223, bias + 0x0008); /* M_BIAS_CON2 */

	if (mode == 0x000C)
		writel(0x00000040, bias + 0x0010); /* M1_BIAS_CON4 */

	iounmap(bias);

	if (model == 0x1) {
		/* O1S */
		if (sensor & 0x2) {
			/* Wide */
			ana_con0 = 0x00000019;
			ana_con1 = 0x0000B3F9;
			ana_con2 = 0x00000004;
			crc_con1 = 0x00001533;
			crc_con2 = 0x00000022;
		} else if (sensor & 0x4) {
			/* Ultra Wide */
			ana_con0 = 0x00000019;
			ana_con1 = 0x0000B3F8;
			ana_con2 = 0x00000004;
			crc_con1 = 0x00001533;
			crc_con2 = 0x00000022;
		}
	} else if (model == 0x2) {
		/* T2S */
		if (sensor & 0x2) {
			/* Wide */
			ana_con0 = 0x00000019;
			ana_con1 = 0x0000B3F9;
			ana_con2 = 0x00000004;
			crc_con1 = 0x00001533;
			crc_con2 = 0x00000022;
		} else if (sensor & 0x4) {
			/* Ultra Wide */
			ana_con0 = 0x00000019;
			ana_con1 = 0x0000B3F8;
			ana_con2 = 0x00000004;
			crc_con1 = 0x00001533;
			crc_con2 = 0x00000022;
		}
	} else if (model == 0x4) {
		/* P3S */
		if (sensor & 0x2) {
			/* Wide */
			ana_con0 = 0x00000019;
			ana_con1 = 0x0000B3F9;
			ana_con2 = 0x00000004;
			crc_con1 = 0x00001533;
			crc_con2 = 0x00000022;
		}
	} else if (model == 0x3) {
		/* R9S */
		if (sensor & 0x2) {
			/* Wide */
			ana_con0 = 0x00000019;
			ana_con1 = 0x0000B3F9;
			ana_con2 = 0x00000004;
			crc_con1 = 0x00001533;
			crc_con2 = 0x00000022;
		}
	}

	printk("PHY : sensor=%d, model=%d, cfg[SPEED]=%d, cfg[SETTLE]=%d, ana_con0=%x, ana_con1=%x, ana_con2=%x, crc_con1=%x, crc_con2=%x",
			sensor, model, cfg[SPEED], cfg[SETTLE], ana_con0, ana_con1, ana_con2, crc_con1, crc_con2);

	if (cfg[SPEED] >= CPHY_REF_SPEED)
		settle_clk_sel = 0;

	for (i = 0; i <= cfg[LANES]; i++) {
		writel(0x00001450, regs + 0x0104 + (i * 0x100)); /* SD_GNR_CON1 */

		writel(ana_con0, regs + 0x0108 + (i * 0x100)); /* SD_ANA_CON0 */
		writel(ana_con1, regs + 0x010C + (i * 0x100)); /* SD_ANA_CON1 */

		writel(ana_con2, regs + 0x0110 + (i * 0x100)); /* SD_ANA_CON2 */
		writel(0x00008600, regs + 0x0114 + (i * 0x100)); /* SD_ANA_CON3 */
		writel(0x00004000, regs + 0x0118 + (i * 0x100)); /* SD_ANA_CON4 */
		writel(0x00000200, regs + 0x011c + (i * 0x100)); /* SD_ANA_CON5 */
		writel(0x00000608, regs + 0x0120 + (i * 0x100)); /* SD_ANA_CON6 */
		writel(0x00000040, regs + 0x0124 + (i * 0x100)); /* SD_ANA_CON7 */

		update_bits(regs + 0x0130 + (i * 0x100), 0, 8, cfg[SETTLE]);    /* SD_TIME_CON0 */
		update_bits(regs + 0x0130 + (i * 0x100), 8, 1, settle_clk_sel); /* SD_TIME_CON0 */

		writel(0x00000034, regs + 0x0134 + (i * 0x100)); /* SD_TIME_CON1 */

		update_bits(regs + 0x0140 + (i * 0x100), 0, 1, 0); /* SD_DESKEW_CON0 */

		writel(crc_con1, regs + 0x0164 + (i * 0x100)); /* SD_CRC_CON1 */
		writel(crc_con2, regs + 0x0168 + (i * 0x100)); /* SD_CRC_CON2 */
		/* Enable should be set at last. */
		writel(0x00000001, regs + 0x0100 + (i * 0x100)); /* SD_GNR_CON0 , Phy data enable */
	}

	usleep_range(200, 201);

	return 0;
}

static int __set_phy_cfg_0504_8xxx_dphy(void __iomem *regs, int option, u32 *cfg)
{
	int i;
	u32 type = LOWORD(cfg[TYPE]);
	u32 settle_clk_sel = 1;
	u32 skew_delay_sel = 0;
	u32 skew_cal_en = 0;
/*
 * cfg[VERSION]
 * [0]: Front, [1]: Wide, [2]: Ultra Wide, [3]: Tele1, [4]: Tele2 => sensor
 * [5]~[7] => model 0x1:O1s, 0x2:T2s, 0x4:P3s, 0x3:R9s
 */
	u8 sensor = cfg[VERSION] & 0x1F;
	u8 model = (cfg[VERSION] & 0xE0) >> 5;
	u32 sc_ana_con0 = 0x00000001, sd_ana_con0 = 0x00000001, 
		sc_ana_con1 = 0x0000EA40, sd_ana_con1 = 0x0000EA40, 
		sc_ana_con2 = 0x00000002, sd_ana_con2 = 0x00000004;

	void __iomem *bias;

	/* phy disable for analog logic reset */
	writel(0x00000000, regs + 0x0000); /* SC_GNR_CON0, Phy clock enable */

	/* phy disable for analog logic reset */
	for (i = 0; i <= cfg[LANES]; i++)
		writel(0x00000000, regs + 0x0100 + (i * 0x100)); /* SD_GNR_CON0 , Phy data enable */

	usleep_range(200, 201);

	bias = ioremap(0x170A1000, 0x1000);

	/* BIAS_SET */
	writel(0x00000010, bias + 0x0000); /* M_BIAS_CON0 */
	writel(0x00000110, bias + 0x0004); /* M_BIAS_CON1 */
	writel(0x00003223, bias + 0x0008); /* M_BIAS_CON2 */

	iounmap(bias);

	if (model == 0x1) {
		/* O1S */
		if (sensor & 0x1) {
			/* Front */
			sd_ana_con1 = 0x0000EAF0;
		}
	} else if (model == 0x3) {
		/* R9S */
		if (sensor & 0x1) {
			/* Front */
			sc_ana_con0 = 0x00000003; /* ana_con0(SC) */
			sd_ana_con0 = 0x00000003; /* ana_con0(SD/CSD) */
			sc_ana_con1 = 0x0000C8F0; /* ana_con1(SC) */
			sd_ana_con1 = 0x0000C8F0; /* ana_con1(SD/CSD) */
			sc_ana_con2 = 0x00000002; /* ana_con2(SC) */
			sd_ana_con2 = 0x00000002; /* ana_con2(SD/CSD) */
		}
	}

	printk("PHY : sensor=%d, model=%d, cfg[SPEED]=%d, cfg[SETTLE]=%d, sd_ana_con1=%x",
			sensor, model, cfg[SPEED], cfg[SETTLE], sd_ana_con1);

	/* Dphy */
	skew_cal_en = 1; /* In DPHY, skew cal enable regardless of mipi speed */
	if (cfg[SPEED] >= PHY_REF_SPEED) {
		settle_clk_sel = 0;

		if (cfg[SPEED] >= 4000 && cfg[SPEED] <= 6500)
			skew_delay_sel = 0;
		else if (cfg[SPEED] >= 3000 && cfg[SPEED] < 4000)
			skew_delay_sel = 1;
		else if (cfg[SPEED] >= 2000 && cfg[SPEED] < 3000)
			skew_delay_sel = 2;
		else if (cfg[SPEED] >= 1500 && cfg[SPEED] < 2000)
			skew_delay_sel = 3;
		else
			skew_delay_sel = 0;
	}

	/* clock */
	writel(0x00001450, regs + 0x0004); /* SC_GNR_CON1 */
	writel(sc_ana_con0, regs + 0x0008); /* SC_ANA_CON0 */
	writel(sc_ana_con1, regs + 0x000C); /* SC_ANA_CON1 */
	writel(sc_ana_con2, regs + 0x0010); /* SC_ANA_CON2 */
	writel(0x00008600, regs + 0x0014); /* SC_ANA_CON3 */
	writel(0x00004000, regs + 0x0018); /* SC_ANA_CON4 */
	writel(0x00000301, regs + 0x0030); /* SC_TIME_CON0 */
	/* Enable should be set at last. */
	writel(0x00000001, regs + 0x0000); /* SC_GNR_CON0, Phy clock enable */

	for (i = 0; i <= cfg[LANES]; i++) {
		writel(0x00001450, regs + 0x0104 + (i * 0x100)); /* SD_GNR_CON1 */
		writel(sd_ana_con0, regs + 0x0108 + (i * 0x100)); /* SD_ANA_CON0 */
		writel(sd_ana_con1, regs + 0x010C + (i * 0x100)); /* SD_ANA_CON1 */

		writel(sd_ana_con2, regs + 0x0110 + (i * 0x100)); /* SD_ANA_CON2 */
		update_bits(regs + 0x0110 + (i * 0x100), 8, 2, skew_delay_sel); /* SD_ANA_CON2 */

		writel(0x00008600, regs + 0x0114 + (i * 0x100)); /* SD_ANA_CON3 */
		writel(0x00004000, regs + 0x0118 + (i * 0x100)); /* SD_ANA_CON4 */

		if ((type == 0xDC) && (i < 3))
			writel(0x00000040, regs + 0x0124 + (i * 0x100)); /* SD_ANA_CON7 */

		update_bits(regs + 0x0130 + (i * 0x100), 0, 8, cfg[SETTLE]);    /* SD_TIME_CON0 */
		update_bits(regs + 0x0130 + (i * 0x100), 8, 1, settle_clk_sel); /* SD_TIME_CON0 */

		writel(0x00000003, regs + 0x0134 + (i * 0x100)); /* SD_TIME_CON1 */

		update_bits(regs + 0x0140 + (i * 0x100), 0, 1, skew_cal_en); /* SD_DESKEW_CON0 */

		writel(0x0000081A, regs + 0x0150 + (i * 0x100)); /* SD_DESKEW_CON4 */
		/* Enable should be set at last. */
		writel(0x00000001, regs + 0x0100 + (i * 0x100)); /* SD_GNR_CON0 , Phy data enable */
	}

	usleep_range(200, 201);

	return 0;
}

static const struct exynos_mipi_phy_cfg phy_cfg_table[] = {
	{
		.major = 0x0501,
		.minor = 0x0000,
		.mode = 0xD,
		.set = __set_phy_cfg_0501_0000_dphy,
	},
	{
		.major = 0x0502,
		.minor = 0x0000,
		.mode = 0xD,
		.set = __set_phy_cfg_0502_0000_dphy,
	},
	{
		.major = 0x0502,
		.minor = 0x0001,
		.mode = 0xD,
		.set = __set_phy_cfg_0502_0001_dphy,
	},
	{
		.major = 0x0502,
		.minor = 0x0002,
		.mode = 0xD,
		.set = __set_phy_cfg_0502_0002_dphy,
	},
	{
		.major = 0x0502,
		.minor = 0x0003,
		.mode = 0xD,
		.set = __set_phy_cfg_0502_0003_dphy,
	},
	{
		/* type : Combo(DCphy) or Dphy, mode : Dphy */
		.major = 0x0503,
		.minor = 0x0000,
		.mode = 0xD,
		.set = __set_phy_cfg_0503_0000_dcphy,
	},
	{
		/* type : Combo(DCphy), mode : Cphy */
		.major = 0x0503,
		.minor = 0x0000,
		.mode = 0xC,
		.set = __set_phy_cfg_0503_0000_dcphy,
	},
	{
		.major = 0x0503,
		.minor = 0x0004,
		.mode = 0xD,
		.set = __set_phy_cfg_0503_0004_dcphy,
	},
	{
		.major = 0x0503,
		.minor = 0x0004,
		.mode = 0xC,
		.set = __set_phy_cfg_0503_0004_dcphy,
	},
	{
		.major = 0x0503,
		.minor = 0x0005,
		.mode = 0xD,
		.set = __set_phy_cfg_0503_0005_dphy,
	},
	{
		.major = 0x0503,
		.minor = 0x0008,
		.mode = 0xD,
		.set = __set_phy_cfg_0503_0008_dphy,
	},
	{
		/* type : Combo(DCphy) or Dphy, mode : Dphy */
		.major = 0x0504,
		.minor = 0x0000,
		.mode = 0xD,
		.set = __set_phy_cfg_0504_0000_dcphy,
	},
	{
		/* type : Combo(DCphy), mode : Cphy */
		.major = 0x0504,
		.minor = 0x0000,
		.mode = 0xC,
		.set = __set_phy_cfg_0504_0000_dcphy,
	},
	{
		/* type : Combo(DCphy), mode : Cphy */
		.major = 0x0504,
		.minor = 0x0002,
		.mode = 0xC,
		.set = __set_phy_cfg_0504_0002_dcphy,
	},
	{
		/* type : Combo(DCphy), mode : Dphy */
		.major = 0x0504,
		.minor = 0x0002,
		.mode = 0xD,
		.set = __set_phy_cfg_0504_0002_dcphy,
	},
	{
		.major = 0x0504,
		.minor = 0x0003,
		.mode = 0xD,
		.set = __set_phy_cfg_0504_0003_dphy,
	},
	{
		/* type : Combo(DCphy), mode : Cphy */
		.major = 0x0504,
		.minor = 0x8000, /* MLB 1 : several setting version */
		.mode = 0xC,
		.set = __set_phy_cfg_0504_8xxx_cphy,
	},
	{
		/* type : Combo(DCphy), mode : Cphy */
		.major = 0x0504,
		.minor = 0x8000, /* MLB 1 : several setting version */
		.mode = 0xD,
		.set = __set_phy_cfg_0504_8xxx_dphy,
	},
	{ },
};

static int __set_phy_cfg(struct exynos_mipi_phy *state,
		struct mipi_phy_desc *phy_desc, int option, void *info)
{
	u32 *cfg = (u32 *)info;
	unsigned long i;
	int ret = -EINVAL;

	for (i = 0; i < ARRAY_SIZE(phy_cfg_table); i++) {
		if (HIWORD(cfg[VERSION]) != phy_cfg_table[i].major)
			continue;

		if (LOWORD(cfg[VERSION]) != phy_cfg_table[i].minor) {
			/* check MLB of minor for several phy setting */
			if (!(LOWORD(cfg[VERSION]) & 0x8000) ||
					!(phy_cfg_table[i].minor & 0x8000))
				continue;
		}

		if (HIWORD(cfg[TYPE]) != phy_cfg_table[i].mode)
			continue;

		/* reset assertion */
		ret = __set_phy_reset(state->reg_reset,
			phy_desc->rst_bit, 0);

		ret = phy_cfg_table[i].set(phy_desc->regs,
				option, cfg);

		/* reset release */
		ret = __set_phy_reset(state->reg_reset,
			phy_desc->rst_bit, 1);

		break;
	}

	return ret;
}

static struct exynos_mipi_phy_data mipi_phy_m4s4_top = {
	.flags = MIPI_PHY_MxSx_SHARED,
	.active_count = 0,
	.slock = __SPIN_LOCK_UNLOCKED(mipi_phy_m4s4_top.slock),
};

static struct exynos_mipi_phy_data mipi_phy_m4s4_mod = {
	.flags = MIPI_PHY_MxSx_SHARED,
	.active_count = 0,
	.slock = __SPIN_LOCK_UNLOCKED(mipi_phy_m4s4_mod.slock),
};

static struct exynos_mipi_phy_data mipi_phy_m4s4s4 = {
	.flags = MIPI_PHY_MxSx_SHARED,
	.active_count = 0,
	.slock = __SPIN_LOCK_UNLOCKED(mipi_phy_m4s4s4.slock),
};

static struct exynos_mipi_phy_data mipi_phy_m4s0 = {
	.flags = MIPI_PHY_MxSx_UNIQUE,
	.active_count = 0,
	.slock = __SPIN_LOCK_UNLOCKED(mipi_phy_m4s0.slock),
};

static struct exynos_mipi_phy_data mipi_phy_m2s4s4s2 = {
	.flags = MIPI_PHY_MxSx_SHARED,
	.active_count = 0,
	.slock = __SPIN_LOCK_UNLOCKED(mipi_phy_m2s4s4s2.slock),
};

static struct exynos_mipi_phy_data mipi_phy_m1s2s2 = {
	.flags = MIPI_PHY_MxSx_SHARED,
	.active_count = 0,
	.slock = __SPIN_LOCK_UNLOCKED(mipi_phy_m1s2s2.slock),
};

static struct exynos_mipi_phy_data mipi_phy_m0s4s4s4_mod = {
	.flags = MIPI_PHY_MxSx_SHARED,
	.active_count = 0,
	.slock = __SPIN_LOCK_UNLOCKED(mipi_phy_m0s4s4s4.slock),
};

static struct exynos_mipi_phy_data mipi_phy_m0s4s4s4s4s4s2 = {
	.flags = MIPI_PHY_MxSx_SHARED,
	.active_count = 0,
	.slock = __SPIN_LOCK_UNLOCKED(mipi_phy_m0s4s4s4s4s4s2.slock),
};

static struct exynos_mipi_phy_data mipi_phy_m0s4s4s4s4s2 = {
	.flags = MIPI_PHY_MxSx_SHARED,
	.active_count = 0,
	.slock = __SPIN_LOCK_UNLOCKED(mipi_phy_m0s4s4s4s4s2.slock),
};

static struct exynos_mipi_phy_data mipi_phy_m0s4s4s2 = {
	.flags = MIPI_PHY_MxSx_SHARED,
	.active_count = 0,
	.slock = __SPIN_LOCK_UNLOCKED(mipi_phy_m0s4s4s2.slock),
};

static const struct of_device_id exynos_mipi_phy_of_table[] = {
	{
		.compatible = "samsung,mipi-phy-m4s4-top",
		.data = &mipi_phy_m4s4_top,
	},
	{
		.compatible = "samsung,mipi-phy-m4s4-mod",
		.data = &mipi_phy_m4s4_mod,
	},
	{
		.compatible = "samsung,mipi-phy-m4s4s4",
		.data = &mipi_phy_m4s4s4,
	},
	{
		.compatible = "samsung,mipi-phy-m4s0",
		.data = &mipi_phy_m4s0,
	},
	{
		.compatible = "samsung,mipi-phy-m2s4s4s2",
		.data = &mipi_phy_m2s4s4s2,
	},
	{
		.compatible = "samsung,mipi-phy-m1s2s2",
		.data = &mipi_phy_m1s2s2,
	},
	{
		.compatible = "samsung,mipi-phy-m0s4s4s4-mod",
		.data = &mipi_phy_m0s4s4s4_mod,
	},
	{
		.compatible = "samsung,mipi-phy-m0s4s4s4s4s4s2",
		.data = &mipi_phy_m0s4s4s4s4s4s2,
	},
	{
		.compatible = "samsung,mipi-phy-m0s4s4s4s4s2",
		.data = &mipi_phy_m0s4s4s4s4s2,
	},
	{
		.compatible = "samsung,mipi-phy-m0s4s4s2",
		.data = &mipi_phy_m0s4s4s2,
	},
	{ },
};
MODULE_DEVICE_TABLE(of, exynos_mipi_phy_of_table);

#define to_mipi_video_phy(desc) \
	container_of((desc), struct exynos_mipi_phy, phys[(desc)->index])

static int exynos_mipi_phy_init(struct phy *phy)
{
	struct mipi_phy_desc *phy_desc = phy_get_drvdata(phy);
	struct exynos_mipi_phy *state = to_mipi_video_phy(phy_desc);

	return __set_phy_init(state, phy_desc, 1);
}

static int exynos_mipi_phy_power_on(struct phy *phy)
{
	struct mipi_phy_desc *phy_desc = phy_get_drvdata(phy);
	struct exynos_mipi_phy *state = to_mipi_video_phy(phy_desc);

	__set_phy_ldo(state, 1);

	return __set_phy_state(state, phy_desc, 1);
}

static int exynos_mipi_phy_power_off(struct phy *phy)
{
	int ret;
	struct mipi_phy_desc *phy_desc = phy_get_drvdata(phy);
	struct exynos_mipi_phy *state = to_mipi_video_phy(phy_desc);

	ret =  __set_phy_state(state, phy_desc, 0);
	__set_phy_ldo(state, 0);

	return ret;
}

#if defined(MODULE)
static int exynos_mipi_phy_configure(struct phy *phy, union phy_configure_opts *opts)
{
	struct mipi_phy_desc *phy_desc = phy_get_drvdata(phy);
	struct exynos_mipi_phy *state = to_mipi_video_phy(phy_desc);
	u32 cfg[5];

	cfg[VERSION] = opts->mipi_dphy.clk_miss;
	cfg[TYPE] = opts->mipi_dphy.clk_post;
	cfg[LANES] = opts->mipi_dphy.lanes;
	cfg[SPEED] = (u32)opts->mipi_dphy.hs_clk_rate;
	cfg[SETTLE] = opts->mipi_dphy.hs_settle;

	return __set_phy_cfg(state, phy_desc, 0, cfg);
}
#else
static int exynos_mipi_phy_set(struct phy *phy, int option, void *info)
{
	struct mipi_phy_desc *phy_desc = phy_get_drvdata(phy);
	struct exynos_mipi_phy *state = to_mipi_video_phy(phy_desc);

	return __set_phy_cfg(state, phy_desc, option, info);
}
#endif

static struct phy *exynos_mipi_phy_of_xlate(struct device *dev,
					struct of_phandle_args *args)
{
	struct exynos_mipi_phy *state = dev_get_drvdata(dev);

	if (WARN_ON(args->args[0] >= EXYNOS_MIPI_PHYS_NUM))
		return ERR_PTR(-ENODEV);

	return state->phys[args->args[0]].phy;
}

static struct phy_ops exynos_mipi_phy_ops = {
	.init		= exynos_mipi_phy_init,
	.power_on	= exynos_mipi_phy_power_on,
	.power_off	= exynos_mipi_phy_power_off,
#if defined(MODULE)
	.configure	= exynos_mipi_phy_configure,
#else
	.set		= exynos_mipi_phy_set,
#endif
	.owner		= THIS_MODULE,
};

static int exynos_mipi_phy_probe(struct platform_device *pdev)
{
	struct device *dev = &pdev->dev;
	struct device_node *node = dev->of_node;
	struct exynos_mipi_phy *state;
	struct phy_provider *phy_provider;
	struct exynos_mipi_phy_data *phy_data;
	const struct of_device_id *of_id;
	unsigned int iso[EXYNOS_MIPI_PHYS_NUM];
	unsigned int rst[EXYNOS_MIPI_PHYS_NUM];
	struct resource *res;
	unsigned int i;
	int ret = 0, elements = 0;
	char *str_regulator[EXYNOS_MIPI_PHY_MAX_REGULATORS];

	state = devm_kzalloc(dev, sizeof(*state), GFP_KERNEL);
	if (!state)
		return -ENOMEM;

	state->dev  = &pdev->dev;

	of_id = of_match_device(of_match_ptr(exynos_mipi_phy_of_table), dev);
	if (!of_id)
		return -EINVAL;

	phy_data = (struct exynos_mipi_phy_data *)of_id->data;

	dev_set_drvdata(dev, state);
	spin_lock_init(&state->slock);

	/* PMU isolation (optional) */
	state->reg_pmu = syscon_regmap_lookup_by_phandle(node,
						   "samsung,pmu-syscon");
	if (IS_ERR(state->reg_pmu)) {
		dev_err(dev, "failed to lookup PMU regmap, use PMU interface\n");
		state->reg_pmu = NULL;
	}

	elements = of_property_count_u32_elems(node, "isolation");
	if ((elements < 0) || (elements > EXYNOS_MIPI_PHYS_NUM))
		return -EINVAL;

	ret = of_property_read_u32_array(node, "isolation", iso,
					elements);
	if (ret) {
		dev_err(dev, "cannot get PHY isolation offset\n");
		return ret;
	}

	/* SYSREG reset (optional) */
	state->reg_reset = syscon_regmap_lookup_by_phandle(node,
						"samsung,reset-sysreg");
	if (IS_ERR(state->reg_reset)) {
		state->reg_reset = NULL;
	} else {
		ret = of_property_read_u32_array(node, "reset", rst, elements);
		if (ret) {
			dev_err(dev, "cannot get PHY reset bit\n");
			return ret;
		}
	}

	of_property_read_u32(node, "owner", &state->owner);

	for (i = 0; i < elements; i++) {
		state->phys[i].iso_offset = iso[i];
		state->phys[i].rst_bit	  = rst[i];
		dev_info(dev, "%s: isolation 0x%x\n", __func__,
				state->phys[i].iso_offset);
		if (state->reg_reset)
			dev_info(dev, "%s: reset %d\n", __func__,
				state->phys[i].rst_bit);

		res = platform_get_resource(pdev, IORESOURCE_MEM, i);
		if (res) {
			state->phys[i].regs = devm_ioremap_resource(dev, res);
			if (IS_ERR(state->phys[i].regs))
				return PTR_ERR(state->phys[i].regs);
		}
	}

	for (i = 0; i < elements; i++) {
		struct phy *generic_phy = devm_phy_create(dev, NULL,
				&exynos_mipi_phy_ops);
		if (IS_ERR(generic_phy)) {
			dev_err(dev, "failed to create PHY\n");
			return PTR_ERR(generic_phy);
		}

		state->phys[i].index	= i;
		state->phys[i].phy	= generic_phy;
		state->phys[i].data	= phy_data;
		phy_set_drvdata(generic_phy, &state->phys[i]);
	}

	/* parse phy regulator */
	for (i = 0; i < EXYNOS_MIPI_PHY_MAX_REGULATORS; i++) {
		state->regulators[i] = NULL;
		str_regulator[i] = NULL;
	}

	if(!of_property_read_string(node, "ldo0",
				(const char **)&str_regulator[0])) {
		state->regulators[0] = regulator_get(dev, str_regulator[0]);
		if (IS_ERR(state->regulators[0])) {
			dev_err(dev, "phy regulator 0.85V get failed\n");
			state->regulators[0] = NULL;
		}
	}

	if(!of_property_read_string(dev->of_node, "ldo1",
				(const char **)&str_regulator[1])) {
		state->regulators[1] = regulator_get(dev, str_regulator[1]);
		if (IS_ERR(state->regulators[1])) {
			dev_err(dev, "phy regulator 1.80V get failed\n");
			state->regulators[1] = NULL;
		}
	}

	phy_provider = devm_of_phy_provider_register(dev,
			exynos_mipi_phy_of_xlate);

	if (IS_ERR(phy_provider))
		dev_err(dev, "failed to create exynos mipi-phy\n");
	else
		dev_err(dev, "creating exynos-mipi-phy\n");

	return PTR_ERR_OR_ZERO(phy_provider);
}

static struct platform_driver exynos_mipi_phy_driver = {
	.probe	= exynos_mipi_phy_probe,
	.driver = {
		.name  = "exynos-mipi-phy-csi",
		.of_match_table = of_match_ptr(exynos_mipi_phy_of_table),
		.suppress_bind_attrs = true,
	}
};
module_platform_driver(exynos_mipi_phy_driver);

MODULE_DESCRIPTION("Samsung EXYNOS SoC MIPI CSI/DSI D/C-PHY driver");
MODULE_LICENSE("GPL v2");
