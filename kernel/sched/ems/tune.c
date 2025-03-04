/*
 * Frequency variant cpufreq driver
 *
 * Copyright (C) 2017 Samsung Electronics Co., Ltd
 * Park Bumgyu <bumgyu.park@samsung.com>
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

#include <dt-bindings/soc/samsung/ems.h>

static char *stune_group_name[] = {
	"root",
	"foreground",
	"background",
	"top-app",
};

static bool emstune_initialized = false;

static struct emstune_mode *emstune_modes;

static struct emstune_mode *cur_mode;
static struct emstune_set cur_set;
int emstune_cur_level;

static int emstune_mode_count;
static int emstune_level_count;

/* HACK: Only for camera/gameSDK scenario */
#define CAMERA_MODE (5)
static int camera_sub_mode;

#define GAMESDK_MODE (10)
static int gamesdk_sub_mode;

#define MAX_SUB_MODE_COUNT (5)

/******************************************************************************
 * helper                                                                     *
 ******************************************************************************/
static inline int
parse_coregroup_field(struct device_node *dn, const char *name,
					int (field)[VENDOR_NR_CPUS])
{
	int count, cpu, cursor;
	u32 val[VENDOR_NR_CPUS];

	if (!dn)
		return -EINVAL;

	count = of_property_count_u32_elems(dn, name);
	if (of_property_read_u32_array(dn, name, (u32 *)&val, count))
		return -EINVAL;

	count = 0;
	for_each_possible_cpu(cpu) {
		if (cpu != cpumask_first(cpu_coregroup_mask(cpu)))
			continue;

		for_each_cpu(cursor, cpu_coregroup_mask(cpu))
			field[cursor] = val[count];

		count++;
	}

	return 0;
}

static inline int
parse_cgroup_field(struct device_node *dn, int *field)
{
	int group, val;

	if (!dn)
		return 0;

	for (group = 0; group < STUNE_GROUP_COUNT; group++) {
		if (of_property_read_u32(dn, stune_group_name[group], &val))
			return -EINVAL;

		field[group] = val;
	}

	return 0;
}

static inline int
parse_cgroup_field_mask(struct device_node *dn, struct cpumask *mask)
{
	int group;
	const char *buf;

	if (!dn)
		return 0;

	for (group = 0; group < STUNE_GROUP_COUNT; group++) {
		if (of_property_read_string(dn, stune_group_name[group], &buf))
			return -EINVAL;

		cpulist_parse(buf, &mask[group]);
	}

	return 0;
}

static inline int
parse_cgroup_coregroup_field(struct device_node *dn,
			int (field)[STUNE_GROUP_COUNT][VENDOR_NR_CPUS])
{
	int group, ret;

	for (group = 0; group < STUNE_GROUP_COUNT; group++) {
		ret = parse_coregroup_field(dn,
				stune_group_name[group], field[group]);
		if (ret)
			return ret;
	}

	return 0;
}

/******************************************************************************
 * mode update                                                                *
 ******************************************************************************/
static RAW_NOTIFIER_HEAD(emstune_mode_chain);

/* emstune_mode_lock *MUST* be held before notifying */
static int emstune_notify_mode_update(void)
{
	return raw_notifier_call_chain(&emstune_mode_chain, 0, &cur_set);
}

int emstune_register_mode_update_notifier(struct notifier_block *nb)
{
	return raw_notifier_chain_register(&emstune_mode_chain, nb);
}

int emstune_unregister_mode_update_notifier(struct notifier_block *nb)
{
	return raw_notifier_chain_unregister(&emstune_mode_chain, nb);
}

