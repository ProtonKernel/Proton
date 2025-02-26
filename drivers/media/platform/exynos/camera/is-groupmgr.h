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

#ifndef IS_GROUP_MGR_H
#define IS_GROUP_MGR_H

#include "is-config.h"
#include "is-time.h"
#include "is-subdev-ctrl.h"
#include "is-video.h"
#include "is-cmd.h"
#include "is_groupmgr_config.h"

enum is_group_state {
	IS_GROUP_OPEN,
	IS_GROUP_INIT,
	IS_GROUP_START,
	IS_GROUP_SHOT,
	IS_GROUP_REQUEST_FSTOP,
	IS_GROUP_FORCE_STOP,
	IS_GROUP_OTF_INPUT,
	IS_GROUP_OTF_OUTPUT,
	IS_GROUP_PIPE_INPUT,
	IS_GROUP_PIPE_OUTPUT,
	IS_GROUP_SEMI_PIPE_INPUT,
	IS_GROUP_SEMI_PIPE_OUTPUT,
	IS_GROUP_VOTF_INPUT,
	IS_GROUP_VOTF_OUTPUT,
	IS_GROUP_STANDBY,
	IS_GROUP_VOTF_CONN_LINK,
};

enum is_group_input_type {
	GROUP_INPUT_MEMORY,
	GROUP_INPUT_OTF,
	GROUP_INPUT_PIPE,
	GROUP_INPUT_SEMI_PIPE,
	GROUP_INPUT_VOTF
};

struct is_frame;
struct is_device_ischain;
typedef int (*is_shot_callback)(struct is_device_ischain *device,
	struct is_frame *frame);
typedef int (*is_pipe_shot_callback)(struct is_device_ischain *device,
	struct is_group *group,
	struct is_frame *frame);

struct is_group_frame {
	struct list_head		list;
	u32				fcount;
	struct is_crop		canv;
	struct camera2_node_group	group_cfg[GROUP_SLOT_MAX];
};

struct is_group_framemgr {
	struct is_group_frame	*gframes;
	spinlock_t			gframe_slock;
	struct list_head		gframe_head;
	u32				gframe_cnt;
};

struct is_group {
	u32				instance;	/* logical stream id */
	u32				logical_id;	/* logical group id */
	u32				id;		/* physical group id */
	u32				sensor_id;	/* physical module enum */
	u32				slot;		/* physical group slot */

	struct is_hw_ip			*hw_ip;
	struct is_group			*next;
	struct is_group			*prev;
	struct is_group			*gnext;
	struct is_group			*gprev;
	struct is_group			*parent;
	struct is_group			*child;
	struct is_group			*head;
	struct is_group			*tail;
	/* For shot batch mode */
	struct is_framemgr		framemgr;

	struct is_subdev		leader;
	struct is_subdev		*junction;
	struct is_subdev		*subdev[ENTRY_END];
	struct is_framemgr		*locked_sub_framemgr[ENTRY_END];

	struct list_head		subdev_list;

	/* for otf interface */
	atomic_t			sensor_fcount;
	atomic_t			backup_fcount;
	struct semaphore		smp_trigger;
	atomic_t			smp_shot_count;
	u32				init_shots;
	u32				asyn_shots;
	u32				sync_shots;
	u32				skip_shots;

	struct camera2_aa_ctl		intent_ctl;
	struct camera2_lens_ctl		lens_ctl;
	enum camera_flash_mode		flash_ctl;

	u32				source_vid; /* source video id */
	u32				pcount; /* program count */
	u32				fcount; /* frame count */
	atomic_t			scount; /* shot count */
	atomic_t			rcount; /* request count */
	unsigned long			state;

	struct list_head		gframe_head;
	u32				gframe_cnt;

	is_shot_callback		shot_callback;
	is_pipe_shot_callback		pipe_shot_callback;
	struct is_device_ischain	*device;
	struct is_device_sensor		*sensor;
	enum is_device_type		device_type;

#ifdef DEBUG_AA
#ifdef DEBUG_FLASH
	enum aa_ae_flashmode		flashmode;
	struct camera2_flash_dm		flash;
#endif
#endif

#ifdef MEASURE_TIME
#ifdef MONITOR_TIME
	struct is_time		time;
#endif
#endif
	u32				aeflashMode; /* Flash Mode Control */
	u32				remainIntentCount;
	u32				remainFlashCtlCount;
#ifdef ENABLE_LOGICAL_VIDEO_NODE
	u32				juction_vid;
#endif
	struct mutex			mlock_votf;
};

