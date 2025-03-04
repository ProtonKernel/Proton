// SPDX-License-Identifier: GPL-2.0-only
/*
 * sec_debug_test.c
 *
 * Copyright (c) 2019 Samsung Electronics Co., Ltd
 *              http://www.samsung.com
 */

#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/sched.h>
#include <linux/delay.h>
#include <linux/cpu.h>
#include <linux/io.h>
#include <linux/slab.h>
#include <asm-generic/io.h>
#include <linux/ctype.h>
#include <linux/sec_debug.h>
#include <linux/kthread.h>
#include <linux/interrupt.h>
#include <linux/irq.h>
#include <linux/preempt.h>
#include <linux/rwsem.h>
#include <linux/moduleparam.h>
#include <linux/cpumask.h>
#include <linux/reboot.h>
#include <asm/stackprotector.h>

#include <soc/samsung/debug-snapshot.h>
#include <soc/samsung/exynos-pmu-if.h>
#include <soc/samsung/exynos_pm_qos.h>
#include <uapi/linux/sched/types.h>

#include "sec_debug_internal.h"

typedef void (*force_error_func)(char **argv, int argc);

static void simulate_KP(char **argv, int argc);
static void simulate_DP(char **argv, int argc);
static void simulate_QDP(char **argv, int argc);
static void simulate_SVC(char **argv, int argc);
static void simulate_SFR(char **argv, int argc);
static void simulate_WP(char **argv, int argc);
static void simulate_TP(char **argv, int argc);
static void simulate_PANIC(char **argv, int argc);
static void simulate_BUG(char **argv, int argc);
static void simulate_WARN(char **argv, int argc);
static void simulate_DABRT(char **argv, int argc);
static void simulate_SAFEFAULT(char **argv, int argc);
static void simulate_PABRT(char **argv, int argc);
static void simulate_UNDEF(char **argv, int argc);
static void simulate_DFREE(char **argv, int argc);
static void simulate_DREF(char **argv, int argc);
static void simulate_MCRPT(char **argv, int argc);
static void simulate_LOMEM(char **argv, int argc);
static void simulate_SOFT_LOCKUP(char **argv, int argc);
static void simulate_SOFTIRQ_LOCKUP(char **argv, int argc);
static void simulate_SOFTIRQ_STORM(char **argv, int argc);
static void simulate_TASK_HARD_LOCKUP(char **argv, int argc);
static void simulate_IRQ_HARD_LOCKUP(char **argv, int argc);
static void simulate_TASK_HARD_LATENCY(char **argv, int argc);
static void simulate_IRQ_HARD_LATENCY(char **argv, int argc);
static void simulate_BAD_SCHED(char **argv, int argc);
static void simulate_SPIN_LOCKUP(char **argv, int argc);
static void simulate_SPINLOCK_ALLCORE(char **argv, int argc);
static void simulate_SPINLOCK_SOFTLOCKUP(char **argv, int argc);
static void simulate_SPINLOCK_HARDLOCKUP(char **argv, int argc);
static void simulate_RW_LOCKUP(char **argv, int argc);
static void simulate_ALLRW_LOCKUP(char **argv, int argc);
static void simulate_PC_ABORT(char **argv, int argc);
static void simulate_SP_ABORT(char **argv, int argc);
static void simulate_JUMP_ZERO(char **argv, int argc);
static void simulate_BUSMON_ERROR(char **argv, int argc);
static void simulate_UNALIGNED(char **argv, int argc);
static void simulate_WRITE_RO(char **argv, int argc);
static void simulate_OVERFLOW(char **argv, int argc);
static void simulate_CORRUPT_MAGIC(char **argv, int argc);
static void simulate_IRQ_STORM(char **argv, int argc);
static void simulate_SYNC_IRQ_LOCKUP(char **argv, int argc);
static void simulate_DISK_SLEEP(char **argv, int argc);
static void simulate_CORRUPT_DELAYED_WORK(char **argv, int argc);
static void simulate_MUTEX_AA(char **argv, int argc);
static void simulate_MUTEX_ABBA(char **argv, int argc);
static void simulate_LIST_BUG(char **argv, int argc);
#ifdef CONFIG_SEC_DEBUG_FREQ
static void simulate_FREQ_SKEW(char **argv, int argc);
#endif
static void simulate_RWSEM_R(char **argv, int argc);
static void simulate_RWSEM_W(char **argv, int argc);
static void simulate_PRINTK_FAULT(char **argv, int argc);
static void simulate_EXIN_UNFZ(char **arg, int argc);
static void simulate_WQLOCK_BUSY_WORKER(char **argv, int argc);
static void simulate_WQLOCK_BUSY_TASK(char **argv, int argc);
static void simulate_STACK_CORRUPTION(char **argv, int argc);
static void simulate_POWER_OFF(char **argv, int argc);
static void simulate_EMERGENT_REBOOT(char **argv, int argc);

