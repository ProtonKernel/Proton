// SPDX-License-Identifier: GPL-2.0-or-later
/* sound/soc/samsung/vts/vts.c
 *
 * ALSA SoC - Samsung VTS driver
 *
 * Copyright (c) 2016 Samsung Electronics Co. Ltd.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */
#include <linux/clk.h>
#include <linux/io.h>
#include <linux/module.h>
#include <linux/of.h>
#include <linux/of_platform.h>
#include <linux/of_reserved_mem.h>
#include <linux/pm_runtime.h>
#include <linux/firmware.h>
#include <linux/dma-mapping.h>
#include <linux/proc_fs.h>
#include <linux/delay.h>
#include <linux/regmap.h>
#include <linux/pm_wakeup.h>
#include <linux/sched/clock.h>
#include <linux/miscdevice.h>
#include <linux/pinctrl/consumer.h>
#include <linux/suspend.h>

#include <asm-generic/delay.h>

#include <sound/soc.h>
#include <sound/soc-dapm.h>
#include <sound/pcm_params.h>
#include <sound/tlv.h>
#include <sound/vts.h>

#include <sound/samsung/mailbox.h>
#include <sound/samsung/vts.h>
#include <soc/samsung/exynos-pmu-if.h>
#include <soc/samsung/exynos-el3_mon.h>
#include <soc/samsung/exynos-itmon.h>

#if IS_ENABLED(CONFIG_EXYNOS_IMGLOADER)
#include <soc/samsung/imgloader.h>
#include <soc/samsung/exynos-s2mpu.h>
#endif

#include <soc/samsung/debug-snapshot.h>
#include "vts.h"
#include "vts_res.h"
#include "vts_log.h"
#include "vts_dump.h"
#include "vts_s_lif_dump.h"
#include "vts_dbg.h"
#include "vts_proc.h"

#ifdef CONFIG_SOC_EXYNOS8895
#define PAD_RETENTION_VTS_OPTION		(0x3148)
#define VTS_CPU_CONFIGURATION			(0x24E0)
#define VTS_CPU_LOCAL_PWR_CFG			(0x00000001)
#define VTS_CPU_STATUS				(0x24E4)
#define VTS_CPU_STATUS_STATUS_MASK		(0x00000001)
#define VTS_CPU_STANDBY				VTS_CPU_STATUS
#define VTS_CPU_STANDBY_STANDBYWFI_MASK		(0x10000000)
#define VTS_CPU_OPTION				(0x24E8)
#define VTS_CPU_OPTION_USE_STANDBYWFI_MASK	(0x00010000)
#define VTS_CPU_RESET_OPTION			VTS_CPU_OPTION
#define VTS_CPU_RESET_OPTION_ENABLE_CPU_MASK	(0x00008000)
#elif defined(CONFIG_SOC_EXYNOS9810)
#define PAD_RETENTION_VTS_OPTION		(0x4AD8)
#define VTS_CPU_CONFIGURATION			(0x4AC4)
#define VTS_CPU_LOCAL_PWR_CFG			(0x00000001)
#define VTS_CPU_STATUS				(0x4AC8)
#define VTS_CPU_STATUS_STATUS_MASK		(0x00000001)
#define VTS_CPU_STANDBY				(0x3814)
#define VTS_CPU_STANDBY_STANDBYWFI_MASK		(0x10000000)
#define VTS_CPU_OPTION				(0x3818)
#define VTS_CPU_OPTION_USE_STANDBYWFI_MASK	(0x00010000)
#define VTS_CPU_RESET_OPTION			(0x4ACC)
#define VTS_CPU_RESET_OPTION_ENABLE_CPU_MASK	(0x10000000)
#elif defined(CONFIG_SOC_EXYNOS9820)
#define PAD_RETENTION_VTS_OPTION		(0x2AA0)
#define VTS_CPU_CONFIGURATION			(0x2C80)
#define VTS_CPU_LOCAL_PWR_CFG			(0x00000001)
#define VTS_CPU_IN				(0x2CA4)
#define VTS_CPU_IN_SLEEPING_MASK		(0x00000002)
#elif defined(CONFIG_SOC_EXYNOS9830)
#define PAD_RETENTION_VTS_OPTION		(0x2D20) /* VTS_OUT:PAD__RTO */
#define VTS_CPU_CONFIGURATION			(0x3080)
#define VTS_CPU_LOCAL_PWR_CFG			(0x00000001)
#define VTS_CPU_IN				(0x30A4)
#define VTS_CPU_IN_SLEEPING_MASK		(0x00000002)
#elif IS_ENABLED(CONFIG_SOC_EXYNOS2100)
#define PAD_RETENTION_VTS_OPTION  (0x32A0) // VTS_OUT : PAD__RTO
#define VTS_CPU_CONFIGURATION   (0x3640)
#define VTS_CPU_LOCAL_PWR_CFG			(0x00000001)
#define VTS_CPU_IN      (0x3664)
#define VTS_CPU_IN_SLEEPING_MASK		(0x00000002)
#endif

#define LIMIT_IN_JIFFIES (msecs_to_jiffies(1000))
#define DMIC_CLK_RATE (768000)
#define VTS_TRIGGERED_TIMEOUT_MS (5000)

#define VTS_DUMP_MAGIC "VTS-LOG0" // 0x30474F4C2D535456ull /* VTS-LOG0 */
/* #define VTS_2MIC_NS */
#define VTS_SYS_CLOCK (66000000)	/* 66M */

#undef EMULATOR
#ifdef EMULATOR
static void __iomem *pmu_alive;
#define set_mask_value(id, mask, value)	(id = ((id & ~mask) | (value & mask)))

static void update_mask_value(void __iomem *sfr,
	unsigned int mask, unsigned int value)
{
	unsigned int sfr_value = readl(sfr);

	set_mask_value(sfr_value, mask, value);
	writel(sfr_value, sfr);
}
#endif

#define clear_bit(data, offset) ((data) &= ~(0x1<<(offset)))
#define clear_bits(data, value, offset) ((data) &= ~((value)<<(offset)))
#define set_bit(data, offset) ((data) |= (0x1<<(offset)))
#define set_bits(data, value, offset) ((data) |= ((value)<<(offset)))
#define check_bit(data, offset)	(((data)>>(offset)) & (0x1))

/* For only external static functions */
static struct vts_data *p_vts_data;
static void vts_dbg_dump_fw_gpr(struct device *dev, struct vts_data *data,
			unsigned int dbg_type);

#ifdef CONFIG_SOC_EXYNOS9830_EVT0
static struct reserved_mem *vts_rmem;

static void *vts_rmem_vmap(struct reserved_mem *rmem)
{
	phys_addr_t phys = rmem->base;
	size_t size = rmem->size;
	unsigned int num_pages = (unsigned int)DIV_ROUND_UP(size, PAGE_SIZE);
	pgprot_t prot = pgprot_writecombine(PAGE_KERNEL);
	struct page **pages, **page;
	void *vaddr = NULL;

	pages = kcalloc(num_pages, sizeof(pages[0]), GFP_KERNEL);
	if (!pages)
		goto out;

	for (page = pages; (page - pages < num_pages); page++) {
		*page = phys_to_page(phys);
		phys += PAGE_SIZE;
	}

	vaddr = vmap(pages, num_pages, VM_MAP, prot);
	kfree(pages);
out:
	return vaddr;
}

static int __init vts_rmem_setup(struct reserved_mem *rmem)
{
	pr_info("%s: base=%pa, size=%pa\n", __func__, &rmem->base, &rmem->size);
	vts_rmem = rmem;
	return 0;
}

RESERVEDMEM_OF_DECLARE(vts_rmem, "exynos,vts_rmem", vts_rmem_setup);
#endif

static struct platform_driver samsung_vts_driver;

bool is_vts(struct device *dev)
{
	return (&samsung_vts_driver.driver) == dev->driver;
}

struct vts_data *vts_get_data(struct device *dev)
{
	while (dev && !is_vts(dev))
		dev = dev->parent;

	return dev ? dev_get_drvdata(dev) : NULL;
}

/* vts mailbox interface functions */
int vts_mailbox_generate_interrupt(
	const struct platform_device *pdev,
	int hw_irq)
{
	struct vts_data *data = p_vts_data;
	struct device *dev = data ? (data->pdev ?
				&data->pdev->dev : NULL) : NULL;
	unsigned long flag;
	int result = 0;

	if (!data || !dev) {
		vts_dev_warn(dev, "%s: VTS not Initialized\n", __func__);
		return -EINVAL;
	}

	spin_lock_irqsave(&data->state_spinlock, flag);
	/* Check VTS state before accessing mailbox */
	if (data->vts_state == VTS_STATE_RUNTIME_SUSPENDED ||
		data->vts_state == VTS_STATE_VOICECALL ||
		data->vts_state == VTS_STATE_NONE) {
		vts_dev_warn(dev, "%s: VTS wrong state [%d]\n", __func__,
			data->vts_state);
		result = -EINVAL;
		goto out;
	}
	result = mailbox_generate_interrupt(pdev, hw_irq);

out:
	spin_unlock_irqrestore(&data->state_spinlock, flag);
	return result;
}

void vts_mailbox_write_shared_register(
	const struct platform_device *pdev,
	const u32 *values,
	int start,
	int count)
{
	struct vts_data *data = p_vts_data;
	struct device *dev = data ? (data->pdev ?
				&data->pdev->dev : NULL) : NULL;
	unsigned long flag;

	if (!data || !dev) {
		vts_dev_warn(dev, "%s: VTS not Initialized\n", __func__);
		return;
	}

	spin_lock_irqsave(&data->state_spinlock, flag);
	/* Check VTS state before accessing mailbox */
	if (data->vts_state == VTS_STATE_RUNTIME_SUSPENDED ||
		data->vts_state == VTS_STATE_VOICECALL ||
		data->vts_state == VTS_STATE_NONE) {
		vts_dev_warn(dev, "%s: VTS wrong state [%d]\n", __func__,
			data->vts_state);
		goto out;
	}
	mailbox_write_shared_register(pdev, values, start, count);

out:
	spin_unlock_irqrestore(&data->state_spinlock, flag);
}

void vts_mailbox_read_shared_register(
	const struct platform_device *pdev,
	u32 *values,
	int start,
	int count)
{
	struct vts_data *data = p_vts_data;
	struct device *dev = data ? (data->pdev ?
				&data->pdev->dev : NULL) : NULL;
	unsigned long flag;

	if (!data || !dev) {
		vts_dev_warn(dev, "%s: VTS not Initialized\n", __func__);
		return;
	}

	spin_lock_irqsave(&data->state_spinlock, flag);
	/* Check VTS state before accessing mailbox */
	if (data && (data->vts_state == VTS_STATE_RUNTIME_SUSPENDED ||
		data->vts_state == VTS_STATE_VOICECALL ||
		data->vts_state == VTS_STATE_NONE)) {
		vts_dev_warn(dev, "%s: VTS wrong state [%d]\n", __func__,
			data->vts_state);
		goto out;
	}

	mailbox_read_shared_register(pdev, values, start, count);

out:
	spin_unlock_irqrestore(&data->state_spinlock, flag);
}

static int vts_start_ipc_transaction_atomic(
	struct device *dev, struct vts_data *data,
	int msg, u32 (*values)[3], int sync)
{
	unsigned long flag;
	long result = 0;
	u32 ack_value = 0;
	enum ipc_state *state = &data->ipc_state_ap;

	vts_dev_info(dev, "%s:++ msg:%d, values: 0x%08x, 0x%08x, 0x%08x\n",
		__func__, msg, (*values)[0],
		(*values)[1], (*values)[2]);

	/* Check VTS state before processing IPC,
	 * in VTS_STATE_RUNTIME_SUSPENDING state only Power Down IPC
	 * can be processed
	 */
	spin_lock_irqsave(&data->state_spinlock, flag);
	if ((data->vts_state == VTS_STATE_RUNTIME_SUSPENDING &&
		msg != VTS_IRQ_AP_POWER_DOWN) ||
		data->vts_state == VTS_STATE_RUNTIME_SUSPENDED ||
		data->vts_state == VTS_STATE_VOICECALL ||
		data->vts_state == VTS_STATE_NONE) {
		vts_dev_warn(dev, "%s: VTS IP %s state\n", __func__,
			(data->vts_state == VTS_STATE_VOICECALL ?
			"VoiceCall" : "Suspended"));
		spin_unlock_irqrestore(&data->state_spinlock, flag);
		return -EINVAL;
	}
	spin_unlock_irqrestore(&data->state_spinlock, flag);

	if (pm_runtime_suspended(dev)) {
		vts_dev_warn(dev, "%s: VTS IP is in suspended state, IPC cann't be processed\n",
			__func__);
		return -EINVAL;
	}

	if (!data->vts_ready) {
		vts_dev_warn(dev, "%s: VTS Firmware Not running\n", __func__);
		return -EINVAL;
	}

	spin_lock(&data->ipc_spinlock);

	*state = SEND_MSG;
	vts_mailbox_write_shared_register(data->pdev_mailbox, *values, 0, 3);
	vts_mailbox_generate_interrupt(data->pdev_mailbox, msg);
	data->running_ipc = msg;

	if (sync) {
		int i;

		for (i = 1000; i && (*state != SEND_MSG_OK) &&
				(*state != SEND_MSG_FAIL) &&
				(ack_value != (0x1 << msg)); i--) {
			vts_mailbox_read_shared_register(data->pdev_mailbox,
							&ack_value, 3, 1);
			vts_dev_dbg(dev, "%s ACK-value: 0x%08x\n", __func__,
				ack_value);
			udelay(50);
		}
		if (!i) {
			vts_dev_warn(dev, "Transaction timeout!! Ack_value:0x%x\n",
					ack_value);
			vts_dbg_dump_fw_gpr(dev, data, VTS_IPC_TRANS_FAIL);
			print_hex_dump(KERN_ERR, "vts-fw-log", DUMP_PREFIX_OFFSET, 32, 4,
					data->sramlog_baseaddr,
					0x800, true);
		}
		if (*state == SEND_MSG_OK || ack_value == (0x1 << msg)) {
			vts_dev_dbg(dev, "Transaction success Ack_value:0x%x\n",
					ack_value);
			if (ack_value == (0x1 << VTS_IRQ_AP_TEST_COMMAND) &&
				((*values)[0] == VTS_ENABLE_DEBUGLOG ||
				(*values)[0] == VTS_ENABLE_AUDIODUMP ||
				(*values)[0] == VTS_ENABLE_LOGDUMP)) {
				u32 ackvalues[3] = {0, 0, 0};

				mailbox_read_shared_register(data->pdev_mailbox,
					ackvalues, 0, 2);
				vts_dev_info(dev, "%s: offset: 0x%x size:0x%x\n",
					__func__, ackvalues[0], ackvalues[1]);
				if ((*values)[0] == VTS_ENABLE_DEBUGLOG) {
					/* Register debug log buffer */
					vts_register_log_buffer(dev,
						ackvalues[0], ackvalues[1]);
					vts_dev_dbg(dev, "%s: Log buffer\n",
						__func__);
				} else {
					u32 dumpmode =
					((*values)[0] == VTS_ENABLE_LOGDUMP ?
					VTS_LOG_DUMP : VTS_AUDIO_DUMP);
					/* register dump offset & size */
					vts_dump_addr_register(dev,
							ackvalues[0],
							ackvalues[1],
							dumpmode);
					vts_dev_dbg(dev, "%s: Dump buffer\n",
							__func__);
				}
			} else if (ack_value ==
				(0x1 << VTS_IRQ_AP_GET_VERSION)) {
				mailbox_read_shared_register(data->pdev_mailbox,
					&data->google_version, 2, 1);
				vts_dev_info(dev, "google version : %d",
					data->google_version);
			}
		} else {
			vts_dev_err(dev, "Transaction failed\n");
		}
		result = (*state == SEND_MSG_OK || ack_value) ? 0 : -EIO;
	}

	/* Clear running IPC & ACK value */
	ack_value = 0x0;
	vts_mailbox_write_shared_register(data->pdev_mailbox,
						&ack_value, 3, 1);
	data->running_ipc = 0;
	*state = IDLE;

	spin_unlock(&data->ipc_spinlock);
	vts_dev_info(dev, "%s:-- msg:%d\n", __func__, msg);

	return (int)result;
}

int vts_start_ipc_transaction(struct device *dev, struct vts_data *data,
		int msg, u32 (*values)[3], int atomic, int sync)
{
	return vts_start_ipc_transaction_atomic(dev, data, msg, values, sync);
}
EXPORT_SYMBOL(vts_start_ipc_transaction);

static int vts_ipc_ack(struct vts_data *data, u32 result)
{
	if (!data->vts_ready)
		return 0;

	pr_debug("%s(%p, %u)\n", __func__, data, result);
	vts_mailbox_write_shared_register(data->pdev_mailbox,
						&result, 0, 1);
	vts_mailbox_generate_interrupt(data->pdev_mailbox,
					VTS_IRQ_AP_IPC_RECEIVED);
	return 0;
}

int vts_send_ipc_ack(struct vts_data *data, u32 result)
{
	return vts_ipc_ack(data, result);
}

static void vts_cpu_power(bool on)
{
	pr_info("%s(%d)\n", __func__, on);

#ifndef EMULATOR
	exynos_pmu_update(VTS_CPU_CONFIGURATION, VTS_CPU_LOCAL_PWR_CFG,
		on ? VTS_CPU_LOCAL_PWR_CFG : 0);
#else
	update_mask_value(pmu_alive + VTS_CPU_CONFIGURATION,
		VTS_CPU_LOCAL_PWR_CFG, on ? VTS_CPU_LOCAL_PWR_CFG : 0);
#endif

#if (defined(CONFIG_SOC_EXYNOS9810) || defined(CONFIG_SOC_EXYNOS8895))
	if (!on) {
#ifndef EMULATOR
		exynos_pmu_update(VTS_CPU_OPTION,
				VTS_CPU_OPTION_USE_STANDBYWFI_MASK,
				VTS_CPU_OPTION_USE_STANDBYWFI_MASK);
#else
		update_mask_value(pmu_alive + VTS_CPU_OPTION,
				VTS_CPU_OPTION_USE_STANDBYWFI_MASK,
				VTS_CPU_OPTION_USE_STANDBYWFI_MASK);
#endif
	}
#endif
}

#if (defined(CONFIG_SOC_EXYNOS9810) || defined(CONFIG_SOC_EXYNOS8895))
static int vts_cpu_enable(bool enable)
{
	unsigned int mask = VTS_CPU_RESET_OPTION_ENABLE_CPU_MASK;
	unsigned int val = (enable ? mask : 0);
	unsigned int status = 0;
	unsigned long after;

	pr_info("%s(%d)\n", __func__, enable);

#ifndef EMULATOR
	exynos_pmu_update(VTS_CPU_RESET_OPTION, mask, val);
#else
	update_mask_value(pmu_alive + VTS_CPU_RESET_OPTION, mask, val);
#endif
	if (enable) {
		after = jiffies + LIMIT_IN_JIFFIES;
		do {
			schedule();
#ifndef EMULATOR
			exynos_pmu_read(VTS_CPU_STATUS, &status);
#else
			status = readl(pmu_alive + VTS_CPU_STATUS);
#endif
		} while (((status & VTS_CPU_STATUS_STATUS_MASK)
				!= VTS_CPU_STATUS_STATUS_MASK)
				&& time_is_after_eq_jiffies(after));
		if (time_is_before_jiffies(after)) {
			pr_err("vts cpu enable timeout\n");
			return -ETIME;
		}
	}

	return 0;
}
#else
static int vts_cpu_enable(bool enable)
{
	unsigned int status = 0;
	unsigned long after;

	after = jiffies + LIMIT_IN_JIFFIES;
	do {
		schedule();
		exynos_pmu_read(VTS_CPU_IN, &status);
	} while (((status & VTS_CPU_IN_SLEEPING_MASK)
		!= VTS_CPU_IN_SLEEPING_MASK)
		&& time_is_after_eq_jiffies(after));
	if (time_is_before_jiffies(after)) {
		pr_err("vts cpu enable timeout\n");
		return -ETIME;
	}

	return 0;
}
#endif

