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

#ifndef IS_SUBDEV_H
#define IS_SUBDEV_H

#include "is-video.h"
#include "is-work.h"

#define SUBDEV_INTERNAL_BUF_MAX		(8)

struct is_device_sensor;
struct is_device_ischain;
struct is_groupmgr;
struct is_group;

enum is_subdev_device_type {
	IS_SENSOR_SUBDEV,
	IS_ISCHAIN_SUBDEV,
};

enum is_subdev_state {
	IS_SUBDEV_OPEN,
	IS_SUBDEV_START,
	IS_SUBDEV_RUN,
	IS_SUBDEV_FORCE_SET,
	IS_SUBDEV_PARAM_ERR,
	IS_SUBDEV_INTERNAL_USE,
	IS_SUBDEV_INTERNAL_S_FMT,
	IS_SUBDEV_VOTF_USE,
	IS_SUBDEV_IOVA_EXTENSION_USE,
};

struct is_subdev_path {
	u32					width;
	u32					height;
	struct is_crop			canv;
	struct is_crop			crop;
};

/* Caution: Do not exceed 64 */
enum is_subdev_id {
	ENTRY_SENSOR,
	ENTRY_SSVC0,
	ENTRY_SSVC1,
	ENTRY_SSVC2,
	ENTRY_SSVC3,
	ENTRY_3AA,
	ENTRY_3AC,
	ENTRY_3AP,
	ENTRY_3AF,
	ENTRY_3AG,
	ENTRY_3AO,
	ENTRY_3AL,
	ENTRY_ISP,
	ENTRY_IXC,
	ENTRY_IXP,
	ENTRY_IXT,
	ENTRY_IXG,
	ENTRY_IXV,
	ENTRY_IXW,
	ENTRY_MEXC, /* MEIP or MCH*/
	ENTRY_MCS,
	ENTRY_M0P,
	ENTRY_M1P,
	ENTRY_M2P,
	ENTRY_M3P,
	ENTRY_M4P,
	ENTRY_M5P,
	ENTRY_VRA,
	ENTRY_PAF, /* PDP(PATSTAT) Bayer RDMA */
	ENTRY_PDAF, /* PDP(PATSTAT) AF RDMA */
	ENTRY_PDST, /* PDP(PATSTAT) PD STAT WDMA */
	ENTRY_CLH,	/* CLAHE RDMA */
	ENTRY_CLHC,	/* CLAHE WDMA */
	ENTRY_ORBXC, /* ORB */
	ENTRY_YPP, /* YUVPP */
	ENTRY_LME,
	ENTRY_LMES,
	ENTRY_LMEC,
	ENTRY_END
};

static ulong is_subdev_wq_id[ENTRY_END] = {
	[ENTRY_SENSOR ... ENTRY_END-1] = WORK_MAX_MAP,
	[ENTRY_SENSOR] = WORK_SHOT_DONE,
	[ENTRY_SSVC0] = WORK_MAX_MAP,
	[ENTRY_SSVC1] = WORK_MAX_MAP,
	[ENTRY_SSVC2] = WORK_MAX_MAP,
	[ENTRY_SSVC3] = WORK_MAX_MAP,
	[ENTRY_3AA] = WORK_SHOT_DONE,
	[ENTRY_3AC] = WORK_30C_FDONE,
	[ENTRY_3AP] = WORK_30P_FDONE,
	[ENTRY_3AF] = WORK_30F_FDONE,
	[ENTRY_3AG] = WORK_30G_FDONE,
	[ENTRY_3AO] = WORK_30O_FDONE,
	[ENTRY_3AL] = WORK_30L_FDONE,
	[ENTRY_ISP] = WORK_SHOT_DONE,
	[ENTRY_IXC] = WORK_I0C_FDONE,
	[ENTRY_IXP] = WORK_I0P_FDONE,
	[ENTRY_IXT] = WORK_I0T_FDONE,
	[ENTRY_IXG] = WORK_I0G_FDONE,
	[ENTRY_IXV] = WORK_I0V_FDONE,
	[ENTRY_IXW] = WORK_I0W_FDONE,
	[ENTRY_MEXC] = WORK_ME0C_FDONE,
	[ENTRY_MCS] = WORK_SHOT_DONE,
	[ENTRY_M0P] = WORK_M0P_FDONE,
	[ENTRY_M1P] = WORK_M1P_FDONE,
	[ENTRY_M2P] = WORK_M2P_FDONE,
	[ENTRY_M3P] = WORK_M3P_FDONE,
	[ENTRY_M4P] = WORK_M4P_FDONE,
	[ENTRY_M5P] = WORK_M5P_FDONE,
	[ENTRY_VRA] = WORK_SHOT_DONE,
	[ENTRY_PAF] = WORK_SHOT_DONE,
	[ENTRY_PDAF] = WORK_MAX_MAP,
	[ENTRY_PDST] = WORK_MAX_MAP,
	[ENTRY_CLH] = WORK_SHOT_DONE,
	[ENTRY_CLHC] = WORK_CL0C_FDONE,
	[ENTRY_ORBXC] = WORK_ORB0C_FDONE,
	[ENTRY_YPP] = WORK_SHOT_DONE,
	[ENTRY_LME] = WORK_SHOT_DONE,
	[ENTRY_LMES] = WORK_LME0S_FDONE,
	[ENTRY_LMEC] = WORK_LME0C_FDONE,
};

struct is_subdev_ops {
	int (*bypass)(struct is_subdev *subdev,
		void *device_data,
		struct is_frame *frame,
		bool bypass);
	int (*cfg)(struct is_subdev *subdev,
		void *device_data,
		struct is_frame *frame,
		struct is_crop *incrop,
		struct is_crop *otcrop,
		u32 *lindex,
		u32 *hindex,
		u32 *indexes);
	int (*tag)(struct is_subdev *subdev,
		void *device_data,
		struct is_frame *frame,
		struct camera2_node *node);
};

