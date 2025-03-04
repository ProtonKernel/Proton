/*
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * User Frequency & Cstate Control driver
 *
 * Copyright (C) 2020 Samsung Electronics Co., Ltd
 * Author : Park Bumgyu <bumgyu.park@samsung.com>
 */

#define pr_fmt(fmt)	KBUILD_MODNAME ": " fmt

#include <linux/cpumask.h>
#include <linux/of.h>
#include <linux/slab.h>
#include <linux/module.h>
#include <linux/platform_device.h>
#include <linux/init.h>
#include <linux/cpu.h>
#include <linux/cpufreq.h>
#include <linux/pm_opp.h>
#include <linux/ems.h>

#include <soc/samsung/exynos-cpupm.h>
#include <soc/samsung/exynos-ufcc.h>
#include <soc/samsung/freq-qos-tracer.h>

/*********************************************************************
 *                         USER CSTATE CONTROL                       *
 *********************************************************************/
static int ucc_initialized = false;

/*
 * struct ucc_config
 *
 * - index : index of cstate config
 * - cstate : mask indicating whether each cpu supports cstate
 *    - mask bit set : cstate enable
 *    - mask bit unset : cstate disable
 */
struct ucc_config {
	int		index;
	struct cpumask	cstate;
};

static struct ucc_config *ucc_configs;
static int ucc_config_count;

/*
 * cur_cstate has cpu cstate config corresponding to maximum index among ucc
 * request. Whenever maximum requested index is changed, cur_state is updated.
 */
static struct cpumask cur_cstate;

LIST_HEAD(ucc_req_list);

static DEFINE_SPINLOCK(ucc_lock);

static int ucc_get_value(void)
{
	struct ucc_req *req;

	req = list_first_entry_or_null(&ucc_req_list, struct ucc_req, list);

	return req ? req->value : -1;
}

static void update_cur_cstate(void)
{
	int value = ucc_get_value();

	if (value < 0) {
		cpumask_copy(&cur_cstate, cpu_possible_mask);
		return;
	}

	cpumask_copy(&cur_cstate, &ucc_configs[value].cstate);
}

enum {
	UCC_REMOVE_REQ,
	UCC_UPDATE_REQ,
	UCC_ADD_REQ,
};

static void list_add_priority(struct ucc_req *new, struct list_head *head)
{
	struct list_head temp;
	struct ucc_req *pivot;

	/*
	 * If new request's value is bigger than first entry,
	 * add new request to first entry.
	 */
	pivot = list_first_entry(head, struct ucc_req, list);
	if (pivot->value <= new->value) {
		list_add(&new->list, head);
		return;
	}

	/*
	 * If new request's value is smaller than last entry,
	 * add new request to last entry.
	 */
	pivot = list_last_entry(head, struct ucc_req, list);
	if (pivot->value >= new->value) {
		list_add_tail(&new->list, head);
		return;
	}

	/*
	 * Find first entry that smaller than new request.
	 * And add new request before that entry.
	 */
	list_for_each_entry(pivot, head, list) {
		if (pivot->value < new->value)
			break;
	}

	list_cut_before(&temp, head, &pivot->list);
	list_add(&new->list, head);
	list_splice(&temp, head);
}

static void ucc_update(struct ucc_req *req, int value, int action)
{
	int prev_value = ucc_get_value();

	switch (action) {
	case UCC_REMOVE_REQ:
		list_del(&req->list);
		req->active = 0;
		break;
	case UCC_UPDATE_REQ:
		list_del(&req->list);
	case UCC_ADD_REQ:
		req->value = value;
		list_add_priority(req, &ucc_req_list);
		req->active = 1;
		break;
	}

	if (prev_value != ucc_get_value())
		update_cur_cstate();
}

void ucc_add_request(struct ucc_req *req, int value)
{
	unsigned long flags;

	if (!ucc_initialized)
		return;

	spin_lock_irqsave(&ucc_lock, flags);

	if (req->active) {
		spin_unlock_irqrestore(&ucc_lock, flags);
		return;
	}

	ucc_update(req, value, UCC_ADD_REQ);

	spin_unlock_irqrestore(&ucc_lock, flags);
}
EXPORT_SYMBOL_GPL(ucc_add_request);

void ucc_update_request(struct ucc_req *req, int value)
{
	unsigned long flags;

	if (!ucc_initialized)
		return;

	spin_lock_irqsave(&ucc_lock, flags);

	if (!req->active) {
		spin_unlock_irqrestore(&ucc_lock, flags);
		return;
	}

	ucc_update(req, value, UCC_UPDATE_REQ);

	spin_unlock_irqrestore(&ucc_lock, flags);
}
EXPORT_SYMBOL_GPL(ucc_update_request);

void ucc_remove_request(struct ucc_req *req)
{
	unsigned long flags;

	if (!ucc_initialized)
		return;

	spin_lock_irqsave(&ucc_lock, flags);

	if (!req->active) {
		spin_unlock_irqrestore(&ucc_lock, flags);
		return;
	}

	ucc_update(req, 0, UCC_REMOVE_REQ);

	spin_unlock_irqrestore(&ucc_lock, flags);
}
EXPORT_SYMBOL_GPL(ucc_remove_request);

static ssize_t ucc_requests_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct ucc_req *req;
	int ret = 0;

	list_for_each_entry(req, &ucc_req_list, list)
		ret += snprintf(buf + ret, PAGE_SIZE - ret,
				"request : %d (%s)\n", req->value, req->name);

	return ret;
}
DEVICE_ATTR_RO(ucc_requests);

