// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2020 Samsung Electronics Co., Ltd.
 *	      http://www.samsung.com/
 *
 * Exynos - Hardlockup Watchdog Detector
 *
 * Author: Hosung Kim <hosung0.kim@samsung.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#include <linux/platform_device.h>
#include <linux/of.h>
#include <linux/of_device.h>
#include <linux/module.h>
#include <linux/mm.h>
#include <linux/cpu.h>
#include <linux/cpuhotplug.h>
#include <linux/suspend.h>
#include <linux/sched/clock.h>
#include <linux/preempt.h>
#include <uapi/linux/sched/types.h>

struct hardlockup_watchdog_pcpu {
	unsigned long hardlockup_touch_ts;
	struct hrtimer hrtimer;
	unsigned long hrtimer_interrupts;
	unsigned long hrtimer_interrupts_saved;
	bool hard_watchdog_warn;
};

static struct hardlockup_watchdog_pcpu __percpu *hardlockup_watchdog_pcpu;

struct hardlockup_watchdog_desc {
	struct mutex mutex;
	unsigned long enabled;
	struct cpumask allowed_mask;
	u64 sample_period;
	unsigned long thresh;
	int watchdog_thresh;
	unsigned int panic;
};

struct hardlockup_watchdog_dev {
	struct device *dev;
	struct hardlockup_watchdog_desc *desc;
	struct of_device_id *match;
};

static struct hardlockup_watchdog_desc hardlockup_watchdog = {
	.enabled		= true,
	.watchdog_thresh	= 10,
	.panic			= true,
};

static const struct of_device_id hardlockup_watchdog_dt_match[] = {
	{.compatible = "samsung,hardlockup-watchdog",
	 .data = NULL,},
	{},
};
MODULE_DEVICE_TABLE(of, hardlockup_watchdog_dt_match);

ATOMIC_NOTIFIER_HEAD(hardlockup_notifier_list);
EXPORT_SYMBOL(hardlockup_notifier_list);

ATOMIC_NOTIFIER_HEAD(hardlockup_handler_notifier_list);
EXPORT_SYMBOL(hardlockup_handler_notifier_list);

u64 hardlockup_watchdog_get_period(void)
{
	return hardlockup_watchdog.sample_period;
}
EXPORT_SYMBOL(hardlockup_watchdog_get_period);

unsigned long hardlockup_watchdog_get_thresh(void)
{
	return hardlockup_watchdog.thresh;
}
EXPORT_SYMBOL(hardlockup_watchdog_get_thresh);

/*
 * Hard-lockup warnings should be triggered after just a few seconds. Soft-
 * lockups can have false positives under extreme conditions. So we generally
 * want a higher threshold for soft lockups than for hard lockups. So we couple
 * the thresholds with a factor: we make the soft threshold twice the amount of
 * time the hard threshold is.
 */
static int get_hardlockup_watchdog_thresh(void)
{
	return hardlockup_watchdog.watchdog_thresh * 2;
}

/*
 * Returns seconds, approximately.  We don't need nanosecond
 * resolution, and we don't need to waste time with a big divide when
 * 2^30ns == 1.074s.
 */
static unsigned long get_timestamp(void)
{
	return local_clock() >> 30;
}

static void set_hardlockup_watchdog_sample_period(void)
{
	/*
	 * convert hardlockup_watchdog.thresh from seconds to ns
	 * the divide by 5 is to give hrtimer several chances (two
	 * or three with the current relation between the soft
	 * and hard thresholds) to increment before the
	 * hardlockup detector generates a warning
	 */
	hardlockup_watchdog.sample_period =
		get_hardlockup_watchdog_thresh() * ((u64)NSEC_PER_SEC / 5);
	hardlockup_watchdog.thresh =
		hardlockup_watchdog.sample_period * 3 / NSEC_PER_SEC;
}

/* Commands for resetting the watchdog */
static void __touch_hardlockup_watchdog(void)
{
	struct hardlockup_watchdog_pcpu *pcpu_val =
				this_cpu_ptr(hardlockup_watchdog_pcpu);

	if (pcpu_val)
		pcpu_val->hardlockup_touch_ts = get_timestamp();
}

static void watchdog_interrupt_count(void)
{
	struct hardlockup_watchdog_pcpu *pcpu_val =
				this_cpu_ptr(hardlockup_watchdog_pcpu);

	if (pcpu_val)
		pcpu_val->hrtimer_interrupts++;
}

static unsigned int watchdog_next_cpu(unsigned int cpu)
{
	unsigned int next_cpu;

	next_cpu = cpumask_next(cpu, &hardlockup_watchdog.allowed_mask);
	if (next_cpu >= nr_cpu_ids)
		next_cpu = cpumask_first(&hardlockup_watchdog.allowed_mask);

	if (next_cpu == cpu)
		return nr_cpu_ids;

	return next_cpu;
}

