/*
 * CPUFreq boost driver
 *
 * Copyright (C) 2020 Samsung Electronics Co., Ltd
 * Park Choonghoon <choong.park@samsung.com>
 */
#include <linux/cpufreq.h>
#include <linux/slab.h>
#include <linux/of.h>
#include <linux/reciprocal_div.h>
#include <linux/miscdevice.h>
#include <linux/uaccess.h>
#include <linux/string.h>

#include "../sched.h"
#include "ems.h"

#include <trace/events/ems.h>
#include <trace/events/ems_debug.h>

bool freqboost_initialized = false;
struct reciprocal_value freqboost_spc_rdiv;

/* We hold freqboost in effect for at least this long */
#define FREQBOOST_HOLD_NS 50000000ULL

#define BOOSTGROUPS_COUNT STUNE_GROUP_COUNT

/* Freqboost groups
 * Keep track of all the boost groups which impact on CPU, for example when a
 * CPU has two RUNNABLE tasks belonging to two different boost groups and thus
 * likely with different boost values.
 * Since on each system we expect only a limited number of boost groups, here
 * we use a simple array to keep track of the metrics required to compute the
 * maximum per-CPU boosting value.
 */
struct boost_groups {
	/* Maximum boost value for all RUNNABLE tasks on a CPU */
	int boost_max;
	u64 boost_ts;
	struct {
		/* True when this boost group maps an actual cgroup */
		bool valid;
		/* The boost for tasks on that boost group */
		int boost;
		/* Count of RUNNABLE tasks on that boost group */
		unsigned tasks;
		/* Count of woken-up tasks on that boost group */
		unsigned wokenup_tasks;
		/* Timestamp of boost activation */
		u64 ts;
	} group[BOOSTGROUPS_COUNT];
	/* CPU's boost group locking */
	raw_spinlock_t lock;
};

/* Boost groups affecting each CPU in the system */
DEFINE_PER_CPU(struct boost_groups, cpu_boost_groups);

static inline bool freqboost_boost_timeout(u64 now, u64 ts)
{
	return ((now - ts) > FREQBOOST_HOLD_NS);
}

static inline bool
freqboost_boost_group_active(int idx, struct boost_groups *bg, u64 now)
{
	if (bg->group[idx].tasks)
		return true;

	return !freqboost_boost_timeout(now, bg->group[idx].ts);
}

static void
freqboost_cpu_update(int cpu, u64 now)
{
	struct boost_groups *bg = &per_cpu(cpu_boost_groups, cpu);
	int boost_max;
	u64 boost_ts;
	int idx;

	/* The root boost group is always active */
	boost_max = bg->group[0].boost;
	boost_ts = now;
	for (idx = 1; idx < BOOSTGROUPS_COUNT; ++idx) {

		/* Ignore non boostgroups not mapping a cgroup */
		if (!bg->group[idx].valid)
			continue;

		/*
		 * A boost group affects a CPU only if it has
		 * RUNNABLE tasks on that CPU or it has hold
		 * in effect from a previous task.
		 */
		if (!freqboost_boost_group_active(idx, bg, now))
			continue;

		/* This boost group is active */
		if (boost_max > bg->group[idx].boost)
			continue;

		boost_max = bg->group[idx].boost;
		boost_ts =  bg->group[idx].ts;
	}

	/* Ensures boost_max is non-negative when all cgroup boost values
	 * are neagtive. Avoids under-accounting of cpu capacity which may cause
	 * task stacking and frequency spikes.*/
	boost_max = max(boost_max, 0);
	bg->boost_max = boost_max;
	bg->boost_ts = boost_ts;
}

#define ENQUEUE_TASK  1
#define DEQUEUE_TASK -1

static inline bool
freqboost_update_timestamp(struct task_struct *p)
{
	return task_has_rt_policy(p);
}

static int vip_threshold = 50;

static ssize_t show_vip_threshold(struct kobject *kobj,
		struct kobj_attribute *attr, char *buf)
{
	return sprintf(buf, "%d\n", vip_threshold);
}

static ssize_t store_vip_threshold(struct kobject *kobj,
		struct kobj_attribute *attr, const char *buf, size_t count)
{
	unsigned long threshold;

	if (!sscanf(buf, "%d", &threshold))
		return -EINVAL;

	vip_threshold = threshold;

	return count;
}

static struct kobj_attribute vip_threshold_attr =
__ATTR(vip_threshold, 0644, show_vip_threshold, store_vip_threshold);

