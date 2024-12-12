/*
 * Copyright (c) 2016 Park Bumgyu, Samsung Electronics Co., Ltd <bumgyu.park@samsung.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * Exynos ACME(A Cpufreq that Meets Every chipset) driver implementation
 */

#define pr_fmt(fmt)	KBUILD_MODNAME ": " fmt

#include <linux/init.h>
#include <linux/of.h>
#include <linux/slab.h>
#include <linux/cpu.h>
#include <linux/cpumask.h>
#include <linux/cpufreq.h>
#include <linux/tick.h>
#include <linux/pm_opp.h>
#include <soc/samsung/cpu_cooling.h>
#include <linux/suspend.h>
#include <linux/platform_device.h>
#include <linux/sec_pm_cpufreq.h>

#include <soc/samsung/debug-snapshot.h>
#include <soc/samsung/cal-if.h>
#include <soc/samsung/ect_parser.h>
#include <soc/samsung/exynos-cpupm.h>
#include <soc/samsung/freq-qos-tracer.h>
#include <soc/samsung/exynos-devfreq.h>
#include <soc/samsung/exynos-dm.h>
#include <soc/samsung/exynos-devfreq.h>
#if IS_ENABLED(CONFIG_ARM_EXYNOS_ACME_DISABLE_BOOT_LOCK)
#include <linux/kq/kq_nad.h>
#endif
#include "exynos-acme.h"

/*
 * list head of cpufreq domain
 */
static LIST_HEAD(domains);

/*
 * list head of units which have cpufreq policy dependancy
 */
static LIST_HEAD(ready_list);

/*********************************************************************
 *                          HELPER FUNCTION                          *
 *********************************************************************/
static struct exynos_cpufreq_domain *find_domain(unsigned int cpu)
{
	struct exynos_cpufreq_domain *domain;

	list_for_each_entry(domain, &domains, list)
		if (cpumask_test_cpu(cpu, &domain->cpus))
			return domain;

	pr_err("cannot find cpufreq domain by cpu\n");
	return NULL;
}

static void enable_domain(struct exynos_cpufreq_domain *domain)
{
	mutex_lock(&domain->lock);
	domain->enabled = true;
	mutex_unlock(&domain->lock);
}

static void disable_domain(struct exynos_cpufreq_domain *domain)
{
	mutex_lock(&domain->lock);
	domain->enabled = false;
	mutex_unlock(&domain->lock);
}

static void update_dm_min_max(struct exynos_cpufreq_domain *domain)
{
	policy_update_call_to_DM(domain->dm_type,
			domain->min_freq_qos,
			min(domain->max_freq_qos, domain->clipped_freq));
}

/*********************************************************************
 *                   PRE/POST HANDLING FOR SCALING                   *
 *********************************************************************/
static int pre_scale(struct cpufreq_freqs *freqs)
{
	return 0;
}

static int post_scale(struct cpufreq_freqs *freqs)
{
	return 0;
}

/*********************************************************************
 *                         FREQUENCY SCALING                         *
 *********************************************************************/
/*
 * Depending on cluster structure, it cannot be possible to get/set
 * cpu frequency while cluster is off. For this, disable cluster-wide
 * power mode while getting/setting frequency.
 */
static unsigned int get_freq(struct exynos_cpufreq_domain *domain)
{
	int wakeup_flag = 0;
	unsigned int freq;
	struct cpumask temp;

	cpumask_and(&temp, &domain->cpus, cpu_online_mask);

	if (cpumask_empty(&temp))
		return domain->old;

	if (domain->need_awake) {
		if (likely(domain->old))
			return domain->old;

		wakeup_flag = 1;
		disable_power_mode(cpumask_any(&domain->cpus), POWERMODE_TYPE_CLUSTER);
	}

	freq = (unsigned int)cal_dfs_get_rate(domain->cal_id);
	if (!freq) {
		/* On changing state, CAL returns 0 */
		freq = domain->old;
	}

	if (unlikely(wakeup_flag))
		enable_power_mode(cpumask_any(&domain->cpus), POWERMODE_TYPE_CLUSTER);

	return freq;
}

static int set_freq(struct exynos_cpufreq_domain *domain,
					unsigned int target_freq)
{
	int err;

	dbg_snapshot_printk("ID %d: %d -> %d (%d)\n",
		domain->id, domain->old, target_freq, DSS_FLAG_IN);

	if (domain->need_awake)
		disable_power_mode(cpumask_any(&domain->cpus), POWERMODE_TYPE_CLUSTER);

	err = cal_dfs_set_rate(domain->cal_id, target_freq);
	if (err < 0)
		pr_err("failed to scale frequency of domain%d (%d -> %d)\n",
			domain->id, domain->old, target_freq);

	if (domain->need_awake)
		enable_power_mode(cpumask_any(&domain->cpus), POWERMODE_TYPE_CLUSTER);

	dbg_snapshot_printk("ID %d: %d -> %d (%d)\n",
		domain->id, domain->old, target_freq, DSS_FLAG_OUT);

	return err;
}

static int scale(struct exynos_cpufreq_domain *domain,
				struct cpufreq_policy *policy,
				unsigned int target_freq)
{
	int ret;
	struct cpufreq_freqs freqs = {
		.policy		= policy,
		.old		= domain->old,
		.new		= target_freq,
		.flags		= 0,
	};

	cpufreq_freq_transition_begin(policy, &freqs);
	dbg_snapshot_freq(domain->id, domain->old, target_freq, DSS_FLAG_IN);

	ret = pre_scale(&freqs);
	if (ret)
		goto fail_scale;

	/* Scale frequency by hooked function, set_freq() */
	ret = set_freq(domain, target_freq);
	if (ret)
		goto fail_scale;

	ret = post_scale(&freqs);
	if (ret)
		goto fail_scale;

fail_scale:
	/* In scaling failure case, logs -1 to exynos snapshot */
	dbg_snapshot_freq(domain->id, domain->old, target_freq,
					ret < 0 ? ret : DSS_FLAG_OUT);
	cpufreq_freq_transition_end(policy, &freqs, ret);

	return ret;
}

