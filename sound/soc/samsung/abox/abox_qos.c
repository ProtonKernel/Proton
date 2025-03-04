/* sound/soc/samsung/abox/abox_qos.c
 *
 * ALSA SoC Audio Layer - Samsung Abox QoS driver
 *
 * Copyright (c) 2018 Samsung Electronics Co. Ltd.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */
#include <linux/debugfs.h>
#include <linux/pm_runtime.h>
#include <linux/sched/clock.h>

#include "abox.h"
#include "abox_proc.h"
#include "abox_qos.h"

static DEFINE_SPINLOCK(abox_qos_lock);
static struct workqueue_struct *abox_qos_wq;
static struct device *dev_abox;

static struct abox_qos abox_qos_aud = {
	.qos_class = ABOX_QOS_AUD,
	.type = ABOX_PM_QOS_MAX,
	.name = "AUD",
	.requests = LIST_HEAD_INIT(abox_qos_aud.requests),
};
static struct abox_qos abox_qos_aud_max = {
	.qos_class = ABOX_QOS_AUD_MAX,
	.type = ABOX_PM_QOS_MIN,
	.name = "AUD_MAX",
	.requests = LIST_HEAD_INIT(abox_qos_aud_max.requests),
};
static struct abox_qos abox_qos_mif = {
	.qos_class = ABOX_QOS_MIF,
	.type = ABOX_PM_QOS_MAX,
	.name = "MIF",
	.requests = LIST_HEAD_INIT(abox_qos_mif.requests),
};
static struct abox_qos abox_qos_int = {
	.qos_class = ABOX_QOS_INT,
	.type = ABOX_PM_QOS_MAX,
	.name = "INT",
	.requests = LIST_HEAD_INIT(abox_qos_int.requests),
};
static struct abox_qos abox_qos_cl0 = {
	.qos_class = ABOX_QOS_CL0,
	.type = ABOX_PM_QOS_MAX,
	.name = "CL0",
	.requests = LIST_HEAD_INIT(abox_qos_cl0.requests),
};
static struct abox_qos abox_qos_cl1 = {
	.qos_class = ABOX_QOS_CL1,
	.type = ABOX_PM_QOS_MAX,
	.name = "CL1",
	.requests = LIST_HEAD_INIT(abox_qos_cl1.requests),
};
static struct abox_qos abox_qos_cl2 = {
	.qos_class = ABOX_QOS_CL2,
	.type = ABOX_PM_QOS_MAX,
	.name = "CL2",
	.requests = LIST_HEAD_INIT(abox_qos_cl2.requests),
};

static struct abox_qos *abox_qos_array[] = {
	&abox_qos_aud,
	&abox_qos_aud_max,
	&abox_qos_mif,
	&abox_qos_int,
	&abox_qos_cl0,
	&abox_qos_cl1,
	&abox_qos_cl2,
};

static struct abox_qos *abox_qos_get_qos(enum abox_qos_class qos_class)
{
	switch (qos_class) {
	case ABOX_QOS_AUD:
		return &abox_qos_aud;
	case ABOX_QOS_AUD_MAX:
		return &abox_qos_aud_max;
	case ABOX_QOS_CL0:
		return &abox_qos_cl0;
	case ABOX_QOS_CL1:
		return &abox_qos_cl1;
	case ABOX_QOS_CL2:
		return &abox_qos_cl2;
	case ABOX_QOS_INT:
		return &abox_qos_int;
	case ABOX_QOS_MIF:
		return &abox_qos_mif;
	default:
		return NULL;
	}
}

static struct abox_qos_req *abox_qos_find_unlocked(struct abox_qos *qos,
		unsigned int id)
{
	struct abox_qos_req *req;

	lockdep_assert_held(&abox_qos_lock);

	list_for_each_entry(req, &qos->requests, list) {
		if (req->id == id)
			return req;
	}

	return NULL;
}

static struct abox_qos_req *abox_qos_find(struct abox_qos *qos, unsigned int id)
{
	struct abox_qos_req *ret;
	unsigned long flags;

	spin_lock_irqsave(&abox_qos_lock, flags);
	ret = abox_qos_find_unlocked(qos, id);
	spin_unlock_irqrestore(&abox_qos_lock, flags);

