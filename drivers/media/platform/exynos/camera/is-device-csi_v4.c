/*
 * Samsung Exynos SoC series FIMC-IS driver
 *
 * exynos fimc-is/mipi-csi functions
 *
 * Copyright (c) 2017 Samsung Electronics Co., Ltd
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#include <linux/module.h>
#include <linux/delay.h>
#include <linux/kernel.h>
#include <linux/errno.h>
#include <linux/platform_device.h>
#include <linux/slab.h>
#include <linux/videodev2.h>
#include <linux/videodev2_exynos_camera.h>
#include <linux/io.h>
#include <linux/phy/phy.h>
#include <linux/fs.h>
#include <soc/samsung/exynos-sci_dbg.h>

#include "is-debug.h"
#include "is-config.h"
#include "is-core.h"
#include "is-device-csi.h"
#include "is-device-sensor.h"
#include "is-votfmgr.h"
#include "is-param.h"
#include "is-irq.h"
#include "is-device-camif-dma.h"

static int csi_s_fro(struct is_device_csi *csi)
{
	struct is_sensor_cfg *sensor_cfg;
	struct is_camif_wdma_module *wdma_mod;
	u32 vc, dma_ch;

	/*
	 * If frame id decoder or FRO mode are enabled, start & end of CSIS link is not used.
	 * Instead of CSIS link interrupt, CSIS WDMA interrupt is used.
	 * So, only error interrupt is enable.
	 */
	sensor_cfg = csi->sensor_cfg;
	if (!sensor_cfg) {
		merr("[CSI%d] sensor cfg is null", csi, csi->ch);
		return -EINVAL;
	}

	if (sensor_cfg->ex_mode == EX_DUALFPS_960) {
		csi->f_id_dec = true;
		csi->dma_batch_num = 960 / 60;
	} else if (sensor_cfg->ex_mode == EX_DUALFPS_480) {
		csi->f_id_dec = true;
		csi->dma_batch_num = 480 / 60;
	} else {
		csi->f_id_dec = false;
		csi->dma_batch_num = 1;
	}

	minfo("[CSI%d] fro dma_batch_num %d\n", csi, csi->ch, csi->dma_batch_num);

	atomic_set(&csi->bufring_cnt, 0);
	csi->otf_batch_num = 1; /* Set default value, will be determined by frame->num_buffers */

	dma_ch = csi->wdma_ch;
	wdma_mod = is_camif_wdma_module_get(dma_ch);
	if (!wdma_mod) {
		merr("[CSI%d][DMA%d] wdma_mod is NULL", csi, csi->ch, dma_ch);
		return -ENODEV;
	}

	csi_hw_s_dma_common_frame_id_decoder(wdma_mod->regs, dma_ch, csi->f_id_dec);

	if (csi_hw_get_version(csi->base_reg) < IS_CSIS_VERSION(5, 4, 0, 0)) {
		/*
		 * TODO: Both FRO count and frame id decoder
		 * dosen't have to be controlled
		 * if shadowing scheme is supported
		 * at start interrupt of frame id decoder.
		 */
		vc = CSI_VIRTUAL_CH_0;
		csi_hw_s_fro_count(csi->wdma->regs_ctl,
			csi->dma_batch_num, vc);
	}

	return 0;
}

static int csi_dma_attach(struct is_device_csi *csi)
{
	int ret = 0;
	int vc;
	unsigned long flag;
	struct is_camif_wdma *wdma;
	int wdma_ch_hint;

	spin_lock_irqsave(&csi->dma_seq_slock, flag);

	if (csi->wdma) {
		minfo("[%s][CSI%d] DMA is already attached.", csi, __func__, csi->ch);
		goto err_already_attach;
	}

	/*
	 * If there is wdma_ch_hint, specific WDMA channel will be selected.
	 * Or, WDMA channel will be selected dynamically.
	 */
	if (csi->sensor_cfg)
		wdma_ch_hint = csi->sensor_cfg->wdma_ch_hint;
	else
		wdma_ch_hint = csi->wdma_ch_hint;

	wdma = is_camif_wdma_get(wdma_ch_hint);
	if (!wdma) {
		merr("[CSI%d] is_camif_wdma_get is failed", csi, csi->ch);
		ret = -ENODEV;
		goto err_wdma_get;
	}
	csi->wdma = wdma;
	csi->wdma_ch = wdma->index;

	ret = csi_s_fro(csi);
	if (ret) {
		merr("[CSI%d] csi_s_fro is fail", csi, csi->ch);
		goto err_s_fro;
	}

	for (vc = CSI_VIRTUAL_CH_0; vc < CSI_VIRTUAL_CH_MAX; vc++)
		csi_hw_dma_reset(wdma->regs_vc[vc]);

	spin_unlock_irqrestore(&csi->dma_seq_slock, flag);

	return ret;

err_s_fro:
	csi->wdma = NULL;
	is_camif_wdma_put(wdma);

err_wdma_get:
err_already_attach:
	spin_unlock_irqrestore(&csi->dma_seq_slock, flag);

	return ret;
}

static void csi_dma_detach(struct is_device_csi *csi)
{

	struct is_camif_wdma *wdma = csi->wdma;

	csi->wdma = NULL;
	is_camif_wdma_put(wdma);
}

inline void csi_frame_start_inline(struct is_device_csi *csi)
{
	u32 inc = 1;
	u32 fcount, hw_fcount;
	struct is_device_sensor *sensor;
	u32 hashkey, hashkey_1, hashkey_2;
	u32 index;
	u32 hw_frame_id[2] = {0, 0};
	u64 timestamp;
	u64 timestampboot;

	/* frame start interrupt */
	csi->sw_checker = EXPECT_FRAME_END;

	if (!csi->f_id_dec) {
		hw_fcount = csi_hw_g_fcount(csi->base_reg, CSI_VIRTUAL_CH_0);
		inc = hw_fcount - csi->hw_fcount;
		csi->hw_fcount = hw_fcount;

		if (unlikely(inc != 1)) {
			if (inc > 1) {
				mwarn("[CSI%d] interrupt lost(%d)", csi, csi->ch, inc);
			} else if (inc == 0) {
				mwarn("[CSI%d] hw_fcount(%d) is not incresed",
					csi, csi->ch, hw_fcount);
				inc = 1;
			}
		}

		/* SW FRO: start interrupt have to be called only once per batch number  */
		if (csi->otf_batch_num > 1) {
			if ((hw_fcount % csi->otf_batch_num) != 1)
				return;
		}
	}

	fcount = atomic_add_return(inc, &csi->fcount);

	dbg_isr(1, "[F%d] S\n", csi, fcount);
	atomic_set(&csi->vvalid, 1);

	sensor = v4l2_get_subdev_hostdata(*csi->subdev);
	if (!sensor) {
		err("sensor is NULL");
		BUG();
	}

	/*
	 * Sometimes, frame->fcount is bigger than sensor->fcount if gtask is delayed.
	 * hashkey_1~2 is to prenvet reverse timestamp.
	 */
	sensor->fcount = fcount;
	hashkey = fcount % IS_TIMESTAMP_HASH_KEY;
	hashkey_1 = (fcount + 1) % IS_TIMESTAMP_HASH_KEY;
	hashkey_2 = (fcount + 2) % IS_TIMESTAMP_HASH_KEY;

	timestamp = is_get_timestamp();
	timestampboot = is_get_timestamp_boot();
	sensor->timestamp[hashkey] = timestamp;
	sensor->timestamp[hashkey_1] = timestamp;
	sensor->timestamp[hashkey_2] = timestamp;
	sensor->timestampboot[hashkey] = timestampboot;
	sensor->timestampboot[hashkey_1] = timestampboot;
	sensor->timestampboot[hashkey_2] = timestampboot;

	v4l2_subdev_notify(*csi->subdev, CSI_NOTIFY_VSYNC, &fcount);

	tasklet_schedule(&csi->tasklet_csis_str);

	index = fcount % DEBUG_FRAME_COUNT;
	csi->debug_info[index].fcount = fcount;
	csi->debug_info[index].instance = csi->instance;
	csi->debug_info[index].cpuid[DEBUG_POINT_FRAME_START] = raw_smp_processor_id();
	csi->debug_info[index].time[DEBUG_POINT_FRAME_START] = cpu_clock(raw_smp_processor_id());

	if (csi->f_id_dec) {
		struct is_camif_wdma_module *wdma_mod;

		wdma_mod = is_camif_wdma_module_get(csi->wdma_ch);
		if (!wdma_mod) {
			merr("[CSI%d] wdma_mod is NULL", csi, csi->ch);
			return;
		}

		/* after increase fcount */
		csi_hw_g_dma_common_frame_id(wdma_mod->regs, csi->dma_batch_num, hw_frame_id);
		sensor->frame_id[hashkey] = ((u64)hw_frame_id[1] << 32) | (u64)hw_frame_id[0];
	}
}

static inline void csi_frame_line_inline(struct is_device_csi *csi)
{
	dbg_isr(1, "[F%d] L\n", csi, atomic_read(&csi->fcount));
	/* frame line interrupt */
	tasklet_schedule(&csi->tasklet_csis_line);
}

static inline void csi_frame_end_inline(struct is_device_csi *csi)
{
	u32 fcount = atomic_read(&csi->fcount);
	u32 index;

	/* frame end interrupt */
	csi->sw_checker = EXPECT_FRAME_START;

	if (!csi->f_id_dec) {
		/* SW FRO: start interrupt have to be called only once per batch number  */
		if (csi->otf_batch_num > 1) {
			if ((csi->hw_fcount % csi->otf_batch_num) != 0)
				return;
		}
	}

	dbg_isr(1, "[F%d] E\n", csi, fcount);

	atomic_set(&csi->vvalid, 0);
	atomic_set(&csi->vblank_count, fcount);
	v4l2_subdev_notify(*csi->subdev, CSI_NOTIFY_VBLANK, &fcount);

	tasklet_schedule(&csi->tasklet_csis_end);

	index = fcount % DEBUG_FRAME_COUNT;
	csi->debug_info[index].cpuid[DEBUG_POINT_FRAME_END] = raw_smp_processor_id();
	csi->debug_info[index].time[DEBUG_POINT_FRAME_END] = cpu_clock(raw_smp_processor_id());
}

static inline void csi_s_frameptr(struct is_device_csi *csi,
		u32 vc, u32 number, bool clear)
{
	struct is_camif_wdma *wdma = csi->wdma;

	if (!wdma) {
		merr("[CSI%d][VC%d] wdma is NULL", csi, csi->ch, vc);
		return;
	}

	csi_hw_s_frameptr(wdma->regs_vc[vc], vc, number, clear);
}

static inline unsigned long csi_s_buf_addr_offset(struct is_device_csi *csi, u32 vc)
{
	struct is_sensor_cfg *sensor_cfg = csi->sensor_cfg;
	u32 width, heigth, byte_per_line;
	unsigned long offset = 0;

	if (vc == CSI_VIRTUAL_CH_0 && sensor_cfg) {
		if (csi_hw_get_version(csi->base_reg) == IS_CSIS_VERSION(5, 4, 0, 0)) {
			/*
			 * PDP constraint:
			 * In case of (image_h : pd_h == 1 : 1),
			 * the height of TRS must be twice of the height of TWS.
			 */
			if (sensor_cfg->votf && sensor_cfg->img_pd_ratio == IS_IMG_PD_RATIO_1_1) {
				width = sensor_cfg->input[vc].width;
				heigth = sensor_cfg->input[vc].height;
				byte_per_line = ALIGN(width * 10 / BITS_PER_BYTE, 16);
				if (IS_ENABLED(USE_IOVA_EXTENSION))
					offset = ALIGN(byte_per_line * heigth, SZ_4K);
				else
					offset = byte_per_line * heigth;
			}
		}

#if defined(SDC_HEADER_GEN)
		if (sensor_cfg->output[vc].extformat == HW_FORMAT_RAW10_SDC) {
			width = sensor_cfg->output[vc].width;
			byte_per_line = ALIGN(width * 10 / BITS_PER_BYTE, 16);
			offset = byte_per_line * 2;
		}
#endif
	}

	return offset;
}

static inline void csi_s_buf_addr(struct is_device_csi *csi, struct is_frame *frame, u32 vc)
{
	int i = 0;
	u32 dvaddr;
	u32 frameptr = 0;
	u32 num_buffers;
	unsigned long flag;
	unsigned long offset;
	struct is_camif_wdma *wdma;

	FIMC_BUG_VOID(!frame);

	wdma = csi->wdma;
	if (!wdma) {
		merr("[CSI%d] wdma is NULL", csi, csi->ch);
		return;
	}

	if (csi->f_id_dec)
		num_buffers = csi->dma_batch_num;
	else
		num_buffers = frame->num_buffers;

	frameptr = num_buffers *
		(atomic_read(&csi->bufring_cnt) % BUF_SWAP_CNT);

	spin_lock_irqsave(&csi->dma_seq_slock, flag);
	do {
		dvaddr = (u32)frame->dvaddr_buffer[i];
		if (!dvaddr) {
			minfo("[CSI%d][VC%d] dvaddr is null\n", csi, csi->ch, vc);
			continue;
		}

		offset = csi_s_buf_addr_offset(csi, vc);

		csi_hw_s_dma_addr(wdma->regs_vc[vc],
				vc, i + frameptr,
				dvaddr + offset);

		mdbg_common(debug_csi, "[%d][CSI%d][VC%d]", " dva(%d:0x%x)\n",
			csi->instance, csi->ch, vc, i + frameptr, dvaddr);
	} while (++i < num_buffers);

	spin_unlock_irqrestore(&csi->dma_seq_slock, flag);
}

static inline void csi_s_output_dma(struct is_device_csi *csi, u32 vc, bool enable)
{
	bool _enable = test_bit(CSIS_DMA_DISABLING, &csi->state) ? false : enable;
	struct is_camif_wdma *wdma = csi->wdma;

	if (!wdma) {
		merr("[CSI%d][VC%d] wdma is NULL", csi, csi->ch, vc);
		return;
	}

	csi_hw_s_output_dma(wdma->regs_vc[vc], vc, _enable);
}

static inline int csi_g_ebuf_ch(struct is_group *group)
{
	int ret = -EINVAL;

	if (group->child->slot == GROUP_SLOT_PAF) {
		ret = group->child->id - GROUP_ID_PAF0;
		if (ret >= MAX_NUM_EBUF_CH)
			ret = -EINVAL;
	}

	return ret;
}

static inline int csi_g_ebuf_rcount(struct is_device_csi *csi)
{
	int ret, i;

	for (i = 0, ret = 0; i < MAX_NUM_EBUF_CH; i ++)
		ret += atomic_read(&csi->ebuf_rcount[i]);

	return ret;
}

static void csi_debug_otf(struct is_group *group, struct is_device_csi *csi)
{
	struct is_core *core;
	struct is_device_sensor *sensor;

	sensor = v4l2_get_subdev_hostdata(*csi->subdev);
	if (!sensor) {
		err("failed to get sensor");
		return;
	}
	core = sensor->private_data;

	is_hardware_debug_otf(&core->hardware, group);
}

