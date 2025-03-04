/* sound/soc/samsung/abox/abox_dump.c
 *
 * ALSA SoC Audio Layer - Samsung Abox Internal Buffer Dumping driver
 *
 * Copyright (c) 2016 Samsung Electronics Co. Ltd.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#include <linux/module.h>
#include <linux/debugfs.h>
#include <linux/spinlock.h>
#include <linux/vmalloc.h>
#include <linux/pm_runtime.h>
#include <linux/sched/clock.h>
#include <sound/samsung/abox.h>

#include "abox_util.h"
#include "abox.h"
#include "abox_proc.h"
#include "abox_log.h"
#include "abox_dump.h"
#include "abox_memlog.h"

#define NAME_LENGTH (SZ_32)

struct abox_dump_info {
	struct device *dev;
	struct list_head list;
	int c_gid;
	int c_id;
	int id;
	char name[NAME_LENGTH];
	bool registered;
	struct mutex lock;
	struct snd_dma_buffer buffer;
	struct snd_pcm_substream *substream;
	size_t pointer;
	bool started;

	struct proc_dir_entry *file;
	bool file_started;
	size_t file_pointer;
	wait_queue_head_t file_waitqueue;

	bool auto_started;
	ssize_t auto_pointer;
	struct memlog_obj *auto_fobj;
	struct work_struct auto_work;

	bool memlog_started;
	ssize_t memlog_pointer;
	struct work_struct memlog_work;
	int memlog_idx;
};

static struct proc_dir_entry *dir_dump;
static struct device *abox_dump_dev_abox;
static LIST_HEAD(abox_dump_list_head);
static DEFINE_SPINLOCK(abox_dump_lock);

static struct abox_dump_info *abox_dump_get_info(int id)
{
	struct abox_dump_info *info;

	list_for_each_entry(info, &abox_dump_list_head, list) {
		if (info->id == id)
			return info;
	}

	return NULL;
}

static struct abox_dump_info *abox_dump_get_info_by_name(const char *name)
{
	struct abox_dump_info *info;

	list_for_each_entry(info, &abox_dump_list_head, list) {
		if (strncmp(info->name, name, sizeof(info->name)) == 0)
			return info;
	}

	return NULL;
}

static void abox_dump_request_dump(int id)
{
	struct abox_dump_info *info = abox_dump_get_info(id);
	ABOX_IPC_MSG msg;
	struct IPC_SYSTEM_MSG *system = &msg.msg.system;
	bool start = info->started || info->file_started ||
			info->auto_started || info->memlog_started;

	abox_dbg(info->dev, "%s(%d)\n", __func__, id);

	msg.ipcid = IPC_SYSTEM;
	system->msgtype = ABOX_REQUEST_DUMP;
	system->param1 = info->c_id;
	system->param2 = start ? 1 : 0;
	system->param3 = info->c_gid;
	abox_request_ipc(abox_dump_dev_abox, msg.ipcid, &msg, sizeof(msg),
			1, 0);
}

struct abox_dump_memlog_entity {
	const char *name;
	struct memlog_obj *fobj;
	size_t buf_size;
};

static struct abox_dump_memlog_entity abox_dump_memlogs[] = {
	{ .name = "rd0", .buf_size = SZ_16M, },
	{ .name = "rd1", .buf_size = SZ_16M, },
	{ .name = "rd3", .buf_size = SZ_16M, },
	{ .name = "rd4", .buf_size = SZ_16M, },
	{ .name = "rd7", .buf_size = SZ_16M, },
	{ .name = "rdb", .buf_size = SZ_16M, },
	{ .name = "wr0", .buf_size = SZ_16M, },
	{ .name = "wr1", .buf_size = SZ_16M, },
	{ .name = "wr2", .buf_size = SZ_16M, },
	{ .name = "wr3", .buf_size = SZ_16M, },
	{ .name = "wr4", .buf_size = SZ_16M, },
	{ .name = "wr6", .buf_size = SZ_16M, },
	{ .name = "uou", .buf_size = SZ_16M, },
	{ .name = "uin", .buf_size = SZ_16M, },
};

static int abox_dump_memlog_file_completed(struct memlog_obj *obj, u32 flags)
{
	/* NOP */
	return 0;
}

static int abox_dump_memlog_status_notify(struct memlog_obj *obj, u32 flags)
{
	/* NOP */
	return 0;
}

static int abox_dump_memlog_level_notify(struct memlog_obj *obj, u32 flags)
{
	/* NOP */
	return 0;
}

