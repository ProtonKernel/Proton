/*
 * Core Exynos Mobile Scheduler
 *
 * Copyright (C) 2018 Samsung Electronics Co., Ltd
 * Park Bumgyu <bumgyu.park@samsung.com>
 */

#include <linux/pm_qos.h>
#include <linux/platform_device.h>

#include "../sched.h"
#include "ems.h"

#define CREATE_TRACE_POINTS
#include <trace/events/ems.h>
#include <trace/events/ems_debug.h>

#include <dt-bindings/soc/samsung/ems.h>

extern int emstune_cur_level;

#define TINY_TASK_RATIO_SHIFT	3	/* 12.5% */
#define BUSY_GROUP_RATIO_SHIFT  2	/* 25% */

static bool ems_initialized;

#define MAX_CLUSTER_NUM 3
static void (*lb_newidle_func[MAX_CLUSTER_NUM])(int, struct rq **, int, bool);

/* List of process element */
struct pe_list {
	struct cpumask *cpus;
	int num_of_cpus;
};

struct pe_list *pe_list;
static int num_of_list;

struct pe_list *get_pe_list(int index)
{
	if (unlikely(!pe_list))
		return NULL;

	if (index >= num_of_list)
		return NULL;

	return &pe_list[index];
}

static int get_pe_list_size(void)
{
	return num_of_list;
}

static void select_fit_cpus(struct tp_env *env)
{
	struct cpumask fit_cpus;
	struct cpumask cpus_allowed;
	struct cpumask ontime_fit_cpus, overcap_cpus, busy_cpus;
	struct task_struct *p = env->p;
	int cpu;
	struct cpumask mask_big_cpu;

	/* Clear masks */
	cpumask_clear(&env->fit_cpus);
	cpumask_clear(&fit_cpus);
	cpumask_clear(&ontime_fit_cpus);
	cpumask_clear(&overcap_cpus);
	cpumask_clear(&busy_cpus);
	cpumask_clear(&mask_big_cpu);

	/*
	 * Make cpus allowed task assignment.
	 * The fit cpus are determined among allowed cpus of below:
	 *   1. cpus allowed of task
	 *   2. cpus allowed of ems tune
	 *   3. cpus allowed of cpu sparing
	 */
	cpumask_and(&cpus_allowed, env->p->cpus_ptr, cpu_active_mask);
	cpumask_and(&cpus_allowed, &cpus_allowed, emstune_cpus_allowed(env->p));
	cpumask_and(&cpus_allowed, &cpus_allowed, ecs_cpus_allowed(env->p));
	cpumask_copy(&env->cpus_allowed, &cpus_allowed);

	if (cpumask_empty(&cpus_allowed)) {
		/* no cpus allowed, give up on selecting cpus */
		return;
	}

	/* Get cpus where fits task from ontime migration */
	ontime_select_fit_cpus(p, &ontime_fit_cpus);
	cpumask_copy(&env->ontime_fit_cpus, &ontime_fit_cpus);

	/*
	 * Find cpus that becomes over capacity.
	 * If utilization of cpu with given task exceeds cpu capacity, it is
	 * over capacity.
	 *
	 * overcap_cpus = cpu util + task util > cpu capacity
	 */
	for_each_cpu(cpu, &cpus_allowed) {
		unsigned long new_util;

		new_util = ml_cpu_util_without(cpu, p) + ml_task_util(p);
		if (new_util > capacity_cpu(cpu))
			cpumask_set_cpu(cpu, &overcap_cpus);
	}

	/*
	 * Find busy cpus.
	 * If this function is called by ontime migration(env->wake == 0),
	 * it looks for busy cpu to exclude from selection. Utilization of cpu
	 * exceeds 12.5% of cpu capacity, it is defined as busy cpu.
	 * (12.5% : this percentage is heuristically obtained)
	 *
	 * However, the task wait time is too long (hungry state), don't consider
	 * the busy cpu to spread the task as much as possible.
	 *
	 * busy_cpus = cpu util >= 12.5% of cpu capacity
	 */
	if (!env->wake && !ml_task_hungry(p)) {
		for_each_cpu(cpu, &cpus_allowed) {
			int threshold = capacity_cpu(cpu) >> 3;

			if (ml_cpu_util(cpu) >= threshold)
				cpumask_set_cpu(cpu, &busy_cpus);
		}

		goto combine_cpumask;
	}

combine_cpumask:
	/*
	 * To select cpuset where task fits, each cpumask is combined as
	 * below sequence:
	 *
	 * 1) Pick ontime_fit_cpus from cpus allowed.
	 * 2) Exclude overcap_cpu from fit cpus.
	 *    The utilization of cpu with given task become over capacity, the
	 *    cpu cannot process the task properly then performance drop.
	 *    therefore, overcap_cpu is excluded.
	 * 3) Exclude cpu which prio_pinning task is assigned to
	 *
	 *    fit_cpus = cpus_allowed & ontime_fit_cpus & ~overcap_cpus
	 *			      & ~prio_pinning_assigned_cpus
	 */
	cpumask_and(&fit_cpus, &cpus_allowed, &ontime_fit_cpus);
	cpumask_andnot(&fit_cpus, &fit_cpus, &overcap_cpus);
	if (!need_prio_pinning(env->p))
		cpumask_andnot(&fit_cpus, &fit_cpus, get_prio_pinning_assigned_mask());

	/*
	 * Case: task migration
	 *
	 * 3) Exclude busy cpus if task migration.
	 *    To improve performance, do not select busy cpus in task
	 *    migration.
	 *
	 *    fit_cpus = fit_cpus & ~busy_cpus
	 */
	if (!env->wake) {
		cpumask_andnot(&fit_cpus, &fit_cpus, &busy_cpus);
		goto finish;
	}

finish:
	if (emstune_cur_level == 1 && ml_task_util(p) < 100) {
		cpumask_set_cpu(7, &mask_big_cpu);
		cpumask_andnot(&fit_cpus, &fit_cpus, &mask_big_cpu);
	}
	cpumask_copy(&env->fit_cpus, &fit_cpus);
	trace_ems_select_fit_cpus(env->p, env->wake,
		*(unsigned int *)cpumask_bits(&env->fit_cpus),
		*(unsigned int *)cpumask_bits(&cpus_allowed),
		*(unsigned int *)cpumask_bits(&ontime_fit_cpus),
		*(unsigned int *)cpumask_bits(&overcap_cpus),
		*(unsigned int *)cpumask_bits(&busy_cpus));
}

