/*
 * Copyright (c) 2016 Samsung Electronics Co., Ltd.
 *		http://www.samsung.com
 *
 * Samsung EXYNOS8 SoC series DPP driver
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
*/

#include <linux/module.h>
#include <linux/clk-provider.h>
#include <linux/clk.h>
#include <linux/of.h>
#include <linux/iommu.h>
#include <linux/console.h>

#include "videodev2_exynos_media.h"
#include "dpp.h"
#include "decon.h"
#include "format.h"
#include <dt-bindings/soc/samsung/exynos2100-devfreq.h>
#include <soc/samsung/exynos-devfreq.h>

#if defined(SYSFS_UNITTEST_INTERFACE)
#include "sysfs_error.h"
#endif

#if IS_ENABLED(CONFIG_MCD_PANEL)
#include "mcd_dpp.h"
#endif


int dpp_log_level = 6;
module_param(dpp_log_level, int, 0644);

struct dpp_device *dpp_drvdata[MAX_DPP_CNT];

static u32 default_fmt[DEFAULT_FMT_CNT] = {
	DECON_PIXEL_FORMAT_ARGB_8888, DECON_PIXEL_FORMAT_ABGR_8888,
	DECON_PIXEL_FORMAT_RGBA_8888, DECON_PIXEL_FORMAT_BGRA_8888,
	DECON_PIXEL_FORMAT_XRGB_8888, DECON_PIXEL_FORMAT_XBGR_8888,
	DECON_PIXEL_FORMAT_RGBX_8888, DECON_PIXEL_FORMAT_BGRX_8888,
};

void dpp_dump(struct dpp_device *dpp)
{
	int acquired = console_trylock();

	__dpp_dump(dpp->id, dpp->res.regs, dpp->res.dma_regs, dpp->attr);

	if (acquired)
		console_unlock();
}

static int dpp_wb_wait_for_framedone(struct dpp_device *dpp)
{
	int ret;
	int done_cnt;

	if (!test_bit(DPP_ATTR_ODMA, &dpp->attr)) {
		dpp_err("waiting for dpp's framedone is only for writeback\n");
		return -EINVAL;
	}

	if (dpp->state == DPP_STATE_OFF) {
		dpp_err("dpp%d power is off state(%d)\n", dpp->id, dpp->state);
		return -EPERM;
	}

	done_cnt = dpp->d.done_count;
	/* TODO: dma framedone should be wait */
	ret = wait_event_interruptible_timeout(dpp->framedone_wq,
			(done_cnt != dpp->d.done_count), msecs_to_jiffies(17));
	if (ret == 0) {
		dpp_err("timeout of dpp%d framedone\n", dpp->id);
		return -ETIMEDOUT;
	}

	return 0;
}

/*
 * [ SBWC info ]
 * buffer and base_addr relationship in SBWC (cf. 8+2)
 * <buffer fd> fd[0]: Y-payload / fd[1]: C-payload
 * <DPU base addr> [0]-Y8:Y_HD / [1]-C8:Y_PL / [2]-Y2:C_HD / [3]-C2:C_PL
 *
 * re-arrange & calculate base addr
 *  fd[1] -> addr[3] C-payload
 *  fd[0] -> addr[1] Y-payload
 *           addr[0] Y-header : [1] + Y_PL_SIZE
 *           addr[2] C-header : [3] + C_PL_SIZE
 */
static void dpp_get_base_addr_params(struct dpp_params_info *p)
{
	switch (p->format) {

	case DECON_PIXEL_FORMAT_NV12N:
		p->addr[1] = NV12N_CBCR_BASE(p->addr[0], p->src.f_w, p->src.f_h);
		break;

	case DECON_PIXEL_FORMAT_NV12_P010:
		p->addr[1] = P010_CBCR_BASE(p->addr[0], p->src.f_w, p->src.f_h);
		break;

	case DECON_PIXEL_FORMAT_NV12M_S10B:
	case DECON_PIXEL_FORMAT_NV21M_S10B:
		p->addr[2] = p->addr[0] + NV12M_Y_SIZE(p->src.f_w, p->src.f_h);
		p->addr[3] = p->addr[1] + NV12M_CBCR_SIZE(p->src.f_w, p->src.f_h);
		p->yhd_y2_strd = S10B_2B_STRIDE(p->src.f_w);
		p->ypl_c2_strd = S10B_2B_STRIDE(p->src.f_w);
		break;

	case DECON_PIXEL_FORMAT_NV12N_10B:
		p->addr[1] = NV12N_10B_CBCR_BASE(p->addr[0], p->src.f_w, p->src.f_h);
		p->addr[2] = p->addr[0] + NV12N_10B_Y_8B_SIZE(p->src.f_w, p->src.f_h);
		p->addr[3] = p->addr[1] + NV12N_10B_CBCR_8B_SIZE(p->src.f_w, p->src.f_h);
		p->yhd_y2_strd = S10B_2B_STRIDE(p->src.f_w);
		p->ypl_c2_strd = S10B_2B_STRIDE(p->src.f_w);
		break;

	case DECON_PIXEL_FORMAT_NV16M_S10B:
	case DECON_PIXEL_FORMAT_NV61M_S10B:
		p->addr[2] = p->addr[0] + NV16M_Y_SIZE(p->src.f_w, p->src.f_h);
		p->addr[3] = p->addr[1] + NV16M_CBCR_SIZE(p->src.f_w, p->src.f_h);
		p->yhd_y2_strd = S10B_2B_STRIDE(p->src.f_w);
		p->ypl_c2_strd = S10B_2B_STRIDE(p->src.f_w);
		break;

	/* for lossless SBWC */
	case DECON_PIXEL_FORMAT_NV12N_SBWC_8B: /* single fd : [0]-Y_PL */
		/* calc CbCr Payload base */
		p->addr[1] = SBWC_8B_CBCR_BASE(p->addr[0], p->src.f_w, p->src.f_h);
	case DECON_PIXEL_FORMAT_NV12M_SBWC_8B:
	case DECON_PIXEL_FORMAT_NV21M_SBWC_8B:
		/* payload */
		p->addr[3] = p->addr[1];
		p->addr[1] = p->addr[0];
		p->ypl_c2_strd = SBWC_8B_STRIDE(p->src.f_w);
		p->cpl_strd = p->ypl_c2_strd;
		/* header */
		p->addr[0] = p->addr[1] + SBWC_8B_Y_SIZE(p->src.f_w, p->src.f_h);
		p->addr[2] = p->addr[3] + SBWC_8B_CBCR_SIZE(p->src.f_w, p->src.f_h);
		p->yhd_y2_strd = SBWC_HEADER_STRIDE(p->src.f_w);
		p->chd_strd = SBWC_HEADER_STRIDE(p->src.f_w);
		break;

	case DECON_PIXEL_FORMAT_NV12N_SBWC_10B: /* single fd : [0]-Y_PL */
		/* calc CbCr Payload base */
		p->addr[1] = SBWC_10B_CBCR_BASE(p->addr[0], p->src.f_w, p->src.f_h);
	case DECON_PIXEL_FORMAT_NV12M_SBWC_10B:
	case DECON_PIXEL_FORMAT_NV21M_SBWC_10B:
		/* payload */
		p->addr[3] = p->addr[1];
		p->addr[1] = p->addr[0];
		p->ypl_c2_strd = SBWC_10B_STRIDE(p->src.f_w);
		p->cpl_strd = p->ypl_c2_strd;
		/* header */
		p->addr[0] = p->addr[1] + SBWC_10B_Y_SIZE(p->src.f_w, p->src.f_h);
		p->addr[2] = p->addr[3] + SBWC_10B_CBCR_SIZE(p->src.f_w, p->src.f_h);
		p->yhd_y2_strd = SBWC_HEADER_STRIDE(p->src.f_w);
		p->chd_strd = SBWC_HEADER_STRIDE(p->src.f_w);
		break;

	/* for lossy SBWC */
	case DECON_PIXEL_FORMAT_NV12N_SBWC_8B_L50: /* single fd : [0]-Y_PL */
		/* calc CbCr Payload base */
		p->addr[1] = SBWCL_8B_CBCR_BASE(p->addr[0], p->src.f_w, p->src.f_h, 50);
	case DECON_PIXEL_FORMAT_NV12M_SBWC_8B_L50:
		/* payload */
		p->addr[3] = p->addr[1];
		p->addr[1] = p->addr[0];
		p->ypl_c2_strd = SBWCL_8B_STRIDE(p->src.f_w, 50);
		p->cpl_strd = p->ypl_c2_strd;
		/* meaningless header */
		p->addr[0] = p->addr[1];
		p->addr[2] = p->addr[3];
		break;

	case DECON_PIXEL_FORMAT_NV12N_SBWC_8B_L75: /* single fd : [0]-Y_PL */
		/* calc CbCr Payload base */
		p->addr[1] = SBWCL_8B_CBCR_BASE(p->addr[0], p->src.f_w, p->src.f_h, 75);
	case DECON_PIXEL_FORMAT_NV12M_SBWC_8B_L75:
		/* payload */
		p->addr[3] = p->addr[1];
		p->addr[1] = p->addr[0];
		p->ypl_c2_strd = SBWCL_8B_STRIDE(p->src.f_w, 75);
		p->cpl_strd = p->ypl_c2_strd;
		/* meaningless header */
		p->addr[0] = p->addr[1];
		p->addr[2] = p->addr[3];
		break;

	case DECON_PIXEL_FORMAT_NV12N_SBWC_10B_L40: /* single fd : [0]-Y_PL */
		/* calc CbCr Payload base */
		p->addr[1] = SBWCL_10B_CBCR_BASE(p->addr[0], p->src.f_w, p->src.f_h, 40);
	case DECON_PIXEL_FORMAT_NV12M_SBWC_10B_L40:
		/* payload */
		p->addr[3] = p->addr[1];
		p->addr[1] = p->addr[0];
		p->ypl_c2_strd = SBWCL_10B_STRIDE(p->src.f_w, 40);
		p->cpl_strd = p->ypl_c2_strd;
		/* meaningless header */
		p->addr[0] = p->addr[1];
		p->addr[2] = p->addr[3];
		break;

	case DECON_PIXEL_FORMAT_NV12N_SBWC_10B_L60: /* single fd : [0]-Y_PL */
		/* calc CbCr Payload base */
		p->addr[1] = SBWCL_10B_CBCR_BASE(p->addr[0], p->src.f_w, p->src.f_h, 60);
	case DECON_PIXEL_FORMAT_NV12M_SBWC_10B_L60:
		/* payload */
		p->addr[3] = p->addr[1];
		p->addr[1] = p->addr[0];
		p->ypl_c2_strd = SBWCL_10B_STRIDE(p->src.f_w, 60);
		p->cpl_strd = p->ypl_c2_strd;
		/* meaningless header */
		p->addr[0] = p->addr[1];
		p->addr[2] = p->addr[3];
		break;

	case DECON_PIXEL_FORMAT_NV12N_SBWC_10B_L80: /* single fd : [0]-Y_PL */
		/* calc CbCr Payload base */
		p->addr[1] = SBWCL_10B_CBCR_BASE(p->addr[0], p->src.f_w, p->src.f_h, 80);
	case DECON_PIXEL_FORMAT_NV12M_SBWC_10B_L80:
		/* payload */
		p->addr[3] = p->addr[1];
		p->addr[1] = p->addr[0];
		p->ypl_c2_strd = SBWCL_10B_STRIDE(p->src.f_w, 80);
		p->cpl_strd = p->ypl_c2_strd;
		/* meaningless header */
		p->addr[0] = p->addr[1];
		p->addr[2] = p->addr[3];
		break;

	default:
		dpp_dbg("%s: YUV format(%d) doesn't require BASE ADDR Calc.\n",
			__func__, p->format);
		break;
	}
}

