/* SPDX-License-Identifier: GPL-2.0-or-later */
/*
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * Copyright (c) 2020 Samsung Electronics Co., Ltd
 *
 * KQ(Kernel Quality) Balancer driver implementation
 *	: Jaecheol Kim <jc22.kim@samsung.com>
 */

#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/init.h>
#include <linux/of.h>
#include <linux/string.h>
#include <linux/cpufreq.h>
#include <linux/platform_device.h>
#include <linux/kq/kq_balancer.h>
#include <linux/sec_class.h>

/*
 * list head of kq balancer
 */
static LIST_HEAD(balancer_domains);

static inline bool kq_balancer_is_domain_cpu(struct balancer_domain *balancer)
{
	if (balancer->type == KQ_BALANCER_DOMAIN_TYPE_CPU)
		return true;
	return false;
}

static inline bool kq_balancer_is_domain_dev(struct balancer_domain *balancer)
{
	if (balancer->type == KQ_BALANCER_DOMAIN_TYPE_DEV)
		return true;
	return false;
}

static inline bool kq_balancer_is_qos_must_stop(struct balancer_domain *balancer)
{
	if (balancer->run == KQ_BALANCER_OPER_STOP)
		return true;
	return false;
}

static inline bool kq_balancer_is_info_running(struct balancer_info *binfo)
{
	if (binfo->run == KQ_BALANCER_OPER_RUN)
		return true;
	return false;
}

static inline bool kq_balancer_is_qos_locked(struct balancer_domain *balancer)
{
	if (balancer->qos_status == KQ_BALANCER_QOS_LOCK)
		return true;
	return false;
}

static inline bool kq_balancer_is_qos_unlocked(struct balancer_domain *balancer)
{
	if (balancer->qos_status == KQ_BALANCER_QOS_UNLOCK)
		return true;
	return false;
}

static inline bool kq_balancer_is_qos_policy_max(int policy)
{
	if (policy == KQ_BALANCER_QOS_POLICY_TYPE_LIT_MAX ||
		policy == KQ_BALANCER_QOS_POLICY_TYPE_MID_MAX ||
		policy == KQ_BALANCER_QOS_POLICY_TYPE_BIG_MAX)
		return true;
	return false;
}

static inline void kq_balancer_cpu_pm_qos_update(struct balancer_domain *balancer,
	unsigned int type, unsigned int freq)
{
	struct cpufreq_policy *policy;

	if (kq_balancer_is_qos_unlocked(balancer)) {
		policy = cpufreq_cpu_get(balancer->num);

		if (kq_balancer_is_qos_policy_max(balancer->qos_policy))
			freq_qos_add_request(&policy->constraints, &balancer->cpu_qos, FREQ_QOS_MAX, freq);
		else
			freq_qos_add_request(&policy->constraints, &balancer->cpu_qos, FREQ_QOS_MIN, freq);
	} else {
		freq_qos_update_request(&balancer->cpu_qos, freq);
	}
}

static inline void kq_balancer_cpu_pm_qos_remove(struct balancer_domain *balancer)
{
	if (kq_balancer_is_qos_locked(balancer))
		freq_qos_remove_request(&balancer->cpu_qos);
}

#if IS_ENABLED(CONFIG_EXYNOS_PM_QOS)
static inline void kq_balancer_dev_pm_qos_update(struct exynos_pm_qos_request *req,
	unsigned int type, unsigned int freq)
{
	if (exynos_pm_qos_request_active(req))
		exynos_pm_qos_update_request(req, freq);
	else
		exynos_pm_qos_add_request(req, type, freq);
}

static inline void kq_balancer_dev_pm_qos_remove(struct exynos_pm_qos_request *req)
{
	if (exynos_pm_qos_request_active(req))
		exynos_pm_qos_remove_request(req);
}
#else
#define kq_balancer_dev_pm_qos_update(req, type, freq) do { } while (0)
#define kq_balancer_dev_pm_qos_remove(req) do { } while (0)
#endif

static inline void kq_balancer_tsk_interruptible_and_wakeup(int task_control_type,
	int task_wakeup_delay)
{
	set_current_state(TASK_INTERRUPTIBLE);

	if (task_control_type == KQ_BALANCER_TASK_DELAYED_WAKEUP) {
		schedule_timeout_interruptible(msecs_to_jiffies(task_wakeup_delay));
		set_current_state(TASK_RUNNING);
	} else if (task_control_type == KQ_BALANCER_TASK_NO_WAKEUP) {
		schedule();
	}
}

static char *kq_balancer_get_domain_qos_status(int status)
{
	switch (status) {
	case KQ_BALANCER_QOS_LOCK:
		return "lock";
	case KQ_BALANCER_QOS_UNLOCK:
		return "unlock";
	}
	return "none";
}

static char *kq_balancer_get_domain_qos_type(int type)
{
	switch (type) {
	case KQ_BALANCER_TYPE_LIT:
		return "cpu-lit";
	case KQ_BALANCER_TYPE_MID:
		return "cpu-mid";
	case KQ_BALANCER_TYPE_BIG:
		return "cpu-big";
	case KQ_BALANCER_TYPE_MIF:
		return "dev-mif";
	case KQ_BALANCER_TYPE_UNSUPPORTED:
		return "unsupported";
	default:
		break;
	}
	return "none";
}

