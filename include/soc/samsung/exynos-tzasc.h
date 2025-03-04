/*
 * Copyright (c) 2018 Samsung Electronics Co., Ltd.
 *	      http://www.samsung.com/
 *
 * EXYNOS - TrustZone Address Space Controller(TZASC) fail detector
 * Author: Junho Choi <junhosj.choi@samsung.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#ifndef __EXYNOS_TZASC_H
#define __EXYNOS_TZASC_H

/* The maximum number of TZASC channel */
#define MAX_NUM_OF_TZASC_CHANNEL			(4)

/* SFR bits information */
/* TZASC Interrupt Status Register */
#define TZASC_INTR_STATUS_OVERLAP_MASK			(1 << 16)
#define TZASC_INTR_STATUS_OVERRUN_MASK			(1 << 8)
#define TZASC_INTR_STATUS_STAT_MASK			(1 << 1)

/* TZASC Fail Address High Register */
#define TZASC_FAIL_ADDR_HIGH_MASK			(0xFF << 0)

/* TZASC Fail Control Register */
#define TZASC_FAIL_CONTROL_DIRECTION_MASK		(1 << 24)
#define TZASC_FAIL_CONTROL_NON_SECURE_MASK		(1 << 21)
#define TZASC_FAIL_CONTROL_PRIVILEGED_MASK		(1 << 20)

/* TZASC Fail ID Register */
#define TZASC_FAIL_ID_VNET_MASK				(0xF << 24)
#define TZASC_FAIL_ID_FAIL_ID_MASK			(0x3FFFF << 0)

/* Return values from SMC */
#define TZASC_ERROR_INVALID_CH_NUM			(0xA)
#define TZASC_ERROR_INVALID_FAIL_INFO_SIZE		(0xB)

#define TZASC_NEED_FAIL_INFO_LOGGING			(0x1EED)
#define TZASC_SKIP_FAIL_INFO_LOGGING			(0x2419)

/* Flag whether fail read information is logged */
#define STR_INFO_FLAG					(0x45545A43)	/* ETZC */

#ifndef __ASSEMBLY__
/* Registers of TZASC Fail Information */
struct tzasc_fail_info {
	unsigned int tzasc_intr_stat;
	unsigned int tzasc_fail_addr_low;
	unsigned int tzasc_fail_addr_high;
	unsigned int tzasc_fail_ctrl;
	unsigned int tzasc_fail_id;
};

/* Data structure for TZASC Fail Information */
struct tzasc_info_data {
	struct device *dev;

	struct tzasc_fail_info *fail_info;
	dma_addr_t fail_info_pa;

	unsigned int ch_num;
	unsigned int interleaving;
	unsigned int privilege;

	unsigned int irq[MAX_NUM_OF_TZASC_CHANNEL];
	unsigned int irqcnt;

	unsigned int info_flag;
	int need_log;
};
#endif	/* __ASSEMBLY__ */
#endif	/* __EXYNOS_TZASC_H */
