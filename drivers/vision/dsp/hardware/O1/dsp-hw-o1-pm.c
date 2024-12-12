// SPDX-License-Identifier: GPL-2.0
/*
 * Samsung Exynos SoC series dsp driver
 *
 * Copyright (c) 2019 Samsung Electronics Co., Ltd.
 *              http://www.samsung.com/
 */

#include <linux/slab.h>
#include <linux/delay.h>
#include <dt-bindings/soc/samsung/exynos-dsp.h>

#include "dsp-log.h"
#include "dsp-hw-o1-system.h"
#include "dsp-hw-o1-memory.h"
#include "dsp-hw-o1-pm.h"

#if IS_ENABLED(CONFIG_ARM_FREQ_QOS_TRACER)
#include <soc/samsung/freq-qos-tracer.h>
#else
#define freq_qos_tracer_add_request(a, b, c, d)
#define freq_qos_tracer_remove_request(a)
#endif

#define DSP_O1_PM_TOKEN_WAIT_COUNT		(500)

static unsigned int vpc_table[] = {
	1196000,
	1066000,
	936000,
	800000,
	715000,
	533000,
	400000,
	266000,
	200000,
	100000,
};

static unsigned int cl2_table[] = {
	2912000,
	2808000,
	2704000,
	2600000,
	2496000,
	2392000,
	2288000,
	2184000,
	2080000,
	1976000,
	1872000,
	1768000,
	1664000,
	1560000,
	1456000,
	1352000,
	1248000,
	1144000,
	1040000,
	936000,
	832000,
	728000,
	624000,
	533000,
};

static unsigned int cl1_table[] = {
	2808000,
	2704000,
	2600000,
	2496000,
	2392000,
	2288000,
	2184000,
	2080000,
	1976000,
	1872000,
	1768000,
	1664000,
	1560000,
	1456000,
	1352000,
	1248000,
	1144000,
	1040000,
	936000,
	832000,
	728000,
	624000,
	533000,
};

static unsigned int cl0_table[] = {
	2210000,
	2106000,
	2002000,
	1898000,
	1794000,
	1690000,
	1586000,
	1482000,
	1378000,
	1274000,
	1170000,
	1066000,
	962000,
	858000,
	754000,
	650000,
	533000,
	400000,
};

static unsigned int int_table[] = {
	800000,
	533000,
	400000,
	267000,
	133000,
	80000,
};

static unsigned int mif_table[] = {
	3172000,
	2730000,
	2535000,
	2288000,
	2028000,
	1716000,
	1539000,
	1352000,
	1014000,
	845000,
	676000,
	546000,
	421000,
};

#ifdef ENABLE_DSP_VELOCE
static int pm_for_veloce;
#endif

static int dsp_hw_o1_pm_devfreq_active(struct dsp_pm *pm)
{
	dsp_check();
#ifndef ENABLE_DSP_VELOCE
	return pm_runtime_active(pm->sys->dev);
#else
	return pm_for_veloce;
#endif
}

static int __dsp_hw_o1_pm_check_valid(struct dsp_pm_devfreq *devfreq, int val)
{
	dsp_check();
	if (val < 0 || val > devfreq->min_qos) {
		dsp_err("devfreq[%s] value(%d) is invalid(L0 ~ L%u)\n",
				devfreq->name, val, devfreq->min_qos);
		return -EINVAL;
	} else {
		return 0;
	}
}

static void __dsp_hw_o1_pm_update_freq_info(struct dsp_pm_devfreq *devfreq)
{
	dsp_enter();
	if (devfreq->id == DSP_O1_DEVFREQ_VPC) {
		dsp_ctrl_dhcp_writel(
				DSP_O1_DHCP_IDX(DSP_O1_DHCP_VPC_FREQUENCY),
				devfreq->table[devfreq->current_qos] / 1000);
		dsp_ctrl_dhcp_writel(
				DSP_O1_DHCP_IDX(DSP_O1_DHCP_VPD_FREQUENCY),
				devfreq->table[devfreq->current_qos] / 1000);
	}
	dsp_leave();
}

static int __dsp_hw_o1_pm_update_devfreq_raw(struct dsp_pm_devfreq *devfreq,
		int val)
{
	int ret, req_val;

	dsp_enter();
	if (val == DSP_GOVERNOR_DEFAULT)
		req_val = devfreq->min_qos;
	else
		req_val = val;

	ret = __dsp_hw_o1_pm_check_valid(devfreq, req_val);
	if (ret)
		goto p_err;

#ifndef ENABLE_DSP_VELOCE
	if (devfreq->current_qos != req_val) {
		if (devfreq->use_freq_qos) {
#if KERNEL_VERSION(5, 4, 0) <= LINUX_VERSION_CODE
			freq_qos_update_request(&devfreq->freq_qos_req,
					devfreq->table[req_val]);
#endif
		} else {
#if IS_ENABLED(CONFIG_EXYNOS_PM_QOS)
			exynos_pm_qos_update_request(&devfreq->req,
					devfreq->table[req_val]);
#else
			pm_qos_update_request(&devfreq->req,
					devfreq->table[req_val]);
#endif
		}
		dsp_dbg("devfreq[%s] is changed from L%d to L%d\n",
				devfreq->name, devfreq->current_qos, req_val);
		devfreq->current_qos = req_val;
	} else {
		dsp_dbg("devfreq[%s] is already running L%d\n",
				devfreq->name, req_val);
	}
#else
	dsp_dbg("devfreq[%s] is not supported\n", devfreq->name);
#endif

	dsp_leave();
	return 0;
p_err:
	return ret;
}

