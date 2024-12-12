/*
* Samsung Exynos5 SoC series FIMC-IS driver
 *
 * exynos5 fimc-is video functions
 *
 * Copyright (c) 2011 Samsung Electronics Co., Ltd
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#include <linux/module.h>
#include <linux/delay.h>
#include <linux/kernel.h>
#include <linux/errno.h>
#include <linux/interrupt.h>
#include <linux/device.h>
#include <linux/platform_device.h>
#include <linux/slab.h>
#include <linux/firmware.h>
#include <linux/dma-mapping.h>
#include <linux/scatterlist.h>
#include <linux/videodev2.h>
#include <linux/videodev2_exynos_camera.h>
#include <linux/v4l2-mediabus.h>
#include <linux/bug.h>
#include <linux/syscalls.h>
#include <linux/videodev2_exynos_media.h>
#include <linux/dma-buf.h>
#include <linux/moduleparam.h>

#include <media/videobuf2-v4l2.h>
#include <media/videobuf2-core.h>
#include <media/v4l2-ctrls.h>
#include <media/v4l2-device.h>
#include <media/v4l2-mem2mem.h>
#include <media/v4l2-mediabus.h>

#include "is-time.h"
#include "is-core.h"
#include "is-param.h"
#include "is-cmd.h"
#include "is-err.h"
#include "is-debug.h"
#include "is-mem.h"
#include "is-video.h"

/*
 * copy from 'include/media/v4l2-ioctl.h'
 * #define V4L2_DEV_DEBUG_IOCTL		0x01
 * #define V4L2_DEV_DEBUG_IOCTL_ARG	0x02
 * #define V4L2_DEV_DEBUG_FOP		0x04
 * #define V4L2_DEV_DEBUG_STREAMING	0x08
 * #define V4L2_DEV_DEBUG_POLL		0x10
 */
#define V4L2_DEV_DEBUG_DMA	0x100

#define DDD_CTRL_IMAGE		0x01
#define DDD_CTRL_META		0x02
#define DDD_CTRL_TYPE_MASK

#define DDD_TYPE_PERIOD		0 /* all frame count matching period	 */
#define DDD_TYPE_INTERVAL	1 /* from any frame count to greater one */
#define DDD_TYPE_ONESHOT	2 /* specific frame count only		 */

extern unsigned int dbg_draw_digit_ctrl;
unsigned int dbg_dma_dump_ctrl;
unsigned int dbg_dma_dump_type = DDD_TYPE_PERIOD;
unsigned int dbg_dma_dump_arg1 = 30;	/* every frame(s) */
unsigned int dbg_dma_dump_arg2;		/* for interval type */

static int param_get_dbg_dma_dump_ctrl(char *buffer, const struct kernel_param *kp)
{
	int ret;

	ret = sprintf(buffer, "DMA dump control: ");

	if (!dbg_dma_dump_ctrl) {
		ret += sprintf(buffer + ret, "None\n");
		ret += sprintf(buffer + ret, "\t- image(0x1)\n");
		ret += sprintf(buffer + ret, "\t- meta (0x2)\n");
	} else {
		if (dbg_dma_dump_ctrl & DDD_CTRL_IMAGE)
			ret += sprintf(buffer + ret, "dump image(0x1) | ");
		if (dbg_dma_dump_ctrl & DDD_CTRL_META)
			ret += sprintf(buffer + ret, "dump meta (0x2) | ");

		ret -= 3;
		ret += sprintf(buffer + ret, "\n");
	}

	return ret;
}

static const struct kernel_param_ops param_ops_dbg_dma_dump_ctrl = {
	.set = param_set_uint,
	.get = param_get_dbg_dma_dump_ctrl,
};

static int param_get_dbg_dma_dump_type(char *buffer, const struct kernel_param *kp)
{
	int ret;

	ret = sprintf(buffer, "DMA dump type: selected(*)\n");
	ret += sprintf(buffer + ret, dbg_dma_dump_type == DDD_TYPE_PERIOD ?
				"\t- period(0)*\n" : "\t- period(0)\n");
	ret += sprintf(buffer + ret, dbg_dma_dump_type == DDD_TYPE_INTERVAL ?
				"\t- interval(1)*\n" : "\t- interval(1)\n");
	ret += sprintf(buffer + ret, dbg_dma_dump_type == DDD_TYPE_ONESHOT ?
				"\t- oneshot(2)*\n" : "\t- oneshot(2)\n");

	return ret;
}

static const struct kernel_param_ops param_ops_dbg_dma_dump_type = {
	.set = param_set_uint,
	.get = param_get_dbg_dma_dump_type,
};

module_param_cb(dma_dump_ctrl, &param_ops_dbg_dma_dump_ctrl,
			&dbg_dma_dump_ctrl, S_IRUGO | S_IWUSR);
module_param_cb(dma_dump_type, &param_ops_dbg_dma_dump_type,
			&dbg_dma_dump_type, S_IRUGO | S_IWUSR);
module_param_named(dma_dump_arg1, dbg_dma_dump_arg1, uint,
			S_IRUGO | S_IWUSR);
module_param_named(dma_dump_arg2, dbg_dma_dump_arg2, uint,
			S_IRUGO | S_IWUSR);

