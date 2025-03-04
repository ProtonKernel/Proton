/*
 * IO performance mode with UFS
 *
 * Copyright (C) 2019 Samsung Electronics Co., Ltd.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * Authors:
 *	Kiwoong <kwmad.kim@samsung.com>
 */
#include <linux/of.h>
#include "ufs-exynos-perf.h"
#define CREATE_TRACE_POINTS
#include <trace/events/ufs_exynos_perf.h>

static struct ufs_perf_control *_perf;

#if IS_ENABLED(CONFIG_CPU_FREQ) || IS_ENABLED(CONFIG_ARM_FREQ_QOS_TRACER)
static int ufs_perf_cpufreq_nb(struct notifier_block *nb,
		unsigned long event, void *arg)
{
	struct cpufreq_policy *policy = (struct cpufreq_policy *)arg;
	struct ufs_perf_control *perf = _perf;

	if (event != CPUFREQ_CREATE_POLICY)
		return NOTIFY_OK;

#if IS_ENABLED(CONFIG_ARM_FREQ_QOS_TRACER)
	if (policy->cpu == CL0)
		freq_qos_tracer_add_request(&policy->constraints,
				&perf->qos_req_cpu_cl0, FREQ_QOS_MIN, 0);
	else if (policy->cpu == CL1)
		freq_qos_tracer_add_request(&policy->constraints,
				&perf->qos_req_cpu_cl1, FREQ_QOS_MIN, 0);
	else if (policy->cpu == CL2)
		freq_qos_tracer_add_request(&policy->constraints,
				&perf->qos_req_cpu_cl2, FREQ_QOS_MIN, 0);
#else
	if (policy->cpu == CL0)
		freq_qos_add_request(&policy->constraints,
				&perf->qos_req_cpu_cl0, FREQ_QOS_MIN, 0);
	else if (policy->cpu == CL1)
		freq_qos_add_request(&policy->constraints,
				&perf->qos_req_cpu_cl1, FREQ_QOS_MIN, 0);
	else if (policy->cpu == CL2)
		freq_qos_add_request(&policy->constraints,
				&perf->qos_req_cpu_cl2, FREQ_QOS_MIN, 0);
#endif

	return NOTIFY_OK;
}
#endif

static void ufs_perf_cp_and_trg(struct ufs_perf_control *perf,
		bool is_big, bool is_cp_time, s64 time, u32 ctrl_flag)
{
	unsigned long flags;

	spin_lock_irqsave(&perf->lock, flags);
	perf->ctrl_flag = ctrl_flag;
	if (is_cp_time) {
		if (time == -1 || is_big) {
			perf->cp_time_b = time;
			perf->count_b = 0;
		}
		if (time == -1 || !is_big) {
			perf->cp_time_l = time;
			perf->count_l = 0;
		}
	} else {
		if (is_big)
			perf->count_b++;
		else
			perf->count_l++;
	}

	/*
	 * In increasing traffic cases after idle,
	 * make it do not update stat because we assume
	 * this is IO centric scenarios.
	 */
	if (perf->is_locked && !perf->is_held &&
				ctrl_flag == UFS_PERF_CTRL_LOCK)
		perf->is_held = true;

	if (!perf->is_active) {
		if (!perf->is_locked &&
				ctrl_flag == UFS_PERF_CTRL_LOCK) {
			trace_ufs_exynos_perf_lock(49);
			complete(&perf->completion);
		}
		if (perf->is_locked &&
				ctrl_flag == UFS_PERF_CTRL_RELEASE) {
			trace_ufs_exynos_perf_lock(99);
			complete(&perf->completion);
		}
	}
	spin_unlock_irqrestore(&perf->lock, flags);
}

