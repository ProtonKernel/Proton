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

#ifndef IS_VIDEO_H
#define IS_VIDEO_H

#include <linux/version.h>
#include <media/v4l2-ioctl.h>
#include "is-type.h"
#include "is-mem.h"
#include "is-framemgr.h"
#include "is-metadata.h"
#include "is-config.h"

#define NUM_OF_META_PLANE      1
#if defined(META_ITF_VER_20192002)
#define SIZE_OF_META_PLANE     SZ_32K
#else
#define SIZE_OF_META_PLANE     (SZ_32K + SZ_16K + SZ_8K) /* 56KB */
#endif

/* configuration by linux kernel version */

/*
 * sensor scenario (26 ~ 31 bit)
 */
#define SENSOR_SCN_MASK			0xFC000000
#define SENSOR_SCN_SHIFT			26

/*
 * stream type
 * [0] : No reprocessing type
 * [1] : reprocessing type
 * [2] : offline reprocessing type
 */
#define INPUT_STREAM_MASK			0x03000000
#define INPUT_STREAM_SHIFT			24

/*
 * senosr position
 * [0] : rear
 * [1] : front
 * [2] : rear2
 * [3] : secure
 */
#define INPUT_POSITION_MASK			0x00FF0000
#define INPUT_POSITION_SHIFT			16

/*
 * video index
 * [x] : connected capture video node index
 */
#define INPUT_VINDEX_MASK			0x0000FF00
#define INPUT_VINDEX_SHIFT			8

/*
 * input type
 * [0] : memory input
 * [1] : on the fly input
 * [2] : pipe input
 */
#define INPUT_INTYPE_MASK			0x000000F0
#define INPUT_INTYPE_SHIFT			4

/*
 * stream leader
 * [0] : No stream leader
 * [1] : stream leader
 */
#define INPUT_LEADER_MASK			0x0000000F
#define INPUT_LEADER_SHIFT			0

#define IS_MAX_NODES			IS_STREAM_COUNT
#define IS_INVALID_BUF_INDEX		0xFF

#define VIDEO_SSX_READY_BUFFERS			0
#define VIDEO_3XS_READY_BUFFERS			0
#define VIDEO_3XC_READY_BUFFERS			0
#define VIDEO_3XP_READY_BUFFERS			0
#define VIDEO_3XF_READY_BUFFERS			0
#define VIDEO_3XG_READY_BUFFERS			0
#define VIDEO_3XO_READY_BUFFERS			0
#define VIDEO_3XL_READY_BUFFERS			0
#define VIDEO_LME_READY_BUFFERS			0
#define VIDEO_LMEXS_READY_BUFFERS			0
#define VIDEO_LMEXC_READY_BUFFERS			0
#define VIDEO_IXS_READY_BUFFERS			0
#define VIDEO_IXC_READY_BUFFERS			0
#define VIDEO_IXP_READY_BUFFERS			0
#define VIDEO_IXT_READY_BUFFERS			0
#define VIDEO_IXG_READY_BUFFERS			0
#define VIDEO_IXV_READY_BUFFERS			0
#define VIDEO_IXW_READY_BUFFERS			0
#define VIDEO_MEXC_READY_BUFFERS			0
#define VIDEO_ORBXC_READY_BUFFERS			0
#define VIDEO_MXS_READY_BUFFERS			0
#define VIDEO_MXP_READY_BUFFERS			0
#define VIDEO_VRA_READY_BUFFERS			0
#define VIDEO_SSXVC0_READY_BUFFERS		0
#define VIDEO_SSXVC1_READY_BUFFERS		0
#define VIDEO_SSXVC2_READY_BUFFERS		0
#define VIDEO_SSXVC3_READY_BUFFERS		0
#define VIDEO_PAFXS_READY_BUFFERS		0
#define VIDEO_CLHXS_READY_BUFFERS		0
#define VIDEO_CLHXC_READY_BUFFERS		0
#define VIDEO_YPP_READY_BUFFERS			0