static int __dsp_hw_o1_pm_devfreq_get_token(struct dsp_pm_devfreq *devfreq,
		bool async)
{
	int repeat = DSP_O1_PM_TOKEN_WAIT_COUNT;

	dsp_enter();
	while (repeat) {
		spin_lock(&devfreq->slock);
		if (devfreq->async_status < 0) {
			devfreq->async_status = async;
			spin_unlock(&devfreq->slock);
			dsp_dbg("get devfreq[%s/%d] token(wait count:%d/%d)\n",
					devfreq->name, async, repeat,
					DSP_O1_PM_TOKEN_WAIT_COUNT);
			dsp_leave();
			return 0;
		}

		spin_unlock(&devfreq->slock);
		udelay(10);
		repeat--;
	}

	dsp_err("other asynchronous work is not done [%s]\n", devfreq->name);
	return -ETIMEDOUT;
}

static void __dsp_hw_o1_pm_devfreq_put_token(struct dsp_pm_devfreq *devfreq)
{
	dsp_enter();
	spin_lock(&devfreq->slock);
	if (devfreq->async_status < 0)
		dsp_warn("devfreq token is invalid(%s/%d)\n",
				devfreq->name, devfreq->async_status);

	devfreq->async_status = -1;
	dsp_dbg("put devfreq[%s] token\n", devfreq->name);
	spin_unlock(&devfreq->slock);
	dsp_leave();
}

static int __dsp_hw_o1_pm_update_devfreq(struct dsp_pm_devfreq *devfreq,
		int val)
{
	int ret;

	dsp_enter();
	ret = __dsp_hw_o1_pm_update_devfreq_raw(devfreq, val);
	if (ret)
		goto p_err;

	__dsp_hw_o1_pm_update_freq_info(devfreq);
	dsp_leave();
	return 0;
p_err:
	return ret;
}

static void dsp_hw_o1_devfreq_async(struct kthread_work *work)
{
	struct dsp_pm_devfreq *devfreq;

	dsp_enter();
	devfreq = container_of(work, struct dsp_pm_devfreq, work);

	__dsp_hw_o1_pm_update_devfreq(devfreq, devfreq->async_qos);
	devfreq->async_qos = -1;
	__dsp_hw_o1_pm_devfreq_put_token(devfreq);
	devfreq->pm->sys->clk.ops->dump(&devfreq->pm->sys->clk);
	dsp_leave();
}

static int __dsp_hw_o1_pm_update_devfreq_nolock(struct dsp_pm *pm, int id,
		int val, bool async)
{
	int ret;

	dsp_enter();
	if (!pm->ops->devfreq_active(pm)) {
		ret = -EINVAL;
		dsp_warn("dsp is not running\n");
		goto p_err;
	}

	if (id < 0 || id >= DSP_O1_DEVFREQ_COUNT) {
		ret = -EINVAL;
		dsp_err("devfreq id(%d) for dsp is invalid\n", id);
		goto p_err;
	}

	ret = __dsp_hw_o1_pm_devfreq_get_token(&pm->devfreq[id], async);
	if (ret)
		goto p_err;

	if (async) {
		pm->devfreq[id].async_qos = val;
		kthread_queue_work(&pm->async_worker, &pm->devfreq[id].work);
	} else {
		ret = __dsp_hw_o1_pm_update_devfreq(&pm->devfreq[id], val);
		__dsp_hw_o1_pm_devfreq_put_token(&pm->devfreq[id]);
		if (ret)
			goto p_err;
	}

	dsp_leave();
	return 0;
p_err:
	return ret;
}

static int dsp_hw_o1_pm_update_devfreq_nolock(struct dsp_pm *pm, int id,
		int val)
{
	dsp_check();
	return __dsp_hw_o1_pm_update_devfreq_nolock(pm, id, val, false);
}

static int dsp_hw_o1_pm_update_devfreq_nolock_async(struct dsp_pm *pm, int id,
		int val)
{
	dsp_check();
	return __dsp_hw_o1_pm_update_devfreq_nolock(pm, id, val, true);
}

static int dsp_hw_o1_pm_update_extra_devfreq(struct dsp_pm *pm, int id, int val)
{
	int ret;

	dsp_enter();
	if (id < 0 || id >= DSP_O1_EXTRA_DEVFREQ_COUNT) {
		ret = -EINVAL;
		dsp_err("extra devfreq id(%d) is invalid\n", id);
		goto p_err;
	}

	mutex_lock(&pm->lock);
	ret = __dsp_hw_o1_pm_update_devfreq_raw(&pm->extra_devfreq[id], val);
	if (ret)
		goto p_err_update;

	mutex_unlock(&pm->lock);
	dsp_leave();
	return 0;
p_err_update:
	mutex_unlock(&pm->lock);
p_err:
	return ret;
}

static int dsp_hw_o1_pm_set_force_qos(struct dsp_pm *pm, int id, int val)
{
	int ret;

	dsp_enter();
	if (id < 0 || id >= DSP_O1_DEVFREQ_COUNT) {
		ret = -EINVAL;
		dsp_err("devfreq id(%d) for dsp is invalid\n", id);
		goto p_err;
	}

	if (val >= 0) {
		ret = __dsp_hw_o1_pm_check_valid(&pm->devfreq[id], val);
		if (ret)
			goto p_err;
	}

	pm->devfreq[id].force_qos = val;
	dsp_leave();
	return 0;
p_err:
	return ret;
}

