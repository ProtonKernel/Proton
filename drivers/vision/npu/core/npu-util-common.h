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

#ifndef _NPU_UTILL_COMMON_H_
#define _NPU_UTILL_COMMON_H_

#include <linux/ktime.h>
#include "npu-session.h"

#define NPU_ERR_RET(cond, fmt, ...) \
do { \
	int ret = (cond); \
	if (unlikely(ret < 0)) { \
		npu_err(fmt, ##__VA_ARGS__); \
		return ret; \
	} \
} while (0)

s64 npu_get_time_ns(void);
s64 npu_get_time_us(void);

int npu_util_validate_user_ncp(struct npu_session *session, struct ncp_header *ncp_header,
				size_t ncp_size);

#endif	/* _NPU_UTILL_COMMON_H_ */