static int dpp_get_params(struct dpp_device *dpp, struct dpp_params_info *p)
{
	u64 src_w, src_h, dst_w, dst_h;
	struct decon_win_config *config = &dpp->dpp_config->config;
	struct dpp_restriction *res = &dpp->restriction;
	const struct dpu_fmt *fmt_info = dpu_find_fmt_info(config->format);

	p->rcv_num = dpp->dpp_config->rcv_num;
	memcpy(&p->src, &config->src, sizeof(struct decon_frame));
	memcpy(&p->dst, &config->dst, sizeof(struct decon_frame));
	memcpy(&p->block, &config->block_area, sizeof(struct decon_win_rect));
#if defined(CONFIG_EXYNOS_SUPPORT_8K_SPLIT)
	memcpy(&p->aux_src, &config->aux_src, sizeof(struct aux_frame));
#endif
	p->rot = config->dpp_parm.rot;
	p->is_comp = config->compression;
	p->format = config->format;
	p->addr[0] = config->dpp_parm.addr[0];
	p->addr[1] = config->dpp_parm.addr[1];
	p->addr[2] = 0;
	p->addr[3] = 0;
	p->eq_mode = config->dpp_parm.eq_mode;
	p->hdr = config->dpp_parm.hdr_std;
	p->max_luminance = config->dpp_parm.max_luminance;
	p->min_luminance = config->dpp_parm.min_luminance;
	p->yhd_y2_strd = 0;
	p->ypl_c2_strd = 0;
	p->chd_strd = 0;
	p->cpl_strd = 0;

	p->comp_type = fmt_info->ct;
	if (p->is_comp) {
		if (fmt_info->cs == DPU_COLORSPACE_RGB)
			p->comp_type = COMP_TYPE_AFBC;
		else {
			p->comp_type = COMP_TYPE_NONE;
			dpp_err("%s: Unknown compression type!!\n", __func__);
			return -EINVAL;
		}
	}

	/* YUV formats need base address calcuation */
	if (fmt_info->cs != DPU_COLORSPACE_RGB)
		dpp_get_base_addr_params(p);

#if defined(CONFIG_EXYNOS_SUPPORT_8K_SPLIT)
	if (is_rotation(config)) {
		src_w = p->src.h - p->aux_src.padd_h;
		src_h = p->src.w - p->aux_src.padd_w;
	} else {
		src_w = p->src.w - p->aux_src.padd_w;
		src_h = p->src.h - p->aux_src.padd_h;
	}
#else
	if (is_rotation(config)) {
		src_w = p->src.h;
		src_h = p->src.w;
	} else {
		src_w = p->src.w;
		src_h = p->src.h;
	}
#endif
	dst_w = p->dst.w;
	dst_h = p->dst.h;

	p->h_ratio = (src_w << 20) / dst_w;
	p->v_ratio = (src_h << 20) / dst_h;

	if ((p->h_ratio != (1 << 20)) || (p->v_ratio != (1 << 20)))
		p->is_scale = true;
	else
		p->is_scale = false;

	if ((config->dpp_parm.rot != DPP_ROT_NORMAL) || (p->is_scale) ||
			IS_YUV(fmt_info) || (p->block.w < res->blk_w.min) ||
			(p->block.h < res->blk_h.min))
		p->is_block = false;
	else
		p->is_block = true;

#if defined(CONFIG_EXYNOS_SUPPORT_HWFC)
	p->hwfc_enable = dpp->dpp_config->hwfc_enable;
	p->hwfc_idx = dpp->dpp_config->hwfc_idx;
#endif

#if defined(CONFIG_EXYNOS_SUPPORT_VOTF_IN)
	p->votf_enable = config->votf_en;
	p->votf_buffer_idx = config->votf_buf_idx;
#endif

#if IS_ENABLED(CONFIG_EXYNOS_SBWC_LIBREQ)
	p->lib_requested = dpp->dpp_config->lib_requested;
#endif
	return 0;
}