static int __dsp_hw_o1_pm_del_static(struct dsp_pm_devfreq *devfreq, int val)
{
	int ret, idx;

	dsp_enter();
	if (!devfreq->static_count[val]) {
		ret = -EINVAL;
		dsp_warn("static count is unstable([%s][L%d]%u)\n",
				devfreq->name, val, devfreq->static_count[val]);
		goto p_err;
	} else {
		devfreq->static_count[val]--;
		if (devfreq->static_total_count) {
			devfreq->static_total_count--;
		} else {
			ret = -EINVAL;
			dsp_warn("static total count is unstable([%s]%u)\n",
					devfreq->name,
					devfreq->static_total_count);
			goto p_err;
		}
	}

	if ((val == devfreq->static_qos) && (!devfreq->static_count[val])) {
		for (idx = val + 1; idx <= devfreq->min_qos; ++idx) {
			if (idx == devfreq->min_qos) {
				devfreq->static_qos = idx;
				break;
			}
			if (devfreq->static_count[idx]) {
				devfreq->static_qos = idx;
				break;
			}
		}
	}

	dsp_leave();
	return 0;
p_err:
	return ret;
}

static void __dsp_hw_o1_pm_add_static(struct dsp_pm_devfreq *devfreq, int val)
{
	dsp_enter();
	devfreq->static_count[val]++;
	devfreq->static_total_count++;

	if (devfreq->static_total_count == 1)
		devfreq->static_qos = val;
	else if (val < devfreq->static_qos)
		devfreq->static_qos = val;

	dsp_leave();
}

static void __dsp_hw_o1_pm_static_reset(struct dsp_pm_devfreq *devfreq)
{
	dsp_enter();
	devfreq->static_qos = devfreq->min_qos;
	devfreq->static_total_count = 0;
	memset(devfreq->static_count, 0x0, DSP_DEVFREQ_RESERVED_NUM << 2);
	dsp_leave();
}

static int dsp_hw_o1_pm_dvfs_enable(struct dsp_pm *pm, int val)
{
	int ret, vpc_run;
	struct dsp_pm_devfreq *devfreq;

	dsp_enter();
	devfreq = &pm->devfreq[DSP_O1_DEVFREQ_VPC];

	mutex_lock(&pm->lock);
	if (!pm->dvfs_disable_count) {
		ret = -EINVAL;
		dsp_warn("dvfs disable count is unstable(%u)\n",
				pm->dvfs_disable_count);
		goto p_err;
	}

	if (val == DSP_GOVERNOR_DEFAULT) {
		dsp_info("DVFS enabled forcibly\n");
		__dsp_hw_o1_pm_static_reset(devfreq);
		pm->dvfs = true;
		pm->dvfs_disable_count = 0;
	} else {
		ret = __dsp_hw_o1_pm_check_valid(devfreq, val);
		if (ret)
			goto p_err;

		ret = __dsp_hw_o1_pm_del_static(devfreq, val);
		if (ret)
			goto p_err;

		if (!(--pm->dvfs_disable_count)) {
			pm->dvfs = true;
			dsp_info("DVFS enabled\n");
		}
	}

	if (!pm->ops->devfreq_active(pm)) {
		mutex_unlock(&pm->lock);
		return 0;
	}

	if (devfreq->static_total_count) {
		if (devfreq->force_qos < 0)
			vpc_run = devfreq->static_qos;
		else
			vpc_run = devfreq->force_qos;
	} else {
		vpc_run = devfreq->dynamic_qos;
	}

	pm->ops->update_devfreq_nolock(pm, DSP_O1_DEVFREQ_VPC, vpc_run);
	pm->sys->clk.ops->dump(&pm->sys->clk);

	mutex_unlock(&pm->lock);
	dsp_leave();
	return 0;
p_err:
	mutex_unlock(&pm->lock);
	return ret;
}

static int dsp_hw_o1_pm_dvfs_disable(struct dsp_pm *pm, int val)
{
	int ret, vpc_run;
	struct dsp_pm_devfreq *devfreq;

	dsp_enter();
	devfreq = &pm->devfreq[DSP_O1_DEVFREQ_VPC];

	mutex_lock(&pm->lock);
	ret = __dsp_hw_o1_pm_check_valid(devfreq, val);
	if (ret)
		goto p_err;

	pm->dvfs = false;
	if (!pm->dvfs_disable_count)
		dsp_info("DVFS disabled\n");
	pm->dvfs_disable_count++;

	__dsp_hw_o1_pm_add_static(devfreq, val);

	if (!pm->ops->devfreq_active(pm)) {
		mutex_unlock(&pm->lock);
		return 0;
	}

	if (devfreq->force_qos < 0)
		vpc_run = devfreq->static_qos;
	else
		vpc_run = devfreq->force_qos;

	pm->ops->update_devfreq_nolock(pm, DSP_O1_DEVFREQ_VPC, vpc_run);
	pm->sys->clk.ops->dump(&pm->sys->clk);

	mutex_unlock(&pm->lock);
	dsp_leave();
	return 0;
p_err:
	mutex_unlock(&pm->lock);
	return ret;
}

static void __dsp_hw_o1_pm_add_dynamic(struct dsp_pm_devfreq *devfreq, int val)
{
	dsp_enter();
	devfreq->dynamic_count[val]++;
	devfreq->dynamic_total_count++;

	if (devfreq->dynamic_total_count == 1)
		devfreq->dynamic_qos = val;
	else if (val < devfreq->dynamic_qos)
		devfreq->dynamic_qos = val;

	dsp_leave();
}