static int abox_dump_memlog_enable_notify(struct memlog_obj *obj, u32 flags)
{
	struct abox_data *data = dev_get_drvdata(abox_dump_dev_abox);
	struct memlog_obj *_memlog_obj;
	struct abox_dump_info *info = NULL;
	int idx;

	for (idx = 0; idx < ARRAY_SIZE(abox_dump_memlogs); idx++) {
		_memlog_obj = abox_dump_memlogs[idx].fobj;
		if (_memlog_obj == obj) {
			info = abox_dump_get_info_by_name(
					abox_dump_memlogs[idx].name);
			break;
		}
	}

	if (!info) {
		abox_info(data->dev, "dump info of %s is not found\n",
				obj->file->file_name);
		return 0;
	}

	abox_info(info->dev, "abox dump log(%s) %s is sent\n",
			info->name, obj->enabled ? "ENABLE" : "DISABLE");

	if (info->memlog_started != obj->enabled) {
		if (obj->enabled)
			pm_runtime_get_sync(info->dev);
		else
			pm_runtime_put(info->dev);
	}

	info->memlog_started = obj->enabled;
	if (obj->enabled)
		info->pointer = info->memlog_pointer = 0;
	info->memlog_idx = idx;
	abox_dump_request_dump(info->id);

	return 0;
}

static const struct memlog_ops abox_dump_memlog_ops = {
	.file_ops_completed = abox_dump_memlog_file_completed,
	.log_status_notify = abox_dump_memlog_status_notify,
	.log_level_notify = abox_dump_memlog_level_notify,
	.log_enable_notify = abox_dump_memlog_enable_notify,
};


void abox_dump_memlog_register(struct device *dev_abox)
{
	struct abox_data *data = dev_get_drvdata(dev_abox);
	char name[32] = {0,};
	int i, ret;

	ret = memlog_register("abox-dump", data->dev, &data->dump_desc);
	if (ret) {
		abox_err(data->dev, "failed to register dump memlog\n");
		return;
	}

	data->dump_desc->ops = abox_dump_memlog_ops;

	for (i = 0; i < ARRAY_SIZE(abox_dump_memlogs); i++) {
		snprintf(name, sizeof(name), "%s.pcm",
				abox_dump_memlogs[i].name);
		abox_dump_memlogs[i].fobj = memlog_alloc_file(data->dump_desc,
				name, SZ_128K,
				abox_dump_memlogs[i].buf_size, 20, 10);
	}
}

static void abox_dump_memlog_work_func(struct work_struct *work)
{
	struct abox_dump_info *info = container_of(work,
			struct abox_dump_info, memlog_work);
	struct memlog_obj *fobj;
	struct device *dev;
	const char *name;
	void *area;
	size_t bytes;
	size_t pointer;
	int ret;

	if (!info) {
		abox_info(abox_dump_dev_abox, "dump information is not found\n");
		return;
	}

	fobj = abox_dump_memlogs[info->memlog_idx].fobj;
	if (!fobj->enabled)
		return;

	dev = info->dev;
	name = info->name;
	area = info->buffer.area;
	bytes = info->buffer.bytes;
	pointer = info->pointer;

	if (pointer < info->memlog_pointer) {
		ret = memlog_write_file(fobj, area + info->memlog_pointer,
				bytes - info->memlog_pointer);
		if (ret < 0)
			abox_err(dev, "Failed to write pcm dump (%d)\n", ret);
		abox_dbg(dev, "memlog_write(%pK, %zx)\n",
				area + info->memlog_pointer,
				bytes - info->memlog_pointer);
		info->memlog_pointer = 0;
	}
	ret = memlog_write_file(fobj, area + info->memlog_pointer,
			pointer - info->memlog_pointer);
	if (ret < 0)
		abox_err(dev, "Failed to write pcm dump (%d)\n", ret);
	abox_dbg(dev, "memlog_write(%pK, %zx)\n",
			area + info->memlog_pointer,
			pointer - info->memlog_pointer);
	info->memlog_pointer = pointer;
}

static void abox_dump_memlog_wait_for_flush(struct abox_dump_info *info)
{
	/* memlog timeout + 1 */
	u64 limit = local_clock() + (NSEC_PER_SEC * (5 + 1));

	while (memlog_is_data_remaining(info->auto_fobj)) {
		if (local_clock() > limit) {
			abox_warn(abox_dump_dev_abox, "memlog flush timeout\n");
			break;
		}
		msleep(100);
	}
}