static int ufs_perf_handler(void *data)
{
	struct ufs_perf_control *perf = (struct ufs_perf_control *)data;
	u32 ctrl_flag;
	unsigned long flags;
	bool is_locked;
	bool is_held;

	perf->is_active = true;
	perf->is_locked = false;
	perf->is_held = false;
	perf->will_stop = false;
	init_completion(&perf->completion);

	while (true) {
		if (kthread_should_stop())
			break;
		if (perf->will_stop)
			continue;

		/* ctrl_flag should be reset to get incoming request */
		spin_lock_irqsave(&perf->lock, flags);
		ctrl_flag = perf->ctrl_flag;
		perf->ctrl_flag = UFS_PERF_CTRL_NONE;
		is_locked = perf->is_locked;
		is_held = perf->is_held;	//
		spin_unlock_irqrestore(&perf->lock, flags);

		if (ctrl_flag == UFS_PERF_CTRL_LOCK) {
			if (!is_locked) {
#if IS_ENABLED(CONFIG_EXYNOS_PM_QOS) || IS_ENABLED(CONFIG_EXYNOS_PM_QOS_MODULE)
				if (perf->pm_qos_int_value)
					exynos_pm_qos_update_request(&perf->pm_qos_int, perf->pm_qos_int_value);
				if (perf->pm_qos_mif_value)
					exynos_pm_qos_update_request(&perf->pm_qos_mif, perf->pm_qos_mif_value);
#endif
#if IS_ENABLED(CONFIG_CPU_FREQ) || IS_ENABLED(CONFIG_ARM_FREQ_QOS_TRACER)
				freq_qos_update_request(&perf->qos_req_cpu_cl0,
						perf->pm_qos_cluster0_value);
				freq_qos_update_request(&perf->qos_req_cpu_cl1,
						perf->pm_qos_cluster1_value);
				freq_qos_update_request(&perf->qos_req_cpu_cl2,
						perf->pm_qos_cluster2_value);
#endif
				ufshcd_wb_ctrl(perf->hba, true);
				spin_lock_irqsave(&perf->lock, flags);
				trace_ufs_exynos_perf_lock(1);
				spin_unlock_irqrestore(&perf->lock, flags);
			}

			spin_lock_irqsave(&perf->lock, flags);
			perf->is_locked = true;
			perf->is_held = true;
			spin_unlock_irqrestore(&perf->lock, flags);
		} else if (ctrl_flag == UFS_PERF_CTRL_RELEASE) {
			if (is_locked) {
				spin_lock_irqsave(&perf->lock, flags);
				trace_ufs_exynos_perf_lock(3);
				spin_unlock_irqrestore(&perf->lock, flags);
				ufshcd_wb_ctrl(perf->hba, false);
#if IS_ENABLED(CONFIG_EXYNOS_PM_QOS) || IS_ENABLED(CONFIG_EXYNOS_PM_QOS_MODULE)
				if (perf->pm_qos_int_value)
					exynos_pm_qos_update_request(&perf->pm_qos_int, 0);
				if (perf->pm_qos_mif_value)
					exynos_pm_qos_update_request(&perf->pm_qos_mif, 0);
#endif
#if IS_ENABLED(CONFIG_CPU_FREQ) || IS_ENABLED(CONFIG_ARM_FREQ_QOS_TRACER)
				freq_qos_update_request(&perf->qos_req_cpu_cl0,
						0);
				freq_qos_update_request(&perf->qos_req_cpu_cl1,
						0);
				freq_qos_update_request(&perf->qos_req_cpu_cl2,
						0);
#endif
			}

			spin_lock_irqsave(&perf->lock, flags);
			perf->is_locked = false;
			spin_unlock_irqrestore(&perf->lock, flags);
		} else {
			spin_lock_irqsave(&perf->lock, flags);
			perf->is_active = false;
			trace_ufs_exynos_perf_lock(8);
			spin_unlock_irqrestore(&perf->lock, flags);

			wait_for_completion(&perf->completion);

			trace_ufs_exynos_perf_lock(9);
			spin_lock_irqsave(&perf->lock, flags);
			perf->is_active = true;
			spin_unlock_irqrestore(&perf->lock, flags);
		}

	}

	__set_current_state(TASK_RUNNING);
	spin_lock_irqsave(&perf->lock, flags);
	perf->handler = NULL;
	spin_unlock_irqrestore(&perf->lock, flags);
	perf->is_active = false;

	return 0;
}

/* EXTERNAL FUNCTIONS */

static void ufs_perf_reset_timer(struct timer_list *t)
{
	struct ufs_perf_control *perf = from_timer(perf, t, reset_timer);
	unsigned long flags;
	u32 ctrl_flag;

	spin_lock_irqsave(&perf->lock, flags);
	perf->is_held = false;
	ctrl_flag = perf->ctrl_flag_in_transit;
	perf->ctrl_flag_in_transit = UFS_PERF_CTRL_NONE;
	spin_unlock_irqrestore(&perf->lock, flags);

	ufs_perf_cp_and_trg(perf, false, true, -1, ctrl_flag);
}

/* check point to re-initiate perf stat */
void ufs_perf_reset(void *data, struct ufs_hba *hba, bool boot)
{
	struct ufs_perf_control *perf = (struct ufs_perf_control *)data;
	unsigned long flags;
	u32 ctrl_flag = (boot) ? UFS_PERF_CTRL_NONE : UFS_PERF_CTRL_RELEASE;

	if (!perf || IS_ERR(perf->handler))
		return;

	if (ctrl_flag == UFS_PERF_CTRL_RELEASE)
		trace_ufs_exynos_perf_lock(4);

	if (!perf->hba && hba)
		perf->hba = hba;

	spin_lock_irqsave(&perf->lock, flags);
	perf->ctrl_flag_in_transit = ctrl_flag;
	spin_unlock_irqrestore(&perf->lock, flags);
	mod_timer(&perf->reset_timer, jiffies + msecs_to_jiffies(perf->th_reset_in_ms) + 1);
}

