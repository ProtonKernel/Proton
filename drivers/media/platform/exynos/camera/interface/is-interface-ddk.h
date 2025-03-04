/*
 * Samsung EXYNOS FIMC-IS (Imaging Subsystem) driver
 *
 * Copyright (C) 2014 Samsung Electronics Co., Ltd.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#ifndef IS_INTERFACE_LIBRARY_H
#define IS_INTERFACE_LIBRARY_H

#include "is-core.h"
#include "is-interface-library.h"
#include "is-param.h"
#include "is-config.h"

#define CHAIN_ID_MASK		(0x0000000F)
#define CHAIN_ID_SHIFT		(0)
#define INSTANCE_ID_MASK	(0x000000F0)
#define INSTANCE_ID_SHIFT	(4)
#define REPROCESSING_FLAG_MASK	(0x00000F00)
#define REPROCESSING_FLAG_SHIFT	(8)
#define INPUT_TYPE_MASK		(0x0000F000)
#define INPUT_TYPE_SHIFT	(12)
#define MODULE_ID_MASK		(0x0FFF0000)
#define MODULE_ID_SHIFT		(16)
#define POSITION_ID_MASK	(0xF0000000)
#define POSITION_ID_SHIFT	(28)

/* num_buffer of shot */
#define SW_FRO_NUM_SHIFT	(16)
#define CURR_INDEX_SHIFT	(24)

#define CHK_VIDEOHDR_MODE_CHANGE(curr_tnr_mode, next_tnr_mode)		\
	(((curr_tnr_mode == TNR_PROCESSING_PREVIEW_POST_OFF &&		\
		next_tnr_mode == TNR_PROCESSING_PREVIEW_POST_ON)	\
	|| (curr_tnr_mode == TNR_PROCESSING_PREVIEW_POST_ON &&		\
		next_tnr_mode == TNR_PROCESSING_PREVIEW_POST_OFF)) ? 1 : 0)

struct is_lib_isp {
	void				*object;
	struct lib_interface_func	*func;
	ulong				tune_count;
};

enum lib_cb_event_type {
	LIB_EVENT_CONFIG_LOCK			= 1,
	LIB_EVENT_FRAME_START			= 2,
	LIB_EVENT_FRAME_END			= 3,
	LIB_EVENT_DMA_A_OUT_DONE		= 4,
	LIB_EVENT_DMA_B_OUT_DONE		= 5,
	LIB_EVENT_FRAME_START_ISR		= 6,
	LIB_EVENT_ERROR_CIN_OVERFLOW		= 7,
	LIB_EVENT_ERROR_CIN_COLUMN		= 8,
	LIB_EVENT_ERROR_CIN_LINE		= 9,
	LIB_EVENT_ERROR_CIN_TOTAL_SIZE		= 10,
	LIB_EVENT_ERROR_COUT_OVERFLOW		= 11,
	LIB_EVENT_ERROR_COUT_COLUMN		= 12,
	LIB_EVENT_ERROR_COUT_LINE		= 13,
	LIB_EVENT_ERROR_COUT_TOTAL_SIZE		= 14,
	LIB_EVENT_ERROR_CONFIG_LOCK_DELAY	= 15,
	LIB_EVENT_ERROR_COMP_OVERFLOW		= 16,
	LIB_EVENT_END
};

enum dcp_dma_in_type {
	DCP_DMA_IN_GDC_MASTER,
	DCP_DMA_IN_GDC_SLAVE,
	DCP_DMA_IN_DISPARITY,
	DCP_DMA_IN_MAX
};

enum dcp_dma_out_type {
	DCP_DMA_OUT_MASTER,	/* plane order is [Y, C, Y2, C2] */
	DCP_DMA_OUT_SLAVE,	/* plane order is [Y, C] */
	DCP_DMA_OUT_MASTER_DS,
	DCP_DMA_OUT_SLAVE_DS,
	DCP_DMA_OUT_DISPARITY,
	DCP_DMA_OUT_MAX
};

