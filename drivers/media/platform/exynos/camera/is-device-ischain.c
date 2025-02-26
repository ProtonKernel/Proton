/*
 * Samsung Exynos5 SoC series FIMC-IS driver
 *
 * exynos5 fimc-is video functions
 *
 * Copyright (c) 2011 Samsung Electronics Co., Ltd
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#include <linux/module.h>
#include <linux/delay.h>
#include <linux/kernel.h>
#include <linux/errno.h>
#include <linux/interrupt.h>
#include <linux/device.h>
#include <linux/platform_device.h>
#include <linux/slab.h>
#include <asm/cacheflush.h>
#include <asm/pgtable.h>
#include <linux/firmware.h>
#include <linux/dma-mapping.h>
#include <linux/scatterlist.h>
#include <linux/videodev2.h>
#include <linux/videodev2_exynos_camera.h>
#include <linux/videodev2_exynos_media.h>
#include <linux/v4l2-mediabus.h>
#include <linux/vmalloc.h>
#include <linux/kthread.h>
#include <linux/syscalls.h>
#include <linux/bug.h>
#if IS_ENABLED(CONFIG_PCI_EXYNOS)
#include <linux/exynos-pci-ctrl.h>
#endif

#include <linux/regulator/consumer.h>
#include <linux/pinctrl/consumer.h>
#include <linux/gpio.h>
#if defined(CONFIG_SCHED_EHMP)
#include <linux/ehmp.h>
#elif defined(CONFIG_SCHED_EMS)
#include <linux/ems.h>
#endif

#include "is-binary.h"
#include "is-time.h"
#include "is-core.h"
#include "is-param.h"
#include "is-cmd.h"
#include "is-err.h"
#include "is-video.h"
#include "is-hw.h"
#include "is-spi.h"
#include "is-groupmgr.h"
#include "is-interface-wrap.h"
#include "is-device-ischain.h"
#include "is-dvfs.h"
#include "is-vender-specific.h"
#include "exynos-is-module.h"
#include "./sensor/module_framework/modules/is-device-module-base.h"

#include "is-vender-specific.h"

/* Default setting values */
#define DEFAULT_PREVIEW_STILL_WIDTH		(1280) /* sensor margin : 16 */
#define DEFAULT_PREVIEW_STILL_HEIGHT		(720) /* sensor margin : 12 */
#define DEFAULT_CAPTURE_VIDEO_WIDTH		(1920)
#define DEFAULT_CAPTURE_VIDEO_HEIGHT		(1080)
#define DEFAULT_CAPTURE_STILL_WIDTH		(2560)
#define DEFAULT_CAPTURE_STILL_HEIGHT		(1920)
#define DEFAULT_CAPTURE_STILL_CROP_WIDTH	(2560)
#define DEFAULT_CAPTURE_STILL_CROP_HEIGHT	(1440)
#define DEFAULT_PREVIEW_VIDEO_WIDTH		(640)
#define DEFAULT_PREVIEW_VIDEO_HEIGHT		(480)
#define DEFAULT_WIDTH		(320)
#define DEFAULT_HEIGHT		(240)

/* sysfs variable for debug */
extern struct is_sysfs_debug sysfs_debug;
extern int debug_clk;

#if defined(QOS_INTCAM)
extern struct is_pm_qos_request exynos_isp_qos_int_cam;
#endif
#if defined(QOS_TNR)
extern struct is_pm_qos_request exynos_isp_qos_tnr;
#endif
#if defined(QOS_CSIS)
extern struct is_pm_qos_request exynos_isp_qos_csis;
#endif
#if defined(QOS_ISP)
extern struct is_pm_qos_request exynos_isp_qos_isp;
#endif
extern struct is_pm_qos_request exynos_isp_qos_int;
extern struct is_pm_qos_request exynos_isp_qos_mem;
extern struct is_pm_qos_request exynos_isp_qos_cam;
extern struct is_pm_qos_request exynos_isp_qos_hpg;
extern struct is_pm_qos_request exynos_isp_qos_cpu_online_min;

extern const struct is_subdev_ops is_subdev_paf_ops;
extern const struct is_subdev_ops is_subdev_3aa_ops;
extern const struct is_subdev_ops is_subdev_3ac_ops;
extern const struct is_subdev_ops is_subdev_3ap_ops;
extern const struct is_subdev_ops is_subdev_3af_ops;
extern const struct is_subdev_ops is_subdev_3ag_ops;
extern const struct is_subdev_ops is_subdev_3ao_ops;
extern const struct is_subdev_ops is_subdev_3al_ops;
extern const struct is_subdev_ops is_subdev_isp_ops;
extern const struct is_subdev_ops is_subdev_ixc_ops;
extern const struct is_subdev_ops is_subdev_ixp_ops;
extern const struct is_subdev_ops is_subdev_ixt_ops;
extern const struct is_subdev_ops is_subdev_ixv_ops;
extern const struct is_subdev_ops is_subdev_ixw_ops;
extern const struct is_subdev_ops is_subdev_ixg_ops;
extern const struct is_subdev_ops is_subdev_mexc_ops;
extern const struct is_subdev_ops is_subdev_orbxc_ops;
extern const struct is_subdev_ops is_subdev_mcs_ops;
extern const struct is_subdev_ops is_subdev_mcsp_ops;
#if defined(SOC_CLH)
extern const struct is_subdev_ops is_subdev_clh_ops;
extern const struct is_subdev_ops is_subdev_clxc_ops;
#else
const struct is_subdev_ops is_subdev_clh_ops;
const struct is_subdev_ops is_subdev_clxc_ops;
#endif
#if defined(SOC_VRA)
extern const struct is_subdev_ops is_subdev_vra_ops;
#endif
#if defined(SOC_YPP)
extern const struct is_subdev_ops is_subdev_ypp_ops;
#else
const struct is_subdev_ops is_subdev_ypp_ops;
#endif
#if defined(SOC_LME0)
extern const struct is_subdev_ops is_subdev_lme_ops;
extern const struct is_subdev_ops is_subdev_lmes_ops;
extern const struct is_subdev_ops is_subdev_lmec_ops;
#else
const struct is_subdev_ops is_subdev_lme_ops;
const struct is_subdev_ops is_subdev_lmes_ops;
const struct is_subdev_ops is_subdev_lmec_ops;
#endif

static int is_ischain_paf_stop(void *qdevice,
	struct is_queue *queue);
static int is_ischain_3aa_stop(void *qdevice,
	struct is_queue *queue);
static int is_ischain_lme_stop(void *qdevice,
	struct is_queue *queue);
static int is_ischain_isp_stop(void *qdevice,
	struct is_queue *queue);
static int is_ischain_mcs_stop(void *qdevice,
	struct is_queue *queue);
static int is_ischain_vra_stop(void *qdevice,
	struct is_queue *queue);
static int is_ischain_clh_stop(void *qdevice,
	struct is_queue *queue);
static int is_ischain_ypp_stop(void *qdevice,
	struct is_queue *queue);

static int is_ischain_paf_shot(struct is_device_ischain *device,
	struct is_frame *frame);
static int is_ischain_3aa_shot(struct is_device_ischain *device,
	struct is_frame *frame);
static int is_ischain_lme_shot(struct is_device_ischain *device,
	struct is_frame *frame);
static int is_ischain_isp_shot(struct is_device_ischain *device,
	struct is_frame *frame);
static int is_ischain_mcs_shot(struct is_device_ischain *device,
	struct is_frame *frame);
#if defined(SOC_VRA)
static int is_ischain_vra_shot(struct is_device_ischain *device,
	struct is_frame *frame);
#endif
static int is_ischain_clh_shot(struct is_device_ischain *device,
	struct is_frame *frame);
static int is_ischain_ypp_shot(struct is_device_ischain *device,
	struct is_frame *frame);

static const struct sensor_param init_sensor_param = {
	.config = {
#ifdef FIXED_FPS_DEBUG
		.frametime = (1000 * 1000) / FIXED_MAX_FPS_VALUE,
		.min_target_fps = FIXED_MAX_FPS_VALUE,
		.max_target_fps = FIXED_MAX_FPS_VALUE,
#else
		.frametime = (1000 * 1000) / 30,
		.min_target_fps = 15,
		.max_target_fps = 30,
#endif
	},
};

static const struct taa_param init_taa_param = {
	.control = {
		.cmd = CONTROL_COMMAND_START,
		.bypass = CONTROL_BYPASS_DISABLE,
		.err = CONTROL_ERROR_NO,
	},
	.otf_input = {
		.cmd = OTF_INPUT_COMMAND_ENABLE,
		.format = OTF_INPUT_FORMAT_BAYER,
		.bitwidth = OTF_INPUT_BIT_WIDTH_10BIT,
		.order = OTF_INPUT_ORDER_BAYER_GR_BG,
		.width = DEFAULT_CAPTURE_STILL_WIDTH,
		.height = DEFAULT_CAPTURE_STILL_HEIGHT,
		.bayer_crop_offset_x = 0,
		.bayer_crop_offset_y = 0,
		.bayer_crop_width = 0,
		.bayer_crop_height = 0,
		.err = OTF_INPUT_ERROR_NO,
	},
	.vdma1_input = {
		.cmd = DMA_INPUT_COMMAND_DISABLE,
		.format = 0,
		.bitwidth = 0,
		.order = 0,
		.plane = 0,
		.width = 0,
		.height = 0,
		.bayer_crop_offset_x = 0,
		.bayer_crop_offset_y = 0,
		.bayer_crop_width = 0,
		.bayer_crop_height = 0,
		.err = 0,
	},
#if !defined(SOC_ORBMCH)
	.ddma_input = {
		.cmd = DMA_INPUT_COMMAND_DISABLE,
	},
#endif
	.otf_output = {
		.cmd = OTF_OUTPUT_COMMAND_DISABLE,
	},
	.vdma4_output = {
		.cmd = DMA_OUTPUT_COMMAND_DISABLE,
		.width = DEFAULT_CAPTURE_STILL_WIDTH,
		.height = DEFAULT_CAPTURE_STILL_HEIGHT,
		.format = DMA_INOUT_FORMAT_YUV444,
		.bitwidth = DMA_INOUT_BIT_WIDTH_8BIT,
		.plane = DMA_INOUT_PLANE_1,
		.order = DMA_INOUT_ORDER_YCbCr,
		.err = DMA_OUTPUT_ERROR_NO,
	},
	.vdma2_output = {
		.cmd = DMA_OUTPUT_COMMAND_DISABLE,
		.width = DEFAULT_CAPTURE_STILL_WIDTH,
		.height = DEFAULT_CAPTURE_STILL_HEIGHT,
		.format = DMA_INOUT_FORMAT_BAYER,
		.bitwidth = DMA_INOUT_BIT_WIDTH_12BIT,
		.plane = DMA_INOUT_PLANE_1,
		.order = DMA_INOUT_ORDER_GB_BG,
		.err = DMA_OUTPUT_ERROR_NO,
	},
	.efd_output = {
		.cmd = DMA_OUTPUT_COMMAND_DISABLE,
		.width = DEFAULT_WIDTH,
		.height = DEFAULT_HEIGHT,
		.format = DMA_INOUT_FORMAT_RGB,
		.bitwidth = DMA_INOUT_BIT_WIDTH_8BIT,
		.plane = DMA_INOUT_PLANE_3,
		.order = DMA_INOUT_ORDER_CbCr,
		.err = DMA_OUTPUT_ERROR_NO,
	},
	.mrg_output = {
		.cmd = DMA_OUTPUT_COMMAND_DISABLE,
		.width = DEFAULT_CAPTURE_STILL_WIDTH,
		.height = DEFAULT_CAPTURE_STILL_HEIGHT,
		.format = DMA_INOUT_FORMAT_BAYER,
		.bitwidth = DMA_INOUT_BIT_WIDTH_16BIT,
		.plane = DMA_INOUT_PLANE_1,
		.order = 0,
		.err = DMA_OUTPUT_ERROR_NO,
	},
	.ods_output = {
		.cmd = DMA_OUTPUT_COMMAND_DISABLE,
		.width = DEFAULT_CAPTURE_STILL_WIDTH / 8,
		.height = DEFAULT_CAPTURE_STILL_HEIGHT / 8,
		.format = DMA_INOUT_FORMAT_Y,
		.bitwidth = DMA_INOUT_BIT_WIDTH_8BIT,
		.plane = DMA_INOUT_PLANE_1,
		.order = 0,
		.err = DMA_OUTPUT_ERROR_NO,
	},
	.lds_output = {
		.cmd = DMA_OUTPUT_COMMAND_DISABLE,
		.width = DEFAULT_CAPTURE_STILL_WIDTH / 8,
		.height = DEFAULT_CAPTURE_STILL_HEIGHT / 8,
		.format = DMA_INOUT_FORMAT_Y,
		.bitwidth = DMA_INOUT_BIT_WIDTH_8BIT,
		.plane = DMA_INOUT_PLANE_1,
		.order = 0,
		.err = DMA_OUTPUT_ERROR_NO,
	},
#if !defined(SOC_ORBMCH)
	.ddma_output = {

		.cmd = DMA_OUTPUT_COMMAND_DISABLE,
	},
#endif
};

static const struct isp_param init_isp_param = {
	.control = {
		.cmd = CONTROL_COMMAND_START,
		.bypass = CONTROL_BYPASS_DISABLE,
		.err = CONTROL_ERROR_NO,
	},
	.otf_input = {
		.cmd = OTF_INPUT_COMMAND_DISABLE,
	},
	.vdma1_input = {
		.cmd = DMA_INPUT_COMMAND_DISABLE,
		.width = 0,
		.height = 0,
		.format = 0,
		.bitwidth = 0,
		.plane = 0,
		.order = 0,
		.err = 0,
	},
#if defined(SOC_TNR_MERGER)
	.vdma2_input = {
		.cmd = DMA_INPUT_COMMAND_DISABLE,
	},
	.vdma6_output = {
		.cmd = DMA_INPUT_COMMAND_DISABLE,
	},
	.vdma7_output = {
		.cmd = DMA_INPUT_COMMAND_DISABLE,
	},
#endif
	.vdma3_input = {
		.cmd = DMA_INPUT_COMMAND_DISABLE,
	},
	.otf_output = {
		.cmd = OTF_OUTPUT_COMMAND_ENABLE,
		.width = DEFAULT_CAPTURE_STILL_WIDTH,
		.height = DEFAULT_CAPTURE_STILL_HEIGHT,
		.format = OTF_YUV_FORMAT,
		.bitwidth = OTF_OUTPUT_BIT_WIDTH_12BIT,
		.order = OTF_OUTPUT_ORDER_BAYER_GR_BG,
		.err = OTF_OUTPUT_ERROR_NO,
	},
	.vdma4_output = {
		.cmd = DMA_OUTPUT_COMMAND_DISABLE,
	},
	.vdma5_output = {
		.cmd = DMA_OUTPUT_COMMAND_DISABLE,
	},
};

static const struct drc_param init_drc_param = {
	.control = {
		.cmd = CONTROL_COMMAND_START,
		.bypass = CONTROL_BYPASS_ENABLE,
		.err = CONTROL_ERROR_NO,
	},
	.otf_input = {
		.cmd = OTF_INPUT_COMMAND_ENABLE,
		.width = DEFAULT_CAPTURE_STILL_WIDTH,
		.height = DEFAULT_CAPTURE_STILL_HEIGHT,
		.format = OTF_YUV_FORMAT,
		.bitwidth = OTF_INPUT_BIT_WIDTH_12BIT,
		.order = OTF_INPUT_ORDER_BAYER_GR_BG,
		.err = OTF_INPUT_ERROR_NO,
	},
	.dma_input = {
		.cmd = DMA_INPUT_COMMAND_DISABLE,
		.width = DEFAULT_CAPTURE_STILL_WIDTH,
		.height = DEFAULT_CAPTURE_STILL_HEIGHT,
		.format = DMA_INOUT_FORMAT_YUV444,
		.bitwidth = DMA_INOUT_BIT_WIDTH_8BIT,
		.plane = DMA_INOUT_PLANE_1,
		.order = DMA_INOUT_ORDER_YCbCr,
		.err = 0,
	},
	.otf_output = {
		.cmd = OTF_OUTPUT_COMMAND_ENABLE,
		.width = DEFAULT_CAPTURE_STILL_WIDTH,
		.height = DEFAULT_CAPTURE_STILL_HEIGHT,
		.format = OTF_OUTPUT_FORMAT_YUV444,
		.bitwidth = OTF_INPUT_BIT_WIDTH_12BIT,
		.order = OTF_OUTPUT_ORDER_BAYER_GR_BG,
		.err = OTF_OUTPUT_ERROR_NO,
	},
};

static const struct scc_param init_scc_param = {
	.control = {
		.cmd = CONTROL_COMMAND_START,
		.bypass = CONTROL_BYPASS_ENABLE,
		.err = CONTROL_ERROR_NO,
	},
	.otf_input = {
		.cmd = OTF_INPUT_COMMAND_ENABLE,
		.width = DEFAULT_CAPTURE_STILL_WIDTH,
		.height = DEFAULT_CAPTURE_STILL_HEIGHT,
		.format = OTF_YUV_FORMAT,
		.bitwidth = OTF_INPUT_BIT_WIDTH_12BIT,
		.order = OTF_INPUT_ORDER_BAYER_GR_BG,
		.bayer_crop_offset_x = 0,
		.bayer_crop_offset_y = 0,
		.bayer_crop_width = 0,
		.bayer_crop_height = 0,
		.err = OTF_INPUT_ERROR_NO,
	},
	.effect = {
		.cmd = 0,
		.arbitrary_cb = 128, /* default value : 128 */
		.arbitrary_cr = 128, /* default value : 128 */
		.yuv_range = SCALER_OUTPUT_YUV_RANGE_FULL,
		.err = 0,
	},
	.input_crop = {
		.cmd = OTF_INPUT_COMMAND_DISABLE,
		.pos_x = 0,
		.pos_y = 0,
		.crop_width = DEFAULT_CAPTURE_STILL_CROP_WIDTH,
		.crop_height = DEFAULT_CAPTURE_STILL_CROP_HEIGHT,
		.in_width = DEFAULT_CAPTURE_STILL_WIDTH,
		.in_height = DEFAULT_CAPTURE_STILL_HEIGHT,
		.out_width = DEFAULT_CAPTURE_VIDEO_WIDTH,
		.out_height = DEFAULT_CAPTURE_VIDEO_HEIGHT,
		.err = 0,
	},
	.output_crop = {
		.cmd = SCALER_CROP_COMMAND_DISABLE,
		.pos_x = 0,
		.pos_y = 0,
		.crop_width = DEFAULT_CAPTURE_STILL_WIDTH,
		.crop_height = DEFAULT_CAPTURE_STILL_HEIGHT,
		.format = DMA_INOUT_FORMAT_YUV422,
		.err = 0,
	},
	.otf_output = {
		.cmd = OTF_OUTPUT_COMMAND_ENABLE,
		.width = DEFAULT_CAPTURE_VIDEO_WIDTH,
		.height = DEFAULT_CAPTURE_VIDEO_HEIGHT,
		.format = OTF_YUV_FORMAT,
		.bitwidth = OTF_OUTPUT_BIT_WIDTH_8BIT,
		.order = OTF_OUTPUT_ORDER_BAYER_GR_BG,
		.err = OTF_OUTPUT_ERROR_NO,
	},
	.dma_output = {
		.cmd = DMA_OUTPUT_COMMAND_DISABLE,
		.width = DEFAULT_CAPTURE_STILL_WIDTH,
		.height = DEFAULT_CAPTURE_STILL_HEIGHT,
		.format = DMA_INOUT_FORMAT_YUV422,
		.bitwidth = DMA_INOUT_BIT_WIDTH_8BIT,
		.plane = DMA_INOUT_PLANE_1,
		.order = DMA_INOUT_ORDER_YCbYCr,
		.err = DMA_OUTPUT_ERROR_NO,
	},
};

static const struct odc_param init_odc_param = {
	.control = {
		.cmd = CONTROL_COMMAND_START,
		.bypass = CONTROL_BYPASS_ENABLE,
		.err = CONTROL_ERROR_NO,
	},
	.otf_input = {
		.cmd = OTF_INPUT_COMMAND_ENABLE,
		.width = DEFAULT_CAPTURE_VIDEO_WIDTH,
		.height = DEFAULT_CAPTURE_VIDEO_HEIGHT,
		.format = OTF_YUV_FORMAT,
		.bitwidth = OTF_INPUT_BIT_WIDTH_8BIT,
		.order = OTF_INPUT_ORDER_BAYER_GR_BG,
		.bayer_crop_offset_x = 0,
		.bayer_crop_offset_y = 0,
		.bayer_crop_width = 0,
		.bayer_crop_height = 0,
		.err = OTF_INPUT_ERROR_NO,
	},
	.otf_output = {
		.cmd = OTF_OUTPUT_COMMAND_ENABLE,
		.width = DEFAULT_CAPTURE_VIDEO_WIDTH,
		.height = DEFAULT_CAPTURE_VIDEO_HEIGHT,
		.format = OTF_YUV_FORMAT,
		.bitwidth = OTF_OUTPUT_BIT_WIDTH_8BIT,
		.order = OTF_OUTPUT_ORDER_BAYER_GR_BG,
		.err = OTF_OUTPUT_ERROR_NO,
	},
};

#if defined(SOC_TPU)
static const struct tpu_param init_tpu_param = {
	.control = {
		.cmd = CONTROL_COMMAND_STOP,
		.bypass = CONTROL_BYPASS_ENABLE,
		.err = CONTROL_ERROR_NO,
	},
	.config = {
		.odc_bypass = true,
		.dis_bypass = true,
		.tdnr_bypass = true,
		.err = 0
	},
	.dma_input = {
		.cmd = DMA_INPUT_COMMAND_DISABLE,
		.width = DEFAULT_CAPTURE_VIDEO_WIDTH,
		.height = DEFAULT_CAPTURE_VIDEO_HEIGHT,
		.format = DMA_INOUT_FORMAT_YUV420,
		.order = 0,
		.err = OTF_INPUT_ERROR_NO,
	},
	.otf_input = {
		.cmd = OTF_INPUT_COMMAND_DISABLE,
		.width = DEFAULT_CAPTURE_VIDEO_WIDTH,
		.height = DEFAULT_CAPTURE_VIDEO_HEIGHT,
		.format = OTF_YUV_FORMAT,
		.bitwidth = OTF_INPUT_BIT_WIDTH_8BIT,
		.order = OTF_INPUT_ORDER_BAYER_GR_BG,
		.bayer_crop_offset_x = 0,
		.bayer_crop_offset_y = 0,
		.bayer_crop_width = 0,
		.bayer_crop_height = 0,
		.err = OTF_INPUT_ERROR_NO,
	},
	.otf_output = {
		.cmd = OTF_OUTPUT_COMMAND_ENABLE,
		.width = DEFAULT_CAPTURE_VIDEO_WIDTH,
		.height = DEFAULT_CAPTURE_VIDEO_HEIGHT,
		.format = OTF_YUV_FORMAT,
		.bitwidth = OTF_OUTPUT_BIT_WIDTH_8BIT,
		.order = OTF_OUTPUT_ORDER_BAYER_GR_BG,
		.err = OTF_OUTPUT_ERROR_NO,
	},
};
#endif

static const struct tdnr_param init_tdnr_param = {
	.control = {
		.cmd = CONTROL_COMMAND_START,
		.bypass = CONTROL_BYPASS_ENABLE,
		.err = CONTROL_ERROR_NO,
	},
	.otf_input = {
		.cmd = OTF_INPUT_COMMAND_ENABLE,
		.width = DEFAULT_CAPTURE_VIDEO_WIDTH,
		.height = DEFAULT_CAPTURE_VIDEO_HEIGHT,
		.format = OTF_YUV_FORMAT,
		.bitwidth = OTF_INPUT_BIT_WIDTH_8BIT,
		.order = OTF_INPUT_ORDER_BAYER_GR_BG,
		.err = OTF_INPUT_ERROR_NO,
	},
	.frame = {
		.cmd = 0,
		.err = 0,
	},
	.otf_output = {
		.cmd = OTF_OUTPUT_COMMAND_ENABLE,
		.width = DEFAULT_CAPTURE_VIDEO_WIDTH,
		.height = DEFAULT_CAPTURE_VIDEO_HEIGHT,
		.format = OTF_YUV_FORMAT,
		.bitwidth = OTF_INPUT_BIT_WIDTH_8BIT,
		.order = OTF_OUTPUT_ORDER_BAYER_GR_BG,
		.err = OTF_OUTPUT_ERROR_NO,
	},
	.dma_output = {
		.cmd = DMA_OUTPUT_COMMAND_DISABLE,
		.width = DEFAULT_CAPTURE_VIDEO_WIDTH,
		.height = DEFAULT_CAPTURE_VIDEO_HEIGHT,
		.format = DMA_INOUT_FORMAT_YUV420,
		.bitwidth = DMA_INOUT_BIT_WIDTH_8BIT,
		.plane = DMA_INOUT_PLANE_2,
		.order = DMA_INOUT_ORDER_CrCb,
		.err = DMA_OUTPUT_ERROR_NO,
	},
};

#ifdef SOC_MCS
static const struct mcs_param init_mcs_param = {
	.control = {
		.cmd = CONTROL_COMMAND_START,
		.bypass = CONTROL_BYPASS_DISABLE,
		.err = CONTROL_ERROR_NO,
	},
	.input = {
		.otf_cmd = OTF_INPUT_COMMAND_ENABLE,
		.otf_format = OTF_INPUT_FORMAT_YUV422,
		.otf_bitwidth = OTF_INPUT_BIT_WIDTH_8BIT,
		.otf_order = OTF_INPUT_ORDER_BAYER_GR_BG,
		.dma_cmd = DMA_INPUT_COMMAND_DISABLE,
		.dma_format = DMA_INOUT_FORMAT_YUV422,
		.dma_bitwidth = DMA_INOUT_BIT_WIDTH_8BIT,
		.dma_order = DMA_INOUT_ORDER_YCbYCr,
		.plane = DMA_INOUT_PLANE_1,
		.width = 0,
		.height = 0,
		.dma_crop_offset_x = 0,
		.dma_crop_offset_y = 0,
		.dma_crop_width = 0,
		.dma_crop_height = 0,
		.err = 0,
	},
	.output[MCSC_OUTPUT0] = {
		.otf_cmd = OTF_OUTPUT_COMMAND_DISABLE,
		.otf_format = OTF_OUTPUT_FORMAT_YUV422,
		.otf_bitwidth = OTF_OUTPUT_BIT_WIDTH_8BIT,
		.otf_order = OTF_OUTPUT_ORDER_BAYER_GR_BG,
		.dma_cmd = DMA_OUTPUT_COMMAND_DISABLE,
		.dma_format = DMA_INOUT_FORMAT_YUV420,
		.dma_bitwidth = DMA_INOUT_BIT_WIDTH_8BIT,
		.dma_order = DMA_INOUT_ORDER_CrCb,
		.plane = 0,
		.crop_offset_x = 0,
		.crop_offset_y = 0,
		.crop_width = 0,
		.crop_height = 0,
		.width = 0,
		.height = 0,
		.dma_stride_y = 0,
		.dma_stride_c = 0,
		.yuv_range = SCALER_OUTPUT_YUV_RANGE_FULL,
		.flip = SCALER_FLIP_COMMAND_NORMAL,
		.hwfc = 0,
		.err = 0,
	},
	.output[MCSC_OUTPUT1] = {
		.otf_cmd = OTF_OUTPUT_COMMAND_DISABLE,
		.otf_format = OTF_OUTPUT_FORMAT_YUV422,
		.otf_bitwidth = OTF_OUTPUT_BIT_WIDTH_8BIT,
		.otf_order = OTF_OUTPUT_ORDER_BAYER_GR_BG,
		.dma_cmd = DMA_OUTPUT_COMMAND_DISABLE,
		.dma_format = DMA_INOUT_FORMAT_YUV420,
		.dma_bitwidth = DMA_INOUT_BIT_WIDTH_8BIT,
		.dma_order = DMA_INOUT_ORDER_CrCb,
		.plane = 0,
		.crop_offset_x = 0,
		.crop_offset_y = 0,
		.crop_width = 0,
		.crop_height = 0,
		.width = 0,
		.height = 0,
		.dma_stride_y = 0,
		.dma_stride_c = 0,
		.yuv_range = SCALER_OUTPUT_YUV_RANGE_FULL,
		.flip = SCALER_FLIP_COMMAND_NORMAL,
		.hwfc = 0,
		.err = 0,
	},
	.output[MCSC_OUTPUT2] = {
		.otf_cmd = OTF_OUTPUT_COMMAND_DISABLE,
		.otf_format = OTF_OUTPUT_FORMAT_YUV422,
		.otf_bitwidth = OTF_OUTPUT_BIT_WIDTH_8BIT,
		.otf_order = OTF_OUTPUT_ORDER_BAYER_GR_BG,
		.dma_cmd = DMA_OUTPUT_COMMAND_DISABLE,
		.dma_format = DMA_INOUT_FORMAT_YUV420,
		.dma_bitwidth = DMA_INOUT_BIT_WIDTH_8BIT,
		.dma_order = DMA_INOUT_ORDER_CrCb,
		.plane = 0,
		.crop_offset_x = 0,
		.crop_offset_y = 0,
		.crop_width = 0,
		.crop_height = 0,
		.width = 0,
		.height = 0,
		.dma_stride_y = 0,
		.dma_stride_c = 0,
		.yuv_range = SCALER_OUTPUT_YUV_RANGE_FULL,
		.flip = SCALER_FLIP_COMMAND_NORMAL,
		.hwfc = 0,
		.err = 0,
	},
	.output[MCSC_OUTPUT3] = {
		.otf_cmd = OTF_OUTPUT_COMMAND_DISABLE,
		.otf_format = OTF_OUTPUT_FORMAT_YUV422,
		.otf_bitwidth = OTF_OUTPUT_BIT_WIDTH_8BIT,
		.otf_order = OTF_OUTPUT_ORDER_BAYER_GR_BG,
		.dma_cmd = DMA_OUTPUT_COMMAND_DISABLE,
		.dma_format = DMA_INOUT_FORMAT_YUV420,
		.dma_bitwidth = DMA_INOUT_BIT_WIDTH_8BIT,
		.dma_order = DMA_INOUT_ORDER_CrCb,
		.plane = 0,
		.crop_offset_x = 0,
		.crop_offset_y = 0,
		.crop_width = 0,
		.crop_height = 0,
		.width = 0,
		.height = 0,
		.dma_stride_y = 0,
		.dma_stride_c = 0,
		.yuv_range = SCALER_OUTPUT_YUV_RANGE_FULL,
		.flip = SCALER_FLIP_COMMAND_NORMAL,
		.hwfc = 0,
		.err = 0,
	},
	.output[MCSC_OUTPUT4] = {
		.otf_cmd = OTF_OUTPUT_COMMAND_DISABLE,
		.otf_format = OTF_OUTPUT_FORMAT_YUV422,
		.otf_bitwidth = OTF_OUTPUT_BIT_WIDTH_8BIT,
		.otf_order = OTF_OUTPUT_ORDER_BAYER_GR_BG,
		.dma_cmd = DMA_OUTPUT_COMMAND_DISABLE,
		.dma_format = DMA_INOUT_FORMAT_YUV420,
		.dma_bitwidth = DMA_INOUT_BIT_WIDTH_8BIT,
		.dma_order = DMA_INOUT_ORDER_CrCb,
		.plane = 0,
		.crop_offset_x = 0,
		.crop_offset_y = 0,
		.crop_width = 0,
		.crop_height = 0,
		.width = 0,
		.height = 0,
		.dma_stride_y = 0,
		.dma_stride_c = 0,
		.yuv_range = SCALER_OUTPUT_YUV_RANGE_FULL,
		.flip = SCALER_FLIP_COMMAND_NORMAL,
		.hwfc = 0,
		.err = 0,
	},
};
#endif

#ifdef SOC_CLH
static const struct clh_param init_clh_param = {
	.control = {
		.cmd = CONTROL_COMMAND_STOP,
		.bypass = CONTROL_BYPASS_ENABLE,
		.err = CONTROL_ERROR_NO,
	},
	.dma_input = {
		.cmd = DMA_INPUT_COMMAND_DISABLE,
		.width = DEFAULT_CAPTURE_VIDEO_WIDTH,
		.height = DEFAULT_CAPTURE_VIDEO_HEIGHT,
		.format = DMA_INOUT_FORMAT_YUV420,
		.order = 0,
		.err = DMA_INPUT_ERROR_NO,
	},
	.dma_output = {
		.cmd = DMA_OUTPUT_COMMAND_DISABLE,
		.width = DEFAULT_CAPTURE_VIDEO_WIDTH,
		.height = DEFAULT_CAPTURE_VIDEO_HEIGHT,
		.format = DMA_INOUT_FORMAT_YUV420,
		.bitwidth = DMA_INOUT_BIT_WIDTH_8BIT,
		.plane = DMA_INOUT_PLANE_2,
		.order = DMA_INOUT_ORDER_CrCb,
		.err = DMA_OUTPUT_ERROR_NO,
	},
};
#endif

