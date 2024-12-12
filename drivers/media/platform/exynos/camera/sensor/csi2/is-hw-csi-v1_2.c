/*
 * Samsung Exynos SoC series FIMC-IS2 driver
 *
 * exynos fimc-is2 hw csi control functions
 *
 * Copyright (c) 2019 Samsung Electronics Co., Ltd
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
#include <linux/io.h>
#if IS_ENABLED(CONFIG_EXYNOS_OTP)
#include <linux/exynos_otp.h>
#endif

#include "is-hw-api-common.h"
#include "is-config.h"
#include "is-type.h"
#include "is-device-csi.h"
#include "is-hw.h"
#include "is-hw-csi-v1_2.h"
#include "is-device-sensor.h"

u32 csi_hw_get_version(u32 __iomem *base_reg)
{
	return is_hw_get_reg(base_reg, &csi_regs[CSIS_R_CSIS_VERSION]);
}

void csi_hw_phy_otp_config(u32 __iomem *base_reg, u32 instance)
{
#if IS_ENABLED(CONFIG_EXYNOS_OTP)
#endif
}

u32 csi_hw_s_fcount(u32 __iomem *base_reg, u32 vc, u32 count)
{
	is_hw_set_reg(base_reg, &csi_regs[CSIS_R_FRM_CNT_CH0 + vc], count);

	return is_hw_get_reg(base_reg, &csi_regs[CSIS_R_FRM_CNT_CH0 + vc]);
}

u32 csi_hw_g_fcount(u32 __iomem *base_reg, u32 vc)
{
	return is_hw_get_reg(base_reg, &csi_regs[CSIS_R_FRM_CNT_CH0 + vc]);
}

int csi_hw_reset(u32 __iomem *base_reg)
{
	int ret = 0;
	u32 retry = 10;

	is_hw_set_field(base_reg, &csi_regs[CSIS_R_CSIS_CMN_CTRL],
			&csi_fields[CSIS_F_SW_RESET], 1);

	while (--retry) {
		udelay(10);
		if (is_hw_get_field(base_reg, &csi_regs[CSIS_R_CSIS_CMN_CTRL],
			&csi_fields[CSIS_F_SW_RESET]) != 1)
			break;
	}

	if (!retry) {
		err("reset is fail(%d)", retry);
		ret = -EINVAL;
		goto p_err;
	}

p_err:
	return ret;
}

int csi_hw_s_settle(u32 __iomem *base_reg,
	u32 settle)
{
	return 0;
}

int csi_hw_s_phy_default_value(u32 __iomem *base_reg, u32 instance)
{
	return 0;
}

int csi_hw_s_lane(u32 __iomem *base_reg,
	struct is_image *image, u32 lanes, u32 mipi_speed, u32 use_cphy)
{
	u32 val = is_hw_get_reg(base_reg, &csi_regs[CSIS_R_CSIS_CMN_CTRL]);
	u32 lane;

	/* lane number */
	val = is_hw_set_field_value(val, &csi_fields[CSIS_F_LANE_NUMBER], lanes);
	is_hw_set_reg(base_reg, &csi_regs[CSIS_R_CSIS_CMN_CTRL], val);

	if (use_cphy) {
		/* TO DO: set valid lane values for Cphy */
		switch (lanes) {
		case CSI_DATA_LANES_1:
			/* lane 0 */
			lane = (0x3);
			break;
		case CSI_DATA_LANES_2:
			/* lane 0 + lane 1 */
			lane = (0x3);
			break;
		case CSI_DATA_LANES_3:
			/* lane 0 + lane 1 + lane 2 */
			lane = (0xF);
			break;
		default:
			err("lanes is invalid(%d)", lanes);
			lane = (0xF);
			break;
		}
	} else {
		switch (lanes) {
		case CSI_DATA_LANES_1:
			/* lane 0 */
			lane = (0x1);
			break;
		case CSI_DATA_LANES_2:
			/* lane 0 + lane 1 */
			lane = (0x3);
			break;
		case CSI_DATA_LANES_3:
			/* lane 0 + lane 1 + lane 2 */
			lane = (0x7);
			break;
		case CSI_DATA_LANES_4:
			/* lane 0 + lane 1 + lane 2 + lane 3 */
			lane = (0xF);
			break;
		default:
			err("lanes is invalid(%d)", lanes);
			lane = (0xF);
			break;
		}
	}

	is_hw_set_field(base_reg, &csi_regs[CSIS_R_PHY_CMN_CTRL],
			&csi_fields[CSIS_F_ENABLE_DAT], lane);

	return 0;
}

int csi_hw_s_control(u32 __iomem *base_reg, u32 id, u32 value)
{
	switch (id) {
	case CSIS_CTRL_INTERLEAVE_MODE:
		/* interleave mode */
		is_hw_set_field(base_reg, &csi_regs[CSIS_R_CSIS_CMN_CTRL],
				&csi_fields[CSIS_F_INTERLEAVE_MODE], value);
		break;
	case CSIS_CTRL_LINE_RATIO:
		/* line irq ratio */
		is_hw_set_field(base_reg, &csi_regs[CSIS_R_LINE_INTR_CH0],
				&csi_fields[CSIS_F_LINE_INTR_CH_N], value);
		break;
	case CSIS_CTRL_DMA_ABORT_REQ:
		/* dma abort req */
		is_hw_set_field(base_reg, &csi_vcdma_cmn_regs[CSIS_R_DMA_CMN_CTRL],
				&csi_vcdma_cmn_fields[CSIS_F_DMA_ABORT_REQ], value);
		break;
	case CSIS_CTRL_ENABLE_LINE_IRQ:
		if (!value) {
			is_hw_set_field(base_reg, &csi_regs[CSIS_R_CSIS_INT_MSK1],
					&csi_fields[CSIS_F_MSK_LINE_END], 0x0);
		} else {
			is_hw_set_field(base_reg, &csi_regs[CSIS_R_CSIS_INT_MSK1],
					&csi_fields[CSIS_F_MSK_LINE_END], 0x1);
			is_hw_set_field(base_reg, &csi_regs[CSIS_R_LINE_END_MSK],
					&csi_fields[CSIS_F_MSK_LINE_END_CH], value);
		}
		break;
	case CSIS_CTRL_PIXEL_ALIGN_MODE:
		is_hw_set_field(base_reg, &csi_regs[CSIS_R_DBG_OPTION_SUITE],
					&csi_fields[CSIS_F_PACKED_PIXEL_MODE], value);
		break;
	case CSIS_CTRL_LRTE:
	case CSIS_CTRL_DESCRAMBLE:
		break;
	default:
		err("control id is invalid(%d)", id);
		break;
	}

	return 0;
}

