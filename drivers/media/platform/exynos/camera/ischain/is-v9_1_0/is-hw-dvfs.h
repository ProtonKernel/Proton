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

#ifndef IS_HW_DVFS_H
#define IS_HW_DVFS_H

#include "is-device-ischain.h"

/* for porting DVFS V2.0 */
/* sensor position */
#define IS_DVFS_SENSOR_POSITION_WIDE 0
#define IS_DVFS_SENSOR_POSITION_FRONT 1
#define IS_DVFS_SENSOR_POSITION_TELE 2
#define IS_DVFS_SENSOR_POSITION_ULTRAWIDE 4
#define IS_DVFS_SENSOR_POSITION_TELE2 6
#define IS_DVFS_SENSOR_POSITION_MACRO -1 /* 6 */
#define IS_DVFS_SENSOR_POSITION_TOF -1 /* 8 */
/* specitial feature */
#define IS_DVFS_FEATURE_WIDE_DUALFPS 0
#define IS_DVFS_FEATURE_WIDE_VIDEOHDR 0

/* for resolution calculation */
#define IS_DVFS_RESOL_FHD_TH (3*1920*1080)
#define IS_DVFS_RESOL_UHD_TH (3*3840*2160)
#define IS_DVFS_RESOL_8K_TH (3*7680*4320)

#define IS_DVFS_STATIC		0
#define IS_DVFS_DYNAMIC		1
#define IS_DVFS_EXTERNAL	2

/* for backword compatibility with DVFS V1.0 */
#define IS_SN_MAX IS_DVFS_SN_MAX
#define IS_SN_END IS_DVFS_SN_END

/* dvfs table idx ex.different dvfa table  pure bayer or dynamic bayer */
#define IS_DVFS_TABLE_IDX_MAX 3

/* Tick count to get some time margin for DVFS scenario transition while streaming. */
#define IS_DVFS_CAPTURE_TICK (KEEP_FRAME_TICK_DEFAULT + 3)
#define IS_DVFS_DUAL_CAPTURE_TICK (2 * IS_DVFS_CAPTURE_TICK)

#define IS_DVFS_STR_LEN 100

/* enum type for each hierarchical level of DVFS scenario */
enum IS_DVFS_FACE {
	IS_DVFS_FACE_REAR,
	IS_DVFS_FACE_FRONT,
	IS_DVFS_FACE_PIP,
	IS_DVFS_FACE_END,
};

enum IS_DVFS_NUM {
	IS_DVFS_NUM_SINGLE,
	IS_DVFS_NUM_DUAL,
	IS_DVFS_NUM_TRIPLE,
	IS_DVFS_NUM_END,
};

enum IS_DVFS_SENSOR {
	IS_DVFS_SENSOR_WIDE,
	IS_DVFS_SENSOR_WIDE_FASTAE,
	IS_DVFS_SENSOR_WIDE_REMOSAIC,
	IS_DVFS_SENSOR_WIDE_VIDEOHDR,
	IS_DVFS_SENSOR_WIDE_SSM,
	IS_DVFS_SENSOR_WIDE_VT,
	IS_DVFS_SENSOR_TELE,
	IS_DVFS_SENSOR_TELE_REMOSAIC,
	IS_DVFS_SENSOR_ULTRAWIDE,
	IS_DVFS_SENSOR_ULTRAWIDE_SSM,
	IS_DVFS_SENSOR_MACRO,
	IS_DVFS_SENSOR_WIDE_TELE,
	IS_DVFS_SENSOR_WIDE_ULTRAWIDE,
	IS_DVFS_SENSOR_WIDE_MACRO,
	IS_DVFS_SENSOR_FRONT,
	IS_DVFS_SENSOR_FRONT_FASTAE,
	IS_DVFS_SENSOR_FRONT_SECURE,
	IS_DVFS_SENSOR_FRONT_VT,
	IS_DVFS_SENSOR_PIP,
	IS_DVFS_SENSOR_TRIPLE,
	IS_DVFS_SENSOR_END,
};

enum IS_DVFS_MODE {
	IS_DVFS_MODE_PHOTO,
	IS_DVFS_MODE_CAPTURE,
	IS_DVFS_MODE_VIDEO,
	IS_DVFS_MODE_END,
};

enum IS_DVFS_RESOL {
	IS_DVFS_RESOL_FHD,
	IS_DVFS_RESOL_UHD,
	IS_DVFS_RESOL_8K,
	IS_DVFS_RESOL_FULL,
	IS_DVFS_RESOL_END,
};

enum IS_DVFS_FPS {
	IS_DVFS_FPS_24,
	IS_DVFS_FPS_30,
	IS_DVFS_FPS_60,
	IS_DVFS_FPS_120,
	IS_DVFS_FPS_240,
	IS_DVFS_FPS_480,
	IS_DVFS_FPS_END,
};