#ifdef SOC_YPP
static const struct ypp_param init_ypp_param = {
	.control = {
		.cmd = CONTROL_COMMAND_STOP,
		.bypass = CONTROL_BYPASS_ENABLE,
		.err = CONTROL_ERROR_NO,
	},
	.dma_input = {
		.cmd = DMA_INPUT_COMMAND_DISABLE,
		.width = DEFAULT_CAPTURE_VIDEO_WIDTH,
		.height = DEFAULT_CAPTURE_VIDEO_HEIGHT,
		.format = DMA_INOUT_FORMAT_YUV422,
		.order = 0,
		.err = DMA_INPUT_ERROR_NO,
	},
	.dma_input_lv2 = {
		.cmd = DMA_INPUT_COMMAND_DISABLE,
		.width = DEFAULT_CAPTURE_VIDEO_WIDTH,
		.height = DEFAULT_CAPTURE_VIDEO_HEIGHT,
		.format = DMA_INOUT_FORMAT_YUV422,
		.order = 0,
		.err = DMA_INPUT_ERROR_NO,
	},
	.dma_input_noise = {
		.cmd = DMA_INPUT_COMMAND_DISABLE,
		.width = DEFAULT_CAPTURE_VIDEO_WIDTH,
		.height = DEFAULT_CAPTURE_VIDEO_HEIGHT,
		.format = DMA_INOUT_FORMAT_NOISE,
		.order = 0,
		.err = DMA_INPUT_ERROR_NO,
	},
	.dma_input_hist = {
		.cmd = DMA_INPUT_COMMAND_DISABLE,
		.width = DEFAULT_CAPTURE_VIDEO_WIDTH,
		.height = DEFAULT_CAPTURE_VIDEO_HEIGHT,
		.format = DMA_INOUT_FORMAT_SVHIST,
		.order = 0,
		.err = DMA_INPUT_ERROR_NO,
	},
	.otf_output = {
		.cmd = OTF_OUTPUT_COMMAND_ENABLE,
		.width = DEFAULT_CAPTURE_STILL_WIDTH,
		.height = DEFAULT_CAPTURE_STILL_HEIGHT,
		.format = OTF_YUV_FORMAT,
		.bitwidth = OTF_OUTPUT_BIT_WIDTH_12BIT,
		.order = OTF_OUTPUT_ORDER_BAYER_GR_BG,
		.err = OTF_OUTPUT_ERROR_NO,
	},
};
#endif

#ifdef SOC_LME0
static const struct lme_param init_lme_param = {
	.control = {
		.cmd = CONTROL_COMMAND_STOP,
		.bypass = CONTROL_BYPASS_ENABLE,
		.err = CONTROL_ERROR_NO,
	},
	.dma = {
		.cur_input_cmd = DMA_INPUT_COMMAND_DISABLE,
		.prev_input_cmd = DMA_INPUT_COMMAND_DISABLE,
		.output_cmd = DMA_INPUT_COMMAND_DISABLE,
		.input_format = DMA_INOUT_FORMAT_Y,
		.input_bitwidth = DMA_INOUT_BIT_WIDTH_8BIT,
		.input_order = DMA_INOUT_ORDER_NO,
		.input_plane = DMA_INOUT_PLANE_1,
		.input_msb = DMA_INOUT_BIT_WIDTH_8BIT - 1,
		.cur_input_width = DEFAULT_CAPTURE_STILL_WIDTH / 8,
		.cur_input_height = DEFAULT_CAPTURE_STILL_HEIGHT / 8,
		.prev_input_width = DEFAULT_CAPTURE_STILL_WIDTH / 8,
		.prev_input_height = DEFAULT_CAPTURE_STILL_HEIGHT / 8,
		.output_format = DMA_INOUT_FORMAT_Y,
		.output_bitwidth = DMA_INOUT_BIT_WIDTH_8BIT,
		.output_order = DMA_INOUT_ORDER_NO,
		.output_plane = DMA_INOUT_PLANE_1,
		.output_msb = DMA_INOUT_BIT_WIDTH_8BIT - 1,
		.err = DMA_INPUT_ERROR_NO,
	}
};
#endif

static const struct vra_param init_vra_param = {
	.control = {
		.cmd = CONTROL_COMMAND_STOP,
		.bypass = CONTROL_BYPASS_DISABLE,
		.err = CONTROL_ERROR_NO,
	},
	.otf_input = {
		.cmd = OTF_INPUT_COMMAND_ENABLE,
		.width = DEFAULT_PREVIEW_STILL_WIDTH,
		.height = DEFAULT_PREVIEW_STILL_HEIGHT,
		.format = OTF_YUV_FORMAT,
		.bitwidth = OTF_INPUT_BIT_WIDTH_8BIT,
		.order = OTF_INPUT_ORDER_BAYER_GR_BG,
		.err = OTF_INPUT_ERROR_NO,
	},
	.dma_input = {
		.cmd = DMA_INPUT_COMMAND_DISABLE,
		.format = 0,
		.bitwidth = 0,
		.order = 0,
		.plane = 0,
		.width = 0,
		.height = 0,
		.err = 0,
	},
	.config = {
		.cmd = FD_CONFIG_COMMAND_MAXIMUM_NUMBER |
			FD_CONFIG_COMMAND_ROLL_ANGLE |
			FD_CONFIG_COMMAND_YAW_ANGLE |
			FD_CONFIG_COMMAND_SMILE_MODE |
			FD_CONFIG_COMMAND_BLINK_MODE |
			FD_CONFIG_COMMAND_EYES_DETECT |
			FD_CONFIG_COMMAND_MOUTH_DETECT |
			FD_CONFIG_COMMAND_ORIENTATION |
			FD_CONFIG_COMMAND_ORIENTATION_VALUE,
		.mode = FD_CONFIG_MODE_NORMAL,
		.max_number = CAMERA2_MAX_FACES,
		.roll_angle = FD_CONFIG_ROLL_ANGLE_FULL,
		.yaw_angle = FD_CONFIG_YAW_ANGLE_45_90,
		.smile_mode = FD_CONFIG_SMILE_MODE_DISABLE,
		.blink_mode = FD_CONFIG_BLINK_MODE_DISABLE,
		.eye_detect = FD_CONFIG_EYES_DETECT_ENABLE,
		.mouth_detect = FD_CONFIG_MOUTH_DETECT_DISABLE,
		.orientation = FD_CONFIG_ORIENTATION_DISABLE,
		.orientation_value = 0,
		.map_width = 0,
		.map_height = 0,
		.err = ERROR_FD_NO,
	},
};

/*
static char setfile_version_str[60];
static char ddk_version_str[60];
static char rta_version_str[60];
static char comp_version_str[60];
*/

#if !defined(DISABLE_SETFILE)
static void is_ischain_cache_flush(struct is_device_ischain *this,
	u32 offset, u32 size)
{

}
#endif

static void is_ischain_region_invalid(struct is_device_ischain *device)
{

}

static void is_ischain_region_flush(struct is_device_ischain *device)
{

}

void is_ischain_meta_flush(struct is_frame *frame)
{

}

void is_ischain_meta_invalid(struct is_frame *frame)
{

}

void is_ischain_savefirm(struct is_device_ischain *this)
{
#ifdef DEBUG_DUMP_FIRMWARE
	loff_t pos;

	CALL_BUFOP(this->minfo->pb_fw, sync_for_cpu,
		this->minfo->pb_fw,
		0,
		(size_t)FW_MEM_SIZE,
		DMA_FROM_DEVICE);

	write_data_to_file("/data/firmware.bin", (char *)this->minfo->kvaddr,
		(size_t)FW_MEM_SIZE, &pos);
#endif
}

#if !defined(DISABLE_SETFILE)
static int is_ischain_loadsetf(struct is_device_ischain *device,
	struct is_vender *vender,
	ulong load_addr)
{
	int ret = 0;
	void *address;
	struct is_binary bin;
	struct is_device_sensor *sensor;
	struct is_module_enum *module;
	int position;

	mdbgd_ischain("%s\n", device, __func__);

	if (IS_ERR_OR_NULL(device->sensor)) {
		merr("sensor device is NULL", device);
		ret = -EINVAL;
		goto out;
	}

	if (test_bit(IS_ISCHAIN_OFFLINE_REPROCESSING, &device->state)) {
		module = device->module_enum;
		goto skip_get_module;
	}

	sensor = device->sensor;
	ret = is_sensor_g_module(sensor, &module);
	if (ret) {
		merr("is_sensor_g_module is fail(%d)", device, ret);
		goto out;
	}

skip_get_module:
	position = device->sensor->position;
	setup_binary_loader(&bin, 3, -EAGAIN, NULL, NULL);
	ret = request_binary(&bin, IS_SETFILE_SDCARD_PATH,
				module->setfile_name, &device->pdev->dev);
	if (ret) {
		merr("request_binary is fail(%d)", device, ret);
		goto out;
	}

	if (IS_SETFILE_SIZE < bin.size) {
		merr("setfile size error!! IS_SETFILE_SIZE(0x%x) "
			"is less than binary size(0x%x)",
			device, IS_SETFILE_SIZE, bin.size);
		ret = -EINVAL;
		goto err_setfile_size;
	}

	address = (void *)(device->minfo->kvaddr + load_addr);
	memcpy((void *)address, bin.data, bin.size);
	is_ischain_cache_flush(device, load_addr, bin.size + 1);
	carve_binary_version(IS_BIN_SETFILE, position, &bin);
err_setfile_size:
	release_binary(&bin);
out:
	if (ret)
		merr("setfile loading is fail", device);
	else
		minfo("Camera: the Setfile were applied successfully.\n", device);

	return ret;
}
#endif

static u32 is_itf_g_logical_group_id(struct is_device_ischain *device)
{
	u32 group_id = 0;
	struct is_group *next;
	struct is_group *group;
	struct is_group *child;

	group = get_ischain_leader_group(device);
	if (device->partial_update) {
		child = group;
		while (child) {
			group_id |= GROUP_ID(child->id) & GROUP_ID_PARM_MASK;
			child = child->child;
		}
	}
	else {
		next = group;
		while (next) {
			group_id |= GROUP_ID(next->id) & GROUP_ID_PARM_MASK;
			next = next->next;
		}
	}
	return group_id;
}

int is_itf_s_param(struct is_device_ischain *device,
	struct is_frame *frame,
	u32 lindex,
	u32 hindex,
	u32 indexes)
{
	int ret = 0;
	u32 flag, index;
	ulong dst_base, src_base;

	FIMC_BUG(!device);

	if (frame) {
		frame->lindex |= lindex;
		frame->hindex |= hindex;

		dst_base = (ulong)&device->is_region->parameter;
		src_base = (ulong)frame->shot->ctl.vendor_entry.parameter;

		frame->shot->ctl.vendor_entry.lowIndexParam |= lindex;
		frame->shot->ctl.vendor_entry.highIndexParam |= hindex;

		for (index = 0; lindex && (index < 32); index++) {
			flag = 1 << index;
			if (lindex & flag) {
				memcpy((ulong *)(dst_base + (index * PARAMETER_MAX_SIZE)),
					(ulong *)(src_base + (index * PARAMETER_MAX_SIZE)),
					PARAMETER_MAX_SIZE);
				lindex &= ~flag;
			}
		}

		for (index = 0; hindex && (index < 32); index++) {
			flag = 1 << index;
			if (hindex & flag) {
				memcpy((u32 *)(dst_base + ((32 + index) * PARAMETER_MAX_SIZE)),
					(u32 *)(src_base + ((32 + index) * PARAMETER_MAX_SIZE)),
					PARAMETER_MAX_SIZE);
				hindex &= ~flag;
			}
		}

		is_ischain_region_flush(device);
	} else {
		/*
		 * this check code is commented until per-frame control is worked fully
		 *
		 * if ( test_bit(IS_ISCHAIN_START, &device->state)) {
		 *	merr("s_param is fail, device already is started", device);
		 *	BUG();
		 * }
		 */

		is_ischain_region_flush(device);

		if (lindex || hindex) {
			ret = is_itf_s_param_wrap(device,
				lindex,
				hindex,
				indexes);
		}
	}

	return ret;
}

void * is_itf_g_param(struct is_device_ischain *device,
	struct is_frame *frame,
	u32 index)
{
	ulong dst_base, src_base, dst_param, src_param;

	FIMC_BUG_NULL(!device);

	if (frame) {
		dst_base = (ulong)&frame->shot->ctl.vendor_entry.parameter[0];
		dst_param = (dst_base + (index * PARAMETER_MAX_SIZE));
		src_base = (ulong)&device->is_region->parameter;
		src_param = (src_base + (index * PARAMETER_MAX_SIZE));
		memcpy((ulong *)dst_param, (ulong *)src_param, PARAMETER_MAX_SIZE);
	} else {
		dst_base = (ulong)&device->is_region->parameter;
		dst_param = (dst_base + (index * PARAMETER_MAX_SIZE));
	}

	return (void *)dst_param;
}

int is_itf_a_param(struct is_device_ischain *device,
	u32 group)
{
	int ret = 0;

	FIMC_BUG(!device);

	ret = is_itf_a_param_wrap(device, group);

	return ret;
}

static int is_itf_f_param(struct is_device_ischain *device)
{
	int ret = 0;
	u32 group = 0;

	group = is_itf_g_logical_group_id(device);

	ret = is_itf_f_param_wrap(device, group);

	return ret;
}

static int is_itf_enum(struct is_device_ischain *device)
{
	int ret = 0;

	mdbgd_ischain("%s()\n", device, __func__);

	ret = is_itf_enum_wrap(device);

	return ret;
}

void is_itf_storefirm(struct is_device_ischain *device)
{
	mdbgd_ischain("%s()\n", device, __func__);

	is_itf_storefirm_wrap(device);
}

void is_itf_restorefirm(struct is_device_ischain *device)
{
	mdbgd_ischain("%s()\n", device, __func__);

	is_itf_restorefirm_wrap(device);
}

int is_itf_set_fwboot(struct is_device_ischain *device, u32 val)
{
	int ret = 0;

	mdbgd_ischain("%s()\n", device, __func__);

	ret = is_itf_set_fwboot_wrap(device, val);

	return ret;
}

static void is_itf_param_init(struct is_region *region, u32 en_partial_up)
{
	if (en_partial_up) {
		memcpy(&region->parameter.sensor, &init_sensor_param,
			sizeof(struct sensor_param));
		memcpy(&region->parameter.taa, &init_taa_param,
			sizeof(struct taa_param));
		return;
	}

	memset(&region->parameter, 0x0, sizeof(struct is_param_region));

	memcpy(&region->parameter.sensor, &init_sensor_param,
		sizeof(struct sensor_param));
	memcpy(&region->parameter.taa, &init_taa_param,
		sizeof(struct taa_param));
	memcpy(&region->parameter.isp, &init_isp_param,
		sizeof(struct isp_param));

#ifdef SOC_TPU
	memcpy(&region->parameter.tpu, &init_tpu_param,
		sizeof(struct tpu_param));
#endif

#ifdef SOC_MCS
	memcpy(&region->parameter.mcs, &init_mcs_param,
		sizeof(struct mcs_param));
#endif
#ifdef SOC_CLH
	memcpy(&region->parameter.clh, &init_clh_param,
		sizeof(struct clh_param));
#endif

#ifdef SOC_YPP
	memcpy(&region->parameter.ypp, &init_ypp_param,
		sizeof(struct ypp_param));
#endif

#ifdef SOC_LME0
	memcpy(&region->parameter.lme, &init_lme_param,
		sizeof(struct lme_param));
#endif

#ifdef SOC_VRA
	memcpy(&region->parameter.vra, &init_vra_param,
		sizeof(struct vra_param));
#endif
}

static int is_itf_open(struct is_device_ischain *device,
	u32 module_id,
	u32 flag,
	struct is_path_info *path,
	struct sensor_open_extended *ext_info)
{
	int ret = 0;
	u32 offset_ext, offset_path;
	struct is_region *region;
	struct is_core *core;
	struct is_vender *vender;

	FIMC_BUG(!device);
	FIMC_BUG(!device->is_region);
	FIMC_BUG(!device->sensor);
	FIMC_BUG(!device->interface);
	FIMC_BUG(!ext_info);

	if (test_bit(IS_ISCHAIN_OPEN_STREAM, &device->state)) {
		merr("stream is already open", device);
		ret = -EINVAL;
		goto p_err;
	}

	region = device->is_region;
	offset_ext = 0;

	core = (struct is_core *)platform_get_drvdata(device->pdev);
	vender = &core->vender;
	is_vender_itf_open(vender, ext_info);

	memcpy(&region->shared[offset_ext], ext_info, sizeof(struct sensor_open_extended));

	offset_path = (sizeof(struct sensor_open_extended) / 4) + 1;
	memcpy(&region->shared[offset_path], path, sizeof(struct is_path_info));

	memset(&region->fd_info, 0x0, sizeof(struct nfd_info));

	is_ischain_region_flush(device);

	ret = is_itf_open_wrap(device,
		module_id,
		flag,
		offset_path);
	if (ret)
		goto p_err;

	mdbgd_ischain("margin %dx%d\n", device, device->margin_width, device->margin_height);

	is_ischain_region_invalid(device);

	if (region->shared[MAX_SHARED_COUNT-1] != MAGIC_NUMBER) {
		merr("MAGIC NUMBER error", device);
		ret = -EINVAL;
		goto p_err;
	}

	set_bit(IS_ISCHAIN_OPEN_STREAM, &device->state);

p_err:
	return ret;
}

static int is_itf_close(struct is_device_ischain *device)
{
	int ret = 0;

	FIMC_BUG(!device);
	FIMC_BUG(!device->interface);

	if (!test_bit(IS_ISCHAIN_OPEN_STREAM, &device->state)) {
		minfo("[%s] stream is already close", device, __func__);
		goto p_err;
	}

	ret = is_itf_close_wrap(device);

	clear_bit(IS_ISCHAIN_OPEN_STREAM, &device->state);

p_err:
	return ret;
}

static DEFINE_MUTEX(setf_lock);
static int is_itf_setfile(struct is_device_ischain *device,
	struct is_vender *vender)
{
	int ret = 0;
	ulong setfile_addr = 0;
	struct is_interface *itf;

	FIMC_BUG(!device);
	FIMC_BUG(!device->interface);
	FIMC_BUG(!vender);

	itf = device->interface;

	mutex_lock(&setf_lock);

	ret = is_itf_setaddr_wrap(itf, device, &setfile_addr);
	if (ret) {
		merr("is_hw_saddr is fail(%d)", device, ret);
		goto p_err;
	}

#if !defined(DISABLE_SETFILE)
	if (!setfile_addr) {
		merr("setfile address is NULL", device);
		goto p_err;
	}

	mdbgd_ischain("%s(0x%08lX)\n", device, __func__, setfile_addr);

	ret = is_ischain_loadsetf(device, vender, setfile_addr);
	if (ret) {
		merr("is_ischain_loadsetf is fail(%d)", device, ret);
		goto p_err;
	}
#endif

	ret = is_itf_setfile_wrap(itf, (device->minfo->kvaddr + setfile_addr), device);
	if (ret)
		goto p_err;

p_err:
	mutex_unlock(&setf_lock);
	return ret;
}

int is_itf_stream_on(struct is_device_ischain *device)
{
	int ret = 0;
	u32 retry = 30000;
	struct is_device_sensor *sensor;
	struct is_groupmgr *groupmgr;
	struct is_group *group_leader;
	struct is_framemgr *framemgr;
	struct is_frame *frame;
	unsigned long flags;
	struct is_resourcemgr *resourcemgr;
	struct is_group_task *gtask;
	u32 scount, init_shots, qcount;
	int scenario_id  = 0;

	FIMC_BUG(!device);
	FIMC_BUG(!device->groupmgr);
	FIMC_BUG(!device->resourcemgr);
	FIMC_BUG(!device->sensor);

	sensor = device->sensor;
	groupmgr = device->groupmgr;
	resourcemgr = device->resourcemgr;
	group_leader = groupmgr->leader[device->instance];
	if (!group_leader) {
		merr("stream leader is NULL", device);
		ret = -EINVAL;
		goto p_err;
	}

	gtask = &groupmgr->gtask[group_leader->id];
	init_shots = group_leader->init_shots;
	scount = atomic_read(&group_leader->scount);

	framemgr = GET_HEAD_GROUP_FRAMEMGR(group_leader, 0);
	if (!framemgr) {
		merr("leader framemgr is NULL", device);
		ret = -EINVAL;
		goto p_err;
	}

	qcount = framemgr->queued_count[FS_REQUEST] + framemgr->queued_count[FS_PROCESS];

	if (init_shots) {
		/* 3ax  group should be started */
		if (!test_bit(IS_GROUP_START, &group_leader->state)) {
			merr("stream leader is NOT started", device);
			ret = -EINVAL;
			goto p_err;
		}

		while (--retry && (scount < init_shots)) {
			udelay(100);
			scount = atomic_read(&group_leader->scount);
		}
	}

	/*
	 * If batch(FRO) mode is used,
	 * asyn shot count doesn't have to bigger than MIN_OF_ASYNC_SHOTS(=1),
	 * because it will be operated like 60 fps.
	 */
	framemgr_e_barrier_irqs(framemgr, 0, flags);
	frame = peek_frame(framemgr, FS_PROCESS);
	framemgr_x_barrier_irqr(framemgr, 0, flags);
	if (frame && frame->num_buffers <= 1) {
		/* trigger one more asyn_shots */
		if (is_sensor_g_fast_mode(sensor) == 1) {
			group_leader->asyn_shots += 1;
			group_leader->skip_shots = group_leader->asyn_shots;
			atomic_inc(&group_leader->smp_shot_count);
			up(&group_leader->smp_trigger);
			up(&gtask->smp_resource);
		}
	}

	minfo("[ISC:D] stream on %s ready(scnt: %d, qcnt: %d, init: %d, asyn: %d, skip: %d)\n",
		device, retry ? "OK" : "NOT", scount, qcount, init_shots,
		group_leader->asyn_shots, group_leader->skip_shots);

#ifdef ENABLE_DVFS
	if ((!is_pm_qos_request_active(&device->user_qos)) && (sysfs_debug.en_dvfs)) {
		struct is_dvfs_ctrl *dvfs_ctrl;

		dvfs_ctrl = &resourcemgr->dvfs_ctrl;

		mutex_lock(&dvfs_ctrl->lock);

		/* try to find dynamic scenario to apply */
		scenario_id = is_dvfs_sel_static(device);
		if (scenario_id >= 0 && !is_dvfs_is_fast_ae(dvfs_ctrl)) {
			struct is_dvfs_scenario_ctrl *static_ctrl = dvfs_ctrl->static_ctrl;
			minfo("[ISC:D] tbl[%d] static scenario(%d)-[%s]\n", device,
				dvfs_ctrl->dvfs_table_idx, scenario_id,
				static_ctrl->scenarios[static_ctrl->cur_scenario_idx].scenario_nm);
			is_set_dvfs((struct is_core *)device->interface->core, device, scenario_id);
		}

		mutex_unlock(&dvfs_ctrl->lock);
	}
#endif

	/* set bts_scen by scenario */
	is_hw_configure_bts_scen(resourcemgr, scenario_id);

	is_resource_set_global_param(resourcemgr, device);
	is_resource_update_lic_sram(resourcemgr, device, true);

	if (debug_clk > 0)
		CALL_POPS(device, print_clk);

	ret = is_itf_stream_on_wrap(device);

p_err:
	return ret;
}

int is_itf_stream_off(struct is_device_ischain *device)
{
	int ret = 0;
	struct is_resourcemgr *resourcemgr;

	FIMC_BUG(!device);

	minfo("[ISC:D] stream off ready\n", device);

	ret = is_itf_stream_off_wrap(device);

	resourcemgr = device->resourcemgr;
	is_resource_clear_global_param(resourcemgr, device);
	is_resource_update_lic_sram(resourcemgr, device, false);

	return ret;
}

int is_itf_process_start(struct is_device_ischain *device,
	u32 group)
{
	int ret = 0;

	ret = is_itf_process_on_wrap(device, group);

	return ret;
}

int is_itf_process_stop(struct is_device_ischain *device,
	u32 group)
{
	int ret = 0;

	ret = is_itf_process_off_wrap(device, group, 0);

	return ret;
}

int is_itf_force_stop(struct is_device_ischain *device,
	u32 group)
{
	int ret = 0;

	ret = is_itf_process_off_wrap(device, group, 1);

	return ret;
}

static int is_itf_init_process_start(struct is_device_ischain *device)
{
	int ret = 0;
	u32 group = 0;

	group = is_itf_g_logical_group_id(device);

	ret = is_itf_process_on_wrap(device, group);

	return ret;
}

int is_itf_i2c_lock(struct is_device_ischain *this,
	int i2c_clk, bool lock)
{
	int ret = 0;
	struct is_interface *itf = this->interface;

	if (lock)
		is_interface_lock(itf);

	ret = is_hw_i2c_lock(itf, this->instance, i2c_clk, lock);

	if (!lock)
		is_interface_unlock(itf);

	return ret;
}

#ifdef PRINT_CAPABILITY
static int is_itf_g_capability(struct is_device_ischain *this)
{
	int ret = 0;
#ifdef PRINT_CAPABILITY
	u32 metadata;
	u32 index;
	struct camera2_sm *capability;
#endif

	ret = is_hw_g_capability(this->interface, this->instance,
		(u32)(this->kvaddr_shared - this->minfo->kvaddr));

	is_ischain_region_invalid(this);

#ifdef PRINT_CAPABILITY
	memcpy(&this->capability, &this->is_region->shared,
		sizeof(struct camera2_sm));
	capability = &this->capability;

	printk(KERN_INFO "===ColorC================================\n");
	printk(KERN_INFO "===ToneMapping===========================\n");
	metadata = capability->tonemap.maxCurvePoints;
	printk(KERN_INFO "maxCurvePoints : %d\n", metadata);

	printk(KERN_INFO "===Scaler================================\n");
	printk(KERN_INFO "foramt : %d, %d, %d, %d\n",
		capability->scaler.availableFormats[0],
		capability->scaler.availableFormats[1],
		capability->scaler.availableFormats[2],
		capability->scaler.availableFormats[3]);

	printk(KERN_INFO "===StatisTicsG===========================\n");
	index = 0;
	metadata = capability->stats.availableFaceDetectModes[index];
	while (metadata) {
		printk(KERN_INFO "availableFaceDetectModes : %d\n", metadata);
		index++;
		metadata = capability->stats.availableFaceDetectModes[index];
	}
	printk(KERN_INFO "maxFaceCount : %d\n",
		capability->stats.maxFaceCount);
	printk(KERN_INFO "histogrambucketCount : %d\n",
		capability->stats.histogramBucketCount);
	printk(KERN_INFO "maxHistogramCount : %d\n",
		capability->stats.maxHistogramCount);
	printk(KERN_INFO "sharpnessMapSize : %dx%d\n",
		capability->stats.sharpnessMapSize[0],
		capability->stats.sharpnessMapSize[1]);
	printk(KERN_INFO "maxSharpnessMapValue : %d\n",
		capability->stats.maxSharpnessMapValue);

	printk(KERN_INFO "===3A====================================\n");
	printk(KERN_INFO "maxRegions : %d\n", capability->aa.maxRegions);

	index = 0;
	metadata = capability->aa.aeAvailableModes[index];
	while (metadata) {
		printk(KERN_INFO "aeAvailableModes : %d\n", metadata);
		index++;
		metadata = capability->aa.aeAvailableModes[index];
	}
	printk(KERN_INFO "aeCompensationStep : %d,%d\n",
		capability->aa.aeCompensationStep.num,
		capability->aa.aeCompensationStep.den);
	printk(KERN_INFO "aeCompensationRange : %d ~ %d\n",
		capability->aa.aeCompensationRange[0],
		capability->aa.aeCompensationRange[1]);
	index = 0;
	metadata = capability->aa.aeAvailableTargetFpsRanges[index][0];
	while (metadata) {
		printk(KERN_INFO "TargetFpsRanges : %d ~ %d\n", metadata,
			capability->aa.aeAvailableTargetFpsRanges[index][1]);
		index++;
		metadata = capability->aa.aeAvailableTargetFpsRanges[index][0];
	}
	index = 0;
	metadata = capability->aa.aeAvailableAntibandingModes[index];
	while (metadata) {
		printk(KERN_INFO "aeAvailableAntibandingModes : %d\n",
			metadata);
		index++;
		metadata = capability->aa.aeAvailableAntibandingModes[index];
	}
	index = 0;
	metadata = capability->aa.awbAvailableModes[index];
	while (metadata) {
		printk(KERN_INFO "awbAvailableModes : %d\n", metadata);
		index++;
		metadata = capability->aa.awbAvailableModes[index];
	}
	index = 0;
	metadata = capability->aa.afAvailableModes[index];
	while (metadata) {
		printk(KERN_INFO "afAvailableModes : %d\n", metadata);
		index++;
		metadata = capability->aa.afAvailableModes[index];
	}
#endif
	return ret;
}
#endif

int is_itf_power_down(struct is_interface *interface)
{
	int ret = 0;

	ret = is_itf_power_down_wrap(interface, 0);

	return ret;
}

int is_itf_sys_ctl(struct is_device_ischain *this,
			int cmd, int val)
{
	int ret = 0;

	ret = is_itf_sys_ctl_wrap(this, cmd, val);

	return ret;
}

static int is_itf_sensor_mode(struct is_device_ischain *device)
{
	int ret = 0;
	struct is_interface *interface;
	struct is_device_sensor *sensor;
	struct is_sensor_cfg *cfg;
	u32 instance, module;

	FIMC_BUG(!device);
	FIMC_BUG(!device->sensor);
	FIMC_BUG(!device->interface);

	if (test_bit(IS_ISCHAIN_REPROCESSING, &device->state))
		goto p_err;

	sensor = device->sensor;
	module = device->module;
	instance = device->instance;
	interface = device->interface;

	cfg = is_sensor_g_mode(sensor);
	if (!cfg) {
		merr("is_sensor_g_mode is fail", device);
		ret = -EINVAL;
		goto p_err;
	}

	ret = is_itf_sensor_mode_wrap(device, cfg);
	if (ret)
		goto p_err;

p_err:
	return ret;
}

int is_itf_grp_shot(struct is_device_ischain *device,
	struct is_group *group,
	struct is_frame *frame)
{
	int ret = 0;
#ifdef CONFIG_USE_SENSOR_GROUP
	unsigned long flags;
	struct is_group *head;
	struct is_framemgr *framemgr;
	bool is_remosaic_preview = false;
#endif
	FIMC_BUG(!device);
	FIMC_BUG(!group);
	FIMC_BUG(!frame);
	FIMC_BUG(!frame->shot);

	/* Cache Flush */
	is_ischain_meta_flush(frame);

	mgrdbgs(1, " SHOT(%d)\n", device, group, frame, frame->index);

#ifdef CONFIG_USE_SENSOR_GROUP

#ifdef ENABLE_MODECHANGE_CAPTURE
	if (!test_bit(IS_ISCHAIN_REPROCESSING, &device->state)
		&& CHK_MODECHANGE_SCN(frame->shot->ctl.aa.captureIntent))
		is_remosaic_preview = true;
#endif

	head = GET_HEAD_GROUP_IN_DEVICE(IS_DEVICE_ISCHAIN, group);
	if (head && !is_remosaic_preview) {
		ret = is_itf_shot_wrap(device, group, frame);
	} else {
		framemgr = GET_HEAD_GROUP_FRAMEMGR(group, frame->cur_shot_idx);
		if (!framemgr) {
			merr("framemgr is NULL", group);
			ret = -EINVAL;
			goto p_err;
		}

		set_bit(group->leader.id, &frame->out_flag);
		framemgr_e_barrier_irqs(framemgr, FMGR_IDX_25, flags);
		trans_frame(framemgr, frame, FS_PROCESS);
		framemgr_x_barrier_irqr(framemgr, FMGR_IDX_25, flags);
	}
#else
	ret = is_itf_shot_wrap(device, group, frame);
#endif

p_err:
	return ret;
}

int is_ischain_runtime_suspend(struct device *dev)
{
	int ret = 0;
	struct platform_device *pdev = to_platform_device(dev);
	struct is_core *core = (struct is_core *)dev_get_drvdata(dev);
	struct is_mem *mem = &core->resourcemgr.mem;
	struct exynos_platform_is *pdata;

#if defined(CONFIG_PM_DEVFREQ)
#if defined(QOS_INTCAM)
	int int_cam_qos;
#endif
#if defined(QOS_TNR)
	int tnr_qos;
#endif
#if defined(QOS_CSIS)
	int csis_qos;
#endif
#if defined(QOS_ISP)
	int isp_qos;
#endif
	int int_qos, mif_qos, cam_qos, hpg_qos;
	int refcount;
#endif
	pdata = dev_get_platdata(dev);

	FIMC_BUG(!pdata);
	FIMC_BUG(!pdata->clk_off);

	info("FIMC_IS runtime suspend in\n");

	CALL_MEMOP(mem, suspend, mem->default_ctx);

#if IS_ENABLED(CONFIG_PCI_EXYNOS)
	exynos_pcie_l1ss_ctrl(1, PCIE_L1SS_CTRL_CAMERA);
#endif

	ret = pdata->clk_off(&pdev->dev);
	if (ret)
		err("clk_off is fail(%d)", ret);

#if defined(CONFIG_PM_DEVFREQ)
	refcount = atomic_dec_return(&core->resourcemgr.qos_refcount);
	if (refcount == 0) {
		/* DEVFREQ release */
		dbgd_resource("[RSC] %s: QoS UNLOCK\n", __func__);
#if defined(QOS_INTCAM)
		int_cam_qos = is_get_qos(core, IS_DVFS_INT_CAM, IS_SN_MAX);
#endif
#if defined(QOS_TNR)
		tnr_qos = is_get_qos(core, IS_DVFS_TNR, IS_SN_MAX);
#endif
#if defined(QOS_CSIS)
		csis_qos = is_get_qos(core, IS_DVFS_CSIS, IS_SN_MAX);
#endif
#if defined(QOS_ISP)
		isp_qos = is_get_qos(core, IS_DVFS_ISP, IS_SN_MAX);
#endif
		int_qos = is_get_qos(core, IS_DVFS_INT, IS_SN_MAX);
		mif_qos = is_get_qos(core, IS_DVFS_MIF, IS_SN_MAX);
		cam_qos = is_get_qos(core, IS_DVFS_CAM, START_DVFS_LEVEL);
		hpg_qos = is_get_qos(core, IS_DVFS_HPG, START_DVFS_LEVEL);

#if defined(QOS_INTCAM)
		if (int_cam_qos > 0)
			is_pm_qos_remove_request(&exynos_isp_qos_int_cam);
#endif
#if defined(QOS_TNR)
		if (tnr_qos > 0)
			is_pm_qos_remove_request(&exynos_isp_qos_tnr);
#endif
#if defined(QOS_CSIS)
		if (csis_qos > 0)
			is_pm_qos_remove_request(&exynos_isp_qos_csis);
#endif
#if defined(QOS_ISP)
		if (isp_qos > 0)
			is_pm_qos_remove_request(&exynos_isp_qos_isp);
#endif
		if (int_qos > 0)
			is_pm_qos_remove_request(&exynos_isp_qos_int);
		if (mif_qos > 0)
			is_pm_qos_remove_request(&exynos_isp_qos_mem);
		if (cam_qos > 0)
			is_pm_qos_remove_request(&exynos_isp_qos_cam);
		if (hpg_qos > 0)
			is_pm_qos_remove_request(&exynos_isp_qos_hpg);
	}
#endif
#if defined (CONFIG_HOTPLUG_CPU) && defined (IS_ONLINE_CPU_MIN)
	is_pm_qos_remove_request(&exynos_isp_qos_cpu_online_min);
#endif
	is_hardware_runtime_suspend(&core->hardware);
#ifdef CAMERA_HW_BIG_DATA_FILE_IO
	if (is_sec_need_update_to_file())
		is_sec_copy_err_cnt_to_file();
#endif
	info("FIMC_IS runtime suspend out\n");
	return 0;
}