static int dpp_check_size(struct dpp_device *dpp, struct dpp_img_format *vi)
{
	struct decon_win_config *config = &dpp->dpp_config->config;
	struct decon_frame *src = &config->src;
	struct decon_frame *dst = &config->dst;
	struct dpp_size_constraints vc;

	dpp_constraints_params(&vc, vi, &dpp->restriction);

	if ((!check_align(src->x, src->y, vc.src_mul_x, vc.src_mul_y)) ||
	   (!check_align(src->f_w, src->f_h, vc.src_mul_w, vc.src_mul_h)) ||
	   (!check_align(src->w, src->h, vc.img_mul_w, vc.img_mul_h)) ||
	   (!check_align(dst->w, dst->h, vc.sca_mul_w, vc.sca_mul_h))) {
		dpp_err("Alignment error!\n");
		goto err;
	}

	if (config->dpp_parm.rot > DPP_ROT_180) {
		if (src->w > vc.img_w_max || src->h < vc.img_w_min ||
			src->h > vc.img_h_max || src->w < vc.img_h_min) {
			dpp_err("Unsupported SRC size!\n");
			goto err;
		}
	} else {
		if (src->w > vc.img_w_max || src->w < vc.img_w_min ||
			src->h > vc.img_h_max || src->h < vc.img_h_min) {
			dpp_err("Unsupported SRC size!\n");
			goto err;
		}
	}

	if (dst->w > vc.sca_w_max || dst->w < vc.sca_w_min ||
		dst->h > vc.sca_h_max || dst->h < vc.sca_h_min) {
		dpp_err("Unsupported DST size!\n");
		goto err;
	}

	/* check boundary */
	if (src->x + src->w > vc.src_w_max || src->y + src->h > vc.src_h_max) {
		dpp_err("Unsupported src boundary size!\n");
		goto err;
	}

	if (src->x + src->w > src->f_w || src->y + src->h > src->f_h) {
		dpp_err("Unsupported src range!\n");
		goto err;
	}

	if (src->x < 0 || src->y < 0 ||
		dst->x < 0 || dst->y < 0) {
		dpp_err("Unsupported src/dst x,y position!\n");
		goto err;
	}

	return 0;
err:
	dpp_err("offset x : %d, offset y: %d\n", src->x, src->y);
	dpp_err("src_mul_x : %d, src_mul_y : %d\n", vc.src_mul_x, vc.src_mul_y);
	dpp_err("src f_w : %d, src f_h: %d\n", src->f_w, src->f_h);
	dpp_err("src_mul_w : %d, src_mul_h : %d\n", vc.src_mul_w, vc.src_mul_h);
	dpp_err("src w : %d, src h: %d\n", src->w, src->h);
	dpp_err("img_mul_w : %d, img_mul_h : %d\n", vc.img_mul_w, vc.img_mul_h);
	dpp_err("dst w : %d, dst h: %d\n", dst->w, dst->h);
	dpp_err("sca_mul_w : %d, sca_mul_h : %d\n", vc.sca_mul_w, vc.sca_mul_h);
	dpp_err("rotation : %d, color_format : %d\n",
				config->dpp_parm.rot, config->format);
	dpp_err("hdr : %d, color_format : %d\n",
				config->dpp_parm.hdr_std, config->format);
	return -EINVAL;
}

static int dpp_check_scale_ratio(struct dpp_device *dpp, struct dpp_params_info *p)
{
	u32 sc_down_max_w, sc_down_max_h;
	u32 sc_up_min_w, sc_up_min_h;
	u32 sc_src_w, sc_src_h;
	struct dpp_restriction *res = &dpp->restriction;

	sc_down_max_w = p->dst.w * res->scale_down;
	sc_down_max_h = p->dst.h * res->scale_down;
	sc_up_min_w = (p->dst.w + (res->scale_up - 1)) / res->scale_up;
	sc_up_min_h = (p->dst.h + (res->scale_up - 1)) / res->scale_up;
#if defined(CONFIG_EXYNOS_SUPPORT_8K_SPLIT)
	if (p->rot > DPP_ROT_180) {
		sc_src_w = p->src.h - p->aux_src.padd_h;
		sc_src_h = p->src.w - p->aux_src.padd_w;
	} else {
		sc_src_w = p->src.w - p->aux_src.padd_w;
		sc_src_h = p->src.h - p->aux_src.padd_h;
	}
#else
	if (p->rot > DPP_ROT_180) {
		sc_src_w = p->src.h;
		sc_src_h = p->src.w;
	} else {
		sc_src_w = p->src.w;
		sc_src_h = p->src.h;
	}
#endif

	if (sc_src_w > sc_down_max_w || sc_src_h > sc_down_max_h) {
		dpp_err("Not support under 1/%dx scale-down!\n", res->scale_down);
		goto err;
	}

	if (sc_src_w < sc_up_min_w || sc_src_h < sc_up_min_h) {
		dpp_err("Not support over %dx scale-up\n", res->scale_up);
		goto err;
	}

	return 0;
err:
	dpp_err("src w(%d) h(%d), dst w(%d) h(%d), rotation(%d)\n",
			p->src.w, p->src.h, p->dst.w, p->dst.h, p->rot);
	return -EINVAL;
}

static int dpp_check_addr(struct dpp_device *dpp, struct dpp_params_info *p)
{
	const struct dpu_fmt *fmt_info = dpu_find_fmt_info(p->format);
	int i;

	for (i = 0; i < fmt_info->num_planes; ++i) {
		if (IS_ERR_OR_NULL((void *)p->addr[i])) {
			dpp_err("DPP%d base address[%d] is NULL\n",
					dpp->id, i);
			return -EINVAL;
		}
	}

	return 0;
}

static int dpp_check_format(struct dpp_device *dpp, struct dpp_params_info *p)
{
	const struct dpu_fmt *fmt_info = dpu_find_fmt_info(p->format);

	if (!test_bit(DPP_ATTR_ROT, &dpp->attr) && (p->rot > DPP_ROT_180)) {
		dpp_err("Not support rotation(%d) in DPP%d\n",
				p->rot, dpp->id);
		return -EINVAL;
	}

#ifndef CONFIG_MCDHDR
	if (!test_bit(DPP_ATTR_HDR, &dpp->attr) && (p->hdr > DPP_HDR_OFF)) {
		dpp_err("Not support HDR:%d in DPP%d - No H/W!\n",
				p->hdr, dpp->id);
		return -EINVAL;
	}
#endif
	if ((p->hdr < DPP_HDR_OFF) || (p->hdr > DPP_TRANSFER_GAMMA2_8)) {
		dpp_err("Unsupported HDR standard in DPP%d, HDR std(%d)\n",
				dpp->id, p->hdr);
		return -EINVAL;
	}

	if (!test_bit(DPP_ATTR_CSC, &dpp->attr) && IS_YUV(fmt_info)) {
		dpp_err("Not support YUV format(%d) in DPP%d\n",
			p->format, dpp->id);
		return -EINVAL;
	}

	if (!test_bit(DPP_ATTR_AFBC, &dpp->attr) && p->is_comp) {
		dpp_err("Not support AFBC decoding in DPP%d\n",
				dpp->id);
		return -EINVAL;
	}

	if (!test_bit(DPP_ATTR_SCALE, &dpp->attr) && p->is_scale) {
		dpp_err("Not support SCALING in DPP%d\n", dpp->id);
		return -EINVAL;
	}

	if (!test_bit(DPP_ATTR_SBWC, &dpp->attr)
		&& ((p->comp_type == COMP_TYPE_SBWC) || (p->comp_type == COMP_TYPE_SBWCL))) {
		dpp_err("Not support SBWC in DPP%d\n", dpp->id);
		return -EINVAL;
	}

	return 0;
}

