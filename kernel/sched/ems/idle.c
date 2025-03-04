/*
 * sched idle for Exynos Mobile Scheduler
 *
 * Copyright (C) 2020 Samsung Electronics Co., Ltd
 * Park Bumgyu <bumgyu.park@samsung.com>
 */

#include <linux/init.h>
#include <linux/ktime.h>
#include <linux/cpu_pm.h>
#include <asm/io.h>

static void __iomem *cpu_net_base;

extern ktime_t tick_nohz_get_next_event(int cpu);

#define CPU_NET_BASE		(0xBFFFF500)
#define CPU_NET_OFFSET(i)	(i * 0x8)

static int cpu_pm_notify_callback(struct notifier_block *self,
		unsigned long action, void *v)
{
	int cpu = smp_processor_id();

	if (action != CPU_PM_ENTER)
		return NOTIFY_OK;

	writeq_relaxed(tick_nohz_get_next_event(cpu),
			cpu_net_base + CPU_NET_OFFSET(cpu));

	return NOTIFY_OK;
}

static struct notifier_block cpu_pm_notifier = {
	.notifier_call = cpu_pm_notify_callback,
};

static int idle_init(void)
{
	int ret;

	cpu_net_base = ioremap(CPU_NET_BASE, SZ_4K);

	ret = cpu_pm_register_notifier(&cpu_pm_notifier);
	if (ret)
		return ret;

	return 0;
}
core_initcall(idle_init)