static ssize_t abox_dump_auto_read(struct file *file, char __user *data,
		size_t count, loff_t *ppos, bool enable)
{
	const size_t sz_buffer = PAGE_SIZE;
	struct abox_dump_info *info;
	char *buffer, *p_buffer;
	ssize_t ret;

	abox_dbg(abox_dump_dev_abox, "%s(%zu, %lld, %d)\n", __func__, count,
			*ppos, enable);

	p_buffer = buffer = kmalloc(sz_buffer, GFP_KERNEL);
	if (!buffer)
		return -ENOMEM;

	list_for_each_entry(info, &abox_dump_list_head, list) {
		if (info->auto_started == enable) {
			p_buffer += snprintf(p_buffer, sz_buffer -
					(p_buffer - buffer),
					"%d(%s) ", info->id, info->name);
		}
	}
	snprintf(p_buffer, 2, "\n");

	ret = simple_read_from_buffer(data, count, ppos, buffer,
			p_buffer - buffer);
	kfree(buffer);
	return ret;
}

static ssize_t abox_dump_auto_write(struct file *file, const char __user *data,
		size_t count, loff_t *ppos, bool enable)
{
	const size_t sz_buffer = PAGE_SIZE;
	struct abox_dump_info *info;
	char *buffer, *p_buffer, *token;
	ssize_t ret;

	abox_dbg(abox_dump_dev_abox, "%s(%zu, %lld, %d)\n", __func__, count,
			*ppos, enable);

	p_buffer = buffer = kzalloc(sz_buffer, GFP_KERNEL);
	if (!buffer)
		return -ENOMEM;

	ret = simple_write_to_buffer(buffer, sz_buffer, ppos, data, count);
	if (ret < 0)
		goto err;

	while ((token = strsep(&p_buffer, " ")) != NULL) {
		char name[NAME_LENGTH];
		int id;

		if (sscanf(token, "%11d", &id) == 1)
			info = abox_dump_get_info(id);
		else if (sscanf(token, "%31s", name) == 1)
			info = abox_dump_get_info_by_name(name);
		else
			info = NULL;

		if (IS_ERR_OR_NULL(info)) {
			abox_err(abox_dump_dev_abox, "invalid argument\n");
			continue;
		}

		if (info->auto_started != enable) {
			if (enable)
				pm_runtime_get_sync(info->dev);
			else
				pm_runtime_put(info->dev);
		}

		info->auto_started = enable;
		if (enable) {
			char filename[SZ_64];
			struct abox_data *data =
				dev_get_drvdata(abox_dump_dev_abox);
			if (!info->auto_fobj) {
				snprintf(filename, sizeof(filename),
						"%s-auto_pcm", info->name);
				info->auto_fobj = memlog_alloc_file(
						data->dump_desc,
						filename, SZ_4M,
						SZ_1G, 20, 1);
				if (!info->auto_fobj) {
					abox_err(abox_dump_dev_abox,
						"auto file %s open error\n",
						info->name);
					goto err;
				}
				/* enable file write */
				info->auto_fobj->enabled = true;
			}

			info->pointer = info->auto_pointer = 0;
		}

		abox_dump_request_dump(info->id);
	}

	/* clear stopped dump */
	list_for_each_entry(info, &abox_dump_list_head, list) {
		if (!info->auto_started && info->auto_fobj) {
			abox_dump_memlog_wait_for_flush(info);
			memlog_free(info->auto_fobj);
			info->auto_fobj = NULL;
		}
	}

err:
	kfree(buffer);
	return ret;
}

static ssize_t abox_dump_auto_start_read(struct file *file,
		char __user *data, size_t count, loff_t *ppos)
{
	return abox_dump_auto_read(file, data, count, ppos, true);
}

static ssize_t abox_dump_auto_start_write(struct file *file,
		const char __user *data, size_t count, loff_t *ppos)
{
	return abox_dump_auto_write(file, data, count, ppos, true);
}

static ssize_t abox_dump_auto_stop_read(struct file *file,
		char __user *data, size_t count, loff_t *ppos)
{
	return abox_dump_auto_read(file, data, count, ppos, false);
}

static ssize_t abox_dump_auto_stop_write(struct file *file,
		const char __user *data, size_t count, loff_t *ppos)
{
	return abox_dump_auto_write(file, data, count, ppos, false);
}

static const struct file_operations abox_dump_auto_start_fops = {
	.read = abox_dump_auto_start_read,
	.write = abox_dump_auto_start_write,
};

static const struct file_operations abox_dump_auto_stop_fops = {
	.read = abox_dump_auto_stop_read,
	.write = abox_dump_auto_stop_write,
};

