/*
 * Samsung EXYNOS CAMERA PostProcessing GDC driver
 *
 * Copyright (C) 2016 Samsung Electronics Co., Ltd.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#include "camerapp-hw-api-gdc.h"
#include "camerapp-hw-reg-gdc-v100.h"


void camerapp_hw_gdc_start(void __iomem *base_addr)
{
	camerapp_sfr_set_field(base_addr, &gdc_regs[GDC_R_FRAMESTART_CMD], &gdc_fields[GDC_F_FRAMESTART_CMD], 1);
}

void camerapp_hw_gdc_stop(void __iomem *base_addr)
{
	/* GDC QCH off */
	camerapp_sfr_set_reg(base_addr, &gdc_regs[GDC_R_GDC_PROCESSING], 0);
}

u32 camerapp_hw_gdc_sw_reset(void __iomem *base_addr)
{
	u32 reset_count = 0;

	/* request to stop bus IF */
	camerapp_sfr_set_field(base_addr, &gdc_regs[GDC_R_REQ_STOP], &gdc_fields[GDC_F_EN_RESET], 1);

	/* wait bus IF reset complete */
	do {
		reset_count++;
		if (reset_count > 10000)
			return reset_count;
	} while (camerapp_sfr_get_field(base_addr, &gdc_regs[GDC_R_REQ_STOP], &gdc_fields[GDC_F_RESET_OK]) != 1);

	/* request to gdc hw */
	camerapp_sfr_set_field(base_addr, &gdc_regs[GDC_R_GDC_SW_RESET], &gdc_fields[GDC_F_SW_RESET], 1);

	/* wait reset complete */
	do {
		reset_count++;
		if (reset_count > 10000)
			return reset_count;
	} while (camerapp_sfr_get_field(base_addr, &gdc_regs[GDC_R_GDC_SW_RESET], &gdc_fields[GDC_F_SW_RESET]) != 0);

	return 0;
}

void camerapp_hw_gdc_clear_intr_all(void __iomem *base_addr)
{
	camerapp_sfr_set_reg(base_addr, &gdc_regs[GDC_R_GDC_INT_STATUS], 0x0001);
}

void camerapp_hw_gdc_mask_intr(void __iomem *base_addr, u32 intr_mask)
{
	camerapp_sfr_set_reg(base_addr, &gdc_regs[GDC_R_GDC_INT_MASK], intr_mask);
}

u32 camerapp_hw_gdc_get_intr_status(void __iomem *base_addr)
{
	return camerapp_sfr_get_reg(base_addr, &gdc_regs[GDC_R_GDC_INT_STATUS]);
}

u32 camerapp_hw_gdc_get_intr_status_and_clear(void __iomem *base_addr)
{
	u32 intStatus;

	intStatus = camerapp_sfr_get_reg(base_addr, &gdc_regs[GDC_R_GDC_INT_STATUS]);
	camerapp_sfr_set_reg(base_addr, &gdc_regs[GDC_R_GDC_INT_STATUS], intStatus);

	return intStatus;
}

void camerapp_hw_gdc_update_grid_param(void __iomem *base_addr, struct gdc_grid_param *grid_param)
{
	u32 i = 0, j = 0;
	u32 sfr_offset = 0x0004;
	u32 sfr_start_x = 0x0100;
	u32 sfr_start_y = 0x0200;

	if (grid_param->is_valid == true) {
		for (i = 0; i < GRID_Y_SIZE; i++) {
			for (j = 0; j < GRID_X_SIZE; j++) {
				u32 cal_sfr_offset = (sfr_offset * i * GRID_X_SIZE) + (sfr_offset * j);
				writel((u32)grid_param->dx[i][j], base_addr + sfr_start_x + cal_sfr_offset);
				writel((u32)grid_param->dy[i][j], base_addr + sfr_start_y + cal_sfr_offset);
			}
		}
	}
}

