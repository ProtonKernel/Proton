/*
 * Copyright (c) 2020 Samsung Electronics Co., Ltd.
 *	      http://www.samsung.com/
 *
 * EXYNOS - Stage 2 Protection Unit(S2MPU)
 * Author: Junho Choi <junhosj.choi@samsung.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#include <linux/bits.h>
#include <linux/err.h>
#include <linux/module.h>
#include <linux/of.h>
#include <linux/of_reserved_mem.h>
#include <linux/device.h>
#include <linux/dma-mapping.h>
#include <linux/interrupt.h>
#include <linux/irq.h>
#include <linux/irqreturn.h>
#include <linux/of_irq.h>
#include <linux/platform_device.h>
#include <linux/slab.h>
#include <linux/debugfs.h>
#include <linux/spinlock.h>
#include <asm/cacheflush.h>

#include <soc/samsung/exynos-hvc.h>
#include <soc/samsung/exynos-s2mpu.h>
#include <soc/samsung/exynos-smc.h>
#if IS_ENABLED(CONFIG_DEBUG_SNAPSHOT)
#include <soc/samsung/debug-snapshot.h>
#endif

static uint32_t instance_num;
static uint32_t subsystem_num;
static spinlock_t s2mpu_lock;

static uint64_t s2mpu_pd_bitmap;

static const char *s2mpu_instance_list[EXYNOS_MAX_S2MPU_INSTANCE];
static const char *s2mpu_subsys_list[EXYNOS_MAX_S2MPU_SUBSYSTEM];

static struct s2mpufd_notifier_block *nb_list[EXYNOS_MAX_S2MPU_SUBSYSTEM];

static bool s2mpu_success[45];

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
uint64_t s2mpufd_notifier_call_register(struct s2mpufd_notifier_block *snb)
{
	uint32_t subsys_idx;

	if (snb == NULL) {
		pr_err("%s: subsystem notifier block is NULL pointer\n", __func__);
		return ERROR_INVALID_NOTIFIER_BLOCK;
	}

	if (snb->subsystem == NULL) {
		pr_err("%s: subsystem is NULL pointer\n", __func__);
		return ERROR_INVALID_SUBSYSTEM_NAME;
	}

	for (subsys_idx = 0; subsys_idx < subsystem_num; subsys_idx++) {
		if (!strncmp(s2mpu_subsys_list[subsys_idx],
					snb->subsystem,
					EXYNOS_MAX_SUBSYSTEM_LEN))
			break;
	}

	if (subsys_idx >= subsystem_num) {
		pr_err("%s: %s is invalid subsystem name or not suported subsystem\n",
				__func__, snb->subsystem);
		return ERROR_DO_NOT_SUPPORT_SUBSYSTEM;
	}

	nb_list[subsys_idx] = snb;

	pr_info("%s: S2mpufd %s FW Notifier call registeration Success!\n",
			__func__, snb->subsystem);

	return 0;
}
EXPORT_SYMBOL(s2mpufd_notifier_call_register);

static uint64_t exynos_s2mpufd_notifier_call(struct s2mpufd_info_data *data)
{
	int ret;
	uint64_t subsys_idx, p_ret = 0;

	pr_info("    +S2MPU Fault Detector Notifier Call Information\n\n");
	for (subsys_idx = 0; subsys_idx < subsystem_num; subsys_idx++) {
		if (!(data->notifier_flag[subsys_idx]))
			continue;

		/* Check Notifier Call Fucntion pointer exist */
		if (nb_list[subsys_idx] == NULL) {
			pr_info("(%s) subsystem didn't register notifier block\n",
					s2mpu_subsys_list[subsys_idx]);
			p_ret |= S2MPUFD_NOTIFIER_PANIC | (S2MPUFD_NOTIFY_BAD << subsys_idx);
			data->notifier_flag[subsys_idx] = 0;

			continue;
		}

		/* Check Notifier call pointer exist */
		if (nb_list[subsys_idx]->notifier_call == NULL) {
			pr_info("(%s) subsystem notifier block doesn't have notifier call\n",
					s2mpu_subsys_list[subsys_idx]);
			p_ret |= S2MPUFD_NOTIFIER_PANIC | (S2MPUFD_NOTIFY_BAD << subsys_idx);
			data->notifier_flag[subsys_idx] = 0;

			continue;
		}

		pr_info("(%s) subsystem notifier call ++\n\n", s2mpu_subsys_list[subsys_idx]);

		ret = nb_list[subsys_idx]->notifier_call(nb_list[subsys_idx],
							 &data->noti_info[subsys_idx]);
		if (ret == S2MPUFD_NOTIFY_BAD)
			p_ret |= S2MPUFD_NOTIFIER_PANIC | (S2MPUFD_NOTIFY_BAD << subsys_idx);
		data->notifier_flag[subsys_idx] = 0;

		pr_info("(%s) subsystem notifier ret[%x] --\n", s2mpu_subsys_list[subsys_idx], ret);
	}

	pr_info("    -S2MPU Fault Detector Notifier Call Information\n");
	pr_info("====================================================\n");

	if (p_ret & S2MPUFD_NOTIFIER_PANIC) {
		pr_err("%s: Notifier Call Request Panic p_ret[%llx]\n",
					__func__, p_ret);
		return p_ret;
	}

	return S2MPUFD_NOTIFY_OK;
}

