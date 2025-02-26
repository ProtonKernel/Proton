// SPDX-License-Identifier: GPL-2.0
/*
 * Samsung Exynos SoC series Pablo driver
 *
 * Exynos Pablo image subsystem functions
 *
 * Copyright (c) 2020 Samsung Electronics Co., Ltd
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#include "is-hw-lme.h"
#include "is-err.h"
#define DDK_LIB_CALL
extern struct is_lib_support gPtr_lib_support;

#define SWAP(x, y, T) do { T SWAP = x; x = y; y = SWAP; } while (0)

#ifndef DDK_LIB_CALL
IS_TIMER_FUNC(is_hw_frame_start_timer)
{
	struct is_hw_ip *hw_ip = from_timer(hw_ip, (struct timer_list *)data, lme_frame_start_timer);
	struct is_hardware *hardware;
	u32 instance, hw_fcount;
	hw_fcount = atomic_read(&hw_ip->fcount);
	instance = atomic_read(&hw_ip->instance);
	hardware = hw_ip->hardware;

	msdbg_hw(2, "lme frame start timer", instance, hw_ip);
	atomic_add(1, &hw_ip->count.fs);
	if (!atomic_read(&hardware->streaming[hardware->sensor_position[instance]]))
		msinfo_hw("[F:%d]F.S\n", instance, hw_ip, hw_fcount);

	is_hardware_frame_start(hw_ip, instance);

	mod_timer(&hw_ip->lme_frame_end_timer, jiffies + msecs_to_jiffies(0));
}

IS_TIMER_FUNC(is_hw_frame_end_timer)
{
	struct is_hw_ip *hw_ip = from_timer(hw_ip, (struct timer_list *)data, lme_frame_end_timer);
	struct is_hardware *hardware;
	u32 instance, hw_fcount;

	hw_fcount = atomic_read(&hw_ip->fcount);
	instance = atomic_read(&hw_ip->instance);
	hardware = hw_ip->hardware;

	msdbg_hw(2, "lme frame end timer", instance, hw_ip);
	atomic_add(hw_ip->num_buffers, &hw_ip->count.fe);
	is_hardware_frame_done(hw_ip, NULL, -1, 0,
					IS_SHOT_SUCCESS, true);

	if (!atomic_read(&hardware->streaming[hardware->sensor_position[instance]]))
		msinfo_hw("[F:%d]F.E\n", instance, hw_ip, hw_fcount);

	atomic_set(&hw_ip->status.Vvalid, V_BLANK);
	if (atomic_read(&hw_ip->count.fs) < atomic_read(&hw_ip->count.fe)) {
		mserr_hw("fs(%d), fe(%d), dma(%d)", instance, hw_ip,
			atomic_read(&hw_ip->count.fs),
			atomic_read(&hw_ip->count.fe),
			atomic_read(&hw_ip->count.dma));
	}

	wake_up(&hw_ip->status.wait_queue);
}
#endif

static int __nocfi is_hw_lme_open(struct is_hw_ip *hw_ip, u32 instance,
	struct is_group *group)
{
	int ret = 0;
	struct is_hw_lme *hw_lme = NULL;

	FIMC_BUG(!hw_ip);

	if (test_bit(HW_OPEN, &hw_ip->state))
		return 0;

	frame_manager_probe(hw_ip->framemgr, hw_ip->id, "HWLME");
	frame_manager_open(hw_ip->framemgr, IS_MAX_HW_FRAME);

	hw_ip->priv_info = vzalloc(sizeof(struct is_hw_lme));
	if (!hw_ip->priv_info) {
		mserr_hw("hw_ip->priv_info(null)", instance, hw_ip);
		ret = -ENOMEM;
		goto err_alloc;
	}

#ifdef USE_ORBMCH_WA
	is_hw_lme_hw_bug_wa(hw_ip->id, true);
#endif

	hw_lme = (struct is_hw_lme *)hw_ip->priv_info;
#ifdef DDK_LIB_CALL
	is_fpsimd_get_func();
	ret = get_lib_func(LIB_FUNC_LME, (void **)&hw_lme->lib_func);
	is_fpsimd_put_func();

	if (hw_lme->lib_func == NULL) {
		mserr_hw("hw_lme->lib_func(null)", instance, hw_ip);
		is_load_clear();
		ret = -EINVAL;
		goto err_lib_func;
	}
	msinfo_hw("get_lib_func is set\n", instance, hw_ip);

	hw_lme->lib_support = &gPtr_lib_support;
	hw_lme->lib[instance].func = hw_lme->lib_func;

	ret = is_lib_isp_chain_create(hw_ip, &hw_lme->lib[instance], instance);
	if (ret) {
		mserr_hw("chain create fail", instance, hw_ip);
		ret = -EINVAL;
		goto err_chain_create;
	}
#endif
	set_bit(HW_OPEN, &hw_ip->state);
	msdbg_hw(2, "open: [G:0x%x], framemgr[%s]", instance, hw_ip,
		GROUP_ID(group->id), hw_ip->framemgr->name);

	return 0;
#ifdef DDK_LIB_CALL
err_chain_create:
err_lib_func:
	vfree(hw_ip->priv_info);
	hw_ip->priv_info = NULL;
#endif
err_alloc:
	frame_manager_close(hw_ip->framemgr);
	return ret;
}

static int is_hw_lme_init(struct is_hw_ip *hw_ip, u32 instance,
	struct is_group *group, bool flag, u32 module_id)
{
	int ret = 0;
	struct is_hw_lme *hw_lme = NULL;

	FIMC_BUG(!hw_ip);
	FIMC_BUG(!hw_ip->priv_info);
	FIMC_BUG(!group);

	hw_lme = (struct is_hw_lme *)hw_ip->priv_info;

#ifdef DDK_LIB_CALL
	hw_lme->lib[instance].object = NULL;
	hw_lme->lib[instance].func   = hw_lme->lib_func;

	if (hw_lme->lib[instance].object != NULL) {
		msdbg_hw(2, "object is already created\n", instance, hw_ip);
	} else {
		ret = is_lib_isp_object_create(hw_ip, &hw_lme->lib[instance],
				instance, (u32)flag, module_id);
		if (ret) {
			mserr_hw("object create fail", instance, hw_ip);
			return -EINVAL;
		}
	}
#else
	timer_setup(&hw_ip->lme_frame_start_timer, (void (*)(struct timer_list *))is_hw_frame_start_timer, 0);
	timer_setup(&hw_ip->lme_frame_end_timer, (void (*)(struct timer_list *))is_hw_frame_end_timer, 0);
#endif

	set_bit(HW_INIT, &hw_ip->state);
	return ret;
}

static int is_hw_lme_deinit(struct is_hw_ip *hw_ip, u32 instance)
{
	int ret = 0;
	struct is_hw_lme *hw_lme;

	FIMC_BUG(!hw_ip);
	FIMC_BUG(!hw_ip->priv_info);

	hw_lme = (struct is_hw_lme *)hw_ip->priv_info;
#ifdef DDK_LIB_CALL
	is_lib_isp_object_destroy(hw_ip, &hw_lme->lib[instance], instance);
	hw_lme->lib[instance].object = NULL;
#endif
	return ret;
}

static int is_hw_lme_close(struct is_hw_ip *hw_ip, u32 instance)
{
	int ret = 0;
	struct is_hw_lme *hw_lme;

	FIMC_BUG(!hw_ip);

	if (!test_bit(HW_OPEN, &hw_ip->state))
		return 0;

	FIMC_BUG(!hw_ip->priv_info);
	hw_lme = (struct is_hw_lme *)hw_ip->priv_info;
#ifdef DDK_LIB_CALL
	FIMC_BUG(!hw_lme->lib_support);

	is_lib_isp_chain_destroy(hw_ip, &hw_lme->lib[instance], instance);
#else
	del_timer(&hw_ip->lme_frame_start_timer);
	del_timer(&hw_ip->lme_frame_end_timer);
#endif

#ifdef USE_ORBMCH_WA
	is_hw_lme_hw_bug_wa(hw_ip->id, false);
#endif

	vfree(hw_ip->priv_info);
	hw_ip->priv_info = NULL;
	frame_manager_close(hw_ip->framemgr);

	clear_bit(HW_OPEN, &hw_ip->state);

	return ret;
}

static int is_hw_lme_enable(struct is_hw_ip *hw_ip, u32 instance, ulong hw_map)
{
	int ret = 0;

	FIMC_BUG(!hw_ip);

	if (!test_bit_variables(hw_ip->id, &hw_map))
		return 0;

	if (!test_bit(HW_INIT, &hw_ip->state)) {
		mserr_hw("not initialized!!", instance, hw_ip);
		return -EINVAL;
	}

	atomic_inc(&hw_ip->run_rsccount);
	set_bit(HW_RUN, &hw_ip->state);

	return ret;
}

static int is_hw_lme_disable(struct is_hw_ip *hw_ip, u32 instance, ulong hw_map)
{
	int ret = 0;
	long timetowait;
	struct is_hw_lme *hw_lme;
	struct lme_param_set *param_set;

	FIMC_BUG(!hw_ip);

	if (!test_bit_variables(hw_ip->id, &hw_map))
		return 0;

	if (!test_bit(HW_INIT, &hw_ip->state)) {
		mserr_hw("not initialized!!", instance, hw_ip);
		return -EINVAL;
	}

	msinfo_hw("lme_disable: Vvalid(%d)\n", instance, hw_ip,
		atomic_read(&hw_ip->status.Vvalid));

	FIMC_BUG(!hw_ip->priv_info);
	hw_lme = (struct is_hw_lme *)hw_ip->priv_info;
	param_set = &hw_lme->param_set[instance];

	timetowait = wait_event_timeout(hw_ip->status.wait_queue,
		!atomic_read(&hw_ip->status.Vvalid),
		IS_HW_STOP_TIMEOUT);

	if (!timetowait) {
		mserr_hw("wait FRAME_END timeout (%ld)", instance,
			hw_ip, timetowait);
		ret = -ETIME;
	}

	param_set->fcount = 0;
	if (test_bit(HW_RUN, &hw_ip->state)) {
		/* TODO: need to kthread_flush when lme use task */