#define EXYNOS_VIDEONODE_FIMC_IS	(100)
#define IS_VIDEO_NAME(name)		("exynos-is-"name)
#define IS_VIDEO_SSX_NAME			IS_VIDEO_NAME("ss")
#define IS_VIDEO_PRE_NAME			IS_VIDEO_NAME("pre")
#define IS_VIDEO_3XS_NAME(id)		IS_VIDEO_NAME("3"#id"s")
#define IS_VIDEO_3XC_NAME(id)		IS_VIDEO_NAME("3"#id"c")
#define IS_VIDEO_3XP_NAME(id)		IS_VIDEO_NAME("3"#id"p")
#define IS_VIDEO_3XF_NAME(id)		IS_VIDEO_NAME("3"#id"f")
#define IS_VIDEO_3XG_NAME(id)		IS_VIDEO_NAME("3"#id"g")
#define IS_VIDEO_3XO_NAME(id)		IS_VIDEO_NAME("3"#id"o")
#define IS_VIDEO_3XL_NAME(id)		IS_VIDEO_NAME("3"#id"l")
#define IS_VIDEO_IXS_NAME(id)		IS_VIDEO_NAME("i"#id"s")
#define IS_VIDEO_IXC_NAME(id)		IS_VIDEO_NAME("i"#id"c")
#define IS_VIDEO_IXP_NAME(id)		IS_VIDEO_NAME("i"#id"p")
#define IS_VIDEO_IXT_NAME(id)		IS_VIDEO_NAME("i"#id"t")
#define IS_VIDEO_IXW_NAME(id)		IS_VIDEO_NAME("i"#id"w")
#define IS_VIDEO_IXG_NAME(id)		IS_VIDEO_NAME("i"#id"g")
#define IS_VIDEO_IXV_NAME(id)		IS_VIDEO_NAME("i"#id"v")
#define IS_VIDEO_MEXC_NAME(id)		IS_VIDEO_NAME("me"#id"c")
#define IS_VIDEO_ORBXC_NAME(id)		IS_VIDEO_NAME("orb"#id"c")
#define IS_VIDEO_MXS_NAME(id)		IS_VIDEO_NAME("m"#id"s")
#define IS_VIDEO_MXP_NAME(id)		IS_VIDEO_NAME("m"#id"p")
#define IS_VIDEO_VRA_NAME			IS_VIDEO_NAME("vra")
#define IS_VIDEO_SSXVC0_NAME(id)		IS_VIDEO_NAME("ss"#id"vc0")
#define IS_VIDEO_SSXVC1_NAME(id)		IS_VIDEO_NAME("ss"#id"vc1")
#define IS_VIDEO_SSXVC2_NAME(id)		IS_VIDEO_NAME("ss"#id"vc2")
#define IS_VIDEO_SSXVC3_NAME(id)		IS_VIDEO_NAME("ss"#id"vc3")
#define IS_VIDEO_PAFXS_NAME(id)		IS_VIDEO_NAME("p"#id"s")
#define IS_VIDEO_CLHXS_NAME(id)		IS_VIDEO_NAME("cl"#id"s")
#define IS_VIDEO_CLHXC_NAME(id)		IS_VIDEO_NAME("cl"#id"c")
#define IS_VIDEO_YPP_NAME			IS_VIDEO_NAME("ypp")
#define IS_VIDEO_LME_NAME(id)			IS_VIDEO_NAME("lme"#id)
#define IS_VIDEO_LMEXS_NAME(id)			IS_VIDEO_NAME("lme"#id"s")
#define IS_VIDEO_LMEXC_NAME(id)			IS_VIDEO_NAME("lme"#id"c")

/* from kernel 4.20 version, some vb2 API parameter has changed */
#if (LINUX_VERSION_CODE >= KERNEL_VERSION(4, 20, 0))
#define is_vb2_qbuf(vbq, buf)			vb2_qbuf(vbq, NULL, buf)
#define is_vb2_prepare_buf(vbq, buf)		vb2_prepare_buf(vbq, NULL, buf)
#define is_fill_vb2_buffer(vb, buf, planes)	fill_vb2_buffer(vb, planes)
#else
#define is_vb2_qbuf(vbq, buf)			vb2_qbuf(vbq, buf)
#define is_vb2_prepare_buf(vbq, buf)		vb2_prepare_buf(vbq, buf)
#define is_fill_vb2_buffer(vb, buf, planes)	fill_vb2_buffer(vb, buf, planes)
#endif

#define VIDEO_OUTPUT_DEVICE_CAPS	(V4L2_CAP_STREAMING | V4L2_CAP_VIDEO_OUTPUT | V4L2_CAP_VIDEO_OUTPUT_MPLANE)
#define VIDEO_CAPTURE_DEVICE_CAPS	(V4L2_CAP_STREAMING | V4L2_CAP_VIDEO_CAPTURE | V4L2_CAP_VIDEO_CAPTURE_MPLANE)

struct is_device_ischain;
struct is_subdev;
struct is_queue;
struct is_video_ctx;
struct is_resourcemgr;

/* sysfs variable for debug */
extern struct is_sysfs_debug sysfs_debug;