struct is_fmt is_formats[] = {
	{
		.name		= "YUV 4:4:4 packed, YCbCr",
		.pixelformat	= V4L2_PIX_FMT_YUV444,
		.num_planes	= 1 + NUM_OF_META_PLANE,
		.mbus_code	= 0, /* Not Defined */
		.bitsperpixel	= { 24 },
		.hw_format	= DMA_INOUT_FORMAT_YUV444,
		.hw_order	= DMA_INOUT_ORDER_YCbCr,
		.hw_bitwidth	= DMA_INOUT_BIT_WIDTH_8BIT,
		.hw_plane	= DMA_INOUT_PLANE_1,
	}, {
		.name		= "YUV 4:2:2 packed, YCbYCr",
		.pixelformat	= V4L2_PIX_FMT_YUYV,
		.num_planes	= 1 + NUM_OF_META_PLANE,
		.mbus_code	= MEDIA_BUS_FMT_YUYV8_2X8,
		.bitsperpixel	= { 16 },
		.hw_format	= DMA_INOUT_FORMAT_YUV422,
		.hw_order	= DMA_INOUT_ORDER_YCbYCr,
		.hw_bitwidth	= DMA_INOUT_BIT_WIDTH_8BIT,
		.hw_plane	= DMA_INOUT_PLANE_1,
	}, {
		.name		= "YUV 4:2:2 packed, YCbYCr",
		.pixelformat	= V4L2_PIX_FMT_YUYV,
		.num_planes	= 1 + NUM_OF_META_PLANE,
		.mbus_code	= MEDIA_BUS_FMT_YUYV8_2X8,
		.bitsperpixel	= { 16 },
		.hw_format	= DMA_INOUT_FORMAT_YUV422,
		.hw_order	= DMA_INOUT_ORDER_YCbYCr,
		.hw_bitwidth	= DMA_INOUT_BIT_WIDTH_10BIT,
		.hw_plane	= DMA_INOUT_PLANE_1,
	}, {
		.name		= "YUV 4:2:2 packed, CbYCrY",
		.pixelformat	= V4L2_PIX_FMT_UYVY,
		.num_planes	= 1 + NUM_OF_META_PLANE,
		.mbus_code	= MEDIA_BUS_FMT_UYVY8_2X8,
		.bitsperpixel	= { 16 },
		.hw_format	= DMA_INOUT_FORMAT_YUV422,
		.hw_order	= DMA_INOUT_ORDER_CbYCrY,
		.hw_bitwidth	= DMA_INOUT_BIT_WIDTH_8BIT,
		.hw_plane	= DMA_INOUT_PLANE_1,
	}, {
		.name		= "YUV 4:2:2 planar, Y/CbCr",
		.pixelformat	= V4L2_PIX_FMT_NV16,
		.num_planes	= 1 + NUM_OF_META_PLANE,
		.bitsperpixel	= { 8, 8 },
		.hw_format	= DMA_INOUT_FORMAT_YUV422,
		.hw_order	= DMA_INOUT_ORDER_CbCr,
		.hw_bitwidth	= DMA_INOUT_BIT_WIDTH_8BIT,
		.hw_plane	= DMA_INOUT_PLANE_2,
	}, {
		.name		= "YUV 4:2:2 planar, Y/CbCr",
		.pixelformat	= V4L2_PIX_FMT_NV61,
		.num_planes	= 1 + NUM_OF_META_PLANE,
		.bitsperpixel	= { 8, 8 },
		.hw_format	= DMA_INOUT_FORMAT_YUV422,
		.hw_order	= DMA_INOUT_ORDER_CrCb,
		.hw_bitwidth	= DMA_INOUT_BIT_WIDTH_8BIT,
		.hw_plane	= DMA_INOUT_PLANE_2,
	}, {
		.name		= "YUV 4:2:2 non-contiguous 2-planar,, Y/CbCr",
		.pixelformat	= V4L2_PIX_FMT_NV16M,
		.num_planes	= 2 + NUM_OF_META_PLANE,
		.bitsperpixel	= { 8, 8 },
		.hw_format	= DMA_INOUT_FORMAT_YUV422,
		.hw_order	= DMA_INOUT_ORDER_CbCr,
		.hw_bitwidth	= DMA_INOUT_BIT_WIDTH_8BIT,
		.hw_plane	= DMA_INOUT_PLANE_2,
	}, {
		.name		= "YUV 4:2:2 non-contiguous 2-planar, Y/CbCr",
		.pixelformat	= V4L2_PIX_FMT_NV61M,
		.num_planes	= 2 + NUM_OF_META_PLANE,
		.bitsperpixel	= { 8, 8 },
		.hw_format	= DMA_INOUT_FORMAT_YUV422,
		.hw_order	= DMA_INOUT_ORDER_CrCb,
		.hw_bitwidth	= DMA_INOUT_BIT_WIDTH_8BIT,
		.hw_plane	= DMA_INOUT_PLANE_2,
	}, {
		.name		= "YUV 4:2:2 planar, Y/Cb/Cr",
		.pixelformat	= V4L2_PIX_FMT_YUV422P,
		.num_planes	= 1 + NUM_OF_META_PLANE,
		.bitsperpixel	= { 8, 8, 8 },
		.hw_format	= DMA_INOUT_FORMAT_YUV422,
		.hw_order	= DMA_INOUT_ORDER_NO,
		.hw_bitwidth	= DMA_INOUT_BIT_WIDTH_8BIT,
		.hw_plane	= DMA_INOUT_PLANE_3,
	}, {
		.name		= "YUV 4:2:0 planar, YCbCr",
		.pixelformat	= V4L2_PIX_FMT_YUV420,
		.num_planes	= 1 + NUM_OF_META_PLANE,
		.bitsperpixel	= { 8, 4, 4 },
		.hw_format	= DMA_INOUT_FORMAT_YUV420,
		.hw_order	= DMA_INOUT_ORDER_NO,
		.hw_bitwidth	= DMA_INOUT_BIT_WIDTH_8BIT,
		.hw_plane	= DMA_INOUT_PLANE_3,
	}, {
		.name		= "YUV 4:2:0 planar, YCbCr",
		.pixelformat	= V4L2_PIX_FMT_YVU420,
		.num_planes	= 1 + NUM_OF_META_PLANE,
		.bitsperpixel	= { 8, 4, 4 },
		.hw_format	= DMA_INOUT_FORMAT_YUV420,
		.hw_order	= DMA_INOUT_ORDER_NO,
		.hw_bitwidth	= DMA_INOUT_BIT_WIDTH_8BIT,
		.hw_plane	= DMA_INOUT_PLANE_3,
	}, {
		.name		= "YUV 4:2:0 planar, Y/CbCr",
		.pixelformat	= V4L2_PIX_FMT_NV12,
		.num_planes	= 1 + NUM_OF_META_PLANE,
		.bitsperpixel	= { 8, 8 },
		.hw_format	= DMA_INOUT_FORMAT_YUV420,
		.hw_order	= DMA_INOUT_ORDER_CbCr,
		.hw_bitwidth	= DMA_INOUT_BIT_WIDTH_8BIT,
		.hw_plane	= DMA_INOUT_PLANE_2,
	}, {
		.name		= "YUV 4:2:0 planar, Y/CrCb",
		.pixelformat	= V4L2_PIX_FMT_NV21,
		.num_planes	= 1 + NUM_OF_META_PLANE,
		.bitsperpixel	= { 8, 8 },
		.hw_format	= DMA_INOUT_FORMAT_YUV420,
		.hw_order	= DMA_INOUT_ORDER_CrCb,
		.hw_bitwidth	= DMA_INOUT_BIT_WIDTH_8BIT,
		.hw_plane	= DMA_INOUT_PLANE_2,
	}, {
		.name		= "YUV 4:2:0 non-contiguous 2-planar, Y/CbCr",
		.pixelformat	= V4L2_PIX_FMT_NV12M,
		.num_planes	= 2 + NUM_OF_META_PLANE,
		.bitsperpixel	= { 8, 8 },
		.hw_format	= DMA_INOUT_FORMAT_YUV420,
		.hw_order	= DMA_INOUT_ORDER_CbCr,
		.hw_bitwidth	= DMA_INOUT_BIT_WIDTH_8BIT,
		.hw_plane	= DMA_INOUT_PLANE_2,
	}, {
		.name		= "YVU 4:2:0 non-contiguous 2-planar, Y/CrCb",
		.pixelformat	= V4L2_PIX_FMT_NV21M,
		.num_planes	= 2 + NUM_OF_META_PLANE,
		.bitsperpixel	= { 8, 8 },
		.hw_format	= DMA_INOUT_FORMAT_YUV420,
		.hw_order	= DMA_INOUT_ORDER_CrCb,
		.hw_bitwidth	= DMA_INOUT_BIT_WIDTH_8BIT,
		.hw_plane	= DMA_INOUT_PLANE_2,
	}, {
		.name		= "YUV 4:2:0 non-contiguous 3-planar, Y/Cb/Cr",
		.pixelformat	= V4L2_PIX_FMT_YUV420M,
		.num_planes	= 3 + NUM_OF_META_PLANE,
		.bitsperpixel	= { 8, 4, 4 },
		.hw_format	= DMA_INOUT_FORMAT_YUV420,
		.hw_order	= DMA_INOUT_ORDER_NO,
		.hw_bitwidth	= DMA_INOUT_BIT_WIDTH_8BIT,
		.hw_plane	= DMA_INOUT_PLANE_3,
	}, {
		.name		= "YUV 4:2:0 non-contiguous 3-planar, Y/Cr/Cb",
		.pixelformat	= V4L2_PIX_FMT_YVU420M,
		.num_planes	= 3 + NUM_OF_META_PLANE,
		.bitsperpixel	= { 8, 4, 4 },
		.hw_format	= DMA_INOUT_FORMAT_YUV420,
		.hw_order	= DMA_INOUT_ORDER_NO,
		.hw_bitwidth	= DMA_INOUT_BIT_WIDTH_8BIT,
		.hw_plane	= DMA_INOUT_PLANE_3,
	}, {
		.name		= "BAYER 8 bit(GRBG)",
		.pixelformat	= V4L2_PIX_FMT_SGRBG8,
		.num_planes	= 1 + NUM_OF_META_PLANE,
		.bitsperpixel	= { 8 },
		.hw_format	= DMA_INOUT_FORMAT_BAYER,
		.hw_order	= DMA_INOUT_ORDER_GB_BG,
		.hw_bitwidth	= DMA_INOUT_BIT_WIDTH_8BIT,
		.hw_plane	= DMA_INOUT_PLANE_1,
	}, {
		.name		= "BAYER 8 bit(BA81)",
		.pixelformat	= V4L2_PIX_FMT_SBGGR8,
		.num_planes	= 1 + NUM_OF_META_PLANE,
		.bitsperpixel	= { 8 },
		.hw_format	= DMA_INOUT_FORMAT_BAYER,
		.hw_order	= DMA_INOUT_ORDER_GB_BG,
		.hw_bitwidth	= DMA_INOUT_BIT_WIDTH_8BIT,
		.hw_plane	= DMA_INOUT_PLANE_1,
	}, {
		.name		= "BAYER 10 bit",
		.pixelformat	= V4L2_PIX_FMT_SBGGR10,
		.num_planes	= 1 + NUM_OF_META_PLANE,
		.bitsperpixel	= { 10 },
		.hw_format	= DMA_INOUT_FORMAT_BAYER,
		.hw_order	= DMA_INOUT_ORDER_GB_BG,
		.hw_bitwidth	= DMA_INOUT_BIT_WIDTH_10BIT,
		.hw_plane	= DMA_INOUT_PLANE_1,
	}, {
		.name		= "BAYER 12 bit",
		.pixelformat	= V4L2_PIX_FMT_SBGGR12,
		.num_planes	= 1 + NUM_OF_META_PLANE,
		.bitsperpixel	= { 12 },
		.hw_format	= DMA_INOUT_FORMAT_BAYER,
		.hw_order	= DMA_INOUT_ORDER_GB_BG,
		.hw_bitwidth	= DMA_INOUT_BIT_WIDTH_12BIT,
		.hw_plane	= DMA_INOUT_PLANE_1,
	}, {
		.name		= "BAYER 16 bit",
		.pixelformat	= V4L2_PIX_FMT_SBGGR16,
		.num_planes	= 1 + NUM_OF_META_PLANE,
		.bitsperpixel	= { 16 },
		.hw_format	= DMA_INOUT_FORMAT_BAYER,
		.hw_order	= DMA_INOUT_ORDER_GB_BG,
		.hw_bitwidth	= DMA_INOUT_BIT_WIDTH_16BIT,
		.hw_plane	= DMA_INOUT_PLANE_1,
	}, {
		.name		= "BAYER 10 bit packed",
		.pixelformat	= V4L2_PIX_FMT_SBGGR10P,
		.num_planes	= 1 + NUM_OF_META_PLANE,
		.bitsperpixel	= { 10 },
		.hw_format	= DMA_INOUT_FORMAT_BAYER_PACKED,
		.hw_order	= DMA_INOUT_ORDER_GB_BG,
		.hw_bitwidth	= DMA_INOUT_BIT_WIDTH_10BIT,
		.hw_plane	= DMA_INOUT_PLANE_1,
	}, {
		.name		= "BAYER 12 bit packed",
		.pixelformat	= V4L2_PIX_FMT_SBGGR12P,
		.num_planes	= 1 + NUM_OF_META_PLANE,
		.bitsperpixel	= { 12 },
		.hw_format	= DMA_INOUT_FORMAT_BAYER_PACKED,
		.hw_order	= DMA_INOUT_ORDER_GB_BG,
		.hw_bitwidth	= DMA_INOUT_BIT_WIDTH_12BIT,
		.hw_plane	= DMA_INOUT_PLANE_1,
	}, {
		.name		= "ARGB8888",
		.pixelformat	= V4L2_PIX_FMT_RGB32,
		.num_planes	= 1 + NUM_OF_META_PLANE,
		.bitsperpixel	= { 32 },
		.hw_format	= DMA_INOUT_FORMAT_RGB,
		.hw_order	= DMA_INOUT_ORDER_ARGB,
		.hw_bitwidth	= DMA_INOUT_BIT_WIDTH_32BIT,
		.hw_plane	= DMA_INOUT_PLANE_1,
	}, {
		.name		= "RGB888 planar",
		.pixelformat	= V4L2_PIX_FMT_RGB24,
		.num_planes	= 1 + NUM_OF_META_PLANE,
		.bitsperpixel	= { 24 },
		.hw_format	= DMA_INOUT_FORMAT_RGB,
		.hw_order	= DMA_INOUT_ORDER_BGR,
		.hw_bitwidth	= DMA_INOUT_BIT_WIDTH_8BIT,
		.hw_plane	= DMA_INOUT_PLANE_3,
	}, {
		.name		= "BGR888 planar",
		.pixelformat	= V4L2_PIX_FMT_BGR24,
		.num_planes	= 1 + NUM_OF_META_PLANE,
		.bitsperpixel	= { 24 },
		.hw_format	= DMA_INOUT_FORMAT_RGB,
		.hw_order	= DMA_INOUT_ORDER_BGR,
		.hw_bitwidth	= DMA_INOUT_BIT_WIDTH_8BIT,
		.hw_plane	= DMA_INOUT_PLANE_3,
	}, {
		.name		= "Y 8bit",
		.pixelformat	= V4L2_PIX_FMT_GREY,
		.num_planes	= 1 + NUM_OF_META_PLANE,
		.bitsperpixel	= { 8 },
		.hw_format	= DMA_INOUT_FORMAT_Y,
		.hw_order	= DMA_INOUT_ORDER_NO,
		.hw_bitwidth	= DMA_INOUT_BIT_WIDTH_8BIT,
		.hw_plane	= DMA_INOUT_PLANE_1,
	}, {
		.name		= "Y 10bit",
		.pixelformat	= V4L2_PIX_FMT_Y10,
		.num_planes	= 1 + NUM_OF_META_PLANE,
		.bitsperpixel	= { 16 },
		.hw_format	= DMA_INOUT_FORMAT_Y,
		.hw_order	= DMA_INOUT_ORDER_NO,
		.hw_bitwidth	= DMA_INOUT_BIT_WIDTH_16BIT,
		.hw_plane	= DMA_INOUT_PLANE_1,
	}, {
		.name		= "Y 12bit",
		.pixelformat	= V4L2_PIX_FMT_Y12,
		.num_planes	= 1 + NUM_OF_META_PLANE,
		.bitsperpixel	= { 16 },
		.hw_format	= DMA_INOUT_FORMAT_Y,
		.hw_order	= DMA_INOUT_ORDER_NO,
		.hw_bitwidth	= DMA_INOUT_BIT_WIDTH_16BIT,
		.hw_plane	= DMA_INOUT_PLANE_1,
	}, {
		.name		= "Y Packed 10bit",
		.pixelformat	= V4L2_PIX_FMT_Y10BPACK,
		.num_planes	= 1 + NUM_OF_META_PLANE,
		.bitsperpixel	= { 10 },
		.hw_format	= DMA_INOUT_FORMAT_Y,
		.hw_order	= DMA_INOUT_ORDER_NO,
		.hw_bitwidth	= DMA_INOUT_BIT_WIDTH_10BIT,
		.hw_plane	= DMA_INOUT_PLANE_1,
	}, {
		.name		= "YUV32",
		.pixelformat	= V4L2_PIX_FMT_YUV32,
		.num_planes = 2 + NUM_OF_META_PLANE,
		.bitsperpixel	= { 8, 8 },
		.hw_format	= DMA_INOUT_FORMAT_Y,
		.hw_order	= DMA_INOUT_ORDER_NO,
		.hw_bitwidth	= DMA_INOUT_BIT_WIDTH_32BIT,
		.hw_plane	= DMA_INOUT_PLANE_2,
	}, {
		.name		= "P210_16B",
		.pixelformat	= V4L2_PIX_FMT_NV16M_P210,
		.num_planes	= 2 + NUM_OF_META_PLANE,
		.bitsperpixel	= { 16, 16 },
		.hw_format	= DMA_INOUT_FORMAT_YUV422,
		.hw_order	= DMA_INOUT_ORDER_CbCr,
		.hw_bitwidth	= DMA_INOUT_BIT_WIDTH_16BIT,
		.hw_plane	= DMA_INOUT_PLANE_2,
	}, {
		.name		= "P210_10B",
		.pixelformat	= V4L2_PIX_FMT_NV16M_P210,
		.num_planes	= 2 + NUM_OF_META_PLANE,
		.bitsperpixel	= { 10, 10 },
		.hw_format	= DMA_INOUT_FORMAT_YUV422,
		.hw_order	= DMA_INOUT_ORDER_CbCr,
		.hw_bitwidth	= DMA_INOUT_BIT_WIDTH_10BIT,
		.hw_plane	= DMA_INOUT_PLANE_2,
	}, {
		.name		= "P010_16B",
		.pixelformat	= V4L2_PIX_FMT_NV12M_P010,
		.num_planes	= 2 + NUM_OF_META_PLANE,
		.bitsperpixel	= { 16, 16 },
		.hw_format	= DMA_INOUT_FORMAT_YUV420,
		.hw_order	= DMA_INOUT_ORDER_CbCr,
		.hw_bitwidth	= DMA_INOUT_BIT_WIDTH_16BIT,
		.hw_plane	= DMA_INOUT_PLANE_2,
	}, {
		.name		= "P010_10B",
		.pixelformat	= V4L2_PIX_FMT_NV12M_P010,
		.num_planes	= 2 + NUM_OF_META_PLANE,
		.bitsperpixel	= { 10, 10 },
		.hw_format	= DMA_INOUT_FORMAT_YUV420,
		.hw_order	= DMA_INOUT_ORDER_CbCr,
		.hw_bitwidth	= DMA_INOUT_BIT_WIDTH_10BIT,
		.hw_plane	= DMA_INOUT_PLANE_2,
	}, {
		.name		= "YUV422 2P 10bit(8+2)",
		.pixelformat	= V4L2_PIX_FMT_NV16M_S10B,
		.num_planes	= 2 + NUM_OF_META_PLANE,
		.bitsperpixel	= { 8, 8 },
		.hw_format	= DMA_INOUT_FORMAT_YUV422,
		.hw_order	= DMA_INOUT_ORDER_CbCr,
		.hw_bitwidth	= DMA_INOUT_BIT_WIDTH_10BIT,
		.hw_plane	= DMA_INOUT_PLANE_4,
	}, {
		.name		= "YUV420 2P 10bit(8+2)",
		.pixelformat	= V4L2_PIX_FMT_NV12M_S10B,
		.num_planes	= 2 + NUM_OF_META_PLANE,
		.bitsperpixel	= { 8, 8 },
		.hw_format	= DMA_INOUT_FORMAT_YUV420,
		.hw_order	= DMA_INOUT_ORDER_CbCr,
		.hw_bitwidth	= DMA_INOUT_BIT_WIDTH_10BIT,
		.hw_plane	= DMA_INOUT_PLANE_4,
	}, {
		.name		= "JPEG",
		.pixelformat	= V4L2_PIX_FMT_JPEG,
		.num_planes	= 1 + NUM_OF_META_PLANE,
		.mbus_code	= MEDIA_BUS_FMT_JPEG_1X8,
		.bitsperpixel	= { 8 },
		.hw_format	= 0,
		.hw_order	= 0,
		.hw_bitwidth	= 0,
		.hw_plane	= 0,
	}, {
		.name		= "DEPTH",
		.pixelformat	= V4L2_PIX_FMT_Z16,
		.num_planes	= 1 + NUM_OF_META_PLANE,
		.bitsperpixel	= { 32 },
		.hw_format	= 0,
		.hw_order	= DMA_INOUT_ORDER_NO,
		.hw_bitwidth	= 0,
		.hw_plane	= 0,
	}

};

struct is_fmt *is_find_format(u32 pixelformat,
	u32 flags)
{
	ulong i;
	struct is_fmt *result, *fmt;
	u8 pixel_size;
	u32 memory_bitwidth = DMA_INOUT_BIT_WIDTH_8BIT;

	result = NULL;

	if (!pixelformat) {
		err("pixelformat is null");
		goto p_err;
	}

	pixel_size = flags & PIXEL_TYPE_SIZE_MASK;
	if (pixel_size == CAMERA_PIXEL_SIZE_10BIT)
		memory_bitwidth = DMA_INOUT_BIT_WIDTH_16BIT;
	else if (pixel_size == CAMERA_PIXEL_SIZE_PACKED_10BIT)
		memory_bitwidth = DMA_INOUT_BIT_WIDTH_10BIT;
	else if (pixel_size == CAMERA_PIXEL_SIZE_8_2BIT)
		memory_bitwidth = DMA_INOUT_BIT_WIDTH_10BIT;

	for (i = 0; i < ARRAY_SIZE(is_formats); ++i) {
		fmt = &is_formats[i];
		if (fmt->pixelformat == pixelformat) {
			if (pixelformat == V4L2_PIX_FMT_NV16M_P210
				|| pixelformat == V4L2_PIX_FMT_NV12M_P010
				||  pixelformat == V4L2_PIX_FMT_YUYV) {
				if (fmt->hw_bitwidth != memory_bitwidth)
					continue;
			}

			result = fmt;
			break;
		}
	}

p_err:
	return result;
}

