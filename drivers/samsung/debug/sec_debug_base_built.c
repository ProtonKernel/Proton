// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright (c) 2020 Samsung Electronics Co., Ltd.
 *      http://www.samsung.com
 *
 * Samsung TN debugging code
 *
 */

#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/platform_device.h>
#include <linux/of.h>
#include <linux/of_fdt.h>
#include <linux/of_reserved_mem.h>
#include <linux/irq.h>
#include <linux/interrupt.h>
#include <linux/irqdesc.h>
#include <linux/mm.h>
#include <linux/uaccess.h>
#include <linux/kdebug.h>

#include <linux/sec_debug.h>
#include "sec_debug_internal.h"

static struct sec_debug_next *secdbg_sdn_va;
static unsigned int sec_debug_next_size;
static struct sec_debug_base_param *secdbg_bb_param;

/* from PA to NonCachable VA */
static unsigned long sdn_ncva_to_pa_offset;

void *secdbg_base_built_get_ncva(unsigned long pa)
{
	return (void *)(pa + sdn_ncva_to_pa_offset);
}
EXPORT_SYMBOL(secdbg_base_built_get_ncva);

#if IS_ENABLED(CONFIG_SEC_DEBUG_AVOID_UNNECESSARY_TRAP)
extern unsigned long long sea_incorrect_addr;
#endif

/*
 * dump a block of kernel memory from around the given address
 */