/* Pablo DVFS SCENARIO enum */
enum IS_DVFS_SN {
	IS_DVFS_SN_DEFAULT,
	/* wide sensor scenarios */
	IS_DVFS_SN_REAR_SINGLE_WIDE_PHOTO,
	IS_DVFS_SN_REAR_SINGLE_WIDE_PHOTO_FULL,
	IS_DVFS_SN_REAR_SINGLE_WIDE_CAPTURE,
	IS_DVFS_SN_REAR_SINGLE_WIDE_VIDEO_FHD30,
	IS_DVFS_SN_REAR_SINGLE_WIDE_VIDEO_FHD60,
	IS_DVFS_SN_REAR_SINGLE_WIDE_VIDEO_FHD60_HF,
	IS_DVFS_SN_REAR_SINGLE_WIDE_VIDEO_FHD60_SUPERSTEADY,
	IS_DVFS_SN_REAR_SINGLE_WIDE_VIDEO_FHD60_HF_SUPERSTEADY,
	IS_DVFS_SN_REAR_SINGLE_WIDE_VIDEO_FHD120,
	IS_DVFS_SN_REAR_SINGLE_WIDE_VIDEO_FHD240,
	IS_DVFS_SN_REAR_SINGLE_WIDE_VIDEO_FHD480,
	IS_DVFS_SN_REAR_SINGLE_WIDE_VIDEO_UHD30,
	IS_DVFS_SN_REAR_SINGLE_WIDE_VIDEO_UHD60,
	IS_DVFS_SN_REAR_SINGLE_WIDE_VIDEO_UHD60_HF,
	IS_DVFS_SN_REAR_SINGLE_WIDE_VIDEO_UHD120,
	IS_DVFS_SN_REAR_SINGLE_WIDE_VIDEO_8K24,
	IS_DVFS_SN_REAR_SINGLE_WIDE_VIDEO_8K24_HF,
	IS_DVFS_SN_REAR_SINGLE_WIDE_VIDEO_8K30,
	IS_DVFS_SN_REAR_SINGLE_WIDE_VIDEO_8K30_HF,
	IS_DVFS_SN_REAR_SINGLE_WIDE_REMOSAIC_PHOTO,
	IS_DVFS_SN_REAR_SINGLE_WIDE_REMOSAIC_CAPTURE,
	IS_DVFS_SN_REAR_SINGLE_WIDE_FASTAE,
	IS_DVFS_SN_REAR_SINGLE_WIDE_VIDEOHDR,
	IS_DVFS_SN_REAR_SINGLE_WIDE_SSM,
	IS_DVFS_SN_REAR_SINGLE_WIDE_VT,
	/* tele sensor scenarios */
	IS_DVFS_SN_REAR_SINGLE_TELE_PHOTO,
	IS_DVFS_SN_REAR_SINGLE_TELE_PHOTO_FULL,
	IS_DVFS_SN_REAR_SINGLE_TELE_CAPTURE,
	IS_DVFS_SN_REAR_SINGLE_TELE_VIDEO_FHD30,
	IS_DVFS_SN_REAR_SINGLE_TELE_VIDEO_FHD60,
	IS_DVFS_SN_REAR_SINGLE_TELE_VIDEO_UHD30,
	IS_DVFS_SN_REAR_SINGLE_TELE_VIDEO_UHD60,
	IS_DVFS_SN_REAR_SINGLE_TELE_VIDEO_8K24,
	IS_DVFS_SN_REAR_SINGLE_TELE_VIDEO_8K24_HF,
	IS_DVFS_SN_REAR_SINGLE_TELE_VIDEO_8K30,
	IS_DVFS_SN_REAR_SINGLE_TELE_VIDEO_8K30_HF,
	IS_DVFS_SN_REAR_SINGLE_TELE_VIDEO_8K_THERMAL,
	IS_DVFS_SN_REAR_SINGLE_TELE_REMOSAIC_PHOTO,
	IS_DVFS_SN_REAR_SINGLE_TELE_REMOSAIC_CAPTURE,
	/* ultra wide sensor scenarios */
	IS_DVFS_SN_REAR_SINGLE_ULTRAWIDE_PHOTO,
	IS_DVFS_SN_REAR_SINGLE_ULTRAWIDE_PHOTO_FULL,
	IS_DVFS_SN_REAR_SINGLE_ULTRAWIDE_CAPTURE,
	IS_DVFS_SN_REAR_SINGLE_ULTRAWIDE_VIDEO_FHD30,
	IS_DVFS_SN_REAR_SINGLE_ULTRAWIDE_VIDEO_FHD30_SUPERSTEADY,
	IS_DVFS_SN_REAR_SINGLE_ULTRAWIDE_VIDEO_FHD30_HF_SUPERSTEADY,
	IS_DVFS_SN_REAR_SINGLE_ULTRAWIDE_VIDEO_FHD60,
	IS_DVFS_SN_REAR_SINGLE_ULTRAWIDE_VIDEO_FHD60_SUPERSTEADY,
	IS_DVFS_SN_REAR_SINGLE_ULTRAWIDE_VIDEO_FHD60_HF_SUPERSTEADY,
	IS_DVFS_SN_REAR_SINGLE_ULTRAWIDE_VIDEO_FHD120,
	IS_DVFS_SN_REAR_SINGLE_ULTRAWIDE_VIDEO_FHD480,
	IS_DVFS_SN_REAR_SINGLE_ULTRAWIDE_VIDEO_UHD30,
	IS_DVFS_SN_REAR_SINGLE_ULTRAWIDE_VIDEO_UHD60,
	IS_DVFS_SN_REAR_SINGLE_ULTRAWIDE_SSM,
	/* macro sensor scenarios */
	IS_DVFS_SN_REAR_SINGLE_MACRO_PHOTO,
	IS_DVFS_SN_REAR_SINGLE_MACRO_PHOTO_FULL,
	IS_DVFS_SN_REAR_SINGLE_MACRO_CAPTURE,
	IS_DVFS_SN_REAR_SINGLE_MACRO_VIDEO_FHD30,
	/* wide and tele dual sensor scenarios */
	IS_DVFS_SN_REAR_DUAL_WIDE_TELE_PHOTO,
	IS_DVFS_SN_REAR_DUAL_WIDE_TELE_CAPTURE,
	IS_DVFS_SN_REAR_DUAL_WIDE_TELE_VIDEO_FHD30,
	IS_DVFS_SN_REAR_DUAL_WIDE_TELE_VIDEO_UHD30,
	IS_DVFS_SN_REAR_DUAL_WIDE_TELE_VIDEO_FHD60,
	IS_DVFS_SN_REAR_DUAL_WIDE_TELE_VIDEO_UHD60,
	/* wide and ultra wide dual sensor scenarios */
	IS_DVFS_SN_REAR_DUAL_WIDE_ULTRAWIDE_PHOTO,
	IS_DVFS_SN_REAR_DUAL_WIDE_ULTRAWIDE_CAPTURE,
	IS_DVFS_SN_REAR_DUAL_WIDE_ULTRAWIDE_VIDEO_FHD30,
	IS_DVFS_SN_REAR_DUAL_WIDE_ULTRAWIDE_VIDEO_UHD30,
	IS_DVFS_SN_REAR_DUAL_WIDE_ULTRAWIDE_VIDEO_FHD60,
	IS_DVFS_SN_REAR_DUAL_WIDE_ULTRAWIDE_VIDEO_UHD60,
	/* wide and macro dual sensor scenarios */
	IS_DVFS_SN_REAR_DUAL_WIDE_MACRO_PHOTO,
	IS_DVFS_SN_REAR_DUAL_WIDE_MACRO_CAPTURE,
	IS_DVFS_SN_REAR_DUAL_WIDE_MACRO_VIDEO_FHD30,
	/* front sensor scenarios */
	IS_DVFS_SN_FRONT_SINGLE_PHOTO,
	IS_DVFS_SN_FRONT_SINGLE_PHOTO_FULL,
	IS_DVFS_SN_FRONT_SINGLE_CAPTURE,
	IS_DVFS_SN_FRONT_SINGLE_VIDEO_FHD30,
	IS_DVFS_SN_FRONT_SINGLE_VIDEO_FHD60,
	IS_DVFS_SN_FRONT_SINGLE_VIDEO_FHD120,
	IS_DVFS_SN_FRONT_SINGLE_VIDEO_UHD30,
	IS_DVFS_SN_FRONT_SINGLE_VIDEO_UHD60,
	IS_DVFS_SN_FRONT_SINGLE_VIDEO_UHD120,
	IS_DVFS_SN_FRONT_SINGLE_FASTAE,
	IS_DVFS_SN_FRONT_SINGLE_SECURE,
	IS_DVFS_SN_FRONT_SINGLE_VT,
	/* pip scenarios */
	IS_DVFS_SN_PIP_DUAL_PHOTO,
	IS_DVFS_SN_PIP_DUAL_CAPTURE,
	IS_DVFS_SN_PIP_DUAL_VIDEO_FHD30,
	/* triple scenarios */
	IS_DVFS_SN_TRIPLE_PHOTO,
	IS_DVFS_SN_TRIPLE_VIDEO_FHD30,
	IS_DVFS_SN_TRIPLE_VIDEO_UHD30,
	IS_DVFS_SN_TRIPLE_VIDEO_FHD60,
	IS_DVFS_SN_TRIPLE_VIDEO_UHD60,
	IS_DVFS_SN_TRIPLE_CAPTURE,
	/* external camera scenarios */
	IS_DVFS_SN_EXT_REAR_SINGLE,
	IS_DVFS_SN_EXT_REAR_DUAL,
	IS_DVFS_SN_EXT_FRONT,
	IS_DVFS_SN_EXT_FRONT_SECURE,
	IS_DVFS_SN_MAX,
	IS_DVFS_SN_END,
};

