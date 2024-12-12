/*
 * Samsung Exynos5 SoC series FIMC-IS driver
 *
 *
 * Copyright (c) 2011 Samsung Electronics Co., Ltd
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#ifndef IS_DEVICE_ISCHAIN_H
#define IS_DEVICE_ISCHAIN_H

#include "is-mem.h"
#include "is-subdev-ctrl.h"
#include "hardware/is-hw-control.h"
#include "is-groupmgr.h"
#include "is-resourcemgr.h"
#include "is-pipe.h"
#include "is-binary.h"
#include "is-debug.h"

#define SENSOR_MAX_CTL			0x3
#define SENSOR_MAX_CTL_MASK		(SENSOR_MAX_CTL-1)

#define REPROCESSING_FLAG		0x80000000
#define REPROCESSING_MASK		0xF0000000
#define REPROCESSING_SHIFT		28
#define OTF_3AA_MASK			0x0F000000
#define OTF_3AA_SHIFT			24
#define SSX_VINDEX_MASK			0x00FF0000
#define SSX_VINDEX_SHIFT		16
#define TAX_VINDEX_MASK			0x0000FF00
#define TAX_VINDEX_SHIFT		8
#define MODULE_MASK			0x000000FF

#define IS_SETFILE_MASK		0x0000FFFF
#define IS_SCENARIO_MASK		0xFFFF0000
#define IS_SCENARIO_SHIFT		16
#define IS_ISP_CRANGE_MASK		0x0F000000
#define IS_ISP_CRANGE_SHIFT	24
#define IS_SCC_CRANGE_MASK		0x00F00000
#define IS_SCC_CRANGE_SHIFT	20
#define IS_SCP_CRANGE_MASK		0x000F0000
#define IS_SCP_CRANGE_SHIFT	16
#define IS_CRANGE_FULL		0
#define IS_CRANGE_LIMITED		1

/*global state*/
enum is_ischain_state {
	IS_ISCHAIN_OPENING,
	IS_ISCHAIN_CLOSING,
	IS_ISCHAIN_OPEN,
	IS_ISCHAIN_INITING,
	IS_ISCHAIN_INIT,
	IS_ISCHAIN_START,
	IS_ISCHAIN_LOADED,
	IS_ISCHAIN_POWER_ON,
	IS_ISCHAIN_OPEN_STREAM,
	IS_ISCHAIN_REPROCESSING,
	IS_ISCHAIN_OFFLINE_REPROCESSING,
};

enum is_stream_type {
	IS_PREVIEW_STREAM = 0,
	IS_REPROCESSING_STREAM,
	IS_OFFLINE_REPROCESSING_STREAM,
};

enum is_camera_device {
	CAMERA_SINGLE_REAR,
	CAMERA_SINGLE_FRONT,
};

enum is_fast_ctl_state {
	IS_FAST_CTL_FREE,
	IS_FAST_CTL_REQUEST,
	IS_FAST_CTL_STATE,
};

struct is_fast_ctl {
	struct list_head	list;
	u32			state;

	bool			lens_pos_flag;
	uint32_t		lens_pos;
};

#define MAX_NUM_FAST_CTL	5
struct fast_control_mgr {
	u32			fast_capture_count;

	spinlock_t		slock;
	struct is_fast_ctl	fast_ctl[MAX_NUM_FAST_CTL];
	u32			queued_count[IS_FAST_CTL_STATE];
	struct list_head	queued_list[IS_FAST_CTL_STATE];
};

#define NUM_OF_3AA_SUBDEV	4
#define NUM_OF_ISP_SUBDEV	2
#define NUM_OF_DCP_SUBDEV	6
#define NUM_OF_MCS_SUBDEV	6
struct is_device_ischain {
	u32					instance; /* logical stream id */
	u32					sensor_id; /* physical module enum */

	struct platform_device			*pdev;
	struct exynos_platform_is		*pdata;

	struct is_resourcemgr			*resourcemgr;
	struct is_groupmgr			*groupmgr;
	struct is_devicemgr			*devicemgr;
	struct is_interface			*interface;

	struct is_hardware			*hardware;

	struct is_mem				*mem;