int is_ischain_runtime_resume(struct device *dev)
{
	int ret = 0;
	struct platform_device *pdev = to_platform_device(dev);
	struct is_core *core = (struct is_core *)dev_get_drvdata(dev);
	struct is_mem *mem = &core->resourcemgr.mem;
	struct exynos_platform_is *pdata;

#if defined(CONFIG_PM_DEVFREQ)
#if defined(QOS_INTCAM)
	int int_cam_qos;
#endif
#if defined(QOS_TNR)
	int tnr_qos;
#endif
#if defined(QOS_CSIS)
	int csis_qos;
#endif
#if defined(QOS_ISP)
	int isp_qos;
#endif
	int int_qos, mif_qos, cam_qos, hpg_qos;
	int refcount;
	char *qos_info;
#endif
	pdata = dev_get_platdata(dev);

	FIMC_BUG(!pdata);
	FIMC_BUG(!pdata->clk_cfg);
	FIMC_BUG(!pdata->clk_on);

	info("FIMC_IS runtime resume in\n");

	ret = is_ischain_runtime_resume_pre(dev);
	if (ret) {
		err("is_runtime_resume_pre is fail(%d)", ret);
		goto p_err;
	}

	ret = pdata->clk_cfg(&pdev->dev);
	if (ret) {
		err("clk_cfg is fail(%d)", ret);
		goto p_err;
	}

	/* HACK: DVFS lock sequence is change.
	 * DVFS level should be locked after power on.
	 */
#if defined(CONFIG_PM_DEVFREQ)
	refcount = atomic_inc_return(&core->resourcemgr.qos_refcount);
	if (refcount == 1) {
#if defined(QOS_INTCAM)
		int_cam_qos = is_get_qos(core, IS_DVFS_INT_CAM, START_DVFS_LEVEL);
#endif
#if defined(QOS_TNR)
		tnr_qos = is_get_qos(core, IS_DVFS_TNR, START_DVFS_LEVEL);
#endif
#if defined(QOS_CSIS)
		csis_qos = is_get_qos(core, IS_DVFS_CSIS, START_DVFS_LEVEL);
#endif
#if defined(QOS_ISP)
		isp_qos = is_get_qos(core, IS_DVFS_ISP, START_DVFS_LEVEL);
#endif
		int_qos = is_get_qos(core, IS_DVFS_INT, START_DVFS_LEVEL);
		mif_qos = is_get_qos(core, IS_DVFS_MIF, START_DVFS_LEVEL);
		cam_qos = is_get_qos(core, IS_DVFS_CAM, START_DVFS_LEVEL);
		hpg_qos = is_get_qos(core, IS_DVFS_HPG, START_DVFS_LEVEL);

		/* DEVFREQ lock */
#if defined(QOS_INTCAM)
		if (int_cam_qos > 0 && !is_pm_qos_request_active(&exynos_isp_qos_int_cam))
			is_pm_qos_add_request(&exynos_isp_qos_int_cam, PM_QOS_INTCAM_THROUGHPUT, int_cam_qos);
#endif
#if defined(QOS_TNR)
		if (tnr_qos > 0 && !is_pm_qos_request_active(&exynos_isp_qos_tnr))
			is_pm_qos_add_request(&exynos_isp_qos_tnr, PM_QOS_TNR_THROUGHPUT, tnr_qos);
#endif
#if defined(QOS_CSIS)
		if (csis_qos > 0 && !is_pm_qos_request_active(&exynos_isp_qos_csis))
			is_pm_qos_add_request(&exynos_isp_qos_csis, PM_QOS_CSIS_THROUGHPUT, csis_qos);
#endif
#if defined(QOS_ISP)
		if (isp_qos > 0 && !is_pm_qos_request_active(&exynos_isp_qos_isp))
			is_pm_qos_add_request(&exynos_isp_qos_isp, PM_QOS_ISP_THROUGHPUT, isp_qos);
#endif
		if (int_qos > 0 && !is_pm_qos_request_active(&exynos_isp_qos_int))
			is_pm_qos_add_request(&exynos_isp_qos_int, PM_QOS_DEVICE_THROUGHPUT, int_qos);
		if (mif_qos > 0 && !is_pm_qos_request_active(&exynos_isp_qos_mem))
			is_pm_qos_add_request(&exynos_isp_qos_mem, PM_QOS_BUS_THROUGHPUT, mif_qos);
		if (cam_qos > 0 && !is_pm_qos_request_active(&exynos_isp_qos_cam))
			is_pm_qos_add_request(&exynos_isp_qos_cam, PM_QOS_CAM_THROUGHPUT, cam_qos);
		if (hpg_qos > 0 && !is_pm_qos_request_active(&exynos_isp_qos_hpg))
			is_pm_qos_add_request(&exynos_isp_qos_hpg, PM_QOS_CPU_ONLINE_MIN, hpg_qos);

		qos_info = __getname();
		if (unlikely(!qos_info)) {
			ret = -ENOMEM;
			goto p_err;
		}
		snprintf(qos_info, PATH_MAX, "[RSC] %s: QoS LOCK", __func__);
#if defined(QOS_INTCAM)
		snprintf(qos_info + strlen(qos_info),
			PATH_MAX, " [INT_CAM(%d)]", int_cam_qos);
#endif
#if defined(QOS_TNR)
		snprintf(qos_info + strlen(qos_info),
			PATH_MAX, " [TNR(%d)]", tnr_qos);
#endif
#if defined(QOS_CSIS)
		snprintf(qos_info + strlen(qos_info),
			PATH_MAX, " [CSIS(%d)]", csis_qos);
#endif
#if defined(QOS_ISP)
		snprintf(qos_info + strlen(qos_info),
			PATH_MAX, " [ISP(%d)]", isp_qos);
#endif
		info("%s [INT(%d), MIF(%d), CAM(%d), HPG(%d)]\n",
			qos_info, int_qos, mif_qos, cam_qos, hpg_qos);
		__putname(qos_info);
	}
#endif
#if defined (CONFIG_HOTPLUG_CPU) && defined (IS_ONLINE_CPU_MIN)
	is_pm_qos_add_request(&exynos_isp_qos_cpu_online_min, PM_QOS_CPU_ONLINE_MIN,
		IS_ONLINE_CPU_MIN);
#endif
	/* Clock on */
	ret = pdata->clk_on(&pdev->dev);
	if (ret) {
		err("clk_on is fail(%d)", ret);
		goto p_err;
	}

	CALL_MEMOP(mem, resume, mem->default_ctx);

#if IS_ENABLED(CONFIG_PCI_EXYNOS)
	exynos_pcie_l1ss_ctrl(0, PCIE_L1SS_CTRL_CAMERA);
#endif

	is_hardware_runtime_resume(&core->hardware);
p_err:
	info("FIMC-IS runtime resume out\n");
	return ret;
}

int is_ischain_power(struct is_device_ischain *device, int on)
{
	int ret = 0;
	int retry = 4;
#if defined(CONFIG_PM)
	int rpm_ret;
#endif
	struct device *dev;
	struct is_core *core;
	struct is_vender *vender;

	FIMC_BUG(!device);
	FIMC_BUG(!device->interface);

	dev = &device->pdev->dev;
	core = (struct is_core *)dev_get_drvdata(dev);
	vender = &core->vender;

	if (on) {
		/* runtime suspend callback can be called lately because of power relationship */
		while (test_bit(IS_ISCHAIN_POWER_ON, &device->state) && (retry > 0)) {
			mwarn("sensor is not yet power off", device);
			msleep(500);
			--retry;
		}
		if (!retry) {
			ret = -EBUSY;
			goto p_err;
		}

		/* 2. FIMC-IS local power enable */
#if defined(CONFIG_PM)
		mdbgd_ischain("pm_runtime_suspended = %d\n", device, pm_runtime_suspended(dev));
		rpm_ret = pm_runtime_get_sync(dev);
		if (rpm_ret < 0)
			merr("pm_runtime_get_sync() return error: %d", device, rpm_ret);
#else
		is_ischain_runtime_resume(dev);
		minfo("%s(%d) - is runtime resume complete\n", device, __func__, on);
#endif

		ret = is_ischain_runtime_resume_post(dev);
		if (ret)
			merr("is_runtime_suspend_post is fail(%d)", device, ret);

		set_bit(IS_ISCHAIN_LOADED, &device->state);
		set_bit(IS_ISCHAIN_POWER_ON, &device->state);
	} else {
		/* FIMC-IS local power down */
		ret = is_ischain_runtime_suspend_post(dev);
		if (ret)
			merr("is_runtime_suspend_post is fail(%d)", device, ret);

#if defined(CONFIG_PM)
		ret = pm_runtime_put_sync(dev);
		if (ret)
			merr("pm_runtime_put_sync is fail(%d)", device, ret);
#else
		ret = is_ischain_runtime_suspend(dev);
		if (ret)
			merr("is_runtime_suspend is fail(%d)", device, ret);
#endif

		clear_bit(IS_ISCHAIN_POWER_ON, &device->state);
	}

p_err:

	minfo("%s(%d):%d\n", device, __func__, on, ret);
	return ret;
}

static int is_ischain_s_sensor_size(struct is_device_ischain *device,
	struct is_frame *frame,
	u32 *lindex,
	u32 *hindex,
	u32 *indexes)
{
	int ret = 0;
	struct param_sensor_config *sensor_config;
	struct is_module_enum *module;
	u32 binning, bns_binning;
	u32 sensor_width, sensor_height;
	u32 bns_width, bns_height;
	u32 framerate;
	u32 ex_mode;
	u32 mono_mode = 0;

	FIMC_BUG(!device->sensor);

	ret = is_sensor_g_module(device->sensor, &module);
	if (ret) {
		merr("is_sensor_g_module is fail(%d)", device, ret);
		goto p_sensor_config;
	}

	if (!module->pdata) {
		merr("module pdata is NULL", device);
		goto p_sensor_config;
	}

	if (module->pdata->sensor_module_type == SENSOR_TYPE_MONO)
		mono_mode = 1;
	minfo("[ISC:D] mono sensor(%d)\n", device, mono_mode);

p_sensor_config:
	binning = is_sensor_g_bratio(device->sensor);
	sensor_width = is_sensor_g_width(device->sensor);
	sensor_height = is_sensor_g_height(device->sensor);
	bns_width = is_sensor_g_bns_width(device->sensor);
	bns_height = is_sensor_g_bns_height(device->sensor);
	framerate = is_sensor_g_framerate(device->sensor);
	ex_mode = is_sensor_g_ex_mode(device->sensor);

	minfo("[ISC:D] binning(%d):%d\n", device, binning);

	if (test_bit(IS_ISCHAIN_REPROCESSING, &device->state))
		bns_binning = 1000;
	else
		bns_binning = is_sensor_g_bns_ratio(device->sensor);

	sensor_config = is_itf_g_param(device, frame, PARAM_SENSOR_CONFIG);
	sensor_config->width = sensor_width;
	sensor_config->height = sensor_height;
	sensor_config->calibrated_width = module->pixel_width;
	sensor_config->calibrated_height = module->pixel_height;
	sensor_config->sensor_binning_ratio_x = binning;
	sensor_config->sensor_binning_ratio_y = binning;
	sensor_config->bns_binning_ratio_x = bns_binning;
	sensor_config->bns_binning_ratio_y = bns_binning;
	sensor_config->bns_margin_left = 0;
	sensor_config->bns_margin_top = 0;
	sensor_config->bns_output_width = bns_width;
	sensor_config->bns_output_height = bns_height;
	sensor_config->frametime = 10 * 1000 * 1000; /* max exposure time */
#ifdef FIXED_FPS_DEBUG
	sensor_config->min_target_fps = FIXED_MAX_FPS_VALUE;
	sensor_config->max_target_fps = FIXED_MAX_FPS_VALUE;
#else
	if (device->sensor->min_target_fps > 0)
		sensor_config->min_target_fps = device->sensor->min_target_fps;
	if (device->sensor->max_target_fps > 0)
		sensor_config->max_target_fps = device->sensor->max_target_fps;
#endif

	if (is_sensor_g_fast_mode(device->sensor) == 1)
		sensor_config->early_config_lock = 1;
	else
		sensor_config->early_config_lock = 0;

#ifdef USE_MONO_SENSOR
	sensor_config->mono_mode = mono_mode;
#endif

	*lindex |= LOWBIT_OF(PARAM_SENSOR_CONFIG);
	*hindex |= HIGHBIT_OF(PARAM_SENSOR_CONFIG);
	(*indexes)++;

	return ret;
}

static int is_ischain_s_path(struct is_device_ischain *device,
	u32 *lindex, u32 *hindex, u32 *indexes)
{
	int ret = 0;

	FIMC_BUG(!device);
	FIMC_BUG(!lindex);
	FIMC_BUG(!hindex);
	FIMC_BUG(!indexes);

#if defined(SOC_TDNR)
	if (test_bit(IS_ISCHAIN_REPROCESSING, &device->state))
		is_subdev_dnr_stop(device, NULL, lindex, hindex, indexes);
#endif

	return ret;
}

static void is_ischain_calc_plane_dva(u32 planes, dma_addr_t dva[], u32 pixelformat,
				u32 width, u32 height, u32 target_addr[])
{
	int i, j;

	switch (pixelformat) {
	case V4L2_PIX_FMT_NV21:
	case V4L2_PIX_FMT_NV12:
	case V4L2_PIX_FMT_NV16:
	case V4L2_PIX_FMT_NV61:
		for (i = 0; i < planes; i++) {
			j = i * 2;
			target_addr[j] = (typeof(*target_addr))dva[i];
			target_addr[j + 1] = target_addr[j] + (width * height);
		}
		break;
	case V4L2_PIX_FMT_YVU420M:
		for (i = 0; i < planes; i += 3) {
			target_addr[i] = (typeof(*target_addr))dva[i];
			target_addr[i + 1] = (typeof(*target_addr))dva[i + 2];
			target_addr[i + 2] = (typeof(*target_addr))dva[i + 1];
		}
		break;
	case V4L2_PIX_FMT_YUV420:
		for (i = 0; i < planes; i++) {
			j = i * 3;
			target_addr[j] = (typeof(*target_addr))dva[i];
			target_addr[j + 1] = target_addr[j] + (width * height);
			target_addr[j + 2] = target_addr[j + 1] + (width * height / 4);
		}
		break;
	case V4L2_PIX_FMT_YVU420: /* AYV12 spec: The width should be aligned by 16 pixel. */
		for (i = 0; i < planes; i++) {
			j = i * 3;
			target_addr[j] = (typeof(*target_addr))dva[i];
			target_addr[j + 2] = target_addr[j] + (ALIGN(width, 16) * height);
			target_addr[j + 1] = target_addr[j + 2] + (ALIGN(width / 2, 16) * height / 2);
		}
		break;
	case V4L2_PIX_FMT_YUV422P:
		for (i = 0; i < planes; i++) {
			j = i * 3;
			target_addr[j] = (typeof(*target_addr))dva[i];
			target_addr[j + 1] = target_addr[j] + (width * height);
			target_addr[j + 2] = target_addr[j + 1] + (width * height / 2);
		}
		break;
	case V4L2_PIX_FMT_NV12M_S10B:
	case V4L2_PIX_FMT_NV21M_S10B:
		for (i = 0; i < planes; i += 2) {
			j = i * 2;
			/* Y_ADDR, UV_ADDR, Y_2BIT_ADDR, UV_2BIT_ADDR */
			target_addr[j] = (typeof(*target_addr))dva[i];
			target_addr[j + 1] = (typeof(*target_addr))dva[i + 1];
			target_addr[j + 2] = target_addr[j] + NV12M_Y_SIZE(width, height);
			target_addr[j + 3] = target_addr[j + 1] + NV12M_CBCR_SIZE(width, height);
		}
		break;
	case V4L2_PIX_FMT_NV16M_S10B:
	case V4L2_PIX_FMT_NV61M_S10B:
		for (i = 0; i < planes; i += 2) {
			j = i * 2;
			/* Y_ADDR, UV_ADDR, Y_2BIT_ADDR, UV_2BIT_ADDR */
			target_addr[j] = (typeof(*target_addr))dva[i];
			target_addr[j + 1] = (typeof(*target_addr))dva[i + 1];
			target_addr[j + 2] = target_addr[j] + NV16M_Y_SIZE(width, height);
			target_addr[j + 3] = target_addr[j + 1] + NV16M_CBCR_SIZE(width, height);
		}
		break;
	case V4L2_PIX_FMT_RGB24:
		for (i = 0; i < planes; i++) {
			j = i * 3;
			target_addr[j + 2] = (typeof(*target_addr))dva[i];
			target_addr[j + 1] = target_addr[j + 2] + (width * height);
			target_addr[j] = target_addr[j + 1] + (width * height);
		}
		break;
	case V4L2_PIX_FMT_BGR24:
		for (i = 0; i < planes; i++) {
			j = i * 3;
			target_addr[j] = (typeof(*target_addr))dva[i];
			target_addr[j + 1] = target_addr[j] + (width * height);
			target_addr[j + 2] = target_addr[j + 1] + (width * height);
		}
		break;
	default:
		for (i = 0; i < planes; i++)
			target_addr[i] = (typeof(*target_addr))dva[i];
		break;
	}
}

static void is_ischain_calc_plane_kva(u32 planes, ulong kva[], u32 pixelformat,
				u32 width, u32 height, uint64_t target_addr[])
{
	int i;

	switch (pixelformat) {
	case V4L2_PIX_FMT_Y12:	/* Only for ME : kernel virtual addr*/
	case V4L2_PIX_FMT_YUV32:	/* Only for 32bit data : kernel virtual addr*/
	case V4L2_PIX_FMT_GREY:	/* Only for 1 plane, 1 byte unit custom data*/
		for (i = 0; i < planes; i++)
			target_addr[i] = kva[i];
		break;
	default:
		for (i = 0; i < planes; i++)
			target_addr[i] = kva[i];
		break;
	}
}

int is_ischain_buf_tag_input(struct is_device_ischain *device,
	struct is_subdev *subdev,
	struct is_frame *ldr_frame,
	u32 pixelformat,
	u32 width,
	u32 height,
	u32 target_addr[])
{
	int ret = 0;
	int i;
	unsigned long flags;
	struct is_framemgr *framemgr;
	struct is_frame *frame;

	framemgr = GET_SUBDEV_FRAMEMGR(subdev);
	FIMC_BUG(!framemgr);

	framemgr_e_barrier_irqs(framemgr, FMGR_IDX_24, flags);

	frame = peek_frame(framemgr, ldr_frame->state);
	if (frame) {
		is_ischain_calc_plane_dva(frame->planes, frame->dvaddr_buffer,
					pixelformat, width, height, target_addr);

		set_bit(subdev->id, &ldr_frame->out_flag);

		/* for draw digit */
		frame->width = width;
		frame->height = height;
	} else {
		for (i = 0; i < IS_MAX_PLANES; i++)
			target_addr[i] = 0;

		ret = -EINVAL;
	}

	framemgr_x_barrier_irqr(framemgr, FMGR_IDX_24, flags);

	return ret;
}

int is_ischain_buf_tag(struct is_device_ischain *device,
	struct is_subdev *subdev,
	struct is_frame *ldr_frame,
	u32 pixelformat,
	u32 width,
	u32 height,
	u32 target_addr[])
{
	int ret = 0;
	int i;
	unsigned long flags;
	struct is_framemgr *framemgr;
	struct is_frame *frame;
	struct camera2_node *capture;
	enum is_frame_state next_state = FS_PROCESS;

	framemgr = GET_SUBDEV_FRAMEMGR(subdev);
	FIMC_BUG(!framemgr);

	framemgr_e_barrier_irqs(framemgr, FMGR_IDX_24, flags);

	frame = peek_frame(framemgr, ldr_frame->state);
	if (frame) {
		struct camera2_stream	*stream;

		stream = frame->stream;
		if (!stream) {
			framemgr_x_barrier_irqr(framemgr, FMGR_IDX_24, flags);
			merr("frame->stream is NULL", device);
			BUG();
		}

		is_ischain_calc_plane_dva(frame->planes, frame->dvaddr_buffer,
					pixelformat, width, height, target_addr);

		capture = &ldr_frame->shot_ext->node_group.capture[subdev->cid];

		frame->fcount = ldr_frame->fcount;
		stream->findex = ldr_frame->index;
		stream->fcount = ldr_frame->fcount;

		if (ldr_frame->stripe_info.region_num
			&& ldr_frame->stripe_info.region_id < ldr_frame->stripe_info.region_num - 1)
			/* Still being stripe processed. */
			next_state = FS_STRIPE_PROCESS;

		if (likely(capture->vid == subdev->vid)) {
			stream->input_crop_region[0] = capture->input.cropRegion[0];
			stream->input_crop_region[1] = capture->input.cropRegion[1];
			stream->input_crop_region[2] = capture->input.cropRegion[2];
			stream->input_crop_region[3] = capture->input.cropRegion[3];
			stream->output_crop_region[0] = capture->output.cropRegion[0];
			stream->output_crop_region[1] = capture->output.cropRegion[1];
			stream->output_crop_region[2] = capture->output.cropRegion[2];
			stream->output_crop_region[3] = capture->output.cropRegion[3];
		} else {
			mserr("capture vid is changed(%d != %d)(%d)",
				subdev, subdev, subdev->vid,
				capture->vid, subdev->cid);
			stream->input_crop_region[0] = 0;
			stream->input_crop_region[1] = 0;
			stream->input_crop_region[2] = 0;
			stream->input_crop_region[3] = 0;
			stream->output_crop_region[0] = 0;
			stream->output_crop_region[1] = 0;
			stream->output_crop_region[2] = 0;
			stream->output_crop_region[3] = 0;
		}

		set_bit(subdev->id, &ldr_frame->out_flag);

		/* for draw digit */
		frame->width = width;
		frame->height = height;

		trans_frame(framemgr, frame, next_state);
	} else {
		for (i = 0; i < IS_MAX_PLANES; i++)
			target_addr[i] = 0;

		ret = -EINVAL;
	}

	framemgr_x_barrier_irqr(framemgr, FMGR_IDX_24, flags);

	return ret;
}

int is_ischain_buf_tag_64bit(struct is_device_ischain *device,
	struct is_subdev *subdev,
	struct is_frame *ldr_frame,
	u32 pixelformat,
	u32 width,
	u32 height,
	uint64_t target_addr[])
{
	int ret = 0;
	int i;
	unsigned long flags;
	struct is_framemgr *framemgr;
	struct is_frame *frame;

	framemgr = GET_SUBDEV_FRAMEMGR(subdev);
	BUG_ON(!framemgr);

	framemgr_e_barrier_irqs(framemgr, FMGR_IDX_24, flags);

	frame = peek_frame(framemgr, ldr_frame->state);
	if (frame) {
		struct camera2_stream	*stream;

		stream = frame->stream;
		if (!stream) {
			framemgr_x_barrier_irqr(framemgr, FMGR_IDX_24, flags);
			merr("frame->stream is NULL", device);
			BUG();
		}

		is_ischain_calc_plane_kva(frame->planes, frame->kvaddr_buffer,
					pixelformat, width, height, target_addr);

		frame->fcount = ldr_frame->fcount;
		stream->findex = ldr_frame->index;
		stream->fcount = ldr_frame->fcount;

		set_bit(subdev->id, &ldr_frame->out_flag);

		/* for draw digit */
		frame->width = width;
		frame->height = height;

		trans_frame(framemgr, frame, FS_PROCESS);
	} else {
		for (i = 0; i < IS_MAX_PLANES; i++)
			target_addr[i] = 0;

		ret = -EINVAL;
	}

	framemgr_x_barrier_irqr(framemgr, FMGR_IDX_24, flags);

	return ret;
}

static int is_ischain_chg_setfile(struct is_device_ischain *device)
{
	int ret = 0;
	u32 group = 0;

	FIMC_BUG(!device);

	group = is_itf_g_logical_group_id(device);

	ret = is_itf_process_stop(device, group);
	if (ret) {
		merr("is_itf_process_stop fail", device);
		goto p_err;
	}

	ret = is_itf_a_param(device, group);
	if (ret) {
		merr("is_itf_a_param is fail", device);
		ret = -EINVAL;
		goto p_err;
	}

	ret = is_itf_process_start(device, group);
	if (ret) {
		merr("is_itf_process_start fail", device);
		ret = -EINVAL;
		goto p_err;
	}

p_err:
	minfo("[ISC:D] %s(%d):%d\n", device, __func__,
		device->setfile & IS_SETFILE_MASK, ret);
	return ret;
}

#ifdef ENABLE_VRA
static int is_ischain_vra_bypass(struct is_device_ischain *device,
	struct is_frame *frame,
	bool bypass)
{
	int ret = 0;
	u32 lindex, hindex, indexes;
	struct param_control *control;
	struct is_group *group;
	struct is_subdev *leader, *subdev;

	FIMC_BUG(!device);

	subdev = &device->group_vra.leader;
	leader = subdev->leader;
	group = container_of(leader, struct is_group, leader);
	lindex = hindex = indexes = 0;

	if (!test_bit(IS_SUBDEV_START, &subdev->state)) {
		mserr("subdev is not start", device, subdev);
		ret = -EINVAL;
		goto p_err;
	}

	control = is_itf_g_param(device, frame, PARAM_FD_CONTROL);
	if (bypass)
		control->cmd = CONTROL_COMMAND_STOP;
	else
		control->cmd = CONTROL_COMMAND_START;
	control->bypass = CONTROL_BYPASS_DISABLE;
	lindex |= LOWBIT_OF(PARAM_FD_CONTROL);
	hindex |= HIGHBIT_OF(PARAM_FD_CONTROL);
	indexes++;

	ret = is_itf_s_param(device, frame, lindex, hindex, indexes);
	if (ret) {
		merr("is_itf_s_param is fail(%d)", device, ret);
		goto p_err;
	}

	if (bypass) {
		clear_bit(IS_SUBDEV_RUN, &subdev->state);
	} else {
		set_bit(IS_SUBDEV_RUN, &subdev->state);
	}

p_err:
	msrinfo("bypass : %d\n", device, subdev, frame, bypass);
	return ret;
}
#endif

int is_ischain_g_ddk_setfile_version(struct is_device_ischain *device,
	void *user_ptr)
{
	int ret = 0;
	int position;
	struct ddk_setfile_ver *version_info;

	version_info = vzalloc(sizeof(struct ddk_setfile_ver));
	if (!version_info) {
		merr("version_info is NULL", device);
		ret = -ENOMEM;
		goto p_err;
	}

	strncpy(version_info->ddk_version, get_binary_version(IS_BIN_LIBRARY, IS_BIN_LIB_HINT_DDK), 60);
	position = device->sensor->position;
	strncpy(version_info->setfile_version, get_binary_version(IS_BIN_SETFILE, position), 60);
	version_info->header1 = SETFILE_VERSION_INFO_HEADER1;
	version_info->header2 = SETFILE_VERSION_INFO_HEADER2;

	ret = copy_to_user(user_ptr, version_info, sizeof(struct ddk_setfile_ver));

	vfree(version_info);
p_err:
	return ret;
}

int is_ischain_g_capability(struct is_device_ischain *device,
	ulong user_ptr)
{
	int ret = 0;
#ifdef PRINT_CAPABILITY
	struct camera2_sm *capability;

	capability = vzalloc(sizeof(struct camera2_sm));
	if (!capability) {
		merr("capability is NULL", device);
		ret = -ENOMEM;
		goto p_err;
	}

	ret = is_itf_g_capability(device);
	if (ret) {
		merr("is_itf_g_capability is fail(%d)", device, ret);
		ret = -EINVAL;
		goto p_err;
	}

	ret = copy_to_user((void *)user_ptr, capability, sizeof(struct camera2_sm));

p_err:
	vfree(capability);
#endif

	return ret;
}

