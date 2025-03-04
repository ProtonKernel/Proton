/* exynos-profiler.h
 *
 * Copyright (C) 2020 Samsung Electronics Co., Ltd.
 *		http://www.samsung.com
 *
 * EXYNOS - Header file for Exynos Multi IP Governor support
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#ifndef __EXYNOS_PROFILER_H
#define __EXYNOS_PROFILER_H

#include <linux/thermal.h>
#include <soc/samsung/cal-if.h>
#include <linux/slab.h>

#define RATIO_UNIT		1000

enum user_type {
	SYSFS,
	MIGOV,
	NUM_OF_USER,
};

enum cstate {
	ACTIVE,
	CLK_OFF,
	PWR_OFF,
	NUM_OF_CSTATE,
};

/* Structure for FREQ */
struct freq_table {
	u32	freq;		/* KHz */
	u32	volt;		/* uV */

	/*
	 * Equation for cost
	 * CPU/GPU : Dyn_Coeff/St_Coeff * F(MHz) * V(mV)^2
	 * MIF	   : Dyn_Coeff/St_Coeff * V(mV)^2
	 */
	u64	dyn_cost;
	u64	st_cost;
};

/*
 * It is free-run count
 * NOTICE: MUST guarantee no overflow
 */
struct freq_cstate {
	ktime_t	*time[NUM_OF_CSTATE];
};
struct freq_cstate_snapshot {
	ktime_t	last_snap_time;
	ktime_t	*time[NUM_OF_CSTATE];
};
struct freq_cstate_result {
	ktime_t	profile_time;

	ktime_t	*time[NUM_OF_CSTATE];
	u32	ratio[NUM_OF_CSTATE];
	u32	freq[NUM_OF_CSTATE];

	u64	dyn_power;
	u64	st_power;
};

static inline u64 * alloc_state_in_freq(int size)
{
	u64 *state_in_state;
	state_in_state = kzalloc(sizeof(u64) * size, GFP_KERNEL);
	if (!state_in_state) {
		pr_err("failed to alloc state_in_freq\n");
		return NULL;
	}
	return state_in_state;
}

static inline
int init_freq_cstate(struct freq_cstate *fc, int depth, int size)
{
	int state;
	for (state = 0; state < depth; state++) {
		ktime_t *time= alloc_state_in_freq(size);
		if (!time) return -ENOMEM;
		fc->time[state] = alloc_state_in_freq(size);
	}
	return 0;
}
static inline
int init_freq_cstate_snapshot(struct freq_cstate_snapshot *fc_snap, int depth, int size)
{
	int state;
	for (state = 0; state < depth; state++) {
		ktime_t *time= alloc_state_in_freq(size);
		if (!time) return -ENOMEM;
		fc_snap->time[state] = alloc_state_in_freq(size);
	}
	return 0;
}
static inline
int init_freq_cstate_result(struct freq_cstate_result *fc_result, int depth, int size)
{
	int state;
	for (state = 0; state < depth; state++) {
		ktime_t *time= alloc_state_in_freq(size);
		if (!time) return -ENOMEM;
		fc_result->time[state] = alloc_state_in_freq(size);
	}
	return 0;
}

static inline
void sync_snap_with_cur(u64 *state_in_freq, u64 *snap, int size)
{
	memcpy(snap, state_in_freq, sizeof(u64) * size);
}

static inline
void make_snap_and_delta(u64 *state_in_freq,
			u64 *snap, u64 *delta, int size)
{
	int idx;

	for (idx = 0; idx < size; idx++) {
		u64 delta_time, cur_snap_time;

		if (!state_in_freq[idx])
			continue;

		/* get current time_in_freq */
		cur_snap_time = state_in_freq[idx];

		/* compute delta between currents snap and previous snap */
		delta_time = cur_snap_time - snap[idx];
		snap[idx] = cur_snap_time;
		delta[idx] = delta_time;
	}
}

static inline
void sync_fcsnap_with_cur(struct freq_cstate *fc,
		struct freq_cstate_snapshot *fc_snap, int size)
{
	int state;

	for (state = 0; state < NUM_OF_CSTATE; state++) {
		if (!fc->time[state])
			continue;
		memcpy(fc_snap->time[state],
			fc->time[state], sizeof(ktime_t) * size);
	}
	/* udpate snapshot time */
	fc_snap->last_snap_time = ktime_get();
}

