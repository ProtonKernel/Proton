/*
 * Copyright (c) 2020 samsung electronics co., ltd.
 *		http://www.samsung.com/
 *
 * Author : Choonghoon Park (choong.park@samsung.com)
 *
 * this program is free software; you can redistribute it and/or modify
 * it under the terms of the gnu general public license version 2 as
 * published by the free software foundation.
 *
 * Exynos Mobile Scheduler Notifier implementation
 */

#define pr_fmt(fmt)	KBUILD_MODNAME ": " fmt

#include <linux/init.h>
#include <linux/of.h>
#include <linux/slab.h>
#include <linux/cpu.h>
#include <linux/cpumask.h>
#include <linux/kthread.h>
#include <linux/module.h>
#include <linux/platform_device.h>

#include <linux/ems.h>

#include <uapi/linux/sched/types.h>

static int emstune_cur_mode;
static int emstune_cur_mode_level;
static int emsn_initialized = 0;

static struct kobject *emsn_kobj;

/******************************************************************************
 * simple mode level list management based on prio                            *
 ******************************************************************************/
/**
 * MLLIST_HEAD_INIT - static struct mllist_head initializer
 * @head:	struct mllist_head variable name
 */
#define MLLIST_HEAD_INIT(head)				\
{							\
	.node_list = LIST_HEAD_INIT((head).node_list)	\
}

/**
 * mllist_node_init - Dynamic struct mllist_node initializer
 * @node:	&struct mllist_node pointer
 * @prio:	initial node priority
 */
static inline void mllist_node_init(struct mllist_node *node, int prio)
{
	node->prio = prio;
	INIT_LIST_HEAD(&node->prio_list);
	INIT_LIST_HEAD(&node->node_list);
}

/**
 * mllist_head_empty - return !0 if a mllist_head is empty
 * @head:	&struct mllist_head pointer
 */
static inline int mllist_head_empty(const struct mllist_head *head)
{
	return list_empty(&head->node_list);
}

/**
 * mllist_node_empty - return !0 if mllist_node is not on a list
 * @node:	&struct mllist_node pointer
 */
static inline int mllist_node_empty(const struct mllist_node *node)
{
	return list_empty(&node->node_list);
}

/**
 * mllist_last - return the last node (and thus, lowest priority)
 * @head:	the &struct mllist_head pointer
 *
 * Assumes the mllist is _not_ empty.
 */
static inline struct mllist_node *mllist_last(const struct mllist_head *head)
{
	return list_entry(head->node_list.prev,
			  struct mllist_node, node_list);
}

/**
 * mllist_first - return the first node (and thus, highest priority)
 * @head:	the &struct mllist_head pointer
 *
 * Assumes the mllist is _not_ empty.
 */
static inline struct mllist_node *mllist_first(const struct mllist_head *head)
{
	return list_entry(head->node_list.next,
			  struct mllist_node, node_list);
}

/**
 * mllist_add - add @node to @head
 *
 * @node:	&struct mllist_node pointer
 * @head:	&struct mllist_head pointer
 */
static void mllist_add(struct mllist_node *node, struct mllist_head *head)
{
	struct mllist_node *first, *iter, *prev = NULL;
	struct list_head *node_next = &head->node_list;

	WARN_ON(!mllist_node_empty(node));
	WARN_ON(!list_empty(&node->prio_list));

	if (mllist_head_empty(head))
		goto ins_node;

	first = iter = mllist_first(head);

	do {
		if (node->prio < iter->prio) {
			node_next = &iter->node_list;
			break;
		}

		prev = iter;
		iter = list_entry(iter->prio_list.next,
				struct mllist_node, prio_list);
	} while (iter != first);

	if (!prev || prev->prio != node->prio)
		list_add_tail(&node->prio_list, &iter->prio_list);
ins_node:
	list_add_tail(&node->node_list, node_next);
}