static struct ucc_req ucc_req =
{
	.name = "ufcc",
};

static int ucc_requested;
static int ucc_requested_val;

static void ucc_control(int value)
{
	ucc_update_request(&ucc_req, value);
	ucc_requested_val = value;
}

static ssize_t cstate_control_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	return snprintf(buf, 10, "%d\n", ucc_requested);
}

static ssize_t cstate_control_store(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t count)
{
	int input;

	if (!sscanf(buf, "%8d", &input))
		return -EINVAL;

	if (input < 0)
		return -EINVAL;

	input = !!input;
	if (input == ucc_requested)
		return count;

	ucc_requested = input;

	if (ucc_requested)
		ucc_add_request(&ucc_req, ucc_requested_val);
	else
		ucc_remove_request(&ucc_req);

	return count;
}
DEVICE_ATTR_RW(cstate_control);

static int cstate_control_level;
static ssize_t cstate_control_level_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	return snprintf(buf, 20, "%d (0=CPD, 1=C2)\n", cstate_control_level);
}

static ssize_t cstate_control_level_store(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t count)
{
	int input;

	if (!sscanf(buf, "%8d", &input))
		return -EINVAL;

	if (input < 0)
		return -EINVAL;

	cstate_control_level = input;

	return count;
}
DEVICE_ATTR_RW(cstate_control_level);

static struct attribute *exynos_ucc_attrs[] = {
	&dev_attr_ucc_requests.attr,
	&dev_attr_cstate_control.attr,
	&dev_attr_cstate_control_level.attr,
	NULL,
};

static struct attribute_group exynos_ucc_group = {
	.name = "ucc",
	.attrs = exynos_ucc_attrs,
};

enum {
	CPD_BLOCK,
	C2_BLOCK
};

static int ucc_cpupm_notifier(struct notifier_block *nb,
				unsigned long event, void *val)
{
	int cpu = smp_processor_id();

	if (!ucc_initialized)
		return NOTIFY_OK;

	if (event == C2_ENTER)
		if (cstate_control_level == C2_BLOCK)
			goto check;

	if (event == CPD_ENTER)
		if (cstate_control_level == CPD_BLOCK)
			goto check;

	return NOTIFY_OK;

check:
	if (!cpumask_test_cpu(cpu, &cur_cstate)) {
		/* not allow enter C2 */
		return NOTIFY_BAD;
	}

	return NOTIFY_OK;
}

static struct notifier_block ucc_cpupm_nb = {
	.notifier_call = ucc_cpupm_notifier,
};

static int exynos_ucc_init(struct platform_device *pdev)
{
	struct device_node *dn, *child;
	int ret, i = 0;

	dn = of_find_node_by_name(pdev->dev.of_node, "ucc");
	ucc_config_count = of_get_child_count(dn);
	if (ucc_config_count == 0) {
		pr_info("There is no ucc-config in DT\n");
		return 0;
	}

	ucc_configs = kcalloc(ucc_config_count,
			sizeof(struct ucc_config), GFP_KERNEL);
	if (!ucc_configs) {
		pr_err("Failed to alloc ucc_configs\n");
		return -ENOMEM;
	}

	for_each_child_of_node(dn, child) {
		const char *mask;

		of_property_read_u32(child, "index", &ucc_configs[i].index);

		if (of_property_read_string(child, "cstate", &mask))
			cpumask_clear(&ucc_configs[i].cstate);
		else
			cpulist_parse(mask, &ucc_configs[i].cstate);

		i++;
	}

	ret = sysfs_create_group(&pdev->dev.kobj, &exynos_ucc_group);
	if (ret) {
		pr_err("Failed to create cstate_control node\n");
		kfree(ucc_configs);
		return ret;
	}

	ret = exynos_cpupm_notifier_register(&ucc_cpupm_nb);
	if (ret)
		return ret;

	cpumask_setall(&cur_cstate);
	ucc_initialized = true;

	return 0;
}

/*********************************************************************
 *                      USER FREQUENCY CONTROL                       *
 *********************************************************************/
enum exynos_ufc_ctrl_type {
	PM_QOS_MIN_LIMIT = 0,
	PM_QOS_MIN_LIMIT_WO_BOOST,
	PM_QOS_MAX_LIMIT,
	TYPE_END,
};

static struct exynos_ufc {
	unsigned int		table_row;
	unsigned int		table_col;
	unsigned int		lit_table_row;

	int			boot_domain_auto_table;

	int			max_vfreq;

	int			last_min_wo_boost_input;
	int			last_max_input;
	int			last_max_strict_input;

	u32			**ufc_lit_table;
	struct list_head	ufc_domain_list;
	struct list_head	ufc_table_list;

	struct cpumask		active_cpus;
} ufc;

enum {
	UFC_COL_VFREQ,
	UFC_COL_BIG,
	UFC_COL_MED,
	UFC_COL_LIT,
	UFC_COL_EMSTUNE,
	UFC_COL_STRICT,
};

enum {
	UFC_STATUS_NORMAL,
	UFC_STATUS_STRICT,
};

static struct emstune_mode_request ufc_emstune_req;

/*
 * helper function
 */

static void ufc_emstune_control(int level)
{
	emstune_update_request(&ufc_emstune_req, level);
}

static char* get_ctrl_type_string(enum exynos_ufc_ctrl_type type)
{
	switch (type) {
	case PM_QOS_MIN_LIMIT:
		return "PM_QOS_MIN_LIMIT";
	case PM_QOS_MAX_LIMIT:
		return "PM_QOS_MAX_LIMIT";
	case PM_QOS_MIN_LIMIT_WO_BOOST:
		return "PM_QOS_MIN_LIMIT_WO_BOOST";
	default :
		return NULL;
	}

	return NULL;
}

