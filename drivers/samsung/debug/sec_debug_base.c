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
#include <linux/memblock.h>
#include <linux/slab.h>
#include <linux/vmalloc.h>
#include <linux/cpumask.h>
#include <linux/percpu.h>
#include <linux/mm.h>
#include <linux/mm_types.h>
#include <linux/platform_device.h>
#include <linux/of.h>
#include <linux/of_fdt.h>
#include <linux/of_reserved_mem.h>
#include <linux/kdebug.h>
#include <linux/sched/task.h>

#include <linux/sec_debug.h>

#include <asm/pgtable.h>
#include <asm/memory.h>

#include <soc/samsung/exynos-pmu-if.h>

#include "sec_debug_internal.h"

#define SEC_DEBUG_NEXT_MEMORY_NAME	"sec_debug_next"

extern int sec_dump_sink_init(void);

/* TODO: masking ? */
enum sec_debug_upload_cause_t {
	UPLOAD_CAUSE_INIT		= 0xCAFEBABE,
	UPLOAD_CAUSE_KERNEL_PANIC	= 0x000000C8,
	UPLOAD_CAUSE_CP_ERROR_FATAL	= 0x000000CC,
	UPLOAD_CAUSE_USER_FAULT		= 0x0000002F,
	UPLOAD_CAUSE_HARD_RESET		= 0x00000066,
	UPLOAD_CAUSE_FORCED_UPLOAD	= 0x00000022,
	UPLOAD_CAUSE_USER_FORCED_UPLOAD	= 0x00000074,
};

static struct sec_debug_next *secdbg_sdn_va;
static unsigned int sec_debug_next_size;
static struct sec_debug_base_param secdbg_base_param;

/* update magic for bootloader */
static void secdbg_base_set_upload_cause(enum sec_debug_upload_cause_t type)
{
	exynos_pmu_write(SEC_DEBUG_PANIC_INFORM, type);

	pr_emerg("sec_debug: set upload cause (0x%x)\n", type);
}

static int secdbg_base_panic_handler(struct notifier_block *nb,
				   unsigned long l, void *buf)
{
	pr_emerg("sec_debug: %s\n", __func__);

	/* Set upload cause */
	//secdbg_base_set_upload_magic(UPLOAD_MAGIC_PANIC, buf);
	if (!strncmp(buf, "User Fault", 10))
		secdbg_base_set_upload_cause(UPLOAD_CAUSE_USER_FAULT);
	else if (!strncmp(buf, "Hard Reset Hook", 15))
		secdbg_base_set_upload_cause(UPLOAD_CAUSE_HARD_RESET);
	else if (!strncmp(buf, "Crash Key", 9))
		secdbg_base_set_upload_cause(UPLOAD_CAUSE_FORCED_UPLOAD);
	else if (!strncmp(buf, "User Crash Key", 14))
		secdbg_base_set_upload_cause(UPLOAD_CAUSE_USER_FORCED_UPLOAD);
	else if (!strncmp(buf, "CP Crash", 8))
		secdbg_base_set_upload_cause(UPLOAD_CAUSE_CP_ERROR_FATAL);
	else
		secdbg_base_set_upload_cause(UPLOAD_CAUSE_KERNEL_PANIC);

	/* dump debugging info */
//	if (dump) {
//		secdbg_base_dump_cpu_stat();
//		debug_show_all_locks();
#ifdef CONFIG_SEC_DEBUG_COMPLETE_HINT
//		secdbg_hint_display_complete_hint();
#endif
//	}

	return NOTIFY_DONE;
}

static int secdbg_base_die_handler(struct notifier_block *nb,
				   unsigned long l, void *buf)
{
	struct die_args *args = (struct die_args *)buf;
	struct pt_regs *regs = args->regs;

	if (!regs)
		return NOTIFY_DONE;

	if (IS_ENABLED(CONFIG_SEC_DEBUG_PRINT_PCLR)) {
		pr_auto(ASL5, "PC is at %pS\n", (void *)(regs)->pc);
		pr_auto(ASL5, "LR is at %pS\n",
			compat_user_mode(regs) ? (void *)regs->compat_lr : (void *)regs->regs[30]);
	}

	return NOTIFY_DONE;
}

