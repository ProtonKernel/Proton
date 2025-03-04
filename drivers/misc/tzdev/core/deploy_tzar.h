/*
 * Copyright (C) 2012-2019 Samsung Electronics, Inc.
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

#ifndef __TZ_DEPLOY_TZAR_H__
#define __TZ_DEPLOY_TZAR_H__

#include <linux/kconfig.h>

#ifdef CONFIG_TZDEV_DEPLOY_TZAR
int tzdev_deploy_tzar(void);
#else
static inline int tzdev_deploy_tzar(void)
{
	return 0;
}
#endif

#if IS_MODULE(CONFIG_TZDEV)
int tz_deploy_tzar_init(void);
#endif

#endif /*__TZ_DEPLOY_TZAR_H__ */