int csi_hw_s_config(u32 __iomem *base_reg,
	u32 vc, struct is_vci_config *config, u32 width, u32 height, bool potf)
{
	int ret = 0;
	u32 val, parallel;
	u32 pd_mode;
	u32 pixel_mode = CSIS_PIXEL_MODE_DUAL;
	struct is_sensor_cfg *sensor_cfg;

	sensor_cfg = container_of(config, struct is_sensor_cfg, input[vc]);
	pd_mode = sensor_cfg->pd_mode;

	if (vc > CSI_VIRTUAL_CH_3) {
		err("invalid vc(%d)", vc);
		ret = -EINVAL;
		goto p_err;
	}

	if ((config->hwformat == HW_FORMAT_YUV420_8BIT) ||
		(config->hwformat == HW_FORMAT_YUV422_8BIT))
		parallel = 1;
	else
		parallel = 0;

	val = is_hw_get_reg(base_reg, &csi_regs[CSIS_R_ISP_CONFIG_CH0 + (vc * 3)]);
	val = is_hw_set_field_value(val, &csi_fields[CSIS_F_VIRTUAL_CHANNEL], config->map);
	val = is_hw_set_field_value(val, &csi_fields[CSIS_F_DATAFORMAT], config->hwformat);
	val = is_hw_set_field_value(val, &csi_fields[CSIS_F_DECOMP_EN], config->hwformat >> DECOMP_EN_BIT);
	val = is_hw_set_field_value(val, &csi_fields[CSIS_F_DECOMP_PREDICT],
		config->hwformat >> DECOMP_PREDICT_BIT);
	val = is_hw_set_field_value(val, &csi_fields[CSIS_F_POTF_ONLY], parallel);
	val = is_hw_set_field_value(val, &csi_fields[CSIS_F_PIXEL_MODE], pixel_mode);
	is_hw_set_reg(base_reg, &csi_regs[CSIS_R_ISP_CONFIG_CH0 + (vc * 3)], val);

	val = is_hw_get_reg(base_reg, &csi_regs[CSIS_R_ISP_RESOL_CH0 + (vc * 3)]);
	val = is_hw_set_field_value(val, &csi_fields[CSIS_F_VRESOL], height);
	val = is_hw_set_field_value(val, &csi_fields[CSIS_F_HRESOL], width);
	is_hw_set_reg(base_reg, &csi_regs[CSIS_R_ISP_RESOL_CH0 + (vc * 3)], val);

	if (config->hwformat & (1 << DECOMP_EN_BIT)) {
		val = is_hw_get_reg(base_reg, &csi_regs[CSIS_R_ISP_SYNC_CH0 + (vc * 3)]);
		val = is_hw_set_field_value(val, &csi_fields[CSIS_F_HSYNC_LINTV], 7);
		is_hw_set_reg(base_reg, &csi_regs[CSIS_R_ISP_SYNC_CH0 + (vc * 3)], val);
	}

p_err:
	return ret;
}

int csi_hw_s_config_dma(u32 __iomem *base_reg, u32 vc, struct is_frame_cfg *cfg, u32 hwformat, u32 dummy_pixel)
{
	int ret = 0;
	u32 val;
	u32 dma_dim = 0;
	u32 dma_pack12 = 0;
	u32 dma_format = 3; /* reserved value */
	u32 dma_stg_mode;

	if (vc > CSI_VIRTUAL_CH_3) {
		err("invalid vc(%d)", vc);
		ret = -EINVAL;
		goto p_err;
	}

	if (!cfg->format) {
		err("cfg->format is null");
		ret = -EINVAL;
		goto p_err;
	}

	switch (cfg->format->pixelformat)
	{
	case V4L2_PIX_FMT_SGRBG8:
	case V4L2_PIX_FMT_SBGGR8:
		dma_dim = CSIS_REG_DMA_1D_DMA;
		dma_pack12 = CSIS_REG_DMA_PACK;
		break;
	case V4L2_PIX_FMT_SBGGR10:
	case V4L2_PIX_FMT_SBGGR12:
	case V4L2_PIX_FMT_SBGGR16:
		dma_dim = CSIS_REG_DMA_2D_DMA;
		dma_pack12 = CSIS_REG_DMA_NORMAL;
		break;
	case V4L2_PIX_FMT_SBGGR10P:
	case V4L2_PIX_FMT_SBGGR12P:
	case V4L2_PIX_FMT_PRIV_MAGIC:
		dma_dim = CSIS_REG_DMA_2D_DMA;
		dma_pack12 = CSIS_REG_DMA_PACK;
		break;
	default:
		dma_dim = CSIS_REG_DMA_2D_DMA;
		dma_pack12 = CSIS_REG_DMA_NORMAL;
		break;
	}

	switch (hwformat) {
	case HW_FORMAT_RAW10:
	case HW_FORMAT_RAW6_DA:
	case HW_FORMAT_RAW7_DS:
	case HW_FORMAT_RAW8_DS:
		dma_format = 0;
		dma_stg_mode = CSIS_REG_DMA_STG_LEGACY_MODE;
		break;
	case HW_FORMAT_RAW12:
		dma_format = 1;
		dma_stg_mode = CSIS_REG_DMA_STG_LEGACY_MODE;
		break;
	case HW_FORMAT_RAW14:
	case HW_FORMAT_RAW10_DA:
		dma_format = 2;
		dma_stg_mode = CSIS_REG_DMA_STG_LEGACY_MODE;
		break;
	case HW_FORMAT_USER:
	case HW_FORMAT_USER1:
	case HW_FORMAT_USER2:
	case HW_FORMAT_EMBEDDED_8BIT:
	case HW_FORMAT_YUV420_8BIT:
	case HW_FORMAT_YUV420_10BIT:
	case HW_FORMAT_YUV422_8BIT:
	case HW_FORMAT_YUV422_10BIT:
	case HW_FORMAT_RGB565:
	case HW_FORMAT_RGB666:
	case HW_FORMAT_RGB888:
	case HW_FORMAT_RAW6:
	case HW_FORMAT_RAW7:
	case HW_FORMAT_RAW8:
		dma_stg_mode = CSIS_REG_DMA_STG_PACKET_MODE;
		break;
	default:
		err("invalid data format (%02X)", hwformat);
		ret = -EINVAL;
		goto p_err;
	}

	val = is_hw_get_reg(base_reg, &csi_vcdma_regs[CSIS_R_DMA0_FMT]);
	val = is_hw_set_field_value(val, &csi_vcdma_fields[CSIS_F_DMA_N_DIM], dma_dim);
	val = is_hw_set_field_value(val, &csi_vcdma_fields[CSIS_F_DMA_N_PACK12], dma_pack12);
	val = is_hw_set_field_value(val, &csi_vcdma_fields[CSIS_F_DMA_N_STORAGE_MODE], dma_stg_mode);

	if (dma_stg_mode == CSIS_REG_DMA_STG_LEGACY_MODE) {
		/* below setting is only valid for storage legacy mode */
		val = is_hw_set_field_value(val, &csi_vcdma_fields[CSIS_F_DMA_N_DATAFORMAT], dma_format);
		is_hw_set_reg(base_reg, &csi_vcdma_regs[CSIS_R_DMA0_FMT], val);

		val = is_hw_get_reg(base_reg, &csi_vcdma_regs[CSIS_R_DMA0_RESOL]);
		val = is_hw_set_field_value(val, &csi_vcdma_fields[CSIS_F_DMA_N_HRESOL], cfg->width + dummy_pixel);
		is_hw_set_reg(base_reg, &csi_vcdma_regs[CSIS_R_DMA0_RESOL], val);
	} else if (dma_stg_mode == CSIS_REG_DMA_STG_PACKET_MODE) {
		is_hw_set_reg(base_reg, &csi_vcdma_regs[CSIS_R_DMA0_FMT], val);
	}

	is_hw_set_reg(base_reg, &csi_vcdma_regs[CSIS_R_DMA0_FCNTSEQ], 0);

p_err:
	return ret;
}

