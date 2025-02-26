// SPDX-License-Identifier: GPL-2.0
/*
 * Samsung Exynos SoC series Pablo driver
 *
 * Copyright (c) 2019 Samsung Electronics Co., Ltd
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */
#include <linux/io.h>
#include <linux/fs.h>
#include <linux/file.h>
#include <linux/syscalls.h>
#include <linux/vmalloc.h>
#include <linux/firmware.h>
#include <linux/delay.h>
#include <soc/samsung/exynos-pmu-if.h>

#include "is-sfr-ois-mcu-v1_1_1.h"
#include "is-hw-api-ois-mcu.h"
#include "is-binary.h"

extern bool mcu_support_oldhw;

void __is_mcu_pmu_control(int on)
{
	if (on) {
		exynos_pmu_update(pmu_ois_mcu_regs[R_OIS_CPU_CONFIGURATION].sfr_offset,
			pmu_ois_mcu_masks[R_OIS_CPU_CONFIGURATION].sfr_offset, 0x1);
	} else {
		exynos_pmu_update(pmu_ois_mcu_regs[R_OIS_CPU_CONFIGURATION].sfr_offset,
			pmu_ois_mcu_masks[R_OIS_CPU_CONFIGURATION].sfr_offset, 0x0);
	}

	info_mcu("%s onoff = %d", __func__, on);
}

int __is_mcu_core_control(void __iomem *base, int on)
{
	u32 val;

	if (on)
		val = 0x1;
	else
		val = 0x0;

	is_hw_set_field(base, &ois_mcu_regs[R_OIS_CM0P_BOOT],
			&ois_mcu_fields[OIS_F_CM0P_BOOT_REQ], val);

	return 0;
}

void is_mcu_set_reg(void __iomem *base, int cmd, u8 val)
{
	is_hw_set_reg_u8(base, &ois_mcu_cmd_regs[cmd], val);
}

u8 is_mcu_get_reg(void __iomem *base, int cmd)
{
	u8 src = 0;

	src = is_hw_get_reg_u8(base, &ois_mcu_cmd_regs[cmd]);

	return src;

}

int __is_mcu_hw_enable(void __iomem *base)
{
	int ret = 0;

	info_mcu("%s started", __func__);

	is_hw_set_field(base, &ois_mcu_regs[R_OIS_CM0P_CTRL0],
		&ois_mcu_fields[OIS_F_CM0P_IPCLKREQ_ON], 1);
	is_hw_set_field(base, &ois_mcu_regs[R_OIS_CM0P_CTRL0],
		&ois_mcu_fields[OIS_F_CM0P_IPCLKREQ_ENABLE], 1);
	is_hw_set_field(base, &ois_mcu_regs[R_OIS_CM0P_CTRL0],
		&ois_mcu_fields[OIS_F_CM0P_SLEEP_CTRL], 0);
	is_hw_set_field(base, &ois_mcu_regs[R_OIS_CM0P_CTRL0],
		&ois_mcu_fields[OIS_F_CM0P_QACTIVE_DIRECT_CTRL], 1);
	is_hw_set_field(base, &ois_mcu_regs[R_OIS_CM0P_CTRL0],
		&ois_mcu_fields[OIS_F_CM0P_FORCE_DBG_PWRUP], 1);
	is_hw_set_field(base, &ois_mcu_regs[R_OIS_CM0P_CTRL0],
		&ois_mcu_fields[OIS_F_CM0P_DISABLE_IRQ], 0);
#if 0
	is_hw_set_field(base, &ois_mcu_regs[R_OIS_CM0P_CTRL0],
		&ois_mcu_fields[OIS_F_CM0P_WDTIRQ_TO_HOST], 1);
	is_hw_set_field(base, &ois_mcu_regs[R_OIS_CM0P_CTRL0],
		&ois_mcu_fields[OIS_F_CM0P_CONNECT_WDT_TO_NMI], 1);
#endif
	is_hw_set_field(base, &ois_mcu_regs[R_OIS_CM0P_REMAP_SPDMA_ADDR],
		&ois_mcu_fields[OIS_F_CM0P_SPDMA_ADDR], 0x107300A0);
	if (mcu_support_oldhw) {
		is_hw_set_field(base, &ois_mcu_regs[R_OIS_CM0P_REMAP_I2C_ADDR],
				&ois_mcu_fields[OIS_F_CM0P_I2C_ADDR], 0x109A0000);
	}

	info_mcu("%s end", __func__);

	return ret;
}

