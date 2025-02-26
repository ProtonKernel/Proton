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

#include "is-hw-yuvpp.h"
#include "is-err.h"
#include "api/is-hw-api-yuvpp-v2_1.h"
#include "is-votfmgr.h"
#include "is-votf-id-table.h"
spinlock_t ypp_out_slock;
#define LIB_MODE USE_DRIVER

int debug_ypp;
module_param(debug_ypp, int, 0644);

extern struct is_lib_support gPtr_lib_support;
static inline void __nocfi __is_hw_ypp_ddk_isr(struct is_interface_hwip *itf_hw, int handler_id)
{
	struct hwip_intr_handler *intr_hw = NULL;

	intr_hw = &itf_hw->handler[handler_id];

	if (intr_hw->valid) {
		is_fpsimd_get_isr();
		intr_hw->handler(intr_hw->id, intr_hw->ctx);
		is_fpsimd_put_isr();
	} else {
		err_itfc("[ID:%d](%d)- chain(%d) empty handler!!",
			itf_hw->id, handler_id, intr_hw->chain_id);
	}
}

static int is_hw_ypp_handle_interrupt(u32 id, void *context)
{
	struct is_hardware *hardware = NULL;
	struct is_hw_ip *hw_ip = NULL;
	struct is_hw_ypp *hw_ypp = NULL;
	struct ypp_param *param = NULL;
	struct is_interface_hwip *itf_hw = NULL;
	u32 status, instance, hw_fcount, strip_index, hl = 0, vl = 0;
	bool f_err = false;
	int hw_slot = -1;

	hw_ip = (struct is_hw_ip *)context;

	hw_ypp = (struct is_hw_ypp *)hw_ip->priv_info;
	hardware = hw_ip->hardware;
	hw_fcount = atomic_read(&hw_ip->fcount);
	instance = atomic_read(&hw_ip->instance);
	strip_index = atomic_read(&hw_ypp->strip_index);
	param = &hw_ip->region[instance]->parameter.ypp;

	hw_slot = is_hw_slot_id(hw_ip->id);
	if (!valid_hw_slot_id(hw_slot)) {
		serr_hw("invalid hw_slot (%d)", hw_ip, hw_slot);
		return IRQ_NONE;
	}

	itf_hw = &hw_ip->itfc->itf_ip[hw_slot];

	if (hw_ip->lib_mode == USE_ONLY_DDK) {
		__is_hw_ypp_ddk_isr(itf_hw, INTR_HWIP1);
		return IRQ_HANDLED;
	}

	status = ypp_hw_g_int_state(hw_ip->regs[REG_SETA], true,
			(hw_ip->num_buffers & 0xffff), &hw_ypp->irq_state[YUVPP_INTR])
			& ypp_hw_g_int_mask(hw_ip->regs[REG_SETA]);

	msdbg_hw(2, "YUVPP interrupt status(0x%x)\n", instance, hw_ip, status);

	if (!test_bit(HW_OPEN, &hw_ip->state)) {
		mserr_hw("invalid interrupt: 0x%x", instance, hw_ip, status);
		return 0;
	}

	if (test_bit(HW_OVERFLOW_RECOVERY, &hardware->hw_recovery_flag)) {
		mserr_hw("During recovery : invalid interrupt", instance, hw_ip);
		return 0;
	}

	if (!test_bit(HW_RUN, &hw_ip->state)) {
		mserr_hw("HW disabled!! interrupt(0x%x)", instance, hw_ip, status);
		return 0;
	}

	if (ypp_hw_is_occured(status, INTR_FRAME_START) && ypp_hw_is_occured(status, INTR_FRAME_END))
		mswarn_hw("start/end overlapped!! (0x%x)", instance, hw_ip, status);

	if (ypp_hw_is_occured(status, INTR_FRAME_START)) {
		if (IS_ENABLED(YPP_DDK_LIB_CALL)) {
			is_lib_isp_event_notifier(hw_ip, &hw_ypp->lib[instance], instance, hw_fcount,
				EVENT_FRAME_START, strip_index);
		} else {
			atomic_add(1, &hw_ip->count.fs);
			_is_hw_frame_dbg_trace(hw_ip, hw_fcount, DEBUG_POINT_FRAME_START);
			if (!atomic_read(&hardware->streaming[hardware->sensor_position[instance]]))
				msinfo_hw("[F:%d]F.S\n", instance, hw_ip, hw_fcount);

			is_hardware_frame_start(hw_ip, instance);
		}
	}

	if (ypp_hw_is_occured(status, INTR_FRAME_END)) {
		if (IS_ENABLED(YPP_DDK_LIB_CALL)) {
			is_lib_isp_event_notifier(hw_ip, &hw_ypp->lib[instance], instance, hw_fcount,
				EVENT_FRAME_END, strip_index);
		} else {
			_is_hw_frame_dbg_trace(hw_ip, hw_fcount, DEBUG_POINT_FRAME_END);
			atomic_add(hw_ip->num_buffers, &hw_ip->count.fe);

			is_hardware_frame_done(hw_ip, NULL, -1, IS_HW_CORE_END,
							IS_SHOT_SUCCESS, true);

			if (!atomic_read(&hardware->streaming[hardware->sensor_position[instance]]))
				msinfo_hw("[F:%d]F.E\n", instance, hw_ip, hw_fcount);

			atomic_set(&hw_ip->status.Vvalid, V_BLANK);
			if (atomic_read(&hw_ip->count.fs) < atomic_read(&hw_ip->count.fe)) {
				mserr_hw("fs(%d), fe(%d), dma(%d), status(0x%x)", instance, hw_ip,
					atomic_read(&hw_ip->count.fs),
					atomic_read(&hw_ip->count.fe),
					atomic_read(&hw_ip->count.dma), status);
			}
			wake_up(&hw_ip->status.wait_queue);
		}
	}

	if (ypp_hw_is_occured(status, INTR_COREX_END_0)) {
		/* */
	}

	if (ypp_hw_is_occured(status, INTR_COREX_END_1)) {
		/* */
	}

	f_err = ypp_hw_is_occured(status, INTR_ERR);

	if (f_err) {
		msinfo_hw("[F:%d] Ocurred error interrupt (%d,%d) status(0x%x)\n",
			instance, hw_ip, hw_fcount, hl, vl, status);
		ypp_hw_dump(hw_ip->regs[REG_SETA]);
	}

	return 0;
}

static int __is_hw_ypp_s_reset(struct is_hw_ip *hw_ip, struct is_hw_ypp *hw_ypp)
{
	int i;

	for (i = 0; i < COREX_MAX; i++)
		hw_ypp->cur_hw_iq_set[i].size = 0;

	return ypp_hw_s_reset(hw_ip->regs[REG_SETA]);
}

static int __is_hw_ypp_s_common_reg(struct is_hw_ip *hw_ip, u32 instance)
{
	int res = 0;
	struct is_hw_ypp *hw_ypp;

	FIMC_BUG(!hw_ip);

	hw_ypp = (struct is_hw_ypp *)hw_ip->priv_info;

	if (hw_ip) {
		msinfo_hw("reset\n", instance, hw_ip);

		res = __is_hw_ypp_s_reset(hw_ip, hw_ypp);
		if (res != 0) {
			mserr_hw("sw reset fail", instance, hw_ip);
			return -ENODEV;
		}
		/* temp corex disable*/
		ypp_hw_s_corex_init(hw_ip->regs[REG_SETA], 0);
		ypp_hw_s_init(hw_ip->regs[REG_SETA]);
	}

	msinfo_hw("clear interrupt\n", instance, hw_ip);

	return 0;
}