static void is_set_plane_size(struct is_frame_cfg *frame,
	unsigned int sizes[],
	unsigned int num_planes)
{
	u32 plane;
	u32 width[IS_MAX_PLANES];
	u32 image_planes = num_planes - NUM_OF_META_PLANE;

	FIMC_BUG_VOID(!frame);
	FIMC_BUG_VOID(!frame->format);

	for (plane = 0; plane < IS_MAX_PLANES; ++plane)
		width[plane] = max(frame->width * frame->format->bitsperpixel[plane] / BITS_PER_BYTE,
					frame->bytesperline[plane]);

	switch (frame->format->pixelformat) {
	case V4L2_PIX_FMT_YUV444:
		dbg("V4L2_PIX_FMT_YUV444(w:%d)(h:%d)\n", frame->width, frame->height);
		for (plane = 0; plane < image_planes; plane++)
			sizes[plane] = width[0] * frame->height;
		sizes[plane] = SIZE_OF_META_PLANE;
		break;
	case V4L2_PIX_FMT_YUYV:
	case V4L2_PIX_FMT_UYVY:
		dbg("V4L2_PIX_FMT_YUYV(w:%d)(h:%d)\n", frame->width, frame->height);
		for (plane = 0; plane < image_planes; plane++)
			sizes[plane] = width[0] * frame->height;
		sizes[plane] = SIZE_OF_META_PLANE;
		break;
	case V4L2_PIX_FMT_NV16:
	case V4L2_PIX_FMT_NV61:
		dbg("V4L2_PIX_FMT_NV16(w:%d)(h:%d)\n", frame->width, frame->height);
		for (plane = 0; plane < image_planes; plane++)
			sizes[plane] = width[0] * frame->height
				+ width[1] * frame->height;
		sizes[plane] = SIZE_OF_META_PLANE;
		break;
	case V4L2_PIX_FMT_NV16M:
	case V4L2_PIX_FMT_NV61M:
		dbg("V4L2_PIX_FMT_NV16(w:%d)(h:%d)\n", frame->width, frame->height);
		for (plane = 0; plane < image_planes; plane += 2) {
			sizes[plane] = width[0] * frame->height;
			sizes[plane + 1] = width[1] * frame->height;
		}
		sizes[plane] = SIZE_OF_META_PLANE;
		break;
	case V4L2_PIX_FMT_YUV422P:
		dbg("V4L2_PIX_FMT_YUV422P(w:%d)(h:%d)\n", frame->width, frame->height);
		for (plane = 0; plane < image_planes; plane++) {
			sizes[plane] = width[0] * frame->height
				+ width[1] * frame->height / 2
				+ width[2] * frame->height / 2;
		}
		sizes[plane] = SIZE_OF_META_PLANE;
		break;
	case V4L2_PIX_FMT_NV12:
	case V4L2_PIX_FMT_NV21:
		dbg("V4L2_PIX_FMT_NV12(w:%d)(h:%d)\n", frame->width, frame->height);
		for (plane = 0; plane < image_planes; plane++) {
			sizes[plane] = width[0] * frame->height
				+ width[1] * frame->height / 2;
		}
		sizes[plane] = SIZE_OF_META_PLANE;
		break;
	case V4L2_PIX_FMT_NV12M:
	case V4L2_PIX_FMT_NV21M:
		dbg("V4L2_PIX_FMT_NV12M(w:%d)(h:%d)\n", frame->width, frame->height);
		for (plane = 0; plane < image_planes; plane += 2) {
			sizes[plane] = width[0] * frame->height;
			sizes[plane + 1] = width[1] * frame->height / 2;
		}
		sizes[plane] = SIZE_OF_META_PLANE;
		break;
	case V4L2_PIX_FMT_YUV420:
	case V4L2_PIX_FMT_YVU420:
		dbg("V4L2_PIX_FMT_YVU420(w:%d)(h:%d)\n", frame->width, frame->height);
		for (plane = 0; plane < image_planes; plane++) {
			sizes[plane] = width[0] * frame->height
				+ width[1] * frame->height / 2
				+ width[2] * frame->height / 2;
		}
		sizes[plane] = SIZE_OF_META_PLANE;
		break;
	case V4L2_PIX_FMT_YUV420M:
	case V4L2_PIX_FMT_YVU420M:
		dbg("V4L2_PIX_FMT_YVU420M(w:%d)(h:%d)\n", frame->width, frame->height);
		for (plane = 0; plane < image_planes; plane += 3) {
			sizes[plane] = width[0] * frame->height;
			sizes[plane + 1] = width[1] * frame->height / 2;
			sizes[plane + 2] = width[2] * frame->height / 2;
		}
		sizes[plane] = SIZE_OF_META_PLANE;
		break;
	case V4L2_PIX_FMT_SGRBG8:
		dbg("V4L2_PIX_FMT_SGRBG8(w:%d)(h:%d)\n", frame->width, frame->height);
		for (plane = 0; plane < image_planes; plane++)
			sizes[plane] = frame->width * frame->height;
		sizes[plane] = SIZE_OF_META_PLANE;
		break;
	case V4L2_PIX_FMT_SBGGR8:
		dbg("V4L2_PIX_FMT_SBGGR8(w:%d)(h:%d)\n", frame->width, frame->height);
		for (plane = 0; plane < image_planes; plane++)
			sizes[plane] = frame->width * frame->height;
		sizes[plane] = SIZE_OF_META_PLANE;
		break;
	case V4L2_PIX_FMT_SBGGR10:
		dbg("V4L2_PIX_FMT_SBGGR10(w:%d)(h:%d)\n", frame->width, frame->height);
		for (plane = 0; plane < image_planes; plane++) {
			sizes[plane] = ALIGN((frame->width * 5) >> 2, 16) * frame->height;
			if (frame->bytesperline[0]) {
				if (frame->bytesperline[0] >= ALIGN((frame->width * 5) >> 2, 16)) {
				sizes[plane] = frame->bytesperline[0]
				    * frame->height;
				} else {
					err("Bytesperline too small\
						(fmt(V4L2_PIX_FMT_SBGGR10), W(%d), Bytes(%d))",
					frame->width,
					frame->bytesperline[0]);
				}
			}
		}
		sizes[plane] = SIZE_OF_META_PLANE;
		break;
	case V4L2_PIX_FMT_SBGGR12:
		dbg("V4L2_PIX_FMT_SBGGR12(w:%d)(h:%d)\n", frame->width, frame->height);
		for (plane = 0; plane < image_planes; plane++) {
			sizes[plane] = ALIGN((frame->width * 3) >> 1, 16) * frame->height;
			if (frame->bytesperline[0]) {
				if (frame->bytesperline[0] >= ALIGN((frame->width * 3) >> 1, 16)) {
					sizes[plane] = frame->bytesperline[0] * frame->height;
				} else {
					err("Bytesperline too small\
						(fmt(V4L2_PIX_FMT_SBGGR12), W(%d), Bytes(%d))",
					frame->width,
					frame->bytesperline[0]);
				}
			}
		}
		sizes[plane] = SIZE_OF_META_PLANE;
		break;
	case V4L2_PIX_FMT_SBGGR16:
		dbg("V4L2_PIX_FMT_SBGGR16(w:%d)(h:%d)\n", frame->width, frame->height);
		for (plane = 0; plane < image_planes; plane++) {
			sizes[plane] = frame->width * frame->height * 2;
			if (frame->bytesperline[0]) {
				if (frame->bytesperline[0] >= frame->width * 2) {
					sizes[plane] = frame->bytesperline[0] * frame->height;
				} else {
					err("Bytesperline too small\
						(fmt(V4L2_PIX_FMT_SBGGR16), W(%d), Bytes(%d))",
					frame->width,
					frame->bytesperline[0]);
				}
			}
		}
		sizes[plane] = SIZE_OF_META_PLANE;
		break;
	case V4L2_PIX_FMT_SBGGR10P: /* width * 10(bit) / 8(bit) */
		dbg("V4L2_PIX_FMT_SBGGR10P(w:%d)(h:%d)\n", frame->width, frame->height);
		for (plane = 0; plane < image_planes; plane++) {
			sizes[plane] = ALIGN((frame->width * 5) >> 2, 16) * frame->height;
			if (frame->bytesperline[0]) {
				if (frame->bytesperline[0] >= ALIGN((frame->width * 5) >> 2, 16)) {
				sizes[plane] = frame->bytesperline[0]
				    * frame->height;
				} else {
					err("Bytesperline too small"
						"(fmt(V4L2_PIX_FMT_SBGGR10P), W(%d), Bytes(%d))",
					frame->width,
					frame->bytesperline[0]);
				}
			}
		}
		sizes[plane] = SIZE_OF_META_PLANE;
		break;
	case V4L2_PIX_FMT_SBGGR12P: /* width * 12(bit) / 8(bit) */
		dbg("V4L2_PIX_FMT_SBGGR12P(w:%d)(h:%d)\n", frame->width, frame->height);
		for (plane = 0; plane < image_planes; plane++) {
			sizes[plane] = ALIGN((frame->width * 3) >> 1, 16) * frame->height;
			if (frame->bytesperline[0]) {
				if (frame->bytesperline[0] >= ALIGN((frame->width * 3) >> 1, 16)) {
					sizes[plane] = frame->bytesperline[0] * frame->height;
				} else {
					err("Bytesperline too small"
						"(fmt(V4L2_PIX_FMT_SBGGR12P), W(%d), Bytes(%d))",
					frame->width,
					frame->bytesperline[0]);
				}
			}
		}
		sizes[plane] = SIZE_OF_META_PLANE;
		break;

	case V4L2_PIX_FMT_RGB32:
		dbg("V4L2_PIX_FMT_RGB32(w:%d)(h:%d)\n", frame->width, frame->height);
		for (plane = 0; plane < image_planes; plane++)
			sizes[plane] = frame->width * frame->height * 4;
		sizes[plane] = SIZE_OF_META_PLANE;
		break;
	case V4L2_PIX_FMT_RGB24:
		dbg("V4L2_PIX_FMT_RGB24(w:%d)(h:%d)\n", frame->width, frame->height);
		for (plane = 0; plane < image_planes; plane++)
			sizes[plane] = frame->width * frame->height * 3;
		sizes[plane] = SIZE_OF_META_PLANE;
		break;
	case V4L2_PIX_FMT_BGR24:
		dbg("V4L2_PIX_FMT_BGR24(w:%d)(h:%d)\n", frame->width, frame->height);
		for (plane = 0; plane < image_planes; plane++)
			sizes[plane] = frame->width * frame->height * 3;
		sizes[plane] = SIZE_OF_META_PLANE;
		break;
	case V4L2_PIX_FMT_GREY:
		dbg("V4L2_PIX_FMT_GREY(w:%d)(h:%d)\n", frame->width, frame->height);
		for (plane = 0; plane < image_planes; plane++)
			sizes[plane] = frame->width * frame->height;
		sizes[plane] = SIZE_OF_META_PLANE;
		break;
	case V4L2_PIX_FMT_Y10:
		dbg("V4L2_PIX_FMT_Y10(w:%d)(h:%d)\n", frame->width, frame->height);
		for (plane = 0; plane < image_planes; plane++)
			sizes[plane] = width[0] * frame->height;
		sizes[plane] = SIZE_OF_META_PLANE;
		break;
	case V4L2_PIX_FMT_Y12:
		dbg("V4L2_PIX_FMT_Y12(w:%d)(h:%d)\n", frame->width, frame->height);
		for (plane = 0; plane < image_planes; plane++)
			sizes[plane] = width[0] * frame->height;
		sizes[plane] = SIZE_OF_META_PLANE;
		break;
	case V4L2_PIX_FMT_Y10BPACK:
		dbg("V4L2_PIX_FMT_Y10BPACK(w:%d)(h:%d)\n", frame->width, frame->height);
		for (plane = 0; plane < image_planes; plane++)
			sizes[plane] = ALIGN(width[0], 16) * frame->height;
		sizes[plane] = SIZE_OF_META_PLANE;
		break;
	case V4L2_PIX_FMT_YUV32:
		dbg("V4L2_PIX_FMT_YUV32(w:%d)(h:%d)\n", frame->width, frame->height);
		for (plane = 0; plane < image_planes; plane++) {
			sizes[plane] = width[plane] * frame->height;
		}
		sizes[plane] = SIZE_OF_META_PLANE;
		break;
	case V4L2_PIX_FMT_NV16M_P210:
		dbg("V4L2_PIX_FMT_NV16M_P210(w:%d)(h:%d)\n", frame->width, frame->height);
		for (plane = 0; plane < image_planes; plane += 2) {
			sizes[plane] =  ALIGN(width[0], 16) * frame->height;
			sizes[plane + 1] = ALIGN(width[1], 16) * frame->height;
		}
		sizes[plane] = SIZE_OF_META_PLANE;
		break;
	case V4L2_PIX_FMT_NV12M_P010:
		dbg("V4L2_PIX_FMT_NV12M_P010(w:%d)(h:%d)\n", frame->width, frame->height);
		for (plane = 0; plane < image_planes; plane += 2) {
			sizes[plane] = ALIGN(width[0], 16) * frame->height;
			sizes[plane + 1] = ALIGN(width[0], 16) * frame->height / 2;
		}
		sizes[plane] = SIZE_OF_META_PLANE;
		break;
	case V4L2_PIX_FMT_NV16M_S10B:
		dbg("V4L2_PIX_FMT_NV16M_S10B(w:%d)(h:%d)\n", frame->width, frame->height);
		for (plane = 0; plane < image_planes; plane += 2) {
			sizes[plane] = NV16M_Y_SIZE(frame->width, frame->height)
				+ NV16M_Y_2B_SIZE(frame->width, frame->height);
			sizes[plane + 1] = NV16M_CBCR_SIZE(frame->width, frame->height)
				+ NV16M_CBCR_2B_SIZE(frame->width, frame->height);
		}
		sizes[plane] = SIZE_OF_META_PLANE;
		break;
	case V4L2_PIX_FMT_NV12M_S10B:
		dbg("V4L2_PIX_FMT_NV12M_S10B(w:%d)(h:%d)\n", frame->width, frame->height);
		for (plane = 0; plane < image_planes; plane += 2) {
			sizes[plane] = NV12M_Y_SIZE(frame->width, frame->height)
				+ NV12M_Y_2B_SIZE(frame->width, frame->height);
			sizes[plane + 1] = NV12M_CBCR_SIZE(frame->width, frame->height)
				+ NV12M_CBCR_2B_SIZE(frame->width, frame->height);
		}
		sizes[plane] = SIZE_OF_META_PLANE;
		break;
	case V4L2_PIX_FMT_JPEG:
		dbg("V4L2_PIX_FMT_JPEG(w:%d)(h:%d)\n", frame->width, frame->height);
		for (plane = 0; plane < image_planes; plane++)
			sizes[plane] = width[0] * frame->height;
		sizes[plane] = SIZE_OF_META_PLANE;
		break;
	case V4L2_PIX_FMT_Z16:
		dbg("V4L2_PIX_FMT_Z16(w:%d)(h:%d)\n", frame->width, frame->height);
		for (plane = 0; plane < image_planes; plane++)
			sizes[plane] = width[0] * frame->height;
		sizes[plane] = SIZE_OF_META_PLANE;
		break;
	default:
		err("unknown pixelformat(%c%c%c%c)\n", (char)((frame->format->pixelformat >> 0) & 0xFF),
			(char)((frame->format->pixelformat >> 8) & 0xFF),
			(char)((frame->format->pixelformat >> 16) & 0xFF),
			(char)((frame->format->pixelformat >> 24) & 0xFF));
		break;
	}
}

static inline void vref_init(struct is_video *video)
{
	atomic_set(&video->refcount, 0);
}

static inline int vref_get(struct is_video *video)
{
	return atomic_inc_return(&video->refcount) - 1;
}

static inline int vref_put(struct is_video *video,
	void (*release)(struct is_video *video))
{
	int ret = 0;

	ret = atomic_sub_and_test(1, &video->refcount);
	if (ret)
		pr_debug("closed all instacne");

	return atomic_read(&video->refcount);
}

static int queue_init(void *priv, struct vb2_queue *vbq,
	struct vb2_queue *vbq_dst)
{
	int ret = 0;
	struct is_video_ctx *vctx = priv;
	struct is_video *video;
	u32 type;

	FIMC_BUG(!vctx);
	FIMC_BUG(!GET_VIDEO(vctx));
	FIMC_BUG(!vbq);

	video = GET_VIDEO(vctx);

	if (video->type == IS_VIDEO_TYPE_CAPTURE)
		type = V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE;
	else
		type = V4L2_BUF_TYPE_VIDEO_OUTPUT_MPLANE;

	vbq->type		= type;
	vbq->io_modes		= VB2_MMAP | VB2_USERPTR | VB2_DMABUF;
	vbq->drv_priv		= vctx;
	vbq->buf_struct_size	= sizeof(struct is_vb2_buf);
	vbq->ops		= vctx->vb2_ops;
	vbq->mem_ops		= vctx->vb2_mem_ops;
	vbq->timestamp_flags	= V4L2_BUF_FLAG_TIMESTAMP_COPY;

	ret = vb2_queue_init(vbq);
	if (ret) {
		mverr("vb2_queue_init fail(%d)", vctx, video, ret);
		goto p_err;
	}

	vctx->queue.vbq = vbq;

p_err:
	return ret;
}

int open_vctx(struct file *file,
	struct is_video *video,
	struct is_video_ctx **vctx,
	u32 instance,
	ulong id,
	const char *name)
{
	int ret = 0;

	FIMC_BUG(!file);
	FIMC_BUG(!video);

	if (atomic_read(&video->refcount) > IS_MAX_NODES) {
		err("[V%02d] can't open vctx, refcount is invalid", video->id);
		ret = -EINVAL;
		goto p_err;
	}

	*vctx = vzalloc(sizeof(struct is_video_ctx));
	if (*vctx == NULL) {
		err("[V%02d] vzalloc is fail", video->id);
		ret = -ENOMEM;
		goto p_err;
	}

	(*vctx)->refcount = vref_get(video);
	(*vctx)->instance = instance;
	(*vctx)->queue.id = id;
	snprintf((*vctx)->queue.name, sizeof((*vctx)->queue.name), "%s", name);
	(*vctx)->state = BIT(IS_VIDEO_CLOSE);

	file->private_data = *vctx;

p_err:
	return ret;
}

