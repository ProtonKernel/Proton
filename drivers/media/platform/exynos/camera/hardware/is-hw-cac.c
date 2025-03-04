/*
 * Samsung Exynos SoC series Pablo driver
 *
 * Copyright (c) 2019 Samsung Electronics Co., Ltd
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#include "is-hw-mcscaler-v3.h"
#include "api/is-hw-api-mcscaler-v3.h"
#include "../interface/is-interface-ischain.h"
#include "is-param.h"
#include "is-err.h"

struct ref_ni hw_mcsc_find_ni_idx_for_cac(struct is_hw_ip *hw_ip,
	struct cac_setfile_contents *cac, u32 cur_ni)
{
	struct ref_ni ret_idx = {0, 0};
	struct ref_ni ni_idx_range;
	u32 ni_idx;

	for (ni_idx = 0; ni_idx < cac->ni_max - 1; ni_idx++) {
		ni_idx_range.min = MULTIPLIED_10(cac->ni_vals[ni_idx]);
		ni_idx_range.max = MULTIPLIED_10(cac->ni_vals[ni_idx + 1]);

		if (ni_idx_range.min < cur_ni && cur_ni < ni_idx_range.max) {
			ret_idx.min = ni_idx;
			ret_idx.max = ni_idx + 1;
			break;
		} else if (cur_ni == ni_idx_range.min) {
			ret_idx.min = ni_idx;
			ret_idx.max = ni_idx;
			break;
		} else if (cur_ni == ni_idx_range.max) {
			ret_idx.min = ni_idx + 1;
			ret_idx.max = ni_idx + 1;
			break;
		}
	}

	sdbg_hw(2, "[CAC] find_ni_idx: cur_ni %d idx %d,%d range %d,%d\n", hw_ip, cur_ni,
		ret_idx.min, ret_idx.max,
		MULTIPLIED_10(cac->ni_vals[ret_idx.min]), MULTIPLIED_10(cac->ni_vals[ret_idx.max]));

	return ret_idx;
}

void hw_mcsc_calc_cac_map_thr(struct cac_cfg_by_ni *cac_cfg,
	struct cac_cfg_by_ni *cfg_min, struct cac_cfg_by_ni *cfg_max,
	struct ref_ni *ni, u32 cur_ni)
{
	int i = 0;
	struct cac_map_thr_cfg *min, *max;

	min = &cfg_min->map_thr_cfg;
	max = &cfg_max->map_thr_cfg;

#ifdef CAC_G2_VERSION
	cac_cfg->map_thr_cfg.map_right_shift = GET_LNR_INTRPL(
			min->map_right_shift, max->map_right_shift,
			ni->min, ni->max, cur_ni);
	dbg_hw(2, "[CAC] map_thr.map_right_shift: set_val %d range %d,%d\n",
			cac_cfg->map_thr_cfg.map_right_shift,
			min->map_right_shift, max->map_right_shift);
	cac_cfg->map_thr_cfg.map_normal_gain = GET_LNR_INTRPL(
			min->map_normal_gain, max->map_normal_gain,
			ni->min, ni->max, cur_ni);
	dbg_hw(2, "[CAC] map_thr.map_normal_gain: set_val %d range %d,%d\n",
			cac_cfg->map_thr_cfg.map_normal_gain,
			min->map_normal_gain, max->map_normal_gain);
	cac_cfg->map_thr_cfg.map_line_thr = GET_LNR_INTRPL(
			min->map_line_thr, max->map_line_thr,
			ni->min, ni->max, cur_ni);
	dbg_hw(2, "[CAC] map_thr.map_line_thr: set_val %d range %d,%d\n",
			cac_cfg->map_thr_cfg.map_line_thr,
			min->map_line_thr, max->map_line_thr);
	for (i = 0; i < CAC_MAP_COEF_SIZE; ++i) {
		cac_cfg->map_thr_cfg.map_coef_y0[i] = GET_LNR_INTRPL(
				min->map_coef_y0[i], max->map_coef_y0[i],
				ni->min, ni->max, cur_ni);
		dbg_hw(2, "[CAC] map_thr.map_coef_y0[%d]: set_val %d range %d,%d\n",
				i,
				cac_cfg->map_thr_cfg.map_coef_y0[i],
				min->map_coef_y0[i], max->map_coef_y0[i]);
		cac_cfg->map_thr_cfg.map_coef_y1[i] = GET_LNR_INTRPL(
				min->map_coef_y1[i], max->map_coef_y1[i],
				ni->min, ni->max, cur_ni);
		dbg_hw(2, "[CAC] map_thr.map_coef_y1[%d]: set_val %d range %d,%d\n",
				i,
				cac_cfg->map_thr_cfg.map_coef_y1[i],
				min->map_coef_y1[i], max->map_coef_y1[i]);
		cac_cfg->map_thr_cfg.map_coef_y2[i] = GET_LNR_INTRPL(
				min->map_coef_y2[i], max->map_coef_y2[i],
				ni->min, ni->max, cur_ni);
		dbg_hw(2, "[CAC] map_thr.map_coef_y2[%d]: set_val %d range %d,%d\n",
				i,
				cac_cfg->map_thr_cfg.map_coef_y2[i],
				min->map_coef_y2[i], max->map_coef_y2[i]);
	}
	for (i = 0; i < CAC_MAP_BLEND_TBL_SIZE; ++i) {
		cac_cfg->map_thr_cfg.map_blend_tbl[i] = GET_LNR_INTRPL(
				min->map_blend_tbl[i], max->map_blend_tbl[i],
				ni->min, ni->max, cur_ni);
		dbg_hw(2, "[CAC] map_thr.map_blend_tbl[%d]: set_val %d range %d,%d\n",
				i,
				cac_cfg->map_thr_cfg.map_blend_tbl[i],
				min->map_blend_tbl[i], max->map_blend_tbl[i]);
	}
#else
	cac_cfg->map_thr_cfg.map_spot_thr_l = GET_LNR_INTRPL(
			min->map_spot_thr_l, max->map_spot_thr_l,
			ni->min, ni->max, cur_ni);
	dbg_hw(2, "[CAC] map_thr.map_spot_thr_l: set_val %d range %d,%d\n",
			cac_cfg->map_thr_cfg.map_spot_thr_l,
			min->map_spot_thr_l, max->map_spot_thr_l);

	cac_cfg->map_thr_cfg.map_spot_thr_h = GET_LNR_INTRPL(
			min->map_spot_thr_h, max->map_spot_thr_h,
			ni->min, ni->max, cur_ni);
	dbg_hw(2, "[CAC] map_thr.map_spot_thr_h: set_val %d range %d,%d\n",
			cac_cfg->map_thr_cfg.map_spot_thr_h,
			min->map_spot_thr_h, max->map_spot_thr_h);

	cac_cfg->map_thr_cfg.map_spot_thr = GET_LNR_INTRPL(
			min->map_spot_thr, max->map_spot_thr,
			ni->min, ni->max, cur_ni);
	dbg_hw(2, "[CAC] map_thr.map_spot_thr: set_val %d range %d,%d\n",
			cac_cfg->map_thr_cfg.map_spot_thr,
			min->map_spot_thr, max->map_spot_thr);

	cac_cfg->map_thr_cfg.map_spot_nr_strength = GET_LNR_INTRPL(
			min->map_spot_nr_strength, max->map_spot_nr_strength,
			ni->min, ni->max, cur_ni);
	dbg_hw(2, "[CAC] map_thr.map_spot_nr_strength: set_val %d range %d,%d\n",
			cac_cfg->map_thr_cfg.map_spot_nr_strength,
			min->map_spot_nr_strength, max->map_spot_nr_strength);
#endif
}

void hw_mcsc_calc_cac_crt_thr(struct cac_cfg_by_ni *cac_cfg,
	struct cac_cfg_by_ni *cfg_min, struct cac_cfg_by_ni *cfg_max,
	struct ref_ni *ni, u32 cur_ni)
{
	int i = 0;
	struct cac_crt_thr_cfg *min, *max;

	min = &cfg_min->crt_thr_cfg;
	max = &cfg_max->crt_thr_cfg;
#ifdef CAC_G2_VERSION
	for (i = 0; i < CAC_CRT_HUE_TBL_SIZE; ++i) {
		cac_cfg->crt_thr_cfg.crt_hue_tbl[i] = GET_LNR_INTRPL(
				min->crt_hue_tbl[i], max->crt_hue_tbl[i],
				ni->min, ni->max, cur_ni);
		dbg_hw(2, "[CAC] crt_thr.crt_hue_tbl[%d]: set_val %d range %d,%d\n",
				i,
				cac_cfg->crt_thr_cfg.crt_hue_tbl[i],
				min->crt_hue_tbl[i], max->crt_hue_tbl[i]);
	}
#else
	cac_cfg->crt_thr_cfg.crt_color_thr_l_dot = GET_LNR_INTRPL(
			min->crt_color_thr_l_dot, max->crt_color_thr_l_dot,
			ni->min, ni->max, cur_ni);
	dbg_hw(2, "[CAC] crt_thr.crt_color_thr_l_dot: set_val %d range %d,%d\n",
			cac_cfg->crt_thr_cfg.crt_color_thr_l_dot,
			min->crt_color_thr_l_dot, max->crt_color_thr_l_dot);

	cac_cfg->crt_thr_cfg.crt_color_thr_l_line = GET_LNR_INTRPL(
			min->crt_color_thr_l_line, max->crt_color_thr_l_line,
			ni->min, ni->max, cur_ni);
	dbg_hw(2, "[CAC] crt_thr.crt_color_thr_l_line: set_val %d range %d,%d\n",
			cac_cfg->crt_thr_cfg.crt_color_thr_l_line,
			min->crt_color_thr_l_line, max->crt_color_thr_l_line);

	cac_cfg->crt_thr_cfg.crt_color_thr_h = GET_LNR_INTRPL(
			min->crt_color_thr_h, max->crt_color_thr_h,
			ni->min, ni->max, cur_ni);
	dbg_hw(2, "[CAC] crt_thr.crt_color_thr_h: set_val %d range %d,%d\n",
			cac_cfg->crt_thr_cfg.crt_color_thr_h,
			min->crt_color_thr_h, max->crt_color_thr_h);
#endif
}

void hw_mcsc_calc_cac_param_by_ni(struct is_hw_ip *hw_ip,
	struct cac_setfile_contents *cac, u32 cur_ni)
{
	struct ref_ni ni_range, ni_idx, ni;
	struct cac_cfg_by_ni cac_cfg, cfg_max, cfg_min;

	ni_range.min = MULTIPLIED_10(cac->ni_vals[0]);
	ni_range.max = MULTIPLIED_10(cac->ni_vals[cac->ni_max - 1]);

	if (cur_ni <= ni_range.min) {
		sdbg_hw(2, "[CAC] cur_ni(%d) <= ni_range.min(%d)\n", hw_ip, cur_ni, ni_range.min);
		ni_idx.min = 0;
		ni_idx.max = 0;
	} else if (cur_ni >= ni_range.max) {
		sdbg_hw(2, "[CAC] cur_ni(%d) >= ni_range.max(%d)\n", hw_ip, cur_ni, ni_range.max);
		ni_idx.min = cac->ni_max - 1;
		ni_idx.max = cac->ni_max - 1;
	} else {
		ni_idx = hw_mcsc_find_ni_idx_for_cac(hw_ip, cac, cur_ni);
	}

	cfg_min = cac->cfgs[ni_idx.min];
	cfg_max = cac->cfgs[ni_idx.max];
	ni.min = MULTIPLIED_10(cac->ni_vals[ni_idx.min]);
	ni.max = MULTIPLIED_10(cac->ni_vals[ni_idx.max]);

	hw_mcsc_calc_cac_map_thr(&cac_cfg, &cfg_min, &cfg_max, &ni, cur_ni);
	hw_mcsc_calc_cac_crt_thr(&cac_cfg, &cfg_min, &cfg_max, &ni, cur_ni);

	is_scaler_set_cac_map_crt_thr(hw_ip->regs[REG_SETA], &cac_cfg);
}

int is_hw_mcsc_update_cac_register(struct is_hw_ip *hw_ip,
	struct is_frame *frame, u32 instance)
{
	int ret = 0;
	struct is_hw_mcsc *hw_mcsc;
	struct is_hw_mcsc_cap *cap;
	struct hw_mcsc_setfile *setfile;
	struct cac_setfile_contents *cac;
	enum exynos_sensor_position sensor_position;
	u32 ni, core_num, ni_index;

	FIMC_BUG(!hw_ip->priv_info);

	hw_mcsc = (struct is_hw_mcsc *)hw_ip->priv_info;
	cap = GET_MCSC_HW_CAP(hw_ip);

	if (cap->cac != MCSC_CAP_SUPPORT)
		return ret;

	/* Only EVT0 */
	if (hw_mcsc->cac_in == DEV_HW_MCSC0)
		return ret;

	sensor_position = hw_ip->hardware->sensor_position[instance];
	setfile = hw_mcsc->cur_setfile[sensor_position][instance];
	cac = &setfile->cac;

	/* calculate cac parameters */
	core_num = GET_CORE_NUM(hw_ip->id);
	ni_index = frame->fcount % NI_BACKUP_MAX;
#ifdef FIXED_TDNR_NOISE_INDEX
	ni = FIXED_TDNR_NOISE_INDEX_VALUE;
#else
	ni = hw_ip->hardware->ni_udm[core_num][ni_index].currentFrameNoiseIndex;
#endif
	if (hw_mcsc->cur_ni[SUBBLK_CAC] == ni)
		goto exit;

	hw_mcsc_calc_cac_param_by_ni(hw_ip, cac, ni);
	msdbg_hw(2, "[CAC][F:%d]: ni(%d)\n",
		instance, hw_ip, frame->fcount, ni);

exit:
	is_scaler_set_cac_enable(hw_ip->regs[REG_SETA], cac->cac_en);
	hw_mcsc->cur_ni[SUBBLK_CAC] = ni;

	return 0;
}

int is_hw_mcsc_recovery_cac_register(struct is_hw_ip *hw_ip,
		struct is_param_region *param, u32 instance)
{
	int ret = 0;
	/* TODO */

	return ret;
}