enum is_video_dev_num {
	IS_VIDEO_SS0_NUM = 1,
	IS_VIDEO_SS1_NUM,
	IS_VIDEO_SS2_NUM,
	IS_VIDEO_SS3_NUM,
	IS_VIDEO_SS4_NUM,
	IS_VIDEO_SS5_NUM,
	IS_VIDEO_BNS_NUM,
	IS_VIDEO_PRE_NUM = 9,
	IS_VIDEO_30S_NUM = 10,
	IS_VIDEO_30C_NUM,
	IS_VIDEO_30P_NUM,
	IS_VIDEO_30F_NUM,
	IS_VIDEO_30G_NUM,
	IS_VIDEO_30O_NUM,	/* ORB DS WDMA */
	IS_VIDEO_30L_NUM,	/* LME DS WDMA */
	IS_VIDEO_32O_NUM,	/* ORB DS WDMA */
	IS_VIDEO_32L_NUM,	/* LME DS WDMA */
	IS_VIDEO_31S_NUM = 20,
	IS_VIDEO_31C_NUM,
	IS_VIDEO_31P_NUM,
	IS_VIDEO_31F_NUM,
	IS_VIDEO_31G_NUM,
	IS_VIDEO_31O_NUM,	/* ORB DS WDMA */
	IS_VIDEO_31L_NUM,	/* LME DS WDMA */
	IS_VIDEO_33O_NUM,	/* ORB DS WDMA */
	IS_VIDEO_33L_NUM,	/* LME DS WDMA */
	IS_VIDEO_I0S_NUM = 30,
	IS_VIDEO_I0C_NUM,
	IS_VIDEO_I0P_NUM,
	IS_VIDEO_I0V_NUM,	/* MCFP/TNR PREV BAYER WDMA */
	IS_VIDEO_I0W_NUM,	/* MCFP/TNR PREV WEIGHT WDMA */
	IS_VIDEO_I0T_NUM,	/* MCFP/TNR PREV BAYER RDMA */
	IS_VIDEO_I0G_NUM,	/* MCFP/TNR PREV WEIGHT RDMA */
	IS_VIDEO_I1S_NUM = 40,
	IS_VIDEO_I1C_NUM,
	IS_VIDEO_I1P_NUM,
	IS_VIDEO_I1T_NUM,
	IS_VIDEO_ME0C_NUM = 48,
	IS_VIDEO_ME1C_NUM = 49,
	/* 55 : this section was reserved by GDC */
	IS_VIDEO_M0S_NUM = 60,
	IS_VIDEO_M1S_NUM,
	IS_VIDEO_M0P_NUM = 70,
	IS_VIDEO_M1P_NUM,
	IS_VIDEO_M2P_NUM,
	IS_VIDEO_M3P_NUM,
	IS_VIDEO_M4P_NUM,
	IS_VIDEO_M5P_NUM,
	IS_VIDEO_VRA_NUM = 80,
	IS_VIDEO_YPP_NUM = 81,
	IS_VIDEO_LME0_NUM = 85,
	IS_VIDEO_LME0S_NUM,
	IS_VIDEO_LME0C_NUM,	/* LME MOTION HW OUT(pure motion) */
	IS_VIDEO_LME1_NUM = 88,
	IS_VIDEO_LME1S_NUM,
	IS_VIDEO_LME1C_NUM,	/* LME MOTION HW OUT(pure motion) */
	IS_VIDEO_CLH0S_NUM = 94,
	IS_VIDEO_CLH0C_NUM,
	IS_VIDEO_ORB0C_NUM,
	IS_VIDEO_ORB1C_NUM,
	/* 100~109 : this section was reserved by jpeg etc. */
	IS_VIDEO_SS0VC0_NUM = 110,
	IS_VIDEO_SS0VC1_NUM,
	IS_VIDEO_SS0VC2_NUM,
	IS_VIDEO_SS0VC3_NUM,
	IS_VIDEO_SS1VC0_NUM,
	IS_VIDEO_SS1VC1_NUM,
	IS_VIDEO_SS1VC2_NUM,
	IS_VIDEO_SS1VC3_NUM,
	IS_VIDEO_SS2VC0_NUM,
	IS_VIDEO_SS2VC1_NUM,
	IS_VIDEO_SS2VC2_NUM = 120,
	IS_VIDEO_SS2VC3_NUM,
	IS_VIDEO_SS3VC0_NUM,
	IS_VIDEO_SS3VC1_NUM,
	IS_VIDEO_SS3VC2_NUM,
	IS_VIDEO_SS3VC3_NUM,
	IS_VIDEO_SS4VC0_NUM,
	IS_VIDEO_SS4VC1_NUM,
	IS_VIDEO_SS4VC2_NUM,
	IS_VIDEO_SS4VC3_NUM,
	IS_VIDEO_SS5VC0_NUM = 130,
	IS_VIDEO_SS5VC1_NUM,
	IS_VIDEO_SS5VC2_NUM,
	IS_VIDEO_SS5VC3_NUM,
	/* 134~135 : reserved */
	IS_VIDEO_PAF0S_NUM = 140,
	IS_VIDEO_PAF1S_NUM,
	IS_VIDEO_PAF2S_NUM,
	IS_VIDEO_PAF3S_NUM,
	IS_VIDEO_32S_NUM = 145,
	IS_VIDEO_32C_NUM,
	IS_VIDEO_32P_NUM,
	IS_VIDEO_32F_NUM,
	IS_VIDEO_32G_NUM,
	IS_VIDEO_33S_NUM = 150,
	IS_VIDEO_33C_NUM,
	IS_VIDEO_33P_NUM,
	IS_VIDEO_33F_NUM,
	IS_VIDEO_33G_NUM = 154,
	/* Logical video node */
	IS_VIDEO_LVN_BASE = 155,
	IS_VIDEO_30D_NUM = IS_VIDEO_LVN_BASE,	/* DRC GRID WDMA*/
	IS_VIDEO_31D_NUM,
	IS_VIDEO_32D_NUM,
	IS_VIDEO_33D_NUM,
	IS_VIDEO_30H_NUM,	/* HF WDMA */
	IS_VIDEO_31H_NUM = 160,
	IS_VIDEO_32H_NUM,
	IS_VIDEO_33H_NUM,
	IS_VIDEO_IMM_NUM,	/* MCFP MOTION RDMA */
	IS_VIDEO_IRG_NUM,	/* DNS RGB RDMA */
	IS_VIDEO_ISC_NUM,	/* DNS SEGMENT CONFIDENCE MAP */
	IS_VIDEO_IDR_NUM,	/* DNS DRC GRID RDMA */
	IS_VIDEO_INR_NUM,	/* DNS NOISE RDMA */
	IS_VIDEO_IND_NUM,	/* DNS NR DS WDMA */
	IS_VIDEO_IDG_NUM,	/* DNS DRC GAIN WDMA */
	IS_VIDEO_ISH_NUM = 170,	/* DNS SV HIST WDMA */
	IS_VIDEO_IHF_NUM,	/* DNS HF WDMA */
	IS_VIDEO_INW_NUM,	/* DNS NOISE WDMA */
	IS_VIDEO_INRW_NUM,	/* DNS NOISE REP WDMA */
	IS_VIDEO_IRGW_NUM,	/* DNS RGB WDMA */
	IS_VIDEO_INB_NUM = 175,	/* DNS NR BAYER */
	IS_VIDEO_YND_NUM,	/* YUVPP NR DS RDMA */
	IS_VIDEO_YDG_NUM,	/* YUVPP DRC GAIN RDMA */
	IS_VIDEO_YSH_NUM,	/* YUVPP SV HIST RDMA */
	IS_VIDEO_YNS_NUM,	/* YUVPP NOISE RDMA */
	IS_VIDEO_LME0M_NUM = 180,	/* LME MOTION SW OUT(processed motion) */
	IS_VIDEO_LME1M_NUM = 181,	/* LME MOTION SW OUT(processed motion) */
	IS_VIDEO_MAX_NUM
};