static inline void
freqboost_tasks_update(struct task_struct *p, int cpu, int idx, int flags, int task_count)
{
	struct boost_groups *bg = &per_cpu(cpu_boost_groups, cpu);
	int tasks = bg->group[idx].tasks + task_count;

	if (idx >= BOOSTGROUPS_COUNT)
		return;

	/* Update boosted tasks count while avoiding to make it negative */
	bg->group[idx].tasks = max(0, tasks);

	if (task_count == ENQUEUE_TASK && flags & ENQUEUE_WAKEUP) {
		if (ml_task_util(p) >= vip_threshold)
			bg->group[idx].wokenup_tasks++;
	}

	/* Update timeout on enqueue */
	if (task_count > 0) {
		u64 now = sched_clock_cpu(cpu);

		if (freqboost_update_timestamp(p))
			bg->group[idx].ts = now;

		/* Boost group activation or deactivation on that RQ */
		if (bg->group[idx].tasks == 1) {
			freqboost_cpu_update(cpu, now);
			emstune_cpu_update(cpu, now);
		}
	}
}

bool freqboost_cpu_boost_group_active(int idx, int cpu, u64 now)
{
	struct boost_groups *bg = &per_cpu(cpu_boost_groups, cpu);

	if (idx >= BOOSTGROUPS_COUNT)
		return false;

	/* Ignore non boostgroups not mapping a cgroup */
	if (!bg->group[idx].valid)
		return false;

	/*
	 * A boost group affects a CPU only if it has
	 * RUNNABLE tasks on that CPU or it has hold
	 * in effect from a previous task.
	 */
	if (bg->group[idx].tasks)
		return true;

	return !freqboost_boost_timeout(now, bg->group[idx].ts);
}

/*
 * NOTE: This function must be called while holding the lock on the CPU RQ
 */
void freqboost_enqueue_task(struct task_struct *p, int cpu, int flags)
{
	struct boost_groups *bg = &per_cpu(cpu_boost_groups, cpu);
	unsigned long irq_flags;
	int idx;

	if (unlikely(!freqboost_initialized))
		return;

	/*
	 * Boost group accouting is protected by a per-cpu lock and requires
	 * interrupt to be disabled to avoid race conditions for example on
	 * do_exit()::cgroup_exit() and task migration.
	 */
	raw_spin_lock_irqsave(&bg->lock, irq_flags);

	idx = cpuctl_task_group_idx(p);

	freqboost_tasks_update(p, cpu, idx, flags, ENQUEUE_TASK);

	raw_spin_unlock_irqrestore(&bg->lock, irq_flags);
}

int freqboost_can_attach(struct cgroup_taskset *tset)
{
	struct task_struct *task;
	struct cgroup_subsys_state *css;
	struct boost_groups *bg;
	struct rq_flags rq_flags;
	unsigned int cpu;
	struct rq *rq;
	int src_bg; /* Source boost group index */
	int dst_bg; /* Destination boost group index */
	int tasks;
	u64 now;

	if (unlikely(!freqboost_initialized))
		return 0;

	cgroup_taskset_for_each(task, css, tset) {
		/*
		 * Lock the CPU's RQ the task is enqueued to avoid race
		 * conditions with migration code while the task is being
		 * accounted
		 */
		rq = task_rq_lock(task, &rq_flags);

		if (!task->on_rq) {
			task_rq_unlock(rq, task, &rq_flags);
			continue;
		}

		/*
		 * Boost group accouting is protected by a per-cpu lock and requires
		 * interrupt to be disabled to avoid race conditions on...
		 */
		cpu = cpu_of(rq);
		bg = &per_cpu(cpu_boost_groups, cpu);
		raw_spin_lock(&bg->lock);

		dst_bg = css->id - 1;
		src_bg = cpuctl_task_group_idx(task);

		if (dst_bg >= BOOSTGROUPS_COUNT || src_bg >= BOOSTGROUPS_COUNT) {
			raw_spin_unlock(&bg->lock);
			task_rq_unlock(rq, task, &rq_flags);
			continue;
		}

		/*
		 * Current task is not changing boostgroup, which can
		 * happen when the new hierarchy is in use.
		 */
		if (unlikely(dst_bg == src_bg)) {
			raw_spin_unlock(&bg->lock);
			task_rq_unlock(rq, task, &rq_flags);
			continue;
		}

		/*
		 * This is the case of a RUNNABLE task which is switching its
		 * current boost group.
		 */

		/* Move task from src to dst boost group */
		tasks = bg->group[src_bg].tasks - 1;
		bg->group[src_bg].tasks = max(0, tasks);
		bg->group[dst_bg].tasks += 1;

		/* Update boost hold start for this group */
		now = sched_clock_cpu(cpu);
		bg->group[dst_bg].ts = now;

		/* Force boost group re-evaluation at next boost check */
		bg->boost_ts = now - FREQBOOST_HOLD_NS;

		raw_spin_unlock(&bg->lock);
		task_rq_unlock(rq, task, &rq_flags);
	}

	return 0;
}

