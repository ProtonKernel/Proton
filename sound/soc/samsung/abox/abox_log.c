/* sound/soc/samsung/abox/abox_log.c
 *
 * ALSA SoC Audio Layer - Samsung Abox Log driver
 *
 * Copyright (c) 2016 Samsung Electronics Co. Ltd.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */
/* #define DEBUG */
#include <linux/mutex.h>
#include <linux/vmalloc.h>
#include <sound/samsung/abox.h>

#include "abox_util.h"
#include "abox.h"
#include "abox_proc.h"
#include "abox_log.h"
#include "abox_memlog.h"

#undef VERBOSE_LOG

#undef TEST
#ifdef TEST
#define SIZE_OF_BUFFER (SZ_128)
#else
#define SIZE_OF_BUFFER (SZ_2M)
#endif

#define S_IRWUG (0660)

struct abox_log_kernel_buffer {
	char *buffer;
	unsigned int index;
	bool wrap;
	bool updated;
	wait_queue_head_t wq;
};

struct abox_log_buffer_info {
	struct list_head list;
	struct device *dev;
	int id;
	bool file_created;
	struct mutex lock;
	struct ABOX_LOG_BUFFER *log_buffer;
	struct abox_log_kernel_buffer kernel_buffer;
};

struct abox_log_file_info {
	struct abox_log_buffer_info *info;
	ssize_t index;
};

static LIST_HEAD(abox_log_list_head);

static void abox_log_memcpy(struct device *dev,
	struct abox_log_kernel_buffer *kernel_buffer,
	const char *src, size_t size)
{
	size_t left_size = SIZE_OF_BUFFER - kernel_buffer->index;

	abox_dbg(dev, "%s(%zu)\n", __func__, size);

	if (left_size < size) {
#ifdef VERBOSE_LOG
		abox_dbg(dev, "0: %s\n", src);
#endif
		memcpy(kernel_buffer->buffer + kernel_buffer->index, src,
				left_size);
		src += left_size;
		size -= left_size;
		kernel_buffer->index = 0;
		kernel_buffer->wrap = true;
	}
#ifdef VERBOSE_LOG
	abox_dbg(dev, "1: %s\n", src);
#endif
	memcpy(kernel_buffer->buffer + kernel_buffer->index, src, size);
	kernel_buffer->index += (unsigned int)size;
}

static void abox_log_flush(struct device *dev,
		struct abox_log_buffer_info *info)
{
	struct ABOX_LOG_BUFFER *log_buffer = info->log_buffer;
	unsigned int index_writer = log_buffer->index_writer;
	struct abox_log_kernel_buffer *kernel_buffer = &info->kernel_buffer;

	if (log_buffer->index_reader == index_writer)
		return;

	abox_dbg(dev, "%s(%d): index_writer=%u, index_reader=%u, size=%u\n",
			__func__, info->id, index_writer,
			log_buffer->index_reader, log_buffer->size);

	mutex_lock(&info->lock);

#if IS_ENABLED(CONFIG_SND_SOC_SAMSUNG_AUDIO)
	if (log_buffer->index_reader >= log_buffer->size ||
		index_writer >= log_buffer->size) {
		abox_err(dev, "%s(%pK,%u,%u,%u) index exceeds buffer size.\n",
			__func__, (void *)log_buffer->buffer, log_buffer->index_reader,
			index_writer, log_buffer->size);
		mutex_unlock(&info->lock);
		return;
	}
	abox_log_extra_copy(log_buffer->buffer,
				log_buffer->index_reader, index_writer, log_buffer->size);
#endif

	if (log_buffer->index_reader > index_writer) {
		abox_log_memcpy(info->dev, kernel_buffer,
				log_buffer->buffer + log_buffer->index_reader,
				log_buffer->size - log_buffer->index_reader);
		log_buffer->index_reader = 0;
	}
	abox_log_memcpy(info->dev, kernel_buffer,
			log_buffer->buffer + log_buffer->index_reader,
			index_writer - log_buffer->index_reader);
	log_buffer->index_reader = index_writer;
	mutex_unlock(&info->lock);

	kernel_buffer->updated = true;
	wake_up_interruptible(&kernel_buffer->wq);

#ifdef TEST
	abox_dbg(dev, "shared_buffer: %s\n", log_buffer->buffer);
	abox_dbg(dev, "kernel_buffer: %s\n", info->kernel_buffer.buffer);
#endif
}

void abox_log_flush_all(struct device *dev)
{
	struct abox_log_buffer_info *info;

	abox_dbg(dev, "%s\n", __func__);

	list_for_each_entry(info, &abox_log_list_head, list) {
		abox_log_flush(info->dev, info);
	}
}
EXPORT_SYMBOL(abox_log_flush_all);

