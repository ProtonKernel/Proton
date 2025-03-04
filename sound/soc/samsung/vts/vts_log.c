// SPDX-License-Identifier: GPL-2.0-or-later
/* sound/soc/samsung/vts/vts_log.c
 *
 * ALSA SoC - Samsung VTS Log driver
 *
 * Copyright (c) 2017 Samsung Electronics Co. Ltd.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */
#include <linux/module.h>
#include <linux/io.h>
#include <linux/debugfs.h>
#include <linux/mutex.h>
#include <sound/soc.h>
#include <linux/pm_runtime.h>

#include "vts.h"
#include "vts_dbg.h"
#include "vts_log.h"
#include "vts_proc.h"

#define VTS_LOG_BUFFER_SIZE (SZ_1M)
#define S_IRWUG (0660)

struct vts_kernel_log_buffer {
	char *buffer;
	unsigned int index;
	bool wrap;
	bool updated;
	wait_queue_head_t wq;
};

struct vts_log_buffer_info {
	struct device *dev;
	struct mutex lock;
	struct vts_log_buffer log_buffer;
	struct vts_kernel_log_buffer kernel_buffer;
	bool registered;
	u32 logbuf_index;
};

static struct vts_log_buffer_info glogbuf_info;

static ssize_t vts_log_file_index;

static int vts_log_file_open(struct inode *inode, struct  file *file)
{
	struct vts_log_buffer_info *info = vts_proc_data(file);
	struct vts_data *data = dev_get_drvdata(info->dev);
	u32 values[3] = {0, 0, 0};
	int result = 0;

	vts_dev_info(info->dev, "%s\n", __func__);

	file->private_data = info;
	vts_log_file_index = -1;

	if (data->vts_ready) {
		values[0] = VTS_ENABLE_DEBUGLOG;
		values[1] = 0;
		values[2] = 0;
		result = vts_start_ipc_transaction(
			info->dev, data, VTS_IRQ_AP_TEST_COMMAND,
			&values, 0, 1);
		if (result < 0)
			vts_dev_err(info->dev, "%s Enable_debuglog ipc transaction failed\n",
				__func__);
	}

	data->fw_logfile_enabled = 1;
	return 0;
}

static int vts_log_file_close(struct inode *inode, struct  file *file)
{
	struct vts_log_buffer_info *info = vts_proc_data(file);
	struct vts_data *data = dev_get_drvdata(info->dev);
	u32 values[3] = {0, 0, 0};
	int result = 0;

	vts_dev_info(info->dev, "%s\n", __func__);

	vts_log_file_index = -1;

	if (data->vts_ready) {
		values[0] = VTS_DISABLE_DEBUGLOG;
		values[1] = 0;
		values[2] = 0;
		result = vts_start_ipc_transaction(
			info->dev, data,
			VTS_IRQ_AP_TEST_COMMAND, &values, 0, 1);
		if (result < 0)
			vts_dev_err(info->dev,
			"%s Disable_debuglog ipc transaction failed\n",
			__func__);
		/* reset VTS SRAM debug log buffer */
		vts_register_log_buffer(info->dev, 0, 0);
	}

	data->fw_logfile_enabled = 0;
	return 0;
}