/**
 * mllist_del - Remove a @node from mllist.
 *
 * @node:	&struct mllist_node pointer - entry to be removed
 * @head:	&struct mllist_head pointer - list head
 */
static void mllist_del(struct mllist_node *node, struct mllist_head *head)
{
	if (!list_empty(&node->prio_list)) {
		if (node->node_list.next != &head->node_list) {
			struct mllist_node *next;

			next = list_entry(node->node_list.next,
					struct mllist_node, node_list);

			/* add the next mllist_node into prio_list */
			if (list_empty(&next->prio_list))
				list_add(&next->prio_list, &node->prio_list);
		}
		list_del_init(&node->prio_list);
	}

	list_del_init(&node->node_list);
}

/**
 * mllist_for_each_entry - iterate over list of given type
 * @pos:	the type * to use as a loop counter
 * @head:	the head for your list
 * @mem:	the name of the list_head within the struct
 */
#define mllist_for_each_entry(pos, head, mem)	\
	 list_for_each_entry(pos, &(head)->node_list, mem.node_list)

/******************************************************************************
 * emstune support                                                            *
 ******************************************************************************/
static DEFINE_SPINLOCK(emstune_mode_lock);
static struct mllist_head emstune_req_list = MLLIST_HEAD_INIT(emstune_req_list);

#define MAX_MODE_LEVEL	(100)

static int get_mode_level(void)
{
	if (mllist_head_empty(&emstune_req_list))
		return 0;

	return mllist_last(&emstune_req_list)->prio;
}

static inline int emstune_request_active(struct emstune_mode_request *req)
{
	unsigned long flags;
	int active;

	spin_lock_irqsave(&emstune_mode_lock, flags);
	active = req->active;
	spin_unlock_irqrestore(&emstune_mode_lock, flags);

	return active;
}

enum emstune_req_type {
	REQ_ADD,
	REQ_UPDATE,
	REQ_REMOVE
};

static void
__emstune_update_request(struct emstune_mode_request *req,
			enum emstune_req_type type, s32 new_level)
{
	char msg[32];
	char *envp[] = { msg, NULL };
	int prev_level;
	unsigned long flags;

	spin_lock_irqsave(&emstune_mode_lock, flags);

	switch (type) {
	case REQ_REMOVE:
		mllist_del(&req->node, &emstune_req_list);
		req->active = 0;
		break;
	case REQ_UPDATE:
		mllist_del(&req->node, &emstune_req_list);
	case REQ_ADD:
		mllist_node_init(&req->node, new_level);
		mllist_add(&req->node, &emstune_req_list);
		req->active = 1;
		break;
	default:
		;
	}

	prev_level = emstune_cur_mode_level;
	emstune_cur_mode_level = get_mode_level();

	if (unlikely(!emsn_initialized)) {
		spin_unlock_irqrestore(&emstune_mode_lock, flags);
		return;
	}

	if (prev_level != emstune_cur_mode_level) {
		snprintf(msg, sizeof(msg), "REQ_MODE_LEVEL=%d", emstune_cur_mode_level);
		spin_unlock_irqrestore(&emstune_mode_lock, flags);

		if (kobject_uevent_env(emsn_kobj->parent, KOBJ_CHANGE, envp))
			pr_warn("Failed to send uevent for mode_level\n");

		return;
	}

	spin_unlock_irqrestore(&emstune_mode_lock, flags);
}

#define DEFAULT_LEVEL	(0)
static void emstune_work_fn(struct work_struct *work)
{
	struct emstune_mode_request *req = container_of(to_delayed_work(work),
						  struct emstune_mode_request,
						  work);

	emstune_update_request(req, DEFAULT_LEVEL);
}

void __emstune_add_request(struct emstune_mode_request *req, char *func, unsigned int line)
{
	if (emstune_request_active(req))
		return;

	INIT_DELAYED_WORK(&req->work, emstune_work_fn);
	req->func = func;
	req->line = line;

	__emstune_update_request(req, REQ_ADD, DEFAULT_LEVEL);
}
EXPORT_SYMBOL_GPL(__emstune_add_request);