static void abox_dump_auto_dump_work_func(struct work_struct *work)
{
	struct abox_dump_info *info = container_of(work,
			struct abox_dump_info, auto_work);
	struct device *dev = info->dev;
	void *area = info->buffer.area;
	size_t bytes = info->buffer.bytes;
	size_t pointer = info->pointer;
	int ret;

	if (pointer < info->auto_pointer) {
		ret = memlog_write_file(info->auto_fobj,
				area + info->auto_pointer,
				bytes - info->auto_pointer);
		if (ret < 0)
			abox_err(dev, "Failed to write pcm dump (%d)\n", ret);
		abox_dbg(dev, "auto_write(%pK, %zx)\n",
				area + info->auto_pointer,
				bytes - info->auto_pointer);
		info->auto_pointer = 0;
	}
	ret = memlog_write_file(info->auto_fobj,
			area + info->auto_pointer,
			pointer - info->auto_pointer);
	if (ret < 0)
		abox_err(dev, "Failed to write pcm dump (%d)\n", ret);
	abox_dbg(dev, "auto_write(%pK, %zx)\n",
			area + info->auto_pointer,
			pointer - info->auto_pointer);
	info->auto_pointer = pointer;
}

static ssize_t abox_dump_file_read(struct file *file, char __user *data,
		size_t count, loff_t *ppos)
{
	struct abox_dump_info *info = file->private_data;
	struct device *dev = info->dev;
	size_t end, pointer;
	ssize_t size;
	int ret;

	abox_dbg(dev, "%s(%#zx)\n", __func__, count);

	do {
		pointer = READ_ONCE(info->pointer);
		end = (info->file_pointer <= pointer) ? pointer :
				info->buffer.bytes;
		size = min(end - info->file_pointer, count);
		abox_dbg(dev, "pointer=%#zx file_pointer=%#zx size=%#zx\n",
				pointer, info->file_pointer, size);
		if (!size) {
			if (file->f_flags & O_NONBLOCK)
				return -EAGAIN;

			ret = wait_event_interruptible(info->file_waitqueue,
					pointer != READ_ONCE(info->pointer));
			if (ret < 0)
				return ret;
		}
	} while (!size);

	if (copy_to_user(data, info->buffer.area + info->file_pointer, size))
		return -EFAULT;

	info->file_pointer += size;
	info->file_pointer %= info->buffer.bytes;

	return size;
}

static int abox_dump_file_open(struct inode *i, struct file *f)
{
	struct abox_dump_info *info = abox_dump_get_data(f);
	struct device *dev = info->dev;

	abox_dbg(dev, "%s\n", __func__);

	if (info->file_started)
		return -EBUSY;

	pm_runtime_get(dev);

	f->private_data = info;
	info->file_started = true;
	info->pointer = info->file_pointer = 0;
	abox_dump_request_dump(info->id);

	return 0;
}

static int abox_dump_file_release(struct inode *i, struct file *f)
{
	struct abox_dump_info *info = f->private_data;
	struct device *dev = info->dev;

	abox_dbg(dev, "%s\n", __func__);

	info->file_started = false;
	abox_dump_request_dump(info->id);

	pm_runtime_put(dev);

	return 0;
}

static unsigned int abox_dump_file_poll(struct file *file, poll_table *wait)
{
	struct abox_dump_info *info = file->private_data;

	abox_dbg(info->dev, "%s\n", __func__);

	poll_wait(file, &info->file_waitqueue, wait);
	return POLLIN | POLLRDNORM;
}

static const struct file_operations abox_dump_fops = {
	.llseek = generic_file_llseek,
	.read = abox_dump_file_read,
	.poll = abox_dump_file_poll,
	.open = abox_dump_file_open,
	.release = abox_dump_file_release,
	.owner = THIS_MODULE,
};

static struct snd_pcm_hardware abox_dump_hardware = {
	.info		= SNDRV_PCM_INFO_INTERLEAVED
			| SNDRV_PCM_INFO_BLOCK_TRANSFER
			| SNDRV_PCM_INFO_MMAP
			| SNDRV_PCM_INFO_MMAP_VALID,
	.formats	= ABOX_SAMPLE_FORMATS,
	.rates		= SNDRV_PCM_RATE_8000_192000 | SNDRV_PCM_RATE_KNOT,
	.rate_min	= 8000,
	.rate_max	= 384000,
	.channels_min	= 1,
	.channels_max	= 8,
	.periods_min	= 2,
	.periods_max	= 32,
};

