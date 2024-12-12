/*
 * sec_tsp_log.c
 *
 * driver supporting debug functions for Samsung touch device
 *
 * COPYRIGHT(C) Samsung Electronics Co., Ltd. 2006-2011 All Right Reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
 * more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.
 */

#include "sec_input.h"
#include "sec_tsp_log.h"

static int sec_tsp_log_index;
static int sec_tsp_log_index_fix;
static int sec_tsp_log_index_full;
static char *sec_tsp_log_buf;
static unsigned int sec_tsp_log_size;

#if IS_ENABLED(CONFIG_TOUCHSCREEN_DUAL_FOLDABLE)
static int sec_tsp_raw_data_index_main;
static int sec_tsp_raw_data_index_sub;
static char *sec_tsp_raw_data_buf;
static unsigned int sec_tsp_raw_data_size;
#else
static int sec_tsp_raw_data_index;
static int sec_tsp_raw_data_index_full;
static char *sec_tsp_raw_data_buf;
static unsigned int sec_tsp_raw_data_size;
#endif

static int sec_tsp_command_history_index;
static int sec_tsp_command_history_index_full;
static char *sec_tsp_command_history_buf;
static unsigned int sec_tsp_command_history_size;

/* Sponge Infinite dump */
static int sec_tsp_sponge_log_index;
static int sec_tsp_sponge_log_index_full;
static char *sec_tsp_sponge_log_buf;
static unsigned int sec_tsp_sponge_log_size;

static struct mutex tsp_log_mutex;

void sec_tsp_sponge_log(char *buf)
{
	int len = 0;
	unsigned int idx;
	size_t size, total_size;

	/* In case of sec_tsp_log_setup is failed */
	if (!sec_tsp_sponge_log_size || !sec_tsp_sponge_log_buf)
		return;

	mutex_lock(&tsp_log_mutex);
	idx = sec_tsp_sponge_log_index;
	size = strlen(buf);
	total_size = size + SEC_TSP_LOG_EXTRA_SIZE;

	/* Overflow buffer size */
	if (idx + total_size >= sec_tsp_sponge_log_size) {
		len = scnprintf(&sec_tsp_sponge_log_buf[0], total_size, "%s ", buf);
		sec_tsp_sponge_log_index_full = sec_tsp_sponge_log_index;
		sec_tsp_sponge_log_index = len;
	} else {
		len = scnprintf(&sec_tsp_sponge_log_buf[idx], total_size, "%s ", buf);
		sec_tsp_sponge_log_index += len;
		sec_tsp_sponge_log_index_full = max(sec_tsp_sponge_log_index, sec_tsp_sponge_log_index_full);
	}
	mutex_unlock(&tsp_log_mutex);
}
EXPORT_SYMBOL(sec_tsp_sponge_log);

static ssize_t sec_tsp_sponge_log_read(struct file *file, char __user *buf,
					size_t len, loff_t *offset)
{
	loff_t pos = *offset;
	ssize_t count;

	if (!sec_tsp_sponge_log_buf)
		return 0;

	if (pos >= sec_tsp_sponge_log_index_full)
		return 0;

	count = min_t(size_t, len, sec_tsp_sponge_log_index_full - pos);
	if (copy_to_user(buf, sec_tsp_sponge_log_buf + pos, count))
		return -EFAULT;

	*offset += count;
	return count;
}

static size_t sec_tsp_log_timestamp(unsigned long idx, char *tbuf)
{
	/* Add the current time stamp */
	unsigned long long t;
	unsigned long nanosec_rem;

	t = local_clock();
	nanosec_rem = do_div(t, 1000000000);

	snprintf(tbuf, SEC_TSP_LOG_TIMESTAMP_SIZE, "[%5lu.%06lu] ", (unsigned long)t, nanosec_rem / 1000);

	return strlen(tbuf);
}

