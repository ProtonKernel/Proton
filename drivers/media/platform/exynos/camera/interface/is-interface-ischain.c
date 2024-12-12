/*
 * drivers/media/video/exynos/fimc-is-mc2/interface/fimc-is-interface-ishcain.c
 *
 * Copyright (c) 2014 Samsung Electronics Co., Ltd.
 *       http://www.samsung.com
 *
 * The header file related to camera
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#include <linux/workqueue.h>
#include <linux/bug.h>
#include <linux/videodev2.h>
#include <linux/videodev2_exynos_camera.h>

#include "is-core.h"
#include "is-err.h"
#include "is-groupmgr.h"

#include "is-interface.h"
#include "is-debug.h"
#include "is-work.h"

#include "is-interface-ischain.h"
#include "is-interface-library.h"
#include "../include/is-hw.h"

u32 __iomem *notify_fcount_sen0;
u32 __iomem *notify_fcount_sen1;
u32 __iomem *notify_fcount_sen2;
u32 __iomem *notify_fcount_sen3;

#define init_request_barrier(itf) mutex_init(&itf->request_barrier)
#define enter_request_barrier(itf) mutex_lock(&itf->request_barrier);
#define exit_request_barrier(itf) mutex_unlock(&itf->request_barrier);
#define init_process_barrier(itf) spin_lock_init(&itf->process_barrier);
#define enter_process_barrier(itf) spin_lock_irq(&itf->process_barrier);
#define exit_process_barrier(itf) spin_unlock_irq(&itf->process_barrier);

extern struct is_sysfs_debug sysfs_debug;
extern struct is_lib_support gPtr_lib_support;

static int is_interface_3aa_probe(struct is_interface_ischain *itfc,
	int hw_id, struct platform_device *pdev, struct is_hardware *hardware)
{
	struct is_interface_hwip *itf_3aa = NULL;
	int i, ret = 0;
	int hw_slot = -1;
	int handler_id = 0;
	struct is_hw_ip *hw_ip;

	FIMC_BUG(!itfc);
	FIMC_BUG(!pdev);

	switch (hw_id) {
	case DEV_HW_3AA0:
		handler_id = ID_3AA_0;
		break;
	case DEV_HW_3AA1:
		handler_id = ID_3AA_1;
		break;
	case DEV_HW_3AA2:
		handler_id = ID_3AA_2;
		break;
	case DEV_HW_3AA3:
		handler_id = ID_3AA_3;
		break;
	default:
		err_itfc("invalid hw_id(%d)", hw_id);
		return -EINVAL;
	}

	hw_slot = is_hw_slot_id(hw_id);
	if (!valid_hw_slot_id(hw_slot)) {
		err_itfc("invalid hw_slot (%d) ", hw_slot);
		return -EINVAL;
	}

	hw_ip = &(hardware->hw_ip[hw_slot]);
	itfc->itf_ip[hw_slot].hw_ip = hw_ip;

	itf_3aa = &itfc->itf_ip[hw_slot];
	itf_3aa->id = hw_id;
	itf_3aa->state = 0;

	ret = is_hw_get_address(itf_3aa, pdev, hw_id);
	if (ret) {
		err_itfc("[ID:%2d] hw_get_address failed (%d)", hw_id, ret);
		return -EINVAL;
	}

	ret = is_hw_get_irq(itf_3aa, pdev, hw_id);
	if (ret) {
		err_itfc("[ID:%2d] hw_get_irq failed (%d)", hw_id, ret);
		return -EINVAL;
	}

	ret = is_hw_request_irq(itf_3aa, hw_id);
	if (ret) {
		err_itfc("[ID:%2d] hw_request_irq failed (%d)", hw_id, ret);
		return -EINVAL;
	}

	for (i = 0; i < INTR_HWIP_MAX; i++) {
		itf_3aa->handler[i].valid = false;

		gPtr_lib_support.intr_handler_taaisp[handler_id][i]
			= (struct hwip_intr_handler *)&itf_3aa->handler[i];

		hw_ip->intr_handler[i] = (struct hwip_intr_handler *)&itf_3aa->handler[i];
	}

	/* library data settings */
	if (!gPtr_lib_support.minfo) {
		gPtr_lib_support.itfc		= itfc;
		gPtr_lib_support.minfo		= itfc->minfo;

		gPtr_lib_support.pdev		= pdev;

#if !defined(ENABLE_DYNAMIC_MEM)
		info_itfc("[ID:%2d] kvaddr for taaisp: 0x%lx\n", hw_id,
			CALL_BUFOP(gPtr_lib_support.minfo->pb_taaisp, kvaddr,
					gPtr_lib_support.minfo->pb_taaisp));
#endif
	}

	set_bit(IS_CHAIN_IF_STATE_INIT, &itf_3aa->state);

	dbg_itfc("[ID:%2d] probe done\n", hw_id);

	return ret;
}

static int is_interface_isp_probe(struct is_interface_ischain *itfc,
	int hw_id, struct platform_device *pdev, struct is_hardware *hardware)
{
	struct is_interface_hwip *itf_isp = NULL;
	int i, ret = 0;
	int hw_slot = -1;
	int handler_id = 0;
	struct is_hw_ip *hw_ip;

	FIMC_BUG(!itfc);
	FIMC_BUG(!pdev);

	switch (hw_id) {
	case DEV_HW_ISP0:
		handler_id = ID_ISP_0;
		break;
	case DEV_HW_ISP1:
		handler_id = ID_ISP_1;
		break;
	default:
		err_itfc("invalid hw_id(%d)", hw_id);
		return -EINVAL;
	}

	hw_slot = is_hw_slot_id(hw_id);
	if (!valid_hw_slot_id(hw_slot)) {
		err_itfc("invalid hw_slot (%d) ", hw_slot);
		return -EINVAL;
	}

	hw_ip = &(hardware->hw_ip[hw_slot]);
	itfc->itf_ip[hw_slot].hw_ip = hw_ip;

	itf_isp = &itfc->itf_ip[hw_slot];
	itf_isp->id = hw_id;
	itf_isp->state = 0;

	ret = is_hw_get_address(itf_isp, pdev, hw_id);
	if (ret) {
		err_itfc("[ID:%2d] hw_get_address failed (%d)", hw_id, ret);
		return -EINVAL;
	}

	ret = is_hw_get_irq(itf_isp, pdev, hw_id);
	if (ret) {
		err_itfc("[ID:%2d] hw_get_irq failed (%d)", hw_id, ret);
		return -EINVAL;
	}

	ret = is_hw_request_irq(itf_isp, hw_id);
	if (ret) {
		err_itfc("[ID:%2d] hw_request_irq failed (%d)", hw_id, ret);
		return -EINVAL;
	}

	for (i = 0; i < INTR_HWIP_MAX; i++) {
		itf_isp->handler[i].valid = false;

		/* TODO: this is not cool */
		gPtr_lib_support.intr_handler_taaisp[handler_id][i]
			= (struct hwip_intr_handler *)&itf_isp->handler[i];
	}

	/* library data settings */
	if (!gPtr_lib_support.minfo) {
		gPtr_lib_support.itfc		= itfc;
		gPtr_lib_support.minfo		= itfc->minfo;

		gPtr_lib_support.pdev		= pdev;

#if !defined(ENABLE_DYNAMIC_MEM)
		info_itfc("[ID:%2d] kvaddr for taaisp: 0x%lx\n", hw_id,
			CALL_BUFOP(gPtr_lib_support.minfo->pb_taaisp, kvaddr,
					gPtr_lib_support.minfo->pb_taaisp));
#endif
	}

	set_bit(IS_CHAIN_IF_STATE_INIT, &itf_isp->state);

	dbg_itfc("[ID:%2d] probe done\n", hw_id);

	return ret;
}