int close_vctx(struct file *file,
	struct is_video *video,
	struct is_video_ctx *vctx)
{
	int ret = 0;

	vfree(vctx);
	file->private_data = NULL;
	ret = vref_put(video, NULL);

	return ret;
}

/*
 * =============================================================================
 * Queue Opertation
 * =============================================================================
 */

static int is_queue_open(struct is_queue *queue,
	u32 rdycount)
{
	int ret = 0;

	queue->buf_maxcount = 0;
	queue->buf_refcount = 0;
	queue->buf_rdycount = rdycount;
	queue->buf_req = 0;
	queue->buf_pre = 0;
	queue->buf_que = 0;
	queue->buf_com = 0;
	queue->buf_dqe = 0;
	clear_bit(IS_QUEUE_BUFFER_PREPARED, &queue->state);
	clear_bit(IS_QUEUE_BUFFER_READY, &queue->state);
	clear_bit(IS_QUEUE_STREAM_ON, &queue->state);
	clear_bit(IS_QUEUE_NEED_TO_REMAP, &queue->state);
	clear_bit(IS_QUEUE_NEED_TO_KMAP, &queue->state);
	clear_bit(IS_QUEUE_NEED_TO_EXTMAP, &queue->state);
	memset(&queue->framecfg, 0, sizeof(struct is_frame_cfg));
	frame_manager_probe(&queue->framemgr, queue->id, queue->name);

	return ret;
}

static int is_queue_close(struct is_queue *queue)
{
	int ret = 0;

	queue->buf_maxcount = 0;
	queue->buf_refcount = 0;
	clear_bit(IS_QUEUE_BUFFER_PREPARED, &queue->state);
	clear_bit(IS_QUEUE_BUFFER_READY, &queue->state);
	clear_bit(IS_QUEUE_STREAM_ON, &queue->state);
	clear_bit(IS_QUEUE_NEED_TO_REMAP, &queue->state);
	clear_bit(IS_QUEUE_NEED_TO_KMAP, &queue->state);
	clear_bit(IS_QUEUE_NEED_TO_EXTMAP, &queue->state);
	frame_manager_close(&queue->framemgr);

	return ret;
}

static int is_queue_set_format_mplane(struct is_video_ctx *vctx,
	struct is_queue *queue,
	void *device,
	struct v4l2_format *format)
{
	int ret = 0;
	u32 plane;
	struct v4l2_pix_format_mplane *pix;
	struct is_fmt *fmt;
	struct is_video *video;

	FIMC_BUG(!queue);

	video = GET_VIDEO(vctx);

	pix = &format->fmt.pix_mp;
	fmt = is_find_format(pix->pixelformat, pix->flags);
	if (!fmt) {
		mverr("[%s] pixel format is not found", vctx, video, queue->name);
		ret = -EINVAL;
		goto p_err;
	}

	queue->framecfg.format		= fmt;
	queue->framecfg.colorspace	= pix->colorspace;
	queue->framecfg.quantization	= pix->quantization;
	queue->framecfg.width		= pix->width;
	queue->framecfg.height		= pix->height;
	queue->framecfg.hw_pixeltype	= pix->flags;

	for (plane = 0; plane < fmt->hw_plane; ++plane) {
		if (pix->plane_fmt[plane].bytesperline) {
			queue->framecfg.bytesperline[plane] =
				pix->plane_fmt[plane].bytesperline;
		} else {
			queue->framecfg.bytesperline[plane] = 0;
		}
	}

	ret = CALL_QOPS(queue, s_format, device, queue);
	if (ret) {
		mverr("[%s] s_format is fail(%d)", vctx, video, queue->name, ret);
		goto p_err;
	}

	mvinfo("[%s]pixelformat(%c%c%c%c) bit(%d) size(%dx%d) flag(0x%x)\n",
		vctx, video, queue->name,
		(char)((fmt->pixelformat >> 0) & 0xFF),
		(char)((fmt->pixelformat >> 8) & 0xFF),
		(char)((fmt->pixelformat >> 16) & 0xFF),
		(char)((fmt->pixelformat >> 24) & 0xFF),
		queue->framecfg.format->hw_bitwidth,
		queue->framecfg.width,
		queue->framecfg.height,
		queue->framecfg.hw_pixeltype);
p_err:
	return ret;
}

int is_queue_setup(struct is_queue *queue,
	void *alloc_ctx,
	unsigned int *num_planes,
	unsigned int sizes[],
	struct device *alloc_devs[])
{
	u32 ret = 0;
	u32 plane;
	struct is_ion_ctx *ctx = alloc_ctx;

	FIMC_BUG(!queue);
	FIMC_BUG(!ctx);
	FIMC_BUG(!num_planes);
	FIMC_BUG(!sizes);
	FIMC_BUG(!queue->framecfg.format);

	*num_planes = (unsigned int)queue->framecfg.format->num_planes;

	is_set_plane_size(&queue->framecfg, sizes, *num_planes);

	if(test_bit(IS_QUEUE_NEED_TO_EXTMAP, &queue->state)) {
		*num_planes += 1; /* TODO: use ext_plane to multi */
		sizes[*num_planes] = 1024 * 1024;	/* 1MB */
	}

	for (plane = 0; plane < *num_planes; plane++) {
		alloc_devs[plane] = ctx->dev;
		queue->framecfg.size[plane] = sizes[plane];
		mdbgv_vid("queue[%d] size : %d\n", plane, sizes[plane]);
	}

	return ret;
}

static void is_queue_subbuf_draw_digit(struct is_queue *queue,
		struct is_frame *frame)
{
	struct is_debug_dma_info dinfo;
	struct is_sub_node *snode = &frame->cap_node;
	struct is_sub_dma_buf *sdbuf;
	struct camera2_node *node;
	struct is_fmt *fmt;
	u32 n, b;

	/* Only handle 1st plane */
	for (n = 0; n < CAPTURE_NODE_MAX; n++) {
		node = &frame->shot_ext->node_group.capture[n];
		if (!node->request)
			continue;

		sdbuf = &queue->out_buf[frame->index].sub[n];
		fmt = is_find_format(node->pixelformat, node->flags);
		if (!fmt) {
			warn("[%s][I%d][F%d] pixelformat(%c%c%c%c) is not found",
				queue->name,
				frame->index,
				frame->fcount,
				(char)((node->pixelformat >> 0) & 0xFF),
				(char)((node->pixelformat >> 8) & 0xFF),
				(char)((node->pixelformat >> 16) & 0xFF),
				(char)((node->pixelformat >> 24) & 0xFF));

			continue;
		}

		dinfo.width = node->output.cropRegion[2];
		dinfo.height = node->output.cropRegion[3];
		dinfo.pixeltype = node->flags;
		dinfo.bpp = fmt->bitsperpixel[0];
		dinfo.pixelformat = fmt->pixelformat;

		for (b = 0; b < sdbuf->num_buffer; b++) {
			dinfo.addr = snode->sframe[n].kva[b * sdbuf->num_plane];

			if (dinfo.addr)
				is_dbg_draw_digit(&dinfo, frame->fcount);
		}
	}
}

static void is_queue_draw_digit(struct is_queue *queue, struct is_vb2_buf *vbuf)
{
	struct is_debug_dma_info dinfo;
	struct is_video_ctx *vctx = container_of(queue, struct is_video_ctx, queue);
	struct is_video *video = GET_VIDEO(vctx);
	struct is_frame_cfg *framecfg = &queue->framecfg;
	struct is_frame *frame;
	u32 index = vbuf->vb.vb2_buf.index;
	u32 num_buffer = vbuf->num_merged_dbufs ? vbuf->num_merged_dbufs : 1;
	u32 num_i_planes = vbuf->vb.vb2_buf.num_planes - NUM_OF_META_PLANE;
	u32 num_ext_planes = 0;
	u32 b;

	if (test_bit(IS_QUEUE_NEED_TO_EXTMAP, &vctx->queue.state))
		num_ext_planes = num_i_planes - vctx->queue.framecfg.format->hw_plane;

	num_i_planes -= num_ext_planes;
	frame = &queue->framemgr.frames[index];

	/* Draw digit on capture node buffer */
	if (video->type == IS_VIDEO_TYPE_CAPTURE) {
		dinfo.width = frame->width ? frame->width : framecfg->width;
		dinfo.height = frame->height ? frame->height : framecfg->height;
		dinfo.pixeltype = framecfg->hw_pixeltype;
		dinfo.bpp = framecfg->format->bitsperpixel[0];
		dinfo.pixelformat = framecfg->format->pixelformat;

		for (b = 0; b < num_buffer; b++) {
			dinfo.addr = queue->buf_kva[index][b * num_i_planes];

			if (dinfo.addr)
				is_dbg_draw_digit(&dinfo, frame->fcount);
		}
	}

#ifdef ENABLE_LOGICAL_VIDEO_NODE
	/* Draw digit on LVN sub node buffers */
	if (video->type == IS_VIDEO_TYPE_LEADER &&
		queue->mode == CAMERA_NODE_LOGICAL)
		is_queue_subbuf_draw_digit(queue, frame);
#endif
}

#ifdef ENABLE_LOGICAL_VIDEO_NODE
static int _is_queue_subbuf_prepare(struct device *dev,
		struct is_vb2_buf *vbuf,
		struct camera2_node_group *node_group,
		bool need_vmap)
{
	int ret;
	struct vb2_buffer *vb = &vbuf->vb.vb2_buf;
	struct is_video_ctx *vctx = vb->vb2_queue->drv_priv;
	struct is_video *video = GET_VIDEO(vctx);
	struct is_queue *queue = GET_QUEUE(vctx);
	struct is_device_ischain *device = GET_DEVICE_ISCHAIN(vctx);
	struct camera2_node *node;
	struct is_sub_buf *sbuf;
	struct v4l2_plane planes[IS_MAX_PLANES];
	struct is_sub_dma_buf *sdbuf;
	u32 index = vb->index;
	u32 num_i_planes, n;

	mdbgv_lvn(3, "[%s][I%d]subbuf_prepare: queue 0x%lx\n", vctx, video,
			queue->name, index, queue);

	/* Extract input subbuf */
	node = &node_group->leader;
	if (!node->request)
		return 0;
	if (node->vid >= IS_VIDEO_MAX_NUM) {
		mverr("[%s][I%d]subbuf_prepare: invalid input (req:%d, vid:%d, length:%d)\n",
			vctx, video,
			queue->name, index,
			node->request, node->vid, node->buf.length);
	}
	/* Disable SBWC for drawing digit on it */
	if (dbg_draw_digit_ctrl)
		node->flags &= ~PIXEL_TYPE_EXTRA_MASK;

#ifdef ENABLE_LVN_DUMMYOUTPUT
	num_i_planes = node->buf.length - NUM_OF_META_PLANE;

	sbuf = &queue->in_buf[index];
	sbuf->ldr_vid = video->id;

	sdbuf = &sbuf->sub[0];
	sdbuf->vid = node->vid;
	sdbuf->num_plane = num_i_planes;

	if (copy_from_user(planes, node->buf.m.planes,
		sizeof(struct v4l2_plane) * num_i_planes) != 0) {
		mverr("[%s][%s][I%d] Failed copy_from_user", vctx, video,
				queue->name,
				vn_name[node->vid],
				index);
		return -EFAULT;
	}

	vbuf->ops->subbuf_prepare(sdbuf, planes, dev);

	ret = vbuf->ops->subbuf_dvmap(sdbuf);
	if (ret) {
		mverr("[%s][%s][I%d]Failed to get dva", vctx, video,
			queue->name,
			vn_name[node->vid],
			index);

		return ret;
	}
#endif

	/* Extract output subbuf */
	for (n = 0; n < CAPTURE_NODE_MAX; n++) {
		node = &node_group->capture[n];
		if (!node->request)
			continue;

		if (node->buf.length > IS_MAX_PLANES ||
			node->vid >= IS_VIDEO_MAX_NUM) {
			mverr("[%s][I%d]subbuf_prepare: invalid output[%d] (req:%d, vid:%d, length:%d) input (req:%d, vid:%d)\n",
				vctx, video,
				queue->name, index,
				n,
				node->request, node->vid, node->buf.length,
				node_group->leader.request, node_group->leader.vid);
			/*
			 * FIXME :
			 * For normal error handling, it would be nice to return error to user
			 * but condition of system need to be preserved because of some issues
			 * So, temporarily BUG() is used here.
			 */
			BUG();
		}
		num_i_planes = node->buf.length - NUM_OF_META_PLANE;

		sbuf = &queue->out_buf[index];
		sbuf->ldr_vid = video->id;

		sdbuf = &sbuf->sub[n];
		sdbuf->vid = node->vid;
		sdbuf->num_plane = num_i_planes;

		if (copy_from_user(planes, node->buf.m.planes,
				sizeof(struct v4l2_plane) * num_i_planes) != 0) {
			mverr("[%s][%s][I%d] Failed copy_from_user", vctx, video,
					queue->name,
					vn_name[node->vid],
					index);
			continue;
		}

		vbuf->ops->subbuf_prepare(sdbuf, planes, dev);

		ret = vbuf->ops->subbuf_dvmap(sdbuf);
		if (ret) {
			mverr("[%s][%s][I%d]Failed to get dva", vctx, video,
					queue->name,
					vn_name[node->vid],
					index);

			return ret;
		}

		if (need_vmap || CHECK_NEED_KVADDR_ID(sdbuf->vid)) {
			ret = vbuf->ops->subbuf_kvmap(sdbuf);
			if (ret) {
				mverr("[%s][%s][I%d]Failed to get dva", vctx, video,
						queue->name,
						vn_name[node->vid],
						index);

				return ret;
			}

			/* need cache maintenance */
			is_q_dbuf_q(device->dbuf_q, sdbuf, vb->vb2_queue->queued_count);

			/* Disable SBWC for drawing digit on it */
			if (dbg_draw_digit_ctrl)
				node->flags &= ~PIXEL_TYPE_EXTRA_MASK;
		}
	}

	return 0;
}