int is_ischain_probe(struct is_device_ischain *device,
	struct is_interface *interface,
	struct is_resourcemgr *resourcemgr,
	struct is_groupmgr *groupmgr,
	struct is_devicemgr *devicemgr,
	struct is_mem *mem,
	struct platform_device *pdev,
	u32 instance)
{
	int ret = 0;

	FIMC_BUG(!interface);
	FIMC_BUG(!mem);
	FIMC_BUG(!pdev);
	FIMC_BUG(!device);

	device->interface	= interface;
	device->mem		= mem;
	device->pdev		= pdev;
	device->pdata		= pdev->dev.platform_data;
	device->instance	= instance;
	device->groupmgr	= groupmgr;
	device->resourcemgr	= resourcemgr;
	device->devicemgr	= devicemgr;
	device->sensor		= NULL;
	device->margin_left	= 0;
	device->margin_right	= 0;
	device->margin_width	= 0;
	device->margin_top	= 0;
	device->margin_bottom	= 0;
	device->margin_height	= 0;
	device->setfile		= 0;
	device->is_region	= NULL;
	device->sensor_id	= SENSOR_NAME_NOTHING;
	device->apply_setfile_fcount = 0;
	device->partial_update = 0;

	atomic_set(&device->group_open_cnt, 0);
	atomic_set(&device->open_cnt, 0);
	atomic_set(&device->init_cnt, 0);

#ifdef ENABLE_BUFFER_HIDING
	is_pipe_probe(&device->pipe);
#endif

	is_group_probe(groupmgr, &device->group_paf, NULL, device,
		is_ischain_paf_shot,
		GROUP_SLOT_PAF, ENTRY_PAF, "PXS", &is_subdev_paf_ops);
	is_group_probe(groupmgr, &device->group_3aa, NULL, device,
		is_ischain_3aa_shot,
		GROUP_SLOT_3AA, ENTRY_3AA, "3XS", &is_subdev_3aa_ops);
	is_group_probe(groupmgr, &device->group_lme, NULL, device,
		is_ischain_lme_shot,
		GROUP_SLOT_LME, ENTRY_LME, "LME", &is_subdev_lme_ops);
	is_group_probe(groupmgr, &device->group_isp, NULL, device,
		is_ischain_isp_shot,
		GROUP_SLOT_ISP, ENTRY_ISP, "IXS", &is_subdev_isp_ops);
	is_group_probe(groupmgr, &device->group_mcs, NULL, device,
		is_ischain_mcs_shot,
		GROUP_SLOT_MCS, ENTRY_MCS, "MXS", &is_subdev_mcs_ops);
#if defined(SOC_VRA)
	is_group_probe(groupmgr, &device->group_vra, NULL, device,
		is_ischain_vra_shot,
		GROUP_SLOT_VRA, ENTRY_VRA, "VXS", &is_subdev_vra_ops);
#endif
	is_group_probe(groupmgr, &device->group_clh, NULL, device,
		is_ischain_clh_shot,
		GROUP_SLOT_CLH, ENTRY_CLH, "CLXS", &is_subdev_clh_ops);
	is_group_probe(groupmgr, &device->group_ypp, NULL, device,
		is_ischain_ypp_shot,
		GROUP_SLOT_YPP, ENTRY_YPP, "YPP", &is_subdev_ypp_ops);

	is_subdev_probe(&device->pdaf, instance, ENTRY_PDAF, "PDAF", NULL);
	is_subdev_probe(&device->pdst, instance, ENTRY_PDST, "PDST", NULL);
	is_subdev_probe(&device->txc, instance, ENTRY_3AC, "3XC", &is_subdev_3ac_ops);
	is_subdev_probe(&device->txp, instance, ENTRY_3AP, "3XP", &is_subdev_3ap_ops);
	is_subdev_probe(&device->txf, instance, ENTRY_3AF, "3XF", &is_subdev_3af_ops);
	is_subdev_probe(&device->txg, instance, ENTRY_3AG, "3XG", &is_subdev_3ag_ops);
	is_subdev_probe(&device->txo, instance, ENTRY_3AO, "3XO", &is_subdev_3ao_ops);
	is_subdev_probe(&device->txl, instance, ENTRY_3AL, "3XL", &is_subdev_3al_ops);

	is_subdev_probe(&device->lmes, instance, ENTRY_LMES, "LMES", &is_subdev_lmes_ops);
	is_subdev_probe(&device->lmec, instance, ENTRY_LMEC, "LMEC", &is_subdev_lmec_ops);
#if defined(SOC_ORBMCH)
	is_subdev_probe(&device->orbxc, instance, ENTRY_ORBXC, "MEX", &is_subdev_orbxc_ops);
#endif
	is_subdev_probe(&device->ixc, instance, ENTRY_IXC, "IXC", &is_subdev_ixc_ops);
	is_subdev_probe(&device->ixp, instance, ENTRY_IXP, "IXP", &is_subdev_ixp_ops);
#if defined(SOC_TNR_MERGER)
	is_subdev_probe(&device->ixt, instance, ENTRY_IXT, "IXT", &is_subdev_ixt_ops);
	is_subdev_probe(&device->ixg, instance, ENTRY_IXG, "IXG", &is_subdev_ixg_ops);
	is_subdev_probe(&device->ixv, instance, ENTRY_IXV, "IXV", &is_subdev_ixv_ops);
	is_subdev_probe(&device->ixw, instance, ENTRY_IXW, "IXW", &is_subdev_ixw_ops);
#endif
	is_subdev_probe(&device->mexc, instance, ENTRY_MEXC, "MEX", &is_subdev_mexc_ops);
	is_subdev_probe(&device->clhc, instance, ENTRY_CLHC, "CLXC", &is_subdev_clxc_ops);

	is_subdev_probe(&device->m0p, instance, ENTRY_M0P, "M0P", &is_subdev_mcsp_ops);
	is_subdev_probe(&device->m1p, instance, ENTRY_M1P, "M1P", &is_subdev_mcsp_ops);
	is_subdev_probe(&device->m2p, instance, ENTRY_M2P, "M2P", &is_subdev_mcsp_ops);
	is_subdev_probe(&device->m3p, instance, ENTRY_M3P, "M3P", &is_subdev_mcsp_ops);
	is_subdev_probe(&device->m4p, instance, ENTRY_M4P, "M4P", &is_subdev_mcsp_ops);
	is_subdev_probe(&device->m5p, instance, ENTRY_M5P, "M5P", &is_subdev_mcsp_ops);

	clear_bit(IS_ISCHAIN_OPEN, &device->state);
	clear_bit(IS_ISCHAIN_LOADED, &device->state);
	clear_bit(IS_ISCHAIN_POWER_ON, &device->state);
	clear_bit(IS_ISCHAIN_OPEN_STREAM, &device->state);
	clear_bit(IS_ISCHAIN_REPROCESSING, &device->state);
	clear_bit(IS_ISCHAIN_OFFLINE_REPROCESSING, &device->state);

	/* clear group open state */
	clear_bit(IS_GROUP_OPEN, &device->group_3aa.state);
	clear_bit(IS_GROUP_OPEN, &device->group_lme.state);
	clear_bit(IS_GROUP_OPEN, &device->group_isp.state);
	clear_bit(IS_GROUP_OPEN, &device->group_mcs.state);
	clear_bit(IS_GROUP_OPEN, &device->group_vra.state);
	clear_bit(IS_GROUP_OPEN, &device->group_clh.state);

	/* clear subdevice state */
	clear_bit(IS_SUBDEV_OPEN, &device->group_3aa.leader.state);
	clear_bit(IS_SUBDEV_OPEN, &device->group_lme.leader.state);
	clear_bit(IS_SUBDEV_OPEN, &device->group_isp.leader.state);
	clear_bit(IS_SUBDEV_OPEN, &device->group_mcs.leader.state);
	clear_bit(IS_SUBDEV_OPEN, &device->group_vra.leader.state);
	clear_bit(IS_SUBDEV_OPEN, &device->group_clh.leader.state);
	clear_bit(IS_SUBDEV_OPEN, &device->txc.state);
	clear_bit(IS_SUBDEV_OPEN, &device->txp.state);
	clear_bit(IS_SUBDEV_OPEN, &device->txf.state);
	clear_bit(IS_SUBDEV_OPEN, &device->txg.state);
	clear_bit(IS_SUBDEV_OPEN, &device->txo.state);
	clear_bit(IS_SUBDEV_OPEN, &device->txl.state);
	clear_bit(IS_SUBDEV_OPEN, &device->lmes.state);
	clear_bit(IS_SUBDEV_OPEN, &device->lmec.state);
	clear_bit(IS_SUBDEV_OPEN, &device->orbxc.state);
	clear_bit(IS_SUBDEV_OPEN, &device->ixc.state);
	clear_bit(IS_SUBDEV_OPEN, &device->ixp.state);
#if defined(SOC_TNR_MERGER)
	clear_bit(IS_SUBDEV_OPEN, &device->ixt.state);
	clear_bit(IS_SUBDEV_OPEN, &device->ixg.state);
	clear_bit(IS_SUBDEV_OPEN, &device->ixv.state);
	clear_bit(IS_SUBDEV_OPEN, &device->ixw.state);
#endif
	clear_bit(IS_SUBDEV_OPEN, &device->mexc.state);
	clear_bit(IS_SUBDEV_OPEN, &device->clhc.state);

	clear_bit(IS_SUBDEV_START, &device->group_3aa.leader.state);
	clear_bit(IS_SUBDEV_START, &device->group_lme.leader.state);
	clear_bit(IS_SUBDEV_START, &device->group_isp.leader.state);
	clear_bit(IS_SUBDEV_START, &device->group_mcs.leader.state);
	clear_bit(IS_SUBDEV_START, &device->group_vra.leader.state);
	clear_bit(IS_SUBDEV_START, &device->group_clh.leader.state);
	clear_bit(IS_SUBDEV_START, &device->txc.state);
	clear_bit(IS_SUBDEV_START, &device->txp.state);
	clear_bit(IS_SUBDEV_START, &device->txf.state);
	clear_bit(IS_SUBDEV_START, &device->txg.state);
	clear_bit(IS_SUBDEV_START, &device->txo.state);
	clear_bit(IS_SUBDEV_START, &device->txl.state);
	clear_bit(IS_SUBDEV_START, &device->lmes.state);
	clear_bit(IS_SUBDEV_START, &device->lmec.state);
	clear_bit(IS_SUBDEV_START, &device->orbxc.state);
	clear_bit(IS_SUBDEV_START, &device->ixc.state);
	clear_bit(IS_SUBDEV_START, &device->ixp.state);
#if defined(SOC_TNR_MERGER)
	clear_bit(IS_SUBDEV_START, &device->ixt.state);
	clear_bit(IS_SUBDEV_START, &device->ixg.state);
	clear_bit(IS_SUBDEV_START, &device->ixv.state);
	clear_bit(IS_SUBDEV_START, &device->ixw.state);
#endif
	clear_bit(IS_SUBDEV_START, &device->mexc.state);
	clear_bit(IS_SUBDEV_START, &device->clhc.state);

	return ret;
}

static int is_ischain_open(struct is_device_ischain *device)
{
	struct is_minfo *minfo = NULL;
	u32 offset_region = 0;
	int ret = 0;
#ifdef SOC_VRA
	bool has_vra_ch1_only = false;
#endif

	FIMC_BUG(!device);
	FIMC_BUG(!device->groupmgr);
	FIMC_BUG(!device->resourcemgr);

	device->minfo = &device->resourcemgr->minfo;
	minfo = device->minfo;

	clear_bit(IS_ISCHAIN_INITING, &device->state);
	clear_bit(IS_ISCHAIN_INIT, &device->state);
	clear_bit(IS_ISCHAIN_START, &device->state);
	clear_bit(IS_ISCHAIN_OPEN_STREAM, &device->state);
	clear_bit(IS_ISCHAIN_REPROCESSING, &device->state);
	clear_bit(IS_ISCHAIN_OFFLINE_REPROCESSING, &device->state);

	/* 2. Init variables */
#if !defined(FAST_FDAE)
	memset(&device->fdUd, 0, sizeof(struct camera2_fd_uctl));
#endif
#ifdef ENABLE_SENSOR_DRIVER
	memset(&device->peri_ctls, 0, sizeof(struct camera2_uctl)*SENSOR_MAX_CTL);
#endif

	/* initial state, it's real apply to setting when opening */
	atomic_set(&device->init_cnt, 0);
	device->margin_left	= 0;
	device->margin_right	= 0;
	device->margin_width	= 0;
	device->margin_top	= 0;
	device->margin_bottom	= 0;
	device->margin_height	= 0;
	device->sensor		= NULL;
	device->module		= 0;
	device->sensor_id	= SENSOR_NAME_NOTHING;
	device->apply_setfile_fcount = 0;
	device->partial_update = 0;
	device->llc_mode = true;

	offset_region = device->instance * PARAM_REGION_SIZE;
	device->is_region	= (struct is_region *)(minfo->kvaddr_region + offset_region);

	device->kvaddr_shared	= device->is_region->shared[0];
	device->dvaddr_shared	= minfo->dvaddr +
				(u32)((ulong)&device->is_region->shared[0] - minfo->kvaddr);

	spin_lock_init(&device->is_region->fd_info_slock);

#ifdef SOC_VRA
	ret = is_hw_g_ctrl(NULL, 0, HW_G_CTRL_HAS_VRA_CH1_ONLY, (void *)&has_vra_ch1_only);
	if(!has_vra_ch1_only)
		is_subdev_open(&device->group_vra.leader, NULL, (void *)&init_vra_param.control,
			device->instance);
#endif

	device->dbuf_q = is_init_dbuf_q();
	if (IS_ERR_OR_NULL(device->dbuf_q)) {
		merr("is_resource_get is fail", device);
		goto p_err;
	}

	ret = is_devicemgr_open(device->devicemgr, (void *)device, IS_DEVICE_ISCHAIN);
	if (ret) {
		err("is_devicemgr_open is fail(%d)", ret);
		goto p_err;
	}

	/* for mediaserver force close */
	ret = is_resource_get(device->resourcemgr, RESOURCE_TYPE_ISCHAIN);
	if (ret) {
		merr("is_resource_get is fail", device);
		goto p_err;
	}

p_err:
	minfo("[ISC:D] %s():%d\n", device, __func__, ret);
	return ret;
}

int is_ischain_open_wrap(struct is_device_ischain *device, bool EOS)
{
	int ret = 0;

	FIMC_BUG(!device);

	if (test_bit(IS_ISCHAIN_CLOSING, &device->state)) {
		merr("open is invalid on closing", device);
		ret = -EPERM;
		goto p_err;
	}

	if (test_bit(IS_ISCHAIN_OPEN, &device->state)) {
		merr("already open", device);
		ret = -EMFILE;
		goto p_err;
	}

	if (atomic_read(&device->open_cnt) > ENTRY_END) {
		merr("open count is invalid(%d)", device, atomic_read(&device->open_cnt));
		ret = -EMFILE;
		goto p_err;
	}

	if (EOS) {
		ret = is_ischain_open(device);
		if (ret) {
			merr("is_chain_open is fail(%d)", device, ret);
			goto p_err;
		}

		clear_bit(IS_ISCHAIN_OPENING, &device->state);
		set_bit(IS_ISCHAIN_OPEN, &device->state);
	} else {
		atomic_inc(&device->open_cnt);
		set_bit(IS_ISCHAIN_OPENING, &device->state);
	}

p_err:
	return ret;
}

static int is_ischain_close(struct is_device_ischain *device)
{
	int ret = 0;
#ifdef SOC_VRA
	bool has_vra_ch1_only = false;
#endif

	FIMC_BUG(!device);

	if (!test_bit(IS_ISCHAIN_OPEN, &device->state)) {
		mwarn("this chain has not been opened", device);
		goto p_err;
	}

	/* subdev close */
#ifdef SOC_DNR
	is_subdev_close(&device->dnr);
#endif
#ifdef SOC_VRA
	is_hw_g_ctrl(NULL, 0, HW_G_CTRL_HAS_VRA_CH1_ONLY, (void *)&has_vra_ch1_only);
	if(!has_vra_ch1_only)
		is_subdev_close(&device->group_vra.leader);
#endif

	ret = is_itf_close(device);
	if (ret)
		merr("is_itf_close is fail", device);

	is_deinit_dbuf_q(device->dbuf_q);

	/* for mediaserver force close */
	ret = is_resource_put(device->resourcemgr, RESOURCE_TYPE_ISCHAIN);
	if (ret)
		merr("is_resource_put is fail", device);

	ret = is_devicemgr_close(device->devicemgr, (void *)device, IS_DEVICE_ISCHAIN);
	if (ret) {
		err("is_devicemgr_close is fail(%d)", ret);
		goto p_err;
	}

	atomic_set(&device->open_cnt, 0);
	clear_bit(IS_ISCHAIN_OPEN_STREAM, &device->state);

	minfo("[ISC:D] %s():%d\n", device, __func__, ret);

p_err:
	return ret;
}

int is_ischain_close_wrap(struct is_device_ischain *device)
{
	int ret = 0;

	FIMC_BUG(!device);

	if (test_bit(IS_ISCHAIN_OPENING, &device->state)) {
		mwarn("close on opening", device);
		clear_bit(IS_ISCHAIN_OPENING, &device->state);
	}

	if (!atomic_read(&device->open_cnt)) {
		merr("open count is invalid(%d)", device, atomic_read(&device->open_cnt));
		ret = -ENOENT;
		goto p_err;
	}

	atomic_dec(&device->open_cnt);
	set_bit(IS_ISCHAIN_CLOSING, &device->state);

	if (!atomic_read(&device->open_cnt)) {
		ret = is_ischain_close(device);
		if (ret) {
			merr("is_chain_close is fail(%d)", device, ret);
			goto p_err;
		}

		clear_bit(IS_ISCHAIN_CLOSING, &device->state);
		clear_bit(IS_ISCHAIN_OPEN, &device->state);
	}

p_err:
	return ret;
}

static int is_ischain_init(struct is_device_ischain *device,
	u32 module_id)
{
	int ret = 0;
	struct is_core *core;
	struct is_module_enum *module;
	struct is_device_sensor *sensor;
	struct is_path_info *path;
	struct is_vender *vender;
	u32 flag;

	FIMC_BUG(!device);
	FIMC_BUG(!device->sensor);

	mdbgd_ischain("%s(module : %d)\n", device, __func__, module_id);

	sensor = device->sensor;
	path = &device->path;
	core = (struct is_core *)platform_get_drvdata(device->pdev);
	vender = &core->vender;

	if (test_bit(IS_ISCHAIN_INIT, &device->state)) {
		minfo("stream is already initialized", device);
		goto p_err;
	}

	if (test_bit(IS_ISCHAIN_OFFLINE_REPROCESSING, &device->state)) {
		module = device->module_enum;
		goto skip_get_module;
	}

	if (!test_bit(IS_SENSOR_S_INPUT, &sensor->state)) {
		merr("I2C gpio is not yet set", device);
		ret = -EINVAL;
		goto p_err;
	}

	ret = is_sensor_g_module(sensor, &module);
	if (ret) {
		merr("is_sensor_g_module is fail(%d)", device, ret);
		goto p_err;
	}

skip_get_module:
	if (module->sensor_id != module_id) {
		merr("module id is invalid(%d != %d)", device, module->sensor_id, module_id);
		ret = -EINVAL;
		goto p_err;
	}

	if (!test_bit(IS_ISCHAIN_REPROCESSING, &device->state)) {
		/*
		 * Initiate cal address. If this value is 0, F/W does not load cal data.
		 * So vender must set this cal_address to let F/W load cal data.
		 */
		module->ext.sensor_con.cal_address = 0;
		ret = is_vender_cal_load(vender, module);
		if (ret) {
			merr("is_vender_cal_load is fail(%d)", device, ret);
			goto p_err;
		}
	}

	ret = is_vender_setfile_sel(vender, module->setfile_name, module->position);
	if (ret) {
		merr("is_vender_setfile_sel is fail(%d)", device, ret);
		goto p_err;
	}

	ret = is_itf_enum(device);
	if (ret) {
		merr("is_itf_enum is fail(%d)", device, ret);
		goto p_err;
	}

	flag = test_bit(IS_ISCHAIN_REPROCESSING, &device->state) ? 1 : 0;

	flag |= (sensor->pdata->scenario == SENSOR_SCENARIO_STANDBY) ? (0x1 << 16) : (0x0 << 16);

	ret = is_itf_open(device, module_id, flag, path, &module->ext);
	if (ret) {
		merr("open fail", device);
		goto p_err;
	}

	ret = is_itf_setfile(device, vender);
	if (ret) {
		merr("setfile fail", device);
		goto p_err;
	}

	device->module = module_id;
	clear_bit(IS_ISCHAIN_INITING, &device->state);
	set_bit(IS_ISCHAIN_INIT, &device->state);

p_err:
	return ret;
}

static int is_ischain_init_wrap(struct is_device_ischain *device,
	u32 stream_type,
	u32 module_id)
{
	int ret = 0;
	u32 sindex;
	struct is_core *core;
	struct is_device_sensor *sensor;
	struct is_module_enum *module;
	struct is_groupmgr *groupmgr;
	struct is_vender_specific *priv;
	u32 sensor_id;

	FIMC_BUG(!device);
	FIMC_BUG(!device->groupmgr);

	if (!test_bit(IS_ISCHAIN_OPEN, &device->state)) {
		merr("NOT yet open", device);
		ret = -EMFILE;
		goto p_err;
	}

	if (test_bit(IS_ISCHAIN_START, &device->state)) {
		merr("already start", device);
		ret = -EINVAL;
		goto p_err;
	}

	if (atomic_read(&device->init_cnt) >= atomic_read(&device->group_open_cnt)) {
		merr("init count value(%d) is invalid", device, atomic_read(&device->init_cnt));
		ret = -EINVAL;
		goto p_err;
	}

	groupmgr = device->groupmgr;
	core = container_of(groupmgr, struct is_core, groupmgr);
	atomic_inc(&device->init_cnt);
	set_bit(IS_ISCHAIN_INITING, &device->state);
	mdbgd_ischain("%s(%d, %d)\n", device, __func__,
		atomic_read(&device->init_cnt), atomic_read(&device->group_open_cnt));

	if (atomic_read(&device->init_cnt) == atomic_read(&device->group_open_cnt)) {
		if (test_bit(IS_GROUP_OPEN, &device->group_3aa.state) &&
			!test_bit(IS_GROUP_INIT, &device->group_3aa.state)) {
			merr("invalid 3aa group state", device);
			ret = -EINVAL;
			goto p_err;
		}

		if (test_bit(IS_GROUP_OPEN, &device->group_lme.state) &&
			!test_bit(IS_GROUP_INIT, &device->group_lme.state)) {
			merr("invalid lme group state", device);
			ret = -EINVAL;
			goto p_err;
		}

		if (test_bit(IS_GROUP_OPEN, &device->group_isp.state) &&
			!test_bit(IS_GROUP_INIT, &device->group_isp.state)) {
			merr("invalid isp group state", device);
			ret = -EINVAL;
			goto p_err;
		}

		if (test_bit(IS_GROUP_OPEN, &device->group_vra.state) &&
			!test_bit(IS_GROUP_INIT, &device->group_vra.state)) {
			merr("invalid vra group state", device);
			ret = -EINVAL;
			goto p_err;
		}

		if (test_bit(IS_GROUP_OPEN, &device->group_clh.state) &&
			!test_bit(IS_GROUP_INIT, &device->group_clh.state)) {
			merr("invalid clh group state", device);
			ret = -EINVAL;
			goto p_err;
		}

		if (stream_type == IS_OFFLINE_REPROCESSING_STREAM)
			goto skip_map_sensor;

		for (sindex = 0; sindex < IS_SENSOR_COUNT; ++sindex) {
			sensor = &core->sensor[sindex];

			if (!test_bit(IS_SENSOR_OPEN, &sensor->state))
				continue;

			if (!test_bit(IS_SENSOR_S_INPUT, &sensor->state))
				continue;

			ret = is_sensor_g_module(sensor, &module);
			if (ret) {
				merr("is_sensor_g_module is fail(%d)", device, ret);
				goto p_err;
			}

			priv = core->vender.private_data;

			switch (module_id) {
			case SENSOR_POSITION_REAR:
				sensor_id = priv->rear_sensor_id;
				break;
			case SENSOR_POSITION_FRONT:
				sensor_id = priv->front_sensor_id;
				break;
			case SENSOR_POSITION_REAR2:
				sensor_id = priv->rear2_sensor_id;
				break;
			case SENSOR_POSITION_FRONT2:
				sensor_id = priv->front2_sensor_id;
				break;
			case SENSOR_POSITION_REAR3:
				sensor_id = priv->rear3_sensor_id;
				break;
			case SENSOR_POSITION_REAR4:
				sensor_id = priv->rear4_sensor_id;
				break;
			case SENSOR_POSITION_REAR_TOF:
				sensor_id = priv->rear_tof_sensor_id;
				break;
			case SENSOR_POSITION_FRONT_TOF:
				sensor_id = priv->front_tof_sensor_id;
				break;
#if IS_ENABLED(CONFIG_SECURE_CAMERA_USE)
			case SENSOR_POSITION_SECURE:
				sensor_id = priv->secure_sensor_id;
				break;
#endif
			default:
				merr("invalid module position(%d)", device, module->position);
				ret = -EINVAL;
				goto p_err;
			}

			if (module->sensor_id == sensor_id
			        && module->position == module_id) {
				device->sensor = sensor;
				device->sensor_id = sensor_id;
				module_id = sensor_id;
				minfo("%s: sensor_id=%d\n", device,
							__func__, sensor_id);
				break;
			}
		}

		if (sindex >= IS_SENSOR_COUNT) {
			merr("moduel id(%d) is invalid", device, module_id);
			ret = -EINVAL;
			goto p_err;
		}

		if (!sensor || !sensor->pdata) {
			merr("sensor is NULL", device);
			ret = -EINVAL;
			goto p_err;
		}

skip_map_sensor:
		if (stream_type) {
			set_bit(IS_ISCHAIN_REPROCESSING, &device->state);
			if (stream_type == IS_OFFLINE_REPROCESSING_STREAM) {
				ret = is_sensor_map_sensor_module(device, module_id, &sensor_id);
				if (ret) {
					merr("fail to map(%d)", device, ret);
					ret = -EINVAL;
					goto p_err;
				}
				module_id = sensor_id;
				set_bit(IS_ISCHAIN_OFFLINE_REPROCESSING, &device->state);
			}
		} else {
			if (sensor->instance != device->instance) {
				merr("instance is mismatched (!= %d)[SS%d]", device, sensor->instance,
					sensor->device_id);
				ret = -EINVAL;
				goto p_err;
			}

			clear_bit(IS_ISCHAIN_REPROCESSING, &device->state);
			clear_bit(IS_ISCHAIN_OFFLINE_REPROCESSING, &device->state);
			sensor->ischain = device;
		}

		ret = is_groupmgr_init(device->groupmgr, device);
		if (ret) {
			merr("is_groupmgr_init is fail(%d)", device, ret);
			goto p_err;
		}

		/* register sensor(DDK, RTA)/preporcessor interface*/
		if (!test_bit(IS_SENSOR_ITF_REGISTER, &sensor->state) &&
			!test_bit(IS_ISCHAIN_OFFLINE_REPROCESSING, &device->state)) {
			ret = is_sensor_register_itf(sensor);
			if (ret) {
				merr("is_sensor_register_itf fail(%d)", device, ret);
				goto p_err;
			}

			set_bit(IS_SENSOR_ITF_REGISTER, &sensor->state);
		}

		ret = is_ischain_init(device, module_id);
		if (ret) {
			merr("is_ischain_init is fail(%d)", device, ret);
			goto p_err;
		}

		atomic_set(&device->init_cnt, 0);

		ret = is_devicemgr_binding(device->devicemgr, sensor, device, IS_DEVICE_ISCHAIN);
		if (ret) {
			merr("is_devicemgr_binding is fail", device);
			goto p_err;
		}
	}

p_err:
	return ret;
}

static void is_fastctl_manager_init(struct is_device_ischain *device)
{
	int i;
	unsigned long flags;
	struct fast_control_mgr *fastctlmgr;
	struct is_fast_ctl *fast_ctl;

	fastctlmgr = &device->fastctlmgr;

	spin_lock_init(&fastctlmgr->slock);
	fastctlmgr->fast_capture_count = 0;

	spin_lock_irqsave(&fastctlmgr->slock, flags);

	for (i = 0; i < IS_FAST_CTL_STATE; i++) {
		fastctlmgr->queued_count[i] = 0;
		INIT_LIST_HEAD(&fastctlmgr->queued_list[i]);
	}

	for (i = 0; i < MAX_NUM_FAST_CTL; i++) {
		fast_ctl = &fastctlmgr->fast_ctl[i];
		fast_ctl->state = IS_FAST_CTL_FREE;
		list_add_tail(&fast_ctl->list, &fastctlmgr->queued_list[IS_FAST_CTL_FREE]);
		fastctlmgr->queued_count[IS_FAST_CTL_FREE]++;
	}

	spin_unlock_irqrestore(&fastctlmgr->slock, flags);
}

static int is_ischain_start(struct is_device_ischain *device)
{
	int ret = 0;
	u32 lindex = 0;
	u32 hindex = 0;
	u32 indexes = 0;

	FIMC_BUG(!device);
	FIMC_BUG(!device->sensor);

	ret = is_hw_ischain_cfg((void *)device);
	if (ret) {
		merr("hw init fail", device);
		goto p_err;
	}

	if (test_bit(IS_ISCHAIN_OFFLINE_REPROCESSING, &device->state))
		indexes++;
	else
		ret = is_ischain_s_sensor_size(device, NULL, &lindex, &hindex, &indexes);
	if (ret) {
		merr("is_ischain_s_sensor_size is fail(%d)", device, ret);
		goto p_err;
	}

#if !defined(FAST_FDAE)
	/* previous fd infomation should be clear */
	memset(&device->fdUd, 0x0, sizeof(struct camera2_fd_uctl));
#endif

#ifdef ENABLE_ULTRA_FAST_SHOT
	memset(&device->is_region->fast_ctl, 0x0, sizeof(struct is_fast_control));
#endif
	is_fastctl_manager_init(device);

	if (!device->partial_update) {
		/* NI information clear */
		memset(&device->cur_noise_idx, 0xFFFFFFFF, sizeof(device->cur_noise_idx));
		memset(&device->next_noise_idx, 0xFFFFFFFF, sizeof(device->next_noise_idx));
	}

	ret = is_itf_sensor_mode(device);
	if (ret) {
		merr("is_itf_sensor_mode is fail(%d)", device, ret);
		goto p_err;
	}

	if (device->sensor->scene_mode >= AA_SCENE_MODE_DISABLED)
		device->is_region->parameter.taa.vdma1_input.scene_mode = device->sensor->scene_mode;

	ret = is_ischain_s_path(device, &lindex, &hindex, &indexes);
	if (ret) {
		merr("is_ischain_s_path is fail(%d)", device, ret);
		goto p_err;
	}

	lindex = 0xFFFFFFFF;
	hindex = 0xFFFFFFFF;
	indexes = 64;

	ret = is_itf_s_param(device , NULL, lindex, hindex, indexes);
	if (ret) {
		merr("is_itf_s_param is fail(%d)", device, ret);
		goto p_err;
	}

	ret = is_itf_f_param(device);
	if (ret) {
		merr("is_itf_f_param is fail(%d)", device, ret);
		goto p_err;
	}

	ret = is_itf_init_process_start(device);
	if (ret) {
		merr("is_itf_init_process_start is fail(%d)", device, ret);
		goto p_err;
	}

#ifdef ENABLE_DVFS
	ret = is_dvfs_sel_table(device->resourcemgr);
	if (ret) {
		merr("is_dvfs_sel_table is fail(%d)", device, ret);
		goto p_err;
	}
#endif

	set_bit(IS_ISCHAIN_START, &device->state);

p_err:
	minfo("[ISC:D] %s(%d):%d\n", device, __func__,
		device->setfile & IS_SETFILE_MASK, ret);

	return ret;
}

int is_ischain_start_wrap(struct is_device_ischain *device,
	struct is_group *group)
{
	int ret = 0;
	struct is_group *leader;

	if (!test_bit(IS_ISCHAIN_INIT, &device->state)) {
		merr("device is not yet init", device);
		ret = -EINVAL;
		goto p_err;
	}

	leader = device->groupmgr->leader[device->instance];
	if (leader != group)
		goto p_err;

	if (test_bit(IS_ISCHAIN_START, &device->state)) {
		merr("already start", device);
		ret = -EINVAL;
		goto p_err;
	}

	if (CHECK_PARTIAL_PARAM_UPDATE(group->id, device->sensor)) {
			device->partial_update = 1;
			info("remosaic mode_change parital up\n");
	} else {
		device->partial_update = 0;
	}

	/* param region init with default value */
	is_itf_param_init(device->is_region, device->partial_update);

	ret = is_groupmgr_start(device->groupmgr, device);
	if (ret) {
		merr("is_groupmgr_start is fail(%d)", device, ret);
		goto p_err;
	}

	ret = is_ischain_start(device);
	if (ret) {
		merr("is_chain_start is fail(%d)", device, ret);
		goto p_err;
	}

p_err:
	device->partial_update = 0;
	return ret;
}

static int is_ischain_stop(struct is_device_ischain *device)
{
	int ret = 0;

	return ret;
}

int is_ischain_stop_wrap(struct is_device_ischain *device,
	struct is_group *group)
{
	int ret = 0;
	struct is_group *leader;

	leader = device->groupmgr->leader[device->instance];
	if (leader != group)
		goto p_err;

	if (!test_bit(IS_ISCHAIN_START, &device->state)) {
		mwarn("already stop", device);
		goto p_err;
	}

	ret = is_groupmgr_stop(device->groupmgr, device);
	if (ret) {
		merr("is_groupmgr_stop is fail(%d)", device, ret);
		goto p_err;
	}

	ret = is_ischain_stop(device);
	if (ret) {
		merr("is_ischain_stop is fail(%d)", device, ret);
		goto p_err;
	}

	clear_bit(IS_ISCHAIN_START, &device->state);

p_err:
	return ret;
}

int is_ischain_paf_open(struct is_device_ischain *device,
	struct is_video_ctx *vctx)
{
	int ret = 0;
	int ret_err = 0;
	u32 group_id;
	struct is_groupmgr *groupmgr;
	struct is_group *group;

	FIMC_BUG(!device);
	FIMC_BUG(!vctx);
	FIMC_BUG(!GET_VIDEO(vctx));

	groupmgr = device->groupmgr;
	group = &device->group_paf;
	group_id = GROUP_ID_PAF0 + GET_PAFXS_ID(GET_VIDEO(vctx));

	ret = is_group_open(groupmgr,
		group,
		group_id,
		vctx);
	if (ret) {
		merr("is_group_open is fail(%d)", device, ret);
		goto err_group_open;
	}

	ret = is_ischain_open_wrap(device, false);
	if (ret) {
		merr("is_ischain_open_wrap is fail(%d)", device, ret);
		goto err_ischain_open;
	}

	atomic_inc(&device->group_open_cnt);

	return 0;

err_ischain_open:
	ret_err = is_group_close(groupmgr, group);
	if (ret_err)
		merr("is_group_close is fail(%d)", device, ret_err);
err_group_open:
	return ret;
}

int is_ischain_paf_close(struct is_device_ischain *device,
	struct is_video_ctx *vctx)
{
	int ret = 0;
	struct is_groupmgr *groupmgr;
	struct is_group *group;
	struct is_queue *queue;

	FIMC_BUG(!device);

	groupmgr = device->groupmgr;
	group = &device->group_paf;
	queue = GET_QUEUE(vctx);

	/* for mediaserver dead */
	if (test_bit(IS_GROUP_START, &group->state)) {
		mgwarn("sudden group close", device, group);
		if (!test_bit(IS_ISCHAIN_REPROCESSING, &device->state))
			is_itf_sudden_stop_wrap(device, device->instance, group);
		set_bit(IS_GROUP_REQUEST_FSTOP, &group->state);
		if (test_bit(IS_HAL_DEBUG_SUDDEN_DEAD_DETECT, &sysfs_debug.hal_debug_mode)) {
			msleep(sysfs_debug.hal_debug_delay);
			panic("HAL sudden group close #1");
		}
	}

	if (group->head && test_bit(IS_GROUP_START, &group->head->state)) {
		mgwarn("sudden group close", device, group);
		if (!test_bit(IS_ISCHAIN_REPROCESSING, &device->state))
			is_itf_sudden_stop_wrap(device, device->instance, group);
		set_bit(IS_GROUP_REQUEST_FSTOP, &group->state);
		if (test_bit(IS_HAL_DEBUG_SUDDEN_DEAD_DETECT, &sysfs_debug.hal_debug_mode)) {
			msleep(sysfs_debug.hal_debug_delay);
			panic("HAL sudden group close #2");
		}
	}

	ret = is_ischain_paf_stop(device, queue);
	if (ret)
		merr("is_ischain_paf_rdma_stop is fail", device);

	ret = is_ischain_close_wrap(device);
	if (ret)
		merr("is_ischain_close_wrap is fail(%d)", device, ret);

	ret = is_group_close(groupmgr, group);
	if (ret)
		merr("is_group_close is fail", device);

	atomic_dec(&device->group_open_cnt);

	return ret;
}

int is_ischain_paf_s_input(struct is_device_ischain *device,
	u32 stream_type,
	u32 position,
	u32 video_id,
	u32 input_type,
	u32 stream_leader)
{
	int ret = 0;
	struct is_group *group;
	struct is_groupmgr *groupmgr;

	FIMC_BUG(!device);
	FIMC_BUG(!device->groupmgr);

	groupmgr = device->groupmgr;
	group = &device->group_paf;

	mdbgd_ischain("%s()\n", device, __func__);

	ret = is_group_init(groupmgr, group, input_type, video_id, stream_leader);
	if (ret) {
		merr("is_group_init is fail(%d)", device, ret);
		goto p_err;
	}

	ret = is_ischain_init_wrap(device, stream_type, position);
	if (ret) {
		merr("is_ischain_init_wrap is fail(%d)", device, ret);
		goto p_err;
	}

p_err:
	return ret;
}

