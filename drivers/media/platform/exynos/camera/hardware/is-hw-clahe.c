// SPDX-License-Identifier: GPL-2.0
/*
 * Samsung Exynos SoC series Pablo driver
 *
 * Exynos Pablo image subsystem functions
 *
 * Copyright (c) 2019 Samsung Electronics Co., Ltd
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#include "is-hw-clahe.h"
#include "is-err.h"

extern struct is_lib_support gPtr_lib_support;

static int __nocfi is_hw_clh_open(struct is_hw_ip *hw_ip, u32 instance,
	struct is_group *group)
{
	int ret = 0;
	struct is_hw_clh *hw_clh = NULL;

	FIMC_BUG(!hw_ip);

	if (test_bit(HW_OPEN, &hw_ip->state))
		return 0;

	frame_manager_probe(hw_ip->framemgr, hw_ip->id, "HWCLH");
	frame_manager_open(hw_ip->framemgr, IS_MAX_HW_FRAME);

	hw_ip->priv_info = vzalloc(sizeof(struct is_hw_clh));
	if (!hw_ip->priv_info) {
		mserr_hw("hw_ip->priv_info(null)", instance, hw_ip);
		ret = -ENOMEM;
		goto err_alloc;
	}

	hw_clh = (struct is_hw_clh *)hw_ip->priv_info;

	is_fpsimd_get_func();
	ret = get_lib_func(LIB_FUNC_CLAHE, (void **)&hw_clh->lib_func);
	is_fpsimd_put_func();

	if (hw_clh->lib_func == NULL) {
		mserr_hw("hw_clh->lib_func(null)", instance, hw_ip);
		is_load_clear();
		ret = -EINVAL;
		goto err_lib_func;
	}
	msinfo_hw("get_lib_func is set\n", instance, hw_ip);

	hw_clh->lib_support = &gPtr_lib_support;
	hw_clh->lib[instance].func = hw_clh->lib_func;

	ret = is_lib_isp_chain_create(hw_ip, &hw_clh->lib[instance], instance);
	if (ret) {
		mserr_hw("chain create fail", instance, hw_ip);
		ret = -EINVAL;
		goto err_chain_create;
	}

	set_bit(HW_OPEN, &hw_ip->state);
	msdbg_hw(2, "open: [G:0x%x], framemgr[%s]", instance, hw_ip,
		GROUP_ID(group->id), hw_ip->framemgr->name);

	return 0;

err_chain_create:
err_lib_func:
	vfree(hw_ip->priv_info);
	hw_ip->priv_info = NULL;
err_alloc:
	frame_manager_close(hw_ip->framemgr);
	return ret;
}

static int is_hw_clh_init(struct is_hw_ip *hw_ip, u32 instance,
	struct is_group *group, bool flag, u32 module_id)
{
	int ret = 0;
	struct is_hw_clh *hw_clh = NULL;

	FIMC_BUG(!hw_ip);
	FIMC_BUG(!hw_ip->priv_info);
	FIMC_BUG(!group);

	hw_clh = (struct is_hw_clh *)hw_ip->priv_info;

	hw_clh->lib[instance].object = NULL;
	hw_clh->lib[instance].func   = hw_clh->lib_func;

	if (hw_clh->lib[instance].object != NULL) {
		msdbg_hw(2, "object is already created\n", instance, hw_ip);
	} else {
		ret = is_lib_isp_object_create(hw_ip, &hw_clh->lib[instance],
				instance, (u32)flag, module_id);
		if (ret) {
			mserr_hw("object create fail", instance, hw_ip);
			return -EINVAL;
		}
	}

	set_bit(HW_INIT, &hw_ip->state);
	return ret;
}

static int is_hw_clh_deinit(struct is_hw_ip *hw_ip, u32 instance)
{
	int ret = 0;
	struct is_hw_clh *hw_clh;

	FIMC_BUG(!hw_ip);
	FIMC_BUG(!hw_ip->priv_info);

	hw_clh = (struct is_hw_clh *)hw_ip->priv_info;

	is_lib_isp_object_destroy(hw_ip, &hw_clh->lib[instance], instance);
	hw_clh->lib[instance].object = NULL;

	return ret;
}

static int is_hw_clh_close(struct is_hw_ip *hw_ip, u32 instance)
{
	int ret = 0;
	struct is_hw_clh *hw_clh;

	FIMC_BUG(!hw_ip);

	if (!test_bit(HW_OPEN, &hw_ip->state))
		return 0;

	FIMC_BUG(!hw_ip->priv_info);
	hw_clh = (struct is_hw_clh *)hw_ip->priv_info;
	FIMC_BUG(!hw_clh->lib_support);

	is_lib_isp_chain_destroy(hw_ip, &hw_clh->lib[instance], instance);
	vfree(hw_ip->priv_info);
	hw_ip->priv_info = NULL;
	frame_manager_close(hw_ip->framemgr);

	clear_bit(HW_OPEN, &hw_ip->state);

	return ret;
}

static int is_hw_clh_enable(struct is_hw_ip *hw_ip, u32 instance, ulong hw_map)
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

static int is_hw_clh_disable(struct is_hw_ip *hw_ip, u32 instance, ulong hw_map)
{
	int ret = 0;
	long timetowait;
	struct is_hw_clh *hw_clh;
	struct clh_param_set *param_set;

	FIMC_BUG(!hw_ip);

	if (!test_bit_variables(hw_ip->id, &hw_map))
		return 0;

	if (!test_bit(HW_INIT, &hw_ip->state)) {
		mserr_hw("not initialized!!", instance, hw_ip);
		return -EINVAL;
	}

	msinfo_hw("clh_disable: Vvalid(%d)\n", instance, hw_ip,
		atomic_read(&hw_ip->status.Vvalid));

	FIMC_BUG(!hw_ip->priv_info);
	hw_clh = (struct is_hw_clh *)hw_ip->priv_info;
	param_set = &hw_clh->param_set[instance];

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
		/* TODO: need to kthread_flush when clh use task */
		is_lib_isp_stop(hw_ip, &hw_clh->lib[instance], instance);
	} else {
		msdbg_hw(2, "already disabled\n", instance, hw_ip);
	}

	if (atomic_dec_return(&hw_ip->run_rsccount) > 0)
		return 0;

	clear_bit(HW_RUN, &hw_ip->state);
	clear_bit(HW_CONFIG, &hw_ip->state);

	return ret;
}