static void csi_s_buf_addr_wrap(void *data, unsigned long id, struct is_frame *frame)
{
	struct is_device_csi *csi;
	u32 vc = CSI_ENTRY_TO_CH(id);

	csi = (struct is_device_csi *)data;
	if (!csi) {
		err("failed to get CSI");
		return;
	}

	/* Move frameptr for shadowing N+1 frame DVA update */
	if ((csi->f_id_dec || (csi->otf_batch_num > 1))
			&& vc == CSI_VIRTUAL_CH_0) {
		u32 frameptr = atomic_inc_return(&csi->bufring_cnt) % BUF_SWAP_CNT;

		frameptr *= csi->dma_batch_num;
		csi_s_frameptr(csi, vc, frameptr, false);
	}

	csi_s_buf_addr(csi, frame, vc);
	csi_s_output_dma(csi, vc, true);

}

static const struct votf_ops csi_votf_ops = {
	.s_addr		= csi_s_buf_addr_wrap,
};

static inline void csi_s_config_dma(struct is_device_csi *csi, struct is_vci_config *vci_config)
{
	int ret, vc = 0;
	bool en_votf;
	struct is_subdev *dma_subdev = NULL;
	struct is_queue *queue;
	struct is_frame_cfg framecfg = {0};
	struct is_fmt fmt;
	struct is_camif_wdma *wdma = csi->wdma;
	struct is_camif_wdma_module *wdma_mod = is_camif_wdma_module_get(csi->wdma_ch);

	/*
	 * CSIS WDMA input mux
	 * Recommended sequence: DMA path select --> DMA config
	 */
	if (wdma->regs_mux) {
		u32 mux_val;

		/* FIXME: It should use dt data. */
		mux_val = csi->ch;

		writel(mux_val, wdma->regs_mux);
		minfo("[CSI%d] input(%d) --> WDMA ch(%d)\n", csi, csi->ch,
			mux_val, csi->wdma_ch);
	}

	if (!wdma_mod) {
		merr("[CSI%d] Failed to get wdma_module", csi, csi->ch);
		return;
	}

	for (vc = CSI_VIRTUAL_CH_0; vc < CSI_VIRTUAL_CH_MAX; vc++) {
		dma_subdev = csi->dma_subdev[vc];

		if (!dma_subdev ||
			!test_bit(IS_SUBDEV_START, &dma_subdev->state))
			continue;

		if (test_bit(IS_SUBDEV_INTERNAL_USE, &dma_subdev->state)) {
			if ((csi->sensor_cfg->output[vc].type == VC_NOTHING) ||
				(csi->sensor_cfg->output[vc].type == VC_PRIVATE))
				continue;

			/* set from internal subdev setting */
			if (csi->sensor_cfg->output[vc].type == VC_TAILPDAF)
				fmt.pixelformat = V4L2_PIX_FMT_SBGGR16;
			else
				fmt.pixelformat = V4L2_PIX_FMT_PRIV_MAGIC;
			fmt.hw_bitwidth = dma_subdev->bits_per_pixel;

			framecfg.format = &fmt;
			framecfg.width = dma_subdev->output.width;
			framecfg.height = dma_subdev->output.height;
		} else {
			/* cpy format from vc video context */
			queue = GET_SUBDEV_QUEUE(dma_subdev);
			if (queue) {
				framecfg = queue->framecfg;
			} else {
				err("vc[%d] subdev queue is NULL!!", vc);
				return;
			}
		}

		if (vci_config[vc].width) {
			framecfg.width = vci_config[vc].width;
			framecfg.height = vci_config[vc].height;
		}

		spin_lock(&wdma_mod->slock);
		en_votf = test_bit(IS_SUBDEV_VOTF_USE, &dma_subdev->state);
		ret = csi_hw_s_dma_common_votf_cfg(wdma_mod->regs,
				framecfg.width,
				csi->wdma_ch,
				vc,
				en_votf);
		spin_unlock(&wdma_mod->slock);

		csi_hw_s_config_dma(wdma->regs_vc[vc],
				vc, &framecfg,
				vci_config[vc].extformat,
				csi->sensor_cfg->dummy_pixel[vc]);

		minfo("[CSI%d][VC%d] VOTF %s, DMA format(%x) (size: %d, %d)\n",
				csi, csi->ch, vc,
				(!ret && en_votf) ? "on" : "off",
				vci_config[vc].extformat,
				framecfg.width, framecfg.height);

#if defined(CSIWDMA_VC1_FORMAT_WA)
		if (!csi->potf) {
			if (vc == CSI_VIRTUAL_CH_0) {
				/* vc1 is needed dma data format setting such as vc0 */
				csi_hw_s_config_dma(wdma->regs_vc[CSI_VIRTUAL_CH_1],
					CSI_VIRTUAL_CH_1, &framecfg,
					vci_config[vc].extformat,
					csi->sensor_cfg->dummy_pixel[vc]);
				minfo("[CSI%d][VC1] DMA format(%x) (size: %d, %d)\n",
					csi, csi->ch,
					vci_config[vc].extformat,
					framecfg.width, framecfg.height);
			} else if ((vc == CSI_VIRTUAL_CH_1) &&
				(vci_config[vc].extformat != vci_config[CSI_VIRTUAL_CH_0].extformat)) {
				err("vc[%d] DMA format should be the same as VC0 (%x, %x)", vc,
					vci_config[CSI_VIRTUAL_CH_0].extformat,
					vci_config[CSI_VIRTUAL_CH_1].extformat);
			}
		}
#endif
		/* vc: determine for vc0 img format for otf path
		 * actual_vc is not used anymore
		 */
		csi_hw_s_config_dma_cmn(wdma->regs_ctl,
				vc, vc, vci_config[vc].hwformat, csi->potf);
	}
}

static inline void csi_s_multibuf_addr(struct is_device_csi *csi, struct is_frame *frame, u32 index, u32 vc)
{
	FIMC_BUG_VOID(!frame);

	csi_hw_s_multibuf_dma_addr(csi->wdma->regs_vc[vc], vc, index,
				(u32)frame->dvaddr_buffer[0]);
}

static struct is_framemgr *csis_get_vc_framemgr(struct is_device_csi *csi, u32 vc)
{
	struct is_subdev *dma_subdev;
	struct is_framemgr *framemgr = NULL;

	if (vc >= CSI_VIRTUAL_CH_MAX) {
		err("VC(%d of %d) is out-of-range", vc, CSI_VIRTUAL_CH_MAX);
		return NULL;
	}

	dma_subdev = csi->dma_subdev[vc];
	if (!dma_subdev ||
			!test_bit(IS_SUBDEV_START, &dma_subdev->state)) {
		return NULL;
	}

	framemgr = GET_SUBDEV_FRAMEMGR(dma_subdev);

	return framemgr;
}

static void csis_s_vc_dma_multibuf(struct is_device_csi *csi)
{
	u32 vc;
	int i;
	struct is_subdev *dma_subdev;
	struct is_framemgr *framemgr;
	struct is_frame *frame;
	unsigned long flags;

	/* dma setting for several virtual ch 1 ~ 3 */
	for (vc = CSI_VIRTUAL_CH_0; vc < CSI_VIRTUAL_CH_MAX; vc++) {
		dma_subdev = csi->dma_subdev[vc];
		if (!dma_subdev
			|| (!test_bit(IS_SUBDEV_INTERNAL_USE, &dma_subdev->state))
			|| (!test_bit(IS_SUBDEV_START, &dma_subdev->state))
			|| (csi->sensor_cfg->output[vc].type == VC_NOTHING)
			|| (csi->sensor_cfg->output[vc].type == VC_PRIVATE))
			continue;

		framemgr = GET_SUBDEV_I_FRAMEMGR(dma_subdev);

		FIMC_BUG_VOID(!framemgr);

		if (test_bit((CSIS_SET_MULTIBUF_VC0 + vc), &csi->state))
			continue;

		framemgr_e_barrier_irqs(framemgr, 0, flags);
		for (i = 0; i < framemgr->num_frames; i++) {
			frame = &framemgr->frames[i];
			csi_s_multibuf_addr(csi, frame, i, vc);
			csi_s_output_dma(csi, vc, true);
			trans_frame(framemgr, frame, FS_FREE);
		}

		framemgr_x_barrier_irqr(framemgr, 0, flags);

		set_bit((CSIS_SET_MULTIBUF_VC0 + vc), &csi->state);
	}
}

static void csis_check_vc_dma_buf(struct is_device_csi *csi)
{
	u32 vc;
	struct is_framemgr *framemgr;
	struct is_frame *frame;
	struct is_subdev *dma_subdev;
	unsigned long flags;

	/* default disable dma setting for several virtual ch 0 ~ 3 */
	for (vc = CSI_VIRTUAL_CH_0; vc < CSI_VIRTUAL_CH_MAX; vc++) {
		dma_subdev = csi->dma_subdev[vc];

		/* skip for internal vc use of not opened vc */
		if (!dma_subdev ||
			!test_bit(IS_SUBDEV_OPEN, &dma_subdev->state) ||
			test_bit(IS_SUBDEV_INTERNAL_USE, &dma_subdev->state))
			continue;

		framemgr = GET_SUBDEV_FRAMEMGR(dma_subdev);

		if (likely(framemgr)) {
			framemgr_e_barrier_irqs(framemgr, 0, flags);

			/* process to NDONE if set to bad frame */
			if (framemgr->queued_count[FS_PROCESS]) {
				frame = peek_frame(framemgr, FS_PROCESS);

				if (frame && frame->result) {
					mserr("[F%d] NDONE(%d, E%X)\n", dma_subdev, dma_subdev,
						frame->fcount, frame->index, frame->result);
					trans_frame(framemgr, frame, FS_COMPLETE);
					CALL_VOPS(dma_subdev->vctx, done, frame->index, VB2_BUF_STATE_ERROR);
				}
			}

			if (framemgr->queued_count[FS_PROCESS] &&
				!csi->pre_dma_enable[vc] && !csi->cur_dma_enable[vc]) {
				minfo("[CSI%d][VC%d] wq_csis_dma is being delayed. [P(%d)]\n", csi, csi->ch, vc,
					framemgr->queued_count[FS_PROCESS]);
				print_frame_queue(framemgr, FS_PROCESS);
			}

			/* print infomation DMA on/off */
			if (test_bit(CSIS_START_STREAM, &csi->state) &&
				csi->pre_dma_enable[vc] != csi->cur_dma_enable[vc]) {
				minfo("[CSI%d][VC%d][F%d] DMA %s [%d/%d/%d]\n", csi, csi->ch, vc,
						atomic_read(&csi->fcount),
						(csi->cur_dma_enable[vc] ? "on" : "off"),
						framemgr->queued_count[FS_REQUEST],
						framemgr->queued_count[FS_PROCESS],
						framemgr->queued_count[FS_COMPLETE]);

				csi->pre_dma_enable[vc] = csi->cur_dma_enable[vc];
			}

			/* after update pre_dma_disable, clear dma enable state */
			csi->cur_dma_enable[vc] = 0;

			framemgr_x_barrier_irqr(framemgr, 0, flags);
		} else {
			merr("[VC%d] framemgr is NULL", csi, vc);
		}

	}
}

static void csis_flush_vc_buf_done(struct is_device_csi *csi, u32 vc,
		enum is_frame_state target,
		enum vb2_buffer_state state)
{
	struct is_device_sensor *device;
	struct is_subdev *dma_subdev;
	struct is_framemgr *ldr_framemgr;
	struct is_framemgr *framemgr;
	struct is_frame *ldr_frame;
	struct is_frame *frame;
	struct is_video_ctx *vctx;
	u32 findex;
	unsigned long flags;

	device = container_of(csi->subdev, struct is_device_sensor, subdev_csi);

	FIMC_BUG_VOID(!device);

	/* buffer done for several virtual ch 0 ~ 3, internal vc is skipped */
	dma_subdev = csi->dma_subdev[vc];
	if (!dma_subdev
		|| test_bit(IS_SUBDEV_INTERNAL_USE, &dma_subdev->state)
		|| !test_bit(IS_SUBDEV_START, &dma_subdev->state))
		return;

	ldr_framemgr = GET_SUBDEV_FRAMEMGR(dma_subdev->leader);
	framemgr = GET_SUBDEV_FRAMEMGR(dma_subdev);
	vctx = dma_subdev->vctx;

	FIMC_BUG_VOID(!ldr_framemgr);
	FIMC_BUG_VOID(!framemgr);

	framemgr_e_barrier_irqs(framemgr, 0, flags);

	frame = peek_frame(framemgr, target);
	while (frame) {
		if (target == FS_PROCESS) {
			findex = frame->stream->findex;
			ldr_frame = &ldr_framemgr->frames[findex];
			clear_bit(dma_subdev->id, &ldr_frame->out_flag);
		}

		CALL_VOPS(vctx, done, frame->index, state);
		trans_frame(framemgr, frame, FS_COMPLETE);
		frame = peek_frame(framemgr, target);
	}

	framemgr_x_barrier_irqr(framemgr, 0, flags);
}

static void csis_flush_vc_multibuf(struct is_device_csi *csi, u32 vc)
{
	int i;
	struct is_subdev *subdev;
	struct is_framemgr *framemgr;
	struct is_frame *frame;
	unsigned long flags;

	subdev = csi->dma_subdev[vc];

	if (!subdev
		|| !test_bit(IS_SUBDEV_START, &subdev->state)
		|| !test_bit(IS_SUBDEV_INTERNAL_USE, &subdev->state)
		|| (csi->sensor_cfg->output[vc].type == VC_NOTHING)
		|| (csi->sensor_cfg->output[vc].type == VC_PRIVATE))
		return;

	framemgr = GET_SUBDEV_I_FRAMEMGR(subdev);
	if (framemgr) {
		framemgr_e_barrier_irqs(framemgr, 0, flags);
		for (i = 0; i < framemgr->num_frames; i++) {
			frame = &framemgr->frames[i];

			if (frame->state == FS_PROCESS
				|| frame->state == FS_COMPLETE) {
				trans_frame(framemgr, frame, FS_FREE);
			}
		}
		framemgr_x_barrier_irqr(framemgr, 0, flags);
	}

	clear_bit((CSIS_SET_MULTIBUF_VC1 + (vc - 1)), &csi->state);
}

static void csis_flush_all_vc_buf_done(struct is_device_csi *csi, u32 state)
{
	u32 i;

	/* buffer done for several virtual ch 0 ~ 3 */
	for (i = CSI_VIRTUAL_CH_0; i < CSI_VIRTUAL_CH_MAX; i++) {
		csis_flush_vc_buf_done(csi, i, FS_PROCESS, state);
		csis_flush_vc_buf_done(csi, i, FS_REQUEST, state);

		csis_flush_vc_multibuf(csi, i);
	}
}

/*
 * Tasklet for handling of start/end interrupt bottom-half
 *
 * tasklet_csis_str_otf : In case of OTF mode (csi->)
 * tasklet_csis_str_m2m : In case of M2M mode (csi->)
 * tasklet_csis_end     : In case of OTF or M2M mode (csi->)
 */