/* setup cpu and task util */
static void get_util_snapshot(struct tp_env *env)
{
	int cpu;
	int task_util_est = (int)ml_task_util_est(env->p);

	/*
	 * We don't agree setting 0 for task util
	 * Because we do better apply active power of task
	 * when get the energy
	 */
	env->task_util = task_util_est ? task_util_est : 1;

	/* fill cpu util */
	for_each_cpu(cpu, cpu_active_mask) {
		env->cpu_util_wo[cpu] = ml_cpu_util_without(cpu, env->p);
		env->cpu_util_with[cpu] = ml_cpu_util_next(cpu, env->p, cpu);
		env->cpu_rt_util[cpu] = cpu_util_rt(cpu_rq(cpu));

		/*
		 * fill cpu util for get_next_cap,
		 * It improves expecting next_cap in governor
		 */
		env->cpu_util[cpu] = ml_cpu_util(cpu) + env->cpu_rt_util[cpu];
		env->nr_running[cpu] = cpu_rq(cpu)->nr_running;
		trace_ems_snapshot_cpu(cpu, env->task_util, env->cpu_util_wo[cpu],
			env->cpu_util_with[cpu], env->cpu_rt_util[cpu],
			env->cpu_util[cpu], env->nr_running[cpu]);
	}
}

static void filter_rt_cpu(struct tp_env *env)
{
	int cpu;
	struct cpumask mask;

	if (env->p->prio > IMPORT_PRIO)
		return;

	cpumask_copy(&mask, &env->fit_cpus);

	for_each_cpu(cpu, &env->fit_cpus)
		if (cpu_rq(cpu)->curr->sched_class == &rt_sched_class)
			cpumask_clear_cpu(cpu, &mask);

	if (!cpumask_empty(&mask))
		cpumask_copy(&env->fit_cpus, &mask);
}

static int tiny_threshold = 16;