static void show_data(unsigned long addr, int nbytes, const char *name)
{
	int	i, j;
	int	nlines;
#if IS_ENABLED(CONFIG_SEC_DEBUG_AVOID_UNNECESSARY_TRAP)
	int	nbytes_offset = nbytes;
#endif
	u32	*p;

	/*
	 * don't attempt to dump non-kernel addresses or
	 * values that are probably just small negative numbers
	 */
	if (addr < PAGE_OFFSET || addr >= -256UL) {
		/*
		 * If kaslr is enabled, Kernel code is able to
		 * located in VMALLOC address.
		 */
		if (IS_ENABLED(CONFIG_RANDOMIZE_BASE)) {
#ifdef CONFIG_VMAP_STACK
			if (!is_vmalloc_addr((const void *)addr))
				return;
#else
			if (addr < (unsigned long)KERNEL_START ||
			    addr > (unsigned long)KERNEL_END)
				return;
#endif
		} else {
			return;
		}
	}

	pr_info("\n%s: %#lx:\n", name, addr);

	/*
	 * round address down to a 32 bit boundary
	 * and always dump a multiple of 32 bytes
	 */
	p = (u32 *)(addr & ~(sizeof(u32) - 1));
	nbytes += (addr & (sizeof(u32) - 1));
	nlines = (nbytes + 31) / 32;


	for (i = 0; i < nlines; i++) {
		/*
		 * just display low 16 bits of address to keep
		 * each line of the dump < 80 characters
		 */
		pr_info("%04lx :", (unsigned long)p & 0xffff);
		for (j = 0; j < 8; j++) {
			u32	data;
#if IS_ENABLED(CONFIG_SEC_DEBUG_AVOID_UNNECESSARY_TRAP)
			if ((sea_incorrect_addr != 0) &&
			    ((unsigned long long)p >= (sea_incorrect_addr - nbytes_offset)) &&
			    ((unsigned long long)p <= (sea_incorrect_addr + nbytes_offset))) {
				if (j == 7)
					pr_cont(" ********\n");
				else
					pr_cont(" ********");
			} else if (probe_kernel_address(p, data)) {
#else
			if (probe_kernel_address(p, data)) {
#endif
				if (j == 7)
					pr_cont(" ********\n");
				else
					pr_cont(" ********");

			} else {
				if (j == 7)
					pr_cont(" %08X\n", data);
				else
					pr_cont(" %08X", data);
			}
			++p;
		}
	}
}

static void show_extra_register_data(struct pt_regs *regs, int nbytes)
{
	mm_segment_t fs;
	unsigned int i;

	fs = get_fs();
	set_fs(KERNEL_DS);
	show_data(regs->pc - nbytes, nbytes * 2, "PC");
	show_data(regs->regs[30] - nbytes, nbytes * 2, "LR");
	show_data(regs->sp - nbytes, nbytes * 2, "SP");
	for (i = 0; i < 30; i++) {
		char name[4];

		snprintf(name, sizeof(name), "X%u", i);
		show_data(regs->regs[i] - nbytes, nbytes * 2, name);
	}
	set_fs(fs);

	pr_info("\n");
}

static int secdbg_base_built_die_handler(struct notifier_block *nb,
				   unsigned long l, void *buf)
{
	struct die_args *args = (struct die_args *)buf;
	struct pt_regs *regs = args->regs;

	if (!regs)
		return NOTIFY_DONE;

	if (!user_mode(regs))
		show_extra_register_data(regs, 256);

	return NOTIFY_DONE;
}

static struct notifier_block nb_die_block = {
	.notifier_call = secdbg_base_built_die_handler,
	.priority = -128,
};

void secdbg_base_built_set_task_in_soft_lockup(uint64_t task)
{
	if (secdbg_sdn_va)
		secdbg_sdn_va->kernd.task_in_soft_lockup = task;
}

void secdbg_base_built_set_cpu_in_soft_lockup(uint64_t cpu)
{
	if (secdbg_sdn_va)
		secdbg_sdn_va->kernd.cpu_in_soft_lockup = cpu;
}

#ifdef CONFIG_SEC_DEBUG_TASK_IN_STATE_INFO
void secdbg_base_built_set_task_in_pm_suspend(struct task_struct *task)
{
	if (secdbg_sdn_va)
		secdbg_sdn_va->kernd.task_in_pm_suspend = (uint64_t)task;
}

void secdbg_base_built_set_task_in_sys_reboot(struct task_struct *task)
{
	if (secdbg_sdn_va)
		secdbg_sdn_va->kernd.task_in_sys_reboot = (uint64_t)task;
}

void secdbg_base_built_set_task_in_sys_shutdown(struct task_struct *task)
{
	if (secdbg_sdn_va)
		secdbg_sdn_va->kernd.task_in_sys_shutdown = (uint64_t)task;
}

void secdbg_base_built_set_task_in_dev_shutdown(struct task_struct *task)
{
	if (secdbg_sdn_va)
		secdbg_sdn_va->kernd.task_in_dev_shutdown = (uint64_t)task;
}

void secdbg_base_built_set_task_in_sync_irq(struct task_struct *task, unsigned int irq, struct irq_desc *desc)
{
	const char *name = (desc && desc->action) ? desc->action->name : NULL;

	if (!secdbg_sdn_va)
		return;

	secdbg_sdn_va->kernd.sync_irq_task = (uint64_t)task;
	secdbg_sdn_va->kernd.sync_irq_num = irq;
	secdbg_sdn_va->kernd.sync_irq_name = (uint64_t)name;
	secdbg_sdn_va->kernd.sync_irq_desc = (uint64_t)desc;

	if (desc) {
		secdbg_sdn_va->kernd.sync_irq_threads_active = desc->threads_active.counter;

		if (desc->action && (desc->action->irq == irq) && desc->action->thread)
			secdbg_sdn_va->kernd.sync_irq_thread = (uint64_t)(desc->action->thread);
		else
			secdbg_sdn_va->kernd.sync_irq_thread = 0;
	}
}
#endif /* SEC_DEBUG_TASK_IN_STATE_INFO */

#ifdef CONFIG_SEC_DEBUG_UNFROZEN_TASK
void secdbg_base_built_set_unfrozen_task(uint64_t task)
{
	if (secdbg_sdn_va)
		secdbg_sdn_va->kernd.unfrozen_task = task;
}

void secdbg_base_built_set_unfrozen_task_count(uint64_t count)
{
	if (secdbg_sdn_va)
		secdbg_sdn_va->kernd.unfrozen_task_count = count;
}
#endif /* SEC_DEBUG_UNFROZEN_TASK */

#if IS_ENABLED(CONFIG_SEC_DEBUG_HANDLE_BAD_STACK)
static DEFINE_PER_CPU(unsigned long, secdbg_bad_stack_chk);

void secdbg_base_built_check_handle_bad_stack(void)
{
	if (this_cpu_read(secdbg_bad_stack_chk))
		/* reentrance handle_bad_stack */
		cpu_park_loop();

	this_cpu_write(secdbg_bad_stack_chk, 0xBAD);
}
#endif

void *secdbg_base_built_get_debug_base(int type)
{
	if (secdbg_sdn_va) {
		if (type == SDN_MAP_AUTO_COMMENT)
			return &(secdbg_sdn_va->auto_comment);
		else if (type == SDN_MAP_EXTRA_INFO)
			return &(secdbg_sdn_va->extra_info);
	}

	pr_crit("%s: return NULL\n", __func__);

	return NULL;
}

unsigned long secdbg_base_built_get_buf_base(int type)
{
	if (secdbg_sdn_va) {
		pr_crit("%s: return %lx (%lx)\n", __func__, (unsigned long)secdbg_sdn_va, secdbg_sdn_va->map.buf[type].base);
		return secdbg_sdn_va->map.buf[type].base;
	}

	pr_crit("%s: return 0\n", __func__);

	return 0;
}

unsigned long secdbg_base_built_get_buf_size(int type)
{
	if (secdbg_sdn_va)
		return secdbg_sdn_va->map.buf[type].size;

	pr_crit("%s: return 0\n", __func__);

	return 0;
}

static void secdbg_base_built_wait_for_init(struct sec_debug_base_param *param)
{
	/*
	 * We must ensure sdn is cleared completely hear.
	 * Pairs with the smp_store_release() in
	 * secdbg_base_set_sdn_ready().
	 */
	smp_cond_load_acquire(&param->init_sdn_done, VAL);
}

static int secdbg_base_built_probe(struct platform_device *pdev)
{
	struct reserved_mem *rmem;
	struct device_node *rmem_np;

	pr_info("%s: start\n", __func__);

	rmem_np = of_parse_phandle(pdev->dev.of_node, "memory-region", 0);
	if (!rmem_np) {
		pr_crit("%s: no such memory-region\n", __func__);
		return -ENODEV;
	}

	rmem = of_reserved_mem_lookup(rmem_np);
	if (!rmem) {
		pr_crit("%s: no such reserved mem of node name %s\n",
				__func__, pdev->dev.of_node->name);
		return -ENODEV;
	}

	if (!rmem->base || !rmem->size || !rmem->priv) {
		pr_info("%s: not ready ...\n", __func__);
		return -EPROBE_DEFER;
	}

	secdbg_bb_param = (struct sec_debug_base_param *)rmem->priv;
	secdbg_base_built_wait_for_init(secdbg_bb_param);

	secdbg_sdn_va = (struct sec_debug_next *)(secdbg_bb_param->sdn_vaddr);
	sec_debug_next_size = (unsigned int)(rmem->size);

	sdn_ncva_to_pa_offset =  (unsigned long)secdbg_sdn_va - (unsigned long)rmem->base;
	pr_info("%s: offset from va: %lx\n", __func__, sdn_ncva_to_pa_offset);

	pr_info("%s: va: %llx ++ %x\n", __func__, (uint64_t)(rmem->priv), (unsigned int)(rmem->size));

	secdbg_comm_auto_comment_init();
	secdbg_base_built_set_memtab_info(&secdbg_sdn_va->memtab);
	secdbg_extra_info_built_init();

	pr_info("%s: done\n", __func__);

	return 0;
}

static const struct of_device_id sec_debug_built_of_match[] = {
	{ .compatible	= "samsung,sec_debug_built" },
	{},
};
MODULE_DEVICE_TABLE(of, sec_debug_built_of_match);

static struct platform_driver sec_debug_built_driver = {
	.probe = secdbg_base_built_probe,
	.driver  = {
		.name  = "sec_debug_base_built",
		.of_match_table = of_match_ptr(sec_debug_built_of_match),
	},
};

static __init int secdbg_base_built_init(void)
{
	pr_info("%s: init\n", __func__);

	register_die_notifier(&nb_die_block);

	return platform_driver_register(&sec_debug_built_driver);
}
late_initcall_sync(secdbg_base_built_init);

static void __exit secdbg_base_built_exit(void)
{
	platform_driver_unregister(&sec_debug_built_driver);
}
module_exit(secdbg_base_built_exit);

MODULE_DESCRIPTION("Samsung Debug base builtin driver");
MODULE_LICENSE("GPL v2");