static void
disable_domain_cpus(struct ufc_domain *ufc_dom)
{
	if (!ufc_dom->allow_disable_cpus)
		return;

	cpumask_andnot(&ufc.active_cpus, &ufc.active_cpus, &ufc_dom->cpus);
	ecs_request("UFC", &ufc.active_cpus);
}

static void
enable_domain_cpus(struct ufc_domain *ufc_dom)
{
	if (!ufc_dom->allow_disable_cpus)
		return;

	cpumask_or(&ufc.active_cpus, &ufc.active_cpus, &ufc_dom->cpus);
	ecs_request("UFC", &ufc.active_cpus);
}

unsigned int get_cpufreq_max_limit(void)
{
	struct ufc_domain *ufc_dom;
	struct cpufreq_policy *policy;
	unsigned int freq_qos_max;

	/* Big --> Mid --> Lit */
	list_for_each_entry(ufc_dom, &ufc.ufc_domain_list, list) {
		policy = cpufreq_cpu_get(cpumask_first(&ufc_dom->cpus));
		if (!policy)
			return -EINVAL;

		/* get value of maximum PM QoS */
		freq_qos_max = policy->max;

		if (freq_qos_max > 0) {
			freq_qos_max = min(freq_qos_max, ufc_dom->max_freq);
			freq_qos_max = max(freq_qos_max, ufc_dom->min_freq);

			return freq_qos_max;
		}
	}

	/* no maximum PM QoS */
	return 0;
}
EXPORT_SYMBOL(get_cpufreq_max_limit);

static unsigned int ufc_get_table_col(struct device_node *child)
{
	unsigned int col;

	of_property_read_u32(child, "table-col", &col);

	return col;
}

static unsigned int ufc_clamp_vfreq(unsigned int vfreq,
				struct ufc_table_info *table_info)
{
	unsigned int max_vfreq;
	unsigned int min_vfreq;

	max_vfreq = table_info->ufc_table[UFC_COL_VFREQ][0];
	min_vfreq = table_info->ufc_table[UFC_COL_VFREQ][ufc.table_row + ufc.lit_table_row - 1];

	if (max_vfreq < vfreq)
		return max_vfreq;
	if (min_vfreq > vfreq)
		return min_vfreq;

	return vfreq;
}

static int ufc_get_proper_min_table_index(unsigned int vfreq,
		struct ufc_table_info *table_info)
{
	int index;

	vfreq = ufc_clamp_vfreq(vfreq, table_info);

	for (index = 0; table_info->ufc_table[0][index] > vfreq; index++)
		;
	if (table_info->ufc_table[0][index] < vfreq)
		index--;

	return index;
}

static int ufc_get_proper_max_table_index(unsigned int vfreq,
		struct ufc_table_info *table_info)
{
	int index;

	vfreq = ufc_clamp_vfreq(vfreq, table_info);

	for (index = 0; table_info->ufc_table[0][index] > vfreq; index++)
		;

	return index;
}

static int ufc_get_proper_table_index(unsigned int vfreq,
		struct ufc_table_info *table_info, int ctrl_type)
{
	int target_idx = 0;

	switch (ctrl_type) {
	case PM_QOS_MIN_LIMIT:
	case PM_QOS_MIN_LIMIT_WO_BOOST:
		target_idx = ufc_get_proper_min_table_index(vfreq, table_info);
		break;

	case PM_QOS_MAX_LIMIT:
		target_idx = ufc_get_proper_max_table_index(vfreq, table_info);
		break;
	}
	return target_idx;
}

static unsigned int
ufc_adjust_freq(unsigned int freq, struct ufc_domain *dom, int ctrl_type)
{
	if (freq > dom->max_freq)
		return dom->max_freq;

	if (freq < dom->min_freq) {
		switch(ctrl_type) {
		case PM_QOS_MIN_LIMIT:
		case PM_QOS_MIN_LIMIT_WO_BOOST:
			return dom->min_freq;
		case PM_QOS_MAX_LIMIT:
			if (freq == 0)
				return 0;
		return dom->min_freq;
		}
	}

	return freq;
}

static struct ufc_table_info *get_table_info(int ctrl_type)
{
	struct ufc_table_info *table_info;

	list_for_each_entry(table_info, &ufc.ufc_table_list, list)
		if (table_info->ctrl_type == ctrl_type)
			return table_info;

	return NULL;
}

/*********************************************************************
 *                     MULTI REQUEST MANAGEMENT                      *
 *********************************************************************/
static void ufc_update_max_limit(void);
static void ufc_update_min_limit(void);

/**
 * PLIST_HEAD_INIT - static struct flist_head initializer
 * @head:	struct flist_head variable name
 */
#define PLIST_HEAD_INIT(head)				\
{							\
	.node_list = LIST_HEAD_INIT((head).node_list)	\
}

/**
 * flist_node_init - Dynamic struct flist_node initializer
 * @node:	&struct flist_node pointer
 * @prio:	initial node priority
 */
static inline void flist_node_init(struct flist_node *node, int prio)
{
	node->prio = prio;
	INIT_LIST_HEAD(&node->prio_list);
	INIT_LIST_HEAD(&node->node_list);
}

/**
 * pllist_head_empty - return !0 if a pllist_head is empty
 * @head:	&struct flist_head pointer
 */
static inline int flist_head_empty(const struct flist_head *head)
{
	return list_empty(&head->node_list);
}

