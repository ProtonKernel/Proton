/*
 * UFS Host Controller driver for Exynos specific extensions
 *
 * Copyright (C) 2013-2014 Samsung Electronics Co., Ltd.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 */

#ifndef _UFS_EXYNOS_H_
#define _UFS_EXYNOS_H_

#include <soc/samsung/exynos_pm_qos.h>
#include <crypto/fmp.h>
#include "ufs-vs-mmio.h"
#include "ufs-vs-regs.h"
#include "ufs-cal-if.h"

/* Version check */
#define UFS_EXYNOS_COMPAT_CAL_MMIO_VER 1
#define UFS_EXYNOS_COMPAT_CAL_IF_VER 4
#if (UFS_VS_MMIO_VER != UFS_EXYNOS_COMPAT_CAL_MMIO_VER)
#error "UFS_VS_MMIO_VER and UFS_EXYNOS_COMPAT_CAL_MMIO_VER aren't matched"
#endif
#if (UFS_CAL_IF_VER != UFS_EXYNOS_COMPAT_CAL_IF_VER)
#error "UFS_CAL_IF_VER and UFS_EXYNOS_COMPAT_CAL_IF_VER aren't matched"
#endif

#define UFS_VER_0004	4
#define UFS_VER_0005	5

#define EOM_PH_SEL_MAX         72
#define EOM_DEF_VREF_MAX       256

enum {
	UFS_S_MON_LV1 = (1 << 0),
	UFS_S_MON_LV2 = (1 << 1),
};

struct ext_cxt {
	u32 offset;
	u32 mask;
	u32 val;
};

/*
 * H_UTP_BOOST and H_FATAL_ERR arn't in here because they were just
 * defined to enable some callback functions explanation.
 */
enum exynos_host_state {
	H_DISABLED = 0,
	H_RESET = 1,
	H_LINK_UP = 2,
	H_LINK_BOOST = 3,
	H_TM_BUSY = 4,
	H_REQ_BUSY = 5,
	H_HIBERN8 = 6,
	H_SUSPEND = 7,
};

enum exynos_clk_state {
	C_OFF = 0,
	C_ON,
};

enum exynos_ufs_ext_blks {
	EXT_SYSREG = 0,
	EXT_BLK_MAX,
#define EXT_BLK_MAX 1
};

enum exynos_ufs_param_id {
	UFS_S_PARAM_EOM_VER = 0,
	UFS_S_PARAM_EOM_SZ,
	UFS_S_PARAM_EOM_OFS,
	UFS_S_PARAM_LANE,
	UFS_S_PARAM_H8_D_MS,
	UFS_S_PARAM_MON,
	UFS_S_PARAM_NUM,
};

/*unique number*/
#define UFS_UN_20_DIGITS 20
#define UFS_UN_MAX_DIGITS 21 //current max digit + 1

#define SERIAL_NUM_SIZE 7

struct ufs_vendor_dev_info {
	char unique_number[UFS_UN_MAX_DIGITS];
	u8	lifetime;
	unsigned int lc_info;
	struct ufs_hba *hba;
};

enum SEC_WB_state {
	WB_OFF = 0,
	WB_ON_READY,
	WB_OFF_READY,
	WB_ON,
	NR_WB_STATE
};

struct SEC_WB_info {
	bool			wb_support;		/* feature support and enabled */
	bool			wb_setup_done;	/* setup is done or not */
	bool			wb_off;			/* WB off or not */
	atomic_t		wb_off_cnt;		/* WB off count */

	enum SEC_WB_state	state;			/* current state */
	unsigned long		state_ts;		/* current state timestamp */

	int			up_threshold_block;	/* threshold for WB on : block(4KB) count */
	int			up_threshold_rqs;	/*               WB on : request count */
	int			down_threshold_block;	/* threshold for WB off : block count */
	int			down_threshold_rqs;	/*               WB off : request count */

	int			wb_disable_threshold_lt;	/* LT threshold that WB is not allowed */

	int			on_delay;		/* WB on delay for WB_ON_READY -> WB_ON */
	int			off_delay;		/* WB off delay for WB_OFF_READY -> WB_OFF */

