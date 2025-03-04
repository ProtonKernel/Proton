/*
 * Exynos Mobile Scheduler CPU selection
 *
 * Copyright (C) 2020 Samsung Electronics Co., Ltd
 */

#include <dt-bindings/soc/samsung/ems.h>

#include "../sched.h"
#include "ems.h"

#include <trace/events/ems.h>
#include <trace/events/ems_debug.h>

/******************************************************************************
 * helper function for cpu selection                                          *
 ******************************************************************************/
static unsigned int
__compute_energy(unsigned long util, unsigned long cap,
				unsigned long dp, unsigned long sp)
{
	return 1 + ((dp * util << SCHED_CAPACITY_SHIFT) / cap)
				+ (sp << SCHED_CAPACITY_SHIFT);
}

extern int emstune_cur_level;
static int get_static_power(struct energy_table *table, int cap_idx)
{
	return emstune_cur_level ? table->states[cap_idx].static_power_2nd :
				table->states[cap_idx].static_power;
}

static unsigned int
compute_energy(struct energy_table *table, struct task_struct *p,
			int target_cpu, int cap_idx)
{
	unsigned int energy;

	energy = __compute_energy(ml_cpu_util_next(target_cpu, p, target_cpu),
				table->states[cap_idx].cap,
				table->states[cap_idx].power,
				get_static_power(table, cap_idx));

	if (idle_cpu(target_cpu))
		energy += table->wakeup_cost;

	return energy;
}

/******************************************************************************
 * best energy cpu selection                                                  *
 ******************************************************************************/
static int get_next_cap(struct tp_env *env, int grp_cpu,
			int dst_cpu, unsigned int *cap_idx)
{
	int idx, cpu;
	unsigned int next_util = 0;
	struct energy_table *table;

	table = get_energy_table(grp_cpu);
	/* energy table does not exist */
	if (!table->nr_states)
		return -1;

	/* Get next capacity of cpu in coregroup with task */
	for_each_cpu(cpu, cpu_coregroup_mask(grp_cpu)) {
		int cpu_util;

		if (cpu == dst_cpu)
			cpu_util = env->cpu_util_with[cpu];
		else
			cpu_util = env->cpu_util_wo[cpu];

		if (cpu_util < next_util)
			continue;

		next_util = cpu_util;
	}

	/* Find the capacity index according to next capacity */
	*cap_idx = table->nr_states - 1;
	for (idx = 0; idx < table->nr_states; idx++) {
		if (table->states[idx].cap >= next_util) {
			*cap_idx = idx;
			break;
		}
	}

	return 0;
}

static int get_current_cap(struct tp_env *env, int grp_cpu,
			int dst_cpu, unsigned int *cap_idx)
{
	int idx, cpu;
	unsigned int next_util = 0;
	struct energy_table *table;

	table = get_energy_table(grp_cpu);
	/* energy table does not exist */
	if (!table->nr_states)
		return -1;

	/* Get next capacity of cpu in coregroup with task */
	for_each_cpu(cpu, cpu_coregroup_mask(grp_cpu)) {
		int cpu_util;

		cpu_util = env->cpu_util_wo[cpu];

		if (cpu_util < next_util)
			continue;

		next_util = cpu_util;
	}

	/* Find the capacity index according to next capacity */
	*cap_idx = table->nr_states - 1;
	for (idx = 0; idx < table->nr_states; idx++) {
		if (table->states[idx].cap >= next_util) {
			*cap_idx = idx;
			break;
		}
	}

	return 0;
}