/*
 * TODO: h/w limitation will be changed in KC
 * This function must be modified for KC after releasing DPP constraints
 */
static int dpp_check_limitation(struct dpp_device *dpp, struct dpp_params_info *p)
{
	int ret = 0;
	struct dpp_img_format vi;
	const struct dpu_fmt *fmt_info = dpu_find_fmt_info(p->format);

	if (dpp->id != ODMA_WB) {
		ret = dpp_check_scale_ratio(dpp, p);
		if (ret) {
			dpp_err("failed to set dpp%d scale information\n",
					dpp->id);
			return -EINVAL;
		}
	}

	dpp_select_format(dpp, &vi, p);

	ret = dpp_check_format(dpp, p);
	if (ret)
		return -EINVAL;

	ret = dpp_check_addr(dpp, p);
	if (ret)
		return -EINVAL;

	if (p->is_comp && p->rot) {
		dpp_err("Not support [AFBC+ROTATION] at the same time in DPP%d\n",
			dpp->id);
		return -EINVAL;
	}

	if (p->is_comp && p->is_block) {
		dpp_err("Not support [AFBC+BLOCK] at the same time in DPP%d\n",
			dpp->id);
		return -EINVAL;
	}

	if (p->is_comp && IS_YUV420(fmt_info)) {
		dpp_err("Not support AFBC decoding for YUV format in DPP%d\n",
			dpp->id);
		return -EINVAL;
	}

	if (p->is_comp && ((p->comp_type == COMP_TYPE_SBWC) || (p->comp_type == COMP_TYPE_SBWCL))) {
		dpp_err("Not support AFBC & SBWC at the same time in DPP%d\n",
			dpp->id);
		return -EINVAL;
	}

	if (((p->comp_type == COMP_TYPE_SBWC) || (p->comp_type == COMP_TYPE_SBWCL)) &&
		((p->rot > DPP_ROT_NORMAL) && (p->rot < DPP_ROT_90))) {
		dpp_err("Not support SBWC & FLIP at the same time in DPP%d\n",
			dpp->id);
		return -EINVAL;
	}

	if (p->is_block && p->is_scale) {
		dpp_err("Not support [BLOCK+SCALE] at the same time in DPP%d\n",
			dpp->id);
		return -EINVAL;
	}

	if (p->is_block && IS_YUV420(fmt_info)) {
		dpp_err("Not support BLOCK Mode for YUV format in DPP%d\n",
			dpp->id);
		return -EINVAL;
	}

	if (!IS_YUV(fmt_info) && (p->rot > DPP_ROT_180)) {
		dpp_err("Not support RGB ROTATION in DPP%d\n",
			dpp->id);
		return -EINVAL;
	}

	/* FIXME */
	if (p->is_block && p->rot) {
		dpp_err("Not support [BLOCK+ROTATION] at the same time in DPP%d\n",
			dpp->id);
		return -EINVAL;
	}

	ret = dpp_check_size(dpp, &vi);
	if (ret)
		return -EINVAL;

	return 0;
}

static int dpp_afbc_enabled(struct dpp_device *dpp, int *afbc_enabled)
{
	int ret = 0;

	if (test_bit(DPP_ATTR_AFBC, &dpp->attr))
		*afbc_enabled = 1;
	else
		*afbc_enabled = 0;

	return ret;
}

static int dpp_set_cursor_config(struct dpp_device *dpp)
{
	struct dpp_params_info params;
	int ret = 0;

	/* parameters from decon driver are translated for dpp driver */
	dpp_get_params(dpp, &params);

	/* all parameters must be passed dpp hw limitation */
	ret = dpp_check_limitation(dpp, &params);
	if (ret)
		goto err;

	/* set all parameters to dpp hw */
	dpp_reg_configure_params(dpp->id, &params, dpp->attr);

	dpp_dbg("dpp%d cursor configuration\n", dpp->id);

err:
	return ret;
}

static int dpp_set_config(struct dpp_device *dpp)
{
	struct dpp_params_info params;
	int ret = 0;

	mutex_lock(&dpp->lock);

	/* parameters from decon driver are translated for dpp driver */
	ret = dpp_get_params(dpp, &params);
	if (ret)
		goto err;

	/* all parameters must be passed dpp hw limitation */
	ret = dpp_check_limitation(dpp, &params);
	if (ret)
		goto err;

	if (dpp->state == DPP_STATE_OFF) {
		dpp_dbg("dpp%d is started\n", dpp->id);
		dpp_reg_init(dpp->id, dpp->attr);

		enable_irq(dpp->res.dma_irq);
		if (test_bit(DPP_ATTR_DPP, &dpp->attr) && !IS_WB(dpp->attr))
			enable_irq(dpp->res.irq);
	}
#if IS_ENABLED(CONFIG_MCD_PANEL)
	ret = dpp_set_mcdhdr_params(dpp, &params);
	if (ret) {
		dpp_err("%s: failed to set mcdhdr params\n", __func__);
		goto err;
	}
#endif
	/* set all parameters to dpp hw */
	dpp_reg_configure_params(dpp->id, &params, dpp->attr);

	DPU_EVENT_LOG(DPU_EVT_DPP_WINCON, &dpp->sd, ktime_set(0, 0));

	dpp_dbg("dpp%d configuration\n", dpp->id);

	dpp->state = DPP_STATE_ON;
	/* to prevent irq storm, irq enable is moved here */
	dpp_reg_irq_enable(dpp->id, dpp->attr);
err:
	mutex_unlock(&dpp->lock);
	return ret;
}

static int dpp_stop(struct dpp_device *dpp, bool reset)
{
	int ret = 0;

	mutex_lock(&dpp->lock);

	if (dpp->state == DPP_STATE_OFF) {
		dpp_warn("dpp%d is already disabled\n", dpp->id);
		goto err;
	}

	DPU_EVENT_LOG(DPU_EVT_DPP_STOP, &dpp->sd, ktime_set(0, 0));

	dpp_reg_deinit(dpp->id, reset, dpp->attr);

	disable_irq(dpp->res.dma_irq);
	if (test_bit(DPP_ATTR_DPP, &dpp->attr) && !IS_WB(dpp->attr))
		disable_irq(dpp->res.irq);

#if IS_ENABLED(CONFIG_MCD_PANEL)
	dpp_stop_mcdhdr(dpp);
#endif
	dpp_dbg("dpp%d is stopped\n", dpp->id);

	dpp->state = DPP_STATE_OFF;
err:
	mutex_unlock(&dpp->lock);
	return ret;
}

static int dpp_s_stream(struct v4l2_subdev *sd, int enable)
{
	struct dpp_device *dpp = container_of(sd, struct dpp_device, sd);

	dpp_dbg("%s +\n", __func__);

	if (!IS_WB(dpp->attr)) {
		dpp_err("%s: dpp%d is not support writeback\n", __func__, dpp->id);
		return -EINVAL;
	}

	if (enable) {
		if (dpp->wb_state == WBMUX_STATE_ON) {
			dpp_warn("dpp%d is already enabled as writeback\n", dpp->id);
			return 0;
		}

		pm_runtime_get_sync(dpp->dev);
		dpp->wb_state = WBMUX_STATE_ON;
	} else {
		if (dpp->wb_state == WBMUX_STATE_OFF) {
			dpp_warn("dpp%d is already disabled as writeback\n", dpp->id);
			return 0;
		}

		pm_runtime_put_sync(dpp->dev);
		dpp->wb_state = WBMUX_STATE_OFF;
	}

	dpp_dbg("%s -\n", __func__);

	return 0;
}

