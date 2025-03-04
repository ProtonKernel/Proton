/* drivers/gpu/arm/.../platform/gpu_pmqos.c
 *
 * Copyright 2011 by S.LSI. Samsung Electronics Inc.
 * San#24, Nongseo-Dong, Giheung-Gu, Yongin, Korea
 *
 * Samsung SoC Mali-T Series DVFS driver
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software FoundatIon.
 */

/**
 * @file gpu_pmqos.c
 * DVFS
 */

#include <mali_kbase.h>

#include <soc/samsung/exynos_pm_qos.h>
#include <soc/samsung/freq-qos-tracer.h>

#include <linux/cpufreq.h>

#include "mali_kbase_platform.h"
#include "gpu_dvfs_handler.h"



#if defined(PM_QOS_CLUSTER2_FREQ_MAX_DEFAULT_VALUE)
#define PM_QOS_CPU_CLUSTER_NUM 3
#else
#define PM_QOS_CPU_CLUSTER_NUM 2
#ifndef PM_QOS_CLUSTER1_FREQ_MAX_DEFAULT_VALUE
#define PM_QOS_CLUSTER1_FREQ_MAX_DEFAULT_VALUE INT_MAX
#endif
#endif

#if defined(CONFIG_SCHED_EMS)
#include <linux/ems.h>
#if defined(CONFIG_SCHED_EMS_TUNE)
#else
static struct gb_qos_request gb_req = {
	.name = "ems_boost",
};
#endif
#endif

struct exynos_pm_qos_request exynos5_g3d_mif_min_qos;
struct exynos_pm_qos_request exynos5_g3d_mif_max_qos;
struct exynos_pm_qos_request exynos5_g3d_cpu_cluster0_min_qos;
struct exynos_pm_qos_request exynos5_g3d_cpu_cluster1_max_qos;
struct exynos_pm_qos_request exynos5_g3d_cpu_cluster1_min_qos;
#if PM_QOS_CPU_CLUSTER_NUM == 3
struct exynos_pm_qos_request exynos5_g3d_cpu_cluster2_max_qos;
struct exynos_pm_qos_request exynos5_g3d_cpu_cluster2_min_qos;
#endif

struct freq_qos_request g3d_req_cpu_cluster0_min_qos;
struct freq_qos_request g3d_req_cpu_cluster1_min_qos;
struct freq_qos_request g3d_req_cpu_cluster1_max_qos;
struct freq_qos_request g3d_req_cpu_cluster2_max_qos;

struct cpufreq_policy* cpu_cluster0_policy;
struct cpufreq_policy* cpu_cluster1_policy;
struct cpufreq_policy* cpu_cluster2_policy;

extern struct kbase_device *pkbdev;

#ifdef CONFIG_MALI_PM_QOS
static int checked_freq_qos_update_request(struct freq_qos_request *req, s32 new_value)
{
	bool cpu_available =
		(req == &g3d_req_cpu_cluster0_min_qos && cpu_online(0)) ||
		(req == &g3d_req_cpu_cluster1_min_qos && cpu_online(4)) ||
		(req == &g3d_req_cpu_cluster1_max_qos && cpu_online(4)) ||
		(req == &g3d_req_cpu_cluster2_max_qos && cpu_online(7));

	if (req && cpu_available)
		return freq_qos_update_request(req, new_value);

	return 0;
}

