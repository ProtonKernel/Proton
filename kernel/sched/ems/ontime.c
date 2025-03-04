/*
 * On-time Migration Feature for Exynos Mobile Scheduler (EMS)
 *
 * Copyright (C) 2018 Samsung Electronics Co., Ltd
 * LEE DAEYEONG <daeyeong.lee@samsung.com>
 */

#include "../sched.h"
#include "ems.h"

#include <trace/events/ems.h>
#include <trace/events/ems_debug.h>

bool ontime_initialized;

/****************************************************************/
/*			On-time migration			*/
/****************************************************************/
#define TASK_TRACK_COUNT	5
#define MIN_CAPACITY_CPU	0

#define cap_scale(v, s)		((v)*(s) >> SCHED_CAPACITY_SHIFT)

static struct list_head *dom_list;

static LIST_HEAD(default_dom_list);

static struct ontime_dom *get_dom(int cpu, struct list_head *list)
{
	struct ontime_dom *dom;

	list_for_each_entry(dom, list, node)
		if (cpumask_test_cpu(cpu, &dom->cpus))
			return dom;

	return NULL;
}

unsigned long get_upper_boundary(int cpu, struct task_struct *p)
{
	struct ontime_dom *dom = get_dom(cpu, dom_list);

	if (!dom)
		return ULONG_MAX;

	return dom->upper_boundary;
}

static inline unsigned long get_lower_boundary(int cpu, struct task_struct *p)
{
	struct ontime_dom *dom = get_dom(cpu, dom_list);

	if (!dom)
		return ULONG_MAX;

	return dom->lower_boundary;
}

/* Structure of ontime migration environment */
struct ontime_env {
	struct rq		*dst_rq;
	struct rq		*src_rq;
	struct task_struct	*target_task;
};
DEFINE_PER_CPU(struct ontime_env, ontime_env);

static inline int check_migrate_slower(int src, int dst)
{
	if (cpumask_test_cpu(src, cpu_coregroup_mask(dst)))
		return false;

	if (capacity_cpu(src) > capacity_cpu(dst))
		return true;
	else
		return false;
}

void ontime_select_fit_cpus(struct task_struct *p, struct cpumask *fit_cpus)
{
	struct ontime_dom *dom;
	int src_cpu = task_cpu(p);
	u32 util = ml_task_util(p);
	struct cpumask mask;
	struct list_head *list = dom_list;

	if (!ontime_initialized)
		return;

	dom = get_dom(src_cpu, list);
	if (!dom)
		return;

	/*
	 * If the task belongs to a group that does not support ontime
	 * migration, task is currently migrating or task wait time is too
	 * long(hungry state), it assigns all active cpus.
	 */
	if (!emstune_ontime(p) || ml_task_hungry(p)) {
		cpumask_copy(&mask, cpu_active_mask);
		goto done;
	}

	/*
	 * case 1) task util < lower boundary
	 *
	 * If task 'util' is smaller than lower boundary of current domain,
	 * do not target specific cpu because ontime migration is not involved
	 * in down migration. All active cpus are fit.
	 *
	 * fit_cpus = cpu_active_mask
	 */
	if (util < dom->lower_boundary) {
		cpumask_copy(&mask, cpu_active_mask);
		goto done;
	}

	cpumask_clear(&mask);

	/*
	 * case 2) lower boundary <= task util < upper boundary
	 *
	 * If task 'util' is between lower boundary and upper boundary of
	 * current domain, both current and faster domain are fit.
	 *
	 * fit_cpus = current cpus & faster cpus
	 */
	if (util < dom->upper_boundary) {
		cpumask_or(&mask, &mask, &dom->cpus);
		list_for_each_entry_continue(dom, list, node)
			cpumask_or(&mask, &mask, &dom->cpus);

		goto done;
	}

	/*
	 * case 3) task util >= upper boundary
	 *
	 * If task 'util' is greater than boundary of current domain, only
	 * faster domain is fit to gurantee cpu performance.
	 *
	 * fit_cpus = faster cpus
	 */
	list_for_each_entry_continue(dom, list, node)
		cpumask_or(&mask, &mask, &dom->cpus);

done:
	cpumask_copy(fit_cpus, &mask);
}

