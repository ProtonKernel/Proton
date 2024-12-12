// SPDX-License-Identifier: GPL-2.0
/*
 * Samsung Exynos SoC series Pablo driver
 *
 * Copyright (c) 2020 Samsung Electronics Co., Ltd
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#include "is-votf-id-table.h"
#include "votf/camerapp-votf.h"
#include "is-debug.h"

unsigned int is_votf_ip[GROUP_ID_MAX] = {
	[GROUP_ID_3AA0 ... GROUP_ID_MAX-1] = 0xFFFFFFFF,

	[GROUP_ID_SS0]	= VOTF_CSIS0,
	[GROUP_ID_SS1]	= VOTF_CSIS0,
	[GROUP_ID_SS2]	= VOTF_CSIS1,
	[GROUP_ID_SS3]	= VOTF_CSIS1,

	[GROUP_ID_PAF0]	= VOTF_PDP,
	[GROUP_ID_PAF1]	= VOTF_PDP,
	[GROUP_ID_PAF2]	= VOTF_PDP,

	[GROUP_ID_ISP0]	= VOTF_REPRO1_IP,
	[GROUP_ID_YPP]	= VOTF_YUVPP_IP,
};

unsigned int is_votf_id[GROUP_ID_MAX][ENTRY_END] = {
	[GROUP_ID_3AA0 ... GROUP_ID_MAX-1][ENTRY_SENSOR ... ENTRY_END-1] = 0xFFFFFFFF,

	[GROUP_ID_SS0][ENTRY_SSVC0]	= 8,
	[GROUP_ID_SS0][ENTRY_SSVC1]	= 9,
	[GROUP_ID_SS1][ENTRY_SSVC0]	= 12,
	[GROUP_ID_SS1][ENTRY_SSVC1]	= 13,
	[GROUP_ID_SS2][ENTRY_SSVC0]	= 0,
	[GROUP_ID_SS2][ENTRY_SSVC1]	= 1,
	[GROUP_ID_SS3][ENTRY_SSVC0]	= 4,
	[GROUP_ID_SS3][ENTRY_SSVC1]	= 5,

	[GROUP_ID_PAF0][ENTRY_PAF]	= 0,
	[GROUP_ID_PAF0][ENTRY_PDAF]	= 3,
	[GROUP_ID_PAF1][ENTRY_PAF]	= 1,
	[GROUP_ID_PAF1][ENTRY_PDAF]	= 4,
	[GROUP_ID_PAF2][ENTRY_PAF]	= 2,
	[GROUP_ID_PAF2][ENTRY_PDAF]	= 5,

	[GROUP_ID_ISP0][ENTRY_ISP]	= 0,
	[GROUP_ID_YPP][ENTRY_YPP]	= 0,
};

static int is_votf_set_service_cfg(struct votf_info *src_info,
		struct votf_info *dst_info,
		u32 width, u32 height)
{
	int ret = 0;
	struct votf_service_cfg cfg;

	memset(&cfg, 0, sizeof(struct votf_service_cfg));

	/* TRS: Slave */
	dst_info->service = TRS;

	cfg.enable = 0x1;
	/*
	 * 0xFF is max value.
	 * Buffer size is (limit x token_size).
	 * But VOTF can hold only 1 frame.
	 */
	cfg.limit = 0xFF;
	cfg.width = width;
	cfg.height = height;
	cfg.token_size = is_votf_get_token_size(dst_info);
	cfg.connected_ip = src_info->ip;
	cfg.connected_id = src_info->id;
	cfg.option = VOTF_OPTION_MSK_CHANGE;

	ret = votfitf_set_service_cfg(dst_info, &cfg);
	if (ret <= 0) {
		ret = -EINVAL;
		err("TRS votf set_service_cfg fail. TWS 0x%04X-%d TRS 0x%04X-%d",
				src_info->ip, src_info->id,
				dst_info->ip, dst_info->id);
		return ret;
	}

	return 0;
}

