/*
 *  Copyright (c) 2019 Samsung Electronics.
 *
 * A header for Hypervisor Call(HVC)
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#ifndef __EXYNOS_HVC_H__
#define __EXYNOS_HVC_H__

#include <linux/arm-smccc.h>

/* HVC FID */
#define HVC_FID_BASE					(0xC6000000)

#define HVC_FID_GET_HARX_INFO				(HVC_FID_BASE + 0x20)

#define HVC_FID_SET_EL2_CRASH_INFO_FP_BUF		(HVC_FID_BASE + 0x40)
#define HVC_FID_SET_EL2_LV3_TABLE_BUF			(HVC_FID_BASE + 0x41)
#define HVC_FID_DELIVER_LV3_ALLOC_IRQ_NUM		(HVC_FID_BASE + 0x42)
#define HVC_FID_GET_EL2_LOG_BUF				(HVC_FID_BASE + 0x43)

#define HVC_FID_SET_S2MPU				(HVC_FID_BASE + 0x100)
#define HVC_FID_SET_S2MPU_BLACKLIST			(HVC_FID_BASE + 0x101)
#define HVC_FID_REQUEST_FW_STAGE2_AP			(HVC_FID_BASE + 0x102)
#define HVC_FID_SET_ALL_S2MPU_BLACKLIST			(HVC_FID_BASE + 0x103)
#define HVC_FID_BAN_KERNEL_RO_REGION			(HVC_FID_BASE + 0x104)
#define HVC_FID_BACKUP_RESTORE_S2MPU			(HVC_FID_BASE + 0x108)
#define HVC_FID_GET_S2MPU_PD_BITMAP			(HVC_FID_BASE + 0x109)

#define HVC_FID_VERIFY_SUBSYSTEM_FW			(HVC_FID_BASE + 0x110)

#define HVC_FID_CHECK_STAGE2_ENABLE			(HVC_FID_BASE + 0x120)
#define HVC_FID_CHECK_S2MPU_ENABLE			(HVC_FID_BASE + 0x121)

#define HVC_CMD_INIT_S2MPUFD				(HVC_FID_BASE + 0x601)
#define HVC_CMD_GET_S2MPUFD_FAULT_INFO			(HVC_FID_BASE + 0x602)

/* HVC ERROR */
#define HVC_UNK						(-1)

#define HARX_ERROR_BASE					(0xE12E0000)
#define HARX_ERROR(ev)					(HARX_ERROR_BASE + (ev))

/* HVC_FID_GET_HARX_INFO */
#define HARX_INFO_MAJOR_VERSION				(1)
#define HARX_INFO_MINOR_VERSION				(0)
#define HARX_INFO_VERSION				(0xE1200000 |			\
							(HARX_INFO_MAJOR_VERSION << 8) |\
							HARX_INFO_MINOR_VERSION)

#define ERROR_INVALID_INFO_TYPE				HARX_ERROR(0x10)

/* HVC_FID_BACKUP_RESTORE_S2MPU */
#define ERROR_NO_S2MPU_IN_BLOCK				(0x2003)

#ifndef	__ASSEMBLY__
#include <linux/types.h>

/* HVC_FID_GET_HARX_INFO */
enum harx_info_type {
	HARX_INFO_TYPE_VERSION = 0,
	HARX_INFO_TYPE_HARX_BASE,
	HARX_INFO_TYPE_HARX_SIZE,
	HARX_INFO_TYPE_RAM_LOG_BASE,
	HARX_INFO_TYPE_RAM_LOG_SIZE
};

static inline unsigned long exynos_hvc(unsigned long hvc_fid,
					unsigned long arg1,
					unsigned long arg2,
					unsigned long arg3,
					unsigned long arg4)
{
	struct arm_smccc_res res;

	arm_smccc_hvc(hvc_fid, arg1, arg2, arg3, arg4, 0, 0, 0, &res);

	return res.a0;
}
#endif	/* __ASSEMBLY__ */

#endif	/* __EXYNOS_HVC_H__ */