static ssize_t vts_log_file_read(
	struct file *file,
	char __user *buf,
	size_t count,
	loff_t *ppos)
{
	struct vts_log_buffer_info *info = file->private_data;
	struct vts_kernel_log_buffer *kernel_buffer = &info->kernel_buffer;
	struct device *dev = info->dev;
	struct vts_data *vts_data = dev_get_drvdata(dev);
	unsigned int index;
	size_t end, size;
	bool first = (vts_log_file_index < 0);
	int result;

	vts_dev_dbg(dev, "%s(%zu, %lld)\n", __func__, count, *ppos);

	mutex_lock(&info->lock);

	if (first)
		vts_log_file_index = likely(kernel_buffer->wrap) ?
						kernel_buffer->index : 0;

	do {
		index = kernel_buffer->index;
		end = ((vts_log_file_index < index) ||
				((vts_log_file_index == index)/* && !first*/)) ?
				index : VTS_LOG_BUFFER_SIZE;
		size = min(end - vts_log_file_index, count);
		if (size == 0 || (first && !kernel_buffer->wrap)
			|| (size > glogbuf_info.log_buffer.size)) {
			mutex_unlock(&info->lock);
			if (file->f_flags & O_NONBLOCK) {
				vts_dev_dbg(dev, "non block\n");
				return -EAGAIN;
			}
			kernel_buffer->updated = false;

			result = wait_event_interruptible(kernel_buffer->wq,
					kernel_buffer->updated);

			if (result != 0) {
				vts_dev_dbg(dev, "interrupted\n");
				return result;
			}
			mutex_lock(&info->lock);
		}
#ifdef VERBOSE_LOG
		vts_dev_dbg(dev, "loop %zu, %zu, %zd, %zu\n",
			size, end, vts_log_file_index, count);
#endif
	} while (size == 0);

	vts_dev_dbg(dev, "start=%zd, end=%zd size=%zd\n",
		vts_log_file_index, end, size);

	if (vts_data->running) {
		if (copy_to_user(buf,
			kernel_buffer->buffer + vts_log_file_index, size)) {
			mutex_unlock(&info->lock);
			return -EFAULT;
		}
	} else {
		vts_dev_err(dev, "%s : Wrong VTS status", __func__);
	}

	vts_log_file_index += size;
	if (vts_log_file_index >= VTS_LOG_BUFFER_SIZE)
		vts_log_file_index = 0;

	mutex_unlock(&info->lock);

	vts_dev_dbg(dev, "%s: size = %zd\n", __func__, size);

	return size;
}

static unsigned int vts_log_file_poll(struct file *file, poll_table *wait)
{
	struct vts_log_buffer_info *info = file->private_data;
	struct vts_kernel_log_buffer *kernel_buffer = &info->kernel_buffer;

	vts_dev_dbg(info->dev, "%s\n", __func__);

	poll_wait(file, &kernel_buffer->wq, wait);
	return POLLIN | POLLRDNORM;
}

static const struct file_operations vts_log_fops = {
	.open = vts_log_file_open,
	.release = vts_log_file_close,
	.read = vts_log_file_read,
	.poll = vts_log_file_poll,
	.llseek = generic_file_llseek,
	.owner = THIS_MODULE,
};

static void vts_log_memcpy(struct device *dev,
	struct vts_kernel_log_buffer *kernel_buffer,
	 char *src, size_t size)
{
	size_t left_size = 0;
	struct vts_data *vts_data = dev_get_drvdata(dev);

	if (!vts_data->running && size) {
		dev_err(dev, "%s : Wrong Status & Value", __func__);
		return;
	}

	left_size = VTS_LOG_BUFFER_SIZE - kernel_buffer->index;

	vts_dev_dbg(dev, "%s(0x%x 0x%x)\n", __func__, left_size, size);

	if (left_size < size) {
#ifdef VERBOSE_LOG
		vts_dev_dbg(dev, "0: %s\n", src);
#endif
		memcpy_fromio(kernel_buffer->buffer +
			kernel_buffer->index, src, left_size);
		src += left_size;
		size -= left_size;
		kernel_buffer->index = 0;
		kernel_buffer->wrap = true;
	}
#ifdef VERBOSE_LOG
	vts_dev_dbg(dev, "1: %s\n", src);
#endif
	memcpy_fromio(kernel_buffer->buffer + kernel_buffer->index, src, size);
	kernel_buffer->index += (unsigned int)size;
}