enum {
	FORCE_KERNEL_PANIC = 0,		/* KP */
	FORCE_WATCHDOG,			/* DP */
	FORCE_QUICKWATCHDOG,		/* QDP */
	FORCE_SVC,			/* SVC */
	FORCE_SFR,			/* SFR */
	FORCE_WARM_RESET,		/* WP */
	FORCE_HW_TRIPPING,		/* TP */
	FORCE_PANIC,			/* PANIC */
	FORCE_BUG,			/* BUG */
	FORCE_WARN,			/* WARN */
	FORCE_DATA_ABORT,		/* DABRT */
	FORCE_SAFEFAULT_ABORT,		/* SAFE FAULT */
	FORCE_PREFETCH_ABORT,		/* PABRT */
	FORCE_UNDEFINED_INSTRUCTION,	/* UNDEF */
	FORCE_DOUBLE_FREE,		/* DFREE */
	FORCE_DANGLING_REFERENCE,	/* DREF */
	FORCE_MEMORY_CORRUPTION,	/* MCRPT */
	FORCE_LOW_MEMEMORY,		/* LOMEM */
	FORCE_SOFT_LOCKUP,		/* SOFT LOCKUP */
	FORCE_SOFTIRQ_LOCKUP,		/* SOFTIRQ LOCKUP */
	FORCE_SOFTIRQ_STORM,		/* SOFTIRQ_STORM */
	FORCE_TASK_HARD_LOCKUP,		/* TASK HARD LOCKUP */
	FORCE_IRQ_HARD_LOCKUP,		/* IRQ HARD LOCKUP */
	FORCE_TASK_HARD_LATENCY,	/* TASK HARD LATENCY*/
	FORCE_IRQ_HARD_LATENCY,		/* IRQ_HARD LATENCY */
	FORCE_SPIN_LOCKUP,		/* SPIN LOCKUP */
	FORCE_SPIN_ALLCORE,		/* SPINLOCK ALL CORE */
	FORCE_SPIN_SOFTLOCKUP,		/* SPINLOCK SOFT LOCKUP */
	FORCE_SPIN_HARDLOCKUP,		/* SPINLOCK HARD LOCKUP */
	FORCE_RW_LOCKUP,		/* RW LOCKUP */
	FORCE_ALLRW_LOCKUP,		/* ALL RW LOCKUP */
	FORCE_PC_ABORT,			/* PC ABORT */
	FORCE_SP_ABORT,			/* SP ABORT */
	FORCE_JUMP_ZERO,		/* JUMP TO ZERO */
	FORCE_BUSMON_ERROR,		/* BUSMON ERROR */
	FORCE_UNALIGNED,		/* UNALIGNED WRITE */
	FORCE_WRITE_RO,			/* WRITE RODATA */
	FORCE_OVERFLOW,			/* STACK OVERFLOW */
	FORCE_BAD_SCHEDULING,		/* BAD SCHED */
	FORCE_CORRUPT_MAGIC,		/* CM */
	FORCE_IRQ_STORM,		/* IRQ STORM */
	FORCE_SYNC_IRQ_LOCKUP,		/* SYNCIRQ LOCKUP */
	FORCE_DISK_SLEEP,		/* DISK SLEEP */
	FORCE_CORRUPT_DELAYED_WORK,	/* CORRUPT DELAYED WORK */
	FORCE_MUTEX_AA,			/* MUTEX AA */
	FORCE_MUTEX_ABBA,		/* MUTEX ABBA */
	FORCE_LIST_BUG,			/* LIST BUG */
#ifdef CONFIG_SEC_DEBUG_FREQ
	FORCE_FREQ_SKEW,		/* FREQ SKEW */
#endif
	FORCE_RWSEM_R,			/* RWSEM READER */
	FORCE_RWSEM_W,			/* RWSEM WRITER */
	FORCE_PRINTK_FAULT,		/* PRINTK FAULT */
	FORCE_EXIN_UNFZ,		/* EXTRA INFO UN FREEZE TASK */
	FORCE_WQLOCK_BUSY_WORKER,	/* WORKQUEUE LOCKUP BUSY WORKER */
	FORCE_WQLOCK_BUSY_TASK,		/* WORKQUEUE LOCKUP BUSY TASK */
	FORCE_STACK_CORRUPTION,		/* STACK CORRUPTION */
	FORCE_POWER_OFF,		/* POWER OFF PRE-NOTIFIER */
	FORCE_EMERGENCY_RESTART,		/* EMERGENCY RESTART */
	NR_FORCE_ERROR,
};

struct force_error_item {
	char errname[SZ_32];
	force_error_func errfunc;
};

struct force_error {
	struct force_error_item item[NR_FORCE_ERROR];
};

struct force_error force_error_vector = {
	.item = {
		{"KP",		&simulate_KP},
		{"DP",		&simulate_DP},
		{"QDP",		&simulate_QDP},
		{"SVC",		&simulate_SVC},
		{"SFR",		&simulate_SFR},
		{"WP",		&simulate_WP},
		{"TP",		&simulate_TP},
		{"panic",	&simulate_PANIC},
		{"bug",		&simulate_BUG},
		{"warn",	&simulate_WARN},
		{"dabrt",	&simulate_DABRT},
		{"safefault",	&simulate_SAFEFAULT},
		{"pabrt",	&simulate_PABRT},
		{"undef",	&simulate_UNDEF},
		{"dfree",	&simulate_DFREE},
		{"danglingref",	&simulate_DREF},
		{"memcorrupt",	&simulate_MCRPT},
		{"lowmem",	&simulate_LOMEM},
		{"softlockup",	&simulate_SOFT_LOCKUP},
		{"softirqlockup",	&simulate_SOFTIRQ_LOCKUP},
		{"softirqstorm",	&simulate_SOFTIRQ_STORM},
		{"taskhardlockup",	&simulate_TASK_HARD_LOCKUP},
		{"irqhardlockup",	&simulate_IRQ_HARD_LOCKUP},
		{"taskhardlatency",	&simulate_TASK_HARD_LATENCY},
		{"irqhardlatency",	&simulate_IRQ_HARD_LATENCY},
		{"spinlockup",	&simulate_SPIN_LOCKUP},
		{"spinlock-allcore",	&simulate_SPINLOCK_ALLCORE},
		{"spinlock-softlockup",	&simulate_SPINLOCK_SOFTLOCKUP},
		{"spinlock-hardlockup",	&simulate_SPINLOCK_HARDLOCKUP},
		{"rwlockup",	&simulate_RW_LOCKUP},
		{"allrwlockup", &simulate_ALLRW_LOCKUP},
		{"pcabort",	&simulate_PC_ABORT},
		{"spabort",	&simulate_SP_ABORT},
		{"jumpzero",	&simulate_JUMP_ZERO},
		{"busmon",	&simulate_BUSMON_ERROR},
		{"unaligned",	&simulate_UNALIGNED},
		{"writero",	&simulate_WRITE_RO},
		{"overflow",	&simulate_OVERFLOW},
		{"badsched",	&simulate_BAD_SCHED},
		{"CM",		&simulate_CORRUPT_MAGIC},
		{"irqstorm",	&simulate_IRQ_STORM},
		{"syncirqlockup",	&simulate_SYNC_IRQ_LOCKUP},
		{"disksleep",	&simulate_DISK_SLEEP},
		{"CDW",		&simulate_CORRUPT_DELAYED_WORK},
		{"mutexaa",	&simulate_MUTEX_AA},
		{"mutexabba",	&simulate_MUTEX_ABBA},
		{"listbug",	&simulate_LIST_BUG},
#ifdef CONFIG_SEC_DEBUG_FREQ
		{"freqskew",	&simulate_FREQ_SKEW},
#endif
		{"rwsem-r",	&simulate_RWSEM_R},
		{"rwsem-w",	&simulate_RWSEM_W},
		{"printkfault",	&simulate_PRINTK_FAULT},
		{"exinunfz",	&simulate_EXIN_UNFZ},
		{"wqlockup-busyworker",	&simulate_WQLOCK_BUSY_WORKER},
		{"wqlockup-busytask",	&simulate_WQLOCK_BUSY_TASK},
		{"stackcorrupt",	&simulate_STACK_CORRUPTION},
		{"poweroff",	&simulate_POWER_OFF},
		{"erst",	&simulate_EMERGENT_REBOOT},
	}
};