static void get_ready_env(struct tp_env *env)
{
	int cpu;

	for_each_cpu(cpu, cpu_active_mask) {
		/*
		 * The weight is pre-defined in EMSTune.
		 * We can get weight depending on current emst mode.
		 */
		env->eff_weight[cpu] = emstune_eff_weight(env->p, cpu, idle_cpu(cpu));
	}

	/* fill up cpumask for scheduling */
	select_fit_cpus(env);

	/* filter rt running cpu */
	filter_rt_cpu(env);

	/* snapshot util to use same util during core selection */
	get_util_snapshot(env);

	/*
	 * Among fit_cpus, idle cpus are included in env->idle_candidates
	 * and running cpus are included in the env->candidate.
	 * Both candidates are exclusive.
	 */
	cpumask_clear(&env->idle_candidates);
	for_each_cpu(cpu, &env->fit_cpus)
		if (idle_cpu(cpu))
			cpumask_set_cpu(cpu, &env->idle_candidates);
	cpumask_andnot(&env->candidates, &env->fit_cpus, &env->idle_candidates);

	/*
	 * Change target tasks's policy to SCHED_POLICY_ENERGY
	 * for power optimization, if
	 * 1) target task's sched_policy is SCHED_POLICY_EFF_TINY and
	 * 2) its utilization is under 6.25% of SCHED_CAPACITY_SCALE.
	 * or
	 * 3) tasks is worker thread.
	 */
	if ((env->sched_policy == SCHED_POLICY_EFF_TINY &&
	     ml_task_util_est(env->p) <= tiny_threshold) ||
	    (env->p->flags & PF_WQ_WORKER))
		env->sched_policy = SCHED_POLICY_ENERGY;

	trace_ems_candidates(env->p, env->sched_policy,
		*(unsigned int *)cpumask_bits(&env->candidates),
		*(unsigned int *)cpumask_bits(&env->idle_candidates));
}

static int
exynos_select_task_rq_dl(struct task_struct *p, int prev_cpu,
			 int sd_flag, int wake_flag)
{
	/* TODO */
	return -1;
}

static int
exynos_select_task_rq_rt(struct task_struct *p, int prev_cpu,
			 int sd_flag, int wake_flag)
{
	/* TODO */
	return -1;
}

extern void sync_entity_load_avg(struct sched_entity *se);

static int
exynos_select_task_rq_fair(struct task_struct *p, int prev_cpu,
			   int sd_flag, int wake_flag)
{
	int target_cpu = -1;
	int sync = (wake_flag & WF_SYNC) && !(current->flags & PF_EXITING);
	int cl_sync = (wake_flag & WF_SYNC_CL) && !(current->flags & PF_EXITING);
	int wake = !p->on_rq;
	struct tp_env env = {
		.p = p,
		.sched_policy = emstune_sched_policy(p),
		.wake = wake,
	};

	/*
	 * Update utilization of waking task to apply "sleep" period
	 * before selecting cpu.
	 */
	if (!(sd_flag & SD_BALANCE_FORK))
		sync_entity_load_avg(&p->se);

	/*
	 * Get ready to find best cpu.
	 * Depending on the state of the task, the candidate cpus and C/E
	 * weight are decided.
	 */
	get_ready_env(&env);

	/* When sysbusy is detected, do scheduling under other policies */
	if (is_sysbusy()) {
		target_cpu = sysbusy_schedule(&env);
		if (target_cpu >= 0) {
			trace_ems_select_task_rq(p, target_cpu, wake, "sysbusy");
			return target_cpu;
		}
	}

	if (need_prio_pinning(p)) {
		target_cpu = prio_pinning_schedule(&env, prev_cpu);
		if (target_cpu >= 0) {
			trace_ems_select_task_rq(p, target_cpu, wake, "prio pinning");
			return target_cpu;
		}
	}

	if (cl_sync) {
		int cl_sync_cpu;
		struct cpumask cl_sync_mask;

		cl_sync_cpu = smp_processor_id();
		cpumask_and(&cl_sync_mask, &env.cpus_allowed,
					   cpu_coregroup_mask(cl_sync_cpu));
		if (!cpumask_empty(&cl_sync_mask)) {
			target_cpu = cpumask_any(&cl_sync_mask);
			trace_ems_select_task_rq(p, target_cpu, wake, "cl sync");
			return target_cpu;
		}
	}

	if (cpumask_empty(&env.fit_cpus)) {
		if (!cpumask_empty(&env.cpus_allowed) &&
			!cpumask_test_cpu(prev_cpu, &env.cpus_allowed)) {
			target_cpu = cpumask_any(&env.cpus_allowed);
			trace_ems_select_task_rq(p, target_cpu, wake, "cpus allowed");
			return target_cpu;
		}

		/* Handle sync flag */
		if (sync) {
			target_cpu = smp_processor_id();
			trace_ems_select_task_rq(p, target_cpu, wake, "sync");
			return target_cpu;
		}

		/*
		 * If there are no fit cpus, give up on choosing rq and keep
		 * the task on the prev cpu
		 */
		trace_ems_select_task_rq(p, prev_cpu, wake, "no fit cpu");
		return prev_cpu;
	}

	target_cpu = find_best_cpu(&env);
	if (target_cpu >= 0) {
		trace_ems_select_task_rq(p, target_cpu, wake, "best_cpu");
		return target_cpu;
	}

	/*
	 * Keep task on allowed cpus if no efficient cpu is found
	 * and prev cpu is not on allowed cpu
	 */
	if (!cpumask_empty(&env.cpus_allowed) &&
		!cpumask_test_cpu(prev_cpu, &env.cpus_allowed)) {
		target_cpu = cpumask_any(&env.cpus_allowed);
		trace_ems_select_task_rq(p, target_cpu, wake, "cpus allowed");
		return target_cpu;
	}

	/* Keep task on prev cpu if no efficient cpu is found */
	target_cpu = prev_cpu;
	trace_ems_select_task_rq(p, target_cpu, wake, "no benefit");

	if (!cpu_active(target_cpu))
		return cpumask_any(p->cpus_ptr);

	return target_cpu;
}

