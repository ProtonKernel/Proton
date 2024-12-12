/*
 * Copyright (c) 2018 Samsung Electronics Co., Ltd
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

extern struct kobject *ems_kobj;
extern struct kobject *ems_drv_kobj;

/*
 * Maximum supported-by-vendor processors.  Setting this smaller saves quite a
 * bit of memory.  Use nr_cpu_ids instead of this except for static bitmaps.
 */
#ifndef CONFIG_VENDOR_NR_CPUS
/* FIXME: This should be fixed in the arch's Kconfig */
#define CONFIG_VENDOR_NR_CPUS	1
#endif

/* Places which use this should consider cpumask_var_t. */
#define VENDOR_NR_CPUS		CONFIG_VENDOR_NR_CPUS

#define IMPORT_PRIO	116

/* structure for task placement environment */
struct tp_env {
	struct task_struct *p;

	int sched_policy;

	struct cpumask cpus_allowed;
	struct cpumask fit_cpus;
	struct cpumask ontime_fit_cpus;
	struct cpumask candidates;
	struct cpumask idle_candidates;

	unsigned long eff_weight[VENDOR_NR_CPUS];	/* efficiency weight */

	int task_util;
	unsigned long cpu_util_wo[VENDOR_NR_CPUS];	/* for ISA util */
	unsigned long cpu_util_with[VENDOR_NR_CPUS];	/* ml_cpu_util_with */
	unsigned long cpu_util[VENDOR_NR_CPUS];		/* ml_cpu_util_with */
	unsigned long cpu_rt_util[VENDOR_NR_CPUS];	/* rt util */
	unsigned long nr_running[VENDOR_NR_CPUS];	/* nr_running */

	int wake;
};

/*
 * The compute capacity, power consumption at this compute capacity and
 * frequency of state. The cap and power are used to find the energy
 * efficiency cpu, and the frequency is used to create the capacity table.
 */
struct energy_state {
	unsigned long frequency;
	unsigned long cap;
	unsigned long power;

	/* for sse */
	unsigned long cap_s;
	unsigned long power_s;

	unsigned long static_power;
	unsigned long static_power_2nd;
};

/*
 * Each cpu can have its own mips_per_mhz, coefficient and energy table.
 * Generally, cpus in the same frequency domain have the same mips_per_mhz,
 * coefficient and energy table.
 */
struct energy_table {
	unsigned int mips_per_mhz;
	unsigned int coefficient;
	unsigned int mips_per_mhz_s;
	unsigned int coefficient_s;
	unsigned int static_coefficient;
	unsigned int static_coefficient_2nd;
	unsigned int wakeup_cost;

	struct energy_state *states;

	unsigned int nr_states;
	unsigned int nr_states_orig;
	unsigned int nr_states_requests[4];
};
extern DEFINE_PER_CPU(struct energy_table, energy_table);
#define get_energy_table(cpu)	&per_cpu(energy_table, cpu)
extern int default_get_next_cap(int dst_cpu, struct task_struct *p);

/* hook */
extern int hook_init(void);

/* energy model */
extern int energy_table_init(void);
extern unsigned long capacity_cpu_orig(int cpu);
extern unsigned long capacity_cpu(int cpu);

/* multi load */
extern void part_init(void);
extern unsigned long ml_task_util(struct task_struct *p);
extern unsigned long ml_task_util_est(struct task_struct *p);
extern unsigned long ml_boosted_task_util(struct task_struct *p);
extern unsigned long ml_cpu_util(int cpu);
extern unsigned long ml_cpu_util_next(int cpu, struct task_struct *p, int dst_cpu);
extern unsigned long ml_cpu_util_without(int cpu, struct task_struct *p);
extern unsigned long ml_boosted_cpu_util(int cpu);
extern int ml_task_hungry(struct task_struct *p);
extern void set_part_period_start(struct rq *rq);
extern int get_part_hist_idx(int);
extern int get_part_hist_value(int, int);

/* efficiency cpu selection */
extern int find_best_cpu(struct tp_env *env);
extern int find_wide_cpu(struct tp_env *env, int sch);

/* ontime migration */
struct ontime_dom {
	struct list_head	node;

	unsigned long		upper_boundary;
	unsigned long		lower_boundary;

	struct cpumask		cpus;
};

