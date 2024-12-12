// SPDX-License-Identifier: GPL-2.0
/*
 * Samsung Exynos SoC series Pablo driver
 *
 * Copyright (c) 2020 Samsung Electronics Co., Ltd
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#include "is-hw-vra-v2.h"
#include "../interface/is-interface-ischain.h"
#include "is-err.h"

void is_hw_vra_save_debug_info(struct is_hw_ip *hw_ip,
	struct is_lib_vra *lib_vra, int debug_point)
{
	struct is_hardware *hardware;
	u32 hw_fcount, index, instance;

	FIMC_BUG_VOID(!hw_ip);

	hardware = hw_ip->hardware;
	instance = atomic_read(&hw_ip->instance);
	hw_fcount = atomic_read(&hw_ip->fcount);

	switch (debug_point) {
	case DEBUG_POINT_FRAME_START:
		_is_hw_frame_dbg_trace(hw_ip, hw_fcount, DEBUG_POINT_FRAME_START);

		if (!atomic_read(&hardware->streaming[hardware->sensor_position[instance]])
			|| test_bit(VRA_LIB_BYPASS_REQUESTED, &lib_vra->state))
			msinfo_hw("[F:%d]F.S\n", instance, hw_ip, hw_fcount);
		break;
	case DEBUG_POINT_FRAME_END:
		_is_hw_frame_dbg_trace(hw_ip, hw_fcount, DEBUG_POINT_FRAME_END);

		index = hw_ip->debug_index[1];
		dbg_isr_hw("[F:%d][S-E] %05llu us\n", hw_ip, hw_fcount,
			(hw_ip->debug_info[index].time[DEBUG_POINT_FRAME_END] -
			hw_ip->debug_info[index].time[DEBUG_POINT_FRAME_START]) / 1000);

		if (!atomic_read(&hardware->streaming[hardware->sensor_position[instance]]))
			msinfo_hw("[F:%d]F.E\n", instance, hw_ip, hw_fcount);

		if (atomic_read(&hw_ip->count.fs) < atomic_read(&hw_ip->count.fe)) {
			mserr_hw("fs(%d), fe(%d), dma(%d)", instance, hw_ip,
				atomic_read(&hw_ip->count.fs),
				atomic_read(&hw_ip->count.fe),
				atomic_read(&hw_ip->count.dma));
		}
		break;
	default:
		break;
	}
	return;
}

int is_hw_vra_frame_end_final_out_ready_done(struct is_lib_vra *lib_vra)
{
	u32 ret = 0;
	struct is_hw_ip *hw_ip = NULL;

	hw_ip = (struct is_hw_ip *)lib_vra->hw_ip;
	/* for VRA tracker & 10 fps operation : Frame done */
	atomic_inc(&hw_ip->count.dma);
	atomic_inc(&hw_ip->count.fe);
	atomic_set(&hw_ip->status.Vvalid, V_BLANK);
	is_hw_vra_save_debug_info(hw_ip, lib_vra, DEBUG_POINT_FRAME_END);

	msdbg_hw(2, "VRA FE & Final done\n", atomic_read(&hw_ip->instance), hw_ip);

	ret = is_hardware_frame_done(hw_ip, NULL, -1,
		IS_HW_CORE_END, IS_SHOT_SUCCESS, true);

	wake_up(&hw_ip->status.wait_queue);

	return ret;
}