static irqreturn_t exynos_s2mpufd_irq_handler(int irq, void *dev_id)
{
	struct s2mpufd_info_data *data = dev_id;
	unsigned long ret;
	uint32_t irq_idx;

	pr_info("S2MPU interrupt called %d !!\n",irq);

	for (irq_idx = 0; irq_idx < data->irqcnt; irq_idx++) {
		if (irq == data->irq[irq_idx])
			break;
	}

	/*
	 * Interrupt status register in S2MPUFD will be cleared
	 * in this HVC handler
	 */
	ret = exynos_hvc(HVC_CMD_GET_S2MPUFD_FAULT_INFO,
			 irq_idx, 0, 0, 0);
	if (ret)
		pr_err("Can't handle S2MPU fault of this subsystem [%d]\n",
				irq_idx);
	return IRQ_WAKE_THREAD;
}

static irqreturn_t exynos_s2mpufd_irq_handler_thread(int irq, void *dev_id)
{
	struct s2mpufd_info_data *data = dev_id;
	unsigned int fault_subsys_idx, intr_cnt, addr_low, fault_vid, fault_rw, fault_type, i;
	unsigned int fault_blacklist_owner;
	uint64_t addr_high;
	unsigned int len, axid;
	uint32_t irq_idx;
	const char *pcie_name[2] = {"PCIE_GEN2", "DEFAULT"};
	unsigned long flags;
	uint64_t ret;

	spin_lock_irqsave(&s2mpu_lock, flags);

	pr_auto(ASL4, "===============[S2MPU FAULT DETECTION]===============\n");

	/* find right irq index */
	for (irq_idx = 0; irq_idx < data->irqcnt; irq_idx++) {
		if (irq == data->irq[irq_idx])
			break;
	}

	pr_auto(ASL4, "- Fault Instance : %s\n", s2mpu_instance_list[irq_idx]);

	intr_cnt = data->fault_info[irq_idx].s2mpufd_intr_cnt;

	pr_auto(ASL4, "- Fault VID Count : %d\n\n", intr_cnt);

	for (i = 0; i < intr_cnt; i++) {
		fault_subsys_idx = data->fault_info[irq_idx].s2mpufd_subsystem[i];
		addr_low = data->fault_info[irq_idx].s2mpufd_fault_addr_low[i];
		addr_high = data->fault_info[irq_idx].s2mpufd_fault_addr_high[i];
		fault_vid = data->fault_info[irq_idx].s2mpufd_fault_vid[i];
		fault_type = data->fault_info[irq_idx].s2mpufd_fault_type[i];
		fault_rw = data->fault_info[irq_idx].s2mpufd_rw[i];
		len = data->fault_info[irq_idx].s2mpufd_len[i];
		axid = data->fault_info[irq_idx].s2mpufd_axid[i];
		fault_blacklist_owner = data->fault_info[irq_idx].s2mpufd_blacklist_owner[i];

		if (!strncmp(s2mpu_subsys_list[fault_subsys_idx], pcie_name[0], EXYNOS_MAX_SUBSYSTEM_LEN))
			pr_auto(ASL4, "- Fault Subsystem : %s\n", "PCIe(Wi-Fi)");
		else if (!strncmp(s2mpu_subsys_list[fault_subsys_idx], pcie_name[1], EXYNOS_MAX_SUBSYSTEM_LEN))
			pr_auto(ASL4, "- Fault Subsystem : %s\n", s2mpu_instance_list[irq_idx]);
		else
			pr_auto(ASL4, "- Fault Subsystem : %s\n", s2mpu_subsys_list[fault_subsys_idx]);

		pr_auto(ASL4, "- Fault Address : %#lx\n",
				addr_high ?
				(addr_high << 32) | addr_low :
				addr_low);

		if (fault_blacklist_owner != FAULT_THIS_ADDR_IS_NOT_BLACKLIST) {
			if (fault_blacklist_owner == OWNER_IS_KERNEL_RO) {
				pr_auto(ASL4, "  (This address is overlapped with blacklist and used by kernel code region)\n");
			} else if (fault_blacklist_owner == OWNER_IS_TEST) {
				pr_auto(ASL4, "  (This address is overlapped with blacklist and used by test region)\n");
			} else {
				pr_auto(ASL4, "  (This address is overlapped with blacklist and used by [%s])\n",
						s2mpu_subsys_list[fault_blacklist_owner]);
			}
		}

		if (fault_type == FAULT_TYPE_MPTW_ACCESS_FAULT) {
			pr_auto(ASL4, "- Fault Type : %s\n", "Memory protection table walk access fault");
		} else if (fault_type == FAULT_TYPE_AP_FAULT) {
			pr_auto(ASL4, "- Fault Type : %s\n", "Access Permission fault");
		} else {
			pr_auto(ASL4, "- Fault Type : %s, Num : 0x%x\n", "Anonymous type", fault_type);
		}

		pr_auto(ASL4, "- Fault Direction : %s\n",
				fault_rw ?
				"WRITE" :
				"READ");

		pr_auto(ASL4, "- Fault Virtual Domain ID : %d\n",
				fault_vid);

		pr_auto(ASL4, "- Fault RW Length : %#x\n",
				len);

		pr_auto(ASL4, "- Fault AxID : %#x\n",
				axid);

		pr_auto(ASL4, "\n");

		data->noti_info[fault_subsys_idx].subsystem = s2mpu_subsys_list[fault_subsys_idx];
		data->noti_info[fault_subsys_idx].fault_addr = (addr_high << 32) | addr_low;
		data->noti_info[fault_subsys_idx].fault_rw = fault_rw;
		data->noti_info[fault_subsys_idx].fault_len = len;
		data->noti_info[fault_subsys_idx].fault_type = fault_type;
		data->notifier_flag[fault_subsys_idx] = S2MPUFD_NOTIFIER_SET;
	}

	pr_info("====================================================\n");

	spin_unlock_irqrestore(&s2mpu_lock, flags);

	ret = exynos_s2mpufd_notifier_call(data);
	if (ret) {
#if IS_ENABLED(CONFIG_DEBUG_SNAPSHOT)
		if (dbg_snapshot_expire_watchdog())
			pr_err("WDT reset fails\n");
#endif
		BUG();
	}

	return IRQ_HANDLED;
}