void ufs_perf_update_stat(void *data, unsigned int len, enum ufs_perf_op op)
{
	struct ufs_perf_control *perf = (struct ufs_perf_control *)data;
	unsigned long flags = 0;
	s64 time;
	bool is_big;
	bool is_cp_time = false;
	u64 time_diff_in_ns;
	u32 ctrl_flag = UFS_PERF_CTRL_NONE;
	struct ufs_hba *hba;

	u32 count;
	s64 cp_time;

	u32 th_count;
	u32 th_period_in_ms;	/* period for check point */

	BUG_ON(op >= UFS_PERF_OP_MAX);
	if (!perf)
		return;

	if (IS_ERR(perf->handler) || IS_ERR(perf->hba))
		return;

	if (op != UFS_PERF_OP_R && op != UFS_PERF_OP_W)
		return;

	hba = perf->hba;

	spin_unlock_irqrestore(hba->host->host_lock, flags);
	del_timer_sync(&perf->reset_timer);
	spin_lock_irqsave(hba->host->host_lock, flags);

	is_big = (len >= perf->th_chunk_in_kb * 1024);
	trace_ufs_exynos_perf_issue((int)is_big, (int)op, (int)len);

	/* Once triggered, lock state is hold right befor idle */
	if (perf->is_held)
		return;

	th_count = (is_big) ? perf->th_count_b : perf->th_count_l;
	th_period_in_ms = (is_big) ? perf->th_period_in_ms_b :
					perf->th_period_in_ms_l;
	time = cpu_clock(raw_smp_processor_id());

	spin_lock_irqsave(&perf->lock, flags);
	count = (is_big) ? perf->count_b : perf->count_l;

	/* in first case, just update time */
	cp_time = (is_big) ? perf->cp_time_b : perf->cp_time_l;
	if (perf->ctrl_flag_in_transit == UFS_PERF_CTRL_RELEASE
	    && cp_time  == -1LL) {
		if (is_big)
			perf->cp_time_b = time;
		else
			perf->cp_time_l = time;
		spin_unlock_irqrestore(&perf->lock, flags);
		return;
	}
	spin_unlock_irqrestore(&perf->lock, flags);

	/* check if check point is needed */
	time_diff_in_ns = (cp_time > time) ? (u64)(cp_time - time) :
				(u64)(time - cp_time);
	if (time_diff_in_ns >= (th_period_in_ms * (1000 * 1000))) {
		is_cp_time = true;

		/* check heavy load */
		ctrl_flag = (count + 1 >= th_count) ?
				UFS_PERF_CTRL_LOCK : UFS_PERF_CTRL_RELEASE;
		if (ctrl_flag == UFS_PERF_CTRL_RELEASE)
			trace_ufs_exynos_perf_lock(2);
	}

	if (len == 524288 && op == UFS_PERF_OP_R) {
		perf->count_b_r_cont++;
		ctrl_flag = (perf->count_b_r_cont >= 32) ?
				UFS_PERF_CTRL_LOCK : UFS_PERF_CTRL_NONE;
		if (ctrl_flag == UFS_PERF_CTRL_LOCK) {
			is_cp_time = true;
			perf->count_b_r_cont = 0;
		}
	} else {
		perf->count_b_r_cont = 0;
	}

	spin_lock_irqsave(&perf->lock, flags);
	trace_ufs_exynos_perf_stat(is_cp_time, time_diff_in_ns, th_period_in_ms,
			cp_time, time, count, th_count,
			(int)perf->is_locked, (int)perf->is_active);
	spin_unlock_irqrestore(&perf->lock, flags);
	ufs_perf_cp_and_trg(perf, is_big, is_cp_time, time, ctrl_flag);
}