u32 is_votf_get_token_size(struct votf_info *vinfo)
{
	u32 lines_in_token;

	switch (vinfo->ip) {
	case VOTF_CSIS0:
	case VOTF_CSIS1:
	case VOTF_PDP:
		if (vinfo->mode == VOTF_FRS) {
			lines_in_token = 40;
			break;
		} else if (vinfo->mode == VOTF_TRS_HEIGHT_X2) {
			if (vinfo->service == TRS) {
				lines_in_token = 2;
				break;
			}
		}

		fallthrough;
	default:
		lines_in_token = 1;
		break;
	};

	return lines_in_token;
}

int is_hw_pdp_set_votf_config(struct is_group *group, struct is_sensor_cfg *s_cfg)
{
	int ret = 0;
	struct is_device_sensor *sensor;
	struct votf_info src_info, dst_info;
	struct is_group *src_gr;
	unsigned int src_gr_id;
	struct is_subdev *src_sd;
	int dma_ch;
	int pd_mode;
	u32 width, height, pd_width, pd_height;
	u32 votf_mode = VOTF_NORMAL;
	u32 option = VOTF_OPTION_MSK_COUNT | VOTF_OPTION_MSK_CHANGE;
	src_gr = group->prev;
	src_sd = group->prev->junction;

	/*
	 * The sensor group id should be re calculated,
	 * because CSIS WDMA is not matched with sensor group id.
	 */
	sensor = src_gr->sensor;
	ret = v4l2_subdev_call(sensor->subdev_csi, core, ioctl,
				SENSOR_IOCTL_G_DMA_CH, &dma_ch);
	if (ret) {
		mgerr("failed to get dma_ch", group, group);
		return ret;
	}

	src_gr_id = GROUP_ID_SS0 + dma_ch;

	width = s_cfg->input[CSI_VIRTUAL_CH_0].width;
	height = s_cfg->input[CSI_VIRTUAL_CH_0].height;

	if (s_cfg->ex_mode == EX_DUALFPS_480 ||
		s_cfg->ex_mode == EX_DUALFPS_960) {
		votf_mode = VOTF_FRS;
	} else {
		if (s_cfg->img_pd_ratio == IS_IMG_PD_RATIO_1_1) {
			/*
			 * PDP constraint:
			 * In case of (image_h : pd_h == 1 : 1),
			 * the height of TRS must be twice of the height of TWS.
			 */
			height |= (height * 2) << 16;
			votf_mode = VOTF_TRS_HEIGHT_X2;
		}
	}

	src_info.ip = is_votf_ip[src_gr_id];
	src_info.id = is_votf_id[src_gr_id][src_sd->id];
	src_info.mode = votf_mode;

	dst_info.ip = is_votf_ip[group->id];
	dst_info.id = is_votf_id[group->id][group->leader.id];
	dst_info.mode = votf_mode;

	ret = is_votf_link_set_service_cfg(&src_info, &dst_info,
			width, height, option);
	if (ret)
		return ret;

	pd_mode = s_cfg->pd_mode;

	if (pd_mode == PD_MOD3 || pd_mode == PD_MSPD_TAIL) {
		votf_mode = VOTF_NORMAL;

		src_info.id = is_votf_id[src_gr_id][ENTRY_SSVC1];
		src_info.mode = votf_mode;
		dst_info.id = is_votf_id[group->id][ENTRY_PDAF];
		dst_info.mode = votf_mode;

		pd_width = s_cfg->input[CSI_VIRTUAL_CH_1].width;
		pd_height = s_cfg->input[CSI_VIRTUAL_CH_1].height;

		ret = is_votf_link_set_service_cfg(&src_info, &dst_info,
				pd_width, pd_height, option);
		if (ret)
			return ret;
	}

	/* for force flush */
	src_info.id = is_votf_id[src_gr_id][src_sd->id];
	dst_info.id = is_votf_id[group->id][group->leader.id];

	__is_votf_force_flush(&src_info, &dst_info);

	if (pd_mode == PD_MOD3 || pd_mode == PD_MSPD_TAIL) {
		src_info.id = is_votf_id[src_gr_id][ENTRY_SSVC1];
		dst_info.id = is_votf_id[group->id][ENTRY_PDAF];

		__is_votf_force_flush(&src_info, &dst_info);
	}

	return 0;
}

