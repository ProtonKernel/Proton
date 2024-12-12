/*
 * Multi-purpose Load tracker
 *
 * Copyright (C) 2018 Samsung Electronics Co., Ltd
 * Park Bumgyu <bumgyu.park@samsung.com>
 */

#include "../sched.h"
#include "../sched-pelt.h"
#include "ems.h"

#include <trace/events/ems.h>
#include <trace/events/ems_debug.h>

/******************************************************************************
 *                           MULTI LOAD for TASK                              *
 ******************************************************************************/
/*
 * ml_task_util - task util
 *
 * Task utilization. The calculation is the same as the task util of cfs.
 */
unsigned long ml_task_util(struct task_struct *p)
{
	return READ_ONCE(p->se.avg.util_avg);
}

int ml_task_hungry(struct task_struct *p)
{
	return 0; /* HACK */
}

#define UTIL_AVG_UNCHANGED 0x1

/*
 * ml_task_util_est - task util with util-est
 *
 * Task utilization with util-est, The calculation is the same as
 * task_util_est of cfs.
 */
static unsigned long _ml_task_util_est(struct task_struct *p)
{
	struct util_est ue = READ_ONCE(p->se.avg.util_est);

	return (max(ue.ewma, ue.enqueued) | UTIL_AVG_UNCHANGED);
}

unsigned long ml_task_util_est(struct task_struct *p)
{
	return max(ml_task_util(p), _ml_task_util_est(p));
}

/******************************************************************************
 *                            MULTI LOAD for CPU                              *
 ******************************************************************************/
/*
 * ml_cpu_util - cpu utilization
 */
unsigned int sysctl_sched_util_est_clamp = 100;
unsigned long ml_cpu_util(int cpu)
{
	struct cfs_rq *cfs_rq;
	unsigned int util, util_est;

	cfs_rq = &cpu_rq(cpu)->cfs;
	util = READ_ONCE(cfs_rq->avg.util_avg);

	if (sched_feat(UTIL_EST)) {
		util_est = READ_ONCE(cfs_rq->avg.util_est.enqueued) * sysctl_sched_util_est_clamp / 100;
		util = max(util, util_est);
	}

	return min_t(unsigned long, util, capacity_cpu_orig(cpu));
}

/*
 * Remove and clamp on negative, from a local variable.
 *
 * A variant of sub_positive(), which does not use explicit load-store
 * and is thus optimized for local variable updates.
 */
#define lsub_positive(_ptr, _val) do {				\
	typeof(_ptr) ptr = (_ptr);				\
	*ptr -= min_t(typeof(*ptr), *ptr, _val);		\
} while (0)

/*
 * ml_cpu_util_without - cpu utilization without waking task
 *
 * Cpu utilization with any contributions from the waking task p removed.
 */
unsigned long ml_cpu_util_without(int cpu, struct task_struct *p)
{
	struct cfs_rq *cfs_rq;
	unsigned int util;

	/* Task has no contribution or is new */
	if (cpu != task_cpu(p) || !READ_ONCE(p->se.avg.last_update_time))
		return ml_cpu_util(cpu);

	cfs_rq = &cpu_rq(cpu)->cfs;
	util = READ_ONCE(cfs_rq->avg.util_avg);

	/* Discount task's util from CPU's util */
	lsub_positive(&util, ml_task_util(p));

	/*
	 * Covered cases:
	 *
	 * a) if *p is the only task sleeping on this CPU, then:
	 *      cpu_util (== task_util) > util_est (== 0)
	 *    and thus we return:
	 *      cpu_util_without = (cpu_util - task_util) = 0
	 *
	 * b) if other tasks are SLEEPING on this CPU, which is now exiting
	 *    IDLE, then:
	 *      cpu_util >= task_util
	 *      cpu_util > util_est (== 0)
	 *    and thus we discount *p's blocked utilization to return:
	 *      cpu_util_without = (cpu_util - task_util) >= 0
	 *
	 * c) if other tasks are RUNNABLE on that CPU and
	 *      util_est > cpu_util
	 *    then we use util_est since it returns a more restrictive
	 *    estimation of the spare capacity on that CPU, by just
	 *    considering the expected utilization of tasks already
	 *    runnable on that CPU.
	 *
	 * Cases a) and b) are covered by the above code, while case c) is
	 * covered by the following code when estimated utilization is
	 * enabled.
	 */
	if (sched_feat(UTIL_EST)) {
		unsigned int estimated =
			READ_ONCE(cfs_rq->avg.util_est.enqueued);

		/*
		 * Despite the following checks we still have a small window
		 * for a possible race, when an execl's select_task_rq_fair()
		 * races with LB's detach_task():
		 *
		 *   detach_task()
		 *     p->on_rq = TASK_ON_RQ_MIGRATING;
		 *     ---------------------------------- A
		 *     deactivate_task()                   \
		 *       dequeue_task()                     + RaceTime
		 *         util_est_dequeue()              /
		 *     ---------------------------------- B
		 *
		 * The additional check on "current == p" it's required to
		 * properly fix the execl regression and it helps in further
		 * reducing the chances for the above race.
		 */
		if (unlikely(task_on_rq_queued(p) || current == p))
			lsub_positive(&estimated, _ml_task_util_est(p));

		util = max(util, estimated);
	}

	/*
	 * Utilization (estimated) can exceed the CPU capacity, thus let's
	 * clamp to the maximum CPU capacity to ensure consistency with
	 * the cpu_util call.
	 */
	return min_t(unsigned long, util, capacity_cpu_orig(cpu));
}

