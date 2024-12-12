/*
 * Copyright (c) 2017 Samsung Electronics Co., Ltd
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

#include <linux/kobject.h>
#include <linux/sched.h>
#include <linux/sched/idle.h>

struct rq;
struct rt_rq;
struct cpufreq_policy;
struct cgroup_taskset;
struct cgroup_subsys_state;

enum {
	STATES_FREQ = 0,
	STATES_PMQOS,
	NUM_OF_REQUESTS,
};

#define EMS_PART_ENQUEUE        0x1
#define EMS_PART_DEQUEUE        0x2
#define EMS_PART_UPDATE         0x4
#define EMS_PART_WAKEUP_NEW     0x8

struct part {
        bool    running;

	int	cpu;

        u64     period_start;
        u64     last_updated;
        u64     active_sum;

#define PART_HIST_SIZE_MAX      20
        int     hist_idx;
        int     hist[PART_HIST_SIZE_MAX];
        int     active_ratio_recent;
};

#ifdef CONFIG_SCHED_EMS
/*
 * core
 */
extern int
exynos_select_task_rq(struct task_struct *p, int prev_cpu, int sd_flag, int wake_flag);
extern int ems_can_migrate_task(struct task_struct *p, int dst_cpu);
extern void ems_tick(struct rq *rq);
extern void lb_newidle_balance(struct rq *dst_rq, void *rf_ptr, int *pulled_task, int *done);

/*
 * multi load
 */
extern void update_cpu_active_ratio(struct rq *rq, struct task_struct *p, int type);

/*
 * ontime migration
 */
extern void ontime_migration(void);


/*
 * Core sparing
 */
extern int ecs_cpu_available(int cpu, struct task_struct *p);

/*
 * Freqboost
 */
extern void freqboost_enqueue_task(struct task_struct *p, int cpu, int flags);
extern void freqboost_dequeue_task(struct task_struct *p, int cpu, int flags);
extern int freqboost_can_attach(struct cgroup_taskset *tset);

/*
 * Sysbusy
 */
extern int sysbusy_on_somac(void);

/*
 * Priority-pinning
 */
extern void prio_pinning_enqueue_task(struct task_struct *p, int cpu);
extern void prio_pinning_dequeue_task(struct task_struct *p, int cpu);
#else /* CONFIG_SCHED_EMS */

/*
 * core
 */
static inline int
exynos_select_task_rq(struct task_struct *p, int prev_cpu, int sd_flag, int wake_flag)
{
	return -1;
}
static inline int ems_can_migrate_task(struct task_struct *p, int dst_cpu) { return 1; }
static inline void ems_tick(struct rq *rq) { }

/*
 * multi load
 */
static inline void update_cpu_active_ratio(struct rq *rq, struct task_struct *p, int type) { }

/*
 * ontime migration
 */
static inline void ontime_migration(void) { }



/*
 * Core sparing
 */
static inline int ecs_cpu_available(int cpu, struct task_struct *p) { return 1; }

/*
 * Freqboost
 */
static inline bool freqboost_cpu_boost_group_active(int idx, int cpu, u64 now) { return false; }
static inline void freqboost_enqueue_task(struct task_struct *p, int cpu, int flags) { }
static inline void freqboost_dequeue_task(struct task_struct *p, int cpu, int flags) { }
static inline int freqboost_can_attach(struct cgroup_taskset *tset) { return 0; }
static inline int freqboost_cpu_boost(int cpu) { return 0; }
static inline long freqboost_margin(unsigned long capacity, unsigned long signal, long boost) { return 0; }
static inline void freqboost_boostgroup_init(void) { }
static inline void freqboost_boostgroup_release(struct cgroup_subsys_state *css) { }

/*
 * Sysbusy
 */
static inline int sysbusy_on_somac(void) { return 0; }

/*
 * Priority-pinning
 */
static inline void prio_pinning_enqueue_task(struct task_struct *p, int cpu) { }
static inline void prio_pinning_dequeue_task(struct task_struct *p, int cpu) { }
#endif /* CONFIG_SCHED_EMS */

/*
 * EMS Tune
 */
struct mllist_head {
	struct list_head node_list;
};

struct mllist_node {
	int			prio;
	struct list_head	prio_list;
	struct list_head	node_list;
};

struct emstune_mode_request {
	struct mllist_node node;
	bool active;
	struct delayed_work work; /* for emstune_update_request_timeout */
	char *func;
	unsigned int line;
};

#define BOOST_LEVEL	(0xB0057)

#if defined(CONFIG_SCHED_EMS)
extern void emstune_cpu_update(int cpu, u64 now);
extern unsigned long emstune_freq_boost(int cpu, unsigned long util);

extern int emstune_register_mode_update_notifier(struct notifier_block *nb);
extern int emstune_unregister_mode_update_notifier(struct notifier_block *nb);

extern int emstune_util_est_group(int st_idx);
#else
static inline void emstune_cpu_update(int cpu, u64 now) { };
static inline unsigned long emstune_freq_boost(int cpu, unsigned long util) { return util; };

static inline int emstune_register_mode_update_notifier(struct notifier_block *nb) { return 0; }
static inline int emstune_unregister_mode_update_notifier(struct notifier_block *nb) { return 0; }

static inline int emstune_util_est_group(int st_idx) { return 0; }
#endif /* CONFIG_SCHED_EMS && CONFIG_SCHED_TUNE */

/* Exynos Fluid Real Time Scheduler */
extern unsigned int frt_disable_cpufreq;

#ifdef CONFIG_SCHED_USE_FLUID_RT
extern int frt_find_lowest_rq(struct task_struct *task);
#endif

#if IS_ENABLED(CONFIG_EXYNOS_MSN)
extern int emstune_get_cur_mode(void);
extern void emstune_update_request(struct emstune_mode_request *req, s32 new_value);
extern void emstune_update_request_timeout(struct emstune_mode_request *req, s32 new_value,
					unsigned long timeout_us);

#define emstune_add_request(req)	do {				\
	__emstune_add_request(req, (char *)__func__, __LINE__);	\
} while(0);
extern void __emstune_add_request(struct emstune_mode_request *req, char *func, unsigned int line);
extern void emstune_remove_request(struct emstune_mode_request *req);

extern void emstune_boost(struct emstune_mode_request *req, int enable);
extern void emstune_boost_timeout(struct emstune_mode_request *req, unsigned long timeout_us);

extern void emstune_mode_change(int next_mode_idx);

extern int ecs_request_register(char *name, const struct cpumask *mask);
extern int ecs_request_unregister(char *name);
extern int ecs_request(char *name, const struct cpumask *mask);
#else
static inline int emstune_get_cur_mode(void) { return 0; }
static inline void emstune_update_request(struct emstune_mode_request *req, s32 new_value) { }
static inline void emstune_update_request_timeout(struct emstune_mode_request *req, s32 new_value,
					unsigned long timeout_us) { }

#define emstune_add_request(req)	do { } while(0);
static inline void __emstune_add_request(struct emstune_mode_request *req, char *func, unsigned int line) { }
static inline void emstune_remove_request(struct emstune_mode_request *req) { }

static inline void emstune_boost(struct emstune_mode_request *req, int enable) { }
static inline void emstune_boost_timeout(struct emstune_mode_request *req, unsigned long timeout_us) { }

static inline void emstune_mode_change(int next_mode_idx) { }

static inline int ecs_request_register(char *name, const struct cpumask *mask) { return 0; }
static inline int ecs_request_unregister(char *name) { return 0; }
static inline int ecs_request(char *name, const struct cpumask *mask) { return 0; }
#endif