static bool has_s2mpu(uint32_t pd)
{
	return (s2mpu_pd_bitmap & BIT_ULL_MASK(pd)) != 0;
}

uint64_t exynos_set_dev_stage2_ap(const char *subsystem,
				  uint64_t base,
				  uint64_t size,
				  uint32_t ap)
{
	uint32_t subsys_idx;

	if (subsystem == NULL) {
		pr_err("%s: Invalid S2MPU name\n", __func__);
		return ERROR_INVALID_SUBSYSTEM_NAME;
	}

	for (subsys_idx = 0; subsys_idx < subsystem_num; subsys_idx++) {
		if (!strncmp(s2mpu_subsys_list[subsys_idx],
				subsystem,
				EXYNOS_MAX_SUBSYSTEM_LEN))
			break;
	}

	if (subsys_idx == subsystem_num) {
		pr_err("%s: DO NOT support %s S2MPU\n",
				__func__, subsystem);
		return ERROR_DO_NOT_SUPPORT_SUBSYSTEM;
	}

	return exynos_hvc(HVC_FID_SET_S2MPU,
			  subsys_idx,
			  base,
			  size,
			  ap);
}
EXPORT_SYMBOL(exynos_set_dev_stage2_ap);

uint64_t exynos_set_stage2_blacklist(const char *subsystem,
				     uint64_t base,
				     uint64_t size)
{
	uint32_t subsys_idx;

	if (subsystem == NULL) {
		pr_err("%s: Invalid S2MPU name\n", __func__);
		return ERROR_INVALID_SUBSYSTEM_NAME;
	}

	for (subsys_idx = 0; subsys_idx < subsystem_num; subsys_idx++) {
		if (!strncmp(s2mpu_subsys_list[subsys_idx],
				subsystem,
				EXYNOS_MAX_SUBSYSTEM_LEN))
			break;
	}

	if (subsys_idx == subsystem_num) {
		pr_err("%s: DO NOT support %s S2MPU\n",
				__func__, subsystem);
		return ERROR_DO_NOT_SUPPORT_SUBSYSTEM;
	}

	return exynos_hvc(HVC_FID_SET_S2MPU_BLACKLIST,
			  subsys_idx,
			  base,
			  size, 0);
}
EXPORT_SYMBOL(exynos_set_stage2_blacklist);