static inline
void make_snapshot_and_time_delta(struct freq_cstate *fc,
			struct freq_cstate_snapshot *fc_snap,
			struct freq_cstate_result *fc_result,
			int size)
{
	int state, idx;

	/* compute time_delta and make snapshot */
	for (state = 0; state < NUM_OF_CSTATE; state++) {
		if (!fc->time[state])
			continue;

		for (idx = 0; idx < size; idx++) {
			u64 delta_time, cur_snap_time;

			/* get current time_in_freq */
			cur_snap_time = fc->time[state][idx];

			/* compute delta between currents snap and previous snap */
			delta_time = cur_snap_time - fc_snap->time[state][idx];
			fc_snap->time[state][idx] = cur_snap_time;
			fc_result->time[state][idx] = delta_time;
		}
	}

	/* save current time */
	fc_result->profile_time = ktime_get() - fc_snap->last_snap_time;
	fc_snap->last_snap_time = ktime_get();
}

#define nsec_to_usec(time)	((time) / 1000)
#define khz_to_mhz(freq)	((freq) / 1000)
#define ST_COST_BASE_TEMP_WEIGHT	(70 * 70)
static inline
void compute_freq_cstate_result(struct freq_table *freq_table,
	struct freq_cstate_result *fc_result, int size, int cur_freq_idx, int temp)
{
	ktime_t total_time;
	u64 freq, ratio;
	u64 st_power = 0, dyn_power = 0;
	ktime_t profile_time = fc_result->profile_time;
	s32 state, idx;
	s32 temp_weight = temp ? (temp * temp) : 1;
	s32 temp_base_weight = temp ? ST_COST_BASE_TEMP_WEIGHT : 1;

	if (unlikely(!fc_result->profile_time))
		return;

	for (state = 0; state < NUM_OF_CSTATE; state++) {
		if (!fc_result->time[state])
			continue;

		total_time = freq = ratio = 0;
		for (idx = 0; idx < size; idx++) {
			ktime_t time = fc_result->time[state][idx];

			total_time += time;
			freq += time * khz_to_mhz(freq_table[idx].freq);

			if (state == ACTIVE) {
				st_power += (time * freq_table[idx].st_cost) * temp_weight;
				dyn_power += (time * freq_table[idx].dyn_cost);
			}

			if (state == CLK_OFF)
				st_power += (time * freq_table[idx].st_cost) * temp_weight;
		}

		fc_result->ratio[state] = total_time * RATIO_UNIT / profile_time;
		fc_result->freq[state] = (RATIO_UNIT * freq) / total_time;

		if (!fc_result->freq[state])
			fc_result->freq[state] = freq_table[cur_freq_idx].freq;
	}
	fc_result->dyn_power = dyn_power / profile_time;
	fc_result->st_power = st_power / (profile_time * temp_base_weight);
}

#define RELATION_LOW	0
#define RELATION_HIGH	1
/*
 * Input table should be DECENSING-ORDER
 * RELATION_LOW : return idx of freq lower than or same with input freq
 * RELATOIN_HIGH: return idx of freq higher thant or same with input freq
 */
static inline u32 get_idx_from_freq(struct freq_table *table,
				u32 size, u32 freq, bool flag)
{
	int idx;

	if (flag == RELATION_HIGH) {
		for (idx = size - 1; idx >=  0; idx--) {
			if (freq <= table[idx].freq)
				return idx;
		}
	} else {
		for (idx = 0; idx < size; idx++) {
			if (freq >= table[idx].freq)
				return idx;
		}
	}
	return flag == RELATION_HIGH ? 0 : size - 1;
}