#ifdef DDK_LIB_CALL
		is_lib_isp_stop(hw_ip, &hw_lme->lib[instance], instance);
#endif
	} else {
		msdbg_hw(2, "already disabled\n", instance, hw_ip);
	}

	if (atomic_dec_return(&hw_ip->run_rsccount) > 0)
		return 0;

	clear_bit(HW_RUN, &hw_ip->state);
	clear_bit(HW_CONFIG, &hw_ip->state);

	return ret;
}

static void is_hw_lme_update_param(struct is_hw_ip *hw_ip, struct is_region *region,
	struct lme_param_set *param_set, u32 lindex, u32 hindex, u32 instance)
{
	struct lme_param *param;

	FIMC_BUG_VOID(!region);
	FIMC_BUG_VOID(!param_set);

	param = &region->parameter.lme;
	param_set->instance_id = instance;

	/* check input */
	if ((lindex & LOWBIT_OF(PARAM_LME_DMA))
		|| (hindex & HIGHBIT_OF(PARAM_LME_DMA))) {
		memcpy(&param_set->dma, &param->dma,
			sizeof(struct param_lme_dma));
	}

}

static int is_hw_lme_shot(struct is_hw_ip *hw_ip, struct is_frame *frame,
	ulong hw_map)
{
	int ret = 0;
	int cur_idx, batch_num;
	struct is_device_ischain *device;
	struct is_hw_lme *hw_lme;
	struct lme_param_set *param_set;
	struct is_region *region;
	struct lme_param *param;
	u32 lindex, hindex, fcount, instance;
	bool frame_done = false;
	bool is_reprocessing;
	u32 cmd_cur_input, cmd_prev_input, cmd_output;

	FIMC_BUG(!hw_ip);
	FIMC_BUG(!frame);

	instance = frame->instance;
	msdbgs_hw(2, "[F:%d]shot\n", instance, hw_ip, frame->fcount);

	if (!test_bit_variables(hw_ip->id, &hw_map))
		return 0;

	if (!test_bit(HW_INIT, &hw_ip->state)) {
		mserr_hw("not initialized!!", instance, hw_ip);
		return -EINVAL;
	}

	is_hw_g_ctrl(hw_ip, hw_ip->id, HW_G_CTRL_FRM_DONE_WITH_DMA, (void *)&frame_done);
	if ((!frame_done)
		|| (!test_bit(ENTRY_LME, &frame->out_flag) && !test_bit(ENTRY_LMEC, &frame->out_flag)
			&& !test_bit(ENTRY_LMES, &frame->out_flag)))
		set_bit(hw_ip->id, &frame->core_flag);


	FIMC_BUG(!hw_ip->priv_info);
	hw_lme = (struct is_hw_lme *)hw_ip->priv_info;
	param_set = &hw_lme->param_set[instance];
	region = hw_ip->region[instance];
	FIMC_BUG(!region);

	param = &region->parameter.lme;
	cur_idx = frame->cur_buf_index;
	fcount = frame->fcount;
	device = hw_ip->group[instance]->device;
#ifdef USE_ORBMCH_FOR_ME
	is_reprocessing = false;
#else
	is_reprocessing = test_bit(IS_ISCHAIN_REPROCESSING, &device->state);
#endif
	if (frame->type == SHOT_TYPE_INTERNAL) {
		cmd_cur_input = param_set->dma.cur_input_cmd;
		param_set->dma.cur_input_cmd = DMA_INPUT_COMMAND_DISABLE;
		param_set->cur_input_dva[0] = 0x0;

		cmd_prev_input = param_set->dma.prev_input_cmd;
		param_set->dma.prev_input_cmd = DMA_INPUT_COMMAND_DISABLE;
		param_set->prev_input_dva[0] = 0x0;

		cmd_output = param_set->dma.output_cmd;
		param_set->dma.output_cmd = DMA_OUTPUT_COMMAND_DISABLE;
		param_set->output_dva[0] = 0x0;

		param_set->tnr_mode = TNR_PROCESSING_PREVIEW_POST_ON;
		hw_ip->internal_fcount[instance] = fcount;
		goto config;
	} else {
		FIMC_BUG(!frame->shot);
		/* per-frame control
		 * check & update size from region
		 */

		lindex = frame->shot->ctl.vendor_entry.lowIndexParam;
		hindex = frame->shot->ctl.vendor_entry.highIndexParam;

		if (hw_ip->internal_fcount[instance] != 0) {
			hw_ip->internal_fcount[instance] = 0;
			param_set->dma.output_cmd = param->dma.output_cmd;
		}

		/*set TNR operation mode */
		if (frame->shot_ext) {
			/*
			if ((frame->shot_ext->tnr_mode >= TNR_PROCESSING_CAPTURE_FIRST) &&
					!CHK_VIDEOHDR_MODE_CHANGE(param_set->tnr_mode, frame->shot_ext->tnr_mode))
				msinfo_hw("[F:%d] TNR mode is changed (%d -> %d)\n",
					instance, hw_ip, frame->fcount,
					param_set->tnr_mode, frame->shot_ext->tnr_mode);
			*/
			param_set->tnr_mode = frame->shot_ext->tnr_mode;
		} else {
			mswarn_hw("frame->shot_ext is null", instance, hw_ip);
			param_set->tnr_mode = TNR_PROCESSING_PREVIEW_POST_ON;
		}
	}

	is_hw_lme_update_param(hw_ip, region, param_set, lindex, hindex, instance);

	/* DMA settings */
	if (is_reprocessing) {
		SWAP(param_set->dma.cur_input_cmd, param_set->dma.prev_input_cmd, u32);
		SWAP(param_set->dma.cur_input_width, param_set->dma.prev_input_width, u32);
		SWAP(param_set->dma.cur_input_height, param_set->dma.prev_input_height, u32);

		cmd_cur_input = is_hardware_dma_cfg_32bit("lems", hw_ip,
				frame, cur_idx,
				&param_set->dma.cur_input_cmd,
				param_set->dma.input_plane,
				param_set->cur_input_dva,
				frame->lmesTargetAddress);

		cmd_prev_input = is_hardware_dma_cfg("lmes_prev", hw_ip,
				frame, cur_idx,
				&param_set->dma.prev_input_cmd,
				param_set->dma.input_plane,
				param_set->prev_input_dva,
				frame->dvaddr_buffer);
	} else {
		cmd_cur_input = is_hardware_dma_cfg("lems", hw_ip,
				frame, cur_idx,
				&param_set->dma.cur_input_cmd,
				param_set->dma.input_plane,
				param_set->cur_input_dva,
				frame->dvaddr_buffer);

		cmd_prev_input = is_hardware_dma_cfg_32bit("lmes_prev", hw_ip,
				frame, cur_idx,
				&param_set->dma.prev_input_cmd,
				param_set->dma.input_plane,
				param_set->prev_input_dva,
				frame->lmesTargetAddress);
	}

	cmd_output = is_hardware_dma_cfg_32bit("lmec", hw_ip,
			frame, cur_idx,
			&param_set->dma.output_cmd,
			param_set->dma.output_plane,
			param_set->output_dva,
			frame->lmecTargetAddress);

	is_hardware_dma_cfg_64bit("lmec", hw_ip,
			frame, cur_idx,
			&param_set->dma.output_cmd,
			param_set->dma.output_plane,
			param_set->output_kva_motion_pure,
			frame->lmecKTargetAddress);

	is_hardware_dma_cfg_64bit("lmem", hw_ip,
			frame, cur_idx,
			&param_set->dma.output_cmd,
			param_set->dma.output_plane,
			param_set->output_kva_motion_processed,
			frame->lmemKTargetAddress);

config:
	param_set->instance_id = instance;
	param_set->fcount = fcount;

	/* multi-buffer */
	hw_ip->num_buffers = frame->num_buffers;
	batch_num = hw_ip->framemgr->batch_num;
	/* Update only for SW_FRO scenario */
	if (batch_num > 1 && hw_ip->num_buffers == 1) {
		hw_ip->num_buffers |= batch_num << SW_FRO_NUM_SHIFT;
		hw_ip->num_buffers |= cur_idx << CURR_INDEX_SHIFT;
	}

	if (frame->type == SHOT_TYPE_INTERNAL) {
		is_log_write("[@][DRV][%d]lme_shot [T:%d][F:%d][IN:0x%x] [%d][OUT:0x%x]\n",
			param_set->instance_id, frame->type,
			param_set->fcount, param_set->cur_input_dva[0],
			param_set->dma.output_cmd, param_set->output_dva[0]);
	}
#ifdef DDK_LIB_CALL
	if (frame->shot) {
		ret = is_lib_isp_set_ctrl(hw_ip, &hw_lme->lib[instance], frame);
		if (ret)
			mserr_hw("set_ctrl fail", instance, hw_ip);
	}

	ret = is_lib_isp_shot(hw_ip, &hw_lme->lib[instance], param_set, frame->shot);

	param_set->dma.cur_input_cmd = cmd_cur_input;
	param_set->dma.prev_input_cmd = cmd_prev_input;
	param_set->dma.output_cmd = cmd_output;

	set_bit(HW_CONFIG, &hw_ip->state);
#else
	mod_timer(&hw_ip->lme_frame_start_timer, jiffies + msecs_to_jiffies(0));
#endif
	return ret;
}

