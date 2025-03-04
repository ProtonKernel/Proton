/*
 * Samsung Exynos SoC series NPU driver
 *
 * Copyright (c) 2017 Samsung Electronics Co., Ltd.
 *              http://www.samsung.com/
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 */

#include <linux/version.h>
#if LINUX_VERSION_CODE >= KERNEL_VERSION(5, 4, 0)
#include <soc/samsung/exynos_pm_qos.h>
#else
#include <linux/pm_qos.h>
#include <soc/samsung/exynos-devfreq.h>
#endif
#include <linux/pm_opp.h>
#include <linux/mutex.h>
#include <linux/of.h>
#include <linux/of_platform.h>

#include "npu-vs4l.h"
#include "npu-device.h"
#include "npu-memory.h"
#include "npu-system.h"
#include "npu-qos.h"

static struct npu_qos_setting *qos_setting;
static LIST_HEAD(qos_list);

static struct npu_qos_freq_lock qos_lock;

static s32 __update_freq_from_showcase(__u32 nCategory);
static ssize_t npu_show_attrs_qos_sysfs(struct device *dev,
		struct device_attribute *attr, char *buf);
static ssize_t npu_store_attrs_qos_sysfs(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t count);
static int npu_qos_sysfs_create(struct npu_system *system);

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

static void __npu_pm_qos_add_notifier(int exynos_pm_qos_class,
					struct notifier_block *notifier)
{
	exynos_pm_qos_add_notifier(exynos_pm_qos_class, notifier);
}
/*
static int __npu_pm_qos_read_req_value(int exynos_pm_qos_class, struct exynos_pm_qos_request *req)
{
	return exynos_pm_qos_read_req_value(exynos_pm_qos_class, req);
}
*/
int __req_param_qos(int uid, __u32 nCategory, struct exynos_pm_qos_request *req, s32 new_value)
{
	int ret = 0;
	s32 cur_value, rec_value;
	struct list_head *pos, *q;
	struct npu_session_qos_req *qr;

	//return 0;
	//Check that same uid, and category whether already registered.
	list_for_each_safe(pos, q, &qos_list) {
		qr = list_entry(pos, struct npu_session_qos_req, list);
		if ((qr->sessionUID == uid) && (qr->eCategory == nCategory)) {
			switch (nCategory) {
			case NPU_S_PARAM_QOS_MO_SCEN_PRESET:
				cur_value = qr->req_mo_scen;
				npu_dbg("[U%u]Change Req MO scen. category : %u, from mo scen : %d to %d\n",
						uid, nCategory, cur_value, new_value);
				list_del(pos);

				qr->sessionUID = uid;
				qr->req_mo_scen = new_value;
				qr->eCategory = nCategory;
				list_add_tail(&qr->list, &qos_list);
				bts_del_scenario(cur_value);
				bts_add_scenario(qr->req_mo_scen);
				return ret;
			default:
				cur_value = qr->req_freq;
				npu_dbg("[U%u]Change Req Freq. category : %u, from freq : %d to %d\n",
						uid, nCategory, cur_value, new_value);
				list_del(pos);
				qr->sessionUID = uid;
				qr->req_freq = new_value;
				qr->eCategory = nCategory;
				list_add_tail(&qr->list, &qos_list);

				rec_value = __update_freq_from_showcase(nCategory);

				if (new_value > rec_value) {
					exynos_pm_qos_update_request(req, new_value);
					npu_dbg("[U%u]Changed Freq. category : %u, from freq : %d to %d\n",
							uid, nCategory, cur_value, new_value);
				} else {
					exynos_pm_qos_update_request(req, rec_value);
					npu_dbg("[U%u]Recovered Freq. category : %u, from freq : %d to %d\n",
							uid, nCategory, cur_value, rec_value);
				}
				return ret;
			}
		}
	}

	//No Same uid, and category. Add new item
	qr = kmalloc(sizeof(struct npu_session_qos_req), GFP_KERNEL);
	if (!qr)
		return -ENOMEM;

	switch (nCategory) {
	case NPU_S_PARAM_QOS_MO_SCEN_PRESET:
		qr->sessionUID = uid;
		qr->req_mo_scen = new_value;
		qr->eCategory = nCategory;
		list_add_tail(&qr->list, &qos_list);
		bts_add_scenario(qr->req_mo_scen);
		return ret;
	default:
		qr->sessionUID = uid;
		qr->req_freq = new_value;
		qr->eCategory = nCategory;
		list_add_tail(&qr->list, &qos_list);

		//If new_value is lager than current value, update the freq
		cur_value = (s32)exynos_pm_qos_read_req_value(req->exynos_pm_qos_class, req);
		npu_dbg("[U%u]New Freq. category : %u freq : %u\n",
				qr->sessionUID, qr->eCategory, qr->req_freq);
		if (cur_value < new_value) {
			npu_dbg("[U%u]Update Freq. category : %u freq : %u\n",
					qr->sessionUID, qr->eCategory, qr->req_freq);
			exynos_pm_qos_update_request(req, new_value);
		}
		return ret;
	}
}