void abox_dump_period_elapsed(int id, size_t pointer)
{
	struct abox_dump_info *info = abox_dump_get_info(id);
	struct device *dev = info->dev;

	abox_dbg(dev, "%s[%d](%zx)\n", __func__, id, pointer);

	info->pointer = pointer;
	if (info->auto_started)
		schedule_work(&info->auto_work);
	if (info->memlog_started)
		schedule_work(&info->memlog_work);
	if (info->file_started)
		wake_up_interruptible(&info->file_waitqueue);
	if (info->started)
		snd_pcm_period_elapsed(info->substream);
}

static int abox_dump_open(struct snd_pcm_substream *substream)
{
	struct snd_soc_pcm_runtime *rtd = substream->private_data;
	int id = rtd->dai_link->id;
	struct abox_dump_info *info = abox_dump_get_info(id);
	struct device *dev = info->dev;
	struct snd_dma_buffer *dmab = &substream->dma_buffer;

	abox_dbg(dev, "%s[%d]\n", __func__, id);

	pm_runtime_get(dev);

	abox_dump_hardware.buffer_bytes_max = dmab->bytes;
	abox_dump_hardware.period_bytes_min = dmab->bytes /
			abox_dump_hardware.periods_max;
	abox_dump_hardware.period_bytes_max = dmab->bytes /
			abox_dump_hardware.periods_min;

	snd_soc_set_runtime_hwparams(substream, &abox_dump_hardware);

	info->substream = substream;

	return 0;
}

static int abox_dump_close(struct snd_pcm_substream *substream)
{
	struct snd_soc_pcm_runtime *rtd = substream->private_data;
	int id = rtd->dai_link->id;
	struct abox_dump_info *info = abox_dump_get_info(id);
	struct device *dev = info->dev;

	abox_dbg(dev, "%s[%d]\n", __func__, id);

	info->substream = NULL;
	pm_runtime_put(dev);

	return 0;
}

static int abox_dump_hw_params(struct snd_pcm_substream *substream,
		struct snd_pcm_hw_params *params)
{
	struct snd_soc_pcm_runtime *rtd = substream->private_data;
	int id = rtd->dai_link->id;
	struct abox_dump_info *info = abox_dump_get_info(id);
	struct device *dev = info->dev;

	abox_dbg(dev, "%s[%d]\n", __func__, id);

	return snd_pcm_lib_malloc_pages(substream, params_buffer_bytes(params));
}

static int abox_dump_hw_free(struct snd_pcm_substream *substream)
{
	struct snd_soc_pcm_runtime *rtd = substream->private_data;
	int id = rtd->dai_link->id;
	struct abox_dump_info *info = abox_dump_get_info(id);
	struct device *dev = info->dev;

	abox_dbg(dev, "%s[%d]\n", __func__, id);

	return snd_pcm_lib_free_pages(substream);
}

static int abox_dump_prepare(struct snd_pcm_substream *substream)
{
	struct snd_soc_pcm_runtime *rtd = substream->private_data;
	int id = rtd->dai_link->id;
	struct abox_dump_info *info = abox_dump_get_info(id);
	struct device *dev = info->dev;

	abox_dbg(dev, "%s[%d]\n", __func__, id);

	info->pointer = 0;

	return 0;
}

static int abox_dump_trigger(struct snd_pcm_substream *substream, int cmd)
{
	struct snd_soc_pcm_runtime *rtd = substream->private_data;
	int id = rtd->dai_link->id;
	struct abox_dump_info *info = abox_dump_get_info(id);
	struct device *dev = info->dev;

	abox_dbg(dev, "%s[%d](%d)\n", __func__, id, cmd);

	switch (cmd) {
	case SNDRV_PCM_TRIGGER_START:
	case SNDRV_PCM_TRIGGER_RESUME:
	case SNDRV_PCM_TRIGGER_PAUSE_RELEASE:
		info->started = true;
		break;
	case SNDRV_PCM_TRIGGER_STOP:
	case SNDRV_PCM_TRIGGER_SUSPEND:
	case SNDRV_PCM_TRIGGER_PAUSE_PUSH:
		info->started = false;
		break;
	default:
		abox_err(dev, "invalid command: %d\n", cmd);
		return -EINVAL;
	}

	abox_dump_request_dump(id);

	return 0;
}