int ems_can_migrate_task(struct task_struct *p, int dst_cpu)
{
	if (!cpu_overutilized(capacity_cpu(task_cpu(p)), ml_cpu_util(task_cpu(p)))) {
		trace_ems_can_migrate_task(p, dst_cpu, false, "underutilized");
		return 0;
	}

	if (!ontime_can_migrate_task(p, dst_cpu)) {
		trace_ems_can_migrate_task(p, dst_cpu, false, "ontime");
		return 0;
	}

	if (!emstune_can_migrate_task(p, dst_cpu)) {
		trace_ems_can_migrate_task(p, dst_cpu, false, "emstune");
		return 0;
	}

	if (need_prio_pinning(p)) {
		trace_ems_can_migrate_task(p, dst_cpu, false, "prio pinning");
		return 0;
	}

	trace_ems_can_migrate_task(p, dst_cpu, true, "n/a");

	return 1;
}

static bool lb_busiest_queue_pre_condition(struct rq *rq, bool check_overutil)
{
	int cpu = cpu_of(rq);

	/* if there is no fair task, we can't balance */
	if (!rq->cfs.h_nr_running)
		return false;

	/* if rq is in the active balance, will be less busy */
	if (rq->active_balance)
		return false;

	if (check_overutil && !cpu_overutilized(capacity_cpu(cpu), ml_cpu_util(cpu)))
		return false;

	return true;
}

static inline bool lb_is_per_cpu_kthread(struct task_struct *p)
{
	if (!(p->flags & PF_KTHREAD))
		return false;

	if (p->nr_cpus_allowed != 1)
		return false;

	return true;
}

static inline bool lb_can_migrate(struct task_struct *p, int dst_cpu)
{
	struct rq *dst_rq = cpu_rq(dst_cpu);

	if (p->exit_state)
		return false;

	if (lb_is_per_cpu_kthread(p))
		return false;

	if (!cpu_active(dst_cpu))
		return false;

	if (!cpumask_test_cpu(dst_rq->cpu, p->cpus_ptr))
		return false;

	if (task_running(NULL, p))
		return false;

	return true;
}

static void attach_task(struct rq *dst_rq, struct task_struct *p)
{
	activate_task(dst_rq, p, ENQUEUE_NOCLOCK);
	check_preempt_curr(dst_rq, p, 0);
}

static void attach_one_task(struct rq *dst_rq, struct task_struct *p)
{
	struct rq_flags rf;

	rq_lock(dst_rq, &rf);
	update_rq_clock(dst_rq);
	attach_task(dst_rq, p);
	rq_unlock(dst_rq, &rf);
}

static void detach_task(struct rq *src_rq, struct rq *dst_rq, struct task_struct *p)
{
	deactivate_task(src_rq, p, DEQUEUE_NOCLOCK);
	set_task_cpu(p, dst_rq->cpu);
}

