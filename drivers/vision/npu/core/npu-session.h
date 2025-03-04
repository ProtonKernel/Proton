/*
 * Samsung Exynos SoC series NPU driver
 *
 * Copyright (c) 2017 Samsung Electronics Co., Ltd.
 *              http://www.samsung.com/
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 */

#ifndef _NPU_SESSION_H_
#define _NPU_SESSION_H_

#include <linux/types.h>
#include <linux/mutex.h>
#include <linux/sched.h>

#include "vs4l.h"

#include "npu-vertex.h"
#include "npu-queue.h"
#include "npu-config.h"
#include "npu-common.h"
#include "npu-scheduler.h"
#include "npu-if-session-protodrv.h"
#include "ncp_header.h"
#include "drv_usr_if.h"

#define FM_SHARED_NUM	1

enum npu_session_state {
	NPU_SESSION_STATE_OPEN,
	NPU_SESSION_STATE_REGISTER,
	NPU_SESSION_STATE_GRAPH_ION_MAP,
	NPU_SESSION_STATE_WGT_KALLOC,
	NPU_SESSION_STATE_IOFM_KALLOC,
	NPU_SESSION_STATE_IOFM_ION_ALLOC,
	NPU_SESSION_STATE_IMB_ION_ALLOC,
	NPU_SESSION_STATE_FORMAT_IN,
	NPU_SESSION_STATE_FORMAT_OT,
	NPU_SESSION_STATE_START,
	NPU_SESSION_STATE_STOP,
	NPU_SESSION_STATE_CLOSE,
};

struct ion_info {
	dma_addr_t		daddr;
	void				*vaddr;
	size_t			size;
};

struct ncp_info {
	u32 address_vector_cnt;
	struct addr_info ncp_addr;
};

#define NPU_Q_TIMEDIFF_WIN_MAX 5
struct npu_session {
	u32 uid;
	u32 frame_id;
	void *cookie;
	void *memory;
	void *exynos;
	struct npu_vertex_ctx vctx;
	const struct npu_session_ops	*gops;
	int (*undo_cb)(struct npu_session *);

	unsigned long ss_state;

	u32 IFM_cnt;
	u32 OFM_cnt;
	u32 IMB_cnt;
	u32 WGT_cnt;
	u32 IOFM_cnt;

	struct ncp_info ncp_info;

	// dynamic allocation, free is required, for ion alloc memory
	struct npu_memory_buffer *ncp_mem_buf;
	struct npu_memory_buffer *ncp_hdr_buf;
	struct npu_memory_buffer *ncp_payload;
	struct npu_memory_buffer *IOFM_mem_buf; // VISION_MAX_CONTAINERLIST * VISION_MAX_BUFFER
	struct npu_memory_buffer *IMB_mem_buf; // IMB_cnt

	// kmalloc, not ion alloc
	struct av_info IFM_info[VISION_MAX_CONTAINERLIST][VISION_MAX_BUFFER];
	struct av_info OFM_info[VISION_MAX_CONTAINERLIST][VISION_MAX_BUFFER];
	struct addr_info *IMB_info;
	struct addr_info *WGT_info;

	int qbuf_IOFM_idx; // IOFM Buffer index
	int dqbuf_IOFM_idx; // IOFM Buffer index

	u32 FIRMWARE_DRAM_TRANSFER; // dram transfer or sram transfer for firmware

	struct npu_nw_sched_param sched_param;/* For NW priority and boundness */

	struct nw_result nw_result;
	wait_queue_head_t wq;

	struct npu_frame control;
	struct mutex *global_lock;

	struct mbox_process_dat mbox_process_dat;

	struct ion_info *imb_ion_info;

	pid_t	pid;

	u32 address_vector_offset;
	u32 address_vector_cnt;
	u32 memory_vector_offset;
	u32 memory_vector_cnt;

#ifdef CONFIG_NPU_ARBITRATION
	unsigned long total_flc_transfer_size, total_sdma_transfer_size;
	u32	cmdq_isa_size;
	u32     inferencefreq_index;
	u64     last_q_time_stamp;
	u32     inferencefreq_window[NPU_Q_TIMEDIFF_WIN_MAX];        /* 0.01 unit */
	u32     inferencefreq;/* can be max, min, avg, or avg2 depending on policy, usually avg*/
#endif
};

typedef int (*session_cb)(struct npu_session *);

struct npu_session_ops {
	int (*control)(struct npu_session *, struct npu_frame *);
	int (*request)(struct npu_session *, struct npu_frame *);
	int (*process)(struct npu_session *, struct npu_frame *);
	int (*cancel)(struct npu_session *, struct npu_frame *);
	int (*done)(struct npu_session *, struct npu_frame *);
	int (*get_resource)(struct npu_session *, struct npu_frame *);
	int (*put_resource)(struct npu_session *, struct npu_frame *);
	int (*update_param)(struct npu_session *, struct npu_frame *);
};

struct addr_info *find_addr_info(struct npu_session *session, u32 av_index, mem_opt_e mem_type, u32 cl_index);
npu_errno_t chk_nw_result_no_error(struct npu_session *session);
int npu_session_open(struct npu_session **session, void *cookie, void *memory);
int npu_session_close(struct npu_session *session);
int npu_session_s_graph(struct npu_session *session, struct vs4l_graph *info);
int npu_session_param(struct npu_session *session, struct vs4l_param_list *plist);
int npu_session_nw_sched_param(struct npu_session *session, struct vs4l_sched_param *param);
void npu_session_ion_sync_for_device(struct npu_memory_buffer *pbuf, off_t offset, size_t size,
					enum dma_data_direction dir);
int npu_session_NW_CMD_LOAD(struct npu_session *session);
int npu_session_NW_CMD_UNLOAD(struct npu_session *session);
int npu_session_NW_CMD_STREAMON(struct npu_session *session);
int npu_session_NW_CMD_STREAMOFF(struct npu_session *session);
int npu_session_register_undo_cb(struct npu_session *session, session_cb cb);
int npu_session_execute_undo_cb(struct npu_session *session);
int npu_session_undo_open(struct npu_session *session);
int npu_session_undo_s_graph(struct npu_session *session);
int npu_session_undo_close(struct npu_session *session);
int npu_session_flush(struct npu_session *session);

#endif