static int __is_hw_ypp_clear_common(struct is_hw_ip *hw_ip, u32 instance)
{
	int res = 0;
	struct is_hw_ypp *hw_ypp;

	FIMC_BUG(!hw_ip);

	hw_ypp = (struct is_hw_ypp *)hw_ip->priv_info;

	res = __is_hw_ypp_s_reset(hw_ip, hw_ypp);
	if (res != 0) {
		mserr_hw("sw reset fail", instance, hw_ip);
		return -ENODEV;
	}
	res = ypp_hw_wait_idle(hw_ip->regs[REG_SETA]);

	if (res)
		mserr_hw("failed to ypp_hw_wait_idle", instance, hw_ip);

	msinfo_hw("final finished ypp\n", instance, hw_ip);

	return res;
}

static int is_hw_ypp_set_cmdq(struct is_hw_ip *hw_ip, u32 instance, u32 set_id)
{
	/*
	 * add to command queue
	 * 1. set corex_update_type_0_setX
	 * normal shot: SETA(wide), SETB(tele, front), SETC(uw)
	 * reprocessing shot: SETD
	 * 2. set cq_frm_start_type to 0. only at first time?
	 * 3. set ms_add_to_queue(set id, frame id).
	 */
	int ret = 0;

	FIMC_BUG(!hw_ip);

	ret = ypp_hw_s_corex_update_type(hw_ip->regs[REG_SETA], set_id);
	if (ret)
		return ret;
	ypp_hw_s_cmdq(hw_ip->regs[REG_SETA], set_id);

	return ret;
}

static int __is_hw_ypp_init_config(struct is_hw_ip *hw_ip, u32 instance, struct is_frame *frame, u32 set_id)
{
	struct is_hw_ypp *hw_ypp;
	struct is_device_ischain *device;
	struct is_group *group;
	struct is_hardware *hardware;
	struct ypp_param_set *param_set = NULL;
	int ret = 0;

	FIMC_BUG(!hw_ip);

	hw_ypp = (struct is_hw_ypp *)hw_ip->priv_info;
	if (!hw_ypp) {
		mserr_hw("failed to get ypp", instance, hw_ip);
		return -ENODEV;
	}

	group = hw_ip->group[instance];
	if (!group) {
		mserr_hw("failed to get group", instance, hw_ip);
		return -ENODEV;
	}

	device = group->device;
	if (!device) {
		mserr_hw("failed to get devcie", instance, hw_ip);
		return -ENODEV;
	}

	hardware = hw_ip->hardware;
	if (!hardware) {
		mserr_hw("failed to get hardware", instance, hw_ip);
		return -EINVAL;
	}

	param_set = &hw_ypp->param_set[instance];

	/* ypp context setting */
	ypp_hw_s_core(hw_ip->regs[REG_SETA], frame->num_buffers, set_id);

	return ret;
}


static int __nocfi is_hw_ypp_open(struct is_hw_ip *hw_ip, u32 instance,
	struct is_group *group)
{
	int i,ret = 0;
	struct is_hw_ypp *hw_ypp = NULL;

	FIMC_BUG(!hw_ip);

	if (test_bit(HW_OPEN, &hw_ip->state))
		return 0;

	frame_manager_probe(hw_ip->framemgr, hw_ip->id, "HWYPP");
	frame_manager_open(hw_ip->framemgr, IS_MAX_HW_FRAME);

	hw_ip->priv_info = vzalloc(sizeof(struct is_hw_ypp));
	if (!hw_ip->priv_info) {
		mserr_hw("hw_ip->priv_info(null)", instance, hw_ip);
		ret = -ENOMEM;
		goto err_alloc;
	}

	hw_ypp = (struct is_hw_ypp *)hw_ip->priv_info;
	if (!hw_ypp) {
		mserr_hw("failed to get hw_ypp", instance, hw_ip);
		ret = -ENODEV;
		goto err_alloc;
	}
	hw_ypp->instance = instance;

	is_fpsimd_get_func();
	ret = get_lib_func(LIB_FUNC_YUVPP, (void **)&hw_ypp->lib_func);
	is_fpsimd_put_func();

	if (hw_ypp->lib_func == NULL) {
		mserr_hw("hw_ypp->lib_func(null)", instance, hw_ip);
		is_load_clear();
		ret = -EINVAL;
		goto err_lib_func;
	}
	msinfo_hw("get_lib_func is set\n", instance, hw_ip);

	hw_ypp->lib_support = &gPtr_lib_support;

	hw_ypp->lib[instance].func = hw_ypp->lib_func;
	if (IS_ENABLED(YPP_DDK_LIB_CALL)) {
		ret = is_lib_isp_chain_create(hw_ip, &hw_ypp->lib[instance], instance);
		if (ret) {
			mserr_hw("chain create fail", instance, hw_ip);
			ret = -EINVAL;
			goto err_chain_create;
		}
	}

	atomic_set(&hw_ip->status.Vvalid, V_BLANK);
	set_bit(HW_OPEN, &hw_ip->state);

	for (i = 0; i < IS_STREAM_COUNT; i++) {
		clear_bit(CR_SET_CONFIG, &hw_ypp->iq_set[i].state);
		set_bit(CR_SET_EMPTY, &hw_ypp->iq_set[i].state);
		spin_lock_init(&hw_ypp->iq_set[i].slock);
	}

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

static int is_hw_ypp_init(struct is_hw_ip *hw_ip, u32 instance,
	struct is_group *group, bool flag, u32 module_id)
{
	int ret = 0;
	struct is_hw_ypp *hw_ypp = NULL;
	struct is_device_ischain *device;
	u32 input_id = 0;

	FIMC_BUG(!hw_ip);
	FIMC_BUG(!hw_ip->priv_info);
	FIMC_BUG(!group);

	hw_ypp = (struct is_hw_ypp *)hw_ip->priv_info;
	if (!hw_ypp) {
		mserr_hw("hw_ypp is null ", instance, hw_ip);
		ret = -ENODATA;
		goto err;
	}
	device = group->device;
	if (!device) {
		mserr_hw("failed to get devcie", instance, hw_ip);
		ret = -ENODEV;
		goto err;
	}

	hw_ip->lib_mode = LIB_MODE;
	hw_ypp->lib[instance].object = NULL;
	hw_ypp->lib[instance].func   = hw_ypp->lib_func;

	if (hw_ypp->lib[instance].object != NULL) {
		msdbg_hw(2, "object is already created\n", instance, hw_ip);
	} else {
		if (IS_ENABLED(YPP_DDK_LIB_CALL)) {
			ret = is_lib_isp_object_create(hw_ip, &hw_ypp->lib[instance],
					instance, (u32)flag, module_id);
			if (ret) {
				mserr_hw("object create fail", instance, hw_ip);
				return -EINVAL;
			}
		}
	}