extern int ontime_init(void);
extern int ontime_can_migrate_task(struct task_struct *p, int dst_cpu);
extern void ontime_select_fit_cpus(struct task_struct *p, struct cpumask *fit_cpus);
extern unsigned long get_upper_boundary(int cpu, struct task_struct *p);

/* esg */
extern int esgov_pre_init(void);
extern int find_allowed_capacity(int cpu, unsigned int new, int power);
extern int find_step_power(int cpu, int step);
extern int get_gov_next_cap(int grp_cpu, int dst_cpu, struct tp_env *env);
extern unsigned int get_diff_num_levels(int cpu, unsigned int freq1, unsigned int freq2);

/* freqboost */
extern int freqboost_init(void);
extern long freqboost_margin(unsigned long capacity, unsigned long signal, long boost);
extern int freqboost_cpu_boost(int cpu);
extern bool freqboost_cpu_boost_group_active(int idx, int cpu, u64 now);
extern int freqboost_wakeup_boost(int cpu, int util);
extern int freqboost_wakeup_boost_pending(int cpu);

/* frt */
extern int frt_init(void);
extern void frt_update_available_cpus(struct rq *rq);

/* core sparing */
struct ecs_stage {
	struct list_head	node;

	unsigned int		id;
	unsigned int		busy_threshold;
	unsigned int		busy_threshold_orig;

	struct cpumask		cpus;
	struct cpumask		monitor_cpus;
};

extern int ecs_init(void);
extern const struct cpumask *ecs_cpus_allowed(struct task_struct *p);
extern void ecs_update(void);

/* EMSTune */
enum stune_group {
	STUNE_ROOT,
	STUNE_FOREGROUND,
	STUNE_BACKGROUND,
	STUNE_TOPAPP,
	STUNE_GROUP_COUNT,
};

/* emstune - sched policy */
extern int emstune_init(void);

struct emstune_sched_policy {
	bool overriding;
	int enabled[STUNE_GROUP_COUNT];
	int policy[STUNE_GROUP_COUNT];
};

/* emstune - weight */
struct emstune_weight {
	bool overriding;
	int ratio[STUNE_GROUP_COUNT][VENDOR_NR_CPUS];
};

/* emstune - idle weight */
struct emstune_idle_weight {
	bool overriding;
	int ratio[STUNE_GROUP_COUNT][VENDOR_NR_CPUS];
};

/* emstune - freq boost */
struct emstune_freq_boost {
	bool overriding;
	int ratio[STUNE_GROUP_COUNT][VENDOR_NR_CPUS];
};

/* emstune - wakeup boost */
struct emstune_wakeup_boost {
	bool overriding;
	int ratio[STUNE_GROUP_COUNT][VENDOR_NR_CPUS];
};

/* emstune - energy step governor */
struct emstune_esg {
	bool overriding;
	int step[VENDOR_NR_CPUS];
	int patient_mode[VENDOR_NR_CPUS];
	int pelt_margin[VENDOR_NR_CPUS];
	int pelt_boost[VENDOR_NR_CPUS];

	int up_rate_limit;
	int down_rate_limit;
	int rapid_scale_up;
	int rapid_scale_down;
};

/* emstune - ontime migration */
struct emstune_ontime {
	bool overriding;
	int enabled[STUNE_GROUP_COUNT];
	struct list_head *p_dom_list;
	struct list_head dom_list;
};

/* emstune - cgroup boosted */
struct emstune_tiny_cd_sched {
	bool overriding;
	int enabled[STUNE_GROUP_COUNT];
};

/* emstune - priority pinning */
struct emstune_prio_pinning {
	bool overriding;
	struct cpumask mask;
	int enabled[STUNE_GROUP_COUNT];
	int prio;
};

/* SCHED CLASS */
#define EMS_SCHED_STOP		(1 << 0)
#define EMS_SCHED_DL		(1 << 1)
#define EMS_SCHED_RT		(1 << 2)
#define EMS_SCHED_FAIR		(1 << 3)
#define EMS_SCHED_IDLE		(1 << 4)
#define NUM_OF_SCHED_CLASS	5

#define EMS_SCHED_CLASS_MASK	(0x1F)

/* emstune - cpus allowed */
struct emstune_cpus_allowed {
	bool overriding;
	unsigned long target_sched_class;
	struct cpumask mask[STUNE_GROUP_COUNT];
};

/* emstune - fluid rt */
struct emstune_frt {
	bool overriding;
	int active_ratio[VENDOR_NR_CPUS];
};