uint64_t exynos_set_all_stage2_blacklist(uint64_t base,
					 uint64_t size)
{
	uint64_t ret = 0;

	ret = exynos_hvc(HVC_FID_SET_ALL_S2MPU_BLACKLIST,
			  base,
			  size, 0, 0);
	if (ret) {
		pr_err("%s: [ERROR] base[%llx], size[%llx]\n", __func__, base, size);
		return ret;
	}

	pr_info("%s: Set s2mpu blacklist done for test\n", __func__);

	return ret;
}
EXPORT_SYMBOL(exynos_set_all_stage2_blacklist);

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
uint64_t exynos_request_fw_stage2_ap(const char *subsystem)
{
	uint32_t idx;
	uint64_t ret = 0;

	if (subsystem == NULL) {
		pr_err("%s: subsystem is NULL pointer\n", __func__);
		return ERROR_INVALID_SUBSYSTEM_NAME;
	}

	for (idx = 0; idx < subsystem_num; idx++) {
		if (!strncmp(s2mpu_subsys_list[idx],
				subsystem,
				EXYNOS_MAX_SUBSYSTEM_LEN))
			break;
	}

	if (idx == subsystem_num) {
		pr_err("%s: DO NOT SUPPORT %s Sub-system\n",
				__func__, subsystem);
		return ERROR_DO_NOT_SUPPORT_SUBSYSTEM;
	}

	pr_debug("%s: %s Sub-system requests FW Stage 2 AP\n",
			__func__, subsystem);

	ret = exynos_hvc(HVC_FID_REQUEST_FW_STAGE2_AP,
				idx, 0, 0, 0);
	if (ret) {
		pr_err("%s: Enabling %s FW Stage 2 AP is failed (%#llx)\n",
				__func__, subsystem, ret);
		return ret;
	}

	pr_info("%s: Allow %s FW Stage 2 access permission\n",
			__func__, subsystem);

	return 0;
}
EXPORT_SYMBOL(exynos_request_fw_stage2_ap);

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
					size_t fw_mem_size)
{
	uint32_t idx;
	uint64_t ret = 0;

	if (subsystem == NULL) {
		pr_err("%s: subsystem is NULL pointer\n", __func__);
		return ERROR_INVALID_SUBSYSTEM_NAME;
	}

	if ((fw_base == 0) || (fw_bin_size == 0) || (fw_mem_size == 0)) {
		pr_err("%s: Invalid FW[%d] binary information", __func__, fw_id);
		pr_err(" - fw_base[%#llx]\n", fw_base);
		pr_err(" - fw_bin_size[%#llx]\n", fw_bin_size);
		pr_err(" - fw_mem_size[%#llx]\n", fw_mem_size);
		return ERROR_INVALID_FW_BIN_INFO;
	}

	for (idx = 0; idx < subsystem_num; idx++) {
		if (!strncmp(s2mpu_subsys_list[idx],
				subsystem,
				EXYNOS_MAX_SUBSYSTEM_LEN))
			break;
	}

	if (idx == subsystem_num) {
		pr_err("%s: DO NOT SUPPORT %s Sub-system\n",
				__func__, subsystem);
		return ERROR_DO_NOT_SUPPORT_SUBSYSTEM;
	}

	pr_debug("%s: Start %s Sub-system FW[%d] verification\n",
			__func__, subsystem, fw_id);

	ret = exynos_hvc(HVC_FID_VERIFY_SUBSYSTEM_FW,
			 ((uint64_t)fw_id << SUBSYSTEM_FW_NUM_SHIFT) |
			 ((uint64_t)idx << SUBSYSTEM_INDEX_SHIFT),
			 fw_base,
			 fw_bin_size,
			 fw_mem_size);
	if (ret) {
		pr_err("%s: %s FW[%d] verification is failed (%#llx)\n",
				__func__, subsystem, fw_id, ret);
		return ret;
	}

	pr_info("%s: %s FW[%d] is verified!\n", __func__, subsystem, fw_id);

	return 0;
}
EXPORT_SYMBOL(exynos_verify_subsystem_fw);