int csi_hw_s_irq_msk(u32 __iomem *base_reg, bool on, bool f_id_dec)
{
	u32 otf_msk;
	u32 otf_msk1;

	/* default setting */
	if (on) {
		if (f_id_dec) {
			/*
			 * If FRO mode is enable, start & end of CSIS link is not used.
			 * Instead of CSIS link interrupt, CSIS WDMA interrupt is used.
			 * So, only error interrupt is enable.
			 */
			otf_msk = CSIS_ERR_MASK0;
			otf_msk1 = CSIS_ERR_MASK1;
		} else {
			otf_msk = CSIS_IRQ_MASK0;
			otf_msk1 = CSIS_IRQ_MASK1;
		}
	} else {
		otf_msk = 0;
		otf_msk1 = 0;
	}

	is_hw_set_reg(base_reg, &csi_regs[CSIS_R_CSIS_INT_MSK0], otf_msk);
	is_hw_set_reg(base_reg, &csi_regs[CSIS_R_CSIS_INT_MSK1], otf_msk1);

	return 0;
}

static void csi_hw_g_err_types_from_err(u32 __iomem *base_reg, u32 err_src0, ulong *err_id)
{
	int i = 0;
	u32 sot_hs_err = 0;
	u32 lost_fs_err = 0;
	u32 lost_fe_err = 0;
	u32 ovf_err = 0;
	u32 wrong_cfg_err = 0;
	u32 err_ecc_err = 0;
	u32 crc_err = 0;
	u32 err_id_err = 0;
	u32 vresol_err = 0;
	u32 hresol_err = 0;
	u32 mal_crc_err = 0;
	u32 inval_code_hs = 0;
	u32 sot_sync_hs = 0;
	u32 crc_err_cphy = 0;

	sot_hs_err    = is_hw_get_field_value(err_src0, &csi_fields[CSIS_F_ERR_SOT_HS]);
	ovf_err       = is_hw_get_field_value(err_src0, &csi_fields[CSIS_F_ERR_OVER]);
	wrong_cfg_err = is_hw_get_field_value(err_src0, &csi_fields[CSIS_F_ERR_WRONG_CFG]);
	err_ecc_err   = is_hw_get_field_value(err_src0, &csi_fields[CSIS_F_ERR_ECC]);
	crc_err       = is_hw_get_field_value(err_src0, &csi_fields[CSIS_F_ERR_CRC_DPHY]);
	err_id_err    = is_hw_get_field_value(err_src0, &csi_fields[CSIS_F_ERR_ID]);

	lost_fs_err   = is_hw_get_field(base_reg, &csi_regs[CSIS_R_ERR_LOST_FS],
					&csi_fields[CSIS_F_ERR_LOST_FS_CH]);
	lost_fe_err   = is_hw_get_field(base_reg, &csi_regs[CSIS_R_ERR_LOST_FE],
					&csi_fields[CSIS_F_ERR_LOST_FE_CH]);
	vresol_err    = is_hw_get_field(base_reg, &csi_regs[CSIS_R_ERR_VRESOL],
					&csi_fields[CSIS_F_VRESOL_MISMATCH_CH]);
	hresol_err    = is_hw_get_field(base_reg, &csi_regs[CSIS_R_ERR_HRESOL],
					&csi_fields[CSIS_F_HRESOL_MISMATCH_CH]);

	inval_code_hs = is_hw_get_field_value(err_src0, &csi_fields[CSIS_F_RXINVALIDCODEHS]);
	sot_sync_hs   = is_hw_get_field_value(err_src0, &csi_fields[CSIS_F_ERRSOTSYNCHS]);
	mal_crc_err   = is_hw_get_field_value(err_src0, &csi_fields[CSIS_F_MAL_CRC]);
	crc_err_cphy  = is_hw_get_field_value(err_src0, &csi_fields[CSIS_F_ERR_CRC_CPHY]);

	for (i = 0; i < CSI_VIRTUAL_CH_MAX; i++) {
		err_id[i] |= ((sot_hs_err    & (1 << i)) ? (1 << CSIS_ERR_SOT_VC) : 0);
		err_id[i] |= ((lost_fs_err   & (1 << i)) ? (1 << CSIS_ERR_LOST_FS_VC) : 0);
		err_id[i] |= ((lost_fe_err   & (1 << i)) ? (1 << CSIS_ERR_LOST_FE_VC) : 0);
		err_id[i] |= ((ovf_err       & (1 << i)) ? (1 << CSIS_ERR_OVERFLOW_VC) : 0);
		err_id[i] |= ((wrong_cfg_err & (1 << i)) ? (1 << CSIS_ERR_WRONG_CFG) : 0);
		err_id[i] |= ((err_ecc_err   & (1 << i)) ? (1 << CSIS_ERR_ECC) : 0);
		err_id[i] |= ((crc_err       & (1 << i)) ? (1 << CSIS_ERR_CRC) : 0);
		err_id[i] |= ((err_id_err    & (1 << i)) ? (1 << CSIS_ERR_ID) : 0);

		err_id[i] |= ((vresol_err    & (1 << i)) ? (1 << CSIS_ERR_VRESOL_MISMATCH) : 0);
		err_id[i] |= ((hresol_err    & (1 << i)) ? (1 << CSIS_ERR_HRESOL_MISMATCH) : 0);
		err_id[i] |= ((inval_code_hs & (1 << i)) ? (1 << CSIS_ERR_INVALID_CODE_HS) : 0);
		err_id[i] |= ((sot_sync_hs & (1 << i)) ? (1 << CSIS_ERR_SOT_SYNC_HS) : 0);
		err_id[i] |= ((mal_crc_err & (1 << i)) ? (1 << CSIS_ERR_MAL_CRC) : 0);
		err_id[i] |= ((crc_err_cphy & (1 << i)) ? (1 << CSIS_ERR_CRC_CPHY) : 0);
	}
}

