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

#ifndef __CP_BTL_H__
#define __CP_BTL_H__

enum cp_btl_id {
	BTL_ID_0,
	BTL_ID_1,
	MAX_BTL_ID,
};

struct cp_btl_mem_region {
	unsigned long p_base;
	void __iomem *v_base;
	unsigned long cp_p_base;
	u32 size;
};

struct cp_btl {
	char *name;
	u32 id;

	bool enabled;
	atomic_t active;

	u32 link_type;
	struct cp_btl_mem_region mem;
#if IS_ENABLED(CONFIG_LINK_DEVICE_PCIE)
	int last_pcie_atu_grp;
#endif

	struct mem_link_device *mld;
	struct miscdevice miscdev;
};

#if IS_ENABLED(CONFIG_CP_BTL)
extern int cp_btl_create(struct cp_btl *btl, struct device *dev);
extern int cp_btl_destroy(struct cp_btl *btl);
#else
static inline int cp_btl_create(struct cp_btl *btl, struct device *dev) { return 0; }
static inline int cp_btl_destroy(struct cp_btl *btl) { return 0; }
#endif

#endif /* __CP_BTL_H__ */