/**
 * flist_node_empty - return !0 if flist_node is not on a list
 * @node:	&struct flist_node pointer
 */
static inline int flist_node_empty(const struct flist_node *node)
{
	return list_empty(&node->node_list);
}

/**
 * flist_last - return the last node (and thus, lowest priority)
 * @head:	the &struct flist_head pointer
 *
 * Assumes the flist is _not_ empty.
 */
static inline struct flist_node *flist_last(const struct flist_head *head)
{
	return list_entry(head->node_list.prev,
			  struct flist_node, node_list);
}

/**
 * flist_first - return the first node (and thus, highest priority)
 * @head:	the &struct flist_head pointer
 *
 * Assumes the flist is _not_ empty.
 */
static inline struct flist_node *flist_first(const struct flist_head *head)
{
	return list_entry(head->node_list.next,
			  struct flist_node, node_list);
}

/**
 * flist_add - add @node to @head
 *
 * @node:	&struct flist_node pointer
 * @head:	&struct flist_head pointer
 */
static void flist_add(struct flist_node *node, struct flist_head *head)
{
	struct flist_node *first, *iter, *prev = NULL;
	struct list_head *node_next = &head->node_list;

	WARN_ON(!flist_node_empty(node));
	WARN_ON(!list_empty(&node->prio_list));

	if (flist_head_empty(head))
		goto ins_node;

	first = iter = flist_first(head);

	do {
		if (node->prio < iter->prio) {
			node_next = &iter->node_list;
			break;
		}

		prev = iter;
		iter = list_entry(iter->prio_list.next,
				struct flist_node, prio_list);
	} while (iter != first);

	if (!prev || prev->prio != node->prio)
		list_add_tail(&node->prio_list, &iter->prio_list);
ins_node:
	list_add_tail(&node->node_list, node_next);
}

/**
 * flist_del - Remove a @node from flist.
 *
 * @node:	&struct flist_node pointer - entry to be removed
 * @head:	&struct flist_head pointer - list head
 */
static void flist_del(struct flist_node *node, struct flist_head *head)
{
	if (!list_empty(&node->prio_list)) {
		if (node->node_list.next != &head->node_list) {
			struct flist_node *next;

			next = list_entry(node->node_list.next,
					struct flist_node, node_list);

			/* add the next flist_node into prio_list */
			if (list_empty(&next->prio_list))
				list_add(&next->prio_list, &node->prio_list);
		}
		list_del_init(&node->prio_list);
	}

	list_del_init(&node->node_list);
}

/**
 * flist_for_each_entry - iterate over list of given type
 * @pos:	the type * to use as a loop counter
 * @head:	the head for your list
 * @mem:	the name of the list_head within the struct
 */
#define flist_for_each_entry(pos, head, mem)	\
	 list_for_each_entry(pos, &(head)->node_list, mem.node_list)

static struct flist_head min_limit_req_list = PLIST_HEAD_INIT(min_limit_req_list);

static int ufc_get_min_limit(void)
{
	if (flist_head_empty(&min_limit_req_list))
		return 0;

	return flist_last(&min_limit_req_list)->prio;
}

static DEFINE_SPINLOCK(ufc_lock);
static int ufc_request_active(struct ufc_req *req)
{
	int active;

	spin_lock(&ufc_lock);
	active = req->active;
	spin_unlock(&ufc_lock);

	return active;
}

enum ufc_req_type {
	REQ_ADD,
	REQ_UPDATE,
	REQ_REMOVE
};

static void
__ufc_update_request(struct ufc_req *req, enum ufc_req_type type, s32 new_freq)
{
	s32 prev_freq;

	spin_lock(&ufc_lock);

	prev_freq = ufc_get_min_limit();

	switch (type) {
	case REQ_REMOVE:
		flist_del(&req->node, &min_limit_req_list);
		req->active = 0;
		break;
	case REQ_UPDATE:
		flist_del(&req->node, &min_limit_req_list);
	case REQ_ADD:
		flist_node_init(&req->node, new_freq);
		flist_add(&req->node, &min_limit_req_list);
		req->active = 1;
		break;
	default:
		;
	}

	new_freq = ufc_get_min_limit();
	spin_unlock(&ufc_lock);

	if (prev_freq != new_freq)
		ufc_update_min_limit();
}

int __ufc_add_request(struct ufc_req *req,
				char *func, unsigned int line)
{
	if (ufc_request_active(req))
		return -EINVAL;

	req->func = func;
	req->line = line;
	__ufc_update_request(req, REQ_ADD, 0);

	return 0;
}
EXPORT_SYMBOL(__ufc_add_request);

int ufc_remove_request(struct ufc_req *req)
{
	if (!ufc_request_active(req))
		return -ENODEV;

	__ufc_update_request(req, REQ_REMOVE, 0);

	return 0;
}
EXPORT_SYMBOL(ufc_remove_request);

int ufc_update_request(struct ufc_req *req, s32 new_freq)
{
	if (!ufc_request_active(req))
		return -ENODEV;

	__ufc_update_request(req, REQ_UPDATE, new_freq);

	return 0;
}
EXPORT_SYMBOL(ufc_update_request);

/*********************************************************************
 *                          PM QOS CONTROL                           *
 *********************************************************************/
static struct ufc_req user_ufc_req;