static bool csi_hw_g_value_of_err(u32 __iomem *base_reg, u32 otf_src0, u32 otf_src1, ulong *err_id)
{
	u32 err_src0 = (otf_src0 & CSIS_ERR_MASK0);
	u32 err_src1 = (otf_src1 & CSIS_ERR_MASK1);
	bool err_flag = false;

	if (err_src0 || err_src1) {
		csi_hw_g_err_types_from_err(base_reg, err_src0, err_id);
		err_flag = true;
	}

	return err_flag;
}

int csi_hw_g_irq_src(u32 __iomem *base_reg, struct csis_irq_src *src, bool clear)
{
	u32 otf_src0;
	u32 otf_src1;
	u32 fs_val;
	u32 fe_val;
	u32 line_val;

	otf_src0 = is_hw_get_reg(base_reg, &csi_regs[CSIS_R_CSIS_INT_SRC0]);
	otf_src1 = is_hw_get_reg(base_reg, &csi_regs[CSIS_R_CSIS_INT_SRC1]);

	fs_val   = is_hw_get_reg(base_reg, &csi_regs[CSIS_R_FS_INT_SRC]);
	fe_val   = is_hw_get_reg(base_reg, &csi_regs[CSIS_R_FE_INT_SRC]);
	line_val = is_hw_get_reg(base_reg, &csi_regs[CSIS_R_LINE_END]);

	if (clear) {
		is_hw_set_reg(base_reg, &csi_regs[CSIS_R_CSIS_INT_SRC0], otf_src0);
		is_hw_set_reg(base_reg, &csi_regs[CSIS_R_CSIS_INT_SRC1], otf_src1);
		is_hw_set_reg(base_reg, &csi_regs[CSIS_R_FS_INT_SRC], fs_val);
		is_hw_set_reg(base_reg, &csi_regs[CSIS_R_FE_INT_SRC], fe_val);
		is_hw_set_reg(base_reg, &csi_regs[CSIS_R_LINE_END], line_val);
	}

	src->otf_start = is_hw_get_field_value(fs_val, &csi_fields[CSIS_F_FRAMESTART_CH]);
	src->otf_end = is_hw_get_field_value(fe_val, &csi_fields[CSIS_F_FRAMEEND_CH]);
	src->line_end = is_hw_get_field_value(line_val, &csi_fields[CSIS_F_LINE_END_CH]);
	src->err_flag = csi_hw_g_value_of_err(base_reg, otf_src0, otf_src1, src->err_id);

	return 0;
}

void csi_hw_s_frameptr(u32 __iomem *base_reg, u32 vc, u32 number, bool clear)
{
	u32 frame_ptr = 0;
	u32 val = is_hw_get_reg(base_reg, &csi_vcdma_regs[CSIS_R_DMA0_CTRL]);

	frame_ptr = is_hw_get_field_value(val, &csi_vcdma_fields[CSIS_F_DMA_N_UPDT_FRAMEPTR]);
	if (clear)
		frame_ptr |= (1 << number);
	else
		frame_ptr &= ~(1 << number);
	val = is_hw_set_field_value(val, &csi_vcdma_fields[CSIS_F_DMA_N_UPDT_FRAMEPTR], frame_ptr);
	is_hw_set_reg(base_reg, &csi_vcdma_regs[CSIS_R_DMA0_CTRL], val);
}

u32 csi_hw_g_frameptr(u32 __iomem *base_reg, u32 vc)
{
	u32 frame_ptr = 0;
	u32 val = is_hw_get_reg(base_reg, &csi_vcdma_regs[CSIS_R_DMA0_ACT_CTRL]);

	frame_ptr = is_hw_get_field_value(val, &csi_vcdma_fields[CSIS_F_ACTIVE_DMA_N_FRAMEPTR]);

	return frame_ptr;
}

void csi_hw_s_dma_addr(u32 __iomem *base_reg, u32 vc, u32 number, u32 addr)
{
	u32 val = is_hw_get_reg(base_reg, &csi_vcdma_regs[CSIS_R_DMA0_FCNTSEQ]);

	csi_hw_s_frameptr(base_reg, vc, number, false);

	is_hw_set_reg(base_reg, &csi_vcdma_regs[CSIS_R_DMA0_ADDR1 + number], addr);

	val |= 1 << number;
	is_hw_set_reg(base_reg, &csi_vcdma_regs[CSIS_R_DMA0_FCNTSEQ], val);
}

void csi_hw_s_multibuf_dma_addr(u32 __iomem *base_reg, u32 vc, u32 number, u32 addr)
{
	u32 val = is_hw_get_reg(base_reg, &csi_vcdma_regs[CSIS_R_DMA0_FCNTSEQ]);

	is_hw_set_reg(base_reg, &csi_vcdma_regs[CSIS_R_DMA0_ADDR1 + number], addr);

	val |= 1 << number;
	is_hw_set_reg(base_reg, &csi_vcdma_regs[CSIS_R_DMA0_FCNTSEQ], val);
}

void csi_hw_s_output_dma(u32 __iomem *base_reg, u32 vc, bool enable)
{
	u32 val = is_hw_get_reg(base_reg, &csi_vcdma_regs[CSIS_R_DMA0_CTRL]);

	val = is_hw_set_field_value(val, &csi_vcdma_fields[CSIS_F_DMA_N_ENABLE], enable);
	val = is_hw_set_field_value(val, &csi_vcdma_fields[CSIS_F_DMA_N_UPDT_PTR_EN], enable);
	is_hw_set_reg(base_reg, &csi_vcdma_regs[CSIS_R_DMA0_CTRL], val);
}

bool csi_hw_g_output_dma_enable(u32 __iomem *base_reg, u32 vc)
{
	/* if DMA_DISABLE field value is 1, this means dma output is disabled */
	if (is_hw_get_field(base_reg, &csi_vcdma_regs[CSIS_R_DMA0_CTRL],
			&csi_vcdma_fields[CSIS_F_DMA_N_ENABLE]))
		return true;
	else
		return false;
}

bool csi_hw_g_output_cur_dma_enable(u32 __iomem *base_reg, u32 vc)
{
	u32 val = is_hw_get_reg(base_reg, &csi_vcdma_regs[CSIS_R_DMA0_ACT_CTRL]);
	/* if DMA_ENABLE field value is 1, this means dma output is enabled */
	bool dma_enable = is_hw_get_field_value(val, &csi_vcdma_fields[CSIS_F_ACTIVE_DMA_N_ENABLE]);

	return dma_enable;
}

void csi_hw_set_start_addr(u32 __iomem *base_reg, u32 number, u32 addr)
{
	u32 __iomem *target_reg;

	if (number == 0) {
		target_reg = base_reg + TO_WORD_OFFSET(0x30);
	} else {
		number--;
		target_reg = base_reg + TO_WORD_OFFSET(0x200 + (0x4*number));
	}

	writel(addr, target_reg);
}

int csi_hw_dma_common_reset(u32 __iomem *base_reg, bool on)
{
	return 0;
}

