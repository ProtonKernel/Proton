/*
 * Samsung Exynos SoC series Pablo driver
 *
 * Exynos pablo group manager configurations
 *
 * Copyright (c) 2019 Samsung Electronics Co., Ltd
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#ifndef IS_GROUP_MGR_CONFIG_H
#define IS_GROUP_MGR_CONFIG_H

/* #define DEBUG_AA */
/* #define DEBUG_FLASH */

#define GROUP_STREAM_INVALID	0xFFFFFFFF

#ifdef CONFIG_USE_SENSOR_GROUP
#define TRACE_GROUP
#define GROUP_ID_3AA0		0
#define GROUP_ID_3AA1		1
#define GROUP_ID_3AA2		2
#define GROUP_ID_ISP0		3
#define GROUP_ID_ISP1		4
#define GROUP_ID_MCS0		5
#define GROUP_ID_MCS1		6
#define GROUP_ID_VRA0		7
#define GROUP_ID_PAF0		8
#define GROUP_ID_PAF1		9
#define GROUP_ID_PAF2		10
#define GROUP_ID_CLH0		11
#define GROUP_ID_SS0		12
#define GROUP_ID_SS1		13
#define GROUP_ID_SS2		14
#define GROUP_ID_SS3		15
#define GROUP_ID_SS4		16
#define GROUP_ID_SS5		17
#define GROUP_ID_3AA3		18	/* For Backward compatilility */
#define GROUP_ID_PAF3		19
#define GROUP_ID_YPP		20
#define GROUP_ID_MAX		21
#define GROUP_ID_PARM_MASK	((1 << (GROUP_ID_SS0)) - 1)
#define GROUP_ID(id)		(1 << (id))

#define GROUP_SLOT_SENSOR	0
#define GROUP_SLOT_PAF		1
#define GROUP_SLOT_3AA		2
#define GROUP_SLOT_ISP		3
#define GROUP_SLOT_MCS		4
#define GROUP_SLOT_VRA		5
#define GROUP_SLOT_CLH		6
#define GROUP_SLOT_YPP		7
#define GROUP_SLOT_MAX		8

static const char * const group_id_name[GROUP_ID_MAX + 1] = {
	[GROUP_ID_3AA0] = "G:3AA0",
	[GROUP_ID_3AA1] = "G:3AA1",
	[GROUP_ID_3AA2] = "G:3AA2",
	[GROUP_ID_ISP0] = "G:ITP0",
	[GROUP_ID_ISP1] = "G:ERR4",
	[GROUP_ID_MCS0] = "G:MCS0",
	[GROUP_ID_MCS1] = "G:ERR6",
	[GROUP_ID_VRA0] = "G:ERR7",
	[GROUP_ID_PAF0] = "G:PDP0",
	[GROUP_ID_PAF1] = "G:PDP1",
	[GROUP_ID_PAF2] = "G:PDP2",
	[GROUP_ID_CLH0] = "G:CLH0",
	[GROUP_ID_SS0] = "G:SS0",
	[GROUP_ID_SS1] = "G:SS1",
	[GROUP_ID_SS2] = "G:SS2",
	[GROUP_ID_SS3] = "G:SS3",
	[GROUP_ID_SS4] = "G:SS4",
	[GROUP_ID_SS5] = "G:SS5",
	[GROUP_ID_3AA3] = "G:ERR18",
	[GROUP_ID_PAF3] = "G:ERR19",
	[GROUP_ID_YPP] = "G:ERR20",
	[GROUP_ID_MAX] = "G:MAX"
};

#else
#define TRACE_GROUP
#define GROUP_ID_3AA0		0
#define GROUP_ID_3AA1		1
#define GROUP_ID_3AA2		2
#define GROUP_ID_ISP0		3
#define GROUP_ID_ISP1		4
#define GROUP_ID_MCS0		5
#define GROUP_ID_MCS1		6
#define GROUP_ID_VRA0		7
#define GROUP_ID_CLH0		8
#define GROUP_ID_MAX		9
#define GROUP_ID_PARM_MASK	((1 << (GROUP_ID_MAX)) - 1)
#define GROUP_ID(id)		(1 << (id))

#define GROUP_SLOT_3AA		0
#define GROUP_SLOT_ISP		1
#define GROUP_SLOT_MCS		2
#define GROUP_SLOT_VRA		3
#define GROUP_SLOT_CLH		4
#define GROUP_SLOT_MAX		5

static const char * const group_id_name[GROUP_ID_MAX + 1] = {
	[GROUP_ID_3AA0] = "G:3AA0",
	[GROUP_ID_3AA1] = "G:3AA1",
	[GROUP_ID_3AA2] = "G:3AA2",
	[GROUP_ID_ISP0] = "G:ITP0",
	[GROUP_ID_ISP1] = "G:ERR4",
	[GROUP_ID_MCS0] = "G:MCS0",
	[GROUP_ID_MCS1] = "G:ERR6",
	[GROUP_ID_VRA0] = "G:ERR7",
	[GROUP_ID_CLH0] = "G:CLH0",
	[GROUP_ID_MAX] = "G:MAX"
};
#endif

/*
 * <LINE_FOR_SHOT_VALID_TIME>
 * If valid time is too short when image height is small, use this feature.
 * If height is smaller than this value, async_shot is increased.
 */
#define LINE_FOR_SHOT_VALID_TIME	500

#define IS_MAX_GFRAME	(VIDEO_MAX_FRAME) /* max shot buffer of F/W : 32 */
#define MIN_OF_ASYNC_SHOTS	1
#define MIN_OF_SYNC_SHOTS	2

#define MIN_OF_SHOT_RSC		(1)
#define MIN_OF_ASYNC_SHOTS_240FPS	(MIN_OF_ASYNC_SHOTS + 0)

#ifdef ENABLE_SYNC_REPROCESSING
#define REPROCESSING_TICK_COUNT	(7) /* about 200ms */
#endif
#endif

