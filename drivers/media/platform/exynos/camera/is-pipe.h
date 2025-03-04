/*
 * Samsung Exynos5 SoC series FIMC-IS driver
 *
 *
 * Copyright (c) 2015 Samsung Electronics Co., Ltd
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */
#ifndef IS_PIPE_H
#define IS_PIPE_H

#include "is-groupmgr.h"

#define IS_MAX_PIPE_BUFS (5)

enum pipe_slot_type {
	PIPE_SLOT_SRC,
	PIPE_SLOT_JUNCTION,
	PIPE_SLOT_DST,
	PIPE_SLOT_MAX,
};

struct is_pipe {
	u32 id;
	struct is_group *src;
	struct is_group *dst;
	struct is_video_ctx *vctx[PIPE_SLOT_MAX];
	struct camera2_dm pipe_dm;
	struct camera2_udm pipe_udm;
	struct v4l2_buffer buf[PIPE_SLOT_MAX][IS_MAX_PIPE_BUFS];
	struct v4l2_plane planes[PIPE_SLOT_MAX][IS_MAX_PIPE_BUFS][IS_MAX_PLANES];
};

int is_pipe_probe(struct is_pipe *pipe);
int is_pipe_create(struct is_pipe *pipe,
	struct is_group *src,
	struct is_group *dst);
#endif
