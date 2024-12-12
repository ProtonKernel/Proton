// SPDX-License-Identifier: GPL-2.0
// Copyright (C) 2018 Western Digital Corporation

#include <linux/err.h>
#include <linux/string.h>
#include <linux/bitfield.h>
#include <asm/unaligned.h>
#include <linux/sec_debug.h>

#include "ufs.h"
#include "ufs-sysfs.h"

#if defined(CONFIG_SEC_KUNIT)
#include <kunit/mock.h>
#endif

/* Called by FS */
extern void (*ufs_debug_func)(void *);

static const char *ufschd_uic_link_state_to_string(
			enum uic_link_state state)
{
	switch (state) {
	case UIC_LINK_OFF_STATE:	return "OFF";
	case UIC_LINK_ACTIVE_STATE:	return "ACTIVE";
	case UIC_LINK_HIBERN8_STATE:	return "HIBERN8";
	case UIC_LINK_BROKEN_STATE:	return "BROKEN";
	default:			return "UNKNOWN";
	}
}

static const char *ufschd_ufs_dev_pwr_mode_to_string(
			enum ufs_dev_pwr_mode state)
{
	switch (state) {
	case UFS_ACTIVE_PWR_MODE:	return "ACTIVE";
	case UFS_SLEEP_PWR_MODE:	return "SLEEP";
	case UFS_POWERDOWN_PWR_MODE:	return "POWERDOWN";
	default:			return "UNKNOWN";
	}
}

static inline ssize_t ufs_sysfs_pm_lvl_store(struct device *dev,
					     struct device_attribute *attr,
					     const char *buf, size_t count,
					     bool rpm)
{
	struct ufs_hba *hba = dev_get_drvdata(dev);
	unsigned long flags, value;

	if (kstrtoul(buf, 0, &value))
		return -EINVAL;

	if (value >= UFS_PM_LVL_MAX)
		return -EINVAL;

	spin_lock_irqsave(hba->host->host_lock, flags);
	if (rpm)
		hba->rpm_lvl = value;
	else
		hba->spm_lvl = value;
	spin_unlock_irqrestore(hba->host->host_lock, flags);
	return count;
}

static ssize_t rpm_lvl_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct ufs_hba *hba = dev_get_drvdata(dev);

	return sprintf(buf, "%d\n", hba->rpm_lvl);
}

static ssize_t rpm_lvl_store(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t count)
{
	return ufs_sysfs_pm_lvl_store(dev, attr, buf, count, true);
}

static ssize_t rpm_target_dev_state_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct ufs_hba *hba = dev_get_drvdata(dev);

	return sprintf(buf, "%s\n", ufschd_ufs_dev_pwr_mode_to_string(
			ufs_pm_lvl_states[hba->rpm_lvl].dev_state));
}

static ssize_t rpm_target_link_state_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct ufs_hba *hba = dev_get_drvdata(dev);

	return sprintf(buf, "%s\n", ufschd_uic_link_state_to_string(
			ufs_pm_lvl_states[hba->rpm_lvl].link_state));
}

static ssize_t spm_lvl_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct ufs_hba *hba = dev_get_drvdata(dev);

	return sprintf(buf, "%d\n", hba->spm_lvl);
}

static ssize_t spm_lvl_store(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t count)
{
	return ufs_sysfs_pm_lvl_store(dev, attr, buf, count, false);
}

static ssize_t spm_target_dev_state_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct ufs_hba *hba = dev_get_drvdata(dev);

	return sprintf(buf, "%s\n", ufschd_ufs_dev_pwr_mode_to_string(
				ufs_pm_lvl_states[hba->spm_lvl].dev_state));
}

static ssize_t spm_target_link_state_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct ufs_hba *hba = dev_get_drvdata(dev);

	return sprintf(buf, "%s\n", ufschd_uic_link_state_to_string(
				ufs_pm_lvl_states[hba->spm_lvl].link_state));
}

/* Convert Auto-Hibernate Idle Timer register value to microseconds */
static int ufshcd_ahit_to_us(u32 ahit)
{
	int timer = FIELD_GET(UFSHCI_AHIBERN8_TIMER_MASK, ahit);
	int scale = FIELD_GET(UFSHCI_AHIBERN8_SCALE_MASK, ahit);

	for (; scale > 0; --scale)
		timer *= UFSHCI_AHIBERN8_SCALE_FACTOR;

	return timer;
}

/* Convert microseconds to Auto-Hibernate Idle Timer register value */
static u32 ufshcd_us_to_ahit(unsigned int timer)
{
	unsigned int scale;

	for (scale = 0; timer > UFSHCI_AHIBERN8_TIMER_MASK; ++scale)
		timer /= UFSHCI_AHIBERN8_SCALE_FACTOR;

	return FIELD_PREP(UFSHCI_AHIBERN8_TIMER_MASK, timer) |
	       FIELD_PREP(UFSHCI_AHIBERN8_SCALE_MASK, scale);
}

static ssize_t auto_hibern8_show(struct device *dev,
				 struct device_attribute *attr, char *buf)
{
	struct ufs_hba *hba = dev_get_drvdata(dev);

	if (!ufshcd_is_auto_hibern8_supported(hba))
		return -EOPNOTSUPP;

	return snprintf(buf, PAGE_SIZE, "%d\n", ufshcd_ahit_to_us(hba->ahit));
}

static ssize_t auto_hibern8_store(struct device *dev,
				  struct device_attribute *attr,
				  const char *buf, size_t count)
{
	struct ufs_hba *hba = dev_get_drvdata(dev);
	unsigned int timer;

	if (!ufshcd_is_auto_hibern8_supported(hba))
		return -EOPNOTSUPP;

	if (kstrtouint(buf, 0, &timer))
		return -EINVAL;

	if (timer > UFSHCI_AHIBERN8_MAX)
		return -EINVAL;

	ufshcd_auto_hibern8_update(hba, ufshcd_us_to_ahit(timer));

	return count;
}

static DEVICE_ATTR_RW(rpm_lvl);
static DEVICE_ATTR_RO(rpm_target_dev_state);
static DEVICE_ATTR_RO(rpm_target_link_state);
static DEVICE_ATTR_RW(spm_lvl);
static DEVICE_ATTR_RO(spm_target_dev_state);
static DEVICE_ATTR_RO(spm_target_link_state);
static DEVICE_ATTR_RW(auto_hibern8);

static struct attribute *ufs_sysfs_ufshcd_attrs[] = {
	&dev_attr_rpm_lvl.attr,
	&dev_attr_rpm_target_dev_state.attr,
	&dev_attr_rpm_target_link_state.attr,
	&dev_attr_spm_lvl.attr,
	&dev_attr_spm_target_dev_state.attr,
	&dev_attr_spm_target_link_state.attr,
	&dev_attr_auto_hibern8.attr,
	NULL
};

static const struct attribute_group ufs_sysfs_default_group = {
	.attrs = ufs_sysfs_ufshcd_attrs,
};

static ssize_t ufs_sysfs_read_desc_param(struct ufs_hba *hba,
				  enum desc_idn desc_id,
				  u8 desc_index,
				  u8 param_offset,
				  u8 *sysfs_buf,
				  u8 param_size)
{
	u8 desc_buf[8] = {0};
	int ret;

	if (param_size > 8)
		return -EINVAL;

	pm_runtime_get_sync(hba->dev);
	ret = ufshcd_read_desc_param(hba, desc_id, desc_index,
				param_offset, desc_buf, param_size);
	pm_runtime_put_sync(hba->dev);

	if (ret)
		return -EINVAL;
	switch (param_size) {
	case 1:
		ret = sprintf(sysfs_buf, "0x%02X\n", *desc_buf);
		break;
	case 2:
		ret = sprintf(sysfs_buf, "0x%04X\n",
			get_unaligned_be16(desc_buf));
		break;
	case 4:
		ret = sprintf(sysfs_buf, "0x%08X\n",
			get_unaligned_be32(desc_buf));
		break;
	case 8:
		ret = sprintf(sysfs_buf, "0x%016llX\n",
			get_unaligned_be64(desc_buf));
		break;
	}

	return ret;
}