static int is_hw_vra_handle_interrupt(u32 id, void *context)
{
	int ret = 0;
	u32 status, intr_mask, intr_status;
	struct is_hardware *hardware;
	struct is_hw_ip *hw_ip = NULL;
	struct is_hw_vra *hw_vra = NULL;
	struct is_lib_vra *lib_vra;
	u32 instance;
	u32 hw_fcount;

	FIMC_BUG(!context);
	hw_ip = (struct is_hw_ip *)context;
	hardware = hw_ip->hardware;
	instance = atomic_read(&hw_ip->instance);
	hw_fcount = atomic_read(&hw_ip->fcount);

	FIMC_BUG(!hw_ip->priv_info);
	hw_vra = (struct is_hw_vra *)hw_ip->priv_info;
	if (unlikely(!hw_vra)) {
		serr_hw("isr: hw_vra is null", hw_ip);
		return -EINVAL;
	}
	lib_vra = &hw_vra->lib_vra;

	intr_status = is_vra_get_all_intr(hw_ip->regs[REG_SETA]);
	intr_mask   = is_vra_get_enable_intr(hw_ip->regs[REG_SETA]);
	status = (intr_mask) & intr_status;
	msdbg_hw(2, "isr: id(%d), status(0x%x), mask(0x%x), status(0x%x)\n",
		instance, hw_ip, id, intr_status, intr_mask, status);

	ret = is_lib_vra_handle_interrupt(&hw_vra->lib_vra, id);
	if (ret) {
		mserr_hw("lib_vra_handle_interrupt is fail (%d)\n" \
			"status(0x%x), mask(0x%x), status(0x%x)", instance, hw_ip,
			ret, intr_status, intr_mask, status);
	}

	if (status & (1 << CNN_END_INT)) {
		status &= (~(1 << CNN_END_INT));
		if (!atomic_read(&hardware->streaming[hardware->sensor_position[instance]])
			|| test_bit(VRA_LIB_BYPASS_REQUESTED, &lib_vra->state))
			msinfo_hw("END_OF_CONTEXT [F:%d]\n", instance, hw_ip,
				atomic_read(&hw_ip->fcount));

		msdbg_hw(2, "END_OF_CONTEXT\n", instance, hw_ip);
		/* frame_done : check is_hw_vra_frame_end_final_out_ready_done() */
	}

	if (status)
		msinfo_hw("error! status(0x%x)\n", instance, hw_ip, intr_status);

	return ret;
}

static void is_hw_vra_reset(struct is_hw_ip *hw_ip)
{
	u32 all_intr;

	/* Interrupt clear */
	all_intr = is_vra_get_all_intr(hw_ip->regs[REG_SETA]);
	is_vra_set_clear_intr(hw_ip->regs, all_intr);
}

static int __nocfi is_hw_vra_open(struct is_hw_ip *hw_ip, u32 instance,
	struct is_group *group)
{
	int ret = 0;
	int ret_err = 0;
	ulong dma_addr;
	struct is_hw_vra *hw_vra = NULL;

	FIMC_BUG(!hw_ip);

	if (test_bit(HW_OPEN, &hw_ip->state))
		return 0;

	frame_manager_probe(hw_ip->framemgr, hw_ip->id, "HWVRA");
	frame_manager_open(hw_ip->framemgr, IS_MAX_HW_FRAME);

	hw_ip->priv_info = vzalloc(sizeof(struct is_hw_vra));
	if(!hw_ip->priv_info) {
		mserr_hw("hw_ip->priv_info(null)", instance, hw_ip);
		ret = -ENOMEM;
		goto err_alloc;
	}

	hw_vra = (struct is_hw_vra *)hw_ip->priv_info;

	is_hw_vra_reset(hw_ip);

	is_fpsimd_get_func();
	get_lib_vra_func((void *)&hw_vra->lib_vra.itf_func);
	is_fpsimd_put_func();

	msdbg_hw(2, "library interface was initialized\n", instance, hw_ip);

	dma_addr = group->device->minfo->kvaddr_vra;
	ret = is_lib_vra_alloc_memory(&hw_vra->lib_vra, dma_addr);
	if (ret) {
		mserr_hw("failed to alloc. memory", instance, hw_ip);
		goto err_vra_alloc_memory;
	}

	ret = is_lib_vra_frame_work_init(&hw_vra->lib_vra, hw_ip->regs[REG_SETA]);
	if (ret) {
		mserr_hw("failed to init. framework (%d)", instance, hw_ip, ret);
		goto err_vra_frame_work_init;
	}

	ret = is_lib_vra_init_task(&hw_vra->lib_vra);
	if (ret) {
		mserr_hw("failed to init. task(%d)", instance, hw_ip, ret);
		goto err_vra_init_task;
	}

	hw_vra->lib_vra.hw_ip = hw_ip;

#if defined(VRA_DMA_TEST_BY_IMAGE)
	ret = is_lib_vra_test_image_load(&hw_vra->lib_vra);
	if (ret) {
		mserr_hw("failed to load test image (%d)", instance, hw_ip, ret);
		return ret;
	}
#endif

	set_bit(HW_OPEN, &hw_ip->state);
	msdbg_hw(2, "open: [G:0x%x], framemgr[%s]", instance, hw_ip,
		GROUP_ID(group->id), hw_ip->framemgr->name);

