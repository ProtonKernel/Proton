/*
 * sec_cmd.c - samsung input command driver
 *
 * Copyright (C) 2014 Samsung Electronics
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 */

#include "sec_cmd.h"
#include "sec_input.h"
#include "sec_tsp_log.h"

struct class *tsp_sec_class;

#if IS_ENABLED(CONFIG_SEC_KUNIT)
__visible_for_testing struct sec_cmd_data *kunit_sec;
EXPORT_SYMBOL(kunit_sec);
#else
#define __visible_for_testing static
#endif

#if IS_ENABLED(CONFIG_TOUCHSCREEN_DUAL_FOLDABLE)
static struct sec_cmd_data *main_sec;
static struct sec_cmd_data *sub_sec;
#endif

#ifdef USE_SEC_CMD_QUEUE
static void sec_cmd_store_function(struct sec_cmd_data *data);
void sec_cmd_execution(struct sec_cmd_data *data, bool lock)
{
	if (lock)
		mutex_lock(&data->fs_lock);

	/* check lock	*/
	mutex_lock(&data->cmd_lock);
	data->cmd_is_running = true;
	mutex_unlock(&data->cmd_lock);

	data->cmd_state = SEC_CMD_STATUS_RUNNING;
	sec_cmd_store_function(data);

	if (lock)
		mutex_unlock(&data->fs_lock);
}
#endif

void sec_cmd_set_cmd_exit(struct sec_cmd_data *data)
{
#ifdef USE_SEC_CMD_QUEUE
	mutex_lock(&data->fifo_lock);
	if (kfifo_len(&data->cmd_queue)) {
		pr_info("%s: %s %s: do next cmd, left cmd[%d]\n", dev_name(data->fac_dev), SECLOG, __func__,
			(int)(kfifo_len(&data->cmd_queue) / sizeof(struct command)));
		mutex_unlock(&data->fifo_lock);

#if IS_ENABLED(CONFIG_TOUCHSCREEN_DUAL_FOLDABLE)
		sec_cmd_execution(data, false);
#else
		schedule_work(&data->cmd_work.work);
#endif
	} else {
		mutex_unlock(&data->fifo_lock);

		mutex_lock(&data->cmd_lock);
		data->cmd_is_running = false;
		mutex_unlock(&data->cmd_lock);
	}
	if (data->wait_cmd_result_done)
		complete_all(&data->cmd_result_done);
#else
	mutex_lock(&data->cmd_lock);
	data->cmd_is_running = false;
	mutex_unlock(&data->cmd_lock);
#endif
}
EXPORT_SYMBOL(sec_cmd_set_cmd_exit);

#ifdef USE_SEC_CMD_QUEUE
static void cmd_exit_work(struct work_struct *work)
{
	struct sec_cmd_data *data = container_of(work, struct sec_cmd_data, cmd_work.work);

	sec_cmd_execution(data, true);
}
#endif

void sec_cmd_set_default_result(struct sec_cmd_data *data)
{
	char *delim = ":";
	memset(data->cmd_result, 0x00, SEC_CMD_RESULT_STR_LEN_EXPAND);
	memcpy(data->cmd_result, data->cmd, SEC_CMD_STR_LEN);
	strlcat(data->cmd_result, delim, SEC_CMD_RESULT_STR_LEN_EXPAND);
}
EXPORT_SYMBOL(sec_cmd_set_default_result);

void sec_cmd_set_cmd_result_all(struct sec_cmd_data *data, char *buff, int len, char *item)
{
	char *delim1 = " ";
	char *delim2 = ":";
	int cmd_result_len;

	cmd_result_len = (int)strlen(data->cmd_result_all) + len + 2 + (int)strlen(item);

	if (cmd_result_len >= SEC_CMD_RESULT_STR_LEN) {
		pr_err("%s: %s %s: cmd length is over (%d)!!", dev_name(data->fac_dev), SECLOG, __func__, cmd_result_len);
		return;
	}

	data->item_count++;
	strlcat(data->cmd_result_all, delim1, sizeof(data->cmd_result_all));
	strlcat(data->cmd_result_all, item, sizeof(data->cmd_result_all));
	strlcat(data->cmd_result_all, delim2, sizeof(data->cmd_result_all));
	strlcat(data->cmd_result_all, buff, sizeof(data->cmd_result_all));
}
EXPORT_SYMBOL(sec_cmd_set_cmd_result_all);

void sec_cmd_set_cmd_result(struct sec_cmd_data *data, char *buff, int len)
{
	if (strlen(buff) >= (unsigned int)SEC_CMD_RESULT_STR_LEN_EXPAND) {
		pr_err("%s %s: cmd length is over (%d)!!", SECLOG, __func__, (int)strlen(buff));
		strlcat(data->cmd_result, "NG", SEC_CMD_RESULT_STR_LEN_EXPAND);
		return;
	}

	data->cmd_result_expand = (int)strlen(buff) / SEC_CMD_RESULT_STR_LEN;
	data->cmd_result_expand_count = 0;

	strlcat(data->cmd_result, buff, SEC_CMD_RESULT_STR_LEN_EXPAND);
}
EXPORT_SYMBOL(sec_cmd_set_cmd_result);

#ifndef USE_SEC_CMD_QUEUE
__visible_for_testing ssize_t sec_cmd_store(struct device *dev,
		struct device_attribute *devattr, const char *buf, size_t count)
{
	struct sec_cmd_data *data = dev_get_drvdata(dev);
	char *cur, *start, *end;
	char buff[SEC_CMD_STR_LEN] = { 0 };
	int len, i;
	struct sec_cmd *sec_cmd_ptr = NULL;
	char delim = ',';
	bool cmd_found = false;
	int param_cnt = 0;

	if (!data) {
		pr_err("%s %s: No platform data found\n", SECLOG, __func__);
		return -EINVAL;
	}

	if (strnlen(buf, SEC_CMD_STR_LEN) >= SEC_CMD_STR_LEN) {
		pr_err("%s: %s %s: cmd length(strlen(buf)) is over (%d,%s)!!\n",
				dev_name(data->fac_dev), SECLOG, __func__, (int)strlen(buf), buf);
		return -EINVAL;
	}

	if (count >= (unsigned int)SEC_CMD_STR_LEN) {
		pr_err("%s: %s %s: cmd length(count) is over (%d,%s)!!\n",
				dev_name(data->fac_dev), SECLOG, __func__, (unsigned int)count, buf);
		return -EINVAL;
	}