/* device tree node for sec_debug */
static struct device_node *kcnst_np;

static uint64_t get_dt_val(const char *name)
{
	uint64_t ret;

	if (of_property_read_u64(kcnst_np, name, &ret)) {
		pr_crit("%s: fail to get %s\n", __func__, name);
		return -EINVAL;
	}

	return ret + (uint64_t)kaslr_offset();
}

static int secdbg_base_get_dt_data(void)
{
	int ret = 0;
	struct device_node *np;

	np = of_find_node_by_path("/sec_debug_data/kconstants");
	if (!np) {
		pr_crit("%s: of_find_node_by_path, failed, /sec_debug/kconstants\n", __func__);
		return -EINVAL;
	}

	pr_crit("%s: set kconstants dt node\n", __func__);
	kcnst_np = np;

	return ret;
}

/* initialize kernel constant data in sec debug next */
static void secdbg_base_set_kconstants(void)
{
	int ret;
	uint64_t va_swapper = 0;

	ret = secdbg_base_get_dt_data();
	if (ret) {
		pr_crit("%s: fail to get dt node\n", __func__);
	} else {
		va_swapper = get_dt_val("swapper_pg_dir");

		pr_info("%s: swapper pg dir: %llx\n", __func__, va_swapper);
		if (!IS_ERR_VALUE(va_swapper)) {
			secdbg_sdn_va->kcnst.pa_swapper = (uint64_t)__virt_to_phys(va_swapper);

			pr_info("%s: swapper: pa: %llx\n", __func__,
					secdbg_sdn_va->kcnst.pa_swapper);
		}
	}

	pr_info("%s: start to get kernel constants\n", __func__);
	secdbg_sdn_va->kcnst.nr_cpus = num_possible_cpus();
	secdbg_sdn_va->kcnst.per_cpu_offset.pa = virt_to_phys(__per_cpu_offset);
	secdbg_sdn_va->kcnst.per_cpu_offset.size = sizeof(__per_cpu_offset[0]);
	secdbg_sdn_va->kcnst.per_cpu_offset.count = ARRAY_SIZE(__per_cpu_offset);

	secdbg_sdn_va->kcnst.phys_offset = PHYS_OFFSET;
	secdbg_sdn_va->kcnst.phys_mask = PHYS_MASK;
	secdbg_sdn_va->kcnst.page_offset = PAGE_OFFSET;
	secdbg_sdn_va->kcnst.page_mask = PAGE_MASK;
	secdbg_sdn_va->kcnst.page_shift = PAGE_SHIFT;

	secdbg_sdn_va->kcnst.va_bits = VA_BITS;
	secdbg_sdn_va->kcnst.kimage_vaddr = kimage_vaddr;
	secdbg_sdn_va->kcnst.kimage_voffset = kimage_voffset;

	secdbg_sdn_va->kcnst.pgdir_shift = PGDIR_SHIFT;
	secdbg_sdn_va->kcnst.pud_shift = PUD_SHIFT;
	secdbg_sdn_va->kcnst.pmd_shift = PMD_SHIFT;
	secdbg_sdn_va->kcnst.ptrs_per_pgd = PTRS_PER_PGD;
	secdbg_sdn_va->kcnst.ptrs_per_pud = PTRS_PER_PUD;
	secdbg_sdn_va->kcnst.ptrs_per_pmd = PTRS_PER_PMD;
	secdbg_sdn_va->kcnst.ptrs_per_pte = PTRS_PER_PTE;

	pr_info("%s: end to get kernel constants\n", __func__);
}

static void init_ess_info(unsigned int index, char *key)
{
	struct ess_info_offset *p;

	p = &(secdbg_sdn_va->ss_info.item[index]);

	secdbg_base_get_kevent_info(p, index);

	memset(p->key, 0, SD_ESSINFO_KEY_SIZE);
	snprintf(p->key, SD_ESSINFO_KEY_SIZE, "%s", key);
}

