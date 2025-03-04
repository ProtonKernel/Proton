/*
 * Copyright (C) 2012-2019, Samsung Electronics Co., Ltd.
 *
 * This software is licensed under the terms of the GNU General Public
 * License version 2, as published by the Free Software Foundation, and
 * may be copied, distributed, and modified under those terms.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */

#include <linux/cpufreq.h>
#include <linux/module.h>
#include <linux/notifier.h>
#include "core/iwservice.h"
#include "core/log.h"
#include "core/subsystem.h"

static void cpufreq_trans_postchange_action(struct cpufreq_freqs *freqs)
{
#if LINUX_VERSION_CODE >= KERNEL_VERSION(5, 2, 0)
	unsigned int cpu;

	log_info(tzdev_cpufreq, "post-transition: cpumask %*pb, old %u, new %u\n",
			cpumask_pr_args(freqs->policy->cpus), freqs->old, freqs->new);

	for_each_cpu(cpu, freqs->policy->cpus)
		tz_iwservice_set_cpu_freq(cpu, freqs->new);
#else
	log_info(tzdev_cpufreq, "post-transition: cpu %u old %u, new %u\n",
			freqs->cpu, freqs->old, freqs->new);
	tz_iwservice_set_cpu_freq(freqs->cpu, freqs->new);
#endif
}

static int cpufreq_trans_callback(struct notifier_block *nb,
				unsigned long event, void *data)
{
	struct cpufreq_freqs *freqs = data;

	log_info(tzdev_cpufreq, "transition callback event %lu\n", event);
	switch (event) {
	case CPUFREQ_POSTCHANGE:
		cpufreq_trans_postchange_action(freqs);
		break;
	}

	return NOTIFY_OK;
}

static struct notifier_block cpufreq_trans_notifier = {
	.notifier_call = cpufreq_trans_callback,
};

int cpufreq_trans_init(void)
{
	return cpufreq_register_notifier(&cpufreq_trans_notifier,
		CPUFREQ_TRANSITION_NOTIFIER);
}

tzdev_late_initcall(cpufreq_trans_init);