int _is_queue_buffer_tag(dma_addr_t saddr[], dma_addr_t taddr[],
	u32 pixelformat, u32 width, u32 height, u32 planes, u32 *hw_planes)
{
	int i, j;
	int ret = 0;

	*hw_planes = planes;
	switch (pixelformat) {
	case V4L2_PIX_FMT_NV21:
	case V4L2_PIX_FMT_NV12:
	case V4L2_PIX_FMT_NV16:
	case V4L2_PIX_FMT_NV61:
		for (i = 0; i < planes; i++) {
			j = i * 2;
			taddr[j] = saddr[i];
			taddr[j + 1] = taddr[j] + (width * height);
		}
		*hw_planes = planes * 2;
		break;
	case V4L2_PIX_FMT_YVU420M:
		for (i = 0; i < planes; i += 3) {
			taddr[i] = saddr[i];
			taddr[i + 1] = saddr[i + 2];
			taddr[i + 2] = saddr[i + 1];
		}
		break;
	case V4L2_PIX_FMT_YUV420:
		for (i = 0; i < planes; i++) {
			j = i * 3;
			taddr[j] = saddr[i];
			taddr[j + 1] = taddr[j] + (width * height);
			taddr[j + 2] = taddr[j + 1] + (width * height / 4);
		}
		*hw_planes = planes * 3;
		break;
	case V4L2_PIX_FMT_YVU420: /* AYV12 spec: The width should be aligned by 16 pixel. */
		for (i = 0; i < planes; i++) {
			j = i * 3;
			taddr[j] = saddr[i];
			taddr[j + 2] = taddr[j] + (ALIGN(width, 16) * height);
			taddr[j + 1] = taddr[j + 2] + (ALIGN(width / 2, 16) * height / 2);
		}
		*hw_planes = planes * 3;
		break;
	case V4L2_PIX_FMT_YUV422P:
		for (i = 0; i < planes; i++) {
			j = i * 3;
			taddr[j] = saddr[i];
			taddr[j + 1] = taddr[j] + (width * height);
			taddr[j + 2] = taddr[j + 1] + (width * height / 2);
		}
		*hw_planes = planes * 3;
		break;
	case V4L2_PIX_FMT_NV12M_S10B:
	case V4L2_PIX_FMT_NV21M_S10B:
		for (i = 0; i < planes; i += 2) {
			j = i * 2;
			/* Y_ADDR, UV_ADDR, Y_2BIT_ADDR, UV_2BIT_ADDR */
			taddr[j] = saddr[i];
			taddr[j + 1] = saddr[i + 1];
			taddr[j + 2] = taddr[j] + NV12M_Y_SIZE(width, height);
			taddr[j + 3] = taddr[j + 1] + NV12M_CBCR_SIZE(width, height);
		}
		break;
	case V4L2_PIX_FMT_NV16M_S10B:
	case V4L2_PIX_FMT_NV61M_S10B:
		for (i = 0; i < planes; i += 2) {
			j = i * 2;
			/* Y_ADDR, UV_ADDR, Y_2BIT_ADDR, UV_2BIT_ADDR */
			taddr[j] = saddr[i];
			taddr[j + 1] = saddr[i + 1];
			taddr[j + 2] = taddr[j] + NV16M_Y_SIZE(width, height);
			taddr[j + 3] = taddr[j + 1] + NV16M_CBCR_SIZE(width, height);
		}
		break;
	case V4L2_PIX_FMT_RGB24:
		for (i = 0; i < planes; i++) {
			j = i * 3;
			taddr[j + 2] = saddr[i];
			taddr[j + 1] = taddr[j + 2] + (width * height);
			taddr[j] = taddr[j + 1] + (width * height);
		}
		*hw_planes = planes * 3;
		break;
	case V4L2_PIX_FMT_BGR24:
		for (i = 0; i < planes; i++) {
			j = i * 3;
			taddr[j] = saddr[i];
			taddr[j + 1] = taddr[j] + (width * height);
			taddr[j + 2] = taddr[j + 1] + (width * height);
		}
		*hw_planes = planes * 3;
		break;
	default:
		for (i = 0; i < planes; i++)
			taddr[i] = saddr[i];
		break;
	}

	return ret;
}

static int _is_queue_subbuf_queue(struct is_video_ctx *vctx,
		struct is_queue *queue,
		struct is_frame *frame)
{
	struct is_video *video = GET_VIDEO(vctx);
	struct camera2_node *node = NULL;
	struct is_sub_node * snode = NULL;
	struct is_sub_dma_buf *sdbuf;
	struct is_crop *crop;
	u32 index = frame->index;
	u32 n, stride_w, stride_h, hw_planes = 0;

#ifdef ENABLE_LVN_DUMMYOUTPUT
	/* Extract input subbuf information */
	node = &frame->shot_ext->node_group.leader;
	if (!node->request)
		return 0;

	snode = &frame->out_node;
	if (is_hw_get_output_slot(node->vid) < 0) {
		mverr("[%s][I%d]Invalid vid %d", vctx, video,
				queue->name,
				index,
				node->vid);
		return -EINVAL;
	}

	sdbuf = &queue->in_buf[index].sub[0];

	/* Setup input subframe */
	snode->sframe[0].id = sdbuf->vid;
	snode->sframe[0].num_planes = sdbuf->num_plane * sdbuf->num_buffer;
	crop = (struct is_crop *)node->input.cropRegion;
	stride_w = max(node->width, crop->w);
	stride_h = max(node->height, crop->h);

	_is_queue_buffer_tag(sdbuf->dva,
			snode->sframe[0].dva,
			node->pixelformat,
			stride_w, stride_h
			snode->sframe[0].num_planes,
			&hw_planes);

	snode->sframe[0].num_planes = hw_planes;
#endif

	/* Extract output subbuf information */
	snode = &frame->cap_node;
	for (n = 0; n < CAPTURE_NODE_MAX; n++) {
		/* clear first */
		snode->sframe[n].id = 0;

		if (snode->sframe[n].kva[0])
			memset(snode->sframe[n].kva, 0x0,
				sizeof(ulong) * snode->sframe[n].num_planes);

		node = &frame->shot_ext->node_group.capture[n];
		if (!node->request)
			continue;

		sdbuf = &queue->out_buf[index].sub[n];

		/* Setup output subframe */
		snode->sframe[n].id = sdbuf->vid;
		snode->sframe[n].num_planes
			= sdbuf->num_plane * sdbuf->num_buffer;
		crop = (struct is_crop *)node->output.cropRegion;
		stride_w = max(node->width, crop->w);
		stride_h = max(node->height, crop->h);

		_is_queue_buffer_tag(sdbuf->dva,
				snode->sframe[n].dva,
				node->pixelformat,
				stride_w, stride_h,
				snode->sframe[n].num_planes,
				&hw_planes);

		snode->sframe[n].num_planes = hw_planes;

		if (sdbuf->kva[0]) {
			mdbgv_lvn(2, "[%s][%s] 0x%lx\n", vctx, video,
				queue->name, vn_name[node->vid], sdbuf->kva[0]);

			memcpy(snode->sframe[n].kva, sdbuf->kva,
				sizeof(ulong) * snode->sframe[n].num_planes);
		}

		mdbgv_lvn(4, "[%s][%s][N%d][I%d] pixelformat %c%c%c%c size %dx%d(%dx%d) length %d\n",
			vctx, video,
			queue->name, vn_name[node->vid],
			n, index,
			(char)((node->pixelformat >> 0) & 0xFF),
			(char)((node->pixelformat >> 8) & 0xFF),
			(char)((node->pixelformat >> 16) & 0xFF),
			(char)((node->pixelformat >> 24) & 0xFF),
			crop->w, crop->h,
			stride_w, stride_h,
			node->buf.length);
	}

	return 0;
}

int __find_mapped_index(__s32 target_fd, struct is_sub_buf buf[], int nidx, int pidx)
{
	int ret = -1;
#ifndef ENABLE_SKIP_PER_FRAME_MAP
	int j;

	for (j = 0; j < IS_MAX_BUFS; j++) {
		if (target_fd == buf[j].sub[nidx].buf_fd[pidx]) {
			ret = j;
			break;
		}
	}

#endif
	return ret;
}
#endif

int is_queue_buffer_queue(struct is_queue *queue,
	struct vb2_buffer *vb)
{
	struct is_video_ctx *vctx = container_of(queue, struct is_video_ctx, queue);
	struct is_video *video = GET_VIDEO(vctx);
	struct is_framemgr *framemgr = &queue->framemgr;
	struct vb2_v4l2_buffer *vb2_v4l2_buf = to_vb2_v4l2_buffer(vb);
	struct is_vb2_buf *vbuf = vb_to_is_vb2_buf(vb2_v4l2_buf);
	unsigned int index = vb->index;
	unsigned int num_i_planes = vb->num_planes - NUM_OF_META_PLANE;
	unsigned int num_ext_planes = 0, pos_ext_p = 0;
	struct is_frame *frame;
	int i;
	int ret = 0;

	FIMC_BUG(!video);

	/* image planes */
	if (IS_ENABLED(CONFIG_DMA_BUF_CONTAINER) && vbuf->num_merged_dbufs) {
		/* vbuf has been sorted by the order of buffer */
		memcpy(queue->buf_dva[index], vbuf->dva,
			sizeof(dma_addr_t) * vbuf->num_merged_dbufs * num_i_planes);

		if (vbuf->kva[0])
			memcpy(queue->buf_kva[index], vbuf->kva,
				sizeof(ulong) * vbuf->num_merged_dbufs * num_i_planes);
		else
			memset(queue->buf_kva[index], 0x0,
				sizeof(ulong) * vbuf->num_merged_dbufs * num_i_planes);
	} else {
		/* if additional plane exist, map for use ext feature */
		if (test_bit(IS_QUEUE_NEED_TO_EXTMAP, &queue->state))
			num_ext_planes = num_i_planes - queue->framecfg.format->hw_plane;

		num_i_planes -= num_ext_planes;

		for (i = 0; i < num_i_planes; i++) {
			if (test_bit(IS_QUEUE_NEED_TO_REMAP, &queue->state))
				queue->buf_dva[index][i] = vbuf->dva[i];
			else
				queue->buf_dva[index][i] = vbuf->ops->plane_dvaddr(vbuf, i);
		}

		if (vbuf->kva[0])
			memcpy(queue->buf_kva[index], vbuf->kva,
				sizeof(ulong) * num_i_planes);
		else
			memset(queue->buf_kva[index], 0x0,
				sizeof(ulong) * num_i_planes);
	}

	frame = &framemgr->frames[index];
	frame->ext_planes = num_ext_planes;

	/* ext plane if exist */
	if(test_bit(IS_QUEUE_NEED_TO_EXTMAP, &queue->state)) {
		if (num_ext_planes)
			pos_ext_p = (num_i_planes * frame->num_buffers) + 1;
		for (i = pos_ext_p; i < pos_ext_p + num_ext_planes; i++) {
			queue->buf_kva[index][i]
					= vbuf->ops->plane_kmap(vbuf, i, 0);
			if (!queue->buf_kva[index][i]) {
				mverr("failed to get ext kva for %s", vctx, video, queue->name);
				ret = -ENOMEM;
				goto err_get_kva_for_ext;
			}
			queue->buf_dva[index][i]
					= vbuf->ops->plane_dvaddr(vbuf, i);
			if (!queue->buf_dva[index][i]) {
				mverr("failed to get ext dva for %s", vctx,
						video, queue->name);
				ret = -ENOMEM;
				goto err_get_dva_for_ext;
			}
		}
	}

	if (video->type == IS_VIDEO_TYPE_LEADER) {
#if defined(USE_OFFLINE_PROCESSING)
		for (i = 0; i < num_ext_planes; i++) {
			frame->shot->uctl.offlineDtDesc.base_kvaddr =
				queue->buf_kva[index][pos_ext_p + i];
			frame->shot->uctl.offlineDtDesc.base_dvaddr =
				queue->buf_dva[index][pos_ext_p + i];
		}
#else
		for (i = 0; i < num_ext_planes; i++)
			frame->kvaddr_ext[i] = queue->buf_kva[index][pos_ext_p + i];
#endif
#ifdef MEASURE_TIME
		frame->tzone = (struct timeval *)frame->shot_ext->timeZone;
#endif
#ifdef ENABLE_LOGICAL_VIDEO_NODE
		if (queue->mode == CAMERA_NODE_LOGICAL) {
			ret = _is_queue_subbuf_queue(vctx, queue, frame);
			if (ret) {
				mverr("[%s][I%d]Failed to subbuf_queue",
					vctx, video, queue->name, index);
				goto err_logical_node;
			}
		}
#endif
	}

	/* uninitialized frame need to get info */
	if (!test_bit(FRAME_MEM_INIT, &frame->mem_state))
		goto set_info;

	/* plane address check */
	for (i = 0; i < frame->planes; i++) {
		if (frame->dvaddr_buffer[i] != queue->buf_dva[index][i]) {
			if (video->resourcemgr->hal_version == IS_HAL_VER_3_2) {
				frame->dvaddr_buffer[i] = queue->buf_dva[index][i];
			} else {
				mverr("buffer[%d][%d] is changed(%pad != %pad)",
					vctx, video,
					index, i,
					&frame->dvaddr_buffer[i],
					&queue->buf_dva[index][i]);
				ret = -EINVAL;
				goto err_dva_changed;
			}
		}

		if (frame->kvaddr_buffer[i] != queue->buf_kva[index][i]) {
			if (video->resourcemgr->hal_version == IS_HAL_VER_3_2) {
				frame->kvaddr_buffer[i] = queue->buf_kva[index][i];
			} else {
				mverr("kvaddr buffer[%d][%d] is changed(0x%08lx != 0x%08lx)",
					vctx, video, index, i,
					frame->kvaddr_buffer[i], queue->buf_kva[index][i]);
				ret = -EINVAL;
				goto err_kva_changed;
			}
		}
	}

	return 0;

set_info:
	if (test_bit(IS_QUEUE_BUFFER_PREPARED, &queue->state)) {
		mverr("already prepared but new index(%d) is came", vctx, video, index);
		ret = -EINVAL;
		goto err_queue_prepared_already;
	}

	for (i = 0; i < frame->planes; i++) {
		frame->dvaddr_buffer[i] = queue->buf_dva[index][i];
		frame->kvaddr_buffer[i] = queue->buf_kva[index][i];
		frame->size[i] = queue->framecfg.size[i];
#ifdef PRINT_BUFADDR
		mvinfo("%s %d.%d %pad\n", vctx, video, framemgr->name, index,
					i, &frame->dvaddr_buffer[i]);
#endif
	}

	set_bit(FRAME_MEM_INIT, &frame->mem_state);

	queue->buf_refcount++;

	if (queue->buf_rdycount == queue->buf_refcount)
		set_bit(IS_QUEUE_BUFFER_READY, &queue->state);

	if (queue->buf_maxcount == queue->buf_refcount) {
		if (IS_ENABLED(CONFIG_DMA_BUF_CONTAINER)
				&& vbuf->num_merged_dbufs)
			mvinfo("%s number of merged buffers: %d\n",
				vctx, video, queue->name, frame->num_buffers);
		set_bit(IS_QUEUE_BUFFER_PREPARED, &queue->state);
	}

	queue->buf_que++;

#ifdef ENABLE_LOGICAL_VIDEO_NODE
err_logical_node:
#endif
err_queue_prepared_already:
err_kva_changed:
err_dva_changed:
err_get_kva_for_ext:
err_get_dva_for_ext:
	return ret;
}

int is_queue_buffer_init(struct vb2_buffer *vb)
{
	struct vb2_v4l2_buffer *vb2_v4l2_buf = to_vb2_v4l2_buffer(vb);
	struct is_vb2_buf *vbuf = vb_to_is_vb2_buf(vb2_v4l2_buf);
	struct is_video_ctx *vctx = vb->vb2_queue->drv_priv;

	vbuf->ops = vctx->is_vb2_buf_ops;

	return 0;
}