/* initialize snapshot offset data in sec debug next */
static void secdbg_base_set_essinfo(void)
{
	unsigned int index = 0;

	memset(&(secdbg_sdn_va->ss_info), 0, sizeof(struct sec_debug_ess_info));

	init_ess_info(index++, "kevnt-task");
	init_ess_info(index++, "kevnt-work");
	init_ess_info(index++, "kevnt-irq");
	init_ess_info(index++, "kevnt-freq");
	init_ess_info(index++, "kevnt-idle");
	init_ess_info(index++, "kevnt-thrm");
	init_ess_info(index++, "kevnt-acpm");

	for (; index < SD_NR_ESSINFO_ITEMS;)
		init_ess_info(index++, "empty");

	for (index = 0; index < SD_NR_ESSINFO_ITEMS; index++)
		pr_info("%s: key: %s offset: %lx nr: %x last: %lx\n", __func__,
				secdbg_sdn_va->ss_info.item[index].key,
				secdbg_sdn_va->ss_info.item[index].base,
				secdbg_sdn_va->ss_info.item[index].nr,
				secdbg_sdn_va->ss_info.item[index].last);
}

/* initialize task_struct offset data in sec debug next */
static void secdbg_base_set_taskinfo(void)
{
	secdbg_sdn_va->task.stack_size = THREAD_SIZE;
	secdbg_sdn_va->task.start_sp = THREAD_START_SP;
	secdbg_sdn_va->task.irq_stack.size = IRQ_STACK_SIZE;
	secdbg_sdn_va->task.irq_stack.start_sp = IRQ_STACK_START_SP;

	secdbg_sdn_va->task.ti.struct_size = sizeof(struct thread_info);
	SET_MEMBER_TYPE_INFO(&secdbg_sdn_va->task.ti.flags, struct thread_info, flags);
	SET_MEMBER_TYPE_INFO(&secdbg_sdn_va->task.ts.cpu, struct task_struct, cpu);

	secdbg_sdn_va->task.ts.struct_size = sizeof(struct task_struct);
	SET_MEMBER_TYPE_INFO(&secdbg_sdn_va->task.ts.state, struct task_struct, state);
	SET_MEMBER_TYPE_INFO(&secdbg_sdn_va->task.ts.exit_state, struct task_struct,
					exit_state);
	SET_MEMBER_TYPE_INFO(&secdbg_sdn_va->task.ts.stack, struct task_struct, stack);
	SET_MEMBER_TYPE_INFO(&secdbg_sdn_va->task.ts.flags, struct task_struct, flags);
	SET_MEMBER_TYPE_INFO(&secdbg_sdn_va->task.ts.on_cpu, struct task_struct, on_cpu);
	SET_MEMBER_TYPE_INFO(&secdbg_sdn_va->task.ts.pid, struct task_struct, pid);
	SET_MEMBER_TYPE_INFO(&secdbg_sdn_va->task.ts.on_rq, struct task_struct, on_rq);
	SET_MEMBER_TYPE_INFO(&secdbg_sdn_va->task.ts.comm, struct task_struct, comm);
	SET_MEMBER_TYPE_INFO(&secdbg_sdn_va->task.ts.tasks_next, struct task_struct,
					tasks.next);
	SET_MEMBER_TYPE_INFO(&secdbg_sdn_va->task.ts.thread_group_next,
					struct task_struct, thread_group.next);
	SET_MEMBER_TYPE_INFO(&secdbg_sdn_va->task.ts.fp, struct task_struct,
					thread.cpu_context.fp);
	SET_MEMBER_TYPE_INFO(&secdbg_sdn_va->task.ts.sp, struct task_struct,
					thread.cpu_context.sp);
	SET_MEMBER_TYPE_INFO(&secdbg_sdn_va->task.ts.pc, struct task_struct,
					thread.cpu_context.pc);

	SET_MEMBER_TYPE_INFO(&secdbg_sdn_va->task.ts.sched_info__pcount,
					struct task_struct, sched_info.pcount);
	SET_MEMBER_TYPE_INFO(&secdbg_sdn_va->task.ts.sched_info__run_delay,
					struct task_struct,
					sched_info.run_delay);
	SET_MEMBER_TYPE_INFO(&secdbg_sdn_va->task.ts.sched_info__last_arrival,
					struct task_struct,
					sched_info.last_arrival);
	SET_MEMBER_TYPE_INFO(&secdbg_sdn_va->task.ts.sched_info__last_queued,
					struct task_struct,
					sched_info.last_queued);
#ifdef CONFIG_SEC_DEBUG_DTASK
	SET_MEMBER_TYPE_INFO(&secdbg_sdn_va->task.ts.ssdbg_wait__type,
					struct task_struct,
					android_vendor_data1[0]);
	SET_MEMBER_TYPE_INFO(&secdbg_sdn_va->task.ts.ssdbg_wait__data,
					struct task_struct,
					android_vendor_data1[1]);
#endif

	secdbg_sdn_va->task.init_task = (uint64_t)&init_task;

	/* TODO: remove unused value */
	/* secdbg_sdn_va->task.irq_stack.pcpu_stack = (uint64_t)&irq_stack_ptr; */
}