/*
 * Unsigned subtract and clamp on underflow.
 *
 * Explicitly do a load-store to ensure the intermediate value never hits
 * memory. This allows lockless observations without ever seeing the negative
 * values.
 */
#define sub_positive(_ptr, _val) do {				\
	typeof(_ptr) ptr = (_ptr);				\
	typeof(*ptr) val = (_val);				\
	typeof(*ptr) res, var = READ_ONCE(*ptr);		\
	res = var - val;					\
	if (res > var)						\
		res = 0;					\
	WRITE_ONCE(*ptr, res);					\
} while (0)

/*
 * Predicts what cpu_util(@cpu) would return if @p was migrated (and enqueued)
 * to @dst_cpu.
 */
unsigned long ml_cpu_util_next(int cpu, struct task_struct *p, int dst_cpu)
{
	struct cfs_rq *cfs_rq = &cpu_rq(cpu)->cfs;
	unsigned long util_est, util = READ_ONCE(cfs_rq->avg.util_avg);

	/*
	 * If @p migrates from @cpu to another, remove its contribution. Or,
	 * if @p migrates from another CPU to @cpu, add its contribution. In
	 * the other cases, @cpu is not impacted by the migration, so the
	 * util_avg should already be correct.
	 */
	if (task_cpu(p) == cpu && dst_cpu != cpu)
		sub_positive(&util, ml_task_util(p));
	else if (task_cpu(p) != cpu && dst_cpu == cpu)
		util += ml_task_util(p);

	if (sched_feat(UTIL_EST)) {
		util_est = READ_ONCE(cfs_rq->avg.util_est.enqueued);

		/*
		 * During wake-up, the task isn't enqueued yet and doesn't
		 * appear in the cfs_rq->avg.util_est.enqueued of any rq,
		 * so just add it (if needed) to "simulate" what will be
		 * cpu_util() after the task has been enqueued.
		 */
		if (dst_cpu == cpu)
			util_est += _ml_task_util_est(p);

		util = max(util, util_est);
	}

	return min(util, capacity_cpu_orig(cpu));
}

/*
 * ml_boosted_cpu_util - cpu utilization with boost
 */
unsigned long ml_boosted_cpu_util(int cpu)
{
	int boost = freqboost_cpu_boost(cpu);
	unsigned long util = ml_cpu_util(cpu);

	if (boost == 0)
		return util;

	return util + freqboost_margin(capacity_cpu(cpu), util, boost);
}

/****************************************************************/
/*		Periodic Active Ratio Tracking			*/
/****************************************************************/
static int part_initialized;
static __read_mostly u64 period_size = 4 * NSEC_PER_MSEC;
static __read_mostly u64 period_hist_size = 10;

static DEFINE_PER_CPU(struct part, part);

static inline int inc_hist_idx(int idx)
{
	return (idx + 1) % period_hist_size;
}

static void
update_cpu_active_ratio_hist(struct part *pa, bool full, unsigned int count)
{
	/* We don't iterate part histogram by above PART_HIST_SIZE_MAX */
	count = count > PART_HIST_SIZE_MAX ? PART_HIST_SIZE_MAX : count;

	/*
	 * Reflect recent active ratio in the history.
	 */
	pa->hist_idx = inc_hist_idx(pa->hist_idx);
	pa->hist[pa->hist_idx] = pa->active_ratio_recent;

	/*
	 * If count is positive, there are empty/full periods.
	 * These will be reflected in the history.
	 */
	while (count--) {
		pa->hist_idx = inc_hist_idx(pa->hist_idx);
		pa->hist[pa->hist_idx] = full ? SCHED_CAPACITY_SCALE : 0;
	}
}

