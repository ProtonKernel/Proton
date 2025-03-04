/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (C) 2019, Samsung Electronics.
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

#ifndef __CPIF_QOS_INFO_H__
#define __CPIF_QOS_INFO_H__

#include <linux/types.h>
#include <linux/hashtable.h>

struct hiprio_uid_list {
	DECLARE_HASHTABLE(uid_map, 9);
};

struct hiprio_uid {
	u32 uid;
	struct hlist_node h_node;
};

int cpif_qos_init_list(void);
struct hiprio_uid_list *cpif_qos_get_list(void);
struct hiprio_uid *cpif_qos_get_node(u32 uid);
bool cpif_qos_add_uid(u32 uid);
bool cpif_qos_remove_uid(u32 uid);


#endif /* __CPIF_QOS_INFO_H__ */