static int is_hardlockup_other_cpu(unsigned int cpu)
{
	struct hardlockup_watchdog_pcpu *pcpu_val =
				per_cpu_ptr(hardlockup_watchdog_pcpu, cpu);

	unsigned long hrint;

	if (!pcpu_val)
		goto out;

	hrint = pcpu_val->hrtimer_interrupts;

	if (pcpu_val->hrtimer_interrupts_saved == hrint) {
		unsigned long now = get_timestamp();
		unsigned long touch_ts = pcpu_val->hardlockup_touch_ts;

		if (time_after(now, touch_ts) &&
			(now - touch_ts >= hardlockup_watchdog.thresh)) {
			pr_err("hardlockup-watchdog: cpu%x: hrint:%lu "
				"thresh:%llu, now:%llu, touch_ts:%llu\n",
				cpu, hrint, hardlockup_watchdog.thresh,
				now, touch_ts);

			return 1;
		}
	}
	pcpu_val->hrtimer_interrupts_saved = hrint;
out:
	return 0;
}

static void watchdog_check_hardlockup_other_cpu(void)
{
	struct hardlockup_watchdog_pcpu *pcpu_val =
				this_cpu_ptr(hardlockup_watchdog_pcpu);
	struct hardlockup_watchdog_pcpu *pcpu_val_next;
	unsigned int next_cpu;

	/*
	 * Test for hardlockups every 3 samples.  The sample period is
	 *  hardlockup_watchdog.thresh * 2 / 5, so 3 samples gets us back to slightly over
	 *  hardlockup_watchdog.thresh (over by 20%).
	 */
	if (!pcpu_val)
		return;

	if ((pcpu_val->hrtimer_interrupts) % 3 != 0)
		return;

	/* check for a hardlockup on the next cpu */
	next_cpu = watchdog_next_cpu(smp_processor_id());
	if (next_cpu >= nr_cpu_ids)
		return;

	smp_rmb();

	pcpu_val_next = per_cpu_ptr(hardlockup_watchdog_pcpu, next_cpu);
	if (is_hardlockup_other_cpu(next_cpu)) {
		/* only warn once */
		if (pcpu_val_next->hard_watchdog_warn == true)
			return;

		if (hardlockup_watchdog.panic) {
			atomic_notifier_call_chain(&hardlockup_notifier_list, 0, (void *)&next_cpu);
			panic("Watchdog detected hard LOCKUP on cpu %u", next_cpu);
		} else {
			WARN(1, "Watchdog detected hard LOCKUP on cpu %u", next_cpu);
		}

		pcpu_val_next->hard_watchdog_warn = true;
	} else {
		pcpu_val_next->hard_watchdog_warn = false;
	}
}

/* watchdog kicker functions */
static enum hrtimer_restart hardlockup_watchdog_fn(struct hrtimer *hrtimer)
{
	int cpu = raw_smp_processor_id();

	if (!hardlockup_watchdog.enabled)
		return HRTIMER_NORESTART;

	/* kick the hardlockup detector */
	watchdog_interrupt_count();

	/* test for hardlockups on the next cpu */
	watchdog_check_hardlockup_other_cpu();

	__touch_hardlockup_watchdog();

	atomic_notifier_call_chain(&hardlockup_handler_notifier_list, 0, (void *)&cpu);

	/* .. and repeat */
	hrtimer_forward_now(hrtimer, ns_to_ktime(hardlockup_watchdog.sample_period));

	return HRTIMER_RESTART;
}

static void hardlockup_watchdog_enable(unsigned int cpu)
{
	struct hardlockup_watchdog_pcpu *pcpu_val =
				per_cpu_ptr(hardlockup_watchdog_pcpu, cpu);
	struct hrtimer *hrtimer;

	if (!pcpu_val)
		return;

	cpumask_set_cpu(cpu, &hardlockup_watchdog.allowed_mask);
	hrtimer = &pcpu_val->hrtimer;

	hrtimer_init(hrtimer, CLOCK_MONOTONIC, HRTIMER_MODE_REL);
	hrtimer->function = hardlockup_watchdog_fn;
	hrtimer_start(hrtimer, ns_to_ktime(hardlockup_watchdog.sample_period),
		      HRTIMER_MODE_REL_PINNED);

	/* Initialize timestamp */
	__touch_hardlockup_watchdog();

	pr_info("%s: cpu%x: enabled - interval: %llu sec\n", __func__, cpu,
			hardlockup_watchdog.sample_period / NSEC_PER_SEC);
}

static void hardlockup_watchdog_disable(unsigned int cpu)
{
	struct hardlockup_watchdog_pcpu *pcpu_val =
				per_cpu_ptr(hardlockup_watchdog_pcpu, cpu);
	struct hrtimer *hrtimer;

	if (!pcpu_val)
		return;

	hrtimer = &pcpu_val->hrtimer;

	WARN_ON_ONCE(cpu != smp_processor_id());

	pr_info("%s: cpu%x: disabled\n", __func__, cpu);

	cpumask_clear_cpu(cpu, &hardlockup_watchdog.allowed_mask);
	hrtimer_cancel(hrtimer);
}

static int hardlockup_stop_fn(void *data)
{
	hardlockup_watchdog_disable(smp_processor_id());
	return 0;
}