static void ufc_update_min_limit(void)
{
	struct ufc_domain *ufc_dom;
	struct ufc_table_info *table_info;
	int target_freq, target_idx, level;
	int prev_status;

	table_info = get_table_info(PM_QOS_MIN_LIMIT);
	if (!table_info) {
		pr_err("failed to find target table\n");
		return;
	}

	target_freq = ufc_get_min_limit();

	if (target_freq < 0)
		target_freq = 0;

	target_idx = ufc_get_proper_table_index(target_freq,
				table_info, PM_QOS_MIN_LIMIT);

	prev_status = table_info->status;
	if (table_info->ufc_table[UFC_COL_STRICT][target_idx])
		table_info->status = UFC_STATUS_STRICT;
	else
		table_info->status = UFC_STATUS_NORMAL;

	/* Big --> Mid --> Lit */
	list_for_each_entry(ufc_dom, &ufc.ufc_domain_list, list) {
		unsigned int col_idx = ufc_dom->table_col_idx;

		target_freq = table_info->ufc_table[col_idx][target_idx];
		target_freq = ufc_adjust_freq(target_freq, ufc_dom, PM_QOS_MIN_LIMIT);

		freq_qos_update_request(&ufc_dom->user_min_qos_req, target_freq);
	}

	level = table_info->ufc_table[UFC_COL_EMSTUNE][target_idx];
	ufc_emstune_control(level);
	ucc_control(level);

	if (prev_status != table_info->status)
		ufc_update_max_limit();
}

static int ufc_determine_max_limit(void)
{
	struct ufc_table_info *table_info;
	int max_limit = ufc.last_max_input;

	table_info = get_table_info(PM_QOS_MIN_LIMIT);
	if (table_info && table_info->status != UFC_STATUS_STRICT)
		return max_limit;

	/* if max_limit is released, max_limit_strict is also ignored */
	if (max_limit < 0)
		return max_limit;

	if (ufc.last_max_strict_input > ufc.last_max_input)
		return ufc.last_max_strict_input;

	return max_limit;
}

static void ufc_update_max_limit(void)
{
	struct ufc_domain *ufc_dom;
	struct ufc_table_info *table_info;
	int target_freq, target_idx;

	table_info = get_table_info(PM_QOS_MAX_LIMIT);
	if (!table_info) {
		pr_err("failed to find target table\n");
		return;
	}

	target_freq = ufc_determine_max_limit();

	if (target_freq <= 0)
		target_freq = ufc.max_vfreq;

	target_idx = ufc_get_proper_table_index(target_freq,
				table_info, PM_QOS_MAX_LIMIT);

	/* Big --> Mid --> Lit */
	list_for_each_entry(ufc_dom, &ufc.ufc_domain_list, list) {
		unsigned int col_idx = ufc_dom->table_col_idx;

		target_freq = table_info->ufc_table[col_idx][target_idx];
		target_freq = ufc_adjust_freq(target_freq, ufc_dom, PM_QOS_MAX_LIMIT);

		if (target_freq == 0) {
			freq_qos_update_request(&ufc_dom->user_max_qos_req, ufc_dom->min_freq);
			disable_domain_cpus(ufc_dom);
			continue;
		}

		enable_domain_cpus(ufc_dom);
		freq_qos_update_request(&ufc_dom->user_max_qos_req, target_freq);
	}
}

static void ufc_update_min_limit_wo_boost(void)
{
	struct ufc_domain *ufc_dom;
	struct ufc_table_info *table_info;
	int target_freq, target_idx;

	table_info = get_table_info(PM_QOS_MIN_LIMIT_WO_BOOST);
	if (!table_info) {
		pr_err("failed to find target table\n");
		return;
	}

	target_freq = ufc.last_min_wo_boost_input;

	/* clear min limit */
	if (target_freq <= 0) {
		list_for_each_entry(ufc_dom, &ufc.ufc_domain_list, list)
			freq_qos_update_request(&ufc_dom->user_min_qos_wo_boost_req, 0);

		return;
	}

	target_idx = ufc_get_proper_table_index(target_freq,
				table_info, PM_QOS_MIN_LIMIT_WO_BOOST);

	/* Big --> Mid --> Lit */
	list_for_each_entry(ufc_dom, &ufc.ufc_domain_list, list) {
		unsigned int col_idx = ufc_dom->table_col_idx;

		target_freq = table_info->ufc_table[col_idx][target_idx];
		target_freq = ufc_adjust_freq(target_freq, ufc_dom,
						PM_QOS_MIN_LIMIT_WO_BOOST);

		freq_qos_update_request(&ufc_dom->user_min_qos_wo_boost_req,
						target_freq);
	}
}

/*
 * sysfs function
 */
static ssize_t cpufreq_table_show(struct device *dev,
			struct device_attribute *attr, char *buf)
{
	struct ufc_table_info *table_info;
	ssize_t count = 0;
	int r_idx;

	if (list_empty(&ufc.ufc_table_list))
		return count;

	table_info = list_first_entry(&ufc.ufc_table_list,
				struct ufc_table_info, list);

	for (r_idx = 0; r_idx < (ufc.table_row + ufc.lit_table_row); r_idx++)
		count += snprintf(&buf[count], 10, "%d ",
				table_info->ufc_table[0][r_idx]);
	count += snprintf(&buf[count], 10, "\n");

	return count;
}

static ssize_t cpufreq_max_limit_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	return snprintf(buf, 10, "%d\n", ufc.last_max_input);
}

static ssize_t cpufreq_max_limit_store(struct device *dev,
				struct device_attribute *attr,
				const char *buf, size_t count)
{
	int input;

	if (!sscanf(buf, "%8d", &input))
		return -EINVAL;

	ufc.last_max_input = input;

	ufc_update_max_limit();

	return count;
}