	if (hw_ip->lib_mode == USE_ONLY_DDK) {
		set_bit(HW_INIT, &hw_ip->state);
		return 0;
	}

	for (input_id = YUVPP_RDMA_L0_Y; input_id < YUVPP_RDMA_MAX; input_id++) {
		ret = ypp_hw_dma_create(&hw_ypp->rdma[input_id], hw_ip->regs[REG_SETA], input_id);
		if (ret) {
			mserr_hw("ypp_hw_dma_create error[%d]", instance, hw_ip, input_id);
			ret = -ENODATA;
			goto err;
		}
	}

	set_bit(HW_INIT, &hw_ip->state);
	return 0;

err:
	return ret;

}

static int is_hw_ypp_deinit(struct is_hw_ip *hw_ip, u32 instance)
{
	int ret = 0;
	struct is_hw_ypp *hw_ypp;

	FIMC_BUG(!hw_ip);
	FIMC_BUG(!hw_ip->priv_info);

	hw_ypp = (struct is_hw_ypp *)hw_ip->priv_info;
	if (IS_ENABLED(YPP_DDK_LIB_CALL)) {
		is_lib_isp_object_destroy(hw_ip, &hw_ypp->lib[instance], instance);
		hw_ypp->lib[instance].object = NULL;
	}
	return ret;
}

static int is_hw_ypp_close(struct is_hw_ip *hw_ip, u32 instance)
{
	struct is_hw_ypp *hw_ypp = NULL;
	int ret = 0;
	ulong flag = 0;

	FIMC_BUG(!hw_ip);
	FIMC_BUG(!hw_ip->priv_info);

	if (!test_bit(HW_OPEN, &hw_ip->state))
		return 0;

	hw_ypp = (struct is_hw_ypp *)hw_ip->priv_info;
	FIMC_BUG(!hw_ypp->lib_support);

	if (IS_ENABLED(YPP_DDK_LIB_CALL))
		is_lib_isp_chain_destroy(hw_ip, &hw_ypp->lib[instance], instance);

	if (hw_ip->lib_mode == USE_DRIVER) {
		spin_lock_irqsave(&ypp_out_slock, flag);
		__is_hw_ypp_clear_common(hw_ip, instance);
		spin_unlock_irqrestore(&ypp_out_slock, flag);
	}

	vfree(hw_ip->priv_info);
	hw_ip->priv_info = NULL;

	frame_manager_close(hw_ip->framemgr);
	clear_bit(HW_OPEN, &hw_ip->state);


	return ret;
}

static int is_hw_ypp_enable(struct is_hw_ip *hw_ip, u32 instance, ulong hw_map)
{
	struct is_hw_ypp *hw_ypp = NULL;
	int ret = 0;
	ulong flag = 0;

	FIMC_BUG(!hw_ip);
	FIMC_BUG(!hw_ip->priv_info);

	if (!test_bit_variables(hw_ip->id, &hw_map))
		return 0;

	if (!test_bit(HW_INIT, &hw_ip->state)) {
		mserr_hw("not initialized!!", instance, hw_ip);
		return -EINVAL;
	}

	atomic_inc(&hw_ip->run_rsccount);

	if (test_bit(HW_RUN, &hw_ip->state))
		return 0;

	msdbg_hw(2, "enable: start\n", instance, hw_ip);

	hw_ypp = (struct is_hw_ypp *)hw_ip->priv_info;
	if (hw_ip->lib_mode == USE_ONLY_DDK) {
		set_bit(HW_RUN, &hw_ip->state);
		return ret;
	}

	spin_lock_irqsave(&ypp_out_slock, flag);
	__is_hw_ypp_s_common_reg(hw_ip, instance);
	spin_unlock_irqrestore(&ypp_out_slock, flag);

	set_bit(HW_RUN, &hw_ip->state);
	msdbg_hw(2, "enable: done\n", instance, hw_ip);

	return ret;
}

static int is_hw_ypp_disable(struct is_hw_ip *hw_ip, u32 instance, ulong hw_map)
{
	int ret = 0;
	long timetowait;
	struct is_hw_ypp *hw_ypp;
	struct ypp_param_set *param_set;

	FIMC_BUG(!hw_ip);

	if (!test_bit_variables(hw_ip->id, &hw_map))
		return 0;

	if (!test_bit(HW_INIT, &hw_ip->state)) {
		mserr_hw("not initialized!!", instance, hw_ip);
		return -EINVAL;
	}

	msinfo_hw("ypp_disable: Vvalid(%d)\n", instance, hw_ip,
		atomic_read(&hw_ip->status.Vvalid));

	FIMC_BUG(!hw_ip->priv_info);
	hw_ypp = (struct is_hw_ypp *)hw_ip->priv_info;

	timetowait = wait_event_timeout(hw_ip->status.wait_queue,
		!atomic_read(&hw_ip->status.Vvalid),
		IS_HW_STOP_TIMEOUT);

	if (!timetowait) {
		mserr_hw("wait FRAME_END timeout (%ld)", instance,
			hw_ip, timetowait);
		ret = -ETIME;
	}

	param_set = &hw_ypp->param_set[instance];
	param_set->fcount = 0;
	if (test_bit(HW_RUN, &hw_ip->state)) {
		if (IS_ENABLED(YPP_DDK_LIB_CALL))
			is_lib_isp_stop(hw_ip, &hw_ypp->lib[instance], instance);
	} else {
		msdbg_hw(2, "already disabled\n", instance, hw_ip);
	}

	if (atomic_dec_return(&hw_ip->run_rsccount) > 0)
		return 0;

	clear_bit(HW_RUN, &hw_ip->state);
	clear_bit(HW_CONFIG, &hw_ip->state);

	return ret;
}

static int __is_hw_ypp_set_rdma_cmd(struct ypp_param_set *param_set, u32 instance, struct is_yuvpp_config *conf)
{
	FIMC_BUG(!param_set);
	FIMC_BUG(!conf);

	param_set->dma_input.cmd = DMA_INPUT_COMMAND_ENABLE;

	if (conf->yuvnr_bypass & 0x1) {
		param_set->dma_input_lv2.cmd = DMA_INPUT_COMMAND_DISABLE;
		param_set->dma_input_drc.cmd = DMA_INPUT_COMMAND_DISABLE;
	} else {
		param_set->dma_input_lv2.cmd = DMA_INPUT_COMMAND_ENABLE;
		param_set->dma_input_drc.cmd = DMA_INPUT_COMMAND_ENABLE;
	}

	if (conf->clahe_bypass & 0x1)
		param_set->dma_input_hist.cmd = DMA_INPUT_COMMAND_DISABLE;
	else
		param_set->dma_input_hist.cmd = DMA_INPUT_COMMAND_ENABLE;

	if (conf->nadd_bypass & 0x1)
		param_set->dma_input_noise.cmd = DMA_INPUT_COMMAND_DISABLE;
	else
		param_set->dma_input_noise.cmd = DMA_INPUT_COMMAND_ENABLE;

	return 0;
}

static int __is_hw_ypp_set_rdma(struct is_hw_ip *hw_ip, struct is_hw_ypp *hw_ypp, struct ypp_param_set *param_set, u32 instance, u32 id, u32 set_id)
{
	u32 *input_dva = NULL;
	u32 cmd;
	u32 grid_x_num = 0, grid_y_num = 0;
	u32 comp_sbwc_en, payload_size;
	u32 in_crop_size_x = 0;
	u32 cache_hint = 0;
	int ret = 0;

	FIMC_BUG(!hw_ip);
	FIMC_BUG(!hw_ypp);
	FIMC_BUG(!param_set);

	switch (id) {
	case YUVPP_RDMA_L0_Y:
	case YUVPP_RDMA_L0_UV:
		input_dva = param_set->input_dva;
		cmd = param_set->dma_input.cmd;
		break;
	case YUVPP_RDMA_L2_Y:
	case YUVPP_RDMA_L2_UV:
	case YUVPP_RDMA_L2_1_Y:
	case YUVPP_RDMA_L2_1_UV:
	case YUVPP_RDMA_L2_2_Y:
	case YUVPP_RDMA_L2_2_UV:
		cmd = param_set->dma_input_lv2.cmd;
		input_dva = param_set->input_dva_lv2;
		in_crop_size_x = ypp_hw_g_in_crop_size_x(hw_ip->regs[REG_SETA], set_id);
		break;
	case YUVPP_RDMA_NOISE:
		if (!ypp_hw_g_nadd_use_noise_rdma(hw_ip->regs[REG_SETA], set_id))
			param_set->dma_input_noise.cmd = DMA_INPUT_COMMAND_DISABLE;
		cmd = param_set->dma_input_noise.cmd;
		input_dva = param_set->input_dva_noise;
		break;
	case YUVPP_RDMA_DRCGAIN:
		cmd = param_set->dma_input_drc.cmd;
		input_dva = param_set->input_dva_drc;
		in_crop_size_x = ypp_hw_g_in_crop_size_x(hw_ip->regs[REG_SETA], set_id);
		break;
	case YUVPP_RDMA_HIST:
		cmd = param_set->dma_input_hist.cmd;
		input_dva = param_set->input_dva_hist;
		ypp_hw_g_hist_grid_num(hw_ip->regs[REG_SETA], set_id, &grid_x_num, &grid_y_num);
		break;
	default:
		merr_hw("invalid ID (%d)", instance,  id);
		return -EINVAL;
	}

	switch (id) {
	case YUVPP_RDMA_L0_Y:
	case YUVPP_RDMA_L0_UV:
	case YUVPP_RDMA_NOISE:
	case YUVPP_RDMA_DRCGAIN:
		cache_hint = 0x7;	// 111: last-access-read
		break;
	case YUVPP_RDMA_L2_Y:
	case YUVPP_RDMA_L2_UV:
	case YUVPP_RDMA_L2_1_Y:
	case YUVPP_RDMA_L2_1_UV:
	case YUVPP_RDMA_L2_2_Y:
	case YUVPP_RDMA_L2_2_UV:
	case YUVPP_RDMA_HIST:
		cache_hint = 0x3;	// 011: cache-noalloc-type
		break;
	default:
		merr_hw("invalid ID (%d)", instance,  id);
		return -EINVAL;
	}

	msdbg_hw(2, "%s: %d\n", instance, hw_ip, hw_ypp->rdma[id].name, cmd);

	ypp_hw_s_rdma_corex_id(&hw_ypp->rdma[id], set_id);

	ret = ypp_hw_s_rdma_init(&hw_ypp->rdma[id], param_set, cmd,
		grid_x_num, grid_y_num, in_crop_size_x, cache_hint,
		&comp_sbwc_en, &payload_size);
	if (ret) {
		mserr_hw("failed to initialize YUVPP_RDMA(%d)", instance, hw_ip, id);
		return -EINVAL;
	}

	if (cmd == DMA_INPUT_COMMAND_ENABLE) {
		ret = ypp_hw_s_rdma_addr(&hw_ypp->rdma[id], input_dva, 0,
			(hw_ip->num_buffers & 0xffff),
			0, comp_sbwc_en, payload_size);
		if (ret) {
			mserr_hw("failed to set YUVPP_RDMA(%d) address", instance, hw_ip, id);
			return -EINVAL;
		}
	}

	return 0;
}

static void __is_hw_ypp_check_size(struct is_hw_ip *hw_ip, struct ypp_param_set *param_set, u32 instance)
{
	struct param_dma_input *input = &param_set->dma_input;
	struct param_otf_output *output = &param_set->otf_output;

	FIMC_BUG_VOID(!param_set);

	msdbgs_hw(2, "hw_ypp_check_size >>>\n", instance, hw_ip);
	msdbgs_hw(2, "dma_input: format(%d),crop_size(%dx%d)\n", instance, hw_ip,
		input->format, input->dma_crop_width, input->dma_crop_height);
	msdbgs_hw(2, "otf_output: otf_cmd(%d),format(%d)\n", instance, hw_ip,
		output->cmd, output->format);
	msdbgs_hw(2, "otf_output: pos(%d,%d),crop%dx%d),size(%dx%d)\n", instance, hw_ip,
		output->crop_offset_x, output->crop_offset_y,
		output->crop_width, output->crop_height,
		output->width, output->height);
	msdbgs_hw(2, "[%d]<<< hw_ypp_check_size <<<\n", instance, hw_ip);
}

static int __is_hw_ypp_bypass(struct is_hw_ip *hw_ip, u32 set_id)
{
	FIMC_BUG(!hw_ip);

	ypp_hw_s_block_bypass(hw_ip->regs[REG_SETA], set_id);

	return 0;
}


static int __is_hw_ypp_update_block_reg(struct is_hw_ip *hw_ip, struct ypp_param_set *param_set, u32 instance, u32 set_id)
{
	struct is_hw_ypp *hw_ypp;
	int ret = 0;
	FIMC_BUG(!hw_ip);
	FIMC_BUG(!param_set);

	hw_ypp = (struct is_hw_ypp *)hw_ip->priv_info;

	if (hw_ypp->instance != instance) {
		msdbg_hw(2, "update_param: hw_ip->instance(%d)\n",
			instance, hw_ip, hw_ypp->instance);
		hw_ypp->instance = instance;
	}

	__is_hw_ypp_check_size(hw_ip, param_set, instance);
	ret = __is_hw_ypp_bypass(hw_ip, set_id);

	return ret;
}

static void __is_hw_ypp_update_param(struct is_hw_ip *hw_ip, struct is_region *region,\
	struct ypp_param_set *param_set, u32 lindex, u32 hindex, u32 instance)
{
	struct ypp_param *param;