struct debug_delayed_work_info {
	int start;
	u32 work_magic;
	struct delayed_work read_info_work;
};

static struct work_struct lockup_work;

static DEFINE_SPINLOCK(sec_debug_test_lock);
static DEFINE_RWLOCK(sec_debug_test_rw_lock);

static int str_to_num(char *s)
{
	if (s) {
		switch (s[0]) {
		case '0':
		case '1':
		case '2':
		case '3':
		case '4':
		case '5':
		case '6':
		case '7':
			return (s[0] - '0');

		default:
			return -1;
		}
	}
	return -1;
}

/* timeout for dog bark/bite */
#define DELAY_TIME 60000

#define EXYNOS_PS_HOLD_CONTROL 0x030c

static void pull_down_other_cpus(void)
{
#ifdef CONFIG_HOTPLUG_CPU
	int cpu, ret;

	for (cpu = num_possible_cpus() - 1; cpu > 0 ; cpu--) {
		ret = cpu_down(cpu);
		if (ret)
			pr_crit("%s: CORE%d ret: %x\n", __func__, cpu, ret);
	}
#endif
}

static void simulate_KP(char **argv, int argc)
{
	*(volatile unsigned int *)0x0 = 0x0; /* SVACE: intended */
}

static void simulate_DP(char **argv, int argc)
{
	pull_down_other_cpus();

	pr_crit("%s() start to hanging\n", __func__);
	local_irq_disable();
	mdelay(DELAY_TIME);
	local_irq_enable();

	/* should not reach here */
}

extern int s3c2410wdt_set_emergency_reset(unsigned int timeout_cnt, int index);

static void simulate_QDP(char **argv, int argc)
{
	if (!argc) {
		s3c2410wdt_set_emergency_reset(10, 0);
		mdelay(DELAY_TIME);
	} else {
		dbg_snapshot_expire_watchdog();
		mdelay(DELAY_TIME);
	}

	/* should not reach here */
}

static void simulate_SVC(char **argv, int argc)
{
	asm("svc #0x0");

	/* should not reach here */
}

static void simulate_SFR(char **argv, int argc)
{
	int ret;
	unsigned long reg, val;
	void __iomem *addr;

	if (!argc)
		return;

	ret = kstrtoul(argv[0], 16, &reg);
	addr = ioremap(reg, 0x10);
	if (!addr) {
		pr_crit("%s() failed to remap 0x%lx, quit\n", __func__, reg);
		return;
	}

	pr_crit("%s() 1st parameter: 0x%lx\n", __func__, reg);


	if (argc == 1) {
		pr_crit("%s() there is no 2nd parameter\n", __func__);
		pr_crit("%s() try to read 0x%lx\n", __func__, reg);

		ret = __raw_readl(addr);

		pr_crit("%s() result : 0x%x\n", __func__, ret);

	} else {
		ret = kstrtoul(argv[1], 16, &val);
		pr_crit("%s() 2nd parameter: 0x%lx\n", __func__, val);
		pr_crit("%s() try to write 0x%lx to 0x%lx\n", __func__, val, reg);

		__raw_writel(val, addr);
	}


	/* should not reach here */
}

static void simulate_WP(char **argv, int argc)
{
	unsigned int ps_hold_control;

	exynos_pmu_read(EXYNOS_PS_HOLD_CONTROL, &ps_hold_control);
	exynos_pmu_write(EXYNOS_PS_HOLD_CONTROL, ps_hold_control & 0xFFFFFEFF);
}

static void simulate_TP(char **argv, int argc)
{
	pr_crit("%s()\n", __func__);
}

static void simulate_PANIC(char **argv, int argc)
{
	if (argv[0] == NULL)
		panic("simulate_panic");
	else
		panic("%s", argv[0]);
}

static void simulate_BUG(char **argv, int argc)
{
	BUG();
}

static void simulate_WARN(char **argv, int argc)
{
	WARN_ON(1);
}

static void simulate_DABRT(char **argv, int argc)
{
	*((volatile int *)0) = 0; /* SVACE: intended */
}

static void simulate_PABRT(char **argv, int argc)
{
	((void (*)(void))0x0)(); /* SVACE: intended */
}

static void simulate_UNDEF(char **argv, int argc)
{
	asm volatile(".word 0xe7f001f2\n\t");
	unreachable();
}

static void simulate_DFREE(char **argv, int argc)
{
	void *p;

	p = kmalloc(sizeof(unsigned int), GFP_KERNEL);
	if (p) {
		*(unsigned int *)p = 0x0;
		kfree(p);
		msleep(1000);
		kfree(p); /* SVACE: intended */
	}
}

static void simulate_DREF(char **argv, int argc)
{
	unsigned int *p;

	p = kmalloc(sizeof(int), GFP_KERNEL);
	if (p) {
		kfree(p);
		*p = 0x1234; /* SVACE: intended */
	}
}

static void simulate_MCRPT(char **argv, int argc)
{
	int *ptr;

	ptr = kmalloc(sizeof(int), GFP_KERNEL);
	if (ptr) {
		*ptr++ = 4;
		*ptr = 2;
		panic("MEMORY CORRUPTION");
	}
}

static void simulate_LOMEM(char **argv, int argc)
{
	int i = 0;

	pr_crit("Allocating memory until failure!\n");
	while (kmalloc(128 * 1024, GFP_KERNEL)) /* SVACE: intended */
		i++;
	pr_crit("Allocated %d KB!\n", i * 128);
}

static void simulate_SOFT_LOCKUP(char **argv, int argc)
{
#if 0
#ifdef CONFIG_LOCKUP_DETECTOR
	softlockup_panic = 1;
#endif
	preempt_disable();
	asm("b .");
	preempt_enable();
#endif
}

static struct tasklet_struct sec_debug_tasklet;
static struct hrtimer softirq_storm_hrtimer;
static unsigned long sample_period;

static void softirq_lockup_tasklet(unsigned long data)
{
	asm("b .");
}

static void simulate_SOFTIRQ_handler(void *info)
{
	tasklet_schedule(&sec_debug_tasklet);
}

static void simulate_SOFTIRQ_LOCKUP(char **argv, int argc)
{
	int cpu;

	tasklet_init(&sec_debug_tasklet, softirq_lockup_tasklet, 0);

	if (argc) {
		cpu = str_to_num(argv[0]);
		smp_call_function_single(cpu,
					simulate_SOFTIRQ_handler, 0, 0);
	} else {
		for_each_online_cpu(cpu) {
			if (cpu == smp_processor_id())
				continue;
			smp_call_function_single(cpu,
						 simulate_SOFTIRQ_handler,
						 0, 0);
		}
	}
}

