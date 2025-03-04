/*
 * Add hook to trace point
 *
 * Copyright (C) 2018 Samsung Electronics Co., Ltd
 * Park Bumgyu <bumgyu.park@samsung.com>
 */

#include "../sched.h"

#include <trace/events/sched.h>
#include <trace/events/ems_debug.h>

/******************************************************************************
 * hook for ftrace                                                            *
 ******************************************************************************/
static void ftrace_pelt_cfs_tp(void *data, struct cfs_rq *cfs_rq)
{
	trace_sched_load_cfs_rq(cfs_rq);
}

static void ftrace_pelt_rt_tp(void *data, struct rq *rq)
{
	trace_sched_load_rt_rq(rq);
}

static void ftrace_pelt_dl_tp(void *data, struct rq *rq)
{
	trace_sched_load_dl_rq(rq);
}

static void ftrace_pelt_irq_tp(void *data, struct rq *rq)
{
#ifdef CONFIG_HAVE_SCHED_AVG_IRQ
	trace_sched_load_irq(rq);
#endif
}

static void ftrace_pelt_se_tp(void *data, struct sched_entity *se)
{
	trace_sched_load_se(se);
}

static void ftrace_sched_overutilized_tp(void *data,
			struct root_domain *rd, bool overutilized)
{
	trace_sched_overutilized(overutilized);
}

int hook_init(void)
{
	int ret = 0;

	ret = register_trace_pelt_cfs_tp(ftrace_pelt_cfs_tp, NULL);
	WARN_ON(ret);
	ret = register_trace_pelt_rt_tp(ftrace_pelt_rt_tp, NULL);
	WARN_ON(ret);
	ret = register_trace_pelt_dl_tp(ftrace_pelt_dl_tp, NULL);
	WARN_ON(ret);
	ret = register_trace_pelt_irq_tp(ftrace_pelt_irq_tp, NULL);
	WARN_ON(ret);
	ret = register_trace_pelt_se_tp(ftrace_pelt_se_tp, NULL);
	WARN_ON(ret);
	ret = register_trace_sched_overutilized_tp(ftrace_sched_overutilized_tp, NULL);
	WARN_ON(ret);

	return ret;
}