static char *kq_balancer_get_domain_qos_policy(int policy)
{
	switch (policy) {
	case KQ_BALANCER_QOS_POLICY_TYPE_LIT_MIN:
		return "lit-min";
	case KQ_BALANCER_QOS_POLICY_TYPE_LIT_MAX:
		return "lit-max";
	case KQ_BALANCER_QOS_POLICY_TYPE_MID_MIN:
		return "mid-min";
	case KQ_BALANCER_QOS_POLICY_TYPE_MID_MAX:
		return "mid-max";
	case KQ_BALANCER_QOS_POLICY_TYPE_BIG_MIN:
		return "big-min";
	case KQ_BALANCER_QOS_POLICY_TYPE_BIG_MAX:
		return "big-max";
	case KQ_BALANCER_QOS_POLICY_TYPE_MIF_MIN:
		return "mif-min";
	case KQ_BALANCER_QOS_POLICY_TYPE_MIF_MAX:
		return "mif-max";
	default:
		break;
	}
	return "none";
}

static void kq_balancer_domains_info(void)
{
	struct balancer_domain *balancer;
	int i;

	list_for_each_entry(balancer, &balancer_domains, list) {
		pr_info("%s name : %s\n", __func__, balancer->name);
		pr_info("%s type : %d\n", __func__, balancer->type);
		pr_info("%s num : %d\n", __func__, balancer->num);
		pr_info("%s max : %d\n", __func__, balancer->max_freq);
		pr_info("%s min : %d\n", __func__, balancer->min_freq);
		pr_info("%s qos delay : %d\n", __func__, balancer->qos_delay);
		pr_info("%s qos status : %s\n", __func__,
			kq_balancer_get_domain_qos_status(balancer->qos_status));
		pr_info("%s qos type : %s\n", __func__,
			kq_balancer_get_domain_qos_type(balancer->qos_type));
		pr_info("%s qos policy : %s\n", __func__,
			kq_balancer_get_domain_qos_policy(balancer->qos_policy));
		pr_info("%s run : %d\n", __func__, balancer->run);
		pr_info("%s size : %d\n", __func__, balancer->table_size);
		for (i = 0; i < balancer->table_size; i++)
			pr_info("%s [%d]idx [%d]khz\n", __func__, i, balancer->freq_table[i].frequency);
	}
}

static void kq_balancer_stop_balancer_thread(struct balancer_info *binfo)
{
	struct balancer_domain *balancer;

	binfo->run = KQ_BALANCER_OPER_STOP;

	list_for_each_entry(balancer, &balancer_domains, list) {
		balancer->run = KQ_BALANCER_OPER_STOP;
		pr_info("%s stop [%s] balancer\n", __func__, balancer->name);
	}
}

static inline void kq_balancer_check_stop_balancer(struct balancer_domain *balancer)
{
	if (kq_balancer_is_qos_must_stop(balancer)) {
		pr_info("%s %s stop [%d:%s]\n", __func__, balancer->name,
			balancer->qos_policy, kq_balancer_get_domain_qos_policy(balancer->qos_policy));

		if (kq_balancer_is_qos_locked(balancer)) {
			if (kq_balancer_is_domain_cpu(balancer))
				kq_balancer_cpu_pm_qos_remove(balancer);
			else
				kq_balancer_dev_pm_qos_remove(&balancer->dev_qos);
		}

		balancer->qos_status = KQ_BALANCER_QOS_UNLOCK;
		kq_balancer_tsk_interruptible_and_wakeup(KQ_BALANCER_TASK_NO_WAKEUP, balancer->qos_delay);
	}
}

static inline void kq_balancer_change_qos_freq(struct balancer_domain *balancer,
	unsigned int freq_idx)
{
	if (kq_balancer_is_domain_cpu(balancer)) {
		kq_balancer_cpu_pm_qos_update(balancer,
			balancer->qos_policy,
			balancer->freq_table[freq_idx].frequency);
	} else {
		kq_balancer_dev_pm_qos_update(&balancer->dev_qos,
			balancer->qos_policy,
			balancer->freq_table[freq_idx].frequency);
	}
	balancer->qos_status = KQ_BALANCER_QOS_LOCK;
}

static inline int kq_balancer_check_next_qos_index(struct balancer_domain *balancer)
{
	if (balancer->qos_manual == KQ_BALANCER_QOS_CONTROL_RANDOM)
		return prandom_u32() % balancer->table_size;
	else
		return balancer->qos_manual_index;
}

static int kq_balancer_start_balancer_thread(void *data)
{
	struct balancer_domain *balancer = data;
	int freq_idx;

	pr_info("%s run [%s] balancer\n", __func__, balancer->name);

	while (!kthread_should_stop()) {
		kq_balancer_check_stop_balancer(balancer);
		freq_idx = kq_balancer_check_next_qos_index(balancer);
		kq_balancer_change_qos_freq(balancer, freq_idx);
		kq_balancer_tsk_interruptible_and_wakeup(KQ_BALANCER_TASK_DELAYED_WAKEUP, balancer->qos_delay);
	}
	return 0;
}