void emstune_remove_request(struct emstune_mode_request *req)
{
	if (!emstune_request_active(req))
		return;

	if (delayed_work_pending(&req->work))
		cancel_delayed_work_sync(&req->work);
	destroy_delayed_work_on_stack(&req->work);

	__emstune_update_request(req, REQ_REMOVE, DEFAULT_LEVEL);
}
EXPORT_SYMBOL_GPL(emstune_remove_request);

void emstune_update_request(struct emstune_mode_request *req, s32 new_level)
{
	/* ignore if the request is not active */
	if (!emstune_request_active(req))
		return;

	/* ignore if the value is out of range except boost level */
	if (new_level != BOOST_LEVEL
	    && (new_level < 0 || new_level > MAX_MODE_LEVEL))
		return;

	__emstune_update_request(req, REQ_UPDATE, new_level);
}
EXPORT_SYMBOL_GPL(emstune_update_request);

void emstune_update_request_timeout(struct emstune_mode_request *req, s32 new_value,
				unsigned long timeout_us)
{
	if (delayed_work_pending(&req->work))
		cancel_delayed_work_sync(&req->work);

	emstune_update_request(req, new_value);

	schedule_delayed_work(&req->work, usecs_to_jiffies(timeout_us));
}
EXPORT_SYMBOL_GPL(emstune_update_request_timeout);

void emstune_boost(struct emstune_mode_request *req, int enable)
{
	if (enable)
		emstune_update_request(req, BOOST_LEVEL);
	else
		emstune_update_request(req, 0);
}
EXPORT_SYMBOL_GPL(emstune_boost);

void emstune_boost_timeout(struct emstune_mode_request *req, unsigned long timeout_us)
{
	emstune_update_request_timeout(req, BOOST_LEVEL, timeout_us);
}
EXPORT_SYMBOL_GPL(emstune_boost_timeout);

void emstune_mode_change(int next_mode_idx)
{
	char msg[32];
	char *envp[] = { msg, NULL };
	int prev_mode;
	unsigned long flags;

	spin_lock_irqsave(&emstune_mode_lock, flags);

	prev_mode = emstune_cur_mode;
	emstune_cur_mode = next_mode_idx;

	if (prev_mode != next_mode_idx) {
		snprintf(msg, sizeof(msg), "REQ_MODE=%d", emstune_cur_mode);
		spin_unlock_irqrestore(&emstune_mode_lock, flags);

		if (kobject_uevent_env(emsn_kobj->parent, KOBJ_CHANGE, envp))
			pr_warn("Failed to send uevent for mode\n");

		return;
	}

	spin_unlock_irqrestore(&emstune_mode_lock, flags);
}
EXPORT_SYMBOL_GPL(emstune_mode_change);

int emstune_get_cur_mode()
{
	return emstune_cur_mode;
}
EXPORT_SYMBOL_GPL(emstune_get_cur_mode);

/*********************************************************************
 *               EXYNOS CORE SPARING REQUEST function                *
 *********************************************************************/
#define ECS_USER_NAME_LEN 	(16)
struct ecs_request {
	char			name[ECS_USER_NAME_LEN];
	struct cpumask		cpus;
	struct list_head	list;
};

static struct {
	struct list_head	requests;
	struct mutex		lock;
	struct cpumask		cpus;
	struct kobject		*kogj;
} ecs;

static struct ecs_request *ecs_find_request(char *name)
{
	struct ecs_request *req;

	list_for_each_entry(req, &ecs.requests, list)
		if (!strcmp(req->name, name))
			return req;

	return NULL;
}

static struct cpumask ecs_get_cpus(void)
{
	struct cpumask mask;
	struct ecs_request *req;
	char buf[10];