enum tnr_processing_mode {
	TNR_PROCESSING_INVALID = -1,

	/* 2 New enum (0, 2, 5, 6, 7 is the same meaning with old enum) */
	TNR_PROCESSING_PREVIEW_POST_ON = 0,
	TNR_PROCESSING_PREVIEW_POST_ON_0_MOTION = 1,	/* ME group X */
	TNR_PROCESSING_PREVIEW_POST_OFF = 2,
	TNR_PROCESSING_CAPTURE_FIRST_POST_ON = 4,	/* for IDCG */
	TNR_PROCESSING_CAPTURE_FIRST = 5,
	TNR_PROCESSING_CAPTURE_MID = 6,
	TNR_PROCESSING_CAPTURE_LAST = 7,
	TNR_PROCESSING_TNR_BYPASS = 9, 			/* DNS RDMA in (not used) */
	TNR_PROCESSING_TNR_OFF = 10, 			/* single capture, MCFP RDMA input */
};

struct taa_param_set {
	struct param_sensor_config	sensor_config;
	struct param_otf_input		otf_input;
	struct param_dma_input		dma_input;
#if !defined(SOC_ORBMCH)
	struct param_dma_input		ddma_input;	/* deprecated */
#endif
	struct param_otf_output		otf_output;
	struct param_dma_output		dma_output_before_bds;
	struct param_dma_output		dma_output_after_bds;
	struct param_dma_output		dma_output_mrg;
	struct param_dma_output		dma_output_efd;
#if defined(ENABLE_ORBDS)
	struct param_dma_output		dma_output_orbds;
#endif
#if defined(ENABLE_LMEDS)
	struct param_dma_output		dma_output_lmeds;
#endif
#if !defined(SOC_ORBMCH)
	struct param_dma_output		ddma_output;	/* deprecated */
#endif
#if defined(ENABLE_HF)
	struct param_dma_output		dma_output_hf;
#endif
#if defined(ENABLE_LOGICAL_VIDEO_NODE)
	struct param_dma_output		dma_output_drcgrid;
#endif
	u32				input_dva[IS_MAX_PLANES];
	u32				output_dva_before_bds[IS_MAX_PLANES];
	u32				output_dva_after_bds[IS_MAX_PLANES];
	u32				output_dva_mrg[IS_MAX_PLANES];
	u32				output_dva_efd[IS_MAX_PLANES];
#if defined(ENABLE_ORBDS)
	u32				output_dva_orbds[IS_MAX_PLANES];
#endif
#if defined(ENABLE_LMEDS)
	u32				output_dva_lmeds[IS_MAX_PLANES];
#endif
#if defined(ENABLE_HF)
	u32				output_dva_hf[IS_MAX_PLANES];
#endif
#if defined(ENABLE_LOGICAL_VIDEO_NODE)
	u32				output_dva_drcgrid[IS_MAX_PLANES];
#endif
	uint64_t			output_kva_me[IS_MAX_PLANES];	/* ME or MCH */
#if defined(SOC_ORBMCH)
	uint64_t			output_kva_orb[IS_MAX_PLANES];
#endif
	u32				instance_id;
	u32				fcount;
	bool				reprocessing;
#ifdef CHAIN_USE_STRIPE_PROCESSING
	struct param_stripe_input	stripe_input;
#endif
};

