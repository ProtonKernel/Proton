/* abc_common.c
 *
 * Abnormal Behavior Catcher Common Driver
 *
 * Copyright (C) 2017 Samsung Electronics
 *
 * Hyeokseon Yu <hyeokseon.yu@samsung.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 */
#include <linux/sti/abc_common.h>
#if IS_ENABLED(CONFIG_SEC_KUNIT)
#include <linux/sti/abc_kunit.h>
#endif
#define DEBUG_ABC
#define ABC_WARNING_REPORT

struct device *sec_abc;
EXPORT_SYMBOL_KUNIT(sec_abc);
__visible_for_testing int abc_enabled;
EXPORT_SYMBOL_KUNIT(abc_enabled);
__visible_for_testing int abc_init;
EXPORT_SYMBOL_KUNIT(abc_init);

#if IS_ENABLED(CONFIG_SEC_KUNIT)
char abc_hub_kunit_test_uevent_str[ABC_HUB_TEST_STR_MAX];
EXPORT_SYMBOL_KUNIT(abc_hub_kunit_test_uevent_str);

void abc_hub_test_get_uevent_str(char *uevent_str)
{
	strlcpy(abc_hub_kunit_test_uevent_str, uevent_str, sizeof(abc_hub_kunit_test_uevent_str));
}
EXPORT_SYMBOL_KUNIT(abc_hub_test_get_uevent_str);

char abc_common_kunit_test_work_str[ABC_TEST_UEVENT_MAX][ABC_TEST_STR_MAX] = {"", };
EXPORT_SYMBOL_KUNIT(abc_common_kunit_test_work_str);
char abc_common_kunit_test_log_str[ABC_TEST_STR_MAX];
EXPORT_SYMBOL_KUNIT(abc_common_kunit_test_log_str);

void abc_common_test_get_work_str(char *uevent_str[])
{
	int i;

	for (i = 0; i < ABC_TEST_UEVENT_MAX; i++) {
		if (uevent_str[i]) {
			if (i >= 2 && !strncmp(uevent_str[i], TIME_KEYWORD, strlen(TIME_KEYWORD)))
				strlcpy(abc_common_kunit_test_work_str[i],
						TIME_KEYWORD, ABC_TEST_STR_MAX);
			else
				strlcpy(abc_common_kunit_test_work_str[i],
						uevent_str[i], ABC_TEST_STR_MAX);
		}
	}
}
EXPORT_SYMBOL_KUNIT(abc_common_test_get_work_str);

void abc_common_test_get_log_str(char *log_str)
{
	strlcpy(abc_common_kunit_test_log_str, log_str, sizeof(abc_common_kunit_test_log_str));
}
#endif /* CONFIG_SEC_KUNIT */

#if IS_ENABLED(CONFIG_OF)

static int parse_gpu_data(struct device *dev,
			  struct abc_platform_data *pdata,
			  struct device_node *np)
{
	struct abc_qdata *cgpu;

	cgpu = pdata->gpu_items;
	cgpu->desc = of_get_property(np, "gpu,label", NULL);

	if (of_property_read_u32(np, "gpu,threshold_count", &cgpu->threshold_cnt)) {
		dev_err(dev, "Failed to get gpu threshold count: node not exist\n");
		return -EINVAL;
	}

	if (of_property_read_u32(np, "gpu,threshold_time", &cgpu->threshold_time)) {
		dev_err(dev, "Failed to get gpu threshold time: node not exist\n");
		return -EINVAL;
	}

	cgpu->buffer.abc_element = kzalloc(sizeof(cgpu->buffer.abc_element[0]) * (cgpu->threshold_cnt + 1), GFP_KERNEL);

	if (!cgpu->buffer.abc_element)
		return -ENOMEM;

	cgpu->buffer.size = cgpu->threshold_cnt + 1;
	cgpu->buffer.rear = 0;
	cgpu->buffer.front = 0;
	cgpu->fail_cnt = 0;

	return 0;
}

static int parse_gpu_page_data(struct device *dev,
				   struct abc_platform_data *pdata,
				   struct device_node *np)
{
	struct abc_qdata *cgpu_page;

	cgpu_page = pdata->gpu_page_items;
	cgpu_page->desc = of_get_property(np, "gpu_page,label", NULL);

