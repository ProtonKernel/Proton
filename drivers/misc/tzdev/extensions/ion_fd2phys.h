/*
 * Copyright (C) 2012-2020 Samsung Electronics, Inc.
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

#ifndef _TZ_ION_FD2PHYS_H
#define _TZ_ION_FD2PHYS_H

#include <linux/kconfig.h>

#if IS_MODULE(CONFIG_TZDEV)
int ionfd2phys_init(void);
void ionfd2phys_exit(void);
#endif

#endif /* _TZ_ION_FD2PHYS_H */