enum is_group_task_state {
	IS_GTASK_START,
	IS_GTASK_REQUEST_STOP
};

struct is_group_task {
	u32				id;
	struct task_struct		*task;
	struct kthread_worker		worker;
	struct semaphore		smp_resource;
	unsigned long			state;
	atomic_t			refcount;

#ifdef ENABLE_SYNC_REPROCESSING
	atomic_t			rep_tick; /* Sync reprocessing tick */
	struct list_head		sync_list;
	atomic_t			preview_cnt[IS_STREAM_COUNT];
	struct list_head		preview_list[IS_STREAM_COUNT];
#endif
#ifdef VH_FPSIMD_API
	struct is_kernel_fpsimd_state	fp_state;
#endif
	spinlock_t			gtask_slock;
};

struct is_shared_framemgr {
	struct is_framemgr		framemgr;
	u32				width;
	u32				height;
	refcount_t			refcount;
	struct mutex			mlock;
};

struct is_groupmgr {
	struct is_group_framemgr	gframemgr[IS_STREAM_COUNT];
	struct is_group			*leader[IS_STREAM_COUNT];
	struct is_group			*group[IS_STREAM_COUNT][GROUP_SLOT_MAX];
	struct is_group_task		gtask[GROUP_ID_MAX];
	struct is_shared_framemgr	shared_framemgr[GROUP_ID_MAX];
#ifdef ENABLE_SYNC_REPROCESSING
	struct timer_list		trigger_timer;
	u32				trigger_instance;
	spinlock_t			trigger_slock;
#endif
#ifdef ENABLE_LOGICAL_VIDEO_NODE
	u32				group_type;
#endif
};

int is_groupmgr_probe(struct platform_device *pdev,
	struct is_groupmgr *groupmgr);
int is_groupmgr_init(struct is_groupmgr *groupmgr,
	struct is_device_ischain *device);
int is_groupmgr_start(struct is_groupmgr *groupmgr,
	struct is_device_ischain *device);
int is_groupmgr_stop(struct is_groupmgr *groupmgr,
	struct is_device_ischain *device);
int is_group_probe(struct is_groupmgr *groupmgr,
	struct is_group *group,
	struct is_device_sensor *sensor,
	struct is_device_ischain *device,
	is_shot_callback shot_callback,
	u32 slot,
	u32 id,
	char *name,
	const struct is_subdev_ops *sops);
int is_group_open(struct is_groupmgr *groupmgr,
	struct is_group *group, u32 id,
	struct is_video_ctx *vctx);
int is_group_close(struct is_groupmgr *groupmgr,
	struct is_group *group);
int is_group_init(struct is_groupmgr *groupmgr,
	struct is_group *group,
	u32 input_type,
	u32 video_id,
	u32 stream_leader);
int is_group_start(struct is_groupmgr *groupmgr,
	struct is_group *group);
int is_group_stop(struct is_groupmgr *groupmgr,
	struct is_group *group);
int is_group_buffer_queue(struct is_groupmgr *groupmgr,
	struct is_group *group,
	struct is_queue *queue,
	u32 index);
int is_group_buffer_finish(struct is_groupmgr *groupmgr,
	struct is_group *group, u32 index);
int is_group_shot(struct is_groupmgr *groupmgr,
	struct is_group *group,
	struct is_frame *frame);
int is_group_done(struct is_groupmgr *groupmgr,
	struct is_group *group,
	struct is_frame *frame,
	u32 done_state);

unsigned long is_group_lock(struct is_group *group,
		enum is_device_type device_type,
		bool leader_lock);
void is_group_unlock(struct is_group *group, unsigned long flags,
		enum is_device_type device_type,
		bool leader_lock);
void is_group_subdev_cancel(struct is_group *group,
		struct is_frame *ldr_frame,
		enum is_device_type device_type,
		enum is_frame_state frame_state,
		bool flush);

int is_group_change_chain(struct is_groupmgr *groupmgr, struct is_group *group, u32 next_id);

/* get head group's subdev framemgr */
struct is_framemgr *is_group_get_head_framemgr(struct is_group *group,
						u32 shot_idx);
#define GET_HEAD_GROUP_FRAMEMGR(group, shot_idx) \
	is_group_get_head_framemgr(group, shot_idx)
#endif
