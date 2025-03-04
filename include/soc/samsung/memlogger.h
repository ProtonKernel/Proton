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

#ifndef MEMLOG_H
#define MEMLOG_H

#include <linux/types.h>
#include <linux/device.h>
#include <linux/time.h>
#include <linux/rtc.h>

/**
 * struct memlog_file
 * @file_name: file name. this is valid only if tpye of obj is file.
 * @max_file_size: maximum file size to store.
 * @max_file_num: maximum file count to store.
 * @polling_period: period(ms) of polling this file to read.
 * @dev: dev for data transfer
 * @minor: memlog class's device minor id
 */
struct memlog_file {
	char file_name[128];
	size_t max_file_size;
	int max_file_num;
	unsigned long polling_period;
	struct device *dev;
	int minor;
};

/**
 * struct memlog_obj - memory logger object information
 * @parent: parent pointer(struct memlog)
 * @enabled: indicate whether the logging is turned on
 * @log_level: indicate this log level of this object
 * @log_type: indicate logging type of this object(direct, array, string, dump,
 * file)
 * @paddr: physical address only if memory is non-cachable type. otherwize,
 * this is 0
 * @vaddr: virtual address for this logging obj
 * @size: size of logging data
 * @file: file information. this is valid only if obj type is file.
 */
struct memlog_obj {
	void *parent;

	bool enabled;
	unsigned int log_level;
	unsigned int log_type;

	dma_addr_t paddr;
	void * vaddr;
	size_t size;

	struct memlog_file *file;
};

/**
 * struct memlog_ops - memoy logger callback functions
 * @log_status_notify: function is called when logging status is chagned
 * (full, empty ...)
 * @log_level_notify: function is called when global log levei is chaged
 * @log_enable_notify: function is called when global log is en/disabled
 * @file_ops_completed: function is called when somthing is finished
 */
struct memlog_ops {
	int (*log_status_notify)(struct memlog_obj *obj, u32 flags);
	int (*log_level_notify)(struct memlog_obj *obj, u32 flags);
	int (*log_enable_notify)(struct memlog_obj *obj, u32 flags);
	int (*file_ops_completed)(struct memlog_obj *obj, u32 flags);
};

/**
 * struct memlog - memory logger descriptor
 * @dev_name: device name which is using memlogger
 * @ops: callback function's for logging object
 * @log_level_all: log level of this device's logging objs
 * @log_enabled_all: indicates whther this device's logging is enabled
 */
struct memlog {
	char dev_name[128];
	struct memlog_ops ops;

	unsigned int log_level_all;
	bool log_enabled_all;
};

typedef size_t (*memlog_data_to_string)(void *src, size_t src_size,
					void *buf, size_t count, loff_t *pos);

#define MEMLOG_DISABLE			(0)
#define MEMLOG_ENABLE			(1)

#define MEMLOG_LEVEL_EMERG		(0)
#define MEMLOG_LEVEL_ERR		(1)
#define MEMLOG_LEVEL_CAUTION		(2)
#define MEMLOG_LEVEL_NOTICE		(3)
#define MEMLOG_LEVEL_INFO		(4)
#define MEMLOG_LEVEL_DEBUG		(5)
#define MEMLOG_DEFAULT_LOG_LEVEL	(MEMLOG_LEVEL_NOTICE)
#define MEMLOG_LOGFWD_LEVEL		(MEMLOG_LEVEL_CAUTION)

#define MEMLOG_FILE_DEFAULT_STATUS	(1)
#define MEMLOG_MEM_CONSTRAONT_DEFAULT	(0)
#define MEMLOG_MEM_TO_FILE_DEFAULT	(1)

#define MEMLOG_TYPE_MASK		(0x0000000f)
#define MEMLOG_TYPE_DUMP		(0x1)
#define MEMLOG_TYPE_DEFAULT		(0x2)
#define MEMLOG_TYPE_ARRAY		(0x3)
#define MEMLOG_TYPE_STRING		(0x4)
#define MEMLOG_TYPE_DIRECT		(0x5)
#define MEMLOG_TYPE_FILE		(0x6)

