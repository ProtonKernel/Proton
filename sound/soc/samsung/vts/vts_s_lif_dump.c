/* sound/soc/samsung/vts/vts_dump.c
 *
 * ALSA SoC - Samsung VTS dump driver
 *
 * Copyright (c) 2019 Samsung Electronics Co. Ltd.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

/* #define DEBUG */
#include <linux/module.h>
#include <linux/debugfs.h>
#include <linux/spinlock.h>
#include <linux/vmalloc.h>
#include <linux/pm_runtime.h>
#include <sound/samsung/vts.h>
#include <sound/samsung/abox.h>

#include "vts_dbg.h"
#include "vts_s_lif.h"
#include "vts_s_lif_dump.h"

#define BUFFER_MAX (SZ_64)
#define NAME_LENGTH (SZ_32)

struct vts_s_lif_dump_info {
	struct device *dev;
	struct list_head list;
	int id;
	char name[NAME_LENGTH];
	struct mutex lock;
	struct snd_dma_buffer buffer;
	struct snd_pcm_substream *substream;
	size_t pointer;
	bool started;
	bool auto_started;
	bool file_created;
	struct file *filp;
	ssize_t auto_pointer;
	struct work_struct auto_work;
};

static struct vts_data *dump_data_vts;
static struct vts_s_lif_data *dump_data_vts_s_lif;
static struct device *dump_dev_vts;
static struct device *dump_dev_vts_s_lif;
/* support only one dump*/
static struct vts_s_lif_dump_info vts_s_lif_dump;

void vts_s_lif_dump_buffer_init(struct device *dev_vts_s_lif, struct vts_data *data_vts);

static void vts_s_lif_dump_request_dump(int id)
{
	/* support only one dump*/
	struct vts_s_lif_dump_info *info = &vts_s_lif_dump;
	bool start = info->started || info->auto_started;
	u32 values[3];
	int ret = 0;

	dev_dbg(dump_dev_vts, "%s(%d)\n", __func__, id);

	values[0] = start ? VTS_START_SLIFDUMP : VTS_STOP_SLIFDUMP;
	values[1] = 0;
	values[2] = 0;
	ret = vts_start_ipc_transaction(dump_dev_vts, dump_data_vts,
			VTS_IRQ_AP_TEST_COMMAND, &values, 0, 1);
	if (ret < 0)
		dev_err(dump_dev_vts, "Enable_debuglog ipc transaction failed\n");
}

static ssize_t vts_s_lif_dump_auto_read(struct file *file, char __user *data,
		size_t count, loff_t *ppos, bool enable)
{
	/* support only one dump*/
	struct vts_s_lif_dump_info *info = &vts_s_lif_dump;
	char buffer[SZ_256] = {0,}, *buffer_p = buffer;

	dev_dbg(dump_dev_vts_s_lif,
			"%s(%zu, %lld, %d)\n", __func__, count,
			*ppos, enable);

	if (info->auto_started == enable) {
		buffer_p += snprintf(buffer_p, sizeof(buffer) -
				(buffer_p - buffer),
				"%d(%s) ", info->id, info->name);
	}

	snprintf(buffer_p, 2, "\n");

	return simple_read_from_buffer(data, count, ppos, buffer,
			buffer_p - buffer);
}

static ssize_t vts_s_lif_dump_auto_write(struct file *file, const char __user *data,
		size_t count, loff_t *ppos, bool enable)
{
	char buffer[SZ_256] = {0,};
	char *p_buffer = buffer, *token = NULL;
	ssize_t ret;
	/* support only one dump*/
	struct vts_s_lif_dump_info *info = &vts_s_lif_dump;
	struct vts_data *dump_data_vts = dev_get_drvdata(dump_dev_vts);
	struct vts_s_lif_data *dump_data_vts_slif = dev_get_drvdata(dump_dev_vts_s_lif);

	dev_dbg(dump_dev_vts_s_lif,
			"%s(%zu, %lld, %d)\n", __func__, count,
			*ppos, enable);

	ret = simple_write_to_buffer(buffer, sizeof(buffer), ppos, data, count);
	if (ret < 0)
		return ret;

	while ((token = strsep(&p_buffer, " ")) != NULL) {
		if (IS_ERR_OR_NULL(info)) {
			dev_err(dump_dev_vts_s_lif, "invalid argument\n");
			continue;
		}

		if (info->auto_started != enable) {
			struct device *dev = info->dev;

			if (enable)
				pm_runtime_get_sync(dev);
			else
				pm_runtime_put(dev);
		}

		info->auto_started = enable;
		dump_data_vts->slif_dump_enabled = enable;
		dump_data_vts_slif->slif_dump_enabled = enable;
		if (enable) {
			/* support onlu one dump */
			vts_s_lif_dump_buffer_init(dump_dev_vts_s_lif, dump_data_vts);
			info->file_created = false;
			info->pointer = info->auto_pointer = 0;
		}

		vts_s_lif_dump_request_dump(info->id);
	}

	return count;
}

