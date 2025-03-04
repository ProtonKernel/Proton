/*
 * Scheduling in busy system feature for Exynos Mobile Scheduler (EMS)
 *
 * Copyright (C) 2020 Samsung Electronics Co., Ltd
 * Choonghoon Park <choong.park@samsung.com>
 */

#include "../sched.h"
#include "ems.h"

#include <trace/events/ems.h>
#include <trace/events/ems_debug.h>

static int sysbusy_initialized;

enum sysbusy_state {
	SYSBUSY_STATE0 = 0,
	SYSBUSY_STATE1,
	SYSBUSY_STATE2,
	SYSBUSY_STATE3,
	NUM_OF_SYSBUSY_STATE,
};

struct sysbusy {
	raw_spinlock_t lock;
	u64 last_update_time;
	u64 boost_duration;
	u64 release_duration;
	enum sysbusy_state state;
	struct cpumask fastest_cpus;
	struct work_struct work;
} sysbusy;

struct somac_env {
	struct rq		*dst_rq;
	int			dst_cpu;
	struct rq		*src_rq;
	int			src_cpu;
	struct task_struct	*target_task;
};
DEFINE_PER_CPU(struct somac_env, somac_env);

struct sysbusy_stat {
	int count;
	u64 last_time;
	u64 time_in_state;
} sysbusy_stats[NUM_OF_SYSBUSY_STATE];

#define TRACK_TASK_COUNT	5
/**********************************************************************
 *			    Hanlding States			      *
 **********************************************************************/
static int sysbusy_find_fastest_cpu(struct tp_env *env)
{
	struct cpumask mask;
	int cpu;

	if (cpumask_empty(&env->cpus_allowed))
		return cpumask_last(env->p->cpus_ptr);

	cpumask_and(&mask, &sysbusy.fastest_cpus, &env->cpus_allowed);
	if (cpumask_empty(&mask))
		cpu = cpumask_last(&env->cpus_allowed);
	else
		cpu = cpumask_any(&mask);

	return cpu;
}

static unsigned long
get_remained_task_util(int cpu, struct tp_env *env)
{
	struct task_struct *p, *excluded = env->p;
	struct rq *rq = cpu_rq(cpu);
	struct sched_entity *se;
	unsigned long util = capacity_cpu_orig(cpu), util_sum = 0;
	unsigned long flags;
	int task_count = 0;

	if (env->wake || cpu != task_cpu(excluded))
		raw_spin_lock_irqsave(&rq->lock, flags);

	if (!rq->cfs.curr) {
		util = 0;
		goto unlock;
	}

	/* Find task entity if entity is cfs_rq. */
	se = rq->cfs.curr;
	se = get_task_entity(se);

	/*
	 * Since current task does not exist in entity list of cfs_rq,
	 * check first that current task is heavy.
	 */
	p = container_of(se, struct task_struct, se);
	if (p != excluded) {
		util = ml_task_util(p);
		util_sum += util;
	}

	se = __pick_first_entity(cfs_rq_of(se));
	while (se && task_count < TRACK_TASK_COUNT) {
		/* Skip non-task entity */
		if (!entity_is_task(se))
			goto next_entity;

		p = container_of(se, struct task_struct, se);
		if (p == excluded)
			goto next_entity;

		util = ml_task_util(p);
		util_sum += util;

next_entity:
		se = __pick_next_entity(se);
		task_count++;
	}

unlock:
	if (env->wake || cpu != task_cpu(excluded))
		raw_spin_unlock_irqrestore(&rq->lock, flags);

	return min_t(unsigned long, util, capacity_cpu_orig(cpu));
}

static int sysbusy_find_min_util_cpu(struct tp_env *env)
{
	int cpu, min_cpu = -1;
	unsigned long min_util = INT_MAX;

	for_each_cpu(cpu, &env->cpus_allowed) {
		unsigned long cpu_util;

		cpu_util = env->cpu_util_wo[cpu];

		/* Add rt util */
		cpu_util += env->cpu_rt_util[cpu];

		if (cpu == task_cpu(env->p) && !cpu_util)
			cpu_util = get_remained_task_util(cpu, env);

		/* find min util cpu with rt util */
		if (cpu_util <= min_util) {
			min_util = cpu_util;
			min_cpu = cpu;
		}
	}

	return min_cpu;
}