void is_queue_buffer_cleanup(struct vb2_buffer *vb)
{
	struct vb2_v4l2_buffer *vb2_v4l2_buf = to_vb2_v4l2_buffer(vb);
	struct is_vb2_buf *vbuf = vb_to_is_vb2_buf(vb2_v4l2_buf);
	struct is_video_ctx *vctx = vb->vb2_queue->drv_priv;
	unsigned int num_i_planes = vb->num_planes - NUM_OF_META_PLANE;
	unsigned int num_m_planes;
	unsigned int pos_ext_p;
	unsigned int num_ext_planes = 0;
	int i;

	if (test_bit(IS_QUEUE_NEED_TO_EXTMAP, &vctx->queue.state))
		num_ext_planes = num_i_planes - vctx->queue.framecfg.format->hw_plane;

	num_i_planes -= num_ext_planes;
	num_m_planes = vbuf->num_merged_sbufs ? vbuf->num_merged_sbufs : 1;


	/* FIXME: doesn't support dmabuf container yet */
	if (test_bit(IS_QUEUE_NEED_TO_KMAP, &vctx->queue.state)) {
		for (i = 0; i < vb->num_planes; i++)
			vbuf->ops->plane_kunmap(vbuf, i, 0);
	} else {
		if (vbuf->num_merged_sbufs)
			vbuf->ops->dbufcon_kunmap(vbuf, num_i_planes);
		else
			vbuf->ops->plane_kunmap(vbuf, num_i_planes, 0);

		if (num_ext_planes) {
			pos_ext_p = num_i_planes + num_m_planes;
			for (i = 0; i < num_ext_planes; i++)
				vbuf->ops->plane_kunmap(vbuf, pos_ext_p + i, 0);
		}
	}
}


int is_queue_buffer_prepare(struct vb2_buffer *vb)
{
	int ret;
	struct vb2_v4l2_buffer *vb2_v4l2_buf = to_vb2_v4l2_buffer(vb);
	struct is_vb2_buf *vbuf = vb_to_is_vb2_buf(vb2_v4l2_buf);
	struct is_video_ctx *vctx = vb->vb2_queue->drv_priv;
	struct is_queue *queue = GET_QUEUE(vctx);
	struct is_video *video = GET_VIDEO(vctx);
	struct is_ion_ctx *ctx = video->alloc_ctx;
	unsigned int index = vb->index;
	struct is_frame *frame = &queue->framemgr.frames[index];
	u32 num_i_planes = vb->num_planes - NUM_OF_META_PLANE;
	u32 num_buffers = 1, num_shots = 1, pos_meta_p, i;
	ulong kva_meta;
	bool need_vmap;

	/* take a snapshot whether it is needed or not */
	need_vmap = (dbg_draw_digit_ctrl ||
		((dbg_dma_dump_ctrl & ~DDD_CTRL_META)
		&& (video->vd.dev_debug & V4L2_DEV_DEBUG_DMA))
		);

	/* Unmerged dma_buf_container */
	if (IS_ENABLED(CONFIG_DMA_BUF_CONTAINER)) {
		ret = vbuf->ops->dbufcon_prepare(vbuf, ctx->dev);
		if (ret) {
			mverr("failed to prepare dmabuf-container: %d",
					vctx, video, index);
			return ret;
		}

		if (vbuf->num_merged_dbufs) {
			ret = vbuf->ops->dbufcon_map(vbuf);
			if (ret) {
				mverr("failed to map dmabuf-container: %d",
						vctx, video, index);
				vbuf->ops->dbufcon_finish(vbuf);
				return ret;
			}

			num_buffers = vbuf->num_merged_dbufs;
		}

		/* Unmerged meta plane */
		ret = vbuf->ops->dbufcon_kmap(vbuf, num_i_planes);
		if (ret) {
			mverr("failed to kmap dmabuf-container: %d",
					vctx, video, index);
			return ret;
		}

		if (vbuf->num_merged_sbufs)
			num_shots = vbuf->num_merged_sbufs;
	}

	/* Get kva of image planes */
	if (test_bit(IS_QUEUE_NEED_TO_KMAP, &queue->state)) {
		for (i = 0; i < num_i_planes; i++)
			vbuf->ops->plane_kmap(vbuf, i, 0);
	} else if (need_vmap) {
		if (vbuf->num_merged_dbufs) {
			for (i = 0; i < num_i_planes; i++)
				vbuf->ops->dbufcon_kmap(vbuf, i);
		} else {
			for (i = 0; i < num_i_planes; i++)
				vbuf->kva[i] = vbuf->ops->plane_kvaddr(vbuf, i);
		}
	}

	/* Disable SBWC for drawing digit on it */
	if (dbg_draw_digit_ctrl)
		queue->framecfg.hw_pixeltype &= ~PIXEL_TYPE_EXTRA_MASK;

	/* Get metadata planes */
	pos_meta_p = num_i_planes * num_buffers;
	if (num_shots > 1) {
		for (i = 0; i < num_shots; i++)
			queue->buf_kva[index][pos_meta_p + i] =
				vbuf->kva[pos_meta_p + i];
	} else {
		kva_meta = vbuf->ops->plane_kmap(vbuf, num_i_planes, 0);
		if (!kva_meta) {
			mverr("[%s][I%d][P%d]Failed to get kva", vctx, video,
				queue->name, index, pos_meta_p);
			return -ENOMEM;
		}

		queue->buf_kva[index][pos_meta_p] = kva_meta;
	}

	/* Setup frame */
	frame->num_buffers = num_buffers;
	frame->planes = num_i_planes * num_buffers;
	frame->num_shots = num_shots;
	kva_meta = queue->buf_kva[index][pos_meta_p];
	if (video->type == IS_VIDEO_TYPE_LEADER) {
		/* Output node */
		frame->shot_ext = (struct camera2_shot_ext *)kva_meta;
		frame->shot = &frame->shot_ext->shot;
		frame->shot_size = sizeof(frame->shot);
		if (sizeof(struct camera2_shot_ext) > SIZE_OF_META_PLANE) {
			mverr("Meta size overflow %d", vctx, video,
				sizeof(struct camera2_shot_ext));
			FIMC_BUG(1);
		}

		if (frame->shot->magicNumber != SHOT_MAGIC_NUMBER) {
			mverr("[%s][I%d]Shot magic number error! 0x%08X size %zd",
					vctx, video,
					queue->name, index,
					frame->shot->magicNumber,
					sizeof(struct camera2_shot_ext));
			return -EINVAL;
		}
	} else {
		/* Capture node */
		frame->stream = (struct camera2_stream *)kva_meta;

		/* TODO : Change type of address into ulong */
		frame->stream->address = (u32)kva_meta;
	}

#ifdef ENABLE_LOGICAL_VIDEO_NODE
	queue->mode = CAMERA_NODE_LOGICAL;

	if (video->type == IS_VIDEO_TYPE_LEADER
		&& queue->mode == CAMERA_NODE_LOGICAL) {
		ctx = video->resourcemgr->mem.default_ctx;

		ret = _is_queue_subbuf_prepare(ctx->dev, vbuf,
				&frame->shot_ext->node_group, need_vmap);
		if (ret) {
			mverr("[%s][I%d]Failed to subbuf_prepare", vctx, video,
					queue->name,
					index);
			return ret;
		}
	}
#endif

	if (test_bit(IS_QUEUE_NEED_TO_REMAP, &queue->state)) {
		ret = vbuf->ops->remap_attr(vbuf, 0);
		if (ret) {
			mverr("failed to remap dmabuf: %d", vctx, video, index);
			return ret;
		}
	}

	queue->buf_pre++;

	return 0;
}

#define V4L2_BUF_FLAG_DISPOSAL	0x10000000
void is_queue_buffer_finish(struct vb2_buffer *vb)
{
	struct vb2_v4l2_buffer *vb2_v4l2_buf = to_vb2_v4l2_buffer(vb);
	struct vb2_queue *q = vb->vb2_queue;
	struct is_vb2_buf *vbuf = vb_to_is_vb2_buf(vb2_v4l2_buf);
	struct is_video_ctx *vctx = vb->vb2_queue->drv_priv;
	struct is_video *video = GET_VIDEO(vctx);
	struct is_queue *queue = GET_QUEUE(vctx);
	u32 framecount = queue->framemgr.frames[vb->index].fcount;
	bool ddd_trigger = false;
	unsigned int num_i_planes = vb->num_planes - NUM_OF_META_PLANE;
	unsigned int num_ext_planes = 0;
	int plane;
	struct vb2_plane *p;
	u32 i;
#if defined(ENABLE_LOGICAL_VIDEO_NODE) && !defined(ENABLE_SKIP_PER_FRAME_MAP)
	unsigned int index = vb->index;
	struct is_sub_buf *sbuf;
	struct is_sub_dma_buf *sdbuf;
	u32 n;
#endif

	if (test_bit(IS_QUEUE_NEED_TO_EXTMAP, &queue->state))
		num_ext_planes = num_i_planes - queue->framecfg.format->hw_plane;

	num_i_planes -= num_ext_planes;

	if (dbg_draw_digit_ctrl)
		is_queue_draw_digit(queue, vbuf);

	if (video->vd.dev_debug & V4L2_DEV_DEBUG_DMA) {
		if (dbg_dma_dump_type == DDD_TYPE_PERIOD)
			ddd_trigger = !(framecount % dbg_dma_dump_arg1);
		else if (dbg_dma_dump_type == DDD_TYPE_INTERVAL)
			ddd_trigger = (framecount >= dbg_dma_dump_arg1)
					&& (framecount <= dbg_dma_dump_arg2);
		else if (dbg_dma_dump_type == DDD_TYPE_ONESHOT)
			ddd_trigger = (framecount == dbg_dma_dump_arg1);

		if (ddd_trigger && (dbg_dma_dump_ctrl & DDD_CTRL_IMAGE))
			is_dbg_dma_dump(&vctx->queue, vctx->instance, vb->index,
					video->id, DBG_DMA_DUMP_IMAGE);

		if (ddd_trigger && (dbg_dma_dump_ctrl & DDD_CTRL_META))
			is_dbg_dma_dump(&vctx->queue, vctx->instance, vb->index,
					video->id, DBG_DMA_DUMP_META);
	}

#ifdef ENABLE_LOGICAL_VIDEO_NODE
	if (queue->mode == CAMERA_NODE_LOGICAL) {
		mdbgv_lvn(3, "[%s][I%d]buf_finish: queue 0x%lx\n",
			vctx, video,
			vn_name[video->id], index, queue);
#ifndef ENABLE_SKIP_PER_FRAME_MAP
#ifdef ENABLE_LVN_DUMMYOUTPUT
		/* release input */
		sbuf = &queue->in_buf[index];
		sdbuf = &sbuf->sub[0];
		vbuf->ops->subbuf_finish(sdbuf);
		sbuf->ldr_vid = 0;
#endif
		/* release output */
		for (n = 0; n < CAPTURE_NODE_MAX; n++) {
			sbuf = &queue->out_buf[index];
			sdbuf = &sbuf->sub[n];
			if (!sdbuf->vid)
				continue;

			if (sdbuf->kva[0])
				vbuf->ops->subbuf_kunmap(sdbuf);

			vbuf->ops->subbuf_finish(sdbuf);
			sbuf->ldr_vid = 0;
		}
#endif
	}
#endif

	if (IS_ENABLED(CONFIG_DMA_BUF_CONTAINER) &&
			(vbuf->num_merged_dbufs)) {
		for (i = 0; i < num_i_planes && vbuf->kva[i]; i++)
			vbuf->ops->dbufcon_kunmap(vbuf, i);

		vbuf->ops->dbufcon_unmap(vbuf);
		vbuf->ops->dbufcon_finish(vbuf);
	}

	if (test_bit(IS_QUEUE_NEED_TO_REMAP, &vctx->queue.state))
		vbuf->ops->unremap_attr(vbuf, 0);

	if ((vb2_v4l2_buf->flags & V4L2_BUF_FLAG_DISPOSAL) &&
			q->memory == VB2_MEMORY_DMABUF) {
		is_queue_buffer_cleanup(vb);

		for (plane = 0; plane < vb->num_planes; plane++) {
			p = &vb->planes[plane];

			if (!p->mem_priv)
				continue;

			if (p->dbuf_mapped)
#if (LINUX_VERSION_CODE >= KERNEL_VERSION(5, 4, 0))
				q->mem_ops->unmap_dmabuf(p->mem_priv);
#else
				q->mem_ops->unmap_dmabuf(p->mem_priv, 0);
#endif

			q->mem_ops->detach_dmabuf(p->mem_priv);
			dma_buf_put(p->dbuf);
			p->mem_priv = NULL;
			p->dbuf = NULL;
			p->dbuf_mapped = 0;
		}
	}
}

void is_queue_wait_prepare(struct vb2_queue *vbq)
{
	struct is_video_ctx *vctx;
	struct is_video *video;

	FIMC_BUG_VOID(!vbq);

	vctx = vbq->drv_priv;
	if (!vctx) {
		err("vctx is NULL");
		return;
	}

	video = vctx->video;
	mutex_unlock(&video->lock);
}

void is_queue_wait_finish(struct vb2_queue *vbq)
{
	struct is_video_ctx *vctx;
	struct is_video *video;

	FIMC_BUG_VOID(!vbq);

	vctx = vbq->drv_priv;
	if (!vctx) {
		err("vctx is NULL");
		return;
	}

	video = vctx->video;
	mutex_lock(&video->lock);
}

int is_queue_start_streaming(struct is_queue *queue,
	void *qdevice)
{
	int ret = 0;

	FIMC_BUG(!queue);

	if (test_bit(IS_QUEUE_STREAM_ON, &queue->state)) {
		err("[%s] already stream on(%ld)", queue->name, queue->state);
		ret = -EINVAL;
		goto p_err;
	}

	if (queue->buf_rdycount && !test_bit(IS_QUEUE_BUFFER_READY, &queue->state)) {
		err("[%s] buffer state is not ready(%ld)", queue->name, queue->state);
		ret = -EINVAL;
		goto p_err;
	}

	ret = CALL_QOPS(queue, start_streaming, qdevice, queue);
	if (ret) {
		err("[%s] start_streaming is fail(%d)", queue->name, ret);
		ret = -EINVAL;
		goto p_err;
	}

	set_bit(IS_QUEUE_STREAM_ON, &queue->state);

p_err:
	return ret;
}

int is_queue_stop_streaming(struct is_queue *queue,
	void *qdevice)
{
	int ret = 0;

	FIMC_BUG(!queue);

	if (!test_bit(IS_QUEUE_STREAM_ON, &queue->state)) {
		err("[%s] already stream off(%ld)", queue->name, queue->state);
		ret = -EINVAL;
		goto p_err;
	}

	ret = CALL_QOPS(queue, stop_streaming, qdevice, queue);
	if (ret) {
		err("[%s] stop_streaming is fail(%d)", queue->name, ret);
		ret = -EINVAL;
	}

	clear_bit(IS_QUEUE_STREAM_ON, &queue->state);
	clear_bit(IS_QUEUE_BUFFER_READY, &queue->state);
	clear_bit(IS_QUEUE_BUFFER_PREPARED, &queue->state);

p_err:
	return ret;
}