static int is_interface_mcsc_probe(struct is_interface_ischain *itfc,
	int hw_id, struct platform_device *pdev, struct is_hardware *hardware)
{
	struct is_interface_hwip *itf_scaler = NULL;
	int ret = 0;
	int hw_slot = -1;
	struct is_hw_ip *hw_ip;

	FIMC_BUG(!itfc);
	FIMC_BUG(!pdev);

	hw_slot = is_hw_slot_id(hw_id);
	if (!valid_hw_slot_id(hw_slot)) {
		err_itfc("invalid hw_slot (%d) ", hw_slot);
		return -EINVAL;
	}

	hw_ip = &(hardware->hw_ip[hw_slot]);
	itfc->itf_ip[hw_slot].hw_ip = hw_ip;

	itf_scaler = &itfc->itf_ip[hw_slot];
	itf_scaler->id = hw_id;
	itf_scaler->state = 0;

	ret = is_hw_get_address(itf_scaler, pdev, hw_id);
	if (ret) {
		err_itfc("[ID:%2d] hw_get_address failed (%d)", hw_id, ret);
		return -EINVAL;
	}

	ret = is_hw_get_irq(itf_scaler, pdev, hw_id);
	if (ret) {
		err_itfc("[ID:%2d] hw_get_irq failed (%d)", hw_id, ret);
		return -EINVAL;
	}

	ret = is_hw_request_irq(itf_scaler, hw_id);
	if (ret) {
		err_itfc("[ID:%2d] hw_request_irq failed (%d)", hw_id, ret);
		return -EINVAL;
	}

	set_bit(IS_CHAIN_IF_STATE_INIT, &itf_scaler->state);

	dbg_itfc("[ID:%2d] probe done\n", hw_id);

	return ret;
}

static int is_interface_vra_probe(struct is_interface_ischain *itfc,
	int hw_id, struct platform_device *pdev, struct is_hardware *hardware)
{
	struct is_interface_hwip *itf_vra = NULL;
	int ret = 0;
	int hw_slot = -1;
	struct is_hw_ip *hw_ip;

	FIMC_BUG(!itfc);
	FIMC_BUG(!pdev);

	hw_slot = is_hw_slot_id(hw_id);
	if (!valid_hw_slot_id(hw_slot)) {
		err_itfc("invalid hw_slot (%d) ", hw_slot);
		return -EINVAL;
	}

	hw_ip = &(hardware->hw_ip[hw_slot]);
	itfc->itf_ip[hw_slot].hw_ip = hw_ip;

	itf_vra = &itfc->itf_ip[hw_slot];
	itf_vra->id = hw_id;
	itf_vra->state = 0;

	ret = is_hw_get_address(itf_vra, pdev, hw_id);
	if (ret) {
		err_itfc("[ID:%2d] hw_get_address failed (%d)", hw_id, ret);
		return -EINVAL;
	}

	ret = is_hw_get_irq(itf_vra, pdev, hw_id);
	if (ret) {
		err_itfc("[ID:%2d] hw_get_irq failed (%d)", hw_id, ret);
		return -EINVAL;
	}

	ret = is_hw_request_irq(itf_vra, hw_id);
	if (ret) {
		err_itfc("[ID:%2d] hw_request_irq failed (%d)", hw_id, ret);
		return -EINVAL;
	}

	/* library data settings */
	if (!gPtr_lib_support.minfo) {
		gPtr_lib_support.itfc		= itfc;
		gPtr_lib_support.minfo		= itfc->minfo;

		gPtr_lib_support.pdev		= pdev;

		info_itfc("[ID:%2d] kvaddr for vra: 0x%lx\n", hw_id,
			CALL_BUFOP(gPtr_lib_support.minfo->pb_vra, kvaddr,
					gPtr_lib_support.minfo->pb_vra));
	} else {
		info_itfc("[ID:%2d] kvaddr for vra: 0x%lx\n", hw_id,
			CALL_BUFOP(gPtr_lib_support.minfo->pb_vra, kvaddr,
					gPtr_lib_support.minfo->pb_vra));
	}

	set_bit(IS_CHAIN_IF_STATE_INIT, &itf_vra->state);

	dbg_itfc("[ID:%2d] probe done\n", hw_id);

	return ret;
}

static int is_interface_clh_probe(struct is_interface_ischain *itfc,
	int hw_id, struct platform_device *pdev, struct is_hardware *hardware)
{
	struct is_interface_hwip *itf_clh = NULL;
	int i, ret = 0;
	int hw_slot = -1;
	int handler_id = 0;
	struct is_hw_ip *hw_ip;

	FIMC_BUG(!itfc);
	FIMC_BUG(!pdev);

	switch (hw_id) {
	case DEV_HW_CLH0:
		handler_id = ID_CLH_0;
		break;
	default:
		err_itfc("invalid hw_id(%d)", hw_id);
		return -EINVAL;
	}

	hw_slot = is_hw_slot_id(hw_id);
	if (!valid_hw_slot_id(hw_slot)) {
		err_itfc("invalid hw_slot (%d) ", hw_slot);
		return -EINVAL;
	}

	hw_ip = &(hardware->hw_ip[hw_slot]);
	itfc->itf_ip[hw_slot].hw_ip = hw_ip;

	itf_clh = &itfc->itf_ip[hw_slot];
	itf_clh->id = hw_id;
	itf_clh->state = 0;

	ret = is_hw_get_address(itf_clh, pdev, hw_id);
	if (ret) {
		err_itfc("[ID:%2d] hw_get_address failed (%d)", hw_id, ret);
		return -EINVAL;
	}

	ret = is_hw_get_irq(itf_clh, pdev, hw_id);
	if (ret) {
		err_itfc("[ID:%2d] hw_get_irq failed (%d)", hw_id, ret);
		return -EINVAL;
	}

	ret = is_hw_request_irq(itf_clh, hw_id);
	if (ret) {
		err_itfc("[ID:%2d] hw_request_irq failed (%d)", hw_id, ret);
		return -EINVAL;
	}

	for (i = 0; i < INTR_HWIP_MAX; i++) {
		itf_clh->handler[i].valid = false;

		/* TODO: this is not cool */
		gPtr_lib_support.intr_handler_taaisp[handler_id][i]
			= (struct hwip_intr_handler *)&itf_clh->handler[i];
	}

	/* library data settings */
	if (!gPtr_lib_support.minfo) {
		gPtr_lib_support.itfc		= itfc;
		gPtr_lib_support.minfo		= itfc->minfo;

		gPtr_lib_support.pdev		= pdev;

#if !defined(ENABLE_DYNAMIC_MEM)
		info_itfc("[ID:%2d] kvaddr for taaisp: 0x%lx\n", hw_id,
			CALL_BUFOP(gPtr_lib_support.minfo->pb_clahe, kvaddr,
					gPtr_lib_support.minfo->pb_clahe));
#endif
	}

	set_bit(IS_CHAIN_IF_STATE_INIT, &itf_clh->state);

	dbg_itfc("[ID:%2d] probe done\n", hw_id);

	return ret;
}