	return ret;
}

static unsigned int abox_qos_target_max(struct abox_qos *qos)
{
	struct abox_qos_req *req;
	unsigned int target = 0;
	unsigned long flags;

	spin_lock_irqsave(&abox_qos_lock, flags);
	list_for_each_entry(req, &qos->requests, list)
		target = (target < req->val) ? req->val : target;
	spin_unlock_irqrestore(&abox_qos_lock, flags);

	return target;
}

static unsigned int abox_qos_target_min(struct abox_qos *qos)
{
	struct abox_qos_req *req;
	unsigned int target = UINT_MAX;
	unsigned long flags;

	spin_lock_irqsave(&abox_qos_lock, flags);
	list_for_each_entry(req, &qos->requests, list)
		target = (target > req->val) ? req->val : target;
	spin_unlock_irqrestore(&abox_qos_lock, flags);

	return target;
}

static unsigned int abox_qos_target(struct abox_qos *qos)
{
	switch (qos->type) {
	case ABOX_PM_QOS_MAX:
		return abox_qos_target_max(qos);
	case ABOX_PM_QOS_MIN:
		return abox_qos_target_min(qos);
	}
}

void abox_qos_apply_base(enum abox_qos_class qos_class, unsigned int val)
{
	struct abox_qos *qos = abox_qos_get_qos(qos_class);

#if IS_ENABLED(CONFIG_EXYNOS_PM_QOS)
	if (!exynos_pm_qos_request_active(&qos->pm_qos_base))
		exynos_pm_qos_add_request(&qos->pm_qos_base, qos_class, val);
	exynos_pm_qos_update_request(&qos->pm_qos_base, val);
	abox_dbg(dev_abox, "applying static qos(%d, %dkHz): %dkHz\n", qos_class,
			val, exynos_pm_qos_request(qos_class));
#else
	abox_info(dev_abox, "pm_qos is disabled temporary(%d, %dkHz)\n",
			qos_class, val);
#endif
}

static void abox_qos_apply_new(struct abox_qos *qos)
{
	struct abox_qos_req *req, *n, *new_req;
	unsigned long flags;

	spin_lock_irqsave(&abox_qos_lock, flags);
	list_for_each_entry_safe(new_req, n, &qos->requests, list) {
		if (new_req->registered)
			continue;

		spin_unlock_irqrestore(&abox_qos_lock, flags);
		req = devm_kmalloc(dev_abox, sizeof(*new_req), GFP_KERNEL);
		spin_lock_irqsave(&abox_qos_lock, flags);
		if (!req)
			continue;

		*req = *new_req;
		req->registered = true;
		list_replace(&new_req->list, &req->list);
		kfree(new_req);
	}
	spin_unlock_irqrestore(&abox_qos_lock, flags);
}

static void abox_qos_apply(struct abox_qos *qos)
{
	enum abox_qos_class qos_class = qos->qos_class;
	int val;

	abox_qos_apply_new(qos);
	val = abox_qos_target(qos);
	if (qos->val != val) {
		if (qos->qos_class == ABOX_QOS_AUD) {
			if (!qos->val && val) {
				pm_runtime_get(dev_abox);
			} else if (qos->val && !val) {
				pm_runtime_mark_last_busy(dev_abox);
				pm_runtime_put_autosuspend(dev_abox);
			}
		}
		qos->val = val;
#if IS_ENABLED(CONFIG_EXYNOS_PM_QOS)
		if (!exynos_pm_qos_request_active(&qos->pm_qos))
			exynos_pm_qos_add_request(&qos->pm_qos, qos_class, val);
		exynos_pm_qos_update_request(&qos->pm_qos, val);
		abox_dbg(dev_abox, "applying qos(%d, %dkHz): %dkHz\n", qos_class,
				val, exynos_pm_qos_request(qos_class));
#else
		abox_info(dev_abox, "pm_qos is disabled temporary(%d, %dkHz)\n",
				qos_class, val);
#endif
	}
}

static void abox_qos_work_func(struct work_struct *work)
{
	struct abox_qos **p_qos;
	size_t len = ARRAY_SIZE(abox_qos_array);

	for (p_qos = abox_qos_array; p_qos - abox_qos_array < len; p_qos++)
		abox_qos_apply(*p_qos);
}