void camerapp_hw_gdc_update_scale_parameters(void __iomem *base_addr, struct gdc_frame *s_frame,
						struct gdc_frame *d_frame, struct gdc_crop_param *crop_param)
{
	u32 gdc_input_width;
	u32 gdc_input_height;
	u32 gdc_crop_offset_x;
	u32 gdc_crop_offset_y;
	u32 gdc_crop_width;
	u32 gdc_crop_height;
	u32 gdc_scale_shifter_x;
	u32 gdc_scale_shifter_y;
	u32 gdc_scale_x;
	u32 gdc_scale_y;
	u32 gdc_inv_scale_x;
	u32 gdc_inv_scale_y;
	u32 gdc_output_offset_x;
	u32 gdc_output_offset_y;
	u32 gdc_output_width;
	u32 gdc_output_height;

	u32 scaleShifterX;
	u32 scaleShifterY;
	u32 imagewidth;
	u32 imageheight;

	/* GDC original input size */
	gdc_input_width = s_frame->width;
	gdc_input_height = s_frame->height;

	/* GDC input crop size from original input size */
	gdc_crop_offset_x = crop_param->crop_start_x;
	gdc_crop_offset_y = crop_param->crop_start_y;
	gdc_crop_width = crop_param->crop_width;
	gdc_crop_height = crop_param->crop_height;
	if (gdc_crop_width == 0) {
		gdc_crop_offset_x = 0;
		gdc_crop_offset_y = 0;
		gdc_crop_width = gdc_input_width;
		gdc_crop_height = gdc_input_height;
	}

	/* Real to virtual grid scaling factor shifters */
	scaleShifterX  = DS_SHIFTER_MAX;
	imagewidth = gdc_input_width << 1;
	while ((imagewidth <= MAX_VIRTUAL_GRID_X) && (scaleShifterX > 0)) {
		imagewidth <<= 1;
		scaleShifterX--;
	}
	gdc_scale_shifter_x = scaleShifterX;

	scaleShifterY  = DS_SHIFTER_MAX;
	imageheight = gdc_input_height << 1;
	while ((imageheight <= MAX_VIRTUAL_GRID_Y) && (scaleShifterY > 0)) {
		imageheight <<= 1;
		scaleShifterY--;
	}
	gdc_scale_shifter_y = scaleShifterY;

	/* Real to virtual grid scaling factors */
	gdc_scale_x = MIN(65535,
		((MAX_VIRTUAL_GRID_X << (DS_FRACTION_BITS + scaleShifterX)) + gdc_input_width / 2) / gdc_input_width);
	gdc_scale_y = MIN(65535,
		((MAX_VIRTUAL_GRID_Y << (DS_FRACTION_BITS + scaleShifterY)) + gdc_input_height / 2) / gdc_input_height);

	/* Virtual to real grid scaling factors */
	gdc_inv_scale_x = gdc_input_width;
	gdc_inv_scale_y = ((gdc_input_height << 3) + 3) / 6;

	/* Meaningful only when out_crop_bypass = 0, x <= (gdc_crop_width - gdc_image_active_width) */
	gdc_output_width = d_frame->width;
	gdc_output_height = d_frame->height;
	if ((gdc_output_width & (GDC_WIDTH_ALIGN - 1)) || (gdc_output_height & (GDC_HEIGHT_ALIGN - 1))) {
		gdc_dbg("gdc output width(%d) is not (%d)align or height(%d) is not (%d)align.\n",
				gdc_output_width, GDC_WIDTH_ALIGN,
				gdc_output_height, GDC_HEIGHT_ALIGN);
		gdc_output_width = ALIGN(gdc_output_width, GDC_WIDTH_ALIGN);
		gdc_output_height = ALIGN(gdc_output_height, GDC_HEIGHT_ALIGN);
	}

	if (gdc_crop_width < gdc_output_width)
		gdc_output_offset_x = 0;
	else
		gdc_output_offset_x = ALIGN((gdc_crop_width - gdc_output_width) >> 1, GDC_OFFSET_ALIGN);

	if (gdc_crop_height < gdc_output_height)
		gdc_output_offset_y = 0;
	else
		gdc_output_offset_y = ALIGN((gdc_crop_height - gdc_output_height) >> 1, GDC_OFFSET_ALIGN);