static int is_interface_ypp_probe(struct is_interface_ischain *itfc,
	int hw_id, struct platform_device *pdev, struct is_hardware *hardware)
{
	struct is_interface_hwip *itf_ypp = NULL;
	int i, ret = 0;
	int hw_slot = -1;
	int handler_id = 0;
	struct is_hw_ip *hw_ip;

	FIMC_BUG(!itfc);
	FIMC_BUG(!pdev);

	switch (hw_id) {
	case DEV_HW_YPP:
		handler_id = ID_YPP;
		break;
	default:
		err_itfc("invalid hw_id(%d)", hw_id);
		return -EINVAL;
	}

	hw_slot = is_hw_slot_id(hw_id);
	if (!valid_hw_slot_id(hw_slot)) {
		err_itfc("invalid hw_slot (%d) ", hw_slot);
		return -EINVAL;
	}

	hw_ip = &(hardware->hw_ip[hw_slot]);
	itfc->itf_ip[hw_slot].hw_ip = hw_ip;

	itf_ypp = &itfc->itf_ip[hw_slot];
	itf_ypp->id = hw_id;
	itf_ypp->state = 0;

	ret = is_hw_get_address(itf_ypp, pdev, hw_id);
	if (ret) {
		err_itfc("[ID:%2d] hw_get_address failed (%d)", hw_id, ret);
		return -EINVAL;
	}

	ret = is_hw_get_irq(itf_ypp, pdev, hw_id);
	if (ret) {
		err_itfc("[ID:%2d] hw_get_irq failed (%d)", hw_id, ret);
		return -EINVAL;
	}

	ret = is_hw_request_irq(itf_ypp, hw_id);
	if (ret) {
		err_itfc("[ID:%2d] hw_request_irq failed (%d)", hw_id, ret);
		return -EINVAL;
	}

	for (i = 0; i < INTR_HWIP_MAX; i++) {
		itf_ypp->handler[i].valid = false;

		/* TODO: this is not cool */
		gPtr_lib_support.intr_handler_taaisp[handler_id][i]
			= (struct hwip_intr_handler *)&itf_ypp->handler[i];
	}

	/* library data settings */
	if (!gPtr_lib_support.minfo) {
		gPtr_lib_support.itfc		= itfc;
		gPtr_lib_support.minfo		= itfc->minfo;
		gPtr_lib_support.pdev		= pdev;
#if !defined(ENABLE_DYNAMIC_MEM)
		info_itfc("[ID:%2d] kvaddr for taaisp: 0x%lx\n", hw_id,
		CALL_BUFOP(gPtr_lib_support.minfo->pb_clahe, kvaddr,
		gPtr_lib_support.minfo->pb_clahe));
#endif
	}

	set_bit(IS_CHAIN_IF_STATE_INIT, &itf_ypp->state);

	dbg_itfc("[ID:%2d] probe done\n", hw_id);

	return ret;
}

static int is_interface_lme_probe(struct is_interface_ischain *itfc,
	int hw_id, struct platform_device *pdev, struct is_hardware *hardware)
{
	struct is_interface_hwip *itf_lme = NULL;
	int i, ret = 0;
	int hw_slot = -1;
	int handler_id = 0;
	struct is_hw_ip *hw_ip;

	FIMC_BUG(!itfc);
	FIMC_BUG(!pdev);

	switch (hw_id) {
	case DEV_HW_LME0:
		handler_id = ID_LME_0;
		break;
	case DEV_HW_LME1:
		handler_id = ID_LME_1;
		break;
	default:
		err_itfc("invalid hw_id(%d)", hw_id);
		return -EINVAL;
	}

	hw_slot = is_hw_slot_id(hw_id);
	if (!valid_hw_slot_id(hw_slot)) {
		err_itfc("invalid hw_slot (%d) ", hw_slot);
		return -EINVAL;
	}

	hw_ip = &(hardware->hw_ip[hw_slot]);
	itfc->itf_ip[hw_slot].hw_ip = hw_ip;

	itf_lme = &itfc->itf_ip[hw_slot];
	itf_lme->id = hw_id;
	itf_lme->state = 0;

	ret = is_hw_get_address(itf_lme, pdev, hw_id);
	if (ret) {
		err_itfc("[ID:%2d] hw_get_address failed (%d)", hw_id, ret);
		return -EINVAL;
	}

	ret = is_hw_get_irq(itf_lme, pdev, hw_id);
	if (ret) {
		err_itfc("[ID:%2d] hw_get_irq failed (%d)", hw_id, ret);
		return -EINVAL;
	}

	ret = is_hw_request_irq(itf_lme, hw_id);
	if (ret) {
		err_itfc("[ID:%2d] hw_request_irq failed (%d)", hw_id, ret);
		return -EINVAL;
	}

	for (i = 0; i < INTR_HWIP_MAX; i++) {
		itf_lme->handler[i].valid = false;

		gPtr_lib_support.intr_handler_taaisp[handler_id][i]
			= (struct hwip_intr_handler *)&itf_lme->handler[i];
	}
	if (!gPtr_lib_support.minfo) {
		gPtr_lib_support.itfc		= itfc;
		gPtr_lib_support.minfo		= itfc->minfo;

		gPtr_lib_support.pdev		= pdev;

#if !defined(ENABLE_DYNAMIC_MEM)
		info_itfc("[ID:%2d] kvaddr for taaisp: 0x%lx\n", hw_id,
			CALL_BUFOP(gPtr_lib_support.minfo->pb_clahe, kvaddr,
					gPtr_lib_support.minfo->pb_clahe));
#endif
	}

	set_bit(IS_CHAIN_IF_STATE_INIT, &itf_lme->state);

	dbg_itfc("[ID:%2d] probe done\n", hw_id);

	return ret;
}

int is_interface_ischain_probe(struct is_interface_ischain *this,
	struct is_hardware *hardware, struct is_resourcemgr *resourcemgr,
	struct platform_device *pdev, ulong core_regs)
{
	enum is_hardware_id hw_id = DEV_HW_END;
	int ret = 0;
	struct device *dev;
	struct exynos_platform_is *pdata;
	int i;

	FIMC_BUG(!this);
	FIMC_BUG(!resourcemgr);

	this->state = 0;
	this->minfo = &resourcemgr->minfo;

	dev = &pdev->dev;
	pdata = dev->platform_data;

	for (i = 0; i < pdata->num_of_ip.taa; i++) {
		hw_id = DEV_HW_3AA0 + i;
		ret = is_interface_3aa_probe(this, hw_id, pdev, hardware);
		if (ret) {
			err_itfc("interface probe fail (hw_id: %d)", hw_id);
			return -EINVAL;
		}
	}

	for (i = 0; i < pdata->num_of_ip.isp; i++) {
		hw_id = DEV_HW_ISP0 + i;
		ret = is_interface_isp_probe(this, hw_id, pdev, hardware);
		if (ret) {
			err_itfc("interface probe fail (hw_id: %d)", hw_id);
			return -EINVAL;
		}
	}

	for (i = 0; i < pdata->num_of_ip.mcsc; i++) {
		hw_id = DEV_HW_MCSC0 + i;
		ret = is_interface_mcsc_probe(this, hw_id, pdev, hardware);
		if (ret) {
			err_itfc("interface probe fail (hw_ip: %d)", hw_id);
			return -EINVAL;
		}
	}

	for (i = 0; i < pdata->num_of_ip.vra; i++) {
		hw_id = DEV_HW_VRA + i;
		ret = is_interface_vra_probe(this, hw_id, pdev, hardware);
		if (ret) {
			err_itfc("interface probe fail (hw_ip: %d)", hw_id);
			return -EINVAL;
		}
	}

	for (i = 0; i < pdata->num_of_ip.clh; i++) {
		hw_id = DEV_HW_CLH0 + i;
		ret = is_interface_clh_probe(this, hw_id, pdev, hardware);
		if (ret) {
			err_itfc("interface probe fail (hw_ip: %d)", hw_id);
			return -EINVAL;
		}
	}

	for (i = 0; i < pdata->num_of_ip.ypp; i++) {
		hw_id = DEV_HW_YPP + i;
		ret = is_interface_ypp_probe(this, hw_id, pdev, hardware);
		if (ret) {
			err_itfc("interface probe fail (hw_ip: %d)", hw_id);
			return -EINVAL;
		}
	}

	for (i = 0; i < pdata->num_of_ip.lme; i++) {
		hw_id = DEV_HW_LME0 + i;
		ret = is_interface_lme_probe(this, hw_id, pdev, hardware);
		if (ret) {
			err_itfc("interface probe fail (hw_ip: %d)", hw_id);
			return -EINVAL;
		}
	}

	set_bit(IS_CHAIN_IF_STATE_INIT, &this->state);
	dbg_itfc("interface ishchain probe done (hw_id: %d)(ret: %d)", hw_id, ret);

	return ret;
}

static inline void wq_func_schedule(struct is_interface *itf,
	struct work_struct *work_wq)
{
	if (itf->workqueue)
		queue_work(itf->workqueue, work_wq);
	else
		schedule_work(work_wq);
}