static void softirq_storm_tasklet(unsigned long data)
{
	preempt_disable();
	mdelay(500);
	pr_crit("%s\n", __func__);
	preempt_enable();
}

static enum hrtimer_restart softirq_storm_timer_fn(struct hrtimer *hrtimer)
{
	hrtimer_forward_now(hrtimer, ns_to_ktime(sample_period));
	tasklet_schedule(&sec_debug_tasklet);
	return HRTIMER_RESTART;
}

static void simulate_SOFTIRQ_STORM(char **argv, int argc)
{
	if (!argc || kstrtol(argv[0], 10, &sample_period))
		sample_period = 1000000;

	pr_crit("%s : set period (%d)\n", __func__, (unsigned int)sample_period);

	tasklet_init(&sec_debug_tasklet, softirq_storm_tasklet, 0);

	hrtimer_init(&softirq_storm_hrtimer, CLOCK_MONOTONIC, HRTIMER_MODE_REL);
	softirq_storm_hrtimer.function = softirq_storm_timer_fn;
	hrtimer_start(&softirq_storm_hrtimer, ns_to_ktime(sample_period),
	      HRTIMER_MODE_REL_PINNED);
}

int task_hard_lockup(void *info)
{
	while (!kthread_should_stop()) {
		local_irq_disable();
		asm("b .");
	}
	return 0;
}

static void simulate_TASK_HARD_LOCKUP(char **argv, int argc)
{
	int cpu;
	struct task_struct *tsk;

	tsk = kthread_create(task_hard_lockup, 0, "hl_test");
	if (IS_ERR(tsk)) {
		pr_warn("Failed to create thread hl_test\n");
		return;
	}

	if (argc) {
		cpu = str_to_num(argv[0]);
		set_cpus_allowed_ptr(tsk, cpumask_of(cpu));
	} else {
		set_cpus_allowed_ptr(tsk, cpumask_of(smp_processor_id()));
	}
	wake_up_process(tsk);
}

static void simulate_IRQ_HARD_LOCKUP_handler(void *info)
{
	asm("b .");
}

static void simulate_IRQ_HARD_LOCKUP(char **argv, int argc)
{
	int cpu;

	if (argc) {
		cpu = str_to_num(argv[0]);
		smp_call_function_single(cpu,
					 simulate_IRQ_HARD_LOCKUP_handler, 0, 0);
	} else {
		for_each_online_cpu(cpu) {
			if (cpu == smp_processor_id())
				continue;
			smp_call_function_single(cpu,
						 simulate_IRQ_HARD_LOCKUP_handler,
						 0, 0);
		}
	}
}

static unsigned long sec_latency = 200;

static int task_hard_latency(void *info)
{
	while (!kthread_should_stop()) {
		local_irq_disable();
		pr_crit("%s [latency:%lu]\n", __func__, sec_latency);
		mdelay(sec_latency);
		local_irq_enable();
		set_current_state(TASK_INTERRUPTIBLE);
		schedule();
		__set_current_state(TASK_RUNNING);
	}
	return 0;
}

static DEFINE_PER_CPU(struct task_struct *, sec_tsk);

static void create_and_wakeup_thread(int cpu)
{
	struct task_struct *tsk = per_cpu(sec_tsk, cpu);

	if (!tsk) {
		tsk = kthread_create(task_hard_latency, 0, "hl_test/%d", cpu);
		if (IS_ERR(tsk)) {
			pr_warn("Failed to create thread hl_test\n");
			return;
		}

		per_cpu(sec_tsk, cpu) = tsk;
	}
	set_cpus_allowed_ptr(tsk, cpumask_of(cpu));
	wake_up_process(tsk);
}

static void simulate_TASK_HARD_LATENCY(char **argv, int argc)
{
	int ret;
	int cpu = 0;

	if (argc) {
		cpu = str_to_num(argv[0]);
		if (argc >= 2)
			ret = kstrtoul(argv[1], 10, &sec_latency);
	}

	if (!argc || cpu < 0 || cpu >= num_possible_cpus()) {
		pr_crit("%s() generate task to all cores [latency:%lu]\n", __func__, sec_latency);
		for_each_online_cpu(cpu) {
			create_and_wakeup_thread(cpu);
		}
	} else {
		pr_crit("%s() generate task [latency:%lu cpu:%d]\n", __func__, sec_latency, cpu);
		create_and_wakeup_thread(cpu);
	}
}

static void simulate_IRQ_HARD_LATENCY_handler(void *info)
{
	pr_crit("%s latency : %lu\n", __func__, sec_latency);
	mdelay(sec_latency);
}

static void simulate_IRQ_HARD_LATENCY(char **argv, int argc)
{
	int ret;
	int cpu = 0;

	if (argc) {
		cpu = str_to_num(argv[0]);
		if (argc == 2)
			ret = kstrtoul(argv[1], 10, &sec_latency);
	}

	if (!argc || cpu < 0 || cpu >= num_possible_cpus()) {
		pr_crit("%s() generate irq to all cores[latency:%lu]\n", __func__, sec_latency);
		for_each_online_cpu(cpu) {
			smp_call_function_single(cpu,
				simulate_IRQ_HARD_LATENCY_handler, 0, 0);
		}
	} else {
		pr_crit("%s() generate irq [latency:%lu cpu:%d]\n", __func__, sec_latency, cpu);
		smp_call_function_single(cpu,
			simulate_IRQ_HARD_LATENCY_handler, 0, 0);
	}
}

static struct exynos_pm_qos_request sec_min_pm_qos;

static void simulate_ALLSPIN_LOCKUP_handler(void *info)
{
	unsigned long flags = 0;

	int cpu = smp_processor_id();

	pr_crit("%s()/cpu:%d\n", __func__, cpu);
	spin_lock_irqsave(&sec_debug_test_lock, flags);
	spin_lock_irqsave(&sec_debug_test_lock, flags);
}

static void make_all_cpu_online(void)
{
	pr_crit("%s()\n", __func__);

	exynos_pm_qos_add_request(&sec_min_pm_qos, PM_QOS_CPU_ONLINE_MIN,
			   PM_QOS_CPU_ONLINE_MIN_DEFAULT_VALUE);
	exynos_pm_qos_update_request(&sec_min_pm_qos,
			      num_possible_cpus());
	while (true) {
		if (num_online_cpus() == num_possible_cpus())
			break;
	}
}