static void kq_balancer_stop_balancer(struct balancer_domain *balancer)
{
	pr_info("%s stop [%s] balancer policy [%s]\n", __func__, balancer->name,
		kq_balancer_get_domain_qos_policy(balancer->qos_policy));
	balancer->run = KQ_BALANCER_OPER_STOP;
}

static int kq_balancer_run_balancer(
	struct balancer_domain *balancer, char *policy)
{
	pr_info("%s start [%s] balancer policy [%s]\n", __func__, balancer->name, policy);

	if (!balancer->kthread) {
		balancer->run = KQ_BALANCER_OPER_RUN;
		balancer->kthread = kthread_run(kq_balancer_start_balancer_thread, balancer,
			"kq-balancer-%s", balancer->name);

		if (IS_ERR_OR_NULL(balancer->kthread)) {
			balancer->run = KQ_BALANCER_OPER_STOP;
			return -EINVAL;
		}
	} else {
		pr_info("%s re-run [%s] balancer [%s]\n", __func__, balancer->name,
			kq_balancer_get_domain_qos_policy(balancer->qos_policy));
		balancer->run = KQ_BALANCER_OPER_RUN;
		wake_up_process(balancer->kthread);
	}
	return 0;
}

static void kq_balancer_set_balancer_policy(
	struct balancer_domain *balancer, int type,
	int policy_max, int policy_min)
{
	if (type == KQ_BALANCER_QOS_POLICY_MAX)
		balancer->qos_policy = policy_max;
	else if (type == KQ_BALANCER_QOS_POLICY_MIN)
		balancer->qos_policy = policy_min;

	pr_info("%s %s set policy %s\n", __func__, balancer->name,
		kq_balancer_get_domain_qos_policy(balancer->qos_policy));
}

static int kq_balancer_update_balancer_policy(
	struct balancer_domain *balancer, char *policy)
{
	int type;

	if (!strncmp(policy, "max", 3)) {
		pr_info("%s %s set type max\n", __func__, balancer->name);
		type = KQ_BALANCER_QOS_POLICY_MAX;
	} else if (!strncmp(policy, "min", 3)) {
		pr_info("%s %s set type min\n", __func__, balancer->name);
		type = KQ_BALANCER_QOS_POLICY_MIN;
	} else {
		pr_info("%s %s set type [%s] invalid\n", __func__, balancer->name, policy);
		return -EINVAL;
	}

	switch (balancer->qos_type) {
	case KQ_BALANCER_TYPE_LIT:
		kq_balancer_set_balancer_policy(balancer, type,
			KQ_BALANCER_QOS_POLICY_TYPE_LIT_MAX,
			KQ_BALANCER_QOS_POLICY_TYPE_LIT_MIN);
		break;
	case KQ_BALANCER_TYPE_MID:
		kq_balancer_set_balancer_policy(balancer, type,
			KQ_BALANCER_QOS_POLICY_TYPE_MID_MAX,
			KQ_BALANCER_QOS_POLICY_TYPE_MID_MIN);
		break;
	case KQ_BALANCER_TYPE_BIG:
		kq_balancer_set_balancer_policy(balancer, type,
			KQ_BALANCER_QOS_POLICY_TYPE_BIG_MAX,
			KQ_BALANCER_QOS_POLICY_TYPE_BIG_MIN);
		break;
	case KQ_BALANCER_TYPE_MIF:
		kq_balancer_set_balancer_policy(balancer, type,
			KQ_BALANCER_QOS_POLICY_TYPE_MIF_MAX,
			KQ_BALANCER_QOS_POLICY_TYPE_MIF_MIN);
		break;
	default:
		pr_err("%s policy not support!", __func__);
		return -EINVAL;
	}

	return 0;
}

static struct device_attribute kq_balancer_attrs[] = {
	KQ_BALANCER_ATTR(balancer),
	KQ_BALANCER_ATTR(status),
	KQ_BALANCER_ATTR(print_log),
	KQ_BALANCER_ATTR(timeout),
	KQ_BALANCER_ATTR(tables),
	KQ_BALANCER_ATTR(manual),
};

static ssize_t kq_balancer_show_balancer_attr(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	struct balancer_info *binfo = dev_get_drvdata(dev);

	pr_info("%s stop balancer\n", __func__);
	kq_balancer_stop_balancer_thread(binfo);

	return snprintf(buf, sizeof(buf), "%s", "OK\n");
}

static ssize_t kq_balancer_show_status_attr(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	return 0;
}

static ssize_t kq_balancer_show_printlog_attr(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	return 0;
}

static ssize_t kq_balancer_show_timeout_attr(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	struct balancer_info *binfo = dev_get_drvdata(dev);

	pr_info("%s timeout : %d\n", __func__, binfo->timeout);

	return sprintf(buf, "%d\n", binfo->timeout);
}