static void vts_reset_cpu(void)
{
#if (defined(CONFIG_SOC_EXYNOS9810) || defined(CONFIG_SOC_EXYNOS8895))
#ifndef EMULATOR
	vts_cpu_enable(false);
	vts_cpu_power(false);
	vts_cpu_power(true);
	vts_cpu_enable(true);
#endif
#else
	vts_cpu_enable(false);
	vts_cpu_power(false);
	vts_cpu_enable(true);
	vts_cpu_power(true);
#endif
}

#if IS_ENABLED(CONFIG_EXYNOS_IMGLOADER)
int vts_imgloader_mem_setup(
	struct imgloader_desc *desc, const u8 *metadata, size_t size,
	phys_addr_t *fw_phys_base, size_t *fw_bin_size, size_t *fw_mem_size)
{
	struct vts_data *data = dev_get_drvdata(desc->dev);

	vts_dev_info(desc->dev, "%s\n", __func__);
	if (!data) {
		vts_dev_err(desc->dev, "vts data is null\n");
		return -EINVAL;
	}

	if (!data->firmware) {
		vts_dev_err(desc->dev, "firmware is not loaded\n");
		return -EINVAL;
	}

	memcpy(data->sram_base, data->firmware->data, data->firmware->size);
	*fw_phys_base = data->sram_phys;
	*fw_bin_size = data->firmware->size;
	*fw_mem_size = data->sram_size;
	vts_dev_info(desc->dev, "firmware is downloaded to %pK (phy = %zu size=%zu)\n",
		data->sram_base, data->sram_phys, data->firmware->size);
	return 0;
}

int vts_imgloader_verify_fw(struct imgloader_desc *desc,
	phys_addr_t fw_phys_base,
	size_t fw_bin_size, size_t fw_mem_size)
{
	uint64_t ret64 = 0;

	ret64 = exynos_verify_subsystem_fw(desc->name, desc->fw_id,
					fw_phys_base, fw_bin_size, fw_mem_size);
	vts_dev_info(desc->dev, "exynos_verify_subsystem_fw sram_size %zu, phy = %zu size=%zu)\n",
		fw_mem_size, fw_phys_base, fw_bin_size);
	if (ret64) {
		vts_dev_warn(desc->dev, "Failed F/W verification, ret=%llu\n",
			ret64);
		return -EIO;
	}
	vts_cpu_power(true);
	ret64 = exynos_request_fw_stage2_ap(desc->name);
	if (ret64) {
		vts_dev_warn(desc->dev, "Failed F/W verification to S2MPU, ret=%llu\n",
			ret64);
		return -EIO;
	}
	return 0;
}

struct imgloader_ops vts_imgloader_ops = {
	.mem_setup = vts_imgloader_mem_setup,
	.verify_fw = vts_imgloader_verify_fw,
};

static int vts_core_imgloader_desc_init(struct platform_device *pdev)
{
	struct vts_data *data = platform_get_drvdata(pdev);
	struct imgloader_desc *desc = &data->vts_imgloader_desc;

	desc->dev = &pdev->dev;
	desc->owner = THIS_MODULE;
	desc->ops = &vts_imgloader_ops;
	desc->fw_name = "vts.bin";
	desc->name = "VTS";
	desc->s2mpu_support = false;
	desc->skip_request_firmware = true;
	desc->fw_id = 0;
	return imgloader_desc_init(desc);
}
#endif

static int vts_download_firmware(struct platform_device *pdev)
{
	struct vts_data *data = platform_get_drvdata(pdev);
	struct device *dev = &pdev->dev;

	vts_dev_info(dev, "%s\n", __func__);
	if (!data->firmware) {
		vts_dev_err(dev, "firmware is not loaded\n");
		return -EAGAIN;
	}

	memcpy(data->sram_base, data->firmware->data, data->firmware->size);
	vts_dev_info(dev, "firmware is downloaded to %pK (size=%zu)\n",
		data->sram_base, data->firmware->size);
	return 0;
}

static int vts_wait_for_fw_ready(struct device *dev)
{
	struct vts_data *data = dev_get_drvdata(dev);
	int result;

	result = wait_event_timeout(data->ipc_wait_queue,
		data->vts_ready, msecs_to_jiffies(3000));
	if (data->vts_ready) {
		result = 0;
	} else {
		vts_dev_err(dev, "VTS Firmware is not ready\n");
		vts_dbg_dump_fw_gpr(dev, data, VTS_FW_NOT_READY);
		result = -ETIME;
	}

	return result;
}

void vts_pad_retention(bool retention)
{
	if (!retention) {
#if (defined(CONFIG_SOC_EXYNOS9810) || defined(CONFIG_SOC_EXYNOS8895))
		exynos_pmu_update(PAD_RETENTION_VTS_OPTION,
			0x10000000, 0x10000000);
#else
		exynos_pmu_update(PAD_RETENTION_VTS_OPTION,
			0x1 << 11, 0x1 << 11);
#endif
	}
}
EXPORT_SYMBOL(vts_pad_retention);

#if !(defined(CONFIG_SOC_EXYNOS8895))
static u32 vts_set_baaw(void __iomem *sfr_base, u64 base, u32 size)
{
	u32 aligned_size = round_up(size, SZ_4M);
	u64 aligned_base = round_down(base, aligned_size);

	/* set VTS BAAW config */
	writel(0x40100, sfr_base);
	writel(0x40200, sfr_base + 0x4);
	writel(0x15900, sfr_base + 0x8);
	writel(0x80000003, sfr_base + 0xC);

	writel(VTS_BAAW_BASE / SZ_4K, sfr_base + VTS_BAAW_SRC_START_ADDRESS);
	writel((VTS_BAAW_BASE + aligned_size) / SZ_4K,
		sfr_base + VTS_BAAW_SRC_END_ADDRESS);
	writel(aligned_base / SZ_4K, sfr_base + VTS_BAAW_REMAPPED_ADDRESS);
	writel(0x80000003, sfr_base + VTS_BAAW_INIT_DONE);

	return base - aligned_base + VTS_BAAW_BASE;
}
#endif

int vts_acquire_sram(struct platform_device *pdev, int vts)
{
#ifdef CONFIG_SOC_EXYNOS8895
	struct vts_data *data = platform_get_drvdata(pdev);
	int previous;
	unsigned long flag;

	if (IS_ENABLED(CONFIG_SOC_EXYNOS8895)) {
		vts_dev_info(&pdev->dev, "%s(%d)\n", __func__, vts);

		if (!vts) {
			while (pm_runtime_active(&pdev->dev)) {
				vts_dev_warn(&pdev->dev,
					"%s Clear existing active states\n",
					__func__);
				pm_runtime_put_sync(&pdev->dev);
			}
		}
		previous = test_and_set_bit(0, &data->sram_acquired);
		if (previous) {
			vts_dev_err(&pdev->dev, "sram acquisition failed\n");
			return -EBUSY;
		}

		if (!vts) {
			pm_runtime_get_sync(&pdev->dev);
			data->voicecall_enabled = true;
			spin_lock_irqsave(&data->state_spinlock, flag);
			data->vts_state = VTS_STATE_VOICECALL;
			spin_unlock_irqrestore(&data->state_spinlock, flag);
		}

		writel((vts ? 0 : 1) << VTS_MEM_SEL_OFFSET,
			data->sfr_base + VTS_SHARED_MEM_CTRL);
	}
#endif

	return 0;
}
EXPORT_SYMBOL(vts_acquire_sram);

int vts_release_sram(struct platform_device *pdev, int vts)
{
#ifdef CONFIG_SOC_EXYNOS8895
	struct vts_data *data = platform_get_drvdata(pdev);

	vts_dev_info(&pdev->dev, "%s(%d)\n", __func__, vts);

	if (IS_ENABLED(CONFIG_SOC_EXYNOS8895)) {
		if (test_bit(0, &data->sram_acquired) &&
				(data->voicecall_enabled || vts)) {
			writel(0 << VTS_MEM_SEL_OFFSET,
					data->sfr_base + VTS_SHARED_MEM_CTRL);
			clear_bit(0, &data->sram_acquired);

			if (!vts) {
				pm_runtime_put_sync(&pdev->dev);
				data->voicecall_enabled = false;
			}
			vts_dev_info(&pdev->dev, "%s(%d) completed\n",
					__func__, vts);
		} else
			vts_dev_warn(&pdev->dev, "%s(%d) already released\n",
					__func__, vts);
	}

#endif
	return 0;
}
EXPORT_SYMBOL(vts_release_sram);

int vts_clear_sram(struct platform_device *pdev)
{
	struct vts_data *data = platform_get_drvdata(pdev);

	pr_info("%s\n", __func__);

	memset(data->sram_base, 0, data->sram_size);

	return 0;
}
EXPORT_SYMBOL(vts_clear_sram);

bool vts_is_on(void)
{
	pr_info("%s : %d\n", __func__,
		(p_vts_data && p_vts_data->enabled));
	return p_vts_data && p_vts_data->enabled;
}
EXPORT_SYMBOL(vts_is_on);

bool vts_is_recognitionrunning(void)
{
	return p_vts_data && p_vts_data->running;
}
EXPORT_SYMBOL(vts_is_recognitionrunning);

static struct snd_soc_dai_driver vts_dai[] = {
	{
		.name = "vts-tri",
		.id = 0,
		.capture = {
			.stream_name = "VTS Trigger Capture",
			.channels_min = 1,
			.channels_max = 2,
			.rates = SNDRV_PCM_RATE_16000,
			.formats = SNDRV_PCM_FMTBIT_S16,
			.sig_bits = 16,
		 },
	},
	{
		.name = "vts-rec",
		.id = 1,
		.capture = {
			.stream_name = "VTS Capture",
			.channels_min = 1,
			.channels_max = 2,
			.rates = SNDRV_PCM_RATE_16000,
			.formats = SNDRV_PCM_FMTBIT_S16,
			.sig_bits = 16,
		 },
	},
};

static const char * const vts_hpf_sel_texts[] = {"120Hz", "40Hz"};
static SOC_ENUM_SINGLE_DECL(vts_hpf_sel, VTS_DMIC_CONTROL_DMIC_IF,
	VTS_DMIC_HPF_SEL_OFFSET, vts_hpf_sel_texts);

static const char * const vts_cps_sel_texts[] = {"normal", "absolute"};
static SOC_ENUM_SINGLE_DECL(vts_cps_sel, VTS_DMIC_CONTROL_DMIC_IF,
	VTS_DMIC_CPS_SEL_OFFSET, vts_cps_sel_texts);

static const DECLARE_TLV_DB_SCALE(vts_gain_tlv_array, 0, 6, 0);

static const char * const vts_sys_sel_texts[] = {"512kHz", "768kHz",
	"384kHz", "2048kHz", "3072kHz_48kHz", "3072kHz_96kHz"};
static SOC_ENUM_SINGLE_DECL(vts_sys_sel, VTS_DMIC_CONTROL_DMIC_IF,
	VTS_DMIC_SYS_SEL_OFFSET, vts_sys_sel_texts);

static int vts_sys_sel_put_enum(
	struct snd_kcontrol *kcontrol, struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_component *component =
		snd_soc_kcontrol_component(kcontrol);
	struct device *dev = component->dev;
	unsigned int *item = ucontrol->value.enumerated.item;
	struct vts_data *data = p_vts_data;

	vts_dev_dbg(dev, "%s(%u)\n", __func__, item[0]);

	data->syssel_rate = item[0];

	return snd_soc_put_enum_double(kcontrol, ucontrol);
}

static const char * const vts_polarity_clk_texts[] = {"rising edge of clock",
	"falling edge of clock"};
static SOC_ENUM_SINGLE_DECL(vts_polarity_clk, VTS_DMIC_CONTROL_DMIC_IF,
	VTS_DMIC_POLARITY_CLK_OFFSET, vts_polarity_clk_texts);

static const char * const vts_polarity_output_texts[] = {"right first",
	"left first"};
static SOC_ENUM_SINGLE_DECL(vts_polarity_output, VTS_DMIC_CONTROL_DMIC_IF,
	VTS_DMIC_POLARITY_OUTPUT_OFFSET, vts_polarity_output_texts);

static const char * const vts_polarity_input_texts[] = {"left PDM on CLK high",
	"left PDM on CLK low"};
static SOC_ENUM_SINGLE_DECL(vts_polarity_input, VTS_DMIC_CONTROL_DMIC_IF,
	VTS_DMIC_POLARITY_INPUT_OFFSET, vts_polarity_input_texts);

static const char * const vts_ovfw_ctrl_texts[] = {"limit", "reset"};
static SOC_ENUM_SINGLE_DECL(vts_ovfw_ctrl, VTS_DMIC_CONTROL_DMIC_IF,
	VTS_DMIC_OVFW_CTRL_OFFSET, vts_ovfw_ctrl_texts);

static const char * const vts_cic_sel_texts[] = {"Off", "On"};
static SOC_ENUM_SINGLE_DECL(vts_cic_sel, VTS_DMIC_CONTROL_DMIC_IF,
	VTS_DMIC_CIC_SEL_OFFSET, vts_cic_sel_texts);

static const char * const vtsvcrecog_mode_text[] = {
	"OFF", "VOICE_TRIGGER_MODE_ON", "SOUND_DETECT_MODE_ON",
	 "VT_ALWAYS_ON_MODE_ON", "GOOGLE_TRIGGER_MODE_ON",
	 "SENSORY_TRIGGER_MODE_ON", "VOICE_TRIGGER_MODE_OFF",
	 "SOUND_DETECT_MODE_OFF", "VT_ALWAYS_ON_MODE_OFF",
	 "GOOGLE_TRIGGER_MODE_OFF", "SENSORY_TRIGGER_MODE_OFF"
};

static const char * const vtsactive_phrase_text[] = {
	"SVOICE", "GOOGLE", "SENSORY"
};
static const char * const vtsforce_reset_text[] = {
	"NONE", "RESET"
};
static SOC_ENUM_SINGLE_EXT_DECL(vtsvcrecog_mode_enum, vtsvcrecog_mode_text);
static SOC_ENUM_SINGLE_EXT_DECL(vtsactive_phrase_enum, vtsactive_phrase_text);
static SOC_ENUM_SINGLE_EXT_DECL(vtsforce_reset_enum, vtsforce_reset_text);

static void vts_complete_firmware_request(
	const struct firmware *fw, void *context)
{
	struct platform_device *pdev = context;
	struct device *dev = &pdev->dev;
	struct vts_data *data = platform_get_drvdata(pdev);
	unsigned int *pversion = NULL;

	if (!fw) {
		vts_dev_err(dev, "Failed to request firmware\n");
		return;
	}

	data->firmware = fw;
	pversion = (unsigned int *) (fw->data + VTSFW_VERSION_OFFSET);
	data->vtsfw_version = *pversion;
	pversion = (unsigned int *) (fw->data + DETLIB_VERSION_OFFSET);
	data->vtsdetectlib_version = *pversion;

	vts_dev_info(dev, "Firmware loaded at %p (%zu)\n", fw->data, fw->size);
	vts_dev_info(dev, "Firmware version: 0x%x library version: 0x%x\n",
		data->vtsfw_version, data->vtsdetectlib_version);
}

static int vts_start_recognization(struct device *dev, int start)
{
	struct vts_data *data = dev_get_drvdata(dev);
	int active_trigger = data->active_trigger;
	int result;
	u32 values[3];

	vts_dev_info(dev, "%s for %s start %d\n", __func__,
		vtsactive_phrase_text[active_trigger], start);

	start = !!start;
	if (start) {
		vts_dev_info(dev, "%s for %s G-loaded:%d s-loaded: %d\n",
			__func__, vtsactive_phrase_text[active_trigger],
			data->google_info.loaded, data->svoice_info.loaded);
		vts_dev_info(dev, "%s exec_mode %d active_trig :%d\n", __func__,
			data->exec_mode, active_trigger);
		if (!(data->exec_mode & (0x1 << VTS_SOUND_DETECT_MODE))) {
			if (active_trigger == TRIGGER_SVOICE &&
				 data->svoice_info.loaded) {
				/*
				 * load svoice model.bin @ offset 0x2A800
				 * file before starting recognition
				 */
				if (data->svoice_info.actual_sz >
					SOUND_MODEL_SVOICE_SIZE_MAX) {
					vts_dev_err(dev, "Failed %s Requested size[0x%zx] > supported[0x%x]\n",
					"svoice.bin",
					data->svoice_info.actual_sz,
					SOUND_MODEL_SVOICE_SIZE_MAX);
					return -EINVAL;
				}
				memcpy(data->sram_base +
					SOUND_MODEL_SVOICE_OFFSET,
					data->svoice_info.data,
					data->svoice_info.actual_sz);
				vts_dev_info(dev, "svoice.bin Binary uploaded size=%zu\n",
					data->svoice_info.actual_sz);

			} else if (active_trigger == TRIGGER_GOOGLE &&
				data->google_info.loaded) {
				/*
				 * load google model.bin @ offset 0x32B00
				 * file before starting recognition
				 */
				if (data->google_info.actual_sz >
					SOUND_MODEL_GOOGLE_SIZE_MAX) {
					vts_dev_err(dev, "Failed %s Requested size[0x%zx] > supported[0x%x]\n",
					"google.bin",
					data->google_info.actual_sz,
					SOUND_MODEL_GOOGLE_SIZE_MAX);
					return -EINVAL;
				}
				memcpy(data->sram_base +
					SOUND_MODEL_GOOGLE_OFFSET,
					data->google_info.data,
					data->google_info.actual_sz);
				vts_dev_info(dev, "google.bin Binary uploaded size=%zu\n",
						data->google_info.actual_sz);
			} else {
				vts_dev_err(dev, "%s Model Binary File not Loaded\n",
					__func__);
				return -EINVAL;
			}
		}

		result = vts_set_dmicctrl(data->pdev,
				((active_trigger == TRIGGER_SVOICE) ?
				VTS_MICCONF_FOR_TRIGGER
				: VTS_MICCONF_FOR_GOOGLE), true);
		if (result < 0) {
			vts_dev_err(dev, "%s: MIC control failed\n",
						 __func__);
			return result;
		}

		if (data->exec_mode & (0x1 << VTS_SOUND_DETECT_MODE)) {
			int ctrl_dmicif;
			/* set VTS Gain for LPSD mode */
			ctrl_dmicif = readl(data->dmic_if0_base
				+ VTS_DMIC_CONTROL_DMIC_IF);
			ctrl_dmicif &= ~(0x7 << VTS_DMIC_GAIN_OFFSET);
			set_bits(ctrl_dmicif, data->lpsdgain,
				VTS_DMIC_GAIN_OFFSET);
			writel(ctrl_dmicif, data->dmic_if0_base
				+ VTS_DMIC_CONTROL_DMIC_IF);
#ifdef VTS_2MIC_NS
			writel(ctrl_dmicif, data->dmic_if1_base
				+ VTS_DMIC_CONTROL_DMIC_IF);
#endif
		}

		/* Send Start recognition IPC command to VTS */
		values[0] = 1 << active_trigger;
		values[1] = 0;
		values[2] = 0;
		result = vts_start_ipc_transaction(dev, data,
				VTS_IRQ_AP_START_RECOGNITION,
				&values, 0, 1);
		if (result < 0) {
			vts_dev_err(dev, "vts ipc VTS_IRQ_AP_START_RECOGNITION failed: %d\n",
				result);
			return result;
		}

		data->vts_state = VTS_STATE_RECOG_STARTED;
		vts_dev_info(dev, "%s start=%d, active_trigger=%d\n", __func__,
			start, active_trigger);
	} else if (!start) {
		values[0] = 1 << active_trigger;
		values[1] = 0;
		values[2] = 0;
		result = vts_start_ipc_transaction(dev, data,
				VTS_IRQ_AP_STOP_RECOGNITION,
				&values, 0, 1);
		if (result < 0) {
			vts_dev_err(dev, "vts ipc VTS_IRQ_AP_STOP_RECOGNITION failed: %d\n",
				result);
			return result;
		}

		data->vts_state = VTS_STATE_RECOG_STOPPED;
		vts_dev_info(dev, "%s start=%d, active_trigger=%d\n", __func__,
			start, active_trigger);

		if (data->clk_path == VTS_CLK_SRC_RCO) {
			result = vts_set_dmicctrl(data->pdev,
					((active_trigger == TRIGGER_SVOICE) ?
					VTS_MICCONF_FOR_TRIGGER
					: VTS_MICCONF_FOR_GOOGLE), false);
			if (result < 0) {
				vts_dev_err(dev, "%s: MIC control failed\n",
							 __func__);
				return result;
			}
		}
	}
	return 0;
}