	camerapp_sfr_set_field(base_addr, &gdc_regs[GDC_R_GDC_CONFIG],
		&gdc_fields[GDC_F_GDC_MIRROR_X], 0);
	camerapp_sfr_set_field(base_addr, &gdc_regs[GDC_R_GDC_CONFIG],
		&gdc_fields[GDC_F_GDC_MIRROR_Y], 0);

#if 0
	camerapp_sfr_set_field(base_addr, &gdc_regs[GDC_R_GDC_INPUT_ORG_SIZE],
		&gdc_fields[GDC_F_GDC_ORG_HEIGHT], gdc_input_height);
	camerapp_sfr_set_field(base_addr, &gdc_regs[GDC_R_GDC_INPUT_ORG_SIZE],
		&gdc_fields[GDC_F_GDC_ORG_WIDTH], gdc_input_width);
#endif

	camerapp_sfr_set_field(base_addr, &gdc_regs[GDC_R_GDC_INPUT_CROP_START],
		&gdc_fields[GDC_F_GDC_CROP_START_X], gdc_crop_offset_x);
	camerapp_sfr_set_field(base_addr, &gdc_regs[GDC_R_GDC_INPUT_CROP_START],
		&gdc_fields[GDC_F_GDC_CROP_START_Y], gdc_crop_offset_y);
	camerapp_sfr_set_field(base_addr, &gdc_regs[GDC_R_GDC_INPUT_CROP_SIZE],
		&gdc_fields[GDC_F_GDC_CROP_WIDTH], gdc_crop_width);
	camerapp_sfr_set_field(base_addr, &gdc_regs[GDC_R_GDC_INPUT_CROP_SIZE],
		&gdc_fields[GDC_F_GDC_CROP_HEIGHT], gdc_crop_height);

	camerapp_sfr_set_field(base_addr, &gdc_regs[GDC_R_GDC_SCALE],
		&gdc_fields[GDC_F_GDC_SCALE_X], gdc_scale_x);
	camerapp_sfr_set_field(base_addr, &gdc_regs[GDC_R_GDC_SCALE],
		&gdc_fields[GDC_F_GDC_SCALE_Y], gdc_scale_y);
	camerapp_sfr_set_field(base_addr, &gdc_regs[GDC_R_GDC_SCALE_SHIFTER],
		&gdc_fields[GDC_F_GDC_SCALE_SHIFTER_X], gdc_scale_shifter_x);
	camerapp_sfr_set_field(base_addr, &gdc_regs[GDC_R_GDC_SCALE_SHIFTER],
		&gdc_fields[GDC_F_GDC_SCALE_SHIFTER_Y], gdc_scale_shifter_y);

	camerapp_sfr_set_field(base_addr, &gdc_regs[GDC_R_GDC_INV_SCALE],
		&gdc_fields[GDC_F_GDC_INV_SCALE_X], gdc_inv_scale_x);
	camerapp_sfr_set_field(base_addr, &gdc_regs[GDC_R_GDC_INV_SCALE],
		&gdc_fields[GDC_F_GDC_INV_SCALE_Y], gdc_inv_scale_y);

	camerapp_sfr_set_field(base_addr, &gdc_regs[GDC_R_GDC_OUT_CROP_START],
		&gdc_fields[GDC_F_GDC_IMAGE_CROP_PRE_X], gdc_output_offset_x);
	camerapp_sfr_set_field(base_addr, &gdc_regs[GDC_R_GDC_OUT_CROP_START],
		&gdc_fields[GDC_F_GDC_IMAGE_CROP_PRE_Y], gdc_output_offset_y);
	camerapp_sfr_set_field(base_addr, &gdc_regs[GDC_R_GDC_OUT_CROP_SIZE],
		&gdc_fields[GDC_F_GDC_IMAGE_ACTIVE_WIDTH], gdc_output_width);
	camerapp_sfr_set_field(base_addr, &gdc_regs[GDC_R_GDC_OUT_CROP_SIZE],
		&gdc_fields[GDC_F_GDC_IMAGE_ACTIVE_HEIGHT], gdc_output_height);

	gdc_dbg("input %dx%d in_crop %d,%d %dx%d -> out_crop %d,%d %dx%d / no scale up\n",
			gdc_input_width, gdc_input_height,
			gdc_crop_offset_x, gdc_crop_offset_y, gdc_crop_width, gdc_crop_height,
			gdc_output_offset_x, gdc_output_offset_y, gdc_output_width, gdc_output_height);
}

