/*
 * Copyright (c) 2018 Samsung Electronics Co., Ltd.
 *		http://www.samsung.com
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 */

#ifndef __EXYNOS_SCI_DBG_H_
#define __EXYNOS_SCI_DBG_H_

#include <linux/time.h>

#define EXYNOS_SCI_DBG_MODULE_NAME	"exynos-sci_dbg"
#define LLC_DSS_NAME			"log_llc"
#define BCM_ESS_NAME			"log_bcm"

#define LLC_DUMP_DATA_Q_SIZE			8
#define LLC_SLICE_END				(0x1)
#define LLC_BANK_END				(0x1)
#define LLC_SET_END				(0x1FF)
#define LLC_WAY_END				(0xF)
#define LLC_QWORD_END				(0x7)

#define SCI_BASE				0x1A000000
#define ArrDbgCntl				0x05C
#define ArrDbgRDataHi				0x06C
#define ArrDbgRDataMi				0x070
#define ArrDbgRDataLo				0x074
#define CCMControl1				0x0A8
#define PM_SCI_DBG_CTL				0x140
#define LLCControl				0x544
#define LLCId_0					0x5C0
#define LLCIdAllocLkup_0			0x5C4
#define LLCAddrMatch				0x4C0
#define LLCAddrMask				0x4C4
#define DebugSrc10_offset			0x2BC
#define DebugSrc32_offset			0x2C0
#define DebugCtrl_offset			0x2D4

#define SMC_MIF0_BASE			0x1C03F000
#define SMC_MIF1_BASE			0x1C13F000
#define SMC_MIF2_BASE			0x1C23F000
#define SMC_MIF3_BASE			0x1C33F000

#define SMC_ALL_BASE			0x1A34F000
#define SYSREG_MIF0_BASE		0x1C020000
#define SYSREG_MIF1_BASE		0x1C120000
#define SYSREG_MIF2_BASE		0x1C220000
#define SYSREG_MIF3_BASE		0x1C320000
#define PPC_DEBUG0_BASE			0x1C050000
#define PPC_DEBUG1_BASE			0x1C150000
#define PPC_DEBUG2_BASE			0x1C250000
#define PPC_DEBUG3_BASE			0x1C350000
#define PPC_DEBUG_CCI			0x1A0D0000
#define SYSREG_CORE_PPC_BASE		0x1A021000

#define TREX_IRPS0_BASE			0x1A8A4000
#define TREX_IRPS1_BASE			0x1A8B4000
#define TREX_IRPS2_BASE			0x1A8C4000
#define TREX_IRPS3_BASE			0x1A8D4000
#define LLC_USER_CONFIG_MATCH		0x400
#define LLC_USER_CONFIG_USER		0x404

#define CACHEAID_BASE			0x1A360000
#define CACHEAID_USER			0x100
#define CACHEAID_CTRL			0x104
#define CACHEAID_GLOBAL_CTRL		0x0
#define CACHEAID_DEFAULT_CTRL		0x10
#define CACHEAID_PMU_ACCESS_CTRL	0x20
#define CACHEAID_PMU_ACCESS_INFO	0x24

/* SCI_PPC_WRAPPER offset */
#define SCI_PPC_PMNC			0x4
#define SCI_PPC_CNTENS			0x8
#define SCI_PPC_INTENS			0x10
#define SCI_PPC_FLAG			0x18
#define SCI_PPC_PMCNT0_LOW		0x34
#define SCI_PPC_PMCNT1_LOW		0x38
#define SCI_PPC_PMCNT2_LOW		0x3C
#define SCI_PPC_PMCNT3_LOW		0x40
#define SCI_PPC_CCNT_LOW		0x48

/* SMC_PPC_WRAPPER offset */
#define SMC_PPC_PMNC			0x0
#define SMC_PPC_CNTENS			0x10
#define SMC_PPC_CCNT			0x100
#define SMC_PPC_PMCNT0			0x110
#define SMC_PPC_PMCNT1			0x120
#define SMC_PPC_PMCNT2			0x130
#define SMC_PPC_PMCNT3_HIGH		0x140
#define SMC_PPC_PMCNT3_LOW		0x150

