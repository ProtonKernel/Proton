/*
 * Copyright (c) 2013 Samsung Electronics Co., Ltd.
 *		http://www.samsung.com
 *
 * Debug-SnapShot: Debug Framework for Ramdump based debugging method
 * The original code is Exynos-Snapshot for Exynos SoC
 *
 * Author: Hosung Kim <hosung0.kim@samsung.com>
 * Author: Changki Kim <changki.kim@samsung.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#include <linux/kernel.h>
#include <linux/io.h>
#include <linux/notifier.h>
#include <linux/delay.h>
#include <linux/input.h>
#include <linux/bitops.h>
#include <linux/sched/clock.h>
#include <linux/sched/debug.h>
#include <linux/nmi.h>
#include <linux/init_task.h>
#include <linux/reboot.h>
#include <linux/smc.h>
#include <linux/kdebug.h>
#include <linux/arm-smccc.h>

#include <asm/cputype.h>
#include <asm/smp_plat.h>
#include <asm/system_misc.h>
#include "system-regs.h"

#include <soc/samsung/debug-snapshot.h>
#include <soc/samsung/exynos-pmu-if.h>
#include "debug-snapshot-local.h"

#include <trace/hooks/debug.h>

struct dbg_snapshot_mmu_reg {
	long SCTLR_EL1;
	long TTBR0_EL1;
	long TTBR1_EL1;
	long TCR_EL1;
	long ESR_EL1;
	long FAR_EL1;
	long CONTEXTIDR_EL1;
	long TPIDR_EL0;
	long TPIDRRO_EL0;
	long TPIDR_EL1;
	long MAIR_EL1;
	long ELR_EL1;
	long SP_EL0;
};

extern void flush_cache_all(void);
static struct pt_regs __percpu **dss_core_reg;
static struct dbg_snapshot_mmu_reg __percpu **dss_mmu_reg;
static struct dbg_snapshot_helper_ops dss_soc_ops;

ATOMIC_NOTIFIER_HEAD(dump_task_notifier_list);

void register_dump_one_task_notifier(struct notifier_block *nb)
{
	atomic_notifier_chain_register(&dump_task_notifier_list, nb);
}
EXPORT_SYMBOL(register_dump_one_task_notifier);

void cache_flush_all(void)
{
	flush_cache_all();
}
EXPORT_SYMBOL(cache_flush_all);

static u64 read_ERRSELR_EL1(void)
{
	u64 reg;

	asm volatile ("mrs %0, S3_0_c5_c3_1\n" : "=r" (reg));
	return reg;
}

static void write_ERRSELR_EL1(u64 val)
{
	asm volatile ("msr S3_0_c5_c3_1, %0\n"
			"isb\n" :: "r" ((__u64)val));
}

static u64 read_ERRIDR_EL1(void)
{
	u64 reg;

	asm volatile ("mrs %0, S3_0_c5_c3_0\n" : "=r" (reg));
	return reg;
}

static u64 read_ERXSTATUS_EL1(void)
{
	u64 reg;

	asm volatile ("mrs %0, S3_0_c5_c4_2\n" : "=r" (reg));
	return reg;
}

static void __attribute__((unused)) write_ERXSTATUS_EL1(u64 val)
{
	asm volatile ("msr S3_0_c5_c4_2, %0\n"
			"isb\n" :: "r" ((__u64)val));
}

static u64 read_ERXMISC0_EL1(void)
{
	u64 reg;

	asm volatile ("mrs %0, S3_0_c5_c5_0\n" : "=r" (reg));
	return reg;
}

static u64 read_ERXMISC1_EL1(void)
{
	u64 reg;

	asm volatile ("mrs %0, S3_0_c5_c5_1\n" : "=r" (reg));
	return reg;
}

static u64 read_ERXADDR_EL1(void)
{
	u64 reg;

	asm volatile ("mrs %0, S3_0_c5_c4_3\n" : "=r" (reg));
	return reg;
}

static void dbg_snapshot_set_core_power_stat(unsigned int val, unsigned cpu)
{
	void __iomem *header = dbg_snapshot_get_header_vaddr();

	if (header)
		__raw_writel(val, header + DSS_OFFSET_CORE_POWER_STAT + cpu * 4);
}

static unsigned int dbg_snapshot_get_core_panic_stat(unsigned cpu)
{
	void __iomem *header = dbg_snapshot_get_header_vaddr();

	return header ?  __raw_readl(header + DSS_OFFSET_PANIC_STAT + cpu * 4) : 0;
}

static void dbg_snapshot_set_core_panic_stat(unsigned int val, unsigned cpu)
{
	void __iomem *header = dbg_snapshot_get_header_vaddr();

	if (header)
		__raw_writel(val, header + DSS_OFFSET_PANIC_STAT + cpu * 4);
}

static void dbg_snapshot_report_reason(unsigned int val)
{
	void __iomem *header = dbg_snapshot_get_header_vaddr();

	if (header)
		__raw_writel(val, header + DSS_OFFSET_EMERGENCY_REASON);
}

static unsigned int dbg_snapshot_get_report_reason(void)
{
	void __iomem *header = dbg_snapshot_get_header_vaddr();

	return header ? __raw_readl(header + DSS_OFFSET_EMERGENCY_REASON) : 0;
}

#if IS_ENABLED(CONFIG_SEC_DEBUG_EMERG_WDT_CALLER)
void dbg_snapshot_set_wdt_caller(unsigned long addr)
{
	void __iomem *header = dbg_snapshot_get_header_vaddr();

	if (header) {
		if (__raw_readq(header + DSS_OFFSET_WDT_CALLER))
			return;

		__raw_writeq(addr, header + DSS_OFFSET_WDT_CALLER);
	}
}
EXPORT_SYMBOL(dbg_snapshot_set_wdt_caller);
#else
static void dbg_snapshot_set_wdt_caller(unsigned long addr)
{
	void __iomem *header = dbg_snapshot_get_header_vaddr();

	if (header)
		__raw_writeq(addr, header + DSS_OFFSET_WDT_CALLER);
}
#endif

int dbg_snapshot_start_watchdog(int sec)
{
	if (dss_soc_ops.start_watchdog)
		return dss_soc_ops.start_watchdog(true, 0, sec);

	return -ENODEV;
}
EXPORT_SYMBOL(dbg_snapshot_start_watchdog);

int dbg_snapshot_expire_watchdog(void)
{
	if (dss_soc_ops.expire_watchdog) {
		unsigned long addr = (unsigned long)return_address(0);

		dbg_snapshot_set_wdt_caller(addr);
		dev_emerg(dss_desc.dev, "Caller: %pS, WDTRESET right now!\n", (void *)addr);
		if (dss_soc_ops.expire_watchdog(3, 0))
			return -ENODEV;
		dbg_snapshot_spin_func();
	} else {
		dev_emerg(dss_desc.dev, "There is no wdt functions!\n");
	}

	return -ENODEV;
}
EXPORT_SYMBOL(dbg_snapshot_expire_watchdog);

int dbg_snapshot_kick_watchdog(void)
{
	if (dss_soc_ops.start_watchdog)
		return dss_soc_ops.start_watchdog(false, 0, 0);

	return -ENODEV;
}
EXPORT_SYMBOL(dbg_snapshot_kick_watchdog);

static unsigned long dbg_snapshot_get_wchan(struct task_struct *p)
{
	unsigned long entry = 0;
	struct stack_trace trace = {0,};

	trace.max_entries = 1;
	trace.entries = &entry;

	if (p != current)
		trace.skip = 1;	/* to skip __switch_to */

	save_stack_trace_tsk(p, &trace);

	return entry;
}