static DECLARE_WORK(abox_qos_work, abox_qos_work_func);

static struct abox_qos_req *abox_qos_new(struct device *dev,
		struct abox_qos *qos, unsigned int id, unsigned int val,
		const char *name)
{
	struct abox_qos_req *req;

	lockdep_assert_held(&abox_qos_lock);

	req = kzalloc(sizeof(*req), GFP_ATOMIC);
	if (!req)
		return req;

	strlcpy(req->name, name, sizeof(req->name));
	req->val = val;
	req->updated = local_clock();
	req->id = id;
	list_add_tail(&req->list, &qos->requests);

	return req;
}

int abox_qos_get_request(struct device *dev, enum abox_qos_class qos_class)
{
	struct abox_qos *qos = abox_qos_get_qos(qos_class);
	int ret;

	if (!qos) {
		abox_err(dev, "%s: invalid class: %d\n", __func__, qos_class);
		return -EINVAL;
	}

	ret = exynos_pm_qos_request(qos_class);
	abox_dbg(dev, "%s(%d): %d\n", __func__, qos_class, ret);

	return ret;
}

int abox_qos_get_target(struct device *dev, enum abox_qos_class qos_class)
{
	struct abox_qos *qos = abox_qos_get_qos(qos_class);
	int ret;

	if (!qos) {
		abox_err(dev, "%s: invalid class: %d\n", __func__, qos_class);
		return -EINVAL;
	}

	ret = abox_qos_target(qos);
	abox_dbg(dev, "%s(%d): %d\n", __func__, qos_class, ret);

	return ret;
}

unsigned int abox_qos_get_value(struct device *dev,
		enum abox_qos_class qos_class, unsigned int id)
{
	struct abox_qos *qos = abox_qos_get_qos(qos_class);
	struct abox_qos_req *req;
	int ret;

	if (!qos) {
		abox_err(dev, "%s: invalid class: %d\n", __func__, qos_class);
		return -EINVAL;
	}

	req = abox_qos_find(qos, id);
	ret = req ? req->val : 0;

	abox_dbg(dev, "%s(%d, %#x): %d\n", __func__, qos_class, id, ret);

	return ret;
}

int abox_qos_is_active(struct device *dev, enum abox_qos_class qos_class,
		unsigned int id)
{
	struct abox_qos *qos = abox_qos_get_qos(qos_class);
	struct abox_qos_req *req;
	int ret;

	if (!qos) {
		abox_err(dev, "%s: invalid class: %d\n", __func__, qos_class);
		return -EINVAL;
	}

	req = abox_qos_find(qos, id);
	ret = (req && req->val);

	abox_dbg(dev, "%s(%d, %#x): %d\n", __func__, qos_class, id, ret);

	return ret;
}

void abox_qos_complete(void)
{
	flush_work(&abox_qos_work);
}

int abox_qos_request(struct device *dev, enum abox_qos_class qos_class,
		unsigned int id, unsigned int val, const char *name)
{
	static const unsigned int DEFAULT_ID = 0xAB0CDEFA;
	struct abox_qos *qos = abox_qos_get_qos(qos_class);
	struct abox_qos_req *req;
	unsigned long flags;
	int ret = 0;

	if (!qos) {
		abox_err(dev, "%s: invalid class: %d\n", __func__, qos_class);
		return -EINVAL;
	}

	if (id == 0)
		id = DEFAULT_ID;

	spin_lock_irqsave(&abox_qos_lock, flags);
	req = abox_qos_find_unlocked(qos, id);
	if (!req) {
		req = abox_qos_new(dev, qos, id, val, name);
		if (!req) {
			abox_err(dev, "qos request failed: %d, %#x, %u, %s\n",
					qos_class, id, val, name);
			ret = -ENOMEM;
		}
	} else if (req->val != val) {
		strlcpy(req->name, name, sizeof(req->name));
		req->val = val;
		req->updated = local_clock();
	} else {
		req = NULL;
	}
	spin_unlock_irqrestore(&abox_qos_lock, flags);

	if (req)
		queue_work(abox_qos_wq, &abox_qos_work);

	return ret;
}

