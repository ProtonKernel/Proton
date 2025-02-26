/*
 * Samsung EXYNOS FIMC-IS (Imaging Subsystem) driver
 *
 * Copyright (C) 2014 Samsung Electronics Co., Ltd.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#include "is-interface-ddk.h"
#include "is-hw-control.h"
#include "sfr/is-sfr-isp-v310.h"
#include "is-err.h"
#include <soc/samsung/exynos-sci_dbg.h>
#include "is-hw-3aa.h"

int debug_irq_ddk;
module_param(debug_irq_ddk, int, 0644);
int debug_time_hw;
module_param(debug_time_hw, int, 0644);

bool check_dma_done(struct is_hw_ip *hw_ip, u32 instance_id, u32 fcount)
{
	bool ret = false;
	struct is_frame *frame;
	struct is_framemgr *framemgr;
	int output_id = 0;
	u32 hw_fcount;
	ulong flags = 0;
	u32 queued_count;

	FIMC_BUG(!hw_ip);

	framemgr = hw_ip->framemgr;
	hw_fcount = atomic_read(&hw_ip->fcount);

	FIMC_BUG(!framemgr);

flush_wait_done_frame:
	framemgr_e_barrier_common(framemgr, 0, flags);
	frame = peek_frame(framemgr, FS_HW_WAIT_DONE);
	queued_count = framemgr->queued_count[FS_HW_WAIT_DONE];
	framemgr_x_barrier_common(framemgr, 0, flags);

	if (frame) {
		if (frame->type == SHOT_TYPE_LATE) {
			msinfo_hw("[F:%d,FF:%d,HWF:%d][WD%d] flush LATE_SHOT\n",
				instance_id, hw_ip, fcount, frame->fcount, hw_fcount,
				queued_count);

			ret = is_hardware_frame_ndone(hw_ip, frame, frame->instance,
				IS_SHOT_LATE_FRAME);
			if (ret) {
				mserr_hw("hardware_frame_ndone fail(LATE_SHOT)",
					frame->instance, hw_ip);
				return true;
			}

			goto flush_wait_done_frame;
		} else {
			if (unlikely(frame->fcount < fcount)) {
				/* Flush the old frame which is in HW_WAIT_DONE state & retry. */
				mswarn_hw("[F:%d,FF:%d,HWF:%d][WD%d] invalid frame(idx:%d)",
					instance_id, hw_ip, fcount, frame->fcount, hw_fcount,
					queued_count, frame->cur_buf_index);

				framemgr_e_barrier_common(framemgr, 0, flags);
				frame_manager_print_info_queues(framemgr);
				framemgr_x_barrier_common(framemgr, 0, flags);

				ret = is_hardware_frame_ndone(hw_ip, frame, frame->instance,
					IS_SHOT_INVALID_FRAMENUMBER);
				if (ret) {
					mserr_hw("hardware_frame_ndone fail(old frame)",
						frame->instance, hw_ip);
					return true;
				}

				goto flush_wait_done_frame;
			} else if (unlikely(frame->fcount > fcount)) {
				mswarn_hw("[F:%d,FF:%d,HWF:%d][WD%d] Too early frame. Skip it.",
					instance_id, hw_ip, fcount, frame->fcount, hw_fcount,
					queued_count);

				framemgr_e_barrier_common(framemgr, 0, flags);
				frame_manager_print_info_queues(framemgr);
				framemgr_x_barrier_common(framemgr, 0, flags);

				return true;
			}
		}
	} else {
		/* Flush the old frame which is in HW_CONFIGURE state & skip dma_done. */
flush_config_frame:
		framemgr_e_barrier_common(framemgr, 0, flags);
		frame = peek_frame(framemgr, FS_HW_CONFIGURE);
		if (frame) {
			if (unlikely(frame->fcount < hw_fcount)) {
				trans_frame(framemgr, frame, FS_HW_WAIT_DONE);
				framemgr_x_barrier_common(framemgr, 0, flags);

				mserr_hw("[F:%d,FF:%d,HWF:%d] late config frame",
					instance_id, hw_ip, fcount, frame->fcount, hw_fcount);

				is_hardware_frame_ndone(hw_ip, frame, frame->instance, IS_SHOT_INVALID_FRAMENUMBER);
				goto flush_config_frame;
			} else if (frame->fcount == hw_fcount) {
				framemgr_x_barrier_common(framemgr, 0, flags);
				msinfo_hw("[F:%d,FF:%d,HWF:%d] right config frame",
					instance_id, hw_ip, fcount, frame->fcount, hw_fcount);
				return true;
			}
		}
		framemgr_x_barrier_common(framemgr, 0, flags);
		mserr_hw("[F:%d,HWF:%d]check_dma_done: frame(null)!!", instance_id, hw_ip, fcount, hw_fcount);
		return true;
	}

	/*
	 * fcount: This value should be same value that is notified by host at shot time.
	 * In case of FRO or batch mode, this value also should be same between start and end.
	 */
	msdbg_hw(1, "check_dma [ddk:%d,hw:%d] frame(F:%d,idx:%d,num_buffers:%d)\n",
			instance_id, hw_ip,
			fcount, hw_fcount,
			frame->fcount, frame->cur_buf_index, frame->num_buffers);

	if (test_bit(hw_ip->id, &frame->core_flag))
		output_id = IS_HW_CORE_END;

	is_hardware_frame_done(hw_ip, NULL, -1, output_id,
			IS_SHOT_SUCCESS, true);

	return ret;
}