static u32 g_print_cnt;
void tasklet_csis_str_otf(unsigned long data)
{
	struct v4l2_subdev *subdev;
	struct is_device_csi *csi;
	struct is_device_sensor *device;
	struct is_device_ischain *ischain;
	struct is_groupmgr *groupmgr;
	struct is_group *group_sensor, *group_3aa, *group_isp;
	u32 fcount, group_sensor_id, group_3aa_id, group_isp_id;
	u32 backup_fcount;

	subdev = (struct v4l2_subdev *)data;
	csi = v4l2_get_subdevdata(subdev);
	if (!csi) {
		err("csi is NULL");
		BUG();
	}

	device = v4l2_get_subdev_hostdata(subdev);
	if (!device) {
		err("device is NULL");
		BUG();
	}

	fcount = atomic_read(&csi->fcount);
	ischain = device->ischain;

#ifdef TASKLET_MSG
	is_info("S%d\n", fcount);
#endif

	groupmgr = ischain->groupmgr;
	group_sensor = &device->group_sensor;
	group_3aa = &ischain->group_3aa;
	group_isp = &ischain->group_isp;

	group_sensor_id = group_sensor->id;
	group_3aa_id = group_3aa->id;
	group_isp_id = group_isp->id;

	if (!test_bit(IS_SENSOR_STAND_ALONE, &device->state)) {
		if (group_3aa_id >= GROUP_ID_MAX) {
			merr("group 3aa id is invalid(%d)", csi, group_3aa_id);
			goto trigger_skip;
		}

		if (group_isp_id >= GROUP_ID_MAX) {
			merr("group isp id is invalid(%d)", csi, group_isp_id);
			goto trigger_skip;
		}
	}

	if (unlikely(list_empty(&group_sensor->smp_trigger.wait_list))) {
		atomic_set(&group_sensor->sensor_fcount, fcount + group_sensor->skip_shots);

		/*
		 * pcount : program count
		 * current program count(location) in kthread
		 */
		if (((g_print_cnt % LOG_INTERVAL_OF_DROPS) == 0) ||
			(g_print_cnt < LOG_INTERVAL_OF_DROPS)) {
			if (!test_bit(IS_SENSOR_STAND_ALONE, &device->state)) {
				minfo("[CSI%d] GP%d(res %d, rcnt %d, scnt %d), "
						KERN_CONT "GP%d(res %d, rcnt %d, scnt %d), "
						KERN_CONT "fcount %d pcount %d\n",
						csi, csi->ch, group_sensor_id,
						groupmgr->gtask[group_sensor_id].smp_resource.count,
						atomic_read(&group_sensor->rcount),
						atomic_read(&group_sensor->scount),
						group_isp_id,
						groupmgr->gtask[group_isp_id].smp_resource.count,
						atomic_read(&group_isp->rcount),
						atomic_read(&group_isp->scount),
						fcount + group_sensor->skip_shots,
						group_sensor->pcount);
			} else {
				minfo("[CSI%d] GP%d(res %d, rcnt %d, scnt %d), "
						KERN_CONT "fcount %d pcount %d\n",
						csi, csi->ch, group_sensor_id,
						groupmgr->gtask[group_sensor_id].smp_resource.count,
						atomic_read(&group_sensor->rcount),
						atomic_read(&group_sensor->scount),
						fcount + group_sensor->skip_shots,
						group_sensor->pcount);
			}
		}
		g_print_cnt++;
	} else {
		backup_fcount = atomic_read(&group_sensor->backup_fcount);
		g_print_cnt = 0;
		if (fcount + group_sensor->skip_shots > backup_fcount) {
			atomic_set(&group_sensor->sensor_fcount, fcount + group_sensor->skip_shots);
			dbg("%s: [F:%d] up(smp_trigger) - [backup_F:%d]\n", __func__, fcount,
				backup_fcount);
			up(&group_sensor->smp_trigger);
		}
	}

trigger_skip:
	/* check all virtual channel's dma */
	csis_check_vc_dma_buf(csi);
	/* re-set internal vc dma if flushed */
	csis_s_vc_dma_multibuf(csi);

	v4l2_subdev_notify(subdev, CSIS_NOTIFY_FSTART, &fcount);
	clear_bit(IS_SENSOR_WAIT_STREAMING, &device->state);
}

void tasklet_csis_str_m2m(unsigned long data)
{
	struct v4l2_subdev *subdev;
	struct is_device_csi *csi;
	u32 fcount;

	subdev = (struct v4l2_subdev *)data;
	csi = v4l2_get_subdevdata(subdev);
	if (!csi) {
		err("[CSI] csi is NULL");
		BUG();
	}

	fcount = atomic_read(&csi->fcount);

#ifdef TASKLET_MSG
	is_info("S%d\n", fcount);
#endif
	/* check all virtual channel's dma */
	csis_check_vc_dma_buf(csi);
	/* re-set internal vc dma if flushed */
	csis_s_vc_dma_multibuf(csi);

	v4l2_subdev_notify(subdev, CSIS_NOTIFY_FSTART, &fcount);
}

static void csi_dma_tag(struct v4l2_subdev *subdev,
	struct is_device_csi *csi,
	struct is_framemgr *framemgr, u32 vc)
{
	u32 findex;
	u32 done_state = 0;
	unsigned long flags;
	unsigned int data_type;
	struct is_subdev *f_subdev;
	struct is_framemgr *ldr_framemgr;
	struct is_video_ctx *vctx = NULL;
	struct is_frame *ldr_frame;
	struct is_frame *frame = NULL;
	struct is_subdev *dma_subdev = csi->dma_subdev[vc];
	struct is_camif_wdma *wdma = csi->wdma;

	if (!dma_subdev) {
		merr("[VC%d] could not get DMA sub-device", csi, vc);
		return;
	}

	if (!test_bit(IS_SUBDEV_INTERNAL_USE, &dma_subdev->state)) {
		framemgr_e_barrier_irqs(framemgr, 0, flags);

		frame = peek_frame(framemgr, FS_PROCESS);
		if (frame) {
			trans_frame(framemgr, frame, FS_COMPLETE);

			/* get subdev and video context */
			f_subdev = frame->subdev;
			WARN_ON(!f_subdev);

			vctx = f_subdev->vctx;
			WARN_ON(!vctx);

			/* get the leader's framemgr */
			ldr_framemgr = GET_SUBDEV_FRAMEMGR(f_subdev->leader);
			WARN_ON(!ldr_framemgr);

			findex = frame->stream->findex;
			ldr_frame = &ldr_framemgr->frames[findex];
			clear_bit(f_subdev->id, &ldr_frame->out_flag);
			done_state = (frame->result) ?
				VB2_BUF_STATE_ERROR : VB2_BUF_STATE_DONE;

			if (frame->result)
				msrinfo("[CSI%d][ERR] NDONE(%d, E%X)\n", f_subdev, f_subdev, ldr_frame,
					csi->ch, frame->index, frame->result);
			else
				msrdbgs(1, "[CSI%d] DONE(%d)\n", f_subdev, f_subdev, ldr_frame,
					csi->ch, frame->index);

			CALL_VOPS(vctx, done, frame->index, done_state);
		}

		framemgr_x_barrier_irqr(framemgr, 0, flags);

		data_type = CSIS_NOTIFY_DMA_END;
	} else {
		/* get internal VC buffer for embedded data */
		if ((csi->sensor_cfg->output[vc].type == VC_EMBEDDED) ||
			(csi->sensor_cfg->output[vc].type == VC_EMBEDDED2)) {
			u32 frameptr = csi_hw_g_frameptr(wdma->regs_vc[vc], vc);

			if (frameptr < framemgr->num_frames) {
				frame = &framemgr->frames[frameptr];

				/* cache invalidate */
				CALL_BUFOP(dma_subdev->pb_subdev[frame->index], sync_for_cpu,
					dma_subdev->pb_subdev[frame->index],
					0,
					dma_subdev->pb_subdev[frame->index]->size,
					DMA_FROM_DEVICE);
					mdbgd_front("%s, %s[%d] = %d\n", csi, __func__, "VC_EMBEDDED",
								frameptr, frame->fcount);
			}

			data_type = CSIS_NOTIFY_DMA_END_VC_EMBEDDED;
		} else if (csi->sensor_cfg->output[vc].type == VC_MIPISTAT) {
			u32 frameptr = csi_hw_g_frameptr(wdma->regs_vc[vc], vc);

			frameptr = frameptr % framemgr->num_frames;
			frame = &framemgr->frames[frameptr];

			data_type = CSIS_NOTIFY_DMA_END_VC_MIPISTAT;

			mdbgd_front("%s, %s[%d] = %d\n", csi, __func__, "VC_MIPISTAT",
							frameptr, frame->fcount);
		} else if (csi->sensor_cfg->output[vc].type == VC_TAILPDAF) {
			u32 frameptr = csi_hw_g_frameptr(wdma->regs_vc[vc], vc);

			frameptr = frameptr % framemgr->num_frames;
			frame = &framemgr->frames[frameptr];

			mdbgd_front("%s, %s[%d] = %d\n", csi, __func__, "VC_TAILPDAF",
							frameptr, frame->fcount);

			return;
		} else {
			return;
		}
	}

	v4l2_subdev_notify(subdev, data_type, frame);
}

static void csi_err_check(struct is_device_csi *csi, ulong *err_id, enum csis_hw_type type)
{
	int vc, err, votf_ch = 0;
	unsigned long prev_err_flag = 0;
	struct is_subdev *dma_subdev;
	struct is_device_sensor *sensor;
	struct is_camif_wdma *wdma = csi->wdma;

	/* 1. Check error */
	for (vc = CSI_VIRTUAL_CH_0; vc < CSI_VIRTUAL_CH_MAX; vc++) {
		prev_err_flag |= csi->error_id[vc];

		dma_subdev = csi->dma_subdev[vc];
		votf_ch |= test_bit(IS_SUBDEV_VOTF_USE, &dma_subdev->state) << vc;
	}

	/* 2. If err occurs first in 1 frame, request DMA abort */
	if (!prev_err_flag) {
		if (wdma)
			csi_hw_s_control(wdma->regs_ctl, CSIS_CTRL_DMA_ABORT_REQ, true);
		else
			mwarn("[CSI%d] wdma is NULL", csi, csi->ch);
	}

	/* 3. Cumulative error */
	for (vc = CSI_VIRTUAL_CH_0; vc < CSI_VIRTUAL_CH_MAX; vc++)
		csi->error_id[vc] |= err_id[vc];

	/* 4. VC ch0 only exception case */
	err = find_first_bit(&err_id[CSI_VIRTUAL_CH_0], CSIS_ERR_END);
	while (err < CSIS_ERR_END) {
		switch (err) {
		case CSIS_ERR_LOST_FE_VC:
			/* 1. disable next dma */
			csi_s_output_dma(csi, CSI_VIRTUAL_CH_0, false);
			/* 2. schedule the end tasklet */
			csi_frame_end_inline(csi);
			/* 3. increase the sensor fcount */
			/* 4. schedule the start tasklet */
			csi_frame_start_inline(csi);
			break;
		case CSIS_ERR_LOST_FS_VC:
			/* disable next dma */
			csi_s_output_dma(csi, CSI_VIRTUAL_CH_0, false);
			break;
		case CSIS_ERR_CRC:
		case CSIS_ERR_MAL_CRC:
		case CSIS_ERR_CRC_CPHY:
			csi->crc_flag = true;

			merr("[CSI%d][DMA%d][VC%d][F%d] CSIS_ERR_CRC_XXX (ID %d)", csi,
				csi->ch,
				csi->wdma_ch,
				CSI_VIRTUAL_CH_0, atomic_read(&csi->fcount), err);

			if (!test_bit(CSIS_ERR_CRC, &prev_err_flag)
				&& !test_bit(CSIS_ERR_MAL_CRC, &prev_err_flag)
				&& !test_bit(CSIS_ERR_CRC_CPHY, &prev_err_flag)) {
				sensor = v4l2_get_subdev_hostdata(*csi->subdev);
				if (sensor)
					is_sensor_dump(sensor);
			}
			break;
		default:
			break;
		}

		/* Check next bit */
		err = find_next_bit(&err_id[CSI_VIRTUAL_CH_0], CSIS_ERR_END, err + 1);
	}
}