static unsigned long abox_log_flush_all_work_rearm_self;
static void abox_log_flush_all_work_func(struct work_struct *work);
static DECLARE_DEFERRABLE_WORK(abox_log_flush_all_work,
		abox_log_flush_all_work_func);

static void abox_log_flush_all_work_func(struct work_struct *work)
{
	abox_log_flush_all(NULL);
	queue_delayed_work(system_unbound_wq, &abox_log_flush_all_work,
			msecs_to_jiffies(1000));
	set_bit(0, &abox_log_flush_all_work_rearm_self);
}

void abox_log_schedule_flush_all(struct device *dev)
{
	if (test_and_clear_bit(0, &abox_log_flush_all_work_rearm_self))
		cancel_delayed_work(&abox_log_flush_all_work);
	queue_delayed_work(system_unbound_wq, &abox_log_flush_all_work,
			msecs_to_jiffies(100));
}
EXPORT_SYMBOL(abox_log_schedule_flush_all);

void abox_log_drain_all(struct device *dev)
{
	cancel_delayed_work(&abox_log_flush_all_work);
	abox_log_flush_all(dev);
}
EXPORT_SYMBOL(abox_log_drain_all);

static int abox_log_file_open(struct inode *inode, struct file *file)
{
	struct abox_log_buffer_info *info = abox_proc_data(file);
	struct abox_log_file_info *finfo;

	abox_dbg(info->dev, "%s\n", __func__);

	finfo = kmalloc(sizeof(*finfo), GFP_KERNEL);
	if (!finfo)
		return -ENOMEM;

	finfo->index = -1;
	finfo->info = info;
	file->private_data = finfo;

	return 0;
}

static int abox_log_file_release(struct inode *inode, struct file *file)
{
	struct abox_log_file_info *finfo = file->private_data;
	struct abox_log_buffer_info *info = finfo->info;

	abox_dbg(info->dev, "%s\n", __func__);
	kfree(finfo);

	return 0;
}

static ssize_t abox_log_file_read(struct file *file, char __user *buf,
		size_t count, loff_t *ppos)
{
	struct abox_log_file_info *finfo = file->private_data;
	struct abox_log_buffer_info *info = finfo->info;
	struct abox_log_kernel_buffer *kernel_buffer = &info->kernel_buffer;
	unsigned int index;
	size_t end, size;
	bool first = (finfo->index < 0);
	int ret;

	abox_dbg(info->dev, "%s(%zu, %lld)\n", __func__, count, *ppos);

	mutex_lock(&info->lock);

	if (first)
		finfo->index = kernel_buffer->wrap ? kernel_buffer->index : 0;

	do {
		index = kernel_buffer->index;
		end = ((finfo->index < index) ||
				((finfo->index == index) && !first)) ?
				index : SIZE_OF_BUFFER;
		size = min(end - finfo->index, count);
		if (size == 0) {
			mutex_unlock(&info->lock);
			if (file->f_flags & O_NONBLOCK) {
				abox_dbg(info->dev, "non block\n");
				return -EAGAIN;
			}
			kernel_buffer->updated = false;

			ret = wait_event_interruptible(kernel_buffer->wq,
					kernel_buffer->updated);
			if (ret != 0) {
				abox_dbg(info->dev, "interrupted\n");
				return ret;
			}
			mutex_lock(&info->lock);
		}
#ifdef VERBOSE_LOG
		abox_dbg(info->dev, "loop %zu, %zu, %zu, %zu\n", size, end,
				finfo->index, count);
#endif
	} while (size == 0);

	abox_dbg(info->dev, "start=%zu, end=%zd size=%zd\n", finfo->index,
			end, size);
	if (copy_to_user(buf, kernel_buffer->buffer + finfo->index, size)) {
		mutex_unlock(&info->lock);
		return -EFAULT;
	}

	finfo->index += size;
	if (finfo->index >= SIZE_OF_BUFFER)
		finfo->index = 0;

	mutex_unlock(&info->lock);

	abox_dbg(info->dev, "%s: size = %zd\n", __func__, size);

	return size;
}

static unsigned int abox_log_file_poll(struct file *file, poll_table *wait)
{
	struct abox_log_file_info *finfo = file->private_data;
	struct abox_log_buffer_info *info = finfo->info;
	struct abox_log_kernel_buffer *kernel_buffer = &info->kernel_buffer;

	abox_dbg(info->dev, "%s\n", __func__);

	poll_wait(file, &kernel_buffer->wq, wait);
	return POLLIN | POLLRDNORM;
}