#define PREV_ADVANTAGE_SHIFT	2
#define CL_ACTIVE_STATE_SHIFT	5
static unsigned int
compute_system_energy(struct tp_env *env,
	const struct cpumask *candidates, int dst_cpu)
{
	int grp_cpu;
	unsigned int task_util;
	unsigned int energy = 0;

	task_util = env->task_util;

	for_each_cpu(grp_cpu, cpu_active_mask) {
		struct energy_table *table;
		unsigned long power;
		unsigned int cpu, cap_idx;
		unsigned int grp_cpu_util = 0;
		unsigned int grp_cpu_util_wo = 0;
		unsigned int grp_energy;
		unsigned int static_power = 0;
		unsigned int cl_idle_cnt = 0;
		unsigned int num_cpu = 0;
		bool cl_active_state;

		/*
		 * if this group is not candidates,
		 * we don't need to compute energy for this group
		*/
		if (!cpumask_intersects(candidates, cpu_coregroup_mask(grp_cpu)))
			continue;

		if (grp_cpu != cpumask_first(cpu_coregroup_mask(grp_cpu)))
			continue;

		/* 1. get next_cap and cap_idx */
		table = get_energy_table(grp_cpu);
		if (get_next_cap(env, grp_cpu, dst_cpu, &cap_idx))
			return 0;

		/* 2. apply weight to power */
		power = (table->states[cap_idx].power * 100) / env->eff_weight[grp_cpu];

		/* 3. calculate group util */
		for_each_cpu(cpu, cpu_coregroup_mask(grp_cpu)) {
			grp_cpu_util += env->cpu_util_wo[cpu];
			grp_cpu_util_wo += env->cpu_util_wo[cpu];
			if (cpu == dst_cpu)
				grp_cpu_util += task_util;

			if (task_cpu(env->p) == cpu && task_cpu(env->p) == dst_cpu)
				grp_cpu_util -= (task_util >> PREV_ADVANTAGE_SHIFT);

			if (idle_cpu(cpu))
				cl_idle_cnt++;
		}

		/* 4. calculate group energy */
		grp_energy = __compute_energy(grp_cpu_util,
				table->states[cap_idx].cap, power, 0);

		/* 5. apply static power when cluster will be waked-up by this task */
		num_cpu = cpumask_weight(cpu_coregroup_mask(grp_cpu));
		cl_active_state = (grp_cpu_util_wo / num_cpu) > (capacity_cpu(grp_cpu) >> CL_ACTIVE_STATE_SHIFT);
		if (grp_cpu && num_cpu == cl_idle_cnt && !cl_active_state
			&& cpumask_test_cpu(dst_cpu, cpu_coregroup_mask(grp_cpu))) {
			static_power = get_static_power(table, cap_idx);
			static_power = (static_power * 100) / env->eff_weight[grp_cpu];
			grp_energy += (static_power << SCHED_CAPACITY_SHIFT);
		}
		energy += grp_energy;

		trace_ems_compute_energy(env->p, task_util, env->eff_weight[grp_cpu],
				dst_cpu, grp_cpu, grp_cpu_util,
				table->states[cap_idx].cap, power, static_power, grp_energy);
	}

	return energy;
}

#define TINY_TASK_THR_SHIFT	3	/* 1/8 of cpu cap */
#define TASK_BLOCKED_PCT	150
static void get_starvated_cpus(struct tp_env *env, struct cpumask *starvated_cpus)
{
	int grp_cpu;

	for_each_possible_cpu(grp_cpu) {
		int cpu, nr_running = 0, blocked_num_thr, num_idle_cpu = 0;

		if (grp_cpu != cpumask_first(cpu_coregroup_mask(grp_cpu)))
			continue;

		blocked_num_thr = cpumask_weight(cpu_coregroup_mask(grp_cpu));
		blocked_num_thr = (blocked_num_thr * TASK_BLOCKED_PCT / 100) + 1;
		for_each_cpu(cpu, cpu_coregroup_mask(grp_cpu)) {
			nr_running += cpu_rq(cpu)->nr_running;
			if (idle_cpu(cpu))
				num_idle_cpu++;
		}

		if (nr_running > blocked_num_thr && !num_idle_cpu) {
			cpumask_or(starvated_cpus, starvated_cpus, cpu_coregroup_mask(grp_cpu));
			trace_ems_task_blocked(env->p, grp_cpu, blocked_num_thr,
							nr_running, num_idle_cpu);
		}
	}
}

static void get_overcap_cpus(struct tp_env *env, struct cpumask *overcap_cpus)
{
	int cpu;

	for_each_cpu(cpu, cpu_active_mask) {
		unsigned long cpu_util = env->cpu_util_with[cpu];

		if (capacity_cpu(cpu) < cpu_util)
			cpumask_set_cpu(cpu, overcap_cpus);
	}
}

/* when this group is slowest and task is tiny, prefer min util idle cpu */
static bool prefer_idle_cpu(int task_util, int grp_cpu, bool adv)
{
	if (!adv || !cpumask_test_cpu(grp_cpu, cpu_coregroup_mask(0)))
		return false;

	/* if task util is under 12.5% of cpu0 capcity, task is tiny */
	if (task_util < (capacity_cpu(0) >> TINY_TASK_THR_SHIFT))
		return true;

	return false;
}