static int lb_idle_pull_tasks(int dst_cpu, int src_cpu)
{
	struct rq *dst_rq = cpu_rq(dst_cpu);
	struct rq *src_rq = cpu_rq(src_cpu);
	struct task_struct *pulled_task = NULL, *p;
	unsigned long flags;

	raw_spin_lock_irqsave(&src_rq->lock, flags);
	list_for_each_entry_reverse(p, &src_rq->cfs_tasks, se.group_node) {
		if (!ems_can_migrate_task(p, dst_cpu))
			continue;

		if (task_running(src_rq, p))
			continue;

		if (lb_can_migrate(p, dst_cpu)) {
			update_rq_clock(src_rq);
			detach_task(src_rq, dst_rq, p);
			pulled_task = p;
			break;
		}
	}

	if (!pulled_task) {
		raw_spin_unlock_irqrestore(&src_rq->lock, flags);
		return 0;
	}

	raw_spin_unlock(&src_rq->lock);

	attach_one_task(dst_rq, pulled_task);

	local_irq_restore(flags);

	return 1;
}

#define NIB_AVG_IDLE_THRESHOLD  500000
static inline bool determine_short_idle(u64 avg_idle)
{
	return avg_idle < NIB_AVG_IDLE_THRESHOLD;
}

static int get_cl_idx(int cpu)
{
	if (cpumask_test_cpu(cpu, cpu_coregroup_mask(0)))
		return 0;
	else if (cpumask_test_cpu(cpu, cpu_coregroup_mask(4)))
		return 1;
	else
		return 2;
}

static void lb_find_busiest_equivalent_queue(int dst_cpu,
		struct cpumask *src_cpus, struct rq **busiest)
{
	int src_cpu, busiest_cpu = -1;
	unsigned long util, busiest_util = 0;

	for_each_cpu_and(src_cpu, src_cpus, cpu_active_mask) {
		struct rq *src_rq = cpu_rq(src_cpu);

		trace_lb_cpu_util(src_cpu, "equal");

		if (!lb_busiest_queue_pre_condition(src_rq, false))
			continue;

		if (src_rq->nr_running < 2)
			continue;

		/* find highest util cpu */
		util = ml_cpu_util(src_cpu) + cpu_util_rt(src_rq);
		if (util < busiest_util)
			continue;

		busiest_util = util;
		busiest_cpu = src_cpu;
	}

	if (busiest_cpu != -1)
		*busiest = cpu_rq(busiest_cpu);
}

static void lb_find_busiest_slower_queue(int dst_cpu, struct cpumask *src_cpus,
				struct rq **busiest, int *is_misfit)
{
	int src_cpu, busiest_cpu = -1;
	unsigned long util, busiest_util = 0;
	unsigned long busiest_misfit_task_load = 0;

	for_each_cpu_and(src_cpu, src_cpus, cpu_active_mask) {
		struct rq *src_rq = cpu_rq(src_cpu);

		trace_lb_cpu_util(src_cpu, "slower");

		if (!lb_busiest_queue_pre_condition(src_rq, true))
			continue;

		if (src_rq->cfs.h_nr_running == 1)
			continue;

		util = ml_cpu_util(src_cpu) + cpu_util_rt(src_rq);
		if (util < busiest_util)
			continue;

		busiest_util = util;
		busiest_cpu = src_cpu;
		busiest_misfit_task_load = src_rq->misfit_task_load;
	}

	if (busiest_cpu != -1) {
		if (busiest_misfit_task_load)
			*is_misfit = true;
		*busiest = cpu_rq(busiest_cpu);
	}
}

static bool lb_group_is_busy(struct cpumask *cpus,
			int nr_task, unsigned long util)
{
	unsigned long capacity = capacity_orig_of(cpumask_first(cpus));
	int nr_cpus = cpumask_weight(cpus);
	/*
	 * if there is a available cpu in the group,
	 * this group is not busy
	 */
	if (nr_task <= nr_cpus)
		return false;

	if ((util + (util >> BUSY_GROUP_RATIO_SHIFT)) < (capacity * nr_cpus))
		return false;

	return true;
}

