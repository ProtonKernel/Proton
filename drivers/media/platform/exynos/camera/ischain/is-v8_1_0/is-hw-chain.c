// SPDX-License-Identifier: GPL-2.0
/*
 * Samsung Exynos SoC series Pablo driver
 * Pablo v8.1 specific functions
 *
 * Copyright (c) 2019 Samsung Electronics Co., Ltd
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
#include "is-hw-settle-7nm-lpp.h"
#include "is-device-sensor.h"
#include "is-device-csi.h"
#include "is-device-ischain.h"

#include "../../interface/is-interface-ischain.h"
#include "../../hardware/is-hw-control.h"
#include "../../hardware/is-hw-mcscaler-v3.h"

/* SYSREG register description */
static struct is_reg sysreg_csis_regs[SYSREG_CSIS_REG_CNT] = {
	{0x0404, "C2_SWRESET"},
	{0x0408, "CSIS_PDP_SC_CON0"},
	{0x040c, "CSIS_PDP_SC_CON1"},
	{0x0410, "CSIS_PDP_SC_CON2"},
	{0x0414, "CSIS_PDP_SC_CON3"},
	{0x0418, "CSIS_PDP_SC_CON4"},
	{0x041c, "CSIS_PDP_SC_CON5"},
	{0x0420, "CSIS_PDP_SC_CON6"},
	{0x0424, "CSIS_PDP_SC_CON7"},
	{0x0440, "CSIS_FRAME_ID_EN"},
	{0x0448, "CSIS_PDP_SC_PDP_IN_EN"},
	{0x0450, "LH_GLUE_CON"},
	{0x0454, "LH_QACTIVE_CON"},
	{0x0500, "MIPI_PHY_CON"},
};

static struct is_reg sysreg_ipp_regs[SYSREG_IPP_REG_CNT] = {
	{0x0400, "IPP_USER_CON0"},
	{0x0404, "IPP_USER_CON1"},
	{0x0420, "LH_QACTIVE_CON"},
};

static struct is_reg sysreg_tnr_regs[SYSREG_TNR_REG_CNT] = {
	{0x0400, "SW_RESETn"},
	{0x0404, "LH_AST_GLUE_TYPE"},
	{0x0410, "LH_ENABLE"},
};

static struct is_reg sysreg_dns_regs[SYSREG_DNS_REG_CNT] = {
	{0x0400, "DNS_USER_CON0"},
	{0x0404, "DNS_USER_CON1"},
};

static struct is_reg sysreg_itp_regs[SYSREG_ITP_REG_CNT] = {
	{0x0400, "ITP_USER_CON"},
};

#if 0 /* not used */
static struct is_reg sysreg_mcsc_regs[SYSREG_MCSC_REG_CNT] = {
	{0x0400, "MCSC_USER_CON0"},
	{0x0404, "MCSC_USER_CON1"},
	{0x0408, "MCSC_USER_CON2"},
};

static struct is_reg sysreg_vra_regs[SYSREG_VRA_REG_CNT] = {
	{0x0400, "VRA_USER_CON0"},
};
#endif

static struct is_field sysreg_csis_fields[SYSREG_CSIS_REG_FIELD_CNT] = {
	{"C2_CSIS_SW_RESET", 0, 1, RW, 0x1},
	{"GLUEMUX_PDP0_VAL", 0, 2, RW, 0x0},
	{"GLUEMUX_PDP1_VAL", 0, 2, RW, 0x0},
	{"GLUEMUX_PDP2_VAL", 0, 2, RW, 0x0},
	{"GLUEMUX_CSIS_DMA0_OTF_SEL", 0, 4, RW, 0x0},
	{"GLUEMUX_CSIS_DMA1_OTF_SEL", 0, 4, RW, 0x0},
	{"GLUEMUX_CSIS_DMA2_OTF_SEL", 0, 4, RW, 0x0},
	{"GLUEMUX_CSIS_DMA3_OTF_SEL", 0, 4, RW, 0x0},
	{"GLUEMUX_CSIS_DMA4_OTF_SEL", 0, 4, RW, 0x0},
	{"FRAME_ID_EN_CSIS0", 0, 1, RW, 0x0},
	{"PDP2_IN_CSIS5_EN", 17, 1, RW, 0x0},
	{"PDP2_IN_CSIS4_EN", 16, 1, RW, 0x0},
	{"PDP2_IN_CSIS3_EN", 15, 1, RW, 0x0},
	{"PDP2_IN_CSIS2_EN", 14, 1, RW, 0x0},
	{"PDP2_IN_CSIS1_EN", 13, 1, RW, 0x0},
	{"PDP2_IN_CSIS0_EN", 12, 1, RW, 0x0},
	{"PDP1_IN_CSIS5_EN", 11, 1, RW, 0x0},
	{"PDP1_IN_CSIS4_EN", 10, 1, RW, 0x0},
	{"PDP1_IN_CSIS3_EN", 9, 1, RW, 0x0},
	{"PDP1_IN_CSIS2_EN", 8, 1, RW, 0x0},
	{"PDP1_IN_CSIS1_EN", 7, 1, RW, 0x0},
	{"PDP1_IN_CSIS0_EN", 6, 1, RW, 0x0},
	{"PDP0_IN_CSIS5_EN", 5, 1, RW, 0x0},
	{"PDP0_IN_CSIS4_EN", 4, 1, RW, 0x0},
	{"PDP0_IN_CSIS3_EN", 3, 1, RW, 0x0},
	{"PDP0_IN_CSIS2_EN", 2, 1, RW, 0x0},
	{"PDP0_IN_CSIS1_EN", 1, 1, RW, 0x0},
	{"PDP0_IN_CSIS0_EN", 0, 1, RW, 0x0},
	{"SW_RESETn_LHM_AST_GLUE_ZOTF2_IPPCSIS", 17, 1, RW, 0x1},
	{"SW_RESETn_LHM_AST_GLUE_ZOTF1_IPPCSIS", 16, 1, RW, 0x1},
	{"SW_RESETn_LHM_AST_GLUE_ZOTF0_IPPCSIS", 15, 1, RW, 0x1},
	{"SW_RESETn_LHM_AST_GLUE_SOTF2_IPPCSIS", 14, 1, RW, 0x1},
	{"SW_RESETn_LHM_AST_GLUE_SOTF1_IPPCSIS", 13, 1, RW, 0x1},
	{"SW_RESETn_LHM_AST_GLUE_SOTF0_IPPCSIS", 12, 1, RW, 0x1},
	{"SW_RESETn_LHM_AST_GLUE_OTF2_IPPCSIS", 11, 1, RW, 0x1},
	{"SW_RESETn_LHM_AST_GLUE_OTF1_IPPCSIS", 10, 1, RW, 0x1},
	{"SW_RESETn_LHM_AST_GLUE_OTF0_IPPCSIS", 9, 1, RW, 0x1},
	{"TYPE_LHM_AST_GLUE_ZOTF2_IPPCSIS", 8, 1, RW, 0x0},
	{"TYPE_LHM_AST_GLUE_ZOTF1_IPPCSIS", 7, 1, RW, 0x0},
	{"TYPE_LHM_AST_GLUE_ZOTF0_IPPCSIS", 6, 1, RW, 0x0},
	{"TYPE_LHM_AST_GLUE_SOTF2_IPPCSIS", 5, 1, RW, 0x0},
	{"TYPE_LHM_AST_GLUE_SOTF1_IPPCSIS", 4, 1, RW, 0x0},
	{"TYPE_LHM_AST_GLUE_SOTF0_IPPCSIS", 3, 1, RW, 0x0},
	{"TYPE_LHM_AST_GLUE_OTF2_IPPCSIS", 2, 1, RW, 0x0},
	{"TYPE_LHM_AST_GLUE_OTF1_IPPCSIS", 1, 1, RW, 0x0},
	{"TYPE_LHM_AST_GLUE_OTF0_IPPCSIS", 0, 1, RW, 0x0},
	{"LHM_AST_ZOTF2_IPPCSIS", 8, 1, RW, 0x1},
	{"LHM_AST_ZOTF1_IPPCSIS", 7, 1, RW, 0x1},
	{"LHM_AST_ZOTF0_IPPCSIS", 6, 1, RW, 0x1},
	{"LHM_AST_SOTF2_IPPCSIS", 5, 1, RW, 0x1},
	{"LHM_AST_SOTF1_IPPCSIS", 4, 1, RW, 0x1},
	{"LHM_AST_SOTF0_IPPCSIS", 3, 1, RW, 0x1},
	{"LHM_AST_OTF2_IPPCSIS", 2, 1, RW, 0x1},
	{"LHM_AST_OTF1_IPPCSIS", 1, 1, RW, 0x1},
	{"LHM_AST_OTF0_IPPCSIS", 0, 1, RW, 0x1},
	{"MIPI_RESETN_DPHY_S2", 5, 1, RW, 0x0},
	{"MIPI_RESETN_DPHY_S1", 4, 1, RW, 0x0},
	{"MIPI_RESETN_DPHY_S", 3, 1, RW, 0x0},
	{"MIPI_RESETN_DCPHY_S2", 2, 1, RW, 0x0},
	{"MIPI_RESETN_DCPHY_S1", 1, 1, RW, 0x0},
	{"MIPI_RESETN_DCPHY_S", 0, 1, RW, 0x0},
};

