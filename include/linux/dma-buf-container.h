/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Dma buf container header
 *
 * Copyright (c) 2020 Samsung Electronics Co., Ltd.
 */

#ifndef _LINUX_DMA_BUF_CONTAINER_H_
#define _LINUX_DMA_BUF_CONTAINER_H_

int dmabuf_container_get_count(struct dma_buf *dmabuf);
struct dma_buf *dmabuf_container_get_buffer(struct dma_buf *dmabuf, int index);
int dmabuf_container_set_mask(struct dma_buf *dmabuf, u64 mask);
int dmabuf_container_get_mask(struct dma_buf *dmabuf, u64 *mask);

#endif /* _LINUX_DMA_BUF_CONTAINER_H_ */