int is_hw_ypp_set_votf_size_config(u32 width, u32 height)
{
#if defined(ENABLE_LOGICAL_VIDEO_NODE)
	u32 link_width, link_height;
#endif
	struct votf_info src_info, dst_info;
	int ret = 0;

	/*********** L0 Y ***********/
	src_info.ip = VOTF_REPRO1_IP;
	src_info.id = 0;

	dst_info.ip = VOTF_YUVPP_IP;
	dst_info.id = 0;

	ret = is_votf_set_service_cfg(&src_info, &dst_info, width, height);
	if (ret < 0) {
		err("L0_Y votf set_service_cfg fail. TWS 0x%04X-%d TRS 0x%04X-%d",
				src_info.ip, src_info.id,
				dst_info.ip, dst_info.id);

		return ret;
	}

	/*********** L0 UV ***********/
	src_info.ip = VOTF_DNS_IP;
	src_info.id = ISP_L0_UV;

	dst_info.ip = VOTF_YUVPP_IP;
	dst_info.id = YUVPP_YUVNR_L0_UV;

	ret = is_votf_set_service_cfg(&src_info, &dst_info, width, height);
	if (ret < 0) {
		err("L0_UV votf set_service_cfg fail. TWS 0x%04X-%d TRS 0x%04X-%d",
				src_info.ip, src_info.id,
				dst_info.ip, dst_info.id);

		return ret;
	}

#if defined(ENABLE_LOGICAL_VIDEO_NODE)
	if (IS_ENABLED(YPP_DDK_LIB_CALL)) {
		/*********** L2 Y/UV ***********/
		src_info.id = ISP_L2_Y;
		dst_info.id = YUVPP_YUVNR_L2_Y;

		link_width = ((width + 2) >> 2) + 1;
		link_height = ((height + 2) >> 2) + 1;

		ret = is_votf_set_service_cfg(&src_info, &dst_info,
				link_width, link_height);
		if (ret < 0) {
			err("L2_Y votf set_service_cfg fail. TWS 0x%04X-%d TRS 0x%04X-%d",
					src_info.ip, src_info.id,
					dst_info.ip, dst_info.id);
			return ret;
		}

		src_info.id = ISP_L2_UV;
		dst_info.id = YUVPP_YUVNR_L2_UV;

		if ((width % 8) == 0)
			link_width = (((width + 4) >> 3) + 1) << 1;
		else
			link_width = ((width + 4) >> 3) << 1;

		if ((height % 8) == 0 || (height % 8) == 6)
			link_height = ((height + 7) >> 3) + 1;
		else
			link_height = (height + 7) >> 3;

		ret = is_votf_set_service_cfg(&src_info, &dst_info,
				link_width, link_height);
		if (ret < 0) {
			err("L2_UV votf set_service_cfg fail. TWS 0x%04X-%d TRS 0x%04X-%d",
					src_info.ip, src_info.id,
					dst_info.ip, dst_info.id);
			return ret;
		}

		/*********** NOISE MAP ***********/
		src_info.id = ISP_NOISE;
		dst_info.id = YUVPP_NOISE;

		link_width = width;
		link_height = height;

		ret = is_votf_set_service_cfg(&src_info, &dst_info,
				link_width, link_height);
		if (ret < 0) {
			err("NOISE votf set_service_cfg fail. TWS 0x%04X-%d TRS 0x%04X-%d",
					src_info.ip, src_info.id,
					dst_info.ip, dst_info.id);
			return ret;
		}

		/*********** DRC GAIN ***********/
		src_info.id = ISP_DRC_GAIN;
		dst_info.id = YUVPP_YUVNR_DRC_GAIN;

		link_width = ((width + 2) >> 2) + 1;
		link_height = ((height + 2) >> 2) + 1;

		ret = is_votf_set_service_cfg(&src_info, &dst_info,
				link_width, link_height);
		if (ret < 0) {
			err("DRC_GAIN votf set_service_cfg fail. TWS 0x%04X-%d TRS 0x%04X-%d",
					src_info.ip, src_info.id,
					dst_info.ip, dst_info.id);
			return ret;
		}

		/*********** SVHIST ***********/
		src_info.id = ISP_HIST;
		dst_info.id = YUVPP_CLAHE_HIST;

		link_width = HIST_WIDTH;
		link_height = HIST_HEIGHT;

		ret = is_votf_set_service_cfg(&src_info, &dst_info,
				link_width, link_height);
		if (ret < 0) {
			err("SVHIST votf set_service_cfg fail. TWS 0x%04X-%d TRS 0x%04X-%d",
					src_info.ip, src_info.id,
					dst_info.ip, dst_info.id);
			return ret;
		}
	}
#endif
	return 0;
}

