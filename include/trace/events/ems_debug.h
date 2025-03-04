/*
 *  Copyright (C) 2019 Park Bumgyu <bumgyu.park@samsung.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#undef TRACE_SYSTEM
#define TRACE_SYSTEM ems_debug

#if !defined(_TRACE_EMS_DEBUG_H) || defined(TRACE_HEADER_MULTI_READ)
#define _TRACE_EMS_DEBUG_H

#include <linux/tracepoint.h>

struct part;
struct rq;

#define LABEL_LEN 32

TRACE_EVENT(ems_monitor_sysbusy,

	TP_PROTO(int cpu, unsigned long cpu_util),

	TP_ARGS(cpu, cpu_util),

	TP_STRUCT__entry(
		__field(	int,		cpu			)
		__field(	unsigned long,	cpu_util		)
	),

	TP_fast_assign(
		__entry->cpu			= cpu;
		__entry->cpu_util		= cpu_util;
	),

	TP_printk("cpu=%d cpu_util=%ld", __entry->cpu, __entry->cpu_util)
);

TRACE_EVENT(ems_heavy_tasks_tracking,

	TP_PROTO(int tsk_cnt, unsigned long util_sum),

	TP_ARGS(tsk_cnt, util_sum),

	TP_STRUCT__entry(
		__field(	int,		tsk_cnt			)
		__field(	unsigned long,	util_sum		)
	),

	TP_fast_assign(
		__entry->tsk_cnt		= tsk_cnt;
		__entry->util_sum		= util_sum;
	),

	TP_printk("heavy_tsk_cnt=%d heavy_util_sum=%ld", __entry->tsk_cnt, __entry->util_sum)
);

/*
 * Tracepoint for multiple test
 */
TRACE_EVENT(ems_test,

	TP_PROTO(struct task_struct *p, int cpu, int state, char *event),

	TP_ARGS(p, cpu, state, event),

	TP_STRUCT__entry(
		__array(	char,		comm,	TASK_COMM_LEN	)
		__field(	pid_t,		pid			)
		__field(	int,		cpu			)
		__field(	int,		state			)
		__array(	char,		event,	64		)
	),

	TP_fast_assign(
		memcpy(__entry->comm, p ? p->comm : "no task", TASK_COMM_LEN);
		__entry->pid			= p ? p->pid : -1;
		__entry->cpu			= cpu;
		__entry->state			= state;
		strncpy(__entry->event, event ? event : "no event", 63);
	),

	TP_printk("comm=%s pid=%d cpu=%d state=%d event=%s",
		  __entry->comm, __entry->pid, __entry->cpu, __entry->state, __entry->event)
);

/*
 * Tracepoint for capacity difference scheduling
 */
TRACE_EVENT(ems_cap_difference,

	TP_PROTO(struct task_struct *p, int cpu, int cur_cap, int next_cap),

	TP_ARGS(p, cpu, cur_cap, next_cap),

	TP_STRUCT__entry(
		__array(	char,		comm,	TASK_COMM_LEN	)
		__field(	pid_t,		pid			)
		__field(	int,		cpu			)
		__field(	int,		cur_cap			)
		__field(	int,		next_cap		)
	),

	TP_fast_assign(
		memcpy(__entry->comm, p ? p->comm : "no task", TASK_COMM_LEN);
		__entry->pid			= p ? p->pid : -1;
		__entry->cpu			= cpu;
		__entry->cur_cap		= cur_cap;
		__entry->next_cap		= next_cap;
	),

	TP_printk("comm=%s pid=%d cpu=%d cur_cap=%d next_cap=%d",
		  __entry->comm, __entry->pid, __entry->cpu,
		  __entry->cur_cap, __entry->next_cap)
);

/*
 * Tracepoint for wakeup balance
 */