#define TSP_BUF_SIZE 512
void sec_debug_tsp_log(char *fmt, ...)
{
	va_list args;
	char buf[TSP_BUF_SIZE];
	char tbuf[SEC_TSP_LOG_TIMESTAMP_SIZE];
	int len = 0;
	unsigned int idx;
	unsigned long size;
	size_t time_size, total_size;

	/* In case of sec_tsp_log_setup is failed */
	if (!sec_tsp_log_size)
		return;

	mutex_lock(&tsp_log_mutex);
	va_start(args, fmt);
	vsnprintf(buf, sizeof(buf), fmt, args);
	va_end(args);

	idx = sec_tsp_log_index;
	size = strlen(buf);
	time_size = sec_tsp_log_timestamp(idx, tbuf);
	total_size = size + time_size + SEC_TSP_LOG_EXTRA_SIZE;

	/* Overflow buffer size */
	if (idx + total_size >= sec_tsp_log_size) {
		if (sec_tsp_log_index_fix + total_size >= sec_tsp_log_size) {
			mutex_unlock(&tsp_log_mutex);
			return;
		}
		len = scnprintf(&sec_tsp_log_buf[sec_tsp_log_index_fix], total_size, "%s%s\n", tbuf, buf);
		sec_tsp_log_index_full = sec_tsp_log_index;
		sec_tsp_log_index = sec_tsp_log_index_fix + len;
	} else {
		len = scnprintf(&sec_tsp_log_buf[idx], total_size, "%s%s\n", tbuf, buf);
		sec_tsp_log_index += len;
		sec_tsp_log_index_full = max(sec_tsp_log_index_full, sec_tsp_log_index);
	}
	mutex_unlock(&tsp_log_mutex);
}
EXPORT_SYMBOL(sec_debug_tsp_log);

#if IS_ENABLED(CONFIG_TOUCHSCREEN_DUAL_FOLDABLE)
void sec_debug_tsp_raw_data(char *fmt, ...)
{
}
EXPORT_SYMBOL(sec_debug_tsp_raw_data);
#else
void sec_debug_tsp_raw_data(char *fmt, ...)
{
	va_list args;
	char buf[TSP_BUF_SIZE];
	char tbuf[SEC_TSP_LOG_TIMESTAMP_SIZE];
	int len = 0;
	unsigned int idx;
	unsigned long size;
	size_t time_size, total_size;

	/* In case of sec_tsp_log_setup is failed */
	if (!sec_tsp_raw_data_size || !sec_tsp_raw_data_buf)
		return;

	mutex_lock(&tsp_log_mutex);
	va_start(args, fmt);
	vsnprintf(buf, sizeof(buf), fmt, args);
	va_end(args);

	idx = sec_tsp_raw_data_index;
	size = strlen(buf);
	time_size = sec_tsp_log_timestamp(idx, tbuf);
	total_size = size + time_size + SEC_TSP_LOG_EXTRA_SIZE;

	/* Overflow buffer size */
	if (idx + total_size >= sec_tsp_raw_data_size) {
		len = scnprintf(&sec_tsp_raw_data_buf[0], total_size, "%s%s\n", tbuf, buf);
		sec_tsp_raw_data_index_full = sec_tsp_raw_data_index;
		sec_tsp_raw_data_index = len;
	} else {
		len = scnprintf(&sec_tsp_raw_data_buf[idx], total_size, "%s%s\n", tbuf, buf);
		sec_tsp_raw_data_index += len;
		sec_tsp_raw_data_index_full = max(sec_tsp_raw_data_index, sec_tsp_raw_data_index_full);
	}
	mutex_unlock(&tsp_log_mutex);
}
EXPORT_SYMBOL(sec_debug_tsp_raw_data);
#endif