static void hardlockup_stop_all(void)
{
	int cpu;

	for_each_cpu(cpu, &hardlockup_watchdog.allowed_mask)
		smp_call_on_cpu(cpu, hardlockup_stop_fn, NULL, false);

	cpumask_clear(&hardlockup_watchdog.allowed_mask);
}

static int hardlockup_start_fn(void *data)
{
	hardlockup_watchdog_enable(smp_processor_id());
	return 0;
}

static void hardlockup_start_all(void)
{
	int cpu;

	for_each_online_cpu(cpu)
			smp_call_on_cpu(cpu, hardlockup_start_fn, NULL, false);
}

static int hardlockup_watchdog_online_cpu(unsigned int cpu)
{
	if (!hardlockup_watchdog.sample_period)
		goto out;

	smp_call_on_cpu(cpu, hardlockup_start_fn, NULL, false);
out:
	return 0;
}

static int hardlockup_watchdog_offline_cpu(unsigned int cpu)
{
	if (!cpumask_test_cpu(cpu, &hardlockup_watchdog.allowed_mask))
		goto out;

	smp_call_on_cpu(cpu, hardlockup_stop_fn, NULL, false);
out:
	return 0;
}

static void __hardlockup_watchdog_cleanup(void)
{
	lockdep_assert_held(&hardlockup_watchdog.mutex);
}

static void hardlockup_watchdog_reconfigure(void)
{
	cpus_read_lock();
	hardlockup_stop_all();
	set_hardlockup_watchdog_sample_period();
	if (hardlockup_watchdog.enabled)
		hardlockup_start_all();

	cpus_read_unlock();
	/*
	 * Must be called outside the cpus locked section to prevent
	 * recursive locking in the perf code.
	 */
	__hardlockup_watchdog_cleanup();
}

static void hardlockup_watchdog_setup(void)
{
	int err;

	if (!(hardlockup_watchdog.enabled))
		return;

	mutex_init(&hardlockup_watchdog.mutex);
	hardlockup_watchdog_pcpu =
			alloc_percpu(struct hardlockup_watchdog_pcpu);

	if (!hardlockup_watchdog_pcpu) {
		pr_err("%s: alloc_percpu is failed\n", __func__);
		return;
	}

	err = cpuhp_setup_state(CPUHP_AP_ONLINE_DYN,
			"hardlockup:online",
			hardlockup_watchdog_online_cpu,
			hardlockup_watchdog_offline_cpu);

	if (err < 0) {
		pr_err("%s: cpuhotplug register is failed - err:%d\n", __func__, err);
		return;
	}

	mutex_lock(&hardlockup_watchdog.mutex);
	hardlockup_watchdog_reconfigure();
	mutex_unlock(&hardlockup_watchdog.mutex);
}

static void hardlockup_watchdog_cleanup(void)
{
	mutex_lock(&hardlockup_watchdog.mutex);
	hardlockup_stop_all();
	__hardlockup_watchdog_cleanup();
	mutex_unlock(&hardlockup_watchdog.mutex);
}

static int hardlockup_watchdog_probe(struct platform_device *pdev)
{
	struct hardlockup_watchdog_dev *watchdog;

	watchdog = devm_kzalloc(&pdev->dev,
			sizeof(struct hardlockup_watchdog_dev), GFP_KERNEL);
	if (!watchdog)
		return -ENOMEM;

	watchdog->dev = &pdev->dev;
	watchdog->desc = &hardlockup_watchdog;

	platform_set_drvdata(pdev, watchdog);
	hardlockup_watchdog_setup();

	dev_info(&pdev->dev, "Hardlockup Watchdog driver\n");

	return 0;
}

static int hardlockup_watchdog_remove(struct platform_device *pdev)
{
	platform_set_drvdata(pdev, NULL);
	hardlockup_watchdog_cleanup();
	free_percpu(hardlockup_watchdog_pcpu);
	return 0;
}

#ifdef CONFIG_PM_SLEEP
static int hardlockup_watchdog_suspend(struct device *dev)
{
	hardlockup_watchdog_offline_cpu(0);
	return 0;
}

static int hardlockup_watchdog_resume(struct device *dev)
{
	hardlockup_watchdog_online_cpu(0);
	return 0;
}

static SIMPLE_DEV_PM_OPS(hardlockup_watchdog_pm_ops, hardlockup_watchdog_suspend, hardlockup_watchdog_resume);
#define HARDLOCKUP_WATCHDOG_PM	(hardlockup_watchdog_pm_ops)
#endif

static struct platform_driver hardlockup_watchdog_driver = {
	.probe = hardlockup_watchdog_probe,
	.remove = hardlockup_watchdog_remove,
	.driver = {
		   .name = "hardlockup-watchdog",
		   .of_match_table = hardlockup_watchdog_dt_match,
		   .pm = &hardlockup_watchdog_pm_ops,
		   },
};
module_platform_driver(hardlockup_watchdog_driver);

MODULE_DESCRIPTION("Samsung Exynos HardLockup Watchdog Driver");
MODULE_LICENSE("GPL v2");
MODULE_AUTHOR("Hosung Kim <hosung0.kim@samsung.com>");
