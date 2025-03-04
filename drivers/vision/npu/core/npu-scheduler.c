/*
 * Samsung Exynos SoC series NPU driver
 *
 * Copyright (c) 2019 Samsung Electronics Co., Ltd.
 *              http://www.samsung.com/
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 */

#include <linux/version.h>
#include <linux/delay.h>
#include <linux/random.h>
#include <soc/samsung/bts.h>

#include "npu-config.h"
#include "npu-scheduler-governor.h"
#include "npu-device.h"
#include "npu-llc.h"
#include "npu-util-regs.h"
#include <soc/samsung/exynos-devfreq.h>

static struct npu_scheduler_info *g_npu_scheduler_info;

static s32 npu_scheduler_get_freq_ceil(struct npu_scheduler_dvfs_info *d, s32 freq);

struct npu_scheduler_info *npu_scheduler_get_info(void)
{
	BUG_ON(!g_npu_scheduler_info);
	return g_npu_scheduler_info;
}

static s32 npu_devfreq_get_domain_freq(struct npu_scheduler_dvfs_info *d)
{
	struct platform_device *pdev;
	struct exynos_devfreq_data *data;
	u32 devfreq_type;

	pdev = d->dvfs_dev;
	if (!pdev) {
		npu_err("platform_device is NULL!\n");
		return d->min_freq;
	}

	data = platform_get_drvdata(pdev);
	if (!data) {
		npu_err("exynos_devfreq_data is NULL!\n");
		return d->min_freq;
	}

	devfreq_type = data->devfreq_type;

	return (s32)exynos_devfreq_get_domain_freq(devfreq_type);
}

/*****************************************************************************
 *****                         wrapper function                          *****
 *****************************************************************************/
#if LINUX_VERSION_CODE >= KERNEL_VERSION(5, 4, 0)
static void __npu_pm_qos_add_request(struct exynos_pm_qos_request *req,
					int exynos_pm_qos_class, s32 value)
{
	exynos_pm_qos_add_request(req, exynos_pm_qos_class, value);
}

static void __npu_pm_qos_update_request(struct exynos_pm_qos_request *req,
							s32 new_value)
{
	exynos_pm_qos_update_request(req, new_value);
}

static int __npu_pm_qos_get_class(struct exynos_pm_qos_request *req)
{
	return req->exynos_pm_qos_class;
}

void npu_pm_qos_update_request(
		struct npu_scheduler_dvfs_info *d,
		struct exynos_pm_qos_request *req, s32 new_freq)
{
	s32 target_freq;

	target_freq = npu_scheduler_get_freq_ceil(d, new_freq);
	exynos_pm_qos_update_request(req, target_freq);
	d->cur_freq = target_freq;
	npu_trace("freq for %s : %d %d (%d)\n",
			d->name, d->cur_freq, new_freq,
			exynos_pm_qos_request(req->exynos_pm_qos_class));
}

#else
static void __npu_pm_qos_add_request(struct pm_qos_request *req,
							s32 new_value)
{
	pm_qos_update_request(req, new_value);
}

static void __npu_pm_qos_update_request(struct pm_qos_request *req,
					int pm_qos_class, s32 value)
{
	pm_qos_add_request(req, pm_qos_class, value);
}

static int __npu_pm_qos_get_class(struct pm_qos_request *req)
{
	return req->pm_qos_class;
}

void npu_pm_qos_update_request(
		struct npu_scheduler_dvfs_info *d,
		struct exynos_pm_qos_request *req, s32 new_freq)
{
	s32 target_freq;

	target_freq = npu_scheduler_get_freq_ceil(d, new_freq);
	pm_qos_update_request(req, target_freq);
	d->cur_freq = target_freq;
	npu_trace("freq for %s : %d %d (%d)\n",
			d->name, d->cur_freq, new_freq,
			pm_qos_request(req->pm_qos_class));
}

#endif

int npu_scheduler_enable(struct npu_scheduler_info *info)
{
	if (!info) {
		npu_err("npu_scheduler_info is NULL!\n");
		return -EINVAL;
	}

	info->enable = 1;

	/* re-schedule work */
	if (info->activated) {
		cancel_delayed_work_sync(&info->sched_work);
		queue_delayed_work(info->sched_wq, &info->sched_work,
				msecs_to_jiffies(0));
	}

	npu_info("done\n");
	return 1;
}

int npu_scheduler_disable(struct npu_scheduler_info *info)
{
	struct npu_scheduler_dvfs_info *d;

	if (!info) {
		npu_err("npu_scheduler_info is NULL!\n");
		return -EINVAL;
	}

	info->enable = 0;
	mutex_lock(&info->exec_lock);
	list_for_each_entry(d, &info->ip_list, ip_list) {
		npu_pm_qos_update_request(d, &d->qos_req_min, d->min_freq);
	}
	mutex_unlock(&info->exec_lock);

	npu_info("done\n");
	return 1;
}

void npu_scheduler_activate_peripheral_dvfs(unsigned long freq)
{
	bool dvfs_active = false;
	bool is_core = false;
	int i;
	struct npu_scheduler_dvfs_info *d;
	struct npu_scheduler_info *info;

	info = g_npu_scheduler_info;

	if (!info->activated || info->is_dvfs_cmd)
		return;

	mutex_lock(&info->exec_lock);
	/* get NPU max freq */
	list_for_each_entry(d, &info->ip_list, ip_list) {
		if (!strcmp("NPU", d->name))
			break;
	}
	npu_trace("NPU maxlock at %ld\n", freq);
	if (freq >= d->max_freq)
		/* maxlock deactivated, DVFS should be active */
		dvfs_active = true;
	else
		/* maxlock activated, DVFS should be inactive */
		dvfs_active = false;

	list_for_each_entry(d, &info->ip_list, ip_list) {
		is_core = false;
		/* check for core DVFS */
		for (i = 0; i < (int)ARRAY_SIZE(npu_scheduler_core_name); i++)
			if (!strcmp(npu_scheduler_core_name[i], d->name)) {
				is_core = true;
				break;
			}

		/* only for peripheral DVFS */
		if (!is_core) {
			npu_pm_qos_update_request(d, &d->qos_req_min, d->min_freq);
			if (dvfs_active)
				d->activated = 1;
			else
				d->activated = 0;
			npu_trace("%s %sactivated\n", d->name,
					d->activated ? "" : "de");
		}
	}
	mutex_unlock(&info->exec_lock);
}

s32 npu_scheduler_get_freq_ceil(struct npu_scheduler_dvfs_info *d, s32 freq)
{
	struct dev_pm_opp *opp;
	unsigned long f;

	/* Calculate target frequency */
	f = (unsigned long) freq;
	opp = dev_pm_opp_find_freq_ceil(&d->dvfs_dev->dev, &f);

	if (IS_ERR(opp))
		return 0;
	else {
		dev_pm_opp_put(opp);
		return (s32)f;
	}
}

void npu_scheduler_set_bts(struct npu_scheduler_info *info)
{
	int ret = 0;

	if (info->mode == NPU_PERF_MODE_NPU_BOOST_ONEXE) {
		info->bts_scenindex = bts_get_scenindex("npu_normal");
		if (info->bts_scenindex < 0) {
			npu_err("bts_get_scenindex failed : %d\n", info->bts_scenindex);
			ret = info->bts_scenindex;
			goto p_err;
		}

		ret = bts_add_scenario(info->bts_scenindex);
		if (ret)
			npu_err("bts_add_scenario failed : %d\n", ret);
	} else if (info->mode == NPU_PERF_MODE_NPU_BOOST ||
			info->mode == NPU_PERF_MODE_NPU_BOOST_DLV3 ||
			info->mode == NPU_PERF_MODE_NPU_DN) {
		info->bts_scenindex = bts_get_scenindex("npu_performance");
		if (info->bts_scenindex < 0) {
			npu_err("bts_get_scenindex failed : %d\n", info->bts_scenindex);
			ret = info->bts_scenindex;
			goto p_err;
		}

		ret = bts_add_scenario(info->bts_scenindex);
		if (ret)
			npu_err("bts_add_scenario failed : %d\n", ret);
	} else {
		if (info->bts_scenindex > 0) {
			ret = bts_del_scenario(info->bts_scenindex);
			if (ret)
				npu_err("bts_del_scenario failed : %d\n", ret);
		} else
			npu_warn("BTS scen index[%d] is not initialized. "
					"Del scenario will be called.\n", info->bts_scenindex);
	}
p_err:
	return;
}

#define	HWACG_STATUS_ENABLE	(0x3A)
#define	HWACG_STATUS_DISABLE	(0x7F)
static void  __attribute__((unused)) npu_scheduler_hwacg_onoff(struct npu_scheduler_info *info)
{
	int ret = 0;

	if (!g_npu_scheduler_info->activated)
		return;

	npu_info("start HWACG setting, mode : %u\n", info->mode);
	if (info->mode == NPU_PERF_MODE_NPU_BOOST ||
			info->mode == NPU_PERF_MODE_NPU_BOOST_DLV3 ||
			info->mode == NPU_PERF_MODE_NPU_DN) {
		if (info->hwacg_status == HWACG_STATUS_DISABLE) {
			npu_info("HWACG already Disable\n");
			return;
		}

		ret = npu_cmd_map(&info->device->system, "hwacgdis");
		if (ret) {
			npu_err("fail(%d) in npu_cmd_map for hwacg_disable\n", ret);
		} else {
			info->hwacg_status = HWACG_STATUS_DISABLE;
			npu_info("HWACG Disable\n");
		}
	} else {
		if (info->hwacg_status == HWACG_STATUS_ENABLE) {
			npu_info("HWACG already Enable\n");
			return;
		}

		ret = npu_cmd_map(&info->device->system, "hwacgen");
		if (ret) {
			npu_err("fail(%d) in npu_cmd_map for hwacg_enable\n", ret);
		} else {
			info->hwacg_status = HWACG_STATUS_ENABLE;
			npu_info("HWACG Enable\n");
		}
	}
}

/* dt_name has prefix "samsung,npudvfs-" */
#define NPU_DVFS_CMD_PREFIX	"samsung,npudvfs-"
static int npu_get_dvfs_cmd_map(struct npu_system *system, struct dvfs_cmd_list *cmd_list)
{
	int i, ret = 0;
	u32 *cmd_clock = NULL;
	char dvfs_name[64], clock_name[64];
	char **cmd_dvfs;
	struct device *dev;

	dvfs_name[0] = '\0';
	clock_name[0] = '\0';
	strcpy(dvfs_name, NPU_DVFS_CMD_PREFIX);
	strcpy(clock_name, NPU_DVFS_CMD_PREFIX);
	strcat(strcat(dvfs_name, cmd_list->name), "-dvfs");
	strcat(strcat(clock_name, cmd_list->name), "-clock");

	dev = &(system->pdev->dev);
	cmd_list->count = of_property_count_strings(
			dev->of_node, dvfs_name);
	if (cmd_list->count <= 0) {
		probe_err("invalid dvfs_cmd list by %s\n", dvfs_name);
		cmd_list->list = NULL;
		cmd_list->count = 0;
		ret = -ENODEV;
		goto err_exit;
	}
	probe_info("%s dvfs %d commands\n", dvfs_name, cmd_list->count);

	cmd_list->list = (struct dvfs_cmd_map *)devm_kmalloc(dev,
				(cmd_list->count + 1) * sizeof(struct dvfs_cmd_map),
				GFP_KERNEL);
	if (!cmd_list->list) {
		probe_err("failed to alloc for dvfs cmd map\n");
		ret = -ENOMEM;
		goto err_exit;
	}
	(cmd_list->list)[cmd_list->count].name = NULL;

	cmd_dvfs = (char **)devm_kmalloc(dev,
			cmd_list->count * sizeof(char *), GFP_KERNEL);
	if (!cmd_dvfs) {
		probe_err("failed to alloc for dvfs_cmd devfreq for %s\n", dvfs_name);
		ret = -ENOMEM;
		goto err_exit;
	}
	ret = of_property_read_string_array(dev->of_node, dvfs_name, (const char **)cmd_dvfs, cmd_list->count);
	if (ret < 0) {
		probe_err("failed to get dvfs_cmd for %s (%d)\n", dvfs_name, ret);
		ret = -EINVAL;
		goto err_dvfs;
	}

	cmd_clock = (u32 *)devm_kmalloc(dev,
			cmd_list->count * NPU_REG_CMD_LEN * sizeof(u32), GFP_KERNEL);
	if (!cmd_clock) {
		probe_err("failed to alloc for dvfs_cmd clock for %s\n", clock_name);
		ret = -ENOMEM;
		goto err_dvfs;
	}
	ret = of_property_read_u32_array(dev->of_node, clock_name, (u32 *)cmd_clock,
			cmd_list->count * NPU_DVFS_CMD_LEN);
	if (ret) {
		probe_err("failed to get reg_cmd for %s (%d)\n", clock_name, ret);
		ret = -EINVAL;
		goto err_clock;
	}

	for (i = 0; i < cmd_list->count; i++) {
		(*(cmd_list->list + i)).name = *(cmd_dvfs + i);
		memcpy((void *)(&(*(cmd_list->list + i)).cmd),
				(void *)(cmd_clock + (i * NPU_DVFS_CMD_LEN)),
				sizeof(u32) * NPU_DVFS_CMD_LEN);
		probe_info("copy %s cmd (%lu)\n", *(cmd_dvfs + i), sizeof(u32) * NPU_DVFS_CMD_LEN);
	}
err_clock:
	devm_kfree(dev, cmd_clock);
err_dvfs:
	devm_kfree(dev, cmd_dvfs);
err_exit:
	return ret;
}