static long dpp_subdev_ioctl(struct v4l2_subdev *sd, unsigned int cmd, void *arg)
{
	struct dpp_device *dpp = v4l2_get_subdevdata(sd);
	bool reset = (bool)arg;
	int ret = 0;
	int *afbc_enabled;
	u32 shd_addr[MAX_PLANE_ADDR_CNT];

	switch (cmd) {
	case DPP_WIN_CONFIG:
		if (!arg) {
			dpp_err("failed to get dpp_config\n");
			ret = -EINVAL;
			break;
		}
		dpp->dpp_config = (struct dpp_config *)arg;
		ret = dpp_set_config(dpp);
		if (ret)
			dpp_err("failed to configure dpp%d\n", dpp->id);
		break;

	case DPP_CURSOR_WIN_CONFIG:
		if (!arg) {
			dpp_err("failed to get cursor_config\n");
			ret = -EINVAL;
			break;
		}
		dpp->dpp_config = (struct dpp_config *)arg;
		ret = dpp_set_cursor_config(dpp);
		if (ret)
			dpp_err("failed to configure dpp%d\n", dpp->id);
		break;

	case DPP_STOP:
		ret = dpp_stop(dpp, reset);
		if (ret)
			dpp_err("failed to stop dpp%d\n", dpp->id);
		break;

	case DPP_DUMP:
		dpp_dump(dpp);
		break;

	case DPP_WB_WAIT_FOR_FRAMEDONE:
		ret = dpp_wb_wait_for_framedone(dpp);
		break;

	case DPP_AFBC_ATTR_ENABLED:
		if (!arg) {
			dpp_err("failed to get afbc enabled info\n");
			ret = -EINVAL;
			break;
		}
		afbc_enabled = (int *)arg;
		ret = dpp_afbc_enabled(dpp, afbc_enabled);
		break;

	case DPP_GET_PORT_NUM:
		if (!arg) {
			dpp_err("failed to get AXI port number\n");
			ret = -EINVAL;
			break;
		}
		*(int *)arg = dpp->port;
		break;

	case DPP_GET_RESTRICTION:
		if (!arg) {
			dpp_err("failed to get dpp restriction\n");
			ret = -EINVAL;
			break;
		}
		memcpy(&(((struct dpp_ch_restriction *)arg)->restriction),
				&dpp->restriction,
				sizeof(struct dpp_restriction));
		((struct dpp_ch_restriction *)arg)->id = dpp->id;
		((struct dpp_ch_restriction *)arg)->attr = dpp->attr;
		break;

	case DPP_GET_SHD_ADDR:
		if (!arg) {
			dpp_err("failed to get shadow base address\n");
			ret = -EINVAL;
			break;
		}

		dma_reg_get_shd_addr(dpp->id, shd_addr, dpp->attr);
		memcpy((u32 *)arg, shd_addr, sizeof(shd_addr));
		break;

	case EXYNOS_DPU_GET_ACLK: /* for WB */
#if defined(CONFIG_EXYNOS_EMUL_DISP)
		return 600000000;
#else
		return clk_get_rate(dpp->res.aclk);
#endif
		break;

	default:
		break;
	}

	return ret;
}

static const struct v4l2_subdev_core_ops dpp_subdev_core_ops = {
	.ioctl = dpp_subdev_ioctl,
};

static const struct v4l2_subdev_video_ops dpp_subdev_video_ops = {
	.s_stream = dpp_s_stream,
};

static struct v4l2_subdev_ops dpp_subdev_ops = {
	.core = &dpp_subdev_core_ops,
	.video = &dpp_subdev_video_ops,
};

static void dpp_init_subdev(struct dpp_device *dpp)
{
	struct v4l2_subdev *sd = &dpp->sd;

	v4l2_subdev_init(sd, &dpp_subdev_ops);
	sd->owner = THIS_MODULE;
	sd->grp_id = dpp->id;
	snprintf(sd->name, sizeof(sd->name), "%s.%d", "dpp-sd", dpp->id);
	v4l2_set_subdevdata(sd, dpp);
}

#if defined(SYSFS_UNITTEST_INTERFACE)
/* dpp irq error state */
static ssize_t dpp_irq_err_sysfs_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	struct dpp_device *dpp = dev_get_drvdata(dev);
	int size = 0;
	int count;

	size = (ssize_t)sprintf(buf, "0x%X", dpp->dpp_irq_err_state);
	dpp_info("DPP(%d) IRQ State : 0x%X\n", dpp->id, dpp->dpp_irq_err_state);

	count = strlen(buf);
	return count;
}

static ssize_t dpp_irq_err_sysfs_store(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t count)
{
	int ret;
	unsigned long cmd;
	struct dpp_device *dpp = dev_get_drvdata(dev);

	ret = kstrtoul(buf, 0, &cmd);
	if (ret)
		return ret;

	if (cmd == 0) {
		dpp_info("DPP(%d) IRQ State: Clear cmd\n", dpp->id);
		dpp->dpp_irq_err_state = 0;
	}
	else {
		dpp_info("DPP(%d) IRQ State: Unknown cmd = %d\n", dpp->id, cmd);
	}

	return count;
}
static DEVICE_ATTR(dpp_irq_err, 0600, dpp_irq_err_sysfs_show, dpp_irq_err_sysfs_store);

int dpp_create_irq_err_sysfs(struct dpp_device *dpp)
{
	int ret = 0;

	ret = device_create_file(dpp->dev, &dev_attr_dpp_irq_err);
	if (ret) {
		dpp_err("failed to create dpp irq err sysfs\n");
		goto error;
	}

	error:

	return ret;
}

/* dma irq error state */
static ssize_t dma_irq_err_sysfs_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	struct dpp_device *dpp = dev_get_drvdata(dev);
	int size = 0;
	int count;

	size = (ssize_t)sprintf(buf, "0x%X", dpp->dma_irq_err_state);
	dpp_info("DMA(%d) IRQ State : 0x%X\n", dpp->id, dpp->dma_irq_err_state);

	count = strlen(buf);
	return count;
}

static ssize_t dma_irq_err_sysfs_store(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t count)
{
	int ret;
	unsigned long cmd;
	struct dpp_device *dpp = dev_get_drvdata(dev);

	ret = kstrtoul(buf, 0, &cmd);
	if (ret)
		return ret;

	if (cmd == 0) {
		dpp_info("DMA(%d) IRQ State: Clear cmd\n", dpp->id);
		dpp->dma_irq_err_state = 0;
	}
	else {
		dpp_info("DMA(%d) IRQ State: Unknown cmd = %d\n", dpp->id, cmd);
	}

	return count;
}
static DEVICE_ATTR(dma_irq_err, 0600, dma_irq_err_sysfs_show, dma_irq_err_sysfs_store);

int dma_create_irq_err_sysfs(struct dpp_device *dpp)
{
	int ret = 0;

	ret = device_create_file(dpp->dev, &dev_attr_dma_irq_err);
	if (ret) {
		dpp_err("failed to create dma irq err sysfs\n");
		goto error;
	}

	error:

	return ret;
}
#endif

