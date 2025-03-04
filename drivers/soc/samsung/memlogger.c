/*
 * Copyright (c) 2020 Samsung Electronics Co., Ltd.
 *		http://www.samsung.com
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 and
 * only version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 */

#include <linux/kernel.h>
#include <linux/vmalloc.h>
#include <linux/io.h>
#include <linux/jiffies.h>
#include <linux/notifier.h>
#include <linux/sched.h>
#include <linux/ktime.h>
#include <linux/sched/clock.h>
#include <linux/delay.h>
#include <linux/module.h>
#include <linux/platform_device.h>
#include <linux/fs.h>
#include <linux/mm.h>
#include <linux/poll.h>
#include <linux/uaccess.h>
#include <linux/wait.h>
#include <linux/cdev.h>
#include <linux/atomic.h>
#include <linux/dma-mapping.h>
#include <linux/dmaengine.h>
#include <linux/of_reserved_mem.h>
#include <linux/of.h>
#include <linux/kdebug.h>
#include <asm/arch_timer.h>
#include <soc/samsung/memlogger.h>

#pragma pack(push, 1)
struct memlog_file_cmd {
	u32 cmd;
	u64 max_file_size;
	u32 max_file_num;
	u64 polling_period;
	char file_name[128];
	char desc_name[128];
	struct list_head list;
};
struct memlog_bl {
	u32 magic;
	u64 paddr;
	u64 size;
	char name[24];
};
#pragma pack(pop)

struct memlog_event {
	char cmd[16];
	struct list_head list;
};

struct memlog_ba_file_data {
	void *file;
	void *data;
	size_t size;
	struct list_head list;
};

struct memlog_obj_prv {

	u32 property_flag;

	raw_spinlock_t log_lock;
	struct list_head list;
	struct memlog_bl *mlg_bl_entry;
	bool is_dead;

	struct kobject *kobj;
	struct kobj_attribute level_ka;
	struct kobj_attribute enable_ka;
	struct kobj_attribute name_ka;
	struct kobj_attribute info_ka;
	struct kobj_attribute sync_ka;
	struct kobj_attribute to_string_ka;

	bool support_file;
	bool support_utc;
	bool support_ktime;
	bool support_logfwd;

	struct cdev cdev;
	struct memlog_obj *file_obj;
	struct memlog_obj *source_obj;
	struct mutex file_mutex;
	struct work_struct file_work;
	struct work_struct file_work_sync;
	void *file_buffer;
	size_t file_buffer_size;
	size_t buffered_data_size;
	memlog_data_to_string to_string;

	void __iomem *base_ptr;
	phys_addr_t dump_paddr;

	atomic_t open_cnt;
	bool is_full;
	size_t remained;
	size_t file_event_threshold;

	void *read_ptr;
	void *curr_ptr;
	void *string_buf;

	char *struct_name;
	size_t array_unit_size;
	u64 read_log_idx;
	u64 log_idx;
	u64 max_idx;

	atomic_t lost_data_cnt;
	atomic_long_t lost_data_amount;

	char obj_name[64];
	int obj_minor;
	bool eob;
	struct memlog_obj obj;
};

struct memlog_prv {
	struct device *dev;

	struct mutex obj_lock;
	struct list_head obj_list;
	int obj_cnt;
	bool under_free_objects;

	struct list_head list;
	struct kobject *kobj;
	struct ida obj_minor_id;

	struct memlog desc;
};

struct memlogger {
	bool inited;
	bool enabled;
	bool in_panic;
	unsigned int global_log_level;
	bool file_default_status;
	bool mem_constraint;
	bool mem_to_file_allow;

	struct memlog_bl (*mlg_bl)[64];
	atomic_t mlg_bl_idx;

	struct workqueue_struct *wq;
	struct mutex memlog_list_lock;
	struct list_head memlog_list;
	int memlog_cnt;
	struct mutex dead_obj_list_lock;
	struct list_head dead_obj_list;
	struct mutex zombie_obj_list_lock;
	struct list_head zombie_obj_list;
	struct work_struct clean_prvobj_work;

	struct kobject *obj_kobj;
	struct bin_attribute total_bin_attr;
	struct bin_attribute dead_bin_attr;
	struct bin_attribute dumpstate_bin_attr;
	struct kobject *tree_kobj;

	struct list_head total_info_file_list;
	struct list_head dead_info_file_list;
	struct list_head dumpstate_file_list;
	struct mutex total_info_file_lock;
	struct mutex dead_info_file_lock;
	struct mutex dumpstate_file_lock;

	struct mutex file_cmd_lock;
	struct list_head file_cmd_list;
	struct kobject *cmd_kobj;
	struct kobj_attribute cmd_ka;

	struct class *memlog_class;
	dev_t memlog_dev;
	struct ida memlog_minor_id;

	struct device *event_dev;
	struct cdev event_cdev;
	struct list_head event_list;
	struct mutex event_lock;
	wait_queue_head_t event_wait_q;
	int event_cdev_minor_id;

	struct reserved_mem *rmem;
	struct device *dev;
};

static struct memlogger main_desc;
static struct memlog *memlog_desc;
static struct memlog_obj *memlog_bl_obj;

static size_t _memlog_write_file(struct memlog_obj *obj,
				void *src, size_t size, bool sync);

#define desc_to_data(d) container_of(d, struct memlog_prv, desc)
#define obj_to_data(d) container_of(d, struct memlog_obj_prv, obj)

#define MEMLOG_STRING_BUF_SIZE		(SZ_512)
#define MEMLOG_BIN_ATTR_SIZE		(PAGE_SIZE * 4)
#define MEMLOG_FILE_THRESHOLD_SIZE	(SZ_4K)

#define MEMLOG_NAME		"memlog"
#define MEMLOG_NUM_DEVICES	(256)
#define MEMLOG_FILE_CMD_SIZE	((size_t)&((struct memlog_file_cmd *)0)->list)
#define MEMLOG_EVENT_CMD	"MLGCMD:C"
#define MEMLOG_EVENT_FILE	"MLGCMD:F"

#define MEMLOG_PROPERTY_SHIFT		(16)
#define MEMLOG_PROPERTY_NONCACHABLE	(0x1 << 16)
#define MEMLOG_PROPERTY_TIMESTAMP	(0x1 << 17)
#define MEMLOG_PROPERTY_UTC		(0x1 << 18)
#define MEMLOG_PROPERTY_LOG_FWD		(0x1 << 19)
#define MEMLOG_PROPERTY_SUPPORT_FILE	(0x1 << 31)

#define MEMLOG_BL_VALID_MAGIC		(0x1090BABA)
#define MEMLOG_BL_INVALID_MAGIC		(0x10900DE1)

static void * memlog_memcpy_align_4(void *dest, const void *src,
							size_t n)
{
	u32 *dp = dest;
	const u32 *sp = src;
	int i;

	if (n % 4)
		return NULL;

	n = n >> 2;

	for (i = 0; i < (int)n; i++)
		*dp++ = *sp++;

	return dest;
}

static void * memlog_memcpy_align_8(void *dest, const void *src,
							size_t n)
{
	u64 *dp = dest;
	const u64 *sp = src;
	int i;

	if (n % 8)
		return NULL;

	n = n >> 3;

	for (i = 0; i < (int)n; i++)
		*dp++ = *sp++;

	return dest;
}

static void memlog_make_event(const char *cmd)
{
	struct memlog_event *evt;

	if (!main_desc.mem_to_file_allow)
		return;

	evt = kzalloc(sizeof(*evt), GFP_KERNEL);
	if (!evt) {
		dev_err(main_desc.dev, "%s: kzalloc failed\n", __func__);
		return;
	}
	strncpy(evt->cmd, cmd, sizeof(evt->cmd) - 1);

	mutex_lock(&main_desc.event_lock);
	list_add(&evt->list, &main_desc.event_list);
	mutex_unlock(&main_desc.event_lock);

	wake_up_interruptible(&main_desc.event_wait_q);
}

static inline void memlog_update_lost_data(struct memlog_obj_prv *prvobj,
							long amount)
{
	int total_cnt;
	long total_amount;

	total_cnt = atomic_inc_return(&prvobj->lost_data_cnt);
	total_amount = atomic_long_add_return(amount,
						&prvobj->lost_data_amount);

	dev_info(main_desc.dev,
		"%s: obj(%s) data lost total cnt(%d) total amount (%ld)\n",
		__func__, prvobj->obj_name, total_cnt, total_amount);
}

static inline void memlog_queue_work(struct work_struct *work)
{
	if (main_desc.wq && !main_desc.in_panic)
		queue_work(main_desc.wq, work);
}

static void memlog_file_push_cmd(struct memlog_file_cmd *cmd)
{
	if (!main_desc.mem_to_file_allow)
		return;

	mutex_lock(&main_desc.file_cmd_lock);
	list_add(&cmd->list, &main_desc.file_cmd_list);
	mutex_unlock(&main_desc.file_cmd_lock);
	memlog_make_event(MEMLOG_EVENT_CMD);
}

static struct memlog_file_cmd *memlog_file_get_cmd(void)
{
	struct memlog_file_cmd *cmd = NULL;

	mutex_lock(&main_desc.file_cmd_lock);
	if (!list_empty(&main_desc.file_cmd_list)) {
		cmd = list_last_entry(&main_desc.file_cmd_list,
					typeof(*cmd), list);
	}
	mutex_unlock(&main_desc.file_cmd_lock);

	return cmd;
}

static int memlog_file_pop_cmd(struct memlog_file_cmd *del_cmd)
{
	struct memlog_file_cmd *cmd = NULL;

	mutex_lock(&main_desc.file_cmd_lock);
	if (!list_empty(&main_desc.file_cmd_list)) {
		cmd = list_last_entry(&main_desc.file_cmd_list,
					typeof(*cmd), list);
		if (!memcmp(del_cmd, cmd, MEMLOG_FILE_CMD_SIZE)) {
			list_del(&cmd->list);
			kfree(cmd);
			mutex_unlock(&main_desc.file_cmd_lock);
			return 0;
		}
	}
	mutex_unlock(&main_desc.file_cmd_lock);

	return -1;
}

static void _memlog_file_work(struct memlog_obj_prv *prvobj, bool sync)
{
	if (!prvobj->is_full)
		return;

	_memlog_write_file(prvobj->file_obj,
				prvobj->file_buffer,
				prvobj->buffered_data_size,
				sync);

	prvobj->is_full = false;
}

static void memlog_file_work(struct work_struct *work)
{
	struct memlog_obj_prv *prvobj = container_of(work,
						struct memlog_obj_prv,
						file_work);

	_memlog_file_work(prvobj, false);
}

static void memlog_file_work_sync(struct work_struct *work)
{
	struct memlog_obj_prv *prvobj = container_of(work,
						struct memlog_obj_prv,
						file_work_sync);

	_memlog_file_work(prvobj, true);
}

static void memlog_clean_prvobj_work(struct work_struct *work)
{
	struct memlog_obj_prv *prvobj = NULL;

	do {
		prvobj = NULL;
		mutex_lock(&main_desc.zombie_obj_list_lock);
		if (!list_empty(&main_desc.zombie_obj_list)) {
			prvobj = list_last_entry(&main_desc.zombie_obj_list,
							typeof(*prvobj), list);
			list_del(&prvobj->list);
		}
		mutex_unlock(&main_desc.zombie_obj_list_lock);

		if (prvobj) {
			msleep(50);
			cdev_del(&prvobj->cdev);
			device_unregister(prvobj->obj.file->dev);
			ida_simple_remove(&main_desc.memlog_minor_id,
						prvobj->obj.file->minor);
			kfree(prvobj->obj.file);
			kfree(prvobj);
		}
	} while (prvobj != NULL);
}

int memlog_write(struct memlog_obj *obj, int log_level,
				void *src, size_t size)
{
	struct memlog_obj_prv *prvobj = obj_to_data(obj);
	struct memlog_obj_prv *file_prvobj;
	void *dest;
	unsigned long flags;

	if (!obj->enabled || (log_level > obj->log_level) ||
			(obj->log_type != MEMLOG_TYPE_DEFAULT))
		return -EPERM;

	if (prvobj->support_logfwd &&
		(log_level <= MEMLOG_LOGFWD_LEVEL) &&
		(prvobj->to_string)) {
		loff_t pos = 0;
		struct memlog_prv *prvdesc = desc_to_data(obj->parent);

		prvobj->to_string(src, size, prvobj->string_buf,
				MEMLOG_STRING_BUF_SIZE - 1, &pos);
		if (pos < MEMLOG_STRING_BUF_SIZE) {
			((char *)prvobj->string_buf)[pos] = 0;
			dev_err(prvdesc->dev, "%s", prvobj->string_buf);
		} else {
			dev_err(prvdesc->dev, "%s: to string err\n", __func__);
		}
	}

	raw_spin_lock_irqsave(&prvobj->log_lock, flags);

	if (size > obj->size) {
		raw_spin_unlock_irqrestore(&prvobj->log_lock, flags);
		return -ENOMEM;
	}

	if ((u64)(prvobj->curr_ptr + size) >
			(u64)(obj->vaddr + obj->size)) {
		void *curr_ptr;

		file_prvobj = obj_to_data(prvobj->file_obj);
		if (prvobj->support_file &&
			atomic_read(&file_prvobj->open_cnt)) {
			if (!prvobj->is_full) {
				prvobj->buffered_data_size =
							(u64)prvobj->curr_ptr -
							(u64)prvobj->read_ptr;
				memcpy(prvobj->file_buffer, prvobj->read_ptr,
					prvobj->buffered_data_size);
				prvobj->is_full = true;
				memlog_queue_work(&prvobj->file_work);
			} else {
				memlog_update_lost_data(file_prvobj,
							(u64)prvobj->curr_ptr -
							(u64)prvobj->read_ptr);
			}
			prvobj->read_ptr = obj->vaddr;
		}

		dest = obj->vaddr;
		curr_ptr = prvobj->curr_ptr;
		prvobj->curr_ptr = obj->vaddr + size;
		prvobj->eob = true;

		memset(curr_ptr, 0x0,
			((u64)(obj->vaddr) + obj->size) - (u64)(curr_ptr));
		memcpy(dest, src, size);
	} else {
		dest = prvobj->curr_ptr;
		prvobj->curr_ptr += size;
		memcpy(dest, src, size);
		file_prvobj = obj_to_data(prvobj->file_obj);

		if ((prvobj->support_file &&
			atomic_read(&file_prvobj->open_cnt)) &&
			!prvobj->is_full &&
			((u64)prvobj->curr_ptr - (u64)prvobj->read_ptr >
			MEMLOG_FILE_THRESHOLD_SIZE)) {
				prvobj->buffered_data_size =
							(u64)prvobj->curr_ptr -
							(u64)prvobj->read_ptr;
				memcpy(prvobj->file_buffer, prvobj->read_ptr,
					prvobj->buffered_data_size);
				prvobj->is_full = true;
				memlog_queue_work(&prvobj->file_work);
				prvobj->read_ptr = prvobj->curr_ptr;
		}
	}

	raw_spin_unlock_irqrestore(&prvobj->log_lock, flags);

	return 0;
}
EXPORT_SYMBOL(memlog_write);


