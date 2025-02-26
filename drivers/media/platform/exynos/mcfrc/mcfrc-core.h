/* linux/drivers/media/platform/exynos/mcfrc/mcfrc-core.h
 *
 * Internal header for Samsung MC FRC driver
 *
 * Copyright (c) 2020 Samsung Electronics Co., Ltd.
 *		http://www.samsung.com
 *
 * Authors: Jungik Seo <jungik.seo@samsung.com>
 *          Igor Kovliga <i.kovliga@samsung.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#ifndef MCFRC_CORE_H_
#define MCFRC_CORE_H_

#include <linux/miscdevice.h>
#include <linux/spinlock.h>
#include <linux/pm_qos.h>
#include <linux/wait.h>

#if 0 && (IS_ENABLED(CONFIG_EXYNOS_PM_QOS) || IS_ENABLED(CONFIG_EXYNOS_PM_QOS_MODULE))
#define CONFIG_MCFRC_USE_DEVFREQ
#endif
#ifdef CONFIG_MCFRC_USE_DEVFREQ
#include <soc/samsung/exynos_pm_qos.h>
#endif

#include "hwmcfrc.h"

#define MODULE_NAME     "exynos-mcfrc"
#define NODE_NAME       "mcfrc"

#define MCFRC_HEADER_BYTES       16
#define MCFRC_BYTES_PER_BLOCK    16

/* mcfrc hardware device state */
#define DEV_RUN         1
#define DEV_SUSPEND     2

#define MCFRC_INPUT_MIN_WIDTH    32
#define MCFRC_INPUT_MIN_HEIGHT   8



/**
 * struct mcfrc_dev - the abstraction for mcfrc device
 * @dev         : pointer to the mcfrc device
 * @clk_producer: the clock producer for this device
 * @regs        : the mapped hardware registers
 * @state       : device state flags
 * @slock       : spinlock for this data structure, mainly used for state var
 * @version     : IP version number
 *
 * @misc	: misc device descriptor for user-kernel interface
 * @tasks	: the list of the tasks that are to be scheduled
 * @contexts	: the list of the contexts that is created for this device
 * @lock_task	: lock to protect the consistency of @tasks list and
 *                @current_task
 * @lock_ctx	: lock to protect the consistency of @contexts list
 * @timeout_jiffies: timeout jiffoes for a task. If a task is not finished
 *                   until @timeout_jiffies elapsed,
 *                   timeout_task() is invoked an the task
 *                   is canced. The user will get an error.
 * @current_task: indicate the task that is currently being processed
 *                NOTE: this pointer is only valid while the task is being
 *                      PROCESSED, not while it's being prepared for processing.
 */
struct mcfrc_dev {
	struct device				*dev;
	struct clk					*clk_producer;
	void __iomem				*regs;
	unsigned long				state;

    unsigned long				task_irq;
    unsigned long				state_irq;
    struct completion           rst_complete;

	spinlock_t					slock;
	u32							version;

#ifdef CONFIG_MCFRC_USE_DEVFREQ
    struct exynos_pm_qos_request    qos_req;
    s32                             qos_req_level;
#endif

	struct miscdevice			misc;
	struct list_head			contexts;
	spinlock_t					lock_task;		/* lock with irqsave for tasks */
	spinlock_t					lock_ctx;		/* lock for contexts */
	struct mcfrc_task			*current_task;
	struct workqueue_struct		*workqueue;
	wait_queue_head_t			suspend_wait;
};

/**
 * mcfrc_ctx - the device context data
 * @mcfrc_dev:		mcfrc IP device for this context
 *
 * @flags:		TBD: maybe not required
 *
 * @node	: node entry to mcfrc_dev.contexts
 * @mutex	: lock to prevent racing between tasks between the same contexts
 * @kref	: usage count of the context not to release the context while a
 *              : task being processed.
 * @priv	: private data that is allowed to store client drivers' private
 *                data
 */
struct mcfrc_ctx {
    struct mcfrc_dev    *mcfrc_dev;
    u32                 flags;

    struct list_head    node;
    struct mutex        mutex;
    struct kref         kref;
    void                *priv;
};