void sec_debug_tsp_log_msg(char *msg, char *fmt, ...)
{
	va_list args;
	char buf[TSP_BUF_SIZE];
	char tbuf[SEC_TSP_LOG_TIMESTAMP_SIZE];
	int len = 0;
	unsigned int idx;
	size_t size, size_dev_name, time_size, total_size;

	/* In case of sec_tsp_log_setup is failed */
	if (!sec_tsp_log_size)
		return;

	mutex_lock(&tsp_log_mutex);
	va_start(args, fmt);
	vsnprintf(buf, sizeof(buf), fmt, args);
	va_end(args);

	idx = sec_tsp_log_index;
	size = strlen(buf);
	size_dev_name = strlen(msg);
	time_size = sec_tsp_log_timestamp(idx, tbuf);
	total_size = size + size_dev_name + time_size + SEC_TSP_LOG_EXTRA_SIZE;

	/* Overflow buffer size */
	if (idx + total_size >= sec_tsp_log_size) {
		if (sec_tsp_log_index_fix + total_size >= sec_tsp_log_size) {
			mutex_unlock(&tsp_log_mutex);
			return;
		}
		len = scnprintf(&sec_tsp_log_buf[sec_tsp_log_index_fix], total_size, "%s%s : %s", tbuf, msg, buf);
		sec_tsp_log_index_full = sec_tsp_log_index;
		sec_tsp_log_index = sec_tsp_log_index_fix + len;
	} else {
		len = scnprintf(&sec_tsp_log_buf[idx], total_size, "%s%s : %s", tbuf, msg, buf);
		sec_tsp_log_index += len;
		sec_tsp_log_index_full = max(sec_tsp_log_index_full, sec_tsp_log_index);
	}
	mutex_unlock(&tsp_log_mutex);
}
EXPORT_SYMBOL(sec_debug_tsp_log_msg);

#if IS_ENABLED(CONFIG_TOUCHSCREEN_DUAL_FOLDABLE)
void sec_debug_tsp_raw_data_msg(char mode, char *msg, char *fmt, ...)
{
	va_list args;
	char buf[TSP_BUF_SIZE];
	char tbuf[SEC_TSP_LOG_TIMESTAMP_SIZE];
	int len = 0;
	unsigned int idx;
	size_t size, size_dev_name, time_size, total_size;

	/* In case of sec_tsp_log_setup is failed */
	if (!sec_tsp_raw_data_size || !sec_tsp_raw_data_buf)
		return;

	mutex_lock(&tsp_log_mutex);
	va_start(args, fmt);
	vsnprintf(buf, sizeof(buf), fmt, args);
	va_end(args);

	if (mode == MAIN_TOUCH) {
		idx = sec_tsp_raw_data_index_main;
	} else if (mode == SUB_TOUCH) {
		idx = sec_tsp_raw_data_index_sub;
	} else {
		mutex_unlock(&tsp_log_mutex);
		return;
	}

	size = strlen(buf);
	size_dev_name = strlen(msg);
	time_size = sec_tsp_log_timestamp(idx, tbuf);
	total_size = size + size_dev_name + time_size + SEC_TSP_LOG_EXTRA_SIZE;

	if (mode == MAIN_TOUCH) {
		/* Overflow buffer size */
		if (idx + total_size >= (sec_tsp_raw_data_size / 2)) {
			len = scnprintf(&sec_tsp_raw_data_buf[0], total_size, "%s%s : %s", tbuf, msg, buf);
			sec_tsp_raw_data_index_main = len;
		} else {
			len = scnprintf(&sec_tsp_raw_data_buf[idx], total_size, "%s%s : %s", tbuf, msg, buf);
			sec_tsp_raw_data_index_main += len;
		}
	} else if (mode == SUB_TOUCH) {
		/* Overflow buffer size */
		if (idx + total_size >= sec_tsp_raw_data_size) {
			len = scnprintf(&sec_tsp_raw_data_buf[sec_tsp_raw_data_size / 2],
					total_size, "%s%s : %s", tbuf, msg, buf);
			sec_tsp_raw_data_index_sub = sec_tsp_raw_data_size / 2 + len;
		} else {
			len = scnprintf(&sec_tsp_raw_data_buf[idx], total_size, "%s%s : %s", tbuf, msg, buf);
			sec_tsp_raw_data_index_sub += len;
		}
	}
	mutex_unlock(&tsp_log_mutex);
}
EXPORT_SYMBOL(sec_debug_tsp_raw_data_msg);
#else
void sec_debug_tsp_raw_data_msg(char *msg, char *fmt, ...)
{
	va_list args;
	char buf[TSP_BUF_SIZE];
	char tbuf[SEC_TSP_LOG_TIMESTAMP_SIZE];
	int len = 0;
	unsigned int idx;
	size_t size, size_dev_name, time_size, total_size;

	/* In case of sec_tsp_log_setup is failed */
	if (!sec_tsp_raw_data_size || !sec_tsp_raw_data_buf)
		return;

	mutex_lock(&tsp_log_mutex);
	va_start(args, fmt);
	vsnprintf(buf, sizeof(buf), fmt, args);
	va_end(args);

	idx = sec_tsp_raw_data_index;
	size = strlen(buf);
	size_dev_name = strlen(msg);
	time_size = sec_tsp_log_timestamp(idx, tbuf);
	total_size = size + size_dev_name + time_size + SEC_TSP_LOG_EXTRA_SIZE;

	/* Overflow buffer size */
	if (idx + total_size >= sec_tsp_raw_data_size) {
		len = scnprintf(&sec_tsp_raw_data_buf[0], total_size, "%s%s : %s", tbuf, msg, buf);
		sec_tsp_raw_data_index_full = sec_tsp_raw_data_index;
		sec_tsp_raw_data_index = len;
	} else {
		len = scnprintf(&sec_tsp_raw_data_buf[idx], total_size, "%s%s : %s", tbuf, msg, buf);
		sec_tsp_raw_data_index += len;
		sec_tsp_raw_data_index_full = max(sec_tsp_raw_data_index, sec_tsp_raw_data_index_full);
	}
	mutex_unlock(&tsp_log_mutex);
}
EXPORT_SYMBOL(sec_debug_tsp_raw_data_msg);
#endif

