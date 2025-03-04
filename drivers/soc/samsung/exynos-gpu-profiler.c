#include <linux/init.h>
#include <linux/module.h>
#include <linux/of.h>
#include <linux/platform_device.h>

#include <soc/samsung/cal-if.h>
#include <soc/samsung/exynos-profiler.h>
#include <soc/samsung/exynos-migov.h>
#include "exynos-gpu-api.h"

enum hwevent {
	Q0_EMPTY,
	Q1_EMPTY,
	NUM_OF_Q,
};

/* Result during profile time */
struct profile_result {
	struct freq_cstate_result	fc_result;

	s32			cur_temp;
	s32			avg_temp;

	/* private data */
	ktime_t			queued_time_snap[NUM_OF_Q];
	ktime_t			queued_last_updated;
	ktime_t			queued_time_ratio[NUM_OF_Q];
};

static struct profiler {
	struct device_node	*root;

	int			enabled;

	s32			migov_id;
	u32			cal_id;

	struct freq_table	*table;
	u32			table_cnt;
	u32			dyn_pwr_coeff;
	u32			st_pwr_coeff;

	const char			*tz_name;
	struct thermal_zone_device	*tz;

	struct freq_cstate		fc;
	struct freq_cstate_snapshot	fc_snap[NUM_OF_USER];

	u32			cur_freq_idx;	/* current freq_idx */
	u32			max_freq_idx;	/* current max_freq_idx */
	u32			min_freq_idx;	/* current min_freq_idx */

	struct profile_result	result[NUM_OF_USER];

	struct kobject		kobj;
} profiler;

/************************************************************************
 *				HELPER					*
 ************************************************************************/

/************************************************************************
 *				SUPPORT-MIGOV				*
 ************************************************************************/
u32 gpupro_get_table_cnt(s32 id)
{
	return profiler.table_cnt;
}

u32 gpupro_get_freq_table(s32 id, u32 *table)
{
	int idx;
	for (idx = 0; idx < profiler.table_cnt; idx++)
		table[idx] = profiler.table[idx].freq;

	return idx;
}

u32 gpupro_get_max_freq(s32 id)
{
	return profiler.table[profiler.max_freq_idx].freq;
}

u32 gpupro_get_min_freq(s32 id)
{
	return profiler.table[profiler.min_freq_idx].freq;
}

u32 gpupro_get_freq(s32 id)
{
	return profiler.result[MIGOV].fc_result.freq[ACTIVE];
}

void gpupro_get_power(s32 id, u64 *dyn_power, u64 *st_power)
{
	*dyn_power = profiler.result[MIGOV].fc_result.dyn_power;
	*st_power = profiler.result[MIGOV].fc_result.st_power;
}

void gpupro_get_power_change(s32 id, s32 freq_delta_ratio,
			u32 *freq, u64 *dyn_power, u64 *st_power)
{
	struct profile_result *result = &profiler.result[MIGOV];
	struct freq_cstate_result *fc_result = &result->fc_result;
	int flag = (STATE_SCALE_WO_SPARE | STATE_SCALE_TIME | STATE_SCALE_WITH_ORG_CAP);

	get_power_change(profiler.table, profiler.table_cnt,
		profiler.cur_freq_idx, profiler.min_freq_idx, profiler.max_freq_idx,
		fc_result->time[ACTIVE], fc_result->time[CLK_OFF], freq_delta_ratio,
		fc_result->profile_time, result->avg_temp, flag, dyn_power, st_power, freq);
}

u32 gpupro_get_active_pct(s32 id)
{
	return profiler.result[MIGOV].fc_result.ratio[ACTIVE];
}

s32 gpupro_get_temp(s32 id)
{
	return profiler.result[MIGOV].avg_temp;
}

void gpupro_set_margin(s32 id, s32 margin)
{
	exynos_migov_set_gpu_margin(margin);
	return;
}

static void gpupro_reset_profiler(int user)
{
	struct freq_cstate *fc = &profiler.fc;
	struct profile_result *result = &profiler.result[user];
	struct freq_cstate_snapshot *fc_snap = &profiler.fc_snap[user];
	ktime_t *cur_queued_time;
	int idx;

	profiler.fc.time[ACTIVE] = exynos_stats_get_gpu_time_in_state();
	sync_fcsnap_with_cur(fc, fc_snap, profiler.table_cnt);

	result->queued_last_updated = exynos_stats_get_gpu_queued_last_updated();
	
	cur_queued_time = exynos_stats_get_gpu_queued_job_time();
	if (cur_queued_time) {
		for (idx = 0; idx < NUM_OF_Q; idx++)
			result->queued_time_snap[idx] = cur_queued_time[idx];
	}
}