#if defined(SOC_TNR_MERGER)
struct isp_param_set {
	struct taa_param_set		*taa_param;
	struct param_otf_input		otf_input;
	struct param_dma_input		dma_input; /* VDMA1 */
	struct param_dma_input		prev_dma_input;	/* VDMA2, for TNR merger */
	struct param_dma_input		prev_wgt_dma_input;	/* VDMA3, for TNR merger */
#if defined(SOC_MCFP)
	struct param_dma_input		motion_dma_input;
#endif
#if defined(ENABLE_RGB_REPROCESSING)
	struct param_dma_input		dma_input_rgb;
	struct param_dma_input		dma_input_noise;
#endif
#if defined(ENABLE_SC_MAP)
	struct param_dma_input		dma_input_scmap;
#endif
#if defined(SOC_MCFP)
	struct param_dma_input		dma_input_drcgrid;
#endif
	struct param_otf_output		otf_output;
#if defined(SOC_YPP)
	struct param_dma_output		dma_output_yuv; /* VDMA4 */
	struct param_dma_output		dma_output_rgb; /* VDMA5 */
#else
	struct param_dma_output		dma_output_chunk;
	struct param_dma_output		dma_output_yuv;
#endif
	struct param_dma_output		dma_output_tnr_wgt; /* VDMA6 */
	struct param_dma_output		dma_output_tnr_prev; /* VDMA7 */
#if defined(SOC_YPP)
	struct param_dma_output		dma_output_nrds;
	struct param_dma_output		dma_output_noise;
	struct param_dma_output		dma_output_hf;
	struct param_dma_output		dma_output_drc;
	struct param_dma_output		dma_output_hist;
#endif
#if defined(ENABLE_RGB_REPROCESSING)
	struct param_dma_output		dma_output_noise_rep;
#endif

	u32				input_dva[IS_MAX_PLANES];
	u32				input_dva_tnr_prev[IS_MAX_PLANES];
	u32				input_dva_tnr_wgt[IS_MAX_PLANES];
#if defined(SOC_MCFP)
	u32				input_dva_motion[IS_MAX_PLANES];
	u64				input_kva_motion[IS_MAX_PLANES]; /* For debugging */
#endif
#if defined(ENABLE_RGB_REPROCESSING)
	u32				input_dva_rgb[IS_MAX_PLANES];
	u32				input_dva_noise[IS_MAX_PLANES];
#endif
#if defined(ENABLE_SC_MAP)
	u32				input_dva_scmap[IS_MAX_PLANES];
#endif
#if defined(SOC_MCFP)
	u32				input_dva_drcgrid[IS_MAX_PLANES];
#endif
#if defined(SOC_YPP)
	u32				output_dva_yuv[IS_MAX_PLANES];
	u32				output_dva_rgb[IS_MAX_PLANES];
#else
	u32				output_dva_chunk[IS_MAX_PLANES];
	u32				output_dva_yuv[IS_MAX_PLANES];
#endif
	u32				output_dva_tnr_wgt[IS_MAX_PLANES];
	u32				output_dva_tnr_prev[IS_MAX_PLANES];
#if defined(SOC_YPP)
	u32				output_dva_nrds[IS_MAX_PLANES];
	u32				output_dva_noise[IS_MAX_PLANES];
	u32				output_dva_hf[IS_MAX_PLANES];
	u32				output_dva_drc[IS_MAX_PLANES];
	u32				output_dva_hist[IS_MAX_PLANES];
#endif
#if defined(ENABLE_RGB_REPROCESSING)
	u32				output_dva_noise_rep[IS_MAX_PLANES];
#endif
	uint64_t			output_kva_ext[IS_MAX_PLANES];	/* use for EXT */

	u32				instance_id;
	u32				fcount;
	bool				reprocessing;
	u32				tnr_mode;
#ifdef CHAIN_USE_STRIPE_PROCESSING
	struct param_stripe_input	stripe_input;
#endif
};
#else
struct isp_param_set {
	struct taa_param_set		*taa_param;
	struct param_otf_input		otf_input;
	struct param_dma_input		dma_input;
	struct param_dma_input		prev_dma_input;	/* for TNR merger */
	struct param_otf_output		otf_output;
	struct param_dma_output		dma_output_chunk;
	struct param_dma_output		dma_output_yuv;