	FIMC_BUG_VOID(!hw_ip);
	FIMC_BUG_VOID(!region);
	FIMC_BUG_VOID(!param_set);

	param = &region->parameter.ypp;
	param_set->instance_id = instance;

	/* check input */
	if ((lindex & LOWBIT_OF(PARAM_YPP_DMA_INPUT))
		|| (hindex & HIGHBIT_OF(PARAM_YPP_DMA_INPUT))) {
		memcpy(&param_set->dma_input, &param->dma_input,
			sizeof(struct param_dma_input));
	}

	if ((lindex & LOWBIT_OF(PARAM_YPP_DMA_LV2_INPUT))
		|| (hindex & HIGHBIT_OF(PARAM_YPP_DMA_LV2_INPUT))) {
		memcpy(&param_set->dma_input_lv2, &param->dma_input_lv2,
			sizeof(struct param_dma_input));
	}

	if ((lindex & LOWBIT_OF(PARAM_YPP_DMA_NOISE_INPUT))
		|| (hindex & HIGHBIT_OF(PARAM_YPP_DMA_NOISE_INPUT))) {
		memcpy(&param_set->dma_input_noise, &param->dma_input_noise,
			sizeof(struct param_dma_input));
	}

	if ((lindex & LOWBIT_OF(PARAM_YPP_DMA_DRC_INPUT))
		|| (hindex & HIGHBIT_OF(PARAM_YPP_DMA_DRC_INPUT))) {
		memcpy(&param_set->dma_input_drc, &param->dma_input_drc,
			sizeof(struct param_dma_input));
	}

