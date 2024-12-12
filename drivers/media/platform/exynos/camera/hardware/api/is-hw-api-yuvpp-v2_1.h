/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Samsung Exynos SoC series Pablo driver
 *
 * ypp HW control APIs
 *
 * Copyright (C) 2020 Samsung Electronics Co., Ltd.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#ifndef IS_HW_API_YUVPP_V2_0_H
#define IS_HW_API_YUVPP_V2_0_H

#include "../is-hw-yuvpp.h"
#include "../is-hw-common-dma.h"


#define COREX_IGNORE			(0)
#define COREX_COPY			(1)
#define COREX_SWAP			(2)

#define HW_TRIGGER			(0)
#define SW_TRIGGER			(1)

#define HBLANK_CYCLE			(0x2D)

#define COMP_BLOCK_WIDTH		256
#define COMP_BLOCK_HEIGHT		1
#define COMP_LOSSYBYTE32NUM_256X1_U10	5

enum set_status {
	SET_SUCCESS,
	SET_ERROR,
};

enum ypp_event_type {
	INTR_FRAME_START,
	INTR_FRAME_END,
	INTR_COREX_END_0,
	INTR_COREX_END_1,
	INTR_ERR,
};

int ypp_hw_s_reset(void __iomem *base);
void ypp_hw_s_init(void __iomem *base);
unsigned int ypp_hw_is_occured(unsigned int status, enum ypp_event_type type);
int ypp_hw_wait_idle(void __iomem *base);
void ypp_hw_dump(void __iomem *base);
void ypp_hw_s_core(void __iomem *base, u32 num_buffers, u32 set_id);
void _ypp_hw_s_cout_fifo(void __iomem *base, u32 set_id);
void _ypp_hw_s_common(void __iomem *base);
void _ypp_hw_s_int_mask(void __iomem *base);
void _ypp_hw_s_secure_id(void __iomem *base, u32 set_id);
void _ypp_hw_s_fro(void __iomem *base, u32 num_buffers, u32 set_id);
void ypp_hw_g_hist_grid_num(void __iomem *base, u32 set_id, u32 *grid_x_num, u32 *grid_y_num);
u32 ypp_hw_g_nadd_use_noise_rdma(void __iomem *base, u32 set_id);
void ypp_hw_dma_dump(struct is_common_dma *dma);
void ypp_hw_s_rdma_corex_id(struct is_common_dma *dma, u32 set_id);
int ypp_hw_s_rdma_init(struct is_common_dma *dma, struct ypp_param_set *param_set,
	u32 enable, u32 grid_x_num, u32 grid_y_num, u32 in_crop_size_x, u32 cache_hint,
	u32 *sbwc_en, u32 *payload_size);
int ypp_hw_s_rdma_addr(struct is_common_dma *dma, u32 *addr, u32 plane, u32 num_buffers,
	int buf_idx, u32 comp_sbwc_en, u32 payload_size);
void ypp_hw_s_global_enable(void __iomem *base, bool enable);
int ypp_hw_s_one_shot_enable(void __iomem *base, struct is_hw_ypp *ypp);
int ypp_hw_s_corex_update_type(void __iomem *base, u32 set_id);
void ypp_hw_s_cmdq(void __iomem *base, u32 set_id);
void ypp_hw_s_corex_init(void __iomem *base, bool enable);
void _ypp_hw_wait_corex_idle(void __iomem *base);
unsigned int ypp_hw_g_int_state(void __iomem *base, bool clear, u32 num_buffers, u32 *irq_state);
unsigned int ypp_hw_g_int_mask(void __iomem *base);
void ypp_hw_yuv_in_crop_bypass(void __iomem *base);
void ypp_hw_yuv_out_crop_bypass(void __iomem *base);
void ypp_hw_yuvnr_bypass(void __iomem *base);
void ypp_hw_yuv422_to_rgb_bypass(void __iomem *base);
void ypp_hw_rgb_to_yuv422_bypass(void __iomem *base);
void ypp_hw_clahe_bypass(void __iomem *base);
void ypp_hw_prc_bypass(void __iomem *base);
void ypp_hw_skind_bypass(void __iomem *base);
void ypp_hw_noise_gen_bypass(void __iomem *base);
void ypp_hw_noise_adder_bypass(void __iomem *base);
void ypp_hw_oetf_gamma_bypass(void __iomem *base);
int ypp_hw_s_yuvnr_incrop(void __iomem *base, u32 set_id,
	bool strip_enable, u32 region_id, u32 total_count, u32 dma_width);
int ypp_hw_s_yuvnr_outcrop(void __iomem *base, u32 set_id, bool strip_enable, u32 region_id, u32 total_count);
int ypp_hw_s_yuvnr_size(void __iomem *base, u32 set_id, u32 yuvpp_strip_start_pos, u32 frame_width,
	u32 dma_width, u32 dma_height, u32 crop_x, u32 crop_y, u32 strip_enable,
	u32 sensor_full_width, u32 sensor_full_height,
	u32 sensor_binning_x, u32 sensor_binning_y, u32 sensor_crop_x, u32 sensor_crop_y,
	u32 taa_crop_x, u32 taa_crop_y, u32 taa_crop_width, u32 taa_crop_height);
int ypp_hw_s_clahe_size(void __iomem *base, u32 set_id, u32 frame_width, u32 dma_height,
	u32 yuvpp_strip_start_pos, u32 strip_enable);
void ypp_hw_s_nadd_size(void __iomem *base, u32 set_id, u32 yuvpp_strip_start_pos, u32 strip_enable);
void ypp_hw_s_coutfifo_size(void __iomem *base, u32 set_id, u32 dma_width, u32 dma_height, bool strip_enable);
int ypp_hw_dma_create(struct is_common_dma *dma, void __iomem *base, u32 input_id);
void ypp_hw_s_block_bypass(void __iomem *base, u32 set_id);
u32 ypp_hw_g_in_crop_size_x(void __iomem *base, u32 set_id);
#endif