int csi_hw_s_dma_common_dynamic(u32 __iomem *base_reg, size_t size, unsigned int dma_ch)
{
	u32 val;
	u32 sram0_split;
	u32 sram1_split;
	u32 max;

	if (!base_reg)
		return 0;

	/* Common DMA Arbitration Priority register */
	/* CSIS_DMA_F_DMA_ARB_PRI_0 : 1 = CSIS0 DMA has a high priority */
	/* CSIS_DMA_F_DMA_ARB_PRI_0 : 2 = CSIS1 DMA has a high priority */
	val = is_hw_get_reg(base_reg, &csi_dma_regs[CSIS_DMA_R_COMMON_DMA_ARB_PRI]);
	val = is_hw_set_field_value(val, &csi_dma_fields[CSIS_DMA_F_DMA_ARB_PRI_0], 0x2);
	is_hw_set_reg(base_reg, &csi_dma_regs[CSIS_DMA_R_COMMON_DMA_ARB_PRI], val);

	/* Common DMA Control register */
	/* CSIS_DMA_F_IP_PROCESSING : 1 = Q-channel clock enable  */
	/* CSIS_DMA_F_IP_PROCESSING : 0 = Q-channel clock disable */
	val = is_hw_get_reg(base_reg, &csi_dma_regs[CSIS_DMA_R_COMMON_DMA_CTRL]);
	val = is_hw_set_field_value(val, &csi_dma_fields[CSIS_DMA_F_IP_PROCESSING], 0x1);
	is_hw_set_reg(base_reg, &csi_dma_regs[CSIS_DMA_R_COMMON_DMA_CTRL], val);

	/* Common DMA SRAM split register */
	/* CSIS_DMA_F_DMA_SRAM1_SPLIT : internal SRAM1 is 10KB (640 * 16 bytes) */
	/* CSIS_DMA_F_DMA_SRAM0_SPLIT : internal SRAM0 is 10KB (640 * 16 bytes) */
	/* This register can be set between 0 to 640 */
	max = (u32)(size / 16);
#if 0
	sram0_split = max / 2;
	sram1_split = max / 2;
#else
	sram0_split = 0x1F0;
	sram1_split = 0x1F0;
#endif
	val = is_hw_get_reg(base_reg, &csi_dma_regs[CSIS_DMA_R_COMMON_DMA_SRAM_SPLIT]);
	val = is_hw_set_field_value(val, &csi_dma_fields[CSIS_DMA_F_DMA_SRAM1_SPLIT], sram1_split);
	val = is_hw_set_field_value(val, &csi_dma_fields[CSIS_DMA_F_DMA_SRAM0_SPLIT], sram0_split);
	is_hw_set_reg(base_reg, &csi_dma_regs[CSIS_DMA_R_COMMON_DMA_SRAM_SPLIT], val);

	/* Common DMA Martix register */
	/* CSIS_DMA_F_DMA_MATRIX : Under Table see */
	/*       CSIS0      CSIS1      CSIS2      CSIS3  */
	/* 0  : SRAM0_0    SRAM0_0    SRAM1_0    SRAM1_1 */
	/* 2  : SRAM0_0    SRAM1_0    SRAM0_1    SRAM1_1 */
	/* 5  : SRAM0_0    SRAM1_1    SRAM1_0    SRAM0_1 */
	/* 14 : SRAM1_0    SRAM0_1    SRAM0_0    SRAM1_1 */
	/* 16 : SRAM1_0    SRAM1_1    SRAM0_0    SRAM0_1 */
	/* 17 : SRAM1_0    SRAM1_1    SRAM0_1    SRAM0_0 */
	/* 22 : SRAM1_1    SRAM1_0    SRAM0_0    SRAM0_1 */
	/* 23 : SRAM1_1    SRAM1_0    SRAM0_1    SRAM0_0 */

	val = is_hw_get_reg(base_reg, &csi_dma_regs[CSIS_DMA_R_COMMON_DMA_MATRIX]);
	val = is_hw_set_field_value(val, &csi_dma_fields[CSIS_DMA_F_DMA_MATRIX], 0x0);
	is_hw_set_reg(base_reg, &csi_dma_regs[CSIS_DMA_R_COMMON_DMA_MATRIX], val);
	is_hw_set_reg(base_reg, &csi_dma_regs[CSIS_DMA_R_COMMON_DMA_MAX_MO], 0x20);
	is_hw_set_reg(base_reg, &csi_dma_regs[CSIS_DMA_R_COMMON_DMA_CLK_CTRL], 0x1);

	return 0;
}

int csi_hw_s_dma_common(u32 __iomem *base_reg)
{
	u32 val;

	if (!base_reg)
		return 0;

	/* Common DMA Arbitration Priority register */
	/* CSIS_DMA_F_DMA_ARB_PRI_0 : 1 = CSIS0 DMA has a high priority */
	/* CSIS_DMA_F_DMA_ARB_PRI_0 : 2 = CSIS1 DMA has a high priority */
	val = is_hw_get_reg(base_reg, &csi_dma_regs[CSIS_DMA_R_COMMON_DMA_ARB_PRI]);
	val = is_hw_set_field_value(val, &csi_dma_fields[CSIS_DMA_F_DMA_ARB_PRI_0], 0x2);
	is_hw_set_reg(base_reg, &csi_dma_regs[CSIS_DMA_R_COMMON_DMA_ARB_PRI], val);

	/* Common DMA Control register */
	/* CSIS_DMA_F_IP_PROCESSING : 1 = Q-channel clock enable  */
	/* CSIS_DMA_F_IP_PROCESSING : 0 = Q-channel clock disable */
	val = is_hw_get_reg(base_reg, &csi_dma_regs[CSIS_DMA_R_COMMON_DMA_CTRL]);
	val = is_hw_set_field_value(val, &csi_dma_fields[CSIS_DMA_F_IP_PROCESSING], 0x1);
	is_hw_set_reg(base_reg, &csi_dma_regs[CSIS_DMA_R_COMMON_DMA_CTRL], val);

	/* Common DMA SRAM split register */
	/* CSIS_DMA_F_DMA_SRAM1_SPLIT : internal SRAM1 is 10KB (640 * 16 bytes) */
	/* CSIS_DMA_F_DMA_SRAM0_SPLIT : internal SRAM0 is 10KB (640 * 16 bytes) */
	/* This register can be set between 0 to 640 */
	val = is_hw_get_reg(base_reg, &csi_dma_regs[CSIS_DMA_R_COMMON_DMA_SRAM_SPLIT]);
	val = is_hw_set_field_value(val, &csi_dma_fields[CSIS_DMA_F_DMA_SRAM1_SPLIT], 0x200);
	val = is_hw_set_field_value(val, &csi_dma_fields[CSIS_DMA_F_DMA_SRAM0_SPLIT], 0x200);
	is_hw_set_reg(base_reg, &csi_dma_regs[CSIS_DMA_R_COMMON_DMA_SRAM_SPLIT], val);

	/* Common DMA Martix register */
	/* CSIS_DMA_F_DMA_MATRIX : Under Table see */
	/*       CSIS0      CSIS1      CSIS2      CSIS3  */
	/* 0  : SRAM0_0    SRAM0_0    SRAM1_0    SRAM1_1 */
	/* 2  : SRAM0_0    SRAM1_0    SRAM0_1    SRAM1_1 */
	/* 5  : SRAM0_0    SRAM1_1    SRAM1_0    SRAM0_1 */
	/* 14 : SRAM1_0    SRAM0_1    SRAM0_0    SRAM1_1 */
	/* 16 : SRAM1_0    SRAM1_1    SRAM0_0    SRAM0_1 */
	/* 17 : SRAM1_0    SRAM1_1    SRAM0_1    SRAM0_0 */
	/* 22 : SRAM1_1    SRAM1_0    SRAM0_0    SRAM0_1 */
	/* 23 : SRAM1_1    SRAM1_0    SRAM0_1    SRAM0_0 */
	val = is_hw_get_reg(base_reg, &csi_dma_regs[CSIS_DMA_R_COMMON_DMA_MATRIX]);
	val = is_hw_set_field_value(val, &csi_dma_fields[CSIS_DMA_F_DMA_MATRIX], 0x2);
	is_hw_set_reg(base_reg, &csi_dma_regs[CSIS_DMA_R_COMMON_DMA_MATRIX], val);

	return 0;
}