static void dbg_snapshot_dump_one_task_info(struct task_struct *tsk, bool is_main)
{
	char state_array[] = {'R', 'S', 'D', 'T', 't', 'X',
			'Z', 'P', 'x', 'K', 'W', 'I', 'N'};
	unsigned char idx = 0;
	unsigned long state, pc = 0;
	unsigned long wchan;
	char symname[KSYM_NAME_LEN];

	if ((tsk == NULL) || !try_get_task_stack(tsk))
		return;

	state = tsk->state | tsk->exit_state;
	pc = KSTK_EIP(tsk);
	while (state) {
		idx++;
		state >>= 1;
	}

	wchan = dbg_snapshot_get_wchan(tsk);
	snprintf(symname, KSYM_NAME_LEN, "%ps", wchan);

	/*
	 * kick watchdog to prevent unexpected reset during panic sequence
	 * and it prevents the hang during panic sequence by watchedog
	 */
	touch_softlockup_watchdog();

	atomic_notifier_call_chain(&dump_task_notifier_list, 0,	(void *)tsk);
	pr_info("%8d %16llu %16llu %16llu %c(%4ld) %1d %16zx %16zx %c %16s [%s]\n",
		tsk->pid, tsk->utime, tsk->stime,
		tsk->se.exec_start, state_array[idx], tsk->state,
		task_cpu(tsk), pc, (unsigned long)tsk,
		is_main ? '*' : ' ', tsk->comm, symname);

	if (tsk->on_cpu && tsk->on_rq && tsk->cpu != smp_processor_id())
		return;

	if (tsk->state == TASK_RUNNING || tsk->state == TASK_WAKING ||
			task_contributes_to_load(tsk))
		sched_show_task(tsk);
}