TRACE_EVENT(ems_select_fit_cpus,

	TP_PROTO(struct task_struct *p, int wake,
		unsigned int fit_cpus, unsigned int cpus_allowed, unsigned int ontime_fit_cpus,
		unsigned int overcap_cpus, unsigned int busy_cpus),

	TP_ARGS(p, wake, fit_cpus, cpus_allowed, ontime_fit_cpus,
				overcap_cpus, busy_cpus),

	TP_STRUCT__entry(
		__array(	char,		comm,	TASK_COMM_LEN	)
		__field(	pid_t,		pid			)
		__field(	int,		src_cpu			)
		__field(	unsigned long,	task_util		)
		__field(	int,		wake			)
		__field(	unsigned int,	fit_cpus		)
		__field(	unsigned int,	cpus_ptr		)
		__field(	unsigned int,	cpus_allowed		)
		__field(	unsigned int,	ontime_fit_cpus		)
		__field(	unsigned int,	overcap_cpus		)
		__field(	unsigned int,	busy_cpus		)
	),

	TP_fast_assign(
		memcpy(__entry->comm, p->comm, TASK_COMM_LEN);
		__entry->pid			= p->pid;
		__entry->src_cpu		= task_cpu(p);
		__entry->task_util		= p->se.avg.util_avg;
		__entry->wake			= wake;
		__entry->fit_cpus		= fit_cpus;
		__entry->cpus_allowed		= cpus_allowed;
		__entry->cpus_ptr		= *(unsigned int *)cpumask_bits(p->cpus_ptr);
		__entry->ontime_fit_cpus	= ontime_fit_cpus;
		__entry->overcap_cpus		= overcap_cpus;
		__entry->busy_cpus		= busy_cpus;
	),

	TP_printk("comm=%s pid=%d src_cpu=%d task_util=%lu wake=%d fit_cpus=%#x cpus_ptr=%#x cpus_allowed=%#x ontime_fit_cpus=%#x overcap_cpus=%#x busy_cpus=%#x",
		  __entry->comm, __entry->pid, __entry->src_cpu, __entry->task_util,
		  __entry->wake, __entry->fit_cpus, __entry->cpus_ptr,__entry->cpus_allowed, __entry->ontime_fit_cpus,
		  __entry->overcap_cpus, __entry->busy_cpus)
);

/*
 * Tracepoint for candidate cpu
 */
TRACE_EVENT(ems_candidates,

	TP_PROTO(struct task_struct *p, int sched_policy,
		unsigned int candidates, unsigned int idle_candidates),

	TP_ARGS(p, sched_policy, candidates, idle_candidates),

	TP_STRUCT__entry(
		__array(	char,		comm,	TASK_COMM_LEN	)
		__field(	pid_t,		pid			)
		__field(	int,		sched_policy		)
		__field(	unsigned int,	candidates		)
		__field(	unsigned int,	idle_candidates		)
	),

	TP_fast_assign(
		memcpy(__entry->comm, p->comm, TASK_COMM_LEN);
		__entry->pid			= p->pid;
		__entry->sched_policy		= sched_policy;
		__entry->candidates		= candidates;
		__entry->idle_candidates	= idle_candidates;
	),

	TP_printk("comm=%s pid=%d sched_policy=%d candidates=%#x idle_candidates=%#x",
		  __entry->comm, __entry->pid, __entry->sched_policy,
		  __entry->candidates, __entry->idle_candidates)
);

TRACE_EVENT(ems_snapshot_cpu,

	TP_PROTO(int cpu, unsigned int task_util, unsigned int util_wo,
		unsigned int util_with, unsigned int rt_util,
		unsigned int util, unsigned int nr_running),

	TP_ARGS(cpu, task_util, util_wo, util_with, rt_util,
		util, nr_running),

	TP_STRUCT__entry(
		__field(	unsigned int,	cpu)
		__field(	unsigned int,	task_util)
		__field(	unsigned int,	util_wo)
		__field(	unsigned int,	util_with)
		__field(	unsigned int,	rt_util)
		__field(	unsigned int,	util)
		__field(	unsigned int,	nr_running)
	),

	TP_fast_assign(
		__entry->cpu       		= cpu;
		__entry->task_util		= task_util;
		__entry->util_wo   		= util_wo;
		__entry->util_with 		= util_with;
		__entry->rt_util   		= rt_util;
		__entry->util			= util;
		__entry->nr_running		= nr_running;
	),

	TP_printk("cpu%u tsk=%u u_wo=%u u_with=%u u_rt=%u util=%u nr=%u",
		  __entry->cpu, __entry->task_util, __entry->util_wo,
		  __entry->util_with, __entry->rt_util,
		  __entry->util, __entry->nr_running)
);

/*
 * Tracepoint for computing energy
 */
TRACE_EVENT(ems_energy_candidates,

	TP_PROTO(struct task_struct *p, unsigned int candidates),

	TP_ARGS(p, candidates),

	TP_STRUCT__entry(
		__array(	char,		comm,	TASK_COMM_LEN	)
		__field(	pid_t,		pid			)
		__field(	unsigned int,	candidates		)
	),

	TP_fast_assign(
		memcpy(__entry->comm, p->comm, TASK_COMM_LEN);
		__entry->pid			= p->pid;
		__entry->candidates		= candidates;
	),

	TP_printk("comm=%s pid=%d candidates=%#x",
		  __entry->comm, __entry->pid, __entry->candidates)
);