int is_hw_ypp_reset_votf(void)
{
	struct votf_info src_info;
	int ret;

	src_info.service = TWS;
	/*********** L0 Y ***********/
	src_info.ip = VOTF_REPRO1_IP;
	src_info.id = 0;

	ret = votfitf_reset(&src_info, SW_RESET);
	if (ret) {
		err("L0_Y votf votfitf_reset fail. TWS 0x%04X-%d",
				src_info.ip, src_info.id);
		return ret;
	}

	/*********** L0 UV ***********/
	src_info.ip = VOTF_DNS_IP;
	src_info.id = ISP_L0_UV;

	ret = votfitf_reset(&src_info, SW_RESET);
	if (ret) {
		err("L0_UV votf votfitf_reset fail. TWS 0x%04X-%d",
				src_info.ip, src_info.id);
		return ret;
	}

#if defined(ENABLE_LOGICAL_VIDEO_NODE)
	if (IS_ENABLED(YPP_DDK_LIB_CALL)) {
		/*********** L2 Y/UV ***********/
		src_info.id = ISP_L2_Y;

		ret = votfitf_reset(&src_info, SW_RESET);
		if (ret) {
			err("L2_Y votf votfitf_reset fail. TWS 0x%04X-%d",
					src_info.ip, src_info.id);
			return ret;
		}

		src_info.id = ISP_L2_UV;

		ret = votfitf_reset(&src_info, SW_RESET);
		if (ret) {
			err("L2_UV votf votfitf_reset fail. TWS 0x%04X-%d",
					src_info.ip, src_info.id);
			return ret;
		}

		/*********** NOISE MAP ***********/
		src_info.id = ISP_NOISE;

		ret = votfitf_reset(&src_info, SW_RESET);
		if (ret) {
			err("NOISE votf votfitf_reset fail. TWS 0x%04X-%d",
					src_info.ip, src_info.id);
			return ret;
		}

		/*********** DRC GAIN ***********/
		src_info.id = ISP_DRC_GAIN;

		ret = votfitf_reset(&src_info, SW_RESET);
		if (ret) {
			err("DRC_GAIN votf votfitf_reset fail. TWS 0x%04X-%d",
					src_info.ip, src_info.id);
			return ret;
		}

		/*********** SVHIST ***********/
		src_info.id = ISP_HIST;

		ret = votfitf_reset(&src_info, SW_RESET);
		if (ret) {
			err("SVHIST votf votfitf_reset fail. TWS 0x%04X-%d",
					src_info.ip, src_info.id);
			return ret;
		}
	}
#endif
	return 0;
}

void is_votf_subdev_flush(struct is_group *group)
{
	struct is_group *src_gr;
	struct votf_info src_info, dst_info;
	int ret;

	src_gr = group->prev;
	if (src_gr->id == GROUP_ID_ISP0 && group->id == GROUP_ID_YPP) {
		/*********** L0 UV ***********/
		src_info.ip = VOTF_DNS_IP;
		src_info.id = ISP_L0_UV;

		dst_info.ip = VOTF_YUVPP_IP;
		dst_info.id = YUVPP_YUVNR_L0_UV;

		ret = __is_votf_flush(&src_info, &dst_info);
		if (ret)
			mgerr("L0_UV votf flush fail. ret %d",
				group, group, ret);

#if defined(ENABLE_LOGICAL_VIDEO_NODE)
		if (IS_ENABLED(YPP_DDK_LIB_CALL)) {
			/*********** L2 Y/UV ***********/
			src_info.id = ISP_L2_Y;
			dst_info.id = YUVPP_YUVNR_L2_Y;

			ret = __is_votf_flush(&src_info, &dst_info);
			if (ret)
				mgerr("L2_Y votf flush fail. ret %d",
					group, group, ret);

			src_info.id = ISP_L2_UV;
			dst_info.id = YUVPP_YUVNR_L2_UV;

			ret = __is_votf_flush(&src_info, &dst_info);
			if (ret)
				mgerr("L2_UV votf flush fail. ret %d",
					group, group, ret);

			/*********** NOISE MAP ***********/
			src_info.id = ISP_NOISE;
			dst_info.id = YUVPP_NOISE;

			ret = __is_votf_flush(&src_info, &dst_info);
			if (ret)
				mgerr("NOISE votf flush fail. ret %d",
					group, group, ret);

			/*********** DRC GAIN ***********/
			src_info.id = ISP_DRC_GAIN;
			dst_info.id = YUVPP_YUVNR_DRC_GAIN;

			ret = __is_votf_flush(&src_info, &dst_info);
			if (ret)
				mgerr("DRC_GAIN votf flush fail. ret %d",
					group, group, ret);

			/*********** SVHIST ***********/
			src_info.id = ISP_HIST;
			dst_info.id = YUVPP_CLAHE_HIST;

			ret = __is_votf_flush(&src_info, &dst_info);
			if (ret)
				mgerr("SVHIST votf flush fail. ret %d",
					group, group, ret);
		}
#endif
	}
}