#define MISFIT_TASK_UTIL_RATIO	(80)
#define MISFIT_TASK_THRESHOLD	((SCHED_CAPACITY_SCALE * MISFIT_TASK_UTIL_RATIO) / 100)
static int decision_somac_task(struct task_struct *p);
static unsigned long sysbusy_find_misfit_task_util(int cpu)
{
	struct rq *rq = cpu_rq(cpu);
	struct task_struct *p;
	int task_count = 0;
	unsigned long util, misfit_util = 0, flags;

	raw_spin_lock_irqsave(&rq->lock, flags);
	list_for_each_entry(p, &rq->cfs_tasks, se.group_node) {
		if (decision_somac_task(p)) {
			util = ml_task_util(p);
			if (util > MISFIT_TASK_THRESHOLD && util > misfit_util)
				misfit_util = util;
		}

		if (task_count++ >= TRACK_TASK_COUNT)
			break;
	}
	raw_spin_unlock_irqrestore(&rq->lock, flags);

	return misfit_util;
}

static bool sysbusy_need_max_perf(int busy_cpu)
{
	int cpu, cpu_util_sum = 0, busy_cpu_util = -1;
	unsigned long misfit_util;

	if (busy_cpu < 0)
		return false;

	misfit_util = sysbusy_find_misfit_task_util(busy_cpu);
	if (!misfit_util)
		return false;

	for_each_cpu(cpu, cpu_active_mask) {
		int cpu_util = ml_cpu_util(cpu);

		/* Add rt util */
		cpu_util += cpu_util_rt(cpu_rq(cpu));

		if (cpu == busy_cpu)
			busy_cpu_util = cpu_util;

		cpu_util_sum += cpu_util;
	}

	return busy_cpu_util * 100 >= cpu_util_sum * 80;
}

static int sysbusy_is_heavy_task(struct tp_env *env)
{
	int cpu, cpu_util_sum = 0;

	for_each_cpu(cpu, cpu_active_mask) {
		int cpu_util = env->cpu_util[cpu];

		/* Add rt util */
		cpu_util += cpu_util_rt(cpu_rq(cpu));

		cpu_util_sum += cpu_util;
	}

	return env->task_util * 100 >= cpu_util_sum * 80;
}

/**********************************************************************
 *			    SOMAC				      *
 **********************************************************************/
static int decision_somac_task(struct task_struct *p)
{
	return cpuctl_task_group_idx(p) == STUNE_TOPAPP ? 1 : 0;
}

static struct task_struct *
pick_somac_task(struct sched_entity *se)
{
	struct task_struct *p, *heaviest_task = container_of(se, struct task_struct, se);
	unsigned long util, max_util = ml_task_util(heaviest_task);
	int task_count = 0;
#ifdef CONFIG_FAIR_GROUP_SCHED
	struct rq *rq = cpu_rq(task_cpu(heaviest_task));

	max_util = decision_somac_task(heaviest_task) ? max_util : 0;

	list_for_each_entry(p, &rq->cfs_tasks, se.group_node) {
		if (decision_somac_task(p)) {
			util = ml_task_util(p);
			if (util > max_util) {
				heaviest_task = p;
				max_util = util;
			}
		}

		if (task_count++ >= TRACK_TASK_COUNT)
			break;
	}
#else
	max_util = decision_somac_task(heaviest_task) ? max_util : 0;

	se = __pick_first_entity(cfs_rq_of(se));
	while (se && task_count < TRACK_TASK_COUNT) {

		/* Skip non-task entity */
		if (!entity_is_task(se))
			goto next_entity;

		p = container_of(se, struct task_struct, se);

		/*
		 * Pick the task with the biggest runnable among tasks whose
		 * runnable is greater than the upper boundary.
		 */
		if (decision_somac_task(p)) {
			util = ml_task_util(p);
			if (util > max_util) {
				heaviest_task = p;
				max_util = util;
			}
		}

next_entity:
		se = __pick_next_entity(se);
		task_count++;
	}
#endif

	return heaviest_task;
}

static bool can_move_task(struct task_struct *p, struct somac_env *env)
{
	struct rq *src_rq = env->src_rq;
	int src_cpu = env->src_cpu;

	if (p->exit_state)
		return false;

	if (unlikely(src_rq != task_rq(p)))
		return false;

	if (unlikely(src_cpu != smp_processor_id()))
		return false;

	if (src_rq->nr_running <= 1)
		return false;

	if (!cpumask_test_cpu(env->dst_cpu, p->cpus_ptr))
		return false;

	if (task_running(env->src_rq, p))
		return false;

	return true;
}