TRACE_EVENT(ems_task_blocked,

	TP_PROTO(struct task_struct *p, int grp, int thr, int nr, int idle_cpu),

	TP_ARGS(p, grp, thr, nr, idle_cpu),

	TP_STRUCT__entry(
		__array(	char,		comm,	TASK_COMM_LEN	)
		__field(	pid_t,		pid			)
		__field(	int,		grp			)
		__field(	int,		thr			)
		__field(	int,		nr			)
		__field(	int,		idle_cpu		)
	),

	TP_fast_assign(
		memcpy(__entry->comm, p ? p->comm : "no task", TASK_COMM_LEN);
		__entry->pid			= p ? p->pid : -1;
		__entry->grp			= grp;
		__entry->thr			= thr;
		__entry->nr			= nr;
		__entry->idle_cpu		= idle_cpu;
	),

	TP_printk("comm=%s pid=%d grp_cpu=%d nr_thr=%d nr=%d idle_cpu=%d",
		__entry->comm, __entry->pid, __entry->grp, __entry->thr,
		__entry->nr, __entry->idle_cpu)
);

TRACE_EVENT(ems_tiny_task_select,

	TP_PROTO(struct task_struct *p, int energy_cpu, int idle_cpu),

	TP_ARGS(p, energy_cpu, idle_cpu),

	TP_STRUCT__entry(
		__array(	char,		comm,	TASK_COMM_LEN	)
		__field(	pid_t,		pid			)
		__field(	int,		energy_cpu		)
		__field(	int,		idle_cpu		)
	),

	TP_fast_assign(
		memcpy(__entry->comm, p ? p->comm : "no task", TASK_COMM_LEN);
		__entry->pid			= p ? p->pid : -1;
		__entry->energy_cpu		= energy_cpu;
		__entry->idle_cpu		= idle_cpu;
	),

	TP_printk("comm=%s pid=%d energy_cpu=%d idle_cpu=%d",
		  __entry->comm, __entry->pid, __entry->energy_cpu, __entry->idle_cpu)
);


TRACE_EVENT(ems_compute_energy,

	TP_PROTO(struct task_struct *p, int tsk_util, int weight, int dst, int cpu,
		unsigned int util, unsigned int cap,
		unsigned int power, unsigned int st_power, unsigned int energy),

	TP_ARGS(p, tsk_util, weight, dst, cpu, util, cap, power, st_power, energy),

	TP_STRUCT__entry(
		__array(	char,		comm,	TASK_COMM_LEN	)
		__field(	pid_t,		pid			)
		__field(	unsigned long,	tsk_util		)
		__field(	int,		weight			)
		__field(	int,		src			)
		__field(	int,		dst			)
		__field(	int,		cpu			)
		__field(	unsigned int,	util			)
		__field(	unsigned int,	cap			)
		__field(	unsigned int,	power			)
		__field(	unsigned int,	st_power		)
		__field(	unsigned int,	energy			)
	),

	TP_fast_assign(
		memcpy(__entry->comm, p->comm, TASK_COMM_LEN);
		__entry->pid		= p->pid;
		__entry->tsk_util	= tsk_util;
		__entry->weight		= weight;
		__entry->src		= p->cpu;
		__entry->dst		= dst;
		__entry->cpu		= cpu;
		__entry->util		= util;
		__entry->cap		= cap;
		__entry->power		= power;
		__entry->st_power	= st_power;
		__entry->energy		= energy;
	),

	TP_printk("comm=%s pid=%d tsk_ut=%lu(w %d) cpu%d->%d cpu%d ut=%u cap=%u pwr=%u st_pwr=%u e=%u",
		__entry->comm, __entry->pid, __entry->tsk_util, __entry->weight,
		__entry->src, __entry->dst, __entry->cpu,  __entry->util,
		__entry->cap,__entry->power, __entry->st_power, __entry->energy)
);

/*
 * Tracepoint for computing energy/capacity efficiency
 */
TRACE_EVENT(ems_compute_eff,

	TP_PROTO(struct task_struct *p, int cpu,
		unsigned long cpu_util_with, unsigned int eff_weight,
		unsigned long capacity, unsigned long energy, unsigned int eff),

	TP_ARGS(p, cpu, cpu_util_with, eff_weight, capacity, energy, eff),

	TP_STRUCT__entry(
		__array(	char,		comm,	TASK_COMM_LEN	)
		__field(	pid_t,		pid			)
		__field(	int,		cpu			)
		__field(	unsigned long,	task_util		)
		__field(	unsigned long,	cpu_util_with		)
		__field(	unsigned int,	eff_weight		)
		__field(	unsigned long,	capacity		)
		__field(	unsigned long,	energy			)
		__field(	unsigned int,	eff			)
	),

	TP_fast_assign(
		memcpy(__entry->comm, p->comm, TASK_COMM_LEN);
		__entry->pid		= p->pid;
		__entry->cpu		= cpu;
		__entry->task_util	= p->se.avg.util_avg;
		__entry->cpu_util_with	= cpu_util_with;
		__entry->eff_weight	= eff_weight;
		__entry->capacity	= capacity;
		__entry->energy		= energy;
		__entry->eff		= eff;
	),

	TP_printk("comm=%s pid=%d cpu=%d task_util=%lu cpu_util_with=%lu eff_weight=%u capacity=%lu energy=%lu eff=%u",
		__entry->comm, __entry->pid, __entry->cpu,
		__entry->task_util, __entry->cpu_util_with,
		__entry->eff_weight, __entry->capacity,
		__entry->energy, __entry->eff)
);