static u32 gpupro_update_profile(int user);
u32 gpupro_update_mode(s32 id, int mode)
{
	if (profiler.enabled != mode) {
		/* reset profiler struct at start time */
		if (mode)
			gpupro_reset_profiler(MIGOV);

		profiler.enabled = mode;

		return 0;
	}
	gpupro_update_profile(MIGOV);

	return 0;
}

u64 gpupro_get_q_empty_pct(s32 type)
{
	return profiler.result[MIGOV].queued_time_ratio[type];
}

u64 gpupro_get_input_nr_avg_cnt(void)
{
	return 0;
}

struct private_fn_gpu gpu_pd_fn = {
	.get_q_empty_pct        = &gpupro_get_q_empty_pct,
	.get_input_nr_avg_cnt   = &gpupro_get_input_nr_avg_cnt,
};
struct domain_fn gpu_fn = {
	.get_table_cnt		= &gpupro_get_table_cnt,
	.get_freq_table		= &gpupro_get_freq_table,
	.get_max_freq		= &gpupro_get_max_freq,
	.get_min_freq		= &gpupro_get_min_freq,
	.get_freq		= &gpupro_get_freq,
	.get_power		= &gpupro_get_power,
	.get_power_change	= &gpupro_get_power_change,
	.get_active_pct		= &gpupro_get_active_pct,
	.get_temp		= &gpupro_get_temp,
	.set_margin		= &gpupro_set_margin,
	.update_mode		= &gpupro_update_mode,
};

/************************************************************************
 *			Gathering GPU Freq Information			*
 ************************************************************************/
static u32 gpupro_update_profile(int user)
{
	struct profile_result *result = &profiler.result[user];
	struct freq_cstate *fc = &profiler.fc;
	struct freq_cstate_snapshot *fc_snap = &profiler.fc_snap[user];
	struct freq_cstate_result *fc_result = &result->fc_result;
	ktime_t* cur_queued_time;
	ktime_t cur_queued_last_updated;
	ktime_t queued_updated_delta;
	int idx;
//	static int g_cnt;

	/* Common Data */
	if (profiler.tz) {
		int temp = get_temp(profiler.tz);
		profiler.result[user].avg_temp = (temp + profiler.result[user].cur_temp) >> 1;
		profiler.result[user].cur_temp = temp;
	}

	profiler.cur_freq_idx = get_idx_from_freq(profiler.table,
			profiler.table_cnt, gpu_dvfs_get_cur_clock(), RELATION_LOW);
	profiler.max_freq_idx = get_idx_from_freq(profiler.table,
			profiler.table_cnt, exynos_stats_get_gpu_max_lock(), RELATION_LOW);
	profiler.min_freq_idx = get_idx_from_freq(profiler.table,
			profiler.table_cnt, exynos_stats_get_gpu_min_lock(), RELATION_HIGH);

	profiler.fc.time[ACTIVE] = exynos_stats_get_gpu_time_in_state();

	make_snapshot_and_time_delta(fc, fc_snap, fc_result, profiler.table_cnt);

	compute_freq_cstate_result(profiler.table, fc_result, profiler.table_cnt,
			profiler.cur_freq_idx, profiler.result[user].avg_temp);

	/* Private Data */
	cur_queued_time = exynos_stats_get_gpu_queued_job_time();
	cur_queued_last_updated = exynos_stats_get_gpu_queued_last_updated();
	queued_updated_delta = cur_queued_last_updated - result->queued_last_updated;
	if (cur_queued_time) {
		for (idx = 0; idx < NUM_OF_Q; idx++) {
			ktime_t queued_time_delta = cur_queued_time[idx] - result->queued_time_snap[idx];

			if (queued_updated_delta)
				result->queued_time_ratio[idx] =
					(queued_time_delta * RATIO_UNIT) / queued_updated_delta;
			else
				result->queued_time_ratio[idx] = 0;
			result->queued_time_ratio[idx] =
				min_t(ktime_t, result->queued_time_ratio[idx], (ktime_t) RATIO_UNIT);
			result->queued_time_snap[idx] = cur_queued_time[idx];
		}
	}
	result->queued_last_updated = cur_queued_last_updated;

	return 0;
}

/************************************************************************
 *						INITIALIZATON									*
 ************************************************************************/
static int register_export_fn(u32 *max_freq, u32 *min_freq, u32 *cur_freq)
{
	*max_freq = (unsigned long)gpu_dvfs_get_max_freq();
	*min_freq = (unsigned long)gpu_dvfs_get_min_freq();
	*cur_freq = (unsigned long)gpu_dvfs_get_cur_clock();

	profiler.table_cnt = exynos_stats_get_gpu_table_size();
	profiler.fc.time[ACTIVE] = exynos_stats_get_gpu_time_in_state();

	return 0;
}