static void find_energy_candidates(struct tp_env *env,
		struct cpumask *candidates, int *min_idle_cpu)
{
	struct cpumask allowed_cpus, starvated_cpus, overcap_cpus, temp_cpus;
	int grp_cpu, src_cpu = task_cpu(env->p);
	bool adv = env->sched_policy == SCHED_POLICY_ENERGY_ADV ? true : false;
	bool high_prio_task = env->p->prio <= IMPORT_PRIO;

	cpumask_clear(candidates);
	cpumask_clear(&allowed_cpus);
	cpumask_clear(&overcap_cpus);
	cpumask_clear(&starvated_cpus);
	cpumask_clear(&temp_cpus);

	/* 1. copy task_cpus_allowed (active & tsk allowed & ecs & emst) */
	cpumask_copy(&allowed_cpus, &env->cpus_allowed);

	/* 2. exclude overcap cpus */
	get_overcap_cpus(env, &overcap_cpus);
	cpumask_andnot(&temp_cpus, &allowed_cpus, &overcap_cpus);
	if (cpumask_empty(&temp_cpus))
		return;
	cpumask_copy(&allowed_cpus, &temp_cpus);

	/* 3. apply ontime mask */
	cpumask_and(&temp_cpus, &allowed_cpus, &env->ontime_fit_cpus);
	if (cpumask_empty(&temp_cpus))
		goto find_min_util_cpu;
	cpumask_copy(&allowed_cpus, &temp_cpus);

	/* 4. exclude starvated cpus */
	if (adv) {
		get_starvated_cpus(env, &starvated_cpus);
		cpumask_andnot(&temp_cpus, &allowed_cpus, &starvated_cpus);
		if (cpumask_empty(&temp_cpus))
			goto find_min_util_cpu;
		cpumask_copy(&allowed_cpus, &temp_cpus);
	}

find_min_util_cpu:
	/* find min util cpus from allowed grp */
	for_each_cpu(grp_cpu, &allowed_cpus) {
		struct cpumask grp_cpus;
		int cpu, min_cpu = -1, min_util = INT_MAX;
		int min_idle_cpu_cnd = -1, min_idle_util = INT_MAX;
		bool select_idle_cpu;

		cpumask_and(&grp_cpus, cpu_coregroup_mask(grp_cpu), &allowed_cpus);
		if (grp_cpu != cpumask_first(&grp_cpus))
			continue;

		select_idle_cpu = prefer_idle_cpu(env->task_util, grp_cpu, adv);

		for_each_cpu_and(cpu,
			cpu_coregroup_mask(grp_cpu), &allowed_cpus) {
			int cpu_util;

			if (high_prio_task && cpu_rq(cpu)->curr->sched_class == &rt_sched_class)
				continue;

			/* get the cpu_util */
			cpu_util = env->cpu_util_with[cpu] + env->cpu_rt_util[cpu];

			/*
			 * give dst_cpu dis-advantage about 25% of task util
			 * when dst_cpu is different with src_cpu
			 */
			if (cpu == src_cpu) {
				cpu_util -= (env->task_util >> PREV_ADVANTAGE_SHIFT);
				cpu_util = max(cpu_util, 0);
			}

			/* find min util cpu */
			if (cpu_util <= min_util) {
				min_util = cpu_util;
				min_cpu = cpu;
			}

			if (select_idle_cpu && idle_cpu(cpu) && cpu_util <= min_idle_util) {
				min_idle_util = cpu_util;
				min_idle_cpu_cnd = cpu;
			}
		}

		if (min_cpu >= 0)
			cpumask_set_cpu(min_cpu, candidates);

		if (min_idle_cpu_cnd >= 0)
			*min_idle_cpu = min_idle_cpu_cnd;
	}

	if (unlikely(cpumask_empty(candidates)))
		cpumask_set_cpu(task_cpu(env->p), candidates);

	trace_ems_energy_candidates(env->p,
		*(unsigned int *)cpumask_bits(candidates));
}

static int
__find_energy_cpu(struct tp_env *env, const struct cpumask *candidates)
{
	int cpu, energy_cpu = -1, min_util = INT_MAX;
	unsigned int min_energy = UINT_MAX;

	if (cpumask_empty(candidates))
		return task_cpu(env->p);

	if (cpumask_weight(candidates) == 1)
		return cpumask_first(candidates);

	/* find energy cpu */
	for_each_cpu(cpu, candidates) {
		int cpu_util = env->cpu_util_with[cpu];
		unsigned int energy;

		/* calculate system energy */
		energy = compute_system_energy(env, candidates, cpu);

		/* 1. find min_energy cpu */
		if (energy < min_energy)
			goto energy_cpu_found;
		if (energy > min_energy)
			continue;

		/* 2. find min_util cpu when energy is same */
		if (cpu_util < min_util)
			goto energy_cpu_found;
		if (cpu_util > min_util)
			continue;

		/* 5. find randomized cpu to prevent seleting same cpu continuously */
		if (cpu_util & 0x1)
			continue;
energy_cpu_found:
		min_energy = energy;
		energy_cpu = cpu;
		min_util = cpu_util;
	}

	return energy_cpu;
}

