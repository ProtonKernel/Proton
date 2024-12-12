// SPDX-License-Identifier: GPL-2.0
/*
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
#include <linux/module.h>
#include <linux/slab.h>
#include <linux/netdevice.h>

#include "../utils/mcps_logger.h"
#include "../utils/mcps_cpu.h"
#include "../utils/mcps_utils.h"

#include "mcps_migration.h"

struct migration_manager __migration_manager;

static inline void lock_migration(struct migration_manager *manager) { spin_lock(&manager->lock); }
static inline void unlock_migration(struct migration_manager *manager) { spin_unlock(&manager->lock); }

mcps_visible_for_testing long mcps_migration_min_interval __read_mostly = 1000000; // 1 ms
module_param(mcps_migration_min_interval, long, 0640);

mcps_visible_for_testing unsigned long mcps_migration_waiting_limit __read_mostly = 2; // 1 jiffy
module_param(mcps_migration_waiting_limit, ulong, 0640);

mcps_visible_for_testing bool __mcps_is_qualified_interval(struct migration_manager *manager, struct timespec64 *time)
{
	return (time->tv_sec > 0
		|| time->tv_nsec > mcps_migration_min_interval
		|| time_after(jiffies, manager->oldest_request_time + mcps_migration_waiting_limit));
}

inline bool mcps_is_qualified_interval(struct timespec64 *time)
{
	return __mcps_is_qualified_interval(&__migration_manager, time);
}

mcps_visible_for_testing bool __mcps_is_migration_request_empty(struct migration_manager *manager)
{
	return !manager->oldest_request_time;
}

/* This function can miss hit*/
inline bool mcps_is_migration_request_empty(void)
{
	return __mcps_is_migration_request_empty(&__migration_manager);
}

mcps_visible_for_testing int __mcps_run_migration(struct migration_manager *manager)
{
	int count = 0;
	unsigned long flags;
	bool again = true;
	struct list_head queue;
	struct migration_request *req;

	INIT_LIST_HEAD(&queue);

	while (again) {
		while ((req = list_first_entry_or_null(&queue, struct migration_request, node))) {
			list_del(&req->node);

			count++;
			manager->request_handler(req->from, req->to, req->option);

			kfree(req);
		}

		local_irq_save(flags);
		lock_migration(manager);

		if (list_empty(&manager->request_queue)) {
			again = false;
		} else {
			list_splice_init(&manager->request_queue, &queue);
			manager->queue_size_locked = 0;
			manager->oldest_request_time = 0;
			again = true;
		}

		unlock_migration(manager);
		local_irq_restore(flags);
	}

	return count;
}

inline int mcps_run_migration(void)
{
	return __mcps_run_migration(&__migration_manager);
}

static struct migration_request *
mcps_dequeue_migration_request_locked(struct migration_manager *manager)
{
	struct migration_request *req = list_first_entry_or_null(
		&manager->request_queue, struct migration_request, node);

	if (req == NULL)
		return NULL;

	list_del(&req->node);
	manager->queue_size_locked--;

	return req;
}

#if defined(CONFIG_KUNIT)
mcps_visible_for_testing struct migration_request *
__mcps_dequeue_migration_request(struct migration_manager *manager)
{
	struct migration_request *req;

	local_irq_disable();
	lock_migration(manager);

	req = mcps_dequeue_migration_request_locked(manager);

	unlock_migration(manager);
	local_irq_enable();

	return req;
}

mcps_visible_for_testing inline struct migration_request *
mcps_dequeue_migration_request(void)
{
	return __mcps_dequeue_migration_request(&__migration_manager);
}
#endif

mcps_visible_for_testing int
__mcps_push_migration_request(struct migration_manager *manager, struct migration_request *req)
{
	local_irq_disable();
	lock_migration(manager);

	if (manager->queue_size_locked >= MAX_MIGRATION_REQUEST) {
		unlock_migration(manager);
		local_irq_enable();

		mcps_err("too much migration request");
		return -EINVAL;
	}

	if (++manager->queue_size_locked == 1)
		manager->oldest_request_time = jiffies;

	list_add_tail(&req->node, &manager->request_queue);

	unlock_migration(manager);
	local_irq_enable();

	return 0;
}

mcps_visible_for_testing inline int mcps_push_migration_request(struct migration_request *req)
{
	return __mcps_push_migration_request(&__migration_manager, req);
}