static void dpp_parse_restriction(struct dpp_device *dpp, struct device_node *n)
{
	u32 range[3] = {0, };
	u32 align[2] = {0, };

	dpp_info("dpp restriction\n");
	of_property_read_u32_array(n, "src_f_w", range, 3);
	dpp->restriction.src_f_w.min = range[0];
	dpp->restriction.src_f_w.max = range[1];
	dpp->restriction.src_f_w.align = range[2];

	of_property_read_u32_array(n, "src_f_h", range, 3);
	dpp->restriction.src_f_h.min = range[0];
	dpp->restriction.src_f_h.max = range[1];
	dpp->restriction.src_f_h.align = range[2];

	of_property_read_u32_array(n, "src_w", range, 3);
	dpp->restriction.src_w.min = range[0];
	dpp->restriction.src_w.max = range[1];
	dpp->restriction.src_w.align = range[2];

	of_property_read_u32_array(n, "src_h", range, 3);
	dpp->restriction.src_h.min = range[0];
	dpp->restriction.src_h.max = range[1];
	dpp->restriction.src_h.align = range[2];

	of_property_read_u32_array(n, "src_xy_align", align, 2);
	dpp->restriction.src_x_align = align[0];
	dpp->restriction.src_y_align = align[1];

	of_property_read_u32_array(n, "dst_f_w", range, 3);
	dpp->restriction.dst_f_w.min = range[0];
	dpp->restriction.dst_f_w.max = range[1];
	dpp->restriction.dst_f_w.align = range[2];

	of_property_read_u32_array(n, "dst_f_h", range, 3);
	dpp->restriction.dst_f_h.min = range[0];
	dpp->restriction.dst_f_h.max = range[1];
	dpp->restriction.dst_f_h.align = range[2];

	of_property_read_u32_array(n, "dst_w", range, 3);
	dpp->restriction.dst_w.min = range[0];
	dpp->restriction.dst_w.max = range[1];
	dpp->restriction.dst_w.align = range[2];

	of_property_read_u32_array(n, "dst_h", range, 3);
	dpp->restriction.dst_h.min = range[0];
	dpp->restriction.dst_h.max = range[1];
	dpp->restriction.dst_h.align = range[2];

	of_property_read_u32_array(n, "dst_xy_align", align, 2);
	dpp->restriction.dst_x_align = align[0];
	dpp->restriction.dst_y_align = align[1];

	of_property_read_u32_array(n, "blk_w", range, 3);
	dpp->restriction.blk_w.min = range[0];
	dpp->restriction.blk_w.max = range[1];
	dpp->restriction.blk_w.align = range[2];

	of_property_read_u32_array(n, "blk_h", range, 3);
	dpp->restriction.blk_h.min = range[0];
	dpp->restriction.blk_h.max = range[1];
	dpp->restriction.blk_h.align = range[2];

	of_property_read_u32_array(n, "blk_xy_align", align, 2);
	dpp->restriction.blk_x_align = align[0];
	dpp->restriction.blk_y_align = align[1];

	if (of_property_read_u32(n, "src_w_rot_max",
				&dpp->restriction.reserved[SRC_W_ROT_MAX]))
		dpp->restriction.reserved[SRC_W_ROT_MAX] = dpp->restriction.src_w.max;
	if (of_property_read_u32(n, "src_h_rot_max",
				&dpp->restriction.src_h_rot_max))
		dpp->restriction.src_h_rot_max = dpp->restriction.src_h.max;
}

static void dpp_print_restriction(struct dpp_device *dpp)
{
	struct dpp_restriction *res = &dpp->restriction;

	dpp_info("src_f_w[%d %d %d] src_f_h[%d %d %d]\n",
			res->src_f_w.min, res->src_f_w.max, res->src_f_w.align,
			res->src_f_h.min, res->src_f_h.max, res->src_f_h.align);
	dpp_info("src_w[%d %d %d] src_h[%d %d %d] src_x_y_align[%d %d]\n",
			res->src_w.min, res->src_w.max, res->src_w.align,
			res->src_h.min, res->src_h.max, res->src_h.align,
			res->src_x_align, res->src_y_align);

	dpp_info("dst_f_w[%d %d %d] dst_f_h[%d %d %d]\n",
			res->dst_f_w.min, res->dst_f_w.max, res->dst_f_w.align,
			res->dst_f_h.min, res->dst_f_h.max, res->dst_f_h.align);
	dpp_info("dst_w[%d %d %d] dst_h[%d %d %d] dst_x_y_align[%d %d]\n",
			res->dst_w.min, res->dst_w.max, res->dst_w.align,
			res->dst_h.min, res->dst_h.max, res->dst_h.align,
			res->dst_x_align, res->dst_y_align);

	dpp_info("blk_w[%d %d %d] blk_h[%d %d %d] blk_x_y_align[%d %d]\n",
			res->blk_w.min, res->blk_w.max, res->blk_w.align,
			res->blk_h.min, res->blk_h.max, res->blk_h.align,
			res->blk_x_align, res->blk_y_align);

	dpp_info("src_w_rot_max[%d]\n", res->reserved[SRC_W_ROT_MAX]);
	dpp_info("src_h_rot_max[%d]\n", res->src_h_rot_max);
}

#if IS_ENABLED(CONFIG_MCD_PANEL)
#define CHECK_CNT	20
static bool dpp0_probe_is_done(void)
{
	int check_cnt = CHECK_CNT;

	if (get_dpp_drvdata(0))
		return true;
	while (!get_dpp_drvdata(0) && check_cnt) {
		usleep_range(1000, 1100);
		check_cnt--;
	}
	if (check_cnt == 0) {
		dpp_err("dpp0 probe is failed\n");
		return false;
	} else {
		usleep_range(1000, 1100);
		dpp_warn("dpp0 probe is late, remaining check cnt:%d \n", check_cnt);
		return true;
	}
}
#endif

static void dpp_parse_dt(struct dpp_device *dpp, struct device *dev)
{
	struct device_node *node = dev->of_node;
	struct dpp_device *dpp0;
	struct dpp_restriction *res = &dpp->restriction;
	int i;
	char format_list[256] = {0, };
	int len = 0, ret;

	dpp->id = of_alias_get_id(dev->of_node, "dpp");

#if IS_ENABLED(CONFIG_MCD_PANEL)
	if (dpp->id && !dpp0_probe_is_done()) {
		dpp_err("dpp0 probe is not finished\n");
	}
#endif
	dpp0 = get_dpp_drvdata(0);

	dpp_info("dpp(%d) probe start..\n", dpp->id);
	of_property_read_u32(node, "attr", (u32 *)&dpp->attr);
	dpp_info("attributes = 0x%lx\n", dpp->attr);
	of_property_read_u32(node, "port", (u32 *)&dpp->port);
	dpp_info("AXI port = %d\n", dpp->port);

	if (dpp->id == 0) {
		dpp_parse_restriction(dpp, node);
		dpp_print_restriction(dpp);
	} else {
		memcpy(&dpp->restriction, &dpp0->restriction,
				sizeof(struct dpp_restriction));
		dpp_print_restriction(dpp);
	}

#if IS_ENABLED(CONFIG_EXYNOS_SBWC_LIBREQ)
	if (of_property_read_bool(node, "lib_reserved"))
		dpp->restriction.reserved[LIB_RESERVED] = 1;
	else
		dpp->restriction.reserved[LIB_RESERVED] = 0;
#endif

	of_property_read_u32(node, "scale_down", (u32 *)&res->scale_down);
	of_property_read_u32(node, "scale_up", (u32 *)&res->scale_up);
	dpp_info("max scale up(%dx), down(1/%dx) ratio\n", res->scale_up,
			res->scale_down);

	memcpy(res->format, default_fmt, sizeof(u32) * DEFAULT_FMT_CNT);
	of_property_read_u32(node, "fmt_cnt", (u32 *)&res->format_cnt);
	of_property_read_u32_array(node, "fmt", &res->format[DEFAULT_FMT_CNT],
			res->format_cnt);
	res->format_cnt += DEFAULT_FMT_CNT;
	dpp_info("supported format count = %d\n", dpp->restriction.format_cnt);

	for (i = 0; i < dpp->restriction.format_cnt; ++i) {
		ret = snprintf(format_list + len, sizeof(format_list) - len,
				"%d ", dpp->restriction.format[i]);
		len += ret;
	}
	format_list[len] = '\0';
	dpp_info("supported format list : %s\n", format_list);

	dpp->dev = dev;
}