void abox_dump_transfer(int id, const char *buf, size_t bytes)
{
	struct abox_dump_info *info = abox_dump_get_info(id);
	struct device *dev = info->dev;
	size_t size, pointer;

	abox_dbg(dev, "%s[%d](%pK, %zx): %zx\n", __func__, id, buf, bytes,
			info->pointer);

	size = min(bytes, info->buffer.bytes - info->pointer);
	memcpy(info->buffer.area + info->pointer, buf, size);
	if (bytes - size > 0)
		memcpy(info->buffer.area, buf + size, bytes - size);

	pointer = (info->pointer + bytes) % info->buffer.bytes;
	abox_dump_period_elapsed(id, pointer);
}

static snd_pcm_uframes_t abox_dump_pointer(struct snd_pcm_substream *substream)
{
	struct snd_soc_pcm_runtime *rtd = substream->private_data;
	int id = rtd->dai_link->id;
	struct abox_dump_info *info = abox_dump_get_info(id);
	struct device *dev = info->dev;

	abox_dbg(dev, "%s[%d]\n", __func__, id);

	return bytes_to_frames(substream->runtime, info->pointer);
}

static struct snd_pcm_ops abox_dump_ops = {
	.open		= abox_dump_open,
	.close		= abox_dump_close,
	.hw_params	= abox_dump_hw_params,
	.hw_free	= abox_dump_hw_free,
	.prepare	= abox_dump_prepare,
	.trigger	= abox_dump_trigger,
	.pointer	= abox_dump_pointer,
};

static int abox_dump_probe(struct snd_soc_component *component)
{
	struct device *dev = component->dev;
	int id = to_platform_device(dev)->id;

	abox_dbg(dev, "%s[%d]\n", __func__, id);

	return 0;
}

static int abox_dump_pcm_new(struct snd_soc_pcm_runtime *runtime)
{
	const size_t default_size = SZ_128K;
	int id = runtime->dai_link->id;
	struct abox_dump_info *info = abox_dump_get_info(id);
	struct device *dev = info->dev;
	struct snd_pcm *pcm = runtime->pcm;
	struct snd_pcm_str *stream = &pcm->streams[SNDRV_PCM_STREAM_CAPTURE];
	struct snd_pcm_substream *substream = stream->substream;
	struct snd_dma_buffer *dmab = &substream->dma_buffer;

	abox_dbg(dev, "%s[%d]\n", __func__, id);

	if (info->buffer.area) {
		dmab->dev.type = SNDRV_DMA_TYPE_DEV;
		dmab->dev.dev = dev;
		dmab->area = info->buffer.area;
		dmab->addr = info->buffer.addr;
		dmab->bytes = info->buffer.bytes;
	} else {
		snd_pcm_lib_preallocate_pages(substream,
				SNDRV_DMA_TYPE_CONTINUOUS,
				(struct device *)GFP_KERNEL,
				default_size, default_size);
		info->buffer = *dmab;
	}

	return 0;
}

static void abox_dump_pcm_free(struct snd_pcm *pcm)
{
	struct snd_soc_pcm_runtime *rtd = pcm->private_data;
	struct snd_pcm_str *stream = &pcm->streams[SNDRV_PCM_STREAM_CAPTURE];
	struct snd_pcm_substream *substream = stream->substream;
	int id = rtd->dai_link->id;
	struct abox_dump_info *info = abox_dump_get_info(id);
	struct device *dev = info->dev;

	abox_dbg(dev, "%s[%d]\n", __func__, id);

	if (substream->dma_buffer.dev.type == SNDRV_DMA_TYPE_CONTINUOUS)
		snd_pcm_lib_free_pages(substream);
}

static const struct snd_soc_component_driver abox_dump_component = {
	.probe		= abox_dump_probe,
	.ops		= &abox_dump_ops,
	.pcm_new	= abox_dump_pcm_new,
	.pcm_free	= abox_dump_pcm_free,
};

static struct snd_soc_card abox_dump_card = {
	.name = "abox_dump",
	.owner = THIS_MODULE,
	.num_links = 0,
};

struct proc_dir_entry *abox_dump_register_file(const char *name, void *data,
		const struct file_operations *fops)
{
	return abox_proc_create_file(name, 0664, dir_dump, fops, data, 0);
}

void abox_dump_unregister_file(struct proc_dir_entry *file)
{
	abox_proc_remove_file(file);
}

void *abox_dump_get_data(const struct file *file)
{
	return abox_proc_data(file);
}