	if ((lindex & LOWBIT_OF(PARAM_YPP_DMA_HIST_INPUT))
		|| (hindex & HIGHBIT_OF(PARAM_YPP_DMA_HIST_INPUT))) {
		memcpy(&param_set->dma_input_hist, &param->dma_input_hist,
			sizeof(struct param_dma_input));
	}

	/* check output*/
	if ((lindex & LOWBIT_OF(PARAM_YPP_OTF_OUTPUT))
		|| (hindex & HIGHBIT_OF(PARAM_YPP_OTF_OUTPUT))) {
		memcpy(&param_set->otf_output, &param->otf_output,
			sizeof(struct param_otf_output));
	}

	if ((lindex & LOWBIT_OF(PARAM_YPP_STRIPE_INPUT))
		|| (hindex & HIGHBIT_OF(PARAM_YPP_STRIPE_INPUT))) {
		memcpy(&param_set->stripe_input, &param->stripe_input,
			sizeof(struct param_stripe_input));
	}

}

static int is_hw_ypp_set_param(struct is_hw_ip *hw_ip, struct is_region *region,
	u32 lindex, u32 hindex, u32 instance, ulong hw_map)
{
	int ret = 0;
	struct is_hw_ypp *hw_ypp;
	struct ypp_param_set *param_set;

	FIMC_BUG(!hw_ip);
	FIMC_BUG(!hw_ip->priv_info);
	FIMC_BUG(!region);

	if (!test_bit_variables(hw_ip->id, &hw_map))
		return 0;

	if (!test_bit(HW_INIT, &hw_ip->state)) {
		mserr_hw("not initialized!!", instance, hw_ip);
		return -EINVAL;
	}

	hw_ip->region[instance] = region;
	hw_ip->lindex[instance] = lindex;
	hw_ip->hindex[instance] = hindex;

	hw_ypp = (struct is_hw_ypp *)hw_ip->priv_info;
	hw_ypp->instance = IS_STREAM_COUNT;

	param_set = &hw_ypp->param_set[instance];
	__is_hw_ypp_update_param(hw_ip, region, param_set, lindex, hindex, instance);
	return ret;
}

static int __is_hw_ypp_set_rta_regs(void __iomem *base, u32 set_id, struct is_hw_ypp *hw_ypp, u32 instance)
{
	struct is_hw_ypp_iq *iq_set = NULL;
	struct cr_set *regs;
	unsigned long flag;
	u32 regs_size;
	int i, ret = 0;

	FIMC_BUG(!hw_ypp);

	iq_set = &hw_ypp->iq_set[instance];

	spin_lock_irqsave(&iq_set->slock, flag);

	if (test_and_clear_bit(CR_SET_CONFIG, &iq_set->state)) {
		regs = iq_set->regs;
		regs_size = iq_set->size;
		if (hw_ypp->cur_hw_iq_set[set_id].size != regs_size ||
			memcmp(hw_ypp->cur_hw_iq_set[set_id].regs, regs, sizeof(struct cr_set) * regs_size) != 0) {

			for (i = 0; i < regs_size; i++)
				writel(regs[i].reg_data, base + GET_COREX_OFFSET(set_id) + regs[i].reg_addr);

			memcpy(hw_ypp->cur_hw_iq_set[set_id].regs, regs, sizeof(struct cr_set) * regs_size);
			hw_ypp->cur_hw_iq_set[set_id].size = regs_size;
		}

		set_bit(CR_SET_EMPTY, &iq_set->state);
	}

	spin_unlock_irqrestore(&iq_set->slock, flag);

	return ret;
}

static int __is_hw_ypp_set_size_regs(struct is_hw_ip *hw_ip, struct ypp_param_set *param_set,
	u32 instance, u32 yuvpp_strip_start_pos, const struct is_frame *frame, u32 set_id)
{
	struct is_device_ischain *device;
	struct is_hw_ypp *hw_ypp;
	struct is_yuvpp_config *config;
	struct is_param_region *param_region;
	unsigned long flag;
	int ret = 0;
	u32 strip_enable;
	u32 dma_width, dma_height;
	u32 frame_width;
	u32 region_id;
	u32 crop_x, crop_y;
	u32 sensor_full_width, sensor_full_height;
	u32 sensor_binning_x, sensor_binning_y;
	u32 sensor_crop_x, sensor_crop_y;
	u32 taa_crop_x, taa_crop_y;
	u32 taa_crop_width, taa_crop_height;

	FIMC_BUG(!hw_ip);
	FIMC_BUG(!param_set);

	hw_ypp = (struct is_hw_ypp *)hw_ip->priv_info;
	config = &hw_ypp->config;

	device = hw_ip->group[instance]->device;
	param_region = &device->is_region->parameter;
	strip_enable = (param_set->stripe_input.total_count == 0) ? 0 : 1;

	region_id = param_set->stripe_input.index;
	frame_width = param_set->stripe_input.full_width;
	dma_width = param_set->dma_input.width;
	dma_height = param_set->dma_input.height;
	crop_x = param_set->dma_input.dma_crop_offset >> 16;
	crop_y = param_set->dma_input.dma_crop_offset & 0xFFFF;
	frame_width = (strip_enable) ? param_set->stripe_input.full_width : dma_width;

	sensor_full_width = frame->shot->udm.frame_info.sensor_size[0];
	sensor_full_height = frame->shot->udm.frame_info.sensor_size[1];
	sensor_crop_x = frame->shot->udm.frame_info.sensor_crop_region[0];
	sensor_crop_y = frame->shot->udm.frame_info.sensor_crop_region[1];
	taa_crop_x = frame->shot->udm.frame_info.taa_in_crop_region[0];
	taa_crop_y = frame->shot->udm.frame_info.taa_in_crop_region[1];
	taa_crop_width = frame->shot->udm.frame_info.taa_in_crop_region[2];
	taa_crop_height = frame->shot->udm.frame_info.taa_in_crop_region[3];
	sensor_binning_x = frame->shot->udm.frame_info.sensor_binning[0];
	sensor_binning_y = frame->shot->udm.frame_info.sensor_binning[1];

	spin_lock_irqsave(&ypp_out_slock, flag);
	ret = ypp_hw_s_yuvnr_incrop(hw_ip->regs[REG_SETA], set_id,
		strip_enable, region_id, param_set->stripe_input.total_count, dma_width);
	if (ret) {
		mserr_hw("failed to set size regs for yuvnr_in_crop", instance, hw_ip);
		goto err;

	}
	ret = ypp_hw_s_yuvnr_size(hw_ip->regs[REG_SETA], set_id, yuvpp_strip_start_pos, frame_width,
		dma_width, dma_height, crop_x, crop_y, strip_enable,
		sensor_full_width, sensor_full_height, sensor_binning_x, sensor_binning_y, sensor_crop_x, sensor_crop_y,
		taa_crop_x, taa_crop_y, taa_crop_width, taa_crop_height);
	if (ret) {
		mserr_hw("failed to set size regs for yuvnr", instance, hw_ip);
		goto err;
	}
	ret = ypp_hw_s_yuvnr_outcrop(hw_ip->regs[REG_SETA], set_id,
		strip_enable, region_id, param_set->stripe_input.total_count);
	if (ret) {
		mserr_hw("failed to set size regs for yuvnr_out_crop", instance, hw_ip);
		goto err;
	}

	ret = ypp_hw_s_clahe_size(hw_ip->regs[REG_SETA], set_id, frame_width, dma_height,
		yuvpp_strip_start_pos, strip_enable);
	if (ret) {
		mserr_hw("failed to set size regs for clahe", instance, hw_ip);
		goto err;
	}

	ypp_hw_s_nadd_size(hw_ip->regs[REG_SETA], set_id, yuvpp_strip_start_pos, strip_enable);

	ypp_hw_s_coutfifo_size(hw_ip->regs[REG_SETA], set_id, dma_width, dma_height, strip_enable);
err:
	spin_unlock_irqrestore(&ypp_out_slock, flag);
	return ret;
}

static int is_hw_ypp_shot(struct is_hw_ip *hw_ip, struct is_frame *frame,
	ulong hw_map)
{
	struct is_hw_ypp *hw_ypp = NULL;
	struct is_region *region = NULL;
	struct ypp_param_set *param_set = NULL;
	struct is_frame *dma_frame;
	struct is_group *group;
	u32 en_votf = 0;
	u32 fcount, instance, lindex, hindex;
	u32 strip_start_pos = 0;
	ulong flag = 0;
	u32 cur_idx;
	u32 set_id;
	int ret = 0, i = 0, batch_num;
	u32 cmd_input, cmd_input_lv2, cmd_input_noise;
	u32 cmd_input_drc, cmd_input_hist;
	u32 stripe_index;
	u32 max_dma_id;

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

	set_bit(hw_ip->id, &frame->core_flag);

	FIMC_BUG(!hw_ip->priv_info);
	hw_ypp = (struct is_hw_ypp *)hw_ip->priv_info;
	param_set = &hw_ypp->param_set[instance];
	region = hw_ip->region[instance];
	FIMC_BUG(!region);

	atomic_set(&hw_ypp->strip_index, frame->stripe_info.region_id);
	fcount = frame->fcount;
	dma_frame = frame;

	FIMC_BUG(!frame->shot);
	/* per-frame control
	 * check & update size from region
	 */
	lindex = frame->shot->ctl.vendor_entry.lowIndexParam;
	hindex = frame->shot->ctl.vendor_entry.highIndexParam;
	if (hw_ip->internal_fcount[instance] != 0)
		hw_ip->internal_fcount[instance] = 0;

	if (frame->shot_ext) {
		if ((param_set->tnr_mode != frame->shot_ext->tnr_mode) &&
				!CHK_VIDEOHDR_MODE_CHANGE(param_set->tnr_mode, frame->shot_ext->tnr_mode))
			msinfo_hw("[F:%d] TNR mode is changed (%d -> %d)\n",
					instance, hw_ip, frame->fcount,
					param_set->tnr_mode, frame->shot_ext->tnr_mode);
		param_set->tnr_mode = frame->shot_ext->tnr_mode;
	} else {
		mswarn_hw("frame->shot_ext is null", instance, hw_ip);
		param_set->tnr_mode = TNR_PROCESSING_PREVIEW_POST_ON;
	}

	__is_hw_ypp_update_param(hw_ip, region, param_set, lindex, hindex, instance);

	/* DMA settings */
	cur_idx = frame->cur_buf_index;

	en_votf = param_set->dma_input.v_otf_enable;
	if (en_votf == OTF_INPUT_COMMAND_ENABLE) {
		group = hw_ip->group[instance];
		dma_frame = is_votf_get_frame(group, TRS, ENTRY_YPP, 0);
	}

	cmd_input = is_hardware_dma_cfg("ypp", hw_ip,
			frame, cur_idx,
			&param_set->dma_input.cmd,
			param_set->dma_input.plane,
			param_set->input_dva,
			dma_frame->dvaddr_buffer);

	cmd_input_lv2 = is_hardware_dma_cfg_32bit("ypnrds", hw_ip,
			frame, cur_idx,
			&param_set->dma_input_lv2.cmd,
			param_set->dma_input_lv2.plane,
			param_set->input_dva_lv2,
			dma_frame->ypnrdsTargetAddress);

	cmd_input_noise = is_hardware_dma_cfg_32bit("ypnoi", hw_ip,
			frame, cur_idx,
			&param_set->dma_input_noise.cmd,
			param_set->dma_input_noise.plane,
			param_set->input_dva_noise,
			dma_frame->ypnoiTargetAddress);

	cmd_input_drc = is_hardware_dma_cfg_32bit("ypdga", hw_ip,
			frame, cur_idx,
			&param_set->dma_input_drc.cmd,
			param_set->dma_input_drc.plane,
			param_set->input_dva_drc,
			dma_frame->ypdgaTargetAddress);

	cmd_input_hist = is_hardware_dma_cfg_32bit("ypsvhist", hw_ip,
			frame, cur_idx,
			&param_set->dma_input_hist.cmd,
			param_set->dma_input_hist.plane,
			param_set->input_dva_hist,
			dma_frame->ypsvhistTargetAddress);

	param_set->instance_id = instance;
	param_set->fcount = fcount;
	/* multi-buffer */
	hw_ip->num_buffers = dma_frame->num_buffers;
	batch_num = hw_ip->framemgr->batch_num;
	/* Update only for SW_FRO scenario */
	if (batch_num > 1 && hw_ip->num_buffers == 1) {
		hw_ip->num_buffers |= batch_num << SW_FRO_NUM_SHIFT;
		hw_ip->num_buffers |= cur_idx << CURR_INDEX_SHIFT;
	}

	if (IS_ENABLED(YPP_DDK_LIB_CALL)) {
		if (frame->shot) {
			ret = is_lib_isp_set_ctrl(hw_ip, &hw_ypp->lib[instance], frame);
			if (ret)
				mserr_hw("set_ctrl fail", instance, hw_ip);
		}

		if (hw_ip->lib_mode == USE_ONLY_DDK) {
			ret = is_lib_isp_shot(hw_ip, &hw_ypp->lib[instance], param_set,
					frame->shot);

			param_set->dma_input.cmd = cmd_input;
			set_bit(HW_CONFIG, &hw_ip->state);

			return ret;
		}
	}

	/* temp direct set*/
	set_id = COREX_DIRECT;

	ret = __is_hw_ypp_init_config(hw_ip, instance, frame, set_id);
	if (ret) {
		mserr_hw("is_hw_ypp_init_config is fail", instance, hw_ip);
		return ret;
	}

	if (IS_ENABLED(YPP_DDK_LIB_CALL)) {
		ret = is_lib_isp_shot(hw_ip, &hw_ypp->lib[instance], param_set, frame->shot);
		if (ret)
			return ret;

		ret = __is_hw_ypp_set_rta_regs(hw_ip->regs[REG_SETA], set_id, hw_ypp, instance);
		if (ret) {
			mserr_hw("__is_hw_ypp_set_rta_regs is fail", instance, hw_ip);
			goto shot_fail_recovery;
		}

		stripe_index = param_set->stripe_input.index;
		strip_start_pos = (stripe_index) ?
			(param_set->stripe_input.stripe_roi_start_pos_x[stripe_index] - STRIPE_MARGIN_WIDTH) : 0;

		ret = __is_hw_ypp_set_size_regs(hw_ip, param_set, instance, strip_start_pos, frame, set_id);
		if (ret) {
			mserr_hw("__is_hw_ypp_set_size_regs is fail", instance, hw_ip);
			goto shot_fail_recovery;
		}
	} else {
		spin_lock_irqsave(&ypp_out_slock, flag);

		__is_hw_ypp_update_block_reg(hw_ip, param_set, instance, set_id);

		spin_unlock_irqrestore(&ypp_out_slock, flag);

		ypp_hw_s_coutfifo_size(hw_ip->regs[REG_SETA], set_id,
			param_set->otf_output.width, param_set->otf_output.height, 0);
	}

	if (IS_ENABLED(YPP_DDK_LIB_CALL))
		max_dma_id = YUVPP_RDMA_MAX;
	else
		max_dma_id = YUVPP_RDMA_L2_Y;

	for (i = YUVPP_RDMA_L0_Y; i < max_dma_id; i++) {
		ret = __is_hw_ypp_set_rdma(hw_ip, hw_ypp, param_set, instance, i, set_id);
		if (ret) {
			mserr_hw("__is_hw_ypp_set_rdma is fail", instance, hw_ip);
			goto shot_fail_recovery;
		}
	}

	if (en_votf == OTF_INPUT_COMMAND_ENABLE) {
		ret = is_hw_ypp_set_votf_size_config(param_set->dma_input.width, param_set->dma_input.height);
		if (ret) {
			mserr_hw("is_hw_ypp_set_votf_size_config is fail", instance, hw_ip);
			goto shot_fail_recovery;
		}
	}

	ret = is_hw_ypp_set_cmdq(hw_ip, instance, set_id);
	if (ret) {
		mserr_hw("is_hw_ypp_set_cmdq is fail", instance, hw_ip);
		goto shot_fail_recovery;
	}

	if (unlikely(debug_ypp))
		ypp_hw_dump(hw_ip->regs[REG_SETA]);

	set_bit(HW_CONFIG, &hw_ip->state);

	return 0;

shot_fail_recovery:
	if (IS_ENABLED(YPP_DDK_LIB_CALL))
		is_lib_isp_reset_recovery(hw_ip, &hw_ypp->lib[instance], instance);

	return ret;
}

static int is_hw_ypp_get_meta(struct is_hw_ip *hw_ip, struct is_frame *frame,
	ulong hw_map)
{
	int ret = 0;
	struct is_hw_ypp *hw_ypp;

	FIMC_BUG(!hw_ip);
	FIMC_BUG(!frame);

	if (!test_bit_variables(hw_ip->id, &hw_map))
		return 0;

	FIMC_BUG(!hw_ip->priv_info);
	hw_ypp = (struct is_hw_ypp *)hw_ip->priv_info;
	if (unlikely(!hw_ypp)) {
		mserr_hw("priv_info is NULL", frame->instance, hw_ip);
		return -EINVAL;
	}

	if (IS_ENABLED(YPP_DDK_LIB_CALL)) {
		ret = is_lib_isp_get_meta(hw_ip, &hw_ypp->lib[frame->instance], frame);
		if (ret)
			mserr_hw("get_meta fail", frame->instance, hw_ip);
	}

	return ret;
}


static int is_hw_ypp_frame_ndone(struct is_hw_ip *hw_ip, struct is_frame *frame,
	u32 instance, enum ShotErrorType done_type)
{
	int ret = 0;
	int output_id;

	FIMC_BUG(!hw_ip);
	FIMC_BUG(!frame);

	output_id = IS_HW_CORE_END;
	if (test_bit(hw_ip->id, &frame->core_flag)) {
		ret = is_hardware_frame_done(hw_ip, frame, -1,
			output_id, done_type, false);
	}

	return ret;
}


static int is_hw_ypp_load_setfile(struct is_hw_ip *hw_ip, u32 instance, ulong hw_map)
{
	int ret = 0, flag = 0;
	ulong addr;
	u32 size, index;
	struct is_hw_ypp *hw_ypp = NULL;
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
	hw_ypp = (struct is_hw_ypp *)hw_ip->priv_info;
	if (IS_ENABLED(YPP_DDK_LIB_CALL)) {
		for (index = 0; index < setfile->using_count; index++) {
			addr = setfile->table[index].addr;
			size = setfile->table[index].size;
			ret = is_lib_isp_create_tune_set(&hw_ypp->lib[instance],
				addr, size, index, flag, instance);

			set_bit(index, &hw_ypp->lib[instance].tune_count);
		}
	}
	set_bit(HW_TUNESET, &hw_ip->state);

	return ret;
}

static int is_hw_ypp_apply_setfile(struct is_hw_ip *hw_ip, u32 scenario,
	u32 instance, ulong hw_map)
{
	int ret = 0;
	u32 setfile_index = 0;
	struct is_hw_ypp *hw_ypp = NULL;
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
	hw_ypp = (struct is_hw_ypp *)hw_ip->priv_info;
	if (IS_ENABLED(YPP_DDK_LIB_CALL))
		ret = is_lib_isp_apply_tune_set(&hw_ypp->lib[instance], setfile_index, instance);

	return ret;
}

static int is_hw_ypp_delete_setfile(struct is_hw_ip *hw_ip, u32 instance, ulong hw_map)
{
	struct is_hw_ypp *hw_ypp = NULL;
	int ret = 0;
	int i = 0;
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
	hw_ypp = (struct is_hw_ypp *)hw_ip->priv_info;
	if (IS_ENABLED(YPP_DDK_LIB_CALL)) {
		for (i = 0; i < setfile->using_count; i++) {
			if (test_bit(i, &hw_ypp->lib[instance].tune_count)) {
				ret = is_lib_isp_delete_tune_set(&hw_ypp->lib[instance],
					(u32)i, instance);
				clear_bit(i, &hw_ypp->lib[instance].tune_count);
			}
		}
	}
	clear_bit(HW_TUNESET, &hw_ip->state);

	return ret;
}

int is_hw_ypp_restore(struct is_hw_ip *hw_ip, u32 instance)
{
	int ret = 0;
	struct is_hw_ypp *hw_ypp = NULL;
	struct is_group *group;

	FIMC_BUG(!hw_ip);
	FIMC_BUG(!hw_ip->priv_info);

	if (!test_bit(HW_OPEN, &hw_ip->state))
		return -EINVAL;

	hw_ypp = (struct is_hw_ypp *)hw_ip->priv_info;

	if (hw_ip->lib_mode == USE_DRIVER) {
		msinfo_hw("hw_ypp_reset\n", instance, hw_ip);

		ret = is_hw_ypp_reset_votf();
		if (ret) {
			mserr_hw("votf reset fail", instance, hw_ip);
			return ret;
		}

		if (__is_hw_ypp_s_common_reg(hw_ip, instance)) {
			mserr_hw("sw reset fail", instance, hw_ip);
			return -ENODEV;
		}

		group = hw_ip->group[instance];
		is_votf_destroy_link(group);
		is_votf_create_link(group, 0, 0);
	}

	if (IS_ENABLED(YPP_DDK_LIB_CALL)) {
		ret = is_lib_isp_reset_recovery(hw_ip, &hw_ypp->lib[instance], instance);
		if (ret) {
			mserr_hw("is_lib_ypp_reset_recovery fail ret(%d)",
					instance, hw_ip, ret);
		}
	}

	return ret;
}

int is_hw_ypp_set_regs(struct is_hw_ip *hw_ip, u32 instance, u32 fcount, struct cr_set *regs, u32 regs_size)
{
	struct is_hw_ypp *hw_ypp = NULL;
	struct is_hw_ypp_iq *iq_set = NULL;
	ulong flag = 0;

	FIMC_BUG(!hw_ip);
	FIMC_BUG(!hw_ip->priv_info);
	FIMC_BUG(!regs);

	hw_ypp = (struct is_hw_ypp *)hw_ip->priv_info;
	iq_set = &hw_ypp->iq_set[instance];

	if (!test_and_clear_bit(CR_SET_EMPTY, &iq_set->state))
		return -EBUSY;

	spin_lock_irqsave(&iq_set->slock, flag);

	iq_set->size = regs_size;
	memcpy((void *)iq_set->regs, (void *)regs, sizeof(struct cr_set) * regs_size);

	spin_unlock_irqrestore(&iq_set->slock, flag);
	set_bit(CR_SET_CONFIG, &iq_set->state);

	msdbg_hw(2, "Store IQ regs set: %p, size(%d)\n", instance, hw_ip, regs, regs_size);

	return instance;
}

int is_hw_ypp_dump_regs(struct is_hw_ip *hw_ip, u32 instance, u32 fcount, struct cr_set *regs, u32 regs_size)
{
	void __iomem *base = NULL;
	int i;

	FIMC_BUG(!hw_ip);
	FIMC_BUG(!regs);

	base = hw_ip->regs[REG_SETA];
	for (i = 0; i < regs_size; i++) {
		regs[i].reg_data = readl(base + regs[i].reg_addr);
		msinfo_hw("<0x%x, 0x%x>\n", instance, hw_ip,
				regs[i].reg_addr, regs[i].reg_data);
	}

	return instance;
}

int is_hw_ypp_set_config(struct is_hw_ip *hw_ip, u32 instance, u32 fcount, struct is_yuvpp_config *conf)
{
	struct is_hw_ypp *hw_ypp = NULL;
	int ret = 0;

	FIMC_BUG(!hw_ip);
	FIMC_BUG(!hw_ip->priv_info);
	FIMC_BUG(!conf);

	hw_ypp = (struct is_hw_ypp *)hw_ip->priv_info;
	hw_ypp->config = *conf;


	ret = __is_hw_ypp_set_rdma_cmd(&hw_ypp->param_set[instance], instance, conf);
	if (!ret)
		return -EINVAL;

	return ret;
}

static int is_hw_ypp_notify_timeout(struct is_hw_ip *hw_ip, u32 instance)
{
	int ret = 0;
	struct is_hw_ypp *hw_ypp = NULL;

	hw_ypp = (struct is_hw_ypp *)hw_ip->priv_info;
	if (!hw_ypp) {
		mserr_hw("failed to get HW YPP", instance, hw_ip);
		return -ENODEV;
	}

	ret = is_lib_notify_timeout(&hw_ypp->lib[instance], instance);

	return ret;
}

const struct is_hw_ip_ops is_hw_ypp_ops = {
	.open			= is_hw_ypp_open,
	.init			= is_hw_ypp_init,
	.deinit			= is_hw_ypp_deinit,
	.close			= is_hw_ypp_close,
	.enable			= is_hw_ypp_enable,
	.disable		= is_hw_ypp_disable,
	.shot			= is_hw_ypp_shot,
	.set_param		= is_hw_ypp_set_param,
	.get_meta		= is_hw_ypp_get_meta,
	.frame_ndone		= is_hw_ypp_frame_ndone,
	.load_setfile		= is_hw_ypp_load_setfile,
	.apply_setfile		= is_hw_ypp_apply_setfile,
	.delete_setfile		= is_hw_ypp_delete_setfile,
	.restore		= is_hw_ypp_restore,
	.set_regs		= is_hw_ypp_set_regs,
	.dump_regs		= is_hw_ypp_dump_regs,
	.set_config		= is_hw_ypp_set_config,
	.notify_timeout		= is_hw_ypp_notify_timeout,
};

int is_hw_ypp_probe(struct is_hw_ip *hw_ip, struct is_interface *itf,
	struct is_interface_ischain *itfc, int id, const char *name)
{
	int ret = 0;
	int hw_slot = -1;