	if (data->cmd_is_running == true) {
		pr_err("%s: %s %s: other cmd is running.\n", dev_name(data->fac_dev), SECLOG, __func__);
		return -EBUSY;
	}

	/* check lock   */
	mutex_lock(&data->cmd_lock);
	data->cmd_is_running = true;
	mutex_unlock(&data->cmd_lock);

	data->cmd_state = SEC_CMD_STATUS_RUNNING;
	for (i = 0; i < ARRAY_SIZE(data->cmd_param); i++)
		data->cmd_param[i] = 0;

	len = (int)count;
	if (*(buf + len - 1) == '\n')
		len--;

	memset(data->cmd, 0x00, ARRAY_SIZE(data->cmd));
	memcpy(data->cmd, buf, len);

	cur = strchr(buf, (int)delim);
	if (cur)
		memcpy(buff, buf, cur - buf);
	else
		memcpy(buff, buf, len);

	pr_debug("%s: %s %s: COMMAND = %s\n", dev_name(data->fac_dev), SECLOG, __func__, buff);

	/* find command */
	list_for_each_entry(sec_cmd_ptr, &data->cmd_list_head, list) {
		if (!strncmp(buff, sec_cmd_ptr->cmd_name, SEC_CMD_STR_LEN)) {
			cmd_found = true;
			break;
		}
	}

check_not_support_cmd:
	/* set not_support_cmd */
	if (!cmd_found) {
		list_for_each_entry(sec_cmd_ptr, &data->cmd_list_head, list) {
			if (!strncmp("not_support_cmd", sec_cmd_ptr->cmd_name,
				SEC_CMD_STR_LEN))
				break;
		}
	}

	/* parsing parameters */
	if (cur && cmd_found) {
		cur++;
		start = cur;
		memset(buff, 0x00, ARRAY_SIZE(buff));

		do {
			if (*cur == delim || cur - buf == len) {
				end = cur;
				memcpy(buff, start, end - start);
				*(buff + strnlen(buff, ARRAY_SIZE(buff))) = '\0';
				if (kstrtoint(buff, 10, data->cmd_param + param_cnt) < 0) {
					pr_err("%s: %s %s: error to parse parameter\n",
							dev_name(data->fac_dev), SECLOG, __func__);
					cmd_found = false;
					goto check_not_support_cmd;
				}
				start = cur + 1;
				memset(buff, 0x00, ARRAY_SIZE(buff));
				param_cnt++;
			}
			cur++;
		} while ((cur - buf <= len) && (param_cnt < SEC_CMD_PARAM_NUM));
	}

	if (cmd_found) {
		pr_info("%s: %s %s: cmd = %s", dev_name(data->fac_dev), SECLOG, __func__, sec_cmd_ptr->cmd_name);
		for (i = 0; i < param_cnt; i++) {
			if (i == 0)
				pr_cont(" param =");
			pr_cont(" %d", data->cmd_param[i]);
		}
		pr_cont("\n");
	} else {
		pr_info("%s: %s %s: cmd = %s(%s)\n", dev_name(data->fac_dev), SECLOG, __func__, buff, sec_cmd_ptr->cmd_name);
	}

	sec_cmd_ptr->cmd_func(data);

	return count;
}
#if IS_ENABLED(CONFIG_SEC_KUNIT)
EXPORT_SYMBOL_KUNIT(sec_cmd_store);
#endif

#else	/* defined USE_SEC_CMD_QUEUE */
static void sec_cmd_store_function(struct sec_cmd_data *data)
{
	char *cur, *start, *end;
	char buff[SEC_CMD_STR_LEN] = { 0 };
	int len, i;
	struct sec_cmd *sec_cmd_ptr = NULL;
	char delim = ',';
	bool cmd_found = false;
	int param_cnt = 0;
	int ret;
	const char *buf;
	size_t count;
	struct command cmd = {{0}};

	if (!data) {
		pr_err("%s %s: No platform data found\n", SECLOG, __func__);
		return;
	}

	mutex_lock(&data->fifo_lock);
	if (kfifo_len(&data->cmd_queue)) {
		ret = kfifo_out(&data->cmd_queue, &cmd, sizeof(struct command));
		if (!ret) {
			pr_err("%s: %s %s: kfifo_out failed, it seems empty, ret=%d\n", dev_name(data->fac_dev), SECLOG, __func__, ret);
			mutex_unlock(&data->fifo_lock);
			return;
		}
	} else {
		pr_err("%s: %s %s: left cmd is nothing\n", dev_name(data->fac_dev), SECLOG, __func__);
		mutex_unlock(&data->fifo_lock);
		mutex_lock(&data->cmd_lock);
		data->cmd_is_running = false;
		mutex_unlock(&data->cmd_lock);
		return;
	}
	mutex_unlock(&data->fifo_lock);

	buf = cmd.cmd;
	count = strlen(buf);

	for (i = 0; i < (int)ARRAY_SIZE(data->cmd_param); i++)
		data->cmd_param[i] = 0;

	len = (int)count;
	if (*(buf + len - 1) == '\n')
		len--;

	memset(data->cmd, 0x00, ARRAY_SIZE(data->cmd));
	memcpy(data->cmd, buf, len);

	cur = strchr(buf, (int)delim);
	if (cur)
		memcpy(buff, buf, cur - buf);
	else
		memcpy(buff, buf, len);

	pr_debug("%s: %s %s: COMMAND : %s\n", dev_name(data->fac_dev), SECLOG, __func__, buff);

	/* find command */
	list_for_each_entry(sec_cmd_ptr, &data->cmd_list_head, list) {
		if (!strncmp(buff, sec_cmd_ptr->cmd_name, SEC_CMD_STR_LEN)) {
			cmd_found = true;
			break;
		}
	}

check_not_support_cmd:
	/* set not_support_cmd */
	if (!cmd_found) {
		list_for_each_entry(sec_cmd_ptr, &data->cmd_list_head, list) {
			if (!strncmp("not_support_cmd", sec_cmd_ptr->cmd_name,
				SEC_CMD_STR_LEN))
				break;
		}
	}

	/* parsing parameters */
	if (cur && cmd_found) {
		cur++;
		start = cur;
		memset(buff, 0x00, ARRAY_SIZE(buff));

		do {
			if (*cur == delim || cur - buf == len) {
				end = cur;
				memcpy(buff, start, end - start);
				*(buff + strnlen(buff, ARRAY_SIZE(buff))) = '\0';
				if (kstrtoint(buff, 10, data->cmd_param + param_cnt) < 0) {
					pr_err("%s: %s %s: error to parse parameter\n",
							dev_name(data->fac_dev), SECLOG, __func__);
					cmd_found = false;
					goto check_not_support_cmd;
				}
				start = cur + 1;
				memset(buff, 0x00, ARRAY_SIZE(buff));
				param_cnt++;
			}
			cur++;
		} while ((cur - buf <= len) && (param_cnt < SEC_CMD_PARAM_NUM));
	}

	if (cmd_found) {
		pr_info("%s: %s %s: cmd = %s", dev_name(data->fac_dev), SECLOG, __func__, sec_cmd_ptr->cmd_name);
		for (i = 0; i < param_cnt; i++) {
			if (i == 0)
				pr_cont(" param =");
			pr_cont(" %d", data->cmd_param[i]);
		}
		pr_cont("\n");
	} else {
		pr_info("%s: %s %s: cmd = %s(%s)\n", dev_name(data->fac_dev), SECLOG, __func__, buff, sec_cmd_ptr->cmd_name);
	}

	sec_cmd_ptr->cmd_func(data);

	if (cmd_found && sec_cmd_ptr->cmd_log) {
		char tbuf[32];
		unsigned long long t;
		unsigned long nanosec_rem;

		memset(tbuf, 0x00, sizeof(tbuf));
		t = local_clock();
		nanosec_rem = do_div(t, 1000000000);
		snprintf(tbuf, sizeof(tbuf), "[r:%lu.%06lu]",
				(unsigned long)t,
				nanosec_rem / 1000);
#if IS_ENABLED(CONFIG_SEC_DEBUG_TSP_LOG)
		sec_debug_tsp_command_history(tbuf);
#endif
	}
}

