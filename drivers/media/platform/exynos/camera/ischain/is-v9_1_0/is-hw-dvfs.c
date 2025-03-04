/*
 * Samsung Exynos SoC series Pablo driver
 *
 * Exynos Pablo DVFS v2 functions
 *
 * Copyright (c) 2020 Samsung Electronics Co., Ltd
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#include <dt-bindings/camera/exynos_is_dt.h>

#include "is-core.h"
#include "is-dvfs.h"
#include "is-hw-dvfs.h"
#include "is-param.h"

static int scenario_idx[IS_DVFS_SN_END];

static char str_face[IS_DVFS_FACE_END + 1][IS_DVFS_STR_LEN] = {
	"REAR",
	"FRONT",
	"PIP",
	"INVALID",
};

static char str_num[IS_DVFS_NUM_END + 1][IS_DVFS_STR_LEN] = {
	"SINGLE",
	"DUAL",
	"TRIPLE",
	"INVALID",
};

static char str_sensor[IS_DVFS_SENSOR_END + 1][IS_DVFS_STR_LEN] = {
	"WIDE",
	"WIDE_FASTAE",
	"WIDE_REMOSAIC",
	"WIDE_VIDEOHDR",
	"WIDE_SSM",
	"TELE",
	"TELE_REMOSAIC",
	"ULTRAWIDE",
	"ULTRAWIDE_SSM",
	"MACRO",
	"WIDE_TELE",
	"WIDE_ULTRAWIDE",
	"WIDE_MACRO",
	"FRONT",
	"FRONT_FASTAE",
	"FRONT_SECURE",
	"FRONT_VT",
	"PIP_COMMON",
	"TRIPLE_COMMON",
	"INVALID",
};

static char str_mode[IS_DVFS_MODE_END + 1][IS_DVFS_STR_LEN] = {
	"PHOTO",
	"CAPTURE",
	"VIDEO",
	"INVALID",
};

static char str_resol[IS_DVFS_RESOL_END + 1][IS_DVFS_STR_LEN] = {
	"FHD",
	"UHD",
	"8K",
	"FULL",
	"INVALID",
};

static char str_fps[IS_DVFS_FPS_END + 1][IS_DVFS_STR_LEN] = {
	"24",
	"30",
	"60",
	"120",
	"240",
	"480",
	"INVALID",
};

/* for DT parsing */
DECLARE_DVFS_DT(IS_DVFS_SN_END,
	{
		.parse_scenario_nm	= "default_",
		.scenario_id		= IS_DVFS_SN_DEFAULT,
		.keep_frame_tick	= -1,
	},
	/* wide sensor scenarios */
	{
		.parse_scenario_nm	= "rear_single_wide_photo_",
		.scenario_id		= IS_DVFS_SN_REAR_SINGLE_WIDE_PHOTO,
		.keep_frame_tick	= -1,
	}, {
		.parse_scenario_nm	= "rear_single_wide_photo_full_",
		.scenario_id		= IS_DVFS_SN_REAR_SINGLE_WIDE_PHOTO_FULL,
		.keep_frame_tick	= -1,
	}, {
		.parse_scenario_nm	= "rear_single_wide_capture_",
		.scenario_id		= IS_DVFS_SN_REAR_SINGLE_WIDE_CAPTURE,
		.keep_frame_tick	= IS_DVFS_CAPTURE_TICK,
	}, {
		.parse_scenario_nm	= "rear_single_wide_video_fhd30_",
		.scenario_id		= IS_DVFS_SN_REAR_SINGLE_WIDE_VIDEO_FHD30,
		.keep_frame_tick	= -1,
	}, {
		.parse_scenario_nm	= "rear_single_wide_video_fhd60_",
		.scenario_id		= IS_DVFS_SN_REAR_SINGLE_WIDE_VIDEO_FHD60,
		.keep_frame_tick	= -1,
	}, {
		.parse_scenario_nm	= "rear_single_wide_video_fhd60_hf_",
		.scenario_id		= IS_DVFS_SN_REAR_SINGLE_WIDE_VIDEO_FHD60_HF,
		.keep_frame_tick	= -1,
	}, {
		.parse_scenario_nm	= "rear_single_wide_video_fhd60_supersteady_",
		.scenario_id		= IS_DVFS_SN_REAR_SINGLE_WIDE_VIDEO_FHD60_SUPERSTEADY,
		.keep_frame_tick	= -1,
	}, {
		.parse_scenario_nm	= "rear_single_wide_video_fhd60_hf_supersteady_",
		.scenario_id		= IS_DVFS_SN_REAR_SINGLE_WIDE_VIDEO_FHD60_HF_SUPERSTEADY,
		.keep_frame_tick	= -1,
	}, {
		.parse_scenario_nm	= "rear_single_wide_video_fhd120_",
		.scenario_id		= IS_DVFS_SN_REAR_SINGLE_WIDE_VIDEO_FHD120,
		.keep_frame_tick	= -1,
	}, {
		.parse_scenario_nm	= "rear_single_wide_video_fhd240_",
		.scenario_id		= IS_DVFS_SN_REAR_SINGLE_WIDE_VIDEO_FHD240,
		.keep_frame_tick	= -1,
	}, {
		.parse_scenario_nm	= "rear_single_wide_video_fhd480_",
		.scenario_id		= IS_DVFS_SN_REAR_SINGLE_WIDE_VIDEO_FHD480,
		.keep_frame_tick	= -1,
	}, {
		.parse_scenario_nm	= "rear_single_wide_video_uhd30_",
		.scenario_id		= IS_DVFS_SN_REAR_SINGLE_WIDE_VIDEO_UHD30,
		.keep_frame_tick	= -1,
	}, {
		.parse_scenario_nm	= "rear_single_wide_video_uhd60_",
		.scenario_id		= IS_DVFS_SN_REAR_SINGLE_WIDE_VIDEO_UHD60,
		.keep_frame_tick	= -1,
	}, {
		.parse_scenario_nm	= "rear_single_wide_video_uhd60_hf_",
		.scenario_id		= IS_DVFS_SN_REAR_SINGLE_WIDE_VIDEO_UHD60_HF,
		.keep_frame_tick	= -1,
	}, {
		.parse_scenario_nm	= "rear_single_wide_video_uhd120_",
		.scenario_id		= IS_DVFS_SN_REAR_SINGLE_WIDE_VIDEO_UHD120,
		.keep_frame_tick	= -1,
	}, {
		.parse_scenario_nm	= "rear_single_wide_video_8k24_",
		.scenario_id		= IS_DVFS_SN_REAR_SINGLE_WIDE_VIDEO_8K24,
		.keep_frame_tick	= -1,
	}, {
		.parse_scenario_nm	= "rear_single_wide_video_8k24_hf_",
		.scenario_id		= IS_DVFS_SN_REAR_SINGLE_WIDE_VIDEO_8K24_HF,
		.keep_frame_tick	= -1,
	}, {
		.parse_scenario_nm	= "rear_single_wide_video_8k30_",
		.scenario_id		= IS_DVFS_SN_REAR_SINGLE_WIDE_VIDEO_8K30,
		.keep_frame_tick	= -1,
	}, {
		.parse_scenario_nm	= "rear_single_wide_video_8k30_hf_",
		.scenario_id		= IS_DVFS_SN_REAR_SINGLE_WIDE_VIDEO_8K30_HF,
		.keep_frame_tick	= -1,
	}, {
		.parse_scenario_nm	= "rear_single_wide_remosaic_photo_",
		.scenario_id		= IS_DVFS_SN_REAR_SINGLE_WIDE_REMOSAIC_PHOTO,
		.keep_frame_tick	= -1,
	}, {
		.parse_scenario_nm	= "rear_single_wide_remosaic_capture_",
		.scenario_id		= IS_DVFS_SN_REAR_SINGLE_WIDE_REMOSAIC_CAPTURE,
		.keep_frame_tick	= IS_DVFS_CAPTURE_TICK,
	}, {
		.parse_scenario_nm	= "rear_single_wide_fastae_",
		.scenario_id		= IS_DVFS_SN_REAR_SINGLE_WIDE_FASTAE,
		.keep_frame_tick	= -1,
	}, {
		.parse_scenario_nm	= "rear_single_wide_videohdr_",
		.scenario_id		= IS_DVFS_SN_REAR_SINGLE_WIDE_VIDEOHDR,
		.keep_frame_tick	= -1,
	}, {
		.parse_scenario_nm	= "rear_single_wide_ssm_",
		.scenario_id		= IS_DVFS_SN_REAR_SINGLE_WIDE_SSM,
		.keep_frame_tick	= -1,
	}, {
		.parse_scenario_nm	= "rear_single_wide_vt_",
		.scenario_id		= IS_DVFS_SN_REAR_SINGLE_WIDE_VT,
		.keep_frame_tick	= -1,
	},
	/* tele sensor scenarios */
	{
		.parse_scenario_nm	= "rear_single_tele_photo_",
		.scenario_id		= IS_DVFS_SN_REAR_SINGLE_TELE_PHOTO,
		.keep_frame_tick	= -1,
	}, {
		.parse_scenario_nm	= "rear_single_tele_photo_full_",
		.scenario_id		= IS_DVFS_SN_REAR_SINGLE_TELE_PHOTO_FULL,
		.keep_frame_tick	= -1,
	}, {
		.parse_scenario_nm	= "rear_single_tele_capture_",
		.scenario_id		= IS_DVFS_SN_REAR_SINGLE_TELE_CAPTURE,
		.keep_frame_tick	= IS_DVFS_CAPTURE_TICK,
	}, {
		.parse_scenario_nm	= "rear_single_tele_video_fhd30_",
		.scenario_id		= IS_DVFS_SN_REAR_SINGLE_TELE_VIDEO_FHD30,
		.keep_frame_tick	= -1,
	}, {
		.parse_scenario_nm	= "rear_single_tele_video_fhd60_",
		.scenario_id		= IS_DVFS_SN_REAR_SINGLE_TELE_VIDEO_FHD60,
		.keep_frame_tick	= -1,
	}, {
		.parse_scenario_nm	= "rear_single_tele_video_uhd30_",
		.scenario_id		= IS_DVFS_SN_REAR_SINGLE_TELE_VIDEO_UHD30,
		.keep_frame_tick	= -1,
	}, {
		.parse_scenario_nm	= "rear_single_tele_video_uhd60_",
		.scenario_id		= IS_DVFS_SN_REAR_SINGLE_TELE_VIDEO_UHD60,
		.keep_frame_tick	= -1,
	}, {
		.parse_scenario_nm	= "rear_single_tele_video_8k24_",
		.scenario_id		= IS_DVFS_SN_REAR_SINGLE_TELE_VIDEO_8K24,
		.keep_frame_tick	= -1,
	}, {
		.parse_scenario_nm	= "rear_single_tele_video_8k24_hf_",
		.scenario_id		= IS_DVFS_SN_REAR_SINGLE_TELE_VIDEO_8K24_HF,
		.keep_frame_tick	= -1,
	}, {
		.parse_scenario_nm	= "rear_single_tele_video_8k30_",
		.scenario_id		= IS_DVFS_SN_REAR_SINGLE_TELE_VIDEO_8K30,
		.keep_frame_tick	= -1,
	}, {
		.parse_scenario_nm	= "rear_single_tele_video_8k30_hf_",
		.scenario_id		= IS_DVFS_SN_REAR_SINGLE_TELE_VIDEO_8K30_HF,
		.keep_frame_tick	= -1,
	}, {
		.parse_scenario_nm	= "rear_single_tele_video_8k_thermal_",
		.scenario_id		= IS_DVFS_SN_REAR_SINGLE_TELE_VIDEO_8K_THERMAL,
		.keep_frame_tick	= -1,
	}, {
		.parse_scenario_nm	= "rear_single_tele_remosaic_photo_",
		.scenario_id		= IS_DVFS_SN_REAR_SINGLE_TELE_REMOSAIC_PHOTO,
		.keep_frame_tick	= -1,
	}, {
		.parse_scenario_nm	= "rear_single_tele_remosaic_capture_",
		.scenario_id		= IS_DVFS_SN_REAR_SINGLE_TELE_REMOSAIC_CAPTURE,
		.keep_frame_tick	= IS_DVFS_CAPTURE_TICK,
	},
	/* ultra wide sensor scenarios */
	{
		.parse_scenario_nm	= "rear_single_ultrawide_photo_",
		.scenario_id		= IS_DVFS_SN_REAR_SINGLE_ULTRAWIDE_PHOTO,
		.keep_frame_tick	= -1,
	}, {
		.parse_scenario_nm	= "rear_single_ultrawide_photo_full_",
		.scenario_id		= IS_DVFS_SN_REAR_SINGLE_ULTRAWIDE_PHOTO_FULL,
		.keep_frame_tick	= -1,
	}, {
		.parse_scenario_nm	= "rear_single_ultrawide_capture_",
		.scenario_id		= IS_DVFS_SN_REAR_SINGLE_ULTRAWIDE_CAPTURE,
		.keep_frame_tick	= IS_DVFS_CAPTURE_TICK,
	}, {
		.parse_scenario_nm	= "rear_single_ultrawide_video_fhd30_",
		.scenario_id		= IS_DVFS_SN_REAR_SINGLE_ULTRAWIDE_VIDEO_FHD30,
		.keep_frame_tick	= -1,
	}, {
		.parse_scenario_nm	= "rear_single_ultrawide_video_fhd30_supersteady_",
		.scenario_id		= IS_DVFS_SN_REAR_SINGLE_ULTRAWIDE_VIDEO_FHD30_SUPERSTEADY,
		.keep_frame_tick	= -1,
	}, {
		.parse_scenario_nm	= "rear_single_ultrawide_video_fhd30_hf_supersteady_",
		.scenario_id		= IS_DVFS_SN_REAR_SINGLE_ULTRAWIDE_VIDEO_FHD30_HF_SUPERSTEADY,
		.keep_frame_tick	= -1,
	}, {
		.parse_scenario_nm	= "rear_single_ultrawide_video_fhd60_",
		.scenario_id		= IS_DVFS_SN_REAR_SINGLE_ULTRAWIDE_VIDEO_FHD60,
		.keep_frame_tick	= -1,
	}, {
		.parse_scenario_nm	= "rear_single_ultrawide_video_fhd60_supersteady_",
		.scenario_id		= IS_DVFS_SN_REAR_SINGLE_ULTRAWIDE_VIDEO_FHD60_SUPERSTEADY,
		.keep_frame_tick	= -1,
	}, {
		.parse_scenario_nm	= "rear_single_ultrawide_video_fhd60_hf_supersteady_",
		.scenario_id		= IS_DVFS_SN_REAR_SINGLE_ULTRAWIDE_VIDEO_FHD60_HF_SUPERSTEADY,
		.keep_frame_tick	= -1,
	}, {
		.parse_scenario_nm	= "rear_single_ultrawide_video_fhd120_",
		.scenario_id		= IS_DVFS_SN_REAR_SINGLE_ULTRAWIDE_VIDEO_FHD120,
		.keep_frame_tick	= -1,
	}, {
		.parse_scenario_nm	= "rear_single_ultrawide_video_fhd480_",
		.scenario_id		= IS_DVFS_SN_REAR_SINGLE_ULTRAWIDE_VIDEO_FHD480,
		.keep_frame_tick	= -1,
	}, {
		.parse_scenario_nm	= "rear_single_ultrawide_video_uhd30_",
		.scenario_id		= IS_DVFS_SN_REAR_SINGLE_ULTRAWIDE_VIDEO_UHD30,
		.keep_frame_tick	= -1,
	}, {
		.parse_scenario_nm	= "rear_single_ultrawide_video_uhd60_",
		.scenario_id		= IS_DVFS_SN_REAR_SINGLE_ULTRAWIDE_VIDEO_UHD60,
		.keep_frame_tick	= -1,
	}, {
		.parse_scenario_nm	= "rear_single_ultrawide_ssm_",
		.scenario_id		= IS_DVFS_SN_REAR_SINGLE_ULTRAWIDE_SSM,
		.keep_frame_tick	= -1,
	},
	/* macro sensor scenarios */
	{
		.parse_scenario_nm	= "rear_single_macro_photo_",
		.scenario_id		= IS_DVFS_SN_REAR_SINGLE_MACRO_PHOTO,
		.keep_frame_tick	= -1,
	}, {
		.parse_scenario_nm	= "rear_single_macro_photo_full_",
		.scenario_id		= IS_DVFS_SN_REAR_SINGLE_MACRO_PHOTO_FULL,
		.keep_frame_tick	= -1,
	}, {
		.parse_scenario_nm	= "rear_single_macro_capture_",
		.scenario_id		= IS_DVFS_SN_REAR_SINGLE_MACRO_CAPTURE,
		.keep_frame_tick	= IS_DVFS_CAPTURE_TICK,
	}, {
		.parse_scenario_nm	= "rear_single_macro_video_fhd30_",
		.scenario_id		= IS_DVFS_SN_REAR_SINGLE_MACRO_VIDEO_FHD30,
		.keep_frame_tick	= -1,
	},
	/* wide and tele dual sensor scenarios */
	{
		.parse_scenario_nm	= "rear_dual_wide_tele_photo_",
		.scenario_id		= IS_DVFS_SN_REAR_DUAL_WIDE_TELE_PHOTO,
		.keep_frame_tick	= -1,
	}, {
		.parse_scenario_nm	= "rear_dual_wide_tele_capture_",
		.scenario_id		= IS_DVFS_SN_REAR_DUAL_WIDE_TELE_CAPTURE,
		.keep_frame_tick	= IS_DVFS_DUAL_CAPTURE_TICK,
	}, {
		.parse_scenario_nm	= "rear_dual_wide_tele_video_fhd30_",
		.scenario_id		= IS_DVFS_SN_REAR_DUAL_WIDE_TELE_VIDEO_FHD30,
		.keep_frame_tick	= -1,
	}, {
		.parse_scenario_nm	= "rear_dual_wide_tele_video_uhd30_",
		.scenario_id		= IS_DVFS_SN_REAR_DUAL_WIDE_TELE_VIDEO_UHD30,
		.keep_frame_tick	= -1,
	}, {
		.parse_scenario_nm	= "rear_dual_wide_tele_video_fhd60_",
		.scenario_id		= IS_DVFS_SN_REAR_DUAL_WIDE_TELE_VIDEO_FHD60,
		.keep_frame_tick	= -1,
	}, {
		.parse_scenario_nm	= "rear_dual_wide_tele_video_uhd60_",
		.scenario_id		= IS_DVFS_SN_REAR_DUAL_WIDE_TELE_VIDEO_UHD60,
		.keep_frame_tick	= -1,
	},
	/* wide and ultra wide dual sensor scenarios */
	{
		.parse_scenario_nm	= "rear_dual_wide_ultrawide_photo_",
		.scenario_id		= IS_DVFS_SN_REAR_DUAL_WIDE_ULTRAWIDE_PHOTO,
		.keep_frame_tick	= -1,
	}, {
		.parse_scenario_nm	= "rear_dual_wide_ultrawide_capture_",
		.scenario_id		= IS_DVFS_SN_REAR_DUAL_WIDE_ULTRAWIDE_CAPTURE,
		.keep_frame_tick	= IS_DVFS_DUAL_CAPTURE_TICK,
	}, {
		.parse_scenario_nm	= "rear_dual_wide_ultrawide_video_fhd30_",
		.scenario_id		= IS_DVFS_SN_REAR_DUAL_WIDE_ULTRAWIDE_VIDEO_FHD30,
		.keep_frame_tick	= -1,
	}, {
		.parse_scenario_nm	= "rear_dual_wide_ultrawide_video_uhd30_",
		.scenario_id		= IS_DVFS_SN_REAR_DUAL_WIDE_ULTRAWIDE_VIDEO_UHD30,
		.keep_frame_tick	= -1,
	}, {
		.parse_scenario_nm	= "rear_dual_wide_ultrawide_video_fhd60_",
		.scenario_id		= IS_DVFS_SN_REAR_DUAL_WIDE_ULTRAWIDE_VIDEO_FHD60,
		.keep_frame_tick	= -1,
	}, {
		.parse_scenario_nm	= "rear_dual_wide_ultrawide_video_uhd60_",
		.scenario_id		= IS_DVFS_SN_REAR_DUAL_WIDE_ULTRAWIDE_VIDEO_UHD60,
		.keep_frame_tick	= -1,
	},
	/* wide and macro dual sensor scenarios */
	{
		.parse_scenario_nm	= "rear_dual_wide_macro_photo_",
		.scenario_id		= IS_DVFS_SN_REAR_DUAL_WIDE_MACRO_PHOTO,
		.keep_frame_tick	= -1,
	}, {
		.parse_scenario_nm	= "rear_dual_wide_macro_capture_",
		.scenario_id		= IS_DVFS_SN_REAR_DUAL_WIDE_MACRO_CAPTURE,
		.keep_frame_tick	= IS_DVFS_DUAL_CAPTURE_TICK,
	}, {
		.parse_scenario_nm	= "rear_dual_wide_macro_video_fhd30_",
		.scenario_id		= IS_DVFS_SN_REAR_DUAL_WIDE_MACRO_VIDEO_FHD30,
		.keep_frame_tick	= -1,
	},
	/* front sensor scenarios */
	{
		.parse_scenario_nm	= "front_single_photo_",
		.scenario_id		= IS_DVFS_SN_FRONT_SINGLE_PHOTO,
		.keep_frame_tick	= -1,
	}, {
		.parse_scenario_nm	= "front_single_photo_full_",
		.scenario_id		= IS_DVFS_SN_FRONT_SINGLE_PHOTO_FULL,
		.keep_frame_tick	= -1,
	}, {
		.parse_scenario_nm	= "front_single_capture_",
		.scenario_id		= IS_DVFS_SN_FRONT_SINGLE_CAPTURE,
		.keep_frame_tick	= IS_DVFS_CAPTURE_TICK,
	}, {
		.parse_scenario_nm	= "front_single_video_fhd30_",
		.scenario_id		= IS_DVFS_SN_FRONT_SINGLE_VIDEO_FHD30,
		.keep_frame_tick	= -1,
	}, {
		.parse_scenario_nm	= "front_single_video_fhd60_",
		.scenario_id		= IS_DVFS_SN_FRONT_SINGLE_VIDEO_FHD60,
		.keep_frame_tick	= -1,
	}, {
		.parse_scenario_nm	= "front_single_video_fhd120_",
		.scenario_id		= IS_DVFS_SN_FRONT_SINGLE_VIDEO_FHD120,
		.keep_frame_tick	= -1,
	}, {
		.parse_scenario_nm	= "front_single_video_uhd30_",
		.scenario_id		= IS_DVFS_SN_FRONT_SINGLE_VIDEO_UHD30,
		.keep_frame_tick	= -1,
	}, {
		.parse_scenario_nm	= "front_single_video_uhd60_",
		.scenario_id		= IS_DVFS_SN_FRONT_SINGLE_VIDEO_UHD60,
		.keep_frame_tick	= -1,
	}, {
		.parse_scenario_nm	= "front_single_video_uhd120_",
		.scenario_id		= IS_DVFS_SN_FRONT_SINGLE_VIDEO_UHD120,
		.keep_frame_tick	= -1,
	}, {
		.parse_scenario_nm	= "front_single_fastae_",
		.scenario_id		= IS_DVFS_SN_FRONT_SINGLE_FASTAE,
		.keep_frame_tick	= -1,
	}, {
		.parse_scenario_nm	= "front_single_secure_",
		.scenario_id		= IS_DVFS_SN_FRONT_SINGLE_SECURE,
		.keep_frame_tick	= -1,
	}, {
		.parse_scenario_nm	= "front_single_vt_",
		.scenario_id		= IS_DVFS_SN_FRONT_SINGLE_VT,
		.keep_frame_tick	= -1,
	},
	/* pip scenarios */
	{
		.parse_scenario_nm	= "pip_dual_photo_",
		.scenario_id		= IS_DVFS_SN_PIP_DUAL_PHOTO,
		.keep_frame_tick	= -1,
	}, {
		.parse_scenario_nm	= "pip_dual_capture_",
		.scenario_id		= IS_DVFS_SN_PIP_DUAL_CAPTURE,
		.keep_frame_tick	= IS_DVFS_CAPTURE_TICK,
	}, {
		.parse_scenario_nm	= "pip_dual_video_fhd30_",
		.scenario_id		= IS_DVFS_SN_PIP_DUAL_VIDEO_FHD30,
		.keep_frame_tick	= -1,
	},
	/* triple scenarios */
	{
		.parse_scenario_nm	= "triple_photo_",
		.scenario_id		= IS_DVFS_SN_TRIPLE_PHOTO,
		.keep_frame_tick	= -1,
	}, {
		.parse_scenario_nm	= "triple_video_fhd30_",
		.scenario_id		= IS_DVFS_SN_TRIPLE_VIDEO_FHD30,
		.keep_frame_tick	= -1,
	}, {
		.parse_scenario_nm	= "triple_video_uhd30_",
		.scenario_id		= IS_DVFS_SN_TRIPLE_VIDEO_UHD30,
		.keep_frame_tick	= -1,
	}, {
		.parse_scenario_nm	= "triple_video_fhd60_",
		.scenario_id		= IS_DVFS_SN_TRIPLE_VIDEO_FHD60,
		.keep_frame_tick	= -1,
	}, {
		.parse_scenario_nm	= "triple_video_uhd60_",
		.scenario_id		= IS_DVFS_SN_TRIPLE_VIDEO_UHD60,
		.keep_frame_tick	= -1,
	}, {
		.parse_scenario_nm	= "triple_capture_",
		.scenario_id		= IS_DVFS_SN_TRIPLE_CAPTURE,
		.keep_frame_tick	= IS_DVFS_CAPTURE_TICK,
	},
	/* external camera scenarios */
	{
		.parse_scenario_nm	= "ext_rear_single_",
		.scenario_id		= IS_DVFS_SN_EXT_REAR_SINGLE,
		.keep_frame_tick	= -1,
	}, {
		.parse_scenario_nm	= "ext_rear_dual_",
		.scenario_id		= IS_DVFS_SN_EXT_REAR_DUAL,
		.keep_frame_tick	= -1,
	}, {
		.parse_scenario_nm	= "ext_front_",
		.scenario_id		= IS_DVFS_SN_EXT_FRONT,
		.keep_frame_tick	= -1,
	}, {
		.parse_scenario_nm	= "ext_front_secure_",
		.scenario_id		= IS_DVFS_SN_EXT_FRONT_SECURE,
		.keep_frame_tick	= -1,
	},
	/* max scenario */
	{
		.parse_scenario_nm	= "max_",
		.scenario_id		= IS_DVFS_SN_MAX,
		.keep_frame_tick	= -1,
	} );