static int is_ischain_paf_start(void *qdevice,
	struct is_queue *queue)
{
	int ret = 0;
	struct is_device_ischain *device = qdevice;
	struct is_groupmgr *groupmgr;
	struct is_group *group;

	FIMC_BUG(!device);

	groupmgr = device->groupmgr;
	group = &device->group_paf;

	ret = is_group_start(groupmgr, group);
	if (ret) {
		merr("is_group_start is fail(%d)", device, ret);
		goto p_err;
	}

	ret = is_ischain_start_wrap(device, group);
	if (ret) {
		merr("is_ischain_start_wrap is fail(%d)", device, ret);
		goto p_err;
	}

p_err:
	return ret;
}

static int is_ischain_paf_stop(void *qdevice,
	struct is_queue *queue)
{
	int ret = 0;
	struct is_device_ischain *device = qdevice;
	struct is_groupmgr *groupmgr;
	struct is_group *group;

	FIMC_BUG(!device);

	groupmgr = device->groupmgr;
	group = &device->group_paf;

	if (!test_bit(IS_GROUP_INIT, &group->state))
		goto p_err;

	ret = is_group_stop(groupmgr, group);
	if (ret) {
		if (ret == -EPERM)
			ret = 0;
		else
			merr("is_group_stop is fail(%d)", device, ret);
		goto p_err;
	}

	ret = is_ischain_stop_wrap(device, group);
	if (ret) {
		merr("is_ischain_stop_wrap is fail(%d)", device, ret);
		goto p_err;
	}

p_err:
	mginfo("%s(%d):%d\n", device, group,  __func__, atomic_read(&group->scount), ret);
	return ret;
}

static int is_ischain_paf_reqbufs(void *qdevice,
	struct is_queue *queue, u32 count)
{
	return 0;
}

static int is_ischain_paf_s_format(void *qdevice,
	struct is_queue *queue)
{
	int ret = 0;
	struct is_device_ischain *device = qdevice;
	struct is_subdev *leader;

	FIMC_BUG(!device);
	FIMC_BUG(!queue);

	leader = &device->group_paf.leader;

	leader->input.width = queue->framecfg.width;
	leader->input.height = queue->framecfg.height;

	leader->input.crop.x = 0;
	leader->input.crop.y = 0;
	leader->input.crop.w = leader->input.width;
	leader->input.crop.h = leader->input.height;

	return ret;
}

int is_ischain_paf_buffer_queue(struct is_device_ischain *device,
	struct is_queue *queue,
	u32 index)
{
	int ret = 0;
	struct is_groupmgr *groupmgr;
	struct is_group *group;

	FIMC_BUG(!device);
	FIMC_BUG(!test_bit(IS_ISCHAIN_OPEN, &device->state));

	mdbgs_ischain(4, "%s\n", device, __func__);

	groupmgr = device->groupmgr;
	group = &device->group_paf;

	ret = is_group_buffer_queue(groupmgr, group, queue, index);
	if (ret)
		merr("is_group_buffer_queue is fail(%d)", device, ret);

	return ret;
}

int is_ischain_paf_buffer_finish(struct is_device_ischain *device,
	u32 index)
{
	int ret = 0;
	struct is_groupmgr *groupmgr;
	struct is_group *group;

	FIMC_BUG(!device);

	mdbgs_ischain(4, "%s\n", device, __func__);

	groupmgr = device->groupmgr;
	group = &device->group_paf;

	ret = is_group_buffer_finish(groupmgr, group, index);
	if (ret)
		merr("is_group_buffer_finish is fail(%d)", device, ret);

	return ret;
}

const struct is_queue_ops is_ischain_paf_ops = {
	.start_streaming	= is_ischain_paf_start,
	.stop_streaming		= is_ischain_paf_stop,
	.s_format		= is_ischain_paf_s_format,
	.request_bufs		= is_ischain_paf_reqbufs
};

int is_ischain_3aa_open(struct is_device_ischain *device,
	struct is_video_ctx *vctx)
{
	int ret = 0;
	int ret_err = 0;
	u32 group_id;
	struct is_groupmgr *groupmgr;
	struct is_group *group;

	FIMC_BUG(!device);
	FIMC_BUG(!vctx);
	FIMC_BUG(!GET_VIDEO(vctx));

	groupmgr = device->groupmgr;
	group = &device->group_3aa;
	group_id = GROUP_ID_3AA0 + GET_3XS_ID(GET_VIDEO(vctx));

	ret = is_group_open(groupmgr,
		group,
		group_id,
		vctx);
	if (ret) {
		merr("is_group_open is fail(%d)", device, ret);
		goto err_group_open;
	}

	ret = is_ischain_open_wrap(device, false);
	if (ret) {
		merr("is_ischain_open_wrap is fail(%d)", device, ret);
		goto err_ischain_open;
	}

	atomic_inc(&device->group_open_cnt);

	return 0;

err_ischain_open:
	ret_err = is_group_close(groupmgr, group);
	if (ret_err)
		merr("is_group_close is fail(%d)", device, ret_err);
err_group_open:
	return ret;
}

int is_ischain_3aa_close(struct is_device_ischain *device,
	struct is_video_ctx *vctx)
{
	int ret = 0;
	struct is_groupmgr *groupmgr;
	struct is_group *group;
	struct is_queue *queue;

	FIMC_BUG(!device);

	groupmgr = device->groupmgr;
	group = &device->group_3aa;
	queue = GET_QUEUE(vctx);

	/* for mediaserver dead */
	if (test_bit(IS_GROUP_START, &group->state)) {
		mgwarn("sudden group close", device, group);
		if (!test_bit(IS_ISCHAIN_REPROCESSING, &device->state))
			is_itf_sudden_stop_wrap(device, device->instance, group);
		set_bit(IS_GROUP_REQUEST_FSTOP, &group->state);
		if (test_bit(IS_HAL_DEBUG_SUDDEN_DEAD_DETECT, &sysfs_debug.hal_debug_mode)) {
			msleep(sysfs_debug.hal_debug_delay);
			panic("HAL sudden group close #1");
		}
	}

	if (group->head && test_bit(IS_GROUP_START, &group->head->state)) {
		mgwarn("sudden group close", device, group);
		if (!test_bit(IS_ISCHAIN_REPROCESSING, &device->state))
			is_itf_sudden_stop_wrap(device, device->instance, group);
		set_bit(IS_GROUP_REQUEST_FSTOP, &group->state);
		if (test_bit(IS_HAL_DEBUG_SUDDEN_DEAD_DETECT, &sysfs_debug.hal_debug_mode)) {
			msleep(sysfs_debug.hal_debug_delay);
			panic("HAL sudden group close #2");
		}
	}

	ret = is_ischain_3aa_stop(device, queue);
	if (ret)
		merr("is_ischain_3aa_stop is fail", device);

	ret = is_ischain_close_wrap(device);
	if (ret)
		merr("is_ischain_close_wrap is fail(%d)", device, ret);

	ret = is_group_close(groupmgr, group);
	if (ret)
		merr("is_group_close is fail", device);

	atomic_dec(&device->group_open_cnt);

	return ret;
}

int is_ischain_3aa_s_input(struct is_device_ischain *device,
	u32 stream_type,
	u32 position,
	u32 video_id,
	u32 input_type,
	u32 stream_leader)
{
	int ret = 0;
	struct is_group *group;
	struct is_groupmgr *groupmgr;

	FIMC_BUG(!device);
	FIMC_BUG(!device->groupmgr);

	groupmgr = device->groupmgr;
	group = &device->group_3aa;

	mdbgd_ischain("%s()\n", device, __func__);

	ret = is_group_init(groupmgr, group, input_type, video_id, stream_leader);
	if (ret) {
		merr("is_group_init is fail(%d)", device, ret);
		goto p_err;
	}

	ret = is_ischain_init_wrap(device, stream_type, position);
	if (ret) {
		merr("is_ischain_init_wrap is fail(%d)", device, ret);
		goto p_err;
	}

p_err:
	return ret;
}

static int is_ischain_3aa_start(void *qdevice,
	struct is_queue *queue)
{
	int ret = 0;
	struct is_device_ischain *device = qdevice;
	struct is_groupmgr *groupmgr;
	struct is_group *group;

	FIMC_BUG(!device);

	groupmgr = device->groupmgr;
	group = &device->group_3aa;

	ret = is_group_start(groupmgr, group);
	if (ret) {
		merr("is_group_start is fail(%d)", device, ret);
		goto p_err;
	}

	ret = is_ischain_start_wrap(device, group);
	if (ret) {
		merr("is_ischain_start_wrap is fail(%d)", device, ret);
		goto p_err;
	}

p_err:
	return ret;
}

static int is_ischain_3aa_stop(void *qdevice,
	struct is_queue *queue)
{
	int ret = 0;
	struct is_device_ischain *device = qdevice;
	struct is_groupmgr *groupmgr;
	struct is_group *group;

	FIMC_BUG(!device);

	groupmgr = device->groupmgr;
	group = &device->group_3aa;

	if (!test_bit(IS_GROUP_INIT, &group->state))
		goto p_err;

	ret = is_group_stop(groupmgr, group);
	if (ret) {
		if (ret == -EPERM)
			ret = 0;
		else
			merr("is_group_stop is fail(%d)", device, ret);
		goto p_err;
	}

	ret = is_ischain_stop_wrap(device, group);
	if (ret) {
		merr("is_ischain_stop_wrap is fail(%d)", device, ret);
		goto p_err;
	}

p_err:
	mginfo("%s(%d):%d\n", device, group,  __func__, atomic_read(&group->scount), ret);
	return ret;
}

static int is_ischain_3aa_reqbufs(void *qdevice,
	struct is_queue *queue, u32 count)
{
	return 0;
}

static int is_ischain_3aa_s_format(void *qdevice,
	struct is_queue *queue)
{
	int ret = 0;
	struct is_device_ischain *device = qdevice;
	struct is_subdev *leader;

	FIMC_BUG(!device);
	FIMC_BUG(!queue);

	leader = &device->group_3aa.leader;

	leader->input.width = queue->framecfg.width;
	leader->input.height = queue->framecfg.height;

	leader->input.crop.x = 0;
	leader->input.crop.y = 0;
	leader->input.crop.w = leader->input.width;
	leader->input.crop.h = leader->input.height;

	return ret;
}

int is_ischain_3aa_buffer_queue(struct is_device_ischain *device,
	struct is_queue *queue,
	u32 index)
{
	int ret = 0;
	struct is_groupmgr *groupmgr;
	struct is_group *group;

	FIMC_BUG(!device);
	FIMC_BUG(!test_bit(IS_ISCHAIN_OPEN, &device->state));

	mdbgs_ischain(4, "%s\n", device, __func__);

	groupmgr = device->groupmgr;
	group = &device->group_3aa;

	ret = is_group_buffer_queue(groupmgr, group, queue, index);
	if (ret)
		merr("is_group_buffer_queue is fail(%d)", device, ret);

	return ret;
}

int is_ischain_3aa_buffer_finish(struct is_device_ischain *device,
	u32 index)
{
	int ret = 0;
	struct is_groupmgr *groupmgr;
	struct is_group *group;

	FIMC_BUG(!device);

	mdbgs_ischain(4, "%s\n", device, __func__);

	groupmgr = device->groupmgr;
	group = &device->group_3aa;

	ret = is_group_buffer_finish(groupmgr, group, index);
	if (ret)
		merr("is_group_buffer_finish is fail(%d)", device, ret);

	return ret;
}

const struct is_queue_ops is_ischain_3aa_ops = {
	.start_streaming	= is_ischain_3aa_start,
	.stop_streaming		= is_ischain_3aa_stop,
	.s_format		= is_ischain_3aa_s_format,
	.request_bufs		= is_ischain_3aa_reqbufs
};

int is_ischain_lme_open(struct is_device_ischain *device,
	struct is_video_ctx *vctx)
{
	int ret = 0;
	int ret_err = 0;
	u32 group_id;
	struct is_groupmgr *groupmgr;
	struct is_group *group;

	FIMC_BUG(!device);
	FIMC_BUG(!vctx);
	FIMC_BUG(!GET_VIDEO(vctx));

	groupmgr = device->groupmgr;
	group = &device->group_lme;
	group_id = GROUP_ID_LME0 + GET_LMEXS_ID(GET_VIDEO(vctx));

	ret = is_group_open(groupmgr,
		group,
		group_id,
		vctx);
	if (ret) {
		merr("is_group_open is fail(%d)", device, ret);
		goto err_group_open;
	}

	ret = is_ischain_open_wrap(device, false);
	if (ret) {
		merr("is_ischain_open_wrap is fail(%d)", device, ret);
		goto err_ischain_open;
	}

	atomic_inc(&device->group_open_cnt);

	return 0;

err_ischain_open:
	ret_err = is_group_close(groupmgr, group);
	if (ret_err)
		merr("is_group_close is fail(%d)", device, ret_err);
err_group_open:
	return ret;
}

int is_ischain_lme_close(struct is_device_ischain *device,
	struct is_video_ctx *vctx)
{
	int ret = 0;
	struct is_groupmgr *groupmgr;
	struct is_group *group;
	struct is_queue *queue;

	FIMC_BUG(!device);

	groupmgr = device->groupmgr;
	group = &device->group_lme;
	queue = GET_QUEUE(vctx);

	/* for mediaserver dead */
	if (test_bit(IS_GROUP_START, &group->state)) {
		mgwarn("sudden group close", device, group);
		if (!test_bit(IS_ISCHAIN_REPROCESSING, &device->state))
			is_itf_sudden_stop_wrap(device, device->instance, group);
		set_bit(IS_GROUP_REQUEST_FSTOP, &group->state);
		if (test_bit(IS_HAL_DEBUG_SUDDEN_DEAD_DETECT, &sysfs_debug.hal_debug_mode)) {
			msleep(sysfs_debug.hal_debug_delay);
			panic("HAL sudden group close #1");
		}
	}

	if (group->head && test_bit(IS_GROUP_START, &group->head->state)) {
		mgwarn("sudden group close", device, group);
		if (!test_bit(IS_ISCHAIN_REPROCESSING, &device->state))
			is_itf_sudden_stop_wrap(device, device->instance, group);
		set_bit(IS_GROUP_REQUEST_FSTOP, &group->state);
		if (test_bit(IS_HAL_DEBUG_SUDDEN_DEAD_DETECT, &sysfs_debug.hal_debug_mode)) {
			msleep(sysfs_debug.hal_debug_delay);
			panic("HAL sudden group close #2");
		}
	}

	ret = is_ischain_lme_stop(device, queue);
	if (ret)
		merr("is_ischain_lme_stop is fail", device);

	ret = is_ischain_close_wrap(device);
	if (ret)
		merr("is_ischain_close_wrap is fail(%d)", device, ret);

	ret = is_group_close(groupmgr, group);
	if (ret)
		merr("is_group_close is fail", device);

	atomic_dec(&device->group_open_cnt);

	return ret;
}

int is_ischain_lme_s_input(struct is_device_ischain *device,
	u32 stream_type,
	u32 position,
	u32 video_id,
	u32 input_type,
	u32 stream_leader)
{
	int ret = 0;
	struct is_group *group;
	struct is_groupmgr *groupmgr;

	FIMC_BUG(!device);
	FIMC_BUG(!device->groupmgr);

	groupmgr = device->groupmgr;
	group = &device->group_lme;

	mdbgd_ischain("%s()\n", device, __func__);

	ret = is_group_init(groupmgr, group, input_type, video_id, stream_leader);
	if (ret) {
		merr("is_group_init is fail(%d)", device, ret);
		goto p_err;
	}

	ret = is_ischain_init_wrap(device, stream_type, position);
	if (ret) {
		merr("is_ischain_init_wrap is fail(%d)", device, ret);
		goto p_err;
	}

p_err:
	return ret;
}

static int is_ischain_lme_start(void *qdevice,
	struct is_queue *queue)
{
	int ret = 0;
	struct is_device_ischain *device = qdevice;
	struct is_groupmgr *groupmgr;
	struct is_group *group;

	FIMC_BUG(!device);

	groupmgr = device->groupmgr;
	group = &device->group_lme;

	ret = is_group_start(groupmgr, group);
	if (ret) {
		merr("is_group_start is fail(%d)", device, ret);
		goto p_err;
	}

	ret = is_ischain_start_wrap(device, group);
	if (ret) {
		merr("is_ischain_start_wrap is fail(%d)", device, ret);
		goto p_err;
	}

p_err:
	return ret;
}

static int is_ischain_lme_stop(void *qdevice,
	struct is_queue *queue)
{
	int ret = 0;
	struct is_device_ischain *device = qdevice;
	struct is_groupmgr *groupmgr;
	struct is_group *group;

	FIMC_BUG(!device);

	groupmgr = device->groupmgr;
	group = &device->group_lme;

	if (!test_bit(IS_GROUP_INIT, &group->state))
		goto p_err;

	ret = is_group_stop(groupmgr, group);
	if (ret) {
		if (ret == -EPERM)
			ret = 0;
		else
			merr("is_group_stop is fail(%d)", device, ret);
		goto p_err;
	}

	ret = is_ischain_stop_wrap(device, group);
	if (ret) {
		merr("is_ischain_stop_wrap is fail(%d)", device, ret);
		goto p_err;
	}

p_err:
	mginfo("%s(%d):%d\n", device, group,  __func__, atomic_read(&group->scount), ret);
	return ret;
}

static int is_ischain_lme_reqbufs(void *qdevice,
	struct is_queue *queue, u32 count)
{
	return 0;
}

static int is_ischain_lme_s_format(void *qdevice,
	struct is_queue *queue)
{
	int ret = 0;
	struct is_device_ischain *device = qdevice;
	struct is_subdev *leader;

	FIMC_BUG(!device);
	FIMC_BUG(!queue);

	leader = &device->group_lme.leader;

	leader->input.width = queue->framecfg.width;
	leader->input.height = queue->framecfg.height;

	leader->input.crop.x = 0;
	leader->input.crop.y = 0;
	leader->input.crop.w = leader->input.width;
	leader->input.crop.h = leader->input.height;

	return ret;
}

int is_ischain_lme_buffer_queue(struct is_device_ischain *device,
	struct is_queue *queue,
	u32 index)
{
	int ret = 0;
	struct is_groupmgr *groupmgr;
	struct is_group *group;

	FIMC_BUG(!device);
	FIMC_BUG(!test_bit(IS_ISCHAIN_OPEN, &device->state));

	mdbgs_ischain(4, "%s\n", device, __func__);

	groupmgr = device->groupmgr;
	group = &device->group_lme;

	ret = is_group_buffer_queue(groupmgr, group, queue, index);
	if (ret)
		merr("is_group_buffer_queue is fail(%d)", device, ret);

	return ret;
}

int is_ischain_lme_buffer_finish(struct is_device_ischain *device,
	u32 index)
{
	int ret = 0;
	struct is_groupmgr *groupmgr;
	struct is_group *group;

	FIMC_BUG(!device);

	mdbgs_ischain(4, "%s\n", device, __func__);

	groupmgr = device->groupmgr;
	group = &device->group_lme;

	ret = is_group_buffer_finish(groupmgr, group, index);
	if (ret)
		merr("is_group_buffer_finish is fail(%d)", device, ret);

	return ret;
}

const struct is_queue_ops is_ischain_lme_ops = {
	.start_streaming	= is_ischain_lme_start,
	.stop_streaming		= is_ischain_lme_stop,
	.s_format		= is_ischain_lme_s_format,
	.request_bufs		= is_ischain_lme_reqbufs
};

int is_ischain_isp_open(struct is_device_ischain *device,
	struct is_video_ctx *vctx)
{
	int ret = 0;
	int ret_err = 0;
	u32 group_id;
	struct is_groupmgr *groupmgr;
	struct is_group *group;

	FIMC_BUG(!device);
	FIMC_BUG(!vctx);
	FIMC_BUG(!GET_VIDEO(vctx));

	groupmgr = device->groupmgr;
	group = &device->group_isp;
	group_id = GROUP_ID_ISP0 + GET_IXS_ID(GET_VIDEO(vctx));

	ret = is_group_open(groupmgr,
		group,
		group_id,
		vctx);
	if (ret) {
		merr("is_group_open is fail(%d)", device, ret);
		goto err_group_open;
	}

	ret = is_ischain_open_wrap(device, false);
	if (ret) {
		merr("is_ischain_open_wrap is fail(%d)", device, ret);
		goto err_ischain_open;
	}

	atomic_inc(&device->group_open_cnt);

	return 0;

err_ischain_open:
	ret_err = is_group_close(groupmgr, group);
	if (ret_err)
		merr("is_group_close is fail(%d)", device, ret_err);
err_group_open:
	return ret;
}

int is_ischain_isp_close(struct is_device_ischain *device,
	struct is_video_ctx *vctx)
{
	int ret = 0;
	struct is_groupmgr *groupmgr;
	struct is_group *group;
	struct is_queue *queue;

	FIMC_BUG(!device);

	groupmgr = device->groupmgr;
	group = &device->group_isp;
	queue = GET_QUEUE(vctx);

	/* for mediaserver dead */
	if (test_bit(IS_GROUP_START, &group->state)) {
		mgwarn("sudden group close", device, group);
		if (!test_bit(IS_ISCHAIN_REPROCESSING, &device->state))
			is_itf_sudden_stop_wrap(device, device->instance, group);
		set_bit(IS_GROUP_REQUEST_FSTOP, &group->state);
		if (test_bit(IS_HAL_DEBUG_SUDDEN_DEAD_DETECT, &sysfs_debug.hal_debug_mode)) {
			msleep(sysfs_debug.hal_debug_delay);
			panic("HAL sudden group close #1");
		}
	}

	if (group->head && test_bit(IS_GROUP_START, &group->head->state)) {
		mgwarn("sudden group close", device, group);
		if (!test_bit(IS_ISCHAIN_REPROCESSING, &device->state))
			is_itf_sudden_stop_wrap(device, device->instance, group);
		set_bit(IS_GROUP_REQUEST_FSTOP, &group->state);
		if (test_bit(IS_HAL_DEBUG_SUDDEN_DEAD_DETECT, &sysfs_debug.hal_debug_mode)) {
			msleep(sysfs_debug.hal_debug_delay);
			panic("HAL sudden group close #2");
		}
	}

	ret = is_ischain_isp_stop(device, queue);
	if (ret)
		merr("is_ischain_isp_stop is fail", device);

	ret = is_ischain_close_wrap(device);
	if (ret)
		merr("is_ischain_close_wrap is fail(%d)", device, ret);

	ret = is_group_close(groupmgr, group);
	if (ret)
		merr("is_group_close is fail", device);

	atomic_dec(&device->group_open_cnt);

	return ret;
}

int is_ischain_isp_s_input(struct is_device_ischain *device,
	u32 stream_type,
	u32 position,
	u32 video_id,
	u32 input_type,
	u32 stream_leader)
{
	int ret = 0;
	struct is_group *group;
	struct is_groupmgr *groupmgr;

	FIMC_BUG(!device);

	groupmgr = device->groupmgr;
	group = &device->group_isp;

	mdbgd_ischain("%s()\n", device, __func__);

	ret = is_group_init(groupmgr, group, input_type, video_id, stream_leader);
	if (ret) {
		merr("is_group_init is fail(%d)", device, ret);
		goto p_err;
	}

	ret = is_ischain_init_wrap(device, stream_type, position);
	if (ret) {
		merr("is_ischain_init_wrap is fail(%d)", device, ret);
		goto p_err;
	}

p_err:
	return ret;
}

static int is_ischain_isp_start(void *qdevice,
	struct is_queue *queue)
{
	int ret = 0;
	struct is_device_ischain *device = qdevice;
	struct is_groupmgr *groupmgr;
	struct is_group *group;

	FIMC_BUG(!device);

	groupmgr = device->groupmgr;
	group = &device->group_isp;

	ret = is_group_start(groupmgr, group);
	if (ret) {
		merr("is_group_start is fail(%d)", device, ret);
		goto p_err;
	}

	ret = is_ischain_start_wrap(device, group);
	if (ret) {
		merr("is_ischain_start_wrap is fail(%d)", device, ret);
		goto p_err;
	}

p_err:
	return ret;
}

static int is_ischain_isp_stop(void *qdevice,
	struct is_queue *queue)
{
	int ret = 0;
	struct is_device_ischain *device = qdevice;
	struct is_groupmgr *groupmgr;
	struct is_group *group;

	FIMC_BUG(!device);

	groupmgr = device->groupmgr;
	group = &device->group_isp;

	if (!test_bit(IS_GROUP_INIT, &group->state))
		goto p_err;

	ret = is_group_stop(groupmgr, group);
	if (ret) {
		if (ret == -EPERM)
			ret = 0;
		else
			merr("is_group_stop is fail(%d)", device, ret);
		goto p_err;
	}

	ret = is_ischain_stop_wrap(device, group);
	if (ret) {
		merr("is_ischain_stop_wrap is fail(%d)", device, ret);
		goto p_err;
	}

p_err:
	mginfo("%s(%d):%d\n", device, group,  __func__, atomic_read(&group->scount), ret);
	return ret;
}

static int is_ischain_isp_reqbufs(void *qdevice,
	struct is_queue *queue, u32 count)
{
	return 0;
}

static int is_ischain_isp_s_format(void *qdevice,
	struct is_queue *queue)
{
	int ret = 0;
	struct is_device_ischain *device = qdevice;
	struct is_subdev *leader;

	FIMC_BUG(!device);
	FIMC_BUG(!queue);

	leader = &device->group_isp.leader;

	leader->input.width = queue->framecfg.width;
	leader->input.height = queue->framecfg.height;

	leader->input.crop.x = 0;
	leader->input.crop.y = 0;
	leader->input.crop.w = leader->input.width;
	leader->input.crop.h = leader->input.height;

	return ret;
}

int is_ischain_isp_buffer_queue(struct is_device_ischain *device,
	struct is_queue *queue,
	u32 index)
{
	int ret = 0;
	struct is_groupmgr *groupmgr;
	struct is_group *group;

	FIMC_BUG(!device);
	FIMC_BUG(!test_bit(IS_ISCHAIN_OPEN, &device->state));

	mdbgs_ischain(4, "%s\n", device, __func__);

	groupmgr = device->groupmgr;
	group = &device->group_isp;

	ret = is_group_buffer_queue(groupmgr, group, queue, index);
	if (ret) {
		merr("is_group_buffer_queue is fail(%d)", device, ret);
		goto p_err;
	}

p_err:
	return ret;
}

int is_ischain_isp_buffer_finish(struct is_device_ischain *device,
	u32 index)
{
	int ret = 0;
	struct is_groupmgr *groupmgr;
	struct is_group *group;

	FIMC_BUG(!device);

	mdbgs_ischain(4, "%s\n", device, __func__);

	groupmgr = device->groupmgr;
	group = &device->group_isp;

	ret = is_group_buffer_finish(groupmgr, group, index);
	if (ret)
		merr("is_group_buffer_finish is fail(%d)", device, ret);

	return ret;
}

const struct is_queue_ops is_ischain_isp_ops = {
	.start_streaming	= is_ischain_isp_start,
	.stop_streaming		= is_ischain_isp_stop,
	.s_format		= is_ischain_isp_s_format,
	.request_bufs		= is_ischain_isp_reqbufs
};

int is_ischain_ypp_open(struct is_device_ischain *device,
	struct is_video_ctx *vctx)
{
	int ret = 0;
	int ret_err = 0;
	u32 group_id;
	struct is_groupmgr *groupmgr;
	struct is_group *group;

	FIMC_BUG(!device);
	FIMC_BUG(!vctx);
	FIMC_BUG(!GET_VIDEO(vctx));

	groupmgr = device->groupmgr;
	group = &device->group_ypp;
	group_id = GROUP_ID_YPP;

	ret = is_group_open(groupmgr,
		group,
		group_id,
		vctx);
	if (ret) {
		merr("is_group_open is fail(%d)", device, ret);
		goto err_group_open;
	}

	ret = is_ischain_open_wrap(device, false);
	if (ret) {
		merr("is_ischain_open_wrap is fail(%d)", device, ret);
		goto err_ischain_open;
	}

	atomic_inc(&device->group_open_cnt);

	return 0;

err_ischain_open:
	ret_err = is_group_close(groupmgr, group);
	if (ret_err)
		merr("is_group_close is fail(%d)", device, ret_err);
err_group_open:
	return ret;
}

int is_ischain_ypp_close(struct is_device_ischain *device,
	struct is_video_ctx *vctx)
{
	int ret = 0;
	struct is_groupmgr *groupmgr;
	struct is_group *group;
	struct is_queue *queue;

	FIMC_BUG(!device);

	groupmgr = device->groupmgr;
	group = &device->group_ypp;
	queue = GET_QUEUE(vctx);

	/* for mediaserver dead */
	if (test_bit(IS_GROUP_START, &group->state)) {
		mgwarn("sudden group close", device, group);
		if (!test_bit(IS_ISCHAIN_REPROCESSING, &device->state))
			is_itf_sudden_stop_wrap(device, device->instance, group);
		set_bit(IS_GROUP_REQUEST_FSTOP, &group->state);
		if (test_bit(IS_HAL_DEBUG_SUDDEN_DEAD_DETECT, &sysfs_debug.hal_debug_mode)) {
			msleep(sysfs_debug.hal_debug_delay);
			panic("HAL sudden group close #1");
		}
	}

	if (group->head && test_bit(IS_GROUP_START, &group->head->state)) {
		mgwarn("sudden group close", device, group);
		if (!test_bit(IS_ISCHAIN_REPROCESSING, &device->state))
			is_itf_sudden_stop_wrap(device, device->instance, group);
		set_bit(IS_GROUP_REQUEST_FSTOP, &group->state);
		if (test_bit(IS_HAL_DEBUG_SUDDEN_DEAD_DETECT, &sysfs_debug.hal_debug_mode)) {
			msleep(sysfs_debug.hal_debug_delay);
			panic("HAL sudden group close #2");
		}
	}

	ret = is_ischain_ypp_stop(device, queue);
	if (ret)
		merr("is_ischain_ypp_stop is fail", device);

	ret = is_ischain_close_wrap(device);
	if (ret)
		merr("is_ischain_close_wrap is fail(%d)", device, ret);

	ret = is_group_close(groupmgr, group);
	if (ret)
		merr("is_group_close is fail", device);

	atomic_dec(&device->group_open_cnt);

	return ret;
}

int is_ischain_ypp_s_input(struct is_device_ischain *device,
	u32 stream_type,
	u32 position,
	u32 video_id,
	u32 input_type,
	u32 stream_leader)
{
	int ret = 0;
	struct is_group *group;
	struct is_groupmgr *groupmgr;

	FIMC_BUG(!device);
	FIMC_BUG(!device->groupmgr);

	groupmgr = device->groupmgr;
	group = &device->group_ypp;

	mdbgd_ischain("%s()\n", device, __func__);

	ret = is_group_init(groupmgr, group, input_type, video_id, stream_leader);
	if (ret) {
		merr("is_group_init is fail(%d)", device, ret);
		goto p_err;
	}

	ret = is_ischain_init_wrap(device, stream_type, position);
	if (ret) {
		merr("is_ischain_init_wrap is fail(%d)", device, ret);
		goto p_err;
	}

p_err:
	return ret;
}

static int is_ischain_ypp_start(void *qdevice,
	struct is_queue *queue)
{
	int ret = 0;
	struct is_device_ischain *device = qdevice;
	struct is_groupmgr *groupmgr;
	struct is_group *group;

	FIMC_BUG(!device);

	groupmgr = device->groupmgr;
	group = &device->group_ypp;

	ret = is_group_start(groupmgr, group);
	if (ret) {
		merr("is_group_start is fail(%d)", device, ret);
		goto p_err;
	}

	ret = is_ischain_start_wrap(device, group);
	if (ret) {
		merr("is_ischain_start_wrap is fail(%d)", device, ret);
		goto p_err;
	}

p_err:
	return ret;
}

static int is_ischain_ypp_stop(void *qdevice,
	struct is_queue *queue)
{
	int ret = 0;
	struct is_device_ischain *device = qdevice;
	struct is_groupmgr *groupmgr;
	struct is_group *group;

	FIMC_BUG(!device);

	groupmgr = device->groupmgr;
	group = &device->group_ypp;

	if (!test_bit(IS_GROUP_INIT, &group->state))
		goto p_err;

	ret = is_group_stop(groupmgr, group);
	if (ret) {
		if (ret == -EPERM)
			ret = 0;
		else
			merr("is_group_stop is fail(%d)", device, ret);
		goto p_err;
	}

	ret = is_ischain_stop_wrap(device, group);
	if (ret) {
		merr("is_ischain_stop_wrap is fail(%d)", device, ret);
		goto p_err;
	}

p_err:
	mginfo("%s(%d):%d\n", device, group,  __func__, atomic_read(&group->scount), ret);
	return ret;
}

static int is_ischain_ypp_reqbufs(void *qdevice,
	struct is_queue *queue, u32 count)
{
	return 0;
}

static int is_ischain_ypp_s_format(void *qdevice,
	struct is_queue *queue)
{
	int ret = 0;
	struct is_device_ischain *device = qdevice;
	struct is_subdev *leader;

	FIMC_BUG(!device);
	FIMC_BUG(!queue);

	leader = &device->group_ypp.leader;

	leader->input.width = queue->framecfg.width;
	leader->input.height = queue->framecfg.height;

	leader->input.crop.x = 0;
	leader->input.crop.y = 0;
	leader->input.crop.w = leader->input.width;
	leader->input.crop.h = leader->input.height;

	return ret;
}

