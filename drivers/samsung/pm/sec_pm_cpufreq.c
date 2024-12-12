/* drivers/samsung/pm/sec_pm_cpufreq.c
 *
 * Copyright (c) 2020 Samsung Electronics Co., Ltd.
 * Author: Minsung Kim <ms925.kim@samsung.com>
 *
 * This software is licensed under the terms of the GNU General Public
 * License version 2, as published by the Free Software Foundation, and
 * may be copied, distributed, and modified under those terms.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 */

#include <linux/module.h>
#include <linux/slab.h>
#include <linux/pm_qos.h>
#include <linux/sec_pm_cpufreq.h>
#include <soc/samsung/freq-qos-tracer.h>

static DEFINE_MUTEX(cpufreq_list_lock);
static LIST_HEAD(sec_pm_cpufreq_list);

static unsigned long throttle_count;

static int sec_pm_cpufreq_set_max_freq(struct sec_pm_cpufreq_dev *cpufreq_dev,
				 unsigned int level)
{
	if (WARN_ON(level > cpufreq_dev->max_level))
		return -EINVAL;

	if (cpufreq_dev->cur_level == level)
		return 0;

	cpufreq_dev->max_freq = cpufreq_dev->freq_table[level].frequency;
	cpufreq_dev->cur_level = level;

	pr_info("%s: throttle cpu%d : %u KHz\n", __func__,
			cpufreq_dev->policy->cpu, cpufreq_dev->max_freq);

	return freq_qos_update_request(&cpufreq_dev->qos_req,
				cpufreq_dev->freq_table[level].frequency);
}

static unsigned long get_level(struct sec_pm_cpufreq_dev *cpufreq_dev,
			       unsigned long freq)
{
	struct cpufreq_frequency_table *freq_table = cpufreq_dev->freq_table;
	unsigned long level;

	for (level = 1; level <= cpufreq_dev->max_level; level++)
		if (freq >= freq_table[level].frequency)
			break;

	return level;
}

/* For SMPL_WARN interrupt */
int sec_pm_cpufreq_throttle_by_one_step(void)
{
	struct sec_pm_cpufreq_dev *cpufreq_dev;
	unsigned long level;
	unsigned long freq;

	++throttle_count;

	list_for_each_entry(cpufreq_dev, &sec_pm_cpufreq_list, node) {
		if (!cpufreq_dev->policy || !cpufreq_dev->freq_table) {
			pr_warn("%s: No cpufreq_dev\n", __func__);
			continue;
		}

		/* Skip LITTLE cluster */
		if (!cpufreq_dev->policy->cpu)
			continue;

		freq = cpufreq_dev->freq_table[0].frequency / 2;
		level = get_level(cpufreq_dev, freq);
		level += throttle_count;

		if (level > cpufreq_dev->max_level)
			level = cpufreq_dev->max_level;

		sec_pm_cpufreq_set_max_freq(cpufreq_dev, level);
	}

	return throttle_count;
}
EXPORT_SYMBOL_GPL(sec_pm_cpufreq_throttle_by_one_step);

void sec_pm_cpufreq_unthrottle(void)
{
	struct sec_pm_cpufreq_dev *cpufreq_dev;

	pr_info("%s: throttle_count: %lu\n", __func__, throttle_count);

	if (!throttle_count)
		return;

	throttle_count = 0;

	list_for_each_entry(cpufreq_dev, &sec_pm_cpufreq_list, node)
		sec_pm_cpufreq_set_max_freq(cpufreq_dev, 0);
}
EXPORT_SYMBOL_GPL(sec_pm_cpufreq_unthrottle);

static unsigned int find_next_max(struct cpufreq_frequency_table *table,
				  unsigned int prev_max)
{
	struct cpufreq_frequency_table *pos;
	unsigned int max = 0;

	cpufreq_for_each_valid_entry(pos, table) {
		if (pos->frequency > max && pos->frequency < prev_max)
			max = pos->frequency;
	}

	return max;
}

static struct sec_pm_cpufreq_dev *
__sec_pm_cpufreq_register(struct cpufreq_policy *policy)
{
	struct sec_pm_cpufreq_dev *cpufreq_dev;
	int ret;
	unsigned int freq, i;

	if (IS_ERR_OR_NULL(policy)) {
		pr_err("%s: cpufreq policy isn't valid: %pK\n", __func__, policy);
		return ERR_PTR(-EINVAL);
	}

	i = cpufreq_table_count_valid_entries(policy);
	if (!i) {
		pr_err("%s: CPUFreq table not found or has no valid entries\n",
			 __func__);
		return ERR_PTR(-ENODEV);
	}

	cpufreq_dev = kzalloc(sizeof(*cpufreq_dev), GFP_KERNEL);
	if (!cpufreq_dev)
		return ERR_PTR(-ENOMEM);

	cpufreq_dev->policy = policy;
	cpufreq_dev->max_level = i - 1;

	cpufreq_dev->freq_table = kmalloc_array(i,
					sizeof(*cpufreq_dev->freq_table),
					GFP_KERNEL);
	if (!cpufreq_dev->freq_table) {
		pr_err("%s: fail to allocate freq_table\n", __func__);
		ret = -ENOMEM;
		goto free_cpufreq_dev;
	}

	/* Fill freq-table in descending order of frequencies */
	for (i = 0, freq = -1; i <= cpufreq_dev->max_level; i++) {
		freq = find_next_max(policy->freq_table, freq);
		cpufreq_dev->freq_table[i].frequency = freq;

		/* Warn for duplicate entries */
		if (!freq)
			pr_warn("%s: table has duplicate entries\n", __func__);
		else
			pr_debug("%s: freq:%u KHz\n", __func__, freq);
	}

	cpufreq_dev->max_freq = cpufreq_dev->freq_table[0].frequency;

	ret = freq_qos_tracer_add_request(&policy->constraints,
				   &cpufreq_dev->qos_req, FREQ_QOS_MAX,
				   cpufreq_dev->freq_table[0].frequency);
	if (ret < 0) {
		pr_err("%s: Failed to add freq constraint (%d)\n", __func__,
		       ret);
		goto free_table;
	}

	mutex_lock(&cpufreq_list_lock);
	list_add(&cpufreq_dev->node, &sec_pm_cpufreq_list);
	mutex_unlock(&cpufreq_list_lock);

	return cpufreq_dev;

free_table:
	kfree(cpufreq_dev->freq_table);
free_cpufreq_dev:
	kfree(cpufreq_dev);

	return ERR_PTR(ret);
}

struct sec_pm_cpufreq_dev *
sec_pm_cpufreq_register(struct cpufreq_policy *policy)
{
	pr_info("%s\n", __func__);

	return __sec_pm_cpufreq_register(policy);
}
EXPORT_SYMBOL_GPL(sec_pm_cpufreq_register);

void sec_pm_cpufreq_unregister(struct sec_pm_cpufreq_dev *cpufreq_dev)
{
	pr_info("%s\n", __func__);

	if (!cpufreq_dev)
		return;

	mutex_lock(&cpufreq_list_lock);
	list_del(&cpufreq_dev->node);
	mutex_unlock(&cpufreq_list_lock);

	freq_qos_tracer_remove_request(&cpufreq_dev->qos_req);

	kfree(cpufreq_dev->freq_table);
	kfree(cpufreq_dev);
}
EXPORT_SYMBOL_GPL(sec_pm_cpufreq_unregister);

MODULE_AUTHOR("Minsung Kim <ms925.kim@samsung.com>");
MODULE_DESCRIPTION("SEC PM CPU Frequency Control");
MODULE_LICENSE("GPL");
