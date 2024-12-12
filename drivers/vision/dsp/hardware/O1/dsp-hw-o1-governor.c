// SPDX-License-Identifier: GPL-2.0
/*
 * Samsung Exynos SoC series dsp driver
 *
 * Copyright (c) 2020 Samsung Electronics Co., Ltd.
 *              http://www.samsung.com/
 */

#include <linux/delay.h>
#include <dt-bindings/soc/samsung/exynos-dsp.h>

#include "dsp-log.h"
#include "dsp-hw-o1-system.h"
#include "dsp-hw-o1-pm.h"
#include "dsp-hw-o1-llc.h"
#include "dsp-hw-o1-governor.h"

#define DSP_O1_GOVERNOR_TOKEN_WAIT_COUNT	(1000)

static int __dsp_hw_o1_governor_get_token(struct dsp_governor *governor,
		bool async)
{
	int repeat = DSP_O1_GOVERNOR_TOKEN_WAIT_COUNT;

	dsp_enter();
	while (repeat) {
		spin_lock(&governor->slock);
		if (governor->async_status < 0) {
			governor->async_status = async;
			spin_unlock(&governor->slock);
			dsp_dbg("get governor[%d] token(wait count:%d/%d)\n",
					async, repeat,
					DSP_O1_GOVERNOR_TOKEN_WAIT_COUNT);
			dsp_leave();
			return 0;
		}

		spin_unlock(&governor->slock);
		udelay(100);
		repeat--;
	}

	dsp_err("asynchronous governor work is not done\n");
	return -ETIMEDOUT;
}

static void __dsp_hw_o1_governor_put_token(struct dsp_governor *governor)
{
	dsp_enter();
	spin_lock(&governor->slock);
	if (governor->async_status < 0)
		dsp_warn("governor token is invalid(%d)\n",
				governor->async_status);

	governor->async_status = -1;
	spin_unlock(&governor->slock);
	dsp_dbg("put governor token\n");
	dsp_leave();
}

static int __dsp_hw_o1_governor_request(struct dsp_governor *governor,
		struct dsp_control_preset *req)
{
	dsp_enter();
	if (req->big_core_level >= 0)
		governor->sys->pm.ops->update_extra_devfreq(&governor->sys->pm,
				DSP_O1_EXTRA_DEVFREQ_CL2, req->big_core_level);

	if (req->middle_core_level >= 0)
		governor->sys->pm.ops->update_extra_devfreq(&governor->sys->pm,
				DSP_O1_EXTRA_DEVFREQ_CL1,
				req->middle_core_level);

	if (req->little_core_level >= 0)
		governor->sys->pm.ops->update_extra_devfreq(&governor->sys->pm,
				DSP_O1_EXTRA_DEVFREQ_CL0,
				req->little_core_level);

	if (req->mif_level >= 0)
		governor->sys->pm.ops->update_extra_devfreq(&governor->sys->pm,
				DSP_O1_EXTRA_DEVFREQ_MIF, req->mif_level);

	if (req->int_level >= 0)
		governor->sys->pm.ops->update_extra_devfreq(&governor->sys->pm,
				DSP_O1_EXTRA_DEVFREQ_INT, req->int_level);

	if (req->mo_scenario_id == DSP_GOVERNOR_DEFAULT)
		governor->sys->bus.ops->mo_all_put(&governor->sys->bus);
	else if (req->mo_scenario_id >= 0)
		governor->sys->bus.ops->mo_get_by_id(&governor->sys->bus,
				req->mo_scenario_id);

	if (req->llc_scenario_id == DSP_GOVERNOR_DEFAULT)
		governor->sys->llc.ops->llc_all_put(&governor->sys->llc);
	else if (req->llc_scenario_id >= 0)
		governor->sys->llc.ops->llc_get_by_id(&governor->sys->llc,
				req->llc_scenario_id);

	if (req->dvfs_ctrl == DSP_GOVERNOR_DEFAULT)
		governor->sys->pm.ops->dvfs_enable(&governor->sys->pm,
				req->dvfs_ctrl);
	else if (req->dvfs_ctrl >= 0)
		governor->sys->pm.ops->dvfs_disable(&governor->sys->pm,
				req->dvfs_ctrl);

	dsp_leave();
	return 0;
}