static void lb_find_busiest_faster_queue(int dst_cpu,
		struct cpumask *src_cpus, struct rq **busiest)
{
	int src_cpu, busiest_cpu = -1;
	struct cpumask candidate_mask;
	unsigned long util, busiest_util = 0;
	unsigned long util_sum = 0, nr_task_sum = 0;
	unsigned long tiny_task_util;

	tiny_task_util = capacity_orig_of(cpumask_first(src_cpus)) >> TINY_TASK_RATIO_SHIFT;
	cpumask_and(&candidate_mask, src_cpus, cpu_active_mask);
	cpumask_and(&candidate_mask, &candidate_mask, ecs_cpus_allowed(NULL));
	if (cpumask_empty(&candidate_mask))
		return;

	for_each_cpu(src_cpu, &candidate_mask) {
		struct rq *src_rq = cpu_rq(src_cpu);

		trace_lb_cpu_util(src_cpu, "faster");

		util = ml_cpu_util(src_cpu) + cpu_util_rt(src_rq);
		util_sum += util;
		nr_task_sum += src_rq->cfs.h_nr_running;

		if (!lb_busiest_queue_pre_condition(src_rq, true))
			continue;

		if (src_rq->cfs.h_nr_running < 2)
			continue;

		if (src_rq->cfs.h_nr_running == 2 &&
			ml_task_util(src_rq->curr) < tiny_task_util)
			continue;

		if (util < busiest_util)
			continue;

		busiest_util = util;
		busiest_cpu = src_cpu;
	}

	/*
	 * Don't allow migrating to lower cluster unless
	 * this faster cluster is sufficiently loaded.
	 */
	if (!lb_group_is_busy(&candidate_mask, nr_task_sum, util_sum))
		return;

	if (busiest_cpu != -1)
		*busiest = cpu_rq(busiest_cpu);
}

static void __lb_find_busiest_queue(int dst_cpu, struct cpumask *src_cpus,
				struct rq **busiest, int *is_misfit)
{
	unsigned long dst_capacity, src_capacity;

	src_capacity = capacity_orig_of(cpumask_first(src_cpus));
	dst_capacity = capacity_orig_of(dst_cpu);

	if (dst_capacity == src_capacity)
		lb_find_busiest_equivalent_queue(dst_cpu, src_cpus, busiest);
	else if (dst_capacity > src_capacity)
		lb_find_busiest_slower_queue(dst_cpu, src_cpus, busiest, is_misfit);
	else
		lb_find_busiest_faster_queue(dst_cpu, src_cpus, busiest);
}

static void lb_newidle_slowest_find_busiest(int dst_cpu, struct rq **busiest,
					int cl_idx, bool short_idle)
{
	struct pe_list *pl = get_pe_list(cl_idx);
	int pe_list_size = get_pe_list_size();
	int cnt;
	int is_misfit = 0;

	if (!pl)
		return;

	for (cnt = 0; cnt < pe_list_size; cnt++) {
		if (cnt > 1) {
			*busiest = NULL;
			return;
		}

		if (cnt && short_idle)
			continue;

		is_misfit = 0;
		__lb_find_busiest_queue(dst_cpu, &pl->cpus[cnt], busiest, &is_misfit);
		if (*busiest)
			return;
	}
}

static void lb_newidle_mid_find_busiest(int dst_cpu, struct rq **busiest,
					int cl_idx, bool short_idle)
{
	struct pe_list *pl = get_pe_list(cl_idx);
	int pe_list_size = get_pe_list_size();
	int cnt;
	int is_misfit = 0;

	if (!pl)
		return;

	for (cnt = 0; cnt < pe_list_size; cnt++) {
		int src_cpu = cpumask_any(&pl->cpus[cnt]);
		bool is_faster = capacity_orig_of(dst_cpu) > capacity_orig_of(src_cpu) ? true : false;

		if (cnt && short_idle)
			continue;

		is_misfit = 0;
		__lb_find_busiest_queue(dst_cpu, &pl->cpus[cnt], busiest, &is_misfit);
		if (*busiest) {
			if (!cnt)
				return;

			if (is_faster && is_misfit)
				return;
			else if (!is_faster)
				return;
			else
				*busiest = NULL;
		}
	}
}