int csi_hw_s_dma_common_pattern_enable(u32 __iomem *base_reg,
	u32 width, u32 height, u32 fps, u32 clk)
{
	u32 val;
	int clk_mhz;
	int vvalid;
	int vblank;
	int vblank_size;
	u32 hblank = 70;	/* This value should be guided according to 3AA HW constrain. */
	u32 v_to_hblank = 0x80;	/* This value should be guided according to 3AA HW constrain. */
	u32 h_to_vblank = 0x40;	/* This value should be guided according to 3AA HW constrain. */

	if (!width || (width % 8 != 0)) {
		err("A width(%d) is not aligned to 8", width);
		return -EINVAL;
	}

	clk_mhz = clk / 1000000;

	/*
	 * V-valid Calculation:
	 * The unit of v-valid is usec.
	 * 2 means 2ppc.
	 */
	vvalid = (width * height) / (clk_mhz * 2);

	/*
	 * V-blank Calculation:
	 * The unit of v-blank is usec.
	 * v-blank operates with 1ppc.
	 */
	vblank = ((1000000 / fps) - vvalid);
	if (vblank < 0) {
		vblank = 1000; /* 1000 us */
		info("FPS is too high. So, FPS is adjusted forcely. vvalid(%d us), vblank(%d us)\n",
			vvalid, vblank);
	}
	vblank_size = vblank * clk_mhz;

	val = is_hw_get_reg(base_reg, &csi_dma_regs[CSIS_DMA_R_TEST_PATTERN_CTRL]);
	val = is_hw_set_field_value(val, &csi_dma_fields[CSIS_DMA_F_VTOHBLANK], v_to_hblank);
	val = is_hw_set_field_value(val, &csi_dma_fields[CSIS_DMA_F_HBLANK], hblank);
	val = is_hw_set_field_value(val, &csi_dma_fields[CSIS_DMA_F_HTOVBLANK], h_to_vblank);
	is_hw_set_reg(base_reg, &csi_dma_regs[CSIS_DMA_R_TEST_PATTERN_CTRL], val);

	val = is_hw_get_reg(base_reg, &csi_dma_regs[CSIS_DMA_R_TEST_PATTERN_SIZE]);
	val = is_hw_set_field_value(val, &csi_dma_fields[CSIS_DMA_F_TP_VSIZE], height);
	val = is_hw_set_field_value(val, &csi_dma_fields[CSIS_DMA_F_TP_HSIZE], width);
	is_hw_set_reg(base_reg, &csi_dma_regs[CSIS_DMA_R_TEST_PATTERN_SIZE], val);

	val = is_hw_get_reg(base_reg, &csi_dma_regs[CSIS_DMA_R_TEST_PATTERN_ON]);
	val = is_hw_set_field_value(val, &csi_dma_fields[CSIS_DMA_F_PPCMODE], CSIS_PIXEL_MODE_SING);
	val = is_hw_set_field_value(val, &csi_dma_fields[CSIS_DMA_F_VBLANK], vblank_size);
	is_hw_set_reg(base_reg, &csi_dma_regs[CSIS_DMA_R_TEST_PATTERN_ON], val);

	is_hw_set_field(base_reg, &csi_dma_regs[CSIS_DMA_R_TEST_PATTERN_ON],
			&csi_dma_fields[CSIS_DMA_F_TESTPATTERN], 1);

	info("Enable Pattern Generator: size(%d x %d), fps(%d), clk(%d Hz), vvalid(%d us), vblank(%d us)\n",
		width, height, fps, clk, vvalid, vblank);

	return 0;
}

void csi_hw_s_dma_common_pattern_disable(u32 __iomem *base_reg)
{
	is_hw_set_field(base_reg, &csi_dma_regs[CSIS_DMA_R_TEST_PATTERN_ON],
		&csi_dma_fields[CSIS_DMA_F_TESTPATTERN], 0);
}

int csi_hw_s_dma_common_votf_enable(u32 __iomem *base_reg, u32 width, u32 dma_ch, u32 vc)
{
	/* Not support */
	return 0;
}

int csi_hw_s_dma_common_frame_id_decoder(u32 __iomem *base_reg, u32 dma_ch,
		u32 enable)
{
	return 0;
}

int csi_hw_g_dma_common_frame_id(u32 __iomem *base_reg, u32 batch_num, u32 *frame_id)
{
	return 0;
}

int csi_hw_clear_fro_count(u32 __iomem *dma_top_reg, u32 __iomem *vc_reg)
{
	return 0;
}

int csi_hw_s_fro_count(u32 __iomem *vc_cmn_reg, u32 batch_num, u32 vc)
{
	return 0;
}

void csi_hw_dma_reset(u32 __iomem *base_reg)
{
	/*
	 * Any other registers are not controlled by 2 instance as well as DMA off,
	 * because DMA cannot be shared between 1 more instance.
	 */
	is_hw_set_reg(base_reg, &csi_vcdma_regs[CSIS_R_DMA0_CTRL], 0);
	is_hw_set_reg(base_reg, &csi_vcdma_regs[CSIS_R_DMA0_FCNTSEQ], 0);
}