static irqreturn_t dpp_irq_handler(int irq, void *priv)
{
	struct dpp_device *dpp = priv;
	u32 dpp_irq = 0;

	spin_lock(&dpp->slock);
	if (dpp->state == DPP_STATE_OFF)
		goto irq_end;

	dpp_irq = dpp_reg_get_irq_and_clear(dpp->id);

#if defined(SYSFS_UNITTEST_INTERFACE)
	// DPP_CFG_ERROR_IRQ
	if (dpp_irq & DPP_CFG_ERROR_IRQ) {
		dpp->dma_irq_err_state |= SYSFS_ERR_DPP_CONFIG_ERROR_IRQ;
	}
#endif

irq_end:
	spin_unlock(&dpp->slock);
	return IRQ_HANDLED;
}

static irqreturn_t dma_irq_handler(int irq, void *priv)
{
	struct dpp_device *dpp = priv;
	u32 irqs, val = 0;

	spin_lock(&dpp->dma_slock);
	if (dpp->state == DPP_STATE_OFF)
		goto irq_end;

	if (test_bit(DPP_ATTR_ODMA, &dpp->attr)) { /* ODMA case */
		irqs = odma_reg_get_irq_and_clear(dpp->id);

		if ((irqs & ODMA_WRITE_SLAVE_ERROR) ||
			       (irqs & ODMA_STATUS_DEADLOCK_IRQ)) {
			dpp_err("odma%d error irq occur(0x%x)\n", dpp->id, irqs);
			dpp_dump(dpp);
			goto irq_end;
		}
		if (irqs & ODMA_STATUS_FRAMEDONE_IRQ) {
			dpp_dbg("dpp%d framedone irq occurs\n", dpp->id);
			dpp->d.done_count++;
			wake_up_interruptible_all(&dpp->framedone_wq);
			DPU_EVENT_LOG(DPU_EVT_DPP_FRAMEDONE, &dpp->sd,
					ktime_set(0, 0));
			goto irq_end;
		}
	} else { /* IDMA case */
		irqs = idma_reg_get_irq_and_clear(dpp->id);

#if defined(SYSFS_UNITTEST_INTERFACE)
		// IDMA_CONFIG_ERR_IRQ
		if (irqs & IDMA_CONFIG_ERR_IRQ) {
			dpp->dma_irq_err_state |= SYSFS_ERR_DMA_CONFIG_ERROR_IRQ;
		}
#endif

		if (irqs & IDMA_RECOVERY_TRG_IRQ) {
			DPU_EVENT_LOG(DPU_EVT_DMA_RECOVERY, &dpp->sd,
					ktime_set(0, 0));
			val = (u32)dpp->dpp_config->config.dpp_parm.comp_src;
			dpp->d.recovery_cnt++;
#if IS_ENABLED(CONFIG_EXYNOS_PM_QOS) || IS_ENABLED(CONFIG_EXYNOS_PM_QOS_MODULE)
			dpp_info("dma%d recovery start(0x%x) cnt(%d) MIF(%lu), INT(%lu), DISP(%lu)\n",
				dpp->id, irqs, dpp->d.recovery_cnt,
				exynos_devfreq_get_domain_freq(DEVFREQ_MIF),
				exynos_devfreq_get_domain_freq(DEVFREQ_INT),
				exynos_devfreq_get_domain_freq(DEVFREQ_DISP));
#else
			dpp_info("dma%d recovery start(0x%x) cnt(%d)\n",
					dpp->id, irqs, dpp->d.recovery_cnt);
#endif
			goto irq_end;
		}
		if ((irqs & IDMA_READ_SLAVE_ERROR) ||
				(irqs & IDMA_STATUS_DEADLOCK_IRQ)) {
			dpp_err("dma%d error irq occur(0x%x)\n", dpp->id, irqs);
			dpp_dump(dpp);
			goto irq_end;
		}
		/*
		 * TODO: Normally, DMA framedone occurs before DPP framedone.
		 * But DMA framedone can occur in case of AFBC crop mode
		 */
		if (irqs & IDMA_STATUS_FRAMEDONE_IRQ) {
			DPU_EVENT_LOG(DPU_EVT_DMA_FRAMEDONE, &dpp->sd,
					ktime_set(0, 0));
#if defined(CONFIG_EXYNOS_SUPPORT_VOTF_IN)
			dma_write_mask(dpp->id, RDMA_VOTF_EN, 0, IDMA_VOTF_EN);
#endif
			goto irq_end;
		}
		if (irqs & IDMA_AFBC_CONFLICT_IRQ) {
			dpp_err("dma%d AFBC conflict irq occurs\n", dpp->id);
			goto irq_end;
		}
		if (irqs & IDMA_AXI_ADDR_ERR_IRQ) {
			dpp_err("dma%d AXI addr err irq occurs\n", dpp->id);
			goto irq_end;
		}
		if (irqs & IDMA_FBC_ERR_IRQ) {
			dpp_err("dma%d FBC err irq occurs\n", dpp->id);
			goto irq_end;
		}
	}

irq_end:
	spin_unlock(&dpp->dma_slock);
	return IRQ_HANDLED;
}