int memlog_write_array(struct memlog_obj *obj, int log_level, void *src)
{
	struct memlog_obj_prv *prvobj = obj_to_data(obj);
	u64 idx;
	void *dest;
	unsigned long flags;

	if (!obj->enabled || (log_level > obj->log_level) ||
			(obj->log_type != MEMLOG_TYPE_ARRAY))
		return -EPERM;

	if (prvobj->support_ktime)
		*(u64 *)src = local_clock();

	if (prvobj->support_utc) {
		struct timespec ts_rtc;
		struct rtc_time tm;

		getnstimeofday(&ts_rtc);
		rtc_time_to_tm(ts_rtc.tv_sec - (sys_tz.tz_minuteswest * 60),
									&tm);
		tm.tm_year += 1900;
		tm.tm_mon += 1;

		if (prvobj->support_ktime)
			memcpy(src + 8, &tm, sizeof(tm));
		else
			memcpy(src, &tm, sizeof(tm));
	}

	if (prvobj->support_logfwd &&
		(log_level <= MEMLOG_LOGFWD_LEVEL) &&
		(prvobj->to_string)) {
		loff_t pos = 0;
		struct memlog_prv *prvdesc = desc_to_data(obj->parent);

		prvobj->to_string(src, prvobj->array_unit_size,
					prvobj->string_buf,
					MEMLOG_STRING_BUF_SIZE - 1, &pos);
		if (pos < MEMLOG_STRING_BUF_SIZE) {
			((char *)prvobj->string_buf)[pos] = 0;
			dev_err(prvdesc->dev, "%s", prvobj->string_buf);
		} else {
			dev_err(prvdesc->dev, "%s: to string err\n", __func__);
		}
	}

	raw_spin_lock_irqsave(&prvobj->log_lock, flags);
	idx = prvobj->log_idx++;

	if (!prvobj->support_file) {
		if (prvobj->log_idx >= prvobj->max_idx) {
			prvobj->log_idx = 0;
			prvobj->eob = true;
		}
		raw_spin_unlock_irqrestore(&prvobj->log_lock, flags);
	}

	dest = obj->vaddr + (idx * prvobj->array_unit_size);

	if (!(prvobj->array_unit_size % 8))
		memlog_memcpy_align_8(dest, src, prvobj->array_unit_size);
	else if (!(prvobj->array_unit_size % 4))
		memlog_memcpy_align_4(dest, src, prvobj->array_unit_size);
	else
		memcpy(dest, src, prvobj->array_unit_size);

	if (prvobj->support_file) {
		struct memlog_obj_prv *file_prvobj =
					obj_to_data(prvobj->file_obj);

		if (prvobj->log_idx >= prvobj->max_idx) {
			if (atomic_read(&file_prvobj->open_cnt)) {
				if (!prvobj->is_full) {
					prvobj->buffered_data_size =
						obj->size -
						(prvobj->array_unit_size *
						prvobj->read_log_idx);

					prvobj->buffered_data_size =
						min(prvobj->buffered_data_size,
						prvobj->file_buffer_size);

					memcpy(prvobj->file_buffer,
						obj->vaddr +
						(prvobj->array_unit_size *
						prvobj->read_log_idx),
						prvobj->buffered_data_size);
					prvobj->is_full = true;
					memlog_queue_work(&prvobj->file_work);
				} else {
					memlog_update_lost_data(file_prvobj,
						obj->size -
						(prvobj->array_unit_size *
						prvobj->read_log_idx));
				}
			}
			prvobj->read_log_idx = 0;
			prvobj->log_idx = 0;
			prvobj->eob = true;
		} else if (atomic_read(&file_prvobj->open_cnt) &&
							!prvobj->is_full) {
			if (((prvobj->log_idx - prvobj->read_log_idx) *
				prvobj->array_unit_size) >
				MEMLOG_FILE_THRESHOLD_SIZE) {
				prvobj->buffered_data_size =
					(prvobj->log_idx -
					prvobj->read_log_idx) *
					prvobj->array_unit_size;

				prvobj->buffered_data_size =
						min(prvobj->buffered_data_size,
						prvobj->file_buffer_size);

				memcpy(prvobj->file_buffer,
					obj->vaddr +
					(prvobj->array_unit_size *
					prvobj->read_log_idx),
					prvobj->buffered_data_size);
				prvobj->is_full = true;
				memlog_queue_work(&prvobj->file_work);
				prvobj->read_log_idx = prvobj->log_idx;
			}
		}
		raw_spin_unlock_irqrestore(&prvobj->log_lock, flags);
	}

	return 0;
}
EXPORT_SYMBOL(memlog_write_array);

int memlog_write_printf(struct memlog_obj *obj, int log_level,
					const char *fmt, ...)
{
	va_list args;
	int ret;

	va_start(args, fmt);
	ret = memlog_write_vsprintf(obj, log_level, fmt, args);
	va_end(args);
	return ret;
}
EXPORT_SYMBOL(memlog_write_printf);

int memlog_write_vsprintf(struct memlog_obj *obj, int log_level,
					const char *fmt, va_list args)
{
	struct memlog_obj_prv *prvobj = obj_to_data(obj);
	struct memlog_obj_prv *file_prvobj;
	unsigned int log_len = 0;
	void *dest;
	u64 tv_kernel;
	unsigned long rem_nsec;
	unsigned long flags;

	if (!obj->enabled || (log_level > obj->log_level) ||
			(obj->log_type != MEMLOG_TYPE_STRING))
		return -EPERM;

	if (prvobj->support_logfwd && (log_level <= MEMLOG_LOGFWD_LEVEL)) {
		struct va_format vaf;
		struct memlog_prv *prvdesc = desc_to_data(obj->parent);

		vaf.fmt = fmt;
		vaf.va = &args;
		dev_err(prvdesc->dev, "%pV", &vaf);
	}

	raw_spin_lock_irqsave(&prvobj->log_lock, flags);

	if (prvobj->support_ktime) {
		tv_kernel = local_clock();
		rem_nsec = do_div(tv_kernel, 1000000000);

		log_len += scnprintf(prvobj->string_buf + log_len,
				MEMLOG_STRING_BUF_SIZE - log_len,
				"[%5lu.%06lu][%d] ", (unsigned long)tv_kernel,
				rem_nsec / 1000, log_level);
	}

	if (prvobj->support_utc) {
		struct timespec ts_rtc;
		struct rtc_time tm;

		getnstimeofday(&ts_rtc);
		rtc_time_to_tm(ts_rtc.tv_sec - (sys_tz.tz_minuteswest * 60),
									&tm);
		log_len += scnprintf(prvobj->string_buf + log_len,
				MEMLOG_STRING_BUF_SIZE - log_len,
				"%d-%02d-%02d %02d:%02d:%02d ",
				tm.tm_year + 1900, tm.tm_mon + 1, tm.tm_mday,
				tm.tm_hour, tm.tm_min, tm.tm_sec);
	}

	log_len += vscnprintf(prvobj->string_buf + log_len,
			MEMLOG_STRING_BUF_SIZE - log_len, fmt, args);

	if (log_len > obj->size) {
		raw_spin_unlock_irqrestore(&prvobj->log_lock, flags);
		return -ENOMEM;
	}

	if ((u64)(prvobj->curr_ptr + log_len) >
			(u64)(obj->vaddr + obj->size)) {
		void *curr_ptr;

		file_prvobj = obj_to_data(prvobj->file_obj);
		if (prvobj->support_file &&
			atomic_read(&file_prvobj->open_cnt)) {
			if (!prvobj->is_full) {
				prvobj->buffered_data_size =
							(u64)prvobj->curr_ptr -
							(u64)prvobj->read_ptr;

				prvobj->buffered_data_size =
						min(prvobj->buffered_data_size,
						prvobj->file_buffer_size);

				memcpy(prvobj->file_buffer, prvobj->read_ptr,
					prvobj->buffered_data_size);
				prvobj->is_full = true;
				memlog_queue_work(&prvobj->file_work);
			} else {
				memlog_update_lost_data(file_prvobj,
							(u64)prvobj->curr_ptr -
							(u64)prvobj->read_ptr);
			}
			prvobj->read_ptr = obj->vaddr;
		}

		dest = obj->vaddr;
		curr_ptr = prvobj->curr_ptr;
		prvobj->curr_ptr = obj->vaddr + log_len;
		prvobj->eob = true;

		memset(curr_ptr, 0x0,
			((u64)(obj->vaddr) + obj->size) - (u64)(curr_ptr));
		memcpy(dest, prvobj->string_buf, log_len);
	} else {
		dest = prvobj->curr_ptr;
		prvobj->curr_ptr += log_len;
		memcpy(dest, prvobj->string_buf, log_len);
		file_prvobj = obj_to_data(prvobj->file_obj);

		if ((prvobj->support_file &&
			atomic_read(&file_prvobj->open_cnt)) &&
			!prvobj->is_full &&
			((u64)prvobj->curr_ptr - (u64)prvobj->read_ptr >
			MEMLOG_FILE_THRESHOLD_SIZE)) {
				prvobj->buffered_data_size =
							(u64)prvobj->curr_ptr -
							(u64)prvobj->read_ptr;

				prvobj->buffered_data_size =
						min(prvobj->buffered_data_size,
						prvobj->file_buffer_size);

				memcpy(prvobj->file_buffer, prvobj->read_ptr,
					prvobj->buffered_data_size);
				prvobj->is_full = true;
				memlog_queue_work(&prvobj->file_work);
				prvobj->read_ptr = prvobj->curr_ptr;
		}
	}

	raw_spin_unlock_irqrestore(&prvobj->log_lock, flags);

	return 0;
}
EXPORT_SYMBOL(memlog_write_vsprintf);

int memlog_do_dump(struct memlog_obj *obj, int log_level)
{
	struct memlog_obj_prv *prvobj = obj_to_data(obj);
	unsigned long flags;

	if (!obj->enabled || (log_level > obj->log_level) ||
			(obj->log_type != MEMLOG_TYPE_DUMP))
		return -EPERM;

	raw_spin_lock_irqsave(&prvobj->log_lock, flags);

	memcpy_fromio(obj->vaddr, prvobj->base_ptr, obj->size);

	if (prvobj->support_file) {
		if (!prvobj->is_full) {
			prvobj->buffered_data_size = obj->size;
			memcpy(prvobj->file_buffer, obj->vaddr,
					prvobj->buffered_data_size);
			prvobj->is_full = true;
			memlog_queue_work(&prvobj->file_work);
		} else {
			memlog_update_lost_data(obj_to_data(prvobj->file_obj),
								obj->size);
		}
	}

	raw_spin_unlock_irqrestore(&prvobj->log_lock, flags);

	return 0;
}
EXPORT_SYMBOL(memlog_do_dump);

int memlog_dump_direct_to_file(struct memlog_obj *obj, int log_level)
{
	struct memlog_obj_prv *prvobj = obj_to_data(obj);
	unsigned long flags;
	int ret = 0;

	if (!obj->enabled || (log_level > obj->log_level) ||
			(obj->log_type != MEMLOG_TYPE_DIRECT))
		return -EPERM;

	raw_spin_lock_irqsave(&prvobj->log_lock, flags);
	if (prvobj->support_file) {
		if (!prvobj->is_full) {
			prvobj->buffered_data_size = obj->size;
			memcpy(prvobj->file_buffer, obj->vaddr,
				prvobj->buffered_data_size);
			prvobj->is_full = true;
			memlog_queue_work(&prvobj->file_work);
		} else {
			ret = -ENOSPC;
			memlog_update_lost_data(obj_to_data(prvobj->file_obj),
								obj->size);
		}
	} else {
		ret = -ENOMEM;
	}

	raw_spin_unlock_irqrestore(&prvobj->log_lock, flags);

	return ret;
}
EXPORT_SYMBOL(memlog_dump_direct_to_file);

static size_t _memlog_write_file(struct memlog_obj *obj,
				void *src, size_t size, bool sync)
{
	struct memlog_obj_prv *prvobj = obj_to_data(obj);
	size_t n = 0;
	size_t copy_size = 0;
	size_t tmp_size;

	if (!obj->enabled || prvobj->is_dead || !main_desc.mem_to_file_allow)
		return 0;

	mutex_lock(&prvobj->file_mutex);
	if ((prvobj->source_obj == NULL ||
		(prvobj->source_obj->log_type == MEMLOG_TYPE_DUMP) ||
		(prvobj->source_obj->log_type == MEMLOG_TYPE_DIRECT) ||
		(atomic_read(&prvobj->open_cnt) != 0)) && !prvobj->is_full) {

		copy_size = min(size, obj->size - prvobj->remained);
		if (copy_size < size)
			memlog_update_lost_data(prvobj,
						(long)(size - copy_size));

		if ((u64)(obj->vaddr + obj->size) <
			(u64)(prvobj->curr_ptr + copy_size)) {
			tmp_size =  (u64)(obj->vaddr + obj->size) -
					(u64)(prvobj->curr_ptr);

			memcpy(prvobj->curr_ptr, src, tmp_size);
			n += tmp_size;
			src += tmp_size;
			prvobj->remained += tmp_size;
			prvobj->curr_ptr = obj->vaddr;
			copy_size -= tmp_size;
		}

		memcpy(prvobj->curr_ptr, src, copy_size);
		n += copy_size;
		src += copy_size;
		prvobj->remained += copy_size;
		prvobj->curr_ptr += copy_size;

		if (prvobj->remained == obj->size)
			prvobj->is_full = true;
		if (prvobj->curr_ptr == (obj->vaddr + obj->size))
			prvobj->curr_ptr = obj->vaddr;
		if (prvobj->remained >= prvobj->file_event_threshold)
			sync = true;
	}
	mutex_unlock(&prvobj->file_mutex);

	if (sync)
		memlog_make_event(MEMLOG_EVENT_FILE);

	return n;
}

size_t memlog_write_file(struct memlog_obj *obj, void *src, size_t size)
{
	return _memlog_write_file(obj, src, size, true);
}
EXPORT_SYMBOL(memlog_write_file);