uint64_t exynos_pd_backup_s2mpu(uint32_t pd)
{
	uint64_t hvc_ret;

	if (has_s2mpu(pd) == false)
		return 0;

	hvc_ret = exynos_hvc(HVC_FID_BACKUP_RESTORE_S2MPU,
				EXYNOS_PD_S2MPU_BACKUP,
				pd, 0, 0);
	if (hvc_ret == ERROR_NO_S2MPU_IN_BLOCK) {
		pr_warn("%s: The Block[%d] has no S2MPU, but it's requested\n",
				__func__, pd);
		return 0;
	}

	return hvc_ret;
}
EXPORT_SYMBOL(exynos_pd_backup_s2mpu);

uint64_t exynos_pd_restore_s2mpu(uint32_t pd)
{
	uint64_t hvc_ret;

	if (has_s2mpu(pd) == false)
		return 0;

	hvc_ret = exynos_hvc(HVC_FID_BACKUP_RESTORE_S2MPU,
				EXYNOS_PD_S2MPU_RESTORE,
				pd, 0, 0);
	if (hvc_ret == ERROR_NO_S2MPU_IN_BLOCK) {
		pr_warn("%s: The Block[%d] has no S2MPU, but it's requested\n",
				__func__, pd);
		return 0;
	}

	return hvc_ret;
}
EXPORT_SYMBOL(exynos_pd_restore_s2mpu);