static int abox_dump_register_work_single(void)
{
	struct abox_dump_info *info, *_info;
	unsigned long flags;

	abox_dbg(abox_dump_dev_abox, "%s\n", __func__);

	spin_lock_irqsave(&abox_dump_lock, flags);
	list_for_each_entry_reverse(_info, &abox_dump_list_head, list) {
		if (!_info->registered)
			break;
	}
	spin_unlock_irqrestore(&abox_dump_lock, flags);

	if (&_info->list == &abox_dump_list_head)
		return -EINVAL;

	info = devm_kmemdup(_info->dev, _info, sizeof(*_info), GFP_KERNEL);
	abox_info(info->dev, "%s(%d, %s, %#zx)\n", __func__, info->id,
			info->name, info->buffer.bytes);
	info->file = abox_dump_register_file(info->name, info, &abox_dump_fops);
	init_waitqueue_head(&info->file_waitqueue);
	INIT_WORK(&info->auto_work, abox_dump_auto_dump_work_func);
	INIT_WORK(&info->memlog_work, abox_dump_memlog_work_func);
	info->registered = true;

	spin_lock_irqsave(&abox_dump_lock, flags);
	list_replace(&_info->list, &info->list);
	spin_unlock_irqrestore(&abox_dump_lock, flags);

	platform_device_register_data(info->dev, "abox-dump",
			info->id, NULL, 0);

	kfree(_info);
	return 0;
}

static void abox_dump_register_work_func(struct work_struct *work)
{
	abox_dbg(abox_dump_dev_abox, "%s\n", __func__);

	do {} while (abox_dump_register_work_single() >= 0);
}

static DECLARE_WORK(abox_dump_register_work, abox_dump_register_work_func);

static int abox_dump_allocate_id(int gid, int id)
{
	struct abox_dump_info *info;
	unsigned long flags;
	int ret = id;

	do {
		spin_lock_irqsave(&abox_dump_lock, flags);
		list_for_each_entry(info, &abox_dump_list_head, list) {
			if (info->id == ret) {
				ret++;
				break;
			}
		}
		spin_unlock_irqrestore(&abox_dump_lock, flags);
	} while (&info->list != &abox_dump_list_head);

	return ret;
}

int abox_dump_register(struct abox_data *data, int gid, int id,
		const char *name, void *area, phys_addr_t addr, size_t bytes)
{
	struct device *dev = data->dev;
	struct abox_dump_info *info;
	unsigned long flags;

	abox_dbg(dev, "%s[%d](%s, %#zx)\n", __func__, id, name, bytes);

	spin_lock_irqsave(&abox_dump_lock, flags);
	info = abox_dump_get_info(id);
	spin_unlock_irqrestore(&abox_dump_lock, flags);
	if (info) {
		abox_dbg(dev, "already registered dump: %d\n", id);
		return 0;
	}

	info = kzalloc(sizeof(*info), GFP_ATOMIC);
	if (!info)
		return -ENOMEM;

	mutex_init(&info->lock);
	info->c_gid = gid;
	info->c_id = id;
	info->id = abox_dump_allocate_id(info->c_gid, info->c_id);
	strlcpy(info->name, name, sizeof(info->name));
	info->buffer.area = area;
	info->buffer.addr = addr;
	info->buffer.bytes = bytes;
	abox_dump_dev_abox = info->dev = dev;

	spin_lock_irqsave(&abox_dump_lock, flags);
	list_add_tail(&info->list, &abox_dump_list_head);
	spin_unlock_irqrestore(&abox_dump_lock, flags);

	schedule_work(&abox_dump_register_work);

	return 0;
}

SND_SOC_DAILINK_DEF(dailink_comp_dummy, DAILINK_COMP_ARRAY(COMP_DUMMY(),));

static void abox_dump_register_card_work_func(struct work_struct *work)
{
	int i;

	pr_debug("%s\n", __func__);

	for (i = 0; i < abox_dump_card.num_links; i++) {
		struct snd_soc_dai_link *link = &abox_dump_card.dai_link[i];

		if (link->name)
			continue;

		link->name = link->stream_name =
				kasprintf(GFP_KERNEL, "dummy%d", i);
		link->cpus = dailink_comp_dummy;
		link->num_cpus = ARRAY_SIZE(dailink_comp_dummy);
		link->codecs = dailink_comp_dummy;
		link->num_codecs = ARRAY_SIZE(dailink_comp_dummy);
		link->no_pcm = 1;
	}
	abox_register_extra_sound_card(abox_dump_card.dev, &abox_dump_card, 2);
}

static DECLARE_DELAYED_WORK(abox_dump_register_card_work,
		abox_dump_register_card_work_func);