int memlog_sync_to_file(struct memlog_obj *obj)
{
	struct memlog_obj_prv *prvobj = obj_to_data(obj);
	struct memlog_obj_prv *file_prvobj;
	unsigned long flags;
	int ret = 0;

	if (!obj->enabled)
		return -EPERM;

	if (!prvobj->support_file) {
		dev_err(main_desc.dev, "%s: obj of %s is not source of file\n",
				__func__,
				desc_to_data(obj->parent)->desc.dev_name);
		return -EINVAL;
	}

	switch (obj->log_type) {
	case MEMLOG_TYPE_DEFAULT:
	case MEMLOG_TYPE_ARRAY:
	case MEMLOG_TYPE_STRING:
		break;
	default:
		dev_err(main_desc.dev, "%s: obj of %s is invalid type(%u)\n",
				__func__,
				desc_to_data(obj->parent)->desc.dev_name,
				obj->log_type);
		return -EINVAL;
		break;
	}

	raw_spin_lock_irqsave(&prvobj->log_lock, flags);
	file_prvobj = obj_to_data(prvobj->file_obj);

	if (atomic_read(&file_prvobj->open_cnt) && !prvobj->is_full) {
		void *src;

		if (obj->log_type == MEMLOG_TYPE_ARRAY) {
			prvobj->buffered_data_size = (prvobj->log_idx -
						prvobj->read_log_idx) *
						prvobj->array_unit_size;
			src = obj->vaddr + (prvobj->read_log_idx *
						prvobj->array_unit_size);
			prvobj->read_log_idx = prvobj->log_idx;
		} else {
			prvobj->buffered_data_size = (u64)prvobj->curr_ptr -
							(u64)prvobj->read_ptr;
			src = prvobj->read_ptr;
			prvobj->read_ptr = prvobj->curr_ptr;
		}

		if (prvobj->buffered_data_size) {
			memcpy(prvobj->file_buffer, src,
				prvobj->buffered_data_size);
			prvobj->is_full = true;
			memlog_queue_work(&prvobj->file_work_sync);
		}
	} else {
		dev_err(file_prvobj->obj.file->dev, "%s:file is not opened or "
						"under cpy process", __func__);
		ret = -ENOENT;
	}

	raw_spin_unlock_irqrestore(&prvobj->log_lock, flags);

	return ret;
}
EXPORT_SYMBOL(memlog_sync_to_file);

int memlog_manual_sync_to_file(struct memlog_obj *obj,
				void *base, size_t size)
{
	struct memlog_obj_prv *prvobj = obj_to_data(obj);
	struct memlog_obj_prv *file_prvobj;
	unsigned long flags;
	int ret = 0;

	if (!obj->enabled)
		return -EPERM;

	if (!prvobj->support_file) {
		dev_err(main_desc.dev, "%s: obj of %s is not source of file\n",
				__func__,
				desc_to_data(obj->parent)->desc.dev_name);
		return -EINVAL;
	}

	switch (obj->log_type) {
	case MEMLOG_TYPE_DIRECT:
		break;
	default:
		dev_err(main_desc.dev, "%s: obj of %s is invalid type(%u)\n",
				__func__,
				desc_to_data(obj->parent)->desc.dev_name,
				obj->log_type);
		return -EINVAL;
		break;
	}

	if (((u64)obj->size < size) || ((u64)obj->vaddr > (u64)base) ||
			(((u64)obj->vaddr + (u64)obj->size) <= (u64)base)) {
		dev_err(main_desc.dev, "%s: invalid base(0x%lx)"
					" or size(0x%lx)\n",
					__func__, (unsigned long)base,
					(unsigned long)size);
		return -EINVAL;
	}


	raw_spin_lock_irqsave(&prvobj->log_lock, flags);
	file_prvobj = obj_to_data(prvobj->file_obj);

	if (!prvobj->is_full) {
		u64 end = (u64)obj->vaddr + (u64)obj->size;
		u64 remain;

		prvobj->buffered_data_size = size;
		if (((u64)base + (u64)size) > end) {
			memcpy(prvobj->file_buffer, base, end - (u64)base);
			remain = size - (end - (u64)base);
			memcpy(prvobj->file_buffer + (end - (u64)base),
							obj->vaddr, remain);
		} else {
			memcpy(prvobj->file_buffer, base, size);
		}
		prvobj->is_full = true;
		memlog_queue_work(&prvobj->file_work_sync);
	} else {
		ret = -ENOSPC;
		dev_info(prvobj->file_obj->file->dev,
			"%s: under cpy process\n",
			__func__);
	}

	raw_spin_unlock_irqrestore(&prvobj->log_lock, flags);

	return ret;
}
EXPORT_SYMBOL(memlog_manual_sync_to_file);

bool memlog_is_data_remaining(struct memlog_obj *obj)
{
	struct memlog_obj_prv *prvobj = obj_to_data(obj);
	bool ret = false;

	if (obj->log_type == MEMLOG_TYPE_FILE) {
		if (prvobj->remained) {
			ret = true;
			dev_info(main_desc.dev,
				"%s: obj(%s) has remaining data(%lx)\n",
				__func__, prvobj->obj_name, prvobj->remained);
			goto out;
		}

		if (prvobj->source_obj) {
			struct memlog_obj_prv *s_prvobj =
					obj_to_data(prvobj->source_obj);

			if (s_prvobj->is_full) {
				dev_info(main_desc.dev,
					"%s: obj(%s)'s source obj(%s) full\n",
					__func__, prvobj->obj_name,
					s_prvobj->obj_name);
				ret = true;
			}
		}
	} else {
		if (prvobj->is_full) {
			dev_info(main_desc.dev,
				"%s: obj(%s) full\n", __func__,
				prvobj->obj_name);
			ret = true;
		}
	}

out:
	return ret;
}
EXPORT_SYMBOL(memlog_is_data_remaining);

int memlog_set_file_event_threshold(struct memlog_obj *obj, size_t threshold)
{
	struct memlog_obj_prv *prvobj = obj_to_data(obj);

	if (obj->log_type != MEMLOG_TYPE_FILE) {
		dev_err(main_desc.dev, "%s: type invalid\n", __func__);
		return -EINVAL;
	}

	if (obj->size < threshold) {
		dev_err(main_desc.dev, "%s: invalid size\n", __func__);
		return -EINVAL;
	}

	prvobj->file_event_threshold = threshold;
	return 0;
}
EXPORT_SYMBOL(memlog_set_file_event_threshold);

static int memlog_open(struct inode *inode, struct file *filep)
{
	struct memlog_obj_prv *prvobj = container_of(inode->i_cdev,
						struct memlog_obj_prv, cdev);
	int open_cnt;

	if (prvobj->is_dead)
		return -EINVAL;

	open_cnt = atomic_inc_return(&prvobj->open_cnt);
	filep->private_data = prvobj;

	dev_info(prvobj->obj.file->dev, "%s: called(%d)\n",
					__func__, open_cnt);
	return 0;
}

static void memlog_free_file_prvobj(struct memlog_obj_prv *prvobj)
{
	struct memlog_obj_prv *pos, *n;

	mutex_lock(&main_desc.dead_obj_list_lock);
	list_for_each_entry_safe(pos, n, &main_desc.dead_obj_list, list) {
		if (prvobj == pos) {
			list_del(&pos->list);
			break;
		}
	}
	mutex_unlock(&main_desc.dead_obj_list_lock);

	if (prvobj == pos) {
		mutex_lock(&main_desc.zombie_obj_list_lock);
		list_add(&prvobj->list, &main_desc.zombie_obj_list);
		mutex_unlock(&main_desc.zombie_obj_list_lock);
		memlog_queue_work(&main_desc.clean_prvobj_work);
	}
}

static int memlog_release(struct inode *inode, struct file *filep)
{
	struct memlog_obj_prv *prvobj = filep->private_data;
	int open_cnt;

	if (prvobj->is_dead) {
		do {
			udelay(1);
		} while (prvobj->obj.vaddr != NULL);
	}

	open_cnt = atomic_dec_return(&prvobj->open_cnt);
	dev_info(main_desc.dev, "%s: %s released(%d)\n",
			__func__, prvobj->obj_name, open_cnt);

	if (prvobj->is_dead && open_cnt == 0)
		memlog_free_file_prvobj(prvobj);

	return 0;
}

static ssize_t memlog_read(struct file *filep, char __user *buf,
					size_t count, loff_t *pos)
{
	struct memlog_obj_prv *prvobj = filep->private_data;
	ssize_t n = 0;
	size_t copy_size = 0;
	size_t tmp_size;

	if (prvobj->obj.enabled == false)
		goto out;

	mutex_lock(&prvobj->file_mutex);
	if (prvobj->remained) {
		copy_size = min(count, prvobj->remained);
		if ((u64)(prvobj->obj.vaddr + prvobj->obj.size) <
				(u64)(prvobj->read_ptr + copy_size)) {
			tmp_size =
				(u64)(prvobj->obj.vaddr + prvobj->obj.size) -
				(u64)(prvobj->read_ptr);
			if (copy_to_user(buf, prvobj->read_ptr, tmp_size)) {
				dev_err(main_desc.dev, "%s: fail to copy "
							"to user\n", __func__);
				goto out_unlock;
			}

			n += tmp_size;
			buf += tmp_size;
			*pos += tmp_size;
			prvobj->remained -= tmp_size;
			prvobj->read_ptr = prvobj->obj.vaddr;
			copy_size -= tmp_size;
		}
		if (copy_to_user(buf,  prvobj->read_ptr, copy_size)) {
			dev_err(main_desc.dev, "%s: fail to copy to user\n",
								__func__);
			goto out_unlock;
		}
		n += copy_size;
		buf += copy_size;
		*pos += copy_size;
		prvobj->remained -= copy_size;
		prvobj->read_ptr += copy_size;
		if (prvobj->read_ptr == (prvobj->obj.vaddr + prvobj->obj.size))
			prvobj->read_ptr = prvobj->obj.vaddr;

		if (n)
			prvobj->is_full = false;
	}
out_unlock:
	mutex_unlock(&prvobj->file_mutex);
out:
	return n;
}

static unsigned int memlog_poll(struct file *filep,
					struct poll_table_struct *wait)
{
	struct memlog_obj_prv *prvobj = filep->private_data;
	unsigned int mask = 0;

	if (prvobj->is_dead)
		return POLLERR;

	if (prvobj->obj.enabled == false)
		return 0;

	mutex_lock(&prvobj->file_mutex);
	if (prvobj->remained)
		mask |= (POLLIN | POLLRDNORM);
	mutex_unlock(&prvobj->file_mutex);

	return mask;
}

static const struct file_operations memlog_file_ops = {
	.open = memlog_open,
	.release = memlog_release,
	.read = memlog_read,
	.poll = memlog_poll
};

static int memlog_event_open(struct inode *inode, struct file *filep)
{
	dev_info(main_desc.event_dev, "%s: called\n", __func__);
	return 0;
}

static int memlog_event_release(struct inode *inode, struct file *filep)
{
	dev_info(main_desc.event_dev, "%s: called\n", __func__);
	return 0;
}

static ssize_t memlog_event_read(struct file *filep, char __user *buf,
						size_t count, loff_t *pos)
{
	ssize_t n = 0;
	struct memlog_event *evt = NULL;
	size_t copy_size;

	mutex_lock(&main_desc.event_lock);
	if (!list_empty(&main_desc.event_list)) {
		evt = list_last_entry(&main_desc.event_list,
					typeof(*evt), list);
		list_del(&evt->list);
	}
	mutex_unlock(&main_desc.event_lock);

	if (evt) {
		copy_size = min(strlen(evt->cmd), count);
		if (copy_to_user(buf, evt->cmd, copy_size)) {
			dev_err(main_desc.dev, "%s: fail to copy to user\n",
								__func__);
		} else {
			*pos += copy_size;
			n = copy_size;
		}
		kfree(evt);
	}

	return n;
}

static unsigned int memlog_event_poll(struct file *filep,
					struct poll_table_struct *wait)
{
	unsigned int mask = 0;

	mutex_lock(&main_desc.event_lock);
	if (!list_empty(&main_desc.event_list))
		mask |= (POLLIN | POLLRDNORM);
	mutex_unlock(&main_desc.event_lock);

	if (!mask) {
		poll_wait(filep, &main_desc.event_wait_q, wait);
		mutex_lock(&main_desc.event_lock);
		if (!list_empty(&main_desc.event_list))
			 mask |= (POLLIN | POLLRDNORM);
		mutex_unlock(&main_desc.event_lock);
	}

	return mask;
}

static const struct file_operations memlog_event_ops = {
	.open = memlog_event_open,
	.release = memlog_event_release,
	.read = memlog_event_read,
	.poll = memlog_event_poll
};

static ssize_t memlog_obj_level_store(struct kobject *kobj,
					struct kobj_attribute *attr,
					const char *buf, size_t count)
{
	struct memlog_obj_prv *prvobj = container_of(attr,
					struct memlog_obj_prv, level_ka);
	struct memlog *desc = prvobj->obj.parent;

	if (count && (buf[0] >= '0' && buf[0] <= ('0' + MEMLOG_LEVEL_DEBUG)))
		prvobj->obj.log_level = buf[0] - '0';

	if (desc->ops.log_level_notify != NULL)
		desc->ops.log_level_notify(&prvobj->obj, 0);

	return count;
}

static ssize_t memlog_obj_level_show(struct kobject *kobj,
				struct kobj_attribute *attr, char *buf)
{
	struct memlog_obj_prv *prvobj = container_of(attr,
					struct memlog_obj_prv, level_ka);
	ssize_t n = 0;

	n += scnprintf(buf, PAGE_SIZE - 1, "%d\n", prvobj->obj.log_level);

	return n;
}

static ssize_t memlog_obj_enable_store(struct kobject *kobj,
					struct kobj_attribute *attr,
					const char *buf, size_t count)
{
	struct memlog_obj_prv *prvobj = container_of(attr,
					struct memlog_obj_prv, enable_ka);
	struct memlog *desc = prvobj->obj.parent;

	if (count && (buf[0] >= '0' && buf[0] <= '1'))
		prvobj->obj.enabled = buf[0] - '0';

	if (desc->ops.log_enable_notify != NULL)
		desc->ops.log_enable_notify(&prvobj->obj, 0);

	return count;
}

static ssize_t memlog_obj_enable_show(struct kobject *kobj,
				struct kobj_attribute *attr, char *buf)
{
	struct memlog_obj_prv *prvobj = container_of(attr,
					struct memlog_obj_prv, enable_ka);
	ssize_t n = 0;

	n += scnprintf(buf, PAGE_SIZE - 1, "%d\n", prvobj->obj.enabled);

	return n;
}

static ssize_t memlog_obj_name_show(struct kobject *kobj,
				struct kobj_attribute *attr, char *buf)
{
	struct memlog_obj_prv *prvobj = container_of(attr,
					struct memlog_obj_prv, name_ka);
	ssize_t n = 0;

	n += scnprintf(buf, sizeof(prvobj->obj_name), "%s\n", prvobj->obj_name);

	return n;
}

