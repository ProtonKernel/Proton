/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Samsung Exynos SoC series dsp driver
 *
 * Copyright (c) 2019 Samsung Electronics Co., Ltd.
 *              http://www.samsung.com/
 */

#ifndef __DSP_UTIL_H__
#define __DSP_UTIL_H__

#include <linux/bitmap.h>

#define DSP_BITMAP_NAME_LEN		(16)

struct dsp_util_queue {
	unsigned int			front;
	unsigned int			rear;
	unsigned int			data_size;
	unsigned int			data_count;
	unsigned int			iova;
	unsigned int			size;
	union {
		struct {
			unsigned int	kva_low;
			unsigned int	kva_high;
		};
		void			*kva;
	};
};

struct dsp_util_bitmap {
	char				name[DSP_BITMAP_NAME_LEN];
	unsigned long			*bitmap;
	unsigned int			bitmap_size;
	unsigned int			used_size;
	unsigned int			base_bit;
};

void dsp_util_queue_dump(struct dsp_util_queue *queue);
int dsp_util_queue_read_count(struct dsp_util_queue *queue);
int dsp_util_queue_check_full(struct dsp_util_queue *queue);
int dsp_util_queue_enqueue(struct dsp_util_queue *queue, void *data,
		size_t data_size);
int dsp_util_queue_check_empty(struct dsp_util_queue *queue);
int dsp_util_queue_dequeue(struct dsp_util_queue *queue, void *data,
		size_t data_size);
int dsp_util_queue_init(struct dsp_util_queue *queue, unsigned int data_size,
		unsigned int queue_size, unsigned int iova,
		unsigned long long kva);

void dsp_util_bitmap_dump(struct dsp_util_bitmap *map);
int dsp_util_bitmap_set_region(struct dsp_util_bitmap *map, unsigned int size);
void dsp_util_bitmap_clear_region(struct dsp_util_bitmap *map,
		unsigned int start, unsigned int size);
int dsp_util_bitmap_init(struct dsp_util_bitmap *map, const char *name,
		unsigned int size);
void dsp_util_bitmap_deinit(struct dsp_util_bitmap *map);

#endif