static void is_lib_io_callback(void *this, enum lib_cb_event_type event_id,
	u32 instance_id)
{
	struct is_hardware *hardware;
	struct is_hw_ip *hw_ip;
	int output_id = 0;
	u32 hw_fcount;
#if defined(ENABLE_FULLCHAIN_OVERFLOW_RECOVERY)
	int ret = 0;
#endif

	FIMC_BUG_VOID(!this);

	hw_ip = (struct is_hw_ip *)this;
	hardware = hw_ip->hardware;
	hw_fcount = atomic_read(&hw_ip->fcount);

	FIMC_BUG_VOID(!hw_ip->hardware);

	switch (event_id) {
	case LIB_EVENT_DMA_A_OUT_DONE:
		if (!atomic_read(&hardware->streaming[hardware->sensor_position[instance_id]]))
			msinfo_hw("[F:%d]DMA A\n", instance_id, hw_ip,
				hw_fcount);
		_is_hw_frame_dbg_trace(hw_ip, hw_fcount, DEBUG_POINT_FRAME_DMA_END);

		switch (hw_ip->id) {
		case DEV_HW_3AA0: /* after BDS */
		case DEV_HW_3AA1:
		case DEV_HW_3AA2:
		case DEV_HW_3AA3:
		case DEV_HW_ISP0: /* chunk output */
		case DEV_HW_ISP1:
			is_hardware_frame_done(hw_ip, NULL, -1,
					output_id, IS_SHOT_SUCCESS, true);
			break;
		case DEV_HW_CLH0:
			err_hw("[%d] Need to check CLH0 DMA A done(%d)!!", instance_id, hw_ip->id);
			break;
		default:
			err_hw("[%d] invalid hw ID(%d)!!", instance_id, hw_ip->id);
			break;
		}

		break;
	case LIB_EVENT_DMA_B_OUT_DONE:
		if (!atomic_read(&hardware->streaming[hardware->sensor_position[instance_id]]))
			msinfo_hw("[F:%d]DMA B\n", instance_id, hw_ip,
				hw_fcount);
		_is_hw_frame_dbg_trace(hw_ip, hw_fcount, DEBUG_POINT_FRAME_DMA_END);
		switch (hw_ip->id) {
		case DEV_HW_3AA0: /* before BDS */
		case DEV_HW_3AA1:
		case DEV_HW_3AA2:
		case DEV_HW_3AA3:
		case DEV_HW_ISP0: /* yuv output */
		case DEV_HW_ISP1:
		case DEV_HW_CLH0:
			is_hardware_frame_done(hw_ip, NULL, -1,
					output_id, IS_SHOT_SUCCESS, true);
			break;
		default:
			err_hw("[%d] invalid hw ID(%d)!!", instance_id, hw_ip->id);
			break;
		}

		break;
	case LIB_EVENT_ERROR_CIN_OVERFLOW:
		is_debug_event_count(IS_EVENT_OVERFLOW_3AA);
#if IS_ENABLED(CONFIG_EXYNOS_SCI_DBG_AUTO)
		smc_ppc_enable(0);
#endif
		msinfo_hw("LIB_EVENT_ERROR_CIN_OVERFLOW\n", instance_id, hw_ip);
		is_hardware_flush_frame(hw_ip, FS_HW_CONFIGURE, IS_SHOT_OVERFLOW);

#if defined(ENABLE_FULLCHAIN_OVERFLOW_RECOVERY)
		ret = is_hw_overflow_recovery();
		if (ret < 0)
			is_debug_s2d(false, "OVERFLOW recovery fail!!!!");
#elif defined(OVERFLOW_PANIC_ENABLE_ISCHAIN)
		is_debug_s2d(false, "CIN OVERFLOW!!");
#endif
		break;
	default:
		msinfo_hw("event_id(%d)\n", instance_id, hw_ip, event_id);
		break;
	}
};