/* SMC_ALL_BASE offset */
#define SMC_DBG_BLK_CTL0		0x294
#define SMC_DBG_BLK_CTL1		0x298
#define SMC_DBG_BLK_CTL2		0x29C
#define SMC_DBG_BLK_CTL3		0x2A0
#define SMC_DBG_BLK_CTL4		0x2A4
#define SMC_DBG_BLK_CTL5		0x2A8
#define SMC_DBG_BLK_CTL6		0x2AC
#define SMC_DBG_BLK_CTL7		0x2B0
#define SMC_DBG_BLK_CTL8		0x2B4
#define SMC_GLOBAL_DBG_CTL		0x290

#define LLC_En_Bit				(25)
#define DisableLlc_Bit				(9)

#define NUM_OF_SMC_DBG_BLK_CTL			(9)
#define NUM_OF_SYSREG_MIF			(4)

/* IPC common definition */
#define SCI_DBG_ONE_BIT_MASK			(0x1)
#define SCI_DBG_ERR_MASK				(0x7)
#define SCI_DBG_ERR_SHIFT				(13)

#define SCI_DBG_CMD_IDX_MASK			(0x3F)
#define SCI_DBG_CMD_IDX_SHIFT			(0)
#define SCI_DBG_DATA_MASK				(0x3F)
#define SCI_DBG_DATA_SHIFT				(6)
#define SCI_DBG_IPC_DIR_SHIFT			(12)

#define SCI_DBG_CMD_GET(cmd_data, mask, shift)	((cmd_data & (mask << shift)) >> shift)
#define SCI_DBG_CMD_CLEAR(mask, shift)		(~(mask << shift))
#define SCI_DBG_CMD_SET(data, mask, shift)		((data & mask) << shift)

#define SCI_DBG_DBGGEN
#ifdef SCI_DBG_DBGGEN
#define SCI_DBG_DBG(x...)	pr_info("sci_dbg_dbg: " x)
#else
#define SCI_DBG_DBG(x...)	do {} while (0)
#endif

#define SCI_DBG_INFO(x...)	pr_info("sci_dbg_info: " x)
#define SCI_DBG_ERR(x...)	pr_err("sci_dbg_err: " x)

struct exynos_ppc_dump_addr {
	u32				p_addr;
	u32				p_size;
};

struct exynos_sci_dbg_dump_addr {
	u32				p_addr;
	u32				p_size;
	u32				buff_size;
	void __iomem			*v_addr;
	void __iomem			*cnt_sfr_base;
	void __iomem			*trex_core_base;
	void __iomem			*sci_base;
	void __iomem			*smc_base;
	void __iomem			*sysreg_mif_base[4];
	void __iomem			*smc_mif_base[4];
	void __iomem			*ppc_dbg_base[4];
	void __iomem			*trex_irps_base[4];
	void __iomem			*debug_base;
};

struct exynos_sci_dbg_dump_data {
	u32				index;
	u64				time;
	u32				count[5];
} __attribute__((packed));

struct exynos_smc_dump_data {
	u32				index;
	u32				smc_ch;
	u64				time;
	u32				count[6];
} __attribute__((packed));

struct exynos_sci_dbg_data {
	struct device			*dev;
	spinlock_t			lock;

	struct exynos_sci_dbg_dump_addr	dump_addr;
	struct exynos_sci_dbg_dump_data	dump_data;
	bool				dump_enable;
	struct hrtimer			hrtimer;
	struct exynos_smc_dump_data	smc_dump_data[4];
	bool				smc_dump_enable;
	struct hrtimer			smc_hrtimer;

	void __iomem			*sci_base;
	void __iomem			*cacheaid_base;
};

extern void carveout_reserved_mem(void *r);
extern bool get_exynos_sci_llc_debug_mode(void);
extern void set_exynos_sci_llc_debug_mode(int enable);

extern void smc_ppc_enable(unsigned int enable);
extern void sci_ppc_enable(unsigned int enable);

extern struct platform_driver exynos_sci_dbg_driver;

#endif	/* __EXYNOS_SCI_DBG_H_ */