struct dvfs_cmd_list npu_dvfs_cmd_list[] = {
	{	.name = "normal",	.list = NULL,	.count = 0	},
	{	.name = "boostonexe",	.list = NULL,	.count = 0	},
	{	.name = "boost",	.list = NULL,	.count = 0	},
	{	.name = "cpu",		.list = NULL,	.count = 0	},
	{	.name = "DN",		.list = NULL,	.count = 0	},
	{	.name = NULL,		.list = NULL,	.count = 0	}
};

static int npu_init_dvfs_cmd_list(struct npu_system *system, struct npu_scheduler_info *info)
{
	int ret = 0;
	int i;

	BUG_ON(!info);

	info->dvfs_list = (struct dvfs_cmd_list *)npu_dvfs_cmd_list;

	for (i = 0; ((info->dvfs_list) + i)->name; i++) {
		if (npu_get_dvfs_cmd_map(system, (info->dvfs_list) + i) == -ENODEV)
			probe_info("No cmd for %s\n", ((info->dvfs_list) + i)->name);
		else
			probe_info("register cmd for %s\n", ((info->dvfs_list) + i)->name);
	}

	return ret;
}

int npu_dvfs_cmd(struct npu_scheduler_info *info, const struct dvfs_cmd_list *cmd_list)
{
	int ret = 0;
	size_t i;
	struct dvfs_cmd_map *t;
	struct npu_scheduler_dvfs_info *d;

	BUG_ON(!info);

	if (!cmd_list) {
		npu_info("No cmd for dvfs\n");
		/* no error, just skip */
		return 0;
	}

	info->is_dvfs_cmd = true;
	mutex_lock(&info->exec_lock);
	for (i = 0; i < cmd_list->count; ++i) {
		t = (cmd_list->list + i);
		if (t->name) {
			npu_info("set %s %s as %d\n", t->name, t->cmd ? "max" : "min", t->freq);
			d = get_npu_dvfs_info(info, t->name);
			switch (t->cmd) {
			case NPU_DVFS_CMD_MIN:
				__npu_pm_qos_update_request(&d->qos_req_min_dvfs_cmd, t->freq);
				break;
			case NPU_DVFS_CMD_MAX:
				d->max_freq = t->freq;
				__npu_pm_qos_update_request(&d->qos_req_max_dvfs_cmd, t->freq);
				break;
			default:
				break;
			}
		} else
			break;
	}
	mutex_unlock(&info->exec_lock);
	info->is_dvfs_cmd = false;
	return ret;
}

int npu_dvfs_cmd_map(struct npu_scheduler_info *info, const char *cmd_name)
{
	BUG_ON(!info);
	BUG_ON(!cmd_name);

	return npu_dvfs_cmd(info, (const struct dvfs_cmd_list *)get_npu_dvfs_cmd_map(info, cmd_name));
}

static struct npu_scheduler_control *npu_sched_ctl_ref;

/* Call-back from Protodrv */
static int npu_scheduler_save_result(struct npu_session *dummy, struct nw_result result)
{
	BUG_ON(!npu_sched_ctl_ref);
	npu_trace("scheduler request completed : result = %u\n", result.result_code);

	npu_sched_ctl_ref->result_code = result.result_code;
	atomic_set(&npu_sched_ctl_ref->result_available, 1);

	wake_up_all(&npu_sched_ctl_ref->wq);
	return 0;
}

static void npu_scheduler_set_llc_policy(struct npu_scheduler_info *info,
							struct npu_nw *nw)
{
	u32 llc_mode = 0;
	u32 llc_size = 0;

	llc_mode = info->llc_mode & 0x000000FF;
	/* Fisrt 2B means FLC ways, convert to bytes (1 ways = 512 KB */
	llc_size = ((info->llc_mode >> 24) & 0xFF) << 19;
	if (llc_mode == NPU_PERF_MODE_NPU_BOOST ||
		llc_mode == NPU_PERF_MODE_NPU_BOOST_DLV3) {
		nw->param0 = NPU_PERF_MODE_NPU_BOOST;
		nw->param1 = llc_size;
	}
	npu_info("llc_mode(%u), llc_size(%u)\n", nw->param0, nw->param1);
}

/* Send mode info to FW and check its response */
static int npu_scheduler_send_mode_to_hw(struct npu_session *session,
					struct npu_scheduler_info *info)
{
	int ret;
	struct npu_nw nw;
	int retry_cnt;

	memset(&nw, 0, sizeof(nw));
	nw.cmd = NPU_NW_CMD_MODE;
	nw.uid = session->uid;

	/* Set callback function on completion */
	nw.notify_func = npu_scheduler_save_result;
	/* Set LLC policy for FW */
	npu_scheduler_set_llc_policy(info, &nw);

	retry_cnt = 0;
	atomic_set(&npu_sched_ctl_ref->result_available, 0);
	while ((ret = npu_ncp_mgmt_put(&nw)) <= 0) {
		npu_info("queue full when inserting scheduler control message. Retrying...");
		if (retry_cnt++ >= NPU_SCHEDULER_CMD_POST_RETRY_CNT) {
			npu_err("timeout exceeded.\n");
			ret = -EWOULDBLOCK;
			goto err_exit;
		}
		msleep(NPU_SCHEDULER_CMD_POST_RETRY_INTERVAL);
	}
	/* Success */
	npu_info("scheduler control message has posted\n");

	ret = wait_event_timeout(
		npu_sched_ctl_ref->wq,
		atomic_read(&npu_sched_ctl_ref->result_available),
		NPU_SCHEDULER_HW_RESP_TIMEOUT);
	if (ret < 0) {
		npu_err("wait_event_timeout error(%d)\n", ret);
		goto err_exit;
	}
	if (!atomic_read(&npu_sched_ctl_ref->result_available)) {
		npu_err("timeout waiting H/W response\n");
		ret = -ETIMEDOUT;
		goto err_exit;
	}
	if (npu_sched_ctl_ref->result_code != NPU_ERR_NO_ERROR) {
		npu_err("hardware reply with NDONE(%d)\n", npu_sched_ctl_ref->result_code);
		ret = -EFAULT;
		goto err_exit;
	}
	ret = 0;

err_exit:
	return ret;
}

static int npu_scheduler_create_attrs(
		struct device *dev,
		struct device_attribute attrs[],
		int device_attr_num)
{
	unsigned long i;
	int rc;

	probe_info("creates scheduler attributes %d sysfs\n",
			device_attr_num);
	for (i = 0; i < device_attr_num; i++) {
		probe_info("sysfs info %s %p %p\n",
				attrs[i].attr.name, attrs[i].show, attrs[i].store);
		rc = device_create_file(dev, &(attrs[i]));
		if (rc)
			goto create_attrs_failed;
	}
	goto create_attrs_succeed;

create_attrs_failed:
	while (i--)
		device_remove_file(dev, &(attrs[i]));
create_attrs_succeed:
	return rc;
}