static void is_hw_clh_update_param(struct is_hw_ip *hw_ip, struct is_region *region,
	struct clh_param_set *param_set, u32 lindex, u32 hindex, u32 instance)
{
	struct clh_param *param;

	FIMC_BUG_VOID(!region);
	FIMC_BUG_VOID(!param_set);

	param = &region->parameter.clh;
	param_set->instance_id = instance;

	/* check input */
	if ((lindex & LOWBIT_OF(PARAM_CLH_DMA_INPUT))
		|| (hindex & HIGHBIT_OF(PARAM_CLH_DMA_INPUT))) {
		memcpy(&param_set->dma_input, &param->dma_input,
			sizeof(struct param_dma_input));
	}

	/* check output*/
	if ((lindex & LOWBIT_OF(PARAM_CLH_DMA_OUTPUT))
		|| (hindex & HIGHBIT_OF(PARAM_CLH_DMA_OUTPUT))) {
		memcpy(&param_set->dma_output, &param->dma_output,
			sizeof(struct param_dma_output));
	}

}

static int is_hw_clh_shot(struct is_hw_ip *hw_ip, struct is_frame *frame,
	ulong hw_map)
{
	int ret = 0;
	int i, cur_idx, batch_num;
	struct is_hw_clh *hw_clh;
	struct clh_param_set *param_set;
	struct is_region *region;
	struct clh_param *param;
	u32 lindex, hindex, fcount, instance;
	bool frame_done = false;
	u32 hw_plane = 0;

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
		|| (!test_bit(ENTRY_CLHC, &frame->out_flag)))
		set_bit(hw_ip->id, &frame->core_flag);

	FIMC_BUG(!hw_ip->priv_info);
	hw_clh = (struct is_hw_clh *)hw_ip->priv_info;
	param_set = &hw_clh->param_set[instance];
	region = hw_ip->region[instance];
	FIMC_BUG(!region);

	param = &region->parameter.clh;
	fcount = frame->fcount;
	cur_idx = frame->cur_buf_index;

	if (frame->type == SHOT_TYPE_INTERNAL) {
		param_set->dma_input.cmd = DMA_INPUT_COMMAND_DISABLE;
		param_set->input_dva[0] = 0x0;
		param_set->output_dva[0] = 0x0;
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
			param_set->dma_output.cmd = param->dma_output.cmd;
		}
	}

	is_hw_clh_update_param(hw_ip, region, param_set, lindex, hindex, instance);

	/* DMA settings */
	if (param_set->dma_input.cmd != DMA_INPUT_COMMAND_DISABLE) {
		hw_plane = param_set->dma_input.plane;
		/* TODO : need to loop (frame->num_buffers) for FRO */
		for (i = 0; i < hw_plane; i++) {
			param_set->input_dva[i] = frame->clxsTargetAddress[i + cur_idx];
			if (frame->clxsTargetAddress[i] == 0) {
				msinfo_hw("[F:%d]clxsTargetAddress[%d] is zero",
					instance, hw_ip, frame->fcount, i);
				FIMC_BUG(1);
			}

			/* TO DO : svhist_input_dva */
#if 0
			param_set->svhist_input_dva[i] = frame->clxcTargetAddress[i + cur_idx];
			if (frame->clxcTargetAddress[i] == 0) {
				msdbg_hw(2, "[F:%d]clxcTargetAddress[%d] is zero",
					instance, hw_ip, frame->fcount, i);
			}
#endif
		}
	}

	if (param_set->dma_output.cmd != DMA_OUTPUT_COMMAND_DISABLE) {
		hw_plane = param_set->dma_output.plane;
		/* TODO : need to loop (frame->num_buffers) for FRO */
		for (i = 0; i < hw_plane; i++) {
			param_set->output_dva[i] = frame->clxcTargetAddress[i + cur_idx];
			if (frame->clxcTargetAddress[i] == 0) {
				msinfo_hw("[F:%d]clxcTargetAddress[%d] is zero",
					instance, hw_ip, frame->fcount, i);
				param_set->dma_output.cmd = DMA_OUTPUT_COMMAND_DISABLE;
			}
		}
	}