void sec_debug_tsp_command_history(char *buf)
{
	int len = 0;
	unsigned int idx;
	size_t size, total_size;

	/* In case of sec_tsp_log_setup is failed */
	if (!sec_tsp_command_history_size || !sec_tsp_command_history_buf)
		return;

	mutex_lock(&tsp_log_mutex);
	idx = sec_tsp_command_history_index;
	size = strlen(buf);
	total_size = size + SEC_TSP_LOG_EXTRA_SIZE;

	/* Overflow buffer size */
	if (idx + total_size >= sec_tsp_command_history_size) {
		len = scnprintf(&sec_tsp_command_history_buf[0], total_size, "%s ", buf);
		sec_tsp_command_history_index_full = sec_tsp_command_history_index;
		sec_tsp_command_history_index = len;
	} else {
		len = scnprintf(&sec_tsp_command_history_buf[idx], total_size, "%s ", buf);
		sec_tsp_command_history_index += len;
		sec_tsp_command_history_index_full = max(sec_tsp_command_history_index,
				sec_tsp_command_history_index_full);
	}
	mutex_unlock(&tsp_log_mutex);
}
EXPORT_SYMBOL(sec_debug_tsp_command_history);

#if IS_ENABLED(CONFIG_TOUCHSCREEN_DUAL_FOLDABLE)
void sec_tsp_raw_data_clear(char mode)
{
	if (!sec_tsp_raw_data_size || !sec_tsp_raw_data_buf)
		return;

	mutex_lock(&tsp_log_mutex);
	if (mode == MAIN_TOUCH) {
		sec_tsp_raw_data_index_main = 0;
		memset(sec_tsp_raw_data_buf, 0x00, sec_tsp_raw_data_size / 2);
	} else if (mode == SUB_TOUCH) {
		sec_tsp_raw_data_index_sub = sec_tsp_raw_data_size / 2;
		memset(sec_tsp_raw_data_buf + sec_tsp_raw_data_index_sub, 0x00, sec_tsp_raw_data_size / 2);
	}
	mutex_unlock(&tsp_log_mutex);
}
EXPORT_SYMBOL(sec_tsp_raw_data_clear);
#else
void sec_tsp_raw_data_clear(void)
{
	if (!sec_tsp_raw_data_size || !sec_tsp_raw_data_buf)
		return;

	mutex_lock(&tsp_log_mutex);
	sec_tsp_raw_data_index = 0;
	sec_tsp_raw_data_index_full = 0;
	memset(sec_tsp_raw_data_buf, 0x00, sec_tsp_raw_data_size);
	mutex_unlock(&tsp_log_mutex);
}
EXPORT_SYMBOL(sec_tsp_raw_data_clear);
#endif