static int is_hw_dvfs_get_hf(struct is_device_ischain *device,
	struct is_dvfs_scenario_param *param)
{
	struct param_mcs_output *mcs_output;

	/* only wide single */
	if (!(param->num == IS_DVFS_NUM_SINGLE))
		return 0;

	mcs_output = is_itf_g_param(device, NULL, PARAM_MCS_OUTPUT5);
	if (mcs_output->dma_cmd == DMA_OUTPUT_COMMAND_ENABLE)
		return 1;
	else
		return 0;
}

int is_hw_dvfs_get_scenario_param(struct is_device_ischain *device,
	int flag_capture, struct is_dvfs_scenario_param *param)
{
	struct is_core *core;
	struct is_device_sensor *sensor_device;
	struct is_resourcemgr *resourcemgr;
	struct is_dual_info *dual_info;
	int pos, fps_th;

	core = (struct is_core *)device->interface->core;
	param->sensor_map = core->sensor_map;
	param->dual_mode = core->dual_info.mode;
	param->sensor_active_map = param->sensor_map;

	sensor_device = device->sensor;
	resourcemgr = sensor_device->resourcemgr;
	if (IS_ENABLED(IS_DVFS_USE_3AA_STNADBY))
		fps_th = (resourcemgr->limited_fps) ? 0 : 5;
	else
		fps_th = 0;

	if (param->dual_mode) {
		if (param->wide_mask) {
			pos = IS_DVFS_SENSOR_POSITION_WIDE;
			if (pos >= 0 && core->dual_info.max_fps[pos] <= fps_th)
				param->sensor_active_map &= ~(1 << pos);
		}
		if (param->tele_mask) {
			pos = IS_DVFS_SENSOR_POSITION_TELE;
			if (pos >= 0 && core->dual_info.max_fps[pos] <= fps_th)
				param->sensor_active_map &= ~(1 << pos);
		}
		if (param->tele2_mask) {
			pos = IS_DVFS_SENSOR_POSITION_TELE2;
			if (pos >= 0 && core->dual_info.max_fps[pos] <= fps_th)
				param->sensor_active_map &= ~(1 << pos);
		}
		if (param->ultrawide_mask) {
			pos = IS_DVFS_SENSOR_POSITION_ULTRAWIDE;
			if (pos >= 0 && core->dual_info.max_fps[pos] <= fps_th)
				param->sensor_active_map &= ~(1 << pos);
		}
		if (param->macro_mask) {
			pos = IS_DVFS_SENSOR_POSITION_MACRO;
			if (pos >= 0 && core->dual_info.max_fps[pos] <= fps_th)
				param->sensor_active_map &= ~(1 << pos);
		}
		if (param->sensor_active_map == 0) {
			mwarn("[DVFS] dual_info inconsistent\n", device);
			if (is_hw_dvfs_get_resol(device, param) == IS_DVFS_RESOL_8K)
				param->sensor_active_map = param->sensor_map;
		}
	}
	param->rear_face = param->sensor_active_map & param->rear_mask;
	param->front_face = param->sensor_active_map & param->front_mask;
	dbg_dvfs(0, "[DVFS] %s() sensor_map: 0x%x, sensor_active_map: 0x%x, rear: 0x%x, front: 0x%x, dual_mode: %d, fps_th: %d\n",
		device, __func__, param->sensor_map, param->sensor_active_map,
		param->rear_face, param->front_face, param->dual_mode, fps_th);

	param->sensor_mode = sensor_device->cfg->special_mode;
	param->sensor_fps = is_sensor_g_framerate(sensor_device);
	param->setfile = (device->setfile & IS_SETFILE_MASK);
	param->scen = (device->setfile & IS_SCENARIO_MASK) >> IS_SCENARIO_SHIFT;
	param->dvfs_scenario = device->dvfs_scenario;
	param->secure = core->scenario;
	dbg_dvfs(1, "[DVFS] %s() sensor_mode: %d, fps: %d, setfile: %d, scen: %d, dvfs_scenario: %x, secure: %d\n",
		device, __func__, param->sensor_mode, param->sensor_fps,
		param->setfile, param->scen, param->dvfs_scenario, param->secure);

	param->face = is_hw_dvfs_get_face(device, param);
	param->num = is_hw_dvfs_get_num(device, param);
	param->sensor = is_hw_dvfs_get_sensor(device, param);
	param->hf = is_hw_dvfs_get_hf(device, param);

	if (param->face > IS_DVFS_FACE_END || param->face < 0)
		param->face = IS_DVFS_FACE_END;
	if (param->num > IS_DVFS_NUM_END || param->num < 0)
		param->num = IS_DVFS_NUM_END;
	if (param->sensor > IS_DVFS_SENSOR_END || param->sensor < 0)
		param->sensor = IS_DVFS_SENSOR_END;

	param->mode = is_hw_dvfs_get_mode(device, flag_capture, param);
	param->resol = is_hw_dvfs_get_resol(device, param);
	param->fps = is_hw_dvfs_get_fps(device, param);

	if (param->mode > IS_DVFS_MODE_END || param->mode < 0)
		param->mode = IS_DVFS_MODE_END;
	if (param->resol > IS_DVFS_RESOL_END || param->resol < 0)
		param->resol = IS_DVFS_RESOL_END;
	if (param->fps > IS_DVFS_FPS_END || param->fps < 0)
		param->fps = IS_DVFS_FPS_END;

	dual_info = &core->dual_info;
	dbg_dvfs(0, "[DVFS] dual info (Rear: %dfps, Rear2: %dfps, Rear3: %dfps, Rear4: %dfps, Rear_TOF: %dfsp)\n",
		device,
		dual_info->max_fps[SENSOR_POSITION_REAR],
		dual_info->max_fps[SENSOR_POSITION_REAR2],
		dual_info->max_fps[SENSOR_POSITION_REAR3],
		dual_info->max_fps[SENSOR_POSITION_REAR4],
		dual_info->max_fps[SENSOR_POSITION_REAR_TOF]);
	dbg_dvfs(0, "[DVFS] (Front: %dfps, Front2: %dfps, Front3: %dfps, Front4: %dfps, Front_TOF: %dfsp, mode: %d)\n",
		device,
		dual_info->max_fps[SENSOR_POSITION_FRONT],
		dual_info->max_fps[SENSOR_POSITION_FRONT2],
		dual_info->max_fps[SENSOR_POSITION_FRONT3],
		dual_info->max_fps[SENSOR_POSITION_FRONT4],
		dual_info->max_fps[SENSOR_POSITION_FRONT_TOF],
		dual_info->mode);

	return 0;
}