	u32					module;
	struct is_minfo				*minfo;
	struct is_path_info			path;

	/* for offline processing */
	struct is_module_enum			*module_enum;
	struct v4l2_subdev			*subdev_module;
	u32					binning;

	struct is_region			*is_region;
	ulong					kvaddr_shared;
	dma_addr_t				dvaddr_shared;

	unsigned long				state;
	atomic_t				group_open_cnt;
	atomic_t				open_cnt;
	atomic_t				init_cnt;

	u32					setfile;
	u32					apply_setfile_fcount;
	u32					dvfs_scenario;
	u32					dvfs_rec_size;
	bool					llc_mode; /* 0: LLC off, 1: LLC on(default) */

#if !defined(FAST_FDAE)
	struct camera2_fd_uctl			fdUd;
#endif
#ifdef ENABLE_SENSOR_DRIVER
	struct camera2_uctl			peri_ctls[SENSOR_MAX_CTL];
#endif

	/* isp margin */
	u32					margin_left;
	u32					margin_right;
	u32					margin_width;
	u32					margin_top;
	u32					margin_bottom;
	u32					margin_height;

#ifdef ENABLE_BUFFER_HIDING
	struct is_pipe				pipe;
#endif

	struct is_group				group_paf;	/* for PAF Bayer RDMA */
	struct is_subdev			pdaf;		/* PDP(PATSTAT) AF RDMA */
	struct is_subdev			pdst;		/* PDP(PATSTAT) PD STAT WDMA */

	struct is_group				group_3aa;
	struct is_subdev			txc;
	struct is_subdev			txp;
	struct is_subdev			txf;
	struct is_subdev			txg;
	struct is_subdev			txo;
	struct is_subdev			txl;
	struct is_subdev			orbxc;		/* for key points/descriptor */

	struct is_group				group_lme;	/* LME RDMA */
	struct is_subdev			lmes;		/* LME MVF WDMA */
	struct is_subdev			lmec;		/* LME COST WDMA */

	struct is_group				group_isp;
	struct is_subdev			ixc;
	struct is_subdev			ixp;
	struct is_subdev			ixt;
	struct is_subdev			ixg;
	struct is_subdev			ixv;
	struct is_subdev			ixw;
	struct is_subdev			mexc;	/* for ME */

	struct is_group				group_ypp;

	struct is_group				group_mcs;
	struct is_subdev			m0p;
	struct is_subdev			m1p;
	struct is_subdev			m2p;
	struct is_subdev			m3p;
	struct is_subdev			m4p;
	struct is_subdev			m5p;

	struct is_group				group_vra;

	struct is_group				group_clh;	/* CLAHE RDMA */
	struct is_subdev			clhc;	/* CLAHE WDMA */

	u32					private_data;
	struct is_device_sensor			*sensor;
	struct is_pm_qos_request		user_qos;

	/* Async metadata control to reduce frame delay */
	struct fast_control_mgr			fastctlmgr;

	/* for NI(noise index from DDK) use */
	u32					cur_noise_idx[NI_BACKUP_MAX]; /* Noise index for N + 1 */
	u32					next_noise_idx[NI_BACKUP_MAX]; /* Noise index for N + 2 */
	u32					partial_update;

	/* cache maintenance for user buffer */
	struct is_dbuf_q			*dbuf_q;
};

/*global function*/
int is_ischain_probe(struct is_device_ischain *device,
	struct is_interface *interface,
	struct is_resourcemgr *resourcemgr,
	struct is_groupmgr *groupmgr,
	struct is_devicemgr *devicemgr,
	struct is_mem *mem,
	struct platform_device *pdev,
	u32 instance);
int is_ischain_g_ddk_setfile_version(struct is_device_ischain *device,
	void *user_ptr);
int is_ischain_g_capability(struct is_device_ischain *this,
	ulong user_ptr);
void is_ischain_meta_invalid(struct is_frame *frame);

int is_ischain_open_wrap(struct is_device_ischain *device, bool EOS);
int is_ischain_close_wrap(struct is_device_ischain *device);
int is_ischain_start_wrap(struct is_device_ischain *device,
	struct is_group *group);
