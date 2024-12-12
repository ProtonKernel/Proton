/*
 * Exynos Core Sparing Governor - Exynos Mobile Scheduler
 *
 * Copyright (C) 2019 Samsung Electronics Co., Ltd
 * Choonghoon Park <choong.park@samsung.com>
 */

#include "../sched.h"
#include "ems.h"

#include <trace/events/ems.h>
#include <trace/events/ems_debug.h>

static int ecs_initialized;

static struct {
	unsigned long update_period;

	struct cpumask cpus;

	/* ecs governor */
	struct ecs_stage *cur_stage;
	struct list_head *stage_list;

	/* ecs requested by userspace */
	struct cpumask user_cpus;
} ecs;

#define MAX_ECS_STAGE	VENDOR_NR_CPUS

static struct {
	int ratio;
} ecs_tune[MAX_ECS_STAGE];

/******************************************************************************
 *                                core sparing	                              *
 ******************************************************************************/
struct ecs_env {
	int		src_cpu;
	int		dst_cpu;
};
static DEFINE_PER_CPU(struct ecs_env, ecs_env);
static DEFINE_PER_CPU(struct cpu_stop_work, ecs_migration_work);

static void
migrate_any_task(struct task_struct *p, struct rq *src_rq, struct rq *dst_rq)
{
	int dst_cpu = cpu_of(dst_rq);

	if (!is_dst_allowed(p, dst_cpu))
		return;

	get_task_struct(p);
	p->on_rq = TASK_ON_RQ_MIGRATING;
	deactivate_task(src_rq, p, 0);
	set_task_cpu(p, dst_cpu);

	activate_task(dst_rq, p, 0);
	p->on_rq = TASK_ON_RQ_QUEUED;
	check_preempt_curr(dst_rq, p, 0);
	put_task_struct(p);
}

static void migrate_dl_tasks(struct rq *src_rq, struct rq *dst_rq)
{
	struct dl_rq *dl_rq = &src_rq->dl;
	struct rb_node *next_node = dl_rq->pushable_dl_tasks_root.rb_leftmost;
	struct task_struct *p = NULL;

	if (RB_EMPTY_ROOT(&dl_rq->pushable_dl_tasks_root.rb_root))
		return;

next_node:
	if (next_node) {
		p = rb_entry(next_node, struct task_struct, pushable_dl_tasks);

		migrate_any_task(p, src_rq, dst_rq);

		next_node = dl_rq->pushable_dl_tasks_root.rb_leftmost;
		goto next_node;
	}
}

static void migrate_rt_tasks(struct rq *src_rq, struct rq *dst_rq)
{
	struct plist_head *head = &src_rq->rt.pushable_tasks;
	struct task_struct *p, *temp;

	if (plist_head_empty(head))
		return;

	plist_for_each_entry_safe(p, temp, head, pushable_tasks)
		migrate_any_task(p, src_rq, dst_rq);
}

static void migrate_cfs_tasks(struct rq *src_rq, struct rq *dst_rq)
{
	struct task_struct *p, *temp;
	struct list_head *tasks;

	lockdep_assert_held(&src_rq->lock);

	tasks = &src_rq->cfs_tasks;

	list_for_each_entry_safe(p, temp, tasks, se.group_node)
		migrate_any_task(p, src_rq, dst_rq);
}

static void migrate_all_class_tasks(struct rq *src_rq, struct rq *dst_rq)
{
	migrate_dl_tasks(src_rq, dst_rq);
	migrate_rt_tasks(src_rq, dst_rq);
	migrate_cfs_tasks(src_rq, dst_rq);
}

static int ecs_migration_cpu_stop(void *data)
{
	struct ecs_env *env = data;
	struct rq *src_rq, *dst_rq;
	int src_cpu = env->src_cpu, dst_cpu = env->dst_cpu;

	/* Get source/destination runqueues */
	src_rq = cpu_rq(src_cpu);
	dst_rq = cpu_rq(dst_cpu);

	BUG_ON(src_rq == dst_rq);

	raw_spin_lock_irq(&src_rq->lock);

	/* Move task from source to destination */
	double_lock_balance(src_rq, dst_rq);
	migrate_all_class_tasks(src_rq, dst_rq);
	double_unlock_balance(src_rq, dst_rq);

	src_rq->active_balance = 0;

	raw_spin_unlock_irq(&src_rq->lock);

	return 0;
}

