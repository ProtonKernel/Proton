/* SPDX-License-Identifier: GPL-2.0
 * Copyright (C) 2018 Western Digital Corporation
 */

#ifndef __UFS_SYSFS_H__
#define __UFS_SYSFS_H__

#include <linux/sysfs.h>

#include <scsi/scsi_proto.h>

#include "ufshcd.h"

void ufs_sysfs_add_nodes(struct device *dev);
void ufs_sysfs_remove_nodes(struct device *dev);
void ufs_sysfs_add_sec_nodes(struct ufs_hba *hba);
void ufs_sysfs_remove_sec_nodes(struct ufs_hba *hba);

extern const struct attribute_group ufs_sysfs_unit_descriptor_group;
extern const struct attribute_group ufs_sysfs_lun_attributes_group;

struct SEC_UFS_op_count {
	unsigned int HW_RESET_count;
#define SEC_UFS_HW_RESET	0xff00
	unsigned int link_startup_count;
	unsigned int Hibern8_enter_count;
	unsigned int Hibern8_exit_count;
	unsigned int op_err;
};

struct SEC_UFS_UIC_cmd_count {
	u8 DME_GET_err;
	u8 DME_SET_err;
	u8 DME_PEER_GET_err;
	u8 DME_PEER_SET_err;
	u8 DME_POWERON_err;
	u8 DME_POWEROFF_err;
	u8 DME_ENABLE_err;
	u8 DME_RESET_err;
	u8 DME_END_PT_RST_err;
	u8 DME_LINK_STARTUP_err;
	u8 DME_HIBER_ENTER_err;
	u8 DME_HIBER_EXIT_err;
	u8 DME_TEST_MODE_err;
	unsigned int UIC_cmd_err;
};

struct SEC_UFS_UIC_err_count {
	u8 PA_ERR_cnt;
	u8 DL_PA_INIT_ERROR_cnt;
	u8 DL_NAC_RECEIVED_ERROR_cnt;
	u8 DL_TC_REPLAY_ERROR_cnt;
	u8 NL_ERROR_cnt;
	u8 TL_ERROR_cnt;
	u8 DME_ERROR_cnt;
	unsigned int UIC_err;
};

struct SEC_UFS_Fatal_err_count {
	u8 DFE;		// Device_Fatal
	u8 CFE;		// Controller_Fatal
	u8 SBFE;	// System_Bus_Fatal
	u8 CEFE;	// Crypto_Engine_Fatal
	u8 LLE;		// Link Lost
	unsigned int Fatal_err;
};

struct SEC_UFS_UTP_count {
	u8 UTMR_query_task_count;
	u8 UTMR_abort_task_count;
	u8 UTR_read_err;
	u8 UTR_write_err;
	u8 UTR_sync_cache_err;
	u8 UTR_unmap_err;
	u8 UTR_etc_err;
	unsigned int UTP_err;
};

struct SEC_UFS_QUERY_count {
	u8 NOP_err;
	u8 R_Desc_err;
	u8 W_Desc_err;
	u8 R_Attr_err;
	u8 W_Attr_err;
	u8 R_Flag_err;
	u8 Set_Flag_err;
	u8 Clear_Flag_err;
	u8 Toggle_Flag_err;
	unsigned int Query_err;
};

struct SEC_SCSI_SENSE_count {
	unsigned int scsi_medium_err;
	unsigned int scsi_hw_err;
};

#define SEC_MAX_LBA_LOGGING     10
#define SEC_ISSUE_REGION_STEP   (250*1024/4)    /* 250MB : 1 LBA = 4KB */
struct SEC_SCSI_SENSE_err_log {
	unsigned long issue_LBA_list[SEC_MAX_LBA_LOGGING];
	unsigned int issue_LBA_count;
	u64 issue_region_map;
};

struct SEC_UFS_counting {
	struct SEC_UFS_op_count op_count;
	struct SEC_UFS_UIC_cmd_count UIC_cmd_count;
	struct SEC_UFS_UIC_err_count UIC_err_count;
	struct SEC_UFS_Fatal_err_count Fatal_err_count;
	struct SEC_UFS_UTP_count UTP_count;
	struct SEC_UFS_QUERY_count query_count;
	struct SEC_SCSI_SENSE_count sense_count;
	struct SEC_SCSI_SENSE_err_log sense_err_log;
};

void SEC_ufs_operation_check(u32 command);
void SEC_ufs_uic_error_check(struct ufs_hba *hba);
void SEC_ufs_fatal_error_check(struct ufs_hba *hba);
void SEC_ufs_utp_error_check(struct scsi_cmnd *cmd, u8 tm_cmd);
void SEC_ufs_query_error_check(struct ufs_hba *hba, enum dev_cmd_type cmd_type);
void SEC_ufs_print_err_info(struct ufs_hba *hba);
void SEC_scsi_sense_error_check(struct ufshcd_lrb *lrbp);

#define SEC_UFS_DATA_ATTR(name, fmt, args...)								\
static ssize_t SEC_UFS_##name##_show(struct device *dev, struct device_attribute *attr, char *buf)	\
{													\
	return sprintf(buf, fmt, args);									\
}													\
static DEVICE_ATTR(name, 0444, SEC_UFS_##name##_show, NULL)

#define SEC_UFS_ERR_COUNT_INC(count, max) ((count) += ((count) < (max)) ? 1 : 0)

#endif