int is_hw_dvfs_get_face(struct is_device_ischain *device,
	struct is_dvfs_scenario_param *param)
{
	if (param->rear_face && param->front_face)
		return IS_DVFS_FACE_PIP;
	else if (param->rear_face)
		return IS_DVFS_FACE_REAR;
	else if (param->front_face)
		return IS_DVFS_FACE_FRONT;
	else
		return -EINVAL;
}

int is_hw_dvfs_get_num(struct is_device_ischain *device,
	struct is_dvfs_scenario_param *param)
{
	switch (param->face) {
	case IS_DVFS_FACE_REAR:
		if (is_hw_dvfs_get_bit_count(param->rear_face) == 1)
			return IS_DVFS_NUM_SINGLE;
		else if (is_hw_dvfs_get_bit_count(param->rear_face) == 2)
				return IS_DVFS_NUM_DUAL;
		else if (is_hw_dvfs_get_bit_count(param->rear_face) == 3)
				return IS_DVFS_NUM_TRIPLE;
		else
			return -EINVAL;
	case IS_DVFS_FACE_FRONT:
		if (is_hw_dvfs_get_bit_count(param->front_face) == 1)
			return IS_DVFS_NUM_SINGLE;
		else
			return -EINVAL;
	case IS_DVFS_FACE_PIP:
		if (is_hw_dvfs_get_bit_count(param->rear_face) +
			is_hw_dvfs_get_bit_count(param->front_face) == 2)
			return IS_DVFS_NUM_DUAL;
		else if (is_hw_dvfs_get_bit_count(param->rear_face) +
			is_hw_dvfs_get_bit_count(param->front_face) == 3)
			return IS_DVFS_NUM_TRIPLE;
		else
			return -EINVAL;
	default:
		break;
	}
	return -EINVAL;
}

