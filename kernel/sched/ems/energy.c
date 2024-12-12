/*
 * Energy efficient cpu selection
 *
 * Copyright (C) 2018 Samsung Electronics Co., Ltd
 * Park Bumgyu <bumgyu.park@samsung.com>
 */

#include <linux/pm_opp.h>

#include "../sched.h"
#include "ems.h"

#include <trace/events/ems.h>
#include <trace/events/ems_debug.h>

DEFINE_PER_CPU(struct energy_table, energy_table);

__weak int get_gov_next_cap(int grp_cpu, int dst_cpu, struct tp_env *env)
{
	return -1;
}

int
default_get_next_cap(int dst_cpu, struct task_struct *p)
{
	struct energy_table *table = get_energy_table(dst_cpu);
	unsigned long next_cap = 0;
	int cpu;

	for_each_cpu(cpu, cpu_coregroup_mask(dst_cpu)) {
		unsigned long util;

		if (cpu == dst_cpu) { /* util with task */
			util = ml_cpu_util_next(cpu, p, dst_cpu);

			/* if it is over-capacity, give up eifficiency calculatation */
			if (util > table->states[table->nr_states - 1].cap)
				return 0;
		}
		else /* util without task */
			util = ml_cpu_util_without(cpu, p);

		/*
		 * The cpu in the coregroup has same capacity and the
		 * capacity depends on the cpu with biggest utilization.
		 * Find biggest utilization in the coregroup and use it
		 * as max floor to know what capacity the cpu will have.
		 */
		if (util > next_cap)
			next_cap = util;
	}

	return next_cap;
}

/*
 * returns allowed capacity base on the allowed power
 * freq: base frequency to find base_power
 * power: allowed_power = base_power + power
 */
int find_allowed_capacity(int cpu, unsigned int freq, int power)
{
	struct energy_table *table = get_energy_table(cpu);
	unsigned long new_power = 0;
	int i, max_idx = table->nr_states - 1;

	if (max_idx < 0)
		return 0;

	/* find power budget for new frequency */
	for (i = 0; i < max_idx; i++)
		if (table->states[i].frequency >= freq)
			break;

	/* calaculate new power budget */
	new_power = table->states[i].power + power;

	/* find minimum freq over the new power budget */
	for (i = 0; i < table->nr_states; i++)
		if (table->states[i].power >= new_power)
			return table->states[i].cap;

	/* return max capacity */
	return table->states[max_idx].cap;
}

int find_step_power(int cpu, int step)
{
	struct energy_table *table = get_energy_table(cpu);
	int max_idx = table->nr_states_orig - 1;

	if (!step || max_idx < 0)
		return 0;

	return (table->states[max_idx].power - table->states[0].power) / step;
}

static unsigned int get_table_index(int cpu, unsigned int freq)
{
	struct energy_table *table = get_energy_table(cpu);
	unsigned int i;
	for (i = 0; i < table->nr_states; i++)
		if (table->states[i].frequency >= freq)
			return i;
	return 0;
}

unsigned int get_diff_num_levels(int cpu, unsigned int freq1, unsigned int freq2)
{
	unsigned int index1, index2;
	index1 = get_table_index(cpu, freq1);
	index2 = get_table_index(cpu, freq2);
	return abs(index1 - index2);
}

unsigned long capacity_cpu_orig(int cpu)
{
	return cpu_rq(cpu)->cpu_capacity_orig;
}

unsigned long capacity_cpu(int cpu)
{
	return cpu_rq(cpu)->cpu_capacity;
}

static void
fill_frequency_table(struct energy_table *table, int table_size,
			struct cpufreq_frequency_table *f_table, int max_f, int min_f)
{
	int i, index = 0;

	for (i = 0; i < table_size; i++) {
		if (f_table[i].frequency > max_f || f_table[i].frequency < min_f)
			continue;

		table->states[index].frequency = f_table[i].frequency;
		index++;
	}
}

static void
fill_power_table(int cpu, struct energy_table *table, int table_size,
			struct cpufreq_frequency_table *f_table,
			int max_f, int min_f)
{
	int i, index = 0;
	int c = table->coefficient, v;
	int static_c = table->static_coefficient;
	unsigned long f_hz, f_mhz, power, static_power;
	struct device *dev = get_cpu_device(cpu);

	if (unlikely(!dev))
		return;

	/* energy table and frequency table are inverted */
	for (i = 0; i < table_size; i++) {
		struct dev_pm_opp *opp;

		if (f_table[i].frequency > max_f || f_table[i].frequency < min_f)
			continue;

		f_mhz = f_table[i].frequency / 1000;	/* KHz -> MHz */
		f_hz = f_table[i].frequency * 1000;		/* KHz -> Hz */

		/* Get voltage from opp */
		opp = dev_pm_opp_find_freq_ceil(dev, &f_hz);
		v = dev_pm_opp_get_voltage(opp) / 1000;		/* uV -> mV */

		/*
		 * power = coefficent * frequency * voltage^2
		 */
		power = c * f_mhz * v * v;
		static_power = static_c * v * v;

		/*
		 * Generally, frequency is more than treble figures in MHz and
		 * voltage is also more then treble figures in mV, so the
		 * calculated power is larger than 10^9. For convenience of
		 * calculation, divide the value by 10^9.
		 */
		do_div(power, 1000000000);
		do_div(static_power, 1000000);
		table->states[index].power = power;
		table->states[index].static_power = static_power;

		static_power = table->static_coefficient_2nd * v * v;
		do_div(static_power, 1000000);
		table->states[index].static_power_2nd = static_power;

		index++;
	}
}