	return 0;

err_vra_init_task:
	ret_err = is_lib_vra_frame_work_final(&hw_vra->lib_vra);
	if (ret_err)
		mserr_hw("lib_vra_destory_object is fail (%d)", instance, hw_ip, ret_err);
err_vra_frame_work_init:
	ret_err = is_lib_vra_free_memory(&hw_vra->lib_vra);
	if (ret_err)
		mserr_hw("lib_vra_free_memory is fail (%d)", instance, hw_ip, ret_err);
err_vra_alloc_memory:
	vfree(hw_ip->priv_info);
	hw_ip->priv_info = NULL;
err_alloc:
	frame_manager_close(hw_ip->framemgr);
	return ret;
}

static int is_hw_vra_init(struct is_hw_ip *hw_ip, u32 instance,
	struct is_group *group, bool flag, u32 module_id)
{
	int ret = 0;
	struct is_hw_vra *hw_vra = NULL;
	struct is_lib_vra *lib_vra;

	FIMC_BUG(!hw_ip);
	FIMC_BUG(!hw_ip->priv_info);
	FIMC_BUG(!group);

	hw_vra = (struct is_hw_vra *)hw_ip->priv_info;
	lib_vra = &hw_vra->lib_vra;

	lib_vra->max_face_num = MAX_FACE_COUNT;

	set_bit(HW_INIT, &hw_ip->state);
	msdbg_hw(2, "ready to start\n", instance, hw_ip);

	return ret;
}

static int is_hw_vra_deinit(struct is_hw_ip *hw_ip, u32 instance)
{
	return 0;
}

static int is_hw_vra_close(struct is_hw_ip *hw_ip, u32 instance)
{
	int ret = 0;
	struct is_hw_vra *hw_vra = NULL;

	FIMC_BUG(!hw_ip);

	if (!test_bit(HW_OPEN, &hw_ip->state))
		return 0;

	hw_vra = (struct is_hw_vra *)hw_ip->priv_info;
	if (unlikely(!hw_vra)) {
		mserr_hw("priv_info is NULL", instance, hw_ip);
		return -EINVAL;
	}

	ret = is_lib_vra_frame_work_final(&hw_vra->lib_vra);
	if (ret) {
		mserr_hw("lib_vra_destory_object is fail (%d)", instance, hw_ip, ret);
		return ret;
	}

	ret = is_lib_vra_free_memory(&hw_vra->lib_vra);
	if (ret) {
		mserr_hw("lib_vra_free_memory is fail", instance, hw_ip);
		return ret;
	}

	vfree(hw_ip->priv_info);
	hw_ip->priv_info = NULL;
	frame_manager_close(hw_ip->framemgr);

	clear_bit(HW_OPEN, &hw_ip->state);

	return ret;
}

static int is_hw_vra_enable(struct is_hw_ip *hw_ip, u32 instance,
	ulong hw_map)
{
	int ret = 0;

	FIMC_BUG(!hw_ip);

	if (!test_bit_variables(hw_ip->id, &hw_map))
		return 0;

	if (!test_bit(HW_INIT, &hw_ip->state)) {
		mserr_hw("not initialized!!", instance, hw_ip);
		return -EINVAL;
	}

	set_bit(HW_RUN, &hw_ip->state);
	atomic_inc(&hw_ip->run_rsccount);

	return ret;
}

static int is_hw_vra_disable(struct is_hw_ip *hw_ip, u32 instance,
	ulong hw_map)
{
	int ret = 0;
	long timetowait;
	struct is_hw_vra *hw_vra = NULL;

	FIMC_BUG(!hw_ip);

	if (!test_bit_variables(hw_ip->id, &hw_map))
		return 0;

	if (!test_bit(HW_INIT, &hw_ip->state)) {
		mserr_hw("not initialized!!", instance, hw_ip);
		return -EINVAL;
	}

	hw_vra = (struct is_hw_vra *)hw_ip->priv_info;
	if (unlikely(!hw_vra)) {
		mserr_hw("priv_info is NULL", instance, hw_ip);
		return -EINVAL;
	}

	ret = is_lib_vra_stop_instance(&hw_vra->lib_vra, instance);
	if (ret) {
		mserr_hw("lib_vra_stop_instance is fail (%d)", instance, hw_ip, ret);
		return ret;
	}

	if (atomic_dec_return(&hw_ip->run_rsccount) > 0)
		return 0;

	msinfo_hw("vra_disable: Vvalid(%d)\n", instance, hw_ip,
		atomic_read(&hw_ip->status.Vvalid));

	if (test_bit(HW_RUN, &hw_ip->state)) {
		timetowait = wait_event_timeout(hw_ip->status.wait_queue,
			!atomic_read(&hw_ip->status.Vvalid),
			IS_HW_STOP_TIMEOUT);

		if (!timetowait)
			mserr_hw("wait FRAME_END timeout (%ld)", instance, hw_ip, timetowait);

		ret = is_lib_vra_stop(&hw_vra->lib_vra);
		if (ret) {
			mserr_hw("lib_vra_stop is fail (%d)", instance, hw_ip, ret);
			return ret;
		}

		is_hw_vra_reset(hw_ip);
		clear_bit(HW_RUN, &hw_ip->state);
		clear_bit(HW_CONFIG, &hw_ip->state);
	} else {
		msdbg_hw(2, "already disabled\n", instance, hw_ip);
	}

	return 0;
}

