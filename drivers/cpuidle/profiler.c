/*
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * CPU idle profiler.
 *
 * Copyright (C) 2020 Samsung Electronics Co., Ltd
 * Author : Park Bumgyu <bumgyu.park@samsung.com>
 */

#include <linux/cpuidle.h>
#include <linux/device.h>
#include <linux/ktime.h>
#include <linux/sysfs.h>
#include <linux/kdev_t.h>

/* cpuidle initialization time */
static ktime_t cpuidle_init_time;

/* idle state count */
static int state_count;

/* cpuidle profile init flag */
static int profile_initialized;

struct cpuidle_state_stats {
	/* count of idle entry */
	unsigned int entry_count;

	/* count of idle entry cancllations */
	unsigned int cancel_count;

	/* idle residency time */
	s64 residency_time;
};

struct cpuidle_stats {
	/* time entered idle */
	ktime_t	entry_time;

	/* entered idle state index */
	int entry_state_index;

	/* idle state stats */
	struct cpuidle_state_stats css[CPUIDLE_STATE_MAX];
};

static struct cpuidle_stats __percpu *stats;

void cpuidle_profile_idle_begin(int idx)
{
	struct cpuidle_stats *stat;

	if (!profile_initialized)
		return;

	stat = this_cpu_ptr(stats);

	stat->entry_time = ktime_get();
	stat->entry_state_index = idx;

	stat->css[idx].entry_count++;
}

void cpuidle_profile_idle_end(int cancel)
{
	struct cpuidle_stats *stat;
	int idx;

	if (!profile_initialized)
		return;

	stat = this_cpu_ptr(stats);
	idx = stat->entry_state_index;

	if (!stat->entry_time)
		return;

	if (cancel) {
		stat->css[idx].cancel_count++;
		return;
	}

	stat->css[idx].residency_time +=
		ktime_to_us(ktime_sub(ktime_get(), stat->entry_time));

	stat->entry_time = 0;
}

static int profiling;

static struct cpuidle_state_stats __percpu *profile_css;
static ktime_t profile_time;

static ssize_t show_profile(struct device *dev,
				struct device_attribute *attr,
				char *buf)
{
	int cpu, i, ret = 0;
	struct cpuidle_state_stats *css;
	s64 total;

	if (profiling)
		return snprintf(buf, PAGE_SIZE, "Profile is ongoing\n");

	ret += snprintf(buf + ret, PAGE_SIZE - ret,
			"format : [cpu] [entry_count] [cancel_count] [time] [(ratio)]\n\n");

	total = ktime_to_us(profile_time);
	for (i = 0; i < state_count; i++) {
		ret += snprintf(buf + ret, PAGE_SIZE - ret, "[state%d]\n", i);
		for_each_possible_cpu(cpu) {
			css = per_cpu_ptr(profile_css, cpu);
			ret += snprintf(buf + ret, PAGE_SIZE - ret,
				"cpu%d %d %d %lld (%d%%)\n",
				cpu,
				css[i].entry_count,
				css[i].cancel_count,
				css[i].residency_time,
				css[i].residency_time * 100 / total);
		}
		ret += snprintf(buf + ret, PAGE_SIZE - ret, "\n");
	}

	ret += snprintf(buf + ret, PAGE_SIZE - ret, "(total %lldus)\n", total);

	return ret;
}

static void do_nothing(void *unused) { }
static ssize_t store_profile(struct device *dev,
				struct device_attribute *attr,
				const char *buf, size_t count)
{
	int input, cpu, i;

	if (!sscanf(buf, "%d", &input))
		return -EINVAL;

	input = !!input;
	if (profiling == input)
		return count;

	profiling = input;

	if (!input)
		goto stop_profile;

	preempt_disable();
	smp_call_function(do_nothing, NULL, 1);
	preempt_enable();

	for_each_possible_cpu(cpu) {
		memcpy(per_cpu_ptr(profile_css, cpu),
			per_cpu_ptr(stats, cpu)->css,
			sizeof(struct cpuidle_state_stats) * CPUIDLE_STATE_MAX);
	}
	profile_time = ktime_get();

	return count;

stop_profile:
#define delta(a, b)	(a = b - a)
#define field_delta(cpu, idx, field)			\
	delta(per_cpu_ptr(profile_css, cpu)[idx].field,	\
		per_cpu_ptr(stats, cpu)->css[idx].field);

	preempt_disable();
	smp_call_function(do_nothing, NULL, 1);
	preempt_enable();

	for_each_possible_cpu(cpu) {
		for (i = 0; i < state_count; i++) {
			field_delta(cpu, i, entry_count);
			field_delta(cpu, i, cancel_count);
			field_delta(cpu, i, residency_time);
		}
	}
	profile_time = ktime_sub(ktime_get(), profile_time);

	return count;
}

static DEVICE_ATTR(profile, 0644, show_profile, store_profile);

static ssize_t show_time_in_state(struct device *dev,
				struct device_attribute *attr,
				char *buf)
{
	int cpu, i, ret = 0;
	struct cpuidle_stats *stat;
	s64 total = ktime_to_us(ktime_sub(ktime_get(), cpuidle_init_time));

	ret += snprintf(buf + ret, PAGE_SIZE - ret,
			"format : [cpu] [entry_count] [cancel_count] [time] [(ratio)]\n\n");
	for (i = 0; i < state_count; i++) {
		ret += snprintf(buf + ret, PAGE_SIZE - ret, "[state%d]\n", i);
		for_each_possible_cpu(cpu) {
			stat = per_cpu_ptr(stats, cpu);
			ret += snprintf(buf + ret, PAGE_SIZE - ret,
				"cpu%d %d %d %lld (%d%%)\n",
				cpu,
				stat->css[i].entry_count,
				stat->css[i].cancel_count,
				stat->css[i].residency_time,
				stat->css[i].residency_time * 100 / total);
		}
		ret += snprintf(buf + ret, PAGE_SIZE - ret, "\n");
	}

	ret += snprintf(buf + ret, PAGE_SIZE - ret, "(total %lldus)\n", total);

	return ret;
}

static DEVICE_ATTR(time_in_state, 0444, show_time_in_state, NULL);

static struct attribute *cpuidle_profile_attrs[] = {
	&dev_attr_time_in_state.attr,
	&dev_attr_profile.attr,
	NULL,
};

static const struct attribute_group cpuidle_profile_group = {
	.attrs = cpuidle_profile_attrs,
};

int __init cpuidle_profile_init(void)
{
	struct class *idle_class;
	struct device *dev;
	struct cpuidle_driver *drv;
	int ret;

	stats = alloc_percpu(struct cpuidle_stats);
	profile_css = __alloc_percpu(sizeof(struct cpuidle_state_stats)
				* CPUIDLE_STATE_MAX,
				__alignof__(struct cpuidle_state_stats));

	cpuidle_init_time = ktime_get();

	drv = cpuidle_get_cpu_driver(per_cpu(cpuidle_devices, 0));
	state_count = drv->state_count;

	profile_initialized = 1;

	idle_class = class_create(THIS_MODULE, "cpuidle");
	dev = device_create(idle_class, NULL, MKDEV(0, 0), NULL, "cpuidle_profiler");
	ret = sysfs_create_group(&dev->kobj, &cpuidle_profile_group);
	if (ret)
		pr_err("%s: failed to create sysfs group\n", __func__);

	return ret;
}