static void vts_log_flush_work_func(struct work_struct *work)
{
	struct device *dev = glogbuf_info.dev;
	struct vts_log_buffer *log_buffer = &glogbuf_info.log_buffer;
	struct vts_kernel_log_buffer *kernel_buffer = NULL;
	struct vts_data *data = dev_get_drvdata(dev);
	int logbuf_index = glogbuf_info.logbuf_index;
	unsigned int index_writer;

	kernel_buffer = &glogbuf_info.kernel_buffer;

	vts_dev_dbg(dev, "%s: %d %d %d %d\n", __func__, __LINE__,
		logbuf_index, data->running, log_buffer->size);

	if (!(data->running) || !(log_buffer->size))
		return;

	log_buffer->index_writer = data->shared_info->log_pos_write;
	log_buffer->index_reader = data->shared_info->log_pos_read;
	index_writer = log_buffer->index_writer;

	vts_dev_dbg(dev, "%s: %d 0x%x 0x%x\n",
		__func__, __LINE__,
		log_buffer->index_writer, log_buffer->index_reader);

	if (log_buffer->index_reader == index_writer)
		return;

	if (data->fw_logfile_enabled) {
		pm_runtime_get_sync(dev);
		vts_start_runtime_resume(dev, 1);
		mutex_lock(&glogbuf_info.lock);

		if (log_buffer->index_reader > index_writer) {
			vts_dev_dbg(dev, "%s1: %d 0x%x 0x%x 0x%x\n",
				__func__, __LINE__,
				log_buffer->index_writer,
				log_buffer->index_reader,
				log_buffer->size);

			if (log_buffer->index_reader > log_buffer->size * 2) {
				vts_dev_info(dev, "%s to check 0x%x > 0x%x\n",
					__func__,
					log_buffer->index_reader,
					log_buffer->size * 2);

			} else {
				vts_log_memcpy(dev, kernel_buffer,
					log_buffer->addr + log_buffer->index_reader,
					(log_buffer->size * 2) - log_buffer->index_reader);
			}
			log_buffer->index_reader = 0;
		}

		vts_dev_dbg(dev, "%s2: %d 0x%x 0x%x 0x%x\n",
			__func__, __LINE__,
			log_buffer->index_writer,
			log_buffer->index_reader,
			log_buffer->size);

		vts_log_memcpy(dev, kernel_buffer,
			log_buffer->addr + log_buffer->index_reader,
			index_writer - log_buffer->index_reader);
		data->shared_info->log_pos_read =
			data->shared_info->log_pos_write;
		log_buffer->index_reader = index_writer;

		wmb(); /* memory barrier */
		mutex_unlock(&glogbuf_info.lock);
		kernel_buffer->updated = true;
		pm_runtime_put_sync(dev);
		wake_up_interruptible(&kernel_buffer->wq);
	}

	if (data->fw_logger_enabled && data->fw_log_obj) {
		pm_runtime_get_sync(dev);
		memlog_write(data->fw_log_obj, MEMLOG_LEVEL_INFO,
			(log_buffer->addr+log_buffer->size*logbuf_index),
			log_buffer->size);
		vts_dev_dbg(dev, "fw log sync : %d",
			memlog_sync_to_file(data->fw_log_obj));
		pm_runtime_put_sync(dev);
	}
}

static DECLARE_WORK(vts_log_work, vts_log_flush_work_func);
void vts_log_schedule_flush(struct device *dev, u32 index)
{
	if (glogbuf_info.registered &&
		glogbuf_info.log_buffer.size) {
		glogbuf_info.logbuf_index = index;
		schedule_work(&vts_log_work);
		vts_dev_dbg(dev, "%s: VTS Log Buffer[%d] Scheduled\n",
			 __func__, index);
	} else
		vts_dev_warn(dev, "%s: VTS Debugging buffer not registered\n",
				 __func__);
}
EXPORT_SYMBOL(vts_log_schedule_flush);

int vts_register_log_buffer(
	struct device *dev,
	u32 addroffset,
	u32 logsz)
{
	struct vts_data *data = dev_get_drvdata(dev);

	vts_dev_dbg(dev, "%s(offset 0x%x)\n", __func__, addroffset);

	if ((addroffset + logsz) > data->sram_size) {
		vts_dev_warn(dev, "%s: wrong offset[0x%x] or size[0x%x]\n",
				__func__, addroffset, logsz);
		return -EINVAL;
	}

	if (!glogbuf_info.registered) {
		glogbuf_info.dev = dev;
		mutex_init(&glogbuf_info.lock);
		glogbuf_info.kernel_buffer.buffer =
				vzalloc(VTS_LOG_BUFFER_SIZE);
		glogbuf_info.kernel_buffer.index = 0;
		glogbuf_info.kernel_buffer.wrap = false;
		init_waitqueue_head(&glogbuf_info.kernel_buffer.wq);

		vts_proc_create_file("vts-log", 0664, NULL, &vts_log_fops, &glogbuf_info, 0);
		glogbuf_info.registered = true;
	}

	/* Update logging buffer address and size info */
	glogbuf_info.log_buffer.addr = data->sram_base + addroffset;
	glogbuf_info.log_buffer.size = logsz;

	vts_dev_info(dev, "%s: %d 0x%x 0x%x\n",
		__func__, __LINE__,
		addroffset,
		glogbuf_info.log_buffer.size);

	return 0;
}
EXPORT_SYMBOL(vts_register_log_buffer);

void vts_log_flush(struct device *dev)
{
	vts_dev_info(dev, "%s\n", __func__);

	vts_log_flush_work_func(NULL);
}
EXPORT_SYMBOL(vts_log_flush);