int is_video_probe(struct is_video *video,
	char *video_name,
	u32 video_number,
	u32 vfl_dir,
	struct is_mem *mem,
	struct v4l2_device *v4l2_dev,
	const struct v4l2_file_operations *fops,
	const struct v4l2_ioctl_ops *ioctl_ops)
{
	int ret = 0;
	u32 video_id;

	vref_init(video);
	mutex_init(&video->lock);
	snprintf(video->vd.name, sizeof(video->vd.name), "%s", video_name);
	video->id		= video_number;
	video->vb2_mem_ops	= mem->vb2_mem_ops;
	video->is_vb2_buf_ops	= mem->is_vb2_buf_ops;
	video->alloc_ctx	= mem->default_ctx;
	video->type		= (vfl_dir == VFL_DIR_RX) ? IS_VIDEO_TYPE_CAPTURE : IS_VIDEO_TYPE_LEADER;
	video->vd.vfl_dir	= vfl_dir;
	video->vd.v4l2_dev	= v4l2_dev;
	video->vd.fops		= fops;
	video->vd.ioctl_ops	= ioctl_ops;
	video->vd.minor		= -1;
	video->vd.release	= video_device_release;
	video->vd.lock		= &video->lock;
	video->vd.device_caps	= (vfl_dir == VFL_DIR_RX) ? VIDEO_CAPTURE_DEVICE_CAPS : VIDEO_OUTPUT_DEVICE_CAPS;
	video_set_drvdata(&video->vd, video);

	video_id = EXYNOS_VIDEONODE_FIMC_IS + video_number;
	ret = video_register_device(&video->vd, VFL_TYPE_GRABBER, video_id);
	if (ret) {
		err("[V%02d] Failed to register video device", video->id);
		goto p_err;
	}

p_err:
	info("[VID] %s(%d) is created. minor(%d)\n", video_name, video_id, video->vd.minor);
	return ret;
}

int is_video_open(struct is_video_ctx *vctx,
	void *device,
	u32 buf_rdycount,
	struct is_video *video,
	const struct vb2_ops *vb2_ops,
	const struct is_queue_ops *qops)
{
	int ret = 0;
	struct is_queue *queue;

	FIMC_BUG(!vctx);
	FIMC_BUG(!video);
	FIMC_BUG(!video->vb2_mem_ops);
	FIMC_BUG(!vb2_ops);

	if (!(vctx->state & BIT(IS_VIDEO_CLOSE))) {
		mverr("already open(%lX)", vctx, video, vctx->state);
		return -EEXIST;
	}

	if (atomic_read(&video->refcount) == 1) {
		sema_init(&video->smp_multi_input, 1);
		video->try_smp		= false;
	}

	queue = GET_QUEUE(vctx);
	queue->vbq = NULL;
	queue->qops = qops;

	vctx->device		= device;
	vctx->next_device	= NULL;
	vctx->subdev		= NULL;
	vctx->video		= video;
	vctx->vb2_ops		= vb2_ops;
	vctx->vb2_mem_ops	= video->vb2_mem_ops;
	vctx->is_vb2_buf_ops	= video->is_vb2_buf_ops;
	vctx->vops.qbuf		= is_video_qbuf;
	vctx->vops.dqbuf	= is_video_dqbuf;
	vctx->vops.done 	= is_video_buffer_done;

	ret = is_queue_open(queue, buf_rdycount);
	if (ret) {
		mverr("is_queue_open is fail(%d)", vctx, video, ret);
		goto p_err;
	}

	queue->vbq = vzalloc(sizeof(struct vb2_queue));
	if (!queue->vbq) {
		mverr("vzalloc is fail", vctx, video);
		ret = -ENOMEM;
		goto p_err;
	}

	ret = queue_init(vctx, queue->vbq, NULL);
	if (ret) {
		mverr("queue_init fail", vctx, video);
		vfree(queue->vbq);
		queue->vbq = NULL;
		goto p_err;
	}

	vctx->state = BIT(IS_VIDEO_OPEN);

p_err:
	return ret;
}

int is_video_close(struct is_video_ctx *vctx)
{
	int ret = 0;
	struct is_video *video;
	struct is_queue *queue;

	FIMC_BUG(!vctx);
	FIMC_BUG(!GET_VIDEO(vctx));

	video = GET_VIDEO(vctx);
	queue = GET_QUEUE(vctx);

	if (vctx->state < BIT(IS_VIDEO_OPEN)) {
		mverr("already close(%lX)", vctx, video, vctx->state);
		return -ENOENT;
	}

	vb2_queue_release(queue->vbq);
	vfree(queue->vbq);
	queue->vbq = NULL;
	is_queue_close(queue);

	/*
	 * vb2 release can call stop callback
	 * not if video node is not stream off
	 */
	vctx->device = NULL;
	vctx->state = BIT(IS_VIDEO_CLOSE);

	return ret;
}

int is_video_s_input(struct file *file,
	struct is_video_ctx *vctx)
{
	u32 ret = 0;

	if (!(vctx->state & (BIT(IS_VIDEO_OPEN) | BIT(IS_VIDEO_S_INPUT) | BIT(IS_VIDEO_S_BUFS)))) {
		mverr(" invalid s_input is requested(%lX)", vctx, vctx->video, vctx->state);
		return -EINVAL;
	}

	vctx->state = BIT(IS_VIDEO_S_INPUT);

	return ret;
}

int is_video_poll(struct file *file,
	struct is_video_ctx *vctx,
	struct poll_table_struct *wait)
{
	u32 ret = 0;
	struct is_queue *queue;

	FIMC_BUG(!vctx);

	queue = GET_QUEUE(vctx);
	ret = vb2_poll(queue->vbq, file, wait);

	return ret;
}

int is_video_mmap(struct file *file,
	struct is_video_ctx *vctx,
	struct vm_area_struct *vma)
{
	u32 ret = 0;
	struct is_queue *queue;

	FIMC_BUG(!vctx);

	queue = GET_QUEUE(vctx);

	ret = vb2_mmap(queue->vbq, vma);

	return ret;
}

int is_video_reqbufs(struct file *file,
	struct is_video_ctx *vctx,
	struct v4l2_requestbuffers *request)
{
	int ret = 0;
	struct is_queue *queue;
	struct is_framemgr *framemgr;
	struct is_video *video;

	FIMC_BUG(!vctx);
	FIMC_BUG(!request);

	video = GET_VIDEO(vctx);
	if (!(vctx->state & (BIT(IS_VIDEO_S_FORMAT) | BIT(IS_VIDEO_STOP) | BIT(IS_VIDEO_S_BUFS)))) {
		mverr("invalid reqbufs is requested(%lX)", vctx, video, vctx->state);
		return -EINVAL;
	}

	queue = GET_QUEUE(vctx);
	if (test_bit(IS_QUEUE_STREAM_ON, &queue->state)) {
		mverr("video is stream on, not applied", vctx, video);
		ret = -EINVAL;
		goto p_err;
	}

	/* before call queue ops if request count is zero */
	if (!request->count) {
		ret = CALL_QOPS(queue, request_bufs, GET_DEVICE(vctx), queue, request->count);
		if (ret) {
			mverr("request_bufs is fail(%d)", vctx, video, ret);
			goto p_err;
		}
	}

	ret = vb2_reqbufs(queue->vbq, request);
	if (ret) {
		mverr("vb2_reqbufs is fail(%d)", vctx, video, ret);
		goto p_err;
	}

	framemgr = &queue->framemgr;
	queue->buf_maxcount = request->count;
	if (queue->buf_maxcount == 0) {
		queue->buf_req = 0;
		queue->buf_pre = 0;
		queue->buf_que = 0;
		queue->buf_com = 0;
		queue->buf_dqe = 0;
		queue->buf_refcount = 0;
		clear_bit(IS_QUEUE_BUFFER_READY, &queue->state);
		clear_bit(IS_QUEUE_BUFFER_PREPARED, &queue->state);
		frame_manager_close(framemgr);
	} else {
		if (queue->buf_maxcount < queue->buf_rdycount) {
			mverr("buffer count is not invalid(%d < %d)", vctx, video,
				queue->buf_maxcount, queue->buf_rdycount);
			ret = -EINVAL;
			goto p_err;
		}

		ret = frame_manager_open(framemgr, queue->buf_maxcount);
		if (ret) {
			mverr("is_frame_open is fail(%d)", vctx, video, ret);
			goto p_err;
		}
	}

	/* after call queue ops if request count is not zero */
	if (request->count) {
		ret = CALL_QOPS(queue, request_bufs, GET_DEVICE(vctx), queue, request->count);
		if (ret) {
			mverr("request_bufs is fail(%d)", vctx, video, ret);
			goto p_err;
		}
	}

	vctx->state = BIT(IS_VIDEO_S_BUFS);

p_err:
	return ret;
}

int is_video_querybuf(struct file *file,
	struct is_video_ctx *vctx,
	struct v4l2_buffer *buf)
{
	int ret = 0;
	struct is_queue *queue;

	FIMC_BUG(!vctx);

	queue = GET_QUEUE(vctx);

	ret = vb2_querybuf(queue->vbq, buf);

	return ret;
}

int is_video_set_format_mplane(struct file *file,
	struct is_video_ctx *vctx,
	struct v4l2_format *format)
{
	int ret = 0;
	u32 condition;
	void *device;
	struct is_video *video;
	struct is_queue *queue;

	FIMC_BUG(!vctx);
	FIMC_BUG(!GET_DEVICE(vctx));
	FIMC_BUG(!GET_VIDEO(vctx));
	FIMC_BUG(!format);

	device = GET_DEVICE(vctx);
	video = GET_VIDEO(vctx);
	queue = GET_QUEUE(vctx);

	/* capture video node can skip s_input */
	if (video->type == IS_VIDEO_TYPE_LEADER)
		condition = BIT(IS_VIDEO_S_INPUT)
			| BIT(IS_VIDEO_S_FORMAT)
			| BIT(IS_VIDEO_S_BUFS);
	else
		condition = BIT(IS_VIDEO_S_INPUT)
			| BIT(IS_VIDEO_S_BUFS)
			| BIT(IS_VIDEO_S_FORMAT)
			| BIT(IS_VIDEO_OPEN)
			| BIT(IS_VIDEO_STOP);

	if (!(vctx->state & condition)) {
		mverr("invalid s_format is requested(%lX)", vctx, video, vctx->state);
		return -EINVAL;
	}

	ret = is_queue_set_format_mplane(vctx, queue, device, format);
	if (ret) {
		mverr("is_queue_set_format_mplane is fail(%d)", vctx, video, ret);
		goto p_err;
	}

	vctx->state = BIT(IS_VIDEO_S_FORMAT);

p_err:
	mdbgv_vid("set_format(%d x %d)\n", queue->framecfg.width, queue->framecfg.height);
	return ret;
}

int is_video_qbuf(struct is_video_ctx *vctx,
	struct v4l2_buffer *buf)
{
	int ret = 0;
	struct is_queue *queue;
	struct vb2_queue *vbq;
	struct vb2_buffer *vb;
	struct is_video *video;
	int plane;
	struct vb2_plane planes[VB2_MAX_PLANES];

	FIMC_BUG(!vctx);
	FIMC_BUG(!buf);

	TIME_QUEUE(TMQ_QS);

	queue = GET_QUEUE(vctx);
	vbq = queue->vbq;
	video = GET_VIDEO(vctx);

	if (!vbq) {
		mverr("vbq is NULL", vctx, video);
		ret = -EINVAL;
		goto p_err;
	}

	queue->buf_req++;

	ret = is_vb2_qbuf(queue->vbq, buf);
	if (ret) {
		mverr("vb2_qbuf is fail(index : %d, %d)", vctx, video, buf->index, ret);

		if (vbq->fileio) {
			mverr("file io in progress", vctx, video);
			ret = -EBUSY;
			goto p_err;
		}

		if (buf->type != queue->vbq->type) {
			mverr("buf type is invalid(%d != %d)", vctx, video,
				buf->type, queue->vbq->type);
			ret = -EINVAL;
			goto p_err;
		}

		if (buf->index >= vbq->num_buffers) {
			mverr("buffer index%d out of range", vctx, video, buf->index);
			ret = -EINVAL;
			goto p_err;
		}

		if (buf->memory != vbq->memory) {
			mverr("invalid memory type%d", vctx, video, buf->memory);
			ret = -EINVAL;
			goto p_err;
		}

		vb = vbq->bufs[buf->index];
		if (!vb) {
			mverr("vb is NULL", vctx, video);
			ret = -EINVAL;
			goto p_err;
		}

		if (V4L2_TYPE_IS_MULTIPLANAR(buf->type)) {
			/* Is memory for copying plane information present? */
			if (buf->m.planes == NULL) {
				mverr("multi-planar buffer passed but "
					   "planes array not provided\n", vctx, video);
				ret = -EINVAL;
				goto p_err;
			}

			if (buf->length < vb->num_planes || buf->length > VB2_MAX_PLANES) {
				mverr("incorrect planes array length, "
					   "expected %d, got %d\n", vctx, video,
					   vb->num_planes, buf->length);
				ret = -EINVAL;
				goto p_err;
			}
		}

		/* for detect vb2 framework err, operate some vb2 functions */
		memset(planes, 0, sizeof(planes[0]) * vb->num_planes);
		vb->vb2_queue->buf_ops->is_fill_vb2_buffer(vb, buf, planes);

		for (plane = 0; plane < vb->num_planes; ++plane) {
			struct dma_buf *dbuf;

			dbuf = dma_buf_get(planes[plane].m.fd);
			if (IS_ERR_OR_NULL(dbuf)) {
				mverr("invalid dmabuf fd(%d) for plane %d\n",
					vctx, video, planes[plane].m.fd, plane);
				goto p_err;
			}

			if (planes[plane].length == 0)
				planes[plane].length = (unsigned int)dbuf->size;

			if (planes[plane].length < vb->planes[plane].min_length) {
				mverr("invalid dmabuf length %u for plane %d, "
					"minimum length %u\n",
					vctx, video, planes[plane].length, plane,
					vb->planes[plane].min_length);
				dma_buf_put(dbuf);
				goto p_err;
			}

			dma_buf_put(dbuf);
		}

		goto p_err;
	}

p_err:
	TIME_QUEUE(TMQ_QE);
	return ret;
}

int is_video_dqbuf(struct is_video_ctx *vctx,
	struct v4l2_buffer *buf,
	bool blocking)
{
	int ret = 0;
	struct is_video *video;
	struct is_queue *queue;

	FIMC_BUG(!vctx);
	FIMC_BUG(!GET_VIDEO(vctx));
	FIMC_BUG(!buf);

	video = GET_VIDEO(vctx);
	queue = GET_QUEUE(vctx);

	if (!queue->vbq) {
		mverr("vbq is NULL", vctx, video);
		ret = -EINVAL;
		goto p_err;
	}

	if (buf->type != queue->vbq->type) {
		mverr("buf type is invalid(%d != %d)", vctx, video, buf->type, queue->vbq->type);
		ret = -EINVAL;
		goto p_err;
	}

	if (!test_bit(IS_QUEUE_STREAM_ON, &queue->state)) {
		mverr("queue is not streamon(%ld)", vctx,  video, queue->state);
		ret = -EINVAL;
		goto p_err;
	}

	queue->buf_dqe++;

	ret = vb2_dqbuf(queue->vbq, buf, blocking);
	if (ret) {
		mverr("vb2_dqbuf is fail(%d)", vctx,  video, ret);
		if (test_bit(IS_HAL_DEBUG_SUDDEN_DEAD_DETECT, &sysfs_debug.hal_debug_mode) &&
				ret == -ERESTARTSYS) {
			msleep(sysfs_debug.hal_debug_delay);
			panic("HAL dead");
		}
		goto p_err;
	}

p_err:
	TIME_QUEUE(TMQ_DQ);
	return ret;
}

int is_video_prepare(struct file *file,
	struct is_video_ctx *vctx,
	struct v4l2_buffer *buf)
{
	int ret = 0;
	unsigned int index = 0;
	struct is_device_ischain *device;
	struct is_queue *queue;
	struct vb2_queue *vbq;
	struct vb2_buffer *vb;
	struct is_video *video;
#ifdef ENABLE_BUFFER_HIDING
	struct is_pipe *pipe;
#endif

	FIMC_BUG(!file);
	FIMC_BUG(!vctx);
	FIMC_BUG(!buf);