config:
	param_set->instance_id = instance;
	param_set->fcount = fcount;

	/* multi-buffer */
	hw_ip->num_buffers = frame->num_buffers;
	batch_num = hw_ip->framemgr->batch_num;
	if (batch_num > 1) {
		hw_ip->num_buffers |= batch_num << SW_FRO_NUM_SHIFT;
		hw_ip->num_buffers |= cur_idx << CURR_INDEX_SHIFT;
	}

	if (frame->type == SHOT_TYPE_INTERNAL) {
		is_log_write("[@][DRV][%d]clh_shot [T:%d][F:%d][IN:0x%x] [%d][OUT:0x%x]\n",
			param_set->instance_id, frame->type,
			param_set->fcount, param_set->input_dva[0],
			param_set->dma_output.cmd, param_set->output_dva[0]);
	}

	if (frame->shot) {
		ret = is_lib_isp_set_ctrl(hw_ip, &hw_clh->lib[instance], frame);
		if (ret)
			mserr_hw("set_ctrl fail", instance, hw_ip);
	}

	ret = is_lib_isp_shot(hw_ip, &hw_clh->lib[instance], param_set, frame->shot);

	set_bit(HW_CONFIG, &hw_ip->state);

	return ret;
}

static int is_hw_clh_set_param(struct is_hw_ip *hw_ip, struct is_region *region,
	u32 lindex, u32 hindex, u32 instance, ulong hw_map)
{
	int ret = 0;
	struct is_hw_clh *hw_clh;
	struct clh_param_set *param_set;

	FIMC_BUG(!hw_ip);

	if (!test_bit_variables(hw_ip->id, &hw_map))
		return 0;

	if (!test_bit(HW_INIT, &hw_ip->state)) {
		mserr_hw("not initialized!!", instance, hw_ip);
		return -EINVAL;
	}

	FIMC_BUG(!hw_ip->priv_info);
	hw_clh = (struct is_hw_clh *)hw_ip->priv_info;
	param_set = &hw_clh->param_set[instance];

	hw_ip->region[instance] = region;
	hw_ip->lindex[instance] = lindex;
	hw_ip->hindex[instance] = hindex;

	is_hw_clh_update_param(hw_ip, region, param_set, lindex, hindex, instance);

	return ret;
}

