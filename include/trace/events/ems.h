/*
 *  Copyright (C) 2017 Park Bumgyu <bumgyu.park@samsung.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#undef TRACE_SYSTEM
#define TRACE_SYSTEM ems

#if !defined(_TRACE_EMS_H) || defined(TRACE_HEADER_MULTI_READ)
#define _TRACE_EMS_H

#include <linux/tracepoint.h>

/*
 * Tracepoint for wakeup balance
 */
TRACE_EVENT(ems_select_task_rq,

	TP_PROTO(struct task_struct *p, int target_cpu, int wakeup, char *state),

	TP_ARGS(p, target_cpu, wakeup, state),

	TP_STRUCT__entry(
		__array(	char,		comm,	TASK_COMM_LEN	)
		__field(	pid_t,		pid			)
		__field(	int,		target_cpu		)
		__field(	int,		wakeup			)
		__array(	char,		state,		30	)
	),

	TP_fast_assign(
		memcpy(__entry->comm, p->comm, TASK_COMM_LEN);
		__entry->pid		= p->pid;
		__entry->target_cpu	= target_cpu;
		__entry->wakeup		= wakeup;
		memcpy(__entry->state, state, 30);
	),

	TP_printk("comm=%s pid=%d target_cpu=%d wakeup=%d state=%s",
		  __entry->comm, __entry->pid, __entry->target_cpu,
		  __entry->wakeup, __entry->state)
);

TRACE_EVENT(ems_find_best_idle,

	TP_PROTO(struct task_struct *p, int task_util, unsigned int idle_candidates,
			unsigned int active_candidates, int bind, int best_cpu),

	TP_ARGS(p, task_util, idle_candidates, active_candidates, bind, best_cpu),

	TP_STRUCT__entry(
		__array(	char,		comm,	TASK_COMM_LEN	)
		__field(	pid_t,		pid			)
		__field(	int,		task_util		)
		__field(	unsigned int,	idle_candidates		)
		__field(	unsigned int,	active_candidates	)
		__field(	int,		bind			)
		__field(	int,		best_cpu		)
	),

	TP_fast_assign(
		memcpy(__entry->comm, p->comm, TASK_COMM_LEN);
		__entry->pid		= p->pid;
		__entry->task_util	= task_util;
		__entry->idle_candidates = idle_candidates;
		__entry->active_candidates = active_candidates;
		__entry->bind = bind;
		__entry->best_cpu = best_cpu;
	),

	TP_printk("comm=%s pid=%d tsk_util=%d idle=%#x active=%#x bind=%d best_cpu=%d",
		__entry->comm, __entry->pid, __entry->task_util,
		__entry->idle_candidates, __entry->active_candidates,
		__entry->bind, __entry->best_cpu)
);


/*
 * Tracepoint for task migration control
 */