static const char * const vn_name[IS_VIDEO_MAX_NUM + 1] = {
	[IS_VIDEO_SS0_NUM] = "V:SS0",
	[IS_VIDEO_SS1_NUM] = "V:SS1",
	[IS_VIDEO_SS2_NUM] = "V:SS2",
	[IS_VIDEO_SS3_NUM] = "V:SS3",
	[IS_VIDEO_SS4_NUM] = "V:SS4",
	[IS_VIDEO_SS5_NUM] = "V:SS5",
	[IS_VIDEO_BNS_NUM] = "V:BNS",
	[IS_VIDEO_PRE_NUM] = "V:PRE",
	[IS_VIDEO_30S_NUM] = "V:30S",
	[IS_VIDEO_30C_NUM] = "V:30C",
	[IS_VIDEO_30P_NUM] = "V:30P",
	[IS_VIDEO_30F_NUM] = "V:30F",
	[IS_VIDEO_30G_NUM] = "V:30G",
	[IS_VIDEO_30O_NUM] = "V:30O",
	[IS_VIDEO_30L_NUM] = "V:30L",
	[IS_VIDEO_32O_NUM] = "V:32O",
	[IS_VIDEO_32L_NUM] = "V:32L",
	[IS_VIDEO_31S_NUM] = "V:31S",
	[IS_VIDEO_31C_NUM] = "V:31C",
	[IS_VIDEO_31P_NUM] = "V:31P",
	[IS_VIDEO_31F_NUM] = "V:31F",
	[IS_VIDEO_31G_NUM] = "V:31G",
	[IS_VIDEO_31O_NUM] = "V:31O",
	[IS_VIDEO_31L_NUM] = "V:31L",
	[IS_VIDEO_33O_NUM] = "V:33O",
	[IS_VIDEO_33L_NUM] = "V:33L",

	[IS_VIDEO_I0S_NUM] = "V:I0S",
	[IS_VIDEO_I0C_NUM] = "V:I0C",
	[IS_VIDEO_I0P_NUM] = "V:I0P",
	[IS_VIDEO_I0V_NUM] = "V:TBW",
	[IS_VIDEO_I0W_NUM] = "V:TWW",
	[IS_VIDEO_I0T_NUM] = "V:TBR",
	[IS_VIDEO_I0G_NUM] = "V:TWR",
	[IS_VIDEO_ME0C_NUM] = "V:M0C",
	[IS_VIDEO_ME1C_NUM] = "V:M1C",
	[IS_VIDEO_M0S_NUM] = "V:M0S",
	[IS_VIDEO_M1S_NUM] = "V:M1S",
	[IS_VIDEO_M0P_NUM] = "V:M0P",
	[IS_VIDEO_M1P_NUM] = "V:M1P",
	[IS_VIDEO_M2P_NUM] = "V:M2P",
	[IS_VIDEO_M3P_NUM] = "V:M3P",
	[IS_VIDEO_M4P_NUM] = "V:M4P",
	[IS_VIDEO_M5P_NUM] = "V:M5P",

	[IS_VIDEO_VRA_NUM] = "V:VRA",
	[IS_VIDEO_YPP_NUM] = "V:YPP",
	[IS_VIDEO_LME0_NUM] = "V:LME0",
	[IS_VIDEO_LME0S_NUM] = "V:LE0S",
	[IS_VIDEO_LME0C_NUM] = "V:LE0C",
	[IS_VIDEO_LME1_NUM] = "V:LME1",
	[IS_VIDEO_LME1S_NUM] = "V:LE1S",
	[IS_VIDEO_LME1C_NUM] = "V:LE1C",
	[IS_VIDEO_CLH0S_NUM] = "V:CLS",
	[IS_VIDEO_CLH0C_NUM] = "V:CLC",
	[IS_VIDEO_ORB0C_NUM] = "V:O0C",
	[IS_VIDEO_ORB1C_NUM] = "V:O1C",

	[IS_VIDEO_SS0VC0_NUM] = "V:S0VC0",
	[IS_VIDEO_SS0VC1_NUM] = "V:S0VC1",
	[IS_VIDEO_SS0VC2_NUM] = "V:S0VC2",
	[IS_VIDEO_SS0VC3_NUM] = "V:S0VC3",
	[IS_VIDEO_SS1VC0_NUM] = "V:S1VC0",
	[IS_VIDEO_SS1VC1_NUM] = "V:S1VC1",
	[IS_VIDEO_SS1VC2_NUM] = "V:S1VC2",
	[IS_VIDEO_SS1VC3_NUM] = "V:S1VC3",
	[IS_VIDEO_SS2VC0_NUM] = "V:S2VC0",
	[IS_VIDEO_SS2VC1_NUM] = "V:S2VC1",
	[IS_VIDEO_SS2VC2_NUM] = "V:S2VC2",
	[IS_VIDEO_SS2VC3_NUM] = "V:S2VC3",
	[IS_VIDEO_SS3VC0_NUM] = "V:S3VC0",
	[IS_VIDEO_SS3VC1_NUM] = "V:S3VC1",
	[IS_VIDEO_SS3VC2_NUM] = "V:S3VC2",
	[IS_VIDEO_SS3VC3_NUM] = "V:S3VC3",
	[IS_VIDEO_SS4VC0_NUM] = "V:S4VC0",
	[IS_VIDEO_SS4VC1_NUM] = "V:S4VC1",
	[IS_VIDEO_SS4VC2_NUM] = "V:S4VC2",
	[IS_VIDEO_SS4VC3_NUM] = "V:S4VC3",
	[IS_VIDEO_SS5VC0_NUM] = "V:S5VC0",
	[IS_VIDEO_SS5VC1_NUM] = "V:S5VC1",
	[IS_VIDEO_SS5VC2_NUM] = "V:S5VC2",
	[IS_VIDEO_SS5VC3_NUM] = "V:S5VC3",

	[IS_VIDEO_PAF0S_NUM] = "V:PDP0",
	[IS_VIDEO_PAF1S_NUM] = "V:PDP1",
	[IS_VIDEO_PAF2S_NUM] = "V:PDP2",
	[IS_VIDEO_PAF3S_NUM] = "V:PDP3",

	[IS_VIDEO_32S_NUM] = "V:32S",
	[IS_VIDEO_32C_NUM] = "V:32C",
	[IS_VIDEO_32P_NUM] = "V:32P",
	[IS_VIDEO_32F_NUM] = "V:32F",
	[IS_VIDEO_32G_NUM] = "V:32G",
	[IS_VIDEO_33S_NUM] = "V:33S",
	[IS_VIDEO_33C_NUM] = "V:33C",
	[IS_VIDEO_33P_NUM] = "V:33P",
	[IS_VIDEO_33F_NUM] = "V:33F",
	[IS_VIDEO_33G_NUM] = "V:33G",

	[IS_VIDEO_30D_NUM] = "V:30D",
	[IS_VIDEO_31D_NUM] = "V:31D",
	[IS_VIDEO_32D_NUM] = "V:32D",
	[IS_VIDEO_33D_NUM] = "V:33D",
	[IS_VIDEO_30H_NUM] = "V:30H",
	[IS_VIDEO_31H_NUM] = "V:31H",
	[IS_VIDEO_32H_NUM] = "V:32H",
	[IS_VIDEO_33H_NUM] = "V:33H",

	[IS_VIDEO_IMM_NUM] = "V:IMTR",
	[IS_VIDEO_IRG_NUM] = "V:IRGBR",
	[IS_VIDEO_ISC_NUM] = "V:ISEGR",
	[IS_VIDEO_IDR_NUM] = "V:IDGR",
	[IS_VIDEO_INR_NUM] = "V:INOR",
	[IS_VIDEO_IND_NUM] = "V:INDS",
	[IS_VIDEO_IDG_NUM] = "V:IDRG",
	[IS_VIDEO_ISH_NUM] = "V:ISVH",
	[IS_VIDEO_IHF_NUM] = "V:IHF",
	[IS_VIDEO_INW_NUM] = "V:INOW",
	[IS_VIDEO_INRW_NUM] = "V:INOWR",
	[IS_VIDEO_IRGW_NUM] = "V:IRGB",
	[IS_VIDEO_INB_NUM] = "V:INRB",

	[IS_VIDEO_YND_NUM] = "V:YNRD",
	[IS_VIDEO_YDG_NUM] = "V:YDRG",
	[IS_VIDEO_YSH_NUM] = "V:YSVH",
	[IS_VIDEO_YNS_NUM] = "V:YNOI",

	[IS_VIDEO_LME0M_NUM] = "V:LE0M",
	[IS_VIDEO_LME1M_NUM] = "V:LE1M",

	[IS_VIDEO_MAX_NUM] = "V:MAX"
};

