/*
 * Copyright (C) 2017 Samsung Electronics, Inc.
 *
 * This software is licensed under the terms of the GNU General Public
 * License version 2, as published by the Free Software Foundation, and
 * may be copied, distributed, and modified under those terms.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */

#ifndef __TZ_REE_TIME_H__
#define __TZ_REE_TIME_H__

#include <linux/kconfig.h>

#if IS_MODULE(CONFIG_TZDEV)
int tz_ree_time_init(void);
void tz_ree_time_fini(void);
#endif

struct tz_ree_time {
	uint32_t sec;
	uint32_t nsec;
} __attribute__((__packed__));

#endif /* __TZ_REE_TIME_H__ */