void sec_tsp_log_fix(void)
{
	char *buf = "FIX LOG!\n";
	char tbuf[SEC_TSP_LOG_TIMESTAMP_SIZE];
	int len = 0;
	unsigned int idx;
	size_t size, time_size, total_size;

	/* In case of sec_tsp_log_setup is failed */
	if (!sec_tsp_log_size)
		return;

	mutex_lock(&tsp_log_mutex);
	idx = sec_tsp_log_index;
	size = strlen(buf);
	time_size = sec_tsp_log_timestamp(idx, tbuf);
	total_size = size + time_size + SEC_TSP_LOG_EXTRA_SIZE;

	/* Overflow buffer size */
	if (idx + total_size >= sec_tsp_log_size) {
		if (sec_tsp_log_index_fix + total_size >= sec_tsp_log_size) {
			mutex_unlock(&tsp_log_mutex);
			return;
		}
		len = scnprintf(&sec_tsp_log_buf[sec_tsp_log_index_fix], total_size, "%s%s", tbuf, buf);
		sec_tsp_log_index = sec_tsp_log_index_fix + len;
	} else {
		len = scnprintf(&sec_tsp_log_buf[idx], total_size, "%s%s", tbuf, buf);
		sec_tsp_log_index += len;
	}
	sec_tsp_log_index_fix = sec_tsp_log_index;
	sec_tsp_log_index_full = max(sec_tsp_log_index_full, sec_tsp_log_index);
	mutex_unlock(&tsp_log_mutex);
}
EXPORT_SYMBOL(sec_tsp_log_fix);

static ssize_t sec_tsp_log_write(struct file *file,
				const char __user *buf,
				size_t count, loff_t *ppos)
{
	char *page = NULL;
	ssize_t ret;
	int new_value;

	if (!sec_tsp_log_buf)
		return 0;

	ret = -EINVAL;
	if (count >= PAGE_SIZE)
		return ret;

	ret = -ENOMEM;
	page = (char *)get_zeroed_page(GFP_KERNEL | __GFP_COMP);
	if (!page)
		return ret;

	ret = -EFAULT;
	if (copy_from_user(page, buf, count))
		goto out;

	ret = -EINVAL;
	if (sscanf(page, "%u", &new_value) != 1) {
		pr_info("%s %s\n", SECLOG, page);
		/* print tsp_log to sec_tsp_log_buf */
		sec_debug_tsp_log("%s", page);
	}
	ret = count;
out:
	free_page((unsigned long)page);
	return ret;
}

static ssize_t sec_tsp_raw_data_write(struct file *file,
				const char __user *buf,
				size_t count, loff_t *ppos)
{
	char *page = NULL;
	ssize_t ret;
	int new_value;

	if (!sec_tsp_raw_data_buf)
		return 0;

	ret = -EINVAL;
	if (count >= PAGE_SIZE)
		return ret;

	ret = -ENOMEM;
	page = (char *)get_zeroed_page(GFP_KERNEL | __GFP_COMP);
	if (!page)
		return ret;

	ret = -EFAULT;
	if (copy_from_user(page, buf, count))
		goto out;

	ret = -EINVAL;
	if (sscanf(page, "%u", &new_value) != 1) {
		pr_info("%s %s\n", SECLOG, page);
		sec_debug_tsp_raw_data("%s", page);
	}
	ret = count;
out:
	free_page((unsigned long)page);
	return ret;
}