void abox_qos_clear(struct device *dev, enum abox_qos_class qos_class)
{
	struct abox_qos *qos = abox_qos_get_qos(qos_class);
	struct abox_qos_req *req;
	unsigned long flags;
	bool dirty = false;

	if (!qos) {
		abox_err(dev, "%s: invalid class: %d\n", __func__, qos_class);
		return;
	}

	spin_lock_irqsave(&abox_qos_lock, flags);
	list_for_each_entry(req, &qos->requests, list) {
		if (req->val) {
			req->val = 0;
			strlcpy(req->name, "clear", sizeof(req->name));
			req->updated = local_clock();
			abox_info(dev, "clearing qos: %d, %#x\n",
					qos_class, req->id);
			dirty = true;
		}
	}
	spin_unlock_irqrestore(&abox_qos_lock, flags);

	if (dirty)
		queue_work(abox_qos_wq, &abox_qos_work);
}

void abox_qos_print(struct device *dev, enum abox_qos_class qos_class)
{
	struct abox_qos *qos = abox_qos_get_qos(qos_class);
	struct abox_qos_req *req;
	unsigned long flags;

	if (!qos) {
		abox_err(dev, "%s: invalid class: %d\n", __func__, qos_class);
		return;
	}

	spin_lock_irqsave(&abox_qos_lock, flags);
	list_for_each_entry(req, &qos->requests, list) {
		if (req->val)
			abox_warn(dev, "qos: %d, %#x\n", qos_class, req->id);
	}
	spin_unlock_irqrestore(&abox_qos_lock, flags);
}

int abox_qos_add_notifier(enum abox_qos_class qos_class,
		struct notifier_block *notifier)
{
	int ret = 0;

#if IS_ENABLED(CONFIG_EXYNOS_PM_QOS)
	ret = exynos_pm_qos_add_notifier(qos_class, notifier);
#endif
	return ret;
}

static ssize_t abox_qos_read_qos(char *buf, size_t size, struct abox_qos *qos)
{
	struct abox_qos_req *req;
	ssize_t offset = 0;
	unsigned long flags;

#if IS_ENABLED(CONFIG_EXYNOS_PM_QOS)
	offset += snprintf(buf + offset, size - offset,
			"name=%s, requested=%u, value=%u\n",
			qos->name, qos->val,
			exynos_pm_qos_request(qos->qos_class));
#else
	offset += snprintf(buf + offset, size - offset,
			"name=%s, requested=%u, value=%u\n",
			qos->name, qos->val, pm_qos_request(qos->qos_class));
#endif
	spin_lock_irqsave(&abox_qos_lock, flags);
	list_for_each_entry(req, &qos->requests, list) {
		unsigned long long time = req->updated;
		unsigned long rem = do_div(time, NSEC_PER_SEC);

		offset += snprintf(buf + offset, size - offset,
				"%#X\t%u\t%s updated at %llu.%09lus\n",
				req->id, req->val, req->name, time, rem);
	}
	spin_unlock_irqrestore(&abox_qos_lock, flags);
	offset += snprintf(buf + offset, size - offset, "\n");

	return offset;
}

static ssize_t abox_qos_read_file(struct file *file, char __user *user_buf,
				    size_t count, loff_t *ppos)
{
	struct abox_qos **p_qos;
	const size_t size = PAGE_SIZE, len = ARRAY_SIZE(abox_qos_array);
	char *buf = kmalloc(size, GFP_KERNEL);
	ssize_t ret = 0;

	if (!buf)
		return -ENOMEM;

	for (p_qos = abox_qos_array; p_qos - abox_qos_array < len; p_qos++)
		ret += abox_qos_read_qos(buf + ret, size - ret, *p_qos);

	if (ret >= 0)
		ret = simple_read_from_buffer(user_buf, count, ppos, buf, ret);

	kfree(buf);

	return ret;
}

static const struct file_operations abox_qos_fops = {
	.open = simple_open,
	.read = abox_qos_read_file,
	.llseek = default_llseek,
};

void abox_qos_init(struct device *adev)
{
	dev_abox = adev;
	abox_qos_wq = alloc_ordered_workqueue("abox_qos",
			WQ_FREEZABLE | WQ_MEM_RECLAIM);
	abox_proc_create_file("qos", 0660, NULL, &abox_qos_fops, NULL, 0);
}