static int
move_somac_task(struct task_struct *target, struct somac_env *env)
{
	struct list_head *tasks = &env->src_rq->cfs_tasks;
	struct task_struct *p, *n;

	list_for_each_entry_safe(p, n, tasks, se.group_node) {
		if (p != target)
			continue;

		p->on_rq = TASK_ON_RQ_MIGRATING;
		deactivate_task(env->src_rq, p, 0);
		set_task_cpu(p, env->dst_cpu);

		activate_task(env->dst_rq, p, 0);
		p->on_rq = TASK_ON_RQ_QUEUED;
		check_preempt_curr(env->dst_rq, p, 0);

		return 1;
	}

	return 0;
}

static int somac_cpu_stop(void *data)
{
	struct somac_env *env = data;
	struct rq *src_rq, *dst_rq;
	struct task_struct *p;
	int src_cpu, dst_cpu;

	/* Initialize environment data */
	src_rq = env->src_rq;
	dst_rq = env->dst_rq = cpu_rq(env->dst_cpu);
	src_cpu = env->src_cpu = env->src_rq->cpu;
	dst_cpu = env->dst_cpu;
	p = env->target_task;

	if (!cpumask_test_cpu(src_cpu, cpu_active_mask) ||
	    !cpumask_test_cpu(dst_cpu, cpu_active_mask))
		return -1;

	raw_spin_lock_irq(&src_rq->lock);

	/* Check task can be migrated */
	if (!can_move_task(p, env))
		goto out_unlock;

	BUG_ON(src_rq == dst_rq);

	/* Move task from source to destination */
	double_lock_balance(src_rq, dst_rq);
	if (move_somac_task(p, env)) {
		trace_somac(p, ml_task_util(p), src_cpu, dst_cpu);
	}
	double_unlock_balance(src_rq, dst_rq);

out_unlock:

	src_rq->active_balance = 0;

	raw_spin_unlock_irq(&src_rq->lock);
	put_task_struct(p);

	return 0;
}

#define HEAVY_TASK_UTIL_RATIO	(40)
#define HEAVY_TASK_THRESHOLD	((SCHED_CAPACITY_SCALE * HEAVY_TASK_UTIL_RATIO) / 100)
static int
heavy_tasks_tracking(int *heavy_task_count, unsigned long *heavy_task_util_sum)
{
	struct task_struct *p;
	unsigned long flags;
	int cpu;
	int local_count = 0;
	unsigned long local_util_sum = 0;

	for_each_online_cpu(cpu) {
		struct rq *rq = cpu_rq(cpu);
		unsigned long util;
		int task_count = 0;
#ifndef CONFIG_FAIR_GROUP_SCHED
		struct sched_entity *se;
#endif

		raw_spin_lock_irqsave(&rq->lock, flags);

		if (!rq->cfs.curr)
			goto unlock;

#ifndef CONFIG_FAIR_GROUP_SCHED
		/*
		 * Find sched entity which is "task", not "cfs_rq"
		 */
		se = rq->cfs.curr;
		se = get_task_entity(se);

		/*
		 * Since current task does not exist in entity list of cfs_rq,
		 * check first that current task is heavy.
		 */
		p = container_of(se, struct task_struct, se);

		if (decision_somac_task(p)) {
			util = ml_task_util(p);
			if (util > HEAVY_TASK_THRESHOLD) {
				local_count++;
				local_util_sum += util;
			}
		}

		se = __pick_first_entity(cfs_rq_of(se));
		while (se && task_count < TRACK_TASK_COUNT) {
			/* Skip non-task entity */
			if (!entity_is_task(se))
				goto next_entity;

			p = container_of(se, struct task_struct, se);

			/*
			 * Count the number of tasks whose
			 * utilizations are greater than heavy task threahold.
			 */

			if (decision_somac_task(p)) {
				util = ml_task_util(p);
				if (util > HEAVY_TASK_THRESHOLD) {
					local_count++;
					local_util_sum += util;
				}
			}

next_entity:
			se = __pick_next_entity(se);
			task_count++;
		}
#else
		list_for_each_entry(p, &rq->cfs_tasks, se.group_node) {
			if (decision_somac_task(p)) {
				util = ml_task_util(p);
				if (util > HEAVY_TASK_THRESHOLD) {
					local_count++;
					local_util_sum += util;
				}
			}

			if (task_count++ >= TRACK_TASK_COUNT)
				break;
		}
#endif
unlock:
		raw_spin_unlock_irqrestore(&rq->lock, flags);
	}

	*heavy_task_count = local_count;
	*heavy_task_util_sum = local_util_sum;

	return 0;
}