static void dsp_hw_o1_governor_async(struct kthread_work *work)
{
	struct dsp_governor *governor;

	dsp_enter();
	governor = container_of(work, struct dsp_governor, work);

	__dsp_hw_o1_governor_request(governor, &governor->async_req);
	__dsp_hw_o1_governor_put_token(governor);
	dsp_leave();
}

static int dsp_hw_o1_governor_request(struct dsp_governor *governor,
		struct dsp_control_preset *req)
{
	int ret;
	bool async;

	dsp_enter();
	if (req->async > 0)
		async = true;
	else
		async = false;

	ret = __dsp_hw_o1_governor_get_token(governor, async);
	if (ret)
		goto p_err;

	if (async) {
		governor->async_req = *req;
		kthread_queue_work(&governor->worker, &governor->work);
	} else {
		ret = __dsp_hw_o1_governor_request(governor, req);
		__dsp_hw_o1_governor_put_token(governor);
		if (ret)
			goto p_err;
	}

	dsp_leave();
	return 0;
p_err:
	return ret;
}

static int dsp_hw_o1_governor_check_done(struct dsp_governor *governor)
{
	int ret;

	dsp_enter();
	ret = __dsp_hw_o1_governor_get_token(governor, false);
	if (ret)
		goto p_err;

	__dsp_hw_o1_governor_put_token(governor);
	dsp_leave();
	return 0;
p_err:
	return ret;
}

static int dsp_hw_o1_governor_flush_work(struct dsp_governor *governor)
{
	dsp_enter();
	kthread_flush_work(&governor->work);
	dsp_leave();
	return 0;
}

static int dsp_hw_o1_governor_open(struct dsp_governor *governor)
{
	dsp_enter();
	dsp_leave();
	return 0;
}

static int dsp_hw_o1_governor_close(struct dsp_governor *governor)
{
	dsp_enter();
	dsp_leave();
	return 0;
}

static int __dsp_hw_o1_governor_worker_init(struct dsp_governor *governor)
{
	int ret;
	struct sched_param param = { .sched_priority = MAX_RT_PRIO - 1 };

	kthread_init_worker(&governor->worker);

	governor->task = kthread_run(kthread_worker_fn, &governor->worker,
			"dsp_hw_o1_governor_worker");
	if (IS_ERR(governor->task)) {
		ret = PTR_ERR(governor->task);
		dsp_err("kthread of governor is not running(%d)\n", ret);
		goto p_err_kthread;
	}

	ret = sched_setscheduler_nocheck(governor->task, SCHED_FIFO, &param);
	if (ret) {
		dsp_err("Failed to set scheduler of governor worker(%d)\n",
				ret);
		goto p_err_sched;
	}

	return ret;
p_err_sched:
	kthread_stop(governor->task);
p_err_kthread:
	return ret;
}

static void __dsp_hw_o1_governor_worker_deinit(struct dsp_governor *governor)
{
	dsp_enter();
	kthread_stop(governor->task);
	dsp_leave();
}

static int dsp_hw_o1_governor_probe(struct dsp_system *sys)
{
	int ret;
	struct dsp_governor *governor;

	dsp_enter();
	governor = &sys->governor;
	governor->sys = sys;

	ret = __dsp_hw_o1_governor_worker_init(governor);
	if (ret)
		goto p_err;

	kthread_init_work(&governor->work, dsp_hw_o1_governor_async);
	spin_lock_init(&governor->slock);
	governor->async_status = -1;

	dsp_leave();
	return 0;
p_err:
	return ret;
}

static void dsp_hw_o1_governor_remove(struct dsp_governor *governor)
{
	dsp_enter();
	__dsp_hw_o1_governor_worker_deinit(governor);
	dsp_leave();
}

static const struct dsp_governor_ops o1_governor_ops = {
	.request	= dsp_hw_o1_governor_request,
	.check_done	= dsp_hw_o1_governor_check_done,
	.flush_work	= dsp_hw_o1_governor_flush_work,

	.open		= dsp_hw_o1_governor_open,
	.close		= dsp_hw_o1_governor_close,
	.probe		= dsp_hw_o1_governor_probe,
	.remove		= dsp_hw_o1_governor_remove,
};

int dsp_hw_o1_governor_init(void)
{
	int ret;

	dsp_enter();
	ret = dsp_governor_register_ops(DSP_DEVICE_ID_O1, &o1_governor_ops);
	if (ret)
		goto p_err;

	dsp_leave();
	return 0;
p_err:
	return ret;
}