	if (of_property_read_u32(np, "gpu_page,threshold_count", &cgpu_page->threshold_cnt)) {
		dev_err(dev, "Failed to get gpu_page threshold count: node not exist\n");
		return -EINVAL;
	}

	if (of_property_read_u32(np, "gpu_page,threshold_time", &cgpu_page->threshold_time)) {
		dev_err(dev, "Failed to get gpu_page threshold time: node not exist\n");
		return -EINVAL;
	}

	cgpu_page->buffer.abc_element = kzalloc(sizeof(cgpu_page->buffer.abc_element[0]) *
						(cgpu_page->threshold_cnt + 1), GFP_KERNEL);

	if (!cgpu_page->buffer.abc_element)
		return -ENOMEM;

	cgpu_page->buffer.size = cgpu_page->threshold_cnt + 1;
	cgpu_page->buffer.rear = 0;
	cgpu_page->buffer.front = 0;
	cgpu_page->fail_cnt = 0;

	return 0;
}

static int parse_aicl_data(struct device *dev,
			   struct abc_platform_data *pdata,
			   struct device_node *np)
{
	struct abc_qdata *caicl;

	caicl = pdata->aicl_items;
	caicl->desc = of_get_property(np, "aicl,label", NULL);

	if (of_property_read_u32(np, "aicl,threshold_count", &caicl->threshold_cnt)) {
		dev_err(dev, "Failed to get aicl threshold count: node not exist\n");
		return -EINVAL;
	}

	if (of_property_read_u32(np, "aicl,threshold_time", &caicl->threshold_time)) {
		dev_err(dev, "Failed to get aicl threshold time: node not exist\n");
		return -EINVAL;
	}

	caicl->buffer.abc_element = kzalloc(sizeof(caicl->buffer.abc_element[0]) *
						(caicl->threshold_cnt + 1), GFP_KERNEL);

	if (!caicl->buffer.abc_element)
		return -ENOMEM;

	caicl->buffer.size = caicl->threshold_cnt + 1;
	caicl->buffer.rear = 0;
	caicl->buffer.front = 0;
	caicl->fail_cnt = 0;

	return 0;
}

static int abc_parse_dt(struct device *dev)
{
	struct abc_platform_data *pdata = dev->platform_data;
	struct device_node *np;
	struct device_node *gpu_np;
	struct device_node *gpu_page_np;
	struct device_node *aicl_np;

	np = dev->of_node;
	pdata->nItem = of_get_child_count(np);
	if (!pdata->nItem) {
		dev_err(dev, "There are no items\n");
		return -ENODEV;
	}

	gpu_np = of_find_node_by_name(np, "gpu");
	pdata->nGpu = of_get_child_count(gpu_np);
	pdata->gpu_items = devm_kzalloc(dev,
					sizeof(struct abc_qdata), GFP_KERNEL);

	if (!pdata->gpu_items) {
		dev_err(dev, "Failed to allocate GPU memory\n");
		return -ENOMEM;
	}

	if (gpu_np)
		parse_gpu_data(dev, pdata, gpu_np);

	gpu_page_np = of_find_node_by_name(np, "gpu_page");
	pdata->nGpuPage = of_get_child_count(gpu_page_np);
	pdata->gpu_page_items = devm_kzalloc(dev,
						 sizeof(struct abc_qdata), GFP_KERNEL);

	if (!pdata->gpu_page_items) {
		dev_err(dev, "Failed to allocate GPU PAGE memory\n");
		return -ENOMEM;
	}

	if (gpu_page_np)
		parse_gpu_page_data(dev, pdata, gpu_page_np);

	aicl_np = of_find_node_by_name(np, "aicl");
	pdata->nAicl = of_get_child_count(aicl_np);
	pdata->aicl_items = devm_kzalloc(dev,
					 sizeof(struct abc_qdata), GFP_KERNEL);

	if (!pdata->aicl_items) {
		dev_err(dev, "Failed to allocate AICL memory\n");
		return -ENOMEM;
	}

	if (aicl_np)
		parse_aicl_data(dev, pdata, aicl_np);

	return 0;
}
#endif

#if IS_ENABLED(CONFIG_OF)
static const struct of_device_id sec_abc_dt_match[] = {
	{ .compatible = "samsung,sec_abc" },
	{ }
};
#endif

