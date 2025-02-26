/*
 * Samsung Exynos9 SoC series FIMC-IS driver
 *
 *
 * Copyright (c) 2017 Samsung Electronics Co., Ltd
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#ifndef IS_COMMON_ENUM_H
#define IS_COMMON_ENUM_H

/*
 * pixel type
 * [0:5] : pixel size
 * [6:7] : extra
 */
#define PIXEL_TYPE_SIZE_MASK			0x3F
#define	PIXEL_TYPE_SIZE_SHIFT			0

#define PIXEL_TYPE_EXTRA_MASK			0xC0
#define	PIXEL_TYPE_EXTRA_SHIFT			6
enum camera_pixel_size {
	CAMERA_PIXEL_SIZE_8BIT = 0,
	CAMERA_PIXEL_SIZE_10BIT,
	CAMERA_PIXEL_SIZE_PACKED_10BIT,
	CAMERA_PIXEL_SIZE_8_2BIT,
	CAMERA_PIXEL_SIZE_12BIT,
	CAMERA_PIXEL_SIZE_13BIT,
	CAMERA_PIXEL_SIZE_14BIT,
	CAMERA_PIXEL_SIZE_15BIT,
	CAMERA_PIXEL_SIZE_16BIT,
	CAMERA_PIXEL_SIZE_PACKED_12BIT, /* DON'T USE, JUST FOR ENUM SYNC WITH HAL */
	CAMERA_PIXEL_SIZE_AND10,
};

/*
 * COMP : Lossless Compression
 * COMP_LOSS : Loss Compression
 */
enum camera_extra {
	NONE = 0,
	COMP,
	COMP_LOSS
};

#endif