static int npu_scheduler_init_dt(struct npu_scheduler_info *info)
{
	int i, count, ret = 0;
	unsigned long f = 0;
	struct dev_pm_opp *opp;
	char *tmp_name;
	struct npu_scheduler_dvfs_info *dinfo;
	struct of_phandle_args pa;

	BUG_ON(!info);

	probe_info("scheduler init by devicetree\n");

	count = of_property_count_strings(info->dev->of_node, "samsung,npusched-names");
	if (IS_ERR_VALUE((unsigned long)count)) {
		probe_err("invalid npusched list in %s node\n", info->dev->of_node->name);
		ret = -EINVAL;
		goto err_exit;
	}

	for (i = 0; i < count; i += 2) {
		/* get dvfs info */
		dinfo = (struct npu_scheduler_dvfs_info *)devm_kzalloc(info->dev,
				sizeof(struct npu_scheduler_dvfs_info), GFP_KERNEL);
		if (!dinfo) {
			probe_err("failed to alloc dvfs info\n");
			ret = -ENOMEM;
			goto err_exit;
		}

		/* get dvfs name (same as IP name) */
		ret = of_property_read_string_index(info->dev->of_node,
				"samsung,npusched-names", i,
				(const char **)&dinfo->name);
		if (ret) {
			probe_err("failed to read dvfs name %d from %s node : %d\n",
					i, info->dev->of_node->name, ret);
			goto err_dinfo;
		}
		/* get governor name  */
		ret = of_property_read_string_index(info->dev->of_node,
				"samsung,npusched-names", i + 1,
				(const char **)&tmp_name);
		if (ret) {
			probe_err("failed to read dvfs name %d from %s node : %d\n",
					i + 1, info->dev->of_node->name, ret);
			goto err_dinfo;
		}

		probe_info("set up %s with %s governor\n", dinfo->name, tmp_name);
		/* get and set governor */

		dinfo->gov = npu_scheduler_governor_get(info, tmp_name);
		/* need to add error check for dinfo->gov */

		/* get dvfs and pm-qos info */
		ret = of_parse_phandle_with_fixed_args(info->dev->of_node,
				"samsung,npusched-dvfs",
				NPU_SCHEDULER_DVFS_TOTAL_ARG_NUM, i / 2, &pa);
		if (ret) {
			probe_err("failed to read dvfs args %d from %s node : %d\n",
					i / 2, info->dev->of_node->name, ret);
			goto err_dinfo;
		}

		dinfo->dvfs_dev = of_find_device_by_node(pa.np);
		if (!dinfo->dvfs_dev) {
			probe_err("invalid dt node for %s devfreq device with %d args\n",
					dinfo->name, pa.args_count);
			ret = -EINVAL;
			goto err_dinfo;
		}
		f = ULONG_MAX;
		opp = dev_pm_opp_find_freq_floor(&dinfo->dvfs_dev->dev, &f);
		if (IS_ERR(opp)) {
			probe_err("invalid max freq for %s\n", dinfo->name);
			ret = -EINVAL;
			goto err_dinfo;
		} else {
			dinfo->max_freq = f;
			dev_pm_opp_put(opp);
		}
		f = 0;
		opp = dev_pm_opp_find_freq_ceil(&dinfo->dvfs_dev->dev, &f);
		if (IS_ERR(opp)) {
			probe_err("invalid min freq for %s\n", dinfo->name);
			ret = -EINVAL;
			goto err_dinfo;
		} else {
			dinfo->min_freq = f;
			dev_pm_opp_put(opp);
		}

		__npu_pm_qos_add_request(&dinfo->qos_req_min,
				get_pm_qos_min(dinfo->name),
				dinfo->min_freq);
		probe_info("add pm_qos min request %s %d as %d\n",
				dinfo->name,
				__npu_pm_qos_get_class(&dinfo->qos_req_min),
				dinfo->min_freq);
		__npu_pm_qos_add_request(&dinfo->qos_req_min_dvfs_cmd,
				get_pm_qos_min(dinfo->name),
				dinfo->min_freq);
		probe_info("add pm_qos min request %s %d as %d\n",
				dinfo->name,
				__npu_pm_qos_get_class(&dinfo->qos_req_min_dvfs_cmd),
				dinfo->min_freq);
		__npu_pm_qos_add_request(&dinfo->qos_req_max_dvfs_cmd,
				get_pm_qos_max(dinfo->name),
				dinfo->max_freq);
		probe_info("add pm_qos max request %s %d as %d\n",
				dinfo->name,
				__npu_pm_qos_get_class(&dinfo->qos_req_max_dvfs_cmd),
				dinfo->max_freq);
		__npu_pm_qos_add_request(&dinfo->qos_req_max_afm,
				get_pm_qos_max(dinfo->name),
				dinfo->max_freq);
		probe_info("add pm_qos max request %s %d as %d\n",
				dinfo->name,
				__npu_pm_qos_get_class(&dinfo->qos_req_max_afm),
				dinfo->max_freq);
		__npu_pm_qos_add_request(&dinfo->qos_req_min_nw_boost,
				get_pm_qos_min(dinfo->name),
				dinfo->min_freq);
		probe_info("add pm_qos min request for boosting %s %d as %d\n",
				dinfo->name,
				__npu_pm_qos_get_class(&dinfo->qos_req_min_nw_boost),
				dinfo->min_freq);

		probe_info("%s %d %d %d %d %d %d\n", dinfo->name,
				pa.args[0], pa.args[1], pa.args[2],
				pa.args[3], pa.args[4], pa.args[5]);

		/* reset values */
		dinfo->cur_freq = dinfo->min_freq;
		dinfo->delay = 0;
		dinfo->limit_min = 0;
		dinfo->limit_max = INT_MAX;
		dinfo->curr_up_delay = 0;
		dinfo->curr_down_delay = 0;

		/* get governor property slot */
		if (dinfo->gov) {
			dinfo->gov_prop = (void *)dinfo->gov->ops->init_prop(dinfo, info->dev, pa.args);
			if (!dinfo->gov_prop) {
				probe_err("failed to init governor property for %s\n",
						dinfo->name);
				goto err_dinfo;
			}

			/* add device in governor */
			list_add_tail(&dinfo->dev_list, &dinfo->gov->dev_list);
		}

		/* add device in scheduler */
		list_add_tail(&dinfo->ip_list, &info->ip_list);

		probe_info("add %s in list\n", dinfo->name);
	}

	return ret;
err_dinfo:
	devm_kfree(info->dev, dinfo);
err_exit:
	return ret;
}

static void npu_scheduler_work(struct work_struct *work);
static void npu_scheduler_afm_work(struct work_struct *work);
static void npu_scheduler_boost_off_work(struct work_struct *work);
#ifdef CONFIG_NPU_ARBITRATION
static void npu_arbitration_work(struct work_struct *work);
#endif

static int npu_scheduler_init_info(s64 now, struct npu_scheduler_info *info)
{
	int i, ret = 0;
	const char *mode_name;

	probe_info("scheduler info init\n");

	info->enable = 1;	/* default enable */
	ret = of_property_read_string(info->dev->of_node,
			"samsung,npusched-mode", &mode_name);
	if (ret)
		info->mode = NPU_PERF_MODE_NORMAL;
	else {
		for (i = 0; i < ARRAY_SIZE(npu_perf_mode_name); i++) {
			if (!strcmp(npu_perf_mode_name[i], mode_name))
				break;
		}
		if (i == ARRAY_SIZE(npu_perf_mode_name)) {
			probe_err("Fail on npu_scheduler_init_info, number out of bounds in array=[%lu]\n",
									ARRAY_SIZE(npu_perf_mode_name));
			return -1;
		}
		info->mode = i;
	}

	for (i = 0; i < NPU_PERF_MODE_NUM; i++)
		info->mode_ref_cnt[i] = 0;

	probe_info("NPU mode : %s\n", npu_perf_mode_name[info->mode]);
	info->bts_scenindex = -1;
	info->llc_status = 0;
	info->hwacg_status = HWACG_STATUS_ENABLE;
	info->ocp_warn_status = 0;

	info->time_stamp = now;
	info->time_diff = 0;
	info->freq_interval = 1;
	info->tfi = 0;
	info->boost_count = 0;

	ret = of_property_read_u32(info->dev->of_node,
			"samsung,npusched-period", &info->period);
	if (ret)
		info->period = NPU_SCHEDULER_DEFAULT_PERIOD;

	probe_info("NPU period %d ms\n", info->period);

	ret = of_property_read_u32(info->dev->of_node,
			"samsung,npusched-afmlimit", &info->afm_limit_freq);
	if (ret)
		info->afm_limit_freq = NPU_SCHEDULER_DEFAULT_AFMLIMIT;

	probe_info("NPU afmlimit %d ms\n", info->afm_limit_freq);

	/* initialize idle information */
	info->idle_load = 0;	/* 0.01 unit */

	/* initialize FPS information */
	info->fps_policy = NPU_SCHEDULER_FPS_MAX;
	mutex_init(&info->fps_lock);
	INIT_LIST_HEAD(&info->fps_frame_list);
	INIT_LIST_HEAD(&info->fps_load_list);
	info->fps_load = 0;	/* 0.01 unit */
	ret = of_property_read_u32(info->dev->of_node,
			"samsung,npusched-tpf-others", &info->tpf_others);
	if (ret)
		info->tpf_others = 0;

	/* initialize RQ information */
	mutex_init(&info->rq_lock);
	info->rq_start_time = now;
	info->rq_idle_start_time = now;
	info->rq_idle_time = 0;
	info->rq_load = 0;	/* 0.01 unit */

	/* initialize common information */
	ret = of_property_read_u32(info->dev->of_node,
			"samsung,npusched-load-policy", &info->load_policy);
	if (ret)
		info->load_policy = NPU_SCHEDULER_LOAD_FPS_RQ;
	/* load window : only for RQ or IDLE load */
	ret = of_property_read_u32(info->dev->of_node,
			"samsung,npusched-loadwin", &info->load_window_size);
	if (ret)
		info->load_window_size = NPU_SCHEDULER_DEFAULT_LOAD_WIN_SIZE;
	info->load_window_index = 0;
	for (i = 0; i < NPU_SCHEDULER_LOAD_WIN_MAX; i++)
		info->load_window[i] = 0;
	info->load = 0;		/* 0.01 unit */
	info->load_idle_time = 0;

	INIT_LIST_HEAD(&info->gov_list);
	INIT_LIST_HEAD(&info->ip_list);

	/* de-activated scheduler */
	info->activated = 0;
	mutex_init(&info->exec_lock);
	info->is_dvfs_cmd = false;
#ifdef CONFIG_PM_SLEEP
	npu_wake_lock_init(info->dev, &info->sws,
				NPU_WAKE_LOCK_SUSPEND, "npu-scheduler");
#endif

	memset((void *)&info->sched_ctl, 0, sizeof(struct npu_scheduler_control));
	npu_sched_ctl_ref = &info->sched_ctl;

	init_waitqueue_head(&info->sched_ctl.wq);

	INIT_DELAYED_WORK(&info->sched_work, npu_scheduler_work);
	INIT_DELAYED_WORK(&info->sched_afm_work, npu_scheduler_afm_work);
	INIT_DELAYED_WORK(&info->boost_off_work, npu_scheduler_boost_off_work);

#ifdef CONFIG_NPU_ARBITRATION
#ifdef CONFIG_PM_SLEEP
	npu_wake_lock_init(info->dev, &info->aws,
				NPU_WAKE_LOCK_SUSPEND, "npu-arbitration");
#endif
	INIT_DELAYED_WORK(&info->arbitration_work, npu_arbitration_work);
	init_waitqueue_head(&info->waitq);
#endif

	probe_info("scheduler info init done\n");

	return 0;
}

void npu_scheduler_gate(
		struct npu_device *device, struct npu_frame *frame, bool idle)
{
	int ret = 0;
	struct npu_scheduler_info *info;
	struct npu_scheduler_fps_load *l;
	struct npu_scheduler_fps_load *tl;
	u32 bid = NPU_BOUND_CORE0;

	BUG_ON(!device);
	info = device->sched;

	mutex_lock(&info->fps_lock);
	tl = NULL;
	/* find load entry */
	list_for_each_entry(l, &info->fps_load_list, list) {
		if (l->uid == frame->uid) {
			tl = l;
			break;
		}
	}
	/* if not, error !! */
	if (!tl) {
		npu_err("fps load data for uid %d NOT found\n", frame->uid);
		mutex_unlock(&info->fps_lock);
		return;
	} else {
		bid = tl->bound_id;
		mutex_unlock(&info->fps_lock);
	}

	npu_trace("try to gate %s for UID %d\n", idle ? "on" : "off", frame->uid);
	if (idle) {
		if (bid == NPU_BOUND_UNBOUND) {
			info->used_count--;
			if (!info->used_count) {
				/* gating activated */
				/* HWACG */
				//npu_hwacg(&device->system, true);
				/* clock OSC switch */
				ret = npu_core_clock_off(&device->system);
				if (ret)
					npu_err("fail(%d) in npu_core_clock_off\n", ret);
			}
		}
	} else {
		if (bid == NPU_BOUND_UNBOUND) {
			if (!info->used_count) {
				/* gating deactivated */
				/* clock OSC switch */
				ret = npu_core_clock_on(&device->system);
				if (ret)
					npu_err("fail(%d) in npu_core_clock_on\n", ret);
				/* HWACG */
				//npu_hwacg(&device->system, false);
			}
			info->used_count++;
		}
	}
}

void npu_scheduler_fps_update_idle(
		struct npu_device *device, struct npu_frame *frame, bool idle)
{
	s64 now, frame_time;
	struct npu_scheduler_info *info;
	struct npu_scheduler_fps_frame *f;
	struct npu_scheduler_fps_load *l;
	struct npu_scheduler_fps_load *tl;
	struct list_head *p;
	s64 new_init_freq, old_init_freq, cur_freq;
	struct npu_scheduler_dvfs_info *d;

	BUG_ON(!device);
	info = device->sched;

	if (list_empty(&info->ip_list)) {
		npu_err("no device for scheduler\n");
		return;
	}

	now = npu_get_time_us();

	npu_trace("uid %d, fid %d : %s\n",
			frame->uid, frame->frame_id, (idle?"done":"processing"));

	mutex_lock(&info->fps_lock);
	tl = NULL;
	/* find load entry */
	list_for_each_entry(l, &info->fps_load_list, list) {
		if (l->uid == frame->uid) {
			tl = l;
			break;
		}
	}
	/* if not, error !! */
	if (!tl) {
		npu_err("fps load data for uid %d NOT found\n", frame->uid);
		mutex_unlock(&info->fps_lock);
		return;
	}