#define default_set cur_mode->sets[0]
#define update_cur_set_field(_member)					\
	if (next_set->_member.overriding)				\
		memcpy(&cur_set._member, &next_set->_member,		\
			sizeof(struct emstune_##_member));		\
	else								\
		memcpy(&cur_set._member, &default_set._member,		\
			sizeof(struct emstune_##_member));		\

static struct cpumask prio_pinning_assigned_mask;
struct cpumask *get_prio_pinning_assigned_mask(void)
{
	if (!emstune_initialized)
		cpumask_clear(&prio_pinning_assigned_mask);

	return &prio_pinning_assigned_mask;
}

static void update_prio_pinning_assigned_tasks(void)
{
	cpumask_clear(&prio_pinning_assigned_mask);
}

static void __update_cur_set(struct emstune_set *next_set)
{
	cur_set.idx = next_set->idx;
	cur_set.desc = next_set->desc;
	cur_set.unique_id = next_set->unique_id;

	update_prio_pinning_assigned_tasks();

	/* update each field of emstune */
	update_cur_set_field(sched_policy);
	update_cur_set_field(weight);
	update_cur_set_field(idle_weight);
	update_cur_set_field(freq_boost);
	update_cur_set_field(wakeup_boost);
	update_cur_set_field(esg);
	update_cur_set_field(ontime);
	update_cur_set_field(cpus_allowed);
	update_cur_set_field(prio_pinning);
	update_cur_set_field(frt);
	update_cur_set_field(ecs);
	update_cur_set_field(tiny_cd_sched);

	emstune_notify_mode_update();
}

static DEFINE_SPINLOCK(emstune_mode_lock);

static void update_cur_set(struct emstune_set *set)
{
	unsigned long flags;

	spin_lock_irqsave(&emstune_mode_lock, flags);
	/* update only when there is a change in the currently active set */
	if (cur_set.unique_id == set->unique_id)
		__update_cur_set(set);
	spin_unlock_irqrestore(&emstune_mode_lock, flags);
}

static int emstune_send_mode_change(int mode_idx)
{
	char msg[32];
	char *envp[] = { msg, NULL };
	int ret = 0;

	if (!ems_drv_kobj)
		return ret;

	snprintf(msg, sizeof(msg), "NOTI_MODE=%d", mode_idx);
	ret = kobject_uevent_env(ems_drv_kobj, KOBJ_CHANGE, envp);
	if (ret)
		pr_warn("Failed to send uevent for notifying mode change\n");

	return ret;
}

static void __emstune_mode_change(int next_mode_idx)
{
	int mode_idx;
	unsigned long flags;

	spin_lock_irqsave(&emstune_mode_lock, flags);

	cur_mode = &emstune_modes[next_mode_idx];
	__update_cur_set(&cur_mode->sets[emstune_cur_level]);
	trace_emstune_mode(cur_mode->idx, emstune_cur_level);

	mode_idx = cur_mode->idx;

	spin_unlock_irqrestore(&emstune_mode_lock, flags);

	emstune_send_mode_change(mode_idx);
}

static void emstune_level_change(int level)
{
	unsigned long flags;

	spin_lock_irqsave(&emstune_mode_lock, flags);

	/* Change set if level changes */
	if (emstune_cur_level != level) {
		__update_cur_set(&cur_mode->sets[level]);
		emstune_cur_level = level;
		trace_emstune_mode(cur_mode->idx, emstune_cur_level);
	}

	spin_unlock_irqrestore(&emstune_mode_lock, flags);
}

/******************************************************************************
 * sched policy                                                               *
 ******************************************************************************/
int emstune_sched_policy(struct task_struct *p)
{
	int st_idx;

	if (unlikely(!emstune_initialized))
		return 0;

	st_idx = cpuctl_task_group_idx(p);

	return cur_set.sched_policy.policy[st_idx];
}

static int
parse_sched_policy(struct device_node *dn, struct emstune_set *set)
{
	struct emstune_sched_policy *sched_policy = &set->sched_policy;

	if (parse_cgroup_field(dn, sched_policy->policy))
		goto fail;

	sched_policy->overriding = true;

fail:
	of_node_put(dn);

	return 0;
}

/******************************************************************************
 * efficiency weight                                                          *
 ******************************************************************************/
#define DEFAULT_WEIGHT	(100)
int emstune_eff_weight(struct task_struct *p, int cpu, int idle)
{
	int idx, weight;

	if (unlikely(!emstune_initialized))
		return DEFAULT_WEIGHT;

	idx = cpuctl_task_group_idx(p);

	if (idle)
		weight = cur_set.idle_weight.ratio[idx][cpu];
	else
		weight = cur_set.weight.ratio[idx][cpu];

	return weight;
}

static int
parse_weight(struct device_node *dn, struct emstune_set *set)
{
	struct emstune_weight *weight = &set->weight;

	if (parse_cgroup_coregroup_field(dn, weight->ratio))
		goto fail;

	weight->overriding = true;

fail:
	of_node_put(dn);

	return 0;
}

static int
parse_idle_weight(struct device_node *dn, struct emstune_set *set)
{
	struct emstune_idle_weight *idle_weight = &set->idle_weight;

	if (parse_cgroup_coregroup_field(dn, idle_weight->ratio))
		goto fail;

	idle_weight->overriding = true;

fail:
	of_node_put(dn);

	return 0;
}

/******************************************************************************
 * frequency boost                                                            *
 ******************************************************************************/
static DEFINE_PER_CPU(int, freq_boost_max);

static struct reciprocal_value emstune_spc_rdiv;
unsigned long emstune_freq_boost(int cpu, unsigned long util)
{
	int boost = per_cpu(freq_boost_max, cpu);
	unsigned long capacity = capacity_cpu(cpu);
	unsigned long boosted_util = 0;
	long long margin = 0;

	if (!boost)
		return util;

	/*
	 * Signal proportional compensation (SPC)
	 *
	 * The Boost (B) value is used to compute a Margin (M) which is
	 * proportional to the complement of the original Signal (S):
	 *   M = B * (capacity - S)
	 * The obtained M could be used by the caller to "boost" S.
	 */
	if (boost >= 0) {
		if (capacity > util) {
			margin  = capacity - util;
			margin *= boost;
		} else
			margin  = 0;
	} else
		margin = -util * boost;

	margin  = reciprocal_divide(margin, emstune_spc_rdiv);

	if (boost < 0)
		margin *= -1;

	boosted_util = util + margin;

	trace_emstune_freq_boost(cpu, boost, util, boosted_util);

	return boosted_util;
}

/* Update maximum values of boost groups of this cpu */
void emstune_cpu_update(int cpu, u64 now)
{
	int idx, boost_max = 0;
	struct emstune_freq_boost *freq_boost;

	if (unlikely(!emstune_initialized))
		return;

	freq_boost = &cur_set.freq_boost;

	/* The root boost group is always active */
	boost_max = freq_boost->ratio[STUNE_ROOT][cpu];
	for (idx = 1; idx < STUNE_GROUP_COUNT; ++idx) {
		int boost;

		if (!freqboost_cpu_boost_group_active(idx, cpu, now))
			continue;

		boost = freq_boost->ratio[idx][cpu];
		if (boost_max < boost)
			boost_max = boost;
	}

	/*
	 * Ensures grp_boost_max is non-negative when all cgroup boost values
	 * are neagtive. Avoids under-accounting of cpu capacity which may cause
	 * task stacking and frequency spikes.
	 */
	per_cpu(freq_boost_max, cpu) = boost_max;
}

static int
parse_freq_boost(struct device_node *dn, struct emstune_set *set)
{
	struct emstune_freq_boost *freq_boost = &set->freq_boost;

	if (parse_cgroup_coregroup_field(dn, freq_boost->ratio))
		goto fail;

	freq_boost->overriding = true;

fail:
	of_node_put(dn);

	return 0;
}

/* Get wakeup boost ratio for current set */
int emstune_wakeup_boost(int group, int cpu)
{
	struct emstune_wakeup_boost *wakeup_boost;

	if (unlikely(!emstune_initialized))
		return 0;

	wakeup_boost = &cur_set.wakeup_boost;

	return wakeup_boost->ratio[group][cpu];
}

static int
parse_wakeup_boost(struct device_node *dn, struct emstune_set *set)
{
	struct emstune_wakeup_boost *wakeup_boost = &set->wakeup_boost;

	if (parse_cgroup_coregroup_field(dn, wakeup_boost->ratio))
		goto fail;

	wakeup_boost->overriding = true;

fail:
	of_node_put(dn);

	return 0;
}

/******************************************************************************
 * energy step governor                                                       *
 ******************************************************************************/
#define DEFAULT_RATE_LIMIT 4 /* 4ms */
static int
parse_esg(struct device_node *dn, struct emstune_set *set)
{
	struct emstune_esg *esg = &set->esg;

	if (parse_coregroup_field(dn, "step", esg->step))
		goto fail;
	if (parse_coregroup_field(dn, "patient-mode", esg->patient_mode))
		goto fail;
	if (parse_coregroup_field(dn, "pelt-margin", esg->pelt_margin))
		goto fail;
	if (parse_coregroup_field(dn, "pelt-boost", esg->pelt_boost))
		goto fail;

	if (of_property_read_u32(dn, "up-rate-limit-ms", &esg->up_rate_limit))
		esg->up_rate_limit = DEFAULT_RATE_LIMIT;
	if (of_property_read_u32(dn, "down-rate-limit-ms", &esg->down_rate_limit))
		esg->down_rate_limit = DEFAULT_RATE_LIMIT;

	of_property_read_u32(dn, "rapid-scale-up", &esg->rapid_scale_up);
	of_property_read_u32(dn, "rapid-scale-down", &esg->rapid_scale_down);

	esg->overriding = true;

fail:
	of_node_put(dn);

	return 0;
}

/******************************************************************************
 * ontime migration                                                           *
 ******************************************************************************/
int emstune_ontime(struct task_struct *p)
{
	int st_idx;

	if (unlikely(!emstune_initialized))
		return 0;

	st_idx = cpuctl_task_group_idx(p);

	return cur_set.ontime.enabled[st_idx];
}

int emstune_tiny_cd_sched(struct task_struct *p)
{
	int st_idx;

	if (unlikely(!emstune_initialized))
		return 0;

	st_idx = cpuctl_task_group_idx(p);

	return cur_set.tiny_cd_sched.enabled[st_idx];
}

static void init_ontime_list(struct emstune_ontime *ontime)
{
	INIT_LIST_HEAD(&ontime->dom_list);

	ontime->p_dom_list = &ontime->dom_list;
}

static void
add_ontime_domain(struct list_head *list, struct cpumask *cpus,
				unsigned long ub, unsigned long lb)
{
	struct ontime_dom *dom;

	list_for_each_entry(dom, list, node) {
		if (cpumask_intersects(&dom->cpus, cpus)) {
			pr_err("domains with overlapping cpu already exist\n");
			return;
		}
	}

	dom = kzalloc(sizeof(struct ontime_dom), GFP_KERNEL);
	if (!dom)
		return;

	cpumask_copy(&dom->cpus, cpus);
	dom->upper_boundary = ub;
	dom->lower_boundary = lb;

	list_add_tail(&dom->node, list);
}

static void remove_ontime_domain(struct list_head *list, int cpu)
{
	struct ontime_dom *dom;
	int found = 0;

	list_for_each_entry(dom, list, node) {
		if (cpumask_test_cpu(cpu, &dom->cpus)) {
			found = 1;
			break;
		}
	}

	if (!found)
		return;

	list_del(&dom->node);
	kfree(dom);
}

static void update_ontime_domain(struct list_head *list, int cpu,
					long ub, long lb)
{
	struct ontime_dom *dom;
	int found = 0;

	list_for_each_entry(dom, list, node) {
		if (cpumask_test_cpu(cpu, &dom->cpus)) {
			found = 1;
			break;
		}
	}

	if (!found)
		return;

	if (ub >= 0)
		dom->upper_boundary = ub;
	if (lb >= 0)
		dom->lower_boundary = lb;
}

static int init_dom_list(struct device_node *dom_list_dn,
				struct list_head *dom_list)
{
	struct device_node *dom_dn;

	for_each_child_of_node(dom_list_dn, dom_dn) {
		struct ontime_dom *dom;
		const char *buf;

		dom = kzalloc(sizeof(struct ontime_dom), GFP_KERNEL);
		if (!dom)
			return -ENOMEM;

		if (of_property_read_string(dom_dn, "cpus", &buf)) {
			kfree(dom);
			return -EINVAL;
		}
		else
			cpulist_parse(buf, &dom->cpus);

		if (of_property_read_u32(dom_dn, "upper-boundary",
					(u32 *)&dom->upper_boundary))
			dom->upper_boundary = 1024;

		if (of_property_read_u32(dom_dn, "lower-boundary",
					(u32 *)&dom->lower_boundary))
			dom->lower_boundary = 0;

		list_add_tail(&dom->node, dom_list);
	}

	return 0;
}

static int
parse_ontime(struct device_node *dn, struct emstune_set *set)
{
	struct emstune_ontime *ontime = &set->ontime;

	if (parse_cgroup_field(dn, ontime->enabled))
		goto fail;

	if (init_dom_list(of_get_child_by_name(dn, "dom-list"),
					&ontime->dom_list))
		goto fail;

	ontime->overriding = true;

fail:
	of_node_put(dn);

	return 0;
}

static int
parse_tiny_cd_sched(struct device_node *dn, struct emstune_set *set)
{
	struct emstune_tiny_cd_sched *tiny_cd_sched = &set->tiny_cd_sched;

	if (parse_cgroup_field(dn, tiny_cd_sched->enabled))
		goto fail;

	tiny_cd_sched->overriding = true;

fail:
	of_node_put(dn);

	return 0;
}

/******************************************************************************
 * per group cpus allowed                                                     *
 ******************************************************************************/
static const char *get_sched_class_str(int class)
{
	if (class & EMS_SCHED_STOP)
		return "STOP";
	if (class & EMS_SCHED_DL)
		return "DL";
	if (class & EMS_SCHED_RT)
		return "RT";
	if (class & EMS_SCHED_FAIR)
		return "FAIR";
	if (class & EMS_SCHED_IDLE)
		return "IDLE";

	return NULL;
}

static int get_sched_class_idx(const struct sched_class *class)
{
	if (class == &stop_sched_class)
		return EMS_SCHED_STOP;

	if (class == &dl_sched_class)
		return EMS_SCHED_DL;

	if (class == &rt_sched_class)
		return EMS_SCHED_RT;

	if (class == &fair_sched_class)
		return EMS_SCHED_FAIR;

	if (class == &idle_sched_class)
		return EMS_SCHED_IDLE;

	return NUM_OF_SCHED_CLASS;
}

static int emstune_prio_pinning(struct task_struct *p);
const struct cpumask *emstune_cpus_allowed(struct task_struct *p)
{
	int st_idx;
	struct emstune_cpus_allowed *cpus_allowed = &cur_set.cpus_allowed;
	int class_idx;

	if (unlikely(!emstune_initialized))
		return cpu_active_mask;

	/* priority of prio_pinning is higher than esmtune.cpus_allowed */
	if (emstune_prio_pinning(p))
		if (cpumask_weight(&cur_set.prio_pinning.mask))
			return &cur_set.prio_pinning.mask;

	class_idx = get_sched_class_idx(p->sched_class);
	if (!(cpus_allowed->target_sched_class & class_idx))
		return cpu_active_mask;

	st_idx = cpuctl_task_group_idx(p);
	if (unlikely(cpumask_empty(&cpus_allowed->mask[st_idx])))
		return cpu_active_mask;

	return &cpus_allowed->mask[st_idx];
}

bool emstune_can_migrate_task(struct task_struct *p, int dst_cpu)
{
	return cpumask_test_cpu(dst_cpu, emstune_cpus_allowed(p));
}

static int
parse_cpus_allowed(struct device_node *dn, struct emstune_set *set)
{
	struct emstune_cpus_allowed *cpus_allowed = &set->cpus_allowed;

	if (!dn)
		return 0;

	if (of_property_read_u32(dn, "target-sched-class",
			(unsigned int *)&cpus_allowed->target_sched_class))
		goto fail;

	if (parse_cgroup_field_mask(dn, cpus_allowed->mask))
		goto fail;

	cpus_allowed->overriding = true;

fail:
	of_node_put(dn);

	return 0;
}

/******************************************************************************
 * priority pinning                                                           *
 ******************************************************************************/
static int emstune_prio_pinning(struct task_struct *p)
{
	int st_idx;

        if (unlikely(!emstune_initialized))
		return 0;

	st_idx = cpuctl_task_group_idx(p);
	if (cur_set.prio_pinning.enabled[st_idx]) {
		if (p->sched_class == &fair_sched_class &&
		    p->prio <= cur_set.prio_pinning.prio)
			return 1;
	}

	return 0;
}

static int
parse_prio_pinning(struct device_node *dn, struct emstune_set *set)
{
	struct emstune_prio_pinning *prio_pinning = &set->prio_pinning;
	const char *buf;

	if (!dn)
		return 0;

	if (parse_cgroup_field(dn, prio_pinning->enabled))
		goto fail;

	if (of_property_read_string(dn, "mask", &buf))
		goto fail;

	cpulist_parse(buf, &prio_pinning->mask);

	if (of_property_read_u32(dn, "prio", &prio_pinning->prio))
		goto fail;

	prio_pinning->overriding = true;

fail:
	of_node_put(dn);

	return 0;
}

int need_prio_pinning(struct task_struct *p)
{
	return emstune_prio_pinning(p);
}

void prio_pinning_enqueue_task(struct task_struct *p, int cpu)
{
	if (!emstune_prio_pinning(p))
		return;

	cpumask_set_cpu(cpu, &prio_pinning_assigned_mask);
}

void prio_pinning_dequeue_task(struct task_struct *p, int cpu)
{
	if (!emstune_prio_pinning(p))
		return;

	cpumask_clear_cpu(cpu, &prio_pinning_assigned_mask);
}

int prio_pinning_schedule(struct tp_env *env, int prev_cpu)
{
	int cpu = -1, st_idx;
	struct cpumask prio_pinning_mask;

	st_idx = cpuctl_task_group_idx(env->p);
	if (!cur_set.prio_pinning.enabled[st_idx])
		return -1;

	cpumask_and(&prio_pinning_mask,
		    &cur_set.prio_pinning.mask, &env->cpus_allowed);

	while (!cpumask_empty(&prio_pinning_mask)) {
		cpu = cpumask_last(&prio_pinning_mask);
		if (!cpumask_test_cpu(cpu, &prio_pinning_assigned_mask))
			break;

		cpumask_clear_cpu(cpu, &prio_pinning_mask);
	}

	/* All prio pinning cpus are occupied by prio pinning tasks */
	if (cpumask_empty(&prio_pinning_mask))
		return -1;

	return cpu;
}

/******************************************************************************
 * FRT                                                                        *
 ******************************************************************************/
static int
parse_frt(struct device_node *dn, struct emstune_set *set)
{
	struct emstune_frt *frt = &set->frt;

	if (parse_coregroup_field(dn, "active-ratio", frt->active_ratio))
		goto fail;

	frt->overriding = true;

fail:
	of_node_put(dn);

	return 0;
}

/******************************************************************************
 * core sparing                                                               *
 ******************************************************************************/
static void update_ecs_threshold(struct list_head *stage_list,
					int id, int threshold)
{
	struct ecs_stage *stage;

	list_for_each_entry(stage, stage_list, node)
		if (stage->id == id)
			stage->busy_threshold_orig = threshold;
}

static void init_ecs_list(struct emstune_ecs *ecs)
{
	INIT_LIST_HEAD(&ecs->stage_list);

	ecs->p_stage_list = &ecs->stage_list;
}

static int init_ecs_stage_list(struct device_node *ecs_dn,
				struct list_head *stage_list)
{
	struct device_node *stage_dn;
	int id = 0;

	for_each_child_of_node(ecs_dn, stage_dn) {
		struct ecs_stage *stage;
		const char *buf;

		stage = kzalloc(sizeof(struct ecs_stage), GFP_KERNEL);
		if (!stage)
			return -ENOMEM;

		if (of_property_read_string(stage_dn, "cpus", &buf)) {
			kfree(stage);
			return -EINVAL;
		}
		else
			cpulist_parse(buf, &stage->cpus);

		if (of_property_read_string(stage_dn, "monitor-cpus", &buf)) {
			kfree(stage);
			return -EINVAL;
		}
		else
			cpulist_parse(buf, &stage->monitor_cpus);

		if (of_property_read_u32(stage_dn, "busy-threshold",
					(u32 *)&stage->busy_threshold_orig))
			stage->busy_threshold_orig = UINT_MAX;

		stage->id = id++;
		list_add_tail(&stage->node, stage_list);
	}

	return 0;
}

static int
parse_ecs(struct device_node *dn, struct emstune_set *set)
{
	struct emstune_ecs *ecs = &set->ecs;

	if (init_ecs_stage_list(dn, &ecs->stage_list))
		goto fail;

	ecs->overriding = true;

fail:
	of_node_put(dn);

	return 0;
}

/******************************************************************************
 * show mode	                                                              *
 ******************************************************************************/
static ssize_t
show_req_mode_level(struct kobject *k, struct kobj_attribute *attr, char *buf)
{
	int ret = 0;
	unsigned long flags;

	spin_lock_irqsave(&emstune_mode_lock, flags);
	ret = sprintf(buf, "%d\n", emstune_cur_level);
	spin_unlock_irqrestore(&emstune_mode_lock, flags);

	return ret;
}

static ssize_t
store_req_mode_level(struct kobject *k, struct kobj_attribute *attr,
				const char *buf, size_t count)
{
	int mode_level;

	if (sscanf(buf, "%d", &mode_level) != 1)
		return -EINVAL;

	if (mode_level < 0 || mode_level >= MAX_MODE_LEVEL)
		return -EINVAL;

	if (mode_level == BOOST_LEVEL)
		mode_level = cur_mode->boost_level;

	emstune_level_change(mode_level);

	return count;
}

static struct kobj_attribute req_mode_level_attr =
__ATTR(req_mode_level, 0644, show_req_mode_level, store_req_mode_level);

static ssize_t
show_req_mode(struct kobject *k, struct kobj_attribute *attr, char *buf)
{
	int ret = 0, i;

	for (i = 0; i < emstune_mode_count; i++) {
		if (i == cur_mode->idx)
			ret += sprintf(buf + ret, ">> ");
		else
			ret += sprintf(buf + ret, "   ");

		ret += sprintf(buf + ret, "%s mode (idx=%d)\n",
				emstune_modes[i].desc, emstune_modes[i].idx);
	}

	return ret;
}

static ssize_t
store_req_mode(struct kobject *k, struct kobj_attribute *attr,
				const char *buf, size_t count)
{
	int mode;

	if (sscanf(buf, "%d", &mode) != 1)
		return -EINVAL;

	/* ignore if requested mode is out of range */
	if (mode < 0 || mode >= emstune_mode_count)
		return -EINVAL;

	if (mode == CAMERA_MODE)
		__emstune_mode_change(mode + camera_sub_mode);
	else if (mode == GAMESDK_MODE)
		__emstune_mode_change(mode + gamesdk_sub_mode);
	else
		__emstune_mode_change(mode);

	pr_info("[EMSTUNE] change mode : %s (idx=%d)\n",
			emstune_modes[mode].desc, mode);

	return count;
}

static struct kobj_attribute req_mode_attr =
__ATTR(req_mode, 0644, show_req_mode, store_req_mode);

/* Sysfs node for CAMERA sub mode */
static ssize_t
show_req_cam_sub_mode(struct kobject *k, struct kobj_attribute *attr, char *buf)
{
	int cur_mode_idx = cur_mode->idx;

	if (cur_mode_idx >= CAMERA_MODE &&
			cur_mode_idx < CAMERA_MODE + MAX_SUB_MODE_COUNT)
		return sprintf(buf, "sub mode=%d only for CAMERA mode\n",
						cur_mode_idx - CAMERA_MODE);
	else
		return sprintf(buf, "All but CAMERA mode don't use sub mode\n");
}

static ssize_t
store_req_cam_sub_mode(struct kobject *k, struct kobj_attribute *attr,
				const char *buf, size_t count)
{
	int sub_mode, cur_mode_idx;

	if (sscanf(buf, "%d", &sub_mode) != 1)
		return -EINVAL;

	/* ignore if requested sub-mode is out of range */
	if (sub_mode < 0 || sub_mode >= MAX_SUB_MODE_COUNT)
		return -EINVAL;

	camera_sub_mode = sub_mode;
	cur_mode_idx = cur_mode->idx;

	/* Camera mode has its own sub modes itself */
	if (cur_mode_idx >= CAMERA_MODE &&
		cur_mode_idx < CAMERA_MODE + MAX_SUB_MODE_COUNT)
		__emstune_mode_change(CAMERA_MODE + camera_sub_mode);

	return count;
}

static struct kobj_attribute req_cam_sub_mode_attr =
__ATTR(req_cam_sub_mode, 0644, show_req_cam_sub_mode, store_req_cam_sub_mode);

/* Sysfs node for gameSDK sub mode */
static ssize_t
show_req_gsdk_sub_mode(struct kobject *k, struct kobj_attribute *attr, char *buf)
{
	int cur_mode_idx = cur_mode->idx;

	if (cur_mode_idx >= GAMESDK_MODE &&
			cur_mode_idx < GAMESDK_MODE + MAX_SUB_MODE_COUNT)
		return sprintf(buf, "sub mode=%d only for gameSDK mode\n",
				cur_mode_idx - GAMESDK_MODE);
	else
		return sprintf(buf, "All but gameSDK mode don't use sub mode\n");
}

static ssize_t
store_req_gsdk_sub_mode(struct kobject *k, struct kobj_attribute *attr,
				const char *buf, size_t count)
{
	int sub_mode, cur_mode_idx;

	if (sscanf(buf, "%d", &sub_mode) != 1)
		return -EINVAL;

	/* ignore if requested sub-mode is out of range */
	if (sub_mode < 0 || sub_mode >= MAX_SUB_MODE_COUNT)
		return -EINVAL;

	gamesdk_sub_mode = sub_mode;
	cur_mode_idx = cur_mode->idx;

	/* gameSDK mode has its own sub modes itself */
	if (cur_mode_idx >= GAMESDK_MODE &&
			cur_mode_idx < GAMESDK_MODE + MAX_SUB_MODE_COUNT)
		__emstune_mode_change(GAMESDK_MODE + gamesdk_sub_mode);

	return count;
}

static struct kobj_attribute req_gsdk_sub_mode_attr =
__ATTR(req_gsdk_sub_mode, 0644, show_req_gsdk_sub_mode, store_req_gsdk_sub_mode);

static char *stune_group_simple_name[] = {
	"root",
	"fg",
	"bg",
	"ta",
};

static ssize_t
show_cur_set(struct kobject *k, struct kobj_attribute *attr, char *buf)
{
	int ret = 0;
	int cpu, group, class;

	ret += scnprintf(buf + ret, PAGE_SIZE - ret, "current set: %d (%s)\n", cur_set.idx, cur_set.desc);
	ret += scnprintf(buf + ret, PAGE_SIZE - ret, "\n");

	ret += scnprintf(buf + ret, PAGE_SIZE - ret, "[sched-policy]\n");
	for (group = 0; group < STUNE_GROUP_COUNT; group++)
		ret += scnprintf(buf + ret, PAGE_SIZE - ret, "%5s", stune_group_simple_name[group]);
	ret += scnprintf(buf + ret, PAGE_SIZE - ret, "\n");
	for (group = 0; group < STUNE_GROUP_COUNT; group++)
		ret += scnprintf(buf + ret, PAGE_SIZE - ret, "%5d",
				cur_set.sched_policy.policy[group]);
	ret += scnprintf(buf + ret, PAGE_SIZE - ret, "\n\n");

	ret += scnprintf(buf + ret, PAGE_SIZE - ret, "[weight]\n");
	ret += scnprintf(buf + ret, PAGE_SIZE - ret, "     ");
	for (group = 0; group < STUNE_GROUP_COUNT; group++)
		ret += scnprintf(buf + ret, PAGE_SIZE - ret, "%5s", stune_group_simple_name[group]);
	ret += scnprintf(buf + ret, PAGE_SIZE - ret, "\n");
	for_each_possible_cpu(cpu) {
		ret += scnprintf(buf + ret, PAGE_SIZE - ret, "cpu%d:", cpu);
		for (group = 0; group < STUNE_GROUP_COUNT; group++)
			ret += scnprintf(buf + ret, PAGE_SIZE - ret, "%5d",
					cur_set.weight.ratio[group][cpu]);
		ret += scnprintf(buf + ret, PAGE_SIZE - ret, "\n");
	}
	ret += scnprintf(buf + ret, PAGE_SIZE - ret, "\n");

	ret += scnprintf(buf + ret, PAGE_SIZE - ret, "[idle-weight]\n");
	ret += scnprintf(buf + ret, PAGE_SIZE - ret, "     ");
	for (group = 0; group < STUNE_GROUP_COUNT; group++)
		ret += scnprintf(buf + ret, PAGE_SIZE - ret, "%5s", stune_group_simple_name[group]);
	ret += scnprintf(buf + ret, PAGE_SIZE - ret, "\n");
	for_each_possible_cpu(cpu) {
		ret += scnprintf(buf + ret, PAGE_SIZE - ret, "cpu%d:", cpu);
		for (group = 0; group < STUNE_GROUP_COUNT; group++)
			ret += scnprintf(buf + ret, PAGE_SIZE - ret, "%5d",
					cur_set.idle_weight.ratio[group][cpu]);
		ret += scnprintf(buf + ret, PAGE_SIZE - ret, "\n");
	}
	ret += scnprintf(buf + ret, PAGE_SIZE - ret, "\n");

	ret += scnprintf(buf + ret, PAGE_SIZE - ret, "[freq-boost]\n");
	ret += scnprintf(buf + ret, PAGE_SIZE - ret, "     ");
	for (group = 0; group < STUNE_GROUP_COUNT; group++)
		ret += scnprintf(buf + ret, PAGE_SIZE - ret, "%5s", stune_group_simple_name[group]);
	ret += scnprintf(buf + ret, PAGE_SIZE - ret, "\n");
	for_each_possible_cpu(cpu) {
		ret += scnprintf(buf + ret, PAGE_SIZE - ret, "cpu%d:", cpu);
		for (group = 0; group < STUNE_GROUP_COUNT; group++)
			ret += scnprintf(buf + ret, PAGE_SIZE - ret, "%5d",
					cur_set.freq_boost.ratio[group][cpu]);
		ret += scnprintf(buf + ret, PAGE_SIZE - ret, "\n");
	}
	ret += scnprintf(buf + ret, PAGE_SIZE - ret, "\n");

	ret += scnprintf(buf + ret, PAGE_SIZE - ret, "[wakeup-boost]\n");
	ret += scnprintf(buf + ret, PAGE_SIZE - ret, "     ");
	for (group = 0; group < STUNE_GROUP_COUNT; group++)
		ret += scnprintf(buf + ret, PAGE_SIZE - ret, "%5s", stune_group_simple_name[group]);
	ret += scnprintf(buf + ret, PAGE_SIZE - ret, "\n");
	for_each_possible_cpu(cpu) {
		ret += scnprintf(buf + ret, PAGE_SIZE - ret, "cpu%d:", cpu);
		for (group = 0; group < STUNE_GROUP_COUNT; group++)
			ret += scnprintf(buf + ret, PAGE_SIZE - ret, "%5d",
					cur_set.wakeup_boost.ratio[group][cpu]);
		ret += scnprintf(buf + ret, PAGE_SIZE - ret, "\n");
	}
	ret += scnprintf(buf + ret, PAGE_SIZE - ret, "\n");

	ret += scnprintf(buf + ret, PAGE_SIZE - ret, "[esg]\n");
	ret += scnprintf(buf + ret, PAGE_SIZE - ret, "      step patient margin  boost\n");
	for_each_possible_cpu(cpu) {
		ret += scnprintf(buf + ret, PAGE_SIZE - ret, "cpu%d:%5d %7d %6d %6d\n",
				cpu, cur_set.esg.step[cpu],
				cur_set.esg.patient_mode[cpu],
				cur_set.esg.pelt_margin[cpu],
				cur_set.esg.pelt_boost[cpu]);
	}
	ret += scnprintf(buf + ret, PAGE_SIZE - ret, "\n");
	ret += scnprintf(buf + ret, PAGE_SIZE - ret, "up rate limit    : %d ms\n", cur_set.esg.up_rate_limit);
	ret += scnprintf(buf + ret, PAGE_SIZE - ret, "down rate limit  : %d ms\n", cur_set.esg.down_rate_limit);
	ret += scnprintf(buf + ret, PAGE_SIZE - ret, "rapid scale up   : %s\n",
			cur_set.esg.rapid_scale_up ? "enabled" : "disabled");
	ret += scnprintf(buf + ret, PAGE_SIZE - ret, "rapid scale down : %s\n",
			cur_set.esg.rapid_scale_down ? "enabled" : "disabled");
	ret += scnprintf(buf + ret, PAGE_SIZE - ret, "\n");

	ret += scnprintf(buf + ret, PAGE_SIZE - ret, "[ontime]\n");
	for (group = 0; group < STUNE_GROUP_COUNT; group++)
		ret += scnprintf(buf + ret, PAGE_SIZE - ret, "%5s", stune_group_simple_name[group]);
	ret += scnprintf(buf + ret, PAGE_SIZE - ret, "\n");
	for (group = 0; group < STUNE_GROUP_COUNT; group++)
		ret += scnprintf(buf + ret, PAGE_SIZE - ret, "%5d", cur_set.ontime.enabled[group]);
	ret += scnprintf(buf + ret, PAGE_SIZE - ret, "\n\n");
	{
		struct ontime_dom *dom;

		ret += scnprintf(buf + ret, PAGE_SIZE - ret, "-----------------------------\n");
		if (!list_empty(cur_set.ontime.p_dom_list)) {
			list_for_each_entry(dom, cur_set.ontime.p_dom_list, node) {
				ret += scnprintf(buf + ret, PAGE_SIZE - ret, " cpus           : %*pbl\n",
					cpumask_pr_args(&dom->cpus));
				ret += scnprintf(buf + ret, PAGE_SIZE - ret, " upper boundary : %lu\n",
					dom->upper_boundary);
				ret += scnprintf(buf + ret, PAGE_SIZE - ret, " lower boundary : %lu\n",
					dom->lower_boundary);
				ret += scnprintf(buf + ret, PAGE_SIZE - ret, "-----------------------------\n");
			}
		} else  {
			ret += scnprintf(buf + ret, PAGE_SIZE - ret, "list empty!\n");
			ret += scnprintf(buf + ret, PAGE_SIZE - ret, "-----------------------------\n");
		}
	}
	ret += scnprintf(buf + ret, PAGE_SIZE - ret, "\n");

	ret += scnprintf(buf + ret, PAGE_SIZE - ret, "[cpus-allowed]\n");
	for (class = 0; class < NUM_OF_SCHED_CLASS; class++)
		ret += scnprintf(buf + ret, PAGE_SIZE - ret, "%5s", get_sched_class_str(1 << class));
	ret += scnprintf(buf + ret, PAGE_SIZE - ret, "\n");
	for (class = 0; class < NUM_OF_SCHED_CLASS; class++)
		ret += scnprintf(buf + ret, PAGE_SIZE - ret, "%5d",
			(cur_set.cpus_allowed.target_sched_class & (1 << class) ? 1 : 0));
	ret += scnprintf(buf + ret, PAGE_SIZE - ret, "\n\n");
	for (group = 0; group < STUNE_GROUP_COUNT; group++)
		ret += scnprintf(buf + ret, PAGE_SIZE - ret, "%5s", stune_group_simple_name[group]);
	ret += scnprintf(buf + ret, PAGE_SIZE - ret, "\n");
	for (group = 0; group < STUNE_GROUP_COUNT; group++)
		ret += scnprintf(buf + ret, PAGE_SIZE - ret, "%#5x",
			*(unsigned int *)cpumask_bits(&cur_set.cpus_allowed.mask[group]));
	ret += scnprintf(buf + ret, PAGE_SIZE - ret, "\n\n");

	ret += scnprintf(buf + ret, PAGE_SIZE - ret, "[prio-pinning]\n");
	ret += scnprintf(buf + ret, PAGE_SIZE - ret, "mask : %#x\n",
			*(unsigned int *)cpumask_bits(&cur_set.prio_pinning.mask));
	ret += scnprintf(buf + ret, PAGE_SIZE - ret, "prio : %d", cur_set.prio_pinning.prio);
	ret += scnprintf(buf + ret, PAGE_SIZE - ret, "\n\n");
	for (group = 0; group < STUNE_GROUP_COUNT; group++)
		ret += scnprintf(buf + ret, PAGE_SIZE - ret, "%5s", stune_group_simple_name[group]);
	ret += scnprintf(buf + ret, PAGE_SIZE - ret, "\n");
	for (group = 0; group < STUNE_GROUP_COUNT; group++)
		ret += scnprintf(buf + ret, PAGE_SIZE - ret, "%5d", cur_set.prio_pinning.enabled[group]);
	ret += scnprintf(buf + ret, PAGE_SIZE - ret, "\n\n");

	ret += scnprintf(buf + ret, PAGE_SIZE - ret, "[frt]\n");
	ret += scnprintf(buf + ret, PAGE_SIZE - ret, "      active\n");
	for_each_possible_cpu(cpu)
		ret += scnprintf(buf + ret, PAGE_SIZE - ret, "cpu%d: %5d%%\n",
				cpu,
				cur_set.frt.active_ratio[cpu]);
	ret += scnprintf(buf + ret, PAGE_SIZE - ret, "\n");

	ret += scnprintf(buf + ret, PAGE_SIZE - ret, "[ecs]\n");
	{
		struct ecs_stage *stage;

		ret += scnprintf(buf + ret, PAGE_SIZE - ret, "-----------------------------\n");
		if (!list_empty(cur_set.ecs.p_stage_list)) {
			list_for_each_entry(stage, cur_set.ecs.p_stage_list, node) {
				ret += scnprintf(buf + ret, PAGE_SIZE - ret, " id             : %d\n",
					stage->id);
				ret += scnprintf(buf + ret, PAGE_SIZE - ret, " cpus           : %*pbl\n",
					cpumask_pr_args(&stage->cpus));
				ret += scnprintf(buf + ret, PAGE_SIZE - ret, " monitor cpus   : %*pbl\n",
					cpumask_pr_args(&stage->monitor_cpus));
				ret += scnprintf(buf + ret, PAGE_SIZE - ret, " busy threshold : %lu\n",
					stage->busy_threshold);
				ret += scnprintf(buf + ret, PAGE_SIZE - ret, " busy threshold : %lu (origin)\n",
					stage->busy_threshold_orig);
				ret += scnprintf(buf + ret, PAGE_SIZE - ret, "-----------------------------\n");
			}
		} else  {
			ret += scnprintf(buf + ret, PAGE_SIZE - ret, "list empty!\n");
			ret += scnprintf(buf + ret, PAGE_SIZE - ret, "-----------------------------\n");
		}
	}
	ret += scnprintf(buf + ret, PAGE_SIZE - ret, "\n");

	ret += scnprintf(buf + ret, PAGE_SIZE - ret, "[tiny cd sched]\n");
	for (group = 0; group < STUNE_GROUP_COUNT; group++)
		ret += scnprintf(buf + ret, PAGE_SIZE - ret, "%5s", stune_group_simple_name[group]);
	ret += scnprintf(buf + ret, PAGE_SIZE - ret, "\n");
	for (group = 0; group < STUNE_GROUP_COUNT; group++)
		ret += scnprintf(buf + ret, PAGE_SIZE - ret, "%5d", cur_set.tiny_cd_sched.enabled[group]);
	ret += scnprintf(buf + ret, PAGE_SIZE - ret, "\n");

	return ret;
}

static struct kobj_attribute cur_set_attr =
__ATTR(cur_set, 0444, show_cur_set, NULL);

static ssize_t
show_aio_tuner(struct kobject *k, struct kobj_attribute *attr, char *buf)
{
	int ret = 0;

	ret = sprintf(buf + ret, "\n");
	ret += sprintf(buf + ret, "echo <mode,level,index,...> <value> > aio_tuner\n");
	ret += sprintf(buf + ret, "\n");
	ret += sprintf(buf + ret, "[0] sched-policy\n");
	ret += sprintf(buf + ret, "	# echo <0,mode,level,group> <policy> > aio_tuner\n");
	ret += sprintf(buf + ret, "	(policy0/1/2/3/4/5/=eff/energy/perf/eff_tiny/eff_enrgy/energy_adv)\n");
	ret += sprintf(buf + ret, "\n");
	ret += sprintf(buf + ret, "[1] weight\n");
	ret += sprintf(buf + ret, "	# echo <1,mode,level,cpu,group> <ratio> > aio_tuner\n");
	ret += sprintf(buf + ret, "	(group0/1/2/3=root/fg/bg/ta)\n");
	ret += sprintf(buf + ret, "\n");
	ret += sprintf(buf + ret, "[2] idle-weight\n");
	ret += sprintf(buf + ret, "	# echo <2,mode,level,cpu,group> <ratio> > aio_tuner\n");
	ret += sprintf(buf + ret, "	(group0/1/2/3=root/fg/bg/ta)\n");
	ret += sprintf(buf + ret, "\n");
	ret += sprintf(buf + ret, "[3] freq-boost\n");
	ret += sprintf(buf + ret, "	# echo <3,mode,level,cpu,group> <ratio> > aio_tuner\n");
	ret += sprintf(buf + ret, "	(group0/1/2/3=root/fg/bg/ta)\n");
	ret += sprintf(buf + ret, "\n");
	ret += sprintf(buf + ret, "[4] esg step\n");
	ret += sprintf(buf + ret, "	# echo <4,mode,level,cpu> <step> > aio_tuner\n");
	ret += sprintf(buf + ret, "	(set one cpu, coregroup to which cpu belongs is set))\n");
	ret += sprintf(buf + ret, "\n");
	ret += sprintf(buf + ret, "[5] esg patient_mode\n");
	ret += sprintf(buf + ret, "	# echo <5,mode,level,cpu> <tick_count> > aio_tuner\n");
	ret += sprintf(buf + ret, "	(set one cpu, coregroup to which cpu belongs is set))\n");
	ret += sprintf(buf + ret, "\n");
	ret += sprintf(buf + ret, "[6] esg pelt margin\n");
	ret += sprintf(buf + ret, "	# echo <6,mode,level,cpu> <ratio> > aio_tuner\n");
	ret += sprintf(buf + ret, "	(set one cpu, coregroup to which cpu belongs is set))\n");
	ret += sprintf(buf + ret, "\n");
	ret += sprintf(buf + ret, "[7] esg pelt boost\n");
	ret += sprintf(buf + ret, "	# echo <7,mode,level,cpu> <ratio> > aio_tuner\n");
	ret += sprintf(buf + ret, "	(set one cpu, coregroup to which cpu belongs is set))\n");
	ret += sprintf(buf + ret, "\n");
	ret += sprintf(buf + ret, "[8] ontime enable\n");
	ret += sprintf(buf + ret, "	# echo <8,mode,level,group> <en/dis> > aio_tuner\n");
	ret += sprintf(buf + ret, "	(enable=1 disable=0)\n");
	ret += sprintf(buf + ret, "	(group0/1/2/3=root/fg/bg/ta)\n");
	ret += sprintf(buf + ret, "\n");
	ret += sprintf(buf + ret, "[9] ontime add domain\n");
	ret += sprintf(buf + ret, "	# echo <9,mode,level> <mask> > aio_tuner\n");
	ret += sprintf(buf + ret, "	(hexadecimal, mask is cpus constituting the domain)\n");
	ret += sprintf(buf + ret, "\n");
	ret += sprintf(buf + ret, "[10] ontime remove domain\n");
	ret += sprintf(buf + ret, "	# echo <10,mode,level> <cpu> > aio_tuner\n");
	ret += sprintf(buf + ret, "	(The domain to which the cpu belongs is removed)\n");
	ret += sprintf(buf + ret, "\n");
	ret += sprintf(buf + ret, "[11] ontime update boundary\n");
	ret += sprintf(buf + ret, "	# echo <11,mode,level,cpu,type> <boundary> > aio_tuner\n");
	ret += sprintf(buf + ret, "	(type0/1=lower boundary/upper boundary\n");
	ret += sprintf(buf + ret, "	(set one cpu, domain to which cpu belongs is set))\n");
	ret += sprintf(buf + ret, "\n");
	ret += sprintf(buf + ret, "[12] cpus_allowed target sched class\n");
	ret += sprintf(buf + ret, "	# echo <12,mode,level> <mask> > aio_tuner\n");
	ret += sprintf(buf + ret, "	(hexadecimal, class0/1/2/3/4=stop/dl/rt/fair/idle)\n");
	ret += sprintf(buf + ret, "\n");
	ret += sprintf(buf + ret, "[13] cpus_allowed\n");
	ret += sprintf(buf + ret, "	# echo <13,mode,level,group> <mask> > aio_tuner\n");
	ret += sprintf(buf + ret, "	(hexadecimal, group0/1/2/3=root/fg/bg/ta)\n");
	ret += sprintf(buf + ret, "\n");
	ret += sprintf(buf + ret, "[14] prio_pinning mask\n");
	ret += sprintf(buf + ret, "	# echo <14,mode,level> <mask> > aio_tuner\n");
	ret += sprintf(buf + ret, "	(hexadecimal)\n");
	ret += sprintf(buf + ret, "\n");
	ret += sprintf(buf + ret, "[15] prio_pinning enable\n");
	ret += sprintf(buf + ret, "	# echo <15,mode,level,group> <enable> > aio_tuner\n");
	ret += sprintf(buf + ret, "	(enable=1 disable=0)\n");
	ret += sprintf(buf + ret, "	(group0/1/2/3=root/fg/bg/ta)\n");
	ret += sprintf(buf + ret, "\n");
	ret += sprintf(buf + ret, "[16] prio_pinning prio\n");
	ret += sprintf(buf + ret, "	# echo <16,mode,level> <prio> > aio_tuner\n");
	ret += sprintf(buf + ret, "	(0 <= prio < 140\n");
	ret += sprintf(buf + ret, "\n");
	ret += sprintf(buf + ret, "[17] frt_coverage_ratio\n");
	ret += sprintf(buf + ret, "	# echo <17,mode,level,cpu> <ratio> > aio_tuner\n");
	ret += sprintf(buf + ret, "	(set one cpu, coregroup to which cpu belongs is set))\n");
	ret += sprintf(buf + ret, "\n");
	ret += sprintf(buf + ret, "[18] ecs update threshold\n");
	ret += sprintf(buf + ret, "	# echo <18,mode,level,id> <threshold> > aio_tuner\n");
	ret += sprintf(buf + ret, "\n");
	ret += sprintf(buf + ret, "[19] wakeup-boost\n");
	ret += sprintf(buf + ret, "	# echo <19,mode,level,cpu,group> <ratio> > aio_tuner\n");
	ret += sprintf(buf + ret, "	(group0/1/2/3=root/fg/bg/ta)\n");
	ret += sprintf(buf + ret, "\n");
	ret += sprintf(buf + ret, "[20] esg rate limit\n");
	ret += sprintf(buf + ret, "	# echo <20,mode,level,up/down> <ms> > aio_tuner\n");
	ret += sprintf(buf + ret, "	(up=0 down=1)\n");
	ret += sprintf(buf + ret, "\n");
	ret += sprintf(buf + ret, "[21] esg rapid scale\n");
	ret += sprintf(buf + ret, "	# echo <21,mode,level,up/down> <enable> > aio_tuner\n");
	ret += sprintf(buf + ret, "	(up=0 down=1)\n");
	ret += sprintf(buf + ret, "\n");
	ret += sprintf(buf + ret, "[22] tiny cd sched enable\n");
	ret += sprintf(buf + ret, "	# echo <22,mode,level,group> <en/dis> > aio_tuner\n");
	ret += sprintf(buf + ret, "	(enable=1 disable=0)\n");
	ret += sprintf(buf + ret, "	(group0/1/2/3=root/fg/bg/ta)\n");
	ret += sprintf(buf + ret, "\n");
	return ret;
}

enum {
	sched_policy,
	weight,
	idle_weight,
	freq_boost,
	esg_step,
	esg_patient_mode,
	esg_pelt_margin,
	esg_pelt_boost,
	ontime_en,
	ontime_add_dom,
	ontime_remove_dom,
	ontime_bdr,
	cpus_allowed_tsc,
	cpus_allowed,
	prio_pinning_mask,
	prio_pinning,
	prio_pinning_prio,
	frt_active_ratio,
	ecs_threshold,
	wakeup_boost,
	esg_rate_limit,
	esg_rapid_scale,
	tiny_cd_sched_en,
	field_count,
};

#define STR_LEN 10
static int cut_hexa_prefix(char *str)
{
	int i;

	if (!(str[0] == '0' && str[1] == 'x'))
		return -EINVAL;

	for (i = 0; i + 2 < STR_LEN; i++) {
		str[i] = str[i + 2];
		str[i + 2] = '\n';
	}

	return 0;
}

static int sanity_check_default(int field, int mode, int level)
{
	if (field < 0 || field >= field_count)
		return -EINVAL;
	if (mode < 0 || mode >= emstune_mode_count)
		return -EINVAL;
	if (level < 0 || level >= emstune_level_count)
		return -EINVAL;

	return 0;
}

static int sanity_check_option(int cpu, int group, int type)
{
	if (cpu < 0 || cpu > NR_CPUS)
		return -EINVAL;
	if (group < 0)
		return -EINVAL;
	if (!(type == 0 || type == 1))
		return -EINVAL;

	return 0;
}

enum val_type {
	VAL_TYPE_ONOFF,
	VAL_TYPE_RATIO,
	VAL_TYPE_RATIO_ALLOW_NEGATIVE,
	VAL_TYPE_LEVEL,
	VAL_TYPE_CPU,
	VAL_TYPE_MASK,
};

static int
sanity_check_convert_value(char *arg, enum val_type type, int limit, void *v)
{
	int ret = 0;
	long value;

	switch (type) {
	case VAL_TYPE_ONOFF:
		if (kstrtol(arg, 10, &value))
			return -EINVAL;
		*(int *)v = !!(int)value;
		break;
	case VAL_TYPE_RATIO_ALLOW_NEGATIVE:
		if (kstrtol(arg, 10, &value))
			return -EINVAL;
		if (value <= -limit || value >= limit)
			return -EINVAL;
		*(int *)v = (int)value;
		break;
	case VAL_TYPE_RATIO:
	case VAL_TYPE_LEVEL:
	case VAL_TYPE_CPU:
		if (kstrtol(arg, 10, &value))
			return -EINVAL;
		if (value < 0 || value >= limit)
			return -EINVAL;
		*(int *)v = (int)value;
		break;
	case VAL_TYPE_MASK:
		if (cut_hexa_prefix(arg))
			return -EINVAL;
		if (cpumask_parse(arg, (struct cpumask *)v) ||
		    cpumask_empty((struct cpumask *)v))
			return -EINVAL;
		break;
	}

	return ret;
}

static void
set_value_coregroup(int (field)[VENDOR_NR_CPUS], int cpu, int v)
{
	int i;

	for_each_cpu(i, cpu_coregroup_mask(cpu))
		field[i] = v;
}


static void set_value_cgroup(int (field)[STUNE_GROUP_COUNT], int group, int v)
{
	if (group >= STUNE_GROUP_COUNT) {
		for (group = 0; group < STUNE_GROUP_COUNT; group++)
			field[group] = v;
		return;
	}

	field[group] = v;
}

static void
set_value_cgroup_coregroup(int (field)[STUNE_GROUP_COUNT][VENDOR_NR_CPUS],
						int group, int cpu, int v)
{
	int i;

	if (group >= STUNE_GROUP_COUNT) {
		for (group = 0; group < STUNE_GROUP_COUNT; group++)
			for_each_cpu(i, cpu_coregroup_mask(cpu))
				field[group][i] = v;
		return;
	}

	for_each_cpu(i, cpu_coregroup_mask(cpu))
		field[group][i] = v;
}

#define NUM_OF_KEY	(10)
static ssize_t
store_aio_tuner(struct kobject *k, struct kobj_attribute *attr,
				const char *buf, size_t count)
{
	struct emstune_set *set;
	char arg0[100], arg1[100], *_arg, *ptr;
	int keys[NUM_OF_KEY], i, ret, v;
	long val;
	int mode, level, field, cpu, group, type;
	struct cpumask mask;

	if (sscanf(buf, "%99s %99s", &arg0, &arg1) != 2)
		return -EINVAL;

	/* fill keys with default value(-1) */
	for (i = 0; i < NUM_OF_KEY; i++)
		keys[i] = -1;

	/* classify key input value by comma(,) */
	_arg = arg0;
	ptr = strsep(&_arg, ",");
	i = 0;
	while (ptr != NULL && (i < NUM_OF_KEY)) {
		/* index of keys array should be under NUM_OF_KEY */
		if (i == NUM_OF_KEY)
			return -EINVAL;

		ret = kstrtol(ptr, 10, &val);
		if (ret)
			return ret;

		keys[i] = (int)val;
		i++;

		ptr = strsep(&_arg, ",");
	};

	field = keys[0];
	mode = keys[1];
	level = keys[2];

	if (sanity_check_default(field, mode, level))
		return -EINVAL;

	set = &emstune_modes[mode].sets[level];

	switch (field) {
	case sched_policy:
		group = keys[3];
		if (sanity_check_option(0, group, 0))
			return -EINVAL;
		if (sanity_check_convert_value(arg1,
				VAL_TYPE_LEVEL, NUM_OF_SCHED_POLICY, &v))
			return -EINVAL;
		set_value_cgroup(set->sched_policy.policy, group, v);
		set->sched_policy.overriding = true;
		break;
#define MAX_RATIO	10000	/* 10000% */
	case weight:
		cpu = keys[3];
		group = keys[4];
		if (sanity_check_option(cpu, group, 0))
			return -EINVAL;
		if (sanity_check_convert_value(arg1,
				VAL_TYPE_RATIO, MAX_RATIO, &v))
			return -EINVAL;
		set_value_cgroup_coregroup(set->weight.ratio, group, cpu, v);
		set->weight.overriding = true;
		break;
	case idle_weight:
		cpu = keys[3];
		group = keys[4];
		if (sanity_check_option(cpu, group, 0))
			return -EINVAL;
		if (sanity_check_convert_value(arg1,
				VAL_TYPE_RATIO, MAX_RATIO, &v))
			return -EINVAL;
		set_value_cgroup_coregroup(set->idle_weight.ratio, group, cpu, v);
		set->idle_weight.overriding = true;
		break;
	case freq_boost:
		cpu = keys[3];
		group = keys[4];
		if (sanity_check_option(cpu, group, 0))
			return -EINVAL;
		if (sanity_check_convert_value(arg1,
				VAL_TYPE_RATIO_ALLOW_NEGATIVE, MAX_RATIO, &v))
			return -EINVAL;
		set_value_cgroup_coregroup(set->freq_boost.ratio, group, cpu, v);
		set->freq_boost.overriding = true;
		break;
#define MAX_ESG_STEP 20
	case esg_step:
		cpu = keys[3];
		if (sanity_check_option(cpu, 0, 0))
			return -EINVAL;
		if (sanity_check_convert_value(arg1,
				VAL_TYPE_LEVEL, MAX_ESG_STEP, &v))
			return -EINVAL;
		set_value_coregroup(set->esg.step, cpu, v);
		set->esg.overriding = true;
		break;
#define MAX_PATIENT_TICK	100
	case esg_patient_mode:
		cpu = keys[3];
		if (sanity_check_option(cpu, 0, 0))
			return -EINVAL;
		if (sanity_check_convert_value(arg1,
				VAL_TYPE_LEVEL, MAX_PATIENT_TICK, &v))
			return -EINVAL;
		set_value_coregroup(set->esg.patient_mode, cpu, v);
		set->esg.overriding = true;
		break;
#define MAX_PELT_MARGIN		1000
	case esg_pelt_margin:
		cpu = keys[3];
		if (sanity_check_option(cpu, 0, 0))
			return -EINVAL;
		if (sanity_check_convert_value(arg1,
				VAL_TYPE_RATIO_ALLOW_NEGATIVE, MAX_PELT_MARGIN, &v))
			return -EINVAL;
		set_value_coregroup(set->esg.pelt_margin, cpu, v);
		set->esg.overriding = true;
		break;
	case esg_pelt_boost:
		cpu = keys[3];
		if (sanity_check_option(cpu, 0, 0))
			return -EINVAL;
		if (sanity_check_convert_value(arg1,
				VAL_TYPE_RATIO_ALLOW_NEGATIVE, MAX_RATIO, &v))
			return -EINVAL;
		set_value_coregroup(set->esg.pelt_boost, cpu, v);
		set->esg.overriding = true;
		break;
	/*
	 * ontime domain is the core of the ontime, other properties are
	 * optional. Therefore, ontime domain must be configured to overriding.
	 */
	case ontime_en:
		group = keys[3];
		if (sanity_check_option(0, group, 0))
			return -EINVAL;
		if (sanity_check_convert_value(arg1, VAL_TYPE_ONOFF, 0, &v))
			return -EINVAL;
		set_value_cgroup(set->ontime.enabled, group, v);
		break;
	case ontime_add_dom:
		if (sanity_check_convert_value(arg1, VAL_TYPE_MASK, 0, &mask))
			return -EINVAL;
		add_ontime_domain(&set->ontime.dom_list, &mask, 1024, 0);
		set->ontime.overriding = true;
		break;
	case ontime_remove_dom:
		if (sanity_check_convert_value(arg1,
				VAL_TYPE_CPU, CONFIG_VENDOR_NR_CPUS, &v))
			return ret;
		remove_ontime_domain(&set->ontime.dom_list, (int)v);
		break;
	case ontime_bdr:
		cpu = keys[3];
		type = keys[4];
		if (sanity_check_option(cpu, 0, type))
			return -EINVAL;
		if (sanity_check_convert_value(arg1, VAL_TYPE_LEVEL, 1024, &v))
			return -EINVAL;
		if (type)
			update_ontime_domain(&set->ontime.dom_list, cpu, v, -1);
		else
			update_ontime_domain(&set->ontime.dom_list, cpu, -1, v);
		break;
	/*
	 * cpus_allowed mask is the core of the cpus_allowed, target sched class
	 * is optional. Therefore, the mask must be set to overriding.
	 */
	case cpus_allowed_tsc:
		if (kstrtol(arg1, 0, &val))
			return -EINVAL;
		val &= EMS_SCHED_CLASS_MASK;
		set->cpus_allowed.target_sched_class = 0;
		for (i = 0; i < NUM_OF_SCHED_CLASS; i++)
			if (test_bit(i, &val))
				set_bit(i, &set->cpus_allowed.target_sched_class);
		if (!set->cpus_allowed.overriding)
			for (i = 0; i < STUNE_GROUP_COUNT; i++)
				cpumask_copy(&set->cpus_allowed.mask[i],
							cpu_possible_mask);
		break;
	case cpus_allowed:
		group = keys[3];
		if (sanity_check_option(0, group, 0))
			return -EINVAL;
		if (sanity_check_convert_value(arg1, VAL_TYPE_MASK, 0, &mask))
			return -EINVAL;
		cpumask_copy(&set->cpus_allowed.mask[group], &mask);
		set->cpus_allowed.overriding = true;
		break;
	/*
	 * prio_pinning mask is the core of the prio_pinning, other properties
	 * are optional. Therefore, the mask must be set to overriding
	 */
	case prio_pinning_mask:
		if (sanity_check_convert_value(arg1, VAL_TYPE_MASK, 0, &mask))
			return -EINVAL;
		cpumask_copy(&set->prio_pinning.mask, &mask);
		set->prio_pinning.overriding = true;
		break;
	case prio_pinning:
		group = keys[3];
		if (sanity_check_option(0, group, 0))
			return -EINVAL;
		if (sanity_check_convert_value(arg1, VAL_TYPE_ONOFF, 0, &v))
			return -EINVAL;
		set_value_cgroup(set->prio_pinning.enabled, group, v);
		break;
	case prio_pinning_prio:
		if (sanity_check_convert_value(arg1,
				VAL_TYPE_LEVEL, MAX_PRIO, &v))
			return ret;
		set->prio_pinning.prio = v;
		break;
	case frt_active_ratio:
		cpu = keys[3];
		if (sanity_check_option(cpu, 0, 0))
			return -EINVAL;
		if (sanity_check_convert_value(arg1, VAL_TYPE_RATIO, 100, &v))
			return -EINVAL;
		for_each_cpu(i, cpu_coregroup_mask(cpu))
			set->frt.active_ratio[i] = v;
		set->frt.overriding = true;
		break;
	case ecs_threshold:
		i = keys[3];
		if (sanity_check_convert_value(arg1, VAL_TYPE_LEVEL, 1024, &v))
			return -EINVAL;
		update_ecs_threshold(&set->ecs.stage_list, i, v);
		break;
	case wakeup_boost:
		cpu = keys[3];
		group = keys[4];
		if (sanity_check_option(cpu, group, 0))
			return -EINVAL;
		if (sanity_check_convert_value(arg1, VAL_TYPE_RATIO, MAX_RATIO, &v))
			return -EINVAL;
		set_value_cgroup_coregroup(set->wakeup_boost.ratio, group, cpu, v);
		set->wakeup_boost.overriding = true;
		break;
	case esg_rate_limit:
		i = !!keys[3];
		if (sanity_check_convert_value(arg1, VAL_TYPE_LEVEL, 1000, &v))
			return -EINVAL;
		if (i)
			set->esg.down_rate_limit = v;
		else
			set->esg.up_rate_limit = v;
		break;
	case esg_rapid_scale:
		i = !!keys[3];
		if (sanity_check_convert_value(arg1, VAL_TYPE_ONOFF, 0, &v))
			return -EINVAL;
		if (i)
			set->esg.rapid_scale_down = v;
		else
			set->esg.rapid_scale_up = v;
		break;
	case tiny_cd_sched_en:
		group = keys[3];
		if (sanity_check_option(0, group, 0))
			return -EINVAL;
		if (sanity_check_convert_value(arg1, VAL_TYPE_ONOFF, 0, &v))
			return -EINVAL;
		set->tiny_cd_sched.enabled[group] = v;
		break;
	}

	update_cur_set(set);

	return count;
}

static struct kobj_attribute aio_tuner_attr =
__ATTR(aio_tuner, 0644, show_aio_tuner, store_aio_tuner);

/******************************************************************************
 * initialization                                                             *
 ******************************************************************************/
static int
emstune_set_init(struct device_node *dn, struct emstune_set *set)
{
	struct device_node *child;

	if (of_property_read_u32(dn, "idx", &set->idx))
		return -ENODATA;

	if (of_property_read_string(dn, "desc", &set->desc))
		return -ENODATA;

#define parse(_name)					\
	child = of_get_child_by_name(dn, #_name);	\
	if (child)					\
		parse_##_name(child, set);

	parse(sched_policy);
	parse(weight);
	parse(idle_weight);
	parse(freq_boost);
	parse(wakeup_boost);
	parse(esg);
	parse(ontime);
	parse(cpus_allowed);
	parse(prio_pinning);
	parse(frt);
	parse(ecs);
	parse(tiny_cd_sched);

	return 0;
}

#define DEFAULT_MODE	0
static int emstune_mode_init(void)
{
	struct device_node *mode_map_dn, *mode_dn, *level_dn;
	int mode_idx, level, ret, unique_id = 0;;

	mode_map_dn = of_find_node_by_path("/ems/emstune/mode-map");
	if (!mode_map_dn)
		return -EINVAL;

	emstune_mode_count = of_get_child_count(mode_map_dn);
	if (!emstune_mode_count)
		return -ENODATA;

	emstune_modes = kzalloc(sizeof(struct emstune_mode)
					* emstune_mode_count, GFP_KERNEL);
	if (!emstune_modes)
		return -ENOMEM;

	mode_idx = 0;
	for_each_child_of_node(mode_map_dn, mode_dn) {
		struct emstune_mode *mode = &emstune_modes[mode_idx];

		for (level = 0; level < MAX_MODE_LEVEL; level++) {
			mode->sets[level].unique_id = unique_id++;
			init_ontime_list(&mode->sets[level].ontime);
			init_ecs_list(&mode->sets[level].ecs);
		}

		mode->idx = mode_idx;
		if (of_property_read_string(mode_dn, "desc", &mode->desc))
			return -EINVAL;

		if (of_property_read_u32(mode_dn, "boost_level",
							&mode->boost_level))
			return -EINVAL;

		level = 0;
		for_each_child_of_node(mode_dn, level_dn) {
			struct device_node *handle =
				of_parse_phandle(level_dn, "set", 0);

			if (!handle)
				return -ENODATA;

			ret = emstune_set_init(handle, &mode->sets[level]);
			if (ret)
				return ret;

			level++;
		}

		/* the level count of each mode must be same */
		if (emstune_level_count) {
			if (emstune_level_count != level) {
				BUG_ON(1);
			}
		} else
			emstune_level_count = level;

		mode_idx++;
	}

	__emstune_mode_change(DEFAULT_MODE);

	return 0;
}

static struct kobject *emstune_kobj;

static int emstune_sysfs_init(void)
{
	emstune_kobj = kobject_create_and_add("emstune", ems_kobj);
	if (!emstune_kobj)
		return -EINVAL;

	if (sysfs_create_file(emstune_kobj, &req_mode_attr.attr))
		return -ENOMEM;

	if (sysfs_create_file(emstune_kobj, &req_mode_level_attr.attr))
		return -ENOMEM;

	if (sysfs_create_file(emstune_kobj, &cur_set_attr.attr))
		return -ENOMEM;

	if (sysfs_create_file(emstune_kobj, &aio_tuner_attr.attr))
		return -ENOMEM;

	/* HACK: Create sysfs nod only for CAMERA/gameSDK mode */
	if (sysfs_create_file(emstune_kobj, &req_cam_sub_mode_attr.attr))
		return -ENOMEM;
	if (sysfs_create_file(emstune_kobj, &req_gsdk_sub_mode_attr.attr))
		return -ENOMEM;

	return 0;
}

static struct delayed_work emstune_boost_expired_dwork;
static void emstune_boost_expired_fn(struct work_struct *work)
{
	emstune_level_change(0);
}

int emstune_init(void)
{
	int ret;

	emstune_spc_rdiv = reciprocal_value(100);

	ret = emstune_mode_init();
	if (ret) {
		pr_err("failed to initialize emstune mode with err=%d\n", ret);
		return ret;
	}

	ret = emstune_sysfs_init();
	if (ret) {
		pr_err("failed to initialize emstune sysfs with err=%d\n", ret);
		return ret;
	}

	emstune_initialized = true;

	/* Booting boost during 40 secs */
	emstune_level_change(cur_mode->boost_level);
	INIT_DELAYED_WORK(&emstune_boost_expired_dwork,
			  emstune_boost_expired_fn);
	schedule_delayed_work(&emstune_boost_expired_dwork,
			      msecs_to_jiffies(40 * MSEC_PER_SEC));

	return 0;
}