static const char *memlog_get_typename(unsigned int log_type)
{
	const char *ptr;

	switch (log_type) {
	case MEMLOG_TYPE_DUMP:
		ptr = "dump";
		break;
	case MEMLOG_TYPE_DEFAULT:
		ptr = "default";
		break;
	case MEMLOG_TYPE_ARRAY:
		ptr = "array";
		break;
	case MEMLOG_TYPE_STRING:
		ptr = "printf";
		break;
	case MEMLOG_TYPE_DIRECT:
		ptr = "direct";
		break;
	case MEMLOG_TYPE_FILE:
		ptr = "file";
		break;
	default:
		ptr = "invalid";
		break;
	}

	return ptr;
}

static ssize_t memlog_obj_info_show(struct kobject *kobj,
				struct kobj_attribute *attr, char *buf)
{
	struct memlog_obj_prv *prvobj = container_of(attr,
					struct memlog_obj_prv, info_ka);
	ssize_t n = 0;
	const char *ptr;

	ptr = memlog_get_typename(prvobj->obj.log_type);
	n += scnprintf(buf + n, PAGE_SIZE - n - 1, "type=%s\n", ptr);

	if (prvobj->obj.log_type == MEMLOG_TYPE_ARRAY)
		n += scnprintf(buf + n, PAGE_SIZE - n - 1, "struct_name=%s\n",
							prvobj->struct_name);

	if (prvobj->file_obj) {
		struct memlog_obj_prv *f_prvobj =
				obj_to_data(prvobj->file_obj);

		n += scnprintf(buf + n, PAGE_SIZE - n - 1,
					"file_supported=y\n");
		n += scnprintf(buf + n, PAGE_SIZE - n - 1,
					"file_name=%s\n",
					f_prvobj->obj_name);
	}

	if (prvobj->obj.log_type == MEMLOG_TYPE_FILE)
		n += scnprintf(buf + n, PAGE_SIZE - n - 1,
					"file_minor_num=%d\n",
					prvobj->obj.file->minor);

	if ((prvobj->obj.log_type == MEMLOG_TYPE_FILE) &&
		(prvobj->source_obj != NULL)) {
		struct memlog_obj_prv *s_prvobj =
				obj_to_data(prvobj->source_obj);

		n += scnprintf(buf + n, PAGE_SIZE - n - 1,
					"source_obj_name=%s\n",
					s_prvobj->obj_name);
		ptr = memlog_get_typename(prvobj->source_obj->log_type);
		n += scnprintf(buf + n, PAGE_SIZE - n - 1,
				"source_obj_type=%s\n", ptr);

		if (prvobj->source_obj->log_type == MEMLOG_TYPE_ARRAY)
			n += scnprintf(buf + n, PAGE_SIZE - n - 1,
					"source_obj_struct_name=%s\n",
					s_prvobj->struct_name);
	}

	return n;
}

static ssize_t memlog_obj_sync_store(struct kobject *kobj,
					struct kobj_attribute *attr,
					const char *buf, size_t count)
{
	struct memlog_obj_prv *prvobj = container_of(attr,
					struct memlog_obj_prv, sync_ka);
	struct memlog *desc = prvobj->obj.parent;
	struct memlog_file_cmd *cmd;

	if (prvobj->obj.log_type != MEMLOG_TYPE_FILE)
		goto out;

	if (count) {
		switch (buf[0]) {
		case 'p':
		case 'r':
			cmd = kzalloc(sizeof(*cmd), GFP_KERNEL);
			if (!cmd)
				goto out;
			if (buf[0] == 'p')
				cmd->cmd = MEMLOG_FILE_CMD_PAUSE;
			else
				cmd->cmd = MEMLOG_FILE_CMD_RESUME;
			strncpy(cmd->file_name, prvobj->obj_name,
						sizeof(cmd->file_name));
			strncpy(cmd->desc_name, desc->dev_name,
						sizeof(cmd->desc_name));
			memlog_file_push_cmd(cmd);
			break;
		case 's':
			if (prvobj->source_obj)
				memlog_sync_to_file(prvobj->source_obj);
			break;
		default:
			break;
		}
	}

out:
	return count;
}

static ssize_t memlog_obj_to_string_show(struct kobject *kobj,
				struct kobj_attribute *attr, char *buf)
{
	struct memlog_obj_prv *prvobj = container_of(attr,
					struct memlog_obj_prv, to_string_ka);
	ssize_t n = 0;
	size_t available_src_size = 0;
	size_t parsed_data_size;

	if (prvobj->to_string == NULL) {
		n += scnprintf(buf, PAGE_SIZE - 1,
				"%s: There is no translation function\n",
				prvobj->obj_name);
		return n;
	}

	if (prvobj->obj.enabled == false)
		goto out;

	mutex_lock(&prvobj->file_mutex);
	if (prvobj->remained) {
		available_src_size = prvobj->remained;
		if ((u64)(prvobj->obj.vaddr + prvobj->obj.size) <
				(u64)(prvobj->read_ptr + available_src_size))
			available_src_size =
				(u64)(prvobj->obj.vaddr + prvobj->obj.size) -
				(u64)(prvobj->read_ptr);

		parsed_data_size = prvobj->to_string(prvobj->read_ptr,
						available_src_size, buf,
						PAGE_SIZE - 1, (loff_t *)&n);
		prvobj->remained -= parsed_data_size;
		prvobj->read_ptr += parsed_data_size;
		if (prvobj->read_ptr == (prvobj->obj.vaddr + prvobj->obj.size))
			prvobj->read_ptr = prvobj->obj.vaddr;
		if (n)
			prvobj->is_full = false;
	}
	mutex_unlock(&prvobj->file_mutex);
out:
	return n;

}

static int memlog_obj_create_sysfs(struct memlog_prv *prvdesc,
				struct memlog_obj_prv *prvobj, bool uaccess)
{
	int ret = -1;
	char name_buf[8];
	umode_t mode;

	if (uaccess)
		mode = 0666;
	else
		mode = 0660;

	scnprintf(name_buf, sizeof(name_buf) - 1, "%d", prvobj->obj_minor);
	prvobj->kobj = kobject_create_and_add(name_buf, prvdesc->kobj);

	if (!prvobj->kobj)
		goto out;

	sysfs_attr_init(&prvobj->enable_ka.attr);
	prvobj->enable_ka.attr.mode = mode;
	prvobj->enable_ka.attr.name = "enable";
	prvobj->enable_ka.show = memlog_obj_enable_show;
	prvobj->enable_ka.store = memlog_obj_enable_store;

	sysfs_attr_init(&prvobj->level_ka.attr);
	prvobj->level_ka.attr.mode = mode;
	prvobj->level_ka.attr.name = "level";
	prvobj->level_ka.show = memlog_obj_level_show;
	prvobj->level_ka.store = memlog_obj_level_store;

	sysfs_attr_init(&prvobj->name_ka.attr);
	prvobj->name_ka.attr.mode = mode;
	prvobj->name_ka.attr.name = "name";
	prvobj->name_ka.show = memlog_obj_name_show;
	prvobj->name_ka.store = NULL;

	sysfs_attr_init(&prvobj->info_ka.attr);
	prvobj->info_ka.attr.mode = mode;
	prvobj->info_ka.attr.name = "info";
	prvobj->info_ka.show = memlog_obj_info_show;
	prvobj->info_ka.store = NULL;

	sysfs_attr_init(&prvobj->sync_ka.attr);
	prvobj->sync_ka.attr.mode = mode;
	prvobj->sync_ka.attr.name = "sync";
	prvobj->sync_ka.show = NULL;
	prvobj->sync_ka.store = memlog_obj_sync_store;

	sysfs_attr_init(&prvobj->to_string_ka.attr);
	prvobj->to_string_ka.attr.mode = mode;
	prvobj->to_string_ka.attr.name = "to_string";
	prvobj->to_string_ka.show = memlog_obj_to_string_show;
	prvobj->to_string_ka.store = NULL;

	ret = sysfs_create_file(prvobj->kobj, &prvobj->enable_ka.attr);
	if (ret)
		goto out_fail_enable;

	ret = sysfs_create_file(prvobj->kobj, &prvobj->level_ka.attr);
	if (ret)
		goto out_fail_level;;

	ret = sysfs_create_file(prvobj->kobj, &prvobj->name_ka.attr);
	if (ret)
		goto out_fail_name;

	ret = sysfs_create_file(prvobj->kobj, &prvobj->info_ka.attr);
	if (ret)
		goto out_fail_info;

	if (prvobj->obj.log_type == MEMLOG_TYPE_FILE) {
		ret = sysfs_create_file(prvobj->kobj, &prvobj->sync_ka.attr);
		if (ret)
			goto out_fail_sync;

		ret = sysfs_create_file(prvobj->kobj,
					&prvobj->to_string_ka.attr);
		if (ret)
			goto out_fail_to_string;
	}

	goto out;
out_fail_to_string:
	sysfs_remove_file(prvobj->kobj, &prvobj->sync_ka.attr);
out_fail_sync:
	sysfs_remove_file(prvobj->kobj, &prvobj->info_ka.attr);
out_fail_info:
	sysfs_remove_file(prvobj->kobj, &prvobj->name_ka.attr);
out_fail_name:
	sysfs_remove_file(prvobj->kobj, &prvobj->level_ka.attr);
out_fail_level:
	sysfs_remove_file(prvobj->kobj, &prvobj->enable_ka.attr);
out_fail_enable:
	kobject_put(prvobj->kobj);
out:
	return ret;
}

static void memlog_obj_remove_sysfs(struct memlog_obj_prv *prvobj)
{
	sysfs_remove_file(prvobj->kobj, &prvobj->enable_ka.attr);
	sysfs_remove_file(prvobj->kobj, &prvobj->level_ka.attr);
	sysfs_remove_file(prvobj->kobj, &prvobj->name_ka.attr);
	sysfs_remove_file(prvobj->kobj, &prvobj->info_ka.attr);
	if (prvobj->obj.log_type == MEMLOG_TYPE_FILE) {
		sysfs_remove_file(prvobj->kobj, &prvobj->sync_ka.attr);
		sysfs_remove_file(prvobj->kobj, &prvobj->to_string_ka.attr);
	}
	kobject_put(prvobj->kobj);
}

void memlog_obj_set_sysfs_mode(struct memlog_obj *obj, bool uaccess)
{
	struct memlog_obj_prv *prvobj = obj_to_data(obj);
	struct memlog_prv *prvdesc = desc_to_data(obj->parent);
	struct memlog *desc = obj->parent;
	int ret;

	memlog_obj_remove_sysfs(prvobj);
	ret = memlog_obj_create_sysfs(prvdesc, prvobj, uaccess);

	dev_info(main_desc.dev, "%s: dest(%s) obj(%s) uaccess:%s ret[%d]\n",
				__func__, desc->dev_name, prvobj->obj_name,
				uaccess ? "true" : "false", ret);
}
EXPORT_SYMBOL(memlog_obj_set_sysfs_mode);

static void memlog_bl_add(struct memlog_prv *prvdesc,
			struct memlog_obj_prv *prvobj)
{
	int idx;
	struct memlog_bl *entry;

	if (!memlog_bl_obj)
		goto out;

	idx = atomic_inc_return(&main_desc.mlg_bl_idx);

	if (idx > (ARRAY_SIZE(*main_desc.mlg_bl) - 2)) {
		dev_err(main_desc.dev, "%s: fail to add bl entry(full)\n",
								__func__);
		goto out;
	}

	entry = &(*main_desc.mlg_bl)[idx];
	entry->magic = MEMLOG_BL_VALID_MAGIC;

	scnprintf(entry->name, 10,
			"%s-", prvdesc->desc.dev_name);
	scnprintf(entry->name + strlen(entry->name), 9,
		"%s", prvobj->obj_name);

	entry->paddr = (u64)prvobj->obj.paddr;
	entry->size = (u64)prvobj->obj.size;
	prvobj->mlg_bl_entry = entry;

out:
	return;
}

static int memlog_add_obj(struct memlog_prv *prvdesc,
			struct memlog_obj_prv *prvobj,
			const char *name)
{
	struct memlog_obj_prv *entry;
	int ret = 0;
	size_t count = strlen(name);

	if (count >= sizeof(prvobj->obj_name)) {
		dev_err(main_desc.dev, "%s: length of name is too long\n",
								__func__);
		return -EINVAL;
	}

	mutex_lock(&prvdesc->obj_lock);

	list_for_each_entry(entry, &prvdesc->obj_list, list) {
		if ((count == strlen(entry->obj_name)) &&
			!strncmp(name, entry->obj_name, count)) {
			dev_err(main_desc.dev, "%s: same name already exist\n",
								__func__);
			ret = -EEXIST;
			break;
		}
	}

	if (!ret) {
		strncpy(prvobj->obj_name, name, count);
		list_add(&prvobj->list, &prvdesc->obj_list);
		prvdesc->obj_cnt++;
		if (prvobj->obj.paddr)
			memlog_bl_add(prvdesc, prvobj);
	}

	mutex_unlock(&prvdesc->obj_lock);

	return ret;
}

static void _memlog_free(struct memlog_obj_prv *prvobj, bool under_creation)
{
	u32 type = prvobj->obj.log_type;
	size_t size = prvobj->obj.size;
	struct memlog_prv *prvdesc = desc_to_data(prvobj->obj.parent);

	ida_simple_remove(&prvdesc->obj_minor_id, prvobj->obj_minor);
	memlog_obj_remove_sysfs(prvobj);
	if (prvobj->file_buffer)
		vfree(prvobj->file_buffer);

	if (prvobj->string_buf)
		kfree(prvobj->string_buf);

	if (prvobj->mlg_bl_entry)
		prvobj->mlg_bl_entry->magic = MEMLOG_BL_INVALID_MAGIC;

	switch (type) {
	case MEMLOG_TYPE_DUMP:
		iounmap(prvobj->base_ptr);
	case MEMLOG_TYPE_DEFAULT:
	case MEMLOG_TYPE_ARRAY:
	case MEMLOG_TYPE_STRING:
	case MEMLOG_TYPE_DIRECT:
		if (prvobj->obj.paddr == 0)
			kfree(prvobj->obj.vaddr);
		else
			dma_free_coherent(main_desc.dev, size,
						prvobj->obj.vaddr,
						prvobj->obj.paddr);
		kfree(prvobj);
		break;
	case MEMLOG_TYPE_FILE:
		mutex_lock(&prvobj->file_mutex);
		vfree(prvobj->obj.vaddr);
		mutex_unlock(&prvobj->file_mutex);

		if (under_creation) {
			cdev_del(&prvobj->cdev);
			device_unregister(prvobj->obj.file->dev);
			kfree(prvobj);
		} else {
			if (atomic_read(&prvobj->open_cnt) == 0) {
				cdev_del(&prvobj->cdev);
				device_unregister(prvobj->obj.file->dev);
				ida_simple_remove(&main_desc.memlog_minor_id,
						prvobj->obj.file->minor);
				kfree(prvobj->obj.file);
				kfree(prvobj);
			} else {
				mutex_lock(&main_desc.dead_obj_list_lock);
				list_add(&prvobj->list,
					&main_desc.dead_obj_list);
				prvobj->obj.vaddr = NULL;
				mutex_unlock(&main_desc.dead_obj_list_lock);
			}
			/* make event for closing dev file */
			memlog_make_event(MEMLOG_EVENT_FILE);
		}
		break;
	default:
		break;
	}
}