/* emstune - core sparing */
struct emstune_ecs {
	bool overriding;
	struct list_head *p_stage_list;
	struct list_head stage_list;
};

struct emstune_set {
	int				idx;
	const char			*desc;
	int				unique_id;

	struct emstune_sched_policy	sched_policy;
	struct emstune_weight		weight;
	struct emstune_idle_weight	idle_weight;
	struct emstune_freq_boost	freq_boost;
	struct emstune_wakeup_boost	wakeup_boost;
	struct emstune_esg		esg;
	struct emstune_ontime		ontime;
	struct emstune_prio_pinning	prio_pinning;
	struct emstune_cpus_allowed	cpus_allowed;
	struct emstune_frt		frt;
	struct emstune_ecs		ecs;
	struct emstune_tiny_cd_sched	tiny_cd_sched;

	struct kobject	  		kobj;
};

#define MAX_MODE_LEVEL	100

struct emstune_mode {
	int				idx;
	const char			*desc;

	struct emstune_set		sets[MAX_MODE_LEVEL];
	int				boost_level;
};

extern bool emstune_can_migrate_task(struct task_struct *p, int dst_cpu);
extern int emstune_eff_weight(struct task_struct *p, int cpu, int idle);
extern const struct cpumask *emstune_cpus_allowed(struct task_struct *p);
extern int emstune_sched_policy(struct task_struct *p);
extern int emstune_ontime(struct task_struct *p);
extern int emstune_tiny_cd_sched(struct task_struct *p);
extern int emstune_util_est(struct task_struct *p);
extern int emstune_init_util(struct task_struct *p);
extern int emstune_wakeup_boost(int group, int cpu);

extern int need_prio_pinning(struct task_struct *p);
extern int prio_pinning_schedule(struct tp_env *env, int prev_cpu);
extern struct cpumask *get_prio_pinning_assigned_mask(void);

static inline int cpu_overutilized(unsigned long capacity, unsigned long util)
{
	return (capacity * 1024) < (util * 1280);
}

static inline struct task_struct *task_of(struct sched_entity *se)
{
	return container_of(se, struct task_struct, se);
}

#ifdef CONFIG_FAIR_GROUP_SCHED
#define entity_is_task(se)	(!se->my_q)
#else
#define entity_is_task(se)	1
#endif

static inline struct sched_entity *get_task_entity(struct sched_entity *se)
{
#ifdef CONFIG_FAIR_GROUP_SCHED
	struct cfs_rq *cfs_rq = se->my_q;

	while (cfs_rq) {
		se = cfs_rq->curr;
		cfs_rq = se->my_q;
	}
#endif

	return se;
}

static inline struct cfs_rq *cfs_rq_of(struct sched_entity *se)
{
#ifdef CONFIG_FAIR_GROUP_SCHED
	return se->cfs_rq;
#else
	return &task_rq(task_of(se))->cfs;
#endif
}

static inline bool is_kthread_per_cpu(struct task_struct *p)
{
	if (!(p->flags & PF_KTHREAD))
		return false;

	if (p->nr_cpus_allowed != 1)
		return false;

	return true;
}

static inline bool is_dst_allowed(struct task_struct *p, int cpu)
{
	if (is_kthread_per_cpu(p) &&
		(cpu != cpumask_any(p->cpus_ptr)))
		return false;

	return cpu_active(cpu);
}

/* Sysbusy */
extern int sysbusy_init(void);
extern void monitor_sysbusy(void);
extern int sysbusy_schedule(struct tp_env *env);
extern int is_sysbusy(void);
extern void somac_tasks(void);

/* declare extern function from sched */
extern u64 decay_load(u64 val, u64 n);
extern u32 __accumulate_pelt_segments(u64 periods, u32 d1, u32 d3);
extern int cpuctl_task_group_idx(struct task_struct *p);
extern struct sched_entity *__pick_next_entity(struct sched_entity *se);

#ifdef CONFIG_UCLAMP_TASK
static inline unsigned long ml_uclamp_task_util(struct task_struct *p)
{
	return clamp(ml_task_util_est(p),
			uclamp_eff_value(p, UCLAMP_MIN),
			uclamp_eff_value(p, UCLAMP_MAX));
}
#else
static inline unsigned long ml_uclamp_task_util(struct task_strcut *p)
{
	return ml_task_util_est(p);
}
#endif