static struct is_field sysreg_ipp_fields[SYSREG_IPP_REG_FIELD_CNT] = {
	{"SW_RESETn_LHS_AST_GLUE_OTF1_IPPDNS", 21, 1, RW, 0x1}, /* IPP_USER_CON0 */
	{"SW_RESETn_LHS_AST_GLUE_OTF0_IPPDNS", 20, 1, RW, 0x1},
	{"SW_RESETn_LHS_AST_GLUE_SOTF2_IPPCSIS", 19, 1, RW, 0x1},
	{"SW_RESETn_LHS_AST_GLUE_SOTF1_IPPCSIS", 18, 1, RW, 0x1},
	{"SW_RESETn_LHS_AST_GLUE_SOTF0_IPPCSIS", 17, 1, RW, 0x1},
	{"SW_RESETn_LHS_AST_GLUE_ZOTF2_IPPCSIS", 16, 1, RW, 0x1},
	{"SW_RESETn_LHS_AST_GLUE_ZOTF1_IPPCSIS", 15, 1, RW, 0x1},
	{"SW_RESETn_LHS_AST_GLUE_ZOTF1_IPPCSIS", 14, 1, RW, 0x1},
	{"SW_RESETn_LHM_AST_GLUE_OTF2_CSISIPP", 13, 1, RW, 0x1},
	{"SW_RESETn_LHM_AST_GLUE_OTF1_CSISIPP", 12, 1, RW, 0x1},
	{"SW_RESETn_LHM_AST_GLUE_OTF0_CSISIPP", 11, 1, RW, 0x1},
	{"TYPE_LHS_AST_GLUE_OTF1_IPPDNS", 10, 1, RW, 0x0},
	{"TYPE_LHS_AST_GLUE_OTF0_IPPDNS", 9, 1, RW, 0x0},
	{"TYPE_LHS_AST_GLUE_SOTF2_IPPCSIS", 8, 1, RW, 0x0},
	{"TYPE_LHS_AST_GLUE_SOTF1_IPPCSIS", 7, 1, RW, 0x0},
	{"TYPE_LHS_AST_GLUE_SOTF0_IPPCSIS", 6, 1, RW, 0x0},
	{"TYPE_LHS_AST_GLUE_ZOTF2_IPPCSIS", 5, 1, RW, 0x0},
	{"TYPE_LHS_AST_GLUE_ZOTF1_IPPCSIS", 4, 1, RW, 0x0},
	{"TYPE_LHS_AST_GLUE_ZOTF2_IPPCSIS", 3, 1, RW, 0x0},
	{"TYPE_LHM_AST_GLUE_OTF2_CSISIP", 2, 1, RW, 0x0},
	{"TYPE_LHM_AST_GLUE_OTF1_CSISIP", 1, 1, RW, 0x0},
	{"TYPE_LHM_AST_GLUE_OTF0_CSISIP", 0, 1, RW, 0x0},
	{"GLUEMUX_OTFOUT_SEL_1", 2, 2, RW, 0x1}, /* IPP_USER_CON1 */
	{"GLUEMUX_OTFOUT_SEL_0", 0, 2, RW, 0x0},
	{"LHS_AST_OTF1_IPPDNS", 10, 1, RW, 0x1},
	{"LHS_AST_OTF0_IPPDNS", 9, 1, RW, 0x1},
	{"LHS_AST_ZOTF2_IPPCSIS", 8, 1, RW, 0x1},
	{"LHS_AST_ZOTF1_IPPCSIS", 7, 1, RW, 0x1},
	{"LHS_AST_ZOTF0_IPPCSIS", 6, 1, RW, 0x1},
	{"LHS_AST_SOTF2_IPPCSIS", 5, 1, RW, 0x1},
	{"LHS_AST_SOTF1_IPPCSIS", 4, 1, RW, 0x1},
	{"LHS_AST_SOTF0_IPPCSIS", 3, 1, RW, 0x1},
	{"LHS_AST_OTF2_IPPCSIS", 2, 1, RW, 0x1},
	{"LHS_AST_OTF1_IPPCSIS", 1, 1, RW, 0x1},
	{"LHS_AST_OTF0_IPPCSIS", 0, 1, RW, 0x1},
};

static struct is_field sysreg_tnr_fields[SYSREG_TNR_REG_FIELD_CNT] = {
	{"SW_RESETn", 0, 1, RW, 0x1}, /* SW_RESETn */
	{"GLUE_OTF_TNRDNS", 0, 1, RW, 0x0}, /* LH_AST_GLUE_TYPE */
	{"VOTF", 1, 1, RW, 0x1}, /* LH_ENABLE */
	{"OTF_TNRDNS", 0, 1, RW, 0x1},
};

static struct is_field sysreg_dns_fields[SYSREG_DNS_REG_FIELD_CNT] = {
	{"GLUEMUX_DNS1_VAL", 5, 1, RW, 0x0}, /* DNS_USER_CON0 */
	{"GLUEMUX_DNS0_VAL", 4, 1, RW, 0x0},
	{"AxCACHE_DNS", 0, 4, RW, 0x2},
	{"Enable_OTF_OUT_CTL_DNSITP", 16, 1, RW, 0x1}, /* DNS_USER_CON1 */
	{"Enable_OTF_IN_CTL_ITPDNS", 15, 1, RW, 0x1},
	{"Enable_OTF4_OUT_DNSITP", 14, 1, RW, 0x1},
	{"Enable_OTF3_OUT_DNSITP", 13, 1, RW, 0x1},
	{"Enable_OTF2_OUT_DNSITP", 12, 1, RW, 0x1},
	{"Enable_OTF1_OUT_DNSITP", 11, 1, RW, 0x1},
	{"Enable_OTF0_OUT_DNSITP", 10, 1, RW, 0x1},
	{"Enable_OTF_IN_ITPDNS", 9, 1, RW, 0x1},
	{"Enable_OTF_IN_TNRDNS", 8, 1, RW, 0x1},
	{"Enable_OTF1_IN_IPPDNS", 7, 1, RW, 0x1},
	{"Enable_OTF0_IN_IPPDNS", 6, 1, RW, 0x1},
	{"TYPE_LHM_AST_GLUE_OTF_TNRDNS", 5, 1, RW, 0x0},
	{"TYPE_LHM_AST_GLUE_OTF1_IPPDNS", 4, 1, RW, 0x0},
	{"TYPE_LHM_AST_GLUE_OTF0_IPPDNS", 3, 1, RW, 0x0},
	{"SW_RESETn_LHM_AST_GLUE_OTF_TNRDNS", 2, 1, RW, 0x1},
	{"SW_RESETn_LHM_AST_GLUE_OTF1_IPPDNS", 1, 1, RW, 0x1},
	{"SW_RESETn_LHM_AST_GLUE_OTF0_IPPDNS", 0, 1, RW, 0x1},
};

static struct is_field sysreg_itp_fields[SYSREG_ITP_REG_FIELD_CNT] = {
	{"Enable_OTF_IN_CTL_DNSITP", 19, 1, RW, 0x1}, /* ITP_USER_CON */
	{"Enable_OTF_OUT_CTL_ITPDNS", 18, 1, RW, 0x1},
	{"Enable_OTF4_IN_DNSITP", 17, 1, RW, 0x1},
	{"Enable_OTF3_IN_DNSITP", 16, 1, RW, 0x1},
	{"Enable_OTF2_IN_DNSITP", 15, 1, RW, 0x1},
	{"Enable_OTF1_IN_DNSITP", 14, 1, RW, 0x1},
	{"Enable_OTF0_IN_DNSITP", 13, 1, RW, 0x1},
	{"Enable_OTF_OUT_ITPDNS", 12, 1, RW, 0x1},
	{"Enable_OTF0_OUT_ITPMCSC", 11, 1, RW, 0x1},
	{"Enable_OTF1_OUT_ITPMCSC", 10, 1, RW, 0x1},
	{"Enable_OTF2_OUT_ITPMCSC", 9, 1, RW, 0x1},
	{"Enable_OTF3_OUT_ITPMCSC", 8, 1, RW, 0x1},
	{"SW_RESETn_LHS_AST_GLUE_OTF3_ITPMCSC", 7, 1, RW, 0x1},
	{"SW_RESETn_LHS_AST_GLUE_OTF2_ITPMCSC", 6, 1, RW, 0x1},
	{"SW_RESETn_LHS_AST_GLUE_OTF1_ITPMCSC", 5, 1, RW, 0x1},
	{"SW_RESETn_LHS_AST_GLUE_OTF0_ITPMCSC", 4, 1, RW, 0x1},
	{"TYPE_LHS_AST_GLUE_OTF3_ITPMCSC", 3, 1, RW, 0x0},
	{"TYPE_LHS_AST_GLUE_OTF2_ITPMCSC", 2, 1, RW, 0x0},
	{"TYPE_LHS_AST_GLUE_OTF1_ITPMCSC", 1, 1, RW, 0x0},
	{"TYPE_LHS_AST_GLUE_OTF0_ITPMCSC", 0, 1, RW, 0x0},
};