static void csi_err_print(struct is_device_csi *csi)
{
	const char *err_str = NULL;
	int vc, err;
#ifdef USE_CAMERA_HW_BIG_DATA
	bool err_report = false;
#endif
#if IS_ENABLED(CONFIG_SEC_ABC)
	bool abc_err_report = false;
#endif

	for (vc = CSI_VIRTUAL_CH_0; vc < CSI_VIRTUAL_CH_MAX; vc++) {
		/* Skip error handling if there's no error in this virtual ch. */
		if (!csi->error_id[vc])
			continue;

		err = find_first_bit(&csi->error_id[vc], CSIS_ERR_END);
		while (err < CSIS_ERR_END) {
			switch (err) {
			case CSIS_ERR_ID:
				err_str = GET_STR(CSIS_ERR_ID);
				break;
			case CSIS_ERR_CRC:
				err_str = GET_STR(CSIS_ERR_CRC);
				break;
			case CSIS_ERR_ECC:
				err_str = GET_STR(CSIS_ERR_ECC);
				break;
			case CSIS_ERR_WRONG_CFG:
				err_str = GET_STR(CSIS_ERR_WRONG_CFG);
				break;
			case CSIS_ERR_OVERFLOW_VC:
				is_debug_event_count(IS_EVENT_OVERFLOW_CSI);
				err_str = GET_STR(CSIS_ERR_OVERFLOW_VC);
#if 0 //def OVERFLOW_PANIC_ENABLE_CSIS
				is_debug_s2d(false, "CSIS error!! %s", err_str);
#endif
				break;
			case CSIS_ERR_LOST_FE_VC:
				err_str = GET_STR(CSIS_ERR_LOST_FE_VC);
				break;
			case CSIS_ERR_LOST_FS_VC:
				err_str = GET_STR(CSIS_ERR_LOST_FS_VC);
				break;
			case CSIS_ERR_SOT_VC:
				err_str = GET_STR(CSIS_ERR_SOT_VC);
				break;
			case CSIS_ERR_DMA_OTF_OVERLAP_VC:
				err_str = GET_STR(CSIS_ERR_DMA_OTF_OVERLAP_VC);
				break;
			case CSIS_ERR_DMA_DMAFIFO_FULL:
				is_debug_event_count(IS_EVENT_OVERFLOW_CSI);
				err_str = GET_STR(CSIS_ERR_DMA_DMAFIFO_FULL);
#if 0 //defined(OVERFLOW_PANIC_ENABLE_CSIS)
#ifdef USE_CAMERA_HW_BIG_DATA
				is_vender_csi_err_handler(csi);
#ifdef CAMERA_HW_BIG_DATA_FILE_IO
				is_sec_copy_err_cnt_to_file();
#endif
#endif
#if IS_ENABLED(CONFIG_EXYNOS_SCI_DBG_AUTO)
				smc_ppc_enable(0);
#endif

				is_debug_s2d(true, "[DMA%d][VC%d] CSIS error!! %s",
					csi->wdma_ch,
					vc,
					err_str);
#endif
				break;
			case CSIS_ERR_INVALID_CODE_HS:
				err_str = GET_STR(CSIS_ERR_INVALID_CODE_HS);
				break;
			case CSIS_ERR_SOT_SYNC_HS:
				err_str = GET_STR(CSIS_ERR_SOT_SYNC_HS);
				break;
			case CSIS_ERR_MAL_CRC:
				err_str = GET_STR(CSIS_ERR_MAL_CRC);
				break;
			case CSIS_ERR_DMA_ABORT_DONE:
				err_str = GET_STR(CSIS_ERR_DMA_ABORT_DONE);
				break;
			case CSIS_ERR_VRESOL_MISMATCH:
				err_str = GET_STR(CSIS_ERR_VRESOL_MISMATCH);
				break;
			case CSIS_ERR_HRESOL_MISMATCH:
				err_str = GET_STR(CSIS_ERR_HRESOL_MISMATCH);
				break;
			case CSIS_ERR_DMA_FRAME_DROP_VC:
				err_str = GET_STR(CSIS_ERR_DMA_FRAME_DROP_VC);
				break;
			case CSIS_ERR_CRC_CPHY:
				err_str = GET_STR(CSIS_ERR_CRC_CPHY);
				break;
			case CSIS_ERR_DMA_LASTDATA_ERROR:
				err_str = GET_STR(CSIS_ERR_DMA_LASTDATA_ERROR);
				break;
			case CSIS_ERR_DMA_LASTADDR_ERROR:
				err_str = GET_STR(CSIS_ERR_DMA_LASTADDR_ERROR);
				break;
			case CSIS_ERR_DMA_FSTART_IN_FLUSH_VC:
				err_str = GET_STR(CSIS_ERR_DMA_FSTART_IN_FLUSH_VC);
				break;
			case CSIS_ERR_DMA_C2COM_LOST_FLUSH_VC:
				err_str = GET_STR(CSIS_ERR_DMA_C2COM_LOST_FLUSH_VC);
				break;
			}

			/* Print error log */
			merr("[CSI%d][DMA%d][VC%d][F%d] Occurred the %s(ID %d)", csi,
				csi->ch, csi->wdma_ch,
				vc,
				atomic_read(&csi->fcount), err_str, err);

#ifdef USE_CAMERA_HW_BIG_DATA
			/* temporarily hwparam count except CSIS_ERR_DMA_ABORT_DONE */
			if ((err != CSIS_ERR_DMA_ABORT_DONE) && err >= CSIS_ERR_ID  && err < CSIS_ERR_END)
				err_report = true;
#endif
#if IS_ENABLED(CONFIG_SEC_ABC)
			if (err >= CSIS_ERR_ID  && err < CSIS_ERR_END)
				abc_err_report = true;
#endif

			/* Check next bit */
			err = find_next_bit(&csi->error_id[vc], CSIS_ERR_END, err + 1);
		}
	}

#ifdef USE_CAMERA_HW_BIG_DATA
	if (err_report)
		is_vender_csi_err_handler(csi);
#endif
#if IS_ENABLED(CONFIG_SEC_ABC)
	if (abc_err_report)
		sec_abc_send_event("MODULE=camera@INFO=camera_error");
#endif
}

void csi_hw_cdump_all(struct is_device_csi *csi)
{
	int vc;
	struct is_camif_wdma *wdma = csi->wdma;
	struct is_camif_wdma_module *wdma_mod = is_camif_wdma_module_get(csi->wdma_ch);

	csi_hw_cdump(csi->base_reg);
	csi_hw_phy_cdump(csi->phy_reg, csi->ch);
	if (wdma) {
		for (vc = CSI_VIRTUAL_CH_0; vc < CSI_VIRTUAL_CH_MAX; vc++)
			csi_hw_vcdma_cdump(wdma->regs_vc[vc]);
		csi_hw_vcdma_cmn_cdump(wdma->regs_ctl);
	}

	if (wdma_mod)
		csi_hw_common_dma_cdump(wdma_mod->regs);
}

void csi_hw_dump_all(struct is_device_csi *csi)
{
	int vc;
	struct is_camif_wdma *wdma = csi->wdma;
	struct is_camif_wdma_module *wdma_mod = is_camif_wdma_module_get(csi->wdma_ch);

	csi_hw_dump(csi->base_reg);
	csi_hw_phy_dump(csi->phy_reg, csi->ch);
	if (wdma) {
		for (vc = CSI_VIRTUAL_CH_0; vc < CSI_VIRTUAL_CH_MAX; vc++)
			csi_hw_vcdma_dump(wdma->regs_vc[vc]);
		csi_hw_vcdma_cmn_dump(wdma->regs_ctl);
	}

	if (wdma_mod)
		csi_hw_common_dma_dump(wdma_mod->regs);
}

static void csi_err_handle_ext(struct is_device_csi *csi, u32 error_id_all)
{
	struct is_device_sensor *device =
		container_of(csi->subdev, struct is_device_sensor, subdev_csi);
	struct is_group *group = &device->group_sensor;

	if (csi_hw_get_version(csi->base_reg) == IS_CSIS_VERSION(5, 4, 0, 0)) {
		/* get s2d dump in only CSIS_ERR_DMA_OTF_OVERLAP_VC case for ESD recovery */
		if (error_id_all == (1 << CSIS_ERR_DMA_OTF_OVERLAP_VC)) {
			csi->error_count_vc_overlap++;
			minfo("[CSI%d] error_count_vc_overlap = %d",
				csi, csi->ch, csi->error_count_vc_overlap);

			/* get connected ip status for detect reason of dma overlap */
			csi_debug_otf(group, csi);

			if (test_bit(IS_GROUP_VOTF_OUTPUT, &group->state)) {
				is_votf_check_wait_con(group->next);
				is_votf_check_invalid_state(group->next);
			}
		}

		if (csi->error_count_vc_overlap >= CSI_ERR_COUNT - 1)
			is_debug_s2d(true, "CSIS error!! - CSIS_ERR_DMA_OTF_OVERLAP_VC");
	}
}

static void csi_err_handle(struct is_device_csi *csi)
{
	struct is_core *core;
	struct is_device_sensor *device;
	struct is_subdev *dma_subdev;
	struct is_camif_wdma *wdma;
	int vc;
	u32 error_id_all = 0;

	/* 1. Print error & Clear error status */
	csi_err_print(csi);

	for (vc = CSI_VIRTUAL_CH_0; vc < CSI_VIRTUAL_CH_MAX; vc++) {
		/* 2. Clear err */
		error_id_all |= csi->error_id_last[vc] = csi->error_id[vc];
		csi->error_id[vc] = 0;

		/* 3. Frame flush */
		dma_subdev = csi->dma_subdev[vc];
		if (dma_subdev && test_bit(IS_SUBDEV_OPEN, &dma_subdev->state)) {
			csis_flush_vc_buf_done(csi, vc, FS_PROCESS, VB2_BUF_STATE_ERROR);
			csis_flush_vc_multibuf(csi, vc);
			err("[F%d][VC%d] frame was done with error", atomic_read(&csi->fcount), vc);
			set_bit((CSIS_BUF_ERR_VC0 + vc), &csi->state);
		}
	}

	csi_err_handle_ext(csi, error_id_all);

	/* 4. Add error count */
	csi->error_count++;

	/* 5. Call sensor DTP if Err count is more than a certain amount */
	if (csi->error_count >= CSI_ERR_COUNT) {
		device = container_of(csi->subdev, struct is_device_sensor, subdev_csi);
		core = device->private_data;
		wdma = csi->wdma;

#ifdef CONFIG_CAMERA_VENDER_MCD /* for ESD recovery */
		err("Checking ESD : csi->error_count = %d, vblank_count = %d, tasklet_csis_end_count = %d",
			csi->error_count, atomic_read(&csi->vblank_count), csi->tasklet_csis_end_count);
		if (csi->error_count >= csi->tasklet_csis_end_count) {
			set_bit(IS_SENSOR_FRONT_DTP_STOP, &device->state);
		} else {
			set_bit(IS_SENSOR_ESD_RECOVERY, &device->state);
		}
#else
		/* Call sensor DTP */
		set_bit(IS_SENSOR_FRONT_DTP_STOP, &device->state);
#endif

		/* Disable CSIS */
		csi_hw_disable(csi->base_reg);

		if (wdma) {
			for (vc = CSI_VIRTUAL_CH_0; vc < CSI_VIRTUAL_CH_MAX; vc++)
				csi_hw_s_dma_irq_msk(wdma->regs_ctl, false);
		}
		csi_hw_s_irq_msk(csi->base_reg, false, csi->f_id_dec);

		/* CSIS register dump */
		if ((csi->error_count == CSI_ERR_COUNT) || (csi->error_count % 20 == 0)) {
			csi_hw_dump_all(csi);
			/* CLK dump */
			schedule_work(&core->wq_data_print_clk);
		}
	}
}

static void wq_csis_dma_vc0(struct work_struct *data)
{
	struct is_device_csi *csi;
	struct is_work_list *work_list;
	struct is_work *work;
	struct is_framemgr *framemgr;

	FIMC_BUG_VOID(!data);

	csi = container_of(data, struct is_device_csi, wq_csis_dma[CSI_VIRTUAL_CH_0]);
	if (!csi) {
		err("[CSI]csi is NULL");
		BUG();
	}

	work_list = &csi->work_list[CSI_VIRTUAL_CH_0];
	get_req_work(work_list, &work);
	while (work) {
		framemgr = csis_get_vc_framemgr(csi, CSI_VIRTUAL_CH_0);
		if (framemgr)
			csi_dma_tag(*csi->subdev, csi, framemgr, CSI_VIRTUAL_CH_0);

		set_free_work(work_list, work);
		get_req_work(work_list, &work);
	}
}

static void wq_csis_dma_vc1(struct work_struct *data)
{
	struct is_device_csi *csi;
	struct is_work_list *work_list;
	struct is_work *work;
	struct is_framemgr *framemgr;

	FIMC_BUG_VOID(!data);

	csi = container_of(data, struct is_device_csi, wq_csis_dma[CSI_VIRTUAL_CH_1]);
	if (!csi) {
		err("[CSI]csi is NULL");
		BUG();
	}

	work_list = &csi->work_list[CSI_VIRTUAL_CH_1];
	get_req_work(work_list, &work);
	while (work) {
		framemgr = csis_get_vc_framemgr(csi, CSI_VIRTUAL_CH_1);
		if (framemgr)
			csi_dma_tag(*csi->subdev, csi, framemgr, CSI_VIRTUAL_CH_1);

		set_free_work(work_list, work);
		get_req_work(work_list, &work);
	}
}

static void wq_csis_dma_vc2(struct work_struct *data)
{
	struct is_device_csi *csi;
	struct is_work_list *work_list;
	struct is_work *work;
	struct is_framemgr *framemgr;

	FIMC_BUG_VOID(!data);

	csi = container_of(data, struct is_device_csi, wq_csis_dma[CSI_VIRTUAL_CH_2]);
	if (!csi) {
		err("[CSI]csi is NULL");
		BUG();
	}

	work_list = &csi->work_list[CSI_VIRTUAL_CH_2];
	get_req_work(work_list, &work);
	while (work) {
		framemgr = csis_get_vc_framemgr(csi, CSI_VIRTUAL_CH_2);
		if (framemgr)
			csi_dma_tag(*csi->subdev, csi, framemgr, CSI_VIRTUAL_CH_2);

		set_free_work(work_list, work);
		get_req_work(work_list, &work);
	}
}

static void wq_csis_dma_vc3(struct work_struct *data)
{
	struct is_device_csi *csi;
	struct is_work_list *work_list;
	struct is_work *work;
	struct is_framemgr *framemgr = NULL;

	FIMC_BUG_VOID(!data);

	csi = container_of(data, struct is_device_csi, wq_csis_dma[CSI_VIRTUAL_CH_3]);
	if (!csi) {
		err("[CSI]csi is NULL");
		BUG();
	}

	work_list = &csi->work_list[CSI_VIRTUAL_CH_3];
	get_req_work(work_list, &work);
	while (work) {
		framemgr = csis_get_vc_framemgr(csi, CSI_VIRTUAL_CH_3);
		if (framemgr)
			csi_dma_tag(*csi->subdev, csi, framemgr, CSI_VIRTUAL_CH_3);

		set_free_work(work_list, work);
		get_req_work(work_list, &work);
	}
}

static void tasklet_csis_end(unsigned long data)
{
	u32 vc, ch, err_flag = 0;
	u32 status = IS_SHOT_SUCCESS;
	struct is_device_csi *csi;
	struct v4l2_subdev *subdev;

	subdev = (struct v4l2_subdev *)data;
	csi = v4l2_get_subdevdata(subdev);
	if (!csi) {
		err("[CSI] csi is NULL");
		BUG();
	}

#ifdef TASKLET_MSG
	is_info("E%d\n", atomic_read(&csi->fcount));
#endif

	for (ch = CSI_VIRTUAL_CH_0; ch < CSI_VIRTUAL_CH_MAX; ch++)
		err_flag |= csi->error_id[ch];

	csi->tasklet_csis_end_count++;

	if (err_flag) {
		csi_err_handle(csi);
	} else {
		/* If error does not occur continously, the system should error count clear */
		csi->error_count = 0;
		csi->error_count_vc_overlap = 0;
	}

	for (vc = CSI_VIRTUAL_CH_0; vc < CSI_VIRTUAL_CH_MAX; vc++) {
		if (test_bit((CSIS_BUF_ERR_VC0 + vc), &csi->state)) {
			clear_bit((CSIS_BUF_ERR_VC0 + vc), &csi->state);
			status = IS_SHOT_CORRUPTED_FRAME;
		}
	}

	v4l2_subdev_notify(subdev, CSIS_NOTIFY_FEND, &status);
}

static void tasklet_csis_line(unsigned long data)
{
	struct is_device_csi *csi;
	struct v4l2_subdev *subdev;

	subdev = (struct v4l2_subdev *)data;
	csi = v4l2_get_subdevdata(subdev);
	if (!csi) {
		err("[CSI] csi is NULL");
		BUG();
	}

#ifdef TASKLET_MSG
	is_info("L%d\n", atomic_read(&csi->fcount));
#endif
	v4l2_subdev_notify(subdev, CSIS_NOTIFY_LINE, NULL);
}

static inline void csi_wq_func_schedule(struct is_device_csi *csi,
		struct work_struct *work_wq)
{
	if (csi->workqueue)
		queue_work(csi->workqueue, work_wq);
	else
		schedule_work(work_wq);
}