/*
 * Tracepoint for computing performance
 */
TRACE_EVENT(ems_compute_perf,

	TP_PROTO(struct task_struct *p, int cpu,
		unsigned int capacity, int idle_state),

	TP_ARGS(p, cpu, capacity, idle_state),

	TP_STRUCT__entry(
		__array(	char,		comm,	TASK_COMM_LEN	)
		__field(	pid_t,		pid			)
		__field(	int,		cpu			)
		__field(	unsigned int,	capacity		)
		__field(	int,		idle_state		)
	),

	TP_fast_assign(
		memcpy(__entry->comm, p->comm, TASK_COMM_LEN);
		__entry->pid		= p->pid;
		__entry->cpu		= cpu;
		__entry->capacity	= capacity;
		__entry->idle_state	= idle_state;
	),

	TP_printk("comm=%s pid=%d cpu=%d capacity=%u idle_state=%d",
		__entry->comm, __entry->pid, __entry->cpu,
		__entry->capacity, __entry->idle_state)
);

/*
 * Tracepoint for system busy boost
 */
TRACE_EVENT(ems_sysbusy_boost,

	TP_PROTO(int boost),

	TP_ARGS(boost),

	TP_STRUCT__entry(
		__field(int,	boost)
	),

	TP_fast_assign(
		__entry->boost	= boost;
	),

	TP_printk("boost=%d", __entry->boost)
);

/*
 * Tracepoint for step_util in ESG
 */
TRACE_EVENT(esg_cpu_step_util,

	TP_PROTO(int cpu, int allowed_cap, int active_ratio, int max, int util),

	TP_ARGS(cpu, allowed_cap, active_ratio, max, util),

	TP_STRUCT__entry(
		__field( int,		cpu				)
		__field( int,		allowed_cap			)
		__field( int,		active_ratio			)
		__field( int,		max				)
		__field( int,		util				)
	),

	TP_fast_assign(
		__entry->cpu			= cpu;
		__entry->allowed_cap		= allowed_cap;
		__entry->active_ratio		= active_ratio;
		__entry->max			= max;
		__entry->util			= util;
	),

	TP_printk("cpu=%d allowed_cap=%d active_ratio=%d, max=%d, util=%d",
			__entry->cpu, __entry->allowed_cap, __entry->active_ratio,
			__entry->max, __entry->util)
);

/*
 * Tracepoint for util in ESG
 */
TRACE_EVENT(esg_cpu_util,

	TP_PROTO(int cpu, int nr_diff, int org_io_util, int io_util,
		int org_step_util, int step_util, int org_pelt_util, int pelt_util,
		int pelt_margin, int pelt_boost, int max, int util),

	TP_ARGS(cpu, nr_diff, org_io_util, io_util,
		org_step_util, step_util, org_pelt_util, pelt_util, pelt_margin, pelt_boost, max, util),

	TP_STRUCT__entry(
		__field( int,		cpu				)
		__field( int,		nr_diff				)
		__field( int,		org_io_util			)
		__field( int,		io_util				)
		__field( int,		org_step_util			)
		__field( int,		step_util			)
		__field( int,		org_pelt_util			)
		__field( int,		pelt_util			)
		__field( int,		pelt_margin			)
		__field( int,		pelt_boost			)
		__field( int,		max				)
		__field( int,		util				)
	),

	TP_fast_assign(
		__entry->cpu			= cpu;
		__entry->nr_diff		= nr_diff;
		__entry->org_io_util		= org_io_util;
		__entry->io_util		= io_util;
		__entry->org_step_util		= org_step_util;
		__entry->step_util		= step_util;
		__entry->org_pelt_util		= org_pelt_util;
		__entry->pelt_util		= pelt_util;
		__entry->pelt_margin		= pelt_margin;
		__entry->pelt_boost		= pelt_boost;
		__entry->max			= max;
		__entry->util			= util;
	),

	TP_printk("cpu=%d nr_dif=%d, io_ut=%d->%d, step_ut=%d->%d pelt_ut=%d->%d(mg=%d boost=%d) max=%d, ut=%d",
			__entry->cpu, __entry->nr_diff,
			__entry->org_io_util, __entry->io_util,
			__entry->org_step_util, __entry->step_util,
			__entry->org_pelt_util, __entry->pelt_util,
			__entry->pelt_margin, __entry->pelt_boost,
			__entry->max, __entry->util)
);