	if (idle) {
		list_for_each(p, &info->fps_frame_list) {
			f = list_entry(p, struct npu_scheduler_fps_frame, list);
			if (f->uid == frame->uid && f->frame_id == frame->frame_id) {
				tl->tfc--;
				/* all frames are processed, check for process time */
				if (!tl->tfc) {
					frame_time = now - f->start_time;
					tl->tpf = frame_time / tl->frame_count + info->tpf_others;
					frame->output->timestamp[4].tv_usec = frame_time;

					/* get current freqency of NPU */
					list_for_each_entry(d, &info->ip_list, ip_list) {
						/* calculate the initial frequency */
						if (!strcmp("NPU", d->name)) {
							mutex_lock(&info->exec_lock);
							cur_freq = (s64) npu_devfreq_get_domain_freq(d);
							new_init_freq = tl->tpf * cur_freq / tl->requested_tpf;
							old_init_freq = (s64) tl->init_freq_ratio * (s64) d->max_freq / 10000;
							/* calculate exponential moving average */
							tl->init_freq_ratio = (old_init_freq / 2 + new_init_freq / 2) * 10000 / (s64) d->max_freq;
							if (tl->init_freq_ratio > 10000)
								tl->init_freq_ratio = 10000;
							mutex_unlock(&info->exec_lock);
							break;
						}
					}

					if (info->load_policy == NPU_SCHEDULER_LOAD_FPS ||
							info->load_policy == NPU_SCHEDULER_LOAD_FPS_RQ)
						info->freq_interval = tl->frame_count /
							NPU_SCHEDULER_FREQ_INTERVAL + 1;
					tl->frame_count = 0;

					tl->fps_load = tl->tpf * 10000 / tl->requested_tpf;
					tl->time_stamp = now;

					npu_trace("load (uid %d) (%lld)/%lld %lld updated\n",
							tl->uid, tl->tpf, tl->requested_tpf, tl->init_freq_ratio);
				}
				/* delete frame entry */
				list_del(p);
				if (tl->tfc)
					npu_trace("uid %d fid %d / %d frames left\n",
						tl->uid, f->frame_id, tl->tfc);
				else
					npu_trace("uid %d fid %d / %lld us per frame\n",
						tl->uid, f->frame_id, tl->tpf);
				if (f)
					kfree(f);
				break;
			}
		}
	} else {
		f = kzalloc(sizeof(struct npu_scheduler_fps_frame), GFP_KERNEL);
		if (!f) {
			npu_err("fail to alloc fps frame info (U%d, F%d)",
					frame->uid, frame->frame_id);
			mutex_unlock(&info->fps_lock);
			return;
		}
		f->uid = frame->uid;
		f->frame_id = frame->frame_id;
		f->start_time = now;
		list_add(&f->list, &info->fps_frame_list);

		/* add frame counts */
		tl->tfc++;
		tl->frame_count++;

		npu_trace("new frame (uid %d (%d frames active), fid %d) added\n",
				tl->uid, tl->tfc, f->frame_id);
	}
	mutex_unlock(&info->fps_lock);
}

static void npu_scheduler_calculate_fps_load(s64 now, struct npu_scheduler_info *info)
{
	unsigned int tmp_load, tmp_min_load, tmp_max_load, tmp_load_count;
	struct npu_scheduler_fps_load *l;

	tmp_load = 0;
	tmp_max_load = 0;
	tmp_min_load = 1000000;
	tmp_load_count = 0;

	mutex_lock(&info->fps_lock);
	list_for_each_entry(l, &info->fps_load_list, list) {
		/* reset FPS load in inactive status */
		if (info->time_stamp >
				l->time_stamp + l->tpf *
				NPU_SCHEDULER_FPS_LOAD_RESET_FRAME_NUM) {
			//npu_trace("UID %d FPS load reset\n", l->uid);
			l->fps_load = 0;
		}

		tmp_load += l->fps_load;
		if (tmp_max_load < l->fps_load)
			tmp_max_load = l->fps_load;
		if (tmp_min_load > l->fps_load)
			tmp_min_load = l->fps_load;
		tmp_load_count++;
	}
	mutex_unlock(&info->fps_lock);

	switch (info->fps_policy) {
	case NPU_SCHEDULER_FPS_MIN:
		info->fps_load = tmp_min_load;
		break;
	case NPU_SCHEDULER_FPS_MAX:
		info->fps_load = tmp_max_load;
		break;
	case NPU_SCHEDULER_FPS_AVG2:	/* average without min, max */
		tmp_load -= tmp_min_load;
		tmp_load -= tmp_max_load;
		tmp_load_count -= 2;
		info->fps_load = tmp_load / tmp_load_count;
		break;
	case NPU_SCHEDULER_FPS_AVG:
		if (tmp_load_count > 0)
			info->fps_load = tmp_load / tmp_load_count;
		break;
	default:
		npu_err("Invalid FPS policy : %d\n", info->fps_policy);
		break;
	}
	//npu_dbg("FPS load : %d\n", info->fps_load);
}

void npu_scheduler_rq_update_idle(struct npu_device *device, bool idle)
{
	s64 now, idle_time;
	struct npu_scheduler_info *info;

	BUG_ON(!device);
	info = device->sched;

	now = npu_get_time_us();

	mutex_lock(&info->rq_lock);

	if (idle) {
		info->rq_idle_start_time = now;
	} else {
		idle_time = now - info->rq_idle_start_time;
		info->rq_idle_time += idle_time;
		info->rq_idle_start_time = 0;
	}

	mutex_unlock(&info->rq_lock);
}

static void npu_scheduler_calculate_rq_load(s64 now, struct npu_scheduler_info *info)
{
	s64 total_diff;

	mutex_lock(&info->rq_lock);

	/* temperary finish idle time */
	if (info->rq_idle_start_time)
		info->rq_idle_time += (now - info->rq_idle_start_time);

	/* calculate load */
	total_diff = now - info->rq_start_time;
	info->rq_load = (total_diff - info->rq_idle_time) * 10000 / total_diff;
	//npu_dbg("RQ load : %d (idle %d, total %d)\n",
	//		(int)info->rq_load, (int)info->rq_idle_time, (int)total_diff);

	/* reset data */
	info->rq_start_time = now;
	if (info->rq_idle_start_time)
		/* restart idle timer */
		info->rq_idle_start_time = now;
	else
		info->rq_idle_start_time = 0;
	info->rq_idle_time = 0;

	mutex_unlock(&info->rq_lock);
}

static int npu_scheduler_check_limit(struct npu_scheduler_info *info,
		struct npu_scheduler_dvfs_info *d)
{
	return 0;
}

static int npu_scheduler_set_freq(struct npu_scheduler_dvfs_info *d, s32 freq)
{
	//npu_dbg("set freq for %s : %d => %d\n", d->name, d->cur_freq, freq);
	if (d->cur_freq == freq) {
		//npu_dbg("stick to current freq : %d\n", d->cur_freq);
		return 0;
	}

	npu_pm_qos_update_request(d, &d->qos_req_min, freq);
	//npu_pm_qos_update_request(d, &d->qos_req_max, freq);

	return 0;
}

static void npu_scheduler_execute_policy(struct npu_scheduler_info *info)
{
	struct npu_scheduler_dvfs_info *d;
	s32	freq = 0;

	if (list_empty(&info->ip_list)) {
		npu_err("no device for scheduler\n");
		return;
	}

	mutex_lock(&info->exec_lock);
	list_for_each_entry(d, &info->ip_list, ip_list) {
		if (!d->activated) {
			npu_trace("%s deactivated\n", d->name);
			continue;
		}

		if (d->delay > 0)
			d->delay -= info->time_diff;

		/* check limitation */
		if (npu_scheduler_check_limit(info, d)) {
			/* emergency status */
		} else {
			/* no emergency status but delay */
			if (d->delay > 0) {
				npu_info("no update by delay %d\n", d->delay);
				continue;
			}
		}

		freq = d->cur_freq;
		/* calculate frequency */
		if (d->gov && d->gov->ops->target)
			d->gov->ops->target(info, d, &freq);

		/* check mode limit */
		if (freq < d->mode_min_freq[info->mode])
			freq = d->mode_min_freq[info->mode];

		/* set frequency */
		if (info->tfi >= info->freq_interval)
			npu_scheduler_set_freq(d, freq);
	}
	mutex_unlock(&info->exec_lock);
	return;
}

static int npu_scheduler_set_mode_freq(struct npu_scheduler_info *info, int uid)
{
	int ret = 0;
	struct npu_scheduler_fps_load *l;
	struct npu_scheduler_fps_load *tl;
	struct npu_scheduler_dvfs_info *d;
	s32	freq = 0;

	if (!info->enable) {
		npu_dbg("scheduler disabled\n");
		return 0;
	}

	if (list_empty(&info->ip_list)) {
		npu_err("no device for scheduler\n");
		return ret;
	}

	if (info->mode == NPU_PERF_MODE_NPU_BOOST_ONEXE) {
		mutex_lock(&info->exec_lock);
		list_for_each_entry(d, &info->ip_list, ip_list) {
			/* no actual governor or no name */
			if (!d->gov || !strcmp(d->gov->name, ""))
				continue;

			freq = d->cur_freq;

			/* check mode limit */
			if (freq < d->mode_min_freq[info->mode])
				freq = d->mode_min_freq[info->mode];

			if (uid == -1) {
				/* requested through sysfs */
				freq = d->max_freq;
			} else {
				/* requested through ioctl() */
				tl = NULL;
				/* find load entry */
				list_for_each_entry(l, &info->fps_load_list, list) {
					if (l->uid == uid) {
						tl = l;
						break;
					}
				}
				/* if not, error !! */
				if (!tl) {
					npu_err("fps load data for uid %d NOT found\n", uid);
					freq = d->max_freq;
				} else {
					freq = tl->init_freq_ratio * d->max_freq / 10000;
					freq = npu_scheduler_get_freq_ceil(d, freq);
					if (freq < d->cur_freq)
						freq = d->cur_freq;
				}
			}
			d->is_init_freq = 1;

			/* set frequency */
			npu_scheduler_set_freq(d, freq);
		}
		mutex_unlock(&info->exec_lock);
	} else {
		/* set dvfs command list for the mode */
		ret = npu_dvfs_cmd_map(info, npu_perf_mode_name[info->mode]);
	}
	return ret;
}

static unsigned int __npu_scheduler_get_load(
		struct npu_scheduler_info *info, u32 load)
{
	int i, load_sum = 0;

	if (info->load_window_index >= info->load_window_size)
		info->load_window_index = 0;

	info->load_window[info->load_window_index++] = load;

	for (i = 0; i < info->load_window_size; i++)
		load_sum += info->load_window[i];

	return (unsigned int)(load_sum / info->load_window_size);
}

int npu_scheduler_boost_on(struct npu_scheduler_info *info)
{
	struct npu_scheduler_dvfs_info *d;

	npu_info("boost on (count %d)\n", info->boost_count + 1);
	if (likely(info->boost_count == 0)) {
		if (unlikely(list_empty(&info->ip_list))) {
			npu_err("no device for scheduler\n");
			return -EPERM;
		}

		mutex_lock(&info->exec_lock);
		list_for_each_entry(d, &info->ip_list, ip_list) {
			/* no actual governor or no name */
			if (!d->gov || !strcmp(d->gov->name, ""))
				continue;

			__npu_pm_qos_update_request(&d->qos_req_min_nw_boost, d->max_freq);
			npu_info("boost on freq for %s : %d\n", d->name, d->max_freq);
		}
		mutex_unlock(&info->exec_lock);
	}
	info->boost_count++;
	return 0;
}