static void do_dma_done_work_func(struct is_device_csi *csi, int vc)
{
	bool retry_flag = true;
	struct work_struct *work_wq;
	struct is_work_list *work_list;
	struct is_work *work;

	work_wq = &csi->wq_csis_dma[vc];
	work_list = &csi->work_list[vc];

retry:
	get_free_work(work_list, &work);
	if (work) {
		work->msg.id = 0;
		work->msg.instance = csi->instance;
		work->msg.param1 = vc;

		work->fcount = atomic_read(&csi->fcount);
		set_req_work(work_list, work);

		if (!work_pending(work_wq))
			csi_wq_func_schedule(csi, work_wq);
	} else {
		merr("[VC%d]free work list is empty. retry(%d)", csi, vc, retry_flag);
		if (retry_flag) {
			retry_flag = false;
			goto retry;
		}
	}
}

static irqreturn_t is_isr_csi(int irq, void *data)
{
	struct is_device_csi *csi;
	int frame_start, frame_end;
	struct csis_irq_src irq_src;
	u32 ch, err_flag = 0;

	csi = data;
	memset(&irq_src, 0x0, sizeof(struct csis_irq_src));
	csi_hw_g_irq_src(csi->base_reg, &irq_src, true);

	dbg_isr(2, "link: ERR(0x%lX, 0x%lX, 0x%lX, 0x%lX) S(0x%X) L(0x%X) E(0x%X)\n", csi,
		irq_src.err_id[CSI_VIRTUAL_CH_0],
		irq_src.err_id[CSI_VIRTUAL_CH_1],
		irq_src.err_id[CSI_VIRTUAL_CH_2],
		irq_src.err_id[CSI_VIRTUAL_CH_3],
		irq_src.otf_start, irq_src.line_end, irq_src.otf_end);

	/* Get Frame Start Status */
	frame_start = irq_src.otf_start & (1 << CSI_VIRTUAL_CH_0);

	/* Get Frame End Status */
	frame_end = irq_src.otf_end & (1 << CSI_VIRTUAL_CH_0);

	/* LINE Irq */
	if (irq_src.line_end & (1 << CSI_VIRTUAL_CH_0))
		csi_frame_line_inline(csi);


	/* Frame Start and End */
	if (frame_start && frame_end) {
		warn("[CSIS%d] start/end overlapped",
					csi->ch);
		/* frame both interrupt since latency */
		if (csi->sw_checker == EXPECT_FRAME_START) {
			csi_frame_start_inline(csi);
			csi_frame_end_inline(csi);
		} else {
			csi_frame_end_inline(csi);
			csi_frame_start_inline(csi);
		}
	/* Frame Start */
	} else if (frame_start) {
		/* W/A: Skip start tasklet at interrupt lost case */
		if (csi->sw_checker != EXPECT_FRAME_START) {
			warn("[CSIS%d] Lost end interrupt\n",
					csi->ch);
			/*
			 * Even though it skips to start tasklet,
			 * framecount of CSI device should be increased
			 * to match with chain device including DDK.
			 */
			atomic_inc(&csi->fcount);
			goto clear_status;
		}
		csi_frame_start_inline(csi);
	/* Frame End */
	} else if (frame_end) {
		/* W/A: Skip end tasklet at interrupt lost case */
		if (csi->sw_checker != EXPECT_FRAME_END) {
			warn("[CSIS%d] Lost start interrupt\n",
					csi->ch);
			/*
			 * Even though it skips to start tasklet,
			 * framecount of CSI device should be increased
			 * to match with chain device including DDK.
			 */
			atomic_inc(&csi->fcount);

			/*
			 * If LOST_FS_VC_ERR is happened, there is only end interrupt
			 * so, check if error occur continuously during 10 frame.
			 */
			/* check to error */
			for (ch = CSI_VIRTUAL_CH_0; ch < CSI_VIRTUAL_CH_MAX; ch++)
				err_flag |= (u32)csi->error_id[ch];

			/* error handling */
			if (err_flag)
				csi_err_handle(csi);

			goto clear_status;
		}
		csi_frame_end_inline(csi);
	}

	/* Check error */
	if (irq_src.err_flag)
		csi_err_check(csi, irq_src.err_id, CSIS_LINK);

clear_status:
	return IRQ_HANDLED;
}

static irqreturn_t is_isr_csi_dma(int irq, void *data)
{
	struct is_device_csi *csi;
	int dma_frame_str = 0;
	int dma_frame_end = 0;
	int dma_abort_done = 0;
	int dma_err_flag = 0;
	ulong dma_err_id[CSI_VIRTUAL_CH_MAX];
	struct csis_irq_src irq_src;
	int vc;
	struct is_framemgr *framemgr;
	struct is_device_sensor *device;
	struct is_group *group;
	struct is_subdev *dma_subdev;
	struct is_camif_wdma *wdma;

	csi = data;
	wdma = csi->wdma;
	if (!wdma) {
		merr("[CSI%d] wdma is NULL", csi, csi->ch);
		return IRQ_HANDLED;
	}

	memset(&irq_src, 0x0, sizeof(struct csis_irq_src));

	for (vc = CSI_VIRTUAL_CH_0; vc < CSI_VIRTUAL_CH_MAX; vc++) {
		if (!csi->dma_subdev[vc])
			continue;

		irq_src.dma_start = 0;
		irq_src.dma_end = 0;
		irq_src.line_end = 0;
		irq_src.dma_abort = 0;
		irq_src.err_flag = 0;
		csi_hw_g_dma_irq_src_vc(wdma->regs_ctl, &irq_src, vc, true);

		dma_err_id[vc] = irq_src.err_id[vc];
		dma_err_flag |= (int)dma_err_id[vc];

		if (irq_src.dma_start)
			dma_frame_str |= 1 << vc;

		if (irq_src.dma_end)
			dma_frame_end |= 1 << vc;

		if (irq_src.dma_abort)
			dma_abort_done |= 1 << vc;

		if (dma_err_id[vc] & ~(1 << CSIS_ERR_DMA_ABORT_DONE))
			minfo("[CSI%d][VC%d] CSIS_ERR_DMA ID(0x%08lX)",
				csi, csi->ch, vc, dma_err_id[vc]);
	}

	device = container_of(csi->subdev, struct is_device_sensor, subdev_csi);
	group = &device->group_sensor;

	if (dma_frame_str) {
		u32 level = (atomic_read(&group->scount) == 1) ? 0 : 1;

		dbg_isr(level, "DS 0x%X\n", csi, dma_frame_str);
	}

	if (dma_frame_end)
		dbg_isr(1, "DE 0x%X\n", csi, dma_frame_end);

	if (dma_abort_done) {
		dbg_isr(1, "DMA ABORT DONE 0x%X\n", csi, dma_abort_done);
		clear_bit(CSIS_DMA_FLUSH_WAIT, &csi->state);
		wake_up(&csi->dma_flush_wait_q);
	}

	for (vc = CSI_VIRTUAL_CH_0; vc < CSI_VIRTUAL_CH_MAX; vc++) {
		if (dma_frame_end & (1 << vc)) {
			/*
			 * During csis_dma disabling, need to set
			 * dma to off
			 */
			if (test_bit(CSIS_DMA_DISABLING, &csi->state))
				csi_s_output_dma(csi, vc, false);

			if (csi->f_id_dec && vc == CSI_VIRTUAL_CH_0) {
				if (csi_hw_get_version(csi->base_reg) <
					IS_CSIS_VERSION(5, 4, 0, 0)) {
					struct is_camif_wdma_module *wdma_mod =
						is_camif_wdma_module_get(csi->wdma_ch);

					/*
					 * TODO: Both FRO count and frame id decoder dosen't have to be controlled
					 * if shadowing scheme is supported at start interrupt of frame id decoder.
					 */
					if (wdma_mod)
						csi_hw_clear_fro_count(
							wdma_mod->regs,
							wdma->regs_vc[vc]);
					else
						mwarn("[CSI%d] wdma_mod is NULL", csi, csi->ch);
				}

				csi_frame_end_inline(csi);
			}

			/*
			 * The embedded data is done at fraem end.
			 * But updating frame_id have to be faster than is_sensor_dm_tag().
			 * So, csi_dma_tag is performed in ISR in only case of embedded data.
			 */
			if (IS_ENABLED(CHAIN_TAG_VC0_DMA_IN_HARDIRQ_CONTEXT) ||
				(csi->sensor_cfg->output[vc].type == VC_EMBEDDED) ||
				(csi->sensor_cfg->output[vc].type == VC_EMBEDDED2)) {
				framemgr = csis_get_vc_framemgr(csi, vc);
				if (framemgr)
					csi_dma_tag(*csi->subdev, csi, framemgr, vc);
				else
					merr("[VC%d] framemgr is NULL", csi, vc);
			} else {
				do_dma_done_work_func(csi, vc);
			}
		}

		if (dma_frame_str & (1 << vc)) {
			if (csi->f_id_dec && vc == CSI_VIRTUAL_CH_0)
				csi_frame_start_inline(csi);

			dma_subdev = csi->dma_subdev[vc];

			/*
			 * In case of VOTF, DMA should be never turned off,
			 * becase VOTF must be handled as like OTF.
			 */
			if (test_bit(IS_SUBDEV_VOTF_USE, &dma_subdev->state)) {
				if (csi->sensor_cfg->input[vc].extformat == HW_FORMAT_RAW10_SDC ||
						IS_ENABLED(CONFIG_VOTF_ONESHOT)) {
					int ret;
					struct is_framemgr *votf_fmgr;

					votf_fmgr = is_votf_get_framemgr(group, TWS, dma_subdev->id);

					if (votf_fmgr) {
						ret = votf_fmgr_call(votf_fmgr, slave, s_oneshot);
						if (ret)
							mserr("votf_oneshot_call(slave) is fail(%d)",
									device, dma_subdev, ret);
					}
				}

				continue;
			}

			if (!test_bit(IS_SUBDEV_OPEN, &dma_subdev->state) ||
				test_bit(IS_SUBDEV_INTERNAL_USE, &dma_subdev->state))
				continue;

			/* dma disable if interrupt occured */
			csi_s_output_dma(csi, vc, false);
		}
	}

	/* Check error */
	if (dma_err_flag)
		csi_err_check(csi, dma_err_id, CSIS_WDMA);

	return IRQ_HANDLED;
}

static irqreturn_t is_isr_csi_ebuf(int irq, void *data)
{
	struct is_device_csi *csi = (struct is_device_csi *)data;
	struct csis_irq_src irq_src;
	int ebuf_ch;

	memset(&irq_src, 0x0, sizeof(struct csis_irq_src));

	csi_hw_g_ebuf_irq_src(csi->ebuf_reg, &irq_src, true);

	if (irq_src.err_flag) {
		minfo("[CSI%d] occurred EBUF(0x%x)\n", csi, csi->ch,
				irq_src.ebuf_status);

		if (irq_src.ebuf_status & 0x0F) {
			/* for manual mode */
			ebuf_ch = ffs(irq_src.ebuf_status & 0x0F) - 1;

			minfo("ebuf_ch(%d)\n", csi, ebuf_ch);

			/* TO DO : reset process */

			return IRQ_HANDLED;
		}
	}

	return IRQ_HANDLED;
}

int is_csi_open(struct v4l2_subdev *subdev,
	struct is_framemgr *framemgr)
{
	int ret = 0;
	int vc;
	struct is_device_csi *csi;
	struct is_device_sensor *device;

	FIMC_BUG(!subdev);

	csi = v4l2_get_subdevdata(subdev);
	if (!csi) {
		err("csi is NULL");
		ret = -EINVAL;
		goto p_err;
	}

	csi->sensor_cfg = NULL;
	csi->error_count = 0;
	csi->error_count_vc_overlap = 0;
	csi->tasklet_csis_end_count = 0;
	csi->sensor_id = SENSOR_NAME_NOTHING;

	for (vc = CSI_VIRTUAL_CH_0; vc < CSI_VIRTUAL_CH_MAX; vc++)
		csi->error_id[vc] = 0;

	memset(&csi->image, 0, sizeof(struct is_image));
	memset(csi->pre_dma_enable, -1, sizeof(csi->pre_dma_enable));
	memset(csi->cur_dma_enable, 0, sizeof(csi->cur_dma_enable));

	device = container_of(csi->subdev, struct is_device_sensor, subdev_csi);

	csi->instance = device->instance;

	if (!test_bit(CSIS_DMA_ENABLE, &csi->state))
		goto p_err;

	minfo("[CSI%d] registered isr handler(%d) state(%d)\n", csi, csi->ch,
				csi->ch, test_bit(CSIS_DMA_ENABLE, &csi->state));
	csi->framemgr = framemgr;
	atomic_set(&csi->fcount, 0);
	atomic_set(&csi->vblank_count, 0);
	atomic_set(&csi->vvalid, 0);

	return 0;

p_err:
	return ret;
}

int is_csi_close(struct v4l2_subdev *subdev)
{
	int ret = 0;
	int vc;
	struct is_device_csi *csi;
	struct is_device_sensor *device;
	struct is_subdev *dma_subdev;

	FIMC_BUG(!subdev);

	csi = v4l2_get_subdevdata(subdev);
	if (!csi) {
		err("csi is NULL");
		ret = -EINVAL;
		goto p_err;
	}

	device = container_of(csi->subdev, struct is_device_sensor, subdev_csi);

	for (vc = CSI_VIRTUAL_CH_0; vc < CSI_VIRTUAL_CH_MAX; vc++) {
		dma_subdev = csi->dma_subdev[vc];

		if (!dma_subdev)
			continue;

		if (!test_bit(IS_SUBDEV_INTERNAL_USE, &dma_subdev->state))
			continue;

		ret = is_subdev_internal_close((void *)device, IS_DEVICE_SENSOR, dma_subdev);
		if (ret)
			merr("[CSI%d][VC%d] is_subdev_internal_close is fail(%d)", csi, csi->ch, vc, ret);
	}

	csi_dma_detach(csi);

p_err:
	return ret;
}

/* value : module enum */
static int csi_init(struct v4l2_subdev *subdev, u32 value)
{
	int ret = 0;
	struct is_device_csi *csi;
	struct is_module_enum *module;
	struct is_device_sensor *device;

	FIMC_BUG(!subdev);

	csi = v4l2_get_subdevdata(subdev);
	if (!csi) {
		err("csi is NULL");
		ret = -EINVAL;
		goto p_err;
	}

	device = container_of(csi->subdev, struct is_device_sensor, subdev_csi);
	module = &device->module_enum[value];
	csi->sensor_id = device->sensor_id;

	csi_hw_reset(csi->base_reg);

	mdbgd_front("%s(%d)\n", csi, __func__, ret);

p_err:
	return ret;
}

static int csi_s_power(struct v4l2_subdev *subdev,
	int on)
{
	int ret = 0;
	struct is_device_csi *csi;
	struct is_camif_wdma_module *wdma_mod;

	FIMC_BUG(!subdev);

	csi = (struct is_device_csi *)v4l2_get_subdevdata(subdev);
	if (!csi) {
		err("csi is NULL");
		return -EINVAL;
	}

	wdma_mod = is_camif_wdma_module_get(0); /* before DMA attach */
	if (!wdma_mod) {
		merr("[CSI%d] wdma_mod is NULL", csi, csi->ch);
		return -ENODEV;
	}

	if (on) {
		if (atomic_inc_return(&wdma_mod->active_cnt) == 1) {
			/* IP processing = 1 */
			csi_hw_dma_common_reset(wdma_mod->regs, on);
		}
	} else {
		if (atomic_dec_return(&wdma_mod->active_cnt) == 0) {
			/* For safe power-off: IP processing = 0 */
			csi_hw_dma_common_reset(wdma_mod->regs, on);
		}
	}

	return ret;
}