static int abox_dump_add_dai_link(struct device *dev)
{
	int id = to_platform_device(dev)->id;
	struct abox_dump_info *info = abox_dump_get_info(id);
	struct snd_soc_dai_link *link;

	abox_dbg(dev, "%s[%d]\n", __func__, id);

	cancel_delayed_work_sync(&abox_dump_register_card_work);

	if (abox_dump_card.num_links <= id) {
		link = krealloc(abox_dump_card.dai_link,
				sizeof(abox_dump_card.dai_link[0]) * (id + 1),
				GFP_KERNEL | __GFP_ZERO);
		if (!link)
			return -ENOMEM;

		abox_dump_card.dai_link = link;
	}

	link = &abox_dump_card.dai_link[id];
	kfree(link->name);
	link->name = link->stream_name = kstrdup(info->name, GFP_KERNEL);
	link->id = id;
	link->cpus = dailink_comp_dummy;
	link->num_cpus = ARRAY_SIZE(dailink_comp_dummy);
	link->platforms = devm_kcalloc(dev, 1, sizeof(*link->platforms),
			GFP_KERNEL);
	if (!link->platforms)
		return -ENOMEM;
	link->platforms->name = dev_name(dev);
	link->num_platforms = 1;
	link->codecs = dailink_comp_dummy;
	link->num_codecs = ARRAY_SIZE(dailink_comp_dummy);
	link->ignore_suspend = 1;
	link->ignore_pmdown_time = 1;
	link->no_pcm = 0;
	link->capture_only = true;

	if (abox_dump_card.num_links <= id)
		abox_dump_card.num_links = id + 1;

	schedule_delayed_work(&abox_dump_register_card_work, HZ);

	return 0;
}

static int samsung_abox_dump_probe(struct platform_device *pdev)
{
	struct device *dev = &pdev->dev;
	int id = to_platform_device(dev)->id;
	struct abox_dump_info *info = abox_dump_get_info(id);
	int ret = 0;

	abox_dbg(dev, "%s[%d]\n", __func__, id);

	if (id < 0) {
		abox_dump_card.dev = &pdev->dev;
		schedule_delayed_work(&abox_dump_register_card_work, 0);
	} else {
		info->dev = dev;
		pm_runtime_no_callbacks(dev);
		pm_runtime_enable(dev);

		ret = devm_snd_soc_register_component(dev, &abox_dump_component,
				NULL, 0);
		if (ret < 0)
			abox_err(dev, "register component failed: %d\n", ret);

		ret = abox_dump_add_dai_link(dev);
		if (ret < 0)
			abox_err(dev, "add dai link failed: %d\n", ret);
	}

	return ret;
}

static int samsung_abox_dump_remove(struct platform_device *pdev)
{
	struct device *dev = &pdev->dev;
	int id = to_platform_device(dev)->id;

	abox_dbg(dev, "%s[%d]\n", __func__, id);

	return 0;
}

static void samsung_abox_dump_shutdown(struct platform_device *pdev)
{
	struct device *dev = &pdev->dev;

	abox_dbg(dev, "%s\n", __func__);
	pm_runtime_disable(dev);
}

static const struct platform_device_id samsung_abox_dump_driver_ids[] = {
	{
		.name = "abox-dump",
	},
	{},
};
MODULE_DEVICE_TABLE(platform, samsung_abox_dump_driver_ids);

struct platform_driver samsung_abox_dump_driver = {
	.probe  = samsung_abox_dump_probe,
	.remove = samsung_abox_dump_remove,
	.shutdown = samsung_abox_dump_shutdown,
	.driver = {
		.name = "abox-dump",
		.owner = THIS_MODULE,
	},
	.id_table = samsung_abox_dump_driver_ids,
};

void abox_dump_init(struct device *dev_abox)
{
	static struct platform_device *pdev;
	static struct proc_dir_entry *auto_start, *auto_stop;

	abox_info(dev_abox, "%s\n", __func__);

	abox_dump_dev_abox = dev_abox;

	if (IS_ERR_OR_NULL(auto_start))
		auto_start = abox_proc_create_file("dump_auto_start", 0660,
				NULL, &abox_dump_auto_start_fops, dev_abox, 0);

	if (IS_ERR_OR_NULL(auto_stop))
		auto_stop = abox_proc_create_file("dump_auto_stop", 0660,
				NULL, &abox_dump_auto_stop_fops, dev_abox, 0);

	if (IS_ERR_OR_NULL(dir_dump))
		dir_dump = abox_proc_mkdir("dump", NULL);

	if (IS_ERR_OR_NULL(pdev))
		pdev = platform_device_register_data(dev_abox,
				"abox-dump", -1, NULL, 0);

	abox_dump_memlog_register(dev_abox);
}