int is_hw_dvfs_get_sensor(struct is_device_ischain *device,
	struct is_dvfs_scenario_param *param)
{
	int vendor;
	vendor = (param->dvfs_scenario >> IS_DVFS_SCENARIO_VENDOR_SHIFT)
			& IS_DVFS_SCENARIO_VENDOR_MASK;
	switch (param->face) {
	case IS_DVFS_FACE_REAR:
		switch (param->num) {
		case IS_DVFS_NUM_SINGLE:
			if (param->sensor_active_map & param->wide_mask) {
				if (param->sensor_mode == IS_SPECIAL_MODE_FASTAE)
					return IS_DVFS_SENSOR_WIDE_FASTAE;
				if (param->sensor_mode == IS_SPECIAL_MODE_REMOSAIC)
					return IS_DVFS_SENSOR_WIDE_REMOSAIC;
				if (vendor == IS_DVFS_SCENARIO_VENDOR_SSM)
					return IS_DVFS_SENSOR_WIDE_SSM;
				if (vendor == IS_DVFS_SCENARIO_VENDOR_LOW_POWER)
					return IS_DVFS_SENSOR_WIDE_VT;
				return IS_DVFS_SENSOR_WIDE;
			} else if ((param->sensor_active_map & param->tele_mask)
				|| (param->sensor_active_map & param->tele2_mask)) {
				if (param->sensor_mode == IS_SPECIAL_MODE_FASTAE)
					return IS_DVFS_SENSOR_WIDE_FASTAE;
				if (param->sensor_mode == IS_SPECIAL_MODE_REMOSAIC)
					return IS_DVFS_SENSOR_TELE_REMOSAIC;
				return IS_DVFS_SENSOR_TELE;
			} else if (param->sensor_active_map & param->ultrawide_mask) {
				if (param->sensor_mode == IS_SPECIAL_MODE_FASTAE)
					return IS_DVFS_SENSOR_WIDE_FASTAE;
				if (vendor == IS_DVFS_SCENARIO_VENDOR_SSM)
					return IS_DVFS_SENSOR_ULTRAWIDE_SSM;
				return IS_DVFS_SENSOR_ULTRAWIDE;
			} else if (param->sensor_active_map & param->macro_mask) {
				if (param->sensor_mode == IS_SPECIAL_MODE_FASTAE)
					return IS_DVFS_SENSOR_WIDE_FASTAE;
				return IS_DVFS_SENSOR_MACRO;
			} else {
				return -EINVAL;
			}
		case IS_DVFS_NUM_DUAL:
			if (((param->sensor_active_map & param->wide_mask) &&
				(param->sensor_active_map & param->tele_mask))
				|| (param->sensor_active_map & param->tele2_mask)) {
				return IS_DVFS_SENSOR_WIDE_TELE;
			} else if (((param->sensor_active_map & param->wide_mask) &&
				(param->sensor_active_map & param->ultrawide_mask))
				|| (param->sensor_active_map & param->ultrawide_mask)) {
				return IS_DVFS_SENSOR_WIDE_ULTRAWIDE;
			} else if ((param->sensor_active_map & param->wide_mask) &&
				(param->sensor_active_map & param->macro_mask)) {
				return IS_DVFS_SENSOR_WIDE_MACRO;
			} else {
				return -EINVAL;
			}
		case IS_DVFS_NUM_TRIPLE:
			return IS_DVFS_SENSOR_TRIPLE;
		default:
			break;
		}

		break;
	case IS_DVFS_FACE_FRONT:
		if (param->sensor_map & param->front_sensor_mask) {
			if (param->sensor_mode == IS_SPECIAL_MODE_FASTAE)
				return IS_DVFS_SENSOR_FRONT_FASTAE;
			else if (param->secure)
				return IS_DVFS_SENSOR_FRONT_SECURE;
			else if (vendor == IS_DVFS_SCENARIO_VENDOR_VT)
				return IS_DVFS_SENSOR_FRONT_VT;
			return IS_DVFS_SENSOR_FRONT;
		} else {
			return -EINVAL;
		}

		break;
	case IS_DVFS_FACE_PIP:
		if (param->sensor_mode == IS_SPECIAL_MODE_FASTAE)
			return IS_DVFS_SENSOR_WIDE_FASTAE;
		switch (param->num) {
		case IS_DVFS_NUM_DUAL:
			return IS_DVFS_SENSOR_PIP;
		case IS_DVFS_NUM_TRIPLE:
			return IS_DVFS_SENSOR_TRIPLE;
		}
		break;
	default:
		break;
	}
	return -EINVAL;
}

int is_hw_dvfs_get_mode(struct is_device_ischain *device, int flag_capture,
	struct is_dvfs_scenario_param *param)
{
	int common;
	common = param->dvfs_scenario & IS_DVFS_SCENARIO_COMMON_MODE_MASK;
	if (flag_capture)
		return IS_DVFS_MODE_CAPTURE;
	if (common == IS_DVFS_SCENARIO_COMMON_MODE_PHOTO)
		return IS_DVFS_MODE_PHOTO;
	else if (common == IS_DVFS_SCENARIO_COMMON_MODE_VIDEO)
		return IS_DVFS_MODE_VIDEO;
	else
		return -EINVAL;
}

int is_hw_dvfs_get_resol(struct is_device_ischain *device,
	struct is_dvfs_scenario_param *param)
{
	int width, height, resol = 0;
	u32 sensor_size = 0;

	width = device->dvfs_rec_size & 0xffff;
	height = device->dvfs_rec_size >> IS_DVFS_SCENARIO_HEIGHT_SHIFT;
	dbg_dvfs(1, "[DVFS] width: %d, height: %d\n",
		device,	width, height);
	resol = width * height;

	sensor_size = is_sensor_g_width(device->sensor) * is_sensor_g_height(device->sensor);
	if (param->mode == IS_DVFS_MODE_PHOTO && resol == sensor_size)
		return IS_DVFS_RESOL_FULL;
	else if (resol < IS_DVFS_RESOL_FHD_TH)
		return IS_DVFS_RESOL_FHD;
	else if (resol < IS_DVFS_RESOL_UHD_TH)
		return IS_DVFS_RESOL_UHD;
	else if (resol < IS_DVFS_RESOL_8K_TH)
		return IS_DVFS_RESOL_8K;
	else
		return -EINVAL;
}