int is_ischain_stop_wrap(struct is_device_ischain *device,
	struct is_group *group);

/* PAF_RDMA subdev */
int is_ischain_paf_open(struct is_device_ischain *device,
	struct is_video_ctx *vctx);
int is_ischain_paf_close(struct is_device_ischain *device,
	struct is_video_ctx *vctx);
int is_ischain_paf_s_input(struct is_device_ischain *device,
	u32 stream_type,
	u32 module_id,
	u32 video_id,
	u32 input_type,
	u32 stream_leader);
int is_ischain_paf_buffer_queue(struct is_device_ischain *device,
	struct is_queue *queue,
	u32 index);
int is_ischain_paf_buffer_finish(struct is_device_ischain *device,
	u32 index);

/* 3AA subdev */
int is_ischain_3aa_open(struct is_device_ischain *device,
	struct is_video_ctx *vctx);
int is_ischain_3aa_close(struct is_device_ischain *device,
	struct is_video_ctx *vctx);
int is_ischain_3aa_s_input(struct is_device_ischain *device,
	u32 stream_type,
	u32 module_id,
	u32 video_id,
	u32 input_type,
	u32 stream_leader);
int is_ischain_3aa_buffer_queue(struct is_device_ischain *device,
	struct is_queue *queue,
	u32 index);
int is_ischain_3aa_buffer_finish(struct is_device_ischain *device,
	u32 index);

/* LME subdev */
int is_ischain_lme_open(struct is_device_ischain *device,
	struct is_video_ctx *vctx);
int is_ischain_lme_close(struct is_device_ischain *device,
	struct is_video_ctx *vctx);
int is_ischain_lme_s_input(struct is_device_ischain *device,
	u32 stream_type,
	u32 module_id,
	u32 video_id,
	u32 input_type,
	u32 stream_leader);
int is_ischain_lme_buffer_queue(struct is_device_ischain *device,
	struct is_queue *queue,
	u32 index);
int is_ischain_lme_buffer_finish(struct is_device_ischain *this,
	u32 index);


/* isp subdev */
int is_ischain_isp_open(struct is_device_ischain *device,
	struct is_video_ctx *vctx);
int is_ischain_isp_close(struct is_device_ischain *device,
	struct is_video_ctx *vctx);
int is_ischain_isp_s_input(struct is_device_ischain *device,
	u32 stream_type,
	u32 module_id,
	u32 video_id,
	u32 input_type,
	u32 stream_leader);
int is_ischain_isp_buffer_queue(struct is_device_ischain *device,
	struct is_queue *queue,
	u32 index);
int is_ischain_isp_buffer_finish(struct is_device_ischain *this,
	u32 index);

/* YPP subdev */
int is_ischain_ypp_open(struct is_device_ischain *device,
	struct is_video_ctx *vctx);
int is_ischain_ypp_close(struct is_device_ischain *device,
	struct is_video_ctx *vctx);
int is_ischain_ypp_s_input(struct is_device_ischain *device,
	u32 stream_type,
	u32 module_id,
	u32 video_id,
	u32 input_type,
	u32 stream_leader);
int is_ischain_ypp_buffer_queue(struct is_device_ischain *device,
	struct is_queue *queue,
	u32 index);
int is_ischain_ypp_buffer_finish(struct is_device_ischain *this,
	u32 index);

/* MCSC subdev */
int is_ischain_mcs_open(struct is_device_ischain *device,
	struct is_video_ctx *vctx);
int is_ischain_mcs_close(struct is_device_ischain *device,
	struct is_video_ctx *vctx);
int is_ischain_mcs_s_input(struct is_device_ischain *device,
	u32 stream_type,
	u32 module_id,
	u32 video_id,
	u32 otf_input,
	u32 stream_leader);
int is_ischain_mcs_buffer_queue(struct is_device_ischain *device,
	struct is_queue *queue,
	u32 index);
int is_ischain_mcs_buffer_finish(struct is_device_ischain *device,
	u32 index);

/* vra subdev */
int is_ischain_vra_open(struct is_device_ischain *device,
	struct is_video_ctx *vctx);
int is_ischain_vra_close(struct is_device_ischain *device,
	struct is_video_ctx *vctx);