/**********************************************************************
 *			    External APIs			      *
 **********************************************************************/
static unsigned long last_somac;
static int interval = 1; /* 1tick = 4ms */
static DEFINE_SPINLOCK(somac_lock);
static DEFINE_PER_CPU(struct cpu_stop_work, somac_work);

static int __somac_tasks(int src_cpu, int dst_cpu,
				struct rq *src_rq, struct rq *dst_rq)
{
	struct somac_env *env = &per_cpu(somac_env, src_cpu);
	unsigned long flags;
	struct sched_entity *se;
	struct task_struct *p;

	if (!cpumask_test_cpu(src_cpu, cpu_active_mask) ||
	    !cpumask_test_cpu(dst_cpu, cpu_active_mask))
		return -1;

	raw_spin_lock_irqsave(&src_rq->lock, flags);
	if (src_rq->active_balance) {
		raw_spin_unlock_irqrestore(&src_rq->lock, flags);
		return -1;
	}

	if (!src_rq->cfs.curr) {
		raw_spin_unlock_irqrestore(&src_rq->lock, flags);
		return -1;
	}

	se = src_rq->cfs.curr;
	se = get_task_entity(se);

	p = pick_somac_task(se);
	if (!p) {
		raw_spin_unlock_irqrestore(&src_rq->lock, flags);
		return -1;
	}

	get_task_struct(p);

	env->dst_cpu = dst_cpu;
	env->src_rq = src_rq;
	env->target_task = p;

	src_rq->active_balance = 1;

	raw_spin_unlock_irqrestore(&src_rq->lock, flags);

	stop_one_cpu_nowait(src_cpu, somac_cpu_stop, env,
			&per_cpu(somac_work, src_cpu));

	return 0;
}

void somac_tasks(void)
{
	int slow_cpu, fast_cpu, cpu;
	unsigned long now = jiffies;
	struct rq *slow_rq, *fast_rq;
	int busy_cpu = -1, idle_cpu = -1;
	unsigned long flags;

	if (unlikely(!sysbusy_initialized))
		return;

	if (!spin_trylock(&somac_lock))
		return;

	raw_spin_lock_irqsave(&sysbusy.lock, flags);
	if (sysbusy.state != SYSBUSY_STATE3) {
		raw_spin_unlock_irqrestore(&sysbusy.lock, flags);
		goto unlock;
	}
	raw_spin_unlock_irqrestore(&sysbusy.lock, flags);

	if (now < last_somac + interval)
		goto unlock;

	if (cpumask_weight(cpu_active_mask) != VENDOR_NR_CPUS ||
	    cpumask_weight(ecs_cpus_allowed(NULL)) != VENDOR_NR_CPUS)
		goto unlock;

	for_each_online_cpu(cpu) {
		if (cpu_rq(cpu)->nr_running > 1)
			busy_cpu = cpu;
		if (!cpu_rq(cpu)->nr_running)
			idle_cpu = cpu;
	}

	if (busy_cpu >= 0 && idle_cpu >= 0) {
		slow_rq = cpu_rq(busy_cpu);
		fast_rq = cpu_rq(idle_cpu);

		/* MIGRATE TASK BUSY -> IDLE */
		__somac_tasks(busy_cpu, idle_cpu, slow_rq, fast_rq);
		goto out;
	}

	for_each_cpu(cpu, cpu_coregroup_mask(0)) {
		slow_cpu = cpu;
		fast_cpu = cpu + 4;

		slow_rq = cpu_rq(slow_cpu);
		fast_rq = cpu_rq(fast_cpu);

		/* MIGRATE TASK SLOW -> FAST */
		if (__somac_tasks(slow_cpu, fast_cpu, slow_rq, fast_rq))
			continue;

		slow_cpu = (fast_cpu - 1) % 4;
		slow_rq = cpu_rq(slow_cpu);

		/* MIGRATE TASK FAST -> SLOW */
		__somac_tasks(fast_cpu, slow_cpu, fast_rq, slow_rq);
	}

out:
	last_somac = now;
unlock:
	spin_unlock(&somac_lock);
}