int is_hw_dvfs_get_fps(struct is_device_ischain *device,
	struct is_dvfs_scenario_param *param)
{
	if (param->sensor_fps <= 24)
		return IS_DVFS_FPS_24;
	else if (param->sensor_fps <= 30)
		return IS_DVFS_FPS_30;
	else if (param->sensor_fps <= 60)
		return IS_DVFS_FPS_60;
	else if (param->sensor_fps <= 120)
		return IS_DVFS_FPS_120;
	else if (param->sensor_fps <= 240)
		return IS_DVFS_FPS_240;
	else if (param->sensor_fps <= 480)
		return IS_DVFS_FPS_480;
	else
		return -EINVAL;
}

int is_hw_dvfs_get_rear_single_wide_scenario(struct is_device_ischain *device,
	struct is_dvfs_scenario_param *param)
{
	int vendor = (param->dvfs_scenario >> IS_DVFS_SCENARIO_VENDOR_SHIFT) & IS_DVFS_SCENARIO_VENDOR_MASK;

	dbg_dvfs(1, "[DVFS] %s() mode: %s, resol: %s, fps: %s\n", device, __func__,
		str_mode[param->mode], str_resol[param->resol], str_fps[param->fps]);

	switch (param->mode) {
	case IS_DVFS_MODE_PHOTO:
		if (param->resol == IS_DVFS_RESOL_FULL)
			return IS_DVFS_SN_REAR_SINGLE_WIDE_PHOTO_FULL;
		else
			return IS_DVFS_SN_REAR_SINGLE_WIDE_PHOTO;
	case IS_DVFS_MODE_CAPTURE:
		return IS_DVFS_SN_REAR_SINGLE_WIDE_CAPTURE;
	case IS_DVFS_MODE_VIDEO:
		switch (param->resol) {
		case IS_DVFS_RESOL_FHD:
			switch (param->fps) {
			case IS_DVFS_FPS_24:
			case IS_DVFS_FPS_30:
				if (vendor == IS_DVFS_SCENARIO_VENDOR_VIDEO_HDR)
					return IS_DVFS_SN_REAR_SINGLE_WIDE_VIDEOHDR;
				return IS_DVFS_SN_REAR_SINGLE_WIDE_VIDEO_FHD30;
			case IS_DVFS_FPS_60:
				switch (param->hf) {
				case 0:
					if (vendor == IS_DVFS_SCENARIO_VENDOR_SUPER_STEADY)
						return IS_DVFS_SN_REAR_SINGLE_WIDE_VIDEO_FHD60_SUPERSTEADY;
					return IS_DVFS_SN_REAR_SINGLE_WIDE_VIDEO_FHD60;
				case 1:
					if (vendor == IS_DVFS_SCENARIO_VENDOR_SUPER_STEADY)
						return IS_DVFS_SN_REAR_SINGLE_WIDE_VIDEO_FHD60_HF_SUPERSTEADY;
					return IS_DVFS_SN_REAR_SINGLE_WIDE_VIDEO_FHD60_HF;
				default:
					goto dvfs_err;
				}
			case IS_DVFS_FPS_120:
				return IS_DVFS_SN_REAR_SINGLE_WIDE_VIDEO_FHD120;
			case IS_DVFS_FPS_240:
				return IS_DVFS_SN_REAR_SINGLE_WIDE_VIDEO_FHD240;
			case IS_DVFS_FPS_480:
				return IS_DVFS_SN_REAR_SINGLE_WIDE_VIDEO_FHD480;
			default:
				goto dvfs_err;
			}
		case IS_DVFS_RESOL_UHD:
			switch (param->fps) {
			case IS_DVFS_FPS_24:
			case IS_DVFS_FPS_30:
				return IS_DVFS_SN_REAR_SINGLE_WIDE_VIDEO_UHD30;
			case IS_DVFS_FPS_60:
				switch (param->hf) {
				case 0:
					return IS_DVFS_SN_REAR_SINGLE_WIDE_VIDEO_UHD60;
				case 1:
					return IS_DVFS_SN_REAR_SINGLE_WIDE_VIDEO_UHD60_HF;
				default:
					goto dvfs_err;
				}
			case IS_DVFS_FPS_120:
				return IS_DVFS_SN_REAR_SINGLE_WIDE_VIDEO_UHD120;
			default:
				goto dvfs_err;
			}
		case IS_DVFS_RESOL_8K:
			switch (param->fps) {
			case IS_DVFS_FPS_24:
				switch (param->hf) {
				case 0:
					return IS_DVFS_SN_REAR_SINGLE_WIDE_VIDEO_8K24;
				case 1:
					return IS_DVFS_SN_REAR_SINGLE_WIDE_VIDEO_8K24_HF;
				default:
					goto dvfs_err;
				}
			case IS_DVFS_FPS_30:
				switch (param->hf) {
				case 0:
					return IS_DVFS_SN_REAR_SINGLE_WIDE_VIDEO_8K30;
				case 1:
					return IS_DVFS_SN_REAR_SINGLE_WIDE_VIDEO_8K30_HF;
				default:
					goto dvfs_err;
				}
			default:
				goto dvfs_err;
			}
		default:
			goto dvfs_err;
		}
	default:
		goto dvfs_err;
	}

dvfs_err:
	merr("%s fails. (mode: %s, resol: %s, fps: %s) not supported.", device, __func__,
		str_mode[param->mode], str_resol[param->resol], str_fps[param->fps]);
	return -EINVAL;
}

int is_hw_dvfs_get_rear_single_wide_remosaic_scenario(struct is_device_ischain *device,
	struct is_dvfs_scenario_param *param)
{
	dbg_dvfs(1, "[DVFS] %s() mode: %s, resol: %s, fps: %s\n", device, __func__,
		str_mode[param->mode], str_resol[param->resol], str_fps[param->fps]);

	switch (param->mode) {
	case IS_DVFS_MODE_PHOTO:
		return IS_DVFS_SN_REAR_SINGLE_WIDE_REMOSAIC_PHOTO;
	case IS_DVFS_MODE_CAPTURE:
		return IS_DVFS_SN_REAR_SINGLE_WIDE_REMOSAIC_CAPTURE;
	default:
		goto dvfs_err;
	}

dvfs_err:
	merr("%s fails. (mode: %s, resol: %s, fps: %s) not supported.", device, __func__,
		str_mode[param->mode], str_resol[param->resol], str_fps[param->fps]);
	return -EINVAL;
}

int is_hw_dvfs_get_rear_single_tele_scenario(struct is_device_ischain *device,
	struct is_dvfs_scenario_param *param)
{
	dbg_dvfs(1, "[DVFS] %s() mode: %s, resol: %s, fps: %s\n", device, __func__,
		str_mode[param->mode], str_resol[param->resol], str_fps[param->fps]);

	switch (param->mode) {
	case IS_DVFS_MODE_PHOTO:
		if (param->resol == IS_DVFS_RESOL_FULL)
			return IS_DVFS_SN_REAR_SINGLE_TELE_PHOTO_FULL;
		else
			return IS_DVFS_SN_REAR_SINGLE_TELE_PHOTO;
	case IS_DVFS_MODE_CAPTURE:
		return IS_DVFS_SN_REAR_SINGLE_TELE_CAPTURE;
	case IS_DVFS_MODE_VIDEO:
		switch (param->resol) {
		case IS_DVFS_RESOL_FHD:
			switch (param->fps) {
			case IS_DVFS_FPS_24:
			case IS_DVFS_FPS_30:
				return IS_DVFS_SN_REAR_SINGLE_TELE_VIDEO_FHD30;
			case IS_DVFS_FPS_60:
				return IS_DVFS_SN_REAR_SINGLE_TELE_VIDEO_FHD60;
			default:
				goto dvfs_err;
			}
		case IS_DVFS_RESOL_UHD:
			switch (param->fps) {
			case IS_DVFS_FPS_24:
			case IS_DVFS_FPS_30:
				return IS_DVFS_SN_REAR_SINGLE_TELE_VIDEO_UHD30;
			case IS_DVFS_FPS_60:
				return IS_DVFS_SN_REAR_SINGLE_TELE_VIDEO_UHD60;
			default:
				goto dvfs_err;
			}
		case IS_DVFS_RESOL_8K:
			switch (param->fps) {
			case IS_DVFS_FPS_24:
				switch (param->hf) {
				case 0:
					return IS_DVFS_SN_REAR_SINGLE_TELE_VIDEO_8K24;
				case 1:
					return IS_DVFS_SN_REAR_SINGLE_TELE_VIDEO_8K24_HF;
				default:
					goto dvfs_err;
				}
			case IS_DVFS_FPS_30:
				switch (param->hf) {
				case 0:
					return IS_DVFS_SN_REAR_SINGLE_TELE_VIDEO_8K30;
				case 1:
					return IS_DVFS_SN_REAR_SINGLE_TELE_VIDEO_8K30_HF;
				default:
					goto dvfs_err;
				}
			default:
				goto dvfs_err;
			}
		default:
			goto dvfs_err;
		}
	default:
		goto dvfs_err;
	}

dvfs_err:
	merr("%s fails. (mode: %s, resol: %s, fps: %s) not supported.", device, __func__,
		str_mode[param->mode], str_resol[param->resol], str_fps[param->fps]);
	return -EINVAL;
}

int is_hw_dvfs_get_rear_single_tele_remosaic_scenario(struct is_device_ischain *device,
	struct is_dvfs_scenario_param *param)
{
	dbg_dvfs(1, "[DVFS] %s() mode: %s, resol: %s, fps: %s\n", device, __func__,
		str_mode[param->mode], str_resol[param->resol], str_fps[param->fps]);

	switch (param->mode) {
	case IS_DVFS_MODE_PHOTO:
		return IS_DVFS_SN_REAR_SINGLE_TELE_REMOSAIC_PHOTO;
	case IS_DVFS_MODE_CAPTURE:
		return IS_DVFS_SN_REAR_SINGLE_TELE_REMOSAIC_CAPTURE;
	default:
		goto dvfs_err;
	}

dvfs_err:
	merr("%s fails. (mode: %s, resol: %s, fps: %s) not supported.", device, __func__,
		str_mode[param->mode], str_resol[param->resol], str_fps[param->fps]);
	return -EINVAL;
}

int is_hw_dvfs_get_rear_single_ultrawide_scenario(struct is_device_ischain *device,
	struct is_dvfs_scenario_param *param)
{
	int vendor = (param->dvfs_scenario >> IS_DVFS_SCENARIO_VENDOR_SHIFT) & IS_DVFS_SCENARIO_VENDOR_MASK;