int is_ischain_ypp_buffer_queue(struct is_device_ischain *device,
	struct is_queue *queue,
	u32 index)
{
	int ret = 0;
	struct is_groupmgr *groupmgr;
	struct is_group *group;

	FIMC_BUG(!device);
	FIMC_BUG(!test_bit(IS_ISCHAIN_OPEN, &device->state));

	mdbgs_ischain(4, "%s\n", device, __func__);

	groupmgr = device->groupmgr;
	group = &device->group_ypp;

	ret = is_group_buffer_queue(groupmgr, group, queue, index);
	if (ret)
		merr("is_group_buffer_queue is fail(%d)", device, ret);

	return ret;
}

int is_ischain_ypp_buffer_finish(struct is_device_ischain *device,
	u32 index)
{
	int ret = 0;
	struct is_groupmgr *groupmgr;
	struct is_group *group;

	FIMC_BUG(!device);

	mdbgs_ischain(4, "%s\n", device, __func__);

	groupmgr = device->groupmgr;
	group = &device->group_ypp;

	ret = is_group_buffer_finish(groupmgr, group, index);
	if (ret)
		merr("is_group_buffer_finish is fail(%d)", device, ret);

	return ret;
}

const struct is_queue_ops is_ischain_ypp_ops = {
	.start_streaming	= is_ischain_ypp_start,
	.stop_streaming		= is_ischain_ypp_stop,
	.s_format		= is_ischain_ypp_s_format,
	.request_bufs		= is_ischain_ypp_reqbufs
};

int is_ischain_mcs_open(struct is_device_ischain *device,
	struct is_video_ctx *vctx)
{
	int ret = 0;
	int ret_err = 0;
	u32 group_id;
	struct is_groupmgr *groupmgr;
	struct is_group *group;

	FIMC_BUG(!device);
	FIMC_BUG(!vctx);
	FIMC_BUG(!GET_VIDEO(vctx));

	groupmgr = device->groupmgr;
	group = &device->group_mcs;
	group_id = GROUP_ID_MCS0 + GET_MXS_ID(GET_VIDEO(vctx));

	ret = is_group_open(groupmgr,
		group,
		group_id,
		vctx);
	if (ret) {
		merr("is_group_open is fail(%d)", device, ret);
		goto err_group_open;
	}

	ret = is_ischain_open_wrap(device, false);
	if (ret) {
		merr("is_ischain_open_wrap is fail(%d)", device, ret);
		goto err_ischain_open;
	}

	atomic_inc(&device->group_open_cnt);

	return 0;

err_ischain_open:
	ret_err = is_group_close(groupmgr, group);
	if (ret_err)
		merr("is_group_close is fail(%d)", device, ret_err);
err_group_open:
	return ret;
}

int is_ischain_mcs_close(struct is_device_ischain *device,
	struct is_video_ctx *vctx)
{
	int ret = 0;
	struct is_groupmgr *groupmgr;
	struct is_group *group;
	struct is_queue *queue;

	FIMC_BUG(!device);

	groupmgr = device->groupmgr;
	group = &device->group_mcs;
	queue = GET_QUEUE(vctx);

	/* for mediaserver dead */
	if (test_bit(IS_GROUP_START, &group->state)) {
		mgwarn("sudden group close", device, group);
		if (!test_bit(IS_ISCHAIN_REPROCESSING, &device->state))
			is_itf_sudden_stop_wrap(device, device->instance, group);
		set_bit(IS_GROUP_REQUEST_FSTOP, &group->state);
		if (test_bit(IS_HAL_DEBUG_SUDDEN_DEAD_DETECT, &sysfs_debug.hal_debug_mode)) {
			msleep(sysfs_debug.hal_debug_delay);
			panic("HAL sudden group close #1");
		}
	}

	if (group->head && test_bit(IS_GROUP_START, &group->head->state)) {
		mgwarn("sudden group close", device, group);
		if (!test_bit(IS_ISCHAIN_REPROCESSING, &device->state))
			is_itf_sudden_stop_wrap(device, device->instance, group);
		set_bit(IS_GROUP_REQUEST_FSTOP, &group->state);
		if (test_bit(IS_HAL_DEBUG_SUDDEN_DEAD_DETECT, &sysfs_debug.hal_debug_mode)) {
			msleep(sysfs_debug.hal_debug_delay);
			panic("HAL sudden group close #2");
		}
	}

	ret = is_ischain_mcs_stop(device, queue);
	if (ret)
		merr("is_ischain_mcs_stop is fail", device);

	ret = is_ischain_close_wrap(device);
	if (ret)
		merr("is_ischain_close_wrap is fail(%d)", device, ret);

	ret = is_group_close(groupmgr, group);
	if (ret)
		merr("is_group_close is fail", device);

	atomic_dec(&device->group_open_cnt);

	return ret;
}

int is_ischain_mcs_s_input(struct is_device_ischain *device,
	u32 stream_type,
	u32 position,
	u32 video_id,
	u32 otf_input,
	u32 stream_leader)
{
	int ret = 0;
	struct is_group *group;
	struct is_groupmgr *groupmgr;

	FIMC_BUG(!device);
	FIMC_BUG(!device->groupmgr);

	groupmgr = device->groupmgr;
	group = &device->group_mcs;

	mdbgd_ischain("%s()\n", device, __func__);

	ret = is_group_init(groupmgr, group, otf_input, video_id, stream_leader);
	if (ret) {
		merr("is_group_init is fail(%d)", device, ret);
		goto p_err;
	}

	ret = is_ischain_init_wrap(device, stream_type, position);
	if (ret) {
		merr("is_ischain_init_wrap is fail(%d)", device, ret);
		goto p_err;
	}

p_err:
	return ret;
}

static int is_ischain_mcs_start(void *qdevice,
	struct is_queue *queue)
{
	int ret = 0;
	struct is_device_ischain *device = qdevice;
	struct is_groupmgr *groupmgr;
	struct is_group *group;

	FIMC_BUG(!device);

	groupmgr = device->groupmgr;
	group = &device->group_mcs;

	ret = is_group_start(groupmgr, group);
	if (ret) {
		merr("is_group_start is fail(%d)", device, ret);
		goto p_err;
	}

	ret = is_ischain_start_wrap(device, group);
	if (ret) {
		merr("is_ischain_start_wrap is fail(%d)", device, ret);
		goto p_err;
	}

p_err:
	return ret;
}

static int is_ischain_mcs_stop(void *qdevice,
	struct is_queue *queue)
{
	int ret = 0;
	struct is_device_ischain *device = qdevice;
	struct is_groupmgr *groupmgr;
	struct is_group *group;

	FIMC_BUG(!device);

	groupmgr = device->groupmgr;
	group = &device->group_mcs;

	if (!test_bit(IS_GROUP_INIT, &group->state))
		goto p_err;

	ret = is_group_stop(groupmgr, group);
	if (ret) {
		if (ret == -EPERM)
			ret = 0;
		else
			merr("is_group_stop is fail(%d)", device, ret);
		goto p_err;
	}

	ret = is_ischain_stop_wrap(device, group);
	if (ret) {
		merr("is_ischain_stop_wrap is fail(%d)", device, ret);
		goto p_err;
	}

p_err:
	mginfo("%s(%d):%d\n", device, group,  __func__, atomic_read(&group->scount), ret);
	return ret;
}

static int is_ischain_mcs_reqbufs(void *qdevice,
	struct is_queue *queue, u32 count)
{
	return 0;
}

static int is_ischain_mcs_s_format(void *qdevice,
	struct is_queue *queue)
{
	int ret = 0;
	struct is_device_ischain *device = qdevice;
	struct is_subdev *leader;

	FIMC_BUG(!device);
	FIMC_BUG(!queue);

	leader = &device->group_mcs.leader;

	leader->input.width = queue->framecfg.width;
	leader->input.height = queue->framecfg.height;

	leader->input.crop.x = 0;
	leader->input.crop.y = 0;
	leader->input.crop.w = leader->input.width;
	leader->input.crop.h = leader->input.height;

	return ret;
}

int is_ischain_mcs_buffer_queue(struct is_device_ischain *device,
	struct is_queue *queue,
	u32 index)
{
	int ret = 0;
	struct is_groupmgr *groupmgr;
	struct is_group *group;

	FIMC_BUG(!device);
	FIMC_BUG(!test_bit(IS_ISCHAIN_OPEN, &device->state));

	mdbgs_ischain(4, "%s\n", device, __func__);

	groupmgr = device->groupmgr;
	group = &device->group_mcs;

	ret = is_group_buffer_queue(groupmgr, group, queue, index);
	if (ret)
		merr("is_group_buffer_queue is fail(%d)", device, ret);

	return ret;
}

int is_ischain_mcs_buffer_finish(struct is_device_ischain *device,
	u32 index)
{
	int ret = 0;
	struct is_groupmgr *groupmgr;
	struct is_group *group;

	FIMC_BUG(!device);

	mdbgs_ischain(4, "%s\n", device, __func__);

	groupmgr = device->groupmgr;
	group = &device->group_mcs;

	ret = is_group_buffer_finish(groupmgr, group, index);
	if (ret)
		merr("is_group_buffer_finish is fail(%d)", device, ret);

	return ret;
}

const struct is_queue_ops is_ischain_mcs_ops = {
	.start_streaming	= is_ischain_mcs_start,
	.stop_streaming		= is_ischain_mcs_stop,
	.s_format		= is_ischain_mcs_s_format,
	.request_bufs		= is_ischain_mcs_reqbufs
};

int is_ischain_vra_open(struct is_device_ischain *device,
	struct is_video_ctx *vctx)
{
	int ret = 0;
	int ret_err = 0;
	struct is_groupmgr *groupmgr;
	struct is_group *group;

	FIMC_BUG(!device);

	groupmgr = device->groupmgr;
	group = &device->group_vra;

	ret = is_group_open(groupmgr,
		group,
		GROUP_ID_VRA0,
		vctx);
	if (ret) {
		merr("is_group_open is fail(%d)", device, ret);
		goto err_group_open;
	}

	ret = is_ischain_open_wrap(device, false);
	if (ret) {
		merr("is_ischain_open_wrap is fail(%d)", device, ret);
		goto err_ischain_open;
	}

	atomic_inc(&device->group_open_cnt);

	return 0;

err_ischain_open:
	ret_err = is_group_close(groupmgr, group);
	if (ret_err)
		merr("is_group_close is fail(%d)", device, ret_err);
err_group_open:
	return ret;
}

int is_ischain_vra_close(struct is_device_ischain *device,
	struct is_video_ctx *vctx)
{
	int ret = 0;
	struct is_groupmgr *groupmgr;
	struct is_group *group;
	struct is_queue *queue;

	FIMC_BUG(!device);

	groupmgr = device->groupmgr;
	group = &device->group_vra;
	queue = GET_QUEUE(vctx);

	/* for mediaserver dead */
	if (test_bit(IS_GROUP_START, &group->state)) {
		mgwarn("sudden group close", device, group);
		if (!test_bit(IS_ISCHAIN_REPROCESSING, &device->state))
			is_itf_sudden_stop_wrap(device, device->instance, group);
		set_bit(IS_GROUP_REQUEST_FSTOP, &group->state);
		if (test_bit(IS_HAL_DEBUG_SUDDEN_DEAD_DETECT, &sysfs_debug.hal_debug_mode)) {
			msleep(sysfs_debug.hal_debug_delay);
			panic("HAL sudden group close #1");
		}
	}

	if (group->head && test_bit(IS_GROUP_START, &group->head->state)) {
		mgwarn("sudden group close", device, group);
		if (!test_bit(IS_ISCHAIN_REPROCESSING, &device->state))
			is_itf_sudden_stop_wrap(device, device->instance, group);
		set_bit(IS_GROUP_REQUEST_FSTOP, &group->state);
		if (test_bit(IS_HAL_DEBUG_SUDDEN_DEAD_DETECT, &sysfs_debug.hal_debug_mode)) {
			msleep(sysfs_debug.hal_debug_delay);
			panic("HAL sudden group close #2");
		}
	}

	ret = is_ischain_vra_stop(device, queue);
	if (ret)
		merr("is_ischain_vra_stop is fail", device);

	ret = is_ischain_close_wrap(device);
	if (ret)
		merr("is_ischain_close_wrap is fail(%d)", device, ret);

	ret = is_group_close(groupmgr, group);
	if (ret)
		merr("is_group_close is fail", device);

	atomic_dec(&device->group_open_cnt);

	return ret;
}

int is_ischain_vra_s_input(struct is_device_ischain *device,
	u32 stream_type,
	u32 position,
	u32 video_id,
	u32 otf_input,
	u32 stream_leader)
{
	int ret = 0;
	struct is_group *group;
	struct is_groupmgr *groupmgr;

	FIMC_BUG(!device);

	groupmgr = device->groupmgr;
	group = &device->group_vra;

	mdbgd_ischain("%s()\n", device, __func__);

	ret = is_group_init(groupmgr, group, otf_input, video_id, stream_leader);
	if (ret) {
		merr("is_group_init is fail(%d)", device, ret);
		goto p_err;
	}

	ret = is_ischain_init_wrap(device, stream_type, position);
	if (ret) {
		merr("is_ischain_init_wrap is fail(%d)", device, ret);
		goto p_err;
	}

p_err:
	return ret;
}

static int is_ischain_vra_start(void *qdevice,
	struct is_queue *queue)
{
	int ret = 0;
	struct is_device_ischain *device = qdevice;
	struct is_groupmgr *groupmgr;
	struct is_group *group;

	FIMC_BUG(!device);

	groupmgr = device->groupmgr;
	group = &device->group_vra;

	ret = is_group_start(groupmgr, group);
	if (ret) {
		merr("is_group_start is fail(%d)", device, ret);
		goto p_err;
	}

	ret = is_ischain_start_wrap(device, group);
	if (ret) {
		merr("is_ischain_start_wrap is fail(%d)", device, ret);
		goto p_err;
	}

p_err:
	return ret;
}

static int is_ischain_vra_stop(void *qdevice,
	struct is_queue *queue)
{
	int ret = 0;
	struct is_device_ischain *device = qdevice;
	struct is_groupmgr *groupmgr;
	struct is_group *group;

	FIMC_BUG(!device);

	groupmgr = device->groupmgr;
	group = &device->group_vra;

	if (!test_bit(IS_GROUP_INIT, &group->state))
		goto p_err;

	ret = is_group_stop(groupmgr, group);
	if (ret) {
		if (ret == -EPERM)
			ret = 0;
		else
			merr("is_group_stop is fail(%d)", device, ret);
		goto p_err;
	}

	ret = is_ischain_stop_wrap(device, group);
	if (ret) {
		merr("is_ischain_stop_wrap is fail(%d)", device, ret);
		goto p_err;
	}

p_err:
	mginfo("%s(%d):%d\n", device, group,  __func__, atomic_read(&group->scount), ret);
	return ret;
}

static int is_ischain_vra_reqbufs(void *qdevice,
	struct is_queue *queue, u32 count)
{
	return 0;
}

static int is_ischain_vra_s_format(void *qdevice,
	struct is_queue *queue)
{
	int ret = 0;
	struct is_device_ischain *device = qdevice;
	struct is_subdev *leader;

	FIMC_BUG(!device);
	FIMC_BUG(!queue);

	leader = &device->group_vra.leader;

	leader->input.width = queue->framecfg.width;
	leader->input.height = queue->framecfg.height;

	leader->input.crop.x = 0;
	leader->input.crop.y = 0;
	leader->input.crop.w = leader->input.width;
	leader->input.crop.h = leader->input.height;

	return ret;
}

int is_ischain_vra_buffer_queue(struct is_device_ischain *device,
	struct is_queue *queue,
	u32 index)
{
	int ret = 0;
	struct is_groupmgr *groupmgr;
	struct is_group *group;

	FIMC_BUG(!device);
	FIMC_BUG(!test_bit(IS_ISCHAIN_OPEN, &device->state));

	mdbgs_ischain(4, "%s\n", device, __func__);

	groupmgr = device->groupmgr;
	group = &device->group_vra;

	ret = is_group_buffer_queue(groupmgr, group, queue, index);
	if (ret) {
		merr("is_group_buffer_queue is fail(%d)", device, ret);
		goto p_err;
	}

p_err:
	return ret;
}

int is_ischain_vra_buffer_finish(struct is_device_ischain *device,
	u32 index)
{
	int ret = 0;
	struct is_groupmgr *groupmgr;
	struct is_group *group;

	FIMC_BUG(!device);

	mdbgs_ischain(4, "%s\n", device, __func__);

	groupmgr = device->groupmgr;
	group = &device->group_vra;

	ret = is_group_buffer_finish(groupmgr, group, index);
	if (ret)
		merr("is_group_buffer_finish is fail(%d)", device, ret);

	return ret;
}

const struct is_queue_ops is_ischain_vra_ops = {
	.start_streaming	= is_ischain_vra_start,
	.stop_streaming		= is_ischain_vra_stop,
	.s_format		= is_ischain_vra_s_format,
	.request_bufs		= is_ischain_vra_reqbufs
};

int is_ischain_clh_open(struct is_device_ischain *device,
	struct is_video_ctx *vctx)
{
	int ret = 0;
	int ret_err = 0;
	struct is_groupmgr *groupmgr;
	struct is_group *group;

	FIMC_BUG(!device);
	FIMC_BUG(!vctx);
	FIMC_BUG(!GET_VIDEO(vctx));

	groupmgr = device->groupmgr;
	group = &device->group_clh;

	ret = is_group_open(groupmgr,
		group,
		GROUP_ID_CLH0,
		vctx);
	if (ret) {
		merr("is_group_open is fail(%d)", device, ret);
		goto err_group_open;
	}

	ret = is_ischain_open_wrap(device, false);
	if (ret) {
		merr("is_ischain_open_wrap is fail(%d)", device, ret);
		goto err_ischain_open;
	}

	atomic_inc(&device->group_open_cnt);

	return 0;

err_ischain_open:
	ret_err = is_group_close(groupmgr, group);
	if (ret_err)
		merr("is_group_close is fail(%d)", device, ret_err);
err_group_open:
	return ret;
}

int is_ischain_clh_close(struct is_device_ischain *device,
	struct is_video_ctx *vctx)
{
	int ret = 0;
	struct is_groupmgr *groupmgr;
	struct is_group *group;
	struct is_queue *queue;

	FIMC_BUG(!device);

	groupmgr = device->groupmgr;
	group = &device->group_clh;
	queue = GET_QUEUE(vctx);

	/* for mediaserver dead */
	if (test_bit(IS_GROUP_START, &group->state)) {
		mgwarn("sudden group close", device, group);
		if (!test_bit(IS_ISCHAIN_REPROCESSING, &device->state))
			is_itf_sudden_stop_wrap(device, device->instance, group);
		set_bit(IS_GROUP_REQUEST_FSTOP, &group->state);
		if (test_bit(IS_HAL_DEBUG_SUDDEN_DEAD_DETECT, &sysfs_debug.hal_debug_mode)) {
			msleep(sysfs_debug.hal_debug_delay);
			panic("HAL sudden group close #1");
		}
	}

	if (group->head && test_bit(IS_GROUP_START, &group->head->state)) {
		mgwarn("sudden group close", device, group);
		if (!test_bit(IS_ISCHAIN_REPROCESSING, &device->state))
			is_itf_sudden_stop_wrap(device, device->instance, group);
		set_bit(IS_GROUP_REQUEST_FSTOP, &group->state);
		if (test_bit(IS_HAL_DEBUG_SUDDEN_DEAD_DETECT, &sysfs_debug.hal_debug_mode)) {
			msleep(sysfs_debug.hal_debug_delay);
			panic("HAL sudden group close #2");
		}
	}

	ret = is_ischain_clh_stop(device, queue);
	if (ret)
		merr("is_ischain_clh_stop is fail", device);

	ret = is_ischain_close_wrap(device);
	if (ret)
		merr("is_ischain_close_wrap is fail(%d)", device, ret);

	ret = is_group_close(groupmgr, group);
	if (ret)
		merr("is_group_close is fail", device);

	atomic_dec(&device->group_open_cnt);

	return ret;
}

int is_ischain_clh_s_input(struct is_device_ischain *device,
	u32 stream_type,
	u32 position,
	u32 video_id,
	u32 input_type,
	u32 stream_leader)
{
	int ret = 0;
	struct is_group *group;
	struct is_groupmgr *groupmgr;

	FIMC_BUG(!device);

	groupmgr = device->groupmgr;
	group = &device->group_clh;

	mdbgd_ischain("%s()\n", device, __func__);

	ret = is_group_init(groupmgr, group, input_type, video_id, stream_leader);
	if (ret) {
		merr("is_group_init is fail(%d)", device, ret);
		goto p_err;
	}

	ret = is_ischain_init_wrap(device, stream_type, position);
	if (ret) {
		merr("is_ischain_init_wrap is fail(%d)", device, ret);
		goto p_err;
	}

p_err:
	return ret;
}

static int is_ischain_clh_start(void *qdevice,
	struct is_queue *queue)
{
	int ret = 0;
	struct is_device_ischain *device = qdevice;
	struct is_groupmgr *groupmgr;
	struct is_group *group;

	FIMC_BUG(!device);

	groupmgr = device->groupmgr;
	group = &device->group_clh;

	ret = is_group_start(groupmgr, group);
	if (ret) {
		merr("is_group_start is fail(%d)", device, ret);
		goto p_err;
	}

	ret = is_ischain_start_wrap(device, group);
	if (ret) {
		merr("is_ischain_start_wrap is fail(%d)", device, ret);
		goto p_err;
	}

p_err:
	return ret;
}

static int is_ischain_clh_stop(void *qdevice,
	struct is_queue *queue)
{
	int ret = 0;
	struct is_device_ischain *device = qdevice;
	struct is_groupmgr *groupmgr;
	struct is_group *group;

	FIMC_BUG(!device);

	groupmgr = device->groupmgr;
	group = &device->group_clh;

	if (!test_bit(IS_GROUP_INIT, &group->state))
		goto p_err;

	ret = is_group_stop(groupmgr, group);
	if (ret) {
		if (ret == -EPERM)
			ret = 0;
		else
			merr("is_group_stop is fail(%d)", device, ret);
		goto p_err;
	}

	ret = is_ischain_stop_wrap(device, group);
	if (ret) {
		merr("is_ischain_stop_wrap is fail(%d)", device, ret);
		goto p_err;
	}

p_err:
	mginfo("%s(%d):%d\n", device, group,  __func__, atomic_read(&group->scount), ret);
	return ret;
}

static int is_ischain_clh_reqbufs(void *qdevice,
	struct is_queue *queue, u32 count)
{
	return 0;
}

static int is_ischain_clh_s_format(void *qdevice,
	struct is_queue *queue)
{
	int ret = 0;
	struct is_device_ischain *device = qdevice;
	struct is_subdev *leader;

	FIMC_BUG(!device);
	FIMC_BUG(!queue);

	leader = &device->group_clh.leader;

	leader->input.width = queue->framecfg.width;
	leader->input.height = queue->framecfg.height;

	leader->input.crop.x = 0;
	leader->input.crop.y = 0;
	leader->input.crop.w = leader->input.width;
	leader->input.crop.h = leader->input.height;

	return ret;
}

int is_ischain_clh_buffer_queue(struct is_device_ischain *device,
	struct is_queue *queue,
	u32 index)
{
	int ret = 0;
	struct is_groupmgr *groupmgr;
	struct is_group *group;

	FIMC_BUG(!device);
	FIMC_BUG(!test_bit(IS_ISCHAIN_OPEN, &device->state));

	mdbgs_ischain(4, "%s\n", device, __func__);

	groupmgr = device->groupmgr;
	group = &device->group_clh;

	ret = is_group_buffer_queue(groupmgr, group, queue, index);
	if (ret) {
		merr("is_group_buffer_queue is fail(%d)", device, ret);
		goto p_err;
	}

p_err:
	return ret;
}

int is_ischain_clh_buffer_finish(struct is_device_ischain *device,
	u32 index)
{
	int ret = 0;
	struct is_groupmgr *groupmgr;
	struct is_group *group;

	FIMC_BUG(!device);

	mdbgs_ischain(4, "%s\n", device, __func__);

	groupmgr = device->groupmgr;
	group = &device->group_clh;

	ret = is_group_buffer_finish(groupmgr, group, index);
	if (ret)
		merr("is_group_buffer_finish is fail(%d)", device, ret);

	return ret;
}

const struct is_queue_ops is_ischain_clh_ops = {
	.start_streaming	= is_ischain_clh_start,
	.stop_streaming		= is_ischain_clh_stop,
	.s_format		= is_ischain_clh_s_format,
	.request_bufs		= is_ischain_clh_reqbufs
};


static int is_ischain_paf_group_tag(struct is_device_ischain *device,
	struct is_frame *frame,
	struct camera2_node *ldr_node)
{
	int ret = 0;
	struct is_group *group;

	group = &device->group_paf;

	ret = CALL_SOPS(&group->leader, tag, device, frame, ldr_node);
	if (ret) {
		merr("is_ischain_paf_tag is fail(%d)", device, ret);
		goto p_err;
	}

p_err:
	return ret;
}

static int is_ischain_3aa_group_tag(struct is_device_ischain *device,
	struct is_frame *frame,
	struct camera2_node *ldr_node)
{
	int ret = 0;
	u32 capture_id;
	struct is_group *group;
	struct is_subdev *subdev;
	struct camera2_node_group *node_group;
	struct camera2_node *cap_node;

	group = &device->group_3aa;
	node_group = &frame->shot_ext->node_group;

	ret = CALL_SOPS(&group->leader, tag, device, frame, ldr_node);
	if (ret) {
		merr("is_ischain_3aa_tag is fail(%d)", device, ret);
		goto p_err;
	}

	for (capture_id = 0; capture_id < CAPTURE_NODE_MAX; ++capture_id) {
		cap_node = &node_group->capture[capture_id];
		subdev = NULL;
		switch (cap_node->vid) {
		case 0:
			break;
		case IS_VIDEO_30C_NUM:
		case IS_VIDEO_31C_NUM:
		case IS_VIDEO_32C_NUM:
			subdev = group->subdev[ENTRY_3AC];
			if (subdev && test_bit(IS_SUBDEV_START, &subdev->state)) {
				ret = CALL_SOPS(subdev, tag, device, frame, cap_node);
				if (ret) {
					merr("is_ischain_3ac_tag is fail(%d)", device, ret);
					goto p_err;
				}
			}
			break;
		case IS_VIDEO_30P_NUM:
		case IS_VIDEO_31P_NUM:
		case IS_VIDEO_32P_NUM:
			subdev = group->subdev[ENTRY_3AP];
			if (subdev && test_bit(IS_SUBDEV_START, &subdev->state)) {
				ret = CALL_SOPS(subdev, tag, device, frame, cap_node);
				if (ret) {
					merr("is_ischain_3ap_tag is fail(%d)", device, ret);
					goto p_err;
				}
			}
			break;
		case IS_VIDEO_30F_NUM:
		case IS_VIDEO_31F_NUM:
		case IS_VIDEO_32F_NUM:
			subdev = group->subdev[ENTRY_3AF];
			if (subdev && test_bit(IS_SUBDEV_START, &subdev->state)) {
				ret = CALL_SOPS(subdev, tag, device, frame, cap_node);
				if (ret) {
					merr("is_ischain_3af_tag is fail(%d)", device, ret);
					goto p_err;
				}
			}
			break;
		case IS_VIDEO_30G_NUM:
		case IS_VIDEO_31G_NUM:
		case IS_VIDEO_32G_NUM:
			subdev = group->subdev[ENTRY_3AG];
			if (subdev && test_bit(IS_SUBDEV_START, &subdev->state)) {
				ret = CALL_SOPS(subdev, tag, device, frame, cap_node);
				if (ret) {
					merr("is_ischain_3ag_tag is fail(%d)", device, ret);
					goto p_err;
				}
			}
			break;
		case IS_VIDEO_30O_NUM:
		case IS_VIDEO_31O_NUM:
		case IS_VIDEO_32O_NUM:
		case IS_VIDEO_33O_NUM:
			subdev = group->subdev[ENTRY_3AO];
			if (subdev && test_bit(IS_SUBDEV_START, &subdev->state)) {
				ret = CALL_SOPS(subdev, tag, device, frame, cap_node);
				if (ret) {
					merr("is_ischain_3ao_tag is fail(%d)", device, ret);
					goto p_err;
				}
			}
			break;
		case IS_VIDEO_30L_NUM:
		case IS_VIDEO_31L_NUM:
		case IS_VIDEO_32L_NUM:
		case IS_VIDEO_33L_NUM:
			subdev = group->subdev[ENTRY_3AL];
			if (subdev && test_bit(IS_SUBDEV_START, &subdev->state)) {
				ret = CALL_SOPS(subdev, tag, device, frame, cap_node);
				if (ret) {
					merr("is_ischain_3al_tag is fail(%d)", device, ret);
					goto p_err;
				}
			}
			break;
		case IS_VIDEO_ME0C_NUM:
		case IS_VIDEO_ME1C_NUM:
			subdev = group->subdev[ENTRY_MEXC];
			if (subdev && test_bit(IS_SUBDEV_START, &subdev->state)) {
				ret = CALL_SOPS(subdev, tag, device, frame, cap_node);
				if (ret) {
					merr("is_ischain_mexc_tag is fail(%d)", device, ret);
					goto p_err;
				}
			}
			break;
		case IS_VIDEO_ORB0C_NUM:
		case IS_VIDEO_ORB1C_NUM:
			subdev = group->subdev[ENTRY_ORBXC];
			if (subdev && test_bit(IS_SUBDEV_START, &subdev->state)) {
				ret = CALL_SOPS(subdev, tag, device, frame, cap_node);
				if (ret) {
					merr("is_ischain_orbxc_tag is fail(%d)", device, ret);
					goto p_err;
				}
			}
			break;
		default:
			break;
		}
	}
p_err:
	return ret;
}

static int is_ischain_isp_group_tag(struct is_device_ischain *device,
	struct is_frame *frame,
	struct camera2_node *ldr_node)
{
	int ret = 0;
	u32 capture_id;
	struct is_group *group;
	struct is_subdev *subdev, *vra;
	struct camera2_node_group *node_group;
	struct camera2_node *cap_node;

	group = &device->group_isp;
	vra = group->subdev[ENTRY_VRA];
	node_group = &frame->shot_ext->node_group;

#ifdef ENABLE_VRA
	if (vra) {
		if (frame->shot_ext->fd_bypass) {
			if (test_bit(IS_SUBDEV_RUN, &vra->state)) {
				ret = is_ischain_vra_bypass(device, frame, true);
				if (ret) {
					merr("fd_bypass(1) is fail", device);
					goto p_err;
				}
			}
		} else {
			if (!test_bit(IS_SUBDEV_RUN, &vra->state)) {
				ret = is_ischain_vra_bypass(device, frame, false);
				if (ret) {
					merr("fd_bypass(0) is fail", device);
					goto p_err;
				}
			}
		}
	}
#endif

	ret = CALL_SOPS(&group->leader, tag, device, frame, ldr_node);
	if (ret) {
		merr("is_ischain_isp_tag is fail(%d)", device, ret);
		goto p_err;
	}

	for (capture_id = 0; capture_id < CAPTURE_NODE_MAX; ++capture_id) {
		cap_node = &node_group->capture[capture_id];
		subdev = NULL;

		switch (cap_node->vid) {
		case 0:
			break;
		case IS_VIDEO_I0C_NUM:
		case IS_VIDEO_I1C_NUM:
			subdev = group->subdev[ENTRY_IXC];
			if (subdev && test_bit(IS_SUBDEV_START, &subdev->state)) {
				ret = CALL_SOPS(subdev, tag, device, frame, cap_node);
				if (ret) {
					merr("is_ischain_ixc_tag is fail(%d)", device, ret);
					goto p_err;
				}
			}
			break;
		case IS_VIDEO_I0P_NUM:
		case IS_VIDEO_I1P_NUM:
			subdev = group->subdev[ENTRY_IXP];
			if (subdev && test_bit(IS_SUBDEV_START, &subdev->state)) {
				ret = CALL_SOPS(subdev, tag, device, frame, cap_node);
				if (ret) {
					merr("is_ischain_ixp_tag is fail(%d)", device, ret);
					goto p_err;
				}
			}
			break;
		case IS_VIDEO_I0T_NUM:
			subdev = group->subdev[ENTRY_IXT];
			if (subdev && test_bit(IS_SUBDEV_START, &subdev->state)) {
				ret = CALL_SOPS(subdev, tag, device, frame, cap_node);
				if (ret) {
					merr("is_ischain_ixt_tag is fail(%d)", device, ret);
					goto p_err;
				}
			}
			break;
		case IS_VIDEO_I0G_NUM:
			subdev = group->subdev[ENTRY_IXG];
			if (subdev && test_bit(IS_SUBDEV_START, &subdev->state)) {
				ret = CALL_SOPS(subdev, tag, device, frame, cap_node);
				if (ret) {
					merr("is_ischain_ixg_tag is fail(%d)", device, ret);
					goto p_err;
				}
			}
			break;
		case IS_VIDEO_I0V_NUM:
			subdev = group->subdev[ENTRY_IXV];
			if (subdev && test_bit(IS_SUBDEV_START, &subdev->state)) {
				ret = CALL_SOPS(subdev, tag, device, frame, cap_node);
				if (ret) {
					merr("is_ischain_ixv_tag is fail(%d)", device, ret);
					goto p_err;
				}
			}
			break;
		case IS_VIDEO_I0W_NUM:
			subdev = group->subdev[ENTRY_IXW];
			if (subdev && test_bit(IS_SUBDEV_START, &subdev->state)) {
				ret = CALL_SOPS(subdev, tag, device, frame, cap_node);
				if (ret) {
					merr("is_ischain_ixw_tag is fail(%d)", device, ret);
					goto p_err;
				}
			}
			break;
		case IS_VIDEO_ME0C_NUM:
		case IS_VIDEO_ME1C_NUM:
			subdev = group->subdev[ENTRY_MEXC];
			if (subdev && test_bit(IS_SUBDEV_START, &subdev->state)) {
				ret = CALL_SOPS(subdev, tag, device, frame, cap_node);
				if (ret) {
					merr("is_ischain_mexc_tag is fail(%d)", device, ret);
					goto p_err;
				}
			}
			break;
		default:
			break;
		}
	}

p_err:
	return ret;
}