static int is_hw_clh_get_meta(struct is_hw_ip *hw_ip, struct is_frame *frame,
	ulong hw_map)
{
	int ret = 0;
	struct is_hw_clh *hw_clh;

	FIMC_BUG(!hw_ip);
	FIMC_BUG(!frame);

	if (!test_bit_variables(hw_ip->id, &hw_map))
		return 0;

	FIMC_BUG(!hw_ip->priv_info);
	hw_clh = (struct is_hw_clh *)hw_ip->priv_info;

	ret = is_lib_isp_get_meta(hw_ip, &hw_clh->lib[frame->instance], frame);
	if (ret)
		mserr_hw("get_meta fail", frame->instance, hw_ip);

	return ret;
}

static int is_hw_clh_frame_ndone(struct is_hw_ip *hw_ip, struct is_frame *frame,
	u32 instance, enum ShotErrorType done_type)
{
	int ret = 0;
	int wq_id_clxc, output_id;

	FIMC_BUG(!hw_ip);
	FIMC_BUG(!frame);

	switch (hw_ip->id) {
	case DEV_HW_CLH0:
		wq_id_clxc = WORK_CL0C_FDONE;
		break;
	default:
		mserr_hw("[F:%d]invalid hw(%d)", instance, hw_ip, frame->fcount, hw_ip->id);
		return -EINVAL;
	}

	output_id = ENTRY_CLHC;
	if (test_bit(output_id, &frame->out_flag)) {
		ret = is_hardware_frame_done(hw_ip, frame, wq_id_clxc,
				output_id, done_type, false);
	}

	output_id = IS_HW_CORE_END;
	if (test_bit(hw_ip->id, &frame->core_flag)) {
		ret = is_hardware_frame_done(hw_ip, frame, -1,
				output_id, done_type, false);
	}

	return ret;
}

static int is_hw_clh_load_setfile(struct is_hw_ip *hw_ip, u32 instance, ulong hw_map)
{
	int flag = 0, ret = 0;
	ulong addr;
	u32 size, index;
	struct is_hw_clh *hw_clh = NULL;
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
	hw_clh = (struct is_hw_clh *)hw_ip->priv_info;

	for (index = 0; index < setfile->using_count; index++) {
		addr = setfile->table[index].addr;
		size = setfile->table[index].size;
		ret = is_lib_isp_create_tune_set(&hw_clh->lib[instance],
			addr, size, index, flag, instance);

		set_bit(index, &hw_clh->lib[instance].tune_count);
	}

	set_bit(HW_TUNESET, &hw_ip->state);

	return ret;
}