static int is_hw_lme_set_param(struct is_hw_ip *hw_ip, struct is_region *region,
	u32 lindex, u32 hindex, u32 instance, ulong hw_map)
{
	int ret = 0;
	struct is_hw_lme *hw_lme;
	struct lme_param_set *param_set;

	FIMC_BUG(!hw_ip);

	if (!test_bit_variables(hw_ip->id, &hw_map))
		return 0;

	if (!test_bit(HW_INIT, &hw_ip->state)) {
		mserr_hw("not initialized!!", instance, hw_ip);
		return -EINVAL;
	}

	FIMC_BUG(!hw_ip->priv_info);
	hw_lme = (struct is_hw_lme *)hw_ip->priv_info;
	param_set = &hw_lme->param_set[instance];

	hw_ip->region[instance] = region;
	hw_ip->lindex[instance] = lindex;
	hw_ip->hindex[instance] = hindex;

	is_hw_lme_update_param(hw_ip, region, param_set, lindex, hindex, instance);
#ifdef DDK_LIB_CALL
	ret = is_lib_isp_set_param(hw_ip, &hw_lme->lib[instance], param_set);
	if (ret)
		mserr_hw("set_param fail", instance, hw_ip);
#endif
	return ret;
}

static int is_hw_lme_get_meta(struct is_hw_ip *hw_ip, struct is_frame *frame,
	ulong hw_map)
{
	int ret = 0;
	struct is_hw_lme *hw_lme;

	FIMC_BUG(!hw_ip);
	FIMC_BUG(!frame);

	if (!test_bit_variables(hw_ip->id, &hw_map))
		return 0;

	FIMC_BUG(!hw_ip->priv_info);
	hw_lme = (struct is_hw_lme *)hw_ip->priv_info;
#ifdef DDK_LIB_CALL
	ret = is_lib_isp_get_meta(hw_ip, &hw_lme->lib[frame->instance], frame);
	if (ret)
		mserr_hw("get_meta fail", frame->instance, hw_ip);
#endif
	return ret;
}