static struct task_struct *
pick_heavy_task(struct sched_entity *se)
{
	struct task_struct *heaviest_task = NULL;
	struct task_struct *p = container_of(se, struct task_struct, se);
	unsigned long util, max_util = 0;
	int task_count = 0;

	/*
	 * Since current task does not exist in entity list of cfs_rq,
	 * check first that current task is heavy.
	 */
	if (emstune_ontime(p)) {
		util = ml_task_util(p);
		if (util >= get_upper_boundary(task_cpu(p), p)) {
			heaviest_task = p;
			max_util = util;
		}
	}

	se = __pick_first_entity(cfs_rq_of(se));
	while (se && task_count < TASK_TRACK_COUNT) {
		/* Skip non-task entity */
		if (!entity_is_task(se))
			goto next_entity;

		p = container_of(se, struct task_struct, se);
		if (!emstune_ontime(p))
			goto next_entity;

		if (need_prio_pinning(p)) {
			heaviest_task = p;
			break;
		}

		/*
		 * Pick the task with the biggest util among tasks whose
		 * wait tiem is too long (hungry state) or whose util is
		 * greater than the upper boundary.
		 */
		util = ml_task_util(p);
		if (ml_task_hungry(p) ||
		    util >= get_upper_boundary(task_cpu(p), p)) {
			if (util > max_util) {
				heaviest_task = p;
				max_util = util;
			}
		}

next_entity:
		se = __pick_next_entity(se);
		task_count++;
	}

	return heaviest_task;
}

static bool can_migrate(struct task_struct *p, struct ontime_env *env)
{
	struct rq *src_rq = env->src_rq, *dst_rq = env->dst_rq;

	if (p->exit_state)
		return false;

	if (!cpu_active(dst_rq->cpu))
		return false;

	if (unlikely(src_rq != task_rq(p)))
		return false;

	if (unlikely(src_rq->cpu != smp_processor_id()))
		return false;

	if (src_rq->nr_running <= 1)
		return false;

	if (!cpumask_test_cpu(dst_rq->cpu, p->cpus_ptr))
		return false;

	if (task_running(src_rq, p))
		return false;

	return true;
}

static void move_task(struct task_struct *p, struct ontime_env *env)
{
	p->on_rq = TASK_ON_RQ_MIGRATING;
	deactivate_task(env->src_rq, p, 0);
	set_task_cpu(p, env->dst_rq->cpu);

	activate_task(env->dst_rq, p, 0);
	p->on_rq = TASK_ON_RQ_QUEUED;
	check_preempt_curr(env->dst_rq, p, 0);
}

static int move_specific_task(struct task_struct *target, struct ontime_env *env)
{
	struct list_head *tasks = &env->src_rq->cfs_tasks;
	struct task_struct *p, *n;

	list_for_each_entry_safe(p, n, tasks, se.group_node) {
		if (p != target)
			continue;

		move_task(p, env);
		return 1;
	}

	return 0;
}

static int ontime_migration_cpu_stop(void *data)
{
	struct ontime_env *env = data;
	struct rq *src_rq, *dst_rq;
	struct task_struct *p;

	/* Initialize environment data */
	src_rq = env->src_rq;
	dst_rq = env->dst_rq;
	p = env->target_task;

	raw_spin_lock_irq(&src_rq->lock);

	/* Check task can be migrated */
	if (!can_migrate(p, env))
		goto out_unlock;

	BUG_ON(src_rq == dst_rq);

	/* Move task from source to destination */
	double_lock_balance(src_rq, dst_rq);
	if (move_specific_task(p, env)) {
		trace_ontime_migration(p, ml_task_util(p),
				src_rq->cpu, dst_rq->cpu);
	}
	double_unlock_balance(src_rq, dst_rq);

out_unlock:
	src_rq->active_balance = 0;

	raw_spin_unlock_irq(&src_rq->lock);
	put_task_struct(p);

	return 0;
}

/****************************************************************/
/*			External APIs				*/
/****************************************************************/
DEFINE_PER_CPU(struct cpu_stop_work, ontime_migration_work);