static int csi_g_ctrl(struct v4l2_subdev *subdev, struct v4l2_control *ctrl)
{
	struct is_device_csi *csi;
	int ret = 0;
	int vc = 0;
	struct is_camif_wdma *wdma;

	FIMC_BUG(!subdev);

	csi = (struct is_device_csi *)v4l2_get_subdevdata(subdev);
	if (!csi) {
		err("csi is NULL");
		return -EINVAL;
	}

	switch (ctrl->id) {
	case V4L2_CID_IS_G_VC1_FRAMEPTR:
	case V4L2_CID_IS_G_VC2_FRAMEPTR:
	case V4L2_CID_IS_G_VC3_FRAMEPTR:
		vc = CSI_VIRTUAL_CH_1 + (ctrl->id - V4L2_CID_IS_G_VC1_FRAMEPTR);

		wdma = csi->wdma;
		if (!wdma) {
			merr("[CSI%d][VC%d] wdma is NULL", csi, csi->ch, vc);
			return -ENODEV;
		}

		ctrl->value = csi_hw_g_frameptr(wdma->regs_vc[vc], vc);
		break;
	default:
		break;
	}

	return ret;
}

static long csi_ioctl(struct v4l2_subdev *subdev, unsigned int cmd, void *arg)
{
	int ret = 0;
	struct is_device_csi *csi;
	struct is_device_sensor *device;
	struct is_group *group;
	struct is_sensor_cfg *sensor_cfg;
	int vc;
	struct v4l2_control *ctrl;
	struct is_camif_wdma_module *wdma_mod;
	struct is_camif_wdma *wdma;

	FIMC_BUG(!subdev);

	csi = v4l2_get_subdevdata(subdev);
	if (!csi) {
		err("csi is NULL");
		ret = -EINVAL;
		goto p_err;
	}

	wdma = csi->wdma;
	device = container_of(csi->subdev, struct is_device_sensor, subdev_csi);

	switch (cmd) {
	/* cancel the current and next dma setting */
	case SENSOR_IOCTL_DMA_CANCEL:
		if (!wdma) {
			merr("[CSI%d] wdma is NULL", csi, csi->ch);
			return -ENODEV;
		}

		group = &device->group_sensor;

		if (!test_bit(IS_GROUP_VOTF_OUTPUT, &group->state)) {
			csi_hw_s_control(wdma->regs_ctl, CSIS_CTRL_DMA_ABORT_REQ, true);
			for (vc = CSI_VIRTUAL_CH_0; vc < CSI_VIRTUAL_CH_MAX; vc++)
				csi_s_output_dma(csi, vc, false);
		}
		break;
	case SENSOR_IOCTL_PATTERN_ENABLE:
		wdma_mod = is_camif_wdma_module_get(csi->wdma_ch);
		if (!wdma_mod) {
			merr("[CSI%d] wdma_mod is NULL", csi, csi->ch);
			return -ENODEV;
		}

		if (is_camif_wdma_module_test_quirk(wdma_mod,
		    IS_CAMIF_WDMA_MODULE_QUIRK_BIT_HAS_TEST_PATTERN_GEN)) {
			u32 clk = 533000000; /* Unit: Hz, This is just for debugging. So, value is fixed */
			u32 fps;

			sensor_cfg = csi->sensor_cfg;
			if (!sensor_cfg) {
				merr("[CSI%d] sensor cfg is null", csi, csi->ch);
				ret = -EINVAL;
				goto p_err;
			}

			fps = sysfs_debug.pattern_fps > 0 ?
				sysfs_debug.pattern_fps : sensor_cfg->framerate;

			ret = csi_hw_s_dma_common_pattern_enable(wdma_mod->regs,
				sensor_cfg->input[CSI_VIRTUAL_CH_0].width,
				sensor_cfg->input[CSI_VIRTUAL_CH_0].height,
				fps, clk);
			if (ret) {
				merr("[CSI%d] csi_hw_s_dma_common_pattern is fail(%d)\n", csi, csi->ch, ret);
				goto p_err;
			}
		}
		break;
	case SENSOR_IOCTL_PATTERN_DISABLE:
		wdma_mod = is_camif_wdma_module_get(csi->wdma_ch);
		if (!wdma_mod) {
			merr("[CSI%d] wdma_mod is NULL", csi, csi->ch);
			return -ENODEV;
		}

		if (is_camif_wdma_module_test_quirk(wdma_mod,
		    IS_CAMIF_WDMA_MODULE_QUIRK_BIT_HAS_TEST_PATTERN_GEN))
			csi_hw_s_dma_common_pattern_disable(wdma_mod->regs);
		break;
	case SENSOR_IOCTL_REGISTE_VOTF:
		group = &device->group_sensor;

		if (!test_bit(IS_GROUP_VOTF_OUTPUT, &group->state)) {
			mwarn("[CSI%d] group is not in VOTF state\n", csi, csi->ch);
			goto p_err;
		}

		sensor_cfg = csi->sensor_cfg;
		if (!sensor_cfg) {
			mwarn("[CSI%d] failed to get sensor_cfg", csi, csi->ch);
			goto p_err;
		}

		for (vc = CSI_VIRTUAL_CH_0; vc < CSI_VIRTUAL_CH_MAX; vc++) {
			struct is_subdev *dma_subdev;

			dma_subdev = csi->dma_subdev[vc];

			if (!dma_subdev)
				continue;

			if (sensor_cfg->input[vc].hwformat == HW_FORMAT_RAW10 ||
				sensor_cfg->input[vc].hwformat == HW_FORMAT_RAW12) {
				/*
				 * When a sensor is mode 3 of 2PD mode,
				 * image & Y data have to set as same VOTF connection
				 */
				set_bit(IS_SUBDEV_VOTF_USE, &dma_subdev->state);
			} else {
				clear_bit(IS_SUBDEV_VOTF_USE, &dma_subdev->state);
				continue;
			}

			ret = is_votf_register_framemgr(group, TWS, csi, &csi_votf_ops,
				dma_subdev->id);
			if (ret)
				mserr("is_votf_register_framemgr is failed\n", dma_subdev, dma_subdev);
		}
		break;
	case SENSOR_IOCTL_G_FRAME_ID:
		{
			u32 *hw_frame_id = (u32 *)arg;

			if (csi->f_id_dec) {
				wdma_mod = is_camif_wdma_module_get(csi->wdma_ch);
				if (!wdma_mod) {
					merr("[CSI%d] wdma_mod is NULL", csi, csi->ch);
					return -ENODEV;
				}
				csi_hw_g_dma_common_frame_id(wdma_mod->regs, csi->dma_batch_num, hw_frame_id);
			} else {
				ret = 1; /* HW frame ID decoder is not available. */
			}
		}
		break;
	case V4L2_CID_SENSOR_SET_FRS_CONTROL:
		ctrl = arg;

		switch (ctrl->value) {
		case FRS_SSM_MODE_FPS_960:
			csi->dma_batch_num = 960 / 60;
			break;
		case FRS_SSM_MODE_FPS_480:
			csi->dma_batch_num = 480 / 60;
			break;
		default:
			return ret;
		}

		if (csi_hw_get_version(csi->base_reg)
			< IS_CSIS_VERSION(5, 4, 0, 0)) {
			if (!wdma) {
				merr("[CSI%d] wdma is NULL", csi, csi->ch);
				return -ENODEV;
			}

			/*
			 * TODO: Both FRO count and frame id decoder dosen't have to be controlled
			 * if shadowing scheme is supported at start interrupt of frame id decoder.
			 */
			vc = CSI_VIRTUAL_CH_0;
			csi_hw_s_fro_count(wdma->regs_ctl,
					csi->dma_batch_num, vc);
		}

		minfo("[CSI%d] change dma_batch_num %d\n", csi, csi->ch, csi->dma_batch_num);
		break;
	case SENSOR_IOCTL_G_HW_FCOUNT:
		{
			u32 *hw_fcount = (u32 *)arg;

			*hw_fcount = csi_hw_g_fcount(csi->base_reg, CSI_VIRTUAL_CH_0);
		}
		break;
	case SENSOR_IOCTL_CSI_G_CTRL:
		ctrl = (struct v4l2_control *)arg;
		ret = csi_g_ctrl(subdev, ctrl);
		if (ret) {
			err("[CSI%d] csi_g_ctrl failed", csi->ch);
			goto p_err;
		}
		break;
	case SENSOR_IOCTL_DISABLE_VC1_VOTF:
		{
			ulong flags;
			u32 csis_ver = csi_hw_get_version(csi->base_reg);

			/*
			 * Skip CSIS DMA common votf configuration
			 * when frame ID decoder is enabled
			 * to prevent overwrite issue on CSIS v5.4.
			 */
			if (csis_ver == IS_CSIS_VERSION(5, 4, 0, 0) &&
				csi->f_id_dec)
				goto p_err;

			wdma_mod = is_camif_wdma_module_get(csi->wdma_ch);
			if (!wdma_mod) {
				merr("[CSI%d] wdma_mod is NULL", csi, csi->ch);
				return -ENODEV;
			}

			spin_lock_irqsave(&wdma_mod->slock, flags);
			clear_bit(IS_SUBDEV_VOTF_USE, &csi->dma_subdev[CSI_VIRTUAL_CH_1]->state);
			csi_hw_s_dma_common_votf_cfg(wdma_mod->regs,
					csi->dma_subdev[CSI_VIRTUAL_CH_1]->output.width,
					csi->wdma_ch,
					CSI_VIRTUAL_CH_1,
					false);
			spin_unlock_irqrestore(&wdma_mod->slock, flags);
			minfo("[CSI%d] VC1 VOTF set to 'off' by ioctl", csi, csi->ch);
		}
		break;
	case SENSOR_IOCTL_CSI_DMA_DISABLE:
		set_bit(CSIS_DMA_DISABLING, &csi->state);
		for (vc = CSI_VIRTUAL_CH_0; vc < CSI_VIRTUAL_CH_MAX; vc++)
			csi_s_output_dma(csi, vc, false);
		mdbgd_front("[CSI%d] DMA disable called by ioctl", csi, csi->ch);
		break;
	case SENSOR_IOCTL_CSI_DMA_ATTACH:
		ret = csi_dma_attach(csi);
		if (ret) {
			merr("[CSI%d] csi_dma_attach is failed", csi, csi->ch);
			goto p_err;
		}
		break;
	case SENSOR_IOCTL_G_DMA_CH:
		if (!wdma) {
			merr("[CSI%d] wdma is NULL", csi, csi->ch);
			return -ENODEV;
		}

		*(int *)arg = csi->wdma->index;
		break;
	default:
		break;
	}

p_err:
	return ret;
}

static const struct v4l2_subdev_core_ops core_ops = {
	.init = csi_init,
	.s_power = csi_s_power,
	.ioctl = csi_ioctl,
};

static void csi_free_irq(struct is_device_csi *csi)
{
	struct is_camif_wdma *wdma = csi->wdma;

	/* EBUF */
	is_free_irq(csi->ebuf_irq, csi);

	/* WDMA */
	csi_hw_s_dma_irq_msk(wdma->regs_ctl, false);
	is_free_irq(wdma->irq, csi);

	/* LINK */
	csi_hw_s_irq_msk(csi->base_reg, false, csi->f_id_dec);
	is_free_irq(csi->irq, csi);

	return;
}

static int csi_request_irq(struct is_device_csi *csi)
{
	int ret = 0;
	size_t name_len;
	struct is_camif_wdma *wdma = csi->wdma;

	/* Registeration of CSIS LINK IRQ */
	name_len = sizeof(csi->irq_name);
	snprintf(csi->irq_name, name_len, "%s-%d", "CSI", csi->ch);
	ret = is_request_irq(csi->irq, is_isr_csi,
			csi->irq_name, 0, csi);
	if (ret) {
		merr("failed to request IRQ for CSI(%d): %d", csi, csi->irq, ret);
		goto err_req_csi_irq;
	}
	csi_hw_s_irq_msk(csi->base_reg, true, csi->f_id_dec);


	ret = is_request_irq(wdma->irq, is_isr_csi_dma,
				wdma->irq_name, 0, csi);
	if (ret) {
		merr("failed to request IRQ(%d) for DMA#%d : ret(%d)",
			csi, wdma->irq, csi->wdma_ch, ret);
		goto err_req_dma_irq;
	}
	csi_hw_s_dma_irq_msk(wdma->regs_ctl, true);

	ret = is_request_irq(csi->ebuf_irq, is_isr_csi_ebuf, "CSI_EBUF",
			IRQF_SHARED, csi);
	if (ret) {
		merr("failed to request IRQ for CSI_EBUF(%d): %d", csi,
				csi->ebuf_irq, ret);
		goto err_req_ebuf_irq;
	}

	return 0;

err_req_ebuf_irq:
err_req_dma_irq:
	csi_free_irq(csi);

err_req_csi_irq:

	return ret;
}

static int csi_stream_on(struct v4l2_subdev *subdev,
	struct is_device_csi *csi)
{
	int ret = 0;
	int vc;
	u32 settle;
	u32 __iomem *base_reg;
	int ebuf_ch;
	struct is_device_sensor *device = v4l2_get_subdev_hostdata(subdev);
	struct is_sensor_cfg *sensor_cfg;
	struct is_subdev *dma_subdev;
	struct is_framemgr *ldr_framemgr;
	struct is_frame *frame;
	struct is_group *group;
	unsigned long flags;

	FIMC_BUG(!csi);
	FIMC_BUG(!device);

	ret = csi_dma_attach(csi);
	if (ret) {
		merr("[CSI%d] csi_dma_attach is failed", csi, csi->ch);
		goto err_dma_attach;
	}

	ret = is_hw_camif_cfg((void *)device);
	if (ret) {
		merr("hw_camif_cfg is error(%d)", device, ret);
		goto err_camif_cfg;
	}

	is_vendor_csi_stream_on(csi);

	if (test_bit(CSIS_START_STREAM, &csi->state)) {
		merr("[CSI%d] already start", csi, csi->ch);
		ret = -EINVAL;
		goto err_start_already;
	}

	sensor_cfg = csi->sensor_cfg;
	if (!sensor_cfg) {
		merr("[CSI%d] sensor cfg is null", csi, csi->ch);
		ret = -EINVAL;
		goto err_invalid_sensor_cfg;
	}

	base_reg = csi->base_reg;

	ldr_framemgr = GET_FRAMEMGR(device->vctx);
	if (!ldr_framemgr) {
		merr("[CSI%d] ldr_framemgr is NULL", csi, csi->ch);
		ret = -EINVAL;
		goto err_invalid_ldr_framemgr;
	}

	framemgr_e_barrier_irqs(ldr_framemgr, 0, flags);
	frame = peek_frame(ldr_framemgr, FS_REQUEST);
	framemgr_x_barrier_irqr(ldr_framemgr, 0, flags);