static int get_vtsvoicerecognize_mode(struct snd_kcontrol *kcontrol,
	struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_component *component =
		snd_soc_kcontrol_component(kcontrol);
	struct vts_data *data = p_vts_data;

	ucontrol->value.integer.value[0] = data->exec_mode;

	vts_dev_dbg(component->dev, "GET VTS Execution mode: %d\n",
			 data->exec_mode);

	return 0;
}

static int set_voicerecognize_mode(struct device *dev, int vcrecognize_mode)
{
	struct vts_data *data = dev_get_drvdata(dev);
	u32 values[3];
	int result = 0;
	int vcrecognize_start = 0;

	u32 keyword_type = 1;
	char env[100] = {0,};
	char *envp[2] = {env, NULL};
	int loopcnt = 10;

	pm_runtime_barrier(dev);

	while (data->voicecall_enabled) {
		vts_dev_warn(dev, "%s voicecall (%d)\n", __func__,
			data->voicecall_enabled);

		if (loopcnt <= 0) {
			vts_dev_warn(dev, "%s VTS SRAM is Used for CP call\n",
				__func__);

			keyword_type = -EBUSY;
			snprintf(env, sizeof(env),
				 "VOICE_WAKEUP_WORD_ID=%x",
				 keyword_type);

			kobject_uevent_env(&dev->kobj, KOBJ_CHANGE, envp);
			return -EBUSY;
		}

		loopcnt--;
		usleep_range(9990, 10000);
	}

	vts_dev_warn(dev, "%s voicecall (%d) (End)\n", __func__,
		data->voicecall_enabled);

	if (vcrecognize_mode < VTS_VOICE_TRIGGER_MODE ||
		vcrecognize_mode >= VTS_MODE_COUNT) {
		vts_dev_err(dev,
		"Invalid voice control mode =%d", vcrecognize_mode);
		return 0;
	}

	vts_dev_info(dev, "%s Current: %d requested %s\n",
			 __func__, data->exec_mode,
			 vtsvcrecog_mode_text[vcrecognize_mode]);
	if ((vcrecognize_mode < VTS_VOICE_TRIGGER_MODE_OFF &&
		data->exec_mode & (0x1 << vcrecognize_mode)) ||
		(vcrecognize_mode >= VTS_VOICE_TRIGGER_MODE_OFF &&
		!(data->exec_mode & (0x1 << (vcrecognize_mode -
			VTS_SENSORY_TRIGGER_MODE))))) {
		vts_dev_err(dev,
		"Requested Recognition mode=%d already completed",
		 vcrecognize_mode);
		return 0;
	}

	if (vcrecognize_mode <= VTS_SENSORY_TRIGGER_MODE) {
		pm_runtime_get_sync(dev);
		vts_start_runtime_resume(dev, 0);
		vts_clk_set_rate(dev, data->syssel_rate);
		vcrecognize_start = true;
	} else
		vcrecognize_start = false;

	if (!pm_runtime_active(dev)) {
		vts_dev_warn(dev, "%s wrong state %d req: %d\n",
				__func__, data->exec_mode,
				vcrecognize_mode);
		return 0;
	}

	values[0] = vcrecognize_mode;
	values[1] = 0;
	values[2] = 0;
	result = vts_start_ipc_transaction(dev,
			 data, VTS_IRQ_AP_SET_MODE,
			 &values, 0, 1);
	if (result < 0) {
		vts_dev_err(dev, "%s IPC transaction Failed\n",
			vtsvcrecog_mode_text[vcrecognize_mode]);
		goto err_ipcmode;
	}

	if (vcrecognize_start)
		data->exec_mode |= (0x1 << vcrecognize_mode);
	else
		data->exec_mode &= ~(0x1 << (vcrecognize_mode -
					VTS_SENSORY_TRIGGER_MODE));

	/* Start/stop the request Voice recognization mode */
	result = vts_start_recognization(dev,
					vcrecognize_start);
	if (result < 0) {
		vts_dev_err(dev, "Start Recognization Failed: %d\n",
			 result);
		goto err_ipcmode;
	}

	if (vcrecognize_start)
		data->voicerecog_start |= (0x1 << data->active_trigger);
	else
		data->voicerecog_start &=
			~(0x1 << data->active_trigger);

	vts_dev_info(dev, "%s Configured: [%d] %s started\n",
		 __func__, data->exec_mode,
		 vtsvcrecog_mode_text[vcrecognize_mode]);

	if (!vcrecognize_start &&
			pm_runtime_active(dev)) {
		pm_runtime_put_sync(dev);
	}
	return  0;

err_ipcmode:
	if (pm_runtime_active(dev) && (vcrecognize_start ||
		data->exec_mode & (0x1 << vcrecognize_mode) ||
		data->voicerecog_start & (0x1 << data->active_trigger))) {
		pm_runtime_put_sync(dev);
	}

	if (!vcrecognize_start) {
		data->exec_mode &= ~(0x1 << (vcrecognize_mode -
					VTS_SENSORY_TRIGGER_MODE));
		data->voicerecog_start &= ~(0x1 << data->active_trigger);
	}

	return result;
}

static int set_vtsvoicerecognize_mode(struct snd_kcontrol *kcontrol,
	struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_component *component =
		snd_soc_kcontrol_component(kcontrol);
	struct device *dev = component->dev;
	struct vts_data *data = dev_get_drvdata(dev);
	int vcrecognize_mode = 0;
	int ret = 0;

	vcrecognize_mode = ucontrol->value.integer.value[0];
	vts_dev_info(dev, "%s state %d %d (%d)\n", __func__,
			data->vts_state,
			data->clk_path,
			data->micclk_init_cnt);

	/* check dmic_if clk */
	if (data->clk_path == VTS_CLK_SRC_AUD0) {
		data->syssel_rate = 4;
		vts_dev_info(dev, "changed as 3072000Hz mode\n");
	}
	ret = set_voicerecognize_mode(dev, vcrecognize_mode);

	return ret;
}

int vts_chk_dmic_clk_mode(struct device *dev)
{
	struct vts_data *data = dev_get_drvdata(dev);
	int ret = 0;
	int micconf_type = 0;
	int syssel_rate_value = 1;
	u32 values[3] = {0, 0, 0};

	vts_dev_info(dev, "%s state %d %d (%d)\n",
		__func__, data->vts_state,
	    data->clk_path,
	    data->micclk_init_cnt);

    /* already started recognization mode and dmic_if clk is 3.072MHz*/
	/* VTS worked + Serial LIF */
	if (data->clk_path == VTS_CLK_SRC_RCO)
		syssel_rate_value = 1;
	else
		syssel_rate_value = 4;

	if (data->vts_state == VTS_STATE_RECOG_STARTED) {
		/* Send Start recognition IPC command to VTS */
		values[0] = 1 << data->active_trigger;
		values[1] = 0;
		values[2] = 0;
		ret = vts_start_ipc_transaction(dev, data,
				VTS_IRQ_AP_STOP_RECOGNITION,
				&values, 0, 1);
		if (ret < 0) {
			vts_dev_err(dev, "vts ipc VTS_IRQ_AP_STOP_RECOGNITION failed: %d\n",
				ret);
			return ret;
		}
		data->syssel_rate = syssel_rate_value;
		vts_clk_set_rate(dev, data->syssel_rate);
		if (data->micclk_init_cnt)
			data->micclk_init_cnt--;
		micconf_type = (data->active_trigger == TRIGGER_SVOICE) ?
				VTS_MICCONF_FOR_TRIGGER
				: VTS_MICCONF_FOR_GOOGLE;
		data->mic_ready &= ~(0x1 << micconf_type);
		ret = vts_set_dmicctrl(data->pdev, micconf_type, true);
		if (ret < 0)
			vts_dev_err(dev, "%s: MIC control failed\n", __func__);
		vts_dev_info(dev, "VTS Trigger : Change SYS_SEL : %d\n",
			syssel_rate_value);
		ret = vts_start_ipc_transaction(dev, data,
				VTS_IRQ_AP_START_RECOGNITION,
				&values, 0, 1);
		if (ret < 0) {
			vts_dev_err(dev, "vts ipc VTS_IRQ_AP_START_RECOGNITION failed: %d\n",
				ret);
			return ret;
		}
		/* 1ms requires (16KHz,16bit,Mono) =
		 * 16samples * 2 bytes = 32 bytes
		 */
		values[0] = data->target_size * 32;
		values[1] = 1 << data->active_trigger;
		values[2] = 0;
		ret = vts_start_ipc_transaction(dev, data,
			VTS_IRQ_AP_TARGET_SIZE,	&values, 0, 1);
		if (ret < 0) {
			vts_dev_err(dev, "Voice Trigger Value setting IPC Transaction Failed: %d\n",
				ret);
			return ret;
		}
		vts_dev_info(dev, "SET Voice Trigger Value: %dms\n",
			data->target_size);
	}

	if (data->vts_state == VTS_STATE_RECOG_TRIGGERED) {
		ret = vts_start_ipc_transaction(dev, data, VTS_IRQ_AP_STOP_COPY,
			&values, 1, 1);
		data->syssel_rate = syssel_rate_value;
		vts_clk_set_rate(dev, data->syssel_rate);
		if (data->micclk_init_cnt)
			data->micclk_init_cnt--;
		ret = vts_set_dmicctrl(data->pdev,
			((data->active_trigger == TRIGGER_SVOICE) ?
			VTS_MICCONF_FOR_TRIGGER : VTS_MICCONF_FOR_GOOGLE),
			true);
		if (ret < 0)
			vts_dev_err(dev, "%s: MIC control failed\n", __func__);
		vts_dev_info(dev, "VTS Triggered Fail : Serial LIF ON\n");
		ret = vts_start_ipc_transaction(dev, data,
			VTS_IRQ_AP_START_COPY, &values, 1, 1);
	}

	if (data->vts_rec_state == VTS_REC_STATE_START) {
		ret = vts_start_ipc_transaction(dev, data,
			VTS_IRQ_AP_STOP_REC, &values, 1, 1);
		data->syssel_rate = syssel_rate_value;
		vts_clk_set_rate(dev, data->syssel_rate);
		if (data->micclk_init_cnt)
			data->micclk_init_cnt--;
		ret = vts_set_dmicctrl(data->pdev,
			VTS_MICCONF_FOR_RECORD, true);
		if (ret < 0)
			vts_dev_err(dev, "%s: MIC control failed\n", __func__);
		vts_dev_info(dev, "VTS Recoding : Change SYS_SEL : %d\n",
			syssel_rate_value);
		ret = vts_start_ipc_transaction(dev, data,
			VTS_IRQ_AP_START_REC, &values, 1, 1);
	}
	return ret;
}
EXPORT_SYMBOL(vts_chk_dmic_clk_mode);

#ifdef CONFIG_SOC_EXYNOS8895
static int set_vtsexec_mode(struct snd_kcontrol *kcontrol,
	struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_component *component =
		snd_soc_kcontrol_component(kcontrol);
	struct vts_data *data = p_vts_data;
	u32 values[3];
	int result = 0;
	int vtsexecution_mode;

	u32 keyword_type = 1;
	char env[100] = {0,};
	char *envp[2] = {env, NULL};
	struct device *dev = &data->pdev->dev;
	int loopcnt = 10;

	pm_runtime_barrier(component->dev);

	while (data->voicecall_enabled) {
		vts_dev_warn(component->dev,
			"%s voicecall (%d)\n", __func__,
			data->voicecall_enabled);

		if (loopcnt <= 0) {
			vts_dev_warn(component->dev, "%s VTS SRAM is Used for CP call\n",
				__func__);

			keyword_type = -EBUSY;
			snprintf(env, sizeof(env),
				 "VOICE_WAKEUP_WORD_ID=%x",
				 keyword_type);

			kobject_uevent_env(&dev->kobj, KOBJ_CHANGE, envp);
			return -EBUSY;
		}

		loopcnt--;
		usleep_range(9990, 10000);
	}

	vts_dev_warn(component->dev, "%s voicecall (%d) (End)\n",
		__func__, data->voicecall_enabled);

	vtsexecution_mode = ucontrol->value.integer.value[0];

	if (vtsexecution_mode >= VTS_MODE_COUNT) {
		vts_dev_err(component->dev,
		"Invalid voice control mode =%d", vtsexecution_mode);
		return 0;
	}

	vts_dev_info(component->dev, "%s Current: %d requested %s\n",
			 __func__, data->exec_mode,
			 vtsexec_mode_text[vtsexecution_mode]);
	if (data->exec_mode == VTS_OFF_MODE &&
		 vtsexecution_mode != VTS_OFF_MODE) {
		pm_runtime_get_sync(component->dev);
		vts_start_runtime_resume(component->dev, 0);
		vts_clk_set_rate(component->dev, data->syssel_rate);
	}

	if (pm_runtime_active(component->dev)) {
		values[0] = vtsexecution_mode;
		values[1] = 0;
		values[2] = 0;
		result = vts_start_ipc_transaction(component->dev,
				 data, VTS_IRQ_AP_SET_MODE,
				 &values, 0, 1);
		if (result < 0) {
			vts_dev_err(component->dev,
				"%s SET_MODE IPC transaction Failed\n",
				vtsexec_mode_text[vtsexecution_mode]);
			if (data->exec_mode == VTS_OFF_MODE &&
				 vtsexecution_mode != VTS_OFF_MODE)
				pm_runtime_put_sync(component->dev);
			return result;
		}
	}
	if (vtsexecution_mode <= VTS_SENSORY_TRIGGER_MODE)
		data->exec_mode |= (0x1 << vtsexecution_mode);
	else
		data->exec_mode &= ~(0x1 << (vtsexecution_mode -
					VTS_SENSORY_TRIGGER_MODE));
	vts_dev_info(component->dev, "%s Configured: [%d] %s\n",
		 __func__, data->exec_mode,
		 vtsexec_mode_text[vtsexecution_mode]);

	if (data->exec_mode == VTS_OFF_MODE &&
		 pm_runtime_active(component->dev)) {
		pm_runtime_put_sync(component->dev);
	}
	return  0;
}
#endif

static int get_vtsactive_phrase(struct snd_kcontrol *kcontrol,
	struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_component *component =
		snd_soc_kcontrol_component(kcontrol);
	struct vts_data *data = p_vts_data;

	ucontrol->value.integer.value[0] = data->active_trigger;

	vts_dev_dbg(component->dev, "GET VTS Active Phrase: %s\n",
			vtsactive_phrase_text[data->active_trigger]);

	return 0;
}

static int set_vtsactive_phrase(struct snd_kcontrol *kcontrol,
	struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_component *component =
		snd_soc_kcontrol_component(kcontrol);
	struct vts_data *data = p_vts_data;
	int vtsactive_phrase;

	vtsactive_phrase = ucontrol->value.integer.value[0];

	if (vtsactive_phrase < 0 || vtsactive_phrase > 2) {
		vts_dev_err(component->dev,
		"Invalid VTS Trigger Key phrase =%d", vtsactive_phrase);
		return 0;
	}

	data->active_trigger = vtsactive_phrase;
	vts_dev_info(component->dev, "VTS Active phrase: %s\n",
		vtsactive_phrase_text[vtsactive_phrase]);

	return  0;
}

static int get_voicetrigger_value(struct snd_kcontrol *kcontrol,
	struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_component *component =
		snd_soc_kcontrol_component(kcontrol);
	struct vts_data *data = p_vts_data;

	ucontrol->value.integer.value[0] = data->target_size;

	vts_dev_info(component->dev, "GET Voice Trigger Value: %d\n",
			data->target_size);

	return 0;
}

static int set_voicetrigger_value(struct snd_kcontrol *kcontrol,
	struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_component *component =
		snd_soc_kcontrol_component(kcontrol);
	struct vts_data *data = p_vts_data;
	int active_trigger = data->active_trigger;
	u32 values[3];
	int result = 0;
	int trig_ms;

	pm_runtime_barrier(component->dev);

	if (data->voicecall_enabled) {
		u32 keyword_type = 1;
		char env[100] = {0,};
		char *envp[2] = {env, NULL};
		struct device *dev = &data->pdev->dev;

		vts_dev_warn(component->dev, "%s VTS SRAM is Used for CP call\n",
			__func__);
		keyword_type = -EBUSY;
		snprintf(env, sizeof(env),
			 "VOICE_WAKEUP_WORD_ID=%x",
			 keyword_type);

		kobject_uevent_env(&dev->kobj, KOBJ_CHANGE, envp);
		return -EBUSY;
	}

	trig_ms = ucontrol->value.integer.value[0];

	if (trig_ms > 2000 || trig_ms < 0) {
		vts_dev_err(component->dev,
		"Invalid Voice Trigger Value = %d (valid range 0~2000ms)",
			trig_ms);
		return 0;
	}

	/* Configure VTS target size */
	/* 1ms requires (16KHz,16bit,Mono) = 16samples * 2 bytes = 32 bytes*/
	values[0] = trig_ms * 32;
	values[1] = 1 << active_trigger;
	values[2] = 0;
	result = vts_start_ipc_transaction(component->dev, data,
		VTS_IRQ_AP_TARGET_SIZE, &values, 0, 1);
	if (result < 0) {
		vts_dev_err(component->dev, "Voice Trigger Value setting IPC Transaction Failed: %d\n",
			result);
		return result;
	}

	data->target_size = trig_ms;
	vts_dev_info(component->dev, "SET Voice Trigger Value: %dms\n",
		data->target_size);

	return 0;
}