static void is_lib_camera_callback(void *this, enum lib_cb_event_type event_id,
	u32 instance_id, void *data)
{
	struct is_hardware *hardware;
	struct is_hw_ip *hw_ip;
	ulong fcount;
	u32 hw_fcount, index;
	bool frame_done = false;
	struct is_framemgr *framemgr;
	struct is_frame *frame;
	ulong flags = 0;
	struct lib_callback_result *cb_result = NULL;
	struct is_group *group;

	FIMC_BUG_VOID(!this);

	hw_ip = (struct is_hw_ip *)this;

	group = hw_ip->group[instance_id];
	if (group && group->hw_ip)
		hw_ip = group->hw_ip;

	hardware = hw_ip->hardware;
	hw_fcount = atomic_read(&hw_ip->fcount);

	FIMC_BUG_VOID(!hw_ip->hardware);

	if (test_bit(HW_OVERFLOW_RECOVERY, &hardware->hw_recovery_flag)) {
		err_hw("[ID:%d] During recovery : invalid interrupt", hw_ip->id);
		return;
	}

	if (IS_ENABLED(CHAIN_USE_STRIPE_PROCESSING) && data) {
		cb_result = (struct lib_callback_result *)data;
		fcount = (ulong)cb_result->fcount;
	} else {
		fcount = (ulong)data;
	}

	switch (event_id) {
	case LIB_EVENT_CONFIG_LOCK:
		atomic_add(1, &hw_ip->count.cl);
		if (unlikely(!atomic_read(&hardware->streaming[hardware->sensor_position[instance_id]])))
			msinfo_hw("[F:%d]C.L %d\n", instance_id, hw_ip,
				hw_fcount, (u32)fcount);

		/* fcount : frame number of current frame in Vvalid */
		is_hardware_config_lock(hw_ip, instance_id, (u32)fcount);
		break;
	case LIB_EVENT_FRAME_START_ISR:
		if (sysfs_debug.pattern_en && (hw_ip->id == DEV_HW_3AA0 ||
			hw_ip->id == DEV_HW_3AA1 || hw_ip->id == DEV_HW_3AA2)) {
			struct is_group *group;
			struct v4l2_subdev *subdev;
			struct is_device_csi *csi;

			group = hw_ip->group[instance_id];
			if (IS_ERR_OR_NULL(group)) {
				mserr_hw("group is NULL", instance_id, hw_ip);
				return;
			}

			if (IS_ERR_OR_NULL(group->device)) {
				mserr_hw("device is NULL", instance_id, hw_ip);
				return;
			}

			if (IS_ERR_OR_NULL(group->device->sensor)) {
				mserr_hw("sensor is NULL", instance_id, hw_ip);
				return;
			}

			if (IS_ERR_OR_NULL(group->device->sensor->subdev_csi)) {
				mserr_hw("subdev_csi is NULL", instance_id, hw_ip);
				return;
			}

			subdev = group->device->sensor->subdev_csi;
			csi = v4l2_get_subdevdata(subdev);
			if (IS_ERR_OR_NULL(csi)) {
				mserr_hw("csi is NULL", instance_id, hw_ip);
				return;
			}

			csi_frame_start_inline(csi);
		}

		_is_hw_frame_dbg_trace(hw_ip, hw_fcount, DEBUG_POINT_FRAME_START);

		if (unlikely(!atomic_read(&hardware->streaming[hardware->sensor_position[instance_id]])
			|| debug_irq_ddk))
			msinfo_hw("[F:%d]F.S\n", instance_id, hw_ip,
				hw_fcount);

		atomic_add(1, &hw_ip->count.fs);
		is_hardware_frame_start(hw_ip, instance_id);
		break;
	case LIB_EVENT_FRAME_END:
		_is_hw_frame_dbg_trace(hw_ip, hw_fcount, DEBUG_POINT_FRAME_END);

		atomic_add(1, &hw_ip->count.fe);
		if (unlikely(!atomic_read(&hardware->streaming[hardware->sensor_position[instance_id]])
			|| debug_irq_ddk))
			msinfo_hw("[F:%d]F.E\n", instance_id, hw_ip,
				hw_fcount);

		if (unlikely(debug_time_hw)) {
			index = hw_ip->debug_index[1];
			msinfo_hw("[TIM] S-E %05llu us\n", instance_id, hw_ip,
				(hw_ip->debug_info[index].time[DEBUG_POINT_FRAME_END] -
				hw_ip->debug_info[index].time[DEBUG_POINT_FRAME_START]) / 1000);
		}

		is_hw_g_ctrl(hw_ip, hw_ip->id, HW_G_CTRL_FRM_DONE_WITH_DMA, (void *)&frame_done);
		if (frame_done) {
			check_dma_done(hw_ip, instance_id, (u32)fcount);
		} else {
			msdbg_hw(1, "dma done interupt separate\n", instance_id, hw_ip);

			is_hardware_frame_done(hw_ip, NULL, -1, IS_HW_CORE_END,
				IS_SHOT_SUCCESS, true);
		}

		atomic_set(&hw_ip->status.Vvalid, V_BLANK);
		wake_up(&hw_ip->status.wait_queue);
		break;
	case LIB_EVENT_ERROR_CONFIG_LOCK_DELAY:
		_is_hw_frame_dbg_trace(hw_ip, hw_fcount, DEBUG_POINT_FRAME_END);

		atomic_add(1, &hw_ip->count.fe);

		framemgr = hw_ip->framemgr;
		framemgr_e_barrier_common(framemgr, 0, flags);
		frame = peek_frame(framemgr, FS_HW_WAIT_DONE);
		framemgr_x_barrier_common(framemgr, 0, flags);

		if (frame) {
			if (is_hardware_frame_ndone(hw_ip, frame, frame->instance,
						IS_SHOT_CONFIG_LOCK_DELAY)) {
				mserr_hw("failure in hardware_frame_ndone",
						frame->instance, hw_ip);
			}
		} else {
			serr_hw("[F:%lu]camera_callback: frame(null)!!(E%d)", hw_ip,
				fcount, event_id);
		}

		atomic_set(&hw_ip->status.Vvalid, V_BLANK);
		wake_up(&hw_ip->status.wait_queue);
		break;
	default:
		break;
	}
};

struct lib_callback_func is_lib_cb_func = {
	.camera_callback	= is_lib_camera_callback,
	.io_callback		= is_lib_io_callback,
};

static int __nocfi __is_extra_chain_create(struct is_lib_isp *this, u32 chain_id, ulong base_addr)
{
	int ret = 0;

	ret = CALL_LIBOP(this, chain_create, chain_id, base_addr, 0x0,
				&is_lib_cb_func);
	if (ret) {
		err_lib("(%d) chain_create fail!!\n", chain_id);
		ret = -EINVAL;
	}

	return ret;
}

int __nocfi is_lib_isp_chain_create(struct is_hw_ip *hw_ip,
	struct is_lib_isp *this, u32 instance_id)
{
	int ret = 0;
	u32 chain_id = 0;
	ulong base_addr, base_addr_b, set_b_offset;
	struct lib_system_config config;
	u32 chain_id_offset = 0;

	FIMC_BUG(!hw_ip);
	FIMC_BUG(!this);
	FIMC_BUG(!this->func);

#ifdef USE_ORBMCH_FOR_ME
	chain_id_offset = 4;
#endif

	switch (hw_ip->id) {
	case DEV_HW_3AA0:
	case DEV_HW_ISP0:
	case DEV_HW_CLH0:
	case DEV_HW_YPP:
		chain_id = 0;
		break;
	case DEV_HW_3AA1:
	case DEV_HW_ISP1:
		chain_id = 1;
		break;
	case DEV_HW_3AA2:
		chain_id = 2;
		break;
	case DEV_HW_3AA3:
		chain_id = 3;
		break;
	case DEV_HW_LME0:
		chain_id = 0 + chain_id_offset;
		break;
	case DEV_HW_LME1:
		chain_id = 1 + chain_id_offset;
		break;
	default:
		err_lib("invalid hw (%d)", hw_ip->id);
		return -EINVAL;
	}