static inline struct task_struct *get_next_thread(struct task_struct *tsk)
{
	return container_of(tsk->thread_group.next, struct task_struct, thread_group);
}

static void dbg_snapshot_dump_task_info(void)
{
	struct task_struct *frst_tsk, *curr_tsk;
	struct task_struct *frst_thr, *curr_thr;

	pr_info("\n");
	pr_info(" current proc : %d %s\n",
			current->pid, current->comm);
	pr_info("--------------------------------------------------------"
			"-------------------------------------------------\n");
	pr_info("%8s %16s %16s %16s %6s %3s %16s %16s  %16s\n",
			"pid", "uTime", "sTime", "exec(ns)", "stat", "cpu",
			"user_pc", "task_struct", "comm");
	pr_info("--------------------------------------------------------"
			"-------------------------------------------------\n");

	/* processes */
	frst_tsk = &init_task;
	curr_tsk = frst_tsk;
	while (curr_tsk) {
		dbg_snapshot_dump_one_task_info(curr_tsk,  true);
		/* threads */
		if (curr_tsk->thread_group.next != NULL) {
			frst_thr = get_next_thread(curr_tsk);
			curr_thr = frst_thr;
			if (frst_thr != curr_tsk) {
				while (curr_thr != NULL) {
					dbg_snapshot_dump_one_task_info(curr_thr, false);
					curr_thr = get_next_thread(curr_thr);
					if (curr_thr == curr_tsk)
						break;
				}
			}
		}
		curr_tsk = container_of(curr_tsk->tasks.next,
					struct task_struct, tasks);
		if (curr_tsk == frst_tsk)
			break;
	}
	pr_info("--------------------------------------------------------"
			"-------------------------------------------------\n");
}

static void dbg_snapshot_save_system(void *unused)
{
	struct dbg_snapshot_mmu_reg *mmu_reg;

	mmu_reg = *per_cpu_ptr(dss_mmu_reg, raw_smp_processor_id());

	asm volatile ("mrs x1, SCTLR_EL1\n\t"	/* SCTLR_EL1 */
		"mrs x2, TTBR0_EL1\n\t"		/* TTBR0_EL1 */
		"stp x1, x2, [%0]\n\t"
		"mrs x1, TTBR1_EL1\n\t"		/* TTBR1_EL1 */
		"mrs x2, TCR_EL1\n\t"		/* TCR_EL1 */
		"stp x1, x2, [%0, #0x10]\n\t"
		"mrs x1, ESR_EL1\n\t"		/* ESR_EL1 */
		"mrs x2, FAR_EL1\n\t"		/* FAR_EL1 */
		"stp x1, x2, [%0, #0x20]\n\t"
		"mrs x1, CONTEXTIDR_EL1\n\t"	/* CONTEXTIDR_EL1 */
		"mrs x2, TPIDR_EL0\n\t"		/* TPIDR_EL0 */
		"stp x1, x2, [%0, #0x30]\n\t"
		"mrs x1, TPIDRRO_EL0\n\t"	/* TPIDRRO_EL0 */
		"mrs x2, TPIDR_EL1\n\t"		/* TPIDR_EL1 */
		"stp x1, x2, [%0, #0x40]\n\t"
		"mrs x1, MAIR_EL1\n\t"		/* MAIR_EL1 */
		"mrs x2, ELR_EL1\n\t"		/* ELR_EL1 */
		"stp x1, x2, [%0, #0x50]\n\t"
		"mrs x1, SP_EL0\n\t"		/* SP_EL0 */
		"str x1, [%0, 0x60]\n\t" :	/* output */
		: "r"(mmu_reg)			/* input */
		: "%x1", "memory"		/* clobbered register */
	);
}