static void wq_func_subdev(struct is_subdev *leader,
	struct is_subdev *subdev,
	struct is_frame *sub_frame,
	u32 fcount, u32 rcount, u32 status)
{
	u32 done_state = VB2_BUF_STATE_DONE;
	u32 findex, mindex;
	struct is_video_ctx *ldr_vctx, *sub_vctx;
	struct is_framemgr *ldr_framemgr, *sub_framemgr;
	struct is_frame *ldr_frame = NULL;

	FIMC_BUG_VOID(!sub_frame);

	ldr_vctx = leader->vctx;
	sub_vctx = subdev->vctx;

	ldr_framemgr = GET_FRAMEMGR(ldr_vctx);
	sub_framemgr = GET_FRAMEMGR(sub_vctx);
	if (!ldr_framemgr) {
		mserr("ldr_framemgr is NULL", subdev, subdev);
		goto p_err;
	}
	if (!sub_framemgr) {
		mserr("sub_framemgr is NULL", subdev, subdev);
		goto p_err;
	}

	findex = sub_frame->stream->findex;
	mindex = ldr_vctx->queue.buf_maxcount;
	if (findex >= mindex) {
		mserr("findex(%d) is invalid(max %d)", subdev, subdev, findex, mindex);
		done_state = VB2_BUF_STATE_ERROR;
		sub_frame->stream->fvalid = 0;
		goto complete;
	}

	ldr_frame = &ldr_framemgr->frames[findex];
	if (ldr_frame->fcount != fcount) {
		mserr("frame mismatched(ldr%d, sub%d)", subdev, subdev, ldr_frame->fcount, fcount);
		done_state = VB2_BUF_STATE_ERROR;
		sub_frame->stream->fvalid = 0;
		goto complete;
	}

	if (status) {
		msrinfo("[ERR] NDONE(%d, E%d)\n", subdev, subdev, ldr_frame, sub_frame->index, status);
		done_state = VB2_BUF_STATE_ERROR;
		sub_frame->stream->fvalid = 0;
	} else {
		msrdbgs(1, " DONE(%d)\n", subdev, subdev, ldr_frame, sub_frame->index);
		sub_frame->stream->fvalid = 1;
	}

	/* For supporting multi input to single output */
	if (subdev->vctx->video->try_smp) {
		subdev->vctx->video->try_smp = false;
		up(&subdev->vctx->video->smp_multi_input);
	}

	/* Skip done when current frame is doing stripe_process. */
	if (!status && (sub_frame->state == FS_STRIPE_PROCESS ||
		(ldr_frame->stripe_info.region_num &&
		sub_frame->stripe_info.region_id < ldr_frame->stripe_info.region_num - 1))) {
		sub_frame->stripe_info.region_id++;
		return;
	}

	clear_bit(subdev->id, &ldr_frame->out_flag);

complete:
	sub_frame->stripe_info.region_id = 0;
	sub_frame->stream->fcount = fcount;
	sub_frame->stream->rcount = rcount;
	sub_frame->fcount = fcount;
	sub_frame->rcount = rcount;

	trans_frame(sub_framemgr, sub_frame, FS_COMPLETE);

	CALL_VOPS(sub_vctx, done, sub_frame->index, done_state);

p_err:
	return;

}

static void wq_func_frame(struct is_subdev *leader,
	struct is_subdev *subdev,
	u32 fcount, u32 rcount, u32 status)
{
	ulong flags;
	struct is_framemgr *framemgr;
	struct is_frame *frame;

	FIMC_BUG_VOID(!leader);
	FIMC_BUG_VOID(!subdev);
	FIMC_BUG_VOID(!leader->vctx);
	FIMC_BUG_VOID(!subdev->vctx);
	FIMC_BUG_VOID(!leader->vctx->video);
	FIMC_BUG_VOID(!subdev->vctx->video);

	framemgr = GET_FRAMEMGR(subdev->vctx);

	framemgr_e_barrier_irqs(framemgr, FMGR_IDX_4, flags);

	frame = find_stripe_process_frame(framemgr, fcount);
	if (!frame)
		frame = peek_frame(framemgr, FS_PROCESS);

	if (frame) {
		if (!frame->stream) {
			mserr("stream is NULL", subdev, subdev);
			BUG();
		}

		if (unlikely(frame->stream->fcount != fcount)) {
			while (frame) {
				if (fcount == frame->stream->fcount) {
					wq_func_subdev(leader, subdev, frame, fcount, rcount, status);
					break;
				} else if (fcount > frame->stream->fcount) {
					wq_func_subdev(leader, subdev, frame, frame->stream->fcount, rcount, 0xF);

					/* get next subdev frame */
					frame = peek_frame(framemgr, FS_PROCESS);
				} else {
					mswarn("frame done is ignored (F %d < SF %d)",
						subdev, subdev, fcount, frame->stream->fcount);
					break;
				}
			}
		} else {
			wq_func_subdev(leader, subdev, frame, fcount, rcount, status);
		}
	} else {
		mserr("frame done(%p) is occured without request", subdev, subdev, frame);
	}

	framemgr_x_barrier_irqr(framemgr, FMGR_IDX_4, flags);
}

static struct is_subdev *get_subdev_from_wq_id(struct is_device_ischain *device, u32 wq_id)
{
	struct is_subdev *subdev;

	switch (wq_id) {
	case WORK_30C_FDONE:
		subdev = &device->txc;
		break;
	case WORK_30P_FDONE:
		subdev = &device->txp;
		break;
	case WORK_30F_FDONE:
		subdev = &device->txf;
		break;
	case WORK_30G_FDONE:
		subdev = &device->txg;
		break;
	case WORK_30O_FDONE:
		subdev = &device->txo;
		break;
	case WORK_30L_FDONE:
		subdev = &device->txl;
		break;
	case WORK_LME0S_FDONE:
		subdev = &device->lmes;
		break;
	case WORK_LME0C_FDONE:
		subdev = &device->lmec;
		break;
	case WORK_ORB0C_FDONE:
		subdev = &device->orbxc;
		break;
	case WORK_I0C_FDONE:
		subdev = &device->ixc;
		break;
	case WORK_I0P_FDONE:
		subdev = &device->ixp;
		break;
	case WORK_I0T_FDONE:
		subdev = &device->ixt;
		break;
	case WORK_I0G_FDONE:
		subdev = &device->ixg;
		break;
	case WORK_I0V_FDONE:
		subdev = &device->ixv;
		break;
	case WORK_I0W_FDONE:
		subdev = &device->ixw;
		break;
	case WORK_ME0C_FDONE:
		subdev = &device->mexc;
		break;
	case WORK_M0P_FDONE:
		subdev = &device->m0p;
		break;
	case WORK_M1P_FDONE:
		subdev = &device->m1p;
		break;
	case WORK_M2P_FDONE:
		subdev = &device->m2p;
		break;
	case WORK_M3P_FDONE:
		subdev = &device->m3p;
		break;
	case WORK_M4P_FDONE:
		subdev = &device->m4p;
		break;
	case WORK_M5P_FDONE:
		subdev = &device->m5p;
		break;
	case WORK_CL0C_FDONE:
		subdev = &device->clhc;
		break;
	default:
		merr("invalid wq_id(%d)", device, wq_id);
		subdev = NULL;
		break;
	}

	return subdev;
}

