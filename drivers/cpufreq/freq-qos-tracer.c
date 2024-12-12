/*
 * Copyright (c) 2020 samsung electronics co., ltd.
 *		http://www.samsung.com/
 *
 * Author : Choonghoon Park (choong.park@samsung.com)
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * Exynos CPU Frequency QoS tracing driver implementation
 */

#define pr_fmt(fmt)	KBUILD_MODNAME ": " fmt

#include <linux/init.h>
#include <linux/of.h>
#include <linux/slab.h>
#include <linux/cpu.h>
#include <linux/cpumask.h>
#include <linux/module.h>
#include <linux/platform_device.h>

#include <soc/samsung/freq-qos-tracer.h>

struct freq_qos_tracer_request {
	struct list_head		node;

	char				*func;
	unsigned int			line;

	struct freq_qos_request 	*req;
};

struct freq_qos_tracer_domain {
	struct list_head		node;

	struct list_head		min_requests;
	struct list_head		max_requests;
	struct freq_constraints 	*constraints;

	struct kobject			kobj;
};

/*
 * list head of cpu freq constraints
 */
static LIST_HEAD(domains);

static DEFINE_SPINLOCK(freq_qos_trace_lock);

/*********************************************************************
 * Helper functions                                                  *
 *********************************************************************/
static struct freq_qos_tracer_domain *
find_freq_qos_domain(struct freq_constraints *f_const)
{
	struct freq_qos_tracer_domain *domain;

	list_for_each_entry(domain, &domains, node) {
		if (f_const == domain->constraints)
			return domain;
	}

	return NULL;
}

static struct freq_qos_tracer_request *
find_freq_qos_req(struct freq_qos_tracer_domain *domain,
		  struct freq_qos_request *f_req)
{
	struct freq_qos_tracer_request *qos_req;
	struct list_head *qos_req_list;

	if (f_req->type == FREQ_QOS_MIN)
		qos_req_list = &domain->min_requests;
	else
		qos_req_list = &domain->max_requests;

	list_for_each_entry(qos_req, qos_req_list, node) {
		if (qos_req->req == f_req)
			return qos_req;
	}

	return NULL;
}

/*********************************************************************
 * External APIs                                                     *
 *********************************************************************/
int __freq_qos_tracer_add_request(char *func, unsigned int line,
				  struct freq_constraints *f_const,
				  struct freq_qos_request *f_req,
				  enum freq_qos_req_type type, s32 value)
{
	int ret = 0;
	struct freq_qos_tracer_request *qos_req;
	struct freq_qos_tracer_domain *domain;


	ret = freq_qos_add_request(f_const, f_req, type, value);
	if (ret < 0)
		return ret;

	qos_req = kzalloc(sizeof(*qos_req), GFP_KERNEL);
	if (!qos_req) {
		ret = -ENOMEM;
		return ret;
	}

	qos_req->func = func;
	qos_req->line = line;
	qos_req->req = f_req;
	INIT_LIST_HEAD(&qos_req->node);

	domain = find_freq_qos_domain(f_const);
	if (!domain) {
		ret = -ENODEV;
		kfree(qos_req);
		return ret;
	}

	spin_lock(&freq_qos_trace_lock);

	if (type == FREQ_QOS_MIN)
		list_add(&qos_req->node, &domain->min_requests);
	else
		list_add(&qos_req->node, &domain->max_requests);

	spin_unlock(&freq_qos_trace_lock);

	return ret;
}
EXPORT_SYMBOL_GPL(__freq_qos_tracer_add_request);

int freq_qos_tracer_remove_request(struct freq_qos_request *req)
{
	int ret = 0;
	struct freq_qos_tracer_domain *domain;
	struct freq_qos_tracer_request *qos_req;

	spin_lock(&freq_qos_trace_lock);

	domain = find_freq_qos_domain(req->qos);
	if (!domain) {
		ret = -ENODEV;
		goto unlock;
	}

	qos_req = find_freq_qos_req(domain, req);
	if (!qos_req) {
		ret = -ENODEV;
		goto unlock;
	}

	list_del(&qos_req->node);
	kfree(qos_req);

	spin_unlock(&freq_qos_trace_lock);

	freq_qos_remove_request(req);

	return 0;

unlock:
	spin_unlock(&freq_qos_trace_lock);
	return ret;
}
EXPORT_SYMBOL_GPL(freq_qos_tracer_remove_request);