static int is_hw_lme_frame_ndone(struct is_hw_ip *hw_ip, struct is_frame *frame,
	u32 instance, enum ShotErrorType done_type)
{
	int ret = 0;
	int output_id = 0;

	FIMC_BUG(!hw_ip);
	FIMC_BUG(!frame);

	if (test_bit(hw_ip->id, &frame->core_flag))
		output_id = IS_HW_CORE_END;

	ret = is_hardware_frame_done(hw_ip, frame, -1,
			output_id, done_type, false);

	return ret;
}

static int is_hw_lme_load_setfile(struct is_hw_ip *hw_ip, u32 instance, ulong hw_map)
{
	int flag = 0, ret = 0;
	struct is_hw_lme *hw_lme = NULL;
	struct is_hw_ip_setfile *setfile;
	enum exynos_sensor_position sensor_position;

	FIMC_BUG(!hw_ip);

	if (!test_bit_variables(hw_ip->id, &hw_map)) {
		msdbg_hw(2, "%s: hw_map(0x%lx)\n", instance, hw_ip, __func__, hw_map);
		return 0;
	}

	if (!test_bit(HW_INIT, &hw_ip->state)) {
		mserr_hw("not initialized!!", instance, hw_ip);
		return -ESRCH;
	}

	sensor_position = hw_ip->hardware->sensor_position[instance];
	setfile = &hw_ip->setfile[sensor_position];

	switch (setfile->version) {
	case SETFILE_V2:
		flag = false;
		break;
	case SETFILE_V3:
		flag = true;
		break;
	default:
		mserr_hw("invalid version (%d)", instance, hw_ip,
			setfile->version);
		return -EINVAL;
	}

	FIMC_BUG(!hw_ip->priv_info);
	hw_lme = (struct is_hw_lme *)hw_ip->priv_info;
	/*
	for (index = 0; index < setfile->using_count; index++) {
		addr = setfile->table[index].addr;
		size = setfile->table[index].size;
		ret = is_lib_isp_create_tune_set(&hw_lme->lib[instance],
			addr, size, index, flag, instance);

		set_bit(index, &hw_lme->lib[instance].tune_count);
	}
	*/
	set_bit(HW_TUNESET, &hw_ip->state);

	return ret;
}