	/* create additional IP chain */
	if (hw_ip->regs[REG_EXT1]) {
		base_addr    = (ulong)hw_ip->regs[REG_EXT1];
		ret = __is_extra_chain_create(this, (chain_id + EXT1_CHAIN_OFFSET), base_addr);
		if (ret) {
			err_lib("ext1 chain_create fail (%d)", hw_ip->id);
			return -EINVAL;
		}
		msinfo_lib("ext1 chain_create done [reg_base:0x%lx][b_offset:0x%x]\n",
				instance_id, hw_ip, base_addr, 0x0);
	}

	if (hw_ip->regs[REG_EXT2]) {
		base_addr    = (ulong)hw_ip->regs[REG_EXT2];
		ret = __is_extra_chain_create(this, (chain_id + EXT2_CHAIN_OFFSET), base_addr);
		if (ret) {
			err_lib("ext2 chain_create fail (%d)", hw_ip->id);
			return -EINVAL;
		}
		msinfo_lib("ext2 chain_create done [reg_base:0x%lx][b_offset:0x%x]\n",
				instance_id, hw_ip, base_addr, 0x0);
	}

	if (hw_ip->regs[REG_EXT3]) {
		base_addr    = (ulong)hw_ip->regs[REG_EXT3];
		ret = __is_extra_chain_create(this, (chain_id + EXT3_CHAIN_OFFSET), base_addr);
		if (ret) {
			err_lib("ext3 chain_create fail (%d)", hw_ip->id);
			return -EINVAL;
		}
		msinfo_lib("ext3 chain_create done [reg_base:0x%lx][b_offset:0x%x]\n",
				instance_id, hw_ip, base_addr, 0x0);
	}

	if (hw_ip->regs[REG_EXT4]) {
		base_addr    = (ulong)hw_ip->regs[REG_EXT4];
		ret = __is_extra_chain_create(this, (chain_id + EXT4_CHAIN_OFFSET), base_addr);
		if (ret) {
			err_lib("ext4 chain_create fail (%d)", hw_ip->id);
			return -EINVAL;
		}
		msinfo_lib("ext4 chain_create done [reg_base:0x%lx][b_offset:0x%x]\n",
				instance_id, hw_ip, base_addr, 0x0);
	}

	base_addr    = (ulong)hw_ip->regs[REG_SETA];
	base_addr_b  = 0; /* FIXED as 0 */
	set_b_offset = (base_addr_b < base_addr) ? 0 : base_addr_b - base_addr;

	ret = CALL_LIBOP(this, chain_create, chain_id, base_addr, set_b_offset,
				&is_lib_cb_func);
	if (ret) {
		err_lib("chain_create fail (%d)", hw_ip->id);
		return -EINVAL;
	}

	/* set system config */
	memset(&config, 0, sizeof(struct lib_system_config));
	config.cmd = LIB_CMD_OVERFLOW_RECOVERY;
	config.args[0] = DDK_OVERFLOW_RECOVERY;

	ret = CALL_LIBOP(this, set_system_config, &config);
	if (ret) {
		err_lib("set_system_config fail (%d)", hw_ip->id);
		return -EINVAL;
	}

	msinfo_lib("chain_create done [reg_base:0x%lx][b_offset:0x%lx](%d)\n",
		instance_id, hw_ip, base_addr, set_b_offset, DDK_OVERFLOW_RECOVERY);

	return ret;
}

int __nocfi is_lib_isp_object_create(struct is_hw_ip *hw_ip,
	struct is_lib_isp *this, u32 instance_id, u32 rep_flag, u32 module_id)
{
	int ret = 0;
	u32 chain_id = 0, input_type, obj_info = 0;
	u32 chain_id_offset = 0;
	struct is_device_ischain *device = NULL;

	FIMC_BUG(!hw_ip);
	FIMC_BUG(!this);
	FIMC_BUG(!this->func);
	FIMC_BUG(!hw_ip->group[instance_id]);

#ifdef USE_ORBMCH_FOR_ME
	chain_id_offset = 4;
#endif

	device = hw_ip->group[instance_id]->device;
	FIMC_BUG(!device);

	switch (hw_ip->id) {
	case DEV_HW_3AA0:
	case DEV_HW_ISP0:
	case DEV_HW_CLH0:
	case DEV_HW_YPP:
		chain_id = 0;
		break;
	case DEV_HW_3AA1:
	case DEV_HW_ISP1:
		chain_id = 1;
		break;
	case DEV_HW_3AA2:
		chain_id = 2;
		break;
	case DEV_HW_3AA3:
		chain_id = 3;
		break;
	case DEV_HW_LME0:
		chain_id = 0 + chain_id_offset;
		break;
	case DEV_HW_LME1:
		chain_id = 1 + chain_id_offset;
		break;
	default:
		err_lib("invalid hw (%d)", hw_ip->id);
		return -EINVAL;
	}

	/* input_type : use only in 3AA (guide by DDK) */
	if (test_bit(IS_GROUP_OTF_INPUT, &hw_ip->group[instance_id]->state))
		input_type = 0; /* default */
	else
		input_type = 1;

	obj_info |= chain_id << CHAIN_ID_SHIFT;
	obj_info |= instance_id << INSTANCE_ID_SHIFT;
	obj_info |= rep_flag << REPROCESSING_FLAG_SHIFT;
	obj_info |= (input_type) << INPUT_TYPE_SHIFT;
	obj_info |= (module_id) << MODULE_ID_SHIFT;
	obj_info |= (device->sensor->position) << POSITION_ID_SHIFT;