/* setup cpu and task util */
#define CPU_ACTIVE	(-1)
static int find_energy_cpu(struct tp_env *env)
{
	struct cpumask candidates;
	int energy_cpu, min_idle_cpu = -1;

	/* set candidates cpu to find energy cpu */
	find_energy_candidates(env, &candidates, &min_idle_cpu);

	energy_cpu = __find_energy_cpu(env, &candidates);

	/*
	 * if task is tiny and selected energy cpu is in slowest cluster,
	 * prefer idle cpu among the slowest cluster.
	 */
	if (cpumask_test_cpu(energy_cpu, cpu_coregroup_mask(0))
		 && min_idle_cpu >= 0 && energy_cpu != min_idle_cpu) {
		trace_ems_tiny_task_select(env->p, energy_cpu, min_idle_cpu);
		return min_idle_cpu;
	}

	return energy_cpu;
}

/******************************************************************************
 * best efficiency cpu selection                                              *
 ******************************************************************************/
static unsigned long long
__compute_efficiency(unsigned long capacity,
		     unsigned long energy,
		     unsigned long eff_weight)
{
	unsigned long long eff;
	/*
	 * Compute performance efficiency
	 *  efficiency = (capacity / util) / energy
	 */
	eff = (capacity << SCHED_CAPACITY_SHIFT * 2) / energy;
	eff = (eff * eff_weight) / 100;

	return eff;
}

static unsigned int
compute_efficiency(struct tp_env *env, int target_cpu, unsigned int eff_weight)
{
	struct energy_table *table = get_energy_table(target_cpu);
	struct task_struct *p = env->p;
	unsigned long next_cap = 0;
	unsigned long capacity, util, energy;
	unsigned long long eff;
	unsigned int cap_idx;
	int i;

	/* energy table does not exist */
	if (!table->nr_states)
		return 0;

	/* Get next capacity of cpu in coregroup with task */
	next_cap = get_gov_next_cap(target_cpu, target_cpu, env);
	if ((int)next_cap < 0)
		next_cap = default_get_next_cap(target_cpu, p);

	/* Find the capacity index according to next capacity */
	cap_idx = table->nr_states - 1;
	for (i = 0; i < table->nr_states; i++) {
		if (table->states[i].cap >= next_cap) {
			cap_idx = i;
			break;
		}
	}

	capacity = table->states[cap_idx].cap;
	util = ml_cpu_util_next(target_cpu, p, target_cpu);
	energy = compute_energy(table, p, target_cpu, cap_idx);

	eff = __compute_efficiency(capacity, energy, eff_weight);

	trace_ems_compute_eff(p, target_cpu, util, eff_weight,
						capacity, energy, (unsigned int)eff);

	return (unsigned int)eff;
}