int __is_mcu_hw_set_clock_peri(void __iomem *base)
{
	int ret = 0;

	is_hw_set_reg(base, &ois_mcu_peri_regs[R_OIS_PERI_FS1], 0x01f0ff00);
	is_hw_set_reg(base, &ois_mcu_peri_regs[R_OIS_PERI_FS2], 0x7f0003e0);
	is_hw_set_reg(base, &ois_mcu_peri_regs[R_OIS_PERI_FS3], 0x001a0000);

	return ret;
}

int __is_mcu_hw_set_init_peri(void __iomem *base)
{
	int ret = 0;
	u32 recover_val = 0;
	u32 src = 0;

	if (mcu_support_oldhw)
		recover_val = 0x00003311;
	else
		recover_val = 0x00001122;

	is_hw_set_reg(base, &ois_mcu_peri_regs[R_OIS_PERI_CON_CTRL], recover_val);

	src = is_hw_get_reg(base, &ois_mcu_peri_regs[R_OIS_PERI_PUD_CTRL]);

	if (mcu_support_oldhw)
		recover_val = src & 0xFFFF00FF;
	else
		recover_val = src & 0xFFFFFF00;

	is_hw_set_reg(base, &ois_mcu_peri_regs[R_OIS_PERI_PUD_CTRL], recover_val);

	recover_val = 0x00002222;
	is_hw_set_reg(base, &ois_mcu_peri_regs[R_OIS_PERI2_CON_CTRL], recover_val);

	src = is_hw_get_reg(base, &ois_mcu_peri_regs[R_OIS_PERI2_PUD_CTRL]);
	recover_val = src & 0xFFFF0000;
	is_hw_set_reg(base, &ois_mcu_peri_regs[R_OIS_PERI2_PUD_CTRL], recover_val);

	return ret;
}

int __is_mcu_hw_set_clear_peri(void __iomem *base)
{
	int ret = 0;	
	u32 recover_val = 0;
	u32 src = 0;

	recover_val = 0x00000000;
	is_hw_set_reg(base, &ois_mcu_peri_regs[R_OIS_PERI_CON_CTRL], recover_val);

	src = is_hw_get_reg(base, &ois_mcu_peri_regs[R_OIS_PERI_PUD_CTRL]);

	if (mcu_support_oldhw)
		recover_val = src & 0xFFFF00FF;
	else
		recover_val = src & 0xFFFFFF00;

	is_hw_set_reg(base, &ois_mcu_peri_regs[R_OIS_PERI_PUD_CTRL], recover_val);

	recover_val = 0x00000000;
	is_hw_set_reg(base, &ois_mcu_peri_regs[R_OIS_PERI2_CON_CTRL], recover_val);

	src = is_hw_get_reg(base, &ois_mcu_peri_regs[R_OIS_PERI2_PUD_CTRL]);
	recover_val = src & 0xFFFF0000;
	is_hw_set_reg(base, &ois_mcu_peri_regs[R_OIS_PERI2_PUD_CTRL], recover_val);

	return ret;
}

int __is_mcu_hw_reset_peri(void __iomem *base, int onoff)
{
	int ret = 0;
	u8 val = 0;

	if (onoff)
		val = 0x1;
	else
		val = 0x0;

	is_hw_set_reg_u8(base, &ois_mcu_peri_regs[R_OIS_PERI_USI_CON], val);

	return ret;
}