__visible_for_testing ssize_t sec_cmd_store(struct device *dev, struct device_attribute *devattr,
			   const char *buf, size_t count)
{
	struct sec_cmd_data *data = dev_get_drvdata(dev);
	struct command cmd = {{0}};
	struct sec_cmd *sec_cmd_ptr = NULL;
	int queue_size;

	if (!data) {
		pr_err("%s %s: No platform data found\n", SECLOG, __func__);
		return -EINVAL;
	}

	if (strnlen(buf, SEC_CMD_STR_LEN) >= SEC_CMD_STR_LEN) {
		pr_err("%s: %s %s: cmd length(strlen(buf)) is over (%d,%s)!!\n",
				dev_name(data->fac_dev), SECLOG, __func__, (int)strlen(buf), buf);
		return -EINVAL;
	}

	if (count >= (unsigned int)SEC_CMD_STR_LEN) {
		pr_err("%s: %s %s: cmd length(count) is over (%d,%s)!!\n",
				dev_name(data->fac_dev), SECLOG, __func__, (unsigned int)count, buf);
		return -EINVAL;
	}

	if (strnlen(buf, SEC_CMD_STR_LEN) == 0) {
		pr_err("%s: %s %s: cmd length is zero (%d,%s) count(%ld)!!\n",
				dev_name(data->fac_dev), SECLOG, __func__, (int)strlen(buf), buf, count);
		return -EINVAL;
	}

	strncpy(cmd.cmd, buf, count);

	if (data->wait_cmd_result_done) {
		int ret;

		mutex_lock(&data->wait_lock);
		if (!data->cmd_result_done.done)
			pr_info("%s: %s %s: %s - waiting prev cmd...\n", dev_name(data->fac_dev), SECLOG, __func__, cmd.cmd);
		ret = wait_for_completion_interruptible_timeout(&data->cmd_result_done, msecs_to_jiffies(2000));
		if (ret <= 0)
			pr_err("%s: %s %s: completion %d\n", dev_name(data->fac_dev), SECLOG, __func__, ret);

		reinit_completion(&data->cmd_result_done);
		mutex_unlock(&data->wait_lock);
	}

	list_for_each_entry(sec_cmd_ptr, &data->cmd_list_head, list) {
		if (!strncmp(cmd.cmd, sec_cmd_ptr->cmd_name, strlen(sec_cmd_ptr->cmd_name))) {
			if (sec_cmd_ptr->cmd_log) {
				char task_info[40];
				char tbuf[32];
				unsigned long long t;
				unsigned long nanosec_rem;

				memset(tbuf, 0x00, sizeof(tbuf));
				t = local_clock();
				nanosec_rem = do_div(t, 1000000000);
				snprintf(tbuf, sizeof(tbuf), "[q:%lu.%06lu]",
						(unsigned long)t,
						nanosec_rem / 1000);

				snprintf(task_info, 40, "\n[%d:%s:%s]", current->pid, current->comm, dev_name(data->fac_dev));
#if IS_ENABLED(CONFIG_SEC_DEBUG_TSP_LOG)
				sec_debug_tsp_command_history(task_info);
				sec_debug_tsp_command_history(cmd.cmd);
				sec_debug_tsp_command_history(tbuf);
#endif
			}
			break;
		}
	}

	mutex_lock(&data->fifo_lock);
	queue_size = (kfifo_len(&data->cmd_queue) / sizeof(struct command));

	if (kfifo_avail(&data->cmd_queue) && (queue_size < SEC_CMD_MAX_QUEUE)) {
		kfifo_in(&data->cmd_queue, &cmd, sizeof(struct command));
		pr_info("%s: %s %s: push cmd: %s\n", dev_name(data->fac_dev), SECLOG, __func__, cmd.cmd);
	} else {
		pr_err("%s: %s %s: cmd_queue is full!!\n", dev_name(data->fac_dev), SECLOG, __func__);

		kfifo_reset(&data->cmd_queue);
		pr_err("%s: %s %s: cmd_queue is reset!!\n", dev_name(data->fac_dev), SECLOG, __func__);
		mutex_unlock(&data->fifo_lock);

		mutex_lock(&data->cmd_lock);
		data->cmd_is_running = false;
		mutex_unlock(&data->cmd_lock);

		if (data->wait_cmd_result_done)
			complete_all(&data->cmd_result_done);

		return -ENOSPC;
	}

	if (data->cmd_is_running == true) {
		pr_err("%s: %s %s: other cmd is running. Wait until previous cmd is done[%d]\n",
			dev_name(data->fac_dev), SECLOG, __func__, (int)(kfifo_len(&data->cmd_queue) / sizeof(struct command)));
		mutex_unlock(&data->fifo_lock);
		return count;
	}
	mutex_unlock(&data->fifo_lock);

	sec_cmd_execution(data, true);
	return count;
}
#if IS_ENABLED(CONFIG_SEC_KUNIT)
EXPORT_SYMBOL_KUNIT(sec_cmd_store);
#endif
#endif