static void wq_func_subdev_xxx(struct work_struct *data, u32 wq_id)
{
	u32 instance, fcount, rcount, status;
	struct is_interface *itf;
	struct is_device_ischain *device;
	struct is_subdev *leader, *subdev;
	struct is_work *work;
	struct is_msg *msg;

	itf = container_of(data, struct is_interface, work_wq[wq_id]);

	get_req_work(&itf->work_list[wq_id], &work);
	while (work) {
		msg = &work->msg;
		instance = msg->instance;
		fcount = msg->param1;
		rcount = msg->param2;
		status = msg->param3;

		if (instance >= IS_STREAM_COUNT) {
			err("instance is invalid(%d)", instance);
			goto p_err;
		}

		device = &((struct is_core *)itf->core)->ischain[instance];
		if (!test_bit(IS_ISCHAIN_OPEN, &device->state)) {
			merr("device is not open", device);
			goto p_err;
		}

		subdev = get_subdev_from_wq_id(device, wq_id);
		if (!subdev) {
			merr("subdev is NULL", device);
			goto p_err;
		}

		if (!test_bit(IS_SUBDEV_START, &subdev->state)) {
			merr("subdev is not start", device);
			goto p_err;
		}

		leader = subdev->leader;
		if (!leader) {
			merr("leader is NULL", device);
			goto p_err;
		}

		wq_func_frame(leader, subdev, fcount, rcount, status);

p_err:
		set_free_work(&itf->work_list[wq_id], work);
		get_req_work(&itf->work_list[wq_id], &work);
	}
}

static void wq_func_30c(struct work_struct *data)
{
	wq_func_subdev_xxx(data, WORK_30C_FDONE);
}

static void wq_func_30p(struct work_struct *data)
{
	wq_func_subdev_xxx(data, WORK_30P_FDONE);
}

static void wq_func_30f(struct work_struct *data)
{
	wq_func_subdev_xxx(data, WORK_30F_FDONE);
}

static void wq_func_30g(struct work_struct *data)
{
	wq_func_subdev_xxx(data, WORK_30G_FDONE);
}

static void wq_func_30o(struct work_struct *data)
{
	wq_func_subdev_xxx(data, WORK_30O_FDONE);
}

static void wq_func_30l(struct work_struct *data)
{
	wq_func_subdev_xxx(data, WORK_30L_FDONE);
}

static void wq_func_orb0c(struct work_struct *data)
{
	wq_func_subdev_xxx(data, WORK_ORB0C_FDONE);
}

static void wq_func_i0c(struct work_struct *data)
{
	wq_func_subdev_xxx(data, WORK_I0C_FDONE);
}

#if defined(SOC_TNR_MERGER)
static void wq_func_i0t(struct work_struct *data)
{
	wq_func_subdev_xxx(data, WORK_I0T_FDONE);
}

static void wq_func_i0g(struct work_struct *data)
{
	wq_func_subdev_xxx(data, WORK_I0G_FDONE);
}

static void wq_func_i0v(struct work_struct *data)
{
	wq_func_subdev_xxx(data, WORK_I0V_FDONE);
}

static void wq_func_i0w(struct work_struct *data)
{
	wq_func_subdev_xxx(data, WORK_I0W_FDONE);
}
#endif

static void wq_func_i0p(struct work_struct *data)
{
	wq_func_subdev_xxx(data, WORK_I0P_FDONE);
}

static void wq_func_me0c(struct work_struct *data)
{
	wq_func_subdev_xxx(data, WORK_ME0C_FDONE);
}

static void wq_func_m0p(struct work_struct *data)
{
	wq_func_subdev_xxx(data, WORK_M0P_FDONE);
}

static void wq_func_m1p(struct work_struct *data)
{
	wq_func_subdev_xxx(data, WORK_M1P_FDONE);
}

static void wq_func_m2p(struct work_struct *data)
{
	wq_func_subdev_xxx(data, WORK_M2P_FDONE);
}

static void wq_func_m3p(struct work_struct *data)
{
	wq_func_subdev_xxx(data, WORK_M3P_FDONE);
}

static void wq_func_m4p(struct work_struct *data)
{
	wq_func_subdev_xxx(data, WORK_M4P_FDONE);
}

static void wq_func_m5p(struct work_struct *data)
{
	wq_func_subdev_xxx(data, WORK_M5P_FDONE);
}

static void wq_func_cl0c(struct work_struct *data)
{
	wq_func_subdev_xxx(data, WORK_CL0C_FDONE);
}

static void wq_func_lme0s(struct work_struct *data)
{
	wq_func_subdev_xxx(data, WORK_LME0S_FDONE);
}

static void wq_func_lme0c(struct work_struct *data)
{
	wq_func_subdev_xxx(data, WORK_LME0C_FDONE);
}

static void wq_func_group_xxx(struct is_groupmgr *groupmgr,
	struct is_group *group,
	struct is_framemgr *framemgr,
	struct is_frame *frame,
	struct is_video_ctx *vctx,
	u32 status, u32 lindex, u32 hindex)
{
	u32 done_state = VB2_BUF_STATE_DONE;
	enum is_frame_state next_state = FS_COMPLETE;

	FIMC_BUG_VOID(!vctx);
	FIMC_BUG_VOID(!framemgr);
	FIMC_BUG_VOID(!frame);

	/* perframe error control */
	if (test_bit(IS_SUBDEV_PARAM_ERR, &group->leader.state)) {
		if (!status) {
			if (frame->lindex || frame->hindex)
				clear_bit(IS_SUBDEV_PARAM_ERR, &group->leader.state);
			else
				status = SHOT_ERR_PERFRAME;
		}
	} else {
		if (status && (frame->lindex || frame->hindex))
			set_bit(IS_SUBDEV_PARAM_ERR, &group->leader.state);
	}

	/* Collecting sensor group NDONE status */
	if (test_bit(IS_GROUP_OTF_INPUT, &group->state))
		status = status ? status : frame->result;

	if (status) {
		mgrinfo("[ERR] NDONE(%d, E%d(L%X H%X))\n", group, group, frame, frame->index, status,
			lindex, hindex);
		done_state = VB2_BUF_STATE_ERROR;
		frame->result = status;

		if (status == IS_SHOT_OVERFLOW) {
#ifdef OVERFLOW_PANIC_ENABLE_ISCHAIN
			panic("G%d overflow", group->id);
#else
			err("G%d overflow", group->id);
#endif
		}
	} else {
		mgrdbgs(1, " DONE(%d)\n", group, group, frame, frame->index);
	}

	/* Cache Invalidation */
	is_ischain_meta_invalid(frame);

	frame->stripe_info.region_id++;
	clear_bit(group->leader.id, &frame->out_flag);
	is_group_done(groupmgr, group, frame, done_state);

	/*
	 * Skip done when current frame is doing stripe process
	 */
	if (!status && frame->state == FS_STRIPE_PROCESS)
		return;

	frame->stripe_info.region_id = 0;
	frame->stripe_info.region_num  = 0;

	/*
	 * Shot Batch Mode
	 * Just set free state for the internal group frames.
	 */
	if (frame->cur_shot_idx)
		next_state = FS_FREE;

	trans_frame(framemgr, frame, next_state);

	/* Only last shot can trigger buffer_done for user */
	if (frame->cur_shot_idx == frame->num_shots - 1)
		CALL_VOPS(vctx, done, frame->index, done_state);
}

void wq_func_group(struct is_device_ischain *device,
	struct is_groupmgr *groupmgr,
	struct is_group *group,
	struct is_framemgr *framemgr,
	struct is_frame *frame,
	struct is_video_ctx *vctx,
	u32 status, u32 fcount)
{
	u32 lindex = 0;
	u32 hindex = 0;

	FIMC_BUG_VOID(!groupmgr);
	FIMC_BUG_VOID(!group);
	FIMC_BUG_VOID(!framemgr);
	FIMC_BUG_VOID(!frame);
	FIMC_BUG_VOID(!vctx);

	TIME_SHOT(TMS_SDONE);
	/*
	 * complete count should be lower than 3 when
	 * buffer is queued or overflow can be occured
	 */
	if (framemgr->queued_count[FS_COMPLETE] >= DIV_ROUND_UP(framemgr->num_frames, 2))
		mginfo("[%s] complete bufs : %d", device, group, __func__, (framemgr->queued_count[FS_COMPLETE] + 1));

	if (status) {
		lindex = frame->shot->ctl.vendor_entry.lowIndexParam;
		lindex &= ~frame->shot->dm.vendor_entry.lowIndexParam;
		hindex = frame->shot->ctl.vendor_entry.highIndexParam;
		hindex &= ~frame->shot->dm.vendor_entry.highIndexParam;
	}

	if (unlikely(fcount != frame->fcount)) {
		if (test_bit(IS_GROUP_OTF_INPUT, &group->state)) {
			while (frame) {
				if (fcount == frame->fcount) {
					wq_func_group_xxx(groupmgr, group, framemgr, frame, vctx,
						status, lindex, hindex);
					break;
				} else if (fcount > frame->fcount) {
					wq_func_group_xxx(groupmgr, group, framemgr, frame, vctx,
						SHOT_ERR_MISMATCH, lindex, hindex);

					/* get next leader frame */
					frame = peek_frame(framemgr, FS_PROCESS);
				} else {
					warn("%d shot done is ignored", fcount);
					break;
				}
			}
		} else {
			wq_func_group_xxx(groupmgr, group, framemgr, frame, vctx,
				SHOT_ERR_MISMATCH, lindex, hindex);
		}
	} else {
		wq_func_group_xxx(groupmgr, group, framemgr, frame, vctx,
			status, lindex, hindex);
	}
}