static void lb_newidle_fastest_find_busiest(int dst_cpu, struct rq **busiest,
					int cl_idx, bool short_idle)
{
	int pe_list_size = get_pe_list_size();
	struct pe_list *pl = get_pe_list(cl_idx);
	int cnt;
	int is_misfit = 0;

	if (!pl)
		return;

	for (cnt = 0; cnt < pe_list_size; cnt++) {
		is_misfit = 0;
		__lb_find_busiest_queue(dst_cpu, &pl->cpus[cnt], busiest, &is_misfit);
		if (*busiest) {
			if (!cnt) {
				return;
			} else if (cnt == 1) {
				if (short_idle && !is_misfit)
					*busiest = NULL;
				else
					return;
			}
		}
	}
}

void lb_newidle_balance(struct rq *dst_rq, void *rf_ptr,
			int *pulled_task, int *done)
{
	struct rq *busiest = NULL;
	int dst_cpu = dst_rq->cpu;
	int src_cpu = -1;
	struct rq_flags *rf = (struct rq_flags *)rf_ptr;
	bool short_idle;
	int cl_idx = get_cl_idx(dst_cpu);

	if (unlikely(!ems_initialized))
		return;

	dst_rq->misfit_task_load = 0;
	*done = true;

	short_idle = determine_short_idle(dst_rq->avg_idle);
	if (unlikely(!ecs_cpu_available(dst_cpu, NULL)))
		return;

	dst_rq->idle_stamp = rq_clock(dst_rq);

	/*
	 * Do not pull tasks towards !active CPUs...
	 */
	if (!cpu_active(dst_cpu))
		return;

	if (dst_rq->nr_running)
		goto out;

	if (!READ_ONCE(dst_rq->rd->overload))
		goto out;

	if (atomic_read(&dst_rq->nr_iowait) && short_idle)
		goto out;

	raw_spin_unlock(&dst_rq->lock);

	lb_newidle_func[cl_idx](dst_rq->cpu, &busiest, cl_idx, short_idle);
	if (busiest) {
		src_cpu = cpu_of(busiest);
		if (dst_cpu != src_cpu && dst_rq->nr_running < 1)
			*pulled_task = lb_idle_pull_tasks(dst_cpu, src_cpu);
	}

	raw_spin_lock(&dst_rq->lock);

out:
	if (dst_rq->cfs.h_nr_running && !*pulled_task)
		*pulled_task = 1;

	/* Is there a task of a high priority class? */
	if (dst_rq->nr_running != dst_rq->cfs.h_nr_running)
		*pulled_task = -1;

	if (*pulled_task)
		dst_rq->idle_stamp = 0;

	rq_repin_lock(dst_rq, rf);

	trace_lb_newidle_balance(dst_cpu, src_cpu, *pulled_task, short_idle);
}

int exynos_select_task_rq(struct task_struct *p, int prev_cpu,
			  int sd_flag, int wake_flag)
{
	int cpu = -1;

	if (p->sched_class == &dl_sched_class)
		cpu = exynos_select_task_rq_dl(p, prev_cpu, sd_flag, wake_flag);
	else if (p->sched_class == &rt_sched_class)
		cpu = exynos_select_task_rq_rt(p, prev_cpu, sd_flag, wake_flag);
	else if (p->sched_class == &fair_sched_class)
		cpu = exynos_select_task_rq_fair(p, prev_cpu, sd_flag, wake_flag);

	if (cpu >= 0 && !is_dst_allowed(p, cpu))
		cpu = -1;

	return cpu;
}

void ems_tick(struct rq *rq)
{
	set_part_period_start(rq);
	update_cpu_active_ratio(rq, NULL, EMS_PART_UPDATE);

	if (rq->cpu == 0)
		frt_update_available_cpus(rq);

	monitor_sysbusy();
	somac_tasks();

	ontime_migration();
	ecs_update();
}

struct kobject *ems_kobj;

static ssize_t show_sched_topology(struct kobject *kobj,
		struct kobj_attribute *attr, char *buf)
{
	int cpu;
	struct sched_domain *sd;
	int ret = 0;

	rcu_read_lock();
	for_each_possible_cpu(cpu) {
		int sched_domain_level = 0;

		sd = rcu_dereference_check_sched_domain(cpu_rq(cpu)->sd);
		while (sd->parent) {
			sched_domain_level++;
			sd = sd->parent;
		}

		for (; sd; sd = sd->child) {
			ret += snprintf(buf + ret, 70,
					"[lv%d] cpu%d: flags=%#x sd->span=%#x sg->span=%#x\n",
					sched_domain_level, cpu, sd->flags,
					*(unsigned int *)cpumask_bits(sched_domain_span(sd)),
					*(unsigned int *)cpumask_bits(sched_group_span(sd->groups)));
			sched_domain_level--;
		}
		ret += snprintf(buf + ret,
				50, "----------------------------------------\n");
	}
	rcu_read_unlock();