static ssize_t vts_s_lif_dump_auto_start_read(struct file *file,
		char __user *data, size_t count, loff_t *ppos)
{
	return vts_s_lif_dump_auto_read(file, data, count, ppos, true);
}

static ssize_t vts_s_lif_dump_auto_start_write(struct file *file,
		const char __user *data, size_t count, loff_t *ppos)
{
	return vts_s_lif_dump_auto_write(file, data, count, ppos, true);
}

static ssize_t vts_s_lif_dump_auto_stop_read(struct file *file,
		char __user *data, size_t count, loff_t *ppos)
{
	return vts_s_lif_dump_auto_read(file, data, count, ppos, false);
}

static ssize_t vts_s_lif_dump_auto_stop_write(struct file *file,
		const char __user *data, size_t count, loff_t *ppos)
{
	return vts_s_lif_dump_auto_write(file, data, count, ppos, false);
}

static const struct file_operations vts_s_lif_dump_auto_start_fops = {
	.read = vts_s_lif_dump_auto_start_read,
	.write = vts_s_lif_dump_auto_start_write,
};

static const struct file_operations vts_s_lif_dump_auto_stop_fops = {
	.read = vts_s_lif_dump_auto_stop_read,
	.write = vts_s_lif_dump_auto_stop_write,
};

static void vts_s_lif_dump_auto_dump_work_func(struct work_struct *work)
{
	struct vts_s_lif_dump_info *info = container_of(work,
			struct vts_s_lif_dump_info, auto_work);
	struct device *dev = info->dev;
/*	const char *name = info->name; */

	dev_dbg(dev, "%s[%d](%zx)(%zx)\n", __func__,
			info->id,
			info->pointer,
			info->auto_pointer);

/*	if (info->auto_started) {
 *		char filename[SZ_64];
 *		struct file *filp;
 *
 *		sprintf(filename, "/data/vts_s_lif_dump-%d.raw", info->id);
 *
 *		if (likely(info->file_created)) {
 *			filp = filp_open(filename, O_RDWR | O_APPEND | O_CREAT,
 *					0660);
 *			dev_dbg(dev, "appended\n");
 *		} else {
 *			filp = filp_open(filename, O_RDWR | O_TRUNC | O_CREAT,
 *					0660);
 *			info->file_created = true;
 *			dev_dbg(dev, "created\n");
 *		}
 *		if (!IS_ERR_OR_NULL(filp)) {
 *			void *area = info->buffer.area;
 *			size_t bytes = info->buffer.bytes;
 *			size_t pointer = info->pointer;
 *			dev_warn(dev, "[kernel_write]%pad, %pK, %zx, %zx)\n",
 *				&info->buffer.addr, area, bytes,
 *				info->auto_pointer);
 *			if (pointer < info->auto_pointer) {
 *				kernel_write(filp, area + info->auto_pointer,
 *					bytes - info->auto_pointer,
 *					&filp->f_pos);
 *				dev_dbg(dev,
 *					"kernel_write(%pK, %zx, %zx, %zx)\n",
 *					area + info->auto_pointer,
 *					pointer, info->auto_pointer,
 *					bytes - info->auto_pointer);
 *				fo->auto_pointer = 0;
 *			}
 *			kernel_write(filp, area + info->auto_pointer,
 *				pointer - info->auto_pointer,
 *				&filp->f_pos);
 *			dev_dbg(dev, "kernel_write(%pK, %zx)\n",
 *				area + info->auto_pointer,
 *				pointer - info->auto_pointer);
 *			info->auto_pointer = pointer;
 *
 *			vfs_fsync(filp, 1);
 *			filp_close(filp, NULL);
 *		} else {
 *			dev_err(dev, "dump file %s open error: %ld\n", name,
 *					PTR_ERR(filp));
 *		}
 *	}
 */
}

