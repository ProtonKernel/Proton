/* SPDX-License-Identifier: GPL-2.0
 *
 * Copyright (C) 2019-2021 Samsung Electronics.
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
#ifndef __MCPS_H__
#define __MCPS_H__

#include <linux/skbuff.h>

extern void mcps_prepare_handler(void);
extern void mcps_complete_handler(int work, int stop);

extern int mcps_try_skb(struct sk_buff *skb);

#ifdef CONFIG_MCPS_GRO_ENABLE
extern int mcps_try_gro(struct sk_buff *skb);
#else
#define mcps_try_gro(skb) mcps_try_skb(skb)
#endif

#endif //__MCPS_H__