static int dsp_hw_o1_pm_update_devfreq_busy(struct dsp_pm *pm, int val)
{
	int ret, vpc_run;
	struct dsp_pm_devfreq *devfreq;

	dsp_enter();
	devfreq = &pm->devfreq[DSP_O1_DEVFREQ_VPC];

	mutex_lock(&pm->lock);
	ret = __dsp_hw_o1_pm_check_valid(devfreq, val);
	if (ret)
		goto p_err;

	__dsp_hw_o1_pm_add_dynamic(devfreq, val);

	if (!pm->dvfs) {
		dsp_dbg("DVFS was disabled(busy)\n");
		mutex_unlock(&pm->lock);
		return 0;
	}

	if (!pm->ops->devfreq_active(pm)) {
		mutex_unlock(&pm->lock);
		return 0;
	}

	if (devfreq->force_qos < 0)
		vpc_run = devfreq->dynamic_qos;
	else
		vpc_run = devfreq->force_qos;

	pm->ops->update_devfreq_nolock(pm, DSP_O1_DEVFREQ_VPC, vpc_run);
	dsp_dbg("DVFS busy\n");
	pm->sys->clk.ops->dump(&pm->sys->clk);

	mutex_unlock(&pm->lock);
	dsp_leave();
	return 0;
p_err:
	mutex_unlock(&pm->lock);
	return ret;
}

static int __dsp_hw_o1_pm_del_dynamic(struct dsp_pm_devfreq *devfreq, int val)
{
	int ret, idx;

	dsp_enter();
	if (!devfreq->dynamic_count[val]) {
		ret = -EINVAL;
		dsp_warn("dynamic count is unstable([%s][L%d]%u)\n",
				devfreq->name, val,
				devfreq->dynamic_count[val]);
		goto p_err;
	} else {
		devfreq->dynamic_count[val]--;
		if (devfreq->dynamic_total_count) {
			devfreq->dynamic_total_count--;
		} else {
			ret = -EINVAL;
			dsp_warn("dynamic total count is unstable([%s]%u)\n",
					devfreq->name,
					devfreq->dynamic_total_count);
			goto p_err;
		}
	}

	if ((val == devfreq->dynamic_qos) && (!devfreq->dynamic_count[val])) {
		for (idx = val + 1; idx <= devfreq->min_qos; ++idx) {
			if (idx == devfreq->min_qos) {
				devfreq->dynamic_qos = idx;
				break;
			}
			if (devfreq->dynamic_count[idx]) {
				devfreq->dynamic_qos = idx;
				break;
			}
		}
	}

	dsp_leave();
	return 0;
p_err:
	return ret;
}

static int dsp_hw_o1_pm_update_devfreq_idle(struct dsp_pm *pm, int val)
{
	int ret;
	struct dsp_pm_devfreq *devfreq;

	dsp_enter();
	devfreq = &pm->devfreq[DSP_O1_DEVFREQ_VPC];

	mutex_lock(&pm->lock);
	ret = __dsp_hw_o1_pm_check_valid(devfreq, val);
	if (ret)
		goto p_err;

	__dsp_hw_o1_pm_del_dynamic(devfreq, val);

	if (!pm->dvfs) {
		dsp_dbg("DVFS was disabled(idle)\n");
		mutex_unlock(&pm->lock);
		return 0;
	}

	if (!pm->ops->devfreq_active(pm)) {
		mutex_unlock(&pm->lock);
		return 0;
	}

	pm->ops->update_devfreq_nolock_async(pm, DSP_O1_DEVFREQ_VPC,
			devfreq->dynamic_qos);
	dsp_dbg("DVFS idle\n");

	mutex_unlock(&pm->lock);
	dsp_leave();
	return 0;
p_err:
	mutex_unlock(&pm->lock);
	return ret;
}

static int dsp_hw_o1_pm_update_devfreq_boot(struct dsp_pm *pm)
{
	int vpc_boot;

	dsp_enter();
	mutex_lock(&pm->lock);
	if (!pm->dvfs) {
		dsp_dbg("DVFS was disabled(boot)\n");
		pm->sys->clk.ops->dump(&pm->sys->clk);
		mutex_unlock(&pm->lock);
		return 0;
	}

	if (pm->devfreq[DSP_O1_DEVFREQ_VPC].force_qos < 0)
		vpc_boot = pm->devfreq[DSP_O1_DEVFREQ_VPC].boot_qos;
	else
		vpc_boot = pm->devfreq[DSP_O1_DEVFREQ_VPC].force_qos;

	pm->ops->update_devfreq_nolock(pm, DSP_O1_DEVFREQ_VPC, vpc_boot);
	dsp_dbg("DVFS boot\n");
	pm->sys->clk.ops->dump(&pm->sys->clk);

	mutex_unlock(&pm->lock);
	dsp_leave();
	return 0;
}

static int dsp_hw_o1_pm_update_devfreq_max(struct dsp_pm *pm)
{
	dsp_enter();
	mutex_lock(&pm->lock);
	if (!pm->dvfs) {
		dsp_dbg("DVFS was disabled(max)\n");
		mutex_unlock(&pm->lock);
		return 0;
	}

	pm->ops->update_devfreq_nolock(pm, DSP_O1_DEVFREQ_VPC, 0);
	dsp_dbg("DVFS max\n");
	pm->sys->clk.ops->dump(&pm->sys->clk);

	mutex_unlock(&pm->lock);
	dsp_leave();
	return 0;
}

static int dsp_hw_o1_pm_update_devfreq_min(struct dsp_pm *pm)
{
	dsp_enter();
	mutex_lock(&pm->lock);
	if (!pm->dvfs) {
		dsp_dbg("DVFS was disabled(min)\n");
		mutex_unlock(&pm->lock);
		return 0;
	}

	pm->ops->update_devfreq_nolock(pm, DSP_O1_DEVFREQ_VPC,
			pm->devfreq[DSP_O1_DEVFREQ_VPC].min_qos);
	dsp_dbg("DVFS min\n");
	pm->sys->clk.ops->dump(&pm->sys->clk);

	mutex_unlock(&pm->lock);
	dsp_leave();
	return 0;
}