#if 0 /* not used */
static struct is_field sysreg_mcsc_fields[SYSREG_MCSC_REG_FIELD_CNT] = {
	{"SW_RESETn_LHM_AST_GLUE_OTF_ITPMCSC", 1, 1, RW, 0x0},
	{"TYPE_LHM_AST_GLUE_OTF_ITPMCSC", 0, 1, RW, 0x0},
	{"QOS_GDC", 12, 4, RW, 0x2},
	{"QOS_MCSC", 8, 4, RW, 0x2},
	{"AxCACHE_GDC", 4, 4, RW, 0x2},
	{"AxCACHE_MCSC", 0, 4, RW, 0x2},
	{"C2AGENT_D2_MCSC_M6S4_C2AGENT_SW_RESET", 4, 1, RW, 0x0},
	{"C2AGENT_D1_MCSC_M6S4_C2AGENT_SW_RESET", 3, 1, RW, 0x0},
	{"C2AGENT_D0_MCSC_M6S4_C2AGENT_SW_RESET", 2, 1, RW, 0x0},
	{"MCSC_C2COM_SW_RESET", 1, 1, RW, 0x0},
	{"Enable_OTF_IN_LHM_AST_D_ITPMCSC", 0, 1, RW, 0x0},
};

static struct is_field sysreg_vra_fields[SYSREG_VRA_REG_FIELD_CNT] = {
	{"QOS_D1_CLAHE", 8, 4, RW, 0x4},
	{"QOS_D0_CLAHE", 4, 4, RW, 0x4},
	{"AxCACHE_CLAHE", 0, 4, RW, 0x2},
};
#endif

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
/* 3AA0 */
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

static irqreturn_t __is_isr4_3aa0(int irq, void *data)
{
	/* for ORBMCH IRQ SHARED */
	struct is_interface_hwip *itf_hw = NULL;
	struct hwip_intr_handler *intr_hw = NULL;

	itf_hw = (struct is_interface_hwip *)data;
	intr_hw = &itf_hw->handler[INTR_HWIP4];

	if (intr_hw->chain_id != ID_ORBMCH_0)
		return IRQ_NONE;

	__is_isr_ddk(data, INTR_HWIP4);
	return IRQ_HANDLED;
}

static irqreturn_t __is_isr5_3aa0(int irq, void *data)
{
	__is_isr_ddk(data, INTR_HWIP5);
	return IRQ_HANDLED;
}

static irqreturn_t __is_isr6_3aa0(int irq, void *data)
{
	__is_isr_ddk(data, INTR_HWIP6);
	return IRQ_HANDLED;
}
/* 3AA1 */
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

static irqreturn_t __is_isr4_3aa1(int irq, void *data)
{
	/* for ORBMCH IRQ SHARED */
	struct is_interface_hwip *itf_hw = NULL;
	struct hwip_intr_handler *intr_hw = NULL;

	itf_hw = (struct is_interface_hwip *)data;
	intr_hw = &itf_hw->handler[INTR_HWIP4];

	if (intr_hw->chain_id != ID_ORBMCH_1)
		return IRQ_NONE;

	__is_isr_ddk(data, INTR_HWIP4);
	return IRQ_HANDLED;
}

static irqreturn_t __is_isr5_3aa1(int irq, void *data)
{
	__is_isr_ddk(data, INTR_HWIP5);
	return IRQ_HANDLED;
}

static irqreturn_t __is_isr6_3aa1(int irq, void *data)
{
	__is_isr_ddk(data, INTR_HWIP6);
	return IRQ_HANDLED;
}
/* 3AA2 */
static irqreturn_t __is_isr1_3aa2(int irq, void *data)
{
	__is_isr_ddk(data, INTR_HWIP1);
	return IRQ_HANDLED;
}

static irqreturn_t __is_isr2_3aa2(int irq, void *data)
{
	__is_isr_ddk(data, INTR_HWIP2);
	return IRQ_HANDLED;
}

#if 0 /* Not used */
static irqreturn_t __is_isr4_3aa2(int irq, void *data)
{
	__is_isr_ddk(data, INTR_HWIP4);
	return IRQ_HANDLED;
}
#endif

static irqreturn_t __is_isr5_3aa2(int irq, void *data)
{
	__is_isr_ddk(data, INTR_HWIP5);
	return IRQ_HANDLED;
}