mcps_visible_for_testing struct migration_request *
mcps_make_migration_request(unsigned int from, unsigned int to, unsigned int option)
{
	struct migration_request *req;

	if (option != MCPS_MIGRATION_FLOWID && !mcps_is_cpu_possible(from)) {
		mcps_err("impossible cpu requested : from [%u] to [%u] op [%u]", from, to, option);
		return NULL;
	}

	if (!mcps_is_cpu_possible(to)) {
		mcps_err("impossible cpu requested : from [%u] to [%u] op [%u]", from, to, option);
		return NULL;
	}

	if (!mcps_is_cpu_online(to)) {
		mcps_err("offline cpu requested : from [%u] to [%u] op [%u]", from, to, option);
		return NULL;
	}

	if (!is_valid_migration_type(option)) {
		mcps_err("invalid option requested : from [%u] to [%u] op [%u]", from, to, option);
		return NULL;
	}

	if (option != MCPS_MIGRATION_FLOWID && from == to) {
		mcps_err("same cpu requested : from [%u] to [%u] op [%u]", from, to, option);
		return NULL;
	}

	req = kzalloc(sizeof(struct migration_request), GFP_ATOMIC);

	if (!req)
		return NULL;

	req->from = from;
	req->to = to;
	req->option = option;

	return req;
}

int mcps_request_migration(unsigned int from, unsigned int to, unsigned int option)
{
	struct migration_request *req;

	req = mcps_make_migration_request(from, to, option);

	if (!req)
		return -ENOMEM;

	if (mcps_push_migration_request(req)) {
		kfree(req);
		return -EINVAL;
	}

	return 0;
}

mcps_visible_for_testing int mcps_migration_request_cb(const char *buf, const struct kernel_param *kp)
{
	int error = 0;
	char *temp, *copy, *sub;
	int from, to, option;

	temp = copy = kstrndup(buf, 14, GFP_KERNEL);

	if (!copy) {
		mcps_err("not enough memory kstrndup %s", buf);
		goto fail;
	}

	mcps_dbg("migration requested %s > %s", buf, copy);

	sub = strsep(&temp, " ");
	from = parseUInt(sub, &error);
	if (error)
		goto fail;

	sub = strsep(&temp, " ");
	to = parseUInt(sub, &error);
	if (error)
		goto fail;

	sub = strsep(&temp, " ");
	option = parseUInt(sub, &error);
	if (error)
		goto fail;

	if (mcps_request_migration(from, to, option))
		goto fail;

	kfree(copy);
	return strlen(buf);

fail:
	mcps_err("fail to request migration : cmd - %s", buf);

	kfree(copy);
	return 0;
};

const struct kernel_param_ops mcps_migration_request_ops = {
	.set = &mcps_migration_request_cb,
};

int migration_request_node;
module_param_cb(mcps_migration,
			&mcps_migration_request_ops,
			&migration_request_node,
			0644);

mcps_visible_for_testing void mcps_migration_no_handler(unsigned int from, unsigned int to, unsigned int option)
{
	mcps_err("<BUG> No handler, migration request is discarded : from [%u] to [%u] op [%u]", from, to, option);
}

mcps_visible_for_testing inline int
__init_migration_manager(struct migration_manager *manager, void (*handler)(unsigned int, unsigned int, unsigned int))
{
	memset(manager, 0, sizeof(struct migration_manager));
	manager->lock = __SPIN_LOCK_UNLOCKED(lock);
	INIT_LIST_HEAD(&manager->request_queue);

	if (!handler) {
		mcps_warn_on(!handler);
		mcps_err("migration handler is NULL. Test code ? Initializing with dummy handler");
		__init_migration_manager(&__migration_manager, mcps_migration_no_handler);

		return -EINVAL;
	}

	manager->request_handler = handler;

	return 0;
}

inline void init_migration_manager(void (*handler)(unsigned int, unsigned int, unsigned int))
{
	__init_migration_manager(&__migration_manager, handler);
}

mcps_visible_for_testing void __release_migration_manager(struct migration_manager *manager)
{
	struct migration_request *req;

	local_irq_disable();
	lock_migration(manager);

	while ((req = mcps_dequeue_migration_request_locked(manager)))
		kfree(req);

	unlock_migration(manager);
	local_irq_enable();
}

inline void release_migration_manager(void)
{
	__release_migration_manager(&__migration_manager);
}