int gpu_pm_qos_command(struct exynos_context *platform, gpu_pmqos_state state)
{
	int idx;

	DVFS_ASSERT(platform);

#ifdef CONFIG_MALI_ASV_CALIBRATION_SUPPORT
	if (platform->gpu_auto_cali_status)
		return 0;
#endif

	switch (state) {
	case GPU_CONTROL_PM_QOS_INIT:
		exynos_pm_qos_add_request(&exynos5_g3d_mif_min_qos, PM_QOS_BUS_THROUGHPUT, 0);
		if (platform->pmqos_mif_max_clock)
			exynos_pm_qos_add_request(&exynos5_g3d_mif_max_qos, PM_QOS_BUS_THROUGHPUT_MAX, PM_QOS_BUS_THROUGHPUT_MAX_DEFAULT_VALUE);

		//get cpu cluster policy info
		cpu_cluster0_policy = cpufreq_cpu_get(0);
		cpu_cluster1_policy = cpufreq_cpu_get(4);
		cpu_cluster2_policy = cpufreq_cpu_get(7);

		if (cpu_cluster0_policy)
			freq_qos_tracer_add_request(&cpu_cluster0_policy->constraints, &g3d_req_cpu_cluster0_min_qos, FREQ_QOS_MIN, 0);

		if (cpu_cluster1_policy)
			freq_qos_tracer_add_request(&cpu_cluster1_policy->constraints, &g3d_req_cpu_cluster1_max_qos, FREQ_QOS_MAX, PM_QOS_CLUSTER1_FREQ_MAX_DEFAULT_VALUE);

#if PM_QOS_CPU_CLUSTER_NUM == 2
		if (platform->boost_egl_min_lock && cpu_cluster1_policy)
			freq_qos_tracer_add_request(&cpu_cluster1_policy->constraints, &g3d_req_cpu_cluster1_min_qos, FREQ_QOS_MIN, 0);
#endif
#if PM_QOS_CPU_CLUSTER_NUM == 3
		if (cpu_cluster1_policy)
			freq_qos_tracer_add_request(&cpu_cluster1_policy->constraints, &g3d_req_cpu_cluster1_min_qos, FREQ_QOS_MIN, 0);

		if (cpu_cluster2_policy)
			freq_qos_tracer_add_request(&cpu_cluster2_policy->constraints, &g3d_req_cpu_cluster2_max_qos, FREQ_QOS_MAX, PM_QOS_CLUSTER2_FREQ_MAX_DEFAULT_VALUE);

		if (platform->boost_egl_min_lock)
			exynos_pm_qos_add_request(&exynos5_g3d_cpu_cluster2_min_qos, PM_QOS_CLUSTER2_FREQ_MIN, 0);
#endif
		for (idx = 0; idx < platform->table_size; idx++)
			platform->save_cpu_max_freq[idx] = platform->table[idx].cpu_big_max_freq;

		platform->is_pm_qos_init = true;

		if (cpu_cluster0_policy)
			cpufreq_cpu_put(cpu_cluster0_policy);

		if (cpu_cluster1_policy)
			cpufreq_cpu_put(cpu_cluster1_policy);

		if (cpu_cluster2_policy)
			cpufreq_cpu_put(cpu_cluster2_policy);

		break;
	case GPU_CONTROL_PM_QOS_DEINIT:
		exynos_pm_qos_remove_request(&exynos5_g3d_mif_min_qos);
		if (platform->pmqos_mif_max_clock)
			exynos_pm_qos_remove_request(&exynos5_g3d_mif_max_qos);

		freq_qos_tracer_remove_request(&g3d_req_cpu_cluster0_min_qos);
		freq_qos_tracer_remove_request(&g3d_req_cpu_cluster1_max_qos);
		//exynos_pm_qos_remove_request(&exynos5_g3d_cpu_cluster1_max_qos);
#if PM_QOS_CPU_CLUSTER_NUM == 2
		if (platform->boost_egl_min_lock)

			freq_qos_tracer_remove_request(&g3d_req_cpu_cluster1_min_qos);
#endif
#if PM_QOS_CPU_CLUSTER_NUM == 3

		freq_qos_tracer_remove_request(&g3d_req_cpu_cluster1_min_qos);

		freq_qos_tracer_remove_request(&g3d_req_cpu_cluster2_max_qos);
		if (platform->boost_egl_min_lock)
			exynos_pm_qos_remove_request(&exynos5_g3d_cpu_cluster2_min_qos);
#endif
		platform->is_pm_qos_init = false;
		break;
	case GPU_CONTROL_PM_QOS_SET:
		if (!platform->is_pm_qos_init) {
			GPU_LOG(DVFS_ERROR, DUMMY, 0u, 0u, "%s: PM QOS ERROR : pm_qos deinit -> set\n", __func__);
			return -ENOENT;
		}
		KBASE_DEBUG_ASSERT(platform->step >= 0);
#ifdef CONFIG_MALI_TSG
		if (platform->is_pm_qos_tsg == true) {
			exynos_pm_qos_update_request(&exynos5_g3d_mif_min_qos, 0);
		} else {
			exynos_pm_qos_update_request(&exynos5_g3d_mif_min_qos, platform->table[platform->step].mem_freq);
		}
#else
		exynos_pm_qos_update_request(&exynos5_g3d_mif_min_qos, platform->table[platform->step].mem_freq);
#endif /* CONFIG_MALI_TSG */
		if (platform->pmqos_mif_max_clock &&
				(platform->table[platform->step].clock >= platform->pmqos_mif_max_clock_base))
			exynos_pm_qos_update_request(&exynos5_g3d_mif_max_qos, platform->pmqos_mif_max_clock);

		if (!(platform->heavy_compute_mode & (HCM_MODE_A | HCM_MODE_C))) {
#ifdef CONFIG_MALI_TSG
		/* if is_pm_qos_tsg is true, it has to unlock all of cpu pmqos for joint governor */
		if (platform->is_pm_qos_tsg == true){
			checked_freq_qos_update_request(&g3d_req_cpu_cluster0_min_qos, 0);
		} else{
			checked_freq_qos_update_request(&g3d_req_cpu_cluster0_min_qos, platform->table[platform->step].cpu_little_min_freq);
		}
#else
		checked_freq_qos_update_request(&g3d_req_cpu_cluster0_min_qos, platform->table[platform->step].cpu_little_min_freq);
#endif	/* CONFIG_MALI_TSG */
		}

#if PM_QOS_CPU_CLUSTER_NUM == 3
#ifdef CONFIG_MALI_TSG
		/* if is_pm_qos_tsg is true, it has to unlock all of cpu pmqos for joint governor */
		if (platform->is_pm_qos_tsg == true) {
			checked_freq_qos_update_request(&g3d_req_cpu_cluster1_min_qos, 0);
			checked_freq_qos_update_request(&g3d_req_cpu_cluster2_max_qos, PM_QOS_CLUSTER2_FREQ_MAX_DEFAULT_VALUE);
		} else {
			checked_freq_qos_update_request(&g3d_req_cpu_cluster1_min_qos, platform->table[platform->step].cpu_middle_min_freq);
			checked_freq_qos_update_request(&g3d_req_cpu_cluster2_max_qos, platform->table[platform->step].cpu_big_max_freq);
		}
#else
		checked_freq_qos_update_request(&g3d_req_cpu_cluster1_min_qos, platform->table[platform->step].cpu_middle_min_freq);
		checked_freq_qos_update_request(&g3d_req_cpu_cluster2_max_qos, platform->table[platform->step].cpu_big_max_freq);
#endif	/* CONFIG_MALI_TSG */
#if defined(CONFIG_MALI_SEC_CL_BOOST) && defined(CONFIG_MALI_CL_PMQOS)
		if (pkbdev->pm.backend.metrics.is_full_compute_util && platform->cl_boost_disable == false) {
			platform->gpu_operation_mode_info = CL_FULL;
			exynos_pm_qos_update_request(&exynos5_g3d_mif_min_qos, platform->cl_pmqos_table[platform->step].mif_min_freq); /* MIF Min request  */
			checked_freq_qos_update_request(&g3d_req_cpu_cluster0_min_qos, platform->cl_pmqos_table[platform->step].cpu_little_min_freq);	/* LIT Min request */
#if PM_QOS_CPU_CLUSTER_NUM == 2
			checked_freq_qos_update_request(&g3d_req_cpu_cluster2_max_qos, platform->cl_pmqos_table[platform->step].cpu_big_max_freq); /* BIG Max request */
#endif
#if PM_QOS_CPU_CLUSTER_NUM == 3
			checked_freq_qos_update_request(&g3d_req_cpu_cluster1_min_qos, platform->cl_pmqos_table[platform->step].cpu_middle_min_freq); /* MID Min request */
			checked_freq_qos_update_request(&g3d_req_cpu_cluster2_max_qos, platform->cl_pmqos_table[platform->step].cpu_big_max_freq); /* BIG Max request */
#endif
		} else {
			platform->gpu_operation_mode_info = GL_NORMAL;
		}
#endif

#ifdef CONFIG_MALI_SEC_CL_BOOST
		if (pkbdev->pm.backend.metrics.is_full_compute_util && platform->cl_boost_disable == false)
			checked_freq_qos_update_request(&g3d_req_cpu_cluster2_max_qos, PM_QOS_CLUSTER2_FREQ_MAX_DEFAULT_VALUE);

		if (platform->heavy_compute_mode & HCM_MODE_A)
			checked_freq_qos_update_request(&g3d_req_cpu_cluster0_min_qos, platform->gpu_heavy_compute_cpu0_min_clock);
		else if (platform->heavy_compute_mode & HCM_MODE_C)
			checked_freq_qos_update_request(&g3d_req_cpu_cluster0_min_qos, platform->gpu_heavy_compute_vk_cpu0_min_clock);
#endif
#endif
		break;
	case GPU_CONTROL_PM_QOS_RESET:
		if (!platform->is_pm_qos_init) {
			GPU_LOG(DVFS_ERROR, DUMMY, 0u, 0u, "%s: PM QOS ERROR : pm_qos deinit -> reset\n", __func__);
			return -ENOENT;
		}
		exynos_pm_qos_update_request(&exynos5_g3d_mif_min_qos, 0);
		if (platform->pmqos_mif_max_clock)
			exynos_pm_qos_update_request(&exynos5_g3d_mif_max_qos, PM_QOS_BUS_THROUGHPUT_MAX_DEFAULT_VALUE);
		checked_freq_qos_update_request(&g3d_req_cpu_cluster0_min_qos, 0);
		checked_freq_qos_update_request(&g3d_req_cpu_cluster1_max_qos, PM_QOS_CLUSTER1_FREQ_MAX_DEFAULT_VALUE);
		//exynos_pm_qos_update_request(&exynos5_g3d_cpu_cluster1_max_qos, PM_QOS_CLUSTER1_FREQ_MAX_DEFAULT_VALUE);
#if PM_QOS_CPU_CLUSTER_NUM == 3
		checked_freq_qos_update_request(&g3d_req_cpu_cluster1_min_qos, 0);
		checked_freq_qos_update_request(&g3d_req_cpu_cluster2_max_qos, PM_QOS_CLUSTER2_FREQ_MAX_DEFAULT_VALUE);
#endif
		break;
	case GPU_CONTROL_PM_QOS_EGL_SET:
		if (!platform->is_pm_qos_init) {
			GPU_LOG(DVFS_ERROR, DUMMY, 0u, 0u, "%s: PM QOS ERROR : pm_qos deinit -> egl_set\n", __func__);
			return -ENOENT;
		}
		if (platform->boost_egl_min_lock)
			exynos_pm_qos_update_request_timeout(&exynos5_g3d_cpu_cluster1_min_qos, platform->boost_egl_min_lock, 30000);

		for (idx = 0; idx < platform->table_size; idx++) {
			platform->table[idx].cpu_big_max_freq = PM_QOS_CLUSTER1_FREQ_MAX_DEFAULT_VALUE;
		}
#if PM_QOS_CPU_CLUSTER_NUM == 3
		if (platform->boost_egl_min_lock)
			exynos_pm_qos_update_request_timeout(&exynos5_g3d_cpu_cluster2_min_qos, platform->boost_egl_min_lock, 30000);

		for (idx = 0; idx < platform->table_size; idx++) {
			platform->table[idx].cpu_big_max_freq = PM_QOS_CLUSTER2_FREQ_MAX_DEFAULT_VALUE;
		}
#endif
		break;
	case GPU_CONTROL_PM_QOS_EGL_RESET:
		if (!platform->is_pm_qos_init) {
			GPU_LOG(DVFS_ERROR, DUMMY, 0u, 0u, "%s: PM QOS ERROR : pm_qos deinit -> egl_reset\n", __func__);
			return -ENOENT;
		}
		for (idx = 0; idx < platform->table_size; idx++)
			platform->table[idx].cpu_big_max_freq = platform->save_cpu_max_freq[idx];
		break;
	default:
		break;
	}

	return 0;
}
#endif