static ssize_t kq_balancer_show_tables_attr(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	struct balancer_domain *balancer;
	int len = 0;
	int i;

	/* WARNING : sum of len must not exceed PAGE_SIZE */
	list_for_each_entry(balancer, &balancer_domains, list) {
		len += scnprintf(buf + len, PAGE_SIZE - len, "name: %s\n", balancer->name);
		len += scnprintf(buf + len, PAGE_SIZE - len, "type: %d\n", balancer->type);
		len += scnprintf(buf + len, PAGE_SIZE - len, "num: %d\n", balancer->num);
		len += scnprintf(buf + len, PAGE_SIZE - len, "max: %d\n", balancer->max_freq);
		len += scnprintf(buf + len, PAGE_SIZE - len, "min: %d\n", balancer->min_freq);
		len += scnprintf(buf + len, PAGE_SIZE - len, "qos delay: %d\n", balancer->qos_delay);
		len += scnprintf(buf + len, PAGE_SIZE - len, "qos status: %s\n",
			kq_balancer_get_domain_qos_status(balancer->qos_status));
		len += scnprintf(buf + len, PAGE_SIZE - len, "qos type: %s\n",
			kq_balancer_get_domain_qos_type(balancer->qos_type));
		len += scnprintf(buf + len, PAGE_SIZE - len, "qos policy: %s\n",
			kq_balancer_get_domain_qos_policy(balancer->qos_policy));
		len += scnprintf(buf + len, PAGE_SIZE - len, "run: %d\n", balancer->run);
		len += scnprintf(buf + len, PAGE_SIZE - len, "size: %d\n", balancer->table_size);

		for (i = 0; i < balancer->table_size; i++)
			len += scnprintf(buf + len, PAGE_SIZE - len,
				"[%d]khz\n", balancer->freq_table[i].frequency);

		if (len >= PAGE_SIZE) {
			pr_info("%s error PAGE_SIZE\n", __func__);
			break;
		}
	}

	return len;
}

static ssize_t kq_balancer_show_manual_attr(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	return 0;
}

static ssize_t kq_balancer_show_attrs(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	const ptrdiff_t offset = attr - kq_balancer_attrs;
	int i = 0;

	pr_info("%s tsk [%s]\n", __func__, current->comm);

	switch (offset) {
	case KQ_BALANCER_SYSFS_BALANCER:
		i = kq_balancer_show_balancer_attr(dev, attr, buf);
		break;
	case KQ_BALANCER_SYSFS_STATUS:
		i = kq_balancer_show_status_attr(dev, attr, buf);
		break;
	case KQ_BALANCER_SYSFS_PRINTLOG:
		i = kq_balancer_show_printlog_attr(dev, attr, buf);
		break;
	case KQ_BALANCER_SYSFS_TIMEOUT:
		i = kq_balancer_show_timeout_attr(dev, attr, buf);
		break;
	case KQ_BALANCER_SYSFS_TABLES:
		i = kq_balancer_show_tables_attr(dev, attr, buf);
		break;
	case KQ_BALANCER_SYSFS_MANUAL:
		i = kq_balancer_show_manual_attr(dev, attr, buf);
		break;
	default:
		pr_err("%s show kq balancer out of range\n", __func__);
		i = -EINVAL;
		break;
	}
	return i;
}

static int kq_balancer_check_invalid_cmd(char *ptr)
{
	if (ptr == NULL) {
		pr_err("%s ptr is null!", __func__);
		return -EINVAL;
	}

	if (strlen(ptr) >= KQ_BALANCER_MAX_CMD_LEN) {
		pr_err("%s %s is too large %d\n", __func__,
			ptr, KQ_BALANCER_MAX_CMD_LEN);
		return -EINVAL;
	}
	return 0;
}

static ssize_t kq_balancer_store_balancer_attr(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t count)
{
	struct balancer_info *binfo = dev_get_drvdata(dev);
	struct balancer_domain *balancer;
	char tmp_buf[KQ_BALANCER_MAX_BUFFER];
	char cmd_buf[KQ_BALANCER_MAX_DELIMITER_LEN][KQ_BALANCER_MAX_CMD_LEN];
	char *tmp_buf_ptr;
	char *ptr_cmd = NULL;
	int i = 0;
	int ret = 0;
	int balancer_start_idx;
	int max_buf_len = KQ_BALANCER_MAX_BUFFER;

	pr_info("%s ori : %s", __func__, buf);

	if (count > max_buf_len) {
		pr_err("%s checkout buffer size (buffer size < %d)\n",
			__func__, max_buf_len);
		return -EINVAL;
	}

	strncpy(tmp_buf, buf, count);
	tmp_buf[count] = '\0';
	tmp_buf_ptr = tmp_buf;

	pr_info("%s copied : %s\n", __func__, tmp_buf);

	while (i < KQ_BALANCER_MAX_DELIMITER_LEN) {
		ptr_cmd = strsep(&tmp_buf_ptr, ",");

		if (kq_balancer_check_invalid_cmd(ptr_cmd)) {
			pr_err("%s parsing error", __func__);
			return -EINVAL;
		}
		snprintf(cmd_buf[i++], KQ_BALANCER_MAX_CMD_LEN, "%s", ptr_cmd);
	}

	ret = sscanf(cmd_buf[KQ_BALANCER_CMD_TIMEOUT], "%d\n", &binfo->timeout);
	if (ret != 1) {
		pr_err("%s invalid timeout\n", __func__);
		return -EINVAL;
	}

	if (!strncmp(cmd_buf[KQ_BALANCER_CMD_OPER], "start", 5)) {
		if (kq_balancer_is_info_running(binfo)) {
			pr_info("%s already running", __func__);
			return count;
		}
		binfo->run = KQ_BALANCER_OPER_RUN;

		balancer_start_idx = KQ_BALANCER_CMD_LIT;
		list_for_each_entry(balancer, &balancer_domains, list) {
			ret = kq_balancer_update_balancer_policy(balancer,
				cmd_buf[balancer_start_idx]);
			if (ret) {
				pr_err("%s error checking policy\n", __func__);
				return -EINVAL;
			}

			ret = kq_balancer_run_balancer(balancer,
				cmd_buf[balancer_start_idx]);
			if (ret) {
				pr_err("%s error running balancer thread\n", __func__);
				return -EINVAL;
			}
			balancer_start_idx++;
		}
		schedule_delayed_work(&binfo->dwork, HZ * binfo->timeout);
	}
	return count;
}