/*********************************************************************
 *                   EXYNOS CPUFREQ DRIVER INTERFACE                 *
 *********************************************************************/
static int exynos_cpufreq_init(struct cpufreq_policy *policy)
{
	struct exynos_cpufreq_domain *domain = find_domain(policy->cpu);

	if (!domain)
		return -EINVAL;

	policy->freq_table = domain->freq_table;
	policy->cur = get_freq(domain);
	policy->cpuinfo.transition_latency = TRANSITION_LATENCY;
	policy->dvfs_possible_from_any_cpu = true;
	cpumask_copy(policy->cpus, &domain->cpus);

	pr_info("Initialize cpufreq policy%d\n", policy->cpu);

	return 0;
}

static unsigned int exynos_cpufreq_resolve_freq(struct cpufreq_policy *policy,
						unsigned int target_freq)
{
	unsigned int index;

	index = cpufreq_frequency_table_target(policy, target_freq, CPUFREQ_RELATION_L);
	if (index < 0) {
		pr_err("target frequency(%d) out of range\n", target_freq);
		return 0;
	}

	return policy->freq_table[index].frequency;
}

static int exynos_cpufreq_online(struct cpufreq_policy *policy)
{
	struct exynos_cpufreq_domain *domain;

	/*
	 * CPU frequency is not changed before cpufreq_resume() is called.
	 * Therefore, if it is called by enable_nonboot_cpus(),
	 * it is ignored.
	 */
	if (cpuhp_tasks_frozen)
		return 0;

	domain = find_domain(policy->cpu);
	if (!domain)
		return 0;

	enable_domain(domain);
	freq_qos_update_request(&domain->max_qos_req, domain->max_freq);

	return 0;
}

static int exynos_cpufreq_offline(struct cpufreq_policy *policy)
{
	struct exynos_cpufreq_domain *domain;

	/*
	 * CPU frequency is not changed after cpufreq_suspend() is called.
	 * Therefore, if it is called by disable_nonboot_cpus(),
	 * it is ignored.
	 */
	if (cpuhp_tasks_frozen)
		return 0;

	domain = find_domain(policy->cpu);
	if (!domain)
		return 0;

	freq_qos_update_request(&domain->max_qos_req, domain->min_freq);
	disable_domain(domain);

	return 0;
}

static int exynos_cpufreq_verify(struct cpufreq_policy_data *new_policy)
{
	struct exynos_cpufreq_domain *domain = find_domain(new_policy->cpu);
	struct cpufreq_policy policy;
	unsigned int min_freq, max_freq;
	int index;

	if (!domain)
		return -EINVAL;

	policy.freq_table = new_policy->freq_table;

	index = cpufreq_table_find_index_ah(&policy, new_policy->max);
	if (index == -1) {
		pr_err("%s : failed to find a proper max frequency\n",__func__);
		return -EINVAL;
	}
	max_freq = policy.freq_table[index].frequency;

	index = cpufreq_table_find_index_al(&policy, new_policy->min);
	if (index == -1) {
		pr_err("%s : failed to find a proper min frequency\n",__func__);
		return -EINVAL;
	}
	min_freq = policy.freq_table[index].frequency;

	new_policy->max = domain->max_freq_qos = max_freq;
	new_policy->min = domain->min_freq_qos = min_freq;

	update_dm_min_max(domain);

	return cpufreq_frequency_table_verify(new_policy, domain->freq_table);
}

static int __exynos_cpufreq_target(struct cpufreq_policy *policy,
				  unsigned int target_freq,
				  unsigned int relation)
{
	struct exynos_cpufreq_domain *domain = find_domain(policy->cpu);
	int ret = 0;

	if (!domain)
		return -EINVAL;

	mutex_lock(&domain->lock);

	if (!domain->enabled) {
		ret = -EINVAL;
		goto out;
	}

	target_freq = cpufreq_driver_resolve_freq(policy, target_freq);
	target_freq = min(target_freq, domain->clipped_freq);

	/* Target is same as current, skip scaling */
	if (domain->old == target_freq)
		goto out;

#define TEN_MHZ (10000)
	if (abs(domain->old - get_freq(domain)) > TEN_MHZ) {
		pr_err("oops, inconsistency between domain->old:%d, real clk:%d\n",
			domain->old, get_freq(domain));
		BUG_ON(1);
	}
#undef TEN_MHZ

	ret = scale(domain, policy, target_freq);
	if (ret)
		goto out;

	pr_debug("CPUFREQ domain%d frequency change %u kHz -> %u kHz\n",
			domain->id, domain->old, target_freq);

	domain->old = target_freq;
	arch_set_freq_scale(&domain->cpus, target_freq, policy->cpuinfo.max_freq);

out:
	mutex_unlock(&domain->lock);

	return ret;
}

static int exynos_cpufreq_target(struct cpufreq_policy *policy,
					unsigned int target_freq,
					unsigned int relation)
{
	struct exynos_cpufreq_domain *domain = find_domain(policy->cpu);
	unsigned long freq;

	if (!domain)
		return -EINVAL;

	if (list_empty(&domain->dm_list))
		return __exynos_cpufreq_target(policy, target_freq, relation);

	freq = (unsigned long)target_freq;

	if (policy->cpu >= 4 &&
	    (target_freq >= 2080000 || domain->old >= 2080000))
		exynos_alt_call_chain();

	return DM_CALL(domain->dm_type, &freq);
}

static unsigned int exynos_cpufreq_get(unsigned int cpu)
{
	struct exynos_cpufreq_domain *domain = find_domain(cpu);

	if (!domain)
		return 0;

	return get_freq(domain);
}

static int __exynos_cpufreq_suspend(struct cpufreq_policy *policy,
				struct exynos_cpufreq_domain *domain)
{
	unsigned int freq;
	struct work_struct *update_work = &policy->update;

	if (!domain)
		return 0;

	mutex_lock(&domain->lock);
	mutex_unlock(&domain->lock);

	freq = domain->resume_freq;

	freq_qos_update_request(policy->min_freq_req, freq);
	freq_qos_update_request(policy->max_freq_req, freq);

