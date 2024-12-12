/*
 * Copyright (c) 2020 Samsung Electronics Co., Ltd.
 *		http://www.samsung.com
 *
 * Hardlockup Debug Driver for Samsung Exynos2100 SOC
 *
 * Author: Hosung Kim (hosung0.kim@samsung.com)
 * Author: Changki Kim (changki.kim@samsung.com)
 * Author: Donghyeok Choe (d7271.choe@samsung.com)
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#include <linux/module.h>
#include <linux/of.h>
#include <linux/percpu.h>
#include <linux/time.h>
#include <linux/delay.h>
#include <linux/spinlock.h>
#include <linux/spinlock_api_smp.h>
#include <linux/smp.h>
#include <linux/dma-mapping.h>
#include <linux/dmaengine.h>
#include <linux/platform_device.h>
#include <linux/device.h>
#include <linux/stddef.h>
#include <linux/sec_debug.h>

#include <asm/ftrace.h>
#include <asm/ptrace.h>
#include <asm/memory.h>
#include <soc/samsung/exynos-smc.h>
#include <soc/samsung/debug-snapshot.h>
#include "debug-snapshot-local.h"

#define HARDLOCKUP_DEBUG_MAGIC			(0xDEADBEEF)
#define HARDLOCKUP_DEBUG_SPIN_INSTS		(0x17FFFFFFD503207F)

struct hardlockup_debug_param {
	unsigned long last_pc_paddr;
	unsigned long spin_pc_paddr;
	unsigned long gpr_paddr;
};

struct hardlockup_debug_dma_memory {
	struct hardlockup_debug_param param;
	unsigned long param_paddr;
	unsigned long spin_func;
	unsigned long datas[64];
};

struct hardlockup_debugger_data {
	int watchdog_intid;
	int lockup_detected;
	unsigned long hardlockup_core_mask;
	void *vaddr;
	dma_addr_t paddr;
	size_t size;
	struct hardlockup_debug_dma_memory *dma_memory;
	raw_spinlock_t log_lock;
};

static struct hardlockup_debugger_data hd_data;

static inline int hardlockup_debug_try_lock_timeout(raw_spinlock_t *lock,
								long timeout)
{
	int ret;

	do {
		ret = _raw_spin_trylock(lock);
		if (!ret)
			udelay(1);
	} while(!ret && (timeout--) > 0);

	return ret;
}

static void hardlockup_debugger_handler(void)
{
	int i, ret;
	int cpu = raw_smp_processor_id();

	ret = hardlockup_debug_try_lock_timeout(&hd_data.log_lock,
						500 * USEC_PER_MSEC);

	pr_emerg("%s: <Core%d Callstack>\n", __func__, cpu);
	pr_emerg("%s: %pS\n", __func__, hd_data.dma_memory->datas[cpu]);

	for (i = 0; i < 5; i++) {
		pr_emerg("%s: %pS\n", __func__, return_address(i));
	}

	hd_data.lockup_detected = 1;
	hd_data.hardlockup_core_mask &= ~(1UL << cpu);

	/*
	 * TODO: There is no regs more in this context.
	 * secdbg_exin_set_backtrace_cpu should be rewrited.
	 */
	//secdbg_exin_set_backtrace_cpu(v_regs, cpu);

	if (ret)
		raw_spin_unlock(&hd_data.log_lock);
	else
		pr_emerg("%s: fail to get log lock\n", __func__);

	panic("hardlockup detected");
}

static int hardlockup_debug_panic_handler(struct notifier_block *nb,
					  unsigned long l, void *buf)
{
	int cpu;
	unsigned long locked_up_mask = 0;
#ifdef SMC_CMD_KERNEL_PANIC_NOTICE
	unsigned long timeout;
	int ret;
#endif

	if (hd_data.lockup_detected)
		goto out;
	/*
	 * Assume that at this stage, CPUs that are still online
	 * (other than the panic-ing CPU) are locked up.
	 */
	for_each_possible_cpu (cpu) {
		if (cpu != raw_smp_processor_id() && cpu_online(cpu))
			locked_up_mask |= (1 << cpu);
	}

	pr_emerg("Hardlockup CPU mask: 0x%lx\n", locked_up_mask);

#ifdef SMC_CMD_KERNEL_PANIC_NOTICE
	if (locked_up_mask) {
		hd_data.hardlockup_core_mask = locked_up_mask;

		/* Setup for generating NMI interrupt to unstopped CPUs */
		ret = exynos_smc(SMC_CMD_KERNEL_PANIC_NOTICE,
				locked_up_mask,
				(unsigned long)hardlockup_debugger_handler,
				hd_data.dma_memory->param.last_pc_paddr);
		if (ret) {
			pr_emerg("Failed to generate NMI for hardlockup, "
				"not support to dump information of core\n");
			locked_up_mask = 0;
		}
		hd_data.lockup_detected = 1;

		/*  Wait for 3 seconds for NMI interrupt handling */
		timeout = USEC_PER_SEC * 3;
		while (hd_data.hardlockup_core_mask != 0 && timeout--)
			udelay(1);
	}
#endif
out:
	if (hd_data.lockup_detected)
		dbg_snapshot_expire_watchdog();

	return NOTIFY_OK;
}