static int get_vtsforce_reset(struct snd_kcontrol *kcontrol,
	struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_component *component =
		snd_soc_kcontrol_component(kcontrol);
	struct vts_data *data = p_vts_data;

	ucontrol->value.integer.value[0] = data->running;

	vts_dev_dbg(component->dev, "GET VTS Force Reset: %s\n",
			(data->running ? "VTS Running" :
			"VTS Not Running"));

	return 0;
}

static int set_vtsforce_reset(struct snd_kcontrol *kcontrol,
	struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_component *component =
		snd_soc_kcontrol_component(kcontrol);
	struct device *dev = component->dev;
	struct vts_data *data = p_vts_data;

	vts_dev_info(dev, "%s: VTS RESET\n", __func__);

	while (data->running && pm_runtime_active(dev)) {
		vts_dev_warn(dev, "%s: clear active models\n", __func__);
		pm_runtime_put_sync(dev);
	}

	return  0;
}

static const struct snd_kcontrol_new vts_controls[] = {
	SOC_SINGLE("PERIOD DATA2REQ", VTS_DMIC_ENABLE_DMIC_IF,
		VTS_DMIC_PERIOD_DATA2REQ_OFFSET, 3, 0),
	SOC_SINGLE("HPF EN", VTS_DMIC_CONTROL_DMIC_IF,
		VTS_DMIC_HPF_EN_OFFSET, 1, 0),
	SOC_ENUM("HPF SEL", vts_hpf_sel),
	SOC_ENUM("CPS SEL", vts_cps_sel),
	SOC_SINGLE_TLV("GAIN", VTS_DMIC_CONTROL_DMIC_IF,
		VTS_DMIC_GAIN_OFFSET, 4, 0, vts_gain_tlv_array),
	SOC_SINGLE("1DB GAIN", VTS_DMIC_CONTROL_DMIC_IF,
		VTS_DMIC_1DB_GAIN_OFFSET, 5, 0),
	SOC_ENUM_EXT("SYS SEL", vts_sys_sel, snd_soc_get_enum_double,
		vts_sys_sel_put_enum),
	SOC_ENUM("POLARITY CLK", vts_polarity_clk),
	SOC_ENUM("POLARITY OUTPUT", vts_polarity_output),
	SOC_ENUM("POLARITY INPUT", vts_polarity_input),
	SOC_ENUM("OVFW CTRL", vts_ovfw_ctrl),
	SOC_ENUM("CIC SEL", vts_cic_sel),
	SOC_ENUM_EXT("VoiceRecognization Mode", vtsvcrecog_mode_enum,
		get_vtsvoicerecognize_mode, set_vtsvoicerecognize_mode),
	SOC_ENUM_EXT("Active Keyphrase", vtsactive_phrase_enum,
		get_vtsactive_phrase, set_vtsactive_phrase),
	SOC_SINGLE_EXT("VoiceTrigger Value",
		SND_SOC_NOPM,
		0, 2000, 0,
		get_voicetrigger_value, set_voicetrigger_value),
	SOC_ENUM_EXT("Force Reset", vtsforce_reset_enum,
		get_vtsforce_reset, set_vtsforce_reset),
};

static int vts_dmic_sel_put(struct snd_kcontrol *kcontrol,
			struct snd_ctl_elem_value *ucontrol)
{
	struct vts_data *data = p_vts_data;
	unsigned int dmic_sel;

	dmic_sel = ucontrol->value.enumerated.item[0];
	if (dmic_sel > 1)
		return -EINVAL;

	pr_info("%s : VTS DMIC SEL: %d\n", __func__, dmic_sel);

	data->dmic_if = dmic_sel;

	return  0;
}

static const char * const dmic_sel_texts[] = {"DPDM", "APDM"};
static SOC_ENUM_SINGLE_EXT_DECL(dmic_sel_enum, dmic_sel_texts);

static const struct snd_kcontrol_new dmic_sel_controls[] = {
	SOC_DAPM_ENUM_EXT("MUX", dmic_sel_enum,
			snd_soc_dapm_get_enum_double, vts_dmic_sel_put),
};

static const struct snd_kcontrol_new dmic_if_controls[] = {
	SOC_DAPM_SINGLE("RCH EN", VTS_DMIC_CONTROL_DMIC_IF,
		VTS_DMIC_RCH_EN_OFFSET, 1, 0),
	SOC_DAPM_SINGLE("LCH EN", VTS_DMIC_CONTROL_DMIC_IF,
		VTS_DMIC_LCH_EN_OFFSET, 1, 0),
};

static const struct snd_soc_dapm_widget vts_dapm_widgets[] = {
	SND_SOC_DAPM_INPUT("PAD APDM"),
	SND_SOC_DAPM_INPUT("PAD DPDM"),
	SND_SOC_DAPM_MUX("DMIC SEL", SND_SOC_NOPM, 0, 0, dmic_sel_controls),
	SOC_MIXER_ARRAY("DMIC IF", SND_SOC_NOPM, 0, 0, dmic_if_controls),
};

static const struct snd_soc_dapm_route vts_dapm_routes[] = {
	// sink, control, source
	{"DMIC SEL", "APDM", "PAD APDM"},
	{"DMIC SEL", "DPDM", "PAD DPDM"},
	{"DMIC IF", "RCH EN", "DMIC SEL"},
	{"DMIC IF", "LCH EN", "DMIC SEL"},
	{"VTS Capture", NULL, "DMIC IF"},
};

int vts_set_dmicctrl(
	struct platform_device *pdev, int micconf_type, bool enable)
{
	struct device *dev = &pdev->dev;
	struct vts_data *data = platform_get_drvdata(pdev);
	int ctrl_dmicif = 0;
#if (defined(CONFIG_SOC_EXYNOS8895) || \
		defined(CONFIG_SOC_EXYNOS9810) || \
		defined(CONFIG_SOC_EXYNOS9820))
	int dmic_clkctrl = 0;
#endif
	int select_dmicclk = 0;

	vts_dev_dbg(dev, "%s-- flag: %d mictype: %d micusagecnt: %d\n",
		 __func__, enable, micconf_type, data->micclk_init_cnt);
	if (!data->vts_ready) {
		vts_dev_warn(dev, "%s: VTS Firmware Not running\n", __func__);
		return -EINVAL;
	}

	if (enable) {
		if (!data->micclk_init_cnt) {
			ctrl_dmicif = readl(data->dmic_if0_base +
				VTS_DMIC_CONTROL_DMIC_IF);

			if (IS_ENABLED(CONFIG_SOC_EXYNOS2100)) {
				clear_bits(ctrl_dmicif,
					7, VTS_DMIC_SYS_SEL_OFFSET);
				set_bits(ctrl_dmicif, data->syssel_rate,
					VTS_DMIC_SYS_SEL_OFFSET);
				vts_dev_info(dev, "%s DMIC IF SYS_SEL : %d\n",
					__func__, data->syssel_rate);
			}

			if (data->dmic_if == APDM) {
				vts_cfg_gpio(dev, "amic_default");
				set_bit(select_dmicclk,
					VTS_ENABLE_CLK_GEN_OFFSET);
				set_bit(select_dmicclk,
					VTS_SEL_EXT_DMIC_CLK_OFFSET);
				set_bit(select_dmicclk,
					VTS_ENABLE_CLK_CLK_GEN_OFFSET);

				/* Set AMIC VTS Gain */
#if IS_ENABLED(CONFIG_SOC_EXYNOS9820)
				clear_bit(ctrl_dmicif,
					VTS_DMIC_DMIC_SEL_OFFSET);
#else
				set_bits(ctrl_dmicif, data->dmic_if,
					VTS_DMIC_DMIC_SEL_OFFSET);
#endif
				set_bits(ctrl_dmicif, data->amicgain,
					VTS_DMIC_GAIN_OFFSET);
			} else {
				vts_cfg_gpio(dev, "dmic_default");
#ifdef VTS_2MIC_NS
				vts_cfg_gpio(dev, "dmic1_default");
#endif
				clear_bit(select_dmicclk,
					VTS_ENABLE_CLK_GEN_OFFSET);
				clear_bit(select_dmicclk,
					VTS_SEL_EXT_DMIC_CLK_OFFSET);
				clear_bit(select_dmicclk,
					VTS_ENABLE_CLK_CLK_GEN_OFFSET);

				/* Set DMIC VTS Gain */
#if IS_ENABLED(CONFIG_SOC_EXYNOS9820)
				set_bit(ctrl_dmicif,
					VTS_DMIC_DMIC_SEL_OFFSET);
				set_bits(ctrl_dmicif,
					5, VTS_DMIC_1DB_GAIN_OFFSET);
#else
				set_bits(ctrl_dmicif, data->dmic_if,
					VTS_DMIC_DMIC_SEL_OFFSET);
#endif
				set_bits(ctrl_dmicif,
					data->dmicgain, VTS_DMIC_GAIN_OFFSET);
			}
			writel(select_dmicclk,
				data->sfr_base + VTS_DMIC_CLK_CON);

			writel(ctrl_dmicif,
				data->dmic_if0_base + VTS_DMIC_CONTROL_DMIC_IF);
#ifdef VTS_2MIC_NS
			writel(ctrl_dmicif,
				data->dmic_if1_base + VTS_DMIC_CONTROL_DMIC_IF);
#endif

#if (defined(CONFIG_SOC_EXYNOS8895) || \
	defined(CONFIG_SOC_EXYNOS9810) || \
	defined(CONFIG_SOC_EXYNOS9820))
			dmic_clkctrl =
				readl(data->sfr_base + VTS_DMIC_CLK_CTRL);
			writel(dmic_clkctrl | (0x1 << VTS_CLK_ENABLE_OFFSET),
				data->sfr_base + VTS_DMIC_CLK_CTRL);
			vts_dev_info(dev, "%s Micclk setting ENABLED\n",
				__func__);
#endif
		}

		/* check whether Mic is already configure or not based on VTS
		 * option type for MIC configuration book keeping
		 */
		if ((!(data->mic_ready & (0x1 << VTS_MICCONF_FOR_TRIGGER)) &&
			(micconf_type == VTS_MICCONF_FOR_TRIGGER)) ||
			(!(data->mic_ready & (0x1 << VTS_MICCONF_FOR_GOOGLE)) &&
			 (micconf_type == VTS_MICCONF_FOR_GOOGLE))) {
			data->micclk_init_cnt++;
			data->mic_ready |= (0x1 << micconf_type);
			vts_dev_info(dev, "%s Micclk ENABLED for TRIGGER ++ %d\n",
				 __func__, data->mic_ready);
		} else if (!(data->mic_ready &
			(0x1 << VTS_MICCONF_FOR_RECORD)) &&
			micconf_type == VTS_MICCONF_FOR_RECORD) {
			data->micclk_init_cnt++;
			data->mic_ready |= (0x1 << micconf_type);
			vts_dev_info(dev, "%s Micclk ENABLED for RECORD ++ %d\n",
				 __func__, data->mic_ready);
		}
	} else {
		if (data->micclk_init_cnt)
			data->micclk_init_cnt--;
		if (!data->micclk_init_cnt) {
			if (data->clk_path != VTS_CLK_SRC_AUD0)
				vts_cfg_gpio(dev, "idle");

#if (defined(CONFIG_SOC_EXYNOS8895) || \
				defined(CONFIG_SOC_EXYNOS9810) || \
				defined(CONFIG_SOC_EXYNOS9820))
			dmic_clkctrl = readl(data->sfr_base +
				VTS_DMIC_CLK_CTRL);
			writel(dmic_clkctrl & ~(0x1 << VTS_CLK_ENABLE_OFFSET),
				data->sfr_base + VTS_DMIC_CLK_CTRL);
			writel(0x0, data->sfr_base + VTS_DMIC_CLK_CON);
#endif
			/* reset VTS Gain to default */
			writel(0x0,
				data->dmic_if0_base + VTS_DMIC_CONTROL_DMIC_IF);
#ifdef VTS_2MIC_NS
			writel(0x0,
				data->dmic_if1_base + VTS_DMIC_CONTROL_DMIC_IF);
#endif
			vts_dev_info(dev, "%s Micclk setting DISABLED\n",
				__func__);
		}

		/* MIC configuration book keeping */
		if (((data->mic_ready & (0x1 << VTS_MICCONF_FOR_TRIGGER)) &&
			(micconf_type == VTS_MICCONF_FOR_TRIGGER)) ||
			((data->mic_ready & (0x1 << VTS_MICCONF_FOR_GOOGLE)) &&
			 (micconf_type == VTS_MICCONF_FOR_GOOGLE))) {
			data->mic_ready &= ~(0x1 << micconf_type);
			vts_dev_info(dev, "%s Micclk DISABLED for TRIGGER -- %d\n",
				 __func__, data->mic_ready);
		} else if ((data->mic_ready &
			(0x1 << VTS_MICCONF_FOR_RECORD)) &&
			micconf_type == VTS_MICCONF_FOR_RECORD) {
			data->mic_ready &= ~(0x1 << micconf_type);
			vts_dev_info(dev, "%s Micclk DISABLED for RECORD -- %d\n",
				 __func__, data->mic_ready);
		}
	}

	return 0;
}
EXPORT_SYMBOL(vts_set_dmicctrl);

static irqreturn_t vts_error_handler(int irq, void *dev_id)
{
	struct platform_device *pdev = dev_id;
	struct device *dev = &pdev->dev;
	struct vts_data *data = platform_get_drvdata(pdev);
	u32 error_code;

	vts_mailbox_read_shared_register(data->pdev_mailbox,
						&error_code, 3, 1);
	if (error_code == VTS_ERR_HARD_FAULT) {
		u32 error_cfsr, error_hfsr, error_dfsr, error_afsr;

		vts_mailbox_read_shared_register(data->pdev_mailbox,
							&error_cfsr, 1, 1);
		vts_mailbox_read_shared_register(data->pdev_mailbox,
							&error_hfsr, 2, 1);
		vts_mailbox_read_shared_register(data->pdev_mailbox,
							&error_dfsr, 4, 1);
		vts_mailbox_read_shared_register(data->pdev_mailbox,
							&error_afsr, 5, 1);
		vts_dev_err(dev, "HardFault CFSR 0x%x\n", (int)error_cfsr);
		vts_dev_err(dev, "HardFault HFSR 0x%x\n", (int)error_hfsr);
		vts_dev_err(dev, "HardFault DFSR 0x%x\n", (int)error_dfsr);
		vts_dev_err(dev, "HardFault AFSR 0x%x\n", (int)error_afsr);
	}
	vts_ipc_ack(data, 1);
	vts_dev_err(dev, "Error occurred on VTS: 0x%x\n", (int)error_code);
	vts_dev_err(dev, "SOC down : %d, MIF down : %d",
		readl(data->sicd_base + SICD_SOC_DOWN_OFFSET),
		readl(data->sicd_base + SICD_MIF_DOWN_OFFSET));

	/* Dump VTS GPR register & SRAM */
	vts_dbg_dump_fw_gpr(dev, data, VTS_FW_ERROR);
	/* log dump */
	print_hex_dump(KERN_ERR, "vts-fw-log", DUMP_PREFIX_OFFSET, 32, 4,
			data->sramlog_baseaddr,
			0x800, true);
	vts_dev_err(dev, "shared_info0x%x, 0x%x 0x%x",
		data->shared_info->vendor_data[0],
		data->shared_info->vendor_data[1],
		data->shared_info->vendor_data[2]);

	switch (error_code) {
	case VTS_ERR_HARD_FAULT:
	case VTS_ERR_BUS_FAULT:
		dbg_snapshot_expire_watchdog();

		break;
	default:
		vts_reset_cpu();
		break;
	}

	return IRQ_HANDLED;
}

static irqreturn_t vts_boot_completed_handler(int irq, void *dev_id)
{
	struct platform_device *pdev = dev_id;
	struct device *dev = &pdev->dev;
	struct vts_data *data = platform_get_drvdata(pdev);

	data->vts_ready = 1;

	vts_ipc_ack(data, 1);
	wake_up(&data->ipc_wait_queue);

	vts_dev_info(dev, "VTS boot completed\n");

	return IRQ_HANDLED;
}

static irqreturn_t vts_ipc_received_handler(int irq, void *dev_id)
{
	struct platform_device *pdev = dev_id;
	struct device *dev = &pdev->dev;
	struct vts_data *data = platform_get_drvdata(pdev);
	u32 result;

	mailbox_read_shared_register(data->pdev_mailbox, &result, 3, 1);
	vts_dev_info(dev, "VTS received IPC: 0x%x\n", result);

	switch (data->ipc_state_ap) {
	case SEND_MSG:
		if (result  == (0x1 << data->running_ipc)) {
			vts_dev_dbg(dev, "IPC transaction completed\n");
			data->ipc_state_ap = SEND_MSG_OK;
		} else {
			vts_dev_err(dev, "IPC transaction error\n");
			data->ipc_state_ap = SEND_MSG_FAIL;
		}
		break;
	default:
		vts_dev_warn(dev, "State fault: %d Ack_value:0x%x\n",
				data->ipc_state_ap, result);
		break;
	}

	return IRQ_HANDLED;
}

static irqreturn_t vts_voice_triggered_handler(int irq, void *dev_id)
{
	struct platform_device *pdev = dev_id;
	struct device *dev = &pdev->dev;
	struct vts_data *data = platform_get_drvdata(pdev);
	u32 id, score, frame_count;
	u32 keyword_type = 1;
	char env[100] = {0,};
	char *envp[2] = {env, NULL};

	if (data->mic_ready & (0x1 << VTS_MICCONF_FOR_TRIGGER) ||
		data->mic_ready & (0x1 << VTS_MICCONF_FOR_GOOGLE)) {
		vts_mailbox_read_shared_register(data->pdev_mailbox,
						&id, 3, 1);
		vts_ipc_ack(data, 1);

		frame_count = (u32)(id & GENMASK(15, 0));
		score = (u32)((id & GENMASK(27, 16)) >> 16);
		id >>= 28;

		vts_dev_info(dev, "VTS triggered: id = %u, score = %u\n",
				id, score);
		vts_dev_info(dev, "VTS triggered: frame_count = %u\n",
				 frame_count);

		if (!(data->exec_mode & (0x1 << VTS_SOUND_DETECT_MODE))) {
			keyword_type = id;
			snprintf(env, sizeof(env),
					 "VOICE_WAKEUP_WORD_ID=%x",
					 keyword_type);
		} else if (data->exec_mode & (0x1 << VTS_SOUND_DETECT_MODE)) {
			snprintf(env, sizeof(env),
				 "VOICE_WAKEUP_WORD_ID=LPSD");
		} else {
			vts_dev_warn(dev, "Unknown VTS Execution Mode!!\n");
		}

		kobject_uevent_env(&dev->kobj, KOBJ_CHANGE, envp);
		__pm_wakeup_event(data->wake_lock, VTS_TRIGGERED_TIMEOUT_MS);
		data->vts_state = VTS_STATE_RECOG_TRIGGERED;
	}

	return IRQ_HANDLED;
}