enum is_video_type {
	IS_VIDEO_TYPE_LEADER,
	IS_VIDEO_TYPE_CAPTURE,
};

enum is_video_state {
	IS_VIDEO_CLOSE,
	IS_VIDEO_OPEN,
	IS_VIDEO_S_INPUT,
	IS_VIDEO_S_FORMAT,
	IS_VIDEO_S_BUFS,
	IS_VIDEO_STOP,
	IS_VIDEO_START,
};

enum is_queue_state {
	IS_QUEUE_BUFFER_PREPARED,
	IS_QUEUE_BUFFER_READY,
	IS_QUEUE_STREAM_ON,
	IS_QUEUE_NEED_TO_REMAP,	/* need remapped DVA with specific attribute */
	IS_QUEUE_NEED_TO_KMAP,	/* need permanent KVA for image planes */
	IS_QUEUE_NEED_TO_EXTMAP,	/* need ext plane for thumbnai, histgram */
};

struct is_frame_cfg {
	struct is_fmt		*format;
	enum v4l2_colorspace		colorspace;
	enum v4l2_quantization		quantization;
	ulong				flip;
	u32				width;
	u32				height;
	u32				hw_pixeltype;
	u32				size[IS_MAX_PLANES];
	u32				bytesperline[IS_MAX_PLANES];
};