#define MEMLOG_DUMP_TYPE_MASK		(0x000000f0)
#define MEMLOG_DUMP_TYPE_SFR		(0x10)
#define MEMLOG_DUMP_TYPE_SDRAM		(0x20)

#define MEMLOG_NOTI_MASK		(0x00000f00)
#define MEMLOG_NOTI_FULL		(0x100)
#define MEMLOG_NOTI_CRASH		(0x200)
#define MEMLOG_NOTI_CREATE_DONE		(0x300)
#define MEMLOG_NOTI_WRITE_DONE		(0x400)
#define MEMLOG_NOTI_SYNC_DONE		(0x500)
#define MEMLOG_NOTI_DUMP_DONE		(0x600)

#define MEMLOG_UFLAG_CACHEABLE		(0x1 << 0)
#define MEMLOG_UFALG_NO_TIMESTAMP	(0x1 << 1)
#define MEMLOG_UFLAG_UTC		(0x1 << 2)
#define MEMLOG_UFLAG_LOG_FWD		(0x1 << 3)
#define MEMLOG_UFLAG_MASK		(MEMLOG_UFALG_NO_TIMESTAMP | \
						MEMLOG_UFLAG_CACHEABLE | \
						MEMLOG_UFLAG_UTC | \
						MEMLOG_UFLAG_LOG_FWD)

#define MEMLOG_FILE_CMD_CREATE		(0x1)
#define MEMLOG_FILE_CMD_PAUSE		(0x2)
#define MEMLOG_FILE_CMD_RESUME		(0x3)

/* APIs */
#if IS_ENABLED(CONFIG_EXYNOS_MEMORY_LOGGER)
int memlog_register(const char *dev_name, struct device *dev,
					struct memlog **desc);
void memlog_unregister(struct memlog *desc);
struct memlog *memlog_get_desc(const char *dev_name);
struct memlog_obj *memlog_alloc(struct memlog *desc, size_t size,
					struct memlog_obj *file_obj,
					const char *name, u32 uflag);
struct memlog_obj *memlog_alloc_array(struct memlog *desc,
					u64 n, size_t size,
					struct memlog_obj *file_obj,
					const char *name,
					const char *struct_name,
					u32 uflag);
struct memlog_obj *memlog_alloc_printf(struct memlog *desc,
					size_t size,
					struct memlog_obj *file_obj,
					const char *name,
					u32 uflag);
struct memlog_obj *memlog_alloc_dump(struct memlog *desc,
					size_t size,
					phys_addr_t paddr,
					bool is_sfr,
					struct memlog_obj *file_obj,
					const char *name);
struct memlog_obj *memlog_alloc_direct(struct memlog *desc, size_t size,
					struct memlog_obj *file_obj,
					const char *name);
struct memlog_obj *memlog_alloc_file(struct memlog *desc,
					const char *file_name,
					size_t buf_size,
					size_t max_file_size,
					unsigned long polling_period,
					int max_file_num);
void memlog_free(struct memlog_obj *obj);
struct memlog_obj *memlog_get_obj_by_name(struct memlog *desc,
					const char *name);
int memlog_write(struct memlog_obj *obj, int log_level,
				void *src, size_t size);
int memlog_write_array(struct memlog_obj *obj, int log_level, void *src);
int memlog_write_printf(struct memlog_obj *obj, int log_level,
					const char *fmt, ...);
int memlog_write_vsprintf(struct memlog_obj *obj, int log_level,
			  const char *fmt, va_list args);
int memlog_do_dump(struct memlog_obj *obj, int log_level);
int memlog_dump_direct_to_file(struct memlog_obj *obj, int log_level);
size_t memlog_write_file(struct memlog_obj *obj, void *src, size_t size);
int memlog_sync_to_file(struct memlog_obj *obj);
int memlog_manual_sync_to_file(struct memlog_obj *obj,
				void *base, size_t size);