enum subdev_ch_mode {
	SCM_WO_PAF_HW,
	SCM_W_PAF_HW,
	SCM_MAX,
};

struct is_subdev {
	u32					id;
	u32					vid; /* video id */
	u32					cid; /* capture node id */
	enum chain_work_map			wq_id; /* workqueue id */
	char					name[4];
	u32					instance;
	unsigned long				state;

	u32					constraints_width; /* spec in width */
	u32					constraints_height; /* spec in height */

	u32					param_otf_in;
	u32					param_dma_in;
	u32					param_otf_ot;
	u32					param_dma_ot;

	struct is_subdev_path		input;
	struct is_subdev_path		output;

	struct list_head			list;

	/* for internal use */
	struct is_framemgr			internal_framemgr;
	u32					batch_num;
	u32					buffer_num;
	u32					bits_per_pixel;
	struct is_priv_buf			*pb_subdev[SUBDEV_INTERNAL_BUF_MAX];
#ifdef ENABLE_LOGICAL_VIDEO_NODE
	struct is_priv_buf			*pb_capture_subdev[SUBDEV_INTERNAL_BUF_MAX][CAPTURE_NODE_MAX];
#endif
	bool					use_shared_framemgr;
	char					data_type[15];

	struct is_video_ctx		*vctx;
	struct is_subdev			*leader;
	const struct is_subdev_ops		*ops;
};

int is_sensor_subdev_open(struct is_device_sensor *device,
	struct is_video_ctx *vctx);
int is_sensor_subdev_close(struct is_device_sensor *device,
	struct is_video_ctx *vctx);

int is_ischain_subdev_open(struct is_device_ischain *device,
	struct is_video_ctx *vctx);
int is_ischain_subdev_close(struct is_device_ischain *device,
	struct is_video_ctx *vctx);

/*common subdev*/
int is_subdev_probe(struct is_subdev *subdev,
	u32 instance,
	u32 id,
	char *name,
	const struct is_subdev_ops *sops);
int is_subdev_open(struct is_subdev *subdev,
	struct is_video_ctx *vctx,
	void *ctl_data,
	u32 instance);
int is_subdev_close(struct is_subdev *subdev);
int is_subdev_buffer_queue(struct is_subdev *subdev, struct vb2_buffer *vb);
int is_subdev_buffer_finish(struct is_subdev *subdev, struct vb2_buffer *vb);

#if defined(SOC_TDNR)
void is_subdev_dnr_start(struct is_device_ischain *device,
	struct is_frame *frame, u32 *lindex, u32 *hindex, u32 *indexes);
void is_subdev_dnr_stop(struct is_device_ischain *device,
	struct is_frame *frame, u32 *lindex, u32 *hindex, u32 *indexes);
void is_subdev_dnr_bypass(struct is_device_ischain *device,
	struct is_frame *frame, u32 *lindex, u32 *hindex, u32 *indexes, bool bypass);
#endif
int is_vra_trigger(struct is_device_ischain *device,
	struct is_subdev *subdev,
	struct is_frame *frame);

int is_sensor_subdev_reqbuf(void *qdevice,
	struct is_queue *queue, u32 count);
bool is_subdev_check_vid(uint32_t vid);
struct is_subdev * video2subdev(enum is_subdev_device_type device_type,
	void *device, u32 vid);

/* internal subdev use */
int is_subdev_internal_open(void *device, enum is_device_type type, struct is_subdev *subdev);
int is_subdev_internal_close(void *device, enum is_device_type type, struct is_subdev *subdev);
int is_subdev_internal_s_format(void *device, enum is_device_type type, struct is_subdev *subdev,
	u32 width, u32 height, u32 bits_per_pixel, u32 buffer_num, const char *type_name);
struct is_sensor_cfg;
int is_subdev_internal_g_bpp(void *device, enum is_device_type type, struct is_subdev *subdev,
	struct is_sensor_cfg *sensor_cfg);
int is_subdev_internal_start(void *device, enum is_device_type type, struct is_subdev *subdev);
int is_subdev_internal_stop(void *device, enum is_device_type type, struct is_subdev *subdev);

#ifdef ENABLE_LOGICAL_VIDEO_NODE
int __mcsc_dma_out_cfg(struct is_device_ischain *device,
	struct is_frame *ldr_frame,
	struct camera2_node *node,
	u32 pindex,
	u32 *lindex,
	u32 *hindex,
	u32 *indexes,
	int index);
#endif
#define GET_SUBDEV_FRAMEMGR(subdev) \
	({ struct is_framemgr *framemgr;						\
	if ((subdev) && (subdev)->vctx)							\
		framemgr = &(subdev)->vctx->queue.framemgr;				\
	else if ((subdev) && test_bit(IS_SUBDEV_INTERNAL_USE, &((subdev)->state)))	\
		framemgr = &(subdev)->internal_framemgr;				\
	else										\
		framemgr = NULL;							\
	framemgr;})

#define GET_SUBDEV_I_FRAMEMGR(subdev)				\
	({ struct is_framemgr *framemgr;			\
	if (subdev)						\
		framemgr = &(subdev)->internal_framemgr;	\
	else							\
		framemgr = NULL;				\
	framemgr; })

#define GET_SUBDEV_QUEUE(subdev) \
	(((subdev) && (subdev)->vctx) ? (&(subdev)->vctx->queue) : NULL)
#define CALL_SOPS(s, op, args...)	(((s) && (s)->ops && (s)->ops->op) ? ((s)->ops->op(s, args)) : 0)
#define MIN(a, b) (((a) < (b)) ? (a) : (b))

#endif