static int sec_abc_resume(struct device *dev)
{
	return 0;
}

static int sec_abc_remove(struct platform_device *pdev)
{
	return 0;
}

static const struct dev_pm_ops sec_abc_pm = {
	.resume = sec_abc_resume,
};

__visible_for_testing void sec_abc_reset_gpu_buffer(void)
{
	struct abc_info *pinfo = dev_get_drvdata(sec_abc);

	pinfo->pdata->gpu_items->buffer.rear = 0;
	pinfo->pdata->gpu_items->buffer.front = 0;
	pinfo->pdata->gpu_items->fail_cnt = 0;
}
EXPORT_SYMBOL_KUNIT(sec_abc_reset_gpu_buffer);

__visible_for_testing void sec_abc_reset_gpu_page_buffer(void)
{
	struct abc_info *pinfo = dev_get_drvdata(sec_abc);

	pinfo->pdata->gpu_page_items->buffer.rear = 0;
	pinfo->pdata->gpu_page_items->buffer.front = 0;
	pinfo->pdata->gpu_page_items->fail_cnt = 0;
}
EXPORT_SYMBOL_KUNIT(sec_abc_reset_gpu_page_buffer);

__visible_for_testing void sec_abc_reset_aicl_buffer(void)
{
	struct abc_info *pinfo = dev_get_drvdata(sec_abc);

	pinfo->pdata->aicl_items->buffer.rear = 0;
	pinfo->pdata->aicl_items->buffer.front = 0;
	pinfo->pdata->aicl_items->fail_cnt = 0;
}
EXPORT_SYMBOL_KUNIT(sec_abc_reset_aicl_buffer);

__visible_for_testing ssize_t store_abc_enabled(struct device *dev,
				 struct device_attribute *attr,
				 const char *buf, size_t count)
{
	struct abc_info *pinfo = dev_get_drvdata(sec_abc);

	if (!strncmp(buf, "1", 1)) {
		ABC_PRINT("ERROR report mode enabled.\n");
#if IS_ENABLED(CONFIG_SEC_KUNIT)
		abc_common_test_get_log_str("ERROR report mode enabled.\n");
#endif
		abc_enabled |= ERROR_REPORT_MODE_BIT;
		complete(&pinfo->enable_done);
	} else if (!strncmp(buf, "0", 1)) {
		ABC_PRINT("ERROR report mode disabled.\n");
#if IS_ENABLED(CONFIG_SEC_KUNIT)
		abc_common_test_get_log_str("ERROR report mode disabled.\n");
#endif
		abc_enabled &= ~(ERROR_REPORT_MODE_BIT);
	} else if (!strncmp(buf, "ALL_REPORT=1", 12)) {
		ABC_PRINT("ALL report mode enabled.\n");
#if IS_ENABLED(CONFIG_SEC_KUNIT)
		abc_common_test_get_log_str("ALL report mode enabled.\n");
#endif
		abc_enabled |= ALL_REPORT_MODE_BIT;
		complete(&pinfo->enable_done);
	} else if (!strncmp(buf, "ALL_REPORT=0", 12)) {
		ABC_PRINT("ALL report mode disabled.\n");
#if IS_ENABLED(CONFIG_SEC_KUNIT)
		abc_common_test_get_log_str("ALL report mode disabled.\n");
#endif
		abc_enabled &= ~(ALL_REPORT_MODE_BIT);
	} else {
		ABC_PRINT("Invalid input.\n");
#if IS_ENABLED(CONFIG_SEC_KUNIT)
		abc_common_test_get_log_str("Invalid input.\n");
#endif
	}

	if (abc_enabled == ABC_DISABLED) {
		sec_abc_reset_gpu_buffer();
		sec_abc_reset_gpu_page_buffer();
		sec_abc_reset_aicl_buffer();
	}

	return count;
}
EXPORT_SYMBOL_KUNIT(store_abc_enabled);

static ssize_t show_abc_enabled(struct device *dev,
				struct device_attribute *attr,
				char *buf)
{
	if (abc_enabled)
		return sprintf(buf, "1\n");
	else
		return sprintf(buf, "0\n");
}
static DEVICE_ATTR(enabled, 0644, show_abc_enabled, store_abc_enabled);