static int npu_qos_freq_qos_add_request(void)
{
	if (!qos_setting)
		return -EINVAL;

	if (!qos_setting->cl0_policy) {
		qos_setting->cl0_policy = cpufreq_cpu_get(0);
		if (qos_setting->cl0_policy)
			freq_qos_tracer_add_request(&qos_setting->cl0_policy->constraints,
					&qos_setting->npu_qos_req_cpu_cl0, FREQ_QOS_MIN, 0);
	}

	if (!qos_setting->cl1_policy) {
		qos_setting->cl1_policy = cpufreq_cpu_get(4);
		if (qos_setting->cl1_policy)
			freq_qos_tracer_add_request(&qos_setting->cl1_policy->constraints,
					&qos_setting->npu_qos_req_cpu_cl1, FREQ_QOS_MIN, 0);
	}

	if (!qos_setting->cl2_policy) {
		qos_setting->cl2_policy = cpufreq_cpu_get(7);
		if (qos_setting->cl2_policy)
			freq_qos_tracer_add_request(&qos_setting->cl2_policy->constraints,
					&qos_setting->npu_qos_req_cpu_cl2, FREQ_QOS_MIN, 0);
	}

	return 0;
}

int __req_param_qos_cpu(int uid, __u32 nCategory, struct freq_qos_request *req, s32 new_value)
{
	int ret = 0;
	s32 cur_value, rec_value;
	struct list_head *pos, *q;
	struct npu_session_qos_req *qr;

	npu_qos_freq_qos_add_request();
	//return 0;
	//Check that same uid, and category whether already registered.
	list_for_each_safe(pos, q, &qos_list) {
		qr = list_entry(pos, struct npu_session_qos_req, list);
		if ((qr->sessionUID == uid) && (qr->eCategory == nCategory)) {
			cur_value = qr->req_freq;
			npu_dbg("[U%u]Change Req Freq. category : %u, from freq : %d to %d\n",
					uid, nCategory, cur_value, new_value);
			list_del(pos);
			qr->sessionUID = uid;
			qr->req_freq = new_value;
			qr->eCategory = nCategory;
			list_add_tail(&qr->list, &qos_list);
			rec_value = __update_freq_from_showcase(nCategory);

			if (new_value > rec_value) {
				freq_qos_update_request(req, new_value);
				npu_dbg("[U%u]Changed Freq. category : %u, from freq : %d to %d\n",
						uid, nCategory, cur_value, new_value);
			} else {
				freq_qos_update_request(req, rec_value);
				npu_dbg("[U%u]Recovered Freq. category : %u, from freq : %d to %d\n",
						uid, nCategory, cur_value, rec_value);
			}
			return ret;
		}
	}
	//No Same uid, and category. Add new item
	qr = kmalloc(sizeof(struct npu_session_qos_req), GFP_KERNEL);
	if (!qr)
		return -ENOMEM;

	qr->sessionUID = uid;
	qr->req_freq = new_value;
	qr->eCategory = nCategory;
	list_add_tail(&qr->list, &qos_list);

	npu_dbg("[U%u]Update Freq. category : %u freq : %u\n",
			qr->sessionUID, qr->eCategory, qr->req_freq);
	freq_qos_update_request(req, new_value);
	return ret;
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

static void __npu_pm_qos_add_notifier(int pm_qos_class,
					struct notifier_block *notifier)
{
	pm_qos_add_notifier(pm_qos_class, notifier);
}
/*
static int __npu_pm_qos_read_req_value(int pm_qos_class, struct pm_qos_request *req)
{
	return pm_qos_read_req_value(pm_qos_class, req);
}
*/
int __req_param_qos(int uid, __u32 nCategory, struct pm_qos_request *req, s32 new_value)
{
	int ret = 0;
	s32 cur_value, rec_value;
	struct list_head *pos, *q;
	struct npu_session_qos_req *qr;

	//return 0;
	//Check that same uid, and category whether already registered.
	list_for_each_safe(pos, q, &qos_list) {
		qr = list_entry(pos, struct npu_session_qos_req, list);
		if ((qr->sessionUID == uid) && (qr->eCategory == nCategory)) {
			switch (nCategory) {
			case NPU_S_PARAM_QOS_MO_SCEN_PRESET:
				cur_value = qr->req_mo_scen;
				npu_dbg("[U%u]Change Req MO scen. category : %u, from mo scen : %d to %d\n",
						uid, nCategory, cur_value, new_value);
				list_del(pos);

				qr->sessionUID = uid;
				qr->req_mo_scen = new_value;
				qr->eCategory = nCategory;
				list_add_tail(&qr->list, &qos_list);
				bts_del_scenario(cur_value);
				bts_add_scenario(qr->req_mo_scen);
				return ret;
			default:
				cur_value = qr->req_freq;
				npu_dbg("[U%u]Change Req Freq. category : %u, from freq : %d to %d\n",
						uid, nCategory, cur_value, new_value);
				list_del(pos);
				qr->sessionUID = uid;
				qr->req_freq = new_value;
				qr->eCategory = nCategory;
				list_add_tail(&qr->list, &qos_list);

				rec_value = __update_freq_from_showcase(nCategory);

				if (new_value > rec_value) {
					pm_qos_update_request(req, new_value);
					npu_dbg("[U%u]Changed Freq. category : %u, from freq : %d to %d\n",
							uid, nCategory, cur_value, new_value);
				} else {
					pm_qos_update_request(req, rec_value);
					npu_dbg("[U%u]Recovered Freq. category : %u, from freq : %d to %d\n",
							uid, nCategory, cur_value, rec_value);
				}
				return ret;
			}
		}
	}

	//No Same uid, and category. Add new item
	qr = kmalloc(sizeof(struct npu_session_qos_req), GFP_KERNEL);
	if (!qr)
		return -ENOMEM;

	switch (nCategory) {
	case NPU_S_PARAM_QOS_MO_SCEN_PRESET:
		qr->sessionUID = uid;
		qr->req_mo_scen = new_value;
		qr->eCategory = nCategory;
		list_add_tail(&qr->list, &qos_list);
		bts_add_scenario(qr->req_mo_scen);
		return ret;
	default:
		qr->sessionUID = uid;
		qr->req_freq = new_value;
		qr->eCategory = nCategory;
		list_add_tail(&qr->list, &qos_list);

		//If new_value is lager than current value, update the freq
		cur_value = (s32)pm_qos_read_req_value(req->pm_qos_class, req);
		npu_dbg("[U%u]New Freq. category : %u freq : %u\n",
				qr->sessionUID, qr->eCategory, qr->req_freq);
		if (cur_value < new_value) {
			npu_dbg("[U%u]Update Freq. category : %u freq : %u\n",
					qr->sessionUID, qr->eCategory, qr->req_freq);
			pm_qos_update_request(req, new_value);
		}
		return ret;
	}
}
#endif

static int npu_qos_max_notifier(struct notifier_block *nb,
		unsigned long action, void *nb_data)
{
	/* activate/deactivate peripheral DVFS */
	npu_scheduler_activate_peripheral_dvfs(action);

	/* reset all peripheral DVFS minlock */
	__npu_pm_qos_update_request(&qos_setting->npu_qos_req_mif, 0);
	__npu_pm_qos_update_request(&qos_setting->npu_qos_req_int, 0);
#if LINUX_VERSION_CODE >= KERNEL_VERSION(5, 4, 0)
	npu_qos_freq_qos_add_request();
	freq_qos_update_request(&qos_setting->npu_qos_req_cpu_cl0, 0);
	freq_qos_update_request(&qos_setting->npu_qos_req_cpu_cl1, 0);
	freq_qos_update_request(&qos_setting->npu_qos_req_cpu_cl2, 0);
#else
	__npu_pm_qos_update_request(&qos_setting->npu_qos_req_cpu_cl0, 0);
	__npu_pm_qos_update_request(&qos_setting->npu_qos_req_cpu_cl1, 0);
	__npu_pm_qos_update_request(&qos_setting->npu_qos_req_cpu_cl2, 0);
#endif

	qos_setting->req_mif_freq = 0;
	qos_setting->req_int_freq = 0;
	qos_setting->req_cl0_freq = 0;
	qos_setting->req_cl1_freq = 0;
	qos_setting->req_cl2_freq = 0;

	return NOTIFY_DONE;
}

int npu_qos_probe(struct npu_system *system)
{
	qos_setting = &(system->qos_setting);

	if (qos_setting->info == NULL) {
		probe_err("info is NULL!\n");
		return 0;
	}

	mutex_init(&qos_setting->npu_qos_lock);

	/* qos add request(default_freq) */
	__npu_pm_qos_add_request(&qos_setting->npu_qos_req_npu,
			PM_QOS_NPU_THROUGHPUT, 0);
	__npu_pm_qos_add_request(&qos_setting->npu_qos_req_dsp,
			PM_QOS_VPC_THROUGHPUT, 0);
	__npu_pm_qos_add_request(&qos_setting->npu_qos_req_mif,
			PM_QOS_BUS_THROUGHPUT, 0);
	__npu_pm_qos_add_request(&qos_setting->npu_qos_req_int,
			PM_QOS_DEVICE_THROUGHPUT, 0);

	/* qos add request(max_freq) */
	__npu_pm_qos_add_request(&qos_setting->npu_qos_req_npu_max,
			PM_QOS_NPU_THROUGHPUT_MAX,
			PM_QOS_NPU_THROUGHPUT_MAX_DEFAULT_VALUE);
	__npu_pm_qos_add_request(&qos_setting->npu_qos_req_dsp_max,
			PM_QOS_VPC_THROUGHPUT_MAX,
			PM_QOS_VPC_THROUGHPUT_MAX_DEFAULT_VALUE);
	__npu_pm_qos_add_request(&qos_setting->npu_qos_req_mif_max,
			PM_QOS_BUS_THROUGHPUT_MAX,
			PM_QOS_BUS_THROUGHPUT_MAX_DEFAULT_VALUE);
	__npu_pm_qos_add_request(&qos_setting->npu_qos_req_int_max,
			PM_QOS_DEVICE_THROUGHPUT_MAX,
			PM_QOS_DEVICE_THROUGHPUT_MAX_DEFAULT_VALUE);

#if LINUX_VERSION_CODE >= KERNEL_VERSION(5, 4, 0)
	npu_qos_freq_qos_add_request();
#else
	__npu_pm_qos_add_request(&qos_setting->npu_qos_req_cpu_cl0, PM_QOS_CLUSTER0_FREQ_MIN, 0);
	__npu_pm_qos_add_request(&qos_setting->npu_qos_req_cpu_cl1, PM_QOS_CLUSTER1_FREQ_MIN, 0);
	__npu_pm_qos_add_request(&qos_setting->npu_qos_req_cpu_cl2, PM_QOS_CLUSTER2_FREQ_MIN, 0);
#endif

	qos_setting->req_npu_freq = 0;
	qos_setting->req_dsp_freq = 0;
	qos_setting->req_int_freq = 0;
	qos_setting->req_mif_freq = 0;
	qos_setting->req_cl0_freq = 0;
	qos_setting->req_cl1_freq = 0;
	qos_setting->req_cl2_freq = 0;
	qos_setting->req_mo_scen = 0;
	qos_setting->req_cpu_aff = 0;

	qos_lock.npu_freq_maxlock = PM_QOS_NPU_THROUGHPUT_MAX_DEFAULT_VALUE;

	qos_setting->npu_qos_max_nb.notifier_call = npu_qos_max_notifier;
	__npu_pm_qos_add_notifier(PM_QOS_NPU_THROUGHPUT_MAX, &qos_setting->npu_qos_max_nb);

	if (npu_qos_sysfs_create(system)) {
		probe_info("npu_qos_sysfs create failed\n");
		return -1;
	}

	probe_info("done\n");
	return 0;
}

int npu_qos_release(struct npu_system *system)
{
	return 0;
}

int npu_qos_open(struct npu_system *system)
{
	//return 0;
	BUG_ON(!system);

	mutex_lock(&qos_setting->npu_qos_lock);

	qos_setting->req_npu_freq = 0;
	qos_setting->req_int_freq = 0;
	qos_setting->req_mif_freq = 0;
	qos_setting->req_cl0_freq = 0;
	qos_setting->req_cl1_freq = 0;
	qos_setting->req_cl2_freq = 0;

	mutex_unlock(&qos_setting->npu_qos_lock);

	npu_info("done\n");
	return 0;
}

int npu_qos_close(struct npu_system *system)
{
	struct list_head *pos, *q;
	struct npu_session_qos_req *qr;

	//return 0;
	BUG_ON(!system);

	mutex_lock(&qos_setting->npu_qos_lock);

	list_for_each_safe(pos, q, &qos_list) {
		qr = list_entry(pos, struct npu_session_qos_req, list);
		switch (qr->eCategory) {
		case NPU_S_PARAM_QOS_MO_SCEN_PRESET:
			bts_del_scenario(qr->req_mo_scen);
			break;
		default:
			break;
		}

		list_del(pos);
		if (qr)
			kfree(qr);
	}
	list_del_init(&qos_list);

	__npu_pm_qos_update_request(&qos_setting->npu_qos_req_npu, 0);
	__npu_pm_qos_update_request(&qos_setting->npu_qos_req_dsp, 0);
	__npu_pm_qos_update_request(&qos_setting->npu_qos_req_mif, 0);
	__npu_pm_qos_update_request(&qos_setting->npu_qos_req_int, 0);

	__npu_pm_qos_update_request(&qos_setting->npu_qos_req_npu_max,
	  		PM_QOS_NPU_THROUGHPUT_MAX_DEFAULT_VALUE);
	__npu_pm_qos_update_request(&qos_setting->npu_qos_req_dsp_max,
			PM_QOS_VPC_THROUGHPUT_MAX_DEFAULT_VALUE);
	__npu_pm_qos_update_request(&qos_setting->npu_qos_req_mif_max,
				PM_QOS_BUS_THROUGHPUT_MAX_DEFAULT_VALUE);
	__npu_pm_qos_update_request(&qos_setting->npu_qos_req_int_max,
				PM_QOS_DEVICE_THROUGHPUT_MAX_DEFAULT_VALUE);

#if LINUX_VERSION_CODE >= KERNEL_VERSION(5, 4, 0)
	npu_qos_freq_qos_add_request();
	freq_qos_update_request(&qos_setting->npu_qos_req_cpu_cl0, 0);
	freq_qos_update_request(&qos_setting->npu_qos_req_cpu_cl1, 0);
	freq_qos_update_request(&qos_setting->npu_qos_req_cpu_cl2, 0);
#else
	__npu_pm_qos_update_request(&qos_setting->npu_qos_req_cpu_cl0, 0);
	__npu_pm_qos_update_request(&qos_setting->npu_qos_req_cpu_cl1, 0);
	__npu_pm_qos_update_request(&qos_setting->npu_qos_req_cpu_cl2, 0);
#endif

	qos_setting->req_npu_freq = 0;
	qos_setting->req_int_freq = 0;
	qos_setting->req_mif_freq = 0;
	qos_setting->req_cl0_freq = 0;
	qos_setting->req_cl1_freq = 0;
	qos_setting->req_cl2_freq = 0;

	qos_lock.npu_freq_maxlock = PM_QOS_NPU_THROUGHPUT_MAX_DEFAULT_VALUE;

	mutex_unlock(&qos_setting->npu_qos_lock);

	npu_info("done\n");
	return 0;
}

static s32 __update_freq_from_showcase(__u32 nCategory)
{
	s32 nValue = 0;
	struct list_head *pos, *q;
	struct npu_session_qos_req *qr;

	qr = NULL;
	list_for_each_safe(pos, q, &qos_list) {
		qr = list_entry(pos, struct npu_session_qos_req, list);
		if (qr->eCategory == nCategory) {
			nValue = qr->req_freq > nValue ? qr->req_freq : nValue;
			npu_dbg("[U%u]Candidate Freq. category : %u  freq : %d\n",
				qr->sessionUID, nCategory, nValue);
		}
	}

	return nValue;
}

static s32 __is_preset_from_showcase(void)
{
	struct list_head *pos, *q;
	struct npu_session_qos_req *qr;

	list_for_each_safe(pos, q, &qos_list) {
		qr = list_entry(pos, struct npu_session_qos_req, list);
		switch (qr->eCategory) {
		case NPU_S_PARAM_QOS_NPU_PRESET:
		case NPU_S_PARAM_QOS_DSP_PRESET:
		case NPU_S_PARAM_QOS_MIF_PRESET:
		case NPU_S_PARAM_QOS_INT_PRESET:
		case NPU_S_PARAM_QOS_CL0_PRESET:
		case NPU_S_PARAM_QOS_CL1_PRESET:
		case NPU_S_PARAM_QOS_CL2_PRESET:
		case NPU_S_PARAM_QOS_MO_SCEN_PRESET:
		case NPU_S_PARAM_QOS_CPU_AFF_PRESET:
			if (qr->req_freq > 0)
				return 1;
			break;
		default:
			break;
		}
	}

	return 0;
}

static bool npu_qos_preset_is_valid_value(int value)
{
	if (value == NPU_QOS_DEFAULT_VALUE)
		return true;

	if (value >= 0)
		return true;

	return false;
}

static s32 npu_qos_preset_get_req_value(int value)
{
	if (value == NPU_QOS_DEFAULT_VALUE)
		return 0;
	else
		return value;
}

npu_s_param_ret npu_qos_param_handler(struct npu_session *sess, struct vs4l_param *param, int *retval)
{
	BUG_ON(!sess);
	BUG_ON(!param);

	npu_info("uid:%u category:%u offset:%u\n", sess->uid, param->target, param->offset);

	mutex_lock(&qos_setting->npu_qos_lock);

	switch (param->target) {
	case NPU_S_PARAM_QOS_NPU:
		qos_setting->req_npu_freq = param->offset;
		__req_param_qos(sess->uid, param->target, &qos_setting->npu_qos_req_npu,
				qos_setting->req_npu_freq);
		goto ok_exit;

	case NPU_S_PARAM_QOS_MIF:
		qos_setting->req_mif_freq = param->offset;
		__req_param_qos(sess->uid, param->target, &qos_setting->npu_qos_req_mif,
				qos_setting->req_mif_freq);
		goto ok_exit;

	case NPU_S_PARAM_QOS_INT:
		qos_setting->req_int_freq = param->offset;
		__req_param_qos(sess->uid, param->target, &qos_setting->npu_qos_req_int,
				qos_setting->req_int_freq);
		goto ok_exit;

	case NPU_S_PARAM_QOS_NPU_MAX:
		qos_setting->req_npu_freq = param->offset;
		__req_param_qos(sess->uid, param->target, &qos_setting->npu_qos_req_npu_max,
				qos_setting->req_npu_freq);
		goto ok_exit;

	case NPU_S_PARAM_QOS_MIF_MAX:
		qos_setting->req_mif_freq = param->offset;
		__req_param_qos(sess->uid, param->target, &qos_setting->npu_qos_req_mif_max,
				qos_setting->req_mif_freq);
		goto ok_exit;

	case NPU_S_PARAM_QOS_INT_MAX:
		qos_setting->req_int_freq = param->offset;
		__req_param_qos(sess->uid, param->target, &qos_setting->npu_qos_req_int_max,
				qos_setting->req_int_freq);
		goto ok_exit;

	case NPU_S_PARAM_QOS_CL0:
		qos_setting->req_cl0_freq = param->offset;
#if LINUX_VERSION_CODE >= KERNEL_VERSION(5, 4, 0)
		__req_param_qos_cpu(sess->uid, param->target, &qos_setting->npu_qos_req_cpu_cl0,
				qos_setting->req_cl0_freq);
#else
		__req_param_qos(sess->uid, param->target, &qos_setting->npu_qos_req_cpu_cl0,
				qos_setting->req_cl0_freq);
#endif
		goto ok_exit;

	case NPU_S_PARAM_QOS_CL1:
		qos_setting->req_cl1_freq = param->offset;
#if LINUX_VERSION_CODE >= KERNEL_VERSION(5, 4, 0)
		__req_param_qos_cpu(sess->uid, param->target, &qos_setting->npu_qos_req_cpu_cl1,
				qos_setting->req_cl1_freq);
#else
		__req_param_qos(sess->uid, param->target, &qos_setting->npu_qos_req_cpu_cl1,
				qos_setting->req_cl1_freq);
#endif
		goto ok_exit;

	case NPU_S_PARAM_QOS_CL2:
		qos_setting->req_cl2_freq = param->offset;
#if LINUX_VERSION_CODE >= KERNEL_VERSION(5, 4, 0)
		__req_param_qos_cpu(sess->uid, param->target, &qos_setting->npu_qos_req_cpu_cl2,
				qos_setting->req_cl2_freq);
#else
		__req_param_qos(sess->uid, param->target, &qos_setting->npu_qos_req_cpu_cl2,
				qos_setting->req_cl2_freq);
#endif
		goto ok_exit;

	case NPU_S_PARAM_QOS_NPU_PRESET:
		if (!npu_qos_preset_is_valid_value(param->offset))
			goto ok_preset_exit;

		qos_setting->req_npu_freq =
			npu_qos_preset_get_req_value(param->offset);
		__req_param_qos(sess->uid, param->target, &qos_setting->npu_qos_req_npu,
				qos_setting->req_npu_freq);
		goto ok_preset_exit;

	case NPU_S_PARAM_QOS_DSP_PRESET:
		if (!npu_qos_preset_is_valid_value(param->offset))
			goto ok_preset_exit;

		qos_setting->req_dsp_freq =
			npu_qos_preset_get_req_value(param->offset);
		__req_param_qos(sess->uid, param->target, &qos_setting->npu_qos_req_dsp,
				qos_setting->req_dsp_freq);
		goto ok_preset_exit;

	case NPU_S_PARAM_QOS_MIF_PRESET:
		if (!npu_qos_preset_is_valid_value(param->offset))
			goto ok_preset_exit;

		qos_setting->req_mif_freq =
			npu_qos_preset_get_req_value(param->offset);
		__req_param_qos(sess->uid, param->target, &qos_setting->npu_qos_req_mif,
				qos_setting->req_mif_freq);
		goto ok_preset_exit;

	case NPU_S_PARAM_QOS_INT_PRESET:
		if (!npu_qos_preset_is_valid_value(param->offset))
			goto ok_preset_exit;

		qos_setting->req_int_freq =
			npu_qos_preset_get_req_value(param->offset);
		__req_param_qos(sess->uid, param->target,
				&qos_setting->npu_qos_req_int,
				qos_setting->req_int_freq);
		goto ok_preset_exit;

	case NPU_S_PARAM_QOS_CL0_PRESET:
		if (!npu_qos_preset_is_valid_value(param->offset))
			goto ok_preset_exit;

		qos_setting->req_cl0_freq = npu_qos_preset_get_req_value(param->offset);
#if LINUX_VERSION_CODE >= KERNEL_VERSION(5, 4, 0)
		__req_param_qos_cpu(sess->uid, param->target, &qos_setting->npu_qos_req_cpu_cl0,
				qos_setting->req_cl0_freq);
#else
		__req_param_qos(sess->uid, param->target, &qos_setting->npu_qos_req_cpu_cl0,
				qos_setting->req_cl0_freq);
#endif
		goto ok_preset_exit;

	case NPU_S_PARAM_QOS_CL1_PRESET:
		if (!npu_qos_preset_is_valid_value(param->offset))
			goto ok_preset_exit;

		qos_setting->req_cl1_freq = npu_qos_preset_get_req_value(param->offset);
#if LINUX_VERSION_CODE >= KERNEL_VERSION(5, 4, 0)
		__req_param_qos_cpu(sess->uid, param->target, &qos_setting->npu_qos_req_cpu_cl1,
				qos_setting->req_cl1_freq);
#else
		__req_param_qos(sess->uid, param->target, &qos_setting->npu_qos_req_cpu_cl1,
				qos_setting->req_cl1_freq);
#endif
		goto ok_preset_exit;

	case NPU_S_PARAM_QOS_CL2_PRESET:
		if (!npu_qos_preset_is_valid_value(param->offset))
			goto ok_preset_exit;

		qos_setting->req_cl2_freq = npu_qos_preset_get_req_value(param->offset);
#if LINUX_VERSION_CODE >= KERNEL_VERSION(5, 4, 0)
		__req_param_qos_cpu(sess->uid, param->target, &qos_setting->npu_qos_req_cpu_cl2,
				qos_setting->req_cl2_freq);
#else
		__req_param_qos(sess->uid, param->target, &qos_setting->npu_qos_req_cpu_cl2,
				qos_setting->req_cl2_freq);
#endif
		goto ok_preset_exit;

	case NPU_S_PARAM_QOS_MO_SCEN_PRESET:
		if (!npu_qos_preset_is_valid_value(param->offset))
			goto ok_preset_exit;

		qos_setting->req_mo_scen = npu_qos_preset_get_req_value(param->offset);
		__req_param_qos(sess->uid, param->target, NULL, qos_setting->req_mo_scen);
		goto ok_preset_exit;

	case NPU_S_PARAM_QOS_CPU_AFF_PRESET:
		if (!npu_qos_preset_is_valid_value(param->offset))
			goto ok_preset_exit;

		qos_setting->req_cpu_aff = param->offset;
		/* To be implemented */
		goto ok_preset_exit;

	case NPU_S_PARAM_CPU_AFF:
	case NPU_S_PARAM_QOS_RST:
	default:
		mutex_unlock(&qos_setting->npu_qos_lock);
		return S_PARAM_NOMB;
	}

ok_preset_exit:
	if (__is_preset_from_showcase())
		npu_scheduler_disable(qos_setting->info);
	else
		npu_scheduler_enable(qos_setting->info);
ok_exit:
	mutex_unlock(&qos_setting->npu_qos_lock);
	return S_PARAM_HANDLED;
}

static struct device_attribute npu_qos_sysfs_attr[] = {
	__ATTR(npu_freq_maxlock, 0664,
		npu_show_attrs_qos_sysfs,
		npu_store_attrs_qos_sysfs),
};

static struct attribute *npu_qos_sysfs_entries[] = {
	&npu_qos_sysfs_attr[0].attr,
	NULL,
};

static struct attribute_group npu_qos_attr_group = {
	.name = "qos_freq",
	.attrs = npu_qos_sysfs_entries,
};
enum {
	NPU_QOS_NPU_FREQ_MAXLOCK = 0,
	NPU_QOS_MIF_FREQ_ATTR_NUM,
};

static ssize_t npu_show_attrs_qos_sysfs(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	int i = 0;
	const ptrdiff_t offset = attr - npu_qos_sysfs_attr;

	switch (offset) {
	case NPU_QOS_NPU_FREQ_MAXLOCK:
		i += scnprintf(buf + i, PAGE_SIZE - i, "%u\n",
				qos_lock.npu_freq_maxlock);
		break;

	default:
		break;
	}

	return i;
}

static ssize_t npu_store_attrs_qos_sysfs(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t count)
{
	int ret = 0, value = 0;
	const ptrdiff_t offset = attr - npu_qos_sysfs_attr;

	ret = sscanf(buf, "%d", &value);
	if (ret > 0) {
		switch (offset) {
		case NPU_QOS_NPU_FREQ_MAXLOCK:
			qos_lock.npu_freq_maxlock = (u32)value;
			__npu_pm_qos_update_request(&qos_setting->npu_qos_req_npu_max,
									value);
			ret = count;
			break;
		default:
			break;
		}
	}

	return ret;
}

static int npu_qos_sysfs_create(struct npu_system *system)
{
	int ret = 0;
	struct npu_device *device;

	BUG_ON(!system);

	device = container_of(system, struct npu_device, system);

	probe_info("npu qos-sysfs create\n");
	probe_info("creating sysfs group %s\n", npu_qos_attr_group.name);

	ret = sysfs_create_group(&device->dev->kobj, &npu_qos_attr_group);
	if (ret) {
		probe_err("failed to create sysfs for %s\n",
						npu_qos_attr_group.name);
	}

	return ret;
}