static void vts_update_kernel_time(struct device *dev)
{
	struct vts_data *data = dev_get_drvdata(dev);
	unsigned long long kernel_time = sched_clock();

	data->shared_info->kernel_sec = (u32)(kernel_time / 1000000000);
	data->shared_info->kernel_msec = (u32)(kernel_time % 1000000000 / 1000000);
}

static irqreturn_t vts_trigger_period_elapsed_handler(int irq, void *dev_id)
{
	struct platform_device *pdev = dev_id;
	struct device *dev = &pdev->dev;
	struct vts_data *data = platform_get_drvdata(pdev);
	struct vts_platform_data *platform_data =
		platform_get_drvdata(data->pdev_vtsdma[0]);
	u32 pointer;

	if (data->mic_ready & (0x1 << VTS_MICCONF_FOR_TRIGGER) ||
		data->mic_ready & (0x1 << VTS_MICCONF_FOR_GOOGLE)) {
		vts_mailbox_read_shared_register(data->pdev_mailbox,
					 &pointer, 2, 1);
		vts_dev_dbg(dev, "%s:[%s] Base: %08x pointer:%08x\n",
			 __func__, (platform_data->id ? "VTS-RECORD" :
			 "VTS-TRIGGER"), data->dma_area_vts, pointer);

		if (pointer)
			platform_data->pointer = (pointer -
					 data->dma_area_vts);
		vts_ipc_ack(data, 1);


		snd_pcm_period_elapsed(platform_data->substream);
	}

	return IRQ_HANDLED;
}

static irqreturn_t vts_record_period_elapsed_handler(int irq, void *dev_id)
{
	struct platform_device *pdev = dev_id;
	struct device *dev = &pdev->dev;
	struct vts_data *data = platform_get_drvdata(pdev);
	struct vts_platform_data *platform_data =
		platform_get_drvdata(data->pdev_vtsdma[1]);
	u32 pointer;

	if (data->vts_state == VTS_STATE_RUNTIME_SUSPENDING ||
		data->vts_state == VTS_STATE_RUNTIME_SUSPENDED ||
		data->vts_state == VTS_STATE_NONE) {
		vts_dev_warn(dev, "%s: VTS wrong state\n", __func__);
		return IRQ_HANDLED;
	}

	if (data->mic_ready & (0x1 << VTS_MICCONF_FOR_RECORD)) {
		vts_mailbox_read_shared_register(data->pdev_mailbox,
						 &pointer, 1, 1);
		vts_dev_dbg(dev, "%s:[%s] Base: %08x pointer:%08x\n",
			 __func__,
			 (platform_data->id ? "VTS-RECORD" : "VTS-TRIGGER"),
			 (data->dma_area_vts + BUFFER_BYTES_MAX/2), pointer);

		if (pointer)
			platform_data->pointer = (pointer -
				(data->dma_area_vts + BUFFER_BYTES_MAX/2));
		vts_ipc_ack(data, 1);

		snd_pcm_period_elapsed(platform_data->substream);
	}

	return IRQ_HANDLED;
}

static irqreturn_t vts_debuglog_bufzero_handler(int irq, void *dev_id)
{
	struct platform_device *pdev = dev_id;
	struct device *dev = &pdev->dev;
	struct vts_data *data = platform_get_drvdata(pdev);

	vts_dev_dbg(dev, "%s LogBuffer Index: %d\n", __func__, 0);

	/* schedule log dump */
	vts_log_schedule_flush(dev, 0);

	vts_ipc_ack(data, 1);

	return IRQ_HANDLED;
}

static irqreturn_t vts_debuglog_bufone_handler(int irq, void *dev_id)
{
	struct platform_device *pdev = dev_id;
	struct device *dev = &pdev->dev;
	struct vts_data *data = platform_get_drvdata(pdev);

	vts_dev_dbg(dev, "%s LogBuffer Index: %d\n", __func__, 1);

	/* schedule log dump */
	vts_log_schedule_flush(dev, 1);

	vts_ipc_ack(data, 1);

	return IRQ_HANDLED;
}

static irqreturn_t vts_audiodump_handler(int irq, void *dev_id)
{
	struct platform_device *pdev = dev_id;
	struct device *dev = &pdev->dev;
	struct vts_data *data = platform_get_drvdata(pdev);
	u32 pointer;

	vts_dev_dbg(dev, "%s\n", __func__);

	if (data->vts_ready && data->audiodump_enabled) {
		u32 ackvalues[3] = {0, 0, 0};

		mailbox_read_shared_register(data->pdev_mailbox,
			ackvalues, 0, 2);
		vts_dev_info(dev, "%sDump offset: 0x%x size:0x%x\n",
			__func__, ackvalues[0], ackvalues[1]);
		/* register audio dump offset & size */
		vts_dump_addr_register(dev, ackvalues[0],
				ackvalues[1], VTS_AUDIO_DUMP);
		/* schedule pcm dump */
		vts_audiodump_schedule_flush(dev);
		/* vts_ipc_ack should be sent once dump is completed */
	} else if (data->vts_ready && data->slif_dump_enabled) {
		vts_mailbox_read_shared_register(data->pdev_mailbox,
						 &pointer, 1, 1);
		vts_dev_info(dev, "audiodump[%08x:%08x][p:%08x]\n",
			 data->dma_area_vts,
			 (data->dma_area_vts + BUFFER_BYTES_MAX/2),
			 pointer);
		vts_ipc_ack(data, 1);
		/* vts_s_lif_dump_period_elapsed(1, pointer); */
	} else {
		vts_ipc_ack(data, 1);
	}

	return IRQ_HANDLED;
}

static irqreturn_t vts_logdump_handler(int irq, void *dev_id)
{
	struct platform_device *pdev = dev_id;
	struct device *dev = &pdev->dev;
	struct vts_data *data = platform_get_drvdata(pdev);

	vts_dev_info(dev, "%s\n", __func__);

	if (data->vts_ready && data->logdump_enabled) {
		/* schedule pcm dump */
		vts_logdump_schedule_flush(dev);
		/* vts_ipc_ack should be sent once dump is completed */
	} else {
		vts_ipc_ack(data, 1);
	}

	return IRQ_HANDLED;
}

static irqreturn_t vts_slif_dump_handler(int irq, void *dev_id)
{
	struct platform_device *pdev = dev_id;
	struct device *dev = &pdev->dev;
	struct vts_data *data = platform_get_drvdata(pdev);

	vts_dev_dbg(dev, "%s\n", __func__);

	if (data->vts_ready && data->logdump_enabled)
		vts_ipc_ack(data, 1);
	else
		vts_ipc_ack(data, 1);

	return IRQ_HANDLED;
}

static irqreturn_t vts_cpwakeup_handler(int irq, void *dev_id)
{
	struct platform_device *pdev = dev_id;
	struct device *dev = &pdev->dev;

	vts_dev_info(dev, "%s\n", __func__);

	return IRQ_HANDLED;
}

void vts_register_dma(struct platform_device *pdev_vts,
		struct platform_device *pdev_vts_dma, unsigned int id)
{
	struct vts_data *data = platform_get_drvdata(pdev_vts);

	if (id < ARRAY_SIZE(data->pdev_vtsdma)) {
		data->pdev_vtsdma[id] = pdev_vts_dma;
		if (id > data->vtsdma_count)
			data->vtsdma_count = id + 1;
		vts_dev_info(&data->pdev->dev, "%s: VTS-DMA id(%u)Registered\n",
			__func__, id);
	} else {
		vts_dev_err(&data->pdev->dev, "%s: invalid id(%u)\n",
			__func__, id);
	}
}
EXPORT_SYMBOL(vts_register_dma);

static int vts_suspend(struct device *dev)
{
	struct vts_data *data = dev_get_drvdata(dev);
	u32 values[3] = {0, 0, 0};
	int result = 0;

	if (data->vts_ready) {
		if (data->running &&
			data->vts_state == VTS_STATE_RECOG_TRIGGERED) {
			result = vts_start_ipc_transaction(dev, data,
					VTS_IRQ_AP_RESTART_RECOGNITION,
					&values, 0, 1);
			if (result < 0) {
				vts_dev_err(dev, "%s restarted trigger failed\n",
					__func__);
				goto error_ipc;
			}
			data->vts_state = VTS_STATE_RECOG_STARTED;
		}

		/* enable vts wakeup source interrupts */
		enable_irq_wake(data->irq[VTS_IRQ_VTS_VOICE_TRIGGERED]);
		enable_irq_wake(data->irq[VTS_IRQ_VTS_ERROR]);
#ifdef TEST_WAKEUP
		enable_irq_wake(data->irq[VTS_IRQ_VTS_CP_WAKEUP]);
#endif
		if (data->audiodump_enabled)
			enable_irq_wake(data->irq[VTS_IRQ_VTS_AUDIO_DUMP]);
		if (data->logdump_enabled)
			enable_irq_wake(data->irq[VTS_IRQ_VTS_LOG_DUMP]);

		vts_dev_info(dev, "%s: Enable VTS Wakeup source irqs\n",
			__func__);
	}
	vts_dev_info(dev, "%s: TEST\n", __func__);
error_ipc:
	return result;
}

static int vts_resume(struct device *dev)
{
	struct vts_data *data = dev_get_drvdata(dev);

	if (data->vts_ready) {
		/* disable vts wakeup source interrupts */
		disable_irq_wake(data->irq[VTS_IRQ_VTS_VOICE_TRIGGERED]);
		disable_irq_wake(data->irq[VTS_IRQ_VTS_ERROR]);
#ifdef TEST_WAKEUP
		disable_irq_wake(data->irq[VTS_IRQ_VTS_CP_WAKEUP]);
#endif
		if (data->audiodump_enabled)
			disable_irq_wake(data->irq[VTS_IRQ_VTS_AUDIO_DUMP]);
		if (data->logdump_enabled)
			disable_irq_wake(data->irq[VTS_IRQ_VTS_LOG_DUMP]);

		vts_dev_info(dev, "%s: Disable VTS Wakeup source irqs\n",
			__func__);
	}
	return 0;
}

static void vts_irq_enable(struct platform_device *pdev, bool enable)
{
	struct vts_data *data = platform_get_drvdata(pdev);
	struct device *dev = &pdev->dev;
	int irqidx;

	vts_dev_info(dev, "%s IRQ Enable: [%s]\n", __func__,
			(enable ? "TRUE" : "FALSE"));

	for (irqidx = 0; irqidx < VTS_IRQ_COUNT; irqidx++) {
		if (enable)
			enable_irq(data->irq[irqidx]);
		else
			disable_irq(data->irq[irqidx]);
	}
#ifdef TEST_WAKEUP
	if (enable)
		enable_irq(data->irq[VTS_IRQ_VTS_CP_WAKEUP]);
	else
		disable_irq(data->irq[VTS_IRQ_VTS_CP_WAKEUP]);
#endif
}

static void vts_save_register(struct vts_data *data)
{
	regcache_cache_only(data->regmap_dmic, true);
	regcache_mark_dirty(data->regmap_dmic);
}

static void vts_restore_register(struct vts_data *data)
{
	regcache_cache_only(data->regmap_dmic, false);
	regcache_sync(data->regmap_dmic);
}

static void vts_memlog_sync_to_file(struct device *dev, struct vts_data *data)
{
	if (data->kernel_log_obj)
		vts_dev_info(dev, "%s kernel_log_obj sync : %d",
			__func__, memlog_sync_to_file(data->kernel_log_obj));

	if (data->fw_log_obj)
		vts_dev_info(dev, "%s fw_log_obj sync : %d",
			__func__, memlog_sync_to_file(data->fw_log_obj));
}

void vts_dbg_dump_fw_gpr(struct device *dev, struct vts_data *data,
			unsigned int dbg_type)
{
	int i;

	vts_dev_dbg(dev, "%s\n", __func__);
	switch (dbg_type) {
	case RUNTIME_SUSPEND_DUMP:
		if (!vts_is_on() || !data->running) {
			vts_dev_info(dev, "%s is skipped due to no power\n",
				__func__);
			return;
		}

		if (data->dump_obj)
			memlog_do_dump(data->dump_obj,
				MEMLOG_LEVEL_INFO);
		/* vts_memlog_sync_to_file(dev, data); */
		/* Save VTS firmware log msgs */
		if (!IS_ERR_OR_NULL(data->p_dump[dbg_type].sram_log) &&
			!IS_ERR_OR_NULL(data->sramlog_baseaddr)) {
			memcpy(data->p_dump[dbg_type].sram_log,
				VTS_DUMP_MAGIC, sizeof(VTS_DUMP_MAGIC));
			memcpy_fromio(data->p_dump[dbg_type].sram_log +
				sizeof(VTS_DUMP_MAGIC),
				data->sramlog_baseaddr,
				VTS_SRAMLOG_SIZE_MAX);
		}
		break;
	case KERNEL_PANIC_DUMP:
	case VTS_ITMON_ERROR:
		if (!data->running) {
			vts_dev_info(dev, "%s is skipped due to not running\n",
				__func__);
			return;
		}
	case VTS_FW_NOT_READY:
	case VTS_IPC_TRANS_FAIL:
	case VTS_FW_ERROR:
		for (i = 0; i <= 14; i++)
			vts_dev_info(dev, "R%d : %x\n", i,
				readl(data->gpr_base + VTS_CM4_R(i)));
		vts_dev_info(dev, "PC : %x\n",
			readl(data->gpr_base + VTS_CM4_PC));

		if (data->dump_obj &&
			(dbg_type == VTS_IPC_TRANS_FAIL ||
			dbg_type == VTS_FW_ERROR)) {
			memlog_do_dump(data->dump_obj,
				MEMLOG_LEVEL_ERR);
			vts_memlog_sync_to_file(dev, data);
		}

		/* Save VTS firmware log msgs */
		if (!IS_ERR_OR_NULL(data->p_dump[dbg_type].sram_log) &&
			!IS_ERR_OR_NULL(data->sramlog_baseaddr)) {
			memcpy(data->p_dump[dbg_type].sram_log,
				VTS_DUMP_MAGIC, sizeof(VTS_DUMP_MAGIC));
			memcpy_fromio(data->p_dump[dbg_type].sram_log +
				sizeof(VTS_DUMP_MAGIC),
				data->sramlog_baseaddr,
				VTS_SRAMLOG_SIZE_MAX);
		}

		/* Save VTS firmware all */
		if (!IS_ERR_OR_NULL(data->p_dump[dbg_type].sram_fw) &&
			!IS_ERR_OR_NULL(data->sram_base)) {
			memcpy_fromio(data->p_dump[dbg_type].sram_fw,
				data->sram_base, data->sram_size);
		}
		break;
	default:
		vts_dev_info(dev, "%s is skipped due to invalid debug type\n",
			__func__);
		return;
	}
	data->p_dump[dbg_type].time = sched_clock();
	data->p_dump[dbg_type].dbg_type = dbg_type;
	for (i = 0; i <= 14; i++)
		data->p_dump[dbg_type].gpr[i] =
			readl(data->gpr_base + VTS_CM4_R(i));
	data->p_dump[dbg_type].gpr[i++] = readl(data->gpr_base + VTS_CM4_PC);
}

static void exynos_vts_panic_handler(void)
{
	static bool has_run;
	struct vts_data *data = p_vts_data;
	struct device *dev =
		data ? (data->pdev ? &data->pdev->dev : NULL) : NULL;

	vts_dev_dbg(dev, "%s\n", __func__);

	if (vts_is_on() && dev) {
		if (has_run) {
			vts_dev_info(dev, "already dumped\n");
			return;
		}
		has_run = true;

		/* Dump VTS GPR register & SRAM */
		vts_dbg_dump_fw_gpr(dev, data, KERNEL_PANIC_DUMP);
	} else {
		vts_dev_info(dev, "%s: dump is skipped due to no power\n",
			__func__);
	}
}

static int vts_panic_handler(struct notifier_block *nb,
			       unsigned long action, void *data)
{
	exynos_vts_panic_handler();
	return NOTIFY_OK;
}

static struct notifier_block vts_panic_notifier = {
	.notifier_call	= vts_panic_handler,
	.next		= NULL,
	.priority	= 0	/* priority: INT_MAX >= x >= 0 */
};

static void vts_update_config(struct device *dev)
{
	struct vts_data *data = dev_get_drvdata(dev);

	data->target_sysclk = data->shared_info->config_fw.target_sys_clk;

	/* update forcely, if you want */
	if (data->target_sysclk != data->sysclk_rate) {
		int ret = 0;

		vts_dev_info(dev, "System Clock: %lu -> %lu\n",
			clk_get_rate(data->clk_sys), data->target_sysclk);
		ret = clk_set_rate(data->clk_sys, data->target_sysclk);
		if (ret < 0)
			vts_dev_err(dev, "failed set clk_sys: %d\n", ret);
	}

	vts_dev_dbg(dev, "System Clock target:%lu current:%lu\n",
			data->target_sysclk, clk_get_rate(data->clk_sys));
}

void vts_disable_sfr(struct device *dev)
{
	struct vts_data *data = dev_get_drvdata(dev);

	vts_dev_info(dev, "%s: enter\n", __func__);

	/* Controls by firmware should be disabled before pd_vts off. */
	writel(0x0, data->dmic_if0_base);
	writel(0x0, data->sfr_base + VTS_DMIC_IF_ENABLE_DMIC_IF);
	writel(0x0, data->dmic_ahb0_base);
	writel(0x0, data->timer0_base);
}

static void vts_firmware_disable_log(struct device *dev)
{
	struct vts_data *data = dev_get_drvdata(dev);
	u32 values[3] = {0, 0, 0};
	int result = 0;

	if (data->running) {
		vts_dev_info(dev, "RUNNING %s :%d\n", __func__, __LINE__);
		if (data->audiodump_enabled) {
			values[0] = VTS_DISABLE_AUDIODUMP;
			values[1] = 0;
			values[2] = 0;
			result = vts_start_ipc_transaction(dev, data,
						VTS_IRQ_AP_TEST_COMMAND,
						&values, 0, 1);
			if (result < 0)
				vts_dev_warn(dev, "Disable_AudioDump ipc failed\n");
			/* reset audio dump offset & size */
			vts_dump_addr_register(dev, 0, 0, VTS_AUDIO_DUMP);
		}

		if (data->logdump_enabled) {
			values[0] = VTS_DISABLE_LOGDUMP;
			values[1] = 0;
			values[2] = 0;
			result = vts_start_ipc_transaction(dev, data,
						VTS_IRQ_AP_TEST_COMMAND,
						&values, 0, 1);
			if (result < 0)
				vts_dev_warn(dev, "Disable_LogDump ipc failed\n");
			/* reset audio dump offset & size */
			vts_dump_addr_register(dev, 0, 0, VTS_LOG_DUMP);
		}

		if (data->fw_logfile_enabled || data->fw_logger_enabled) {
			values[0] = VTS_DISABLE_DEBUGLOG;
			values[1] = 0;
			values[2] = 0;
			result = vts_start_ipc_transaction(dev, data,
					VTS_IRQ_AP_TEST_COMMAND,
					&values, 0, 1);
			if (result < 0)
				vts_dev_warn(dev, "Disable_debuglog ipc transaction failed\n");
			/* reset VTS SRAM debug log buffer */
			vts_register_log_buffer(dev, 0, 0);
		}
	}
}