static void simulate_SPINLOCK_ALLCORE(char **argv, int argc)
{
	unsigned long flags;

	make_all_cpu_online();
	preempt_disable();
	smp_call_function(simulate_ALLSPIN_LOCKUP_handler, NULL, 0);
	spin_lock_irqsave(&sec_debug_test_lock, flags);
	spin_lock_irqsave(&sec_debug_test_lock, flags);
}

static void simulate_SPINLOCK_SOFTLOCKUP(char **argv, int argc)
{
	make_all_cpu_online();
	preempt_disable();
	spin_lock(&sec_debug_test_lock);
	spin_lock(&sec_debug_test_lock);
}

static void simulate_SPINLOCK_HARDLOCKUP(char **argv, int argc)
{
	make_all_cpu_online();
	preempt_disable();
	smp_call_function_single(1, simulate_ALLSPIN_LOCKUP_handler, NULL, 0);
	smp_call_function_single(2, simulate_ALLSPIN_LOCKUP_handler, NULL, 0);
	smp_call_function_single(3, simulate_ALLSPIN_LOCKUP_handler, NULL, 0);
	smp_call_function_single(4, simulate_ALLSPIN_LOCKUP_handler, NULL, 0);
}

static void simulate_SAFEFAULT(char **argv, int argc)
{
	/* TODO : spinlock is in built-in */
	/* spin_debug_skip_panic(); */

	make_all_cpu_online();
	preempt_disable();

	smp_call_function(simulate_ALLSPIN_LOCKUP_handler, NULL, 0);

	pr_info("%s %p %s %d %p %p %lx\n",
		__func__, current, current->comm, current->pid,
		current_thread_info(), current->stack, current_stack_pointer);

	write_sysreg(0xfafa, sp_el0);
	mb();

	*((volatile unsigned int *)0) = 0;
}

static void simulate_SPIN_LOCKUP(char **argv, int argc)
{
	spin_lock(&sec_debug_test_lock);
	spin_lock(&sec_debug_test_lock);
}

static void simulate_RW_LOCKUP(char **argv, int argc)
{
	write_lock(&sec_debug_test_rw_lock);
	read_lock(&sec_debug_test_rw_lock);
}

static void simulate_ALLRW_LOCKUP_handler(void *info)
{
	unsigned long flags = 0;

	int cpu = raw_smp_processor_id();

	pr_crit("%s()/cpu:%d\n", __func__, cpu);
	if (cpu % 2)
		read_lock_irqsave(&sec_debug_test_rw_lock, flags);
	else
		write_lock_irqsave(&sec_debug_test_rw_lock, flags);
}

static void simulate_ALLRW_LOCKUP(char **argv, int argc)
{
	unsigned long flags;

	make_all_cpu_online();

	write_lock_irqsave(&sec_debug_test_rw_lock, flags);

	smp_call_function(simulate_ALLRW_LOCKUP_handler, NULL, 0);

	read_lock_irqsave(&sec_debug_test_rw_lock, flags);
}

static void simulate_PC_ABORT(char **argv, int argc)
{
	asm("add x30, x30, #0x1\n\t"
	    "ret");
}

static void simulate_SP_ABORT(char **argv, int argc)
{
	asm("mov x29, #0xff00\n\t"
	    "mov sp, #0xff00\n\t"
	    "ret");
}

static void simulate_JUMP_ZERO(char **argv, int argc)
{
	asm("mov x0, #0x0\n\t"
	    "br x0");
}

static void simulate_BUSMON_ERROR(char **argv, int argc)
{
}

static void simulate_UNALIGNED(char **argv, int argc)
{
	static u8 data[5] __aligned(4) = {1, 2, 3, 4, 5};
	u32 *p;
	u32 val = 0x12345678;
	u32 written;

	p = (u32 *)(data + 1);
	pr_info("%s: p->0x%px\n", __func__, p);

	if (*p == 0)
		val = 0x87654321;
	*p = val;

	written = *(u32 *)&data[0];
	pr_info("%s: data[0]: 0x%08x\n", __func__, written);

	if (written == (u32)((val << 8) | data[0]))
		pr_info("This system may allow unaligned access\n");
}

static void simulate_WRITE_RO(char **argv, int argc)
{
	unsigned long *ptr;

// Write to function addr will triger a warning by JOPP compiler
#ifdef CONFIG_RKP_CFP_JOPP
	/* TODO: __start_rodata is not EXPORTed */
	/* ptr = (unsigned long *)__start_rodata; */
#else
	ptr = (unsigned long *)simulate_WRITE_RO;
#endif
	*ptr ^= 0x12345678;
}

#define BUFFER_SIZE SZ_1K

static int recursive_loop(int remaining)
{
	char buf[BUFFER_SIZE];

	/*sub sp, sp, #(S_FRAME_SIZE+PRESERVE_STACK_SIZE) = 320+256 = 576 @kernel_ventry*/
	if (((unsigned long)(current->stack) + 575) > current_stack_pointer)
		*((volatile unsigned int *)0) = 0;

	/* Make sure compiler does not optimize this away. */
	memset(buf, (remaining & 0xff) | 0x1, BUFFER_SIZE);
	if (!remaining)
		return 0;
	else
		return recursive_loop(remaining - 1);
}

static void simulate_OVERFLOW(char **argv, int argc)
{
	recursive_loop(1000);
}

static void simulate_BAD_SCHED_handler(void *info)
{
	if (is_idle_task(current)) {
		*(int *)info = 1;
		msleep(1000);
	}
}

static void simulate_BAD_SCHED(char **argv, int argc)
{
	int cpu;
	int ret = 0;
	int tries = 0;

	while (true) {
		tries++;
		pr_crit("%dth try.\n", tries);
		for_each_online_cpu(cpu) {
			smp_call_function_single(cpu,
				simulate_BAD_SCHED_handler, &ret, 1);
			if (ret)
				return;	/* success */
		}
		mdelay(100);
	}
}

static void simulate_CORRUPT_MAGIC(char **argv, int argc)
{
	/* TODO: need extra info c */
#if 0
	int magic;

	if (argc) {
		magic = str_to_num(argv[0]);
		simulate_extra_info_force_error(magic);
	} else {
		simulate_extra_info_force_error(0);
	}
#endif
}