int csi_hw_enable(u32 __iomem *base_reg, u32 use_cphy)
{
	/* update shadow */
	is_hw_set_field(base_reg, &csi_regs[CSIS_R_CSIS_UPD_SDW],
			&csi_fields[CSIS_F_UPDATE_SHADOW], 0xF);

	/* PHY selection */
	is_hw_set_field(base_reg, &csi_regs[CSIS_R_CSIS_CMN_CTRL],
			&csi_fields[CSIS_F_PHY_SEL], 0);

	/* PHY on */
	is_hw_set_field(base_reg, &csi_regs[CSIS_R_PHY_CMN_CTRL],
			&csi_fields[CSIS_F_ENABLE_CLK], 1);

	/* Q-channel disable */
#ifdef ENABLE_HWACG_CONTROL
	is_hw_set_field(base_reg, &csi_regs[CSIS_R_CSIS_CMN_CTRL],
			&csi_fields[CSIS_F_QCHANNEL_EN], 0);
#endif

	/* csi enable */
	is_hw_set_field(base_reg, &csi_regs[CSIS_R_CSIS_CMN_CTRL],
			&csi_fields[CSIS_F_CSI_EN], 1);

	return 0;
}

int csi_hw_disable(u32 __iomem *base_reg)
{
	/* PHY off */
	is_hw_set_field(base_reg, &csi_regs[CSIS_R_PHY_CMN_CTRL],
			&csi_fields[CSIS_F_ENABLE_CLK], 0);
	is_hw_set_field(base_reg, &csi_regs[CSIS_R_PHY_CMN_CTRL],
			&csi_fields[CSIS_F_ENABLE_DAT], 0);

	/* csi disable */
	is_hw_set_field(base_reg, &csi_regs[CSIS_R_CSIS_CMN_CTRL],
			&csi_fields[CSIS_F_CSI_EN], 0);

	/* Q-channel enable */
#ifdef ENABLE_HWACG_CONTROL
	is_hw_set_field(base_reg, &csi_regs[CSIS_R_CSIS_CMN_CTRL],
			&csi_fields[CSIS_F_QCHANNEL_EN], 1);
#endif

	return 0;
}

int csi_hw_dump(u32 __iomem *base_reg)
{
	u32 csis_ver = csi_hw_get_version(base_reg);

	info("CSI Core SFR DUMP (v%d.%d[.%d[.%d]])\n",
			(csis_ver >> 24) & 0xFF,
			(csis_ver >> 16) & 0xFF,
			(csis_ver >> 8) & 0xFF,
			(csis_ver >> 0) & 0xFF);

	is_hw_dump_regs(base_reg, csi_regs, CSIS_REG_CNT);

	return 0;
}

int csi_hw_vcdma_dump(u32 __iomem *base_reg)
{
	u32 csis_ver = csi_hw_get_version(base_reg);

	info("CSI VC/DMA SFR DUMP (v%d.%d[.%d[.%d]])\n",
			(csis_ver >> 24) & 0xFF,
			(csis_ver >> 16) & 0xFF,
			(csis_ver >> 8) & 0xFF,
			(csis_ver >> 0) & 0xFF);

	is_hw_dump_regs(base_reg, csi_vcdma_regs, CSIS_VCDMA_REG_CNT);

	return 0;
}

int csi_hw_vcdma_cmn_dump(u32 __iomem *base_reg)
{
	u32 csis_ver = csi_hw_get_version(base_reg);

	info("CSI VC/DMA Common SFR DUMP (v%d.%d[.%d[.%d]])\n",
			(csis_ver >> 24) & 0xFF,
			(csis_ver >> 16) & 0xFF,
			(csis_ver >> 8) & 0xFF,
			(csis_ver >> 0) & 0xFF);

	is_hw_dump_regs(base_reg, csi_vcdma_cmn_regs, CSIS_VCDMA_CMN_REG_CNT);

	return 0;
}

int csi_hw_phy_dump(u32 __iomem *base_reg, u32 instance)
{
	u32 csis_ver = csi_hw_get_version(base_reg);

	info("CSI PHY SFR DUMP (v%d.%d[.%d[.%d]])\n",
			(csis_ver >> 24) & 0xFF,
			(csis_ver >> 16) & 0xFF,
			(csis_ver >> 8) & 0xFF,
			(csis_ver >> 0) & 0xFF);

	is_hw_dump_regs(base_reg, csi_phy_regs[instance], PHY_REG_CNT);

	return 0;
}

int csi_hw_common_dma_dump(u32 __iomem *base_reg)
{
	info("CSI COMMON DMA SFR DUMP (v1.2)\n");
	is_hw_dump_regs(base_reg, csi_dma_regs, CSIS_DMA_REG_CNT);

	return 0;
}

#if defined(ENABLE_CLOG_RESERVED_MEM)
int csi_hw_cdump(u32 __iomem *base_reg)
{
	u32 csis_ver = csi_hw_get_version(base_reg);

	cinfo("CSI Core SFR DUMP (v%d.%d[.%d[.%d]])\n",
			(csis_ver >> 24) & 0xFF,
			(csis_ver >> 16) & 0xFF,
			(csis_ver >> 8) & 0xFF,
			(csis_ver >> 0) & 0xFF);

	is_hw_cdump_regs(base_reg, csi_regs, CSIS_REG_CNT);

	return 0;
}

int csi_hw_vcdma_cdump(u32 __iomem *base_reg)
{
	u32 csis_ver = csi_hw_get_version(base_reg);

	cinfo("CSI VC/DMA SFR DUMP (v%d.%d[.%d[.%d]])\n",
			(csis_ver >> 24) & 0xFF,
			(csis_ver >> 16) & 0xFF,
			(csis_ver >> 8) & 0xFF,
			(csis_ver >> 0) & 0xFF);

	is_hw_cdump_regs(base_reg, csi_vcdma_regs, CSIS_VCDMA_REG_CNT);

	return 0;
}

int csi_hw_vcdma_cmn_cdump(u32 __iomem *base_reg)
{
	u32 csis_ver = csi_hw_get_version(base_reg);

	cinfo("CSI VC/DMA Common SFR DUMP (v%d.%d[.%d[.%d]])\n",
			(csis_ver >> 24) & 0xFF,
			(csis_ver >> 16) & 0xFF,
			(csis_ver >> 8) & 0xFF,
			(csis_ver >> 0) & 0xFF);

	is_hw_cdump_regs(base_reg, csi_vcdma_cmn_regs, CSIS_VCDMA_CMN_REG_CNT);

	return 0;
}

int csi_hw_phy_cdump(u32 __iomem *base_reg, u32 instance)
{
	u32 csis_ver = csi_hw_get_version(base_reg);

	cinfo("CSI PHY SFR DUMP (v%d.%d[.%d[.%d]])\n",
			(csis_ver >> 24) & 0xFF,
			(csis_ver >> 16) & 0xFF,
			(csis_ver >> 8) & 0xFF,
			(csis_ver >> 0) & 0xFF);

	is_hw_cdump_regs(base_reg, csi_phy_regs[instance], PHY_REG_CNT);

	return 0;
}

int csi_hw_common_dma_cdump(u32 __iomem *base_reg)
{
	cinfo("CSI COMMON DMA SFR DUMP (v5.3)\n");
	is_hw_cdump_regs(base_reg, csi_dma_regs, CSIS_DMA_REG_CNT);

	return 0;
}
#endif

