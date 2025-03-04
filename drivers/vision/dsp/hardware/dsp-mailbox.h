/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Samsung Exynos SoC series dsp driver
 *
 * Copyright (c) 2019 Samsung Electronics Co., Ltd.
 *              http://www.samsung.com/
 */

#ifndef __HW_DSP_MAILBOX_H__
#define __HW_DSP_MAILBOX_H__

#include "dsp-util.h"
#include "dsp-task.h"
#include "dsp-time.h"

struct dsp_system;
struct dsp_mailbox;

enum dsp_mailbox_version {
	DSP_MAILBOX_VERSION_START,
	DSP_MAILBOX_V1,
	DSP_MAILBOX_VERSION_END
};

enum dsp_message_version {
	DSP_MESSAGE_VERSION_START,
	DSP_MESSAGE_V1,
	DSP_MESSAGE_V2,
	DSP_MESSAGE_V3,
	DSP_MESSAGE_VERSION_END
};

struct dsp_mailbox_to_fw {
	unsigned int			mailbox_version;
	unsigned int			message_version;
	unsigned int			task_id;
	unsigned int			pool_iova;
	unsigned int			pool_size;
	unsigned int			message_id;
	unsigned int			message_size;
	unsigned int			reserved[9];
};

struct dsp_mailbox_to_host {
	unsigned int			mailbox_version;
	unsigned int			message_version;
	unsigned int			task_id;
	unsigned int			task_ret;
	unsigned int			message_id;
	unsigned int			reserved[3];
};

struct dsp_mailbox_pool_manager {
	spinlock_t			slock;
	struct list_head		pool_list;
	unsigned int			pool_count;
	struct dsp_util_bitmap		pool_map;

	size_t				size;
	dma_addr_t			iova;
	void				*kva;

	unsigned int			block_size;
	unsigned int			block_count;
	unsigned int			used_count;
	struct dsp_mailbox		*mbox;
};

struct dsp_mailbox_pool {
	unsigned int			block_count;
	int				block_start;
	size_t				size;
	size_t				used_size;
	dma_addr_t			iova;
	void				*kva;
	struct list_head		list;
	int				pm_qos;

	struct dsp_mailbox_pool_manager	*owner;
	struct dsp_time			time;
};

struct dsp_mailbox_ops {
	struct dsp_mailbox_pool *(*alloc_pool)(struct dsp_mailbox *mbox,
			unsigned int size);
	void (*free_pool)(struct dsp_mailbox *mbox,
			struct dsp_mailbox_pool *pool);
	void (*dump_pool)(struct dsp_mailbox *mbox,
			struct dsp_mailbox_pool *pool);
	int (*send_task)(struct dsp_mailbox *mbox, struct dsp_task *task);
	int (*receive_task)(struct dsp_mailbox *mbox);
	int (*send_message)(struct dsp_mailbox *mbox, unsigned int message_id);

	int (*start)(struct dsp_mailbox *mbox);
	int (*stop)(struct dsp_mailbox *mbox);

	int (*open)(struct dsp_mailbox *mbox);
	int (*close)(struct dsp_mailbox *mbox);
	int (*probe)(struct dsp_system *sys);
	void (*remove)(struct dsp_mailbox *mbox);
};

struct dsp_mailbox {
	struct mutex			lock;
	unsigned int			mailbox_version;
	unsigned int			message_version;
	struct dsp_util_queue		*to_fw;
	struct dsp_mailbox_to_host	*to_host;

	struct dsp_mailbox_pool_manager	*pool_manager;
	const struct dsp_mailbox_ops	*ops;
	struct dsp_system		*sys;
};

struct dsp_mailbox_pool *dsp_mailbox_alloc_pool(struct dsp_mailbox *mbox,
		unsigned int size);
void dsp_mailbox_free_pool(struct dsp_mailbox_pool *pool);
void dsp_mailbox_dump_pool(struct dsp_mailbox_pool *pool);

int dsp_mailbox_set_ops(struct dsp_mailbox *mbox, unsigned int dev_id);
int dsp_mailbox_register_ops(unsigned int dev_id,
		const struct dsp_mailbox_ops *ops);

#endif