void dbg_snapshot_ecc_dump(void)
{
	ERRSELR_EL1_t errselr_el1;
	ERRIDR_EL1_t erridr_el1;
	ERXSTATUS_EL1_t erxstatus_el1;
	ERXMISC0_EL1_t erxmisc0_el1;
	ERXMISC1_EL1_t erxmisc1_el1;
	ERXADDR_EL1_t erxaddr_el1;
	int i;

	switch (read_cpuid_part_number()) {
	case ARM_CPU_PART_CORTEX_A55:
	case ARM_CPU_PART_CORTEX_A76:
	case ARM_CPU_PART_CORTEX_A77:
	case ARM_CPU_PART_HERCULES:
	case ARM_CPU_PART_HERA:
		asm volatile ("HINT #16");
		erridr_el1.reg = read_ERRIDR_EL1();
		dev_emerg(dss_desc.dev, "ECC error check erridr_el1.NUM = [0x%lx]\n",
				(unsigned long)erridr_el1.field.NUM);

		for (i = 0; i < (int)erridr_el1.field.NUM; i++) {
#if IS_ENABLED(CONFIG_SEC_DEBUG_AUTO_COMMENT)
			const unsigned int buf_max = 256;
			char summ_buf[buf_max] = "";
			int pos = 0;
#endif

			errselr_el1.reg = read_ERRSELR_EL1();
			errselr_el1.field.SEL = i;
			write_ERRSELR_EL1(errselr_el1.reg);

			isb();

			erxstatus_el1.reg = read_ERXSTATUS_EL1();
			if (!erxstatus_el1.field.Valid) {
				dev_emerg(dss_desc.dev,
					"ERRSELR_EL1.SEL = %d, NOT Error, "
					"ERXSTATUS_EL1 = [0x%lx]\n",
					i, (unsigned long)erxstatus_el1.reg);
				continue;
			}

			if (erxstatus_el1.field.AV) {
				erxaddr_el1.reg = read_ERXADDR_EL1();
				dev_emerg(dss_desc.dev,
						"Error Address : [0x%lx]\n",
						(unsigned long)erxaddr_el1.reg);
#if IS_ENABLED(CONFIG_SEC_DEBUG_AUTO_COMMENT)
				pos += scnprintf(summ_buf + pos, buf_max - pos,
						 "[Addr:0x%lx]",
						 (unsigned long)erxaddr_el1.reg);
#endif
			}
			if (erxstatus_el1.field.OF) {
				dev_emerg(dss_desc.dev,
					"There was more than one error has occurred."
					"the other error have been discarded.\n");
#if IS_ENABLED(CONFIG_SEC_DEBUG_AUTO_COMMENT)
				pos += scnprintf(summ_buf + pos, buf_max - pos,
						 "[Overflow]");
#endif
			}
			if (erxstatus_el1.field.ER) {
				dev_emerg(dss_desc.dev,
					"Error Reported by external abort\n");
#if IS_ENABLED(CONFIG_SEC_DEBUG_AUTO_COMMENT)
				pos += scnprintf(summ_buf + pos, buf_max - pos,
						 "[External]");
#endif
			}
			if (erxstatus_el1.field.UE) {
				dev_emerg(dss_desc.dev,
					"Uncorrected Error (Not defferred)\n");
#if IS_ENABLED(CONFIG_SEC_DEBUG_AUTO_COMMENT)
				pos += scnprintf(summ_buf + pos, buf_max - pos,
						 "[Uncorrected]");
#endif
			}
			if (erxstatus_el1.field.DE) {
				dev_emerg(dss_desc.dev,	"Deffered Error\n");
#if IS_ENABLED(CONFIG_SEC_DEBUG_AUTO_COMMENT)
				pos += scnprintf(summ_buf + pos, buf_max - pos,
						 "[Deferred]");
#endif
			}
			if (erxstatus_el1.field.MV) {
				erxmisc0_el1.reg = read_ERXMISC0_EL1();
				erxmisc1_el1.reg = read_ERXMISC1_EL1();
				dev_emerg(dss_desc.dev,
					"ERXMISC0_EL1 = [0x%lx] ERXMISC1_EL1 = [0x%lx]"
					"ERXSTATUS_EL1[15:8] = [0x%lx]"
					"ERXSTATUS_EL1[7:0] = [0x%lx]\n",
					(unsigned long)erxmisc0_el1.reg,
					(unsigned long)erxmisc1_el1.reg,
					(unsigned long)erxstatus_el1.field.IERR,
					(unsigned long)erxstatus_el1.field.SERR);
#if IS_ENABLED(CONFIG_SEC_DEBUG_AUTO_COMMENT)
				pos += scnprintf(summ_buf + pos, buf_max - pos,
						 "[MISC0:0x%lx][MISC1:0x%lx][IERR:0x%lx][SERR:0x%lx]",
						 (unsigned long)erxmisc0_el1.reg,
						 (unsigned long)erxmisc1_el1.reg,
						 (unsigned long)erxstatus_el1.field.IERR,
						 (unsigned long)erxstatus_el1.field.SERR);
#endif
			}

#if IS_ENABLED(CONFIG_SEC_DEBUG_AUTO_COMMENT)
			pr_auto(ASL6, "ECC cpu:%u sel:%d status:0x%lx %s\n",
					raw_smp_processor_id(), i,
					erxstatus_el1.reg, summ_buf);
#endif
		}
		break;
	default:
		break;
	}
}
EXPORT_SYMBOL(dbg_snapshot_ecc_dump);