#define STATE_SCALE_CNT			(1 << 0)	/* ramainnig cnt even if freq changed */
#define STATE_SCALE_TIME		(1 << 1)	/* scaling up/down time with changed freq */
#define	STATE_SCALE_WITH_SPARE		(1 << 2)	/* freq boost with spare cap */
#define	STATE_SCALE_WO_SPARE		(1 << 3)	/* freq boost without spare cap */
#define	STATE_SCALE_WITH_ORG_CAP	(1 << 4)	/* freq boost with original capacity */
static inline
void compute_power_freq(struct freq_table *freq_table, u32 size, s32 cur_freq_idx,
		u64 *active_in_freq, u64 *clkoff_in_freq, u64 profile_time,
		s32 temp, u64 *dyn_power_ptr, u64 *st_power_ptr,
		u32 *active_freq_ptr, u32 *active_ratio_ptr)
{
	s32 idx;
	u64 st_power = 0, dyn_power = 0;
	u64 active_freq = 0, total_time = 0;
	s32 temp_weight = temp ? (temp * temp) : 1;
	s32 temp_base_weight = temp ? ST_COST_BASE_TEMP_WEIGHT : 1;

	if (unlikely(!profile_time))
		return;

	for (idx = 0; idx < size; idx++) {
		u64 time;

		if (likely(active_in_freq)) {
			time = active_in_freq[idx];

			dyn_power += (time* freq_table[idx].dyn_cost);
			st_power += (time* freq_table[idx].st_cost) * temp_weight;
			active_freq += time* khz_to_mhz(freq_table[idx].freq);
			total_time += time;
		}

		if (likely(clkoff_in_freq)) {
			time = clkoff_in_freq[idx];
			st_power += (time* freq_table[idx].st_cost) * temp_weight;
		}
	}

	if (active_ratio_ptr)
		*active_ratio_ptr = total_time * RATIO_UNIT / profile_time;

	if (active_freq_ptr) {
		*active_freq_ptr = active_freq * RATIO_UNIT / total_time;
		if (*active_freq_ptr == 0)
			*active_freq_ptr = khz_to_mhz(freq_table[cur_freq_idx].freq);
	}

	if (dyn_power_ptr)
		*dyn_power_ptr = dyn_power / profile_time;

	if (st_power_ptr)
		*st_power_ptr = st_power / (profile_time * temp_base_weight);
}

static inline
unsigned int get_boundary_with_spare(struct freq_table *freq_table,
			s32 max_freq_idx, s32 freq_delta_ratio, s32 cur_idx)
{
	int cur_freq = freq_table[cur_idx].freq;
	int max_freq = freq_table[max_freq_idx].freq;

	return (cur_freq * RATIO_UNIT - max_freq * freq_delta_ratio)
					/ (RATIO_UNIT - freq_delta_ratio);
}

static inline
unsigned int get_boundary_wo_spare(struct freq_table *freq_table,
					s32 freq_delta_ratio, s32 cur_idx)
{
	if (freq_delta_ratio <= 0)
		return freq_table[cur_idx + 1].freq * RATIO_UNIT
				/ (RATIO_UNIT + freq_delta_ratio);

	return freq_table[cur_idx].freq * RATIO_UNIT
				/ (RATIO_UNIT + freq_delta_ratio);
}

static inline
unsigned int get_boundary_freq(struct freq_table *freq_table,
		s32 max_freq_idx, s32 freq_delta_ratio, s32 flag, s32 cur_idx)
{
	if (freq_delta_ratio <= 0)
		return get_boundary_wo_spare(freq_table, freq_delta_ratio, cur_idx);

	if (flag & STATE_SCALE_WITH_SPARE)
		return get_boundary_with_spare(freq_table, max_freq_idx,
						freq_delta_ratio, cur_idx);

	return get_boundary_wo_spare(freq_table, freq_delta_ratio, cur_idx);

}

static inline
int get_boundary_freq_delta_ratio(struct freq_table *freq_table,
		s32 min_freq_idx, s32 cur_idx, s32 freq_delta_ratio, u32 boundary_freq)
{
	int delta_pct;
	unsigned int lower_freq;
	unsigned int cur_freq = freq_table[cur_idx].freq;

	lower_freq = (cur_idx == min_freq_idx) ? 0 : freq_table[cur_idx + 1].freq;

	if (freq_delta_ratio > 0)
		delta_pct = (cur_freq - boundary_freq) * RATIO_UNIT
						/ (cur_freq - lower_freq);
	else
		delta_pct = (boundary_freq - lower_freq) * RATIO_UNIT
						/ (cur_freq - lower_freq);
	return delta_pct;
}

static inline
unsigned int get_freq_with_spare(struct freq_table *freq_table,
	s32 max_freq_idx, s32 cur_freq, s32 freq_delta_ratio, s32 flag)
{
	if (flag & STATE_SCALE_WITH_ORG_CAP)
		max_freq_idx = 0;

	return cur_freq + ((freq_table[max_freq_idx].freq - cur_freq)
					* freq_delta_ratio / RATIO_UNIT);
}