static int dpp_init_resources(struct dpp_device *dpp, struct platform_device *pdev)
{
	struct resource *res;
	int ret;
#if defined(CONFIG_EXYNOS_SUPPORT_VOTF_IN)
	struct dpp_device *dpp_temp = NULL;
#endif

	res = platform_get_resource(pdev, IORESOURCE_MEM, 0);
	if (!res) {
		dpp_err("failed to get mem resource\n");
		return -ENOENT;
	}
	dpp_info("dma res: start(0x%x), end(0x%x)\n",
			(u32)res->start, (u32)res->end);

	dpp->res.dma_regs = devm_ioremap_resource(dpp->dev, res);
	if (IS_ERR(dpp->res.dma_regs)) {
		dpp_err("failed to remap DPU_DMA SFR region\n");
		return -EINVAL;
	}

	/* DPP0 channel can only access common area of DPU_DMA */
	if (!IS_ENABLED(CONFIG_SOC_EXYNOS2100) && dpp->id == 0) {
		res = platform_get_resource(pdev, IORESOURCE_MEM, 2);
		if (!res) {
			dpp_err("failed to get mem resource\n");
			return -ENOENT;
		}
		dpp_info("dma common res: start(0x%x), end(0x%x)\n",
				(u32)res->start, (u32)res->end);

		dpp->res.dma_com_regs = devm_ioremap_resource(dpp->dev, res);
		if (IS_ERR(dpp->res.dma_com_regs)) {
			dpp_err("failed to remap DPU_DMA COMMON SFR region\n");
			return -EINVAL;
		}
	}

	res = platform_get_resource(pdev, IORESOURCE_IRQ, 0);
	if (!res) {
		dpp_err("failed to get dpu dma irq resource\n");
		return -ENOENT;
	}
	dpp_info("dma irq no = %lld\n", res->start);

	dpp->res.dma_irq = res->start;
	ret = devm_request_irq(dpp->dev, res->start, dma_irq_handler, 0,
			pdev->name, dpp);
	if (ret) {
		dpp_err("failed to install DPU DMA irq\n");
		return -EINVAL;
	}
	disable_irq(dpp->res.dma_irq);

	if (test_bit(DPP_ATTR_DPP, &dpp->attr)) {
		res = platform_get_resource(pdev, IORESOURCE_MEM, 1);
		if (!res) {
			dpp_err("failed to get mem resource\n");
			return -ENOENT;
		}
		dpp_info("res: start(0x%x), end(0x%x)\n",
				(u32)res->start, (u32)res->end);

		dpp->res.regs = devm_ioremap_resource(dpp->dev, res);
		if (IS_ERR(dpp->res.regs)) {
			dpp_err("failed to remap DPP SFR region\n");
			return -EINVAL;
		}
	}

	if (test_bit(DPP_ATTR_DPP, &dpp->attr) && !IS_WB(dpp->attr)) {
		res = platform_get_resource(pdev, IORESOURCE_IRQ, 1);
		if (!res) {
			dpp_err("failed to get dpp irq resource\n");
			return -ENOENT;
		}
		dpp_info("dpp irq no = %lld\n", res->start);

		dpp->res.irq = res->start;
		ret = devm_request_irq(dpp->dev, res->start, dpp_irq_handler, 0,
				pdev->name, dpp);
		if (ret) {
			dpp_err("failed to install DPP irq\n");
			return -EINVAL;
		}
		disable_irq(dpp->res.irq);
	}

#if defined(CONFIG_EXYNOS_SUPPORT_VOTF_IN)
	/* DPU C2SERV Global */
	if (IS_ENABLED(CONFIG_SOC_EXYNOS2100)) {
		if (dpp->id == DPUF0_FIRST_DPP_ID) { /* DPUF0 */
			res = platform_get_resource(pdev, IORESOURCE_MEM, 3);
			if (!res) {
				dpp_err("failed to get mem resource\n");
				return -ENOENT;
			}
			dpp_info("dpuf0 votf res: start(0x%x), end(0x%x)\n",
					(u32)res->start, (u32)res->end);

			dpp->res.votf_base_addr = (u32)res->start;
			dpp->res.votf_regs = devm_ioremap_resource(dpp->dev, res);
			if (IS_ERR(dpp->res.votf_regs)) {
				dpp_err("failed to remap dpuf0 votf SFR region\n");
				return -EINVAL;
			}
		}

		if (dpp->id > DPUF0_FIRST_DPP_ID && dpp->id < DPUF1_FIRST_DPP_ID) {
			dpp_temp = get_dpp_drvdata(DPUF0_FIRST_DPP_ID);
			if (!dpp_temp) {
				dpp_err("failed to get votf base address\n");
				return -ENOENT;
			}

			dpp->res.votf_base_addr = dpp_temp->res.votf_base_addr;
			dpp->res.votf_regs = dpp_temp->res.votf_regs;
		}

		if (dpp->id == DPUF1_FIRST_DPP_ID) { /* DPUF1 */
			res = platform_get_resource(pdev, IORESOURCE_MEM, 2);
			if (!res) {
				dpp_err("failed to get mem resource\n");
				return -ENOENT;
			}
			dpp_info("dpuf1 votf res: start(0x%x), end(0x%x)\n",
					(u32)res->start, (u32)res->end);

			dpp->res.votf_base_addr = (u32)res->start;
			dpp->res.votf_regs = devm_ioremap_resource(dpp->dev, res);
			if (IS_ERR(dpp->res.votf_regs)) {
				dpp_err("failed to remap dpuf1 votf SFR region\n");
				return -EINVAL;
			}
		}

		/* except WB case*/
		if (dpp->id > DPUF1_FIRST_DPP_ID && dpp->id < (MAX_DPP_CNT - 1)) {
			dpp_temp = get_dpp_drvdata(DPUF1_FIRST_DPP_ID);
			if (!dpp_temp) {
				dpp_err("failed to get votf base address\n");
				return -ENOENT;
			}

			dpp->res.votf_base_addr = dpp_temp->res.votf_base_addr;
			dpp->res.votf_regs = dpp_temp->res.votf_regs;
		}
	}
#endif

	return 0;
}

/*
 * If dpp becomes output device of DECON, it is responsible for clock, sysmmu
 * and power domain control like DSIM or DP device.
 */
static int dpp_set_output_device(struct dpp_device *dpp)
{
	if (!IS_WB(dpp->attr))
		return 0;

	dpp_dbg("%s +\n", __func__);
	dma_set_mask(dpp->dev, DMA_BIT_MASK(32));

#if !defined(CONFIG_EXYNOS_EMUL_DISP)
	dpp->res.aclk = devm_clk_get(dpp->dev, "aclk");
	if (IS_ERR_OR_NULL(dpp->res.aclk)) {
		dpp_err("failed to get aclk\n");
		return PTR_ERR(dpp->res.aclk);
	}
#endif

	pm_runtime_enable(dpp->dev);
	iommu_register_device_fault_handler(dpp->dev, dpu_sysmmu_fault_handler_wb, NULL);

	dpp->wb_state = WBMUX_STATE_OFF;
	dpp_dbg("%s -\n", __func__);

	return 0;
}

static int dpp_probe(struct platform_device *pdev)
{
	struct device *dev = &pdev->dev;
	struct dpp_device *dpp;
	int ret = 0;

	dpp = devm_kzalloc(dev, sizeof(*dpp), GFP_KERNEL);
	if (!dpp) {
		dpp_err("failed to allocate dpp device.\n");
		ret = -ENOMEM;
		goto err;
	}

	dpp_parse_dt(dpp, dev);
	dpp_drvdata[dpp->id] = dpp;

	spin_lock_init(&dpp->slock);
	spin_lock_init(&dpp->dma_slock);
	mutex_init(&dpp->lock);
	init_waitqueue_head(&dpp->framedone_wq);

	ret = dpp_init_resources(dpp, pdev);
	if (ret)
		goto err_clk;

	dpp_init_subdev(dpp);
	platform_set_drvdata(pdev, dpp);

	/* dpp becomes output device of connected DECON in case of writeback */
	ret = dpp_set_output_device(dpp);
	if (ret)
		goto err_clk;

#if IS_ENABLED(CONFIG_MCD_PANEL)
	ret = dpp_get_mcdhdr_info(dpp);
	if (ret) {
		dpp_err("failed to get mcdhdr info\n");
		goto err_clk;
	}
#endif

#if defined(SYSFS_UNITTEST_INTERFACE)
	dpp_create_irq_err_sysfs(dpp);
	dma_create_irq_err_sysfs(dpp);
#endif
	dpp->state = DPP_STATE_OFF;
	dpp_info("dpp%d is probed successfully\n", dpp->id);

	return 0;

err_clk:
	kfree(dpp);
err:
	return ret;
}

static int dpp_remove(struct platform_device *pdev)
{
	dpp_info("%s driver unloaded\n", pdev->name);
	return 0;
}

static int dpp_runtime_suspend(struct device *dev)
{
	struct dpp_device *dpp = dev_get_drvdata(dev);

	dpp_dbg("%s +\n", __func__);
	clk_disable_unprepare(dpp->res.aclk);
	dpp_dbg("%s -\n", __func__);

	return 0;
}

static int dpp_runtime_resume(struct device *dev)
{
	struct dpp_device *dpp = dev_get_drvdata(dev);

	dpp_dbg("%s +\n", __func__);
	clk_prepare_enable(dpp->res.aclk);
	dpp_dbg("%s -\n", __func__);

	return 0;
}

static const struct of_device_id dpp_of_match[] = {
	{ .compatible = "samsung,exynos9-dpp" },
	{},
};
MODULE_DEVICE_TABLE(of, dpp_of_match);

static const struct dev_pm_ops dpp_pm_ops = {
	.runtime_suspend	= dpp_runtime_suspend,
	.runtime_resume		= dpp_runtime_resume,
};

struct platform_driver dpp_driver __refdata = {
	.probe		= dpp_probe,
	.remove		= dpp_remove,
	.driver = {
		.name	= DPP_MODULE_NAME,
		.owner	= THIS_MODULE,
		.pm	= &dpp_pm_ops,
		.of_match_table = of_match_ptr(dpp_of_match),
		.suppress_bind_attrs = true,
	}
};

MODULE_AUTHOR("Jaehoe Yang <jaehoe.yang@samsung.com>");
MODULE_AUTHOR("Minho Kim <m8891.kim@samsung.com>");
MODULE_DESCRIPTION("Samsung EXYNOS DPP driver");
MODULE_LICENSE("GPL");