static int vts_firmware_shutdown(struct device *dev)
{
	struct platform_device *pdev = to_platform_device(dev);
	struct vts_data *data = dev_get_drvdata(dev);

#if (defined(CONFIG_SOC_EXYNOS9810) || defined(CONFIG_SOC_EXYNOS8895))
	int i = 1000;
	unsigned int status = 0;
#endif
	u32 values[3] = {0, 0, 0};
	bool is_timeout = false;
	unsigned long flag;
	int result = 0;

	if (data->running) {
		values[0] = 0;
		values[1] = 0;
		values[2] = 0;
		result = vts_start_ipc_transaction(dev,
			data, VTS_IRQ_AP_POWER_DOWN, &values, 0, 1);
		if (result < 0) {
			vts_dev_warn(dev, "POWER_DOWN IPC transaction Failed\n");

			result = vts_start_ipc_transaction(dev,
				data, VTS_IRQ_AP_POWER_DOWN, &values, 0, 1);
			if (result < 0) {
				vts_dev_warn(dev, "POWER_DOWN IPC transaction 2nd Failed\n");
				is_timeout = true;
			}
		}

		/* Dump VTS GPR register & Log messages */
		vts_dbg_dump_fw_gpr(dev, data, RUNTIME_SUSPEND_DUMP);
		vts_dev_info(dev, "shared_info 0x%x, %d %d",
			data->shared_info->vendor_data[0],
			data->shared_info->vendor_data[1],
			data->shared_info->vendor_data[2]);

#if (defined(CONFIG_SOC_EXYNOS9810) || defined(CONFIG_SOC_EXYNOS8895))
		/* wait for VTS STANDBYWFI in STATUS SFR */
		do {
			exynos_pmu_read(VTS_CPU_STANDBY, &status);
			vts_dev_dbg(dev, "%s Read VTS_CPU_STANDBY for STANDBYWFI status\n",
				__func__);
		} while (i-- && !(status & VTS_CPU_STANDBY_STANDBYWFI_MASK));

		if (!i)
			vts_dev_warn(dev, "VTS IP entering WFI time out\n");
#else
		vts_cpu_enable(false);
#endif

		if (data->irq_state) {
			vts_irq_enable(pdev, false);
			data->irq_state = false;
		}

		vts_disable_sfr(dev);
#if IS_ENABLED(CONFIG_SOC_EXYNOS2100)
		clk_disable(data->clk_dmic_sync);
#else
		clk_disable(data->clk_dmic);
#endif
		spin_lock_irqsave(&data->state_spinlock, flag);
		data->vts_state = VTS_STATE_RUNTIME_SUSPENDED;
		spin_unlock_irqrestore(&data->state_spinlock, flag);

#if (defined(CONFIG_SOC_EXYNOS9810) || defined(CONFIG_SOC_EXYNOS8895))
		vts_cpu_enable(false);
#endif
		vts_cpu_power(false);
		data->running = false;
	}

	if (is_timeout)
		return -ETIME;

	return 0;
}

#define VTS_FW_SHUDOWN_RETRY_CNT 5

static int vts_runtime_suspend(struct device *dev)
{
	struct vts_data *data = dev_get_drvdata(dev);
	int result = 0;
	unsigned long flag;
	int retry_cnt = 0;

	vts_dev_info(dev, "%s\n", __func__);
	if (data->sysevent_dev)
		sysevent_put((void *)data->sysevent_dev);

	while (1) {
		vts_firmware_disable_log(dev);

		spin_lock_irqsave(&data->state_spinlock, flag);
		data->vts_state = VTS_STATE_RUNTIME_SUSPENDING;
		spin_unlock_irqrestore(&data->state_spinlock, flag);

		vts_dev_info(dev, "%s save register :%d\n", __func__, __LINE__);
		vts_save_register(data);

		result = vts_firmware_shutdown(dev);

		if (retry_cnt >= VTS_FW_SHUDOWN_RETRY_CNT) {
			vts_dev_warn(dev, "%s: firmware shudown retry_cnt has reached the limit\n",
					__func__);

			break;
		}

		if (result) {
			retry_cnt++;
			vts_dev_err(dev, "%s: firmware shutdown failed, retry_cnt: %d\n",
					__func__, retry_cnt);
			usleep_range(19990, 20000);
			vts_start_runtime_resume(dev, 0);
			usleep_range(19990, 20000);
		} else {
			break;
		}
	}

	spin_lock_irqsave(&data->state_spinlock, flag);
	data->vts_state = VTS_STATE_RUNTIME_SUSPENDED;
	spin_unlock_irqrestore(&data->state_spinlock, flag);

	mailbox_update_vts_is_on(false);
	data->enabled = false;
	data->exec_mode = VTS_OFF_MODE;
	data->active_trigger = TRIGGER_SVOICE;
	data->voicerecog_start = 0;
	data->target_size = 0;
	/* reset micbias setting count */
	data->micclk_init_cnt = 0;
	data->mic_ready = 0;
	data->vts_ready = 0;
#if defined(CONFIG_SOC_EXYNOS9820)
	exynos_pd_tz_save(0x15410204);
#endif
	vts_dev_info(dev, "%s Exit\n", __func__);
	return 0;
}

int vts_start_runtime_resume(struct device *dev, int skip_log)
{
	struct platform_device *pdev = to_platform_device(dev);
	struct vts_data *data = dev_get_drvdata(dev);
	u32 values[3];
	int result;
	unsigned long long kernel_time;

	if (data->running) {
		if (!skip_log)
			vts_dev_info(dev, "SKIP %s\n", __func__);
		return 0;
	}
	vts_dev_info(dev, "%s\n", __func__);

	result = clk_set_rate(data->clk_sys, VTS_SYS_CLOCK);
	if (result < 0) {
		dev_err(dev, "clk_set_rate: %d\n", result);
		goto error_clk;
	}
	data->sysclk_rate = clk_get_rate(data->clk_sys);
	dev_info(dev, "System Clock : %ld\n", data->sysclk_rate);

#if defined(CONFIG_SOC_EXYNOS9820)
	exynos_pd_tz_restore(0x15410204);
#endif

	data->vts_state = VTS_STATE_RUNTIME_RESUMING;
	data->enabled = true;
	mailbox_update_vts_is_on(true);

	vts_restore_register(data);
	vts_cfg_gpio(dev, "dmic_default");
	vts_cfg_gpio(dev, "idle");
	vts_pad_retention(false);

	if (!data->irq_state) {
		vts_irq_enable(pdev, true);
		data->irq_state = true;
	}

#if IS_ENABLED(CONFIG_SOC_EXYNOS2100)
	result = clk_enable(data->clk_dmic_sync);
#else
	result = clk_enable(data->clk_dmic);
#endif
	if (result < 0) {
		vts_dev_err(dev, "Failed to enable the clock\n");
		goto error_clk;
	}
#if IS_ENABLED(CONFIG_SOC_EXYNOS2100)
	vts_dev_info(dev, "dmic clock rate:%lu\n",
		clk_get_rate(data->clk_dmic_sync));
#else
	vts_dev_info(dev, "dmic clock rate:%lu\n",
		clk_get_rate(data->clk_dmic));
#endif

#if (IS_ENABLED(CONFIG_SOC_EXYNOS9810) || IS_ENABLED(CONFIG_SOC_EXYNOS8895))
	vts_cpu_power(true);
#endif

#if (IS_ENABLED(CONFIG_SOC_EXYNOS9830) || IS_ENABLED(CONFIG_SOC_EXYNOS2100))
	if (!data->firmware) {
		vts_dev_info(dev, "%s : request_firmware_direct\n",
			__func__);
		result = request_firmware_direct(
			(const struct firmware **)&data->firmware,
			"vts.bin", dev);

		if (result < 0) {
			vts_dev_err(dev, "Failed to request_firmware_nowait\n");
			return 0;
		}
		vts_dev_info(dev, "vts_complete_firmware_request : OK\n");
		vts_complete_firmware_request(data->firmware, pdev);
	}
#endif

#if IS_ENABLED(CONFIG_SOC_EXYNOS2100_EVT0)
	/* R0SIZE_VAL */
	writel(0x0, data->intmem_code + 0x4);
	writel(0x0, data->intmem_data + 0x4);
	writel(0x0, data->intmem_pcm + 0x4);
#endif

	if (IS_ENABLED(CONFIG_EXYNOS_IMGLOADER)) {
		vts_dev_info(dev, "imgloader_boot : %d\n", data->imgloader);
		if (data->imgloader)
			result = imgloader_boot(&data->vts_imgloader_desc);
	} else
		result = vts_download_firmware(pdev);

	if (result < 0) {
		vts_dev_err(dev, "Failed to download firmware\n");
		data->enabled = false;
		mailbox_update_vts_is_on(false);
		goto error_firmware;
	}

	if (IS_ENABLED(CONFIG_EXYNOS_IMGLOADER))
		vts_dev_info(dev, "VTS IMGLOADER\n");
	else {
		if (IS_ENABLED(CONFIG_SOC_EXYNOS9810)
			|| IS_ENABLED(CONFIG_SOC_EXYNOS8895))
			vts_cpu_enable(true);
		else
			vts_cpu_power(true);
	}
	/* Enable CM4 GPR Dump */
	writel(0x1, data->gpr_base);
	vts_dev_info(dev, "GPR DUMP BASE Enable\n");

	result = vts_wait_for_fw_ready(dev);
	/* ADD FW READY STATUS */
	if (result < 0) {
		vts_dev_err(dev, "Failed to vts_wait_for_fw_ready\n");
		data->enabled = false;
		mailbox_update_vts_is_on(false);
		goto error_firmware;
	}

	vts_update_config(dev);

	if (data->google_version == 0) {
		values[0] = 2;
		values[1] = 0;
		values[2] = 0;
		result = vts_start_ipc_transaction(dev, data,
			VTS_IRQ_AP_GET_VERSION, &values, 0, 2);
		if (result < 0)
			dev_err(dev, "VTS_IRQ_AP_GET_VERSION ipc failed\n");
	} else {
		vts_dev_info(dev, "%s google version = %d\n",
			__func__, data->google_version);
	}
	vts_dev_info(dev, "Firmware version: 0x%x library version: 0x%x\n",
		data->vtsfw_version, data->vtsdetectlib_version);

	/* Configure select sys clock rate */
	vts_clk_set_rate(dev, data->syssel_rate);

	data->dma_area_vts = vts_set_baaw(data->baaw_base,
			data->dmab.addr, BUFFER_BYTES_MAX);

	values[0] = data->dma_area_vts;
	values[1] = 0x140;
	values[2] = 0x800;
	result = vts_start_ipc_transaction(dev,
		data, VTS_IRQ_AP_SET_DRAM_BUFFER, &values, 0, 1);
	if (result < 0) {
		vts_dev_err(dev, "DRAM_BUFFER Setting IPC transaction Failed\n");
		goto error_firmware;
	}

	values[0] = VTS_OFF_MODE;
	values[1] = 0;
	values[2] = 0;
	result = vts_start_ipc_transaction(dev,
		data, VTS_IRQ_AP_SET_MODE, &values, 0, 1);
	if (result < 0) {
		vts_dev_err(dev, "SET_MODE to OFF IPC transaction Failed\n");
		goto error_firmware;
	}
	data->exec_mode = VTS_OFF_MODE;

	values[0] = VTS_ENABLE_SRAM_LOG;
	values[1] = 0;
	values[2] = 0;
	result = vts_start_ipc_transaction(dev,
		data, VTS_IRQ_AP_TEST_COMMAND, &values, 0, 1);
	if (result < 0) {
		vts_dev_err(dev, "Enable_SRAM_log ipc transaction failed\n");
		goto error_firmware;
	}

	if (data->fw_logfile_enabled || data->fw_logger_enabled) {
		values[0] = VTS_ENABLE_DEBUGLOG;
		values[1] = 0;
		values[2] = 0;
		result = vts_start_ipc_transaction(dev,
			data, VTS_IRQ_AP_TEST_COMMAND, &values, 0, 1);
		if (result < 0) {
			vts_dev_err(dev, "Enable_debuglog ipc transaction failed\n");
			goto error_firmware;
		}
	}

	/* Enable Audio data Dump */
	if (data->audiodump_enabled) {
		values[0] = VTS_ENABLE_AUDIODUMP;
		values[1] = (VTS_ADUIODUMP_AFTER_MINS * 60);
		values[2] = 0;
		result = vts_start_ipc_transaction(dev, data,
				VTS_IRQ_AP_TEST_COMMAND, &values,
				0, 2);
		if (result < 0) {
			vts_dev_err(dev, "Enable_AudioDump ipc failed\n");
			goto error_firmware;
		}
	}

	/* Enable VTS FW log Dump */
	if (data->logdump_enabled) {
		values[0] = VTS_ENABLE_LOGDUMP;
		values[1] = (VTS_LOGDUMP_AFTER_MINS * 60);
		values[2] = 0;
		result = vts_start_ipc_transaction(dev, data,
				VTS_IRQ_AP_TEST_COMMAND, &values,
				0, 2);
		if (result < 0) {
			vts_dev_err(dev, "Enable_LogDump ipc failed\n");
			goto error_firmware;
		}
	}

	vts_update_kernel_time(dev);
	/* Send Kernel Time */
	kernel_time = sched_clock();
	values[0] = VTS_KERNEL_TIME;
	values[1] = (u32)(kernel_time / 1000000000);
	values[2] = (u32)(kernel_time % 1000000000 / 1000000);
	vts_dev_info(dev, "Time : %d.%d\n", values[1], values[2]);
	result = vts_start_ipc_transaction(dev, data,
		VTS_IRQ_AP_TEST_COMMAND, &values, 0, 2);
	if (result < 0)
		vts_dev_err(dev, "VTS_KERNEL_TIME ipc failed\n");
	vts_dev_dbg(dev, "%s DRAM-setting and VTS-Mode is completed\n",
		__func__);
	vts_dev_info(dev, "%s Exit\n", __func__);
	data->running = true;
	data->vts_state = VTS_STATE_RUNTIME_RESUMED;
	return 0;

error_firmware:
	vts_cpu_power(false);

#if IS_ENABLED(CONFIG_SOC_EXYNOS2100)
	clk_disable(data->clk_dmic_sync);
#else
	clk_disable(data->clk_dmic);
#endif

error_clk:
	if (data->irq_state) {
		vts_irq_enable(pdev, false);
		data->irq_state = false;
	}
	data->running = false;
	data->enabled = false;
	mailbox_update_vts_is_on(false);
	return 0;
}
EXPORT_SYMBOL(vts_start_runtime_resume);

static int vts_runtime_resume(struct device *dev)
{
	struct vts_data *data = dev_get_drvdata(dev);
	void *retval = NULL;

	cmu_vts_rco_400_control(1);
	vts_dev_info(dev, "%s RCO400 ON\n", __func__);
	if (data->sysevent_dev) {
		retval = sysevent_get(data->sysevent_desc.name);
		if (!retval)
			vts_dev_err(dev, "fail in sysevent_get\n");
	}
	data->enabled = true;
	dev_info(dev, "%s\n", __func__);
	return 0;
}

static const struct dev_pm_ops samsung_vts_pm = {
	SET_SYSTEM_SLEEP_PM_OPS(vts_suspend, vts_resume)
	SET_RUNTIME_PM_OPS(vts_runtime_suspend, vts_runtime_resume, NULL)
};

static const struct of_device_id exynos_vts_of_match[] = {
	{
		.compatible = "samsung,vts",
	},
	{},
};
MODULE_DEVICE_TABLE(of, exynos_vts_of_match);

static ssize_t vtsfw_version_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct vts_data *data = dev_get_drvdata(dev);
	unsigned int version = data->vtsfw_version;

	buf[0] = (((version >> 24) & 0xFF) + '0');
	buf[1] = '.';
	buf[2] = (((version >> 16) & 0xFF) + '0');
	buf[3] = '.';
	buf[4] = (((version >> 8) & 0xFF) + '0');
	buf[5] = '.';
	buf[6] = ((version & 0xFF) + '0');
	buf[7] = '\n';
	buf[8] = '\0';

	return 9;
}

static ssize_t vtsdetectlib_version_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct vts_data *data = dev_get_drvdata(dev);
	unsigned int version = data->vtsdetectlib_version;

	buf[0] = (((version >> 24) & 0xFF) + '0');
	buf[1] = '.';
	buf[2] = (((version >> 16) & 0xFF) + '0');
	buf[3] = '.';
	buf[4] = ((version & 0xFF) + '0');
	buf[5] = '\n';
	buf[6] = '\0';

	return 7;
}

static ssize_t vts_audiodump_show(struct device *dev,
			struct device_attribute *attr,
			char *buf)
{
	struct vts_data *data = dev_get_drvdata(dev);

	return sprintf(buf, "%d\n", data->audiodump_enabled);
}

static ssize_t vts_audiodump_store(struct device *dev,
			struct device_attribute *attr,
			const char *buf, size_t count)
{
	struct vts_data *data = dev_get_drvdata(dev);
	u32 val = 0;
	u32 values[3] = {0, 0, 0};
	int result;
	int err = kstrtouint(buf, 0, &val);

	if (err < 0)
		return err;
	data->audiodump_enabled = (val ? true : false);

	if (data->vts_ready) {
		if (data->audiodump_enabled) {
			values[0] = VTS_ENABLE_AUDIODUMP;
			values[1] = (VTS_ADUIODUMP_AFTER_MINS * 60);
		} else {
			values[0] = VTS_DISABLE_AUDIODUMP;
			values[1] = 0;
		}
		values[2] = 0;
		result = vts_start_ipc_transaction(dev, data,
				VTS_IRQ_AP_TEST_COMMAND, &values,
				0, 2);
		if (result < 0) {
			vts_dev_err(dev, "AudioDump[%d] ipc failed\n",
				data->audiodump_enabled);
		}
	}

	vts_dev_info(dev, "%s: Audio dump %sabled\n",
		__func__, (val ? "en" : "dis"));
	return count;
}

static ssize_t vts_logdump_show(struct device *dev,
			struct device_attribute *attr,
			char *buf)
{
	struct vts_data *data = dev_get_drvdata(dev);

	return sprintf(buf, "%d\n", data->logdump_enabled);
}

static ssize_t vts_logdump_store(struct device *dev,
			struct device_attribute *attr,
			const char *buf, size_t count)
{
	struct vts_data *data = dev_get_drvdata(dev);
	u32 val = 0;
	u32 values[3] = {0, 0, 0};
	int result;
	int err = kstrtouint(buf, 0, &val);

	if (err < 0)
		return err;

	data->logdump_enabled = (val ? true : false);

	if (data->vts_ready) {
		if (data->logdump_enabled) {
			values[0] = VTS_ENABLE_LOGDUMP;
			values[1] = (VTS_LOGDUMP_AFTER_MINS * 60);
		} else {
			values[0] = VTS_DISABLE_LOGDUMP;
			values[1] = 0;
		}
		values[2] = 0;
		result = vts_start_ipc_transaction(dev, data,
				VTS_IRQ_AP_TEST_COMMAND, &values,
				0, 2);
		if (result < 0) {
			vts_dev_err(dev, "LogDump[%d] ipc failed\n",
				data->logdump_enabled);
		}
	}

	vts_dev_info(dev, "%s: Log dump %sabled\n",
		__func__, (val ? "en" : "dis"));
	return count;
}

static DEVICE_ATTR_RO(vtsfw_version);
static DEVICE_ATTR_RO(vtsdetectlib_version);
static DEVICE_ATTR_RW(vts_audiodump);
static DEVICE_ATTR_RW(vts_logdump);