/**
 * enum MCFRC_state - state of a task
 *
 * @MCFRC_BUFSTATE_READY	: Task is verified and scheduled for processing
 * @MCFRC_BUFSTATE_PROCESSING  : Task is being processed by H/W.
 * @MCFRC_BUFSTATE_DONE	: Task is completed.
 * @MCFRC_BUFSTATE_TIMEDOUT	: Task was not completed before timeout expired
 * @MCFRC_BUFSTATE_ERROR:	: Task is not processed due to verification
 *                                failure
 */
enum mcfrc_state {
	MCFRC_BUFSTATE_READY,
	MCFRC_BUFSTATE_PROCESSING,
	MCFRC_BUFSTATE_DONE,
	MCFRC_BUFSTATE_TIMEDOUT,
	MCFRC_BUFSTATE_RETRY,
	MCFRC_BUFSTATE_ERROR,
};

/**
 * struct mcfrc_buffer_plane_dma - descriptions of a buffer
 *
 * @bytes_used	: the size of the buffer that is accessed by H/W. This is filled
 *                based on the task information received from userspace.
 * @dmabuf	: pointer to dmabuf descriptor if the buffer type is
 *		  HWMCFRC_BUFFER_DMABUF.
 *		  This is NULL when the buffer type is HWMCFRC_BUFFER_USERPTR and
 *		  the ptr points to user memory which did NOT already have a
 *		  dmabuf associated to it.
 * @attachment	: pointer to dmabuf attachment descriptor if the buffer type is
 *		  HWMCFRC_BUFFER_DMABUF (or if it's HWMCFRC_BUFFER_USERPTR and we
 *		  found and reused the dmabuf that was associated to that chunk
 *		  of memory, if there was any).
 * @sgt		: scatter-gather list that describes physical memory information
 *		: of the buffer. This is valid under the same condition as
 *		  @attachment and @dmabuf.
 * @dma_addr	: DMA address that is the address of the buffer in the H/W's
 *		: address space.
 *		  We use the IOMMU to map the input dma buffer or user memory
 *		  to a contiguous mapping in H/W's address space, so that H/W
 *		  can perform a DMA transfer on it.
 * @priv	: the client device driver's private data
 * @offset      : when buffer is of type HWMCFRC_BUFFER_USERPTR, this is the
 *		  offset of the buffer inside the VMA that holds it.
 */
struct mcfrc_buffer_plane_dma {
	size_t						bytes_used;
	struct dma_buf				*dmabuf;
	struct dma_buf_attachment	*attachment;
	struct sg_table				*sgt;
	dma_addr_t					dma_addr;
	off_t						offset;

    void                        *vaddr;
};

/**
 * struct mcfrc_buffer_dma - description of buffers for a task
 *
 * @buffers	: pointer to buffers received from userspace
 * @plane	: the corresponding internal buffer structure
 */
struct mcfrc_buffer_dma {
	/* pointer to mcfrc_task.user_task.buf_inout */
	const struct hwMCFRC_buffer		*buffer;
	struct mcfrc_buffer_plane_dma	plane;
    enum dma_data_direction         dir;
};

enum mcfrc_type {
	MCFRC_FULL
};

/**
 * struct mcfrc_task - describes a task to process
 *
 * @user_task	: descriptions about the surfaces and format to process,
 *                provided by userspace.
 *                It also includes the encoding configuration (block size,
 *                dual plane, refinement iterations, partitioning, block mode)
 * @task_node	: list entry to mcfrc_dev.tasks
 * @ctx		: pointer to mcfrc_ctx that the task is valid under.
 * @complete	: waiter to finish the task
 * @dma_buf_out	: descriptions of the capture buffer (i.e. result from device)
 * @dma_buf_cap	: descriptions of the output buffer
 *                (i.e. what we're giving as input to device)
 * @state	: state of the task
 */
struct mcfrc_task {
    struct hwMCFRC_task     user_task;
    struct mcfrc_ctx        *ctx;
    struct completion       complete;
    struct mcfrc_buffer_dma dma_buf_in[2];
    struct mcfrc_buffer_dma dma_buf_out;
    struct mcfrc_buffer_dma dma_buf_params;
    struct mcfrc_buffer_dma dma_buf_temp;
    struct work_struct      work;
    enum mcfrc_state        state;
    enum mcfrc_type         type;
};

#endif /* MCFRC_CORE_H_ */