static inline void dbg_snapshot_save_core(struct pt_regs *regs)
{
	unsigned int cpu = raw_smp_processor_id();
	struct pt_regs *core_reg = *per_cpu_ptr(dss_core_reg, cpu);

	if (!core_reg) {
		pr_err("Core reg is null\n");
		return;
	}
	if (!regs) {
		asm volatile ("str x0, [%0, #0]\n\t"
				"mov x0, %0\n\t"
				"stp x1, x2, [x0, #0x8]\n\t"
				"stp x3, x4, [x0, #0x18]\n\t"
				"stp x5, x6, [x0, #0x28]\n\t"
				"stp x7, x8, [x0, #0x38]\n\t"
				"stp x9, x10, [x0, #0x48]\n\t"
				"stp x11, x12, [x0, #0x58]\n\t"
				"stp x13, x14, [x0, #0x68]\n\t"
				"stp x15, x16, [x0, #0x78]\n\t"
				"stp x17, x18, [x0, #0x88]\n\t"
				"stp x19, x20, [x0, #0x98]\n\t"
				"stp x21, x22, [x0, #0xa8]\n\t"
				"stp x23, x24, [x0, #0xb8]\n\t"
				"stp x25, x26, [x0, #0xc8]\n\t"
				"stp x27, x28, [x0, #0xd8]\n\t"
				"stp x29, x30, [x0, #0xe8]\n\t" :
				: "r"(core_reg));
		core_reg->sp = core_reg->regs[29];
		core_reg->pc =
			(unsigned long)(core_reg->regs[30] - sizeof(unsigned int));
	} else {
		memcpy(core_reg, regs, sizeof(struct user_pt_regs));
	}

	dev_emerg(dss_desc.dev, "core register saved(CPU:%d)\n", cpu);
}

static void dbg_snapshot_save_context(struct pt_regs *regs, bool stack_dump)
{
	int cpu;
	unsigned long flags;

	if (!dbg_snapshot_get_enable())
		return;

	cpu = raw_smp_processor_id();
	raw_spin_lock_irqsave(&dss_desc.ctrl_lock, flags);

	/* If it was already saved the context information, it should be skipped */
	if (dbg_snapshot_get_core_panic_stat(cpu) !=  DSS_SIGN_PANIC) {
		dbg_snapshot_set_core_panic_stat(DSS_SIGN_PANIC, cpu);
		dbg_snapshot_save_system(NULL);
		dbg_snapshot_save_core(regs);
		dbg_snapshot_ecc_dump();
		dev_emerg(dss_desc.dev, "context saved(CPU:%d)\n", cpu);
	} else
		dev_emerg(dss_desc.dev, "skip context saved(CPU:%d)\n", cpu);

	if (stack_dump)
		dump_stack();

	raw_spin_unlock_irqrestore(&dss_desc.ctrl_lock, flags);

	cache_flush_all();
}

static void dbg_snapshot_dump_panic(char *str, size_t len)
{
	/*  This function is only one which runs in panic funcion */
	if (str && len && len < SZ_512)
		memcpy(dbg_snapshot_get_header_vaddr() + DSS_OFFSET_PANIC_STRING,
				str, len);
}