/* reset abc log_list */
static ssize_t store_abc_log(struct device *dev,
				 struct device_attribute *attr,
				 const char *buf, size_t count)
{
	struct abc_info *pinfo = dev_get_drvdata(sec_abc);
	struct abc_log_entry *abc_log;

	mutex_lock(&pinfo->log_mutex);

	pinfo->log_list_cnt = 0;

	while (!list_empty(&pinfo->log_list)) {
		abc_log = list_first_entry(&pinfo->log_list, struct abc_log_entry, node);
		list_del(&abc_log->node);
		kfree(abc_log);
	}

	mutex_unlock(&pinfo->log_mutex);

	return count;
}

/* read abc log_list */
static ssize_t show_abc_log(struct device *dev,
				struct device_attribute *attr,
				char *buf)
{
	struct abc_info *pinfo = dev_get_drvdata(sec_abc);
	struct abc_log_entry *abc_log;
	int count = 0;

	mutex_lock(&pinfo->log_mutex);

	list_for_each_entry(abc_log, &pinfo->log_list, node) {
		count += snprintf(buf + count, PAGE_SIZE - count, "%s\n", abc_log->abc_log_str);
	}

	mutex_unlock(&pinfo->log_mutex);

	return count;
}
static DEVICE_ATTR(log, 0644, show_abc_log, store_abc_log);

__visible_for_testing int sec_abc_is_full(struct abc_buffer *buffer)
{
	if ((buffer->rear + 1) % buffer->size == buffer->front)
		return 1;
	else
		return 0;
}
EXPORT_SYMBOL_KUNIT(sec_abc_is_full);

__visible_for_testing int sec_abc_is_empty(struct abc_buffer *buffer)
{
	if (buffer->front == buffer->rear)
		return 1;
	else
		return 0;
}
EXPORT_SYMBOL_KUNIT(sec_abc_is_empty);

__visible_for_testing void sec_abc_enqueue(struct abc_buffer *buffer, struct abc_fault_info in)
{
	if (sec_abc_is_full(buffer)) {
		ABC_PRINT("queue is full.\n");
#if IS_ENABLED(CONFIG_SEC_KUNIT)
		abc_common_test_get_log_str("queue is full.\n");
#endif
	} else {
		buffer->rear = (buffer->rear + 1) % buffer->size;
		buffer->abc_element[buffer->rear] = in;
	}
}
EXPORT_SYMBOL_KUNIT(sec_abc_enqueue);

__visible_for_testing void sec_abc_dequeue(struct abc_buffer *buffer, struct abc_fault_info *out)
{
	if (sec_abc_is_empty(buffer)) {
		ABC_PRINT("queue is empty.\n");
#if IS_ENABLED(CONFIG_SEC_KUNIT)
		abc_common_test_get_log_str("queue is empty.\n");
#endif
	} else {
		buffer->front = (buffer->front + 1) % buffer->size;
		*out = buffer->abc_element[buffer->front];
	}
}
EXPORT_SYMBOL_KUNIT(sec_abc_dequeue);

__visible_for_testing int sec_abc_get_diff_time(struct abc_buffer *buffer)
{
	int front_time, rear_time;

	front_time = buffer->abc_element[(buffer->front + 1) % buffer->size].cur_time;
	rear_time = buffer->abc_element[buffer->rear].cur_time;

	ABC_PRINT("front time : %d sec (%d) rear_time %d sec (%d) diff : %d\n",
		  front_time,
		  buffer->front + 1,
		  rear_time,
		  buffer->rear,
		  rear_time - front_time);

	return rear_time - front_time;
}
EXPORT_SYMBOL_KUNIT(sec_abc_get_diff_time);

int sec_abc_get_enabled(void)
{
	return abc_enabled;
}
EXPORT_SYMBOL(sec_abc_get_enabled);

static void sec_abc_get_uevent_level_str(char *uevent_level_str, char *event_level, char *event_type)
{
	if (abc_enabled & ALL_REPORT_MODE_BIT) {
#if IS_ENABLED(CONFIG_SEC_FACTORY)
		if (!strncmp(event_type, "gpu_", 4))
			snprintf(uevent_level_str, ABC_BUFFER_MAX, "WARN=%s", event_type);
		else
			snprintf(uevent_level_str, ABC_BUFFER_MAX, "INFO=%s", event_type);
#else
		if (!strncmp(event_level, "INFO", 4))
			snprintf(uevent_level_str, ABC_BUFFER_MAX, "INFO=%s", event_type);
		else
			snprintf(uevent_level_str, ABC_BUFFER_MAX, "WARN=%s", event_type);
#endif
	} else {
		snprintf(uevent_level_str, ABC_BUFFER_MAX, "%s=%s", event_level, event_type);
	}
}

