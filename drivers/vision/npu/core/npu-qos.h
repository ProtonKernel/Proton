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

#ifndef _NPU_QOS_H_
#define _NPU_QOS_H_
#include <linux/version.h>
#if LINUX_VERSION_CODE >= KERNEL_VERSION(5, 4, 0)
#include <soc/samsung/exynos_pm_qos.h>
#include <soc/samsung/freq-qos-tracer.h>
#else
#include <linux/pm_qos.h>
#endif
#include <linux/mutex.h>
#include <linux/list.h>
#include <linux/notifier.h>
#include <soc/samsung/bts.h>
#define NPU_QOS_DEFAULT_VALUE   (INT_MAX)

struct npu_qos_freq_lock {
	u32	npu_freq_maxlock;
	//u32	dnc_freq_maxlock;
};

struct npu_qos_setting {
	struct mutex		npu_qos_lock;
	struct platform_device	*dvfs_npu_dev;

#if LINUX_VERSION_CODE >= KERNEL_VERSION(5, 4, 0)
	//struct exynos_pm_qos_request	npu_qos_req_dnc;
	struct exynos_pm_qos_request	npu_qos_req_npu;
	struct exynos_pm_qos_request	npu_qos_req_dsp;
	struct exynos_pm_qos_request	npu_qos_req_mif;
	struct exynos_pm_qos_request	npu_qos_req_int;
	//struct exynos_pm_qos_request	npu_qos_req_dnc_max;
	struct exynos_pm_qos_request	npu_qos_req_npu_max;
	struct exynos_pm_qos_request	npu_qos_req_dsp_max;
	struct exynos_pm_qos_request	npu_qos_req_mif_max;
	struct exynos_pm_qos_request	npu_qos_req_int_max;
	struct freq_qos_request			npu_qos_req_cpu_cl0;
	struct freq_qos_request			npu_qos_req_cpu_cl1;
	struct freq_qos_request			npu_qos_req_cpu_cl2;
	struct cpufreq_policy			*cl0_policy;
	struct cpufreq_policy			*cl1_policy;
	struct cpufreq_policy			*cl2_policy;
#else
	struct pm_qos_request	npu_qos_req_dnc;
	struct pm_qos_request	npu_qos_req_npu;
	struct pm_qos_request	npu_qos_req_dsp;
	struct pm_qos_request	npu_qos_req_mif;
	struct pm_qos_request	npu_qos_req_int;
	struct pm_qos_request	npu_qos_req_dnc_max;
	struct pm_qos_request	npu_qos_req_npu_max;
	struct pm_qos_request	npu_qos_req_dsp_max;
	struct pm_qos_request	npu_qos_req_mif_max;
	struct pm_qos_request	npu_qos_req_int_max;
	struct pm_qos_request	npu_qos_req_cpu_cl0;
	struct pm_qos_request	npu_qos_req_cpu_cl1;
	struct pm_qos_request	npu_qos_req_cpu_cl2;
#endif
	s32		req_cl0_freq;
	s32		req_cl1_freq;
	s32		req_cl2_freq;
	s32		req_npu_freq;
	s32		req_dsp_freq;
	//s32		req_dnc_freq;
	s32		req_mif_freq;
	s32		req_int_freq;

	s32		req_mo_scen;
	u32		req_cpu_aff;

	u32		dsp_type;
	u32		dsp_max_freq;
	u32		npu_max_freq;

	struct notifier_block npu_qos_max_nb;
	struct npu_scheduler_info *info;
};

struct npu_session_qos_req {
	s32		sessionUID;
	s32		req_freq;
	s32		req_mo_scen;
	__u32		eCategory;
	struct list_head list;
};

struct npu_system;
int npu_qos_probe(struct npu_system *system);
int npu_qos_release(struct npu_system *system);
int npu_qos_open(struct npu_system *system);
int npu_qos_close(struct npu_system *system);
npu_s_param_ret npu_qos_param_handler(struct npu_session *sess,
	struct vs4l_param *param, int *retval);

#endif	/* _NPU_QOS_H_ */
