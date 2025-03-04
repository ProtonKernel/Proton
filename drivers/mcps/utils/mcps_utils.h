/* SPDX-License-Identifier: GPL-2.0
 *
 * Copyright (C) 2021 Samsung Electronics.
 *
 * This software is licensed under the terms of the GNU General Public
 * License version 2, as published by the Free Software Foundation, and
 * may be copied, distributed, and modified under those terms.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 */
#ifndef __MCPS_UTILS_H__
#define __MCPS_UTILS_H__

#include "mcps_logger.h"

static inline unsigned int parseUInt(const char *buf, int *error)
{
	unsigned int ret = 0;

	if (buf == NULL) {
		mcps_err("wrong string : NULL");
		goto fail;
	}

	if (kstrtouint(buf, 0, &ret)) {
		mcps_err("wrong string : %s", buf);
		goto fail;
	}

	return ret;

fail:
	*error = -EINVAL;

	return ret;
}

#endif