static int is_hw_lme_apply_setfile(struct is_hw_ip *hw_ip, u32 scenario,
	u32 instance, ulong hw_map)
{
	int ret = 0;

	FIMC_BUG(!hw_ip);

	if (!test_bit_variables(hw_ip->id, &hw_map)) {
		msdbg_hw(2, "%s: hw_map(0x%lx)\n", instance, hw_ip, __func__, hw_map);
		return 0;
	}

	if (!test_bit(HW_INIT, &hw_ip->state)) {
		mserr_hw("not initialized!!", instance, hw_ip);
		return -ESRCH;
	}
	/*
	sensor_position = hw_ip->hardware->sensor_position[instance];
	setfile = &hw_ip->setfile[sensor_position];

	if (setfile->using_count == 0)
		return 0;

	setfile_index = setfile->index[scenario];
	if (setfile_index >= setfile->using_count) {
		mserr_hw("setfile index is out-of-range, [%d:%d]",
				instance, hw_ip, scenario, setfile_index);
		return -EINVAL;
	}

	msinfo_hw("setfile (%d) scenario (%d)\n", instance, hw_ip,
		setfile_index, scenario);

	FIMC_BUG(!hw_ip->priv_info);
	hw_lme = (struct is_hw_lme *)hw_ip->priv_info;

	ret = is_lib_isp_apply_tune_set(&hw_lme->lib[instance], setfile_index, instance);
	*/

	return ret;
}