static ssize_t sec_tsp_log_read(struct file *file, char __user *buf,
					size_t len, loff_t *offset)
{
	loff_t pos = *offset;
	ssize_t count;

	if (!sec_tsp_log_buf)
		return 0;

	if (pos >= sec_tsp_log_index_full)
		return 0;

	count = min_t(size_t, len, sec_tsp_log_index_full - pos);
	if (copy_to_user(buf, sec_tsp_log_buf + pos, count))
		return -EFAULT;

	*offset += count;
	return count;
}

#if IS_ENABLED(CONFIG_TOUCHSCREEN_DUAL_FOLDABLE)
static ssize_t sec_tsp_raw_data_read(struct file *file, char __user *buf,
					size_t len, loff_t *offset)
{
	loff_t pos = *offset;
	ssize_t count;
	int pos_range;

	if (!sec_tsp_raw_data_buf)
		return 0;

	pos_range = sec_tsp_raw_data_index_main + (sec_tsp_raw_data_index_sub - (sec_tsp_raw_data_size / 2));
	if (pos_range < 0)
		return 0;
	if (pos >= pos_range)
		return 0;

	if (pos < sec_tsp_raw_data_index_main) {
		count = min_t(size_t, len, sec_tsp_raw_data_index_main - pos);
		if (copy_to_user(buf, sec_tsp_raw_data_buf + pos, count))
			return -EFAULT;
	} else {
		count = min_t(size_t, len, sec_tsp_raw_data_index_sub - (sec_tsp_raw_data_size / 2)
						- (pos - sec_tsp_raw_data_index_main));
		if (copy_to_user(buf, &sec_tsp_raw_data_buf[sec_tsp_raw_data_size / 2] + (pos - sec_tsp_raw_data_index_main), count))
			return -EFAULT;
	}

	*offset += count;
	return count;
}
#else
static ssize_t sec_tsp_raw_data_read(struct file *file, char __user *buf,
					size_t len, loff_t *offset)
{
	loff_t pos = *offset;
	ssize_t count;

	if (!sec_tsp_raw_data_buf)
		return 0;

	if (pos >= sec_tsp_raw_data_index_full)
		return 0;

	count = min_t(size_t, len, sec_tsp_raw_data_index_full - pos);
	if (copy_to_user(buf, sec_tsp_raw_data_buf + pos, count))
		return -EFAULT;

	*offset += count;
	return count;
}
#endif

static ssize_t sec_tsp_command_history_read(struct file *file, char __user *buf,
					size_t len, loff_t *offset)
{
	loff_t pos = *offset;
	ssize_t count;

	if (!sec_tsp_command_history_buf)
		return 0;

	if (pos >= sec_tsp_command_history_index_full)
		return 0;

	count = min_t(size_t, len, sec_tsp_command_history_index_full - pos);
	if (copy_to_user(buf, sec_tsp_command_history_buf + pos, count))
		return -EFAULT;

	*offset += count;
	return count;
}

static sec_input_proc_ops(THIS_MODULE, tsp_msg_file_ops, sec_tsp_log_read, sec_tsp_log_write);
static sec_input_proc_ops(THIS_MODULE, tsp_raw_data_file_ops, sec_tsp_raw_data_read, sec_tsp_raw_data_write);
static sec_input_proc_ops(THIS_MODULE, tsp_command_history_file_ops, sec_tsp_command_history_read, NULL);
static sec_input_proc_ops(THIS_MODULE, tsp_sponge_log_file_ops, sec_tsp_sponge_log_read, NULL);