	flush_work(update_work);

	return 0;
}

static int exynos_cpufreq_suspend(struct cpufreq_policy *policy)
{
	return __exynos_cpufreq_suspend(policy, find_domain(policy->cpu));
}

static int __exynos_cpufreq_resume(struct cpufreq_policy *policy,
				struct exynos_cpufreq_domain *domain)
{
	if (!domain)
		return -EINVAL;

	mutex_lock(&domain->lock);
	mutex_unlock(&domain->lock);

	freq_qos_update_request(policy->max_freq_req, domain->max_freq);
	freq_qos_update_request(policy->min_freq_req, domain->min_freq);

	return 0;
}

static int exynos_cpufreq_resume(struct cpufreq_policy *policy)
{
	return __exynos_cpufreq_resume(policy, find_domain(policy->cpu));
}

static void exynos_cpufreq_ready(struct cpufreq_policy *policy)
{
	struct exynos_cpufreq_ready_block *ready_block;

	list_for_each_entry(ready_block, &ready_list, list) {
		if (ready_block->update)
			ready_block->update(policy);
		if (ready_block->get_target)
			ready_block->get_target(policy, exynos_cpufreq_target);
	}
}

static int exynos_cpufreq_pm_notifier(struct notifier_block *notifier,
				       unsigned long pm_event, void *v)
{
	struct exynos_cpufreq_domain *domain;
	struct cpufreq_policy *policy;

	switch (pm_event) {
	case PM_SUSPEND_PREPARE:
		list_for_each_entry_reverse(domain, &domains, list) {
			policy = cpufreq_cpu_get(cpumask_any(&domain->cpus));
			if (!policy)
				continue;
			if (__exynos_cpufreq_suspend(policy, domain))
				return NOTIFY_BAD;
		}
		break;
	case PM_POST_SUSPEND:
		list_for_each_entry(domain, &domains, list) {
			policy = cpufreq_cpu_get(cpumask_any(&domain->cpus));
			if (!policy)
				continue;
			if (__exynos_cpufreq_resume(policy, domain))
				return NOTIFY_BAD;
		}
		break;
	}
	return NOTIFY_OK;
}

static struct notifier_block exynos_cpufreq_pm = {
	.notifier_call = exynos_cpufreq_pm_notifier,
	.priority = INT_MAX,
};

static struct cpufreq_driver exynos_driver = {
	.name		= "exynos_cpufreq",
	.flags		= CPUFREQ_STICKY | CPUFREQ_HAVE_GOVERNOR_PER_POLICY,
	.init		= exynos_cpufreq_init,
	.verify		= exynos_cpufreq_verify,
	.target		= exynos_cpufreq_target,
	.get		= exynos_cpufreq_get,
	.resolve_freq	= exynos_cpufreq_resolve_freq,
	.online		= exynos_cpufreq_online,
	.offline	= exynos_cpufreq_offline,
	.suspend	= exynos_cpufreq_suspend,
	.resume		= exynos_cpufreq_resume,
	.ready		= exynos_cpufreq_ready,
	.attr		= cpufreq_generic_attr,
};

/*********************************************************************
 *                       CPUFREQ SYSFS			             *
 *********************************************************************/
static ssize_t show_freq_qos_min(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	ssize_t count = 0;
	struct exynos_cpufreq_domain *domain;

	list_for_each_entry(domain, &domains, list)
		count += snprintf(buf + count, 30, "policy%d: qos_min: %d\n",
				cpumask_first(&domain->cpus),
				domain->user_min_qos_req.pnode.prio);
	return count;
}

static ssize_t store_freq_qos_min(struct device *dev,
				struct device_attribute *attr,
				const char *buf, size_t count)
{
	int freq, cpu;
	struct exynos_cpufreq_domain *domain;

	if (!sscanf(buf, "%d %8d", &cpu, &freq))
		return -EINVAL;

	if (cpu < 0 || cpu >= NR_CPUS || freq < 0)
		return -EINVAL;

	domain = find_domain(cpu);
	if (!domain)
		return -EINVAL;

	freq_qos_update_request(&domain->user_min_qos_req, freq);

	return count;
}

static ssize_t show_freq_qos_max(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	ssize_t count = 0;
	struct exynos_cpufreq_domain *domain;

	list_for_each_entry(domain, &domains, list)
		count += snprintf(buf + count, 30, "policy%d: qos_max: %d\n",
				cpumask_first(&domain->cpus),
				domain->user_max_qos_req.pnode.prio);
	return count;
}

static ssize_t store_freq_qos_max(struct device *dev,
				struct device_attribute *attr,
				const char *buf, size_t count)
{
	int freq, cpu;
	struct exynos_cpufreq_domain *domain;

	if (!sscanf(buf, "%d %8d", &cpu, &freq))
		return -EINVAL;

	if (cpu < 0 || cpu >= NR_CPUS || freq < 0)
		return -EINVAL;

	domain = find_domain(cpu);
	if (!domain)
		return -EINVAL;

	freq_qos_update_request(&domain->user_max_qos_req, freq);

	return count;
}

static DEVICE_ATTR(freq_qos_max, S_IRUGO | S_IWUSR,
		show_freq_qos_max, store_freq_qos_max);
static DEVICE_ATTR(freq_qos_min, S_IRUGO | S_IWUSR,
		show_freq_qos_min, store_freq_qos_min);

struct acme_attr {
	struct attribute attr;
	ssize_t (*show)(struct kobject *, char *);
	ssize_t (*store)(struct kobject *, const char *, size_t count);
};

static ssize_t acme_show_clipped_freq(struct kobject *kobj, char *buf)
{
	struct exynos_cpufreq_domain *domain =
			container_of(kobj, struct exynos_cpufreq_domain, kobj);

	return snprintf(buf, 30, "%d\n", domain->clipped_freq);
}