	device = GET_DEVICE_ISCHAIN(vctx);
	queue = GET_QUEUE(vctx);
	vbq = queue->vbq;
	video = GET_VIDEO(vctx);
	index = buf->index;
	if (index >= VB2_MAX_FRAME) {
                mverr("buffer index[%d] range over", vctx, video, buf->index);
                ret = -EINVAL;
                goto p_err;
	}

        vb = queue->vbq->bufs[index];
        if (!vb) {
                mverr("vb is NULL", vctx, video);
                ret = -EINVAL;
                goto p_err;
        }
#ifdef ENABLE_BUFFER_HIDING
	pipe = &device->pipe;

	if ((pipe->dst) && (pipe->dst->leader.vid == video->id)) {
		if (index >=  IS_MAX_PIPE_BUFS) {
			mverr("The leader index(%d) is bigger than array size(%d)",
				vctx, video, index, IS_MAX_PIPE_BUFS);
			ret = -EINVAL;
			goto p_err;
		}

		if (!V4L2_TYPE_IS_MULTIPLANAR(buf->type)) {
			mverr("the type of passed buffer is not multi-planar",
					vctx, video);
			ret = -EINVAL;
			goto p_err;
		}

		if (!buf->m.planes) {
			mverr("planes array not provided", vctx, video);
			ret = -EINVAL;
			goto p_err;
		}

		if (buf->length > IS_MAX_PLANES) {
			mverr("incorrect planes array length", vctx, video);
			ret = -EINVAL;
			goto p_err;
		}

		/* Destination */
		memcpy(&pipe->buf[PIPE_SLOT_DST][index], buf, sizeof(struct v4l2_buffer));
		memcpy(pipe->planes[PIPE_SLOT_DST][index], buf->m.planes, sizeof(struct v4l2_plane) * buf->length);
		pipe->buf[PIPE_SLOT_DST][index].m.planes = (struct v4l2_plane *)pipe->planes[PIPE_SLOT_DST][index];
	} else if ((pipe->dst) && (pipe->vctx[PIPE_SLOT_JUNCTION]) &&
			(pipe->vctx[PIPE_SLOT_JUNCTION]->video->id == video->id)) {
		if (index >=  IS_MAX_PIPE_BUFS) {
			mverr("The junction index(%d) is bigger than array size(%d)",
				vctx, video, index, IS_MAX_PIPE_BUFS);
			ret = -EINVAL;
			goto p_err;
		}

		/* Junction */
		if ((pipe->dst) && test_bit(IS_GROUP_PIPE_INPUT, &pipe->dst->state)) {
			memcpy(&pipe->buf[PIPE_SLOT_JUNCTION][index], buf, sizeof(struct v4l2_buffer));
			memcpy(pipe->planes[PIPE_SLOT_JUNCTION][index], buf->m.planes, sizeof(struct v4l2_plane) * buf->length);
			pipe->buf[PIPE_SLOT_JUNCTION][index].m.planes = (struct v4l2_plane *)pipe->planes[PIPE_SLOT_JUNCTION][index];
		}
	}
#endif

	if (!vbq) {
		mverr("vbq is NULL", vctx, video);
		ret = -EINVAL;
		goto p_err;
	}

	vb = vbq->bufs[buf->index];
	if (!vb) {
		mverr("vb is NULL", vctx, video);
		ret = -EINVAL;
		goto p_err;
	}

	ret = is_vb2_prepare_buf(vbq, buf);
	if (ret) {
		mverr("vb2_prepare_buf is fail(index : %d, %d)", vctx, video, buf->index, ret);
		goto p_err;
	}

	ret = is_queue_buffer_queue(queue, vb);
	if (ret) {
		mverr("is_queue_buffer_queue is fail(index : %d, %d)", vctx, video, buf->index, ret);
		goto p_err;
	}

p_err:
	return ret;
}

int is_video_streamon(struct file *file,
	struct is_video_ctx *vctx,
	enum v4l2_buf_type type)
{
	int ret = 0;
	struct vb2_queue *vbq;
	struct is_video *video;

	FIMC_BUG(!file);
	FIMC_BUG(!vctx);

	video = GET_VIDEO(vctx);
	if (!(vctx->state & (BIT(IS_VIDEO_S_BUFS) | BIT(IS_VIDEO_STOP)))) {
		mverr("invalid streamon is requested(%lX)", vctx, video, vctx->state);
		return -EINVAL;
	}

	vbq = GET_QUEUE(vctx)->vbq;
	if (!vbq) {
		mverr("vbq is NULL", vctx, video);
		ret = -EINVAL;
		goto p_err;
	}

	if (vbq->type != type) {
		mverr("invalid stream type(%d != %d)", vctx, video, vbq->type, type);
		ret = -EINVAL;
		goto p_err;
	}

	if (vb2_is_streaming(vbq)) {
		mverr("streamon: already streaming", vctx, video);
		ret = -EINVAL;
		goto p_err;
	}

	ret = vb2_streamon(vbq, type);
	if (ret) {
		mverr("vb2_streamon is fail(%d)", vctx, video, ret);
		goto p_err;
	}

	vctx->state = BIT(IS_VIDEO_START);

p_err:
	return ret;
}

int is_video_streamoff(struct file *file,
	struct is_video_ctx *vctx,
	enum v4l2_buf_type type)
{
	int ret = 0;
	u32 rcnt, pcnt, ccnt;
	struct is_queue *queue;
	struct vb2_queue *vbq;
	struct is_framemgr *framemgr;
	struct is_video *video;

	FIMC_BUG(!file);
	FIMC_BUG(!vctx);

	video = GET_VIDEO(vctx);
	if (!(vctx->state & BIT(IS_VIDEO_START))) {
		mverr("invalid streamoff is requested(%lX)", vctx, video, vctx->state);
		return -EINVAL;
	}

	queue = GET_QUEUE(vctx);
	framemgr = &queue->framemgr;
	vbq = queue->vbq;
	if (!vbq) {
		mverr("vbq is NULL", vctx, video);
		ret = -EINVAL;
		goto p_err;
	}

	framemgr_e_barrier_irq(framemgr, FMGR_IDX_0);
	rcnt = framemgr->queued_count[FS_REQUEST];
	pcnt = framemgr->queued_count[FS_PROCESS];
	ccnt = framemgr->queued_count[FS_COMPLETE];
	framemgr_x_barrier_irq(framemgr, FMGR_IDX_0);

	if (rcnt + pcnt + ccnt > 0)
		mvinfo("[%s] stream off : queued buffer is not empty(R%d, P%d, C%d)", vctx,
			video, __func__, rcnt, pcnt, ccnt);

	if (vbq->type != type) {
		mverr("invalid stream type(%d != %d)", vctx, video, vbq->type, type);
		ret = -EINVAL;
		goto p_err;
	}

	if (!vbq->streaming) {
		mverr("streamoff: not streaming", vctx, video);
		ret = -EINVAL;
		goto p_err;
	}

	ret = vb2_streamoff(vbq, type);
	if (ret) {
		mverr("vb2_streamoff is fail(%d)", vctx, video, ret);
		goto p_err;
	}

	ret = frame_manager_flush(framemgr);
	if (ret) {
		mverr("is_frame_flush is fail(%d)", vctx, video, ret);
		ret = -EINVAL;
		goto p_err;
	}

	vctx->state = BIT(IS_VIDEO_STOP);

p_err:
	return ret;
}

int is_video_s_ctrl(struct file *file,
	struct is_video_ctx *vctx,
	struct v4l2_control *ctrl)
{
	int ret = 0;
	struct is_video *video;
	struct is_device_ischain *device;
	struct is_resourcemgr *resourcemgr;
	struct is_queue *queue;
	struct is_group *head;

	FIMC_BUG(!vctx);
	FIMC_BUG(!GET_DEVICE_ISCHAIN(vctx));
	FIMC_BUG(!GET_VIDEO(vctx));
	FIMC_BUG(!ctrl);

	device = GET_DEVICE_ISCHAIN(vctx);
	video = GET_VIDEO(vctx);
	queue = GET_QUEUE(vctx);
	resourcemgr = device->resourcemgr;

	ret = is_vender_video_s_ctrl(ctrl, device);
	if (ret) {
		mverr("is_vender_video_s_ctrl is fail(%d)", vctx, video, ret);
		goto p_err;
	}

	switch (ctrl->id) {
	case V4L2_CID_IS_END_OF_STREAM:
		ret = is_ischain_open_wrap(device, true);
		if (ret) {
			mverr("is_ischain_open_wrap is fail(%d)", vctx, video, ret);
			goto p_err;
		}
		break;
	case V4L2_CID_IS_SET_SETFILE:
	{
		u32 scenario;

		if (test_bit(IS_ISCHAIN_START, &device->state)) {
			mverr("device is already started, setfile applying is fail", vctx, video);
			ret = -EINVAL;
			goto p_err;
		}

		device->setfile = ctrl->value;
		scenario = (device->setfile & IS_SCENARIO_MASK) >> IS_SCENARIO_SHIFT;
		mvinfo(" setfile(%d), scenario(%d) at s_ctrl\n", device, video,
			device->setfile & IS_SETFILE_MASK, scenario);
		break;
	}
	case V4L2_CID_IS_S_DVFS_SCENARIO:
	{
		u32 vendor;

		device->dvfs_scenario = ctrl->value;
		vendor = (device->dvfs_scenario >> IS_DVFS_SCENARIO_VENDOR_SHIFT) & IS_DVFS_SCENARIO_VENDOR_MASK;
		mvinfo("[DVFS] value(%x), common(%d), vendor(%d) at s_ctrl\n", device, video,
			device->dvfs_scenario, device->dvfs_scenario & IS_DVFS_SCENARIO_COMMON_MODE_MASK, vendor);
		break;
	}
	case V4L2_CID_IS_S_DVFS_RECORDING_SIZE:
	{
		device->dvfs_rec_size = ctrl->value;
		mvinfo("[DVFS] rec_width(%d), rec_height(%d) at s_ctrl\n", device, video,
			device->dvfs_rec_size & 0xffff, device->dvfs_rec_size >> IS_DVFS_SCENARIO_HEIGHT_SHIFT);
		break;
	}
	case V4L2_CID_IS_HAL_VERSION:
		if (ctrl->value < 0 || ctrl->value >= IS_HAL_VER_MAX) {
			mverr("hal version(%d) is invalid", vctx, video, ctrl->value);
			ret = -EINVAL;
			goto p_err;
		}
		resourcemgr->hal_version = ctrl->value;
		break;
	case V4L2_CID_IS_DEBUG_DUMP:
		info("Print fimc-is info dump by HAL");
		is_resource_dump();

		if (ctrl->value)
			panic("intentional panic from camera HAL");
		break;
	case V4L2_CID_IS_DVFS_CLUSTER0:
	case V4L2_CID_IS_DVFS_CLUSTER1:
	case V4L2_CID_IS_DVFS_CLUSTER2:
		is_resource_ioctl(resourcemgr, ctrl);
		break;
	case V4L2_CID_IS_DEBUG_SYNC_LOG:
		is_logsync(device->interface, ctrl->value, IS_MSG_TEST_SYNC_LOG);
		break;
	case V4L2_CID_HFLIP:
		if (ctrl->value)
			set_bit(SCALER_FLIP_COMMAND_X_MIRROR, &queue->framecfg.flip);
		else
			clear_bit(SCALER_FLIP_COMMAND_X_MIRROR, &queue->framecfg.flip);
		break;
	case V4L2_CID_VFLIP:
		if (ctrl->value)
			set_bit(SCALER_FLIP_COMMAND_Y_MIRROR, &queue->framecfg.flip);
		else
			clear_bit(SCALER_FLIP_COMMAND_Y_MIRROR, &queue->framecfg.flip);
		break;
	case V4L2_CID_IS_INTENT:
		head = GET_HEAD_GROUP_IN_DEVICE(IS_DEVICE_ISCHAIN, &device->group_3aa);

		head->intent_ctl.captureIntent = ctrl->value;
		mvinfo(" s_ctrl intent(%d)\n", vctx, video, ctrl->value);
		break;
	case V4L2_CID_IS_CAMERA_TYPE:
		switch (ctrl->value) {
		case IS_COLD_BOOT:
			/* change value to X when !TWIZ | front */
			is_itf_fwboot_init(device->interface);
			break;
		case IS_WARM_BOOT:
			/* change value to X when TWIZ & back | frist time back camera */
			if (!test_bit(IS_IF_LAUNCH_FIRST, &device->interface->launch_state))
				device->interface->fw_boot_mode = FIRST_LAUNCHING;
			else
				device->interface->fw_boot_mode = WARM_BOOT;
			break;
		default:
			mverr("unsupported ioctl(0x%X)", vctx, video, ctrl->id);
			ret = -EINVAL;
			break;
		}
		break;
	case V4L2_CID_IS_OPENING_HINT:
	{
		struct is_core *core;
		core = (struct is_core *)dev_get_drvdata(is_dev);
		if (core) {
			mvinfo("opening hint(%d)\n", device, video, ctrl->value);
			core->vender.opening_hint = ctrl->value;
		}
		break;
	}
	case V4L2_CID_IS_CLOSING_HINT:
	{
		struct is_core *core;
		core = (struct is_core *)dev_get_drvdata(is_dev);
		if (core) {
			mvinfo("closing hint(%d)\n", device, video, ctrl->value);
			core->vender.closing_hint = ctrl->value;
		}
		break;
	}
	case VENDER_S_CTRL:
		/* This s_ctrl is needed to skip, when the s_ctrl id was found. */
		break;
	case V4L2_CID_IS_S_USE_EXT_PLANE:
		if (ctrl->value)
			set_bit(IS_QUEUE_NEED_TO_EXTMAP, &queue->state);
		else
			clear_bit(IS_QUEUE_NEED_TO_EXTMAP, &queue->state);
		mvinfo("[%s]use ext_plane:%d\n", vctx, video, queue->name,
				ctrl->value);
		break;
	case V4L2_CID_IS_S_LLC_CONFIG:
		device->llc_mode = ctrl->value;
		break;
	default:
		mverr("unsupported ioctl(0x%X)", vctx, video, ctrl->id);
		ret = -EINVAL;
		break;
	}

p_err:
	return ret;
}

int is_video_buffer_done(struct is_video_ctx *vctx,
	u32 index, u32 state)
{
	int ret = 0;
	struct vb2_buffer *vb;
	struct is_queue *queue;
	struct is_video *video;

	FIMC_BUG(!vctx);
	FIMC_BUG(!vctx->video);
	FIMC_BUG(index >= IS_MAX_BUFS);

	queue = GET_QUEUE(vctx);
	video = GET_VIDEO(vctx);

	if (!queue->vbq) {
		mverr("vbq is NULL", vctx, video);
		ret = -EINVAL;
		goto p_err;
	}

	vb = queue->vbq->bufs[index];
	if (!vb) {
		mverr("vb is NULL", vctx, video);
		ret = -EINVAL;
		goto p_err;
	}

	if (!test_bit(IS_QUEUE_STREAM_ON, &queue->state)) {
		warn("%d video queue is not stream on", vctx->video->id);
		ret = -EINVAL;
		goto p_err;
	}

	if (vb->state != VB2_BUF_STATE_ACTIVE && vb->state != VB2_BUF_STATE_ERROR) {
		mverr("vb buffer[%d] state is not active(%d)", vctx, video, index, vb->state);
		ret = -EINVAL;
		goto p_err;
	}

	queue->buf_com++;

	vb2_buffer_done(vb, state);

p_err:
	return ret;
}