static void
__update_cpu_active_ratio(int cpu, struct part *pa, u64 now)
{
	u64 elapsed = now - pa->period_start;
	unsigned int period_count = 0;

	if (pa->running) {
		/*
		 * If 'pa->running' is true, it means that the rq is active
		 * from last_update until now.
		 */
		u64 contributer, remainder;

		/*
		 * If now is in recent period, contributer is from last_updated to now.
		 * Otherwise, it is from last_updated to period_end
		 * and remaining active time will be reflected in the next step.
		 */
		contributer = min(now, pa->period_start + period_size);
		pa->active_sum += contributer - pa->last_updated;
		pa->active_ratio_recent =
			div64_u64(pa->active_sum << SCHED_CAPACITY_SHIFT, period_size);

		/*
		 * If now has passed recent period, calculate full periods and reflect they.
		 */
		period_count = div64_u64_rem(elapsed, period_size, &remainder);
		if (period_count) {
			update_cpu_active_ratio_hist(pa, true, period_count - 1);
			pa->active_sum = remainder;
			pa->active_ratio_recent =
				div64_u64(pa->active_sum << SCHED_CAPACITY_SHIFT, period_size);
		}
	} else {
		/*
		 * If 'pa->running' is false, it means that the rq is idle
		 * from last_update until now.
		 */

		/*
		 * If now has passed recent period, calculate empty periods and reflect they.
		 */
		period_count = div64_u64(elapsed, period_size);
		if (period_count) {
			update_cpu_active_ratio_hist(pa, false, period_count - 1);
			pa->active_ratio_recent = 0;
			pa->active_sum = 0;
		}
	}

	pa->period_start += period_size * period_count;
	pa->last_updated = now;
}

void update_cpu_active_ratio(struct rq *rq, struct task_struct *p, int type)
{
	int cpu = cpu_of(rq);
	struct part *pa = &per_cpu(part, cpu);
	u64 now = sched_clock_cpu(0);

	if (unlikely(pa->period_start == 0))
		return;

	switch (type) {
	/*
	 * 1) Enqueue
	 * This type is called when the rq is switched from idle to running.
	 * In this time, Update the active ratio for the idle interval
	 * and change the state to running.
	 */
	case EMS_PART_ENQUEUE:
		__update_cpu_active_ratio(cpu, pa, now);

		if (rq->nr_running == 0) {
			pa->running = true;
			trace_multi_load_update_cpu_active_ratio(cpu, pa, "enqueue");
		}
		break;
	/*
	 * 2) Dequeue
	 * This type is called when the rq is switched from running to idle.
	 * In this time, Update the active ratio for the running interval
	 * and change the state to not-running.
	 */
	case EMS_PART_DEQUEUE:
		__update_cpu_active_ratio(cpu, pa, now);

		if (rq->nr_running == 1) {
			pa->running = false;
			trace_multi_load_update_cpu_active_ratio(cpu, pa, "dequeue");
		}
		break;
	/*
	 * 3) Update
	 * This type is called to update the active ratio during rq is running.
	 */
	case EMS_PART_UPDATE:
		__update_cpu_active_ratio(cpu, pa, now);
		trace_multi_load_update_cpu_active_ratio(cpu, pa, "update");
		break;

	case EMS_PART_WAKEUP_NEW:
		__update_cpu_active_ratio(cpu, pa, now);
		trace_multi_load_update_cpu_active_ratio(cpu, pa, "new task");
		break;
	}
}

void set_part_period_start(struct rq *rq)
{
	struct part *pa = &per_cpu(part, cpu_of(rq));
	u64 now;

	if (unlikely(!part_initialized))
		return;

	if (likely(pa->period_start))
		return;

	now = sched_clock_cpu(0);
	pa->period_start = now;
	pa->last_updated = now;
}

int get_part_hist_idx(int cpu)
{
	struct part *pa = &per_cpu(part, cpu);

	return pa->hist_idx;
}

int get_part_hist_value(int cpu, int idx)
{
	struct part *pa = &per_cpu(part, cpu);

	return pa->hist[idx];
}

/********************************************************/
/*		     INITIALIZATION			*/
/********************************************************/
void part_init(void)
{
	int cpu, idx;

	for_each_possible_cpu(cpu) {
		struct part *pa = &per_cpu(part, cpu);

		/* Set by default value */
		pa->running = false;
		pa->cpu = cpu;
		pa->active_sum = 0;
		pa->active_ratio_recent = 0;
		pa->hist_idx = 0;
		for (idx = 0; idx < PART_HIST_SIZE_MAX; idx++)
			pa->hist[idx] = 0;

		pa->period_start = 0;
		pa->last_updated = 0;
	}

	part_initialized = 1;
}

int sysctl_sched_util_est_handler(struct ctl_table *table, int write,
				void __user *buffer, size_t *lenp,
				loff_t *ppos)
{
	unsigned int old_value;
	int result;

	old_value = sysctl_sched_util_est_clamp;

	result = proc_dointvec(table, write, buffer, lenp, ppos);
	if (result)
		goto undo;
	if (!write)
		goto done;

	if (sysctl_sched_util_est_clamp < 0 || sysctl_sched_util_est_clamp > 100) {
		result = -EINVAL;
		goto undo;
	}

	goto done;

undo:
	sysctl_sched_util_est_clamp = old_value;

done:
	return result;
}
