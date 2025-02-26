/* drivers/media/platform/exynos/mcfrc/mcfrc-helper.h
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

#ifndef MCFRC_HELPER_H_
#define MCFRC_HELPER_H_

#include <linux/dma-buf.h>
#include "mcfrc-core.h"

/**
 * mcfrc_set_dma_address - set DMA address
 */
static inline void mcfrc_set_dma_address(
		struct mcfrc_buffer_dma *buffer_dma,
		dma_addr_t dma_addr)
{
	buffer_dma->plane.dma_addr = dma_addr;
}

int mcfrc_map_dma_attachment(struct device *dev,
		     struct mcfrc_buffer_plane_dma *plane,
		     enum dma_data_direction dir);

void mcfrc_unmap_dma_attachment(struct device *dev,
			struct mcfrc_buffer_plane_dma *plane,
			enum dma_data_direction dir);

int mcfrc_dma_addr_map(struct device *dev,
		      struct mcfrc_buffer_dma *buf,
		      enum dma_data_direction dir);

void mcfrc_dma_addr_unmap(struct device *dev,
			 struct mcfrc_buffer_dma *buf);
/*
void mcfrc_sync_for_device(struct device *dev,
			  struct mcfrc_buffer_plane_dma *plane,
			  enum dma_data_direction dir);

void mcfrc_sync_for_cpu(struct device *dev,
		       struct mcfrc_buffer_plane_dma *plane,
		       enum dma_data_direction dir);
*/
#endif /* MCFRC_HELPER_H_ */
