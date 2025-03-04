// SPDX-License-Identifier: GPL-2.0
/*
 * Samsung Exynos SoC series dsp driver
 *
 * Copyright (c) 2019 Samsung Electronics Co., Ltd.
 *              http://www.samsung.com/
 */

#include <linux/io.h>
#include <linux/slab.h>

#include "dsp-log.h"
#include "dsp-util.h"

void dsp_util_queue_dump(struct dsp_util_queue *queue)
{
	unsigned long long kva_low, kva_high, kva;

	dsp_enter();
	dsp_notice("queue front(%u)/rear(%u)\n",
			readl(&queue->front), readl(&queue->rear));
	dsp_notice("queue data_size(%u)/data_count(%u)\n",
			readl(&queue->data_size), readl(&queue->data_count));

	kva_low = readl(&queue->kva_low);
	kva_high = readl(&queue->kva_high);
	kva = (kva_high << 32) | kva_low;

	dsp_notice("queue iova(%#x)/kva(%#llx)/size(%u)\n",
			readl(&queue->iova), kva, readl(&queue->size));
	dsp_leave();
}

int dsp_util_queue_read_count(struct dsp_util_queue *queue)
{
	return readl(&queue->data_count);
}

int dsp_util_queue_check_full(struct dsp_util_queue *queue)
{
	unsigned int count;
	unsigned int mirror_front, mirror_rear;
	unsigned int front, rear;

	dsp_check();
	count = readl(&queue->data_count);
	mirror_front = readl(&queue->front);
	mirror_rear = readl(&queue->rear);
	front = mirror_front % (count << 1);
	rear = mirror_rear % (count << 1);

	return (front == rear && mirror_front != mirror_rear);
}

int dsp_util_queue_enqueue(struct dsp_util_queue *queue, void *data,
		size_t data_size)
{
	int ret;
	unsigned int rear, mirror_rear;
	unsigned int q_data_size, q_data_count;
	unsigned long long kva_low, kva_high;
	void *kva;

	dsp_enter();
	q_data_size = readl(&queue->data_size);
	if (data_size > q_data_size) {
		ret = -EINVAL;
		dsp_err("size(%zu) can't be greater than data_size(%u) of q\n",
				data_size, q_data_size);
		goto p_err;
	}

	q_data_count = readl(&queue->data_count);
	mirror_rear = readl(&queue->rear);
	rear = mirror_rear % q_data_count;

	kva_low = readl(&queue->kva_low);
	kva_high = readl(&queue->kva_high);
	kva = (void *)((kva_high << 32) | kva_low);
	memcpy(kva + (rear * q_data_size), data, data_size);
	writel((mirror_rear + 1) % (q_data_count << 1), &queue->rear);

	dsp_leave();
	return 0;
p_err:
	return ret;
}

int dsp_util_queue_check_empty(struct dsp_util_queue *queue)
{
	return readl(&queue->front) == readl(&queue->rear);
}

int dsp_util_queue_dequeue(struct dsp_util_queue *queue, void *data,
		size_t data_size)
{
	int ret;
	unsigned int front, mirror_front;
	unsigned int q_data_size, q_data_count;
	unsigned long long kva_low, kva_high;
	void *kva;

	dsp_enter();
	q_data_size = readl(&queue->data_size);
	if (data_size > q_data_size) {
		ret = -EINVAL;
		dsp_err("size(%zu) can't be greater than data_size(%u) of q\n",
				data_size, q_data_size);
		goto p_err;
	}

	q_data_count = readl(&queue->data_count);
	mirror_front = readl(&queue->front);
	front = mirror_front % q_data_count;

	kva_low = readl(&queue->kva_low);
	kva_high = readl(&queue->kva_high);
	kva = (void *)((kva_high << 32) | kva_low);
	memcpy(data, kva + (front * q_data_size), data_size);
	writel((mirror_front + 1) % (q_data_count << 1), &queue->front);

	dsp_leave();
	return 0;
p_err:
	return ret;
}