void ufs_perf_populate_dt(void *data, struct device_node *np)
{
	struct ufs_perf_control *perf = (struct ufs_perf_control *)data;

	if (!perf || IS_ERR(perf->handler))
		return;

	/* Default, not to throttle device tp */
	if (of_property_read_u32(np, "perf-int", &perf->pm_qos_int_value))
		perf->pm_qos_int_value = 800000;

	if (of_property_read_u32(np, "perf-mif", &perf->pm_qos_mif_value))
		perf->pm_qos_mif_value = 3172000;

	/* Default, to issue request fast */
	if (of_property_read_u32(np, "perf-cluster0", &perf->pm_qos_cluster0_value))
		perf->pm_qos_cluster0_value = 2002000;

	if (of_property_read_u32(np, "perf-cluster1", &perf->pm_qos_cluster1_value))
		perf->pm_qos_cluster1_value = 2002000;

	if (of_property_read_u32(np, "perf-cluster2", &perf->pm_qos_cluster2_value))
		perf->pm_qos_cluster2_value = 2002000;

	if (of_property_read_u32(np, "perf-chunk", &perf->th_chunk_in_kb))
		perf->th_chunk_in_kb = 128;

	if (of_property_read_u32(np, "perf-count-b", &perf->th_count_b))
		perf->th_count_b = 60;

	if (of_property_read_u32(np, "perf-count-l", &perf->th_count_l))
		perf->th_count_l = 48;

	/* Default, to escape scenarios that requires power consumption */
	if (of_property_read_u32(np, "perf-period-in-ms-b", &perf->th_period_in_ms_b))
		perf->th_period_in_ms_b = 160;

	/* Default, to escape scenarios that requires power consumption */
	if (of_property_read_u32(np, "perf-period-in-ms-l", &perf->th_period_in_ms_l))
		perf->th_period_in_ms_l = 3;

	/* Default, to escape idle case during IO centric cases */
	if (of_property_read_u32(np, "perf-reset-delay-in-ms", &perf->th_reset_in_ms))
		perf->th_reset_in_ms = 100;

	perf->th_count_b_r_cont = 30;
	perf->count_b_r_cont = 0;
}

bool ufs_perf_init(void **data, struct device *dev)
{
	struct ufs_perf_control *perf;
	bool ret = false;

	/* perf and perf->handler is used to check using performance mode */
	*data = devm_kzalloc(dev, sizeof(struct ufs_perf_control), GFP_KERNEL);
	if (*data == NULL)
		goto out;

	perf = (struct ufs_perf_control *)(*data);
	_perf = perf;

	spin_lock_init(&perf->lock);

	perf->handler = kthread_run(ufs_perf_handler, perf,
				"ufs_perf_%d", 0);
	if (IS_ERR(perf->handler))
		goto out;

	timer_setup(&perf->reset_timer, ufs_perf_reset_timer, 0);
#if IS_ENABLED(CONFIG_EXYNOS_PM_QOS) || IS_ENABLED(CONFIG_EXYNOS_PM_QOS_MODULE)
	exynos_pm_qos_add_request(&perf->pm_qos_int, PM_QOS_DEVICE_THROUGHPUT, 0);
	exynos_pm_qos_add_request(&perf->pm_qos_mif, PM_QOS_BUS_THROUGHPUT, 0);
#endif

#if IS_ENABLED(CONFIG_CPU_FREQ) || IS_ENABLED(CONFIG_ARM_FREQ_QOS_TRACER)
	perf->cpufreq_nb.notifier_call = ufs_perf_cpufreq_nb;
	cpufreq_register_notifier(&perf->cpufreq_nb, CPUFREQ_POLICY_NOTIFIER);
#endif

	ret = true;
out:
	return ret;
}

void ufs_perf_exit(void *data)
{
	struct ufs_perf_control *perf = (struct ufs_perf_control *)data;

	if (perf && !IS_ERR(perf->handler)) {
#if IS_ENABLED(CONFIG_EXYNOS_PM_QOS) || IS_ENABLED(CONFIG_EXYNOS_PM_QOS_MODULE)
		exynos_pm_qos_remove_request(&perf->pm_qos_int);
		exynos_pm_qos_remove_request(&perf->pm_qos_mif);
#endif

#if IS_ENABLED(CONFIG_ARM_FREQ_QOS_TRACER)
	freq_qos_tracer_remove_request(&perf->qos_req_cpu_cl0);
	freq_qos_tracer_remove_request(&perf->qos_req_cpu_cl1);
	freq_qos_tracer_remove_request(&perf->qos_req_cpu_cl2);
#elif IS_ENABLED(CONFIG_CPU_FREQ)
	freq_qos_remove_request(&perf->qos_req_cpu_cl0);
	freq_qos_remove_request(&perf->qos_req_cpu_cl1);
	freq_qos_remove_request(&perf->qos_req_cpu_cl2);
#endif
		perf->will_stop = true;
		complete(&perf->completion);
		kthread_stop(perf->handler);
	}
}
MODULE_AUTHOR("Kiwoong Kim <kwmad.kim@samsung.com>");
MODULE_DESCRIPTION("Exynos UFS performance booster");
MODULE_LICENSE("GPL");
MODULE_VERSION("0.1");