static inline
unsigned int get_freq_wo_spare(struct freq_table *freq_table,
		s32 max_freq_idx, s32 cur_freq, s32 freq_delta_ratio)
{
	return cur_freq + ((cur_freq * freq_delta_ratio) / RATIO_UNIT);
}

static inline
int get_scaled_target_idx(struct freq_table *freq_table,
			s32 min_freq_idx, s32 max_freq_idx,
			s32 freq_delta_ratio, s32 flag, s32 cur_idx)
{
	int idx, cur_freq, target_freq;
	int target_idx;

	cur_freq = freq_table[cur_idx].freq;

	if (flag & STATE_SCALE_WITH_SPARE) {
		if (freq_delta_ratio > 0)
			target_freq = get_freq_with_spare(freq_table, max_freq_idx,
							cur_freq, freq_delta_ratio, flag);
		else
			target_freq = get_freq_wo_spare(freq_table, max_freq_idx,
							cur_freq, freq_delta_ratio);
	} else {
		target_freq = get_freq_wo_spare(freq_table, max_freq_idx,
						cur_freq, freq_delta_ratio);
	}

	if (target_freq == cur_freq)
		target_idx = cur_idx;

	if (target_freq > cur_freq) {
		for (idx = cur_idx; idx >= max_freq_idx; idx--) {
			if (freq_table[idx].freq >= target_freq) {
				target_idx = idx;
				break;
			}
			target_idx = 0;
		}
	} else {
		for (idx = cur_idx; idx <= min_freq_idx; idx++) {
			if (freq_table[idx].freq < target_freq) {
				target_idx = idx;
				break;
			}
			target_idx = min_freq_idx;
		}
	}

	if (abs(target_idx - cur_idx) > 1)
		target_idx = ((target_idx - cur_idx) > 0) ? cur_idx + 1 : cur_idx - 1;

	return target_idx;
}

static inline
void __scale_state_in_freq(struct freq_table *freq_table,
		s32 min_freq_idx, s32 max_freq_idx,
		u64 *active_state, u64 *clkoff_state,
		s32 freq_delta_ratio, s32 flag, s32 cur_idx)
{
	int target_freq, cur_freq, boundary_freq;
	int real_freq_delta_ratio, boundary_freq_delta_ratio, target_idx;
	ktime_t boundary_time;

	target_idx = get_scaled_target_idx(freq_table, min_freq_idx, max_freq_idx,
						freq_delta_ratio, flag, cur_idx);
	if (target_idx == cur_idx)
		return;

	cur_freq = freq_table[cur_idx].freq;
	target_freq = freq_table[target_idx].freq;

	boundary_freq = get_boundary_freq(freq_table, max_freq_idx,
				freq_delta_ratio, flag, cur_idx);
	if (freq_delta_ratio > 0) {
		if (boundary_freq <= freq_table[cur_idx + 1].freq)
			boundary_freq = freq_table[cur_idx + 1].freq;
	} else {
		if (boundary_freq > freq_table[cur_idx].freq)
			boundary_freq = freq_table[cur_idx].freq;
	}

	boundary_freq_delta_ratio = get_boundary_freq_delta_ratio(freq_table,
					min_freq_idx, cur_idx, freq_delta_ratio, boundary_freq);
	real_freq_delta_ratio = (target_freq - cur_freq) / (cur_freq / RATIO_UNIT);

	boundary_time = active_state[cur_idx] * boundary_freq_delta_ratio / RATIO_UNIT;

	if (flag & STATE_SCALE_CNT)
		active_state[target_idx] += boundary_time;
	else
		active_state[target_idx] += (boundary_time * RATIO_UNIT)
					/ (RATIO_UNIT + real_freq_delta_ratio);
	active_state[cur_idx] -= boundary_time;
}
static inline
void scale_state_in_freq(struct freq_table *freq_table,
		s32 min_freq_idx, s32 max_freq_idx,
		u64 *active_state, u64 *clkoff_state,
		s32 freq_delta_ratio, s32 flag)
{
	int idx;

	if (freq_delta_ratio > 0)
		for (idx = max_freq_idx + 1; idx <= min_freq_idx; idx++)
			__scale_state_in_freq(freq_table,
				min_freq_idx, max_freq_idx,
				active_state, clkoff_state,
				freq_delta_ratio, flag, idx);
	else
		for (idx = min_freq_idx - 1; idx >= max_freq_idx; idx--)
			__scale_state_in_freq(freq_table,
				min_freq_idx, max_freq_idx,
				active_state, clkoff_state,
				freq_delta_ratio, flag, idx);
}