int is_ischain_vra_s_input(struct is_device_ischain *device,
	u32 stream_type,
	u32 module_id,
	u32 video_id,
	u32 otf_input,
	u32 stream_leader);
int is_ischain_vra_buffer_queue(struct is_device_ischain *device,
	struct is_queue *queue,
	u32 index);
int is_ischain_vra_buffer_finish(struct is_device_ischain *this,
	u32 index);

/* CLAHE subdev */
int is_ischain_clh_open(struct is_device_ischain *device,
	struct is_video_ctx *vctx);
int is_ischain_clh_close(struct is_device_ischain *device,
	struct is_video_ctx *vctx);
int is_ischain_clh_s_input(struct is_device_ischain *device,
	u32 stream_type,
	u32 module_id,
	u32 video_id,
	u32 input_type,
	u32 stream_leader);
int is_ischain_clh_buffer_queue(struct is_device_ischain *device,
	struct is_queue *queue,
	u32 index);
int is_ischain_clh_buffer_finish(struct is_device_ischain *device,
	u32 index);

int is_itf_stream_on(struct is_device_ischain *this);
int is_itf_stream_off(struct is_device_ischain *this);
int is_itf_process_start(struct is_device_ischain *device,
	u32 group);
int is_itf_process_stop(struct is_device_ischain *device,
	u32 group);
int is_itf_force_stop(struct is_device_ischain *device,
	u32 group);
int is_itf_grp_shot(struct is_device_ischain *device,
	struct is_group *group,
	struct is_frame *frame);
int is_itf_i2c_lock(struct is_device_ischain *this,
	int i2c_clk, bool lock);

int is_itf_s_param(struct is_device_ischain *device,
	struct is_frame *frame,
	u32 lindex,
	u32 hindex,
	u32 indexes);
void * is_itf_g_param(struct is_device_ischain *device,
	struct is_frame *frame,
	u32 index);
void is_itf_storefirm(struct is_device_ischain *device);
void is_itf_restorefirm(struct is_device_ischain *device);
int is_itf_set_fwboot(struct is_device_ischain *device, u32 val);

int is_ischain_buf_tag_input(struct is_device_ischain *device,
	struct is_subdev *subdev,
	struct is_frame *ldr_frame,
	u32 pixelformat,
	u32 width,
	u32 height,
	u32 target_addr[]);
int is_ischain_buf_tag(struct is_device_ischain *device,
	struct is_subdev *subdev,
	struct is_frame *ldr_frame,
	u32 pixelformat,
	u32 width,
	u32 height,
	u32 target_addr[]);
int is_ischain_buf_tag_64bit(struct is_device_ischain *device,
	struct is_subdev *subdev,
	struct is_frame *ldr_frame,
	u32 pixelformat,
	u32 width,
	u32 height,
	uint64_t target_addr[]);
int is_ischain_update_sensor_mode(struct is_device_ischain *device,
	struct is_frame *frame);

extern const struct is_queue_ops is_ischain_paf_ops;
extern const struct is_queue_ops is_ischain_3aa_ops;
extern const struct is_queue_ops is_ischain_lme_ops;
extern const struct is_queue_ops is_ischain_isp_ops;
extern const struct is_queue_ops is_ischain_dis_ops;
extern const struct is_queue_ops is_ischain_dcp_ops;
extern const struct is_queue_ops is_ischain_mcs_ops;
extern const struct is_queue_ops is_ischain_vra_ops;
extern const struct is_queue_ops is_ischain_clh_ops;
extern const struct is_queue_ops is_ischain_ypp_ops;
extern const struct is_queue_ops is_ischain_subdev_ops;

int is_itf_power_down(struct is_interface *interface);
int is_ischain_power(struct is_device_ischain *this, int on);

#define IS_EQUAL_COORD(i, o)				\
	(((i)[0] != (o)[0]) || ((i)[1] != (o)[1]) ||	\
	 ((i)[2] != (o)[2]) || ((i)[3] != (o)[3]))
#define IS_NULL_COORD(c)				\
	(!(c)[0] && !(c)[1] && !(c)[2] && !(c)[3])
#endif