static void __move_from_spared_cpus(int src_cpu, int dst_cpu)
{
	struct ecs_env *env = &per_cpu(ecs_env, src_cpu);

	if (unlikely(src_cpu == dst_cpu))
		return;

	if (cpu_rq(src_cpu)->active_balance)
		return;

	env->src_cpu = src_cpu;
	env->dst_cpu = dst_cpu;
	cpu_rq(src_cpu)->active_balance = 1;

	/* Migrate all tasks from src to dst through stopper */
	stop_one_cpu_nowait(src_cpu, ecs_migration_cpu_stop, env,
			&per_cpu(ecs_migration_work, src_cpu));
}

static void move_from_spared_cpus(struct cpumask *spared_cpus)
{
	int cpu;

	for_each_cpu(cpu, spared_cpus)
		__move_from_spared_cpus(cpu, 0);
}

static void update_ecs_cpus(void)
{
	struct cpumask spared_cpus, prev_cpus, complement_cpus;

	cpumask_copy(&prev_cpus, &ecs.cpus);

	cpumask_and(&ecs.cpus, &ecs.cur_stage->cpus, cpu_possible_mask);
	cpumask_and(&ecs.cpus, &ecs.cpus, &ecs.user_cpus);

	cpumask_andnot(&complement_cpus, &prev_cpus, &ecs.cpus);
	if (cpumask_empty(&complement_cpus))
		return;

	cpumask_andnot(&spared_cpus, cpu_active_mask, &ecs.cpus);
	if (!cpumask_empty(&spared_cpus))
		move_from_spared_cpus(&spared_cpus);
}

int ecs_cpu_available(int cpu, struct task_struct *p)
{
	if (!ecs_initialized || (p && is_kthread_per_cpu(p)))
		return true;

	return cpumask_test_cpu(cpu, &ecs.cpus);
}

const struct cpumask *ecs_cpus_allowed(struct task_struct *p)
{
	if (!ecs_initialized || (p && is_kthread_per_cpu(p)))
		return cpu_active_mask;

	return &ecs.cpus;
}

/******************************************************************************
 *                             core sparing governor                          *
 ******************************************************************************/
static struct ecs_stage default_stage = {
	.id = 0,
	.busy_threshold = UINT_MAX,
};

static LIST_HEAD(default_stage_list);

static inline struct ecs_stage *first_stage(void)
{
	return list_first_entry(ecs.stage_list, struct ecs_stage, node);
}

static inline struct ecs_stage *last_stage(void)
{
	return list_last_entry(ecs.stage_list, struct ecs_stage, node);
}

static bool need_backward(int monitor_util_sum)
{
	return monitor_util_sum < ecs.cur_stage->busy_threshold >> 1;
}

static bool prev_stage(int monitor_util_sum)
{
	bool move_stage = false;

	do {
		/* is current first stage? */
		if (ecs.cur_stage == first_stage())
			break;

		move_stage = true;
		ecs.cur_stage = list_prev_entry(ecs.cur_stage, node);
	} while (need_backward(monitor_util_sum));

	return move_stage;
}

static bool need_forward(int monitor_util_sum)
{
	return monitor_util_sum > ecs.cur_stage->busy_threshold;
}

static bool next_stage(int monitor_util_sum)
{
	bool move_stage = false;

	do {
		/* is current last stage? */
		if (ecs.cur_stage == last_stage())
			break;

		move_stage = true;
		ecs.cur_stage = list_next_entry(ecs.cur_stage, node);
	} while (need_forward(monitor_util_sum));

	return move_stage;
}