static int is_hw_vra_update_param(struct is_hw_ip *hw_ip,
	struct vra_param *param, u32 lindex, u32 hindex, u32 instance, u32 fcount)
{
	int ret = 0;
	struct is_hw_vra *hw_vra;
	struct is_lib_vra *lib_vra;

	FIMC_BUG(!hw_ip);
	FIMC_BUG(!param);

	hw_vra = (struct is_hw_vra *)hw_ip->priv_info;

	lib_vra = &hw_vra->lib_vra;

#ifdef VRA_DMA_TEST_BY_IMAGE
	ret = is_lib_vra_test_input(lib_vra, instance);
	if (ret) {
		mserr_hw("test_input is fail (%d)", instance, hw_ip, ret);
		return ret;
	}
	return 0;
#endif

	if (param->dma_input.cmd == DMA_INPUT_COMMAND_ENABLE) {
		if ((lindex & LOWBIT_OF(PARAM_FD_DMA_INPUT))
			|| (hindex & HIGHBIT_OF(PARAM_FD_DMA_INPUT))) {
			ret = is_lib_vra_dma_input(lib_vra, param, instance, fcount);
			if (ret) {
				mserr_hw("dma_input is fail (%d)", instance, hw_ip, ret);
				return ret;
			}
		}
	} else {
		mserr_hw("param setting is wrong! otf_input.cmd(%d), dma_input.cmd(%d)",
			instance, hw_ip, param->otf_input.cmd, param->dma_input.cmd);
		return -EINVAL;
	}

	return ret;
}

static int is_hw_vra_update_crop_info(struct is_lib_vra *lib_vra,
		struct is_frame *frame, u32 instance)
{
	int ret = 0;

	FIMC_BUG(!lib_vra);
	FIMC_BUG(!frame);
	FIMC_BUG(!frame->shot_ext);

	lib_vra->in_sizes[instance].org_crop_info.crop_offset_x =
		frame->shot_ext->vra_ext.org_crop_info.crop_offset_x;
	lib_vra->in_sizes[instance].org_crop_info.crop_offset_y =
		frame->shot_ext->vra_ext.org_crop_info.crop_offset_y;
	lib_vra->in_sizes[instance].org_crop_info.crop_width =
		frame->shot_ext->vra_ext.org_crop_info.crop_width;
	lib_vra->in_sizes[instance].org_crop_info.crop_height =
		frame->shot_ext->vra_ext.org_crop_info.crop_height;
	lib_vra->in_sizes[instance].org_crop_info.full_width =
		frame->shot_ext->vra_ext.org_crop_info.full_width;
	lib_vra->in_sizes[instance].org_crop_info.full_height =
		frame->shot_ext->vra_ext.org_crop_info.full_height;

	return ret;
}

