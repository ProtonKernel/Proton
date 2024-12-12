/*
 * Samsung Exynos5 SoC series FIMC-IS driver
 *
 *
 * Copyright (c) 2016 Samsung Electronics Co., Ltd
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */


#ifndef IS_DEVICE_MGR_H
#define IS_DEVICE_MGR_H

#include "is-config.h"
#include "is-device-ischain.h"
#include "is-device-sensor.h"

#ifdef CONFIG_USE_SENSOR_GROUP
#define GET_DEVICE_TYPE_BY_GRP(group_id)	\
	({enum is_device_type type;	\
	 switch (group_id) {		\
	 case GROUP_ID_SS0:		\
	 case GROUP_ID_SS1:		\
	 case GROUP_ID_SS2:		\
	 case GROUP_ID_SS3:		\
	 case GROUP_ID_SS4:		\
	 case GROUP_ID_SS5:		\
		type = IS_DEVICE_SENSOR;	\
		break;			\
	 default:			\
		type = IS_DEVICE_ISCHAIN;	\
		break;			\
	 }; type;})

#define GET_HEAD_GROUP_IN_DEVICE(type, group)	\
	({ struct is_group *head;			\
	 head = (group)->head;				\
	 while (head) {					\
		if (head->device_type == type)		\
			break;				\
		else					\
			head = head->child;		\
	 }; head;})

#define GET_OUT_FLAG_IN_DEVICE(device_type, out_flag)				\
	({unsigned long tmp_out_flag;						\
	if (device_type == IS_DEVICE_ISCHAIN)				\
		tmp_out_flag = ((out_flag) & (~((1 << ENTRY_3AA) - 1)));	\
	else									\
		tmp_out_flag = ((out_flag) & ((1 << ENTRY_3AA) - 1));		\
	tmp_out_flag;})
#else
#define GET_DEVICE_TYPE_BY_GRP(group_id) IS_DEVICE_ISCHAIN
#define GET_HEAD_GROUP_IN_DEVICE(type, group) ((group)->head)
#define GET_OUT_FLAG_IN_DEVICE(device_type, out_flag) (out_flag)
#endif

struct devicemgr_sensor_tag_data {
	struct is_devicemgr	*devicemgr;
	struct is_group		*group;
	u32			fcount;
	u32			stream;
	u32			cur_shot_idx;
};

struct is_devicemgr {
	struct is_device_sensor		*sensor[IS_STREAM_COUNT];
	struct is_device_ischain		*ischain[IS_STREAM_COUNT];
	struct tasklet_struct			tasklet[IS_STREAM_COUNT];
	struct devicemgr_sensor_tag_data	tag_data[IS_STREAM_COUNT];
};

struct is_group *get_ischain_leader_group(struct is_device_ischain *device);
int is_devicemgr_probe(struct is_devicemgr *devicemgr);
int is_devicemgr_open(struct is_devicemgr *devicemgr,
		void *device, enum is_device_type type);
int is_devicemgr_binding(struct is_devicemgr *devicemgr,
		struct is_device_sensor *sensor,
		struct is_device_ischain *ischain,
		enum is_device_type type);
int is_devicemgr_start(struct is_devicemgr *devicemgr,
		void *device, enum is_device_type type);
int is_devicemgr_stop(struct is_devicemgr *devicemgr,
		void *device, enum is_device_type type);
int is_devicemgr_close(struct is_devicemgr *devicemgr,
		void *device, enum is_device_type type);
int is_devicemgr_shot_callback(struct is_group *group,
		struct is_frame *frame,
		u32 fcount,
		enum is_device_type type);
int is_devicemgr_shot_done(struct is_group *group,
		struct is_frame *ldr_frame,
		u32 status);
void is_devicemgr_late_shot_handle(struct is_group *group,
		struct is_frame *frame, u32 status);
#endif