static int __npu_scheduler_boost_off(struct npu_scheduler_info *info)
{
	int ret = 0;
	struct npu_scheduler_dvfs_info *d;

	if (list_empty(&info->ip_list)) {
		npu_err("no device for scheduler\n");
		ret = -EPERM;
		goto p_err;
	}

	mutex_lock(&info->exec_lock);
	list_for_each_entry(d, &info->ip_list, ip_list) {
		/* no actual governor or no name */
		if (!d->gov || !strcmp(d->gov->name, ""))
			continue;

		__npu_pm_qos_update_request(&d->qos_req_min_nw_boost, d->min_freq);
		npu_info("boost off freq for %s : %d\n", d->name, d->min_freq);
	}
	mutex_unlock(&info->exec_lock);
	return ret;
p_err:
	return ret;
}

int npu_scheduler_boost_off(struct npu_scheduler_info *info)
{
	int ret = 0;

	info->boost_count--;
	npu_info("boost off (count %d)\n", info->boost_count);

	if (info->boost_count <= 0) {
		ret = __npu_scheduler_boost_off(info);
		info->boost_count = 0;
	} else if (info->boost_count > 0)
		queue_delayed_work(info->sched_wq, &info->boost_off_work,
				msecs_to_jiffies(NPU_SCHEDULER_BOOST_TIMEOUT));

	return ret;
}

int npu_scheduler_boost_off_timeout(struct npu_scheduler_info *info, s64 timeout)
{
	int ret = 0;

	if (timeout == 0) {
		npu_scheduler_boost_off(info);
	} else if (timeout > 0) {
		queue_delayed_work(info->sched_wq, &info->boost_off_work,
				msecs_to_jiffies(timeout));
	} else {
		npu_err("timeout cannot be less than 0\n");
		ret = -EPERM;
		goto p_err;
	}
	return ret;
p_err:
	return ret;
}

static void npu_scheduler_boost_off_work(struct work_struct *work)
{
	struct npu_scheduler_info *info;

	/* get basic information */
	info = container_of(work, struct npu_scheduler_info, boost_off_work.work);
	npu_scheduler_boost_off(info);
}

static void __npu_scheduler_work(struct npu_scheduler_info *info)
{
	s64 now;
	int is_last_idle = 0;
	u32 load, load_idle;

	if (!info->enable) {
		npu_dbg("scheduler disabled\n");
		return;
	}

	now = npu_get_time_us();
	info->time_diff = now - info->time_stamp;
	info->time_stamp = now;

	/* get idle information */

	/* get FPS information */
	npu_scheduler_calculate_fps_load(now, info);

	/* get RQ information */
	npu_scheduler_calculate_rq_load(now, info);

	/* get global npu load */
	switch (info->load_policy) {
	case NPU_SCHEDULER_LOAD_IDLE:
		load = info->idle_load;
		break;
	case NPU_SCHEDULER_LOAD_FPS:
	case NPU_SCHEDULER_LOAD_FPS_RQ:
		load = info->fps_load;
		break;
	case NPU_SCHEDULER_LOAD_RQ:
		load = info->rq_load;
		break;
	default:
		load = 0;
		break;
	}

	info->load = __npu_scheduler_get_load(info, load);

	if (info->load_policy == NPU_SCHEDULER_LOAD_FPS_RQ)
		load_idle = info->rq_load;
	else
		load_idle = info->load;

	if (load_idle) {
		if (info->load_idle_time) {
			info->load_idle_time +=
				(info->time_diff * (10000 - load_idle) / 10000);
			is_last_idle = 1;
		} else
			info->load_idle_time = 0;
	} else
		info->load_idle_time += info->time_diff;

	info->tfi++;
	//npu_trace("__npu scheduler work : tfi %d freq interval %d "
	//		"timestamp %llu (diff %llu), idle time %llu %s\n",
	//		info->tfi, info->freq_interval,
	//		info->time_stamp, info->time_diff, info->load_idle_time,
	//		is_last_idle ? "last" : "");

	/* decide frequency change */
	npu_scheduler_execute_policy(info);
	if (info->tfi >= info->freq_interval)
		info->tfi = 0;

	if (is_last_idle)
		info->load_idle_time = 0;
}

static void npu_scheduler_work(struct work_struct *work)
{
	struct npu_scheduler_info *info;

	/* get basic information */
	info = container_of(work, struct npu_scheduler_info, sched_work.work);

	__npu_scheduler_work(info);

	//npu_dbg("queue scheduler work after %dms\n", info->period);
	queue_delayed_work(info->sched_wq, &info->sched_work,
			msecs_to_jiffies(info->period));
}

#ifdef CONFIG_NPU_ARBITRATION
int npu_arbitration_save_result(struct npu_session *sess, struct nw_result nw_result)
{
	int ret = 0;

	g_npu_scheduler_info->result_code = nw_result.result_code;
	g_npu_scheduler_info->result_value = nw_result.nw.result_value;
	wake_up_interruptible(&g_npu_scheduler_info->waitq);

	return ret;
}

static u32 arbitration_algo_random_test(struct npu_scheduler_info *info)
{
	struct npu_system *system = &info->device->system;
	u32 max_cores = system->max_npu_core;
	u32 cores_tobe_active = 0;

	/* random number from info->fw_min_active_cores to max_cores */
	cores_tobe_active =
		get_random_u32() % (max_cores + 1 - info->fw_min_active_cores)
		       + info->fw_min_active_cores;

	return cores_tobe_active;
}

static u32 arbitration_algo(struct npu_scheduler_info *info)
{
	struct npu_system *system = &info->device->system;
	struct npu_sessionmgr *s_mgr;
	struct npu_session *session;
	u64 cumulative_tps, max_tps, min_tps, tps;
	u64 now, total_cmdq_isa_size = 0;
	u32 active_session, count, session_count;
	u32 cores_tobe_active = 0, max_cores = 0;
	u32 temp1 = 0, temp2 = 0;
	int max_bind_core = NPU_BOUND_UNBOUND;
	int i;
	bool debug = false;

	if (debug)
		return arbitration_algo_random_test(info);

	max_cores = system->max_npu_core;
	s_mgr = &info->device->sessionmgr;

	if (s_mgr->count_thread_ncp[max_cores]) {
		cores_tobe_active = max_cores;
	} else {
		active_session = atomic_read(&info->device->vertex.start_cnt.refcount);

		cumulative_tps = 0;
		max_tps = 0;
		min_tps = -1;
		now = npu_get_time_us();
		for (count = 0, session_count = 0;
		     count < active_session && session_count < NPU_MAX_SESSION;
		     session_count++) {
			/* get fps, record max, record min, add cumulative.*/
			if (!s_mgr->session[session_count]) {
				npu_trace("session was closed\n");
				continue;
			}

			if (s_mgr->session[session_count]->ss_state <
			    BIT(NPU_SESSION_STATE_START)) {
				npu_trace("session not in start state\n");
				continue;
			}

			count++;

			session = s_mgr->session[session_count];
			/* If the last q happened 10 seconds back,
			 * the values are too stale
			 */
			if (abs(now - session->last_q_time_stamp) >
			    configs[LASTQ_TIME_THRESHOLD]) {
				npu_trace("now = 0x%llx lastq = 0x%llx\n",
					now, session->last_q_time_stamp);
				continue;
			}

			if (max_bind_core < session->sched_param.bound_id)
				max_bind_core = session->sched_param.bound_id;

			tps = (session->total_flc_transfer_size +
				session->total_sdma_transfer_size) *
				session->inferencefreq;
			npu_trace("count = %d flc-%ld sdma-%ld inf_freq=%d addrofInfFreq=0x%llx tps-%d\n",
				count, session->total_flc_transfer_size,
				session->total_sdma_transfer_size,
				session->inferencefreq, &session->inferencefreq,
				tps);

			cumulative_tps += tps;

			if (max_tps < tps)
				max_tps = tps;
			if (min_tps > tps)
				min_tps = tps;
			npu_trace("For this session, cmdq isa size = 0x%x\n",
				session->cmdq_isa_size);
			total_cmdq_isa_size += (session->cmdq_isa_size *
						session->inferencefreq);
			npu_trace("Transaction_Per_Core : %d, CMDQ_Complexity_Per_Core : %d, LastQ_Threshold : %d\n",
				configs[TRANSACTIONS_PER_CORE],
				configs[CMDQ_COMPLEXITY_PER_CORE],
				configs[LASTQ_TIME_THRESHOLD]);
		}

		npu_trace("cumulative_tps = 0x%llx TRANSACTIONS_PER_CORE = 0x%llx\n",
		cumulative_tps, configs[TRANSACTIONS_PER_CORE]);
		npu_trace("Total cmdq isa size = 0x%llx\n", total_cmdq_isa_size);

		for (i = max_cores; i > 0; i--) {
			if (cumulative_tps > (i - 1) *
			    configs[TRANSACTIONS_PER_CORE]) {
				temp1 = i;
				break;
			}
		}

		/* Expected complexity of NCP using cmdq sizes */
		for (i = max_cores; i > 0; i--) {
			if (total_cmdq_isa_size > (i - 1) *
			    configs[CMDQ_COMPLEXITY_PER_CORE]) {
				temp2 = i;
				break;
			}
		}

		cores_tobe_active = (temp1 + temp2) / 2;
	}

	for (i = max_cores; i > 0; i--) {
		if (cores_tobe_active < i && s_mgr->count_thread_ncp[i]) {
			cores_tobe_active = i;
			break;
		}
	}

	if ((cores_tobe_active < (max_bind_core + 1)) &&
	    (max_bind_core != NPU_BOUND_UNBOUND))
		cores_tobe_active = max_bind_core + 1;

	/* FW needs atleast info->fw_min_active_cores to be active */
	if (cores_tobe_active < info->fw_min_active_cores)
		cores_tobe_active = info->fw_min_active_cores;

	return cores_tobe_active;
}

static void __npu_arbitration_work(struct npu_scheduler_info *info)
{
	struct npu_system *system = &info->device->system;
	struct npu_nw req = {};
	u32 cores_tobe_active = 0;
	int ret = 0;
	int prev = info->num_cores_active;

	cores_tobe_active = arbitration_algo(info);

	if (info->num_cores_active == cores_tobe_active)
		return;

	if (info->num_cores_active < cores_tobe_active) {
		/* enable clocks first, then send message to FW */
		system->core_tobe_active_num = cores_tobe_active;
		ret = npu_core_gate(info->device, false);
		if (ret) {
			npu_err("fail(%d) in npu_core_gate\n", ret);
			return;
		}
	}
	/* send a firmware mailbox command and get response */
	req.npu_req_id = 0;
	req.result_code = 0;
	req.cmd = NPU_NW_CMD_CORE_CTL;
	req.notify_func = npu_arbitration_save_result;
	req.param0 = cores_tobe_active;

	info->result_code = NPU_SYSTEM_JUST_STARTED;
	/* queue the message for sending */
	ret = npu_ncp_mgmt_put(&req);
	if (!ret) {
		npu_err("fail in npu_ncp_mgmt_put\n");
		return;
	}
	/* wait for response from FW */
	wait_event_interruptible(info->waitq,
				 info->result_code != NPU_SYSTEM_JUST_STARTED);

	if (info->result_code != NPU_ERR_NO_ERROR) {
		/* firmware failed or did not accept the change in number of
		 * active cores. hence keep info->num_active_cores unchanged
		 */
		npu_err("failure response for core_ctl message\n");
	} else {
		/* 'info->result_value' number of cores are active in FW */
		if (likely(info->result_value <= system->max_npu_core))
			info->num_cores_active = info->result_value;
	}

	if (system->core_active_num > info->num_cores_active) {
		/* either FW succeeded in disabling the cores as we asked, or
		 * FW didn't succeed in enabling cores as we asked.
		 * hence disable the extra cores
		 */
		system->core_tobe_active_num = info->num_cores_active;
		ret = npu_core_gate(info->device, true);
		if (ret) {
			npu_err("fail(%d) in npu_core_gate\n", ret);
			return;
		}
	}

	npu_notice("core on/off changes: previous %d -> %d now active\n",
			prev, info->num_cores_active);
}