static inline
void get_power_change(struct freq_table *freq_table, s32 size,
	s32 cur_freq_idx, s32 min_freq_idx, s32 max_freq_idx,
	u64 *active_state, u64 *clkoff_state,
	s32 freq_delta_ratio, u64 profile_time, s32 temp, s32 flag,
	u64 *scaled_dyn_power, u64 *scaled_st_power, u32 *scaled_active_freq)
{
	u64 *scaled_active_state, *scaled_clkoff_state;

	scaled_active_state = kzalloc(sizeof(u64) * size, GFP_KERNEL);
	scaled_clkoff_state = kzalloc(sizeof(u64) * size, GFP_KERNEL);

	/* copy state-in-freq table */
	memcpy(scaled_active_state, active_state, sizeof(u64) * size);
	memcpy(scaled_clkoff_state, clkoff_state, sizeof(u64) * size);

	/* scaling table with freq_delta_ratio */
	scale_state_in_freq(freq_table, min_freq_idx, max_freq_idx,
			scaled_active_state, scaled_clkoff_state,
			freq_delta_ratio, flag);

	/* computing power with scaled table */
	compute_power_freq(freq_table, size, cur_freq_idx,
		scaled_active_state, scaled_clkoff_state, profile_time, temp,
		scaled_dyn_power, scaled_st_power, scaled_active_freq, NULL);

	kfree(scaled_active_state);
	kfree(scaled_clkoff_state);
}

extern int exynos_build_static_power_table(struct device_node *np, int **var_table,
		unsigned int *var_volt_size, unsigned int *var_temp_size, char *tz_name);

#define PWR_COST_CFVV	0
#define PWR_COST_CVV	1
static inline u64 pwr_cost(u32 freq, u32 volt, u32 coeff, bool flag)
{
	u64 cost = coeff;
	/*
	 * Equation for power cost
	 * PWR_COST_CFVV : Coeff * F(MHz) * V^2
	 * PWR_COST_CVV  : Coeff * V^2
	 */
	volt = volt >> 10;
	cost = ((cost * volt * volt) >> 20);
	if (flag == PWR_COST_CFVV)
		return (cost * (freq >> 10)) >> 10;

	return cost;
}

static inline
int init_static_cost(struct freq_table *freq_table, int size, int weight,
			struct device_node *np, struct thermal_zone_device *tz)
{
	int volt_size, temp_size;
	int *table = NULL;
	int freq_idx, idx, ret = 0;

	ret = exynos_build_static_power_table(np, &table, &volt_size, &temp_size, tz->type);
	if (ret) {
		int cal_id, ratio, asv_group, static_coeff;
		int ratio_table[9] = { 5, 6, 8, 11, 15, 20, 27, 36, 48 };

		pr_info("%s: there is no static power table for %s\n", tz->type);

		// Make static power table from coeff and ids
		if (of_property_read_u32(np, "cal-id", &cal_id)) {
			if (of_property_read_u32(np, "g3d_cmu_cal_id", &cal_id)) {
				pr_err("%s: Failed to get cal-id\n", __func__);
				return -EINVAL;
			}
		}

		if (of_property_read_u32(np, "static-power-coefficient", &static_coeff)) {
			pr_err("%s: Failed to get staic_coeff\n", __func__);
			return -EINVAL;
		}

		ratio = cal_asv_get_ids_info(cal_id);
		asv_group = cal_asv_get_grp(cal_id);


		if (asv_group < 0 || asv_group > 8)
			asv_group = 5;

		if (!ratio)
			ratio = ratio_table[asv_group];

		static_coeff *= ratio;

		pr_info("%s static power table through ids (weight=%d)\n", tz->type, weight);
		for (idx = 0; idx < size; idx++) {
			freq_table[idx].st_cost = pwr_cost(freq_table[idx].freq,
					freq_table[idx].volt, static_coeff, PWR_COST_CVV) / weight;
#ifdef CONFIG_EXYNOS_DEBUG_INFO
			pr_info("freq_idx=%2d freq=%d, volt=%d cost=%8d\n",
					idx, freq_table[idx].freq, freq_table[idx].volt, freq_table[idx].st_cost);
#endif
		}

		return 0;
	}

