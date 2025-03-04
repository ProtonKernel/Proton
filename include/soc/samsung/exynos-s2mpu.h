/*
 * Copyright (c) 2019 Samsung Electronics Co., Ltd.
 *	      http://www.samsung.com/
 *
 * EXYNOS - Stage 2 Protection Unit(S2MPU)
 * Author: Junho Choi <junhosj.choi@samsung.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#ifndef __EXYNOS_S2MPU_H__
#define __EXYNOS_S2MPU_H__

#define EXYNOS_MAX_S2MPU_INSTANCE		(64)
#define EXYNOS_MAX_S2MPU_SUBSYSTEM		(20)
#define EXYNOS_MAX_SUBSYSTEM_LEN		(10)

#define SUBSYSTEM_FW_NUM_SHIFT			(32)
#define SUBSYSTEM_INDEX_SHIFT			(0)

/* Error */
#define ERROR_INVALID_SUBSYSTEM_NAME		(0x1)
#define ERROR_DO_NOT_SUPPORT_SUBSYSTEM		(0x2)
#define ERROR_INVALID_FW_BIN_INFO		(0x3)
#define ERROR_INVALID_NOTIFIER_BLOCK		(0x4)
#define ERROR_INVALID_PARAMETER			(0x5)
#define ERROR_S2MPU_NOT_INITIALIZED		(0xE12E0001)

/* Backup and restore S2MPU */
#define EXYNOS_PD_S2MPU_BACKUP			(0)
#define EXYNOS_PD_S2MPU_RESTORE			(1)

#define MAX_NUM_OF_S2MPUFD_CHANNEL              (42)
#define MAX_VID_OF_S2MPUFD_FAULT_INFO		(8)

#define FAULT_TYPE_MPTW_ACCESS_FAULT		(0x1)
#define FAULT_TYPE_AP_FAULT			(0x2)
#define FAULT_THIS_ADDR_IS_NOT_BLACKLIST	(0xFFFF)

#define OWNER_IS_KERNEL_RO			(99)
#define OWNER_IS_TEST				(98)

/* Return values from SMC */
#define S2MPUFD_ERROR_INVALID_CH_NUM		(0x600)
#define S2MPUFD_ERROR_INVALID_FAULT_INFO_SIZE	(0x601)

/* S2MPUFD Notifier */
#define S2MPUFD_NOTIFIER_SET			(1)
#define S2MPUFD_NOTIFIER_UNSET			(0)

#define S2MPUFD_NOTIFIER_PANIC			(0xFF00000000)
#define S2MPUFD_NOTIFY_OK			(0)
#define S2MPUFD_NOTIFY_BAD			(1)

/* S2MPU enable check */
#define S2_BUF_SIZE 		(30)

#ifndef __ASSEMBLY__
/* S2MPU access permission */
enum stage2_ap {
	ATTR_NO_ACCESS = 0x0,
	ATTR_RO = 0x1,
	ATTR_WO = 0x2,
	ATTR_RW = 0x3
};

/* Registers of S2MPUFD Fault Information */
struct s2mpufd_fault_info {
	unsigned int s2mpufd_intr_cnt;
	unsigned int s2mpufd_subsystem[MAX_VID_OF_S2MPUFD_FAULT_INFO];
	unsigned int s2mpufd_fault_addr_low[MAX_VID_OF_S2MPUFD_FAULT_INFO];
	unsigned int s2mpufd_fault_addr_high[MAX_VID_OF_S2MPUFD_FAULT_INFO];
	unsigned int s2mpufd_fault_vid[MAX_VID_OF_S2MPUFD_FAULT_INFO];
	unsigned int s2mpufd_fault_type[MAX_VID_OF_S2MPUFD_FAULT_INFO];
	unsigned int s2mpufd_rw[MAX_VID_OF_S2MPUFD_FAULT_INFO];
	unsigned int s2mpufd_len[MAX_VID_OF_S2MPUFD_FAULT_INFO];
	unsigned int s2mpufd_axid[MAX_VID_OF_S2MPUFD_FAULT_INFO];
	unsigned int s2mpufd_blacklist_owner[MAX_VID_OF_S2MPUFD_FAULT_INFO];
};