static void npu_arbitration_work(struct work_struct *work)
{
	struct npu_scheduler_info *info;

	/* get basic information */
	info = container_of(work, struct npu_scheduler_info, arbitration_work.work);

	__npu_arbitration_work(info);

	queue_delayed_work(info->arbitration_wq, &info->arbitration_work,
			msecs_to_jiffies(info->period));
}

static void npu_arbitration_queue_work(struct npu_scheduler_info *info)
{
#ifdef CONFIG_PM_SLEEP
	npu_wake_lock_timeout(info->aws, msecs_to_jiffies(100));
#endif
	queue_delayed_work(info->arbitration_wq, &info->arbitration_work,
			msecs_to_jiffies(100));
}
static void npu_arbitration_cancel_work(struct npu_scheduler_info *info)
{
	cancel_delayed_work_sync(&info->arbitration_work);
}

#else /* !CONFIG_NPU_ARBITRATION */

static inline void npu_arbitration_queue_work(struct npu_scheduler_info *info) { }
static inline void npu_arbitration_cancel_work(struct npu_scheduler_info *info) { }

#endif /* CONFIG_NPU_ARBITRATION */


static ssize_t npu_show_attrs_scheduler(struct device *dev,
		struct device_attribute *attr, char *buf);
static ssize_t npu_store_attrs_scheduler(struct device *dev,
		struct device_attribute *attr,
		const char *buf, size_t count);

static struct device_attribute npu_scheduler_attrs[] = {
	NPU_SCHEDULER_ATTR(scheduler_enable),
	NPU_SCHEDULER_ATTR(scheduler_mode),

	NPU_SCHEDULER_ATTR(timestamp),
	NPU_SCHEDULER_ATTR(timediff),
	NPU_SCHEDULER_ATTR(period),

	NPU_SCHEDULER_ATTR(load_idle_time),
	NPU_SCHEDULER_ATTR(load),
	NPU_SCHEDULER_ATTR(load_policy),

	NPU_SCHEDULER_ATTR(idle_load),

	NPU_SCHEDULER_ATTR(fps_tpf),
	NPU_SCHEDULER_ATTR(fps_load),
	NPU_SCHEDULER_ATTR(fps_policy),
	NPU_SCHEDULER_ATTR(fps_all_load),
	NPU_SCHEDULER_ATTR(fps_target),

	NPU_SCHEDULER_ATTR(rq_load),
};

enum {
	NPU_SCHEDULER_ENABLE = 0,
	NPU_SCHEDULER_MODE,

	NPU_SCHEDULER_TIMESTAMP,
	NPU_SCHEDULER_TIMEDIFF,
	NPU_SCHEDULER_PERIOD,

	NPU_SCHEDULER_LOAD_IDLE_TIME,
	NPU_SCHEDULER_LOAD,
	NPU_SCHEDULER_LOAD_POLICY,

	NPU_SCHEDULER_IDLE_LOAD,

	NPU_SCHEDULER_FPS_TPF,
	NPU_SCHEDULER_FPS_LOAD,
	NPU_SCHEDULER_FPS_POLICY,
	NPU_SCHEDULER_FPS_ALL_LOAD,
	NPU_SCHEDULER_FPS_TARGET,

	NPU_SCHEDULER_RQ_LOAD,
};

ssize_t npu_show_attrs_scheduler(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	const ptrdiff_t offset = attr - npu_scheduler_attrs;
	int i = 0, k;
	struct npu_scheduler_fps_load *l;

	BUG_ON(!g_npu_scheduler_info);

	switch (offset) {
	case NPU_SCHEDULER_ENABLE:
		i += scnprintf(buf + i, PAGE_SIZE - i, "%d\n",
			g_npu_scheduler_info->enable);
		break;
	case NPU_SCHEDULER_MODE:
		for (k = 0; k < ARRAY_SIZE(npu_perf_mode_name); k++) {
			if (k == g_npu_scheduler_info->mode)
				i += scnprintf(buf + i, PAGE_SIZE - i, "*");
			else
				i += scnprintf(buf + i, PAGE_SIZE - i, " ");
			i += scnprintf(buf + i, PAGE_SIZE - i, "%d %s\n",
				k, npu_perf_mode_name[k]);
		}
		break;

	case NPU_SCHEDULER_TIMESTAMP:
		i += scnprintf(buf + i, PAGE_SIZE - i, "%llu\n",
			g_npu_scheduler_info->time_stamp);
		break;
	case NPU_SCHEDULER_TIMEDIFF:
		i += scnprintf(buf + i, PAGE_SIZE - i, "%llu\n",
			g_npu_scheduler_info->time_diff);
		break;
	case NPU_SCHEDULER_PERIOD:
		i += scnprintf(buf + i, PAGE_SIZE - i, "%d\n",
			g_npu_scheduler_info->period);
		break;
	case NPU_SCHEDULER_LOAD_IDLE_TIME:
		i += scnprintf(buf + i, PAGE_SIZE - i, "%llu\n",
			g_npu_scheduler_info->load_idle_time);
		break;
	case NPU_SCHEDULER_LOAD:
		i += scnprintf(buf + i, PAGE_SIZE - i, "%d\n",
			g_npu_scheduler_info->load);
		break;
	case NPU_SCHEDULER_LOAD_POLICY:
		for (k = 0; k < ARRAY_SIZE(npu_scheduler_load_policy_name); k++) {
			if (k == g_npu_scheduler_info->load_policy)
				i += scnprintf(buf + i, PAGE_SIZE - i, "*");
			else
				i += scnprintf(buf + i, PAGE_SIZE - i, " ");
			i += scnprintf(buf + i, PAGE_SIZE - i, "%d %s\n",
				k, npu_scheduler_load_policy_name[k]);
		}
		break;

	case NPU_SCHEDULER_IDLE_LOAD:
		i += scnprintf(buf + i, PAGE_SIZE - i, "%d\n",
			g_npu_scheduler_info->idle_load);
		break;

	case NPU_SCHEDULER_FPS_TPF:
		i += scnprintf(buf + i, PAGE_SIZE - i, " uid\ttpf\tKPI\tinit\n");
		mutex_lock(&g_npu_scheduler_info->fps_lock);
		list_for_each_entry(l, &g_npu_scheduler_info->fps_load_list, list) {
			i += scnprintf(buf + i, PAGE_SIZE - i, " %d\t%lld\t%lld\t%lld\n",
			l->uid, l->tpf, l->requested_tpf, l->init_freq_ratio);
		}
		mutex_unlock(&g_npu_scheduler_info->fps_lock);
		break;
	case NPU_SCHEDULER_FPS_LOAD:
		i += scnprintf(buf + i, PAGE_SIZE - i, "%d\n",
			g_npu_scheduler_info->fps_load);
		break;
	case NPU_SCHEDULER_FPS_POLICY:
		for (k = 0; k < ARRAY_SIZE(npu_scheduler_fps_policy_name); k++) {
			if (k == g_npu_scheduler_info->fps_policy)
				i += scnprintf(buf + i, PAGE_SIZE - i, "*");
			else
				i += scnprintf(buf + i, PAGE_SIZE - i, " ");
			i += scnprintf(buf + i, PAGE_SIZE - i, "%d %s\n",
				k, npu_scheduler_fps_policy_name[k]);
		}
		break;
	case NPU_SCHEDULER_FPS_ALL_LOAD:
		i += scnprintf(buf + i, PAGE_SIZE - i, " uid\tload\n");
		mutex_lock(&g_npu_scheduler_info->fps_lock);
		list_for_each_entry(l, &g_npu_scheduler_info->fps_load_list, list) {
			i += scnprintf(buf + i, PAGE_SIZE - i, " %d\t%d\n",
			l->uid, l->fps_load);
		}
		mutex_unlock(&g_npu_scheduler_info->fps_lock);
		break;
	case NPU_SCHEDULER_FPS_TARGET:
		i += scnprintf(buf + i, PAGE_SIZE - i, " uid\ttarget\n");
		mutex_lock(&g_npu_scheduler_info->fps_lock);
		list_for_each_entry(l, &g_npu_scheduler_info->fps_load_list, list) {
			i += scnprintf(buf + i, PAGE_SIZE - i, " %d\t%lld\n",
			l->uid, l->requested_tpf);
		}
		mutex_unlock(&g_npu_scheduler_info->fps_lock);
		break;

	case NPU_SCHEDULER_RQ_LOAD:
		i += scnprintf(buf + i, PAGE_SIZE - i, "%d\n",
			g_npu_scheduler_info->rq_load);
		break;

	default:
		break;
	}

	return i;
}

ssize_t npu_store_attrs_scheduler(struct device *dev,
		struct device_attribute *attr,
		const char *buf, size_t count)
{
	const ptrdiff_t offset = attr - npu_scheduler_attrs;
	int ret = 0;
	int x = 0;
	int y = 0;
	struct npu_scheduler_fps_load *l;
	struct npu_session sess;

	BUG_ON(!g_npu_scheduler_info);

	switch (offset) {
	case NPU_SCHEDULER_ENABLE:
		if (sscanf(buf, "%d", &x) > 0) {
			if (x)
				x = 1;
			g_npu_scheduler_info->enable = x;
		}
		break;
	case NPU_SCHEDULER_MODE:
		if (sscanf(buf, "%d", &x) > 0) {
			if (x >= ARRAY_SIZE(npu_perf_mode_name)) {
				npu_err("Invalid mode value %d, ignored\n", x);
				ret = -EINVAL;
				break;
			}
			g_npu_scheduler_info->mode = x;
			if (g_npu_scheduler_info->activated) {
				npu_scheduler_set_mode_freq(g_npu_scheduler_info, -1);
				npu_scheduler_set_bts(g_npu_scheduler_info);
				npu_set_llc(g_npu_scheduler_info);
				npu_scheduler_hwacg_onoff(g_npu_scheduler_info);
				memset(&sess, 0, sizeof(struct npu_session));
				npu_scheduler_send_mode_to_hw(&sess, g_npu_scheduler_info);
			}
		}
		break;

	case NPU_SCHEDULER_TIMESTAMP:
		break;
	case NPU_SCHEDULER_TIMEDIFF:
		break;
	case NPU_SCHEDULER_PERIOD:
		if (sscanf(buf, "%d", &x) > 0)
			g_npu_scheduler_info->period = x;
		break;
	case NPU_SCHEDULER_LOAD_IDLE_TIME:
		break;
	case NPU_SCHEDULER_LOAD:
		break;
	case NPU_SCHEDULER_LOAD_POLICY:
		if (sscanf(buf, "%d", &x) > 0) {
			if (x >= ARRAY_SIZE(npu_scheduler_load_policy_name)) {
				npu_err("Invalid load policy : %d\n", x);
				ret = -EINVAL;
				break;
			}
			g_npu_scheduler_info->load_policy = x;
		}
		break;

	case NPU_SCHEDULER_IDLE_LOAD:
		break;

	case NPU_SCHEDULER_FPS_TPF:
		break;
	case NPU_SCHEDULER_FPS_LOAD:
		break;
	case NPU_SCHEDULER_FPS_POLICY:
		if (sscanf(buf, "%d", &x) > 0) {
			if (x >= ARRAY_SIZE(npu_scheduler_fps_policy_name)) {
				npu_err("Invalid FPS policy : %d\n", x);
				ret = -EINVAL;
				break;
			}
			g_npu_scheduler_info->fps_policy = x;
		}
		break;
	case NPU_SCHEDULER_FPS_ALL_LOAD:
		break;
	case NPU_SCHEDULER_FPS_TARGET:
		if (sscanf(buf, "%d %d", &x, &y) > 0) {
			mutex_lock(&g_npu_scheduler_info->fps_lock);
			list_for_each_entry(l, &g_npu_scheduler_info->fps_load_list, list) {
				if (l->uid == x) {
					l->requested_tpf = y;
					break;
				}
			}
			mutex_unlock(&g_npu_scheduler_info->fps_lock);
		}
		break;

	case NPU_SCHEDULER_RQ_LOAD:
		break;

	default:
		break;
	}

	if (!ret)
		ret = count;
	return ret;
}