	info_lib("obj_create: chain(%d), instance(%d), rep(%d), in_type(%d)\n",
		chain_id, instance_id, rep_flag, input_type);
	info_lib("obj_info(0x%08x), module_id(%d)\n", obj_info, module_id);

	ret = CALL_LIBOP(this, object_create, &this->object, obj_info, hw_ip);
	if (ret) {
		err_lib("object_create fail (%d)", hw_ip->id);
		return -EINVAL;
	}

	msinfo_lib("object_create done\n", instance_id, hw_ip);

	return ret;
}

void __nocfi is_lib_isp_chain_destroy(struct is_hw_ip *hw_ip,
	struct is_lib_isp *this, u32 instance_id)
{
	int ret = 0;
	u32 chain_id = 0;
	u32 chain_id_offset = 0;

	FIMC_BUG_VOID(!hw_ip);
	FIMC_BUG_VOID(!this->func);

#ifdef USE_ORBMCH_FOR_ME
	chain_id_offset = 4;
#endif

	switch (hw_ip->id) {
	case DEV_HW_3AA0:
	case DEV_HW_ISP0:
	case DEV_HW_CLH0:
	case DEV_HW_YPP:
		chain_id = 0;
		break;
	case DEV_HW_3AA1:
	case DEV_HW_ISP1:
		chain_id = 1;
		break;
	case DEV_HW_3AA2:
		chain_id = 2;
		break;
	case DEV_HW_3AA3:
		chain_id = 3;
		break;
	case DEV_HW_LME0:
		chain_id = 0 + chain_id_offset;
		break;
	case DEV_HW_LME1:
		chain_id = 1 + chain_id_offset;
		break;
	default:
		err_lib("invalid hw (%d)", hw_ip->id);
		return;
	}


#if defined(EXT_CHAIN_DESTROY)
	/* destroy additional IP chain */
	if (hw_ip->regs[REG_EXT1]) {
		ret = CALL_LIBOP(this, chain_destroy, (chain_id + EXT1_CHAIN_OFFSET));
		if (ret) {
			err_lib("ext1 chain_destroy fail (%d)", hw_ip->id);
			return;
		}
		msinfo_lib("ext1 chain_destroy done\n", instance_id, hw_ip);
	}

	if (hw_ip->regs[REG_EXT2]) {
		ret = CALL_LIBOP(this, chain_destroy, (chain_id + EXT2_CHAIN_OFFSET));
		if (ret) {
			err_lib("ext2 chain_destroy fail (%d)", hw_ip->id);
			return;
		}
		msinfo_lib("ext2 chain_destroy done\n", instance_id, hw_ip);
	}

	if (hw_ip->regs[REG_EXT3]) {
		ret = CALL_LIBOP(this, chain_destroy, (chain_id + EXT3_CHAIN_OFFSET));
		if (ret) {
			err_lib("ext3 chain_destroy fail (%d)", hw_ip->id);
			return;
		}
		msinfo_lib("ext3 chain_destroy done\n", instance_id, hw_ip);
	}

	if (hw_ip->regs[REG_EXT4]) {
		ret = CALL_LIBOP(this, chain_destroy, (chain_id + EXT4_CHAIN_OFFSET));
		if (ret) {
			err_lib("ext4 chain_destroy fail (%d)", hw_ip->id);
			return;
		}
		msinfo_lib("ext4 chain_destroy done\n", instance_id, hw_ip);
	}
#endif

	ret = CALL_LIBOP(this, chain_destroy, chain_id);
	if (ret) {
		err_lib("chain_destroy fail (%d)", hw_ip->id);
		return;
	}

#if defined(SOC_DNS0S)
	if (hw_ip->id == DEV_HW_ISP0) {
		msinfo_lib("set force_internal_clock of DNS\n", instance_id, hw_ip);
		writel(0x1, hw_ip->regs[REG_EXT2] + 0x1c);
	}
#endif

	msinfo_lib("chain_destroy done\n", instance_id, hw_ip);
}

void __nocfi is_lib_isp_object_destroy(struct is_hw_ip *hw_ip,
	struct is_lib_isp *this, u32 instance_id)
{
	int ret = 0;

	FIMC_BUG_VOID(!hw_ip);
	FIMC_BUG_VOID(!this);
	FIMC_BUG_VOID(!this->func);

	if (!this->object) {
		err_lib("object(NULL) destroy fail (%d)", hw_ip->id);
		return;
	}

	ret = CALL_LIBOP(this, object_destroy, this->object, instance_id);
	if (ret) {
		err_lib("object_destroy fail (%d)", hw_ip->id);
		return;
	}

	msinfo_lib("object_destroy done\n", instance_id, hw_ip);
}

int __nocfi is_lib_isp_set_param(struct is_hw_ip *hw_ip,
	struct is_lib_isp *this, void *param)
{
	int ret;

	FIMC_BUG(!hw_ip);
	FIMC_BUG(!this);
	FIMC_BUG(!this->func);
	FIMC_BUG(!this->object);

	ret = CALL_LIBOP(this, set_param, this->object, param);
	if (ret)
		mserr_lib("set_param fail", atomic_read(&hw_ip->instance), hw_ip);

	return ret;
}

int __nocfi is_lib_isp_set_ctrl(struct is_hw_ip *hw_ip,
	struct is_lib_isp *this, struct is_frame *frame)
{
	int ret = 0;

	FIMC_BUG(!hw_ip);
	FIMC_BUG(!this);
	FIMC_BUG(!frame);
	FIMC_BUG(!this->func);
	FIMC_BUG(!this->object);