/*
 * NOTE: This function must be called while holding the lock on the CPU RQ
 */
void freqboost_dequeue_task(struct task_struct *p, int cpu, int flags)
{
	struct boost_groups *bg = &per_cpu(cpu_boost_groups, cpu);
	unsigned long irq_flags;
	int idx;

	if (unlikely(!freqboost_initialized))
		return;

	/*
	 * Boost group accouting is protected by a per-cpu lock and requires
	 * interrupt to be disabled to avoid race conditions on...
	 */
	raw_spin_lock_irqsave(&bg->lock, irq_flags);

	idx = cpuctl_task_group_idx(p);

	freqboost_tasks_update(p, cpu, idx, flags, DEQUEUE_TASK);

	raw_spin_unlock_irqrestore(&bg->lock, irq_flags);
}

int freqboost_cpu_boost(int cpu)
{
	struct boost_groups *bg;
	u64 now;

	bg = &per_cpu(cpu_boost_groups, cpu);
	now = sched_clock_cpu(cpu);

	/* Check to see if we have a hold in effect */
	if (freqboost_boost_timeout(now, bg->boost_ts)) {
		freqboost_cpu_update(cpu, now);
		emstune_cpu_update(cpu, now);
	}

	return bg->boost_max;
}

long
freqboost_margin(unsigned long capacity, unsigned long signal, long boost)
{
	long long margin = 0;

	if (signal > capacity)
		return 0;

	/*
	 * Signal proportional compensation (SPC)
	 *
	 * The Boost (B) value is used to compute a Margin (M) which is
	 * proportional to the complement of the original Signal (S):
	 *   M = B * (capacity - S)
	 * The obtained M could be used by the caller to "boost" S.
	 */
	if (boost >= 0) {
		margin  = capacity - signal;
		margin *= boost;
	} else
		margin = -signal * boost;

	margin  = reciprocal_divide(margin, freqboost_spc_rdiv);

	if (boost < 0)
		margin *= -1;
	return margin;
}

int freqboost_wakeup_boost_pending(int cpu)
{
	struct boost_groups *bg;
	int i;
	int pending = 0;

	bg = &per_cpu(cpu_boost_groups, cpu);

	for (i = 0; i < BOOSTGROUPS_COUNT; i++) {
		if (bg->group[i].wokenup_tasks && emstune_wakeup_boost(i, cpu)) {
			pending = true;
			break;
		}
	}

	return pending;
}

int freqboost_wakeup_boost(int cpu, int util)
{
	struct boost_groups *bg;
	int i;
	int wakeup_boost = 0;
	unsigned long capacity = capacity_cpu(cpu);
	unsigned long boosted_util;

	bg = &per_cpu(cpu_boost_groups, cpu);

	for (i = 0; i < BOOSTGROUPS_COUNT; i++) {
		int boost;

		if (!bg->group[i].wokenup_tasks)
			continue;

		boost = emstune_wakeup_boost(i, cpu);

		if (boost > wakeup_boost)
			wakeup_boost = boost;

		/* Clear wokenup_tasks to avoid duplicate wakeup boost */
		bg->group[i].wokenup_tasks = 0;
	}

	boosted_util = util + freqboost_margin(capacity, util, wakeup_boost);

	trace_freqboost_wakeup_boost(cpu, wakeup_boost, util, boosted_util);

	return boosted_util;
}

static inline void
freqboost_init_cgroups(void)
{
	struct boost_groups *bg;
	int cpu;
	int idx;

	/* Initialize the per CPU boost groups */
	for_each_possible_cpu(cpu) {
		bg = &per_cpu(cpu_boost_groups, cpu);

		memset(bg, 0, sizeof(struct boost_groups));
		raw_spin_lock_init(&bg->lock);
		for (idx = 0; idx < BOOSTGROUPS_COUNT; idx++)
			bg->group[idx].valid = true;
	}
}

/*
 * Initialize the cgroup structures
 */
int freqboost_init(void)
{
	int ret;

	freqboost_spc_rdiv = reciprocal_value(100);
	freqboost_init_cgroups();

	freqboost_initialized = true;

	ret = sysfs_create_file(ems_kobj, &vip_threshold_attr.attr);
	if (ret)
		pr_warn("%s: failed to create vip_threshold sysfs\n", __func__);

	return 0;
}