static int __dsp_hw_o1_pm_set_boot_qos(struct dsp_pm *pm, int id, int val)
{
	int ret;

	dsp_enter();
	if (id < 0 || id >= DSP_O1_DEVFREQ_COUNT) {
		ret = -EINVAL;
		dsp_err("devfreq id(%d) for dsp is invalid\n", id);
		goto p_err;
	}

	ret = __dsp_hw_o1_pm_check_valid(&pm->devfreq[id], val);
	if (ret)
		goto p_err;

	pm->devfreq[id].boot_qos = val;
	dsp_leave();
	return 0;
p_err:
	return ret;
}

static int dsp_hw_o1_pm_set_boot_qos(struct dsp_pm *pm, int val)
{
	int ret;

	dsp_enter();
	ret = __dsp_hw_o1_pm_set_boot_qos(pm, DSP_O1_DEVFREQ_VPC, val);
	if (ret)
		goto p_err;

	dsp_leave();
	return 0;
p_err:
	return ret;
}

static int dsp_hw_o1_pm_boost_enable(struct dsp_pm *pm)
{
	int ret;
	struct dsp_pm_devfreq *devfreq;

	dsp_enter();
	devfreq = pm->extra_devfreq;

	mutex_lock(&pm->lock);
	__dsp_hw_o1_pm_update_devfreq_raw(&devfreq[DSP_O1_EXTRA_DEVFREQ_CL2],
			DSP_PM_QOS_L0);
	__dsp_hw_o1_pm_update_devfreq_raw(&devfreq[DSP_O1_EXTRA_DEVFREQ_CL1],
			DSP_PM_QOS_L0);
	__dsp_hw_o1_pm_update_devfreq_raw(&devfreq[DSP_O1_EXTRA_DEVFREQ_CL0],
			DSP_PM_QOS_L0);
	__dsp_hw_o1_pm_update_devfreq_raw(&devfreq[DSP_O1_EXTRA_DEVFREQ_INT],
			DSP_PM_QOS_L0);
	__dsp_hw_o1_pm_update_devfreq_raw(&devfreq[DSP_O1_EXTRA_DEVFREQ_MIF],
			DSP_PM_QOS_L0);
	mutex_unlock(&pm->lock);

	ret = pm->ops->dvfs_disable(pm, DSP_PM_QOS_L0);
	if (ret)
		goto p_err_dvfs;

	dsp_info("boost mode of pm is enabled\n");
	dsp_leave();
	return 0;
p_err_dvfs:
	mutex_lock(&pm->lock);
	__dsp_hw_o1_pm_update_devfreq_raw(&devfreq[DSP_O1_EXTRA_DEVFREQ_MIF],
			devfreq[DSP_O1_EXTRA_DEVFREQ_MIF].min_qos);
	__dsp_hw_o1_pm_update_devfreq_raw(&devfreq[DSP_O1_EXTRA_DEVFREQ_INT],
			devfreq[DSP_O1_EXTRA_DEVFREQ_INT].min_qos);
	__dsp_hw_o1_pm_update_devfreq_raw(&devfreq[DSP_O1_EXTRA_DEVFREQ_CL0],
			devfreq[DSP_O1_EXTRA_DEVFREQ_CL0].min_qos);
	__dsp_hw_o1_pm_update_devfreq_raw(&devfreq[DSP_O1_EXTRA_DEVFREQ_CL1],
			devfreq[DSP_O1_EXTRA_DEVFREQ_CL1].min_qos);
	__dsp_hw_o1_pm_update_devfreq_raw(&devfreq[DSP_O1_EXTRA_DEVFREQ_CL2],
			devfreq[DSP_O1_EXTRA_DEVFREQ_CL2].min_qos);
	mutex_unlock(&pm->lock);
	return ret;
}

static int dsp_hw_o1_pm_boost_disable(struct dsp_pm *pm)
{
	struct dsp_pm_devfreq *devfreq;

	dsp_enter();
	pm->ops->dvfs_enable(pm, DSP_PM_QOS_L0);

	devfreq = pm->extra_devfreq;
	mutex_lock(&pm->lock);
	__dsp_hw_o1_pm_update_devfreq_raw(&devfreq[DSP_O1_EXTRA_DEVFREQ_MIF],
			devfreq[DSP_O1_EXTRA_DEVFREQ_MIF].min_qos);
	__dsp_hw_o1_pm_update_devfreq_raw(&devfreq[DSP_O1_EXTRA_DEVFREQ_INT],
			devfreq[DSP_O1_EXTRA_DEVFREQ_INT].min_qos);
	__dsp_hw_o1_pm_update_devfreq_raw(&devfreq[DSP_O1_EXTRA_DEVFREQ_CL0],
			devfreq[DSP_O1_EXTRA_DEVFREQ_CL0].min_qos);
	__dsp_hw_o1_pm_update_devfreq_raw(&devfreq[DSP_O1_EXTRA_DEVFREQ_CL1],
			devfreq[DSP_O1_EXTRA_DEVFREQ_CL1].min_qos);
	__dsp_hw_o1_pm_update_devfreq_raw(&devfreq[DSP_O1_EXTRA_DEVFREQ_CL2],
			devfreq[DSP_O1_EXTRA_DEVFREQ_CL2].min_qos);
	mutex_unlock(&pm->lock);

	dsp_info("boost mode of pm is disabled\n");
	dsp_leave();
	return 0;
}