static ssize_t kq_balancer_store_status_attr(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t count)
{
	char *envp[2] = {"NAD_BALANCER_UEVENT", NULL};

	kobject_uevent_env(&dev->kobj, KOBJ_CHANGE, envp);
	return count;
}

static ssize_t kq_balancer_store_printlog_attr(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t count)
{
	pr_info("%s %s\n", __func__, buf);
	return count;
}

static ssize_t kq_balancer_store_timeout_attr(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t count)
{
	struct balancer_info *binfo = dev_get_drvdata(dev);
	int ret;
	int timeout;

	pr_info("%s timeout prev = %d\n", __func__, binfo->timeout);

	ret = kstrtoint(buf, 10, &timeout);
	if (ret) {
		pr_err("%s invalid input %s ret %d\n", __func__, buf, ret);
		return 0;
	}
	binfo->timeout = timeout;

	pr_info("%s timeout after = %d\n", __func__, binfo->timeout);

	return count;
}

static ssize_t kq_balancer_store_tables_attr(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t count)
{
	return count;
}

static ssize_t kq_balancer_store_manual_attr(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t count)
{
	struct balancer_info *binfo = dev_get_drvdata(dev);
	struct balancer_domain *balancer;
	char tmp_buf[KQ_BALANCER_MAX_MANUAL_BUFFER];
	char cmd_buf[KQ_BALANCER_MAX_MANUAL_DELIMITER_LEN][KQ_BALANCER_MAX_CMD_LEN];
	char *tmp_buf_ptr;
	char *ptr_cmd = NULL;
	int i = 0;
	int ret = 0;
	int freq_idx = 0;
	int max_buf_len = KQ_BALANCER_MAX_MANUAL_BUFFER;

	if (count > max_buf_len) {
		pr_err("%s checkout buffer size (buffer size < %d)\n",
			__func__, max_buf_len);
		return -EINVAL;
	}
	strncpy(tmp_buf, buf, count);
	tmp_buf[count] = '\0';
	tmp_buf_ptr = tmp_buf;

	while (i < KQ_BALANCER_MAX_MANUAL_DELIMITER_LEN) {
		ptr_cmd = strsep(&tmp_buf_ptr, ",");

		if (kq_balancer_check_invalid_cmd(ptr_cmd)) {
			pr_err("%s parsing error", __func__);
			return -EINVAL;
		}
		snprintf(cmd_buf[i++], KQ_BALANCER_MAX_CMD_LEN, "%s", ptr_cmd);
	}

	ret = sscanf(cmd_buf[KQ_BALANCER_CMD_MANUAL_FREQ_IDX], "%d\n", &freq_idx);
	if (ret != 1) {
		pr_err("%s invalid index value\n", __func__);
		return -EINVAL;
	}

	list_for_each_entry(balancer, &balancer_domains, list) {
		if (strcmp(cmd_buf[KQ_BALANCER_CMD_MANUAL_DOMAIN], balancer->name) == 0) {
			if (freq_idx >= 0 && balancer->table_size < freq_idx) {
				if (kq_balancer_is_info_running(binfo)) {
					binfo->run = KQ_BALANCER_OPER_STOP;
					balancer->qos_manual = KQ_BALANCER_QOS_CONTROL_RANDOM;
					balancer->qos_manual_index = 0;
					pr_info("%s stop\n", __func__);
					kq_balancer_stop_balancer(balancer);
				} else {
					binfo->run = KQ_BALANCER_OPER_RUN;
					balancer->qos_manual = KQ_BALANCER_QOS_CONTROL_MANUAL;
					balancer->qos_manual_index = freq_idx;
					pr_info("%s run\n", __func__);

					ret = kq_balancer_update_balancer_policy(balancer,
						cmd_buf[KQ_BALANCER_CMD_MANUAL_QOS_TYPE]);
					if (ret) {
						pr_err("%s error checking policy\n", __func__);
						return -EINVAL;
					}

					ret = kq_balancer_run_balancer(balancer,
						cmd_buf[KQ_BALANCER_CMD_MANUAL_QOS_TYPE]);
					if (ret) {
						pr_err("%s error running balancer thread\n", __func__);
						return -EINVAL;
					}
				}
			} else {
				pr_err("%s %s domain invalid freq index [max:%d][request:%d]\n",
					__func__, balancer->name, balancer->table_size, freq_idx);
				return -EINVAL;
			}
		}
	}
	return count;
}