static struct memlog_obj *_memlog_alloc(struct memlog *desc, size_t size,
					u32 flag, phys_addr_t paddr,
					struct memlog_file *file,
					const char *name)
{
	struct memlog_obj_prv *prvobj;
	struct memlog_obj *obj = NULL;
	struct memlog_prv *prvdesc = desc_to_data(desc);
	u32 type = flag & MEMLOG_TYPE_MASK;
	int ret;

	prvobj = kzalloc(sizeof(*prvobj), GFP_KERNEL);
	if (!prvobj) {
		dev_err(main_desc.dev, "%s: fail to alloc prvobj\n", __func__);
		goto out;
	}

	prvobj->obj_minor = ida_simple_get(&prvdesc->obj_minor_id, 0,
					MEMLOG_NUM_DEVICES, GFP_KERNEL);
	if (prvobj->obj_minor < 0) {
		dev_err(main_desc.dev, "%s: No more minor numbers(obj)\n",
								 __func__);
		kfree(prvobj);
		goto out;
	}

	prvobj->property_flag = flag;
	raw_spin_lock_init(&prvobj->log_lock);
	prvobj->obj_name[0] = 0;
	prvobj->obj.parent = (void *)desc;
	prvobj->obj.enabled = desc->log_enabled_all;
	prvobj->obj.log_type = type;
	prvobj->obj.log_level = desc->log_level_all;
	prvobj->obj.size = size;
	atomic_set(&prvobj->lost_data_cnt, 0);
	atomic_long_set(&prvobj->lost_data_amount, 0);

	ret = memlog_obj_create_sysfs(prvdesc, prvobj, false);
	if (ret) {
		dev_err(main_desc.dev, "%s: fail to create sysfs\n", __func__);
		kfree(prvobj);
		goto out;
	}

	if (flag & MEMLOG_PROPERTY_SUPPORT_FILE) {
		INIT_WORK(&prvobj->file_work, memlog_file_work);
		INIT_WORK(&prvobj->file_work_sync, memlog_file_work_sync);

		if (main_desc.mem_to_file_allow) {
			prvobj->support_file = true;
			prvobj->file_buffer_size = size;
			prvobj->file_buffer = vzalloc(prvobj->file_buffer_size);

			if (!prvobj->file_buffer) {
				memlog_obj_remove_sysfs(prvobj);
				kfree(prvobj);
				goto out;
			}
		}
	}
	if (flag & MEMLOG_PROPERTY_UTC)
		prvobj->support_utc = true;

	if (flag & MEMLOG_PROPERTY_TIMESTAMP)
		prvobj->support_ktime = true;

	if (flag & MEMLOG_PROPERTY_LOG_FWD)
		prvobj->support_logfwd = true;

	if (type == MEMLOG_TYPE_STRING || prvobj->support_logfwd) {
		prvobj->string_buf = kzalloc(MEMLOG_STRING_BUF_SIZE,
							GFP_KERNEL);
		if (!prvobj->string_buf) {
			dev_err(main_desc.dev,
				"%s: fail to alloc string_buf\n", __func__);
			if (prvobj->file_buffer)
				vfree(prvobj->file_buffer);
			memlog_obj_remove_sysfs(prvobj);
			kfree(prvobj);
			goto out;
		}
	}

	switch (type) {
	case MEMLOG_TYPE_DUMP:
		prvobj->base_ptr = ioremap(paddr, size);
		if (!prvobj->base_ptr) {
			dev_err(main_desc.dev, "%s: ioremap fail\n", __func__);
			if (prvobj->file_buffer)
				vfree(prvobj->file_buffer);
			memlog_obj_remove_sysfs(prvobj);
			kfree(prvobj);
			goto out;
		}
		prvobj->dump_paddr = paddr;
	case MEMLOG_TYPE_DEFAULT:
	case MEMLOG_TYPE_ARRAY:
	case MEMLOG_TYPE_STRING:
	case MEMLOG_TYPE_DIRECT:
		if (flag & MEMLOG_PROPERTY_NONCACHABLE) {
			prvobj->obj.vaddr = dma_alloc_coherent(main_desc.dev,
							size,
							&prvobj->obj.paddr,
							GFP_KERNEL);
			if (prvobj->obj.vaddr)
				memset(prvobj->obj.vaddr, 0, size);
		} else {
			prvobj->obj.vaddr = kzalloc(size, GFP_KERNEL);
			prvobj->obj.paddr = 0;
		}

		if (prvobj->obj.vaddr == NULL) {
			dev_err(main_desc.dev, "%s: fail to alloc memory\n",
								__func__);
			if (prvobj->file_buffer)
				vfree(prvobj->file_buffer);
			memlog_obj_remove_sysfs(prvobj);
			kfree(prvobj);
			goto out;
		}
		break;
	case MEMLOG_TYPE_FILE:
		prvobj->obj.enabled = main_desc.file_default_status;
		if (main_desc.mem_to_file_allow) {
			prvobj->obj.vaddr = vzalloc(size);
			if (!prvobj->obj.vaddr) {
				vfree(prvobj->file_buffer);
				memlog_obj_remove_sysfs(prvobj);
				kfree(prvobj);
				goto out;
			}
		} else {
			prvobj->obj.vaddr = NULL;
		}

		file->dev = device_create(main_desc.memlog_class,
				main_desc.dev,
				MKDEV(MAJOR(main_desc.memlog_dev), file->minor),
				prvobj, file->file_name);
		if (IS_ERR(file->dev)) {
			ret = PTR_ERR(file->dev);
			dev_err(main_desc.dev, "%s: device_create failed "
				"for %s (%d)", __func__, file->file_name, ret);
			vfree(prvobj->obj.vaddr);
			if (prvobj->file_buffer)
				vfree(prvobj->file_buffer);
			memlog_obj_remove_sysfs(prvobj);
			kfree(prvobj);
			goto out;
		}

		cdev_init(&prvobj->cdev, &memlog_file_ops);
		ret = cdev_add(&prvobj->cdev,
			MKDEV(MAJOR(main_desc.memlog_dev), file->minor), 1);
		if (ret < 0) {
			dev_err(main_desc.dev, "%s: cdev add failed "
						"for %s (%d)\n", __func__,
						file->file_name, ret);
			device_unregister(file->dev);
			vfree(prvobj->obj.vaddr);
			if (prvobj->file_buffer)
				vfree(prvobj->file_buffer);
			memlog_obj_remove_sysfs(prvobj);
			kfree(prvobj);
			goto out;
		}

		prvobj->file_event_threshold = size / 2;
		prvobj->obj.file = file;
		break;
	default:
		memlog_obj_remove_sysfs(prvobj);
		kfree(prvobj);
		goto out;
		break;
	}

	ret = memlog_add_obj(prvdesc, prvobj, name);
	if (ret)
		_memlog_free(prvobj, true);
	else
		obj = &prvobj->obj;
out:
	return obj;
}

static inline u32 memlog_convert_uflag(u32 uflag)
{
	u32 flag;

	flag = (uflag & MEMLOG_UFLAG_MASK) << MEMLOG_PROPERTY_SHIFT;
	flag ^= (MEMLOG_PROPERTY_NONCACHABLE | MEMLOG_PROPERTY_TIMESTAMP);

	return flag;
}

static void memlog_reset_file_obj_vmem(struct memlog_obj *obj, size_t size)
{
	void *vaddr;

	if ((obj->log_type != MEMLOG_TYPE_FILE) ||
		(obj->vaddr == NULL) || (obj->size == 0) || (size == 0)) {
		dev_info(main_desc.dev, "%s: invalid data\n", __func__);

		if (obj->log_type != MEMLOG_TYPE_FILE)
			dev_info(main_desc.dev,
				"invalid type (%s)\n",
				memlog_get_typename(obj->log_type));
		if (obj->vaddr == NULL)
			dev_info(main_desc.dev, "obj->vaddr is NULL\n");
		if (obj->size == 0)
			dev_info(main_desc.dev, "obj->size is '0'\n");
		if (size == 0)
			dev_info(main_desc.dev, "size is '0'\n");

		return;
	}

	vaddr = vzalloc(size);
	if (vaddr) {
		struct memlog_obj_prv *prvobj = obj_to_data(obj);

		vfree(obj->vaddr);
		obj->vaddr = vaddr;
		obj->size = size;
		prvobj->curr_ptr = vaddr;
		prvobj->read_ptr = vaddr;
	} else {
		dev_err(main_desc.dev,
			"%s: fail to reset(fail to vmalloc)\n", __func__);
	}
}

struct memlog_obj *memlog_alloc(struct memlog *desc, size_t size,
					struct memlog_obj *file_obj,
					const char *name, u32 uflag)
{
	struct memlog_obj *obj = NULL;
	struct memlog_obj_prv *prvobj;
	u32 flag;

	flag = memlog_convert_uflag(uflag);
	flag |= MEMLOG_TYPE_DEFAULT;

	if (file_obj) {
		if ((file_obj->log_type != MEMLOG_TYPE_FILE) ||
			(file_obj->size < size)) {
			dev_err(main_desc.dev, "%s: invalid file obj\n",
								__func__);
			return obj;
		}
		flag |= MEMLOG_PROPERTY_SUPPORT_FILE;
	}

	obj = _memlog_alloc(desc, size, flag, 0, NULL, name);
	if (obj) {
		prvobj = obj_to_data(obj);
		prvobj->file_obj = file_obj;
		prvobj->curr_ptr = prvobj->obj.vaddr;
		if (file_obj) {
			struct memlog_obj_prv *file_prvobj =
						obj_to_data(file_obj);

			file_prvobj->source_obj = obj;
			prvobj->read_ptr = prvobj->obj.vaddr;
		}
	} else {
		dev_err(main_desc.dev, "%s: alloc failed\n", __func__);
	}

	return obj;
}
EXPORT_SYMBOL(memlog_alloc);

struct memlog_obj *memlog_alloc_array(struct memlog *desc,
					u64 n, size_t size,
					struct memlog_obj *file_obj,
					const char *name,
					const char *struct_name,
					u32 uflag)
{
	struct memlog_obj *obj = NULL;
	struct memlog_obj_prv *prvobj;
	char *_struct_name;
	u32 flag;

	flag = memlog_convert_uflag(uflag);
	flag |= MEMLOG_TYPE_ARRAY;

	if (file_obj) {
		if ((file_obj->log_type != MEMLOG_TYPE_FILE) ||
			(file_obj->size < (size * n))) {
			dev_err(main_desc.dev, "%s: invalid file obj\n",
								__func__);
			return obj;
		}
		flag |= MEMLOG_PROPERTY_SUPPORT_FILE;
	}

	_struct_name = kzalloc(strlen(struct_name) + 1, GFP_KERNEL);
	if (!_struct_name) {
		dev_err(main_desc.dev, "%s: fail to alloc struct name\n",
								__func__);
		return obj;
	}
	strncpy(_struct_name, struct_name, strlen(struct_name));

	if (main_desc.mem_constraint) {
		if (file_obj) {
			size_t reset_size;

			if (n >= 4) {
				reset_size = (n / 4) * size;
				n /= 2;
			} else if (n >= 2) {
				reset_size = (n / 2) * size;
				n /= 2;
			} else {
				reset_size = n * size;
			}

			memlog_reset_file_obj_vmem(file_obj, reset_size);
		}
	}

	obj = _memlog_alloc(desc, n * size, flag, 0, NULL, name);
	if (obj) {
		prvobj = obj_to_data(obj);
		prvobj->file_obj = file_obj;
		prvobj->max_idx = obj->size / size;
		prvobj->array_unit_size = size;
		prvobj->struct_name = _struct_name;
		if (file_obj) {
			struct memlog_obj_prv *file_prvobj =
						obj_to_data(file_obj);

			file_prvobj->source_obj = obj;
			prvobj->read_log_idx = 0;

			if (main_desc.mem_constraint &&
					!!prvobj->file_buffer) {
				void *vaddr;
				size_t reset_size;

				reset_size = file_obj->size;
				vaddr = vzalloc(reset_size);
				if (vaddr) {
					vfree(prvobj->file_buffer);
					prvobj->file_buffer_size = reset_size;
					prvobj->file_buffer = vaddr;
				}
			}
		}
	} else {
		kfree(_struct_name);
		dev_err(main_desc.dev, "%s: alloc failed\n", __func__);
	}

	return obj;
}
EXPORT_SYMBOL(memlog_alloc_array);

struct memlog_obj *memlog_alloc_printf(struct memlog *desc,
					size_t size,
					struct memlog_obj *file_obj,
					const char *name,
					u32 uflag)
{
	struct memlog_obj *obj = NULL;
	struct memlog_obj_prv *prvobj;
	u32 flag;

	flag = memlog_convert_uflag(uflag);
	flag |= MEMLOG_TYPE_STRING;

	if (file_obj) {
		if ((file_obj->log_type != MEMLOG_TYPE_FILE) ||
			(file_obj->size < size)) {
			dev_err(main_desc.dev, "%s: invalid file obj\n",
								__func__);
			return obj;
		}
		flag |= MEMLOG_PROPERTY_SUPPORT_FILE;
	}

	if (main_desc.mem_constraint) {
		if (file_obj)
			memlog_reset_file_obj_vmem(file_obj, size / 4);
		size /= 2;
	}

	obj = _memlog_alloc(desc, size, flag, 0, NULL, name);
	if (obj) {
		prvobj = obj_to_data(obj);
		prvobj->file_obj = file_obj;
		prvobj->curr_ptr = prvobj->obj.vaddr;
		if (file_obj) {
			struct memlog_obj_prv *file_prvobj =
						obj_to_data(file_obj);

			file_prvobj->source_obj = obj;
			prvobj->read_ptr = prvobj->obj.vaddr;

			if (main_desc.mem_constraint &&
					!!prvobj->file_buffer) {
				void *vaddr;
				size_t reset_size;

				reset_size = file_obj->size;
				vaddr = vzalloc(reset_size);
				if (vaddr) {
					vfree(prvobj->file_buffer);
					prvobj->file_buffer_size = reset_size;
					prvobj->file_buffer = vaddr;
				}
			}
		}
	} else {
		dev_err(main_desc.dev, "%s: alloc failed\n", __func__);
	}

	return obj;
}
EXPORT_SYMBOL(memlog_alloc_printf);