static void update_ecs_stage(void)
{
	int cpu;
	int monitor_util_sum = 0, prev_threshold;
	struct cpumask mask, prev_mask;

	if (!ecs.cur_stage)
		return;

	cpumask_and(&mask, &ecs.cur_stage->monitor_cpus, cpu_online_mask);
	if (cpumask_empty(&mask))
		return;

	for_each_cpu(cpu, &mask)
		monitor_util_sum += ml_cpu_util(cpu);

	if (need_forward(monitor_util_sum)) {
		prev_threshold = ecs.cur_stage->busy_threshold;
		cpumask_copy(&prev_mask, &ecs.cur_stage->cpus);
		if (next_stage(monitor_util_sum)) {
			update_ecs_cpus();
			trace_ecs_update_stage("forward", ecs.cur_stage->id,
				monitor_util_sum,
				prev_threshold,
				*(unsigned int *)cpumask_bits(&prev_mask),
				*(unsigned int *)cpumask_bits(&ecs.cur_stage->cpus));
		}
	}

	if (need_backward(monitor_util_sum)) {
		prev_threshold = ecs.cur_stage->busy_threshold >> 1;
		cpumask_copy(&prev_mask, &ecs.cur_stage->cpus);
		if (prev_stage(monitor_util_sum)) {
			update_ecs_cpus();
			trace_ecs_update_stage("backward", ecs.cur_stage->id,
				monitor_util_sum,
				prev_threshold,
				*(unsigned int *)cpumask_bits(&prev_mask),
				*(unsigned int *)cpumask_bits(&ecs.cur_stage->cpus));
		}
	}
}

static DEFINE_RAW_SPINLOCK(ecs_gov_lock);
static unsigned long last_update_jiffies;

void ecs_update(void)
{
	unsigned long now = jiffies;

	if (!ecs_initialized)
		return;

	if (!raw_spin_trylock(&ecs_gov_lock))
		return;

	if (!ecs.stage_list || list_empty(ecs.stage_list)) {
		raw_spin_unlock(&ecs_gov_lock);
		return;
	}

	if (is_sysbusy()) {
		if (ecs.cur_stage != last_stage()) {
			struct cpumask prev_mask;
			cpumask_copy(&prev_mask, &ecs.cur_stage->cpus);
			ecs.cur_stage = last_stage();
			update_ecs_cpus();
			trace_ecs_update_stage("sysbusy", ecs.cur_stage->id,
				0,
				0,
				*(unsigned int *)cpumask_bits(&prev_mask),
				*(unsigned int *)cpumask_bits(&ecs.cur_stage->cpus));

		}
		raw_spin_unlock(&ecs_gov_lock);

		return;
	}

	if (now <= last_update_jiffies + msecs_to_jiffies(ecs.update_period)) {
		raw_spin_unlock(&ecs_gov_lock);
		return;
	}

	update_ecs_stage();

	last_update_jiffies = now;

	raw_spin_unlock(&ecs_gov_lock);
}

/******************************************************************************
 *                       emstune mode update notifier                         *
 ******************************************************************************/
static int ecs_mode_update_callback(struct notifier_block *nb,
				unsigned long val, void *v)
{
	struct emstune_set *cur_set = (struct emstune_set *)v;
	struct ecs_stage *stage;
	unsigned long flags;

	raw_spin_lock_irqsave(&ecs_gov_lock, flags);

	if (cur_set->ecs.p_stage_list &&
	    !list_empty(cur_set->ecs.p_stage_list))
		ecs.stage_list = cur_set->ecs.p_stage_list;
	else
		ecs.stage_list = &default_stage_list;

	list_for_each_entry(stage, ecs.stage_list, node) {
		stage->busy_threshold =
			stage->busy_threshold_orig *
			ecs_tune[stage->id].ratio / 100;
	}

	ecs.cur_stage = first_stage();
	update_ecs_cpus();

	raw_spin_unlock_irqrestore(&ecs_gov_lock, flags);

	return NOTIFY_OK;
}

static struct notifier_block ecs_mode_update_notifier = {
	.notifier_call = ecs_mode_update_callback,
};

/******************************************************************************
 *                                    sysfs                                   *
 ******************************************************************************/
static struct kobject *ecs_kobj;

static ssize_t show_ecs_stage(struct kobject *kobj,
		struct kobj_attribute *attr, char *buf)
{
	struct ecs_stage *stage;
	int ret = 0;
	struct list_head *list = ecs.stage_list;

	if (list_empty(list))
		return sprintf(buf, "stage list is empty\n");

