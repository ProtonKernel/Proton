/*
 * Samsung Exynos SoC series NPU driver
 *
 * Copyright (c) 2017 Samsung Electronics Co., Ltd.
 *              http://www.samsung.com/
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 */

#ifndef _NPU_PROFILE_H_
#define _NPU_PROFILE_H_

	/* FRAME
	 * 0(0x00) ~ 31 : Processing
	 * .uid = UID
	 * .frame_id = frame ID
	 * .param[0] = direction (EQ and DQ only)
	 * .param[1] = request ID (between SCHEDULED ~ DONE only)
	 * .param[2] = reserved
	 * .param[3] = reserved
	 * .param[4] = reserved
	 */
	/* NW
	 * 32(0x20) ~ 63 : Network mgmt.
	 * .uid = UID
	 * .frame_id = N/A
	 * .param[0] = Command code (See nw_cmd_e definition in npu-commoh.h)
	 * .param[1] = request ID (between SCHEDULED ~ DONE only)
	 * .param[2] = reserved
	 * .param[3] = reserved
	 * .param[4] = reserved
	 */
#define NPU_PROFILE_ID	{ \
	NPU_PROFILE_IDS(FRAME_EQ),	\
	NPU_PROFILE_IDS(FRAME_SCHEDULED),	\
	NPU_PROFILE_IDS(FRAME_PROCESS),	\
	NPU_PROFILE_IDS(FRAME_COMPLETED),	\
	NPU_PROFILE_IDS(FRAME_DONE),	\
	NPU_PROFILE_IDS(FRAME_DQ),	\
	NPU_PROFILE_IDS(FRAME_VS4L_QBUF_ENTER),	\
	NPU_PROFILE_IDS(FRAME_VS4L_DQBUF_RET),	\
	NPU_PROFILE_IDS(NW_RECEIVED),	\
	NPU_PROFILE_IDS(NW_SCHEDULED),	\
	NPU_PROFILE_IDS(NW_PROCESS),	\
	NPU_PROFILE_IDS(NW_COMPLETED),	\
	NPU_PROFILE_IDS(NW_DONE),	\
	NPU_PROFILE_IDS(NW_NOTIFIED),	\
	NPU_PROFILE_IDS(NW_VS4L_ENTER),	\
	NPU_PROFILE_IDS(NW_VS4L_RET),	\
	NPU_PROFILE_IDS(MAX)	\
}

#undef NPU_PROFILE_IDS
#define NPU_PROFILE_IDS(x)	PROBE_ID_DD_##x
enum NPU_PROFILE_ID;
#undef NPU_PROFILE_IDS
#define NPU_PROFILE_IDS(x)	#x
static char *npu_profile_id_name[] = NPU_PROFILE_ID;

struct npu_profile_operation {
	u32 (*timestamp)(void);
};

#ifdef CONFIG_NPU_USE_SPROFILER

#include <npu-common.h>
#include <linux/mutex.h>
#include <linux/wait.h>
#include "npu-util-statekeeper.h"

#define PPMU_NUM 5

#define NPU_PROFILE_UNIT_NUM	10	/* 0: Firmware, 1: D.Drv */
#define NPU_PROFILE_POINT_NUM	((4096)*(1024))

/* Device driver specific */
#define PROFILE_UID		0xFFFF0000	/* Special UID for profile */
#define PROFILE_CMD_POST_RETRY_INTERVAL	100
#define PROFILE_CMD_POST_RETRY_CNT	10
#define PROFILE_HW_RESP_TIMEOUT		1000

enum profile_unit_id {
	PROFILE_UNIT_FIRMWARE = 0,
	PROFILE_UNIT_DEVICE_DRIVER,
	PROFILE_UNIT_END,
};

struct probe_point {
	u32 id;
	u32 timestamp;
	u32 session_id; /* session id is same with object id of firmware */
	u32 frame_id;
	u32 param[5];
	/*
	 * parameter represents various meaning depend on id
	 * espeically GP, SG
	 * param[0] : group id
	 * param[1] : subgroup id
	 */
};

struct probe_meta {
	u32	profile_unit_id;
	u32	start_idx;
	u32	total_count;
	u32	valid_count;
};

struct npu_profile {
	u32			point_cnt;
	u32			rsvd0;
	struct probe_meta	meta[NPU_PROFILE_UNIT_NUM];
	struct probe_point	point[NPU_PROFILE_POINT_NUM];
};


struct npu_profile_control {
	struct npu_profile		*profile_data;	/* Pointer to buf's vaddr */
	struct npu_memory_buffer        buf;		/* Shared buffer handle to struct profile_data object */
	struct npu_statekeeper          statekeeper;
	struct npu_profile_operation	ops;
	int                             unit_num;       /* Not used yet */
	int                             point_num;      /* Not used yet */
	atomic_t			next_probe_point_idx;	/* Index of npu_profile.point[] to be used on next profile data */
	void __iomem			*pwm;		/* PWM SFR */
	struct mutex			lock;
	wait_queue_head_t		wq;
	int				result_code;
	atomic_t			result_available;
};

struct npu_system;
int npu_profile_probe(struct npu_system *system);
int npu_profile_open(struct npu_system *system);
int npu_profile_close(struct npu_system *system);
int npu_profile_release(void);
int npu_profile_set_operation(const struct npu_profile_operation *ops);

void profile_point1(const u32 point_id, const u32 uid, const u32 frame_id, const u32 p0);
void profile_point3(
	const u32 point_id, const u32 uid, const u32 frame_id,
	const u32 p0, const u32 p1, const u32 p2);

#else	/* !CONFIG_NPU_USE_SPROFILER */

#define profile_point1(point_id, uid, frame_id, p0)
#define profile_point3(point_id, uid, frame_id, p0, p1, p2)
#define npu_profile_set_operation(ops)	0

#endif	/* CONFIG_NPU_USE_SPROFILER */

#endif	/* _NPU_PROFILE_H_ */