static void simulate_IRQ_STORM(char **argv, int argc)
{
	int i;
	long irq;

	if (argc) {
		if (!kstrtol(argv[0], 10, &irq))
			irq_set_irq_type((unsigned int)irq,
					IRQF_TRIGGER_HIGH | IRQF_SHARED);
		else
			pr_crit("%s : wrong irq number (%d)\n", __func__,
						(unsigned int)irq);
	} else {
		for_each_irq_nr(i) {
			struct irq_desc *desc = irq_to_desc(i);

			if (desc && desc->action && desc->action->name)
				if (!strcmp(desc->action->name, "gpio-keys: KEY_VOLUMEDOWN")) {
					irq_set_irq_type(i,
						IRQF_TRIGGER_HIGH | IRQF_SHARED);
					break;
				}
		}
		if (i == nr_irqs)
			pr_crit("%s : irq (gpio-keys: KEY_VOLUMEDOWN) not found\n", __func__);

	}
}

static void dummy_wait_for_completion(void)
{
	DECLARE_COMPLETION_ONSTACK(done);

	wait_for_completion(&done);
}

static irqreturn_t dummy_wait_for_completion_irq_handler(int irq, void *data)
{
	dummy_wait_for_completion();
	return IRQ_HANDLED;
}

static void simulate_SYNC_IRQ_LOCKUP(char **argv, int argc)
{
	int i;
	long irq;

	if (argc) {
		if (!kstrtol(argv[0], 10, &irq)) {
			struct irq_desc *desc = irq_to_desc(irq);

			if (desc && desc->action && desc->action->thread_fn)
				desc->action->thread_fn = dummy_wait_for_completion_irq_handler;
		} else {
			pr_crit("%s : wrong irq number (%d)\n", __func__,
					(unsigned int)irq);
		}
	} else {
		for_each_irq_nr(i) {
			struct irq_desc *desc = irq_to_desc(i);

			if (desc && desc->action &&
				desc->action->name && desc->action->thread_fn)
				if (!strcmp(desc->action->name, "sec_ts")) {
					desc->action->thread_fn = dummy_wait_for_completion_irq_handler;
					break;
				}
		}
		if (i == nr_irqs)
			pr_crit("%s : irq (sec_ts) not found\n", __func__);

	}
}

static void simulate_DISK_SLEEP(char **argv, int argc)
{
	dummy_wait_for_completion();
}

static void secdbg_delay_work(struct work_struct *work)
{
	struct debug_delayed_work_info *info = container_of(work, struct debug_delayed_work_info,
							    read_info_work.work);

	pr_crit("%s info->work_magic : %d\n", __func__, info->work_magic);
}

static void simulate_CORRUPT_DELAYED_WORK(char **argv, int argc)
{
	struct debug_delayed_work_info *info;

	info = kzalloc(sizeof(struct debug_delayed_work_info), GFP_KERNEL);

	pr_crit("%s(): address of info is 0x%p\n", __func__, info);

	if (!info)
		return;

	info->start = true;
	info->work_magic = 0xE055E055;

	INIT_DELAYED_WORK(&info->read_info_work, secdbg_delay_work);
	schedule_delayed_work(&info->read_info_work, msecs_to_jiffies(5000));
	kfree(info);
}

DEFINE_MUTEX(sec_debug_test_mutex_0);
DEFINE_MUTEX(sec_debug_test_mutex_1);

static void test_mutex_aa(struct mutex *lock)
{
	mutex_lock(lock);

	mutex_lock(lock);
}

static void simulate_MUTEX_AA(char **argv, int argc)
{
	int num;

	if (argc)
		num = str_to_num(argv[0]);
	else
		num = 0;

	switch (num % 2) {
	case 1:
		test_mutex_aa(&sec_debug_test_mutex_1);
		break;
	case 0:
	default:
		test_mutex_aa(&sec_debug_test_mutex_0);
		break;
	}

}

struct test_abba {
	struct work_struct work;
	struct mutex a_mutex;
	struct mutex b_mutex;
	struct completion a_ready;
	struct completion b_ready;
};

static void test_abba_work(struct work_struct *work)
{
	struct test_abba *abba = container_of(work, typeof(*abba), work);

	mutex_lock(&abba->b_mutex);

	complete(&abba->b_ready);
	wait_for_completion(&abba->a_ready);

	mutex_lock(&abba->a_mutex);

	pr_err("%s: got 2 mutex\n", __func__);

	mutex_unlock(&abba->a_mutex);
	mutex_unlock(&abba->b_mutex);
}

static void test_mutex_abba(void)
{
	struct test_abba abba;

	mutex_init(&abba.a_mutex);
	mutex_init(&abba.b_mutex);
	INIT_WORK_ONSTACK(&abba.work, test_abba_work);
	init_completion(&abba.a_ready);
	init_completion(&abba.b_ready);

	schedule_work(&abba.work);

	mutex_lock(&abba.a_mutex);

	complete(&abba.a_ready);
	wait_for_completion(&abba.b_ready);

	mutex_lock(&abba.b_mutex);

	pr_err("%s: got 2 mutex\n", __func__);

	mutex_unlock(&abba.b_mutex);
	mutex_unlock(&abba.a_mutex);

	flush_work(&abba.work);
	destroy_work_on_stack(&abba.work);
}

static void simulate_MUTEX_ABBA(char **argv, int argc)
{
	test_mutex_abba();
}

static void simulate_LIST_BUG(char **argv, int argc)
{
	LIST_HEAD(test_head);
	struct list_head node;

	list_add(&node, &test_head);
	list_add(&node, &test_head);

	list_del(&node);
	list_del(&node);
}

#ifdef CONFIG_SEC_DEBUG_FREQ
static void simulate_FREQ_SKEW(char **argv, int argc)
{
	int ret = -1;
	int en = 0;
	int type = 0;
	unsigned long freq = 0;
	const int default_en = 3;
	const int default_type = 3;
	const unsigned long default_freq = 100000;

	if (argc >= 2) {
		ret = kstrtoint(argv[0], 0, &type);
		pr_crit("%s() 1st parameter: %d (ret=%d)\n", __func__, type, ret);
		if (ret == 0) {
			ret = kstrtoul(argv[1], 0, &freq);
			pr_crit("%s() 2nd parameter: %lu (ret=%d)\n", __func__, freq, ret);
		}

		if (argc > 2) {
			ret = kstrtoint(argv[2], 0, &en);
			pr_crit("%s() 3rd parameter: %d (ret=%d)\n", __func__, en, ret);
		} else {
			en = default_en;
		}
	}

	if (ret < 0) {
		pr_err("%s() failed to get args (%d)\n", __func__, ret);
		type = default_type;
		freq = default_freq;
		en = default_en;
	}

	pr_crit("%s() try to type:%d, freq:%lu, en:%d\n", __func__, type, freq, en);
	secdbg_freq_check(type, 0, freq, en);
}
#endif

struct test_resem {
	struct work_struct work;
	struct completion a_ready;
	struct completion b_ready;
};