#define INVALID_CPU	-1
static int find_best_eff_cpu(struct tp_env *env)
{
	unsigned int eff;
	unsigned int inner_best_eff = 0, outer_best_eff = 0, prev_eff = 0;
	int best_cpu = INVALID_CPU, prev_cpu = task_cpu(env->p);
	int inner_best_cpu = INVALID_CPU, outer_best_cpu = INVALID_CPU;
	int cpu;
	/* The util ratio of tiny task is under 6.25% of SCHED_CAPACITY_SCALE. */
	int tiny_ratio = SCHED_CAPACITY_SCALE >> 4;
	struct cpumask cd_candidates;

	if (emstune_tiny_cd_sched(env->p) &&
		ml_task_util(env->p) <= tiny_ratio) {
		cpumask_clear(&cd_candidates);

		for_each_cpu(cpu, &env->fit_cpus) {
			struct energy_table *table = get_energy_table(cpu);
			unsigned int cap_idx;
			int curr_cap, next_cap, cap_diff;

			if (get_current_cap(env, cpu, cpu, &cap_idx))
				continue;

			curr_cap = table->states[cap_idx].cap;

			if (get_next_cap(env, cpu, cpu, &cap_idx))
				continue;

			next_cap = table->states[cap_idx].cap;

			cap_diff = next_cap - curr_cap;

			if (!cap_diff)
				cpumask_set_cpu(cpu, &cd_candidates);

			trace_ems_cap_difference(env->p, cpu, curr_cap, next_cap);
		}

		if (!cpumask_empty(&cd_candidates)) {
			/* find best efficiency cpu among cd candidates */
			for_each_cpu(cpu, &cd_candidates) {
				eff = compute_efficiency(env, cpu, env->eff_weight[cpu]);

				if (cpu == prev_cpu)
					prev_eff = eff;

				if (cpumask_test_cpu(cpu, cpu_coregroup_mask(prev_cpu))) {
					if (eff > inner_best_eff) {
						inner_best_eff = eff;
						inner_best_cpu = cpu;
					}
				} else {
					if (eff > outer_best_eff) {
						outer_best_eff = eff;
						outer_best_cpu = cpu;
					}
				}
			}
		}

		if (inner_best_cpu != INVALID_CPU || outer_best_cpu != INVALID_CPU)
			goto skip_find_running;
	}

	if (cpumask_empty(&env->idle_candidates))
		goto skip_find_idle;

	/* find best efficiency cpu among idle */
	for_each_cpu(cpu, &env->idle_candidates) {
		eff = compute_efficiency(env, cpu, env->eff_weight[cpu]);

		if (cpu == prev_cpu)
			prev_eff = eff;

		if (cpumask_test_cpu(cpu, cpu_coregroup_mask(prev_cpu))) {
			if (eff > inner_best_eff) {
				inner_best_eff = eff;
				inner_best_cpu = cpu;
			}
		} else {
			if (eff > outer_best_eff) {
				outer_best_eff = eff;
				outer_best_cpu = cpu;
			}
		}
	}

skip_find_idle:
	if (cpumask_empty(&env->candidates))
		goto skip_find_running;

	/* find best efficiency cpu among running */
	for_each_cpu(cpu, &env->candidates) {
		eff = compute_efficiency(env, cpu, env->eff_weight[cpu]);

		if (cpu == prev_cpu)
			prev_eff = eff;

		if (cpumask_test_cpu(cpu, cpu_coregroup_mask(prev_cpu))) {
			if (eff > inner_best_eff) {
				inner_best_eff = eff;
				inner_best_cpu = cpu;
			}
		} else {
			if (eff > outer_best_eff) {
				outer_best_eff = eff;
				outer_best_cpu = cpu;
			}
		}
	}

skip_find_running:
	/*
	 * Give 6.25% margin to prev cpu efficiency.
	 * These margins mean migration costs.
	 */
	if (inner_best_eff <= prev_eff + (prev_eff >> 4))
		inner_best_cpu = INVALID_CPU;

	if (outer_best_eff <= prev_eff + (prev_eff >> 4))
		outer_best_cpu = INVALID_CPU;

	/*
	 * If both of them are invalid, select prev cpu.
	 */
	if (inner_best_cpu == INVALID_CPU && outer_best_cpu == INVALID_CPU)
		return prev_cpu;

	/*
	 * inner_best_cpu and outer_best_cpu cannot be INVAILD
	 * at the same time in this point.
	 * If both of them are valid, select most efficient one.
	 */
	if (inner_best_cpu == INVALID_CPU)
		best_cpu = outer_best_cpu;
	else if (outer_best_cpu == INVALID_CPU)
		best_cpu = inner_best_cpu;
	else
		best_cpu = inner_best_eff >= outer_best_eff ?
				inner_best_cpu : outer_best_cpu;

	return best_cpu;
}

/******************************************************************************
 * best performance cpu selection                                             *
 ******************************************************************************/
static int find_biggest_spare_cpu(struct cpumask *candidates, int *cpu_util_wo)
{
	int cpu, max_cpu = INVALID_CPU, max_cap = 0;

	for_each_cpu(cpu, candidates) {
		int curr_cap;
		int spare_cap;

		/* get current cpu capacity */
		curr_cap = (capacity_cpu(cpu)
				* arch_scale_freq_capacity(cpu)) >> SCHED_CAPACITY_SHIFT;
		spare_cap = curr_cap - cpu_util_wo[cpu];

		if (max_cap < spare_cap) {
			max_cap = spare_cap;
			max_cpu = cpu;
		}
	}
	return max_cpu;
}