struct memlog_obj *memlog_alloc_dump(struct memlog *desc,
					size_t size,
					phys_addr_t paddr,
					bool is_sfr,
					struct memlog_obj *file_obj,
					const char *name)
{
	struct memlog_obj *obj = NULL;
	struct memlog_obj_prv *prvobj;
	u32 flag;

	flag = MEMLOG_TYPE_DUMP |
		MEMLOG_PROPERTY_NONCACHABLE;

	if (file_obj) {
		if ((file_obj->log_type != MEMLOG_TYPE_FILE) ||
			(file_obj->size < size)) {
			dev_err(main_desc.dev, "%s: invalid file obj\n",
								__func__);
			return obj;
		}
		flag |= MEMLOG_PROPERTY_SUPPORT_FILE;
	}

	if (is_sfr)
		flag |= MEMLOG_DUMP_TYPE_SFR;
	else
		flag |= MEMLOG_DUMP_TYPE_SDRAM;

	obj = _memlog_alloc(desc, size, flag, paddr, NULL, name);
	if (obj) {
		prvobj = obj_to_data(obj);
		prvobj->file_obj = file_obj;
		if (file_obj) {
			struct memlog_obj_prv *file_prvobj =
						obj_to_data(file_obj);

			file_prvobj->source_obj = obj;
		}
	} else {
		dev_err(main_desc.dev, "%s: alloc failed\n", __func__);
	}

	return obj;
}
EXPORT_SYMBOL(memlog_alloc_dump);

struct memlog_obj *memlog_alloc_direct(struct memlog *desc, size_t size,
					struct memlog_obj *file_obj,
					const char *name)
{
	struct memlog_obj *obj = NULL;
	struct memlog_obj_prv *prvobj;
	u32 flag;

	flag = MEMLOG_TYPE_DIRECT |
		MEMLOG_PROPERTY_NONCACHABLE;

	if (file_obj) {
		if ((file_obj->log_type != MEMLOG_TYPE_FILE) ||
			(file_obj->size < size)) {
			dev_err(main_desc.dev, "%s: invalid file obj\n",
								__func__);
			return obj;
		}
		flag |= MEMLOG_PROPERTY_SUPPORT_FILE;
	}

	obj = _memlog_alloc(desc, size, flag, 0, NULL, name);
	if (obj) {
		prvobj = obj_to_data(obj);
		prvobj->file_obj = file_obj;
		if (file_obj) {
			struct memlog_obj_prv *file_prvobj =
						obj_to_data(file_obj);

			file_prvobj->source_obj = obj;
		}
	} else {
		dev_err(main_desc.dev, "%s: alloc failed\n", __func__);
	}

	return obj;
}
EXPORT_SYMBOL(memlog_alloc_direct);

struct memlog_obj *memlog_alloc_file(struct memlog *desc,
					const char *file_name,
					size_t buf_size,
					size_t max_file_size,
					unsigned long polling_period,
					int max_file_num)
{
	struct memlog_obj *obj = NULL;
	struct memlog_obj_prv *prvobj;
	struct memlog_file *file;
	struct memlog_file_cmd *cmd;
	u32 flag;

	file = kzalloc(sizeof(*file), GFP_KERNEL);
	if (!file) {
		dev_err(main_desc.dev, "%s: fail to alloc file descriptor\n",
								__func__);
		goto out;
	}

	file->minor = ida_simple_get(&main_desc.memlog_minor_id, 0,
					MEMLOG_NUM_DEVICES, GFP_KERNEL);
	if (file->minor < 0) {
		dev_err(main_desc.dev, "%s: No more minor numbers left\n",
								__func__);
		kfree(file);
		goto out;
	}

	cmd = kzalloc(sizeof(*cmd), GFP_KERNEL);
	if (!cmd) {
		dev_err(main_desc.dev, "%s: fail to alloc cmd\n", __func__);
		ida_simple_remove(&main_desc.memlog_minor_id, file->minor);
		kfree(file);
		goto out;
	}

	scnprintf(file->file_name, sizeof(file->file_name) - 1, "memlog-%d-%s",
			file->minor, file_name);

	file->max_file_size = max_file_size;
	file->polling_period = polling_period;
	file->max_file_num = max_file_num;

	flag = MEMLOG_TYPE_FILE;

	obj = _memlog_alloc(desc, buf_size, flag, 0, file, file_name);
	if (!obj) {
		dev_err(main_desc.dev, "%s: alloc failed\n", __func__);
		ida_simple_remove(&main_desc.memlog_minor_id, file->minor);
		kfree(file);
		kfree(cmd);
		goto out;
	}

	prvobj = obj_to_data(obj);
	prvobj->curr_ptr = obj->vaddr;
	prvobj->read_ptr = obj->vaddr;
	atomic_set(&prvobj->open_cnt, 0);
	prvobj->is_full = false;
	prvobj->remained = 0;
	mutex_init(&prvobj->file_mutex);

	cmd->cmd = MEMLOG_FILE_CMD_CREATE;
	cmd->max_file_size = (u64)max_file_size;
	cmd->max_file_num = (u32)max_file_num;
	cmd->polling_period = (u64)polling_period;
	memcpy(cmd->file_name, file->file_name, sizeof(cmd->file_name));
	memcpy(cmd->desc_name, ((struct memlog *)obj->parent)->dev_name,
						sizeof(cmd->desc_name));

	memlog_file_push_cmd(cmd);
out:
	return obj;
}
EXPORT_SYMBOL(memlog_alloc_file);

void memlog_free(struct memlog_obj *obj)
{
	u32 type = obj->log_type;
	struct memlog_obj_prv *prvobj = obj_to_data(obj);
	struct memlog_prv *prvdesc = desc_to_data(obj->parent);

	if (prvobj->is_dead)
		return;

	prvobj->is_dead = true;
	obj->enabled = false;

	if (!prvdesc->under_free_objects)
		mutex_lock(&prvdesc->obj_lock);
	list_del(&prvobj->list);
	prvdesc->obj_cnt--;
	if (!prvdesc->under_free_objects)
		mutex_unlock(&prvdesc->obj_lock);

	switch (type) {
	case MEMLOG_TYPE_ARRAY:
		kfree(prvobj->struct_name);
	case MEMLOG_TYPE_DUMP:
	case MEMLOG_TYPE_DEFAULT:
	case MEMLOG_TYPE_STRING:
	case MEMLOG_TYPE_DIRECT:
		if (prvobj->file_obj) {
			struct memlog_obj_prv *f_prvobj =
					obj_to_data(prvobj->file_obj);
			unsigned long flags;

			raw_spin_lock_irqsave(&prvobj->log_lock, flags);
			do {
				if (prvobj->is_full)
					udelay(1);
			} while (prvobj->is_full);

			f_prvobj->source_obj = NULL;

			raw_spin_unlock_irqrestore(&prvobj->log_lock, flags);
		}
		break;
	case MEMLOG_TYPE_FILE:
		if (prvobj->source_obj) {
			struct memlog_obj_prv *s_prvobj =
					obj_to_data(prvobj->source_obj);
			unsigned long flags;

			raw_spin_lock_irqsave(&s_prvobj->log_lock, flags);
			do {
				if (s_prvobj->is_full)
					udelay(1);
			} while (s_prvobj->is_full);


			s_prvobj->file_obj = NULL;
			s_prvobj->support_file = false;
			raw_spin_unlock_irqrestore(&s_prvobj->log_lock, flags);
		}
		break;
	default:
		break;
	}

	_memlog_free(prvobj, false);
}
EXPORT_SYMBOL(memlog_free);

struct memlog_obj *memlog_get_obj_by_name(struct memlog *desc,
					const char *name)
{
	struct memlog_prv *prvdesc = desc_to_data(desc);
	struct memlog_obj_prv *prvobj;
	struct memlog_obj *obj = NULL;
	size_t count = min(strlen(name), sizeof(prvobj->obj_name));

	mutex_lock(&prvdesc->obj_lock);
	list_for_each_entry(prvobj, &prvdesc->obj_list, list) {
		if ((strlen(prvobj->obj_name) == count) &&
			!strncmp(name, prvobj->obj_name, count)) {
				obj = &prvobj->obj;
				break;
			}
	}
	mutex_unlock(&prvdesc->obj_lock);

	return obj;
}
EXPORT_SYMBOL(memlog_get_obj_by_name);

struct memlog *memlog_get_desc(const char *name)
{
	struct memlog *desc = NULL;
	struct memlog_prv *prvdesc;
	size_t count;

	mutex_lock(&main_desc.memlog_list_lock);
	list_for_each_entry(prvdesc, &main_desc.memlog_list, list) {

		count = strlen(name);
		if (count > sizeof(prvdesc->desc.dev_name))
			count = sizeof(prvdesc->desc.dev_name);

		if ((count == strlen(prvdesc->desc.dev_name)) &&
			!strncmp(prvdesc->desc.dev_name, name, count)) {
			desc = &prvdesc->desc;
			break;
		}
	}
	mutex_unlock(&main_desc.memlog_list_lock);

	return desc;
}
EXPORT_SYMBOL(memlog_get_desc);

int memlog_register_data_to_string(struct memlog_obj *obj,
					memlog_data_to_string fp)
{
	struct memlog_obj_prv *prvobj = obj_to_data(obj);

	prvobj->to_string = fp;
	return 0;
}
EXPORT_SYMBOL(memlog_register_data_to_string);

void memlog_unregister(struct memlog *desc)
{
	struct memlog_prv *prvdesc = desc_to_data(desc);
	struct memlog_obj_prv *pos, *n;

	mutex_lock(&main_desc.memlog_list_lock);
	list_del(&prvdesc->list);
	mutex_unlock(&main_desc.memlog_list_lock);

	mutex_lock(&prvdesc->obj_lock);
	prvdesc->under_free_objects = true;
	list_for_each_entry_safe(pos, n, &prvdesc->obj_list, list) {
		memlog_free(&pos->obj);
	}
	mutex_unlock(&prvdesc->obj_lock);

	kobject_put(prvdesc->kobj);
	kfree(prvdesc);
}
EXPORT_SYMBOL(memlog_unregister);

int memlog_register(const char *dev_name, struct device *dev,
					struct memlog **desc)
{
	struct memlog_prv *prvdata;
	int ret = 0;

	*desc = NULL;
	if (!main_desc.inited) {
		ret = -ENOENT;
		goto out;
	}

	if (memlog_get_desc(dev_name)) {
		ret = -EEXIST;
		goto out;
	}

	prvdata = kzalloc(sizeof(*prvdata), GFP_KERNEL);
	if (!prvdata) {
		ret = -ENOMEM;
		goto out;
	}

	prvdata->kobj = kobject_create_and_add(dev_name, main_desc.tree_kobj);
	if (!prvdata->kobj) {
		ret = -ECHILD;
		kfree(prvdata);
		goto out;
	}

	prvdata->dev = dev;
	mutex_init(&prvdata->obj_lock);
	INIT_LIST_HEAD(&prvdata->obj_list);

	mutex_lock(&main_desc.memlog_list_lock);
	list_add(&prvdata->list, &main_desc.memlog_list);
	main_desc.memlog_cnt++;
	mutex_unlock(&main_desc.memlog_list_lock);

	strncpy(prvdata->desc.dev_name, dev_name,
		sizeof(prvdata->desc.dev_name) - 1);
	prvdata->desc.log_level_all = main_desc.global_log_level;
	prvdata->desc.log_enabled_all = main_desc.enabled;
	*desc = &prvdata->desc;
	ida_init(&prvdata->obj_minor_id);

	dev_info(main_desc.dev, "%s: desc '%s' is registered\n", __func__,
						prvdata->desc.dev_name);
out:
	return ret;
}
EXPORT_SYMBOL(memlog_register);

static int memlog_init_rmem(struct device *dev)
{
	int ret = 0;
	struct device_node *np;
	struct reserved_mem *rmem;

	np = of_parse_phandle(dev->of_node, "memory-region", 0);
	if (!np) {
		dev_err(dev, "%s: there is no memory-region node\n", __func__);
		ret = -ENOMEM;
		goto out;
	}

	rmem = of_reserved_mem_lookup(np);
	if (!rmem) {
		dev_err(dev, "%s: no such rmem node\n", __func__);
		ret = -ENOMEM;
		goto out;
	}

	ret = of_reserved_mem_device_init(dev);
	if (ret) {
		dev_err(dev, "%s: fail to init device memory\n", __func__);
		ret = -EINVAL;
		goto out;
	}

	main_desc.rmem = rmem;
	dev_info(dev, "%s: success init rmem\n", __func__);
out:
	return ret;
}

static void memlog_init_global_property(struct device *dev)
{
	struct device_node *np;
	int ret;

	np = of_find_node_by_name(NULL, "samsung,memlog_policy");
	if (!np) {
		dev_info(dev, "%s: there is no memlogger_policy node "
				"global log level set to default(%u)\n",
				__func__, MEMLOG_DEFAULT_LOG_LEVEL);
		main_desc.global_log_level = MEMLOG_DEFAULT_LOG_LEVEL;

		dev_info(dev, "%s: file_default_status set to default(%u)\n",
				__func__, MEMLOG_FILE_DEFAULT_STATUS);
		main_desc.file_default_status = MEMLOG_FILE_DEFAULT_STATUS;

		dev_info(dev, "%s: mem_constraint set to default(%u)\n",
				__func__, MEMLOG_MEM_CONSTRAONT_DEFAULT);
		main_desc.mem_constraint = MEMLOG_MEM_CONSTRAONT_DEFAULT;

		dev_info(dev, "%s: mem_to_file_allow set to default(%u)\n",
				__func__, MEMLOG_MEM_TO_FILE_DEFAULT);
		main_desc.mem_to_file_allow = MEMLOG_MEM_TO_FILE_DEFAULT;
	} else {
		ret = of_property_read_u32(np, "samsung,log-level",
						&main_desc.global_log_level);
		if (ret) {
			dev_info(dev, "%s: fail to read log-level, "
					"set to default(%u)\n",
					__func__, MEMLOG_DEFAULT_LOG_LEVEL);
			main_desc.global_log_level = MEMLOG_DEFAULT_LOG_LEVEL;
		} else {
			dev_info(dev, "%s: global log level set to (%u)\n",
					__func__, main_desc.global_log_level);
		}

		ret = of_property_read_u32(np, "samsung,file-default-status",
					(u32 *)&main_desc.file_default_status);
		if (ret) {
			dev_info(dev, "%s: fail to read file-default-status, "
					"set to default(%u)\n",
					__func__, MEMLOG_FILE_DEFAULT_STATUS);
			main_desc.file_default_status =
						MEMLOG_FILE_DEFAULT_STATUS;
		} else {
			dev_info(dev, "%s: file-default-status set to (%u)\n",
				__func__, (u32)main_desc.file_default_status);
		}

		ret = of_property_read_u32(np, "samsung,mem-constraint",
					(u32 *)&main_desc.mem_constraint);
		if (ret) {
			dev_info(dev, "%s: fail to read mem-constraint, "
				"set to default(%u)\n",
				__func__, MEMLOG_MEM_CONSTRAONT_DEFAULT);
			main_desc.mem_constraint =
					MEMLOG_MEM_CONSTRAONT_DEFAULT;
		} else {
			dev_info(dev, "%s: mem-constraint set to (%u)\n",
				__func__, (u32)main_desc.mem_constraint);
		}

		ret = of_property_read_u32(np, "samsung,mem-to-file-allow",
					(u32 *)&main_desc.mem_to_file_allow);
		if (ret) {
			dev_info(dev, "%s: fail to read mem-to-file-allow, "
				"set to default(%u)\n", __func__,
				MEMLOG_MEM_TO_FILE_DEFAULT);
			main_desc.mem_to_file_allow =
					MEMLOG_MEM_TO_FILE_DEFAULT;
		} else {
			dev_info(dev, "%s: mem-to-file-allow set to (%u)\n",
				__func__, (u32)main_desc.mem_to_file_allow);
		}
	}
}