	cpumask_setall(&mask);

	list_for_each_entry(req, &ecs.requests, list)
		cpumask_and(&mask, &mask, &req->cpus);

	if (cpumask_empty(&mask) || !cpumask_test_cpu(0, &mask)) {
		scnprintf(buf, sizeof(buf), "%*pbl", cpumask_pr_args(&mask));
		panic("ECS cpumask(%s) is wrong\n", buf);
	}

	return mask;
}

static int ecs_send_request(void)
{
	char msg[32];
	char *envp[] = { msg, NULL };
	struct cpumask cpus;
	int ret = 0;

	cpus = ecs_get_cpus();

	/* if there is no mask change, skip */
	if (cpumask_equal(&ecs.cpus, &cpus))
		return 0;

	cpumask_copy(&ecs.cpus, &cpus);
	snprintf(msg, sizeof(msg), "REQ_ECS_MASK=%x", cpus);
	ret = kobject_uevent_env(emsn_kobj->parent, KOBJ_CHANGE, envp);
	if (ret)
		pr_warn("Failed to send uevent for ecs_request\n");

	return ret;
}

int ecs_request_register(char *name, const struct cpumask *mask)
{
	struct ecs_request *req;
	char buf[ECS_USER_NAME_LEN];
	int ret;

	if (!emsn_initialized)
		return 0;

	mutex_lock(&ecs.lock);

	/* check whether name is already register or not */
	if (ecs_find_request(name))
		panic("Failed to register ecs request! already existed\n");

	/* allocate memory for new request */
	req = kzalloc(sizeof(struct ecs_request), GFP_KERNEL);
	if (!req) {
		mutex_unlock(&ecs.lock);
		return -ENOMEM;
	}

	/* init new request information */
	cpumask_copy(&req->cpus, mask);
	strcpy(req->name, name);

	/* register request list */
	list_add(&req->list, &ecs.requests);

	scnprintf(buf, sizeof(buf), "%*pbl", cpumask_pr_args(&req->cpus));
	pr_info("Register new request [name:%s, mask:%s]\n", req->name, buf);

	/* applying new request */
	ret = ecs_send_request();

	mutex_unlock(&ecs.lock);

	return ret;
}
EXPORT_SYMBOL_GPL(ecs_request_register);

/* remove request on the request list of exynos_core_sparing request */
int ecs_request_unregister(char *name)
{
	struct ecs_request *req;
	int ret;

	if (!emsn_initialized)
		return 0;

	mutex_lock(&ecs.lock);

	req = ecs_find_request(name);

	/* remove request from list and free */
	list_del(&req->list);
	kfree(req);

	ret = ecs_send_request();

	mutex_unlock(&ecs.lock);

	return ret;
}
EXPORT_SYMBOL_GPL(ecs_request_unregister);

int ecs_request(char *name, const struct cpumask *mask)
{
	struct ecs_request *req;
	int ret;

	if (!emsn_initialized)
		return 0;

	mutex_lock(&ecs.lock);

	req = ecs_find_request(name);
	if (!req) {
		mutex_unlock(&ecs.lock);
		return -EINVAL;
	}

	cpumask_copy(&req->cpus, mask);

	ret = ecs_send_request();

	mutex_unlock(&ecs.lock);

	return ret;
}
EXPORT_SYMBOL_GPL(ecs_request);

/*********************************************************************
 * Sysfs function                                                    *
 *********************************************************************/