	ret = CALL_LIBOP(this, set_ctrl, this->object, frame->instance,
				frame->fcount, frame->shot);
	if (ret)
		mserr_lib("set_ctrl fail", atomic_read(&hw_ip->instance), hw_ip);

	return 0;
}

int __nocfi is_lib_isp_shot(struct is_hw_ip *hw_ip,
	struct is_lib_isp *this, void *param_set, struct camera2_shot *shot)
{
	int ret = 0;

	FIMC_BUG(!hw_ip);
	FIMC_BUG(!this);
	FIMC_BUG(!param_set);
	FIMC_BUG(!this->func);
	FIMC_BUG(!this->object);

	ret = CALL_LIBOP_NO_FPSIMD(this, shot, this->object, param_set,
			shot, hw_ip->num_buffers);
	if (ret)
		mserr_lib("shot fail", atomic_read(&hw_ip->instance), hw_ip);

	return ret;
}

int __nocfi is_lib_isp_get_meta(struct is_hw_ip *hw_ip,
	struct is_lib_isp *this, struct is_frame *frame)

{
	int ret = 0;

	FIMC_BUG(!hw_ip);
	FIMC_BUG(!this);
	FIMC_BUG(!frame);
	FIMC_BUG(!this->func);
	FIMC_BUG(!this->object);
	FIMC_BUG(!frame->shot);

	ret = CALL_LIBOP(this, get_meta, this->object, frame->instance,
				frame->fcount, frame->shot);
	if (ret)
		mserr_lib("get_meta fail", atomic_read(&hw_ip->instance), hw_ip);

	return ret;
}

void __nocfi is_lib_isp_stop(struct is_hw_ip *hw_ip,
	struct is_lib_isp *this, u32 instance_id)
{
	int ret = 0;

	FIMC_BUG_VOID(!hw_ip);
	FIMC_BUG_VOID(!this);
	FIMC_BUG_VOID(!this->func);
	FIMC_BUG_VOID(!this->object);

	ret = CALL_LIBOP(this, stop, this->object, instance_id);
	if (ret) {
		err_lib("object_suspend fail (%d)", hw_ip->id);
		return;
	}
	msinfo_lib("object_suspend done\n", instance_id, hw_ip);
}

int __nocfi is_lib_isp_create_tune_set(struct is_lib_isp *this,
	ulong addr, u32 size, u32 index, int flag, u32 instance_id)
{
	int ret = 0;
	struct lib_tune_set tune_set;

	FIMC_BUG(!this);
	FIMC_BUG(!this->func);
	FIMC_BUG(!this->object);

	tune_set.index = index;
	tune_set.addr = addr;
	tune_set.size = size;
	tune_set.decrypt_flag = flag;

	ret = CALL_LIBOP(this, create_tune_set, this->object, instance_id,
				&tune_set);
	if (ret) {
		err_lib("create_tune_set fail (%d)", ret);
		return ret;
	}

	minfo_lib("create_tune_set index(%d)\n", instance_id, index);

	return ret;
}

int __nocfi is_lib_isp_apply_tune_set(struct is_lib_isp *this,
	u32 index, u32 instance_id)
{
	int ret = 0;

	FIMC_BUG(!this);
	FIMC_BUG(!this->func);
	FIMC_BUG(!this->object);

	ret = CALL_LIBOP(this, apply_tune_set, this->object, instance_id, index);
	if (ret) {
		err_lib("apply_tune_set fail (%d)", ret);
		return ret;
	}

	return ret;
}

int __nocfi is_lib_isp_delete_tune_set(struct is_lib_isp *this,
	u32 index, u32 instance_id)
{
	int ret = 0;

	FIMC_BUG(!this);
	FIMC_BUG(!this->func);
	FIMC_BUG(!this->object);

	ret = CALL_LIBOP(this, delete_tune_set, this->object, instance_id, index);
	if (ret) {
		err_lib("delete_tune_set fail (%d)", ret);
		return ret;
	}

	minfo_lib("delete_tune_set index(%d)\n", instance_id, index);

	return ret;
}

int __nocfi is_lib_isp_change_chain(struct is_hw_ip *hw_ip,
	struct is_lib_isp *this, u32 instance_id, u32 next_id)
{
	int ret = 0;

	FIMC_BUG(!this);
	FIMC_BUG(!this->func);
	FIMC_BUG(!this->object);

	ret = CALL_LIBOP(this, change_chain, this->object, instance_id, next_id);
	if (ret) {
		err_lib("change_chain fail (%d)", ret);
		return ret;
	}

	return ret;
}

int __nocfi is_lib_isp_load_cal_data(struct is_lib_isp *this,
	u32 instance_id, ulong addr)
{
	char version[32];
	int ret = 0;

	FIMC_BUG(!this);
	FIMC_BUG(!this->func);
	FIMC_BUG(!this->object);

	memcpy(version, (void *)(addr + 0x20), (IS_CAL_VER_SIZE - 1));
	version[IS_CAL_VER_SIZE] = '\0';
	minfo_lib("CAL version: %s\n", instance_id, version);

	ret = CALL_LIBOP(this, load_cal_data, this->object, instance_id, addr);
	if (ret) {
		err_lib("apply_tune_set fail (%d)", ret);
		return ret;
	}

	return ret;
}

int __nocfi is_lib_isp_get_cal_data(struct is_lib_isp *this,
	u32 instance_id, struct cal_info *c_info, int type)
{
	int ret = 0;

	FIMC_BUG(!this);
	FIMC_BUG(!this->func);
	FIMC_BUG(!this->object);

	ret = CALL_LIBOP(this, get_cal_data, this->object, instance_id,
				c_info, type);
	if (ret) {
		err_lib("apply_tune_set fail (%d)", ret);
		return ret;
	}
	dbg_lib(3, "%s: data(%d,%d,%d,%d)\n",
		__func__, c_info->data[0], c_info->data[1], c_info->data[2], c_info->data[3]);

	return ret;
}