void camerapp_hw_gdc_update_dma_size(void __iomem *base_addr, struct gdc_frame *s_frame, struct gdc_frame *d_frame)
{
	u32 input_dma_width;
	u32 output_dma_width;

	/* dma size : $%16 == 0 */
	/* same as lum, chroma value : 420 bit only */
	input_dma_width = ALIGN(((s_frame->width + 15) / 16) * 16, 16);
	output_dma_width = ALIGN(((d_frame->width + 15) / 16) * 16, 16);
	camerapp_sfr_set_reg(base_addr, &gdc_regs[GDC_R_PXC_STRIDE_LUM], input_dma_width);		/* RDMA */
	camerapp_sfr_set_reg(base_addr, &gdc_regs[GDC_R_PXC_STRIDE_CHROMA], input_dma_width);
	camerapp_sfr_set_reg(base_addr, &gdc_regs[GDC_R_WAXI_STRIDE_LUM], output_dma_width);		/* WDMA */
	camerapp_sfr_set_reg(base_addr, &gdc_regs[GDC_R_WAXI_STRIDE_CHROMA], output_dma_width);

	return;
}

void camerapp_hw_gdc_set_dma_address(void __iomem *base_addr, struct gdc_frame *s_frame, struct gdc_frame *d_frame)
{
	/*Dst Base address of the first Y, UV plane in the DRAM.  must be [1:0] == 0 : Must be multiple of 4*/
	camerapp_sfr_set_field(base_addr, &gdc_regs[GDC_R_GDC_DMA_ADDRESS],
		&gdc_fields[GDC_F_GDC_DMA_ADDRESS], d_frame->addr.y);
	camerapp_sfr_set_field(base_addr, &gdc_regs[GDC_R_GDC_DMA_ADDRESS_2],
		&gdc_fields[GDC_F_GDC_DMA_ADDRESS_2], d_frame->addr.cb);

	/* RDMA address */
	camerapp_sfr_set_field(base_addr, &gdc_regs[GDC_R_PXC_DPB_BASE_LUM],
		&gdc_fields[GDC_F_DPBLUMBASE], s_frame->addr.y);
	camerapp_sfr_set_field(base_addr, &gdc_regs[GDC_R_PXC_DPB_BASE_CHROMA],
		&gdc_fields[GDC_F_DPBCHRBASE], s_frame->addr.cb);
}

void camerapp_hw_gdc_set_format(void __iomem *base_addr, struct gdc_frame *s_frame, struct gdc_frame *d_frame)
{
	/* Pixel format = 0: NV12 (2plane Y/UV order) / 1: NV21 (2plane Y/VU order) / No 3plane */
	if (s_frame->gdc_fmt->pixelformat == d_frame->gdc_fmt->pixelformat)
		camerapp_sfr_set_field(base_addr, &gdc_regs[GDC_R_PXC_PIXEL_FORMAT], &gdc_fields[GDC_F_PIXELFORMAT], 0);
	else
		camerapp_sfr_set_field(base_addr, &gdc_regs[GDC_R_PXC_PIXEL_FORMAT], &gdc_fields[GDC_F_PIXELFORMAT], 1);
	camerapp_sfr_set_field(base_addr, &gdc_regs[GDC_R_PXC_PIXEL_FORMAT], &gdc_fields[GDC_F_ENDIAN], 0);	/* 0 : Little endian  / 1:  Big endian */
}