struct is_dvfs_scenario_param {
	int rear_mask;
	int front_mask;
	int rear_face;
	int front_face;
	int wide_mask;
	int tele_mask;
	int tele2_mask;
	int ultrawide_mask;
	int macro_mask;
	int front_sensor_mask;
	unsigned long sensor_map;
	unsigned long sensor_active_map;
	u32 sensor_mode;
	int dual_mode;
	int sensor_fps;
	int setfile;
	int scen;
	int dvfs_scenario;
	int secure;
	int face;
	int num;
	int sensor;
	int mode;
	int resol;
	int fps;
	int hf; /* HF IP on/off */
};

/* for assign staic / dynamic scenario check logic data */
int is_hw_dvfs_init(void *dvfs_data);
int is_hw_dvfs_get_face(struct is_device_ischain *device, struct is_dvfs_scenario_param *param);
int is_hw_dvfs_get_num(struct is_device_ischain *device, struct is_dvfs_scenario_param *param);
int is_hw_dvfs_get_sensor(struct is_device_ischain *device, struct is_dvfs_scenario_param *param);
int is_hw_dvfs_get_mode(struct is_device_ischain *device, int flag_capture, struct is_dvfs_scenario_param *param);
int is_hw_dvfs_get_resol(struct is_device_ischain *device, struct is_dvfs_scenario_param *param);
int is_hw_dvfs_get_fps(struct is_device_ischain *device, struct is_dvfs_scenario_param *param);
int is_hw_dvfs_get_scenario(struct is_device_ischain *device, int flag_capture);
int is_hw_dvfs_get_ext_scenario(struct is_device_ischain *device, int flag_capture);
int is_hw_dvfs_get_rear_single_wide_scenario(struct is_device_ischain *device, struct is_dvfs_scenario_param *param);
int is_hw_dvfs_get_rear_single_wide_remosaic_scenario(struct is_device_ischain *device, struct is_dvfs_scenario_param *param);
int is_hw_dvfs_get_rear_single_tele_scenario(struct is_device_ischain *device, struct is_dvfs_scenario_param *param);
int is_hw_dvfs_get_rear_single_tele_remosaic_scenario(struct is_device_ischain *device, struct is_dvfs_scenario_param *param);
int is_hw_dvfs_get_rear_single_ultrawide_scenario(struct is_device_ischain *device, struct is_dvfs_scenario_param *param);
int is_hw_dvfs_get_rear_single_macro_scenario(struct is_device_ischain *device, struct is_dvfs_scenario_param *param);
int is_hw_dvfs_get_rear_dual_wide_tele_scenario(struct is_device_ischain *device, struct is_dvfs_scenario_param *param);
int is_hw_dvfs_get_rear_dual_wide_ultrawide_scenario(struct is_device_ischain *device, struct is_dvfs_scenario_param *param);
int is_hw_dvfs_get_rear_dual_wide_macro_scenario(struct is_device_ischain *device, struct is_dvfs_scenario_param *param);
int is_hw_dvfs_get_front_single_front_scenario(struct is_device_ischain *device, struct is_dvfs_scenario_param *param);
int is_hw_dvfs_get_pip_dual_wide_front_scenario(struct is_device_ischain *device, struct is_dvfs_scenario_param *param);
char *is_hw_dvfs_get_scenario_name(int id, int type);
int is_hw_dvfs_get_frame_tick(int id);
unsigned int is_hw_dvfs_get_bit_count(unsigned long bits);
void is_hw_dvfs_init_face_mask(struct is_device_ischain *device, struct is_dvfs_scenario_param *param);
int is_hw_dvfs_get_scenario_param(struct is_device_ischain *device, int flag_capture, struct is_dvfs_scenario_param *param);
bool is_hw_dvfs_get_bundle_update_seq(u32 scenario_id, u32 *bundle_num,
		u32 *bundle_domain, u32 *bundle_scn, bool *operating);


#endif /* IS_HW_DVFS_H */