static void secdbg_base_set_spinlockinfo(void)
{
#ifdef CONFIG_DEBUG_SPINLOCK
	SET_MEMBER_TYPE_INFO(&secdbg_sdn_va->rlock.owner_cpu, struct raw_spinlock, owner_cpu);
	SET_MEMBER_TYPE_INFO(&secdbg_sdn_va->rlock.owner, struct raw_spinlock, owner);
	secdbg_sdn_va->rlock.debug_enabled = 1;
#else
	secdbg_sdn_va->rlock.debug_enabled = 0;
#endif
}

/* from PA to NonCachable VA */
static unsigned long sdn_ncva_to_pa_offset;

void *secdbg_base_get_ncva(unsigned long pa)
{
	return (void *)(pa + sdn_ncva_to_pa_offset);
}
EXPORT_SYMBOL(secdbg_base_get_ncva);

struct watchdogd_info *secdbg_base_get_wdd_info(void)
{
	if (!secdbg_sdn_va) {
		pr_crit("%s: return NULL\n", __func__);
		return NULL;
	}

	pr_crit("%s: return right value\n", __func__);
	return &(secdbg_sdn_va->kernd.wddinfo);
}
EXPORT_SYMBOL_GPL(secdbg_base_get_wdd_info);

/* Share SDN memory area */
void *secdbg_base_get_debug_base(int type)
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
EXPORT_SYMBOL(secdbg_base_get_debug_base);

void *secdbg_base_get_kcnst_base(void)
{
	if (secdbg_sdn_va)
		return &(secdbg_sdn_va->kcnst);

	pr_crit("%s: return NULL\n", __func__);

	return NULL;
}
EXPORT_SYMBOL(secdbg_base_get_kcnst_base);

unsigned long secdbg_base_get_buf_base(int type)
{
	if (secdbg_sdn_va) {
		pr_crit("%s: return %lx\n", __func__, secdbg_sdn_va->map.buf[type].base);
		return secdbg_sdn_va->map.buf[type].base;
	}

	pr_crit("%s: return 0\n", __func__);

	return 0;
}
EXPORT_SYMBOL(secdbg_base_get_buf_base);

unsigned long secdbg_base_get_buf_size(int type)
{
	if (secdbg_sdn_va)
		return secdbg_sdn_va->map.buf[type].size;

	pr_crit("%s: return 0\n", __func__);

	return 0;
}
EXPORT_SYMBOL(secdbg_base_get_buf_size);

unsigned long secdbg_base_get_end_addr(void)
{
	return (unsigned long)secdbg_sdn_va + sec_debug_next_size;
}
EXPORT_SYMBOL(secdbg_base_get_end_addr);