static void __iomem *samsung_vts_membase_request_and_map(
	struct platform_device *pdev, const char *name, const char *size)
{
	const __be32 *prop;
	unsigned int len = 0;
	unsigned int data_base = 0;
	unsigned int data_size = 0;
	void __iomem *result = NULL;
	struct device *dev = &pdev->dev;
	struct device_node *np = dev->of_node;

	prop = of_get_property(np, name, &len);
	if (!prop) {
		vts_dev_err(&pdev->dev, "Failed to of_get_property %s\n", name);
		return ERR_PTR(-EFAULT);
	}
	data_base = be32_to_cpup(prop);
	prop = of_get_property(np, size, &len);
	if (!prop) {
		vts_dev_err(&pdev->dev, "Failed to of_get_property %s\n", size);
		return ERR_PTR(-EFAULT);
	}
	data_size = be32_to_cpup(prop);
	if (data_base && data_size) {
		result = ioremap(data_base, data_size);
		if (IS_ERR_OR_NULL(result)) {
			vts_dev_err(&pdev->dev, "Failed to map %s\n", name);
			return ERR_PTR(-EFAULT);
		}
	}
	return result;
}

static void __iomem *samsung_vts_devm_request_and_map(
	struct platform_device *pdev, const char *name,
	phys_addr_t *phys_addr, size_t *size)
{
	struct resource *res;
	void __iomem *result;

	res = platform_get_resource_byname(pdev, IORESOURCE_MEM, name);
	if (IS_ERR_OR_NULL(res)) {
		vts_dev_err(&pdev->dev, "Failed to get %s\n", name);
		return ERR_PTR(-EINVAL);
	}
	if (phys_addr)
		*phys_addr = res->start;
	if (size)
		*size = resource_size(res);
	res = devm_request_mem_region(&pdev->dev,
		res->start, resource_size(res), name);
	if (IS_ERR_OR_NULL(res)) {
		vts_dev_err(&pdev->dev, "Failed to request %s\n", name);
		return ERR_PTR(-EFAULT);
	}
	result = devm_ioremap(&pdev->dev, res->start, resource_size(res));
	if (IS_ERR_OR_NULL(result)) {
		vts_dev_err(&pdev->dev, "Failed to map %s\n", name);
		return ERR_PTR(-EFAULT);
	}
	vts_dev_info(&pdev->dev, "%s: %s(%pK) is mapped on %pK with size of %zu",
			__func__, name,
			(void *)res->start, result, (size_t)resource_size(res));
	return result;
}

static int samsung_vts_devm_request_threaded_irq(
		struct platform_device *pdev, const char *irq_name,
		unsigned int hw_irq, irq_handler_t thread_fn)
{
	struct device *dev = &pdev->dev;
	struct vts_data *data = platform_get_drvdata(pdev);
	int result;

	data->irq[hw_irq] = platform_get_irq_byname(pdev, irq_name);
	if (data->irq[hw_irq] < 0) {
		vts_dev_err(dev, "Failed to get irq %s: %d\n",
			irq_name, data->irq[hw_irq]);
		return data->irq[hw_irq];
	}

	result = devm_request_threaded_irq(dev, data->irq[hw_irq],
			NULL, thread_fn,
			IRQF_TRIGGER_RISING | IRQF_ONESHOT, dev->init_name,
			pdev);

	if (result < 0)
		vts_dev_err(dev, "Unable to request irq %s: %d\n",
			irq_name, result);

	return result;
}

static struct clk *devm_clk_get_and_prepare(
	struct device *dev, const char *name)
{
	struct clk *clk;
	int result;

	clk = devm_clk_get(dev, name);
	if (IS_ERR(clk)) {
		vts_dev_err(dev, "Failed to get clock %s\n", name);
		goto error;
	}

	result = clk_prepare(clk);
	if (result < 0) {
		vts_dev_err(dev, "Failed to prepare clock %s\n", name);
		goto error;
	}

error:
	return clk;
}

static const struct reg_default vts_dmic_reg_defaults[] = {
	{0x0000, 0x00030000},
	{0x0004, 0x00000000},
};

static const struct regmap_config vts_component_regmap_config = {
	.reg_bits = 32,
	.val_bits = 32,
	.reg_stride = 4,
	.max_register = VTS_DMIC_CONTROL_DMIC_IF,
	.reg_defaults = vts_dmic_reg_defaults,
	.num_reg_defaults = ARRAY_SIZE(vts_dmic_reg_defaults),
	.cache_type = REGCACHE_RBTREE,
	.fast_io = true,
};

static int vts_fio_open(struct inode *inode, struct file *file)
{
	file->private_data = p_vts_data;
	return 0;
}

static long vts_fio_common_ioctl(struct file *file,
		unsigned int cmd, int __user *_arg)
{
	struct vts_data *data = (struct vts_data *)file->private_data;
	struct platform_device *pdev;
	struct device *dev;
	int arg;

	if (!data || (((cmd >> 8) & 0xff) != 'V'))
		return -ENOTTY;
	pdev = data->pdev;
	dev = &pdev->dev;
	if (get_user(arg, _arg))
		return -EFAULT;

	switch (cmd) {
	case VTSDRV_MISC_IOCTL_WRITE_SVOICE:
		if (arg < 0 || arg > VTS_MODEL_SVOICE_BIN_MAXSZ)
			return -EINVAL;
		memcpy(data->svoice_info.data, data->dmab_model.area, arg);
		data->svoice_info.loaded = true;
		data->svoice_info.actual_sz = arg;
		break;
	case VTSDRV_MISC_IOCTL_WRITE_GOOGLE:
		if (arg < 0 || arg > VTS_MODEL_GOOGLE_BIN_MAXSZ)
			return -EINVAL;
		memcpy(data->google_info.data, data->dmab_model.area, arg);
		data->google_info.loaded = true;
		data->google_info.actual_sz = arg;
		break;
	case VTSDRV_MISC_IOCTL_READ_GOOGLE_VERSION:
		if (data->google_version == 0) {
			vts_dev_info(dev, "get_sync : Read Google Version");
			pm_runtime_get_sync(dev);
			vts_start_runtime_resume(dev, 0);
			if (pm_runtime_active(dev))
				pm_runtime_put(dev);
		}
		vts_dev_info(dev, "Google Version : %d", data->google_version);
		put_user(data->google_version, (int __user *)_arg);
		break;
	default:
		pr_err("VTS unknown ioctl = 0x%x\n", cmd);
		return -EINVAL;
	}
	return 0;
}

static long vts_fio_ioctl(struct file *file,
		unsigned int cmd, unsigned long _arg)
{
	return vts_fio_common_ioctl(file, cmd, (int __user *)_arg);
}

#ifdef CONFIG_COMPAT
static long vts_fio_compat_ioctl(struct file *file,
		unsigned int cmd, unsigned long _arg)
{
	return vts_fio_common_ioctl(file, cmd, compat_ptr(_arg));
}
#endif /* CONFIG_COMPAT */

static int vts_fio_mmap(struct file *file, struct vm_area_struct *vma)
{
	struct vts_data *data = (struct vts_data *)file->private_data;

	return dma_mmap_wc(&data->pdev->dev, vma, data->dmab_model.area,
			   data->dmab_model.addr, data->dmab_model.bytes);
}

static const struct file_operations vts_fio_fops = {
	.owner			= THIS_MODULE,
	.open			= vts_fio_open,
	.release		= NULL,
	.unlocked_ioctl		= vts_fio_ioctl,
#ifdef CONFIG_COMPAT
	.compat_ioctl		= vts_fio_compat_ioctl,
#endif /* CONFIG_COMPAT */
	.mmap			= vts_fio_mmap,
};

static struct miscdevice vts_fio_miscdev = {
	.minor =    MISC_DYNAMIC_MINOR,
	.name =     "vts_fio_dev",
	.fops =     &vts_fio_fops
};

static int vts_memlog_file_completed(struct memlog_obj *obj, u32 flags)
{
	/* NOP */
	return 0;
}

static int vts_memlog_status_notify(struct memlog_obj *obj, u32 flags)
{
	/* NOP */
	return 0;
}

static int vts_memlog_level_notify(struct memlog_obj *obj, u32 flags)
{
	/* NOP */
	return 0;
}

static int vts_memlog_enable_notify(struct memlog_obj *obj, u32 flags)
{
	struct vts_data *data = p_vts_data;
	struct platform_device *pdev = data->pdev;
	struct device *dev = &pdev->dev;
	u32 values[3] = {0, 0, 0};
	int result = 0;

	if (data->kernel_log_obj)
		vts_dev_info(dev, "vts kernel_log_obj enable : %d",
			data->kernel_log_obj->enabled);

	if (data->fw_log_obj) {
		data->fw_logger_enabled = data->fw_log_obj->enabled;
		vts_dev_info(dev, "firmware logger enabled : %d",
			data->fw_logger_enabled);
		if (data->vts_ready) {
			if (data->fw_logger_enabled)
				values[0] = VTS_ENABLE_DEBUGLOG;
			else {
				values[0] = VTS_DISABLE_DEBUGLOG;
				vts_register_log_buffer(dev, 0, 0);
			}
			values[1] = 0;
			values[2] = 0;
			result = vts_start_ipc_transaction(dev, data,
				VTS_IRQ_AP_TEST_COMMAND, &values, 0, 1);
			if (result < 0)
				vts_dev_err(dev, "%s debuglog ipc transaction failed\n",
					__func__);
		}
	}
	return 0;
}

static const struct memlog_ops vts_memlog_ops = {
	.file_ops_completed = vts_memlog_file_completed,
	.log_status_notify = vts_memlog_status_notify,
	.log_level_notify = vts_memlog_level_notify,
	.log_enable_notify = vts_memlog_enable_notify,
};

static int vts_pm_notifier(struct notifier_block *nb,
		unsigned long action, void *nb_data)
{
	struct vts_data *data = container_of(nb, struct vts_data, pm_nb);
	struct platform_device *pdev = data->pdev;
	struct device *dev = &pdev->dev;

	vts_dev_info(dev, "%s(%lu)\n", __func__, action);

	switch (action) {
	case PM_SUSPEND_PREPARE:
		/* Nothing to do */
		break;
	default:
		/* Nothing to do */
		break;
	}
	return NOTIFY_DONE;
}

static int vts_itmon_notifier(struct notifier_block *nb,
		unsigned long action, void *nb_data)
{
	struct vts_data *data = container_of(nb, struct vts_data, itmon_nb);
	struct platform_device *pdev = data->pdev;
	struct device *dev = &pdev->dev;
	struct itmon_notifier *itmon_data = nb_data;

	if (itmon_data && itmon_data->dest && (strncmp("VTS", itmon_data->dest,
			sizeof("VTS") - 1) == 0)) {
		vts_dev_info(dev, "%s(%lu)\n", __func__, action);
		/* Dump VTS GPR register & SRAM */
		vts_dbg_dump_fw_gpr(dev, data, VTS_ITMON_ERROR);
		data->enabled = false;
		mailbox_update_vts_is_on(false);
		return NOTIFY_BAD;
	}
	return NOTIFY_DONE;
}

static int vts_sysevent_ramdump(
	int enable, const struct sysevent_desc *sysevent)
{
	struct vts_data *data = p_vts_data;
	struct platform_device *pdev = data->pdev;
	struct device *dev = &pdev->dev;
	/*
	 * This function is called in syseventtem_put / restart
	 * TODO: Ramdump related
	 */
	vts_dev_info(dev, "%s: call-back function\n", __func__);
	return 0;
}

static int vts_sysevent_powerup(const struct sysevent_desc *sysevent)
{
	struct vts_data *data = p_vts_data;
	struct platform_device *pdev = data->pdev;
	struct device *dev = &pdev->dev;
	/*
	 * This function is called in syseventtem_get / restart
	 * TODO: Power up related
	 */
	vts_dev_info(dev, "%s: call-back function\n", __func__);
	return 0;
}

static int vts_sysevent_shutdown(
	const struct sysevent_desc *sysevent, bool force_stop)
{
	struct vts_data *data = p_vts_data;
	struct platform_device *pdev = data->pdev;
	struct device *dev = &pdev->dev;
	/*
	 * This function is called in syseventtem_get / restart
	 * TODO: Shutdown related
	 */
	vts_dev_info(dev, "%s: call-back function\n", __func__);
	return 0;
}

static void vts_sysevent_crash_shutdown(const struct sysevent_desc *sysevent)
{
	struct vts_data *data = p_vts_data;
	struct platform_device *pdev = data->pdev;
	struct device *dev = &pdev->dev;
	/*
	 * This function is called in syseventtem_get / restart
	 * TODO: Crash Shutdown related
	 */
	vts_dev_info(dev, "%s: call-back function\n", __func__);
}

static int vts_component_probe(struct snd_soc_component *component)
{
	struct device *dev = component->dev;
	struct vts_data *data = dev_get_drvdata(dev);

	struct platform_device *pdev = data->pdev;
	int result;

	vts_dev_info(dev, "%s\n", __func__);
	data->cmpnt = component;

#if (IS_ENABLED(CONFIG_SOC_EXYNOS9830) || IS_ENABLED(CONFIG_SOC_EXYNOS2100))
	if (!data->firmware) {
		vts_dev_info(dev, "%s : request_firmware_direct\n", __func__);
		result = request_firmware_direct(
			(const struct firmware **)&data->firmware,
			"vts.bin", dev);

		if (result < 0) {
			vts_dev_err(dev, "Failed to request_firmware_nowait\n");
		} else {
			vts_dev_info(dev, "complete_firmware_request : OK\n");
			vts_complete_firmware_request(data->firmware, pdev);
		}
	}
#endif
	vts_dev_info(dev, "%s(%d)\n", __func__, __LINE__);

	return 0;
}

static const struct snd_soc_component_driver vts_component = {
	.probe = vts_component_probe,
	.controls = vts_controls,
	.num_controls = ARRAY_SIZE(vts_controls),
	.dapm_widgets = vts_dapm_widgets,
	.num_dapm_widgets = ARRAY_SIZE(vts_dapm_widgets),
	.dapm_routes = vts_dapm_routes,
	.num_dapm_routes = ARRAY_SIZE(vts_dapm_routes),
};

static struct platform_driver * const vts_sub_drivers[] = {
	&samsung_vts_s_lif_driver,
	&samsung_vts_dma_driver,
};

static int samsung_vts_probe(struct platform_device *pdev)
{
	struct device *dev = &pdev->dev;
	struct device_node *np = dev->of_node;
	struct vts_data *data;
	int i, result;
#if (defined(CONFIG_SOC_EXYNOS8895) || \
		defined(CONFIG_SOC_EXYNOS9810) || \
		defined(CONFIG_SOC_EXYNOS9820))
	int dmic_clkctrl = 0;
#endif

	vts_dev_info(dev, "%s\n", __func__);
	data = devm_kzalloc(dev, sizeof(struct vts_data), GFP_KERNEL);
	if (!data) {
		vts_dev_err(dev, "Failed to allocate memory\n");
		result = -ENOMEM;
		goto error;
	}

	dma_set_mask_and_coherent(dev, DMA_BIT_MASK(36));

	/* Model binary memory allocation */
	data->google_version = 0;
	data->google_info.max_sz = VTS_MODEL_GOOGLE_BIN_MAXSZ;
	data->google_info.actual_sz = 0;
	data->google_info.loaded = false;
	data->google_info.data = vmalloc(VTS_MODEL_GOOGLE_BIN_MAXSZ);
	if (!data->google_info.data) {
		vts_dev_err(dev, "%s Failed to allocate Grammar Bin memory\n",
			__func__);
		result = -ENOMEM;
		goto error;
	}

	data->svoice_info.max_sz = VTS_MODEL_SVOICE_BIN_MAXSZ;
	data->svoice_info.actual_sz = 0;
	data->svoice_info.loaded = false;
	data->svoice_info.data = vmalloc(VTS_MODEL_SVOICE_BIN_MAXSZ);
	if (!data->svoice_info.data) {
		vts_dev_err(dev, "%s Failed to allocate Net Bin memory\n",
			__func__);
		result = -ENOMEM;
		goto error;
	}

	/* initialize device structure members */
	data->active_trigger = TRIGGER_SVOICE;

	/* initialize micbias setting count */
	data->micclk_init_cnt = 0;
	data->mic_ready = 0;
	data->vts_state = VTS_STATE_NONE;
	data->vts_rec_state = VTS_REC_STATE_STOP;
	data->vts_tri_state = VTS_TRI_STATE_COPY_STOP;

	platform_set_drvdata(pdev, data);
	data->pdev = pdev;
	p_vts_data = data;

	init_waitqueue_head(&data->ipc_wait_queue);
	spin_lock_init(&data->ipc_spinlock);
	spin_lock_init(&data->state_spinlock);
	mutex_init(&data->ipc_mutex);
	mutex_init(&data->mutex_pin);
	data->wake_lock = wakeup_source_register(dev, "vts");
	data->pinctrl = devm_pinctrl_get(dev);
	if (IS_ERR(data->pinctrl)) {
		vts_dev_err(dev, "Couldn't get pins (%li)\n",
				PTR_ERR(data->pinctrl));
		return PTR_ERR(data->pinctrl);
	}

	data->sfr_base = samsung_vts_devm_request_and_map(pdev,
		"sfr", NULL, NULL);
	if (IS_ERR(data->sfr_base)) {
		result = PTR_ERR(data->sfr_base);
		goto error;
	}

	data->baaw_base = samsung_vts_devm_request_and_map(pdev,
		"baaw", NULL, NULL);
	if (IS_ERR(data->baaw_base)) {
		result = PTR_ERR(data->baaw_base);
		goto error;
	}

	data->sram_base = samsung_vts_devm_request_and_map(pdev,
		"sram", &data->sram_phys, &data->sram_size);
	if (IS_ERR(data->sram_base)) {
		result = PTR_ERR(data->sram_base);
		goto error;
	}

	data->dmic_if0_base = samsung_vts_devm_request_and_map(pdev,
		"dmic", NULL, NULL);
	if (IS_ERR(data->dmic_if0_base)) {
		result = PTR_ERR(data->dmic_if0_base);
		goto error;
	}

#if IS_ENABLED(CONFIG_SOC_EXYNOS9820) || \
	IS_ENABLED(CONFIG_SOC_EXYNOS9830) || \
	IS_ENABLED(CONFIG_SOC_EXYNOS2100)
	data->dmic_if1_base = samsung_vts_devm_request_and_map(pdev,
		"dmic1", NULL, NULL);
	if (IS_ERR(data->dmic_if1_base)) {
		result = PTR_ERR(data->dmic_if1_base);
		goto error;
	}
#endif
	data->gpr_base = samsung_vts_devm_request_and_map(pdev,
		"gpr", NULL, NULL);
	if (IS_ERR(data->gpr_base)) {
		result = PTR_ERR(data->gpr_base);
		goto error;
	}

	data->sicd_base = samsung_vts_membase_request_and_map(pdev,
		"sicd-base", "sicd-size");
	if (IS_ERR(data->sicd_base)) {
		result = PTR_ERR(data->sicd_base);
		goto error;
	}