/*
 * Tracepoint for slack timer func called
 */
TRACE_EVENT(esgov_slack_func,

	TP_PROTO(int cpu),

	TP_ARGS(cpu),

	TP_STRUCT__entry(
		__field(int, cpu)
	),

	TP_fast_assign(
		__entry->cpu = cpu;
	),

	TP_printk("cpu=%d SLACK EXPIRED", __entry->cpu)
);

/*
 * Tracepoint for detalis of slack timer func called
 */
TRACE_EVENT(esgov_slack,

	TP_PROTO(int cpu, unsigned long util,
		unsigned long min, unsigned long action, int ret),

	TP_ARGS(cpu, util, min, action, ret),

	TP_STRUCT__entry(
		__field(int, cpu)
		__field(unsigned long, util)
		__field(unsigned long, min)
		__field(unsigned long, action)
		__field(int, ret)
	),

	TP_fast_assign(
		__entry->cpu = cpu;
		__entry->util = util;
		__entry->min = min;
		__entry->action = action;
		__entry->ret = ret;
	),

	TP_printk("cpu=%d util=%ld min=%ld action=%ld ret=%d", __entry->cpu,
			__entry->util, __entry->min, __entry->action, __entry->ret)
);

/*
 * Tracepoint for wakeup boost
 */
TRACE_EVENT(freqboost_wakeup_boost,

	TP_PROTO(int cpu, int ratio, unsigned long util, unsigned long boosted_util),

	TP_ARGS(cpu, ratio, util, boosted_util),

	TP_STRUCT__entry(
		__field( int,		cpu			)
		__field( int,		ratio			)
		__field( unsigned long,	util			)
		__field( unsigned long,	boosted_util		)
	),

	TP_fast_assign(
		__entry->cpu			= cpu;
		__entry->ratio			= ratio;
		__entry->util			= util;
		__entry->boosted_util		= boosted_util;
	),

	TP_printk("cpu=%d ratio=%d util=%lu boosted_util=%lu",
		  __entry->cpu, __entry->ratio, __entry->util, __entry->boosted_util)
);

/*
 * Tracepoint for available cpus in FRT
 */
TRACE_EVENT(frt_available_cpus,

	TP_PROTO(int cpu, int util_sum, int busy_thr, unsigned int available_mask),

	TP_ARGS(cpu, util_sum, busy_thr, available_mask),

	TP_STRUCT__entry(
		__field(	int,		cpu		)
		__field(	int,		util_sum	)
		__field(	int,		busy_thr	)
		__field(	unsigned int,	available_mask	)
	),

	TP_fast_assign(
		__entry->cpu		= cpu;
		__entry->util_sum	= util_sum;
		__entry->busy_thr	= busy_thr;
		__entry->available_mask	= available_mask;
	),

	TP_printk("cpu=%d util_sum=%d busy_thr=%d available_mask=%x",
		__entry->cpu,__entry->util_sum,
		__entry->busy_thr, __entry->available_mask)
);

/*
 * Tracepoint for active ratio tracking
 */
TRACE_EVENT(multi_load_update_cpu_active_ratio,

	TP_PROTO(int cpu, struct part *pa, char *event),

	TP_ARGS(cpu, pa, event),

	TP_STRUCT__entry(
		__field( int,	cpu				)
		__field( u64,	start				)
		__field( int,	recent				)
		__field( int,	last				)
		__array( char,	event,		64		)
	),

	TP_fast_assign(
		__entry->cpu			= cpu;
		__entry->start			= pa->period_start;
		__entry->recent			= pa->active_ratio_recent;
		__entry->last			= pa->hist[pa->hist_idx];
		strncpy(__entry->event, event, 63);
	),

	TP_printk("cpu=%d start=%llu recent=%4d last=%4d event=%s",
		  __entry->cpu, __entry->start, __entry->recent,
		  __entry->last, __entry->event)
);

/*
 * Tracepoint for active ratio
 */
TRACE_EVENT(multi_load_cpu_active_ratio,

	TP_PROTO(int cpu, unsigned long part_util, unsigned long pelt_util),

	TP_ARGS(cpu, part_util, pelt_util),

	TP_STRUCT__entry(
		__field( int,		cpu					)
		__field( unsigned long,	part_util				)
		__field( unsigned long,	pelt_util				)
	),

	TP_fast_assign(
		__entry->cpu			= cpu;
		__entry->part_util		= part_util;
		__entry->pelt_util		= pelt_util;
	),

	TP_printk("cpu=%d part_util=%lu pelt_util=%lu",
			__entry->cpu, __entry->part_util, __entry->pelt_util)
);