static void secdbg_base_set_version(void)
{
	secdbg_sdn_va->version[1] = SEC_DEBUG_KERNEL_UPPER_VERSION << 16;
	secdbg_sdn_va->version[1] += SEC_DEBUG_KERNEL_LOWER_VERSION;
}

static void secdbg_base_set_magic(void)
{
	secdbg_sdn_va->magic[0] = SEC_DEBUG_MAGIC0;
	secdbg_sdn_va->magic[1] = SEC_DEBUG_MAGIC1;
}

static void secdbg_base_clear_sdn(struct sec_debug_next *d)
{
#define clear_sdn_field(__p, __m)	memset(&(__p)->__m, 0x0, sizeof((__p)->__m))

	clear_sdn_field(d, memtab);
	clear_sdn_field(d, ksyms);
	clear_sdn_field(d, kcnst);
	clear_sdn_field(d, task);
	clear_sdn_field(d, ss_info);
	clear_sdn_field(d, rlock);
	clear_sdn_field(d, kernd);
}

static void secdbg_base_set_sdn_ready(void)
{
	/*
	 * Ensure clearing sdn is visible before we indicate done.
	 * Pairs with the smp_cond_load_acquire() in
	 * secdbg_base_built_wait_for_init().
	 */
	smp_store_release(&secdbg_base_param.init_sdn_done, true);
}

static int secdbg_base_init_sdn(void)
{
	if (!secdbg_sdn_va) {
		pr_crit("%s: SDN is not allocated, quit\n", __func__);

		return -1;
	}

	secdbg_base_set_magic();
	secdbg_base_set_version();

	/* set kallsyms data */
	secdbg_ksym_set_kallsyms_info(&(secdbg_sdn_va->ksyms));

	secdbg_base_set_kconstants();
	secdbg_base_set_essinfo();

	/* set kernel constants */
	secdbg_base_set_taskinfo();
	secdbg_base_set_spinlockinfo();

#if 0
	secdbg_base_set_task_in_pm_suspend((uint64_t)NULL);
	secdbg_base_set_task_in_sys_reboot((uint64_t)NULL);
	secdbg_base_set_task_in_sys_shutdown((uint64_t)NULL);
	secdbg_base_set_task_in_dev_shutdown((uint64_t)NULL);
	secdbg_base_set_sysrq_crash(NULL);
	secdbg_base_set_task_in_soft_lockup((uint64_t)NULL);
	secdbg_base_set_cpu_in_soft_lockup((uint64_t)0);
	secdbg_base_set_task_in_hard_lockup((uint64_t)NULL);
	secdbg_base_set_cpu_in_hard_lockup((uint64_t)0);
	secdbg_base_set_unfrozen_task((uint64_t)NULL);
	secdbg_base_set_unfrozen_task_count((uint64_t)0);
	secdbg_base_set_task_in_sync_irq((uint64_t)NULL, 0, NULL, NULL);
	secdbg_base_clr_device_shutdown_timeinfo();
#endif

	pr_info("%s: done\n", __func__);

	return 0;
}