	ret += snprintf(buf + ret, PAGE_SIZE - ret,
			"-----------------------------\n");
	list_for_each_entry(stage, list, node) {
		ret += snprintf(buf + ret, PAGE_SIZE - ret,
				" id             : %d\n",
				stage->id);
		ret += snprintf(buf + ret, PAGE_SIZE - ret,
				" cpus           : %*pbl\n",
				cpumask_pr_args(&stage->cpus));
		ret += snprintf(buf + ret, PAGE_SIZE - ret,
				" monitor cpus   : %*pbl\n",
				cpumask_pr_args(&stage->monitor_cpus));
		ret += snprintf(buf + ret, PAGE_SIZE - ret,
				" busy threshold : %d\n",
				stage->busy_threshold);
		ret += snprintf(buf + ret, PAGE_SIZE - ret,
				" busy threshold : %d (origin)\n",
				stage->busy_threshold_orig);
		ret += snprintf(buf + ret, PAGE_SIZE - ret,
				"-----------------------------\n");
	}
	ret += snprintf(buf + ret, PAGE_SIZE - ret,
			"usage: echo <id> <thresh> > stage\n");

	return ret;
}

static ssize_t store_ecs_stage(struct kobject *kobj,
		struct kobj_attribute *attr, const char *buf, size_t count)
{
	struct ecs_stage *stage;
	int id, busy_threshold;

	if (!sscanf(buf, "%d %d", &id, &busy_threshold))
		return -EINVAL;

	if (id < 0 || busy_threshold < 0)
		return -EINVAL;

	list_for_each_entry(stage, ecs.stage_list, node) {
		if (stage->id == id) {
			stage->busy_threshold = busy_threshold;
			break;
		}
	}

	return count;
}

static struct kobj_attribute ecs_stage =
__ATTR(stage, 0644, show_ecs_stage, store_ecs_stage);

static ssize_t show_ecs_cpus(struct kobject *kobj,
		struct kobj_attribute *attr, char *buf)
{
	return sprintf(buf, "cpus : %#x\n", ecs.cpus);
}

static struct kobj_attribute ecs_cpus =
__ATTR(cpus, 0444, show_ecs_cpus, NULL);

static ssize_t show_ecs_req_cpus(struct kobject *kobj,
		struct kobj_attribute *attr, char *buf)
{
	return sprintf(buf, "request : %#x\n", ecs.user_cpus);
}

#define STR_LEN (6)
static ssize_t store_ecs_req_cpus(struct kobject *kobj,
		struct kobj_attribute *attr, const char *buf, size_t count)
{
	int ret;
	char str[STR_LEN];
	struct cpumask mask;
	unsigned long flags;

	if (strlen(buf) >= STR_LEN)
		return -EINVAL;

	if (!sscanf(buf, "%s", str))
		return -EINVAL;

	if (str[0] == '0' && str[1] =='x')
		ret = cpumask_parse(str + 2, &mask);
	else
		ret = cpumask_parse(str, &mask);

	if (ret){
		pr_err("input of req_cpus(%s) is not correct\n", buf);
		return -EINVAL;
	}

	raw_spin_lock_irqsave(&ecs_gov_lock, flags);
	cpumask_copy(&ecs.user_cpus, &mask);

	update_ecs_cpus();
	raw_spin_unlock_irqrestore(&ecs_gov_lock, flags);

	return count;
}

static struct kobj_attribute ecs_req_cpus =
__ATTR(req_cpus, 0644, show_ecs_req_cpus, store_ecs_req_cpus);

static ssize_t show_ecs_update_period(struct kobject *kobj,
		struct kobj_attribute *attr, char *buf)
{
	return sprintf(buf, "%d\n", ecs.update_period);
}

static ssize_t store_ecs_update_period(struct kobject *kobj,
		struct kobj_attribute *attr, const char *buf, size_t count)
{
	unsigned long period;

	if (!sscanf(buf, "%d", &period))
		return -EINVAL;

	ecs.update_period = period;

	return count;
}

static struct kobj_attribute ecs_update_period =
__ATTR(update_period, 0644, show_ecs_update_period, store_ecs_update_period);

static ssize_t show_ecs_ratio(struct kobject *kobj,
		struct kobj_attribute *attr, char *buf)
{
	int ret = 0, id;
	struct ecs_stage *stage;

	list_for_each_entry(stage, ecs.stage_list, node) {
		id = stage->id;
		ret += snprintf(buf + ret, PAGE_SIZE - ret,
			"id=%d ratio=%d busy-threshold=%d origin-busy-threshold=%d\n",
			id, ecs_tune[id].ratio, stage->busy_threshold,
			stage->busy_threshold_orig);
	}

	return ret;
}

static ssize_t store_ecs_ratio(struct kobject *kobj,
		struct kobj_attribute *attr, const char *buf, size_t count)
{
	int id, ratio;
	struct ecs_stage *stage;
	unsigned long flags;

