/*
 * Copyright (c) 2017 Samsung Electronics Co., Ltd.
 *		http://www.samsung.com
 *
 * Header file for Exynos Repeater driver
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#ifndef _EXYNOS_REPEATER_H_
#define _EXYNOS_REPEATER_H_

#include <linux/dma-buf.h>

#define MAX_SHARED_BUF_NUM	3

/**
 * struct shared_buffer_info
 *
 * @pixel_format	: pixel_format of bufs
 * @width		: width of bufs
 * @height		: height of bufs
 * @buffer_count	: valid buffer count of bufs
 * @bufs		: pointer of struct dma_buf for buffer sharing.
 */
struct shared_buffer_info {
	int pixel_format;
	int width;
	int height;
	int buffer_count;
	struct dma_buf *bufs[MAX_SHARED_BUF_NUM];
};

/*
 * struct repeater_encoding_param
 * @time_stamp	: timestamp value
 */
struct repeater_encoding_param {
	u64 time_stamp;
};

#endif /* _EXYNOS_REPEATER_H_ */