void ontime_migration(void)
{
	struct rq *rq = this_rq();
	struct ontime_env *env = &per_cpu(ontime_env, rq->cpu);
	struct sched_entity *se;
	struct task_struct *p;
	int dst_cpu;
	unsigned long flags;
	struct ontime_dom *dom;
	struct list_head *list = dom_list;

	if (!ontime_initialized)
		return;

	dom = get_dom(cpu_of(rq), list);
	if (!dom)
		return;

	/*
	 * No need to traverse rq to find a heavy task
	 * if this CPU utilization is under upper boundary
	 */
	if (ml_cpu_util(cpu_of(rq)) < dom->upper_boundary)
		return;

	raw_spin_lock_irqsave(&rq->lock, flags);

	/*
	 * Ontime migration is not performed when active balance is in progress.
	 */
	if (rq->active_balance) {
		raw_spin_unlock_irqrestore(&rq->lock, flags);
		return;
	}

	/*
	 * No need to migration if source cpu does not have cfs tasks.
	 */
	if (!rq->cfs.curr) {
		raw_spin_unlock_irqrestore(&rq->lock, flags);
		return;
	}

	/* Find task entity if entity is cfs_rq. */
	se = rq->cfs.curr;
	se = get_task_entity(se);

	/*
	 * Pick task to be migrated.
	 * Return NULL if there is no heavy task in rq.
	 */
	p = pick_heavy_task(se);
	if (!p) {
		raw_spin_unlock_irqrestore(&rq->lock, flags);
		return;
	}

	/* Select destination cpu which the heavy task will be moved */
	dst_cpu = exynos_select_task_rq(p, rq->cpu, 0, 0);
	if (dst_cpu < 0 || rq->cpu == dst_cpu) {
		raw_spin_unlock_irqrestore(&rq->lock, flags);
		return;
	}

	get_task_struct(p);

	/* Set environment data */
	env->dst_rq = cpu_rq(dst_cpu);
	env->src_rq = rq;
	env->target_task = p;

	/* Prevent active balance to use stopper for migration */
	rq->active_balance = 1;

	raw_spin_unlock_irqrestore(&rq->lock, flags);

	/* Migrate task through stopper */
	stop_one_cpu_nowait(rq->cpu, ontime_migration_cpu_stop, env,
			&per_cpu(ontime_migration_work, rq->cpu));
}

int ontime_can_migrate_task(struct task_struct *p, int dst_cpu)
{
	int src_cpu = task_cpu(p);
	u32 util;

	if (!ontime_initialized)
		return true;

	if (!emstune_ontime(p))
		return true;

	/*
	 * Task is heavy enough but load balancer tries to migrate the task to
	 * slower cpu, it does not allow migration.
	 */
	util = ml_task_util(p);
	if (util >= get_lower_boundary(src_cpu, p) &&
	    check_migrate_slower(src_cpu, dst_cpu)) {
		int cpu_task_avg = ml_cpu_util(src_cpu) / cpu_rq(src_cpu)->nr_running;
		int task_util = ml_task_util(p);

		/*
		 * However, only if the source cpu is overutilized, it allows
		 * migration if the task is not very heavy.
		 * (criteria : task util is under 75% of cpu util)
		 */
		if (cpu_overutilized(capacity_cpu(src_cpu), ml_cpu_util(src_cpu)) &&
			ml_task_util(p) * 100 < (ml_cpu_util(src_cpu) * 75)) {
			trace_ontime_can_migrate_task(p, dst_cpu, true, "src overutil");
			return true;
		}

		if (ml_task_hungry(p) &&
			(task_util < cpu_task_avg + (cpu_task_avg >> 2))) {
			trace_ontime_can_migrate_task(p, dst_cpu, true, "hungry task");
			return true;
		}

		trace_ontime_can_migrate_task(p, dst_cpu, false, "migrate to slower");
		return false;
	}

	trace_ontime_can_migrate_task(p, dst_cpu, true, "n/a");

	return true;
}

/****************************************************************/
/*		   emstune mode update notifier			*/
/****************************************************************/
static int ontime_mode_update_callback(struct notifier_block *nb,
				unsigned long val, void *v)
{
	struct emstune_set *cur_set = (struct emstune_set *)v;

	if (cur_set->ontime.p_dom_list &&
	    !list_empty(cur_set->ontime.p_dom_list))
		dom_list = cur_set->ontime.p_dom_list;

	return NOTIFY_OK;
}