static void __dsp_hw_o1_pm_enable(struct dsp_pm_devfreq *devfreq, bool dvfs)
{
	int init_qos;

	dsp_enter();
	if (devfreq->force_qos < 0) {
		if (!dvfs) {
			init_qos = devfreq->static_qos;
			dsp_info("devfreq[%s] is enabled(L%d)(static)\n",
					devfreq->name, init_qos);
		} else {
			init_qos = devfreq->min_qos;
			dsp_info("devfreq[%s] is enabled(L%d)(dynamic)\n",
					devfreq->name, init_qos);
		}
	} else {
		init_qos = devfreq->force_qos;
		dsp_info("devfreq[%s] is enabled(L%d)(force)\n",
				devfreq->name, init_qos);
	}

#ifndef ENABLE_DSP_VELOCE
#if IS_ENABLED(CONFIG_EXYNOS_PM_QOS)
	exynos_pm_qos_add_request(&devfreq->req, devfreq->class_id,
			devfreq->table[init_qos]);
#else
	pm_qos_add_request(&devfreq->req, devfreq->class_id,
			devfreq->table[init_qos]);
#endif
#endif
	devfreq->current_qos = init_qos;
	__dsp_hw_o1_pm_update_freq_info(devfreq);
	dsp_leave();
}

static void __dsp_hw_o1_pm_extra_enable(struct dsp_pm_devfreq *devfreq)
{
	dsp_enter();
#ifndef ENABLE_DSP_VELOCE
	if (devfreq->use_freq_qos) {
		devfreq->policy = cpufreq_cpu_get(devfreq->class_id);
		if (!devfreq->policy) {
			dsp_err("Failed to get policy[%s/%d]\n",
					devfreq->name, devfreq->class_id);
			return;
		}

#if KERNEL_VERSION(5, 4, 0) <= LINUX_VERSION_CODE
		freq_qos_tracer_add_request(&devfreq->policy->constraints,
				&devfreq->freq_qos_req, FREQ_QOS_MIN,
				devfreq->table[devfreq->min_qos]);
#endif
	} else {
#if IS_ENABLED(CONFIG_EXYNOS_PM_QOS)
		exynos_pm_qos_add_request(&devfreq->req, devfreq->class_id,
				devfreq->table[devfreq->min_qos]);
#else
		pm_qos_add_request(&devfreq->req, devfreq->class_id,
				devfreq->table[devfreq->min_qos]);
#endif
	}
#endif
	devfreq->current_qos = devfreq->min_qos;
	dsp_dbg("devfreq[%s] is enabled(L%d)\n",
			devfreq->name, devfreq->current_qos);
	dsp_leave();
}

static int dsp_hw_o1_pm_enable(struct dsp_pm *pm)
{
	int idx;

	dsp_enter();
	mutex_lock(&pm->lock);

	for (idx = 0; idx < DSP_O1_DEVFREQ_COUNT; ++idx)
		__dsp_hw_o1_pm_enable(&pm->devfreq[idx], pm->dvfs);

	for (idx = 0; idx < DSP_O1_EXTRA_DEVFREQ_COUNT; ++idx)
		__dsp_hw_o1_pm_extra_enable(&pm->extra_devfreq[idx]);

#ifdef ENABLE_DSP_VELOCE
	pm_for_veloce = 1;
#endif
	mutex_unlock(&pm->lock);
	dsp_leave();
	return 0;
}

static void __dsp_hw_o1_pm_disable(struct dsp_pm_devfreq *devfreq)
{
	dsp_enter();
	kthread_flush_work(&devfreq->work);
#ifndef ENABLE_DSP_VELOCE
#if IS_ENABLED(CONFIG_EXYNOS_PM_QOS)
	exynos_pm_qos_remove_request(&devfreq->req);
#else
	pm_qos_remove_request(&devfreq->req);
#endif
#endif
	dsp_info("devfreq[%s] is disabled\n", devfreq->name);
	dsp_leave();
}

static void __dsp_hw_o1_pm_extra_disable(struct dsp_pm_devfreq *devfreq)
{
	dsp_enter();
#ifndef ENABLE_DSP_VELOCE
	if (devfreq->use_freq_qos) {
#if KERNEL_VERSION(5, 4, 0) <= LINUX_VERSION_CODE
		freq_qos_tracer_remove_request(&devfreq->freq_qos_req);
#endif
		devfreq->policy = NULL;
	} else {
#if IS_ENABLED(CONFIG_EXYNOS_PM_QOS)
		exynos_pm_qos_remove_request(&devfreq->req);
#else
		pm_qos_remove_request(&devfreq->req);
#endif
	}
#endif
	dsp_dbg("devfreq[%s] is disabled\n", devfreq->name);
	dsp_leave();
}

static int dsp_hw_o1_pm_disable(struct dsp_pm *pm)
{
	int idx;

	dsp_enter();
	mutex_lock(&pm->lock);
#ifdef ENABLE_DSP_VELOCE
	pm_for_veloce = 0;
#endif

	for (idx = DSP_O1_EXTRA_DEVFREQ_COUNT - 1; idx >= 0; --idx)
		__dsp_hw_o1_pm_extra_disable(&pm->extra_devfreq[idx]);

	for (idx = DSP_O1_DEVFREQ_COUNT - 1; idx >= 0; --idx)
		__dsp_hw_o1_pm_disable(&pm->devfreq[idx]);

	mutex_unlock(&pm->lock);
	dsp_leave();
	return 0;
}

static int __dsp_hw_o1_pm_check_class_id(struct dsp_pm_devfreq *devfreq)
{
	dsp_enter();
	if (devfreq->use_freq_qos) {
		dsp_dbg("devfreq[%s] uses freq_qos(%d)\n",
				devfreq->name, devfreq->class_id);
	} else {
#ifndef ENABLE_DSP_VELOCE
		if (!devfreq->class_id) {
			dsp_err("devfreq[%s] class_id must not be zero(%d)\n",
					devfreq->name, devfreq->class_id);
			return -EINVAL;
		}
#endif
		dsp_dbg("devfreq[%s] class_id is %d\n", devfreq->name,
				devfreq->class_id);
	}
	dsp_leave();
	return 0;
}

