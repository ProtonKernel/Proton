/*
 * Samsung Exynos5 SoC series FIMC-IS driver
 *
 *
 * Copyright (c) 2011 Samsung Electronics Co., Ltd
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#ifndef IS_FRAME_MGR_H
#define IS_FRAME_MGR_H

#include <linux/kthread.h>
#include <linux/videodev2.h>
#include "is-time.h"
#include "is-config.h"
#include "is_framemgr_config.h"

/* #define TRACE_FRAME */
#define FMGR_IDX_0		(1 << 0 )
#define FMGR_IDX_1		(1 << 1 )
#define FMGR_IDX_2		(1 << 2 )
#define FMGR_IDX_3		(1 << 3 )
#define FMGR_IDX_4		(1 << 4 )
#define FMGR_IDX_5		(1 << 5 )
#define FMGR_IDX_6		(1 << 6 )
#define FMGR_IDX_7		(1 << 7 )
#define FMGR_IDX_8		(1 << 8 )
#define FMGR_IDX_9		(1 << 9 )
#define FMGR_IDX_10		(1 << 10)
#define FMGR_IDX_11		(1 << 11)
#define FMGR_IDX_12		(1 << 12)
#define FMGR_IDX_13		(1 << 13)
#define FMGR_IDX_14		(1 << 14)
#define FMGR_IDX_15		(1 << 15)
#define FMGR_IDX_16		(1 << 16)
#define FMGR_IDX_17		(1 << 17)
#define FMGR_IDX_18		(1 << 18)
#define FMGR_IDX_19		(1 << 19)
#define FMGR_IDX_20		(1 << 20)
#define FMGR_IDX_21		(1 << 21)
#define FMGR_IDX_22		(1 << 22)
#define FMGR_IDX_23		(1 << 23)
#define FMGR_IDX_24		(1 << 24)
#define FMGR_IDX_25		(1 << 25)
#define FMGR_IDX_26		(1 << 26)
#define FMGR_IDX_27		(1 << 27)
#define FMGR_IDX_28		(1 << 28)
#define FMGR_IDX_29		(1 << 29)
#define FMGR_IDX_30		(1 << 30)
#define FMGR_IDX_31		(1 << 31)

#define framemgr_e_barrier_irqs(this, index, flag)		\
	do {							\
		this->sindex |= index;				\
		spin_lock_irqsave(&this->slock, flag);		\
	} while (0)
#define framemgr_x_barrier_irqr(this, index, flag)		\
	do {							\
		spin_unlock_irqrestore(&this->slock, flag);	\
		this->sindex &= ~index;				\
	} while (0)
#define framemgr_e_barrier_irq(this, index)			\
	do {							\
		this->sindex |= index;				\
		spin_lock_irq(&this->slock);			\
	} while (0)
#define framemgr_x_barrier_irq(this, index)			\
	do {							\
		spin_unlock_irq(&this->slock);			\
		this->sindex &= ~index;				\
	} while (0)
#define framemgr_e_barrier(this, index)				\
	do {							\
		this->sindex |= index;				\
		spin_lock(&this->slock);			\
	} while (0)
#define framemgr_x_barrier(this, index)				\
	do {							\
		spin_unlock(&this->slock);			\
		this->sindex &= ~index;				\
	} while (0)

enum is_frame_state {
	FS_FREE,
	FS_REQUEST,
	FS_PROCESS,
	FS_COMPLETE,
	FS_STRIPE_PROCESS,
	FS_INVALID
};

#define FS_HW_FREE	FS_FREE
#define FS_HW_REQUEST	FS_REQUEST
#define FS_HW_CONFIGURE	FS_PROCESS
#define FS_HW_WAIT_DONE	FS_COMPLETE
#define FS_HW_INVALID	FS_INVALID

#define NR_FRAME_STATE FS_INVALID

enum is_frame_mem_state {
	/* initialized memory */
	FRAME_MEM_INIT,
	/* mapped memory */
	FRAME_MEM_MAPPED
};

struct is_frame;

struct votf_ops {
	void (*s_addr)(void *data, unsigned long id, struct is_frame *frame); /* DMA address setting */
	void (*s_oneshot)(void *data, unsigned long id); /* oneshot enable */
	void (*wait_idle)(void *data, unsigned long id);
};

struct votf_action {
	unsigned long		id;	/* enum is_subdev_id */
	void			*data;	/* struct including HW SFR base address */
	const struct votf_ops	*ops;
};

struct is_framemgr {
	unsigned long		id;
	char			name[IS_STR_LEN];
	spinlock_t		slock;
	ulong			sindex;

	u32			num_frames;
	struct is_frame	*frames;

	u32			queued_count[NR_FRAME_STATE];
	struct list_head	queued_list[NR_FRAME_STATE];

	struct votf_action	master;	/* WDMA */
	struct votf_action	slave;	/* RDMA */

	u32			batch_num;

	u32			proc_warn_cnt;
};

static const char * const hw_frame_state_name[NR_FRAME_STATE] = {
	"Free",
	"Request",
	"Configure",
	"Wait_Done"
};

static const char * const frame_state_name[NR_FRAME_STATE] = {
	"Free",
	"Request",
	"Process",
	"Complete",
	"Stripe"
};

ulong frame_fcount(struct is_frame *frame, void *data);
int put_frame(struct is_framemgr *this, struct is_frame *frame,
			enum is_frame_state state);
struct is_frame *get_frame(struct is_framemgr *this,
			enum is_frame_state state);
int trans_frame(struct is_framemgr *this, struct is_frame *frame,
			enum is_frame_state state);
struct is_frame *peek_frame(struct is_framemgr *this,
			enum is_frame_state state);
struct is_frame *peek_frame_tail(struct is_framemgr *this,
			enum is_frame_state state);
struct is_frame *find_frame(struct is_framemgr *this,
			enum is_frame_state state,
			ulong (*fn)(struct is_frame *, void *), void *data);
struct is_frame *find_stripe_process_frame(struct is_framemgr *this,
			u32 fcount);
void print_frame_queue(struct is_framemgr *this,
			enum is_frame_state state);

int frame_manager_probe(struct is_framemgr *this, u32 id, const char *name);
int frame_manager_open(struct is_framemgr *this, u32 buffers);
int frame_manager_close(struct is_framemgr *this);
int frame_manager_flush(struct is_framemgr *this);
void frame_manager_print_queues(struct is_framemgr *this);
#if defined(ENABLE_CLOG_RESERVED_MEM)
void frame_manager_dump_queues(struct is_framemgr *this);
#endif
void frame_manager_print_info_queues(struct is_framemgr *this);

#endif