/* Structure of notifier info */
struct s2mpufd_notifier_info {
	const char * subsystem;
	unsigned long fault_addr;
	unsigned int fault_rw;
	unsigned int fault_len;
	unsigned int fault_type;
};

/* Structure of notifier block */
struct s2mpufd_notifier_block {
	const char * subsystem;
	int (*notifier_call)(struct s2mpufd_notifier_block *,
			     struct s2mpufd_notifier_info *);
};

/* Data structure for S2MPUFD Fault Information */
struct s2mpufd_info_data {
	struct device *dev;
	struct s2mpufd_fault_info *fault_info;
	dma_addr_t fault_info_pa;
	unsigned int ch_num;
	unsigned int irq[MAX_NUM_OF_S2MPUFD_CHANNEL];
	unsigned int irqcnt;
	struct s2mpufd_notifier_info *noti_info;
	unsigned int *notifier_flag;
};

uint64_t exynos_set_dev_stage2_ap(const char *subsystem,
				  uint64_t base,
				  uint64_t size,
				  uint32_t ap);

uint64_t exynos_set_stage2_blacklist(const char *subsystem,
				     uint64_t base,
				     uint64_t size);

uint64_t exynos_set_all_stage2_blacklist(uint64_t base,
					 uint64_t size);

/*
 * Request Stage 2 access permission of FW to allow
 * access memory.
 *
 * This function must be called in case that only
 * sub-system FW is verified.
 *
 * @subsystem: Sub-system name
 *	       Please refer to subsystem-names property of
 *	       s2mpu node in exynosXXXX-security.dtsi.
 */
uint64_t exynos_request_fw_stage2_ap(const char *subsystem);

/*
 * Verify the signature of sub-system FW.
 *
 * @subsystem:	 Sub-system name
 *		 Please refer to subsystem-names property of
 *		 s2mpu node in exynosXXXX-security.dtsi.
 * @fw_id:	 FW ID of this subsystem
 * @fw_base:	 FW base address
 *		 It's physical address(PA) and should be aligned
 *		 with 64KB because of S2MPU granule.
 * @fw_bin_size: FW binary size
 *		 It should be aligned with 4bytes because of
 *		 the limit of signature verification.
 * @fw_mem_size: The size to be used by FW
 *		 This memory region needs to be protected
 *		 from other sub-systems.
 *		 It should be aligned with 64KB like fw_base
 *		 because of S2MPU granlue.
 */
uint64_t exynos_verify_subsystem_fw(const char *subsystem,
					uint32_t fw_id,
					uint64_t fw_base,
					size_t fw_bin_size,
					size_t fw_mem_size);
/*
 * Register subsystem's S2MPU Fault Detector notifier call.
 *
 * @s2mpufd_notifier_block:	S2MPU FD Notifier structure
 *				It should have two elements.
 *				One is Sub-system's name, 'subsystem'.
 *				The other is notifier call function pointer,
 *				s2mpufd_notifier_fn_t 'notifier_call'.
 * @'subsystem':		Please refer to subsystem-names property
 *				of s2mpu node in exynosXXXX-security.dtsi.
 * @'notifier_call'		It's type is s2mpufd_notifier_fn_t.
 *				And it should return S2MPUFD_NOTIFY
 */
uint64_t s2mpufd_notifier_call_register(struct s2mpufd_notifier_block *snb);

#ifdef CONFIG_EXYNOS_S2MPU_PD
uint64_t exynos_pd_backup_s2mpu(uint32_t pd);
uint64_t exynos_pd_restore_s2mpu(uint32_t pd);
#else
static inline uint64_t exynos_pd_backup_s2mpu(uint32_t pd)
{
	return 0;
}

static inline uint64_t exynos_pd_restore_s2mpu(uint32_t pd)
{
	return 0;
}
#endif

#endif	/* __ASSEMBLY__ */
#endif	/* __EXYNOS_S2MPU_H__ */