	dbg_dvfs(1, "[DVFS] %s() mode: %s, resol: %s, fps: %s\n", device, __func__,
		str_mode[param->mode], str_resol[param->resol], str_fps[param->fps]);

	switch (param->mode) {
	case IS_DVFS_MODE_PHOTO:
		if (param->resol == IS_DVFS_RESOL_FULL)
			return IS_DVFS_SN_REAR_SINGLE_ULTRAWIDE_PHOTO_FULL;
		else
			return IS_DVFS_SN_REAR_SINGLE_ULTRAWIDE_PHOTO;
	case IS_DVFS_MODE_CAPTURE:
		return IS_DVFS_SN_REAR_SINGLE_ULTRAWIDE_CAPTURE;
	case IS_DVFS_MODE_VIDEO:
		switch (param->resol) {
		case IS_DVFS_RESOL_FHD:
			switch (param->fps) {
			case IS_DVFS_FPS_24:
			case IS_DVFS_FPS_30:
				switch (param->hf) {
				case 0:
					if (vendor == IS_DVFS_SCENARIO_VENDOR_SUPER_STEADY)
						return IS_DVFS_SN_REAR_SINGLE_ULTRAWIDE_VIDEO_FHD30_SUPERSTEADY;
					return IS_DVFS_SN_REAR_SINGLE_ULTRAWIDE_VIDEO_FHD30;
				case 1:
					if (vendor == IS_DVFS_SCENARIO_VENDOR_SUPER_STEADY)
						return IS_DVFS_SN_REAR_SINGLE_ULTRAWIDE_VIDEO_FHD30_HF_SUPERSTEADY;
					else
						goto dvfs_err;
				default:
					goto dvfs_err;
				}
			case IS_DVFS_FPS_60:
				switch (param->hf) {
				case 0:
					if (vendor == IS_DVFS_SCENARIO_VENDOR_SUPER_STEADY)
						return IS_DVFS_SN_REAR_SINGLE_ULTRAWIDE_VIDEO_FHD60_SUPERSTEADY;
					return IS_DVFS_SN_REAR_SINGLE_ULTRAWIDE_VIDEO_FHD60;
				case 1:
					if (vendor == IS_DVFS_SCENARIO_VENDOR_SUPER_STEADY)
						return IS_DVFS_SN_REAR_SINGLE_ULTRAWIDE_VIDEO_FHD60_HF_SUPERSTEADY;
					else
						goto dvfs_err;
				default:
					goto dvfs_err;
				}
			/* use the same scenario as that of wide sensor */
			case IS_DVFS_FPS_120:
				return IS_DVFS_SN_REAR_SINGLE_ULTRAWIDE_VIDEO_FHD120;
			case IS_DVFS_FPS_480:
				return IS_DVFS_SN_REAR_SINGLE_ULTRAWIDE_VIDEO_FHD480;
			default:
				goto dvfs_err;
			}
		case IS_DVFS_RESOL_UHD:
			switch (param->fps) {
			case IS_DVFS_FPS_24:
			case IS_DVFS_FPS_30:
				return IS_DVFS_SN_REAR_SINGLE_ULTRAWIDE_VIDEO_UHD30;
			case IS_DVFS_FPS_60:
				return IS_DVFS_SN_REAR_SINGLE_ULTRAWIDE_VIDEO_UHD60;
			default:
				goto dvfs_err;
			}
		default:
			goto dvfs_err;
		}
	default:
		goto dvfs_err;
	}

dvfs_err:
	merr("%s fails. (mode: %s, resol: %s, fps: %s) not supported.", device, __func__,
		str_mode[param->mode], str_resol[param->resol], str_fps[param->fps]);
	return -EINVAL;
}

int is_hw_dvfs_get_rear_single_macro_scenario(struct is_device_ischain *device,
	struct is_dvfs_scenario_param *param)
{
	dbg_dvfs(1, "[DVFS] %s() mode: %s, resol: %s, fps: %s\n", device, __func__,
		str_mode[param->mode], str_resol[param->resol], str_fps[param->fps]);

	switch (param->mode) {
	case IS_DVFS_MODE_PHOTO:
		if (param->resol == IS_DVFS_RESOL_FULL)
			return IS_DVFS_SN_REAR_SINGLE_MACRO_PHOTO_FULL;
		else
			return IS_DVFS_SN_REAR_SINGLE_MACRO_PHOTO;
	case IS_DVFS_MODE_CAPTURE:
		return IS_DVFS_SN_REAR_SINGLE_MACRO_CAPTURE;
	case IS_DVFS_MODE_VIDEO:
		switch (param->resol) {
		case IS_DVFS_RESOL_FHD:
			return IS_DVFS_SN_REAR_SINGLE_MACRO_VIDEO_FHD30;
		default:
			goto dvfs_err;
		}
	default:
		goto dvfs_err;
	}

dvfs_err:
	merr("%s fails. (mode: %s, resol: %s, fps: %s) not supported.", device, __func__,
		str_mode[param->mode], str_resol[param->resol], str_fps[param->fps]);
	return -EINVAL;
}

int is_hw_dvfs_get_rear_dual_wide_tele_scenario(struct is_device_ischain *device,
	struct is_dvfs_scenario_param *param)
{
	dbg_dvfs(1, "[DVFS] %s() mode: %s, resol: %s, fps: %s\n", device, __func__,
		str_mode[param->mode], str_resol[param->resol], str_fps[param->fps]);

	switch (param->mode) {
	case IS_DVFS_MODE_PHOTO:
		return IS_DVFS_SN_REAR_DUAL_WIDE_TELE_PHOTO;
	case IS_DVFS_MODE_CAPTURE:
		return IS_DVFS_SN_REAR_DUAL_WIDE_TELE_CAPTURE;
	case IS_DVFS_MODE_VIDEO:
		switch (param->resol) {
		case IS_DVFS_RESOL_FHD:
			switch (param->fps) {
			case IS_DVFS_FPS_24:
			case IS_DVFS_FPS_30:
				return IS_DVFS_SN_REAR_DUAL_WIDE_TELE_VIDEO_FHD30;
			case IS_DVFS_FPS_60:
				return IS_DVFS_SN_REAR_DUAL_WIDE_TELE_VIDEO_FHD60;
			default:
				goto dvfs_err;
			}
		case IS_DVFS_RESOL_UHD:
			switch (param->fps) {
			case IS_DVFS_FPS_24:
			case IS_DVFS_FPS_30:
				return IS_DVFS_SN_REAR_DUAL_WIDE_TELE_VIDEO_UHD30;
			case IS_DVFS_FPS_60:
				return IS_DVFS_SN_REAR_DUAL_WIDE_TELE_VIDEO_UHD60;
			default:
				goto dvfs_err;
			}
		default:
			goto dvfs_err;
		}
	default:
		goto dvfs_err;
	}

dvfs_err:
	merr("%s fails. (mode: %s, resol: %s, fps: %s) not supported.", device, __func__,
		str_mode[param->mode], str_resol[param->resol], str_fps[param->fps]);
	return -EINVAL;
}

int is_hw_dvfs_get_rear_dual_wide_ultrawide_scenario(struct is_device_ischain *device,
	struct is_dvfs_scenario_param *param)
{
	dbg_dvfs(1, "[DVFS] %s() mode: %s, resol: %s, fps: %s\n", device, __func__,
		str_mode[param->mode], str_resol[param->resol], str_fps[param->fps]);

	switch (param->mode) {
	case IS_DVFS_MODE_PHOTO:
		return IS_DVFS_SN_REAR_DUAL_WIDE_ULTRAWIDE_PHOTO;
	case IS_DVFS_MODE_CAPTURE:
		return IS_DVFS_SN_REAR_DUAL_WIDE_ULTRAWIDE_CAPTURE;
	case IS_DVFS_MODE_VIDEO:
		switch (param->resol) {
		case IS_DVFS_RESOL_FHD:
			switch (param->fps) {
			case IS_DVFS_FPS_24:
			case IS_DVFS_FPS_30:
				return IS_DVFS_SN_REAR_DUAL_WIDE_ULTRAWIDE_VIDEO_FHD30;
			case IS_DVFS_FPS_60:
				return IS_DVFS_SN_REAR_DUAL_WIDE_ULTRAWIDE_VIDEO_FHD60;
			default:
				goto dvfs_err;
			}
		case IS_DVFS_RESOL_UHD:
			switch (param->fps) {
			case IS_DVFS_FPS_24:
			case IS_DVFS_FPS_30:
				return IS_DVFS_SN_REAR_DUAL_WIDE_ULTRAWIDE_VIDEO_UHD30;
			case IS_DVFS_FPS_60:
				return IS_DVFS_SN_REAR_DUAL_WIDE_ULTRAWIDE_VIDEO_UHD60;
			default:
				goto dvfs_err;
			}
		default:
			goto dvfs_err;
		}
	default:
		goto dvfs_err;
	}

dvfs_err:
	merr("%s fails. (mode: %s, resol: %s, fps: %s) not supported.", device, __func__,
		str_mode[param->mode], str_resol[param->resol], str_fps[param->fps]);
	return -EINVAL;
}

int is_hw_dvfs_get_rear_dual_wide_macro_scenario(struct is_device_ischain *device,
	struct is_dvfs_scenario_param *param)
{
	dbg_dvfs(1, "[DVFS] %s() mode: %s, resol: %s, fps: %s\n", device, __func__,
		str_mode[param->mode], str_resol[param->resol], str_fps[param->fps]);

	switch (param->mode) {
	case IS_DVFS_MODE_PHOTO:
		return IS_DVFS_SN_REAR_DUAL_WIDE_MACRO_PHOTO;
	case IS_DVFS_MODE_CAPTURE:
		return IS_DVFS_SN_REAR_DUAL_WIDE_MACRO_CAPTURE;
	case IS_DVFS_MODE_VIDEO:
		switch (param->resol) {
		case IS_DVFS_RESOL_FHD:
			return IS_DVFS_SN_REAR_DUAL_WIDE_MACRO_VIDEO_FHD30;
		default:
			goto dvfs_err;
		}
	default:
		goto dvfs_err;
	}

dvfs_err:
	merr("%s fails. (mode: %s, resol: %s, fps: %s) not supported.", device, __func__,
		str_mode[param->mode], str_resol[param->resol], str_fps[param->fps]);
	return -EINVAL;
}