	return ret;
}

static struct kobj_attribute sched_topology_attr =
__ATTR(sched_topology, 0444, show_sched_topology, NULL);

static ssize_t show_tiny_threshold(struct kobject *kobj,
		struct kobj_attribute *attr, char *buf)
{
	return sprintf(buf, "%d\n", tiny_threshold);
}

static ssize_t store_tiny_threshold(struct kobject *kobj,
		struct kobj_attribute *attr, const char *buf, size_t count)
{
	unsigned long threshold;

	if (!sscanf(buf, "%d", &threshold))
		return -EINVAL;

	tiny_threshold = threshold;

	return count;
}

static struct kobj_attribute tiny_threshold_attr =
__ATTR(tiny_threshold, 0644, show_tiny_threshold, store_tiny_threshold);

struct kobject *ems_drv_kobj;
static int ems_probe(struct platform_device *pdev)
{
	ems_drv_kobj = &pdev->dev.kobj;
	return 0;
}

static const struct of_device_id of_ems_match[] = {
	{ .compatible = "samsung,ems", },
	{ },
};
MODULE_DEVICE_TABLE(of, of_ems_match);

static struct platform_driver ems_driver = {
	.driver = {
		.name = "ems",
		.owner = THIS_MODULE,
		.of_match_table = of_ems_match,
	},
	.probe		= ems_probe,
};

struct delayed_work ems_init_dwork;
static void ems_delayed_init(struct work_struct *work)
{
	sysbusy_init();
}

static void pe_list_init(void)
{
	struct device_node *dn, *child;
	int index = 0;

	dn = of_find_node_by_path("/ems/pe-list");
	if (!dn) {
		pr_err("%s: Fail to get pe-list node\n", __func__);
		return;
	}

	num_of_list = of_get_child_count(dn);
	if (num_of_list == 0)
		return;

	pe_list = kmalloc_array(num_of_list, sizeof(struct pe_list), GFP_KERNEL);

	for_each_child_of_node(dn, child) {
		struct pe_list *pl = &pe_list[index++];
		const char *buf[10];
		int i;

		pl->num_of_cpus = of_property_count_strings(child, "cpus");
		if (pl->num_of_cpus == 0)
			continue;

		pl->cpus = kmalloc_array(pl->num_of_cpus, sizeof(struct cpumask), GFP_KERNEL);

		of_property_read_string_array(child, "cpus", buf, pl->num_of_cpus);
		for (i = 0; i < pl->num_of_cpus; i++)
			cpulist_parse(buf[i], &pl->cpus[i]);
	}
}

static void lb_newidle_init(void)
{
	lb_newidle_func[0] = lb_newidle_slowest_find_busiest;
	lb_newidle_func[1] = lb_newidle_mid_find_busiest;
	lb_newidle_func[2] = lb_newidle_fastest_find_busiest;
}

static int ems_init(void)
{
	int ret;

	ems_kobj = kobject_create_and_add("ems", kernel_kobj);

	ret = sysfs_create_file(ems_kobj, &sched_topology_attr.attr);
	if (ret)
		pr_warn("%s: failed to create sysfs\n", __func__);

	ret = sysfs_create_file(ems_kobj, &tiny_threshold_attr.attr);
	if (ret)
		pr_warn("%s: failed to create sysfs\n", __func__);

	hook_init();
	energy_table_init();
	part_init();
	ontime_init();
	esgov_pre_init();
	freqboost_init();
	frt_init();
	ecs_init();
	emstune_init();
	pe_list_init();
	lb_newidle_init();

	/* some api must be called after ems initialized */
	ems_initialized = true;

	/* booting lock for sysbusy, it is expired after 40s */
	INIT_DELAYED_WORK(&ems_init_dwork, ems_delayed_init);
	schedule_delayed_work(&ems_init_dwork,
			msecs_to_jiffies(40 * MSEC_PER_SEC));

	return platform_driver_register(&ems_driver);
}
core_initcall(ems_init);