static int set_candidate_cpus(struct tp_env *env, int task_util, const struct cpumask *mask,
		struct cpumask *idle_candidates, struct cpumask *active_candidates, int *cpu_util_wo)
{
	int cpu, bind = false;

	if (unlikely(!cpumask_equal(mask, cpu_possible_mask)))
		bind = true;

	for_each_cpu_and(cpu, mask, cpu_active_mask) {
		unsigned long capacity = capacity_cpu(cpu);

		if (!cpumask_test_cpu(cpu, env->p->cpus_ptr))
			continue;

		/* remove overfit cpus from candidates */
		if (likely(!bind) && (capacity < (cpu_util_wo[cpu] + task_util)))
			continue;

		if (!cpu_rq(cpu)->nr_running)
			cpumask_set_cpu(cpu, idle_candidates);
		else
			cpumask_set_cpu(cpu, active_candidates);
	}

	return bind;
}

static int find_best_perf_cpu(struct tp_env *env)
{
	struct cpumask idle_candidates, active_candidates;
	int cpu_util_wo[NR_CPUS];
	int cpu, best_cpu = INVALID_CPU;
	int task_util;
	int bind;

	cpumask_clear(&idle_candidates);
	cpumask_clear(&active_candidates);

	task_util = ml_task_util_est(env->p);
	for_each_cpu(cpu, cpu_active_mask) {
		/* get the ml cpu util wo */
		cpu_util_wo[cpu] = ml_cpu_util(cpu);
		if (cpu == task_cpu(env->p))
			cpu_util_wo[cpu] = max(cpu_util_wo[cpu] - task_util, 0);
	}

	bind = set_candidate_cpus(env, task_util, emstune_cpus_allowed(env->p),
			&idle_candidates, &active_candidates, cpu_util_wo);

	/* find biggest spare cpu among the idle_candidates */
	best_cpu = find_biggest_spare_cpu(&idle_candidates, cpu_util_wo);
	if (best_cpu != INVALID_CPU) {
		trace_ems_find_best_idle(env->p, task_util,
			*(unsigned int *)cpumask_bits(&idle_candidates),
			*(unsigned int *)cpumask_bits(&active_candidates), bind, best_cpu);
		return best_cpu;
	}

	/* find biggest spare cpu among the active_candidates */
	best_cpu = find_biggest_spare_cpu(&active_candidates, cpu_util_wo);
	if (best_cpu != INVALID_CPU) {
		trace_ems_find_best_idle(env->p, task_util,
			*(unsigned int *)cpumask_bits(&idle_candidates),
			*(unsigned int *)cpumask_bits(&active_candidates), bind, best_cpu);
		return best_cpu;
	}

	/* if there is no best_cpu, return previous cpu */
	best_cpu = task_cpu(env->p);
	trace_ems_find_best_idle(env->p, task_util,
		*(unsigned int *)cpumask_bits(&idle_candidates),
		*(unsigned int *)cpumask_bits(&active_candidates), bind, best_cpu);

	return best_cpu;
}

/******************************************************************************
 * cpu with low energy in best efficienct domain selection                    *
 ******************************************************************************/
static int find_best_eff_energy_cpu(struct tp_env *env)
{
	int eff_cpu, energy_cpu;
	struct cpumask candidates;

	eff_cpu = find_best_eff_cpu(env);
	if (eff_cpu < 0)
		return eff_cpu;

	cpumask_copy(&candidates, cpu_coregroup_mask(eff_cpu));
	energy_cpu = __find_energy_cpu(env, &candidates);

	return energy_cpu;
}

/******************************************************************************
 * best cpu selection                                                         *
 ******************************************************************************/
int find_best_cpu(struct tp_env *env)
{
	int best_cpu = INVALID_CPU;

	switch (env->sched_policy) {
	case SCHED_POLICY_EFF:
	case SCHED_POLICY_EFF_TINY:
		/* Find best efficiency cpu */
		best_cpu = find_best_eff_cpu(env);
		break;
	case SCHED_POLICY_ENERGY:
	case SCHED_POLICY_ENERGY_ADV:
		/* Find lowest energy cpu */
		best_cpu = find_energy_cpu(env);
		break;
	case SCHED_POLICY_PERF:
		/* Find best performance cpu */
		best_cpu = find_best_perf_cpu(env);
		break;
	case SCHED_POLICY_EFF_ENERGY:
		/* Find lowest energy cpu in best efficient domain */
		best_cpu = find_best_eff_energy_cpu(env);
		break;
	}

	return best_cpu;
}