	/* below values will be used when (wb_off == true) */
	int			lp_up_threshold_block;		/* threshold for WB on : block(4KB) count */
	int			lp_up_threshold_rqs;		/*               WB on : request count */
	int			lp_down_threshold_block;	/* threshold for WB off : block count */
	int			lp_down_threshold_rqs;		/*               WB off : request count */
	int			lp_on_delay;	/* on_delay multiplier when (wb_off == true) */
	int			lp_off_delay;	/* off_delay multiplier when (wb_off == true) */

	int			wb_current_block;	/* current block counts in WB_ON state */
	int			wb_current_rqs;		/*         request counts in WB_ON */

	unsigned int		wb_curr_issued_block;	/* amount issued block count during current WB_ON session */
	int			wb_curr_issued_min_block;	/* min. issued block count */
	int			wb_curr_issued_max_block;	/* max. issued block count */
	unsigned int		wb_issued_size_cnt[4];	/* volume count of amount issued block per WB_ON session */

	unsigned int		wb_total_issued_mb;	/* amount issued Write Size(MB) in all WB_ON */
};

struct exynos_ufs {
	struct device *dev;
	struct ufs_hba *hba;

	/*
	 * Do not change the order of iomem variables.
	 * Standard HCI regision is populated in core driver.
	 */
	void __iomem *reg_hci;			/* exynos-specific hci */
	void __iomem *reg_unipro;		/* unipro */
	void __iomem *reg_ufsp;			/* ufs protector */
	void __iomem *reg_phy;			/* phy */
	void __iomem *reg_cport;		/* cport */
#define NUM_OF_UFS_MMIO_REGIONS 5

	/*
	 * Do not change the order of remap variables.
	 */
	struct regmap *regmap_sys;		/* io coherency */
	struct ext_cxt cxt_phy_iso;
	struct ext_cxt cxt_iocc;
	struct ext_cxt cxt_tcxo_con;

	/*
	 * Do not change the order of clock variables
	 */
	struct clk *clk_hci;
	struct clk *clk_unipro;

	/* exynos specific state */
	enum exynos_host_state h_state;
	enum exynos_host_state h_state_prev;
	enum exynos_clk_state c_state;

	u32 mclk_rate;

	int num_rx_lanes;
	int num_tx_lanes;

	struct uic_pwr_mode req_pmd_parm;
	struct uic_pwr_mode act_pmd_parm;

	int id;
	enum smu_id		fmp;
	enum smu_id		smu;

	spinlock_t dbg_lock;
	int under_dump;

	/* Support system power mode */
	int idle_ip_index;

	/* PM QoS for stability, not for performance */
	struct exynos_pm_qos_request	pm_qos_int;
	s32			pm_qos_int_value;

	/* cal */
	struct ufs_cal_param	cal_param;

	/* performance */
	void *perf;
	struct ufs_vs_handle handle;

	spinlock_t tcxo_far_lock;
	u32 tcxo_far_owner;

	u32 peer_available_lane_rx;
	u32 peer_available_lane_tx;
	u32 available_lane_rx;
	u32 available_lane_tx;

	/*
	 * This variable is to make UFS driver's operations change
	 * for specific purposes, e.g. unit test cases, or report
	 * some information to user land.
	 */
	u32 params[UFS_S_PARAM_NUM];

	/* sysfs */
	struct kobject sysfs_kobj;

	/* async resume */
	struct work_struct resume_work;

	bool skip_flush;
	bool deep_suspended;
	unsigned int reset_cnt;
	u64 transferred_bytes;

	struct workqueue_struct *SEC_WB_workq;
	struct work_struct SEC_WB_on_work;
	struct work_struct SEC_WB_off_work;
	struct SEC_WB_info sec_wb_info;
};

#if IS_ENABLED(CONFIG_SEC_UFS_WB_FEATURE)
static inline bool ufs_sec_is_wb_allowed(struct exynos_ufs *ufs)
{
	return ufs->sec_wb_info.wb_support;
}
#endif

static inline struct exynos_ufs *to_exynos_ufs(struct ufs_hba *hba)
{
	return dev_get_platdata(hba->dev);
}