static int is_hw_clh_apply_setfile(struct is_hw_ip *hw_ip, u32 scenario,
	u32 instance, ulong hw_map)
{
	int ret = 0;
	u32 setfile_index = 0;
	struct is_hw_clh *hw_clh = NULL;
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
	hw_clh = (struct is_hw_clh *)hw_ip->priv_info;

	ret = is_lib_isp_apply_tune_set(&hw_clh->lib[instance], setfile_index, instance);

	return ret;
}

static int is_hw_clh_delete_setfile(struct is_hw_ip *hw_ip, u32 instance, ulong hw_map)
{
	struct is_hw_clh *hw_clh = NULL;
	int i, ret = 0;
	struct is_hw_ip_setfile *setfile;
	enum exynos_sensor_position sensor_position;

	FIMC_BUG(!hw_ip);

	if (!test_bit_variables(hw_ip->id, &hw_map)) {
		msdbg_hw(2, "%s: hw_map(0x%lx)\n", instance, hw_ip, __func__, hw_map);
		return 0;
	}

	if (!test_bit(HW_INIT, &hw_ip->state)) {
		msdbg_hw(2, "Not initialized\n", instance, hw_ip);
		return 0;
	}

	sensor_position = hw_ip->hardware->sensor_position[instance];
	setfile = &hw_ip->setfile[sensor_position];

	if (setfile->using_count == 0)
		return 0;

	FIMC_BUG(!hw_ip->priv_info);
	hw_clh = (struct is_hw_clh *)hw_ip->priv_info;

	for (i = 0; i < setfile->using_count; i++) {
		if (test_bit(i, &hw_clh->lib[instance].tune_count)) {
			ret = is_lib_isp_delete_tune_set(&hw_clh->lib[instance],
				(u32)i, instance);
			clear_bit(i, &hw_clh->lib[instance].tune_count);
		}
	}

	clear_bit(HW_TUNESET, &hw_ip->state);

	return ret;
}

int is_hw_clh_restore(struct is_hw_ip *hw_ip, u32 instance)
{
	int ret = 0;
	struct is_hw_clh *hw_clh = NULL;

	BUG_ON(!hw_ip);
	BUG_ON(!hw_ip->priv_info);

	if (!test_bit(HW_OPEN, &hw_ip->state))
		return -EINVAL;

	hw_clh = (struct is_hw_clh *)hw_ip->priv_info;

	ret = is_lib_isp_reset_recovery(hw_ip, &hw_clh->lib[instance], instance);
	if (ret) {
		mserr_hw("is_lib_clh_reset_recovery fail ret(%d)",
				instance, hw_ip, ret);
	}

	return ret;
}

const struct is_hw_ip_ops is_hw_clh_ops = {
	.open			= is_hw_clh_open,
	.init			= is_hw_clh_init,
	.deinit			= is_hw_clh_deinit,
	.close			= is_hw_clh_close,
	.enable			= is_hw_clh_enable,
	.disable		= is_hw_clh_disable,
	.shot			= is_hw_clh_shot,
	.set_param		= is_hw_clh_set_param,
	.get_meta		= is_hw_clh_get_meta,
	.frame_ndone		= is_hw_clh_frame_ndone,
	.load_setfile		= is_hw_clh_load_setfile,
	.apply_setfile		= is_hw_clh_apply_setfile,
	.delete_setfile		= is_hw_clh_delete_setfile,
	.restore		= is_hw_clh_restore
};

int is_hw_clh_probe(struct is_hw_ip *hw_ip, struct is_interface *itf,
	struct is_interface_ischain *itfc, int id, const char *name)
{
	int ret = 0;

	FIMC_BUG(!hw_ip);
	FIMC_BUG(!itf);
	FIMC_BUG(!itfc);

	/* initialize device hardware */
	hw_ip->id   = id;
	snprintf(hw_ip->name, sizeof(hw_ip->name), "%s", name);
	hw_ip->ops  = &is_hw_clh_ops;
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