static ssize_t cpufreq_min_limit_show(struct device *dev,
				struct device_attribute *attr, char *buf)
{
	return snprintf(buf, 10, "%d\n", ufc_get_min_limit());
}

static ssize_t cpufreq_min_limit_store(struct device *dev,
				struct device_attribute *attr,
				const char *buf, size_t count)
{
	int input;

	if (!sscanf(buf, "%8d", &input))
		return -EINVAL;

	ufc_update_request(&user_ufc_req, input);

	return count;
}

static ssize_t cpufreq_min_limit_wo_boost_show(struct device *dev,
				struct device_attribute *attr, char *buf)
{
	return snprintf(buf, 10, "%d\n", ufc.last_min_wo_boost_input);
}

static ssize_t cpufreq_min_limit_wo_boost_store(struct device *dev,
				struct device_attribute *attr,
				const char *buf, size_t count)
{
	int input;

	if (!sscanf(buf, "%8d", &input))
		return -EINVAL;

	ufc.last_min_wo_boost_input = input;
	ufc_update_min_limit_wo_boost();

	return count;
}

static ssize_t cpufreq_max_limit_strict_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	return snprintf(buf, 10, "%d\n", ufc.last_max_strict_input);
}

static ssize_t cpufreq_max_limit_strict_store(struct device *dev,
				struct device_attribute *attr,
				const char *buf, size_t count)
{
	int input;

	if (!sscanf(buf, "%8d", &input))
		return -EINVAL;

	/* Save the input for sse change */
	ufc.last_max_strict_input = input;
	ufc_update_max_limit();

	return count;
}

static ssize_t debug_max_table_show(struct device *dev,
				struct device_attribute *attr, char *buf)
{
	struct ufc_table_info *table_info;
	int count = 0;
	int c_idx, r_idx;
	char *ctrl_str;

	if (list_empty(&ufc.ufc_table_list))
		return count;

	table_info = list_first_entry(&ufc.ufc_table_list,
				struct ufc_table_info, list);
	ctrl_str = get_ctrl_type_string(table_info->ctrl_type);

	count += snprintf(buf + count, PAGE_SIZE - count, "It's only for debug\n");
	count += snprintf(buf + count, PAGE_SIZE - count, "Table Ctrl Type: %s(%d)\n",
			ctrl_str, table_info->ctrl_type);
	count += snprintf(buf + count, PAGE_SIZE - count, "   VFreq     BIG      MID      LIT        LEVEL     UCC     STRICT\n");

	for (r_idx = 0; r_idx < (ufc.table_row + ufc.lit_table_row); r_idx++) {
		for (c_idx = 0; c_idx < ufc.table_col; c_idx++) {
			count += snprintf(buf + count, PAGE_SIZE - count, "%9d",
					table_info->ufc_table[c_idx][r_idx]);
		}
		count += snprintf(buf + count, PAGE_SIZE - count,"\n");
	}

	return count;
}

static ssize_t debug_min_table_show(struct device *dev,
				struct device_attribute *attr, char *buf)
{
	struct ufc_table_info *table_info;
	int count = 0;
	int c_idx, r_idx;
	char *ctrl_str;

	if (list_empty(&ufc.ufc_table_list))
		return count;

	table_info = list_last_entry(&ufc.ufc_table_list,
				struct ufc_table_info, list);
	ctrl_str = get_ctrl_type_string(table_info->ctrl_type);

	count += snprintf(buf + count, PAGE_SIZE - count, "It's only for debug\n");
	count += snprintf(buf + count, PAGE_SIZE - count, "Table Ctrl Type: %s(%d)\n",
			ctrl_str, table_info->ctrl_type);
	count += snprintf(buf + count, PAGE_SIZE - count, "   VFreq     BIG      MID      LIT        LEVEL     UCC     STRICT\n");

	for (r_idx = 0; r_idx < (ufc.table_row + ufc.lit_table_row); r_idx++) {
		for (c_idx = 0; c_idx < ufc.table_col; c_idx++) {
			count += snprintf(buf + count, PAGE_SIZE - count, "%9d",
					table_info->ufc_table[c_idx][r_idx]);
		}
		count += snprintf(buf + count, PAGE_SIZE - count,"\n");
	}

	return count;
}

static ssize_t debug_min_limit_req_show(struct device *dev,
				struct device_attribute *attr, char *buf)
{
	struct ufc_req *req;
	int ret = 0;
	int tot_reqs = 0;

	spin_lock(&ufc_lock);
	flist_for_each_entry(req, &min_limit_req_list, node) {
		tot_reqs++;
		ret += sprintf(buf + ret, "%d: %d (%s:%d)\n", tot_reqs,
							(req->node).prio,
							req->func,
							req->line);
	}

	ret += sprintf(buf + ret, "cur min limit: %d\n", ufc_get_min_limit());
	spin_unlock(&ufc_lock);

	return ret;
}

static DEVICE_ATTR_RO(cpufreq_table);
static DEVICE_ATTR_RW(cpufreq_min_limit);
static DEVICE_ATTR_RW(cpufreq_min_limit_wo_boost);
static DEVICE_ATTR_RW(cpufreq_max_limit);
static DEVICE_ATTR_RW(cpufreq_max_limit_strict);
static DEVICE_ATTR_RO(debug_max_table);
static DEVICE_ATTR_RO(debug_min_table);
static DEVICE_ATTR_RO(debug_min_limit_req);