#define EMSN_ATTR_RO(_name)						\
static struct emsn_attr emsn_attr_##_name =				\
__ATTR(_name, 0444, emsn_show_##_name, NULL)

#define EMSN_ATTR_RW(_name)						\
static struct emsn_attr emsn_attr_##_name =				\
__ATTR(_name, 0644, emsn_show_##_name, emsn_store_##_name)

#define to_attr(a) container_of(a, struct emsn_attr, attr)

struct emsn_attr {
	struct attribute attr;
	ssize_t (*show)(struct kobject *, struct attribute *, char *);
	ssize_t (*store)(struct kobject *, const char *, size_t count);
};

static ssize_t emsn_show_mode(struct kobject *kobj,
				struct attribute *attr, char *buf)
{
	return snprintf(buf, 30, "%d\n", emstune_cur_mode);
}

static ssize_t emsn_show_mode_level(struct kobject *kobj,
				struct attribute *attr, char *buf)
{
	return snprintf(buf, 30, "%d\n", emstune_cur_mode_level);
}

static ssize_t emsn_show_requested_mode_levels(struct kobject *kobj,
				struct attribute *attr, char *buf)
{
	struct emstune_mode_request *req;
	int ret = 0;
	int tot_reqs = 0;
	unsigned long flags;

	spin_lock_irqsave(&emstune_mode_lock, flags);
	mllist_for_each_entry(req, &emstune_req_list, node) {
		tot_reqs++;
		ret += sprintf(buf + ret, "%d: %d (%s:%d)\n", tot_reqs,
							(req->node).prio,
							req->func,
							req->line);
	}

	ret += sprintf(buf + ret, "current level: %d, requests: total=%d\n",
						emstune_cur_mode_level, tot_reqs);
	spin_unlock_irqrestore(&emstune_mode_lock, flags);

	return ret;
}

EMSN_ATTR_RO(mode);
EMSN_ATTR_RO(mode_level);
EMSN_ATTR_RO(requested_mode_levels);

static struct attribute *emsn_attrs[] = {
	&emsn_attr_mode.attr,
	&emsn_attr_mode_level.attr,
	&emsn_attr_requested_mode_levels.attr,
	NULL,
};

static ssize_t show(struct kobject *kobj, struct attribute *attr, char *buf)
{
	struct emsn_attr *eattr = to_attr(attr);
	ssize_t ret;

	ret = eattr->show(kobj, attr, buf);

	return ret;
}

static ssize_t store(struct kobject *kobj, struct attribute *attr,
		     const char *buf, size_t count)
{
	struct emsn_attr *eattr = to_attr(attr);
	ssize_t ret;

	ret = eattr->store(kobj, buf, count);

	return ret;
}

static const struct sysfs_ops emsn_sysfs_ops = {
	.show	= show,
	.store	= store,
};

static struct kobj_type emsn_ktype = {
	.sysfs_ops	= &emsn_sysfs_ops,
	.default_attrs	= emsn_attrs,
};

/*********************************************************************
 * Initialization                                                    *
 *********************************************************************/
static int exynos_msn_probe(struct platform_device *pdev)
{
	int ret = 0;

	emsn_kobj = kzalloc(sizeof(struct kobject), GFP_KERNEL);
	if (!emsn_kobj)
		return -ENOMEM;

	ret = kobject_init_and_add(emsn_kobj, &emsn_ktype, &pdev->dev.kobj, "notify");
	if (ret)
		return ret;

	/* initialize ecs_request */
	INIT_LIST_HEAD(&ecs.requests);

	mutex_init(&ecs.lock);

	emsn_initialized = 1;

	pr_info("Initialized Exynos Mobile Scheduler Notifier\n");

	return ret;
}

static const struct of_device_id of_exynos_msn_match[] = {
	{ .compatible = "samsung,exynos-msn", },
	{ },
};
MODULE_DEVICE_TABLE(of, of_exynos_msn_match);

static struct platform_driver exynos_msn_driver = {
	.driver = {
		.name	= "exynos-msn",
		.owner = THIS_MODULE,
		.of_match_table = of_exynos_msn_match,
	},
	.probe		= exynos_msn_probe,
};

module_platform_driver(exynos_msn_driver);

MODULE_DESCRIPTION("Exynos Mobile Scheduler Notifier");
MODULE_LICENSE("GPL");