int __nocfi is_lib_isp_sensor_info_mode_chg(struct is_lib_isp *this,
	u32 instance_id, struct camera2_shot *shot)
{
	int ret = 0;

	FIMC_BUG(!this);
	FIMC_BUG(!shot);
	FIMC_BUG(!this->func);
	FIMC_BUG(!this->object);

	ret = CALL_LIBOP(this, sensor_info_mode_chg, this->object, instance_id,
				shot);
	if (ret) {
		err_lib("sensor_info_mode_chg fail (%d)", ret);
		return ret;
	}

	return ret;
}

int __nocfi is_lib_isp_sensor_update_control(struct is_lib_isp *this,
	u32 instance_id, u32 frame_count, struct camera2_shot *shot)
{
	int ret = 0;

	FIMC_BUG(!this);
	FIMC_BUG(!this->func);
	FIMC_BUG(!this->object);

	ret = CALL_LIBOP(this, sensor_update_ctl, this->object, instance_id,
				frame_count, shot);
	if (ret) {
		err_lib("sensor_update_ctl fail (%d)", ret);
		return ret;
	}

	return ret;
}

int __nocfi is_lib_isp_reset_recovery(struct is_hw_ip *hw_ip,
		struct is_lib_isp *this, u32 instance_id)
{
	int ret = 0;

	BUG_ON(!hw_ip);
	BUG_ON(!this->func);

	ret = CALL_LIBOP(this, recovery, instance_id);
	if (ret) {
		err_lib("chain_reset_recovery fail (%d)", hw_ip->id);
		return ret;
	}
	msinfo_lib("chain_reset_recovery done\n", instance_id, hw_ip);

	return ret;
}

int is_lib_isp_convert_face_map(struct is_hardware *hardware,
	struct taa_param_set *param_set, struct is_frame *frame)
{
	int ret = 0;
#ifdef ENABLE_VRA
	int i;
	u32 fd_width = 0, fd_height = 0;
	u32 bayer_crop_width, bayer_crop_height;
	struct is_group *group = NULL;
	struct is_group *group_vra;
	struct camera2_shot *shot = NULL;
	struct is_device_ischain *device = NULL;
	struct param_otf_input *fd_otf_input;
	struct param_dma_input *fd_dma_input;

	FIMC_BUG(!hardware);
	FIMC_BUG(!param_set);
	FIMC_BUG(!frame);

	shot = frame->shot;
	FIMC_BUG(!shot);

	group = (struct is_group *)frame->group;
	FIMC_BUG(!group);

	device = group->device;
	FIMC_BUG(!device);

	if (shot->uctl.fdUd.faceDetectMode == FACEDETECT_MODE_OFF)
		return 0;

	/*
	 * The face size which an algorithm uses is determined
	 * by the input bayer crop size of 3aa.
	 */
	if (param_set->otf_input.cmd == OTF_INPUT_COMMAND_ENABLE) {
		bayer_crop_width = param_set->otf_input.bayer_crop_width;
		bayer_crop_height = param_set->otf_input.bayer_crop_height;
	} else if (param_set->dma_input.cmd == DMA_INPUT_COMMAND_ENABLE) {
		bayer_crop_width = param_set->dma_input.bayer_crop_width;
		bayer_crop_height = param_set->dma_input.bayer_crop_height;
	} else {
		err_hw("invalid hw input!!\n");
		return -EINVAL;
	}

	if (bayer_crop_width == 0 || bayer_crop_height == 0) {
		warn_hw("%s: invalid crop size (%d * %d)!!",
			__func__, bayer_crop_width, bayer_crop_height);
		return 0;
	}

	/* The face size is determined by the fd input size */
	group_vra = &device->group_vra;
	if (test_bit(IS_GROUP_INIT, &group_vra->state)
		&& (!test_bit(IS_GROUP_OTF_INPUT, &group_vra->state))) {

		fd_dma_input = is_itf_g_param(device, frame, PARAM_FD_DMA_INPUT);
		if (fd_dma_input->cmd == DMA_INPUT_COMMAND_ENABLE) {
			fd_width = fd_dma_input->width;
			fd_height = fd_dma_input->height;
		}
	} else {
		fd_otf_input = is_itf_g_param(device, frame, PARAM_FD_OTF_INPUT);
		if (fd_otf_input->cmd == OTF_INPUT_COMMAND_ENABLE) {
			fd_width = fd_otf_input->width;
			fd_height = fd_otf_input->height;
		}
	}

	if (fd_width == 0 || fd_height == 0) {
		warn_hw("%s: invalid fd size (%d * %d)!!",
			__func__, fd_width, fd_height);
		return 0;
	}

	/* Convert face size */
	for (i = 0; i < CAMERA2_MAX_FACES; i++) {
		if (shot->uctl.fdUd.faceScores[i] == 0)
			continue;

		shot->uctl.fdUd.faceRectangles[i][0] =
				CONVRES(shot->uctl.fdUd.faceRectangles[i][0],
				fd_width, bayer_crop_width);
		shot->uctl.fdUd.faceRectangles[i][1] =
				CONVRES(shot->uctl.fdUd.faceRectangles[i][1],
				fd_height, bayer_crop_height);
		shot->uctl.fdUd.faceRectangles[i][2] =
				CONVRES(shot->uctl.fdUd.faceRectangles[i][2],
				fd_width, bayer_crop_width);
		shot->uctl.fdUd.faceRectangles[i][3] =
				CONVRES(shot->uctl.fdUd.faceRectangles[i][3],
				fd_height, bayer_crop_height);

		dbg_lib(3, "%s: ID(%d), x_min(%d), y_min(%d), x_max(%d), y_max(%d)\n",
			__func__, shot->uctl.fdUd.faceIds[i],
			shot->uctl.fdUd.faceRectangles[i][0],
			shot->uctl.fdUd.faceRectangles[i][1],
			shot->uctl.fdUd.faceRectangles[i][2],
			shot->uctl.fdUd.faceRectangles[i][3]);
	}

#endif
	return ret;
}