	u32				input_dva[IS_MAX_PLANES];
	u32				output_dva_chunk[IS_MAX_PLANES];
	u32				output_dva_yuv[IS_MAX_PLANES];
	uint64_t			output_kva_me[IS_MAX_PLANES];	/* ME or MCH */
	u32				tnr_prev_input_dva[IS_MAX_PLANES];

	u32				instance_id;
	u32				fcount;
	bool				reprocessing;
	u32				tnr_mode;
#ifdef CHAIN_USE_STRIPE_PROCESSING
	struct param_stripe_input	stripe_input;
#endif
};
#endif

struct tpu_param_set {
	struct param_control		control;
	struct param_tpu_config		config;
	struct param_otf_input		otf_input;
	struct param_dma_input		dma_input;
	struct param_otf_output		otf_output;
	struct param_dma_output		dma_output;
	u32				input_dva[IS_MAX_PLANES];
	u32				output_dva[IS_MAX_PLANES];

	u32				instance_id;
	u32				fcount;
	bool				reprocessing;
#ifdef CHAIN_USE_STRIPE_PROCESSING
	struct param_stripe_input	stripe_input;
#endif
};

struct dcp_param_set {
	struct param_control		control;
	struct param_dcp_config		config;

	struct param_dma_input		dma_input_m;	/* master input */
	struct param_dma_input		dma_input_s;	/* slave input */
	struct param_dma_output		dma_output_m;	/* master output */
	struct param_dma_output		dma_output_s;	/* slave output */
	struct param_dma_output		dma_output_m_ds;/* master Down Scale output */
	struct param_dma_output		dma_output_s_ds;/* slave Down Scale output */

	struct param_dma_input		dma_input_disparity;
	struct param_dma_output		dma_output_disparity;

	u32				input_dva[DCP_DMA_IN_MAX][4];
	u32				output_dva[DCP_DMA_OUT_MAX][4];

	u32 				instance_id;
	u32				fcount;
	bool				reprocessing;
};

struct clh_param_set {
	struct param_dma_input		dma_input;
	struct param_dma_output		dma_output;
	u32				input_dva[IS_MAX_PLANES];
	u32				svhist_input_dva[IS_MAX_PLANES];
	u32				output_dva[IS_MAX_PLANES];

	u32				scale_ratio;
	u32				instance_id;
	u32				fcount;
};

struct ypp_param_set {
	struct isp_param_set		*isp_param;
	struct param_dma_input		dma_input;
	struct param_dma_input		dma_input_lv2;
	struct param_dma_input		dma_input_noise;
	struct param_dma_input		dma_input_drc;
	struct param_dma_input		dma_input_hist;
	struct param_otf_output		otf_output;

	u32				input_dva[IS_MAX_PLANES];
	u32				input_dva_lv2[IS_MAX_PLANES];
	u32				input_dva_noise[IS_MAX_PLANES];
	u32				input_dva_drc[IS_MAX_PLANES];
	u32				input_dva_hist[IS_MAX_PLANES];
	u32				mode;
	u32				reserved[4];
	u32				tnr_mode;
	u32				instance_id;
	u32				fcount;
	bool				reprocessing;
#ifdef CHAIN_USE_STRIPE_PROCESSING
	struct param_stripe_input	stripe_input;
#endif
};

struct lme_param_set {
	struct param_lme_dma		dma;

	u32				cur_input_dva[IS_MAX_PLANES];
	u32				prev_input_dva[IS_MAX_PLANES];
	u32				output_dva[IS_MAX_PLANES];
	u64				output_kva_motion_pure[IS_MAX_PLANES];
	u64				output_kva_motion_processed[IS_MAX_PLANES];
	u32 				instance_id;
	u32				fcount;
	bool				reprocessing;
	u32				tnr_mode;
};

struct lib_callback_result {
	u32	fcount;
	u32	stripe_region_id;
	u32	reserved[4];
};