struct is_queue_ops {
	int (*start_streaming)(void *qdevice,
		struct is_queue *queue);
	int (*stop_streaming)(void *qdevice,
		struct is_queue *queue);
	int (*s_format)(void *qdevice,
		struct is_queue *queue);
	int (*request_bufs)(void *qdevice,
		struct is_queue *queue,
		u32 count);
};

struct is_video_ops {
	int (*qbuf)(struct is_video_ctx *vctx,
		struct v4l2_buffer *buf);
	int (*dqbuf)(struct is_video_ctx *vctx,
		struct v4l2_buffer *buf,
		bool blocking);
	int (*done)(struct is_video_ctx *vctx,
		u32 index, u32 state);
};

struct is_queue {
	struct vb2_queue		*vbq;
	const struct is_queue_ops	*qops;
	struct is_framemgr 	framemgr;
	struct is_frame_cfg	framecfg;

	u32				buf_maxcount;
	u32				buf_rdycount;
	u32				buf_refcount;
	dma_addr_t			buf_dva[IS_MAX_BUFS][IS_MAX_PLANES];
	ulong				buf_kva[IS_MAX_BUFS][IS_MAX_PLANES];

	/* for debugging */
	u32				buf_req;
	u32				buf_pre;
	u32				buf_que;
	u32				buf_com;
	u32				buf_dqe;