static int is_hw_vra_shot(struct is_hw_ip *hw_ip, struct is_frame *frame,
	ulong hw_map)
{
	int ret = 0;
	struct is_hw_vra *hw_vra = NULL;
	struct vra_param *param;
	struct is_lib_vra *lib_vra;
	u32 instance, lindex, hindex;
	unsigned char *buffer_kva = NULL;
	unsigned char *buffer_dva = NULL;

	FIMC_BUG(!hw_ip);
	FIMC_BUG(!frame);

	/* multi-buffer */
	hw_ip->cur_s_int = 0;
	hw_ip->cur_e_int = 0;
	if (frame->num_buffers)
		hw_ip->num_buffers = frame->num_buffers;

	instance = frame->instance;
	msdbgs_hw(2, "[F:%d] shot\n", instance, hw_ip, frame->fcount);

	hw_vra  = (struct is_hw_vra *)hw_ip->priv_info;
	param   = &hw_ip->region[instance]->parameter.vra;
	lib_vra = &hw_vra->lib_vra;

	if (!test_bit_variables(hw_ip->id, &hw_map))
		return 0;

	if (!test_bit(HW_INIT, &hw_ip->state)) {
		msdbg_hw(2, "not initialized\n", instance, hw_ip);
		return -EINVAL;
	}

	if (frame->type == SHOT_TYPE_INTERNAL) {
		msdbg_hw(2, "request not exist\n", instance, hw_ip);
		goto new_frame;
	}

	FIMC_BUG(!frame->shot);
	/* per-frame control
	 * check & update size from region */
	lindex = frame->shot->ctl.vendor_entry.lowIndexParam;
	hindex = frame->shot->ctl.vendor_entry.highIndexParam;

#if !defined(VRA_DMA_TEST_BY_IMAGE)
	ret = is_hw_vra_update_param(hw_ip, param, lindex, hindex,
			instance, frame->fcount);
	if (ret) {
		mserr_hw("lib_vra_update_param is fail (%d)", instance, hw_ip, ret);
		return ret;
	}

	ret = is_hw_vra_update_crop_info(lib_vra, frame, instance);
	if (ret) {
		mserr_hw("lib_vra_update_crop_info is fail (%d)", instance, hw_ip, ret);
		return ret;
	}
#endif

new_frame:
	/*
	 * DMA mode: the buffer value is VRA input DMA address.
	 */
	/* VRA input is always DMA Input only. */
	/* TODO: It is required to support other YUV format */
	buffer_kva = (unsigned char *)
		(frame->kvaddr_buffer[frame->cur_buf_index * 2]);
	buffer_dva = (unsigned char *)
		(frame->dvaddr_buffer[frame->cur_buf_index * 2]);
	hw_ip->mframe = frame;

	msdbg_hw(2, "[F:%d]lib_vra_new_frame\n", instance, hw_ip, frame->fcount);
	lib_vra->fr_index = frame->fcount;
	ret = is_lib_vra_new_frame(lib_vra, buffer_kva, buffer_dva, instance);
	if (ret) {
		mserr_hw("lib_vra_new_frame is fail (%d)", instance, hw_ip, ret);
		msinfo_hw("count fs(%d), fe(%d), dma(%d)\n", instance, hw_ip,
			atomic_read(&hw_ip->count.fs),
			atomic_read(&hw_ip->count.fe),
			atomic_read(&hw_ip->count.dma));
		return ret;
	}

	/* for Frame_start - no F.S interrupt */
	atomic_inc(&hw_ip->count.fs);
	is_hardware_frame_start(hw_ip, instance);
	is_hw_vra_save_debug_info(hw_ip, lib_vra, DEBUG_POINT_FRAME_START);
	atomic_set(&hw_ip->status.Vvalid, V_VALID);
	clear_bit(HW_CONFIG, &hw_ip->state);

#if !defined(VRA_DMA_TEST_BY_IMAGE)
	set_bit(hw_ip->id, &frame->core_flag);
#endif
	set_bit(HW_CONFIG, &hw_ip->state);

	if (lib_vra->skip_hw)
		ret = is_lib_vra_run_tracker(&hw_vra->lib_vra, instance, frame->fcount);

	return 0;
}

static int is_hw_vra_set_param(struct is_hw_ip *hw_ip,
	struct is_region *region, u32 lindex, u32 hindex, u32 instance,
	ulong hw_map)
{
	int ret = 0;
	struct is_hw_vra *hw_vra;
	struct vra_param *param;
	struct is_lib_vra *lib_vra;
	u32 fcount;

	FIMC_BUG(!hw_ip);
	FIMC_BUG(!region);

	if (!test_bit_variables(hw_ip->id, &hw_map))
		return 0;

	if (!test_bit(HW_INIT, &hw_ip->state)) {
		mserr_hw("not initialized!!", instance, hw_ip);
		return -EINVAL;
	}

	fcount = atomic_read(&hw_ip->fcount);

	hw_ip->region[instance] = region;
	hw_ip->lindex[instance] = lindex;
	hw_ip->hindex[instance] = hindex;

	param   = &region->parameter.vra;
	hw_vra  = (struct is_hw_vra *)hw_ip->priv_info;
	lib_vra = &hw_vra->lib_vra;

	msdbg_hw(2, "set_param \n", instance, hw_ip);
	ret = is_hw_vra_update_param(hw_ip, param,
			lindex, hindex, instance, fcount);
	if (ret) {
		mserr_hw("set_param is fail (%d)", instance, hw_ip, ret);
		return ret;
	}

	return ret;
}