struct lib_callback_func {
	void (*camera_callback)(void *hw_ip, enum lib_cb_event_type event_id,
		u32 instance_id, void *data);
	void (*io_callback)(void *hw_ip, enum lib_cb_event_type event_id,
		u32 instance_id);
};

struct lib_tune_set {
	u32 index;
	ulong addr;
	u32 size;
	int decrypt_flag;
};

#define LIB_ISP_ADDR		(DDK_LIB_ADDR + LIB_ISP_OFFSET)
enum lib_func_type {
	LIB_FUNC_3AA = 1,
	LIB_FUNC_ISP,
	LIB_FUNC_TPU,
	LIB_FUNC_VRA,
	LIB_FUNC_DCP,
	LIB_FUNC_CLAHE,
	LIB_FUNC_YUVPP,
	LIB_FUNC_LME,
	LIB_FUNC_TYPE_END
};

enum lib_cmd_type {
	LIB_CMD_OVERFLOW_RECOVERY = 1,
	LIB_CMD_TIMEOUT = 2,
};

struct lib_system_config {
	enum lib_cmd_type cmd;
	u32 args[9];
};

typedef u32 (*register_lib_isp)(u32 type, void **lib_func);
static const register_lib_isp get_lib_func = (register_lib_isp)LIB_ISP_ADDR;

struct lib_interface_func {
	int (*chain_create)(u32 chain_id, ulong addr, u32 offset,
		const struct lib_callback_func *cb);
	int (*object_create)(void **pobject, u32 obj_info, void *hw);
	int (*chain_destroy)(u32 chain_id);
	int (*object_destroy)(void *object, u32 sensor_id);
	int (*stop)(void *object, u32 instance_id);
	int (*recovery)(u32 instance_id);
	int (*set_param)(void *object, void *param_set);
	int (*set_ctrl)(void *object, u32 instance_id, u32 frame_number,
		struct camera2_shot *shot);
	int (*shot)(void *object, void *param_set, struct camera2_shot *shot, u32 num_buffers);
	int (*get_meta)(void *object, u32 instance_id, u32 frame_number,
		struct camera2_shot *shot);
	int (*create_tune_set)(void *isp_object, u32 instance_id, struct lib_tune_set *set);
	int (*apply_tune_set)(void *isp_object, u32 instance_id, u32 index);
	int (*delete_tune_set)(void *isp_object, u32 instance_id, u32 index);
#if defined(SOC_33S)
	int (*set_line_buffer_offset)(u32 index, u32 num_set, u32 *offset);
#else
	int (*set_line_buffer_offset)(u32 index, u32 num_set, u32 *offset, u32 trigger_context);
#endif
	int (*change_chain)(void *isp_object, u32 instance_id, u32 chain_id);
	int (*load_cal_data)(void *isp_object, u32 instance_id, ulong kvaddr);
	int (*get_cal_data)(void *isp_object, u32 instance_id,
		struct cal_info *data, int type);
	int (*sensor_info_mode_chg)(void *isp_object, u32 instance_id,
		struct camera2_shot *shot);
	int (*sensor_update_ctl)(void *isp_object, u32 instance_id,
		u32 frame_count, struct camera2_shot *shot);
	int (*set_system_config)(struct lib_system_config *config);
#if defined(SOC_33S)
	int (*set_line_buffer_trigger)(u32 trigger_value, u32 trigger_event);
#endif
#if defined(SOC_YPP)
	int (*event_notifier)(int hw_id, int instance_id, u32 fcount, int event_id, u32 strip_index);
#endif
#if defined(USE_OFFLINE_PROCESSING)
	int (*get_offline_data)(void *isp_object, u32 instance,
		struct offline_data_desc *data_desc, int fcount);
#endif
};

int is_lib_isp_chain_create(struct is_hw_ip *hw_ip,
	struct is_lib_isp *this, u32 instance_id);
int is_lib_isp_object_create(struct is_hw_ip *hw_ip,
	struct is_lib_isp *this, u32 instance_id, u32 rep_flag, u32 module_id);