/*
 * Tracepoint for task migration control
 */
TRACE_EVENT(ontime_can_migrate_task,

	TP_PROTO(struct task_struct *tsk, int dst_cpu, int migrate, char *label),

	TP_ARGS(tsk, dst_cpu, migrate, label),

	TP_STRUCT__entry(
		__array( char,		comm,	TASK_COMM_LEN	)
		__field( pid_t,		pid			)
		__field( int,		src_cpu			)
		__field( int,		dst_cpu			)
		__field( int,		migrate			)
		__array( char,		label,	64		)
	),

	TP_fast_assign(
		memcpy(__entry->comm, tsk->comm, TASK_COMM_LEN);
		__entry->pid			= tsk->pid;
		__entry->src_cpu		= task_cpu(tsk);
		__entry->dst_cpu		= dst_cpu;
		__entry->migrate		= migrate;
		strncpy(__entry->label, label, 63);
	),

	TP_printk("comm=%s pid=%d src_cpu=%d dst_cpu=%d migrate=%d reason=%s",
		__entry->comm, __entry->pid, __entry->src_cpu, __entry->dst_cpu,
		__entry->migrate, __entry->label)
);

/*
 * Tracepoint for updating spared cpus
 */
TRACE_EVENT(ecs_update_stage,

	TP_PROTO(char *move, unsigned int id,
		unsigned int util_sum, unsigned int threshold,
		unsigned int prev_cpus, unsigned int next_cpus),

	TP_ARGS(move, id, util_sum, threshold, prev_cpus, next_cpus),

	TP_STRUCT__entry(
		__array(	char,		move,	64		)
		__field(	unsigned int,	id			)
		__field(	unsigned int,	util_sum		)
		__field(	unsigned int,	threshold		)
		__field(	unsigned int,	prev_cpus		)
		__field(	unsigned int,	next_cpus		)
	),

	TP_fast_assign(
		strncpy(__entry->move, move, 63);
		__entry->id			= id;
		__entry->util_sum		= util_sum;
		__entry->threshold		= threshold;
		__entry->prev_cpus		= prev_cpus;
		__entry->next_cpus		= next_cpus;
	),

	TP_printk("move=%s id=%d util_sum=%u threshold=%u cpus=%#x->%#x",
		__entry->move, __entry->id,
		__entry->util_sum, __entry->threshold,
		__entry->prev_cpus, __entry->next_cpus)
);

/*
 * Tracepoint for frequency boost
 */
TRACE_EVENT(emstune_freq_boost,

	TP_PROTO(int cpu, int ratio, unsigned long util, unsigned long boosted_util),

	TP_ARGS(cpu, ratio, util, boosted_util),

	TP_STRUCT__entry(
		__field( int,		cpu			)
		__field( int,		ratio			)
		__field( unsigned long,	util			)
		__field( unsigned long,	boosted_util		)
	),

	TP_fast_assign(
		__entry->cpu			= cpu;
		__entry->ratio			= ratio;
		__entry->util			= util;
		__entry->boosted_util		= boosted_util;
	),

	TP_printk("cpu=%d ratio=%d util=%lu boosted_util=%lu",
		  __entry->cpu, __entry->ratio, __entry->util, __entry->boosted_util)
);

/*
 * Tracepint for PMU Contention AVG
 */
TRACE_EVENT(cont_crossed_thr,

	TP_PROTO(int cpu, u64 event_thr, u64 event_ratio, u64 active_thr, u64 active_ratio),

	TP_ARGS(cpu, event_thr, event_ratio, active_thr, active_ratio),

	TP_STRUCT__entry(
		__field(	int,		cpu			)
		__field(	u64,		event_thr		)
		__field(	u64,		event_ratio		)
		__field(	u64,		active_thr		)
		__field(	u64,		active_ratio		)
	),

	TP_fast_assign(
		__entry->cpu		= cpu;
		__entry->event_thr	= event_thr;
		__entry->event_ratio	= event_ratio;
		__entry->active_thr	= active_thr;
		__entry->active_ratio	= active_ratio;
	),

	TP_printk("cpu=%d  event_thr=%lu, event_ratio=%lu active_thr=%lu active_ratio=%lu",
		__entry->cpu, __entry->event_thr, __entry->event_ratio,
		__entry->active_thr, __entry->active_ratio)
);