__visible_for_testing ssize_t sec_cmd_show_status(struct device *dev,
				 struct device_attribute *devattr, char *buf)
{
	struct sec_cmd_data *data = dev_get_drvdata(dev);
	char buff[16] = { 0 };

	if (!data) {
		pr_err("%s %s: No platform data found\n", SECLOG, __func__);
		return -EINVAL;
	}

	if (data->cmd_state == SEC_CMD_STATUS_WAITING)
		snprintf(buff, sizeof(buff), "WAITING");

	else if (data->cmd_state == SEC_CMD_STATUS_RUNNING)
		snprintf(buff, sizeof(buff), "RUNNING");

	else if (data->cmd_state == SEC_CMD_STATUS_OK)
		snprintf(buff, sizeof(buff), "OK");

	else if (data->cmd_state == SEC_CMD_STATUS_FAIL)
		snprintf(buff, sizeof(buff), "FAIL");

	else if (data->cmd_state == SEC_CMD_STATUS_EXPAND)
		snprintf(buff, sizeof(buff), "EXPAND");

	else if (data->cmd_state == SEC_CMD_STATUS_NOT_APPLICABLE)
		snprintf(buff, sizeof(buff), "NOT_APPLICABLE");

	pr_debug("%s: %s %s: %d, %s\n", dev_name(data->fac_dev), SECLOG, __func__, data->cmd_state, buff);

	return snprintf(buf, sizeof(buff), "%s\n", buff);
}
#if IS_ENABLED(CONFIG_SEC_KUNIT)
EXPORT_SYMBOL_KUNIT(sec_cmd_show_status);
#endif

static ssize_t sec_cmd_show_status_all(struct device *dev,
				 struct device_attribute *devattr, char *buf)
{
	struct sec_cmd_data *data = dev_get_drvdata(dev);
	char buff[16] = { 0 };

	if (!data) {
		pr_err("%s %s: No platform data found\n", SECLOG, __func__);
		return -EINVAL;
	}

	if (data->cmd_all_factory_state == SEC_CMD_STATUS_WAITING)
		snprintf(buff, sizeof(buff), "WAITING");

	else if (data->cmd_all_factory_state == SEC_CMD_STATUS_RUNNING)
		snprintf(buff, sizeof(buff), "RUNNING");

	else if (data->cmd_all_factory_state == SEC_CMD_STATUS_OK)
		snprintf(buff, sizeof(buff), "OK");

	else if (data->cmd_all_factory_state == SEC_CMD_STATUS_FAIL)
		snprintf(buff, sizeof(buff), "FAIL");

	else if (data->cmd_state == SEC_CMD_STATUS_EXPAND)
		snprintf(buff, sizeof(buff), "EXPAND");

	else if (data->cmd_all_factory_state == SEC_CMD_STATUS_NOT_APPLICABLE)
		snprintf(buff, sizeof(buff), "NOT_APPLICABLE");

	pr_debug("%s: %s %s: %d, %s\n", dev_name(data->fac_dev), SECLOG, __func__, data->cmd_all_factory_state, buff);

	return snprintf(buf, sizeof(buff), "%s\n", buff);
}

__visible_for_testing ssize_t sec_cmd_show_result(struct device *dev,
				 struct device_attribute *devattr, char *buf)
{
	struct sec_cmd_data *data = dev_get_drvdata(dev);
	int size;

	if (!data) {
		pr_err("%s %s: No platform data found\n", SECLOG, __func__);
		return -EINVAL;
	}

	size = snprintf(buf, SEC_CMD_RESULT_STR_LEN, "%s\n",
		data->cmd_result + (SEC_CMD_RESULT_STR_LEN - 1) * data->cmd_result_expand_count);

	if (data->cmd_result_expand_count != data->cmd_result_expand) {
		data->cmd_state = SEC_CMD_STATUS_EXPAND;
		data->cmd_result_expand_count++;
	} else {
		data->cmd_state = SEC_CMD_STATUS_WAITING;
	}

	pr_info("%s: %s %s: %s\n", dev_name(data->fac_dev), SECLOG, __func__, buf);

	sec_cmd_set_cmd_exit(data);

	return size;
}
#if IS_ENABLED(CONFIG_SEC_KUNIT)
EXPORT_SYMBOL_KUNIT(sec_cmd_show_result);
#endif

static ssize_t sec_cmd_show_result_all(struct device *dev,
				 struct device_attribute *devattr, char *buf)
{
	struct sec_cmd_data *data = dev_get_drvdata(dev);
	int size;

	if (!data) {
		pr_err("%s %s: No platform data found\n", SECLOG, __func__);
		return -EINVAL;
	}

	data->cmd_state = SEC_CMD_STATUS_WAITING;
	pr_info("%s: %s %s: %d, %s\n", dev_name(data->fac_dev), SECLOG, __func__, data->item_count, data->cmd_result_all);
	size = snprintf(buf, SEC_CMD_RESULT_STR_LEN, "%d%s\n", data->item_count, data->cmd_result_all);

	sec_cmd_set_cmd_exit(data);

	data->item_count = 0;
	memset(data->cmd_result_all, 0x00, SEC_CMD_RESULT_STR_LEN);

	return size;
}

static ssize_t sec_cmd_list_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct sec_cmd_data *data = dev_get_drvdata(dev);
	struct sec_cmd *sec_cmd_ptr = NULL;
	char *buffer;
	char buffer_name[SEC_CMD_STR_LEN];
	int ret = 0;

	buffer = kzalloc(data->cmd_buffer_size + 30, GFP_KERNEL);
	if (!buffer)
		return -ENOMEM;

	snprintf(buffer, 30, "++factory command list++\n");

	list_for_each_entry(sec_cmd_ptr, &data->cmd_list_head, list) {
		if (strncmp(sec_cmd_ptr->cmd_name, "not_support_cmd", 15)) {
			snprintf(buffer_name, SEC_CMD_STR_LEN, "%s\n", sec_cmd_ptr->cmd_name);
			strlcat(buffer, buffer_name, data->cmd_buffer_size + 30);
		}
	}

	ret = snprintf(buf, SEC_CMD_BUF_SIZE, "%s\n", buffer);
	kfree(buffer);

	return ret;
}