static int is_ischain_ypp_group_tag(struct is_device_ischain *device,
	struct is_frame *frame,
	struct camera2_node *ldr_node)
{
	int ret = 0;
	struct is_group *group;

	group = &device->group_ypp;

	ret = CALL_SOPS(&group->leader, tag, device, frame, ldr_node);
	if (ret) {
		merr("is_ischain_ypp_tag is fail(%d)", device, ret);
		goto p_err;
	}

p_err:
	return ret;
}

static int is_ischain_mcs_group_tag(struct is_device_ischain *device,
	struct is_frame *frame,
	struct camera2_node *ldr_node)
{
	int ret = 0;
	u32 capture_id;
	struct is_group *group;
	struct is_group *head;
	struct is_subdev *subdev, *vra;
	struct camera2_node_group *node_group;
	struct camera2_node *cap_node;

	group = &device->group_mcs;
	head = GET_HEAD_GROUP_IN_DEVICE(IS_DEVICE_ISCHAIN, group);
	vra = group->subdev[ENTRY_VRA];
	node_group = &frame->shot_ext->node_group;

#ifdef ENABLE_VRA
	if (vra) {
		if (frame->shot_ext->fd_bypass) {
			if (test_bit(IS_SUBDEV_RUN, &vra->state)) {
				ret = CALL_SOPS(&group->leader, bypass, device, frame, true);
				if (ret) {
					merr("mcs_bypass(1) is fail(%d)", device, ret);
					goto p_err;
				}
				ret = is_ischain_vra_bypass(device, frame, true);
				if (ret) {
					merr("fd_bypass(1) is fail", device);
					goto p_err;
				}
			}
		} else {
			if (!test_bit(IS_SUBDEV_RUN, &vra->state)) {
				ret = CALL_SOPS(&group->leader, bypass, device, frame, false);
				if (ret) {
					merr("mcs_bypass(0) is fail(%d)", device, ret);
					goto p_err;
				}
				ret = is_ischain_vra_bypass(device, frame, false);
				if (ret) {
					merr("fd_bypass(0) is fail", device);
					goto p_err;
				}
			}
		}
	}
#endif

	/* At Full-OTF case(Head group is OTF),
	 * set next_noise_idx for next frame applying.
	 *
	 * At DMA input case, set cur_noise_idx for current frame appling.
	 */
	if (test_bit(IS_GROUP_OTF_INPUT, &head->state)) {
		frame->noise_idx = device->next_noise_idx[frame->fcount % NI_BACKUP_MAX];
		/* clear back up NI value */
		device->next_noise_idx[frame->fcount % NI_BACKUP_MAX] = 0xFFFFFFFF;
	} else {
		frame->noise_idx = device->cur_noise_idx[frame->fcount % NI_BACKUP_MAX];
		/* clear back up NI value */
		device->cur_noise_idx[frame->fcount % NI_BACKUP_MAX] = 0xFFFFFFFF;
	}

	ret = CALL_SOPS(&group->leader, tag, device, frame, ldr_node);
	if (ret) {
		merr("is_ischain_mcs_tag is fail(%d)", device, ret);
		goto p_err;
	}

	for (capture_id = 0; capture_id < CAPTURE_NODE_MAX; ++capture_id) {
		cap_node = &node_group->capture[capture_id];
		subdev = NULL;

		switch (cap_node->vid) {
		case 0:
			break;
		case IS_VIDEO_M0P_NUM:
			subdev = group->subdev[ENTRY_M0P];
			if (subdev && test_bit(IS_SUBDEV_START, &subdev->state)) {
				ret = CALL_SOPS(subdev, tag, device, frame, cap_node);
				if (ret) {
					merr("is_ischain_mxp_tag is fail(%d)", device, ret);
					goto p_err;
				}
			}
			break;
		case IS_VIDEO_M1P_NUM:
			subdev = group->subdev[ENTRY_M1P];
			if (subdev && test_bit(IS_SUBDEV_START, &subdev->state)) {
				ret = CALL_SOPS(subdev, tag, device, frame, cap_node);
				if (ret) {
					merr("is_ischain_mxp_tag is fail(%d)", device, ret);
					goto p_err;
				}
			}
			break;
		case IS_VIDEO_M2P_NUM:
			subdev = group->subdev[ENTRY_M2P];
			if (subdev && test_bit(IS_SUBDEV_START, &subdev->state)) {
				ret = CALL_SOPS(subdev, tag, device, frame, cap_node);
				if (ret) {
					merr("is_ischain_mxp_tag is fail(%d)", device, ret);
					goto p_err;
				}
			}
			break;
		case IS_VIDEO_M3P_NUM:
			subdev = group->subdev[ENTRY_M3P];
			if (subdev && test_bit(IS_SUBDEV_START, &subdev->state)) {
				ret = CALL_SOPS(subdev, tag, device, frame, cap_node);
				if (ret) {
					merr("is_ischain_mxp_tag is fail(%d)", device, ret);
					goto p_err;
				}
			}
			break;
		case IS_VIDEO_M4P_NUM:
			subdev = group->subdev[ENTRY_M4P];
			if (subdev && test_bit(IS_SUBDEV_START, &subdev->state)) {
				ret = CALL_SOPS(subdev, tag, device, frame, cap_node);
				if (ret) {
					merr("is_ischain_mxp_tag is fail(%d)", device, ret);
					goto p_err;
				}
			}
			break;
		case IS_VIDEO_M5P_NUM:
			subdev = group->subdev[ENTRY_M5P];
			if (subdev && test_bit(IS_SUBDEV_START, &subdev->state)) {
				ret = CALL_SOPS(subdev, tag, device, frame, cap_node);
				if (ret) {
					merr("is_ischain_mxp_tag is fail(%d)", device, ret);
					goto p_err;
				}
			}
			break;
		default:
			break;
		}
	}

p_err:
	return ret;
}

static int is_ischain_vra_group_tag(struct is_device_ischain *device,
	struct is_frame *frame,
	struct camera2_node *ldr_node)
{
	int ret = 0;
	struct is_group *group;
	struct is_subdev *vra;

	group = &device->group_vra;
	vra = &group->leader;

#ifdef ENABLE_VRA
	if (vra) {
		if (frame->shot_ext->fd_bypass) {
			if (test_bit(IS_SUBDEV_RUN, &vra->state)) {
				ret = is_ischain_vra_bypass(device, frame, true);
				if (ret) {
					merr("fd_bypass(1) is fail", device);
					goto p_err;
				}
			}
		} else {
			if (!test_bit(IS_SUBDEV_RUN, &vra->state)) {
				ret = is_ischain_vra_bypass(device, frame, false);
				if (ret) {
					merr("fd_bypass(0) is fail", device);
					goto p_err;
				}
			}
		}
	}
#endif

	ret = CALL_SOPS(&group->leader, tag, device, frame, ldr_node);
	 if (ret) {
			 merr("is_ischain_vra_tag fail(%d)", device, ret);
			 goto p_err;
	 }

p_err:
	return ret;
}

static int is_ischain_clh_group_tag(struct is_device_ischain *device,
	struct is_frame *frame,
	struct camera2_node *ldr_node)
{
	int ret = 0;
	u32 capture_id;
	struct is_group *group;
	struct is_subdev *subdev;
	struct camera2_node_group *node_group;
	struct camera2_node *cap_node;

	group = &device->group_clh;
	node_group = &frame->shot_ext->node_group;

	ret = CALL_SOPS(&group->leader, tag, device, frame, ldr_node);
	if (ret) {
		merr("is_ischain_isp_tag is fail(%d)", device, ret);
		goto p_err;
	}

	for (capture_id = 0; capture_id < CAPTURE_NODE_MAX; ++capture_id) {
		cap_node = &node_group->capture[capture_id];
		subdev = NULL;

		switch (cap_node->vid) {
		case 0:
			break;
		case IS_VIDEO_CLH0C_NUM:
			subdev = group->subdev[ENTRY_CLHC];
			if (subdev && test_bit(IS_SUBDEV_START, &subdev->state)) {
				ret = CALL_SOPS(subdev, tag, device, frame, cap_node);
				if (ret) {
					merr("is_ischain_clhc_tag is fail(%d)", device, ret);
					goto p_err;
				}
			}
			break;
		default:
			break;
		}
	}

p_err:
	return ret;
}

static int is_ischain_lme_group_tag(struct is_device_ischain *device,
	struct is_frame *frame,
	struct camera2_node *ldr_node)
{
	int ret = 0;
	u32 capture_id;
	struct is_group *group;
	struct is_subdev *subdev;
	struct camera2_node_group *node_group;
	struct camera2_node *cap_node;

	group = &device->group_lme;
	node_group = &frame->shot_ext->node_group;

	ret = CALL_SOPS(&group->leader, tag, device, frame, ldr_node);
	if (ret) {
		merr("is_ischain_isp_tag is fail(%d)", device, ret);
		goto p_err;
	}

	for (capture_id = 0; capture_id < CAPTURE_NODE_MAX; ++capture_id) {
		cap_node = &node_group->capture[capture_id];
		subdev = NULL;
		switch (cap_node->vid) {
		case 0:
			break;
		case IS_VIDEO_LME0S_NUM:
		case IS_VIDEO_LME1S_NUM:
			subdev = group->subdev[ENTRY_LMES];
			if (subdev && test_bit(IS_SUBDEV_START, &subdev->state)) {
				ret = CALL_SOPS(subdev, tag, device, frame, cap_node);
				if (ret) {
					merr("is_ischain_lmes_tag is fail(%d)", device, ret);
					goto p_err;
				}
			}
			break;
		case IS_VIDEO_LME0C_NUM:
		case IS_VIDEO_LME1C_NUM:
			subdev = group->subdev[ENTRY_LMEC];
			if (subdev && test_bit(IS_SUBDEV_START, &subdev->state)) {
				ret = CALL_SOPS(subdev, tag, device, frame, cap_node);
				if (ret) {
					merr("is_ischain_lmec_tag is fail(%d)", device, ret);
					goto p_err;
				}
			}
			break;
		default:
			break;
		}
	}

p_err:
	return ret;
}

static void is_ischain_update_setfile(struct is_device_ischain *device,
	struct is_group *group,
	struct is_frame *frame)
{
	int ret;
	bool update_condition = false;
	u32 setfile_save = 0;

	/* setfile update will do at below conditions.
	 * 1. Preview & leader group shot.
	 * 2. Reprocessing & First group that attempt to shot.
	 */
	if (!test_bit(IS_ISCHAIN_REPROCESSING, &device->state)) {
		if (group->id == get_ischain_leader_group(device)->id)
			update_condition = true;
	} else {
		if (frame->fcount != device->apply_setfile_fcount)
			update_condition = true;
	}

	if ((frame->shot_ext->setfile != device->setfile) && update_condition) {
		setfile_save = device->setfile;
		device->setfile = frame->shot_ext->setfile;
		device->apply_setfile_fcount = frame->fcount;

		mgrinfo(" setfile change at shot(%d -> %d)\n", device, group, frame,
			setfile_save, device->setfile & IS_SETFILE_MASK);

		if (test_bit(IS_ISCHAIN_REPROCESSING, &device->state)) {
			ret = is_ischain_chg_setfile(device);
			if (ret) {
				merr("is_ischain_chg_setfile is fail", device);
				device->setfile = setfile_save;
			}
		}
	}
}

static void is_ischain_update_shot(struct is_device_ischain *device,
	struct is_frame *frame)
{
	struct fast_control_mgr *fastctlmgr = &device->fastctlmgr;
	struct is_fast_ctl *fast_ctl = NULL;
	struct camera2_shot *shot = frame->shot;
	unsigned long flags;
	u32 state;

#ifdef ENABLE_ULTRA_FAST_SHOT
	if (device->fastctlmgr.fast_capture_count) {
		device->fastctlmgr.fast_capture_count--;
		frame->shot->ctl.aa.captureIntent = AA_CAPTURE_INTENT_PREVIEW;
		mrinfo("captureIntent update\n", device, frame);
	}
#endif

	spin_lock_irqsave(&fastctlmgr->slock, flags);

	state = IS_FAST_CTL_REQUEST;
	if (fastctlmgr->queued_count[state]) {
		/* get req list */
		fast_ctl = list_first_entry(&fastctlmgr->queued_list[state],
			struct is_fast_ctl, list);
		list_del(&fast_ctl->list);
		fastctlmgr->queued_count[state]--;

		/* Read fast_ctl: lens */
		if (fast_ctl->lens_pos_flag) {
			shot->uctl.lensUd.pos = fast_ctl->lens_pos;
			shot->uctl.lensUd.posSize = 12; /* fixed value: bit size(12 bit) */
			shot->uctl.lensUd.direction = 0; /* fixed value: 0(infinite), 1(macro) */
			shot->uctl.lensUd.slewRate = 0; /* fixed value: only DW actuator speed */
			shot->ctl.aa.afMode = AA_AFMODE_OFF;
			fast_ctl->lens_pos_flag = false;
		}

		/* set free list */
		state = IS_FAST_CTL_FREE;
		fast_ctl->state = state;
		list_add_tail(&fast_ctl->list, &fastctlmgr->queued_list[state]);
		fastctlmgr->queued_count[state]++;
	}

	spin_unlock_irqrestore(&fastctlmgr->slock, flags);

	if (fast_ctl)
		mrinfo("fast_ctl: uctl.lensUd.pos(%d)\n", device, frame, shot->uctl.lensUd.pos);
}

#if defined(USE_OFFLINE_PROCESSING)
int is_ischain_update_sensor_mode(struct is_device_ischain *device,
	struct is_frame *frame)
{
	int rep_flag = 0;
	int ret = 0;
	struct param_sensor_config *sensor_config;
	u32 hindex = 0, lindex = 0, indexes;

	rep_flag = test_bit(IS_ISCHAIN_REPROCESSING, &device->state);

	if (rep_flag) {
		sensor_config = is_itf_g_param(device, NULL, PARAM_SENSOR_CONFIG);
		sensor_config->width = frame->shot_ext->user.sensor_mode.width;
		sensor_config->height = frame->shot_ext->user.sensor_mode.height;
		sensor_config->calibrated_width = frame->shot_ext->user.sensor_mode.width;
		sensor_config->calibrated_height = frame->shot_ext->user.sensor_mode.height;
		sensor_config->sensor_binning_ratio_x = frame->shot_ext->user.sensor_mode.binning;
		sensor_config->sensor_binning_ratio_y = frame->shot_ext->user.sensor_mode.binning;
		sensor_config->bns_binning_ratio_x = 1000;
		sensor_config->bns_binning_ratio_y = 1000;
		sensor_config->bns_margin_left = 0;
		sensor_config->bns_margin_top = 0;
		sensor_config->bns_output_width = frame->shot_ext->user.sensor_mode.width;
		sensor_config->bns_output_height = frame->shot_ext->user.sensor_mode.height;
		sensor_config->frametime = 10 * 1000 * 1000; /*  max exposure time */
		sensor_config->min_target_fps = 0;
		sensor_config->max_target_fps = 0;

		lindex |= LOWBIT_OF(PARAM_SENSOR_CONFIG);
		hindex |= HIGHBIT_OF(PARAM_SENSOR_CONFIG);

		ret = is_itf_s_param_wrap(device, lindex, hindex, indexes);
		if (ret)
			merr("is_itf_s_param_wrap is fail(%d)", device, ret);

		device->binning = frame->shot_ext->user.sensor_mode.binning;
	} else {
		frame->shot_ext->user.sensor_mode.width = is_sensor_g_width(device->sensor);
		frame->shot_ext->user.sensor_mode.height = is_sensor_g_height(device->sensor);;
		frame->shot_ext->user.sensor_mode.binning = is_sensor_g_bratio(device->sensor);
		frame->shot_ext->user.sensor_mode.ex_mode = is_sensor_g_ex_mode(device->sensor);
	}

	return ret;
}
#endif

static int is_ischain_paf_shot(struct is_device_ischain *device,
	struct is_frame *check_frame)
{
	int ret = 0;
	unsigned long flags;
	struct is_resourcemgr *resourcemgr;
	struct is_group *group, *child, *vra;
	struct is_framemgr *framemgr;
	struct is_frame *frame;
	struct camera2_node_group *node_group;
	struct camera2_node ldr_node = {0, };
	enum is_frame_state next_state = FS_PROCESS;

	FIMC_BUG(!device);
	FIMC_BUG(!check_frame);

	mdbgs_ischain(4, "%s()\n", device, __func__);
	resourcemgr = device->resourcemgr;
	group = &device->group_paf;
	frame = NULL;

	framemgr = GET_HEAD_GROUP_FRAMEMGR(group, check_frame->cur_shot_idx);
	if (!framemgr) {
		merr("framemgr is NULL", device);
		ret = -EINVAL;
		goto framemgr_err;
	}

	frame = peek_frame(framemgr, check_frame->state);

	if (unlikely(!frame)) {
		merr("frame is NULL", device);
		ret = -EINVAL;
		goto frame_err;
	}

	if (unlikely(frame != check_frame)) {
		merr("frame checking is fail(%p != %p)", device, frame, check_frame);
		ret = -EINVAL;
		goto p_err;
	}

	if (unlikely(!frame->shot)) {
		merr("frame->shot is NULL", device);
		ret = -EINVAL;
		goto p_err;
	}

	frame->shot->ctl.vendor_entry.lowIndexParam = 0;
	frame->shot->ctl.vendor_entry.highIndexParam = 0;
	frame->shot->dm.vendor_entry.lowIndexParam = 0;
	frame->shot->dm.vendor_entry.highIndexParam = 0;
	node_group = &frame->shot_ext->node_group;

	PROGRAM_COUNT(8);

	is_ischain_update_setfile(device, group, frame);

	if (test_bit(IS_GROUP_OTF_INPUT, &group->state)) {
		enum aa_capture_intent captureIntent;
		captureIntent = group->intent_ctl.captureIntent;

		if (captureIntent != AA_CAPTURE_INTENT_CUSTOM) {
			if (group->remainIntentCount > 0) {
				frame->shot->ctl.aa.captureIntent = captureIntent;
				frame->shot->ctl.aa.vendor_captureCount = group->intent_ctl.vendor_captureCount;
				frame->shot->ctl.aa.vendor_captureExposureTime = group->intent_ctl.vendor_captureExposureTime;
				frame->shot->ctl.aa.vendor_captureEV = group->intent_ctl.vendor_captureEV;
				if (group->intent_ctl.vendor_isoValue) {
					frame->shot->ctl.aa.vendor_isoMode = AA_ISOMODE_MANUAL;
					frame->shot->ctl.aa.vendor_isoValue = group->intent_ctl.vendor_isoValue;
					frame->shot->ctl.sensor.sensitivity = frame->shot->ctl.aa.vendor_isoValue;
				}
				if (group->intent_ctl.vendor_aeExtraMode) {
					frame->shot->ctl.aa.vendor_aeExtraMode = group->intent_ctl.vendor_aeExtraMode;
				}
				if (group->intent_ctl.aeMode) {
					frame->shot->ctl.aa.aeMode = group->intent_ctl.aeMode;
				}
				memcpy(&(frame->shot->ctl.aa.vendor_multiFrameEvList),
					&(group->intent_ctl.vendor_multiFrameEvList),
					sizeof(group->intent_ctl.vendor_multiFrameEvList));
				memcpy(&(frame->shot->ctl.aa.vendor_multiFrameIsoList),
					&(group->intent_ctl.vendor_multiFrameIsoList),
					sizeof(group->intent_ctl.vendor_multiFrameIsoList));
				memcpy(&(frame->shot->ctl.aa.vendor_multiFrameExposureList),
					&(group->intent_ctl.vendor_multiFrameExposureList),
					sizeof(group->intent_ctl.vendor_multiFrameExposureList));
				group->remainIntentCount--;
			} else {
				group->intent_ctl.captureIntent = AA_CAPTURE_INTENT_CUSTOM;
				group->intent_ctl.vendor_captureCount = 0;
				group->intent_ctl.vendor_captureExposureTime = 0;
				group->intent_ctl.vendor_captureEV = 0;
				memset(&(group->intent_ctl.vendor_multiFrameEvList), 0,
					sizeof(group->intent_ctl.vendor_multiFrameEvList));
				memset(&(group->intent_ctl.vendor_multiFrameIsoList), 0,
					sizeof(group->intent_ctl.vendor_multiFrameIsoList));
				memset(&(group->intent_ctl.vendor_multiFrameExposureList), 0,
					sizeof(group->intent_ctl.vendor_multiFrameExposureList));
				group->intent_ctl.vendor_isoValue = 0;
				group->intent_ctl.vendor_aeExtraMode = AA_AE_EXTRA_MODE_AUTO;
				group->intent_ctl.aeMode = 0;
			}
			minfo("frame count(%d), intent(%d), count(%d) captureExposureTime(%d) remainIntentCount(%d)\n",
				device, frame->fcount,
				frame->shot->ctl.aa.captureIntent, frame->shot->ctl.aa.vendor_captureCount,
				frame->shot->ctl.aa.vendor_captureExposureTime, group->remainIntentCount);
		}

		if (group->remainFlashCtlCount) {
			frame->shot->uctl.flashMode = group->flash_ctl;

			group->remainFlashCtlCount--;
			if (group->remainFlashCtlCount == 0)
				group->flash_ctl = CAMERA_FLASH_MODE_OFF;
		}

		if (frame->shot->ctl.aa.sceneMode == AA_SCENE_MODE_ASTRO) {
			resourcemgr->shot_timeout = SHOT_TIMEOUT * 10; // 30s timeout
		} else if (frame->shot->ctl.aa.sceneMode == AA_SCENE_MODE_HYPERLAPSE
			&& frame->shot->ctl.aa.vendor_currentHyperlapseMode == 300) {
			resourcemgr->shot_timeout = SHOT_TIMEOUT * 5; // 15s timeout
		} else if (frame->shot->ctl.aa.vendor_captureExposureTime >= ((SHOT_TIMEOUT * 1000) - 1000000)) {
			/*
			* Adjust the shot timeout value based on sensor exposure time control.
			* Exposure Time >= (SHOT_TIMEOUT - 1sec): Increase shot timeout value.
			*/
			resourcemgr->shot_timeout = (frame->shot->ctl.aa.vendor_captureExposureTime / 1000) + SHOT_TIMEOUT;
			resourcemgr->shot_timeout_tick = KEEP_FRAME_TICK_DEFAULT;
		} else if (resourcemgr->shot_timeout_tick > 0) {
			resourcemgr->shot_timeout_tick--;
		} else {
			resourcemgr->shot_timeout = SHOT_TIMEOUT;
		}
	}

	/* fd information copy */
#if !defined(ENABLE_SHARED_METADATA) && !defined(FAST_FDAE)
	memcpy(&frame->shot->uctl.fdUd, &device->fdUd, sizeof(struct camera2_fd_uctl));
#endif

	PROGRAM_COUNT(9);

	if (test_bit(IS_SUBDEV_PARAM_ERR, &group->head->leader.state))
		set_bit(IS_SUBDEV_FORCE_SET, &group->head->leader.state);
	else
		clear_bit(IS_SUBDEV_FORCE_SET, &group->head->leader.state);

#if defined(USE_OFFLINE_PROCESSING)
	ret = is_ischain_update_sensor_mode(device, frame);
	if (ret) {
		merr("is_ischain_update_sensor_mode is fail(%d)", device, ret);
		goto p_err;
	}
#endif

	child = group;
	while (child) {
		switch (child->slot) {
		case GROUP_SLOT_PAF:
			TRANS_CROP(ldr_node.input.cropRegion,
				node_group->leader.input.cropRegion);

			ldr_node.pixelformat = node_group->leader.pixelformat;
			ldr_node.pixelsize = node_group->leader.pixelsize;

			ret = is_ischain_paf_group_tag(device, frame, &ldr_node);
			if (ret) {
				merr("is_ischain_paf_group_tag is fail(%d)", device, ret);
				goto p_err;
			}
			break;
		case GROUP_SLOT_3AA:
			TRANS_CROP(ldr_node.input.cropRegion,
				node_group->leader.input.cropRegion);
			if (child->junction->cid < CAPTURE_NODE_MAX) {
				TRANS_CROP(ldr_node.output.cropRegion,
					node_group->capture[child->junction->cid].output.cropRegion);
			} else if (child->junction->id == child->leader.id) {
				TRANS_CROP(ldr_node.output.cropRegion,
					ldr_node.input.cropRegion);
			} else {
				mgerr("capture id(%d) is invalid", group, group, child->junction->cid);
			}

			ret = is_ischain_3aa_group_tag(device, frame, &ldr_node);
			if (ret) {
				merr("is_ischain_3aa_group_tag is fail(%d)", device, ret);
				goto p_err;
			}
			break;
		case GROUP_SLOT_LME:
			TRANS_CROP(ldr_node.input.cropRegion,
				ldr_node.output.cropRegion);
			TRANS_CROP(ldr_node.output.cropRegion,
				ldr_node.input.cropRegion);
			ret = is_ischain_lme_group_tag(device, frame, &ldr_node);
			if (ret) {
				merr("is_ischain_lme_group_tag is fail(%d)", device, ret);
				goto p_err;
			}
			break;
		case GROUP_SLOT_ISP:
			TRANS_CROP(ldr_node.input.cropRegion,
				ldr_node.output.cropRegion);
			TRANS_CROP(ldr_node.output.cropRegion,
				ldr_node.input.cropRegion);
			ret = is_ischain_isp_group_tag(device, frame, &ldr_node);
			if (ret) {
				merr("is_ischain_isp_group_tag is fail(%d)", device, ret);
				goto p_err;
			}
			break;
		case GROUP_SLOT_YPP:
			TRANS_CROP(ldr_node.input.cropRegion,
				node_group->leader.input.cropRegion);

			ret = is_ischain_ypp_group_tag(device, frame, &ldr_node);
			if (ret) {
				merr("is_ischain_ypp_group_tag is fail(%d)", device, ret);
				goto p_err;
			}
			break;
		case GROUP_SLOT_MCS:
			TRANS_CROP(ldr_node.input.cropRegion,
				ldr_node.output.cropRegion);
			TRANS_CROP(ldr_node.output.cropRegion,
				ldr_node.input.cropRegion);
			ret = is_ischain_mcs_group_tag(device, frame, &ldr_node);
			if (ret) {
				merr("is_ischain_mcs_group_tag is fail(%d)", device, ret);
				goto p_err;
			}
			break;
		case GROUP_SLOT_VRA:
			vra = &device->group_vra;
			TRANS_CROP(ldr_node.input.cropRegion,
				(u32 *)&vra->prev->junction->output.crop);
			TRANS_CROP(ldr_node.output.cropRegion,
				ldr_node.input.cropRegion);
			ret = is_ischain_vra_group_tag(device, frame, &ldr_node);
			if (ret) {
				merr("is_ischain_vra_group_tag is fail(%d)", device, ret);
				goto p_err;
			}
			break;
		default:
			merr("group slot is invalid(%d)", device, child->slot);
			BUG();
		}

		child = child->child;
	}

	PROGRAM_COUNT(10);

p_err:
	is_ischain_update_shot(device, frame);

	if (frame->stripe_info.region_num
		&& frame->stripe_info.region_id < frame->stripe_info.region_num - 1)
		next_state = FS_STRIPE_PROCESS;

	if (ret) {
		mgrerr(" SKIP(%d) : %d\n", device, group, check_frame, check_frame->index, ret);
	} else {
		set_bit(group->leader.id, &frame->out_flag);
		framemgr_e_barrier_irqs(framemgr, FMGR_IDX_25, flags);
		trans_frame(framemgr, frame, next_state);
		framemgr_x_barrier_irqr(framemgr, FMGR_IDX_25, flags);
	}

frame_err:
framemgr_err:
	return ret;
}

static int is_ischain_3aa_shot(struct is_device_ischain *device,
	struct is_frame *check_frame)
{
	int ret = 0;
	unsigned long flags;
	struct is_resourcemgr *resourcemgr;
	struct is_group *group, *child, *vra;
	struct is_framemgr *framemgr;
	struct is_frame *frame;
	struct camera2_node_group *node_group;
	struct camera2_node ldr_node = {0, };
	enum is_frame_state next_state = FS_PROCESS;

	mdbgs_ischain(4, "%s()\n", device, __func__);

	FIMC_BUG(!device);
	FIMC_BUG(!check_frame);

	resourcemgr = device->resourcemgr;
	group = &device->group_3aa;
	frame = NULL;

	framemgr = GET_HEAD_GROUP_FRAMEMGR(group, check_frame->cur_shot_idx);
	if (!framemgr) {
		merr("framemgr is NULL", device);
		ret = -EINVAL;
		goto framemgr_err;
	}

	frame = peek_frame(framemgr, check_frame->state);

	if (unlikely(!frame)) {
		merr("frame is NULL. check_state(%d)", device, check_frame->state);
		ret = -EINVAL;
		goto frame_err;
	}

	if (unlikely(frame != check_frame)) {
		merr("frame checking is fail(%p != %p)", device, frame, check_frame);
		ret = -EINVAL;
		goto p_err;
	}

	if (unlikely(!frame->shot)) {
		merr("frame->shot is NULL", device);
		ret = -EINVAL;
		goto p_err;
	}

	frame->shot->ctl.vendor_entry.lowIndexParam = 0;
	frame->shot->ctl.vendor_entry.highIndexParam = 0;
	frame->shot->dm.vendor_entry.lowIndexParam = 0;
	frame->shot->dm.vendor_entry.highIndexParam = 0;
	node_group = &frame->shot_ext->node_group;

	PROGRAM_COUNT(8);

	is_ischain_update_setfile(device, group, frame);

	if (test_bit(IS_GROUP_OTF_INPUT, &group->state)) {
		enum aa_capture_intent captureIntent;
		captureIntent = group->intent_ctl.captureIntent;

		if (captureIntent != AA_CAPTURE_INTENT_CUSTOM) {
			if (group->remainIntentCount > 0) {
				frame->shot->ctl.aa.captureIntent = captureIntent;
				frame->shot->ctl.aa.vendor_captureCount = group->intent_ctl.vendor_captureCount;
				frame->shot->ctl.aa.vendor_captureExposureTime = group->intent_ctl.vendor_captureExposureTime;
				frame->shot->ctl.aa.vendor_captureEV = group->intent_ctl.vendor_captureEV;
				if (group->intent_ctl.vendor_isoValue) {
					frame->shot->ctl.aa.vendor_isoMode = AA_ISOMODE_MANUAL;
					frame->shot->ctl.aa.vendor_isoValue = group->intent_ctl.vendor_isoValue;
					frame->shot->ctl.sensor.sensitivity = frame->shot->ctl.aa.vendor_isoValue;
				}
				if (group->intent_ctl.vendor_aeExtraMode) {
					frame->shot->ctl.aa.vendor_aeExtraMode = group->intent_ctl.vendor_aeExtraMode;
				}
				if (group->intent_ctl.aeMode) {
					frame->shot->ctl.aa.aeMode = group->intent_ctl.aeMode;
				}
				memcpy(&(frame->shot->ctl.aa.vendor_multiFrameEvList),
					&(group->intent_ctl.vendor_multiFrameEvList),
					sizeof(group->intent_ctl.vendor_multiFrameEvList));
				memcpy(&(frame->shot->ctl.aa.vendor_multiFrameIsoList),
					&(group->intent_ctl.vendor_multiFrameIsoList),
					sizeof(group->intent_ctl.vendor_multiFrameIsoList));
				memcpy(&(frame->shot->ctl.aa.vendor_multiFrameExposureList),
					&(group->intent_ctl.vendor_multiFrameExposureList),
					sizeof(group->intent_ctl.vendor_multiFrameExposureList));
				group->remainIntentCount--;
			} else {
				group->intent_ctl.captureIntent = AA_CAPTURE_INTENT_CUSTOM;
				group->intent_ctl.vendor_captureCount = 0;
				group->intent_ctl.vendor_captureExposureTime = 0;
				group->intent_ctl.vendor_captureEV = 0;
				memset(&(group->intent_ctl.vendor_multiFrameEvList), 0,
					sizeof(group->intent_ctl.vendor_multiFrameEvList));
				memset(&(group->intent_ctl.vendor_multiFrameIsoList), 0,
					sizeof(group->intent_ctl.vendor_multiFrameIsoList));
				memset(&(group->intent_ctl.vendor_multiFrameExposureList), 0,
					sizeof(group->intent_ctl.vendor_multiFrameExposureList));
				group->intent_ctl.vendor_isoValue = 0;
				group->intent_ctl.vendor_aeExtraMode = AA_AE_EXTRA_MODE_AUTO;
				group->intent_ctl.aeMode = 0;
			}
			minfo("frame count(%d), intent(%d), count(%d) captureExposureTime(%d) remainIntentCount(%d)\n",
				device, frame->fcount,
				frame->shot->ctl.aa.captureIntent, frame->shot->ctl.aa.vendor_captureCount,
				frame->shot->ctl.aa.vendor_captureExposureTime, group->remainIntentCount);
		}

		if (group->lens_ctl.aperture != 0) {
			frame->shot->ctl.lens.aperture = group->lens_ctl.aperture;
			group->lens_ctl.aperture = 0;
		}

		if (frame->shot->ctl.aa.sceneMode == AA_SCENE_MODE_ASTRO) {
			resourcemgr->shot_timeout = SHOT_TIMEOUT * 10; // 30s timeout
		} else if (frame->shot->ctl.aa.sceneMode == AA_SCENE_MODE_HYPERLAPSE
			&& frame->shot->ctl.aa.vendor_currentHyperlapseMode == 300) {
			resourcemgr->shot_timeout = SHOT_TIMEOUT * 5; // 15s timeout
		} else if (frame->shot->ctl.aa.vendor_captureExposureTime >= ((SHOT_TIMEOUT * 1000) - 1000000)) {
			/*
			* Adjust the shot timeout value based on sensor exposure time control.
			* Exposure Time >= (SHOT_TIMEOUT - 1sec): Increase shot timeout value.
			*/
			resourcemgr->shot_timeout = (frame->shot->ctl.aa.vendor_captureExposureTime / 1000) + SHOT_TIMEOUT;
			resourcemgr->shot_timeout_tick = KEEP_FRAME_TICK_DEFAULT;
		} else if (resourcemgr->shot_timeout_tick > 0) {
			resourcemgr->shot_timeout_tick--;
		} else {
			resourcemgr->shot_timeout = SHOT_TIMEOUT;
		}
	}