TRACE_EVENT(cont_distribute_pmu_count,

	TP_PROTO(int cpu, u64 elapsed, u64 period_count,
			u64 curr0, u64 curr1,
			u64 prev0, u64 prev1,
			u64 diff0, u64 diff1, int event),

	TP_ARGS(cpu, elapsed, period_count,
		curr0, curr1, prev0, prev1, diff0, diff1, event),

	TP_STRUCT__entry(
		__field(	int,		cpu			)
		__field(	u64,		elapsed			)
		__field(	u64,		period_count		)
		__field(	u64,		curr0			)
		__field(	u64,		curr1			)
		__field(	u64,		prev0			)
		__field(	u64,		prev1			)
		__field(	u64,		diff0			)
		__field(	u64,		diff1			)
		__field(	int,		event			)
	),

	TP_fast_assign(
		__entry->cpu		= cpu;
		__entry->elapsed	= elapsed;
		__entry->period_count	= period_count;
		__entry->curr0		= curr0;
		__entry->curr1		= curr1;
		__entry->prev0		= prev0;
		__entry->prev1		= prev1;
		__entry->diff0		= diff0;
		__entry->diff1		= diff1;
		__entry->event		= event;
	),

	TP_printk("cpu=%d elapsed=%lu, pcnt=%lu, c0=%lu c1=%lu p0=%lu p1=%lu d0=%lu d1=%lu event=%d",
		__entry->cpu, __entry->elapsed, __entry->period_count,
		__entry->curr0, __entry->curr1,
		__entry->prev0, __entry->prev1,
		__entry->diff0, __entry->diff1, __entry->event)
);

/*
 * Tracepoint for cfs_rq load tracking:
 */
TRACE_EVENT(sched_load_cfs_rq,

	TP_PROTO(struct cfs_rq *cfs_rq),

	TP_ARGS(cfs_rq),

	TP_STRUCT__entry(
		__field(        int,            cpu                     )
		__field(        unsigned long,  load                    )
		__field(        unsigned long,  rbl_load                )
		__field(        unsigned long,  util                    )
	),

	TP_fast_assign(
		struct rq *rq = __trace_get_rq(cfs_rq);

		__entry->cpu            = rq->cpu;
		__entry->load           = cfs_rq->avg.load_avg;
		__entry->rbl_load       = cfs_rq->avg.runnable_load_avg;
		__entry->util           = cfs_rq->avg.util_avg;
	),

	TP_printk("cpu=%d load=%lu rbl_load=%lu util=%lu",
			__entry->cpu, __entry->load,
			__entry->rbl_load,__entry->util)
);

/*
 * Tracepoint for rt_rq load tracking:
 */
TRACE_EVENT(sched_load_rt_rq,

	TP_PROTO(struct rq *rq),

	TP_ARGS(rq),

	TP_STRUCT__entry(
		__field(        int,            cpu                     )
		__field(        unsigned long,  util                    )
	),

	TP_fast_assign(
		__entry->cpu    = rq->cpu;
		__entry->util   = rq->avg_rt.util_avg;
	),

	TP_printk("cpu=%d util=%lu", __entry->cpu,
		__entry->util)
);

/*
 * Tracepoint for dl_rq load tracking:
 */
TRACE_EVENT(sched_load_dl_rq,

	TP_PROTO(struct rq *rq),

	TP_ARGS(rq),

	TP_STRUCT__entry(
		__field(        int,            cpu                     )
		__field(        unsigned long,  util                    )
	),

	TP_fast_assign(
		__entry->cpu    = rq->cpu;
		__entry->util   = rq->avg_dl.util_avg;
	),

	TP_printk("cpu=%d util=%lu", __entry->cpu,
		__entry->util)
);

#ifdef CONFIG_HAVE_SCHED_AVG_IRQ
/*
 * Tracepoint for irq load tracking:
 */
TRACE_EVENT(sched_load_irq,

	TP_PROTO(struct rq *rq),

	TP_ARGS(rq),

	TP_STRUCT__entry(
		__field(        int,            cpu                     )
		__field(        unsigned long,  util                    )
	),

	TP_fast_assign(
		__entry->cpu    = rq->cpu;
		__entry->util   = rq->avg_irq.util_avg;
	),

	TP_printk("cpu=%d util=%lu", __entry->cpu,
		__entry->util)
);
#endif

#ifdef CREATE_TRACE_POINTS
static inline
struct rq * __trace_get_rq(struct cfs_rq *cfs_rq)
{
#ifdef CONFIG_FAIR_GROUP_SCHED
	return cfs_rq->rq;
#else
	return container_of(cfs_rq, struct rq, cfs);
#endif
}
#endif /* CREATE_TRACE_POINTS */

/*
 * Tracepoint for sched_entity load tracking:
 */