#if IS_ENABLED(CONFIG_SOC_EXYNOS2100)
	data->intmem_code = samsung_vts_devm_request_and_map(pdev,
		"intmem_code", NULL, NULL);
	if (IS_ERR(data->intmem_code)) {
		result = PTR_ERR(data->intmem_code);
		goto error;
	}
	data->intmem_data = samsung_vts_devm_request_and_map(pdev,
		"intmem_data", NULL, NULL);
	if (IS_ERR(data->intmem_data)) {
		result = PTR_ERR(data->intmem_data);
		goto error;
	}
	data->intmem_pcm = samsung_vts_devm_request_and_map(pdev,
		"intmem_pcm", NULL, NULL);
	if (IS_ERR(data->intmem_pcm)) {
		result = PTR_ERR(data->intmem_pcm);
		goto error;
	}
#endif
	data->dmic_ahb0_base = samsung_vts_devm_request_and_map(pdev,
		"dmic_ahb0", NULL, NULL);
	if (IS_ERR(data->dmic_ahb0_base)) {
		result = PTR_ERR(data->dmic_ahb0_base);
		goto error;
	}

	data->timer0_base = samsung_vts_devm_request_and_map(pdev,
		"timer0", NULL, NULL);
	if (IS_ERR(data->timer0_base)) {
		result = PTR_ERR(data->timer0_base);
		goto error;
	}

	data->lpsdgain = 0;
	data->dmicgain = 0;
	data->amicgain = 0;

	/* read tunned VTS gain values */
	of_property_read_u32(np, "lpsd-gain", &data->lpsdgain);
	of_property_read_u32(np, "dmic-gain", &data->dmicgain);
	of_property_read_u32(np, "amic-gain", &data->amicgain);
	data->imgloader = of_property_read_bool(np,
		"samsung,imgloader-vts-support");

	vts_dev_info(dev, "VTS Tunned Gain value LPSD: %d DMIC: %d AMIC: %d\n",
			data->lpsdgain, data->dmicgain, data->amicgain);

#ifdef CONFIG_SOC_EXYNOS9830_EVT0
	vts_dev_info(dev, "VTS 9830 evt0 : use reserved memory\n");

	if (vts_rmem)
		data->dmab.area = vts_rmem_vmap(vts_rmem);

	data->dmab.addr = vts_rmem->base;

	data->dmab.bytes = BUFFER_BYTES_MAX/2;
	data->dmab.dev.dev = dev;
	data->dmab.dev.type = SNDRV_DMA_TYPE_DEV;

	data->dmab_rec.area = (data->dmab.area + BUFFER_BYTES_MAX/2);
	data->dmab_rec.addr = (data->dmab.addr + BUFFER_BYTES_MAX/2);
	data->dmab_rec.bytes = BUFFER_BYTES_MAX/2;
	data->dmab_rec.dev.dev = dev;
	data->dmab_rec.dev.type = SNDRV_DMA_TYPE_DEV;
#else
	vts_dev_info(dev, "VTS 9830 evt1 : use dmam_alloc_coherent\n");
	data->dmab.area = dmam_alloc_coherent(dev,
		BUFFER_BYTES_MAX, &data->dmab.addr, GFP_KERNEL);
	if (data->dmab.area == NULL) {
		result = -ENOMEM;
		goto error;
	}
	data->dmab.bytes = BUFFER_BYTES_MAX/2;
	data->dmab.dev.dev = dev;
	data->dmab.dev.type = SNDRV_DMA_TYPE_DEV;

	data->dmab_rec.area = (data->dmab.area + BUFFER_BYTES_MAX/2);
	data->dmab_rec.addr = (data->dmab.addr + BUFFER_BYTES_MAX/2);
	data->dmab_rec.bytes = BUFFER_BYTES_MAX/2;
	data->dmab_rec.dev.dev = dev;
	data->dmab_rec.dev.type = SNDRV_DMA_TYPE_DEV;
#endif

	data->dmab_log.area = dmam_alloc_coherent(dev, LOG_BUFFER_BYTES_MAX,
				&data->dmab_log.addr, GFP_KERNEL);
	if (data->dmab_log.area == NULL) {
		result = -ENOMEM;
		goto error;
	}
	data->dmab_log.bytes = LOG_BUFFER_BYTES_MAX;
	data->dmab_log.dev.dev = dev;
	data->dmab_log.dev.type = SNDRV_DMA_TYPE_DEV;

	/* VTSDRV_MISC_MODEL_BIN_MAXSZ =
	 * max(VTS_MODEL_GOOGLE_BIN_MAXSZ, VTS_MODEL_SVOICE_BIN_MAXSZ)
	 */
	data->dmab_model.area = dmam_alloc_coherent(dev,
		VTSDRV_MISC_MODEL_BIN_MAXSZ,
		&data->dmab_model.addr, GFP_KERNEL);
	if (data->dmab_model.area == NULL) {
		result = -ENOMEM;
		goto error;
	}
	data->dmab_model.bytes = VTSDRV_MISC_MODEL_BIN_MAXSZ;
	data->dmab_model.dev.dev = dev;
	data->dmab_model.dev.type = SNDRV_DMA_TYPE_DEV;

#if IS_ENABLED(CONFIG_SOC_EXYNOS8895) || \
	IS_ENABLED(CONFIG_SOC_EXYNOS9820) || \
	IS_ENABLED(CONFIG_SOC_EXYNOS9830) || \
	IS_ENABLED(CONFIG_SOC_EXYNOS2100)
	data->clk_rco = devm_clk_get_and_prepare(dev, "rco");
	if (IS_ERR(data->clk_rco)) {
		result = PTR_ERR(data->clk_rco);
		goto error;
	}

	result = clk_enable(data->clk_rco);
	if (result < 0) {
		vts_dev_err(dev, "Failed to enable the rco\n");
		goto error;
	}
#endif

#if !(IS_ENABLED(CONFIG_SOC_EXYNOS2100))
	data->clk_dmic = devm_clk_get_and_prepare(dev, "dmic");
	if (IS_ERR(data->clk_dmic)) {
		result = PTR_ERR(data->clk_dmic);
		goto error;
	}
#endif

	data->clk_dmic_if = devm_clk_get_and_prepare(dev, "dmic_if");
	if (IS_ERR(data->clk_dmic_if)) {
		result = PTR_ERR(data->clk_dmic_if);
		goto error;
	}

	data->clk_dmic_sync = devm_clk_get_and_prepare(dev, "dmic_sync");
	if (IS_ERR(data->clk_dmic_sync)) {
		result = PTR_ERR(data->clk_dmic_sync);
		goto error;
	}
	data->clk_sys = devm_clk_get_and_prepare(dev, "sysclk");
	if (IS_ERR(data->clk_sys)) {
		result = PTR_ERR(data->clk_sys);
		goto error;
	}

	data->mux_clk_dmic_if = devm_clk_get_and_prepare(dev, "mux_dmic_if");
	if (IS_ERR(data->mux_clk_dmic_if)) {
		result = PTR_ERR(data->mux_clk_dmic_if);
		goto error;
	}

	result = samsung_vts_devm_request_threaded_irq(pdev, "error",
			VTS_IRQ_VTS_ERROR, vts_error_handler);
	if (result < 0)
		goto error;

	result = samsung_vts_devm_request_threaded_irq(pdev, "boot_completed",
			VTS_IRQ_VTS_BOOT_COMPLETED, vts_boot_completed_handler);
	if (result < 0)
		goto error;

	result = samsung_vts_devm_request_threaded_irq(pdev, "ipc_received",
			VTS_IRQ_VTS_IPC_RECEIVED, vts_ipc_received_handler);
	if (result < 0)
		goto error;

	result = samsung_vts_devm_request_threaded_irq(pdev,
		"voice_triggered",
		VTS_IRQ_VTS_VOICE_TRIGGERED, vts_voice_triggered_handler);
	if (result < 0)
		goto error;

	result = samsung_vts_devm_request_threaded_irq(pdev,
		"trigger_period_elapsed",
		VTS_IRQ_VTS_PERIOD_ELAPSED, vts_trigger_period_elapsed_handler);
	if (result < 0)
		goto error;

	result = samsung_vts_devm_request_threaded_irq(pdev,
		"record_period_elapsed",
		VTS_IRQ_VTS_REC_PERIOD_ELAPSED,
		vts_record_period_elapsed_handler);
	if (result < 0)
		goto error;

	result = samsung_vts_devm_request_threaded_irq(pdev, "debuglog_bufzero",
		VTS_IRQ_VTS_DBGLOG_BUFZERO, vts_debuglog_bufzero_handler);
	if (result < 0)
		goto error;

	result = samsung_vts_devm_request_threaded_irq(pdev, "debuglog_bufone",
			VTS_IRQ_VTS_DBGLOG_BUFONE, vts_debuglog_bufone_handler);
	if (result < 0)
		goto error;

	result = samsung_vts_devm_request_threaded_irq(pdev, "audio_dump",
			VTS_IRQ_VTS_AUDIO_DUMP, vts_audiodump_handler);
	if (result < 0)
		goto error;

	result = samsung_vts_devm_request_threaded_irq(pdev, "log_dump",
			VTS_IRQ_VTS_LOG_DUMP, vts_logdump_handler);
	if (result < 0)
		goto error;

	result = samsung_vts_devm_request_threaded_irq(pdev, "slif_dump",
			VTS_IRQ_VTS_SLIF_DUMP, vts_slif_dump_handler);
	if (result < 0)
		goto error;

	result = samsung_vts_devm_request_threaded_irq(pdev, "cp_wakeup",
			VTS_IRQ_VTS_CP_WAKEUP, vts_cpwakeup_handler);
	if (result < 0)
		goto error;

	data->irq_state = true;

	data->pdev_mailbox = of_find_device_by_node(of_parse_phandle(np,
		"mailbox", 0));
	if (!data->pdev_mailbox) {
		vts_dev_err(dev, "Failed to get mailbox\n");
		result = -EPROBE_DEFER;
		goto error;
	}

#if (defined(CONFIG_SOC_EXYNOS8895) || \
		defined(CONFIG_SOC_EXYNOS9810) || \
		defined(CONFIG_SOC_EXYNOS9820))
	result = request_firmware_nowait(THIS_MODULE,
			FW_ACTION_HOTPLUG,
			"vts.bin",
			dev,
			GFP_KERNEL,
			pdev,
			vts_complete_firmware_request);
	if (result < 0) {
		vts_dev_err(dev, "Failed to request firmware\n");
		goto error;
	}
#endif
	if (data->imgloader) {
		result = vts_core_imgloader_desc_init(pdev);
		if (result < 0)
			return result;
	}

	data->regmap_dmic = devm_regmap_init_mmio_clk(dev,
			NULL,
			data->dmic_if0_base,
			&vts_component_regmap_config);

	result = snd_soc_register_component(dev,
		&vts_component, vts_dai, ARRAY_SIZE(vts_dai));
	if (result < 0) {
		vts_dev_err(dev, "Failed to register ASoC component\n");
		goto error;
	}

#ifdef EMULATOR
	pmu_alive = ioremap(0x16480000, 0x10000);
#endif

	data->pm_nb.notifier_call = vts_pm_notifier;
	register_pm_notifier(&data->pm_nb);

	data->itmon_nb.notifier_call = vts_itmon_notifier;
	itmon_notifier_chain_register(&data->itmon_nb);

	data->sysevent_dev = NULL;
	data->sysevent_desc.name = pdev->name;
	data->sysevent_desc.owner = THIS_MODULE;
	data->sysevent_desc.ramdump = vts_sysevent_ramdump;
	data->sysevent_desc.powerup = vts_sysevent_powerup;
	data->sysevent_desc.shutdown = vts_sysevent_shutdown;
	data->sysevent_desc.crash_shutdown = vts_sysevent_crash_shutdown;
	data->sysevent_desc.dev = &pdev->dev;
	data->sysevent_dev = sysevent_register(&data->sysevent_desc);
	if (IS_ERR(data->sysevent_dev)) {
		result = PTR_ERR(data->sysevent_dev);
		vts_dev_err(dev, "fail in device_event_probe : %d\n", result);
		goto error;
	}
	vts_proc_probe();
	pm_runtime_enable(dev);
	pm_runtime_get_sync(dev);

	vts_clk_set_rate(dev, 0);
	vts_cfg_gpio(dev, "idle");

	data->voicecall_enabled = false;
	data->voicerecog_start = 0;
	data->syssel_rate = 0;
	data->target_size = 0;
	data->vtsfw_version = 0x0;
	data->vtsdetectlib_version = 0x0;
	data->fw_logfile_enabled = 0;
	data->fw_logger_enabled = 0;
	data->audiodump_enabled = false;
	data->logdump_enabled = false;
	vts_set_clk_src(dev, VTS_CLK_SRC_RCO);

#if (defined(CONFIG_SOC_EXYNOS8895) || \
		defined(CONFIG_SOC_EXYNOS9810) || \
		defined(CONFIG_SOC_EXYNOS9820))
		dmic_clkctrl = readl(data->sfr_base + VTS_DMIC_CLK_CTRL);
		writel(dmic_clkctrl & ~(0x1 << VTS_CLK_ENABLE_OFFSET),
					data->sfr_base + VTS_DMIC_CLK_CTRL);
		vts_dev_dbg(dev, "DMIC_CLK_CTRL: Before 0x%x After 0x%x\n",
			dmic_clkctrl,
			readl(data->sfr_base + VTS_DMIC_CLK_CTRL));
#else
	vts_dev_info(dev, "Skip VTS_DMI_CLK_CTRL enable");
#endif

	result = device_create_file(dev, &dev_attr_vtsfw_version);
	if (result < 0)
		vts_dev_warn(dev, "Failed to create file: %s\n",
			"vtsfw_version");

	result = device_create_file(dev, &dev_attr_vtsdetectlib_version);
	if (result < 0)
		vts_dev_warn(dev, "Failed to create file: %s\n",
			"vtsdetectlib_version");

	result = device_create_file(dev, &dev_attr_vts_audiodump);
	if (result < 0)
		vts_dev_warn(dev, "Failed to create file: %s\n",
			"vts_audiodump");

	result = device_create_file(dev, &dev_attr_vts_logdump);
	if (result < 0)
		vts_dev_warn(dev, "Failed to create file: %s\n",
			"vts_logdump");

	data->sramlog_baseaddr = (char *)(data->sram_base +
		VTS_SRAMLOG_MSGS_OFFSET);
	data->shared_info = (struct vts_shared_info *)(data->sramlog_baseaddr +	VTS_SRAMLOG_SIZE_MAX);

	atomic_notifier_chain_register(&panic_notifier_list,
		&vts_panic_notifier);

	/* initialize log buffer offset as non */
	vts_register_log_buffer(dev, 0, 0);

	device_init_wakeup(dev, true);

	result = memlog_register("VTS_DRV", dev, &data->log_desc);
	if (result) {
		vts_dev_err(dev, "memlog_register fail");
		return result;
	}
	data->log_desc->ops = vts_memlog_ops;
	data->kernel_log_file_obj = memlog_alloc_file(data->log_desc,
		"ker-mem.memlog", SZ_256K, SZ_512K, 5000, 1);
	data->kernel_log_obj = memlog_alloc_printf(data->log_desc,
		SZ_256K, data->kernel_log_file_obj, "ker-mem", 0);
	data->dump_file_obj = memlog_alloc_file(data->log_desc,
		"fw-dmp.memlog", SZ_2K, SZ_4K, 5000, 1);
	data->dump_obj = memlog_alloc_dump(data->log_desc,
		SZ_2K, data->sram_phys + VTS_SRAMLOG_MSGS_OFFSET,
		false, data->dump_file_obj, "fw-dmp");
	data->fw_log_file_obj = memlog_alloc_file(data->log_desc,
		"fw-mem.memlog", SZ_128K, SZ_256K, 5000, 1);
	data->fw_log_obj = memlog_alloc(data->log_desc,
		SZ_128K, data->fw_log_file_obj, "fw-mem",
		MEMLOG_UFALG_NO_TIMESTAMP);
	/*
	 *if (data->fw_log_obj)
	 *	data->fw_logger_enabled = data->fw_log_obj->enabled;
	 */
	/* Allocate Memory for error logging */
	for (i = 0; i < VTS_DUMP_LAST; i++) {
		data->p_dump[i].sram_log =
			kzalloc(VTS_SRAMLOG_SIZE_MAX +
				sizeof(VTS_DUMP_MAGIC), GFP_KERNEL);
		if (!data->p_dump[i].sram_log) {
			vts_dev_info(dev, "%s is skipped due to lack of memory for sram log\n",
				__func__);
			continue;
		}
		if (i != RUNTIME_SUSPEND_DUMP) {
			/* Allocate VTS firmware all */
			data->p_dump[i].sram_fw =
				kzalloc(data->sram_size, GFP_KERNEL);
			if (!data->p_dump[i].sram_fw) {
				vts_dev_info(dev, "%s is skipped due to lack of memory for sram dump\n",
					__func__);
				continue;
			}
		}
	}

	result = misc_register(&vts_fio_miscdev);
	if (result)
		vts_dev_warn(dev, "Failed to create device for sound model download\n");
	else
		vts_dev_info(dev, "misc_register ok");

	vts_dev_info(dev, "register sub drivers\n");
	result = platform_register_drivers(vts_sub_drivers,
			ARRAY_SIZE(vts_sub_drivers));
	if (result)
		vts_dev_warn(dev, "Failed to register sub drivers\n");
	else {
		vts_dev_info(dev, "register sub drivers OK\n");
		of_platform_populate(np, NULL, NULL, dev);
	}

	if (pm_runtime_active(dev))
		pm_runtime_put(dev);
	vts_dev_info(dev, "Probed successfully\n");
error:
	return result;
}

static int samsung_vts_remove(struct platform_device *pdev)
{
	int i;
	struct device *dev = &pdev->dev;
	struct vts_data *data = platform_get_drvdata(pdev);

	pm_runtime_disable(dev);


#if IS_ENABLED(CONFIG_SOC_EXYNOS2100)
	clk_unprepare(data->clk_dmic_sync);
#else
	clk_unprepare(data->clk_dmic);
#endif

#ifndef CONFIG_PM
	vts_runtime_suspend(dev);
#endif
	release_firmware(data->firmware);
	if (data->google_info.data)
		vfree(data->google_info.data);
	if (data->svoice_info.data)
		vfree(data->svoice_info.data);

	for (i = 0; i < RUNTIME_SUSPEND_DUMP; i++) {
		/* Free memory for VTS firmware */
		kfree(data->p_dump[i].sram_fw);
	}

	snd_soc_unregister_component(dev);
#ifdef EMULATOR
	iounmap(pmu_alive);
#endif
	return 0;
}

static struct platform_driver samsung_vts_driver = {
	.probe  = samsung_vts_probe,
	.remove = samsung_vts_remove,
	.driver = {
		.name = "samsung-vts",
		.owner = THIS_MODULE,
		.of_match_table = of_match_ptr(exynos_vts_of_match),
		.pm = &samsung_vts_pm,
	},
};

module_platform_driver(samsung_vts_driver);

/* Module information */
MODULE_AUTHOR("Gyeongtaek Lee, <gt82.lee@samsung.com>");
MODULE_AUTHOR("Palli Satish Kumar Reddy, <palli.satish@samsung.com>");
MODULE_AUTHOR("Jaewon Kim, <jaewons.kim@samsung.com>");
MODULE_DESCRIPTION("Samsung Voice Trigger System");
MODULE_ALIAS("platform:samsung-vts");
MODULE_LICENSE("GPL");