static void wq_func_shot(struct work_struct *data)
{
	struct is_device_ischain *device;
	struct is_interface *itf;
	struct is_msg *msg;
	struct is_framemgr *framemgr;
	struct is_frame *frame;
	struct is_groupmgr *groupmgr;
	struct is_group *group;
	struct is_group *head;
	struct is_work_list *work_list;
	struct is_work *work;
	struct is_video_ctx *vctx;
	ulong flags;
	u32 fcount, status, cur_shot_idx;
	int instance;
	int group_id;
	struct is_core *core;

	FIMC_BUG_VOID(!data);

	itf = container_of(data, struct is_interface, work_wq[WORK_SHOT_DONE]);
	work_list = &itf->work_list[WORK_SHOT_DONE];
	group  = NULL;
	vctx = NULL;
	framemgr = NULL;

	get_req_work(work_list, &work);
	while (work) {
		core = (struct is_core *)itf->core;
		instance = work->msg.instance;
		group_id = work->msg.group;
		device = &((struct is_core *)itf->core)->ischain[instance];
		groupmgr = device->groupmgr;

		msg = &work->msg;
		fcount = msg->param1;
		status = msg->param2;
		cur_shot_idx = msg->param3;

		switch (group_id) {
		case GROUP_ID(GROUP_ID_PAF0):
		case GROUP_ID(GROUP_ID_PAF1):
		case GROUP_ID(GROUP_ID_PAF2):
		case GROUP_ID(GROUP_ID_PAF3):
			group = &device->group_paf;
			break;
		case GROUP_ID(GROUP_ID_3AA0):
		case GROUP_ID(GROUP_ID_3AA1):
		case GROUP_ID(GROUP_ID_3AA2):
			group = &device->group_3aa;
			break;
		case GROUP_ID(GROUP_ID_LME0):
		case GROUP_ID(GROUP_ID_LME1):
			group = &device->group_lme;
			break;
		case GROUP_ID(GROUP_ID_ISP0):
		case GROUP_ID(GROUP_ID_ISP1):
			group = &device->group_isp;
			break;
		case GROUP_ID(GROUP_ID_YPP):
			group = &device->group_ypp;
			break;
		case GROUP_ID(GROUP_ID_MCS0):
		case GROUP_ID(GROUP_ID_MCS1):
			group = &device->group_mcs;
			break;
		case GROUP_ID(GROUP_ID_VRA0):
			group = &device->group_vra;
			break;
		case GROUP_ID(GROUP_ID_CLH0):
			group = &device->group_clh;
			break;
		default:
			merr("unresolved group (%s)", device,
				group_id_name[GROUP_ID(group_id)]);
			group = NULL;
			vctx = NULL;
			framemgr = NULL;
			goto remain;
		}

		head = group->head;
		vctx = head->leader.vctx;
		if (!vctx) {
			merr("vctx is NULL", device);
			goto remain;
		}

		framemgr = GET_HEAD_GROUP_FRAMEMGR(group, cur_shot_idx);
		if (!framemgr) {
			merr("framemgr is NULL", device);
			goto remain;
		}

		framemgr_e_barrier_irqs(framemgr, FMGR_IDX_5, flags);

		frame = find_stripe_process_frame(framemgr, fcount);
		if (!frame)
			frame = peek_frame(framemgr, FS_PROCESS);

		if (frame) {
			/* clear bit for child group */
			if (group != head)
				clear_bit(group->leader.id, &frame->out_flag);
#ifdef MEASURE_TIME
#ifdef EXTERNAL_TIME
			do_gettimeofday(&frame->tzone[TM_SHOT_D]);
#endif
#endif

			wq_func_group(device, groupmgr, head, framemgr, frame,
				vctx, status, fcount);
		} else {
			mgerr("invalid shot done(%d)", device, head, fcount);
			frame_manager_print_queues(framemgr);
		}

		framemgr_x_barrier_irqr(framemgr, FMGR_IDX_5, flags);

remain:
		set_free_work(work_list, work);
		get_req_work(work_list, &work);
	}
}

static inline void print_framemgr_spinlock_usage(struct is_core *core)
{
	u32 i;
	struct is_device_ischain *ischain;
	struct is_device_sensor *sensor;
	struct is_framemgr *framemgr;
	struct is_subdev *subdev;

	for (i = 0; i < IS_SENSOR_COUNT; ++i) {
		sensor = &core->sensor[i];
		if (test_bit(IS_SENSOR_OPEN, &sensor->state) && (framemgr = &sensor->vctx->queue.framemgr))
			info("[@] framemgr(%s) sindex : 0x%08lX\n", framemgr->name, framemgr->sindex);
	}

	for (i = 0; i < IS_STREAM_COUNT; ++i) {
		ischain = &core->ischain[i];
		if (test_bit(IS_ISCHAIN_OPEN, &ischain->state)) {
			/* 3AA GROUP */
			subdev = &ischain->group_3aa.leader;
			if (test_bit(IS_SUBDEV_OPEN, &subdev->state) && (framemgr = GET_SUBDEV_FRAMEMGR(subdev)))
				info("[@] framemgr(%s) sindex : 0x%08lX\n", framemgr->name, framemgr->sindex);

			subdev = &ischain->txc;
			if (test_bit(IS_SUBDEV_OPEN, &subdev->state) && (framemgr = GET_SUBDEV_FRAMEMGR(subdev)))
				info("[@] framemgr(%s) sindex : 0x%08lX\n", framemgr->name, framemgr->sindex);

			subdev = &ischain->txp;
			if (test_bit(IS_SUBDEV_OPEN, &subdev->state) && (framemgr = GET_SUBDEV_FRAMEMGR(subdev)))
				info("[@] framemgr(%s) sindex : 0x%08lX\n", framemgr->name, framemgr->sindex);

			subdev = &ischain->txf;
			if (test_bit(IS_SUBDEV_OPEN, &subdev->state) && (framemgr = GET_SUBDEV_FRAMEMGR(subdev)))
				info("[@] framemgr(%s) sindex : 0x%08lX\n", framemgr->name, framemgr->sindex);

			subdev = &ischain->txg;
			if (test_bit(IS_SUBDEV_OPEN, &subdev->state) && (framemgr = GET_SUBDEV_FRAMEMGR(subdev)))
				info("[@] framemgr(%s) sindex : 0x%08lX\n", framemgr->name, framemgr->sindex);

			subdev = &ischain->txo;
			if (test_bit(IS_SUBDEV_OPEN, &subdev->state) && (framemgr = GET_SUBDEV_FRAMEMGR(subdev)))
				info("[@] framemgr(%s) sindex : 0x%08lX\n", framemgr->name, framemgr->sindex);

			subdev = &ischain->txl;
			if (test_bit(IS_SUBDEV_OPEN, &subdev->state) && (framemgr = GET_SUBDEV_FRAMEMGR(subdev)))
				info("[@] framemgr(%s) sindex : 0x%08lX\n", framemgr->name, framemgr->sindex);

			subdev = &ischain->orbxc;
			if (test_bit(IS_SUBDEV_OPEN, &subdev->state) && (framemgr = GET_SUBDEV_FRAMEMGR(subdev)))
				info("[@] framemgr(%s) sindex : 0x%08lX\n", framemgr->name, framemgr->sindex);

			/* ISP GROUP */
			subdev = &ischain->group_isp.leader;
			if (test_bit(IS_SUBDEV_OPEN, &subdev->state) && (framemgr = GET_SUBDEV_FRAMEMGR(subdev)))
				info("[@] framemgr(%s) sindex : 0x%08lX\n", framemgr->name, framemgr->sindex);

			subdev = &ischain->ixc;
			if (test_bit(IS_SUBDEV_OPEN, &subdev->state) && (framemgr = GET_SUBDEV_FRAMEMGR(subdev)))
				info("[@] framemgr(%s) sindex : 0x%08lX\n", framemgr->name, framemgr->sindex);

			subdev = &ischain->ixp;
			if (test_bit(IS_SUBDEV_OPEN, &subdev->state) && (framemgr = GET_SUBDEV_FRAMEMGR(subdev)))
				info("[@] framemgr(%s) sindex : 0x%08lX\n", framemgr->name, framemgr->sindex);

			subdev = &ischain->ixt;
			if (test_bit(IS_SUBDEV_OPEN, &subdev->state) && (framemgr = GET_SUBDEV_FRAMEMGR(subdev)))
				info("[@] framemgr(%s) sindex : 0x%08lX\n", framemgr->name, framemgr->sindex);

			subdev = &ischain->ixg;
			if (test_bit(IS_SUBDEV_OPEN, &subdev->state) && (framemgr = GET_SUBDEV_FRAMEMGR(subdev)))
				info("[@] framemgr(%s) sindex : 0x%08lX\n", framemgr->name, framemgr->sindex);

			subdev = &ischain->ixv;
			if (test_bit(IS_SUBDEV_OPEN, &subdev->state) && (framemgr = GET_SUBDEV_FRAMEMGR(subdev)))
				info("[@] framemgr(%s) sindex : 0x%08lX\n", framemgr->name, framemgr->sindex);

			subdev = &ischain->ixw;
			if (test_bit(IS_SUBDEV_OPEN, &subdev->state) && (framemgr = GET_SUBDEV_FRAMEMGR(subdev)))
				info("[@] framemgr(%s) sindex : 0x%08lX\n", framemgr->name, framemgr->sindex);

			subdev = &ischain->mexc;
			if (test_bit(IS_SUBDEV_OPEN, &subdev->state) && (framemgr = GET_SUBDEV_FRAMEMGR(subdev)))
				info("[@] framemgr(%s) sindex : 0x%08lX\n", framemgr->name, framemgr->sindex);
		}
	}
}

