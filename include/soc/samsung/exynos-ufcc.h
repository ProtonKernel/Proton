/*
 * Copyright (c) 2020 Samsung Electronics Co., Ltd
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 and
 * only version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */

#include <linux/pm_qos.h>

struct ucc_req {
	char			name[20];
	int			active;
	int			value;
	struct list_head	list;
};

struct flist_head {
	struct list_head	node_list;
};

struct flist_node {
	int			prio;
	struct list_head	prio_list;
	struct list_head	node_list;
};

struct ufc_req {
	struct flist_node node;
	bool active;
	char *func;
	unsigned int line;
};

#if IS_ENABLED(CONFIG_EXYNOS_UFCC)
extern void ucc_add_request(struct ucc_req *req, int value);
extern void ucc_update_request(struct ucc_req *req, int value);
extern void ucc_remove_request(struct ucc_req *req);

#define ufc_add_request(req) __ufc_add_request(req, (char *)__func__, __LINE__);
extern int __ufc_add_request(struct ufc_req *req, char *func, unsigned int line);
extern int ufc_remove_request(struct ufc_req *req);
extern int ufc_update_request(struct ufc_req *req, s32 new_freq);
#else
static inline void ucc_add_request(struct ucc_req *req, int value) { }
static inline void ucc_update_request(struct ucc_req *req, int value) { }
static inline void ucc_remove_request(struct ucc_req *req) { }

#define ufc_add_request(req) __ufc_add_request(req, (char *)__func__, __LINE__);
static inline int __ufc_add_request(struct ufc_req *req, char *func, unsigned int line) { return 0; }
static inline int ufc_remove_request(struct ufc_req *req) { return 0; }
static inline int ufc_update_request(struct ufc_req *req, s32 new_freq) { return 0; }
#endif

enum exynos_ufc_execution_mode {
	AARCH64_MODE = 0,
	AARCH32_MODE,
	MODE_END,
};

struct ufc_table_info {
	int			ctrl_type;
	int			mode;
	u32			**ufc_table;
	struct list_head	list;
	int			status;

	struct kobject		kobj;
};

struct ufc_domain {
	struct cpumask		cpus;

	unsigned int		table_col_idx;
	unsigned int		min_freq;
	unsigned int		max_freq;
	unsigned int		clear_freq;

	struct freq_qos_request	user_min_qos_req;
	struct freq_qos_request	user_max_qos_req;
	struct freq_qos_request	user_min_qos_wo_boost_req;

	int			allow_disable_cpus;

	struct list_head	list;
};