void is_lib_isp_chain_destroy(struct is_hw_ip *hw_ip,
	struct is_lib_isp *this, u32 instance_id);
void is_lib_isp_object_destroy(struct is_hw_ip *hw_ip,
	struct is_lib_isp *this, u32 instance_id);
int is_lib_isp_set_param(struct is_hw_ip *hw_ip,
	struct is_lib_isp *this, void *param);
int is_lib_isp_set_ctrl(struct is_hw_ip *hw_ip,
	struct is_lib_isp *this, struct is_frame *frame);
int is_lib_isp_shot(struct is_hw_ip *hw_ip,
	struct is_lib_isp *this, void *param_set, struct camera2_shot *shot);
int is_lib_isp_get_meta(struct is_hw_ip *hw_ip,
	struct is_lib_isp *this, struct is_frame *frame);
void is_lib_isp_stop(struct is_hw_ip *hw_ip,
	struct is_lib_isp *this, u32 instance_id);
int is_lib_isp_create_tune_set(struct is_lib_isp *this,
	ulong addr, u32 size, u32 index, int flag, u32 instance_id);
int is_lib_isp_apply_tune_set(struct is_lib_isp *this,
	u32 index, u32 instance_id);
int is_lib_isp_delete_tune_set(struct is_lib_isp *this,
	u32 index, u32 instance_id);
int is_lib_isp_change_chain(struct is_hw_ip *hw_ip,
	struct is_lib_isp *this, u32 instance_id, u32 next_id);
int is_lib_isp_load_cal_data(struct is_lib_isp *this,
	u32 index, ulong addr);
int is_lib_isp_get_cal_data(struct is_lib_isp *this,
	u32 instance_id, struct cal_info *c_info, int type);
int is_lib_isp_sensor_info_mode_chg(struct is_lib_isp *this,
	u32 instance_id, struct camera2_shot *shot);
int is_lib_isp_sensor_update_control(struct is_lib_isp *this,
	u32 instance_id, u32 frame_count, struct camera2_shot *shot);
int is_lib_isp_convert_face_map(struct is_hardware *hardware,
	struct taa_param_set *param_set, struct is_frame *frame);
void is_lib_isp_configure_algorithm(void);
void is_isp_get_bcrop1_size(void __iomem *base_addr, u32 *width, u32 *height);
int is_lib_isp_reset_recovery(struct is_hw_ip *hw_ip,
	struct is_lib_isp *this, u32 instance_id);
int is_lib_isp_event_notifier(struct is_hw_ip *hw_ip, struct is_lib_isp *this,
	int instance, u32 fcount, int event_id, u32 strip_index);
int is_lib_set_sram_offset(struct is_hw_ip *hw_ip,
	struct is_lib_isp *this, u32 instance_id);
int is_lib_notify_timeout(struct is_lib_isp *this, u32 instance);
int is_lib_get_offline_data(struct is_lib_isp *this, u32 instance,
	void *data_desc, int fcount);
#define CALL_LIBOP(lib, op, args...)					\
	({								\
		int ret_call_libop;					\
									\
		is_fpsimd_get_func();					\
		ret_call_libop = ((lib)->func->op ?			\
				(lib)->func->op(args) : -EINVAL);	\
		is_fpsimd_put_func();					\
									\
	ret_call_libop; })

#define CALL_LIBOP_NO_FPSIMD(lib, op, args...)				\
	((lib)->func->op ? (lib)->func->op(args) : -EINVAL)

#define CALL_LIBOP_ISR(lib, op, args...)				\
	({								\
		int ret_call_libop;					\
									\
		is_fpsimd_get_isr();					\
		ret_call_libop = ((lib)->func->op ?			\
				(lib)->func->op(args) : -EINVAL);	\
		is_fpsimd_put_isr();					\
									\
	ret_call_libop; })
#endif