static void sec_abc_work_func(struct work_struct *work)
{
	struct abc_info *pinfo = container_of(work, struct abc_info, work);
	struct abc_qdata *pgpu, *pgpu_page, *paicl;
	struct abc_fault_info in, out;
	struct abc_log_entry *abc_log;

	char *c, *p, *p2;
	char *uevent_str[ABC_UEVENT_MAX] = {0,};
	char temp[ABC_BUFFER_MAX], timestamp[ABC_BUFFER_MAX], temp2[ABC_BUFFER_MAX];
	char uevent_level_str[ABC_BUFFER_MAX] = {0,};
	char *event_level, *event_type;
#if IS_ENABLED(CONFIG_SEC_ABC_MOTTO)
	char temp3[ABC_BUFFER_MAX], *event_module, *tempp;
#endif
	int idx = 0;
	int i = 0;
	u64 ktime;
	unsigned long ktime_ms;
	int ktime_rem;

	strcpy(temp, pinfo->abc_str);
	p = &temp[0];

	/* Caculate current kernel time */
	ktime = local_clock();
	ktime_rem = do_div(ktime, NSEC_PER_MSEC);
	ktime_ms = (unsigned long)ktime;
	ktime_rem = do_div(ktime, MSEC_PER_SEC);

	/* Parse uevent string */
	while ((c = strsep(&p, "@")) != NULL) {
		uevent_str[idx] = c;
		idx++;
	}
	sprintf(timestamp, "TIMESTAMP=%lu", ktime_ms);
	uevent_str[idx++] = &timestamp[0];
	uevent_str[idx] = NULL;

	strcpy(temp2, uevent_str[1]);
	p2 = &temp2[0];
	event_level = strsep(&p2, "=");
	event_type = p2;
	if (event_level == NULL || event_type == NULL) {
		ABC_PRINT("invalid event\n");
		return;
	}

	ABC_PRINT("event_level : %s, event type : %s\n", event_level, event_type);
#if IS_ENABLED(CONFIG_SEC_ABC_MOTTO)
	strcpy(temp3,uevent_str[0]);
	event_module = &temp3[0];
	tempp = strsep(&event_module, "=");

	if (event_module != NULL) {
		ABC_PRINT("event_module : %s\n", event_module);
		motto_send_device_info(event_module, event_type);
	}
#endif

	sec_abc_get_uevent_level_str(uevent_level_str, event_level, event_type);

	if (abc_enabled & ALL_REPORT_MODE_BIT) {
		uevent_str[1] = uevent_level_str;
		kobject_uevent_env(&sec_abc->kobj, KOBJ_CHANGE, uevent_str);
	}

	if (!strncmp(uevent_level_str, "INFO=", 5)) {
		ABC_PRINT("event_level is INFO. Don't Send uevent\n");
		return;
	}

#if defined(DEBUG_ABC)
	/* print except for TIMESTAMP(uevent_str[idx - 1]) */
	for (i = 0; i < idx - 1; i++)
		ABC_PRINT("%s\n", uevent_str[i]);
#endif

	/* Add abc log_list */
	mutex_lock(&pinfo->log_mutex);

	abc_log = kzalloc(sizeof(*abc_log), GFP_KERNEL);
	if (abc_log) {
		snprintf(abc_log->abc_log_str, ABC_LOG_STR_LEN, "[%5lu.%03d]%s_%s",
			 (unsigned long)ktime, (int)(ktime_rem / NSEC_PER_MSEC), uevent_str[0] + 7, uevent_str[1] + 6);
		if (pinfo->log_list_cnt < ABC_LOG_MAX) {
			list_add_tail(&abc_log->node, &pinfo->log_list);
			pinfo->log_list_cnt++;
		} else {
			list_add_tail(&abc_log->node, &pinfo->log_list);
			abc_log = list_first_entry(&pinfo->log_list, struct abc_log_entry, node);
			list_del(&abc_log->node);
			kfree(abc_log);
		}
	} else {
		ABC_PRINT("failed to allocate abc_log\n");
	}

	mutex_unlock(&pinfo->log_mutex);

	if (abc_enabled != ABC_DISABLED) {
		snprintf(uevent_level_str, ABC_BUFFER_MAX, "ERROR=%s", event_type);
		uevent_str[1] = uevent_level_str;

		pgpu = pinfo->pdata->gpu_items;
		pgpu_page = pinfo->pdata->gpu_page_items;
		paicl = pinfo->pdata->aicl_items;
		/* GPU fault */
		if (pgpu->buffer.size && !strncasecmp(event_type, "gpu_fault", 9)) {
			in.cur_time = (unsigned long)ktime;
			in.cur_cnt = pgpu->fail_cnt++;

			ABC_PRINT("gpu fail count : %d\n", pgpu->fail_cnt);
			sec_abc_enqueue(&pgpu->buffer, in);

			/* Check gpu fault */
			/* Case 1 : Over threshold count */
			if (pgpu->fail_cnt >= pgpu->threshold_cnt) {
				if (sec_abc_get_diff_time(&pgpu->buffer) < pgpu->threshold_time) {
					ABC_PRINT("GPU fault occurred. Send uevent.\n");
#if IS_ENABLED(CONFIG_SEC_KUNIT)
					abc_common_test_get_work_str(uevent_str);
#endif
					kobject_uevent_env(&sec_abc->kobj, KOBJ_CHANGE, uevent_str);
				}
				pgpu->fail_cnt = 0;
				sec_abc_dequeue(&pgpu->buffer, &out);
#if IS_ENABLED(CONFIG_SEC_KUNIT)
				abc_common_test_get_work_str(uevent_str);
#endif
				ABC_PRINT("cur_time : %lu sec cur_cnt : %d\n", out.cur_time, out.cur_cnt);
			/* Case 2 : Check front and rear node in queue. Because it's occurred within max count */
			} else if (sec_abc_is_full(&pgpu->buffer)) {
				if (sec_abc_get_diff_time(&pgpu->buffer) < pgpu->threshold_time) {
					ABC_PRINT("GPU fault occurred. Send uevent.\n");
#if IS_ENABLED(CONFIG_SEC_KUNIT)
					abc_common_test_get_work_str(uevent_str);
#endif
					kobject_uevent_env(&sec_abc->kobj, KOBJ_CHANGE, uevent_str);
				}
				sec_abc_dequeue(&pgpu->buffer, &out);
				ABC_PRINT("cur_time : %lu sec cur_cnt : %d\n", out.cur_time, out.cur_cnt);
			}
		} else if (pgpu_page->buffer.size && !strncasecmp(event_type, "gpu_page_fault", 14)) { /* gpu page fault */
			in.cur_time = (unsigned long)ktime;
			in.cur_cnt = pgpu_page->fail_cnt++;

			ABC_PRINT("gpu_page fail count : %d\n", pgpu_page->fail_cnt);
			sec_abc_enqueue(&pgpu_page->buffer, in);

			/* Check gpu_page fault */
			/* Case 1 : Over threshold count */
			if (pgpu_page->fail_cnt >= pgpu_page->threshold_cnt) {
				if (sec_abc_get_diff_time(&pgpu_page->buffer) < pgpu_page->threshold_time) {
					ABC_PRINT("GPU PAGE fault occurred. Send uevent.\n");
#if IS_ENABLED(CONFIG_SEC_KUNIT)
					abc_common_test_get_work_str(uevent_str);
#endif
					kobject_uevent_env(&sec_abc->kobj, KOBJ_CHANGE, uevent_str);
				}
				pgpu_page->fail_cnt = 0;
				sec_abc_dequeue(&pgpu_page->buffer, &out);

				ABC_PRINT("cur_time : %lu sec cur_cnt : %d\n", out.cur_time, out.cur_cnt);
			/* Case 2 : Check front and rear node in queue. Because it's occurred within max count */
			} else if (sec_abc_is_full(&pgpu_page->buffer)) {
				if (sec_abc_get_diff_time(&pgpu_page->buffer) < pgpu_page->threshold_time) {
					ABC_PRINT("GPU PAGE fault occurred. Send uevent.\n");
#if IS_ENABLED(CONFIG_SEC_KUNIT)
					abc_common_test_get_work_str(uevent_str);
#endif
					kobject_uevent_env(&sec_abc->kobj, KOBJ_CHANGE, uevent_str);
				}
				sec_abc_dequeue(&pgpu_page->buffer, &out);
				ABC_PRINT("cur_time : %lu sec cur_cnt : %d\n", out.cur_time, out.cur_cnt);
			}
		} else if (paicl->buffer.size && !strncasecmp(event_type, "aicl", 4)) { /* AICL fault */
			in.cur_time = (unsigned long)ktime;
			in.cur_cnt = paicl->fail_cnt++;

			ABC_PRINT("aicl fail count : %d\n", paicl->fail_cnt);
			sec_abc_enqueue(&paicl->buffer, in);

			/* Check aicl fault */
			if (paicl->fail_cnt >= paicl->threshold_cnt) {
				if (sec_abc_get_diff_time(&paicl->buffer) < paicl->threshold_time) {
					ABC_PRINT("AICL fault occurred. Send uevent.\n");
#if IS_ENABLED(CONFIG_SEC_KUNIT)
					abc_common_test_get_work_str(uevent_str);
#endif
					kobject_uevent_env(&sec_abc->kobj, KOBJ_CHANGE, uevent_str);
					ABC_PRINT("AFTER AICL UEVENT.\n");
					while (!sec_abc_is_empty(&paicl->buffer))
						sec_abc_dequeue(&paicl->buffer, &out);
					paicl->fail_cnt = 0;
				} else {
					paicl->fail_cnt--;
					sec_abc_dequeue(&paicl->buffer, &out);
					ABC_PRINT("cur_time : %lu sec cur_cnt : %d\n", out.cur_time, out.cur_cnt);
				}
			}
		} else {
			/* Others */
#if IS_ENABLED(CONFIG_SEC_KUNIT)
			abc_common_test_get_work_str(uevent_str);
#endif
			kobject_uevent_env(&sec_abc->kobj, KOBJ_CHANGE, uevent_str);
			ABC_PRINT("Send uevent.\n");
		}
	}
}