int csi_hw_s_dma_irq_msk(u32 __iomem *base_reg, bool on)
{
	u32 dma_msk;

	/* default setting */
	if (on) {
		/* base interrupt setting */
		dma_msk = CSIS_DMA_IRQ_MASK;
	} else {
		dma_msk = 0;
	}

	is_hw_set_reg(base_reg, &csi_vcdma_cmn_regs[CSIS_R_DMA_INT_MASK], dma_msk);

	return 0;
}

int csi_hw_g_dma_irq_src_vc(u32 __iomem *base_reg, struct csis_irq_src *src, u32 vc_phys, bool clear)
{
	u32 dma_src;
	u32 mask = 0;

	dma_src = is_hw_get_reg(base_reg, &csi_vcdma_cmn_regs[CSIS_R_DMA_INT_SRC]);

	mask |= 1 << (csi_vcdma_cmn_fields[CSIS_F_DMA_ABORT_DONE].bit_start);
	mask |= 1 << (csi_vcdma_cmn_fields[CSIS_F_DMA_ERROR].bit_start);
	mask |= 1 << (csi_vcdma_cmn_fields[CSIS_F_DMA_OTF_OVERLAP].bit_start + vc_phys);
	mask |= 1 << (csi_vcdma_cmn_fields[CSIS_F_DMA_FRM_END].bit_start + vc_phys);
	mask |= 1 << (csi_vcdma_cmn_fields[CSIS_F_DMA_FRM_START].bit_start + vc_phys);
	mask |= 1 << (csi_vcdma_cmn_fields[CSIS_F_DMA_LINE_END].bit_start + vc_phys);
	dma_src = dma_src & mask;

	if (clear)
		is_hw_set_reg(base_reg, &csi_vcdma_cmn_regs[CSIS_R_DMA_INT_SRC], dma_src);

	src->dma_start = is_hw_get_field_value(dma_src, &csi_vcdma_cmn_fields[CSIS_F_DMA_FRM_START]);
	src->dma_end = is_hw_get_field_value(dma_src, &csi_vcdma_cmn_fields[CSIS_F_DMA_FRM_END]);
	src->dma_abort = is_hw_get_field_value(dma_src, &csi_vcdma_cmn_fields[CSIS_F_DMA_ABORT_DONE]);

	if (dma_src & (1 << (csi_vcdma_cmn_fields[CSIS_F_DMA_OTF_OVERLAP].bit_start + vc_phys)))
		src->err_id[vc_phys] |= 1 << CSIS_ERR_DMA_OTF_OVERLAP_VC;

	if (dma_src & (1 << csi_vcdma_cmn_fields[CSIS_F_DMA_ERROR].bit_start))
		src->err_id[vc_phys] |= 1 << CSIS_ERR_DMA_DMAFIFO_FULL;

	if (dma_src & (1 << csi_vcdma_cmn_fields[CSIS_F_DMA_ABORT_DONE].bit_start))
		src->err_id[vc_phys] |= 1 << CSIS_ERR_DMA_ABORT_DONE;

	return 0;
}

int csi_hw_s_config_dma_cmn(u32 __iomem *base_reg, u32 vc, u32 actual_vc, u32 hwformat, bool potf)

{
	int ret = 0;
	u32 val;
	u32 otf_format;
	u32 dma_input_path;

	if (vc > CSI_VIRTUAL_CH_3) {
		err("invalid vc(%d)", vc);
		ret = -EINVAL;
		goto p_err;
	}

	if (vc == CSI_VIRTUAL_CH_0) {
		switch (hwformat) {
		case HW_FORMAT_RAW10:
		case HW_FORMAT_RAW6_DA:
		case HW_FORMAT_RAW7_DS:
		case HW_FORMAT_RAW8_DS:
			otf_format = 0;
			dma_input_path = CSIS_REG_DMA_INPUT_OTF;
			break;
		case HW_FORMAT_RAW12:
			otf_format = 1;
			dma_input_path = CSIS_REG_DMA_INPUT_OTF;
			break;
		case HW_FORMAT_RAW14:
		case HW_FORMAT_RAW10_DA:
			otf_format = 2;
			dma_input_path = CSIS_REG_DMA_INPUT_OTF;
			break;
		case HW_FORMAT_RAW8:
		case HW_FORMAT_USER:
		case HW_FORMAT_USER1:
		case HW_FORMAT_USER2:
			otf_format = 3;
			dma_input_path = CSIS_REG_DMA_INPUT_PRL;
			break;
		default:
			err("invalid data format (%02X)", hwformat);
			ret = -EINVAL;
			goto p_err;
		}

		val = is_hw_get_reg(base_reg, &csi_vcdma_cmn_regs[CSIS_R_OTF_FORMAT]);
		val = is_hw_set_field_value(val, &csi_vcdma_cmn_fields[CSIS_F_OTF_FORMAT], otf_format);
		is_hw_set_reg(base_reg, &csi_vcdma_cmn_regs[CSIS_R_OTF_FORMAT], val);

		val = is_hw_get_reg(base_reg, &csi_vcdma_cmn_regs[CSIS_R_DMA_DATA_CTRL]);
		val = is_hw_set_field_value(val, &csi_vcdma_cmn_fields[CSIS_F_DMA_INPUT_PATH_CH0],
			dma_input_path);
		is_hw_set_reg(base_reg, &csi_vcdma_cmn_regs[CSIS_R_DMA_DATA_CTRL], val);
	}

p_err:
	return ret;
}

int csi_hw_s_phy_config(u32 __iomem *base_reg,
	u32 lanes, u32 mipi_speed, u32 settle, u32 instance)
{
	return 0;
}

#define MAX_PHY_CFG 10
int csi_hw_s_phy_set(struct phy *phy, u32 lanes, u32 mipi_speed,
		u32 settle, u32 instance, u32 use_cphy)
{
	int ret = 0;
	unsigned int phy_cfg[MAX_PHY_CFG];

	/*
	 * [0]: the version of PHY (major << 16 | minor)
	 * [1]: the type of PHY (mode << 16 | type)
	 * [2]: the number of lanes (zero-based)
	 * [3]: the data rate
	 * [4]: the settle value for the data rate
	 */
	phy_cfg[0] = 0x0503 << 16;
	phy_cfg[1] = 0x000D << 16;

	phy_cfg[0] |= 0x0008;
	switch (instance) {
	case CSI_ID_A:
	case CSI_ID_B:
	case CSI_ID_C:
		phy_cfg[1] |= 0xD;
		break;
	default:
		err("csi instance is invalid(%d)", instance);
		break;
	}

	phy_cfg[2] = lanes;
	phy_cfg[3] = mipi_speed;
	phy_cfg[4] = settle;

	ret = phy_set(phy, 0, (void *)phy_cfg);
	if (ret) {
		err("failed to set PHY");
		return ret;
	}

	return ret;
}

