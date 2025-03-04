/*
 * Copyright (C) 2012-2020, Samsung Electronics Co., Ltd.
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

#ifndef __TZ_UIWSOCK_H__
#define __TZ_UIWSOCK_H__

#include <linux/kconfig.h>

#if IS_MODULE(CONFIG_TZDEV)
int tz_uiwsock_init(void);
#endif

#endif /* __TZ_UIWSOCK_H__ */