	pr_info("%s static power table from DTM (weight=%d)\n", tz->type, weight);
	for (freq_idx = 0; freq_idx < size; freq_idx++) {
		int freq_table_volt = freq_table[freq_idx].volt / 1000;
		for (idx = 1; idx <= volt_size; idx++) {
			int cost;
			int dtm_volt = *(table + ((temp_size + 1) * idx));
			if (dtm_volt < freq_table_volt)
				continue;

			cost = *(table + ((temp_size + 1) * idx + 1));
			cost = cost / weight;
			freq_table[freq_idx].st_cost = cost;
#ifdef CONFIG_EXYNOS_DEBUG_INFO
			pr_info("freq_idx=%2d freq=%d, volt=%d dtm_volt=%d, cost=%8d\n",
					freq_idx, freq_table[freq_idx].freq, freq_table_volt,
					dtm_volt, cost);
#endif
			break;
		}
	}

	kfree(table);

	return 0;
}

static inline int is_vaild_freq(u32 *freq_table, u32 table_cnt, u32 freq)
{
	int idx;
	for (idx = 0; idx < table_cnt; idx++)
		if(freq_table[idx] == freq)
			return true;
	return false;
}

static inline struct freq_table *init_freq_table (u32 *freq_table, u32 table_cnt,
					u32 cal_id, u32 max_freq, u32 min_freq,
					u32 dyn_ceff, u32 st_ceff,
					bool dyn_flag, bool st_flag)
{
	struct freq_table *table;
	unsigned int *cal_vtable, cal_table_cnt;
	unsigned long *cal_ftable;
	int cal_idx, idx;

	/* alloc table */
	table = kzalloc(sizeof(struct freq_table) * table_cnt, GFP_KERNEL);
	if (!table) {
		pr_err("cpupro: failed to alloc table(min=%d max=%d cnt=%d)\n",
					min_freq, max_freq, table_cnt);
		return NULL;
	}
	/* get cal table including invalid and valid freq */
	cal_table_cnt = cal_dfs_get_lv_num(cal_id);
	cal_ftable = kzalloc(sizeof(unsigned long) * cal_table_cnt, GFP_KERNEL);
	cal_vtable = kzalloc(sizeof(unsigned int) * cal_table_cnt, GFP_KERNEL);
	if (!cal_ftable || !cal_vtable) {
		pr_err("cpupro: failed to alloc cal-table(cal-cnt=%d)\n",
								cal_table_cnt);
		kfree(table);
		return NULL;
	}
	cal_dfs_get_rate_table(cal_id, cal_ftable);
	cal_dfs_get_asv_table(cal_id, cal_vtable);

	/* fill freq/volt and dynamic/static power cost */
	idx = 0;
	for (cal_idx = 0; cal_idx < cal_table_cnt; cal_idx++) {
		unsigned int freq = cal_ftable[cal_idx];

		if (freq > max_freq)
			continue;
		if (freq < min_freq)
			continue;

		if (freq_table && !is_vaild_freq(freq_table, table_cnt, freq))
			continue;

		table[idx].freq = freq;
		table[idx].volt = cal_vtable[cal_idx];
		table[idx].dyn_cost = pwr_cost(table[idx].freq,
					table[idx].volt, dyn_ceff, dyn_flag);
		table[idx].st_cost = pwr_cost(table[idx].freq,
					table[idx].volt, st_ceff, st_flag);

		idx++;
	}
	kfree(cal_ftable);
	kfree(cal_vtable);

	return table;
}

static inline struct thermal_zone_device * init_temp (const char *name)
{
	return thermal_zone_get_zone_by_name(name);
}

static inline s32 get_temp(struct thermal_zone_device *tz)
{
	s32 temp;
	thermal_zone_get_temp(tz, &temp);
	return temp / 1000;
}
#endif /* __EXYNOS_PROFILER_H */