/* event string format
 *
 * ex) MODULE=tsp@ERROR=power_status_mismatch
 *	 MODULE=tsp@ERROR=power_status_mismatch@EXT_LOG=fw_ver(0108)
 *
 */
void sec_abc_send_event(char *str)
{
	struct abc_info *pinfo;

	if (!abc_init) {
		ABC_PRINT("ABC driver is not initialized!\n");
#if IS_ENABLED(CONFIG_SEC_KUNIT)
		abc_common_test_get_log_str("ABC driver is not initialized!\n");
#endif
		return;
	}

	if (abc_enabled == ABC_DISABLED) {
		ABC_PRINT("ABC is disabled!\n");
#if IS_ENABLED(CONFIG_SEC_KUNIT)
		abc_common_test_get_log_str("ABC is disabled!\n");
#endif
		return;
	}
	pr_info("%s: %s\n", __func__, str);
	pinfo = dev_get_drvdata(sec_abc);

	strlcpy(pinfo->abc_str, str, sizeof(pinfo->abc_str));
	queue_work(pinfo->workqueue, &pinfo->work);
}
EXPORT_SYMBOL(sec_abc_send_event);

/**
 * sec_abc_wait_enable() - wait for abc enable done
 * Return : 0 for success, -1 for fail(timeout or abc not initialized)
 */