static DEVICE_ATTR(cmd, 0220, NULL, sec_cmd_store);
static DEVICE_ATTR(cmd_status, 0444, sec_cmd_show_status, NULL);
static DEVICE_ATTR(cmd_status_all, 0444, sec_cmd_show_status_all, NULL);
static DEVICE_ATTR(cmd_result, 0444, sec_cmd_show_result, NULL);
static DEVICE_ATTR(cmd_result_all, 0444, sec_cmd_show_result_all, NULL);
static DEVICE_ATTR(cmd_list, 0444, sec_cmd_list_show, NULL);

static struct attribute *sec_fac_attrs[] = {
	&dev_attr_cmd.attr,
	&dev_attr_cmd_status.attr,
	&dev_attr_cmd_status_all.attr,
	&dev_attr_cmd_result.attr,
	&dev_attr_cmd_result_all.attr,
	&dev_attr_cmd_list.attr,
	NULL,
};

static struct attribute_group sec_fac_attr_group = {
	.attrs = sec_fac_attrs,
};


int sec_cmd_init(struct sec_cmd_data *data, struct sec_cmd *cmds,
			int len, int devt)
{
	const char *dev_name;
	int ret, i;

	INIT_LIST_HEAD(&data->cmd_list_head);

	data->cmd_buffer_size = 0;
	for (i = 0; i < len; i++) {
		list_add_tail(&cmds[i].list, &data->cmd_list_head);
		if (cmds[i].cmd_name)
			data->cmd_buffer_size += strlen(cmds[i].cmd_name) + 1;
	}

	mutex_init(&data->cmd_lock);
	mutex_init(&data->fs_lock);

	mutex_lock(&data->cmd_lock);
	data->cmd_is_running = false;
	mutex_unlock(&data->cmd_lock);

	data->cmd_result = kzalloc(SEC_CMD_RESULT_STR_LEN_EXPAND, GFP_KERNEL);
	if (!data->cmd_result)
		goto err_alloc_cmd_result;

#ifdef USE_SEC_CMD_QUEUE
	if (kfifo_alloc(&data->cmd_queue,
		SEC_CMD_MAX_QUEUE * sizeof(struct command), GFP_KERNEL)) {
		pr_err("%s %s: failed to alloc queue for cmd\n", SECLOG, __func__);
		goto err_alloc_queue;
	}
	mutex_init(&data->fifo_lock);
	mutex_init(&data->wait_lock);
	init_completion(&data->cmd_result_done);
	complete_all(&data->cmd_result_done);

	INIT_DELAYED_WORK(&data->cmd_work, cmd_exit_work);
#endif

	switch (devt) {
	case SEC_CLASS_DEVT_TSP:
		dev_name = SEC_CLASS_DEV_NAME_TSP;
		break;
#if IS_ENABLED(CONFIG_TOUCHSCREEN_DUAL_FOLDABLE)
	case SEC_CLASS_DEVT_TSP1:
		dev_name = SEC_CLASS_DEV_NAME_TSP1;
		break;
	case SEC_CLASS_DEVT_TSP2:
		dev_name = SEC_CLASS_DEV_NAME_TSP2;
		break;
#endif
	case SEC_CLASS_DEVT_TKEY:
		dev_name = SEC_CLASS_DEV_NAME_TKEY;
		break;
	case SEC_CLASS_DEVT_WACOM:
		dev_name = SEC_CLASS_DEV_NAME_WACOM;
		break;
	case SEC_CLASS_DEVT_SIDEKEY:
		dev_name = SEC_CLASS_DEV_NAME_SIDEKEY;
		break;
	default:
		pr_err("%s %s: not defined devt=%d\n", SECLOG, __func__, devt);
		goto err_get_dev_name;
	}

#if IS_ENABLED(CONFIG_DRV_SAMSUNG)
	data->fac_dev = sec_device_create(data, dev_name);
#else
	tsp_sec_class = class_create(THIS_MODULE, "tsp_sec");
	if (unlikely(IS_ERR(tsp_sec_class))) {
		pr_err("%s %s: Failed to create class(sec) %ld\n", SECLOG, __func__, PTR_ERR(tsp_sec_class));
		return PTR_ERR(tsp_sec_class);
	}
	data->fac_dev = device_create(tsp_sec_class, NULL, devt, data, "%s", dev_name);
#endif

	if (IS_ERR(data->fac_dev)) {
		pr_err("%s %s: failed to create device for the sysfs\n", SECLOG, __func__);
		goto err_sysfs_device;
	}

	dev_set_drvdata(data->fac_dev, data);

	ret = sysfs_create_group(&data->fac_dev->kobj, &sec_fac_attr_group);
	if (ret < 0) {
		pr_err("%s %s: failed to create sysfs group\n", SECLOG, __func__);
		goto err_sysfs_group;
	}

#if IS_ENABLED(CONFIG_TOUCHSCREEN_DUAL_FOLDABLE)
	switch (devt) {
	case SEC_CLASS_DEVT_TSP1:
	case SEC_CLASS_DEVT_TSP2:
		sec_cmd_virtual_tsp_register(data);
		break;
	};
#endif

	pr_info("%s: %s %s: done\n", dev_name, SECLOG, __func__);

	return 0;

err_sysfs_group:
#if IS_ENABLED(CONFIG_DRV_SAMSUNG)
	sec_device_destroy(data->fac_dev->devt);
#else
	device_destroy(tsp_sec_class, devt);
#endif
err_sysfs_device:
err_get_dev_name:
#ifdef USE_SEC_CMD_QUEUE
	mutex_destroy(&data->fifo_lock);
	kfifo_free(&data->cmd_queue);
	mutex_destroy(&data->wait_lock);
err_alloc_queue:
#endif
	kfree(data->cmd_result);
err_alloc_cmd_result:
	mutex_destroy(&data->cmd_lock);
	list_del(&data->cmd_list_head);
	return -ENODEV;
}
EXPORT_SYMBOL(sec_cmd_init);