	if (sscanf(buf, "%d %d", &id, &ratio) != 2)
		return -EINVAL;

	if (id < 0 || id >= MAX_ECS_STAGE)
		return -EINVAL;

	raw_spin_lock_irqsave(&ecs_gov_lock, flags);

	ecs_tune[id].ratio = ratio;
	list_for_each_entry(stage, ecs.stage_list, node) {
		if (stage->id == id) {
			stage->busy_threshold =
				stage->busy_threshold_orig
				* ecs_tune[id].ratio / 100;
			break;
		}
	}

	raw_spin_unlock_irqrestore(&ecs_gov_lock, flags);

	return count;
}

static struct kobj_attribute ecs_ratio =
__ATTR(ratio, 0644, show_ecs_ratio, store_ecs_ratio);

static int ecs_sysfs_init(void)
{
	int ret;

	ecs_kobj = kobject_create_and_add("ecs", ems_kobj);
	if (!ecs_kobj) {
		pr_info("%s: fail to create node\n", __func__);
		return -EINVAL;
	}

	ret = sysfs_create_file(ecs_kobj, &ecs_stage.attr);
	if (ret)
		pr_warn("%s: failed to create ecs sysfs\n", __func__);
	ret = sysfs_create_file(ecs_kobj, &ecs_cpus.attr);
	if (ret)
		pr_warn("%s: failed to create ecs sysfs\n", __func__);
	ret = sysfs_create_file(ecs_kobj, &ecs_req_cpus.attr);
	if (ret)
		pr_warn("%s: failed to create ecs sysfs\n", __func__);
	ret = sysfs_create_file(ecs_kobj, &ecs_update_period.attr);
	if (ret)
		pr_warn("%s: failed to create ecs sysfs\n", __func__);
	ret = sysfs_create_file(ecs_kobj, &ecs_ratio.attr);
	if (ret)
		pr_warn("%s: failed to create ecs sysfs\n", __func__);

	return ret;
}

/******************************************************************************
 *                               initialization                               *
 ******************************************************************************/
static int
init_ecs_stage(struct device_node *dn, struct ecs_stage *stage, int id)
{
	const char *buf;

	if (of_property_read_string(dn, "cpus", &buf)) {
		pr_err("%s: cpus property is omitted\n", __func__);
		return -EINVAL;
	} else
		cpulist_parse(buf, &stage->cpus);

	if (of_property_read_string(dn, "monitor-cpus", &buf)) {
		pr_err("%s: cpus property is omitted\n", __func__);
		return -EINVAL;
	} else
		cpulist_parse(buf, &stage->monitor_cpus);

	if (of_property_read_u32(dn, "busy-threshold", &stage->busy_threshold))
		stage->busy_threshold = UINT_MAX;

	stage->id = id;
	list_add_tail(&stage->node, &default_stage_list);

	return 0;
}

int ecs_init(void)
{
	struct device_node *dn, *child;
	struct ecs_stage *stage;
	int id = 0;

	/* 16 msec in default */
	ecs.update_period = 16;

	cpumask_copy(&ecs.cpus, cpu_possible_mask);
	cpumask_copy(&ecs.user_cpus, cpu_possible_mask);
	cpumask_copy(&default_stage.cpus, cpu_possible_mask);
	cpumask_clear(&default_stage.monitor_cpus);

	ecs.cur_stage = &default_stage;
	ecs.stage_list = &default_stage_list;

	dn = of_find_node_by_path("/ems/ecs");
	if (!dn) {
		list_add_tail(&default_stage.node, &default_stage_list);
		goto skip_init;
	}

	for_each_child_of_node(dn, child) {
		stage = kzalloc(sizeof(struct ecs_stage), GFP_KERNEL);
		if (!stage) {
			pr_err("%s: fail to alloc ecs stage\n", __func__);
			return -ENOMEM;
		}

		if (init_ecs_stage(child, stage, id++)) {
			kfree(stage);
			return -EINVAL;
		}
	}

	ecs.cur_stage = first_stage();

skip_init:
	for (id = 0; id < MAX_ECS_STAGE; id++)
		ecs_tune[id].ratio = 100;

	ecs_sysfs_init();
	emstune_register_mode_update_notifier(&ecs_mode_update_notifier);

	update_ecs_cpus();

	ecs_initialized = true;

	return 0;
}