static int memlog_devnode_init(struct device *dev)
{
	int ret;

	main_desc.memlog_class = class_create(THIS_MODULE, MEMLOG_NAME);
	ret = alloc_chrdev_region(&main_desc.memlog_dev, 0,
				MEMLOG_NUM_DEVICES, MEMLOG_NAME);

	if (ret) {
		dev_err(dev, "%s: unable to allocate major\n", __func__);
		goto out;
	}

	ida_init(&main_desc.memlog_minor_id);
	main_desc.event_cdev_minor_id = ida_simple_get(
						&main_desc.memlog_minor_id, 0,
						MEMLOG_NUM_DEVICES,
						GFP_KERNEL);
	if (main_desc.event_cdev_minor_id < 0) {
		dev_err(dev, "%s: fail to get minor_id\n", __func__);
		ret = -ENODEV;
		goto out;
	}

	main_desc.event_dev = device_create(main_desc.memlog_class,
					main_desc.dev,
					MKDEV(MAJOR(main_desc.memlog_dev),
					main_desc.event_cdev_minor_id),
					NULL, "memlog-evt");
	if (IS_ERR(main_desc.event_dev)) {
		ret = PTR_ERR(main_desc.event_dev);
		dev_err(dev, "%s: device_create failed (%d)\n", __func__, ret);
		goto out_free_id;
	}

	cdev_init(&main_desc.event_cdev, &memlog_event_ops);
	ret = cdev_add(&main_desc.event_cdev,
			MKDEV(MAJOR(main_desc.memlog_dev),
			main_desc.event_cdev_minor_id), 1);
	if (ret < 0) {
		dev_err(dev, "%s: fail to cdev_add(%d)\n", __func__, ret);
		goto out_free_device;
	}

	mutex_init(&main_desc.event_lock);
	INIT_LIST_HEAD(&main_desc.event_list);
	init_waitqueue_head(&main_desc.event_wait_q);

	ret = 0;
	goto out;
out_free_device:
	device_unregister(main_desc.event_dev);
out_free_id:
	ida_simple_remove(&main_desc.memlog_minor_id,
				main_desc.event_cdev_minor_id);
out:
	return ret;
}

static ssize_t memlog_cmd_info_show(struct kobject *kobj,
				struct kobj_attribute *attr, char *buf)
{
	struct memlog_file_cmd *cmd;
	ssize_t n = 0;

	cmd = memlog_file_get_cmd();
	if (cmd) {
		memcpy(buf, cmd, MEMLOG_FILE_CMD_SIZE);
		n = MEMLOG_FILE_CMD_SIZE;
	}

	return n;
}

static ssize_t memlog_cmd_info_store(struct kobject *kobj,
					struct kobj_attribute *attr,
					const char *buf, size_t count)
{
	struct memlog_file_cmd cmd;
	int ret;

	if (count != MEMLOG_FILE_CMD_SIZE) {
		dev_err(main_desc.dev, "%s: invalid size(%lu/%lu)\n",
				__func__, (unsigned long)count,
				(unsigned long)(MEMLOG_FILE_CMD_SIZE));
	} else {
		memcpy(&cmd, buf, MEMLOG_FILE_CMD_SIZE);
		ret = memlog_file_pop_cmd(&cmd);
		if (ret)
			dev_err(main_desc.dev, "%s: fail to pop\n", __func__);
	}

	return count;
}

static struct memlog_ba_file_data *memlog_ba_search_file(
					struct list_head *list,
					struct mutex *lock, void *file)
{
	struct memlog_ba_file_data *entry;
	struct memlog_ba_file_data *ret = NULL;

	mutex_lock(lock);
	list_for_each_entry(entry, list, list) {
		if (entry->file == file) {
			ret = entry;
			break;
		}
	}
	mutex_unlock(lock);

	return ret;
}

static void memlog_ba_file_add(struct list_head *list, struct mutex *lock,
					struct memlog_ba_file_data *data)
{
	mutex_lock(lock);
	list_add(&data->list, list);
	mutex_unlock(lock);
}

static void memlog_ba_file_delete(struct list_head *list, struct mutex *lock,
					struct memlog_ba_file_data *data)
{
	struct memlog_ba_file_data *pos, *n;

	mutex_lock(lock);
	list_for_each_entry_safe(pos, n, list, list) {
		if (pos == data) {
			list_del(&pos->list);
			break;
		}
	}
	mutex_unlock(lock);
}

static size_t memlog_total_obj_info_set(void *buf, size_t size)
{
	struct memlog_prv *prvdesc;
	struct memlog_obj_prv *prvobj;
	size_t max = size - 1;
	size_t n = 0;

	n += scnprintf(buf + n, max - n,
			"%16s %32s %10s %2s %8s %8s\n",
			"desc_name", "obj_name", "type",
			"en", "paddr", "size");

	mutex_lock(&main_desc.memlog_list_lock);
	list_for_each_entry(prvdesc, &main_desc.memlog_list, list) {
		mutex_lock(&prvdesc->obj_lock);
		list_for_each_entry(prvobj, &prvdesc->obj_list, list) {
			const char *type;
			struct memlog_obj *obj = &prvobj->obj;

			type = memlog_get_typename(obj->log_type);
			n += scnprintf(buf + n, max - n,
					"%16s %32s %10s %2u %8lx %8lx\n",
					prvdesc->desc.dev_name,
					prvobj->obj_name,
					type, (unsigned int)obj->enabled,
					(unsigned long)obj->paddr,
					(unsigned long)obj->size);
		}
		mutex_unlock(&prvdesc->obj_lock);
	}
	mutex_unlock(&main_desc.memlog_list_lock);

	return n;
}

static ssize_t memlog_total_obj_info_read(struct file *file,
					struct kobject *kobj,
					struct bin_attribute *attr,
					char *buf, loff_t off, size_t count)
{
	struct memlog_ba_file_data *data;

	data = memlog_ba_search_file(&main_desc.total_info_file_list,
				&main_desc.total_info_file_lock, file);
	if (data == NULL) {
		size_t size = file_inode(file)->i_size;

		data = kzalloc(sizeof(*data), GFP_KERNEL);
		data->data = kzalloc(size, GFP_KERNEL);
		data->size = memlog_total_obj_info_set(data->data, size);
		data->file = file;
		memlog_ba_file_add(&main_desc.total_info_file_list,
				&main_desc.total_info_file_lock, data);
	}

	if (data->size > off) {
		count = min(count, data->size - (size_t)off);
		memcpy(buf, data->data + off, count);
	} else {
		memlog_ba_file_delete(&main_desc.total_info_file_list,
				&main_desc.total_info_file_lock, data);
		kfree(data->data);
		kfree(data);
		count = 0;
	}

	return count;
}

static size_t memlog_dead_obj_info_set(void *buf, size_t size)
{
	struct memlog_obj_prv *prvobj;
	size_t max = size - 1;
	size_t n = 0;

	n += scnprintf(buf + n, max - n, "%6s %32s %10s\n",
				"status", "obj_name", "type");

	mutex_lock(&main_desc.dead_obj_list_lock);
	list_for_each_entry(prvobj, &main_desc.dead_obj_list, list) {
		const char *type;

		type = memlog_get_typename(prvobj->obj.log_type);
		n += scnprintf(buf + n, max - n, "%6s %32s %10s\n",
				"dead", prvobj->obj_name, type);
	}
	mutex_unlock(&main_desc.dead_obj_list_lock);

	mutex_lock(&main_desc.zombie_obj_list_lock);
	list_for_each_entry(prvobj, &main_desc.zombie_obj_list, list) {
		const char *type;

		type = memlog_get_typename(prvobj->obj.log_type);
		n += scnprintf(buf + n, max - n, "%6s %32s %10s\n",
				"zombie", prvobj->obj_name, type);
	}
	mutex_unlock(&main_desc.zombie_obj_list_lock);

	return n;
}

static ssize_t memlog_dead_obj_info_read(struct file *file,
					struct kobject *kobj,
					struct bin_attribute *attr,
					char *buf, loff_t off, size_t count)
{
	struct memlog_ba_file_data *data;

	data = memlog_ba_search_file(&main_desc.dead_info_file_list,
				&main_desc.dead_info_file_lock, file);

	if (data == NULL) {
		size_t size = file_inode(file)->i_size;

		data = kzalloc(sizeof(*data), GFP_KERNEL);
		data->data = kzalloc(size, GFP_KERNEL);
		data->size = memlog_dead_obj_info_set(data->data, size);
		data->file = file;
		memlog_ba_file_add(&main_desc.dead_info_file_list,
				&main_desc.dead_info_file_lock, data);
	}

	if (data->size > off) {
		count = min(count, data->size - (size_t)off);
		memcpy(buf, data->data + off, count);
	} else {
		memlog_ba_file_delete(&main_desc.dead_info_file_list,
				&main_desc.dead_info_file_lock, data);
		kfree(data->data);
		kfree(data);
		count = 0;
	}

	return count;
}

static void *memlog_dumpstate_find_eob(void *vaddr, size_t size)
{
	char *buf = vaddr;
	int idx = (int)size - 1;

	while (idx >= 0) {
		if (buf[idx])
			break;
		idx--;
	}

	if (idx < 0)
		idx = 0;

	return (void *)&buf[idx];
}

static size_t get_range_size_of_circular_buf(size_t buf, size_t size, size_t start, size_t end)
{
	return (end + size - start) % size;
}

static size_t memlog_dumpstate_copy_data(struct memlog_obj_prv *prvobj, void *buf, size_t max)
{
	void *eob;
	void *data_buf;
	unsigned long flags;
	size_t curr_ptr_before_memcpy;
	size_t curr_ptr_after_memcpy;
	size_t copy_size;
	size_t overwritten_data_size = 0;
	size_t total_copy_size = 0;
	struct memlog *desc = prvobj->obj.parent;
	size_t n = 0;

	data_buf = vzalloc(prvobj->obj.size);
	if (!data_buf)
		return scnprintf(buf, max, "%s, %s fail(fail to vzalloc)\n",
							desc->dev_name, prvobj->obj_name);

	n += scnprintf(buf + n, max - n, "%s, %s start\n", desc->dev_name, prvobj->obj_name);

	raw_spin_lock_irqsave(&prvobj->log_lock, flags);
	if (prvobj->eob)
		eob = memlog_dumpstate_find_eob(prvobj->obj.vaddr, prvobj->obj.size);
	else
		eob = NULL;
	curr_ptr_before_memcpy = (size_t)prvobj->curr_ptr;
	raw_spin_unlock_irqrestore(&prvobj->log_lock, flags);

	if ((size_t)eob > curr_ptr_before_memcpy) {
		copy_size = (size_t)eob - curr_ptr_before_memcpy + 1;
		memcpy(data_buf, (void *)curr_ptr_before_memcpy, copy_size);
		total_copy_size = copy_size;
	}

	copy_size = (size_t)curr_ptr_before_memcpy - (size_t)prvobj->obj.vaddr;
	memcpy(data_buf + total_copy_size, prvobj->obj.vaddr, copy_size);
	total_copy_size += copy_size;

	raw_spin_lock_irqsave(&prvobj->log_lock, flags);
	curr_ptr_after_memcpy = (size_t)prvobj->curr_ptr;
	raw_spin_unlock_irqrestore(&prvobj->log_lock, flags);

	overwritten_data_size = get_range_size_of_circular_buf((size_t)prvobj->obj.vaddr,
								prvobj->obj.size,
								curr_ptr_before_memcpy,
								curr_ptr_after_memcpy);
	copy_size = total_copy_size - overwritten_data_size;
	if ((max - n) >= copy_size) {
		memcpy(buf + n, data_buf + overwritten_data_size, copy_size);
		n += copy_size;
	}
	vfree(data_buf);
	n += scnprintf(buf + n, max - n, "%s, %s end\n\n", desc->dev_name, prvobj->obj_name);

	return n;
}

static size_t _memlog_dumpstate_copy_parsed_data(struct memlog_obj_prv *prvobj,
								void *buf, size_t max,
								void *src, size_t src_size)
{
	size_t remained;
	void *copy_ptr;
	int expired_cnt;
	size_t n = 0;

	copy_ptr = src;
	remained = src_size;
	expired_cnt = 10000;
	while (remained && (expired_cnt > 0) && (n < max)) {
		size_t parsed_data_size;
		size_t copy_size = 0;

		parsed_data_size = prvobj->to_string(copy_ptr,
							remained, buf + n,
							max - n,
							(loff_t *)&copy_size);
		if (parsed_data_size > remained)
			parsed_data_size = remained;
		remained -= parsed_data_size;
		copy_ptr += parsed_data_size;

		if (max <= (n + copy_size)) {
			n = max;
			break;
		}
		n += copy_size;
		expired_cnt--;
	}

	return n;
}