int is_hw_dvfs_get_front_single_front_scenario(struct is_device_ischain *device,
	struct is_dvfs_scenario_param *param)
{
	dbg_dvfs(1, "[DVFS] %s() mode: %s, resol: %s, fps: %s\n", device, __func__,
		str_mode[param->mode], str_resol[param->resol], str_fps[param->fps]);

	switch (param->mode) {
	case IS_DVFS_MODE_PHOTO:
		if (param->resol == IS_DVFS_RESOL_FULL)
			return IS_DVFS_SN_FRONT_SINGLE_PHOTO_FULL;
		else
			return IS_DVFS_SN_FRONT_SINGLE_PHOTO;
	case IS_DVFS_MODE_CAPTURE:
		return IS_DVFS_SN_FRONT_SINGLE_CAPTURE;
	case IS_DVFS_MODE_VIDEO:
		switch (param->resol) {
		case IS_DVFS_RESOL_FHD:
			switch (param->fps) {
			case IS_DVFS_FPS_24:
			case IS_DVFS_FPS_30:
				return IS_DVFS_SN_FRONT_SINGLE_VIDEO_FHD30;
			case IS_DVFS_FPS_60:
				return IS_DVFS_SN_FRONT_SINGLE_VIDEO_FHD60;
			case IS_DVFS_FPS_120:
				return IS_DVFS_SN_FRONT_SINGLE_VIDEO_FHD120;
			default:
				goto dvfs_err;
			}
		case IS_DVFS_RESOL_UHD:
			switch (param->fps) {
			case IS_DVFS_FPS_24:
			case IS_DVFS_FPS_30:
				return IS_DVFS_SN_FRONT_SINGLE_VIDEO_UHD30;
			case IS_DVFS_FPS_60:
				return IS_DVFS_SN_FRONT_SINGLE_VIDEO_UHD60;
			case IS_DVFS_FPS_120:
				return IS_DVFS_SN_FRONT_SINGLE_VIDEO_UHD120;
			default:
				goto dvfs_err;
			}
		default:
			goto dvfs_err;
		}
	default:
		goto dvfs_err;
	}

dvfs_err:
	merr("%s fails. (mode: %s, resol: %s, fps: %s) not supported.", device, __func__,
		str_mode[param->mode], str_resol[param->resol], str_fps[param->fps]);
	return -EINVAL;
}

int is_hw_dvfs_get_pip_dual_scenario(struct is_device_ischain *device,
	struct is_dvfs_scenario_param *param)
{
	dbg_dvfs(1, "[DVFS] %s() mode: %s, resol: %s, fps: %s\n", device, __func__,
		str_mode[param->mode], str_resol[param->resol], str_fps[param->fps]);

	switch (param->mode) {
	case IS_DVFS_MODE_PHOTO:
		return IS_DVFS_SN_PIP_DUAL_PHOTO;
	case IS_DVFS_MODE_CAPTURE:
		return IS_DVFS_SN_PIP_DUAL_CAPTURE;
	case IS_DVFS_MODE_VIDEO:
		switch (param->resol) {
		case IS_DVFS_RESOL_FHD:
			return IS_DVFS_SN_PIP_DUAL_VIDEO_FHD30;
		default:
			goto dvfs_err;
		}
	default:
		goto dvfs_err;
	}

dvfs_err:
	merr("%s fails. (mode: %s, resol: %s, fps: %s) not supported.", device, __func__,
		str_mode[param->mode], str_resol[param->resol], str_fps[param->fps]);
	return -EINVAL;
}

int is_hw_dvfs_get_triple_scenario(struct is_device_ischain *device,
	struct is_dvfs_scenario_param *param)
{
	dbg_dvfs(1, "[DVFS] %s() mode: %s, resol: %s, fps: %s\n", device, __func__,
		str_mode[param->mode], str_resol[param->resol], str_fps[param->fps]);

	switch (param->mode) {
	case IS_DVFS_MODE_PHOTO:
		return IS_DVFS_SN_TRIPLE_PHOTO;
	case IS_DVFS_MODE_CAPTURE:
		return IS_DVFS_SN_TRIPLE_CAPTURE;
	case IS_DVFS_MODE_VIDEO:
		switch (param->resol) {
		case IS_DVFS_RESOL_FHD:
			switch (param->fps) {
			case IS_DVFS_FPS_24:
			case IS_DVFS_FPS_30:
				return IS_DVFS_SN_TRIPLE_VIDEO_FHD30;
			case IS_DVFS_FPS_60:
				return IS_DVFS_SN_TRIPLE_VIDEO_FHD60;
			default:
				goto dvfs_err;
			}
		case IS_DVFS_RESOL_UHD:
			switch (param->fps) {
			case IS_DVFS_FPS_24:
			case IS_DVFS_FPS_30:
				return IS_DVFS_SN_TRIPLE_VIDEO_UHD30;
			case IS_DVFS_FPS_60:
				return IS_DVFS_SN_TRIPLE_VIDEO_UHD60;
			default:
				goto dvfs_err;
			}
		default:
			goto dvfs_err;
		}
	default:
		goto dvfs_err;
	}

dvfs_err:
	merr("%s fails. (mode: %s, resol: %s, fps: %s) not supported.", device, __func__,
		str_mode[param->mode], str_resol[param->resol], str_fps[param->fps]);
	return -EINVAL;
}

int is_hw_dvfs_get_scenario(struct is_device_ischain *device, int flag_capture)
{
	struct is_dvfs_scenario_param param;

#if !defined(ENABLE_DVFS)
	return IS_DVFS_SN_DEFAULT;
#endif

	is_hw_dvfs_init_face_mask(device, &param);
	if (is_hw_dvfs_get_scenario_param(device, flag_capture, &param) < 0)
		goto dvfs_err;

	dbg_dvfs(1, "[DVFS] %s() face: %s, num: %s, sensor: %s\n", device, __func__,
		str_face[param.face], str_num[param.num], str_sensor[param.sensor]);

	switch (param.face) {
	case IS_DVFS_FACE_REAR:
		switch (param.num) {
		case IS_DVFS_NUM_SINGLE:
			switch (param.sensor) {
			/* wide sensor scenarios */
			case IS_DVFS_SENSOR_WIDE:
				return is_hw_dvfs_get_rear_single_wide_scenario(device, &param);
			case IS_DVFS_SENSOR_WIDE_FASTAE:
				return IS_DVFS_SN_REAR_SINGLE_WIDE_FASTAE;
			case IS_DVFS_SENSOR_WIDE_REMOSAIC:
				return is_hw_dvfs_get_rear_single_wide_remosaic_scenario(device, &param);
			case IS_DVFS_SENSOR_WIDE_VIDEOHDR:
				return IS_DVFS_SN_REAR_SINGLE_WIDE_VIDEOHDR;
			case IS_DVFS_SENSOR_WIDE_SSM:
				return IS_DVFS_SN_REAR_SINGLE_WIDE_SSM;
			case IS_DVFS_SENSOR_WIDE_VT:
				return IS_DVFS_SN_REAR_SINGLE_WIDE_VT;
			/* tele sensor scenarios */
			case IS_DVFS_SENSOR_TELE:
				return is_hw_dvfs_get_rear_single_tele_scenario(device, &param);
			case IS_DVFS_SENSOR_TELE_REMOSAIC:
				return is_hw_dvfs_get_rear_single_tele_remosaic_scenario(device, &param);
			/* ultra wide sensor scenarios */
			case IS_DVFS_SENSOR_ULTRAWIDE:
				return is_hw_dvfs_get_rear_single_ultrawide_scenario(device, &param);
			case IS_DVFS_SENSOR_ULTRAWIDE_SSM:
				return IS_DVFS_SN_REAR_SINGLE_ULTRAWIDE_SSM;
			/* macro sensor scenarios */
			case IS_DVFS_SENSOR_MACRO:
				return is_hw_dvfs_get_rear_single_macro_scenario(device, &param);
			default:
				goto dvfs_err;
			}
		case IS_DVFS_NUM_DUAL:
			switch (param.sensor) {
			/* wide and tele dual sensor scenarios */
			case IS_DVFS_SENSOR_WIDE_TELE:
				return is_hw_dvfs_get_rear_dual_wide_tele_scenario(device, &param);
			/* wide and ultra wide dual sensor scenarios */
			case IS_DVFS_SENSOR_WIDE_ULTRAWIDE:
				return is_hw_dvfs_get_rear_dual_wide_ultrawide_scenario(device, &param);
			/* wide and macro dual sensor scenarios */
			case IS_DVFS_SENSOR_WIDE_MACRO:
				return is_hw_dvfs_get_rear_dual_wide_macro_scenario(device, &param);
			default:
				goto dvfs_err;
			}
		case IS_DVFS_NUM_TRIPLE:
			return is_hw_dvfs_get_triple_scenario(device, &param);
		default:
			goto dvfs_err;
		}
	case IS_DVFS_FACE_FRONT:
		switch (param.num) {
		case IS_DVFS_NUM_SINGLE:
			switch (param.sensor) {
			case IS_DVFS_SENSOR_FRONT:
				return is_hw_dvfs_get_front_single_front_scenario(device, &param);
			case IS_DVFS_SENSOR_FRONT_FASTAE:
				return IS_DVFS_SN_FRONT_SINGLE_FASTAE;
			case IS_DVFS_SENSOR_FRONT_SECURE:
				return IS_DVFS_SN_FRONT_SINGLE_SECURE;
			case IS_DVFS_SENSOR_FRONT_VT:
				return IS_DVFS_SN_FRONT_SINGLE_VT;
			default:
				goto dvfs_err;
			}
		default:
			goto dvfs_err;
		}
	case IS_DVFS_FACE_PIP:
		if (param.sensor == IS_DVFS_SENSOR_WIDE_FASTAE)
			return IS_DVFS_SN_REAR_SINGLE_WIDE_FASTAE;
		switch (param.num) {
		case IS_DVFS_NUM_DUAL:
			return is_hw_dvfs_get_pip_dual_scenario(device, &param);
		case IS_DVFS_NUM_TRIPLE:
			return is_hw_dvfs_get_triple_scenario(device, &param);
		default:
			goto dvfs_err;
		}
	default:
		goto dvfs_err;
	}

dvfs_err:
	merr("[DVFS] %s fails. (face: %s, num: %s ,sensor: %s) not supported.\n", device,
		__func__, str_face[param.face], str_num[param.num], str_sensor[param.sensor]);
	return -EINVAL;
}