static ssize_t kq_balancer_store_attrs(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t count)
{
	const ptrdiff_t offset = attr - kq_balancer_attrs;

	switch (offset) {
	case KQ_BALANCER_SYSFS_BALANCER:
		count = kq_balancer_store_balancer_attr(dev, attr, buf, count);
		break;
	case KQ_BALANCER_SYSFS_STATUS:
		count = kq_balancer_store_status_attr(dev, attr, buf, count);
		break;
	case KQ_BALANCER_SYSFS_PRINTLOG:
		count = kq_balancer_store_printlog_attr(dev, attr, buf, count);
		break;
	case KQ_BALANCER_SYSFS_TIMEOUT:
		count = kq_balancer_store_timeout_attr(dev, attr, buf, count);
		break;
	case KQ_BALANCER_SYSFS_TABLES:
		count = kq_balancer_store_tables_attr(dev, attr, buf, count);
		break;
	case KQ_BALANCER_SYSFS_MANUAL:
		count = kq_balancer_store_manual_attr(dev, attr, buf, count);
		break;
	default:
		pr_info("%s store kq balancer out of range\n", __func__);
		break;
	}
	return count;
}

static int kq_balancer_create_attr(struct device *dev)
{
	int i;
	int ret;

	for (i = 0; i < ARRAY_SIZE(kq_balancer_attrs); i++) {
		//pr_info("%s create %dth file\n", __func__, i+1);
		ret = device_create_file(dev, &kq_balancer_attrs[i]);
		if (ret)
			goto create_balancer_attr_fail;
	}
	return 0;

create_balancer_attr_fail:
	while (i--) {
		pr_err("%s remove %dth file\n", __func__, i);
		device_remove_file(dev, &kq_balancer_attrs[i]);
	}
	return -EINVAL;
}

static int kq_balancer_create_sysfs(struct platform_device *pdev,
	struct balancer_info *binfo)
{
	int ret;

	binfo->sdev = sec_device_create(binfo, "sec_nad_balancer");
	if (IS_ERR(binfo->sdev)) {
		pr_err("%s sysfs create fail\n", __func__);
		return PTR_ERR(binfo->sdev);
	}

	ret = kq_balancer_create_attr(binfo->sdev);
	if (ret) {
		pr_err("%s attr sysfs fail\n", __func__);
		goto error_create_balancer_sysfs;
	}
	return 0;

error_create_balancer_sysfs:
	sec_device_destroy(binfo->sdev->devt);
	return -EINVAL;
}

static int kq_balancer_create_devfreq_table_by_domain(
	struct platform_device *pdev, struct balancer_domain *balancer)
{
	/* TODO : will update from exynos-devfreq */
	/*
	 * struct profiler_device profiler;
	 * struct freq_cstate *fc = &profiler.fc;
	 * exynos_devfreq_get_profile(balancer->num, fc->time, profiler.freq_stats);
	 */
	struct device_node *np = pdev->dev.of_node;
	struct device_node *dev_dp = of_find_node_by_name(np, "dev");
	struct device_node *dev_mif_dp = of_find_node_by_name(dev_dp, "devmif");
	int i;
	int ret;
	int freq;

	if (!dev_mif_dp) {
		pr_err("%s devmif not exist\n", __func__);
		return -EINVAL;
	}

	ret = of_property_read_u32(dev_mif_dp, "dev,size", &balancer->table_size);
	if (ret) {
		pr_err("%s [%s] can't get dev,size\n", __func__, balancer->name);
		return ret;
	}

	balancer->freq_table = devm_kzalloc(&pdev->dev,
		sizeof(struct balancer_freq_table) * (balancer->table_size + 1),
		GFP_KERNEL);

	if (!balancer->freq_table) {
		pr_err("%s failed to allocate [%s] freq table\n",
			 __func__, balancer->name);
		return -ENOMEM;
	}

	for (i = 0; i < balancer->table_size; i++) {
		if (of_property_read_u32_index(dev_mif_dp, "dev,table", i, &freq)) {
			pr_err("%s [%s] can't get [%d] table\n",
				 __func__, balancer->name, i);
			devm_kfree(&pdev->dev, balancer);
			return -EINVAL;
		}
		balancer->freq_table[i].frequency = freq;
	}

	balancer->min_freq = balancer->freq_table[balancer->table_size-1].frequency;
	balancer->max_freq = balancer->freq_table[0].frequency;

	return 0;
}

static int kq_balancer_create_cpufreq_table_by_domain(
	struct platform_device *pdev, struct balancer_domain *balancer)
{
	struct cpufreq_policy *policy;
	struct cpufreq_frequency_table *pos;
	unsigned int freq_table[KQ_BALANCER_MAX_FREQ_LEN];
	int i = 0;

	balancer->min_freq = ~0;
	balancer->max_freq = 0;
	balancer->table_size = 0;

	pr_info("%s checking %s\n", __func__, balancer->name);

	policy = cpufreq_cpu_get_raw(balancer->num);
	if (!policy) {
		pr_err("%s can't get cpu%d policy", __func__, balancer->num);
		return -EINVAL;
	}

	if (!policy->freq_table) {
		pr_err("%s cpu%d table not exist!", __func__, balancer->num);
		return -EINVAL;
	}

