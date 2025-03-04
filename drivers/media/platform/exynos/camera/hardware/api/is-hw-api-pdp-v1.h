/*
 * Samsung Exynos SoC series Pablo driver
 *
 * Copyright (c) 2019 Samsung Electronics Co., Ltd
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#ifndef IS_HW_PDP_H
#define IS_HW_PDP_H

#include "is-hw-pdp.h"
#include "is-device-sensor.h"
#include "is-hw-api-common.h"

#define COREX_OFFSET 0x8000

#define COREX_IGNORE			(0)
#define COREX_COPY			(1)
#define COREX_SWAP			(2)

#define HW_TRIGGER			(0)
#define SW_TRIGGER			(1)

enum pdp_event_type {
	PE_START,
	PE_END,
	PE_PAF_STAT0,
	PE_PAF_STAT1,
	PE_PAF_STAT2,
	PE_ERR_INT1,
	PE_ERR_INT2,
	PE_PAF_OVERFLOW,
	PE_ERR_RDMA_IRQ,
	PE_SPECIAL,
};

enum pdp_input_path_type {
	OTF = 0,
	STRGEN = 1,
	DMA = 2,
};

/* status */
u32 pdp_hw_g_ip_version(void __iomem *base);
unsigned int pdp_hw_g_idle_state(void __iomem *base);
void pdp_hw_get_line(void __iomem *base, u32 *total_line, u32 *curr_line);

/* config */
void pdp_hw_s_before_sensor_start(void __iomem *base);
void pdp_hw_s_global_enable(void __iomem *base, bool enable);
void pdp_hw_s_global_enable_clear(void __iomem *base);
int pdp_hw_s_one_shot_enable(struct is_pdp *pdp);
void pdp_hw_s_corex_enable(void __iomem *base, bool enable);
void pdp_hw_s_corex_type(void __iomem *base, u32 type);
void pdp_hw_g_corex_state(void __iomem *base, u32 *corex_enable);
void pdp_hw_s_pd_size(void __iomem *base, u32 width, u32 height, u32 hwformat,
	u32 img_pixelsize);
void pdp_hw_s_img_size(void __iomem *base,
	struct is_crop full_size,
	struct is_crop crop_size,
	struct is_crop comp_size,
	u32 hwformat, u32 pixelsize,
	struct is_sensor_cfg *sensor_cfg);
void pdp_hw_s_core(struct is_pdp *pdp, bool pd_enable, struct is_sensor_cfg *sensor_cfg,
	struct is_crop img_full_size,
	struct is_crop img_crop_size,
	struct is_crop img_comp_size,
	u32 img_hwformat, u32 img_pixelsize,
	u32 pd_width, u32 pd_height, u32 pd_hwformat,
	u32 sensor_type, u32 path, int sensor_mode, u32 fps, u32 en_sdc, u32 en_votf,
	u32 num_buffers, u32 binning, u32 position);
void pdp_hw_s_init(void __iomem *base);
void pdp_hw_s_reset(void __iomem *base);
void pdp_hw_s_global(void __iomem *base, u32 ch, u32 lic_mode, u32 lic_14bit, void *data);
void pdp_hw_s_context(void __iomem *base, u32 curr_ch, u32 curr_path, u32 dma_only);
void pdp_hw_s_path(void __iomem *base, u32 path);
void pdp_hw_s_wdma_init(void __iomem *base, u32 ch);
u32 pdp_hw_g_wdma_addr(void __iomem *base);
void pdp_hw_s_wdma_enable(void __iomem *base, dma_addr_t address);
void pdp_hw_s_wdma_disable(void __iomem *base);
void pdp_hw_s_rdma_addr(void __iomem *base, dma_addr_t *address, u32 num_buffers, u32 direct,
	struct is_sensor_cfg *sensor_cfg);
void pdp_hw_update_rdma_linegap(void __iomem *base, struct is_sensor_cfg *sensor_cfg,
		struct is_pdp *pdp, u32 en_votf, u32 min_fps, u32 max_fps);
void pdp_hw_s_af_rdma_addr(void __iomem *base, dma_addr_t *address, u32 num_buffers, u32 direct);
void pdp_hw_s_post_frame_gap(void __iomem *base, u32 interval);
void pdp_hw_s_fro(void __iomem *base, u32 num_buffers);
int pdp_hw_wait_idle(void __iomem *base, unsigned long state, u32 frame_time);
int pdp_hw_rdma_wait_idle(void __iomem *base);
void pdp_hw_s_lic_bit_mode(void __iomem *base, u32 pixelsize);

/* stat */
int pdp_hw_g_stat0(void __iomem *base, void *buf, size_t len);
void pdp_hw_g_pdstat_size(u32 *width, u32 *height, u32 *bits_per_pixel);
void pdp_hw_s_pdstat_path(void __iomem *base, bool enable);
void pdp_hw_s_line_row(void __iomem *base, bool pd_enable, int sensor_mode, u32 binning);

/* sensor mode */
void pdp_hw_s_sensor_type(void __iomem *base, u32 sensor_type);
bool pdp_hw_to_sensor_type(u32 pd_mode, u32 *sensor_type);

/* interrupt */
unsigned int pdp_hw_g_int1_state(void __iomem *base, bool clear, u32 *irq_state);
unsigned int pdp_hw_g_int1_mask(void __iomem *base);
unsigned int pdp_hw_g_int2_state(void __iomem *base, bool clear, u32 *irq_state);
unsigned int pdp_hw_g_int2_mask(void __iomem *base);
unsigned int pdp_hw_g_int2_rdma_state(void __iomem *base, bool clear);
unsigned int pdp_hw_is_occurred(unsigned int state, enum pdp_event_type type);
void pdp_hw_g_int1_str(const char **int_str);
void pdp_hw_g_int2_str(const char **int_str);
void pdp_hw_g_int2_rdma_str(const char **int_str);

/* debug */
void pdp_hw_s_config_default(void __iomem *base);
int pdp_hw_dump(void __iomem *base);

/* bpc/reorder sram size for MSPD mode */
int pdp_hw_get_bpc_reorder_sram_size(u32 width, int vc_sensor_mode);
#endif