void camerapp_hw_gdc_update_param(void __iomem *base_addr, struct gdc_dev *gdc)
{
	struct gdc_frame *d_frame, *s_frame;
	struct gdc_crop_param *crop_param;

	s_frame = &gdc->current_ctx->s_frame;
	d_frame = &gdc->current_ctx->d_frame;
	crop_param = &gdc->current_ctx->crop_param;

	/* interpolation type */
	/* 0 -all closest / 1 - Y bilinear, UV closest / 2 - all bilinear / 3 - Y bi-cubic, UV bilinear */
	camerapp_sfr_set_field(base_addr, &gdc_regs[GDC_R_GDC_INTERPOLATION],
		&gdc_fields[GDC_F_GDC_INTERP_TYPE], 3);
	/* Clamping type: 0 - none / 1 - min/max / 2 - near pixels min/max */
	camerapp_sfr_set_field(base_addr, &gdc_regs[GDC_R_GDC_INTERPOLATION],
		&gdc_fields[GDC_F_GDC_CLAMP_TYPE], 2);

	/* gdc grid scale setting */
	camerapp_hw_gdc_update_scale_parameters(base_addr, s_frame, d_frame, crop_param);
	/* gdc grid setting */
	camerapp_hw_gdc_update_grid_param(base_addr, &gdc->current_ctx->grid_param);

	/* dma buffer size & address setting */
	camerapp_hw_gdc_set_dma_address(base_addr, s_frame, d_frame);
	camerapp_hw_gdc_update_dma_size(base_addr, s_frame, d_frame);

	/* in / out data Format */
	camerapp_hw_gdc_set_format(base_addr, s_frame, d_frame);

	/* The values are multiples of 32, value : 32 ~ 512 */
	camerapp_sfr_set_field(base_addr, &gdc_regs[GDC_R_GDC_PRO_SIZE], &gdc_fields[GDC_F_GDC_PRO_WIDTH], 64);
	/* The values are multiples of 8, value : 8 ~ 512 */
	camerapp_sfr_set_field(base_addr, &gdc_regs[GDC_R_GDC_PRO_SIZE], &gdc_fields[GDC_F_GDC_PRO_HEIGHT], 64);

	camerapp_sfr_set_reg(base_addr, &gdc_regs[GDC_R_GDC_PROCESSING], 1);
}


void camerapp_hw_gdc_status_read(void __iomem *base_addr)
{
	u32 ReqCntLum;
	u32 HitCntLum;
	u32 isPixelBusy;
	u32 isPortBusy;
	u32 cacheStatus;
	u32 hitFifoStatus;
	u32 missFifoStatus;
	u32 isPending;

	/* read Total request count of luminance cache channel #0 */
	ReqCntLum = camerapp_sfr_get_reg(base_addr, &gdc_regs[GDC_R_PXC_REQ_CNT_LUM_0]);
	HitCntLum = camerapp_sfr_get_reg(base_addr, &gdc_regs[GDC_R_PXC_HIT_CNT_LUM_0]);
	isPixelBusy = camerapp_sfr_get_field(base_addr, &gdc_regs[GDC_R_PXC_DEBUG_BUSY], &gdc_fields[GDC_F_PXCBUSYLUM]);
	isPortBusy = camerapp_sfr_get_field(base_addr, &gdc_regs[GDC_R_PXC_DEBUG_BUSY], &gdc_fields[GDC_F_PORT0BUSY]);
	cacheStatus = camerapp_sfr_get_field(base_addr, &gdc_regs[GDC_R_PXC_DEBUG_LUM0], &gdc_fields[GDC_F_CHROMACACHEST]);
	hitFifoStatus = camerapp_sfr_get_field(base_addr, &gdc_regs[GDC_R_PXC_DEBUG_LUM0], &gdc_fields[GDC_F_HITFIFOST]);
	missFifoStatus = camerapp_sfr_get_field(base_addr, &gdc_regs[GDC_R_PXC_DEBUG_LUM0], &gdc_fields[GDC_F_MISSFIFOST]);
	isPending = camerapp_sfr_get_reg(base_addr, &gdc_regs[GDC_R_PEND]);

	pr_info("[GDC_Status] = ReqCntLum(%x) HitCntLum(%x) isPixelBusy(%x) isPortBusy(%x)\
		cacheStatus0(%x) hit(%x) miss(%x) pending(%x)\n",
		ReqCntLum, HitCntLum,
		isPixelBusy, isPortBusy,
		cacheStatus, hitFifoStatus, missFifoStatus,
		isPending);
}

void camerapp_hw_gdc_votf_enable(void __iomem *base_addr, u8 rw)
{
	return;
}

void camerapp_gdc_sfr_dump(void __iomem *base_addr)
{
	u32 i = 0;
	u32 reg_value = 0;

	gdc_dbg("gdc v1.0");

	for(i = 0; i < GDC_REG_CNT; i++) {
		reg_value = readl(base_addr + gdc_regs[i].sfr_offset);
		pr_info("[DUMP] reg:[%s][0x%04X], value:[0x%08X]\n",
			gdc_regs[i].reg_name, gdc_regs[i].sfr_offset, reg_value);
	}
}
