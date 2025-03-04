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
#ifndef __LOGGER_H__
#define __LOGGER_H__

#include <linux/printk.h>

#define mcps_err(fmt, ...) pr_err("MCPSE[%d] %s : "fmt"\n", raw_smp_processor_id(), __func__, ##__VA_ARGS__)
#define mcps_dbg(fmt, ...) pr_debug("MCPSD[%d] %s : "fmt"\n", raw_smp_processor_id(), __func__, ##__VA_ARGS__)

#if defined(CONFIG_SEC_KUNIT)

#define mcps_visible_for_testing
#define mcps_warn_on(cond)

#else // #if defined(CONFIG_KUNIT)

#define mcps_visible_for_testing static
#define mcps_warn_on(cond) WARN_ON((cond))

#endif // #if defined(CONFIG_KUNIT)

#endif