static const struct file_operations abox_log_fops = {
	.open = abox_log_file_open,
	.release = abox_log_file_release,
	.read = abox_log_file_read,
	.poll = abox_log_file_poll,
	.llseek = generic_file_llseek,
	.owner = THIS_MODULE,
};

static LIST_HEAD(abox_log_register_head);
static DEFINE_SPINLOCK(abox_log_register_lock);

void abox_log_register_buffer_work_func(struct work_struct *work)
{
	struct abox_log_buffer_info *info, *_info, *n;
	char *name;
	unsigned long flags;

	spin_lock_irqsave(&abox_log_register_lock, flags);
	list_for_each_entry_safe(_info, n, &abox_log_register_head, list) {
		if (_info->kernel_buffer.buffer)
			continue;

		abox_info(_info->dev, "%s(%d)\n", __func__, _info->id);

		spin_unlock_irqrestore(&abox_log_register_lock, flags);

		info = devm_kmemdup(_info->dev, _info, sizeof(*_info),
				GFP_KERNEL);
		mutex_init(&info->lock);
		info->file_created = false;
		info->kernel_buffer.buffer = vzalloc(SIZE_OF_BUFFER);
		info->kernel_buffer.index = 0;
		info->kernel_buffer.wrap = false;
		init_waitqueue_head(&info->kernel_buffer.wq);
		spin_lock_irqsave(&abox_log_register_lock, flags);
		list_add_tail(&info->list, &abox_log_list_head);
		list_del(&_info->list);
		spin_unlock_irqrestore(&abox_log_register_lock, flags);
		devm_kfree(_info->dev, _info);

		name = kasprintf(GFP_KERNEL, "log-%02d", info->id);
		abox_proc_create_file(name, 0664, NULL, &abox_log_fops,
				info, 0);
		kfree(name);

		spin_lock_irqsave(&abox_log_register_lock, flags);
	}
	spin_unlock_irqrestore(&abox_log_register_lock, flags);
}

static DECLARE_WORK(abox_log_register_buffer_work,
		abox_log_register_buffer_work_func);

static int abox_log_init(struct device *dev)
{
	static int has_run;

	if (has_run)
		return 0;

	has_run = 1;
	abox_info(dev, "%s\n", __func__);

#ifdef TEST
	abox_log_test_buffer = vzalloc(SZ_128);
	abox_log_test_buffer->size = SZ_64;
	abox_log_register_buffer(NULL, 0, abox_log_test_buffer);
	schedule_delayed_work(&abox_log_test_work, msecs_to_jiffies(1000));
#endif

	return 0;
}

int abox_log_register_buffer(struct device *dev, int id,
		struct ABOX_LOG_BUFFER *buffer)
{
	struct abox_log_buffer_info *info;
	unsigned long flags;

	abox_dbg(dev, "%s(%d)\n", __func__, id);

	abox_log_init(dev);

	spin_lock_irqsave(&abox_log_register_lock, flags);
	list_for_each_entry(info, &abox_log_list_head, list) {
		if (info->id == id) {
			abox_dbg(dev, "registered log: %d\n", id);
			spin_unlock_irqrestore(&abox_log_register_lock, flags);
			return 0;
		}
	}

	info = devm_kzalloc(dev, sizeof(*info), GFP_ATOMIC);
	if (info) {
		info->dev = dev;
		info->id = id;
		info->log_buffer = buffer;
		list_add_tail(&info->list, &abox_log_register_head);
		schedule_work(&abox_log_register_buffer_work);
	}
	spin_unlock_irqrestore(&abox_log_register_lock, flags);

	return 0;
}
EXPORT_SYMBOL(abox_log_register_buffer);

#ifdef TEST
static struct ABOX_LOG_BUFFER *abox_log_test_buffer;
static void abox_log_test_work_func(struct work_struct *work);
DECLARE_DELAYED_WORK(abox_log_test_work, abox_log_test_work_func);
static void abox_log_test_work_func(struct work_struct *work)
{
	struct ABOX_LOG_BUFFER *log = abox_log_test_buffer;
	static unsigned int i;
	char buffer[32];
	char *buffer_index = buffer;
	int size, left;

	pr_debug("%s: %d\n", __func__, i);

	size = snprintf(buffer, sizeof(buffer), "%d ", i++);

	if (log->index_writer + size > log->size) {
		left = log->size - log->index_writer;
		memcpy(&log->buffer[log->index_writer], buffer_index, left);
		log->index_writer = 0;
		buffer_index += left;
	}

	left = size - (buffer_index - buffer);
	memcpy(&log->buffer[log->index_writer], buffer_index, left);
	log->index_writer += left;

	abox_log_flush_all(NULL);

	schedule_delayed_work(&abox_log_test_work, msecs_to_jiffies(1000));
}
#endif