static int dbg_snapshot_pre_panic_handler(struct notifier_block *nb,
					  unsigned long l, void *buf)
{
	static int in_panic;
	static int cpu = PANIC_CPU_INVALID;

	if (in_panic++ && cpu == raw_smp_processor_id()) {
		dev_err(dss_desc.dev, "Possible infinite panic\n");
		dbg_snapshot_expire_watchdog();
	}

	cpu = raw_smp_processor_id();

	return 0;
}

static int dbg_snapshot_post_panic_handler(struct notifier_block *nb,
					   unsigned long l, void *buf)
{
	unsigned long cpu;

	if (!dbg_snapshot_get_enable())
		return 0;

	/* Again disable log_kevents */
	dbg_snapshot_set_item_enable("log_kevents", false);
	dbg_snapshot_dump_panic(buf, strlen((char *)buf));
	dbg_snapshot_report_reason(DSS_SIGN_PANIC);
	for_each_possible_cpu(cpu) {
		if (cpu_is_offline(cpu))
			dbg_snapshot_set_core_power_stat(DSS_SIGN_DEAD, cpu);
		else
			dbg_snapshot_set_core_power_stat(DSS_SIGN_ALIVE, cpu);
	}

	dbg_snapshot_dump_task_info();
	dbg_snapshot_output();
	dbg_snapshot_log_output();
	dbg_snapshot_print_log_report();
	dbg_snapshot_save_context(NULL, false);

	if (dss_desc.panic_to_wdt)
		dbg_snapshot_expire_watchdog();

	return 0;
}

#if !IS_ENABLED(CONFIG_SEC_DEBUG)
static long probe_kernel_addr(void *dst, const void *src, size_t size)
{
	long ret;
	mm_segment_t old_fs = get_fs();

	set_fs(KERNEL_DS);
	pagefault_disable();
	ret = __copy_from_user_inatomic(dst, src, size);
	pagefault_enable();
	set_fs(old_fs);

	return ret;
}

static void show_data(unsigned long addr, int nbytes, const char *name)
{
	int i, j, nlines;
	u32 *p, data;

	/*
	 * don't attempt to dump non-kernel addresses or
	 * values that are probably just small negative numbers
	 */
	if (addr < PAGE_OFFSET || addr > -256UL)
		return;

	pr_info("\n%s: %#lx:\n", name, addr);
	p = (u32 *)(addr & ~(sizeof(u32) - 1));
	nbytes += (addr & (sizeof(u32) - 1));
	nlines = (nbytes + 31) / 32;

	for (i = 0; i < nlines; i++) {
		pr_cont("%04lx :", (unsigned long)p & 0xffff);
		for (j = 0; j < 8; j++, p++) {
			if (probe_kernel_addr(&data, p, sizeof(data)))
				pr_cont(" ********");
			else
				pr_cont(" %08X", data);
		}
		pr_cont("\n");
	}
}

static void show_extra_register_data(struct pt_regs *regs, int nbytes)
{
	unsigned int i;
	unsigned long flags;
	mm_segment_t fs;

	raw_spin_lock_irqsave(&dss_desc.ctrl_lock, flags);
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
	raw_spin_unlock_irqrestore(&dss_desc.ctrl_lock, flags);
}
#else
static void show_extra_register_data(struct pt_regs *regs, int nbytes)
{
}
#endif /* !IS_ENABLED(CONFIG_SEC_DEBUG) */

static int dbg_snapshot_die_handler(struct notifier_block *nb,
				   unsigned long l, void *buf)
{
	struct die_args *args = (struct die_args *)buf;
	struct pt_regs *regs = args->regs;

	if (user_mode(regs))
		return NOTIFY_DONE;

	dbg_snapshot_save_context(regs, false);
	dbg_snapshot_set_item_enable("log_kevents", false);
	show_extra_register_data(regs, 128);

	return NOTIFY_DONE;
}

static int dbg_snapshot_restart_handler(struct notifier_block *nb,
				    unsigned long mode, void *cmd)
{
	int cpu;

	if (!dbg_snapshot_get_enable())
		return NOTIFY_DONE;

	if (dbg_snapshot_get_report_reason() == DSS_SIGN_PANIC)
		return NOTIFY_DONE;

	dev_emerg(dss_desc.dev, "normal reboot starting\n");
	dbg_snapshot_report_reason(DSS_SIGN_NORMAL_REBOOT);
	dbg_snapshot_scratch_clear();
	dev_emerg(dss_desc.dev, "normal reboot done\n");

	/* clear DSS_SIGN_PANIC when normal reboot */
	for_each_possible_cpu(cpu) {
		dbg_snapshot_set_core_panic_stat(DSS_SIGN_RESET, cpu);
	}

	cache_flush_all();

	return NOTIFY_DONE;
}