void sec_cmd_exit(struct sec_cmd_data *data, int devt)
{
#ifdef USE_SEC_CMD_QUEUE
	struct command cmd = {{0}};
	int ret;
#endif

	pr_info("%s: %s %s\n", dev_name(data->fac_dev), SECLOG, __func__);
	sysfs_remove_group(&data->fac_dev->kobj, &sec_fac_attr_group);
	dev_set_drvdata(data->fac_dev, NULL);
#if IS_ENABLED(CONFIG_DRV_SAMSUNG)
	sec_device_destroy(data->fac_dev->devt);
#else
	device_destroy(tsp_sec_class, devt);
#endif
#ifdef USE_SEC_CMD_QUEUE
	mutex_lock(&data->fifo_lock);
	while (kfifo_len(&data->cmd_queue)) {
		ret = kfifo_out(&data->cmd_queue, &cmd, sizeof(struct command));
		if (!ret) {
			pr_err("%s %s: kfifo_out failed, it seems empty, ret=%d\n", SECLOG, __func__, ret);
		}
		pr_info("%s %s: remove pending commands: %s", SECLOG, __func__, cmd.cmd);
	}
	mutex_unlock(&data->fifo_lock);
	mutex_destroy(&data->fifo_lock);
	kfifo_free(&data->cmd_queue);
	mutex_destroy(&data->wait_lock);

	cancel_delayed_work_sync(&data->cmd_work);
	flush_delayed_work(&data->cmd_work);
#endif
	data->fac_dev = NULL;
	kfree(data->cmd_result);
	mutex_destroy(&data->cmd_lock);
	list_del(&data->cmd_list_head);

#if IS_ENABLED(CONFIG_TOUCHSCREEN_DUAL_FOLDABLE)
	if (devt == SEC_CLASS_DEVT_TSP1)
		main_sec = NULL;
	if (devt == SEC_CLASS_DEVT_TSP2)
		sub_sec = NULL;
#endif
}
EXPORT_SYMBOL(sec_cmd_exit);

void sec_cmd_send_event_to_user(struct sec_cmd_data *data, char *test, char *result)
{
	char *event[5];
	char timestamp[32];
	char feature[32];
	char stest[32];
	char sresult[64];
	ktime_t calltime;
	u64 realtime;
	int curr_time;
	char *eol = "\0";

	if (!data || !data->fac_dev)
		return;

	calltime = ktime_get();
	realtime = ktime_to_ns(calltime);
	do_div(realtime, NSEC_PER_USEC);
	curr_time = realtime / USEC_PER_MSEC;

	snprintf(timestamp, 32, "TIMESTAMP=%d", curr_time);
	strncat(timestamp, eol, 1);
	snprintf(feature, 32, "FEATURE=TSP");
	strncat(feature, eol, 1);
	if (!test) {
		snprintf(stest, 32, "TEST=NULL");
	} else {
		snprintf(stest, 32, "%s", test);
	}
	strncat(stest, eol, 1);

	if (!result) {
		snprintf(sresult, 64, "RESULT=NULL");
	} else {
		snprintf(sresult, 64, "%s", result);
	}
	strncat(sresult, eol, 1);

	pr_info("%s: %s %s: time:%s, feature:%s, test:%s, result:%s\n",
			dev_name(data->fac_dev), SECLOG, __func__, timestamp, feature, stest, sresult);

	event[0] = timestamp;
	event[1] = feature;
	event[2] = stest;
	event[3] = sresult;
	event[4] = NULL;

	kobject_uevent_env(&data->fac_dev->kobj, KOBJ_CHANGE, event);
}
EXPORT_SYMBOL(sec_cmd_send_event_to_user);

#if IS_ENABLED(CONFIG_TOUCHSCREEN_DUAL_FOLDABLE)
void sec_cmd_virtual_tsp_register(struct sec_cmd_data *sec)
{
	if (strcmp(dev_name(sec->fac_dev), SEC_CLASS_DEV_NAME_TSP1) == 0) {
		main_sec = sec;
		input_info(true, sec->fac_dev, "%s: main\n", __func__);
#if (LINUX_VERSION_CODE > KERNEL_VERSION(5, 10, 0))
		main_sec->sysfs_functions = devm_kzalloc(main_sec->fac_dev, sizeof(struct sec_ts_virtual_sysfs_function), GFP_KERNEL);
		if (!main_sec->sysfs_functions) {
			return;
		}
#endif
	} else if (strcmp(dev_name(sec->fac_dev), SEC_CLASS_DEV_NAME_TSP2) == 0) {
		sub_sec = sec;
		input_info(true, sec->fac_dev, "%s: sub\n", __func__);
#if (LINUX_VERSION_CODE > KERNEL_VERSION(5, 10, 0))
		sub_sec->sysfs_functions = devm_kzalloc(sub_sec->fac_dev, sizeof(struct sec_ts_virtual_sysfs_function), GFP_KERNEL);
		if (!sub_sec->sysfs_functions) {
			return ;
		}
#endif
	}
}

#if (LINUX_VERSION_CODE > KERNEL_VERSION(5, 10, 0))
int sec_cmd_virtual_tsp_read_sysfs(struct sec_cmd_data *sec, const char *path, char *buf, int len)
{
	int ret = 0;

	if (main_sec) {
		if (strcmp(path, PATH_MAIN_SEC_CMD_STATUS) == 0)
			sec_cmd_show_status(main_sec->fac_dev, NULL, buf);
		else if (strcmp(path, PATH_MAIN_SEC_CMD_RESULT) == 0)
			sec_cmd_show_result(main_sec->fac_dev, NULL, buf);
		else if (strcmp(path, PATH_MAIN_SEC_CMD_STATUS_ALL) == 0)
			sec_cmd_show_status_all(main_sec->fac_dev, NULL, buf);
		else if (strcmp(path, PATH_MAIN_SEC_CMD_RESULT_ALL) == 0)
			sec_cmd_show_result_all(main_sec->fac_dev, NULL, buf);
		else if (strcmp(path, PATH_MAIN_SEC_SYSFS_SUPPORT_FEATURE) == 0) {
			if (main_sec->sysfs_functions->sec_tsp_support_feature_show != NULL)
				main_sec->sysfs_functions->sec_tsp_support_feature_show(main_sec->fac_dev, NULL, buf);
		} else if (strcmp(path, PATH_MAIN_SEC_SYSFS_PROX_POWER_OFF) == 0) {
			if (main_sec->sysfs_functions->sec_tsp_prox_power_off_show != NULL)
				main_sec->sysfs_functions->sec_tsp_prox_power_off_show(main_sec->fac_dev, NULL, buf);
		}
	}

	if (sub_sec) {
		if (strcmp(path, PATH_SUB_SEC_CMD_STATUS) == 0)
			sec_cmd_show_status(sub_sec->fac_dev, NULL, buf);
		else if (strcmp(path, PATH_SUB_SEC_CMD_RESULT) == 0)
			sec_cmd_show_result(sub_sec->fac_dev, NULL, buf);
		else if (strcmp(path, PATH_SUB_SEC_CMD_STATUS_ALL) == 0)
			sec_cmd_show_status_all(sub_sec->fac_dev, NULL, buf);
		else if (strcmp(path, PATH_SUB_SEC_CMD_RESULT_ALL) == 0)
			sec_cmd_show_result_all(sub_sec->fac_dev, NULL, buf);
		else if (strcmp(path, PATH_SUB_SEC_SYSFS_PROX_POWER_OFF) == 0) {
			if (sub_sec->sysfs_functions->sec_tsp_prox_power_off_show != NULL)
				sub_sec->sysfs_functions->sec_tsp_prox_power_off_show(sub_sec->fac_dev, NULL, buf);
		}
	}

	if (ret < 0) {
		input_err(true, sec->fac_dev, "%s: failed to read, len:%d, ret:%d\n", __func__, len, ret);
		ret = -EIO;
	}

	return ret;
}
#else
int sec_cmd_virtual_tsp_read_sysfs(struct sec_cmd_data *sec, const char *path, char *buf, int len)
{
	int ret = 0;
	mm_segment_t old_fs;
	struct file *sysfs;

	old_fs = get_fs();
	set_fs(KERNEL_DS);

	sysfs = filp_open(path, O_RDONLY, 0444);
	if (IS_ERR(sysfs)) {
		ret = PTR_ERR(sysfs);
		input_err(true, sec->fac_dev, "%s: %s open fail, %d\n", __func__, path, ret);
		set_fs(old_fs);
		return ret;
	}

	ret = sysfs->f_op->read(sysfs, buf, len, &sysfs->f_pos);
	if (ret < 0) {
		input_err(true, sec->fac_dev, "%s: failed to read, len:%d, ret:%d\n", __func__, len, ret);
		ret = -EIO;
	}

	filp_close(sysfs, current->files);
	set_fs(old_fs);

	return ret;
}
#endif
EXPORT_SYMBOL(sec_cmd_virtual_tsp_read_sysfs);

