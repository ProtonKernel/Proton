/*
 * Copyright (c) 2018 Samsung Electronics Co., Ltd.
 *		http://www.samsung.com
 *
 * Format Header file for Exynos DPU driver
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
*/

#ifndef __EXYNOS_FORMAT_H__
#define __EXYNOS_FORMAT_H__

#define DPU_UNDEF_BITS_DEPTH		0xabcd

enum decon_pixel_format {
	/* RGB 8bit display */
	/* 4byte */
	DECON_PIXEL_FORMAT_ARGB_8888 = 0,
	DECON_PIXEL_FORMAT_ABGR_8888,
	DECON_PIXEL_FORMAT_RGBA_8888,
	DECON_PIXEL_FORMAT_BGRA_8888,
	DECON_PIXEL_FORMAT_XRGB_8888,
	DECON_PIXEL_FORMAT_XBGR_8888,
	DECON_PIXEL_FORMAT_RGBX_8888,
	DECON_PIXEL_FORMAT_BGRX_8888,
	/* 2byte */
	DECON_PIXEL_FORMAT_RGBA_5551,
	DECON_PIXEL_FORMAT_BGRA_5551,
	DECON_PIXEL_FORMAT_ABGR_4444,
	DECON_PIXEL_FORMAT_RGBA_4444,
	DECON_PIXEL_FORMAT_BGRA_4444,
	DECON_PIXEL_FORMAT_RGB_565,
	DECON_PIXEL_FORMAT_BGR_565,

	/* RGB 10bit display */
	/* 4byte */
	DECON_PIXEL_FORMAT_ARGB_2101010,
	DECON_PIXEL_FORMAT_ABGR_2101010,
	DECON_PIXEL_FORMAT_RGBA_1010102,
	DECON_PIXEL_FORMAT_BGRA_1010102,

	/* YUV 8bit display */
	/* YUV422 2P */
	DECON_PIXEL_FORMAT_NV16,
	DECON_PIXEL_FORMAT_NV61,
	/* YUV422 3P */
	DECON_PIXEL_FORMAT_YVU422_3P,
	/* YUV420 2P */
	DECON_PIXEL_FORMAT_NV12,
	DECON_PIXEL_FORMAT_NV21,
	DECON_PIXEL_FORMAT_NV12M,
	DECON_PIXEL_FORMAT_NV21M,
	/* YUV420 3P */
	DECON_PIXEL_FORMAT_YUV420,
	DECON_PIXEL_FORMAT_YVU420,
	DECON_PIXEL_FORMAT_YUV420M,
	DECON_PIXEL_FORMAT_YVU420M,
	/* YUV - 2 planes but 1 buffer */
	DECON_PIXEL_FORMAT_NV12N,
	DECON_PIXEL_FORMAT_NV12N_10B,

	/* YUV 10bit display */
	/* YUV420 2P */
	DECON_PIXEL_FORMAT_NV12M_P010,
	DECON_PIXEL_FORMAT_NV21M_P010,

	/* YUV420(P8+2) 4P */
	DECON_PIXEL_FORMAT_NV12M_S10B,
	DECON_PIXEL_FORMAT_NV21M_S10B,

	/* YUV422 2P */
	DECON_PIXEL_FORMAT_NV16M_P210,
	DECON_PIXEL_FORMAT_NV61M_P210,

	/* YUV422(P8+2) 4P */
	DECON_PIXEL_FORMAT_NV16M_S10B,
	DECON_PIXEL_FORMAT_NV61M_S10B,

	DECON_PIXEL_FORMAT_NV12_P010,

	/* formats for lossless SBWC case */
	DECON_PIXEL_FORMAT_NV12M_SBWC_8B,
	DECON_PIXEL_FORMAT_NV12M_SBWC_10B,
	DECON_PIXEL_FORMAT_NV21M_SBWC_8B,
	DECON_PIXEL_FORMAT_NV21M_SBWC_10B,
	DECON_PIXEL_FORMAT_NV12N_SBWC_8B,
	DECON_PIXEL_FORMAT_NV12N_SBWC_10B,

	/* formats for lossy SBWC case  */
	DECON_PIXEL_FORMAT_NV12M_SBWC_8B_L50,
	DECON_PIXEL_FORMAT_NV12M_SBWC_8B_L75,
	DECON_PIXEL_FORMAT_NV12N_SBWC_8B_L50,
	DECON_PIXEL_FORMAT_NV12N_SBWC_8B_L75,
	DECON_PIXEL_FORMAT_NV12M_SBWC_10B_L40,
	DECON_PIXEL_FORMAT_NV12M_SBWC_10B_L60,
	DECON_PIXEL_FORMAT_NV12M_SBWC_10B_L80,
	DECON_PIXEL_FORMAT_NV12N_SBWC_10B_L40,
	DECON_PIXEL_FORMAT_NV12N_SBWC_10B_L60,
	DECON_PIXEL_FORMAT_NV12N_SBWC_10B_L80,

	DECON_PIXEL_FORMAT_MAX,
};

enum dpu_colorspace {
	DPU_COLORSPACE_RGB,
	DPU_COLORSPACE_YUV420,
	DPU_COLORSPACE_YUV422,
};

enum dpp_comp_type {
	COMP_TYPE_NONE = 0,
	COMP_TYPE_AFBC,
	COMP_TYPE_SBWC,			/* lossless */
	COMP_TYPE_SBWCL,		/* lossy */
	COMP_TYPE_CSET,
};

struct dpu_fmt {
	const char *name;
	enum decon_pixel_format fmt;	/* user-interfaced color format */
	u32 dma_fmt;			/* applied color format to DPU_DMA(In) */
	u32 dpp_fmt;			/* applied color format to DPP(Out) */
	u8 bpp;				/* bits per pixel */
	u8 padding;			/* padding bits per pixel */
	u8 bpc;				/* bits per each color component */
	u8 num_planes;			/* plane(s) count of color format */
	u8 num_buffers;			/* number of input buffer(s) */
	u8 num_meta_planes; 		/* number of meta plane(s) */
	u8 len_alpha;			/* length of alpha bits */
	enum dpu_colorspace cs;
	enum dpp_comp_type ct;
};

/* format */
#define IS_YUV420(f)		((f)->cs == DPU_COLORSPACE_YUV420)
#define IS_YUV422(f)		((f)->cs == DPU_COLORSPACE_YUV422)
#define IS_YUV(f)	\
	(((f)->cs == DPU_COLORSPACE_YUV420) || ((f)->cs == DPU_COLORSPACE_YUV422))
#define IS_YUV10(f)		(IS_YUV(f) && ((f)->bpc == 10))
#define IS_RGB32(f)	\
	(((f)->cs == DPU_COLORSPACE_RGB) && (((f)->bpp + (f)->padding) == 32))
#define IS_OPAQUE(f)		((f)->len_alpha == 0)

const struct dpu_fmt *dpu_find_fmt_info(enum decon_pixel_format fmt);

#endif /* __EXYNOS_FORMAT_H__ */