#ifdef CONFIG_OF_RESERVED_MEM
static int __init exynos_s2mpu_reserved_mem_setup(struct reserved_mem *remem)
{
	pr_err("%s: Reserved memory for S2MPU table: addr=%lx, size=%lx\n",
			__func__, remem->base, remem->size);

	return 0;
}
RESERVEDMEM_OF_DECLARE(s2mpu_table, "exynos,s2mpu_table", exynos_s2mpu_reserved_mem_setup);
#endif

static void __init exynos_pd_get_s2mpu_bitmap(void)
{
	uint64_t bitmap;

	bitmap = exynos_hvc(HVC_FID_GET_S2MPU_PD_BITMAP,
				0, 0, 0, 0);
	if (bitmap == HVC_UNK) {
		pr_err("%s: Fail to get S2MPU PD bitmap\n");
		return;
	}

	s2mpu_pd_bitmap = bitmap;
}

static ssize_t exynos_s2mpu_enable_check(struct device *dev,
					 struct device_attribute *attr,
					 char *buf)
{
	int ret = 0;
	int idx;

	for(idx = 0; idx < instance_num; idx++) {
		if(s2mpu_success[idx])
			ret += snprintf(buf + ret,
					(instance_num - idx) * S2_BUF_SIZE,
					"s2mpu(%2d) %s is enable\n", idx, s2mpu_instance_list[idx]);
		else
			ret += snprintf(buf + ret,
					(instance_num - idx) * S2_BUF_SIZE,
					"s2mpu(%2d) %s is disable\n", idx, s2mpu_instance_list[idx]);
	}

	return ret;
}

static ssize_t exynos_s2mpu_enable_test(struct device *dev,
					struct device_attribute *attr,
					const char *buf,
					size_t count)
{
	uint64_t ret = 0;
	uint64_t idx;

	dev_info(dev, "s2mpu enable check end\n");

	ret = exynos_hvc(HVC_FID_CHECK_S2MPU_ENABLE, 0, 0, 0, 0);

	for(idx = 0; idx < instance_num; idx++) {
		if (!(ret & ((uint64_t)0x1 << idx))) {
			s2mpu_success[idx] = 0;
			dev_err(dev, "s2mpu(%lld) is not enable\n", idx);
		}
		else
			s2mpu_success[idx] = 1;
	}

	dev_info(dev, "s2mpu enable check end\n");

	return count;
}

static DEVICE_ATTR(exynos_s2mpu_enable, 0644,
		exynos_s2mpu_enable_check, exynos_s2mpu_enable_test);