static struct attribute *exynos_ufc_attrs[] = {
	&dev_attr_cpufreq_table.attr,
	&dev_attr_cpufreq_min_limit.attr,
	&dev_attr_cpufreq_min_limit_wo_boost.attr,
	&dev_attr_cpufreq_max_limit.attr,
	&dev_attr_cpufreq_max_limit_strict.attr,
	&dev_attr_debug_max_table.attr,
	&dev_attr_debug_min_table.attr,
	&dev_attr_debug_min_limit_req.attr,
	NULL,
};

static struct attribute_group exynos_ufc_group = {
	.name = "ufc",
	.attrs = exynos_ufc_attrs,
};

/*
 * init function
 */
static void ufc_free_all(void)
{
	struct ufc_table_info *table_info;
	struct ufc_domain *ufc_dom;
	int col_idx;

	pr_err("Failed: can't initialize ufc table and domain");

	while(!list_empty(&ufc.ufc_table_list)) {
		table_info = list_first_entry(&ufc.ufc_table_list,
				struct ufc_table_info, list);
		list_del(&table_info->list);

		for (col_idx = 0; table_info->ufc_table[col_idx]; col_idx++)
			kfree(table_info->ufc_table[col_idx]);

		kfree(table_info->ufc_table);
		kfree(table_info);
	}

	while(!list_empty(&ufc.ufc_domain_list)) {
		ufc_dom = list_first_entry(&ufc.ufc_domain_list,
				struct ufc_domain, list);
		list_del(&ufc_dom->list);
		kfree(ufc_dom);
	}
}

static int ufc_init_freq_qos(void)
{
	struct cpufreq_policy *policy;
	struct ufc_domain *ufc_dom;

	list_for_each_entry(ufc_dom, &ufc.ufc_domain_list, list) {
		policy = cpufreq_cpu_get(cpumask_first(&ufc_dom->cpus));
		if (!policy)
			return -EINVAL;

		freq_qos_tracer_add_request(&policy->constraints, &ufc_dom->user_min_qos_req,
				FREQ_QOS_MIN, policy->cpuinfo.min_freq);
		freq_qos_tracer_add_request(&policy->constraints, &ufc_dom->user_max_qos_req,
				FREQ_QOS_MAX, policy->cpuinfo.max_freq);
		freq_qos_tracer_add_request(&policy->constraints, &ufc_dom->user_min_qos_wo_boost_req,
				FREQ_QOS_MIN, policy->cpuinfo.min_freq);
	}

	/* Success */
	return 0;
}

static int ufc_init_sysfs(struct kobject *kobj)
{
	int ret = 0;

	ret = sysfs_create_group(kobj, &exynos_ufc_group);

	return ret;
}

static int ufc_parse_init_table(struct device_node *dn,
		struct ufc_table_info *ufc_info, struct cpufreq_policy *policy)
{
	int col_idx, row_idx, row_backup;
	int ret;
	struct cpufreq_frequency_table *pos;

	for (col_idx = 0; col_idx < ufc.table_col; col_idx++) {
		for (row_idx = 0; row_idx < ufc.table_row; row_idx++) {
			ret = of_property_read_u32_index(dn, "table",
					(col_idx + (row_idx * ufc.table_col)),
					&(ufc_info->ufc_table[col_idx][row_idx]));
			if (ret)
				return -EINVAL;
		}
	}

	if (!policy)
		return 0;

	cpufreq_for_each_entry(pos, policy->freq_table) {
		if (pos->frequency > policy->max)
			continue;
	}

	row_backup = row_idx + ufc.lit_table_row - 1;
	for (col_idx = 0; col_idx < ufc.table_col; col_idx++) {
		row_idx = row_backup;
		cpufreq_for_each_entry(pos, policy->freq_table) {
			if (pos->frequency > policy->max)
				continue;

#define SCALE_SIZE 8
			if (col_idx == UFC_COL_VFREQ)
				ufc_info->ufc_table[col_idx][row_idx--]
						= pos->frequency / SCALE_SIZE;
			else if (col_idx == UFC_COL_LIT)
				ufc_info->ufc_table[col_idx][row_idx--]
						= pos->frequency;
			else
				ufc_info->ufc_table[col_idx][row_idx--] = 0;
		}

		ufc.max_vfreq = ufc_info->ufc_table[UFC_COL_VFREQ][0];
	}

	/* Success */
	return 0;
}

static struct cpufreq_policy *bootcpu_policy;
static int init_ufc_table(struct device_node *dn)
{
	struct ufc_table_info *table_info;
	int col_idx;

	table_info = kzalloc(sizeof(struct ufc_table_info), GFP_KERNEL);
	if (!table_info)
		return -ENOMEM;

	list_add_tail(&table_info->list, &ufc.ufc_table_list);

	if (of_property_read_u32(dn, "ctrl-type", &table_info->ctrl_type))
		return -EINVAL;

	table_info->ufc_table = kzalloc(sizeof(u32 *) * ufc.table_col, GFP_KERNEL);
	if (!table_info->ufc_table)
		return -ENOMEM;

	for (col_idx = 0; col_idx < ufc.table_col; col_idx++) {
		table_info->ufc_table[col_idx] = kzalloc(sizeof(u32) *
				(ufc.table_row + ufc.lit_table_row + 1), GFP_KERNEL);

		if (!table_info->ufc_table[col_idx])
			return -ENOMEM;
	}

	ufc_parse_init_table(dn, table_info, bootcpu_policy);

	/* Success */
	return 0;
}