bool memlog_is_data_remaining(struct memlog_obj *obj);
void memlog_obj_set_sysfs_mode(struct memlog_obj *obj, bool uaccess);
int memlog_register_data_to_string(struct memlog_obj *obj,
					memlog_data_to_string fp);
int memlog_set_file_event_threshold(struct memlog_obj *obj, size_t threshold);
#else /* CONFIG_EXYNOS_MEMORY_LOGGER */
static inline int memlog_register(const char *dev_name, struct device *dev,
							struct memlog **desc)
{
	*desc = NULL;
	return -1;
}
static inline void memlog_unregister(struct memlog *desc)
{
}
static inline struct memlog *memlog_get_desc(const char *dev_name)
{
	return NULL;
}
static inline struct memlog_obj *memlog_alloc(struct memlog *desc,
						size_t size,
						struct memlog_obj *file_obj,
						const char *name,
						u32 uflag)
{
	return NULL;
}
static inline struct memlog_obj *memlog_alloc_array(struct memlog *desc,
						u64 n, size_t size,
						struct memlog_obj *file_obj,
						const char *name,
						const char *struct_name,
						u32 uflag)
{
	return NULL;
}
static inline struct memlog_obj *memlog_alloc_printf(struct memlog *desc,
						size_t size,
						struct memlog_obj *file_obj,
						const char *name,
						u32 uflag)
{
	return NULL;
}
static inline struct memlog_obj *memlog_alloc_dump(struct memlog *desc,
						size_t size,
						phys_addr_t paddr,
						bool is_sfr,
						struct memlog_obj *file_obj,
						const char *name)
{
	return NULL;
}
static inline struct memlog_obj *memlog_alloc_direct(struct memlog *desc,
						size_t size,
						struct memlog_obj *file_obj,
						const char *name)
{
	return NULL;
}
static inline struct memlog_obj *memlog_alloc_file(struct memlog *desc,
						const char *file_name,
						size_t buf_size,
						size_t max_file_size,
						unsigned long polling_period,
						int max_file_num)
{
	return NULL;
}
static inline void memlog_free(struct memlog_obj *obj)
{
}
static inline struct memlog_obj *memlog_get_obj_by_name(struct memlog *desc,
							const char *name)
{
	return NULL;
}
static inline int memlog_write(struct memlog_obj *obj, int log_level,
						void *src, size_t size)
{
	return -1;
}
static inline int memlog_write_array(struct memlog_obj *obj, int log_level,
								void *src)
{
	return -1;
}
static inline int memlog_write_printf(struct memlog_obj *obj, int log_level,
							const char *fmt, ...)
{
	return -1;
}
static inline int memlog_write_vsprintf(struct memlog_obj *obj, int log_level,
					const char *fmt, va_list args)
{
	return -1;
}
static inline int memlog_do_dump(struct memlog_obj *obj, int log_level)
{
	return -1;
}
static inline int memlog_dump_direct_to_file(struct memlog_obj *obj,
							int log_level)
{
	return -1;
}
static inline size_t memlog_write_file(struct memlog_obj *obj,
					void *src, size_t size)
{
	return 0;
}
static inline int memlog_sync_to_file(struct memlog_obj *obj)
{
	return -1;
}
static inline int memlog_manual_sync_to_file(struct memlog_obj *obj,
						void *base, size_t size)
{
	return -1;
}
static inline bool memlog_is_data_remaining(struct memlog_obj *obj)
{
	return false;
}

static inline void memlog_obj_set_sysfs_mode(struct memlog_obj *obj,
							bool uaccess)
{
}
static inline int memlog_register_data_to_string(struct memlog_obj *obj,
						memlog_data_to_string fp)
{
	return -1;
}
static inline int memlog_set_file_event_threshold(struct memlog_obj *obj,
							size_t threshold)
{
	return -1;
}
#endif /* CONFIG_EXYNOS_MEMORY_LOGGER */
#endif