void is_isp_get_bcrop1_size(void __iomem *base_addr, u32 *width, u32 *height)
{
	*width  = is_hw_get_field(base_addr, &isp_regs[ISP_R_BCROP1_SIZE_X], &isp_fields[ISP_F_BCROP1_SIZE_X]);
	*height = is_hw_get_field(base_addr, &isp_regs[ISP_R_BCROP1_SIZE_Y], &isp_fields[ISP_F_BCROP1_SIZE_Y]);
}

int __nocfi is_lib_isp_event_notifier(struct is_hw_ip *hw_ip, struct is_lib_isp *this,
	int instance, u32 fcount, int event_id, u32 strip_index)
{
	int ret = 0;

	FIMC_BUG(!hw_ip);
	FIMC_BUG(!this);
	FIMC_BUG(!this->func);
	FIMC_BUG(!this->object);

	switch (hw_ip->id) {
	case DEV_HW_YPP:
		ret = CALL_LIBOP_ISR(this, event_notifier, DEV_HW_YPP, instance, fcount, event_id, strip_index);
		if (ret)
			err_lib("ypp shot fail (%d)", DEV_HW_YPP);
		break;
	default:
		err_lib("invalid hw (%d)", hw_ip->id);
		break;
	}

	return ret;
}

#if IS_ENABLED(ENABLE_3AA_LIC_OFFSET)
int __nocfi is_lib_set_sram_offset(struct is_hw_ip *hw_ip,
	struct is_lib_isp *this, u32 instance_id)
{
	struct is_hardware *hw = NULL;
	u32 offset[LIC_TRIGGER_MODE] = {0, }, mode;
	int index, ret = 0;
	u32 offsets = LIC_CHAIN_OFFSET_NUM / 2 - 1;
	u32 set_idx = offsets + 1;
	int i;

	FIMC_BUG(!hw_ip);
	FIMC_BUG(!this);
	FIMC_BUG(!this->func);

	hw = hw_ip->hardware;

	index = COREX_SETA * set_idx; /* setA */
	for (i = LIC_OFFSET_0; i < offsets; i++)
		offset[i] = hw->lic_offset[0][index + i];

	mode = hw->lic_offset[0][index + offsets];
#if defined(SOC_33S)
	ret = CALL_LIBOP(this, set_line_buffer_offset,
		COREX_SETA, offsets, (u32 *)&offset);
#else
	ret = CALL_LIBOP(this, set_line_buffer_offset,
		COREX_SETA, offsets, (u32 *)&offset, mode);
#endif
	if (ret) {
		err_lib("set_line_buffer_offset fail (%d)", hw_ip->id);
		return ret;
	}
	msinfo_lib("set_line_buffer_offset [%d][%d, %d, %d] done\n",
		instance_id, hw_ip,
		COREX_SETA, offset[LIC_OFFSET_0], offset[LIC_OFFSET_1], offset[LIC_OFFSET_2]);

	index = COREX_SETB * set_idx; /* setB */
	for (i = LIC_OFFSET_0; i < offsets; i++)
		offset[i] = hw->lic_offset[0][index + i];

	mode = hw->lic_offset[0][index + offsets];
#if defined(SOC_33S)
	ret = CALL_LIBOP(this, set_line_buffer_offset,
		COREX_SETB, offsets, (u32 *)&offset);
#else
	ret = CALL_LIBOP(this, set_line_buffer_offset,
		COREX_SETB, offsets, (u32 *)&offset, mode);
#endif
	if (ret) {
		err_lib("set_line_buffer_offset fail (%d)", hw_ip->id);
		return ret;
	}
	msinfo_lib("set_line_buffer_offset [%d][%d, %d, %d] done\n",
		instance_id, hw_ip,
		COREX_SETB, offset[LIC_OFFSET_0], offset[LIC_OFFSET_1], offset[LIC_OFFSET_2]);

#if defined(SOC_33S)
	/* TODO: set trigger mode by scenario */
	ret = CALL_LIBOP(this, set_line_buffer_trigger, LIC_NO_TRIGGER, LBOFFSET_NONE);
	if (ret) {
		err_lib("set_line_buffer_trigger fail (%d)", hw_ip->id);
		return ret;
	}
#endif
	return ret;
}
#endif

int __nocfi is_lib_notify_timeout(struct is_lib_isp *this, u32 instance)
{
	int ret = 0;
	struct lib_system_config config;

	FIMC_BUG(!this);
	FIMC_BUG(!this->func);

	/* set system config */
	memset(&config, 0, sizeof(struct lib_system_config));

	config.cmd = LIB_CMD_TIMEOUT;
	config.args[0] = instance;

	ret = CALL_LIBOP(this, set_system_config, &config);

	if (ret) {
		err_lib("set_system_config fail");
		return -EINVAL;
	}

	return ret;
}

int is_lib_get_offline_data(struct is_lib_isp *this, u32 instance,
	void *data_desc, int fcount)
{
	int ret = 0;
#if defined(USE_OFFLINE_PROCESSING)
	ret = CALL_LIBOP(this, get_offline_data, this->object, instance,
				(struct offline_data_desc *)data_desc, fcount);
#endif
	return ret;
}