static struct notifier_block nb_restart_block = {
	.notifier_call = dbg_snapshot_restart_handler,
	.priority = INT_MAX,
};

static struct notifier_block nb_pre_panic_block = {
	.notifier_call = dbg_snapshot_pre_panic_handler,
	.priority = INT_MAX,
};

static struct notifier_block nb_post_panic_block = {
	.notifier_call = dbg_snapshot_post_panic_handler,
	.priority = INT_MIN,
};

static struct notifier_block nb_die_block = {
	.notifier_call = dbg_snapshot_die_handler,
	.priority = INT_MAX,
};

void dbg_snapshot_do_dpm_policy(unsigned int policy)
{
	switch(policy) {
	case GO_DEFAULT_ID:
		break;
	case GO_PANIC_ID:
		panic("%pS", return_address(0));
		break;
	case GO_WATCHDOG_ID:
	case GO_S2D_ID:
		if (dbg_snapshot_expire_watchdog())
			panic("WDT rst fail for s2d, wdt device not probed");
		dbg_snapshot_spin_func();
		break;
	case GO_ARRAYDUMP_ID:
		if (dss_soc_ops.run_arraydump)
			dss_soc_ops.run_arraydump();
		break;
	case GO_SCANDUMP_ID:
		if (dss_soc_ops.run_scandump_mode)
			dss_soc_ops.run_scandump_mode();
		break;
	}
}
EXPORT_SYMBOL(dbg_snapshot_do_dpm_policy);

void dbg_snapshot_register_wdt_ops(void *start, void *expire, void *stop)
{
	if (start)
		dss_soc_ops.start_watchdog = start;
	if (expire)
		dss_soc_ops.expire_watchdog = expire;
	if (stop)
		dss_soc_ops.stop_watchdog = stop;

	dev_info(dss_desc.dev, "Add %s%s%s funtions from %pS\n",
			start ? "wdt start, " : "",
			expire ? "wdt expire, " : "",
			stop ? "wdt stop" : "",
			return_address(0));
}
EXPORT_SYMBOL(dbg_snapshot_register_wdt_ops);

void dbg_snapshot_register_debug_ops(void *halt, void *arraydump,
				    void *scandump)
{
	if (halt)
		dss_soc_ops.stop_all_cpus = halt;
	if (arraydump)
		dss_soc_ops.run_arraydump = arraydump;
	if (scandump)
		dss_soc_ops.run_scandump_mode = scandump;

	dev_info(dss_desc.dev, "Add %s%s%s funtions from %pS\n",
			halt ? "halt, " : "",
			arraydump ? "arraydump, " : "",
			scandump ? "scandump mode" : "",
			return_address(0));
}
EXPORT_SYMBOL(dbg_snapshot_register_debug_ops);

static void dbg_snapshot_ipi_stop(void *ignore, struct pt_regs *regs)
{
	dbg_snapshot_save_context(regs, true);
}

void dbg_snapshot_init_utils(void)
{
	size_t vaddr;
	int i;

	vaddr = dss_items[DSS_ITEM_HEADER_ID].entry.vaddr;

	dss_mmu_reg = alloc_percpu(struct dbg_snapshot_mmu_reg *);
	dss_core_reg = alloc_percpu(struct pt_regs *);
	for_each_possible_cpu(i) {
		*per_cpu_ptr(dss_mmu_reg, i) = (struct dbg_snapshot_mmu_reg *)
					  (vaddr + DSS_HEADER_SZ +
					   i * DSS_MMU_REG_OFFSET);
		*per_cpu_ptr(dss_core_reg, i) = (struct pt_regs *)
					   (vaddr + DSS_HEADER_SZ + DSS_MMU_REG_SZ +
					    i * DSS_CORE_REG_OFFSET);
	}

	register_die_notifier(&nb_die_block);
	register_restart_handler(&nb_restart_block);
	atomic_notifier_chain_register(&panic_notifier_list, &nb_pre_panic_block);
	atomic_notifier_chain_register(&panic_notifier_list, &nb_post_panic_block);
	register_trace_android_vh_ipi_stop(dbg_snapshot_ipi_stop, NULL);

	smp_call_function(dbg_snapshot_save_system, NULL, 1);
	dbg_snapshot_save_system(NULL);
}