static struct npu_system *npu_afm_system;

#define NPU_OCP_FEATURE		(0x0)
#define NPU_AFM_ENABLE		(0x1)
#define NPU_AFM_DISABLE		(0x0)
#define	NPU_OCP_LIMIT_SET	(0x1)
#define	NPU_OCP_LIMIT_REL	(0x0)

static void npu_scheduler_control_afm_grobal(struct npu_system *system, int enable)
{
	if (enable)
		npu_cmd_map(system, "glafmen");
	else
		npu_cmd_map(system, "glafmdis");
}

static void npu_scheduler_control_afm_interrupt(struct npu_system *system, int enable)
{
	if (enable)
		npu_cmd_map(system, "afmitren");
	else
		npu_cmd_map(system, "afmitrdis");
}

static void npu_scheduler_control_afm_throttling(struct npu_system *system, int enable)
{

	if (enable)
		npu_cmd_map(system, "afmthren");
	else
		npu_cmd_map(system, "afmthrdis");

	npu_cmd_map(system, "printafm");
}

static int npu_scheduler_check_afm_interrupt(void)
{
	return npu_cmd_map(npu_afm_system, "chkafmitr");
}

static void npu_scheduler_clear_afm_interrupt(void)
{
	int val = 0;

	val = npu_cmd_map(npu_afm_system, "chkafmitr");
	if (val)
		npu_cmd_map(npu_afm_system, "clrafmitr");
}

static void npu_scheduler_clear_afm_tdc(void)
{
	npu_cmd_map(npu_afm_system, "clrafmtdc");
}

static irqreturn_t npu_scheduler_ocp_isr(int irq, void *data)
{
	if (!npu_scheduler_check_afm_interrupt())
		return IRQ_NONE;

	npu_scheduler_control_afm_interrupt(npu_afm_system, NPU_AFM_DISABLE);

	g_npu_scheduler_info->ocp_warn_status = 1;

	queue_delayed_work(g_npu_scheduler_info->sched_wq,
			&g_npu_scheduler_info->sched_afm_work,
			msecs_to_jiffies(0));

	if (g_npu_scheduler_info->activated) {
		npu_scheduler_clear_afm_tdc();
		npu_scheduler_clear_afm_interrupt();
	}

	return IRQ_HANDLED;
}

static void __npu_scheduler_afm_work(struct npu_scheduler_info *info, unsigned int limit)
{
	struct npu_scheduler_dvfs_info *d;

	mutex_lock(&info->exec_lock);
	list_for_each_entry(d, &info->ip_list, ip_list) {
		if (!strcmp(d->name, "NPU")) {
			/* set frequency */
			if (limit == NPU_OCP_LIMIT_SET)
				__npu_pm_qos_update_request(&d->qos_req_max_afm, info->afm_limit_freq);
			/* limit == NPU_OCP_LIMIT_REL */
			else
				__npu_pm_qos_update_request(&d->qos_req_max_afm, d->max_freq);
		}
	}
	mutex_unlock(&info->exec_lock);
}

static void npu_scheduler_afm_work(struct work_struct *work)
{
	struct npu_scheduler_info *info;

	info = container_of(work, struct npu_scheduler_info, sched_afm_work.work);
	if (!info->activated) {
		__npu_scheduler_afm_work(info, NPU_OCP_LIMIT_REL);
		return;
	}

	npu_dbg("afm work start(ocp_status : %d)\n", info->ocp_warn_status);
	if (!info->ocp_warn_status) {
		npu_dbg("Trying release afm_limit_freq\n");
		__npu_scheduler_afm_work(info, NPU_OCP_LIMIT_REL);

		if (info->activated) {
			npu_scheduler_clear_afm_tdc();
			npu_scheduler_clear_afm_interrupt();
			npu_scheduler_control_afm_interrupt(npu_afm_system, NPU_AFM_ENABLE);
		}
		npu_dbg("End release afm_limit_freq\n");
		return;
	}
	__npu_scheduler_afm_work(info, NPU_OCP_LIMIT_SET);

	info->ocp_warn_status = 0;
	queue_delayed_work(info->sched_wq, &info->sched_afm_work,
			msecs_to_jiffies(info->period));
	npu_dbg("afm work end\n");
}

static int npu_scheduler_ocp_probe(struct npu_device *device)
{
	int ret = 0;
	struct npu_system *system = &device->system;
	struct device *dev = &system->pdev->dev;

	ret = devm_request_irq(dev, system->irq2, npu_scheduler_ocp_isr, IRQF_TRIGGER_HIGH, "exynos-npu", NULL);
	if (ret)
		probe_err("fail(%d) in devm_request_irq(2)\n", ret);

	npu_afm_system = system;
	return ret;
}

static int npu_scheduler_ocp_release(struct npu_device *device)
{
	int ret = 0;
	struct npu_system *system = &device->system;
	struct device *dev = &system->pdev->dev;

	devm_free_irq(dev, system->irq2, NULL);

	npu_afm_system = NULL;
	return ret;
}

int npu_scheduler_probe(struct npu_device *device)
{
	int ret = 0;
	s64 now;
	struct npu_scheduler_info *info;

	BUG_ON(!device);

	info = kzalloc(sizeof(struct npu_scheduler_info), GFP_KERNEL);
	if (!info) {
		probe_err("failed to alloc info\n");
		ret = -ENOMEM;
		goto err_info;
	}
	memset(info, 0, sizeof(struct npu_scheduler_info));
	device->sched = info;
	device->system.qos_setting.info = info;
	info->device = device;
	info->dev = device->dev;

	now = npu_get_time_us();

	/* init scheduler data */
	ret = npu_scheduler_init_info(now, info);
	if (ret) {
		probe_err("fail(%d) init info\n", ret);
		ret = -EFAULT;
		goto err_info;
	}

	ret = npu_scheduler_create_attrs(info->dev,
			npu_scheduler_attrs, ARRAY_SIZE(npu_scheduler_attrs));
	if (ret) {
		probe_err("fail(%d) create attributes\n", ret);
		ret = -EFAULT;
		goto err_info;
	}

	/* register governors */
	ret = npu_scheduler_governor_register(info);
	if (ret) {
		probe_err("fail(%d) register governor\n", ret);
		ret = -EFAULT;
		goto err_info;
	}

	/* init scheduler with dt */
	ret = npu_scheduler_init_dt(info);
	if (ret) {
		probe_err("fail(%d) initial setting with dt\n", ret);
		ret = -EFAULT;
		goto err_info;
	}

	/* init dvfs command list with dt */
	ret = npu_init_dvfs_cmd_list(&device->system, info);
	if (ret) {
		probe_err("fail(%d) initial dvfs command setting with dt\n", ret);
		ret = -EFAULT;
		goto err_info;
	}

	info->sched_wq = create_singlethread_workqueue(dev_name(device->dev));
	if (!info->sched_wq) {
		probe_err("fail to create workqueue\n");
		ret = -EFAULT;
		goto err_info;
	}

	/* register ocp irq */
	ret = npu_scheduler_ocp_probe(device);
	if (ret) {
		probe_err("fail(%d) register ocp irq\n", ret);
		ret = -EFAULT;
		goto err_info;
	}
#ifdef CONFIG_NPU_ARBITRATION
	info->arbitration_wq = create_singlethread_workqueue(dev_name(device->dev));
	if (!info->arbitration_wq) {
		probe_err("fail to create arbitration workqueue\n");
		ret = -EFAULT;
		goto err_info;
	}
#endif

	g_npu_scheduler_info = info;
	probe_info("done\n");
	return ret;
err_info:
	if (info)
		kfree(info);
	g_npu_scheduler_info = NULL;
	return ret;
}

int npu_scheduler_release(struct npu_device *device)
{
	int ret = 0;
	struct npu_scheduler_info *info;

	BUG_ON(!device);
	info = device->sched;

	ret = npu_scheduler_ocp_release(device);
	if (ret) {
		probe_err("fail(%d) relase ocp\n", ret);
		ret = -EFAULT;
		goto p_err;
	}

	ret = npu_scheduler_governor_unregister(info);
	if (ret) {
		probe_err("fail(%d) unregister governor\n", ret);
		ret = -EFAULT;
		goto p_err;
	}

p_err:
	g_npu_scheduler_info = NULL;
	return ret;
}

int npu_scheduler_load(struct npu_device *device, const struct npu_session *session)
{
	int ret = 0;
	struct npu_scheduler_info *info;
	struct npu_scheduler_fps_load *l;
	struct npu_scheduler_dvfs_info *d;

	BUG_ON(!device);
	info = device->sched;

	mutex_lock(&info->fps_lock);
	/* create load data for session */
	l = kzalloc(sizeof(struct npu_scheduler_fps_load), GFP_KERNEL);
	if (!l) {
		npu_err("failed to alloc fps_load\n");
		ret = -ENOMEM;
		mutex_unlock(&info->fps_lock);
		return ret;
	}
	l->uid = session->uid;
	l->priority = session->sched_param.priority;
	l->bound_id = session->sched_param.bound_id;
	l->tfc = 0;		/* temperary frame count */
	l->frame_count = 0;
	l->tpf = 0;		/* time per frame */
	l->requested_tpf = NPU_SCHEDULER_DEFAULT_TPF;
	l->init_freq_ratio = 10000; /* 100%, max frequency */
	l->mode = NPU_PERF_MODE_NORMAL;
	list_add(&l->list, &info->fps_load_list);

	npu_info("load for uid %d (p %d b %d) added\n",
			l->uid, l->priority, l->bound_id);

	mutex_lock(&info->exec_lock);
	list_for_each_entry(d, &info->ip_list, ip_list) {
		/* no actual governor or no name */
		if (!d->gov || !strcmp(d->gov->name, ""))
			continue;


		npu_info("DVFS start : %s (%s)\n",
				d->name, d->gov ? d->gov->name : "");
		if (d->gov)
			d->gov->ops->start(d);

		/* set frequency */
		npu_scheduler_set_freq(d, d->max_freq);
		d->is_init_freq = 1;
		d->activated = 1;
	}
	mutex_unlock(&info->exec_lock);
	mutex_unlock(&info->fps_lock);

	return ret;
}

void npu_scheduler_unload(struct npu_device *device, const struct npu_session *session)
{
	struct npu_scheduler_info *info;
	struct npu_scheduler_fps_load *l;
	struct npu_scheduler_dvfs_info *d;

	BUG_ON(!device);
	info = device->sched;

	mutex_lock(&info->fps_lock);
	/* delete load data for session */
	list_for_each_entry(l, &info->fps_load_list, list) {
		if (l->uid == session->uid) {
			list_del(&l->list);
			if (l)
				kfree(l);
			npu_info("load for uid %d deleted\n", session->uid);
			break;
		}
	}

	if (list_empty(&info->fps_load_list)) {
		mutex_lock(&info->exec_lock);
		list_for_each_entry(d, &info->ip_list, ip_list) {
			npu_info("DVFS stop : %s (%s)\n",
					d->name, d->gov ? d->gov->name : "");
			d->activated = 0;
			if (d->gov)
				d->gov->ops->stop(d);
			d->is_init_freq = 0;
		}
		mutex_unlock(&info->exec_lock);
	}
	mutex_unlock(&info->fps_lock);

	npu_dbg("done\n");
}