static int parse_dt(struct device_node *dn)
{
	int ret;

	/* necessary data */
	ret = of_property_read_u32(dn, "cal-id", &profiler.cal_id);
	if (ret)
		return -2;

	/* un-necessary data */
	ret = of_property_read_s32(dn, "migov-id", &profiler.migov_id);
	if (ret)
		profiler.migov_id = -1;	/* Don't support migov */

	of_property_read_u32(dn, "power-coefficient", &profiler.dyn_pwr_coeff);
	of_property_read_u32(dn, "static-power-coefficient", &profiler.st_pwr_coeff);
	of_property_read_string(dn, "tz-name", &profiler.tz_name);

	return 0;
}

static int init_profile_result(struct profile_result *result, int size)
{
	if (init_freq_cstate_result(&result->fc_result, NUM_OF_CSTATE, size))
		return -ENOMEM;
	return 0;
}

#ifdef CONFIG_EXYNOS_DEBUG_INFO
static void show_profiler_info(void)
{
	int idx;

	pr_info("================ gpu domain ================\n");
	pr_info("min= %dKHz, max= %dKHz\n",
			profiler.table[profiler.table_cnt - 1].freq, profiler.table[0].freq);
	for (idx = 0; idx < profiler.table_cnt; idx++)
		pr_info("lv=%3d freq=%8d volt=%8d dyn_cost=%5d st_cost=%5d\n",
			idx, profiler.table[idx].freq, profiler.table[idx].volt,
			profiler.table[idx].dyn_cost,
			profiler.table[idx].st_cost);
	if (profiler.tz_name)
		pr_info("support temperature (tz_name=%s)\n", profiler.tz_name);
	if (profiler.migov_id != -1)
		pr_info("support migov domain(id=%d)\n", profiler.migov_id);
}
#endif

static int exynos_gpu_profiler_probe(struct platform_device *pdev)
{
	unsigned int org_max_freq, org_min_freq, cur_freq;
	int ret, idx;

	/* get node of device tree */
	if (!pdev->dev.of_node) {
		pr_err("gpupro: failed to get device treee\n");
		return -EINVAL;
	}
	profiler.root = pdev->dev.of_node;

	/* Parse data from Device Tree to init domain */
	ret = parse_dt(profiler.root);
	if (ret) {
		pr_err("gpupro: failed to parse dt(ret: %d)\n", ret);
		return -EINVAL;
	}

	register_export_fn(&org_max_freq, &org_min_freq, &cur_freq);

	/* init freq table */
	profiler.table = init_freq_table(
			exynos_stats_get_gpu_freq_table(), profiler.table_cnt,
			profiler.cal_id, org_max_freq, org_min_freq,
			profiler.dyn_pwr_coeff, profiler.st_pwr_coeff,
			PWR_COST_CFVV, PWR_COST_CFVV);
	if (!profiler.table) {
		pr_err("gpupro: failed to init freq_table\n");
		return -EINVAL;
	}
	profiler.max_freq_idx = 0;
	profiler.min_freq_idx = profiler.table_cnt - 1;
	profiler.cur_freq_idx = get_idx_from_freq(profiler.table,
				profiler.table_cnt, cur_freq, RELATION_HIGH);

	if (init_freq_cstate(&profiler.fc, NUM_OF_CSTATE, profiler.table_cnt))
			return -ENOMEM;

	/* init snapshot & result table */
	for (idx = 0; idx < NUM_OF_USER; idx++) {
		if (init_freq_cstate_snapshot(&profiler.fc_snap[idx],
						NUM_OF_CSTATE, profiler.table_cnt))
			return -ENOMEM;

		if (init_profile_result(&profiler.result[idx], profiler.table_cnt))
			return -EINVAL;
	}

	/* get thermal-zone to get temperature */
	if (profiler.tz_name)
		profiler.tz = init_temp(profiler.tz_name);

	if (profiler.tz)
		init_static_cost(profiler.table, profiler.table_cnt,
				1, profiler.root, profiler.tz);

	ret = exynos_migov_register_domain(MIGOV_GPU, &gpu_fn, &gpu_pd_fn);

#ifdef CONFIG_EXYNOS_DEBUG_INFO
	show_profiler_info();
#endif

	return ret;
}

static const struct of_device_id exynos_gpu_profiler_match[] = {
	{
		.compatible	= "samsung,exynos-gpu-profiler",
	},
	{},
};
MODULE_DEVICE_TABLE(of, exynos_gpu_profiler_match);

static struct platform_driver exynos_gpu_profiler_driver = {
	.probe		= exynos_gpu_profiler_probe,
	.driver	= {
		.name	= "exynos-gpu-profiler",
		.owner	= THIS_MODULE,
		.of_match_table = exynos_gpu_profiler_match,
	},
};

static int exynos_gpu_profiler_init(void)
{
	return platform_driver_register(&exynos_gpu_profiler_driver);
}
late_initcall(exynos_gpu_profiler_init);

MODULE_SOFTDEP("pre: exynos-migov");
MODULE_DESCRIPTION("Exynos GPU Profiler");
MODULE_LICENSE("GPL");