static int dsp_hw_o1_pm_open(struct dsp_pm *pm)
{
	int idx, ret;

	dsp_enter();
	for (idx = 0; idx < DSP_O1_DEVFREQ_COUNT; ++idx) {
		ret = __dsp_hw_o1_pm_check_class_id(&pm->devfreq[idx]);
		if (ret)
			goto p_err;
	}

	for (idx = 0; idx < DSP_O1_EXTRA_DEVFREQ_COUNT; ++idx) {
		ret = __dsp_hw_o1_pm_check_class_id(&pm->extra_devfreq[idx]);
		if (ret)
			goto p_err;
	}

	dsp_leave();
	return 0;
p_err:
	return ret;
}

static void __dsp_hw_o1_pm_init(struct dsp_pm_devfreq *devfreq)
{
	dsp_enter();
	devfreq->static_qos = devfreq->min_qos;
	devfreq->static_total_count = 0;
	memset(devfreq->static_count, 0x0, DSP_DEVFREQ_RESERVED_NUM << 2);
	devfreq->dynamic_qos = devfreq->min_qos;
	devfreq->dynamic_total_count = 0;
	memset(devfreq->dynamic_count, 0x0, DSP_DEVFREQ_RESERVED_NUM << 2);
	devfreq->force_qos = -1;

	devfreq->async_status = -1;
	devfreq->async_qos = -1;
	dsp_leave();
}

static int dsp_hw_o1_pm_close(struct dsp_pm *pm)
{
	int idx;

	dsp_enter();
	if (!pm->dvfs_lock) {
		dsp_info("DVFS is reinitialized\n");
		pm->dvfs = true;
		pm->dvfs_disable_count = 0;
		for (idx = 0; idx < DSP_O1_DEVFREQ_COUNT; ++idx)
			__dsp_hw_o1_pm_init(&pm->devfreq[idx]);
	}
	dsp_leave();
	return 0;
}

static int __dsp_hw_o1_pm_worker_init(struct dsp_pm *pm)
{
	int ret;
	struct sched_param param = { .sched_priority = MAX_RT_PRIO - 1 };

	kthread_init_worker(&pm->async_worker);

	pm->async_task = kthread_run(kthread_worker_fn, &pm->async_worker,
			"dsp_pm_worker");
	if (IS_ERR(pm->async_task)) {
		ret = PTR_ERR(pm->async_task);
		dsp_err("kthread of pm is not running(%d)\n", ret);
		goto p_err_kthread;
	}

	ret = sched_setscheduler_nocheck(pm->async_task, SCHED_FIFO, &param);
	if (ret) {
		dsp_err("Failed to set scheduler of pm worker(%d)\n", ret);
		goto p_err_sched;
	}

	return ret;
p_err_sched:
	kthread_stop(pm->async_task);
p_err_kthread:
	return ret;
}

static void __dsp_hw_o1_pm_worker_deinit(struct dsp_pm *pm)
{
	dsp_enter();
	kthread_stop(pm->async_task);
	dsp_leave();
}