static int exynos_s2mpu_probe(struct platform_device *pdev)
{
	struct s2mpufd_info_data *data;
	int i, ret = 0;

	spin_lock_init(&s2mpu_lock);

	data = devm_kzalloc(&pdev->dev, sizeof(struct s2mpufd_info_data), GFP_KERNEL);
	if (!data) {
		dev_err(&pdev->dev, "Fail to allocate memory(s2mpufd_info_data)\n");
		ret = -ENOMEM;
		goto out;
	}

	platform_set_drvdata(pdev, data);
	data->dev = &pdev->dev;

	ret = of_property_read_u32(data->dev->of_node,
			"channel",
			&data->ch_num);

	if (ret) {
		dev_err(data->dev,
				"Fail to get S2MPUFD channel number(%d) from dt\n",
				data->ch_num);
		goto out;
	}

	dev_info(data->dev, "S2MPUFD channel number is valid\n");

	/* Allocate S2MPUFD fault information buffers as to subsystems */
	data->fault_info = devm_kzalloc(data->dev, sizeof(struct s2mpufd_fault_info) * data->ch_num, GFP_KERNEL);
	if (!data->fault_info) {
		dev_err(data->dev, "Fail to allocate memory(s2mpufd_fault_info)\n");
		ret = -ENOMEM;
		goto out;
	}

	data->fault_info_pa = virt_to_phys((void *)data->fault_info);

	dev_dbg(data->dev,
		"VA of s2mpufd_fault_info : %lx\n",
		(unsigned long)data->fault_info);
	dev_dbg(data->dev,
		"PA of s2mpufd_fault_info : %llx\n",
		data->fault_info_pa);

	ret = of_property_read_u32(data->dev->of_node, "irqcnt", &data->irqcnt);
	if (ret) {
		dev_err(data->dev,
			"Fail to get irqcnt(%d) from dt\n",
			data->irqcnt);
		goto out_with_dma_free;
	}

	dev_dbg(data->dev,
		"The number of S2MPUFD interrupt : %d\n",
		data->irqcnt);

	for (i = 0; i < data->irqcnt; i++) {
		data->irq[i] = irq_of_parse_and_map(data->dev->of_node, i);
		if (!data->irq[i]) {
			dev_err(data->dev,
				"Fail to get irq(%d) from dt\n",
				data->irq[i]);
			ret = -EINVAL;
			goto out_with_dma_free;
		}
		ret = devm_request_threaded_irq(data->dev,
						data->irq[i],
						exynos_s2mpufd_irq_handler,
						exynos_s2mpufd_irq_handler_thread,
						IRQF_ONESHOT,
						pdev->name,
						data);
		if (ret) {
			dev_err(data->dev,
					"Fail to request IRQ handler. ret(%d) irq(%d)\n",
					ret, data->irq[i]);
			goto out_with_dma_free;
		}
	}
	ret = exynos_hvc(HVC_CMD_INIT_S2MPUFD,
			data->fault_info_pa,
			data->ch_num,
			sizeof(struct s2mpufd_fault_info),
			0);
	if (ret) {
		switch (ret) {
		case S2MPUFD_ERROR_INVALID_CH_NUM:
			dev_err(data->dev,
				"The channel number(%d) defined in DT is invalid\n",
				data->ch_num);
			break;
		case S2MPUFD_ERROR_INVALID_FAULT_INFO_SIZE:
			dev_err(data->dev,
				"The size of struct s2mpufd_fault_info(%#lx) is invalid\n",
				sizeof(struct s2mpufd_fault_info));
			break;
		default:
			dev_err(data->dev,
				"Unknown error from SMC. ret[%#x]\n",
				ret);
			break;
		}
		ret = -EINVAL;
		goto out;
	}

	data->noti_info = devm_kzalloc(data->dev,
				       sizeof(struct s2mpufd_notifier_info) * subsystem_num,
				       GFP_KERNEL);
	if (!data->noti_info) {
		dev_err(data->dev, "Fail to allocate memory(s2mpufd_notifier_info)\n");
		ret = -ENOMEM;
		goto out;
	}

	data->notifier_flag = devm_kzalloc(data->dev, sizeof(unsigned int) * subsystem_num, GFP_KERNEL);
	if (!data->notifier_flag) {
		dev_err(data->dev, "Fail to allocate memory(s2mpufd_notifier_flag)\n");
		ret = -ENOMEM;
		goto out;
	}

	ret = device_create_file(data->dev, &dev_attr_exynos_s2mpu_enable);
	if (ret) {
		 dev_err(data->dev, "exynos_s2mpu_enable sysfs_create_file fail");
		 return ret;
	}

	dev_info(data->dev, "Exynos S2MPUFD driver probe done!\n");

	return 0;

out_with_dma_free:
	platform_set_drvdata(pdev, NULL);

	data->fault_info = NULL;
	data->fault_info_pa = 0;

	data->ch_num = 0;
	data->irqcnt = 0;

out:
	return ret;
}

static int exynos_s2mpu_remove(struct platform_device *pdev)
{
	struct s2mpufd_info_data *data = platform_get_drvdata(pdev);
	int i;

	platform_set_drvdata(pdev, NULL);

	for (i = 0; i < data->ch_num; i++)
		data->irq[i] = 0;

	data->ch_num = 0;
	data->irqcnt = 0;

	return 0;
}