static int is_hw_lme_delete_setfile(struct is_hw_ip *hw_ip, u32 instance, ulong hw_map)
{
	int ret = 0;

	FIMC_BUG(!hw_ip);

	if (!test_bit_variables(hw_ip->id, &hw_map)) {
		msdbg_hw(2, "%s: hw_map(0x%lx)\n", instance, hw_ip, __func__, hw_map);
		return 0;
	}

	if (!test_bit(HW_INIT, &hw_ip->state)) {
		msdbg_hw(2, "Not initialized\n", instance, hw_ip);
		return 0;
	}
	/*
	sensor_position = hw_ip->hardware->sensor_position[instance];
	setfile = &hw_ip->setfile[sensor_position];

	if (setfile->using_count == 0)
		return 0;

	FIMC_BUG(!hw_ip->priv_info);
	hw_lme = (struct is_hw_lme *)hw_ip->priv_info;

	for (i = 0; i < setfile->using_count; i++) {
		if (test_bit(i, &hw_lme->lib[instance].tune_count)) {
			ret = is_lib_isp_delete_tune_set(&hw_lme->lib[instance],
				(u32)i, instance);
			clear_bit(i, &hw_lme->lib[instance].tune_count);
		}
	}
	*/
	clear_bit(HW_TUNESET, &hw_ip->state);

	return ret;
}