#if (LINUX_VERSION_CODE > KERNEL_VERSION(5, 10, 0))
int sec_cmd_virtual_tsp_write_sysfs(struct sec_cmd_data *sec, const char *path, const char *cmd)
{
	int ret = 0;
	int len;

	len = strlen(cmd);
	if (strncmp(path, PATH_MAIN_SEC_CMD, 23) == 0) {
		if (main_sec)
			ret = sec_cmd_store(main_sec->fac_dev, NULL, cmd, len);
	} else if (strncmp(path, PATH_SUB_SEC_CMD, 23) == 0) {
		if (sub_sec)
			ret = sec_cmd_store(sub_sec->fac_dev, NULL, cmd, len);
	} else if (strncmp(path, PATH_MAIN_SEC_SYSFS_DUALSCREEN_POLICY, 38) == 0) {
		if (main_sec) {
			if (main_sec->sysfs_functions->dualscreen_policy_store != NULL)
				ret = main_sec->sysfs_functions->dualscreen_policy_store(main_sec->fac_dev, NULL, cmd, len);
		}
	} else if (strncmp(path, PATH_SUB_SEC_SYSFS_DUALSCREEN_POLICY, 38) == 0) {
		if (sub_sec) {
			if (sub_sec->sysfs_functions->dualscreen_policy_store != NULL)
				ret = sub_sec->sysfs_functions->dualscreen_policy_store(sub_sec->fac_dev, NULL, cmd, len);
		}
	} else if (strncmp(path, PATH_MAIN_SEC_SYSFS_PROX_POWER_OFF, 34) == 0) {
		if (main_sec) {
			if (main_sec->sysfs_functions->sec_tsp_prox_power_off_store != NULL)
				ret = main_sec->sysfs_functions->sec_tsp_prox_power_off_store(main_sec->fac_dev, NULL, cmd, len);
		}
	} else if (strncmp(path, PATH_SUB_SEC_SYSFS_PROX_POWER_OFF, 34) == 0) {
		if (sub_sec) {
			if (sub_sec->sysfs_functions->sec_tsp_prox_power_off_store != NULL)
				ret = sub_sec->sysfs_functions->sec_tsp_prox_power_off_store(sub_sec->fac_dev, NULL, cmd, len);
		}
	}

	if (ret != len) {
		input_err(true, sec->fac_dev, "%s: failed to write, len:%d, ret:%d\n", __func__, len, ret);
		ret = -EIO;
	}

	return ret;
}
#else
int sec_cmd_virtual_tsp_write_sysfs(struct sec_cmd_data *sec, const char *path, const char *cmd)
{
	int ret = 0;
	mm_segment_t old_fs;
	struct file *sysfs;
	int len;

	if (strncmp(path, PATH_SUB_SEC_SYSFS_DUALSCREEN_POLICY, 38) == 0)
		return ret;

	len = strlen(cmd);
	old_fs = get_fs();
	set_fs(KERNEL_DS);

	sysfs = filp_open(path, O_WRONLY, 0220);
	if (IS_ERR(sysfs)) {
		ret = PTR_ERR(sysfs);
		input_err(true, sec->fac_dev, "%s: %s open fail, %d\n", __func__, path, ret);
		set_fs(old_fs);
		return ret;
	}

	ret = sysfs->f_op->write(sysfs, cmd, len, &sysfs->f_pos);
	if (ret != len) {
		input_err(true, sec->fac_dev, "%s: failed to write, len:%d, ret:%d\n", __func__, len, ret);
		ret = -EIO;
	}

	filp_close(sysfs, current->files);
	set_fs(old_fs);

	return ret;
}
#endif
EXPORT_SYMBOL(sec_cmd_virtual_tsp_write_sysfs);

static int sec_cmd_virtual_tsp_get_cmd_status(struct sec_cmd_data *sec, char *path)
{
	u8 buff[16];
	int ret;

	memset(buff, 0x00, sizeof(buff));

	ret = sec_cmd_virtual_tsp_read_sysfs(sec, path, buff, sizeof(buff));
	if (ret < 0)
		return SEC_CMD_STATUS_FAIL;

	if (strncmp(buff, "WAITING", 7) == 0)
		return SEC_CMD_STATUS_WAITING;
	else if (strncmp(buff, "OK", 2) == 0)
		return SEC_CMD_STATUS_OK;
	else if (strncmp(buff, "FAIL", 4) == 0)
		return SEC_CMD_STATUS_FAIL;
	else if (strncmp(buff, "RUNNING", 7) == 0)
		return SEC_CMD_STATUS_RUNNING;
	else if (strncmp(buff, "EXPAND", 6) == 0)
		return SEC_CMD_STATUS_EXPAND;
	else
		return SEC_CMD_STATUS_NOT_APPLICABLE;
}