int dsp_util_queue_init(struct dsp_util_queue *queue, unsigned int data_size,
		unsigned int queue_size, unsigned int iova,
		unsigned long long kva)
{
	int ret;

	dsp_enter();
	if (!data_size || !queue_size || !iova || !kva) {
		ret = -EINVAL;
		dsp_err("queue parameter can't be zero(%u/%u/%u/%llu)\n",
				data_size, queue_size, iova, kva);
		goto p_err;
	}

	if (data_size > queue_size) {
		ret = -EINVAL;
		dsp_err("data can't be greater than the entire queue(%u/%u)\n",
				data_size, queue_size);
		goto p_err;
	}

	writel(0, &queue->front);
	writel(0, &queue->rear);
	writel(data_size, &queue->data_size);
	writel(queue_size / data_size, &queue->data_count);
	writel(iova, &queue->iova);
	writel(queue_size, &queue->size);
	writel(kva & 0xffffffff, &queue->kva_low);
	writel(kva >> 32, &queue->kva_high);
	dsp_leave();
	return 0;
p_err:
	return ret;
}

void dsp_util_bitmap_dump(struct dsp_util_bitmap *map)
{
	dsp_enter();
	dsp_notice("bitmap[%s] dump : size(%u)/used_size(%u)/base_bit(%u)\n",
			map->name, map->bitmap_size, map->used_size,
			map->base_bit);
	print_hex_dump(KERN_NOTICE, "[Exynos][DSP][NOTICE]: bitmap raw: ",
			DUMP_PREFIX_NONE, 32, 8, map->bitmap,
			BITS_TO_LONGS(map->bitmap_size) * sizeof(long), false);
	dsp_leave();
}

int dsp_util_bitmap_set_region(struct dsp_util_bitmap *map, unsigned int size)
{
	int ret;
	unsigned long start, end, check;
	bool turn = false;

	dsp_enter();
	if (!size) {
		ret = -EINVAL;
		dsp_err("Invalid bitmap size[%s](%u)\n", map->name, size);
		goto p_err;
	}

	if (size > map->bitmap_size - map->used_size) {
		ret = -ENOMEM;
		dsp_err("Not enough bitmap[%s](%u)\n", map->name, size);
		goto p_err;
	}

	start = map->base_bit;
again:
	start = find_next_zero_bit(map->bitmap, map->bitmap_size, start);

	end = start + size - 1;
	if (end >= map->bitmap_size) {
		if (turn) {
			ret = -ENOMEM;
			dsp_err("Not enough contiguous bitmap[%s](%u)\n",
					map->name, size);
			goto p_err;
		} else {
			turn = true;
			start = 0;
			goto again;
		}
	}

	check = find_next_bit(map->bitmap, end, start);
	if (check < end) {
		start = check + 1;
		goto again;
	}

	bitmap_set(map->bitmap, start, size);
	map->base_bit = end + 1;
	map->used_size += size;

	dsp_leave();
	return start;
p_err:
	dsp_util_bitmap_dump(map);
	return ret;
}

void dsp_util_bitmap_clear_region(struct dsp_util_bitmap *map,
		unsigned int start, unsigned int size)
{
	dsp_enter();
	if ((map->bitmap_size < start + size - 1) ||
			size > map->used_size) {
		dsp_warn("Invalid clear parameter[%s](%u/%u)\n",
				map->name, start, size);
		dsp_util_bitmap_dump(map);
		return;
	}

	map->used_size -= size;
	bitmap_clear(map->bitmap, start, size);
	dsp_leave();
}

int dsp_util_bitmap_init(struct dsp_util_bitmap *map, const char *name,
		unsigned int size)
{
	int ret;

	dsp_enter();
	if (!size) {
		ret = -EINVAL;
		dsp_err("bitmap size can not be zero\n");
		goto p_err;
	}

	map->bitmap = kzalloc(BITS_TO_LONGS(size) * sizeof(long), GFP_KERNEL);
	if (!map->bitmap) {
		ret = -ENOMEM;
		dsp_err("Failed to init bitmap(%u/%lu)\n",
				size, BITS_TO_LONGS(size) * sizeof(long));
		goto p_err;
	}

	snprintf(map->name, DSP_BITMAP_NAME_LEN, "%s", name);
	map->bitmap_size = size;
	map->used_size = 0;
	map->base_bit = 0;

	dsp_leave();
	return 0;
p_err:
	return ret;
}

void dsp_util_bitmap_deinit(struct dsp_util_bitmap *map)
{
	dsp_enter();
	kfree(map->bitmap);
	dsp_leave();
}