static struct notifier_block hardlockup_debug_panic_nb = {
	.notifier_call = hardlockup_debug_panic_handler,
};

void static hardlockup_debugger_set_memory(
				struct hardlockup_debugger_data *data){
	unsigned long last_pc_paddr;
	unsigned long spin_pc_paddr;
	unsigned long gpr_paddr;
	unsigned long param_paddr;
	unsigned long spin_func;

	last_pc_paddr = (unsigned long)data->paddr +
			(unsigned long)offsetof(
				struct hardlockup_debug_dma_memory, datas);
	spin_pc_paddr = (unsigned long)data->paddr +
			(unsigned long)offsetof(
				struct hardlockup_debug_dma_memory, spin_func);
	gpr_paddr = dbg_snapshot_get_header_paddr();
	param_paddr = (unsigned long)data->paddr +
			(unsigned long)offsetof(
				struct hardlockup_debug_dma_memory, param);
	spin_func = HARDLOCKUP_DEBUG_SPIN_INSTS;

	data->dma_memory->param.last_pc_paddr = last_pc_paddr;
	data->dma_memory->param.spin_pc_paddr = spin_pc_paddr;
	data->dma_memory->param.gpr_paddr = gpr_paddr;
	data->dma_memory->param_paddr = param_paddr;
	data->dma_memory->spin_func = spin_func;
}

static int hardlockup_debugger_probe(struct platform_device *pdev)
{
	struct device_node *node = pdev->dev.of_node;
	int ret = -1;

	if (!node) {
		pr_info("Failed to find debug device tree node\n");
	} else {
		ret = of_property_read_u32(node, "use_multistage_wdt_irq",
						&hd_data.watchdog_intid);
		if (ret)
			pr_info("Multistage watchdog is not supported\n");
	}

	dma_set_mask_and_coherent(&pdev->dev, DMA_BIT_MASK(36));
	hd_data.vaddr = dma_alloc_coherent(&pdev->dev, PAGE_SIZE,
					&hd_data.paddr, GFP_KERNEL);
	if (hd_data.vaddr == NULL) {
		ret = -ENOMEM;
		goto out;
	}
	hd_data.size = PAGE_SIZE;
	hd_data.dma_memory = (struct hardlockup_debug_dma_memory *)
							hd_data.vaddr;
	hardlockup_debugger_set_memory(&hd_data);

	if (!ret) {
#ifdef SMC_CMD_LOCKUP_NOTICE
		ret = exynos_smc(SMC_CMD_LOCKUP_NOTICE,
			(unsigned long)hardlockup_debugger_handler,
			hd_data.watchdog_intid,
			hd_data.dma_memory->param_paddr);
#else
		ret = -EINVAL;
#endif
	}
	pr_info("%s to register all-core lockup detector - ret: %d\n",
				(ret == 0) ? "success" : "failed", ret);


	raw_spin_lock_init(&hd_data.log_lock);

	atomic_notifier_chain_register(&panic_notifier_list,
				       &hardlockup_debug_panic_nb);

	pr_info("Initialized hardlockup debug dump successfully.\n");
out:
	return ret;
}


static const struct of_device_id hardlockup_debugger_device_ids[] = {
	{
		.compatible = "samsung,hardlockup-debug",
	},
};

static struct platform_driver hardlockup_debugger_driver = {
	.probe = hardlockup_debugger_probe,
	.driver = {
		.name = "hardlockup-debugger",
		.owner = THIS_MODULE,
		.of_match_table = hardlockup_debugger_device_ids,
	},
};

static int __init hardlockup_debugger_init(void)
{
	return platform_driver_register(&hardlockup_debugger_driver);
}
module_init(hardlockup_debugger_init);

MODULE_LICENSE("GPL v2");
MODULE_DESCRIPTION("Hardlockup Debug DRIVER");