static int is_hw_vra_frame_ndone(struct is_hw_ip *hw_ip,
	struct is_frame *frame, u32 instance, enum ShotErrorType done_type)
{
	int ret = 0;
	int wq_id;
	int output_id;
	struct is_hw_vra *hw_vra;

	FIMC_BUG(!hw_ip);
	FIMC_BUG(!frame);

	hw_vra = (struct is_hw_vra *)hw_ip->priv_info;

	wq_id     = -1;
	output_id = IS_HW_CORE_END;
	if (test_bit_variables(hw_ip->id, &frame->core_flag))
		ret = is_hardware_frame_done(hw_ip, frame, wq_id, output_id,
				done_type, false);

	if (done_type == IS_SHOT_TIMEOUT)
		ret = is_lib_vra_sw_reset(&hw_vra->lib_vra);

	return ret;
}

static int is_hw_vra_load_setfile(struct is_hw_ip *hw_ip, u32 instance, ulong hw_map)
{
	return 0;
}

static int is_hw_vra_apply_setfile(struct is_hw_ip *hw_ip, u32 scenario,
	u32 instance, ulong hw_map)
{
	return 0;
}
static int is_hw_vra_delete_setfile(struct is_hw_ip *hw_ip, u32 instance,
		ulong hw_map)
{
	clear_bit(HW_TUNESET, &hw_ip->state);

	return 0;
}

static int is_hw_vra_get_meta(struct is_hw_ip *hw_ip,
		struct is_frame *frame, unsigned long hw_map)
{
	int ret = 0;
	struct is_hw_vra *hw_vra;

	if (unlikely(!frame)) {
		mserr_hw("get_meta: frame is null", atomic_read(&hw_ip->instance), hw_ip);
		return 0;
	}

	if (!test_bit_variables(hw_ip->id, &hw_map))
		return 0;

	hw_vra = (struct is_hw_vra *)hw_ip->priv_info;
	if (unlikely(!hw_vra)) {
		mserr_hw("priv_info is NULL", frame->instance, hw_ip);
		return -EINVAL;
	}

	ret = is_lib_vra_get_meta(&hw_vra->lib_vra, frame);
	if (ret)
		mserr_hw("get_meta is fail (%d)", frame->instance, hw_ip, ret);

	return ret;
}

const struct is_hw_ip_ops is_hw_vra_ops = {
	.open			= is_hw_vra_open,
	.init			= is_hw_vra_init,
	.deinit			= is_hw_vra_deinit,
	.close			= is_hw_vra_close,
	.enable			= is_hw_vra_enable,
	.disable		= is_hw_vra_disable,
	.shot			= is_hw_vra_shot,
	.set_param		= is_hw_vra_set_param,
	.get_meta		= is_hw_vra_get_meta,
	.frame_ndone		= is_hw_vra_frame_ndone,
	.load_setfile		= is_hw_vra_load_setfile,
	.apply_setfile		= is_hw_vra_apply_setfile,
	.delete_setfile		= is_hw_vra_delete_setfile,
};

int is_hw_vra_probe(struct is_hw_ip *hw_ip, struct is_interface *itf,
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
	hw_ip->ops  = &is_hw_vra_ops;
	hw_ip->itf  = itf;
	hw_ip->itfc = itfc;
	atomic_set(&hw_ip->fcount, 0);
	hw_ip->is_leader = true;
	atomic_set(&hw_ip->status.Vvalid, V_BLANK);
	atomic_set(&hw_ip->rsccount, 0);
	atomic_set(&hw_ip->run_rsccount, 0);
	init_waitqueue_head(&hw_ip->status.wait_queue);

	/* set fd sfr base address */
	hw_slot = is_hw_slot_id(id);
	if (!valid_hw_slot_id(hw_slot)) {
		serr_hw("invalid hw_slot (%d)", hw_ip, hw_slot);
		return 0;
	}

	itfc->itf_ip[hw_slot].handler[INTR_HWIP1].handler = &is_hw_vra_handle_interrupt;

	clear_bit(HW_OPEN, &hw_ip->state);
	clear_bit(HW_INIT, &hw_ip->state);
	clear_bit(HW_CONFIG, &hw_ip->state);
	clear_bit(HW_RUN, &hw_ip->state);
	clear_bit(HW_TUNESET, &hw_ip->state);

	sinfo_hw("probe done\n", hw_ip);

	return ret;
}