	csi->otf_batch_num = frame ? frame->num_buffers : 1;
	csi->hw_fcount = csi_hw_s_fcount(base_reg, CSI_VIRTUAL_CH_0, 0);
	csi->error_count = 0;
	csi->error_count_vc_overlap = 0;
	csi->tasklet_csis_end_count = 0;

	ret = csi_request_irq(csi);
	if (ret) {
		merr("[CSI%d] csi_request_irq is fail", csi, csi->ch);
		goto err_csi_request_irq;
	}

	settle = sensor_cfg->settle;
	if (!settle) {
		if (sensor_cfg->mipi_speed)
			settle = is_hw_find_settle(sensor_cfg->mipi_speed, csi->use_cphy);
		else
			merr("[CSI%d] mipi_speed is invalid", csi, csi->ch);
	}

	dma_subdev = csi->dma_subdev[CSI_VIRTUAL_CH_0];
	if (dma_subdev && test_bit(IS_SUBDEV_VOTF_USE, &dma_subdev->state))
		csi->potf = true;
	else
		csi->potf = false;

	minfo("[CSI%d] settle(%dx%d@%d) = %d, speed(%u Mbps), %u lane, potf(%d), otf_batch(%d), hw_fcount(%d)\n",
		csi, csi->ch,
		csi->image.window.width,
		csi->image.window.height,
		sensor_cfg->framerate,
		settle, sensor_cfg->mipi_speed, sensor_cfg->lanes + 1,
		csi->potf, csi->otf_batch_num, csi->hw_fcount);

	if (device->ischain)
		set_bit(CSIS_JOIN_ISCHAIN, &csi->state);
	else
		clear_bit(CSIS_JOIN_ISCHAIN, &csi->state);

	/* PHY control */
	/* PHY be configured in CSI driver */
	csi_hw_s_phy_default_value(csi->phy_reg, csi->ch);
	csi_hw_phy_otp_config(csi->phy_reg, csi->ch);
	csi_hw_s_settle(csi->phy_reg, settle);
	ret = csi_hw_s_phy_config(csi->phy_reg, sensor_cfg->lanes, sensor_cfg->mipi_speed, settle, csi->ch);
	if (ret) {
		merr("[CSI%d] csi_hw_s_phy_config is fail", csi, csi->ch);
		goto err_config_phy;
	}

#if defined(SECURE_CAMERA_FACE) && defined(PROTECT_SYSREG_IS)
	/* if sysreg_is is secure, skip phy reset */
	if (device->ex_scenario != IS_SCENARIO_SECURE) {
#endif
	/* PHY be configured in PHY driver */
	ret = csi_hw_s_phy_set(csi->phy, sensor_cfg->lanes, sensor_cfg->mipi_speed, settle, csi->ch,
				csi->use_cphy);
	if (ret) {
		merr("[CSI%d] csi_hw_s_phy_set is fail", csi, csi->ch);
		goto err_set_phy;
	}
#if defined(SECURE_CAMERA_FACE) && defined(PROTECT_SYSREG_IS)
	}
#endif

	/* CSIS core setting */
	csi_hw_s_lane(base_reg, &csi->image, sensor_cfg->lanes, sensor_cfg->mipi_speed, csi->use_cphy);
	csi_hw_s_control(base_reg, CSIS_CTRL_INTERLEAVE_MODE, sensor_cfg->interleave_mode);
	csi_hw_s_control(base_reg, CSIS_CTRL_PIXEL_ALIGN_MODE, 0x1);
	csi_hw_s_control(base_reg, CSIS_CTRL_LRTE, sensor_cfg->lrte);
	csi_hw_s_control(base_reg, CSIS_CTRL_DESCRAMBLE, device->pdata->scramble);

	if (sensor_cfg->interleave_mode == CSI_MODE_CH0_ONLY) {
		csi_hw_s_config(base_reg,
			CSI_VIRTUAL_CH_0,
			&sensor_cfg->input[CSI_VIRTUAL_CH_0],
			csi->image.window.width,
			csi->image.window.height,
			csi->potf);
	} else {
		u32 vc = 0;

		for (vc = CSI_VIRTUAL_CH_0; vc < CSI_VIRTUAL_CH_MAX; vc++) {
			csi_hw_s_config(base_reg,
					vc, &sensor_cfg->input[vc],
					sensor_cfg->input[vc].width,
					sensor_cfg->input[vc].height,
					csi->potf);

			minfo("[CSI%d][VC%d] size(%dx%d)\n", csi, csi->ch, vc,
				sensor_cfg->input[vc].width, sensor_cfg->input[vc].height);
		}
	}

	for (vc = CSI_VIRTUAL_CH_0; vc < CSI_VIRTUAL_CH_MAX; vc++) {
		/* clear fails alarm */
		csi->error_id[vc] = 0;
	}

	/* CSIS WDMA setting */
	if (test_bit(CSIS_DMA_ENABLE, &csi->state)) {
		clear_bit(CSIS_DMA_DISABLING, &csi->state);

		for (vc = CSI_VIRTUAL_CH_0; vc < CSI_VIRTUAL_CH_MAX; vc++) {
			/* runtime buffer done state for error */
			clear_bit(CSIS_BUF_ERR_VC0 + vc, &csi->state);
		}

		csi->sw_checker = EXPECT_FRAME_START;
		csi->overflow_cnt = 0;

		csi_s_config_dma(csi, sensor_cfg->output);
		memset(csi->pre_dma_enable, -1, sizeof(csi->pre_dma_enable));

		/* for multi frame buffer setting for internal vc */
		csis_s_vc_dma_multibuf(csi);

		/* Tasklet Setting */
		/* OTF */
		tasklet_init(&csi->tasklet_csis_str, tasklet_csis_str_otf, (unsigned long)subdev);
		tasklet_init(&csi->tasklet_csis_end, tasklet_csis_end, (unsigned long)subdev);

		/* DMA Workqueue Setting */
		if (csi->dma_subdev[CSI_VIRTUAL_CH_0]) {
			INIT_WORK(&csi->wq_csis_dma[CSI_VIRTUAL_CH_0], wq_csis_dma_vc0);
			init_work_list(&csi->work_list[CSI_VIRTUAL_CH_0], CSI_VIRTUAL_CH_0, MAX_WORK_COUNT);
		}

		if (csi->dma_subdev[CSI_VIRTUAL_CH_1]) {
			INIT_WORK(&csi->wq_csis_dma[CSI_VIRTUAL_CH_1], wq_csis_dma_vc1);
			init_work_list(&csi->work_list[CSI_VIRTUAL_CH_1], CSI_VIRTUAL_CH_1, MAX_WORK_COUNT);
		}

		if (csi->dma_subdev[CSI_VIRTUAL_CH_2]) {
			INIT_WORK(&csi->wq_csis_dma[CSI_VIRTUAL_CH_2], wq_csis_dma_vc2);
			init_work_list(&csi->work_list[CSI_VIRTUAL_CH_2], CSI_VIRTUAL_CH_2, MAX_WORK_COUNT);
		}

		if (csi->dma_subdev[CSI_VIRTUAL_CH_3]) {
			INIT_WORK(&csi->wq_csis_dma[CSI_VIRTUAL_CH_3], wq_csis_dma_vc3);
			init_work_list(&csi->work_list[CSI_VIRTUAL_CH_3], CSI_VIRTUAL_CH_3, MAX_WORK_COUNT);
		}
	}

	/* if sensor's output otf was enabled, enable line irq */
	if (!test_bit(IS_SENSOR_OTF_OUTPUT, &device->state)) {
		/* update line_fcount for sensor_notify_by_line */
		device->line_fcount = atomic_read(&csi->fcount) + 1;
		minfo("[CSI%d] start line irq cnt(%d)\n", csi, csi->ch, device->line_fcount);

		csi_hw_s_control(base_reg, CSIS_CTRL_LINE_RATIO, csi->image.window.height * CSI_LINE_RATIO / 20);
		csi_hw_s_control(base_reg, CSIS_CTRL_ENABLE_LINE_IRQ, 0x1);
		tasklet_init(&csi->tasklet_csis_line, tasklet_csis_line, (unsigned long)subdev);
		minfo("[CSI%d] ENABLE Line irq(%d)\n", csi, csi->ch, CSI_LINE_RATIO);
	}

	group = &device->group_sensor;
	if (!test_bit(IS_GROUP_VOTF_OUTPUT, &group->state) &&
			test_bit(IS_GROUP_OTF_OUTPUT, &group->state)) {

		mutex_lock(&csi->ebuf_lock);

		ebuf_ch = csi_g_ebuf_ch(group);
		if (csi->mcb_reg && csi->ebuf_reg && ebuf_ch >= 0) {
			minfo("CSI(%d) --> EBUF(%d)\n", csi, csi->ch, ebuf_ch);

			if (!csi_g_ebuf_rcount(csi))
				csi_hw_s_mcb_qch(csi->mcb_reg, true);

			if (!atomic_read(&csi->ebuf_rcount[ebuf_ch]))
				csi_hw_s_ebuf_enable(csi->ebuf_reg, true,
					ebuf_ch, EBUF_MANUAL_MODE);

			/* ebuf support 2 vc */
			csi_hw_s_cfg_ebuf(csi->ebuf_reg, ebuf_ch,
				CSI_VIRTUAL_CH_0,
				sensor_cfg->input[CSI_VIRTUAL_CH_0].width,
				sensor_cfg->input[CSI_VIRTUAL_CH_0].height);

			csi_hw_s_cfg_ebuf(csi->ebuf_reg, ebuf_ch,
				CSI_VIRTUAL_CH_1,
				sensor_cfg->input[CSI_VIRTUAL_CH_1].width,
				sensor_cfg->input[CSI_VIRTUAL_CH_1].height);

			atomic_inc(&csi->ebuf_rcount[ebuf_ch]);
		}
		mutex_unlock(&csi->ebuf_lock);
	}

	if (!sysfs_debug.pattern_en)
		csi_hw_enable(base_reg, csi->use_cphy);

	if (unlikely(debug_csi))
		csi_hw_dump_all(csi);

	set_bit(CSIS_START_STREAM, &csi->state);
	csi->crc_flag = false;

	return 0;

err_config_phy:
err_set_phy:
	csi_free_irq(csi);

err_csi_request_irq:
err_invalid_ldr_framemgr:
err_invalid_sensor_cfg:
err_start_already:
err_camif_cfg:
	csi_dma_detach(csi);

err_dma_attach:
	return ret;
}

static int csi_stream_off(struct v4l2_subdev *subdev,
	struct is_device_csi *csi)
{
	int vc;
	int ebuf_ch;
	u32 __iomem *base_reg;
	struct is_device_sensor *device = v4l2_get_subdev_hostdata(subdev);
	struct is_group *group;
	long timetowait;

	FIMC_BUG(!csi);
	FIMC_BUG(!device);

	if (!test_bit(CSIS_START_STREAM, &csi->state)) {
		merr("[CSI%d] already stop", csi, csi->ch);
		return -EINVAL;
	}

	group = &device->group_sensor;
	if (!test_bit(IS_GROUP_VOTF_OUTPUT, &group->state) &&
			test_bit(IS_GROUP_OTF_OUTPUT, &group->state)) {

		mutex_lock(&csi->ebuf_lock);

		ebuf_ch = csi_g_ebuf_ch(group);
		if (csi->mcb_reg && csi->ebuf_reg && ebuf_ch >= 0) {
			atomic_dec(&csi->ebuf_rcount[ebuf_ch]);

			if (!atomic_read(&csi->ebuf_rcount[ebuf_ch]))
				csi_hw_s_ebuf_enable(csi->ebuf_reg, false,
					ebuf_ch, EBUF_MANUAL_MODE);

			if (!csi_g_ebuf_rcount(csi))
				csi_hw_s_mcb_qch(csi->mcb_reg, false);
		}
		mutex_unlock(&csi->ebuf_lock);
	}

	base_reg = csi->base_reg;

	csi_hw_disable(base_reg);

	csi_hw_reset(base_reg);

	clear_bit(CSIS_DMA_DISABLING, &csi->state);

	if (!test_bit(IS_SENSOR_OTF_OUTPUT, &device->state))
		tasklet_kill(&csi->tasklet_csis_line);

	if (test_bit(CSIS_DMA_ENABLE, &csi->state)) {
		struct is_camif_wdma *wdma = csi->wdma;
		struct is_camif_wdma_module *wdma_mod = is_camif_wdma_module_get(csi->wdma_ch);

		tasklet_kill(&csi->tasklet_csis_str);
		tasklet_kill(&csi->tasklet_csis_end);

		for (vc = CSI_VIRTUAL_CH_0; vc < CSI_VIRTUAL_CH_MAX; vc++) {
			/*
			 * DMA HW doesn't have own reset register.
			 * So, all virtual ch dma should be disabled
			 * because previous state is remained after stream on.
			 */
			csi_s_output_dma(csi, vc, false);
			csi_hw_dma_reset(wdma->regs_vc[vc]);

			if (csi->dma_subdev[vc])
				if (flush_work(&csi->wq_csis_dma[vc]))
					minfo("[CSI%d][VC%d] flush_work executed!\n", csi, csi->ch, vc);
		}

		/* Always run DMA abort done to flush data that would be remained */
		set_bit(CSIS_DMA_FLUSH_WAIT, &csi->state);

		csi_hw_s_control(wdma->regs_ctl, CSIS_CTRL_DMA_ABORT_REQ, true);

		timetowait = wait_event_timeout(csi->dma_flush_wait_q,
			!test_bit(CSIS_DMA_FLUSH_WAIT, &csi->state),
			CSI_WAIT_ABORT_TIMEOUT);
		if (!timetowait)
			merr("[CSI%d] wait ABORT_DONE timeout!\n", csi, csi->ch);

		csis_flush_all_vc_buf_done(csi, VB2_BUF_STATE_ERROR);

		/* DMA vOTF config set to off to all channels */
		for (vc = CSI_VIRTUAL_CH_0; vc < CSI_VIRTUAL_CH_MAX; vc++)
			csi_hw_s_dma_common_votf_cfg(wdma_mod->regs,
					csi->dma_subdev[vc]->output.width,
					csi->wdma_ch,
					vc,
					false);

		/* Reset DMA input MUX */
		if (wdma->regs_mux)
			writel(0xFFFFFFFF, wdma->regs_mux);
	}

	atomic_set(&csi->vvalid, 0);
	memset(csi->cur_dma_enable, 0, sizeof(csi->cur_dma_enable));

	csi_free_irq(csi);

	is_vendor_csi_stream_off(csi);

	csi_dma_detach(csi);

	clear_bit(CSIS_START_STREAM, &csi->state);

	minfo("[CSI%d] stream off done\n", csi, csi->ch);

	return 0;
}