static DECLARE_RWSEM(secdbg_test_rwsem);
static DEFINE_MUTEX(secdbg_test_mutex_for_rwsem);

static void test_rwsem_read_work(struct work_struct *work)
{
	struct test_resem *t = container_of(work, typeof(*t), work);

	pr_crit("%s: trying read\n", __func__);
	down_read(&secdbg_test_rwsem);

	complete(&t->b_ready);
	wait_for_completion(&t->a_ready);

	mutex_lock(&secdbg_test_mutex_for_rwsem);

	pr_crit("%s: error\n", __func__);

	mutex_unlock(&secdbg_test_mutex_for_rwsem);
	up_read(&secdbg_test_rwsem);

}

static void simulate_RWSEM_R(char **argv, int argc)
{
	struct test_resem twork;

	INIT_WORK_ONSTACK(&twork.work, test_rwsem_read_work);
	init_completion(&twork.a_ready);
	init_completion(&twork.b_ready);

	schedule_work(&twork.work);

	mutex_lock(&secdbg_test_mutex_for_rwsem);

	complete(&twork.a_ready);
	wait_for_completion(&twork.b_ready);

	pr_crit("%s: trying write\n", __func__);
	down_write(&secdbg_test_rwsem);

	pr_crit("%s: error\n", __func__);

	up_write(&secdbg_test_rwsem);
	mutex_unlock(&secdbg_test_mutex_for_rwsem);

}

static void test_rwsem_write_work(struct work_struct *work)
{
	struct test_resem *t = container_of(work, typeof(*t), work);

	pr_crit("%s: trying write\n", __func__);
	down_write(&secdbg_test_rwsem);

	complete(&t->b_ready);
	wait_for_completion(&t->a_ready);

	mutex_lock(&secdbg_test_mutex_for_rwsem);

	pr_crit("%s: error\n", __func__);

	mutex_unlock(&secdbg_test_mutex_for_rwsem);
	up_write(&secdbg_test_rwsem);
}

static void simulate_RWSEM_W(char **argv, int argc)
{
	struct test_resem twork;

	INIT_WORK_ONSTACK(&twork.work, test_rwsem_write_work);
	init_completion(&twork.a_ready);
	init_completion(&twork.b_ready);

	schedule_work(&twork.work);

	mutex_lock(&secdbg_test_mutex_for_rwsem);

	complete(&twork.a_ready);
	wait_for_completion(&twork.b_ready);

	pr_crit("%s: trying read\n", __func__);
	down_read(&secdbg_test_rwsem);

	pr_crit("%s: error\n", __func__);

	up_read(&secdbg_test_rwsem);
	mutex_unlock(&secdbg_test_mutex_for_rwsem);
}

static void simulate_PRINTK_FAULT(char **argv, int argc)
{
	pr_err("%s: trying fault: %s\n", __func__, (char *)0x80000000);
}

static void simulate_EXIN_UNFZ(char **argv, int argc)
{
//	struct task_struct *tsk = current;

	/* TODO: enable this */
//	secdbg_exin_set_unfz(tsk->comm, tsk->pid);
//	secdbg_exin_set_unfz(tsk->parent->comm, tsk->parent->pid);

//	pr_crit("exin unfz tasks: %s\n", secdbg_exin_get_unfz());
}

static int preempt_off;

static void sec_debug_wqlock(struct work_struct *work)
{
	pr_crit("%s\n", __func__);
	if (preempt_off)
		preempt_disable();
	asm("b .");
}

static void simulate_WQLOCK_BUSY_WORKER(char **argv, int argc)
{

	int cpu = 0, ret = 0, idx = 0;

	if (argc) {
		ret = kstrtoint(argv[idx], 0, &preempt_off);
		if (ret)
			goto wakeup;

		preempt_off = preempt_off > 0 ? 1 : 0;

		if (++idx == argc)
			goto wakeup;

		ret = kstrtoint(argv[idx], 0, &cpu);
		if (ret)
			goto wakeup;

		cpu = (cpu < 0 || cpu >= num_possible_cpus()) ? 0 : cpu;
	}

wakeup:
	pr_info("%s preempt_off : %d, bound cpu : %d\n", __func__, preempt_off, cpu);

	INIT_WORK(&lockup_work, sec_debug_wqlock);
	schedule_work_on(cpu, &lockup_work);
}

static int busy_task(void *info)
{
	while (!kthread_should_stop()) {
		if (preempt_off)
			preempt_disable();

		asm("b .");
	}
	return 0;
}

static void simulate_WQLOCK_BUSY_TASK(char **argv, int argc)
{
	int wakeup_order = 0, cpu = 0, prio = 0, ret = 0, idx = 0;
	struct task_struct *tsk;
	static struct sched_param param = { .sched_priority = 0 };

	if (argc) {

		ret = kstrtoint(argv[idx], 0, &preempt_off);
		if (ret)
			goto wakeup;

		preempt_off = preempt_off > 0 ? 1 : 0;

		if (++idx == argc)
			goto wakeup;

		ret = kstrtoint(argv[idx], 0, &wakeup_order);
		if (ret)
			goto wakeup;

		wakeup_order = wakeup_order > 0 ? 1 : 0;

		if (++idx == argc)
			goto wakeup;

		ret = kstrtoint(argv[idx], 0, &cpu);
		if (ret)
			goto wakeup;

		cpu = (cpu < 0 || cpu >= num_possible_cpus()) ? 0 : cpu;

		if (++idx == argc)
			goto wakeup;

		ret = kstrtoint(argv[idx], 0, &prio);
		if (ret)
			goto wakeup;

		prio = (prio < 0 || prio > 140) ? 0 : prio;
	}

wakeup:
	pr_info("%s preempt_off : %d, wakeup_order : %d, bound cpu : %d, prio : %d\n", __func__, preempt_off, wakeup_order, cpu, prio);
	tsk = kthread_create(busy_task, 0, "busy_task");

	if (IS_ERR(tsk)) {
		pr_warn("Failed to create thread by_task\n");
		return;
	}

	if (prio >= 0 && prio < MAX_RT_PRIO) {
		param.sched_priority = (int)prio;
		sched_setscheduler_nocheck(tsk, SCHED_FIFO, &param);
	} else {
		tsk->prio = (int)prio;
		tsk->static_prio = (int)prio;
		tsk->normal_prio = (int)prio;
		sched_setscheduler_nocheck(tsk, SCHED_NORMAL, &param);
	}

	set_cpus_allowed_ptr(tsk, cpumask_of(cpu));
	INIT_WORK(&lockup_work, sec_debug_wqlock);
	if (wakeup_order) {
		wake_up_process(tsk);
		schedule_work_on(cpu, &lockup_work);
	} else {
		schedule_work_on(cpu, &lockup_work);
		msleep(1000);
		wake_up_process(tsk);
	}
}