static int __init sec_tsp_log_init(void)
{
	struct proc_dir_entry *entry_tsp_msg, *entry_tsp_raw_data, *entry_tsp_cmd_hist, *entry_tsp_sponge_log;
	char *vaddr_tsp_log, *vaddr_tsp_raw_data, *vaddr_tsp_command_history, *vaddr_tsp_sponge_log;

	pr_info("%s %s: init start\n", SECLOG, __func__);

	mutex_init(&tsp_log_mutex);

	sec_tsp_log_size = SEC_TSP_LOG_BUF_SIZE;
	vaddr_tsp_log = kmalloc(sec_tsp_log_size, GFP_KERNEL | __GFP_COMP);
	if (!vaddr_tsp_log) {
		pr_info("%s %s: ERROR! vaddr_tsp_log alloc failed!\n", SECLOG, __func__);
	}
	sec_tsp_log_buf = vaddr_tsp_log;

	sec_tsp_raw_data_size = SEC_TSP_RAW_DATA_BUF_SIZE;
	vaddr_tsp_raw_data = kmalloc(sec_tsp_raw_data_size, GFP_KERNEL | __GFP_COMP);
	if (!vaddr_tsp_raw_data) {
		pr_info("%s %s: ERROR! vaddr_tsp_raw_data alloc failed!\n", SECLOG, __func__);
	}
	sec_tsp_raw_data_buf = vaddr_tsp_raw_data;

#if IS_ENABLED(CONFIG_TOUCHSCREEN_DUAL_FOLDABLE)
	sec_tsp_raw_data_index_sub = (sec_tsp_raw_data_size / 2);
#endif

	sec_tsp_command_history_size = SEC_TSP_COMMAND_HISTORY_BUF_SIZE;
	vaddr_tsp_command_history = kmalloc(sec_tsp_command_history_size, GFP_KERNEL | __GFP_COMP);
	if (!vaddr_tsp_command_history) {
		pr_info("%s %s: ERROR! vaddr_tsp_command_history alloc failed!\n", SECLOG, __func__);
	}
	sec_tsp_command_history_buf = vaddr_tsp_command_history;

	sec_tsp_sponge_log_size = SEC_TSP_SPONGE_LOG_BUF_SIZE;
	vaddr_tsp_sponge_log = kmalloc(sec_tsp_sponge_log_size, GFP_KERNEL | __GFP_COMP);
	if (!vaddr_tsp_sponge_log) {
		pr_info("%s %s: ERROR! vaddr_tsp_sponge_log alloc failed!\n", SECLOG, __func__);
	}
	sec_tsp_sponge_log_buf = vaddr_tsp_sponge_log;

	if (sec_tsp_log_buf) {
		entry_tsp_msg = proc_create("tsp_msg", S_IFREG | S_IRUSR | S_IRGRP,
				NULL, &tsp_msg_file_ops);
		if (!entry_tsp_msg)
			pr_err("%s %s: failed to create proc entry of tsp_msg\n", SECLOG, __func__);
	}

	if (sec_tsp_raw_data_buf) {
		entry_tsp_raw_data = proc_create("tsp_raw_data", S_IFREG | 0444,
				NULL, &tsp_raw_data_file_ops);
		if (!entry_tsp_raw_data)
			pr_err("%s %s: failed to create proc entry of tsp_raw_data\n", SECLOG, __func__);
	}

	if (sec_tsp_command_history_buf) {
		entry_tsp_cmd_hist = proc_create("tsp_cmd_hist", S_IFREG | 0444,
				NULL, &tsp_command_history_file_ops);
		if (!entry_tsp_cmd_hist)
			pr_err("%s %s: failed to create proc entry of tsp_command_history\n", SECLOG, __func__);
	}

	if (sec_tsp_sponge_log_buf) {
		entry_tsp_sponge_log = proc_create("tsp_sponge_log", S_IFREG | 0444,
				NULL, &tsp_sponge_log_file_ops);
		if (!entry_tsp_sponge_log)
			pr_err("%s %s: failed to create proc entry of tsp_sponge_log\n", SECLOG, __func__);
	}

	pr_info("%s %s: init done\n", SECLOG, __func__);

	return 0;
}
fs_initcall(sec_tsp_log_init);	/* earlier than device_initcall */

MODULE_DESCRIPTION("Samsung debug sysfs for Input module");
MODULE_LICENSE("GPL");