int sec_cmd_virtual_tsp_write_cmd(struct sec_cmd_data *sec, bool main, bool sub)
{
	u8 buff[16];
	int ret_sub = 0;
	int ret_main = 0;
	bool exit = false;

	sec_cmd_set_default_result(sec);

	if (!main && !sub) {
		snprintf(buff, sizeof(buff), "%s", "NA");
		sec->cmd_state = SEC_CMD_STATUS_NOT_APPLICABLE;
		goto err;
	}

	if (sub && sub_sec) {
		input_dbg(true, sec->fac_dev, "%s: send to sub\n", sec->cmd);
		ret_sub = sec_cmd_virtual_tsp_write_sysfs(sec, PATH_SUB_SEC_CMD, sec->cmd);
		if (ret_sub < 0) {
			snprintf(buff, sizeof(buff), "%s", "NG");
			sec->cmd_state = SEC_CMD_STATUS_FAIL;
			goto main;
		}
		sec->cmd_state = sec_cmd_virtual_tsp_get_cmd_status(sec, PATH_SUB_SEC_CMD_STATUS);
		input_dbg(true, sec->fac_dev, "%s: sub_sec OK\n", sec->cmd);
		if (!sub_sec->cmd_is_running)
			exit = true;
		sec_cmd_virtual_tsp_read_sysfs(sec, PATH_SUB_SEC_CMD_RESULT, sec->cmd_result, SEC_CMD_RESULT_STR_LEN);
		memset(sec->cmd_result, 0x00, SEC_CMD_RESULT_STR_LEN_EXPAND);
		sec_cmd_set_cmd_result(sec, sub_sec->cmd_result, strlen(sub_sec->cmd_result));
	}
main:
	if (main && main_sec) {
		input_dbg(true, sec->fac_dev, "%s: send to main\n", sec->cmd);
		ret_main = sec_cmd_virtual_tsp_write_sysfs(sec, PATH_MAIN_SEC_CMD, sec->cmd);
		if (ret_main < 0) {
			snprintf(buff, sizeof(buff), "%s", "NG");
			sec->cmd_state = SEC_CMD_STATUS_FAIL;
			goto err;
		}
		sec->cmd_state = sec_cmd_virtual_tsp_get_cmd_status(sec, PATH_MAIN_SEC_CMD_STATUS);
		input_dbg(true, sec->fac_dev, "%s: main_sec OK\n", sec->cmd);
		if (!main_sec->cmd_is_running)
			exit = true;
		sec_cmd_virtual_tsp_read_sysfs(sec, PATH_MAIN_SEC_CMD_RESULT, sec->cmd_result, SEC_CMD_RESULT_STR_LEN);
		memset(sec->cmd_result, 0x00, SEC_CMD_RESULT_STR_LEN_EXPAND);
		sec_cmd_set_cmd_result(sec, main_sec->cmd_result, strlen(main_sec->cmd_result));
	}
	if (exit) {
		input_dbg(true, sec->fac_dev, "%s: set_cmd_exit\n", sec->cmd);
		sec_cmd_set_cmd_exit(sec);
	} else if ((main && !main_sec) || (sub && !sub_sec)) {
		input_err(true, sec->fac_dev, "%s: some device is not registered in virtual tsp.\n", sec->cmd);
		sec_cmd_set_cmd_exit(sec);
	}

	return (ret_sub < 0 || ret_main < 0) ? -1 : 0;

err:
	sec_cmd_set_cmd_result(sec, buff, SEC_CMD_RESULT_STR_LEN);
	sec_cmd_set_cmd_exit(sec);

	return -1;
}
EXPORT_SYMBOL(sec_cmd_virtual_tsp_write_cmd);

void sec_cmd_virtual_tsp_write_cmd_factory_all(struct sec_cmd_data *sec, bool main, bool sub)
{
	u8 buff[16];
	int ret = 0;

	sec_cmd_set_default_result(sec);

	if (!main && !sub) {
		snprintf(buff, sizeof(buff), "%s", "NA");
		sec->cmd_state = SEC_CMD_STATUS_NOT_APPLICABLE;
		goto err;
	}

	if (sub && sub_sec) {
		input_dbg(true, sec->fac_dev, "%s: sub\n", sec->cmd);
		ret = sec_cmd_virtual_tsp_write_sysfs(sec, PATH_SUB_SEC_CMD, sec->cmd);
		if (ret < 0) {
			snprintf(buff, sizeof(buff), "%s", "NG");
			sec->cmd_all_factory_state = SEC_CMD_STATUS_FAIL;
			goto main;
		}
		sec->cmd_all_factory_state = sec_cmd_virtual_tsp_get_cmd_status(sec, PATH_SUB_SEC_CMD_STATUS_ALL);
		sec_cmd_virtual_tsp_read_sysfs(sec, PATH_SUB_SEC_CMD_RESULT_ALL, sec->cmd_result_all, SEC_CMD_RESULT_STR_LEN);
	}
main:
	if (main && main_sec) {
		input_dbg(true, sec->fac_dev, "%s: main\n", sec->cmd);
		ret = sec_cmd_virtual_tsp_write_sysfs(sec, PATH_MAIN_SEC_CMD, sec->cmd);
		if (ret < 0) {
			snprintf(buff, sizeof(buff), "%s", "NG");
			sec->cmd_all_factory_state = SEC_CMD_STATUS_FAIL;
			goto err;
		}
		sec->cmd_all_factory_state = sec_cmd_virtual_tsp_get_cmd_status(sec, PATH_MAIN_SEC_CMD_STATUS_ALL);
		sec_cmd_virtual_tsp_read_sysfs(sec, PATH_MAIN_SEC_CMD_RESULT_ALL, sec->cmd_result_all, SEC_CMD_RESULT_STR_LEN);
	}

	return;

err:
	sec_cmd_set_cmd_result_all(sec, buff, SEC_CMD_RESULT_STR_LEN, "NONE");
}
EXPORT_SYMBOL(sec_cmd_virtual_tsp_write_cmd_factory_all);
#endif

#if IS_ENABLED(CONFIG_SEC_KUNIT) && !IS_ENABLED(CONFIG_TOUCHSCREEN_DUAL_FOLDABLE)

static int __init sec_cmd_m_init(void)
{
	return 0;
}

static void __exit sec_cmd_m_exit(void)
{
}

module_init(sec_cmd_m_init);
module_exit(sec_cmd_m_exit);
#endif

MODULE_DESCRIPTION("Samsung input command");
MODULE_LICENSE("GPL");