	cpufreq_for_each_valid_entry(pos, policy->freq_table) {
		freq_table[i] = pos->frequency;

		if (freq_table[i] < balancer->min_freq)
			balancer->min_freq = freq_table[i];
		if (freq_table[i] > balancer->max_freq)
			balancer->max_freq = freq_table[i];

		balancer->table_size++;
		i++;
	}

	balancer->freq_table = devm_kzalloc(&pdev->dev,
		sizeof(struct balancer_freq_table) * (balancer->table_size + 1),
		GFP_KERNEL);

	if (!balancer->freq_table) {
		pr_err("%s failed to allocate [%s] freq table\n",
			 __func__, balancer->name);
		return -ENOMEM;
	}

	for (i = 0; i < balancer->table_size; i++)
		balancer->freq_table[i].frequency = freq_table[i];

	return 0;
}

static int kq_balancer_create_balancer_freq_table(struct platform_device *pdev)
{
	struct balancer_domain *balancer;

	list_for_each_entry(balancer, &balancer_domains, list) {
		if (kq_balancer_is_domain_cpu(balancer)) {
			if (kq_balancer_create_cpufreq_table_by_domain(pdev, balancer))
				return -EINVAL;
		} else if (kq_balancer_is_domain_dev(balancer)) {
			if (kq_balancer_create_devfreq_table_by_domain(pdev, balancer))
				return -EINVAL;
		} else {
			pr_err("%s invalid domain type\n", __func__);
			return -EINVAL;
		}
	}
	return 0;
}

static int kq_balancer_parse_cpu_dt(struct balancer_domain *balancer,
	struct device_node *dn)
{
	int ret;

	ret = of_property_read_string(dn, "cpu,name",
		(char const **)&balancer->name);
	if (ret) {
		pr_err("%s can't get cpu,name\n", __func__);
		return ret;
	}

	ret = of_property_read_u32(dn, "cpu,base", &balancer->num);
	if (ret) {
		pr_err("%s can't get cpu,base\n", __func__);
		return ret;
	}

	ret = of_property_read_u32(dn, "cpu,qos_delay", &balancer->qos_delay);
	if (ret) {
		pr_err("%s %s no cpu,qos_delay set default delay\n",
			__func__, balancer->name);
		balancer->qos_delay = KQ_BALANCER_DEFAULT_QOS_DELAY;
	}

	pr_info("%s cpu : [%s] [%d] [%d]\n", __func__,
		balancer->name, balancer->num, balancer->qos_delay);

	return 0;
}

static int kq_balancer_parse_dev_dt(struct balancer_domain *balancer,
	struct device_node *dn)
{
	int ret;

	ret = of_property_read_string(dn, "dev,name",
		(char const **)&balancer->name);
	if (ret) {
		pr_err("%s can't get dev,name\n", __func__);
		return ret;
	}

	ret = of_property_read_u32(dn, "dev,base", &balancer->num);
	if (ret) {
		pr_err("%s can't get dev,base\n", __func__);
		return ret;
	}

	ret = of_property_read_u32(dn, "dev,qos_delay", &balancer->qos_delay);
	if (ret) {
		pr_err("%s %s no dev,qos_delay set default delay\n",
			__func__, balancer->name);
		balancer->qos_delay = KQ_BALANCER_DEFAULT_QOS_DELAY;
	}

	pr_info("%s dev : [%s] [%d] [%d]\n", __func__,
		balancer->name, balancer->num, balancer->qos_delay);

	return 0;
}

static int kq_balancer_parse_dt(struct platform_device *pdev,
	struct balancer_info *binfo)
{
	struct device_node *np = pdev->dev.of_node;
	struct device_node *cpu_dp = of_find_node_by_name(np, "cpu");
	struct device_node *dev_dp = of_find_node_by_name(np, "dev");
	struct device_node *cluster_dp;
	struct balancer_domain *balancer;

	/* cpu parts */
	if (!cpu_dp) {
		pr_err("%s there's no cpu information!\n", __func__);
		return -EINVAL;
	}

	for_each_child_of_node(cpu_dp, cluster_dp) {
		balancer = devm_kzalloc(&pdev->dev,
			sizeof(struct balancer_domain), GFP_KERNEL);

		if (!balancer) {
			pr_err("%s failed alloc cpu balancer\n", __func__);
			return -ENOMEM;
		}

		balancer->type = KQ_BALANCER_DOMAIN_TYPE_CPU;

		if (kq_balancer_parse_cpu_dt(balancer,
			cluster_dp)) {
			pr_err("%s failed to initialize cpu domain\n", __func__);
			devm_kfree(&pdev->dev, balancer);
			return -EINVAL;
		}
		list_add_tail(&balancer->list, &balancer_domains);
	}

	/* dev parts */
	if (!dev_dp) {
		pr_err("%s there's no dev information!\n", __func__);
		return -EINVAL;
	}

	for_each_child_of_node(dev_dp, cluster_dp) {
		balancer = devm_kzalloc(&pdev->dev,
			sizeof(struct balancer_domain), GFP_KERNEL);

		if (!balancer) {
			pr_err("%s failed alloc cpu balancer\n", __func__);
			return -ENOMEM;
		}

		balancer->type = KQ_BALANCER_DOMAIN_TYPE_DEV;

		if (kq_balancer_parse_dev_dt(balancer,
			cluster_dp)) {
			pr_err("%s failed to initialize dev domain\n", __func__);
			devm_kfree(&pdev->dev, balancer);
			return -EINVAL;
		}
		list_add_tail(&balancer->list, &balancer_domains);
	}