void vts_s_lif_dump_period_elapsed(int id, size_t pointer)
{
	struct vts_s_lif_dump_info *info = &vts_s_lif_dump;
	struct device *dev = info->dev;

	dev_dbg(dev, "%s[%d](%zx)\n", __func__, id, pointer);

	info->pointer = (pointer -
			(dump_data_vts->dma_area_vts + BUFFER_BYTES_MAX/2));
	schedule_work(&info->auto_work);
}
EXPORT_SYMBOL(vts_s_lif_dump_period_elapsed);

static int samsung_vts_s_lif_dump_probe(struct platform_device *pdev)
{
	return 0;
}

static int samsung_vts_s_lif_dump_remove(struct platform_device *pdev)
{
	struct device *dev = &pdev->dev;
	int id = to_platform_device(dev)->id;

	dev_dbg(dev, "%s[%d]\n", __func__, id);

	return 0;
}

static const struct platform_device_id samsung_vts_s_lif_dump_driver_ids[] = {
	{
		.name = "samsung-slif-dump",
	},
	{},
};
MODULE_DEVICE_TABLE(platform, samsung_vts_s_lif_dump_driver_ids);

struct platform_driver samsung_vts_s_lif_dump_driver = {
	.probe  = samsung_vts_s_lif_dump_probe,
	.remove = samsung_vts_s_lif_dump_remove,
	.driver = {
		.name = "samsung-slif-dump",
		.owner = THIS_MODULE,
	},
	.id_table = samsung_vts_s_lif_dump_driver_ids,
};

void vts_s_lif_dump_buffer_init(struct device *dev_vts_s_lif, struct vts_data *data_vts)
{
	/* support only one dump*/
	struct vts_s_lif_dump_info *info = &vts_s_lif_dump;

	info->dev = dev_vts_s_lif;
	info->buffer.area = data_vts->dmab_rec.area;
	info->buffer.addr = data_vts->dmab_rec.addr;
	info->buffer.bytes = data_vts->dmab_rec.bytes;

	dev_dbg(info->dev, "[vts_info]%pad, %pK, %zx)\n",
			&info->buffer.addr,
			info->buffer.area,
			info->buffer.bytes);
}

void vts_s_lif_dump_init(struct device *dev_vts_s_lif)
{
	static struct platform_device *pdev;
	struct device *dev_vts;
	struct vts_s_lif_data *data;
	static struct dentry *auto_start, *auto_stop;
	struct dentry *dbg_dir = vts_dbg_get_root_dir();
	/* support only one dump*/
	struct vts_s_lif_dump_info *info = &vts_s_lif_dump;

	data = dev_get_drvdata(dev_vts_s_lif);
	dump_dev_vts_s_lif = data->dev;
	dump_dev_vts = data->dev_vts;
	dev_vts = data->dev_vts;

	dump_data_vts_s_lif = data;
	dump_data_vts = data->vts_data;

	vts_s_lif_dump_buffer_init(dump_dev_vts_s_lif, dump_data_vts);
	INIT_WORK(&info->auto_work, vts_s_lif_dump_auto_dump_work_func);

	dev_info(dump_dev_vts, "%s\n", __func__);

	if (auto_start && !IS_ERR(auto_start))
		debugfs_remove(auto_start);
	auto_start = debugfs_create_file("dump_auto_start", 0660, dbg_dir,
			dev_vts, &vts_s_lif_dump_auto_start_fops);

	if (auto_stop && !IS_ERR(auto_stop))
		debugfs_remove(auto_stop);
	auto_stop = debugfs_create_file("dump_auto_stop", 0660, dbg_dir,
			dev_vts, &vts_s_lif_dump_auto_stop_fops);

	if (pdev && !IS_ERR(pdev))
		platform_device_unregister(pdev);
	pdev = platform_device_register_data(dev_vts,
			"samsung-slif-dump", -1, NULL, 0);
}
