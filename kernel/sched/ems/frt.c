#include "../sched.h"
#include "../pelt.h"
#include "ems.h"

#include <trace/events/ems.h>
#include <trace/events/ems_debug.h>

struct frt_dom {
	unsigned int		active_ratio;
	int			coregroup;
	struct cpumask		cpus;

	struct list_head	list;
	struct frt_dom		*next;
	struct frt_dom		*prev;
	/* kobject for sysfs group */
	struct kobject		kobj;
};

struct cpumask available_mask;
unsigned int frt_disable_cpufreq;

LIST_HEAD(frt_list);
DEFINE_RAW_SPINLOCK(frt_lock);
DEFINE_PER_CPU_SHARED_ALIGNED(struct frt_dom *, frt_rqs);
bool frt_initialized;

/*
 * Optional action to be done while updating the load average
 */
#define UPDATE_TG	0x1
#define SKIP_AGE_LOAD	0x2
#define DO_ATTACH	0x4

#define RATIO_SCALE_SHIFT	10
#define ratio_scale(v, r) (((v) * (r) * 10) >> RATIO_SCALE_SHIFT)

#define cpu_selected(cpu)	(cpu >= 0)

#define add_positive(_ptr, _val) do {                           \
	typeof(_ptr) ptr = (_ptr);                              \
	typeof(_val) val = (_val);                              \
	typeof(*ptr) res, var = READ_ONCE(*ptr);                \
								\
	res = var + val;                                        \
								\
	if (val < 0 && res > var)                               \
		res = 0;                                        \
								\
	WRITE_ONCE(*ptr, res);                                  \
} while (0)

#define sub_positive(_ptr, _val) do {				\
	typeof(_ptr) ptr = (_ptr);				\
	typeof(*ptr) val = (_val);				\
	typeof(*ptr) res, var = READ_ONCE(*ptr);		\
	res = var - val;					\
	if (res > var)						\
		res = 0;					\
	WRITE_ONCE(*ptr, res);					\
} while (0)

static u64 frt_cpu_util(int cpu)
{
	struct rq *rq = cpu_rq(cpu);
	u64 cpu_util;

	cpu_util = ml_cpu_util(cpu);
	cpu_util += READ_ONCE(rq->avg_rt.util_avg) + READ_ONCE(rq->avg_dl.util_avg);

	return cpu_util;
}

static struct kobject *frt_kobj;
struct frt_attr {
	struct attribute attr;
	ssize_t (*show)(struct kobject *, char *);
	ssize_t (*store)(struct kobject *, const char *, size_t count);
};