static void
fill_cap_table(struct energy_table *table, unsigned long max_mips)
{
	int i;
	int mpm = table->mips_per_mhz;
	unsigned long f;

	for (i = 0; i < table->nr_states; i++) {
		f = table->states[i].frequency;

		/*
		 *     mips(f) = f * mips_per_mhz
		 * capacity(f) = mips(f) / max_mips * 1024
		 */
		table->states[i].cap = f * mpm * 1024 / max_mips;
	}
}

static inline int
find_nr_states(struct energy_table *table, int clipped_freq)
{
	int i;

	for (i = table->nr_states_orig - 1; i >= 0; i--) {
		if (table->states[i].frequency <= clipped_freq)
			break;
	}

	return i + 1;
}

static unsigned long find_max_mips(void)
{
	struct energy_table *table;
	unsigned long max_mips = 0;
	int cpu;

	/*
	 * Find fastest cpu among the cpu to which the energy table is allocated.
	 * The mips and max frequency of fastest cpu are needed to calculate
	 * capacity.
	 */
	for_each_possible_cpu(cpu) {
		unsigned long max_f, mpm;
		unsigned long mips;

		table = get_energy_table(cpu);
		if (!table->states)
			continue;

		/* max mips = max_f * mips_per_mhz */
		max_f = table->states[table->nr_states - 1].frequency;
		mpm = max(table->mips_per_mhz, table->mips_per_mhz_s);
		mips = max_f * mpm;
		if (mips > max_mips)
			max_mips = mips;
	}

	return max_mips;
}

static void
update_capacity(struct energy_table *table, int cpu, bool init)
{
	int last_state = table->nr_states - 1;
	struct sched_domain *sd;

	if (last_state < 0)
		return;

	/* announce capacity update to cfs */
	topology_set_cpu_scale(cpu, table->states[last_state].cap);
	rcu_read_lock();
	for_each_domain(cpu, sd)
		update_group_capacity(sd, cpu);
	rcu_read_unlock();
}

static ssize_t show_energy_table(struct kobject *kobj,
		struct kobj_attribute *attr, char *buf)
{
	int cpu, i;
	int ret = 0;

	for_each_possible_cpu(cpu) {
		struct energy_table *table;

		if (cpu != cpumask_first(cpu_coregroup_mask(cpu)))
			continue;

		ret += snprintf(buf + ret, PAGE_SIZE - ret, "[Energy Table: cpu%d]\n", cpu);

		table = get_energy_table(cpu);
		for (i = 0; i < table->nr_states; i++) {
			ret += snprintf(buf + ret, PAGE_SIZE - ret,
				"cap=%lu dp=%lu | sp=%lu\n",
				table->states[i].cap, table->states[i].power,
				table->states[i].static_power);
		}

		ret += snprintf(buf + ret, PAGE_SIZE - ret, "\n");
	}

	return ret;
}

static struct kobj_attribute energy_table_attr =
__ATTR(energy_table, 0444, show_energy_table, NULL);

static void init_wakeup_cost(void)
{
	int cpu, grp_cpu;

	for_each_possible_cpu(cpu) {
		unsigned int wakeup_cost;
		struct energy_table *table;

		if (cpu != cpumask_first(cpu_coregroup_mask(cpu)))
			continue;

		table = get_energy_table(cpu);
		if (!table->nr_states)
			continue;

		wakeup_cost = table->states[0].power * table->states[0].cap;
		wakeup_cost -= table->states[0].power;
		wakeup_cost /= table->states[table->nr_states - 1].cap;
		wakeup_cost++;

		for_each_cpu(grp_cpu, cpu_coregroup_mask(cpu))
			table->wakeup_cost = wakeup_cost;
	}
}

/*
 * Whenever frequency domain is registered, and energy table corresponding to
 * the domain is created. Because cpu in the same frequency domain has the same
 * energy table. Capacity is calculated based on the max frequency of the fastest
 * cpu, so once the frequency domain of the faster cpu is regsitered, capacity
 * is recomputed.
 */