#define ACME_ATTR_RO(name)						\
static struct acme_attr acme_attr_##name =				\
__ATTR(name, 0444, acme_show_##name, NULL)

ACME_ATTR_RO(clipped_freq);

static ssize_t show(struct kobject *kobj, struct attribute *at, char *buf)
{
	struct acme_attr *fvattr = container_of(at, struct acme_attr, attr);
	return fvattr->show(kobj, buf);
}

static ssize_t store(struct kobject *kobj, struct attribute *at,
					const char *buf, size_t count)
{
	struct acme_attr *fvattr = container_of(at, struct acme_attr, attr);
	return fvattr->store(kobj, buf, count);
}

static const struct sysfs_ops acme_sysfs_ops = {
	.show	= show,
	.store	= store,
};

static struct attribute *acme_attrs[] = {
	&acme_attr_clipped_freq.attr,
	NULL
};

static struct kobj_type ktype_acme = {
	.sysfs_ops	= &acme_sysfs_ops,
	.default_attrs	= acme_attrs,
};

/*********************************************************************
 *                       CPUFREQ DEV FOPS                            *
 *********************************************************************/

static ssize_t cpufreq_fops_write(struct file *filp, const char __user *buf,
		size_t count, loff_t *f_pos)
{
	s32 value;
	struct freq_qos_request *req = filp->private_data;
	if (count == sizeof(s32)) {
		if (copy_from_user(&value, buf, sizeof(s32)))
			return -EFAULT;
	} else {
		int ret;

		ret = kstrtos32_from_user(buf, count, 16, &value);
		if (ret)
			return ret;
	}

	freq_qos_update_request(req, value);

	return count;
}

static ssize_t cpufreq_fops_read(struct file *filp, char __user *buf,
		size_t count, loff_t *f_pos)
{
	s32 value = 0;
	return simple_read_from_buffer(buf, count, f_pos, &value, sizeof(s32));
}

static int cpufreq_fops_open(struct inode *inode, struct file *filp)
{
	int ret;
	struct exynos_cpufreq_file_operations *fops = container_of(filp->f_op,
			struct exynos_cpufreq_file_operations,
			fops);
	struct freq_qos_request *req = kzalloc(sizeof(*req), GFP_KERNEL);
	if (!req)
		return -ENOMEM;

	filp->private_data = req;
	ret = freq_qos_tracer_add_request(fops->freq_constraints, req, fops->req_type, fops->default_value);
	if (ret)
		return ret;

	return 0;
}

static int cpufreq_fops_release(struct inode *inode, struct file *filp)
{
	struct freq_qos_request *req = filp->private_data;

	freq_qos_tracer_remove_request(req);
	kfree(req);

	return 0;
}

/*********************************************************************
 *                       EXTERNAL EVENT HANDLER                      *
 *********************************************************************/
static int exynos_cpu_cooling_notifier(struct notifier_block *notifier,
				       unsigned long cpu, void *data)
{
	struct exynos_cpufreq_domain *domain;
	struct cpufreq_policy *policy;
	unsigned int freq = *(unsigned int *)data;

	domain = find_domain(cpu);
	if (!domain)
		return NOTIFY_BAD;

	policy = cpufreq_cpu_get(cpu);
	if (!policy)
		return NOTIFY_BAD;

	freq = clamp_val(freq, domain->min_freq, domain->max_freq);

	/* update clipped_freq and DM constraint */
	domain->clipped_freq = freq;
	update_dm_min_max(domain);

	/* trigger frequency scaling to apply clipped_freq */
	if (exynos_cpufreq_target(policy, domain->old, CPUFREQ_RELATION_H)) {
		cpufreq_cpu_put(policy);
		return NOTIFY_BAD;
	}

	cpufreq_cpu_put(policy);

	return NOTIFY_OK;
}

static struct notifier_block exynos_cpu_cooling_nb = {
	.notifier_call = exynos_cpu_cooling_notifier,
};

/*********************************************************************
 *                       EXTERNAL REFERENCE APIs                     *
 *********************************************************************/
void exynos_cpufreq_ready_list_add(struct exynos_cpufreq_ready_block *rb)
{
	if (!rb)
		return;

	list_add(&rb->list, &ready_list);
}
EXPORT_SYMBOL(exynos_cpufreq_ready_list_add);

#if IS_ENABLED(CONFIG_SEC_BOOTSTAT)
void sec_bootstat_get_cpuinfo(int *freq, int *online)
{
	int cpu;
	int cluster;
	struct exynos_cpufreq_domain *domain;

	get_online_cpus();
	*online = cpumask_bits(cpu_online_mask)[0];
	for_each_online_cpu(cpu) {
		domain = find_domain(cpu);
		if (!domain)
			continue;
		pr_err("%s, dm type = %d\n", __func__, domain->dm_type);
		cluster = 0;
		if (domain->dm_type == DM_CPU_CL1)
			cluster = 1;
		else if (domain->dm_type == DM_CPU_CL2)
			cluster = 2;

		freq[cluster] = get_freq(domain);
	}
	put_online_cpus();
}
EXPORT_SYMBOL(sec_bootstat_get_cpuinfo);
#endif

/*********************************************************************
 *                      SUPPORT for DVFS MANAGER                     *
 *********************************************************************/
static int init_constraint_table_ect(struct exynos_cpufreq_dm *dm,
					struct device_node *dn)
{
	void *block;
	struct ect_minlock_domain *ect_domain;
	const char *ect_name;
	unsigned int index, c_index;
	bool valid_row = false;
	int ret;

	ret = of_property_read_string(dn, "ect-name", &ect_name);
	if (ret)
		return ret;

	block = ect_get_block(BLOCK_MINLOCK);
	if (!block)
		return -ENODEV;

	ect_domain = ect_minlock_get_domain(block, (char *)ect_name);
	if (!ect_domain)
		return -ENODEV;

	for (index = 0; index < dm->c.table_length; index++) {
		unsigned int freq = dm->c.freq_table[index].master_freq;

		for (c_index = 0; c_index < ect_domain->num_of_level; c_index++) {
			/* find row same as frequency */
			if (freq == ect_domain->level[c_index].main_frequencies) {
				dm->c.freq_table[index].slave_freq
					= ect_domain->level[c_index].sub_frequencies;
				valid_row = true;
				break;
			}
		}

		/*
		 * Due to higher levels of constraint_freq should not be NULL,
		 * they should be filled with highest value of sub_frequencies
		 * of ect until finding first(highest) domain frequency fit with
		 * main_frequeucy of ect.
		 */
		if (!valid_row)
			dm->c.freq_table[index].slave_freq
				= ect_domain->level[0].sub_frequencies;
	}

	return 0;
}

static void
validate_dm_constraint_table(struct exynos_dm_freq *table, int table_size,
					int master_cal_id, int slave_cal_id)
{
	unsigned long *ect_table;
	int ect_size, index, ect_index;

	if (!master_cal_id)
		goto validate_slave;

	/* validate master frequency */
	ect_size = cal_dfs_get_lv_num(master_cal_id);
	ect_table = kzalloc(sizeof(unsigned long) * ect_size, GFP_KERNEL);
	if (!ect_table)
		return;

	cal_dfs_get_rate_table(master_cal_id, ect_table);

	for (index = 0; index < table_size; index++) {
		unsigned int freq = table[index].master_freq;

		ect_index = ect_size;
		while (--ect_index >= 0) {
			if (freq <= ect_table[ect_index]) {
				table[index].master_freq = ect_table[ect_index];
				break;
			}
		}
	}

	kfree(ect_table);

validate_slave:
	if (!slave_cal_id)
		return;

	/* validate slave frequency */
	ect_size = cal_dfs_get_lv_num(slave_cal_id);
	ect_table = kzalloc(sizeof(unsigned long) * ect_size, GFP_KERNEL);
	if (!ect_table)
		return;

	cal_dfs_get_rate_table(slave_cal_id, ect_table);

	for (index = 0; index < table_size; index++) {
		unsigned int freq = table[index].slave_freq;

		ect_index = ect_size;
		while (--ect_index >= 0) {
			if (freq <= ect_table[ect_index]) {
				table[index].slave_freq = ect_table[ect_index];
				break;
			}
		}
	}

	kfree(ect_table);
}

static int init_constraint_table_dt(struct exynos_cpufreq_dm *dm,
					struct device_node *dn)
{
	struct exynos_dm_freq *table;
	int size, table_size, index, c_index;

	/*
	 * A DM constraint table row consists of master and slave frequency
	 * value, the size of a row is 64bytes. Divide size in half when
	 * table is allocated.
	 */
	size = of_property_count_u32_elems(dn, "table");
	if (size < 0)
		return size;

	table_size = size / 2;
	table = kzalloc(sizeof(struct exynos_dm_freq) * table_size, GFP_KERNEL);
	if (!table)
		return -ENOMEM;

	of_property_read_u32_array(dn, "table", (unsigned int *)table, size);

	validate_dm_constraint_table(table, table_size,
			dm->master_cal_id, dm->slave_cal_id);

	for (index = 0; index < dm->c.table_length; index++) {
		unsigned int freq = dm->c.freq_table[index].master_freq;

		for (c_index = 0; c_index < table_size; c_index++) {
			if (freq == table[c_index].master_freq) {
				dm->c.freq_table[index].slave_freq
					= table[c_index].slave_freq;
				break;
			}
		}
	}

	kfree(table);
	return 0;
}

static int dm_scaler(int dm_type, void *devdata, unsigned int target_freq,
						unsigned int relation)
{
	struct exynos_cpufreq_domain *domain = devdata;
	struct cpufreq_policy *policy;
	struct cpumask mask;
	int ret;

	/* Skip scaling if all cpus of domain are hotplugged out */
	cpumask_and(&mask, &domain->cpus, cpu_online_mask);
	if (cpumask_empty(&mask))
		return -ENODEV;

	policy = cpufreq_cpu_get(cpumask_first(&mask));
	if (!policy) {
		pr_err("%s: failed get cpufreq policy\n", __func__);
		return -ENODEV;
	}

	ret = __exynos_cpufreq_target(policy, target_freq, relation);

	cpufreq_cpu_put(policy);

	return ret;
}

static int init_dm(struct exynos_cpufreq_domain *domain,
				struct device_node *dn)
{
	struct exynos_cpufreq_dm *dm;
	struct device_node *root;
	struct of_phandle_iterator iter;
	int ret, err;

	ret = of_property_read_u32(dn, "dm-type", &domain->dm_type);
	if (ret)
		return ret;

	ret = exynos_dm_data_init(domain->dm_type, domain, domain->min_freq,
				domain->max_freq, domain->old);
	if (ret)
		return ret;

	/* Initialize list head of DVFS Manager constraints */
	INIT_LIST_HEAD(&domain->dm_list);

	/*
	 * Initialize DVFS Manager constraints
	 * - constraint_type : minimum or maximum constraint
	 * - constraint_dm_type : cpu/mif/int/.. etc
	 * - guidance : constraint from chipset characteristic
	 * - freq_table : constraint table
	 */
	root = of_get_child_by_name(dn, "dm-constraints");
	of_for_each_phandle(&iter, err, root, "list", NULL, 0) {
		int index, r_index;

		/* allocate DM constraint */
		dm = kzalloc(sizeof(struct exynos_cpufreq_dm), GFP_KERNEL);
		if (!dm)
			goto init_fail;

		list_add_tail(&dm->list, &domain->dm_list);

		of_property_read_u32(iter.node, "const-type", &dm->c.constraint_type);
		of_property_read_u32(iter.node, "dm-slave", &dm->c.dm_slave);
		of_property_read_u32(iter.node, "master-cal-id", &dm->master_cal_id);
		of_property_read_u32(iter.node, "slave-cal-id", &dm->slave_cal_id);

		/* allocate DM constraint table */
		dm->c.freq_table = kzalloc(sizeof(struct exynos_dm_freq)
					* domain->table_size, GFP_KERNEL);
		if (!dm->c.freq_table)
			goto init_fail;

		dm->c.table_length = domain->table_size;

		/*
		 * fill master freq, domain frequency table is in ascending
		 * order, but DM constraint table must be in descending
		 * order.
		 */
		index = 0;
		r_index = domain->table_size - 1;
		while (r_index >= 0) {
			dm->c.freq_table[index].master_freq =
				domain->freq_table[r_index].frequency;
			index++;
			r_index--;
		}

		/* fill slave freq */
		if (of_property_read_bool(iter.node, "guidance")) {
			dm->c.guidance = true;
			if (init_constraint_table_ect(dm, iter.node))
				continue;
		} else {
			if (init_constraint_table_dt(dm, iter.node))
				continue;
		}

		/* dynamic disable for migov control */
		if (of_property_read_bool(iter.node, "dynamic-disable"))
			dm->c.support_dynamic_disable = true;

		/* register DM constraint */
		ret = register_exynos_dm_constraint_table(domain->dm_type, &dm->c);
		if (ret)
			goto init_fail;
	}

	return register_exynos_dm_freq_scaler(domain->dm_type, dm_scaler);

init_fail:
	while (!list_empty(&domain->dm_list)) {
		dm = list_last_entry(&domain->dm_list,
				struct exynos_cpufreq_dm, list);
		list_del(&dm->list);
		kfree(dm->c.freq_table);
		kfree(dm);
	}

	return 0;
}

/*********************************************************************
 *                  INITIALIZE EXYNOS CPUFREQ DRIVER                 *
 *********************************************************************/
static void print_domain_info(struct exynos_cpufreq_domain *domain)
{
	int i;
	char buf[10];

	pr_info("CPUFREQ of domain%d cal-id : %#x\n",
			domain->id, domain->cal_id);

	scnprintf(buf, sizeof(buf), "%*pbl", cpumask_pr_args(&domain->cpus));
	pr_info("CPUFREQ of domain%d sibling cpus : %s\n",
			domain->id, buf);

	pr_info("CPUFREQ of domain%d boot freq = %d kHz resume freq = %d kHz\n",
			domain->id, domain->boot_freq, domain->resume_freq);

	pr_info("CPUFREQ of domain%d max freq : %d kHz, min freq : %d kHz\n",
			domain->id,
			domain->max_freq, domain->min_freq);

	pr_info("CPUFREQ of domain%d table size = %d\n",
			domain->id, domain->table_size);

	for (i = 0; i < domain->table_size; i++) {
		if (domain->freq_table[i].frequency == CPUFREQ_ENTRY_INVALID)
			continue;

		pr_info("CPUFREQ of domain%d : L%-2d  %7d kHz\n",
			domain->id,
			domain->freq_table[i].driver_data,
			domain->freq_table[i].frequency);
	}
}

static void init_sysfs(struct kobject *kobj)
{
	struct exynos_cpufreq_domain *domain;

	if (sysfs_create_file(kobj, &dev_attr_freq_qos_max.attr))
		pr_err("failed to create user_max node\n");

	if (sysfs_create_file(kobj, &dev_attr_freq_qos_min.attr))
		pr_err("failed to create user_min node\n");

	list_for_each_entry(domain, &domains, list) {
		if (kobject_init_and_add(&domain->kobj, &ktype_acme, kobj,
					 "domain%d", domain->id))
			pr_err("Fail to init sysfs node(domain%d)\n", domain->id);
	}
}

static void freq_qos_release(struct work_struct *work)
{
	struct exynos_cpufreq_domain *domain = container_of(to_delayed_work(work),
						  struct exynos_cpufreq_domain,
						  work);

	freq_qos_update_request(&domain->min_qos_req, domain->min_freq);
	freq_qos_update_request(&domain->max_qos_req, domain->max_freq);
}

static int
init_freq_qos(struct exynos_cpufreq_domain *domain, struct cpufreq_policy *policy)
{
	unsigned int boot_qos, val;
	struct device_node *dn = domain->dn;
	int ret;
#if IS_ENABLED(CONFIG_ARM_EXYNOS_ACME_DISABLE_BOOT_LOCK)
	int cpufreq_disable_boot_qos_lock_idx = 0;
#endif

	ret = freq_qos_tracer_add_request(&policy->constraints, &domain->min_qos_req,
				FREQ_QOS_MIN, domain->min_freq);
	if (ret < 0)
		return ret;

	ret = freq_qos_tracer_add_request(&policy->constraints, &domain->max_qos_req,
				FREQ_QOS_MAX, domain->max_freq);
	if (ret < 0)
		return ret;

	ret = freq_qos_tracer_add_request(&policy->constraints, &domain->user_min_qos_req,
				FREQ_QOS_MIN, domain->min_freq);
	if (ret < 0)
		return ret;

	ret = freq_qos_tracer_add_request(&policy->constraints, &domain->user_max_qos_req,
				FREQ_QOS_MAX, domain->max_freq);
	if (ret < 0)
		return ret;

	/*
	 * Basically booting pm_qos is set to max frequency of domain.
	 * But if pm_qos-booting exists in device tree,
	 * booting pm_qos is selected to smaller one
	 * between max frequency of domain and the value defined in device tree.
	 */
	boot_qos = domain->max_freq;
	if (!of_property_read_u32(dn, "pm_qos-booting", &val))
		boot_qos = min(boot_qos, val);

#if IS_ENABLED(CONFIG_ARM_EXYNOS_ACME_DISABLE_BOOT_LOCK)
	pr_info("%s: flexable_cpu_boot = %d\n", __func__, flexable_cpu_boot);

	if (flexable_cpu_boot == FLEXBOOT_ALL) {
		pr_info("All skip boot cpu[%d] max qos lock\n", domain->id);
	} else if ((flexable_cpu_boot >= FLEXBOOT_LIT) && (flexable_cpu_boot <= FLEXBOOT_BIG)) {
		cpufreq_disable_boot_qos_lock_idx = flexable_cpu_boot - 1;
		if (domain->id == cpufreq_disable_boot_qos_lock_idx)
			pr_info("skip boot cpu[%d] max qos lock\n", domain->id);
		else
			freq_qos_update_request(&domain->max_qos_req, boot_qos);
	} else {
		freq_qos_update_request(&domain->min_qos_req, boot_qos);
		freq_qos_update_request(&domain->max_qos_req, boot_qos);
	}
#else
	freq_qos_update_request(&domain->min_qos_req, boot_qos);
	freq_qos_update_request(&domain->max_qos_req, boot_qos);
#endif
	pr_info("domain%d operates at %dKHz for 40secs\n", domain->id, boot_qos);

	/* booting boost, it is expired after 40s */
	INIT_DELAYED_WORK(&domain->work, freq_qos_release);
	schedule_delayed_work(&domain->work, msecs_to_jiffies(40000));

	return 0;
}

static int
init_fops(struct exynos_cpufreq_domain *domain, struct cpufreq_policy *policy)
{
	char *node_name_buffer;
	int ret, buffer_size;;

	buffer_size = sizeof(char [64]);
	node_name_buffer = kzalloc(buffer_size, GFP_KERNEL);
	if (node_name_buffer == NULL)
		return -ENOMEM;

	snprintf(node_name_buffer, buffer_size,
			"cluster%d_freq_min", domain->id);

	domain->min_qos_fops.fops.write		= cpufreq_fops_write;
	domain->min_qos_fops.fops.read		= cpufreq_fops_read;
	domain->min_qos_fops.fops.open		= cpufreq_fops_open;
	domain->min_qos_fops.fops.release	= cpufreq_fops_release;
	domain->min_qos_fops.fops.llseek	= noop_llseek;

	domain->min_qos_fops.miscdev.minor	= MISC_DYNAMIC_MINOR;
	domain->min_qos_fops.miscdev.name	= node_name_buffer;
	domain->min_qos_fops.miscdev.fops	= &domain->min_qos_fops.fops;

	domain->min_qos_fops.freq_constraints	= &policy->constraints;
	domain->min_qos_fops.default_value	= FREQ_QOS_MIN_DEFAULT_VALUE;
	domain->min_qos_fops.req_type		= FREQ_QOS_MIN;

	ret = misc_register(&domain->min_qos_fops.miscdev);
	if (ret) {
		pr_err("CPUFREQ couldn't register misc device min for domain %d", domain->id);
		kfree(node_name_buffer);
		return ret;
	}

	node_name_buffer = kzalloc(buffer_size, GFP_KERNEL);
	if (node_name_buffer == NULL)
		return -ENOMEM;

	snprintf(node_name_buffer, buffer_size,
			"cluster%d_freq_max", domain->id);

	domain->max_qos_fops.fops.write		= cpufreq_fops_write;
	domain->max_qos_fops.fops.read		= cpufreq_fops_read;
	domain->max_qos_fops.fops.open		= cpufreq_fops_open;
	domain->max_qos_fops.fops.release	= cpufreq_fops_release;
	domain->max_qos_fops.fops.llseek	= noop_llseek;

	domain->max_qos_fops.miscdev.minor	= MISC_DYNAMIC_MINOR;
	domain->max_qos_fops.miscdev.name	= node_name_buffer;
	domain->max_qos_fops.miscdev.fops	= &domain->max_qos_fops.fops;

	domain->max_qos_fops.freq_constraints	= &policy->constraints;
	domain->max_qos_fops.default_value	= FREQ_QOS_MAX_DEFAULT_VALUE;
	domain->max_qos_fops.req_type		= FREQ_QOS_MAX;

	ret = misc_register(&domain->max_qos_fops.miscdev);
	if (ret) {
		pr_err("CPUFREQ couldn't register misc device max for domain %d", domain->id);
		kfree(node_name_buffer);
		return ret;
	}

	return 0;
}

struct freq_volt {
	unsigned int freq;
	unsigned int volt;
};

static int init_domain(struct exynos_cpufreq_domain *domain,
					struct device_node *dn)
{
	unsigned int val, raw_table_size;
	int index, i;
	unsigned int freq_table[100];
	struct freq_volt *fv_table;
	const char *buf;
	int ret;

	/*
	 * Get cpumask which belongs to domain.
	 */
	ret = of_property_read_string(dn, "sibling-cpus", &buf);
	if (ret) {
		kfree(freq_table);
		return ret;
	}
	cpulist_parse(buf, &domain->cpus);
	cpumask_and(&domain->cpus, &domain->cpus, cpu_possible_mask);
	if (cpumask_weight(&domain->cpus) == 0) {
		kfree(freq_table);
		return -ENODEV;
	}

	/* Get CAL ID */
	ret = of_property_read_u32(dn, "cal-id", &domain->cal_id);
	if (ret)
		return ret;

	/*
	 * Set min/max frequency.
	 * If max-freq property exists in device tree, max frequency is
	 * selected to smaller one between the value defined in device
	 * tree and CAL. In case of min-freq, min frequency is selected
	 * to bigger one.
	 */
	domain->max_freq = cal_dfs_get_max_freq(domain->cal_id);
	domain->min_freq = cal_dfs_get_min_freq(domain->cal_id);

	if (!of_property_read_u32(dn, "max-freq", &val))
		domain->max_freq = min(domain->max_freq, val);
	if (!of_property_read_u32(dn, "min-freq", &val))
		domain->min_freq = max(domain->min_freq, val);

	domain->max_freq_qos = domain->max_freq;
	domain->min_freq_qos = domain->min_freq;

	domain->clipped_freq = domain->max_freq;

	/* Get freq-table from device tree and cut the out of range */
	raw_table_size = of_property_count_u32_elems(dn, "freq-table");
	if (of_property_read_u32_array(dn, "freq-table",
				freq_table, raw_table_size)) {
		pr_err("%s: freq-table does not exist\n", __func__);
		return -ENODATA;
	}

	domain->table_size = 0;
	for (index = 0; index < raw_table_size; index++) {
		if (freq_table[index] > domain->max_freq ||
		    freq_table[index] < domain->min_freq) {
			freq_table[index] = CPUFREQ_ENTRY_INVALID;
			continue;
		}

		domain->table_size++;
	}

	/*
	 * Get volt table from CAL with given freq-table
	 * cal_dfs_get_freq_volt_table() is called by filling the desired
	 * frequency in fv_table, the corresponding volt is filled.
	 */
	fv_table = kzalloc(sizeof(struct freq_volt)
				* (domain->table_size), GFP_KERNEL);
	if (!fv_table) {
		pr_err("%s: failed to alloc fv_table\n", __func__);
		return -ENOMEM;
	}

	i = 0;
	for (index = 0; index < raw_table_size; index++) {
		if (freq_table[index] == CPUFREQ_ENTRY_INVALID)
			continue;
		fv_table[i].freq = freq_table[index];
		i++;
	}

	if (cal_dfs_get_freq_volt_table(domain->cal_id,
				fv_table, domain->table_size)) {
		pr_err("%s: failed to get fv table from CAL\n", __func__);
		kfree(fv_table);
		return -EINVAL;
	}

	/*
	 * Allocate and initialize frequency table.
	 * Last row of frequency table must be set to CPUFREQ_TABLE_END.
	 * Table size should be one larger than real table size.
	 */
	domain->freq_table = kzalloc(sizeof(struct cpufreq_frequency_table)
				* (domain->table_size + 1), GFP_KERNEL);
	if (!domain->freq_table) {
		kfree(freq_table);
		return -ENOMEM;
	}

	for (index = 0; index < domain->table_size; index++) {
		domain->freq_table[index].driver_data = index;
		domain->freq_table[index].frequency = fv_table[index].freq;
	}
	domain->freq_table[index].driver_data = index;
	domain->freq_table[index].frequency = CPUFREQ_TABLE_END;

	/*
	 * Add OPP table for thermal.
	 * Thermal CPU cooling is based on the OPP table.
	 */
	for (index = domain->table_size -1; index >= 0; index--) {
		int cpu;

		for_each_cpu_and(cpu, &domain->cpus, cpu_possible_mask)
			dev_pm_opp_add(get_cpu_device(cpu),
				fv_table[index].freq * 1000,
				fv_table[index].volt);
	}

	kfree(fv_table);

	/*
	 * Initialize other items.
	 */
	if (of_property_read_bool(dn, "need-awake"))
		domain->need_awake = true;

	domain->boot_freq = cal_dfs_get_boot_freq(domain->cal_id);
	domain->resume_freq = cal_dfs_get_resume_freq(domain->cal_id);

	domain->old = get_freq(domain);
	if (domain->old < domain->min_freq || domain->max_freq < domain->old) {
		WARN(1, "Out-of-range freq(%dkhz) returned for domain%d in init time\n",
					domain->old,  domain->id);
		domain->old = domain->boot_freq;
	}

	mutex_init(&domain->lock);

	/*
	 * Initialize CPUFreq DVFS Manager
	 * DVFS Manager is the optional function, it does not check return value
	 */
	init_dm(domain, dn);

	dev_pm_opp_of_register_em(&domain->cpus);

	pr_info("Complete to initialize cpufreq-domain%d\n", domain->id);

	return 0;
}

static int exynos_cpufreq_probe(struct platform_device *pdev)
{
	struct device_node *dn;
	struct exynos_cpufreq_domain *domain;
	unsigned int domain_id = 0;
	int ret = 0;

	/*
	 * Pre-initialization.
	 *
	 * allocate and initialize cpufreq domain
	 */
	for_each_child_of_node(pdev->dev.of_node, dn) {
		domain = kzalloc(sizeof(struct exynos_cpufreq_domain), GFP_KERNEL);
		if (!domain) {
			pr_err("failed to allocate domain%d\n", domain_id);
			return -ENOMEM;
		}

		domain->id = domain_id++;
		if (init_domain(domain, dn)) {
			pr_err("failed to initialize cpufreq domain%d\n",
							domain->id);
			kfree(domain->freq_table);
			kfree(domain);
			continue;
		}

		domain->dn = dn;
		list_add_tail(&domain->list, &domains);

		print_domain_info(domain);
	}

	if (!domain_id) {
		pr_err("Failed to initialize cpufreq driver\n");
		return -ENOMEM;
	}

	/* Register cpufreq driver */
	ret = cpufreq_register_driver(&exynos_driver);
	if (ret) {
		pr_err("failed to register cpufreq driver\n");
		return ret;
	}

	/*
	 * Post-initialization
	 *
	 * 1. create sysfs to control frequency min/max
	 * 2. enable frequency scaling of each domain
	 * 3. initialize freq qos of each domain
	 * 4. register notifier bloack
	 */
	init_sysfs(&pdev->dev.kobj);

	list_for_each_entry(domain, &domains, list) {
		struct cpufreq_policy *policy;

		enable_domain(domain);

		policy = cpufreq_cpu_get_raw(cpumask_first(&domain->cpus));
		if (!policy)
			continue;

#if defined(CONFIG_EXYNOS_CPU_THERMAL) || defined(CONFIG_EXYNOS_CPU_THERMAL_MODULE)
		exynos_cpufreq_cooling_register(domain->dn, policy);
#endif
		ret = init_freq_qos(domain, policy);
		if (ret) {
			pr_info("failed to init pm_qos with err %d\n", ret);
			return ret;
		}
		ret = init_fops(domain, policy);
		if (ret) {
			pr_info("failed to init fops with err %d\n", ret);
			return ret;
		}

		sec_pm_cpufreq_register(policy);
	}

	cpu_cooling_notifier_register(&exynos_cpu_cooling_nb);

	register_pm_notifier(&exynos_cpufreq_pm);

	pr_info("Initialized Exynos cpufreq driver\n");

	return ret;
}

static const struct of_device_id of_exynos_cpufreq_match[] = {
	{ .compatible = "samsung,exynos-acme", },
	{ },
};
MODULE_DEVICE_TABLE(of, of_exynos_cpufreq_match);

static struct platform_driver exynos_cpufreq_driver = {
	.driver = {
		.name	= "exynos-acme",
		.owner = THIS_MODULE,
		.of_match_table = of_exynos_cpufreq_match,
	},
	.probe		= exynos_cpufreq_probe,
};

module_platform_driver(exynos_cpufreq_driver);

MODULE_DESCRIPTION("Exynos ACME");
MODULE_LICENSE("GPL");