	u32				id;
	char				name[IS_STR_LEN];
	unsigned long			state;

#ifdef ENABLE_LOGICAL_VIDEO_NODE
	/* for logical video node */
	u32				mode;
	struct is_sub_buf		in_buf[IS_MAX_BUFS];
	struct is_sub_buf		out_buf[IS_MAX_BUFS];
#endif
};

struct is_video_ctx {
	u32				instance; /* logical stream id */

	struct is_queue			queue;
	u32				refcount;
	unsigned long			state;

	void				*device;
	void				*next_device;
	void				*subdev;
	struct is_video			*video;

	const struct vb2_ops		*vb2_ops;
	const struct vb2_mem_ops	*vb2_mem_ops;
	const struct is_vb2_buf_ops	*is_vb2_buf_ops;
	struct is_video_ops		vops;

#if defined(MEASURE_TIME) && defined(MONITOR_TIME)
	unsigned long long		time[TMQ_END];
	unsigned long long		time_total[TMQ_END];
	unsigned long			time_cnt;
#endif
};

struct is_video {
	u32				id;
	enum is_video_type 	type;
	atomic_t			refcount;
	struct mutex			lock;

	struct video_device		vd;
	struct is_resourcemgr	*resourcemgr;
	const struct vb2_mem_ops	*vb2_mem_ops;
	const struct is_vb2_buf_ops *is_vb2_buf_ops;
	void				*alloc_ctx;

	struct semaphore		smp_multi_input;
	bool				try_smp;
};

/* video context operation */
int open_vctx(struct file *file,
	struct is_video *video,
	struct is_video_ctx **vctx,
	u32 instance,
	ulong id,
	const char *name);
int close_vctx(struct file *file,
	struct is_video *video,
	struct is_video_ctx *vctx);

/* queue operation */
int is_queue_setup(struct is_queue *queue,
	void *alloc_ctx,
	unsigned int *num_planes,
	unsigned int sizes[],
	struct device *alloc_devs[]);
int is_queue_buffer_queue(struct is_queue *queue,
	struct vb2_buffer *vb);
int is_queue_buffer_init(struct vb2_buffer *vb);
void is_queue_buffer_cleanup(struct vb2_buffer *vb);
int is_queue_buffer_prepare(struct vb2_buffer *vb);
void is_queue_wait_prepare(struct vb2_queue *vbq);
void is_queue_wait_finish(struct vb2_queue *vbq);
int is_queue_start_streaming(struct is_queue *queue,
	void *qdevice);
int is_queue_stop_streaming(struct is_queue *queue,
	void *qdevice);
void is_queue_buffer_finish(struct vb2_buffer *vb);

/* video operation */
int is_video_probe(struct is_video *video,
	char *video_name,
	u32 video_number,
	u32 vfl_dir,
	struct is_mem *mem,
	struct v4l2_device *v4l2_dev,
	const struct v4l2_file_operations *fops,
	const struct v4l2_ioctl_ops *ioctl_ops);
int is_video_open(struct is_video_ctx *vctx,
	void *device,
	u32 buf_rdycount,
	struct is_video *video,
	const struct vb2_ops *vb2_ops,
	const struct is_queue_ops *qops);
int is_video_close(struct is_video_ctx *vctx);
int is_video_s_input(struct file *file,
	struct is_video_ctx *vctx);
int is_video_poll(struct file *file,
	struct is_video_ctx *vctx,
	struct poll_table_struct *wait);
int is_video_mmap(struct file *file,
	struct is_video_ctx *vctx,
	struct vm_area_struct *vma);
int is_video_reqbufs(struct file *file,
	struct is_video_ctx *vctx,
	struct v4l2_requestbuffers *request);
int is_video_querybuf(struct file *file,
	struct is_video_ctx *vctx,
	struct v4l2_buffer *buf);
int is_video_set_format_mplane(struct file *file,
	struct is_video_ctx *vctx,
	struct v4l2_format *format);
int is_video_qbuf(struct is_video_ctx *vctx,
	struct v4l2_buffer *buf);
int is_video_dqbuf(struct is_video_ctx *vctx,
	struct v4l2_buffer *buf,
	bool blocking);
int is_video_prepare(struct file *file,
	struct is_video_ctx *vctx,
	struct v4l2_buffer *buf);
int is_video_streamon(struct file *file,
	struct is_video_ctx *vctx,
	enum v4l2_buf_type type);