int is_votf_subdev_create_link(struct is_group *group)
{
	struct is_group *src_gr;
	struct votf_info src_info, dst_info;
	u32 width, height;
	u32 change_option = VOTF_OPTION_MSK_COUNT;
	int ret;

	src_gr = group->prev;
	/* size will set in shot routine */
	width = height = 0;

	if (src_gr->id == GROUP_ID_ISP0 && group->id == GROUP_ID_YPP) {
		/*********** L0 UV ***********/
		src_info.ip = VOTF_DNS_IP;
		src_info.id = ISP_L0_UV;
		src_info.mode = VOTF_NORMAL;

		dst_info.ip = VOTF_YUVPP_IP;
		dst_info.id = YUVPP_YUVNR_L0_UV;
		dst_info.mode = VOTF_NORMAL;

#if defined(USE_VOTF_AXI_APB)
		votfitf_create_link(src_info.ip, dst_info.ip);
#endif
		ret = is_votf_link_set_service_cfg(&src_info, &dst_info,
				width, height, change_option);
		if (ret)
			return ret;

#if defined(ENABLE_LOGICAL_VIDEO_NODE)
		if (IS_ENABLED(YPP_DDK_LIB_CALL)) {
			/*********** L2 Y/UV ***********/
			src_info.id = ISP_L2_Y;
			dst_info.id = YUVPP_YUVNR_L2_Y;

			ret = is_votf_link_set_service_cfg(&src_info, &dst_info,
					width, height, change_option);
			if (ret)
				return ret;

			src_info.id = ISP_L2_UV;
			dst_info.id = YUVPP_YUVNR_L2_UV;

			ret = is_votf_link_set_service_cfg(&src_info, &dst_info,
					width, height, change_option);
			if (ret)
				return ret;

			/*********** NOISE MAP ***********/
			src_info.id = ISP_NOISE;
			dst_info.id = YUVPP_NOISE;

			ret = is_votf_link_set_service_cfg(&src_info, &dst_info,
					width, height, change_option);
			if (ret)
				return ret;

			/*********** DRC GAIN ***********/
			src_info.id = ISP_DRC_GAIN;
			dst_info.id = YUVPP_YUVNR_DRC_GAIN;

			ret = is_votf_link_set_service_cfg(&src_info, &dst_info,
					width, height, change_option);
			if (ret)
				return ret;

			/*********** SVHIST ***********/
			src_info.id = ISP_HIST;
			dst_info.id = YUVPP_CLAHE_HIST;

			ret = is_votf_link_set_service_cfg(&src_info, &dst_info,
					width, height, change_option);
			if (ret)
				return ret;
		}
#endif
	}

	return 0;
}

#if defined(USE_VOTF_AXI_APB)
void is_votf_subdev_destroy_link(struct is_group *group)
{
	struct is_group *src_gr;

	src_gr = group->prev;
	if (src_gr && src_gr->id == GROUP_ID_ISP0 && group->id == GROUP_ID_YPP)
		votfitf_destroy_link(VOTF_DNS_IP, VOTF_YUVPP_IP);
}
#endif