#if IS_ENABLED(CONFIG_SEC_UFS_WB_FEATURE)
#define SEC_UFS_WB_DATA_ATTR(name, fmt, member)				\
static ssize_t ufs_sec_##name##_show(struct device *dev,		\
		struct device_attribute *attr, char *buf)		\
{									\
	struct ufs_hba *hba = dev_get_drvdata(dev);			\
	struct exynos_ufs *ufs = to_exynos_ufs(hba);		\
	struct SEC_WB_info *wb_info = &ufs->sec_wb_info;		\
	return sprintf(buf, fmt, wb_info->member);			\
}									\
static ssize_t ufs_sec_##name##_store(struct device *dev,		\
		struct device_attribute *attr, const char *buf,		\
		size_t count)						\
{									\
	u32 value;							\
	struct ufs_hba *hba = dev_get_drvdata(dev);			\
	struct exynos_ufs *ufs = to_exynos_ufs(hba);		\
	struct SEC_WB_info *wb_info = &ufs->sec_wb_info;		\
									\
	if (kstrtou32(buf, 0, &value))					\
		return -EINVAL;						\
									\
	wb_info->member = value;					\
									\
	return count;							\
}									\
static DEVICE_ATTR(name, 0664, ufs_sec_##name##_show, ufs_sec_##name##_store)

#define SEC_UFS_WB_TIME_ATTR(name, fmt, member)				\
static ssize_t ufs_sec_##name##_show(struct device *dev,		\
		struct device_attribute *attr, char *buf)		\
{									\
	struct ufs_hba *hba = dev_get_drvdata(dev);			\
	struct exynos_ufs *ufs = to_exynos_ufs(hba);		\
	struct SEC_WB_info *wb_info = &ufs->sec_wb_info;		\
	return sprintf(buf, fmt, jiffies_to_msecs(wb_info->member));	\
}									\
static ssize_t ufs_sec_##name##_store(struct device *dev,		\
		struct device_attribute *attr, const char *buf,		\
		size_t count)						\
{									\
	u32 value;							\
	struct ufs_hba *hba = dev_get_drvdata(dev);			\
	struct exynos_ufs *ufs = to_exynos_ufs(hba);		\
	struct SEC_WB_info *wb_info = &ufs->sec_wb_info;		\
									\
	if (kstrtou32(buf, 0, &value))					\
		return -EINVAL;						\
									\
	wb_info->member = msecs_to_jiffies(value);			\
									\
	return count;							\
}									\
static DEVICE_ATTR(name, 0664, ufs_sec_##name##_show, ufs_sec_##name##_store)

#define SEC_UFS_WB_DATA_RO_ATTR(name, fmt, args...)                     \
static ssize_t ufs_sec_##name##_show(struct device *dev,                \
		struct device_attribute *attr, char *buf)               \
{                                                                       \
	struct ufs_hba *hba = dev_get_drvdata(dev);                     \
	struct exynos_ufs *ufs = to_exynos_ufs(hba);		\
	struct SEC_WB_info *wb_info = &ufs->sec_wb_info;               \
	return sprintf(buf, fmt, args);                                 \
}                                                                       \
static DEVICE_ATTR(name, 0444, ufs_sec_##name##_show, NULL)

#endif

int exynos_ufs_init_dbg(struct ufs_vs_handle *);
int exynos_ufs_dbg_set_lanes(struct ufs_vs_handle *,
				struct device *dev, u32);
void exynos_ufs_dump_info(struct ufs_vs_handle *, struct device *dev);
void exynos_ufs_cmd_log_start(struct ufs_vs_handle *,
				struct ufs_hba *, struct scsi_cmnd *);
void exynos_ufs_cmd_log_end(struct ufs_vs_handle *,
				struct ufs_hba *hba, int tag);
void exynos_ufs_fmp_config(struct ufs_hba *hba, bool init);

bool exynos_ufs_srpmb_get_wlun_uac(void);
void exynos_ufs_srpmb_set_wlun_uac(bool flag);
#endif /* _UFS_EXYNOS_H_ */