int is_hw_lme_restore(struct is_hw_ip *hw_ip, u32 instance)
{
	int ret = 0;
	struct is_hw_lme *hw_lme = NULL;

	BUG_ON(!hw_ip);
	BUG_ON(!hw_ip->priv_info);

	if (!test_bit(HW_OPEN, &hw_ip->state))
		return -EINVAL;

	hw_lme = (struct is_hw_lme *)hw_ip->priv_info;
#ifdef DDK_LIB_CALL
	ret = is_lib_isp_reset_recovery(hw_ip, &hw_lme->lib[instance], instance);
	if (ret) {
		mserr_hw("is_lib_lme_reset_recovery fail ret(%d)",
				instance, hw_ip, ret);
	}
#endif
	return ret;
}

static int is_hw_lme_notify_timeout(struct is_hw_ip *hw_ip, u32 instance)
{
	int ret = 0;
	struct is_hw_lme *hw_lme = NULL;

	hw_lme = (struct is_hw_lme *)hw_ip->priv_info;
	if (!hw_lme) {
		mserr_hw("failed to get HW LME", instance, hw_ip);
		return -ENODEV;
	}

	ret = is_lib_notify_timeout(&hw_lme->lib[instance], instance);

	return ret;
}

const struct is_hw_ip_ops is_hw_lme_ops = {
	.open			= is_hw_lme_open,
	.init			= is_hw_lme_init,
	.deinit			= is_hw_lme_deinit,
	.close			= is_hw_lme_close,
	.enable			= is_hw_lme_enable,
	.disable		= is_hw_lme_disable,
	.shot			= is_hw_lme_shot,
	.set_param		= is_hw_lme_set_param,
	.get_meta		= is_hw_lme_get_meta,
	.frame_ndone		= is_hw_lme_frame_ndone,
	.load_setfile		= is_hw_lme_load_setfile,
	.apply_setfile		= is_hw_lme_apply_setfile,
	.delete_setfile		= is_hw_lme_delete_setfile,
	.restore		= is_hw_lme_restore,
	.notify_timeout		= is_hw_lme_notify_timeout,
};

int is_hw_lme_probe(struct is_hw_ip *hw_ip, struct is_interface *itf,
	struct is_interface_ischain *itfc, int id, const char *name)
{
	int ret = 0;

	FIMC_BUG(!hw_ip);
	FIMC_BUG(!itf);
	FIMC_BUG(!itfc);

	/* initialize device hardware */
	hw_ip->id   = id;
	snprintf(hw_ip->name, sizeof(hw_ip->name), "%s", name);
	hw_ip->ops  = &is_hw_lme_ops;
	hw_ip->itf  = itf;
	hw_ip->itfc = itfc;
	atomic_set(&hw_ip->fcount, 0);
	hw_ip->is_leader = true;
	atomic_set(&hw_ip->status.Vvalid, V_BLANK);
	atomic_set(&hw_ip->rsccount, 0);
	atomic_set(&hw_ip->run_rsccount, 0);
	init_waitqueue_head(&hw_ip->status.wait_queue);

	clear_bit(HW_OPEN, &hw_ip->state);
	clear_bit(HW_INIT, &hw_ip->state);
	clear_bit(HW_CONFIG, &hw_ip->state);
	clear_bit(HW_RUN, &hw_ip->state);
	clear_bit(HW_TUNESET, &hw_ip->state);

	sinfo_hw("probe done\n", hw_ip);

	return ret;
}