int sysbusy_on_somac(void)
{
	return sysbusy.state == SYSBUSY_STATE3;
}

int is_sysbusy(void)
{
	return sysbusy.state != SYSBUSY_STATE0;
}

static void
update_sysbusy_stat(int old_state, int next_state, unsigned long now)
{
	if (old_state == next_state)
		return;

	sysbusy_stats[old_state].count++;
	sysbusy_stats[old_state].time_in_state +=
		now - sysbusy_stats[old_state].last_time;

	sysbusy_stats[next_state].last_time = now;
}

static void change_sysbusy_state(int next_state, unsigned long now)
{
	int old_state = sysbusy.state;

	if (next_state <= SYSBUSY_STATE1)
		sysbusy.boost_duration = 0;
	else
		sysbusy.boost_duration = 250;	/* 250HZ = 1s */

	sysbusy.release_duration = 0;
	sysbusy.state = next_state;

	trace_ems_sysbusy_boost(next_state);
	update_sysbusy_stat(old_state, next_state, now);
}

#define SHUFFLING_TRIGGER_RANGE_LEFT	(6)
#define SHUFFLING_TRIGGER_RANGE_RIGHT	(10)
#define SHUFFLING_STOP_MORE_THAN	(11)
#define SHUFFLING_STOP_LESS_THAN	(4)
#define RELEASE_THRESHOLD		(2)
#define in_range(v, l, r)	((l) <= (v) && (v) <= (r))
void monitor_sysbusy(void)
{
	int cpu, busy_count = 0, busy_cpu = -1;
	unsigned long now = jiffies;
	enum sysbusy_state old_state;
	unsigned long flags;

	if (unlikely(!sysbusy_initialized))
		return;

	if (!raw_spin_trylock_irqsave(&sysbusy.lock, flags))
		return;

	if (now <= sysbusy.last_update_time + sysbusy.boost_duration)
		goto out;

	sysbusy.last_update_time = now;

	for_each_online_cpu(cpu) {
		/* count busy cpu */
		if (ml_cpu_util(cpu) * 100 >= capacity_cpu(cpu) * 80) {
			busy_count++;
			busy_cpu = cpu;
		}

		trace_ems_monitor_sysbusy(cpu, ml_cpu_util(cpu));
	}

	old_state = sysbusy.state;
	if (old_state <= SYSBUSY_STATE1 &&
			busy_count == 1 && sysbusy_need_max_perf(busy_cpu)) {
		change_sysbusy_state(SYSBUSY_STATE1, now);
	} else if (busy_count >= VENDOR_NR_CPUS) {
		int heavy_task_count;
		unsigned long heavy_task_util_sum;

		heavy_tasks_tracking(&heavy_task_count, &heavy_task_util_sum);

		trace_ems_heavy_tasks_tracking(heavy_task_count, heavy_task_util_sum);

		/* Check sum of utilizatins of heavy tasks */
		if (heavy_task_util_sum * 100 <
				(SCHED_CAPACITY_SCALE * VENDOR_NR_CPUS) * 80)
			goto check_release;

		/* Check the number of heavy tasks */
		if (in_range(heavy_task_count,
			SHUFFLING_TRIGGER_RANGE_LEFT, SHUFFLING_TRIGGER_RANGE_RIGHT)) {
			/* Start somac and freq boosting */
			change_sysbusy_state(SYSBUSY_STATE3, now);
		} else if (in_range(heavy_task_count,
			SHUFFLING_STOP_MORE_THAN, VENDOR_NR_CPUS * 2)) {
			/* No somac but freq boosting */
			change_sysbusy_state(SYSBUSY_STATE2, now);
		} else if (heavy_task_count <= SHUFFLING_STOP_LESS_THAN) {
			/* Neither somac or freq boosting */
			sysbusy.release_duration++;
		}
	} else
		sysbusy.release_duration++;

check_release:
	if (sysbusy.release_duration >= RELEASE_THRESHOLD)
		change_sysbusy_state(SYSBUSY_STATE0, now);

	if ((old_state == SYSBUSY_STATE0 && sysbusy.state > SYSBUSY_STATE0) ||
		(old_state > SYSBUSY_STATE0 && sysbusy.state == SYSBUSY_STATE0))
		schedule_work(&sysbusy.work);

out:
	raw_spin_unlock_irqrestore(&sysbusy.lock, flags);
}

