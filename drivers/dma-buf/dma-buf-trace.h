// SPDX-License-Identifier: GPL-2.0
/*
 * dma-buf-trace header
 *
 * Copyright (c) 2020 Samsung Electronics Co., Ltd.
 */

#ifndef _DMA_BUF_TRACE_H_
#define _DMA_BUF_TRACE_H_

#include <linux/slab.h>
#include <linux/uaccess.h>
#include <linux/dma-buf.h>
#include <linux/miscdevice.h>
#include <linux/device.h>
#include <linux/mm_types.h>
#include <linux/types.h>
#include <linux/miscdevice.h>
#include <linux/spinlock.h>
#include <linux/types.h>

/* dma_buf_debugfs_dir is defined in dma-buf.c */
struct dentry;
extern struct dentry *dma_buf_debugfs_dir;

#ifdef CONFIG_DMABUF_TRACE
int dmabuf_trace_alloc(struct dma_buf *dmabuf);
void dmabuf_trace_free(struct dma_buf *dmabuf);
int dmabuf_trace_track_buffer(struct dma_buf *dmabuf);
int dmabuf_trace_untrack_buffer(struct dma_buf *dmabuf);
#else
static inline int dmabuf_trace_alloc(struct dma_buf *dmabuf)
{
	return 0;
}
static inline void dmabuf_trace_free(struct dma_buf *dmabuf)
{
}
static inline int dmabuf_trace_track_buffer(struct dma_buf *dmabuf)
{
	return 0;
}
static inline int dmabuf_trace_untrack_buffer(struct dma_buf *dmabuf)
{
	return 0;
}
#endif

#endif