IS_TIMER_FUNC(interface_timer)
{
	u32 shot_count, scount_3ax, scount_isp;
	u32 fcount, i;
	unsigned long flags;
	struct is_interface *itf = from_timer(itf, (struct timer_list *)data, timer);

	struct is_core *core;
	struct is_device_ischain *device;
	struct is_device_sensor *sensor;
	struct is_framemgr *framemgr;
	struct is_work_list *work_list;

	FIMC_BUG_VOID(!itf->core);

	if (!test_bit(IS_IF_STATE_OPEN, &itf->state)) {
		is_info("shot timer is terminated\n");
		return;
	}

	core = itf->core;

	for (i = 0; i < IS_STREAM_COUNT; ++i) {
		device = &core->ischain[i];
		shot_count = 0;
		scount_3ax = 0;
		scount_isp = 0;

		sensor = device->sensor;
		if (!sensor)
			continue;

		if (!test_bit(IS_SENSOR_FRONT_START, &sensor->state))
			continue;

		if (test_bit(IS_ISCHAIN_OPEN_STREAM, &device->state)) {
			spin_lock_irqsave(&itf->shot_check_lock, flags);
			if (atomic_read(&itf->shot_check[i])) {
				atomic_set(&itf->shot_check[i], 0);
				atomic_set(&itf->shot_timeout[i], 0);
				spin_unlock_irqrestore(&itf->shot_check_lock, flags);
				continue;
			}
			spin_unlock_irqrestore(&itf->shot_check_lock, flags);

			if (test_bit(IS_GROUP_START, &device->group_3aa.state)) {
				framemgr = GET_HEAD_GROUP_FRAMEMGR(&device->group_3aa, 0);
				if (framemgr) {
					framemgr_e_barrier_irqs(framemgr, FMGR_IDX_6, flags);
					scount_3ax = framemgr->queued_count[FS_PROCESS];
					shot_count += scount_3ax;
					framemgr_x_barrier_irqr(framemgr, FMGR_IDX_6, flags);
				} else {
					minfo("\n### group_3aa framemgr is null ###\n", device);
				}
			}

			if (test_bit(IS_GROUP_START, &device->group_isp.state)) {
				framemgr = GET_HEAD_GROUP_FRAMEMGR(&device->group_isp, 0);
				if (framemgr) {
					framemgr_e_barrier_irqs(framemgr, FMGR_IDX_7, flags);
					scount_isp = framemgr->queued_count[FS_PROCESS];
					shot_count += scount_isp;
					framemgr_x_barrier_irqr(framemgr, FMGR_IDX_7, flags);
				} else {
					minfo("\n### group_isp framemgr is null ###\n", device);
				}
			}

			if (test_bit(IS_GROUP_START, &device->group_vra.state)) {
				framemgr = GET_HEAD_GROUP_FRAMEMGR(&device->group_vra, 0);
				if (framemgr) {
					framemgr_e_barrier_irqs(framemgr, FMGR_IDX_31, flags);
					shot_count += framemgr->queued_count[FS_PROCESS];
					framemgr_x_barrier_irqr(framemgr, FMGR_IDX_31, flags);
				} else {
					minfo("\n### group_vra framemgr is null ###\n", device);
				}
			}
		}

		if (shot_count) {
			atomic_inc(&itf->shot_timeout[i]);
			minfo("shot timer[%d] is increased to %d\n", device,
				i, atomic_read(&itf->shot_timeout[i]));
		}

		if (atomic_read(&itf->shot_timeout[i]) > TRY_TIMEOUT_COUNT) {
			merr("shot command is timeout(%d, %d(%d+%d))", device,
				atomic_read(&itf->shot_timeout[i]),
				shot_count, scount_3ax, scount_isp);

			minfo("\n### 3ax framemgr info ###\n", device);
			if (scount_3ax) {
				framemgr = GET_HEAD_GROUP_FRAMEMGR(&device->group_3aa, 0);
				if (framemgr) {
					framemgr_e_barrier_irqs(framemgr, 0, flags);
					frame_manager_print_queues(framemgr);
					framemgr_x_barrier_irqr(framemgr, 0, flags);
				} else {
					minfo("\n### 3ax framemgr is null ###\n", device);
				}
			}

			minfo("\n### isp framemgr info ###\n", device);
			if (scount_isp) {
				framemgr = GET_HEAD_GROUP_FRAMEMGR(&device->group_isp, 0);
				if (framemgr) {
					framemgr_e_barrier_irqs(framemgr, 0, flags);
					frame_manager_print_queues(framemgr);
					framemgr_x_barrier_irqr(framemgr, 0, flags);
				} else {
					minfo("\n### isp framemgr is null ###\n", device);
				}
			}

			minfo("\n### work list info ###\n", device);
			work_list = &itf->work_list[WORK_SHOT_DONE];
			print_fre_work_list(work_list);
			print_req_work_list(work_list);

			/* framemgr spinlock check */
			print_framemgr_spinlock_usage(core);

			merr("[@] camera firmware panic!!!", device);
			is_debug_s2d(true, "IS_SHOT_CMD_TIMEOUT");

			return;
		}
	}

	for (i = 0; i < IS_SENSOR_COUNT; ++i) {
		sensor = &core->sensor[i];

		if (!test_bit(IS_SENSOR_BACK_START, &sensor->state))
			continue;

		if (!test_bit(IS_SENSOR_FRONT_START, &sensor->state))
			continue;

		fcount = is_sensor_g_fcount(sensor);
		if (fcount == atomic_read(&itf->sensor_check[i])) {
			atomic_inc(&itf->sensor_timeout[i]);
			pr_err ("sensor timer[%d] is increased to %d(fcount : %d)\n", i,
				atomic_read(&itf->sensor_timeout[i]), fcount);
			is_sensor_dump(sensor);
		} else {
			atomic_set(&itf->sensor_timeout[i], 0);
			atomic_set(&itf->sensor_check[i], fcount);
		}

		if (atomic_read(&itf->sensor_timeout[i]) > SENSOR_TIMEOUT_COUNT) {
			merr("sensor is timeout(%d, %d)", sensor,
				atomic_read(&itf->sensor_timeout[i]),
				atomic_read(&itf->sensor_check[i]));

			/* framemgr spinlock check */
			print_framemgr_spinlock_usage(core);

#ifdef SENSOR_PANIC_ENABLE
			/* if panic happened, fw log dump should be happened by panic handler */
			mdelay(2000);
			panic("[@] camera sensor panic!!!");
#else
#if defined(ENABLE_CLOG_RESERVED_MEM)
			is_resource_cdump();
#else
			is_resource_dump();
#endif
#endif
			return;
		}
	}

	mod_timer(&itf->timer, jiffies + (IS_COMMAND_TIMEOUT/TRY_TIMEOUT_COUNT));
}