/* base register for accessing canary in stack */
#define SZ_STACK_FP	SZ_128
#define SZ_STACK_SP	SZ_4

static void secdbg_test_stack_corruption_type0(unsigned long cdata)
{
	volatile unsigned long data_array[SZ_STACK_FP];
	volatile unsigned long *ptarget = (unsigned long *)data_array + SZ_STACK_FP;

	pr_info("%s: cdata: %016lx\n", __func__, cdata);
	pr_info("%s: __stack_chk_guard: %016lx\n", __func__, __stack_chk_guard);
	pr_info("%s: original: [<0x%px>]: %016lx\n", __func__, ptarget, *ptarget);

	*ptarget = cdata;

	pr_info("%s: corrupted: [<0x%px>]: %016lx\n", __func__, ptarget, *ptarget);
}

static void secdbg_test_stack_corruption_type1(unsigned long cdata)
{
	volatile unsigned long data_array[SZ_STACK_SP];
	volatile unsigned long *ptarget = (unsigned long *)data_array + SZ_STACK_SP;

	pr_info("%s: cdata: %016lx\n", __func__, cdata);
	pr_info("%s: __stack_chk_guard: %016lx\n", __func__, __stack_chk_guard);
	pr_info("%s: original: [<0x%px>]: %016lx\n", __func__, ptarget, *ptarget);

	*ptarget = cdata;

	pr_info("%s: corrupted: [<0x%px>]: %016lx\n", __func__, ptarget, *ptarget);
}

/*
 * 1st arg
 *   0: use fp (default)
 *   1: use sp
 * 2nd arg is data pattern for corruption (default value can be used)
 */
static void simulate_STACK_CORRUPTION(char **argv, int argc)
{
	int ret;
	unsigned int type = 0;
	unsigned long cdata = 0xBEEFCAFE01234567;

	if (argc > 0) {
		ret = kstrtouint(argv[0], 0, &type);
		if (ret)
			pr_err("%s: Failed to get first argument\n", __func__);
	}

	if (argc > 1) {
		ret = kstrtoul(argv[1], 0, &cdata);
		if (ret)
			pr_err("%s: Failed to get second argument\n", __func__);
	}

	if (type == 0) {
		pr_info("%s: call x29 fn: %pS\n", __func__, secdbg_test_stack_corruption_type0);
		secdbg_test_stack_corruption_type0(cdata);
	} else {
		pr_info("%s: call sp fn: %pS\n", __func__, secdbg_test_stack_corruption_type1);
		secdbg_test_stack_corruption_type1(cdata);
	}
}

/* for debugging power off */
static int off_nc_registered;

/* {INFORM}   {VALUE}
 * 0x00000000 00000000
 */
static unsigned long off_debug_config_value;

extern void exynos_mach_restart(const char *cmd);

static int sec_debug_pre_power_off_func(struct notifier_block *nb,
					  unsigned long l, void *buf)
{
	int inf_index, inf_value;
	unsigned int inf_type[4] = {0, 0, EXYNOS_PMU_INFORM2, EXYNOS_PMU_INFORM3};

	inf_index = (off_debug_config_value >> 32) & 0xF;
	inf_value = (off_debug_config_value) & 0xFFFFFFFF;

	if ((inf_index < 2) || (3 < inf_index)) {
		pr_crit("%s: not allowed, inform, %lx\n", __func__,
			off_debug_config_value);
	} else {
		pr_crit("%s: set inform%d as %x\n", __func__,
			inf_index, inf_value);

		exynos_pmu_write(inf_type[inf_index], inf_value);
	}

	pr_crit("%s: called, do soft reset\n", __func__);

	exynos_mach_restart("sw reset");

	return 0;
}

static struct notifier_block nb_pre_power_off_block = {
	.notifier_call = sec_debug_pre_power_off_func,
	.priority = INT_MAX,
};

static void simulate_POWER_OFF(char **argv, int argc)
{
	int ret;
	int enable = 0;

	if (argc) {
		enable = str_to_num(argv[0]);
		if (argc == 2)
			ret = kstrtoul(argv[1], 16, &off_debug_config_value);
	}

	if (enable == 1) {
		if (off_nc_registered == 0) {
			pr_crit("%s: off nc is registered\n", __func__);
			atomic_notifier_chain_register(&sec_power_off_notifier_list, &nb_pre_power_off_block);
			off_nc_registered = 1;
		} else {
			pr_crit("%s: off nc is already registered\n", __func__);
		}
	} else {
		if (off_nc_registered) {
			pr_crit("%s: off nc is un-registered\n", __func__);
			atomic_notifier_chain_unregister(&sec_power_off_notifier_list, &nb_pre_power_off_block);
			off_nc_registered = 0;
		} else {
			pr_crit("%s: off nc is NOT registered\n", __func__);
		}
	}
}
/* for debugging power off */

static void simulate_EMERGENT_REBOOT(char **argv, int argc)
{
	pr_crit("%s: restart\n", __func__);

	emergency_restart();
}

static int sec_debug_get_force_error(char *buffer, const struct kernel_param *kp)
{
	int i;
	int size = 0;

	for (i = 0; i < NR_FORCE_ERROR; i++)
		size += scnprintf(buffer + size, PAGE_SIZE - size, "%s\n",
				  force_error_vector.item[i].errname);

	return size;
}

static int sec_debug_set_force_error(const char *val, const struct kernel_param *kp)
{
	int i;
	int argc = 0;
	char **argv;

	argv = argv_split(GFP_KERNEL, val, &argc);

	for (i = 0; i < NR_FORCE_ERROR; i++) {
		if (!strcmp(argv[0], force_error_vector.item[i].errname)) {
			pr_crit("%s() arg : %s\n", __func__, val);
			pr_crit("%ps start\n", force_error_vector.item[i].errfunc);
			force_error_vector.item[i].errfunc(&argv[1], argc - 1);
			break;
		}
	}

	argv_free(argv);

	return 0;
}

static const struct kernel_param_ops sec_debug_force_error_ops = {
		.set	= sec_debug_set_force_error,
		.get	= sec_debug_get_force_error,
};

module_param_cb(force_error, &sec_debug_force_error_ops, NULL, 0600);

MODULE_DESCRIPTION("Samsung Debug Test driver");
MODULE_LICENSE("GPL v2");