static int init_ufc_domain(struct device_node *dn)
{
	struct ufc_domain *ufc_dom;
	struct cpufreq_policy *policy;
	const char *buf;

	ufc_dom = kzalloc(sizeof(struct ufc_domain), GFP_KERNEL);
	if (!ufc_dom)
		return -ENOMEM;

	list_add(&ufc_dom->list, &ufc.ufc_domain_list);

	if (of_property_read_string(dn, "shared-cpus", &buf))
		return -EINVAL;

	cpulist_parse(buf, &ufc_dom->cpus);
	cpumask_and(&ufc_dom->cpus, &ufc_dom->cpus, cpu_online_mask);
	if (cpumask_empty(&ufc_dom->cpus)) {
		list_del(&ufc_dom->list);
		kfree(ufc_dom);
		return 0;
	}

	if (of_property_read_u32(dn, "table-col-idx", &ufc_dom->table_col_idx))
		return -EINVAL;

	policy = cpufreq_cpu_get(cpumask_first(&ufc_dom->cpus));
	if (!policy)
		return -EINVAL;

	ufc_dom->min_freq = policy->cpuinfo.min_freq;
	ufc_dom->max_freq = policy->cpuinfo.max_freq;

#define BOOTCPU 0
	if (cpumask_test_cpu(BOOTCPU, &ufc_dom->cpus)
				&& ufc.boot_domain_auto_table) {
		struct cpufreq_frequency_table *pos;
		int lit_row = 0;

		bootcpu_policy = policy;

		cpufreq_for_each_entry(pos, policy->freq_table) {
			if (pos->frequency > policy->max)
				continue;

			lit_row++;
		}

		ufc.lit_table_row = lit_row;
	}
	cpufreq_cpu_put(policy);

	if (of_property_read_bool(dn, "allow-disable-cpus"))
		ufc_dom->allow_disable_cpus = 1;

	/* Success */
	return 0;
}

static int init_ufc(struct device_node *dn)
{
	struct device_node *table_node;
	int size;

	INIT_LIST_HEAD(&ufc.ufc_domain_list);
	INIT_LIST_HEAD(&ufc.ufc_table_list);

	ufc.table_col = ufc_get_table_col(dn);
	if (ufc.table_col < 0)
		return -EINVAL;

	table_node = of_find_node_by_type(dn, "ufc-table");
	size = of_property_count_u32_elems(table_node, "table");
	ufc.table_row = size / ufc.table_col;

	if (of_property_read_bool(dn, "boot-domain-auto-table"))
		ufc.boot_domain_auto_table = 1;

	ufc.last_max_input = 0;
	ufc.last_min_wo_boost_input = 0;

	emstune_add_request(&ufc_emstune_req);

	return 0;
}

static int exynos_ufc_init(struct platform_device *pdev)
{
	int cpu;
	struct device_node *dn;

	dn = of_find_node_by_name(pdev->dev.of_node, "ufc");

	/* check whether cpufreq is registered or not */
	for_each_online_cpu(cpu) {
		if (!cpufreq_cpu_get(0))
			return 0;
	}

	if (init_ufc(dn)) {
		pr_err("exynos-ufc: Failed to init init_ufc\n");
		return 0;
	}

	dn = NULL;
	while((dn = of_find_node_by_type(dn, "ufc-domain"))) {
		if (init_ufc_domain(dn)) {
			pr_err("exynos-ufc: Failed to init init_ufc_domain\n");
			ufc_free_all();
			return 0;
		}
	}

	dn = NULL;
	while((dn = of_find_node_by_type(dn, "ufc-table"))) {
		if (init_ufc_table(dn)) {
			pr_err("exynos-ufc: Failed to init init_ufc_table\n");
			ufc_free_all();
			return 0;
		}
	}

	if (ufc_init_sysfs(&pdev->dev.kobj)){
		pr_err("exynos-ufc: Failed to init sysfs\n");
		ufc_free_all();
		return 0;
	}

	if (ufc_init_freq_qos()) {
		pr_err("exynos-ufc: Failed to init freq_qos\n");
		ufc_free_all();
		return 0;
	}

	cpumask_setall(&ufc.active_cpus);
	if (ecs_request_register("UFC", &ufc.active_cpus)) {
		pr_err("exynos-ufc: Failed to register ecs\n");
		ufc_free_all();
		return 0;
	}

	ufc_add_request(&user_ufc_req);

	pr_info("exynos-ufc: Complete UFC driver initialization\n");

	return 0;
}

/*********************************************************************
 *                       DRIVER INITIALIZATION                       *
 *********************************************************************/
static int exynos_ufcc_probe(struct platform_device *pdev)
{
	exynos_ucc_init(pdev);
	exynos_ufc_init(pdev);

	return 0;
}

static const struct of_device_id of_exynos_ufcc_match[] = {
	{ .compatible = "samsung,exynos-ufcc", },
	{ },
};
MODULE_DEVICE_TABLE(of, of_exynos_ufcc_match);

static struct platform_driver exynos_ufcc_driver = {
	.driver = {
		.name	= "exynos-ufcc",
		.owner = THIS_MODULE,
		.of_match_table = of_exynos_ufcc_match,
	},
	.probe		= exynos_ufcc_probe,
};

static int __init exynos_ufcc_init(void)
{
	return platform_driver_register(&exynos_ufcc_driver);
}
late_initcall(exynos_ufcc_init);

static void __exit exynos_ufcc_exit(void)
{
	platform_driver_unregister(&exynos_ufcc_driver);
}
module_exit(exynos_ufcc_exit);

MODULE_SOFTDEP("pre: exynos-acme");
MODULE_DESCRIPTION("Exynos UFCC driver");
MODULE_LICENSE("GPL");