	/* fd information copy */
#if !defined(ENABLE_SHARED_METADATA) && !defined(FAST_FDAE)
	memcpy(&frame->shot->uctl.fdUd, &device->fdUd, sizeof(struct camera2_fd_uctl));
#endif

	PROGRAM_COUNT(9);

	if (test_bit(IS_SUBDEV_PARAM_ERR, &group->head->leader.state))
		set_bit(IS_SUBDEV_FORCE_SET, &group->head->leader.state);
	else
		clear_bit(IS_SUBDEV_FORCE_SET, &group->head->leader.state);

#if defined(USE_OFFLINE_PROCESSING)
	ret = is_ischain_update_sensor_mode(device, frame);
	if (ret) {
		merr("is_ischain_update_sensor_mode is fail(%d)", device, ret);
		goto p_err;
	}
#endif

	child = group;
	while (child) {
		switch (child->slot) {
		case GROUP_SLOT_3AA:
			TRANS_CROP(ldr_node.input.cropRegion,
				node_group->leader.input.cropRegion);
			if (child->junction->cid < CAPTURE_NODE_MAX) {
				TRANS_CROP(ldr_node.output.cropRegion,
					node_group->capture[child->junction->cid].output.cropRegion);
			} else {
				mgerr("capture id(%d) is invalid", group, group, child->junction->cid);
			}

			ret = is_ischain_3aa_group_tag(device, frame, &ldr_node);
			if (ret) {
				merr("is_ischain_3aa_group_tag is fail(%d)", device, ret);
				goto p_err;
			}
			break;
		case GROUP_SLOT_LME:
			TRANS_CROP(ldr_node.input.cropRegion,
				ldr_node.output.cropRegion);
			TRANS_CROP(ldr_node.output.cropRegion,
				ldr_node.input.cropRegion);
			ret = is_ischain_lme_group_tag(device, frame, &ldr_node);
			if (ret) {
				merr("is_ischain_lme_group_tag is fail(%d)", device, ret);
				goto p_err;
			}
			break;
		case GROUP_SLOT_ISP:
			TRANS_CROP(ldr_node.input.cropRegion,
				ldr_node.output.cropRegion);
			TRANS_CROP(ldr_node.output.cropRegion,
				ldr_node.input.cropRegion);
			ret = is_ischain_isp_group_tag(device, frame, &ldr_node);
			if (ret) {
				merr("is_ischain_isp_group_tag is fail(%d)", device, ret);
				goto p_err;
			}
			break;
		case GROUP_SLOT_YPP:
			TRANS_CROP(ldr_node.input.cropRegion,
				node_group->leader.input.cropRegion);

			ret = is_ischain_ypp_group_tag(device, frame, &ldr_node);
			if (ret) {
				merr("is_ischain_ypp_group_tag is fail(%d)", device, ret);
				goto p_err;
			}
			break;
		case GROUP_SLOT_MCS:
			TRANS_CROP(ldr_node.input.cropRegion,
				ldr_node.output.cropRegion);
			TRANS_CROP(ldr_node.output.cropRegion,
				ldr_node.input.cropRegion);
			ret = is_ischain_mcs_group_tag(device, frame, &ldr_node);
			if (ret) {
				merr("is_ischain_mcs_group_tag is fail(%d)", device, ret);
				goto p_err;
			}
			break;
		case GROUP_SLOT_VRA:
			vra = &device->group_vra;
			TRANS_CROP(ldr_node.input.cropRegion,
				(u32 *)&vra->prev->junction->output.crop);
			TRANS_CROP(ldr_node.output.cropRegion,
				ldr_node.input.cropRegion);
			ret = is_ischain_vra_group_tag(device, frame, &ldr_node);
			if (ret) {
				merr("is_ischain_vra_group_tag is fail(%d)", device, ret);
				goto p_err;
			}
			break;
		default:
			merr("group slot is invalid(%d)", device, child->slot);
			BUG();
		}

		child = child->child;
	}

	PROGRAM_COUNT(10);

p_err:
	is_ischain_update_shot(device, frame);

	if (frame->stripe_info.region_num
		&& frame->stripe_info.region_id < frame->stripe_info.region_num - 1)
		next_state = FS_STRIPE_PROCESS;

	if (ret) {
		mgrerr(" SKIP(%d) : %d\n", device, group, check_frame, check_frame->index, ret);
	} else {
		set_bit(group->leader.id, &frame->out_flag);
		framemgr_e_barrier_irqs(framemgr, FMGR_IDX_25, flags);
		trans_frame(framemgr, frame, next_state);
#ifdef SENSOR_REQUEST_DELAY
		if (test_bit(IS_GROUP_OTF_INPUT, &group->state) &&
			(frame->shot->uctl.opMode == CAMERA_OP_MODE_HAL3_GED)) {
			if (framemgr->queued_count[FS_REQUEST] < SENSOR_REQUEST_DELAY)
				mgrwarn(" late sensor control shot", device, group, frame);
		}
#endif
		framemgr_x_barrier_irqr(framemgr, FMGR_IDX_25, flags);
	}

frame_err:
framemgr_err:
	return ret;
}

static int is_ischain_lme_shot(struct is_device_ischain *device,
	struct is_frame *check_frame)
{
	int ret = 0;
	unsigned long flags;
	struct is_group *group, *child, *vra;
	struct is_framemgr *framemgr;
	struct is_frame *frame;
	struct camera2_node_group *node_group;
	struct camera2_node ldr_node = {0, };
	enum is_frame_state next_state = FS_PROCESS;

	FIMC_BUG(!device);
	FIMC_BUG(!check_frame);
	FIMC_BUG(device->sensor_id >= SENSOR_NAME_END);

	mdbgs_ischain(4, "%s\n", device, __func__);

	frame = NULL;
	group = &device->group_lme;

	framemgr = GET_HEAD_GROUP_FRAMEMGR(group, check_frame->cur_shot_idx);
	if (!framemgr) {
		merr("framemgr is NULL", device);
		ret = -EINVAL;
		goto framemgr_err;
	}

	frame = peek_frame(framemgr, check_frame->state);

	if (unlikely(!frame)) {
		merr("frame is NULL. check_state(%d)", device, check_frame->state);
		ret = -EINVAL;
		goto frame_err;
	}

	if (unlikely(frame != check_frame)) {
		merr("frame checking is fail(%p != %p)", device, frame, check_frame);
		ret = -EINVAL;
		goto p_err;
	}

	if (unlikely(!frame->shot)) {
		merr("frame->shot is NULL", device);
		ret = -EINVAL;
		goto p_err;
	}

	frame->shot->ctl.vendor_entry.lowIndexParam = 0;
	frame->shot->ctl.vendor_entry.highIndexParam = 0;
	frame->shot->dm.vendor_entry.lowIndexParam = 0;
	frame->shot->dm.vendor_entry.highIndexParam = 0;
	node_group = &frame->shot_ext->node_group;

	PROGRAM_COUNT(8);

	is_ischain_update_setfile(device, group, frame);

	PROGRAM_COUNT(9);

	if (test_bit(IS_SUBDEV_PARAM_ERR, &group->leader.state))
		set_bit(IS_SUBDEV_FORCE_SET, &group->leader.state);
	else
		clear_bit(IS_SUBDEV_FORCE_SET, &group->leader.state);

#if defined(USE_OFFLINE_PROCESSING)
	ret = is_ischain_update_sensor_mode(device, frame);
	if (ret) {
		merr("is_ischain_update_sensor_mode is fail(%d)", device, ret);
		goto p_err;
	}
#endif

	child = group;
	while (child) {
		switch (child->slot) {
		case GROUP_SLOT_LME:
			TRANS_CROP(ldr_node.input.cropRegion,
				ldr_node.output.cropRegion);
			TRANS_CROP(ldr_node.output.cropRegion,
				ldr_node.input.cropRegion);

			ret = is_ischain_lme_group_tag(device, frame, &ldr_node);
			if (ret) {
				merr("is_ischain_lme_group_tag is fail(%d)", device, ret);
				goto p_err;
			}
			break;
		case GROUP_SLOT_ISP:
			TRANS_CROP(ldr_node.input.cropRegion,
				node_group->leader.input.cropRegion);
			TRANS_CROP(ldr_node.output.cropRegion,
				ldr_node.input.cropRegion);

			ldr_node.pixelformat = node_group->leader.pixelformat;
			ldr_node.pixelsize = node_group->leader.pixelsize;
			ret = is_ischain_isp_group_tag(device, frame, &ldr_node);
			if (ret) {
				merr("is_ischain_isp_group_tag is fail(%d)", device, ret);
				goto p_err;
			}
			break;
		case GROUP_SLOT_YPP:
			TRANS_CROP(ldr_node.input.cropRegion,
				node_group->leader.input.cropRegion);

			ret = is_ischain_ypp_group_tag(device, frame, &ldr_node);
			if (ret) {
				merr("is_ischain_ypp_group_tag is fail(%d)", device, ret);
				goto p_err;
			}
			break;
		case GROUP_SLOT_MCS:
			TRANS_CROP(ldr_node.input.cropRegion,
				ldr_node.output.cropRegion);
			TRANS_CROP(ldr_node.output.cropRegion,
				ldr_node.input.cropRegion);
			ret = is_ischain_mcs_group_tag(device, frame, &ldr_node);
			if (ret) {
				merr("is_ischain_mcs_group_tag is fail(%d)", device, ret);
				goto p_err;
			}
			break;
		case GROUP_SLOT_VRA:
			vra = &device->group_vra;
			TRANS_CROP(ldr_node.input.cropRegion,
				(u32 *)&vra->prev->junction->output.crop);
			TRANS_CROP(ldr_node.output.cropRegion,
				ldr_node.input.cropRegion);
			ret = is_ischain_vra_group_tag(device, frame, &ldr_node);
			if (ret) {
				merr("is_ischain_vra_group_tag is fail(%d)", device, ret);
				goto p_err;
			}
			break;
		default:
			merr("group slot is invalid(%d)", device, child->slot);
			BUG();
		}

		child = child->child;
	}

#ifdef PRINT_PARAM
	if (frame->fcount == 1) {
		is_hw_memdump(device->interface,
			(ulong) &device->is_region->parameter,
			(ulong) &device->is_region->parameter + sizeof(device->is_region->parameter));
	}
#endif

	PROGRAM_COUNT(10);

p_err:
	if (frame->stripe_info.region_num
		&& frame->stripe_info.region_id < frame->stripe_info.region_num - 1)
		next_state = FS_STRIPE_PROCESS;

	if (ret) {
		mgrerr(" SKIP(%d) : %d\n", device, group, check_frame, check_frame->index, ret);
	} else {
		set_bit(group->leader.id, &frame->out_flag);
		framemgr_e_barrier_irqs(framemgr, FMGR_IDX_26, flags);
		trans_frame(framemgr, frame, next_state);
		framemgr_x_barrier_irqr(framemgr, FMGR_IDX_26, flags);
	}

frame_err:
framemgr_err:
	return ret;
}

static int is_ischain_isp_shot(struct is_device_ischain *device,
	struct is_frame *check_frame)
{
	int ret = 0;
	unsigned long flags;
	struct is_group *group, *child, *vra;
	struct is_framemgr *framemgr;
	struct is_frame *frame;
	struct camera2_node_group *node_group;
	struct camera2_node ldr_node = {0, };
	enum is_frame_state next_state = FS_PROCESS;

	FIMC_BUG(!device);
	FIMC_BUG(!check_frame);
	FIMC_BUG(device->sensor_id >= SENSOR_NAME_END);

	mdbgs_ischain(4, "%s\n", device, __func__);

	frame = NULL;
	group = &device->group_isp;

	framemgr = GET_HEAD_GROUP_FRAMEMGR(group, check_frame->cur_shot_idx);
	if (!framemgr) {
		merr("framemgr is NULL", device);
		ret = -EINVAL;
		goto framemgr_err;
	}

	frame = peek_frame(framemgr, check_frame->state);

	if (unlikely(!frame)) {
		merr("frame is NULL. check_state(%d)", device, check_frame->state);
		ret = -EINVAL;
		goto frame_err;
	}

	if (unlikely(frame != check_frame)) {
		merr("frame checking is fail(%p != %p)", device, frame, check_frame);
		ret = -EINVAL;
		goto p_err;
	}

	if (unlikely(!frame->shot)) {
		merr("frame->shot is NULL", device);
		ret = -EINVAL;
		goto p_err;
	}

	frame->shot->ctl.vendor_entry.lowIndexParam = 0;
	frame->shot->ctl.vendor_entry.highIndexParam = 0;
	frame->shot->dm.vendor_entry.lowIndexParam = 0;
	frame->shot->dm.vendor_entry.highIndexParam = 0;
	node_group = &frame->shot_ext->node_group;

	PROGRAM_COUNT(8);

	is_ischain_update_setfile(device, group, frame);

	PROGRAM_COUNT(9);

	if (test_bit(IS_SUBDEV_PARAM_ERR, &group->leader.state))
		set_bit(IS_SUBDEV_FORCE_SET, &group->leader.state);
	else
		clear_bit(IS_SUBDEV_FORCE_SET, &group->leader.state);

#if defined(USE_OFFLINE_PROCESSING)
	ret = is_ischain_update_sensor_mode(device, frame);
	if (ret) {
		merr("is_ischain_update_sensor_mode is fail(%d)", device, ret);
		goto p_err;
	}
#endif

	child = group;
	while (child) {
		switch (child->slot) {
		case GROUP_SLOT_ISP:
			TRANS_CROP(ldr_node.input.cropRegion,
				node_group->leader.input.cropRegion);
			TRANS_CROP(ldr_node.output.cropRegion,
				ldr_node.input.cropRegion);

			ldr_node.pixelformat = node_group->leader.pixelformat;
			ldr_node.pixelsize = node_group->leader.pixelsize;
			ret = is_ischain_isp_group_tag(device, frame, &ldr_node);
			if (ret) {
				merr("is_ischain_isp_group_tag is fail(%d)", device, ret);
				goto p_err;
			}
			break;
		case GROUP_SLOT_YPP:
			TRANS_CROP(ldr_node.input.cropRegion,
				node_group->leader.input.cropRegion);

			ret = is_ischain_ypp_group_tag(device, frame, &ldr_node);
			if (ret) {
				merr("is_ischain_ypp_group_tag is fail(%d)", device, ret);
				goto p_err;
			}
			break;
		case GROUP_SLOT_MCS:
			TRANS_CROP(ldr_node.input.cropRegion,
				ldr_node.output.cropRegion);
			TRANS_CROP(ldr_node.output.cropRegion,
				ldr_node.input.cropRegion);
			ret = is_ischain_mcs_group_tag(device, frame, &ldr_node);
			if (ret) {
				merr("is_ischain_mcs_group_tag is fail(%d)", device, ret);
				goto p_err;
			}
			break;
		case GROUP_SLOT_VRA:
			vra = &device->group_vra;
			TRANS_CROP(ldr_node.input.cropRegion,
				(u32 *)&vra->prev->junction->output.crop);
			TRANS_CROP(ldr_node.output.cropRegion,
				ldr_node.input.cropRegion);
			ret = is_ischain_vra_group_tag(device, frame, &ldr_node);
			if (ret) {
				merr("is_ischain_vra_group_tag is fail(%d)", device, ret);
				goto p_err;
			}
			break;
		default:
			merr("group slot is invalid(%d)", device, child->slot);
			BUG();
		}

		child = child->child;
	}

#ifdef PRINT_PARAM
	if (frame->fcount == 1) {
		is_hw_memdump(device->interface,
			(ulong) &device->is_region->parameter,
			(ulong) &device->is_region->parameter + sizeof(device->is_region->parameter));
	}
#endif

	PROGRAM_COUNT(10);

p_err:
	if (frame->stripe_info.region_num
		&& frame->stripe_info.region_id < frame->stripe_info.region_num - 1)
		next_state = FS_STRIPE_PROCESS;

	if (ret) {
		mgrerr(" SKIP(%d) : %d\n", device, group, check_frame, check_frame->index, ret);
	} else {
		set_bit(group->leader.id, &frame->out_flag);
		framemgr_e_barrier_irqs(framemgr, FMGR_IDX_26, flags);
		trans_frame(framemgr, frame, next_state);
		framemgr_x_barrier_irqr(framemgr, FMGR_IDX_26, flags);
	}

frame_err:
framemgr_err:
	return ret;
}

static int is_ischain_ypp_shot(struct is_device_ischain *device,
	struct is_frame *check_frame)
{
	int ret = 0;
	unsigned long flags;
	struct is_group *group, *child, *vra;
	struct is_framemgr *framemgr;
	struct is_frame *frame;
	struct camera2_node_group *node_group;
	struct camera2_node ldr_node = {0, };
	enum is_frame_state next_state = FS_PROCESS;

	FIMC_BUG(!device);
	FIMC_BUG(!check_frame);
	FIMC_BUG(device->sensor_id >= SENSOR_NAME_END);

	mdbgs_ischain(4, "%s\n", device, __func__);

	frame = NULL;
	group = &device->group_ypp;

	framemgr = GET_HEAD_GROUP_FRAMEMGR(group, check_frame->cur_shot_idx);
	if (!framemgr) {
		merr("framemgr is NULL", device);
		ret = -EINVAL;
		goto framemgr_err;
	}

	frame = peek_frame(framemgr, check_frame->state);

	if (unlikely(!frame)) {
		merr("frame is NULL. check_state(%d)", device, check_frame->state);
		ret = -EINVAL;
		goto frame_err;
	}

	if (unlikely(frame != check_frame)) {
		merr("frame checking is fail(%p != %p)", device, frame, check_frame);
		ret = -EINVAL;
		goto p_err;
	}

	if (unlikely(!frame->shot)) {
		merr("frame->shot is NULL", device);
		ret = -EINVAL;
		goto p_err;
	}

	frame->shot->ctl.vendor_entry.lowIndexParam = 0;
	frame->shot->ctl.vendor_entry.highIndexParam = 0;
	frame->shot->dm.vendor_entry.lowIndexParam = 0;
	frame->shot->dm.vendor_entry.highIndexParam = 0;
	node_group = &frame->shot_ext->node_group;

	PROGRAM_COUNT(8);

	is_ischain_update_setfile(device, group, frame);

	PROGRAM_COUNT(9);

	if (test_bit(IS_SUBDEV_PARAM_ERR, &group->leader.state))
		set_bit(IS_SUBDEV_FORCE_SET, &group->leader.state);
	else
		clear_bit(IS_SUBDEV_FORCE_SET, &group->leader.state);

	child = group;
	while (child) {
		switch (child->slot) {
		case GROUP_SLOT_YPP:
			TRANS_CROP(ldr_node.input.cropRegion,
				node_group->leader.input.cropRegion);

			ret = is_ischain_ypp_group_tag(device, frame, &ldr_node);
			if (ret) {
				merr("is_ischain_ypp_group_tag is fail(%d)", device, ret);
				goto p_err;
			}
			break;
		case GROUP_SLOT_MCS:
			TRANS_CROP(ldr_node.input.cropRegion,
				node_group->leader.input.cropRegion);
			TRANS_CROP(ldr_node.output.cropRegion,
				ldr_node.input.cropRegion);
			ret = is_ischain_mcs_group_tag(device, frame, &ldr_node);
			if (ret) {
				merr("is_ischain_mcs_group_tag is fail(%d)", device, ret);
				goto p_err;
			}
			break;
		case GROUP_SLOT_VRA:
			vra = &device->group_vra;
			TRANS_CROP(ldr_node.input.cropRegion,
				(u32 *)&vra->prev->junction->output.crop);
			TRANS_CROP(ldr_node.output.cropRegion,
				ldr_node.input.cropRegion);
			ret = is_ischain_vra_group_tag(device, frame, &ldr_node);
			if (ret) {
				merr("is_ischain_vra_group_tag is fail(%d)", device, ret);
				goto p_err;
			}
			break;
		default:
			merr("group slot is invalid(%d)", device, child->slot);
			BUG();
		}

		child = child->child;
	}

#ifdef PRINT_PARAM
	if (frame->fcount == 1) {
		is_hw_memdump(device->interface,
			(ulong) &device->is_region->parameter,
			(ulong) &device->is_region->parameter + sizeof(device->is_region->parameter));
	}
#endif

	PROGRAM_COUNT(10);

p_err:
	if (frame->stripe_info.region_num
		&& frame->stripe_info.region_id < frame->stripe_info.region_num - 1)
		next_state = FS_STRIPE_PROCESS;

	if (ret) {
		mgrerr(" SKIP(%d) : %d\n", device, group, check_frame, check_frame->index, ret);
	} else {
		set_bit(group->leader.id, &frame->out_flag);
		framemgr_e_barrier_irqs(framemgr, FMGR_IDX_26, flags);
		trans_frame(framemgr, frame, next_state);
		framemgr_x_barrier_irqr(framemgr, FMGR_IDX_26, flags);
	}

frame_err:
framemgr_err:
	return ret;
}

static int is_ischain_mcs_shot(struct is_device_ischain *device,
	struct is_frame *check_frame)
{
	int ret = 0;
	unsigned long flags;
	struct is_group *group, *child, *vra;
	struct is_framemgr *framemgr;
	struct is_frame *frame;
	struct camera2_node_group *node_group;
	struct camera2_node ldr_node = {0, };
	enum is_frame_state next_state = FS_PROCESS;

	mdbgs_ischain(4, "%s()\n", device, __func__);

	FIMC_BUG(!device);
	FIMC_BUG(!check_frame);

	frame = NULL;
	group = &device->group_mcs;

	framemgr = GET_HEAD_GROUP_FRAMEMGR(group, check_frame->cur_shot_idx);
	if (!framemgr) {
		merr("framemgr is NULL", device);
		ret = -EINVAL;
		goto framemgr_err;
	}

	frame = peek_frame(framemgr, check_frame->state);

	if (unlikely(!frame)) {
		merr("frame is NULL. check_state(%d)", device, check_frame->state);
		ret = -EINVAL;
		goto frame_err;
	}

	if (unlikely(frame != check_frame)) {
		merr("frame checking is fail(%p != %p)", device, frame, check_frame);
		ret = -EINVAL;
		goto p_err;
	}

	if (unlikely(!frame->shot)) {
		merr("frame->shot is NULL", device);
		ret = -EINVAL;
		goto p_err;
	}

	frame->shot->ctl.vendor_entry.lowIndexParam = 0;
	frame->shot->ctl.vendor_entry.highIndexParam = 0;
	frame->shot->dm.vendor_entry.lowIndexParam = 0;
	frame->shot->dm.vendor_entry.highIndexParam = 0;
	node_group = &frame->shot_ext->node_group;

	PROGRAM_COUNT(8);

	is_ischain_update_setfile(device, group, frame);

	PROGRAM_COUNT(9);

	if (test_bit(IS_SUBDEV_PARAM_ERR, &group->leader.state))
		set_bit(IS_SUBDEV_FORCE_SET, &group->leader.state);
	else
		clear_bit(IS_SUBDEV_FORCE_SET, &group->leader.state);

#if defined(USE_OFFLINE_PROCESSING)
	ret = is_ischain_update_sensor_mode(device, frame);
	if (ret) {
		merr("is_ischain_update_sensor_mode is fail(%d)", device, ret);
		goto p_err;
	}
#endif

	child = group;
	while (child) {
		switch (child->slot) {
		case GROUP_SLOT_MCS:
			TRANS_CROP(ldr_node.input.cropRegion,
				node_group->leader.input.cropRegion);
			TRANS_CROP(ldr_node.output.cropRegion,
				ldr_node.input.cropRegion);
			ret = is_ischain_mcs_group_tag(device, frame, &ldr_node);
			if (ret) {
				merr("is_ischain_mcs_group_tag is fail(%d)", device, ret);
				goto p_err;
			}
			break;
		case GROUP_SLOT_VRA:
			vra = &device->group_vra;
			TRANS_CROP(ldr_node.input.cropRegion,
				(u32 *)&vra->prev->junction->output.crop);
			TRANS_CROP(ldr_node.output.cropRegion,
				ldr_node.input.cropRegion);
			ret = is_ischain_vra_group_tag(device, frame, &ldr_node);
			if (ret) {
				merr("is_ischain_vra_group_tag is fail(%d)", device, ret);
				goto p_err;
			}
			break;
		default:
			merr("group slot is invalid(%d)", device, child->slot);
			BUG();
		}

		child = child->child;
	}

	PROGRAM_COUNT(10);

p_err:
	if (frame->stripe_info.region_num
		&& frame->stripe_info.region_id < frame->stripe_info.region_num - 1)
		next_state = FS_STRIPE_PROCESS;

	if (ret) {
		mgrerr(" SKIP(%d) : %d\n", device, group, check_frame, check_frame->index, ret);
	} else {
		set_bit(group->leader.id, &frame->out_flag);
		framemgr_e_barrier_irqs(framemgr, FMGR_IDX_29, flags);
		trans_frame(framemgr, frame, next_state);
		framemgr_x_barrier_irqr(framemgr, FMGR_IDX_29, flags);
	}

frame_err:
framemgr_err:
	return ret;
}

#if defined(SOC_VRA)
static int is_ischain_vra_shot(struct is_device_ischain *device,
	struct is_frame *check_frame)
{
	int ret = 0;
	unsigned long flags;
	struct is_group *group, *child;
	struct is_framemgr *framemgr;
	struct is_frame *frame;
	struct camera2_node_group *node_group;
	struct camera2_node ldr_node = {0, };

	mdbgs_ischain(4, "%s()\n", device, __func__);

	FIMC_BUG(!device);
	FIMC_BUG(!check_frame);

	frame = NULL;
	group = &device->group_vra;

	framemgr = GET_HEAD_GROUP_FRAMEMGR(group, check_frame->cur_shot_idx);
	if (!framemgr) {
		merr("framemgr is NULL", device);
		ret = -EINVAL;
		goto framemgr_err;
	}

	frame = peek_frame(framemgr, FS_REQUEST);

	if (unlikely(!frame)) {
		merr("frame is NULL", device);
		ret = -EINVAL;
		goto frame_err;
	}

	if (unlikely(frame != check_frame)) {
		merr("frame checking is fail(%p != %p)", device, frame, check_frame);
		ret = -EINVAL;
		goto p_err;
	}

	if (unlikely(!frame->shot)) {
		merr("frame->shot is NULL", device);
		ret = -EINVAL;
		goto p_err;
	}

	frame->shot->ctl.vendor_entry.lowIndexParam = 0;
	frame->shot->ctl.vendor_entry.highIndexParam = 0;
	frame->shot->dm.vendor_entry.lowIndexParam = 0;
	frame->shot->dm.vendor_entry.highIndexParam = 0;
	node_group = &frame->shot_ext->node_group;

	PROGRAM_COUNT(8);

	is_ischain_update_setfile(device, group, frame);

	PROGRAM_COUNT(9);

	if (test_bit(IS_SUBDEV_PARAM_ERR, &group->leader.state))
		set_bit(IS_SUBDEV_FORCE_SET, &group->leader.state);
	else
		clear_bit(IS_SUBDEV_FORCE_SET, &group->leader.state);

#if defined(USE_OFFLINE_PROCESSING)
	ret = is_ischain_update_sensor_mode(device, frame);
	if (ret) {
		merr("is_ischain_update_sensor_mode is fail(%d)", device, ret);
		goto p_err;
	}
#endif

	child = group;
	while (child) {
		switch (child->slot) {
		case GROUP_SLOT_VRA:
			TRANS_CROP(ldr_node.input.cropRegion,
				node_group->leader.input.cropRegion);
			TRANS_CROP(ldr_node.output.cropRegion,
				ldr_node.input.cropRegion);
			ret = is_ischain_vra_group_tag(device, frame, &ldr_node);
			if (ret) {
				merr("is_ischain_vra_group_tag is fail(%d)", device, ret);
				goto p_err;
			}
			break;
		default:
			merr("group slot is invalid(%d)", device, child->slot);
			BUG();
		}

		child = child->child;
	}

	PROGRAM_COUNT(10);

p_err:
	if (ret) {
		mgrerr(" SKIP(%d) : %d\n", device, group, check_frame, check_frame->index, ret);
	} else {
		set_bit(group->leader.id, &frame->out_flag);
		framemgr_e_barrier_irqs(framemgr, FMGR_IDX_30, flags);
		trans_frame(framemgr, frame, FS_PROCESS);
		framemgr_x_barrier_irqr(framemgr, FMGR_IDX_30, flags);
	}

frame_err:
framemgr_err:
	return ret;
}
#endif

static int is_ischain_clh_shot(struct is_device_ischain *device,
	struct is_frame *check_frame)
{
	int ret = 0;
	unsigned long flags;
	struct is_group *group, *child;
	struct is_framemgr *framemgr;
	struct is_frame *frame;
	struct camera2_node_group *node_group;
	struct camera2_node ldr_node = {0, };

	FIMC_BUG(!device);
	FIMC_BUG(!check_frame);
	FIMC_BUG(device->sensor_id >= SENSOR_NAME_END);

	mdbgs_ischain(4, "%s\n", device, __func__);

	frame = NULL;
	group = &device->group_clh;

	framemgr = GET_HEAD_GROUP_FRAMEMGR(group, check_frame->cur_shot_idx);
	if (!framemgr) {
		merr("framemgr is NULL", device);
		ret = -EINVAL;
		goto framemgr_err;
	}

	frame = peek_frame(framemgr, FS_REQUEST);

	if (unlikely(!frame)) {
		merr("frame is NULL", device);
		ret = -EINVAL;
		goto frame_err;
	}

	if (unlikely(frame != check_frame)) {
		merr("frame checking is fail(%p != %p)", device, frame, check_frame);
		ret = -EINVAL;
		goto p_err;
	}

	if (unlikely(!frame->shot)) {
		merr("frame->shot is NULL", device);
		ret = -EINVAL;
		goto p_err;
	}

	frame->shot->ctl.vendor_entry.lowIndexParam = 0;
	frame->shot->ctl.vendor_entry.highIndexParam = 0;
	frame->shot->dm.vendor_entry.lowIndexParam = 0;
	frame->shot->dm.vendor_entry.highIndexParam = 0;
	node_group = &frame->shot_ext->node_group;

	PROGRAM_COUNT(8);

	is_ischain_update_setfile(device, group, frame);

	PROGRAM_COUNT(9);

	if (test_bit(IS_SUBDEV_PARAM_ERR, &group->leader.state))
		set_bit(IS_SUBDEV_FORCE_SET, &group->leader.state);
	else
		clear_bit(IS_SUBDEV_FORCE_SET, &group->leader.state);

#if defined(USE_OFFLINE_PROCESSING)
	ret = is_ischain_update_sensor_mode(device, frame);
	if (ret) {
		merr("is_ischain_update_sensor_mode is fail(%d)", device, ret);
		goto p_err;
	}
#endif

	child = group;
	while (child) {
		switch (child->slot) {
		case GROUP_SLOT_CLH:
			TRANS_CROP(ldr_node.input.cropRegion,
				node_group->leader.input.cropRegion);
			TRANS_CROP(ldr_node.output.cropRegion,
				ldr_node.input.cropRegion);
			ret = is_ischain_clh_group_tag(device, frame, &ldr_node);
			if (ret) {
				merr("is_ischain_clh_group_tag is fail(%d)", device, ret);
				goto p_err;
			}
			break;
		default:
			merr("group slot is invalid(%d)", device, child->slot);
			BUG();
		}

		child = child->child;
	}

#ifdef PRINT_PARAM
	if (frame->fcount == 1) {
		is_hw_memdump(device->interface,
			(ulong) &device->is_region->parameter,
			(ulong) &device->is_region->parameter + sizeof(device->is_region->parameter));
	}
#endif

	PROGRAM_COUNT(10);

p_err:
	if (ret) {
		mgrerr(" SKIP(%d) : %d\n", device, group, check_frame, check_frame->index, ret);
	} else {
		set_bit(group->leader.id, &frame->out_flag);
		framemgr_e_barrier_irqs(framemgr, FMGR_IDX_26, flags);
		trans_frame(framemgr, frame, FS_PROCESS);
		framemgr_x_barrier_irqr(framemgr, FMGR_IDX_26, flags);
	}

frame_err:
framemgr_err:
	return ret;
}

void is_bts_control(struct is_device_ischain *device)
{
#if defined(USE_BTS_SCEN)
	struct is_groupmgr *groupmgr;
	struct is_group *group_leader;

	FIMC_BUG_VOID(!device);
	FIMC_BUG_VOID(!device->groupmgr);

	groupmgr = device->groupmgr;
	group_leader = groupmgr->leader[device->instance];

	if (test_bit(IS_GROUP_OTF_INPUT, &group_leader->state))
		bts_scen_update(TYPE_CAM_BNS, false);
	else
		bts_scen_update(TYPE_CAM_BNS, true);
#endif
}