	/* setting parts */
	if (of_property_read_u32(np, "timeout", &binfo->timeout)) {
		pr_err("%s error to read timeout value\n", __func__);
		return -EINVAL;
	}

	return 0;
}

static struct balancer_info *kq_balancer_alloc_init_info(struct platform_device *pdev)
{
	struct balancer_info *binfo;

	binfo = devm_kzalloc(&pdev->dev,
				sizeof(struct balancer_info), GFP_KERNEL);

	if (!binfo)
		return NULL;

	binfo->timeout = KQ_BALANCER_DEFAULT_OPER_TIMEOUT;
	binfo->run = KQ_BALANCER_OPER_STOP;

	return binfo;
}

static void kq_balancer_update_domain_type_var(struct balancer_domain *balancer)
{
	if (!strncmp(balancer->name, "cpu-lit", 7)) {
		balancer->qos_type = KQ_BALANCER_TYPE_LIT;
		balancer->qos_policy = KQ_BALANCER_QOS_POLICY_TYPE_LIT_MAX;
	} else if (!strncmp(balancer->name, "cpu-mid", 7)) {
		balancer->qos_type = KQ_BALANCER_TYPE_MID;
		balancer->qos_policy = KQ_BALANCER_QOS_POLICY_TYPE_MID_MAX;
	} else if (!strncmp(balancer->name, "cpu-big", 7)) {
		balancer->qos_type = KQ_BALANCER_TYPE_BIG;
		balancer->qos_policy = KQ_BALANCER_QOS_POLICY_TYPE_BIG_MAX;
	} else if (!strncmp(balancer->name, "dev-mif", 7)) {
		balancer->qos_type = KQ_BALANCER_TYPE_MIF;
		balancer->qos_policy = KQ_BALANCER_QOS_POLICY_TYPE_MIF_MAX;
	} else {
		balancer->qos_type = KQ_BALANCER_TYPE_UNSUPPORTED;
		balancer->qos_policy = KQ_BALANCER_QOS_POLICY_NONE;
		balancer->qos_status = KQ_BALANCER_QOS_END;
	}
	pr_info("%s update policy [%d][%s]\n", __func__, balancer->qos_policy,
		kq_balancer_get_domain_qos_policy(balancer->qos_policy));
}

static void kq_balancer_init_remain_balancer_domains_var(struct balancer_info *binfo)
{
	struct balancer_domain *balancer;

	list_for_each_entry(balancer, &balancer_domains, list) {
		balancer->kthread = NULL;
		balancer->run = KQ_BALANCER_OPER_STOP;
		balancer->qos_status = KQ_BALANCER_QOS_UNLOCK;
		balancer->qos_manual = KQ_BALANCER_QOS_CONTROL_RANDOM;
		kq_balancer_update_domain_type_var(balancer);
		binfo->max_domain_count++;
	}
}

static void kq_balancer_delayed_work(struct work_struct *work)
{
	struct balancer_info *binfo =
		container_of(work, struct balancer_info, dwork.work);

	pr_info("%s balancer work timeout!\n", __func__);
	kq_balancer_stop_balancer_thread(binfo);
}

static int kq_balancer_probe(struct platform_device *pdev)
{
	struct balancer_info *binfo = NULL;
	int ret = 0;

	binfo = kq_balancer_alloc_init_info(pdev);
	if (!binfo) {
		pr_err("%s failed to allocate balancer info\n", __func__);
		return ret;
	}

	if (pdev->dev.of_node) {
		ret = kq_balancer_parse_dt(pdev, binfo);
		if (ret) {
			pr_err("%s failed to parse dt\n", __func__);
			goto free_balancer_info;
		}
	} else
		return -EINVAL;

	ret = kq_balancer_create_balancer_freq_table(pdev);
	if (ret) {
		pr_err("%s failed to create balancer table\n", __func__);
		goto free_balancer_info;
	}

	ret = kq_balancer_create_sysfs(pdev, binfo);
	if (ret) {
		pr_err("%s failed to create sysfs\n", __func__);
		goto free_balancer_info;
	}

	kq_balancer_init_remain_balancer_domains_var(binfo);
	kq_balancer_domains_info();

	INIT_DELAYED_WORK(&binfo->dwork, kq_balancer_delayed_work);

	platform_set_drvdata(pdev, binfo);

	return ret;

free_balancer_info:
	devm_kfree(&pdev->dev, binfo);
	return -EINVAL;
}

static const struct of_device_id of_kq_balancer_match[] = {
	{ .compatible = "samsung,kq-balancer", },
	{ },
};

MODULE_DEVICE_TABLE(of, of_kq_balancer_match);

static struct platform_driver kq_balancer_driver = {
	.driver = {
		.name = "kq-balancer",
		.owner = THIS_MODULE,
		.of_match_table = of_kq_balancer_match,
	},
	.probe = kq_balancer_probe,
};

module_platform_driver(kq_balancer_driver);

MODULE_DESCRIPTION("kernel quality clock balancer");
MODULE_AUTHOR("jc22.kim");
MODULE_LICENSE("GPL");