int sec_abc_wait_enabled(void)
{
	struct abc_info *pinfo;
	unsigned long timeout;

	if (!abc_init) {
		ABC_PRINT("ABC driver is not initialized!\n");
		return -1;
	}

	if (abc_enabled)
		return 0;

	pinfo = dev_get_drvdata(sec_abc);

	reinit_completion(&pinfo->enable_done);

	timeout = wait_for_completion_timeout(&pinfo->enable_done,
						  msecs_to_jiffies(ABC_WAIT_ENABLE_TIMEOUT));

	if (timeout == 0) {
		ABC_PRINT("%s : timeout!\n", __func__);
		return -1;
	}

	return 0;
}
EXPORT_SYMBOL(sec_abc_wait_enabled);

static int sec_abc_probe(struct platform_device *pdev)
{
	struct abc_platform_data *pdata;
	struct abc_info *pinfo;
	int ret = 0;

	ABC_PRINT("%s\n", __func__);

	abc_init = false;

	if (pdev->dev.of_node) {
		pdata = devm_kzalloc(&pdev->dev,
					 sizeof(struct abc_platform_data), GFP_KERNEL);

		if (!pdata) {
			dev_err(&pdev->dev, "Failed to allocate platform data\n");
			ret = -ENOMEM;
			goto out;
		}

		pdev->dev.platform_data = pdata;
		ret = abc_parse_dt(&pdev->dev);
		if (ret) {
			dev_err(&pdev->dev, "Failed to parse dt data\n");
			goto err_parse_dt;
		}

		pr_info("%s: parse dt done\n", __func__);
	} else {
		pdata = pdev->dev.platform_data;
	}

	if (!pdata) {
		dev_err(&pdev->dev, "There are no platform data\n");
		ret = -EINVAL;
		goto out;
	}

	pinfo = kzalloc(sizeof(*pinfo), GFP_KERNEL);

	if (!pinfo) {
		ret = -ENOMEM;
		goto err_alloc_pinfo;
	}

#if IS_ENABLED(CONFIG_DRV_SAMSUNG)
	pinfo->dev = sec_device_create(pinfo, "sec_abc");
#else
	pinfo->dev = device_create(sec_class, NULL, 0, NULL, "sec_abc");
#endif
	if (IS_ERR(pinfo->dev)) {
		pr_err("%s Failed to create device(sec_abc)!\n", __func__);
		ret = -ENODEV;
		goto err_create_device;
	}
	sec_abc = pinfo->dev;

	ret = device_create_file(pinfo->dev, &dev_attr_enabled);
	if (ret) {
		pr_err("%s: Failed to create device enabled file\n", __func__);
		goto err_create_abc_enabled_sysfs;
	}

	ret = device_create_file(pinfo->dev, &dev_attr_log);
	if (ret) {
		pr_err("%s: Failed to create device log file\n", __func__);
		goto err_create_abc_log_sysfs;
	}

	INIT_WORK(&pinfo->work, sec_abc_work_func);

	pinfo->workqueue = create_singlethread_workqueue("sec_abc_wq");
	if (!pinfo->workqueue)
		goto err_create_abc_wq;

	INIT_LIST_HEAD(&pinfo->log_list);
	pinfo->log_list_cnt = 0;

	init_completion(&pinfo->enable_done);

	mutex_init(&pinfo->log_mutex);

	pinfo->pdata = pdata;

	platform_set_drvdata(pdev, pinfo);
#if IS_ENABLED(CONFIG_SEC_ABC_MOTTO)
	motto_init(pdev);
#endif
	abc_init = true;
	return ret;
err_create_abc_wq:
	device_remove_file(pinfo->dev, &dev_attr_log);
err_create_abc_log_sysfs:
	device_remove_file(pinfo->dev, &dev_attr_enabled);
err_create_abc_enabled_sysfs:
#if IS_ENABLED(CONFIG_DRV_SAMSUNG)
	sec_device_destroy(sec_abc->devt);
#else
	device_destroy(sec_class, sec_abc->devt);
#endif
err_create_device:
	kfree(pinfo);
err_alloc_pinfo:
err_parse_dt:
	devm_kfree(&pdev->dev, pdata);
	pdev->dev.platform_data = NULL;
out:
	return ret;
}

static struct platform_driver sec_abc_driver = {
	.probe = sec_abc_probe,
	.remove = sec_abc_remove,
	.driver = {
		.name = "sec_abc",
		.owner = THIS_MODULE,
#if IS_ENABLED(CONFIG_PM)
		.pm	= &sec_abc_pm,
#endif
#if IS_ENABLED(CONFIG_OF)
		.of_match_table = of_match_ptr(sec_abc_dt_match),
#endif
	},
};

static int __init sec_abc_init(void)
{
	ABC_PRINT("%s\n", __func__);

	return platform_driver_register(&sec_abc_driver);
}

static void __exit sec_abc_exit(void)
{
	return platform_driver_unregister(&sec_abc_driver);
}

module_init(sec_abc_init);
module_exit(sec_abc_exit);

MODULE_DESCRIPTION("Samsung ABC Driver");
MODULE_AUTHOR("Samsung Electronics");
MODULE_LICENSE("GPL");