TRACE_EVENT(sched_load_se,

	TP_PROTO(struct sched_entity *se),

	TP_ARGS(se),

	TP_STRUCT__entry(
		__field(        int,            cpu                           )
		__array(        char,           comm,   TASK_COMM_LEN         )
		__field(        pid_t,          pid                           )
		__field(        unsigned long,  load                          )
		__field(        unsigned long,  rbl_load                      )
		__field(        unsigned long,  util                          )
	),

	TP_fast_assign(
#ifdef CONFIG_FAIR_GROUP_SCHED
		struct cfs_rq *gcfs_rq = se->my_q;
#else
		struct cfs_rq *gcfs_rq = NULL;
#endif
		struct task_struct *p = gcfs_rq ? NULL
				: container_of(se, struct task_struct, se);
		struct rq *rq = gcfs_rq ? __trace_get_rq(gcfs_rq) : NULL;

		__entry->cpu            = rq ? rq->cpu
					: task_cpu((container_of(se, struct task_struct, se)));
		memcpy(__entry->comm, p ? p->comm : "(null)",
			p ? TASK_COMM_LEN : sizeof("(null)"));
		__entry->pid = p ? p->pid : -1;
		__entry->load = se->avg.load_avg;
		__entry->rbl_load = se->avg.runnable_load_avg;
		__entry->util = se->avg.util_avg;
	),

	TP_printk("cpu=%d comm=%s pid=%d load=%lu rbl_load=%lu util=%lu",
			__entry->cpu, __entry->comm, __entry->pid,
			__entry->load, __entry->rbl_load, __entry->util)
);

/*
 * Tracepoint for system overutilized flag
 */
TRACE_EVENT(sched_overutilized,

	TP_PROTO(int overutilized),

	TP_ARGS(overutilized),

	TP_STRUCT__entry(
		__field( int,  overutilized    )
	),

	TP_fast_assign(
		__entry->overutilized   = overutilized;
	),

	TP_printk("overutilized=%d",
		__entry->overutilized)
);

/*
 * Tracepoint for somac scheduling
 */
TRACE_EVENT(somac,

	TP_PROTO(struct task_struct *p, unsigned long util,
				int src_cpu, int dst_cpu),

	TP_ARGS(p, util, src_cpu, dst_cpu),

	TP_STRUCT__entry(
		__array(	char,		comm,	TASK_COMM_LEN	)
		__field(	pid_t,		pid			)
		__field(	unsigned long,	util			)
		__field(	int,		src_cpu			)
		__field(	int,		dst_cpu			)
	),

	TP_fast_assign(
		memcpy(__entry->comm, p->comm, TASK_COMM_LEN);
		__entry->pid		= p->pid;
		__entry->util		= util;
		__entry->src_cpu	= src_cpu;
		__entry->dst_cpu	= dst_cpu;
	),

	TP_printk("comm=%s pid=%d util_avg=%lu src_cpu=%d dst_cpu=%d",
		__entry->comm, __entry->pid, __entry->util,
		__entry->src_cpu, __entry->dst_cpu)
);

/*
 * Tracepoint for lb cpu util
 */
TRACE_EVENT(lb_cpu_util,

	TP_PROTO(int cpu, char *label),

	TP_ARGS(cpu, label),

	TP_STRUCT__entry(
		__field(	int,		cpu			)
		__field(	int,		active_balance		)
		__field(	int,		idle			)
		__field(	int,		nr_running		)
		__field(	int,		cfs_nr_running		)
		__field(	unsigned long,	misfit_load		)
		__field(	unsigned long,	cpu_util		)
		__field(	unsigned long,	capacity_orig		)
		__array(	char,		label,	LABEL_LEN	)
		),

	TP_fast_assign(
		__entry->cpu			= cpu;
		__entry->active_balance		= cpu_rq(cpu)->active_balance;
		__entry->idle			= available_idle_cpu(cpu);
		__entry->nr_running		= cpu_rq(cpu)->nr_running;
		__entry->cfs_nr_running		= cpu_rq(cpu)->cfs.h_nr_running;
		__entry->misfit_load		= cpu_rq(cpu)->misfit_task_load;
		__entry->cpu_util		= ml_cpu_util(cpu);
		__entry->capacity_orig		= capacity_orig_of(cpu);
		strncpy(__entry->label, label, LABEL_LEN - 1);
		),

	TP_printk("cpu=%d, active_balance=%1d, idle=%1d, nr_running=%2d, cfs_nr_running=%2d, "
		"misfit_load=%4lu, cpu_util=%4lu, capacity=%4lu, reason=%s",
		__entry->cpu, __entry->active_balance, __entry->idle,
		__entry->nr_running, __entry->cfs_nr_running, __entry->misfit_load,
		__entry->cpu_util, __entry->capacity_orig, __entry->label)
);

#endif /* _TRACE_EMS_DEBUG_H */

/* This part must be outside protection */
#include <trace/define_trace.h>