int __is_mcu_hw_clear_peri(void __iomem *base)
{
	int ret = 0;

	is_hw_set_reg_u8(base, &ois_mcu_peri_regs[R_OIS_PERI_USI_CON_CLEAR], 0x05);

	return ret;
}

int __is_mcu_hw_disable(void __iomem *base)
{
	int ret = 0;

	info_mcu("%s started", __func__);

	is_hw_set_field(base, &ois_mcu_regs[R_OIS_CM0P_CTRL0],
		&ois_mcu_fields[OIS_F_CM0P_DISABLE_IRQ], 1);
	is_hw_set_field(base, &ois_mcu_regs[R_OIS_CM0P_CTRL0],
		&ois_mcu_fields[OIS_F_CM0P_IPCLKREQ_ON], 0);
	is_hw_set_field(base, &ois_mcu_regs[R_OIS_CM0P_CTRL0],
		&ois_mcu_fields[OIS_F_CM0P_IPCLKREQ_ENABLE], 1);
	is_hw_set_field(base, &ois_mcu_regs[R_OIS_CM0P_CTRL0],
		&ois_mcu_fields[OIS_F_CM0P_SLEEP_CTRL], 0);
	is_hw_set_field(base, &ois_mcu_regs[R_OIS_CM0P_CTRL0],
		&ois_mcu_fields[OIS_F_CM0P_QACTIVE_DIRECT_CTRL], 1);
	is_hw_set_field(base, &ois_mcu_regs[R_OIS_CM0P_CTRL0],
		&ois_mcu_fields[OIS_F_CM0P_FORCE_DBG_PWRUP], 0);
#if 0
	is_hw_set_field(base, &ois_mcu_regs[R_OIS_CM0P_CTRL0],
		&ois_mcu_fields[OIS_F_CM0P_WDTIRQ_TO_HOST], 0);
	is_hw_set_field(base, &ois_mcu_regs[R_OIS_CM0P_CTRL0],
		&ois_mcu_fields[OIS_F_CM0P_CONNECT_WDT_TO_NMI], 0);
#endif
	is_hw_set_field(base, &ois_mcu_regs[R_OIS_CM0P_REMAP_SPDMA_ADDR],
		&ois_mcu_fields[OIS_F_CM0P_SPDMA_ADDR], 0);
	if (mcu_support_oldhw) {
		is_hw_set_field(base, &ois_mcu_regs[R_OIS_CM0P_REMAP_I2C_ADDR],
				&ois_mcu_fields[OIS_F_CM0P_I2C_ADDR], 0);
	}

	usleep_range(1000, 1100);

	info_mcu("%s end", __func__);

	return ret;
}

long  __is_mcu_load_fw(void __iomem *base, struct device *dev)
{
	long ret = 0;
	struct is_binary mcu_bin;

	BUG_ON(!base);

	info_mcu("%s started", __func__);

	setup_binary_loader(&mcu_bin, 3, -EAGAIN, NULL, NULL);
	ret = request_binary(&mcu_bin, IS_MCU_PATH, IS_MCU_FW_NAME, dev);
	if (ret) {
		err_mcu("request_firmware was failed(%ld)\n", ret);
		ret = 0;
		goto request_err;
	}

	memcpy((void *)(base), (void *)mcu_bin.data, mcu_bin.size);
	info_mcu("Request FW was done (%s%s, %ld)\n",
		IS_MCU_PATH, IS_MCU_FW_NAME, mcu_bin.size);

	ret = mcu_bin.size;

request_err:
	release_binary(&mcu_bin);

	info_mcu("%s %d end", __func__, __LINE__);

	return ret;
}

unsigned int __is_mcu_get_sram_size(void)
{
	return 0xDFFF; /* fixed for v1.1.x */
}

unsigned int is_mcu_hw_g_irq_type(unsigned int state, enum mcu_event_type type)
{
	return state & (1 << type);
}