static int secdbg_base_reserved_memory_setup(struct device *dev)
{
	unsigned long i, j;
	struct page *page;
	struct page **pages;
	int page_size, mem_count = 0;
	void *vaddr;
	pgprot_t prot = __pgprot(PROT_NORMAL_NC);
	unsigned long flags = VM_NO_GUARD | VM_MAP;
	struct reserved_mem *rmem;
	struct device_node *rmem_np;

	pr_info("%s: start\n", __func__);

	mem_count = of_count_phandle_with_args(dev->of_node, "memory-region", NULL);
	if (mem_count <= 0) {
		pr_crit("%s: no such memory-region: ret %d\n", __func__, mem_count);

		return 0;
	}

	for (j = 0 ; j < mem_count ; j++) {
		rmem_np = of_parse_phandle(dev->of_node, "memory-region", j);
		if (!rmem_np) {
			pr_crit("%s: no such memory-region(%lu)\n", __func__, j);

			return 0;
		}

		rmem = of_reserved_mem_lookup(rmem_np);
		if (!rmem) {
			pr_crit("%s: no such reserved mem of node name %s\n", __func__,
				dev->of_node->name);

			return 0;
		} else if (!rmem->base || !rmem->size) {
			pr_crit("%s: wrong base(0x%llx) or size(0x%llx)\n",
					__func__, rmem->base, rmem->size);

			return 0;
		}

		pr_info("%s: %s: set as non cacheable\n", __func__, rmem->name);

		page_size = rmem->size / PAGE_SIZE;
		pages = kcalloc(page_size, sizeof(struct page *), GFP_KERNEL);
		page = phys_to_page(rmem->base);

		for (i = 0; i < page_size; i++)
			pages[i] = page++;

		vaddr = vmap(pages, page_size, flags, prot);
		kfree(pages);
		if (!vaddr) {
			pr_crit("%s: %s: paddr:%llx page_size:%llx failed to mapping between virt and phys\n",
					__func__, rmem->name, rmem->base, rmem->size);

			return 0;
		}

		pr_info("%s: %s: base VA: %llx ++ %x\n", __func__, rmem->name,
			(uint64_t)vaddr, (unsigned int)(rmem->size));

		if (!strncmp(SEC_DEBUG_NEXT_MEMORY_NAME, rmem->name, strlen(rmem->name))) {
			rmem->priv = &secdbg_base_param;
			secdbg_base_param.sdn_vaddr = vaddr;
			secdbg_sdn_va = (struct sec_debug_next *)vaddr;
			sec_debug_next_size = (unsigned int)(rmem->size);
			sdn_ncva_to_pa_offset = (unsigned long)vaddr - (unsigned long)rmem->base;

			secdbg_base_clear_sdn(secdbg_sdn_va);

			secdbg_base_set_sdn_ready();

			pr_info("%s: set private: %llx, offset from va: %lx\n", __func__,
				(uint64_t)vaddr, sdn_ncva_to_pa_offset);
		}
	}

	return 1;
}

static int secdbg_base_probe(struct platform_device *pdev)
{
	int ret = 0;

	pr_info("%s: start\n", __func__);

	if (!secdbg_base_reserved_memory_setup(&pdev->dev)) {
		pr_crit("%s: failed\n", __func__);
		return -ENODEV;
	}

	ret = secdbg_base_init_sdn();
	if (ret) {
		pr_crit("%s: fail to init sdn\n", __func__);
		return -ENODEV;
	}

	ret = sec_dump_sink_init();
	if (ret) {
		pr_crit("%s: fail to sec_init dump_sink\n", __func__);
		return -ENODEV;
	}

	return 0;
}

static const struct of_device_id sec_debug_of_match[] = {
	{ .compatible	= "samsung,sec_debug" },
	{},
};
MODULE_DEVICE_TABLE(of, sec_debug_of_match);

static struct platform_driver sec_debug_driver = {
	.probe = secdbg_base_probe,
	.driver  = {
		.name  = "sec_debug_base",
		.of_match_table = of_match_ptr(sec_debug_of_match),
	},
};

static struct notifier_block nb_panic_block = {
	.notifier_call = secdbg_base_panic_handler,
};

static struct notifier_block nb_die_block = {
	.notifier_call = secdbg_base_die_handler,
};

static int __init secdbg_base_init(void)
{
	int ret;

	ret = atomic_notifier_chain_register(&panic_notifier_list, &nb_panic_block);
	pr_info("%s panic nc register done (%d)\n", __func__, ret);
	register_die_notifier(&nb_die_block);

	return platform_driver_register(&sec_debug_driver);
}
core_initcall(secdbg_base_init);

static void __exit secdbg_base_exit(void)
{
	atomic_notifier_chain_unregister(&panic_notifier_list, &nb_panic_block);
	platform_driver_unregister(&sec_debug_driver);
}
module_exit(secdbg_base_exit);

MODULE_DESCRIPTION("Samsung Debug base driver");
MODULE_LICENSE("GPL v2");