/*********************************************************************
 * Sysfs support                                                     *
 *********************************************************************/
struct freq_qos_tracer_attr {
	struct attribute attr;
	ssize_t (*show)(struct kobject *, char *);
	ssize_t (*store)(struct kobject *, const char *, size_t count);
};

#define to_freq_qos_tracer_dom(k)					\
	container_of(k, struct freq_qos_tracer_domain, kobj)

#define FREQ_QOS_TRACER_ATTR_RW(name)					\
static struct freq_qos_tracer_attr freq_qos_tracer_attr_##name =	\
__ATTR(name, 0644, freq_qos_tracer_##name##_show,			\
		   freq_qos_tracer_##name##_store)

#define FREQ_QOS_TRACER_ATTR_RO(name)					\
static struct freq_qos_tracer_attr freq_qos_tracer_attr_##name =	\
__ATTR(name, 0444, freq_qos_tracer_##name##_show,			\
		   NULL)

static ssize_t
freq_qos_tracer_limit_show(struct kobject *kobj, char *buf,
			   enum freq_qos_req_type type)
{
	int ret = 0;
	int tot_reqs = 0, active_reqs = 0;
	struct freq_qos_tracer_domain *domain = to_freq_qos_tracer_dom(kobj);
	struct freq_qos_tracer_request *qos_req;
	struct list_head *qos_req_list;
	s32 val;
	char *type_str;

	if (type == FREQ_QOS_MIN) {
		qos_req_list = &domain->min_requests;
		val = 0;
		type_str = "Minimum";
	} else {
		qos_req_list = &domain->max_requests;
		val = INT_MAX;
		type_str = "Maximum";
	}

	spin_lock(&freq_qos_trace_lock);
	if (list_empty(qos_req_list)) {
		snprintf(buf, PAGE_SIZE, "Empty!\n");
		goto print_total;
	}

	list_for_each_entry(qos_req, qos_req_list, node) {
		char *state = "Default";

		if (freq_qos_request_active(qos_req->req)) {
			active_reqs++;
			state = "Active";

			if (type == FREQ_QOS_MIN)
				val = max((qos_req->req->pnode).prio, val);
			else
				val = min((qos_req->req->pnode).prio, val);
		}

		tot_reqs++;

		ret += snprintf(buf + ret, PAGE_SIZE, "%d: %d: %s(%s:%d)\n", tot_reqs,
				(qos_req->req->pnode).prio, state,
				qos_req->func,
				qos_req->line);
	}

print_total:
	ret += snprintf(buf + ret, PAGE_SIZE,
			"Type=%s, Value=%d, Requests: actvie=%d / total=%d\n",
			type_str, val, active_reqs, tot_reqs);

	spin_unlock(&freq_qos_trace_lock);

	return ret;
}

static ssize_t
freq_qos_tracer_min_limit_show(struct kobject *kobj, char *buf)
{
	return freq_qos_tracer_limit_show(kobj, buf, FREQ_QOS_MIN);
}

static ssize_t
freq_qos_tracer_max_limit_show(struct kobject *kobj, char *buf)
{
	return freq_qos_tracer_limit_show(kobj, buf, FREQ_QOS_MAX);
}

FREQ_QOS_TRACER_ATTR_RO(min_limit);
FREQ_QOS_TRACER_ATTR_RO(max_limit);

static struct attribute *freq_qos_tracer_attrs[] = {
	&freq_qos_tracer_attr_min_limit.attr,
	&freq_qos_tracer_attr_max_limit.attr,
	NULL,
};

static ssize_t show(struct kobject *kobj, struct attribute *at, char *buf)
{
	struct freq_qos_tracer_attr *fvattr =
			container_of(at, struct freq_qos_tracer_attr, attr);
	return fvattr->show(kobj, buf);
}

static ssize_t store(struct kobject *kobj, struct attribute *at,
					const char *buf, size_t count)
{
	struct freq_qos_tracer_attr *fvattr =
			container_of(at, struct freq_qos_tracer_attr, attr);
	return fvattr->store(kobj, buf, count);
}

static const struct sysfs_ops freq_qos_tracer_sysfs_ops = {
	.show	= show,
	.store	= store,
};

static struct kobj_type ktype_freq_qos_tracer = {
	.sysfs_ops	= &freq_qos_tracer_sysfs_ops,
	.default_attrs	= freq_qos_tracer_attrs,
};

/*********************************************************************
 * Initialization                                                    *
 *********************************************************************/
static cpumask_var_t cpus_to_visit;
static struct kobject *pdev_kobj;

static struct notifier_block freq_qos_tracer_notifier;
static void init_constraint_done_workfn(struct work_struct *work)
{
	cpufreq_unregister_notifier(&freq_qos_tracer_notifier,
				    CPUFREQ_POLICY_NOTIFIER);
	free_cpumask_var(cpus_to_visit);
	pdev_kobj = NULL;
}
static DECLARE_WORK(init_constraint_done_work, init_constraint_done_workfn);

static int
freq_qos_tracer_callback(struct notifier_block *nb,
			 unsigned long val,
			 void *data)
{
	struct cpufreq_policy *policy = data;
	struct freq_qos_tracer_domain *domain;

	if (val != CPUFREQ_CREATE_POLICY)
		return 0;

	domain = kzalloc(sizeof(*domain), GFP_KERNEL);
	if (!domain)
		return -ENOMEM;

	INIT_LIST_HEAD(&domain->min_requests);
	INIT_LIST_HEAD(&domain->max_requests);
	INIT_LIST_HEAD(&domain->node);
	domain->constraints = &policy->constraints;
	list_add(&domain->node, &domains);

	if (kobject_init_and_add(&domain->kobj, &ktype_freq_qos_tracer,
				 pdev_kobj, "domain%d", policy->cpu))
		pr_err("Failed to init exynos freq QoS sysfs "
		       "of policy%d\n", policy->cpu);

	cpumask_andnot(cpus_to_visit, cpus_to_visit, policy->related_cpus);
	if (cpumask_empty(cpus_to_visit)) {
		pr_info("init CPU freq constraint done\n");
		schedule_work(&init_constraint_done_work);
	}

	return 0;
}

static struct notifier_block freq_qos_tracer_notifier = {
	.notifier_call = freq_qos_tracer_callback,
	.priority = INT_MIN,
};

static int freq_qos_tracer_probe(struct platform_device *pdev)
{
	int ret = 0;

	if (!alloc_cpumask_var(&cpus_to_visit, GFP_KERNEL))
		return -ENOMEM;

	cpumask_copy(cpus_to_visit, cpu_possible_mask);
	pdev_kobj = &pdev->dev.kobj;

	ret = cpufreq_register_notifier(&freq_qos_tracer_notifier,
					CPUFREQ_POLICY_NOTIFIER);
	if (ret)
		free_cpumask_var(cpus_to_visit);

	pr_info("Initialized Exynos freq QoS tracing driver\n");

	return ret;
}

static const struct of_device_id of_freq_qos_tracer_match[] = {
	{ .compatible = "samsung,freq-qos-tracer", },
	{ },
};
MODULE_DEVICE_TABLE(of, of_freq_qos_tracer_match);

static struct platform_driver freq_qos_tracer_driver = {
	.driver = {
		.name	= "freq-qos-tracer",
		.owner = THIS_MODULE,
		.of_match_table = of_freq_qos_tracer_match,
	},
	.probe		= freq_qos_tracer_probe,
};

module_platform_driver(freq_qos_tracer_driver);

MODULE_DESCRIPTION("CPUFreq QoS Tracer");
MODULE_LICENSE("GPL");