	FIMC_BUG(!hw_ip);
	FIMC_BUG(!itf);
	FIMC_BUG(!itfc);

	/* initialize device hardware */
	hw_ip->id   = id;
	snprintf(hw_ip->name, sizeof(hw_ip->name), "%s", name);
	hw_ip->ops  = &is_hw_ypp_ops;
	hw_ip->itf  = itf;
	hw_ip->itfc = itfc;
	atomic_set(&hw_ip->fcount, 0);
	hw_ip->is_leader = true;
	atomic_set(&hw_ip->status.Vvalid, V_BLANK);
	atomic_set(&hw_ip->rsccount, 0);
	atomic_set(&hw_ip->run_rsccount, 0);
	init_waitqueue_head(&hw_ip->status.wait_queue);

	hw_slot = is_hw_slot_id(id);
	if (!valid_hw_slot_id(hw_slot)) {
		serr_hw("invalid hw_slot (%d)", hw_ip, hw_slot);
		return -EINVAL;
	}

	itfc->itf_ip[hw_slot].handler[INTR_HWIP5].handler = &is_hw_ypp_handle_interrupt;
	itfc->itf_ip[hw_slot].handler[INTR_HWIP5].id = INTR_HWIP5;
	itfc->itf_ip[hw_slot].handler[INTR_HWIP5].valid = true;

	clear_bit(HW_OPEN, &hw_ip->state);
	clear_bit(HW_INIT, &hw_ip->state);
	clear_bit(HW_CONFIG, &hw_ip->state);
	clear_bit(HW_RUN, &hw_ip->state);
	clear_bit(HW_TUNESET, &hw_ip->state);

	spin_lock_init(&ypp_out_slock);
	sinfo_hw("probe done\n", hw_ip);

	return ret;
}