static const struct of_device_id exynos_s2mpu_of_match_table[] = {
	{ .compatible = "samsung,exynos-s2mpu", },
	{ },
};

static struct platform_driver exynos_s2mpu_driver = {
	.probe = exynos_s2mpu_probe,
	.remove = exynos_s2mpu_remove,
	.driver = {
		.name = "exynos-s2mpu",
		.owner = THIS_MODULE,
		.of_match_table = of_match_ptr(exynos_s2mpu_of_match_table),
	}
};
MODULE_DEVICE_TABLE(of, exynos_s2mpu_of_match_table);

static int __init exynos_s2mpu_init(void)
{
	struct device_node *np = NULL;
	int ret = 0;
	struct reserved_mem *rmem;
	struct device_node *rmem_np;
	uint64_t hvc_ret = 0;

	np = of_find_compatible_node(NULL, NULL, "samsung,exynos-s2mpu");
	if (np == NULL) {
		pr_err("%s: Do not support S2MPU\n", __func__);
		return -ENODEV;
	}

	rmem_np = of_parse_phandle(np, "memory_region", 0);
	rmem = of_reserved_mem_lookup(rmem_np);
	if (!rmem) {
		pr_err("%s: fail to acquire memory region\n", __func__);
		return -EADDRNOTAVAIL;
	}

	pr_err("%s: Reserved memory for S2MPU table: addr=%lx, size=%lx\n",
			__func__, rmem->base, rmem->size);

	ret = of_property_read_u32(np, "subsystem-num", &subsystem_num);
	if (ret) {
		pr_err("%s: Fail to get S2MPU subsystem number from device tree\n",
			__func__);
		return ret;
	}

	pr_debug("%s: S2MPU subsystem number : %d\n", __func__, subsystem_num);

	ret = of_property_read_string_array(np,
					    "subsystem-names",
					    s2mpu_subsys_list,
					    subsystem_num);
	if (ret < 0) {
		pr_err("%s: Fail to get S2MPU subsystem list from device tree\n",
			__func__);
		return ret;
	}

	ret = of_property_read_u32(np, "instance-num", &instance_num);
	if (ret) {
		pr_err("%s: Fail to get S2MPU instance number from device tree\n",
			__func__);
		return ret;
	}

	pr_debug("%s: S2MPU instance number : %d\n", __func__, instance_num);

	ret = of_property_read_string_array(np,
					    "instance-names",
					    s2mpu_instance_list,
					    instance_num);
	if (ret < 0) {
		pr_err("%s: Fail to get S2MPU instance list from device tree\n",
			__func__);
		return ret;
	}

	/* Set kernel_RO_region to all s2mpu blacklist */
	hvc_ret = exynos_hvc(HVC_FID_BAN_KERNEL_RO_REGION,
			0, 0, 0, 0);

	if (hvc_ret != 0)
		pr_info("%s: S2MPU blacklist set fail, hvc_ret[%d]\n", __func__, hvc_ret);
	else
		pr_info("%s: Set kernel RO region to all s2mpu blacklist done\n", __func__);

	exynos_pd_get_s2mpu_bitmap();

	pr_info("%s: S2MPU PD bitmap - %llx\n", __func__, s2mpu_pd_bitmap);
	pr_info("%s: Making S2MPU list is done\n", __func__);

	return platform_driver_register(&exynos_s2mpu_driver);
}

static void __exit exynos_s2mpu_exit(void)
{
	platform_driver_unregister(&exynos_s2mpu_driver);
}

module_init(exynos_s2mpu_init);
module_exit(exynos_s2mpu_exit);

MODULE_DESCRIPTION("Exynos Stage 2 Protection Unit(S2MPU) driver");
MODULE_AUTHOR("<junhosj.choi@samsung.com>");
MODULE_LICENSE("GPL");