static size_t memlog_dumpstate_copy_parsed_data(struct memlog_obj_prv *prvobj,
							void *buf, size_t max)
{
	void *eob;
	void *data_buf;
	unsigned long flags;
	size_t curr_ptr_before_memcpy;
	size_t curr_ptr_after_memcpy;
	size_t copy_size;
	size_t total_copy_size = 0;
	size_t overwritten_data_size = 0;
	struct memlog *desc = prvobj->obj.parent;
	size_t n = 0;

	data_buf = vzalloc(prvobj->obj.size);
	if (!data_buf)
		return scnprintf(buf, max, "%s, %s fail(fail to vzalloc)\n",
							desc->dev_name, prvobj->obj_name);

	n += scnprintf(buf + n, max - n, "%s, %s start\n", desc->dev_name, prvobj->obj_name);

	raw_spin_lock_irqsave(&prvobj->log_lock, flags);
	if (prvobj->obj.log_type == MEMLOG_TYPE_DEFAULT) {
		if (prvobj->eob)
			eob = memlog_dumpstate_find_eob(prvobj->obj.vaddr, prvobj->obj.size);
		else
			eob = NULL;
		curr_ptr_before_memcpy = (size_t)prvobj->curr_ptr;
	} else {
		if (prvobj->eob)
			eob = prvobj->obj.vaddr + prvobj->obj.size - 1;
		else
			eob = NULL;
		curr_ptr_before_memcpy = (size_t)prvobj->obj.vaddr +
						((size_t)prvobj->log_idx *
						prvobj->array_unit_size);
	}
	raw_spin_unlock_irqrestore(&prvobj->log_lock, flags);

	if ((size_t)eob > curr_ptr_before_memcpy) {
		copy_size = (size_t)eob - curr_ptr_before_memcpy + 1;
		memcpy(data_buf, (void *)curr_ptr_before_memcpy, copy_size);
		total_copy_size = copy_size;
	}

	copy_size = (size_t)curr_ptr_before_memcpy - (size_t)prvobj->obj.vaddr;
	memcpy(data_buf + total_copy_size, prvobj->obj.vaddr, copy_size);
	total_copy_size += copy_size;

	raw_spin_lock_irqsave(&prvobj->log_lock, flags);
	if (prvobj->obj.log_type == MEMLOG_TYPE_DEFAULT)
		curr_ptr_after_memcpy = (size_t)prvobj->curr_ptr;
	else
		curr_ptr_after_memcpy = (size_t)prvobj->obj.vaddr +
						((size_t)prvobj->log_idx *
						prvobj->array_unit_size);
	raw_spin_unlock_irqrestore(&prvobj->log_lock, flags);

	overwritten_data_size = get_range_size_of_circular_buf((size_t)prvobj->obj.vaddr,
								prvobj->obj.size,
								curr_ptr_before_memcpy,
								curr_ptr_after_memcpy);

	copy_size = total_copy_size - overwritten_data_size;
	n += _memlog_dumpstate_copy_parsed_data(prvobj, buf + n, max - n,
						data_buf + overwritten_data_size, copy_size);
	vfree(data_buf);
	n += scnprintf(buf + n, max - n, "%s, %s end\n\n", desc->dev_name, prvobj->obj_name);

	return n;
}

static size_t memlog_dumpstate_set_objdata(struct memlog_obj_prv *prvobj,
						void *buf, size_t max)
{
	struct memlog_obj *obj;
	size_t n = 0;

	obj = &prvobj->obj;

	if ((obj->log_type != MEMLOG_TYPE_FILE) &&
		(obj->log_type != MEMLOG_TYPE_STRING) &&
		(prvobj->to_string == NULL)) {
		if (prvobj->file_obj) {
			struct memlog_obj_prv *file_prvobj =
						obj_to_data(prvobj->file_obj);

			if (file_prvobj->to_string)
				prvobj->to_string = file_prvobj->to_string;
		}
	}

	if (obj->log_type == MEMLOG_TYPE_STRING)
		n = memlog_dumpstate_copy_data(prvobj, buf, max);
	else if (((obj->log_type == MEMLOG_TYPE_DEFAULT) ||
			(obj->log_type == MEMLOG_TYPE_ARRAY)) &&
			(prvobj->to_string != NULL))
		n = memlog_dumpstate_copy_parsed_data(prvobj, buf, max);

	return n;
}

static size_t memlog_dumpstate_data_set(void *buf, size_t size)
{
	struct memlog_prv *prvdesc;
	struct memlog_obj_prv *prvobj;
	size_t max = size - 1;
	size_t n = 0;

	mutex_lock(&main_desc.memlog_list_lock);
	list_for_each_entry(prvdesc, &main_desc.memlog_list, list) {
		mutex_lock(&prvdesc->obj_lock);
		list_for_each_entry(prvobj, &prvdesc->obj_list, list) {
			n += memlog_dumpstate_set_objdata(prvobj,
							buf + n, max - n);
		}
		mutex_unlock(&prvdesc->obj_lock);
	}
	mutex_unlock(&main_desc.memlog_list_lock);

	return n;
}

static ssize_t memlog_dumpstate_read(struct file *file,
					struct kobject *kobj,
					struct bin_attribute *attr,
					char *buf, loff_t off, size_t count)
{
	struct memlog_ba_file_data *data;

	data = memlog_ba_search_file(&main_desc.dumpstate_file_list,
				&main_desc.dumpstate_file_lock, file);

	if (data == NULL) {
		size_t size = file_inode(file)->i_size;

		data = kzalloc(sizeof(*data), GFP_KERNEL);
		if (!data)
			return 0;

		data->data = vzalloc(size);
		if (!data->data) {
			kfree(data);
			return 0;
		}
		data->size = memlog_dumpstate_data_set(data->data, size);
		data->file = file;
		memlog_ba_file_add(&main_desc.dumpstate_file_list,
				&main_desc.dumpstate_file_lock, data);
	}

	if (data->size > off) {
		count = min(count, data->size - (size_t)off);
		memcpy(buf, data->data + off, count);
	} else {
		memlog_ba_file_delete(&main_desc.dumpstate_file_list,
				&main_desc.dumpstate_file_lock, data);
		vfree(data->data);
		kfree(data);
		count = 0;
	}

	return count;
}

static int memlog_sysfs_init(struct device *dev)
{
	int ret = 0;

	main_desc.cmd_kobj = kobject_create_and_add("cmd_info", &dev->kobj);
	if (!main_desc.cmd_kobj) {
		dev_err(dev, "%s: fail to create cmd info kobj\n", __func__);
		ret = -ENOMEM;
		goto out;
	}

	sysfs_attr_init(&main_desc.cmd_ka.attr);
	main_desc.cmd_ka.attr.mode = 0660;
	main_desc.cmd_ka.attr.name = "cmd";
	main_desc.cmd_ka.show = memlog_cmd_info_show;
	main_desc.cmd_ka.store = memlog_cmd_info_store;
	ret = sysfs_create_file(main_desc.cmd_kobj, &main_desc.cmd_ka.attr);
	if (ret) {
		dev_err(main_desc.dev, "%s: fail to create cmd\n", __func__);
		goto out_fail_cmd_ka;
	}

	main_desc.obj_kobj = kobject_create_and_add("object_information",
								&dev->kobj);
	if (!main_desc.obj_kobj) {
		dev_err(dev, "%s: fail to create obj info kobj\n", __func__);
		ret = -ENOMEM;
		goto out_fail_obj_info;
	}

	sysfs_attr_init(&main_desc.total_bin_attr.attr);
	main_desc.total_bin_attr.attr.mode = 0660;
	main_desc.total_bin_attr.attr.name = "total_object";
	main_desc.total_bin_attr.size = MEMLOG_BIN_ATTR_SIZE;
	main_desc.total_bin_attr.read = memlog_total_obj_info_read;
	main_desc.total_bin_attr.write = NULL;
	main_desc.total_bin_attr.mmap = NULL;
	ret = sysfs_create_bin_file(main_desc.obj_kobj,
				&main_desc.total_bin_attr);
	if (ret) {
		dev_err(main_desc.dev, "%s: fail to create obj\n", __func__);
		goto out_fail_obj_ka;
	}

	sysfs_attr_init(&main_desc.dead_bin_attr.attr);
	main_desc.dead_bin_attr.attr.mode = 0660;
	main_desc.dead_bin_attr.attr.name = "dead_object";
	main_desc.dead_bin_attr.size = MEMLOG_BIN_ATTR_SIZE;
	main_desc.dead_bin_attr.read = memlog_dead_obj_info_read;
	main_desc.dead_bin_attr.write = NULL;
	main_desc.dead_bin_attr.mmap = NULL;
	ret = sysfs_create_bin_file(main_desc.obj_kobj,
				&main_desc.dead_bin_attr);
	if (ret) {
		dev_err(main_desc.dev, "%s: fail to create dead obj\n",
								__func__);
		goto out_fail_dead_obj;
	}

	sysfs_attr_init(&main_desc.dumpstate_bin_attr.attr);
	main_desc.dumpstate_bin_attr.attr.mode = 0664;
	main_desc.dumpstate_bin_attr.attr.name = "dumpstate";
	main_desc.dumpstate_bin_attr.size = SZ_32M;
	main_desc.dumpstate_bin_attr.read = memlog_dumpstate_read;
	main_desc.dumpstate_bin_attr.write = NULL;
	main_desc.dumpstate_bin_attr.mmap = NULL;
	ret = sysfs_create_bin_file(main_desc.obj_kobj,
				&main_desc.dumpstate_bin_attr);
	if (ret) {
		dev_err(main_desc.dev, "%s: fail to create dumpstate\n",
								__func__);
		goto out_fail_dumpstate;
	}

	main_desc.tree_kobj = kobject_create_and_add("memlog-tree", &dev->kobj);
	if (!main_desc.tree_kobj) {
		dev_err(dev, "%s: fail to create obj tree kobj\n", __func__);
		ret = -ENOMEM;
		goto out_fail_memlog_tree;
	}

	INIT_LIST_HEAD(&main_desc.total_info_file_list);
	INIT_LIST_HEAD(&main_desc.dead_info_file_list);
	INIT_LIST_HEAD(&main_desc.dumpstate_file_list);
	mutex_init(&main_desc.total_info_file_lock);
	mutex_init(&main_desc.dead_info_file_lock);
	mutex_init(&main_desc.dumpstate_file_lock);
	goto out;

out_fail_memlog_tree:
	sysfs_remove_bin_file(main_desc.obj_kobj,
				&main_desc.dumpstate_bin_attr);
out_fail_dumpstate:
	sysfs_remove_bin_file(main_desc.obj_kobj, &main_desc.dead_bin_attr);
out_fail_dead_obj:
	sysfs_remove_bin_file(main_desc.obj_kobj, &main_desc.total_bin_attr);
out_fail_obj_ka:
	kobject_put(main_desc.obj_kobj);
out_fail_obj_info:
	sysfs_remove_file(main_desc.cmd_kobj, &main_desc.cmd_ka.attr);
out_fail_cmd_ka:
	kobject_put(main_desc.cmd_kobj);
out:
	return ret;
}

static void memlog_bl_write_base_addr(struct device *dev)
{
	struct device_node *bl_node;
	void __iomem *ptr;
	u32 base, offset;
	int ret;

	bl_node = of_parse_phandle(dev->of_node, "samsung,bl-node", 0);

	if (!bl_node) {
		dev_info(dev, "%s: fail to find bl-node\n", __func__);
		goto out;
	}

	ret = of_property_read_u32(bl_node, "samsung,bl-base", &base);
	if (ret) {
		dev_info(dev, "%s: fail to find base node\n");
		goto out;
	}

	ret = of_property_read_u32(bl_node, "samsung,bl-offset", &offset);
	if (ret) {
		dev_info(dev, "%s: fail to find offset node\n");
		goto out;
	}

	ptr = ioremap((phys_addr_t)(base + offset), 0x10);
	if (!ptr) {
		dev_info(dev, "%s: fail to ioremap\n", __func__);
		goto out;
	}

	((u64 *)ptr)[0] = (u64)memlog_bl_obj->paddr;
	((u64 *)ptr)[1] = MEMLOG_BL_VALID_MAGIC;
out:
	return;
}

static void memlog_bl_init(struct device *dev)
{
	int ret;

	ret = memlog_register("AP_MLG", dev, &memlog_desc);
	if (!ret && !!memlog_desc) {
		memlog_bl_obj = memlog_alloc_direct(memlog_desc,
						sizeof(*main_desc.mlg_bl),
						NULL, "etc-tbl");
		if (memlog_bl_obj) {
			main_desc.mlg_bl = memlog_bl_obj->vaddr;
			atomic_set(&main_desc.mlg_bl_idx, -1);
			memlog_bl_write_base_addr(dev);
			dev_info(dev, "%s: success alloc memlg bl\n", __func__);
		} else {
			main_desc.mlg_bl = NULL;
			dev_info(dev, "%s: fail to alloc memlog_bl obj\n", __func__);
		}
	} else {
		dev_info(dev, "%s: fail to register memlogger desc(%d)\n", __func__, ret);
	}
}

static int memlog_fault_handler(struct notifier_block *nb,
				unsigned long l, void *buf)
{
	main_desc.in_panic = true;
	return 0;
}

static struct notifier_block np_memlogger_panic_block = {
	.notifier_call = memlog_fault_handler,
	.priority = INT_MAX,
};

static struct notifier_block np_memlogger_die_block = {
	.notifier_call = memlog_fault_handler,
	.priority = INT_MAX,
};

static int memlog_probe(struct platform_device *pdev)
{
	int ret = 0;
	struct device *dev = &pdev->dev;

	main_desc.wq = alloc_workqueue("mlg_events", WQ_UNBOUND | WQ_SYSFS, 0);
	if (!main_desc.wq)
		goto out;

	ret = memlog_sysfs_init(dev);
	if (ret)
		goto out;

	ret = memlog_devnode_init(dev);
	if (ret)
		goto out;

	ret = memlog_init_rmem(dev);
	if (ret)
		goto out;

	memlog_init_global_property(dev);
	mutex_init(&main_desc.memlog_list_lock);
	INIT_LIST_HEAD(&main_desc.memlog_list);

	mutex_init(&main_desc.dead_obj_list_lock);
	INIT_LIST_HEAD(&main_desc.dead_obj_list);

	mutex_init(&main_desc.zombie_obj_list_lock);
	INIT_LIST_HEAD(&main_desc.zombie_obj_list);
	INIT_WORK(&main_desc.clean_prvobj_work, memlog_clean_prvobj_work);

	mutex_init(&main_desc.file_cmd_lock);
	INIT_LIST_HEAD(&main_desc.file_cmd_list);

	register_die_notifier(&np_memlogger_die_block);
	atomic_notifier_chain_register(&panic_notifier_list,
					&np_memlogger_panic_block);

	main_desc.dev = dev;
	main_desc.inited = true;
	main_desc.memlog_cnt = 0;
	main_desc.enabled = true;

	memlog_bl_init(dev);
	dev_info(dev, "%s: success memlogger probe\n", __func__);
out:
	return ret;
}

static const struct of_device_id memlog_of_match[] = {
	{ .compatible	= "samsung,memlogger" },
	{},
};
MODULE_DEVICE_TABLE(of, memlog_of_match);

static struct platform_driver memlog_driver = {
	.probe = memlog_probe,
	.driver = {
		.name = "Memlogger",
		.of_match_table = of_match_ptr(memlog_of_match),
	},
};

static int memlog_init(void)
{
	return platform_driver_register(&memlog_driver);
}
core_initcall(memlog_init);

MODULE_AUTHOR("Hosung Kim <hosung0.kim@samsung.com>");
MODULE_AUTHOR("Changki Kim <changki.kim@samsung.com>");
MODULE_AUTHOR("Donghyeok Choe <d7271.choe@samsung.com>");
MODULE_DESCRIPTION("Memlogger");
MODULE_LICENSE("GPL v2");