#define frt_attr_rw(_name)				\
static struct frt_attr _name##_attr =			\
__ATTR(_name, 0644, show_##_name, store_##_name)

#define frt_show(_name)								\
static ssize_t show_##_name(struct kobject *k, char *buf)			\
{										\
	struct frt_dom *dom = container_of(k, struct frt_dom, kobj);		\
										\
	return sprintf(buf, "%u\n", (unsigned int)dom->_name);		\
}

#define frt_store(_name, _type, _max)						\
static ssize_t store_##_name(struct kobject *k, const char *buf, size_t count)	\
{										\
	unsigned int val;							\
	struct frt_dom *dom = container_of(k, struct frt_dom, kobj);		\
										\
	if (!sscanf(buf, "%u", &val))						\
		return -EINVAL;							\
										\
	val = val > _max ? _max : val;						\
	dom->_name = (_type)val;						\
										\
	return count;								\
}

frt_store(active_ratio, int, 100);
frt_show(active_ratio);
frt_attr_rw(active_ratio);

static ssize_t show(struct kobject *kobj, struct attribute *at, char *buf)
{
	struct frt_attr *frtattr = container_of(at, struct frt_attr, attr);

	return frtattr->show(kobj, buf);
}

static ssize_t store(struct kobject *kobj, struct attribute *at,
		     const char *buf, size_t count)
{
	struct frt_attr *frtattr = container_of(at, struct frt_attr, attr);

	return frtattr->store(kobj, buf, count);
}

static const struct sysfs_ops frt_sysfs_ops = {
	.show	= show,
	.store	= store,
};

static struct attribute *dom_frt_attrs[] = {
	&active_ratio_attr.attr,
	NULL
};
static struct kobj_type ktype_frt = {
	.sysfs_ops	= &frt_sysfs_ops,
	.default_attrs	= dom_frt_attrs,
};

static ssize_t store_disable_cpufreq(struct kobject *kobj,
		struct kobj_attribute *attr, const char *buf,
		size_t count)
{
	unsigned int val;
	if (!sscanf(buf, "%u", &val))
		return -EINVAL;
	frt_disable_cpufreq = val;
	return count;
}

static ssize_t show_disable_cpufreq(struct kobject *kobj,
		struct kobj_attribute *attr, char *buf)
{
	return sprintf(buf, "%u\n", frt_disable_cpufreq);
}

static struct kobj_attribute disable_cpufreq_attr =
__ATTR(disable_cpufreq, 0644, show_disable_cpufreq, store_disable_cpufreq);

static struct attribute *frt_attrs[] = {
	&disable_cpufreq_attr.attr,
	NULL,
};

static const struct attribute_group frt_group = {
	.attrs = frt_attrs,
};

static const struct cpumask *get_available_cpus(void)
{
	return &available_mask;
}

static int frt_mode_update_callback(struct notifier_block *nb,
				unsigned long val, void *v)
{
	struct emstune_set *cur_set = (struct emstune_set *)v;
	struct frt_dom *dom;

	list_for_each_entry(dom, &frt_list, list) {
		int cpu = cpumask_first(&dom->cpus);

		dom->active_ratio = cur_set->frt.active_ratio[cpu];
	}

	return NOTIFY_OK;
}

static struct notifier_block frt_mode_update_notifier = {
	.notifier_call = frt_mode_update_callback,
};

static int frt_sysfs_init(void)
{
	struct frt_dom *dom;

	if (list_empty(&frt_list))
		return 0;

	frt_kobj = kobject_create_and_add("frt", ems_kobj);
	if (!frt_kobj)
		goto out;

	/* Add frt sysfs node for each coregroup */
	list_for_each_entry(dom, &frt_list, list) {
		if (kobject_init_and_add(&dom->kobj, &ktype_frt,
				frt_kobj, "coregroup%d", dom->coregroup))
			goto out;
	}

	/* add frt syfs for global control */
	if (sysfs_create_group(frt_kobj, &frt_group))
		goto out;

	emstune_register_mode_update_notifier(&frt_mode_update_notifier);

	return 0;

out:
	pr_err("FRT(%s): failed to create sysfs node\n", __func__);
	return -EINVAL;
}

static void frt_parse_dt(struct device_node *dn, struct frt_dom *dom, int cnt)
{
	struct device_node *frt, *coregroup;
	char name[15];

	frt = of_get_child_by_name(dn, "frt");
	if (!frt)
		goto disable;

	snprintf(name, sizeof(name), "coregroup%d", cnt);
	coregroup = of_get_child_by_name(frt, name);
	if (!coregroup)
		goto disable;
	dom->coregroup = cnt;

	if (of_property_read_u32(coregroup, "active-ratio", &dom->active_ratio))
		return;

	return;

disable:
	dom->coregroup = cnt;
	dom->active_ratio = 100;
	pr_err("FRT(%s): failed to parse frt node\n", __func__);
}

int frt_init(void)
{
	struct frt_dom *cur, *prev = NULL, *head = NULL;
	struct device_node *dn;
	int cpu, tcpu, cnt = 0;

	dn = of_find_node_by_path("/ems");
	if (!dn)
		return 0;

	INIT_LIST_HEAD(&frt_list);
	cpumask_setall(&available_mask);

	for_each_possible_cpu(cpu) {
		if (cpu != cpumask_first(cpu_coregroup_mask(cpu)))
			continue;

		cur = kzalloc(sizeof(struct frt_dom), GFP_KERNEL);
		if (!cur) {
			pr_err("FRT(%s): failed to allocate dom\n", __func__);
			goto put_node;
		}

		cpumask_copy(&cur->cpus, cpu_coregroup_mask(cpu));

		frt_parse_dt(dn, cur, cnt++);

		/* make linke between rt domains */
		if (head == NULL)
			head = cur;

		if (prev) {
			prev->next = cur;
			cur->prev = prev;
		}
		cur->next = head;
		head->prev = cur;

		prev = cur;

		for_each_cpu(tcpu, &cur->cpus)
			per_cpu(frt_rqs, tcpu) = cur;

		list_add_tail(&cur->list, &frt_list);
	}
	frt_sysfs_init();

	frt_initialized = true;
	pr_info("%s: frt initialized complete!\n", __func__);

put_node:
	of_node_put(dn);

	return 0;
}

/*****************************************************************************/
/*				SELECT WAKEUP CPU			     */
/*****************************************************************************/
static void heavy_tasks_tracking(void);
void frt_update_available_cpus(struct rq *rq)
{
	struct frt_dom *dom, *prev_idle_dom = NULL;
	struct cpumask mask;
	unsigned long flags;

	if (unlikely(!frt_initialized))
		return;

	if (!raw_spin_trylock_irqsave(&frt_lock, flags))
		return;

	heavy_tasks_tracking();

	cpumask_copy(&mask, cpu_active_mask);
	list_for_each_entry_reverse(dom, &frt_list, list) {
		unsigned long dom_util_sum = 0;
		unsigned long dom_active_thr = 0;
		unsigned long capacity;
		struct cpumask active_cpus;
		int first_cpu, cpu;

		cpumask_and(&active_cpus, &dom->cpus, cpu_active_mask);
		first_cpu = cpumask_first(&active_cpus);
		/* all cpus of domain is offed */
		if (first_cpu >= VENDOR_NR_CPUS)
			continue;

		for_each_cpu(cpu, &active_cpus)
			dom_util_sum += frt_cpu_util(cpu);

		capacity = capacity_cpu(first_cpu) * cpumask_weight(&active_cpus);
		dom_active_thr = ratio_scale(capacity, dom->active_ratio);

		/* domain is idle */
		if (dom_util_sum < dom_active_thr) {
			/* if prev domain is also idle, clear prev domain cpus */
			if (prev_idle_dom)
				cpumask_andnot(&mask, &mask, &prev_idle_dom->cpus);
			prev_idle_dom = dom;
		}

		trace_frt_available_cpus(first_cpu, dom_util_sum,
			dom_active_thr, *(unsigned int *)cpumask_bits(&mask));
	}

	cpumask_copy(&available_mask, &mask);
	raw_spin_unlock_irqrestore(&frt_lock, flags);
}

static int find_idle_cpu(struct task_struct *task)
{
	int cpu, best_cpu = -1;
	int cpu_prio, max_prio = -1;
	u64 cpu_util, min_util = ULLONG_MAX;
	struct cpumask candidate_cpus;
	struct frt_dom *dom, *prefer_dom;

	cpumask_and(&candidate_cpus, task->cpus_ptr, cpu_active_mask);
	cpumask_and(&candidate_cpus, &candidate_cpus, get_available_cpus());
	cpumask_and(&candidate_cpus, &candidate_cpus, emstune_cpus_allowed(task));
	if (unlikely(cpumask_empty(&candidate_cpus)))
		cpumask_copy(&candidate_cpus, task->cpus_ptr);

	prefer_dom = dom = per_cpu(frt_rqs, 0);
	if (unlikely(!dom))
		return best_cpu;
	do {
		for_each_cpu_and(cpu, &dom->cpus, &candidate_cpus) {
			if (!idle_cpu(cpu))
				continue;

			if (!ecs_cpu_available(cpu, task))
				continue;

			cpu_prio = cpu_rq(cpu)->rt.highest_prio.curr;
			if (cpu_prio < max_prio)
				continue;

			cpu_util = frt_cpu_util(cpu);

			if ((cpu_prio > max_prio)
				|| (cpu_util < min_util)
				|| (cpu_util == min_util && task_cpu(task) == cpu)) {
				min_util = cpu_util;
				max_prio = cpu_prio;
				best_cpu = cpu;
			}
		}

		if (cpu_selected(best_cpu)) {
			trace_frt_select_task_rq(task, best_cpu, &candidate_cpus,
					emstune_cpus_allowed(task), "IDLE-FIRST");
			return best_cpu;
		}

		dom = dom->next;
	} while (dom != prefer_dom);

	trace_frt_select_task_rq(task, best_cpu, &candidate_cpus,
			emstune_cpus_allowed(task), "fail_idlecpu");

	return best_cpu;
}

#define TRACK_TASK_COUNT		5
s64 heavy_task_util[VENDOR_NR_CPUS];
static void update_heavy_util(struct task_struct *p, u64 thr, u64 *max)
{
	u64 util = ml_task_util(p);

	if (util > thr && util > *max) {
		*max = util;
		trace_frt_find_heavy_task(task_cpu(p), p, util);
	}
}

static void heavy_tasks_tracking(void)
{
	struct task_struct *p;
	unsigned long flags;
	int cpu;

	for_each_cpu(cpu, cpu_active_mask) {
		struct rq *rq = cpu_rq(cpu);
		s64 thr, max_util = 0;
		int task_count = 0;
#ifndef CONFIG_FAIR_GROUP_SCHED
		struct sched_entity *se;
#endif
		if (cpumask_test_cpu(cpu, cpu_coregroup_mask(0)))
			continue;

		raw_spin_lock_irqsave(&rq->lock, flags);

		if (!rq->cfs.curr)
			goto unlock;

		thr = capacity_cpu(cpu) >> 1;

#ifndef CONFIG_FAIR_GROUP_SCHED
		se = rq->cfs.curr;
		se = get_task_entity(se);

		p = container_of(se, struct task_struct, se);
		update_heavy_util(p, thr, &max_util);

		se = __pick_first_entity(cfs_rq_of(se));
		while (se && task_count < TRACK_TASK_COUNT) {
			if (!entity_is_task(se))
				goto next_entity;

			p = container_of(se, struct task_struct, se);
			update_heavy_util(p, thr, &max_util);

next_entity:
			se = __pick_next_entity(se);
			task_count++;
		}
#else
		list_for_each_entry(p, &rq->cfs_tasks, se.group_node) {
			update_heavy_util(p, thr, &max_util);

			if (task_count++ >= TRACK_TASK_COUNT)
				break;
		}
#endif

unlock:
		heavy_task_util[cpu] = max_util;
		raw_spin_unlock_irqrestore(&rq->lock, flags);
	}
}

static void find_heavy_task_cpu(int cpu, s64 *max_util, int *heavy_cpu)
{
	s64 util = heavy_task_util[cpu];
	if (util > *max_util) {
		*max_util = util;
		*heavy_cpu = cpu;
		trace_frt_find_heavy_cpu(cpu, util);
	}
}

#define FIND_MIN_UTIL	0
#define FIND_MAX_SPARE	1
static void find_best_recessive_cpu(int cpu, struct task_struct *task,
				s64 *best_val, int *best_cpu, int *backup_cpu,
				s64 *max_util, int *heavy_cpu, int type)
{
	s64 cpu_val = ml_cpu_util(cpu);
	s64 cpu_cap = capacity_cpu(cpu);

	if (type == FIND_MIN_UTIL) {
		if (cpu_val > *best_val)
			return;
	} else {
		find_heavy_task_cpu(cpu, max_util, heavy_cpu);
		cpu_val = min(cpu_val, cpu_cap);
		cpu_cap -= cpu_val;
		if (cpu_cap < *best_val)
			return;
		cpu_val = cpu_cap;
	}

	if ((cpu_val == *best_val) && !(task_cpu(task) == cpu))
		return;

	if (cpu_selected(*best_cpu))
		*backup_cpu = *best_cpu;

	*best_val = cpu_val;
	*best_cpu = cpu;
}

extern int emstune_cur_level;
extern DEFINE_PER_CPU(cpumask_var_t, local_cpu_mask);
static int find_recessive_cpu(struct task_struct *task)
{
	int cpu, backup_cpu = -1, heavy_cpu = -1, best_cpu = -1;
	struct cpumask *lowest_mask;
	struct cpumask candidate_cpus;
	struct frt_dom *dom, *prefer_dom;
	int type = emstune_cur_level == 2 ? FIND_MAX_SPARE : FIND_MIN_UTIL;
	s64 best_val = emstune_cur_level == 2 ? LONG_MIN : LONG_MAX;
	s64 max_util = LONG_MIN;

	/* Make sure the mask is initialized first */
	lowest_mask = this_cpu_cpumask_var_ptr(local_cpu_mask);
	cpupri_find(&task_rq(task)->rd->cpupri, task, lowest_mask);
	if (unlikely(!lowest_mask)) {
		trace_frt_select_task_rq(task, best_cpu, NULL, NULL, "NA LOWESTMSK");
		return best_cpu;
	}
	cpumask_and(&candidate_cpus, lowest_mask, cpu_active_mask);
	cpumask_and(&candidate_cpus, &candidate_cpus, get_available_cpus());
	cpumask_and(&candidate_cpus, &candidate_cpus, emstune_cpus_allowed(task));

	prefer_dom = dom = per_cpu(frt_rqs, 0);
	if (unlikely(!dom))
		return best_cpu;
	do {
		for_each_cpu_and(cpu, &dom->cpus, &candidate_cpus) {
			if (!ecs_cpu_available(cpu, task))
				continue;

			find_best_recessive_cpu(cpu, task, &best_val, &best_cpu,
					&backup_cpu, &max_util, &heavy_cpu, type);
		}

		if (type == FIND_MAX_SPARE && dom->next != prefer_dom)
			goto next_dom;

		if (heavy_cpu == best_cpu && cpu_selected(backup_cpu))
			best_cpu = backup_cpu;

		if (cpu_selected(best_cpu)) {
			trace_frt_select_task_rq(task, best_cpu, &candidate_cpus,
				(const struct cpumask *)lowest_mask,
				rt_task(cpu_rq(best_cpu)->curr) ? "RT-RECESS" : "FAIR-RECESS");
			return best_cpu;
		}
next_dom:
		dom = dom->next;
	} while (dom != prefer_dom);

	trace_frt_select_task_rq(task, best_cpu, &candidate_cpus,
			(const struct cpumask *)lowest_mask, "fail_recess");

	return best_cpu;
}

int frt_find_lowest_rq(struct task_struct *task)
{
	int best_cpu = -1;

	trace_frt_select_task_rq(task, best_cpu, NULL, get_available_cpus(), "start");
	if (task->nr_cpus_allowed == 1) {
		trace_frt_select_task_rq(task, best_cpu, NULL, NULL, "NA ALLOWED");
		return best_cpu;
	}

	if (!rt_task(task)) {
		trace_frt_select_task_rq(task, best_cpu, NULL, NULL, "NOT RT TASK");
		return best_cpu;
	}
	/*
	 * Fluid Sched Core selection procedure:
	 *
	 * 1. idle CPU selection
	 * 2. recessive task first
	 */

	/* 1. idle CPU selection */
	best_cpu = find_idle_cpu(task);
	if (cpu_selected(best_cpu))
		goto out;

	/* 2. recessive task first */
	best_cpu = find_recessive_cpu(task);
	if (cpu_selected(best_cpu))
		goto out;

out:
	if (!cpu_selected(best_cpu)) {
 		best_cpu = task_rq(task)->cpu;
	}

	if (!cpumask_test_cpu(best_cpu, cpu_active_mask)) {
		trace_frt_select_task_rq(task, best_cpu, NULL, NULL, "NOTHING_VALID");
		best_cpu = -1;
	}

	return best_cpu;
}