void npu_scheduler_update_sched_param(struct npu_device *device, struct npu_session *session)
{
	struct npu_scheduler_info *info;
	struct npu_scheduler_fps_load *l;

	BUG_ON(!device);
	info = device->sched;

	mutex_lock(&info->fps_lock);
	/* delete load data for session */
	list_for_each_entry(l, &info->fps_load_list, list) {
		if (l->uid == session->uid) {
			l->priority = session->sched_param.priority;
			l->bound_id = session->sched_param.bound_id;
			npu_info("update sched param for uid %d (p %d b %d)\n",
				l->uid, l->priority, l->bound_id);
			break;
		}
	}
	mutex_unlock(&info->fps_lock);
}

void npu_scheduler_set_init_freq(
		struct npu_device *device, npu_uid_t session_uid)
{
	s32 init_freq;
	struct npu_scheduler_info *info;
	struct npu_scheduler_dvfs_info *d;
	struct npu_scheduler_fps_load *l;
	struct npu_scheduler_fps_load *tl;

	BUG_ON(!device);
	info = device->sched;

	if (!info->enable) {
		npu_dbg("scheduler disabled\n");
		return;
	}

	mutex_lock(&info->fps_lock);
	tl = NULL;
	/* find load entry */
	list_for_each_entry(l, &info->fps_load_list, list) {
		if (l->uid == session_uid) {
			tl = l;
			break;
		}
	}

	/* if not, error !! */
	if (!tl) {
		npu_err("fps load data for uid %d NOT found\n", session_uid);
		mutex_unlock(&info->fps_lock);
		return;
	}

	mutex_lock(&info->exec_lock);
	list_for_each_entry(d, &info->ip_list, ip_list) {
		/* no actual governor or no name */
		if (!d->gov || !strcmp(d->gov->name, ""))
			continue;

		if (d->is_init_freq == 0) {
			init_freq = tl->init_freq_ratio * d->max_freq / 10000;
			init_freq = npu_scheduler_get_freq_ceil(d, init_freq);

			if (init_freq < d->cur_freq)
				init_freq = d->cur_freq;

			/* set frequency */
			npu_scheduler_set_freq(d, init_freq);
			d->is_init_freq = 1;
		}
	}
	mutex_unlock(&info->exec_lock);
	mutex_unlock(&info->fps_lock);
}

int npu_scheduler_open(struct npu_device *device)
{
	int ret = 0;
	struct npu_scheduler_info *info;

	BUG_ON(!device);
	info = device->sched;

	if (info->llc_status) {
		info->mode = NPU_PERF_MODE_NORMAL;
		npu_set_llc(info);
	}

	/* activate scheduler */
	info->activated = 1;
	info->is_dvfs_cmd = false;

	/* set dvfs command list for default mode */
	npu_dvfs_cmd_map(info, npu_perf_mode_name[info->mode]);

#ifdef CONFIG_NPU_SCHEDULER_OPEN_CLOSE
#ifdef CONFIG_PM_SLEEP
	npu_wake_lock_timeout(info->sws, msecs_to_jiffies(100));
#endif
	queue_delayed_work(info->sched_wq, &info->sched_work,
			msecs_to_jiffies(100));

	npu_arbitration_cancel_work(info);
	npu_arbitration_queue_work(info);
#endif
	npu_info("done\n");
	return ret;
}

int npu_scheduler_close(struct npu_device *device)
{
	int i, ret = 0;
	struct npu_scheduler_info *info;

	BUG_ON(!device);
	info = device->sched;

	info->load_window_index = 0;
	for (i = 0; i < NPU_SCHEDULER_LOAD_WIN_MAX; i++)
		info->load_window[i] = 0;
	info->load = 0;
	info->load_idle_time = 0;
	info->idle_load = 0;
	info->fps_load = 0;
	info->rq_load = 0;
	info->tfi = 0;

	/* de-activate scheduler */
	info->activated = 0;

#ifdef CONFIG_NPU_SCHEDULER_OPEN_CLOSE
	cancel_delayed_work_sync(&info->sched_work);
	npu_arbitration_cancel_work(info);
#endif
	npu_info("boost off (count %d)\n", info->boost_count);
	__npu_scheduler_boost_off(info);
	info->boost_count = 0;
	info->is_dvfs_cmd = false;

	npu_info("done\n");
	return ret;
}

void npu_scheduler_late_open(struct npu_device *device)
{
	if (NPU_OCP_FEATURE) {
		npu_scheduler_control_afm_grobal(&device->system, NPU_AFM_ENABLE);
		npu_scheduler_control_afm_interrupt(&device->system, NPU_AFM_ENABLE);
		npu_scheduler_control_afm_throttling(&device->system, NPU_AFM_ENABLE);
	}
}

void npu_scheduler_early_close(struct npu_device *device)
{
	if (NPU_OCP_FEATURE) {
		npu_scheduler_control_afm_grobal(&device->system, NPU_AFM_DISABLE);
		npu_scheduler_control_afm_interrupt(&device->system, NPU_AFM_DISABLE);
		npu_scheduler_control_afm_throttling(&device->system, NPU_AFM_DISABLE);
	}
}

int npu_scheduler_resume(struct npu_device *device)
{
	int ret = 0;
	struct npu_scheduler_info *info;

	BUG_ON(!device);
	info = device->sched;

	/* re-schedule work */
	if (info->activated) {
		cancel_delayed_work_sync(&info->sched_work);
#ifdef CONFIG_PM_SLEEP
		npu_wake_lock_timeout(info->sws, msecs_to_jiffies(100));
#endif
		queue_delayed_work(info->sched_wq, &info->sched_work,
				msecs_to_jiffies(100));

		npu_arbitration_cancel_work(info);
		npu_arbitration_queue_work(info);
	}

	npu_info("done\n");
	return ret;
}

int npu_scheduler_suspend(struct npu_device *device)
{
	int ret = 0;
	struct npu_scheduler_info *info;

	BUG_ON(!device);
	info = device->sched;

	if (info->activated) {
		cancel_delayed_work_sync(&info->sched_work);
		npu_arbitration_cancel_work(info);
	}

	npu_info("done\n");
	return ret;
}

int npu_scheduler_start(struct npu_device *device)
{
	int ret = 0;
	struct npu_scheduler_info *info;

	BUG_ON(!device);
	info = device->sched;

#ifdef CONFIG_NPU_SCHEDULER_START_STOP
#ifdef CONFIG_PM_SLEEP
	npu_wake_lock_timeout(info->sws, msecs_to_jiffies(100));
#endif
	queue_delayed_work(info->sched_wq, &info->sched_work,
				msecs_to_jiffies(100));

	npu_arbitration_cancel_work(info);
	npu_arbitration_queue_work(info);
#endif

	npu_info("done\n");
	return ret;
}

int npu_scheduler_stop(struct npu_device *device)
{
	int i, ret = 0;
	struct npu_scheduler_info *info;

	BUG_ON(!device);
	info = device->sched;

#ifdef CONFIG_NPU_SCHEDULER_START_STOP
	info->load_window_index = 0;
	for (i = 0; i < NPU_SCHEDULER_LOAD_WIN_MAX; i++)
		info->load_window[i] = 0;
	info->load = 0;
	info->load_idle_time = 0;
	info->idle_load = 0;
	info->fps_load = 0;
	info->rq_load = 0;
	info->tfi = 0;

	cancel_delayed_work_sync(&info->sched_work);
	npu_arbitration_cancel_work(info);
#endif
	npu_info("boost off (count %d)\n", info->boost_count);
	__npu_scheduler_boost_off(info);
	info->boost_count = 0;

	npu_info("done\n");
	return ret;
}

static u32 calc_next_mode(struct npu_scheduler_info *info, u32 req_mode, u32 prev_mode, npu_uid_t uid)
{
	int i;
	u32 ret;
	u32 sess_prev_mode = 0;
	int found = 0;
	struct npu_scheduler_fps_load *l;

	ret = NPU_PERF_MODE_NORMAL;

	mutex_lock(&info->fps_lock);
	list_for_each_entry(l, &info->fps_load_list, list) {
		if (l->uid == uid) {
			found = 1;
			/* read previous mode of a session and update next mode */
			sess_prev_mode = l->mode;
			l->mode = req_mode;
			break;
		}
	}
	mutex_unlock(&info->fps_lock);

	if (unlikely(!found))
		return ret;

	/* update reference count for each mode */
	if (req_mode == NPU_PERF_MODE_NORMAL) {
		if (info->mode_ref_cnt[sess_prev_mode] > 0)
			info->mode_ref_cnt[sess_prev_mode]--;
	} else {
		info->mode_ref_cnt[req_mode]++;
	}

	/* calculate next mode */
	for (i = 0; i < NPU_PERF_MODE_NUM; i++)
		if (info->mode_ref_cnt[i] > 0)
			ret = i;

	return ret;
}

npu_s_param_ret npu_scheduler_param_handler(struct npu_session *sess, struct vs4l_param *param, int *retval)
{
	int found = 0;
	struct npu_scheduler_fps_load *l;
	npu_s_param_ret ret = S_PARAM_HANDLED;
	u32 req_mode, prev_mode, next_mode;

	BUG_ON(!sess);
	BUG_ON(!param);

	mutex_lock(&g_npu_scheduler_info->fps_lock);
	list_for_each_entry(l, &g_npu_scheduler_info->fps_load_list, list) {
		if (l->uid == sess->uid) {
			found = 1;
			break;
		}
	}
	mutex_unlock(&g_npu_scheduler_info->fps_lock);
	if (!found) {
		npu_err("UID %d NOT found\n", sess->uid);
		ret = S_PARAM_NOMB;
		return ret;
	}

	switch (param->target) {
	case NPU_S_PARAM_PERF_MODE:
		if (param->offset < NPU_PERF_MODE_NUM) {
			req_mode = param->offset;
			prev_mode = g_npu_scheduler_info->mode;
			next_mode = calc_next_mode(g_npu_scheduler_info, req_mode, prev_mode, sess->uid);
			g_npu_scheduler_info->mode = next_mode;
			npu_dbg("req_mode:%u, prev_mode:%u, next_mode:%u\n",
					req_mode, prev_mode, next_mode);

			if ((g_npu_scheduler_info->activated) && (req_mode == next_mode)) {
				npu_scheduler_set_mode_freq(g_npu_scheduler_info, sess->uid);
				if (prev_mode != next_mode)
					npu_scheduler_set_bts(g_npu_scheduler_info);
				npu_set_llc(g_npu_scheduler_info);
				npu_scheduler_hwacg_onoff(g_npu_scheduler_info);
				npu_scheduler_send_mode_to_hw(sess, g_npu_scheduler_info);
				npu_dbg("new NPU performance mode : %s\n",
						npu_perf_mode_name[g_npu_scheduler_info->mode]);
			}
		} else {
			npu_err("Invalid NPU performance mode : %d\n",
					param->offset);
			ret = S_PARAM_NOMB;
		}
		break;

	case NPU_S_PARAM_PRIORITY:
		if (param->offset > NPU_SCHEDULER_PRIORITY_MAX) {
			npu_err("Invalid priority : %d\n", param->offset);
			ret = S_PARAM_NOMB;
		} else {
			mutex_lock(&g_npu_scheduler_info->fps_lock);
			l->priority = param->offset;

			/* TODO: hand over priority info to session */

			npu_dbg("set priority of uid %d as %d\n",
					l->uid, l->priority);
			mutex_unlock(&g_npu_scheduler_info->fps_lock);
		}
		break;

	case NPU_S_PARAM_TPF:
		if (param->offset == 0) {
			npu_err("No TPF setting : %d\n", param->offset);
			ret = S_PARAM_NOMB;
		} else {
			mutex_lock(&g_npu_scheduler_info->fps_lock);
			l->requested_tpf = param->offset;
			npu_dbg("set tpf of uid %d as %lld\n",
					l->uid, l->requested_tpf);
			mutex_unlock(&g_npu_scheduler_info->fps_lock);
		}
		break;

	default:
		ret = S_PARAM_NOMB;
		break;
	}

	return ret;
}