TRACE_EVENT(ems_can_migrate_task,

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
 * Tracepoint for policy/PM QoS update in ESG
 */
TRACE_EVENT(esg_update_limit,

	TP_PROTO(int cpu, int min, int max),

	TP_ARGS(cpu, min, max),

	TP_STRUCT__entry(
		__field( int,		cpu				)
		__field( int,		min				)
		__field( int,		max				)
	),

	TP_fast_assign(
		__entry->cpu			= cpu;
		__entry->min			= min;
		__entry->max			= max;
	),

	TP_printk("cpu=%d min_cap=%d, max_cap=%d",
		__entry->cpu, __entry->min, __entry->max)
);

/*
 * Tracepoint for frequency request in ESG
 */
TRACE_EVENT(esg_req_freq,

	TP_PROTO(int cpu, int util, int freq, int rapid_scale),

	TP_ARGS(cpu, util, freq, rapid_scale),

	TP_STRUCT__entry(
		__field( int,		cpu				)
		__field( int,		util				)
		__field( int,		freq				)
		__field( int,		rapid_scale			)
	),

	TP_fast_assign(
		__entry->cpu			= cpu;
		__entry->util			= util;
		__entry->freq			= freq;
		__entry->rapid_scale		= rapid_scale;
	),

	TP_printk("cpu=%d util=%d freq=%d rapid_scale=%d",
		__entry->cpu, __entry->util, __entry->freq, __entry->rapid_scale)
);

/*
 * Tracepoint for ontime migration
 */
TRACE_EVENT(ontime_migration,

	TP_PROTO(struct task_struct *p, unsigned long load,
				int src_cpu, int dst_cpu),

	TP_ARGS(p, load, src_cpu, dst_cpu),

	TP_STRUCT__entry(
		__array(	char,		comm,	TASK_COMM_LEN	)
		__field(	pid_t,		pid			)
		__field(	unsigned long,	load			)
		__field(	int,		src_cpu			)
		__field(	int,		dst_cpu			)
	),

	TP_fast_assign(
		memcpy(__entry->comm, p->comm, TASK_COMM_LEN);
		__entry->pid		= p->pid;
		__entry->load		= load;
		__entry->src_cpu	= src_cpu;
		__entry->dst_cpu	= dst_cpu;
	),

	TP_printk("comm=%s pid=%d ontime_load_avg=%lu src_cpu=%d dst_cpu=%d",
		__entry->comm, __entry->pid, __entry->load,
		__entry->src_cpu, __entry->dst_cpu)
);

/*
 * Tracepoint for emstune mode
 */
TRACE_EVENT(emstune_mode,

	TP_PROTO(int next_mode, int next_level),

	TP_ARGS(next_mode, next_level),

	TP_STRUCT__entry(
		__field( int,		next_mode		)
		__field( int,		next_level		)
	),

	TP_fast_assign(
		__entry->next_mode		= next_mode;
		__entry->next_level		= next_level;
	),

	TP_printk("mode=%d level=%d", __entry->next_mode, __entry->next_level)
);

/*
 * Tracepoint for global boost
 */
TRACE_EVENT(ems_global_boost,

	TP_PROTO(char *name, int boost),

	TP_ARGS(name, boost),

	TP_STRUCT__entry(
		__array(	char,	name,	64	)
		__field(	int,	boost		)
	),

	TP_fast_assign(
		memcpy(__entry->name, name, 64);
		__entry->boost		= boost;
	),

	TP_printk("name=%s global_boost=%d", __entry->name, __entry->boost)
);

/*
 * Trace for overutilization flag of LBT
 */
TRACE_EVENT(ems_lbt_overutilized,

	TP_PROTO(int cpu, int level, unsigned long util, unsigned long capacity, bool overutilized),

	TP_ARGS(cpu, level, util, capacity, overutilized),

	TP_STRUCT__entry(
		__field( int,		cpu			)
		__field( int,		level			)
		__field( unsigned long,	util			)
		__field( unsigned long,	capacity		)
		__field( bool,		overutilized		)
	),

	TP_fast_assign(
		__entry->cpu			= cpu;
		__entry->level			= level;
		__entry->util			= util;
		__entry->capacity		= capacity;
		__entry->overutilized		= overutilized;
	),

	TP_printk("cpu=%d level=%d util=%lu capacity=%lu overutilized=%d",
		__entry->cpu, __entry->level, __entry->util,
		__entry->capacity, __entry->overutilized)
);

/*
 * Tracepoint for overutilzed status for lb dst_cpu's sibling
 */
TRACE_EVENT(ems_lb_sibling_overutilized,

	TP_PROTO(int dst_cpu, int level, unsigned long ou),

	TP_ARGS(dst_cpu, level, ou),

	TP_STRUCT__entry(
		__field( int,		dst_cpu			)
		__field( int,		level			)
		__field( unsigned long,	ou			)
	),

	TP_fast_assign(
		__entry->dst_cpu		= dst_cpu;
		__entry->level			= level;
		__entry->ou			= ou;
	),

	TP_printk("dst_cpu=%d level=%d ou=%lu",
				__entry->dst_cpu, __entry->level, __entry->ou)
);

/*
 * Tracepint for PMU Contention AVG
 */
TRACE_EVENT(frt_find_heavy_task,

	TP_PROTO(int cpu, struct task_struct *p, s64 util),

	TP_ARGS(cpu, p, util),

	TP_STRUCT__entry(
		__field(	int,		cpu		)
		__array(	char,		comm,	TASK_COMM_LEN	)
		__field(	pid_t,		pid			)
		__field(	s64,		util			)
		),

	TP_fast_assign(
		__entry->cpu = cpu;
		memcpy(__entry->comm, p->comm, TASK_COMM_LEN);
		__entry->pid		= p->pid;
		__entry->util		= util;
		),

	TP_printk("cp%d comm=%s pid=%d util=%lld",
		__entry->cpu, __entry->comm, __entry->pid, __entry->util)
);

TRACE_EVENT(frt_find_heavy_cpu,

	TP_PROTO(int cpu, s64 util),

	TP_ARGS(cpu, util),

	TP_STRUCT__entry(
		__field(	int,		cpu		)
		__field(	s64,		util			)
		),

	TP_fast_assign(
		__entry->cpu = cpu;
		__entry->util		= util;
		),

	TP_printk("cpu%d util=%lld",
		__entry->cpu, __entry->util)
);

TRACE_EVENT(cont_found_victim,

	TP_PROTO(int victim_found, u64 victim_found_time),

	TP_ARGS(victim_found, victim_found_time),

	TP_STRUCT__entry(
		__field(	int,		victim_found		)
		__field(	u64,		victim_found_time	)
		),

	TP_fast_assign(
		__entry->victim_found = victim_found;
		__entry->victim_found_time = victim_found_time;
		),

	TP_printk("victim_found=%d time=%lu",
		__entry->victim_found, __entry->victim_found_time)
   );

TRACE_EVENT(cont_set_period_start,

	TP_PROTO(int cpu, u64 last_updated, u64 period_start, u32 hist_idx, u64 prev0, u64 prev1),

	TP_ARGS(cpu, last_updated, period_start, hist_idx, prev0, prev1),

	TP_STRUCT__entry(
		__field(	int,		cpu			)
		__field(	u64,		last_updated		)
		__field(	u64,		period_start		)
		__field(	u32,		hist_idx		)
		__field(	u64,		prev0			)
		__field(	u64,		prev1			)
		),

	TP_fast_assign(
		__entry->cpu		= cpu;
		__entry->last_updated	= last_updated;
		__entry->period_start	= period_start;
		__entry->hist_idx	= hist_idx;
		__entry->prev0		= prev0;
		__entry->prev1		= prev1;
		),

	TP_printk("cpu=%d last_updated=%lu, period_start=%lu, hidx=%u prev0=%lu prev1=%lu",
			__entry->cpu, __entry->last_updated, __entry->period_start,
			__entry->hist_idx, __entry->prev0, __entry->prev1)
);

/*
 * Tracepoint for logging FRT schedule activity
 */
TRACE_EVENT(frt_select_task_rq,

	TP_PROTO(struct task_struct *tsk, int best, struct cpumask *candi,
					const struct cpumask *mask, char* str),

	TP_ARGS(tsk, best, candi, mask, str),

 	TP_STRUCT__entry(
 		__array( char,	selectby,	TASK_COMM_LEN	)
 		__array( char,	targettsk,	TASK_COMM_LEN	)
 		__field( pid_t,	pid				)
		__field( int,	prio				)
 		__field( int,	bestcpu				)
 		__field( int,	prevcpu				)
		__field( unsigned int,	cpus_ptr		)
		__field( unsigned int,	candi			)
		__field( unsigned int,	mask			)
 	),

 	TP_fast_assign(
 		memcpy(__entry->selectby, str, TASK_COMM_LEN);
 		memcpy(__entry->targettsk, tsk->comm, TASK_COMM_LEN);
 		__entry->pid			= tsk->pid;
		__entry->prio			= tsk->prio;
 		__entry->bestcpu		= best;
 		__entry->prevcpu		= task_cpu(tsk);
		__entry->cpus_ptr		= *(unsigned int *)cpumask_bits(tsk->cpus_ptr);
		__entry->candi			= candi ? *(unsigned int *)cpumask_bits(candi) : 0;
		__entry->mask			= mask ? *(unsigned int *)cpumask_bits(mask) : 0;
 	),

	TP_printk("frt: comm=%s pid=%d prio=%d cpus_ptr=%#x candi=%#x mask=%#x cpu%d->cpu%d by %s",
 		  __entry->targettsk,
 		  __entry->pid,
		  __entry->prio,
		  __entry->cpus_ptr,
		  __entry->candi,
		  __entry->mask,
 		  __entry->prevcpu,
		  __entry->bestcpu,
 		  __entry->selectby)
 );

/*
 * Tracepoint for ems newidle balance
 */
TRACE_EVENT(lb_newidle_balance,

	TP_PROTO(int this_cpu, int busy_cpu, int pulled, bool short_idle),

	TP_ARGS(this_cpu, busy_cpu, pulled, short_idle),

	TP_STRUCT__entry(
		__field(	int,		cpu			)
		__field(	int,		busy_cpu		)
		__field(	int,		pulled			)
		__field(	unsigned int,	nr_running		)
		__field(	unsigned int,	rt_nr_running		)
		__field(	int,		nr_iowait		)
		__field(	u64,		avg_idle		)
		__field(        bool,		short_idle		)
		__field(	int,		overload		)
	),

	TP_fast_assign(
		__entry->cpu			= this_cpu;
		__entry->busy_cpu		= busy_cpu;
		__entry->pulled			= pulled;
		__entry->nr_running		= cpu_rq(this_cpu)->nr_running;
		__entry->rt_nr_running		= cpu_rq(this_cpu)->rt.rt_nr_running;
		__entry->nr_iowait		= atomic_read(&(cpu_rq(this_cpu)->nr_iowait));
		__entry->avg_idle		= cpu_rq(this_cpu)->avg_idle;
		__entry->short_idle		= short_idle;
		__entry->overload		= cpu_rq(this_cpu)->rd->overload;
	),

	TP_printk("cpu=%1d, busy_cpu=%2d, pulled=%1d, nr_running=%2u, rt_nr_running=%2u, "
		"nr_iowait=%1d, avg_idle=%7llu, short_idle=%1d, overload=%1d",
		__entry->cpu, __entry->busy_cpu, __entry->pulled,
		__entry->nr_running, __entry->rt_nr_running, __entry->nr_iowait,
		__entry->avg_idle, __entry->short_idle, __entry->overload)
);

#endif /* _TRACE_EMS_H */

/* This part must be outside protection */
#include <trace/define_trace.h>