static void init_sched_energy_table(struct cpufreq_policy *policy)
{
	struct energy_table *table;
	int i, cpu, table_size = 0, valid_table_size = 0;
	unsigned long max_mips;
	struct cpumask *cpus = policy->related_cpus;
	struct cpufreq_frequency_table *cursor, *f_table = policy->freq_table;
	int max_f = policy->cpuinfo.max_freq, min_f = policy->cpuinfo.min_freq;

	cpufreq_for_each_entry(cursor, f_table)
		table_size++;

	/* get size of valid frequency table to allocate energy table */
	for (i = 0; i < table_size; i++) {
		if (f_table[i].frequency > max_f || f_table[i].frequency < min_f)
			continue;

		valid_table_size++;
	}

	/* there is no valid row in the table, energy table is not created */
	if (!valid_table_size)
		return;

	/* allocate memory for energy table and fill frequency */
	for_each_cpu(cpu, cpus) {
		table = get_energy_table(cpu);
		table->states = kcalloc(valid_table_size,
				sizeof(struct energy_state), GFP_KERNEL);
		if (unlikely(!table->states))
			return;

		table->nr_states = table->nr_states_orig = valid_table_size;

		fill_frequency_table(table, table_size, f_table, max_f, min_f);
	}

	/*
	 * Because 'capacity' column of energy table is a relative value, the
	 * previously configured capacity of energy table can be reconfigurated
	 * based on the maximum mips whenever new cpu's energy table is
	 * initialized. On the other hand, since 'power' column of energy table
	 * is an obsolute value, it needs to be configured only once at the
	 * initialization of the energy table.
	 */
	max_mips = find_max_mips();
	for_each_possible_cpu(cpu) {
		table = get_energy_table(cpu);
		if (!table->states)
			continue;

		/*
		 * 1. fill power column of energy table only for the cpu that
		 *    initializes the energy table.
		 */
		if (cpumask_test_cpu(cpu, cpus))
			fill_power_table(cpu, table, table_size,
					f_table, max_f, min_f);

		/* 2. fill capacity column of energy table */
		fill_cap_table(table, max_mips);

		/* 3. update per-cpu capacity variable */
		update_capacity(table, cpu, true);
	}

	init_wakeup_cost();
	topology_update();
}

static int init_sched_energy_data(void)
{
	struct device_node *cpu_node, *cpu_phandle;
	int cpu, ret;

	for_each_possible_cpu(cpu) {
		struct energy_table *table;

		cpu_node = of_get_cpu_node(cpu, NULL);
		if (!cpu_node) {
			pr_warn("CPU device node missing for CPU %d\n", cpu);
			return -ENODATA;
		}

		cpu_phandle = of_parse_phandle(cpu_node, "sched-energy-data", 0);
		if (!cpu_phandle) {
			pr_warn("CPU device node has no sched-energy-data\n");
			return -ENODATA;
		}

		table = get_energy_table(cpu);
		if (of_property_read_u32(cpu_phandle, "mips-per-mhz", &table->mips_per_mhz)) {
			pr_warn("No mips-per-mhz data\n");
			return -ENODATA;
		}

		if (of_property_read_u32(cpu_phandle, "power-coefficient", &table->coefficient)) {
			pr_warn("No power-coefficient data\n");
			return -ENODATA;
		}

		if (of_property_read_u32(cpu_phandle, "static-power-coefficient",
								&table->static_coefficient)) {
			pr_warn("No static-power-coefficient data\n");
			return -ENODATA;
		}

		if (of_property_read_u32(cpu_phandle, "static-power-coefficient-2nd",
								&table->static_coefficient_2nd))
			table->static_coefficient_2nd = table->static_coefficient;

		of_node_put(cpu_phandle);
		of_node_put(cpu_node);
	}

	ret = sysfs_create_file(ems_kobj, &energy_table_attr.attr);
	if (ret)
		pr_warn("%s: failed to create sysfs\n", __func__);

	return 0;
}

static cpumask_var_t cpus_to_visit;
static void parsing_done_workfn(struct work_struct *work);
static DECLARE_WORK(parsing_done_work, parsing_done_workfn);

static int
init_energy_table_callback(struct notifier_block *nb,
			   unsigned long val,
			   void *data)
{
	struct cpufreq_policy *policy = data;

	if (val != CPUFREQ_CREATE_POLICY)
		return 0;

	cpumask_andnot(cpus_to_visit, cpus_to_visit, policy->related_cpus);

	init_sched_energy_table(policy);

	if (cpumask_empty(cpus_to_visit)) {
		pr_debug("ems: energy: init energy table done\n");
		schedule_work(&parsing_done_work);
	}

	return 0;
}

static struct notifier_block init_energy_table_notifier = {
	.notifier_call = init_energy_table_callback,
	.priority = INT_MIN,
};

static void parsing_done_workfn(struct work_struct *work)
{
	cpufreq_unregister_notifier(&init_energy_table_notifier,
					 CPUFREQ_POLICY_NOTIFIER);
	free_cpumask_var(cpus_to_visit);
}

int energy_table_init(void)
{
	int ret;

	init_sched_energy_data();

	if (!alloc_cpumask_var(&cpus_to_visit, GFP_KERNEL))
		return -ENOMEM;

	cpumask_copy(cpus_to_visit, cpu_possible_mask);

	ret = cpufreq_register_notifier(&init_energy_table_notifier,
					CPUFREQ_POLICY_NOTIFIER);

	if (ret)
		free_cpumask_var(cpus_to_visit);

	return ret;
}