static irqreturn_t __is_isr6_3aa2(int irq, void *data)
{
	__is_isr_ddk(data, INTR_HWIP6);
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

static irqreturn_t __is_isr4_itp0(int irq, void *data)
{
	__is_isr_ddk(data, INTR_HWIP4);
	return IRQ_HANDLED;
}
/* MCSC */
static irqreturn_t __is_isr1_mcs0(int irq, void *data)
{
	__is_isr_host(data, INTR_HWIP1);
	return IRQ_HANDLED;
}

/* CLAHE */
static irqreturn_t __is_isr1_clh0(int irq, void *data)
{
	__is_isr_ddk(data, INTR_HWIP1);
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
		__is_hw_group_init(group);
		group->subdev[ENTRY_PAF] = &device->group_paf.leader;
		group->subdev[ENTRY_PDAF] = &device->pdaf;
		group->subdev[ENTRY_PDST] = &device->pdst;

		list_add_tail(&device->group_paf.leader.list, &group->subdev_list);
		list_add_tail(&device->pdaf.list, &group->subdev_list);
		list_add_tail(&device->pdst.list, &group->subdev_list);
		break;
	case GROUP_SLOT_3AA:
		__is_hw_group_init(group);
		group->subdev[ENTRY_3AA] = &device->group_3aa.leader;
		group->subdev[ENTRY_3AC] = &device->txc;
		group->subdev[ENTRY_3AP] = &device->txp;
		group->subdev[ENTRY_3AF] = &device->txf;
		group->subdev[ENTRY_3AG] = &device->txg;
		group->subdev[ENTRY_MEXC] = &device->mexc;
		group->subdev[ENTRY_ORBXC] = &device->orbxc;

		list_add_tail(&device->group_3aa.leader.list, &group->subdev_list);
		list_add_tail(&device->txc.list, &group->subdev_list);
		list_add_tail(&device->txp.list, &group->subdev_list);
		list_add_tail(&device->txf.list, &group->subdev_list);
		list_add_tail(&device->txg.list, &group->subdev_list);
		list_add_tail(&device->mexc.list, &group->subdev_list);
		list_add_tail(&device->orbxc.list, &group->subdev_list);

		device->txc.param_dma_ot = PARAM_3AA_VDMA4_OUTPUT;
		device->txp.param_dma_ot = PARAM_3AA_VDMA2_OUTPUT;
		device->txf.param_dma_ot = PARAM_3AA_FDDMA_OUTPUT;
		device->txg.param_dma_ot = PARAM_3AA_MRGDMA_OUTPUT;
		break;
	case GROUP_SLOT_ISP:
		__is_hw_group_init(group);
		group->subdev[ENTRY_ISP] = &device->group_isp.leader;
		group->subdev[ENTRY_IXC] = &device->ixc;
		group->subdev[ENTRY_IXP] = &device->ixp;
		group->subdev[ENTRY_IXT] = &device->ixt;
		group->subdev[ENTRY_IXG] = &device->ixg;
		group->subdev[ENTRY_IXV] = &device->ixv;
		group->subdev[ENTRY_IXW] = &device->ixw;

		list_add_tail(&device->group_isp.leader.list, &group->subdev_list);
		list_add_tail(&device->ixc.list, &group->subdev_list);
		list_add_tail(&device->ixp.list, &group->subdev_list);
		list_add_tail(&device->ixt.list, &group->subdev_list);
		list_add_tail(&device->ixg.list, &group->subdev_list);
		list_add_tail(&device->ixv.list, &group->subdev_list);
		list_add_tail(&device->ixw.list, &group->subdev_list);
		break;
	case GROUP_SLOT_MCS:
		__is_hw_group_init(group);
		group->subdev[ENTRY_MCS] = &device->group_mcs.leader;
		group->subdev[ENTRY_M0P] = &device->m0p;
		group->subdev[ENTRY_M1P] = &device->m1p;
		group->subdev[ENTRY_M2P] = &device->m2p;
		group->subdev[ENTRY_M3P] = &device->m3p;
		group->subdev[ENTRY_M4P] = &device->m4p;
		group->subdev[ENTRY_M5P] = &device->m5p;

		list_add_tail(&device->group_mcs.leader.list, &group->subdev_list);
		list_add_tail(&device->m0p.list, &group->subdev_list);
		list_add_tail(&device->m1p.list, &group->subdev_list);
		list_add_tail(&device->m2p.list, &group->subdev_list);
		list_add_tail(&device->m3p.list, &group->subdev_list);
		list_add_tail(&device->m4p.list, &group->subdev_list);
		list_add_tail(&device->m5p.list, &group->subdev_list);

		device->m0p.param_dma_ot = PARAM_MCS_OUTPUT0;
		device->m1p.param_dma_ot = PARAM_MCS_OUTPUT1;
		device->m2p.param_dma_ot = PARAM_MCS_OUTPUT2;
		device->m3p.param_dma_ot = PARAM_MCS_OUTPUT3;
		device->m4p.param_dma_ot = PARAM_MCS_OUTPUT4;
		device->m5p.param_dma_ot = PARAM_MCS_OUTPUT5;
		break;
	case GROUP_SLOT_VRA:
		__is_hw_group_init(group);
		group->subdev[ENTRY_VRA] = &device->group_vra.leader;

		list_add_tail(&device->group_vra.leader.list, &group->subdev_list);
		break;
	case GROUP_SLOT_CLH:
		__is_hw_group_init(group);
		group->subdev[ENTRY_CLH] = &device->group_clh.leader;
		group->subdev[ENTRY_CLHC] = &device->clhc;

		list_add_tail(&device->group_clh.leader.list, &group->subdev_list);
		list_add_tail(&device->clhc.list, &group->subdev_list);
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
	case GROUP_ID_PAF0:
	case GROUP_ID_PAF1:
	case GROUP_ID_PAF2:
		leader->constraints_width = GROUP_PDP_MAX_WIDTH;
		leader->constraints_height = GROUP_PDP_MAX_HEIGHT;
		break;
	case GROUP_ID_3AA0:
	case GROUP_ID_3AA1:
	case GROUP_ID_3AA2:
		leader->constraints_width = GROUP_3AA_MAX_WIDTH;
		leader->constraints_height = GROUP_3AA_MAX_HEIGHT;
		break;
	case GROUP_ID_ISP0:
	case GROUP_ID_ISP1:
	case GROUP_ID_MCS0:
	case GROUP_ID_MCS1:
		leader->constraints_width = GROUP_ITP_MAX_WIDTH;
		leader->constraints_height = GROUP_ITP_MAX_HEIGHT;
		break;
	case GROUP_ID_VRA0:
		leader->constraints_width = GROUP_VRA_MAX_WIDTH;
		leader->constraints_height = GROUP_VRA_MAX_HEIGHT;
		break;
	case GROUP_ID_CLH0:
		leader->constraints_width = GROUP_CLAHE_MAX_WIDTH;
		leader->constraints_height = GROUP_CLAHE_MAX_HEIGHT;
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
	case DEV_HW_PAF0:
		slot_id = 0;
		break;
	case DEV_HW_PAF1:
		slot_id = 1;
		break;
	case DEV_HW_PAF2:
		slot_id = 2;
		break;
	case DEV_HW_3AA0:
		slot_id = 3;
		break;
	case DEV_HW_3AA1:
		slot_id = 4;
		break;
	case DEV_HW_3AA2:
		slot_id = 5;
		break;
	case DEV_HW_ISP0:
		slot_id = 6;
		break;
	case DEV_HW_MCSC0:
		slot_id = 7;
		break;
	case DEV_HW_CLH0:
		slot_id = 8;
		break;
	case DEV_HW_ISP1:
	case DEV_HW_MCSC1:
	case DEV_HW_VRA:
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
	case GROUP_ID_PAF0:
		hw_list[hw_index] = DEV_HW_PAF0; hw_index++;
		break;
	case GROUP_ID_PAF1:
		hw_list[hw_index] = DEV_HW_PAF1; hw_index++;
		break;
	case GROUP_ID_PAF2:
		hw_list[hw_index] = DEV_HW_PAF2; hw_index++;
		break;
	case GROUP_ID_3AA0:
		hw_list[hw_index] = DEV_HW_3AA0; hw_index++;
		break;
	case GROUP_ID_3AA1:
		hw_list[hw_index] = DEV_HW_3AA1; hw_index++;
		break;
	case GROUP_ID_3AA2:
		hw_list[hw_index] = DEV_HW_3AA2; hw_index++;
		break;
	case GROUP_ID_ISP0:
		hw_list[hw_index] = DEV_HW_ISP0; hw_index++;
		break;
	case GROUP_ID_MCS0:
		hw_list[hw_index] = DEV_HW_MCSC0; hw_index++;
		break;
	case GROUP_ID_CLH0:
		hw_list[hw_index] = DEV_HW_CLH0; hw_index++;
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

		mem_res = platform_get_resource(pdev, IORESOURCE_MEM, IORESOURCE_3AA_DMA_TOP);
		if (!mem_res) {
			dev_err(&pdev->dev, "Failed to get io memory region\n");
			return -EINVAL;
		}

		itf_hwip->hw_ip->regs_start[REG_EXT2] = mem_res->start;
		itf_hwip->hw_ip->regs_end[REG_EXT2] = mem_res->end;
		itf_hwip->hw_ip->regs[REG_EXT2] = ioremap_nocache(mem_res->start, resource_size(mem_res));
		if (!itf_hwip->hw_ip->regs[REG_EXT2]) {
			dev_err(&pdev->dev, "Failed to remap io region\n");
			return -EINVAL;
		}

		idx = 0;
		itf_hwip->hw_ip->dump_region[REG_EXT2][idx].start = 0x0;
		itf_hwip->hw_ip->dump_region[REG_EXT2][idx++].end = 0x4FFF;
		itf_hwip->hw_ip->dump_region[REG_EXT2][idx].start = 0x6000;
		itf_hwip->hw_ip->dump_region[REG_EXT2][idx++].end = 0xFFFF;

		mem_res = platform_get_resource(pdev, IORESOURCE_MEM, IORESOURCE_ORBMCH);
		if (!mem_res) {
			dev_err(&pdev->dev, "Failed to get io memory region\n");
			return -EINVAL;
		}

		itf_hwip->hw_ip->regs_start[REG_EXT1] = mem_res->start;
		itf_hwip->hw_ip->regs_end[REG_EXT1] = mem_res->end;
		itf_hwip->hw_ip->regs[REG_EXT1] = ioremap_nocache(mem_res->start, resource_size(mem_res));
		if (!itf_hwip->hw_ip->regs[REG_EXT1]) {
			dev_err(&pdev->dev, "Failed to remap io region\n");
			return -EINVAL;
		}

		info_itfc("[ID:%2d] 3AA VA(0x%lx)\n", hw_id, (ulong)itf_hwip->hw_ip->regs[REG_SETA]);
		info_itfc("[ID:%2d] 3AA DMA VA(0x%lx)\n", hw_id, (ulong)itf_hwip->hw_ip->regs[REG_EXT2]);
		info_itfc("[ID:%2d] ORBMCH VA(0x%lx)\n", hw_id, (ulong)itf_hwip->hw_ip->regs[REG_EXT1]);
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
		mem_res = platform_get_resource(pdev, IORESOURCE_MEM, IORESOURCE_3AA_DMA_TOP);
		if (!mem_res) {
			dev_err(&pdev->dev, "Failed to get io memory region\n");
			return -EINVAL;
		}

		idx = 0;
		itf_hwip->hw_ip->dump_region[REG_SETA][idx].start = 0x0;
		itf_hwip->hw_ip->dump_region[REG_SETA][idx++].end = 0x1FB3;
		itf_hwip->hw_ip->dump_region[REG_SETA][idx].start = 0x1FB8;
		itf_hwip->hw_ip->dump_region[REG_SETA][idx++].end = 0x9FB3;
		itf_hwip->hw_ip->dump_region[REG_SETA][idx].start = 0x9FB8;
		itf_hwip->hw_ip->dump_region[REG_SETA][idx++].end = 0xFFFF;

		itf_hwip->hw_ip->regs_start[REG_EXT2] = mem_res->start;
		itf_hwip->hw_ip->regs_end[REG_EXT2] = mem_res->end;
		itf_hwip->hw_ip->regs[REG_EXT2] = ioremap_nocache(mem_res->start, resource_size(mem_res));
		if (!itf_hwip->hw_ip->regs[REG_EXT2]) {
			dev_err(&pdev->dev, "Failed to remap io region\n");
			return -EINVAL;
		}

		idx = 0;
		itf_hwip->hw_ip->dump_region[REG_EXT2][idx].start = 0x0;
		itf_hwip->hw_ip->dump_region[REG_EXT2][idx++].end = 0x4FFF;
		itf_hwip->hw_ip->dump_region[REG_EXT2][idx].start = 0x6000;
		itf_hwip->hw_ip->dump_region[REG_EXT2][idx++].end = 0xFFFF;

		mem_res = platform_get_resource(pdev, IORESOURCE_MEM, IORESOURCE_ORBMCH);
		if (!mem_res) {
			dev_err(&pdev->dev, "Failed to get io memory region\n");
			return -EINVAL;
		}

		itf_hwip->hw_ip->regs_start[REG_EXT1] = mem_res->start;
		itf_hwip->hw_ip->regs_end[REG_EXT1] = mem_res->end;
		itf_hwip->hw_ip->regs[REG_EXT1] = ioremap_nocache(mem_res->start, resource_size(mem_res));
		if (!itf_hwip->hw_ip->regs[REG_EXT1]) {
			dev_err(&pdev->dev, "Failed to remap io region\n");
			return -EINVAL;
		}

		info_itfc("[ID:%2d] 3AA VA(0x%lx)\n", hw_id, (ulong)itf_hwip->hw_ip->regs[REG_SETA]);
		info_itfc("[ID:%2d] 3AA DMA VA(0x%lx)\n", hw_id, (ulong)itf_hwip->hw_ip->regs[REG_EXT2]);
		info_itfc("[ID:%2d] ORBMCH VA(0x%lx)\n", hw_id, (ulong)itf_hwip->hw_ip->regs[REG_EXT1]);
		break;
	case DEV_HW_3AA2:
		mem_res = platform_get_resource(pdev, IORESOURCE_MEM, IORESOURCE_3AA2);
		if (!mem_res) {
			dev_err(&pdev->dev, "Failed to get io memory region\n");
			return -EINVAL;
		}
		itf_hwip->hw_ip->regs_start[REG_SETA] = mem_res->start + (2 * LIC_CHAIN_OFFSET);
		itf_hwip->hw_ip->regs_end[REG_SETA] = mem_res->end;
		itf_hwip->hw_ip->regs[REG_SETA] = ioremap_nocache(mem_res->start, resource_size(mem_res));
		if (!itf_hwip->hw_ip->regs[REG_SETA]) {
			dev_err(&pdev->dev, "Failed to remap io region\n");
			return -EINVAL;
		}
		itf_hwip->hw_ip->regs[REG_SETA] += (2 * LIC_CHAIN_OFFSET);
		mem_res = platform_get_resource(pdev, IORESOURCE_MEM, IORESOURCE_3AA_DMA_TOP);
		if (!mem_res) {
			dev_err(&pdev->dev, "Failed to get io memory region\n");
			return -EINVAL;
		}

		idx = 0;
		itf_hwip->hw_ip->dump_region[REG_SETA][idx].start = 0x0;
		itf_hwip->hw_ip->dump_region[REG_SETA][idx++].end = 0x1FB3;
		itf_hwip->hw_ip->dump_region[REG_SETA][idx].start = 0x1FB8;
		itf_hwip->hw_ip->dump_region[REG_SETA][idx++].end = 0x9FB3;
		itf_hwip->hw_ip->dump_region[REG_SETA][idx].start = 0x9FB8;
		itf_hwip->hw_ip->dump_region[REG_SETA][idx++].end = 0xFFFF;

		itf_hwip->hw_ip->regs_start[REG_EXT2] = mem_res->start;
		itf_hwip->hw_ip->regs_end[REG_EXT2] = mem_res->end;
		itf_hwip->hw_ip->regs[REG_EXT2] = ioremap_nocache(mem_res->start, resource_size(mem_res));
		if (!itf_hwip->hw_ip->regs[REG_EXT2]) {
			dev_err(&pdev->dev, "Failed to remap io region\n");
			return -EINVAL;
		}

		idx = 0;
		itf_hwip->hw_ip->dump_region[REG_EXT2][idx].start = 0x0;
		itf_hwip->hw_ip->dump_region[REG_EXT2][idx++].end = 0x4FFF;
		itf_hwip->hw_ip->dump_region[REG_EXT2][idx].start = 0x6000;
		itf_hwip->hw_ip->dump_region[REG_EXT2][idx++].end = 0xFFFF;

		mem_res = platform_get_resource(pdev, IORESOURCE_MEM, IORESOURCE_ORBMCH);
		if (!mem_res) {
			dev_err(&pdev->dev, "Failed to get io memory region\n");
			return -EINVAL;
		}

		itf_hwip->hw_ip->regs_start[REG_EXT1] = mem_res->start;
		itf_hwip->hw_ip->regs_end[REG_EXT1] = mem_res->end;
		itf_hwip->hw_ip->regs[REG_EXT1] = ioremap_nocache(mem_res->start, resource_size(mem_res));
		if (!itf_hwip->hw_ip->regs[REG_EXT1]) {
			dev_err(&pdev->dev, "Failed to remap io region\n");
			return -EINVAL;
		}

		info_itfc("[ID:%2d] 3AA VA(0x%lx)\n", hw_id, (ulong)itf_hwip->hw_ip->regs[REG_SETA]);
		info_itfc("[ID:%2d] 3AA DMA VA(0x%lx)\n", hw_id, (ulong)itf_hwip->hw_ip->regs[REG_EXT2]);
		info_itfc("[ID:%2d] ORBMCH VA(0x%lx)\n", hw_id, (ulong)itf_hwip->hw_ip->regs[REG_EXT1]);
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

		mem_res = platform_get_resource(pdev, IORESOURCE_MEM, IORESOURCE_TNR);
		if (!mem_res) {
			dev_err(&pdev->dev, "Failed to get io memory region\n");
			return -EINVAL;
		}

		itf_hwip->hw_ip->regs_start[REG_EXT1] = mem_res->start;
		itf_hwip->hw_ip->regs_end[REG_EXT1] = mem_res->end;
		itf_hwip->hw_ip->regs[REG_EXT1] = ioremap_nocache(mem_res->start, resource_size(mem_res));
		if (!itf_hwip->hw_ip->regs[REG_EXT1]) {
			dev_err(&pdev->dev, "Failed to remap io region\n");
			return -EINVAL;
		}
		idx = 0;
		itf_hwip->hw_ip->dump_region[REG_EXT1][idx].start = 0x0;
		itf_hwip->hw_ip->dump_region[REG_EXT1][idx++].end = 0x8BFF;
		itf_hwip->hw_ip->dump_region[REG_EXT1][idx].start = 0x9000;
		itf_hwip->hw_ip->dump_region[REG_EXT1][idx++].end = 0x9BFF;
		itf_hwip->hw_ip->dump_region[REG_EXT1][idx].start = 0xA000;
		itf_hwip->hw_ip->dump_region[REG_EXT1][idx++].end = 0xFFFF;

		mem_res = platform_get_resource(pdev, IORESOURCE_MEM, IORESOURCE_DNS0);
		if (!mem_res) {
			dev_err(&pdev->dev, "Failed to get io memory region\n");
			return -EINVAL;
		}

		itf_hwip->hw_ip->regs_start[REG_EXT2] = mem_res->start;
		itf_hwip->hw_ip->regs_end[REG_EXT2] = mem_res->end;
		itf_hwip->hw_ip->regs[REG_EXT2] = ioremap_nocache(mem_res->start, resource_size(mem_res));
		if (!itf_hwip->hw_ip->regs[REG_EXT2]) {
			dev_err(&pdev->dev, "Failed to remap io region\n");
			return -EINVAL;
		}
		idx = 0;
		itf_hwip->hw_ip->dump_region[REG_EXT2][idx].start = 0x0;
		itf_hwip->hw_ip->dump_region[REG_EXT2][idx++].end = 0x00FF;
		itf_hwip->hw_ip->dump_region[REG_EXT2][idx].start = 0x0200;
		itf_hwip->hw_ip->dump_region[REG_EXT2][idx++].end = 0x09FF;
		itf_hwip->hw_ip->dump_region[REG_EXT2][idx].start = 0x0B00;
		itf_hwip->hw_ip->dump_region[REG_EXT2][idx++].end = 0x0DFF;
		itf_hwip->hw_ip->dump_region[REG_EXT2][idx].start = 0x1100;
		itf_hwip->hw_ip->dump_region[REG_EXT2][idx++].end = 0x11FF;
		itf_hwip->hw_ip->dump_region[REG_EXT2][idx].start = 0x2000;
		itf_hwip->hw_ip->dump_region[REG_EXT2][idx++].end = 0x23FF;
		itf_hwip->hw_ip->dump_region[REG_EXT2][idx].start = 0x2800;
		itf_hwip->hw_ip->dump_region[REG_EXT2][idx++].end = 0x29FF;
		itf_hwip->hw_ip->dump_region[REG_EXT2][idx].start = 0x2C00;
		itf_hwip->hw_ip->dump_region[REG_EXT2][idx++].end = 0x2CFF;
		itf_hwip->hw_ip->dump_region[REG_EXT2][idx].start = 0x3000;
		itf_hwip->hw_ip->dump_region[REG_EXT2][idx++].end = 0xFFFF;

		info_itfc("[ID:%2d] ITP0 VA(0x%lx)\n", hw_id, (ulong)itf_hwip->hw_ip->regs[REG_SETA]);
		info_itfc("[ID:%2d] TNR DMA VA(0x%lx)\n", hw_id, (ulong)itf_hwip->hw_ip->regs[REG_EXT1]);
		info_itfc("[ID:%2d] DNS0 VA(0x%lx)\n", hw_id, (ulong)itf_hwip->hw_ip->regs[REG_EXT2]);
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
	case DEV_HW_CLH0:
		mem_res = platform_get_resource(pdev, IORESOURCE_MEM, IORESOURCE_CLH0);
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

		itf_hwip->irq[INTR_HWIP4] = platform_get_irq(pdev, 2);
		if (itf_hwip->irq[INTR_HWIP4] < 0) {
			err("Failed to get irq MEIP0\n");
			return -EINVAL;
		}

		itf_hwip->irq[INTR_HWIP5] = platform_get_irq(pdev, 3);
		if (itf_hwip->irq[INTR_HWIP5] < 0) {
			err("Failed to get irq 3aa0 DMA1\n");
			return -EINVAL;
		}

		itf_hwip->irq[INTR_HWIP6] = platform_get_irq(pdev, 4);
		if (itf_hwip->irq[INTR_HWIP6] < 0) {
			err("Failed to get irq 3aa0 DMA2\n");
			return -EINVAL;
		}
		break;
	case DEV_HW_3AA1:
		itf_hwip->irq[INTR_HWIP1] = platform_get_irq(pdev, 5);
		if (itf_hwip->irq[INTR_HWIP1] < 0) {
			err("Failed to get irq 3aa1-1\n");
			return -EINVAL;
		}

		itf_hwip->irq[INTR_HWIP2] = platform_get_irq(pdev, 6);
		if (itf_hwip->irq[INTR_HWIP2] < 0) {
			err("Failed to get irq 3aa1-2\n");
			return -EINVAL;
		}

		itf_hwip->irq[INTR_HWIP4] = platform_get_irq(pdev, 7);
		if (itf_hwip->irq[INTR_HWIP4] < 0) {
			err("Failed to get irq MEIP1\n");
			return -EINVAL;
		}

		itf_hwip->irq[INTR_HWIP5] = platform_get_irq(pdev, 8);
		if (itf_hwip->irq[INTR_HWIP5] < 0) {
			err("Failed to get irq 3aa1 DMA1\n");
			return -EINVAL;
		}

		itf_hwip->irq[INTR_HWIP6] = platform_get_irq(pdev, 9);
		if (itf_hwip->irq[INTR_HWIP6] < 0) {
			err("Failed to get irq 3aa1 DMA2\n");
			return -EINVAL;
		}
		break;
	case DEV_HW_3AA2:
		itf_hwip->irq[INTR_HWIP1] = platform_get_irq(pdev, 10);
		if (itf_hwip->irq[INTR_HWIP1] < 0) {
			err("Failed to get irq 3aa2-1\n");
			return -EINVAL;
		}

		itf_hwip->irq[INTR_HWIP2] = platform_get_irq(pdev, 11);
		if (itf_hwip->irq[INTR_HWIP2] < 0) {
			err("Failed to get irq 3aa2-2\n");
			return -EINVAL;
		}

		itf_hwip->irq[INTR_HWIP5] = platform_get_irq(pdev, 12);
		if (itf_hwip->irq[INTR_HWIP5] < 0) {
			err("Failed to get irq 3aa2 DMA1\n");
			return -EINVAL;
		}

		itf_hwip->irq[INTR_HWIP6] = platform_get_irq(pdev, 13);
		if (itf_hwip->irq[INTR_HWIP6] < 0) {
			err("Failed to get irq 3aa2 DMA2\n");
			return -EINVAL;
		}
		break;
	case DEV_HW_ISP0:
		itf_hwip->irq[INTR_HWIP1] = platform_get_irq(pdev, 14);
		if (itf_hwip->irq[INTR_HWIP1] < 0) {
			err("Failed to get irq isp0-1\n");
			return -EINVAL;
		}

		itf_hwip->irq[INTR_HWIP2] = platform_get_irq(pdev, 15);
		if (itf_hwip->irq[INTR_HWIP2] < 0) {
			err("Failed to get irq isp0-2\n");
			return -EINVAL;
		}

		itf_hwip->irq[INTR_HWIP4] = platform_get_irq(pdev, 16);
		if (itf_hwip->irq[INTR_HWIP4] < 0) {
			err("Failed to get irq tnr0\n");
			return -EINVAL;
		}
		break;
	case DEV_HW_MCSC0:
		itf_hwip->irq[INTR_HWIP1] = platform_get_irq(pdev, 17);
		if (itf_hwip->irq[INTR_HWIP1] < 0) {
			err("Failed to get irq mcsc0\n");
			return -EINVAL;
		}
		break;
	case DEV_HW_CLH0:
		itf_hwip->irq[INTR_HWIP1] = platform_get_irq(pdev, 18);
		if (itf_hwip->irq[INTR_HWIP1] < 0) {
			err("Failed to get irq clh0\n");
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
		ret = __is_hw_request_irq(itf_hwip, "orbmch0", INTR_HWIP4, IRQF_SHARED, __is_isr4_3aa0);
		ret = __is_hw_request_irq(itf_hwip, "3a0dma0", INTR_HWIP5, IRQF_TRIGGER_NONE, __is_isr5_3aa0);
		ret = __is_hw_request_irq(itf_hwip, "3a0dma1", INTR_HWIP6, IRQF_TRIGGER_NONE, __is_isr6_3aa0);
		break;
	case DEV_HW_3AA1:
		ret = __is_hw_request_irq(itf_hwip, "3a1-0", INTR_HWIP1, IRQF_TRIGGER_NONE, __is_isr1_3aa1);
		ret = __is_hw_request_irq(itf_hwip, "3a1-1", INTR_HWIP2, IRQF_TRIGGER_NONE, __is_isr2_3aa1);
		ret = __is_hw_request_irq(itf_hwip, "orbmch1", INTR_HWIP4, IRQF_SHARED, __is_isr4_3aa1);
		ret = __is_hw_request_irq(itf_hwip, "3a1dma0", INTR_HWIP5, IRQF_TRIGGER_NONE, __is_isr5_3aa1);
		ret = __is_hw_request_irq(itf_hwip, "3a1dma1", INTR_HWIP6, IRQF_TRIGGER_NONE, __is_isr6_3aa1);
		break;
	case DEV_HW_3AA2:
		ret = __is_hw_request_irq(itf_hwip, "3a2-0", INTR_HWIP1, IRQF_TRIGGER_NONE, __is_isr1_3aa2);
		ret = __is_hw_request_irq(itf_hwip, "3a2-1", INTR_HWIP2, IRQF_TRIGGER_NONE, __is_isr2_3aa2);
		ret = __is_hw_request_irq(itf_hwip, "3a2dma0", INTR_HWIP5, IRQF_TRIGGER_NONE, __is_isr5_3aa2);
		ret = __is_hw_request_irq(itf_hwip, "3a2dma1", INTR_HWIP6, IRQF_TRIGGER_NONE, __is_isr6_3aa2);
		break;
	case DEV_HW_ISP0:
		ret = __is_hw_request_irq(itf_hwip, "itp0", INTR_HWIP1, IRQF_TRIGGER_NONE, __is_isr1_itp0);
		ret = __is_hw_request_irq(itf_hwip, "itp0", INTR_HWIP2, IRQF_TRIGGER_NONE, __is_isr2_itp0);
		ret = __is_hw_request_irq(itf_hwip, "tnr0", INTR_HWIP4, IRQF_TRIGGER_NONE, __is_isr4_itp0);
		break;
	case DEV_HW_MCSC0:
		ret = __is_hw_request_irq(itf_hwip, "mcs0", INTR_HWIP1, IRQF_TRIGGER_NONE, __is_isr1_mcs0);
		break;
	case DEV_HW_CLH0:
		ret = __is_hw_request_irq(itf_hwip, "clh0", INTR_HWIP1, IRQF_TRIGGER_NONE, __is_isr1_clh0);
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
		if (hw_id == IS_VIDEO_M3P_NUM) {
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

			cap->hw_ver = HW_SET_VERSION(8, 0, 1, 0);
			cap->max_output = 5;
			cap->max_djag = 1;
			cap->max_cac = 1;
			cap->max_uvsp = 2;
			cap->hwfc = MCSC_CAP_SUPPORT;
			cap->in_otf = MCSC_CAP_SUPPORT;
			cap->in_dma = MCSC_CAP_NOT_SUPPORT;
			cap->out_dma[0] = MCSC_CAP_SUPPORT;
			cap->out_dma[1] = MCSC_CAP_SUPPORT;
			cap->out_dma[2] = MCSC_CAP_SUPPORT;
			cap->out_dma[3] = MCSC_CAP_SUPPORT;
			cap->out_dma[4] = MCSC_CAP_SUPPORT;
			cap->out_dma[5] = MCSC_CAP_NOT_SUPPORT;
			cap->out_otf[0] = MCSC_CAP_NOT_SUPPORT;
			cap->out_otf[1] = MCSC_CAP_NOT_SUPPORT;
			cap->out_otf[2] = MCSC_CAP_NOT_SUPPORT;
			cap->out_otf[3] = MCSC_CAP_NOT_SUPPORT;
			cap->out_otf[4] = MCSC_CAP_NOT_SUPPORT;
			cap->out_otf[5] = MCSC_CAP_NOT_SUPPORT;
			cap->out_hwfc[3] = MCSC_CAP_SUPPORT;
			cap->out_hwfc[4] = MCSC_CAP_SUPPORT;
			cap->out_post[0] = MCSC_CAP_SUPPORT;
			cap->out_post[1] = MCSC_CAP_SUPPORT;
			cap->out_post[2] = MCSC_CAP_SUPPORT;
			cap->out_post[3] = MCSC_CAP_NOT_SUPPORT;
			cap->out_post[4] = MCSC_CAP_NOT_SUPPORT;
			cap->out_post[5] = MCSC_CAP_NOT_SUPPORT;
			cap->enable_shared_output = false;
			cap->tdnr = MCSC_CAP_NOT_SUPPORT;
			cap->djag = MCSC_CAP_SUPPORT;
			cap->cac = MCSC_CAP_SUPPORT;
			cap->uvsp = MCSC_CAP_NOT_SUPPORT;
			cap->ysum = MCSC_CAP_NOT_SUPPORT;
			cap->ds_vra = MCSC_CAP_NOT_SUPPORT;
		}
		break;
	case DEV_HW_MCSC1:
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

	/* TODO */

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

int blk_csis_qactive_control(struct is_device_ischain *device, bool on_off)
{
	void __iomem *csis_sys_regs;
	u32 csis_val = 0;
	int i = 0, ret = 0;

	FIMC_BUG(!device);
	csis_sys_regs = ioremap_nocache(SYSREG_CSIS_BASE_ADDR, 0x1000);

	csis_val = is_hw_get_reg(csis_sys_regs,
				&sysreg_csis_regs[SYSREG_R_CSIS_LH_QACTIVE_CON]);
	for (i = SYSREG_F_LHM_AST_ZOTF2_IPPCSIS; i <= SYSREG_F_LHM_AST_OTF0_IPPCSIS; i++) {
		csis_val = is_hw_set_field_value(csis_val,
				&sysreg_csis_fields[i], on_off);
	}

	minfo("SYSREG_R_CSIS_LH_QACTIVE_CON:(0x%08X)\n", device, csis_val);
	is_hw_set_reg(csis_sys_regs,
		&sysreg_csis_regs[SYSREG_R_CSIS_LH_QACTIVE_CON], csis_val);

	iounmap(csis_sys_regs);
	return ret;
}

int blk_ipp_mux0_control(struct is_device_ischain *device, u32 value)
{
	void __iomem *ipp_sys_regs;
	u32 ipp_val = 0;
	int ret = 0;

	FIMC_BUG(!device);
	ipp_sys_regs = ioremap_nocache(SYSREG_IPP_BASE_ADDR, 0x1000);

	ipp_val = is_hw_get_reg(ipp_sys_regs,
				&sysreg_ipp_regs[SYSREG_R_IPP_USER_CON1]);
	/* 0: IPP ch0, 1: IPP ch1, 2: IPP ch2, 3: not support */
	ipp_val = is_hw_set_field_value(ipp_val,
				&sysreg_ipp_fields[SYSREG_F_GLUEMUX_OTFOUT_SEL_0], value);

	minfo("SYSREG_R_IPP_USER_CON1:(0x%08X)\n", device, ipp_val);
	is_hw_set_reg(ipp_sys_regs, &sysreg_ipp_regs[SYSREG_R_IPP_USER_CON1], ipp_val);

	iounmap(ipp_sys_regs);
	return ret;
}

int blk_ipp_mux1_control(struct is_device_ischain *device, u32 value)
{
	void __iomem *ipp_sys_regs;
	u32 ipp_val = 0;
	int ret = 0;

	FIMC_BUG(!device);
	ipp_sys_regs = ioremap_nocache(SYSREG_IPP_BASE_ADDR, 0x1000);

	ipp_val = is_hw_get_reg(ipp_sys_regs,
				&sysreg_ipp_regs[SYSREG_R_IPP_USER_CON1]);
	/* 0: IPP ch0, 1: IPP ch1, 2: IPP ch2, 3: not support */
	ipp_val = is_hw_set_field_value(ipp_val,
				&sysreg_ipp_fields[SYSREG_F_GLUEMUX_OTFOUT_SEL_1], value);

	minfo("SYSREG_R_IPP_USER_CON1:(0x%08X)\n", device, ipp_val);
	is_hw_set_reg(ipp_sys_regs, &sysreg_ipp_regs[SYSREG_R_IPP_USER_CON1], ipp_val);

	iounmap(ipp_sys_regs);
	return ret;
}

int blk_ipp_qactive_control(struct is_device_ischain *device, bool on_off)
{
	void __iomem *ipp_sys_regs;
	u32 ipp_val = 0;
	int i = 0, ret = 0;

	FIMC_BUG(!device);
	ipp_sys_regs = ioremap_nocache(SYSREG_IPP_BASE_ADDR, 0x1000);

	ipp_val = is_hw_get_reg(ipp_sys_regs,
				&sysreg_ipp_regs[SYSREG_R_IPP_LH_QACTIVE_CON]);
	for (i = SYSREG_F_LHS_AST_OTF1_IPPDNS; i <= SYSREG_F_LHS_AST_OTF0_IPPCSIS; i++) {
		ipp_val = is_hw_set_field_value(ipp_val,
				&sysreg_ipp_fields[i], on_off);
	}

	info("SYSREG_R_IPP_LH_QACTIVE_CON:(0x%08X)\n", ipp_val);
	is_hw_set_reg(ipp_sys_regs,
		&sysreg_ipp_regs[SYSREG_R_IPP_LH_QACTIVE_CON], ipp_val);

	iounmap(ipp_sys_regs);
	return ret;
}

int blk_tnr_lh_control(struct is_device_ischain *device, bool on_off)
{
	void __iomem *tnr_sys_regs;
	u32 tnr_val = 0;
	int ret = 0;

	FIMC_BUG(!device);

	tnr_sys_regs = ioremap_nocache(SYSREG_TNR_BASE_ADDR, 0x1000);

	tnr_val = is_hw_get_reg(tnr_sys_regs,
				&sysreg_tnr_regs[SYSREG_R_LH_ENABLE]);
	tnr_val = is_hw_set_field_value(tnr_val,
				&sysreg_tnr_fields[SYSREG_F_VOTF], on_off);
	tnr_val = is_hw_set_field_value(tnr_val,
				&sysreg_tnr_fields[SYSREG_F_OTF_TNRDNS], on_off);
	info("SYSREG_R_LH_ENABLE:(0x%08X)\n", tnr_val);

	is_hw_set_reg(tnr_sys_regs,
		&sysreg_tnr_regs[SYSREG_R_LH_ENABLE], tnr_val);
	iounmap(tnr_sys_regs);

	return ret;
}

int blk_dns_mux_control(struct is_device_ischain *device, u32 value)
{
	void __iomem *dns_sys_regs;
	u32 dns_val = 0;
	int ret = 0;

	FIMC_BUG(!device);
	dns_sys_regs = ioremap_nocache(SYSREG_DNS_BASE_ADDR, 0x1000);

	/* DNS input mux setting */
	/* DNS0 <- TNR */
	dns_val = is_hw_get_reg(dns_sys_regs,
				&sysreg_dns_regs[SYSREG_R_DNS_USER_CON0]);
	dns_val = is_hw_set_field_value(dns_val,
				&sysreg_dns_fields[SYSREG_F_GLUEMUX_DNS0_VAL], value);

	info("SYSREG_R_DNS_USER_CON0:(0x%08X)\n", dns_val);
	is_hw_set_reg(dns_sys_regs, &sysreg_dns_regs[SYSREG_R_DNS_USER_CON0], dns_val);

	iounmap(dns_sys_regs);
	return ret;
}

int blk_dns_otfenable_control(struct is_device_ischain *device, u32 value)
{
	void __iomem *dns_sys_regs;
	u32 dns_val = 0;
	int ret = 0;

	FIMC_BUG(!device);
	dns_sys_regs = ioremap_nocache(SYSREG_DNS_BASE_ADDR, 0x1000);

	/* DNS otf enable */
	dns_val = is_hw_get_reg(dns_sys_regs,
				&sysreg_dns_regs[SYSREG_R_DNS_USER_CON1]);
	dns_val = is_hw_set_field_value(dns_val,
				&sysreg_dns_fields[SYSREG_F_Enable_OTF_OUT_CTL_DNSITP], value);
	dns_val = is_hw_set_field_value(dns_val,
				&sysreg_dns_fields[SYSREG_F_Enable_OTF_IN_CTL_ITPDNS], value);
	dns_val = is_hw_set_field_value(dns_val,
				&sysreg_dns_fields[SYSREG_F_Enable_OTF4_OUT_DNSITP], value);
	dns_val = is_hw_set_field_value(dns_val,
				&sysreg_dns_fields[SYSREG_F_Enable_OTF3_OUT_DNSITP], value);
	dns_val = is_hw_set_field_value(dns_val,
				&sysreg_dns_fields[SYSREG_F_Enable_OTF2_OUT_DNSITP], value);
	dns_val = is_hw_set_field_value(dns_val,
				&sysreg_dns_fields[SYSREG_F_Enable_OTF1_OUT_DNSITP], value);
	dns_val = is_hw_set_field_value(dns_val,
				&sysreg_dns_fields[SYSREG_F_Enable_OTF0_OUT_DNSITP], value);
	dns_val = is_hw_set_field_value(dns_val,
				&sysreg_dns_fields[SYSREG_F_Enable_OTF_IN_ITPDNS], value);
	dns_val = is_hw_set_field_value(dns_val,
				&sysreg_dns_fields[SYSREG_F_Enable_OTF_IN_TNRDNS], value);
	dns_val = is_hw_set_field_value(dns_val,
				&sysreg_dns_fields[SYSREG_F_Enable_OTF1_IN_IPPDNS], value);
	dns_val = is_hw_set_field_value(dns_val,
				&sysreg_dns_fields[SYSREG_F_Enable_OTF0_IN_IPPDNS], value);

	minfo("SYSREG_R_DNS_USER_CON1:(0x%08X)\n", device, dns_val);
	is_hw_set_reg(dns_sys_regs, &sysreg_dns_regs[SYSREG_R_DNS_USER_CON1], dns_val);

	iounmap(dns_sys_regs);
	return ret;
}

int blk_itp_otfenable_control(struct is_device_ischain *device, u32 value)
{
	void __iomem *itp_sys_regs;
	u32 itp_val = 0;
	int ret = 0;

	FIMC_BUG(!device);
	itp_sys_regs = ioremap_nocache(SYSREG_ITP_BASE_ADDR, 0x1000);

	/* Enable OTF */
	itp_val = is_hw_get_reg(itp_sys_regs,
				&sysreg_itp_regs[SYSREG_R_ITP_USER_CON]);
	itp_val = is_hw_set_field_value(itp_val,
				&sysreg_itp_fields[SYSREG_F_Enable_OTF_IN_CTL_DNSITP], value);
	itp_val = is_hw_set_field_value(itp_val,
				&sysreg_itp_fields[SYSREG_F_Enable_OTF_OUT_CTL_ITPDNS], value);
	itp_val = is_hw_set_field_value(itp_val,
				&sysreg_itp_fields[SYSREG_F_Enable_OTF4_IN_DNSITP], value);
	itp_val = is_hw_set_field_value(itp_val,
				&sysreg_itp_fields[SYSREG_F_Enable_OTF3_IN_DNSITP], value);
	itp_val = is_hw_set_field_value(itp_val,
				&sysreg_itp_fields[SYSREG_F_Enable_OTF2_IN_DNSITP], value);
	itp_val = is_hw_set_field_value(itp_val,
				&sysreg_itp_fields[SYSREG_F_Enable_OTF1_IN_DNSITP], value);
	itp_val = is_hw_set_field_value(itp_val,
				&sysreg_itp_fields[SYSREG_F_Enable_OTF0_IN_DNSITP], value);

	minfo("SYSREG_R_ITP_USER_CON:(0x%08X)\n", device, itp_val);
	is_hw_set_reg(itp_sys_regs, &sysreg_itp_regs[SYSREG_R_ITP_USER_CON], itp_val);

	iounmap(itp_sys_regs);
	return ret;
}

int is_hw_ischain_cfg(void *ischain_data)
{
	int ret = 0;
	struct is_device_ischain *device;

	FIMC_BUG(!ischain_data);

	device = (struct is_device_ischain *)ischain_data;
	if (test_bit(IS_ISCHAIN_REPROCESSING, &device->state))
		return ret;

	return ret;
}

int is_hw_ischain_enable(struct is_device_ischain *device)
{
	int ret = 0;

	FIMC_BUG(!device);

	ret = blk_dns_mux_control(device, 1);
	if (!ret)
		merr("blk_dns_mux_control is failed (%d)\n", device, ret);

	ret = blk_tnr_lh_control(device, 1);
	if (!ret)
		merr("blk_tnr_lh_control is failed (%d)\n", device, ret);

	info("%s: complete\n", __func__);

	return ret;
}

int is_hw_ischain_disable(struct is_device_ischain *device)
{
	int ret = 0;

	FIMC_BUG(!device);

	ret = blk_tnr_lh_control(device, 0);
	if (ret)
		merr("blk_tnr_lh_control is failed (%d)\n", device, ret);

	info("%s: complete\n", __func__);

	return ret;
}

/* TODO: remove this, compile check only */
#ifdef ENABLE_HWACG_CONTROL
void is_hw_csi_qchannel_enable_all(bool enable)
{
	void __iomem *csi0_regs;
	void __iomem *csi1_regs;
	void __iomem *csi2_regs;
	void __iomem *csi3_regs;
	void __iomem *csi4_regs;
	void __iomem *csi5_regs;

	u32 reg_val;

	csi0_regs = ioremap_nocache(CSIS0_QCH_EN_ADDR, SZ_4);
	csi1_regs = ioremap_nocache(CSIS1_QCH_EN_ADDR, SZ_4);
	csi2_regs = ioremap_nocache(CSIS2_QCH_EN_ADDR, SZ_4);
	csi3_regs = ioremap_nocache(CSIS3_QCH_EN_ADDR, SZ_4);
	csi4_regs = ioremap_nocache(CSIS4_QCH_EN_ADDR, SZ_4);
	csi5_regs = ioremap_nocache(CSIS5_QCH_EN_ADDR, SZ_4);

	reg_val = readl(csi0_regs);
	reg_val &= ~(1 << 20);
	writel(enable << 20 | reg_val, csi0_regs);

	reg_val = readl(csi1_regs);
	reg_val &= ~(1 << 20);
	writel(enable << 20 | reg_val, csi1_regs);

	reg_val = readl(csi2_regs);
	reg_val &= ~(1 << 20);
	writel(enable << 20 | reg_val, csi2_regs);

	reg_val = readl(csi3_regs);
	reg_val &= ~(1 << 20);
	writel(enable << 20 | reg_val, csi3_regs);

	reg_val = readl(csi4_regs);
	reg_val &= ~(1 << 20);
	writel(enable << 20 | reg_val, csi4_regs);

	reg_val = readl(csi5_regs);
	reg_val &= ~(1 << 20);
	writel(enable << 20 | reg_val, csi5_regs);

	iounmap(csi0_regs);
	iounmap(csi1_regs);
	iounmap(csi2_regs);
	iounmap(csi3_regs);
	iounmap(csi4_regs);
	iounmap(csi5_regs);
}
#endif

void is_hw_djag_adjust_out_size(struct is_device_ischain *ischain,
					u32 in_width, u32 in_height,
					u32 *out_width, u32 *out_height)
{
	struct is_global_param *g_param;
	int bratio;
	bool is_down_scale;

	if (!ischain) {
		err_hw("device is NULL");
		return;
	}

	g_param = &ischain->resourcemgr->global_param;
	is_down_scale = (*out_width < in_width) || (*out_height < in_height);
	bratio = is_sensor_g_bratio(ischain->sensor);
	if (bratio < 0) {
		err_hw("failed to get sensor_bratio");
		return;
	}

	dbg_hw(2, "%s:video_mode %d is_down_scale %d bratio %d\n", __func__,
			g_param->video_mode, is_down_scale, bratio);

	if (g_param->video_mode
		&& is_down_scale
		&& bratio >= MCSC_DJAG_ENABLE_SENSOR_BRATIO) {
		dbg_hw(2, "%s:%dx%d -> %dx%d\n", __func__,
				*out_width, *out_height, in_width, in_height);

		*out_width = in_width;
		*out_height = in_height;
	}
}

void __nocfi is_hw_interrupt_relay(struct is_group *group, void *hw_ip_data)
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
	case GROUP_ID_CLH0:
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