int is_video_streamoff(struct file *file,
	struct is_video_ctx *vctx,
	enum v4l2_buf_type type);
int is_video_s_ctrl(struct file *file,
	struct is_video_ctx *vctx,
	struct v4l2_control *ctrl);
int is_video_buffer_done(struct is_video_ctx *vctx,
	u32 index, u32 state);

struct is_fmt *is_find_format(u32 pixelformat, u32 flags);

extern int is_pre_video_probe(void *data);
extern int is_ssx_video_probe(void *data);
extern int is_30s_video_probe(void *data);
extern int is_30c_video_probe(void *data);
extern int is_30p_video_probe(void *data);
extern int is_30f_video_probe(void *data);
extern int is_30g_video_probe(void *data);
extern int is_30o_video_probe(void *data);
extern int is_30l_video_probe(void *data);
extern int is_31s_video_probe(void *data);
extern int is_31c_video_probe(void *data);
extern int is_31p_video_probe(void *data);
extern int is_31f_video_probe(void *data);
extern int is_31g_video_probe(void *data);
extern int is_31o_video_probe(void *data);
extern int is_31l_video_probe(void *data);
extern int is_32s_video_probe(void *data);
extern int is_32c_video_probe(void *data);
extern int is_32p_video_probe(void *data);
extern int is_32f_video_probe(void *data);
extern int is_32g_video_probe(void *data);
extern int is_32o_video_probe(void *data);
extern int is_32l_video_probe(void *data);
extern int is_33s_video_probe(void *data);
extern int is_33c_video_probe(void *data);
extern int is_33p_video_probe(void *data);
extern int is_33f_video_probe(void *data);
extern int is_33g_video_probe(void *data);
extern int is_33o_video_probe(void *data);
extern int is_33l_video_probe(void *data);
extern int is_i0s_video_probe(void *data);
extern int is_i0c_video_probe(void *data);
extern int is_i0p_video_probe(void *data);
extern int is_i0t_video_probe(void *data);
extern int is_i0g_video_probe(void *data);
extern int is_i0v_video_probe(void *data);
extern int is_i0w_video_probe(void *data);
extern int is_i1s_video_probe(void *data);
extern int is_i1c_video_probe(void *data);
extern int is_i1p_video_probe(void *data);
extern int is_me0c_video_probe(void *data);
extern int is_me1c_video_probe(void *data);
extern int is_orb0c_video_probe(void *data);
extern int is_orb1c_video_probe(void *data);
extern int is_m0s_video_probe(void *data);
extern int is_m1s_video_probe(void *data);
extern int is_m0p_video_probe(void *data);
extern int is_m1p_video_probe(void *data);
extern int is_m2p_video_probe(void *data);
extern int is_m3p_video_probe(void *data);
extern int is_m4p_video_probe(void *data);
extern int is_m5p_video_probe(void *data);
extern int is_vra_video_probe(void *data);
extern int is_ssxvc0_video_probe(void *data);
extern int is_ssxvc1_video_probe(void *data);
extern int is_ssxvc2_video_probe(void *data);
extern int is_ssxvc3_video_probe(void *data);
extern int is_paf0s_video_probe(void *data);
extern int is_paf1s_video_probe(void *data);
extern int is_paf2s_video_probe(void *data);
extern int is_paf3s_video_probe(void *data);
extern int is_cl0s_video_probe(void *data);
extern int is_cl0c_video_probe(void *data);
extern int is_ypp_video_probe(void *data);
extern int is_lme0_video_probe(void *data);
extern int is_lme0s_video_probe(void *data);
extern int is_lme0c_video_probe(void *data);
extern int is_lme1_video_probe(void *data);
extern int is_lme1s_video_probe(void *data);
extern int is_lme1c_video_probe(void *data);

#define GET_VIDEO(vctx) 		(vctx ? (vctx)->video : NULL)
#define GET_QUEUE(vctx) 		(vctx ? &(vctx)->queue : NULL)
#define GET_FRAMEMGR(vctx)		(vctx ? &(vctx)->queue.framemgr : NULL)
#define GET_DEVICE(vctx)		(vctx ? (vctx)->device : NULL)
#ifdef CONFIG_USE_SENSOR_GROUP
#define GET_DEVICE_ISCHAIN(vctx)	(vctx ? (((vctx)->next_device) ? (vctx)->next_device : (vctx)->device) : NULL)
#else
#define GET_DEVICE_ISCHAIN(vctx)	GET_DEVICE(vctx)
#endif
#define CALL_QOPS(q, op, args...)	(((q)->qops->op) ? ((q)->qops->op(args)) : 0)
#define CALL_VOPS(v, op, args...)	((v) && ((v)->vops.op) ? ((v)->vops.op(v, args)) : 0)
#endif