int is_hw_dvfs_get_ext_scenario(struct is_device_ischain *device, int flag_capture)
{
	struct is_dvfs_scenario_param param;

#if !defined(ENABLE_DVFS)
	return IS_DVFS_SN_DEFAULT;
#endif

	is_hw_dvfs_init_face_mask(device, &param);
	if (is_hw_dvfs_get_scenario_param(device, flag_capture, &param) < 0)
		goto dvfs_err;

	dbg_dvfs(1, "[DVFS] %s() face: %s, num: %s, sensor: %s\n", device,
		__func__, str_face[param.face], str_num[param.num], str_sensor[param.sensor]);
	switch (param.face) {
	case IS_DVFS_FACE_REAR:
		switch (param.num) {
		case IS_DVFS_NUM_SINGLE:
			return IS_DVFS_SN_EXT_REAR_SINGLE;
		case IS_DVFS_NUM_DUAL:
			return IS_DVFS_SN_EXT_REAR_DUAL;
		default:
			goto dvfs_err;
		}
	case IS_DVFS_FACE_FRONT:
		switch (param.num) {
		case IS_DVFS_NUM_SINGLE:
			switch (param.sensor) {
			case IS_DVFS_SENSOR_FRONT:
				return IS_DVFS_SN_EXT_FRONT;
			case IS_DVFS_SENSOR_FRONT_SECURE:
				return IS_DVFS_SN_EXT_FRONT;
			default:
				goto dvfs_err;
			}
		default:
			goto dvfs_err;
		}
	default:
		goto dvfs_err;
	}

dvfs_err:
	merr("[DVFS] %s fails. (face: %s, num: %s ,sensor: %s) not supported.\n", device,
		__func__, str_face[param.face], str_num[param.num], str_sensor[param.sensor]);
	return -EINVAL;
}

char g_str_scn[3][IS_DVFS_STR_LEN];
char *is_hw_dvfs_get_scenario_name(int id, int type)
{
	int i;
	char *str_scn = g_str_scn[type];

	/* For readability, change parse_scenaro_nm to upper character */
	for (i = 0; is_dvfs_dt_arr[scenario_idx[id]].parse_scenario_nm[i]; i++)
		str_scn[i] = toupper(is_dvfs_dt_arr[scenario_idx[id]].parse_scenario_nm[i]);
	str_scn[i - 1] = '\0';

	return str_scn;
}

int is_hw_dvfs_get_frame_tick(int id)
{
	return is_dvfs_dt_arr[scenario_idx[id]].keep_frame_tick;
}

void is_hw_dvfs_init_face_mask(struct is_device_ischain *device,
	struct is_dvfs_scenario_param *param)
{
	int pos;

	pos = IS_DVFS_SENSOR_POSITION_WIDE;
	param->wide_mask = (pos < 0) ? 0 : (1 << pos);
	pos = IS_DVFS_SENSOR_POSITION_TELE;
	param->tele_mask = (pos < 0) ? 0 : (1 << pos);
	pos = IS_DVFS_SENSOR_POSITION_TELE2;
	param->tele2_mask = (pos < 0) ? 0 : (1 << pos);
	pos = IS_DVFS_SENSOR_POSITION_ULTRAWIDE;
	param->ultrawide_mask = (pos < 0) ? 0 : (1 << pos);
	pos = IS_DVFS_SENSOR_POSITION_MACRO;
	param->macro_mask = (pos < 0) ? 0 : (1 << pos);
	param->rear_mask = param->wide_mask | param->tele_mask | param->tele2_mask |
				param->ultrawide_mask | param->macro_mask;

	pos = IS_DVFS_SENSOR_POSITION_FRONT;
	param->front_sensor_mask = (pos < 0) ? 0 : (1 << pos);
	param->front_mask = param->front_sensor_mask;

	dbg_dvfs(1, "[DVFS] %s() rear mask: 0x%x, front mask: 0x%x\n", device,
		__func__, param->rear_mask, param->front_mask);
}

unsigned int is_hw_dvfs_get_bit_count(unsigned long bits)
{
	unsigned int count = 0;

	while (bits) {
		bits &= (bits - 1);
		count++;
	}

	return count;
}

bool is_hw_dvfs_get_bundle_update_seq(u32 scenario_id, u32 *nums,
		u32 *types, u32 *scns, bool *operating)
{
	int i = 0;
	bool need_update = false;

	FIMC_BUG(!types);
	FIMC_BUG(!scns);

	switch (scenario_id) {
	case IS_DVFS_SN_REAR_SINGLE_WIDE_CAPTURE:
	case IS_DVFS_SN_REAR_SINGLE_TELE_CAPTURE:
	case IS_DVFS_SN_REAR_DUAL_WIDE_TELE_CAPTURE:
	case IS_DVFS_SN_REAR_DUAL_WIDE_ULTRAWIDE_CAPTURE:
	case IS_DVFS_SN_FRONT_SINGLE_CAPTURE:
		need_update = true;
		*operating = true;
		break;
	case IS_DVFS_SN_REAR_SINGLE_WIDE_PHOTO:
	case IS_DVFS_SN_REAR_SINGLE_TELE_PHOTO:
	case IS_DVFS_SN_REAR_SINGLE_ULTRAWIDE_PHOTO:
	case IS_DVFS_SN_REAR_DUAL_WIDE_TELE_PHOTO:
	case IS_DVFS_SN_REAR_DUAL_WIDE_ULTRAWIDE_PHOTO:
	case IS_DVFS_SN_REAR_SINGLE_WIDE_VIDEO_FHD30:
	case IS_DVFS_SN_REAR_SINGLE_WIDE_VIDEO_UHD30:
	case IS_DVFS_SN_REAR_SINGLE_TELE_VIDEO_FHD30:
	case IS_DVFS_SN_REAR_SINGLE_TELE_VIDEO_UHD30:
	case IS_DVFS_SN_REAR_SINGLE_ULTRAWIDE_VIDEO_FHD30:
	case IS_DVFS_SN_REAR_SINGLE_ULTRAWIDE_VIDEO_UHD30:
	case IS_DVFS_SN_REAR_DUAL_WIDE_TELE_VIDEO_FHD30:
	case IS_DVFS_SN_REAR_DUAL_WIDE_TELE_VIDEO_UHD30:
	case IS_DVFS_SN_REAR_DUAL_WIDE_ULTRAWIDE_VIDEO_FHD30:
	case IS_DVFS_SN_REAR_DUAL_WIDE_ULTRAWIDE_VIDEO_UHD30:
	case IS_DVFS_SN_REAR_SINGLE_WIDE_VIDEO_FHD60:
	case IS_DVFS_SN_REAR_SINGLE_WIDE_VIDEO_UHD60:
	case IS_DVFS_SN_REAR_SINGLE_TELE_VIDEO_FHD60:
	case IS_DVFS_SN_REAR_SINGLE_TELE_VIDEO_UHD60:
	case IS_DVFS_SN_REAR_DUAL_WIDE_TELE_VIDEO_FHD60:
	case IS_DVFS_SN_REAR_DUAL_WIDE_TELE_VIDEO_UHD60:
	case IS_DVFS_SN_REAR_DUAL_WIDE_ULTRAWIDE_VIDEO_FHD60:
	case IS_DVFS_SN_REAR_DUAL_WIDE_ULTRAWIDE_VIDEO_UHD60:
	case IS_DVFS_SN_PIP_DUAL_PHOTO:
	case IS_DVFS_SN_PIP_DUAL_VIDEO_FHD30:
	case IS_DVFS_SN_TRIPLE_PHOTO:
	case IS_DVFS_SN_TRIPLE_VIDEO_FHD30:
	case IS_DVFS_SN_TRIPLE_VIDEO_UHD30:
	case IS_DVFS_SN_TRIPLE_VIDEO_FHD60:
	case IS_DVFS_SN_TRIPLE_VIDEO_UHD60:
	case IS_DVFS_SN_FRONT_SINGLE_VIDEO_FHD30:
	case IS_DVFS_SN_FRONT_SINGLE_VIDEO_UHD30:
	case IS_DVFS_SN_FRONT_SINGLE_VIDEO_FHD60:
	case IS_DVFS_SN_FRONT_SINGLE_VIDEO_UHD60:
		need_update = true;
		break;
	default:
		if (*operating) {
			need_update = true;
			*operating = false;
		}
		break;
	}

	/*
	 * If CSIS - PDP votf used as global mode,
	 * It should be ensured that
	 * frequency of CSIS should always be greater than or equal to PDP.
	 * As shown in the sequence below,
	 * it should be increased to Max level and
	 * level must be changed according to the sequence.
	 * It is the same when restoring after change.
	 */
	if (need_update) {
		types[i] = IS_DVFS_CSIS;
		scns[i] = IS_DVFS_SN_MAX;
		i++;
		types[i] = IS_DVFS_CAM;
		scns[i] = IS_DVFS_SN_MAX;
		i++;
		types[i] = IS_DVFS_CAM;
		scns[i] = scenario_id;
		i++;
		types[i] = IS_DVFS_CSIS;
		scns[i] = scenario_id;
		i++;

		*nums = i;
	}

	return need_update;
}

struct is_dvfs_scenario static_scenarios = {
	.scenario_nm = "DVFS static scenario",
};

struct is_dvfs_scenario dynamic_scenarios = {
	.scenario_nm = "DVFS dynamic scenario",
};

struct is_dvfs_scenario external_scenarios = {
	.scenario_nm = "DVFS external scenario",
};

int is_hw_dvfs_init(void *dvfs_data)
{
	int i, ret = 0;
	struct is_dvfs_ctrl *dvfs_ctrl;

	dvfs_ctrl = (struct is_dvfs_ctrl *)dvfs_data;

	FIMC_BUG(!dvfs_ctrl);

	dvfs_ctrl->static_ctrl->cur_scenario_id	= -1;
	dvfs_ctrl->static_ctrl->scenarios = &static_scenarios;
	dvfs_ctrl->dynamic_ctrl->cur_scenario_id	= -1;
	dvfs_ctrl->dynamic_ctrl->cur_frame_tick	= -1;
	dvfs_ctrl->dynamic_ctrl->scenarios = &dynamic_scenarios;
	dvfs_ctrl->external_ctrl->cur_scenario_id	= -1;
	dvfs_ctrl->external_ctrl->scenarios = &external_scenarios;

	for (i = 0; i < IS_DVFS_SN_END; i++) {
		scenario_idx[is_dvfs_dt_arr[i].scenario_id] = i;
	}

	return ret;
}