#define UFS_DESC_PARAM(_name, _puname, _duname, _size)			\
static ssize_t _name##_show(struct device *dev,				\
	struct device_attribute *attr, char *buf)			\
{									\
	struct ufs_hba *hba = dev_get_drvdata(dev);			\
	return ufs_sysfs_read_desc_param(hba, QUERY_DESC_IDN_##_duname,	\
		0, _duname##_DESC_PARAM##_puname, buf, _size);		\
}									\
static DEVICE_ATTR_RO(_name)

#define UFS_DEVICE_DESC_PARAM(_name, _uname, _size)			\
	UFS_DESC_PARAM(_name, _uname, DEVICE, _size)

UFS_DEVICE_DESC_PARAM(device_type, _DEVICE_TYPE, 1);
UFS_DEVICE_DESC_PARAM(device_class, _DEVICE_CLASS, 1);
UFS_DEVICE_DESC_PARAM(device_sub_class, _DEVICE_SUB_CLASS, 1);
UFS_DEVICE_DESC_PARAM(protocol, _PRTCL, 1);
UFS_DEVICE_DESC_PARAM(number_of_luns, _NUM_LU, 1);
UFS_DEVICE_DESC_PARAM(number_of_wluns, _NUM_WLU, 1);
UFS_DEVICE_DESC_PARAM(boot_enable, _BOOT_ENBL, 1);
UFS_DEVICE_DESC_PARAM(descriptor_access_enable, _DESC_ACCSS_ENBL, 1);
UFS_DEVICE_DESC_PARAM(initial_power_mode, _INIT_PWR_MODE, 1);
UFS_DEVICE_DESC_PARAM(high_priority_lun, _HIGH_PR_LUN, 1);
UFS_DEVICE_DESC_PARAM(secure_removal_type, _SEC_RMV_TYPE, 1);
UFS_DEVICE_DESC_PARAM(support_security_lun, _SEC_LU, 1);
UFS_DEVICE_DESC_PARAM(bkops_termination_latency, _BKOP_TERM_LT, 1);
UFS_DEVICE_DESC_PARAM(initial_active_icc_level, _ACTVE_ICC_LVL, 1);
UFS_DEVICE_DESC_PARAM(specification_version, _SPEC_VER, 2);
UFS_DEVICE_DESC_PARAM(manufacturing_date, _MANF_DATE, 2);
UFS_DEVICE_DESC_PARAM(manufacturer_id, _MANF_ID, 2);
UFS_DEVICE_DESC_PARAM(rtt_capability, _RTT_CAP, 1);
UFS_DEVICE_DESC_PARAM(rtc_update, _FRQ_RTC, 2);
UFS_DEVICE_DESC_PARAM(ufs_features, _UFS_FEAT, 1);
UFS_DEVICE_DESC_PARAM(ffu_timeout, _FFU_TMT, 1);
UFS_DEVICE_DESC_PARAM(queue_depth, _Q_DPTH, 1);
UFS_DEVICE_DESC_PARAM(device_version, _DEV_VER, 2);
UFS_DEVICE_DESC_PARAM(number_of_secure_wpa, _NUM_SEC_WPA, 1);
UFS_DEVICE_DESC_PARAM(psa_max_data_size, _PSA_MAX_DATA, 4);
UFS_DEVICE_DESC_PARAM(psa_state_timeout, _PSA_TMT, 1);
UFS_DEVICE_DESC_PARAM(ext_feature_sup, _EXT_UFS_FEATURE_SUP, 4);
UFS_DEVICE_DESC_PARAM(wb_presv_us_en, _WB_PRESRV_USRSPC_EN, 1);
UFS_DEVICE_DESC_PARAM(wb_type, _WB_TYPE, 1);
UFS_DEVICE_DESC_PARAM(wb_shared_alloc_units, _WB_SHARED_ALLOC_UNITS, 4);

static struct attribute *ufs_sysfs_device_descriptor[] = {
	&dev_attr_device_type.attr,
	&dev_attr_device_class.attr,
	&dev_attr_device_sub_class.attr,
	&dev_attr_protocol.attr,
	&dev_attr_number_of_luns.attr,
	&dev_attr_number_of_wluns.attr,
	&dev_attr_boot_enable.attr,
	&dev_attr_descriptor_access_enable.attr,
	&dev_attr_initial_power_mode.attr,
	&dev_attr_high_priority_lun.attr,
	&dev_attr_secure_removal_type.attr,
	&dev_attr_support_security_lun.attr,
	&dev_attr_bkops_termination_latency.attr,
	&dev_attr_initial_active_icc_level.attr,
	&dev_attr_specification_version.attr,
	&dev_attr_manufacturing_date.attr,
	&dev_attr_manufacturer_id.attr,
	&dev_attr_rtt_capability.attr,
	&dev_attr_rtc_update.attr,
	&dev_attr_ufs_features.attr,
	&dev_attr_ffu_timeout.attr,
	&dev_attr_queue_depth.attr,
	&dev_attr_device_version.attr,
	&dev_attr_number_of_secure_wpa.attr,
	&dev_attr_psa_max_data_size.attr,
	&dev_attr_psa_state_timeout.attr,
	&dev_attr_ext_feature_sup.attr,
	&dev_attr_wb_presv_us_en.attr,
	&dev_attr_wb_type.attr,
	&dev_attr_wb_shared_alloc_units.attr,
	NULL,
};

static const struct attribute_group ufs_sysfs_device_descriptor_group = {
	.name = "device_descriptor",
	.attrs = ufs_sysfs_device_descriptor,
};

#define UFS_INTERCONNECT_DESC_PARAM(_name, _uname, _size)		\
	UFS_DESC_PARAM(_name, _uname, INTERCONNECT, _size)

UFS_INTERCONNECT_DESC_PARAM(unipro_version, _UNIPRO_VER, 2);
UFS_INTERCONNECT_DESC_PARAM(mphy_version, _MPHY_VER, 2);

static struct attribute *ufs_sysfs_interconnect_descriptor[] = {
	&dev_attr_unipro_version.attr,
	&dev_attr_mphy_version.attr,
	NULL,
};

static const struct attribute_group ufs_sysfs_interconnect_descriptor_group = {
	.name = "interconnect_descriptor",
	.attrs = ufs_sysfs_interconnect_descriptor,
};

#define UFS_GEOMETRY_DESC_PARAM(_name, _uname, _size)			\
	UFS_DESC_PARAM(_name, _uname, GEOMETRY, _size)

UFS_GEOMETRY_DESC_PARAM(raw_device_capacity, _DEV_CAP, 8);
UFS_GEOMETRY_DESC_PARAM(max_number_of_luns, _MAX_NUM_LUN, 1);
UFS_GEOMETRY_DESC_PARAM(segment_size, _SEG_SIZE, 4);
UFS_GEOMETRY_DESC_PARAM(allocation_unit_size, _ALLOC_UNIT_SIZE, 1);
UFS_GEOMETRY_DESC_PARAM(min_addressable_block_size, _MIN_BLK_SIZE, 1);
UFS_GEOMETRY_DESC_PARAM(optimal_read_block_size, _OPT_RD_BLK_SIZE, 1);
UFS_GEOMETRY_DESC_PARAM(optimal_write_block_size, _OPT_WR_BLK_SIZE, 1);
UFS_GEOMETRY_DESC_PARAM(max_in_buffer_size, _MAX_IN_BUF_SIZE, 1);
UFS_GEOMETRY_DESC_PARAM(max_out_buffer_size, _MAX_OUT_BUF_SIZE, 1);
UFS_GEOMETRY_DESC_PARAM(rpmb_rw_size, _RPMB_RW_SIZE, 1);
UFS_GEOMETRY_DESC_PARAM(dyn_capacity_resource_policy, _DYN_CAP_RSRC_PLC, 1);
UFS_GEOMETRY_DESC_PARAM(data_ordering, _DATA_ORDER, 1);
UFS_GEOMETRY_DESC_PARAM(max_number_of_contexts, _MAX_NUM_CTX, 1);
UFS_GEOMETRY_DESC_PARAM(sys_data_tag_unit_size, _TAG_UNIT_SIZE, 1);
UFS_GEOMETRY_DESC_PARAM(sys_data_tag_resource_size, _TAG_RSRC_SIZE, 1);
UFS_GEOMETRY_DESC_PARAM(secure_removal_types, _SEC_RM_TYPES, 1);
UFS_GEOMETRY_DESC_PARAM(memory_types, _MEM_TYPES, 2);
UFS_GEOMETRY_DESC_PARAM(sys_code_memory_max_alloc_units,
	_SCM_MAX_NUM_UNITS, 4);
UFS_GEOMETRY_DESC_PARAM(sys_code_memory_capacity_adjustment_factor,
	_SCM_CAP_ADJ_FCTR, 2);
UFS_GEOMETRY_DESC_PARAM(non_persist_memory_max_alloc_units,
	_NPM_MAX_NUM_UNITS, 4);
UFS_GEOMETRY_DESC_PARAM(non_persist_memory_capacity_adjustment_factor,
	_NPM_CAP_ADJ_FCTR, 2);
UFS_GEOMETRY_DESC_PARAM(enh1_memory_max_alloc_units,
	_ENM1_MAX_NUM_UNITS, 4);
UFS_GEOMETRY_DESC_PARAM(enh1_memory_capacity_adjustment_factor,
	_ENM1_CAP_ADJ_FCTR, 2);
UFS_GEOMETRY_DESC_PARAM(enh2_memory_max_alloc_units,
	_ENM2_MAX_NUM_UNITS, 4);
UFS_GEOMETRY_DESC_PARAM(enh2_memory_capacity_adjustment_factor,
	_ENM2_CAP_ADJ_FCTR, 2);
UFS_GEOMETRY_DESC_PARAM(enh3_memory_max_alloc_units,
	_ENM3_MAX_NUM_UNITS, 4);
UFS_GEOMETRY_DESC_PARAM(enh3_memory_capacity_adjustment_factor,
	_ENM3_CAP_ADJ_FCTR, 2);
UFS_GEOMETRY_DESC_PARAM(enh4_memory_max_alloc_units,
	_ENM4_MAX_NUM_UNITS, 4);
UFS_GEOMETRY_DESC_PARAM(enh4_memory_capacity_adjustment_factor,
	_ENM4_CAP_ADJ_FCTR, 2);
UFS_GEOMETRY_DESC_PARAM(wb_max_alloc_units, _WB_MAX_ALLOC_UNITS, 4);
UFS_GEOMETRY_DESC_PARAM(wb_max_wb_luns, _WB_MAX_WB_LUNS, 1);
UFS_GEOMETRY_DESC_PARAM(wb_buff_cap_adj, _WB_BUFF_CAP_ADJ, 1);
UFS_GEOMETRY_DESC_PARAM(wb_sup_red_type, _WB_SUP_RED_TYPE, 1);
UFS_GEOMETRY_DESC_PARAM(wb_sup_wb_type, _WB_SUP_WB_TYPE, 1);


static struct attribute *ufs_sysfs_geometry_descriptor[] = {
	&dev_attr_raw_device_capacity.attr,
	&dev_attr_max_number_of_luns.attr,
	&dev_attr_segment_size.attr,
	&dev_attr_allocation_unit_size.attr,
	&dev_attr_min_addressable_block_size.attr,
	&dev_attr_optimal_read_block_size.attr,
	&dev_attr_optimal_write_block_size.attr,
	&dev_attr_max_in_buffer_size.attr,
	&dev_attr_max_out_buffer_size.attr,
	&dev_attr_rpmb_rw_size.attr,
	&dev_attr_dyn_capacity_resource_policy.attr,
	&dev_attr_data_ordering.attr,
	&dev_attr_max_number_of_contexts.attr,
	&dev_attr_sys_data_tag_unit_size.attr,
	&dev_attr_sys_data_tag_resource_size.attr,
	&dev_attr_secure_removal_types.attr,
	&dev_attr_memory_types.attr,
	&dev_attr_sys_code_memory_max_alloc_units.attr,
	&dev_attr_sys_code_memory_capacity_adjustment_factor.attr,
	&dev_attr_non_persist_memory_max_alloc_units.attr,
	&dev_attr_non_persist_memory_capacity_adjustment_factor.attr,
	&dev_attr_enh1_memory_max_alloc_units.attr,
	&dev_attr_enh1_memory_capacity_adjustment_factor.attr,
	&dev_attr_enh2_memory_max_alloc_units.attr,
	&dev_attr_enh2_memory_capacity_adjustment_factor.attr,
	&dev_attr_enh3_memory_max_alloc_units.attr,
	&dev_attr_enh3_memory_capacity_adjustment_factor.attr,
	&dev_attr_enh4_memory_max_alloc_units.attr,
	&dev_attr_enh4_memory_capacity_adjustment_factor.attr,
	&dev_attr_wb_max_alloc_units.attr,
	&dev_attr_wb_max_wb_luns.attr,
	&dev_attr_wb_buff_cap_adj.attr,
	&dev_attr_wb_sup_red_type.attr,
	&dev_attr_wb_sup_wb_type.attr,
	NULL,
};

static const struct attribute_group ufs_sysfs_geometry_descriptor_group = {
	.name = "geometry_descriptor",
	.attrs = ufs_sysfs_geometry_descriptor,
};

#define UFS_HEALTH_DESC_PARAM(_name, _uname, _size)			\
	UFS_DESC_PARAM(_name, _uname, HEALTH, _size)

UFS_HEALTH_DESC_PARAM(eol_info, _EOL_INFO, 1);
UFS_HEALTH_DESC_PARAM(life_time_estimation_a, _LIFE_TIME_EST_A, 1);
UFS_HEALTH_DESC_PARAM(life_time_estimation_b, _LIFE_TIME_EST_B, 1);

static struct attribute *ufs_sysfs_health_descriptor[] = {
	&dev_attr_eol_info.attr,
	&dev_attr_life_time_estimation_a.attr,
	&dev_attr_life_time_estimation_b.attr,
	NULL,
};

static const struct attribute_group ufs_sysfs_health_descriptor_group = {
	.name = "health_descriptor",
	.attrs = ufs_sysfs_health_descriptor,
};

#define UFS_POWER_DESC_PARAM(_name, _uname, _index)			\
static ssize_t _name##_index##_show(struct device *dev,			\
	struct device_attribute *attr, char *buf)			\
{									\
	struct ufs_hba *hba = dev_get_drvdata(dev);			\
	return ufs_sysfs_read_desc_param(hba, QUERY_DESC_IDN_POWER, 0,	\
		PWR_DESC##_uname##_0 + _index * 2, buf, 2);		\
}									\
static DEVICE_ATTR_RO(_name##_index)

UFS_POWER_DESC_PARAM(active_icc_levels_vcc, _ACTIVE_LVLS_VCC, 0);
UFS_POWER_DESC_PARAM(active_icc_levels_vcc, _ACTIVE_LVLS_VCC, 1);
UFS_POWER_DESC_PARAM(active_icc_levels_vcc, _ACTIVE_LVLS_VCC, 2);
UFS_POWER_DESC_PARAM(active_icc_levels_vcc, _ACTIVE_LVLS_VCC, 3);
UFS_POWER_DESC_PARAM(active_icc_levels_vcc, _ACTIVE_LVLS_VCC, 4);
UFS_POWER_DESC_PARAM(active_icc_levels_vcc, _ACTIVE_LVLS_VCC, 5);
UFS_POWER_DESC_PARAM(active_icc_levels_vcc, _ACTIVE_LVLS_VCC, 6);
UFS_POWER_DESC_PARAM(active_icc_levels_vcc, _ACTIVE_LVLS_VCC, 7);
UFS_POWER_DESC_PARAM(active_icc_levels_vcc, _ACTIVE_LVLS_VCC, 8);
UFS_POWER_DESC_PARAM(active_icc_levels_vcc, _ACTIVE_LVLS_VCC, 9);
UFS_POWER_DESC_PARAM(active_icc_levels_vcc, _ACTIVE_LVLS_VCC, 10);
UFS_POWER_DESC_PARAM(active_icc_levels_vcc, _ACTIVE_LVLS_VCC, 11);
UFS_POWER_DESC_PARAM(active_icc_levels_vcc, _ACTIVE_LVLS_VCC, 12);
UFS_POWER_DESC_PARAM(active_icc_levels_vcc, _ACTIVE_LVLS_VCC, 13);
UFS_POWER_DESC_PARAM(active_icc_levels_vcc, _ACTIVE_LVLS_VCC, 14);
UFS_POWER_DESC_PARAM(active_icc_levels_vcc, _ACTIVE_LVLS_VCC, 15);
UFS_POWER_DESC_PARAM(active_icc_levels_vccq, _ACTIVE_LVLS_VCCQ, 0);
UFS_POWER_DESC_PARAM(active_icc_levels_vccq, _ACTIVE_LVLS_VCCQ, 1);
UFS_POWER_DESC_PARAM(active_icc_levels_vccq, _ACTIVE_LVLS_VCCQ, 2);
UFS_POWER_DESC_PARAM(active_icc_levels_vccq, _ACTIVE_LVLS_VCCQ, 3);
UFS_POWER_DESC_PARAM(active_icc_levels_vccq, _ACTIVE_LVLS_VCCQ, 4);
UFS_POWER_DESC_PARAM(active_icc_levels_vccq, _ACTIVE_LVLS_VCCQ, 5);
UFS_POWER_DESC_PARAM(active_icc_levels_vccq, _ACTIVE_LVLS_VCCQ, 6);
UFS_POWER_DESC_PARAM(active_icc_levels_vccq, _ACTIVE_LVLS_VCCQ, 7);
UFS_POWER_DESC_PARAM(active_icc_levels_vccq, _ACTIVE_LVLS_VCCQ, 8);
UFS_POWER_DESC_PARAM(active_icc_levels_vccq, _ACTIVE_LVLS_VCCQ, 9);
UFS_POWER_DESC_PARAM(active_icc_levels_vccq, _ACTIVE_LVLS_VCCQ, 10);
UFS_POWER_DESC_PARAM(active_icc_levels_vccq, _ACTIVE_LVLS_VCCQ, 11);
UFS_POWER_DESC_PARAM(active_icc_levels_vccq, _ACTIVE_LVLS_VCCQ, 12);
UFS_POWER_DESC_PARAM(active_icc_levels_vccq, _ACTIVE_LVLS_VCCQ, 13);
UFS_POWER_DESC_PARAM(active_icc_levels_vccq, _ACTIVE_LVLS_VCCQ, 14);
UFS_POWER_DESC_PARAM(active_icc_levels_vccq, _ACTIVE_LVLS_VCCQ, 15);
UFS_POWER_DESC_PARAM(active_icc_levels_vccq2, _ACTIVE_LVLS_VCCQ2, 0);
UFS_POWER_DESC_PARAM(active_icc_levels_vccq2, _ACTIVE_LVLS_VCCQ2, 1);
UFS_POWER_DESC_PARAM(active_icc_levels_vccq2, _ACTIVE_LVLS_VCCQ2, 2);
UFS_POWER_DESC_PARAM(active_icc_levels_vccq2, _ACTIVE_LVLS_VCCQ2, 3);
UFS_POWER_DESC_PARAM(active_icc_levels_vccq2, _ACTIVE_LVLS_VCCQ2, 4);
UFS_POWER_DESC_PARAM(active_icc_levels_vccq2, _ACTIVE_LVLS_VCCQ2, 5);
UFS_POWER_DESC_PARAM(active_icc_levels_vccq2, _ACTIVE_LVLS_VCCQ2, 6);
UFS_POWER_DESC_PARAM(active_icc_levels_vccq2, _ACTIVE_LVLS_VCCQ2, 7);
UFS_POWER_DESC_PARAM(active_icc_levels_vccq2, _ACTIVE_LVLS_VCCQ2, 8);
UFS_POWER_DESC_PARAM(active_icc_levels_vccq2, _ACTIVE_LVLS_VCCQ2, 9);
UFS_POWER_DESC_PARAM(active_icc_levels_vccq2, _ACTIVE_LVLS_VCCQ2, 10);
UFS_POWER_DESC_PARAM(active_icc_levels_vccq2, _ACTIVE_LVLS_VCCQ2, 11);
UFS_POWER_DESC_PARAM(active_icc_levels_vccq2, _ACTIVE_LVLS_VCCQ2, 12);
UFS_POWER_DESC_PARAM(active_icc_levels_vccq2, _ACTIVE_LVLS_VCCQ2, 13);
UFS_POWER_DESC_PARAM(active_icc_levels_vccq2, _ACTIVE_LVLS_VCCQ2, 14);
UFS_POWER_DESC_PARAM(active_icc_levels_vccq2, _ACTIVE_LVLS_VCCQ2, 15);

static struct attribute *ufs_sysfs_power_descriptor[] = {
	&dev_attr_active_icc_levels_vcc0.attr,
	&dev_attr_active_icc_levels_vcc1.attr,
	&dev_attr_active_icc_levels_vcc2.attr,
	&dev_attr_active_icc_levels_vcc3.attr,
	&dev_attr_active_icc_levels_vcc4.attr,
	&dev_attr_active_icc_levels_vcc5.attr,
	&dev_attr_active_icc_levels_vcc6.attr,
	&dev_attr_active_icc_levels_vcc7.attr,
	&dev_attr_active_icc_levels_vcc8.attr,
	&dev_attr_active_icc_levels_vcc9.attr,
	&dev_attr_active_icc_levels_vcc10.attr,
	&dev_attr_active_icc_levels_vcc11.attr,
	&dev_attr_active_icc_levels_vcc12.attr,
	&dev_attr_active_icc_levels_vcc13.attr,
	&dev_attr_active_icc_levels_vcc14.attr,
	&dev_attr_active_icc_levels_vcc15.attr,
	&dev_attr_active_icc_levels_vccq0.attr,
	&dev_attr_active_icc_levels_vccq1.attr,
	&dev_attr_active_icc_levels_vccq2.attr,
	&dev_attr_active_icc_levels_vccq3.attr,
	&dev_attr_active_icc_levels_vccq4.attr,
	&dev_attr_active_icc_levels_vccq5.attr,
	&dev_attr_active_icc_levels_vccq6.attr,
	&dev_attr_active_icc_levels_vccq7.attr,
	&dev_attr_active_icc_levels_vccq8.attr,
	&dev_attr_active_icc_levels_vccq9.attr,
	&dev_attr_active_icc_levels_vccq10.attr,
	&dev_attr_active_icc_levels_vccq11.attr,
	&dev_attr_active_icc_levels_vccq12.attr,
	&dev_attr_active_icc_levels_vccq13.attr,
	&dev_attr_active_icc_levels_vccq14.attr,
	&dev_attr_active_icc_levels_vccq15.attr,
	&dev_attr_active_icc_levels_vccq20.attr,
	&dev_attr_active_icc_levels_vccq21.attr,
	&dev_attr_active_icc_levels_vccq22.attr,
	&dev_attr_active_icc_levels_vccq23.attr,
	&dev_attr_active_icc_levels_vccq24.attr,
	&dev_attr_active_icc_levels_vccq25.attr,
	&dev_attr_active_icc_levels_vccq26.attr,
	&dev_attr_active_icc_levels_vccq27.attr,
	&dev_attr_active_icc_levels_vccq28.attr,
	&dev_attr_active_icc_levels_vccq29.attr,
	&dev_attr_active_icc_levels_vccq210.attr,
	&dev_attr_active_icc_levels_vccq211.attr,
	&dev_attr_active_icc_levels_vccq212.attr,
	&dev_attr_active_icc_levels_vccq213.attr,
	&dev_attr_active_icc_levels_vccq214.attr,
	&dev_attr_active_icc_levels_vccq215.attr,
	NULL,
};

static const struct attribute_group ufs_sysfs_power_descriptor_group = {
	.name = "power_descriptor",
	.attrs = ufs_sysfs_power_descriptor,
};

#define UFS_STRING_DESCRIPTOR(_name, _pname)				\
static ssize_t _name##_show(struct device *dev,				\
	struct device_attribute *attr, char *buf)			\
{									\
	u8 index;							\
	struct ufs_hba *hba = dev_get_drvdata(dev);			\
	int ret;							\
	int desc_len = QUERY_DESC_MAX_SIZE;				\
	u8 *desc_buf;							\
									\
	desc_buf = kzalloc(QUERY_DESC_MAX_SIZE, GFP_ATOMIC);		\
	if (!desc_buf)                                                  \
		return -ENOMEM;                                         \
	pm_runtime_get_sync(hba->dev);					\
	ret = ufshcd_query_descriptor_retry(hba,			\
		UPIU_QUERY_OPCODE_READ_DESC, QUERY_DESC_IDN_DEVICE,	\
		0, 0, desc_buf, &desc_len);				\
	if (ret) {							\
		ret = -EINVAL;						\
		goto out;						\
	}								\
	index = desc_buf[DEVICE_DESC_PARAM##_pname];			\
	kfree(desc_buf);						\
	desc_buf = NULL;						\
	ret = ufshcd_read_string_desc(hba, index, &desc_buf,		\
				      SD_ASCII_STD);			\
	if (ret < 0)							\
		goto out;						\
	ret = snprintf(buf, PAGE_SIZE, "%s\n", desc_buf);		\
out:									\
	pm_runtime_put_sync(hba->dev);					\
	kfree(desc_buf);						\
	return ret;							\
}									\
static DEVICE_ATTR_RO(_name)

UFS_STRING_DESCRIPTOR(manufacturer_name, _MANF_NAME);
UFS_STRING_DESCRIPTOR(product_name, _PRDCT_NAME);
UFS_STRING_DESCRIPTOR(oem_id, _OEM_ID);
UFS_STRING_DESCRIPTOR(serial_number, _SN);
UFS_STRING_DESCRIPTOR(product_revision, _PRDCT_REV);

static struct attribute *ufs_sysfs_string_descriptors[] = {
	&dev_attr_manufacturer_name.attr,
	&dev_attr_product_name.attr,
	&dev_attr_oem_id.attr,
	&dev_attr_serial_number.attr,
	&dev_attr_product_revision.attr,
	NULL,
};

static const struct attribute_group ufs_sysfs_string_descriptors_group = {
	.name = "string_descriptors",
	.attrs = ufs_sysfs_string_descriptors,
};

static inline bool ufshcd_is_wb_flags(enum flag_idn idn)
{
	return ((idn >= QUERY_FLAG_IDN_WB_EN) &&
		(idn <= QUERY_FLAG_IDN_WB_BUFF_FLUSH_DURING_HIBERN8));
}

#define UFS_FLAG(_name, _uname)						\
static ssize_t _name##_show(struct device *dev,				\
	struct device_attribute *attr, char *buf)			\
{									\
	bool flag;							\
	int ret;							\
	u8 index = 0;							\
	struct ufs_hba *hba = dev_get_drvdata(dev);			\
	if (ufshcd_is_wb_flags(QUERY_FLAG_IDN##_uname))			\
		index = ufshcd_wb_get_query_index(hba);			\
	pm_runtime_get_sync(hba->dev);					\
	ret = ufshcd_query_flag(hba, UPIU_QUERY_OPCODE_READ_FLAG,	\
			QUERY_FLAG_IDN##_uname, index, &flag);		\
	pm_runtime_put_sync(hba->dev);					\
	if (ret)							\
		return -EINVAL;						\
	return sprintf(buf, "%s\n", flag ? "true" : "false");		\
}									\
static DEVICE_ATTR_RO(_name)

UFS_FLAG(device_init, _FDEVICEINIT);
UFS_FLAG(permanent_wpe, _PERMANENT_WPE);
UFS_FLAG(power_on_wpe, _PWR_ON_WPE);
UFS_FLAG(bkops_enable, _BKOPS_EN);
UFS_FLAG(life_span_mode_enable, _LIFE_SPAN_MODE_ENABLE);
UFS_FLAG(phy_resource_removal, _FPHYRESOURCEREMOVAL);
UFS_FLAG(busy_rtc, _BUSY_RTC);
UFS_FLAG(disable_fw_update, _PERMANENTLY_DISABLE_FW_UPDATE);
UFS_FLAG(wb_enable, _WB_EN);
UFS_FLAG(wb_flush_en, _WB_BUFF_FLUSH_EN);
UFS_FLAG(wb_flush_during_h8, _WB_BUFF_FLUSH_DURING_HIBERN8);

static struct attribute *ufs_sysfs_device_flags[] = {
	&dev_attr_device_init.attr,
	&dev_attr_permanent_wpe.attr,
	&dev_attr_power_on_wpe.attr,
	&dev_attr_bkops_enable.attr,
	&dev_attr_life_span_mode_enable.attr,
	&dev_attr_phy_resource_removal.attr,
	&dev_attr_busy_rtc.attr,
	&dev_attr_disable_fw_update.attr,
	&dev_attr_wb_enable.attr,
	&dev_attr_wb_flush_en.attr,
	&dev_attr_wb_flush_during_h8.attr,
	NULL,
};

static const struct attribute_group ufs_sysfs_flags_group = {
	.name = "flags",
	.attrs = ufs_sysfs_device_flags,
};

static inline bool ufshcd_is_wb_attrs(enum attr_idn idn)
{
	return ((idn >= QUERY_ATTR_IDN_WB_FLUSH_STATUS) &&
		(idn <= QUERY_ATTR_IDN_CURR_WB_BUFF_SIZE));
}

#define UFS_ATTRIBUTE(_name, _uname)					\
static ssize_t _name##_show(struct device *dev,				\
	struct device_attribute *attr, char *buf)			\
{									\
	struct ufs_hba *hba = dev_get_drvdata(dev);			\
	u32 value;							\
	u8 index = 0;							\
	int ret;							\
	if (ufshcd_is_wb_attrs(QUERY_ATTR_IDN##_uname))			\
		index = ufshcd_wb_get_query_index(hba);			\
	pm_runtime_get_sync(hba->dev);					\
	ret = ufshcd_query_attr(hba, UPIU_QUERY_OPCODE_READ_ATTR,	\
			QUERY_ATTR_IDN##_uname, index, 0, &value);	\
	pm_runtime_put_sync(hba->dev);					\
	if (ret)							\
		return -EINVAL;						\
	return sprintf(buf, "0x%08X\n", value);				\
}									\
static DEVICE_ATTR_RO(_name)

UFS_ATTRIBUTE(boot_lun_enabled, _BOOT_LU_EN);
UFS_ATTRIBUTE(current_power_mode, _POWER_MODE);
UFS_ATTRIBUTE(active_icc_level, _ACTIVE_ICC_LVL);
UFS_ATTRIBUTE(ooo_data_enabled, _OOO_DATA_EN);
UFS_ATTRIBUTE(bkops_status, _BKOPS_STATUS);
UFS_ATTRIBUTE(purge_status, _PURGE_STATUS);
UFS_ATTRIBUTE(max_data_in_size, _MAX_DATA_IN);
UFS_ATTRIBUTE(max_data_out_size, _MAX_DATA_OUT);
UFS_ATTRIBUTE(reference_clock_frequency, _REF_CLK_FREQ);
UFS_ATTRIBUTE(configuration_descriptor_lock, _CONF_DESC_LOCK);
UFS_ATTRIBUTE(max_number_of_rtt, _MAX_NUM_OF_RTT);
UFS_ATTRIBUTE(exception_event_control, _EE_CONTROL);
UFS_ATTRIBUTE(exception_event_status, _EE_STATUS);
UFS_ATTRIBUTE(ffu_status, _FFU_STATUS);
UFS_ATTRIBUTE(psa_state, _PSA_STATE);
UFS_ATTRIBUTE(psa_data_size, _PSA_DATA_SIZE);
UFS_ATTRIBUTE(wb_flush_status, _WB_FLUSH_STATUS);
UFS_ATTRIBUTE(wb_avail_buf, _AVAIL_WB_BUFF_SIZE);
UFS_ATTRIBUTE(wb_life_time_est, _WB_BUFF_LIFE_TIME_EST);
UFS_ATTRIBUTE(wb_cur_buf, _CURR_WB_BUFF_SIZE);


static struct attribute *ufs_sysfs_attributes[] = {
	&dev_attr_boot_lun_enabled.attr,
	&dev_attr_current_power_mode.attr,
	&dev_attr_active_icc_level.attr,
	&dev_attr_ooo_data_enabled.attr,
	&dev_attr_bkops_status.attr,
	&dev_attr_purge_status.attr,
	&dev_attr_max_data_in_size.attr,
	&dev_attr_max_data_out_size.attr,
	&dev_attr_reference_clock_frequency.attr,
	&dev_attr_configuration_descriptor_lock.attr,
	&dev_attr_max_number_of_rtt.attr,
	&dev_attr_exception_event_control.attr,
	&dev_attr_exception_event_status.attr,
	&dev_attr_ffu_status.attr,
	&dev_attr_psa_state.attr,
	&dev_attr_psa_data_size.attr,
	&dev_attr_wb_flush_status.attr,
	&dev_attr_wb_avail_buf.attr,
	&dev_attr_wb_life_time_est.attr,
	&dev_attr_wb_cur_buf.attr,
	NULL,
};

static const struct attribute_group ufs_sysfs_attributes_group = {
	.name = "attributes",
	.attrs = ufs_sysfs_attributes,
};

static const struct attribute_group *ufs_sysfs_groups[] = {
	&ufs_sysfs_default_group,
	&ufs_sysfs_device_descriptor_group,
	&ufs_sysfs_interconnect_descriptor_group,
	&ufs_sysfs_geometry_descriptor_group,
	&ufs_sysfs_health_descriptor_group,
	&ufs_sysfs_power_descriptor_group,
	&ufs_sysfs_string_descriptors_group,
	&ufs_sysfs_flags_group,
	&ufs_sysfs_attributes_group,
	NULL,
};

#define UFS_LUN_DESC_PARAM(_pname, _puname, _duname, _size)		\
static ssize_t _pname##_show(struct device *dev,			\
	struct device_attribute *attr, char *buf)			\
{									\
	struct scsi_device *sdev = to_scsi_device(dev);			\
	struct ufs_hba *hba = shost_priv(sdev->host);			\
	u8 lun = ufshcd_scsi_to_upiu_lun(sdev->lun);			\
	if (!ufs_is_valid_unit_desc_lun(lun))				\
		return -EINVAL;						\
	return ufs_sysfs_read_desc_param(hba, QUERY_DESC_IDN_##_duname,	\
		lun, _duname##_DESC_PARAM##_puname, buf, _size);	\
}									\
static DEVICE_ATTR_RO(_pname)

#define UFS_UNIT_DESC_PARAM(_name, _uname, _size)			\
	UFS_LUN_DESC_PARAM(_name, _uname, UNIT, _size)

UFS_UNIT_DESC_PARAM(boot_lun_id, _BOOT_LUN_ID, 1);
UFS_UNIT_DESC_PARAM(lun_write_protect, _LU_WR_PROTECT, 1);
UFS_UNIT_DESC_PARAM(lun_queue_depth, _LU_Q_DEPTH, 1);
UFS_UNIT_DESC_PARAM(psa_sensitive, _PSA_SENSITIVE, 1);
UFS_UNIT_DESC_PARAM(lun_memory_type, _MEM_TYPE, 1);
UFS_UNIT_DESC_PARAM(data_reliability, _DATA_RELIABILITY, 1);
UFS_UNIT_DESC_PARAM(logical_block_size, _LOGICAL_BLK_SIZE, 1);
UFS_UNIT_DESC_PARAM(logical_block_count, _LOGICAL_BLK_COUNT, 8);
UFS_UNIT_DESC_PARAM(erase_block_size, _ERASE_BLK_SIZE, 4);
UFS_UNIT_DESC_PARAM(provisioning_type, _PROVISIONING_TYPE, 1);
UFS_UNIT_DESC_PARAM(physical_memory_resourse_count, _PHY_MEM_RSRC_CNT, 8);
UFS_UNIT_DESC_PARAM(context_capabilities, _CTX_CAPABILITIES, 2);
UFS_UNIT_DESC_PARAM(large_unit_granularity, _LARGE_UNIT_SIZE_M1, 1);
UFS_UNIT_DESC_PARAM(wb_buf_alloc_units, _WB_BUF_ALLOC_UNITS, 4);


static struct attribute *ufs_sysfs_unit_descriptor[] = {
	&dev_attr_boot_lun_id.attr,
	&dev_attr_lun_write_protect.attr,
	&dev_attr_lun_queue_depth.attr,
	&dev_attr_psa_sensitive.attr,
	&dev_attr_lun_memory_type.attr,
	&dev_attr_data_reliability.attr,
	&dev_attr_logical_block_size.attr,
	&dev_attr_logical_block_count.attr,
	&dev_attr_erase_block_size.attr,
	&dev_attr_provisioning_type.attr,
	&dev_attr_physical_memory_resourse_count.attr,
	&dev_attr_context_capabilities.attr,
	&dev_attr_large_unit_granularity.attr,
	&dev_attr_wb_buf_alloc_units.attr,
	NULL,
};

const struct attribute_group ufs_sysfs_unit_descriptor_group = {
	.name = "unit_descriptor",
	.attrs = ufs_sysfs_unit_descriptor,
};

static ssize_t dyn_cap_needed_attribute_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	u32 value;
	struct scsi_device *sdev = to_scsi_device(dev);
	struct ufs_hba *hba = shost_priv(sdev->host);
	u8 lun = ufshcd_scsi_to_upiu_lun(sdev->lun);
	int ret;

	pm_runtime_get_sync(hba->dev);
	ret = ufshcd_query_attr(hba, UPIU_QUERY_OPCODE_READ_ATTR,
			QUERY_ATTR_IDN_DYN_CAP_NEEDED, lun, 0, &value);
	pm_runtime_put_sync(hba->dev);
	if (ret)
		return -EINVAL;

	return sprintf(buf, "0x%08X\n", value);
}
static DEVICE_ATTR_RO(dyn_cap_needed_attribute);

static struct attribute *ufs_sysfs_lun_attributes[] = {
	&dev_attr_dyn_cap_needed_attribute.attr,
	NULL,
};

const struct attribute_group ufs_sysfs_lun_attributes_group = {
	.attrs = ufs_sysfs_lun_attributes,
};

/* UFSHCD UIC layer error flags */
enum {
	UFSHCD_UIC_DL_PA_INIT_ERROR = (1 << 0), /* Data link layer error */
	UFSHCD_UIC_DL_NAC_RECEIVED_ERROR = (1 << 1), /* Data link layer error */
	UFSHCD_UIC_DL_TCx_REPLAY_ERROR = (1 << 2), /* Data link layer error */
	UFSHCD_UIC_NL_ERROR = (1 << 3), /* Network layer error */
	UFSHCD_UIC_TL_ERROR = (1 << 4), /* Transport Layer error */
	UFSHCD_UIC_DME_ERROR = (1 << 5), /* DME error */
	UFSHCD_UIC_DL_ERROR = (1 << 6), /* Data link layer error */
};

#if !defined(CONFIG_SEC_KUNIT)
static struct SEC_UFS_counting SEC_err_info;
#else
__visible_for_testing struct SEC_UFS_counting SEC_err_info;
#endif

void SEC_ufs_print_err_info(struct ufs_hba *hba)
{
	struct SEC_UFS_counting *err_info = &SEC_err_info;
	struct SEC_UFS_op_count *op_cnt = &(err_info->op_count);
	struct SEC_UFS_UIC_err_count *uic_err_cnt = &(err_info->UIC_err_count);
	struct SEC_UFS_UTP_count *utp_err = &(err_info->UTP_count);
	struct SEC_UFS_QUERY_count *query_cnt = &(err_info->query_count);
	struct SEC_SCSI_SENSE_count *sense_err = &(err_info->sense_count);

	dev_err(hba->dev, "Count: %u UIC: %u UTP: %u QUERY: %u\n",
		op_cnt->HW_RESET_count,
		uic_err_cnt->UIC_err,
		utp_err->UTP_err,
		query_cnt->Query_err);

	dev_err(hba->dev, "Sense Key: medium: %u, hw: %u\n",
		sense_err->scsi_medium_err, sense_err->scsi_hw_err);
}

/**
 * ufs_sec_send_errinfo - Send UFS Error Information to AP
 * Format : U0H0L0X0Q0R0W0F0
 * U : UTP cmd ERRor count
 * H : HWRESET count
 * L : Link startup failure count
 * X : Link Lost Error count
 * Q : UTMR QUERY_TASK error count
 * R : READ error count
 * W : WRITE error count
 * F : Device Fatal Error count
 **/
void SEC_ufs_send_err_info(void *data)
{
	struct SEC_UFS_counting *err_info = &SEC_err_info;
	char buf[23];

	if (err_info) {
		sprintf(buf, "U%uH%uL%uX%uQ%uR%uW%uF%uSM%uSH%u",
				(err_info->UTP_count.UTP_err > 9)	/* UTP Error */
				? 9 : err_info->UTP_count.UTP_err,
				(err_info->op_count.HW_RESET_count > 9)	/* HW Reset */
				? 9 : err_info->op_count.HW_RESET_count,
				(err_info->op_count.link_startup_count > 9)	/* Link Startup Fail */
				? 9 : err_info->op_count.link_startup_count,
				(err_info->Fatal_err_count.LLE > 9)		/* Link Lost */
				? 9 : err_info->Fatal_err_count.LLE,
				(err_info->UTP_count.UTMR_query_task_count > 9)	/* Query task */
				? 9 : err_info->UTP_count.UTMR_query_task_count,
				(err_info->UTP_count.UTR_read_err > 9)	/* UTRR */
				? 9 : err_info->UTP_count.UTR_read_err,
				(err_info->UTP_count.UTR_write_err > 9)	/* UTRW */
				? 9 : err_info->UTP_count.UTR_write_err,
				(err_info->Fatal_err_count.DFE > 9)	/* Device Fatal Error */
				? 9 : err_info->Fatal_err_count.DFE,
				(err_info->sense_count.scsi_medium_err > 9)		/* Device Medium error */
				? 9 : err_info->sense_count.scsi_medium_err,
				(err_info->sense_count.scsi_hw_err > 9)			/* Device HW error */
				? 9 : err_info->sense_count.scsi_hw_err);
		printk(KERN_WARNING "%s: Send UFS information to AP : %s\n", __func__, buf);

//#ifdef CONFIG_SEC_DEBUG_EXTRA_INFO
//		secdbg_exin_set_ufs_error(buf);
//#endif
	}
}

void SEC_ufs_operation_check(u32 command)
{
	struct SEC_UFS_counting *err_info = &SEC_err_info;
	struct SEC_UFS_op_count *op_cnt = &(err_info->op_count);

	switch (command) {
	case SEC_UFS_HW_RESET:
		SEC_UFS_ERR_COUNT_INC(op_cnt->HW_RESET_count, UINT_MAX);
		break;
	case UIC_CMD_DME_LINK_STARTUP:
		SEC_UFS_ERR_COUNT_INC(op_cnt->link_startup_count, UINT_MAX);
		break;
	case UIC_CMD_DME_HIBER_ENTER:
		SEC_UFS_ERR_COUNT_INC(op_cnt->Hibern8_enter_count, UINT_MAX);
		break;
	case UIC_CMD_DME_HIBER_EXIT:
		SEC_UFS_ERR_COUNT_INC(op_cnt->Hibern8_exit_count, UINT_MAX);
		break;
	default:
		break;
	}
	SEC_UFS_ERR_COUNT_INC(op_cnt->op_err, UINT_MAX);
}

void SEC_ufs_uic_error_check(struct ufs_hba *hba)
{
	struct SEC_UFS_counting *err_info = &SEC_err_info;
	struct uic_command *uic_cmd = hba->active_uic_cmd;
	struct SEC_UFS_UIC_cmd_count *uic_cmd_cnt = &(err_info->UIC_cmd_count);
	struct SEC_UFS_UIC_err_count *uic_err_cnt = &(err_info->UIC_err_count);
	u32 uic_error = hba->uic_error;

	if (!uic_cmd)	/* No UIC CMD and UIC layer error is reported */
		goto uic_layer_error;

	// uic CMD error count logging
	switch (uic_cmd->command & COMMAND_OPCODE_MASK) {
	case UIC_CMD_DME_GET:
		SEC_UFS_ERR_COUNT_INC(uic_cmd_cnt->DME_GET_err, U8_MAX);
		break;
	case UIC_CMD_DME_SET:
		SEC_UFS_ERR_COUNT_INC(uic_cmd_cnt->DME_SET_err, U8_MAX);
		break;
	case UIC_CMD_DME_PEER_GET:
		SEC_UFS_ERR_COUNT_INC(uic_cmd_cnt->DME_PEER_GET_err, U8_MAX);
		break;
	case UIC_CMD_DME_PEER_SET:
		SEC_UFS_ERR_COUNT_INC(uic_cmd_cnt->DME_PEER_SET_err, U8_MAX);
		break;
	case UIC_CMD_DME_POWERON:
		SEC_UFS_ERR_COUNT_INC(uic_cmd_cnt->DME_POWERON_err, U8_MAX);
		break;
	case UIC_CMD_DME_POWEROFF:
		SEC_UFS_ERR_COUNT_INC(uic_cmd_cnt->DME_POWEROFF_err, U8_MAX);
		break;
	case UIC_CMD_DME_ENABLE:
		SEC_UFS_ERR_COUNT_INC(uic_cmd_cnt->DME_ENABLE_err, U8_MAX);
		break;
	case UIC_CMD_DME_RESET:
		SEC_UFS_ERR_COUNT_INC(uic_cmd_cnt->DME_RESET_err, U8_MAX);
		break;
	case UIC_CMD_DME_END_PT_RST:
		SEC_UFS_ERR_COUNT_INC(uic_cmd_cnt->DME_END_PT_RST_err, U8_MAX);
		break;
	case UIC_CMD_DME_LINK_STARTUP:
		SEC_UFS_ERR_COUNT_INC(uic_cmd_cnt->DME_LINK_STARTUP_err, U8_MAX);
		break;
	case UIC_CMD_DME_HIBER_ENTER:
		SEC_UFS_ERR_COUNT_INC(uic_cmd_cnt->DME_HIBER_ENTER_err, U8_MAX);
		break;
	case UIC_CMD_DME_HIBER_EXIT:
		SEC_UFS_ERR_COUNT_INC(uic_cmd_cnt->DME_HIBER_EXIT_err, U8_MAX);
		break;
	case UIC_CMD_DME_TEST_MODE:
		SEC_UFS_ERR_COUNT_INC(uic_cmd_cnt->DME_TEST_MODE_err, U8_MAX);
		break;
	default:
		break;
	}

	if (uic_cmd->command & COMMAND_OPCODE_MASK)
		SEC_UFS_ERR_COUNT_INC(uic_cmd_cnt->UIC_cmd_err, UINT_MAX);
uic_layer_error:
	// hba->uic_error; // uic error info
	// check and count each bit
	if (uic_error & UFSHCD_UIC_DL_PA_INIT_ERROR) {
		SEC_UFS_ERR_COUNT_INC(uic_err_cnt->DL_PA_INIT_ERROR_cnt, U8_MAX);
		SEC_UFS_ERR_COUNT_INC(uic_err_cnt->UIC_err, UINT_MAX);
	}
	if (uic_error & UFSHCD_UIC_DL_NAC_RECEIVED_ERROR) {
		SEC_UFS_ERR_COUNT_INC(uic_err_cnt->DL_NAC_RECEIVED_ERROR_cnt, U8_MAX);
		SEC_UFS_ERR_COUNT_INC(uic_err_cnt->UIC_err, UINT_MAX);
	}
	if (uic_error & UFSHCD_UIC_DL_TCx_REPLAY_ERROR) {
		SEC_UFS_ERR_COUNT_INC(uic_err_cnt->DL_TC_REPLAY_ERROR_cnt, U8_MAX);
		SEC_UFS_ERR_COUNT_INC(uic_err_cnt->UIC_err, UINT_MAX);
	}
	if (uic_error & UFSHCD_UIC_NL_ERROR) {
		SEC_UFS_ERR_COUNT_INC(uic_err_cnt->NL_ERROR_cnt, U8_MAX);
		SEC_UFS_ERR_COUNT_INC(uic_err_cnt->UIC_err, UINT_MAX);
	}
	if (uic_error & UFSHCD_UIC_TL_ERROR) {
		SEC_UFS_ERR_COUNT_INC(uic_err_cnt->TL_ERROR_cnt, U8_MAX);
		SEC_UFS_ERR_COUNT_INC(uic_err_cnt->UIC_err, UINT_MAX);
	}
	if (uic_error & UFSHCD_UIC_DME_ERROR) {
		SEC_UFS_ERR_COUNT_INC(uic_err_cnt->DME_ERROR_cnt, U8_MAX);
		SEC_UFS_ERR_COUNT_INC(uic_err_cnt->UIC_err, UINT_MAX);
	}
}

void SEC_ufs_fatal_error_check(struct ufs_hba *hba)
{
	struct SEC_UFS_counting *err_info = &SEC_err_info;
	struct SEC_UFS_Fatal_err_count *fatal_err_cnt = &(err_info->Fatal_err_count);

	// hba->error; // check and count each bit
	if (hba->errors & DEVICE_FATAL_ERROR) {
		SEC_UFS_ERR_COUNT_INC(fatal_err_cnt->DFE, U8_MAX);
		SEC_UFS_ERR_COUNT_INC(fatal_err_cnt->Fatal_err, UINT_MAX);
	}
	if (hba->errors & CONTROLLER_FATAL_ERROR) {
		SEC_UFS_ERR_COUNT_INC(fatal_err_cnt->CFE, U8_MAX);
		SEC_UFS_ERR_COUNT_INC(fatal_err_cnt->Fatal_err, UINT_MAX);
	}
	if (hba->errors & SYSTEM_BUS_FATAL_ERROR) {
		SEC_UFS_ERR_COUNT_INC(fatal_err_cnt->SBFE, U8_MAX);
		SEC_UFS_ERR_COUNT_INC(fatal_err_cnt->Fatal_err, UINT_MAX);
	}
	if (hba->errors & CRYPTO_ENGINE_FATAL_ERROR) {
		SEC_UFS_ERR_COUNT_INC(fatal_err_cnt->CEFE, U8_MAX);
		SEC_UFS_ERR_COUNT_INC(fatal_err_cnt->Fatal_err, UINT_MAX);
	}
	if (hba->errors & UIC_LINK_LOST) {
		SEC_UFS_ERR_COUNT_INC(fatal_err_cnt->LLE, U8_MAX);
		SEC_UFS_ERR_COUNT_INC(fatal_err_cnt->Fatal_err, UINT_MAX);
	}
}

void SEC_ufs_utp_error_check(struct scsi_cmnd *cmd, u8 tm_cmd)
{
	struct SEC_UFS_counting *err_info = &SEC_err_info;
	struct SEC_UFS_UTP_count *utp_err = &(err_info->UTP_count);
	int opcode = 0;
	
	switch (tm_cmd) {
	case UFS_QUERY_TASK:
		SEC_UFS_ERR_COUNT_INC(utp_err->UTMR_query_task_count, U8_MAX);
		break;
	case UFS_ABORT_TASK:
		SEC_UFS_ERR_COUNT_INC(utp_err->UTMR_abort_task_count, U8_MAX);
		break;
	default:
		break;
	}

	if (!cmd)
		goto out;

	opcode = cmd->cmnd[0];
	switch (opcode) {
	case WRITE_10:
		SEC_UFS_ERR_COUNT_INC(utp_err->UTR_write_err, U8_MAX);
		break;
	case READ_10:
	case READ_16:
		SEC_UFS_ERR_COUNT_INC(utp_err->UTR_read_err, U8_MAX);
		break;
	case SYNCHRONIZE_CACHE:
		SEC_UFS_ERR_COUNT_INC(utp_err->UTR_sync_cache_err, U8_MAX);
		break;
	case UNMAP:
		SEC_UFS_ERR_COUNT_INC(utp_err->UTR_unmap_err, U8_MAX);
		break;
	default:
		SEC_UFS_ERR_COUNT_INC(utp_err->UTR_etc_err, U8_MAX);
		break;
	}

out:
	if (tm_cmd || opcode)
		SEC_UFS_ERR_COUNT_INC(utp_err->UTP_err, UINT_MAX);
}

void SEC_ufs_query_error_check(struct ufs_hba *hba, enum dev_cmd_type cmd_type)
{
	struct SEC_UFS_counting *err_info = &SEC_err_info;
	struct SEC_UFS_QUERY_count *query_cnt = &(err_info->query_count);
	struct ufs_query_req *request = &hba->dev_cmd.query.request;
	enum query_opcode opcode = request->upiu_req.opcode;

	if (cmd_type == DEV_CMD_TYPE_NOP) {
		SEC_UFS_ERR_COUNT_INC(query_cnt->NOP_err, U8_MAX);
	} else {
		switch (opcode) {
		case UPIU_QUERY_OPCODE_READ_DESC:
			SEC_UFS_ERR_COUNT_INC(query_cnt->R_Desc_err, U8_MAX);
			break;
		case UPIU_QUERY_OPCODE_WRITE_DESC:
			SEC_UFS_ERR_COUNT_INC(query_cnt->W_Desc_err, U8_MAX);
			break;
		case UPIU_QUERY_OPCODE_READ_ATTR:
			SEC_UFS_ERR_COUNT_INC(query_cnt->R_Attr_err, U8_MAX);
			break;
		case UPIU_QUERY_OPCODE_WRITE_ATTR:
			SEC_UFS_ERR_COUNT_INC(query_cnt->W_Attr_err, U8_MAX);
			break;
		case UPIU_QUERY_OPCODE_READ_FLAG:
			SEC_UFS_ERR_COUNT_INC(query_cnt->R_Flag_err, U8_MAX);
			break;
		case UPIU_QUERY_OPCODE_SET_FLAG:
			SEC_UFS_ERR_COUNT_INC(query_cnt->Set_Flag_err, U8_MAX);
			break;
		case UPIU_QUERY_OPCODE_CLEAR_FLAG:
			SEC_UFS_ERR_COUNT_INC(query_cnt->Clear_Flag_err, U8_MAX);
			break;
		case UPIU_QUERY_OPCODE_TOGGLE_FLAG:
			SEC_UFS_ERR_COUNT_INC(query_cnt->Toggle_Flag_err, U8_MAX);
			break;
		default:
			break;
		}
	}

	if ((cmd_type == DEV_CMD_TYPE_NOP) || opcode)
		SEC_UFS_ERR_COUNT_INC(query_cnt->Query_err, UINT_MAX);
}

void SEC_scsi_sense_error_check(struct ufshcd_lrb *lrbp)
{
	struct SEC_UFS_counting *err_info = &SEC_err_info;
	struct SEC_SCSI_SENSE_count *sense_err = &(err_info->sense_count);
	struct SEC_SCSI_SENSE_err_log *sense_err_log = &(err_info->sense_err_log);
	int sense_key = (0x0F & lrbp->sense_buffer[2]);
	struct scsi_cmnd *cmd;
	unsigned long lba = 0;
	unsigned long region_bit = 0;
	unsigned int lba_count = sense_err_log->issue_LBA_count;
	int i = 0;

	if (sense_key == 0x03) {
		sense_err->scsi_medium_err++;
		if (lrbp->cmd)
			cmd = lrbp->cmd;
		else
			return;

		if (cmd->device->lun == 0) {
			lba = (cmd->cmnd[2] << 24) | (cmd->cmnd[3] << 16) |
				(cmd->cmnd[4] << 8) | (cmd->cmnd[5] << 0);

			if (lba_count < SEC_MAX_LBA_LOGGING) {
				for (i = 0; i < SEC_MAX_LBA_LOGGING; i++) {
					if (sense_err_log->issue_LBA_list[i] == lba)
						return;
				}
				sense_err_log->issue_LBA_list[lba_count] = lba;
				sense_err_log->issue_LBA_count++;
			}

			region_bit = lba / SEC_ISSUE_REGION_STEP;
			if (region_bit > 51)
				region_bit = 52;
		} else if (cmd->device->lun < SCSI_W_LUN_BASE) {
			region_bit = (unsigned long)(64 - cmd->device->lun);
		}

		sense_err_log->issue_region_map |= ((u64)1 << region_bit);
	} else if (sense_key == 0x04)
		sense_err->scsi_hw_err++;
}

SEC_UFS_DATA_ATTR(SEC_UFS_op_cnt, "\"HWRESET\":\"%u\",\"LINKFAIL\":\"%u\",\"H8ENTERFAIL\":\"%u\",\"H8EXITFAIL\":\"%u\"\n",
		SEC_err_info.op_count.HW_RESET_count, SEC_err_info.op_count.link_startup_count,
		SEC_err_info.op_count.Hibern8_enter_count, SEC_err_info.op_count.Hibern8_exit_count);

SEC_UFS_DATA_ATTR(SEC_UFS_uic_cmd_cnt, "\"TESTMODE\":\"%u\",\"DME_GET\":\"%u\",\"DME_SET\":\"%u\""
		",\"DME_PGET\":\"%u\",\"DME_PSET\":\"%u\",\"PWRON\":\"%u\",\"PWROFF\":\"%u\""
		",\"DME_EN\":\"%u\",\"DME_RST\":\"%u\",\"EPRST\":\"%u\",\"LINKSTARTUP\":\"%u\""
		",\"H8ENTER\":\"%u\",\"H8EXIT\":\"%u\"\n",
		SEC_err_info.UIC_cmd_count.DME_TEST_MODE_err,	// TEST_MODE
		SEC_err_info.UIC_cmd_count.DME_GET_err,		// DME_GET
		SEC_err_info.UIC_cmd_count.DME_SET_err,		// DME_SET
		SEC_err_info.UIC_cmd_count.DME_PEER_GET_err,	// DME_PEER_GET
		SEC_err_info.UIC_cmd_count.DME_PEER_SET_err,	// DME_PEER_SET
		SEC_err_info.UIC_cmd_count.DME_POWERON_err,	// DME_POWERON
		SEC_err_info.UIC_cmd_count.DME_POWEROFF_err,	// DME_POWEROFF
		SEC_err_info.UIC_cmd_count.DME_ENABLE_err,	// DME_ENABLE
		SEC_err_info.UIC_cmd_count.DME_RESET_err,	// DME_RESET
		SEC_err_info.UIC_cmd_count.DME_END_PT_RST_err,	// DME_END_PT_RST
		SEC_err_info.UIC_cmd_count.DME_LINK_STARTUP_err,// DME_LINK_STARTUP
		SEC_err_info.UIC_cmd_count.DME_HIBER_ENTER_err,	// DME_HIBERN8_ENTER
		SEC_err_info.UIC_cmd_count.DME_HIBER_EXIT_err);	// DME_HIBERN8_EXIT

SEC_UFS_DATA_ATTR(SEC_UFS_uic_err_cnt, "\"PAERR\":\"%u\",\"DLPAINITERROR\":\"%u\",\"DLNAC\":\"%u\""
		",\"DLTCREPLAY\":\"%u\",\"NLERR\":\"%u\",\"TLERR\":\"%u\",\"DMEERR\":\"%u\"\n",
		SEC_err_info.UIC_err_count.PA_ERR_cnt,			// PA_ERR
		SEC_err_info.UIC_err_count.DL_PA_INIT_ERROR_cnt,	// DL_PA_INIT_ERROR
		SEC_err_info.UIC_err_count.DL_NAC_RECEIVED_ERROR_cnt,	// DL_NAC_RECEIVED
		SEC_err_info.UIC_err_count.DL_TC_REPLAY_ERROR_cnt,	// DL_TCx_REPLAY_ERROR
		SEC_err_info.UIC_err_count.NL_ERROR_cnt,		// NL_ERROR
		SEC_err_info.UIC_err_count.TL_ERROR_cnt,		// TL_ERROR
		SEC_err_info.UIC_err_count.DME_ERROR_cnt);		// DME_ERROR

SEC_UFS_DATA_ATTR(SEC_UFS_fatal_cnt, "\"DFE\":\"%u\",\"CFE\":\"%u\",\"SBFE\":\"%u\""
		",\"CEFE\":\"%u\",\"LLE\":\"%u\"\n",
		SEC_err_info.Fatal_err_count.DFE,		// Device_Fatal
		SEC_err_info.Fatal_err_count.CFE,		// Controller_Fatal
		SEC_err_info.Fatal_err_count.SBFE,		// System_Bus_Fatal
		SEC_err_info.Fatal_err_count.CEFE,		// Crypto_Engine_Fatal
		SEC_err_info.Fatal_err_count.LLE);		// Link_Lost

SEC_UFS_DATA_ATTR(SEC_UFS_utp_cnt, "\"UTMRQTASK\":\"%u\",\"UTMRATASK\":\"%u\",\"UTRR\":\"%u\""
		",\"UTRW\":\"%u\",\"UTRSYNCCACHE\":\"%u\",\"UTRUNMAP\":\"%u\",\"UTRETC\":\"%u\"\n",
		SEC_err_info.UTP_count.UTMR_query_task_count,	// QUERY_TASK
		SEC_err_info.UTP_count.UTMR_abort_task_count,	// ABORT_TASK
		SEC_err_info.UTP_count.UTR_read_err,		// READ_10
		SEC_err_info.UTP_count.UTR_write_err,		// WRITE_10
		SEC_err_info.UTP_count.UTR_sync_cache_err,	// SYNC_CACHE
		SEC_err_info.UTP_count.UTR_unmap_err,		// UNMAP
		SEC_err_info.UTP_count.UTR_etc_err);		// etc

SEC_UFS_DATA_ATTR(SEC_UFS_query_cnt, "\"NOPERR\":\"%u\",\"R_DESC\":\"%u\",\"W_DESC\":\"%u\""
		",\"R_ATTR\":\"%u\",\"W_ATTR\":\"%u\",\"R_FLAG\":\"%u\",\"S_FLAG\":\"%u\""
		",\"C_FLAG\":\"%u\",\"T_FLAG\":\"%u\"\n",
		SEC_err_info.query_count.NOP_err,
		SEC_err_info.query_count.R_Desc_err,		// R_Desc
		SEC_err_info.query_count.W_Desc_err,		// W_Desc
		SEC_err_info.query_count.R_Attr_err,		// R_Attr
		SEC_err_info.query_count.W_Attr_err,		// W_Attr
		SEC_err_info.query_count.R_Flag_err,		// R_Flag
		SEC_err_info.query_count.Set_Flag_err,		// Set_Flag
		SEC_err_info.query_count.Clear_Flag_err,	// Clear_Flag
		SEC_err_info.query_count.Toggle_Flag_err);	// Toggle_Flag

SEC_UFS_DATA_ATTR(SEC_UFS_err_sum, "\"OPERR\":\"%u\",\"UICCMD\":\"%u\",\"UICERR\":\"%u\""
		",\"FATALERR\":\"%u\",\"UTPERR\":\"%u\",\"QUERYERR\":\"%u\"\n",
		SEC_err_info.op_count.op_err,
		SEC_err_info.UIC_cmd_count.UIC_cmd_err,
		SEC_err_info.UIC_err_count.UIC_err,
		SEC_err_info.Fatal_err_count.Fatal_err,
		SEC_err_info.UTP_count.UTP_err,
		SEC_err_info.query_count.Query_err);

SEC_UFS_DATA_ATTR(sense_err_count, "\"MEDIUM\":\"%u\",\"HWERR\":\"%u\"\n",
		SEC_err_info.sense_count.scsi_medium_err,
		SEC_err_info.sense_count.scsi_hw_err);

SEC_UFS_DATA_ATTR(sense_err_logging, "\"LBA0\":\"%lx\",\"LBA1\":\"%lx\",\"LBA2\":\"%lx\""
		",\"LBA3\":\"%lx\",\"LBA4\":\"%lx\",\"LBA5\":\"%lx\""
		",\"LBA6\":\"%lx\",\"LBA7\":\"%lx\",\"LBA8\":\"%lx\",\"LBA9\":\"%lx\""
		",\"REGIONMAP\":\"%016llx\"\n",
		SEC_err_info.sense_err_log.issue_LBA_list[0],
		SEC_err_info.sense_err_log.issue_LBA_list[1],
		SEC_err_info.sense_err_log.issue_LBA_list[2],
		SEC_err_info.sense_err_log.issue_LBA_list[3],
		SEC_err_info.sense_err_log.issue_LBA_list[4],
		SEC_err_info.sense_err_log.issue_LBA_list[5],
		SEC_err_info.sense_err_log.issue_LBA_list[6],
		SEC_err_info.sense_err_log.issue_LBA_list[7],
		SEC_err_info.sense_err_log.issue_LBA_list[8],
		SEC_err_info.sense_err_log.issue_LBA_list[9],
		SEC_err_info.sense_err_log.issue_region_map);

static struct attribute *sec_ufs_error_attributes[] = {
	&dev_attr_SEC_UFS_op_cnt.attr,
	&dev_attr_SEC_UFS_uic_cmd_cnt.attr,
	&dev_attr_SEC_UFS_uic_err_cnt.attr,
	&dev_attr_SEC_UFS_fatal_cnt.attr,
	&dev_attr_SEC_UFS_utp_cnt.attr,
	&dev_attr_SEC_UFS_query_cnt.attr,
	&dev_attr_SEC_UFS_err_sum.attr,
	&dev_attr_sense_err_count.attr,
	&dev_attr_sense_err_logging.attr,
	NULL
};

static struct attribute_group sec_ufs_error_attribute_group = {
	.attrs	= sec_ufs_error_attributes,
};

void ufs_sysfs_add_sec_nodes(struct ufs_hba *hba)
{
	int ret;
	struct device *shost_dev = &(hba->host->shost_dev);

	ret = sysfs_create_group(&shost_dev->kobj, &sec_ufs_error_attribute_group);
	if (ret)
		dev_err(hba->dev, "cannot create sec error sysfs group err: %d\n", ret);

	/* init ufs_sec_debug function */
	ufs_debug_func = SEC_ufs_send_err_info;
}

void ufs_sysfs_remove_sec_nodes(struct ufs_hba *hba)
{
	struct device *shost_dev = &(hba->host->shost_dev);

	sysfs_remove_group(&shost_dev->kobj, &sec_ufs_error_attribute_group);
}

void ufs_sysfs_add_nodes(struct device *dev)
{
	int ret;

	ret = sysfs_create_groups(&dev->kobj, ufs_sysfs_groups);
	if (ret)
		dev_err(dev,
			"%s: sysfs groups creation failed (err = %d)\n",
			__func__, ret);
}

void ufs_sysfs_remove_nodes(struct device *dev)
{
	sysfs_remove_groups(&dev->kobj, ufs_sysfs_groups);
}
