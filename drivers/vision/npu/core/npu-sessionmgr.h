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

#ifndef _NPU_SESSIONMGR_H_
#define _NPU_SESSIONMGR_H_

#include <linux/time.h>
#include "npu-session.h"

#define NPU_MAX_SESSION 32
#define NPU_MAX_CORES_ALLOWED	11 /* 1 greather than max(10) to ease indexing */

struct npu_sessionmgr {
	struct npu_session *session[NPU_MAX_SESSION];
	unsigned long state;
#ifdef CONFIG_NPU_ARBITRATION
	unsigned long cumulative_flc_size;
	unsigned long cumulative_sdma_size;
	unsigned long count_thread_ncp[NPU_MAX_CORES_ALLOWED];
#endif
	atomic_t session_cnt;
	struct mutex mlock;
};

int npu_sessionmgr_probe(struct npu_sessionmgr *sessionmgr);

int npu_sessionmgr_open(struct npu_sessionmgr *sessionmgr);
int npu_sessionmgr_start(struct npu_sessionmgr *sessionmgr);
int npu_sessionmgr_stop(struct npu_sessionmgr *sessionmgr);
int npu_sessionmgr_close(struct npu_sessionmgr *sessionmgr);

int npu_sessionmgr_regID(struct npu_sessionmgr *sessionmgr, struct npu_session *session);
int npu_sessionmgr_unregID(struct npu_sessionmgr *sessionmgr, struct npu_session *session);

#endif