static int dsp_hw_o1_pm_probe(struct dsp_system *sys)
{
	int ret;
	struct dsp_pm *pm;
	struct dsp_pm_devfreq *devfreq;
	unsigned int idx;

	dsp_enter();
	pm = &sys->pm;
	pm->sys = sys;

	pm->devfreq = kzalloc(sizeof(*devfreq) * DSP_O1_DEVFREQ_COUNT,
			GFP_KERNEL);
	if (!pm->devfreq) {
		ret = -ENOMEM;
		dsp_err("Failed to alloc dsp_pm_devfreq\n");
		goto p_err_devfreq;
	}

	devfreq = &pm->devfreq[DSP_O1_DEVFREQ_VPC];
	snprintf(devfreq->name, DSP_DEVFREQ_NAME_LEN, "vpc");
	devfreq->count = sizeof(vpc_table) / sizeof(*vpc_table);
	devfreq->table = vpc_table;
#ifdef CONFIG_EXYNOS_DSP_HW_O1
	devfreq->class_id = PM_QOS_VPC_THROUGHPUT;
#endif

	for (idx = 0; idx < DSP_O1_DEVFREQ_COUNT; ++idx) {
		devfreq = &pm->devfreq[idx];
		devfreq->pm = pm;

		devfreq->id = idx;
		devfreq->force_qos = -1;
		devfreq->min_qos = devfreq->count - 1;
		devfreq->dynamic_qos = devfreq->min_qos;
		devfreq->static_qos = devfreq->min_qos;

		kthread_init_work(&devfreq->work, dsp_hw_o1_devfreq_async);
		spin_lock_init(&devfreq->slock);
		devfreq->async_status = -1;
		devfreq->async_qos = -1;
	}

	pm->extra_devfreq = kzalloc(
			sizeof(*devfreq) * DSP_O1_EXTRA_DEVFREQ_COUNT,
			GFP_KERNEL);
	if (!pm->extra_devfreq) {
		ret = -ENOMEM;
		dsp_err("Failed to alloc extra_devfreq\n");
		goto p_err_extra_devfreq;
	}

	devfreq = &pm->extra_devfreq[DSP_O1_EXTRA_DEVFREQ_CL2];
	snprintf(devfreq->name, DSP_DEVFREQ_NAME_LEN, "cl2");
	devfreq->count = sizeof(cl2_table) / sizeof(*cl2_table);
	devfreq->table = cl2_table;
#ifdef CONFIG_EXYNOS_DSP_HW_O1
#if KERNEL_VERSION(5, 4, 0) <= LINUX_VERSION_CODE
	devfreq->use_freq_qos = true;
	devfreq->class_id = 7;
#else
	devfreq->class_id = PM_QOS_CLUSTER2_FREQ_MIN;
#endif
#endif

	devfreq = &pm->extra_devfreq[DSP_O1_EXTRA_DEVFREQ_CL1];
	snprintf(devfreq->name, DSP_DEVFREQ_NAME_LEN, "cl1");
	devfreq->count = sizeof(cl1_table) / sizeof(*cl1_table);
	devfreq->table = cl1_table;
#ifdef CONFIG_EXYNOS_DSP_HW_O1
#if KERNEL_VERSION(5, 4, 0) <= LINUX_VERSION_CODE
	devfreq->use_freq_qos = true;
	devfreq->class_id = 4;
#else
	devfreq->class_id = PM_QOS_CLUSTER1_FREQ_MIN;
#endif
#endif

	devfreq = &pm->extra_devfreq[DSP_O1_EXTRA_DEVFREQ_CL0];
	snprintf(devfreq->name, DSP_DEVFREQ_NAME_LEN, "cl0");
	devfreq->count = sizeof(cl0_table) / sizeof(*cl0_table);
	devfreq->table = cl0_table;
#ifdef CONFIG_EXYNOS_DSP_HW_O1
#if KERNEL_VERSION(5, 4, 0) <= LINUX_VERSION_CODE
	devfreq->use_freq_qos = true;
	devfreq->class_id = 0;
#else
	devfreq->class_id = PM_QOS_CLUSTER0_FREQ_MIN;
#endif
#endif

	devfreq = &pm->extra_devfreq[DSP_O1_EXTRA_DEVFREQ_INT];
	snprintf(devfreq->name, DSP_DEVFREQ_NAME_LEN, "int");
	devfreq->count = sizeof(int_table) / sizeof(*int_table);
	devfreq->table = int_table;
#ifdef CONFIG_EXYNOS_DSP_HW_O1
	devfreq->class_id = PM_QOS_DEVICE_THROUGHPUT;
#endif

	devfreq = &pm->extra_devfreq[DSP_O1_EXTRA_DEVFREQ_MIF];
	snprintf(devfreq->name, DSP_DEVFREQ_NAME_LEN, "mif");
	devfreq->count = sizeof(mif_table) / sizeof(*mif_table);
	devfreq->table = mif_table;
#ifdef CONFIG_EXYNOS_DSP_HW_O1
	devfreq->class_id = PM_QOS_BUS_THROUGHPUT;
#endif

	for (idx = 0; idx < DSP_O1_EXTRA_DEVFREQ_COUNT; ++idx) {
		devfreq = &pm->extra_devfreq[idx];
		devfreq->pm = pm;

		devfreq->id = idx;
		devfreq->min_qos = devfreq->count - 1;
	}

	pm_runtime_enable(sys->dev);
	mutex_init(&pm->lock);
	pm->dvfs = true;
	pm->dvfs_lock = false;

	ret = __dsp_hw_o1_pm_worker_init(pm);
	if (ret)
		goto p_err_worker;

	dsp_leave();
	return 0;
p_err_worker:
	kfree(pm->extra_devfreq);
p_err_extra_devfreq:
	kfree(pm->devfreq);
p_err_devfreq:
	return ret;
}

static void dsp_hw_o1_pm_remove(struct dsp_pm *pm)
{
	dsp_enter();
	__dsp_hw_o1_pm_worker_deinit(pm);
	mutex_destroy(&pm->lock);
	pm_runtime_disable(pm->sys->dev);
	kfree(pm->extra_devfreq);
	kfree(pm->devfreq);
	dsp_leave();
}

static const struct dsp_pm_ops o1_pm_ops = {
	.devfreq_active			= dsp_hw_o1_pm_devfreq_active,
	.update_devfreq_nolock		= dsp_hw_o1_pm_update_devfreq_nolock,
	.update_devfreq_nolock_async	=
		dsp_hw_o1_pm_update_devfreq_nolock_async,
	.update_extra_devfreq		= dsp_hw_o1_pm_update_extra_devfreq,
	.set_force_qos			= dsp_hw_o1_pm_set_force_qos,
	.dvfs_enable			= dsp_hw_o1_pm_dvfs_enable,
	.dvfs_disable			= dsp_hw_o1_pm_dvfs_disable,
	.update_devfreq_busy		= dsp_hw_o1_pm_update_devfreq_busy,
	.update_devfreq_idle		= dsp_hw_o1_pm_update_devfreq_idle,
	.update_devfreq_boot		= dsp_hw_o1_pm_update_devfreq_boot,
	.update_devfreq_max		= dsp_hw_o1_pm_update_devfreq_max,
	.update_devfreq_min		= dsp_hw_o1_pm_update_devfreq_min,
	.set_boot_qos			= dsp_hw_o1_pm_set_boot_qos,
	.boost_enable			= dsp_hw_o1_pm_boost_enable,
	.boost_disable			= dsp_hw_o1_pm_boost_disable,
	.enable				= dsp_hw_o1_pm_enable,
	.disable			= dsp_hw_o1_pm_disable,

	.open				= dsp_hw_o1_pm_open,
	.close				= dsp_hw_o1_pm_close,
	.probe				= dsp_hw_o1_pm_probe,
	.remove				= dsp_hw_o1_pm_remove,
};

int dsp_hw_o1_pm_init(void)
{
	int ret;

	dsp_enter();
	ret = dsp_pm_register_ops(DSP_DEVICE_ID_O1, &o1_pm_ops);
	if (ret)
		goto p_err;

	dsp_leave();
	return 0;
p_err:
	return ret;
}