static struct notifier_block ontime_mode_update_notifier = {
	.notifier_call = ontime_mode_update_callback,
};

/****************************************************************/
/*				SYSFS				*/
/****************************************************************/
static struct kobject *ontime_kobj;

static ssize_t show_ontime_domain(struct kobject *kobj,
		struct kobj_attribute *attr, char *buf)
{
	struct ontime_dom *dom;
	int ret = 0;
	struct list_head *list = dom_list;

	ret += snprintf(buf + ret, PAGE_SIZE - ret,
			"-----------------------------\n");
	list_for_each_entry(dom, list, node) {
		ret += snprintf(buf + ret, PAGE_SIZE - ret,
				" cpus           : %*pbl\n",
				cpumask_pr_args(&dom->cpus));
		ret += snprintf(buf + ret, PAGE_SIZE - ret,
				" upper boundary : %lu\n",
				dom->upper_boundary);
		ret += snprintf(buf + ret, PAGE_SIZE - ret,
				" lower boundary : %lu\n",
				dom->lower_boundary);
		ret += snprintf(buf + ret, PAGE_SIZE - ret,
				"-----------------------------\n");
	}

	return ret;
}

static ssize_t store_ontime_domain(struct kobject *kobj,
		struct kobj_attribute *attr, const char *buf, size_t count)
{
	struct ontime_dom *dom;
	int cpu, ub, lb;

	if (!sscanf(buf, "%d %d %d", &cpu, &ub, &lb))
		return -EINVAL;

	if (cpu < 0 || cpu >= nr_cpu_ids || ub < 0 || lb < 0)
		return -EINVAL;

	dom = get_dom(cpu, dom_list);
	if (!dom)
		return -ENODATA;

	dom->upper_boundary = ub;
	dom->lower_boundary = lb;

	return count;
}

static struct kobj_attribute ontime_domain =
__ATTR(domain, 0644, show_ontime_domain, store_ontime_domain);

static int ontime_sysfs_init(void)
{
	int ret;

	ontime_kobj = kobject_create_and_add("ontime", ems_kobj);
	if (!ontime_kobj) {
		pr_info("%s: fail to create node\n", __func__);
		return -EINVAL;
	}

	ret = sysfs_create_file(ontime_kobj, &ontime_domain.attr);
	if (ret)
		pr_warn("%s: failed to create ontime sysfs\n", __func__);

	return ret;
}

/****************************************************************/
/*			initialization				*/
/****************************************************************/
static int
init_ontime_dom(struct device_node *dn, struct ontime_dom *dom)
{
	const char *buf;
	char *str_ub = "upper-boundary";
	char *str_lb = "lower-boundary";

	if (of_property_read_string(dn, "cpus", &buf)) {
		pr_err("%s: cpus property is omitted\n", __func__);
		return -EINVAL;
	} else
		cpulist_parse(buf, &dom->cpus);

	if (of_property_read_u32(dn, str_ub, (u32 *)&dom->upper_boundary))
		dom->upper_boundary = 1024;
	if (of_property_read_u32(dn, str_lb, (u32 *)&dom->lower_boundary))
		dom->lower_boundary = 0;

	list_add_tail(&dom->node, &default_dom_list);

	return 0;
}

int ontime_init(void)
{
	struct device_node *dn, *child;
	struct ontime_dom *dom;

	dn = of_find_node_by_path("/ems/ontime");
	if (!dn)
		goto skip_init;

	dom_list = &default_dom_list;
	for_each_child_of_node(dn, child) {
		dom = kzalloc(sizeof(struct ontime_dom), GFP_KERNEL);
		if (!dom) {
			pr_err("%s: fail to alloc ontime domain\n", __func__);
			return -ENOMEM;
		}

		if (init_ontime_dom(child, dom)) {
			kfree(dom);
			return -EINVAL;
		}
	}

skip_init:
	ontime_sysfs_init();
	emstune_register_mode_update_notifier(&ontime_mode_update_notifier);

	ontime_initialized = true;

	return 0;
}