int sysbusy_schedule(struct tp_env *env)
{
	int target_cpu = -1;

	if (unlikely(!sysbusy_initialized))
		return task_cpu(env->p);

	switch (sysbusy.state) {
	case SYSBUSY_STATE1:
		if (sysbusy_is_heavy_task(env))
			target_cpu = sysbusy_find_fastest_cpu(env);
		else
			sysbusy_find_min_util_cpu(env);
		break;
	case SYSBUSY_STATE2:
		target_cpu = sysbusy_find_min_util_cpu(env);
		break;
	case SYSBUSY_STATE3:
		if (ml_task_util(env->p) > HEAVY_TASK_THRESHOLD)
			target_cpu = task_cpu(env->p);
		else
			target_cpu = sysbusy_find_min_util_cpu(env);
		break;
	case SYSBUSY_STATE0:
	default:
		;
	}

	return target_cpu;
}

/**********************************************************************
 *			    SYSFS support			      *
 **********************************************************************/
static ssize_t show_somac_interval(struct kobject *kobj,
		struct kobj_attribute *attr, char *buf)
{
	return snprintf(buf, 10, "%d\n", interval);
}

static ssize_t store_somac_interval(struct kobject *kobj,
	struct kobj_attribute *attr, const char *buf, size_t count)
{
	int input;

	if (sscanf(buf, "%d", &input) != 1)
		return -EINVAL;

	interval = input;

	return count;
}

static struct kobj_attribute somac_interval =
__ATTR(somac_interval, 0644, show_somac_interval, store_somac_interval);

static ssize_t show_sysbusy_stat(struct kobject *kobj,
		struct kobj_attribute *attr, char *buf)
{
	int i, count = 0;

	for (i = SYSBUSY_STATE1; i < NUM_OF_SYSBUSY_STATE; i++)
		count += snprintf(buf + count, PAGE_SIZE - count,
				"[state%d] count:%d time_in_state=%lldms\n",
				i, sysbusy_stats[i].count,
				jiffies_to_msecs(sysbusy_stats[i].time_in_state));

	return count;
}

static struct kobj_attribute sysbusy_stat_attr =
__ATTR(sysbusy_stat, 0644, show_sysbusy_stat, NULL);

/**********************************************************************
 *			    Initialization			      *
 **********************************************************************/
static void sysbusy_boost_fn(struct work_struct *work)
{
	char msg[32];
	char *envp[] = { msg, NULL };
	int cur_state;
	unsigned long flags;

	raw_spin_lock_irqsave(&sysbusy.lock, flags);
	cur_state = sysbusy.state;
	raw_spin_unlock_irqrestore(&sysbusy.lock, flags);

	snprintf(msg, sizeof(msg), "SYSBUSY=%d", cur_state);
	if (kobject_uevent_env(ems_drv_kobj, KOBJ_CHANGE, envp))
		pr_warn("Failed to send uevent for sysbusy\n");
}

static void init_fastest_cpus(void)
{
	int cpu, max_cpu = -1;
	unsigned long  max_cap = 0;

	for_each_cpu(cpu, cpu_active_mask) {
		unsigned long cap;

		if (cpu != cpumask_first(cpu_coregroup_mask(cpu)))
			continue;

		cap = capacity_cpu_orig(cpu);
		if (cap > max_cap) {
			max_cpu = cpu;
			max_cap = cap;
		}
	}

	if (max_cpu >= 0)
		cpumask_copy(&sysbusy.fastest_cpus, cpu_coregroup_mask(max_cpu));
	else
		cpumask_copy(&sysbusy.fastest_cpus, cpu_active_mask);
}

int sysbusy_init(void)
{
	int ret;

	raw_spin_lock_init(&sysbusy.lock);
	INIT_WORK(&sysbusy.work, sysbusy_boost_fn);

	init_fastest_cpus();

	ret = sysfs_create_file(ems_kobj, &somac_interval.attr);
	if (ret)
		pr_warn("%s: failed to create somac sysfs\n", __func__);

	ret = sysfs_create_file(ems_kobj, &sysbusy_stat_attr.attr);
	if (ret)
		pr_warn("%s: failed to create somac sysfs\n", __func__);

	sysbusy_initialized = true;

	return 0;
}