unsigned int is_mcu_hw_g_irq_state(void __iomem *base, bool clear)
{
	u32 src;

	src = is_hw_get_reg(base, &ois_mcu_regs[R_OIS_CM0P_IRQ_STATUS]);
	if (clear)
		is_hw_set_reg(base, &ois_mcu_regs[R_OIS_CM0P_IRQ_CLEAR], src);

	return src;

}

void __is_mcu_hw_s_irq_enable(void __iomem *base, u32 intr_enable)
{
	is_hw_set_reg(base, &ois_mcu_regs[R_OIS_CM0P_IRQ_ENABLE],
		(intr_enable & 0xF));
}

int __is_mcu_hw_sram_dump(void __iomem *base, unsigned int range)
{
	unsigned int i;
	u8 reg_value = 0;
	char *sram_info;

	sram_info = __getname();
	if (unlikely(!sram_info))
		return -ENOMEM;

	info_mcu("SRAM DUMP ++++ start (v1.1.0)\n");
	info_mcu("Kernel virtual for sram: %08lx\n", (ulong)base);
	snprintf(sram_info, PATH_MAX, "%04X:", 0);
	for (i = 0; i <= range; i++) {
		reg_value = *(u8 *)(base + i);
		if ((i > 0) && !(i % 0x10)) {
			info_mcu("%s\n", sram_info);
			snprintf(sram_info, PATH_MAX,
				"%04x: %02X", i, reg_value);
		} else {
			snprintf(sram_info + strlen(sram_info),
				PATH_MAX, " %02X", reg_value);
		}
	}
	info_mcu("%s\n", sram_info);

	__putname(sram_info);
	info_mcu("Kernel virtual for sram: %08lx\n",
		(ulong)(base + i - 1));
	info_mcu("SRAM DUMP --- end (v1.1.0)\n");

	return 0;
}

int __is_mcu_hw_sfr_dump(void __iomem *base, unsigned int range)
{
	unsigned int i;
	u8 reg_value = 0;
	char *sram_info;

	sram_info = __getname();
	if (unlikely(!sram_info))
		return -ENOMEM;

	info_mcu("SFR DUMP ++++ start (v1.1.0)\n");
	info_mcu("Kernel virtual for : %08lx\n", (ulong)base);
	snprintf(sram_info, PATH_MAX, "%04X:", 0);
	for (i = 0; i <= range; i++) {
		reg_value = *(u8 *)(base + i);
		if ((i > 0) && !(i%0x10)) {
			info_mcu("%s\n", sram_info);
			snprintf(sram_info, PATH_MAX,
				"%04x: %02X", i, reg_value);
		} else {
			snprintf(sram_info + strlen(sram_info),
				PATH_MAX, " %02X", reg_value);
		}
	}
	info_mcu("%s\n", sram_info);

	__putname(sram_info);
	info_mcu("Kernel virtual for : %08lx\n",
		(ulong)(base + i - 1));
	info_mcu("SFR DUMP --- end (v1.1.0)\n");

	return 0;
}

int __is_mcu_hw_cr_dump(void __iomem *base)
{
	info_mcu("SFR DUMP ++++ start (v1.1.0)\n");

	is_hw_dump_regs(base, ois_mcu_regs, R_OIS_REG_CNT);

	info_mcu("SFR DUMP --- end (v1.1.0)\n");

	return 0;
}

int __is_mcu_hw_peri1_dump(void __iomem *base)
{
	info_mcu("PERI1 SFR DUMP ++++ start (v1.1.0)\n");

	__is_mcu_hw_sfr_dump(base, 0xDFFF);

	info_mcu("PERI1 SFR DUMP --- end (v1.1.0)\n");
	return 0;
}

int __is_mcu_hw_peri2_dump(void __iomem *base)
{
	info_mcu("PERI2 SFR DUMP ++++ start (v1.1.0)\n");

	__is_mcu_hw_sfr_dump(base, 0xDFFF);

	info_mcu("PERI2 SFR DUMP --- end (v1.1.0)\n");
	return 0;
}

