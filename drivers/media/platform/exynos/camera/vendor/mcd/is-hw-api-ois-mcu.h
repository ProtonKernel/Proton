/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Samsung Exynos SoC series Pablo driver
 *
 * Copyright (c) 2019 Samsung Electronics Co., Ltd
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#ifndef IS_HW_API_OIS_MCU_H
#define IS_HW_API_OIS_MCU_H

#include "is-hw-api-common.h"
#include "is-ois-mcu.h"

enum mcu_event_type {
	MCU_IRQ_WDT,
	MCU_IRQ_WDT_RST,
	MCU_IRQ_LOCKUP_RST,
	MCU_IRQ_SYS_RST,
	MCU_ERR,
};

/*
 * Configuation functions
 */
void __is_mcu_pmu_control(int on);
int __is_mcu_hw_enable(void __iomem *base);
int __is_mcu_hw_disable(void __iomem *base);
int __is_mcu_core_control(void __iomem *base, int on);
long __is_mcu_load_fw(void __iomem *base, struct device *dev);
unsigned int __is_mcu_get_sram_size(void);
int __is_mcu_hw_reset_peri(void __iomem *base, int onoff);
int __is_mcu_hw_set_clock_peri(void __iomem *base);
int __is_mcu_hw_set_init_peri(void __iomem *base);
int __is_mcu_hw_set_clear_peri(void __iomem *base);
int __is_mcu_hw_clear_peri(void __iomem *base);

/*
 * interrupt functions
 */
unsigned int is_mcu_hw_g_irq_state(void __iomem *base, bool clear);
void __is_mcu_hw_s_irq_enable(void __iomem *base, u32 intr_enable);
unsigned int is_mcu_hw_g_irq_type(unsigned int state, enum mcu_event_type type);

/*
 * debug functions
 */
int __is_mcu_hw_sram_dump(void __iomem *base, unsigned int range);
int __is_mcu_hw_cr_dump(void __iomem *base);
int __is_mcu_hw_peri1_dump(void __iomem *base);
int __is_mcu_hw_peri2_dump(void __iomem *base);

/*
* control function
*/
void is_mcu_set_reg(void __iomem *base, int cmd, u8 val);
u8 is_mcu_get_reg(void __iomem *base, int cmd);

#endif