int is_interface_probe(struct is_interface *this,
	struct is_minfo *minfo,
	ulong regs,
	u32 irq,
	void *core_data)
{
	int ret = 0, work_id;
	struct is_core *core = (struct is_core *)core_data;

	dbg_interface(1, "%s\n", __func__);

	init_request_barrier(this);
	init_process_barrier(this);
	init_waitqueue_head(&this->lock_wait_queue);
	init_waitqueue_head(&this->init_wait_queue);
	init_waitqueue_head(&this->idle_wait_queue);
	spin_lock_init(&this->shot_check_lock);

	this->workqueue = alloc_workqueue("is/[H/U]", WQ_HIGHPRI | WQ_UNBOUND, 0);
	if (!this->workqueue)
		probe_warn("failed to alloc own workqueue, will be use global one");

	INIT_WORK(&this->work_wq[WORK_SHOT_DONE], wq_func_shot);
	INIT_WORK(&this->work_wq[WORK_30C_FDONE], wq_func_30c);
	INIT_WORK(&this->work_wq[WORK_30P_FDONE], wq_func_30p);
	INIT_WORK(&this->work_wq[WORK_30F_FDONE], wq_func_30f);
	INIT_WORK(&this->work_wq[WORK_30G_FDONE], wq_func_30g);
	INIT_WORK(&this->work_wq[WORK_30O_FDONE], wq_func_30o);
	INIT_WORK(&this->work_wq[WORK_30L_FDONE], wq_func_30l);
	INIT_WORK(&this->work_wq[WORK_LME0S_FDONE], wq_func_lme0s);
	INIT_WORK(&this->work_wq[WORK_LME0C_FDONE], wq_func_lme0c);
	INIT_WORK(&this->work_wq[WORK_ORB0C_FDONE], wq_func_orb0c);
	INIT_WORK(&this->work_wq[WORK_I0C_FDONE], wq_func_i0c);
	INIT_WORK(&this->work_wq[WORK_I0P_FDONE], wq_func_i0p);
#if defined(SOC_TNR_MERGER)
	INIT_WORK(&this->work_wq[WORK_I0T_FDONE], wq_func_i0t);
	INIT_WORK(&this->work_wq[WORK_I0G_FDONE], wq_func_i0g);
	INIT_WORK(&this->work_wq[WORK_I0V_FDONE], wq_func_i0v);
	INIT_WORK(&this->work_wq[WORK_I0W_FDONE], wq_func_i0w);
#endif
	INIT_WORK(&this->work_wq[WORK_ME0C_FDONE], wq_func_me0c);

	INIT_WORK(&this->work_wq[WORK_M0P_FDONE], wq_func_m0p);
	INIT_WORK(&this->work_wq[WORK_M1P_FDONE], wq_func_m1p);
	INIT_WORK(&this->work_wq[WORK_M2P_FDONE], wq_func_m2p);
	INIT_WORK(&this->work_wq[WORK_M3P_FDONE], wq_func_m3p);
	INIT_WORK(&this->work_wq[WORK_M4P_FDONE], wq_func_m4p);
	INIT_WORK(&this->work_wq[WORK_M5P_FDONE], wq_func_m5p);

	INIT_WORK(&this->work_wq[WORK_CL0C_FDONE], wq_func_cl0c);

	this->core = (void *)core;
	this->regs = (void *)regs;

	clear_bit(IS_IF_STATE_OPEN, &this->state);
	clear_bit(IS_IF_STATE_START, &this->state);
	clear_bit(IS_IF_STATE_BUSY, &this->state);
	clear_bit(IS_IF_STATE_READY, &this->state);
	clear_bit(IS_IF_STATE_LOGGING, &this->state);

	for (work_id = WORK_SHOT_DONE; work_id < WORK_MAX_MAP; work_id++)
		init_work_list(&this->work_list[work_id], work_id, MAX_WORK_COUNT);

	this->err_report_vendor = NULL;

	return ret;
}

int is_interface_open(struct is_interface *this)
{
	int i;
	int ret = 0;

	if (test_bit(IS_IF_STATE_OPEN, &this->state)) {
		err("already open");
		ret = -EMFILE;
		goto exit;
	}

	dbg_interface(1, "%s\n", __func__);

	for (i = 0; i < IS_STREAM_COUNT; i++) {
		this->streaming[i] = IS_IF_STREAMING_INIT;
		this->processing[i] = IS_IF_PROCESSING_INIT;
		atomic_set(&this->shot_check[i], 0);
		atomic_set(&this->shot_timeout[i], 0);
	}
	for (i = 0; i < IS_SENSOR_COUNT; i++) {
		atomic_set(&this->sensor_check[i], 0);
		atomic_set(&this->sensor_timeout[i], 0);
	}

	clear_bit(IS_IF_STATE_START, &this->state);
	clear_bit(IS_IF_STATE_BUSY, &this->state);
	clear_bit(IS_IF_STATE_READY, &this->state);
	clear_bit(IS_IF_STATE_LOGGING, &this->state);

	timer_setup(&this->timer, (void (*)(struct timer_list *))interface_timer, 0);
	this->timer.expires = jiffies + (IS_COMMAND_TIMEOUT/TRY_TIMEOUT_COUNT);
	add_timer(&this->timer);

	set_bit(IS_IF_STATE_OPEN, &this->state);

exit:
	return ret;
}

int is_interface_close(struct is_interface *this)
{
	int ret = 0;

	if (!test_bit(IS_IF_STATE_OPEN, &this->state)) {
		err("already close");
		ret = -EMFILE;
		goto exit;
	}

	del_timer_sync(&this->timer);

	dbg_interface(1, "%s\n", __func__);

	clear_bit(IS_IF_STATE_OPEN, &this->state);

exit:
	return ret;
}

void is_interface_reset(struct is_interface *this)
{
	return;
}

int is_hw_logdump(struct is_interface *this)
{
	return 0;
}

int is_hw_regdump(struct is_interface *this)
{
	return 0;
}

int is_hw_memdump(struct is_interface *this,
	ulong start,
	ulong end)
{
	return 0;
}

int is_hw_i2c_lock(struct is_interface *this,
	u32 instance, int i2c_clk, bool lock)
{
	return 0;
}

void is_interface_lock(struct is_interface *this)
{
	return;
}

void is_interface_unlock(struct is_interface *this)
{
	return;
}

int is_hw_g_capability(struct is_interface *this,
	u32 instance, u32 address)
{
	return 0;
}

int is_hw_fault(struct is_interface *this)
{
	return 0;
}
