/*
* Samsung Exynos5 SoC series Camera PostProcessing driver
 *
 * Copyright (c) 2016 Samsung Electronics Co., Ltd
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#ifndef CAMERAPP_VIDEO_H
#define CAMERAPP_VIDEO_H

#include <linux/videodev2.h>

/* video node */
#define CAMERAPP_VIDEONODE_GDC	55
#define CAMERAPP_VIDEONODE_STR  56

/* EXYNOS_VIDEONODE_FIMC_IS (100) + x */
#define EXYNOS_VIDEONODE_CAMERAPP(x)		(100 + x)


/* related Data format */

/*
 * pixel flag
 * [0:5] : pixel size
 * [6:7] : extra
 */
#define CAMERAPP_PIXEL_SIZE_MASK		0x3F
#define	CAMERAPP_PIXEL_SIZE_SHIFT		0

#define CAMERAPP_EXTRA_MASK			0xC0
#define	CAMERAPP_EXTRA_SHIFT			6

/* Block size for YUV format compression */
#define CAMERAPP_COMP_BLOCK_WIDTH		32
#define CAMERAPP_COMP_BLOCK_HEIGHT		4

enum camerapp_pixel_size {
	CAMERAPP_PIXEL_SIZE_8BIT = 0,
	CAMERAPP_PIXEL_SIZE_10BIT,
	CAMERAPP_PIXEL_SIZE_PACKED_10BIT,
	CAMERAPP_PIXEL_SIZE_8_2BIT,
};

enum camerapp_extra {
	NONE = 0,
	COMP,
	COMP_LOSS,
};

#endif