static int csi_s_stream(struct v4l2_subdev *subdev, int enable)
{
	int ret = 0;
	struct is_device_csi *csi;

	FIMC_BUG(!subdev);

	csi = (struct is_device_csi *)v4l2_get_subdevdata(subdev);
	if (!csi) {
		err("csi is NULL");
		return -EINVAL;
	}

	if (enable) {
		ret = csi_stream_on(subdev, csi);
		if (ret) {
			merr("[CSI%d] csi_stream_on is fail(%d)", csi, csi->ch, ret);
			goto p_err;
		}
	} else {
		ret = csi_stream_off(subdev, csi);
		if (ret) {
			merr("[CSI%d] csi_stream_off is fail(%d)", csi, csi->ch, ret);
			goto p_err;
		}
	}

p_err:
	mdbgd_front("%s(%d)\n", csi, __func__, ret);

	return ret;
}

static int csi_s_format(struct v4l2_subdev *subdev,
	struct v4l2_subdev_pad_config *cfg,
	struct v4l2_subdev_format *fmt)
{
	int ret = 0;
	int vc;
	struct is_device_csi *csi;
	struct is_device_sensor *device;
	struct is_sensor_cfg *sensor_cfg;

	FIMC_BUG(!subdev);
	FIMC_BUG(!fmt);

	csi = (struct is_device_csi *)v4l2_get_subdevdata(subdev);
	if (!csi) {
		err("csi is NULL");
		return -ENODEV;
	}

	csi->image.window.offs_h = 0;
	csi->image.window.offs_v = 0;
	csi->image.window.width = fmt->format.width;
	csi->image.window.height = fmt->format.height;
	csi->image.window.o_width = fmt->format.width;
	csi->image.window.o_height = fmt->format.height;
	csi->image.format.pixelformat = fmt->format.code;
	csi->image.format.field = fmt->format.field;

	device = v4l2_get_subdev_hostdata(subdev);
	if (!device) {
		merr("device is NULL", csi);
		ret = -ENODEV;
		goto p_err;
	}

	sensor_cfg = csi->sensor_cfg = device->cfg;
	if (!device->cfg) {
		merr("sensor cfg is invalid", csi);
		ret = -EINVAL;
		goto p_err;
	}

	/*
	 * DMA HW doesn't have own reset register.
	 * So, all virtual ch dma should be disabled
	 * because previous state is remained after stream on.
	 */
	for (vc = CSI_VIRTUAL_CH_0; vc < CSI_VIRTUAL_CH_MAX; vc++) {
		u32 w, h, bits_per_pixel, buffer_num;
		struct is_subdev *dma_subdev;
		struct is_vci_config *vci_cfg;
		const char *type_name;

		dma_subdev = csi->dma_subdev[vc];

		if (!dma_subdev)
			continue;

		vci_cfg = &sensor_cfg->output[vc];
		if (vci_cfg->type == VC_NOTHING)
			continue;

		w = vci_cfg->width;
		h = vci_cfg->height;

		if (sensor_cfg->dummy_pixel[vc])
			w += sensor_cfg->dummy_pixel[vc];

		/* VC type dependent value setting */
		switch (vci_cfg->type) {
		case VC_TAILPDAF:
			bits_per_pixel = 16;
			if (sensor_cfg->framerate <= 30)
				buffer_num = 3;
			else
				buffer_num = SUBDEV_INTERNAL_BUF_MAX;

			type_name = "VC_TAILPDAF";
			break;
		case VC_MIPISTAT:
			bits_per_pixel = 8;
			buffer_num = SUBDEV_INTERNAL_BUF_MAX;
			type_name = "VC_MIPISTAT";
			break;
		case VC_EMBEDDED:
			bits_per_pixel = 8;
			buffer_num = SUBDEV_INTERNAL_BUF_MAX;
			type_name = "VC_EMBEDDED";
			break;
		case VC_EMBEDDED2:
			bits_per_pixel = 8;
			buffer_num = SUBDEV_INTERNAL_BUF_MAX;
			type_name = "VC_EMBEDDED2";
			break;
		case VC_PRIVATE:
			bits_per_pixel = 8;
			buffer_num = SUBDEV_INTERNAL_BUF_MAX;
			type_name = "VC_PRIVATE";
			break;
		default:
			merr("[CSI%d][VC%d] wrong internal vc type(%d)", csi, csi->ch, vc, vci_cfg->type);
			return -EINVAL;
		}

		ret = is_subdev_internal_open((void *)device, IS_DEVICE_SENSOR, dma_subdev);
		if (ret) {
			merr("[CSI%d][VC%d] is_subdev_internal_open is fail(%d)", csi, csi->ch, vc, ret);
			return -EINVAL;
		}

		ret = is_subdev_internal_s_format((void *)device, IS_DEVICE_SENSOR, dma_subdev,
						w, h, bits_per_pixel, buffer_num, type_name);
		if (ret) {
			merr("[CSI%d][VC%d] is_subdev_internal_s_format is fail(%d)", csi, csi->ch, vc, ret);
			return -EINVAL;
		}
	}

p_err:
	mdbgd_front("%s(%dx%d, %X)\n", csi, __func__, fmt->format.width, fmt->format.height, fmt->format.code);
	return ret;
}


static int csi_s_buffer(struct v4l2_subdev *subdev, void *buf, unsigned int *size)
{
	int ret = 0;
	u32 vc = 0;
	struct is_device_csi *csi;
	struct is_framemgr *framemgr;
	struct is_subdev *dma_subdev = NULL;
	struct is_frame *frame;
	struct is_camif_wdma *wdma;

	FIMC_BUG(!subdev);

	csi = (struct is_device_csi *)v4l2_get_subdevdata(subdev);
	if (unlikely(csi == NULL)) {
		err("csi is NULL");
		ret = -EINVAL;
		goto p_err;
	}

	if (!test_bit(CSIS_DMA_ENABLE, &csi->state))
		goto p_err;

	/* for virtual channels */
	frame = (struct is_frame *)buf;
	if (!frame) {
		err("frame is NULL");
		ret = -EINVAL;
		goto p_err;
	}

	dma_subdev = frame->subdev;
	vc = CSI_ENTRY_TO_CH(dma_subdev->id);

	framemgr = GET_SUBDEV_FRAMEMGR(dma_subdev);
	if (!framemgr) {
		err("framemgr is NULL");
		ret = -EINVAL;
		goto p_err;
	}

	wdma = csi->wdma;
	if (!wdma) {
		merr("[CSI%d][VC%d] wdma is NULL", csi, csi->ch, vc);
		ret = -ENODEV;
		goto p_err;
	}

	if (!test_bit(IS_SUBDEV_VOTF_USE, &dma_subdev->state) &&
		csi_hw_g_output_dma_enable(wdma->regs_vc[vc], vc)) {
		err("[VC%d][F%d] already DMA enabled!!", vc, frame->fcount);
		ret = -EINVAL;

		frame->result = IS_SHOT_BAD_FRAME;
		trans_frame(framemgr, frame, FS_PROCESS);
	} else {
		if (csi->otf_batch_num > 1)
			csi_hw_s_fro_count(wdma->regs_ctl,
					csi->otf_batch_num, vc);

		csi_s_buf_addr(csi, frame, vc);
		csi_s_output_dma(csi, vc, true);
		csi->cur_dma_enable[vc] = 1;

		trans_frame(framemgr, frame, FS_PROCESS);
	}

p_err:
	return ret;
}


static int csi_g_errorCode(struct v4l2_subdev *subdev, u32 *errorCode)
{
	int ret = 0;
	int vc;
	struct is_device_csi *csi;

	FIMC_BUG(!subdev);

	csi = (struct is_device_csi *)v4l2_get_subdevdata(subdev);
	if (!csi) {
		err("csi is NULL");
		return -EINVAL;
	}

	for (vc = CSI_VIRTUAL_CH_0; vc < CSI_VIRTUAL_CH_MAX; vc++)
		*errorCode |= csi->error_id_last[vc];

	return ret;
}


static const struct v4l2_subdev_video_ops video_ops = {
	.s_stream = csi_s_stream,
	.s_rx_buffer = csi_s_buffer,
	.g_input_status = csi_g_errorCode
};

static const struct v4l2_subdev_pad_ops pad_ops = {
	.set_fmt = csi_s_format
};

static const struct v4l2_subdev_ops subdev_ops = {
	.core = &core_ops,
	.video = &video_ops,
	.pad = &pad_ops
};

int is_csi_probe(void *parent, u32 device_id, u32 ch, int wdma_ch_hint)
{
	int vc, m;
	int ret = 0;
	struct v4l2_subdev *subdev_csi;
	struct is_device_csi *csi;
	struct is_device_sensor *device = parent;
	struct is_core *core;
	struct resource *res;
	struct platform_device *pdev;
	struct device *dev;
	struct device_node *dnode;

	FIMC_BUG(!device);

	pdev = device->pdev;
	dev = &pdev->dev;
	dnode = dev->of_node;

	FIMC_BUG(!device->pdata);

	csi = devm_kzalloc(dev, sizeof(struct is_device_csi), GFP_KERNEL);
	if (!csi) {
		merr("failed to alloc memory for CSI device", device);
		ret = -ENOMEM;
		goto err_alloc_csi;
	}

	/* CSI */
	res = platform_get_resource_byname(pdev, IORESOURCE_MEM, "csi");
	if (!res) {
		probe_err("failed to get memory resource for CSI");
		ret = -ENODEV;
		goto err_get_csi_res;
	}

	csi->regs_start = res->start;
	csi->regs_end = res->end;
	csi->base_reg = devm_ioremap_nocache(dev, res->start, resource_size(res));
	if (!csi->base_reg) {
		probe_err("can't ioremap for CSI");
		ret = -ENOMEM;
		goto err_ioremap_csi;
	}

	csi->irq = platform_get_irq_byname(pdev, "csi");
	if (csi->irq < 0) {
		probe_err("failed to get IRQ resource for CSI: %d", csi->irq);
		ret = csi->irq;
		goto err_get_csi_irq;
	}

	csi->ebuf_irq = platform_get_irq_byname(pdev, "ebuf");
	if (csi->ebuf_irq < 0) {
		probe_err("failed to get IRQ resource for EBUF: %d", csi->ebuf_irq);
		ret = csi->ebuf_irq;
		goto err_get_ebuf_irq;
	}

	/* PHY */
	res = platform_get_resource_byname(pdev, IORESOURCE_MEM, "phy");
	if (!res) {
		probe_err("failed to get memory resource for PHY");
		ret = -ENODEV;
		goto err_get_phy_res;
	}

	csi->phy_reg = devm_ioremap_nocache(dev, res->start, resource_size(res));
	if (!csi->phy_reg) {
		probe_err("can't ioremap for PHY");
		ret = -ENOMEM;
		goto err_ioremap_phy;
	}

	csi->phy = devm_phy_get(dev, "csis_dphy");
	if (IS_ERR(csi->phy)) {
		probe_err("failed to get PHY device for CSI");
		ret = PTR_ERR(csi->phy);
		goto err_get_phy_dev;
	}

	csi->ch = ch;
	csi->wdma_ch_hint = wdma_ch_hint;
	csi->device_id = device_id;
	csi->sensor_id = SENSOR_NAME_NOTHING;
	csi->use_cphy = device->pdata->use_cphy;
	minfo("[CSI%d] use_cphy(%d)\n", csi, csi->ch, csi->use_cphy);

	/* MCB - optional? */
	res = platform_get_resource_byname(pdev, IORESOURCE_MEM, "mcb");
	if (res) {
		csi->mcb_reg = devm_ioremap_nocache(dev, res->start, resource_size(res));
		if (!csi->mcb_reg) {
			probe_err("can't ioremap for MCB");
			ret = -ENOMEM;
			goto err_ioremap_mcb;
		}
	}

	/* EBUF - optional? */
	res = platform_get_resource_byname(pdev, IORESOURCE_MEM, "ebuf");
	if (res) {
		csi->ebuf_reg = devm_ioremap_nocache(dev, res->start, resource_size(res));
		if (!csi->ebuf_reg) {
			probe_err("can't ioremap for EBUF");
			ret = -ENOMEM;
			goto err_ioremap_ebuf;
		}
	}

	subdev_csi = devm_kzalloc(dev, sizeof(struct v4l2_subdev), GFP_KERNEL);
	if (!subdev_csi) {
		merr("subdev_csi is NULL", device);
		ret = -ENOMEM;
		goto err_alloc_subdev_csi;
	}

	device->subdev_csi = subdev_csi;

	csi->subdev = &device->subdev_csi;
	core = device->private_data;

	snprintf(csi->name, IS_STR_LEN, "CSI%d", csi->ch);

	/* default state setting */
	clear_bit(CSIS_SET_MULTIBUF_VC1, &csi->state);
	clear_bit(CSIS_SET_MULTIBUF_VC2, &csi->state);
	clear_bit(CSIS_SET_MULTIBUF_VC3, &csi->state);
	set_bit(CSIS_DMA_ENABLE, &csi->state);

	/* init dma subdev slots */
	for (vc = CSI_VIRTUAL_CH_0; vc < CSI_VIRTUAL_CH_MAX; vc++)
		csi->dma_subdev[vc] = NULL;

	csi->workqueue = alloc_workqueue("is-csi/[H/U]", WQ_HIGHPRI | WQ_UNBOUND, 0);
	if (!csi->workqueue)
		probe_warn("failed to alloc CSI own workqueue, will be use global one");

	v4l2_subdev_init(subdev_csi, &subdev_ops);
	v4l2_set_subdevdata(subdev_csi, csi);
	v4l2_set_subdev_hostdata(subdev_csi, device);
	snprintf(subdev_csi->name, V4L2_SUBDEV_NAME_SIZE, "csi-subdev.%d", ch);
	ret = v4l2_device_register_subdev(&device->v4l2_dev, subdev_csi);
	if (ret) {
		merr("v4l2_device_register_subdev is fail(%d)", device, ret);
		goto err_reg_v4l2_subdev;
	}

	spin_lock_init(&csi->dma_seq_slock);
	mutex_init(&csi->ebuf_lock);

	init_waitqueue_head(&csi->dma_flush_wait_q);

	for (m = 0; m < MAX_NUM_EBUF_CH; m++)
		atomic_set(&csi->ebuf_rcount[m], 0);

	minfo("[CSI%d] %s(%d)\n", csi, csi->ch, __func__, ret);
	return 0;

err_reg_v4l2_subdev:
	devm_kfree(dev, subdev_csi);
	device->subdev_csi = NULL;

err_alloc_subdev_csi:
	if (csi->ebuf_reg)
		devm_iounmap(dev, csi->ebuf_reg);

err_ioremap_ebuf:
	if (csi->mcb_reg)
		devm_iounmap(dev, csi->mcb_reg);

err_ioremap_mcb:
	devm_phy_put(dev, csi->phy);

err_get_phy_dev:
	devm_iounmap(dev, csi->phy_reg);

err_ioremap_phy:
err_get_phy_res:
err_get_ebuf_irq:
err_get_csi_irq:
	devm_iounmap(dev, csi->base_reg);

err_ioremap_csi:
err_get_csi_res:
	devm_kfree(dev, csi);

err_alloc_csi:
	err("[SS%d][CSI%d] %s: %d", device_id, ch, __func__, ret);
	return ret;
}
