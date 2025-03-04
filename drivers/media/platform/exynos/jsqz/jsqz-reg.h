/* linux/drivers/media/platform/exynos/mmsqz-regs.h
 *
 * Register definition file for Samsung JPEG Squeezer driver
 *
 * Copyright (c) 2018 Samsung Electronics Co., Ltd.
 *		http://www.samsung.com
 *
 * Author: Jungik Seo <jungik.seo@samsung.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#ifndef JSQZ_REGS_H_
#define JSQZ_REGS_H_
#include <linux/io.h>
#include <linux/delay.h>
#include <linux/module.h>
#include "jsqz-core.h"

// SFR
#define     REG_0_Y_ADDR                0x000
#define     REG_1_U_ADDR                0x004
#define     REG_2_V_ADDR                0x008
#define     REG_3_INPUT_SIZE            0x00C
#define     REG_4_INPUT_TYPE            0x010
#define     REG_5_OP_MODE               0x014
#define     REG_6_CONFIG_DC             0x018

#define     REG_7_TO_22_Y_Q_MAT         0x01C
#define     REG_23_TO_38_C_Q_MAT        0x05C

#define     REG_39_SW_RESET             0x09C
#define     REG_40_INTERRUPT_EN         0x0A0
#define     REG_41_INTERRUPT_CLEAR      0x0A4
#define     REG_42_MMSQZ_HW_START       0x0A8
#define     REG_43_MMSQZ_HW_DONE        0x0AC
#define     REG_44_CONFIG_STRIDE        0x0B0
#define     REG_45_CONFIG_TIMEOUT       0x0B4
#define     REG_46_HW_DEBUG             0x0B8
#define     REG_47_ERROR_FLAG           0x0BC

#define     REG_48_TO_63_INIT_Y_Q		0x0C0
#define     REG_64_TO_79_INIT_C_Q		0x100

#define     REG_80_VELOCITY             0x140
#define     REG_81_TUNE_DC              0x144
#define     REG_82_TUNE_ALPHA           0x148
#define     REG_83_TUNE_DQP             0x14C
#define     REG_84_FRM_AVG_DQP          0x150
#define     REG_85_DQP_ADDR             0x154

// define APIs
static inline void jsqz_sw_reset(void __iomem *base)
{
	writel(0x0, base + REG_39_SW_RESET);
	writel(0x1, base + REG_39_SW_RESET);
}

static inline void jsqz_interrupt_enable(void __iomem *base)
{
	writel(0x1, base + REG_40_INTERRUPT_EN);
	//writel(0x0, base + REG_41_INTERRUPT_CLEAR);
}

static inline void jsqz_interrupt_disable(void __iomem *base)
{
	writel(0x0, base + REG_40_INTERRUPT_EN);
}

static inline void jsqz_interrupt_clear(void __iomem *base)
{
	writel(0x1, base + REG_41_INTERRUPT_CLEAR);
}

static inline u32 jsqz_get_interrupt_status(void __iomem *base)
{
	return readl(base + REG_41_INTERRUPT_CLEAR);
}

static inline void jsqz_hw_start(void __iomem *base)
{
	writel(0x1, base + REG_42_MMSQZ_HW_START);
}

static inline u32 jsqz_check_done(void __iomem *base)
{
	return readl(base + REG_43_MMSQZ_HW_DONE) & 0x1;
}
static inline void jsqz_on_off_time_out(void __iomem *base, u32 time)
{
	u32 sfr = 0;
    if (time == 0)
        writel(0x0, base + REG_45_CONFIG_TIMEOUT);
    else {
        sfr = (time * 533) >> 7;
        writel((sfr | 0x00020000), base + REG_45_CONFIG_TIMEOUT);
    }
}

static inline void jsqz_set_stride_on_n_value(void __iomem *base, u32 value)
{
    if (value == 0)
        writel(0x0, base + REG_44_CONFIG_STRIDE);
    else
        writel((value | 0x00010000), base + REG_44_CONFIG_STRIDE);
}

static inline void jsqz_set_input_size(void __iomem *base, u32 size)
{
    writel(size, base + REG_3_INPUT_SIZE);
}

static inline void jsqz_set_input_configs(void __iomem *base, u32 type, u32 mode, u32 use_dc)
{
    writel(type, base + REG_4_INPUT_TYPE);
    writel(mode, base + REG_5_OP_MODE);
    writel(use_dc, base + REG_6_CONFIG_DC);
}

static inline void jsqz_set_input_addr_luma(void __iomem *base, dma_addr_t y_addr)
{
    writel(y_addr, base + REG_0_Y_ADDR);
}

static inline void jsqz_set_input_addr_chroma(void __iomem *base, dma_addr_t u_addr, dma_addr_t v_addr)
{
    writel(u_addr, base + REG_1_U_ADDR);
    writel(v_addr, base + REG_2_V_ADDR);
}

static inline void jsqz_set_input_qtbl(void __iomem *base, u32 * input_qt)
{
	int i;
    for (i = 0; i < 16; i++)
    {
        writel(input_qt[i], base + REG_48_TO_63_INIT_Y_Q + (i * 0x4));
        writel(input_qt[i+16], base + REG_64_TO_79_INIT_C_Q + (i * 0x4));
    }
}
static inline u32 jsqz_get_error_flags(void __iomem *base)
{
	return readl(base + REG_47_ERROR_FLAG);
}

static inline void jsqz_get_init_qtbl(void __iomem *base, u32 * init_qt)
{
	int i;
    for (i = 0; i < 16; i++)
    {
        init_qt[i] = readl(base + REG_48_TO_63_INIT_Y_Q + (i * 0x4));
        init_qt[i+16] = readl(base + REG_64_TO_79_INIT_C_Q + (i * 0x4));
    }
}

static inline void jsqz_get_output_regs(void __iomem *base, u32 * output_qt)
{
	int i;
    for (i = 0; i < 16; i++)
    {
        output_qt[i] = readl(base + REG_7_TO_22_Y_Q_MAT + (i * 0x4));
        output_qt[i+16] = readl(base + REG_23_TO_38_C_Q_MAT + (i * 0x4));
    }
}

static inline void jsqz_set_output_addr(void __iomem *base, dma_addr_t dqp_addr)
{
	writel(dqp_addr, base + REG_85_DQP_ADDR);
}

static inline void jsqz_set_velocity(void __iomem *base, u32 vel_xy)
{
    writel(vel_xy, base + REG_80_VELOCITY);
}

static inline void jsqz_set_tune_dc(void __iomem *base, u32 dc)
{
    writel(dc, base + REG_81_TUNE_DC);
}

static inline void jsqz_set_tune_alpha(void __iomem *base, u32 alpha)
{
    writel(alpha, base + REG_82_TUNE_ALPHA);
}

static inline void jsqz_set_tune_dqp(void __iomem *base, u32 dqp)
{
    writel(dqp, base + REG_83_TUNE_DQP);
}

static inline u32 jsqz_get_frame_dqp(void __iomem *base)
{
    return readl(base + REG_84_FRM_AVG_DQP);
}

static inline void jsqz_print_all_regs(struct jsqz_dev *jsqz)
{
	int i;
	void __iomem *base = jsqz->regs;
	dev_dbg(jsqz->dev, "%s: BEGIN\n", __func__);
	for (i = 0; i < 7; i++)
	{
		dev_dbg(jsqz->dev, "%s: 0x%08x : %08x\n", __func__, (i*0x4), readl(base + (i*0x4)));
	}
	for (i = 0; i < 7; i++)
	{
		dev_dbg(jsqz->dev, "%s: 0x%08x : %08x\n", __func__, (REG_39_SW_RESET + (i*0x4)), readl(base + REG_39_SW_RESET + (i*0x4)));
	}
	dev_dbg(jsqz->dev, "%s: 0x%08x : %08x\n", __func__, REG_47_ERROR_FLAG, readl(base + REG_47_ERROR_FLAG));
	for (i = 0; i < 6; i++)
	{
		dev_dbg(jsqz->dev, "%s: 0x%08x : %08x\n", __func__, (REG_80_VELOCITY + (i*0x4)), readl(base + REG_80_VELOCITY + (i*0x4)));
	}
	dev_dbg(jsqz->dev, "%s: END\n", __func__);
}


/*
static inline int get_hw_enc_status(void __iomem *base)
{
	unsigned int status = 0;

	status = readl(base + MMSQZ_ENC_STAT_REG) & (KBit0 | KBit1);
	return (status != 0 ? -1:0);
}
*/


#endif /* JSQZ_REGS_H_ */

