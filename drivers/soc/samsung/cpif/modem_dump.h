/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (C) 2016 Samsung Electronics.
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

#ifndef __MODEM_DUMP_H__
#define __MODEM_DUMP_H__

#include "modem_prj.h"
#include "link_device_memory.h"

extern int cp_get_log_dump(struct io_device *iod, struct link_device *ld, unsigned long arg);

#endif
