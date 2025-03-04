/* SPDX-License-Identifier: GPL-2.0-only */
/*
 * Samsung debugging features for Samsung's SoC's.
 *
 * Copyright (c) 2020 Samsung Electronics Co., Ltd.
 *      http://www.samsung.com
 */

#ifndef SEC_DEBUG_TYPES_H
#define SEC_DEBUG_TYPES_H

#ifdef CONFIG_SEC_DEBUG_DTASK
enum {
	DTYPE_NONE,
	DTYPE_MUTEX,
	DTYPE_RWSEM,
	DTYPE_WORK,
	DTYPE_CPUHP,
	DTYPE_KTHREAD,
	DTYPE_RTMUTEX,
};

struct sec_debug_wait {
	long	type;
	void	*data;
};
#endif

#endif /* SEC_DEBUG_TYPES_H */
